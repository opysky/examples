#pragma once

#include "FPSCounter.h"

enum class KeyAction {
    Pressed, 
    Released, 
};

enum KeyModifiers {
    KeyModifiers_None = 0,
    KeyModifiers_Shift = 1,
    KeyModifiers_Control = 2,
    KeyModifiers_Alt = 4,
};

class Direct3D11Application {
public:
    int run(HINSTANCE hInstance, std::wstring const& appTitle);

    HWND hWndApp() { return _hWnd; }
    wil::com_ptr<ID3D11Device> const& nativeDevice() { return _device; }
    wil::com_ptr<ID3D11DeviceContext> nativeDeviceContext() { return _context; }
    void allowTearing(bool value);
    void toggleDevTools() { _isDevToolsVisible = !_isDevToolsVisible; }

    std::wstring applicationFilePath();
    std::wstring applicationDirPath();
    std::wstring assetsDirPath();

protected:
    virtual bool winProc(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* result);

    virtual void initialize() {}
    virtual void finalize() {}
    virtual void beginRun() {}
    virtual void endRun() {}
    virtual void onUpdate(float deltaTime) {}
    virtual void onDraw(wil::com_ptr<ID3D11DeviceContext> const& context) {}
    virtual void onImGui() {}

    virtual void onSizeChanged(UINT nType, SIZE const& size);
    virtual void onKeyEvent(KeyAction action, int code, uint32_t modifiers) {}

private:
    void createDevice();
    void disposeDevice();
    void update();
    void draw();

    static LRESULT CALLBACK bootstrapWinProc(HWND, UINT, WPARAM, LPARAM);
    static LRESULT CALLBACK dispatchWinProc(HWND, UINT, WPARAM, LPARAM, UINT_PTR, DWORD_PTR);

private:
    HINSTANCE _hInstance = nullptr;
    HWND _hWnd = nullptr;
    std::wstring _windowText;

    wil::com_ptr<ID3D11Device> _device;
    wil::com_ptr<ID3D11DeviceContext> _context;
    wil::com_ptr<IDXGISwapChain2> _swapChain;
    wil::com_ptr<ID3D11RenderTargetView> _backBufferRTV;
    wil::com_ptr<ID3D11DepthStencilView> _depthBufferDSV;
    
    D3D11_FEATURE_DATA_THREADING _featureThreading;
    D3D11_FEATURE_DATA_DOUBLES _featureDoubles;
    BOOL _tearingSupported = FALSE;

    Stopwatch _stopwatch;
    FPSCounter _fpsCounter;
    double _currentTime = 0.0;
    double _currentFps = 0.0;
    bool _allowTearing = false;
    bool _isDevToolsVisible = false;
};