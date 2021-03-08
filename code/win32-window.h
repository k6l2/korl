#pragma once
#include "win32-main.h"
internal v2u32 
	korl_w32_window_getWindowDimensions(HWND hwnd);
internal DWORD 
	korl_w32_window_queryNearestMonitorRefreshRate(HWND hwnd);
enum class KorlWin32MoveSizeMode : u8
	{ OFF
	, MOVE_MOUSE
	, MOVE_KEYBOARD
	, SIZE_MOUSE
	, SIZE_KEYBOARD };
internal void 
	korl_w32_window_setMoveSizeMode(KorlWin32MoveSizeMode mode, HWND hWnd);
struct Korl_W32_Window_WndProcResult
{
	LRESULT lResultOverride;
	enum class CallerAction : u8
		{ NONE
		, DEFAULT_WINDOW_PROCEDURE }
			callerActionRequired;
};
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_sysCommand(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_lButtonDown(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_nclButtonDown(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_lButtonUp(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_nclButtonUp(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_keyDown(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_mouseMove(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal Korl_W32_Window_WndProcResult 
	korl_w32_window_wndProc_captureChanged(
		HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
internal void 
	korl_w32_window_stepMoveSize(
		const f32 deltaSeconds, const GameMouse*const mouse, 
		const GameKeyboard*const keyboard);