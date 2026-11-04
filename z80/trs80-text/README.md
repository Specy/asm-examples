# TRS-80 text

Build, then Run to see text and block graphics on the TRS-80's display.

The display is memory: 1 KB at `0x3C00`, 64 columns by 16 rows, one byte a character. Store a byte and the character appears. Characters `0x20` to `0x7F` are text, and 128 to 191 are 2 × 3 blocks of chunky pixels, which also make the screen a grid of 128 × 48 pixels.

The `; @screen trs80` comment asks the editor for this display. Being a comment, it leaves the program as any other Z80 assembler reads it.
