RATE    equ 8000                ; samples a second
LENGTH  equ 4000                ; half a second of samples
HALF    equ 10                  ; samples at each level: 8000 / 20 = 400 Hz

    lea samples, a0             ; fill the samples with a square wave
    move.w #LENGTH/(2*HALF)-1, d1
wave:
    moveq #HALF-1, d2
high:
    move.b #$C0, (a0)+          ; 8-bit samples are unsigned: $80 is silence
    dbra d2, high
    moveq #HALF-1, d2
low:
    move.b #$40, (a0)+
    dbra d2, low
    dbra d1, wave

    lea wavname, a1             ; write both files
    lea wav, a2
    move.l #samples+LENGTH-wav, d2
    bsr write
    lea midname, a1
    lea midi, a2
    move.l #midiend-midi, d2
    bsr write

    lea wavname, a1
    move.b #70, d0              ; the standard player plays the WAV
    trap #15
    move.l #50, d1
    move.b #23, d0              ; wait the half second it lasts
    trap #15
    lea midname, a1
    move.b #73, d0              ; the DirectX player plays the MIDI file
    trap #15
    move.b #9, d0
    trap #15

; write: a1 = filename, a2 = first byte, d2.l = byte count
write:
    move.b #52, d0              ; create the file; its handle is in d1.l
    trap #15
    move.l a2, a1
    move.b #54, d0              ; write the bytes
    trap #15
    move.b #56, d0              ; close the file
    trap #15
    rts

    org $2000
wavname: dc.b 'tone.wav', 0
midname: dc.b 'tune.mid', 0

; This WAV file: a 44-byte header, then samples. Numbers are stored low byte first.
wav:     dc.b 'RIFF', (36+LENGTH)&$FF, (36+LENGTH)>>8, 0, 0, 'WAVE'
         dc.b 'fmt ', 16, 0, 0, 0       ; a format chunk of 16 bytes:
         dc.b 1, 0, 1, 0                ; integer PCM, one channel,
         dc.b RATE&$FF, RATE>>8, 0, 0   ; samples a second,
         dc.b RATE&$FF, RATE>>8, 0, 0   ; bytes a second,
         dc.b 1, 0, 8, 0                ; one byte a sample, of 8 bits
         dc.b 'data', LENGTH&$FF, LENGTH>>8, 0, 0
samples: ds.b LENGTH

; A MIDI file: a header, then one track of events. Its numbers are stored
; high byte first, as the 68000 stores them.
midi:    dc.b 'MThd'
         dc.l 6                         ; six bytes of header data follow
         dc.w 0, 1, 96                  ; format 0, one track, 96 ticks a quarter note
         dc.b 'MTrk'
         dc.l midiend-track             ; the track's length
track:   dc.b 0, $C0, 73                ; at once, channel 1 takes instrument 73, the flute
         dc.b 0, $90, 60, 100           ; at once, middle C starts at velocity 100
         dc.b 96, $80, 60, 0            ; a quarter note later, it stops
         dc.b 0, $90, 64, 100           ; E
         dc.b 96, $80, 64, 0
         dc.b 0, $90, 67, 100           ; G
         dc.b 96, $80, 67, 0
         dc.b 0, $90, 72, 100           ; the C above middle C
         dc.b 96, $80, 72, 0
         dc.b 0, $FF, $2F, 0            ; the end of the track
midiend:
