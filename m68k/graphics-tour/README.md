# Graphics tour

Build, then Run. The program demonstrates the M68K screen's drawing tasks in one picture: pen and fill colours, a pixel and reading it back, lines, filled and outlined rectangles and ellipses, flood fill, the pen's width, drawing modes and text at a pixel position. It then demonstrates the standard player's sound tasks with WAV files the program writes itself.

It is a port of `graphicSound.X68`, one of the example programs that come with [EASy68K](http://www.easy68k.com), by Chuck Kelly. This editor supports drawing modes 2, 4, 16 and 17, so the port omits the original's loop over the other modes. The remaining drawing demonstrations work as they do in EASy68K.

## License

Being a port of EASy68K's program, this example is under the GNU General Public License, version 2 or later (`LICENSE`), not the MIT License of the rest of asm-examples. EASy68K is copyright Charles Kelly and Tim Larson.
