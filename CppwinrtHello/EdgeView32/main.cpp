#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;
using namespace Windows::Web::UI::Interop;


class MainWindow : public CWindowImpl<MainWindow> {
public:
	DECLARE_WND_CLASS(nullptr)

	IAsyncAction LaunchApp(int nCmdShow) {
		Create(nullptr, CWindow::rcDefault, L"EdgeView32", WS_OVERLAPPEDWINDOW);
		CenterWindow();
		ShowWindow(nCmdShow);
		Invalidate();

		_webViewProcess = WebViewControlProcess();

		CRect rect;
		GetClientRect(&rect);
		_webView = co_await _webViewProcess.CreateWebViewControlAsync((int64_t)m_hWnd, Rect(rect.left, rect.top, rect.Width(), rect.Height()));

		Uri uri(L"http://aka.ms/cppwinrt");
		_webView.Navigate(uri);
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
		if (!_webView || nType == SIZE_MINIMIZED || size.cx == 0 || size.cy == 0) {
			return;
		}

		CRect rect;
		GetClientRect(&rect);
		_webView.Bounds(Rect(rect.left, rect.top, rect.Width(), rect.Height()));
	}

	void OnDestroy() {
		_webViewProcess.Terminate();
		PostQuitMessage(0);
	}

	WebViewControlProcess _webViewProcess = nullptr;
	WebViewControl _webView = nullptr;
};

// Windows::Web::UI::Interop関連では特に必要ないみたい
//auto CreateDispatcherQueueController()
//{
//	namespace abi = ABI::Windows::System;
//
//	DispatcherQueueOptions options
//	{
//		sizeof(DispatcherQueueOptions),
//		DQTYPE_THREAD_CURRENT,
//		DQTAT_COM_STA
//	};
//
//	Windows::System::DispatcherQueueController controller{ nullptr };
//	check_hresult(CreateDispatcherQueueController(options, reinterpret_cast<abi::IDispatcherQueueController**>(put_abi(controller))));
//	return controller;
//}

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
    init_apartment(apartment_type::single_threaded);

	//auto controller = CreateDispatcherQueueController();

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
