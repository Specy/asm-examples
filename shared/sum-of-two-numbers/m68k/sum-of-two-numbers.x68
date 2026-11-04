    lea first, a1
    move.b #18, d0      ; print the first prompt and read a number
    trap #15
    move.l d1, d2       ; save the first number in d2

    lea second, a1
    move.b #18, d0      ; print the second prompt and read a number
    trap #15
    add.l d1, d2        ; add the second number to the saved first number

    lea answer, a1
    move.l d2, d1       ; task 17 prints the number in d1
    move.b #17, d0      ; print the answer string and the sum
    trap #15

    move.b #9, d0       ; end the program
    trap #15

    org $2000
first:  dc.b 'First number: ', 0
second: dc.b 10, 'Second number: ', 0
answer: dc.b 10, 'The sum is ', 0
