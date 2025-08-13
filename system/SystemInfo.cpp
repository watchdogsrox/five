#if __WIN32PC && !__TOOL && !__RESOURCECOMPILER

#include "system/SystemInfo.h"
#include "grcore/d3dwrapper.h"
#include "grcore/adapter.h"
// #include "grcore/adapter_d3d9.h"
#include "grcore/device.h"
#include "string/string.h"

SYSTEM_INFO*	CSystemInfo::m_sysInfo = NULL;
MEMORYSTATUSEX*	CSystemInfo::m_memStatus = NULL;

int CSystemInfo::numPhysicalCores = -1;

char CSystemInfo::rawOsVersionString[MAX_PATH];
char CSystemInfo::windowsString[MAX_PATH];
char CSystemInfo::processorString[MAX_PATH];
char CSystemInfo::processorIdentifier[MAX_PATH];
char CSystemInfo::processorSpeed[MAX_PATH];
char CSystemInfo::languageString[MAX_PATH];

char CSystemInfo::videoCardDescription[512];
char CSystemInfo::videoCardRAM[512];
char CSystemInfo::videoCardResolution[512];
char CSystemInfo::driverVersion[512];
char CSystemInfo::dxVersion[512];

char CSystemInfo::MBManufacturer[512];
char CSystemInfo::MBProduct[512];
char CSystemInfo::BIOSVendor[512];
char CSystemInfo::BIOSVersion[512];

char CSystemInfo::AdapterIndex9[4] = {0};
bool CSystemInfo::WindowsVersionRetrieved = false;

struct MSFT_PhysicalDisk
{
	char DeviceId[512];
	char FriendlyName[512];
	u64  HealthStatus;
	u64  BusType;
	u64  SpindleSpeed;
	u64  MediaType;
};

MSFT_PhysicalDisk physicalDisk;

namespace rage
{
	extern HRESULT GetVideoMemory( HMONITOR hMonitor, s64 *pdwAvailableVidMem, s64 *pdwFreeVidMem );

	NOSTRIP_PC_XPARAM(adapter);
}

typedef HRESULT (WINAPI *LPCREATEDXGIFACTORY)(REFIID, void**);
#if __D3D11
typedef HRESULT (WINAPI *LPD3D11CREATEDEVICE)(IDXGIAdapter* pAdapter,
											  D3D_DRIVER_TYPE DriverType,
											  HMODULE Software,
											  UINT Flags,
											  CONST D3D_FEATURE_LEVEL* pFeatureLevels,
											  UINT FeatureLevels,
											  UINT SDKVersion,
											  ID3D11Device** ppDevice,
											  D3D_FEATURE_LEVEL* pFeatureLevel,
											  ID3D11DeviceContext** ppImmediateContext);
typedef HRESULT (WINAPI *LPD3D10CREATEDEVICE1)(IDXGIAdapter *pAdapter,
											   D3D10_DRIVER_TYPE DriverType,
											   HMODULE Software,
											   UINT Flags,
											   D3D10_FEATURE_LEVEL1 HardwareLevel,
											   UINT SDKVersion,
											   ID3D10Device1** ppDevice);
#endif

void CSystemInfo::RefreshSystemInfo()
{
	if (!m_sysInfo)
		m_sysInfo = rage_new SYSTEM_INFO;
	GetNativeSystemInfo(m_sysInfo);
	
	if (!m_memStatus)
		m_memStatus = rage_new MEMORYSTATUSEX;
	m_memStatus->dwLength = sizeof(MEMORYSTATUSEX);
	GlobalMemoryStatusEx(m_memStatus);

	GetWindowsVersion(&rawOsVersionString[0], &windowsString[0], MAX_PATH);
	GetCPUInformation(&processorString[0], &processorIdentifier[0], &processorSpeed[0], MAX_PATH);
	GetVideoCardInformation(&videoCardDescription[0], &videoCardRAM[0], &videoCardResolution[0], &driverVersion[0], &dxVersion[0], 512);
	GetHDDInformation();
	GetBIOSInformation(&MBManufacturer[0], &MBProduct[0], &BIOSVendor[0], &BIOSVersion[0], 512);
	GetUserLanguage(&languageString[0], MAX_PATH);

	int iProcessorPackages = 0;
	int iProcessorCores = 0;
	int iLogicalCores = 0;
	if(sysTaskManager::GetProcessorInfo(iProcessorPackages, iProcessorCores, iLogicalCores))
	{
		numPhysicalCores = iProcessorCores;
	}
}

