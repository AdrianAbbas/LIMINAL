# LIMINAL — DIRECTION UPDATE 1
## Major Product Overhaul — Milestones 1–7

**Read this document in full before modifying the repository.**

This document overrides the original LIMINAL specification wherever they conflict. It is a major change in product direction, not a request for incremental cosmetic improvements.

The current LIMINAL build works as a basic environmental noise generator, but it does not yet fulfill the intended creative purpose.

Its primary problems are:

1. The interface is generic, visually empty, and does not evoke liminal spaces.
2. The plugin lacks proper playable musical instruments.
3. Environments sound insufficiently different.
4. Environment selection does not meaningfully transform an instrument's character.
5. Sound design relies too heavily on generic electrical noise, ventilation, and drones.
6. The environment-specific visual presentation is almost nonexistent.

The goal is to rebuild LIMINAL into a **cinematic environmental instrument workstation**, combining playable musical instruments, continuously evolving background ambience, environment-specific audio processing, and distinctive visual presentation.

Do not discard working code unnecessarily. Audit the existing implementation first, identify reusable components, and replace inadequate systems systematically.

**The most important priority is sound quality, followed by visual identity and usability.**

---

# GLOBAL DEVELOPMENT RULES

These rules apply to every milestone.

- Implement real functionality, not placeholder controls.
- Build and run available tests after each milestone.
- Never leave the repository intentionally broken.
- Commit completed milestones separately.
- Maintain a `DEVLOG.md` with progress, decisions, known problems, and next steps.
- Keep a milestone checklist at the beginning of the DEVLOG.
- Never claim an audio feature is complete because the UI control exists.
- Preserve AU/VST3 compatibility.
- GarageBand and Logic Pro are important target hosts.
- Maintain real-time audio safety.
- Optimize for a fanless Apple Silicon MacBook Air.
- Avoid unnecessary dependencies and excessive CPU consumption.
- Do not use copyrighted commercial instrument samples.
- Use original synthesis, physically inspired models, or properly licensed sample assets.
- Do not make destructive changes to unrelated files or repositories.
- If a feature cannot be finished, document it instead of pretending it works.
- Make reasonable implementation decisions without asking questions at every step.

After completing each milestone, produce a brief status report identifying what changed, what was tested, what remains unverified, and any known limitations.

Do not stop at planning. Implement the changes.

---

# MILESTONE 1 — COMPLETE MUSICAL INSTRUMENT ENGINE

**Priority: Critical**

LIMINAL must become a playable instrument, not simply a background ambience plugin.

Implement a proper polyphonic MIDI instrument system alongside the existing environmental audio engine.

The plugin must support MIDI input in GarageBand, Logic, and compatible VST3 hosts.

## 1A. Six instrument families

Implement six genuinely different musical instrument families.

### Instrument 1 — FORGOTTEN PIANO

An old, slightly detuned piano that feels distant, nostalgic, and unsettling.

Required characteristics:

- Hammer-like attack.
- Natural note decay.
- Velocity sensitivity.
- Polyphony.
- Adjustable detuning.
- Mechanical noise.
- Subtle pitch instability.
- Uneven note characteristics.
- Pedal-style sustain behavior where feasible.

Controls:

- Age
- Detune
- Softness
- Mechanical Noise
- Decay
- Distance

Do not simply use a sine wave with a decay envelope.

Use a convincing multistring/modal synthesis approach, or properly licensed samples combined with expressive processing.

### Instrument 2 — FADED SYNTH

A warm, nostalgic synthesizer reminiscent of deteriorated analog electronics.

Required characteristics:

- Multiple oscillators.
- Slightly unstable tuning.
- Low-pass filtering.
- Slow modulation.
- Warm saturation.
- Polyphonic envelopes.

Controls:

- Detune
- Warmth
- Drift
- Attack
- Release
- Width

It must support playable melodic lines and rich chords.

### Instrument 3 — GLASS MEMORIES

A delicate, fragile, bell-like instrument.

Required characteristics:

- Inharmonic partials.
- Resonant metallic or glass-like decay.
- Soft transient options.
- Adjustable pitch instability.
- Extended atmospheric tails.

Controls:

