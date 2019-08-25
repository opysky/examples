#include "stdafx.h"

using namespace Microsoft::WRL;

class MainWindow : public CWindowImpl<MainWindow> {
public:
	DECLARE_WND_CLASS(nullptr)

	void Launch(int nCmdShow) {
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		Create(nullptr, CWindow::rcDefault, L"HelloWebView2", WS_OVERLAPPEDWINDOW);
		
		CreateWebView2EnvironmentWithDetails(nullptr, nullptr, WEBVIEW2_RELEASE_CHANNEL_PREFERENCE_CANARY, nullptr,
			Callback<IWebView2CreateWebView2EnvironmentCompletedHandler>(
				[this](HRESULT result, IWebView2Environment* env) -> HRESULT {

					// Create a WebView, whose parent is the main window hWnd
					env->CreateWebView(*this, Callback<IWebView2CreateWebViewCompletedHandler>(
						[this](HRESULT result, IWebView2WebView* webview) -> HRESULT {
							if (webview != nullptr) {
								_webView = webview;
							}

							// Add a few settings for the webview
							// this is a redundant demo step as they are the default settings values
							IWebView2Settings* Settings;
							_webView->get_Settings(&Settings);
							Settings->put_IsScriptEnabled(TRUE);
							Settings->put_AreDefaultScriptDialogsEnabled(TRUE);
							Settings->put_IsWebMessageEnabled(TRUE);

							// Resize WebView to fit the bounds of the parent window
							RECT bounds;
							GetClientRect(&bounds);
							_webView->put_Bounds(bounds);

							// Schedule an async task to navigate to Bing
							_webView->Navigate(L"https://www.bing.com/");

							// Step 4 - Navigation events
							
							// Step 5 - Scripting
							
							// Step 6 - Communication between host and web content

							return S_OK;
						}).Get());
					return S_OK;
				}).Get());

		CenterWindow();
		ShowWindow(nCmdShow);
		Invalidate();
	}

private:
	BEGIN_MSG_MAP(MainWindow)
		MSG_WM_SIZE(OnSize)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()

	void OnSize(int nType, CSize size) {
		if (_webView == nullptr) {
			return;
		}

		CRect bounds;
		GetClientRect(&bounds);
		_webView->put_Bounds(bounds);
	}

	void OnDestroy() {
		PostQuitMessage(0);
	}

	ComPtr<IWebView2WebView> _webView;
};

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int nCmdShow) {
	_Module.Init(nullptr, hInstance);

	CMessageLoop kicker;
	_Module.AddMessageLoop(&kicker);

	MainWindow win;
	win.Launch(nCmdShow);

	int wParam = kicker.Run();

	_Module.RemoveMessageLoop();
	_Module.Term();

	return wParam;
}