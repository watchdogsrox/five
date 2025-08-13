///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxGPUSystems.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	23 November 2009
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "PtFxGPUSystems.h"

// rage 
#include "grmodel/shader.h"

// framework
#include "fwscene/stores/txdstore.h"
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"

// game
#include "Game/Weather.h"
#include "cutscene/CutSceneManagerNew.h"
#include "camera/viewports/ViewportManager.h"
#include "Peds/Ped.h"
#include "Renderer/RenderPhases/RenderPhaseHeightMap.h"
#include "Renderer/PostProcessFX.h"
#include "renderer/rendertargets.h"
#include "Timecycle/TimeCycleConfig.h"
#include "Vfx/Systems/VfxWeather.h"
#include "Vfx/GPUParticles/PtFxGPUManager.h"
#include "Vfx/VfxHelper.h"
#include "TimeCycle/TimeCycle.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxGPULitShaderVars
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Set
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPULitShaderVars::Set					(grmShader* pShader, float lightIntensityMult)
{
	pShader->SetVar(m_lightIntensityMultId, lightIntensityMult);
}


///////////////////////////////////////////////////////////////////////////////
//  SetupShader
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPULitShaderVars::SetupShader			(grmShader* pShader)
{
	m_lightIntensityMultId = pShader->LookupVar("LightIntensityMult");
}




///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxGPUSystem
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUSystem::Shutdown					()
{
	// unload the texture dictionaries
	for (u32 i=0; i<m_numSettings; i++)
	{
		// release the texture dictionary
		vfxUtil::ShutdownTxd("fxweather");
	}

	// reset the number of gpu fx settings
	m_numSettings = 0;

	delete m_pPtxGpu;
	m_pPtxGpu = NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  PreRender
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUSystem::PreRender					(grcViewport& viewport, float deltaTime, bool resetParticles)
{
	// set the gpu particle system's gravity and wind influence
	m_pPtxGpu->SetGravity(m_currGravity);
	m_pPtxGpu->SetWindInfluence(m_currWindInfluence);
	
	// Check if camera water depth is greather than zero to figure if we are underwater
	bool isUnderwaterThisFrame = CVfxHelper::GetGameCamWaterDepth()>0.0f;

	// Set the gpu particle height collision render targets
	// When we are underwater, we don't care about collision anymore so we can use a blank texture
	const grcRenderTarget* pRT = CRenderPhaseHeight::GetPtFxGPUCollisionMap()->GetRenderTargetRT();

	// Set a texture which contains "far" depth.
	const grcTexture *pHeightMap = (isUnderwaterThisFrame || pRT == NULL) ? grcTexture::NoneWhite : pRT;
	m_pPtxGpu->GetUpdateShader()->SetHeightMap(pHeightMap);

#if RAIN_UPDATE_CPU_HEIGHTMAP
	const grcTexture *pHeightMapStag = NULL;
	if(ptxgpuBase::CpuRainUpdateEnabled())
	{
		CRenderPhaseHeight *const renderPhaseHeight = CRenderPhaseHeight::GetPtFxGPUCollisionMap();
		if (AssertVerify(renderPhaseHeight && renderPhaseHeight->m_currentStagingRenderTarget < renderPhaseHeight->m_numStagingRenderTargets))
		{
			const s32 currStagIdx = (renderPhaseHeight->m_currentStagingRenderTarget+1) % (renderPhaseHeight->m_numStagingRenderTargets);
			const grcTexture *pHeightMapStag0 = renderPhaseHeight->m_stagingRenderTex[currStagIdx];
			pHeightMapStag = ((isUnderwaterThisFrame || pHeightMapStag0 == NULL) ? NULL : pHeightMapStag0);
			m_pPtxGpu->GetUpdateShader()->SetHeightMapSrc(pHeightMapStag);
		}
	}
#endif //RAIN_UPDATE_CPU_HEIGHTMAP

	// This matrix will transform every position to XY at zero and Z at one (0 on PC, Durango and Orbis) which effectively disables collisions.
	static const Matrix44 disableCollisionMat = Matrix44(
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 1.0f);

	m_pPtxGpu->GetUpdateShader()->SetHeightMapTransformMtx(isUnderwaterThisFrame ? disableCollisionMat : CRenderPhaseHeight::GetPtFxGPUCollisionMap()->GetHeightTransformMtx());

	PTXGPU_USE_WIND_DISTURBANCE_ONLY(m_pPtxGpu->GetUpdateShader()->SetWindDisturbanceTextures(m_pPtxGpu->ms_pWindDisturbanceTexture[0], m_pPtxGpu->ms_pWindDisturbanceTexture[1]);)

	// Camera cut related stuff
	bool hasCut = false;
	if(CutSceneManager::GetInstance()->IsCutscenePlayingBack())
	{
		const camFrame& curFrame = gVpMan.GetCurrentGameViewportFrame();
		hasCut = curFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) || curFrame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);
	}

	// update the gpu particle system
	WindType_e windType = WIND_TYPE_AIR;
	if (isUnderwaterThisFrame)
	{
		windType = WIND_TYPE_WATER;
	}
	m_pPtxGpu->Update(&viewport, deltaTime, windType, m_currLevel, hasCut ? 0.0f : 1.0f, resetParticles);
	
	// tidy up
	m_pPtxGpu->GetUpdateShader()->SetHeightMap(NULL);
