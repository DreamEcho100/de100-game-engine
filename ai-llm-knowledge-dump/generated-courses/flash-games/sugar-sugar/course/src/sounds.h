/*
 * sounds.h  —  Sound and music ID constants
 *
 * These are the only sound IDs the game ever passes to platform_play_sound()
 * and platform_play_music().  Each platform backend either plays real audio
 * for them or leaves the functions as empty stubs.
 *
 * To add a new sound: add an enum constant here.  No changes to game.c.
 */

#ifndef SOUNDS_H
#define SOUNDS_H

/* Sound effects — passed to platform_play_sound() */
typedef enum {
    SOUND_CUP_FILL,       /* short chime when one cup reaches 100%    */
    SOUND_LEVEL_COMPLETE, /* fanfare when all cups are filled          */
    SOUND_COUNT
} SOUND_ID;

/* Music tracks — passed to platform_play_music() */
typedef enum {
    MUSIC_GAMEPLAY,       /* ambient loop during active levels         */
    MUSIC_COUNT
} MUSIC_ID;

#endif /* SOUNDS_H */
