    lea greeting, a1    ; a1 gets the address of the first character
    move.b #13, d0      ; task 13: print the string and start a new line
    trap #15

    lea question, a1    ; address of the second string
    move.l #42, d1      ; the number to print after it
    move.b #17, d0      ; task 17: print the string, then the number
    trap #15

    move.b #9, d0       ; task 9: end the program
    trap #15

    org $2000
greeting: dc.b 'Hello, world!', 0
question: dc.b 'The answer is ', 0