#if RAIN_UPDATE_CPU_HEIGHTMAP
	if(ptxgpuBase::CpuRainUpdateEnabled())
	{
		m_pPtxGpu->GetUpdateShader()->UnSetHeightMapSrc(pHeightMapStag);
	}
#endif

	PTXGPU_USE_WIND_DISTURBANCE_ONLY(m_pPtxGpu->GetUpdateShader()->SetWindDisturbanceTextures(NULL, NULL);)
}


///////////////////////////////////////////////////////////////////////////////
//  GlobalPreUpdate
///////////////////////////////////////////////////////////////////////////////

void CPtFxGPUSystem::GlobalPreUpdate()
{
	PTXGPU_USE_WIND_DISTURBANCE_ONLY(ptxgpuBase::UpdateWindDisturbanceTextures();)
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateWindDisturbanceTextures
///////////////////////////////////////////////////////////////////////////////

#if PTXGPU_USE_WIND_DISTURBANCE
void CPtFxGPUSystem::UpdateWindDisturbanceTextures() 
{
	ptxgpuBase::UpdateWindDisturbanceTextures();
}
#endif // PTXGPU_USE_WIND_DISTURBANCE


///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxGPUDropSystem
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUDropSystem::Init							(unsigned int systemId, int numParticles, const char* updateShaderName, const char* updateShaderTechnique, const char* renderShaderName, const char* renderShaderTechnique, bool bUseSoft, grcRenderTarget* pParentTarget /*= NULL*/)
{
	ptfxAssertf(m_pPtxGpu==NULL, "initialisation error");
	m_pPtxGpu = rage_new ptxgpuDrop;

	ptxgpuDropUpdateShader *pUpdateShader = rage_new ptxgpuDropUpdateShader(systemId,updateShaderName);
	ptxgpuDropRenderShader *pRenderShader = rage_new ptxgpuDropRenderShader(renderShaderName);
	pUpdateShader->SetTechnique(updateShaderTechnique);
	pRenderShader->SetTechnique(renderShaderTechnique);
	m_useSoft = bUseSoft;
	// init the gpu particle systems with the shader and set some techniques
	m_pPtxGpu->Init(systemId, numParticles, pUpdateShader, pRenderShader, pParentTarget);
	
	m_currFxIndex = -1;
}


///////////////////////////////////////////////////////////////////////////////
//  Load
///////////////////////////////////////////////////////////////////////////////

void CPtFxGPUDropSystem::LoadFromXmlFile(const CWeatherGpuFxFromXmlFile& WeatherGpuFxFromXmlFile)
{
	ptfxAssertf(m_numSettings < PTFXGPU_FINAL_MAX_SETTINGS, "CPtFxGPUDropSystem::LoadFromXmlFile - too many GPU fx types - %s will be ingnored on non-beta builds", WeatherGpuFxFromXmlFile.m_Name.c_str());
	if (m_numSettings<PTFXGPU_MAX_SETTINGS)
	{
		m_gpuDropSettings[m_numSettings].m_hashValue = atHashValue(WeatherGpuFxFromXmlFile.m_Name);

#if !__FINAL
		strcpy(m_gpuDropSettings[m_numSettings].m_name, WeatherGpuFxFromXmlFile.m_Name.c_str());
#endif

		m_gpuDropSettings[m_numSettings].m_driveType = WeatherGpuFxFromXmlFile.m_driveType;
		m_gpuDropSettings[m_numSettings].m_windInfluence = WeatherGpuFxFromXmlFile.m_windInfluence;
		m_gpuDropSettings[m_numSettings].m_gravity = WeatherGpuFxFromXmlFile.m_gravity;

		// load in the gpu particle settings
		m_gpuDropSettings[m_numSettings].m_dropEmitterSettings.Load(WeatherGpuFxFromXmlFile.m_emitterSettingsName);
		m_gpuDropSettings[m_numSettings].m_dropRenderSettings.Load(WeatherGpuFxFromXmlFile.m_renderSettingsName);

		// set up the texture dictionary
		strLocalIndex txdSlot = strLocalIndex(vfxUtil::InitTxd("fxweather"));

		// load in the gpu particle textures
		m_gpuDropSettings[m_numSettings].m_dropRenderSettings.pTexture = g_TxdStore.Get(txdSlot)->Lookup(WeatherGpuFxFromXmlFile.m_diffuseName);
		ptfxAssertf(m_gpuDropSettings[m_numSettings].m_dropRenderSettings.pTexture, "diffuse texture not found");

		m_gpuDropSettings[m_numSettings].m_dropRenderSettings.pDistotionTexture = const_cast<grcTexture*>( grcTexture::NoneBlack );
		if(WeatherGpuFxFromXmlFile.m_distortionTexture.GetLength() > 0)
		{
			m_gpuDropSettings[m_numSettings].m_dropRenderSettings.pDistotionTexture = g_TxdStore.Get(txdSlot)->Lookup(WeatherGpuFxFromXmlFile.m_distortionTexture);
			ptfxAssertf(m_gpuDropSettings[m_numSettings].m_dropRenderSettings.pDistotionTexture != NULL, "distorion texture not found");
			if(!m_gpuDropSettings[m_numSettings].m_dropRenderSettings.pDistotionTexture)
			{
				m_gpuDropSettings[m_numSettings].m_dropRenderSettings.pDistotionTexture = const_cast<grcTexture*>( grcTexture::NoneBlack );
			}
		}

		m_numSettings++;
	}
	else
	{
		ptfxAssertf(0, "CPtFxGPUDropSystem::LoadFromXmlFile - too many GPU fx types - ignoring %s", WeatherGpuFxFromXmlFile.m_Name.c_str());
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUDropSystem::Update							(s32 prevGpuFxIndex, s32 nextGpuFxIndex, float weatherInterp, float maxVfxRegionLevel, float currVfxRegionLevel)
{
	// early out if the indices are incorrect, only -1 and [0, PTFXGPU_MAX_SETTINGS) are correct
	if (prevGpuFxIndex < -1 || prevGpuFxIndex >= PTFXGPU_MAX_SETTINGS ||
		nextGpuFxIndex < -1 || nextGpuFxIndex >= PTFXGPU_MAX_SETTINGS)
	{
		return;
	}

	// update the current state
	if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_RAIN)) || 
		(nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_RAIN)))
	{
		if(m_pPtxGpu->IsMistInstance())
		{
			// Force the mist to start only when the rain gets strong enough
			static const float fMistThresh = 0.5f;
			m_currLevel = Max(g_vfxWeather.GetCurrRain() - fMistThresh, 0.0f);
			m_currLevel /= (1.0f - fMistThresh);
		}
		else
		{
			m_currLevel = g_vfxWeather.GetCurrRain();
		}
	}
	else if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_SNOW)) || 
			 (nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_SNOW)))
	{
		if(m_pPtxGpu->IsGroundInstance())
		{
			m_currLevel = g_vfxWeather.GetCurrSnowMist();
		}
		else
		{
			m_currLevel = g_vfxWeather.GetCurrSnow();
		}
	}
	else if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_REGION)) || 
			 (nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_REGION)))
	{
		m_currLevel = maxVfxRegionLevel;
	}
	else if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_UNDERWATER)) || 
			 (nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_UNDERWATER)))
	{
		m_currLevel = 1.0f;
	}
	else if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_NONE)) || 
			 (nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_NONE)))
	{
		m_currLevel = 1.0f;
	}

	//Override settings here in case if it is set
	if(m_useOverrideSettings)
	{
		m_currLevel = m_overrideCurrLevel;
	}

	if (prevGpuFxIndex==-1 && nextGpuFxIndex==-1)
	{
		m_currLevel = 0.0f;
		m_currFxIndex = -1;
	}
	else if (prevGpuFxIndex==-1)
	{
		ptfxAssertf(nextGpuFxIndex>=0 && nextGpuFxIndex<(s32)m_numSettings, "next GPU index is out of range");
		m_currDropEmitterSettings = m_gpuDropSettings[nextGpuFxIndex].m_dropEmitterSettings;
		m_currDropRenderSettings = m_gpuDropSettings[nextGpuFxIndex].m_dropRenderSettings;

		m_currWindInfluence = m_gpuDropSettings[nextGpuFxIndex].m_windInfluence;
		m_currGravity = m_gpuDropSettings[nextGpuFxIndex].m_gravity;
		m_currFxIndex = nextGpuFxIndex;
	}
	else if (nextGpuFxIndex==-1)
	{
		ptfxAssertf(prevGpuFxIndex>=0 && prevGpuFxIndex<(s32)m_numSettings, "previous GPU index is out of range");
		m_currDropEmitterSettings = m_gpuDropSettings[prevGpuFxIndex].m_dropEmitterSettings;
		m_currDropRenderSettings = m_gpuDropSettings[prevGpuFxIndex].m_dropRenderSettings;

		m_currWindInfluence = m_gpuDropSettings[prevGpuFxIndex].m_windInfluence;
		m_currGravity = m_gpuDropSettings[prevGpuFxIndex].m_gravity;
		m_currFxIndex = prevGpuFxIndex;
	}
	else
	{
		ptfxAssertf(prevGpuFxIndex>=0 && prevGpuFxIndex<(s32)m_numSettings, "previous GPU index is out of range");
		ptfxAssertf(nextGpuFxIndex>=0 && nextGpuFxIndex<(s32)m_numSettings, "next GPU index is out of range");

		if (prevGpuFxIndex==nextGpuFxIndex)
		{
			m_currDropEmitterSettings = m_gpuDropSettings[prevGpuFxIndex].m_dropEmitterSettings;
			m_currDropRenderSettings = m_gpuDropSettings[prevGpuFxIndex].m_dropRenderSettings;

			m_currWindInfluence = m_gpuDropSettings[prevGpuFxIndex].m_windInfluence;
			m_currGravity = m_gpuDropSettings[prevGpuFxIndex].m_gravity;
			m_currFxIndex = prevGpuFxIndex;
		}
		else
		{
			if (ptfxVerifyf(m_gpuDropSettings[prevGpuFxIndex].m_driveType == m_gpuDropSettings[nextGpuFxIndex].m_driveType, "trying to interpolate between 2 GPU effects of different types"))
			{
				m_currDropEmitterSettings.Interpolate(m_gpuDropSettings[prevGpuFxIndex].m_dropEmitterSettings, m_gpuDropSettings[nextGpuFxIndex].m_dropEmitterSettings, weatherInterp);
				m_currDropRenderSettings.Interpolate(m_gpuDropSettings[prevGpuFxIndex].m_dropRenderSettings, m_gpuDropSettings[nextGpuFxIndex].m_dropRenderSettings, weatherInterp);

				m_currWindInfluence = m_gpuDropSettings[prevGpuFxIndex].m_windInfluence + ((m_gpuDropSettings[nextGpuFxIndex].m_windInfluence - m_gpuDropSettings[prevGpuFxIndex].m_windInfluence)*weatherInterp);
				m_currGravity = m_gpuDropSettings[prevGpuFxIndex].m_gravity + ((m_gpuDropSettings[nextGpuFxIndex].m_gravity - m_gpuDropSettings[prevGpuFxIndex].m_gravity)*weatherInterp);

				m_currFxIndex = prevGpuFxIndex; // We use prev here, since that's where stuff like texture is coming from.
			}
			else
			{
				m_currDropEmitterSettings = m_gpuDropSettings[nextGpuFxIndex].m_dropEmitterSettings;
				m_currDropRenderSettings = m_gpuDropSettings[nextGpuFxIndex].m_dropRenderSettings;

				m_currWindInfluence = m_gpuDropSettings[nextGpuFxIndex].m_windInfluence;
				m_currGravity = m_gpuDropSettings[nextGpuFxIndex].m_gravity;
				m_currFxIndex = nextGpuFxIndex;
			}
		}
	}

	if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_REGION)) || 
		(nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_REGION)))
	{
		m_currDropRenderSettings.colour.w *= currVfxRegionLevel;
	}

	// hack for when it's raining and a slowdown special ability is active
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed && pPed->GetSpecialAbility() && pPed->GetSpecialAbility()->IsActive() && 
		(pPed->GetSpecialAbility()->GetType()==SAT_CAR_SLOWDOWN || pPed->GetSpecialAbility()->GetType()==SAT_BULLET_TIME))
	{
		if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_RAIN)) || 
			(nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_RAIN)))
		{
			if (!m_pPtxGpu->IsMistInstance() && !m_pPtxGpu->IsGroundInstance())
			{
				Vector4 currSizeMinMax = m_currDropRenderSettings.sizeMinMax;
				m_currDropRenderSettings.sizeMinMax = Vector4(currSizeMinMax.x*GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_WIDTH_SCALE, 
															  currSizeMinMax.y*GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_HEIGHT_SCALE, 
															  currSizeMinMax.z*GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_WIDTH_SCALE, 
															  currSizeMinMax.w*GPUPTFX_SLOWDOWN_ABILITY_RAIN_DROP_HEIGHT_SCALE);
			}
		}
	}

	if(m_useOverrideSettings)
	{
		m_currDropEmitterSettings.boxCentreOffset = RCC_VECTOR3(m_overrideBoxOffset);
		m_currDropEmitterSettings.boxSize = RCC_VECTOR3(m_overrideBoxSize);
	}

}

