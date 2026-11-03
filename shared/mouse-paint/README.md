# Mouse paint

Painting with the mouse: hold the left button to paint under the pointer, hold Shift as well to paint in red, and press the right button to clear the Screen. The position is in Screen pixels whatever the panel's zoom, so the brush lands under the pointer. Build, then Run. It runs until you press Stop.

| Target | How it reads the mouse                                                                                                             |
| ------ | ---------------------------------------------------------------------------------------------------------------------------------- |
| M68K   | Task 61 answers with the buttons and modifiers in D0 and the position in D1, Y in the high word and X in the low word.             |
| Z80    | Ports `0x40` and `0x41` answer X and Y, and `0x42` the buttons and modifiers, for the view in B. View 0 is the mouse as it is now. |
