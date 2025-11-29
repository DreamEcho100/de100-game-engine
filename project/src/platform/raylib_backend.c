#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>

#define file_scoped_fn static
#define local_persist_var static
#define global_var static

typedef struct {
  void *memory;
  int width;
  int height;
  int bytes_per_pixel;
  Texture2D texture;
  bool has_texture;
} OffscreenBuffer;

global_var OffscreenBuffer g_backbuffer;

inline file_scoped_fn void RenderWeirdGradient(OffscreenBuffer *buffer,
                                               int blue_offset,
                                               int green_offset) {
  int pitch = buffer->width * buffer->bytes_per_pixel;
  uint8_t *row = (uint8_t *)buffer->memory;

  for (int y = 0; y < buffer->height; ++y) {
    uint32_t *pixels = (uint32_t *)row;
    for (int x = 0; x < buffer->width; ++x) {
      uint8_t blue = (x + blue_offset);
      uint8_t green = (y + green_offset);

      *pixels++ = (0xFF000000 | (green << 8) | (blue << 16));
    }
    row += pitch;
  }
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void ResizeBackBuffer(OffscreenBuffer *buffer, int width,
                                     int height) {
  printf("Resizing back buffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = buffer->width;
  int old_height = buffer->height;

  // Update first!
  buffer->width = width;
  buffer->height = height;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (buffer->memory && old_width > 0 && old_height > 0) {
    munmap(buffer->memory, old_width * old_height * buffer->bytes_per_pixel);
  }
  buffer->memory = NULL;

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (buffer->has_texture) {
    UnloadTexture(buffer->texture);
    buffer->has_texture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * buffer->bytes_per_pixel;
  buffer->memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (buffer->memory == MAP_FAILED) {
    buffer->memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return;
  }

  // memset(buffer->memory, 0, buffer_size); // Raylib does not auto-clear like
  // mmap
  memset(buffer->memory, 0, buffer_size);

  buffer->width = width;
  buffer->height = height;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = buffer->memory,
               .width = buffer->width,
               .height = buffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};
  buffer->texture = LoadTextureFromImage(img);
  buffer->has_texture = true;
  printf("Raylib texture created successfully\n");
}

/********************************************************************
 UPDATE WINDOW (BLIT)
 Equivalent to XPutImage or StretchDIBits
*********************************************************************/
file_scoped_fn void UpdateWindowFromBackBuffer(OffscreenBuffer *buffer) {
  if (!buffer->has_texture || !buffer->memory)
    return;

  // Upload CPU → GPU
  UpdateTexture(buffer->texture, buffer->memory);

  // Draw GPU texture → screen
  DrawTexture(buffer->texture, 0, 0, WHITE);
}

/**
 * MAIN FUNCTION
 *
 * Same purpose as X11 version, but MUCH simpler!
 */
int platform_main() {

  /**
   * InitWindow() does ALL of this:
   * - XOpenDisplay() - Connect to display server
   * - XCreateSimpleWindow() - Create the window
   * - XStoreName() - Set window title
   * - XSetWMProtocols() - Set up close handler
   * - XSelectInput() - Register for events
   * - XMapWindow() - Show the window
   *
   * Raylib handles all platform differences internally.
   * Works on Windows, Linux, macOS, web, mobile!
   */
  InitWindow(800, 600, "Handmade Hero");
  printf("Window created and shown\n");

  /**
   * ENABLE WINDOW RESIZING
   *
   * By default, Raylib creates a FIXED-SIZE window.
   * This is different from X11/Windows which allow resizing by default.
   *
   * SetWindowState(FLAG_WINDOW_RESIZABLE) tells Raylib:
   * "Allow the user to resize this window"
   *
   * This is like setting these properties in X11:
   * - Setting size hints with XSetWMNormalHints()
   * - Allowing min/max size changes
   *
   * WHY FIXED BY DEFAULT?
   * Games often need a specific resolution and don't want the window resized.
   * But for Casey's lesson, we want to demonstrate resize events!
   *
   * WEB ANALOGY:
   * It's like the difference between:
   * - <div style="resize: none">  (default Raylib)
   * - <div style="resize: both">  (after SetWindowState)
   */
  SetWindowState(FLAG_WINDOW_RESIZABLE);
  printf("Window is now resizable\n");

  /**
   * OPTIONAL: Set target FPS
   *
   * Raylib has built-in frame rate limiting.
   * We don't need this for Day 002, but it's useful later.
   */
  SetTargetFPS(60);

  g_backbuffer.memory = NULL;
  g_backbuffer.width = 0;
  g_backbuffer.height = 0;
  g_backbuffer.bytes_per_pixel = 4;
  memset(&g_backbuffer.texture, 0, sizeof(g_backbuffer.texture));
  g_backbuffer.has_texture = false;
  g_backbuffer.width = 1280;
  g_backbuffer.height = 720;
  int test_x = 0;
  int test_y = 0;
  int blue_offset = 0;
  int green_offset = 0;

  ResizeBackBuffer(&g_backbuffer, g_backbuffer.width, g_backbuffer.height);

  /**
   * EVENT LOOP
   *
   * WindowShouldClose() checks if:
   * - User clicked X button (WM_CLOSE / ClientMessage)
   * - User pressed ESC key
   *
   * It's like: while (!g_Running) in X11
   *
   * Raylib handles the event queue internally - we don't see XNextEvent()
   */
  while (!WindowShouldClose()) {
    /**
     * CHECK FOR WINDOW RESIZE EVENT
     *
     * Casey's WM_SIZE equivalent.
     * In X11, this is ConfigureNotify event.
     *
     * IsWindowResized() returns true when window size changes.
     * This is like checking event.type == ConfigureNotify in X11.
     *
     * When window is resized, we:
     * 1. Toggle the color (like Casey does)
     * 2. Log the new size (like printf in X11 version)
     *
     * WEB ANALOGY:
     * window.addEventListener('resize', () => {
     *   console.log('Window resized!');
     *   toggleColor();
     * });
     */
    if (IsWindowResized()) {
      printf("Window resized to: %dx%d\n", GetScreenWidth(), GetScreenHeight());
      ResizeBackBuffer(&g_backbuffer, GetScreenWidth(), GetScreenHeight());
    }

    // Render gradient into CPU pixel buffer
    RenderWeirdGradient(&g_backbuffer, blue_offset, green_offset);

    /**
     * BEGIN DRAWING (PAINT EVENT)
     *
     * This is Casey's WM_PAINT event!
     * In X11, this is the Expose event.
     *
     * BeginDrawing() is like:
     * - BeginPaint() in Windows
     * - Handling the Expose event in X11
     *
     * IMPORTANT DIFFERENCE FROM X11:
     * - X11: Expose event only fires when window needs repaint (resize,
     * uncover)
     * - Raylib: BeginDrawing() is called EVERY FRAME (game loop style)
     *
     * This is because Raylib is designed for games which redraw constantly,
     * while traditional windowing systems only redraw when necessary.
     *
     * To match Casey's behavior, we toggle color on RESIZE, not every frame.
     */
    BeginDrawing();

    /**
     * CLEAR SCREEN (Toggle between white/black)
     *
     * This is like:
     * - PatBlt() with WHITENESS/BLACKNESS in Casey's Windows code
     * - XFillRectangle() in X11
     *
     * ClearBackground() fills the entire window with a color
     */
    ClearBackground(BLACK);

    UpdateWindowFromBackBuffer(&g_backbuffer); // CPU → GPU → Screen

    int total_pixels = g_backbuffer.width * g_backbuffer.height;
    if (test_x + 1 < g_backbuffer.width - 1) {
      test_x += 1;
    } else {
      test_x = 0;
      if (test_y + 75 < g_backbuffer.height - 1) {
        test_y += 75;
      } else {
        test_y = 0;
      }
    }
    int test_offset = test_y * g_backbuffer.width + test_x;

    if (test_offset < total_pixels) {

      // Draw the green pixel
      DrawPixel(test_x, test_y, RED);
    }

    /**
     * END DRAWING
     *
     * This is like:
     * - EndPaint() in Windows
     * - XCopyArea() to flip backbuffer to screen in X11
     *
     * Raylib automatically:
     * - Swaps the backbuffer to the front buffer (double buffering)
     * - Handles vsync
     * - Processes window events (resize, focus, etc.)
     * - Logs these events internally
     */
    EndDrawing();

    blue_offset++;
    // green_offset++;
  }

  /**
   * CLEANUP
   *
   * CloseWindow() does ALL of this:
   * - XDestroyWindow()
   * - XCloseDisplay()
   * - Frees all internal Raylib resources
   *
   * One line vs multiple in X11!
   */
  printf("Cleaning up...\n");
  // Cleanup
  if (g_backbuffer.has_texture)
    UnloadTexture(g_backbuffer.texture);
  if (g_backbuffer.memory)
    // free(g_backbuffer.memory);
    munmap(g_backbuffer.memory, g_backbuffer.width * g_backbuffer.height * 4);
  CloseWindow();
  printf("Goodbye!\n");

  return 0;
}
