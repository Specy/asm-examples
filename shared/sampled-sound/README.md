# Sampled sound

This program computes a sound one sample at a time, then plays it as a scale. A sample is the level of the sound at one instant. Turn your sound on before you run it.

| Target | Where the samples go |
| ------ | -------------------- |
| M68K   | Into the editor's Synth at `$E00100`: a fading square wave of signed bytes, 32 samples per period. The program plays it at eight pitches by changing the sample rate. EASy68K does not have the Synth. |
| MIPS   | Same as M68K, with the Synth at `0xffff0100`. MARS does not have it. |
| RISC-V | Same as M68K, with the Synth at `0xffff0100`. RARS does not have it. |
| X86    | Into `/dev/dsp`, at 22,050 samples per second: eight fading notes of a C major scale, as signed 16-bit mono samples. |

Go to the course's lecture to read how the samples are made: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/sampled-sound), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/sampled-sound), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/sampled-sound) or [X86](https://asm-editor.specy.app/learn/courses/x86/examples/sampled-sound).
