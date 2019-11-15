#pragma once

struct CaptureSample {
    winrt::com_ptr<ID3D11Texture2D> surface;
    winrt::Windows::Foundation::TimeSpan timestamp;
};

class CaptureRecorder {
public:
    CaptureRecorder();
    
    winrt::Windows::Foundation::IAsyncAction StartRecordAsync(
        winrt::Windows::Graphics::Capture::GraphicsCaptureItem const& item, 
        winrt::Windows::Storage::Streams::IRandomAccessStream const& stream);

    void StopRecord();

    bool IsRecording() { return !!_framePool; }

private:
    void OnFrameArrived(
        winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool const& sender,
        winrt::Windows::Foundation::IInspectable const& args);

    void OnMediaStreamSourceStarting(
        winrt::Windows::Media::Core::MediaStreamSource const& sender,
        winrt::Windows::Media::Core::MediaStreamSourceStartingEventArgs const& args);

    void OnMediaStreamSourceSampleRequested(
        winrt::Windows::Media::Core::MediaStreamSource const& sender,
        winrt::Windows::Media::Core::MediaStreamSourceSampleRequestedEventArgs const& args);

private:
    winrt::com_ptr<ID3D11Device> _d3dDevice;
    std::unique_ptr<::DirectX::SpriteBatch> _spriteBatch;
    concurrency::concurrent_queue<CaptureSample> _commitQueue;
    std::mutex _syncObject;
    std::condition_variable _queueReadyCV;

    winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice _device{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool _framePool{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureItem _captureItem{ nullptr };
    winrt::Windows::Graphics::Capture::GraphicsCaptureSession _captureSession{ nullptr };
    winrt::Windows::Graphics::Capture::Direct3D11CaptureFramePool::FrameArrived_revoker _frameArrived;

    winrt::Windows::Media::Transcoding::MediaTranscoder _transcoder{ nullptr };
    winrt::Windows::Media::MediaProperties::MediaEncodingProfile _outputProfile{ nullptr };
    winrt::Windows::Media::Core::MediaStreamSource::Starting_revoker _streamSourceStarting;
    winrt::Windows::Media::Core::MediaStreamSource::SampleRequested_revoker _streamSourceSampleRequested;
};

