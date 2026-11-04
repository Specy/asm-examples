# Bouncing ball

Build, then Run to watch a ball bounce around the Screen. It runs until you press Stop.

Each version draws the ball and waits before drawing again. The wait sets the intended frame rate, though a slower computer may take longer to draw a frame. M68K and Z80 draw off screen and present the finished frame. MIPS and RISC-V update the bitmap display directly.

| Target | How it draws and waits                                                                                                                                                                                                                                  |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | Draws off screen (task 92, mode 17), shows the frame with task 94 and waits two hundredths of a second with task 23, on the 640 × 480 screen a program starts with.                                                                                     |
| MIPS   | Erases the ball and draws it again on the bitmap display, then sleeps for a frame (syscall 32). Every 64 frames it prints the program time that has passed (syscall 30). The `@screen` comment at the top sets up a 512 × 512 display of 4-pixel units. |
| RISC-V | As on MIPS, with ecalls 32 and 30.                                                                                                                                                                                                                      |
| Z80    | Draws off screen, presents the frame and waits for the next animation frame on port `0x51`, on the 256 × 192 Screen a program starts with.                                                                                                              |
| X86    | Draws each frame on the framebuffer's hidden page and then shows it, or straight on the visible page when there is only one. It draws 600 frames at up to 60 a second, then ends. |
