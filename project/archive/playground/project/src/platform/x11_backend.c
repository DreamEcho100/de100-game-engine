// Enable POSIX features (needed for nanosleep and clock_gettime functions)
// Similar to how you'd polyfill browser APIs, but for system APIs
// IMPORTANT: This MUST be defined BEFORE any includes!
#define _POSIX_C_SOURCE 199309L

#include "platform.h"
#include <X11/X.h>
#include <stdbool.h>

#include "../game/game.h"
#include "base.h"

// X11 is the windowing system for Linux (like the DOM API for browsers)
// Think of it as the low-level API that lets you create windows, draw pixels,
// and handle input
#include <X11/Xatom.h>   // X11 atom definitions (like XA_ATOM)
#include <X11/Xft/Xft.h> // Xft for anti-aliased fonts (smooth text rendering)
#include <X11/Xlib.h>    // Core X11 functions (creating windows, etc.)
#include <X11/Xutil.h>   // X11 utility functions
#include <X11/extensions/XShm.h> // Shared memory extension (for fast rendering)

#include <stdio.h>   // printf, fprintf (like console.log)
#include <stdlib.h>  // malloc, free (manual memory management)
#include <string.h>  // string manipulation functions
#include <sys/ipc.h> // Inter-Process Communication (for shared memory)
#include <sys/shm.h> // Shared memory functions (like SharedArrayBuffer in JS)
#include <time.h>    // Timing functions (like performance.now())
#include <unistd.h>  // Unix system calls

/**
 * Draw a rounded rectangle with optional border (like border-radius in CSS)
 *
 * X11 doesn't have rounded rectangles by default, so we fake it
 * by drawing filled arcs at corners and rectangles for the sides
 */
static void draw_rounded_rect(Display *d, Drawable target, GC gc, int x, int y,
                              int width, int height, int radius) {
  // Draw non-overlapping rectangles to avoid artifacts
  // Center horizontal rectangle
  XFillRectangle(d, target, gc, x + radius, y, width - 2 * radius, height);
  // Left vertical rectangle
  XFillRectangle(d, target, gc, x, y + radius, radius, height - 2 * radius);
  // Right vertical rectangle
  XFillRectangle(d, target, gc, x + width - radius, y + radius, radius,
                 height - 2 * radius);

  // Draw corner arcs (like border-radius)
  XFillArc(d, target, gc, x, y, 2 * radius, 2 * radius, 90 * 64,
           90 * 64); // Top-left
  XFillArc(d, target, gc, x + width - 2 * radius, y, 2 * radius, 2 * radius, 0,
           90 * 64); // Top-right
  XFillArc(d, target, gc, x, y + height - 2 * radius, 2 * radius, 2 * radius,
           180 * 64, 90 * 64); // Bottom-left
  XFillArc(d, target, gc, x + width - 2 * radius, y + height - 2 * radius,
           2 * radius, 2 * radius, 270 * 64, 90 * 64); // Bottom-right
}

/**
 * Draw rounded rectangle outline (border only)
 */
static void draw_rounded_rect_border(Display *d, Drawable target, GC gc, int x,
                                     int y, int width, int height, int radius) {
  // Draw straight edges
  XDrawLine(d, target, gc, x + radius, y, x + width - radius, y); // Top
  XDrawLine(d, target, gc, x + radius, y + height, x + width - radius,
            y + height);                                           // Bottom
  XDrawLine(d, target, gc, x, y + radius, x, y + height - radius); // Left
  XDrawLine(d, target, gc, x + width, y + radius, x + width,
            y + height - radius); // Right

  // Draw corner arcs (outline only)
  XDrawArc(d, target, gc, x, y, 2 * radius, 2 * radius, 90 * 64,
           90 * 64); // Top-left
  XDrawArc(d, target, gc, x + width - 2 * radius, y, 2 * radius, 2 * radius, 0,
           90 * 64); // Top-right
  XDrawArc(d, target, gc, x, y + height - 2 * radius, 2 * radius, 2 * radius,
           180 * 64, 90 * 64); // Bottom-left
  XDrawArc(d, target, gc, x + width - 2 * radius, y + height - 2 * radius,
           2 * radius, 2 * radius, 270 * 64, 90 * 64); // Bottom-right
}

/**
 * Check if mouse is over a button (like :hover in CSS)
 */
static int is_mouse_over_button(int mouseX, int mouseY, int btnX, int btnY,
                                int btnW, int btnH) {
  return mouseX >= btnX && mouseX <= btnX + btnW && mouseY >= btnY &&
         mouseY <= btnY + btnH;
}

/**
 * Convert RGBA color components to BGRA format (32-bit packed integer)
 *
 * In X11, pixels are often stored as 32-bit integers in BGRA order:
 * - Bit 0-7:   Red
 * - Bit 8-15:  Green
 * - Bit 16-23: Blue
 * - Bit 24-31: Alpha
 *
 * This is similar to how CSS colors work, but stored as a single number
 * instead of separate channels. Example: 0xFFFF0000 = opaque red
 *
 * @param r Red component (0-255)
 * @param g Green component (0-255)
 * @param b Blue component (0-255)
 * @param a Alpha component (0-255, 255=opaque)
 * @return 32-bit BGRA color value
 */
static uint32_t bgra_format(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
  // Bit shifting to pack 4 bytes into one 32-bit integer
  // Similar to: (a << 24) | (b << 16) | (g << 8) | r
  // In JS this would be like: (a * 0x1000000) + (b * 0x10000) + (g * 0x100) + r
  return (a << 24) | (b << 16) | (g << 8) | r;
}

inline int platform_show_message_box(const char *title, const char *message,
                                     ShowMessageBoxOptions options) {
  Display *display = XOpenDisplay(NULL);

  if (display == NULL) {
    fprintf(stderr, "Unable to open X display for message box\n");
    return 0; // Indicate failure
  }

  int screen = XDefaultScreen(display);
  Window root = RootWindow(display, screen);

  // Default colors (like CSS variables)
  // System now provides title bar, so we only need dialog colors
  unsigned long dialogBg =
      options.bgColor ? options.bgColor : 0xF5F5F5; // Light gray
  unsigned long borderCol =
      options.borderColor ? options.borderColor : 0xE0E0E0; // Gray border

  // Create window with proper attributes
  XSetWindowAttributes attrs;
  attrs.background_pixel = dialogBg;
  attrs.border_pixel = borderCol;
  // Removed override_redirect to allow window manager to handle
  // moving/decorations

  Window dialog = XCreateWindow(display, root, options.x, options.y,
                                options.width, options.height,
                                0, // No border (we'll draw our own)
                                CopyFromParent, InputOutput, CopyFromParent,
                                CWBackPixel | CWBorderPixel, &attrs);

  // Set window title
  XStoreName(display, dialog, title);

  // Tell window manager this is a dialog window (gets proper decorations)
  Atom wm_window_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
  Atom wm_window_type_dialog =
      XInternAtom(display, "_NET_WM_WINDOW_TYPE_DIALOG", False);
  XChangeProperty(display, dialog, wm_window_type, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&wm_window_type_dialog, 1);

  // Make window stay on top
  Atom wm_state = XInternAtom(display, "_NET_WM_STATE", False);
  Atom wm_state_above = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
  XChangeProperty(display, dialog, wm_state, XA_ATOM, 32, PropModeReplace,
                  (unsigned char *)&wm_state_above, 1);

  // Handle close button
  Atom wm_delete = XInternAtom(display, "WM_DELETE_WINDOW", False);
  XSetWMProtocols(display, dialog, &wm_delete, 1);

  // Select events we care about (including mouse motion for hover effects)
  XSelectInput(display, dialog,
               ExposureMask | KeyPressMask | ButtonPressMask |
                   PointerMotionMask | ButtonReleaseMask);

  // Show the window
  XMapWindow(display, dialog);
  XRaiseWindow(display, dialog); // Bring to front

  /**
   * Create double buffer to prevent flickering
   *
   * A Pixmap is an off-screen drawable surface (like an OffscreenCanvas in web)
   * We draw to this invisible surface first, then copy it to the visible window
   * in one atomic operation. This prevents tearing and flickering.
   *
   * Think of it like:
   * const offscreenCanvas = document.createElement('canvas');
   * const offscreenCtx = offscreenCanvas.getContext('2d');
   * // Draw everything to offscreenCtx
   * // Then: ctx.drawImage(offscreenCanvas, 0, 0);
   *
   * Parameters:
   * - display: Connection to X server
   * - dialog: Parent window (determines which screen to use)
   * - width, height: Size of the buffer
   * - DefaultDepth: Color depth (bits per pixel, usually 24 or 32)
   */
  Pixmap backbuffer =
      XCreatePixmap(display, dialog, options.width, options.height,
                    DefaultDepth(display, screen));

  // Create graphics context
  GC gc = XCreateGC(display, dialog, 0, NULL);

  /**
   * Set up Xft for smooth, anti-aliased fonts (like modern web fonts)
   *
   * Visual: Describes the color format and display capabilities
   * Think of it as the "color mode" - like knowing if you're in RGB, sRGB, or
   * P3 color space
   *
   * Colormap: A palette of colors that can be displayed
   * On modern systems this is usually TrueColor (24-bit RGB)
   * Think of it like the browser's color profile
   */
  Visual *visual = DefaultVisual(display, screen);
  Colormap colormap = DefaultColormap(display, screen);

  /**
   * XftDraw is like a 2D rendering context for text (similar to
   * CanvasRenderingContext2D) It handles all the anti-aliasing, font hinting,
   * and subpixel rendering
   *
   * We attach it to our backbuffer so text is drawn off-screen first
   * Think of it as: const ctx = offscreenCanvas.getContext('2d');
   */
  XftDraw *xftdraw = XftDrawCreate(display, backbuffer, visual, colormap);

  /**
   * Load modern anti-aliased font (uses fontconfig, same as web browsers)
   *
   * XftFontOpenName uses the same font-matching system as Firefox/Chrome
   * The format is "Family-Size" like "DejaVu Sans-14" (similar to CSS font
   * shorthand)
   *
   * We try multiple fonts in order of preference (like CSS font-family
   * fallback):
   * 1. DejaVu Sans - Most popular Linux font, excellent Unicode coverage
   * 2. Liberation Sans - Red Hat's font, similar to Arial
   * 3. sans-serif - Generic fallback (system will choose)
   *
   * This is like CSS:
   * font-family: 'DejaVu Sans', 'Liberation Sans', sans-serif;
   * font-size: 14px;
   *
   * XftFont* is like a Font object - it contains glyph metrics, kerning, etc.
   */
  XftFont *font = XftFontOpenName(display, screen, "DejaVu Sans-14");
  if (!font) {
    font = XftFontOpenName(display, screen, "Liberation Sans-14");
  }
  if (!font) {
    font =
        XftFontOpenName(display, screen, "sans-serif-14"); // Generic fallback
  }
  if (!font) {
    fprintf(stderr, "Warning: Could not load Xft font, text may not display\n");
  }

  // Calculate button layout (like flexbox in CSS)
  int buttonCount = options.buttonCount > 0 ? options.buttonCount : 1;
  const char **buttonLabels =
      options.buttons ? options.buttons : (const char *[]){"OK"};
  int *buttonValues = options.buttonValues ? options.buttonValues : (int[]){1};

  int buttonWidth = 90;
  int buttonHeight = 35;
  int buttonSpacing = 12;
  int totalButtonWidth =
      (buttonWidth + buttonSpacing) * buttonCount - buttonSpacing;
  int buttonStartX = (options.width - totalButtonWidth) / 2;
  int buttonY = options.height - buttonHeight - 20;

  // Button state tracking (like React state)
  int hoveredButton = -1;
  int result = 0;
  int running = 1;
  int needsFullRedraw = 1; // Track if we need to redraw everything

  /**
   * XEvent is a union that can hold any type of X11 event
   * Think of it like a discriminated union in TypeScript:
   * type XEvent = KeyPressEvent | MouseMoveEvent | ExposeEvent | ...
   *
   * The 'type' field tells us which event it is (like event.type in DOM)
   */
  XEvent event;

  /**
   * Main event loop - runs until user closes dialog
   * This is like an async event listener loop in JavaScript:
   *
   * while (running) {
   *   const event = await getNextEvent();
   *   handleEvent(event);
   * }
   */
  while (running) {
    /**
     * Wait for next event (BLOCKING call)
     *
     * XNextEvent will pause here until an event arrives
     * Think of it like: const event = await new Promise(resolve => {...})
     *
     * Events include:
     * - Mouse movement (MotionNotify)
     * - Mouse clicks (ButtonPress)
     * - Keyboard (KeyPress)
     * - Window needs redraw (Expose)
     * - etc.
     */
    XNextEvent(display, &event);

    switch (event.type) {
    case Expose:
      if (event.xexpose.count > 0)
        break;             // Skip if more expose events coming
      needsFullRedraw = 1; // Window was exposed, redraw everything
      // Fall through to redraw

      // Note: We removed the 'break' so Expose falls through to redraw
    }

    // Unified rendering (draw to backbuffer, then copy to window)
    if (needsFullRedraw) {
      // Clear background in backbuffer (like clearing a canvas)
      XSetForeground(display, gc, dialogBg);
      XFillRectangle(display, backbuffer, gc, 0, 0, options.width,
                     options.height);

      // System provides title bar now, so we don't draw one
      // Just draw icon based on type (like FontAwesome icons)
      const char *icon = options.type == MSG_BX_INFO       ? "ℹ"
                         : options.type == MSG_BX_WARNING  ? "⚠"
                         : options.type == MSG_BX_ERROR    ? "✖"
                         : options.type == MSG_BX_QUESTION ? "?"
                                                           : "ℹ";

      unsigned long iconColor = options.type == MSG_BX_INFO ? 0x2196F3 : // Blue
                                    options.type == MSG_BX_WARNING ? 0xFF9800
                                                                   : // Orange
                                    options.type == MSG_BX_ERROR ? 0xF44336
                                                                 : // Red
                                    options.type == MSG_BX_QUESTION ? 0x4CAF50
                                                                    : // Green
                                    0x2196F3;

      // Draw icon with Xft for smooth rendering
      // XftColor is like a CSS color but in X11 format for anti-aliased
      // rendering
      XftColor xftIconColor;

      /**
       * Convert our hex color (0xRRGGBB) to XRenderColor format
       *
       * XRenderColor uses 16-bit values (0-65535) instead of 8-bit (0-255)
       * Think of it like converting rgba(255, 0, 0, 1) to a different scale
       *
       * Bit operations explained:
       * - iconColor >> 16: Shift right 16 bits to get red channel
       *   Example: 0x2196F3 >> 16 = 0x21 (red component)
       * - & 0xFF: Mask to keep only last 8 bits (0-255 range)
       * - * 257: Scale from 8-bit (0-255) to 16-bit (0-65535)
       *   Why 257? Because 255 * 257 = 65535 (max 16-bit value)
       *
       * In JS terms: (parseInt('21', 16) / 255) * 65535
       */
      XRenderColor renderIconColor = {
          .red = (unsigned short)(((iconColor >> 16) & 0xFF) * 257),
          .green = (unsigned short)(((iconColor >> 8) & 0xFF) * 257),
          .blue = (unsigned short)((iconColor & 0xFF) * 257),
          .alpha = 0xFFFF // Fully opaque (like alpha: 1.0)
      };

      // Allocate the color in the X server's colormap (like registering a CSS
      // custom property)
      XftColorAllocValue(display, visual, colormap, &renderIconColor,
                         &xftIconColor);
      // Align icon with message text baseline
      int baselineY = 35;
      XftDrawStringUtf8(xftdraw, &xftIconColor, font, 25, baselineY,
                        (XftChar8 *)icon, strlen(icon));

      /**
       * Free the color from the colormap
       *
       * In X11, colors are resources that need to be allocated and freed
       * (similar to how you'd close database connections or file handles)
       *
       * The colormap has limited slots, so we clean up after use
       * Think of it like: URL.revokeObjectURL() for blob URLs
       *
       * In C, you must manually free almost everything you allocate!
       * No garbage collector will do it for you.
       */
      XftColorFree(display, visual, colormap, &xftIconColor);

      // Draw message text with Xft (like <p> tag, word-wrapped)
      XftColor xftTextColor;
      XRenderColor renderTextColor = {
          .red = 0x3333, .green = 0x3333, .blue = 0x3333, .alpha = 0xFFFF};
      XftColorAllocValue(display, visual, colormap, &renderTextColor,
                         &xftTextColor);

      int msgX = 45;
      int msgY = baselineY; // Same baseline as icon
      int maxLineWidth = options.width - 50;

      // Simple word wrapping (like CSS word-wrap: break-word)
      const char *line = message;
      while (*line) {
        // strchr finds the next space character (like String.indexOf(' '))
        const char *space = strchr(line, ' ');

        // Calculate word length using pointer arithmetic
        // If space found: distance between pointers, else: length of remaining
        // string In JS: const wordLen = space ? message.indexOf(' ',
        // currentPos) - currentPos : message.length
        int wordLen = space ? (space - line) : strlen(line);

        /**
         * XGlyphInfo holds text measurement info (like TextMetrics in Canvas
         * API)
         *
         * Important fields:
         * - width: Actual width of the rendered glyphs (like
         * measureText().width)
         * - height: Height of the text
         * - xOff: Horizontal advance (where next character should start)
         * - yOff: Vertical advance
         *
         * Think of this as:
         * const metrics = ctx.measureText(text);
         * const width = metrics.width;
         */
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, font, (XftChar8 *)line, wordLen, &extents);

        if (extents.xOff > maxLineWidth) {
          msgY += 20; // New line
        }

        XftDrawStringUtf8(xftdraw, &xftTextColor, font, msgX, msgY,
                          (XftChar8 *)line, wordLen);
        msgX += extents.xOff + 5;

        if (msgX > maxLineWidth) {
          msgX = 50;
          msgY += 20;
        }

        line += wordLen;
        if (*line == ' ')
          line++;
        if (!*line)
          break;
      }
      XftColorFree(display, visual, colormap, &xftTextColor);

      // Draw buttons (like <button> elements with hover states)
      for (int i = 0; i < buttonCount; i++) {
        int btnX = buttonStartX + i * (buttonWidth + buttonSpacing);

        // Button background color (changes on hover, like :hover)
        unsigned long btnColor =
            (i == hoveredButton) ? 0x1976D2 : 0x2196F3; // Darker when hovered
        XSetForeground(display, gc, btnColor);
        draw_rounded_rect(display, backbuffer, gc, btnX, buttonY, buttonWidth,
                          buttonHeight, 4);

        // Draw subtle border to smooth out the rounded corners
        XSetForeground(display, gc, 0x1565C0); // Darker blue border
        draw_rounded_rect_border(display, backbuffer, gc, btnX, buttonY,
                                 buttonWidth, buttonHeight, 4);

        // Button text with Xft (like button label)
        const char *label = buttonLabels[i];
        XGlyphInfo extents;
        XftTextExtentsUtf8(display, font, (XftChar8 *)label, strlen(label),
                           &extents);

        XftColor xftWhite;
        XRenderColor renderWhite = {
            .red = 0xFFFF, .green = 0xFFFF, .blue = 0xFFFF, .alpha = 0xFFFF};
        XftColorAllocValue(display, visual, colormap, &renderWhite, &xftWhite);

        XftDrawStringUtf8(
            xftdraw, &xftWhite, font, btnX + (buttonWidth - extents.width) / 2,
            buttonY + buttonHeight / 2 + 7, (XftChar8 *)label, strlen(label));
        XftColorFree(display, visual, colormap, &xftWhite);
      }

      /**
       * Copy backbuffer to window (like ctx.drawImage in canvas)
       *
       * This is the "page flip" or "buffer swap" - the moment everything
       * becomes visible XCopyArea is a fast hardware-accelerated copy operation
       *
       * Think of it like:
       * ctx.drawImage(offscreenCanvas, 0, 0);
       *
       * Parameters:
       * - display: Connection to X server
       * - backbuffer: Source (what we drew to)
       * - dialog: Destination (visible window)
       * - gc: Graphics context (drawing settings)
       * - 0, 0: Source X,Y (copy from top-left of backbuffer)
       * - width, height: Size of area to copy
       * - 0, 0: Destination X,Y (paste at top-left of window)
       *
       * This single atomic operation prevents flickering because users never
       * see the intermediate drawing steps - they only see the final result!
       */
      XCopyArea(display, backbuffer, dialog, gc, 0, 0, options.width,
                options.height, 0, 0);

      /**
       * XFlush forces X11 to send all pending commands to the server NOW
       * Without this, commands might be buffered and delayed
       * Think of it like: await fetch() or canvas.requestAnimationFrame()
       * It ensures our drawing appears immediately
       */
      XFlush(display);
      needsFullRedraw = 0;
    }

    // Continue processing other events
    switch (event.type) {
    case Expose:
      // Already handled above
      break;

    case MotionNotify:
      // Track mouse for hover effects (like onMouseMove in React)
      {
        int oldHovered = hoveredButton;
        hoveredButton = -1;

        for (int i = 0; i < buttonCount; i++) {
          int btnX = buttonStartX + i * (buttonWidth + buttonSpacing);
          if (is_mouse_over_button(event.xmotion.x, event.xmotion.y, btnX,
                                   buttonY, buttonWidth, buttonHeight)) {
            hoveredButton = i;
            break;
          }
        }

        // Redraw only if hover state changed (like React re-render)
        if (oldHovered != hoveredButton) {
          needsFullRedraw = 1; // Trigger redraw in next loop iteration
        }
      }
      break;

    case ButtonPress:
      // Check which button was clicked (like onClick handler)
      for (int i = 0; i < buttonCount; i++) {
        int btnX = buttonStartX + i * (buttonWidth + buttonSpacing);
        if (is_mouse_over_button(event.xbutton.x, event.xbutton.y, btnX,
                                 buttonY, buttonWidth, buttonHeight)) {
          result = buttonValues[i];
          running = 0;
          break;
        }
      }
      break;

    case KeyPress:
      // Keyboard shortcuts (like onKeyPress)
      {
        KeySym key = XLookupKeysym(&event.xkey, 0);
        if (key == XK_Escape) {
          result = 0; // ESC = Cancel
          running = 0;
        } else if (key == XK_Return) {
          result = buttonValues[0]; // Enter = First button
          running = 0;
        }
      }
      break;

    case ClientMessage:
      // Window close button
      if ((Atom)event.xclient.data.l[0] == wm_delete) {
        result = 0;
        running = 0;
      }
      break;
    }
  }

  /**
   * ====================================================================
   * CLEANUP - Manual Memory Management
   * ====================================================================
   *
   * In C, there's NO garbage collector! Every resource we allocate must
   * be manually freed. Think of it like closing all event listeners,
   * database connections, file handles, etc. in JavaScript.
   *
   * Order matters - clean up in reverse order of creation:
   * - Font and drawing context first (they depend on display)
   * - Then pixmap and GC (they depend on display)
   * - Window (it depends on display)
   * - Finally display connection itself
   *
   * Missing cleanup = memory leaks! The OS won't clean up for you
   * until the entire process exits.
   */

  // Close font (free font metrics, glyph cache, etc.)
  if (font)
    XftFontClose(display, font);

  // Destroy Xft drawing context (free rendering resources)
  if (xftdraw)
    XftDrawDestroy(xftdraw);

  // Free the offscreen buffer (like deleting an OffscreenCanvas)
  XFreePixmap(display, backbuffer);

  // Free graphics context (drawing state like colors, line width, etc.)
  XFreeGC(display, gc);

  // Destroy the dialog window (removes it from screen and frees resources)
  XDestroyWindow(display, dialog);

  // Close connection to X server (like socket.close())
  // This also frees any remaining resources we forgot to clean up
  XCloseDisplay(display);

  return result;
}

