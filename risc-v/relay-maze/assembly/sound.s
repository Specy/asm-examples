# Original sound cues: (frame count, frequency in Hz) halfword pairs, terminated by 0, 0.
        .data
        .align 2
sound_voices: .space 36
sound_dot_phase: .word 0
background_mode: .word 0
background_step: .word 0
background_frames: .word 0
roam_melody: .half 392, 0, 494, 587, 440, 0, 523, 659, 392, 494, 587, 740, 659, 523, 440, 0
roam_bass: .half 98, 98, 98, 98, 110, 110, 110, 110, 98, 98, 98, 98, 131, 131, 131, 131
power_melody: .half 587, 740, 880, 740, 659, 830, 988, 830, 587, 740, 880, 1175, 988, 830, 740, 659
power_bass: .half 147, 147, 196, 196, 165, 165, 220, 220, 147, 147, 196, 196, 175, 175, 247, 247
music_bass: .half 10, 196, 10, 247, 10, 294, 14, 196, 8, 220, 12, 294, 0, 0
music_melody: .half 6, 587, 6, 740, 8, 880, 4, 740, 8, 659, 8, 784, 12, 988, 12, 880, 0, 0
sound_dot_a: .half 2, 960, 2, 1120, 0, 0
sound_dot_b: .half 2, 840, 2, 1040, 0, 0
sound_bonus: .half 5, 659, 5, 830, 9, 1109, 0, 0
sound_enemy: .half 4, 392, 4, 587, 7, 784, 0, 0
sound_death: .half 10, 440, 9, 370, 9, 294, 12, 220, 14, 147, 0, 0
sound_pop: .half 5, 98, 0, 0
sound_extra_life: .half 7, 523, 7, 659, 7, 784, 14, 1047, 0, 0

        .text
init_sounds:
        li t0, SYNTH
        li t1, CHANNEL_COUNT
init_sound_channel:
        li t2, 1
        sb t2, SY_WAVEFORM(t0)
        sb t2, SY_CONTROL(t0)
        li t2, 127
        sb t2, SY_SUSTAIN(t0)
        sb zero, SY_VOLUME(t0)
        addi t0, t0, SY_CHANNEL
        addi t1, t1, -1
        bnez t1, init_sound_channel
        ret

# play_sound / play_song(a0=channel,a1=cue).
play_song:
play_sound:
        li t0, 12
        mul t0, a0, t0
        la t1, sound_voices
        add t0, t0, t1
        sw a1, 0(t0)
        sw zero, 4(t0)
        ret
stop_sounds:
        la t0, background_mode
        sw zero, 0(t0)
        la t0, background_step
        sw zero, 0(t0)
        la t0, background_frames
        sw zero, 0(t0)
        la t0, sound_voices
        li t1, SYNTH
        li t2, CHANNEL_COUNT
stop_sound_channel:
        sw zero, 0(t0)
        sw zero, 4(t0)
        sb zero, SY_VOLUME(t1)
        addi t0, t0, 12
        addi t1, t1, SY_CHANNEL
        addi t2, t2, -1
        bnez t2, stop_sound_channel
        ret
play_chomp:
        la t0, sound_dot_phase
        lw t1, 0(t0)
        addi t2, t1, 1
        sw t2, 0(t0)
        andi t1, t1, 1
        la a1, sound_dot_a
        beqz t1, play_dot
        la a1, sound_dot_b
play_dot:
        li a0, CHANNEL_3
        j play_sound
update_sounds:
        la t0, paused
        lw t0, 0(t0)
        bnez t0, sound_begin_channels
        li t2, 0
        la t0, game_state
        lw t0, 0(t0)
        li t1, ST_PLAY
        beq t0, t1, sound_select_theme
        li t1, ST_EAT_ENEMY
        bne t0, t1, sound_check_theme
sound_select_theme:
        li t2, 1
        la t0, fright_timer
        lw t0, 0(t0)
        beqz t0, sound_check_theme
        li t2, 2
sound_check_theme:
        la t0, background_mode
        lw t1, 0(t0)
        beq t1, t2, sound_begin_channels
        sw t2, 0(t0)
        la t0, background_step
        sw zero, 0(t0)
        li t1, 0
        beqz t2, sound_reset_theme
        li t1, 10
        li t0, 1
        beq t2, t0, sound_reset_theme
        li t1, 6
sound_reset_theme:
        la t0, background_frames
        sw t1, 0(t0)
sound_begin_channels:
        li a0, 0
        la a1, sound_voices
        li a2, SYNTH
sound_channel:
        li a3, 0                            # Hz
        li a4, 0                            # volume
        la t0, paused
        lw t0, 0(t0)
        bnez t0, sound_write
        lw t0, 0(a1)                        # next note pair
        beqz t0, sound_background
        lw t1, 4(a1)                        # frames left
        bnez t1, sound_active
        lhu t1, 0(t0)
        beqz t1, sound_end
        lhu t2, 2(t0)
        sw t2, 8(a1)
        addi t0, t0, 4
        sw t0, 0(a1)
sound_active:
        addi t1, t1, -1
        sw t1, 4(a1)
        lw a3, 8(a1)
        li a4, 64
        j sound_write
sound_end:
        sw zero, 0(a1)
sound_background:
        li t0, CHANNEL_3
        beq a0, t0, sound_write
        la t0, background_mode
        lw t0, 0(t0)
        beqz t0, sound_write
        li t1, 2
        beq t0, t1, sound_power_theme
        la t1, roam_bass
        li t2, 24
        beqz a0, sound_theme_note
        la t1, roam_melody
        li t2, 32
        j sound_theme_note
sound_power_theme:
        la t1, power_bass
        li t2, 28
        beqz a0, sound_theme_note
        la t1, power_melody
        li t2, 44
sound_theme_note:
        la t0, background_step
        lw t0, 0(t0)
        slli t0, t0, 1
        add t1, t1, t0
        lhu a3, 0(t1)
        beqz a3, sound_write
        mv a4, t2
sound_write:
        sh a3, SY_FREQUENCY(a2)
        sb a4, SY_VOLUME(a2)
        addi a0, a0, 1
        addi a1, a1, 12
        addi a2, a2, SY_CHANNEL
        li t0, CHANNEL_COUNT
        blt a0, t0, sound_channel
        la t0, paused
        lw t0, 0(t0)
        bnez t0, sound_done
        la t0, background_mode
        lw t0, 0(t0)
        beqz t0, sound_done
        la t1, background_frames
        lw t2, 0(t1)
        addi t2, t2, -1
        sw t2, 0(t1)
        bnez t2, sound_done
        la t2, background_step
        lw t3, 0(t2)
        addi t3, t3, 1
        andi t3, t3, 15
        sw t3, 0(t2)
        li t2, 10
        li t3, 1
        beq t0, t3, sound_next_step
        li t2, 6
sound_next_step:
        sw t2, 0(t1)
sound_done:
        ret
