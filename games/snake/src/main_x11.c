#define _POSIX_C_SOURCE 200809L

#include "game.h"
#include "platform.h"

#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/X.h>
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

/* GLX extension for VSync */
typedef void (*PFNGLXSWAPINTERVALEXTPROC)(Display *, GLXDrawable, int);
// typedef int (*PFNGLXSWAPINTERVALMESAPROC)(int);

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform State
 * ═══════════════════════════════════════════════════════════════════════════
 */

typedef struct {
  Display *display;
  Window window;
  GLXContext gl_context;
  GLuint texture_id;
  int screen;
  int width;
  int height;
  bool vsync_enabled;
} X11State;

static X11State g_x11 = {0};
static double g_start_time = 0.0;
static double g_last_frame_time = 0.0;
static const double g_target_frame_time = 1.0 / 60.0;

/* ═══════════════════════════════════════════════════════════════════════════
 * Timing
 * ═══════════════════════════════════════════════════════════════════════════
 */

static double get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  double now = (double)tv.tv_sec + (double)tv.tv_usec / 1000000.0;
  if (g_start_time == 0.0)
    g_start_time = now;
  return now - g_start_time;
}

static void sleep_ms(int ms) {
  struct timespec ts;
  ts.tv_sec = ms / 1000;
  ts.tv_nsec = (ms % 1000) * 1000000L;
  nanosleep(&ts, NULL);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * VSync Setup
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void setup_vsync(void) {
  const char *extensions =
      glXQueryExtensionsString(g_x11.display, g_x11.screen);

  if (extensions && strstr(extensions, "GLX_EXT_swap_control")) {
    PFNGLXSWAPINTERVALEXTPROC glXSwapIntervalEXT =
        (PFNGLXSWAPINTERVALEXTPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalEXT");
    if (glXSwapIntervalEXT) {
      glXSwapIntervalEXT(g_x11.display, g_x11.window, 1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_EXT_swap_control)\n");
      return;
    }
  }

  if (extensions && strstr(extensions, "GLX_MESA_swap_control")) {
    PFNGLXSWAPINTERVALMESAPROC glXSwapIntervalMESA =
        (PFNGLXSWAPINTERVALMESAPROC)glXGetProcAddressARB(
            (const GLubyte *)"glXSwapIntervalMESA");
    if (glXSwapIntervalMESA) {
      glXSwapIntervalMESA(1);
      g_x11.vsync_enabled = true;
      printf("✓ VSync enabled (GLX_MESA_swap_control)\n");
      return;
    }
  }

  g_x11.vsync_enabled = false;
  printf("⚠ VSync not available\n");
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Platform Init
 * ═══════════════════════════════════════════════════════════════════════════
 */

int platform_init(PlatformGameProps *platform_game_props) {
  g_x11.display = XOpenDisplay(NULL);
  if (!g_x11.display) {
    fprintf(stderr, "Failed to open display\n");
    return 1;
  }

  Bool supported;
  XkbSetDetectableAutoRepeat(g_x11.display, True, &supported);
  printf("✓ Auto-repeat detection: %s\n",
         supported ? "enabled" : "not supported");

  g_x11.screen = DefaultScreen(g_x11.display);
  g_x11.width = platform_game_props->width;
  g_x11.height = platform_game_props->height;

  int visual_attribs[] = {GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None};
  XVisualInfo *visual =
      glXChooseVisual(g_x11.display, g_x11.screen, visual_attribs);
  if (!visual) {
    fprintf(stderr, "Failed to choose GLX visual\n");
    return 1;
  }

  Colormap colormap =
      XCreateColormap(g_x11.display, RootWindow(g_x11.display, g_x11.screen),
                      visual->visual, AllocNone);

  XSetWindowAttributes attrs;
  attrs.colormap = colormap;
  attrs.event_mask =
      ExposureMask | KeyPressMask | KeyReleaseMask | StructureNotifyMask;

  g_x11.window = XCreateWindow(
      g_x11.display, RootWindow(g_x11.display, g_x11.screen), 100, 100,
      platform_game_props->width, platform_game_props->height, 0, visual->depth,
      InputOutput, visual->visual, CWColormap | CWEventMask, &attrs);

  g_x11.gl_context = glXCreateContext(g_x11.display, visual, NULL, GL_TRUE);
  if (!g_x11.gl_context) {
    fprintf(stderr, "Failed to create GLX context\n");
    XFree(visual);
    return 1;
  }

  XFree(visual);
  XStoreName(g_x11.display, g_x11.window, platform_game_props->title);
  XMapWindow(g_x11.display, g_x11.window);
  XFlush(g_x11.display);

  glXMakeCurrent(g_x11.display, g_x11.window, g_x11.gl_context);

  /* Setup OpenGL for 2D */
  glViewport(0, 0, g_x11.width, g_x11.height);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(0, platform_game_props->width, platform_game_props->height, 0, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_TEXTURE_2D);

  /* Create texture for backbuffer */
  glGenTextures(1, &g_x11.texture_id);
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

  setup_vsync();

  printf("✓ OpenGL initialized: %s\n", glGetString(GL_VERSION));
  return 0;
}

void platform_shutdown(void) {
  if (g_x11.texture_id)
    glDeleteTextures(1, &g_x11.texture_id);
  if (g_x11.gl_context) {
    glXMakeCurrent(g_x11.display, None, NULL);
    glXDestroyContext(g_x11.display, g_x11.gl_context);
  }
  if (g_x11.window)
    XDestroyWindow(g_x11.display, g_x11.window);
  if (g_x11.display)
    XCloseDisplay(g_x11.display);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Input Handling
 * ═══════════════════════════════════════════════════════════════════════════
 */

void platform_get_input(GameInput *input,
                        PlatformGameProps *platform_game_props) {
  (void)input;
  XEvent event;

  while (XPending(g_x11.display) > 0) {
    XNextEvent(g_x11.display, &event);

    switch (event.type) {
    case KeyPress: {
      KeySym key = XLookupKeysym(&event.xkey, 0);
      switch (key) {
      case XK_q:
      case XK_Q:
      case XK_Escape:
        platform_game_props->is_running = false;
        input->quit = true;
        break;
      case XK_r:
      case XK_R:
      case XK_space:
        input->restart = 1;
        break;
      case XK_Left:
      case XK_a:
      case XK_A:
        UPDATE_BUTTON(input->turn_left, 1);
        break;
      case XK_Right:
      case XK_d:
      case XK_D:
        UPDATE_BUTTON(input->turn_right, 1);
        break;
      }
      break;
    }
    case KeyRelease: {
      KeySym key = XLookupKeysym(&event.xkey, 0);
      switch (key) {
      case XK_Left:
      case XK_a:
      case XK_A:
        UPDATE_BUTTON(input->turn_left, 0);
        break;
      case XK_Right:
      case XK_d:
      case XK_D:
        UPDATE_BUTTON(input->turn_right, 0);
        break;
      }
      break;
    }

    case ClientMessage: {
      printf("Received ClientMessage event\n");
      Atom wm_delete_window =
          XInternAtom(g_x11.display, "WM_DELETE_WINDOW", False);
      if ((Atom)event.xclient.data.l[0] == wm_delete_window) {
        platform_game_props->is_running = false;
        // input->quit = 1;
        break;
      }
      break;
    }

    case ConfigureNotify:
      if (event.xconfigure.width != g_x11.width ||
          event.xconfigure.height != g_x11.height) {
        g_x11.width = event.xconfigure.width;
        g_x11.height = event.xconfigure.height;
        glViewport(0, 0, g_x11.width, g_x11.height);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glOrtho(0, g_x11.width, g_x11.height, 0, -1, 1);
        glMatrixMode(GL_MODELVIEW);
      }
      break;
    }
  }
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Display Backbuffer via OpenGL Texture
 * ═══════════════════════════════════════════════════════════════════════════
 */

static void platform_display_backbuffer(Backbuffer *bb) {
  glClear(GL_COLOR_BUFFER_BIT);

  /* Upload backbuffer to texture */
  glBindTexture(GL_TEXTURE_2D, g_x11.texture_id);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, bb->width, bb->height, 0, GL_RGBA,
               GL_UNSIGNED_BYTE, bb->pixels);

  /* Draw textured quad */
  glBegin(GL_QUADS);
  glTexCoord2f(0.0f, 0.0f);
  glVertex2f(0, 0);
  glTexCoord2f(1.0f, 0.0f);
  glVertex2f(bb->width, 0);
  glTexCoord2f(1.0f, 1.0f);
  glVertex2f(bb->width, bb->height);
  glTexCoord2f(0.0f, 1.0f);
  glVertex2f(0, bb->height);
  glEnd();

  glXSwapBuffers(g_x11.display, g_x11.window);
}

/* ═══════════════════════════════════════════════════════════════════════════
 * Main
 * ═══════════════════════════════════════════════════════════════════════════
 */

int main(void) {
  PlatformGameProps platform_game_props = {0};
  if (platform_game_props_init(&platform_game_props) != 0) {
    return 1;
  }

  if (platform_init(&platform_game_props) != 0) {
    return 1;
  }

  GameInput game_input = (GameInput){};
  GameState game_state = (GameState){};
  game_init(&game_state);

  g_last_frame_time = get_time();

  while (platform_game_props.is_running) {
    double current_time = get_time();
    float delta_time = (float)(current_time - g_last_frame_time);
    g_last_frame_time = current_time;

    prepare_input_frame(&game_input);
    platform_get_input(&game_input, &platform_game_props);

    // if (game_input.quit)
    //   break;

    // if (game_state.game_over && game_input.restart) {
    //   game_init(&game_state, &game_input);
    // } else {
    game_update(&game_state, &game_input, delta_time);
    // }

    // /* Game renders to backbuffer - platform independent! */
    // game_render(&backbuffer, &game_state);

    game_render(&game_state, &platform_game_props.backbuffer);

    /* Platform displays the backbuffer */
    platform_display_backbuffer(&platform_game_props.backbuffer);

    /* Frame limiting (only if no VSync) */
    if (!g_x11.vsync_enabled) {
      double frame_time = get_time() - current_time;
      if (frame_time < g_target_frame_time) {
        sleep_ms((int)((g_target_frame_time - frame_time) * 1000.0));
      }
    }
  }

  platform_game_props_free(&platform_game_props);
  platform_shutdown();
  return 0;
}
