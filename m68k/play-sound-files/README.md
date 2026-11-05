# Play sound files

This program writes two sound files and plays them: `tone.wav`, half a second of a square wave, and `tune.mid`, four notes on a flute. Turn your sound on before you run it.

A WAV file stores samples: the level of the sound over time. A MIDI file stores instructions for a synthesizer, which makes the sound. Each MIDI event is a delay in ticks, an event code and its data. The program writes both files with the file tasks of `trap #15`, then plays them with the sound tasks.

Go to the course's lecture to read what every byte of both files means: [Play sound files](https://asm-editor.specy.app/learn/courses/m68k/examples/play-sound-files).
