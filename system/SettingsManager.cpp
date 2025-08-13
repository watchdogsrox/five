#if RSG_PC || RSG_DURANGO || RSG_ORBIS

#include "SettingsManager.h"
#include "SettingsManager_parser.h"

#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "grcore/device.h"
#include "grcore/texturepc.h"
#include "grprofile/timebars.h"
#include "frontend/ProfileSettings.h"
#include "system/exception.h"

#include "control/replay/replay.h"
#include "modelinfo/VehicleModelInfo.h"
#include "peds/ped.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Lights/LightEntity.h"
#include "renderer/Lights/TiledLighting.h"
#include "renderer/PostProcessFX.h"
#include "renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseReflection.h"
#include "renderer/SSAO.h"
#include "renderer/PlantsGrassRenderer.h"
#include "scene/lod/LodMgr.h"
#include "scene/EntityBatch.h"
#include "streaming/streaming.h"
#include "timecycle/TimeCycle.h"
#include "vehicles/vehicle.h"

#include "system/SettingsDefaults.h"

#include "vfx/misc/FogVolume.h"
#include "vfx/misc/Puddles.h"
#include "vfx/decals/DecalManager.h"
#include "vfx/misc/TerrainGrid.h"

const char* CSettingsManager::sm_presetsPath = "platform:/data/presets.meta";

#if RSG_PC
#define ONLY_MATCHING_MULTIMONITOR_WINDOW 0
#define ALLOW_MULTIMONITOR_WINDOW 0
#define CENTER_MULTIMONITOR_WINDOW 0

const char* CSettingsManager::sm_settingsPath = "user:/settings.xml";
#else
const char* CSettingsManager::sm_settingsPath = "platform:/data/settings.xml";
#endif

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
//PARAM(graphicsSettings, "Set all settings to the value specified");

#if __DEV && RSG_PC
PARAM(testDeviceReset, "have device reset called periodically without having to resize the window");
PARAM(dumpRenderTargetSizes, "Dump out the calculated and actual render target sizes");
PARAM(deviceResetLogPath, "Set a path to send the files too. Default is x:/");
#endif

#if __BANK && RSG_PC
PARAM(useConsoleSettings, "Use the console-level graphics settings");
PARAM(settingsFile, "Custom settings file");
#endif // __BANK && RSG_PC

NOSTRIP_PARAM(tessellation,"[QUALITY SETTINGS] Set tessellation on or off (0-3)");
NOSTRIP_PARAM(lodScale,"[QUALITY SETTINGS] Set Lod Distance Level (0.0-1.0f)");
NOSTRIP_PARAM(pedLodBias,"[QUALITY SETTINGS] Set Pedestrians Lod Distance Bias (0.0-1.0f)");
NOSTRIP_PARAM(vehicleLodBias,"[QUALITY SETTINGS] Set Vehicles Lod Distance Bias (0.0-1.0f)");
NOSTRIP_PARAM(shadowQuality,"[QUALITY SETTINGS] Set shadow quality (0-3)");
NOSTRIP_PARAM(reflectionQuality,"[QUALITY SETTINGS] Set reflection quality (0-3)");
NOSTRIP_PARAM(reflectionMSAA,"[QUALITY SETTINGS] Set reflection MSAA level (0, 2, 4, or 8)");
NOSTRIP_PARAM(SSAO,"[QUALITY SETTINGS] Set SSAO quality (0-2)");
NOSTRIP_PARAM(anisotropicQualityLevel, "[QUALITY SETTINGS] Set anisotropic Filter Quality Level (0-16)");
NOSTRIP_PARAM(textureQuality,"[QUALITY SETTINGS] Set texture quality (0-2)");
NOSTRIP_PARAM(particleQuality,"[QUALITY SETTINGS] Set particle quality (0-2)");
NOSTRIP_PARAM(waterQuality,"[QUALITY SETTINGS] Set water quality (0-1)");
NOSTRIP_PARAM(grassQuality,"[QUALITY SETTINGS] Set grass quality (0-3)");
NOSTRIP_PARAM(shaderQuality, "[QUALITY SETTINGS] Set shader quality (0-2)");
NOSTRIP_PARAM(shadowSoftness, "Selects between linear, rpdb, box 4x4, soft 16, NVIDIA PCSS and AMD ContactLess (0-5)");
NOSTRIP_PARAM(particleShadows, "[QUALITY SETTINGS] Enable particle shadows");
NOSTRIP_PARAM(shadowDistance, "[QUALITY SETTINGS] Shadow distance scaler (1.0 - 4.0)");
NOSTRIP_PARAM(shadowLongShadows, "[QUALITY SETTINGS] Enable shadow rendering for dusk and dawn");
NOSTRIP_PARAM(shadowSplitZStart, "[QUALITY SETTINGS] ** shadow something something **");
NOSTRIP_PARAM(shadowSplitZEnd, "[QUALITY SETTINGS] ** shadow something something **");
NOSTRIP_PARAM(shadowAircraftWeight, "[QUALITY SETTINGS] ** shadow something something **");
NOSTRIP_PARAM(shadowDisableScreenSizeCheck, "[QUALITY SETTINGS] Disable screen size check when rendering shadows");
NOSTRIP_PARAM(ultraShadows, "[QUALITY SETTINGS] Enable extra-large shadow resolution");
NOSTRIP_PARAM(reflectionBlur, "[QUALITY SETTINGS] Enable reflection map blur");
NOSTRIP_PARAM(fxaa, "[QUALITY SETTINGS] Set FXAA quality (0-3)");
NOSTRIP_PARAM(txaa, "[QUALITY SETTINGS] Enable NVIDIA TXAA");
NOSTRIP_PARAM(fogVolumes, "[QUALITY SETTINGS] Enable lights volumetric effects in foggy weather");
NOSTRIP_PARAM(SSA, "[QUALITY SETTINGS] Enable SSA");
NOSTRIP_PARAM(cityDensity, "[QUALITY SETTINGS] Control city density (0.0 - 1.0)");
NOSTRIP_PARAM(pedVariety, "[QUALITY SETTINGS] Control ped variety (0.0 - 1.0)");
NOSTRIP_PARAM(vehicleVariety, "[QUALITY SETTINGS] Control vehicle variety (0.0 - 1.0");
NOSTRIP_PARAM(postFX, "[QUALITY SETTINGS] Set postFX quality (0-3)");
NOSTRIP_PARAM(noInGameDOF, "Disable In-game DOF effects");
NOSTRIP_PARAM(HDStreamingInFlight, "[QUALITY SETTINGS] Enable HD streaming while in flight");
NOSTRIP_PARAM(MaxLODScale, "[QUALITY SETTINGS] Maximum lod scale (0.0-1.0)");
NOSTRIP_PARAM(MotionBlurStrength, "[QUALITY SETTINGS] Overall Motion blur strength (0.0-1.0)");
NOSTRIP_PARAM(samplingMode, "[QUALITY SETTINGS] Down/Super Sampling mode (0-9)");

namespace rage
{
	NOSTRIP_XPARAM(width);
	NOSTRIP_XPARAM(height);

	NOSTRIP_PC_XPARAM(frameLimit);
}

#if RSG_PC
PARAM(WindowedMultiMonitor,"Allow windowed mode resolution spanning multiple monitors");
PARAM(OnlyMatchingMultiMonitor,"Only allow the windowed multimonitor resolution if all monitors are the same dimenions");
PARAM(OnlyCenterMultiMonitor,"Only allow the multimonitor system if it's on the centered monitor");

XPARAM(rag);
XPARAM(ragUseOwnWindow);

NOSTRIP_XPARAM(fullscreen);
NOSTRIP_XPARAM(windowed);
namespace rage
{
	NOSTRIP_XPARAM(adapter);
	XPARAM(outputMonitor);
	XPARAM(monitor);

	XPARAM(borderless);
	XPARAM(setHwndMain);
	XPARAM(nvstereo);
}

NOSTRIP_XPARAM(DX10);
NOSTRIP_XPARAM(DX10_1);
NOSTRIP_XPARAM(DX11);
#endif //RSG_PC

NOSTRIP_PARAM(useMinimumSettings,"Reset settings to the minimum");
NOSTRIP_PARAM(safemode,"Start settings at minimum but don't save it");
NOSTRIP_PARAM(UseAutoSettings,"Use automatic generated settings");

NOSTRIP_PARAM(ignoreDifferentVideoCard, "Don't reset settings with a new card");

PARAM(useMaximumSettings,"Set all but resolution to the max");
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS

#if DEVICE_MSAA
namespace rage	{
	NOSTRIP_XPARAM(multiSample);
#if DEVICE_EQAA
	NOSTRIP_XPARAM(multiFragment);
#endif
#if __D3D11
	XPARAM(multiSampleQuality);
#endif
}
#endif

//Increase this define by one every time you make changes where everyone should rebuild from default autoconfigure
#define FILE_VERSION_NUMBER 27

#define DEFAULT_SETTING_VALUE (CSettings::Ultra)

#if RSG_PC
void HandleUnknownDeviceChange()
{
	CSettingsManager::GetInstance().HandlePossibleAdapterChange();
}

void CSettingsManager::DeviceLost()
{

}

void CSettingsManager::DeviceReset()
{
	GetInstance().ApplyResetSettings();
}

void CSettingsManager::ApplyResetSettings()
{
	if ((m_settings.m_graphics == m_settingsOnReset.m_graphics) && (m_settings.m_video == m_settingsOnReset.m_video))
	{
		GetInstance().ResolveDeviceChanged();
		m_settingsOnReset = m_settings;
		GetInstance().InitResolutionIndex();
		return;
	}

	bool resetTheShaders = (m_settings.m_graphics.m_MSAA != m_settingsOnReset.m_graphics.m_MSAA) || (m_settings.m_graphics.m_PostFX != m_settingsOnReset.m_graphics.m_PostFX);
	m_settings = m_settingsOnReset;

	RefreshRate refreshRate = GRCDEVICE.GetRefreshRate();
	m_settings.m_video.m_RefreshRate = refreshRate.Numerator / refreshRate.Denominator;

	CPopulationHelpers::OnSystemCpuRatingChanged();

	ApplySettings();
	if (resetTheShaders) ResetShaders();
}

void CSettingsManager::ResetShaders()
{
	{
#if __ASSERT
		// Disable assert on late loaded shaders.
		grcEffect::SetAllowShaderLoading(true);
#endif // __ASSERT

		DeferredLighting::DeleteShaders();
		DeferredLighting::InitShaders();
		DeferredLighting::TerminateRenderStates();
		DeferredLighting::InitRenderStates();
		CTiledLighting::SetupShader();
		CRenderPhaseCascadeShadowsInterface::ResetShaders();
		SSAO::ResetShaders();
#if SUPPORT_HDAO
		SSAO::Reset_HDAOShaders();
#endif
#if SUPPORT_HDAO2
		SSAO::Reset_HDAO2Shaders();
#endif
		PostFX::ResetMSAAShaders();

		CFogVolumeMgr::ResetShaders();
		CDecalManager::ResetShaders();

		PuddlePassSingleton::InstanceRef().ResetShaders();

		TerrainGrid::GetInstance().InitShaders();

#if __BANK
		DebugRenderingMgr::Init();
#endif

#if __ASSERT
		// Enable assert on late loaded shaders.
		grcEffect::SetAllowShaderLoading(false);
#endif // __ASSERT
	}
}

void CSettingsManager::ApplyVideoSettings(CVideoSettings& videoSettings, bool centerWindow, bool deviceReset, bool initialization)
{
	GRCDEVICE.SetAdapterOrdinal(videoSettings.m_AdapterIndex);
	GRCDEVICE.SetOutputMonitor(videoSettings.m_OutputIndex);

	float fTripleHeadAspectMultiplier = 1.0f;
	float fAspectRatio = 0.0f;

#if SUPPORT_MULTI_MONITOR
	float fWidth = (float)videoSettings.m_ScreenWidth;
	float fHeight = (float)videoSettings.m_ScreenHeight;
	bool bMultihead = (fWidth / fHeight) > 3.5f; // 3x1 cutoff as defined in monitor.cpp

	if(bMultihead)
		fTripleHeadAspectMultiplier = 3.0f;
#endif // SUPPORT_MULTI_MONITOR

	switch(videoSettings.m_AspectRatio)
	{
	case AR_AUTO:
		fAspectRatio = 0.0f;
		break;
	case AR_3to2:
		fAspectRatio = (3.0f / 2.0f);
		break;
	case AR_4to3:
		fAspectRatio = (4.0f / 3.0f);
		break;
	case AR_5to3:
		fAspectRatio = (5.0f / 3.0f);
		break;
	case AR_5to4:
		fAspectRatio = (5.0f / 4.0f);
		break;
	case  AR_16to9:
		fAspectRatio = (16.0f / 9.0f);
		break;
	case AR_16to10:
		fAspectRatio = (16.0f / 10.0f);
		break;
	case AR_17to9:
		fAspectRatio = (17.0f / 9.0f);
		break;
	case AR_21to9:
		fAspectRatio = (21.0f / 9.0f);
		break;
	default:
		Assertf(false, "Invalid aspect enum for aspect ratio");
		fAspectRatio = 0.0f;
		break;
	}

	fAspectRatio *= fTripleHeadAspectMultiplier;
	GRCDEVICE.SetAspectRatio(fAspectRatio);

	if (!IsFullscreenAllowed(videoSettings))
	{
		if (videoSettings.m_Windowed == 0)
			videoSettings.m_Windowed = 2;
	}

	if (videoSettings.m_Windowed == 1)
	{
		GRCDEVICE.SetDesireBorderless(false);

		bool needRecenter = centerWindow;

#if !__FINAL
		if( (PARAM_rag.Get() || PARAM_setHwndMain.Get()) && !PARAM_ragUseOwnWindow.Get() )
			needRecenter = false;
#endif
		if( needRecenter )
		{
			GRCDEVICE.SetRecenterWindow(true);
		}
	}
	else if (videoSettings.m_Windowed == 2)
	{
		GRCDEVICE.SetDesireBorderless(true);
	}
	else
	{
		GRCDEVICE.SetDesireBorderless(false);
	}

	grcDisplayWindow newWindow;
	newWindow.uWidth = videoSettings.m_ScreenWidth;
	newWindow.uHeight = videoSettings.m_ScreenHeight;
	newWindow.uRefreshRate = videoSettings.m_RefreshRate;
	newWindow.bFullscreen = !videoSettings.m_Windowed;

	extern s32 gFrameLimit;
	gFrameLimit = newWindow.uFrameLock = videoSettings.m_VSync;
#if !__FINAL
	//assert when the window is docked in rag not using -ragUseOwnWindow
	//as it may mean that some settings are not actually updated until a restart
	if(PARAM_rag.Get() && m_initialised)
	{
		AssertMsg(PARAM_ragUseOwnWindow.Get(), "Settings may not be applied without -ragUseOwnWindow");	
	}
#endif

	if (initialization)
	{
		GRCDEVICE.InitializeWindow(newWindow);
	}
	else
	{
		if (deviceReset)
		{
			GRCDEVICE.SetRequireDeviceRestoreCallbacks(true);  //We require that the deviceRestore callbacks occur regardless of rebuilding the backbuffer
			GRCDEVICE.SetFullscreenWindow(newWindow);
			GRCDEVICE.ChangeDevice(newWindow);
		}
		else
		{
			DeviceReset();
			CPauseMenu::DeviceReset();
		}
	}
}
#endif

