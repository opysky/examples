#include "stdafx.h"

namespace {

    const UINT FrameCount = 2;

}

class MainWindow : public CWindowImpl<MainWindow>, public CIdleHandler {
public:
    DECLARE_WND_CLASS_EX(nullptr, CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, -1)

    void Launch() {
        Create(nullptr, CWindow::rcDefault, L"00_Hello", WS_OVERLAPPEDWINDOW);

        CenterWindow();
        ShowWindow(SW_SHOWNORMAL);
        Invalidate();

        UINT factoryFlags = 0;
#if defined(_DEBUG)
        {
            // Direct3D 12 Debug layer 有効化の手続き
            wil::com_ptr<ID3D12Debug> debugController;
            THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put())));
            debugController->EnableDebugLayer();
            // このフラグは Direct3D 11 まではデバイス作成時の指定等によってはランタイムが設定することもあったそうな。
            // Direct3D 12 は D3D12CreateDevice がシンプルになってるぶんここで明示的に指定しようということかしら？
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        // Direct3D 12 で WARP ドライバを使用する場合は IDXGIFactory4::EnumWarpAdapter が必要になる
        wil::com_ptr<IDXGIFactory4> factory;
        THROW_IF_FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.put())));
        
        // ハードウェアアダプタの取得
        wil::com_ptr<IDXGIAdapter1> adapter;
        for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, adapter.put()); ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }
            // MinimumFeatureLevel で作成できるか検証
            if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }

        THROW_IF_FAILED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(_device.put())));

        // 種類とオプションを指定してコマンドキューを作成
        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        THROW_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_commandQueue.put())));

        CRect clientRect;
        GetClientRect(&clientRect);

        DXGI_SWAP_CHAIN_DESC1 scd = {};
        scd.Width = static_cast<UINT>(clientRect.Width());
        scd.Height = static_cast<UINT>(clientRect.Height());
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferCount = FrameCount;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scd.SampleDesc.Count = 1;

        // Direct3D 12 のスワップチェインの作成にはデバイスではなくコマンドキューを渡す
        wil::com_ptr<IDXGISwapChain1> swapChain;
        THROW_IF_FAILED(factory->CreateSwapChainForHwnd(_commandQueue.get(), *this, &scd, nullptr, nullptr, swapChain.put()));
        
        // Direct3D 12 では IDXGISwapChain3::GetCurrentBackBufferIndex が必要
        _swapChain = swapChain.query<IDXGISwapChain3>();
        
        // Direct3D 12 では GetBuffer から返されるリソースは仮想化されていないので
        // レンダリングすべき現在のバッファへの序数をアプリ側で記録しておく必要がある
        _frameIndex = _swapChain->GetCurrentBackBufferIndex();

        // RTV 用のデスクリプタヒープを作成
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors = FrameCount;
        heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        THROW_IF_FAILED(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeap.put())));
        {
            // バックバッファ毎にデスクリプタヒープに RTV を作成
            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
            auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            for (UINT i = 0; i < FrameCount; ++i) {
                THROW_IF_FAILED(_swapChain->GetBuffer(i, IID_PPV_ARGS(_renderTargets[i].put())));
                _device->CreateRenderTargetView(_renderTargets[i].get(), nullptr, rtvHandle);
                rtvHandle.ptr += rtvDescriptorSize;
            }
        }

        // 種類を指定してコマンドアロケータを作成
        THROW_IF_FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_commandAllocator.put())));

        // 種類とアロケータと PSO を指定してコマンドリストを作成。このサンプルでは PSO は未使用
        THROW_IF_FAILED(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.get(), nullptr, IID_PPV_ARGS(_commandList.put())));
        
        // レンダリングループの先頭ではコマンドリストが閉じていることを前提としている
        THROW_IF_FAILED(_commandList->Close());

        // GPU の完了を待機するためのフェンスと待機イベントを作成。
        // フェンスは GPU の進行具合をカウンターで管理する。
        // ここでは 0 を初期値とし、fenceValue は次のフレームの完了を示す値。
        THROW_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_gpuFence.put())));
        _fenceEvent.create(wil::EventOptions::None);
        _fenceValue = 1;
    }

    virtual BOOL OnIdle() {
        OnRender();
        return TRUE;
    }

