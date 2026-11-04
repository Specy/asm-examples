.data
rates:                      # samples a second for each note: 8000 plays the sound as computed
    .half 8000, 8980, 10079, 10679, 11986, 13454, 15102, 16000, 0
pluck:
    .space 4000             # 4,000 signed samples: half a second at 8,000 a second

.text
.globl main
main:
    # fill the buffer with a fading square wave: 32 samples per cycle
    la t0, pluck
    li t1, 0               # sample index
    li t6, 4000
fill:
    srli t2, t1, 5        # cycle number: sample index / 32
    li t3, 125
    sub t3, t3, t2       # amplitude falls by 1 per cycle
    andi t4, t1, 16       # the first half of each cycle is high,
    beqz t4, store
    sub t3, zero, t3     # the second half low
store:
    add t5, t0, t1
    sb t3, 0(t5)
    addi t1, t1, 1
    blt t1, t6, fill

    # copy the samples into the Synth's Sample memory
    li s0, 0xffff0100
    sw t0, 0x180(s0)      # from the buffer
    sh t6, 0x184(s0)      # 4,000 bytes
    sh zero, 0x186(s0)    # to the start of Sample memory
    sb zero, 0x188(s0)    # trigger the copy; the written value does not matter

    # channel 0 plays them
    sh zero, 0x0a(s0)     # from sample 0
    sh t6, 0x0c(s0)       # for 4,000 samples, then silence
    li t1, 6
    sb t1, 0x11(s0)       # waveform 6: sample
    li t1, 120
    sb t1, 0x12(s0)       # volume
    li t1, 127
    sb t1, 0x13(s0)       # sustain: the samples fade by themselves

    la s1, rates
    li s2, 1               # the gate bit
next:
    lhu t1, 0(s1)         # the next rate
    beqz t1, done
    sb zero, 0x10(s0)     # gate off, then on: from the first sample again
    sh t1, 0(s0)
    sb s2, 0x10(s0)       # write the gate bit to channel 0's control register
    li a7, 32
    li a0, 300             # wait 300 ms before starting the next note
    ecall
    addi s1, s1, 2
    j next

done:
    li a7, 10
    ecall