#if NV_SUPPORT
void CSettingsManager::ChangeStereoSeparation(float fSep)
{
	// update pause menu setting
	//CPauseMenu::SetMenuPreference(PREF_VID_STEREO_SEPARATION, (s32)fSep);

	// save this setting
	CSettingsManager::GetInstance().Load();
	if (fSep < 0.0f)	// stereo deactivated
	{
		m_settings.m_video.m_Stereo = 0;	// assume stereo is deactivated
		CSettingsManager::GetInstance().Save(m_settings);
	}
	else
	{
		m_settings.m_video.m_Separation = fSep;
		m_settings.m_video.m_Stereo = 1;	// assume stereo is activated
		CSettingsManager::GetInstance().Save(m_settings);
	}
	m_settingsOnReset = m_settings;
}
void CSettingsManager::ChangeStereoConvergence(float fConv)
{
	// update pause menu setting
	//CPauseMenu::SetMenuPreference(PREF_VID_STEREO_SEPARATION, (s32)fSep);

	// save this setting
	CSettingsManager::GetInstance().Load();
	if (fConv < 0.0f)	// stereo deactivated
	{
		m_settings.m_video.m_Stereo = 0;	// assume stereo is deactivated
		CSettingsManager::GetInstance().Save(m_settings);
	}
	else
	{
		m_settings.m_video.m_Convergence = fConv;
		m_settings.m_video.m_Stereo = 1;	// assume stereo is activated
		CSettingsManager::GetInstance().Save(m_settings);
		CPauseMenu::SetStereoOverride(true);
	}
	m_settingsOnReset = m_settings;
}
#endif

