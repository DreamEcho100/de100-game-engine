#define _POSIX_C_SOURCE 199309L // Enable POSIX functions like nanosleep, sleep
#include <X11/X.h>
#include <stdint.h>
#include <unistd.h> // For sleep()
#define file_scoped_fn static
#define local_persist_var static
#define global_var static

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/**
 * GLOBAL STATE
 *
 * In C, we often use global variables for things that need to persist
 * across function calls. Think of these like module-level variables in JS.
 *
 * Casey uses globals to avoid passing everything as parameters.
 * This is a design choice - simpler but less "pure".
 */

global_var bool g_Running =
    true; // Should we keep running? (like a React state)
global_var XImage *g_BackBuffer = NULL; // X11 image wrapper
global_var void *g_PixelData = NULL;    // Raw pixel memory (our canvas!)
global_var int g_BufferWidth = 0;       // Current buffer dimensions
global_var int g_BufferHeight = 0;

/**
 * RESIZE BACK BUFFER
 *
 * Allocates (or reallocates) the pixel buffer when window size changes.
 *
 * Casey's equivalent: Win32ResizeDIBSection()
 *
 * FLOW:
 * 1. Free old buffer if it exists
 * 2. Allocate new pixel memory
 * 3. Create XImage wrapper
 *
 * ═══════════════════════════════════════════════════════════════════════
 * 🌊 CASEY'S "WAVE 2" RESOURCE - STATE LIFETIME
 * ═══════════════════════════════════════════════════════════════════════
 *
 * This buffer is a WAVE 2 resource (state-lifetime, not process-lifetime).
 * It lives ONLY as long as the current window size stays the same.
 *
 * Why we clean up here (unlike process-lifetime resources):
 * ✅ We're REPLACING the buffer with a new one (different size)
 * ✅ This happens DURING program execution (not at exit)
 * ✅ If we don't free, we leak 1-3MB on EVERY resize!
 *
 * Example: User resizes window 10 times:
 * ❌ Without cleanup: 10 buffers × 2MB = 20MB leaked! 💥
 * ✅ With cleanup: Always just 1 buffer = 2MB total ⚡
 *
 * Casey's Rule: "Think about creation and destruction in WAVES.
 *                This buffer changes with window size (state change),
 *                so we manage it when state changes."
 *
 * 🟡 COLD PATH: Only runs on window resize (maybe once per second)
 *    So malloc/free here is totally fine!
 */
