# TRS-80 bounce

A block bouncing around the TRS-80's display. Each frame is drawn in ordinary memory, where nobody sees it, then copied into video memory with one `ldir` of 1,024 bytes, so the screen never shows half a frame and no double-buffering command is needed. The arrow keys steer, read from the keyboard matrix at `0x3800`. Build, Run, then click the Screen. It runs until you press Stop.
