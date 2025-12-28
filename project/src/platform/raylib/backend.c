#define _POSIX_C_SOURCE 199309L
#include "backend.h"
#include "../../base.h"
#include "../../game.h"
#include "audio.h"

#include <raylib.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>

typedef struct {
  Texture2D texture;
  bool has_texture;
} OffscreenBufferMeta;

file_scoped_global_var OffscreenBufferMeta g_backbuffer_meta = {0};
file_scoped_global_var struct timespec g_frame_start;
file_scoped_global_var struct timespec g_frame_end;

// file_scoped_global_var int g_joystick_fd = -1;

/*
Will be added when needed
typedef struct {
    int width;
    int height;
} X11WindowDimension;

file_scoped_fn X11WindowDimension
get_window_dimension(Display *display, Window window) {
    XWindowAttributes attrs;
    XGetWindowAttributes(display, window, &attrs);
    return (X11WindowDimension){attrs.width, attrs.height};
}
*/

// ═══════════════════════════════════════════════════════════════
// 🎮 Initialize gamepad (Raylib cross-platform!)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn bool raylib_init_gamepad(GameState *game_state) {
  printf("Searching for gamepad...\n");

  // Try gamepads 0-3 (Raylib supports up to 4 controllers)
  for (int i = 0; i < 4; i++) {
    if (IsGamepadAvailable(i)) {
      const char *name = GetGamepadName(i);
      printf("✅ Gamepad %d connected: %s\n", i, name);

      // Use the first available gamepad
      game_state->gamepad_id = i;
      return true;
    }
  }

  printf("❌ No gamepad found\n");
  printf("  - Is controller plugged in or paired via Bluetooth?\n");
  printf("  - Raylib supports Xbox, PlayStation, Nintendo controllers\n");
  printf("  - Game will run with keyboard input only\n");

  game_state->gamepad_id = -1; // No gamepad
  return false;
}

inline file_scoped_fn void handle_keyboard_inputs() {

  g_game_state.controls.up = IsKeyDown(KEY_W) || IsKeyDown(KEY_UP);
  g_game_state.controls.left = IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT);
  g_game_state.controls.down = IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN);
  g_game_state.controls.right = IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT);

  // Musical note frequencies (same as X11 version)
  if (IsKeyPressed(KEY_Z)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_C4;
  }
  if (IsKeyPressed(KEY_X)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_D4;
  }
  if (IsKeyPressed(KEY_C)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_E4;
  }
  if (IsKeyPressed(KEY_V)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_F4;
  }
  if (IsKeyPressed(KEY_B)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_G4;
  }
  if (IsKeyPressed(KEY_N)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_A4;
  }
  if (IsKeyPressed(KEY_M)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_B4;
  }
  if (IsKeyPressed(KEY_COMMA)) {
    g_game_state.controls.set_to_defined_tone = DEFINED_TONE_C5;
  }

  // Volume control ([ and ])
  if (IsKeyPressed(KEY_LEFT_BRACKET)) {
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      g_game_state.controls.decrease_sound_volume = true;
      g_game_state.controls.increase_sound_volume = false;
    } else {
      g_game_state.controls.move_sound_pan_left = true;
      g_game_state.controls.move_sound_pan_right = false;
    }
  }
  if (IsKeyPressed(KEY_RIGHT_BRACKET)) {
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      g_game_state.controls.increase_sound_volume = true;
      g_game_state.controls.decrease_sound_volume = false;
    } else {
      g_game_state.controls.move_sound_pan_right = true;
      g_game_state.controls.move_sound_pan_left = false;
    }
  }

  if (IsKeyPressed(KEY_ESCAPE)) {
    printf("ESCAPE pressed - exiting\n");
    g_game_state.is_running = false;
  }

  if (IsKeyPressed(KEY_F1)) {
    printf("F1 pressed - showing audio debug\n");
    raylib_debug_audio();
  }
}

