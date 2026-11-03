# Bouncing ball

A ball bouncing around the Screen. Every version draws a frame, shows it whole and then waits, so the ball is never seen half drawn and it moves at the same pace however fast the computer running the editor is. Build, then Run. It runs until you press Stop.

| Target | How it draws and waits                                                                                                                                                                                                                                  |
| ------ | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | Draws off screen (task 92, mode 17), shows the frame with task 94 and waits two hundredths of a second with task 23, on the 640 × 480 screen a program starts with.                                                                                     |
| MIPS   | Erases the ball and draws it again on the bitmap display, then sleeps for a frame (syscall 32). Every 64 frames it prints the program time that has passed (syscall 30). The `@screen` comment at the top sets up a 512 × 512 display of 4-pixel units. |
| RISC-V | As on MIPS, with ecalls 32 and 30.                                                                                                                                                                                                                      |
| Z80    | Draws off screen, presents the frame and waits for the next animation frame on port `0x51`, on the 256 × 192 Screen a program starts with.                                                                                                              |
