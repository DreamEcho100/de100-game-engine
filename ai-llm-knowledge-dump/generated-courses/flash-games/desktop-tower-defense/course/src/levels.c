/* src/levels.c  —  Desktop Tower Defense | Wave Definitions
 *
 * g_waves[0] = wave 1, g_waves[39] = wave 40.
 * base_hp_override = 0 means "use CREEP_DEFS[type].base_hp".
 */
#include "game.h"

const WaveDef g_waves[WAVE_COUNT] = {
    /* Wave 1  */ [0]  = { CREEP_NORMAL,   10, 0.8f,  0 },
    /* Wave 2  */ [1]  = { CREEP_NORMAL,   12, 0.75f, 0 },
    /* Wave 3  */ [2]  = { CREEP_NORMAL,   15, 0.7f,  0 },
    /* Wave 4  */ [3]  = { CREEP_FAST,     15, 0.5f,  0 },
    /* Wave 5  */ [4]  = { CREEP_NORMAL,   20, 0.7f,  0 },
    /* Wave 6  */ [5]  = { CREEP_NORMAL,   22, 0.65f, 0 },
    /* Wave 7  */ [6]  = { CREEP_FLYING,   10, 0.6f,  0 },
    /* Wave 8  */ [7]  = { CREEP_NORMAL,   25, 0.6f,  0 },
    /* Wave 9  */ [8]  = { CREEP_ARMOURED, 15, 0.7f,  0 },
    /* Wave 10 */ [9]  = { CREEP_BOSS,      1, 0.0f,  1000 },
    /* Wave 11 */ [10] = { CREEP_NORMAL,   20, 0.6f,  0 },
    /* Wave 12 */ [11] = { CREEP_FAST,     20, 0.5f,  0 },
    /* Wave 13 */ [12] = { CREEP_SPAWN,    10, 0.7f,  0 },
    /* Wave 14 */ [13] = { CREEP_FLYING,   20, 0.55f, 0 },
    /* Wave 15 */ [14] = { CREEP_BOSS,      1, 0.0f,  2000 },
    /* Wave 16 */ [15] = { CREEP_NORMAL,   30, 0.55f, 0 },
    /* Wave 17 */ [16] = { CREEP_ARMOURED, 20, 0.65f, 0 },
    /* Wave 18 */ [17] = { CREEP_FAST,     25, 0.45f, 0 },
    /* Wave 19 */ [18] = { CREEP_SPAWN,    15, 0.65f, 0 },
    /* Wave 20 */ [19] = { CREEP_BOSS,      1, 0.0f,  3000 },
    /* Wave 21 */ [20] = { CREEP_FLYING,   25, 0.5f,  0 },
    /* Wave 22 */ [21] = { CREEP_FAST,     30, 0.4f,  0 },
    /* Wave 23 */ [22] = { CREEP_ARMOURED, 25, 0.6f,  0 },
    /* Wave 24 */ [23] = { CREEP_NORMAL,   35, 0.5f,  0 },
    /* Wave 25 */ [24] = { CREEP_BOSS,      2, 0.0f,  3500 },
    /* Wave 26 */ [25] = { CREEP_SPAWN,    20, 0.6f,  0 },
    /* Wave 27 */ [26] = { CREEP_ARMOURED, 30, 0.55f, 0 },
    /* Wave 28 */ [27] = { CREEP_FLYING,   30, 0.45f, 0 },
    /* Wave 29 */ [28] = { CREEP_FAST,     35, 0.4f,  0 },
    /* Wave 30 */ [29] = { CREEP_BOSS,      1, 0.0f,  5000 },
    /* Wave 31 */ [30] = { CREEP_SPAWN,    25, 0.55f, 0 },
    /* Wave 32 */ [31] = { CREEP_ARMOURED, 35, 0.5f,  0 },
    /* Wave 33 */ [32] = { CREEP_FLYING,   35, 0.4f,  0 },
    /* Wave 34 */ [33] = { CREEP_FAST,     40, 0.35f, 0 },
    /* Wave 35 */ [34] = { CREEP_NORMAL,   50, 0.45f, 0 },
    /* Wave 36 */ [35] = { CREEP_SPAWN,    30, 0.5f,  0 },
    /* Wave 37 */ [36] = { CREEP_ARMOURED, 40, 0.45f, 0 },
    /* Wave 38 */ [37] = { CREEP_FLYING,   40, 0.35f, 0 },
    /* Wave 39 */ [38] = { CREEP_FAST,     50, 0.3f,  0 },
    /* Wave 40 */ [39] = { CREEP_BOSS,      1, 0.0f,  10000 }, /* Final boss */
};
