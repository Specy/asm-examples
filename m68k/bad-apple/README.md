# Bad Apple!!

Build, then Run to watch Bad Apple!! on the M68K screen. This version stores 6,526 frames of 16 × 16 pixels, one byte a pixel, and scales them up to the full height of the screen.

A frame is drawn as filled rectangles (task 87). Only the cells that changed since the previous frame are drawn again: about eight of the 256 cells. Drawing fewer cells helps fit the work within the frame budget. `FPS` at the top sets the intended pace. Task 23 waits whole hundredths of a second, so at the video's own 30 frames a second each frame waits three hundredths.

## License

The program is under the MIT License, as the rest of asm-examples. Its frames are not: they come from "Bad Apple!!", a video by Anira to Alstroemeria Records' "Bad Apple!! feat. nomico", an arrangement of ZUN's music from the Touhou Project, and they belong to their authors.
