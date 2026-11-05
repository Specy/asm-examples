# Drawing a house

This program draws a house under the sun, with sky and ground, on the Screen. Each part is drawn over the parts before it.

| Target | How it draws |
| ------ | ------------ |
| M68K   | With the drawing tasks of `trap #15`: filled rectangles (task 87), an ellipse for the sun (88), lines around the roof, filled from a point inside it (85, 89), and a label (95). The picture is kept by the Screen, not in M68K memory. |
| MIPS   | Stores colour words into a 32 × 32 grid of words, one word per cell. `fill_rect` and `fill_disc` work out which words to change. The `@screen` comment at the top connects the grid to the Screen. |
| RISC-V | Same as MIPS. |
| X86    | Fills rectangles in the framebuffer, four bytes per pixel. The sloping roof and the round sun are made of rectangles one row high. |
| Z80    | Writes drawing commands to the Screen's ports: it sets the colours and coordinates, then sends the command. |

Go to the course's lecture to read how each version works: [M68K](https://asm-editor.specy.app/learn/courses/m68k/examples/drawing-on-the-screen), [MIPS](https://asm-editor.specy.app/learn/courses/mips/examples/drawing-on-the-screen), [RISC-V](https://asm-editor.specy.app/learn/courses/risc-v/examples/drawing-on-the-screen), [X86](https://asm-editor.specy.app/learn/courses/x86/examples/drawing-on-the-screen) or [Z80](https://asm-editor.specy.app/learn/courses/z80/examples/drawing-on-the-screen).
