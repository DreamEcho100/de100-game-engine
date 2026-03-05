/* src/audio.c  —  Desktop Tower Defense | PCM Audio System
 *
 * All sounds are synthesized — no WAV files loaded.
 * Each SoundDef describes a tone by frequency, slide, duration, and volume.
 * game_get_audio_samples() fills the AudioOutputBuffer using phase accumulators.
 * Click prevention: each SoundInstance has a brief fade-out envelope.
 *   (Fade-in would require storing total_duration in SoundInstance.)
 *
 * Synthesis rules:
 *   • Sine wave for all SFX — smooth, no harmonic aliasing.
 *   • Trapezoidal envelope: 4 ms fade-out on every sound instance.
 *   • Background music: sine-wave drone at ~110 Hz, slowly frequency-drifting.
 *   • Phase accumulators NEVER reset between calls — discontinuity = click.
 *
 * JS analogy: game_get_audio_samples() ≈ AudioWorkletProcessor.process().
 * It is called by the platform far more often than game_audio_update()
 * (at 44100 Hz / 2048 spp ≈ ~21× per game frame).  Keep it allocation-free.
 */
#include "game.h"
#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------
 * Sound definition table — indexed by SfxId
 * frequency:     start Hz
 * frequency_end: end Hz (0 = constant pitch)
 * duration_ms:   milliseconds
 * volume:        0.0–1.0
 * -------------------------------------------------------------------------*/
static const SoundDef SOUND_DEFS[SFX_COUNT] = {
    /* SFX_TOWER_FIRE_PELLET  */ { 800.0f,  400.0f,  50.0f,  0.3f },
    /* SFX_TOWER_FIRE_SQUIRT  */ { 300.0f,  200.0f,  80.0f,  0.4f },
    /* SFX_TOWER_FIRE_DART    */ { 600.0f,  900.0f,  40.0f,  0.4f },
    /* SFX_TOWER_FIRE_SNAP    */ { 100.0f,   50.0f, 150.0f,  0.7f },
    /* SFX_TOWER_FIRE_SWARM   */ { 400.0f,  200.0f, 100.0f,  0.5f },
    /* SFX_FROST_PULSE        */ { 1200.0f, 600.0f, 120.0f,  0.3f },
    /* SFX_BASH_HIT           */ { 150.0f,   80.0f, 100.0f,  0.6f },
    /* SFX_CREEP_DEATH        */ { 500.0f,  200.0f,  80.0f,  0.4f },
    /* SFX_BOSS_DEATH         */ {  80.0f,   40.0f, 400.0f,  0.8f },
    /* SFX_LIFE_LOST          */ { 200.0f,  100.0f, 250.0f,  0.7f },
    /* SFX_TOWER_PLACE        */ { 900.0f,  700.0f,  60.0f,  0.3f },
    /* SFX_TOWER_SELL         */ { 700.0f,  900.0f,  80.0f,  0.3f },
    /* SFX_WAVE_COMPLETE      */ { 440.0f,  880.0f, 200.0f,  0.5f },
    /* SFX_GAME_OVER          */ { 300.0f,  100.0f, 500.0f,  0.6f },
    /* SFX_VICTORY            */ { 440.0f,  880.0f, 600.0f,  0.7f },
};

/* =========================================================================
 * game_audio_init  —  called once at startup
 * =========================================================================
 * Zero-initialise then set default volumes and the ambient music drone.
 * Must be called before any other audio function.
 */
void game_audio_init(GameAudioState *audio) {
    memset(audio, 0, sizeof(*audio));

    audio->master_volume = 0.8f;
    audio->sfx_volume    = 1.0f;
    audio->music_volume  = 0.4f;

    /* Deep ambient drone — 110 Hz (A2), low and unobtrusive */
    audio->music_tone.frequency     = 110.0f;
    audio->music_tone.target_volume = 0.3f;
    audio->music_tone.is_playing    = 1;
    /* current_volume starts at 0 and ramps up via game_audio_update() */
}

/* =========================================================================
 * game_play_sound / game_play_sound_at
 * =========================================================================
 * Trigger a one-shot SFX.  Finds a free slot in active_sounds[] and
 * initialises it from SOUND_DEFS[id].  If all slots are occupied the new
 * sound is dropped silently — brief SFX loss is preferable to eviction glitches.
 */
void game_play_sound(GameAudioState *audio, SfxId id) {
    game_play_sound_at(audio, id, 0.0f);
}

void game_play_sound_at(GameAudioState *audio, SfxId id, float pan) {
    if (id < 0 || id >= SFX_COUNT) return;

    /* Find a free (inactive) slot */
    int slot = -1;
    for (int i = 0; i < MAX_SIMULTANEOUS_SOUNDS; ++i) {
        if (!audio->active_sounds[i].active) { slot = i; break; }
    }
    if (slot < 0) return; /* pool full — drop new sound silently */

    const SoundDef *def = &SOUND_DEFS[id];
    SoundInstance  *s   = &audio->active_sounds[slot];

    s->phase          = 0.0f;
    s->frequency      = def->frequency;
    s->time_remaining = def->duration_ms / 1000.0f;
    s->volume         = def->volume * audio->sfx_volume * audio->master_volume;
    s->pan_position   = pan;
    s->active         = 1;

    /* Linear frequency slide: Hz/sample from start to end over full duration */
    if (def->frequency_end > 0.0f) {
        float duration_secs  = def->duration_ms / 1000.0f;
        float total_samples  = duration_secs * (float)AUDIO_SAMPLE_RATE;
        s->frequency_slide   = (def->frequency_end - def->frequency) / total_samples;
    } else {
        s->frequency_slide = 0.0f;
    }
}

