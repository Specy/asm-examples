# Play a tune

Plays the first phrase of Beethoven's "Ode to Joy". A loop reads each note's pitch and duration from a table and plays it to the end before the next. Build, then Run, with your sound on.

| Target | How it plays a note                                                                                                   |
| ------ | --------------------------------------------------------------------------------------------------------------------- |
| MIPS   | Service 33 of `syscall`, which plays a note and waits for it to end.                                                  |
| RISC-V | Service 33 of `ecall`, as on MIPS.                                                                                    |
| X86    | MIDI messages written to `/dev/snd/midiC1D0`: a piano, with a drum under every note.                                  |
| Z80    | `out` sets the instrument, a piano, and each note's duration, and `in a, (c)` plays the note and waits for it to end. |

The editor's courses explain it line by line: [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/play-a-tune), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/play-a-tune), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/play-a-tune) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/play-a-tune).
