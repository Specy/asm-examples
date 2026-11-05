# Mouse paint

Build, then Run. Hold the left mouse button to paint under the pointer on the Screen.

On M68K and Z80, hold Shift as well to paint in red, and press the right button to clear the Screen. Press Stop to end the program.

On X86, use the wheel to change the colour, shown at the top left. To clear the Screen, press Stop, then Run again. The program also ends after thirty seconds without a pointer event.

The position is in Screen pixels whatever the panel's zoom, so the brush lands under the pointer.

| Target | How it reads the mouse                                                                                                             |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | Task 61 answers with the buttons and modifiers in D0 and the position in D1, Y in the high word and X in the low word.             |
| Z80    | Ports `0x40` and `0x41` answer X and Y, and `0x42` the buttons and modifiers, for the view in B. View 0 is the mouse as it is now. |
| X86    | Finds the input device that reports absolute positions, waits for events with `poll` and paints when a whole report has arrived. |