- Fragility
- Brightness
- Resonance
- Detune
- Decay
- Distance

This should work especially well for sparse horror melodies.

### Instrument 4 — EMPTY CHOIR

A synthetic, wordless vocal-like pad.

Required characteristics:

- Formant-inspired filtering.
- Soft vowel-like resonances.
- Slow envelopes.
- Detuned layered voices.
- Stereo movement.

Controls:

- Breath
- Formant
- Detune
- Width
- Darkness
- Release

No intelligible speech is required.

### Instrument 5 — LOST MUSIC BOX

A worn mechanical music-box instrument.

Required characteristics:

- Short metallic plucks.
- Unstable tuning.
- Mechanical clicks.
- Uneven note decay.
- Delicate upper harmonics.

Controls:

- Wear
- Pitch Drift
- Mechanism
- Brightness
- Decay
- Distance

### Instrument 6 — HOLLOW DRONE

A playable sustained atmospheric instrument for low notes, chords, and evolving pads.

Required characteristics:

- Multiple oscillators.
- Slow detuning.
- Resonant filtering.
- Subharmonics.
- Stereo modulation.
- Very long attack and release options.

Controls:

- Density
- Instability
- Harmonics
- Sub
- Movement
- Release

## 1B. Instrument architecture

All six instruments must support:

- MIDI note input.
- Note-on/note-off.
- Velocity.
- Polyphony appropriate to the instrument.
- Sustain pedal where applicable.
- Pitch bend.
- Smooth parameter automation.
- Preset loading and saving.
- Voice stealing without clicks.
- Independent level controls.
- Appropriate gain management.

The six instruments must sound clearly different even with every environmental effect disabled.

## 1C. Instrument selection

Add an **INSTRUMENTS** page with six selectable instrument cards.

Each card displays:

- Instrument name.
- Instrument icon or visual artwork.
- Current level.
- Mute/enable state.
- Quick-edit controls.
- Access to detailed synthesis parameters.

Provide a focused instrument editing view rather than cramming every control into the main screen.

Allow multiple instruments to be enabled simultaneously, with independently adjustable levels.

For the initial implementation, incoming MIDI may be routed to all enabled instruments. Add optional MIDI channel filtering or instrument-specific key ranges if technically practical.

## Milestone 1 acceptance tests

- All six instruments produce sound from MIDI input.
- Each instrument has a recognizable, distinct timbre.
- Chords are playable.
- Note release works correctly.
- No stuck notes.
- No uncontrolled clipping.
- Host automation works.
- The plugin loads as a software instrument in GarageBand.
- Existing continuous ambience functionality remains operational.

Do not proceed to the next milestone with only one generic synth engine renamed six times.

---

# MILESTONE 2 — COMPLETE ENVIRONMENTAL AUDIO OVERHAUL

**Priority: Critical**

The current environments are not sufficiently distinct.

Changing locations must result in a dramatic, immediately recognizable transformation.

An abandoned hospital must not sound like an underground parking garage with a slightly different reverb.

## 2A. Environment identity system

Each environment must define:

1. Unique background ambience.
2. Distinct foreground incidental sounds.
3. Unique acoustic dimensions.
4. Material absorption and reflection characteristics.
5. Environment-specific musical processing.
6. Environmental evolution patterns.
7. A distinctive visual theme.
8. A recommended collection of instrument presets.

An environment must be a complete sound-design configuration, not a small list of slider values.

## 2B. Ten core environments

Implement the following environments.

### 1. VACANT HOSPITAL

Sound identity:

- Fluorescent electrical buzz.
- Air circulation.
- Distant medical-equipment-like beeps.
- Occasional ventilation rattling.
- Sparse metallic impacts.
- Long sterile corridors.

Instrument processing:

- Dark, filtered piano.
- Distant stereo reflections.
- Subtle unstable pitch.
- Slightly cold, clinical resonance.

### 2. EMPTY SHOPPING MALL

Sound identity:

- Very large open interior.
- Escalator-like mechanical hum.
- Distant ventilation.
- Sparse metallic structural movement.
- Long diffuse reflections.

Instrument processing:

- Washed-out piano.
- Long atmospheric decay.
- Wide stereo image.
- Soft high-frequency attenuation.

