# TRS-80 text

The TRS-80's display is memory: 1 KB at `0x3C00`, 64 columns by 16 rows, one byte a character. Store a byte and the character appears. Characters `0x20` to `0x7F` are text, and 128 to 191 are 2 × 3 blocks of chunky pixels, which also make the screen a grid of 128 × 48 pixels. Build, then Run.

The `; @screen trs80` comment asks the editor for this display. Being a comment, it leaves the program as any other Z80 assembler reads it.
