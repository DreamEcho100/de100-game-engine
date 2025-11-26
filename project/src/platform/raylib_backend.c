/**
 * ====================================================================
 * DAY 002: OPENING A WINDOW (RAYLIB VERSION)
 * ====================================================================
 *
 * Casey's Lesson: Create a window and handle basic window events
 *
 * This does the EXACT same thing as the X11 version, but with Raylib!
 *
 * RAYLIB ADVANTAGE:
 * Raylib is like React compared to vanilla DOM manipulation.
 * It abstracts away platform differences and handles many details
 * automatically.
 *
 * What takes 200+ lines in X11 takes ~30 lines in Raylib!
 */

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
   * STATE: Color toggle
   *
   * Same as g_IsWhite in X11 version
   */
  bool isWhite = true;

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
      isWhite = !isWhite;
      printf("Window resized to: %dx%d - Color: %s\n", GetScreenWidth(),
             GetScreenHeight(), isWhite ? "WHITE" : "BLACK");
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
    ClearBackground(isWhite ? WHITE : BLACK);

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