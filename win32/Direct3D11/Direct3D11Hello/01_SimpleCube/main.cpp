#include "stdafx.h"
#include "Direct3D11Application.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

namespace {
    struct ConstantBuffer {
        Matrix worldViewProjectionMatrix;
    };

}

class App : public Direct3D11Application
{
    void beginRun() override 
    {
        auto device = nativeDevice();
        auto context = nativeDeviceContext();

        {
            wil::com_ptr<ID3DBlob> bytecode;
            wil::com_ptr<ID3DBlob> error;
            UINT compileFlags = 0;
#if defined(_DEBUG)
            compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
            auto shaderFile = assetsDirPath().append(L"Shaders\\simple.hlsl");

            THROW_IF_FAILED(D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "VSMain", "vs_5_0", compileFlags, 0, bytecode.put(), error.put()));
            THROW_IF_FAILED(device->CreateVertexShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, _vertexShader.put()));

            THROW_IF_FAILED(device->CreateInputLayout(
                VertexPositionColor::InputElements,
                VertexPositionColor::InputElementCount,
                bytecode->GetBufferPointer(),
                bytecode->GetBufferSize(),
                _inputLayout.put()));

            THROW_IF_FAILED(D3DCompileFromFile(shaderFile.c_str(), nullptr, nullptr, "PSMain", "ps_5_0", compileFlags, 0, bytecode.put(), error.put()));
            THROW_IF_FAILED(device->CreatePixelShader(bytecode->GetBufferPointer(), bytecode->GetBufferSize(), nullptr, _pixelShader.put()));
        }

        {
            VertexPositionColor cubeVertices[] = {
                {XMFLOAT3( 1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f)},
                {XMFLOAT3( 1.0f,-1.0f, 1.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f)},
                {XMFLOAT3(-1.0f,-1.0f, 1.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f)},
                {XMFLOAT3(-1.0f, 1.0f, 1.0f), XMFLOAT4(1.0f, 0.0f, 1.0f, 1.0f)},
                {XMFLOAT3( 1.0f, 1.0f,-1.0f), XMFLOAT4(0.0f, 1.0f, 1.0f, 1.0f)},
                {XMFLOAT3( 1.0f,-1.0f,-1.0f), XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f)},
                {XMFLOAT3(-1.0f,-1.0f,-1.0f), XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f)},
                {XMFLOAT3(-1.0f, 1.0f,-1.0f), XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f)},
            };

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(cubeVertices);
            bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = cubeVertices;
            THROW_IF_FAILED(device->CreateBuffer(&bd, &initData, _vertexBuffer.put()));
        }

        {
            uint32_t cubeIndices[] = {
                0, 1, 2, 0, 2, 3,
                4, 5, 1, 4, 1, 0,
                7, 6, 5, 7, 5, 4,
                3, 2, 6, 3, 6, 7,
                0, 3, 4, 3, 7, 4,
                1, 5, 6, 1, 6, 2,
            };

            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(cubeIndices);
            bd.BindFlags = D3D11_BIND_INDEX_BUFFER;

            D3D11_SUBRESOURCE_DATA initData = {};
            initData.pSysMem = cubeIndices;
            THROW_IF_FAILED(device->CreateBuffer(&bd, &initData, _indexBuffer.put()));
        }

        {
            D3D11_BUFFER_DESC bd = {};
            bd.Usage = D3D11_USAGE_DEFAULT;
            bd.ByteWidth = sizeof(ConstantBuffer);
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;

            THROW_IF_FAILED(device->CreateBuffer(&bd, nullptr, _constantBuffer.put()));
        }

        _commonStates = std::make_unique<CommonStates>(device.get());

        showStatistics(true);
        showCaps(true);
    }

    void endRun() override 
    {
        _commonStates = nullptr;
        _constantBuffer = nullptr;
        _indexBuffer = nullptr;
        _vertexBuffer = nullptr;
        _inputLayout = nullptr;
        _pixelShader = nullptr;
        _vertexShader = nullptr;
    }

    void onUpdate(float deltaTime) override
    {
        RECT rc;
        GetClientRect(hWndApp(), &rc);
        float aspectRatio = static_cast<float>(rc.right - rc.left) / (rc.bottom - rc.top);

        static float yAngle = 0.0f;
        yAngle += 45.0f * deltaTime;

        auto object = Matrix::CreateRotationY(XMConvertToRadians(yAngle));
        auto camera = Matrix::CreateLookAt(Vector3(3, 3, 5), Vector3::Zero, Vector3::Up);
        auto projection = Matrix::CreatePerspectiveFieldOfView(XMConvertToRadians(45), aspectRatio, 0.1f, 100.0f);

        _cbuffer.worldViewProjectionMatrix = object * camera * projection;
    }

    void onDraw(wil::com_ptr<ID3D11DeviceContext> const& context) override
    {
        context->RSSetState(_commonStates->CullCounterClockwise());
        context->OMSetDepthStencilState(_commonStates->DepthDefault(), 0x0);

        context->IASetInputLayout(_inputLayout.get());
        
        ID3D11Buffer* vertexBuffers[] = {
            _vertexBuffer.get(),
        };
        UINT stride = sizeof(VertexPositionColor);
        UINT offset = 0;
        context->IASetVertexBuffers(0, _countof(vertexBuffers), vertexBuffers, &stride, &offset);

        context->IASetIndexBuffer(_indexBuffer.get(), DXGI_FORMAT_R32_UINT, 0);

        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        context->VSSetShader(_vertexShader.get(), nullptr, 0);

        context->PSSetShader(_pixelShader.get(), nullptr, 0);

        context->UpdateSubresource(_constantBuffer.get(), 0, nullptr, &_cbuffer, 0, 0);

        ID3D11Buffer* constantBuffers[] = {
            _constantBuffer.get(),
        };
        context->VSSetConstantBuffers(0, _countof(constantBuffers), constantBuffers);

        context->DrawIndexed(36, 0, 0);
    }

    void onImGui() override
    {
        
    }

    void onKeyEvent(KeyAction action, int code, uint32_t modifiers) override
    {
        if (action == KeyAction::Pressed) {
            switch (code) {
            case 'V': {
                static bool _allowTearing = false;
                _allowTearing = !_allowTearing;
                allowTearing(_allowTearing);
                break;
            }
            }
        }
    }

private:
    wil::com_ptr<ID3D11VertexShader> _vertexShader;
    wil::com_ptr<ID3D11PixelShader> _pixelShader;
    wil::com_ptr<ID3D11InputLayout> _inputLayout;
    wil::com_ptr<ID3D11Buffer> _vertexBuffer;
    wil::com_ptr<ID3D11Buffer> _indexBuffer;
    wil::com_ptr<ID3D11Buffer> _constantBuffer;
    ConstantBuffer _cbuffer;
    std::unique_ptr<CommonStates> _commonStates;
};

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, WCHAR*, int)
{
    App app;
    return app.run(hInstance, L"01_SimpleCube");
}