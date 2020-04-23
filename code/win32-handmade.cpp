#include <windows.h>
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, 
                    PWSTR pCmdLine, int nCmdShow)
{
	MessageBoxA(NULL, "this is a handmade test", "w/e bro", 
	            MB_OK | MB_ICONINFORMATION);
	return 0;
}