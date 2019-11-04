#include "stdafx.h"
#include "WindowsInAltTab.h"

namespace {

bool IsAltTabWindow(WindowInfo const& info)
{
	HWND hwnd = info.hwnd;
	auto title = info.windowText;
	auto className = info.className;

	if (hwnd == GetShellWindow()) {
		return false;
	}
	if (title.length() == 0) {
		return false;
	}
	if (GetAncestor(hwnd, GA_ROOT) != hwnd) {
		return false;
	}
	if (!IsWindowVisible(hwnd)) {
		return false;
	}
	LONG style = GetWindowLong(hwnd, GWL_STYLE);
	if (!((style & WS_DISABLED) != WS_DISABLED)) {
		return false;
	}
	DWORD cloaked = FALSE;
	HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
	if (SUCCEEDED(hr) && cloaked == DWM_CLOAKED_SHELL) {
		return false;
	}

	// Logicool Optionsの隠しウィンドウとかこれでも捕まえられないなあ

	return true;
}

}

WindowInfo::WindowInfo(HWND hwnd)
{
	this->hwnd = hwnd;
	std::array<wchar_t, 512> buffer;
	::GetClassName(hwnd, &buffer[0], buffer.size());
	className = &buffer[0];
	::GetWindowText(hwnd, &buffer[0], buffer.size());
	windowText = &buffer[0];
}

std::vector<WindowInfo> GetWindowsInAltTab()
{
	std::vector<WindowInfo> windows;
	
	EnumWindows([](HWND hwnd, LPARAM lParam) -> BOOL {
		auto windows = reinterpret_cast<std::vector<WindowInfo>*>(lParam);
		auto window = WindowInfo(hwnd);
		if (!IsAltTabWindow(window)) {
			return TRUE;
		}
		windows->push_back(window);
		return TRUE;
	}, reinterpret_cast<LPARAM>(&windows));

	return windows;
}
