# Bitmap tour

Build, then Run. The program paints a red and green ramp over a 256 × 256 grid, puts a blue square in the middle and draws white lines along the top and bottom rows, then stops.

On the bitmap display, one word of memory is one pixel and its low 24 bits are the colour.

The `# @screen unit=1 width=256 height=256 base=display` comment at the top sets up the Screen at every Build: one word drawn one pixel square, and the grid starting at the `display` label. Other assemblers read it as an ordinary comment. The MIPS and RISC-V versions differ only in their instructions.
