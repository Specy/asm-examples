# Keyboard and display

Build, Run, then click the Screen and type. The program echoes characters to the console through the display transmitter. Most characters also paint a cell of the bitmap display in a colour made from their code. `c` clears the console and `q` ends the program; neither paints a cell.

The program reads and writes these memory-mapped registers:

| Address      | Register                                                              |
| ------------ | --------------------------------------------------------------------- |
| `0xffff0000` | Receiver control: bit 0 is set while a character is waiting           |
| `0xffff0004` | Receiver data: the character, in the low byte                         |
| `0xffff0008` | Transmitter control: bit 0 is set when the display is ready to accept a character |
| `0xffff000c` | Transmitter data: store a character here to print it                  |

While no character is waiting, the program sleeps for ten milliseconds between checks. Sleeping avoids spending instructions on a continuous polling loop; each check still runs a few instructions.
