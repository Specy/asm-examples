# Chiptune

This program plays a short tune on three voices at once: a melody, a bass line and drums. Turn your sound on before you run it.

| Target | The sound chip |
| ------ | -------------- |
| M68K   | The editor's Synth at `$E00100`: a pulse wave for the melody, a triangle wave for the bass and noise for the drums, each shaped by its channel's envelope. EASy68K does not have the Synth. |
| MIPS   | The same Synth, at `0xffff0100`. MARS does not have it. |
| RISC-V | The same Synth, at `0xffff0100`. RARS does not have it. |
| Z80    | The MSX's AY-3-8910 sound chip, through ports `0xA0` and `0xA1`: square waves on channels A and B for the melody and bass, and noise on channel C, shaped by the envelope generator, for the drums. |

Go to the course's lecture to read what each register does: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/chiptune), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/chiptune), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/chiptune) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/chiptune).
