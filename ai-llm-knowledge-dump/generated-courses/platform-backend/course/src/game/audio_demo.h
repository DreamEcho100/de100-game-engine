#ifndef GAME_AUDIO_DEMO_H
#define GAME_AUDIO_DEMO_H

/* ═══════════════════════════════════════════════════════════════════════════
 * game/audio_demo.h — Platform Foundation Course
 * ═══════════════════════════════════════════════════════════════════════════
 * Public interface for audio_demo.c.
 * Included by both backends' main.c.
 *
 * LESSON 14 — Remove this file (and audio_demo.c) when creating a game
 *             course; replace with game/audio.h + game/audio.c.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include "../utils/audio.h"

/* Initialize GameAudioState with the demo music sequencer. */
void game_audio_init_demo(GameAudioState *audio);

/* Synthesize `num_frames` PCM sample-frames into `buf`. Called by audio loop. */
void game_get_audio_samples(GameAudioState *audio, AudioOutputBuffer *buf, int num_frames);

/* Advance sequencer + frequency slides.  Called once per game frame. */
void game_audio_update(GameAudioState *audio, float dt_ms);

/* Trigger a one-shot sound effect by SoundID. */
void game_play_sound_at(GameAudioState *audio, SoundID id);

#endif /* GAME_AUDIO_DEMO_H */
