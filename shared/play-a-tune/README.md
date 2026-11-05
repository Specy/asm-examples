# Play a tune

This program plays the first phrase of Beethoven's "Ode to Joy". Turn your sound on before you run it.

A loop reads each note's pitch and duration from a table, and plays the note to the end before the next one.

| Target | How it plays a note |
| ------ | ------------------- |
| MIPS   | With service 33 of `syscall`, which plays a note and waits for it to end. |
| RISC-V | With service 33 of `ecall`, as on MIPS. |
| X86    | Writes MIDI messages to `/dev/snd/midiC1D0`: a piano, with a drum on every note. |
| Z80    | `out` sets the instrument, a piano, and each note's duration. `in a, (c)` plays the note and waits for it to end. |

Go to the course's lecture to read a line-by-line explanation: [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/play-a-tune), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/play-a-tune), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/play-a-tune) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/play-a-tune).
