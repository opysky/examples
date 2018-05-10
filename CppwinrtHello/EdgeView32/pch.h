//
// pch.h
// Precompiled header for commonly included header files
//

#pragma once

#include "atlbase.h"
#include "atlapp.h"
extern CAppModule _Module;
#include "atlwin.h"
#include "atlcrack.h"
#include "atlgdi.h"
#include "atlmisc.h"

#define NOMINMAX

#include "winrt/Windows.System.h"
#include "winrt/Windows.Foundation.h"
#include "winrt/Windows.Web.UI.Interop.h"
#include <DispatcherQueue.h>