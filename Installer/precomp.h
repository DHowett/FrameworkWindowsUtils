#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#include <SetupAPI.h>
#include <newdev.h>
#include <cfgmgr32.h>

#include <string>
#include <string_view>
#include <memory>
#include <filesystem>
#include <algorithm>
#include <set>
#include <unordered_set>

#include <wil/result_macros.h>
#include <wil/win32_helpers.h>
#include <wil/resource.h>
#include <wil/stl.h>
