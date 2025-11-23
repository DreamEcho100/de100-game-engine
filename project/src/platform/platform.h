#ifndef PLATFORM_H
#define PLATFORM_H

#include <stdint.h>

typedef struct {
  int width;
  int height;
  uint32_t *pixels;
} PlatformFrame;

int platform_main();


// Button configuration (like HTML button elements)
typedef struct {
  const char *label;    // Button text
  int value;            // Return value when clicked
  int x, y;             // Position
  int width, height;    // Size
  unsigned long color;  // Background color
  unsigned long textColor;
  int isHovered;        // For hover effects
} MessageBoxButton;
typedef enum {
  MSG_BX_INFO,
  MSG_BX_WARNING,
  MSG_BX_ERROR,
  MSG_BX_QUESTION
} MessageBoxType;
// Button types for common scenarios
typedef enum {
  MSG_BX_OK = 1,
  MSG_BX_CANCEL = 0,
  MSG_BX_YES = 1,
  MSG_BX_NO = 0,
  MSG_BX_RETRY = 2,
  MSG_BX_IGNORE = 3
} MessageBoxButtonType;
typedef struct {
  MessageBoxType type;
  const char *icon;
  int x;
  int y;
  int width;
  int height;
  int borderSize;
  unsigned long borderColor;
  unsigned long bgColor;
  
  // New fields for prettier dialogs
  const char **buttons;        // Array of button labels (e.g., {"Yes", "No", NULL})
  int *buttonValues;           // Array of return values (e.g., {1, 0})
  int buttonCount;             // Number of buttons
  unsigned long titleBgColor;  // Title bar background
  unsigned long titleTextColor; // Title text color
  int fontSize;                // Text size
} ShowMessageBoxOptions;

/**
* Cross-platform message box
 *
 * Works on Windows, Linux, macOS - like using a polyfill in JS
 *
 * @param title Window title
 * @param message Message text
 * @param type "info", "warning", "error", "question"
 * @param icon "info", "warning", "error", "question"
 *
 * Returns 1 for OK, 0 for Cancel (on question dialogs)
 */
int platform_show_message_box(const char *title, const char *message, ShowMessageBoxOptions options);


#endif
