.eqv COUNT, 10

.data
numbers: .space 40          # ten words, four bytes each

.text
main:
# If and else: the larger in t2, the distance in t4
    li t0, 37               # a = 37
    li t1, 64               # b = 64

    blt t1, t0, a_is_bigger # if b is below a, a is the answer
    mv t2, t1               # otherwise b is
    j done
a_is_bigger:
    mv t2, t0
done:

    slt t3, t0, t1          # the same question, answered as a number

    sub t4, t0, t1          # the distance, which may come out negative
    bgez t4, positive       # if it did not, it is already the answer
    sub t4, zero, t4        # otherwise flip its sign
positive:

# A loop: the numbers 1 to 10 into the words at numbers
    la s0, numbers          # where to write next
    li s1, 1                # what to write
    li s2, COUNT            # how many still to go
fill:
    sw s1, 0(s0)            # write it
    addi s0, s0, 4          # step on to the next word
    addi s1, s1, 1          # next number
    addi s2, s2, -1         # one less to go
    bnez s2, fill
