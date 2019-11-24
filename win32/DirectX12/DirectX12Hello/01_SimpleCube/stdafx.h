#pragma once

#define NOMINMAX

#include <atlbase.h>
#include <atlapp.h>
extern CAppModule _Module;
#include <atlwin.h>
#include <atlctrls.h>
#include <atlcrack.h>
#include <atlmisc.h>

#include <wil/resource.h>
#include <wil/com.h>

#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXColors.h>

#include <SimpleMath.h>

#include "d3dx12.h"
#include "FPSCounter.h"

#include <array>

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")