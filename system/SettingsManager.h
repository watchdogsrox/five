#ifndef SETTINGS_MANAGER_H
#define SETTINGS_MANAGER_H

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
#include "grcore/device.h"
#include "atl/array.h"
#include "data/base.h"
#include "grcore/config.h"
#include "input/mapper_defs.h"
#include "parser/macros.h"
#include "bank/bkmgr.h"
#include "atl/string.h"

enum eDXLevelSupported
{
	DXS_10,
	DXS_10_1,
	DXS_11
};

enum eSettingsManagerConfig
{
	SMC_AUTO,
	SMC_DEMO,
	SMC_FIXED,
	SMC_SAFE,
	SMC_MEDIUM,
	SMC_UNKNOWN
};

// Base class for settings
struct CSettings : public datBase
{
	// Levels of quality
	enum eSettingsLevel
	{
		Low,
		Medium,
		High,
		Ultra,
		Custom,
		Special,
		eSettingsCount
	};

	PAR_PARSABLE;
};

// All graphics settings that can feasibly be part of Low, Medium or High
struct CGraphicsSettings : public CSettings
{
	// User-editable settings
	// Implcit: Global Detail Level is inferred from the below settings
	eSettingsLevel m_Tessellation;
	float          m_LodScale;
	float		   m_PedLodBias;
	float		   m_VehicleLodBias;
	eSettingsLevel m_ShadowQuality;
	eSettingsLevel m_ReflectionQuality;
	int			   m_ReflectionMSAA;

	eSettingsLevel m_SSAO;
	int            m_AnisotropicFiltering;
	int            m_MSAA;
	int            m_MSAAFragments;
	int            m_MSAAQuality;
	int			   m_SamplingMode;

	eSettingsLevel m_TextureQuality;
	eSettingsLevel m_ParticleQuality;
	eSettingsLevel m_WaterQuality;
	eSettingsLevel m_GrassQuality;
	
	eSettingsLevel m_ShaderQuality;

	// Advanced settings
	eSettingsLevel m_Shadow_SoftShadows;
	bool		   m_UltraShadows_Enabled;
	bool           m_Shadow_ParticleShadows;
	float		   m_Shadow_Distance;
	bool		   m_Shadow_LongShadows;
	float		   m_Shadow_SplitZStart;
	float		   m_Shadow_SplitZEnd;
	float		   m_Shadow_aircraftExpWeight;
	bool		   m_Shadow_DisableScreenSizeCheck;
	bool           m_Reflection_MipBlur;
	bool           m_FXAA_Enabled;
	bool		   m_TXAA_Enabled;
	bool           m_Lighting_FogVolumes;
	bool           m_Shader_SSA;
	float		   m_CityDensity;
    float		   m_PedVarietyMultiplier;
    float		   m_VehicleVarietyMultiplier;
	int            m_DX_Version;
	eSettingsLevel m_PostFX;
	bool		   m_DoF;
	bool           m_HdStreamingInFlight;
	float		   m_MaxLodScale;
	float		   m_MotionBlurStrength;

	bool operator==(const CGraphicsSettings& rhs) const
	{
		return memcmp((const void*)this, (const void*)&rhs, sizeof(*this)) == 0;
	}

	bool AreVideoMemorySettingsSame(CGraphicsSettings& settings) const;
	bool NeedsDeviceResetComparedTo(CGraphicsSettings& settings) const;
	bool NeedsGameRestartComparedTo(CGraphicsSettings& settings) const;

	void PrintOutSettings() const;
	void OutputSettings(fiStream * outHandle) const;

	bool IsParticleShadowsEnabled() const
	{
		return (m_ShadowQuality >= CSettings::High && m_ShaderQuality != CSettings::Low);
	}

	bool IsCloudShadowsEnabled() const
	{
		return (m_ShadowQuality >= CSettings::Medium && m_ShaderQuality != CSettings::Low);
	}

	PAR_PARSABLE;
};

// All system-related settings (CPU- or disk-intensive) that can feasibly be part of Low, Medium or High
struct CSystemSettings : public CSettings
{
	// User-editable settings
	// Implcit: System Level is inferred from the below settings

