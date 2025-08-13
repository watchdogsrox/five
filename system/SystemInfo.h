//
// name:		SystemInfo.h
// description:	Queries the PC hardware to pull information for a steam like survey
//				as well as for Windows Error Reporting.
//
#ifndef _SYSTEMINFO_H_
#define _SYSTEMINFO_H_

#if __WIN32PC && !__TOOL && !__RESOURCECOMPILER

#pragma warning(disable: 4668)
#include <windows.h>
#pragma warning(error: 4668)
#include <stdio.h>

class CSystemInfo
{
private:
	static SYSTEM_INFO*		m_sysInfo;
	static MEMORYSTATUSEX*	m_memStatus;
	
	static int		numPhysicalCores;
	static char		rawOsVersionString[MAX_PATH];
	static char		windowsString[MAX_PATH];
	static char		processorString[MAX_PATH];
	static char		processorIdentifier[MAX_PATH];
	static char		processorSpeed[MAX_PATH];
	static char		languageString[MAX_PATH];

	static char		videoCardDescription[512];
	static char		videoCardRAM[512];
	static char		videoCardResolution[512];
	static char		driverVersion[512];
	static char		dxVersion[512];

	static char		MBManufacturer[512];
	static char		MBProduct[512];
	static char		BIOSVendor[512];
	static char		BIOSVersion[512];

	static char		AdapterIndex9[4];

	static bool     WindowsVersionRetrieved;

public:
	static void		Initialize();
	static void		RefreshSystemInfo();

	static void		DisplaySystemInfo();
	static void		WriteSystemInfo(FILE* fp);

	static int		GetNumberOfPhysicalCores();
	static int		GetNumberOfCPUS();
	static u64		GetTotalPhysicalRAM();

	static u64		GetTotalPhysicalHDD();
	static u64		GetAvailablePhysicalHDD();

	static bool		GetIsSSD();
	static u64      GetMediaType();
	static u64      GetSpindleSpeed();
	static u64		GetBusType();
	static char*	GetFriendlyName();
	static char*	GetDeviceId();

	static bool		GetCPUInformation(char* processorString, char* processorIdentifier, char* processorSpeed, int bufferCount);
	static bool		GetVideoCardInformation(char* description, char* VRAM, char* displayResolution, char* driverVersion, char* dxVersion, int bufferCount);
	static bool		GetHDDInformation();
	static bool		GetBIOSInformation(char* MBManufacturer, char* MBProduct, char* BIOSVendor, char* BIOSVersion, int bufferCount);

	static bool		GetWindowsVersion(char* pszRawOS, char* pszOS, int bufferCount);
	static bool		GetShortWindowsVersion(char* pszOS, int bufferCount);

	static bool		GetUserLanguage(char* language, int bufferCount);

	// Information Query
	static char*	GetWindowsString()			{ return windowsString; }
	static char*	GetRawWindowsVersion()		{ return rawOsVersionString; }

	static char*	GetProcessorString()		{ return processorString; }
	static char*	GetProcessorIdentifier()	{ return processorIdentifier; }
	static char*	GetProcessorSpeed()			{ return processorSpeed; }
	static char*	GetLanguageString()			{ return languageString; }
	
	static char*	GetVideoCardDescription()	{ return videoCardDescription; }
	static char*	GetVideoCardRam()			{ return videoCardRAM; }
	static char*	GetVideoCardResolution()	{ return videoCardResolution; }
	static char*	GetVideoCardDriverVersion()	{ return driverVersion; }
	static char*	GetDXVersion()				{ return dxVersion; }

	static char*	GetMBManufacturer()			{ return MBManufacturer; }
	static char*	GetMBProduct()				{ return MBProduct; }
	static char*	GetBIOSVersion()			{ return BIOSVersion; }
	static char*	GetBIOSVendor()				{ return BIOSVendor; }

	static int		GetDXRuntimeVersion();
};

#endif //__WIN32PC && !__TOOL && !__RESOURCECOMPILER

#endif

