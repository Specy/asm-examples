# Graphics tour

This program shows every drawing task of the M68K screen in one picture: pen and fill colours, setting and reading a pixel, lines, filled and outlined rectangles and ellipses, flood fill, pen width, drawing modes, and text at a pixel position. Then it plays sounds with the sound tasks, from WAV files it writes itself.

It is a port of `graphicSound.X68`, an example program by Chuck Kelly that comes with [EASy68K](http://www.easy68k.com). The editor supports only drawing modes 2, 4, 16 and 17, so the port leaves out the original's loop over the other modes. Everything else works as it does in EASy68K.

## License

Being a port of EASy68K's program, this example is under the GNU General Public License, version 2 or later (`LICENSE`), not the MIT License of the rest of asm-examples. EASy68K is copyright Charles Kelly and Tim Larson.
