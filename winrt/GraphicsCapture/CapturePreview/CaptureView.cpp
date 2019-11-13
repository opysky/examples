#include "stdafx.h"
#include "CaptureView.h"
#include "Direct3DHelper.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;

using namespace ::DirectX;

namespace {

auto CreateCaptureItemForWindow(HWND hwnd)
{
	namespace abi = ABI::Windows::Graphics::Capture;

	auto factory = get_activation_factory<GraphicsCaptureItem>();
	auto interop = factory.as<IGraphicsCaptureItemInterop>();
	GraphicsCaptureItem item{ nullptr };
	check_hresult(interop->CreateForWindow(hwnd, guid_of<abi::IGraphicsCaptureItem>(), reinterpret_cast<void**>(put_abi(item))));
	return item;
}

auto FitInBox(Size const& source, Size const& destination)
{
	// アスペクト比を保持したままボックスに収まる矩形を計算
	Rect box;

	box.Width = destination.Width;
	box.Height = destination.Height;
	float aspect = source.Width / source.Height;
	if (box.Width >= box.Height * aspect) {
		box.Width = box.Height * aspect;
	}
	aspect = source.Height / source.Width;
	if (box.Height >= box.Width * aspect) {
		box.Height = box.Width * aspect;
	}
	box.X = (destination.Width - box.Width) * 0.5f;
	box.Y = (destination.Height - box.Height) * 0.5f;

	return CRect(
		static_cast<int>(box.X), 
		static_cast<int>(box.Y), 
		static_cast<int>(box.X + box.Width), 
		static_cast<int>(box.Y + box.Height));
}

} 

HRESULT CaptureView::CreateDevice()
{
	WINRT_VERIFY(IsWindow());
	
	_device = CreateDirect3DDevice();
	_d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(_device);

	auto dxgiDevice = _d3dDevice.as<IDXGIDevice2>();
	com_ptr<IDXGIAdapter> dxgiAdapter;
	check_hresult(dxgiDevice->GetParent(guid_of<IDXGIAdapter>(), dxgiAdapter.put_void()));
	com_ptr<IDXGIFactory2> dxgiFactory;
	check_hresult(dxgiAdapter->GetParent(guid_of<IDXGIFactory2>(), dxgiFactory.put_void()));

	CRect clientRect;
	GetClientRect(&clientRect);
	DXGI_SWAP_CHAIN_DESC1 scd = {};
	scd.Width = clientRect.Width();
	scd.Height = clientRect.Height();
	scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	scd.SampleDesc.Count = 1;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

	check_hresult(dxgiFactory->CreateSwapChainForHwnd(
		_d3dDevice.get(),
		*this,
		&scd,
		nullptr,
		nullptr,
		_dxgiSwapChain.put()));

	com_ptr<ID3D11Texture2D> chainedBuffer;
	check_hresult(_dxgiSwapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), chainedBuffer.put_void()));
	check_hresult(_d3dDevice->CreateRenderTargetView(chainedBuffer.get(), nullptr, _chainedBufferRTV.put()));

	com_ptr<ID3D11DeviceContext> context;
	_d3dDevice->GetImmediateContext(context.put());
	_spriteBatch = std::make_unique<SpriteBatch>(context.get());
	
	return S_OK;
}

void CaptureView::StartCapture(winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item)
{
	// Direct3D11CaptureFramePool は使いまわせても良さそうな気がするけど
	// GraphicsCaptureSession と一緒に破棄しないとダメっぽ
	StopCapture();

    _captureItem = item;

	_framePool = Direct3D11CaptureFramePool::Create(
		_device,
		DirectXPixelFormat::B8G8R8A8UIntNormalized,
		2,
		_captureItem.Size());
    _frameArrived = _framePool.FrameArrived(auto_revoke, { this, &CaptureView::OnFrameArrived });
	
    _captureSession = _framePool.CreateCaptureSession(item);
	_captureSession.StartCapture();
}

void CaptureView::StartCaptureForHwnd(HWND hwndItem)
{
	StartCapture(CreateCaptureItemForWindow(hwndItem));
}

void CaptureView::StopCapture()
{
	if (IsCapturing()) {		
		_frameArrived.revoke();

        _captureSession = nullptr;

		// 何故か Direct3DCaptureFramePool は Close を手動で呼び出さないと
        // 参照カウンタが残ってるというレポートが吐かれる？
        _framePool.Close();
		_framePool = nullptr;
		
        _captureItem = nullptr;
	}
}

