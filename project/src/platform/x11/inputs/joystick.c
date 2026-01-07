#include "../../../base.h"
#include "../../../game.h"
#include "../../_common/input.h"
#include <fcntl.h>
#include <linux/joystick.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

typedef struct {
  int fd;                // File descriptor for /dev/input/jsX
  char device_name[128]; // For debugging
} LinuxJoystickState;

file_scoped_global_var LinuxJoystickState g_joysticks[MAX_JOYSTICK_COUNT] = {0};

// ═══════════════════════════════════════════════════════════════
// 🎮 JOYSTICK DYNAMIC LOADING (Casey's Pattern for Linux)
// ═══════════════════════════════════════════════════════════════
// Define function signature macro
#define LINUX_JOYSTICK_READ(name) ssize_t name(int fd, struct js_event *event)

// Create typedef
typedef LINUX_JOYSTICK_READ(linux_joystick_read);

// Stub implementation (no joystick available)
LINUX_JOYSTICK_READ(LinuxJoystickReadStub) {
  (void)fd;
  (void)event;
  // Return -1 = error/no device (like ERROR_DEVICE_NOT_CONNECTED)
  return -1;
}

// Global function pointer (initially stub)
file_scoped_global_var linux_joystick_read *LinuxJoystickRead_ =
    LinuxJoystickReadStub;

// Redefine API name
#define LinuxJoystickRead LinuxJoystickRead_

// Real implementation (only used if joystick found)
file_scoped_fn LINUX_JOYSTICK_READ(linux_joystick_read_impl) {
  // This is what actually reads from /dev/input/js*
  return read(fd, event, sizeof(*event));
}

void linux_init_joystick(GameControllerInput *controller_old_input,
                         GameControllerInput *controller_new_input) {
  printf("Searching for gamepad...\n");

  const char *device_paths[] = {"/dev/input/js0", "/dev/input/js1",
                                "/dev/input/js2", "/dev/input/js3"};

  // Initialize ALL controllers FIRST
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    if (i == KEYBOARD_CONTROLLER_INDEX)
      continue;

    controller_old_input[i].controller_index = i;
    controller_old_input[i].is_connected = false;

    controller_new_input[i].controller_index = i;
    controller_new_input[i].is_connected = false;

    int g_joysticks_index = i - 1; // Adjust for keyboard at index 0
    if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
      g_joysticks[g_joysticks_index].fd = -1;
      memset(g_joysticks[g_joysticks_index].device_name, 0,
             sizeof(g_joysticks[g_joysticks_index].device_name));
    }
  }

  // Mark keyboard (slot 0) as connected AFTER the loop
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_old_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_connected = true;
  controller_new_input[KEYBOARD_CONTROLLER_INDEX].is_analog = false;

  for (int i = MAX_KEYBOARD_COUNT; i < MAX_JOYSTICK_COUNT; i++) {
    int fd = open(device_paths[i - MAX_KEYBOARD_COUNT], O_RDONLY | O_NONBLOCK);

    if (fd >= 0) {
      char name[128] = {0};
      if (ioctl(fd, JSIOCGNAME(sizeof(name)), name) >= 0) {

        printf("i: %d\n", i);
        // Skip virtual devices
        if (strstr(name, "virtual") || strstr(name, "keyd")) {
          close(fd);
          continue;
        }

        // Store controller index AND file descriptor separately
        controller_old_input[i].controller_index = i;
        controller_old_input[i].is_connected = true;
        controller_old_input[i].is_analog = true; // Joysticks are analog

        controller_new_input[i].controller_index = i;
        controller_new_input[i].is_connected = true;
        controller_new_input[i].is_analog = true; // Joysticks are analog

        int g_joysticks_index =
            i - MAX_KEYBOARD_COUNT; // Adjust for keyboard at index 0
        if (g_joysticks_index >= 0 && g_joysticks_index < MAX_JOYSTICK_COUNT) {
          g_joysticks[g_joysticks_index].fd = fd;
          strncpy(g_joysticks[g_joysticks_index].device_name, name,
                  sizeof(g_joysticks[g_joysticks_index].device_name) - 1);
        }

        // Create wrapper function that uses our fd
        // (Linux doesn't have DLLs, but we can still use the pattern!)
        LinuxJoystickRead_ = linux_joystick_read_impl; // Real implementation

        printf("✅ Joystick connected: %s\n", name);
        // return true;
        continue; //
      } else {
        close(fd);
      }
    }
  }
}

