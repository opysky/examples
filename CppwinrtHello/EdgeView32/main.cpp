#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI;
using namespace Windows::Web::UI::Interop;

class EdgeView : public CWindowImpl<EdgeView> {
public:
	DECLARE_WND_CLASS(nullptr)

	IAsyncAction CreateWebViewAsync(HWND hWndParent, ATL::_U_RECT rect) {
		CWindowImpl<EdgeView>::Create(hWndParent, rect);

		_webViewProcess = WebViewControlProcess();

		CRect client;
		GetClientRect(&client);
		_webView = co_await _webViewProcess.CreateWebViewControlAsync(
			(int64_t)m_hWnd, 
			Rect(client.left, client.top, client.Width(), client.Height()));

		Uri uri(L"https://www.babylonjs.com/");
		_webView.Navigate(uri);

		// ウィンドウの新規作成リクエストは既定だと何もしてくれないっぽ
		_webView.NewWindowRequested([&](IWebViewControl const& s, WebViewControlNewWindowRequestedEventArgs const& e) {
			_webView.Navigate(e.Uri());
			e.Handled(true);
		});
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

		// Boundsを設定すれば再描画される
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
		co_await _edgeView.CreateWebViewAsync(*this, rect);
	}

private:
	BEGIN_MSG_MAP(MainWindow) 
		MSG_WM_SIZE(OnSize)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()

	void OnSize(int nType, CSize const& size) {
		if (!_edgeView || nType==SIZE_MINIMIZED || nType==SIZE_MAXHIDE || size.cx==0 || size.cy==0) {
			return;
		}

		CRect rect;
		GetClientRect(&rect);
		_edgeView.SetWindowPos(nullptr, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top, SWP_NOZORDER | SWP_NOACTIVATE);
	}

	void OnDestroy() {
		PostQuitMessage(0);
	}

	EdgeView _edgeView;
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
