#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI::Interop;

class EdgeView : public CWindowImpl<EdgeView> {
public:
	DECLARE_WND_CLASS(nullptr)

	IAsyncAction CreateWebViewAsync(HWND hWndParent, ATL::_U_RECT rect) {
		CWindowImpl<EdgeView>::Create(hWndParent, rect);

		_webViewProcess = WebViewControlProcess();

		_webView = co_await _webViewProcess.CreateWebViewControlAsync(
			(int64_t)m_hWnd, 
			Rect(rect.m_lpRect->left, 
				rect.m_lpRect->top, 
				rect.m_lpRect->right - rect.m_lpRect->left, 
				rect.m_lpRect->bottom - rect.m_lpRect->top));

		Uri uri(L"http://aka.ms/cppwinrt");
		_webView.Navigate(uri);
	}

private:
	BEGIN_MSG_MAP(EdgeView)
		MSG_WM_SIZE(OnSize)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()

	void OnSize(int nType, CSize const& size) {
		if (!_webView || nType == SIZE_MINIMIZED || nType == SIZE_MAXHIDE || size.cx == 0 || size.cy == 0) {
			return;
		}

		CRect rect;
		GetClientRect(&rect);
		_webView.Bounds(Rect(rect.left, rect.top, rect.Width(), rect.Height()));
	}

	void OnDestroy() {
		if (_webViewProcess) {
			_webViewProcess.Terminate();
		}
	}

	WebViewControlProcess _webViewProcess{ nullptr };
	WebViewControl _webView{ nullptr };
};


class MainWindow : public CWindowImpl<MainWindow> {
public:
	DECLARE_WND_CLASS(nullptr)

	IAsyncAction LaunchAsync(int nCmdShow) {
		Create(nullptr, CWindow::rcDefault, L"EdgeView32", WS_OVERLAPPEDWINDOW);

		CenterWindow();
		ShowWindow(nCmdShow);
		Invalidate();

		CRect rect;
		GetClientRect(&rect);
		co_await _webView.CreateWebViewAsync(*this, rect);
	}

private:
	BEGIN_MSG_MAP(MainWindow) 
		//MSG_WM_PAINT(OnPaint)
		MSG_WM_SIZE(OnSize)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()

	//void OnPaint(HDC) {
	//	CPaintDC dc(*this);

	//	Uri uri(L"http://aka.ms/cppwinrt");

	//	std::array<wchar_t, 256> text;
	//	std::swprintf(&text[0], text.size(), L"Hello, %s!", uri.AbsoluteUri().c_str());

	//	CRect rect;
	//	GetClientRect(&rect);
	//	dc.DrawTextW(&text[0], -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
	//}

	void OnSize(int nType, CSize const& size) {
		if (!_webView || nType==SIZE_MINIMIZED || nType==SIZE_MAXHIDE || size.cx==0 || size.cy==0) {
			return;
		}

		CRect rect;
		GetClientRect(&rect);

		_webView.SetWindowPos(nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void OnDestroy() {
		PostQuitMessage(0);
	}

	EdgeView _webView;
};

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    init_apartment(apartment_type::single_threaded);

	_Module.Init(nullptr, hInstance);

	CMessageLoop kicker;
	_Module.AddMessageLoop(&kicker);

	MainWindow win;
	win.LaunchAsync(nCmdShow);

	int wParam = kicker.Run();

	_Module.RemoveMessageLoop();
	_Module.Term();
	return wParam;
}
