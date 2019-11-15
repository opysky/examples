#include "stdafx.h"
#include "CaptureRecorder.h"
#include "Direct3DHelper.h"

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Media::Core;
using namespace winrt::Windows::Media::MediaProperties;
using namespace winrt::Windows::Media::Transcoding;
using namespace winrt::Windows::Graphics;
using namespace winrt::Windows::Graphics::DirectX;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;

using namespace ::DirectX;

namespace {

void print_context()
{
    APTTYPE type;
    APTTYPEQUALIFIER qualifier;
    HRESULT const result = CoGetApartmentType(&type, &qualifier);
    wchar_t const* pstrType;
    if (result == S_OK) {
        pstrType = type == APTTYPE_MTA ? L"MTA" : L"STA";
    } else {
        pstrType = L"N/A";
    }

    std::array<wchar_t, 256> text;
    swprintf(text.data(), text.size(), L"thread:%d apartment:%ls\n", GetCurrentThreadId(), pstrType);
    OutputDebugString(text.data());
}

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

CaptureRecorder::CaptureRecorder()
{
    _device = CreateDirect3DDevice();
    _d3dDevice = GetDXGIInterfaceFromObject<ID3D11Device>(_device);

    com_ptr<ID3D11DeviceContext> context;
    _d3dDevice->GetImmediateContext(context.put());
    _spriteBatch = std::make_unique<SpriteBatch>(context.get());

    _transcoder = MediaTranscoder();
    _transcoder.HardwareAccelerationEnabled(true);

    _outputProfile = MediaEncodingProfile::CreateMp4(VideoEncodingQuality::HD1080p);
    _outputProfile.Video().FrameRate().Numerator(60);
    _outputProfile.Video().FrameRate().Denominator(1);
}

IAsyncAction CaptureRecorder::StartRecordAsync(
    GraphicsCaptureItem const& item, 
    IRandomAccessStream const& stream)
{
    auto framePool = Direct3D11CaptureFramePool::Create(
        _device,
        DirectXPixelFormat::B8G8R8A8UIntNormalized,
        2,
        item.Size());
    auto session = framePool.CreateCaptureSession(item);
    
    auto inputProperties = VideoEncodingProperties::CreateUncompressed(
        MediaEncodingSubtypes::Bgra8(), 
        _outputProfile.Video().Width(), 
        _outputProfile.Video().Height());
    auto streamSource = MediaStreamSource(VideoStreamDescriptor(inputProperties));
    streamSource.BufferTime(TimeSpan::zero());
    _streamSourceStarting = streamSource.Starting(auto_revoke, { this, &CaptureRecorder::OnMediaStreamSourceStarting });
    _streamSourceSampleRequested = streamSource.SampleRequested(auto_revoke, { this, &CaptureRecorder::OnMediaStreamSourceSampleRequested });

    _commitQueue.clear();

    _captureItem = item;
    _framePool = framePool;
    _frameArrived = _framePool.FrameArrived(auto_revoke, { this, &CaptureRecorder::OnFrameArrived });
    _captureSession = session;

    _captureSession.StartCapture();

    auto transcode = co_await _transcoder.PrepareMediaStreamSourceTranscodeAsync(streamSource, stream, _outputProfile);
    co_await transcode.TranscodeAsync();
}

void CaptureRecorder::StopRecord()
{
    if (IsRecording()) {
        _commitQueue.push(CaptureSample());
        _queueReadyCV.notify_one();

        _frameArrived.revoke();
        _captureSession = nullptr;
        _framePool.Close();
        _framePool = nullptr;
        _captureItem = nullptr;
    }
}

void CaptureRecorder::OnFrameArrived(
    Direct3D11CaptureFramePool const& sender,
    winrt::Windows::Foundation::IInspectable const& args)
{
    auto frame = sender.TryGetNextFrame();
    auto frameSurface = GetDXGIInterfaceFromObject<ID3D11Texture2D>(frame.Surface());
    auto contentSize = frame.ContentSize();

    com_ptr<ID3D11Texture2D> commitSurface;
    com_ptr<ID3D11RenderTargetView> commitSurfaceRTV;

    SizeInt32 videoSize;
    videoSize.Width = _outputProfile.Video().Width(); 
    videoSize.Height = _outputProfile.Video().Height();
    {
        D3DMutexLock lk(_d3dDevice);

        D3D11_TEXTURE2D_DESC desc = { 0 };
        desc.ArraySize = 1;
        desc.MipLevels = 1;
        desc.Width = videoSize.Width;
        desc.Height = videoSize.Height;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        check_hresult(_d3dDevice->CreateTexture2D(&desc, nullptr, commitSurface.put()));
        check_hresult(_d3dDevice->CreateRenderTargetView(commitSurface.get(), nullptr, commitSurfaceRTV.put()));

        com_ptr<ID3D11ShaderResourceView> frameSurfaceSRV;
        check_hresult(_d3dDevice->CreateShaderResourceView(frameSurface.get(), nullptr, frameSurfaceSRV.put()));

        com_ptr<ID3D11DeviceContext> context;
        _d3dDevice->GetImmediateContext(context.put());

        ID3D11RenderTargetView* pRTVs[1];
        pRTVs[0] = commitSurfaceRTV.get();
        context->OMSetRenderTargets(1, pRTVs, nullptr);

        D3D11_VIEWPORT vp = { 0 };
        vp.Width = static_cast<float>(videoSize.Width);
        vp.Height = static_cast<float>(videoSize.Height);
        context->RSSetViewports(1, &vp);

        auto clearColor = D2D1::ColorF(D2D1::ColorF::CornflowerBlue);
        context->ClearRenderTargetView(commitSurfaceRTV.get(), &clearColor.r);

        _spriteBatch->Begin();

        CRect sourceRect, destinationRect;

        sourceRect.left = 0;
        sourceRect.top = 0;
        sourceRect.right = contentSize.Width;
        sourceRect.bottom = contentSize.Height;

        destinationRect = FitInBox(
            { static_cast<float>(contentSize.Width), static_cast<float>(contentSize.Height) },
            { static_cast<float>(videoSize.Width), static_cast<float>(videoSize.Height) });

        _spriteBatch->Draw(frameSurfaceSRV.get(), destinationRect, &sourceRect);

        _spriteBatch->End();

        auto surfaceDesc = frame.Surface().Description();
        auto itemSize = _captureItem.Size();
        if (itemSize.Width != surfaceDesc.Width || itemSize.Height != surfaceDesc.Height) {
            SizeInt32 size;
            size.Width = std::max(itemSize.Width, 1);
            size.Height = std::max(itemSize.Height, 1);
            _framePool.Recreate(_device, DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, size);
        }
    }

    CaptureSample commit;
    commit.surface = commitSurface;
    commit.timestamp = frame.SystemRelativeTime();

    _commitQueue.push(commit);
    _queueReadyCV.notify_one();
}

void CaptureRecorder::OnMediaStreamSourceStarting(
    MediaStreamSource const& sender, 
    MediaStreamSourceStartingEventArgs const& args)
{
    CaptureSample commit;
    {
        std::unique_lock<std::mutex> lk(_syncObject);
        _queueReadyCV.wait(lk, [this, &commit]() {
            return _commitQueue.try_pop(commit);
        });
    }

    args.Request().SetActualStartPosition(commit.timestamp);
}

void CaptureRecorder::OnMediaStreamSourceSampleRequested(
    MediaStreamSource const& sender, 
    MediaStreamSourceSampleRequestedEventArgs const& args)
{
    CaptureSample commit;
    {
        std::unique_lock<std::mutex> lk(_syncObject);
        _queueReadyCV.wait(lk, [this, &commit]() {
            return _commitQueue.try_pop(commit);
        });
    }
    if (!!commit.surface) {
        auto sample = MediaStreamSample::CreateFromDirect3D11Surface(
            CreateDirect3DSurfaceFromD3D11Texture2D(commit.surface), 
            commit.timestamp);
        args.Request().Sample(sample);
    }
    else {
        args.Request().Sample({ nullptr });
        _streamSourceSampleRequested.revoke();
        _streamSourceStarting.revoke();
    }
}
