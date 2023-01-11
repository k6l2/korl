# TODO: korl-gui scroll bars

- our primary objective here is to be able to add scrollbars to windows in arbitrary ways
  - entire window contents
  - sub-region of the window, with widgets that are "outside" of the scrollable region of the window
  - collapsable "tabs" containing widgets within a window, like how DearImgui does this with their giant demo window
- the immediate technique I am thinking about to approach this problem is by implementing a "scroll-area" widget
- using the widget DAG, we can make children of the scroll-area widget be affected by the scroll bars, 
  while widgets that are not children of the scroll-area are not accessible if the window is too small

[x] create a scroll area widget
[x] make windows with the KORL_GUI_WINDOW_STYLE_FLAG_RESIZABLE style automatically spawn a scroll area
[x] ensure that when a window ends, the scroll area widget is also popped off the widget parent stack
[x] to determine the content size of the scroll area widget, attempt to fill the full remaining available content AABB of our parent
[x] ensure that all widget logic so far is unaffected by this change

[x] if the user is hovering over the scroll area, we need to make mouse scroll wheel events modify the scroll area widget's child widget cursor (shift the y component)
[x] shift + mouse scroll wheel => horizontal scrolling of the widget's contents (shift the childWidgetCursor x component)
[x] clamp the scroll range to the visible widget contents

[x] create a vertical scroll bar widget
[x] to start out, just make the vertical scroll bar widget draw a box where it is
[x] make the scroll area widget automatically spawn a vertical scroll bar widget
[x] add a korl-gui mechanism to force a new child widget to have a maximum order index (instead of incrementing from the last order index), or maybe just add some API to force a child widget to have a specific order index or something
[x] force the vertical scroll bar widget spawned by the scroll area to have a maximal order index, and ensure it is now drawing on top of all other widget contents

[x] keep track of the scroll bar's slider size using the difference between aabbChildren & aabb
[x] draw the scroll bar's slider
[x] draw the regions on either side of the slider in a subtle way
  - maybe translucent elements that have a more distant Z position
[x] ensure that the appearence of a scroll bar affects the "scrollable region" such that we can scroll the aabbChildren enough to allow the aabbChildren to be unobstructed by the scroll bars themselves
[ ] set the scroll bar slider to the correct position, based on the SCROLL_AREA's contentOffset

[ ] augment korl_gui_scrollBar to allow the caller to set the size of the bar
[ ] modify the mouse drag behavior of SCROLL_BAR widget to allow the user to click+drag on the scroll bar's slider
[ ] return the "scroll distance" from korl_gui_scrollBar

[ ] clamp the slider's size to some minimum size (?)
  - after a minimum scroll bar size is reached, we can:
    - draw the slider as just a line primitive, instead of a quad
    - disable scroll bar slider mouse click checks
    - clicks on the scroll bar will just set the scroll position to be that exact position

[ ] hold CTRL + click & drag to pan by the exact mouse delta

[ ] create a horizontal scroll bar widget, using similar behavior as the vertical scroll bar widget
[ ] ensure that there are no negative interactions between the vertical & horizontal scroll bars of a scroll area widget