/*
 * The pixel loops in C, for a machine whose platform has no faster ones (PLATFORM_BLIT): the
 * same three calls os/platform/x86/blit.asm makes of x86's `rep` instructions. A stride is the
 * distance in bytes from the start of one row to the start of the next.
 */
#if !PLATFORM_BLIT

#define BLIT_ROW(picture, stride, row) ((uint32_t *)((uint8_t *)(picture) + (row) * (stride)))

void fill_rect32(uint32_t *to, size_t stride, size_t width, size_t height, uint32_t colour) {
    for (size_t y = 0; y < height; y++) {
        uint32_t *row = BLIT_ROW(to, stride, y);
        for (size_t x = 0; x < width; x++) row[x] = colour;
    }
}

/* A from_stride of 0 copies the same row into every row of the destination. */
void copy_rect32(uint32_t *to, size_t to_stride, const uint32_t *from, size_t from_stride,
                 size_t width, size_t height) {
    for (size_t y = 0; y < height; y++)
        memcpy(BLIT_ROW(to, to_stride, y), BLIT_ROW(from, from_stride, y), width * 4);
}

/* Within one picture, when the two rectangles may overlap, as a window dragged a few pixels does:
 * rows go bottom-up when moving down, so that no row is overwritten before it is read, and
 * memmove takes care of each row. */
void move_rect32(uint32_t *to, const uint32_t *from, size_t stride, size_t width, size_t height) {
    if (to <= from)
        for (size_t y = 0; y < height; y++)
            memmove(BLIT_ROW(to, stride, y), BLIT_ROW(from, stride, y), width * 4);
    else
        for (size_t y = height; y-- > 0;)
            memmove(BLIT_ROW(to, stride, y), BLIT_ROW(from, stride, y), width * 4);
}

#endif
