.data
tune:                       # pitch, milliseconds; a pitch of 0 ends the tune
    .word 64, 400,  64, 400,  65, 400,  67, 400
    .word 67, 400,  65, 400,  64, 400,  62, 400
    .word 60, 400,  60, 400,  62, 400,  64, 400
    .word 64, 600,  62, 200,  62, 800
    .word 0

.text
.globl main
main:
    la $t0, tune            # address of the next note's pitch
    li $a2, 0               # instrument 0, the acoustic grand piano
    li $a3, 100             # volume, 0 to 127

next:
    lw $a0, 0($t0)          # pitch
    beq $a0, $zero, done
    lw $a1, 4($t0)          # duration in milliseconds
    li $v0, 33              # service 33: play the note, then wait for it to end
    syscall
    addi $t0, $t0, 8        # the next pair of words
    j next

done:
    li $v0, 10
    syscall
