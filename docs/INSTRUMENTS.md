# Milestone 1 instrument bank

Six independent families accept MIDI simultaneously when enabled. Each has twelve
main voices and twelve short retirement slots for steals. Every instrument has an
independent enable and level. The temporary INSTRUMENTS page exposes six cards;
click a card's name for its six synthesis controls. ENVIRONMENT returns to the
legacy technical room view. This is functional navigation, not Milestone 4's
cinematic redesign.

All six instrument controls are normalised 0–1, continuously smoothed over 25 ms:

| Family | Controls and DSP mapping |
|---|---|
| Forgotten Piano | Age: dispersive modes, uneven tuning and slow pitch movement. Detune: separation of three strings. Softness: hammer harmonic rolloff with velocity. Mechanical Noise: short hammer noise. Decay: 0.6–9.6 s modal time scale. Distance: source attenuation (dedicated per-family distance filtering is pending Milestone 3). |
| Faded Synth | Detune: paired additive saw oscillators. Warmth: low-pass cutoff and mild saturation. Drift: slow oscillator tuning movement. Attack: 0.005–3.005 s. Release: 0.05–10.05 s. Width: note-dependent stereo spread. |
| Glass Memories | Fragility: excitation rise (soft to sharp). Brightness: high-mode amplitude. Resonance: relative upper-mode loss. Detune: mode pitch motion. Decay: 0.4–12.4 s mode time scale. Distance: source attenuation. |
| Empty Choir | Breath: noise and slower attack. Formant: two spectral formant peaks, approximately 350–1000 and 1100–2800 Hz. Detune: paired voiced harmonics. Width: stereo spread. Darkness: low-pass coloration. Release: 0.2–14.2 s. |
| Lost Music Box | Wear: key-dependent mistuning. Pitch Drift: slow pitch motion. Mechanism: short mechanical click. Brightness: inharmonic tine modes. Decay: 0.12–4.12 s. Distance: attenuation. |
| Hollow Drone | Density: slow onset and sustained spectral energy. Instability: independently wandering harmonic frequencies. Harmonics: partial levels and low-pass tone. Sub: sub-octave component. Movement: spectral and stereo evolution. Release: 0.3–25.3 s. |

Envelopes are exponential time constants, not times to exact silence. Release
terminates below -100 dB envelope gain; a long drone can therefore last much
longer than its displayed time constant. Acoustic tails continue separately.

MIDI note on/off, velocity, channel-specific CC64 sustain, two-semitone pitch bend,
CC123 all-notes-off and CC120 all-sound-off are handled at sample offsets. MIDI
channels share each family's voice pool, but bend, pedal and note-off matching are
channel-specific. No key-range editor, MPE or configurable bend range is provided.
New notes route to currently enabled families. Enabling another family while a
key is already held takes effect on the next note-on. Muting a family fades its
level; the oscillator/envelope states continue to avoid abrupt resets.

Instrument audio enters the existing common room and output chain. Independent
per-family effects and acoustically complete distance processing belong to
Milestone 3 and are not claimed here. Environment mode can play MIDI instruments
alongside continuous ambience. Instrument mode gates the legacy noise layers;
the new bank remains playable. The old eight-voice tonal bank remains in the
standalone DSP class for compatibility tests, but processor MIDI now routes to
the new families rather than doubling their sound with the old bank.

State version 2 appends 48 stable IDs; version-1 sessions/presets migrate missing
instrument controls to defaults. Existing old IDs remain unchanged. Scene A/B
still uses the old reduced morph: instrument configurations are saved in scene
slots but are not yet interpolated/applied by that morph. Expanded scene behavior
is explicitly Milestone 5 work.

This is original procedural synthesis, not commercial sample playback. The
piano and choir are deliberately simplified models; their musical realism must
be judged from dry listening renders, not from parameter labels or energy metrics.
Very rapid repeated stealing beyond the retirement pool can still truncate a
retiring voice. Maximum voices, oversampling and CPU reduction for quality modes
remain Milestone 6 work. Soft synth saturation is not oversampled. No claim of
fanless MacBook Air performance or GarageBand/Logic host validation is made yet.

Performance: model/filter/envelope coefficients refresh every 32 samples; MIDI
release invalidates cached coefficients immediately. Phases/envelopes advance
per sample and modal amplitudes interpolate. The 72-voice stress render improved
from 11.08 to 2.17 seconds per two seconds of audio on the cloud CPU. Maximum
layering is still not guaranteed real time; fanless Mac profiling remains required.