### 3. ABANDONED SCHOOL

Sound identity:

- Fluorescent lighting.
- Air ducts.
- Distant pipe movement.
- Occasional mechanical creaking.
- Narrow hallway reflections.

Instrument processing:

- Nostalgic detuned piano.
- Slightly degraded music box.
- Dark, narrow reflections.
- Subtle pitch flutter.

### 4. UNDERGROUND PARKING GARAGE

Sound identity:

- Deep ventilation rumble.
- Distant mechanical movement.
- Hollow impacts.
- Concrete reflections.
- Low-frequency structural vibration.

Instrument processing:

- Heavy low-mid resonance.
- Strong early reflections.
- Long concrete reverberation.
- Darkened high frequencies.

### 5. ABANDONED INDOOR POOL

Sound identity:

- Large tiled chamber.
- Very long bright reflections.
- Distant water drips.
- Low ventilation.
- Sparse metallic echoes.

Instrument processing:

- Bright reflective reverb.
- Distinct echoes.
- Extended high-frequency tails.
- Spacious stereo field.

### 6. EMPTY HOTEL

Sound identity:

- Quiet ventilation.
- Distant elevator-like machinery.
- Soft structural creaks.
- Muffled sounds behind walls.
- Carpeted interior acoustics.

Instrument processing:

- Muffled piano.
- Soft, dark reverb.
- Subtle tape-like degradation.
- Intimate but distant ambience.

### 7. DESERTED SUBWAY STATION

Sound identity:

- Tunnel airflow.
- Electrical infrastructure.
- Deep mechanical vibrations.
- Distant rail-like resonances.
- Long directional reflections.

Instrument processing:

- Metallic delays.
- Strong bass resonance.
- Long tunnel echo.
- Occasional pitch modulation.

### 8. EMPTY SUPERMARKET

Sound identity:

- Refrigeration compressors.
- Fluorescent hum.
- Ventilation.
- Occasional compressor cycling.
- Reflective commercial interior.

Instrument processing:

- Thin, nostalgic tonal coloration.
- Mild chorusing.
- Artificial room reflections.
- Low electrical modulation.

### 9. ENDLESS OFFICE

Sound identity:

- Computer/transformer hum.
- HVAC.
- Fluorescent lighting.
- Subtle electronic activity.
- Repetitive architectural acoustics.

Instrument processing:

- Soft, distant synth textures.
- Spectral filtering.
- Modulated delay.
- Unnaturally diffuse reverb.

### 10. IMPOSSIBLE ROOM

Sound identity:

- Unstable room tone.
- Unnatural resonances.
- Nonphysical reflections.
- Slowly shifting harmonic content.
- Unpredictable environmental activity.

Instrument processing:

- Pitch-shifting reverberation.
- Unstable stereo space.
- Dissonant harmonics.
- Very long atmospheric tails.

## 2C. Replace simplistic ambience generation

Each location must contain multiple original, procedurally generated sound elements.

Improve the existing electrical, ventilation, structure, distant-activity, atmospheric, and substructure engines.

Do not merely change their levels.

Use location-specific event types, oscillator models, filtering, modulation, acoustic responses, and event distributions.

Examples:

- Hospital: periodic equipment-like electronic tones.
- Pool: synthesized water drops with tiled reflections.
- Supermarket: refrigeration compressor cycles.
- Subway: rail resonances and directional tunnel echoes.
- Hotel: distant elevator-like mechanisms and muffled impacts.

Sound events should vary naturally over time.

Avoid repetitive 5–10-second patterns.

## 2D. Environment comparison tests

Implement a controlled comparison test:

- Render 30 seconds of ambience for every environment.
- Use the same random seed and matching master output target.
- Disable instruments.
- Compare spectral profiles, event density, modulation, and reverberation.

Then repeat with a standard MIDI piano phrase.

Every environment should sound recognizably different in both tests.

Do not use loudness differences as the primary way to distinguish environments.

## Milestone 2 acceptance tests

- Ten distinct environmental soundscapes.
- Unique procedural events per environment.
- Meaningfully different acoustic characteristics.
- No obvious short repeating loops.
- Audible difference with the same instrument phrase.
- Stable output levels.
- No regressions to existing synthesis functionality.

