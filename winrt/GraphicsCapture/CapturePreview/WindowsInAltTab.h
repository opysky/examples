#pragma once

struct WindowInfo {
	HWND hwnd;
	std::wstring windowText;
	std::wstring className;

	WindowInfo(HWND hwnd);
};

std::vector<WindowInfo> GetWindowsInAltTab();