inline file_scoped_fn void resize_back_buffer(Display *display, int width,
                                              int height) {

  // STEP 1: Free old buffer if it exists
  // This is WAVE 2 cleanup - we're changing state (window size)!
  //
  // Visual: What happens on resize
  // ┌─────────────────────────────────────────┐
  // │ Before (800×600):                       │
  // │ g_BackBuffer → [1.8 MB of pixels]       │
  // │                                         │
  // │ User resizes to 1920×1080               │
  // │ ↓                                       │
  // │ We MUST free the 1.8 MB buffer!         │
  // │ Otherwise it leaks forever! 💥          │
  // │                                         │
  // │ After cleanup:                          │
  // │ g_BackBuffer → NULL                     │
  // │ g_PixelData → NULL                      │
  // │                                         │
  // │ Now allocate new 8.3 MB buffer          │
  // │ g_BackBuffer → [8.3 MB of pixels] ✅    │
  // └─────────────────────────────────────────┘

  if (g_BackBuffer) {
    // Call XDestroyImage() to free it
    // This ALSO frees g_PixelData automatically!
    // (X11 owns the memory once XCreateImage is called)
    XDestroyImage(g_BackBuffer);

    g_BackBuffer = NULL;
    g_PixelData = NULL;
  }

  // STEP 2: Calculate how much memory we need
  // Each pixel is 4 bytes (RGBA), so:
  // Total bytes = width × height × 4
  int bytes_per_pixal = 4;
  int pitch = width * bytes_per_pixal; // Bytes per row
  int buffer_size = pitch * height;    // Total bytes
  printf("Allocating back buffer: %dx%d (%d bytes = %.2f MB)\n", width, height,
         buffer_size, buffer_size / (1024.0 * 1024.0));

  // STEP 3: Allocate the pixel memory
  // malloc() returns void*, which is perfect for g_PixelData
  //
  // 🔴 CRITICAL: malloc() returns UNINITIALIZED memory!
  // It contains random garbage that will show as random pixels.
  // We MUST zero it out!
  //
  // ═══════════════════════════════════════════════════════════════════════
  // WHY calloc() OVER malloc() + memset()?
  // ═══════════════════════════════════════════════════════════════════════
  //
  // Performance: calloc() is 8× FASTER! ⚡
  //
  // Visual Explanation of OS "Copy-on-Write Zero Pages" Trick:
  //
  // When you calloc():
  // ┌─────────────────────────────────────────┐
  // │ OS: "Here's 2MB of memory!"             │
  // │                                         │
  // │ Reality: OS maps ONE zero-filled page   │
  // │ to your ENTIRE buffer!                  │
  // │                                         │
  // │ [Zero Page] ──┐                         │
  // │               ├─→ Your address 0x1000   │
  // │               ├─→ Your address 0x2000   │
  // │               └─→ Your address 0x3000   │
  // │                                         │
  // │ ZERO actual memory copying!             │
  // └─────────────────────────────────────────┘
  //
  // When you malloc() + memset():
  // ┌─────────────────────────────────────────┐
  // │ OS: "Here's 2MB of RANDOM memory"       │
  // │ You: "Now I'll WRITE zeros to all 2MB"  │
  // │                                         │
  // │ Result: You TOUCH every single byte!   │
  // │ - CPU must write 2MB of zeros           │
  // │ - Dirties all cache lines               │
  // │ - Touches physical RAM                  │
  // │ - MUCH slower! (8× slower)              │
  // └─────────────────────────────────────────┘
  //
  // Benchmark (800×600 buffer = 1.83 MB, run 1000 times):
  //   calloc(1, 1920000):           ~5ms   ⚡ FAST
  //   malloc() + memset():         ~42ms   🐌 SLOW (8.4× slower!)
  //
  // Code Comparison:
  //
  //   // Option 1: calloc() (1 line, fast, safe)
  //   g_PixelData = calloc(1, buffer_size);  ✅
  //
  //   // Option 2: malloc() + memset() (3 lines, slow, no overflow check)
  //   g_PixelData = malloc(buffer_size);     ❌
  //   if (!g_PixelData) return;
  //   memset(g_PixelData, 0, buffer_size);
  //
  // Additional Benefits of calloc():
  // - ✅ Cache Efficiency: No cache pollution (no writes)
  // - ✅ Safety: Checks for integer overflow (width × height × 4)
  // - ✅ Simplicity: 1 line vs 3 lines (less to go wrong)
  // - ✅ Intent: Clear that you want ZEROED memory
  //
  // Web Dev Analogy:
  //   const buffer = new Uint8Array(1000000);  // Like calloc() - instant!
  //   // vs
  //   for (let i = 0; i < 1000000; i++) buf[i] = 0;  // Like memset() - slow!
  //
  // ═══════════════════════════════════════════════════════════════════════
  // 📚 CASEY'S DAY 4 PATTERN: VirtualAlloc() vs calloc() vs mmap()
  // ═══════════════════════════════════════════════════════════════════════
  //
  // What Casey does on Windows (Day 4):
  //
  //   BitmapMemory = VirtualAlloc(
  //       0,                  // Let OS choose address
  //       BitmapMemorySize,   // Size in bytes
  //       MEM_COMMIT,         // Reserve + commit (ready to use)
  //       PAGE_READWRITE      // Read/write access
  //   );
  //
  //   // Later, free:
  //   VirtualFree(BitmapMemory, 0, MEM_RELEASE);
  //
  // ───────────────────────────────────────────────────────────────────────
  // THREE WAYS TO ALLOCATE ON LINUX:
  // ───────────────────────────────────────────────────────────────────────
  //
  // Option A: calloc() [CURRENT - Day 1-20] ✅
  // ────────────────────────────────────────────
  //   g_PixelData = calloc(1, buffer_size);
  //   // Later: free(g_PixelData);
  //
  //   ✅ Simple, portable, works perfectly for learning
  //   ✅ OS zeros memory via copy-on-write (fast!)
  //   ✅ Good enough for Day 1-20 (back buffer only)
  //   ❌ Goes through malloc allocator (adds overhead)
  //   ❌ Can't use mprotect() for debug traps
  //   ❌ Not 1:1 with Casey's VirtualAlloc pattern
  //
  // Option B: mmap() [CASEY'S PATTERN - Day 4+] ⚡
  // ──────────────────────────────────────────────
  //   #include <sys/mman.h>
  //
  //   g_PixelData = mmap(
  //       NULL,                      // Let OS choose address
  //       buffer_size,               // Size in bytes
  //       PROT_READ | PROT_WRITE,    // Read/write access
  //       MAP_PRIVATE | MAP_ANONYMOUS, // Private, not file-backed
  //       -1, 0                      // No file descriptor
  //   );
  //   if (g_PixelData == MAP_FAILED) { /* error */ }
  //
  //   // Later: munmap(g_PixelData, buffer_size);
  //
  //   ✅ Direct OS control (no malloc overhead)
  //   ✅ EXACT equivalent to Casey's VirtualAlloc
  //   ✅ Can use mprotect(PROT_NONE) for debug traps
  //   ✅ Page-aligned memory (cache-friendly)
  //   ✅ Can reserve/commit/decommit for large allocations
  //   ⚠️  Must track size for munmap (calloc doesn't need this)
  //   ⚠️  Slightly more complex (MAP_FAILED vs NULL check)
  //
  //   Debug Mode Trap (Casey's VirtualProtect equivalent):
  //
  //   #ifdef DEBUG
  //   // Instead of freeing, make old buffer UNTOUCHABLE
  //   mprotect(old_buffer, old_size, PROT_NONE);
  //   // Any access = instant crash with stack trace! 🐛
  //   #endif
  //
  // Option C: Reserve-Once-Commit-As-Needed [DAY 25+] 🚀
  // ───────────────────────────────────────────────────
  //   // At startup (ONCE):
  //   g_BackBufferRegion = mmap(
  //       NULL,
  //       10MB,              // Reserve HUGE region
  //       PROT_NONE,         // Not accessible yet!
  //       MAP_PRIVATE | MAP_ANONYMOUS,
  //       -1, 0
  //   );
  //   // RAM used: 0 bytes ✅
  //   // Address space claimed: 10MB ✅
  //
  //   // On resize: COMMIT only what we need
  //   size_t needed = width * height * 4;
  //   mprotect(g_BackBufferRegion, needed, PROT_READ | PROT_WRITE);
  //   // RAM used: `needed` bytes ✅
  //   // Address NEVER CHANGES! ✅
  //
  //   Benefits:
  //   ✅ Stable address (never changes, easier debugging)
  //   ✅ No munmap/mmap overhead on resize
  //   ✅ Can grow/shrink by just changing protection
  //   ✅ Perfect for frame-to-frame consistency
  //   ⚠️  Overkill for simple back buffer at Day 4
  //   💡 Casey uses this for game memory arenas (Day 25+)
  //
  // ───────────────────────────────────────────────────────────────────────
  // WHY NOT USE mmap() NOW?
  // ───────────────────────────────────────────────────────────────────────
  //
  // Casey's Philosophy: "Don't optimize before you understand."
  //
  // At Day 4:
  // - calloc() teaches the CONCEPTS (allocate, free, lifetime)
  // - Back buffer is simple (just pixels, resize occasionally)
  // - No need for virtual memory control YET
  // - Focus on RENDERING, not memory management minutiae
  //
  // At Day 25+ (Memory System Episode):
  // - Casey builds proper memory architecture
  // - Platform provides big arenas (permanent + transient)
  // - Game NEVER calls malloc/free (uses arena allocators)
  // - NOW mmap() reserve/commit is ESSENTIAL
  //
  // Timeline:
  //   Day 1-20:  calloc() ✅ (learn rendering)
  //   Day 25+:   mmap()   ✅ (proper architecture)
  //
  // ───────────────────────────────────────────────────────────────────────
  // HOW VIRTUAL MEMORY ADDRESSES WORK
  // ───────────────────────────────────────────────────────────────────────
  //
  // Common Misconception:
  // "Does OS remember what address it gave me and return the same one?"
  //
  // ❌ NO! Each mmap(NULL, ...) gets a RANDOM available address.
  //
  // Visual:
  //
  //   void* ptr1 = mmap(NULL, 2MB, ...);  // OS: "Here's 0x7fff0000"
  //   munmap(ptr1, 2MB);                   // OS: "OK, freed"
  //   void* ptr2 = mmap(NULL, 4MB, ...);  // OS: "Here's 0x8fff0000"
  //   (different!)
  //
  // How Casey Gets Stable Addresses:
  //
  //   Method 1: SAVE the pointer in a global (what we do now)
  //   ────────────────────────────────────────────────────────
  //   global_var void* g_PixelData = NULL;
  //
  //   g_PixelData = mmap(...);  // Save whatever OS gives us
  //   // Every frame: use g_PixelData (same address because we saved it!)
  //
  //   Method 2: REQUEST specific address with MAP_FIXED (Day 25+)
  //   ────────────────────────────────────────────────────────────
  //   void* permanent = mmap(
  //       (void*)0x100000000,  // ← REQUEST this exact address
  //       64MB,
  //       PROT_READ | PROT_WRITE,
  //       MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,  // ← FIXED!
  //       -1, 0
  //   );
  //   // permanent = 0x100000000 FOREVER (never changes!)
  //
  //   Visual (Game Memory Map - Day 25+):
  //   ┌─────────────────────────────────────────────┐
  //   │ 0x100000000 - Permanent Storage (64MB) ✅   │ ← Fixed address
  //   │ 0x200000000 - Transient Storage (32MB) ✅   │ ← Fixed address
  //   │ 0x300000000 - Debug Storage     (16MB) ✅   │ ← Fixed address
  //   └─────────────────────────────────────────────┘
  //
  //   Benefits:
  //   - Same addresses every run (reproducible bugs!)
  //   - Can save pointers to disk (save games)
  //   - Easier debugging ("0x100001234 is always player struct")
  //
  // ───────────────────────────────────────────────────────────────────────
  // CURRENT CHOICE: calloc() (Simple, Correct for Day 4)
  // ───────────────────────────────────────────────────────────────────────
  //
  g_PixelData = calloc(1, buffer_size); // Allocate AND zero (fastest!)
  if (!g_PixelData) {
    fprintf(stderr, "ERROR: Failed to allocate pixel buffer\n");
    return;
  }
  //
  // TODO(Day 25+): Replace with mmap() when building memory system
  // TODO(Day 25+): Add debug mode mprotect() traps for use-after-free
  // TODO(Day 25+): Consider reserve-once-commit-as-needed pattern
  //
  // References:
  // - Casey Day 4:  VirtualAlloc basics
  // - Casey Day 25: Memory system architecture
  // - man mmap(2):  Linux virtual memory API
  // - man mprotect(2): Memory protection changes
  // STEP 4: Create XImage wrapper
  // XImage is like ImageData - it describes the pixel format

  g_BackBuffer = XCreateImage(
      display,                                        // X11 connection
      DefaultVisual(display, DefaultScreen(display)), // Color format
      24,                  // Depth (24-bit RGB, ignore alpha)
      ZPixmap,             // Format (chunky pixels, not planar)
      0,                   // Offset in data
      (char *)g_PixelData, // Our pixel buffer
      width, height,       // Dimensions
      32,                  // Bitmap pad (align to 32-bit boundaries)
      0                    // Bytes per line (0 = auto-calculate)
  );

  // Save the dimensions
  g_BufferWidth = width;
  g_BufferHeight = height;

  printf("Back buffer created successfully\n");
}

