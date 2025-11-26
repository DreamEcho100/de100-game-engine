#include "base.h"

int show_message_box(const char *title, const char *message,
                     ShowMessageBoxOptions options,
                     int (*platform_show_message_box)(const char *,
                                                      const char *,
                                                      ShowMessageBoxOptions)) {

  // Set defaults based on message type (like default props in React)
  return platform_show_message_box(
      title, message,
      (ShowMessageBoxOptions){
          .type = options.type,
          .icon = options.icon                      ? options.icon
                  : options.type == MSG_BX_INFO     ? "info"
                  : options.type == MSG_BX_WARNING  ? "warning"
                  : options.type == MSG_BX_ERROR    ? "error"
                  : options.type == MSG_BX_QUESTION ? "question"
                                                    : "info",
          .width = options.width > 0 ? options.width : 400,
          .height = options.height > 0 ? options.height : 250,
          .x = options.x > 0 ? options.x : 100,
          .y = options.y > 0 ? options.y : 100,
          .borderColor =
              options.borderColor > 0 ? options.borderColor : 0xE0E0E0,
          .borderSize = options.borderSize > 0 ? options.borderSize : 2,
          .bgColor = options.bgColor > 0 ? options.bgColor : 0xF5F5F5,

          // New prettier defaults
          .titleBgColor =
              options.titleBgColor > 0 ? options.titleBgColor : 0x2196F3,
          .titleTextColor =
              options.titleTextColor > 0 ? options.titleTextColor : 0xFFFFFF,
          .buttons = options.buttons,
          .buttonValues = options.buttonValues,
          .buttonCount = options.buttonCount,
          .fontSize = options.fontSize > 0 ? options.fontSize : 14,
      });
}