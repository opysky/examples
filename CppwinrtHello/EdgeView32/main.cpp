#include "pch.h"

using namespace winrt;
using namespace Windows::Foundation;

int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    init_apartment(apartment_type::single_threaded);
    
	Uri uri(L"http://aka.ms/cppwinrt");
	
	std::array<wchar_t, 256> text;
	std::swprintf(&text[0], text.size(), L"Hello, %ls!\n", uri.AbsoluteUri().c_str());
	OutputDebugStringW(&text[0]);

	return 0;
}
