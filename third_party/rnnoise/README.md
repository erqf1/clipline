RNNoise v0.1 (https://github.com/xiph/rnnoise, BSD-3-Clause, see COPYING), unmodified except:
- `VARARRAY` in `src/arch.h` replaces C99 variable-length arrays in `celt_lpc.c` and `pitch.c` (MSVC).
Used by Clipline to remove keyboard/mouse clicks and background noise from the microphone.
