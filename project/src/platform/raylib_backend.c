#include <raylib.h>
#include <stdbool.h>
#include <stdio.h>

/**
 * MAIN FUNCTION
 *
 * Same purpose as X11 version, but MUCH simpler!
 */
int platform_main() {

  /**
   * STEP 1-7: ALL IN ONE LINE!
   *
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
      printf("Window resized to: %dx%d - Color: %s\n", GetScreenWidth(),
             GetScreenHeight());
    }

    /**
     * CHECK FOR WINDOW FOCUS EVENT
     *
     * Casey's WM_ACTIVATEAPP equivalent.
     * In X11, this is FocusIn/FocusOut event.
     *
     * RAYLIB LIMITATION:
     * Raylib doesn't expose focus events directly in the simple API.
     * For Day 002, this isn't critical - focus events are just logged.
     *
     * In a real game, you'd use this to pause when window loses focus.
     */

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

    // ═══════════════════════════════════════════════════════════════════
    // 🎨 DRAW DIAGONAL LINE (Learning Exercise - Day 3)
    // ═══════════════════════════════════════════════════════════════════
    //
    // RAYLIB vs X11 COMPARISON:
    //
    // X11 (manual pixel manipulation):
    //   uint32_t *pixels = (uint32_t *)g_PixelData;
    //   for (int i = 0; i < min(width, height); i++) {
    //       int offset = i * width + i;
    //       pixels[offset] = 0xFFFFFFFF;  // White pixel
    //   }
    //   XPutImage(display, window, gc, backBuffer, ...);
    //
    // Raylib (high-level drawing API):
    //   DrawLine(x1, y1, x2, y2, WHITE);  // One line! 🎯
    //
    // Both do the SAME thing, but Raylib abstracts away:
    // - Pixel addressing math (offset = y * width + x)
    // - Bounds checking
    // - Back buffer management
    // - Blitting to screen
    //
    // This is like comparing:
    // - Canvas 2D API: ctx.strokeRect(x, y, w, h)
    // - WebGL: Setting up buffers, shaders, vertices...
    //
    int width = GetScreenWidth();
    int height = GetScreenHeight();
    int diagonal_length = width < height ? width : height;

    // Draw diagonal line from top-left (0,0) to bottom-right
    // In X11, we did: pixels[i * width + i] = white
    // In Raylib, we just call: DrawLine()!
    DrawLine(0, 0, diagonal_length - 1, diagonal_length - 1, WHITE);

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
  CloseWindow();
  printf("Goodbye!\n");

  return 0;
}