---

# MILESTONE 3 — PER-INSTRUMENT CINEMATIC EFFECTS

**Priority: Critical**

Every instrument must have its own effects.

The selected environment must automatically establish an appropriate instrument-effects character.

The user must also be able to customize or bypass these effects independently.

## 3A. Required effect modules

Implement:

**1. Detune / Drift**

Slow pitch instability, random pitch wander, and stereo detuning.

**2. Tape Degradation**

Wow/flutter, bandwidth reduction, gentle saturation, hiss, and optional dropouts.

**3. Environmental Reverb**

Uses the selected environment's acoustic profile.

**4. Echo / Delay**

Tempo-synchronized or free-time delay, with filtering and feedback controls.

**5. Chorus / Modulation**

Subtle stereo widening through modulation.

**6. Spectral Filter**

Low-pass, high-pass, and resonant tonal control.

**7. Granular Texture**

Optional granular smearing, freeze, or fragmentation for abstract textures.

Implement a practical, efficient granular effect rather than an unnecessarily expensive engine.

## 3B. Per-instrument effect management

Each instrument must expose:

- Individual effect toggles.
- Effect amount.
- Detailed controls.
- Effect order where appropriate.
- Dry/wet mix.
- Environment influence amount.

Allow users to choose how much the current environment changes each instrument.

At 0% environment influence, the instrument should retain its original sound.

At 100%, the full environmental effects profile should be applied.

## 3C. Environmental effect defaults

Each environment should have distinct default effects for every instrument.

For example:

**Forgotten Piano in Empty Mall:**
Long diffuse reverb, high-frequency softening, moderate distance, gentle detuning.

**Forgotten Piano in Pool:**
Bright tiled reverb, strong echoes, minimal tape degradation.

**Forgotten Piano in Hospital:**
Cold filtering, short early reflections, distant stereo image.

**Forgotten Piano in Impossible Room:**
Unstable pitch, spectral smearing, unusual reflections, pitch-shifted tails.

These must be meaningfully different effect chains.

## 3D. Smooth transitions

Changing environments must not produce clicks or unstable feedback.

Use appropriate parameter smoothing, effect-state transitions, and crossfading when required.

## Milestone 3 acceptance tests

- Every instrument supports independent effects.
- Effects are audible and adjustable.
- No placeholder controls.
- Environment influence works.
- Bypassing effects restores the dry instrument.
- Environment changes are smooth.
- CPU remains practical.
- Existing presets restore correctly.

---

# MILESTONE 4 — COMPLETE VISUAL REDESIGN

**Priority: Critical**

The current GUI is visually inadequate and must be substantially redesigned.

Do not retain the current appearance as the primary interface.

The plugin should evoke the emotional character of liminal environments.

It should feel like an atmospheric cinematic instrument, not a generic engineering utility.

## 4A. New visual identity

The graphical style should draw from:

- Dreamlike empty interiors.
- Analog photography.
- Architectural spaces.
- Faded memories.
- Artificial lighting.
- Psychological horror.
- Atmospheric cinematic imagery.

Avoid generic industrial dashboards, excessive boxes, default JUCE-style controls, or a screen dominated by tiny sliders.

The interface should be visually rich but remain clear and professional.

## 4B. Dynamic environment backgrounds

**Every environment must have a distinct visual identity.**

Examples:

- Hospital: cold green fluorescent corridor.
- Mall: enormous empty atrium.
- School: yellowed institutional hallway.
- Parking Garage: concrete pillars and isolated lighting.
- Pool: blue tiled chamber.
- Hotel: warm, faded carpeted corridor.
- Subway: deserted underground platform.
- Supermarket: empty aisles under fluorescent lighting.
- Office: repetitive cubicles and overhead lights.
- Impossible Room: geometrically unsettling architecture.

The main visual must change when the selected environment changes.

Use original, appropriately licensed images or carefully designed original vector/rendered scenes.

Do not fetch copyrighted images and bundle them without permission.

A stylized but polished visual implementation is preferable to stolen photographic assets.

Do not rely on AI image-generation APIs at plugin runtime.

## 4C. Ambient animation

Add subtle environmental movement, such as:

- Fluorescent flickering.
- Light pulsing.
- Slow haze movement.
- Reflection changes.
- Gentle image distortion.
- Subtle environmental activity.

Animation should respond to the audio engine or environment state where practical.

Keep animation understated.

Optimize through caching and modest frame rates.

## 4D. Main interface layout

### MAIN VIEW

The main view should prioritize:

1. Large environmental artwork.
2. Environment selector.
3. Instrument selection.
4. Four macro controls.
5. Minimal transport and output controls.

The visual should occupy a substantial portion of the interface.

Do not leave half the screen as empty black space.

### INSTRUMENT VIEW

Show:

- Instrument selection cards.
- Detailed synthesis controls.
- Effects.
- Envelope controls.
- Instrument output.

### ENVIRONMENT VIEW

Show:

- Ambient sound layers.
- Room dimensions.
- Materials.
- Source and listener positioning.
- Environmental evolution.
- Event-density controls.

Move the current technical floor-plan interface into this dedicated page.

It should no longer dominate the main display.

### EFFECTS VIEW

Show independent effect chains for each instrument.

### SETTINGS VIEW

Show:

- Quality settings.
- MIDI settings.
- Output management.
- Host synchronization.
- Preset management.

## 4E. Visual consistency

Create a reusable UI design system.

Include:

- Consistent typography.
- Clear active and inactive states.
- Custom controls.
- Unified spacing.
- Subtle depth.
- Readable values.
- Context-sensitive accent colors.
- Resizable layout where practical.

All controls must function and remain readable at normal plugin sizes.

Do not implement a beautiful static mockup disconnected from audio parameters.

## Milestone 4 acceptance tests

- Every environment has distinct artwork.
- Artwork changes on environment selection.
- Main interface is substantially redesigned.
- Existing controls remain accessible.
- Instrument and effects pages work.
- Animations perform efficiently.
- No clipped or overlapping UI elements.
- Screenshots are captured and reviewed at multiple window sizes.

---

# MILESTONES 5–7 — Creative depth, workflow and polish

<WritingBlock id="72642" variant="document"># LIMINAL — DIRECTION UPDATE 1
## Continuation: Milestones 5–7

Use this document together with the preceding Milestones 1–4 specification.

---

# MILESTONE 5 — CHARACTERS, EVOLUTION AND CINEMATIC TRANSITIONS

**Priority: High**

Every environment needs multiple emotional identities, called **Characters**.

Characters should transform the environment, musical processing, and event behavior without changing its fundamental location.

## 5A. Three Characters per environment

Implement the following:

| Environment | Character 1 | Character 2 | Character 3 |
|---|---|---|---|
| Hospital | Sterile Silence | After Hours | Something Wrong |
| Shopping Mall | Closing Time | Forgotten | Endless |
| School | After Class | Childhood Memory | Nightmare |
| Parking Garage | Vacant | Distant Movement | Pursuit |
| Indoor Pool | Still Water | Forgotten Summer | Submerged |
| Hotel | Late Night | Faded Luxury | Room 404 |
| Subway Station | Last Train | Abandoned | Approaching |
| Supermarket | Closing Shift | Nostalgia | Fluorescent Nightmare |
| Endless Office | After Hours | Dream State | Infinite Cubicles |
| Impossible Room | Familiar | Unstable | Beyond Reality |

Each Character must modify:

- Environmental event selection and density.
- Background ambience characteristics.
- Musical instrument recommendations.
- Default instrument effects.
- Room coloration.
- Evolution speed and depth.
- Unreal macro response.
- Visual atmosphere, such as lighting or color grading.

Characters must produce clearly audible changes.

Do not implement Characters as names pointing to nearly identical presets.

## 5B. Evolution

Improve the environmental evolution system so locations feel continuously alive.

Support:

- Slow spectral changes.
- Irregular mechanical events.
- Occasional environmental disturbances.
- Gradual changes in spatial perception.
- Slow harmonic instability.
- Deterministic random seeds.
- Adjustable evolution rate.
- Freeze and reset.

Provide a visual indication of environmental activity without distracting animations.

## 5C. Scene morphing

Preserve and improve the existing Scene A/B functionality.

