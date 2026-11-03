/*
 * The icons, drawn one character per pixel. A space is transparent; every other character is a
 * colour of icon_colour below. The desktop shows them twice their size, the taskbar, the title
 * bars and the Start menu at their own.
 */

static uint32_t icon_colour(char c) {
    switch (c) {
    case 'k': return BLACK;
    case 'w': return WHITE;
    case 'g': return FACE;
    case 'd': return SHADOW;
    case 'l': return LIGHT;
    case 'b': return 0x0000ff;
    case 'n': return NAVY;
    case 'c': return 0x00ffff;
    case 't': return 0x008080;
    case 'r': return 0xff0000;
    case 'm': return 0x800000;
    case 'y': return 0xffff00;
    case 'o': return 0xc0a000;
    case 'e': return 0x00c000;
    case 's': return 0xa05a2c;
    case 'p': return 0x800080;
    }
    return 0xff00ff;
}

void draw_icon(const Icon *icon, int x, int y, int scale) {
    int ox, oy;
    gfx_get_origin(&ox, &oy);
    if (!rect_overlaps(rect(x + ox, y + oy, 16 * scale, 16 * scale), gfx_get_clip())) return;
    for (int row = 0; row < 16; row++) {
        const char *pixels = icon->rows[row];
        for (int column = 0; column < 16; column++) {
            char c = pixels[column];
            if (c == ' ') continue;
            /* a run of one colour is one rectangle */
            int run = 1;
            while (column + run < 16 && pixels[column + run] == c) run++;
            fill_rect(x + column * scale, y + row * scale, run * scale, scale, icon_colour(c));
            column += run - 1;
        }
    }
}

const Icon ICON_NOTEPAD = { {
    "   d d d d d    ",
    "  kdkdkdkdkdk   ",
    "  kwwwwwwwwwwk  ",
    "  kwbbbbbbbbwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwbbbbbbbbwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwbbbbbbbbwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwbbbbbbbbwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwbbbbbwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwkd ",
    "  kkkkkkkkkkkkd ",
    "   dddddddddddd ",
} };

const Icon ICON_CALCULATOR = { {
    " kkkkkkkkkkkkk  ",
    " kgggggggggggkd ",
    " kgkkkkkkkkkgkd ",
    " kgkeeeeeeekgkd ",
    " kgkeeeeeeekgkd ",
    " kgkkkkkkkkkgkd ",
    " kgggggggggggkd ",
    " kggwwgwwgrrgkd ",
    " kggkkgkkgkkgkd ",
    " kgggggggggggkd ",
    " kggwwgwwgrrgkd ",
    " kggkkgkkgkkgkd ",
    " kgggggggggggkd ",
    " kggwwgwwgrrgkd ",
    " kggkkgkkgkkgkd ",
    " kkkkkkkkkkkkkd ",
} };

const Icon ICON_PAINT = { {
    "           kk   ",
    "          kssk  ",
    "         kssk   ",
    "        kssk    ",
    "       kssk     ",
    "      kddk      ",
    "     kddk       ",
    "    kkkk        ",
    "   krrk         ",
    "  krrk          ",
    " krrk           ",
    " kkk            ",
    "                ",
    "  rr  yy  bb    ",
    " rrrr yyyybbbb  ",
    "  rr  yy  bb    ",
} };

const Icon ICON_MINES = { {
    "                ",
    "       k        ",
    "       k        ",
    "   k kkkkk k    ",
    "    kkkkkkk     ",
    "   kkwwkkkkk    ",
    "   kkwwkkkkk    ",
    " kkkkkkkkkkkkk  ",
    "   kkkkkkkkk    ",
    "   kkkkkkkkk    ",
    "    kkkkkkk     ",
    "   k kkkkk k    ",
    "       k        ",
    "       k        ",
    "                ",
    "                ",
} };

const Icon ICON_CLOCK = { {
    "     kkkkkk     ",
    "   kkwwwwwwkk   ",
    "  kwwwwkwwwwwk  ",
    " kwwwwwkwwwwwwk ",
    " kwwwwwkwwwwwwk ",
    "kwwwwwwkwwwwwwwk",
    "kwwwwwwkwwwwwwwk",
    "kwwwwwwkkkkkwwwk",
    "kwwwwwwwwwwwwwwk",
    "kwwwwwwwwwwwwwwk",
    " kwwwwwwwwwwwwk ",
    " kwwwwwwwwwwwwk ",
    "  kwwwwwwwwwwk  ",
    "   kkwwwwwwkk   ",
    "     kkkkkk     ",
    "                ",
} };

