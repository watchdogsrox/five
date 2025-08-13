/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodScale.cpp
// PURPOSE : manages all lod scales
// AUTHOR :  Ian Kiigan
// CREATED : 07/03/13
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/lod/LodScale.h"

#include "camera/CamInterface.h"
#include "camera/cinematic/camera/tracking/CinematicHeliChaseCamera.h"
#include "camera/cinematic/camera/tracking/CinematicStuntCamera.h"
#include "camera/gameplay/GameplayDirector.h"
#include "camera/viewports/ViewportManager.h"
#include "cutscene/CutSceneCameraEntity.h"
#include "cutscene/CutSceneManagerNew.h"
#include "fwrenderer/RenderPhaseBase.h"
#include "fwsys/timer.h"
#include "network/NetworkInterface.h"
#include "renderer/Lights/Lights.h"
#include "renderer/Renderer.h"
#include "renderer/RenderPhases/RenderPhaseDefLight.h"
#include "scene/lod/LodDebug.h"
#include "scene/lod/LodMgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "streaming/streamingrequestlist.h"
#include "system/param.h"
#include "timecycle/TimeCycle.h"
#include "vehicles/vehiclepopulation.h"

#if RSG_PC
#include "system/SettingsDefaults.h"
#endif

NOSTRIP_PARAM(normalaiminglodscale, "[LodScale.cpp] Permit normal camera fov driven LOD scale when aiming");

PARAM(globalLOD, "[LodScale.cpp] Override global root LOD scale");

#define SNIPER_GRASS_SCALE (1.5f)

