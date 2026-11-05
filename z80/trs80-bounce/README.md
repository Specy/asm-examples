# TRS-80 bounce

This program bounces a block around the TRS-80's display. Click the Screen, then steer the block with the arrow keys. Press Stop to end it.

The program uses double buffering by hand, because the TRS-80 has no command for it: it draws each frame in ordinary memory, then copies it into video memory with one `ldir` of 1,024 bytes. The arrow keys are read from the keyboard matrix at `0x3800`.
