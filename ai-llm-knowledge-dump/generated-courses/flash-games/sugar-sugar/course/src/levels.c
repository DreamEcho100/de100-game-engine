/*
 * levels.c  —  Sugar, Sugar | Level Definitions
 *
 * All 30 puzzle levels are defined here as a static read-only array.
 * No file I/O, no parser, no allocation.  The compiler copies them into
 * the binary's data segment.  Access is O(1) — just array indexing.
 *
 * Canvas is 640 × 480 pixels.  Origin (0,0) = top-left.
 *
 * Emitters: sugar pours from these points.
 * Cups:     the goal containers.  required_color == GRAIN_WHITE means
 *           any colour is accepted.
 * Filters:  coloured rectangles.  Grains passing through change colour.
 * Obstacles: solid rectangles baked into the line bitmap at load time.
 * Teleporters: paired circles.  Grains that touch A are warped to B
 *              (and vice-versa) with a short cooldown to prevent loops.
 */

#include "game.h"

/* Helper macros to make the level data readable */
#define EMITTER(px, py, rate)    { (px), (py), (rate), 0.0f }
#define CUP(cx,cy,cw,ch, col, req)  { (cx),(cy),(cw),(ch), (col),(req),0 }
#define FILTER(fx,fy,fw,fh, col)    { (fx),(fy),(fw),(fh), (col) }
#define OBS(ox,oy,ow,oh)            { (ox),(oy),(ow),(oh) }
#define TELE(ax,ay,bx,by,r)         { (ax),(ay),(bx),(by),(r) }

LevelDef g_levels[TOTAL_LEVELS] = {

/* ------------------------------------------------------------------ */
/* LEVEL 1  —  Straight drop.  One cup, dead centre.
   Just let the sugar fall.  No drawing required.                       */
/* ------------------------------------------------------------------ */
[0] = {
    .index         = 0,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 240) },
    .cup_count     = 1,
    .cups          = { CUP(278, 370, 85, 100, GRAIN_WHITE, 150) },
},

/* ------------------------------------------------------------------ */
/* LEVEL 2  —  Draw a slope to the right.
   Emitter is on the left; cup is on the right.                        */
/* ------------------------------------------------------------------ */
[1] = {
    .index         = 1,
    .emitter_count = 1,
    .emitters      = { EMITTER(60, 20, 240) },
    .cup_count     = 1,
    .cups          = { CUP(490, 370, 85, 100, GRAIN_WHITE, 150) },
},

/* ------------------------------------------------------------------ */
/* LEVEL 3  —  Two cups.  Split the stream.                            */
/* ------------------------------------------------------------------ */
[2] = {
    .index         = 2,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 270) },
    .cup_count     = 2,
    .cups          = {
        CUP( 60, 370, 85, 100, GRAIN_WHITE, 100),
        CUP(490, 370, 85, 100, GRAIN_WHITE, 100),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 4  —  Three cups.  Route sugar to each one.                   */
/* ------------------------------------------------------------------ */
[3] = {
    .index         = 3,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 300) },
    .cup_count     = 3,
    .cups          = {
        CUP( 50, 370, 80, 100, GRAIN_WHITE, 80),
        CUP(278, 370, 85, 100, GRAIN_WHITE, 80),
        CUP(505, 370, 80, 100, GRAIN_WHITE, 80),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 5  —  Horizontal shelf obstacle.  Route around it.            */
/* ------------------------------------------------------------------ */
[4] = {
    .index           = 4,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 240) },
    .cup_count       = 1,
    .cups            = { CUP(50, 370, 85, 100, GRAIN_WHITE, 150) },
    .obstacle_count  = 1,
    .obstacles       = { OBS(150, 180, 340, 14) },
},

