/**
 * ====================================================================
 * DAY 002: OPENING A WINDOW (X11 VERSION)
 * ====================================================================
 *
 * Casey's Lesson: Create a window and handle basic window events
 *
 * What we're learning:
 * 1. How to create a window
 * 2. How to handle window events (resize, close, paint, etc.)
 * 3. Message/Event loop basics
 * 4. Simple rendering (flashing black/white on repaint)
 *
 * WEB DEV ANALOGY:
 * Think of this like creating an HTML page that responds to events:
 * - Creating window = document.createElement('div')
 * - Event loop = addEventListener() for various events
 * - Painting = updating the canvas or DOM
 *
 * X11 CONCEPTS:
 * - Display: Connection to the X server (like WebSocket to server)
 * - Window: The actual window object (like a DOM element)
 * - Event: User interactions (like DOM events: click, resize, etc.)
 * - XEvent: Structure that holds event data (like JavaScript Event object)
 */

#include <X11/X.h>
#define _POSIX_C_SOURCE 199309L // Enable POSIX functions like nanosleep

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
static bool g_Running = true; // Should we keep running? (like a React state)
static bool g_IsWhite = true; // Current background color toggle

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
static void handle_event(Display *display, Window window, XEvent *event) {
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
    printf("Window resized to: %dx%d\n", event->xconfigure.width,
           event->xconfigure.height);
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

    printf("Repainting window - Color: %s\n", g_IsWhite ? "WHITE" : "BLACK");

    // Create a graphics context (like canvas.getContext('2d'))
    XGCValues values;
    GC gc = XCreateGC(display, window, 0, &values);

    // Set the color (like ctx.fillStyle = 'white')
    // 0xFFFFFF = white, 0x000000 = black (hex RGB, just like CSS!)
    XSetForeground(display, gc, g_IsWhite ? 0xFFFFFF : 0x000000);

    // Get window dimensions (like element.getBoundingClientRect())
    Window root;
    int x, y;
    unsigned int width, hight, border, depth;
    XGetGeometry(display, window, &root, &x, &y, &width, &hight, &border,
                 &depth);

    // Fill the entire window with color (like ctx.fillRect())
    XFillRectangle(display, window, gc, 0, 0, width, hight);

    // Cleanup the graphics context (manual memory management)
    XFreeGC(display, gc);

    // Toggle color for next paint
    g_IsWhite = !g_IsWhite;
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
      XCreateSimpleWindow(display,                     //
                          root,                        //
                          0, 0,                        // x, y position
                          800, 600,                    // width, height
                          1,                           // border width
                          BlackPixel(display, screen), // border color
                          WhitePixel(display, screen)  // background color
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

    /**
     * WAIT FOR NEXT EVENT
     *
     * XNextEvent() blocks (waits) until an event arrives.
     * Like await in JavaScript - execution stops here until event.
     *
     * When an event arrives, it's stored in the 'event' variable.
     */
    XNextEvent(display, &event);

    /**
     * HANDLE THE EVENT
     *
     * This is like calling your event handler function.
     * Our handle_event() is like Casey's MainWindowCallback()
     */
    handle_event(display, window, &event);
  }

  /**
   * STEP 9: CLEANUP (VERY IMPORTANT IN C!)
   *
   * Unlike JavaScript with garbage collection, C requires manual cleanup.
   * Think of this like:
   * - Closing database connections
   * - Unsubscribing from observables
   * - Clearing intervals/timeouts
   * - componentWillUnmount() in React
   *
   * If we don't clean up, we leak memory and system resources.
   *
   * Order matters: Clean up in reverse order of creation!
   * 1. Destroy window (like removeChild())
   * 2. Close display connection (like closing WebSocket)
   */
  printf("Cleaning up...\n");
  XDestroyWindow(display, window);
  XCloseDisplay(display);
  printf("Goodbye!\n");

  return 0;
}