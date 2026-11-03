# Graphics tour

Every drawing task of the M68K screen on one picture: pen and fill colours, a pixel and reading it back, lines, filled and outlined rectangles and ellipses, flood fill, the pen's width, the drawing modes and text at a pixel position. Then the sound tasks of the standard player, playing WAV Files the program writes itself. Build, then Run.

It is a port of `graphicSound.X68`, one of the example programs that come with [EASy68K](http://www.easy68k.com), by Chuck Kelly. This editor supports the drawing modes 2, 4, 16 and 17, so the original's loop over the other modes is gone; everything else draws what it draws in EASy68K.

## License

Being a port of EASy68K's program, this example is under the GNU General Public License, version 2 or later (`LICENSE`), not the MIT License of the rest of asm-examples. EASy68K is copyright Charles Kelly and Tim Larson.
