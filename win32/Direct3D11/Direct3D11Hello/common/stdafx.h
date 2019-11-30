#pragma once

#define NOMINMAX

#include <wil/resource.h>
#include <wil/com.h>

#include <wtypes.h>
#include <winbase.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <Shlwapi.h>

#include <dxgi1_6.h>
#include <d3d11_4.h>
#include <d2d1_3helper.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include <VertexTypes.h>
#include <SimpleMath.h>
#include <CommonStates.h>

#include <array>
#include <algorithm>
#include <cstdint>
#include <cassert>

#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")