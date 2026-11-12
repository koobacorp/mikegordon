# MIKE GORDON — plan (not started; waiting for Toto's "go")

A vibe and character plugin for guitar loops and any sample. Original code and look.
VST3 only, light on CPU, small file, resizable high-contrast window.

## Signal flow (sections can be dragged into a different order)
Input > Tone (amp) > Filter > Pitch > Motion > Texture > Space > Width/Ducking > Output

## 1. Tone: four amp voices (original, inspired by how these behave)
- Glass: very clean, built-in stereo chorus (JC-120 style)
- Warm: round clean, spring reverb + tremolo (Fender Twin/Deluxe style)
- Chime: bright, can break up a little (AC30 style)
- Swim: true pitch vibrato wobble (Magnatone style)
- Controls: Drive (kept light), Bass, Mid, Treble, Bright switch, Amp on/off

## 2. Filter
- Low-pass + high-pass sweep, resonance
- Movement: off / LFO (BPM-synced) / envelope (auto-wah)

## 3. Pitch
- Octave down / up blend, subtle detune for width

## 4. Motion (BPM-synced or free)
- Chorus, Flanger, Phaser, Uni-Vibe-style swirl, Tremolo; Rate, Depth, Mix each

## 5. Texture
- Tape wobble, vinyl crackle, hiss, bit-crush, worn-cassette tone

## 6. Space
- Spring reverb, plate reverb, tape echo (synced), Shimmer, Freeze/Hold

## 7. Width and ducking
- Stereo width with mono bass; ducking via sidechain or tempo pump

## 8. Output and workflow
- Big Amount knob, auto level-match + soft limiter, before/after button
- 3 assignable macro knobs, automatable in FL
- ~40 Vibe presets (Smooth, Dreamy, Dusty, Underwater, Broken Tape...)
- Randomize with per-section lock

## Left out on purpose
- Heavy distortion and cab/mic sim (Silver Bullet covers grit)

## Checks
- Automated tests for every control, pluginval strictness 10, CPU measured
- Demo WAVs of presets on a guitar loop
- Real feel and sound: Toto in FL

## Delivery
- One finished tested build, universal Mac VST3 zip; not installed until Toto asks

## Design (Toto, 2026-11-12)
- Minimal and friendly. Size things by how often they're used: Amp picker, Amount, Vibe presets,
  and the main knob per section are big; rarely used options (effect order, macro assignment,
  ducking source, randomize locks) are small or tucked behind a section's "more" toggle.
- Generous, even spacing; no clutter; clear labels on every control (no unlabelled dropdowns).
- Skin: emerald green. Accent: blonde/light warm brown (honey/sand). Strong contrast for main controls.
- Resizable window.

## Design (Toto's notes, added at "go")
- Minimal and friendly. Size things by how often they're used: big = amp picker, Amount, preset browser,
  the main knob of each section. Small or tucked behind a per-section "more" toggle = effect order,
  macro assignment, ducking source, randomize locks. Every control labelled.
- Skin: emerald green. Accent: blonde / honey light-brown. Strong contrast on the main controls.
- Resizable window.