/* ------------------------------------------------------------------ */
/* LEVEL 6  —  Two shelves at different heights.  Zigzag route.        */
/* ------------------------------------------------------------------ */
[5] = {
    .index           = 5,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 240) },
    .cup_count       = 1,
    .cups            = { CUP(490, 370, 85, 100, GRAIN_WHITE, 150) },
    .obstacle_count  = 2,
    .obstacles       = {
        OBS( 80, 150, 260, 12),
        OBS(300, 250, 260, 12),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 7  —  Two emitters, two cups.  Match each stream to a cup.   */
/* ------------------------------------------------------------------ */
[6] = {
    .index         = 6,
    .emitter_count = 2,
    .emitters      = { EMITTER(120, 20, 180), EMITTER(520, 20, 180) },
    .cup_count     = 2,
    .cups          = {
        CUP( 60, 370, 85, 100, GRAIN_WHITE, 120),
        CUP(490, 370, 85, 100, GRAIN_WHITE, 120),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 8  —  V-shaped obstacles.  Cup at the bottom of the V.       */
/* ------------------------------------------------------------------ */
[7] = {
    .index           = 7,
    .emitter_count   = 1,
    .emitters        = { EMITTER(180, 20, 270) },
    .cup_count       = 1,
    .cups            = { CUP(278, 370, 85, 100, GRAIN_WHITE, 160) },
    .obstacle_count  = 2,
    .obstacles       = {
        OBS( 80, 240, 180, 12),
        OBS(380, 240, 180, 12),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 9  —  First colour!  Red filter → red cup + white cup.       */
/* ------------------------------------------------------------------ */
[8] = {
    .index           = 8,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 240) },
    .cup_count       = 2,
    .cups            = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490, 370, 85, 100, GRAIN_WHITE,  100),
    },
    .filter_count    = 1,
    .filters         = { FILTER(60, 180, 100, 22, GRAIN_RED) },
},

/* ------------------------------------------------------------------ */
/* LEVEL 10  —  Two colours.  Red left, green right.                   */
/* ------------------------------------------------------------------ */
[9] = {
    .index           = 9,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 270) },
    .cup_count       = 2,
    .cups            = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490, 370, 85, 100, GRAIN_GREEN,  100),
    },
    .filter_count    = 2,
    .filters         = {
        FILTER( 60, 200, 100, 22, GRAIN_RED),
        FILTER(480, 200, 100, 22, GRAIN_GREEN),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 11  —  Orange cup needs an orange filter detour.              */
/* ------------------------------------------------------------------ */
[10] = {
    .index           = 10,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 240) },
    .cup_count       = 2,
    .cups            = {
        CUP(490, 370, 85, 100, GRAIN_ORANGE, 100),
        CUP( 60, 370, 85, 100, GRAIN_WHITE,   100),
    },
    .filter_count    = 1,
    .filters         = { FILTER(460, 130, 100, 22, GRAIN_ORANGE) },
    .obstacle_count  = 1,
    .obstacles       = { OBS(200, 220, 240, 12) },
},

/* ------------------------------------------------------------------ */
/* LEVEL 12  —  Three colours, three cups.                             */
/* ------------------------------------------------------------------ */
[11] = {
    .index           = 11,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 300) },
    .cup_count       = 3,
    .cups            = {
        CUP( 40, 370,  80, 100, GRAIN_RED,    80),
        CUP(278, 370,  85, 100, GRAIN_GREEN,   80),
        CUP(515, 370,  80, 100, GRAIN_ORANGE,  80),
    },
    .filter_count    = 3,
    .filters         = {
        FILTER( 40, 170, 90, 22, GRAIN_RED),
        FILTER(275, 170, 90, 22, GRAIN_GREEN),
        FILTER(510, 170, 90, 22, GRAIN_ORANGE),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 13  —  Gravity switch introduction.  Cup is above emitter.   */
/* ------------------------------------------------------------------ */
[12] = {
    .index              = 12,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 440, 240) },
    .cup_count          = 1,
    .cups               = { CUP(278, 30, 85, 80, GRAIN_WHITE, 150) },
    .has_gravity_switch = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 14  —  Gravity + two cups: one above, one below.             */
/* ------------------------------------------------------------------ */
[13] = {
    .index              = 13,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 240, 240) },
    .cup_count          = 2,
    .cups               = {
        CUP(278,  30, 85, 80, GRAIN_WHITE, 100),
        CUP(278, 370, 85, 80, GRAIN_WHITE, 100),
    },
    .has_gravity_switch = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 15  —  Gravity + colour.  Flip to pass through a high filter.*/
/* ------------------------------------------------------------------ */
[14] = {
    .index              = 14,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 240, 240) },
    .cup_count          = 2,
    .cups               = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490, 370, 85, 100, GRAIN_WHITE,  100),
    },
    .filter_count       = 1,
    .filters            = { FILTER(60, 80, 100, 22, GRAIN_RED) },
    .has_gravity_switch = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 16  —  Gravity + shelves maze.                                */
/* ------------------------------------------------------------------ */
[15] = {
    .index              = 15,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 20, 240) },
    .cup_count          = 1,
    .cups               = { CUP(278, 370, 85, 100, GRAIN_WHITE, 150) },
    .obstacle_count     = 3,
    .obstacles          = {
        OBS( 80, 150, 200, 12),
        OBS(360, 230, 200, 12),
        OBS( 80, 310, 200, 12),
    },
    .has_gravity_switch = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 17  —  Cyclic screen introduction.                            */