// ═══════════════════════════════════════════════════════════════
// 🎮 Poll gamepad state (Raylib's cross-platform API!)
// ═══════════════════════════════════════════════════════════════
file_scoped_fn void raylib_poll_gamepad(GameState *game_state) {
  // Check if gamepad is still connected
  if (game_state->gamepad_id < 0 ||
      !IsGamepadAvailable(game_state->gamepad_id)) {
    return;
  }

  int gamepad = game_state->gamepad_id;

  // ═══════════════════════════════════════════════════════════
  // 🎮 BUTTON EVENTS (Raylib maps to standard layout)
  // ═══════════════════════════════════════════════════════════
  // Raylib uses SDL2 button mapping (works across all controllers!)
  // - PlayStation: X=A, O=B, □=X, △=Y
  // - Xbox: A=A, B=B, X=X, Y=Y
  // - Nintendo: B=A, A=B, Y=X, X=Y

  // Face buttons
  game_state->controls.a_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
  game_state->controls.b_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT);
  game_state->controls.x_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_LEFT);
  game_state->controls.y_button =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_UP);

  // Shoulder buttons
  game_state->controls.left_shoulder =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_TRIGGER_1);
  game_state->controls.right_shoulder =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_RIGHT_TRIGGER_1);

  // Menu buttons
  game_state->controls.back =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_MIDDLE_LEFT); // Share/Back
  game_state->controls.start = IsGamepadButtonDown(
      gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT); // Options/Start

  // D-pad (works on ALL controllers!)
  game_state->controls.up =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_UP);
  game_state->controls.down =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_DOWN);
  game_state->controls.left =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_LEFT);
  game_state->controls.right =
      IsGamepadButtonDown(gamepad, GAMEPAD_BUTTON_LEFT_FACE_RIGHT);

  // ═══════════════════════════════════════════════════════════
  // 🎮 ANALOG STICKS (normalized -1.0 to +1.0)
  // ═══════════════════════════════════════════════════════════
  // Raylib automatically handles deadzones and normalization!

  game_state->controls.left_stick_x =
      (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_X) *
                32767.0f);
  game_state->controls.left_stick_y =
      (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_Y) *
                32767.0f);
  game_state->controls.right_stick_x =
      (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_X) *
                32767.0f);
  game_state->controls.right_stick_y =
      (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_Y) *
                32767.0f);

  // Triggers (also normalized 0.0 to +1.0)
  game_state->controls.left_trigger =
      (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_LEFT_TRIGGER) *
                32767.0f);
  game_state->controls.right_trigger =
      (int16_t)(GetGamepadAxisMovement(gamepad, GAMEPAD_AXIS_RIGHT_TRIGGER) *
                32767.0f);

  // Debug output for button presses (only print on state change)
  if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
    printf("Button A pressed\n");
  }
  if (IsGamepadButtonPressed(gamepad, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
    printf("Button Start pressed\n");
  }
}