void CSystemInfo::Initialize()
{
	RefreshSystemInfo();
#if __DEV
	DisplaySystemInfo();
#endif
}

void CSystemInfo::DisplaySystemInfo()
{
	// Show the current information.
	Displayf("Current PC System Information");
	Displayf("------------------------------");
	Displayf("Windows Version: %s", windowsString);
	Displayf("Language: %s", languageString);
	Displayf("Processor String: %s", processorString);
	Displayf("Processor Identifier: %s", processorIdentifier);
	Displayf("Processor Speed: %s", processorSpeed);
	Displayf("Physical Cores: %d", GetNumberOfPhysicalCores());
	Displayf("Logical Processors: %d", GetNumberOfCPUS());
	Displayf("Video Card Description: %s", videoCardDescription);
	Displayf("Video Card Resolution: %s", videoCardResolution);
	Displayf("Video Card Ram: %s", videoCardRAM);
	Displayf("Video Card Driver Version: %s", driverVersion);
	Displayf("DirectX Version: %s", dxVersion);
	Displayf("System RAM: %I64u MB", GetTotalPhysicalRAM() / (1024 * 1024));
	Displayf("HDD Size Total: %I64u MB", GetTotalPhysicalHDD() / (1024 * 1024));
	Displayf("HDD Size Available: %I64u MB", GetAvailablePhysicalHDD() / (1024 * 1024));
	Displayf("Motherboard Manufacturer: %s", MBManufacturer);
	Displayf("Motherboard Product: %s", MBProduct);
	Displayf("BIOS Vendor: %s", BIOSVendor);
	Displayf("BIOS Version: %s", BIOSVersion);
}

void CSystemInfo::WriteSystemInfo(FILE* fp)
{
	Assertf(fp, "No valid file handle to write system info to.");

	if (fp)
	{
		char line[1024];

		sprintf(line, "Windows Version: %s\r\n", windowsString);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Language: %s\r\n", languageString);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Processor String: %s\r\n", processorString);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Processor Identifier: %s\r\n", processorIdentifier);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Processor Speed: %s\r\n", processorSpeed);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Physical CPUs: %d\r\n", GetNumberOfCPUS());
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Video Card Description: %s\r\n", videoCardDescription);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Video Card Resolution: %s\r\n", videoCardResolution);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Video Card Ram: %s\r\n", videoCardRAM);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Video Card Driver Version: %s\r\n", driverVersion);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "DirectX Version: %s\r\n", dxVersion);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "System RAM: %I64u MB\r\n", GetTotalPhysicalRAM() / (1024 * 1024));
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "HDD Size Total: %I64u MB\r\n", GetTotalPhysicalHDD() / (1024 * 1024));
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "HDD Size Available: %I64u MB\r\n", GetAvailablePhysicalHDD() / (1024 * 1024));
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Motherboard Manufacturer: %s\r\n", MBManufacturer);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "Motherboard Product: %s\r\n", MBProduct);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "BIOS Vendor: %s\r\n", BIOSVendor);
		fwrite(line, strlen(line), 1, fp);
		sprintf(line, "BIOS Version: %s\r\n\r\n\r\n", BIOSVersion);
		fwrite(line, strlen(line), 1, fp);
	}
}

bool CSystemInfo::GetCPUInformation(char* processorString, char* processorIdentifier, char* processorSpeed, int bufferCount)
{
	Assert(processorString);
	Assert(processorIdentifier);
	Assert(processorSpeed);

	HKEY hKey;
	DWORD dwRes = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, 
		TEXT("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0\\"),
		0,
		KEY_READ,
		&hKey);

	DWORD dwSize = bufferCount;
	if(dwRes == ERROR_SUCCESS)
	{
		dwRes = RegQueryValueEx(
			hKey,
			TEXT("Identifier"),
			NULL,
			NULL,
			(LPBYTE)processorIdentifier,
			&dwSize);

		if (dwRes != ERROR_SUCCESS)
			return false;

		dwSize = bufferCount;

		dwRes = RegQueryValueEx(
			hKey,
			TEXT("ProcessorNameString"),
			NULL,
			NULL,
			(LPBYTE)processorString,
			&dwSize);

		if (dwRes != ERROR_SUCCESS)
			return false;

		DWORD dwData;
		dwSize = sizeof(DWORD);

		dwRes = RegQueryValueEx(
			hKey,
			TEXT("~MHz"),
			NULL,
			NULL,
			(LPBYTE)&dwData,
			&dwSize);

		if (dwRes != ERROR_SUCCESS)
			return false;

		float fGHZ = (float)(dwData / 1000.0f);
		sprintf(processorSpeed, "%.2f Ghz", fGHZ);

		RegCloseKey(hKey);
	}

	return true;
}

