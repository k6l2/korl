# TODO korl-gui-widget-inputText

[x] accept a string buffer & allocation strategy from the user upon invocation of the INPUT_TEXT widget
[x] draw the INPUT_TEXT widget
[x] draw the INPUT_TEXT cursor when the widget becomes active
[x] default the console's INPUT_TEXT widget to be active when first invoked
[x] default the console's INPUT_TEXT cursor to be the end of the string buffer when first invoked
[x] draw the cursor at the correct position
[ ] move cursor with [Ctrl]+{vim cursor move keys: J,L}
[ ] hold [Shift]+move-cursor to select string buffer glyphemes
[ ] draw a highlighted region behind selected string buffer glyphemes
[ ] [BackSpace] or [Del] to delete selection
[ ] [BackSpace] to delete cursor previous codepoint
[ ] [Del] to delete cursor next codepoint
[ ] [any-non-whitespace-key] to append codepoint to string buffer
[ ] [Ctrl]+[C] or [Ctrl]+[Y] to copy/yank selection to clipboard
[ ] [Ctrl]+[V] or [Ctrl]+[P] to paste clipboard contents to string buffer
[ ] [Ctrl]+[Shift]+[6] to move cursor to start of string buffer
[ ] [Ctrl]+[Shift]+[A] to move cursor to the end of string buffer
[ ] [Ctrl]+[A] to select all
[ ] [Enter] to propogate a signal back to the user who invoked the INPUT_TEXT widget