// Raylib pixel composer (R8G8B8A8 format)
file_scoped_fn uint32_t compose_pixel_rgba(uint8_t r, uint8_t g, uint8_t b,
                                           uint8_t a) {
  return (a << 24) | (b << 16) | (g << 8) | r;
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void resize_back_buffer(OffscreenBuffer *backbuffer,
                                       OffscreenBufferMeta *backbuffer_meta,
                                       int width, int height) {
  printf("Resizing back backbuffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = backbuffer->width;
  int old_height = backbuffer->height;

  // Update first!
  backbuffer->width = width;
  backbuffer->height = height;
  backbuffer->pitch = backbuffer->width * backbuffer->bytes_per_pixel;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (backbuffer->memory && old_width > 0 && old_height > 0) {
    munmap(backbuffer->memory,
           old_width * old_height * backbuffer->bytes_per_pixel);
  }
  backbuffer->memory = NULL;

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (backbuffer_meta->has_texture) {
    UnloadTexture(backbuffer_meta->texture);
    backbuffer_meta->has_texture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * backbuffer->bytes_per_pixel;
  backbuffer->memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (backbuffer->memory == MAP_FAILED) {
    backbuffer->memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return;
  }

  // // memset(backbuffer->memory, 0, buffer_size); // Raylib does not
  // auto-clear like
  // // mmap
  // memset(backbuffer->memory, 0, buffer_size);

  backbuffer->width = width;
  backbuffer->height = height;
  backbuffer->pitch = backbuffer->width * backbuffer->bytes_per_pixel;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = backbuffer->memory,
               .width = backbuffer->width,
               .height = backbuffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               // format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8
               .mipmaps = 1};
  backbuffer_meta->texture = LoadTextureFromImage(img);
  backbuffer_meta->has_texture = true;
  printf("Raylib texture created successfully\n");
}

/********************************************************************
 UPDATE WINDOW (BLIT)
 Equivalent to XPutImage or StretchDIBits
*********************************************************************/
file_scoped_fn void
update_window_from_backbuffer(OffscreenBuffer *backbuffer,
                              OffscreenBufferMeta *backbuffer_meta) {
  if (!backbuffer_meta->has_texture || !backbuffer->memory)
    return;

  // Upload CPU → GPU
  UpdateTexture(backbuffer_meta->texture, backbuffer->memory);

  // Draw GPU texture → screen
  DrawTexture(backbuffer_meta->texture, 0, 0, WHITE);
}

/********************************************************************
 RESIZE BACKBUFFER
 - Free old CPU memory
 - Free old GPU texture
 - Allocate new CPU pixel memory
 - Create new Raylib texture (GPU)
*********************************************************************/
file_scoped_fn void ResizeBackBuffer(OffscreenBuffer *backbuffer,
                                     OffscreenBufferMeta *backbuffer_meta,
                                     int width, int height) {
  printf("Resizing back backbuffer → %dx%d\n", width, height);

  if (width <= 0 || height <= 0) {
    printf("Rejected resize: invalid size\n");
    return;
  }

  int old_width = backbuffer->width;
  int old_height = backbuffer->height;

  // Update first!
  backbuffer->width = width;
  backbuffer->height = height;

  // ---- 1. FREE OLD PIXEL MEMORY
  // -------------------------------------------------
  if (backbuffer->memory && old_width > 0 && old_height > 0) {
    munmap(backbuffer->memory,
           old_width * old_height * backbuffer->bytes_per_pixel);
  }
  backbuffer->memory = NULL;

  // ---- 2. FREE OLD TEXTURE
  // ------------------------------------------------------
  if (backbuffer_meta->has_texture) {
    UnloadTexture(backbuffer_meta->texture);
    backbuffer_meta->has_texture = false;
  }
  // ---- 3. ALLOCATE NEW BACKBUFFER
  // ----------------------------------------------
  int buffer_size = width * height * backbuffer->bytes_per_pixel;
  backbuffer->memory = mmap(NULL, buffer_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

  if (backbuffer->memory == MAP_FAILED) {
    backbuffer->memory = NULL;
    fprintf(stderr, "mmap failed: could not allocate %d bytes\n", buffer_size);
    return;
  }

  // memset(backbuffer->memory, 0, buffer_size); // Raylib does not auto-clear
  // like mmap
  memset(backbuffer->memory, 0, buffer_size);

  backbuffer->width = width;
  backbuffer->height = height;

  // ---- 4. CREATE RAYLIB TEXTURE
  // -------------------------------------------------
  Image img = {.data = backbuffer->memory,
               .width = backbuffer->width,
               .height = backbuffer->height,
               .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
               .mipmaps = 1};
  backbuffer_meta->texture = LoadTextureFromImage(img);
  backbuffer_meta->has_texture = true;
  printf("Raylib texture created successfully\n");
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
  InitWindow(1250, 720, "Handmade Hero");
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

  // Initialize game state
  init_game_state();

  // ═══════════════════════════════════════════════════════════
  // 🎮 Initialize gamepad (cross-platform!)
  // ═══════════════════════════════════════════════════════════
  raylib_init_gamepad(&g_game_state);

  // ═══════════════════════════════════════════════════════════
  // 🔊 Initialize audio (Day 7-9)
  // ═══════════════════════════════════════════════════════════
  raylib_init_audio();

  int init_backbuffer_status =
      init_backbuffer(1280, 720, 4, compose_pixel_rgba);
  if (init_backbuffer_status != 0) {
    fprintf(stderr, "Failed to initialize backbuffer\n");
    return init_backbuffer_status;
  }

  init_game_state();

  resize_back_buffer(&g_backbuffer, &g_backbuffer_meta, g_backbuffer.width,
                     g_backbuffer.height);

  // DEBUG: Verify allocation and texture creation
  // printf("DBG: backbuffer mem=%p w=%d h=%d bpp=%d has_texture=%d\n",
  //        g_backbuffer.memory, g_backbuffer.width, g_backbuffer.height,
  //        g_backbuffer.bytes_per_pixel, g_backbuffer_meta.has_texture);

  // // DEBUG: Fill with a solid test color (should paint the screen if pipeline
  // // works)
  // if (g_backbuffer.memory) {
  //   uint32_t *px = (uint32_t *)g_backbuffer.memory;
  //   int total = g_backbuffer.width * g_backbuffer.height;
  //   for (int i = 0; i < total; ++i) {
  //     px[i] = 0xFF0000FF; // test: opaque red (R=255,G=0,B=0,A=255) in
  //     R8G8B8A8
  //   }
  //   BeginDrawing();
  //   ClearBackground(BLACK);
  //   update_window_from_backbuffer(&g_backbuffer, &g_backbuffer_meta);
  //   EndDrawing();
  // }

  printf("Entering main loop...\n");

  clock_gettime(CLOCK_MONOTONIC, &g_frame_start);

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
      ResizeBackBuffer(&g_backbuffer, &g_backbuffer_meta, GetScreenWidth(),
                       GetScreenHeight());
    }

    // ═══════════════════════════════════════════════════════
    // ⌨️ KEYBOARD INPUT (cross-platform!)
    // ═══════════════════════════════════════════════════════
    handle_keyboard_inputs();

    // ═══════════════════════════════════════════════════════
    // 🎮 GAMEPAD INPUT (cross-platform!)
    // ═══════════════════════════════════════════════════════
    raylib_poll_gamepad(&g_game_state);

    // ═══════════════════════════════════════════════════════
    // 🎮 Process all input
    // ═══════════════════════════════════════════════════════
    handle_controls();

    // ═══════════════════════════════════════════════════════
    // 🎨 RENDER
    // ═══════════════════════════════════════════════════════
    if (g_backbuffer.memory) {
      // Render gradient

      render_weird_gradient();
      testPixelAnimation(0xFF0000FF); // opaque red in R8G8B8A8

      game_update_and_render(0xFF0000FF);
      // Example: Convert Raylib Color struct to int (RGBA)
      // You can now use color_int as a packed 32-bit RGBA value
      BeginDrawing();
      ClearBackground(BLACK);
      update_window_from_backbuffer(&g_backbuffer, &g_backbuffer_meta);
      EndDrawing();
    }

    clock_gettime(CLOCK_MONOTONIC, &g_frame_end);

    real64 ms_per_frame =
        (g_frame_end.tv_sec - g_frame_start.tv_sec) * 1000.0 +
        (g_frame_end.tv_nsec - g_frame_start.tv_nsec) * 1000000.0;
    real64 fps = 1000.0 / ms_per_frame;

    // printf("%.2fms/f, %.2ff/s\n", ms_per_frame, fps);

    g_frame_start = g_frame_end;
  }

  /**
   * STEP 9: CLEANUP - CASEY'S "RESOURCE LIFETIMES IN WAVES" PHILOSOPHY
   *
   * Raylib handles most resources as process-lifetime (Wave 1).
   * The OS will bulk-clean everything instantly when the process exits.
   * We only need to manually clean up state-lifetime resources (Wave 2),
   * like our backbuffer, if we were replacing them during runtime.
   *
   * So, following Casey's philosophy, we just exit cleanly!
   */
  printf("Goodbye!\n");
  RED;

  return 0;
}