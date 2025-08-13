///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	PtFxDebug.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	17 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#if __BANK

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Vfx/Particles/PtFxDebug.h"

// rage
#include "rmptfx/ptxmanager.h"

// framework
#include "vfx/ptfx/ptfxasset.h"
#include "vfx/ptfx/ptfxdebug.h"
#include "vfx/vfxwidget.h"

// game
#include "Peds/Ped.h"
#include "Scene/Portals/Portal.h"
#include "Scene/World/GameWorld.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/particles/PtfxEntity.h"
#include "Vfx/VfxSettings.h"
#include "Renderer/RenderPhases/RenderPhaseCascadeShadows.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  STATIC VARIABLES
///////////////////////////////////////////////////////////////////////////////

bool CPtFxDebug::m_moveUsingTeleport = false;
float CPtFxDebug::m_movePlayerPedDist = 10.0f;
float CPtFxDebug::m_movePlayerVehDist = 10.0f;


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////



///////////////////////////////////////////////////////////////////////////////
//  PARAMS
///////////////////////////////////////////////////////////////////////////////

XPARAM(ptfxperformancetest);

PARAM(ptfxnodownsampling, "disable the downsampled particles");


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS PtFxDebug
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxDebug::Init						()
{
	m_disablePtFxRender = false;

	// init our debug widgets
#if PTFX_ALLOW_INSTANCE_SORTING
	m_disableSortedRender = false;
	m_disableSortedEffects = false;
	m_forceSortedEffects = false;
	m_onlyFlaggedSortedEffects = false;
	m_disableSortedVehicleOffsets = false;
	m_renderSortedEffectsBoundingBox = false;
	m_renderSortedVehicleEffectsBoundingBox = false;
	m_renderSortedVehicleEffectsCamDebug = false;

	m_sortedEffectsVehicleCamDirOffset = PTFX_SORTED_VEHICLE_CAM_DIR_OFFSET;
	m_sortedEffectsVehicleCamPosOffset = PTFX_SORTED_VEHICLE_CAM_POS_OFFSET;
	m_sortedEffectsVehicleMinDistFromCenter = PTFX_SORTED_VEHICLE_MIN_DIST_FROM_CENTER;
#endif 

	m_disableHighBucket = false;
	m_disableMediumBucket = false;
	m_disableLowBucket = false;
	m_disableRefractionBucket = false;
	m_disableWaterRefractionBucket = false;
	m_drawWireframe = false;
#if PTFX_RENDER_IN_MIRROR_REFLECTIONS
	m_disableRenderMirrorReflections = false;
#endif //PTFX_RENDER_IN_MIRROR_REFLECTIONS

#if __D3D11 || RSG_ORBIS
	m_tessellationFactor = 5.0f;
#endif
#if RMPTFX_USE_PARTICLE_SHADOWS
	m_globalParticleShadowIntensity = 0.5f;
	m_globalParticleShadowIntensityOveride = false;
	m_globalParticleShadowSlopeBias = CSM_PARTICLE_SHADOWS_DEPTH_SLOPE_BIAS;
	m_globalParticleShadowBiasRange = CSM_PARTICLE_SHADOWS_DEPTH_BIAS_RANGE;
	m_globalParticleShadowBiasRangeFalloff = CSM_PARTICLE_SHADOWS_DEPTH_BIAS_RANGE_FALLOFF;
#endif //RMPTFX_USE_PARTICLE_SHADOWS

#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
		m_globalParticleDofAlphaScale = PTFX_PARTICLE_DOF_ALPHA_SCALE;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

#if USE_SHADED_PTFX_MAP
	m_disableShadowedPtFx = false;
	m_renderShadowedPtFxBox = false;
	m_shadowedPtFxRange = PTFX_SHADOWED_RANGE_DEFAULT;
	m_shadowedPtFxLoHeight = PTFX_SHADOWED_LOW_HEIGHT_DEFAULT;
	m_shadowedPtFxHiHeight = PTFX_SHADOWED_HIGH_HEIGHT_DEFAULT;
	m_shadowedPtFxBlurKernelSize = PTFX_SHADOWED_BLUR_KERNEL_SIZE_DEFAULT;
#endif //USE_SHADED_PTFX_MAP

	if (PARAM_ptfxnodownsampling.Get())
	{
		m_disableDownsampling = true;
	}
	else
	{
		m_disableDownsampling = false;
	}

	m_renderAllDownsampled = false;
	m_dynamicQualitySystemEnabled = true;
	m_qualityUpdateTime = PTFX_DYNAMIC_QUALITY_UPDATE_TIME;			// seconds
	m_totalRenderTimeThreshold = PTFX_DYNAMIC_QUALITY_TOTAL_TIME_THRESHOLD;	// ms
	m_lowResRenderTimeThreshold = PTFX_DYNAMIC_QUALITY_LOWRES_TIME_THRESHOLD;	// ms
	m_endDownsamplingThreshold = PTFX_DYNAMIC_QUALITY_END_LOWRES_TIME_THRESHOLD;	// ms

	m_renderDebugCollisionProbes = false;
	m_renderDebugCollisionProbesFrames = 0;
	m_renderDebugCollisionPolys = false;
	m_interiorFxLevel = 0.0f;
	m_currAirResistance = 1.0f;

	m_overrideDeltaTime = false;
	m_overrideDeltaTimeValue = 1.0f/30.0f;
	m_disableWaterRefractionPostProcess = false;

	m_alwaysPassShouldBeRenderedRuleChecks = false;

	m_ExplosionPerformanceTest = false;
}

