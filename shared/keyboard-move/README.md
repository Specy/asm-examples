# Keyboard move

This program moves a square while you hold the arrow keys. Click the Screen first, so the keys go to the program. Press Stop to end it.

Each frame, the program asks which keys are held down, as games do, instead of reading the characters you type.

| Target | How it reads the keys |
| ------ | --------------------- |
| M68K   | Task 19 reports the state of four keys at once. The title is printed with task 14, which writes it to the Terminal and also draws it at the Screen's text cursor. |
| Z80    | The key state port, `0x31`, reports the state of the key whose code is in B. The title goes to the console port, which also draws it at the Screen's text cursor. |