private:

    void OnRender() {
        // コマンドリストによって確保されたメモリをリセットし再使用できるよう指示。
        // この時コマンドの実行が完了していなければならない
        THROW_IF_FAILED(_commandAllocator->Reset());

        // コマンドリストが再度コマンドを記録できるように指示。
        // この時コマンドリストは closed 状態でなければならない
        THROW_IF_FAILED(_commandList->Reset(_commandAllocator.get(), nullptr));
        
        {
            // バックバッファの状態が PRESENT (COMMON) から RENDER_TARGET に遷移するまで待つリソースバリアを設置
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = _renderTargets[_frameIndex].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _commandList->ResourceBarrier(1, &barrier);
        }
        
        // RTV 用デスクリプタヒープから各 RTV のハンドルの位置を算出
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
        auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        rtvHandle.ptr += _frameIndex * rtvDescriptorSize;

        auto clearColor = D2D1::ColorF(D2D1::ColorF::CornflowerBlue);
        _commandList->ClearRenderTargetView(rtvHandle, &clearColor.r, 0, nullptr);

        {
            // バックバッファの状態が RENDER_TARGET から PRESENT (COMMON) に遷移するまで待つリソースバリアを設置。
            // Present を実行するにはバックバッファが PRESENT (COMMON) 状態でなければならない
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier.Transition.pResource = _renderTargets[_frameIndex].get();
            barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
            barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
            barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            _commandList->ResourceBarrier(1, &barrier);
        }

        // コマンドの記録を終了する。この時コマンドリストが closed 状態だとエラーになる
        THROW_IF_FAILED(_commandList->Close());

        std::array<ID3D12CommandList*, 1> commandLists = {
            _commandList.get(),
        };
        _commandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

        THROW_IF_FAILED(_swapChain->Present(1, 0));

        WaitForGpu();
    }

    void WaitForGpu() {
        // GPU の完了を示すフェンスの値の更新を GPU に指示
        UINT64 value = _fenceValue;
        ++_fenceValue;
        THROW_IF_FAILED(_commandQueue->Signal(_gpuFence.get(), value));

        // フェンスの現在の値を取得し、GPU が完了していなければスレッドを待機させる
        if (_gpuFence->GetCompletedValue() < value) {
            THROW_IF_FAILED(_gpuFence->SetEventOnCompletion(value, _fenceEvent.get()));
            _fenceEvent.wait();
        }

        // GPU が完了していれば現在のフレーム序数を同期させる
        _frameIndex = _swapChain->GetCurrentBackBufferIndex();
    }

private:
    BEGIN_MSG_MAP(MainWindow)
        MSG_WM_SIZE(OnSize)
        MSG_WM_DESTROY(OnDestory)
    END_MSG_MAP()

    void OnSize(UINT type, CSize const& size) {
        if (!_swapChain || type == SIZE_MINIMIZED) {
            return;
        }

        DXGI_SWAP_CHAIN_DESC1 scd;
        _swapChain->GetDesc1(&scd);

        if (size.cx != scd.Width || size.cy != scd.Height) {
            // Direct3D 11 までと同様スワップチェインのリサイズ前にバックバッファへの参照を全て解除する。
            // RTV は COM オブジェクトで無くなったので明示的な解放が不要となる。
            // 逆にリソースが GPU によって使用されていないことを保証するのはアプリ側の責任となる
            for (int i = 0; i < FrameCount; ++i) {
                _renderTargets[i] = nullptr;
            }

            auto width = std::max(static_cast<UINT>(size.cx), 1u);
            auto height = std::max(static_cast<UINT>(size.cy), 1u);
            THROW_IF_FAILED(_swapChain->ResizeBuffers(scd.BufferCount, width, height, scd.Format, scd.Flags));

            // 現在のフレーム序数はリセット
            _frameIndex = _swapChain->GetCurrentBackBufferIndex();

            {
                // RTV も再作成。単純に既存のデスクリプタヒープ上で上書きする
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
                auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

                for (UINT i = 0; i < FrameCount; ++i) {
                    THROW_IF_FAILED(_swapChain->GetBuffer(i, IID_PPV_ARGS(_renderTargets[i].put())));
                    _device->CreateRenderTargetView(_renderTargets[i].get(), nullptr, rtvHandle);
                    rtvHandle.ptr += rtvDescriptorSize;
                }
            }
        }
    }

    void OnDestory() {
        WaitForGpu();

        for (int i = 0; i < FrameCount; ++i) {
            _renderTargets[i] = nullptr;
        }
        
        _gpuFence = nullptr;
        _commandList = nullptr;
        _commandAllocator = nullptr;
        _rtvHeap = nullptr;
        _swapChain = nullptr;
        _commandQueue = nullptr;
        _device = nullptr;

        PostQuitMessage(0);
    }

private:
    wil::com_ptr<ID3D12Device> _device;
    wil::com_ptr<ID3D12CommandQueue> _commandQueue;
    wil::com_ptr<IDXGISwapChain3> _swapChain;
    wil::com_ptr<ID3D12Resource> _renderTargets[FrameCount];
    wil::com_ptr<ID3D12DescriptorHeap> _rtvHeap;
    UINT _frameIndex;
    wil::com_ptr<ID3D12CommandAllocator> _commandAllocator;
    wil::com_ptr<ID3D12GraphicsCommandList> _commandList;
    wil::com_ptr<ID3D12Fence> _gpuFence;
    wil::unique_event _fenceEvent;
    UINT64 _fenceValue;
};

class MessageSpin : public CMessageLoop {
public:
    virtual BOOL OnIdle(int nIdleCount) {
        CMessageLoop::OnIdle(nIdleCount);
        return TRUE;
    }
};

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, WCHAR*, int)
{
    //SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    _Module.Init(nullptr, hInstance);

    MessageSpin kicker;
    _Module.AddMessageLoop(&kicker);

    MainWindow app;
    app.Launch();

    kicker.AddIdleHandler(&app);
    auto wParam = kicker.Run();

    _Module.RemoveMessageLoop();
    _Module.Term();

    return wParam;

}