void linux_close_joysticks(void) {
  for (int i = 0; i < MAX_JOYSTICK_COUNT; i++) {
    if (g_joysticks[i].fd >= 0) {
      close(g_joysticks[i].fd);
      g_joysticks[i].fd = -1;
      memset(g_joysticks[i].device_name, 0, sizeof(g_joysticks[i].device_name));
    }
  }
}

void linux_poll_joystick(GameInput *old_input, GameInput *new_input) {

  for (int controller_index = 0; controller_index < MAX_CONTROLLER_COUNT;
       controller_index++) {

    // Get controller state
    GameControllerInput *old_controller =
        &old_input->controllers[controller_index];
    GameControllerInput *new_controller =
        &new_input->controllers[controller_index];

    int g_joysticks_index = controller_index - 1;
    if (g_joysticks_index < 0 || g_joysticks_index >= MAX_JOYSTICK_COUNT) {
      // Skip keyboard (index 0)
      continue;
    }
    LinuxJoystickState *joystick_state = &g_joysticks[g_joysticks_index];

    // printf("controller_index: %d, new_controller->is_connected: %d\n",
    //        controller_index, new_controller->is_connected);
    // Use continue instead of return
    if (!new_controller->is_connected || joystick_state->fd < 0) {
      continue; // Skip this controller, check the next one
    }

    struct js_event event;

    while (LinuxJoystickRead(joystick_state->fd, &event) == sizeof(event)) {

      if (event.type & JS_EVENT_INIT) {
        continue;
      }

      // NOTE: Ignore the joystick for now since I don't have them

      // printf("JS_EVENT_BUTTON: %d, JS_EVENT_AXIS: %d\n", JS_EVENT_BUTTON,
      //  JS_EVENT_AXIS);
      // printf("event.type: %d, JS_EVENT_AXIS: %d\n", event.type,
      // JS_EVENT_AXIS);
      // ═══════════════════════════════════════════════════════════
      // 🎮 BUTTON EVENTS
      // ═══════════════════════════════════════════════════════════
      if (event.type == JS_EVENT_BUTTON) {
        bool is_pressed = (event.value != 0);

        printf("event.number: %d\n", event.number);
        printf("is_pressed: %d\n", is_pressed);

        // ───────────────────────────────────────────────────────
        // PS4/PS5 "Wireless Controller" Button Mapping
        // ───────────────────────────────────────────────────────
        // Button layout (PlayStation naming):
        //   0 = X (cross)     - South button
        //   1 = O (circle)    - East button
        //   3 = □ (square)    - West button
        //   2 = △ (triangle)  - North button
        //   4 = L1 (left bumper)
        //   5 = R1 (right bumper)
        //   6 = L2 (left trigger button, not analog value!)
        //   7 = R2 (right trigger button, not analog value!)
        //   8 = Share (PS4) / Create (PS5)
        //   9 = Options (PS4/PS5)
        //  10 = L3 (left stick button)
        //  11 = R3 (right stick button)
        //  12 = PS button (PlayStation logo)
        //  13 = Touchpad button (PS4/PS5)
        // ───────────────────────────────────────────────────────

        switch (event.number) {
          // // Face buttons (cross, circle, square, triangle)
          // case 0: // X (cross) - Map to A button
          //   new_controller1->a_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button X (cross) pressed\n");
          //   break;

          // case 1: // O (circle) - Map to B button
          //   new_controller1->b_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button O (circle) pressed\n");
          //   break;

          // case 3: // □ (square) - Map to X button
          //   new_controller1->x_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button □ (square) pressed\n");
          //   break;

          // case 2: // △ (triangle) - Map to Y button
          //   new_controller1->y_button = is_pressed;
          //   if (is_pressed)
          //     printf("Button △ (triangle) pressed\n");
          //   break;

          // Shoulder buttons
        case 4: // L1
                // new_controller1->left_shoulder = is_pressed;
                // if (is_pressed)
                //   printf("Button L1 pressed\n");
          process_game_button_state(is_pressed, &new_controller->left_shoulder);
          break;

        case 5: // R1
          // new_controller1->right_shoulder = is_pressed;
          // if (is_pressed)
          //   printf("Button R1 pressed\n");
          process_game_button_state(is_pressed,
                                    &new_controller->right_shoulder);
          break;

          // // Trigger buttons (L2/R2 as digital buttons)
          // // Note: Analog values are on axes 3 and 4
          // case 6: // L2 button
          //   if (is_pressed)
          //     printf("Button L2 pressed\n");
          //   break;

          // case 7: // R2 button
          //   if (is_pressed)
          //     printf("Button R2 pressed\n");
          //   break;

          // // Menu buttons
          // case 8: // Share/Create - Map to Back
          //   new_controller1->back = is_pressed;
          //   if (is_pressed)
          //     printf("Button Share/Create pressed\n");
          //   break;

          // case 9: // Options - Map to Start
          //   new_controller1->start = is_pressed;
          //   if (is_pressed)
          //     printf("Button Options pressed\n");
          //   break;

          // // Stick buttons
          // case 11: // L3 (left stick click)
          //   if (is_pressed)
          //     printf("Button L3 (left stick) pressed\n");
          //   break;

          // case 12: // R3 (right stick click)
          //   if (is_pressed)
          //     printf("Button R3 (right stick) pressed\n");
          //   break;

          // // Special buttons
          // case 10: // PS button
          //   if (is_pressed)
          //     printf("Button PS (logo) pressed\n");
          //   break;

          // case 13: // Touchpad button
          //   if (is_pressed)
          //     printf("Button Touchpad pressed\n");
          //   break;

          // default:
          //   // Unknown button
          //   if (is_pressed) {
          //     printf("Unknown button %d pressed\n", event.number);
          //   }
          //   break;
        }
      }

      // ═══════════════════════════════════════════════════════════
      // 🎮 AXIS EVENTS (Analog Sticks + Triggers + D-Pad)
      // ═══════════════════════════════════════════════════════════
      else if (event.type == JS_EVENT_AXIS) {
        new_controller->is_analog = true;
        // printf("new_controller->is_analog: %d\n", new_controller->is_analog);

        new_controller->start_x = old_controller->end_x;
        new_controller->start_y = old_controller->end_y;

        // ───────────────────────────────────────────────────────
        // PS4/PS5 "Wireless Controller" Axis Mapping
        // ───────────────────────────────────────────────────────
        // Axes:
        //   0 = Left stick X    (-32767 left, +32767 right)
        //   1 = Left stick Y    (-32767 up,   +32767 down)
        //   2 = Right stick X   (-32767 left, +32767 right)
        //   3 = L2 trigger      (0 released, +32767 pressed)
        //   4 = R2 trigger      (0 released, +32767 pressed)
        //   5 = Right stick Y   (-32767 up,   +32767 down)
        //   6 = D-pad X         (-32767 left, +32767 right)
        //   7 = D-pad Y         (-32767 up,   +32767 down)
        // ───────────────────────────────────────────────────────

        switch (event.number) {
        // ───────────────────────────────────────────────────────
        // Left Stick (axes 0-1)
        // ───────────────────────────────────────────────────────
        case 0: {
          // ═══════════════════════════════════════════════════
          // 🔥 CRITICAL: Linux joystick normalization
          // ═══════════════════════════════════════════════════
          // Linux /dev/input/jsX range: -32767 to +32767
          // This is SYMMETRIC (unlike XInput's asymmetric range)!
          //
          // SO WE USE SINGLE DIVISOR:
          //   X = (real32)event.value / 32767.0f;
          //
          // Casey's XInput needs TWO divisors:
          //   if(Pad->sThumbLX < 0) X = value / 32768.0f;
          //   else X = value / 32767.0f;
          //
          // Because XInput has -32768 to +32767 (asymmetric)
          // ═══════════════════════════════════════════════════

          // new_controller1->left_stick_x = event.value;
          real32 x = (real32)event.value / 32767.0f;

          new_controller->end_x = x;
          // new_controller1->min_x = fminf(new_controller1->min_x, x);
          new_controller->min_x = x; // Day 13: just set to end_x
          new_controller->max_x = x; // Day 14+: track actual min/max

          break;
        }
        // ─────────────────────────────────────────────────────
        // LEFT STICK Y (Axis 1)
        // ─────────────────────────────────────────────────────
        case 1: {
          real32 y = (real32)event.value / 32767.0f;

          // NOTE: Joystick Y is often inverted!
          // Up = negative, Down = positive
          // Game might want to flip this:
          // y = -y;  // Uncomment if your game wants up = positive

          new_controller->end_y = y;
          new_controller->min_y = y;
          new_controller->max_y = y;

          break;
        }

          // // ───────────────────────────────────────────────────────
          // // Right Stick (axes 2, 5) ← NOTE: Y is axis 5, not 3!
          // // ───────────────────────────────────────────────────────
          // // TODO(Day 14+): Add right stick support if needed
          // case 3: // L2 trigger X
          //   printf("Right Stick X\n");
          //   // Optional: Store trigger pressure if you need it
          //   new_controller1->right_stick_x = event.value;
          //   break;
          // case 4: // R2 trigger Y
          //   printf("Right Stick Y\n");
          //   new_controller1->right_stick_y = event.value;
          //   break;

          // // ───────────────────────────────────────────────────────
          // // Triggers (analog, 0 to +32767)
          // // ───────────────────────────────────────────────────────
          // case 2:
          //   new_controller1->left_trigger = event.value;
          //   printf("L2 trigger\n");
          //   break;
          // case 5: // ← Right stick Y is axis 5!
          //   new_controller1->right_trigger = event.value;
          //   printf("R2 trigger\n");
          //   break;

          // ─────────────────────────────────────────────────────
          // RIGHT STICK (Axes 2-5 depending on controller)
          // ─────────────────────────────────────────────────────
          // TODO(Day 14+): Add right stick support if needed

          // ═══════════════════════════════════════════════════════════
          // 🎮 D-PAD (Axes 6-7 on PlayStation controllers)
          // ═══════════════════════════════════════════════════════════
          // D-pad is DIGITAL (4 directions) but reported as ANALOG axis!
          // We must set BOTH button states AND analog values.
          //
          // Range: -32767 (left/up) to +32767 (right/down)
          // Threshold: Use ±16384 (half of max) for digital detection
          // ═══════════════════════════════════════════════════════════

        case 6: { // D-pad X (left/right)

          // ✅ FIX: Set analog values for game layer!
          new_controller->start_x = old_controller->end_x;
          new_controller->start_y = old_controller->end_y;

          if (event.value < -16384) {
            // D-pad LEFT pressed
            process_game_button_state(true, &new_controller->left);
            process_game_button_state(false, &new_controller->right);

            // ✅ Set analog value for left (-1.0)
            new_controller->end_x = -1.0f;

          } else if (event.value > 16384) {
            // D-pad RIGHT pressed
            process_game_button_state(true, &new_controller->right);
            process_game_button_state(false, &new_controller->left);

            // ✅ Set analog value for right (+1.0)
            new_controller->end_x = +1.0f;

          } else {
            // D-pad X RELEASED (centered)
            process_game_button_state(false, &new_controller->left);
            process_game_button_state(false, &new_controller->right);

            // ✅ Set analog value to zero
            new_controller->end_x = 0.0f;
          }

          // Update min/max for Day 13 (just mirror end value)
          new_controller->min_x = new_controller->max_x = new_controller->end_x;

          break;
        }

        case 7: { // D-pad Y (up/down)

          // ✅ FIX: Set analog values for game layer!
          new_controller->start_x = old_controller->end_x;
          new_controller->start_y = old_controller->end_y;

          if (event.value < -16384) {
            // D-pad UP pressed
            process_game_button_state(true, &new_controller->up);
            process_game_button_state(false, &new_controller->down);

            // ✅ Set analog value for up (+1.0)
            // NOTE: You might need to invert this depending on your coordinate
            // system
            new_controller->end_y = -1.0f;

          } else if (event.value > 16384) {
            // D-pad DOWN pressed
            process_game_button_state(true, &new_controller->down);
            process_game_button_state(false, &new_controller->up);

            // ✅ Set analog value for down (-1.0)
            new_controller->end_y = 1.0f;

          } else {
            // D-pad Y RELEASED (centered)
            process_game_button_state(false, &new_controller->up);
            process_game_button_state(false, &new_controller->down);

            // ✅ Set analog value to zero
            new_controller->end_y = 0.0f;
          }

          // Update min/max for Day 13 (just mirror end value)
          new_controller->min_y = new_controller->max_y = new_controller->end_y;

          break;
        }

        default:
          //   // printf("D-pad number: %d, value: %d\n", event.number,
          //   event.value);
          break;
        }
      }
    }
  }
}

void debug_joystick_state(GameInput *old_game_input) {

  printf("\n🎮 Controller States:\n");
  for (int i = 0; i < MAX_CONTROLLER_COUNT; i++) {
    GameControllerInput *c = &old_game_input->controllers[i];
    LinuxJoystickState *joystick_state = NULL;
    if (i > 0 && i - 1 < MAX_JOYSTICK_COUNT) {
      joystick_state = &g_joysticks[i - 1];
    }
    printf("  [%d] connected=%d analog=%d fd=%d end_x=%.2f end_y=%.2f\n", i,
           c->is_connected, c->is_analog,
           joystick_state ? joystick_state->fd : -1, c->end_x, c->end_y);
  }
}