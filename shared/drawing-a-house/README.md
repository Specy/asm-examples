# Drawing a house

A house under the sun, with sky and ground, drawn on the Screen. Build, then Run, and look at the Screen. Later parts are drawn over earlier ones.

| Target | How it draws                                                                                                                                                                                                          |
| ------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | Drawing tasks of `trap #15`: filled rectangles (task 87), an ellipse for the sun (88), lines around the roof filled from a point inside it (85, 89), and a label (95). The Screen keeps the picture, not M68K memory. |
| MIPS   | Stores colour words into a 32 × 32 grid of words, one cell each. `fill_rect` and `fill_disc` work out which words to change. The `@screen` comment at the top connects the grid to the Screen.                        |
| RISC-V | As on MIPS.                                                                                                                                                                                                           |
| X86    | Fills rectangles in the framebuffer, four bytes a pixel. A rectangle one row high is enough for the sloping roof and the round sun.                                                                                   |
| Z80    | Writes drawing commands to the Screen's ports: set the colours and coordinates, then send one command.                                                                                                                |

The editor's courses explain each version: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/drawing-on-the-screen), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/drawing-on-the-screen), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/drawing-on-the-screen), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/drawing-on-the-screen) and [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/drawing-on-the-screen).