/*   Cup is near the top.  Let sugar fall through the bottom to loop.  */
/* ------------------------------------------------------------------ */
[16] = {
    .index         = 16,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 240) },
    .cup_count     = 1,
    .cups          = { CUP(490, 50, 85, 80, GRAIN_WHITE, 150) },
    .is_cyclic     = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 18  —  Cyclic + two cups: needs multiple loop passes.         */
/* ------------------------------------------------------------------ */
[17] = {
    .index         = 17,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 240) },
    .cup_count     = 2,
    .cups          = {
        CUP( 50,  50, 85, 80, GRAIN_WHITE, 100),
        CUP(490,  50, 85, 80, GRAIN_WHITE, 100),
    },
    .is_cyclic     = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 19  —  Cyclic + colour filter on loop path.                   */
/* ------------------------------------------------------------------ */
[18] = {
    .index         = 18,
    .emitter_count = 1,
    .emitters      = { EMITTER(320, 20, 240) },
    .cup_count     = 2,
    .cups          = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490,  50, 85,  80, GRAIN_WHITE,  100),
    },
    .filter_count  = 1,
    .filters       = { FILTER(60, 340, 100, 22, GRAIN_RED) },
    .is_cyclic     = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 20  —  Cyclic + obstacles + two colours.                      */
/* ------------------------------------------------------------------ */
[19] = {
    .index           = 19,
    .emitter_count   = 1,
    .emitters        = { EMITTER(320, 20, 270) },
    .cup_count       = 2,
    .cups            = {
        CUP( 60, 370, 85, 100, GRAIN_GREEN,  100),
        CUP(490,  50, 85,  80, GRAIN_RED,    100),
    },
    .filter_count    = 2,
    .filters         = {
        FILTER( 60, 340, 100, 22, GRAIN_GREEN),
        FILTER(480,  70, 100, 22, GRAIN_RED),
    },
    .obstacle_count  = 1,
    .obstacles       = { OBS(160, 200, 320, 12) },
    .is_cyclic       = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 21  —  Teleporter introduction.  Portal A top, portal B side. */
/* ------------------------------------------------------------------ */
[20] = {
    .index              = 20,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 20, 240) },
    .cup_count          = 1,
    .cups               = { CUP( 60, 370, 85, 100, GRAIN_WHITE, 150) },
    .teleporter_count   = 1,
    .teleporters        = { TELE(320, 150, 100, 350, 18) },
},

/* ------------------------------------------------------------------ */
/* LEVEL 22  —  Two teleporters, two cups.                             */
/* ------------------------------------------------------------------ */
[21] = {
    .index              = 21,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 20, 240) },
    .cup_count          = 2,
    .cups               = {
        CUP( 60, 370, 85, 100, GRAIN_WHITE, 100),
        CUP(490, 370, 85, 100, GRAIN_WHITE, 100),
    },
    .teleporter_count   = 2,
    .teleporters        = {
        TELE(160, 120,  80, 320, 16),
        TELE(480, 120, 560, 320, 16),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 23  —  Teleporter + colour.  Change colour mid-teleport path. */
/* ------------------------------------------------------------------ */
[22] = {
    .index              = 22,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 20, 240) },
    .cup_count          = 2,
    .cups               = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490, 370, 85, 100, GRAIN_WHITE,  100),
    },
    .filter_count       = 1,
    .filters            = { FILTER(60, 270, 100, 22, GRAIN_RED) },
    .teleporter_count   = 1,
    .teleporters        = { TELE(320, 150,  80, 240, 18) },
},

/* ------------------------------------------------------------------ */
/* LEVEL 24  —  Gravity + teleporter combo.                            */
/* ------------------------------------------------------------------ */
[23] = {
    .index              = 23,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 240, 240) },
    .cup_count          = 1,
    .cups               = { CUP(490, 370, 85, 100, GRAIN_WHITE, 150) },
    .teleporter_count   = 1,
    .teleporters        = { TELE(320, 380, 100,  80, 18) },
    .has_gravity_switch = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 25  —  Cyclic + teleporter.  Use looping to reach a portal.  */
/* ------------------------------------------------------------------ */
[24] = {
    .index              = 24,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 20, 240) },
    .cup_count          = 1,
    .cups               = { CUP( 60, 370, 85, 100, GRAIN_WHITE, 150) },
    .teleporter_count   = 1,
    .teleporters        = { TELE(490, 300, 100, 200, 18) },
    .is_cyclic          = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 26  —  All mechanics: gravity + cyclic + colour.              */
