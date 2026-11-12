# MIKE GORDON v1.0 - what was tested

All results below come from automated tests. Nothing here has been heard in FL Studio yet.

- Engine tests (tests/test_engine.cpp, plain C++, also run on the CI Mac): every control is
  checked. Filters and EQ are compared to textbook formulas (RBJ cookbook biquads, pre-warped
  state-variable filter, first-order allpass phase, Linkwitz-Riley 24 dB/oct mono-bass split),
  flanger comb notches against |cos(pi f D)|, phaser notch against the exact allpass product,
  tremolo depth/period, synced rates for all 15 divisions against the host beat, vibrato depth
  against 2*pi*f*A, echo time and repeat decay, spring RT60, freeze hold/release, level match,
  Amount mix law, bit-exact bypass / Before / Amount 0, safety limiter ceiling, click-free
  switching (23 cases), runaway feedback at maximum settings, NaN/inf input, denormals,
  sample rates 44.1-192 kHz, block-size independence, all presets, randomize + locks, macros,
  section order, and a 10-minute random long run.
- Plugin tests (tests/plugin_tests.cpp, JUCE, CI Mac): parameters match the engine, buses
  (stereo, mono, mono->stereo, sidechain), bypass, state save/load, programs, macros, order,
  randomize locks, sample rates/block sizes, editor build + resize + screenshots, CPU.
- pluginval strictness 10 on the VST3.