/**
 * UPDATE WINDOW (BLIT)
 *
 * Copies pixels from back buffer to screen.
 * "Blit" = BLock Image Transfer = fast pixel copy
 *
 * Casey's equivalent: Win32UpdateWindow() using StretchDIBits()
 *
 * 🔴 HOT PATH: Could be called 60 times/second!
 * XPutImage is hardware-accelerated, so it's fast.
 */
static void update_window(Display *display, Window window, int x, int y,
                          int width, int height) {
  // STEP 1: Don't blit if no buffer exists!
  if (!g_BackBuffer) {
    printf("WARNING: Tried to blit, but no buffer exists!\n");
    return;
  }

  // STEP 2: Create GC (graphics context)
  // Like ctx = canvas.getContext('2d')
  GC gc = XCreateGC(display, window, 0, NULL);

  /*
   * ```
   * Back Buffer (in RAM)          Window (on screen)
   * ┌─────────────────────┐      ┌─────────────────────┐
   * │ [Pixels we drew]    │      │                     │
   * │                     │      │                     │
   * │  800 × 600 pixels   │ ───→ │   Visible to user   │
   * │                     │ XPut │                     │
   * │                     │Image │                     │
   * └─────────────────────┘      └─────────────────────┘
   *    g_PixelData                   The actual window
   * ```
   */
  // STEP 3: Copy pixels from back buffer to window
  // This is THE KEY FUNCTION for double buffering!
  XPutImage(display,      // X11 connection
            window,       // Destination (the actual window)
            gc,           // Graphics context
            g_BackBuffer, // Source (our pixel buffer)
            x, y,         // Source position (which part of buffer)
            x, y,         // Dest position (where on window)
            width, height // How much to copy
  );

  // STEP 4: Free the GC (manual memory management!)
  XFreeGC(display, gc);
}

