# MIKE GORDON v1.0

A vibe and character box for guitar loops, keys, vocals, drums - any sample. It is not a
distortion: it makes things smooth, dreamy, dusty, wobbly or wide. VST3 for FL Studio on Mac
(Apple Silicon and Intel). Original code, names and look.

## The quick way
1. Pick a preset at the top (Smooth, Dreamy, Dusty, Underwater, Broken Tape, Motion, Rhythmic, Wide...).
   The < and > arrows step through them.
2. Turn **AMOUNT** (big knob on the right) to choose how much of the vibe you hear. 0% = your original.
3. Hold your ears honest with **BEFORE**: click it to hear the original, click again to come back.
   Level match keeps both at the same loudness, so you judge the vibe, not the volume.
4. Press **Randomize** for a new idea. Click "lock" on any section you want to keep first.

## The sections (left to right is the order the sound goes through)
- **Tone - pick an amp.** Glass = crystal clean with stereo chorus. Warm = round clean with
  tremolo + spring. Chime = bright, can break up a little. Swim = real pitch vibrato wobble.
  Drive is kept light. The second knob changes with the amp (Chorus / Trem / Sparkle / Vibrato),
  Speed sets how fast it moves. Bass, Mid, Treble, Spring and Bright are underneath.
- **Filter.** Big knob = Low-pass (turn down to make it muffled / far away). Movement row:
  Off, LFO (sweeps in time with your song) or Envelope (auto-wah that follows how hard you play).
- **Pitch.** Detune = width and chorus-like thickness. Oct Down / Oct Up blend in octaves.
- **Motion.** Chorus, Flanger, Phaser, Swirl (a rotating-speaker style swirl) or Tremolo.
  Depth is the big knob. Turn on "Sync to tempo" to lock the speed to FL's BPM.
- **Texture.** Tape Wobble, Cassette (dull, worn tone), Crackle, Hiss, Crush (bit-crush).
  Noise only plays while sound is coming in (switch that off under "more").
- **Space.** Spring, Plate or Echo. Mix is the big knob. Shimmer adds a sparkly octave on top.
  Freeze holds the current sound as a pad. For Echo, the third knob becomes the echo time.
- **Width + Duck.** Width spreads the sound; Mono bass keeps the low end in the centre so it
  doesn't fight your 808. Duck makes the effect pump with the beat (tempo pump by default,
  or a real sidechain - see INSTALL.txt).

"more" on a section shows its less-used controls. "back" returns.

## Bottom strip
- **Macros 1-3** - one knob that moves several controls. Defaults: Dream (more space, motion,
  width), Dark (less treble, lower filter), Age (more wobble, cassette, crackle). Dark needs the
  Filter section on for its filter part; Dream needs Space/Motion on to be heard. "assign"
  changes what a macro turns. All three can be automated in FL.
- **Order** - drag a section left or right to change the order. Reverb before the amp
  sounds very different from after it.

## Good to know
- Zero latency. Octave/detune voices and the vibrato naturally trail the dry sound by a few
  milliseconds (that's how pitch effects work); the plug-in itself adds no delay.
- The Safety limiter only touches peaks above -1 dBFS.
- Your own presets: "Save" at the top. They go in Music/MIKE GORDON Presets.
- The window is resizable (drag the bottom-right corner).
