# Lesson 01 — Window + GLX/OpenGL Context

## By the end of this lesson you will have:
A black 800×600 window on your screen with the title "Asteroids". Nothing moves yet — this lesson is purely about making the OS give you a window backed by an OpenGL rendering context. When you close the window (or press Escape) the program exits cleanly.

---

## Step 1 — Connect to the X11 display server

X11 is Linux's window system. Think of it like a browser's `window` object — before you can do anything visual, you need a handle to the environment.

```c
g_display = XOpenDisplay(NULL);
if (!g_display) { fprintf(stderr, "FATAL: XOpenDisplay\n"); return; }
```

`NULL` means "use the `DISPLAY` environment variable" (usually `:0` on a local machine). `g_display` is a global `Display *` — it's the connection handle for every subsequent X11 call.

**JS analogy:** `XOpenDisplay` is like `document.getElementById('canvas').getContext('2d')` — you ask the environment for a context and everything else hangs off it.

**What you see:** nothing visible yet, just a connection established internally.

---

## Step 2 — Choose a GLX framebuffer configuration

OpenGL on Linux goes through GLX. Before creating a window, you have to ask GLX "do you have a framebuffer that supports double-buffering, RGBA, and 8 bits per channel?" That answer comes back as a `GLXFBConfig`.

```c
int    screen = DefaultScreen(g_display);
Window root   = RootWindow(g_display, screen);

static const int fb_attribs[] = {
    GLX_RENDER_TYPE,   GLX_RGBA_BIT,
    GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    GLX_DOUBLEBUFFER,  True,
    GLX_RED_SIZE,   8, GLX_GREEN_SIZE, 8,
    GLX_BLUE_SIZE,  8, GLX_ALPHA_SIZE, 8,
    None
};
int n_configs = 0;
GLXFBConfig *fbc = glXChooseFBConfig(g_display, screen, fb_attribs, &n_configs);
if (!fbc || n_configs == 0) { fprintf(stderr, "FATAL: glXChooseFBConfig\n"); return; }

XVisualInfo *vi = glXGetVisualFromFBConfig(g_display, fbc[0]);
```

**Why double-buffering?** The GPU draws into a back buffer while displaying the front buffer. `glXSwapBuffers` flips them atomically — this is how you avoid tearing. Without it you'd see half-drawn frames.

**JS analogy:** `GLXFBConfig` is like requesting `{ antialias: true, alpha: true }` when calling `canvas.getContext('webgl', options)`. You're negotiating capabilities with the driver before anything is displayed.

**What you see:** still nothing — this is a negotiation step with the driver.

---

## Step 3 — Create the X11 window with the GLX visual

Normally you'd use `XCreateSimpleWindow`, but that only works with the default visual. Because we need a specific OpenGL-compatible visual (from `vi` above), we must use the lower-level `XCreateWindow`.

```c
XSetWindowAttributes swa;
swa.colormap   = XCreateColormap(g_display, root, vi->visual, AllocNone);
swa.event_mask = KeyPressMask | KeyReleaseMask | StructureNotifyMask;

g_window = XCreateWindow(g_display, root,
    0, 0, (unsigned)width, (unsigned)height, 0,
    vi->depth, InputOutput, vi->visual,
    CWColormap | CWEventMask, &swa);
```

`CWColormap | CWEventMask` tells X11 which fields of `swa` to actually use.  
`KeyPressMask | KeyReleaseMask` registers interest in keyboard events. Without these, no key events will be delivered to your window.

**Why not `XCreateSimpleWindow`?** That function inherits the parent's visual, which is the default 24-bit visual. Our GLX config may require a different depth or visual class. Using a mismatched visual causes a BadMatch error at GL context creation time.

**What you see:** still nothing — the window exists in X11's internal state but hasn't been shown yet.

---

## Step 4 — Register the "close window" protocol

When the user clicks the ✕ button, X11 doesn't kill your process — it sends a `ClientMessage` event. You must register for this explicitly.

```c
g_wm_delete = XInternAtom(g_display, "WM_DELETE_WINDOW", False);
XSetWMProtocols(g_display, g_window, &g_wm_delete, 1);
XStoreName(g_display, g_window, title);
```

`XInternAtom` retrieves a numeric ID for the string `"WM_DELETE_WINDOW"`. Later in `platform_get_input` we check if an incoming `ClientMessage` carries this atom, and if so, set `g_is_running = 0`.

**JS analogy:** like adding a `beforeunload` event listener so you can do cleanup before the tab closes.

---

## Step 5 — Create the OpenGL context and make it current

```c
g_glctx = glXCreateContext(g_display, vi, NULL, GL_TRUE);
XFree(vi);
XFree(fbc);

XMapWindow(g_display, g_window);
glXMakeCurrent(g_display, g_window, g_glctx);
```

