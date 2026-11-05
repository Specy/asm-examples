# Bad Apple!!

This program plays the Bad Apple!! video on the M68K screen. It stores 6,526 frames of 16 × 16 pixels, one byte per pixel, and scales them up to the height of the screen.

Each frame is drawn as filled rectangles (task 87). Only the cells that changed since the previous frame are drawn again, about eight of the 256 per frame, which keeps each frame cheap to draw.

`FPS` at the top of the program sets the frame rate. The video runs at 30 frames per second, so each frame waits three hundredths of a second with task 23, which waits in whole hundredths.

## License

The program is under the MIT License, as the rest of asm-examples. Its frames are not: they come from "Bad Apple!!", a video by Anira to Alstroemeria Records' "Bad Apple!! feat. nomico", an arrangement of ZUN's music from the Touhou Project, and they belong to their authors.
