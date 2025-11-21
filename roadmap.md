# 🧭 **UPDATED HANDMADE HERO ROADMAP (Linux + PURE X11 BACKEND)**

**Your optimized, realistic, Linux-native Handmade Hero journey**
**~2–2.5 years @ 10 hrs/week**, fully aligned with Casey’s episodes.

---

# ✔️ QUICK SUMMARY OF CHANGES

You now have:

- **One X11 backend** → using X11, XShm, POSIX, pthreads, dlopen
- **One Raylib backend** → optional sanity-check backend
- **Primary dev path = X11 backend**
- **Matching Windows → Linux episode mapping**
- **New “Linux Platform Fundamentals Phase”**
- **New “Backend Maturity Levels”**
- **New validation checklist**
- **Realistic weekly schedule**

---

# 🌑 **PHASE 0 — Project Setup (DONE)**

You already have:

- C compiler (clang or gcc)
- `build.sh` / `run.sh`
- Project structure
- X11 backend foundation
- Raylib backend option
- `platform.h` / `game.h` clear separation

---

# 🌒 **PHASE 1 — Linux Platform Fundamentals (2–4 weeks)**

This prepares you for translating Win32 → Linux correctly.

## 🎯 Goals

Understand the Linux equivalents of Casey’s Windows assumptions.

### ✔️ Learn Linux memory model

- `mmap()` vs `VirtualAlloc`
- Demand paging
- Overcommit
- Anonymous mappings
- File-backed mappings

### ✔️ Learn Linux file descriptor model

Everything is a file descriptor:

- X11 connection
- Audio device
- Gamepad
- Files
- Sockets

### ✔️ Learn pthreads basics

- `pthread_create`
- `pthread_join`
- `pthread_mutex_*`
- Scheduling differences from Win32

### ✔️ Learn X11 fundamental concepts

- Message queue (`XNextEvent`)
- Xlib is _not thread-safe_ without `XInitThreads()`
- X11 is _network-transparent_ (unlike Win32)
- How MIT-SHM speedup works

### ✔️ Required reads

```bash
man mmap
man clock_gettime
man pthread_create
man XInitThreads
man XCreateWindow
man XShmPutImage
```

**Why this matters:**
Casey assumes a Windows-style mental model.
You must “translate” that into Unix thinking.

---

# 🌓 **PHASE 2 — Build the Pure X11 Platform Layer (2–4 months)**

### ✔️ You build **this structure**:

```
my_game/
│
├── build/
│
├── build.sh
├── run.sh
│
└── src/
    ├── main.c
    │
    ├── game/
    │   ├── game.h
    │   └── game.c
    │
    └── platform/
        ├── platform.h
        ├── platform_selector.h
        │
        ├── x11_backend.c
        └── raylib_backend.c
```

---

## 📌 **Episode Mapping (Windows → Linux)**

| Casey Episode | Windows API        | Your Linux API                           |
| ------------- | ------------------ | ---------------------------------------- |
| 1–3           | Win32 Window + GDI | **X11 Window + XShm + software blitter** |
| 4–6           | Win32 Input        | **X11 keyboard + mouse**                 |
| 7             | XInput gamepads    | **evdev or SDL2-only for controllers**   |
| 8–10          | QPC timing         | **clock_gettime / nanosleep**            |
| 11–18         | DirectSound        | **PulseAudio or ALSA**                   |
| 19–21         | Win32 file IO      | **POSIX open/read/write/mmap**           |
| 22–25         | Hot Reload         | **dlopen + dlsym + stat**                |

---

## 🔵 **Backend Maturity Model** _(New + Important)_

### 🥇 **PRIMARY BACKEND:**

### **`x11_backend.c` (pure Linux, authoritative)**

This should always be the version you treat as “real Handmade.”

Contains:

- X11 window
- XShmPutImage
- POSIX timing
- POSIX file IO
- pthreads
- Pulse/ALSA
- dlopen hot-reload

### 🥈 **SECONDARY BACKEND:**

### `raylib_backend.c`

Used for:

- sanity checks
- debugging
- verifying game layer correctness

But **NOT** for following Casey’s episodes.

---

## 🧩 **Phase 2 Milestones (Linux Replacements for Windows Episodes)**

---

## 🔹 **Milestone 1: Window + Backbuffer**

Implement:

- X11 display connection
- Create Window
- Create MIT-SHM shared image
- Software backbuffer in RAM
- Blit it with `XShmPutImage`
- Frame pacing at ~60Hz

Use Casey’s workflow, but map Win32 → X11.

---

## 🔹 **Milestone 2: Input**

Implement:

- X11 keyboard events (`XKeyEvent`)
- X11 mouse events
- Translate to your `game_input` struct

Gamepad options:

- **SDL2 gamepad subsystem** (recommended)
- OR direct **evdev** if you want pure Linux

---

## 🔹 **Milestone 3: Timing**

Implement:

```c
clock_gettime(CLOCK_MONOTONIC, &start);
nanosleep();
```

Replace Windows:

- `QueryPerformanceCounter` → `clock_gettime`
- `Sleep()` → `nanosleep()`

---

## 🔹 **Milestone 4: Audio Output**

Two paths:

### ⭐ Recommended: PulseAudio Simple API

Simple, stable, less boilerplate.

### Alternative: ALSA PCM

Harder, more control.

Implement:

- Audio buffer callback
- Low-latency ring buffer
- Write samples each frame

---

## 🔹 **Milestone 5: File IO**

Implement:

- `open() / read() / write()`
- `stat()`
- `mmap()` file loading
- Directory iteration (`opendir`, `readdir`)

---

## 🔹 **Milestone 6: Hot Reload**

Implement:

- `dlopen()` on game code
- `dlsym()` to load `game_update()`
- Compare timestamps using `stat()`
- Reload `.so` on change

This is the biggest Handmade Hero feature.

---

# 🌔 **PHASE 3 — Pure Game Development**

Everything in this phase is **platform-independent**.

You follow Casey **exactly** from episode ~26 to ~400.

Includes:

- World simulation
- Entities & collisions
- Hero movement
- Tile maps
- Rendering pipeline (software)
- Animation system
- Audio mixing
- Debug tools
- Memory arenas
- Asset system
- Intro to OpenGL (you can skip or adapt)

This is where 70% of Handmade Hero’s content lives.

---

# 🌕 **PHASE 4 — Advanced Engine Work**

Optional but powerful.

Includes:

- Multi-threaded work queue (`pthread`)
- SIMD optimizations with SSE/AVX
- Real-time debug panel
- File watching
- Asset building tools
- OpenGL Renderer (if you want)
- Profiling tools
- Memory diagnostics
- ECS-like data-oriented structures

---

# 🧪 **NEW: Dual-Backend Validation Suite**

(Ensures your game layer is 100% portable)

Each frame:

1. Update game with X11 backend
2. Update game with Raylib backend
3. Hash final backbuffer
4. Compare hashes

```c
uint64_t hash = 0;
for (int i = 0; i < width*height*4; i++)
    hash = (hash * 1315423911u) + pixels[i];
```

If both backends output the same buffer → your game logic is platform-independent.

Huge confidence booster.

---
