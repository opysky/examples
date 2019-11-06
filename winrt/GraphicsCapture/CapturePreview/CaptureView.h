#pragma once

class CaptureView : public CWindowImpl<CaptureView> {
public:
	DECLARE_WND_CLASS_EX(nullptr, CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS, -1)

	HRESULT CreateDevice();
	bool StartCapture(winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item);
	bool StartCaptureForHwnd(HWND hwndItem);
	void StopCapture();
	bool IsCapturing() { return _framePool != nullptr; }

private:
	BEGIN_MSG_MAP(CaptureView)
		MSG_WM_PAINT(OnPaint)
		MSG_WM_SIZE(OnSize)
		MSG_WM_DESTROY(OnDestroy)
	END_MSG_MAP()

	void OnPaint(CDCHandle);
	void OnSize(UINT nType, CSize const& size);
	void OnDestroy();

private:
	void OnFrameArrived(
		winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
		winrt::Windows::Foundation::IInspectable const& args);

private:
	winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice _device{ nullptr };
	winrt::com_ptr<ID3D11Device> _d3dDevice;
	winrt::com_ptr<IDXGISwapChain1> _dxgiSwapChain;
	winrt::com_ptr<ID3D11RenderTargetView> _chainedBufferRTV;

	std::unique_ptr<::DirectX::SpriteBatch> _spriteBatch;

	winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool _framePool{ nullptr };
	winrt::Windows::Graphics::Capture::GraphicsCaptureItem _captureItem{ nullptr };
	winrt::Windows::Graphics::Capture::GraphicsCaptureSession _captureSession{ nullptr };
	winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker _frameArrived;
};