///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxDebug::InitWidgets						()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	bkGroup* pGroup = ptfxDebug::GetGameGroup();

	pVfxBank->SetCurrentGroup(*pGroup);
	{
		pVfxBank->AddTitle("INFO");
		pVfxBank->AddSlider("Interior Fx Level", &m_interiorFxLevel, 0.0f, 1.0f, 0.0f);
		pVfxBank->AddSlider("Air Resistance", &m_currAirResistance, 0.0f, 1.0f, 0.0f);

#if __DEV
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");

		pVfxBank->AddSlider("Soak Slow Inc Level (Low)", &PTFX_SOAK_SLOW_INC_LEVEL_LOW, 0.0f, 5.0f, 0.005f, NullCB, "");
		pVfxBank->AddSlider("Soak Slow Inc Level (High)", &PTFX_SOAK_SLOW_INC_LEVEL_HIGH, 0.0f, 5.0f, 0.005f, NullCB, "");
		pVfxBank->AddSlider("Soak Medium Inc Level (Low)", &PTFX_SOAK_MEDIUM_INC_LEVEL_LOW, 0.0f, 5.0f, 0.005f, NullCB, "");
		pVfxBank->AddSlider("Soak Medium Inc Level (High)", &PTFX_SOAK_MEDIUM_INC_LEVEL_HIGH, 0.0f, 5.0f, 0.005f, NullCB, "");
		pVfxBank->AddSlider("Soak Fast Inc Level (Low)", &PTFX_SOAK_FAST_INC_LEVEL_LOW, 0.0f, 5.0f, 0.005f, NullCB, "");
		pVfxBank->AddSlider("Soak Fast Inc Level (High)", &PTFX_SOAK_FAST_INC_LEVEL_HIGH, 0.0f, 5.0f, 0.005f, NullCB, "");
#endif

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG TOGGLES");
		pVfxBank->AddToggle("Disable Rendering", &m_disablePtFxRender);

#if PTFX_ALLOW_INSTANCE_SORTING
		pVfxBank->PushGroup("Sorted Ptfx");
		pVfxBank->AddToggle("Disable Sorted Rendering", &m_disableSortedRender);
		pVfxBank->AddToggle("Disable Sorted Effects", &m_disableSortedEffects);
		pVfxBank->AddToggle("Force Sorted Effects", &m_forceSortedEffects);
		pVfxBank->AddToggle("Only Flagged Sorted Effects", &m_onlyFlaggedSortedEffects);
		pVfxBank->AddToggle("Disable Sorted Vehicle Offsets", &m_disableSortedVehicleOffsets);
		pVfxBank->AddToggle("Render Sorted Effects Bounding Boxes", &m_renderSortedEffectsBoundingBox);
		pVfxBank->AddToggle("Render Sorted Vehicle Effects Bounding Boxes (PICK VEHICLE TO SEE)", &m_renderSortedVehicleEffectsBoundingBox);
		pVfxBank->AddToggle("Render Sorted Vehicle Effects Cam Debug (PICK VEHICLE TO SEE)", &m_renderSortedVehicleEffectsCamDebug);		

		pVfxBank->AddSlider("Vehicle Camera Dir Offset", &m_sortedEffectsVehicleCamDirOffset, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("Vehicle Camera Pos Offset", &m_sortedEffectsVehicleCamPosOffset, 0.0f, 5.0f, 0.001f);
		pVfxBank->AddSlider("Vehicle Min Dist From Center at Min Velocity", &m_sortedEffectsVehicleMinDistFromCenter, 0.0f, 5.0f, 0.001f);

		pVfxBank->PopGroup();
#endif

		pVfxBank->AddToggle("Disable High Bucket", &m_disableHighBucket);
		pVfxBank->AddToggle("Disable Medium Bucket", &m_disableMediumBucket);
		pVfxBank->AddToggle("Disable Low Bucket", &m_disableLowBucket);
		pVfxBank->AddToggle("Disable Refraction Bucket", &m_disableRefractionBucket);	
		pVfxBank->AddToggle("Disable Water Refraction Bucket", &m_disableWaterRefractionBucket);	
#if PTFX_RENDER_IN_MIRROR_REFLECTIONS
		pVfxBank->AddToggle("Disable Render Mirror Reflections", &m_disableRenderMirrorReflections);	
#endif //PTFX_RENDER_IN_MIRROR_REFLECTIONS

		pVfxBank->AddToggle("Disable Water Refraction PostProcess", &m_disableWaterRefractionPostProcess);
		pVfxBank->AddToggle("Always Pass Should Be Rendered Rule Checks", &m_alwaysPassShouldBeRenderedRuleChecks);

		pVfxBank->AddToggle("Disable Downsampling", &m_disableDownsampling);

		pVfxBank->AddToggle("Draw Wireframe", &m_drawWireframe);
#if __D3D11 || RSG_ORBIS
		pVfxBank->AddSlider("Particle Tessellation", &m_tessellationFactor, 0, 10, 1);		
#endif

		pVfxBank->AddSeparator();
		pVfxBank->AddToggle("Explosion performance test", &m_ExplosionPerformanceTest);

#if RMPTFX_USE_PARTICLE_SHADOWS
		pVfxBank->AddSeparator();
		pVfxBank->AddToggle("Overide particle Intensity", &m_globalParticleShadowIntensityOveride);
		pVfxBank->AddSlider("Particle Shadow Intensity", &m_globalParticleShadowIntensity, 0.0f, 2.0f, 0.05f);

		pVfxBank->AddSeparator();		
		pVfxBank->AddSlider("Particle Shadow Slope Bias", &m_globalParticleShadowSlopeBias, 0.0f, 6.0f, 0.05f);
		pVfxBank->AddSlider("Particle Shadow Bias Range", &m_globalParticleShadowBiasRange, 0.0f, 100.0f, 0.5f);
		pVfxBank->AddSlider("Particle Shadow Bias Range Falloff", &m_globalParticleShadowBiasRangeFalloff, 0.0f, 100.0f, 0.5f);

#endif //RMPTFX_USE_PARTICLE_SHADOWS

#if PTFX_APPLY_DOF_TO_PARTICLES
		if (CPtFxManager::UseParticleDOF())
		{
			pVfxBank->AddSeparator();
			pVfxBank->AddSlider("Particle DOF Alpha Scale", &m_globalParticleDofAlphaScale, 0.0f, 10.0f, 0.1f);
		}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

		pVfxBank->AddSeparator();

#if USE_SHADED_PTFX_MAP
		pVfxBank->AddToggle("Disable Shadowed Particles", &m_disableShadowedPtFx);
		//pVfxBank->AddToggle("Render Shadowed Particles Box", &m_renderShadowedPtFxBox);
		pVfxBank->AddSlider("Shadowed Particle Distance", &m_shadowedPtFxRange, 0.5f, 100.0f, 0.5f);
		pVfxBank->AddSlider("Shadowed Particle Low Height Offset", &m_shadowedPtFxLoHeight, -4.0f, 4.0f, 0.01f);
		pVfxBank->AddSlider("Shadowed Particle High Height Offset", &m_shadowedPtFxHiHeight, 1.0f, 20.0, 0.01f);
		pVfxBank->AddSlider("Shadowed Particle Blur Kernel Size", &m_shadowedPtFxBlurKernelSize, 1.0f, 10.0, 0.1f);
				
		pVfxBank->AddSeparator();
#endif //USE_SHADED_PTFX_MAP

		pVfxBank->AddToggle("Render All Downsampled", &m_renderAllDownsampled);

#if PTFX_DYNAMIC_QUALITY
		pVfxBank->AddToggle("Dynamic Quality System (DQ)", &m_dynamicQualitySystemEnabled);
		pVfxBank->AddSlider("DQ Update Frequency (seconds)", &m_qualityUpdateTime, 0.1f, 10.0, 0.1f);
		pVfxBank->AddSlider("DQ PtFx Total Render Time Threshold (ms)", &m_totalRenderTimeThreshold, 0.1f, 10.0, 0.1f);
		pVfxBank->AddSlider("DQ PtFx Low Res Only Threshold (ms)", &m_lowResRenderTimeThreshold, 0.1f, 10.0, 0.1f);
		pVfxBank->AddSlider("DQ PtFx Stop Downsampling Threshold (ms)", &m_endDownsamplingThreshold, 0.1f, 10.0, 0.1f);
#endif

		pVfxBank->AddToggle("Override Delta Time", &m_overrideDeltaTime);
		pVfxBank->AddSlider("Override Delta Time Value", &m_overrideDeltaTimeValue, -1.0f, 1.0f, 0.001f);

		pVfxBank->AddToggle("Render Debug Collision Probes", &m_renderDebugCollisionProbes);
		pVfxBank->AddSlider("Render Debug Collision Probes Frames", &m_renderDebugCollisionProbesFrames, 0, 500, 1);
		pVfxBank->AddToggle("Render Debug Collision Polys", &m_renderDebugCollisionPolys);
		
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG MISC");
		pVfxBank->AddButton("Remove Fx From Player", RemoveFxFromPlayer);
		pVfxBank->AddButton("Remove Fx From Player Vehicle", RemoveFxFromPlayerVeh);
		pVfxBank->AddButton("Remove Fx Around Player", RemoveFxAroundPlayer);

		pVfxBank->AddToggle("Move Using Teleport", &m_moveUsingTeleport);
		pVfxBank->AddSlider("Move Player Ped Dist", &m_movePlayerPedDist, 1.0f, 50.0f, 1.0f);
		pVfxBank->AddButton("Move Player Ped Forward", MovePlayerPedForward);
		pVfxBank->AddButton("Move Player Ped Back", MovePlayerPedBack);
		pVfxBank->AddSlider("Move Player Vehicle Dist", &m_movePlayerVehDist, 1.0f, 50.0f, 1.0f);
		pVfxBank->AddButton("Move Player Vehicle Forward", MovePlayerVehForward);
		pVfxBank->AddButton("Move Player Vehicle Back", MovePlayerVehBack);
	}

	pVfxBank->UnSetCurrentGroup(*pGroup);
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxDebug::Update						()
{
	m_interiorFxLevel = CPortal::GetInteriorFxLevel();
	m_currAirResistance = g_vfxSettings.GetPtFxAirResistance();

	if (PARAM_ptfxperformancetest.Get())
	{
		CPtFxDebug::UpdatePerformanceTest();
	}
}


///////////////////////////////////////////////////////////////////////////////
//	RemoveFxFromPlayer
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::RemoveFxFromPlayer				()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		g_ptFxManager.RemovePtFxFromEntity(pPlayerPed);
	}
}


///////////////////////////////////////////////////////////////////////////////
//	RemoveFxFromPlayerVeh
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::RemoveFxFromPlayerVeh			()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		CVehicle* pPlayerVehicle = pPlayerPed->GetMyVehicle();
		if (pPlayerVehicle)
		{

			g_ptFxManager.RemovePtFxFromEntity(pPlayerVehicle);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	RemoveFxAroundPlayer
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::RemoveFxAroundPlayer			()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		g_ptFxManager.RemovePtFxInRange(pPlayerPed->GetTransform().GetPosition(), 20.0f);
	}
}


///////////////////////////////////////////////////////////////////////////////
//	MovePlayerPedForward
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::MovePlayerPedForward			()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		Vec3V vForward = pPlayerPed->GetTransform().GetMatrix().GetCol1();
		Vec3V vCurrPos = pPlayerPed->GetTransform().GetPosition();
		Vec3V vNewPos = vCurrPos + (vForward * ScalarVFromF32(m_movePlayerPedDist));

		if (m_moveUsingTeleport)
		{
			pPlayerPed->Teleport(RCC_VECTOR3(vNewPos), pPlayerPed->GetCurrentHeading());
		}
		else
		{
			pPlayerPed->SetPosition(RCC_VECTOR3(vNewPos));
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	MovePlayerPedBack
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::MovePlayerPedBack				()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		Vec3V vForward = pPlayerPed->GetTransform().GetMatrix().GetCol1();
		Vec3V vCurrPos = pPlayerPed->GetTransform().GetPosition();
		Vec3V vNewPos = vCurrPos - (vForward * ScalarVFromF32(m_movePlayerPedDist));

		if (m_moveUsingTeleport)
		{
			pPlayerPed->Teleport(RCC_VECTOR3(vNewPos), pPlayerPed->GetCurrentHeading());
		}
		else
		{
			pPlayerPed->SetPosition(RCC_VECTOR3(vNewPos));
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	MovePlayerVehForward
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::MovePlayerVehForward			()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		CVehicle* pPlayerVehicle = pPlayerPed->GetMyVehicle();
		if (pPlayerVehicle)
		{
			Vec3V vForward = pPlayerVehicle->GetTransform().GetMatrix().GetCol1();
			Vec3V vCurrPos = pPlayerVehicle->GetTransform().GetPosition();
			Vec3V vNewPos = vCurrPos + (vForward * ScalarVFromF32(m_movePlayerVehDist));

			if (m_moveUsingTeleport)
			{
				pPlayerVehicle->Teleport(RCC_VECTOR3(vNewPos));
			}
			else
			{
				pPlayerVehicle->SetPosition(RCC_VECTOR3(vNewPos));
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	MovePlayerVehBack
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::MovePlayerVehBack				()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		CVehicle* pPlayerVehicle = pPlayerPed->GetMyVehicle();
		if (pPlayerVehicle)
		{
			Vec3V vForward = pPlayerVehicle->GetTransform().GetMatrix().GetCol1();
			Vec3V vCurrPos = pPlayerVehicle->GetTransform().GetPosition();
			Vec3V vNewPos = vCurrPos - (vForward * ScalarVFromF32(m_movePlayerVehDist));

			if (m_moveUsingTeleport)
			{
				pPlayerVehicle->Teleport(RCC_VECTOR3(vNewPos));
			}
			else
			{
				pPlayerVehicle->SetPosition(RCC_VECTOR3(vNewPos));
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	UpdatePerformanceTest
///////////////////////////////////////////////////////////////////////////////

void			CPtFxDebug::UpdatePerformanceTest			()
{
	ptfxDebug::SetOnlyPlayThisFx(false);

	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();
	if (pPlayerPed)
	{
		Vec3V vBasePos = pPlayerPed->GetTransform().GetPosition();
		Vec3V vForward = pPlayerPed->GetTransform().GetMatrix().GetCol1();
		Vec3V vRight = pPlayerPed->GetTransform().GetMatrix().GetCol0();
		ScalarV vRadius = ScalarV(V_FIVE);

		static float numPtFx = 32;
		for (int i=0; i<numPtFx; i++)
		{
			float sinVal = sinf(((float)i/(float)numPtFx) * TWO_PI);
			float cosVal = cosf(((float)i/(float)numPtFx) * TWO_PI);
			ScalarV vSinVal = ScalarVFromF32(sinVal);
			ScalarV vCosVal = ScalarVFromF32(cosVal);

			Vec3V vCurrPos = vBasePos;
			vCurrPos += vForward * vCosVal * vRadius;
			vCurrPos += vRight * vSinVal * vRadius;

			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(this, i, false, "fire_map", justCreated);

			// check the effect exists
			if (pFxInst)
			{
				pFxInst->SetBasePos(vCurrPos);

				if (justCreated)
				{		
					pFxInst->Start();
					pFxInst->SetCanBeCulled(false);
				}
			}
		}
	}

	ptfxDebug::SetOnlyPlayThisFx(true);
}


#endif // __BANK 