/**
 * Main entry point for the X11 platform backend
 *
 * This is like the main() function in a Node.js app or the root component
 * in React - it sets up everything and runs the game loop.
 *
 * Flow:
 * 1. Connect to X11 server (like connecting to a browser's rendering engine)
 * 2. Create a window (like creating a <canvas> element)
 * 3. Set up shared memory for fast pixel rendering (like WebGL buffers)
 * 4. Run game loop (like requestAnimationFrame)
 * 5. Clean up resources when done
 */
int platform_main() {
  // Window dimensions (like canvas.width and canvas.height in HTML5)
  int width = 800, height = 600;

  // int canStart =
  //     show_message_box("Start Game", "Do you want to start the game?",
  //                      (ShowMessageBoxOptions){.type = MSG_BX_QUESTION,
  //                                              .icon = "question",
  //                                              .width = 300,
  //                                              .height = 150},
  //                      *platform_show_message_box);

  // Example 1: Simple Yes/No dialog (like window.confirm)
  const char *yesNoButtons[] = {"Yes", "No"};
  int yesNoValues[] = {1, 0};

  int userChoice = show_message_box(
      "Confirm Action", "Are you sure you want to start the game?",
      (ShowMessageBoxOptions){.type = MSG_BX_QUESTION,
                              .width = 400,
                              .height = 200,
                              .buttons = yesNoButtons,
                              .buttonValues = yesNoValues,
                              .buttonCount = 2},
      platform_show_message_box);

  if (!userChoice) {
    return 0; // User chose not to start the game
  }

  /**
   * Connect to the X11 display server
   *
   * Think of Display as the connection to your graphics system.
   * It's like opening a WebSocket connection to a rendering server.
   * NULL means "use the default display" (usually :0)
   *
   * In web terms: This is like getting access to the browser's rendering
   * context
   */
  Display *d = XOpenDisplay(NULL);
  if (!d) {
    // fprintf is like console.error() - prints to stderr
    fprintf(stderr, "Failed to open display\n");
    return 1; // Non-zero return = error (like process.exit(1) in Node.js)
  }

  /**
   * Check if MIT-SHM (Shared Memory) extension is available
   *
   * MIT-SHM is an optimization that lets us write pixels directly to shared
   * memory instead of copying them over a socket. This is MUCH faster.
   *
   * Think of it like the difference between:
   * - Slow: Sending pixels via fetch() calls
   * - Fast: Writing directly to a SharedArrayBuffer
   *
   * Not all X11 servers support this (especially remote ones), so we check
   * first.
   */
  int shm_major, shm_minor; // Version numbers (we don't actually use these)
  Bool pixmaps; // Whether pixmaps are supported (we don't use this either)

  if (!XShmQueryVersion(d, &shm_major, &shm_minor, &pixmaps)) {
    fprintf(stderr, "MIT-SHM extension not available\n");
    XCloseDisplay(d); // Always clean up! (like closing database connections)
    return 1;
  }

  /**
   * Get the root window (the desktop background)
   *
   * Every window needs a parent. The root window is the top-level parent.
   * It's like the <body> element in HTML - everything goes inside it.
   */
  Window root = DefaultRootWindow(d);

  /**
   * Create our game window
   *
   * Parameters:
   * - d: Display connection
   * - root: Parent window (the desktop)
   * - 0, 0: Initial position (x, y) on screen
   * - width, height: Window size
   * - 0: Border width (we don't want a border)
   * - BlackPixel: Border color (unused since border width is 0)
   * - WhitePixel: Background color
   *
   * This is like: document.createElement('div') and setting its style
   */
  Window win = XCreateSimpleWindow(d, root, 0, 0, width, height, 0,
                                   BlackPixel(d, 0), WhitePixel(d, 0));

  /**
   * Tell X11 which events we want to receive for this window
   *
   * ExposureMask: Window needs to be redrawn (like 'resize' event)
   * KeyPressMask: Key was pressed (like 'keydown' event)
   *
   * The | operator combines flags (like setting multiple CSS classes)
   *
   * This is like: window.addEventListener('keydown', handler)
   */
  XSelectInput(d, win, ExposureMask | KeyPressMask);

  /**
   * Make the window visible
   *
   * Windows start hidden by default. This is like setting display: block
   * or calling element.style.visibility = 'visible'
   */
  XMapWindow(d, win);

  /**
   * Create a Graphics Context (GC)
   *
   * A GC holds drawing state like colors, line width, fonts, etc.
   * It's like a CanvasRenderingContext2D in HTML5 Canvas.
   *
   * We need this to draw our image to the window.
   *
   * Parameters:
   * - d: Display connection
   * - win: Window to draw in
   * - 0: No special flags
   * - NULL: Use default values for all settings
   */
  GC gc = XCreateGC(d, win, 0, NULL);

  /**
   * ====================================================================
   * SHARED MEMORY SETUP (The Performance Optimization)
   * ====================================================================
   *
   * This section sets up a shared memory buffer that both our program
   * and the X server can access. This avoids copying pixel data.
   *
   * Normal flow (SLOW):
   * 1. We write pixels to local buffer
   * 2. XPutImage copies entire buffer through socket to X server
   * 3. X server displays it
   *
   * Shared memory flow (FAST):
   * 1. We write pixels directly to shared buffer
   * 2. XShmPutImage just tells X server "render what's in shared memory"
   * 3. X server displays it (no copy needed!)
   *
   * This is like the difference between:
   * - Uploading an image via HTTP (slow)
   * - Pointing to an image already on the server (fast)
   */

  /**
   * Structure to hold shared memory info
   * Think of this as metadata about our shared buffer
   */
  XShmSegmentInfo shminfo;

  /**
   * Get the visual (color format) for our display
   * A "Visual" describes how colors are represented (RGB, indexed, etc.)
   * It's like knowing if you're working with RGB, RGBA, or indexed colors
   */
  Visual *visual = DefaultVisual(d, 0);

  /**
   * Create an XImage backed by shared memory
   *
   * XImage is like an ImageData object in Canvas API - it holds raw pixel data
   *
   * Parameters:
   * - d: Display connection
   * - visual: Color format to use
   * - DefaultDepth(d, 0): Color depth (usually 24 or 32 bits per pixel)
   * - ZPixmap: Format (raw pixel array, not a bitmap)
   * - NULL: No initial data (we'll write to it later)
   * - &shminfo: Shared memory info (filled in by this function)
   * - width, height: Image dimensions
   */
  XImage *img = XShmCreateImage(d, visual, DefaultDepth(d, 0), ZPixmap, NULL,
                                &shminfo, width, height);

  if (!img) {
    fprintf(stderr, "Failed to create shared memory image\n");
    XCloseDisplay(d);
    return 1;
  }

  /**
   * Allocate the actual shared memory segment
   *
   * shmget = "shared memory get" - creates a shared memory segment
   *
   * Parameters:
   * - IPC_PRIVATE: Create new segment (not accessing existing one)
   * - size: Total bytes needed (bytes_per_line * height)
   * - IPC_CREAT | 0777: Create if doesn't exist, with full permissions
   *
   * Returns: Shared memory ID (like a file descriptor)
   *
   * In web terms: This is like creating a SharedArrayBuffer
   */
  shminfo.shmid =
      shmget(IPC_PRIVATE, img->bytes_per_line * img->height, IPC_CREAT | 0777);
  if (shminfo.shmid < 0) {
    fprintf(stderr, "Failed to allocate shared memory\n");
    XDestroyImage(img); // Clean up image first
    XCloseDisplay(d);
    return 1;
  }

  /**
   * Attach the shared memory to our process's address space
   *
   * shmat = "shared memory attach" - maps the memory into our program
   *
   * After this, we can read/write to shminfo.shmaddr like any pointer.
   * It's like mapping a file into memory or getting a pointer to a buffer.
   *
   * We also set img->data to point to the same memory, so XImage knows
   * where its pixels are.
   */
  shminfo.shmaddr = img->data = (char *)shmat(shminfo.shmid, 0, 0);

  /**
   * Tell X11 whether it can modify this memory
   * False = X server can modify it (though it usually won't)
   */
  shminfo.readOnly = False;

  /**
   * Attach the shared memory to the X server
   *
   * Now both our process and the X server have access to the same memory!
   * This is the magic that makes fast rendering possible.
   *
   * If this fails, it might be because:
   * - Running over SSH without X11 forwarding
   * - X server doesn't support shared memory
   * - Security restrictions
   */
  if (!XShmAttach(d, &shminfo)) {
    fprintf(stderr, "Failed to attach shared memory to X server\n");
    shmdt(shminfo.shmaddr);             // Detach from our process
    shmctl(shminfo.shmid, IPC_RMID, 0); // Mark for deletion
    XDestroyImage(img);
    XCloseDisplay(d);
    return 1;
  }

  /**
   * Mark the shared memory segment for deletion
   *
   * Don't worry - it won't actually be deleted until everyone detaches!
   * This is like setting a file to be deleted when the last handle closes.
   *
   * Why do this now? So if our program crashes, the memory doesn't leak.
   * The OS will clean it up when both our process and X server detach.
   */
  shmctl(shminfo.shmid, IPC_RMID, 0);

  /**
   * ====================================================================
   * GAME LOOP SETUP
   * ====================================================================
   */

  /**
   * Cast shared memory to uint32_t pointer
   *
   * Each pixel is 32 bits (4 bytes): BGRA format
   * So we can treat the buffer as an array of uint32_t values.
   *
   * Example:
   * buffer[0] = 0xFFFF0000;  // First pixel = red
   * buffer[1] = 0xFF00FF00;  // Second pixel = green
   *
   * This is like: const pixels = new Uint32Array(imageData.data.buffer)
   */
  uint32_t *buffer = (uint32_t *)shminfo.shmaddr;

  /**
   * Initialize game state
   *
   * {0} initializes all fields to zero (like {} in JavaScript)
   * The game will use this to track things like player position,
   * animation state, etc.
   */
  GameState state = {0};

  /**
   * Frame timing setup (for 60 FPS)
   *
   * struct timespec holds time with nanosecond precision:
   * - tv_sec: seconds
   * - tv_nsec: nanoseconds (0-999,999,999)
   *
   * 16,666,667 nanoseconds = 16.67 milliseconds = 1/60 second
   * This is like: const targetFrameTime = 1000/60 in JS
   */
  struct timespec target_frame_time = {0, 16666667}; // 60 FPS
  struct timespec frame_start, frame_end, sleep_time;

  /**
   * ====================================================================
   * MAIN GAME LOOP (Like requestAnimationFrame)
   * ====================================================================
   *
   * This runs forever until the user presses a key.
   * Each iteration:
   * 1. Process input events
   * 2. Update game state
   * 3. Render frame
   * 4. Sleep to maintain 60 FPS
   */
  while (1) {
    /**
     * Record when this frame started
     *
     * CLOCK_MONOTONIC: A clock that never goes backwards (good for timing)
     * This is like: const frameStart = performance.now()
     */
    clock_gettime(CLOCK_MONOTONIC, &frame_start);

    /**
     * Process all pending events (input handling)
     *
     * XPending returns number of events waiting in queue
     * We process them all before updating the game.
     *
     * This is like processing all events in the event queue
     * before rendering a frame in a game engine.
     */
    while (XPending(d)) {
      XEvent e;          // Structure to hold event data
      XNextEvent(d, &e); // Get next event from queue (blocking)

      /**
       * Check event type
       *
       * We only care about KeyPress events for now.
       * When any key is pressed, we exit the game.
       *
       * In a real game, you'd check e.xkey.keycode to see which key
       * was pressed and handle it appropriately.
       */
      if (e.type == KeyPress)
        goto cleanup; // Jump to cleanup code (like a try/finally)
    }

    /**
     * Update game logic and render to buffer
     *
     * This function:
     * 1. Updates game state (physics, AI, etc.)
     * 2. Draws pixels directly to buffer
     *
     * The game writes pixels like:
     * buffer[y * width + x] = bgra_format(255, 0, 0, 255);  // Red pixel at
     * (x,y)
     *
     * This is like: ctx.putImageData(imageData, 0, 0) in Canvas
     */
    game_update(&state, buffer, width, height, bgra_format);

    /**
     * Display the rendered frame
     *
     * XShmPutImage tells X server to draw our shared memory image.
     * No pixel data is copied - it just says "display what's in shared memory"!
     *
     * Parameters:
     * - d: Display connection
     * - win: Window to draw in
     * - gc: Graphics context
     * - img: Image to display
     * - 0, 0: Source position (top-left of image)
     * - 0, 0: Destination position (top-left of window)
     * - width, height: Size to display
     * - False: Don't send completion event (we don't need it)
     *
     * XFlush ensures the command is sent immediately (not buffered)
     *
     * This is like: requestAnimationFrame callback finishing - browser displays
     * frame
     */
    XShmPutImage(d, win, gc, img, 0, 0, 0, 0, width, height, False);
    XFlush(d); // Force X11 to process the display command now

    /**
     * Frame rate limiting (cap at 60 FPS)
     *
     * Calculate how long this frame took, then sleep for the remaining time
     * to maintain a consistent 60 FPS.
     */

    // Get current time (end of frame)
    clock_gettime(CLOCK_MONOTONIC, &frame_end);

    /**
     * Calculate elapsed time in nanoseconds
     *
     * Convert seconds difference to nanoseconds and add nanoseconds difference
     * This is like: const elapsed = performance.now() - frameStart
     */
    long elapsed_ns = (frame_end.tv_sec - frame_start.tv_sec) * 1000000000L +
                      (frame_end.tv_nsec - frame_start.tv_nsec);

    /**
     * Calculate how long to sleep
     *
     * If frame took 10ms and target is 16.67ms, sleep for 6.67ms
     */
    long sleep_ns = target_frame_time.tv_nsec - elapsed_ns;

    /**
     * Sleep if we have time left in the frame
     *
     * If frame took longer than 16.67ms, skip sleeping (we're running slow)
     * Otherwise sleep to maintain consistent frame rate
     *
     * This is like: await new Promise(r => setTimeout(r, remainingTime))
     */
    if (sleep_ns > 0) {
      sleep_time.tv_sec = 0;
      sleep_time.tv_nsec = sleep_ns;
      nanosleep(&sleep_time, NULL); // Sleep for specified time
    }
  }
  // Loop continues until user presses a key...

/**
 * ====================================================================
 * CLEANUP (Resource Deallocation)
 * ====================================================================
 *
 * In C, we have to manually free everything we allocated.
 * There's no garbage collector! Think of this like cleaning up
 * event listeners, closing database connections, etc.
 *
 * Order matters - we clean up in reverse order of creation:
 * 1. Detach shared memory from X server
 * 2. Destroy XImage
 * 3. Detach shared memory from our process
 * 4. Close display connection
 */
cleanup:
  /**
   * Detach shared memory from X server
   * Now X server can't access this memory anymore
   */
  XShmDetach(d, &shminfo);

  /**
   * Destroy the XImage structure
   * This frees the metadata, but not the actual pixel data
   * (that's in shared memory, which we clean up separately)
   */
  XDestroyImage(img);

  /**
   * Detach shared memory from our process
   *
   * Since we marked the segment for deletion earlier (IPC_RMID),
   * and this is the last detach, the OS will now free the memory.
   *
   * This is like: buffer = null; to release a reference
   */
  shmdt(shminfo.shmaddr);

  /**
   * Close connection to X server
   * This also frees the window, GC, and all other X resources
   *
   * Like: socket.close() or db.disconnect()
   */
  XCloseDisplay(d);

  /**
   * Return success (0 = no error in Unix convention)
   * Non-zero would indicate an error occurred
   */
  return 0;
}