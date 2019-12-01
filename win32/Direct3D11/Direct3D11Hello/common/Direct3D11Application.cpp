#include "stdafx.h"
#include "Direct3D11Application.h"

// なんでヘッダーに宣言してないんじゃろ？
IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {

const DWORD WinStyle_Fixed = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);
const DWORD WinStyle_Resizable = (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);
const DWORD WinStyle_Fullscreen = (WS_POPUP | WS_SYSMENU);
const DXGI_FORMAT OutputFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
const UINT BackBufferCount = 2;

__declspec(thread) Direct3D11Application* _that = nullptr;

}

int Direct3D11Application::run(HINSTANCE hInstance, std::wstring const& appTitle)
{
    //SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    
    _hInstance = hInstance;
    _windowText = appTitle;

    initialize();

    createDevice();

    beginRun();

    _stopwatch.start();

    MSG msg = {};    
    PeekMessage(&msg, 0, 0, 0, PM_NOREMOVE);
    while (msg.message != WM_QUIT) {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE) != 0) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else {
            update();
            draw();
        }
    }

    endRun();

    disposeDevice();

    finalize();

    return static_cast<int>(msg.wParam);
}

void Direct3D11Application::allowTearing(bool value)
{
    _allowTearing = value;
}

std::wstring Direct3D11Application::applicationFilePath()
{
    WCHAR buffer[MAX_PATH];
    auto length = GetModuleFileName(_hInstance, buffer, MAX_PATH);
    return std::wstring(buffer);
}

std::wstring Direct3D11Application::applicationDirPath()
{
    WCHAR buffer[MAX_PATH];
    auto length = GetModuleFileName(_hInstance, buffer, MAX_PATH);
    PathRemoveFileSpec(buffer);
    return std::wstring(buffer);
}

std::wstring Direct3D11Application::assetsDirPath()
{
    return applicationDirPath().append(L"\\..\\..\\Assets\\");
}

bool Direct3D11Application::winProc(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* result)
{
    if (auto lr = ImGui_ImplWin32_WndProcHandler(_hWnd, uMsg, wParam, lParam)) {
        *result = lr;
        return true;
    }

    bool handled = false;
    switch (uMsg) {
    case WM_KEYDOWN: 
    case WM_SYSKEYDOWN: {
        uint32_t modifiers = KeyModifiers_None;
        if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= KeyModifiers_Shift;
        if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= KeyModifiers_Control;
        if (GetKeyState(VK_MENU) & 0x8000) modifiers |= KeyModifiers_Alt;

        if (uMsg != WM_SYSKEYDOWN || wParam != VK_MENU) {
            onKeyEvent(KeyAction::Pressed, static_cast<int>(wParam), modifiers);
        }
        break;
    }
    case WM_KEYUP:
    case WM_SYSKEYUP: {
        uint32_t modifiers = KeyModifiers_None;
        if (GetKeyState(VK_SHIFT) & 0x8000) modifiers |= KeyModifiers_Shift;
        if (GetKeyState(VK_CONTROL) & 0x8000) modifiers |= KeyModifiers_Control;
        if (GetKeyState(VK_MENU) & 0x8000) modifiers |= KeyModifiers_Alt;

        if (uMsg != WM_SYSKEYDOWN || wParam != VK_MENU) {
            onKeyEvent(KeyAction::Released, static_cast<int>(wParam), modifiers);
        }
        break;
    }
    case WM_SIZE: {
        UINT nType = static_cast<UINT>(wParam);
        SIZE size{ GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
        onSizeChanged(nType, size);
        break;
    }
    case WM_GETMINMAXINFO: {
        MINMAXINFO* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        mmi->ptMinTrackSize.x = 320;
        mmi->ptMinTrackSize.y = 240;
        break;
    }
    case WM_DESTROY: {
        PostQuitMessage(0);
        break;
    }
    }
    return handled;
}

void Direct3D11Application::onSizeChanged(UINT nType, SIZE const& size)
{
    if (!_swapChain || nType == SIZE_MINIMIZED)
        return;

    DXGI_SWAP_CHAIN_DESC1 scd;
    _swapChain->GetDesc1(&scd);
    if (scd.Width == size.cx && scd.Height == size.cy)
        return;

    // IDXGISwapChain::ResizeBuffers の前にバックバッファへの参照をすべて解放しないと失敗する
    _depthBufferDSV = nullptr;
    _backBufferRTV = nullptr;
    
    wil::com_ptr<ID3D11DeviceContext> context;
    nativeDevice()->GetImmediateContext(context.put());
    context->ClearState();

    THROW_IF_FAILED(_swapChain->ResizeBuffers(
        scd.BufferCount,
        std::max<UINT>(size.cx, 1u),
        std::max<UINT>(size.cy, 1u),
        scd.Format,
        scd.Flags));

    {
        wil::com_ptr<ID3D11Texture2D> backBuffer;
        THROW_IF_FAILED(_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put())));
        THROW_IF_FAILED(_device->CreateRenderTargetView(backBuffer.get(), nullptr, _backBufferRTV.put()));
    }

    {
        wil::com_ptr<ID3D11Texture2D> depthBuffer;
        D3D11_TEXTURE2D_DESC dsd = {};
        dsd.Width = std::max<UINT>(size.cx, 1u);
        dsd.Height = std::max<UINT>(size.cy, 1u);
        dsd.MipLevels = 1;
        dsd.ArraySize = 1;
        dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsd.SampleDesc.Count = 1;
        dsd.Usage = D3D11_USAGE_DEFAULT;
        dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        THROW_IF_FAILED(_device->CreateTexture2D(&dsd, nullptr, depthBuffer.put()));
        THROW_IF_FAILED(_device->CreateDepthStencilView(depthBuffer.get(), nullptr, _depthBufferDSV.put()));
    }
}

