SYNTH   equ $E00100             ; channel 0; channel 1 is at +$20 and channel 2 at +$40

    lea SYNTH, a0
    ; channel 0, the lead: a narrow pulse, struck, then held a little quieter
    move.b #0, $11(a0)          ; waveform 0: pulse
    move.w #1024, $02(a0)       ; pulse width: high a quarter of the time
    move.w #4, $04(a0)          ; attack
    move.w #200, $06(a0)        ; decay
    move.w #60, $08(a0)         ; release
    move.b #80, $12(a0)         ; volume
    move.b #70, $13(a0)         ; sustain
    ; channel 1, the bass: a triangle, held at full level
    move.b #1, $31(a0)          ; waveform 1: triangle
    move.w #40, $28(a0)         ; release
    move.b #127, $32(a0)        ; volume
    move.b #127, $33(a0)        ; sustain
    ; channel 2, the drums: noise with a short decay, its sustain left at 0
    move.b #4, $51(a0)          ; waveform 4: noise
    move.w #90, $46(a0)         ; decay
    move.b #100, $52(a0)        ; volume

    lea song, a1
step:
    move.w (a1)+, d2            ; the lead
    cmp.w #$FFFF, d2
    beq done
    lea SYNTH, a2
    bsr strike
    move.w (a1)+, d2            ; the bass
    lea SYNTH+$20, a2
    bsr strike
    move.w (a1)+, d2            ; the drum
    lea SYNTH+$40, a2
    bsr strike
    move.l #15, d1              ; wait 15 hundredths of a second per step
    move.b #23, d0              ; task 23: wait
    trap #15
    bra step

; Plays d2 Hz on the channel at a2; 0 leaves it alone. Leaves a1 unchanged.
strike:
    tst.w d2
    beq held
    move.b #0, $10(a2)          ; gate off, then on: restart the envelope
    move.w d2, (a2)
    move.b #1, $10(a2)
held:
    rts

done:
    move.l #60, d1              ; let the last chord ring
    move.b #23, d0
    trap #15
    move.b #9, d0               ; the program's end releases every gate
    trap #15

; Three words per step: lead Hz, bass Hz, drum noise Hz. Each long line holds four steps.
; 0 leaves that channel as it is; a lead of $FFFF ends the song.
song:
    dc.w 523, 131, 1200,   659, 0, 0,   784, 0, 24000,   659, 0, 0
    dc.w 523, 131, 7000,   659, 0, 0,   784, 0, 24000,   1047, 0, 0
    dc.w 440, 110, 1200,   523, 0, 0,   659, 0, 24000,   523, 0, 0
    dc.w 440, 110, 7000,   523, 0, 0,   659, 0, 24000,   880, 0, 0
    dc.w 349, 87, 1200,   440, 0, 0,   523, 0, 24000,   440, 0, 0
    dc.w 349, 87, 7000,   440, 0, 0,   523, 0, 24000,   698, 0, 0
    dc.w 392, 98, 1200,   494, 0, 0,   587, 0, 24000,   494, 0, 0
    dc.w 392, 98, 7000,   494, 0, 0,   587, 0, 24000,   784, 0, 0
    dc.w 523, 131, 1200
    dc.w $FFFF