/* =========================================================================
 * game_audio_update  —  called once per game frame (~60 Hz)
 * =========================================================================
 * Advances the ambient music tone:
 *   • Smoothly ramps current_volume toward target_volume (avoids mute clicks).
 *   • Wanders frequency every ~2 s for a living, shifting drone.
 */
void game_audio_update(GameAudioState *audio, float dt) {
    static float freq_timer = 0.0f;

    ToneGenerator *t = &audio->music_tone;

    /* Volume ramp — 0.1 × dt per frame: reaches target in ~1/0.1 = 10 s full range */
    float target = t->is_playing ? t->target_volume : 0.0f;
    float step   = 0.1f * dt;
    if (t->current_volume < target) {
        t->current_volume += step;
        if (t->current_volume > target) t->current_volume = target;
    } else if (t->current_volume > target) {
        t->current_volume -= step;
        if (t->current_volume < target) t->current_volume = target;
    }

    /* Ambient sequencer: drift frequency slightly every ~2 s */
    if (t->is_playing) {
        freq_timer += dt;
        if (freq_timer >= 2.0f) {
            freq_timer -= 2.0f;
            /* Cycle through a small set of harmonically related pitches (A2 area) */
            static const float freq_offsets[] = { 110.0f, 120.0f, 100.0f, 130.0f, 110.0f };
            static int freq_step = 0;
            freq_step       = (freq_step + 1) % 5;
            t->frequency    = freq_offsets[freq_step];
        }
    }
}

/* =========================================================================
 * game_get_audio_samples  —  hot path (called at audio-buffer rate, ~21×/frame)
 * =========================================================================
 * Fills out->samples with interleaved stereo int16 PCM.
 * out->sample_count is the number of stereo FRAMES; each frame = 2 int16_t.
 *
 * Envelope: 4 ms fade-out at end of each SFX prevents end-of-sound clicks.
 *
 * CRITICAL: phase accumulators are NEVER reset between calls.
 * Resetting phase mid-waveform creates a discontinuity → audible click.
 */
void game_get_audio_samples(GameState *state, AudioOutputBuffer *out) {
    GameAudioState *audio = &state->audio;

    /* Static phase for the background music drone — persists across calls */
    static float music_phase = 0.0f;

    float inv_sample_rate = 1.0f / (float)out->samples_per_second;
    float attack_samples  = 0.004f * (float)out->samples_per_second; /* 4 ms */

    for (int i = 0; i < out->sample_count; ++i) {
        float left  = 0.0f;
        float right = 0.0f;

        /* ── Sound effects (sine wave, trapezoidal envelope) ─────────────── */
        for (int si = 0; si < MAX_SIMULTANEOUS_SOUNDS; ++si) {
            SoundInstance *inst = &audio->active_sounds[si];
            if (!inst->active) continue;

            /* Fade-out: ramp amplitude to zero during the last 4 ms */
            float envelope = 1.0f;
            if (inst->time_remaining * (float)out->samples_per_second < attack_samples)
                envelope = inst->time_remaining * (float)out->samples_per_second / attack_samples;

            float sample = sinf(inst->phase * 2.0f * 3.14159f) * inst->volume * envelope;

            /* Linear stereo panning: -1 = full left, 0 = center, +1 = full right */
            float pan = CLAMP(inst->pan_position, -1.0f, 1.0f);
            left  += sample * (1.0f - MAX(0.0f,  pan));
            right += sample * (1.0f - MAX(0.0f, -pan));

            /* Advance oscillator — NEVER reset between samples or calls */
            inst->phase += inst->frequency * inv_sample_rate;
            if (inst->phase >= 1.0f) inst->phase -= 1.0f;

            /* Linear frequency slide toward frequency_end */
            inst->frequency += inst->frequency_slide;

            /* Tick lifetime and deactivate when sound expires */
            inst->time_remaining -= inv_sample_rate;
            if (inst->time_remaining <= 0.0f) inst->active = 0;
        }

        /* ── Background music drone (sine wave, current_volume already ramped) */
        ToneGenerator *t = &audio->music_tone;
        if (t->current_volume > 0.0001f && t->frequency > 0.0f) {
            float music_amp = sinf(music_phase * 2.0f * 3.14159f)
                              * t->current_volume
                              * audio->music_volume
                              * audio->master_volume;
            left  += music_amp;
            right += music_amp;

            music_phase += t->frequency * inv_sample_rate;
            if (music_phase >= 1.0f) music_phase -= 1.0f;
        }

        /* Clamp and convert to signed 16-bit */
        left  = CLAMP(left,  -1.0f, 1.0f);
        right = CLAMP(right, -1.0f, 1.0f);
        out->samples[i * 2]     = (int16_t)(left  * 32767.0f);
        out->samples[i * 2 + 1] = (int16_t)(right * 32767.0f);
    }
}
