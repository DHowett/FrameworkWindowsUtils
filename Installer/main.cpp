#include "precomp.h"

#include <initguid.h>  // emit the GUIDs in CrosEC into our own binary...
#include "../CrosEC/Public.h"

#pragma region WIL Extensions
namespace wil::details {
template <>
struct string_maker<std::vector<wchar_t>> {
	HRESULT make(PCWSTR source, size_t length) WI_NOEXCEPT try {
		m_value = source ? std::vector<wchar_t>(source, source + length) : std::vector<wchar_t>(length, L'\0');
		return S_OK;
	} catch(...) {
		return E_OUTOFMEMORY;
	}

	wchar_t* buffer() { return &m_value[0]; }

	HRESULT trim_at_existing_null(size_t length) {
		m_value.resize(length);
		return S_OK;
	}

	std::vector<wchar_t> release() { return std::vector<wchar_t>(std::move(m_value)); }

	static PCWSTR get(const std::vector<wchar_t>& value) { return value.data(); }

private:
	std::vector<wchar_t> m_value;
};
}  // namespace wil::details

using unique_hdevinfo = wil::unique_any<HDEVINFO,
                                        decltype(SetupDiDestroyDeviceInfoList),
                                        &SetupDiDestroyDeviceInfoList,
                                        wil::details::pointer_access_all,
                                        HDEVINFO,
                                        HDEVINFO,
                                        INVALID_HANDLE_VALUE>;

#pragma endregion

static constexpr HRESULT S_REBOOT{(HRESULT)3};

static std::set<std::wstring_view, std::less<void>> multiSzToSet(const std::vector<wchar_t>& multiSz) {
	std::set<std::wstring_view, std::less<void>> r;
	auto begin{multiSz.begin()};
	while(begin != multiSz.end()) {
		const auto l{wcslen(&*begin)};
		if(l == 0) {  // an empty string in a REG_MULTI_SZ signals the end
			break;
		}
		r.insert(std::wstring_view{&*begin, l});
		begin += l + 1;
	}
	return r;
}

static constexpr std::wstring_view gCrosECDeviceNodeHwidList{LR"(ROOT\CrosEC)"
                                                             "\0\0",  // double null-terminate as this is a HWID List
                                                             14};
static constexpr std::wstring_view gCrosECDeviceNode{gCrosECDeviceNodeHwidList.substr(0, 11)};

static HRESULT Install() {
	BOOL reboot{FALSE};

	std::filesystem::path infPath{L"CrosEC.inf"};
	infPath = std::filesystem::absolute(infPath);

	GUID ClassGUID;
	WCHAR ClassName[256];
	RETURN_IF_WIN32_BOOL_FALSE(
		SetupDiGetINFClassW(infPath.native().c_str(), &ClassGUID, ClassName, (DWORD)std::size(ClassName), 0));

	unique_hdevinfo DeviceInfoSet;
	DeviceInfoSet.reset(SetupDiCreateDeviceInfoList(&ClassGUID, 0));
	RETURN_HR_IF(E_FAIL, !DeviceInfoSet);

	SP_DEVINFO_DATA DeviceInfoData{};
	DeviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);
	RETURN_IF_WIN32_BOOL_FALSE(SetupDiCreateDeviceInfoW(DeviceInfoSet.get(), ClassName, &ClassGUID, nullptr, 0,
	                                                    DICD_GENERATE_ID, &DeviceInfoData));

	RETURN_IF_WIN32_BOOL_FALSE(SetupDiSetDeviceRegistryPropertyW(
		DeviceInfoSet.get(), &DeviceInfoData, SPDRP_HARDWAREID, (LPBYTE)gCrosECDeviceNodeHwidList.data(),
		(DWORD)(gCrosECDeviceNodeHwidList.size() * sizeof(WCHAR))));

	RETURN_IF_WIN32_BOOL_FALSE(SetupDiCallClassInstaller(DIF_REGISTERDEVICE, DeviceInfoSet.get(), &DeviceInfoData));

	RETURN_IF_WIN32_BOOL_FALSE(UpdateDriverForPlugAndPlayDevicesW(nullptr, gCrosECDeviceNodeHwidList.data(),
	                                                              infPath.native().c_str(), 0, &reboot));

	return reboot ? S_REBOOT : S_OK;
}

static HRESULT RemoveCrosECDeviceNode() {
	unique_hdevinfo devs;
	devs.reset(SetupDiGetClassDevsExW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES, nullptr, nullptr, nullptr));
	RETURN_HR_IF(E_FAIL, !devs);

	SP_DEVINFO_LIST_DETAIL_DATA devInfoListDetail{};
	devInfoListDetail.cbSize = sizeof(devInfoListDetail);
	RETURN_HR_IF(E_FAIL, !SetupDiGetDeviceInfoListDetailW(devs.get(), &devInfoListDetail));

	SP_DEVINFO_DATA devInfo{};
	devInfo.cbSize = sizeof(devInfo);
	int found{}, failures{};
	for(DWORD devIndex = 0; SetupDiEnumDeviceInfo(devs.get(), devIndex, &devInfo); devIndex++) {
		wchar_t devID[MAX_DEVICE_ID_LEN];
		RETURN_HR_IF(E_FAIL,
		             CR_SUCCESS != CM_Get_Device_IDW(devInfo.DevInst, devID, (DWORD)std::size(devID), 0));

		std::vector<wchar_t> hwIdsMultiSz;
		DWORD dataType{};
		wil::AdaptFixedSizeToAllocatedResult(hwIdsMultiSz, [&](PWSTR buffer, size_t size, size_t* needed) {
			DWORD neededBytes;
			RETURN_IF_WIN32_BOOL_FALSE(SetupDiGetDeviceRegistryPropertyW(
				devs.get(), &devInfo, SPDRP_HARDWAREID, &dataType, (PBYTE)buffer,
				(DWORD)(size * sizeof(wchar_t)), &neededBytes));
			*needed = neededBytes / sizeof(wchar_t);
			return S_OK;
		});
		auto hwIds{multiSzToSet(hwIdsMultiSz)};
		if(hwIds.contains(gCrosECDeviceNode)) {
			++found;
			fwprintf(stderr, L"[+] Removing %s... ", devID);
			if(!SetupDiCallClassInstaller(DIF_REMOVE, devs.get(), &devInfo)) {
				auto lastError{GetLastError()};
				failures++;
				fwprintf(stderr, L"FAILED (%8.08lx)\r\n", HRESULT_FROM_SETUPAPI(lastError));
			} else {
				fwprintf(stderr, L"OK\r\n");
			}
		}
	}

	if(!found) {
		fwprintf(stderr, L"[+] No device nodes found.\r\n");
	} else {
		fwprintf(stderr, L"[+] Removed %d device%s.\r\n", found - failures,
		         (found - failures) == 1 ? L"" : L"s");
	}
	return found > 0 ? S_OK : S_FALSE;
}

static HRESULT Remove() {
	std::filesystem::path infPath{L"CrosEC.inf"};
	infPath = std::filesystem::absolute(infPath);

	RETURN_IF_FAILED(RemoveCrosECDeviceNode());

	fwprintf(stderr, L"[+] Removing CrosEC.inf... ");
	BOOL reboot;
	if(!DiUninstallDriverW(nullptr, infPath.native().c_str(), 0, &reboot)) {
		auto lastError{GetLastError()};
		fwprintf(stderr, L"FAILED (%8.08lx)\r\n", HRESULT_FROM_SETUPAPI(lastError));
		return HRESULT_FROM_SETUPAPI(lastError);
	} else {
		fwprintf(stderr, L"OK\r\n");
	}

	return S_OK;
}

static int PrintUsage(wchar_t* progName) {
	fwprintf(stderr, L"Usage: %s <install|uninstall>\r\n", progName);
	return 1;
}

int __cdecl wmain(_In_ int argc, _In_reads_(argc) PWSTR* argv) {
	if(argc < 2) {
		return PrintUsage(argv[0]);
	}

	if(wcscmp(argv[1], L"install") == 0) {
		return Install();
	} else if(wcscmp(argv[1], L"uninstall") == 0) {
		return Remove();
	}

	return PrintUsage(argv[0]);
}
