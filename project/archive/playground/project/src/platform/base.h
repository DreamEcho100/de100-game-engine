#include "./platform.h"

int show_message_box(
	const char *title,
	const char *message,
	ShowMessageBoxOptions options,
	// The function reference that handle
	int (*platform_show_message_box)(const char *, const char *, ShowMessageBoxOptions)
);