bool CSystemInfo::GetBIOSInformation(char* MBManufacturer, char* MBProduct, char* BIOSVendor, char* BIOSVersion, int bufferCount)
{
	Assert(MBManufacturer);
	Assert(MBProduct);
	Assert(BIOSVendor);
	Assert(BIOSVersion);

	HKEY hKey;
	DWORD dwRes = RegOpenKeyEx(
		HKEY_LOCAL_MACHINE, 
		TEXT("HARDWARE\\DESCRIPTION\\System\\BIOS\\"),
		0,
		KEY_READ,
		&hKey);

	DWORD dwSize = bufferCount;

	if(dwRes == ERROR_SUCCESS)
	{
		dwRes = RegQueryValueEx(
			hKey,
			TEXT("BaseBoardManufacturer"),
			NULL,
			NULL,
			(LPBYTE)MBManufacturer,
			&dwSize);

		if (dwRes != ERROR_SUCCESS)
			return false;

		dwSize = bufferCount;

		dwRes = RegQueryValueEx(
			hKey,
			TEXT("BaseBoardProduct"),
			NULL,
			NULL,
			(LPBYTE)MBProduct,
			&dwSize);

		if (dwRes != ERROR_SUCCESS)
			return false;

		dwSize = bufferCount;

		dwRes = RegQueryValueEx(
			hKey,
			TEXT("BIOSVendor"),
			NULL,
			NULL,
			(LPBYTE)BIOSVendor,
			&dwSize);

		if (dwRes != ERROR_SUCCESS)
			return false;

		dwSize = bufferCount;

		dwRes = RegQueryValueEx(
			hKey,
			TEXT("BIOSVersion"),
			NULL,
			NULL,
			(LPBYTE)BIOSVersion,
			&dwSize);

		if (dwRes != ERROR_SUCCESS)
			return false;

		RegCloseKey(hKey);
	}

	return true;
}

bool CSystemInfo::GetVideoCardInformation(char* D3D11_ONLY(description), char* D3D11_ONLY(VRAM), char* ASSERT_ONLY(D3D11_ONLY(displayResolution)), char* D3D11_ONLY(driverVersion), char* dxVersion, int D3D11_ONLY(bufferCount))
{
#if __D3D11
	Assert(description);
	Assert(VRAM);
	Assert(displayResolution);
	Assert(driverVersion);
#endif
	Assert(dxVersion);

	HMODULE g_hD3D9 = ::LoadLibrary("D3D9.DLL");
	g_Direct3DCreate9 = (IDirect3D9 * (WINAPI *)(UINT SDKVersion))::GetProcAddress(g_hD3D9, "Direct3DCreate9");

	if (!g_Direct3DCreate9)
	{
		Errorf("Could not create DirectX 9 device to query video card information.");
		return false;
	}

	sprintf(dxVersion, "DirectX 9");

#if __D3D11
	int iDXRuntimeVersion = GetDXRuntimeVersion();

	if (iDXRuntimeVersion > 900)
	{
		HMODULE hD3D11 = ::LoadLibrary("D3D11.DLL");

		if (hD3D11)
		{
			IDXGIAdapter* pAdapter = NULL;

			UINT uAdapterIndex = 0;

			if (PARAM_adapter.Get())
				PARAM_adapter.Get(uAdapterIndex);

			HMODULE hDXGI = LoadLibrary("dxgi.dll");

			if (hDXGI)
			{
				LPCREATEDXGIFACTORY pfnCreateDXGIFactory = (LPCREATEDXGIFACTORY)::GetProcAddress(hDXGI, "CreateDXGIFactory");

				IDXGIFactory* pFactory = NULL;
				HRESULT hCreateDXGIFactory = pfnCreateDXGIFactory(__uuidof(IDXGIFactory), (LPVOID*)&pFactory);

				if (SUCCEEDED(hCreateDXGIFactory))
				{
					if (pFactory->EnumAdapters(uAdapterIndex, &pAdapter) != S_OK)
						pAdapter = NULL;

					pFactory->Release();
				}
			}

			D3D_FEATURE_LEVEL oFeatureLevel;

			LPD3D11CREATEDEVICE pfnD3D11CreateDevice = (LPD3D11CREATEDEVICE)::GetProcAddress(hD3D11,"D3D11CreateDevice");
			HRESULT hD3D11CreateDevice = pfnD3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0, NULL, 0, D3D11_SDK_VERSION, NULL, &oFeatureLevel, NULL);

			if (SUCCEEDED(hD3D11CreateDevice))
			{
				if (oFeatureLevel == D3D_FEATURE_LEVEL_11_0)
				{
					sprintf(dxVersion, "DirectX 11");
				}
				else if (oFeatureLevel == D3D_FEATURE_LEVEL_10_1)
				{
					sprintf(dxVersion, "DirectX 10.1");
				}
				else if (oFeatureLevel == D3D_FEATURE_LEVEL_10_0)
				{
					sprintf(dxVersion, "DirectX 10");
				}
				else if (oFeatureLevel < D3D_FEATURE_LEVEL_10_0)
				{
					sprintf(dxVersion, "DirectX 9");
				}
				else
				{
					sprintf(dxVersion, "Raw DirectX Feature Level %x", (int)oFeatureLevel);
				}
			}

			Displayf("DX Version from SystemInfo: %s", dxVersion);

			if (pAdapter)
			{
				DXGI_ADAPTER_DESC oAdapterDesc;

				if (SUCCEEDED(pAdapter->GetDesc(&oAdapterDesc)))
				{
					sprintf(driverVersion, "Vendor: %u Device: %u Subsystem: %u Rev: %u", oAdapterDesc.VendorId, oAdapterDesc.DeviceId, oAdapterDesc.SubSysId, oAdapterDesc.Revision);
					sprintf(VRAM, "%" SIZETFMT "u MB", oAdapterDesc.DedicatedVideoMemory / (1024 * 1024));
					WideCharToMultiByte(CP_UTF8, 0, oAdapterDesc.Description, -1, description, bufferCount, NULL, NULL);
				}

				pAdapter->Release();
			}

			FreeLibrary(hDXGI);
			FreeLibrary(hD3D11);
		}
	}
	else