bool CSettingsManager::ApplySettings()
{
	/*************** User settings ***************/

#if RSG_PC
	InitResolutionIndex();

	GRCDEVICE.LockContextIfInitialized();
#endif

#if RSG_PC
	m_IsLowQualitySystem = SettingsDefaults::IsLowQualitySystem(m_settings);

	GRCDEVICE.SetSeparationPercentage((float)m_settings.m_video.m_Separation);
	GRCDEVICE.SetDefaultConvergenceDistance(m_settings.m_video.m_Convergence);

	//Lodscale
	const float finalRootScale = CalcFinalLodRootScale();
	g_LodScale.SetRootScale(finalRootScale);

	const bool allowOrphanScaling = !(m_settings.m_graphics.m_LodScale <= 0.2f);
	fwMapDataContents::SetAllowOrphanScaling(allowOrphanScaling);

	const bool enableInstancedGrass = m_settings.m_graphics.m_GrassQuality > CSettings::Low;
	fwMapDataContents::SetEnableInstancedGrass(enableInstancedGrass);

	CStreaming::SetForceEnableHdMapStreaming(m_settings.m_graphics.m_HdStreamingInFlight);

	const bool forceLowestPriority = (m_settings.m_graphics.m_LodScale <= 0.2f);
	CInstancePriority::SetForceLowestPriority(forceLowestPriority);

	const float pedMemoryMult = m_settings.m_graphics.m_PedVarietyMultiplier*(SettingsDefaults::sm_fPedVarietyMultMaxRange-SettingsDefaults::sm_fPedVarietyMultMinRange)+SettingsDefaults::sm_fPedVarietyMultMinRange;
	CPedPopulation::ms_pedMemoryBudgetMultiplierNG = Clamp(pedMemoryMult,0.05f,10.0f);

	const float vehMemoryMult = m_settings.m_graphics.m_VehicleVarietyMultiplier*(SettingsDefaults::sm_fVehVarietyMultMaxRange-SettingsDefaults::sm_fVehVarietyMultMinRange)+SettingsDefaults::sm_fVehVarietyMultMinRange;
	CVehiclePopulation::ms_vehicleMemoryBudgetMultiplierNG = Clamp(vehMemoryMult,0.05f,10.0f);

#endif // RSG_PC
	CPed::SetLodMultiplierBias(m_settings.m_graphics.m_PedLodBias);
	CVehicleModelInfo::SetLodMultiplierBias(m_settings.m_graphics.m_VehicleLodBias);

	//Tessellation
#if RAGE_SUPPORT_TESSELLATION_TECHNIQUES
	CRenderer::SetTessellationQuality(m_settings.m_graphics.m_Tessellation * (m_settings.m_graphics.m_DX_Version >= 2));
#endif

	//Anisotropic filtering
	grcTextureFactory::SetMaxAnisotropy(m_settings.m_graphics.m_AnisotropicFiltering);
#if __D3D11 || RSG_ORBIS
	grcStateBlock::SetAnisotropicValue(m_settings.m_graphics.m_AnisotropicFiltering);
#endif

	//disable this temporarily so we have a consistency across the platforms, using the defaults in replaysettings.h
	//can be reenabled when settings manager is know to work correctly on all platforms
	//re-enable the block count setting as this shouldn't cause problems.
#if GTA_REPLAY
	//CReplayMgr::SetBytesPerReplayBlock((u32)settings.m_system.m_numBytesPerReplayBlock);
#if !__FINAL
	if (rage::sysParam::FindCommandLineArg("@useDecreasedReplayMemory"))
	{
		m_settings.m_system.m_numReplayBlocks = 30;
	}
#endif // !__FINAL
	CReplayMgr::SetNumberOfReplayBlocksFromSettings((u16)m_settings.m_system.m_numReplayBlocks);
	//CReplayMgr::SetMaxSizeOfStreamingReplay((u32)settings.m_system.m_maxSizeOfStreamingReplay);
#endif // GTA_REPLAY

#if RAGE_TIMEBARS
	const float fRefreshRate = static_cast<float>( grcDevice::GetRefreshRate() );
	const float fVSync = static_cast<float>( m_settings.m_video.m_VSync );
	const float fTargetFrameRate = (fVSync > 0.0f) && (fRefreshRate > 0.0f) ? (fRefreshRate / fVSync) : (DESIRED_FRAME_RATE);
	::rage::g_pfTB.UpdateRatedFrameRateBasedOnSettings(fTargetFrameRate);
#endif

	/*************** Advanced settings ***************/

	//SHADOW QUALITY
	//Soft shadows
	Displayf ("APPLY SOFT SHADOWS IN APPLY SETTINGS");
	CRenderPhaseCascadeShadowsInterface::SetSoftShadowProperties();
	//Particle shadows (DX11 Only)
	//Disable particle shadows in medium or less
	if (!m_settings.m_graphics.IsParticleShadowsEnabled())
	{
		CRenderPhaseCascadeShadowsInterface::SetParticleShadowsEnabled(false);
	}
	else
	{
		CRenderPhaseCascadeShadowsInterface::SetParticleShadowsEnabled(m_settings.m_graphics.m_Shadow_ParticleShadows);
	}
	//Scene  light shadows
	u32 useSceneShadows = m_settings.m_graphics.m_ShadowQuality ? 1 : 0;
	Lights::SetUseSceneShadows(useSceneShadows);
	Lights::SetupForceSceneLightFlags(useSceneShadows);
	Lights::SetMapLightFadeDistance(m_settings.m_graphics.m_LodScale);
	//Headlight shadows
	//Shadow Distance
#if ALLOW_SHADOW_DISTANCE_SETTING
	float shadowDistance = m_settings.m_graphics.m_ShadowQuality > CSettings::eSettingsLevel::High ? m_settings.m_graphics.m_Shadow_Distance : 1.0f;

 	if(m_settings.m_graphics.m_Shadow_SoftShadows >= 4)
 	{
 		//	Don't mess the shadow distance when using AMD or NVidia PCSS
 		shadowDistance = 1.0f;
 	}

	CRenderPhaseCascadeShadowsInterface::SetDistanceMultiplierSetting(shadowDistance, m_settings.m_graphics.m_Shadow_SplitZStart, m_settings.m_graphics.m_Shadow_SplitZEnd, m_settings.m_graphics.m_Shadow_aircraftExpWeight);
#endif
	//Long shadows
#if ALLOW_LONG_SHADOWS_SETTING
	CRenderPhaseCascadeShadowsInterface::SetShadowDirectionClamp(!m_settings.m_graphics.m_Shadow_LongShadows);
	g_timeCycle.SetLongShadowsEnabled(m_settings.m_graphics.m_Shadow_LongShadows);
#endif

#if __D3D11
	//REFLECTION QUALITY
	//Compute shader mip map blur

	//Standard mip blur (if compute not on)
	CRenderPhaseReflection::ms_EnableMipBlur = m_settings.m_graphics.m_Reflection_MipBlur;
#endif // __D3D11

	//FXAA QUALITY
	//FXAA enabled
	PostFX::SetFXAA(m_settings.m_graphics.m_FXAA_Enabled);

#if USE_NV_TXAA
	// TXAA Quality
	// TXAA enabled
	PostFX::SetTXAA(m_settings.m_graphics.m_TXAA_Enabled);
#endif // USE_NV_TXAA

#if __D3D11 && RSG_PC
	//TEXTURE QUALITY
	//Texture quality level
	bool bUseHDSwap = true;
	u32 uTexQuality = m_settings.m_graphics.m_TextureQuality;
	if (uTexQuality < CSettings::High)
	{
		uTexQuality++;
		bUseHDSwap = false;
	}
	CTexLod::EnableStateSwapper(bUseHDSwap);
	grcTexturePC::SetTextureQuality(uTexQuality);
#endif
#if RSG_PC
	grcEffect::SetShaderQuality(m_settings.m_graphics.m_ShaderQuality);
	if(m_settings.m_graphics.m_ShaderQuality >= CSettings::eSettingsLevel::Medium)
		m_settings.m_graphics.m_Shader_SSA = true;
	else
		m_settings.m_graphics.m_Shader_SSA = false;
#endif // RSG_PC
	//SHADER QUALITY
	//Sub sampled alpha
	PostFX::SetUseSubSampledAlpha(m_settings.m_graphics.m_Shader_SSA && ((m_settings.m_graphics.m_MSAA == 0) || (m_settings.m_graphics.m_MSAAFragments == 1)));

	//LIGHTING QUALITY
	//Fog Volumes
	CLightEntity::ms_enableFogVolumes = m_settings.m_graphics.m_Lighting_FogVolumes;
	CGrassBatch::SetQuality(m_settings.m_graphics.m_GrassQuality);
	CFileLoader::SetPtfxSetting(m_settings.m_graphics.m_ParticleQuality);

#if PLANTS_USE_LOD_SETTINGS
	//GRASS QUALITY
	switch (m_settings.m_graphics.m_GrassQuality)
	{
	case CSettings::Low:
		{
			CGrassRenderer::SetPlantLODToUse(PLANTLOD_LOW);
			break;
		}
	case CSettings::Medium:
		{
			CGrassRenderer::SetPlantLODToUse(PLANTLOD_LOW);
			break;
		}
	case CSettings::High:
		{
			CGrassRenderer::SetPlantLODToUse(PLANTLOD_HIGH);
			break;
		}
	case CSettings::Ultra:
		{
			CGrassRenderer::SetPlantLODToUse(PLANTLOD_HIGH);
			break;
		}
	default:
		{
			CGrassRenderer::SetPlantLODToUse(PLANTLOD_LOW);
			break;
		}
	}
#endif

#if __BANK
	CRenderTargets::UpdateBank(true);
#endif

#if RSG_PC
	GRCDEVICE.SetSamplesAndFragments(m_settings.m_graphics.m_MSAA
#if DEVICE_EQAA
		, m_settings.m_graphics.m_MSAAFragments
#endif
		);
	GRCDEVICE.SetMSAAQuality(m_settings.m_graphics.m_MSAAQuality);
	VideoResManager::SetScalingMode(m_settings.m_graphics.m_SamplingMode);
#endif

#if RSG_PC
	GRCDEVICE.UnlockContextIfInitialized();
#endif

#if BACKTRACE_ENABLED && RSG_PC
	auto AddSettingLevelAnnotation = [](const char* name, CSettings::eSettingsLevel level)
	{
		const char* levelString = "unknown";
		switch (level)
		{
		case CSettings::eSettingsLevel::Low:     levelString = "low"; break;
		case CSettings::eSettingsLevel::Medium:  levelString = "medium"; break;
		case CSettings::eSettingsLevel::High:    levelString = "high"; break;
		case CSettings::eSettingsLevel::Ultra:   levelString = "ultra"; break;
		case CSettings::eSettingsLevel::Custom:  levelString = "custom"; break;
		case CSettings::eSettingsLevel::Special: levelString = "special"; break;
		default:
			levelString = "unknown";
		}
		sysException::SetAnnotation(name, levelString);
	};

	auto AddIntAnnotation = [](const char* name, int value)
	{
		char buffer[32];
		formatf(buffer, "%d", value);
		sysException::SetAnnotation(name, buffer);
	};

	auto AddFloatAnnotation = [](const char* name, float value)
	{
		char buffer[32];
		formatf(buffer, "%f", value);
		sysException::SetAnnotation(name, buffer);
	};

	auto AddU32Annotation = [](const char* name, u32 value)
	{
		char buffer[32];
		formatf(buffer, "%u", value);
		sysException::SetAnnotation(name, buffer);
	};

	auto AddBoolAnnotation = [](const char* name, bool value)
	{
		sysException::SetAnnotation(name, value ? "true" : "false");
	};

	// Video Settings
	rage::grcAdapterD3D11* pAdapter = (rage::grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(m_settings.m_video.m_AdapterIndex);
	DXGI_ADAPTER_DESC desc;
	pAdapter->GetDeviceAdapter()->GetDesc(&desc);
	char adapterDesc[128];
	WideToUtf8(adapterDesc, (rage::char16*) desc.Description, 128);

	sysException::SetAnnotation("VID_adapter", adapterDesc);
	AddU32Annotation("VID_manufacturer", pAdapter->GetManufacturer());
	sysException::SetAnnotation("VID_driver", pAdapter->GetDriverString());
	AddIntAnnotation("VID_GPU_Count", pAdapter->GetGPUCount());
	AddIntAnnotation("VID_Output_Count", pAdapter->GetOutputCount());
	AddIntAnnotation("VID_AdapterIndex", m_settings.m_video.m_AdapterIndex);
	AddIntAnnotation("VID_OutputIndex", m_settings.m_video.m_OutputIndex);

	AddIntAnnotation("VID_ScreenWidth", m_settings.m_video.m_ScreenWidth);
	AddIntAnnotation("VID_ScreenHeight", m_settings.m_video.m_ScreenHeight);
	AddIntAnnotation("VID_RefreshRate", m_settings.m_video.m_RefreshRate);
	AddIntAnnotation("VID_Windowed", m_settings.m_video.m_Windowed);
	AddIntAnnotation("VID_VSync", m_settings.m_video.m_VSync);
	AddIntAnnotation("VID_Stereo", m_settings.m_video.m_Stereo);

	// Graphics
	AddIntAnnotation("GFX_DXVersion", m_settings.m_graphics.m_DX_Version);
	AddSettingLevelAnnotation("GFX_Tessellation", m_settings.m_graphics.m_Tessellation);
	AddFloatAnnotation("GFX_LODScale", m_settings.m_graphics.m_LodScale);
	AddFloatAnnotation("GFX_PEDLODBias", m_settings.m_graphics.m_PedLodBias);
	AddFloatAnnotation("GFX_VehicleLODBias", m_settings.m_graphics.m_VehicleLodBias);
	AddSettingLevelAnnotation("GFX_ShadowQuality", m_settings.m_graphics.m_ShadowQuality);
	AddSettingLevelAnnotation("GFX_ReflectionQuality", m_settings.m_graphics.m_ReflectionQuality);
	AddIntAnnotation("GFX_ReflectionMSAA", m_settings.m_graphics.m_ReflectionMSAA);
	AddSettingLevelAnnotation("GFX_SSAO", m_settings.m_graphics.m_SSAO );
	AddIntAnnotation("GFX_Anisotropic", m_settings.m_graphics.m_AnisotropicFiltering);
	AddIntAnnotation("GFX_MSAA", m_settings.m_graphics.m_MSAA);
	AddIntAnnotation("GFX_MSAAQuality", m_settings.m_graphics.m_MSAAQuality);
	AddIntAnnotation("GFX_SampleMode", m_settings.m_graphics.m_SamplingMode);

	AddSettingLevelAnnotation("GFX_TextureQuality", m_settings.m_graphics.m_TextureQuality);
	AddSettingLevelAnnotation("GFX_ParticleQuality", m_settings.m_graphics.m_ParticleQuality);
	AddSettingLevelAnnotation("GFX_WaterQuality", m_settings.m_graphics.m_WaterQuality);
	AddSettingLevelAnnotation("GFX_GrassQuality", m_settings.m_graphics.m_GrassQuality);
	AddSettingLevelAnnotation("GFX_ShaderQuality", m_settings.m_graphics.m_ShaderQuality);
	AddSettingLevelAnnotation("GFX_PostFX", m_settings.m_graphics.m_PostFX);
	AddBoolAnnotation("GFX_DOF", m_settings.m_graphics.m_DoF);

	// Advanced graphics settings
	AddSettingLevelAnnotation("GFX_SoftShadows", m_settings.m_graphics.m_Shadow_SoftShadows);
	AddBoolAnnotation("GFX_UltraShadows", m_settings.m_graphics.m_UltraShadows_Enabled);
	AddBoolAnnotation("GFX_ParticleShadows", m_settings.m_graphics.m_Shadow_ParticleShadows);
	AddFloatAnnotation("GFX_ShadowDistance", m_settings.m_graphics.m_Shadow_Distance);
	AddBoolAnnotation("GFX_LongShadows", m_settings.m_graphics.m_Shadow_LongShadows);
	AddBoolAnnotation("GFX_DisableScreenSizeCheck", m_settings.m_graphics.m_Shadow_DisableScreenSizeCheck);
	AddBoolAnnotation("GFX_ReflectionMipBlur", m_settings.m_graphics.m_Reflection_MipBlur);
	
	AddBoolAnnotation("GFX_FXAA", m_settings.m_graphics.m_FXAA_Enabled);
	AddBoolAnnotation("GFX_TXAA", m_settings.m_graphics.m_TXAA_Enabled);
	AddBoolAnnotation("GFX_FogVolumes", m_settings.m_graphics.m_Lighting_FogVolumes);
	AddBoolAnnotation("GFX_SSA", m_settings.m_graphics.m_Shader_SSA);
	AddFloatAnnotation("GFX_CityDensity", m_settings.m_graphics.m_CityDensity);
	AddFloatAnnotation("GFX_PedVariety", m_settings.m_graphics.m_PedVarietyMultiplier);
	AddFloatAnnotation("GFX_VehicleVariety", m_settings.m_graphics.m_VehicleVarietyMultiplier);
	AddBoolAnnotation("GFX_HDStreamingInFlight", m_settings.m_graphics.m_HdStreamingInFlight);
	AddFloatAnnotation("GFX_MaxLODScale", m_settings.m_graphics.m_MaxLodScale);
	AddFloatAnnotation("GFX_MotionStrength", m_settings.m_graphics.m_MotionBlurStrength);
#endif

	return true;
}

#if RSG_DURANGO || RSG_ORBIS || RSG_BANK
void CSettingsManager::InitializeConsoleSettings()
{
	//Graphics
	m_settings.m_graphics.m_DX_Version = 2;
	m_settings.m_graphics.m_Tessellation = CSettings::High;
	m_settings.m_graphics.m_LodScale = 1.0f;
	m_settings.m_graphics.m_PedLodBias = 0.2f;
	m_settings.m_graphics.m_VehicleLodBias = 0.15f;
	m_settings.m_graphics.m_ShadowQuality = CSettings::High;
	m_settings.m_graphics.m_ReflectionQuality = CSettings::High;
	m_settings.m_graphics.m_ReflectionMSAA = 2;

	m_settings.m_graphics.m_SSAO = CSettings::High;
	m_settings.m_graphics.m_AnisotropicFiltering = 0;

	m_settings.m_graphics.m_MSAA = 4;           //Not sure on these
	m_settings.m_graphics.m_MSAAFragments = 1;      //Not sure on these
	m_settings.m_graphics.m_MSAAQuality = 4;        //Not sure on these
	m_settings.m_graphics.m_SamplingMode = 0;

	m_settings.m_graphics.m_TextureQuality = CSettings::High;
	m_settings.m_graphics.m_ParticleQuality = CSettings::High;
	m_settings.m_graphics.m_WaterQuality = CSettings::High;
	m_settings.m_graphics.m_GrassQuality = CSettings::High;

	m_settings.m_graphics.m_ShaderQuality = CSettings::High;
	m_settings.m_graphics.m_PostFX = CSettings::Ultra;
	m_settings.m_graphics.m_DoF = true;

	// Advanced settings
	m_settings.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
	m_settings.m_graphics.m_UltraShadows_Enabled = false;
	m_settings.m_graphics.m_Shadow_ParticleShadows = true;
	m_settings.m_graphics.m_Shadow_Distance = 1.0f;
	m_settings.m_graphics.m_Shadow_LongShadows = false;
	m_settings.m_graphics.m_Shadow_SplitZStart = 0.93f;
	m_settings.m_graphics.m_Shadow_SplitZEnd = 0.89f;
	m_settings.m_graphics.m_Shadow_aircraftExpWeight = 0.99f;
	m_settings.m_graphics.m_Shadow_DisableScreenSizeCheck = false;
	m_settings.m_graphics.m_Reflection_MipBlur = true;
	m_settings.m_graphics.m_FXAA_Enabled = true;
	m_settings.m_graphics.m_TXAA_Enabled = false;
	m_settings.m_graphics.m_Lighting_FogVolumes = true;
	m_settings.m_graphics.m_Shader_SSA = true;
	m_settings.m_graphics.m_CityDensity = 1.0f;
	m_settings.m_graphics.m_PedVarietyMultiplier = 0.8f;
	m_settings.m_graphics.m_VehicleVarietyMultiplier = 0.8f;
	m_settings.m_graphics.m_HdStreamingInFlight = false;
	m_settings.m_graphics.m_MaxLodScale = 0.0f;
	m_settings.m_graphics.m_MotionBlurStrength = 0.0f;

	//System
	m_settings.m_system.m_numBytesPerReplayBlock = 9000000;
	m_settings.m_system.m_numReplayBlocks = 36;
	m_settings.m_system.m_maxSizeOfStreamingReplay = 1024;
	m_settings.m_system.m_maxFileStoreSize = 65536;


	//Audio
	m_settings.m_audio.m_Audio3d = false;


	//Video
	m_settings.m_video.m_AdapterIndex = 0;
	m_settings.m_video.m_OutputIndex = 0;
	m_settings.m_video.m_ScreenWidth = 1280;
	m_settings.m_video.m_ScreenHeight = 720;
	m_settings.m_video.m_RefreshRate = 60;
	m_settings.m_video.m_Windowed = 0;
	m_settings.m_video.m_VSync = 0;
	m_settings.m_video.m_Stereo = 0;
	m_settings.m_video.m_Convergence = 0.1f;
	m_settings.m_video.m_Separation = 1.0f;
	m_settings.m_video.m_PauseOnFocusLoss = 1;

#if RSG_DURANGO
	// Apply Durango-specific overrides (if ever needed) ... for example:
//	m_settings.m_graphics.m_Shadow_SoftShadows = CSettings::High;
#elif RSG_ORBIS
	// Apply Orbis-specific overrides (if ever needed) ... for example:
//	m_settings.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
#endif // RSG_DURANGO / RSG_ORBIS
}
#endif // 

#if RSG_PC
Settings CSettingsManager::GetSafeModeSettings()
{
	Settings minimumSettings;
	//Graphics
	minimumSettings.m_graphics.m_Tessellation = CSettings::Low;
	minimumSettings.m_graphics.m_LodScale = 0.0f;
	minimumSettings.m_graphics.m_PedLodBias = 0.0f;
	minimumSettings.m_graphics.m_VehicleLodBias = 0.0f;
	minimumSettings.m_graphics.m_ShadowQuality = CSettings::Medium;
	minimumSettings.m_graphics.m_ReflectionQuality = CSettings::Low;
	minimumSettings.m_graphics.m_ReflectionMSAA = 0;

	minimumSettings.m_graphics.m_SSAO = CSettings::Low;
	minimumSettings.m_graphics.m_AnisotropicFiltering = 0;

	minimumSettings.m_graphics.m_MSAA = 0;					//Not sure on these
	minimumSettings.m_graphics.m_MSAAFragments = 0;			//Not sure on these
	minimumSettings.m_graphics.m_MSAAQuality = 0;			//Not sure on these
	minimumSettings.m_graphics.m_SamplingMode = 0;

	minimumSettings.m_graphics.m_TextureQuality = CSettings::Low;
	minimumSettings.m_graphics.m_ParticleQuality = CSettings::Low;
	minimumSettings.m_graphics.m_WaterQuality = CSettings::Low;
	minimumSettings.m_graphics.m_GrassQuality = CSettings::Low;

	minimumSettings.m_graphics.m_ShaderQuality = CSettings::Low;
	minimumSettings.m_graphics.m_PostFX = CSettings::Low;
	minimumSettings.m_graphics.m_DoF = false;
	minimumSettings.m_graphics.m_MotionBlurStrength = 0.0f;

	minimumSettings.m_graphics.m_Shadow_SoftShadows = CSettings::Low;
	minimumSettings.m_graphics.m_UltraShadows_Enabled = false;
	minimumSettings.m_graphics.m_Shadow_ParticleShadows = false;
	minimumSettings.m_graphics.m_Shadow_Distance = 1.0f;				// No GUI or Command line control
	minimumSettings.m_graphics.m_Shadow_LongShadows = false; 
	minimumSettings.m_graphics.m_Shadow_SplitZStart = 0.93f;			// No GUI or Command line control
	minimumSettings.m_graphics.m_Shadow_SplitZEnd = 0.89f;				// No GUI or Command line control
	minimumSettings.m_graphics.m_Shadow_aircraftExpWeight = 0.99f;		// No GUI or Command line control
	minimumSettings.m_graphics.m_Shadow_DisableScreenSizeCheck = false;
	minimumSettings.m_graphics.m_Reflection_MipBlur = true;
	minimumSettings.m_graphics.m_FXAA_Enabled = false;
	minimumSettings.m_graphics.m_TXAA_Enabled = false;
	minimumSettings.m_graphics.m_Lighting_FogVolumes = false;
	minimumSettings.m_graphics.m_Shader_SSA = false;
	minimumSettings.m_graphics.m_CityDensity = 0.0f;
    minimumSettings.m_graphics.m_PedVarietyMultiplier = 0.8f;
    minimumSettings.m_graphics.m_VehicleVarietyMultiplier = 0.8f;

	minimumSettings.m_graphics.m_DX_Version = 2;

	minimumSettings.m_graphics.m_HdStreamingInFlight = false;
	minimumSettings.m_graphics.m_MaxLodScale = 0.0f;

	//System
	minimumSettings.m_system.m_numBytesPerReplayBlock = 9000000;
	minimumSettings.m_system.m_numReplayBlocks = 36;
	minimumSettings.m_system.m_maxSizeOfStreamingReplay = 1024;
	minimumSettings.m_system.m_maxFileStoreSize = 65536;

	//Audio
	minimumSettings.m_audio.m_Audio3d = false;

	//Video
	minimumSettings.m_video.m_AdapterIndex = 0;
	minimumSettings.m_video.m_OutputIndex = 0;
	minimumSettings.m_video.m_ScreenWidth = 800;
	minimumSettings.m_video.m_ScreenHeight = 600;
	minimumSettings.m_video.m_RefreshRate = 60;
	minimumSettings.m_video.m_Windowed = 1;
	minimumSettings.m_video.m_VSync = 2;
	minimumSettings.m_video.m_Stereo = 0;
	minimumSettings.m_video.m_Convergence = 0.1f;
	minimumSettings.m_video.m_Separation = 1.0f;
	minimumSettings.m_video.m_PauseOnFocusLoss = 1;
	minimumSettings.m_video.m_AspectRatio = AR_AUTO;

	minimumSettings.m_configSource = SMC_SAFE;

	return minimumSettings;
}
#endif

#define ApplyCLISettingsI(cmd,setting,min,max)\
	int cmd;\
	if (PARAM_##cmd.Get(cmd))\
	{\
		m_settings.m_graphics.setting = (CSettings::eSettingsLevel)(Clamp(cmd,min,max));\
	}

#define ApplyCLISettingsF(cmd,setting,min,max)\
	float cmd;\
	if (PARAM_##cmd.Get(cmd))\
	{\
		m_settings.m_graphics.setting = (Clamp(cmd,min,max));\
	}

#define ApplyCLISettingsB(cmd,setting)\
	int cmd;\
	if (PARAM_##cmd.Get(cmd))\
	{\
		m_settings.m_graphics.setting = (cmd == 1);\
	}

void CSettingsManager::ApplyCommandLineOverrides()
	{
	ApplyCLISettingsI(tessellation,m_Tessellation,0,3);
	ApplyCLISettingsF(lodScale,m_LodScale,0.0f,1.0f);
	ApplyCLISettingsF(pedLodBias,m_PedLodBias,0.0f,1.0f);
	ApplyCLISettingsF(vehicleLodBias,m_VehicleLodBias,0.0f,1.0f);

	int shadowQuality;
	if (PARAM_shadowQuality.Get(shadowQuality))
	{
		m_settings.m_graphics.m_ShadowQuality = (CSettings::eSettingsLevel)(Clamp(shadowQuality,0,2));
		m_settings.m_graphics.m_ShadowQuality=(CSettings::eSettingsLevel)((int)m_settings.m_graphics.m_ShadowQuality+1);
	}

	ApplyCLISettingsI(reflectionQuality,m_ReflectionQuality,0,3);
	ApplyCLISettingsI(reflectionMSAA, m_ReflectionMSAA,0,8);
	ApplyCLISettingsI(SSAO,m_SSAO,0,2);
	ApplyCLISettingsI(anisotropicQualityLevel,m_AnisotropicFiltering,0,16);
#if DEVICE_MSAA
	ApplyCLISettingsI(multiSample,m_MSAA,0,8);
#if RSG_PC
	ApplyCLISettingsI(multiSampleQuality,m_MSAAQuality,0,16);
	ApplyCLISettingsI(samplingMode,m_SamplingMode,0,9);
#endif
#endif
	ApplyCLISettingsI(textureQuality,m_TextureQuality,0,2);
	ApplyCLISettingsI(particleQuality,m_ParticleQuality,0,2);
	ApplyCLISettingsI(waterQuality,m_WaterQuality,0,1);
	ApplyCLISettingsI(grassQuality,m_GrassQuality,0,3);
	ApplyCLISettingsI(shaderQuality,m_ShaderQuality,0,2);
	ApplyCLISettingsB(ultraShadows, m_UltraShadows_Enabled);
	ApplyCLISettingsI(shadowSoftness,m_Shadow_SoftShadows,0,5);
	ApplyCLISettingsB(particleShadows,m_Shadow_ParticleShadows);
	ApplyCLISettingsF(shadowDistance,m_Shadow_Distance,1.0f,4.0f);
	ApplyCLISettingsB(shadowLongShadows,m_Shadow_LongShadows);
	ApplyCLISettingsF(shadowSplitZStart,m_Shadow_SplitZStart,0.0f,1.0f);
	ApplyCLISettingsF(shadowSplitZEnd,m_Shadow_SplitZEnd,0.0f,1.0f);
	ApplyCLISettingsF(shadowAircraftWeight,m_Shadow_aircraftExpWeight,0.0f,1.0f);
	ApplyCLISettingsB(shadowDisableScreenSizeCheck,m_Shadow_DisableScreenSizeCheck);
	ApplyCLISettingsB(reflectionBlur,m_Reflection_MipBlur);
	ApplyCLISettingsB(fxaa,m_FXAA_Enabled);
	ApplyCLISettingsB(txaa,m_TXAA_Enabled);
	ApplyCLISettingsB(fogVolumes,m_Lighting_FogVolumes);
	ApplyCLISettingsB(SSA,m_Shader_SSA);
	ApplyCLISettingsF(cityDensity,m_CityDensity,0.0f,1.0f);
	ApplyCLISettingsF(pedVariety,m_PedVarietyMultiplier,0.0f,1.0f);
    ApplyCLISettingsF(vehicleVariety,m_VehicleVarietyMultiplier,0.0f,1.0f);
	ApplyCLISettingsI(postFX,m_PostFX,0,3);
	ApplyCLISettingsB(noInGameDOF,m_DoF);
	ApplyCLISettingsB(HDStreamingInFlight,m_HdStreamingInFlight);
	ApplyCLISettingsF(MaxLODScale,m_MaxLodScale,0.0f,1.0f);
	ApplyCLISettingsF(MotionBlurStrength, m_MotionBlurStrength, 0.0f, 1.0f);

#if RSG_PC
	if (PARAM_DX11.Get())
	{
		m_settings.m_graphics.m_DX_Version = 2;
	}
	else if (PARAM_DX10_1.Get())
	{
		m_settings.m_graphics.m_DX_Version = 1;
	}
	else if (PARAM_DX10.Get())
	{
		m_settings.m_graphics.m_DX_Version = 0;
		m_settings.m_graphics.m_MSAA = 0;
	}
#endif // RSG_PC

#if RSG_PC && RSG_BANK
	if (PARAM_useConsoleSettings.Get())
	{
		// Default 
		InitializeConsoleSettings();
		m_settings.m_configSource = SMC_FIXED;
	}
#endif // RSG_PC && RSG_BANK

	int frameLimit;
	if (PARAM_frameLimit.Get(frameLimit))
	{
		m_settings.m_video.m_VSync = rage::Min(frameLimit, 2);
	}

#if RSG_PC
	if (PARAM_fullscreen.Get())
	{
		m_settings.m_video.m_Windowed = 0;
		GRCDEVICE.SetDesireBorderless(false);
	}
	else if (PARAM_borderless.Get())
	{
		m_settings.m_video.m_Windowed = 2;
		GRCDEVICE.SetDesireBorderless(true);
	}
	else if (PARAM_windowed.Get())
	{
		m_settings.m_video.m_Windowed = 1;
		GRCDEVICE.SetDesireBorderless(false);
	}


	int monitorIndex = 0;
	int adapterOrdinal = -1;
	int outputMonitor = -1;
	if (PARAM_monitor.Get(monitorIndex))
	{
		ConvertFromMonitorIndex(monitorIndex, adapterOrdinal, outputMonitor);
	}
	else
	{
		PARAM_adapter.Get(adapterOrdinal);
		PARAM_outputMonitor.Get(outputMonitor);
	}

	if (adapterOrdinal != -1) m_settings.m_video.m_AdapterIndex = adapterOrdinal;
	if (outputMonitor != -1) m_settings.m_video.m_OutputIndex = outputMonitor;

	if (m_settings.m_video.m_AdapterIndex >= grcAdapterManager::GetInstance()->GetAdapterCount())
	{
		m_settings.m_video.m_AdapterIndex = 0;
	}

	const grcAdapterD3D11* adapterTest = (grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(m_settings.m_video.m_AdapterIndex);
	if (m_settings.m_video.m_OutputIndex >= adapterTest->GetOutputCount()) {
		m_settings.m_video.m_OutputIndex = 0;
	}

	int useStereo;
	if (PARAM_nvstereo.Get(useStereo))
	{
		m_settings.m_video.m_Stereo = useStereo;
	}
#endif

	m_CommandLineWindow.uRefreshRate = 60;
	m_WidthHeightOverride = false;
	if (PARAM_width.Get(m_settings.m_video.m_ScreenWidth))
	{
		m_WidthHeightOverride = true;
	}
	if (PARAM_height.Get(m_settings.m_video.m_ScreenHeight))
	{
		m_WidthHeightOverride = true;
	}
	m_CommandLineWindow.uWidth = m_settings.m_video.m_ScreenWidth;
	m_CommandLineWindow.uHeight = m_settings.m_video.m_ScreenHeight;

	if (PARAM_useMaximumSettings.Get())
	{
		m_settings.m_graphics.m_Tessellation = (CSettings::eSettingsLevel) 3;
		m_settings.m_graphics.m_LodScale = 1.0f;
		m_settings.m_graphics.m_PedLodBias = 1.0f;
		m_settings.m_graphics.m_VehicleLodBias = 1.0f;
		m_settings.m_graphics.m_ShadowQuality = (CSettings::eSettingsLevel) 3;
		m_settings.m_graphics.m_ReflectionQuality = (CSettings::eSettingsLevel) 3;
		m_settings.m_graphics.m_ReflectionMSAA = 4;

		m_settings.m_graphics.m_SSAO = (CSettings::eSettingsLevel) 2;
		m_settings.m_graphics.m_AnisotropicFiltering = (CSettings::eSettingsLevel) 16;

		m_settings.m_graphics.m_TextureQuality = (CSettings::eSettingsLevel) 2;
		m_settings.m_graphics.m_ParticleQuality= (CSettings::eSettingsLevel) 2;
		m_settings.m_graphics.m_WaterQuality = (CSettings::eSettingsLevel) 1;
		m_settings.m_graphics.m_GrassQuality = (CSettings::eSettingsLevel) 3;
		m_settings.m_graphics.m_ShaderQuality = (CSettings::eSettingsLevel) 2;

		// Advanced settings
		m_settings.m_graphics.m_Shadow_SoftShadows = (CSettings::eSettingsLevel) 3;
		m_settings.m_graphics.m_UltraShadows_Enabled = false;
		m_settings.m_graphics.m_Shadow_ParticleShadows = true;
		m_settings.m_graphics.m_Shadow_DisableScreenSizeCheck = true;
		m_settings.m_graphics.m_Shadow_LongShadows = true;
		m_settings.m_graphics.m_Reflection_MipBlur = true;
		m_settings.m_graphics.m_FXAA_Enabled = true;
		m_settings.m_graphics.m_Lighting_FogVolumes = true;
		m_settings.m_graphics.m_CityDensity = 1.0f;
		m_settings.m_graphics.m_PedVarietyMultiplier = 0.8f;
		m_settings.m_graphics.m_VehicleVarietyMultiplier = 0.8f;
		m_settings.m_graphics.m_PostFX = (CSettings::eSettingsLevel) 3;
		m_settings.m_graphics.m_DoF = true;
		m_settings.m_graphics.m_HdStreamingInFlight = true;
		m_settings.m_graphics.m_MaxLodScale = 1.0f;
		m_settings.m_graphics.m_MotionBlurStrength = 1.0f;
	}
}

void CSettingsManager::Initialize()
{
#if RSG_DURANGO || RSG_ORBIS
	InitializeConsoleSettings();
#elif RSG_PC
	Load();
#endif

	ApplyCommandLineOverrides();

#if RSG_PC
	ApplyStartupSettings();

	InitializeResolutionLists();

	InitializeMultiMonitor();

	bool ignoreWindowLimits = false;
	if (m_WidthHeightOverride && m_settings.m_video.m_ScreenWidth == (int)m_CommandLineWindow.uWidth && m_settings.m_video.m_ScreenHeight == (int)m_CommandLineWindow.uHeight) ignoreWindowLimits = true;
	if (m_MultiMonitorAvailable && m_settings.m_video.m_ScreenWidth == (int)m_MultiMonitorWindow.uWidth && m_settings.m_video.m_ScreenHeight == (int)m_MultiMonitorWindow.uHeight) ignoreWindowLimits = true;
	GRCDEVICE.SetIngoreWindowLimits(ignoreWindowLimits);
#endif

	ApplyLoadedSettings();
#if RSG_PC
	m_OriginalAdapter = m_settings.m_video.m_AdapterIndex;
	GRCDEVICE.DeviceChangeHandler = HandleUnknownDeviceChange;
#endif
	PrintOutSettings(m_settings);
#if RSG_PC && !__FINAL
	m_initialised = true;
#endif
}

#if RSG_PC


void CSettingsManager::ApplyReplaySettings()
{
	#if GTA_REPLAY
		m_ReplaySettings = m_settings;

		PostFX::SetDOFInReplay(m_settings.m_graphics.m_PostFX>=CSettings::High);

		m_ReplaySettings.m_graphics.m_PostFX = CSettings::Ultra;
		m_ReplaySettings.m_graphics.m_ReflectionQuality = CSettings::Ultra;
		m_ReplaySettings.m_graphics.m_ShadowQuality = CSettings::Ultra;
		m_ReplaySettings.m_graphics.m_Shadow_SoftShadows = rage::Max(m_settings.m_graphics.m_Shadow_SoftShadows, CSettings::Ultra);
		m_ReplaySettings.m_graphics.m_FXAA_Enabled = true;
		m_ReplaySettings.m_graphics.m_AnisotropicFiltering = 16;
		m_ReplaySettings.m_graphics.m_SSAO = CSettings::High;
		m_ReplaySettings.m_graphics.m_Tessellation = rage::Max(CSettings::High, m_ReplaySettings.m_graphics.m_Tessellation);
		m_ReplaySettings.m_graphics.m_WaterQuality = CSettings::High;
		m_ReplaySettings.m_graphics.m_DoF = true;
		m_ReplaySettings.m_graphics.m_PedLodBias = 1.0f;
		m_ReplaySettings.m_graphics.m_VehicleLodBias = 1.0f;

		GetInstance().RequestNewSettings(m_ReplaySettings);
	#endif
}


void CSettingsManager::ApplyStartupSettings()
{
	if (SettingsDefaults::GetInstance().GetDefaultGraphics().m_DX_Version < m_settings.m_graphics.m_DX_Version)
	{
		m_settings.m_graphics.m_DX_Version = SettingsDefaults::GetInstance().GetDefaultGraphics().m_DX_Version;
	}

	if (m_settings.m_graphics.m_DX_Version == 2)
	{
		PARAM_DX11.Set("1");
	}
	else if (m_settings.m_graphics.m_DX_Version == 1)
	{
		PARAM_DX10_1.Set("1");
	}
	else if (m_settings.m_graphics.m_DX_Version == 0)
	{
		PARAM_DX10.Set("1");
		PARAM_multiSample.Set("0");
		m_settings.m_graphics.m_MSAA = 0;
	}
	GRCDEVICE.SetRequestStereoDesired(m_settings.m_video.m_Stereo ? 1 : 0);
}
#endif // RSG_PC

void GetFinalConfigFilePath(char *finalPath, const char* initialPath)
{
	strcpy(finalPath, initialPath);

#if __BANK && RSG_PC
	const char *customSettingsPath;
	if(PARAM_settingsFile.Get(customSettingsPath))
	{
		strcpy(finalPath, "user:/");
		strcat(finalPath, customSettingsPath);
	}
#endif
}


void CSettingsManager::Load()
{
	INIT_PARSER;

	char settingsFile[255];
	GetFinalConfigFilePath(settingsFile, sm_settingsPath);

	// Register this object
	PARSER.InitObject(*this);

	m_settings.m_configSource = SMC_UNKNOWN;

	// Parse the user's settings file, checking for errors.
	parSettings settings;
	settings.SetFlag(parSettings::READ_SAFE_BUT_SLOW, true);
	settings.SetFlag(parSettings::WARN_ON_MISSING_DATA, true);
	if (PARAM_UseAutoSettings.Get())
	{
		GenerateDefaultSettings();
	}
	else if (!rage::ASSET.Exists(settingsFile, ""))
	{
		Warningf("Settings file missing, regenerating!");
		GenerateDefaultSettings();
		Save(m_settings);
	}
	else if (!Verifyf(PARSER.LoadObject(settingsFile, "", m_settings, &settings), "Loading settings failed, regenerating!"))
	{
		GenerateDefaultSettings();
		Save(m_settings);
	}
	else
		Displayf("[settings] Loaded settings successfully.");

#if RSG_PC
	if (m_settings.m_video.m_AdapterIndex >= grcAdapterManager::GetInstance()->GetAdapterCount())
	{
		m_settings.m_video.m_AdapterIndex = 0;
	}

	const grcAdapterD3D11* adapterTest = (grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(m_settings.m_video.m_AdapterIndex);
	if (m_settings.m_video.m_OutputIndex >= adapterTest->GetOutputCount()) {
		m_settings.m_video.m_OutputIndex = 0;
	}

	IDXGIAdapter* pDeviceAdapter = adapterTest->GetDeviceAdapter();
	AssertMsg(pDeviceAdapter, "Could not get the current device adapter");

	DXGI_ADAPTER_DESC oAdapterDesc;
	pDeviceAdapter->GetDesc(&oAdapterDesc);

	char description[128 * 3];
	wchar_t* wideDescription = rage::DXDiag::GetCardName(oAdapterDesc);
	WideToAscii((char *)description, (const rage::char16*)wideDescription, (int) wcslen((const rage::char16*)wideDescription)+1);

	if (!PARAM_ignoreDifferentVideoCard.Get())
	{
		if (m_settings.m_version != FILE_VERSION_NUMBER || m_settings.m_VideoCardDescription != description)
		{
			GenerateDefaultSettings();
			Save(m_settings);
		}
	}
#endif
#if RSG_PC
	if (PARAM_useMinimumSettings.Get() || PARAM_safemode.Get())
	{
		m_settings = GetSafeModeSettings();
		m_settings.m_configSource = SMC_SAFE;
	}
#endif

	SHUTDOWN_PARSER;
}

bool CSettingsManager::Save(Settings& saveSettings)
{
#if RSG_PC
	IDXGIAdapter* pDeviceAdapter = ((const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(saveSettings.m_video.m_AdapterIndex))->GetDeviceAdapter();
	AssertMsg(pDeviceAdapter, "Could not get the current device adapter");

	DXGI_ADAPTER_DESC oAdapterDesc;
	pDeviceAdapter->GetDesc(&oAdapterDesc);

	char description[128 * 3];
	wchar_t* wideDescription = rage::DXDiag::GetCardName(oAdapterDesc);
	WideToAscii((char *)description, (const rage::char16*)wideDescription, (int) wcslen((const rage::char16*)wideDescription)+1);

	saveSettings.m_VideoCardDescription.Set((char *)description, (int) strlen(description), 0, -1);
#endif

	char settingsFile[255];
	GetFinalConfigFilePath(settingsFile, sm_settingsPath);

	bool success = true;

	INIT_PARSER;
#if RSG_PC || __BANK
	success = PARSER.SaveObjectAnyBuild(settingsFile, "", &saveSettings);
#else
	(void) saveSettings;
#endif
	SHUTDOWN_PARSER;

	return success;
}

void CSettingsManager::GenerateDefaultSettings()
{
#if RSG_PC && __D3D11
	SettingsDefaults::GetInstance().PerformMinSpecTests();

//	if( PARAM_UseAutoSettings.Get() )
	{
		m_settings = GetSafeModeSettings();
		m_settings.m_graphics = SettingsDefaults::GetInstance().GetDefaultGraphics();
		m_settings.m_video = SettingsDefaults::GetInstance().GetDefaultVideo();
		m_settings.m_configSource = SMC_AUTO;

		m_settings.m_video.m_Stereo = GRCDEVICE.CanUseStereo();
#if __BANK
		m_settings.m_video.m_Windowed = 1;
#endif
	}
#elif RSG_DURANGO || RSG_ORBIS
	InitializeConsoleSettings();
#endif // RSG_PC && __D3D11
	m_settings.m_version = FILE_VERSION_NUMBER;
}

#if RSG_PC
void CSettingsManager::RevertToPreviousSettings()
{
	PostFX::SetDOFInReplay(true);

	RequestNewSettings(m_PreviousSettings);
}

void AddToListIfNotPresent(grcDisplayWindow &window, atArray<grcDisplayWindow> &list)
{
	bool foundMatch = false;
	for(int index = 0; index < list.size(); index++)
	{
		if (list[index].uWidth == window.uWidth && list[index].uHeight == window.uHeight)
			foundMatch = true;
	}

	if (!foundMatch) list.PushAndGrow(window);
}

void CSettingsManager::InitializeMultiMonitor()
{
	int iAllowMultiMonitorWindow = 0;
	int iOnlyMatchingWindowMonitor = 0;
#if ALLOW_MULTIMONITOR_WINDOW
	iAllowMultiMonitorWindow = 1;
#endif
#if ONLY_MATCHING_MULTIMONITOR_WINDOW
	iOnlyMatchingWindowMonitor = 1;
#endif

	PARAM_WindowedMultiMonitor.Get(iAllowMultiMonitorWindow);
	PARAM_OnlyMatchingMultiMonitor.Get(iOnlyMatchingWindowMonitor);

	struct MonitorInfo {
		RECT DesktopCoordinates;
		int monitorIndex;
	};

	atArray<MonitorInfo> monitors;

	const int monitorCount = GetMonitorCount();

	if (iAllowMultiMonitorWindow == 0 || monitorCount < 2)
	{
		m_MultiMonitorAvailable = false;
		return;
	}

	const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(0);
	const grcAdapterD3D11Output* pMonitorOutput = pAdapter->GetOutput(0);
	DXGI_OUTPUT_DESC desc;
	unsigned int dpiX, dpiY;
	grcAdapterD3D11Output::GetDesc(pAdapter->GetHighPart(), pAdapter->GetLowPart(), pMonitorOutput, desc, dpiX, dpiY);

	int referenceWidth = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
	int referenceHeight = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;
	int referenceTop = desc.DesktopCoordinates.top;

	int smallestMatchingWidth = referenceWidth;
	int smallestHeight = referenceHeight;
	int smallestIndex = 0;

	int lowestTop = referenceTop;
	int highestBottom = desc.DesktopCoordinates.bottom;

	bool bAllMonitorsMatch = true;
	for (int index = 0; index < monitorCount; index++)
	{
		int adapterOrdinal, outputMonitor;
		ConvertFromMonitorIndex(index, adapterOrdinal, outputMonitor);

		pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(adapterOrdinal);
		pMonitorOutput = pAdapter->GetOutput(outputMonitor);

		grcAdapterD3D11Output::GetDesc(pAdapter->GetHighPart(), pAdapter->GetLowPart(), pMonitorOutput, desc, dpiX, dpiY);

		MonitorInfo monitor;
		monitor.DesktopCoordinates = desc.DesktopCoordinates;
		monitor.monitorIndex = index;

		monitors.PushAndGrow(monitor);

		int width = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
		int height = desc.DesktopCoordinates.bottom - desc.DesktopCoordinates.top;

		if (width != referenceWidth || height != referenceHeight || referenceTop != desc.DesktopCoordinates.top)
		{
			bAllMonitorsMatch = false;
		}

		if (smallestHeight > height || (smallestHeight == height && smallestMatchingWidth > width))
		{
			smallestHeight = height;
			smallestMatchingWidth = width;
			smallestIndex = index;
		}

		if (lowestTop > desc.DesktopCoordinates.top)
		{
			lowestTop = desc.DesktopCoordinates.top;
		}

		if (highestBottom < desc.DesktopCoordinates.bottom)
		{
			highestBottom = desc.DesktopCoordinates.bottom;
		}
	}

	for (long i = 0; i < monitorCount-1; i++)
	{
		for (long j = 0; j < monitorCount-1; j++)
		{
			if (monitors[j].DesktopCoordinates.left > monitors[j+1].DesktopCoordinates.left)
			{
				MonitorInfo swap = monitors[j];
				monitors[j] = monitors[j+1];
				monitors[j+1] = swap;
			}
		}
	}

	bool windowsAreSequential = true;

	for (long i = 0; i < monitorCount - 1; i++)
	{
		if (monitors[i].DesktopCoordinates.right != monitors[i+1].DesktopCoordinates.left)
		{
			windowsAreSequential = false;
		}
	}

	if ((bAllMonitorsMatch || iOnlyMatchingWindowMonitor == 0) && windowsAreSequential && smallestHeight >= 600)
	{
		m_MultiMonitorAvailable = true;
		m_MultiMonitorWindow.uWidth = smallestMatchingWidth * min(monitorCount, 3);
		m_MultiMonitorWindow.uHeight = smallestHeight;
		m_MultiMonitorWindow.uRefreshRate = 60;
		m_MultiMonitorIndex = monitors[monitorCount / 2].monitorIndex;
	}
	else
	{
		m_MultiMonitorAvailable = false;
	}
}

void CSettingsManager::InitializeResolutionLists()
{
	m_resolutionsLists.clear();
	m_resolutionIndex = -1;

	const int monitorCount = GetMonitorCount();

	for (int index = 0; index < monitorCount; index++)
	{
		int adapterOrdinal, outputMonitor;
		ConvertFromMonitorIndex(index, adapterOrdinal, outputMonitor);

		const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(adapterOrdinal);
		const grcAdapterD3D11Output* pMonitorOutput = pAdapter->GetOutput(outputMonitor);

		atArray<grcDisplayWindow> list;

		int iDisplayModes = pMonitorOutput->GetModeCount();
		grcDisplayWindow oDisplayWindow;
		for (int mode = 0; mode < iDisplayModes; mode++)
		{
			pMonitorOutput->GetMode(&oDisplayWindow, mode);
			if (oDisplayWindow.uHeight < oDisplayWindow.uWidth)
				list.PushAndGrow(oDisplayWindow);
		}

		if (list.size() == 0)
		{
			DXGI_OUTPUT_DESC desc;
			unsigned int dpiX, dpiY;
			grcAdapterD3D11Output::GetDesc(pAdapter->GetHighPart(), pAdapter->GetLowPart(), pMonitorOutput, desc, dpiX, dpiY);

			for (int mode = 0; mode < iDisplayModes; mode++)
			{
				pMonitorOutput->GetMode(&oDisplayWindow, mode);
				oDisplayWindow.uHeight = (u32) ((float) oDisplayWindow.uWidth * ((float) oDisplayWindow.uWidth / (float)oDisplayWindow.uHeight));
				AddToListIfNotPresent(oDisplayWindow, list);

				oDisplayWindow.uHeight = (u32)(oDisplayWindow.uWidth * 0.75f);
				AddToListIfNotPresent(oDisplayWindow, list);

				oDisplayWindow.uHeight = (u32)(oDisplayWindow.uWidth * 0.5625f);
				AddToListIfNotPresent(oDisplayWindow, list);
			}

			oDisplayWindow.uWidth = desc.DesktopCoordinates.right - desc.DesktopCoordinates.left;
			oDisplayWindow.uHeight = (u32)((float)oDisplayWindow.uWidth / grcDevice::sm_minAspectRatio);

			AddToListIfNotPresent(oDisplayWindow, list);
		}

		for(int i = 1; i < list.size(); ++i)
		{
			if(list[i-1].uWidth == list[i].uWidth && list[i-1].uHeight == list[i].uHeight)
			{
				list.Delete(i);
				i--;
			}
		}

		for(int i = 0; i < list.size(); ++i)
		{
			if(list[i].uWidth < 800 || list[i].uHeight < 600)
			{
				list.Delete(i);
				i--;
			}
		}

		m_resolutionsLists.PushAndGrow(list);
	}

	InitResolutionIndex();
}

void CSettingsManager::HandlePossibleAdapterChange()
{
	bool result = grcAdapterManager::GetInstance()->VerifyAdapters(DXGI_FORMAT_R8G8B8A8_UNORM);

	if (!result)
	{
		grcAdapterManager::ShutdownClass();
		grcAdapterManager::InitClass(DXGI_FORMAT_R8G8B8A8_UNORM);

		if (m_settings.m_video.m_AdapterIndex >= grcAdapterManager::GetInstance()->GetAdapterCount())
		{
			m_settings.m_video.m_AdapterIndex = 0;
			GRCDEVICE.SetAdapterOrdinal(0);
		}
		if (m_settings.m_video.m_OutputIndex >= grcAdapterManager::GetInstance()->GetAdapter(m_settings.m_video.m_AdapterIndex)->GetOutputCount())
		{
			m_settings.m_video.m_OutputIndex = 0;
			GRCDEVICE.SetOutputMonitor(0);
		}
		m_settings.m_video.m_Windowed = 1;

		InitializeResolutionLists();

		m_AdapterOutputChanged = true;

		RequestNewSettings(m_settings);
	}
}

void CSettingsManager::ResolveDeviceChanged()
{
	if (!GRCDEVICE.IsWindowed())
	{
		m_settings.m_video.m_Windowed = 0;
	}
	else
	{
		LONG_PTR result = GetWindowLongPtr( g_hwndMain, GWL_STYLE );
		if (result & WS_POPUP)
		{
			m_settings.m_video.m_Windowed = 2;
		}
		else
		{
			m_settings.m_video.m_Windowed = 1;
		}
	}

	if (GRCDEVICE.IsWindowDragResized() || m_settings.m_video.m_Windowed == 0)
	{
		m_settings.m_video.m_ScreenWidth = GRCDEVICE.GetWidth();
		m_settings.m_video.m_ScreenHeight = GRCDEVICE.GetHeight();
	}

	RefreshRate refreshRate = GRCDEVICE.GetRefreshRate();
	m_settings.m_video.m_RefreshRate = refreshRate.Numerator / refreshRate.Denominator;
}

void CSettingsManager::InitResolutionIndex()
{
	m_resolutionIndex = -1;
	int monitorIndex = ConvertToMonitorIndex(m_settings.m_video.m_AdapterIndex, m_settings.m_video.m_OutputIndex);
	atArray<grcDisplayWindow> &list = GetResolutionList(monitorIndex);

	if (m_settings.m_video.m_Windowed == 0)
		list = GetNativeResolutionList(monitorIndex);

	for (int index = 0; index < list.size(); index++)
	{
		if (list[index].uWidth == (u32)m_settings.m_video.m_ScreenWidth && list[index].uHeight == (u32)m_settings.m_video.m_ScreenHeight)
		{
			m_resolutionIndex = index;
		}
	}

	if (m_resolutionIndex == -1 && m_settings.m_video.m_Windowed == 0)
	{
		for (int index = 0; index < list.size(); index++)
		{
			if (list[index].uWidth == (u32)GRCDEVICE.GetWidth() && list[index].uHeight == (u32)GRCDEVICE.GetHeight())
			{
				m_resolutionIndex = index;
			}
		}
	}

	if (m_resolutionIndex == -1)
	{
		m_resolutionIndex = 0;
	}
}

atArray<grcDisplayWindow> &CSettingsManager::GetNativeResolutionList(int monitorIndex)
{
	return m_resolutionsLists[monitorIndex];
}

atArray<grcDisplayWindow> &CSettingsManager::GetResolutionList(int monitorIndex)
{
	m_currentResolutionList = m_resolutionsLists[monitorIndex];

	grcDisplayWindow customWindow;
	if (CSettingsManager::GetInstance().GetCommandLineResolution(customWindow))
	{
		AddToListIfNotPresent(customWindow, m_currentResolutionList);
	}
	if (CSettingsManager::GetInstance().GetMultiMonitorResolution(customWindow, monitorIndex))
	{
		AddToListIfNotPresent(customWindow, m_currentResolutionList);
	}

	if (GRCDEVICE.IsWindowDragResized())
	{
		grcDisplayWindow uniqueWindow;
		uniqueWindow.uWidth = GRCDEVICE.GetWidth();
		uniqueWindow.uHeight = GRCDEVICE.GetHeight();
		uniqueWindow.uRefreshRate = 60;

		AddToListIfNotPresent(uniqueWindow, m_currentResolutionList);
	}
	return m_currentResolutionList;
}

int CSettingsManager::GetResolutionIndex()
{
	if (GRCDEVICE.IsWindowDragResized())
	{
		int monitorIndex = ConvertToMonitorIndex(m_settings.m_video.m_AdapterIndex, m_settings.m_video.m_OutputIndex);
		atArray<grcDisplayWindow> &resolutionList = GetResolutionList(monitorIndex);
		u32 width = GRCDEVICE.GetWidth();
		u32 height = GRCDEVICE.GetHeight();
		for (int index = 0; index < resolutionList.size(); index++)
		{
			if (resolutionList[index].uWidth == width && resolutionList[index].uHeight == height)
			{
				return index;
			}
		}
	}
	return m_resolutionIndex;
}

int CSettingsManager::ConvertToMonitorIndex(int adapterOrdinal, int outputMonitor)
{
	int monitorCount = 0;
	for (int adapterIndex = 0; adapterIndex < adapterOrdinal; adapterIndex++)
	{
		const grcAdapterD3D11* adapter = (grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(adapterIndex);
		monitorCount += adapter->GetOutputCount();
	}
	return monitorCount + outputMonitor;
}

void CSettingsManager::ConvertFromMonitorIndex(int monitorIndex, int &adapterOrdinal, int &outputMonitor)
{
	int outputIndexTotal = 0;
	adapterOrdinal = outputMonitor = 0;
	for (int adapterIndex = 0; adapterIndex < grcAdapterManager::GetInstance()->GetAdapterCount(); adapterIndex++)
	{
		const grcAdapterD3D11* adapter = (grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(adapterIndex);
		if ((adapter->GetOutputCount() + outputIndexTotal) > monitorIndex)
		{
			adapterOrdinal = adapterIndex;
			outputMonitor = monitorIndex - outputIndexTotal;
			break;
		}
		outputIndexTotal += adapter->GetOutputCount();
	}
}

int CSettingsManager::GetMonitorCount()
{
	int monitorTotal = 0;
	for (int adapterIndex = 0; adapterIndex < grcAdapterManager::GetInstance()->GetAdapterCount(); adapterIndex++)
	{
		const grcAdapterD3D11* adapter = (grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(adapterIndex);
		monitorTotal += adapter->GetOutputCount();
	}

	return monitorTotal;
}

bool CSettingsManager::GetCommandLineResolution(grcDisplayWindow &customWindow)
{
	customWindow = m_CommandLineWindow;
	return m_WidthHeightOverride;
}

bool CSettingsManager::GetMultiMonitorResolution(grcDisplayWindow &customWindow, int monitorIndex)
{
	int iOnlyUseCenterMonitor = 0;
#if CENTER_MULTIMONITOR_WINDOW
	iOnlyUseCenterMonitor = 1;
#endif
	PARAM_OnlyCenterMultiMonitor.Get(iOnlyUseCenterMonitor);
	if (iOnlyUseCenterMonitor == 1 && monitorIndex != m_MultiMonitorIndex)
		return false;

	customWindow = m_MultiMonitorWindow;
	return m_MultiMonitorAvailable;
}

bool CSettingsManager::IsSupportedMonitorResoluion(const CVideoSettings &videoSettings)
{
	int monitorIndex = ConvertToMonitorIndex(videoSettings.m_AdapterIndex, videoSettings.m_OutputIndex);
	atArray<grcDisplayWindow> &list = GetNativeResolutionList(monitorIndex);

	grcDisplayWindow customWindow;
	if (CSettingsManager::GetInstance().GetCommandLineResolution(customWindow))
	{
		AddToListIfNotPresent(customWindow, list);
	}
	if (CSettingsManager::GetInstance().GetMultiMonitorResolution(customWindow, monitorIndex))
	{
		AddToListIfNotPresent(customWindow, list);
	}

	for (int index = 0; index < list.size(); index++)
	{
		if (list[index].uWidth == (u32)videoSettings.m_ScreenWidth && list[index].uHeight == (u32)videoSettings.m_ScreenHeight)
		{
			return true;
		}
	}

	return false;
}

bool CSettingsManager::IsFullscreenAllowed(CVideoSettings &videoSettings)
{
	const grcAdapterD3D11* pAdapter = (const grcAdapterD3D11*)grcAdapterManager::GetInstance()->GetAdapter(videoSettings.m_AdapterIndex);
	const grcAdapterD3D11Output* pMonitorOutput = pAdapter->GetOutput(videoSettings.m_OutputIndex);

	int iDisplayModes = pMonitorOutput->GetModeCount();
	grcDisplayWindow oDisplayWindow;
	for (int mode = 0; mode < iDisplayModes; mode++)
	{
		pMonitorOutput->GetMode(&oDisplayWindow, mode);
		if (oDisplayWindow.uHeight < oDisplayWindow.uWidth)
			return true;
	}
	return false;
}
#endif //RSG_PC

CSettingsManager::CSettingsManager()
{
	m_NewSettingsRequested = false;
	m_AdapterOutputChanged = false;
#if RSG_PC
	m_IsLowQualitySystem = false;
#if !__FINAL
	m_initialised = false;
#endif
#endif
}

#if RSG_PC
eDXLevelSupported CSettingsManager::HighestDXVersionSupported()
{
	u32 dxFeatureLevel = GRCDEVICE.GetDXFeatureLevelSupported();
	switch (dxFeatureLevel)
	{
	case 1100:
		return DXS_11;
		break;
	case 1010:
		return DXS_10_1;
		break;
	case 1000:
		return DXS_10;
		break;
	default:
		Assertf(false, "Unknown DX Feature level encountered in Settings Manager");
		return DXS_10;
		break;
	}
}

float CSettingsManager::CalcFinalLodRootScale()
{
	return SettingsDefaults::CalcFinalLodRootScale(m_settings);
}

void CSettingsManager::VerifyDXVerion()
{
	static bool hasBeenDone = false;
	if (!hasBeenDone)
	{
		u32 dxFeatureLevel = GRCDEVICE.GetDxFeatureLevel();
		if (dxFeatureLevel)
		{
			switch (dxFeatureLevel)
			{
			case 1100:
				m_settings.m_graphics.m_DX_Version = 2;
				break;
			case 1010:
				m_settings.m_graphics.m_DX_Version = 1;
				break;
			case 1000:
				m_settings.m_graphics.m_DX_Version = 0;
				break;
			default:
				m_settings.m_graphics.m_DX_Version = 0;
				Assertf(false, "Unknown DX Feature level encountered in Settings Manager");
				break;
			}
			hasBeenDone = true;
		}
	}
}

Settings CSettingsManager::GetUISettings()
{
	Settings internalSettings = GetSettings();

	if (!GRCDEVICE.IsWindowed())
	{
		internalSettings.m_video.m_Windowed = 0;
	}
	else
	{
		LONG_PTR result = GetWindowLongPtr( g_hwndMain, GWL_STYLE );
		if (result & WS_POPUP)
		{
			internalSettings.m_video.m_Windowed = 2;
		}
		else
		{
			internalSettings.m_video.m_Windowed = 1;
		}
	}

// 	internalSettings.m_video.m_ScreenWidth = GRCDEVICE.GetWidth();
// 	internalSettings.m_video.m_ScreenHeight = GRCDEVICE.GetHeight();

	return internalSettings;
}
#endif //RSG_PC

const Settings& CSettingsManager::GetSettings()
{
#if RSG_PC
	VerifyDXVerion();
#endif
	return m_settings;
}

#if __BANK

void CSettingsManager::CreateWidgets()
{
	const char* bankName = "SettingsManager";

	if (BANKMGR.FindBank(bankName))
		return;

	bkBank& bank = BANKMGR.CreateBank(bankName);

	RagSliders& ragSliders = GetRagData();
	static const char* settingNames[] =
	{
		"Low",
		"Medium",
		"High",
		"Ultra",
		"Custom"
	};
#if RSG_PC
	static const char* aspectRatioNames[] =
	{
		"Auto",
		"4:3",
		"5:4",
		"16:9",
		"16:10"
	};
#endif

#if RSG_PC
	ragSliders.settings = SettingsDefaults::GetInstance().GetDefault();
#else
	ragSliders.settings = GetInstance().m_settings;
#endif
	ragSliders.editMode = false;

	// These are in a nested struct to avoid polluting the game namespace
	// with what is essentially little-used debugging code
	struct Handlers
	{
		static void updateSysValue() {}
		static void updateAudValue() {}
	};
#if RSG_PC
	bank.PushGroup("Window Resizing", false);
	bank.AddSlider("Maximum Aspect Ratio", &grcDevice::sm_maxAspectRatio, 0.0f, 5.0f, 0.1f);
	bank.AddSlider("Minimum Aspect Ratio", &grcDevice::sm_minAspectRatio, 0.0f, 5.0f, 0.1f);
	bank.PopGroup();
#endif
	bank.PushGroup("User Graphics Settings", false);
	
	CGraphicsSettings& gfx = ragSliders.settings.m_graphics;

	bank.AddCombo("Tessellation",           (int*)&gfx.m_Tessellation, 5, settingNames);
	bank.AddSlider("LOD scale",             &gfx.m_LodScale, 0.0f, 10.0f, 0.1f);
	bank.AddSlider("Ped LOD bias",			&gfx.m_PedLodBias, -0.99f, 3.0f, 0.1f);
	bank.AddSlider("Vehicle LOD bias",		&gfx.m_VehicleLodBias, -0.99f, 3.0f, 0.1f);
	bank.AddCombo ("Shadow Quality",        (int*)&gfx.m_ShadowQuality, 5, settingNames);
	bank.AddCombo ("Reflection Quality",    (int*)&gfx.m_ReflectionQuality, 5, settingNames);
	bank.AddSlider("Reflection MSAA",		&gfx.m_ReflectionMSAA, 0, 8, 1);
	bank.AddSlider("City Density",		    &gfx.m_CityDensity, 0.1f, 1.0f, 0.1f);
	bank.AddSlider("Ped Variety Mutliplier",&gfx.m_PedVarietyMultiplier , 0.0f, 1.0f, 0.1f);
	bank.AddSlider("Vehicle Variety Mutliplier",&gfx.m_VehicleVarietyMultiplier, 0.0f, 1.0f, 0.1f);
	bank.AddToggle("Allow HD when flying",	&gfx.m_HdStreamingInFlight);
	bank.AddSlider("LodScale Multiplier",	&gfx.m_MaxLodScale,0.0f,1.0f,0.1f);

	bank.AddSlider("MotionBlur Strength",	&gfx.m_MotionBlurStrength, 0.0f, 1.0f, 0.1f);
	bank.AddCombo ("SSAO",					(int*)&gfx.m_SSAO, 5, settingNames);
	bank.AddSlider("Anisotropic Filtering", &gfx.m_AnisotropicFiltering, 0, 16, 1);
	bank.AddSlider("MSAA",                  &gfx.m_MSAA, 0, 32, 1);
	bank.AddSlider("MSAA Fragments",        &gfx.m_MSAAFragments, 0, 32, 1);
	bank.AddSlider("MSAA Quality",          &gfx.m_MSAAQuality, 0, 32, 1);
	bank.AddSlider("Sampling Mode",			&gfx.m_SamplingMode, 0, 9, 1);

	bank.AddToggle ("FXAA",                 &gfx.m_FXAA_Enabled);
	bank.AddToggle ("TXAA",					&gfx.m_TXAA_Enabled);
	bank.AddCombo ("Texture Quality",       (int*)&gfx.m_TextureQuality, 5, settingNames);
	bank.AddCombo ("Particle Quality",      (int*)&gfx.m_ParticleQuality, 5, settingNames);
	bank.AddCombo ("Water Quality",         (int*)&gfx.m_WaterQuality, 5, settingNames);
	bank.AddCombo ("Grass Quality",			(int*)&gfx.m_GrassQuality, 5, settingNames);

	bank.AddCombo ("Shader Quality",        (int*)&gfx.m_ShaderQuality, 5, settingNames);
	bank.AddCombo("Soft Shadows",			(int*)&gfx.m_Shadow_SoftShadows,  5, settingNames);

	bank.PopGroup();
	bank.PushGroup("User System Settings", false);

#if GTA_REPLAY
	CSystemSettings& sys = ragSliders.settings.m_system;

	if(sys.m_numBytesPerReplayBlock < MIN_BYTES_PER_REPLAY_BLOCK || sys.m_numBytesPerReplayBlock > MAX_BYTES_PER_REPLAY_BLOCK)
		sys.m_numBytesPerReplayBlock = MIN_BYTES_PER_REPLAY_BLOCK;
	if(sys.m_numReplayBlocks < MIN_NUM_REPLAY_BLOCKS || sys.m_numReplayBlocks > MAX_NUM_REPLAY_BLOCKS)
		sys.m_numReplayBlocks = MIN_NUM_REPLAY_BLOCKS;

	//bank.AddSlider("Bytes Per Replay Block", &sys.m_numBytesPerReplayBlock,		MIN_BYTES_PER_REPLAY_BLOCK, MAX_BYTES_PER_REPLAY_BLOCK, 1000000,	Handlers::updateSysValue);
	bank.AddSlider("Number of Replay Blocks", &sys.m_numReplayBlocks,			MIN_NUM_REPLAY_BLOCKS,		MAX_NUM_REPLAY_BLOCKS,		1,			Handlers::updateSysValue);

	//bank.AddSlider("Max Size of Replay Stream", &sys.m_maxSizeOfStreamingReplay, MIN_REPLAY_STREAMING_SIZE, MAX_REPLAY_STREAMING_SIZE, 500, Handlers::updateSysValue);
#endif // GTA_REPLAY

	bank.PopGroup();
	bank.PushGroup("User Audio Settings", false);

	CAudioSettings& aud = ragSliders.settings.m_audio;

	bank.AddToggle("3D audio", &aud.m_Audio3d, Handlers::updateAudValue);
	
	bank.PopGroup();
#if RSG_PC
	bank.PushGroup("User Video Settings", false);

	CVideoSettings& video = ragSliders.settings.m_video;

	bank.AddSlider("Adapter number", &video.m_AdapterIndex, 0, 8, 1);
	bank.AddSlider("Output number", &video.m_OutputIndex, 0, 32, 1);
	bank.AddSlider("Screen width", &video.m_ScreenWidth, 0, 16384, 8);
	bank.AddSlider("Screen height", &video.m_ScreenHeight, 0, 16384, 8);
	bank.AddSlider("Refresh rate", &video.m_RefreshRate, 0, 250, 1);
	bank.AddSlider("Windowed", &video.m_Windowed, 0, 2, 1);
	bank.AddSlider("VSync", &video.m_VSync, 0, 2, 1);
	bank.AddCombo ("AspectRatio", (int*)&video.m_AspectRatio, 5, aspectRatioNames);

	bank.PopGroup();
#endif
	bank.PushGroup("Advanced (XML-only) Settings", false, "These settings will be driven by the above sliders.");

	bank.AddSeparator();
	bank.AddTitle("Shadow Quality");
	bank.AddToggle("Particle Shadows",		 &gfx.m_Shadow_ParticleShadows);
	bank.AddSlider("Shadow Distance",        &gfx.m_Shadow_Distance, 1, 10, 0.5f);
	bank.AddToggle("Long Shadows",			 &gfx.m_Shadow_LongShadows);
	bank.AddSlider("Shadow Split Z Start",   &gfx.m_Shadow_SplitZStart, 0, 5, 0.05f);
	bank.AddSlider("Shadow Split Z End",     &gfx.m_Shadow_SplitZEnd, 0, 5, 0.05f);
	bank.AddSlider("Aircraft Exp Weight",    &gfx.m_Shadow_aircraftExpWeight, 0, 5, 0.05f);
	bank.AddToggle("Disable Small Object check", &gfx.m_Shadow_DisableScreenSizeCheck);
	bank.AddSeparator();
	bank.AddTitle("Reflection Quality");
	bank.AddToggle("Standard Mip Blur",		 &gfx.m_Reflection_MipBlur);
	bank.AddSeparator();
	bank.AddTitle("FXAA Quality");
	bank.AddToggle("FXAA Enabled",			 &gfx.m_FXAA_Enabled);
	bank.AddSeparator();
	bank.AddTitle("TXAA Quality");
	bank.AddToggle("TXAA Enabled",			 &gfx.m_TXAA_Enabled);
	bank.AddSeparator();
	bank.AddTitle("Texture Quality");
	bank.AddSeparator();
	bank.AddTitle("Particle Quality");
	bank.AddSeparator();
	bank.AddTitle("Water Quality");
	bank.AddSeparator();
	bank.AddTitle("Lighting Quality");
	bank.AddToggle("Lighting_FogVolumes",    &gfx.m_Lighting_FogVolumes);
	bank.AddSeparator();
	bank.AddTitle("Shader Quality");
	bank.AddToggle("Sub Sample Alpha",		 &gfx.m_Shader_SSA);

	bank.PopGroup();

	bank.AddToggle("Edit Mode", &ragSliders.editMode);
	bank.AddButton("Apply", ApplyRagSettings);
	bank.AddButton("Save and Apply Settings", SaveAndApplyRagSettings);
	bank.AddButton("Reset", UpdateRagData);
	bank.AddButton("Reload From Disk", ForceReload);
}

void CSettingsManager::ApplyRagSettings(bool save)
{
	RagSliders& sliders = GetRagData();

	GetInstance().RequestNewSettings(sliders.settings);
	if (save) GetInstance().Save(sliders.settings);
}

void CSettingsManager::ApplyRagSettings()
{
	ApplyRagSettings(false);
}

void CSettingsManager::SaveAndApplyRagSettings()
{
	ApplyRagSettings(true);
}

CSettingsManager::RagSliders& CSettingsManager::GetRagData()
{
	static RagSliders sliders;
	return sliders;
}

void CSettingsManager::UpdateRagData()
{
	RagSliders& data = GetRagData();
	data.settings = GetInstance().m_settings;

	for (int i = 0; i < data.combos.GetCount(); i++)
		data.combos[i]->SetValue(data.combos[i]->GetValue());
}

void CSettingsManager::ForceReload()
{
	GetInstance().Load();
}
#endif // __BANK

#if RSG_PC
void CSettingsManager::GraphicsConfigSource(char* &info) const {
	char infostr[50];

	strcpy(infostr, "Graphics Config: ");

	switch(m_settings.m_configSource)
	{
		case SMC_AUTO:
		{
#if RSG_PC
			strcat(infostr, "AUTO");
			if (!(m_settings.m_graphics == SettingsDefaults::GetInstance().GetDefaultGraphics()))
			{
				strcat(infostr, " [MODIFIED]");
			}
#endif
			break;
		}
		default:
		{
			strcat(infostr, "UNKNOWN");
			break;
		}
	}

	size_t len = strlen(infostr);
	info = rage_new char[len + 1];
	strcpy(info, infostr);
}
#endif // RSG_PC

bool CSettingsManager::DoNewSettingsRequireRestart(const Settings& settings)
{
#if RSG_PC
	if (settings.m_video.m_AdapterIndex != m_OriginalAdapter && !settings.m_video.m_Windowed) return true;
#endif
	return settings.NeedsGameRestartComparedTo(m_settings);
}

bool CSettingsManager::DoNewSettingsRequiresDeviceReset(const Settings& settings)
{
	return settings.NeedsDeviceResetComparedTo(m_settings);
}

void CSettingsManager::RequestNewSettings(const Settings settings)
{
#if RSG_PC
	m_settingsOnReset = settings;
	m_NewSettingsRequested = true;

	m_PreviousSettings = m_settings;
	if (GRCDEVICE.IsWindowDragResized())
	{
		m_PreviousSettings.m_video.m_ScreenWidth = GRCDEVICE.GetWidth();
		m_PreviousSettings.m_video.m_ScreenHeight = GRCDEVICE.GetHeight();
	}

	GRCDEVICE.SetWindowDragResized(!IsSupportedMonitorResoluion(settings.m_video));

	bool ignoreWindowLimits = false;
	if (GRCDEVICE.IsWindowDragResized()) ignoreWindowLimits = true;
	if (m_WidthHeightOverride && settings.m_video.m_ScreenWidth == (int)m_CommandLineWindow.uWidth && settings.m_video.m_ScreenHeight == (int)m_CommandLineWindow.uHeight) ignoreWindowLimits = true;
	if (m_MultiMonitorAvailable && settings.m_video.m_ScreenWidth == (int)m_MultiMonitorWindow.uWidth && settings.m_video.m_ScreenHeight == (int)m_MultiMonitorWindow.uHeight) ignoreWindowLimits = true;
	GRCDEVICE.SetIngoreWindowLimits(ignoreWindowLimits);
#else
	m_settings = settings;
	ApplySettings();
#endif
}

#if RSG_PC
void CSettingsManager::SafeUpdate()
{
	if (m_NewSettingsRequested)
	{
		bool bCenterWindow = ((m_settingsOnReset.m_video.m_Windowed != m_settings.m_video.m_Windowed) ||
			(m_settingsOnReset.m_video.m_AdapterIndex != m_settings.m_video.m_AdapterIndex) || 
			(m_settingsOnReset.m_video.m_OutputIndex != m_settings.m_video.m_OutputIndex) || 
			(m_settingsOnReset.m_video.m_ScreenHeight != m_settings.m_video.m_ScreenHeight) || 
			(m_settingsOnReset.m_video.m_ScreenWidth != m_settings.m_video.m_ScreenWidth));

		ApplyVideoSettings(m_settingsOnReset.m_video, bCenterWindow || m_AdapterOutputChanged, DoNewSettingsRequiresDeviceReset(m_settingsOnReset) || m_AdapterOutputChanged);
		m_NewSettingsRequested = false;
		m_AdapterOutputChanged = false;
	}
}
#endif

void CSettingsManager::ApplyLoadedSettings()
{
	ApplySettings();
#if RSG_PC
	m_settingsOnReset = m_settings;
	ApplyVideoSettings(m_settings.m_video, false, false, true);
#endif
}

bool CGraphicsSettings::AreVideoMemorySettingsSame(CGraphicsSettings& settings) const
{
	return	m_LodScale == settings.m_LodScale &&
			m_PedLodBias == settings.m_PedLodBias &&
			m_VehicleLodBias == settings.m_VehicleLodBias &&
			m_ShadowQuality == settings.m_ShadowQuality &&
			m_ReflectionQuality == settings.m_ReflectionQuality &&
			m_ReflectionMSAA == settings.m_ReflectionMSAA &&
			m_SSAO == settings.m_SSAO &&
			m_MSAA == settings.m_MSAA &&
			m_MSAAFragments == settings.m_MSAAFragments &&
			m_MSAAQuality == settings.m_MSAAQuality &&
			m_SamplingMode == settings.m_SamplingMode && 
			m_TextureQuality == settings.m_TextureQuality &&
			m_ParticleQuality == settings.m_ParticleQuality &&
			m_WaterQuality == settings.m_WaterQuality &&
			m_GrassQuality == settings.m_GrassQuality &&
			m_ShaderQuality == settings.m_ShaderQuality &&
			m_Shadow_SoftShadows == settings.m_Shadow_SoftShadows &&
			m_UltraShadows_Enabled == settings.m_UltraShadows_Enabled &&
			m_Shadow_ParticleShadows == settings.m_Shadow_ParticleShadows &&
			m_Shadow_Distance == settings.m_Shadow_Distance &&
			m_Shadow_LongShadows == settings.m_Shadow_LongShadows &&
			m_Shadow_SplitZStart == settings.m_Shadow_SplitZStart &&
			m_Shadow_SplitZEnd == settings.m_Shadow_SplitZEnd &&
			m_Shadow_aircraftExpWeight == settings.m_Shadow_aircraftExpWeight &&
			m_Shadow_DisableScreenSizeCheck == settings.m_Shadow_DisableScreenSizeCheck &&
			m_Reflection_MipBlur == settings.m_Reflection_MipBlur &&
			m_TXAA_Enabled == settings.m_TXAA_Enabled &&
			m_Lighting_FogVolumes == settings.m_Lighting_FogVolumes &&
			m_Shader_SSA == settings.m_Shader_SSA &&
			m_PedVarietyMultiplier == settings.m_PedVarietyMultiplier &&
			m_VehicleVarietyMultiplier == settings.m_VehicleVarietyMultiplier &&
			m_DX_Version == settings.m_DX_Version &&
			m_PostFX == settings.m_PostFX &&
			m_DoF == settings.m_DoF &&
			m_HdStreamingInFlight == settings.m_HdStreamingInFlight &&
			m_MaxLodScale == settings.m_MaxLodScale;
}

bool CGraphicsSettings::NeedsDeviceResetComparedTo(CGraphicsSettings& settings) const
{
	return (settings.m_ShadowQuality != m_ShadowQuality) || 
		(settings.m_ReflectionQuality != m_ReflectionQuality) || 
		(settings.m_ReflectionMSAA != m_ReflectionMSAA) ||
		(settings.m_WaterQuality != m_WaterQuality) || 
		(settings.m_Shadow_SoftShadows != m_Shadow_SoftShadows) ||
		(settings.m_MSAA != m_MSAA) ||
		(settings.m_TXAA_Enabled != m_TXAA_Enabled) || 
		(settings.m_MSAAFragments != m_MSAAFragments) ||
		(settings.m_ReflectionMSAA != m_ReflectionMSAA) ||
		(settings.m_SamplingMode != m_SamplingMode) ||
		(settings.m_UltraShadows_Enabled != m_UltraShadows_Enabled) ||
		(settings.m_PostFX != m_PostFX) ||
		(settings.m_SSAO != m_SSAO);
}

bool CGraphicsSettings::NeedsGameRestartComparedTo(CGraphicsSettings& settings) const
{
	return (settings.m_DX_Version	!= m_DX_Version) || 
		(settings.m_ShaderQuality	!= m_ShaderQuality) || 
		(settings.m_TextureQuality	!= m_TextureQuality)  || 
		(settings.m_ParticleQuality	!= m_ParticleQuality) ||
		(settings.m_GrassQuality	!= m_GrassQuality);
}

bool CVideoSettings::AreVideoMemorySettingsSame(CVideoSettings& settings) const
{
	return	m_ScreenWidth == settings.m_ScreenWidth &&
			m_ScreenHeight == settings.m_ScreenHeight &&
			m_Stereo == settings.m_Stereo;
}

bool CVideoSettings::NeedsDeviceResetComparedTo(CVideoSettings& settings) const
{
	return (settings.m_RefreshRate != m_RefreshRate) || 
		(settings.m_AspectRatio != m_AspectRatio) ||
		(settings.m_Windowed != m_Windowed) || 
		(settings.m_OutputIndex != m_OutputIndex) || 
		(settings.m_AdapterIndex != m_AdapterIndex) || 
		(settings.m_VSync != m_VSync) || 
		(settings.m_ScreenHeight != m_ScreenHeight) || 
		(settings.m_ScreenWidth != m_ScreenWidth);
}

bool CVideoSettings::NeedsGameRestartComparedTo(CVideoSettings& settings) const
{
	return settings.m_Stereo != m_Stereo || (settings.m_Stereo && m_Windowed);
}

void CVideoSettings::PrintOutSettings() const
{
#if !__NO_OUTPUT
	for (int i = 0; parser_CVideoSettings__MemberNames[i] != NULL; i++)
	{
		rage::parMemberSimpleData* data = (rage::parMemberSimpleData*)parser_CVideoSettings__Members[i];
		const char* name = parser_CVideoSettings__MemberNames[i];
		void* value = (void*)(((size_t)this) + ((size_t)parser_MemberOffsets[i]));
		if (data->m_Type == rage::parMemberType::TYPE_BOOL)
			Displayf("[settings] %s: %s", name, (*((bool*)value)) ? "true" : "false");
		else if (data->m_Type == rage::parMemberType::TYPE_INT)
			Displayf("[settings] %s: %d", name, *((int*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_UINT)
			Displayf("[settings] %s: %u", name, *((int*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_FLOAT)
			Displayf("[settings] %s: %f", name, *((float*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_ENUM)
		{
			if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_32BIT)
				Displayf("[settings] %s: %u", name, *((int*)value));
			else if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_16BIT)
				Displayf("[settings] %s: %u", name, (int)*((short*)value));
			else if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_8BIT)
				Displayf("[settings] %s: %u", name, (int)*((char*)value));
		}
	}
#endif // !__NO_OUTPUT
}

void CGraphicsSettings::PrintOutSettings() const
{
#if !__NO_OUTPUT
	for (int i = 0; parser_CGraphicsSettings__MemberNames[i] != NULL; i++)
	{
		rage::parMemberSimpleData* data = (rage::parMemberSimpleData*)parser_CGraphicsSettings__Members[i];
		const char* name = parser_CGraphicsSettings__MemberNames[i];
		void* value = (void*)(((size_t)this) + ((size_t)parser_MemberOffsets[i]));
		if (data->m_Type == rage::parMemberType::TYPE_BOOL)
			Displayf("[settings] %s: %s", name, (*((bool*)value)) ? "true" : "false");
		else if (data->m_Type == rage::parMemberType::TYPE_INT)
			Displayf("[settings] %s: %d", name, *((int*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_UINT)
			Displayf("[settings] %s: %u", name, *((int*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_FLOAT)
			Displayf("[settings] %s: %f", name, *((float*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_ENUM)
		{
			if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_32BIT)
				Displayf("[settings] %s: %u", name, *((int*)value));
			else if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_16BIT)
				Displayf("[settings] %s: %u", name, (int)*((short*)value));
			else if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_8BIT)
				Displayf("[settings] %s: %u", name, (int)*((char*)value));
		}
	}
#endif // !__NO_OUTPUT
}

void CGraphicsSettings::OutputSettings(fiStream * outHandle) const
{
	for (int i = 0; parser_CGraphicsSettings__MemberNames[i] != NULL; i++)
	{
		rage::parMemberSimpleData* data = (rage::parMemberSimpleData*)parser_CGraphicsSettings__Members[i];
		const char* name = parser_CGraphicsSettings__MemberNames[i];
		void* value = (void*)(((size_t)this) + ((size_t)parser_MemberOffsets[i]));
		if (data->m_Type == rage::parMemberType::TYPE_BOOL)
			fprintf(outHandle,"%s: %s\n", name, (*((bool*)value)) ? "true" : "false");
		else if (data->m_Type == rage::parMemberType::TYPE_INT)
			fprintf(outHandle,"%s: %d\n", name, *((int*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_UINT)
			fprintf(outHandle,"%s: %u\n", name, *((int*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_FLOAT)
			fprintf(outHandle,"%s: %f\n", name, *((float*)value));
		else if (data->m_Type == rage::parMemberType::TYPE_ENUM)
		{
			if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_32BIT)
				fprintf(outHandle,"%s: %u\n", name, *((int*)value));
			else if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_16BIT)
				fprintf(outHandle,"%s: %u\n", name, (int)*((short*)value));
			else if (data->m_Subtype == parMemberEnumSubType::SUBTYPE_8BIT)
				fprintf(outHandle,"%s: %u\n", name, (int)*((char*)value));
		}
	}
}

void CSettingsManager::PrintOutSettings(const Settings& settings)
{
	Displayf("[settings] Using Settings:");
	settings.m_graphics.PrintOutSettings();
	settings.m_video.PrintOutSettings();
}

#if __DEV && RSG_PC
bool CSettingsManager::sm_ResetTestsActive = true;

void CSettingsManager::ProcessTests()
{
	if (!sm_ResetTestsActive)
		return;

	if (!PARAM_testDeviceReset.Get())
		return;

	static s32 timeTillNextReset = 15000;

	static bool performingDeviceReset = false;

	static fiStream* calculatedSizeStream = NULL;
	static fiStream* actualSizeStream = NULL;
	static fiStream* deviceResetLogStream = NULL;

	static int testNumber = 0;

	//Initialize file streams (should only run the first time)
	if (!deviceResetLogStream)
	{
		char defaultPathname[] = "x:/";
		const char* logPathname = defaultPathname;
		PARAM_deviceResetLogPath.Get(logPathname);
		ASSET.PushFolder(logPathname);

		char resetFilename[] = "DeviceResetLog";
		deviceResetLogStream = ASSET.Create(resetFilename, "txt");
		ASSET.PopFolder();
	}

	//If currently doing device reset test check if it has completed, and if so close up log files.
	if (performingDeviceReset && !GRCDEVICE.GetDeviceResetTestActive())
	{
		GRCDEVICE.StopLoggingResets();
		char endOfResetMessage[40] = "";
		formatf(endOfResetMessage, 40, "\r\nEnd of test %d\r\n\r\n", testNumber);
		deviceResetLogStream->Write(endOfResetMessage, (int)strlen(endOfResetMessage));
		deviceResetLogStream->Flush();

		if (PARAM_dumpRenderTargetSizes.Get())
		{
			grcTextureFactoryDX11::StopLogging();
			actualSizeStream->Flush();
			actualSizeStream->Close();
			actualSizeStream = NULL;


			char defaultPathname[] = "x:/";
			const char* logPathname = defaultPathname;
			PARAM_deviceResetLogPath.Get(logPathname);
			ASSET.PushFolder(logPathname);

			char calculatedFilename[200] = "";
			formatf(calculatedFilename, 200, "CalculatedRenderTargetSizes%d", testNumber);
//			ASSET.CreateLeadingPath(calculatedFilename);
			calculatedSizeStream = ASSET.Create(calculatedFilename, "txt");
			ASSET.PopFolder();

			SettingsDefaults &defaults = SettingsDefaults::GetInstance();
			SettingsDefaults::StartLogging(calculatedSizeStream);
			defaults.renderTargetMemSizeFor(CSettingsManager::GetInstance().GetSettings());
			SettingsDefaults::StopLogging();
			calculatedSizeStream->Flush();
			calculatedSizeStream->Close();
			calculatedSizeStream = NULL;
		}

		timeTillNextReset = 10000;
		performingDeviceReset = false;
	}

	//Exit out if not enough time has elapsed till next reset
	if (timeTillNextReset > 0)
	{
		timeTillNextReset -= fwTimer::GetTimeStepInMilliseconds();
		return;
	}

	//if busy performing a test then exit
	if (performingDeviceReset) return;

	//Set all variables as test active, activate the log files for the test
	performingDeviceReset = true;
	GRCDEVICE.SetDeviceResetTestActive(true);

	Settings newSettings;
	switch (++testNumber)
	{
	case 1:
		newSettings = CSettingsManager::GetInstance().GetUISettings();
		newSettings.m_video.m_ScreenHeight = 800;
		newSettings.m_video.m_ScreenWidth = 1280;
		newSettings.m_video.m_Windowed = 1;
		break;
	case 2:
		newSettings = CSettingsManager::GetInstance().GetUISettings();
		newSettings.m_video.m_ScreenHeight = 720;
		newSettings.m_video.m_ScreenWidth = 1280;
		newSettings.m_video.m_Windowed = 2;
		break;
	case 3:
		newSettings = CSettingsManager::GetInstance().GetUISettings();
		newSettings.m_video.m_ScreenHeight = 1200;
		newSettings.m_video.m_ScreenWidth = 1920;
		newSettings.m_video.m_Windowed = 0;
		break;
	case 4:
		newSettings = CSettingsManager::GetInstance().GetUISettings();
		newSettings.m_video.m_ScreenHeight = 720;
		newSettings.m_video.m_ScreenWidth = 1280;
		newSettings.m_video.m_Windowed = 0;
		break;
	case 5:
		newSettings = CSettingsManager::GetInstance().GetUISettings();
		newSettings.m_video.m_ScreenHeight = 900;
		newSettings.m_video.m_ScreenWidth = 1200;
		newSettings.m_video.m_Windowed = 1;
		break;
	case 6:
		char endOfTestMessage[] = "\r\n\r\nTest successfully completed";
		deviceResetLogStream->Write(endOfTestMessage, (int)strlen(endOfTestMessage));
		//TestDone, close the log files
		if (calculatedSizeStream) {calculatedSizeStream->Close(); calculatedSizeStream = NULL;}
		if (actualSizeStream) {actualSizeStream->Close(); actualSizeStream = NULL;}
		if (deviceResetLogStream) {deviceResetLogStream->Close(); deviceResetLogStream = NULL;}
		performingDeviceReset = false;
		sm_ResetTestsActive = false;
		break;
	}

	//Call the apply settings with whatever the new tests values are
	if (sm_ResetTestsActive)
	{
		GRCDEVICE.StartLoggingResets(deviceResetLogStream);
		if (PARAM_dumpRenderTargetSizes.Get())
		{
			char defaultPathname[] = "x:/";
			const char* logPathname = defaultPathname;
			PARAM_deviceResetLogPath.Get(logPathname);
			ASSET.PushFolder(logPathname);

			char actualFilename[40] = "";
			formatf(actualFilename, 40, "ActualRenderTargetSizes%d", testNumber);
			ASSET.CreateLeadingPath(actualFilename);
			actualSizeStream = ASSET.Create(actualFilename, "txt");
			ASSET.PopFolder();

			grcTextureFactoryDX11::StartLogging(actualSizeStream);
		}

		CSettingsManager::GetInstance().RequestNewSettings(newSettings);
	}
}

bool CSettingsManager::ResetTestsCompleted() {
	if (PARAM_testDeviceReset.Get())
	{
		return !sm_ResetTestsActive;
	}
	return false;
}
#endif

#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS
