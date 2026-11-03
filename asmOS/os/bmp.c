/*
 * BMP pictures: the uncompressed 24- and 32-bit kind Paint writes and most tools can write. A file
 * is a 54-byte header, then the rows, bottom row first unless the height is negative, each row
 * padded to a multiple of four bytes, each pixel blue, green, red (and alpha).
 */

static long bmp_get32(const unsigned char *at) {
    return (long)(int32_t)(at[0] | at[1] << 8 | at[2] << 16 | (uint32_t)at[3] << 24);
}

static void bmp_put32(unsigned char *at, long value) {
    for (int i = 0; i < 4; i++) at[i] = (unsigned char)(value >> (8 * i));
}

/* The picture, in memory from `allocate`. */
static uint32_t *bmp_decode_with(const char *data, long size, int *w, int *h,
                                 void *(*allocate)(size_t)) {
    const unsigned char *bytes = (const unsigned char *)data;
    if (size < 54 || bytes[0] != 'B' || bytes[1] != 'M') return NULL;
    long offset = bmp_get32(bytes + 10), width = bmp_get32(bytes + 18), height = bmp_get32(bytes + 22);
    int bits = bytes[28] | bytes[29] << 8, compression = (int)bmp_get32(bytes + 30);
    if ((bits != 24 && bits != 32) || (compression != 0 && compression != 3)) return NULL;
    int top_down = height < 0;
    if (top_down) height = -height;
    if (width <= 0 || height <= 0 || width > 4096 || height > 4096) return NULL;
    long row_bytes = (width * (bits / 8) + 3) & ~3;
    if (offset + row_bytes * height > size) return NULL;
    uint32_t *pixels = allocate((size_t)width * height * 4);
    if (!pixels) return NULL;
    for (long y = 0; y < height; y++) {
        const unsigned char *row = bytes + offset + (top_down ? y : height - 1 - y) * row_bytes;
        for (long x = 0; x < width; x++, row += bits / 8)
            pixels[y * width + x] = (uint32_t)(row[2] << 16 | row[1] << 8 | row[0]);
    }
    *w = (int)width;
    *h = (int)height;
    return pixels;
}

static void *allocate_kernel(size_t size) { return malloc(size); }

uint32_t *bmp_decode(const char *data, long size, int *w, int *h) {
    return bmp_decode_with(data, size, w, h, allocate_kernel);
}

uint32_t *os_bmp_decode(const char *data, long size, int *w, int *h) {
    return bmp_decode_with(data, size, w, h, os_alloc);
}

char *os_bmp_encode(const uint32_t *pixels, int w, int h, long *size) {
    long row_bytes = ((long)w * 3 + 3) & ~3, length = 54 + row_bytes * h;
    unsigned char *file = os_alloc(length);
    if (!file) return NULL;
    file[0] = 'B';
    file[1] = 'M';
    bmp_put32(file + 2, length);
    bmp_put32(file + 10, 54);
    bmp_put32(file + 14, 40);
    bmp_put32(file + 18, w);
    bmp_put32(file + 22, h);
    file[26] = 1;
    file[28] = 24;
    bmp_put32(file + 34, row_bytes * h);
    bmp_put32(file + 38, 2835);
    bmp_put32(file + 42, 2835);
    for (int y = 0; y < h; y++) {
        unsigned char *row = file + 54 + (long)(h - 1 - y) * row_bytes;
        for (int x = 0; x < w; x++) {
            uint32_t pixel = pixels[y * w + x];
            row[x * 3] = pixel & 0xff;
            row[x * 3 + 1] = pixel >> 8 & 0xff;
            row[x * 3 + 2] = pixel >> 16 & 0xff;
        }
    }
    *size = length;
    return (char *)file;
}