/* ------------------------------------------------------------------ */
[25] = {
    .index              = 25,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 240, 240) },
    .cup_count          = 2,
    .cups               = {
        CUP( 60, 370, 85, 100, GRAIN_RED,   100),
        CUP(490,  50, 85,  80, GRAIN_WHITE,  100),
    },
    .filter_count       = 1,
    .filters            = { FILTER(60, 340, 100, 22, GRAIN_RED) },
    .has_gravity_switch = 1,
    .is_cyclic          = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 27  —  All mechanics: teleporter + colour + gravity.          */
/* ------------------------------------------------------------------ */
[26] = {
    .index              = 26,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 20, 240) },
    .cup_count          = 3,
    .cups               = {
        CUP( 40, 370,  80, 100, GRAIN_RED,    80),
        CUP(278, 370,  85, 100, GRAIN_GREEN,   80),
        CUP(515, 370,  80, 100, GRAIN_WHITE,   80),
    },
    .filter_count       = 2,
    .filters            = {
        FILTER( 40, 200, 90, 22, GRAIN_RED),
        FILTER(280, 200, 90, 22, GRAIN_GREEN),
    },
    .teleporter_count   = 1,
    .teleporters        = { TELE(510, 150, 510, 280, 18) },
    .has_gravity_switch = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 28  —  Two emitters + all colours + obstacles.                */
/* ------------------------------------------------------------------ */
[27] = {
    .index           = 27,
    .emitter_count   = 2,
    .emitters        = { EMITTER(120, 20, 180), EMITTER(520, 20, 180) },
    .cup_count       = 4,
    .cups            = {
        CUP( 30, 370,  70, 100, GRAIN_RED,    70),
        CUP(180, 370,  70, 100, GRAIN_GREEN,   70),
        CUP(395, 370,  70, 100, GRAIN_ORANGE,  70),
        CUP(545, 370,  70, 100, GRAIN_WHITE,   70),
    },
    .filter_count    = 3,
    .filters         = {
        FILTER( 30, 180, 90, 22, GRAIN_RED),
        FILTER(180, 240, 90, 22, GRAIN_GREEN),
        FILTER(395, 180, 90, 22, GRAIN_ORANGE),
    },
    .obstacle_count  = 2,
    .obstacles       = {
        OBS( 80, 140, 200, 12),
        OBS(360, 140, 200, 12),
    },
},

/* ------------------------------------------------------------------ */
/* LEVEL 29  —  Grand puzzle: cyclic + portals + gravity + 3 colours. */
/* ------------------------------------------------------------------ */
[28] = {
    .index              = 28,
    .emitter_count      = 1,
    .emitters           = { EMITTER(320, 240, 240) },
    .cup_count          = 3,
    .cups               = {
        CUP( 40, 370,  80, 100, GRAIN_RED,    80),
        CUP(278,  30,  85,  80, GRAIN_WHITE,   80),
        CUP(515, 370,  80, 100, GRAIN_GREEN,   80),
    },
    .filter_count       = 2,
    .filters            = {
        FILTER( 40, 340, 90, 22, GRAIN_RED),
        FILTER(515, 340, 90, 22, GRAIN_GREEN),
    },
    .teleporter_count   = 1,
    .teleporters        = { TELE(100, 240, 540, 240, 20) },
    .has_gravity_switch = 1,
    .is_cyclic          = 1,
},

/* ------------------------------------------------------------------ */
/* LEVEL 30  —  The final challenge.  All mechanics, 4 cups.           */
/* ------------------------------------------------------------------ */
[29] = {
    .index              = 29,
    .emitter_count      = 2,
    .emitters           = { EMITTER(160, 20, 180), EMITTER(480, 20, 180) },
    .cup_count          = 4,
    .cups               = {
        CUP( 30, 370,  70, 100, GRAIN_RED,    70),
        CUP(180, 370,  70, 100, GRAIN_GREEN,   70),
        CUP(395, 370,  70, 100, GRAIN_ORANGE,  70),
        CUP(545,  30,  70,  70, GRAIN_WHITE,   70),
    },
    .filter_count       = 3,
    .filters            = {
        FILTER( 30, 180, 90, 22, GRAIN_RED),
        FILTER(180, 260, 90, 22, GRAIN_GREEN),
        FILTER(395, 180, 90, 22, GRAIN_ORANGE),
    },
    .teleporter_count   = 1,
    .teleporters        = { TELE(500, 200, 500, 380, 18) },
    .obstacle_count     = 2,
    .obstacles          = {
        OBS( 80, 140, 200, 12),
        OBS(360, 140, 200, 12),
    },
    .has_gravity_switch = 1,
    .is_cyclic          = 1,
},

}; /* end g_levels */