Allow two scenes to store:

- Environment.
- Character.
- Enabled instruments.
- Instrument parameters.
- Individual effects.
- Background ambience.
- Unreal settings.
- Master controls.

Morphing should interpolate compatible parameters smoothly.

For incompatible instruments or environment engines, use carefully controlled audio crossfading or an explicitly defined switching strategy.

Do not attempt to linearly interpolate incompatible discrete DSP states.

Avoid clicks, abrupt changes in loudness, and stuck MIDI notes.

## 5D. Main performance macros

Retain and improve:

- **Presence:** Apparent closeness and prominence.
- **Scale:** Acoustic size and spatial character.
- **Activity:** Event density and environmental movement.
- **Unreal:** Transformation from believable space to impossible space.

Each environment and Character must respond differently to these macros.

For example, increasing Unreal in the pool should produce unnatural reflective echoes, while increasing it in the school should emphasize pitch instability, nostalgic degradation, and unsettling spatial movement.

## Milestone 5 acceptance tests

- Thirty functional Characters.
- Audible distinctions between Characters.
- Distinct visual atmosphere per Character.
- Smooth Scene A/B transitions.
- Reliable scene state restoration.
- No clicking or large level jumps.
- Automatable macros.
- Deterministic evolution when using fixed seeds.

---

# MILESTONE 6 — PERFORMANCE, PRESETS AND PRODUCTION WORKFLOW

**Priority: High**

LIMINAL must be practical for real film and game music sessions.

## 6A. Preset browser

Organize presets into:

- Environments
- Instruments
- Characters
- Complete Scenes
- User Presets

Provide at least:

- 10 environment presets.
- 30 Character configurations.
- 6 foundational instrument presets.
- 20 additional instrument variations.
- 20 complete cinematic scene presets.

Complete scene presets must configure the instruments, ambience, processing, and atmosphere together.

Example scene presets:

- Forgotten Childhood
- The Last Patient
- Closing Time
- Dream of an Empty Pool
- Corridor Without End
- Mall After Midnight
- Memories of Summer
- Something Below the Station
- Rooms That Shouldn't Exist
- Home, But Wrong

## 6B. Performance and MIDI

Support:

- MIDI note input.
- Sustain pedal.
- Pitch bend.
- Polyphony.
- Velocity.
- Instrument layering.
- Note-range controls where practical.
- DAW automation.

Allow optional velocity-to-filter and velocity-to-volume modulation.

The plugin must behave like a genuine musical instrument in GarageBand and Logic.

## 6C. Recording and export

Implement an export feature for generated ambience.

Support:

- Rendering background ambience to WAV.
- User-selected duration.
- Sample-rate selection where appropriate.
- Stereo output.
- Optional instrument-free ambience stems.
- Offline rendering where feasible.

The render must use the same underlying DSP engine as live playback.

Do not render from a separate, lower-quality approximation.

Ensure offline rendering is deterministic when fixed-seed and transport-synchronized settings are enabled.

If reliable in-plugin export requires a non-real-time worker or standalone utility, implement it outside the audio callback.

## 6D. CPU optimization

Introduce three quality modes:

**ECO:** Reduced processing for large DAW sessions and fanless laptops.

**STANDARD:** Balanced everyday production mode.

**HIGH:** Maximum detail for final rendering.

Quality should adjust appropriate parameters such as:

- Reverb complexity.
- Oversampling.
- Granular density.
- Maximum voices.
- Environmental event processing.
- Optional visual animation detail.

Quality changes must not create audible artifacts.

Optimize for Apple Silicon without sacrificing portability.

## 6E. State management

Ensure complete state restoration when:

- Saving a GarageBand project.
- Reopening the project.
- Duplicating a plugin instance.
- Changing a scene.
- Loading a user preset.
- Switching environments.

Use stable parameter identifiers and versioned state serialization.

## Milestone 6 acceptance tests

- All presets load.
- MIDI playback works.
- Sustained chords remain stable.
- DAW state is restored correctly.
- WAV export works where supported.
- No excessive memory growth.
- CPU usage scales with Quality mode.
- Project reload does not change saved scenes unexpectedly.

---