	// Advanced settings
	int		m_numBytesPerReplayBlock;
	int		m_numReplayBlocks;
	int		m_maxFileStoreSize;

	int		m_maxSizeOfStreamingReplay;

	int     m_SystemPlaceholder1;
	bool    m_SystemPlaceholder2;
	float   m_SystemPlaceholder3;

	bool operator==(const CSystemSettings& rhs) const
	{
		return memcmp((const void*)this, (const void*)&rhs, sizeof(*this)) == 0;
	}

	PAR_PARSABLE;
};

// All audio settings that can feasibly be part of Low, Medium or High
struct CAudioSettings : public CSettings
{
	bool m_Audio3d;

	bool operator==(const CAudioSettings& rhs) const
	{
		return memcmp((const void*)this, (const void*)&rhs, sizeof(*this)) == 0;
	}

	PAR_PARSABLE;
};

enum eAspectRatio
{
	AR_AUTO,
	AR_3to2,
	AR_4to3,
	AR_5to3,
	AR_5to4,
	AR_16to9,
	AR_16to10,
	AR_17to9,
	AR_21to9
};

// Graphics settings that aren't per-level
struct CVideoSettings : public CSettings
{
	int		m_AdapterIndex;
	int		m_OutputIndex;
	int		m_ScreenWidth;
	int		m_ScreenHeight;
	int		m_RefreshRate;
	int		m_Windowed;
	int     m_VSync;
	int		m_Stereo;
	float		m_Convergence;
	float		m_Separation;
	int		m_PauseOnFocusLoss;
	eAspectRatio m_AspectRatio;

	virtual void Validate() {}

	void PrintOutSettings() const;

	bool AreVideoMemorySettingsSame(CVideoSettings& settings) const;
	bool NeedsDeviceResetComparedTo(CVideoSettings& settings) const;
	bool NeedsGameRestartComparedTo(CVideoSettings& settings) const;

	bool operator==(const CVideoSettings& rhs) const
	{
		return memcmp((const void*)this, (const void*)&rhs, sizeof(*this)) == 0;
	}

	PAR_PARSABLE;
};

// Settings group
struct Settings : public datBase
{
	int							m_version;
	eSettingsManagerConfig		m_configSource;
	CGraphicsSettings			m_graphics;
	CSystemSettings				m_system;
	CAudioSettings				m_audio;
	CVideoSettings		        m_video;
	atString					m_VideoCardDescription;

	bool AreVideoMemorySettingsSame(Settings& settings) const
	{
		return m_graphics.AreVideoMemorySettingsSame(settings.m_graphics) && m_video.AreVideoMemorySettingsSame(settings.m_video);
	}
	bool NeedsDeviceResetComparedTo(Settings& settings) const
	{
		return m_graphics.NeedsDeviceResetComparedTo(settings.m_graphics) || m_video.NeedsDeviceResetComparedTo(settings.m_video);
	}
	bool NeedsGameRestartComparedTo(Settings& settings) const
	{
		return m_graphics.NeedsGameRestartComparedTo(settings.m_graphics) || m_video.NeedsGameRestartComparedTo(settings.m_video);
	}
	PAR_PARSABLE;
};

// Manager class that controls loading, saving, accessing and modifying collections of the above settings
class CSettingsManager : public datBase
{
public:
	// Singleton
	static CSettingsManager&		GetInstance() { static CSettingsManager sm; return sm; }

	// Paths
	static const char* sm_settingsPath, *sm_presetsPath;

	// Read from / write to disk
	void ApplyCommandLineOverrides();
	void Initialize();
	void Load();
	bool Save(Settings& saveSettings);

public:
#if RSG_PC
	eDXLevelSupported	HighestDXVersionSupported();
	float CalcFinalLodRootScale();	

#endif // RSG_PC

	static CSettings::eSettingsLevel GetReflectionQuality() { return GetInstance().GetSettings().m_graphics.m_ReflectionQuality; }
	static int						 GetReflectionMSAALevel() { return GetInstance().GetSettings().m_graphics.m_ReflectionMSAA; }

protected:
	Settings m_settings;
	Settings m_currentAppliedSettings;
	Settings m_settingsOnReset;
	Settings m_PreviousSettings;
#if RSG_PC
	Settings m_ReplaySettings;
#endif

