/* src/sprites.h — Sprite sheet system with async loading
 *
 * Three modes of operation:
 *   1. Placeholder mode (atlas_path == NULL)  — colored rects + label text.
 *   2. Loading mode                            — placeholder while thread loads.
 *   3. Atlas mode (loaded == 1)               — nearest-neighbour blit from PNG.
 *
 * Quick-start to replace placeholders with real sprites:
 *   a) Drop a PNG atlas into course/assets/sprites/atlas.png
 *   b) Edit SPRITE_DEFS[] src_x/src_y/src_w/src_h to match your atlas layout
 *   c) Call sprites_init("assets/sprites/atlas.png") or sprites_load_async()
 *
 * On load failure: a magenta "MISSING" placeholder is drawn and the error is
 * logged to stderr so you can see exactly which file is absent.
 *
 * Asset replacement workflow → course/assets/sprites/README.md
 */
#ifndef DTD_SPRITES_H
#define DTD_SPRITES_H

#include "utils/backbuffer.h"

/* ─── Sprite IDs ────────────────────────────────────────────────────────── */
typedef enum {
    SPR_TOWER_PELLET = 0,
    SPR_TOWER_SQUIRT,
    SPR_TOWER_DART,
    SPR_TOWER_SNAP,
    SPR_TOWER_SWARM,
    SPR_TOWER_FROST,
    SPR_TOWER_BASH,
    SPR_CREEP_NORMAL,
    SPR_CREEP_FAST,
    SPR_CREEP_FLYING,
    SPR_CREEP_ARMOURED,
    SPR_CREEP_SPAWN,
    SPR_CREEP_BOSS,
    SPR_PROJECTILE,
    SPR_EXPLOSION,
    SPR_BACKGROUND_TILE_EVEN,
    SPR_BACKGROUND_TILE_ODD,
    SPR_MISSING,   /* magenta drawn on invalid IDs or atlas load failure */
    SPR_COUNT,
} SpriteId;

/* ─── Sprite definition ─────────────────────────────────────────────────── */
typedef struct {
    int src_x, src_y;           /* top-left pixel in sprite atlas */
    int src_w, src_h;           /* size in atlas                  */
    uint32_t placeholder_color; /* drawn when no atlas is loaded  */
    const char *label;          /* short text on placeholder      */
} SpriteDef;

/* ─── Atlas load state ──────────────────────────────────────────────────── */
typedef enum {
    SPRITE_LOAD_IDLE    = 0,  /* init not yet called             */
    SPRITE_LOAD_LOADING = 1,  /* background thread running       */
    SPRITE_LOAD_READY   = 2,  /* atlas pixels available          */
    SPRITE_LOAD_ERROR   = 3,  /* failed — error_msg has details  */
} SpriteLoadState;

/* ─── Atlas state ────────────────────────────────────────────────────────── */
typedef struct {
    uint32_t       *pixels;         /* NULL → placeholders in use     */
    int             width, height;
    int             loaded;         /* 1 when pixels is valid         */
    SpriteLoadState load_state;
    char            error_msg[256]; /* set on SPRITE_LOAD_ERROR       */
} SpriteAtlas;

extern SpriteAtlas       g_sprite_atlas;
extern const SpriteDef   SPRITE_DEFS[SPR_COUNT];

/* ─── API ────────────────────────────────────────────────────────────────── */

/* Synchronous: blocks until load completes.  NULL → placeholder mode. */
void sprites_init(const char *atlas_path);

/* Asynchronous: spawns a pthread and returns immediately.
 * Call sprites_is_ready() each frame to check completion. */
void sprites_load_async(const char *atlas_path);

/* Returns 1 once loading finished (success or failure). */
int  sprites_is_ready(void);

void sprites_shutdown(void);

void draw_sprite(Backbuffer *bb, SpriteId id,
                 int dst_x, int dst_y, int dst_w, int dst_h);

void draw_sprite_frame(Backbuffer *bb, SpriteId id, int frame,
                       int dst_x, int dst_y, int dst_w, int dst_h);

#endif /* DTD_SPRITES_H */