#endif
	{
		Displayf("DX Version from SystemInfo: %s", dxVersion);
	}
	

	return true;
}

bool CSystemInfo::GetHDDInformation()
{	
	HRESULT hr;
	hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	if(hr == S_OK)
	{
		IWbemLocator *pIWbemLocator = NULL;
		hr = CoCreateInstance(__uuidof(WbemLocator), NULL, CLSCTX_INPROC_SERVER, __uuidof(IWbemLocator), (LPVOID *)&pIWbemLocator);

		if(hr == S_OK)
		{
			BSTR bstrServer = SysAllocString(L"ROOT\\microsoft\\windows\\storage");
			IWbemServices *pIWbemServices;
			hr = pIWbemLocator->ConnectServer(bstrServer, NULL, NULL, 0L, 0L, NULL, NULL, &pIWbemServices);

			if(hr == S_OK)
			{
				hr = CoSetProxyBlanket(pIWbemServices, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_DEFAULT);

				if(hr == S_OK)
				{
					TCHAR exeName[MAX_PATH];
					GetModuleFileName(NULL, exeName, MAX_PATH);
					TCHAR driveLetter = (TCHAR)toupper(exeName[0]);

					WCHAR queryBuffer[512];
					swprintf_s(queryBuffer, 512, L"SELECT * FROM MSFT_Partition WHERE DriveLetter='%c'", driveLetter);
					BSTR bstrWQL = SysAllocString(L"WQL");
					BSTR bstrPartition = SysAllocString(queryBuffer);
					u64 diskNum = 0;
					bool foundDrive = false;

					IEnumWbemClassObject* pPartitionEnum = NULL;
					hr = pIWbemServices->ExecQuery(bstrWQL, bstrPartition, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pPartitionEnum);
					if(hr == S_OK)
					{
						IWbemClassObject *storageWbemObject = NULL;
						ULONG uReturn = 0;

						while(pPartitionEnum)
						{
							HRESULT hr = pPartitionEnum->Next(WBEM_INFINITE, 1, &storageWbemObject, &uReturn);
							if(0 == uReturn || hr != S_OK)
							{
								break;
							}

							VARIANT var;
							if (Verifyf(SUCCEEDED(storageWbemObject->Get(L"DiskNumber", 0, &var, NULL, NULL)), "Could not get DiskNumber"))
							{
								diskNum = var.llVal;
								foundDrive = true;
							}

							storageWbemObject->Release();
						}

						if(pPartitionEnum)
							pPartitionEnum->Release();
					}
					SysFreeString(bstrPartition);

					if(!foundDrive)
					{
						swprintf_s(queryBuffer, 512, L"SELECT * FROM MSFT_Volume WHERE Size=%llu", GetTotalPhysicalHDD());
						BSTR bstrVolume = SysAllocString(queryBuffer);
						IEnumWbemClassObject* pVolumeEnum = NULL;
						hr = pIWbemServices->ExecQuery(bstrWQL, bstrVolume, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pVolumeEnum);
						if(hr == S_OK)
						{
							IWbemClassObject *storageWbemObject = NULL;
							ULONG uReturn = 0;

							while(pVolumeEnum)
							{
								HRESULT hr = pVolumeEnum->Next(WBEM_INFINITE, 1, &storageWbemObject, &uReturn);
								if(0 == uReturn || hr != S_OK)
								{
									break;
								}

								VARIANT var;
								if (Verifyf(SUCCEEDED(storageWbemObject->Get(L"DriveLetter", 0, &var, NULL, NULL)), "Could not get DriveLetter"))
								{
									swprintf_s(queryBuffer, 512, L"SELECT * FROM MSFT_Partition WHERE DriveLetter='%c'", var.cVal);
									BSTR bstrPartition = SysAllocString(queryBuffer);
									IEnumWbemClassObject* pVolPartitionEnum = NULL;
									hr = pIWbemServices->ExecQuery(bstrWQL, bstrPartition, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pVolPartitionEnum);
									if(hr == S_OK)
									{
										IWbemClassObject *storageWbemObject = NULL;
										ULONG uReturn = 0;

										while(pVolPartitionEnum)
										{
											HRESULT hr = pVolPartitionEnum->Next(WBEM_INFINITE, 1, &storageWbemObject, &uReturn);
											if(0 == uReturn || hr != S_OK)
											{
												break;
											}

											VARIANT var;
											var.bstrVal=0; 
											if (Verifyf(SUCCEEDED(storageWbemObject->Get(L"DiskNumber", 0, &var, NULL, NULL)), "Could not get DiskNumber"))
											{
												diskNum = var.llVal;
												foundDrive = true;
											}

											storageWbemObject->Release();
										}

										if(pVolPartitionEnum)
											pVolPartitionEnum->Release();
									}
									SysFreeString(bstrPartition);
								}

								storageWbemObject->Release();
							}

							if(pVolumeEnum)
								pVolumeEnum->Release();
						}
						SysFreeString(bstrVolume);
					}

					// MSFT_Partition.DiskNumber will match MSFT_PhysicalDisk.DeviceId for physical drives despite being UInt32 and String
					swprintf_s(queryBuffer, 512, L"SELECT * FROM MSFT_PhysicalDisk WHERE DeviceId=%d", diskNum);

					BSTR bstrPhysDisk = SysAllocString(queryBuffer);
					IEnumWbemClassObject* pDiskEnum = NULL;
					hr = pIWbemServices->ExecQuery(bstrWQL, bstrPhysDisk, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pDiskEnum);

					if(hr == S_OK)
					{
						IWbemClassObject *storageWbemObject = NULL;
						ULONG uReturn = 0;

						while(pDiskEnum)
						{
							HRESULT hr = pDiskEnum->Next(WBEM_INFINITE, 1, &storageWbemObject, &uReturn);
							if(0 == uReturn || hr != S_OK)
							{
								break;
							}

#define LS(x) L#x
#define FillPhysicalDiskPropertyNumber(prop_name)\
	if (Verifyf(SUCCEEDED(storageWbemObject->Get(LS(prop_name), 0, &var, NULL, NULL)), "Could not get" #prop_name))\
	physicalDisk.prop_name = var.llVal

#define FillPhysicalDiskPropertyString(prop_name)\
	var.bstrVal=0; if (Verifyf(SUCCEEDED(storageWbemObject->Get(LS(prop_name), 0, &var, NULL, NULL)), "Could not get" #prop_name))\
	WideCharToMultiByte(CP_ACP, 0, var.bstrVal, -1, physicalDisk.prop_name, sizeof(physicalDisk.prop_name), NULL, NULL)

#define FillPhysicalDiskPropertyDate(prop_name)\
	if (Verifyf(SUCCEEDED(storageWbemObject->Get(LS(prop_name), 0, &var, NULL, NULL)), "Could not get" #prop_name))\
	physicalDisk.prop_name = var.date

							VARIANT var;
							FillPhysicalDiskPropertyNumber(BusType);
							FillPhysicalDiskPropertyString(DeviceId);
							FillPhysicalDiskPropertyNumber(HealthStatus);
							FillPhysicalDiskPropertyNumber(SpindleSpeed);
							FillPhysicalDiskPropertyNumber(MediaType);
							FillPhysicalDiskPropertyString(FriendlyName);

							storageWbemObject->Release();
						}

						if(pDiskEnum)
							pDiskEnum->Release();
					}
					SysFreeString(bstrPhysDisk);
					SysFreeString(bstrWQL);
				}
				pIWbemServices->Release();
			}
			SysFreeString(bstrServer);
		}
	}
	CoUninitialize();

	return true;
}

int CSystemInfo::GetNumberOfPhysicalCores()
{
	if(numPhysicalCores <= 0)
		RefreshSystemInfo();

	return numPhysicalCores;
}

int CSystemInfo::GetNumberOfCPUS()
{
	if (!m_sysInfo)
		RefreshSystemInfo();

	return m_sysInfo->dwNumberOfProcessors;
}

u64 CSystemInfo::GetTotalPhysicalRAM()
{
	if (!m_memStatus)
		RefreshSystemInfo();

	return (u64)m_memStatus->ullTotalPhys;
}

u64	CSystemInfo::GetTotalPhysicalHDD()
{
	u64 totalBytes = 0;
	GetDiskFreeSpaceEx(NULL, NULL, (PULARGE_INTEGER)&totalBytes, NULL);
	return totalBytes;
}

u64	CSystemInfo::GetAvailablePhysicalHDD()
{
	u64 availableBytes = 0;
	GetDiskFreeSpaceEx(NULL, NULL, NULL, (PULARGE_INTEGER)&availableBytes);
	return availableBytes;
}

bool CSystemInfo::GetIsSSD()
{
	// Media Type 4 is SSD, also spindle speed will be 0
	return physicalDisk.MediaType == 4 && physicalDisk.SpindleSpeed == 0;
}

u64 CSystemInfo::GetMediaType()
{
	return physicalDisk.MediaType;
}

u64 CSystemInfo::GetSpindleSpeed()
{
	return physicalDisk.SpindleSpeed;
}

u64 CSystemInfo::GetBusType()
{
	return physicalDisk.BusType;
}

char* CSystemInfo::GetFriendlyName()
{
	return physicalDisk.FriendlyName;
}

char* CSystemInfo::GetDeviceId()
{
	return physicalDisk.DeviceId;
}

bool CSystemInfo::GetWindowsVersion(char* pszRawOS, char* pszOS, int bufferCount)
{
	OSVERSIONINFOEX osvi;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((OSVERSIONINFO*)&osvi);

	if (!m_sysInfo)
		RefreshSystemInfo();

	Assert(pszOS);
	Assert(pszRawOS);

	// when future OS's arrive, we'll get the raw version number
	if (osvi.wServicePackMajor == 0)
	{
		formatf(pszRawOS, bufferCount, "%d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion);
	}
	else
	{
		formatf(pszRawOS, bufferCount, "%d.%d SP%d.%d", osvi.dwMajorVersion, osvi.dwMinorVersion, osvi.wServicePackMajor, osvi.wServicePackMinor);
	}

	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion > 4)
	{
		// Don't try to get windows version again if we've done it once at the start of the game (tbf lots of the systeminfo will never change but whatever)
		// It can cause crashes as we're using DXDiag stuff to get win 11 info for now.
		if (WindowsVersionRetrieved == true)
		{
			return true;
		}

		safecpy(pszOS, "Microsoft ", bufferCount);

		if (osvi.dwMajorVersion > 6)
		{
			safecatf(pszOS, bufferCount, "Windows Unknown Version (%d.%d)", osvi.dwMajorVersion, osvi.dwMinorVersion);
		}
		else if (osvi.dwMajorVersion == 6)
		{
			if(osvi.dwMinorVersion == 0)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "Windows Vista", bufferCount);
				}
				else
				{
					safecat(pszOS, "Windows Server 2008", bufferCount);
				}
			}
			else if(osvi.dwMinorVersion == 1)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "Windows 7", bufferCount);
				}
				else
				{
					safecat(pszOS, "Windows Server 2008 R2", bufferCount);
				}
			}
			else if(osvi.dwMinorVersion == 2)
			{
				/*
					The GetVersionEx() Windows API reports that the OS is Windows 8 when running Windows 10, so we cannot
					differentiate between Windows 8 and Windows 10 from the API.

					“Applications not manifested for Windows 8.1 or Windows 10 will return the Windows 8 OS version value (6.2).”
					- https://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx

					As a kludge, attempt to load d3d12.dll and if it succeeds, assume we're running Windows 10 (or higher).
				*/
				auto hD3D12Module = LoadLibrary("d3d12.dll");
				if (hD3D12Module != NULL)
				{
					// At this point it could be Win 10 or Win 11 because the API returns the same info for them, 
					// so let's try and get OS info from DX Diag, the only system atm that reports Win 11 correctly				
					rage::DXDiag::GetOsName(pszOS, bufferCount);
					FreeLibrary(hD3D12Module);
				}
				else if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "Windows 8", bufferCount);
				}
				else
				{
					safecat(pszOS, "Windows Server 2012", bufferCount);
				}
			}
			else if(osvi.dwMinorVersion == 3)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "Windows 8.1", bufferCount);
				}
				else
				{
					safecat(pszOS, "Windows Server 2012 R2", bufferCount);
				}
			}
		}

		if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
		{
			safecat(pszOS, "Windows XP ", bufferCount);
			if (osvi.wSuiteMask & VER_SUITE_PERSONAL)
				safecat(pszOS, "Home Edition", bufferCount);
			else
				safecat(pszOS, "Professional", bufferCount);
		}

		if (osvi.dwMajorVersion >= 6)
		{
			if (m_sysInfo->wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64)
				safecat(pszOS, ", 64-bit", bufferCount);
			else if (m_sysInfo->wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL)
				safecat(pszOS, ", 32-bit", bufferCount);
		}

		if (osvi.wServicePackMajor != 0)
		{
			char servicePackString[128];
			sprintf(servicePackString, ", Service Pack %d", osvi.wServicePackMajor);
			safecat(pszOS, servicePackString, bufferCount);
		}
		
		WindowsVersionRetrieved = true;
	}

	return true;
}

