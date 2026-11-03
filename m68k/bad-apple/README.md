# Bad Apple!!

Bad Apple!!, the shadow-play video, on the M68K screen: 6,526 frames of 16 × 16 pixels, one byte a pixel, scaled up to the full height of the screen. Build, then Run.

A frame is drawn as filled rectangles (task 87), and only the cells that changed since the frame before are drawn again. About eight of the 256 change between frames, which is what keeps a frame within its time. `FPS` at the top sets the pace. Task 23 waits whole hundredths of a second, so at the video's own 30 frames a second a frame waits three hundredths.

## License

The program is under the MIT License, as the rest of asm-examples. Its frames are not: they come from "Bad Apple!!", a video by Anira to Alstroemeria Records' "Bad Apple!! feat. nomico", an arrangement of ZUN's music from the Touhou Project, and they belong to their authors.
