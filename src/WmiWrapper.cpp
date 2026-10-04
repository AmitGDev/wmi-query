/*
    WmiWrapper.cpp
    Copyright (c) 2024-2026, Amit Gefen

    Permission is hereby granted, free of charge, to any person obtaining a copy
    of this software and associated documentation files (the "Software"), to
    deal in the Software without restriction, including without limitation the
    rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
    sell copies of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be included in
    all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
    FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
    IN THE SOFTWARE.
*/

// Based on the 7 steps Microsoft Example: Getting WMI Data from the Local
// Computer Ref.:
// https://docs.microsoft.com/en-us/windows/win32/wmisdk/example--getting-wmi-data-from-the-local-computer

#include "WmiWrapper.hpp"

#include <combaseapi.h>
#include <comdef.h>
#include <minwindef.h>
#include <oaidl.h>
#include <objbase.h>
#include <objidlbase.h>
#include <oleauto.h>
#include <rpcdce.h>
#include <wbemcli.h>
#include <winerror.h>
#include <winnt.h>
#include <wtypes.h>
#include <wtypesbase.h>

#include <cstdint>
#include <string>
#include <utility>

#include "WmiNamespaceName.hpp"
#include "scope.hpp"

#pragma comment(lib, "wbemuuid.lib")

namespace amitgdev {

template <typename T>
  requires requires(T* ptr) { ptr->Release(); }
static void SafeRelease(T*& ptr) noexcept {
  if (ptr != nullptr) {
    ptr->Release();
    ptr = nullptr;
  }
}

// Stringify the VARIANT types WMI uses for the properties callers read. The
// union access is unavoidable with the raw VARIANT ABI, so it is confined to
// this function instead of being suppressed (or repeated) at every call site.
static std::wstring VariantToWString(const VARIANT& value,
                                     const CIMTYPE cim_type) {
  // NOLINTBEGIN(cppcoreguidelines-pro-type-union-access)
  switch (value.vt) {
    case VT_BSTR:
      // A null BSTR is a valid empty string, but constructing a wstring from
      // a null pointer is undefined. The length-based ctor also preserves
      // embedded nulls.
      return value.bstrVal != nullptr
                 ? std::wstring(value.bstrVal, SysStringLen(value.bstrVal))
                 : std::wstring();
    case VT_I4:
      // WMI delivers uint32 as a signed VT_I4, so a value above INT32_MAX
      // would print as negative. The CIM type tells the two apart.
      return cim_type == CIM_UINT32
                 ? std::to_wstring(static_cast<std::uint32_t>(value.lVal))
                 : std::to_wstring(value.lVal);
    case VT_I2:   // sint16
      return std::to_wstring(value.iVal);
    case VT_UI1:  // uint8
      return std::to_wstring(value.bVal);
    case VT_BOOL:
      // VARIANT_TRUE is -1, so print a normalized 0/1 rather than the raw
      // value.
      return value.boolVal != VARIANT_FALSE ? L"1" : L"0";
    default:
      // Null, empty, arrays and unsupported types intentionally map to "".
      return {};
  }
  // NOLINTEND(cppcoreguidelines-pro-type-union-access)
}

HRESULT
WmiWrapper::Connect(const WmiNamespaceName& wmi_namespace_name) noexcept {
  // A repeated call replaces the old connection instead of leaking it.
  Cleanup();

  // Any failure below leaves the object disconnected rather than holding a
  // locator or service that cannot be used.
  amitgdev::scope_exit cleanup_guard{[this] noexcept { Cleanup(); }};

  // Step 3
  HRESULT hres =
      CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
                       IID_IWbemLocator, reinterpret_cast<LPVOID*>(&loc_));
  if (FAILED(hres)) {
    return hres;
  }

  // Step 4
  // Null credentials/locale/authority connect as the current user.
  hres = loc_->ConnectServer(_bstr_t(wmi_namespace_name.c_str()), nullptr,
                             nullptr, nullptr, 0, nullptr, nullptr, &svc_);
  if (FAILED(hres)) {
    return hres;
  }

  // Step 5
  // The proxy gets its own security settings, independent of the process-wide
  // defaults. Without this, calls through svc_ can fail with access denied.
  hres = CoSetProxyBlanket(svc_, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
                           RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE,
                           nullptr, EOAC_NONE);
  if (FAILED(hres)) {
    return hres;
  }

  cleanup_guard.release();  // Commit ownership of the connected resources.

  return hres;
}

HRESULT WmiWrapper::ExecQuery(const std::wstring& query) noexcept {
  // A previous result set is dropped so repeated queries do not leak it.
  SafeRelease(enumerator_);

  if (svc_ == nullptr) {
    return E_NOT_VALID_STATE;
  }

  // Step 6
  // WMI flags are signed by design in the Windows API.
  // NOLINTBEGIN(bugprone-signed-bitwise)
  return svc_->ExecQuery(bstr_t("WQL"), bstr_t(query.c_str()),
                         WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY,
                         nullptr, &enumerator_);
  // NOLINTEND(bugprone-signed-bitwise)
}

// Step 7
Columns WmiWrapper::GetColumns(const Rows& rows) const {
  Columns data_set_set;

  if (enumerator_ == nullptr) {
    return data_set_set;
  }

  // One object per Next() call keeps ownership simple: each object is released
  // before the next one is fetched.
  while (true) {
    IWbemClassObject* class_object = nullptr;
    ULONG returned = 0;

    const HRESULT hres =
        enumerator_->Next(WBEM_INFINITE, 1, &class_object, &returned);
    if (FAILED(hres) || returned == 0) {
      break;
    }

    const amitgdev::scope_exit class_object_guard{
        [class_object] noexcept { class_object->Release(); }};

    Column data_set;
    data_set.reserve(rows.size());

    for (const auto& row : rows) {
      // RAII wrapper so the VARIANT is cleared even if the conversion throws.
      _variant_t property;
      CIMTYPE cim_type = 0;

      // A missing property leaves the VARIANT empty, which maps to "".
      class_object->Get(row.c_str(), 0, property.GetAddress(), &cim_type,
                        nullptr);

      data_set.emplace_back(row, VariantToWString(property, cim_type));
    }

    data_set_set.emplace_back(std::move(data_set));
  }

  return data_set_set;
}

// Cleanup
// Releases in reverse order of acquisition, and the enumerator and services
// before the locator that produced them.
void WmiWrapper::Cleanup() noexcept {
  SafeRelease(enumerator_);
  SafeRelease(svc_);
  SafeRelease(loc_);
}

// Static
HRESULT WmiWrapper::Initialize() noexcept {
  // Step 1
  // Thread-wide
  HRESULT hres = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

  if (FAILED(hres)) {
    return hres;
  }

  // Step 2
  // Process-wide, and it can be set only once. A repeated call, a call after
  // Uninitialize(), or a host that already configured COM security yields
  // RPC_E_TOO_LATE. That is not a failure here: Connect() sets the proxy
  // security that WMI needs on its own connection.
  hres = CoInitializeSecurity(
      nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
      RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

  if (FAILED(hres) && hres != RPC_E_TOO_LATE) {
    CoUninitialize();  // Release the reference taken in step 1.
    return hres;
  }

  ++init_depth_;  // Balanced by one Uninitialize() on this thread.
  return S_OK;
}

// Static
void WmiWrapper::Uninitialize() noexcept {
  if (init_depth_ > 0) {
    CoUninitialize();
    --init_depth_;
  }
}

}  // namespace amitgdev