CLodScale g_LodScale;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CLodScale
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
CLodScale::CLodScale()
: m_fGrassQualityScale(1.0f)
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Init
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodScale::Init()
{
	// input scales
#if RSG_PC
	const float rootScale = CSettingsManager::GetInstance().CalcFinalLodRootScale();
	SetRootScale(rootScale);
#else
	m_fRoot_In = 1.0f;
#endif

	m_fMarketing_In = 1.0f;
	m_fScript_In = 1.0f;
	m_fTimeCycleGlobal_In = 1.0f;
	m_fGrass_In = 1.0f;
	//Don't set m_fGrassQualityScale here in case the quality scale has already been updated. (Initialized in constructor for this reason.)
	for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		m_afPerLodLevel_In[i] = 1.0f;
	}

	// output scales
	m_fGlobal_Out = 1.0f;
	m_fGlobalIgnoreSettings_Out = 1.0f;
	m_fGlobalRT_Out = 1.0f;
	m_fGrassRT_Out = 1.0f;
	m_fGrass_Out = 1.0f;
	for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		m_afPerLodLevel_Out[i] = 1.0f;
	}

	// state
	m_bScriptOverrideThisFrame = false;
	m_bScriptOverride = false;
	m_bBlendAfterOverride = false;
	m_blendOutDuration = 10.0f;
	m_blendOutCurrentTime = 0.0f;
	m_blendOutStart = 0.0f;

	// override root lod based on command line
	if (PARAM_globalLOD.Get())
	{
		float fLod = 1.0f;
		PARAM_globalLOD.Get(fLod);
		m_fRoot_In = Min(Max(0.5f, fLod), 10.0f);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		determine the output lod scale values to be used this frame
//				based on many different inputs
//////////////////////////////////////////////////////////////////////////
void CLodScale::Update()
{
#if RSG_PC
	const float fPreviousLodScale = m_fGlobal_Out;
#endif

	float fCameraScale = 1.0f;
	float fGlobalScale = 1.0f;

	CalculateGlobalScale(fGlobalScale, fCameraScale);
	GetTimeCycleValues();
	GetGrassBatchValues();

#if RSG_PC
	ProcessOverrides(fGlobalScale, fCameraScale, fPreviousLodScale);
#else
	ProcessOverrides(fGlobalScale, fCameraScale);
#endif

	BlendOutAfterOverride(fGlobalScale);

#if __BANK
	CLodScaleTweaker::AdjustScales();	// last chance to modify scales!!
#endif	//__BANK

	HardenAllOutputScales(fGlobalScale);
	PostUpdate(fGlobalScale);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetRootScale
// PURPOSE:		the root scale is applied to all other scales. used for platform
//				specific uniform scaling, network specific behaviour etc
//////////////////////////////////////////////////////////////////////////
float CLodScale::GetRootScale()
{
	// during cutscenes we cannot observe platform-specific increases in root LOD scale
	// because it would invalidate cutscene SRLs.
	return ( gStreamingRequestList.IsPlaybackActive() ? Min(m_fRoot_In, 1.0f) : m_fRoot_In );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CalculateGlobalScale
// PURPOSE:		determine ideal lod scale based on camera fov and other root scale values
//////////////////////////////////////////////////////////////////////////
void CLodScale::CalculateGlobalScale(float& fGlobalScale, float& fCameraScale)
{
	fCameraScale = ((CRenderPhaseDeferredLighting_SceneToGBuffer *) g_SceneToGBufferPhaseNew)->ComputeLodDistanceScale();
	fGlobalScale = fCameraScale * GetRootScale();

#if __DEV
	fGlobalScale *= m_fMarketing_In;
#endif	//__DEV
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetTimeCycleValues
// PURPOSE:		retrieve all lod scale values desired by time cycle modifiers
//////////////////////////////////////////////////////////////////////////
void CLodScale::GetTimeCycleValues()
{
	// guard against a rare TC bug that screws up LOD scales!
	const bool bCanTrustTC =  g_timeCycle.GetLodMult() >= 0.2f;

	m_fTimeCycleGlobal_In							= ( bCanTrustTC ? g_timeCycle.GetLodMult() : 1.0f );
	m_afPerLodLevel_In[LODTYPES_DEPTH_HD]			= ( bCanTrustTC ? g_timeCycle.GetLodScale_Hd() : 1.0f );
	m_afPerLodLevel_In[LODTYPES_DEPTH_ORPHANHD]		= ( bCanTrustTC ? g_timeCycle.GetLodScale_OrphanHd() : 1.0f );
	m_afPerLodLevel_In[LODTYPES_DEPTH_LOD]			= ( bCanTrustTC ? g_timeCycle.GetLodScale_Lod() : 1.0f );
	m_afPerLodLevel_In[LODTYPES_DEPTH_SLOD1]		= ( bCanTrustTC ? g_timeCycle.GetLodScale_Slod1() : 1.0f );
	m_afPerLodLevel_In[LODTYPES_DEPTH_SLOD2]		= ( bCanTrustTC ? g_timeCycle.GetLodScale_Slod2() : 1.0f );
	m_afPerLodLevel_In[LODTYPES_DEPTH_SLOD3]		= ( bCanTrustTC ? g_timeCycle.GetLodScale_Slod3() : 1.0f );
	m_afPerLodLevel_In[LODTYPES_DEPTH_SLOD4]		= ( bCanTrustTC ? g_timeCycle.GetLodScale_Slod4() : 1.0f );

	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetGrassBatchValues
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodScale::GetGrassBatchValues()
{
	m_fGrass_In = (camInterface::GetGameplayDirector().IsFirstPersonAiming() ? SNIPER_GRASS_SCALE : 1.0f) * m_fGrassQualityScale;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ProcessOverrides
// PURPOSE:		allow any systems that need to override the global scale to do so
//////////////////////////////////////////////////////////////////////////
#if RSG_PC
void CLodScale::ProcessOverrides(float& fGlobalScale, const float fCameraScale, const float fPrevScale)
#else
void CLodScale::ProcessOverrides(float& fGlobalScale, const float fCameraScale)
#endif
{
	m_bScriptOverrideThisFrame = false;

	//////////////////////////////////////////////////////////////////////////
	// check if cutscene is controlling lod scales
	bool bCutSceneOverride = false;
	CutSceneManager* csMgr = CutSceneManager::GetInstance();
	Assert(csMgr);
	if( csMgr->IsCutscenePlayingBack() )
	{
		const CCutSceneCameraEntity* cam = csMgr->GetCamEntity();
		if( cam && cam->GetMapLodScale()!=-1)
		{
			bCutSceneOverride = true;
		}
	}

	// if normal game camera (ie not sniping etc) or a cutscene is controlling lod scales
	// then ignore the ideal camera-based lod scale and instead take the timecycle one
	if (fCameraScale==1.0f || bCutSceneOverride)
	{
		fGlobalScale = m_fTimeCycleGlobal_In * GetRootScale();
	}

	//////////////////////////////////////////////////////////////////////////
	// keep the lod scale under control for some specific cinematic cameras that have rapid changes in FOV
	const camBaseCamera* renderedCamera = camInterface::GetDominantRenderedCamera();
	const bool bStuntCamActive = (g_ContinuityMgr.IsPlayerInCity() && renderedCamera && renderedCamera->GetClassId()==camCinematicStuntCamera::GetStaticClassId());
	const bool bAircraftCinematicCam = ( g_ContinuityMgr.IsFlyingAircraft() && renderedCamera && renderedCamera->GetIsClassId(camCinematicHeliChaseCamera::GetStaticClassId()) );
	const bool bSkyDivingCinematicCam = ( g_ContinuityMgr.IsSkyDiving() && renderedCamera && renderedCamera->GetIsClassId(camCinematicHeliChaseCamera::GetStaticClassId()) );

	//////////////////////////////////////////////////////////////////////////
	// REMOVE THIS AFTER GTAV PC
	//
	// It was decided very late in the development of GTAV PC that we didn't like the LOD scale
	// to change with small FOV changes during aiming of weapons. This code detects that case
	// and overrides the scale accordingly
#if RSG_PC
	const float aimingLerpDurationMs = 2500.0;
	const float aimingCoolOffDurationMs = 2000.0f;
	static sysTimer s_aimingTimer;
	static sysTimer s_aimingCoolOffTimer;
	static bool s_bAiming = false;
	const bool bOnFootNormalAiming = ( g_ContinuityMgr.IsPlayerOnFoot() && !g_ContinuityMgr.IsUsingSniperOrTelescope() && g_ContinuityMgr.IsPlayerAiming() /*&& fCameraScale<=1.32*/);

	const bool bGameplayCameraActive = camInterface::IsDominantRenderedDirector(camInterface::GetGameplayDirector());

	if (bOnFootNormalAiming)
	{
		s_aimingCoolOffTimer.Reset();
	}
#endif	//RSG_PC
	
	if ( bStuntCamActive || bAircraftCinematicCam || bSkyDivingCinematicCam)
	{
		fGlobalScale = 1.0f * GetRootScale();
	}
#if RSG_PC
	else if ( bGameplayCameraActive
		&& !PARAM_normalaiminglodscale.Get()
		&& (bOnFootNormalAiming || s_aimingCoolOffTimer.GetMsTime()<aimingCoolOffDurationMs)
		BANK_ONLY(&& CLodScaleTweaker::ms_bEnableAimingScales) )
	{
		if (!s_bAiming)
		{
			s_aimingTimer.Reset();
		}
		s_bAiming = true;

		const float fMaxScale = Min(1.0f, m_fTimeCycleGlobal_In);	// modifier boxes can reduce the normal scale

		const float fTargetScale = Max( (0.68f * fCameraScale), fMaxScale );
		const float blendLevel = Min( 1.0f, (s_aimingTimer.GetMsTime() / aimingLerpDurationMs) );
		const float overrideScale = Lerp( blendLevel, fPrevScale, (fTargetScale * GetRootScale()) );

		fGlobalScale = overrideScale;
	}
	else
	{
		s_bAiming = false;
	}
#endif	//RSG_PC
	//////////////////////////////////////////////////////////////////////////
	

	//////////////////////////////////////////////////////////////////////////
	// process script override and remapping for this frame. override beats remapping
	if (m_bScriptRemappingThisFrame)
	{
		if (!m_bScriptOverride && !g_PlayerSwitch.IsActive())
		{
			CVehiclePopulation::SetRendererRangeMultiplierOverrideThisFrame(fGlobalScale);
		}

		const float fClampedCameraScale = Clamp(fCameraScale, m_fRemapOldMin, m_fRemapOldMax);
		const float fRange = Range(fClampedCameraScale, m_fRemapOldMin, m_fRemapOldMax);
		const float fRemappedScale = m_fRemapNewMin + fRange*(m_fRemapNewMax - m_fRemapNewMin);

		fGlobalScale = fRemappedScale * GetRootScale();

		m_bScriptRemappingThisFrame = false;
	}

	if (m_bScriptOverride)
	{
		fGlobalScale = m_fScript_In * GetRootScale();

		m_blendOutStart = m_fScript_In;
		m_bScriptOverrideThisFrame = true;
		m_bBlendAfterOverride = true;

		m_bScriptOverride = false;
	}

	//////////////////////////////////////////////////////////////////////////
	// player switch may need to modify the global lod scale
	if (g_PlayerSwitch.IsActive())
	{
		if (g_LodMgr.IsSlodModeActive())
		{
			fGlobalScale = 5.0f;	//TODO tidy this
		}
		else
		{
			g_PlayerSwitch.ModifyLodScale(fGlobalScale);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	BlendOutAfterOverride
// PURPOSE:		if script set the lod scale in a previous frame, interp back to ideal value
//////////////////////////////////////////////////////////////////////////
void CLodScale::BlendOutAfterOverride(float& fGlobalScale)
{
	if (!m_bScriptOverrideThisFrame && m_bBlendAfterOverride)
	{
		fGlobalScale = Lerp(m_blendOutCurrentTime / m_blendOutDuration, m_blendOutStart, fGlobalScale);
		m_blendOutCurrentTime += fwTimer::GetTimeStep();
		if (m_blendOutCurrentTime >= m_blendOutDuration)
		{
			m_blendOutStart = 0.0f;
			m_blendOutCurrentTime = 0.0f;
			m_bBlendAfterOverride = false;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	HardenAllOutputScales
// PURPOSE:		write back final scale values for this frame
//////////////////////////////////////////////////////////////////////////
void CLodScale::HardenAllOutputScales(const float fGlobalScale)
{
	m_fGlobal_Out = fGlobalScale;
	m_fGlobalIgnoreSettings_Out = (fGlobalScale / m_fRoot_In);

	for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
	{
		// store final pre-multiplied scales to avoid thousands of extra mults later
		m_afPerLodLevel_Out[i] = fGlobalScale * m_afPerLodLevel_In[i];
	}

	// eventually we should just move towards unique scales for all entity types and all lod levels.
	m_fGrass_Out = m_fGrass_In;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	PostUpdate
// PURPOSE:		update any cached copies of the global scale
//////////////////////////////////////////////////////////////////////////
void CLodScale::PostUpdate(const float fGlobalScale)
{
	// it would be better if these all went away
	const bool bDoingStaticCeilingShotsOfPlayerSwitch = ( g_PlayerSwitch.IsActive() && g_LodMgr.IsSlodModeActive() );
	Lights::SetUpdateLodDistScale( bDoingStaticCeilingShotsOfPlayerSwitch ? 1.0f : fGlobalScale );
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetGlobalScaleFromScript
// PURPOSE:		register a desired script override of global lod scale this frame only
//////////////////////////////////////////////////////////////////////////
void CLodScale::SetGlobalScaleFromScript(const float fGlobalScale)
{
	m_bScriptOverride = true;
	m_fScript_In = fGlobalScale;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetRemappingFromScript
// PURPOSE:		rather than setting an explicit overridden global lod scale this frame,
//				script can instead remap the camera-fov-based scale from the old range to the new range
//////////////////////////////////////////////////////////////////////////
void CLodScale::SetRemappingFromScript(const float fOldMin, const float fOldMax, const float fNewMin, const float fNewMax)
{
	m_bScriptRemappingThisFrame = true;
	m_fRemapOldMin = fOldMin;
	m_fRemapOldMax = fOldMax;
	m_fRemapNewMin = fNewMin;
	m_fRemapNewMax = fNewMax;
}