void Direct3D11Application::createDevice()
{
    WNDCLASSEX wcex = {};
    wcex.cbSize = sizeof(wcex);
    wcex.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wcex.hInstance = _hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.lpfnWndProc = bootstrapWinProc;
    
    //背面消去の抑制
    wcex.hbrBackground = 0; 
    //wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    std::array<wchar_t, 128> autoName;
    std::swprintf(autoName.data(), autoName.size(), L"Direct3D11Applicationlication:%p", reinterpret_cast<void*>(&wcex));
    wcex.lpszClassName = autoName.data();

    RegisterClassEx(&wcex);

    _that = this;
    CreateWindow(
        autoName.data(), 
        _windowText.c_str(), 
        WinStyle_Resizable, 
        CW_USEDEFAULT, 
        CW_USEDEFAULT, 
        640, 
        480, 
        nullptr, 
        nullptr, 
        _hInstance, 
        nullptr);
    ShowWindow(_hWnd, SW_SHOWDEFAULT);

    RECT clientRect;
    GetClientRect(_hWnd, &clientRect);
    SIZE clientSize{ clientRect.right - clientRect.left, clientRect.bottom - clientRect.top };
    {
        UINT createDeviceFlags = 0; //D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
        createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
        THROW_IF_FAILED(D3D11CreateDevice(
            nullptr,
            D3D_DRIVER_TYPE_HARDWARE,
            nullptr,
            createDeviceFlags,
            nullptr,
            0,
            D3D11_SDK_VERSION,
            _device.put(),
            nullptr,
            _context.put()));

        auto dxgiDevice = _device.query<IDXGIDevice>();
        wil::com_ptr<IDXGIAdapter> adapter;
        THROW_IF_FAILED(dxgiDevice->GetParent(IID_PPV_ARGS(adapter.put())));
        wil::com_ptr<IDXGIFactory5> factory;
        THROW_IF_FAILED(adapter->GetParent(IID_PPV_ARGS(factory.put())));

        THROW_IF_FAILED(_device->CheckFeatureSupport(D3D11_FEATURE_THREADING, &_featureThreading, sizeof(_featureThreading)));
        THROW_IF_FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &_tearingSupported, sizeof(_tearingSupported)));
        
        UINT swapChainFlags = 0;
        if (_tearingSupported) {
            swapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        DXGI_SWAP_CHAIN_DESC1 scd = {};
        scd.Width = static_cast<UINT>(clientSize.cx);
        scd.Height = static_cast<UINT>(clientSize.cy);
        scd.Format = OutputFormat;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.BufferCount = BackBufferCount;
        scd.SampleDesc.Count = 1;
        scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
        scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
        scd.Flags = swapChainFlags;

        wil::com_ptr<IDXGISwapChain1> swapChain;
        THROW_IF_FAILED(factory->CreateSwapChainForHwnd(
            _device.get(),
            _hWnd,
            &scd,
            nullptr,
            nullptr,
            swapChain.put()));
        _swapChain = swapChain.query<IDXGISwapChain2>();
    }

    {
        wil::com_ptr<ID3D11Texture2D> backBuffer;
        THROW_IF_FAILED(_swapChain->GetBuffer(0, IID_PPV_ARGS(backBuffer.put())));
        THROW_IF_FAILED(_device->CreateRenderTargetView(backBuffer.get(), nullptr, _backBufferRTV.put()));
    }

    {
        wil::com_ptr<ID3D11Texture2D> depthBuffer;
        D3D11_TEXTURE2D_DESC dsd = {};
        dsd.Width = static_cast<UINT>(clientSize.cx);
        dsd.Height = static_cast<UINT>(clientSize.cy);
        dsd.MipLevels = 1;
        dsd.ArraySize = 1;
        dsd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
        dsd.SampleDesc.Count = 1;
        dsd.Usage = D3D11_USAGE_DEFAULT;
        dsd.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        THROW_IF_FAILED(_device->CreateTexture2D(&dsd, nullptr, depthBuffer.put()));
        THROW_IF_FAILED(_device->CreateDepthStencilView(depthBuffer.get(), nullptr, _depthBufferDSV.put()));        
    }

    {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGui::StyleColorsDark();

        auto io = ImGui::GetIO();
        io.IniFilename = nullptr;

        ImGui_ImplWin32_Init(_hWnd);
        ImGui_ImplDX11_Init(_device.get(), _context.get());
    }
}

