# Keyboard and display

This program echoes the characters you type to the console, using memory-mapped I/O. Click the Screen, then type. Most characters also paint a cell of the bitmap display, in a colour made from the character's code. `c` clears the console and `q` ends the program.

The program reads and writes these memory-mapped registers:

| Address      | Register |
| ------------ | -------- |
| `0xffff0000` | Receiver control: bit 0 is set while a character is waiting |
| `0xffff0004` | Receiver data: the character, in the low byte |
| `0xffff0008` | Transmitter control: bit 0 is set when the display is ready to accept a character |
| `0xffff000c` | Transmitter data: store a character here to print it |

The program polls the receiver control register. While no character is waiting, it sleeps for 10 ms between checks instead of looping without a pause.