struct OSInfo
{
	u32 version;
	u32 osbuild;
	const char *name;
};

// When new OS updates come out we'll need to update here. See https://en.wikipedia.org/wiki/Windows_10_version_history
static const OSInfo g_osInfo[] =
{
	{1511, 10586, "November Update"},
	{1607, 14393, "Anniversary Update"},
	{1703, 15063, "Creators Update"},
	{1709, 16299, "Fall Creators Update"},
	{1803, 17134, "April 2018 Update"},
	{1809, 17763, "October 2018 Update"},
	{1903, 18362, "May 2019 Update"},
	{1909, 18363, "November 2019 Update"},
	{2004, 19041, "May 2020 Update"},
	{2005, 19042, "October 2020 Update"}, // 20H2
	{2011, 19043, "May 2021 Update" }, // The version number is not really correct as it should be 21H1
};

bool CSystemInfo::GetShortWindowsVersion(char* pszOS, int bufferCount)
{
// TODO: This code is going to need to be updated to newer APIs soon.
// From https://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx
// [GetVersionEx may be altered or unavailable for releases after Windows 8.1. Instead, use the Version Helper APIs]
#pragma warning(push)
#pragma warning(disable:4996)    // warning C4996: 'GetVersionExA': was declared deprecated

	OSVERSIONINFOEX osvi;

	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	GetVersionEx((OSVERSIONINFO*)&osvi);

	Initialize();

	Assert(pszOS);

	if (osvi.dwPlatformId == VER_PLATFORM_WIN32_NT && osvi.dwMajorVersion > 4)
	{
		safecpy(pszOS, "Win ", bufferCount);

		if (osvi.dwMajorVersion > 10)
		{
			safecatf_sized(pszOS, bufferCount, "Unk(%d.%d)", osvi.dwMajorVersion, osvi.dwMinorVersion);
		}
		else if (osvi.dwMajorVersion == 10)
		{
			const OSInfo *pInfo = nullptr;
			for (u32 i = 0; i < NELEM(g_osInfo); ++i)
			{
				if (osvi.dwBuildNumber == g_osInfo[i].osbuild)
				{
					pInfo = &g_osInfo[i];
					break;
				}
			}

			if(Verifyf(pInfo, "Could not find Windows OSBuild"))
				safecatf_sized(pszOS, bufferCount, "%d.%d V%d (%d)", osvi.dwMajorVersion, osvi.dwMinorVersion, pInfo->version, pInfo->osbuild);
			else
				safecatf_sized(pszOS, bufferCount, "%d.%d VUnk", osvi.dwMajorVersion, osvi.dwMinorVersion);
		}
		else if (osvi.dwMajorVersion == 6)
		{
			if(osvi.dwMinorVersion == 0)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "Vista", bufferCount);
				}
				else
				{
					safecat(pszOS, "Server 2008", bufferCount);
				}
			}
			else if(osvi.dwMinorVersion == 1)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "7", bufferCount);
				}
				else
				{
					safecat(pszOS, "Server 2008 R2", bufferCount);
				}
			}
			else if(osvi.dwMinorVersion == 2)
			{
				/*
					The GetVersionEx() Windows API reports that the OS is Windows 8 when running Windows 10, so we cannot
					differentiate between Windows 8 and Windows 10 from the API.

					“Applications not manifested for Windows 8.1 or Windows 10 will return the Windows 8 OS version value (6.2).”
					- https://msdn.microsoft.com/en-us/library/windows/desktop/ms724451(v=vs.85).aspx

					As a kludge, attempt to load d3d12.dll and if it succeeds, assume we're running Windows 10 (or higher).
				*/
				auto hD3D12Module = LoadLibrary("d3d12.dll");
				if (hD3D12Module != NULL)
				{
					safecat(pszOS, "10", bufferCount);
					FreeLibrary(hD3D12Module);
				}
				else if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "8", bufferCount);
				}
				else
				{
					safecat(pszOS, "Server 2012", bufferCount);
				}
			}
			else if(osvi.dwMinorVersion == 3)
			{
				if (osvi.wProductType == VER_NT_WORKSTATION)
				{
					safecat(pszOS, "8.1", bufferCount);
				}
				else
				{
					safecat(pszOS, "Server 2012 R2", bufferCount);
				}
			}
		}

		if (osvi.dwMajorVersion == 5 && osvi.dwMinorVersion == 1)
		{
			safecat(pszOS, "XP ", bufferCount);
			if (osvi.wSuiteMask & VER_SUITE_PERSONAL)
				safecat(pszOS, "Home", bufferCount);
			else
				safecat(pszOS, "Pro", bufferCount);
		}

		if (osvi.wServicePackMajor != 0)
		{
			char servicePackString[128];
			sprintf(servicePackString, ", sp %d", osvi.wServicePackMajor);
			safecat(pszOS, servicePackString, bufferCount);
		}
	}

	return true;

