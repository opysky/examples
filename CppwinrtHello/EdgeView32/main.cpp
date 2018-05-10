#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;

class MainWindow : public CWindowImpl<MainWindow> {
public:
	DECLARE_WND_CLASS(nullptr)

	void LaunchApp(int nCmdShow) {
		Create(nullptr, CWindow::rcDefault, nullptr, WS_OVERLAPPEDWINDOW);
		CenterWindow();
		ShowWindow(nCmdShow);
		Invalidate();
	}

private:
	BEGIN_MSG_MAP(MainWindow) 
		MSG_WM_PAINT(OnPaint)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()

	void OnPaint(HDC) {
		CPaintDC dc(*this);

		Uri uri(L"http://aka.ms/cppwinrt");

		std::array<wchar_t, 256> text;
		std::swprintf(&text[0], text.size(), L"Hello, %s!", uri.AbsoluteUri().c_str());

		CRect rect;
		GetClientRect(&rect);
		dc.DrawTextW(&text[0], -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	}

	void OnDestroy() {
		PostQuitMessage(0);
	}
};

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    init_apartment(apartment_type::single_threaded);

	_Module.Init(nullptr, hInstance);

	CMessageLoop kicker;
	_Module.AddMessageLoop(&kicker);

	MainWindow win;
	win.LaunchApp(nCmdShow);

	int wParam = kicker.Run();

	_Module.RemoveMessageLoop();
	_Module.Term();
	return wParam;
}