///////////////////////////////////////////////////////////////////////////////
//  Interpolate
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUDropSystem::Interpolate(s32 thirdGpuFxIndex, float weatherInterp)
{
	// update the current state
	if (m_gpuDropSettings[thirdGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_RAIN))
	{
		m_currLevel = g_vfxWeather.GetCurrRain();
	}
	else if (m_gpuDropSettings[thirdGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_SNOW))
	{
		m_currLevel = g_vfxWeather.GetCurrSnow();
	}
	else if (m_gpuDropSettings[thirdGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_NONE))
	{
		m_currLevel = 1.0f; // underwater - always on
	}

	if(m_useOverrideSettings)
	{
		m_currLevel = m_overrideCurrLevel;
	}

	if( m_currFxIndex == -1  || m_currFxIndex == thirdGpuFxIndex )
	{ // not trully interpolating...
		m_currDropEmitterSettings = m_gpuDropSettings[thirdGpuFxIndex].m_dropEmitterSettings;
		m_currDropRenderSettings = m_gpuDropSettings[thirdGpuFxIndex].m_dropRenderSettings;

		m_currWindInfluence = m_gpuDropSettings[thirdGpuFxIndex].m_windInfluence;
		
		m_currGravity = m_gpuDropSettings[thirdGpuFxIndex].m_gravity;
	
		m_currFxIndex = thirdGpuFxIndex;
	}
	else
	{
		m_currDropEmitterSettings.Interpolate(m_gpuDropSettings[thirdGpuFxIndex].m_dropEmitterSettings, weatherInterp);
		m_currDropRenderSettings.Interpolate(m_gpuDropSettings[thirdGpuFxIndex].m_dropRenderSettings, weatherInterp);

		float tmp = m_currWindInfluence;
		m_currWindInfluence = tmp + ((m_gpuDropSettings[thirdGpuFxIndex].m_windInfluence - tmp)*weatherInterp);
		
		tmp = m_currGravity;
		m_currGravity = tmp + ((m_gpuDropSettings[thirdGpuFxIndex].m_gravity - tmp)*weatherInterp);
	}

	if(m_useOverrideSettings)
	{
		m_currDropEmitterSettings.boxCentreOffset = RCC_VECTOR3(m_overrideBoxOffset);
		m_currDropEmitterSettings.boxSize = RCC_VECTOR3(m_overrideBoxSize);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  PreRender
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUDropSystem::PreRender					(grcViewport& viewport, float deltaTime, bool resetParticles)
{
	// check if gpu particles are active
	if (m_currLevel>0.0f)
	{
		// set up the emitter
		((ptxgpuDrop*)m_pPtxGpu)->SetEmitterSettings(m_currDropEmitterSettings);

		CPtFxGPUSystem::PreRender(viewport, deltaTime, resetParticles);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUDropSystem::Render						(grcViewport& viewport, ptxRenderSetup* pRenderSetup, Vec3V_In vCamVel, float camPosZ, float groundPosZ BANK_ONLY(, Vec4V_In vCamAngleLimits))
{
	// check if gpu particles are active
	if (m_currLevel>0.0f)
	{
		// render the gpu particles
		Matrix44 cam = RCC_MATRIX44(viewport.GetCameraMtx44());
		Vector3 boxForward = -cam.c.GetVector3() * Vector3(1.0f, 1.0f, 0.0f);
		boxForward.Normalize();
		Vector3 boxCentrePos = cam.d.GetVector3() + boxForward * 10.0f;
		Vector3 boxWidth(35.0f, 35.0f, 25.0f);
		Lights::UseLightsInArea(spdAABB(VECTOR3_TO_VEC3V(boxCentrePos - boxWidth), VECTOR3_TO_VEC3V(boxCentrePos + boxWidth)), false, true, false, true);

		const bool bHasDistortion = HasDistortion();
		if ( m_currDropRenderSettingsRT.backgroundDistortionBlurLevel > -1 )
		{
			m_currDropRenderSettingsRT.pBackBufferTexture = PostFX::GetDownSampledBackBuffer(bHasDistortion ? m_currDropRenderSettingsRT.backgroundDistortionBlurLevel : 0);
		}
		else
		{
			m_currDropRenderSettingsRT.pBackBufferTexture = const_cast<grcTexture*>( grcTexture::NoneBlack );
		}

		if (g_visualSettings.GetV<float>(atHashValue("rain.useLitShader"), 1.0f))
		{
			float lightIntensityMult = g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_GPUPTFX_LIGHT_INTENSITY_MULT);					
			m_currDropRenderSettingsRT.lightIntensityMultiplier = lightIntensityMult;
		}

		((ptxgpuDrop*)m_pPtxGpu)->SetRenderSettings(m_currDropRenderSettingsRT);

		//Lets figure out the pass
		bool bUseAnimatedTextures = (m_currDropRenderSettingsRT.textureAnimRateScaleOverLifeStart2End2.x > 0);
		bool bUseABBlend = ((m_currDropRenderSettingsRT.textureRowsColsStartEnd.z != m_currDropRenderSettingsRT.textureAnimRateScaleOverLifeStart2End2.z) || 
			(m_currDropRenderSettingsRT.textureRowsColsStartEnd.w != m_currDropRenderSettingsRT.textureAnimRateScaleOverLifeStart2End2.w)) ;
		const int pass = BIT(0) * bUseAnimatedTextures + BIT(1) * bUseABBlend + BIT(2) * bHasDistortion;

		m_pPtxGpu->GetRenderShader()->SetPass(pass);
		((ptxgpuDrop*)m_pPtxGpu)->Render(&viewport, vCamVel, m_currDropEmitterSettings.lifeMinMax.y, camPosZ, groundPosZ, pRenderSetup, Min(m_currLevel*1.5f, 1.0f) BANK_ONLY(, vCamAngleLimits));
		m_pPtxGpu->GetRenderShader()->SetPass(0);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  GetSettingIndex
///////////////////////////////////////////////////////////////////////////////

s32			CPtFxGPUDropSystem::GetSettingIndex			(atHashValue hashValue)
{
	for (u32 i=0; i<m_numSettings; i++)
	{
		if (m_gpuDropSettings[i].m_hashValue == hashValue)
		{
			return i;
		}
	}

	return -1;
}

///////////////////////////////////////////////////////////////////////////////
//  GetEmitterBounds
///////////////////////////////////////////////////////////////////////////////

bool		CPtFxGPUDropSystem::GetEmitterBounds		(Mat44V_ConstRef camMatrix, Vec3V_Ref emitterMin, Vec3V_Ref emitterMax)
{
	if (m_currLevel>0.0f)
	{
		((ptxgpuDrop*)m_pPtxGpu)->GetEmitterBounds(camMatrix, emitterMin, emitterMax);
		return true;
	}

	return false;
}

#if __BANK 
///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUDropSystem::InitWidgets					()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->AddTitle("INFO");
	pVfxBank->AddSlider("Num Settings", &m_numSettings, 0, PTFXGPU_MAX_SETTINGS, 0);
	pVfxBank->AddSlider("Curr Level", &m_currLevel, -100.0f, 100.0f, 0.0f);
	pVfxBank->AddSlider("Curr Wind Influence", &m_currWindInfluence, -100.0f, 100.0f, 0.0f);
	pVfxBank->AddSlider("Curr Gravity", &m_currGravity, -100.0f, 100.0f, 0.0f);

	m_pPtxGpu->AddWidgets(*pVfxBank);

	for (u32 i=0; i<m_numSettings; i++)
	{
		char txt[64];
#if !__FINAL
		sprintf(txt, "Emitter Settings - %s", m_gpuDropSettings[i].m_name);
#else
		sprintf(txt, "Emitter Settings - %d", i);
#endif
		pVfxBank->PushGroup(txt, false);
		m_gpuDropSettings[i].m_dropEmitterSettings.AddWidgets(*pVfxBank, true);
		pVfxBank->PopGroup();

#if !__FINAL
		sprintf(txt, "Render Settings - %s", m_gpuDropSettings[i].m_name);
#else
		sprintf(txt, "Render Settings - %d", i);
#endif
		pVfxBank->PushGroup(txt, false);
		m_gpuDropSettings[i].m_dropRenderSettings.AddWidgets(*pVfxBank, true);
		pVfxBank->PopGroup();
	}
}

void CPtFxGPUDropSystem::RenderDebug(const grcViewport& viewport, const Color32& color)
{
	if (m_currLevel>0.0f)
	{
		((ptxgpuDrop*)m_pPtxGpu)->RenderDebug(viewport, color);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  DebugUpdateRenderSettings
///////////////////////////////////////////////////////////////////////////////

void			CPtFxGPUDropSystem::DebugUpdateRenderSettings			(s32 prevGpuFxIndex, s32 nextGpuFxIndex, float weatherInterp, float currVfxRegionLevel)
{
	//This function just updates the render settings while the game is paused. It makes it easier for artists to tune the settings in real time without unpausing the game
	if (prevGpuFxIndex==-1 && nextGpuFxIndex==-1)
	{
		//Do nothing as state does not change
	}
	else if (prevGpuFxIndex==-1)
	{
		ptfxAssertf(nextGpuFxIndex>=0 && nextGpuFxIndex<(s32)m_numSettings, "next GPU index is out of range");
		m_currDropRenderSettings = m_gpuDropSettings[nextGpuFxIndex].m_dropRenderSettings;
	}
	else if (nextGpuFxIndex==-1)
	{
		ptfxAssertf(prevGpuFxIndex>=0 && prevGpuFxIndex<(s32)m_numSettings, "previous GPU index is out of range");
		m_currDropRenderSettings = m_gpuDropSettings[prevGpuFxIndex].m_dropRenderSettings;
	}
	else
	{
		ptfxAssertf(prevGpuFxIndex>=0 && prevGpuFxIndex<(s32)m_numSettings, "previous GPU index is out of range");
		ptfxAssertf(nextGpuFxIndex>=0 && nextGpuFxIndex<(s32)m_numSettings, "next GPU index is out of range");

		if (prevGpuFxIndex==nextGpuFxIndex)
		{
			m_currDropRenderSettings = m_gpuDropSettings[prevGpuFxIndex].m_dropRenderSettings;
		}
		else
		{
			ptfxAssertf(m_gpuDropSettings[prevGpuFxIndex].m_driveType == m_gpuDropSettings[nextGpuFxIndex].m_driveType, "trying to interpolate between 2 GPU effects of different types");

			m_currDropRenderSettings.Interpolate(m_gpuDropSettings[prevGpuFxIndex].m_dropRenderSettings, m_gpuDropSettings[nextGpuFxIndex].m_dropRenderSettings, weatherInterp);
		}
	}

	if ((prevGpuFxIndex!=-1 && m_gpuDropSettings[prevGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_REGION)) || 
		(nextGpuFxIndex!=-1 && m_gpuDropSettings[nextGpuFxIndex].m_driveType==(u32)(DRIVE_TYPE_REGION)))
	{
		m_currDropRenderSettings.colour.w *= currVfxRegionLevel;
	}
}
#endif // __BANK


///////////////////////////////////////////////////////////////////////////////
//  PauseSimulation
///////////////////////////////////////////////////////////////////////////////

void CPtFxGPUDropSystem::PauseSimulation()
{
	m_pPtxGpu->PauseSimulation();
}