`glXCreateContext` creates the GL state machine (matrices, textures, etc.).  
`XMapWindow` makes the window visible on screen.  
`glXMakeCurrent` binds the GL context to the window — from this point on, all `gl*` calls operate on this context.

**Why `XFree` on `vi` and `fbc`?** These were heap-allocated by X11 on our behalf. We `XFree` them (not `free`) because they live in memory managed by the X11 library.

**What you see:** a black window appears on screen.

---

## Step 6 — Set up VSync

```c
static void setup_vsync(void) {
    const char *exts = glXQueryExtensionsString(g_display,
                                                DefaultScreen(g_display));
    if (exts && strstr(exts, "GLX_EXT_swap_control")) {
        typedef void (*PFNGLXSWAPINTERVALEXT)(Display *, GLXDrawable, int);
        PFNGLXSWAPINTERVALEXT fn =
            (PFNGLXSWAPINTERVALEXT)(void *)glXGetProcAddressARB(
                (const GLubyte *)"glXSwapIntervalEXT");
        if (fn) {
            fn(g_display, glXGetCurrentDrawable(), 1);
            fprintf(stderr, "✓ VSync enabled (GLX_EXT_swap_control)\n");
            return;
        }
    }
    fprintf(stderr, "! VSync not available\n");
}
```

`GLX_EXT_swap_control` is an extension — not always present. We probe for it, then load the function pointer at runtime with `glXGetProcAddressARB` (like `dlsym` for GL). Calling `fn(..., 1)` sets the swap interval to 1, meaning `glXSwapBuffers` will block until the monitor's next vertical sync pulse.

**Why VSync?** It caps the frame rate to the monitor refresh rate (60 Hz) and eliminates tearing. Crucially for this course, it also drives our `dt` calculation — each frame is ~16.67 ms, giving smooth physics.

**JS analogy:** `requestAnimationFrame` implicitly does this; `glXSwapBuffers` with interval 1 is the explicit C equivalent.

---

## Step 7 — Set up the orthographic projection and GL texture

```c
glGenTextures(1, &g_texture);
glBindTexture(GL_TEXTURE_2D, g_texture);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

glMatrixMode(GL_PROJECTION);
glLoadIdentity();
glOrtho(0, 1, 1, 0, -1, 1);
glMatrixMode(GL_MODELVIEW);
glLoadIdentity();
glEnable(GL_TEXTURE_2D);
```

`glOrtho(0, 1, 1, 0, -1, 1)` maps the NDC unit square to texture coordinates so that (0,0) is the top-left and (1,1) is the bottom-right. This lets us draw a fullscreen quad with texture coords `(0,0)` to `(1,1)` and have it fill the window perfectly.

`GL_NEAREST` filter means "no blending between pixels" — when we zoom in, pixels stay pixel-sharp rather than blurring. This is what we want for a pixel-art-style game.

**Why a texture?** The game renders into a CPU-side pixel array (`bb.pixels`). Each frame we upload that array to the GPU as a texture and draw a fullscreen quad. This is the simplest correct approach — one `glTexImage2D` call per frame rather than using raw framebuffer objects.

---

## Build & Run

```
clang src/asteroids.c src/main_x11.c -o build/asteroids_x11 -lX11 -lxkbcommon -lGL -lGLX -lm
./build/asteroids_x11
```

**What you should see:** An 800×600 black window titled "Asteroids" appears. Your terminal shows:
```
✓ VSync enabled (GLX_EXT_swap_control)
✓ Auto-repeat detection: enabled
✓ OpenGL initialized: 4.6.0 ...
```
Pressing Escape or clicking ✕ exits cleanly.

---

## Key Concepts

- `XOpenDisplay` — connects your process to the X11 display server; all subsequent X11 calls need this handle.
- `glXChooseFBConfig` — negotiates a double-buffered RGBA framebuffer with the GPU driver.
- `XCreateWindow` (not `XCreateSimpleWindow`) — required when you need a custom GLX visual.
- `glXCreateContext` — creates the OpenGL state machine tied to your window.
- `XMapWindow` — makes the window visible; call it after the GL context is ready.
- `glXMakeCurrent` — binds the GL context so `gl*` calls work.
- `glOrtho(0,1,1,0,-1,1)` — orthographic projection with (0,0)=top-left, (1,1)=bottom-right.
- `GL_NEAREST` — pixel-exact texture filtering; no interpolation blur.
- `setup_vsync` — loads `glXSwapIntervalEXT` at runtime to cap frames to 60 Hz.

## Exercise

Change the window dimensions from `800×600` to `640×480` by modifying `SCREEN_W` and `SCREEN_H` in `asteroids.h`. Rebuild and confirm the window resizes. Then change them back — the rest of the course assumes 800×600.