const Icon ICON_TERMINAL = { {
    "                ",
    " kkkkkkkkkkkkkk ",
    " knnnnnnnnnnnnk ",
    " kkkkkkkkkkkkkk ",
    " kkkkkkkkkkkkkk ",
    " kkekkkkkkkkkkk ",
    " kkkekkkkkkkkkk ",
    " kkkkekkkkkkkkk ",
    " kkkekkkkkkkkkk ",
    " kkekkkeeeekkkk ",
    " kkkkkkkkkkkkkk ",
    " kkkkkkkkkkkkkk ",
    " kkkkkkkkkkkkkk ",
    " kkkkkkkkkkkkkk ",
    "                ",
    "                ",
} };

const Icon ICON_FOLDER = { {
    "                ",
    "                ",
    "  kkkkk         ",
    " kwyyyyk        ",
    "kwyyyyyykkkkkkk ",
    "kwyyyyyyyyyyyyyk",
    "kyyyyyyyyyyyyyok",
    "kyyyyyyyyyyyyyok",
    "kyyyyyyyyyyyyyok",
    "kyyyyyyyyyyyyyok",
    "kyyyyyyyyyyyyyok",
    "kyyyyyyyyyyyyyok",
    "kooooooooooooook",
    "kkkkkkkkkkkkkkkk",
    "                ",
    "                ",
} };

const Icon ICON_COMPUTER = { {
    "                ",
    "  kkkkkkkkkkkk  ",
    "  kggggggggggk  ",
    "  kgkkkkkkkkgk  ",
    "  kgkttttttkgk  ",
    "  kgkttttttkgk  ",
    "  kgkttttttkgk  ",
    "  kgkttttttkgk  ",
    "  kgkkkkkkkkgk  ",
    "  kggggggggegk  ",
    "  kkkkkkkkkkkk  ",
    "      kggk      ",
    "    kkkkkkkk    ",
    "   kggggggggk   ",
    "   kkkkkkkkkk   ",
    "                ",
} };

const Icon ICON_CHIP = { {
    "                ",
    "     d d d d    ",
    "     d d d d    ",
    "   kkkkkkkkkk   ",
    " ddknnnnnnnnkdd ",
    "   knnnnnnnnk   ",
    " ddknnyyyynnkdd ",
    "   knnyyyynnk   ",
    " ddknnyyyynnkdd ",
    "   knnyyyynnk   ",
    " ddknnnnnnnnkdd ",
    "   knnnnnnnnk   ",
    "   kkkkkkkkkk   ",
    "     d d d d    ",
    "     d d d d    ",
    "                ",
} };

const Icon ICON_PICTURE = { {
    "                ",
    " kkkkkkkkkkkkkk ",
    " kccccccccccyyk ",
    " kcccccccccyyyk ",
    " kccwwcccccyyyk ",
    " kcwwwwccccccck ",
    " kcccccccccccck ",
    " kcccccccekkkck ",
    " kccccccceeeeck ",
    " kcceccceeeeeek ",
    " kceeeceeeeeeek ",
    " keeeeeeeeeeeek ",
    " kkkkkkkkkkkkkk ",
    "                ",
    "                ",
    "                ",
} };

const Icon ICON_SHUTDOWN = { {
    "                ",
    "       rr       ",
    "   rr  rr  rr   ",
    "  rr   rr   rr  ",
    " rr    rr    rr ",
    " rr    rr    rr ",
    " rr          rr ",
    " rr          rr ",
    " rr          rr ",
    "  rr        rr  ",
    "   rr      rr   ",
    "    rrrrrrrr    ",
    "                ",
    "                ",
    "                ",
    "                ",
} };

const Icon ICON_PACMAN = { {
    "                ",
    "     yyyyy      ",
    "   yyyyyyyyy    ",
    "  yyyyyyyyyyy   ",
    " yyyyyyyyyyy    ",
    " yyyyyyyyy      ",
    "yyyyyyyy        ",
    "yyyyyy      ww  ",
    "yyyyyyyy        ",
    " yyyyyyyyy      ",
    " yyyyyyyyyyy    ",
    "  yyyyyyyyyyy   ",
    "   yyyyyyyyy    ",
    "     yyyyy      ",
    "                ",
    "                ",
} };

const Icon ICON_BLOCKS = { {
    "    bbbkbbbk    ",
    "    bbbkbbbk    ",
    "    bbbkbbbk    ",
    "    kkkkkkkk    ",
    "    bbbkyyykyyyk",
    "    bbbkyyykyyyk",
    "    bbbkyyykyyyk",
    "    kkkkkkkkkkkk",
    "rrrkrrrkyyykyyyk",
    "rrrkrrrkyyykyyyk",
    "rrrkrrrkyyykyyyk",
    "kkkkkkkkkkkkkkkk",
    "rrrkrrrkeeekeeek",
    "rrrkrrrkeeekeeek",
    "rrrkrrrkeeekeeek",
    "kkkkkkkkkkkkkkkk",
} };

const Icon ICON_DOOM = { {
    "     mmmmmm     ",
    "   mmrrrrrrmm   ",
    "  mrrrrrrrrrrm  ",
    " mrrrrwwwwrrrrm ",
    " mrrrwweewwrrrm ",
    "mrrrrweekewrrrrm",
    "mrrrrwweewwrrrrm",
    "mrrrrrwwwwrrrrrm",
    "mrrrrrrrrrrrrrrm",
    "mrrkkkkkkkkkkrrm",
    "mrrkwkwkwkwkkrrm",
    "mrrkkkkkkkkkkrrm",
    " mrrkwkwkwkwkrm ",
    " mrrrkkkkkkkrrm ",
    "  mmrrrrrrrrmm  ",
    "    mmmmmmmm    ",
} };

const Icon ICON_VIEWER = { {
    "                ",
    "nnnnnnnnnnnnnnnn",
    "nwwwwwwwwwwwwwwn",
    "nwccccccccccyywn",
    "nwcccccccccyyywn",
    "nwccwwccccccccwn",
    "nwcwwwwcccccccwn",
    "nwccccccceccccwn",
    "nwcccccceeeeccwn",
    "nwccecceeeeeecwn",
    "nweeeeeeeeeeeewn",
    "nwwwwwwwwwwwwwwn",
    "nnnnnnnnnnnnnnnn",
    "      nnnn      ",
    "    nnnnnnnn    ",
    "                ",
} };

const Icon ICON_MUSIC = { {
    "                ",
    "      ppppppppp ",
    "      ppppppppp ",
    "      p       p ",
    "      p       p ",
    "      p       p ",
    "      p       p ",
    "      p       p ",
    "      p       p ",
    "      p    pppp ",
    "   pppp   ppppp ",
    "  ppppp   ppppp ",
    "  ppppp    ppp  ",
    "   ppp          ",
    "                ",
    "                ",
} };

const Icon ICON_VIDEO = { {
    "kkkkkkkkkkkkkkkk",
    "kwkkwkkwkkwkkwkk",
    "kkkkkkkkkkkkkkkk",
    "kwwwwwwwwwwwwwwk",
    "kwwwwwwwkwwwwwwk",
    "kwwwwwwkweewwwwk",
    "kwwwkkkwkkkwwwwk",
    "kwwkkkkkkkkkwwwk",
    "kwwkkkkkkkkkwwwk",
    "kwwkkkkkkkkkwwwk",
    "kwwkkkkkkkkkwwwk",
    "kwwwkkkkkkkwwwwk",
    "kwwwwkkwkkwwwwwk",
    "kwwwwwwwwwwwwwwk",
    "kkkkkkkkkkkkkkkk",
    "kwkkwkkwkkwkkwkk",
} };

const Icon ICON_HEX = { {
    "  kkkkkkkkkk    ",
    "  kwwwwwwwwkk   ",
    "  kwwwwwwwwkwk  ",
    "  kwwwwwwwwkkkk ",
    "  kwbbwbbwwwwwk ",
    "  kwwwwwwwwwwwk ",
    "  kwbbwbbwbbwwk ",
    "  kwwwwwwwwwwwk ",
    "  kwbbwbbwwwwwk ",
    "  kwwwwwwwwwwwk ",
    "  kwbbwbbwbbwwk ",
    "  kwwwwwwwwwwwk ",
    "  kwbbwwwwwwwwk ",
    "  kwwwwwwwwwwwk ",
    "  kkkkkkkkkkkkk ",
    "                ",
} };