#pragma warning(pop)
}

bool CSystemInfo::GetUserLanguage(char* language, int bufferCount)
{
	int langID = GetUserDefaultUILanguage();
	int primID = PRIMARYLANGID(langID);

	switch(primID)
	{
	case LANG_ENGLISH:
		safecpy(language, "English", bufferCount);
		break;
	case LANG_FRENCH:
		safecpy(language, "French", bufferCount);
		break;
	case LANG_ITALIAN:
		safecpy(language, "Italian", bufferCount);
		break;
	case LANG_GERMAN:
		safecpy(language, "German", bufferCount);
		break;
	case LANG_SPANISH:
		safecpy(language, "Spanish", bufferCount);
		break;
	case LANG_RUSSIAN:
		safecpy(language, "Russian", bufferCount);
		break;
	case LANG_JAPANESE:
		safecpy(language, "Japanese", bufferCount);
		break;
	case LANG_PORTUGUESE:
		safecpy(language, "Portuguese", bufferCount);
		break;
	case LANG_POLISH:
		safecpy(language, "Polish", bufferCount);
		break;
	default:
		safecpy(language, "English", bufferCount);
		break;
	}

	return true;
}

int CSystemInfo::GetDXRuntimeVersion()
{
	int iDXRuntimeVersion = 900;

	// Get OS version
	OSVERSIONINFOEX osvi;
	ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
	osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
	::GetVersionEx((OSVERSIONINFO*)(&osvi));

#if __D3D11
	if (osvi.dwMajorVersion >= 6)
	{
		HMODULE hD3D11 = ::LoadLibrary("D3D11.DLL");

		if (hD3D11)
		{
			LPD3D11CREATEDEVICE pfnD3D11CreateDevice = (LPD3D11CREATEDEVICE)::GetProcAddress(hD3D11,"D3D11CreateDevice");

			if (pfnD3D11CreateDevice)
				iDXRuntimeVersion = 1100;

			::FreeLibrary(hD3D11);
		}

		if (iDXRuntimeVersion == 900)
		{
			HMODULE hD3D10 = ::LoadLibrary("D3D10_1.DLL");

			if (hD3D10)
			{
				LPD3D10CREATEDEVICE1 pfnD3D10CreateDevice1 = (LPD3D10CREATEDEVICE1)::GetProcAddress(hD3D10, "D3D10CreateDevice1");

				if (pfnD3D10CreateDevice1)
					iDXRuntimeVersion = 1000;

				::FreeLibrary(hD3D10);
			}
		}
	}
#endif

	return iDXRuntimeVersion;
}

#endif //__WIN32PC && !__TOOL && !__RESOURCECOMPILER
