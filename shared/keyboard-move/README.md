# Keyboard move

A square that moves while the arrow keys are held. Every frame the program asks which keys are down, as a game does, instead of reading the characters typed. Build, Run, then click the Screen so that the keys go to the program instead of the editor. It runs until you press Stop.

| Target | How it reads the keys                                                                                                                               |
| ------ | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | Task 19 answers for four keys at once. The title is printed with task 14, which writes it to the Terminal and draws it at the screen's text cursor. |
| Z80    | The key state port, `0x31`, answers for the key code in B. The title goes to the console port, which also draws it at the Screen's text cursor.     |
