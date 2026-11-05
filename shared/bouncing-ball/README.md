# Bouncing ball

This program bounces a ball around the Screen. Press Stop to end it. The X86 version ends by itself after 600 frames.

Each frame, the program draws the ball and then waits. The wait sets the frame rate. M68K, Z80 and X86 use double buffering: they draw the frame off screen, then show it whole. MIPS and RISC-V draw straight on the bitmap display.

| Target | How it draws and waits |
| ------ | ---------------------- |
| M68K   | Draws off screen (task 92, mode 17), shows the frame with task 94, then waits two hundredths of a second with task 23. Uses the 640 × 480 screen. |
| MIPS   | Erases the ball and draws it again on the bitmap display, then sleeps for one frame (syscall 32). Every 64 frames it prints the elapsed time (syscall 30). The `@screen` comment at the top sets up a 512 × 512 display of 4-pixel units. |
| RISC-V | Same as MIPS, with ecalls 32 and 30. |
| X86    | Draws on the framebuffer's hidden page, then shows it. If the framebuffer has only one page, it draws on the visible page. Draws 600 frames at up to 60 per second, then ends. |
| Z80    | Draws off screen, shows the frame, then waits for the next frame on port `0x51`. Uses the 256 × 192 Screen. |
