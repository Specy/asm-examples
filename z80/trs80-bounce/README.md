# TRS-80 bounce

Build, Run, then click the Screen. Use the arrow keys to steer the block as it bounces around the TRS-80's display. It runs until you press Stop.

Each frame is drawn in ordinary memory, then copied into video memory with one `ldir` of 1,024 bytes. The screen shows the completed frame without a double-buffering command. The program reads the arrow keys from the keyboard matrix at `0x3800`.
