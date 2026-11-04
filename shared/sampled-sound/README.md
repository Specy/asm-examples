# Sampled sound

A sound computed one sample at a time, then played as a scale. A sample is the sound's level at one instant. Build, then Run, with your sound on.

| Target | Where the samples go                                                                                                                                                                             |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| M68K   | A fading square wave of signed bytes, 32 samples a period, copied into the editor's Synth at `$E00100` and played at eight pitches by changing the sample rate. EASy68K does not have the Synth. |
| MIPS   | As on M68K, with the Synth at `0xffff0100`. MARS does not have it.                                                                                                                               |
| RISC-V | As on M68K, with the Synth at `0xffff0100`. RARS does not have it.                                                                                                                               |
| X86    | Eight fading notes of a C major scale, written as signed 16-bit mono samples to `/dev/dsp` at 22,050 samples a second.                                                                           |

The editor's courses go through the samples: [Sampled sound](https://asm-editor.specy.app/learn/courses/m68k/examples/sampled-sound) for M68K, and for [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/sampled-sound), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/sampled-sound) and [X86](https://asm-editor.specy.app/learn/courses/x86/examples/sampled-sound).