void Direct3D11Application::disposeDevice()
{
    {
        ImGui_ImplDX11_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
    }
    _depthBufferDSV = nullptr;
    _backBufferRTV = nullptr;
    _swapChain = nullptr;
    _device = nullptr;
}

void Direct3D11Application::update()
{
    double currTime = _stopwatch.elapsedSec();
    auto deltaTime = currTime - _currentTime;
    _currentTime = currTime;
    _fpsCounter.update(deltaTime);

    onUpdate(static_cast<float>(deltaTime));
}

void Direct3D11Application::draw()
{
    {
        ID3D11RenderTargetView* renderTargetViews[1];
        renderTargetViews[0] = _backBufferRTV.get();
        _context->OMSetRenderTargets(1, renderTargetViews, _depthBufferDSV.get());

        DXGI_SWAP_CHAIN_DESC1 scd;
        _swapChain->GetDesc1(&scd);

        D3D11_VIEWPORT vp = {};
        vp.Width = static_cast<float>(scd.Width);
        vp.Height = static_cast<float>(scd.Height);
        vp.MinDepth = D3D11_MIN_DEPTH;
        vp.MaxDepth = D3D11_MAX_DEPTH;
        _context->RSSetViewports(1, &vp);

        _context->ClearRenderTargetView(_backBufferRTV.get(), DirectX::Colors::CornflowerBlue);
        _context->ClearDepthStencilView(_depthBufferDSV.get(), D3D11_CLEAR_DEPTH, 1.0f, 0x0);

        onDraw(_context);
    }

    {
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        if (_isStatVisible) {
            ImGui::SetNextWindowSize(ImVec2(200, 100), ImGuiCond_Once);
            if (ImGui::Begin("Frame Statistics")) {
                ImGui::Text("fps: %.2lf", _fpsCounter.currentFps());
            }
            ImGui::End();
        }

        if (_isCapsVisible) {
            if (ImGui::Begin("D3D11 Caps")) {
                ImGui::Text("DriverConcurrentCreates: %s", _featureThreading.DriverConcurrentCreates ? "true" : "false");
                ImGui::Text("DriverCommandLists: %s", _featureThreading.DriverCommandLists ? "true" : "false");
                ImGui::Text("PRESENT_ALLOW_TEARING: %s", _tearingSupported ? "true" : "false");
            }
            ImGui::End();
        }

        onImGui();

        ImGui::Render();
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    }

    DXGI_PRESENT_PARAMETERS pp = {};
    UINT syncInterval = 1;
    UINT presentFlags = 0;
    if (_allowTearing && _tearingSupported) {
        syncInterval = 0;
        presentFlags = DXGI_PRESENT_ALLOW_TEARING;
    }
    THROW_IF_FAILED(_swapChain->Present1(syncInterval, presentFlags, &pp));
}

LRESULT Direct3D11Application::bootstrapWinProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto that = _that;
    _that = nullptr;
    that->_hWnd = hWnd;

    SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(DefWindowProc));

    auto uIdSubclass = reinterpret_cast<UINT_PTR>(that);
    auto dwRefData = reinterpret_cast<DWORD_PTR>(that);
    SetWindowSubclass(hWnd, dispatchWinProc, uIdSubclass, dwRefData);

    return dispatchWinProc(hWnd, uMsg, wParam, lParam, uIdSubclass, dwRefData);
}

LRESULT Direct3D11Application::dispatchWinProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
    auto that = reinterpret_cast<Direct3D11Application*>(dwRefData);
    LRESULT result = 0;
    bool handled = that->winProc(uMsg, wParam, lParam, &result);

    switch (uMsg) {
    case WM_NCDESTROY: {
        // "you must remove your window subclass before the window being subclassed is destroyed."
        // http://blogs.msdn.com/b/oldnewthing/archive/2003/11/11/55653.aspx
        RemoveWindowSubclass(hWnd, dispatchWinProc, uIdSubclass);
        break;
    }
    }
    return handled ? result : DefSubclassProc(hWnd, uMsg, wParam, lParam);

}
