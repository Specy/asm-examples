# Play sound files

Writes two files and plays them: `tone.wav`, half a second of a square wave, and `tune.mid`, four notes on a flute. Build, then Run, with your sound on.

The WAV file holds samples, the sound's level over time. The MIDI file holds instructions for a synthesizer, which makes the sound: each event is a delay in ticks, an event code and its data. The program writes both with the file tasks of `trap #15`, then plays them with the sound tasks.

The editor's course goes through every byte of both files: [Play sound files](https://asm-editor.specy.app/learn/courses/m68k/examples/play-sound-files).
