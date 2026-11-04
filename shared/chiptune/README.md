# Chiptune

A short tune on three voices at once: a melody, a bass and drums. Build, then Run, with your sound on.

| Target | The sound chip                                                                                                                                                                              |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | The editor's Synth at `$E00100`: a pulse wave for the melody, a triangle wave for the bass and noise for the drums, each shaped by its channel's envelope. EASy68K does not have the Synth. |
| MIPS   | The same Synth at `0xffff0100`. MARS does not have it.                                                                                                                                      |
| RISC-V | The same Synth at `0xffff0100`. RARS does not have it.                                                                                                                                      |
| Z80    | The MSX's AY-3-8910, through ports `0xA0` and `0xA1`: square waves on channels A and B, and noise on C following the envelope generator for the drums.                                      |

The editor's courses explain each register it sets: [Chiptune](https://asm-editor.specy.app/learn/courses/m68k/examples/chiptune) for M68K, and for [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/chiptune), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/chiptune) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/chiptune).
