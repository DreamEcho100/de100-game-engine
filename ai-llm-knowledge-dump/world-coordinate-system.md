> Stopped at "7. Step-by-Step: How Recanonicalizing Works"

# 🗺️ World Coordinate System: The Complete Mental Model

**Your Goal**: Understand coordinate systems so deeply you could rebuild them from scratch.

**Casey's Core Insight**:

> "A coordinate system is just a way to answer the question: WHERE IS THIS THING?"

---

## 📚 Table of Contents

1. [The Problem: Why Do We Need This?](#the-problem)
2. [Mental Model #1: The Naive Approach](#mental-model-1)
3. [Mental Model #2: Screen vs World Coordinates](#mental-model-2)
4. [Mental Model #3: Hierarchical Coordinates](#mental-model-3)
5. [The Old System (Before Day 30)](#old-system)
6. [The New System (Day 30+)](#new-system)
7. [Step-by-Step: How Recanonicalizing Works](#how-it-works)
8. [Complete Code Walkthrough](#code-walkthrough)
9. [Common Mistakes & Debugging](#common-mistakes)
10. [Exercises](#exercises)

---

<a name="the-problem"></a>

## 1. The Problem: Why Do We Need This?

### The Fundamental Question

You're making a game. You need to answer: **"Where is the player?"**

### Naive Web Dev Answer

```javascript
const player = {
  x: 150, // pixels from left edge of screen
  y: 200, // pixels from top edge of screen
};
```

**This works for:**

- Simple UI elements
- Small, single-screen games
- Things that don't move much

**This BREAKS for:**

- Large worlds (what if world is 100,000 pixels wide?)
- Scrolling (does x=150 mean screen coords or world coords?)
- Precision (floats lose precision with large numbers)
- Multiplayer (how do you sync coordinates?)

---

### Casey's Key Insight: Floating Point Precision

```
IEEE 754 float32 has ~7.2 decimal digits of precision

Small number:
  12.34567    ✅ All digits are meaningful

Large number:
  1234567.0   ✅ Only 7 significant digits
  1234567.8   ❌ The .8 might get lost in rounding!

HUGE number:
  12345678    ✅ No decimals, OK
  12345678.5  ❌ The .5 is GONE (rounded away)
```

**What This Means:**

```c
// Player at x=10000 pixels (far right in world)
float x = 10000.0f;
x += 0.1f;  // Try to move 0.1 pixels right

// What you GET might be:
// x = 10000.0f  (the 0.1 was lost!)
```

---

### 🚫 ANTI-PATTERN #1: "JavaScript Doesn't Have This Problem"

**Web Dev Thinking:**

```javascript
// JavaScript uses 64-bit floats, so more precision!
let x = 10000;
x += 0.1;
console.log(x); // 10000.1 ✅ Works fine!
```

**❌ WHY THIS FAILS IN C:**

1. **Performance:** `double` (64-bit) is slower than `float` (32-bit) on many platforms
2. **Memory:** Games have THOUSANDS of entities - 2x memory usage adds up!
3. **SIMD:** Modern CPUs can process 4 floats at once, but only 2 doubles
4. **Still breaks eventually:** Even doubles lose precision at 100,000+ units

**✅ CORRECT APPROACH:**

Use hierarchical coordinates. Keep numbers small.

---

### 🪲 BUG SCENARIO: The Jittery Player

**Symptom:** Player moves smoothly near spawn, but jitters/stutters far from origin.

**Code:**

```c
// ❌ BAD: Single float for world position
float player_x = 50000.0f;  // Far from origin

// Every frame:
player_x += velocity * 0.016f;  // 60 FPS
// velocity = 5 pixels/sec
// movement = 5 * 0.016 = 0.08 pixels

// At x=50000, precision is ~0.006
// 0.08 gets rounded to 0.0 or 0.128!
// Player either doesn't move or jumps too far!
```

**Debugging:**

```c
printf("Player X: %.10f\n", player_x);
// Output: 50000.0000000000  (no sub-pixel precision!)

printf("Velocity: %.10f\n", velocity * frame_time);
// Output: 0.0800000000

printf("After add: %.10f\n", player_x + velocity * frame_time);
// Output: 50000.0000000000  (lost!)
```

**✅ FIX:**

```c
// Keep player near origin by using chunk/tile system
int chunk_x = 100;  // Far away
float local_x = 5.0f;  // But local position is small!

local_x += velocity * frame_time;  // Works perfectly! ✅
```

**The Math:**

```
Precision = largest_value / (2^23)

At x=10000:
  precision ≈ 10000 / 8388608 ≈ 0.0012 pixels

You CANNOT represent positions more precisely than ~0.001 pixels!
```

**Web Dev Analogy:**

```javascript
// JavaScript has similar issues
let x = 10000000000000000; // 16 zeros
x += 1;
console.log(x === 10000000000000000); // true! The +1 was lost!
```

---

### The Solution: Keep Numbers Small

Instead of:

```
Player at x=100,000 pixels
```

Do:

```
Player at:
  - Chunk 10
  - Position 0.0 pixels within that chunk
```

**Analogy: Street Addresses**

Bad (naive):

```
John lives at position 523,481,200 millimeters from the Prime Meridian
```

Good (hierarchical):

```
John lives at:
  Country: Egypt
  City: Cairo
  Street: Tahrir
  Building: 42
  Apartment: 7
```

**Why hierarchical is better:**

1. **Numbers stay small** (apartment 7, not 523481200)
2. **Easy to understand** (I know Cairo!)
3. **Easy to compare** (same city? same street?)
4. **Easy to expand** (add more buildings to street)

---

<a name="mental-model-1"></a>

## 2. Mental Model #1: The Naive Approach

Let's build this up from zero knowledge.

### Program #1: Static Screen, No World

```c
// The ENTIRE universe is 800×600 pixels
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600

typedef struct {
  float x;  // 0 to 800
  float y;  // 0 to 600
} Position;

Position player = { .x = 400, .y = 300 };  // Center of screen
```

**Coordinate System:**

```
(0,0) ──────────────────────────────────► x
│
│         (400, 300)
│              ●  Player
│
│
▼
y

           SCREEN_WIDTH = 800
```

**Questions to answer:**

- Where is player? **x=400, y=300**
- Is player on screen? **Always yes** (world = screen)
- Can player leave screen? **No** (world ends at screen edge)

**Web Dev Analog:**

```javascript
// Like a fixed div with no scrolling
const player = { x: 400, y: 300 };
canvas.fillRect(player.x, player.y, 50, 50);
```

✅ **This works fine for:** Pong, Tetris, static board games

❌ **This FAILS for:** Platformers, RPGs, anything with a camera

---

### 🎯 HANDS-ON: Try This Right Now!

**Open a C file and write this:**

```c
#include <stdio.h>

int main() {
    float x = 0.0f;

    // Simulate player moving 1000 times
    for (int i = 0; i < 1000; i++) {
        x += 100.0f;  // Move 100 pixels each step
    }

    printf("Small movements, total: %.10f\n", x);

    // Now try to move 0.1 pixels
    x += 0.1f;
    printf("After +0.1: %.10f\n", x);

    // Did it work?
    if (x == 100000.1f) {
        printf("✅ Precision maintained!\n");
    } else {
        printf("❌ Lost precision! Expected 100000.1\n");
    }

    return 0;
}
```

**Compile and run:**

```bash
gcc -o test test.c && ./test
```

**What happens?** You'll see precision loss in action!

---

### Program #2: Scrolling World (First Attempt)

Now the world is BIGGER than the screen. We need a **camera**.

```c
#define SCREEN_WIDTH 800
#define SCREEN_HEIGHT 600
#define WORLD_WIDTH 3200   // 4× wider than screen
#define WORLD_HEIGHT 2400  // 4× taller than screen

typedef struct {
  float world_x;  // Position in world (0 to 3200)
  float world_y;  // Position in world (0 to 2400)
} WorldPosition;

WorldPosition player = { .world_x = 1600, .world_y = 1200 };  // World center

typedef struct {
  float x;  // Camera position in world
  float y;
} Camera;

Camera camera = { .x = 1200, .y = 900 };  // Camera at offset
```

**Drawing the player:**

```c
void draw_player(Canvas* canvas, WorldPosition player, Camera camera) {
  // Convert world position to screen position
  float screen_x = player.world_x - camera.x;
  float screen_y = player.world_y - camera.y;

  // Only draw if on screen
  if (screen_x >= 0 && screen_x < SCREEN_WIDTH &&
      screen_y >= 0 && screen_y < SCREEN_HEIGHT) {
    draw_rect(canvas, screen_x, screen_y, 50, 50);
  }
}
```

**Visualization:**

```
World (3200×2400):
┌────────────────────────────────────────────────┐
│                                                │
│                                                │
│         ┌─────────────────┐                   │
│         │  Screen         │                   │
│         │  (camera view)  │                   │
│         │                 │                   │
│         │      ●          │                   │
│         │    Player       │                   │
│         │                 │                   │
│         └─────────────────┘                   │
│              ↑                                 │
│         Camera at                             │
│         (1200, 900)                           │
│                                                │
└────────────────────────────────────────────────┘

Player world position: (1600, 1200)
Camera position: (1200, 900)
Player screen position: (1600-1200, 1200-900) = (400, 300)
```

**Web Dev Analog:**

```javascript
// Like a scrollable div
const worldPos = { x: 1600, y: 1200 };
const cameraOffset = { x: 1200, y: 900 };

const screenX = worldPos.x - cameraOffset.x;
const screenY = worldPos.y - cameraOffset.y;

canvas.fillRect(screenX, screenY, 50, 50);
```

✅ **This works for:** Small 2D platformers, simple RPGs

❌ **This FAILS for:**

- Large worlds (remember float precision!)
- Tile-based collision (how do you check which tile you're on?)
- Chunk loading (can't easily determine "which area am I in?")

---

### 🚫 ANTI-PATTERN #2: "Just Add More Digits"

**Beginner Attempt:**

```c
// ❌ "I'll just use double instead of float!"
typedef struct {
    double world_x;  // 64-bit instead of 32-bit
    double world_y;
} Position;

Position player = {.world_x = 100000.0, .world_y = 50000.0};
```

**Why this is still WRONG:**

1. **Delaying the inevitable:** Doubles lose precision at ~1,000,000+ units
2. **Performance hit:** 2x memory bandwidth, slower SIMD operations
3. **Misses the real lesson:** You need hierarchical thinking for large worlds

**Real-world example:** Minecraft uses CHUNKS (16×16 blocks) for this exact reason!

---

### 🎯 MINI-EXERCISE: Which Tile Am I On?

**Given:**

```c
float world_x = 387.5f;  // pixels
int tile_size = 60;       // pixels per tile
```

**Question:** Which tile (0-indexed) is the player on?

**Common WRONG answers:**

```c
// ❌ WRONG #1: Forgot to convert float to int
int tile_x = world_x / tile_size;  // Compiler warning!

// ❌ WRONG #2: Used wrong rounding
int tile_x = (int)(world_x / tile_size + 0.5);  // Rounds, don't want that!

// ❌ WRONG #3: Used ceiling
int tile_x = ceilf(world_x / tile_size);  // Off by one for exact multiples!
```

**✅ CORRECT:**

```c
int tile_x = (int)floorf(world_x / tile_size);
// 387.5 / 60 = 6.458...
// floor(6.458) = 6
// Answer: Tile 6 ✅
```

**Why floor?** Because tile 0 spans [0, 60), tile 1 spans [60, 120), etc.

**Try it yourself:** What tile is `world_x = 120.0f` on? (Answer: 2, not 1!)

---

<a name="mental-model-2"></a>

## 3. Mental Model #2: Screen vs World Coordinates

Before we go hierarchical, let's nail down the TWO coordinate systems you deal with.

### Coordinate System #1: Screen Space

```
(0,0) ────────────────────────► X
│
│    800×600 pixel window
│
│         ●  (400, 300)
│           Player drawn here
│
▼
Y
```

**Properties:**

- Origin at top-left of window
- X increases right
- Y increases down (graphics convention)
- Range: (0,0) to (screen_width, screen_height)
- **This is where you DRAW**

**C Code:**

```c
// Screen space = where pixels are
draw_rect(canvas, screen_x, screen_y, width, height);
```

---

### Coordinate System #2: World Space

```
World is HUGE (potentially infinite)

        (-∞,-∞)                    (+∞,+∞)
             ◄─────────────────►

             World can be any size

             Player at (1600, 1200)
             Enemy at (3000, 500)
             House at (-200, 800)
```

**Properties:**

- Origin can be anywhere (you decide!)
- Often MUCH larger than screen
- Can be negative
- Units can be pixels, meters, tiles, whatever
- **This is where game logic happens**

**C Code:**

```c
// World space = where things "actually are"
player.world_x += velocity * dt;

// Collision check in world space
if (distance(player.world_pos, enemy.world_pos) < 50) {
  // Hit!
}
```

---

### The Conversion: World → Screen

```c
// World space → Screen space
float screen_x = world_x - camera_x;
float screen_y = world_y - camera_y;

// If result is outside (0,0) to (width, height), it's off-screen
```

**Example:**

```
Camera at (1000, 500)

Entity at world (1200, 700):
  screen_x = 1200 - 1000 = 200  ✅ On screen (0-800)
  screen_y = 700 - 500 = 200    ✅ On screen (0-600)
  DRAW IT!

Entity at world (5000, 300):
  screen_x = 5000 - 1000 = 4000  ❌ Off screen (> 800)
  screen_y = 300 - 500 = -200    ❌ Off screen (< 0)
  DON'T DRAW!
```

---

### Key Insight: Keep Them Separate!

```c
// ❌ BAD: Mixing coordinate systems
typedef struct {
  float x;  // Is this screen or world? WHO KNOWS?!
  float y;
} Position;

// ✅ GOOD: Explicit naming
typedef struct {
  float world_x;
  float world_y;
} WorldPosition;

typedef struct {
  float screen_x;
  float screen_y;
} ScreenPosition;
```

**Casey's Rule:**

> "Never use a bare 'x' or 'y'. Always name your coordinate system.
> If you can't tell which space a coordinate is in, your code is broken."

---

### 🚫 ANTI-PATTERN #3: Mixed Coordinate Systems

**REAL BUG from user code:**

```c
// ❌ BAD: What does this mean?
void draw_entity(Entity e) {
    draw_rect(e.x, e.y, 50, 50);  // Is e.x screen or world? 🤔
}

// Later in code:
entity.x = 1000;  // Setting world position...
draw_entity(entity);  // ...but drawing uses screen position! 👥
```

**✅ FIX:**

```c
void draw_entity(Entity e, Camera camera) {
    float screen_x = e.world_x - camera.x;
    float screen_y = e.world_y - camera.y;
    draw_rect(screen_x, screen_y, 50, 50);  // Clear what we're drawing!
}
```

---

### 🪲 BUG SCENARIO: Off-By-One World Position

**Symptom:** Entity appears 1 pixel off from where you set it.

**Code:**

```c
// ❌ WRONG: Forgot camera is at center, not origin
float screen_x = world_x - camera_x;
float screen_y = world_y - camera_y;
// If camera_x = 400 (center of 800px screen),
// and world_x = 400,
// screen_x = 0 (left edge, not center!)
```

**✅ CORRECT:**

```c
float screen_x = (world_x - camera_x) + (screen_width / 2);
float screen_y = (world_y - camera_y) + (screen_height / 2);
// Now camera_x=400, world_x=400 → screen_x = 0 + 400 = 400 (center!) ✅
```

---

### 🎯 HANDS-ON: Trace World → Screen Conversion

**Given:**

```c
Camera camera = {.x = 1000, .y = 500};
Entity entity = {.world_x = 1200, .world_y = 600};
int screen_width = 800;
int screen_height = 600;
```

**Step-by-step conversion:**

```
1. World position: (1200, 600)
2. Camera offset: (1000, 500)
3. Relative: (1200 - 1000, 600 - 500) = (200, 100)
4. Center camera: (200 + 400, 100 + 300) = (600, 400)
5. Screen position: (600, 400) ← Entity draws here!
```

**Try it:** Where would entity at `(800, 500)` appear? (Answer: 200, 300)

---

<a name="mental-model-3"></a>

## 4. Mental Model #3: Hierarchical Coordinates

Now we level up. Instead of:

```
Player at x=100,000 pixels
```

We use a **hierarchy**:

```
Player at:
  Chunk 10
  Tile 5
  Offset +0.3 meters
```

### Why Hierarchical? The Compression Principle

**Casey's Philosophy: Compression-Oriented Programming**

Your brain can hold ~7 things at once (Miller's Law). Good code compresses information.

**Example: Representing 1 million items**

❌ **Uncompressed** (can't fit in your head):

```
Item 1 at 523481.2
Item 2 at 523481.7
Item 3 at 523482.1
...
Item 1000000 at 999999.9
```

✅ **Compressed** (fits in your head):

```
All items are in:
  Region 52
  SubRegion 3481
  Positions range 0.0 to 1.0
```

Now you can reason: "Oh, they're all in region 52? Then I only load region 52's collision data!"

---

### The Hierarchy: How Handmade Hero Does It

```
┌─────────────────────────────────────────────┐
│ LEVEL 1: TILEMAP                            │
│                                             │
│ World contains multiple tilemaps            │
│ Each tilemap is like a "room" or "chunk"   │
│                                             │
│ Position: (tilemap_x, tilemap_y)           │
│ Example: Tilemap (0,0), Tilemap (1,0)      │
│                                             │
│ Analogy: Country                            │
└─────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────┐
│ LEVEL 2: TILE                               │
│                                             │
│ Each tilemap contains a grid of tiles       │
│ Example: 17×9 tiles per tilemap             │
│                                             │
│ Position: (tile_x, tile_y)                 │
│ Example: Tile (3, 5) within tilemap         │
│                                             │
│ Analogy: City within country                │
└─────────────────────────────────────────────┘
          │
          ▼
┌─────────────────────────────────────────────┐
│ LEVEL 3: SUB-TILE OFFSET                    │
│                                             │
│ Position within a tile (meters or pixels)  │
│ Range: 0.0 to tile_size                     │
│                                             │
│ Position: (tile_rel_x, tile_rel_y)         │
│ Example: 0.3 meters into the tile           │
│                                             │
│ Analogy: Street address within city         │
└─────────────────────────────────────────────┘
```

---

### Full Example: Where Is The Player?

```c
WorldCanonicalPosition player = {
  .tilemap_x = 1,           // Tilemap column 1
  .tilemap_y = 0,           // Tilemap row 0
  .tile_x = 3,              // Tile column 3 within tilemap
  .tile_y = 5,              // Tile row 5 within tilemap
  .tile_rel_offset_x = 0.3, // 0.3 meters into the tile
  .tile_rel_offset_y = 0.7, // 0.7 meters into the tile
};
```

**Visualization:**

```
World Grid:
┌──────────────┬──────────────┬──────────────┐
│ Tilemap      │ Tilemap      │ Tilemap      │
│ (0,0)        │ (1,0)        │ (2,0)        │
│              │              │              │
│              │ ╔══════════╗ │              │
│              │ ║ Tile     ║ │              │
│              │ ║ Grid     ║ │              │
│              │ ║ (17×9)    ║ │              │
│              │ ║          ║ │              │
│              │ ║    ●     ║ │              │
│              │ ║  Player  ║ │              │
│              │ ╚══════════╝ │              │
│              │              │              │
├──────────────┼──────────────┼──────────────┤
│ Tilemap      │ Tilemap      │ Tilemap      │
│ (0,1)        │ (1,1)        │ (2,1)        │
└──────────────┴──────────────┴──────────────┘

Zooming into Tilemap (1,0):
┌─────┬─────┬─────┬─────┬─────┬─────┬...
│ T   │ T   │ T   │ T   │ T   │ T   │
│(0,0)│(1,0)│(2,0)│(3,0)│(4,0)│(5,0)│
├─────┼─────┼─────┼─────┼─────┼─────┤
│ T   │ T   │ T   │ T   │ T   │ T   │
│(0,1)│(1,1)│(2,1)│(3,1)│(4,1)│(5,1)│
├─────┼─────┼─────┼─────┼─────┼─────┤
│ T   │ T   │ T   │ T   │ T   │ T   │
│(0,2)│(1,2)│(2,2)│(3,2)│(4,2)│(5,2)│
├─────┼─────┼─────┼─────┼─────┼─────┤
│ T   │ T   │ T   │ T   │ T   │ T   │
│(0,3)│(1,3)│(2,3)│(3,3)│(4,3)│(5,3)│
├─────┼─────┼─────┼─────┼─────┼─────┤
│ T   │ T   │ T   │╔════╗ T   │ T   │
│(0,4)│(1,4)│(2,4)│║(3,4)║(4,4)│(5,4)│
├─────┼─────┼─────┼╚════╝─────┼─────┤
│ T   │ T   │ T   │ ╔═══╗ T   │ T   │
│(0,5)│(1,5)│(2,5)│ ║(3,5)║(4,5)│(5,5)│
│     │     │     │ ║  ●  ║     │     │
│     │     │     │ ║ Here║     │     │
│     │     │     │ ╚═══╝     │     │
└─────┴─────┴─────┴─────┴─────┴─────┘

Zooming into Tile (3,5):
┌─────────────────────────┐
│ Tile (3,5)              │
│ Size: 1.4 meters²       │
│                         │
│  0.0m              1.4m │
│   ┌──────────────────┐  │
│   │                  │  │
│   │      ●           │  │
│   │    (0.3, 0.7)    │  │
│   │    Player's      │  │
│   │    exact pos     │  │
│   │                  │  │
│   └──────────────────┘  │
│  0.0m              1.4m │
└─────────────────────────┘
```

---

### Why This Is Brilliant

**1. Small Numbers**

```c
// Instead of:
float x = 1234567.8f;  // Loses precision!

// We have:
int tilemap_x = 100;          // Integer, perfect precision
int tile_x = 5;               // Integer, perfect precision
float tile_rel_x = 0.3f;      // Small float, good precision
```

---

### 🚫 ANTI-PATTERN #4: "I'll Just Track Total Offset"

**Beginner Attempt:**

```c
// ❌ BAD: Manually tracking which chunk
typedef struct {
    int chunk_x;
    float total_offset_x;  // Total meters from chunk origin
} Position;

Position pos = {.chunk_x = 5, .total_offset_x = 123.4f};

// Later: Which TILE am I on?
int tile_x = ???;  // Have to recalculate every time! 😩
```

**Why this is WRONG:**

1. **Redundant work:** Every collision check recalculates tile
2. **Error-prone:** Tile calculation might differ in different places
3. **Doesn't scale:** What if you add Z-layers? More dimensions?

**✅ CORRECT: Canonical Form**

```c
typedef struct {
    int tilemap_x;    // Chunk
    int tile_x;       // Tile within chunk (pre-calculated!)
    float offset_x;   // Offset within tile (always small!)
} CanonicalPosition;

// Collision check = trivial!
if (tilemap->tiles[pos.tile_y][pos.tile_x] == WALL) { /* hit! */ }
```

---

### 🎯 HANDS-ON: Mental Math Practice

**Setup:**

```
tile_side_in_meters = 1.4
tiles_per_map = 17
```

**Questions:**

1. Entity at `tilemap=0, tile=8, offset=0.7` — how many meters from world origin?
2. Entity moves from `tile=16, offset=1.2` by `+0.5` meters — what's new position?
3. Two entities: `tile=3, offset=0.3` and `tile=5, offset=0.9` — distance between them?

<details>
<summary>Answers</summary>

1. **Total meters:**

   ```
   tilemap: 0 * (17 * 1.4) = 0
   tile: 8 * 1.4 = 11.2
   offset: 0.7
   Total: 0 + 11.2 + 0.7 = 11.9 meters
   ```

2. **New position:**

   ```
   Start: tile=16, offset=1.2
   Move: +0.5
   New offset: 1.2 + 0.5 = 1.7
   1.7 > 1.4 → overflow!

   Overflow: floor(1.7 / 1.4) = 1 tile
   New tile: 16 + 1 = 17
   17 >= 17 → wrap to next tilemap!

   Final: tilemap=1, tile=0, offset=0.3
   ```

3. **Distance:**
   ```
   Entity 1: tile=3, offset=0.3 → 3*1.4 + 0.3 = 4.5m
   Entity 2: tile=5, offset=0.9 → 5*1.4 + 0.9 = 7.9m
   Distance: 7.9 - 4.5 = 3.4 meters
   ```

</details>

**2. Easy Collision Checks**

```c
// "Which tile am I on?" is TRIVIAL:
int tile_x = player.tile_x;
int tile_y = player.tile_y;

// Check if that tile is solid:
if (tilemap->tiles[tile_y][tile_x] == TILE_WALL) {
  // Collision!
}
```

**3. Easy Chunk Loading**

```c
// "Which tilemap am I in?" is TRIVIAL:
int tilemap_x = player.tilemap_x;
int tilemap_y = player.tilemap_y;

// Load only nearby tilemaps:
load_tilemap(world, tilemap_x - 1, tilemap_y);  // Left
load_tilemap(world, tilemap_x + 1, tilemap_y);  // Right
load_tilemap(world, tilemap_x, tilemap_y - 1);  // Up
load_tilemap(world, tilemap_x, tilemap_y + 1);  // Down
```

**4. Easy Distance Checks**

```c
// "Are these two entities close?"
// Only meaningful if they're in nearby tiles!

if (abs(entity1.tilemap_x - entity2.tilemap_x) > 1 ||
    abs(entity1.tilemap_y - entity2.tilemap_y) > 1) {
  // Too far apart, don't even bother calculating distance
  return false;
}

// They're close, now do precise distance check
```

---

<a name="old-system"></a>

## 5. The Old System (Before Day 30)

Let's look at what you had BEFORE the commit.

### Data Structures (Old)

```c
// The player was stored with PIXEL coordinates
typedef struct {
  float x;          // Pixel position in tilemap
  float y;          // Pixel position in tilemap
  int tilemap_x;    // Which tilemap (column)
  int tilemap_y;    // Which tilemap (row)

  float width;      // In pixels
  float height;     // In pixels

  // Colors
  float color_r, color_g, color_b, color_a;
} Player;

// For checking positions
typedef struct {
  float offset_x;   // Pixel offset from tilemap origin
  float offset_y;   // Pixel offset from tilemap origin
  i32 tilemap_x;    // Which tilemap
  i32 tilemap_y;    // Which tilemap
} TilemapRelativePosition;
```

**Key Point:** Units were **PIXELS**.

---

### How Movement Worked (Old)

```c
// Player movement (BEFORE)
void handle_controls(...) {
  float d_player_x = 0.0f;
  float d_player_y = 0.0f;

  if (controller->move_up.is_down) {
    d_player_y = -game->speed;    // speed in pixels/sec
  }
  // ... other directions

  // Calculate new position IN PIXELS
  TilemapRelativePosition new_pos = {
    .offset_x = game->player.x + (d_player_x * frame_time),
    .offset_y = game->player.y + (d_player_y * frame_time),
    .tilemap_x = game->player.tilemap_x,
    .tilemap_y = game->player.tilemap_y,
  };

  // Convert to canonical (handles tilemap transitions)
  WorldCanonicalPosition canonical = get_canonical_pos(&game->world, new_pos);

  // Check if tile is empty
  if (is_world_point_empty(&game->world, canonical)) {
    // Update player position
    game->player.x = new_pos.offset_x;
    game->player.y = new_pos.offset_y;
    game->player.tilemap_x = canonical.tilemap_x;
    game->player.tilemap_y = canonical.tilemap_y;
  }
}
```

---

### The get_canonical_pos() Function (Old)

This was the HEART of the old system. Let's break it down completely.

```c
WorldCanonicalPosition get_canonical_pos(World *world,
                                         TilemapRelativePosition pos) {
  WorldCanonicalPosition result = {0};

  // ─── START with same tilemap ───
  result.tilemap_x = pos.tilemap_x;
  result.tilemap_y = pos.tilemap_y;

  // ─── STEP 1: Convert to tilemap-relative coordinates ───
  //
  // World has an "origin" (where tilemap[0,0] starts on screen)
  // Subtract origin to get position relative to tilemap[0,0]
  //
  // Example:
  //   origin_x = -30 (tilemap starts 30px left of screen)
  //   offset_x = 150 (player is 150px from screen left)
  //   tilemap_offset_x = 150 - (-30) = 180px into tilemap
  //
  float tilemap_offset_x = pos.offset_x - world->origin_x;
  float tilemap_offset_y = pos.offset_y - world->origin_y;

  // ─── STEP 2: Calculate tile indices ───
  //
  // Divide by tile size to get which tile we're in
  // floor() ensures negative values round toward -∞
  //
  // Example:
  //   tilemap_offset_x = 180
  //   tile_side_in_px = 60
  //   tile_x = floor(180 / 60) = floor(3.0) = 3
  //
  // Example (negative):
  //   tilemap_offset_x = -30
  //   tile_side_in_px = 60
  //   tile_x = floor(-30 / 60) = floor(-0.5) = -1 (overflow!)
  //
  result.tile_x = floorf(tilemap_offset_x / world->tile_side_in_px);
  result.tile_y = floorf(tilemap_offset_y / world->tile_side_in_px);

  // ─── STEP 3: Calculate sub-tile offset ───
  //
  // This is position WITHIN the tile (0 to tile_size-1)
  //
  // Example:
  //   tilemap_offset_x = 180
  //   tile_x = 3
  //   tile_side_in_px = 60
  //   tile_rel_offset_x = 180 - (3 * 60) = 180 - 180 = 0
  //
  // Example:
  //   tilemap_offset_x = 195
  //   tile_x = 3
  //   tile_rel_offset_x = 195 - (3 * 60) = 195 - 180 = 15
  //
  result.tile_rel_offset_x =
      tilemap_offset_x - (result.tile_x * world->tile_side_in_px);
  result.tile_rel_offset_y =
      tilemap_offset_y - (result.tile_y * world->tile_side_in_px);

  // ─── Sanity checks ───
  DEV_ASSERT(result.tile_rel_offset_x >= 0);
  DEV_ASSERT(result.tile_rel_offset_y >= 0);
  DEV_ASSERT(result.tile_rel_offset_x < world->tile_side_in_px);
  DEV_ASSERT(result.tile_rel_offset_y < world->tile_side_in_px);

  // ─── STEP 4: Handle tilemap transitions ───
  //
  // If tile_x < 0, we've moved LEFT into previous tilemap
  // If tile_x >= tiles_per_map, we've moved RIGHT into next tilemap
  //
  if (result.tile_x < 0) {
    result.tile_x = result.tile_x + world->tiles_per_map_x_count - 1;
    --result.tilemap_x;
  }

  if (result.tile_y < 0) {
    result.tile_y = result.tile_y + world->tiles_per_map_y_count;
    --result.tilemap_y;
  }

  if (result.tile_x >= world->tiles_per_map_x_count) {
    result.tile_x = result.tile_x - world->tiles_per_map_x_count + 1;
    ++result.tilemap_x;
  }

  if (result.tile_y >= world->tiles_per_map_y_count) {
    result.tile_y = result.tile_y - world->tiles_per_map_y_count;
    ++result.tilemap_y;
  }

  return result;
}
```

---

### Example Walkthrough (Old System)

Let's trace through a specific example step-by-step.

**Setup:**

```c
World world = {
  .origin_x = 0,
  .origin_y = 0,
  .tile_side_in_px = 60,
  .tiles_per_map_x_count = 17,
  .tiles_per_map_y_count = 9,
};

Player player = {
  .x = 150,
  .y = 100,
  .tilemap_x = 0,
  .tilemap_y = 0,
};

// Player moves right by 50 pixels
TilemapRelativePosition new_pos = {
  .offset_x = 150 + 50,  // = 200
  .offset_y = 100,
  .tilemap_x = 0,
  .tilemap_y = 0,
};
```

**Calling get_canonical_pos:**

```
STEP 1: Tilemap-relative coordinates
  tilemap_offset_x = 200 - 0 = 200
  tilemap_offset_y = 100 - 0 = 100

STEP 2: Tile indices
  tile_x = floor(200 / 60) = floor(3.333) = 3
  tile_y = floor(100 / 60) = floor(1.666) = 1

STEP 3: Sub-tile offset
  tile_rel_offset_x = 200 - (3 * 60) = 200 - 180 = 20
  tile_rel_offset_y = 100 - (1 * 60) = 100 - 60 = 40

STEP 4: Tilemap transitions?
  tile_x = 3, tiles_per_map_x = 17
  3 >= 0 ✅ and 3 < 17 ✅
  No transition needed!

RESULT:
  tilemap_x = 0
  tilemap_y = 0
  tile_x = 3
  tile_y = 1
  tile_rel_offset_x = 20 pixels
  tile_rel_offset_y = 40 pixels
```

**Visualization:**

```
Tilemap (0,0):
┌────┬────┬────┬────┬────┬────┬...
│    │    │    │    │    │    │
│(0,0)│(1,0)│(2,0)│(3,0)│(4,0)│(5,0)│
│    │    │    │    │    │    │
├────┼────┼────┼────┼────┼────┤
│    │    │    │╔═══╗    │    │
│(0,1)│(1,1)│(2,1)│║(3,1)║(4,1)│(5,1)│
│    │    │    │║ ● ║    │    │
│    │    │    │║   ║    │    │
│    │    │    │╚═══╝    │    │
└────┴────┴────┴────┴────┴────┘

Player is at tile (3,1)
Within that tile, at offset (20px, 40px)

Tile (3,1) spans pixels:
  X: 180 to 240 (tile_x * 60 = 180)
  Y: 60 to 120  (tile_y * 60 = 60)

Player at pixels (200, 100):
  200 - 180 = 20 pixels into the tile ✅
  100 - 60 = 40 pixels into the tile ✅
```

---

### Problems With The Old System

1. **Mixed units** (pixels in some places, meters in others)
2. **World origin confusion** (what does origin_x actually mean?)
3. **That weird ±1 in tilemap transitions** (why -1 and +1?)
4. **Harder to reason about** (converting pixels → tiles → offsets)

Casey realized this and SIMPLIFIED it. Let's see how.

---

<a name="new-system"></a>

## 6. The New System (Day 30+)

Casey's big insight: **"Let's just use meters everywhere and keep coordinates canonical."**

### Data Structures (New)

```c
// Player now stores CANONICAL position in METERS
typedef struct {
  WorldCanonicalPosition world_canonical_pos;  // Primary position

  float width;       // Still in meters
  float height;      // Still in meters

  // Colors (unchanged)
  float color_r, color_g, color_b, color_a;
} Player;

// Canonical position: Always valid, always in range
typedef struct {
  i32 tilemap_x;          // Which tilemap (column)
  i32 tilemap_y;          // Which tilemap (row)

  i32 tile_x;             // Which tile in tilemap (column)
  i32 tile_y;             // Which tile in tilemap (row)

  f32 tile_rel_offset_x;  // Meters within tile (0 to tile_side_in_meters)
  f32 tile_rel_offset_y;  // Meters within tile (0 to tile_side_in_meters)
} WorldCanonicalPosition;
```

**Key Differences:**

1. **No more separate x, y in player** - only canonical position
2. **No more TilemapRelativePosition** - we work directly with canonical
3. **Units are METERS** not pixels
4. **Position is ALWAYS canonical** (we "recanon" after every change)

---

### What Does "Canonical" Mean?

**Canonical** = "In standard/normal form"

A canonical position has these **invariants** (rules that are always true):

```c
// INVARIANTS (must ALWAYS be true):
// 1. tile_rel_offset is in range [0, tile_side_in_meters)
DEV_ASSERT(pos.tile_rel_offset_x >= 0);
DEV_ASSERT(pos.tile_rel_offset_x < world->tile_side_in_meters);

// 2. tile indices are in range [0, tiles_per_map)
DEV_ASSERT(pos.tile_x >= 0);
DEV_ASSERT(pos.tile_x < world->tiles_per_map_x_count);
DEV_ASSERT(pos.tile_y >= 0);
DEV_ASSERT(pos.tile_y < world->tiles_per_map_y_count);

// 3. tilemap indices can be any integer
// (no bounds check - world is infinite!)
```

**Example of NON-canonical positions:**

```c
// ❌ NON-CANONICAL: tile_rel_offset out of range
WorldCanonicalPosition bad1 = {
  .tilemap_x = 0, .tilemap_y = 0,
  .tile_x = 3, .tile_y = 5,
  .tile_rel_offset_x = 2.0,  // ❌ If tile_side_in_meters = 1.4,
  .tile_rel_offset_y = 0.5,  //    this should overflow to next tile!
};

// ❌ NON-CANONICAL: tile index out of range
WorldCanonicalPosition bad2 = {
  .tilemap_x = 0, .tilemap_y = 0,
  .tile_x = 20,  // ❌ If tiles_per_map_x = 17,
  .tile_y = 5,   //    this should overflow to next tilemap!
  .tile_rel_offset_x = 0.5,
  .tile_rel_offset_y = 0.5,
};

// ❌ NON-CANONICAL: negative values
WorldCanonicalPosition bad3 = {
  .tilemap_x = 0, .tilemap_y = 0,
  .tile_x = -1,  // ❌ Should wrap to previous tilemap!
  .tile_y = 5,
  .tile_rel_offset_x = 0.5,
  .tile_rel_offset_y = 0.5,
};
```

---

### 🚫 ANTI-PATTERN #5: "I'll Check For Validity Later"

**Beginner Mistake:**

```c
// ❌ BAD: Create non-canonical position, use it
WorldCanonicalPosition pos = {
    .tile_x = 3,
    .tile_rel_offset_x = 5.0f,  // Way out of bounds!
};

// Use it immediately without checking!
if (is_world_point_empty(&world, pos)) {  // 💣 BUG!
    // Checking wrong tile!
}
```

**What goes WRONG:**

```
Assuming tile_side_in_meters = 1.4:

Offset 5.0 meters should be in tile:
  3 + floor(5.0 / 1.4) = 3 + 3 = tile 6

But we're checking tile 3!
  Collision check on WRONG tile → player walks through walls! 👻
```

**✅ CORRECT:**

```c
WorldCanonicalPosition pos = {
    .tile_x = 3,
    .tile_rel_offset_x = 5.0f,
};

// ALWAYS recanonicalize before use!
pos = recanonicalize_pos(&world, pos);  // ✅

// Now pos = {tile_x=6, offset_x=0.8}
if (is_world_point_empty(&world, pos)) {  // Checking correct tile! ✅
    // ...
}
```

---

### 🪲 BUG SCENARIO: The Teleporting Player

**Symptom:** Player randomly jumps to wrong location.

**Code:**

```c
// Movement update
player.world_canonical_pos.tile_rel_offset_x += velocity * dt;

// ❌ FORGOT TO RECANONICALIZE!

// On next frame:
if (player.world_canonical_pos.tile_rel_offset_x > 2.0f) {
    // This assert might NOT fire because we're checking wrong values!
}

// Later, when recanonicalizing finally happens:
player.world_canonical_pos = recanonicalize_pos(&world, player.world_canonical_pos);
// Suddenly tile_x changes from 5 to 6!
// Visually looks like teleportation! 😱
```

**Why it happens:**

```
Frame 1: tile_x=5, offset=1.3
Move +0.3: offset=1.6 (NON-CANONICAL, but not caught!)

Frame 2: Move +0.3: offset=1.9 (still not caught!)

Frame 3: Some other code calls recanonicalize
Suddenly: tile_x=6, offset=0.5

Visually: Player jumps from one tile to another instantly!
```

**✅ FIX:**

```c
// ALWAYS recanonicalize IMMEDIATELY after modifying position
player.world_canonical_pos.tile_rel_offset_x += velocity * dt;
player.world_canonical_pos = recanonicalize_pos(&world, player.world_canonical_pos); // ✅

// Now transitions are smooth!
```

---

### 🎯 HANDS-ON: Spot The Non-Canonical Positions

**Given:**

```c
World world = {
    .tile_side_in_meters = 1.4f,
    .tiles_per_map_x_count = 17,
    .tiles_per_map_y_count = 9,
};
```

**Which of these are canonical? Put ✅ or ❌ next to each:**

```c
1. {tilemap=0, tile_x=5,  offset_x=0.7}   __
2. {tilemap=0, tile_x=16, offset_x=1.5}   __
3. {tilemap=0, tile_x=-1, offset_x=0.3}   __
4. {tilemap=1, tile_x=0,  offset_x=0.0}   __
5. {tilemap=0, tile_x=17, offset_x=0.0}   __
6. {tilemap=0, tile_x=8,  offset_x=-0.1}  __
7. {tilemap=0, tile_x=10, offset_x=1.4}   __
8. {tilemap=-1,tile_x=16, offset_x=1.39}  __
```

<details>
<summary>Answers</summary>

```
1. ✅ Canonical (all values in range)
2. ❌ Non-canonical (offset=1.5 > 1.4)
3. ❌ Non-canonical (tile_x < 0)
4. ✅ Canonical (tilemap can be any int, tile=0 is valid)
5. ❌ Non-canonical (tile_x=17 >= 17)
6. ❌ Non-canonical (offset < 0)
7. ❌ Non-canonical (offset=1.4 is NOT < 1.4, though we allow <= for FP)
8. ✅ Canonical (tilemap=-1 is OK, 1.39 < 1.4 ✅)
```

</details>

**To FIX non-canonical positions, we "recanonicalize":**

```c
// Convert non-canonical → canonical
WorldCanonicalPosition canonical = recanonicalize_pos(&world, bad_pos);

// Now all invariants are satisfied!
```

---

### The Core Function: recanonicalize_coord()

This is the NEW heart of the system. Much simpler than get_canonical_pos()!

```c
// Recanonicalize a SINGLE coordinate (x or y)
//
// BEFORE: tile_rel might be < 0 or >= tile_side_in_meters
//         tile might be < 0 or >= tiles_per_map
//
// AFTER:  tile_rel is in [0, tile_side_in_meters)
//         tile is in [0, tiles_per_map)
//         tilemap is adjusted if needed
//
void recanonicalize_coord(World *world,
                         i32 tiles_per_map,
                         i32 *tilemap,
                         i32 *tile,
                         f32 *tile_rel) {

  // ─── STEP 1: Fix tile_rel overflow ───
  //
  // How many FULL tiles did tile_rel overflow/underflow?
  //
  // Example:
  //   tile_rel = 2.5 meters
  //   tile_side_in_meters = 1.4 meters
  //   offset = floor(2.5 / 1.4) = floor(1.785) = 1
  //   → We're 1 full tile over!
  //
  // Example (negative):
  //   tile_rel = -0.3 meters
  //   tile_side_in_meters = 1.4 meters
  //   offset = floor(-0.3 / 1.4) = floor(-0.214) = -1
  //   → We're 1 full tile under!
  //
  i32 offset = floorf(*tile_rel / world->tile_side_in_meters);

  // Move those full tiles into the tile index
  *tile += offset;

  // Remove the overflow from tile_rel
  //
  // Example:
  //   tile_rel = 2.5, offset = 1, tile_side = 1.4
  //   tile_rel = 2.5 - (1 * 1.4) = 2.5 - 1.4 = 1.1 meters
  //   → Now in range [0, 1.4) ✅
  //
  // Example (negative):
  //   tile_rel = -0.3, offset = -1, tile_side = 1.4
  //   tile_rel = -0.3 - (-1 * 1.4) = -0.3 + 1.4 = 1.1 meters
  //   → Now in range [0, 1.4) ✅
  //
  *tile_rel -= offset * world->tile_side_in_meters;

  // Sanity checks
  DEV_ASSERT(*tile_rel >= 0);
  DEV_ASSERT(*tile_rel <= world->tile_side_in_meters);  // <= due to FP imprecision

  // ─── STEP 2: Fix tile overflow ───
  //
  // If tile < 0, move to previous tilemap
  // If tile >= tiles_per_map, move to next tilemap
  //
  if (*tile < 0) {
    //   tile = -1, tiles_per_map = 17
    //   tile = -1 + 17 = 16 ✅ (last tile of previous tilemap)
    //
    //   tile = -2, tiles_per_map = 17
    //   tile = -2 + 17 = 15 ✅
    //
    *tile += tiles_per_map;
    --*tilemap;  // Move to previous tilemap

  } else if (*tile >= tiles_per_map) {
    //   tile = 17, tiles_per_map = 17
    //   tile = 17 - 17 = 0 ✅ (first tile of next tilemap)
    //
    //   tile = 18, tiles_per_map = 17
    //   tile = 18 - 17 = 1 ✅
    //
    *tile -= tiles_per_map;
    ++*tilemap;  // Move to next tilemap
  }

  // Now tile is in [0, tiles_per_map) ✅
}
```

---

### The Wrapper: recanonicalize_pos()

```c
// Recanonicalize BOTH x and y coordinates
WorldCanonicalPosition recanonicalize_pos(World *world,
                                          WorldCanonicalPosition pos) {
  WorldCanonicalPosition result = pos;

  // Fix X coordinate
  recanonicalize_coord(world,
                      world->tiles_per_map_x_count,
                      &result.tilemap_x,
                      &result.tile_x,
                      &result.tile_rel_offset_x);

  // Fix Y coordinate
  recanonicalize_coord(world,
                      world->tiles_per_map_y_count,
                      &result.tilemap_y,
                      &result.tile_y,
                      &result.tile_rel_offset_y);

  return result;
}
```

**Usage:**

```c
// Player moves
player.world_canonical_pos.tile_rel_offset_x += velocity_x * dt;
player.world_canonical_pos.tile_rel_offset_y += velocity_y * dt;

// Position might now be non-canonical (overflowed tile bounds)
// So recanonicalize it:
player.world_canonical_pos =
    recanonicalize_pos(&world, player.world_canonical_pos);

// Now guaranteed canonical again! ✅
```

---

<a name="how-it-works"></a>

## 7. Step-by-Step: How Recanonicalizing Works

Let's trace through concrete examples.

### Example 1: Moving Right Within a Tile

**Before:**

```c
World world = {
  .tile_side_in_meters = 1.4f,
  .tiles_per_map_x_count = 17,
};

WorldCanonicalPosition pos = {
  .tilemap_x = 0,
  .tilemap_y = 0,
  .tile_x = 3,
  .tile_y = 5,
  .tile_rel_offset_x = 0.5,  // 0.5 meters into tile
  .tile_rel_offset_y = 0.7,
};

// Player moves right by 0.3 meters
pos.tile_rel_offset_x += 0.3;  // Now 0.8 meters
```

**After adding velocity:**

```
pos.tile_rel_offset_x = 0.8 meters
```

**Is this canonical?**

```
tile_rel_offset_x = 0.8
tile_side_in_meters = 1.4

0 <= 0.8 < 1.4  ✅ IN RANGE!
```

**Calling recanonicalize_coord:**

```
STEP 1: Calculate offset
  offset = floor(0.8 / 1.4) = floor(0.571) = 0

  *tile += 0           → tile_x stays 3
  *tile_rel -= 0 * 1.4 → tile_rel stays 0.8

STEP 2: Check tile bounds
  tile_x = 3
  0 <= 3 < 17  ✅ IN RANGE!

  No tilemap transition needed.

RESULT:
  tilemap_x = 0 (unchanged)
  tile_x = 3 (unchanged)
  tile_rel_offset_x = 0.8 (unchanged)
```

**Conclusion:** Position was already canonical, no change needed!

---

### Example 2: Moving Right Across Tile Boundary

**Before:**

```c
WorldCanonicalPosition pos = {
  .tilemap_x = 0,
  .tile_x = 3,
  .tile_rel_offset_x = 1.2,  // Close to edge
};

// Player moves right by 0.5 meters
pos.tile_rel_offset_x += 0.5;  // Now 1.7 meters (OVERFLOW!)
```

**After adding velocity:**

```
pos.tile_rel_offset_x = 1.7 meters
tile_side_in_meters = 1.4 meters

1.7 >= 1.4  ❌ OUT OF RANGE! Not canonical!
```

**Calling recanonicalize_coord:**

```
STEP 1: Calculate offset
  offset = floor(1.7 / 1.4) = floor(1.214) = 1

  We've overflowed by 1 full tile!

  *tile += 1
    tile_x = 3 + 1 = 4

  *tile_rel -= 1 * 1.4
    tile_rel = 1.7 - 1.4 = 0.3 meters

  Now tile_rel is back in range [0, 1.4) ✅

STEP 2: Check tile bounds
  tile_x = 4
  0 <= 4 < 17  ✅ IN RANGE!

  No tilemap transition needed.

RESULT:
  tilemap_x = 0 (unchanged)
  tile_x = 4 (was 3, now 4 - moved to next tile!)
  tile_rel_offset_x = 0.3 (was 1.7, now 0.3 - wrapped!)
```

**Visualization:**

```
BEFORE:
┌────────┬────────┬────────┬────────┬────────┐
│ Tile 2 │ Tile 3 │ Tile 4 │ Tile 5 │ Tile 6 │
│        │        │        │        │        │
│        │     ●──┼──►     │        │        │
│        │   1.2m │  +0.5m │        │        │
│        │        │  =1.7m │        │        │
└────────┴────────┴────────┴────────┴────────┘
         └──1.4m─►─┘
         Tile size

AFTER:
┌────────┬────────┬────────┬────────┬────────┐
│ Tile 2 │ Tile 3 │ Tile 4 │ Tile 5 │ Tile 6 │
│        │        │        │        │        │
│        │        │  ●     │        │        │
│        │        │ 0.3m   │        │        │
│        │        │        │        │        │
└────────┴────────┴────────┴────────┴────────┘

Player crossed tile boundary!
  Was at: Tile 3, offset 1.2m
  Moved: +0.5m
  Now at: Tile 4, offset 0.3m
```

---

### Example 3: Moving Left Into Previous Tile

**Before:**

```c
WorldCanonicalPosition pos = {
  .tilemap_x = 0,
  .tile_x = 3,
  .tile_rel_offset_x = 0.2,  // Close to left edge
};

// Player moves left by 0.5 meters
pos.tile_rel_offset_x += -0.5;  // Now -0.3 meters (UNDERFLOW!)
```

**After adding velocity:**

```
pos.tile_rel_offset_x = -0.3 meters

-0.3 < 0  ❌ OUT OF RANGE! Not canonical!
```

**Calling recanonicalize_coord:**

```
STEP 1: Calculate offset
  offset = floor(-0.3 / 1.4) = floor(-0.214) = -1

  We've underflowed by 1 full tile!

  *tile += -1
    tile_x = 3 + (-1) = 2

  *tile_rel -= (-1) * 1.4
    tile_rel = -0.3 - (-1.4) = -0.3 + 1.4 = 1.1 meters

  Now tile_rel is back in range [0, 1.4) ✅

STEP 2: Check tile bounds
  tile_x = 2
  0 <= 2 < 17  ✅ IN RANGE!

  No tilemap transition needed.

RESULT:
  tilemap_x = 0 (unchanged)
  tile_x = 2 (was 3, now 2 - moved to previous tile!)
  tile_rel_offset_x = 1.1 (was -0.3, now 1.1 - wrapped!)
```

**Visualization:**

```
BEFORE:
┌────────┬────────┬────────┬────────┐
│ Tile 1 │ Tile 2 │ Tile 3 │ Tile 4 │
│        │        │        │        │
│        │    ◄───┼─●      │        │
│        │ -0.5m  │0.2m    │        │
│        │        │=-0.3m  │        │
└────────┴────────┴────────┴────────┘
         └──1.4m─►─┘

AFTER:
┌────────┬────────┬────────┬────────┐
│ Tile 1 │ Tile 2 │ Tile 3 │ Tile 4 │
│        │        │        │        │
│        │      ● │        │        │
│        │    1.1m│        │        │
│        │        │        │        │
└────────┴────────┴────────┴────────┘

Player crossed tile boundary backward!
  Was at: Tile 3, offset 0.2m
  Moved: -0.5m (total -0.3m)
  Now at: Tile 2, offset 1.1m (wrapped from -0.3)
```

**The Math (why 1.1?):**

```
We want to find: "What's -0.3 meters in the previous tile?"

Previous tile spans: [0, 1.4) meters

-0.3 meters before the current tile's origin
= (tile_size - 0.3) meters into the previous tile
= 1.4 - 0.3
= 1.1 meters  ✅

Verification:
  Previous tile ends at 1.4m
  Current tile starts at 0m
  -0.3m before current tile's start
  = 1.4 - 0.3 from previous tile's start
  = 1.1m ✅
```

---

### Example 4: Moving Across Tilemap Boundary

**Before:**

```c
WorldCanonicalPosition pos = {
  .tilemap_x = 0,
  .tile_x = 16,  // Last tile in tilemap (tiles are 0-16)
  .tile_rel_offset_x = 1.2,
};

// tiles_per_map_x_count = 17
// tile_side_in_meters = 1.4

// Player moves right by 0.5 meters
pos.tile_rel_offset_x += 0.5;  // Now 1.7 meters
```

**After adding velocity:**

```
pos.tile_rel_offset_x = 1.7 meters
```

**Calling recanonicalize_coord:**

```
STEP 1: Fix tile_rel overflow
  offset = floor(1.7 / 1.4) = 1

  *tile += 1
    tile_x = 16 + 1 = 17

  *tile_rel -= 1 * 1.4
    tile_rel = 1.7 - 1.4 = 0.3 meters

STEP 2: Check tile bounds
  tile_x = 17
  tiles_per_map = 17

  17 >= 17  ❌ OUT OF RANGE!

  *tile -= tiles_per_map
    tile_x = 17 - 17 = 0  (first tile of next tilemap)

  ++*tilemap
    tilemap_x = 0 + 1 = 1  (moved to next tilemap!)

RESULT:
  tilemap_x = 1 (was 0, now 1 - moved to next tilemap!)
  tile_x = 0 (was 16→17, now 0 - wrapped to first tile!)
  tile_rel_offset_x = 0.3
```

**Visualization:**

```
BEFORE:
┌─────────────────┬─────────────────┐
│ Tilemap 0       │ Tilemap 1       │
├─────┬─────┬─────┼─────┬─────┬─────┤
│ ... │T15  │T16  │T0   │T1   │T2   │
│     │     │     │     │     │     │
│     │     │  ●──┼─►   │     │     │
│     │     │1.2m │+0.5m│     │     │
│     │     │     │=1.7m│     │     │
└─────┴─────┴─────┴─────┴─────┴─────┘
              └──1.4m──►

AFTER:
┌─────────────────┬─────────────────┐
│ Tilemap 0       │ Tilemap 1       │
├─────┬─────┬─────┼─────┬─────┬─────┤
│ ... │T15  │T16  │T0   │T1   │T2   │
│     │     │     │     │     │     │
│     │     │     │ ●   │     │     │
│     │     │     │0.3m │     │     │
│     │     │     │     │     │     │
└─────┴─────┴─────┴─────┴─────┴─────┘

Player crossed TILEMAP boundary!
  Was at: Tilemap 0, Tile 16, offset 1.2m
  Moved: +0.5m
  Now at: Tilemap 1, Tile 0, offset 0.3m
```

---

<a name="code-walkthrough"></a>

## 8. Complete Code Walkthrough

Let's trace through the actual game code with your Day 32 changes.

### Initialization (init.c)

```c
GAME_INIT(game_init) {
  // ─── WORLD SETUP ───
  game->world.tile_side_in_px = 60;
  game->world.tile_side_in_meters = 1.4;

  // Calculate conversion factor
  game->world.meters_to_pixels =
      (f32)game->world.tile_side_in_px / game->world.tile_side_in_meters;
  //  = 60 / 1.4 ≈ 42.857 pixels per meter

  game->world.tiles_per_map_x_count = 17;
  game->world.tiles_per_map_y_count = 9;

  // ─── PLAYER SETUP ───
  f32 player_height = 1.4f;  // 1.4 meters tall

  game->player = (Player){
    .color_r = 0.0f,
    .color_g = 1.0f,
    .color_b = 1.0f,
    .color_a = 1.0f,

    .height = player_height,            // 1.4 meters
    .width = 0.75f * player_height,     // 1.05 meters

    // Start at tilemap (0,0), tile (3,3), offset (5,5) meters
    .world_canonical_pos = (WorldCanonicalPosition){
      .tilemap_x = 0,
      .tilemap_y = 0,
      .tile_x = 3,
      .tile_y = 3,
      .tile_rel_offset_x = 5.0f,  // ❌ WAIT! This will overflow!
      .tile_rel_offset_y = 5.0f,  // ❌ tile_side_in_meters is only 1.4!
    },
  };

  // NOTE: This initial position is NON-CANONICAL!
  // offset 5.0 >= 1.4, so it will wrap to:
  //   tile_x = 3 + floor(5.0/1.4) = 3 + 3 = 6
  //   offset_x = 5.0 - (3*1.4) = 5.0 - 4.2 = 0.8 meters
  //
  // You should recanonicalize after init:
  game->player.world_canonical_pos =
      recanonicalize_pos(&game->world, game->player.world_canonical_pos);
}
```

**After recanonicalizing the initial position:**

```
Original:
  tile_x = 3, offset_x = 5.0

After recanonicalize:
  5.0 / 1.4 = 3.571 → offset = 3
  tile_x = 3 + 3 = 6
  offset_x = 5.0 - (3 * 1.4) = 5.0 - 4.2 = 0.8

Final:
  tile_x = 6, offset_x = 0.8  ✅ Now canonical!
```

---

### Movement (main.c)

```c
void handle_controls(GameControllerInput *inputs,
                     HandMadeHeroGameState *game,
                     f32 frame_time) {

  // ─── CALCULATE VELOCITY ───
  f32 d_player_x = 0.0f;
  f32 d_player_y = 0.0f;

  if (controller->move_up.is_down) {
    d_player_y = -game->speed;  // speed is now in meters/sec (e.g., 2.0)
  }
  if (controller->move_down.is_down) {
    d_player_y = game->speed;
  }
  if (controller->move_left.is_down) {
    d_player_x = -game->speed;
  }
  if (controller->move_right.is_down) {
    d_player_x = game->speed;
  }

  // ─── CALCULATE NEW POSITION ───
  //
  // Start with current (canonical) position
  WorldCanonicalPosition player_bottom_center_pos =
      game->player.world_canonical_pos;

  // Add velocity (might make position non-canonical!)
  player_bottom_center_pos.tile_rel_offset_x += (d_player_x * frame_time);
  player_bottom_center_pos.tile_rel_offset_y += (d_player_y * frame_time);

  // Fix any overflow/underflow
  player_bottom_center_pos =
      recanonicalize_pos(&game->world, player_bottom_center_pos);

  // ─── CHECK COLLISION ───
  //
  // Player origin is at bottom-center, but we need to check
  // the full width of the player
  //
  //        ┌─────────┐
  //        │         │
  //        │ Player  │
  //        │         │
  //        └────●────┘
  //     left   │  right
  //           center
  //

  // Left foot
  WorldCanonicalPosition player_bottom_left_pos = player_bottom_center_pos;
  player_bottom_left_pos.tile_rel_offset_x -= game->player.width * 0.5f;
  player_bottom_left_pos =
      recanonicalize_pos(&game->world, player_bottom_left_pos);

  // Right foot
  WorldCanonicalPosition player_bottom_right_pos = player_bottom_center_pos;
  player_bottom_right_pos.tile_rel_offset_x += game->player.width * 0.5f;
  player_bottom_right_pos =
      recanonicalize_pos(&game->world, player_bottom_right_pos);

  // Check if all three points are empty
  if (is_world_point_empty(&game->world, player_bottom_center_pos) &&
      is_world_point_empty(&game->world, player_bottom_left_pos) &&
      is_world_point_empty(&game->world, player_bottom_right_pos)) {

    // ✅ All clear! Update player position
    game->player.world_canonical_pos = player_bottom_center_pos;
  }
  // else: Collision! Don't update position (player stays where they were)
}
```

---

### Collision Check

```c
bool32 is_world_point_empty(World *world, WorldCanonicalPosition pos) {
  bool32 is_empty = false;

  // Get tilemap at this position
  Tilemap *tilemap = get_tilemap(world, pos.tilemap_x, pos.tilemap_y);

  if (tilemap) {
    // Check if tile is empty (value == 0)
    u32 tile_value = tilemap->tiles[pos.tile_y * world->tiles_per_map_x_count +
                                    pos.tile_x];
    is_empty = (tile_value == 0);
  }

  return is_empty;
}
```

**Key Point:** We check the tile at `(tile_x, tile_y)`, ignoring `tile_rel_offset`.

Why? Because collision is tile-based. Either the entire tile is solid or empty.

---

### Drawing

```c
void draw_player(Canvas *canvas, World *world, Player *player) {
  // ─── CONVERT CANONICAL → SCREEN ───

  // STEP 1: Convert canonical position to pixels
  //
  // Start with tilemap origin (where on screen is tilemap[0,0]?)
  f32 screen_x = -world->origin_x;  // Usually 0
  f32 screen_y = -world->origin_y;

  // Add tilemap offset (which tilemap chunk are we in?)
  screen_x += player->world_canonical_pos.tilemap_x *
              (world->tiles_per_map_x_count * world->tile_side_in_px);
  screen_y += player->world_canonical_pos.tilemap_y *
              (world->tiles_per_map_y_count * world->tile_side_in_px);

  // Add tile offset (which tile in the tilemap?)
  screen_x += player->world_canonical_pos.tile_x * world->tile_side_in_px;
  screen_y += player->world_canonical_pos.tile_y * world->tile_side_in_px;

  // Add sub-tile offset (position within the tile, in meters)
  //   Convert meters → pixels using meters_to_pixels
  screen_x += player->world_canonical_pos.tile_rel_offset_x *
              world->meters_to_pixels;
  screen_y += player->world_canonical_pos.tile_rel_offset_y *
              world->meters_to_pixels;

  // STEP 2: Convert player dimensions (meters → pixels)
  f32 player_width_px = player->width * world->meters_to_pixels;
  f32 player_height_px = player->height * world->meters_to_pixels;

  // STEP 3: Draw!
  draw_rect(canvas,
            screen_x - player_width_px * 0.5f,  // Center horizontally
            screen_y - player_height_px,        // Origin at bottom
            player_width_px,
            player_height_px,
            player->color_r, player->color_g, player->color_b, player->color_a);
}
```

---

<a name="common-mistakes"></a>

## 9. Common Mistakes & Debugging

### Mistake #1: Forgetting to Recanonicalize

```c
// ❌ BAD
player.world_canonical_pos.tile_rel_offset_x += velocity_x * dt;
// Position is now NON-CANONICAL! Bugs will happen!

// ✅ GOOD
player.world_canonical_pos.tile_rel_offset_x += velocity_x * dt;
player.world_canonical_pos = recanonicalize_pos(&world, player.world_canonical_pos);
```

**Symptoms:**

- Asserts firing (`tile_rel_offset >= tile_side_in_meters`)
- Player "teleporting" randomly
- Collision detection breaking
- Entities appearing in wrong tiles

**Real-world debugging:**

```c
// Add this after EVERY position modification during debugging
DEV_ASSERT(is_canonical(&world, player.world_canonical_pos));

// If it fires, add a breakpoint and check:
printf("Non-canonical position detected!\n");
printf("  Tile: (%d, %d)\n", player.world_canonical_pos.tile_x,
                            player.world_canonical_pos.tile_y);
printf("  Offset: (%.3f, %.3f)\n", player.world_canonical_pos.tile_rel_offset_x,
                                   player.world_canonical_pos.tile_rel_offset_y);
printf("  Max offset: %.3f\n", world.tile_side_in_meters);
```

---

### Mistake #2: Mixing Units (Meters vs Pixels)

```c
// ❌ BAD: Using pixels for position
player.world_canonical_pos.tile_rel_offset_x = 100;  // ???

// ✅ GOOD: Using meters
player.world_canonical_pos.tile_rel_offset_x = 0.5;  // 0.5 meters
```

**Rule:** ALL positions are in meters. Convert to pixels ONLY for drawing.

**Where this bites you:**

```c
// ❌ WRONG EXAMPLE:
float player_width_pixels = 50;  // Player is 50 pixels wide

// Check left edge collision
WorldCanonicalPosition left_pos = player_pos;
left_pos.tile_rel_offset_x -= player_width_pixels / 2;  // 👥 BUG!
// This subtracts 25 PIXELS, but offset is in METERS!

// ✅ CORRECT:
float player_width_meters = player_width_pixels / world.meters_to_pixels;
left_pos.tile_rel_offset_x -= player_width_meters / 2;  // ✅
```

**Debugging tip:** Print units in debug messages!

```c
printf("Player width: %.2f meters (%.0f pixels)\n",
       player_width_meters, player_width_pixels);
```

---

### Mistake #3: Not Understanding tile_rel_offset Range

```c
// tile_side_in_meters = 1.4

// ❌ WRONG: Thinking tile_rel_offset is 0 to 1
pos.tile_rel_offset_x = 0.5;  // Not "50% of tile"!

// ✅ RIGHT: tile_rel_offset is 0 to tile_side_in_meters
pos.tile_rel_offset_x = 0.7;  // 0.7 meters (out of 1.4)
```

**Mental Model:**

```
Tile spans [0, tile_side_in_meters) meters
  NOT [0, 1) normalized!

Example:
  tile_side_in_meters = 1.4
  tile_rel_offset = 0.7
  → Halfway through the tile (0.7 / 1.4 = 50%)
```

**Conversion formulas:**

```c
// Meters to percentage of tile
float pct = tile_rel_offset / world.tile_side_in_meters;

// Percentage to meters
float meters = pct * world.tile_side_in_meters;

// Center of tile
float center = world.tile_side_in_meters * 0.5f;  // 0.7 meters
```

---

### Mistake #4: Ignoring Floating Point Precision

```c
// ❌ Could fail due to floating point imprecision
DEV_ASSERT(tile_rel_offset < tile_side_in_meters);

// ✅ Use <= instead
DEV_ASSERT(tile_rel_offset <= tile_side_in_meters);
```

**Why?**

```
tile_rel_offset might be 1.4000001 due to FP rounding
  Instead of exactly 1.4

So we allow <= instead of strict <
```

**More robust checking:**

```c
#define EPSILON 0.0001f

bool is_canonical_with_epsilon(World *world, WorldCanonicalPosition pos) {
    // Allow small epsilon for floating point errors
    if (pos.tile_rel_offset_x < -EPSILON ||
        pos.tile_rel_offset_x > world->tile_side_in_meters + EPSILON) {
        return false;
    }
    // ... other checks
    return true;
}
```

---

### Mistake #5: Off-By-One in Tile Boundaries

**Common error:**

```c
// ❌ WRONG: Which tiles does a 1.0 meter entity span?
tile_span = (int)(entity_width / tile_side);  // Rounds down!
// 1.0 / 1.4 = 0.71 → 0 tiles (WRONG!)

// ✅ CORRECT: Account for starting position
int start_tile = entity_left_pos.tile_x;
int end_tile = entity_right_pos.tile_x;
int tile_span = end_tile - start_tile + 1;  // Inclusive!
```

**Example:**

```
Entity at tile_x=5, offset=0.3
Entity width = 1.0 meters

Left edge:  tile=5, offset=0.3 - 0.5 = -0.2 → wraps to tile=4, offset=1.2
Right edge: tile=5, offset=0.3 + 0.5 = 0.8 → stays in tile=5

Entity spans: tiles 4 and 5 (2 tiles total)
```

---

### Mistake #6: Recanonicalizing Too Often (Performance)

**Inefficient code:**

```c
// ❌ BAD: Recanonicalizing multiple times
pos.tile_rel_offset_x += 0.1;
pos = recanonicalize_pos(&world, pos);  // Heavy function call!

pos.tile_rel_offset_y += 0.1;
pos = recanonicalize_pos(&world, pos);  // Again!

pos.tile_rel_offset_x -= 0.05;
pos = recanonicalize_pos(&world, pos);  // Third time!
```

**✅ BETTER:**

```c
// Accumulate all changes
pos.tile_rel_offset_x += 0.1;
pos.tile_rel_offset_y += 0.1;
pos.tile_rel_offset_x -= 0.05;

// Recanonicalize once at the end
pos = recanonicalize_pos(&world, pos);  // Only once! ✅
```

**Performance tip:** Recanonicalizing is NOT free. Do it strategically:

- ✅ After player movement
- ✅ Before collision checks
- ✅ Before rendering
- ❌ NOT in tight inner loops

---

### Mistake #7: Not Validating Input Data

**Loading from file:**

```c
// ❌ BAD: Trust file data
WorldCanonicalPosition pos;
fread(&pos, sizeof(pos), 1, file);
// What if file is corrupted? pos might be non-canonical!

// ✅ GOOD: Validate and fix
fread(&pos, sizeof(pos), 1, file);
pos = recanonicalize_pos(&world, pos);  // Ensure canonical
if (!is_canonical(&world, pos)) {
    fprintf(stderr, "ERROR: Invalid position in save file!\n");
    pos = get_default_spawn_pos(&world);
}
```

---

### Mistake #8: Forgetting Negative Coordinates

**Only testing positive movement:**

```c
// ❌ Tests only positive movement
pos.tile_rel_offset_x = 1.3;
pos.tile_rel_offset_x += 0.3;  // → 1.6, overflows to next tile ✅

// But forgot to test negative!
pos.tile_rel_offset_x = 0.2;
pos.tile_rel_offset_x -= 0.5;  // → -0.3, should wrap to previous tile!
```

**Always test BOTH directions:**

```c
void test_recanonicalizing() {
    // Test positive overflow
    WorldCanonicalPosition pos = {.tile_x = 5, .tile_rel_offset_x = 1.3};
    pos.tile_rel_offset_x += 0.3;
    pos = recanonicalize_pos(&world, pos);
    assert(pos.tile_x == 6 && pos.tile_rel_offset_x < 0.4);

    // Test negative underflow
    pos = (WorldCanonicalPosition){.tile_x = 5, .tile_rel_offset_x = 0.2};
    pos.tile_rel_offset_x -= 0.5;
    pos = recanonicalize_pos(&world, pos);
    assert(pos.tile_x == 4 && pos.tile_rel_offset_x > 1.0);

    printf("✅ Recanonicalization tests passed!\n");
}
```

---

### Debugging Tips

**Tip #1: Always print canonical position**

```c
void print_canonical_pos(WorldCanonicalPosition pos) {
  printf("Canonical Position:\n");
  printf("  Tilemap: (%d, %d)\n", pos.tilemap_x, pos.tilemap_y);
  printf("  Tile: (%d, %d)\n", pos.tile_x, pos.tile_y);
  printf("  Offset: (%.3f, %.3f) meters\n",
         pos.tile_rel_offset_x, pos.tile_rel_offset_y);
}

// Usage:
if (stuck_in_wall) {
    printf("👥 Player stuck!\n");
    print_canonical_pos(player.world_canonical_pos);
}
```

**Tip #2: Check if position is canonical**

```c
bool is_canonical(World *world, WorldCanonicalPosition pos) {
  if (pos.tile_rel_offset_x < 0 ||
      pos.tile_rel_offset_x > world->tile_side_in_meters) {
    printf("❌ tile_rel_offset_x out of range: %.3f\n", pos.tile_rel_offset_x);
    return false;
  }

  if (pos.tile_x < 0 ||
      pos.tile_x >= world->tiles_per_map_x_count) {
    printf("❌ tile_x out of range: %d\n", pos.tile_x);
    return false;
  }

  // All checks pass
  return true;
}
```

**Tip #3: Visualize on screen**

```c
// Draw debug text
char debug_text[256];
snprintf(debug_text, sizeof(debug_text),
         "TM(%d,%d) T(%d,%d) Off(%.2f,%.2f)",
         player.world_canonical_pos.tilemap_x,
         player.world_canonical_pos.tilemap_y,
         player.world_canonical_pos.tile_x,
         player.world_canonical_pos.tile_y,
         player.world_canonical_pos.tile_rel_offset_x,
         player.world_canonical_pos.tile_rel_offset_y);
draw_text(canvas, 10, 10, debug_text);
```

**Tip #4: Log tile transitions**

```c
void update_player_position(Player *player, World *world, float dx, float dy) {
    WorldCanonicalPosition old_pos = player->world_canonical_pos;

    player->world_canonical_pos.tile_rel_offset_x += dx;
    player->world_canonical_pos.tile_rel_offset_y += dy;
    player->world_canonical_pos = recanonicalize_pos(world, player->world_canonical_pos);

    // Log transitions
    if (old_pos.tile_x != player->world_canonical_pos.tile_x ||
        old_pos.tile_y != player->world_canonical_pos.tile_y) {
        printf("➡️  Tile transition: (%d,%d) → (%d,%d)\n",
               old_pos.tile_x, old_pos.tile_y,
               player->world_canonical_pos.tile_x,
               player->world_canonical_pos.tile_y);
    }
}
```

**Tip #5: Unit test critical paths**

```c
void test_edge_cases() {
    World world = {
        .tile_side_in_meters = 1.4f,
        .tiles_per_map_x_count = 17,
    };

    // Test 1: Exact tile boundary
    WorldCanonicalPosition pos = {.tile_x = 5, .tile_rel_offset_x = 1.4f};
    pos = recanonicalize_pos(&world, pos);
    assert(pos.tile_x == 6 && pos.tile_rel_offset_x < 0.01f);

    // Test 2: Zero offset
    pos = (WorldCanonicalPosition){.tile_x = 5, .tile_rel_offset_x = 0.0f};
    pos = recanonicalize_pos(&world, pos);
    assert(pos.tile_x == 5 && pos.tile_rel_offset_x == 0.0f);

    // Test 3: Large overflow
    pos = (WorldCanonicalPosition){.tile_x = 5, .tile_rel_offset_x = 10.0f};
    pos = recanonicalize_pos(&world, pos);
    assert(pos.tile_x == 12);  // 5 + floor(10/1.4) = 5 + 7 = 12

    // Test 4: Tilemap boundary
    pos = (WorldCanonicalPosition){.tile_x = 16, .tile_rel_offset_x = 1.5f};
    pos = recanonicalize_pos(&world, pos);
    assert(pos.tilemap_x == 1 && pos.tile_x == 0);

    printf("✅ All edge case tests passed!\n");
}
```

---

<a name="exercises"></a>

## 10. Exercises

Time to solidify your understanding! Do these in order.

### Exercise 1: Manual Recanonicalizing (Paper & Pen)

Given:

```c
World world = {
  .tile_side_in_meters = 1.4,
  .tiles_per_map_x_count = 17,
};
```

Recanonicalize these positions BY HAND:

**A)**

```c
WorldCanonicalPosition pos = {
  .tilemap_x = 0,
  .tile_x = 5,
  .tile_rel_offset_x = 2.1,  // > 1.4, overflow!
};
```

What are the final values of `tilemap_x`, `tile_x`, `tile_rel_offset_x`?

**B)**

```c
WorldCanonicalPosition pos = {
  .tilemap_x = 0,
  .tile_x = 5,
  .tile_rel_offset_x = -0.7,  // < 0, underflow!
};
```

**C)**

```c
WorldCanonicalPosition pos = {
  .tilemap_x = 0,
  .tile_x = 16,  // Last tile
  .tile_rel_offset_x = 1.5,  // Overflow into next tilemap!
};
```

<details>
<summary>Solutions</summary>

**A)**

```
offset = floor(2.1 / 1.4) = floor(1.5) = 1
tile_x = 5 + 1 = 6
tile_rel_offset_x = 2.1 - (1 * 1.4) = 2.1 - 1.4 = 0.7

tile_x = 6 < 17, no tilemap change

Answer:
  tilemap_x = 0
  tile_x = 6
  tile_rel_offset_x = 0.7
```

**B)**

```
offset = floor(-0.7 / 1.4) = floor(-0.5) = -1
tile_x = 5 + (-1) = 4
tile_rel_offset_x = -0.7 - (-1 * 1.4) = -0.7 + 1.4 = 0.7

tile_x = 4 >= 0, no tilemap change

Answer:
  tilemap_x = 0
  tile_x = 4
  tile_rel_offset_x = 0.7
```

**C)**

```
offset = floor(1.5 / 1.4) = floor(1.071) = 1
tile_x = 16 + 1 = 17
tile_rel_offset_x = 1.5 - 1.4 = 0.1

tile_x = 17 >= 17, overflow!
tile_x = 17 - 17 = 0
tilemap_x = 0 + 1 = 1

Answer:
  tilemap_x = 1
  tile_x = 0
  tile_rel_offset_x = 0.1
```

</details>

---

### Exercise 2: Implement is_canonical() Check

Write a function that verifies if a position is canonical:

```c
bool is_canonical(World *world, WorldCanonicalPosition pos) {
  // Your code here
  // Return true if all invariants are satisfied
  // Return false otherwise
}
```

Test cases:

```c
// Should return true
WorldCanonicalPosition good = {
  .tilemap_x = 0, .tilemap_y = 0,
  .tile_x = 5, .tile_y = 3,
  .tile_rel_offset_x = 0.7, .tile_rel_offset_y = 0.3,
};

// Should return false (offset too large)
WorldCanonicalPosition bad1 = {
  .tilemap_x = 0, .tilemap_y = 0,
  .tile_x = 5, .tile_y = 3,
  .tile_rel_offset_x = 2.0,  // > tile_side_in_meters
  .tile_rel_offset_y = 0.3,
};

// Should return false (tile index out of range)
WorldCanonicalPosition bad2 = {
  .tilemap_x = 0, .tilemap_y = 0,
  .tile_x = 20,  // >= tiles_per_map_x_count
  .tile_y = 3,
  .tile_rel_offset_x = 0.7, .tile_rel_offset_y = 0.3,
};
```

<details>
<summary>Solution</summary>

```c
bool is_canonical(World *world, WorldCanonicalPosition pos) {
  // Check tile_rel_offset bounds
  if (pos.tile_rel_offset_x < 0.0f ||
      pos.tile_rel_offset_x > world->tile_side_in_meters) {
    return false;
  }
  if (pos.tile_rel_offset_y < 0.0f ||
      pos.tile_rel_offset_y > world->tile_side_in_meters) {
    return false;
  }

  // Check tile index bounds
  if (pos.tile_x < 0 || pos.tile_x >= world->tiles_per_map_x_count) {
    return false;
  }
  if (pos.tile_y < 0 || pos.tile_y >= world->tiles_per_map_y_count) {
    return false;
  }

  // Tilemap indices can be any integer (no bounds check needed)

  return true;
}
```

</details>

---

### Exercise 3: Convert Pixel Position to Canonical

Write a function that converts an old-style pixel position to canonical:

```c
WorldCanonicalPosition pixels_to_canonical(World *world,
                                           float pixel_x,
                                           float pixel_y) {
  // Your code here
  // Assume pixels are relative to tilemap (0,0) origin
}
```

Test case:

```c
World world = {
  .tile_side_in_px = 60,
  .tile_side_in_meters = 1.4,
  .tiles_per_map_x_count = 17,
};

// Pixel position (200, 150)
WorldCanonicalPosition pos = pixels_to_canonical(&world, 200, 150);

// Should output:
//   tilemap_x = 0, tilemap_y = 0
//   tile_x = 3 (200 / 60 = 3.333 → 3)
//   tile_y = 2 (150 / 60 = 2.5 → 2)
//   tile_rel_offset_x = 0.466... meters
//     (200 - 3*60 = 40 pixels, 40/42.857 = 0.933 meters... wait recalculate)
```

<details>
<summary>Solution</summary>

```c
WorldCanonicalPosition pixels_to_canonical(World *world,
                                           float pixel_x,
                                           float pixel_y) {
  WorldCanonicalPosition pos = {0};

  // Start at tilemap (0,0)
  pos.tilemap_x = 0;
  pos.tilemap_y = 0;

  // Calculate tile indices
  pos.tile_x = (i32)(pixel_x / world->tile_side_in_px);
  pos.tile_y = (i32)(pixel_y / world->tile_side_in_px);

  // Calculate pixel offset within tile
  float pixel_offset_x = pixel_x - (pos.tile_x * world->tile_side_in_px);
  float pixel_offset_y = pixel_y - (pos.tile_y * world->tile_side_in_px);

  // Convert pixels to meters
  float pixels_to_meters = world->tile_side_in_meters / world->tile_side_in_px;
  pos.tile_rel_offset_x = pixel_offset_x * pixels_to_meters;
  pos.tile_rel_offset_y = pixel_offset_y * pixels_to_meters;

  // Recanonicalize (in case tile indices overflowed tilemap)
  pos = recanonicalize_pos(world, pos);

  return pos;
}
```

</details>

---

### Exercise 4: Calculate Distance Between Positions

Write a function that calculates the distance (in meters) between two canonical positions:

```c
float distance_between(World *world,
                       WorldCanonicalPosition pos1,
                       WorldCanonicalPosition pos2) {
  // Your code here
  // Hint: Convert both to a common coordinate system first
}
```

**Challenge:** Handle positions in different tilemaps!

<details>
<summary>Solution</summary>

```c
float distance_between(World *world,
                       WorldCanonicalPosition pos1,
                       WorldCanonicalPosition pos2) {
  // Convert both positions to meters from world origin

  // pos1 in meters
  float meters1_x =
      pos1.tilemap_x * (world->tiles_per_map_x_count * world->tile_side_in_meters) +
      pos1.tile_x * world->tile_side_in_meters +
      pos1.tile_rel_offset_x;

  float meters1_y =
      pos1.tilemap_y * (world->tiles_per_map_y_count * world->tile_side_in_meters) +
      pos1.tile_y * world->tile_side_in_meters +
      pos1.tile_rel_offset_y;

  // pos2 in meters
  float meters2_x =
      pos2.tilemap_x * (world->tiles_per_map_x_count * world->tile_side_in_meters) +
      pos2.tile_x * world->tile_side_in_meters +
      pos2.tile_rel_offset_x;

  float meters2_y =
      pos2.tilemap_y * (world->tiles_per_map_y_count * world->tile_side_in_meters) +
      pos2.tile_y * world->tile_side_in_meters +
      pos2.tile_rel_offset_y;

  // Euclidean distance
  float dx = meters2_x - meters1_x;
  float dy = meters2_y - meters1_y;

  return sqrtf(dx * dx + dy * dy);
}
```

</details>

---

### Exercise 5: Implement Teleport

Write a function that teleports the player to a specific tile:

```c
void teleport_player(Player *player, World *world,
                     i32 tilemap_x, i32 tilemap_y,
                     i32 tile_x, i32 tile_y) {
  // Your code here
  // Place player at center of the specified tile
}
```

Hint: Center of tile means `tile_rel_offset = tile_side_in_meters / 2`

<details>
<summary>Solution</summary>

```c
void teleport_player(Player *player, World *world,
                     i32 tilemap_x, i32 tilemap_y,
                     i32 tile_x, i32 tile_y) {
  player->world_canonical_pos = (WorldCanonicalPosition){
    .tilemap_x = tilemap_x,
    .tilemap_y = tilemap_y,
    .tile_x = tile_x,
    .tile_y = tile_y,
    .tile_rel_offset_x = world->tile_side_in_meters * 0.5f,
    .tile_rel_offset_y = world->tile_side_in_meters * 0.5f,
  };

  // Ensure it's canonical (in case tile indices are out of range)
  player->world_canonical_pos =
      recanonicalize_pos(world, player->world_canonical_pos);
}
```

</details>

---

### Exercise 6: Fix The Initial Position Bug

**THE BUG:** In your init.c, you're setting:

```c
.tile_rel_offset_x = 5.0f,
.tile_rel_offset_y = 5.0f,
```

But `tile_side_in_meters = 1.4`, so 5.0 will overflow multiple tiles!

**YOUR TASK:**

1. Calculate what the ACTUAL canonical position should be after recanonicalizing
2. Fix the init code to set the correct values directly
3. Add a recanonicalize call after initialization as a safety check

**Expected Result:**

```c
// Instead of:
.tile_x = 3,
.tile_rel_offset_x = 5.0f,

// Should be:
.tile_x = 6,         // 3 + 3 tiles overflowed
.tile_rel_offset_x = 0.8f,  // 5.0 - (3 * 1.4) = 0.8
```

**Why This Matters:** Initializing with canonical values avoids a recanonicalize call at startup. More importantly, it forces you to THINK in canonical coordinates from the start!

<details>
<summary>Solution</summary>

```c
GAME_INIT(game_init) {
  // ... world setup ...

  f32 player_height = 1.4f;

  // ✅ CORRECTED: Initialize with canonical values
  game->player = (Player){
    .color_r = 0.0f,
    .color_g = 1.0f,
    .color_b = 1.0f,
    .color_a = 1.0f,
    .height = player_height,
    .width = 0.75f * player_height,
    .world_canonical_pos = (WorldCanonicalPosition){
      .tilemap_x = 0,
      .tilemap_y = 0,
      .tile_x = 6,                     // ✅ Corrected from 3
      .tile_y = 6,                     // ✅ Corrected from 3
      .tile_rel_offset_x = 0.8f,       // ✅ Corrected from 5.0
      .tile_rel_offset_y = 0.8f,       // ✅ Corrected from 5.0
    },
  };

  // Safety check (should not change anything if done correctly)
  DEV_ASSERT(is_canonical(&game->world, game->player.world_canonical_pos));

  // Or just recanonicalize to be safe:
  game->player.world_canonical_pos =
      recanonicalize_pos(&game->world, game->player.world_canonical_pos);
}
```

**Alternative: Position player at a meaningful location**

```c
// Start player at center of tile (3, 3) in tilemap (0, 0)
.world_canonical_pos = (WorldCanonicalPosition){
  .tilemap_x = 0,
  .tilemap_y = 0,
  .tile_x = 3,
  .tile_y = 3,
  .tile_rel_offset_x = game->world.tile_side_in_meters * 0.5f,  // 0.7m (center)
  .tile_rel_offset_y = game->world.tile_side_in_meters * 0.5f,  // 0.7m (center)
},
```

</details>

---

### Exercise 7: Debug Visualization (Build It!)

Implement a debug overlay that shows you EXACTLY where the player is.

**YOUR TASK:** Add this to your drawing code:

```c
void draw_coordinate_debug(Canvas *canvas, World *world, Player *player) {
  // 1. Draw the current tile's bounds as a rectangle
  // 2. Draw text showing canonical position
  // 3. Draw a crosshair at the tile origin
  // 4. Draw tilemap boundaries
}
```

**Specific Implementation Requirements:**

**Part A: Highlight Current Tile**

```c
// Calculate screen position of tile origin
f32 tile_screen_x = /* your code */;
f32 tile_screen_y = /* your code */;

// Draw tile bounds in green
draw_rect_outline(canvas,
                  tile_screen_x,
                  tile_screen_y,
                  world->tile_side_in_px,
                  world->tile_side_in_px,
                  0.0f, 1.0f, 0.0f, 1.0f);  // Green
```

**Part B: Draw Debug Text**

```c
char debug_text[256];
snprintf(debug_text, sizeof(debug_text),
         "TM(%d,%d) T(%d,%d) Off(%.2f,%.2f)",
         player->world_canonical_pos.tilemap_x,
         player->world_canonical_pos.tilemap_y,
         player->world_canonical_pos.tile_x,
         player->world_canonical_pos.tile_y,
         player->world_canonical_pos.tile_rel_offset_x,
         player->world_canonical_pos.tile_rel_offset_y);

draw_text(canvas, 10, 10, debug_text, /* color */);
```

**Part C: Draw Grid Lines**

```c
// Draw all tile boundaries in the visible area
for (int tile_y = 0; tile_y < world->tiles_per_map_y_count; tile_y++) {
  for (int tile_x = 0; tile_x < world->tiles_per_map_x_count; tile_x++) {
    // Calculate screen pos and draw grid
  }
}
```

**Why This Matters:**

- You'll SEE tile transitions happen in real-time
- You'll catch bugs where canonical position is wrong
- You'll understand the coordinate system viscerally, not just theoretically

<details>
<summary>Complete Solution</summary>

```c
void draw_coordinate_debug(Canvas *canvas, World *world, Player *player) {
  WorldCanonicalPosition *pos = &player->world_canonical_pos;

  // ─── 1. Highlight current tile ───
  f32 tile_screen_x = pos->tilemap_x * (world->tiles_per_map_x_count * world->tile_side_in_px) +
                      pos->tile_x * world->tile_side_in_px;
  f32 tile_screen_y = pos->tilemap_y * (world->tiles_per_map_y_count * world->tile_side_in_px) +
                      pos->tile_y * world->tile_side_in_px;

  // Draw tile outline (green)
  draw_rect_outline(canvas, tile_screen_x, tile_screen_y,
                    world->tile_side_in_px, world->tile_side_in_px,
                    0.0f, 1.0f, 0.0f, 1.0f);

  // ─── 2. Draw crosshair at tile origin ───
  draw_line(canvas, tile_screen_x - 5, tile_screen_y,
            tile_screen_x + 5, tile_screen_y,
            1.0f, 0.0f, 0.0f, 1.0f);  // Red
  draw_line(canvas, tile_screen_x, tile_screen_y - 5,
            tile_screen_x, tile_screen_y + 5,
            1.0f, 0.0f, 0.0f, 1.0f);  // Red

  // ─── 3. Draw grid for visible tiles ───
  // (Optional: only draw if you have many tiles visible)
  for (int ty = 0; ty < world->tiles_per_map_y_count; ty++) {
    for (int tx = 0; tx < world->tiles_per_map_x_count; tx++) {
      f32 x = tx * world->tile_side_in_px;
      f32 y = ty * world->tile_side_in_px;

      draw_rect_outline(canvas, x, y,
                        world->tile_side_in_px,
                        world->tile_side_in_px,
                        0.3f, 0.3f, 0.3f, 0.5f);  // Gray, semi-transparent
    }
  }

  // ─── 4. Draw tilemap boundaries (thick lines) ───
  f32 tilemap_width_px = world->tiles_per_map_x_count * world->tile_side_in_px;
  f32 tilemap_height_px = world->tiles_per_map_y_count * world->tile_side_in_px;

  draw_rect_outline(canvas, 0, 0,
                    tilemap_width_px, tilemap_height_px,
                    1.0f, 1.0f, 0.0f, 1.0f);  // Yellow (current tilemap)

  // ─── 5. Debug text overlay ───
  char debug_lines[512];
  snprintf(debug_lines, sizeof(debug_lines),
           "Position:\n"
           "  Tilemap: (%d, %d)\n"
           "  Tile: (%d, %d)\n"
           "  Offset: (%.3f, %.3f) meters\n"
           "  Offset: (%.1f, %.1f) px within tile\n"
           "  Speed: %.1f m/s",
           pos->tilemap_x, pos->tilemap_y,
           pos->tile_x, pos->tile_y,
           pos->tile_rel_offset_x, pos->tile_rel_offset_y,
           pos->tile_rel_offset_x * world->meters_to_pixels,
           pos->tile_rel_offset_y * world->meters_to_pixels,
           player->speed);

  draw_text(canvas, 10, 10, debug_lines, 1.0f, 1.0f, 1.0f, 1.0f);
}
```

**To use it:**

```c
// In your main rendering function
draw_player(canvas, &game->world, &game->player);
draw_coordinate_debug(canvas, &game->world, &game->player);  // Add this
```

**Optional: Toggle with a key**

```c
static bool show_debug = false;

if (input->key_F3.pressed) {
  show_debug = !show_debug;
}

if (show_debug) {
  draw_coordinate_debug(canvas, &game->world, &game->player);
}
```

</details>

---

### Exercise 8: Test Tilemap Boundary Collision

**THE PROBLEM:** Your collision detection checks 3 points (left, center, right foot). But what if:

- Left foot is in tilemap (0,0) tile (16, 5) — edge of tilemap
- Right foot overflows into tilemap (1,0) tile (0, 5) — different tilemap!

Does your collision detection work across tilemap boundaries?

**YOUR TASK:**

1. Create a test scenario where player straddles a tilemap boundary
2. Place a wall on one side of the boundary
3. Verify collision works correctly

**Test Setup:**

```c
// Place player near right edge of tilemap (0,0)
player.world_canonical_pos = (WorldCanonicalPosition){
  .tilemap_x = 0,
  .tilemap_y = 0,
  .tile_x = 16,  // Last tile
  .tile_y = 5,
  .tile_rel_offset_x = 1.3f,  // Near right edge (tile is 1.4m wide)
  .tile_rel_offset_y = 0.7f,
};

// Player width is 1.05m
// At offset 1.3m, left edge is at: 1.3 - 0.525 = 0.775m ✅ In tile 16
// Right edge is at: 1.3 + 0.525 = 1.825m ❌ OVERFLOWS into tilemap (1,0)!

// Make tilemap(1,0), tile(0,5) a WALL
tilemaps[1][0].tiles[5 * 17 + 0] = 1;  // Solid wall

// When player moves right, should they collide?
```

**Expected Behavior:**

- Player should NOT be able to walk through the tilemap boundary into the wall
- Collision detection must work across tilemap transitions

**Why This Matters:** This is a REAL bug that can happen! If your collision only checks the current tilemap, entities can phase through walls at boundaries.

<details>
<summary>Solution & Explanation</summary>

Your CURRENT code already handles this correctly! Let's trace why:

```c
// When player moves right:
WorldCanonicalPosition player_bottom_right_pos = player_bottom_center_pos;
player_bottom_right_pos.tile_rel_offset_x += game->player.width * 0.5f;

// Before recanonicalize:
//   tilemap_x = 0, tile_x = 16, offset_x = 1.3 + 0.525 = 1.825

player_bottom_right_pos = recanonicalize_pos(&world, player_bottom_right_pos);

// After recanonicalize:
//   offset = floor(1.825 / 1.4) = 1
//   tile_x = 16 + 1 = 17
//   offset_x = 1.825 - 1.4 = 0.425
//
//   tile_x = 17 >= 17, so:
//   tile_x = 17 - 17 = 0
//   tilemap_x = 0 + 1 = 1
//
// Final: tilemap_x=1, tile_x=0, offset_x=0.425 ✅

// Now collision check:
if (is_world_point_empty(&game->world, player_bottom_right_pos)) {
  // This checks tilemap[1][0].tiles[5][0]
  // Which is our WALL!
  // Returns false, collision detected! ✅
}
```

**The key:** `recanonicalize_pos()` AUTOMATICALLY handles tilemap transitions, so your collision detection "just works" across boundaries!

**To test this manually:**

1. Add this test function:

```c
void test_tilemap_boundary_collision(World *world) {
  printf("\n=== Testing Tilemap Boundary Collision ===\n");

  // Set up position near boundary
  WorldCanonicalPosition pos = {
    .tilemap_x = 0,
    .tilemap_y = 0,
    .tile_x = 16,
    .tile_y = 5,
    .tile_rel_offset_x = 1.3f,
    .tile_rel_offset_y = 0.7f,
  };

  printf("Starting position: TM(%d,%d) T(%d,%d) Off(%.2f,%.2f)\n",
         pos.tilemap_x, pos.tilemap_y, pos.tile_x, pos.tile_y,
         pos.tile_rel_offset_x, pos.tile_rel_offset_y);

  // Offset by player width
  pos.tile_rel_offset_x += 0.525f;  // Half of 1.05m width

  printf("After offset: TM(%d,%d) T(%d,%d) Off(%.2f,%.2f)\n",
         pos.tilemap_x, pos.tilemap_y, pos.tile_x, pos.tile_y,
         pos.tile_rel_offset_x, pos.tile_rel_offset_y);

  // Recanonicalize
  pos = recanonicalize_pos(world, pos);

  printf("After recanon: TM(%d,%d) T(%d,%d) Off(%.2f,%.2f)\n",
         pos.tilemap_x, pos.tilemap_y, pos.tile_x, pos.tile_y,
         pos.tile_rel_offset_x, pos.tile_rel_offset_y);

  // Check collision
  bool is_empty = is_world_point_empty(world, pos);
  printf("Tile is %s\n", is_empty ? "EMPTY" : "SOLID");

  printf("=== Test Complete ===\n\n");
}
```

2. Call it from init:

```c
GAME_INIT(game_init) {
  // ... setup code ...

  #ifdef HANDMADE_HERO_DEV
  test_tilemap_boundary_collision(&game->world);
  #endif
}
```

3. Run and verify output

</details>

---

### Exercise 9: Implement Camera Follow

**YOUR TASK:** Make the camera follow the player smoothly.

Currently, all drawing is relative to world origin (0,0). Add a camera that:

1. Centers on the player
2. Smoothly interpolates (no jerky movement)
3. Stays within world bounds (doesn't show empty space)

**Data Structure:**

```c
typedef struct {
  WorldCanonicalPosition target_pos;  // Where camera wants to be
  f32 screen_x;                       // Current camera position (pixels)
  f32 screen_y;
  f32 smoothing;                      // 0.0 = instant, 1.0 = very smooth
} Camera;
```

**Implementation Steps:**

**Part A: Convert canonical position to screen pixels**

```c
f32 canonical_to_screen_x(World *world, WorldCanonicalPosition pos) {
  // Your code here
  // Convert the full canonical position to a flat pixel coordinate
}
```

**Part B: Update camera each frame**

```c
void update_camera(Camera *camera, World *world,
                   WorldCanonicalPosition player_pos, f32 dt) {
  // Calculate target position (player position)
  f32 target_x = canonical_to_screen_x(world, player_pos);
  f32 target_y = canonical_to_screen_y(world, player_pos);

  // Smooth interpolation (lerp)
  // current = current + (target - current) * smoothing * dt

  // Your code here
}
```

**Part C: Apply camera offset to all drawing**

```c
void draw_player(Canvas *canvas, World *world, Player *player, Camera *camera) {
  f32 screen_x = canonical_to_screen_x(world, player->world_canonical_pos);
  f32 screen_y = canonical_to_screen_y(world, player->world_canonical_pos);

  // Apply camera offset
  screen_x -= camera->screen_x;
  screen_y -= camera->screen_y;

  // Center on screen
  screen_x += canvas->width / 2;
  screen_y += canvas->height / 2;

  // Draw...
}
```

**Why This Matters:** Camera control is ESSENTIAL for games. You'll use canonical positions to figure out where to center the camera, and you'll see how the coordinate system makes this straightforward.

<details>
<summary>Solution</summary>

```c
// ─── Data structure (add to game state) ───
typedef struct {
  f32 x;          // Camera position in pixels (flat world space)
  f32 y;
  f32 smoothing;  // 0.1 = slow, 10.0 = fast
} Camera;

// ─── Conversion function ───
f32 canonical_to_flat_x(World *world, WorldCanonicalPosition pos) {
  return pos.tilemap_x * (world->tiles_per_map_x_count * world->tile_side_in_px) +
         pos.tile_x * world->tile_side_in_px +
         pos.tile_rel_offset_x * world->meters_to_pixels;
}

f32 canonical_to_flat_y(World *world, WorldCanonicalPosition pos) {
  return pos.tilemap_y * (world->tiles_per_map_y_count * world->tile_side_in_px) +
         pos.tile_y * world->tile_side_in_px +
         pos.tile_rel_offset_y * world->meters_to_pixels;
}

// ─── Update camera (call every frame) ───
void update_camera(Camera *camera, World *world,
                   WorldCanonicalPosition player_pos, f32 dt) {
  // Target = player position
  f32 target_x = canonical_to_flat_x(world, player_pos);
  f32 target_y = canonical_to_flat_y(world, player_pos);

  // Smooth lerp: move toward target
  f32 blend = 1.0f - powf(0.01f, camera->smoothing * dt);
  camera->x += (target_x - camera->x) * blend;
  camera->y += (target_y - camera->y) * blend;
}

// ─── Apply camera to drawing ───
void draw_player(Canvas *canvas, World *world, Player *player, Camera *camera) {
  // Get player's flat world position
  f32 world_x = canonical_to_flat_x(world, player->world_canonical_pos);
  f32 world_y = canonical_to_flat_y(world, player->world_canonical_pos);

  // Convert to screen space (relative to camera)
  f32 screen_x = world_x - camera->x + (canvas->width / 2);
  f32 screen_y = world_y - camera->y + (canvas->height / 2);

  // Draw
  f32 player_width_px = player->width * world->meters_to_pixels;
  f32 player_height_px = player->height * world->meters_to_pixels;

  draw_rect(canvas,
            screen_x - player_width_px * 0.5f,
            screen_y - player_height_px,
            player_width_px, player_height_px,
            player->color_r, player->color_g, player->color_b, player->color_a);
}

// ─── Init camera ───
GAME_INIT(game_init) {
  // ... existing init ...

  game->camera = (Camera){
    .x = canonical_to_flat_x(&game->world, game->player.world_canonical_pos),
    .y = canonical_to_flat_y(&game->world, game->player.world_canonical_pos),
    .smoothing = 5.0f,  // Tune this value
  };
}

// ─── Update loop ───
GAME_UPDATE_AND_RENDER(game_update_and_render) {
  // ... handle controls ...

  // Update camera
  update_camera(&game->camera, &game->world,
                game->player.world_canonical_pos, frame_time);

  // ... drawing ...
}
```

**Alternative: Instant camera (no smoothing)**

```c
camera->x = canonical_to_flat_x(world, player_pos);
camera->y = canonical_to_flat_y(world, player_pos);
```

**Tuning smoothing:**

- `smoothing = 100.0f` → instant (no delay)
- `smoothing = 5.0f` → medium (slight delay)
- `smoothing = 1.0f` → slow (floaty feeling)

</details>

---

### Exercise 10: The Ultimate Test - Warp Gates

Implement a "warp gate" system that teleports the player between distant locations.

**Requirements:**

1. Place two warp gates on the map (different tilemaps)
2. When player enters gate A, teleport to gate B
3. Vice versa
4. Add a cooldown so player doesn't instantly warp back
5. Show a visual effect

**Data Structure:**

```c
typedef struct {
  WorldCanonicalPosition position;
  WorldCanonicalPosition destination;
  f32 radius;  // Activation distance (in meters)
  f32 color_r, color_g, color_b;
} WarpGate;
```

**Implementation Steps:**

**Part A: Check if player is near a gate**

```c
void check_warp_gates(Player *player, WarpGate *gates, int gate_count,
                      f32 *warp_cooldown, f32 dt) {
  // Decrease cooldown
  if (*warp_cooldown > 0) {
    *warp_cooldown -= dt;
    return;
  }

  // Check each gate
  for (int i = 0; i < gate_count; i++) {
    f32 dist = distance_between(world, player->world_canonical_pos,
                                gates[i].position);

    if (dist < gates[i].radius) {
      // WARP!
      // Your code here
    }
  }
}
```

**Part B: Draw the gates**

```c
void draw_warp_gate(Canvas *canvas, World *world, WarpGate *gate, Camera *camera) {
  // Convert canonical position to screen
  // Draw a circle at that position
  // Add particle effects (optional!)
}
```

**Why This Matters:**

- Tests your understanding of canonical positions
- Tests distance calculations across tilemaps
- Shows how coordinate system enables complex gameplay
- You're building a REAL game feature!

<details>
<summary>Complete Solution</summary>

```c
// ─── Data structures ───
typedef struct {
  WorldCanonicalPosition position;
  WorldCanonicalPosition destination;
  f32 radius;  // meters
  f32 color_r, color_g, color_b;
} WarpGate;

// ─── Init gates (in game_init) ───
game->warp_gates[0] = (WarpGate){
  .position = (WorldCanonicalPosition){
    .tilemap_x = 0, .tilemap_y = 0,
    .tile_x = 8, .tile_y = 4,
    .tile_rel_offset_x = 0.7f, .tile_rel_offset_y = 0.7f,
  },
  .destination = (WorldCanonicalPosition){
    .tilemap_x = 1, .tilemap_y = 0,
    .tile_x = 8, .tile_y = 4,
    .tile_rel_offset_x = 0.7f, .tile_rel_offset_y = 0.7f,
  },
  .radius = 1.0f,  // 1 meter activation radius
  .color_r = 0.0f, .color_g = 0.5f, .color_b = 1.0f,  // Blue
};

game->warp_gates[1] = (WarpGate){
  .position = game->warp_gates[0].destination,  // Swap
  .destination = game->warp_gates[0].position,
  .radius = 1.0f,
  .color_r = 1.0f, .color_g = 0.5f, .color_b = 0.0f,  // Orange
};

game->warp_gate_count = 2;
game->warp_cooldown = 0.0f;

// ─── Check for warp (in update loop) ───
void check_warp_gates(Player *player, World *world,
                     WarpGate *gates, int gate_count,
                     f32 *warp_cooldown, f32 dt) {
  // Cooldown timer
  if (*warp_cooldown > 0) {
    *warp_cooldown -= dt;
    return;
  }

  // Check each gate
  for (int i = 0; i < gate_count; i++) {
    f32 dist = distance_between(world,
                               player->world_canonical_pos,
                               gates[i].position);

    if (dist < gates[i].radius) {
      // WARP!
      printf("🌀 Warping! Distance: %.2fm\n", dist);

      player->world_canonical_pos = gates[i].destination;

      // Ensure it's canonical (should already be, but safety first!)
      player->world_canonical_pos =
          recanonicalize_pos(world, player->world_canonical_pos);

      // Set cooldown (prevent instant return warp)
      *warp_cooldown = 1.0f;  // 1 second cooldown

      break;  // Only warp once per frame
    }
  }
}

// ─── Draw gates ───
void draw_warp_gates(Canvas *canvas, World *world,
                    WarpGate *gates, int count, Camera *camera) {
  for (int i = 0; i < count; i++) {
    WarpGate *gate = &gates[i];

    // Convert position to screen
    f32 world_x = canonical_to_flat_x(world, gate->position);
    f32 world_y = canonical_to_flat_y(world, gate->position);

    f32 screen_x = world_x - camera->x + (canvas->width / 2);
    f32 screen_y = world_y - camera->y + (canvas->height / 2);

    // Draw circle
    f32 radius_px = gate->radius * world->meters_to_pixels;
    draw_circle(canvas, screen_x, screen_y, radius_px,
                gate->color_r, gate->color_g, gate->color_b, 0.5f);

    // Draw glow effect (optional)
    draw_circle(canvas, screen_x, screen_y, radius_px * 1.5f,
                gate->color_r, gate->color_g, gate->color_b, 0.2f);
  }
}

// ─── In main update loop ───
GAME_UPDATE_AND_RENDER(game_update_and_render) {
  // ... handle controls ...

  // Check warp gates
  check_warp_gates(&game->player, &game->world,
                  game->warp_gates, game->warp_gate_count,
                  &game->warp_cooldown, frame_time);

  // ... update camera ...

  // Draw
  draw_warp_gates(canvas, &game->world,
                 game->warp_gates, game->warp_gate_count,
                 &game->camera);
  draw_player(canvas, &game->world, &game->player, &game->camera);
}
```

**Enhancements to try:**

1. Add particle effects when warping
2. Play a sound effect
3. Add a "charging" animation before warp
4. Make gates one-way (remove the return gate)
5. Add more gates in a network

**Testing:**

1. Place gates in different tilemaps
2. Walk through gate
3. Verify you appear at destination
4. Verify cooldown prevents instant return
5. Check debug overlay to confirm new canonical position

</details>

---

## 🎓 Conclusion

You now understand:

✅ **Why** hierarchical coordinates exist (precision, chunking, mental models)  
✅ **What** canonical means (invariants that must hold true)  
✅ **How** recanonicalizing works (step-by-step math)  
✅ **When** to recanonicalize (after any position change)

### Casey's Parting Wisdom

> "Coordinate systems are just compression. You're taking a huge world
> and compressing it into a small, understandable representation.
>
> If you can't explain your coordinate system in 30 seconds,
> it's too complex. Simplify."

### What's Next?

**Day 33+:** Casey will add more features on top of this foundation:

- Entities (enemies, items) with canonical positions
- Camera system (smooth scrolling, look-ahead)
- Collision shapes (not just tile-based)
- Z-layers (entities at different heights)

But now you have the FOUNDATION. You can build the rest yourself!

---

## 📚 Quick Reference

### Canonical Position Invariants

```c
// MUST ALWAYS BE TRUE:
0 <= tile_rel_offset_x <= tile_side_in_meters
0 <= tile_rel_offset_y <= tile_side_in_meters

0 <= tile_x < tiles_per_map_x_count
0 <= tile_y < tiles_per_map_y_count

tilemap_x, tilemap_y can be ANY integer
```

### When to Recanonicalize

```c
// ✅ YES, recanonicalize after:
pos.tile_rel_offset_x += velocity * dt;  // Movement
pos = recanonicalize_pos(&world, pos);

// ✅ YES, recanonicalize after:
pos.tile_rel_offset_x -= player_width * 0.5f;  // Offset calculation
pos = recanonicalize_pos(&world, pos);

// ❌ NO, don't need to recanonicalize:
pos.tilemap_x = 0;  // Direct assignment to canonical values
pos.tile_x = 5;
pos.tile_rel_offset_x = 0.5f;  // (if you KNOW it's valid)
```

### Conversion Cheat Sheet

```c
// Meters → Pixels
pixels = meters * world->meters_to_pixels;

// Pixels → Meters
meters = pixels / world->meters_to_pixels;

// Canonical → Flat meters (for distance calculations)
meters_x = tilemap_x * (tiles_per_map_x * tile_side_in_meters) +
           tile_x * tile_side_in_meters +
           tile_rel_offset_x;

// Canonical → Screen pixels (for drawing)
screen_x = tilemap_x * (tiles_per_map_x * tile_side_in_px) +
           tile_x * tile_side_in_px +
           tile_rel_offset_x * meters_to_pixels;
```

---

**Now go forth and build! And remember Casey's golden rule:**

> "When in doubt, ASSERT. Better to crash with a helpful message
> than continue with corrupted state."

---

**Questions? Stuck? Re-read the section you're confused about.**  
**Still stuck? Work through the exercises BY HAND.**  
**STILL stuck? Ask me specific questions!**

You've got this! 🚀

---

## 🎯 Your Roadmap to Mastery

### Phase 1: Understanding (Do This NOW - 30 minutes)

**Tasks:**

1. ✅ Read Sections 1-3 (Problem → Screen vs World)
2. ✅ Do the float precision test (Section 1)
3. ✅ Try the "Which tile am I on?" exercise (Section 2)
4. ✅ Trace World→Screen conversion (Section 3)

**Goal:** Understand WHY we need hierarchical coordinates.

**You'll know you're ready when:** You can explain float precision problems to a friend.

---

### Phase 2: Mental Models (Do This TODAY - 1 hour)

**Tasks:**

1. ✅ Read Sections 4-6 (Hierarchical → New System)
2. ✅ Do Exercise 1 on paper (manual recanonicalizing - 3 examples)
3. ✅ Answer the "Mental Math Practice" questions in Section 4
4. ✅ Complete the "Spot The Non-Canonical" quiz in Section 6

**Goal:** Hold the coordinate system "in your head."

**You'll know you're ready when:** You can recanonicalize positions without a computer.

---

### Phase 3: Fixing Your Code (Do This WEEK - 2 hours)

**Tasks:**

1. ✅ Read Section 7 (How Recanonicalizing Works)
2. ✅ Read Section 9 (Common Mistakes - ALL of them!)
3. ✅ Implement Exercise 2 (is_canonical checker)
4. ✅ Fix Exercise 6 (your init.c bug)
5. ✅ Add DEV_ASSERT calls after every position modification
6. ✅ Run your game - fix any crashes

**Goal:** Your code runs without coordinate bugs.

**You'll know you're ready when:** No asserts fire, player moves smoothly across tiles.

---

### Phase 4: Visualization (Do This MONTH - 3 hours)

**Tasks:**

1. ✅ Read Section 8 (Complete Code Walkthrough)
2. ✅ Implement Exercise 7 (full debug visualization)
3. ✅ Test Exercise 8 (tilemap boundary collision)
4. ✅ Watch your debug overlay as you move
5. ✅ Verify tile transitions are smooth

**Goal:** SEE the coordinate system working in real-time.

**You'll know you're ready when:** You can watch tiles change and understand what's happening.

---

### Phase 5: Advanced Features (Do This ANYTIME - ongoing)

**Tasks:**

1. ✅ Implement Exercise 4 (distance_between)
2. ✅ Implement Exercise 5 (teleport)
3. ✅ Implement Exercise 9 (camera follow)
4. ✅ Implement Exercise 10 (warp gates - capstone!)

**Goal:** Build real game features using canonical coordinates.

**You'll know you're ready when:** You can add new position-based features confidently.

---

## 🏆 Mastery Checklist

### Level 1: Novice → Apprentice

- [ ] Can explain why floats lose precision
- [ ] Can identify coordinate system bugs in code
- [ ] Knows when to call recanonicalize_pos()
- [ ] Has fixed the common mistakes in their code

**How to level up:** Do Phases 1-3 above.

---

### Level 2: Apprentice → Competent

- [ ] Can recanonicalize positions by hand
- [ ] Can convert between different coordinate systems
- [ ] Has implemented debug visualization
- [ ] Can debug coordinate bugs in < 10 minutes

**How to level up:** Do Phase 4, implement all debug tools.

---

### Level 3: Competent → Proficient

- [ ] Can implement new position-based features
- [ ] Anticipates edge cases (tile boundaries, tilemap transitions)
- [ ] Writes unit tests for coordinate functions
- [ ] Optimizes by reducing recanonicalize calls

**How to level up:** Do Phase 5, build warp gates and camera system.

---

### Level 4: Proficient → Expert

- [ ] Can design coordinate systems for new games
- [ ] Can explain trade-offs (hierarchical vs fixed-point vs double)
- [ ] Can teach others about coordinate systems
- [ ] Spots coordinate issues in other people's code

**How to level up:** Help others, write your own coordinate system from scratch.

---

### Level 5: Expert → Master

- [ ] Has shipped a game using hierarchical coordinates
- [ ] Can implement in any language (C, C++, Rust, Zig)
- [ ] Knows when NOT to use hierarchical coords
- [ ] Contributes to game engine design discussions

**You've made it!** 🎖️

---

## 💡 Casey's Final Wisdom

> "The coordinate system is the FOUNDATION of your game.
> Get it wrong, and everything built on top will be shaky.
> Get it right, and complex features become simple.
>
> This is compression-oriented programming:
> Take a complex problem (infinite world, float precision)
> and compress it into a simple, understandable system.
>
> If you understand WHY each piece exists,
> you can rebuild it from scratch.
> That's mastery."

> "Don't just follow the code. UNDERSTAND the code.
> When you understand it, you own it.
> When you own it, you can change it.
> When you can change it, you can build anything."

---

## 📖 Further Reading

### Casey Muratori's Handmade Hero

- **Day 30:** Moving Between Tile Maps - First introduction
- **Day 31:** Unified Position Representation - Canonical form
- **Day 32:** Trying Out Actual Game Code - Your commit!
- **Day 33:** Entity Movement in Camera Space - Next steps

### Related Concepts

**Fixed-Point Arithmetic:**

- Alternative to floats (used in older games like Doom)
- Integer math with implicit decimal point
- Trade-offs: Limited range but perfect precision

**Spatial Partitioning:**

- Quadtrees (2D) / Octrees (3D) - Related chunking
- Grid-based partitioning (what tilemaps use)
- Bounding Volume Hierarchies (BVH) for collision

**Real-World Examples:**

- **Minecraft:** 16×16 block chunks, exactly like tilemaps!
- **Terraria:** Tile-based world with chunking
- **Factorio:** Huge maps using hierarchical coords
- **Dwarf Fortress:** Z-levels are like tilemap layers

### Deep Dives

**IEEE 754 Floating Point:**

- Sign bit, exponent, mantissa representation
- Why precision decreases with magnitude
- Special values (NaN, infinity, denormals)

**Compression-Oriented Programming:**

- Casey's philosophy applied to all code
- Data-oriented design principles
- Memory layouts for cache efficiency

---

## 🚀 What to Do RIGHT NOW

### If you have 5 minutes:

1. Close this document
2. Open your code
3. Find the line: `.tile_rel_offset_x = 5.0f,`
4. Fix it to: `.tile_rel_offset_x = 0.8f,` and `.tile_x = 6,`
5. Run your game - verify player spawns correctly

### If you have 15 minutes:

1. Do Exercise 1 on paper (3 recanonicalizing examples)
2. Implement Exercise 2 (is_canonical checker)
3. Add DEV_ASSERT after every position change
4. Test your game

### If you have 1 hour:

1. Read Sections 7-9 carefully
2. Implement Exercise 7 (debug visualization)
3. Play your game with F3 overlay on
4. Watch tile transitions happen

### If you have 3 hours:

1. Work through ALL 10 exercises
2. Implement debug tools
3. Build warp gates (Exercise 10)
4. Celebrate - you've mastered coordinates! 🎉

---

## ⚠️ Common Pitfalls to Avoid

### Pitfall #1: "I'll understand it later"

❌ **DON'T** skip ahead without understanding each section.  
✅ **DO** work through examples by hand until they make sense.

### Pitfall #2: "I'll just copy the code"

❌ **DON'T** copy-paste without understanding WHY.  
✅ **DO** type it yourself and explain each line out loud.

### Pitfall #3: "I don't need exercises"

❌ **DON'T** skip exercises thinking you get it.  
✅ **DO** at least Exercises 1, 2, 6, 7 - they catch real bugs!

### Pitfall #4: "This is too much"

❌ **DON'T** try to learn it all in one sitting.  
✅ **DO** take breaks. Come back when fresh.

### Pitfall #5: "I'll optimize first"

❌ **DON'T** worry about performance until it works.  
✅ **DO** make it correct first, THEN optimize.

---

## 🆘 When You Get Stuck

### Problem: "I don't understand recanonicalizing"

**Solution:**

1. Re-read Section 7 (How Recanonicalizing Works)
2. Do Example 2 from Section 7 with pencil and paper
3. Draw the tile grid and walk through step-by-step
4. Use the debug printf statements from Section 9

### Problem: "My player teleports randomly"

**Solution:**

1. You forgot to recanonicalize - see Mistake #1 in Section 9
2. Add DEV_ASSERT(is_canonical(...)) after EVERY position change
3. Check the debug visualization (Exercise 7)
4. Look for places where you modify tile_rel_offset

### Problem: "I get assert failures"

**Solution:**

1. Print the position when assert fires (Section 9, Tip #1)
2. Check if you're mixing meters and pixels (Mistake #2)
3. Verify tile_rel_offset range (Mistake #3)
4. Check for negative coordinates (Mistake #8)

### Problem: "The course is too long"

**Solution:**

1. You DON'T have to read it all at once
2. Start with Sections 1-3, then take a break
3. Come back later for Sections 4-6
4. Do exercises over several days
5. Use the Quick Reference section as a reminder

### Problem: "I want more examples"

**Solution:**

1. The 10 exercises ARE the examples
2. Build them incrementally
3. Start with Exercise 1 (paper), then 2 (code), then 6 (fix bug)
4. Each exercise teaches something new

---

## 🎁 Bonus: One-Page Cheat Sheet

### Core Data Structure

```c
typedef struct {
    i32 tilemap_x, tilemap_y;     // Which chunk (any int)
    i32 tile_x, tile_y;            // Which tile (0 to tiles_per_map-1)
    f32 tile_rel_offset_x, _y;    // Offset in tile (0 to tile_side_in_meters)
} WorldCanonicalPosition;
```

### Invariants (Must ALWAYS Be True)

```c
0 <= tile_rel_offset_x < tile_side_in_meters
0 <= tile_x < tiles_per_map_x_count
tilemap_x = any integer
```

### Key Function

```c
pos.tile_rel_offset_x += velocity * dt;
pos = recanonicalize_pos(&world, pos);  // ALWAYS call after modifying!
```

### Common Conversions

```c
// Meters ↔ Pixels
pixels = meters * world->meters_to_pixels;
meters = pixels / world->meters_to_pixels;

// Canonical → Flat Meters
total_meters = tilemap_x * (tiles_per_map_x * tile_side_in_meters)
             + tile_x * tile_side_in_meters
             + tile_rel_offset_x;

// Distance Between Positions
dx = pos1_flat_meters_x - pos2_flat_meters_x;
dist = sqrtf(dx*dx + dy*dy);
```

### Common Bugs

1. ❌ Forgot to recanonicalize → Teleporting player
2. ❌ Mixed meters and pixels → Wrong collision detection
3. ❌ Assumed offset is 0-1 → Off-by-one errors
4. ❌ Didn't test negative movement → Underflow bugs

### Quick Debug

```c
#define DEBUG_POS(p) printf("TM(%d,%d) T(%d,%d) Off(%.2f,%.2f)\n", \
    p.tilemap_x, p.tilemap_y, p.tile_x, p.tile_y, \
    p.tile_rel_offset_x, p.tile_rel_offset_y)
```

---

**Print this page and keep it next to your desk!**

---

_Last updated: Enhanced course with practical examples, anti-patterns, and debugging_  
_Document version: 3.0 - Comprehensive Practical Edition_  
_Total sections: 10 | Exercises: 10 | Anti-patterns: 6 | Bug scenarios: 5 | Debugging tips: 8_