/* The Assembly Editor: a code window with its line gutter, a breakpoint and the current line. */
const Icon ICON_IDE = { {
    "                ",
    " kkkkkkkkkkkkkk ",
    " knnnnnnnnnnwnk ",
    " kkkkkkkkkkkkkk ",
    " kgggkwwwwwwwwk ",
    " kgggkwbbbwttwk ",
    " kgrgkwwwwwwwwk ",
    " krrrkwwbbwtttk ",
    " kgrgkwwwwwwwwk ",
    " kgggkyybbbyyyk ",
    " kgggkwwwwwwwwk ",
    " kgggkwwbbwwmmk ",
    " kgggkwwwwwwwwk ",
    " kgggkweeeeewwk ",
    " kkkkkkkkkkkkkk ",
    "                ",
} };


const Icon ICON_TASKS = { {
    "                ",
    " kkkkkkkkkkkkkk ",
    " kkkkkkkkkkkkkk ",
    " kkkkkkkkkkkkek ",
    " kkkkkkkkkkkekk ",
    " kkkkkkekkkkekk ",
    " kkkkkkeekkekkk ",
    " kekkkekkekekkk ",
    " kkekekkkkekkkk ",
    " kkkekkkkkkkkkk ",
    " kkkkkkkkkkkkkk ",
    " kkkkkkkkkkkkkk ",
    "       gg       ",
    "    gggggggg    ",
    "                ",
    "                ",
} };

const Icon ICON_SETTINGS = { {
    "      dddd      ",
    "  dd  dddd  dd  ",
    " dddddddddddddd ",
    "  dddddddddddd  ",
    "  dddd    dddd  ",
    "dddddd    dddddd",
    "ddddd      ddddd",
    "ddddd      ddddd",
    "ddddd      ddddd",
    "ddddd      ddddd",
    "dddddd    dddddd",
    "  dddd    dddd  ",
    "  dddddddddddd  ",
    " dddddddddddddd ",
    "  dd  dddd  dd  ",
    "      dddd      ",
} };

const Icon ICON_FILE = { {
    "  kkkkkkkk      ",
    "  kwwwwwwkk     ",
    "  kwwwwwwkwk    ",
    "  kwwwwwwkwwk   ",
    "  kwwwwwwkkkkk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kwwwwwwwwwwk  ",
    "  kkkkkkkkkkkk  ",
    "                ",
} };

const Icon ICON_WARNING = { {
    "       kk       ",
    "      kyyk      ",
    "      kyyk      ",
    "     kyyyyk     ",
    "     kykkyk     ",
    "    kyykkyyk    ",
    "    kyykkyyk    ",
    "   kyyykkyyyk   ",
    "   kyyykkyyyk   ",
    "  kyyyykkyyyyk  ",
    "  kyyyyyyyyyyk  ",
    " kyyyyykkyyyyyk ",
    " kyyyyykkyyyyyk ",
    "kyyyyyyyyyyyyyyk",
    "kkkkkkkkkkkkkkkk",
    "                ",
} };

const Icon ICON_INFO = { {
    "     bbbbbb     ",
    "   bbbbbbbbbb   ",
    "  bbbbbwwbbbbb  ",
    " bbbbbbwwbbbbbb ",
    " bbbbbbbbbbbbbb ",
    "bbbbbbwwwbbbbbbb",
    "bbbbbbbwwbbbbbbb",
    "bbbbbbbwwbbbbbbb",
    "bbbbbbbwwbbbbbbb",
    "bbbbbbbwwbbbbbbb",
    " bbbbbbwwbbbbbb ",
    " bbbbbwwwwbbbbb ",
    "  bbbbbbbbbbbb  ",
    "   bbbbbbbbbb   ",
    "     bbbbbb     ",
    "                ",
} };

const Icon ICON_QUESTION = { {
    "     bbbbbb     ",
    "   bbbbbbbbbb   ",
    "  bbbbwwwwbbbb  ",
    " bbbbwwbbwwbbbb ",
    " bbbbbbbbwwbbbb ",
    "bbbbbbbbwwbbbbbb",
    "bbbbbbbwwbbbbbbb",
    "bbbbbbbwwbbbbbbb",
    "bbbbbbbbbbbbbbbb",
    "bbbbbbbwwbbbbbbb",
    " bbbbbbwwbbbbbb ",
    " bbbbbbbbbbbbbb ",
    "  bbbbbbbbbbbb  ",
    "   bbbbbbbbbb   ",
    "     bbbbbb     ",
    "                ",
} };
