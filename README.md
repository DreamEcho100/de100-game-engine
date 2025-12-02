# 📝 Handmade Hero: Quick Learning Notes

---

## Resources

### Main Links

- [Episode Guide](https://guide.handmadehero.org/)

### Podcasts

- [CORECURSIVE #062 - Video Game Programming From Scratch - With Casey Muratori](https://corecursive.com/062-game-programming/)

### Blogs

- [Learn Game Engine Programming](https://engine-programming.net/)
- [How 99% of C Tutorials Get it Wrong](https://sbaziotis.com/uncat/how-c-tutorials-get-it-wrong.html)
- [Cache-Friendly Code](https://www.baeldung.com/cs/cache-friendly-code)

## YouTube Playlists

- [Beginner C Videos By - Jacob Sorber _(@JacobSorber)_](https://www.youtube.com/playlist?list=PL9IEJIKnBJjG5H0ylFAzpzs9gSmW_eICB)

## Days summary

### **Day 1-2**

#### 🗓️ **Day 1-2 - Git Commit Summary**

##### **Commit 1a318ec (Nov 19) - "init"**

- Basic C setup, first compilation

##### **Commit bb97195 (Nov 20) - "init"**

- Installed Raylib from source
- First graphical window
- Learned linker flags (`-lraylib -lX11 -lGL`)

##### **Commit 43be272 (Nov 21, 19:38) - "Implement platform layer"**

**📁 Project Structure:**

```
src/
├── game/          # Platform-independent logic
│   ├── game.h
│   └── game.c
└── platform/      # OS-specific code
    ├── x11_backend.c
    └── raylib_backend.c
```

**✅ Learned:**

- Layer separation (game vs platform)
- Pixel buffer rendering
- Simple gradient animation
- 60 FPS frame timing

##### **Commit 96147fc (Nov 21, 20:08) - "Add platform backends"**

**✅ Learned:**

- 3 rendering strategies: Basic X11 → MIT-SHM → GLX
- Pixel formats: BGRA (X11) vs RGBA (Raylib)
- Performance tiers: Software → Optimized → Hardware

##### **Commit 14933ba (Nov 21, 21:08) - "Refactor build scripts"**

**✅ Learned:**

- Focus over complexity (removed GLX/SHM backends)
- One good X11 implementation > 4 mediocre ones

##### **Commit 7d2ff1e (Nov 23, 21:31) - "Update requirements and roadmap"**

**✅ Learned:**

- Planning multi-year project
- Xft font support

##### **Commits f647196 → 157f661 (Nov 23-26) - "Platform message box"**

**✅ Learned Advanced X11:**

- Modal dialog windows
- Anti-aliased text (Xft)
- Rounded rectangle buttons + hover effects
- Double buffering (Pixmap)
- Word wrapping
- Window type atoms (`_NET_WM_WINDOW_TYPE_DIALOG`)
- Event-driven UI (MotionNotify, ButtonPress)
- Wave 2 resource cleanup (10+ cleanup calls)

---

#### 🎯 **Day 1-2 - Core Concepts (Short List)**

| Concept            | What You Built                                      |
| ------------------ | --------------------------------------------------- |
| **Platform Layer** | Dual backends (X11 + Raylib), same game code        |
| **Rendering**      | Pixel buffer → gradient animation                   |
| **X11 Basics**     | Window, GC, XImage, event loop                      |
| **X11 Advanced**   | MIT-SHM, Xft fonts, Pixmap, modal dialogs           |
| **Frame Timing**   | 60 FPS with `nanosleep()`                           |
| **Memory**         | `mmap()`, shared memory, Wave 2 cleanup             |
| **UI**             | Custom message box (250+ lines, 800+ comment lines) |

---

#### ✅ **Day 1-2 - Skills Acquired**

- ✅ C compilation + linking
- ✅ Build scripts (`build.sh`, `run.sh`)
- ✅ Platform abstraction architecture
- ✅ X11 fundamentals + advanced features
- ✅ Pixel buffer rendering (BGRA/RGBA)
- ✅ Frame-based animation loop
- ✅ Resource lifetime management (Wave 1/2)
- ✅ Event-driven UI programming

---

### **Day 3-5**

#### 🗓️ **Day 3-5 Git - Commit Summary**

##### **Commits e7e6991 → 7915890 (Nov 28 - Dec 1)**

---

#### 🎯 **Day 3-5 - Core Concepts (Short List)**

| Concept                    | What You Built                                                                     |
| -------------------------- | ---------------------------------------------------------------------------------- |
| **Back Buffer**            | Double buffering with XImage/mmap (Wave 2 resource)                                |
| **Pixel Math**             | `offset = y * width + x` (2D→1D mapping)                                           |
| **Memory Management**      | `calloc()` (8× faster than `malloc()+memset()`) → `mmap()` (Casey's Day 4 pattern) |
| **Resource Waves**         | Wave 1 (process) vs Wave 2 (state) - when to free/not free                         |
| **Gradient Animation**     | CPU rendering with pitch/row iteration                                             |
| **OffscreenBuffer Struct** | Grouped data (Day 5 pattern) vs scattered globals                                  |
| **Fixed Buffer**           | 1280×720 fixed, no resize (Day 5 philosophy)                                       |
| **GC Reuse**               | Create once, reuse (Casey's CS_OWNDC pattern)                                      |
| **Render Refactor**        | `render_weird_gradient()` takes buffer parameter                                   |

---

#### 📊 **Day 3-5 Git - Key Code Evolution**

##### **e7e6991 (Nov 28) - Wave 2 Resources**

**✅ Learned:**

- Globals → `OffscreenBuffer` struct
- `resize_back_buffer()` (Wave 2: free old, allocate new)
- `update_window()` with XPutImage blitting
- Casey's "Resource Lifetimes in Waves" philosophy
- `calloc()` vs `malloc()+memset()` (8× performance difference!)

**Key Files:**

- Added llm.txt (13 Casey philosophies)
- `x11_backend.c`: Back buffer architecture

---

##### **59ddf6b → 117f955 (Nov 28-29) - Gradient & Pixel Drawing**

**✅ Learned:**

- Switched `calloc()` → `mmap()` (Casey's Day 4 pattern)
- `RenderWeirdGradient()` function (pitch-based row iteration)
- Pixel addressing bugs (segfault at i=1000, NULL pointer)
- Bounds checking (`offset < total_pixels`)
- Dual-axis animation (`x_offset++`, `y_offset += 2`)

**Bugs Fixed:**

- ❌ `offset = i * width + i` (treats i as both x and y!)
- ✅ `offset = y * width + x` (separate x, y)
- ❌ Drawing before buffer allocated
- ✅ Pre-allocate in `platform_main()`

---

##### **a6e564e → 7915890 (Nov 29 - Dec 1) - Day 5 Refactor**

**✅ Learned:**

- Scattered globals → `OffscreenBuffer` struct (Day 5 pattern)
- `pitch = width * bytes_per_pixel` stored in struct
- Lowercase function names (`RenderWeirdGradient` → `render_weird_gradient`)
- Raylib: Added `resize_back_buffer()`, `update_window_from_backbuffer()`
- Fixed buffer philosophy (1280×720, never resize)
- GC reuse (create once in `platform_main()`, reuse every frame)
- Removed auto-animation (prep for input-driven)

**Before (Day 3-4):**

```c
global_var XImage *g_BackBuffer;
global_var void *g_PixelData;
global_var int g_BufferWidth, g_BufferHeight;
```

**After (Day 5):**

```c
typedef struct {
    XImage *info;
    void *memory;
    int width, height, pitch, bytes_per_pixel;
} OffscreenBuffer;
global_var OffscreenBuffer g_backbuffer;
```

**Removed ConfigureNotify resize:**

```c
// ❌ Day 4: Resize on every window change
case ConfigureNotify:
    resize_back_buffer(...);

// ✅ Day 5: Fixed buffer, commented out
// resize_back_buffer(...);
```

**GC Reuse (Fixed Segfault):**

```c
// ❌ Day 4: Create/free GC every frame
void update_window() {
    GC gc = XCreateGC();
    XPutImage(..., gc, ...);
    XFreeGC(gc);  // 💥 Freed, then reused next frame!
}

// ✅ Day 5: Create once, reuse forever
// In platform_main():
GC gc = XCreateGC(display, window, 0, NULL);
while (g_is_running) {
    update_window(&g_backbuffer, display, window, gc, ...);
}
// OS cleans up GC on exit (Wave 1)
```

---

#### 🐛 **Day 3-5 Git - Common Bugs Fixed**

| Bug                    | Cause                                               | Fix                                     |
| ---------------------- | --------------------------------------------------- | --------------------------------------- |
| **Segfault at i=1000** | `offset = i * width + i` (wrong formula)            | `offset = y * width + x`                |
| **NULL pointer**       | Drawing before buffer allocated                     | Pre-allocate in `platform_main()`       |
| **Use-after-free**     | `XFreeGC()` in `update_window()`, reused next frame | Create GC once, never free (Wave 1)     |
| **3× allocations**     | ConfigureNotify fires for same size                 | Comment out resize (Day 5 fixed buffer) |

---

#### 📚 **Day 3-5 Git - Casey's Philosophies Applied**

1. **Resource Waves:**

   - Wave 1 (process): `display`, `window`, `gc` → Never free
   - Wave 2 (state): `g_backbuffer` → Free on resize

2. **Simplicity Over Abstraction:**

   - No `WindowDimension` helper yet (not needed)
   - Direct struct access (`buffer->width`)

3. **Performance as Feature:**

   - `calloc()` (8× faster) → `mmap()` (Casey's pattern)
   - GC reuse (no malloc/free per frame)

4. **Debug Ability:**
   - Bounds checking: `if (offset < total_pixels)`
   - Defensive NULL check: `if (!buffer->info) return;`

---

#### ✅ **Day 3-5 Git - Skills Acquired**

| Skill                     | Day 3                 | Day 4              | Day 5              |
| ------------------------- | --------------------- | ------------------ | ------------------ |
| **Double buffering**      | ✅ XImage + XPutImage | -                  | -                  |
| **Pixel math**            | ✅ `offset = y*w+x`   | -                  | -                  |
| **calloc() optimization** | ✅ 8× faster          | -                  | -                  |
| **mmap() allocation**     | -                     | ✅ Casey's pattern | -                  |
| **Gradient rendering**    | -                     | ✅ Pitch-based     | -                  |
| **Struct refactor**       | -                     | -                  | ✅ OffscreenBuffer |
| **Fixed buffer**          | -                     | -                  | ✅ No resize       |
| **GC reuse**              | -                     | -                  | ✅ Wave 1 resource |
| **Lowercase naming**      | -                     | -                  | ✅ Consistency     |
