# Flash Game Reverse Engineering & Planning Prompt

You are tasked with researching and constructing a comprehensive PLAN.md for reconstructing a legacy Flash game without access to its source code.

This is a technical reverse-engineering and systems-architecture exercise.

The output must be rigorous, implementation-oriented, and suitable for a low-level, data-oriented development approach (C-style architecture preferred).

---

## 1. Research & Documentation Phase

### 1.1 Game Discovery

- Identify the original developer and release year.
- Document platform history (Newgrounds, Kongregate, etc.).
- Collect gameplay footage (YouTube, archived Flash portals).
- Search forums for undocumented mechanics or edge cases.
- Identify sequels or spiritual successors.

Deliverable:

- Historical Overview Section

---

## 2. Mechanics Decomposition

### 2.1 Core Gameplay Loop

- Define the update loop structure.
- Identify win/loss conditions.
- Define player goals and feedback loops.

### 2.2 Input System

- Keyboard mapping
- Mouse interaction
- Continuous vs discrete input
- Buffering rules

### 2.3 Game State Machine

- Enumerate all global states.
- Enumerate all player states.
- Map legal transitions.
- Identify forbidden transitions.
- Define guard conditions.

Must include:

- Transition table
- Event-triggered transitions
- Timing-based transitions

### 2.4 Entity System

- Static objects
- Dynamic actors
- Projectiles
- Particles
- UI elements

Specify:

- Data layout (struct-of-arrays preferred)
- Memory lifetime rules
- Ownership model

---

## 3. Mathematics & Physics

- Coordinate system definition
- Unit scaling
- Collision detection type (AABB, SAT, circle, pixel-perfect)
- Integration method (Euler, semi-implicit, fixed timestep)
- Spatial partitioning needs

If physics-based:

- Force model
- Constraint resolution
- Restitution & friction handling

---

## 4. Rendering Architecture

- Camera model
- Parallax layers
- Sprite batching approach
- Animation state handling
- Draw order rules

---

## 5. Audio System

- SFX triggering rules
- Music looping
- State-based audio transitions

---

## 6. Asset Inventory

List:

- All sprite categories
- Animation sequences
- Sound effects
- UI assets
- Fonts
- Level data format
- Any other assets that are not covered by the previous categories

Try to install them if you can. or tell me on how to install them, and tell me if they are in a format that can be easily used or if they need conversion.

---

## 7. Modern Reconstruction Stack

Evaluate:

- C + SDL
- C++ + OpenGL
- Godot
- Unity
- WebGL + Canvas
- Custom engine

Recommend one stack and justify.

---

## 8. Performance Considerations

- Memory layout strategy
- Cache coherency
- Branch prediction concerns
- State transition cost
- Update ordering constraints

Explicitly avoid unnecessary abstraction layers.

---

## 9. Development Roadmap

Phase 1: Core Loop Skeleton  
Phase 2: Input & State Machine  
Phase 3: Movement & Physics  
Phase 4: Collision System  
Phase 5: Rendering  
Phase 6: Audio  
Phase 7: Level Design  
Phase 8: Polish

Each phase must define:

- Deliverables
- Testing checkpoints
- Risk areas

---

## 10. Risk Assessment

Identify:

- Hidden mechanics
- Physics instability
- Timing discrepancies
- Replay determinism issues

Provide mitigation strategies.

---

## 11. Testing Strategy

- Deterministic replay testing
- State transition assertions
- Collision edge-case tests
- Performance profiling plan

---

## 12. Output Format

The output must be a complete PLAN.md document with:

- Clear section headings
- Bullet lists where appropriate
- Technical specificity
- No vague language
- Actionable steps only
