# PC speaker

This program plays the opening of Beethoven's "Für Elise" on the PC speaker, which plays one square-wave tone at a time. Turn your sound on before you run it.

The tune is a table of pairs of dwords: a frequency in hertz and a duration in milliseconds. For example, `659, 150` plays 659 Hz for 150 ms. A frequency of 0 is a rest, and the pair `0, 0` ends the tune.

Go to the course's lecture to read a line-by-line explanation: [PC speaker](https://asm-editor.specy.app/learn/courses/x86/examples/pc-speaker).
