#include "stdafx.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace {

    const UINT FrameCount = 2;
    const wchar_t* ApplicationTitle = L"01_SimpleCube";

    struct VertexPositionColor {
        Vector3 position;
        Color color;
    };

    struct ConstantBuffer {
        Matrix worldViewProjectionMatrix;
    };
}

class MainWindow : public CWindowImpl<MainWindow>, public CIdleHandler {
public:
    DECLARE_WND_CLASS_EX(nullptr, CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, -1)

    void Launch() {
        Create(nullptr, CWindow::rcDefault, ApplicationTitle, WS_OVERLAPPEDWINDOW);

        CenterWindow();
        ShowWindow(SW_SHOWNORMAL);
        Invalidate();

        UpdateStatusText(0.0);

        UINT factoryFlags = 0;
#if defined(_DEBUG)
        {
            wil::com_ptr<ID3D12Debug> debugController;
            THROW_IF_FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(debugController.put())));
            debugController->EnableDebugLayer();
            factoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
#endif
        wil::com_ptr<IDXGIFactory5> factory;
        THROW_IF_FAILED(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(factory.put())));
        
        wil::com_ptr<IDXGIAdapter1> adapter;
        for (UINT i = 0; DXGI_ERROR_NOT_FOUND != factory->EnumAdapters1(i, adapter.put()); ++i) {
            DXGI_ADAPTER_DESC1 desc;
            adapter->GetDesc1(&desc);
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
                continue;
            }
            if (SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr))) {
                break;
            }
        }

        THROW_IF_FAILED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(_device.put())));

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        THROW_IF_FAILED(_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(_commandQueue.put())));

        CRect clientRect;
        GetClientRect(&clientRect);

        BOOL tearingSupported = FALSE;
        THROW_IF_FAILED(factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &tearingSupported, sizeof(tearingSupported)));
        UINT swapChainFlags = 0;
        if (tearingSupported) {
            swapChainFlags |= DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
        }

        DXGI_SWAP_CHAIN_DESC1 scd = {};
        scd.Width = static_cast<UINT>(clientRect.Width());
        scd.Height = static_cast<UINT>(clientRect.Height());
        scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        scd.BufferCount = FrameCount;
        scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        scd.SampleDesc.Count = 1;
        scd.Flags = swapChainFlags;

        wil::com_ptr<IDXGISwapChain1> swapChain;
        THROW_IF_FAILED(factory->CreateSwapChainForHwnd(_commandQueue.get(), *this, &scd, nullptr, nullptr, swapChain.put()));
        
        _swapChain = swapChain.query<IDXGISwapChain3>();
        
        _frameIndex = _swapChain->GetCurrentBackBufferIndex();

        {
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = FrameCount;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
            THROW_IF_FAILED(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_rtvHeap.put())));

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
            auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

            for (UINT i = 0; i < FrameCount; ++i) {
                THROW_IF_FAILED(_swapChain->GetBuffer(i, IID_PPV_ARGS(_renderTargets[i].put())));
                _device->CreateRenderTargetView(_renderTargets[i].get(), nullptr, rtvHandle);
                rtvHandle.ptr += rtvDescriptorSize;
            }
        }

        {
            // 深度ステンシルバッファの作成
            auto dsd = CD3DX12_RESOURCE_DESC::Tex2D(
                DXGI_FORMAT_D32_FLOAT, 
                clientRect.Width(), 
                clientRect.Height(), 
                1, 0, 1, 0, 
                D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

            THROW_IF_FAILED(_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAG_NONE, 
                &dsd, 
                D3D12_RESOURCE_STATE_DEPTH_WRITE, 
                &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0x0),
                IID_PPV_ARGS(_depthBuffer.put())));

            // DSV 用のデスクリプタヒープを作成
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = 1;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            THROW_IF_FAILED(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_dsvHeap.put())));

            // DSV の作成
            D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
            dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
            dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
            _device->CreateDepthStencilView(_depthBuffer.get(), &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart());
        }

        THROW_IF_FAILED(_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(_commandAllocator.put())));

        THROW_IF_FAILED(_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _commandAllocator.get(), nullptr, IID_PPV_ARGS(_commandList.put())));
        
        THROW_IF_FAILED(_commandList->Close());

        THROW_IF_FAILED(_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(_gpuFence.put())));
        _fenceEvent.create(wil::EventOptions::None);
        _fenceValue = 1;

        {
            // キューブ描画用のルートシグニチャを作成

            // CBV のデスクリプタを 1 つ持つ範囲
            CD3DX12_DESCRIPTOR_RANGE ranges[1];
            ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

            // その範囲を持つデスクリプタテーブルが 1 つとしてルートシグニチャを定義
            CD3DX12_ROOT_PARAMETER parameters[1];
            parameters[0].InitAsDescriptorTable(1, ranges, D3D12_SHADER_VISIBILITY_VERTEX);

            D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
                D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
                D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;

            CD3DX12_ROOT_SIGNATURE_DESC rsd;
            rsd.Init(1, parameters, 0, nullptr, rootSignatureFlags);

            wil::com_ptr<ID3DBlob> signature;
            wil::com_ptr<ID3DBlob> error;
            THROW_IF_FAILED(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, signature.put(), error.put()));
            THROW_IF_FAILED(_device->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(_rootSignature.put())));
        }

        {
            // キューブ描画用の PSO の作成
            wil::com_ptr<ID3DBlob> vsBytecode;
            wil::com_ptr<ID3DBlob> psBytecode;
            wil::com_ptr<ID3DBlob> error;
            UINT compileFlags = 0;
#if defined(_DEBUG)
            compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            auto shaderFilename = GetAssetsPath(L"Shaders\\simple.hlsl");
            THROW_IF_FAILED(D3DCompileFromFile(shaderFilename.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, vsBytecode.put(), error.put()));
            THROW_IF_FAILED(D3DCompileFromFile(shaderFilename.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, psBytecode.put(), error.put()));

            std::array<D3D12_INPUT_ELEMENT_DESC, 2> inputLayout = {{
                {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
                {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            }};

            D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
            psoDesc.pRootSignature = _rootSignature.get();
            psoDesc.InputLayout = { inputLayout.data(), inputLayout.size() };
            psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBytecode.get());
            psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBytecode.get());
            psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
            psoDesc.SampleMask = UINT_MAX;
            psoDesc.SampleDesc.Count = 1;
            psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            psoDesc.NumRenderTargets = 1;
            psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
            psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            THROW_IF_FAILED(_device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(_pipelineState.put())));
        }

        {
            // 頂点バッファの作成            
            VertexPositionColor cubeVertices[] = {
                {{ 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
                {{ 1.0f,-1.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
                {{-1.0f,-1.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
                {{-1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f}},
                {{ 1.0f, 1.0f,-1.0f}, {0.0f, 1.0f, 1.0f, 1.0f}},
                {{ 1.0f,-1.0f,-1.0f}, {1.0f, 1.0f, 0.0f, 1.0f}},
                {{-1.0f,-1.0f,-1.0f}, {0.5f, 0.5f, 0.5f, 1.0f}},
                {{-1.0f, 1.0f,-1.0f}, {0.0f, 0.0f, 0.0f, 1.0f}},
            };
            
            UINT vertexBufferSize = sizeof(cubeVertices);

            // 静的な頂点バッファなら本来は Default ヒープに作成すべき
            THROW_IF_FAILED(_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(_vertexBuffer.put())));

            UINT8* mappedData;
            CD3DX12_RANGE readRange(0, 0);
            THROW_IF_FAILED(_vertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
            std::memcpy(mappedData, cubeVertices, vertexBufferSize);
            _vertexBuffer->Unmap(0, nullptr);
        }

        {
            // インデックスバッファの作成
            uint32_t cubeIndices[] = { 
                0, 1, 2, 0, 2, 3, 
                4, 5, 1, 4, 1, 0, 
                7, 6, 5, 7, 5, 4, 
                3, 2, 6, 3, 6, 7, 
                0, 3, 4, 3, 7, 4, 
                1, 5, 6, 1, 6, 2, 
            };

            UINT indexBufferSize = sizeof(cubeIndices);

            // 静的なインデックスバッファなら本来は Default ヒープに作成すべき
            THROW_IF_FAILED(_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(_indexBuffer.put())));

            UINT8* mappedData;
            CD3DX12_RANGE readRange(0, 0);
            THROW_IF_FAILED(_indexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
            std::memcpy(mappedData, cubeIndices, indexBufferSize);
            _indexBuffer->Unmap(0, nullptr);
        }

        {
            // 定数バッファの作成
            // 毎フレーム CPU から書き込むので Upload ヒープが望ましい？
            THROW_IF_FAILED(_device->CreateCommittedResource(
                &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
                D3D12_HEAP_FLAG_NONE,
                &CD3DX12_RESOURCE_DESC::Buffer(1024 * 64),
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                IID_PPV_ARGS(_constantBuffer.put())));

            // CBV 用のデスクリプタヒープを作成
            D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
            heapDesc.NumDescriptors = 1;
            heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
            heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
            THROW_IF_FAILED(_device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(_cbvHeap.put())));

            // CBV を作成
            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation = _constantBuffer->GetGPUVirtualAddress();
            cbvDesc.SizeInBytes = (sizeof(ConstantBuffer) + 255) & ~255;
            _device->CreateConstantBufferView(&cbvDesc, _cbvHeap->GetCPUDescriptorHandleForHeapStart());

            // Unmap しなくてもコマンドリストがバッファの内容を GPU へ反映してくれるらしい
            CD3DX12_RANGE readRange(0, 0);
            THROW_IF_FAILED(_constantBuffer->Map(0, &readRange, reinterpret_cast<void**>(&_cbMappedData)));
            *_cbMappedData = {};
        }

        _stopwatch.start();
    }

    virtual BOOL OnIdle() {
        static double lastTime = 0;
        double currentTime = static_cast<float>(_stopwatch.elapsedSec());
        double deltaTime = currentTime - lastTime;
        lastTime = currentTime;

        static double fpsValue = 0;
        _fpsCounter.update(deltaTime);
        double fps = _fpsCounter.currentFps();
        if (fps != fpsValue) {
            UpdateStatusText(fps);
            fpsValue = fps;
        }
        
        OnUpdate(static_cast<float>(deltaTime));
        
        OnRender();
        
        return TRUE;
    }

private:
    std::wstring GetAssetsPath(std::wstring const& filename) {
        std::array<wchar_t, MAX_PATH> appDirPath;
        GetModuleFileName(nullptr, appDirPath.data(), appDirPath.size());
        PathRemoveFileSpec(appDirPath.data());
        std::wstring result(appDirPath.data());
        return result.append(L"\\..\\..\\Assets\\").append(filename);
    }

    void UpdateStatusText(double fps) {
        std::array<wchar_t, 256> text;
        std::swprintf(text.data(), text.size(), L"%ls [fps: %.2lf]\n", ApplicationTitle, fps);
        SetWindowText(text.data());
    }

    void OnUpdate(float deltaTime) {
        CRect clientRect;
        GetClientRect(&clientRect);
        float aspectRatio = static_cast<float>(clientRect.Width()) / clientRect.Height();

        static float yAngle = 0.0f;
        yAngle += 45.0f * deltaTime;

        auto object = Matrix::CreateRotationY(XMConvertToRadians(yAngle));
        auto camera = Matrix::CreateLookAt(Vector3(3, 3, 5), Vector3::Zero, Vector3::Up);
        auto projection = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45), aspectRatio, 0.1f, 100.0f);

        _cbMappedData->worldViewProjectionMatrix =  object * camera * projection;
    }

    void OnRender() {
        THROW_IF_FAILED(_commandAllocator->Reset());

        THROW_IF_FAILED(_commandList->Reset(_commandAllocator.get(), nullptr));

        {
            // フレームバッファのバインド及びクリア
            CRect clientRect;
            GetClientRect(&clientRect);
            CD3DX12_VIEWPORT viewport(0.0f, 0.0f, static_cast<FLOAT>(clientRect.Width()), static_cast<FLOAT>(clientRect.Height()));
            CD3DX12_RECT scissorRect(0, 0, clientRect.Width(), clientRect.Height());

            _commandList->RSSetViewports(1, &viewport);
            _commandList->RSSetScissorRects(1, &scissorRect);

            _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_renderTargets[_frameIndex].get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

            D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
            auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            rtvHandle.ptr += _frameIndex * rtvDescriptorSize;

            auto dsvHandle = _dsvHeap->GetCPUDescriptorHandleForHeapStart();

            _commandList->OMSetRenderTargets(1, &rtvHandle, FALSE, &dsvHandle);

            _commandList->ClearRenderTargetView(rtvHandle, Colors::CornflowerBlue, 0, nullptr);
            _commandList->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0x0, 0, nullptr);
        }

        {
            // 立方体のレンダリング
            _commandList->SetPipelineState(_pipelineState.get());
            
            _commandList->SetGraphicsRootSignature(_rootSignature.get());

            std::array<ID3D12DescriptorHeap*, 1> descriptorHeaps = {
                _cbvHeap.get(), 
            };
            _commandList->SetDescriptorHeaps(descriptorHeaps.size(), descriptorHeaps.data());

            _commandList->SetGraphicsRootDescriptorTable(0, _cbvHeap->GetGPUDescriptorHandleForHeapStart());
            
            _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // VBV/IBV は保持しておく必要はないみたい

            D3D12_VERTEX_BUFFER_VIEW vertexBufferView = {};
            vertexBufferView.BufferLocation = _vertexBuffer->GetGPUVirtualAddress();
            vertexBufferView.SizeInBytes = static_cast<UINT>(_vertexBuffer->GetDesc().Width);
            vertexBufferView.StrideInBytes = sizeof(VertexPositionColor);
            _commandList->IASetVertexBuffers(0, 1, &vertexBufferView);

            D3D12_INDEX_BUFFER_VIEW indexBufferView = {};
            indexBufferView.BufferLocation = _indexBuffer->GetGPUVirtualAddress();
            indexBufferView.Format = DXGI_FORMAT_R32_UINT;
            indexBufferView.SizeInBytes = static_cast<UINT>(_indexBuffer->GetDesc().Width);
            _commandList->IASetIndexBuffer(&indexBufferView);

            _commandList->DrawIndexedInstanced(36, 1, 0, 0, 0);
        }

        _commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(_renderTargets[_frameIndex].get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

        THROW_IF_FAILED(_commandList->Close());

        std::array<ID3D12CommandList*, 1> commandLists = {
            _commandList.get(),
        };
        _commandQueue->ExecuteCommandLists(commandLists.size(), commandLists.data());

        // 今は DXGI_PRESENT_ALLOW_TEARING を明示しないと 
        // SyncInterval を 0 にしただけでは垂直同期を無視できない環境があるっぽい？
        // 条件が良くわからない…
        bool allowTearing = false;
        UINT syncInterval = 1;
        UINT presentFlags = 0;
        DXGI_PRESENT_PARAMETERS pp = {};
        if (allowTearing) {
            syncInterval = 0;
            presentFlags |= DXGI_PRESENT_ALLOW_TEARING;
        }
        THROW_IF_FAILED(_swapChain->Present1(syncInterval, presentFlags, &pp));

        WaitForGpu();
    }

    void WaitForGpu() {
        UINT64 value = _fenceValue;
        ++_fenceValue;
        THROW_IF_FAILED(_commandQueue->Signal(_gpuFence.get(), value));

        if (_gpuFence->GetCompletedValue() < value) {
            THROW_IF_FAILED(_gpuFence->SetEventOnCompletion(value, _fenceEvent.get()));
            _fenceEvent.wait();
        }

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
            for (int i = 0; i < FrameCount; ++i) {
                _renderTargets[i] = nullptr;
            }

            auto width = std::max(static_cast<UINT>(size.cx), 1u);
            auto height = std::max(static_cast<UINT>(size.cy), 1u);
            THROW_IF_FAILED(_swapChain->ResizeBuffers(scd.BufferCount, width, height, scd.Format, scd.Flags));

            _frameIndex = _swapChain->GetCurrentBackBufferIndex();

            {
                D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = _rtvHeap->GetCPUDescriptorHandleForHeapStart();
                auto rtvDescriptorSize = _device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

                for (UINT i = 0; i < FrameCount; ++i) {
                    THROW_IF_FAILED(_swapChain->GetBuffer(i, IID_PPV_ARGS(_renderTargets[i].put())));
                    _device->CreateRenderTargetView(_renderTargets[i].get(), nullptr, rtvHandle);
                    rtvHandle.ptr += rtvDescriptorSize;
                }
            }

            {
                // 深度ステンシルバッファもリサイズのため再作成
                auto dsd = CD3DX12_RESOURCE_DESC::Tex2D(
                    DXGI_FORMAT_D32_FLOAT,
                    width,
                    height,
                    1, 0, 1, 0,
                    D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);

                THROW_IF_FAILED(_device->CreateCommittedResource(
                    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                    D3D12_HEAP_FLAG_NONE,
                    &dsd,
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    &CD3DX12_CLEAR_VALUE(DXGI_FORMAT_D32_FLOAT, 1.0f, 0x0),
                    IID_PPV_ARGS(_depthBuffer.put())));

                D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
                dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
                dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
                _device->CreateDepthStencilView(_depthBuffer.get(), &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart());
            }
        }
    }

    void OnDestory() {
        WaitForGpu();

        for (int i = 0; i < FrameCount; ++i) {
            _renderTargets[i] = nullptr;
        }

        _cbvHeap = nullptr;
        _constantBuffer = nullptr;
        _indexBuffer = nullptr;
        _vertexBuffer = nullptr;
        _pipelineState = nullptr;
        _rootSignature = nullptr;
        
        _gpuFence = nullptr;
        _commandList = nullptr;
        _commandAllocator = nullptr;
        _dsvHeap = nullptr;
        _depthBuffer = nullptr;
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
    wil::com_ptr<ID3D12Resource> _depthBuffer;
    wil::com_ptr<ID3D12DescriptorHeap> _dsvHeap;
    UINT _frameIndex;
    wil::com_ptr<ID3D12CommandAllocator> _commandAllocator;
    wil::com_ptr<ID3D12GraphicsCommandList> _commandList;
    wil::com_ptr<ID3D12Fence> _gpuFence;
    wil::unique_event _fenceEvent;
    UINT64 _fenceValue;

    wil::com_ptr<ID3D12RootSignature> _rootSignature;
    wil::com_ptr<ID3D12PipelineState> _pipelineState;
    wil::com_ptr<ID3D12Resource> _vertexBuffer;
    wil::com_ptr<ID3D12Resource> _indexBuffer;
    wil::com_ptr<ID3D12DescriptorHeap> _cbvHeap;
    wil::com_ptr<ID3D12Resource> _constantBuffer;
    ConstantBuffer* _cbMappedData;

    Stopwatch _stopwatch;
    FPSCounter _fpsCounter;
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