	bool m_NewSettingsRequested;
	bool m_AdapterOutputChanged;

	bool m_WidthHeightOverride;
	grcDisplayWindow m_CommandLineWindow;

	bool m_MultiMonitorAvailable;
	grcDisplayWindow m_MultiMonitorWindow;
	int m_MultiMonitorIndex;

#if RSG_PC
	int m_OriginalAdapter;
	bool m_IsLowQualitySystem;
#if !__FINAL
	bool m_initialised;
#endif

	atArray<atArray<grcDisplayWindow>> m_resolutionsLists;
	int m_resolutionIndex;
	atArray<grcDisplayWindow> m_currentResolutionList;
#endif

#if RSG_PC
	void				VerifyDXVerion();
#endif //RSG_PC
public:
#if RSG_PC
	Settings			GetUISettings();
#endif
	const Settings&		GetSettings();

#if RSG_PC
	Settings			GetSafeModeSettings();
#endif

public:
	// Constructor needs to be public for Parser
	CSettingsManager();

public:
#if RSG_PC
	void				SafeUpdate();

#if NV_SUPPORT
	// callback for device stereo change
	void				ChangeStereoSeparation(float fSep);
	void				ChangeStereoConvergence(float fConv);
#endif
#endif
	void				RequestNewSettings(const Settings settings);
	void				RevertToPreviousSettings();

	bool				DoNewSettingsRequireRestart(const Settings& settings);
	bool				DoNewSettingsRequiresDeviceReset(const Settings& settings);
	bool				ApplySettings();
	void				PrintOutSettings(const Settings& settings);

#if RSG_PC
	static void			DeviceLost();
	static void			DeviceReset();
	void				ApplyResetSettings();
	void				ApplyStartupSettings();
	void				ApplyReplaySettings();

	void				ResetShaders();
	void				ApplyVideoSettings(CVideoSettings& videoSettings, bool centerWindow, bool deviceReset, bool initialization = false);

	atArray<grcDisplayWindow> &GetNativeResolutionList(int monitorIndex);
	atArray<grcDisplayWindow> &GetResolutionList(int monitorIndex);

	void				InitializeMultiMonitor();
	void				InitializeResolutionLists();
	int					GetResolutionIndex();
	void				InitResolutionIndex();
	static int			ConvertToMonitorIndex(int adapterOrdinal, int outputMonitor);
	static void			ConvertFromMonitorIndex(int monitorIndex, int &adapterIndex, int &outputMonitorIndex);
	static int			GetMonitorCount();

	void				HandlePossibleAdapterChange();

	void				ResolveDeviceChanged();

	bool				GetCommandLineResolution(grcDisplayWindow &customWindow);
	bool				GetMultiMonitorResolution(grcDisplayWindow &customWindow, int monitorIndex);
	bool				IsSupportedMonitorResoluion(const CVideoSettings &videoSettings);
	bool				IsFullscreenAllowed(CVideoSettings &videoSettings);
	bool				IsLowQualitySystem() const { return m_IsLowQualitySystem; }
#endif

#if RSG_PC
	void				GraphicsConfigSource(char* &info) const;
#endif // RSG_PC

protected:
	void				ApplyLoadedSettings();
	void				GenerateDefaultSettings();

#if RSG_DURANGO || RSG_ORBIS || RSG_BANK
	void				InitializeConsoleSettings();
#endif // 

#if __BANK
public:

	static void			CreateWidgets();

	struct RagSliders 
	{
		Settings settings;
		atArray<bkCombo*> combos;
		bool editMode;
	};
	static RagSliders&	GetRagData();
	static void			UpdateRagData();
	static void			ApplyRagSettings(bool save);
	static void			ApplyRagSettings();
	static void			SaveAndApplyRagSettings();
	static void			ForceReload();
	static void         UpdatePresets();
#endif

#if __DEV && RSG_PC
public:
	static bool			sm_ResetTestsActive;
	static void			ProcessTests();
	static bool			ResetTestsCompleted();
#endif
	PAR_PARSABLE;
};

#endif // RSG_PC || RSG_DURANGO || RSG_OBIS

#endif
