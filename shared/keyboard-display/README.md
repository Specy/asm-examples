# Keyboard and display

The simulator's memory-mapped keyboard and display:

| Address      | Register                                                              |
| ------------ | --------------------------------------------------------------------- |
| `0xffff0000` | Receiver control: bit 0 is set while a character is waiting           |
| `0xffff0004` | Receiver data: the character, in the low byte                         |
| `0xffff0008` | Transmitter control: bit 0 is set while the display takes a character |
| `0xffff000c` | Transmitter data: store a character here to print it                  |

Build, Run, then click the Screen to give the program the keyboard, and type. Every key is echoed to the console through the transmitter and paints one cell of the bitmap display, in a colour made from its code. `c` clears the console and `q` ends the program. While no key is waiting, the program sleeps for ten milliseconds between looks, so waiting costs no instructions.
