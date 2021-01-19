/*
    LSMTest.c
    Implements a small program demonstrating the use of the LooplessSizeMove
        functions defined in LooplessSizeMove.h within the context of a basic
        Windows app.

    Author: Nathaniel J Fries

    The author asserts no copyright, this work is released into the public domain.
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#ifdef TIME_LOOP
#include <stdio.h>
#endif // TIME_LOOP

#include "LooplessSizeMove.h"


#define CLASSNAME L"LSMCLASS"

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch(msg)
    {
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        case WM_GETMINMAXINFO:
        {
            /* just to test minmax restricting logic. */
            PMINMAXINFO info = (PMINMAXINFO)lParam;
            info->ptMinTrackSize.x = 640;
            info->ptMinTrackSize.y = 480;
            info->ptMaxTrackSize.x = 800;
            info->ptMaxTrackSize.y = 680;
            break;
        }
        default:
            break;
    }

    return LSMProc(hwnd, msg, wParam, lParam);
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrev, LPSTR cmdLine, int nCmdShow)
{
    WNDCLASSEXW wndCls;
    BOOL run = TRUE;
    HWND wnd = 0;
    #ifdef TIME_LOOP
    LARGE_INTEGER perfBase;
    LARGE_INTEGER perfNow;
    LARGE_INTEGER perfFreq;
    QueryPerformanceFrequency(&perfFreq);
    #endif

    wndCls.cbSize = sizeof(WNDCLASSEXW);
    wndCls.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wndCls.lpfnWndProc = &WndProc;
    wndCls.cbClsExtra = 0;
    wndCls.cbWndExtra = sizeof(DWORD_PTR);
    wndCls.hInstance = hInstance;
    wndCls.hIcon = 0;
    wndCls.hCursor = 0;
    wndCls.hbrBackground = (HBRUSH)COLOR_BACKGROUND+1;
    wndCls.lpszMenuName = 0;
    wndCls.lpszClassName = CLASSNAME;
    wndCls.hIconSm = 0;
    RegisterClassExW(&wndCls);

    wnd = CreateWindowExW(WS_EX_OVERLAPPEDWINDOW, CLASSNAME, L"Loopless Resize!", WS_OVERLAPPEDWINDOW,
            100, 100, 640, 480, NULL, NULL, hInstance, NULL);
    ShowWindow(wnd, nCmdShow);



    while(run)
    {
        MSG msg;
        #ifdef TIME_LOOP
        double ms;
        QueryPerformanceCounter(&perfBase);
        #endif
        while(PeekMessageW(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            SizingCheck(&msg);
            DispatchMessageW(&msg);
            if(msg.message == WM_QUIT)
            {
                run = FALSE;
                break;
            }
        }
        #ifdef TIME_LOOP
        QueryPerformanceCounter(&perfNow);

        ms = (double)(*(long long*)(&perfNow) - *(long long *)(&perfBase)) / (*(long long *)(&perfFreq) / 1000);
        if(ms < 0.05d)
        {
            //printf("~0ms\n");
        }
        else
        {
            printf("%.03fms\n", ms);
            printf("slow!\n");
            if(ms > 10.0d)
            {
                Beep(7500, 30);
            }
        }
        #endif
    }

    LSMCleanup();

    return 0;
}
