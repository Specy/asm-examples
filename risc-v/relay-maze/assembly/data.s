# Game state, picture buffers and difficulty settings.

        .data
# ---------------------------------------------------------------- picture buffers
# Both share the Screen's layout: a row is ROW_BYTES, so one offset, y * ROW_BYTES + x * 4,
# names the same pixel in each of them and on the Screen.
background:     .space  SCREEN_BYTES            # maze, dots and text, without sprites
maze_image:     .space  SCREEN_BYTES            # maze.bmp, unpacked to words, from row 24
sprite_sheet:   .space  147456                  # sprites.bmp, unpacked: 256 by 144 words
tile_sheet:     .space  20480                   # tiles.bmp, unpacked: 128 by 40 words
file_buffer:    .space  262144                  # the raw bytes of the file being loaded
wall_pixels:    .space  131072                  # offsets of the maze picture's lit pixels

        .align  2
file_length:    .word   0
wall_pixel_count:.word  0

# ---------------------------------------------------------------- the maze
maze_layout:    .space  868                     # maze.txt as read, one byte a tile
tiles:          .space  868                     # this level's copy, which loses its dots
        .align  2
dots_total:     .word   0
energizers:     .space  64                      # (column, row) word pairs, at most 8
energizer_count:.word   0

# ---------------------------------------------------------------- the game
game_state:     .word   ST_TITLE
state_timer:    .word   0                       # frames spent in the current state
frame_count:    .word   0
level:          .word   1
level_ptr:      .word   0                       # this level's entry in level_table
score:          .word   0
high_score:     .word   0
lives:          .word   0                       # spare lives, drawn at the bottom left
extra_life_given:.word  0
dots_left:      .word   0
dots_eaten:     .word   0
mode_index:     .word   0                       # even: scatter, odd: chase
mode_timer:     .word   0                       # frames left in this mode, 0 for ever
fright_timer:   .word   0
enemies_eaten:   .word   0                       # since the last energizer
eaten_enemy:    .word   0                       # the enemy record shown as its score
global_mode:    .word   0                       # 1 after a death: one dot counter for all
global_dots:    .word   0
dot_timer:      .word   0                       # frames since the player last ate a dot
pursuit_enabled:  .word   1
bonus_timer:    .word   0
bonus_score_timer:.word 0
paused:         .word   0
key_pressed:    .word   0
pause_requested:.word   0
show_actors:    .word   0                       # the player and the enemies are on the maze
ready_intro:    .word   0                       # frames of PLAYER ONE before READY!
energizers_lit: .word   1
oneup_lit:      .word   1
rng_state:      .word   0x2545f491
next_deadline:  .word   0
deadline_thirds:.word   0

player_x:          .word   0
player_y:          .word   0
player_dir:        .word   LEFT
player_want:       .word   LEFT                    # the direction the player asked for
player_acc:        .word   0
player_stall:      .word   0                       # frames to stand still after eating
player_moving:     .word   0
player_anim:       .word   0                       # pixels moved, for the animation

enemies:         .space  256                     # ENEMY_COUNT records of G_SIZE bytes
slots:          .space  192                     # SLOT_COUNT records of SP_SIZE bytes

# ---------------------------------------------------------------- tables
dir_dx:         .word   1, -1, 0, 0             # indexed by direction
dir_dy:         .word   0, 0, -1, 1
choice_order:   .word   UP, LEFT, DOWN, RIGHT   # direction order used when distances tie

# Chaser, Ambusher, Scout, Roamer: x, y, direction, state, home x, scatter corner x and y (in tiles),
# and the row of sprites.bmp they are drawn from.
enemy_start:    .word   111, 115, LEFT, GS_ACTIVE, 111, 25, 0, 2
                .word   111, 139, DOWN, GS_HOUSE, 111, 2, 0, 3
                .word   95, 139, UP, GS_HOUSE, 95, 27, 35, 4
                .word   127, 139, UP, GS_HOUSE, 127, 0, 35, 5

# Original scatter/chase timings and difficulty settings.
modes_intro: .word 360, 1050, 300, 1200, 240, 1500, 180
modes_regular: .word 300, 1200, 240, 1500, 180, 1800, 120
modes_fast: .word 240, 1500, 180, 1800, 120, 2100, 60

level_table:
        .word 0, 150, 256, 232, 120, 36, 248, 14, 264, 288, 144, 420, 5, modes_intro, 24, 48
        .word 1, 350, 272, 248, 128, 40, 264, 18, 280, 304, 152, 360, 5, modes_regular, 16, 40
        .word 2, 550, 288, 264, 136, 44, 280, 20, 296, 320, 160, 300, 5, modes_regular, 8, 32
        .word 3, 850, 304, 280, 144, 48, 296, 22, 312, 328, 168, 240, 4, modes_fast, 0, 24
        .word 4, 1250, 312, 288, 152, 52, 304, 24, 320, 336, 176, 210, 4, modes_fast, 0, 16
        .word 5, 1750, 320, 296, 160, 56, 312, 26, 328, 344, 184, 180, 3, modes_fast, 0, 8

# ---------------------------------------------------------------- text
name_maze_txt:  .asciz  "assets/maze.txt"            # Project paths, from the Project's root
name_maze_bmp:  .asciz  "assets/maze.bmp"
name_sprites:   .asciz  "assets/sprites.bmp"
name_tiles:     .asciz  "assets/tiles.bmp"

text_1up:       .asciz  "1UP"
text_high:      .asciz  "HIGH SCORE"
text_ready:     .asciz  "READY!"
text_player:    .asciz  "PLAYER ONE"
text_game_over: .asciz  "GAME  OVER"
text_title:     .asciz "RELAY MAZE"
text_push:      .asciz  "PUSH SPACE"
text_paused:    .asciz  "PAUSED"

text_banner:    .ascii  "Relay Maze for RISC-V\n"
                .ascii  "Click the Screen so it gets the keyboard, then press space.\n"
                .ascii  "Move with W A S D (Z Q S D and 8 4 5 6 work too), P pauses.\n"
                .asciz  "The arrow keys do not type characters, so the game cannot see them.\n"
text_error:     .asciz  "player: "
text_cannot_open:.asciz ": cannot open it. It has to be a File of this Project.\n"
text_too_big:   .asciz  ": it is larger than the 256 KB the program reads.\n"
text_not_bmp:   .asciz  ": not a BMP picture.\n"
text_bad_header:.asciz  ": the BMP header is not one this program reads (save it as a plain 24 bit BMP).\n"
text_bad_size:  .asciz  ": the picture has the wrong size. It must be "
text_by:        .asciz  " by "
text_pixels:    .asciz  " pixels.\n"
text_bad_depth: .asciz  ": save it as a 24 or 32 bit BMP, without compression.\n"
text_truncated: .asciz  ": the file ends before its last row of pixels.\n"
text_bad_line:  .asciz  ": every line must be 28 characters long; this one is not: line "
text_bad_rows:  .asciz  ": the maze must be 31 lines of 28 characters.\n"
text_newline:   .asciz  "\n"