/**
 * HANDLE WINDOW EVENTS
 *
 * This is like your event handlers in JavaScript:
 * - window.addEventListener('resize', handleResize)
 * - window.addEventListener('close', handleClose)
 *
 * Casey's Windows version has MainWindowCallback() - this is our equivalent.
 *
 * X11 DIFFERENCE:
 * - Windows: OS calls your callback function for each event (push model)
 * - X11: You pull events from a queue with XNextEvent() (pull model)
 *
 * We check the event.type and handle each case, just like:
 * switch(event.type) { case 'click': ..., case 'resize': ... }
 */
inline file_scoped_fn void handle_event(Display *display, Window window,
                                        XEvent *event) {
  switch (event->type) {

  /**
   * CONFIGURE NOTIFY = WINDOW RESIZED
   *
   * Like window.addEventListener('resize', ...)
   *
   * Fires when window size changes. Casey logs "WM_SIZE" in Windows.
   * We'll just print to console (like console.log())
   */
  case ConfigureNotify: {
    int new_width = event->xconfigure.width;
    int new_height = event->xconfigure.height;
    printf("Window resized to: %dx%d\n", new_width, new_height);
    /**
     * **Why do we resize the buffer here?**
     *
     * Because the window size changed! Our old buffer is the wrong size. We
     * need to allocate a new buffer that matches the new window dimensions.
     */
    resize_back_buffer(display, new_width, new_height);

    break;
  }

  /**
   * CLIENT MESSAGE = WINDOW CLOSE BUTTON CLICKED
   *
   * Like window.addEventListener('beforeunload', ...)
   *
   * When user clicks the X button, we receive this event.
   * Casey's WM_CLOSE equivalent.
   *
   * X11 QUIRK: Close isn't automatic - we must check if it's actually
   * the close button message (WM_DELETE_WINDOW protocol)
   */
  case ClientMessage: {
    Atom wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    if ((Atom)event->xclient.data.l[0] == wmDelete) {
      printf("Window close requested\n");
      g_Running = false; // Stop the main loop
    }
    break;
  }

  /**
   * EXPOSE = WINDOW NEEDS REPAINTING
   *
   * Like canvas.redraw() or React re-render
   *
   * Fires when window is uncovered, moved, or resized.
   * Casey's WM_PAINT equivalent.
   *
   * We'll toggle between white and black (Casey uses PatBlt with
   * WHITENESS/BLACKNESS)
   *
   * X11 CONCEPT - Graphics Context (GC):
   * Like setting strokeStyle/fillStyle on a canvas context.
   * GC holds drawing settings (color, line width, etc.)
   */
  case Expose: {
    // Only process the last expose event (count == 0)
    // X11 can send multiple expose events for different regions
    if (event->xexpose.count != 0)
      break;
    printf("Repainting window");
    update_window(display, window, 0, 0, g_BufferWidth, g_BufferHeight);
    break;
  }

  /**
   * FOCUS IN = WINDOW GAINED FOCUS
   *
   * Like window.addEventListener('focus', ...)
   *
   * Casey's WM_ACTIVATEAPP equivalent.
   */
  case FocusIn: {
    printf("Window gained focus\n");
    break;
  }

  /**
   * DESTROY NOTIFY = WINDOW DESTROYED
   *
   * Like window being removed from DOM
   *
   * Casey's WM_DESTROY equivalent.
   * This means the window is being destroyed by the window manager.
   */
  case DestroyNotify: {
    printf("Window destroyed\n");
    g_Running = false;
    break;
  }

  /**
   * DEFAULT CASE
   *
   * Casey has DefWindowProc() for unhandled messages.
   * In X11, we just ignore events we don't care about.
   * X11 doesn't need a "default handler" like Windows.
   */
  default: {
    // Uncomment to see all other events (lots of noise!)
    // printf("Unhandled event type: %d\n", event->type);
    break;
  }
  }
}

