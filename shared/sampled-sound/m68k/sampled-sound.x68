SYNTH   equ $E00100
COUNT   equ 4000                ; samples: half a second at 8,000 a second

    ; compute a plucked string: a square wave of 32 samples a period, fading away
    lea pluck, a0
    moveq #0, d1                ; the sample's number
fill:
    move.w d1, d2
    lsr.w #5, d2                ; which period it is in: its number / 32
    moveq #125, d3
    sub.w d2, d3                ; the amplitude, falling by 1 a period
    btst #4, d1                 ; the first half of each period is high,
    beq store
    neg.w d3                    ; the second half low
store:
    move.b d3, (a0)+
    addq.w #1, d1
    cmp.w #COUNT, d1
    blt fill

    ; copy the samples into the Synth's Sample memory
    lea SYNTH, a0
    move.l #pluck, $180(a0)     ; from the buffer
    move.w #COUNT, $184(a0)     ; 4,000 bytes
    move.w #0, $186(a0)         ; to the start of Sample memory
    move.b #0, $188(a0)         ; now

    ; channel 0 plays them
    move.w #0, $0A(a0)          ; from sample 0
    move.w #COUNT, $0C(a0)      ; for 4,000 samples, then silence
    move.b #6, $11(a0)          ; waveform 6: sample
    move.b #120, $12(a0)        ; volume
    move.b #127, $13(a0)        ; sustain: the samples fade by themselves

    lea rates, a1
next:
    move.w (a1)+, d2            ; the next rate
    beq done
    move.b #0, $10(a0)          ; gate off, then on: from the first sample again
    move.w d2, (a0)
    move.b #1, $10(a0)
    move.l #30, d1              ; 30 hundredths of a second a note
    move.b #23, d0              ; task 23: wait
    trap #15
    bra next

done:
    move.b #9, d0
    trap #15

rates:                          ; samples a second for each note: 8000 plays the sound as computed
    dc.w 8000, 8980, 10079, 10679, 11986, 13454, 15102, 16000, 0
pluck:
    ds.b COUNT
