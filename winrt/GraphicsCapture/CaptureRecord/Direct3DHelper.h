#pragma once

template <typename T>
auto GetDXGIInterfaceFromObject(winrt::Windows::Foundation::IInspectable const& object)
{
	auto access = object.as<::Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
	winrt::com_ptr<T> result;
	winrt::check_hresult(access->GetInterface(winrt::guid_of<T>(), result.put_void()));
	return result;
}

auto CreateDirect3DDevice()
{
	UINT createDeviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	winrt::com_ptr<ID3D11Device> d3dDevice;
	winrt::check_hresult(D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createDeviceFlags,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		d3dDevice.put(),
		nullptr,
		nullptr));

	auto multithread = d3dDevice.as<ID3D11Multithread>();
	multithread->SetMultithreadProtected(TRUE);

	auto dxgiDevice = d3dDevice.as<IDXGIDevice>();

	winrt::com_ptr<::IInspectable> device;
	winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgiDevice.get(), device.put()));
	return device.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DDevice>();
}

auto CreateDirect3DSurfaceFromD3D11Texture2D(winrt::com_ptr<ID3D11Texture2D> const& texture)
{
    auto dxgiSurface = texture.as<IDXGISurface>();
    winrt::com_ptr<::IInspectable> surface;
    winrt::check_hresult(CreateDirect3D11SurfaceFromDXGISurface(dxgiSurface.get(), surface.put()));
    return surface.as<winrt::Windows::Graphics::DirectX::Direct3D11::IDirect3DSurface>();
}

class D3DMutexLock {
public:
    D3DMutexLock(winrt::com_ptr<ID3D11Device> const& device) {
        _mutex = device.as<ID3D11Multithread>();
        _mutex->Enter();
    }
    
    ~D3DMutexLock() {
        _mutex->Leave();
    }

private:
    winrt::com_ptr<ID3D11Multithread> _mutex;
};