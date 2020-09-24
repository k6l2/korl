; 2: A window's title can contain WinTitle anywhere inside it to be a match.
SetTitleMatchMode, 2
if WinExist("Microsoft Visual Studio")
{
	WinActivate ; switch to the window found above
	Send, {F5}  ; start debugging the application in VS
}
else
	MsgBox, VS window not found! :(
