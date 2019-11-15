#include "stdafx.h"
#include "CaptureRecorder.h"

using namespace winrt;
using namespace winrt::Windows::System;
using namespace winrt::Windows::Storage;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Storage::Pickers;
using namespace winrt::Windows::Graphics::DirectX::Direct3D11;
using namespace winrt::Windows::Graphics::Capture;

class MainWindow : public CWindowImpl<MainWindow> {
public:
    DECLARE_WND_CLASS(nullptr)

    void Launch() {
        
        Create(nullptr, &CRect(0, 0, 300, 100), L"CaptureRecord", WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX);

        CenterWindow();
        ShowWindow(SW_SHOWNORMAL);
        Invalidate();

        _recorder = std::make_unique<CaptureRecorder>();
    }

private:
    BEGIN_MSG_MAP(MainWindow)
        COMMAND_CODE_HANDLER_EX(BN_CLICKED, OnButtonClicked)
        MSG_WM_CREATE(OnCreate)
        MSG_WM_DESTROY(OnDestory)
    END_MSG_MAP()

    fire_and_forget OnButtonClicked(UINT uNotifyCode, int, CWindow const&) {
        if (_recorder->IsRecording()) {
            _recorder->StopRecord();
            _pickerButton.SetWindowText(L"Rec...");
            co_return;
        }

        GraphicsCapturePicker capturePicker;
        auto initializer = capturePicker.as<::IInitializeWithWindow>();
        initializer->Initialize(*this);
        auto item = co_await capturePicker.PickSingleItemAsync();

        std::array<wchar_t, MAX_PATH> appDirPath;
        std::array<wchar_t, MAX_PATH> tempFileName;
        GetModuleFileName(nullptr, appDirPath.data(), appDirPath.size());
        PathRemoveFileSpec(appDirPath.data());
        GetTempFileName(appDirPath.data(), L"rec", 0, tempFileName.data());
        PathStripPath(tempFileName.data());

        auto tempFolder = co_await StorageFolder::GetFolderFromPathAsync(appDirPath.data());
        auto tempFile = co_await tempFolder.CreateFileAsync(tempFileName.data(), CreationCollisionOption::ReplaceExisting);
        {
            auto stream = co_await tempFile.OpenAsync(FileAccessMode::ReadWrite);
            _pickerButton.SetWindowText(L"Stop...");
            co_await _recorder->StartRecordAsync(item, stream);
        }

        auto savePicker = FileSavePicker();
        initializer = savePicker.as<IInitializeWithWindow>();
        initializer->Initialize(*this);
        savePicker.SuggestedStartLocation(PickerLocationId::VideosLibrary);
        savePicker.SuggestedFileName(L"recoded");
        savePicker.DefaultFileExtension(L".mp4");
        savePicker.FileTypeChoices().Insert(L"MP4 Video", single_threaded_vector<hstring>({ L".mp4" }));
        if (auto outputFile = co_await savePicker.PickSaveFileAsync()) {
            co_await tempFile.MoveAndReplaceAsync(outputFile);
        }
        else {
            co_await tempFile.DeleteAsync();
        }
    }

    LRESULT OnCreate(LPCREATESTRUCT) {
        LOGFONT lf;
        SystemParametersInfo(SPI_GETICONTITLELOGFONT, sizeof(lf), &lf, 0);
        _iconTitleFont.Attach(CreateFontIndirect(&lf));

        CRect clientRect;
        GetClientRect(&clientRect);

        CRect buttonRect;
        buttonRect.left = clientRect.left + 10;
        buttonRect.right = clientRect.right - 10;
        buttonRect.top = clientRect.top + 10;
        buttonRect.bottom = clientRect.bottom - 10;
        _pickerButton.Create(
            *this,
            buttonRect,
            L"Rec...",
            BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE);
        _pickerButton.SetFont(_iconTitleFont);

        return 0;
    }

    void OnDestory() {
        PostQuitMessage(0);
    }

private:
    CFont _iconTitleFont;
    CButton _pickerButton;
    std::unique_ptr<CaptureRecorder> _recorder;
};

CAppModule _Module;

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, WCHAR*, int)
{
    init_apartment(apartment_type::single_threaded);

    //SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    _Module.Init(nullptr, hInstance);

    CMessageLoop kicker;
    _Module.AddMessageLoop(&kicker);

    MainWindow app;
    app.Launch();

    auto wParam = kicker.Run();

    _Module.RemoveMessageLoop();
    _Module.Term();

    return wParam;
}