void CaptureView::OnPaint(CDCHandle)
{
	CPaintDC dc(*this);

	if (!_dxgiSwapChain || IsCapturing()) {
		return;
	}

	com_ptr<ID3D11DeviceContext> context;
	_d3dDevice->GetImmediateContext(context.put());

	ID3D11RenderTargetView* pRTVs[1] = { _chainedBufferRTV.get() };
	context->OMSetRenderTargets(1, pRTVs, nullptr);

	auto clearColor = D2D1::ColorF(D2D1::ColorF::CornflowerBlue);
	context->ClearRenderTargetView(_chainedBufferRTV.get(), &clearColor.r);

	DXGI_PRESENT_PARAMETERS pp = { 0 };
	_dxgiSwapChain->Present1(1, 0, &pp);
}

void CaptureView::OnSize(UINT nType, CSize const& size)
{
	if (_dxgiSwapChain == nullptr) {
		return;
	}

	if ((nType == SIZE_RESTORED || nType == SIZE_MAX) && size.cx > 0 && size.cy) {
		_chainedBufferRTV = nullptr;
		_dxgiSwapChain->ResizeBuffers(2, size.cx, size.cy, DXGI_FORMAT_B8G8R8A8_UNORM, 0);
		
		com_ptr<ID3D11Texture2D> chainedBuffer;
		check_hresult(_dxgiSwapChain->GetBuffer(0, guid_of<ID3D11Texture2D>(), chainedBuffer.put_void()));
		check_hresult(_d3dDevice->CreateRenderTargetView(chainedBuffer.get(), nullptr, _chainedBufferRTV.put()));

		Invalidate();
	}
}

void CaptureView::OnDestroy()
{
	StopCapture();

	_spriteBatch = nullptr;
	_chainedBufferRTV = nullptr;
	_dxgiSwapChain = nullptr;
	_d3dDevice = nullptr;
	_device = nullptr;
}

void CaptureView::OnFrameArrived(
	Direct3D11CaptureFramePool const& sender, 
	winrt::Windows::Foundation::IInspectable const& args)
{
	auto frame = sender.TryGetNextFrame();
	auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());

	// なんでか知らんが CaptureItem::Size と CaptureFrame::ContentSize が異なる場合が多い
	// レンダリングは ContentSize を基準にするべき
	auto contentSize = frame.ContentSize();

	com_ptr<ID3D11ShaderResourceView> frameSurfaceSRV;
	check_hresult(_d3dDevice->CreateShaderResourceView(frameSurface.get(), nullptr, frameSurfaceSRV.put()));

	com_ptr<ID3D11DeviceContext> context;
	_d3dDevice->GetImmediateContext(context.put());

	ID3D11RenderTargetView* pRTVs[1];
	pRTVs[0] = _chainedBufferRTV.get();
	context->OMSetRenderTargets(1, pRTVs, nullptr);

	D3D11_VIEWPORT vp = { 0 };
	DXGI_SWAP_CHAIN_DESC1 scd;
	_dxgiSwapChain->GetDesc1(&scd);
	vp.Width = static_cast<float>(scd.Width);
	vp.Height = static_cast<float>(scd.Height);
	context->RSSetViewports(1, &vp);

	auto clearColor = D2D1::ColorF(D2D1::ColorF::CornflowerBlue);
	context->ClearRenderTargetView(_chainedBufferRTV.get(), &clearColor.r);

	_spriteBatch->Begin();

	CRect sourceRect, destinationRect;

	sourceRect.left = 0;
	sourceRect.top = 0;
	sourceRect.right = contentSize.Width;
	sourceRect.bottom = contentSize.Height;

	destinationRect = FitInBox(
		{ static_cast<float>(contentSize.Width), static_cast<float>(contentSize.Height) }, 
		{ static_cast<float>(scd.Width), static_cast<float>(scd.Height) });

	_spriteBatch->Draw(frameSurfaceSRV.get(), destinationRect, &sourceRect);

	_spriteBatch->End();

	DXGI_PRESENT_PARAMETERS pp = { 0 };
	_dxgiSwapChain->Present1(1, 0, &pp);

	// Surface のサイズはおそらく FramePool 作成時のバッファサイズ。
	// CaptureItem のサイズはウィンドウのサイズに追従するので
	// 差異があるなら FramePool を再作成する
    auto surfaceDesc = frame.Surface().Description();
	auto itemSize = _captureItem.Size();
	if (itemSize.Width != surfaceDesc.Width || itemSize.Height != surfaceDesc.Height) {
		// GraphicsCaptureItem::Closed は参照しているウィンドウが破棄されたら
		// 発行されるイベントかと思ったけどそういうわけでも無さそう…
		// Size が 0 なら破棄されたと見做すしかないかな？
        SizeInt32 size;
		size.Width = std::max(itemSize.Width, 1);
		size.Height = std::max(itemSize.Height, 1);
		_framePool.Recreate(_device, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
	}
}
