# PC speaker

The opening of Beethoven's "Für Elise" on the PC speaker, which plays one square-wave tone at a time. Build, then Run, with your sound on.

The tune is a table of pairs of dwords, a frequency in hertz and a duration in milliseconds: `659, 150` plays 659 Hz for 150 ms. A frequency of 0 is a rest, and the pair `0, 0` ends the tune.

The editor's course goes through it: [PC speaker](https://asm-editor.specy.app/learn/courses/x86/examples/pc-speaker).
