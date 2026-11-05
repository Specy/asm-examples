# Bitmap tour

This program paints a 256 × 256 picture on the bitmap display: a red and green colour ramp, a blue square in the middle, and white lines along the top and bottom rows.

On the bitmap display, each word of memory is one pixel, and its low 24 bits are the colour.

The `# @screen unit=1 width=256 height=256 base=display` comment at the top sets up the Screen: 256 × 256 pixels, one word per pixel, starting at the `display` label. Other assemblers ignore it, because it is a comment. The MIPS and RISC-V versions differ only in their instructions.
