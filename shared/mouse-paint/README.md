# Mouse paint

This program paints with the mouse: hold the left button to paint under the pointer on the Screen.

On M68K and Z80, hold Shift as well to paint in red, and press the right button to clear the Screen. Press Stop to end the program.

On X86, turn the wheel to change the colour, which is shown at the top left. To clear the Screen, press Stop, then run the program again. The program also ends after thirty seconds without any mouse input.

The program gets the pointer's position in Screen pixels, whatever the zoom, so the brush stays under the pointer.

| Target | How it reads the mouse |
| ------ | ---------------------- |
| M68K   | Task 61 returns the buttons and modifier keys in D0, and the position in D1: Y in the high word, X in the low word. |
| Z80    | Ports `0x40` and `0x41` return X and Y, and `0x42` returns the buttons and modifier keys, for the view in B. View 0 is the mouse's current state. |
| X86    | Finds the input device that reports absolute positions, waits for events with `poll`, and paints once a whole report has arrived. |