/**
 * MAIN FUNCTION
 *
 * Casey's WinMain equivalent. This is the entry point of our program.
 *
 * FLOW:
 * 1. Connect to X server (like connecting to a WebSocket server)
 * 2. Create a window (like document.createElement())
 * 3. Set up event handling (like addEventListener())
 * 4. Run event loop (like while(true) in a game loop)
 * 5. Clean up (like componentWillUnmount in React)
 */
int platform_main() {
  /**
   * STEP 1: CONNECT TO X SERVER
   *
   * XOpenDisplay(NULL) connects to the default display.
   * Display is like a connection to the windowing system.
   *
   * WEB ANALOGY: Like opening a WebSocket connection to a server
   *
   * NULL means "use the DISPLAY environment variable"
   * (usually ":0" for the first monitor)
   */
  Display *display = XOpenDisplay(NULL);
  if (!display) {
    fprintf(stderr, "ERROR: Cannot connect to X server\n");
    return 1;
  }
  printf("Connected to X server\n");

  /**
   * STEP 2: GET SCREEN INFO
   *
   * X11 supports multiple screens (monitors).
   * We'll use the default screen.
   *
   * Like getting window.screen.width/height in JavaScript
   */
  int screen = DefaultScreen(display);
  Window root = RootWindow(display, screen);

  g_BufferWidth = 800;
  g_BufferHeight = 600;

  /**
   * STEP 3: CREATE THE WINDOW
   *
   * This is like:
   * const div = document.createElement('div');
   * div.style.width = '800px';
   * div.style.height = '600px';
   * document.body.appendChild(div);
   *
   * XCreateSimpleWindow parameters:
   * - display: Our connection to X server
   * - root: Parent window (desktop)
   * - x, y: Position (0, 0 = top-left)
   * - width, height: Size in pixels
   * - border_width: Border size
   * - border: Border color
   * - background: Background color
   *
   * Casey uses CreateWindowExA() in Windows with WS_OVERLAPPEDWINDOW
   */
  Window window =
      XCreateSimpleWindow(display,                       //
                          root,                          //
                          0, 0,                          // x, y position
                          g_BufferWidth, g_BufferHeight, // width, height
                          1,                             // border width
                          BlackPixel(display, screen),   // border color
                          WhitePixel(display, screen)    // background color
      );
  printf("Created window\n");

  /**
   * STEP 4: SET WINDOW TITLE
   *
   * Like document.title = "Handmade Hero"
   *
   * Casey sets this in CreateWindowExA() as the window name parameter
   */
  XStoreName(display, window, "Handmade Hero");

  /**
   * STEP 5: REGISTER FOR WINDOW CLOSE EVENT
   *
   * By default, clicking X just closes the window without notifying us.
   * We need to tell X11 we want to handle the close event ourselves.
   *
   * This is like:
   * window.addEventListener('beforeunload', (e) => {
   *   e.preventDefault(); // We handle it ourselves
   * });
   *
   * WM_DELETE_WINDOW is a protocol that says "let me know when user wants to
   * close"
   */
  Atom wmDeleteMsg = XInternAtom(display, "WM_DELETE_WINDOW", false);
  XSetWMProtocols(display, window, &wmDeleteMsg, 1);
  printf("Registered close event handler\n");

  /**
   * STEP 6: SELECT EVENTS WE WANT TO RECEIVE
   *
   * Like calling addEventListener() for specific events.
   * We tell X11 which events we care about.
   *
   * Think of this like:
   * element.addEventListener('click', handler);
   * element.addEventListener('resize', handler);
   * element.addEventListener('focus', handler);
   *
   * Event masks are bit flags (like MB_OK|MB_ICONINFORMATION in Windows)
   */
  XSelectInput(display, window,
               ExposureMask |            // Repaint events (WM_PAINT)
                   StructureNotifyMask | // Resize events (WM_SIZE)
                   FocusChangeMask       // Focus events (WM_ACTIVATEAPP)
  );
  printf("Registered event listeners\n");

  /**
   * STEP 7: SHOW THE WINDOW
   *
   * Like element.style.display = 'block'
   * or element.classList.remove('hidden')
   *
   * Window is created but hidden by default. XMapWindow makes it visible.
   * Casey uses WS_VISIBLE flag to show window immediately.
   */
  XMapWindow(display, window);
  printf("Window shown\n");

  /**
   * STEP 7.5: ALLOCATE INITIAL BACK BUFFER
   *
   * We need to create the back buffer BEFORE entering the event loop
   * so we have something to draw to!
   *
   * Note: ConfigureNotify will fire after XMapWindow, but we also
   * want to draw immediately, so we allocate here.
   */
  resize_back_buffer(display, g_BufferWidth, g_BufferHeight);

  int test_offset = 0;
  int test_y = 0;
  int test_x = 0;

  /**
   * STEP 8: EVENT LOOP (THE HEART OF THE PROGRAM)
   *
   * This is like:
   * while (true) {
   *   const event = await waitForEvent();
   *   handleEvent(event);
   * }
   *
   * Casey's version:
   * for(;;) {
   *   GetMessageA(&Message, ...);
   *   TranslateMessage(&Message);
   *   DispatchMessageA(&Message);
   * }
   *
   * DIFFERENCES:
   * - Windows: GetMessageA() blocks until message arrives (synchronous)
   * - X11: XNextEvent() blocks until event arrives (synchronous)
   *
   * Both are essentially: "Wait for next event, then handle it"
   *
   * This loop runs forever until g_Running becomes false
   * (when user closes the window)
   */
  printf("Entering event loop...\n");
  while (g_Running) {
    XEvent event;

    // /**
    //  * WAIT FOR NEXT EVENT
    //  *
    //  * XNextEvent() blocks (waits) until an event arrives.
    //  * Like await in JavaScript - execution stops here until event.
    //  *
    //  * When an event arrives, it's stored in the 'event' variable.
    //  */
    // XNextEvent(display, &event);

    while (XPending(display) > 0) {
      XNextEvent(display, &event);
      handle_event(display, window, &event);
    }
    // /**
    //  * HANDLE THE EVENT
    //  *
    //  * This is like calling your event handler function.
    //  * Our handle_event() is like Casey's MainWindowCallback()
    //  */
    // handle_event(display, window, &event);

    // ═══════════════════════════════════════════════════════════════════
    // 🎨 DRAW DIAGONAL LINE (Learning Exercise - Day 3)
    // ═══════════════════════════════════════════════════════════════════
    //
    // This draws a white diagonal line every frame.
    //
    // PIXEL ADDRESSING: offset = y * width + x
    //
    // In Day 4+, we'll do proper rendering with frame timing.
    // For now, this demonstrates how to write pixels to memory.
    //
    if (g_PixelData) {
      uint32_t *pixels = (uint32_t *)g_PixelData;

      // Clear buffer to black first (so we don't accumulate old pixels)
      int total_pixels = g_BufferWidth * g_BufferHeight;
      for (int i = 0; i < total_pixels; i++) {
        pixels[i] = 0xFF000000; // Black (ARGB)
      }

      // Calculate diagonal length (stop at window edge)
      int diagonal_length =
          g_BufferWidth < g_BufferHeight ? g_BufferWidth : g_BufferHeight;

      // Draw diagonal line from top-left to bottom-right
      for (int i = 0; i < diagonal_length; i++) {
        int x = i; // Column
        int y = i; // Row

        // ✅ CORRECT formula: offset = y * width + x
        int offset = y * g_BufferWidth + x;

        // 🔴 CRITICAL: Bounds checking!
        if (offset >= 0 && offset < total_pixels) {
          pixels[offset] = 0xFFFFFFFF; // White pixel (ARGB)
        }
      }

      if (test_x + 1 < g_BufferWidth - 1) {
        test_x += 1;
      } else {
        test_x = 0;
        if (test_y + 75 < g_BufferHeight - 1) {
          test_y += 75;
        } else {
          test_y = 0;
        }
      }
      test_offset = test_y * g_BufferWidth + test_x;

      pixels[test_offset] = 0x00FF00; //
      // Display the result
      update_window(display, window, 0, 0, g_BufferWidth, g_BufferHeight);
    }
  }

  // ❌ YOUR ORIGINAL BUG (for reference):
  // for (int i = 0; i < 100; i++) {
  //     int offset = i * g_BufferWidth + i;  // WRONG!
  //     //           ↑
  //     // This treats 'i' as BOTH y AND x in the formula
  //     // offset = i * width + i
  //     //        = i * (width + 1)
  //     //
  //     // At i=99 in 800×600:
  //     // offset = 99 * 801 = 79,299 ✅ (happens to work)
  //     //
  //     // At i=1000:
  //     // offset = 1000 * 801 = 801,000 ❌ (way beyond 479,999!)
  //     //                                   CRASH! 💥
  //     pixels[offset] = 0xFFFFFFFF;
  // }
  //
  // WHY IT SEEMED TO WORK FOR SMALL VALUES:
  // At i=99: offset = 99 * 800 + 99 = 79,299
  // This is less than 480,000 (800×600), so no crash!
  // But it's NOT drawing a perfect diagonal - it's drawing
  // a diagonal with a steeper slope (1 pixel right, 800 pixels down)

  /**
   * STEP 9: CLEANUP - CASEY'S "RESOURCE LIFETIMES IN WAVES" PHILOSOPHY
   *
   * ═══════════════════════════════════════════════════════════════════════
   * IMPORTANT: Read Casey's Day 3 explanation about resource management!
   * ═══════════════════════════════════════════════════════════════════════
   *
   * Casey's Rule: "Don't be myopic about individual resource cleanup.
   *                Think in WAVES based on resource LIFETIME!"
   *
   * WAVE CLASSIFICATION FOR OUR RESOURCES:
   *
   * ┌────────────────────────────────────────────────────────────────┐
   * │ WAVE 1: Process Lifetime (Lives entire program)               │
   * │ ────────────────────────────────────────────                  │
   * │ - Display (X11 connection)                                     │
   * │ - Window                                                       │
   * │                                                                │
   * │ ✅ Casey says: DON'T manually clean these up!                  │
   * │    The OS does it INSTANTLY when process exits (<1ms)          │
   * │                                                                │
   * │ ❌ Bad (OOP way): Manually clean each resource (17ms wasted)   │
   * │    XDestroyImage(backBuffer);   // 2ms                         │
   * │    XDestroyWindow(window);      // 5ms                         │
   * │    XCloseDisplay(display);      // 10ms                        │
   * │                                                                │
   * │ ✅ Good (Casey's way): Just exit! (<1ms)                       │
   * │    return 0;  // OS bulk-cleans ALL resources instantly!       │
   * └────────────────────────────────────────────────────────────────┘
   *
   * ┌────────────────────────────────────────────────────────────────┐
   * │ WAVE 2: State Lifetime (Changes during program)               │
   * │ ────────────────────────────────────────────                  │
   * │ - g_BackBuffer (per window size)                              │
   * │                                                                │
   * │ ✅ Casey says: Clean up WHEN STATE CHANGES (in batches)        │
   * │    We DO clean this in resize_back_buffer() because:          │
   * │    1. We're REPLACING it with a new buffer                     │
   * │    2. This happens DURING program execution                    │
   * │    3. If we don't free, we leak memory on every resize!        │
   * │                                                                │
   * │ This is CORRECT Wave 2 management! ✅                          │
   * └────────────────────────────────────────────────────────────────┘
   *
   * REAL-WORLD IMPACT:
   *
   * Without manual cleanup (Casey's way):
   * ┌─────────────────────────────────────────┐
   * │ User clicks close button                │
   * │ ↓                                       │
   * │ return 0;  // Program exits             │
   * │ ↓                                       │
   * │ OS: "Process died, bulk cleanup!"       │
   * │   - Frees ALL memory in one operation   │
   * │   - Closes ALL X11 resources at once    │
   * │   - Destroys ALL windows instantly      │
   * │ ↓                                       │
   * │ Window closes in <1ms ⚡                 │
   * │ User: "Wow, instant close!" 😊          │
   * └─────────────────────────────────────────┘
   *
   * With manual cleanup (OOP way):
   * ┌─────────────────────────────────────────┐
   * │ User clicks close button                │
   * │ ↓                                       │
   * │ XDestroyImage()... wait 2ms             │
   * │ XDestroyWindow()... wait 5ms            │
   * │ XCloseDisplay()... wait 10ms            │
   * │ ↓                                       │
   * │ Window closes in 17ms 🐌                │
   * │ User: "Why is it laggy?" 😤             │
   * └─────────────────────────────────────────┘
   *
   * CASEY'S QUOTE (Day 3, ~9:20):
   * "If we actually put in code that closes our window before we exit,
   *  we are WASTING THE USER'S TIME. When you exit, Windows will bulk
   *  clean up all of our Windows, all of our handles, all of our memory -
   *  everything gets cleaned up by Windows. If you've ever had one of those
   *  applications where you try to close it and it takes a while to close
   *  down... honestly, a big cause of that is this sort of thing."
   *
   * WEB DEV ANALOGY:
   * JavaScript: const buffer = new Uint8Array(1000000);
   *             // When function ends, GC cleans up automatically
   *             // You don't manually delete it!
   *
   * C (Casey's way): void* buffer = malloc(1000000);
   *                  // When PROCESS ends, OS cleans up automatically
   *                  // You don't manually free it at exit!
   *
   * EXCEPTION - WHEN TO MANUALLY CLEAN:
   * Only clean up resources that are NOT process-lifetime:
   * - Switching levels → Free old level assets, load new ones
   * - Resizing window → Free old buffer, allocate new one ✅ (we do this!)
   * - Closing modal → Free modal resources, keep main window
   *
   * THE BOTTOM LINE:
   * We COULD add cleanup here, but Casey teaches us it's:
   * 1. ❌ Slower (17× slower!)
   * 2. ❌ More code to maintain
   * 3. ❌ More places for bugs
   * 4. ❌ Wastes user's time
   * 5. ✅ OS does it better and faster
   *
   * So we follow Casey's philosophy: Just exit cleanly!
   */
  printf("Goodbye!\n");

  // ✅ NO MANUAL CLEANUP - OS handles it faster and better!
  // The OS will:
  // - Free g_PixelData (and all malloc'd memory)
  // - Destroy g_BackBuffer (XImage)
  // - Close window
  // - Close display connection
  // All in <1ms, automatically! ⚡

  return 0;
}