# MILESTONE 7 — FINAL SOUND QUALITY, VISUAL POLISH AND VALIDATION

**Priority: Critical before release**

This milestone is not a license to merely declare the product complete.

It is a deliberate quality-control stage addressing audio realism, instrument expressiveness, environment identity, user experience, and host compatibility.

## 7A. Audio quality evaluation

Create reproducible demo renders demonstrating:

1. Each instrument playing the same suitable MIDI phrase.
2. Each environment with no instruments.
3. The same piano phrase through all ten environments.
4. All three Characters in one environment.
5. A full Unreal macro sweep.
6. Scene A/B morphing.
7. Long-form environmental evolution.

Export representative WAV files and provide a clear index identifying their preset and settings.

Evaluate:

- Audible differences.
- Spectral richness.
- Dynamic consistency.
- Natural decay.
- Stereo coherence.
- Unwanted resonances.
- Harshness.
- Low-frequency accumulation.
- Clicking.
- Repetition.
- Musical usefulness.

Do not treat automated signal metrics as proof of high subjective sound quality.

Do not use arbitrary thresholds to claim two locations sound emotionally distinct.

## 7B. Instrument refinement

Each instrument must be examined for:

- Convincing attack.
- Proper note decay.
- Expressive velocity response.
- Tuning stability where appropriate.
- Adjustable intentional detuning.
- Chord performance.
- Modulation quality.
- Consistent levels across the keyboard.

Improve weak engines rather than masking them with excessive reverb.

The dry instrument must be musically useful.

## 7C. Environment refinement

Every environment must have a distinct sonic signature based on:

- Source types.
- Event distributions.
- Tonal spectrum.
- Reverberation.
- Architectural acoustics.
- Mechanical activity.
- Instrument processing.
- Character behavior.

If two environments sound essentially identical, treat this as a quality failure requiring revision.

## 7D. Interface polish

Review the GUI for:

- Distinctive visual atmosphere.
- Readable controls.
- Consistent layout.
- Smooth interactions.
- Coherent typography.
- Clear navigation.
- Efficient rendering.
- Proper scaling.
- Lack of visual clutter.
- No unnecessary empty space.

The environment artwork should remain visually prominent.

Compare the completed interface against the current build and document the meaningful improvements.

Take screenshots of at least five substantially different environments.

## 7E. Host testing

Test where tooling is available:

- Standalone application.
- GarageBand AU.
- Logic Pro AU.
- Compatible VST3 host.

Explicitly report which hosts were actually tested.

Do not claim GarageBand testing unless GarageBand was launched and the plugin verified.

Test:

- Loading.
- MIDI input.
- Presets.
- Parameter automation.
- Playback.
- Project reopening.
- Instance duplication.
- Stereo output.
- Plugin window resizing.

## 7F. Final deliverables

Provide:

- Complete updated source code.
- Passing automated tests.
- Updated build instructions.
- Updated README.
- Complete parameter documentation.
- Factory presets.
- DEMO_AUDIO directory or artifact links where available.
- UI screenshots.
- DEVLOG milestone checklist.
- Known limitations.
- Build and installation instructions.

At the end, present every milestone with one of these statuses:

- Completed and verified.
- Implemented but not fully tested.
- Partially completed.
- Blocked.
- Not started.

Never describe unverified functionality as finished.

---

# IMPLEMENTATION PRIORITY

Follow this order:

1. Complete the playable instrument engine.
2. Make environments drastically different.
3. Implement environment-specific instrument effects.
4. Replace the interface.
5. Implement Characters and cinematic transitions.
6. Complete the production workflow.
7. Perform comprehensive testing and refinement.

**Do not prioritize visual polish while instruments are still missing.**

**Do not prioritize adding more environments before the existing environments sound meaningfully different.**

**Do not claim success because all requested buttons and screens exist.**

The fundamental goal is that a composer can open LIMINAL, select an environment, play a beautiful but unsettling piano melody, and immediately hear that melody become part of the chosen architectural space.

Changing the environment should transform the entire experience, both visually and sonically.

Begin by auditing the existing repository, updating `DEVLOG.md`, and implementing Milestone 1. Continue through subsequent milestones as the environment and available execution budget permit, preserving a buildable repository at each completed stage.