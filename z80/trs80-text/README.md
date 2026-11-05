# TRS-80 text

This program shows text and block graphics on the TRS-80's display.

The display is memory: 1 KB at `0x3C00`, 64 columns by 16 rows, one byte per character. Store a byte there and its character appears. Bytes `0x20` to `0x7F` are text characters. Bytes `0x80` to `0xBF` are blocks of 2 × 3 pixels, which turn the screen into a grid of 128 × 48 pixels.

The `; @screen trs80` comment tells the editor to use this display. Other Z80 assemblers ignore it, because it is a comment.
