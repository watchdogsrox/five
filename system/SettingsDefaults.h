
#ifndef SETTINGS_DEFAULTS_H
#define SETTINGS_DEFAULTS_H

#include "grcore/config.h"

#if __WIN32PC && __D3D11
//namespace rage {

#include "grcore/d3dwrapper.h"

#include "SettingsManager.h"

struct IntelDeviceInfoHeader;

class SettingsDefaults {
	Settings m_Defaults;

	DXGI_ADAPTER_DESC m_oAdapterDesc;

	grcDisplayWindow m_defaultDisplayWindow;

	float m_defaultAspectRatio;

	u32 m_defaultDirectXVersion; u32 m_defaultVSync;
	float m_defaultStereo3DConvergence; float m_defaultStereo3DSeparation;

	s64 m_memClockMax; s64 m_gpuClockMax;
	s64 m_videoMemSize; s64 m_sharedMemSize; s64 m_memBandwidth;
	s64 m_numOfCores; s64 m_processingPower; s64 m_numGPUs;

	int m_osVersion; bool m_bcreatedAdapter;
	void* m_pD3D9;
	void* m_hDXGI;

	bool m_setMinTextureQuality;
	int m_currentDefaultSettingsLevel;

	DeviceManufacturer m_cardManufacturer;

	SettingsDefaults();
	~SettingsDefaults() {};

#if __DEV
	static fiStream* m_pLoggingStream;
#endif

public:
	static float sm_fMaxLodScaleMinRange;
	static float sm_fMaxLodScaleMaxRange;

	static float sm_fPedVarietyMultMinRange;
	static float sm_fPedVarietyMultMaxRange;
	static float sm_fVehVarietyMultMinRange;
	static float sm_fVehVarietyMultMaxRange;

#if __DEV
	static void StartLogging(fiStream* pStream) {m_pLoggingStream = pStream;}
	static void StopLogging() {m_pLoggingStream = NULL;}
	static fiStream* GetLoggingStream() {return m_pLoggingStream;}
#endif

	static SettingsDefaults& GetInstance();
	static float CalcFinalLodRootScale(const Settings &settings);

	CGraphicsSettings& GetDefaultGraphics() {return m_Defaults.m_graphics;}
	CSystemSettings& GetDefaultSystem() {return m_Defaults.m_system;}
	CAudioSettings&	GetDefaultAudio() {return m_Defaults.m_audio;}
	CVideoSettings&	GetDefaultVideo() {return m_Defaults.m_video;}
	Settings& GetDefault() {return m_Defaults;}
	s64 totalVideoMemory();

	s64 videoMemoryUsage(CSettings::eSettingsLevel textureQuality, const Settings &settings);
	s64 videoMemSizeFor(Settings settings);
	bool testSettingsForVideoMemorySpace(Settings settings);
	s64 renderTargetMemSizeFor(Settings settings);

	long getIntelDeviceInfo( unsigned int VendorId, IntelDeviceInfoHeader *pIntelDeviceInfoHeader, void *pIntelDeviceInfoBuffer );
	void GetIntelDisplayInfo();
	void GetDisplayInfo();

	void initAdapter();
	void shutdownAdapter();
	void initCardManufacturer();
	void initResolutionToMax();
	void SetResolutionToClosestMode();
	void GetResolutionToClosestMode(grcDisplayWindow& currentSettings);
	void SetDXVersion();

	void PerformMinSpecTests();
	void VerifyCpuSpecs();
	void DisplayMinReqMsgBox(int errorCode);

	void DetermineTextureDefaults();
	void getNVidiaDefaults();
	void getATIDefaults();
	void getIntelDefaults();

	void downgradeToLevel0Settings();
	void downgradeToLevel1Settings();
	void downgradeToLevel2Settings();
	void downgradeToLevel3Settings();
	void downgradeToLevel4Settings();
	void InitializeToMaximumDefaultSettings();

	void LimitDefaultsForWeakCpu();
	void minimumSpecNoCheapShaders ();

	static bool IsLowQualitySystem(const Settings &settings);
};

//}

#endif //__WIN32PC

#endif // SETTINGS_DEFAULTS_H
