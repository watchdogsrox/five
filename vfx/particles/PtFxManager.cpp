///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	ParticleFx.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	31 Oct 2005
//	WHAT:	Implementation of the Rage Particle effects
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Vfx/Particles/PtFxManager.h"

// Rage headers
#include "audiosoundtypes/sound.h"
#include "bank/bkmgr.h"
#include "grcore/allocscope.h"
#include "grcore/gfxcontext.h"
#include "grcore/im.h"
#include "grcore/texturegcm.h"
#include "grprofile/timebars.h"
#include "rmptfx/ptxdrawlist.h"
#include "rmptfx/ptxInterface.h"
#include "rmptfx/ptxconfig.h"
#include "rmptfx/ptxmanager.h"
#include "rmptfx/ptxu_acceleration.h"
#include "rmptfx/ptxu_dampening.h"
#include "script/thread.h"
#include "system/param.h"
#include "system/memops.h"
#include "system/memory.h"
#include "vectormath/classes.h"

// Framework headers
#include "fwscene/world/worldmgr.h"
#include "fwscene/search/Search.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxasset.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxconfig.h"
#include "vfx/ptfx/ptfxdebug.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxscript.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/ptfx/behaviours/ptxu_decal.h"
#include "vfx/ptfx/behaviours/ptxu_decalpool.h"
#include "vfx/ptfx/behaviours/ptxu_decaltrail.h"
#include "vfx/ptfx/behaviours/ptxu_fire.h"
#include "vfx/ptfx/behaviours/ptxu_fogvolume.h"
#include "vfx/ptfx/behaviours/ptxu_light.h"
#include "vfx/ptfx/behaviours/ptxu_liquid.h"
#include "vfx/ptfx/behaviours/ptxu_river.h"
#include "vfx/ptfx/behaviours/ptxu_zcull.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"

// Game headers
#include "Audio/ambience/ambientaudioentity.h"
#include "Camera/CamInterface.h"
#include "Camera/Cinematic/CinematicDirector.h"
#include "camera/base/BaseCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Viewports/ViewportManager.h"
#include "Control/replay/replay.h"
#include "Control/replay/effects/ParticleMiscFxPacket.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Game/Wind.h"
#include "ModelInfo/VehicleModelInfoFlags.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Physics/Physics.h"
#include "Renderer/Deferred/DeferredLighting.h"
#include "Renderer/DrawLists/drawListMgr.h"
#include "Renderer/Lights/AsyncLightOcclusionMgr.h"
#include "Renderer/Lights/Lights.h"
#include "Renderer/Mirrors.h"
#include "Renderer/Occlusion.h"
#include "Renderer/PostProcessFX.h"
#include "Renderer/RenderPhases/RenderPhaseReflection.h"
#include "Renderer/rendertargets.h"
#include "Renderer/RenderThread.h"
#include "Renderer/RenderPhases/RenderPhaseCascadeShadows.h"
#include "renderer/RenderPhases/RenderPhaseMirrorReflection.h"
#include "Renderer/River.h"
#include "weapons/projectiles/Projectile.h"
#if __PPU
#include "Renderer/Shadows/Shadows.h"
#endif
#include "Renderer/Util/ShaderUtil.h"
#include "Scene/DataFileMgr.h"
#include "Scene/World/GameWorld.h"
#include "Scene/World/GameWorldHeightMap.h"
#include "Script/Script.h"
#include "Script/Handlers/GameScriptResources.h"
#include "Shaders/ShaderLib.h"
#include "Scene/portals/Portal.h"
#include "Streaming/Streaming.h"
#include "System/ControlMgr.h"
#if RSG_PC
#include "System/SettingsManager.h"
#endif
#include "System/ThreadPriorities.h"
#include "TimeCycle/TimeCycle.h"
#include "TimeCycle/TimeCycleConfig.h"
#include "VehicleAI/Task/TaskVehicleAnimation.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Metadata/PtFxAssetInfo.h"
#include "Vfx/Misc/Coronas.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Misc/FogVolume.h"
#include "Vfx/Particles/PtFxDebug.h"
#include "Vfx/Particles/PtFxEntity.h"
#include "Vfx/Systems/VfxGlass.h"
#include "Vfx/Systems/VfxLens.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Systems/VfxTrail.h"
#include "Vfx/Systems/VfxWater.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/VfxSettings.h"
#include "Vfx/Sky/Sky.h"
#include "Weapons/Weapon.h"
#include "control/replay/ReplaySettings.h"

#if __D3D11
#include "grcore/rendertarget_d3d11.h"
#endif
#if RSG_ORBIS
#include "grcore/rendertarget_gnm.h"
#include "grcore/texturefactory_gnm.h"
#endif

#if PTFX_DYNAMIC_QUALITY
#include "grcore/gputimer.h"
grcGpuTimer g_ptfxGPUTimerMedRes;
grcGpuTimer g_ptfxGPUTimerLowRes;
#endif

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()
RMPTFX_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#if __D3D11 || RSG_ORBIS
#define DEFAULT_TESSELLATION_FACTOR	(5.0f)
#define DEFAULT_SHADOW_INTENSITY (0.5f)
#endif

#define PTFX_ENABLE_SHADOW_CULL_STATE_UPDATE		(0)

#define PTFX_WATER_REFRACTION_ENABLED				(0)

#define PTXSTR_GAME_PROGVAR_POST_PROCESS_FLIPDEPTH_NEARPLANEFADE_PARAM	"postProcess_FlipDepth_NearPlaneFade_Params"
#define PTXSTR_GAME_PROGVAR_WATER_FOG_PARAM								"waterfogPtfxParams"
#define PTXSTR_GAME_PROGVAR_WATER_FOG_COLOR_BUFFER						"waterColorTexture"
#define PTXSTR_GAME_PROGVAR_SHADEDMAP									"ShadedPtFxMap"
#define PTXSTR_GAME_PROGVAR_SHADEDPARAMS								"gShadedPtFxParams"
#define PTXSTR_GAME_PROGVAR_SHADEDPARAMS2								"gShadedPtFxParams2"
#define PTXSTR_GAME_PROGVAR_FOG_RAY_MAP									"FogRayMap"
#define PTXSTR_GAME_PROGVAR_ACTIVE_SHADOW_CASCADES						"activeShadowCascades"
#define PTXSTR_GAME_PROGVAR_NUM_ACTIVE_SHADOW_CASCADES					"numActiveShadowCascades"

///////////////////////////////////////////////////////////////////////////////
//  ENUMERATIONS
///////////////////////////////////////////////////////////////////////////////

enum PtFxDataVolumeTypes
{
	PTFX_DATAVOLUMETYPE_FIRE_CONTAINED			= 0,
	PTFX_DATAVOLUMETYPE_FIRE,
	PTFX_DATAVOLUMETYPE_WATER,
	PTFX_DATAVOLUMETYPE_SAND,
	PTFX_DATAVOLUMETYPE_MUD,
	PTFX_DATAVOLUMETYPE_BLOOD,
	PTFX_DATAVOLUMETYPE_DUST,
	PTFX_DATAVOLUMETYPE_SMOKE,
	PTFX_DATAVOLUMETYPE_BLAZE,
	PTFX_DATAVOLUMETYPE_SOAK_SLOW,
	PTFX_DATAVOLUMETYPE_SOAK_MEDIUM,
	PTFX_DATAVOLUMETYPE_SOAK_FAST,
	PTFX_DATAVOLUMETYPE_FIRE_FLAMETHROWER,
};


enum ptfxBucketRenderStage
{
	PTFX_BUCKET_RENDER_STAGE_NORMAL = 0,
	PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION,
	PTFX_BUCKET_RENDER_STAGE_REFRACTION,
	PTFX_BUCKET_RENDER_STAGE_SHADOWS,
	PTFX_BUCKET_RENDER_STAGE_SIMPLE,
	PTFX_BUCKET_RENDER_STAGE_TOTAL
};

///////////////////////////////////////////////////////////////////////////////
//  EXTERNALLY DECLARED VARIABLES
///////////////////////////////////////////////////////////////////////////////

#if __BANK
extern bool gLastGenMode;
#endif


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CPtFxManager g_ptFxManager;	 

CPtFxCallbacks g_ptFxCallbacks;

// params
#if __DEV
XPARAM(ptfxloadall);
PARAM(ptfxloadmost, "loads most ptfx assets - skips any rayfire and script assets");
#endif
#if !__FINAL
XPARAM(ptfxassetoverride);
#endif
// We can't use timebars all the time because there are some frames where the ptx update never gets called (e.g. when paused) and not 
// calling the timebar functions for every thread every frame throws off the absolute time display
PARAM(ptfxtimebar, "display the ptfx timebar along with all the others");

// settings
dev_float PTFX_SOAK_SLOW_INC_LEVEL_LOW = 0.0f;
dev_float PTFX_SOAK_SLOW_INC_LEVEL_HIGH = 0.06f;

dev_float PTFX_SOAK_MEDIUM_INC_LEVEL_LOW = 0.0f;
dev_float PTFX_SOAK_MEDIUM_INC_LEVEL_HIGH = 0.3f;

dev_float PTFX_SOAK_FAST_INC_LEVEL_LOW = 0.0f;
dev_float PTFX_SOAK_FAST_INC_LEVEL_HIGH = 1.5f;

__THREAD ptfxBucketRenderStage g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_NORMAL;
__THREAD u8 g_PtfxWaterSurfaceRenderState = PTFX_WATERSURFACE_RENDER_BOTH;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CLASS CPtfxShadowDrawToAllCascades  
///////////////////////////////////////////////////////////////////////////////
class CPtfxShadowDrawToAllCascades : public ptxDrawInterface::ptxMultipleDrawInterface
{
public:
	CPtfxShadowDrawToAllCascades(s32 noOfCascades, grcViewport *pViewports, Vec4V *pWindows, bool bUseInstancedShadow)
	{
		m_noOfCascades = noOfCascades;
		m_pViewports = pViewports;
		m_pWindows = pWindows;
		m_bUseInstancedShadow = bUseInstancedShadow;
		m_uCulled = 0;
	}
	~CPtfxShadowDrawToAllCascades()
	{
	}
	u32 NoOfDraws()
	{
		return (u32)m_noOfCascades;
	}
	void PreDraw(u32 i)
	{
		const Vec4V temp = FloatToIntRaw<0>(m_pWindows[i]);
		const int x = temp.GetXi();
		const int y = temp.GetYi();
		const int w = temp.GetZi();
		const int h = temp.GetWi();

		grcViewport::SetCurrent((const grcViewport *)&m_pViewports[i],false);
		grcViewport::SetCurrentWorldIdentity();
		grcViewport::GetCurrent()->ResetWindow();
		grcViewport::GetCurrent()->SetWindow(x, y, w, h);

#if RSG_DURANGO || RSG_ORBIS
		GRCDEVICE.SetProgramResources();
#endif

#if CASCADE_SHADOW_TEXARRAY
		grcTextureFactory::GetInstance().SetArrayView(i);
#endif //CASCADE_SHADOW_TEXARRAY
	}
	void PreDrawInstanced()
	{
		//do nothing
	}
	void PostDraw(u32)
	{
		g_ptFxManager.SetParticleShadowsRenderedFlag(true);
	}
	void PostDrawInstanced()
	{
		g_ptFxManager.SetParticleShadowsRenderedFlag(true);
	}


	grcViewport *GetViewport(int i)		{ return (grcViewport *)&m_pViewports[i]; }
	bool IsUsingInstanced()	{ return m_bUseInstancedShadow; }
	void SetCulledState(bool Culled, u32 i) 
	{ 
		Culled ? m_uCulled |= (1<<i) : m_uCulled &= ~(1<<i);
	}
	bool GetCulledState(int i)	{ return ((1 << i) & m_uCulled) ? true : false; }
	bool ShouldBeRendered(ptxEffectInst* pEffectInst)
	{ 
#if PTFX_ENABLE_SHADOW_CULL_STATE_UPDATE
		return (RMPTFX_EDITOR_ONLY(RMPTFXMGR.GetInterface().IsConnected() || ) (pEffectInst && pEffectInst->GetEffectRule() && !pEffectInst->GetEffectRule()->GetHasNoShadows() && !pEffectInst->GetDataDBMT(RMPTFXTHREADER.GetDrawBufferId())->GetCustomFlag(PTFX_DRAW_SHADOW_CULLED)));
#else
		return (!pEffectInst->GetDataDBMT(RMPTFXTHREADER.GetDrawBufferId())->GetCustomFlag(PTFX_DRAW_SHADOW_CULLED));
#endif
	}

	void SetNumActiveCascades(u8 numActiveCascades) { m_numActiveCascades = numActiveCascades;}
	u32 GetNumActiveCascades() { return m_numActiveCascades;}
public:
	s32 m_noOfCascades;
	grcViewport *m_pViewports;
	Vec4V *m_pWindows;
	bool m_bUseInstancedShadow;
	u8 m_uCulled;
	u8 m_numActiveCascades;
};

///////////////////////////////////////////////////////////////////////////////
// CLASS CSetLightingConstants  
///////////////////////////////////////////////////////////////////////////////
bool CSetLightingConstants::ms_defShadingConstantsCached = false;
grcEffectVar CSetLightingConstants::ms_shaderVarIdDepthTexture		= grcevNONE;
grcEffectVar CSetLightingConstants::ms_shaderVarIdFogRayTexture		= grcevNONE;
grcEffectVar CSetLightingConstants::ms_shaderVarIdOOScreenSize		= grcevNONE;
grcEffectVar CSetLightingConstants::ms_shaderVarIdProjParams		= grcevNONE;
grcEffectVar CSetLightingConstants::ms_shaderVarIdShearParams0		= grcevNONE;
grcEffectVar CSetLightingConstants::ms_shaderVarIdShearParams1		= grcevNONE;
grcEffectVar CSetLightingConstants::ms_shaderVarIdShearParams2		= grcevNONE;
grcEffectTechnique	CSetLightingConstants::ms_projSpriteTechniqueId = grcetNONE;

bool CSetLightingConstants::PreDraw(const ptxEffectInst* pEffectInst, const ptxEmitterInst* pEmitterInst, Vec3V_In vMin_In, Vec3V_In vMax_In, u32 shaderHashName, grmShader* pShader) const
{
	if (pEmitterInst != NULL)
	{
		const ptxParticleRule* pEffectRule = pEmitterInst->GetParticleRule();
		if (pEffectRule != NULL)
		{
			// get the technique id for this effect, we only want to set these shader constants
			// for a specific set of techniques
			grcEffectTechnique curTechnique = pEffectRule->GetShaderInst().GetShaderTemplateTechnique();
			SetDeferredShadingGlobals(pShader, curTechnique);
		}
	}

	if( g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SHADOWS )
		return true;

	Vec3V vMin = vMin_In;
	Vec3V vMax = vMax_In;

	if (m_unlitShader != shaderHashName)
	{
		spdAABB box(vMin, vMax);

		// we need to force light shader constants to be pushed, otherwise these will contain
		// whatever values were set last (which might be invalid if there are no active lights).
		Lights::UseLightsInArea(box, pEffectInst->GetInInterior(), true, false, true);
		CShaderLib::SetGlobalInInterior(pEffectInst->GetInInterior());
		
		// set ambient globals
		Lights::m_lightingGroup.SetAmbientLightingGlobals();

		if (pEffectInst != NULL)
		{
			float natAmbScale, artAmbScale;
			pEffectInst->GetAmbientScales(natAmbScale, artAmbScale);
			CShaderLib::SetGlobalNaturalAmbientScale(natAmbScale);
			CShaderLib::SetGlobalArtificialAmbientScale(artAmbScale);
		}

	}

	if (ms_defShadingConstantsCached == false)
	{
		CacheDeferredShadingGlobals(pShader);
	}
	
	
	return true;
}

void CSetLightingConstants::CacheDeferredShadingGlobals(grmShader* pShader)
{
	bool bOk;

	if (ms_projSpriteTechniqueId == grcetNONE)
	{
		ms_projSpriteTechniqueId = pShader->LookupTechnique("RGBA_proj", false);
	}

	ms_shaderVarIdDepthTexture = pShader->LookupVar("gbufferTextureDepth", false);
	bOk = ms_shaderVarIdDepthTexture != grcevNONE;
	ms_shaderVarIdFogRayTexture = pShader->LookupVar("FogRayMap", false);
	bOk = ms_shaderVarIdFogRayTexture != grcevNONE;
	ms_shaderVarIdOOScreenSize = pShader->LookupVar("deferredLightScreenSize", false);
	bOk = bOk && ms_shaderVarIdOOScreenSize != grcevNONE;
	ms_shaderVarIdProjParams = pShader->LookupVar("deferredProjectionParams", false);
	bOk = bOk && ms_shaderVarIdProjParams != grcevNONE;
	ms_shaderVarIdShearParams0 = pShader->LookupVar("deferredPerspectiveShearParams0", false);
	bOk = bOk && ms_shaderVarIdShearParams0 != grcevNONE;
	ms_shaderVarIdShearParams1 = pShader->LookupVar("deferredPerspectiveShearParams1", false);
	bOk = bOk && ms_shaderVarIdShearParams1 != grcevNONE;
	ms_shaderVarIdShearParams2 = pShader->LookupVar("deferredPerspectiveShearParams2", false);
	bOk = bOk && ms_shaderVarIdShearParams2 != grcevNONE;

	ms_defShadingConstantsCached = bOk; 
}

void CSetLightingConstants::SetDeferredShadingGlobals(grmShader* pShader, grcEffectTechnique techniqueId)
{
	// if constant ids are not cached or the current technique is not a projection one
	// we cannot set the constants
	if (ms_defShadingConstantsCached == false || techniqueId != ms_projSpriteTechniqueId )
	{
		return;
	}

	pShader->SetVar(ms_shaderVarIdDepthTexture, 
#if __PPU
		CRenderTargets::GetDepthBufferAsColor() );
#else
# if DEVICE_MSAA
		GRCDEVICE.GetMSAA() ? CRenderTargets::GetDepthResolved() :
# endif
		CRenderTargets::GetDepthBuffer() );
#endif // platforms

	// calc and set the one over screen size shader var
	Vec4V vOOScreenSize(0.0f, 0.0f, 1.0f/GRCDEVICE.GetWidth(), 1.0f/GRCDEVICE.GetHeight());
	pShader->SetVar(ms_shaderVarIdOOScreenSize, RCC_VECTOR4(vOOScreenSize));
	
	Vec4V projParams;
	Vec3V shearProj0, shearProj1, shearProj2;
	DeferredLighting::GetProjectionShaderParams(projParams, shearProj0, shearProj1, shearProj2);

	pShader->SetVar(ms_shaderVarIdProjParams, projParams);
	pShader->SetVar(ms_shaderVarIdShearParams0, shearProj0);
	pShader->SetVar(ms_shaderVarIdShearParams1, shearProj1);
	pShader->SetVar(ms_shaderVarIdShearParams2, shearProj2);

	pShader->SetVar(ms_shaderVarIdFogRayTexture, PostFX::GetFogRayRT());
}

///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxCallbacks
///////////////////////////////////////////////////////////////////////////////

void CPtFxCallbacks::GetMatrixFromBoneIndex(Mat34V_InOut boneMtx, const void* pEntity, int boneIndex, bool useAltSkeleton)
{
	const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pActualEntity, boneIndex, useAltSkeleton);
}

void CPtFxCallbacks::UpdateFxInst(ptxEffectInst* pFxInst)
{
	// test if this effect has any additional data attached to it that needs processed (e.g. fires)
	// rmptfx is storing enough data to create a capsule
	if (pFxInst->GetIsPlaying() && pFxInst->GetUseDataVolume())
	{
		g_ptFxManager.UpdateDataCapsule(pFxInst);
	}

	// cache ambient scalars and interior location
	if (pFxInst->GetIsFinished() == false)
	{
		Vec3V vPos = pFxInst->GetCurrPos();	
		Vec3V prevPos = pFxInst->GetPrevPos();	
		const bool bHasPtfxMoved = (IsEqualAll(vPos, prevPos) == 0);

		fwEntity* pAttachEntity = ptfxAttach::GetAttachEntity(pFxInst);
		CEntity* pEntity = static_cast<CEntity*>(pAttachEntity);
		const bool bFirstTimeUpdate = (false == pFxInst->GetAmbientScalesSetup());

		if (g_ptFxManager.GetForceVehicleInteriorFlag())
		{
			pFxInst->GetDataDB()->SetCustomFlag(PTFX_IS_VEHICLE_INTERIOR);
		}
		else
		{
			pFxInst->GetDataDB()->ClearCustomFlag(PTFX_IS_VEHICLE_INTERIOR);
			if(pEntity)
			{
				//Let's check if the effect inst is attached to the vehicle
				if(pEntity->GetIsTypeVehicle())
				{
					//If so, check if is the current vehicle that the camera is in 
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
					if(pVehicle->IsCamInsideVehicleInterior() && pFxInst->GetFlag(PTFX_EFFECTINST_FORCE_VEHICLE_INTERIOR))
					{
						//Let's set the flag when the current vehicle has the camera in it and the effect inst has the force vehicle interior flag set
						pFxInst->GetDataDB()->SetCustomFlag(PTFX_IS_VEHICLE_INTERIOR);
					}				
				}
				else if(CVfxHelper::IsEntityInCurrentVehicle(pEntity))
				{
					//Check if the effect inst is attached to weapon/ped inside the vehicle
					//we wont get into this scope if entity is a vehicle due to previous condition
					pFxInst->GetDataDB()->SetCustomFlag(PTFX_IS_VEHICLE_INTERIOR);
				}
				else if (pFxInst->GetFlag(PTFX_EFFECTINST_FORCE_VEHICLE_INTERIOR))
				{
					// check for effect inst indirectly attached to current vehicle or weapon/ped inside it
					CPed* pPed = const_cast<CPed*>(camInterface::GetGameplayDirector().GetFollowPed());
					if (pPed && CVfxHelper::IsEntityInCurrentVehicle(pPed) )
						pFxInst->GetDataDB()->SetCustomFlag(PTFX_IS_VEHICLE_INTERIOR);
				}
			}
		}

		//Always update the first time for every effect instance or if the ptfx has moved
		if(bHasPtfxMoved || bFirstTimeUpdate)
		{
			const bool bShortLiveEffect = pFxInst->GetEffectRule()->GetIsShortLived();
			bool bGetAmbientScalesFromAttachedEntity = false;
			bool bGetInteriorFromAttachedEntity = false;

			fwInteriorLocation interiorLocation;

			// If we've got a valid cached result for interior test, use it
			if(pFxInst->GetIsInteriorLocationCached())
			{
				u32 packedInteriorLocation = pFxInst->GetInteriorLocation();
				interiorLocation.SetAsUint32(packedInteriorLocation);
			}
			else if(pEntity)
			{
				//If the ptfx is attached to the entity then we set the ambient scales and interior from the entity
				//This does not cost anything

				//Uncomment the third part if we want particles not use artist set
				// ambient scales for certain entities
				if(pEntity->GetUseAmbientScale() /* && pEntity->GetUseDynamicAmbientScale()*/)
				{
					bGetAmbientScalesFromAttachedEntity = true;
					pFxInst->SetAmbientScales(CShaderLib::DivideBy255(pEntity->GetNaturalAmbientScale()), CShaderLib::DivideBy255(pEntity->GetArtificialAmbientScale()));
					pFxInst->SetAmbientScalesSetup(true);
				}
				interiorLocation = pEntity->GetInteriorLocation();
				pFxInst->SetInInterior(interiorLocation.IsValid());
				pFxInst->SetInteriorLocation(interiorLocation.GetAsUint32());
				bGetInteriorFromAttachedEntity = true;
			}

			//Proceed if its not a short lived effect or if its the first update or if it has moved
			if(bFirstTimeUpdate || bHasPtfxMoved || !bShortLiveEffect)
			{
				if(!(bGetInteriorFromAttachedEntity || pFxInst->GetIsInteriorLocationCached()) )
				{
					interiorLocation = CVfxHelper::GetCamInteriorLocation();
					pFxInst->SetInInterior(interiorLocation.IsValid());
					pFxInst->SetInteriorLocation(interiorLocation.GetAsUint32());
				}
				//Set ambient scales if not set already by the attached effect
				if(!bGetAmbientScalesFromAttachedEntity)
				{
					Vec2V vAmbientScale = g_timeCycle.CalcAmbientScale(vPos, interiorLocation);
					pFxInst->SetAmbientScales(vAmbientScale.GetXf(), vAmbientScale.GetYf());
					pFxInst->SetAmbientScalesSetup(true);
				}
			}			
		}
	}
}

bool CPtFxCallbacks::RemoveFxInst(ptxEffectInst* pFxInst)
{

	//Clearing these custom flags to avoid asserts
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_REFRACT_RULE);
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_WATER_ABOVE_RULE);
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_WATER_BELOW_RULE);

#if PTFX_ALLOW_INSTANCE_SORTING
	// can this be moved to CPtFxSortedEntity?
	if (pFxInst->GetFlag(PTFX_RESERVED_SORTED))
	{
		// finish the effect - it's still reserved so won't deactivate until a subsequent update loop
		pFxInst->Finish(false);
		return true;
	}
#endif

	return false;
}

void CPtFxCallbacks::ShutdownFxInst(ptxEffectInst* pFxInst)
{
#if PTFX_ALLOW_INSTANCE_SORTING
	if (pFxInst->GetFlag(PTFX_RESERVED_SORTED))
	{
		g_ptFxManager.GetSortedInterface().Remove(pFxInst);
	}
#endif
	//Clearing these custom flags to avoid asserts
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_REFRACT_RULE);
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_WATER_ABOVE_RULE);
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_WATER_BELOW_RULE);
}

void CPtFxCallbacks::RemoveScriptResource(int ptfxScriptId)
{
	// check if the script effect exists - it may have been removed at the start of this chain of events
	if (ptfxScript::DoesExist(ptfxScriptId))
	{
		ptfxScript::SetRemoveWhenDestroyed(ptfxScriptId, false);
		CTheScripts::GetScriptHandlerMgr().RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_PTFX, ptfxScriptId);
	}
}

Vec3V_Out CPtFxCallbacks::GetGameCamPos()
{
	return CVfxHelper::GetGameCamPos();
}

float CPtFxCallbacks::GetGameCamWaterZ()
{
	return CVfxHelper::GetGameCamWaterZ();
}

float CPtFxCallbacks::GetWaterZ(Vec3V_In vPos, ptxEffectInst* pEffectInst)
{
	fwEntity* pEntity = ptfxAttach::GetAttachEntity(pEffectInst);
	float waterZ;
	CVfxHelper::GetWaterZ(vPos, waterZ, pEntity);
	return waterZ;
}

void CPtFxCallbacks::GetRiverVel(Vec3V_In vPos, Vec3V_InOut vVel)
{
	// get the river flow at this position
	Vector2 riverFlow(0.0f, 0.0f);
	River::GetRiverFlowFromPosition(vPos, riverFlow);
	vVel = Vec3V(riverFlow.x, riverFlow.y, 0.0f);
}

float CPtFxCallbacks::GetGravitationalAcceleration()
{
	return CPhysics::GetGravitationalAcceleration();
}

float CPtFxCallbacks::GetAirResistance()
{
	return g_vfxSettings.GetPtFxAirResistance();
}

void CPtFxCallbacks::AddLight(ptfxLightInfo& lightInfo)
{
	g_ptFxManager.StoreLight(lightInfo);
}

void CPtFxCallbacks::AddFire(ptfxFireInfo& fireInfo)
{
	g_ptFxManager.StoreFire(fireInfo);	
}

void CPtFxCallbacks::AddDecal(ptfxDecalInfo& decalInfo)
{
	g_ptFxManager.StoreDecal(decalInfo);
}

void CPtFxCallbacks::AddDecalPool(ptfxDecalPoolInfo& decalPoolInfo)
{
	g_ptFxManager.StoreDecalPool(decalPoolInfo);
}

void CPtFxCallbacks::AddDecalTrail(ptfxDecalTrailInfo& decalTrailInfo)
{
	g_ptFxManager.StoreDecalTrail(decalTrailInfo);
}

void CPtFxCallbacks::AddLiquid(ptfxLiquidInfo& liquidInfo)
{
	g_ptFxManager.StoreLiquid(liquidInfo);
}

void CPtFxCallbacks::AddFogVolume(ptfxFogVolumeInfo& fogVolumeInfo)
{
	g_ptFxManager.StoreFogVolume(fogVolumeInfo);
}

// render behaviors
void CPtFxCallbacks::DrawFogVolume(ptfxFogVolumeInfo& fogVolumeInfo)
{
	g_ptFxManager.DrawFogVolume(fogVolumeInfo);
}

bool CPtFxCallbacks::CanGetTriggeredInst()
{
#if GTA_REPLAY
	if (CReplayMgr::IsEditModeActive() && !CReplayMgr::IsPreCachingScene())
	{
		if (CReplayMgr::GetCurrentPlayBackState().IsSet(REPLAY_STATE_PAUSE) ||
			CReplayMgr::GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK))
		{
			return false;
		}
	}
#endif	// GTA_REPLAY

	return true;
}

bool CPtFxCallbacks::CanGetRegisteredInst(bool bypassReplayPauseCheck)
{
#if	GTA_REPLAY
	if (CReplayMgr::IsEditModeActive() && !CReplayMgr::IsPreCachingScene())
	{
		if ((CReplayMgr::GetCurrentPlayBackState().IsSet(REPLAY_STATE_PAUSE) && !bypassReplayPauseCheck) ||
			CReplayMgr::GetCurrentPlayBackState().IsSet(REPLAY_DIRECTION_BACK))
		{
			return false;
		}
	}
#endif	// GTA_REPLAY

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtFxShaderVarUtils
///////////////////////////////////////////////////////////////////////////////

const char* CPtFxShaderVarUtils::sm_progVarNameTable[PTXPROGVAR_GAME_COUNT] = 
{	
	PTXSTR_GAME_PROGVAR_POST_PROCESS_FLIPDEPTH_NEARPLANEFADE_PARAM,
	PTXSTR_GAME_PROGVAR_WATER_FOG_PARAM,
	PTXSTR_GAME_PROGVAR_WATER_FOG_COLOR_BUFFER,
	PTXSTR_GAME_PROGVAR_SHADEDMAP,
	PTXSTR_GAME_PROGVAR_SHADEDPARAMS,
	PTXSTR_GAME_PROGVAR_SHADEDPARAMS2,
	PTXSTR_GAME_PROGVAR_FOG_RAY_MAP,
	PTXSTR_GAME_PROGVAR_ACTIVE_SHADOW_CASCADES,
	PTXSTR_GAME_PROGVAR_NUM_ACTIVE_SHADOW_CASCADES,
};

const char* CPtFxShaderVarUtils::GetProgrammedVarName(ptxShaderProgVarType type) 
{
	ptxAssertf((type>=PTXPROGVAR_GAME_FIRST && type<PTXPROGVAR_COUNT), "programmed shader var out of range");
	return sm_progVarNameTable[type - PTXPROGVAR_GAME_FIRST];
}

///////////////////////////////////////////////////////////////////////////////
//  CLASS CPtfxShaderCallbacks
///////////////////////////////////////////////////////////////////////////////


void CPtfxShaderCallbacks::InitProgVars(ptxShaderTemplate* pPtxShaderTemplate)
{
	if(pPtxShaderTemplate == NULL)
	{
		return;
	}

	for (int i=PTXPROGVAR_GAME_FIRST; i<PTXPROGVAR_COUNT; i++)
	{
		ptxShaderProgVarType shaderVarType = static_cast<ptxShaderProgVarType>(i);
		grcEffectVar shaderVarHandle = pPtxShaderTemplate->GetGrmShader()->LookupVar(CPtFxShaderVarUtils::GetProgrammedVarName(shaderVarType), false);
		pPtxShaderTemplate->InitProgVar(shaderVarType, shaderVarHandle);
	}
}
///////////////////////////////////////////////////////////////////////////////
//	SetShaderVariables
///////////////////////////////////////////////////////////////////////////////

void CPtfxShaderCallbacks::SetShaderVars(ptxShaderTemplate* pPtxShaderTemplate)
{
	if(pPtxShaderTemplate == NULL)
	{
		return;
	}

	//Use underwater post process only when camera is above water and we're rendering underwater ptfx.
	bool bUseUnderwaterPostProcess = g_ptFxManager.IsRenderingIntoWaterRefraction() && !Water::IsCameraUnderwater() BANK_ONLY(&& !g_ptFxManager.GetGameDebugInterface().GetDisableWaterRefractionPostProcess());

	//use water refraction gamma correction when rendering into the refraction target.
	bool bRefractionGammaCorrection = g_ptFxManager.IsRenderingIntoWaterRefraction() BANK_ONLY(&& !g_ptFxManager.GetGameDebugInterface().GetDisableWaterRefractionPostProcess());
	//Let's flip the depth lookup when rendering particles in Mirror reflections renderphase
	const bool bFlipDepth = (g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION);
	const bool bApplyNearPlaneFade = (g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION);
	const Vec4V postProcess_FlipdDepth_NearPlaneFadeParams = Vec4V(
		bUseUnderwaterPostProcess?1.0f:0.0f, 
		bRefractionGammaCorrection?1.0f:0.0f, 
		bFlipDepth?1.0f:0.0f,
		bApplyNearPlaneFade?1.0f:0.0f);

	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_POST_PROCESS_FLIPDEPTH_NEARPLANEFADE_PARAM, postProcess_FlipdDepth_NearPlaneFadeParams);
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_WATER_FOG_PARAM, CVfxHelper::GetWaterFogParams());
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_WATER_FOG_COLOR_BUFFER, CVfxHelper::GetWaterFogTexture());
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_FOG_MAP_PARAM, PostFX::FogRayRT);
#if USE_SHADED_PTFX_MAP
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_SHADEDMAP, g_ptFxManager.GetPtfxShadedMap());
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_SHADEDPARAMS, g_ptFxManager.GetPtfxShadedParams());
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_SHADEDPARAMS2, g_ptFxManager.GetPtfxShadedParams2());
#endif

	ptxDrawInterface::ptxMultipleDrawInterface* pMultiDrawInterface = g_ptFxManager.GetMultiDrawInterface();
	Vec4V activeShadowCascades = Vec4V(0.0f,0.0f,0.0f,0.0f);
	float numActiveShadowCascades = 0;
	if(pMultiDrawInterface)
	{
		u32 currentIndex = 0;
		for(u32 i = 0; i < pMultiDrawInterface->NoOfDraws(); i++)
		{
			if( pMultiDrawInterface->GetCulledState(i) == false)
			{
				Assert(currentIndex < 4);
				activeShadowCascades[currentIndex++] = (float)i;
			}
		}
		numActiveShadowCascades = (float)pMultiDrawInterface->GetNumActiveCascades();
		Assert(numActiveShadowCascades == currentIndex);
	}
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_ACTIVE_SHADOW_CASCADES, activeShadowCascades);
	pPtxShaderTemplate->SetShaderVar(PTXPROGVAR_NUM_ACTIVE_SHADOW_CASCADES, Vec4V(numActiveShadowCascades, 1.0f/numActiveShadowCascades, 0.0f, 0.0f));

}

///////////////////////////////////////////////////////////////////////////////
//  CLASS PtFx
///////////////////////////////////////////////////////////////////////////////

#if RMPTFX_DEBUG_PRINT_SHADER_NAMES_AND_PASSES

#define STRINGIFY(x) #x
#define ON 1
#define OFF 0


// C version of macros in ptfx_sprite,fx.
#define DEF_Technique(diffuseMode, bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract) \
{ "ptfx_sprite", STRINGIFY(diffuseMode), { bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract }, 0 },

#define DEF_TechniqueVariant(diffuseMode, variant, bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract) \
{ "ptfx_sprite", STRINGIFY(diffuseMode##_##variant), { bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract }, 0 },

#define DEF_TechniqueVariant_Tessellated(diffuseMode, variant, bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract) \
{ "ptfx_sprite", STRINGIFY(diffuseMode##_##variant), { bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract }, 0 },

#define DEF_TechniqueVariantVS(diffuseMode, variant, vs_variant, bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract) \
{ "ptfx_sprite", STRINGIFY(diffuseMode##_##variant), { bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract }, 0 },

#define DEF_TechniqueVariantVSPS(diffuseMode, variant, vs_variant, ps_variant, bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract) \
{ "ptfx_sprite", STRINGIFY(diffuseMode##_##variant), { bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract }, 0 },

#define DEF_TechniqueNameVSPS(name, vs, ps, bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract) \
{ "ptfx_sprite", STRINGIFY(name), { bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasubtract }, 0 },

// C version of macros in ptfx_trail.fx
#define DEF_ptfxTrailTechnique(tech, vs, ps, bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasub) \
{ "ptfx_trail", STRINGIFY(tech), { bm_default, bm_normal, bm_add, bm_subtract, bm_compalpha, bm_compalphasub }, 0 },

ptxShaderTemplate::RMPTFX_DEBUG_PRINT_SHADER_NAMES_AND_PASSES_FILTER g_DebugPrintFilter[] =
{
	// sprite
	#include "shader_source/VFX/ptfx_sprite.fxh"
	// trail
	#include "shader_source/VFX/ptfx_trail.fxh"
	{ (char *)NULL, (char *)NULL, 0, 0 },
};

#endif //RMPTFX_DEBUG_PRINT_SHADER_NAMES_AND_PASSES

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::Init(unsigned initMode)
{
    if(initMode == INIT_CORE)
    {
		// depth/stencil blocks
		grcDepthStencilStateDesc depthStencilDesc;
		grcDepthStencilStateDesc mirrorReflection_simple_DepthStencilDesc;
		depthStencilDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);

		depthStencilDesc.StencilEnable = true;
		depthStencilDesc.StencilReadMask = DEFERRED_MATERIAL_INTERIOR_VEH;
		depthStencilDesc.StencilWriteMask = 0xf0;
		depthStencilDesc.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
		depthStencilDesc.BackFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		depthStencilDesc.BackFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
		depthStencilDesc.BackFace.StencilPassOp = grcRSV::STENCILOP_KEEP;
		depthStencilDesc.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
		depthStencilDesc.FrontFace.StencilDepthFailOp = grcRSV::STENCILOP_KEEP;
		depthStencilDesc.FrontFace.StencilFailOp = grcRSV::STENCILOP_KEEP;
		depthStencilDesc.FrontFace.StencilPassOp = grcRSV::STENCILOP_KEEP;

		//not applying fixup for mirror reflection and simple render phase
		//Once the fixup is applied to those render phase we need to apply the fixup here also
		mirrorReflection_simple_DepthStencilDesc.DepthFunc = grcRSV::CMP_LESSEQUAL;
		mirrorReflection_simple_DepthStencilDesc.StencilEnable = false;
		


		//////////////////////////////////////////////////////////////////////////
		// Stencil On
		// write off, test off
		depthStencilDesc.DepthWriteMask = 0;
		depthStencilDesc.DepthEnable = 0;
		m_VehicleInteriorAlphaDepthStencilStates[0][0] = grcStateBlock::CreateDepthStencilState(depthStencilDesc);

		// write off, test on
		depthStencilDesc.DepthWriteMask = 0;
		depthStencilDesc.DepthEnable = 1;
		m_VehicleInteriorAlphaDepthStencilStates[0][1] = grcStateBlock::CreateDepthStencilState(depthStencilDesc);

		// write on, test off						   
		depthStencilDesc.DepthWriteMask = 1;		   
		depthStencilDesc.DepthEnable = 0;			   
		m_VehicleInteriorAlphaDepthStencilStates[1][0] =  grcStateBlock::CreateDepthStencilState(depthStencilDesc);

		// write on, test on						   
		depthStencilDesc.DepthWriteMask = 1;		   
		depthStencilDesc.DepthEnable = 1;			   
		m_VehicleInteriorAlphaDepthStencilStates[1][1] = grcStateBlock::CreateDepthStencilState(depthStencilDesc);

		//////////////////////////////////////////////////////////////////////////
		// Mirror depth-stencil
		// Stencil Off
		// write off, test off
		mirrorReflection_simple_DepthStencilDesc.DepthWriteMask = 0;
		mirrorReflection_simple_DepthStencilDesc.DepthEnable = 0;
		m_MirrorReflection_Simple_DepthStencilState[0][0] = grcStateBlock::CreateDepthStencilState(mirrorReflection_simple_DepthStencilDesc);

		// write off, test on
		mirrorReflection_simple_DepthStencilDesc.DepthWriteMask = 0;
		mirrorReflection_simple_DepthStencilDesc.DepthEnable = 1;
		m_MirrorReflection_Simple_DepthStencilState[0][1] = grcStateBlock::CreateDepthStencilState(mirrorReflection_simple_DepthStencilDesc);

		// write on, test off						   
		mirrorReflection_simple_DepthStencilDesc.DepthWriteMask = 1;		   
		mirrorReflection_simple_DepthStencilDesc.DepthEnable = 0;			   
		m_MirrorReflection_Simple_DepthStencilState[1][0] =  grcStateBlock::CreateDepthStencilState(mirrorReflection_simple_DepthStencilDesc);

		// write on, test on						   
		mirrorReflection_simple_DepthStencilDesc.DepthWriteMask = 1;		   
		mirrorReflection_simple_DepthStencilDesc.DepthEnable = 1;			   
		m_MirrorReflection_Simple_DepthStencilState[1][1] = grcStateBlock::CreateDepthStencilState(mirrorReflection_simple_DepthStencilDesc);

#if RMPTFX_USE_PARTICLE_SHADOWS || PTFX_APPLY_DOF_TO_PARTICLES
		grcBlendStateDesc blendStateDesc;

		blendStateDesc.BlendRTDesc[0].BlendEnable = 1;
		blendStateDesc.BlendRTDesc[0].SrcBlendAlpha = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[0].DestBlendAlpha = grcRSV::BLEND_INVSRCALPHA;
		blendStateDesc.BlendRTDesc[0].BlendOpAlpha = grcRSV::BLENDOP_ADD;

#if RMPTFX_USE_PARTICLE_SHADOWS
		blendStateDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
		blendStateDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
		blendStateDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;

		//Shadow blend state has the the alpha blend state in both color and alpha				
		m_ShadowBlendState = grcStateBlock::CreateBlendState(blendStateDesc);
#endif
		
#if PTFX_APPLY_DOF_TO_PARTICLES	
		if (CPtFxManager::UseParticleDOF())
		{
			blendStateDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
			blendStateDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_SRCALPHA;
			blendStateDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;
		
			PostFX::SetupBlendForDOF(blendStateDesc, false);

			m_ptfxDofBlendNormal = grcStateBlock::CreateBlendState(blendStateDesc);

			blendStateDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
			blendStateDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
			blendStateDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;

			m_ptfxDofBlendAdd = grcStateBlock::CreateBlendState(blendStateDesc);

			blendStateDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_ONE;
			blendStateDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
			blendStateDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_SUBTRACT;

			m_ptfxDofBlendSubtract = grcStateBlock::CreateBlendState(blendStateDesc);

			blendStateDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
			blendStateDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
			blendStateDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_ADD;

			m_ptfxDofBlendCompositeAlpha = grcStateBlock::CreateBlendState(blendStateDesc);


			blendStateDesc.BlendRTDesc[0].DestBlend = grcRSV::BLEND_INVSRCALPHA;
			blendStateDesc.BlendRTDesc[0].SrcBlend = grcRSV::BLEND_ONE;
			blendStateDesc.BlendRTDesc[0].BlendOp = grcRSV::BLENDOP_REVSUBTRACT;

			m_ptfxDofBlendCompositeAlphaSubtract = grcStateBlock::CreateBlendState(blendStateDesc);
		}
#endif // PTFX_APPLY_DOF_TO_PARTICLES
#endif // RMPTFX_USE_PARTICLE_SHADOWS || PTFX_APPLY_DOF_TO_PARTICLES

		ptfxManager::Init(PTFX_MAX_PARTICLES, PTFX_MAX_INSTS, PTFX_MAX_TRAILS, PTFX_MAX_REGISTERED, PTFX_MAX_SCRIPTED, PTFX_MAX_HISTORIES, "common:/data/effects/ptxclipregions.dat");

	    InitRMPTFX();
		InitShaderOverrides();
	    InitThread();

	    // init the sub systems
#if __BANK
	    m_gameDebug.Init();
#endif

		g_pPtfxCallbacks = &g_ptFxCallbacks;
		m_ptfxRenderFlags[0] = m_ptfxRenderFlags[1] = 0;
		m_ptfxCurrRenderFlags = 0;

#if PTFX_ALLOW_INSTANCE_SORTING
	    m_ptFxSorted.Init();
#endif 
	    m_ptFxCollision.Init();

		// init script data
		m_forceVehicleInteriorFlag = false;
		m_forceVehicleInteriorFlagScriptThreadId = THREAD_INVALID;

		// init buffered data
		m_numLightInfos = 0;
		m_numFireInfos = 0;
		m_numDecalInfos = 0;
		m_numDecalPoolInfos = 0;
		m_numDecalTrailInfos = 0;
		m_numLiquidInfos = 0;

		m_numGlassImpactInfos = 0;
		m_numGlassSmashInfos = 0;
		m_numGlassShatterInfos = 0;

		m_blockFxListRemoval = false;
		m_currShadowViewports.Reserve(CASCADE_SHADOWS_COUNT);

		sysMemSet(m_isRenderingRefractionPtfxList, 0, sizeof(m_isRenderingRefractionPtfxList));
		sysMemSet(m_isRenderingIntoWaterRefraction, 0, sizeof(m_isRenderingIntoWaterRefraction));

		m_IsRefractPtfxPresent[0] = false;
		m_IsRefractPtfxPresent[1] = false;
#if FPS_MODE_SUPPORTED
		m_isFirstPersonCamActive[0] = false;
		m_isFirstPersonCamActive[1] = false;
#endif
#if __XENON
		m_useDownrezzedPtfx = false;
#endif
#if USE_DOWNSAMPLED_PARTICLES
		m_disableDownsamplingThisFrame = false;
#endif

#if RMPTFX_USE_PARTICLE_SHADOWS
		//m_GlobalShadowIntensityValue is already set from visual settings when we come here
		RMPTFXMGR.SetGlobalShadowIntensity(m_GlobalShadowIntensityValue);
		//Change m_globalParticleShadowBias and m_globalParticleShadowSlopeBias in ptfxDebug.cpp too.		
		m_GlobalShadowSlopeBias = CSM_PARTICLE_SHADOWS_DEPTH_SLOPE_BIAS;
		m_GlobalShadowBiasRange = CSM_PARTICLE_SHADOWS_DEPTH_BIAS_RANGE;
		m_GlobalshadowBiasRangeFalloff = CSM_PARTICLE_SHADOWS_DEPTH_BIAS_RANGE_FALLOFF;
		m_bAnyParticleShadowsRendered = false;
#endif
#if PTFX_APPLY_DOF_TO_PARTICLES
		if (CPtFxManager::UseParticleDOF())
			m_globalParticleDofAlphaScale = PTFX_PARTICLE_DOF_ALPHA_SCALE;
#endif //PTFX_APPLY_DOF_TO_PARTICLES

		sysMemSet(m_pMulitDrawInterface, 0, sizeof(m_pMulitDrawInterface));

#if GTA_REPLAY
		m_ReplayScriptPtfxAssetID = -1;
		m_RecordReplayScriptRequestAsset = true;
#endif
    }
    else if(initMode == INIT_SESSION)
    {
#if PTFX_ALLOW_INSTANCE_SORTING
	    m_ptFxSorted.InitLevel();
#endif 
	    // init the sub systems
		ptfxManager::InitLevel();

	    // load in the level assets
	    InitGlobalAssets();
    }

#if RMPTFX_DEBUG_PRINT_SHADER_NAMES_AND_PASSES
	ptxShaderTemplate::SetDebugPrintFilter(g_DebugPrintFilter);
#endif 

	BeginFrame();
}

void CPtFxManager::InitShaderOverrides()
{


	//Initialize the underwater technique overrides
	ptxShaderTemplate* spriteShader = ptxShaderTemplateList::LoadShader("ptfx_sprite");
	m_ptxSpriteWaterRefractionShaderOverride.Init(spriteShader->GetGrmShader());
	m_ptxSpriteWaterRefractionShaderOverride.DisableAllTechniques();
	grcEffectTechnique ptfx_sprite_rgba_lit_soft_underwater_Technique = spriteShader->GetGrmShader()->LookupTechnique("RGBA_lit_soft_waterrefract", true);
	grcEffectTechnique ptfx_sprite_rgba_lit_soft_Technique = spriteShader->GetGrmShader()->LookupTechnique("RGBA_lit_soft", true);

	grcEffectTechnique ptfx_sprite_rgba_soft_underwater_Technique = spriteShader->GetGrmShader()->LookupTechnique("RGBA_soft_waterrefract", true);
	grcEffectTechnique ptfx_sprite_rgba_soft_Technique = spriteShader->GetGrmShader()->LookupTechnique("RGBA_soft", true);

	grcEffectTechnique ptfx_sprite_rgba_proj_underwater_Technique = spriteShader->GetGrmShader()->LookupTechnique("RGBA_proj_waterrefract", true);
	grcEffectTechnique ptfx_sprite_rgba_proj_Technique = spriteShader->GetGrmShader()->LookupTechnique("RGBA_proj", true);

	m_ptxSpriteWaterRefractionShaderOverride.OverrideTechniqueWith(ptfx_sprite_rgba_lit_soft_Technique,ptfx_sprite_rgba_lit_soft_underwater_Technique);
	m_ptxSpriteWaterRefractionShaderOverride.OverrideTechniqueWith(ptfx_sprite_rgba_soft_Technique,ptfx_sprite_rgba_soft_underwater_Technique);
	m_ptxSpriteWaterRefractionShaderOverride.OverrideTechniqueWith(ptfx_sprite_rgba_proj_Technique,ptfx_sprite_rgba_proj_underwater_Technique);
	m_ptxSpriteWaterRefractionShaderOverride.EnableTechnique(ptfx_sprite_rgba_lit_soft_underwater_Technique);
	m_ptxSpriteWaterRefractionShaderOverride.EnableTechnique(ptfx_sprite_rgba_soft_underwater_Technique);
	m_ptxSpriteWaterRefractionShaderOverride.EnableTechnique(ptfx_sprite_rgba_proj_underwater_Technique);


	ptxShaderTemplate* trailShader = ptxShaderTemplateList::LoadShader("ptfx_trail");
	m_ptxTrailWaterRefractionShaderOverride.Init(trailShader->GetGrmShader());
	m_ptxTrailWaterRefractionShaderOverride.DisableAllTechniques();

	grcEffectTechnique ptfx_trail_rgba_lit_soft_underwater_Technique = trailShader->GetGrmShader()->LookupTechnique("RGBA_lit_soft_waterrefract", true);
	grcEffectTechnique ptfx_trail_rgba_lit_soft_Technique = trailShader->GetGrmShader()->LookupTechnique("RGBA_lit_soft", true);

	grcEffectTechnique ptfx_trail_rgba_soft_underwater_Technique = trailShader->GetGrmShader()->LookupTechnique("RGBA_soft_waterrefract", true);
	grcEffectTechnique ptfx_trail_rgba_soft_Technique = trailShader->GetGrmShader()->LookupTechnique("RGBA_soft", true);

	m_ptxTrailWaterRefractionShaderOverride.OverrideTechniqueWith(ptfx_trail_rgba_lit_soft_Technique,ptfx_trail_rgba_lit_soft_underwater_Technique);
	m_ptxTrailWaterRefractionShaderOverride.OverrideTechniqueWith(ptfx_trail_rgba_soft_Technique,ptfx_trail_rgba_soft_underwater_Technique);
	m_ptxTrailWaterRefractionShaderOverride.EnableTechnique(ptfx_trail_rgba_lit_soft_underwater_Technique);
	m_ptxTrailWaterRefractionShaderOverride.EnableTechnique(ptfx_trail_rgba_soft_underwater_Technique);

#if RMPTFX_BANK
	ptxShaderTemplateOverride::InitDebug();
#endif

}

///////////////////////////////////////////////////////////////////////////////
//  InitSessionSync
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::InitSessionSync()
{
	ptfxAssertf(m_streamingRequests.HaveAllLoaded(), "not all particle assets have streamed in");
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::Shutdown(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_CORE)
    {       
	    m_ptFxCollision.Shutdown();
#if PTFX_ALLOW_INSTANCE_SORTING
	    m_ptFxSorted.Shutdown();
#endif 

		g_pPtfxCallbacks = NULL;

	    ShutdownThread();
	    ShutdownRMPTFX();

	    ptfxManager::Shutdown();
    }
    else if(shutdownMode == SHUTDOWN_SESSION)
    {
        // unload the level assets
	    ShutdownGlobalAssets();

        ptfxManager::ShutdownLevel();

#if PTFX_ALLOW_INSTANCE_SORTING
	    m_ptFxSorted.ShutdownLevel();
#endif

	    // reset rmptfx
	    RMPTFXMGR.ResetAllImmediately();
    }
}



///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::RemoveScript(scrThreadId scriptThreadId)
{
	if (scriptThreadId == m_forceVehicleInteriorFlagScriptThreadId)
	{
		m_forceVehicleInteriorFlag = false;
		m_forceVehicleInteriorFlagScriptThreadId = THREAD_INVALID;
	}
}



#if RSG_PC
void					CPtFxManager::DeviceLost				()
{
	if (g_ptxManager::IsInstantiated())
	{
		RMPTFXMGR.SetDepthBuffer(NULL);
		RMPTFXMGR.SetFrameBuffer(NULL);
		RMPTFXMGR.SetFrameBufferSampler(NULL);
	}
}
void					CPtFxManager::DeviceReset				()
{
	if (g_ptxManager::IsInstantiated())
	{
		g_ptFxManager.InitRMPTFXTexture();
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  InitGlobalAssets
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::InitGlobalAssets				()
{
	ptfxManager::GetAssetStore().PrepForXmlLoading();

	// load in the global core asset
	strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atHashValue("core"));
	if (ptfxVerifyf(slot.IsValid(), "cannot find particle asset with this name in any loaded rpf (core)"))
	{
		m_streamingRequests.PushRequest(slot.Get(), ptfxManager::GetAssetStore().GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
	}


	// load all other assets in now if required
#if __DEV
	if (PARAM_ptfxloadall.Get())
	{
		// make sure to finish the load of core before loading everything else, because everything else may share resources with core
		CStreaming::LoadAllRequestedObjects();
		ptfxAssertf(m_streamingRequests.HaveAllLoaded(), "the 'core' particle asset has not streamed in");

		for (int i=0; i<ptfxManager::GetAssetStore().GetSize(); i++)
		{
			if (ptfxManager::GetAssetStore().IsValidSlot(strLocalIndex(i)))
			{
				m_streamingRequests.PushRequest(i, ptfxManager::GetAssetStore().GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
			}
		}
	}
	else if (PARAM_ptfxloadmost.Get())
	{
		// make sure to finish the load of core before loading everything else, because everything else may share resources with core
		CStreaming::LoadAllRequestedObjects();
		ptfxAssertf(m_streamingRequests.HaveAllLoaded(), "the 'core' particle asset has not streamed in");

		for (int i=0; i<ptfxManager::GetAssetStore().GetSize(); i++)
		{
			if (ptfxManager::GetAssetStore().IsValidSlot(strLocalIndex(i)))
			{
				ptxFxListDef* pFxListDef = ptfxManager::GetAssetStore().GetSlot(strLocalIndex(i));
				const char* pFxListName = pFxListDef->m_name.GetCStr();
				bool isCutsceneFxList = pFxListName[0]=='c' && pFxListName[1]=='u' && pFxListName[2]=='t';
				bool isScriptFxList = pFxListName[0]=='s' && pFxListName[1]=='c' && pFxListName[2]=='r';
				bool isRayfireFxList = pFxListName[0]=='d' && pFxListName[1]=='e' && pFxListName[2]=='s';
				if (isCutsceneFxList==false && isScriptFxList==false && isRayfireFxList==false)
				{
					m_streamingRequests.PushRequest(i, ptfxManager::GetAssetStore().GetStreamingModuleId(), STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
				}
			}
		}
	}
#endif

	// load in the episodic assets
// 	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::RMPTFX_FILE);
// 	while (DATAFILEMGR.IsValid(pData))
// 	{
// 		// load the episode effects
// 		slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(hashValue(pData->m_filename));
// 		if (ptfxVerifyf(slot.IsValid(), ""))
// 		{
// 			m_streamingRequests.PushRequest(slot, ptfxManager::GetAssetStore().GetStreamingModuleId());
// 		}
// 
// 		pData = DATAFILEMGR.GetNextFile(pData);
// 	}
}


///////////////////////////////////////////////////////////////////////////////
//  ShutdownGlobalAssets
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ShutdownGlobalAssets			()
{
	for (int i=0; i<m_streamingRequests.GetRequestCount(); i++)
	{
		strRequest& strReq = m_streamingRequests.GetRequest(i);
		{
			ptfxManager::GetAssetStore().ClearRequiredFlag(strReq.GetRequestId().Get(), STRFLAG_FORCE_LOAD | STRFLAG_DONTDELETE);
		}
	}

	// release the streamed global assets (this should put their ref count to 1)
	m_streamingRequests.ReleaseAll();
}


///////////////////////////////////////////////////////////////////////////////
//  InitThread
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::InitThread					()
{
	// init the update thread
	m_startUpdateSema = sysIpcCreateSema(0);	
	m_finishUpdateSema = sysIpcCreateSema(0);	
	m_nextUpdateDelta = 0.0f;

	m_gpuTimerSumMedRes = 0.0f;
	m_gpuTimerSumLowRes = 0.0f;
	m_updateDeltaSum = 0.0f;
	m_gpuTimeMedRes[0] = m_gpuTimeMedRes[1] = 0.0f;
	m_gpuTimeLowRes[0] = m_gpuTimeLowRes[1] = 0.0f;
	m_numGPUFrames = 0;
	m_ptfxRenderFlags[0] = m_ptfxRenderFlags[1] = 0;
	m_ptfxCurrRenderFlags = 0;

	m_startedUpdate = false;
	m_updateThreadId = sysIpcCreateThread(UpdateThread, this, ((RSG_ORBIS ? 128 : 56) * 1024), PRIO_PTFX_THREAD, "RMPTFX Update", 1, "RmptfxUpdateThread");
}


///////////////////////////////////////////////////////////////////////////////
//  ShutdownThread
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::ShutdownThread				()
{
	// shutdown the update thread
	if (m_startedUpdate)
	{
		sysIpcWaitSema(m_finishUpdateSema);
	}

	m_nextUpdateDelta = -999.0f;

	sysIpcSignalSema(m_startUpdateSema);
	sysIpcWaitSema(m_finishUpdateSema);

	// clean up handles
	sysIpcWaitThreadExit(m_updateThreadId);

	sysIpcDeleteSema(m_finishUpdateSema);
	sysIpcDeleteSema(m_startUpdateSema);
}


///////////////////////////////////////////////////////////////////////////////
//  InitRMPTFX
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::InitRMPTFX					()
{
	RMPTFXMGR.SetShaderInitProgVarsFunctor(MakeFunctor(CPtfxShaderCallbacks::InitProgVars));
	RMPTFXMGR.SetShaderSetProgVarsFunctor(MakeFunctor(CPtfxShaderCallbacks::SetShaderVars));

	// add game specific text to the interface
#if RMPTFX_EDITOR
	RMPTFXMGR.AddEffectRuleGameFlagName("Sort Within GTA Scene");

	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Fire (Contained)");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Fire");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Water");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("<NOTUSED> Sand");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("<NOTUSED> Mud");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("<NOTUSED> Blood");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("<NOTUSED> Dust");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("<NOTUSED> Smoke");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Blaze");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Soak - Slow");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Soak - Medium");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Soak - Fast");
	RMPTFXMGR.AddEffectRuleDataVolumeTypeName("Fire (Flame Thrower)");
#endif

	// create the draw lists
//	for (s32 i=0; i<NUM_PTFX_DRAWLISTS; i++)
//	{
//		char drawListName[32];
//		sprintf(drawListName, "drawList%d", i);
//		m_pDrawList[i] = RMPTFXMGR.CreateDrawList(drawListName);
	//	}
	m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW] = RMPTFXMGR.CreateDrawList("Low Quality");
	m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM] = RMPTFXMGR.CreateDrawList("Medium Quality");
	m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH] = RMPTFXMGR.CreateDrawList("High Quality");

	// init the game specific behaviours
	RMPTFX_REGISTER_BEHAVIOUR(ptxu_Decal);
	RMPTFX_REGISTER_BEHAVIOUR(ptxu_DecalPool);
	//RMPTFX_REGISTER_BEHAVIOUR(ptxu_DecalTrail);
	//RMPTFX_REGISTER_BEHAVIOUR(ptxu_Fire);
	RMPTFX_REGISTER_BEHAVIOUR(ptxu_FogVolume);
	RMPTFX_REGISTER_BEHAVIOUR(ptxu_Light);
	RMPTFX_REGISTER_BEHAVIOUR(ptxu_Liquid);
	RMPTFX_REGISTER_BEHAVIOUR(ptxu_River);
	RMPTFX_REGISTER_BEHAVIOUR(ptxu_ZCull);
	
	// init the callbacks
	RMPTFXMGR.SetEffectInstStartFunctor(MakeFunctor(EffectInstStartFunctor));
	RMPTFXMGR.SetEffectInstStopFunctor(MakeFunctor(EffectInstStopFunctor));
	RMPTFXMGR.SetEffectInstDeactivateFunctor(MakeFunctor(EffectInstDeactivateFunctor));
	//RMPTFXMGR.SetEffectInstSpawnFunctor(MakeFunctorRet(EffectInstSpawnFunctor));
	RMPTFXMGR.SetEffectInstDistToCamFunctor(MakeFunctorRet(EffectInstDistToCamFunctor));
	RMPTFXMGR.SetEffectInstOcclusionFunctor(MakeFunctorRet(EffectInstOcclusionFunctor));
	RMPTFXMGR.SetEffectInstPostUpdateFunctor(MakeFunctor(EffectInstPostUpdateFunctor));
	RMPTFXMGR.SetEffectInstUpdateCullingFunctor(MakeFunctor(EffectInstUpdateCullingFunctor));
	RMPTFXMGR.SetEffectInstUpdateFinalizeFunctor(MakeFunctor(EffectInstUpdateFinalizeFunctor));
	RMPTFXMGR.SetEffectInstShouldBeRenderedFunctor(MakeFunctorRet(EffectInstShouldBeRenderedFunctor));
	RMPTFXMGR.SetEffectInstInitFunctor(MakeFunctor(EffectInstInitFunctor));
	RMPTFXMGR.SetEffectInstUpdateSetupFunctor(MakeFunctor(EffectInstUpdateSetupFunctor));
	RMPTFXMGR.SetParticleRuleInitFunctor(MakeFunctor(ParticleRuleInitFunctor));
	RMPTFXMGR.SetParticleRuleShouldBeRenderedFunctor(MakeFunctorRet(ParticleRuleShouldBeRenderedFunctor));
	RMPTFXMGR.SetOverrideDepthStencilStateFunctor(MakeFunctorRet(OverrideDepthStencilStateFunctor));
	RMPTFXMGR.SetOverrideBlendStateFunctor(MakeFunctorRet(OverrideBlendStateFunctor));

	// set up the name of the shader var that controls model colour
	RMPTFXMGR.SetForcedColourShaderVarName("Colorize");

	// read and write access to the depth buffers for the soft particles
#if __PPU
	RMPTFXMGR.SetPatchedDepthBuffer(CRenderTargets::GetDepthBufferAsColor());
#endif // __PPU	

#if DEVICE_MSAA
	RMPTFXMGR.SetDepthBuffer(CRenderTargets::GetDepthResolved());
#else
	RMPTFXMGR.SetDepthBuffer(CRenderTargets::GetDepthBuffer());
#endif

	grcRenderTarget *const backBuffer = CRenderTargets::GetBackBuffer(false);
	grcRenderTarget *const backSampler =
#if __D3D11 || DEVICE_MSAA
		__D3D11 || GRCDEVICE.GetMSAA()? CRenderTargets::GetBackBufferCopy(false) :
#endif
		backBuffer;

	RMPTFXMGR.SetFrameBuffer(backBuffer );
	RMPTFXMGR.SetFrameBufferSampler(backSampler );

	// Set the hook for the lighting constants
	// NOTE: let RMPTFX manager the memory
	RMPTFXMGR.SetRenderSetup( rage_new CSetLightingConstants("ptfx_model") );

	// init the downsample buffers
#if USE_DOWNSAMPLED_PARTICLES
	InitDownsampling();
#endif

#if USE_SHADED_PTFX_MAP
	InitShadowedPtFxBuffer();

	m_CloudTexture = CShaderLib::LookupTexture("cloudnoise");
	Assert(m_CloudTexture);
	if(m_CloudTexture)
		m_CloudTexture->AddRef();

	// grab some shaders and shader variables we'll need to properly render our shaded particle buffer
	m_shadedParticleBufferShader = grmShaderFactory::GetInstance().Create();
	m_shadedParticleBufferShader->Load("ptfx_sprite");
	m_shadedParticleBufferShaderTechnique = m_shadedParticleBufferShader->LookupTechnique("shadedparticles_buffergen");
	m_shadedParticleBufferShaderShadowMapId = m_shadedParticleBufferShader->LookupVar("gCSMShadowTextureSamp");
	m_shadedParticleBufferShaderParamsId = m_shadedParticleBufferShader->LookupVar("gShadedPtFxParams");
	m_shadedParticleBufferShaderParams2Id = m_shadedParticleBufferShader->LookupVar("gShadedPtFxParams2");

	m_shadedParticleBufferShaderCloudSampler = m_shadedParticleBufferShader->LookupVar("CloudShadowMap");
	m_shadedParticleBufferShaderSmoothStepSampler = m_shadedParticleBufferShader->LookupVar("SmoothStepMap");
#endif //USE_SHADED_PTFX_MAP

#if RMPTFX_USE_PARTICLE_SHADOWS
	m_GlobalShadowBiasVar = grcEffect::LookupGlobalVar("gGlobalParticleShadowBias");
#endif
	
	// settings
	RMPTFXMGR.SetHighQualityEvo(0.0f);

#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		m_GlobalParticleDofAlphaScaleVar = grcEffect::LookupGlobalVar("gGlobalParticleDofAlphaScale");

		m_particleDepthMapResolved = NULL;
		m_particleDepthMap = NULL;
		m_particleAlphaMapResolved = NULL;
		m_particleAlphaMap = NULL;

		InitRMPTFXTexture();
	}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
	RMPTFXMGR.SetUsingGeometryShader(true);
#endif

	// setup the keyframe prop thread ID for the main thread
	RMPTFXMGR.SetupPtxKeyFramePropThreadId();
}


///////////////////////////////////////////////////////////////////////////////
//  ShutdownRMPTFX
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::ShutdownRMPTFX			()
{
#if USE_DOWNSAMPLED_PARTICLES
	ShutdownDownsampling();
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  InitRMPTFXTexture
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::InitRMPTFXTexture					()
{
#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		grcTextureFactory::CreateParams params;
		const grcRenderTargetType type = grcrtPermanent;
		const u8                  heap = 0;

		params.Multisample = GRCDEVICE.GetMSAA();
	# if DEVICE_EQAA
		params.Multisample.DisableCoverage();
		params.ForceFragmentMask = false;
		params.SupersampleFrequency = params.Multisample.m_uFragments;
	# else // DEVICE_EQAA
		u32 multisampleQuality = GRCDEVICE.GetMSAAQuality();
		params.MultisampleQuality = (u8)multisampleQuality;
	# endif // DEVICE_EQAA
	# if RSG_ORBIS
		// these have to be together on PS4
		params.ForceFragmentMask = params.EnableFastClear = true;
	# endif // RSG_ORBIS

		const unsigned width	= VideoResManager::GetSceneWidth();
		const unsigned height	= VideoResManager::GetSceneHeight();
		const int ptfxDepthbpp = 16;
		const grcTextureFormat ptfxDepthFormat = grctfR16F;

		const int ptfxAlphabpp = 8;
		const grcTextureFormat ptfxAlphaFormat = grctfL8;

		//Create as an MSAA render target as its bound as a secondary target alongside the MSAA backbuffer. May be able to get around this on PS4 and Xbox?
		//Particle Depth map is a 16bit buffer thats placed in ESRAM. It's 16bit because we need precision for applying Depth of field. We will be storing linear depth in
		//this buffer
		params.Format = ptfxDepthFormat;
#if RSG_DURANGO
		params.ESRAMPhase = grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_2
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_3
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_4
			| grcTextureFactory::TextureCreateParams::ESRPHASE_DRAWSCENE_5;
#endif
		m_particleDepthMap = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "PARTICLE_DEPTH_MAP", type, width, height, ptfxDepthbpp, &params, heap WIN32PC_ONLY(, m_particleDepthMap));
#if RSG_DURANGO
		params.ESRAMPhase = 0;
#endif

		//This is an 8 bit alpha target used for DOF thats not part of ESRAM
		params.Format = ptfxAlphaFormat;
		m_particleAlphaMap = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "PARTICLE_ALPHA_MAP", type, width, height, ptfxAlphabpp, &params, heap WIN32PC_ONLY(, m_particleAlphaMap));

	# if DEVICE_MSAA
		if ((params.Multisample && RSG_PC) || params.Multisample > 1)
		{
			params.Multisample = grcDevice::MSAA_None;
			ORBIS_ONLY(params.EnableFastClear = false;)
	#if RSG_PC
				if (m_particleDepthMapResolved == m_particleDepthMap) {m_particleDepthMapResolved = NULL;}
				if (m_particleAlphaMapResolved == m_particleAlphaMap) {m_particleAlphaMapResolved = NULL;}
	#endif
			//Making sure the resolved targets are same as the MSAA targets
			params.Format = ptfxDepthFormat;
			m_particleDepthMapResolved = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "PARTICLE_DEPTH_MAP_RESOLVED", type, width, height, ptfxDepthbpp, &params, heap WIN32PC_ONLY(, m_particleDepthMapResolved));

			params.Format = ptfxAlphaFormat;
			m_particleAlphaMapResolved = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "PARTICLE_ALPHA_MAP_RESOLVED", type, width, height, ptfxAlphabpp, &params, heap WIN32PC_ONLY(, m_particleAlphaMapResolved));
		}else
		{
	#if RSG_PC
			if (m_particleDepthMapResolved != m_particleDepthMap && m_particleDepthMapResolved) {delete m_particleDepthMapResolved; m_particleDepthMapResolved = NULL;}
			if (m_particleAlphaMapResolved != m_particleAlphaMap && m_particleAlphaMapResolved) {delete m_particleAlphaMapResolved; m_particleAlphaMapResolved = NULL;}
	#endif
			m_particleDepthMapResolved = m_particleDepthMap;
			m_particleAlphaMapResolved = m_particleAlphaMap;
		}
	# endif //DEVICE_MSAA
	}
	else
	{
		if (m_particleDepthMapResolved != m_particleDepthMap && m_particleDepthMapResolved) {delete m_particleDepthMapResolved;}
		m_particleDepthMapResolved = NULL;
		if (m_particleAlphaMapResolved != m_particleAlphaMap && m_particleAlphaMapResolved) {delete m_particleAlphaMapResolved;}
		m_particleAlphaMapResolved = NULL;
		if (m_particleDepthMap) {delete m_particleDepthMap; m_particleDepthMap = NULL;}
		if (m_particleAlphaMap) {delete m_particleAlphaMap; m_particleAlphaMap = NULL;}
	}
#endif //PTFX_APPLY_DOF_TO_PARTICLES
}

#if USE_DOWNSAMPLED_PARTICLES
///////////////////////////////////////////////////////////////////////////////
//  InitDownsampling
///////////////////////////////////////////////////////////////////////////////
void		 	CPtFxManager::InitDownsampling			()
{
// create downsampling Buffers
	const int width		= VideoResManager::GetSceneWidth()/2;
	const int height	= VideoResManager::GetSceneHeight()/2;
	// color
	grcTextureFactory::CreateParams params;
	params.Multisample = 0;
	params.HasParent = true;
	params.Parent = NULL; 
	params.UseFloat = true;
#if __PPU
	params.InTiledMemory = true;
	params.IsSwizzled = false;
#endif
	params.UseHierZ = false;
	params.IsSRGB = false;

#if __PS3
	const int bpp = 64;
	params.Format = grctfA16B16G16R16F;
	u8 rtPoolHeap = 5;
	RTMemPoolID memPoolID = PSN_RTMEMPOOL_P5120_TILED_LOCAL_COMPMODE_DISABLED_2;
#else 
	const int bpp = 32;
	params.Format = grctfA8R8G8B8;
	RTMemPoolID memPoolID = XENON_RTMEMPOOL_GBUFFER23;
	u8 rtPoolHeap = kPtfxDownsampleHeap;

	params.Parent = CRenderTargets::GetDepthBufferQuarter();
#endif

	m_pLowResPtfxBuffer = CRenderTargets::CreateRenderTarget(memPoolID, "PTX Downsample Color", grcrtPermanent, width, height, bpp, &params, rtPoolHeap);
	if(memPoolID != RTMEMPOOL_NONE)
	{
		m_pLowResPtfxBuffer->AllocateMemoryFromPool();
	}

	// Always allocate that one after!
	// We need that surface to be at the end of the memory pool so the blurred water reflection
	// doesn't screw us over!

	//Create conditional query for downsampling ptfx
	m_PtfxDownsampleQuery = GRCDEVICE.CreateConditionalQuery();
}


///////////////////////////////////////////////////////////////////////////////
//  ShutdownDownsampling
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::ShutdownDownsampling		()
{
	GRCDEVICE.DestroyConditionalQuery(m_PtfxDownsampleQuery);
}

#endif //USE_DOWNSAMPLED_PARTICLES

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::Update						(float deltaTime)
{
#if __BANK
	if (m_gameDebug.GetOverrideDeltaTime())
	{
		deltaTime = m_gameDebug.GetOverrideDeltaTimeValue();
	}
	else
#endif
	{
		float minDeltaTime = 0.0f;
		float maxDeltaTime = fwTimer::GetMaximumFrameTime();

		REPLAY_ONLY(if (!CReplayMgr::IsReplayInControlOfWorld()))
		{
			ptfxAssertf(deltaTime<=maxDeltaTime, "particle update delta time (%.2f) is greater than the maximum allowed (%.2f) - this will be clamped", deltaTime, maxDeltaTime);
			ptfxAssertf(deltaTime>=minDeltaTime, "particle update delta time (%.2f) is less than the minimum allowed (%.2f) - this will be clamped", deltaTime, minDeltaTime);
		}

		deltaTime = Clamp(deltaTime, minDeltaTime, maxDeltaTime);
	}

#if GTA_REPLAY
	//On a replay we typically miss any calls to RequestPtFxAsset so we cache the slot id of the asset
	//and add a replay packet to load the assets at the beginning of each clip.
	if(CReplayMgr::IsRecording() && m_ReplayScriptPtfxAssetID != -1)
	{
		if( m_RecordReplayScriptRequestAsset )
		{
			CReplayMgr::RecordFx<CPacketRequestPtfxAsset>(
				CPacketRequestPtfxAsset(m_ReplayScriptPtfxAssetID));
			m_RecordReplayScriptRequestAsset = false;
		}
	}
	else
	{
		m_RecordReplayScriptRequestAsset = true;
	}
#endif //GTA_REPLAY


	m_updateDeltaSum += deltaTime;
	UpdateDynamicQuality();

#if __BANK
	m_gameDebug.Update();

	if (ptfxDebug::GetDisablePtFx())
	{
		return;
	}

	if( m_gameDebug.GetExplosionPerformanceTest() )
	{
		ProcessExplosionPerformanceTest();
	}
#endif

	if (gVpMan.GetGameViewport())
	{
		m_currViewport = gVpMan.GetGameViewport()->GetGrcViewport();
	}

#if FPS_MODE_SUPPORTED
	const camBaseCamera* pCamera = camInterface::GetDominantRenderedCamera();
	m_isFirstPersonCamActive[gRenderThreadInterface.GetUpdateBuffer()] = pCamera && pCamera->GetIsClassId(camFirstPersonShooterCamera::GetStaticClassId());
#endif

	CRenderPhaseCascadeShadowsInterface::GetCascadeViewports(m_currShadowViewports);
	// check all of the global assets have loaded
	ptfxAssertf(m_streamingRequests.HaveAllLoaded(), "the global particle assets are not all loaded");

	// set the memory bucket
	USE_MEMBUCKET(MEMBUCKET_FX);

#if RMPTFX_EDITOR 
	// make sure rmptfx knows about our main game camera matrix - for starting debug effects
	RMPTFXMGR.SetDebugEffectCameraMtx(&RCC_MAT34V(camInterface::GetMat()));
#endif

	Assertf(ptfxManager::GetAssetStore().IsSafeToRemoveFxLists(), "Hey someone left the safeToRemoveFxLists flag cleared for too long!");
	ptfxManager::GetAssetStore().SetIsSafeToRemoveFxLists(false);

	// set some flags
	CVfxHelper::SetUsingCinematicCam(camInterface::GetCinematicDirector().IsRendering());

	ptfxManager::Update(deltaTime, RCC_MAT34V(camInterface::GetMat()));

	CPed::ClearPedsThatCanGetWetThisFrame();

	m_ptFxCollision.Update();

	// check everything is fine
	ptfxAssertf(!m_startedUpdate, "update thread is already started");
	m_startedUpdate = true;

	// set the next update delta
	m_nextUpdateDelta = deltaTime;

	// signal that the update can start
	sysIpcSignalSema(m_startUpdateSema);
}

#if __BANK
u32 sExplosionTestState = 0;
u32 sExplosionTestStartTime = 0; 
void CPtFxManager::ProcessExplosionPerformanceTest()
{
	static float offsetForward = 7.0f;
	static float offsetRight = 2.0f;

	//Pause the explosions after this time.
	static float pauseTime = 0.75;
	static eExplosionTag explosionType = EXP_TAG_CAR;

	CPed * pPlayerPed = FindPlayerPed();
	if (pPlayerPed)
	{	
		switch(sExplosionTestState)
		{
		case 0:
			{
				Vec3V playerPos = pPlayerPed->GetTransform().GetPosition();
				Vec3V playerForward = pPlayerPed->GetTransform().GetForward();
				Vec3V playerRight = pPlayerPed->GetTransform().GetRight();

				Vec3V newPosA = playerPos + (playerForward * ScalarV(offsetForward));
				Vec3V newPosB = newPosA + (playerRight * ScalarV(offsetRight));
				Vec3V newPosC = newPosA - (playerRight * ScalarV(offsetRight));
				Vec3V newPosD = newPosA + (playerForward * ScalarV(offsetForward));
				Vec3V newPosE = newPosD + (playerRight * ScalarV(2*offsetRight));
				Vec3V newPosF = newPosD - (playerRight * ScalarV(2*offsetRight));
				Vec3V newPosG = newPosD + (playerForward * ScalarV(offsetForward));
				Vec3V newPosH = newPosG + (playerRight * ScalarV(2*offsetRight));
				Vec3V newPosI = newPosG - (playerRight * ScalarV(2*offsetRight));
				Vec3V newPosJ = newPosG + (playerRight * ScalarV(4*offsetRight));
				Vec3V newPosK = newPosG - (playerRight * ScalarV(4*offsetRight));

				const u32 numPositions = 10;
				Vec3V positions[numPositions] = {newPosB, newPosC, newPosD, newPosE, newPosF, newPosG, newPosH, newPosI, newPosJ, newPosK};
				for( u32 i = 0; i < numPositions; i++)
				{
					CExplosionManager::CExplosionArgs explosionArgs(explosionType, VEC3V_TO_VECTOR3(positions[i]));
					explosionArgs.m_fCamShake = 0.0;
					CExplosionManager::AddExplosion(explosionArgs, NULL, true);	
				}

				sExplosionTestStartTime = fwTimer::GetTimeInMilliseconds();
				sExplosionTestState++;
				break;
			}

		case 1:
			{
				if( fwTimer::GetTimeInMilliseconds() - sExplosionTestStartTime > pauseTime * 1000)
				{
					fwTimer::StartUserPause();
					m_gameDebug.ResetExplosionPerformanceTest();
					sExplosionTestState = 0;
				}
			}
			break;
		}
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//	UpdateDynamicQuality
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::UpdateDynamicQuality		(void)
{
	// Note: right now this only works on PS3 because the 360 has a visual pop when you switch between the two.

	m_numGPUFrames++;
	m_gpuTimerSumMedRes += m_gpuTimeMedRes[gRenderThreadInterface.GetUpdateBuffer()];
	m_gpuTimerSumLowRes += m_gpuTimeLowRes[gRenderThreadInterface.GetUpdateBuffer()];

	// the render flags may change this frame, but since this value is double buffered between the render
	// thread and the update thread, we need to make sure we are keeping them in sync
	m_ptfxRenderFlags[gRenderThreadInterface.GetUpdateBuffer()] = m_ptfxCurrRenderFlags;

#if __BANK
	if (m_gameDebug.GetRenderAllDownsampled())
	{
		m_ptfxRenderFlags[gRenderThreadInterface.GetUpdateBuffer()] = PTFX_RF_MEDRES_DOWNSAMPLE | PTFX_RF_LORES_DOWNSAMPLE;
		m_ptfxCurrRenderFlags = PTFX_RF_MEDRES_DOWNSAMPLE | PTFX_RF_LORES_DOWNSAMPLE;
		
		m_numGPUFrames = 0;
		m_gpuTimerSumMedRes = 0.0f;
		m_gpuTimerSumLowRes = 0.0f;
		m_updateDeltaSum = 0.0f;
		return;
	}
#if PTFX_DYNAMIC_QUALITY
	else if (!m_gameDebug.GetDynamicQualitySystemEnabled())
#endif
	{
		m_ptfxRenderFlags[gRenderThreadInterface.GetUpdateBuffer()] = 0;
		m_ptfxCurrRenderFlags = 0;
		
		m_numGPUFrames = 0;
		m_gpuTimerSumMedRes = 0.0f;
		m_gpuTimerSumLowRes = 0.0f;
		m_updateDeltaSum = 0.0f;
		return;
	}
#endif

#if PTFX_DYNAMIC_QUALITY

	const float qualityUpdateTime = BANK_SWITCH(m_gameDebug.GetDQUpdateFrequency(), PTFX_DYNAMIC_QUALITY_UPDATE_TIME);			// seconds
	const float totalRenderTimeThreshold = BANK_SWITCH(m_gameDebug.GetDQTotalRenderTimeThreshold(), PTFX_DYNAMIC_QUALITY_TOTAL_TIME_THRESHOLD);	// ms
	const float lowResRenderTimeThreshold = BANK_SWITCH(m_gameDebug.GetDQLowResRenderTimeThreshold(), PTFX_DYNAMIC_QUALITY_LOWRES_TIME_THRESHOLD);	// ms
	const float endDownsamplingThreshold = BANK_SWITCH(m_gameDebug.GetDQEndDownsampleThreshold(), PTFX_DYNAMIC_QUALITY_END_LOWRES_TIME_THRESHOLD);	// ms

	// check every few seconds to see if we need to modify the quality of particles
	if (m_updateDeltaSum > qualityUpdateTime)
	{
		float averagePtfxRenderTimeMedRes = m_gpuTimerSumMedRes / (float)m_numGPUFrames;
		float averagePtfxRenderTimeLowRes = m_gpuTimerSumLowRes / (float)m_numGPUFrames;
		u8 renderFlags = m_ptfxRenderFlags[gRenderThreadInterface.GetUpdateBuffer()];
		
		if (averagePtfxRenderTimeMedRes + averagePtfxRenderTimeLowRes > totalRenderTimeThreshold)
		{
			// if we are actually render some low res particles, and they are individually consuming more than 2.5ms, the first
			// thing we want to try is just downsampling the low res particles.
			if (averagePtfxRenderTimeLowRes > lowResRenderTimeThreshold &&
				!(renderFlags & PTFX_RF_LORES_DOWNSAMPLE))
			{
				renderFlags = PTFX_RF_LORES_DOWNSAMPLE;
			}
			else
			{
				// either there aren't enough low-res to really matter, or they are already downsampled.  Either way,
				// it's time to downsample the hi-res particles as well.
				renderFlags = PTFX_RF_LORES_DOWNSAMPLE | PTFX_RF_MEDRES_DOWNSAMPLE;
			}
		}
		else if (averagePtfxRenderTimeMedRes + averagePtfxRenderTimeLowRes < endDownsamplingThreshold)
		{
			// if particles have reached the point where they are less than specified amount in render time cost, stop downsampling.
			renderFlags = 0;
		}

		m_ptfxRenderFlags[gRenderThreadInterface.GetUpdateBuffer()] = renderFlags;
		m_ptfxCurrRenderFlags = renderFlags;

		m_numGPUFrames = 0;
		m_gpuTimerSumMedRes = 0.0f;
		m_gpuTimerSumLowRes = 0.0f;
		m_updateDeltaSum = 0.0f;
	}

#endif
}


///////////////////////////////////////////////////////////////////////////////
//	StoreLight
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreLight				(ptfxLightInfo& lightInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeLightCS);
	if (m_numLightInfos<PTFX_MAX_STORED_LIGHTS)
	{
		m_lightInfos[m_numLightInfos++] = lightInfo;
	}
	else
	{
		ptfxWarningf("trying to add too many lights from particle effects this frame");
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredLights
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredLights		()
{
	for (int i=0; i<m_numLightInfos; i++)
	{
		if(m_lightInfos[i].pEffectInst == NULL)
		{
			continue;
		}

		Vec3V vLightPos = m_lightInfos[i].vPos;
		Vec3V vLightDir(V_Z_AXIS_WZERO);
		Vec3V vLightTan(V_Y_AXIS_WZERO);
		Vec3V vLightCol(m_lightInfos[i].lightCol.GetRedf(), m_lightInfos[i].lightCol.GetGreenf(), m_lightInfos[i].lightCol.GetBluef());
		float lightIntensity = m_lightInfos[i].lightIntensity;
		float lightFalloff = m_lightInfos[i].lightFalloff;
		float lightRange = m_lightInfos[i].lightRange;
		float lightAngle = m_lightInfos[i].lightAngle;

		Vec3V vCoronaCol(m_lightInfos[i].coronaCol.GetRedf(), m_lightInfos[i].coronaCol.GetGreenf(), m_lightInfos[i].coronaCol.GetBluef());
		float coronaIntensity = m_lightInfos[i].coronaIntensity;
		float coronaSize = m_lightInfos[i].coronaSize;
		float coronaFlare = m_lightInfos[i].coronaFlare;
		float coronaZBias = m_lightInfos[i].coronaZBias;

#if __BANK
		if (gLastGenMode)
		{
			lightIntensity *= 0.2f;
		}
#endif

		if (lightRange>0.0f)
		{
			u32 lightFlags = LIGHTFLAG_FX;
			if (m_lightInfos[i].castsShadows)
			{
				lightFlags |= LIGHTFLAG_CAST_SHADOWS | LIGHTFLAG_CAST_DYNAMIC_GEOM_SHADOWS | LIGHTFLAG_CAST_STATIC_GEOM_SHADOWS;
			}

			// set up omni light
			CLightSource* pLightSource = CAsyncLightOcclusionMgr::AddLight();
			if (pLightSource)
			{
				pLightSource->Reset();
				pLightSource->SetCommon(LIGHT_TYPE_POINT, lightFlags, RCC_VECTOR3(vLightPos), RCC_VECTOR3(vLightCol), lightIntensity, LIGHT_ALWAYS_ON);

				// override for spotlights
				if (m_lightInfos[i].lightType!=0)
				{
					pLightSource->SetType(LIGHT_TYPE_SPOT);

					// calc direction and tangent
					Mat34V vEffectMtxWld = m_lightInfos[i].pEffectInst->GetMatrix();
					Mat34V vEmitterMtxWld = m_lightInfos[i].pEmitterInst->GetCreationDomainMtxWld(vEffectMtxWld);

					if (m_lightInfos[i].lightType==1)					// spotlight aligned to effect's x axis
					{
						vLightDir = vEffectMtxWld.GetCol0();
						vLightTan = vEffectMtxWld.GetCol2();
					}
					else if (m_lightInfos[i].lightType==2)				// spotlight aligned to effect's y axis
					{
						vLightDir = vEffectMtxWld.GetCol1();
						vLightTan = vEffectMtxWld.GetCol0();
					}
					else if (m_lightInfos[i].lightType==3)				// spotlight aligned to effect's z axis
					{
						vLightDir = vEffectMtxWld.GetCol2();
						vLightTan = vEffectMtxWld.GetCol1();
					}
					else if (m_lightInfos[i].lightType==4)				// spotlight aligned to emitter's x axis
					{
						vLightDir = vEmitterMtxWld.GetCol0();
						vLightTan = vEmitterMtxWld.GetCol2();
					}
					else if (m_lightInfos[i].lightType==5)				// spotlight aligned to emitter's y axis
					{
						vLightDir = vEmitterMtxWld.GetCol1();
						vLightTan = vEmitterMtxWld.GetCol0();
					}
					else if (m_lightInfos[i].lightType==6)				// spotlight aligned to emitter's z axis
					{
						vLightDir = vEmitterMtxWld.GetCol2();
						vLightTan = vEmitterMtxWld.GetCol1();
					}
				}

				pLightSource->SetDirTangent(RCC_VECTOR3(vLightDir), RCC_VECTOR3(vLightTan));
				pLightSource->SetRadius(lightRange);
				pLightSource->SetFalloffExponent(lightFalloff);

				if (pLightSource->GetType()==LIGHT_TYPE_SPOT)
					pLightSource->SetSpotlight(0.0f, lightAngle);

				if (m_lightInfos[i].castsShadows)
				{
					pLightSource->SetShadowTrackingId(fwIdKeyGenerator::Get(m_lightInfos[i].pPointItem, 0));
				}

				fwEntity* pAttachEntityFW = ptfxAttach::GetAttachEntity(m_lightInfos[i].pEffectInst);
				if (pAttachEntityFW)
				{
					CEntity* pAttachEntity = static_cast<CEntity*>(pAttachEntityFW);
					fwInteriorLocation interiorLocation = pAttachEntity->GetInteriorLocation();
					pLightSource->SetInInterior(interiorLocation);
				}
				else
				{
					pLightSource->SetInInterior(CVfxHelper::GetCamInteriorLocation());
				}

				// NOTE: we don't want to call Lights::AddSceneLight directly - the AsyncLightOcclusionMgr will handle that for us
			}

		}

		if (coronaSize>0.0f)
		{		
			bool useDir = coronaFlare>0.0f;
			Vec3V vDir = Vec3V(V_ZERO);
			if (useDir)
			{
				Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix().b);
				Vec3V_ConstRef vCamUp = RCC_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix().c);
				vDir = Lerp(ScalarVFromF32(coronaFlare), vCamUp, -vCamForward);
			}

			u8 coronaFlags = 0;

			if (m_lightInfos[i].coronaOnlyInReflection || m_lightInfos[i].coronasAdded)
			{
				coronaFlags |= CORONA_ONLY_IN_REFLECTION;
			}
			
			if (m_lightInfos[i].coronaNotInReflection)
			{
				coronaFlags |= CORONA_DONT_REFLECT;
			}

			coronaFlags |= (useDir) ? CORONA_HAS_DIRECTION : 0;

			g_coronas.Register(vLightPos,
				coronaSize,
				vCoronaCol,
				coronaIntensity,
				coronaZBias,
				vDir,
				1.0f,
				CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
			    CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
				coronaFlags);
				
			m_lightInfos[i].coronasAdded = false;
		}
	}

	if (!fwTimer::IsGamePaused())
	{
		m_numLightInfos = 0;
	}
}

///////////////////////////////////////////////////////////////////////////////
//	StoreFogVolume
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreFogVolume			(ptfxFogVolumeInfo& fogVolumeInfo)
{

	FogVolume_t fogVolumeParams;
	fogVolumeParams.pos = fogVolumeInfo.vPos;
	fogVolumeParams.scale = fogVolumeInfo.vScale;
	fogVolumeParams.rotation = fogVolumeInfo.vRotation;
	fogVolumeParams.range = fogVolumeInfo.range;
	fogVolumeParams.density = fogVolumeInfo.density;
	fogVolumeParams.fallOff = fogVolumeInfo.falloff;
	fogVolumeParams.hdrMult = fogVolumeInfo.hdrMult;
	fogVolumeParams.col = fogVolumeInfo.col;
	fogVolumeParams.lightingType = (FogVolumeLighting_e)fogVolumeInfo.lightingType;
	fogVolumeParams.useGroundFogColour = fogVolumeInfo.useGroundFogColour;
	fogVolumeParams.interiorHash = CVfxHelper::GetHashFromInteriorLocation(fogVolumeInfo.interiorLocation);

	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeFogVolumeCS);
	g_fogVolumeMgr.Register(fogVolumeParams);

}

///////////////////////////////////////////////////////////////////////////////
//	DrawFogVolume
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::DrawFogVolume			(ptfxFogVolumeInfo& fogVolumeInfo)
{
	//Disable fog volume renders during mirror reflection and see through renderphases
	if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION || g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SIMPLE)
	{
		return;
	}

	FogVolume_t fogVolumeParams;
	fogVolumeParams.pos = fogVolumeInfo.vPos;
	fogVolumeParams.scale = fogVolumeInfo.vScale;
	fogVolumeParams.rotation = fogVolumeInfo.vRotation;
	fogVolumeParams.range = fogVolumeInfo.range;
	fogVolumeParams.density = fogVolumeInfo.density;
	fogVolumeParams.fallOff = fogVolumeInfo.falloff;
	fogVolumeParams.hdrMult = fogVolumeInfo.hdrMult;
	fogVolumeParams.col = fogVolumeInfo.col;
	fogVolumeParams.lightingType = (FogVolumeLighting_e)fogVolumeInfo.lightingType;
	fogVolumeParams.useGroundFogColour = fogVolumeInfo.useGroundFogColour;
	fogVolumeParams.interiorHash = CVfxHelper::GetHashFromInteriorLocation(fogVolumeInfo.interiorLocation);
	BANK_ONLY(fogVolumeParams.debugRender = true;)
	g_fogVolumeMgr.RenderImmediate(fogVolumeParams);

}
///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredLights
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessLateStoredLights		()
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));
	
	for (int i=0; i<m_numLightInfos; i++)
	{
		Vec3V vLightPos = m_lightInfos[i].vPos;
		Vec3V vCoronaCol(m_lightInfos[i].coronaCol.GetRedf(), m_lightInfos[i].coronaCol.GetGreenf(), m_lightInfos[i].coronaCol.GetBluef());
		float coronaIntensity = m_lightInfos[i].coronaIntensity;
		float coronaSize = m_lightInfos[i].coronaSize;
		float coronaFlare = m_lightInfos[i].coronaFlare;
		float coronaZBias = m_lightInfos[i].coronaZBias;

		if (coronaSize>0.0f)
		{		
			bool useDir = coronaFlare>0.0f;
			Vec3V vDir = Vec3V(V_ZERO);
			if (useDir)
			{
				Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix().b);
				Vec3V_ConstRef vCamUp = RCC_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix().c);
				vDir = Lerp(ScalarVFromF32(coronaFlare), vCamUp, -vCamForward);
			}

			bool added = false;
			if( !m_lightInfos[i].coronaOnlyInReflection )
			{
				u8 coronaFlags = 0;

				if (m_lightInfos[i].coronaNotInReflection)
				{
					coronaFlags |= CORONA_DONT_REFLECT;
				}

				coronaFlags |= (useDir) ? CORONA_HAS_DIRECTION : 0;

				added = g_coronas.LateRegister(vLightPos,
					coronaSize,
					vCoronaCol,
					coronaIntensity,
					coronaZBias,
					vDir,
					1.0f,
					CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER,
					CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER,
					coronaFlags);
			}
			m_lightInfos[i].coronasAdded = added;
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//	StoreFire
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreFire					(ptfxFireInfo& fireInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeFireCS);
	if (m_numFireInfos<PTFX_MAX_STORED_FIRES)
	{
		m_fireInfos[m_numFireInfos++] = fireInfo;
	}
	else
	{
		ptfxWarningf("trying to add too many fires from particle effects this frame");
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredFires
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredFires		()
{
	for (int i=0; i<m_numFireInfos; i++)
	{
		g_fireMan.StartMapFire(
			m_fireInfos[i].vPos,				
			m_fireInfos[i].vNormal,				
			m_fireInfos[i].mtlId,				
			NULL,						
			m_fireInfos[i].burnTime, 
			m_fireInfos[i].burnStrength, 
			m_fireInfos[i].peakTime, 
			m_fireInfos[i].fuelLevel, 
			m_fireInfos[i].fuelBurnRate, 
			m_fireInfos[i].numGenerations,
			Vec3V(V_ZERO), 
			BANK_ONLY(Vec3V(V_ZERO),) 
			false);
	}

	m_numFireInfos = 0;
}


///////////////////////////////////////////////////////////////////////////////
//	StoreDecal
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreDecal					(ptfxDecalInfo& decalInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeDecalCS);
	if (m_numDecalInfos<PTFX_MAX_STORED_DECALS)
	{
		m_decalInfos[m_numDecalInfos++] = decalInfo;
	}
	else
	{
		ptfxWarningf("trying to add too many decals from particle effects this frame");
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredDecals
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredDecals		()
{
	for (int i=0; i<m_numDecalInfos; i++)
	{
		if(m_decalInfos[i].pEffectInst == NULL)
		{
			continue;
		}

		// get random side vector
		Vec3V vSide;
		if (m_decalInfos[i].isDirectional)
		{
			Vec3V vAlign = m_decalInfos[i].vPos - m_decalInfos[i].pEffectInst->GetCurrPos();
			vAlign = Normalize(vAlign);
			if (CVfxHelper::GetRandomTangentAlign(vSide, -m_decalInfos[i].vDir, vAlign)==false)
			{
				return;
			}
		}
		else
		{
			if (CVfxHelper::GetRandomTangent(vSide, -m_decalInfos[i].vDir)==false)
			{
				return;
			}
		}

		s32 decalRenderSettingIndex;
		s32 decalRenderSettingCount;
		g_decalMan.FindRenderSettingInfo(m_decalInfos[i].decalId, decalRenderSettingIndex, decalRenderSettingCount);

		Color32 col(m_decalInfos[i].colR, m_decalInfos[i].colG, m_decalInfos[i].colB, m_decalInfos[i].colA);

		float width = m_decalInfos[i].width;
		float height = m_decalInfos[i].height;
		if (m_decalInfos[i].distanceScale!=0.0f)
		{
			float dist = Dist(m_decalInfos[i].vPos, m_decalInfos[i].pEffectInst->GetCurrPos()).Getf();
			float whMult = 1.0f + (dist*m_decalInfos[i].distanceScale);
			whMult = MAX(0.0f, whMult);
			width *= whMult;
			height *= whMult;
		}

		DecalType_e decalType = DECALTYPE_GENERIC_SIMPLE_COLN;
		if (m_decalInfos[i].useComplexColn)
		{
			decalType = DECALTYPE_GENERIC_COMPLEX_COLN;
		}

		g_decalMan.AddGeneric(decalType, decalRenderSettingIndex, decalRenderSettingCount, m_decalInfos[i].vPos, m_decalInfos[i].vDir, vSide, width, height, m_decalInfos[i].projectionDepth, m_decalInfos[i].totalLife, m_decalInfos[i].fadeInTime, m_decalInfos[i].uvMultStart, m_decalInfos[i].uvMultEnd, m_decalInfos[i].uvMultTime, col, m_decalInfos[i].flipU, m_decalInfos[i].flipV, m_decalInfos[i].duplicateRejectDist, false, false REPLAY_ONLY(, 0.0f));
	}

	m_numDecalInfos = 0;
}


///////////////////////////////////////////////////////////////////////////////
//	StoreDecalPool
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreDecalPool				(ptfxDecalPoolInfo& decalPoolInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeDecalPoolCS);
	if (m_numDecalPoolInfos<PTFX_MAX_STORED_DECAL_POOLS)
	{
		m_decalPoolInfos[m_numDecalPoolInfos++] = decalPoolInfo;
	}
	else
	{
		ptfxWarningf("trying to add too many decal pools from particle effects this frame");
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredDecalPools
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredDecalPools		()
{
	for (int i=0; i<m_numDecalPoolInfos; i++)
	{
		VfxLiquidType_e liquidType = VFXLIQUID_TYPE_NONE;
		s32 decalRenderSettingIndex = -1;
		s32 decalRenderSettingCount = 0;
		if (m_decalPoolInfos[i].liquidType>=0)
		{
			liquidType = (VfxLiquidType_e)m_decalPoolInfos[i].liquidType;
		}
		else
		{
			g_decalMan.FindRenderSettingInfo(m_decalPoolInfos[i].decalRenderSettingId, decalRenderSettingIndex, decalRenderSettingCount);
		}

		Color32 col(m_decalPoolInfos[i].colR, m_decalPoolInfos[i].colG, m_decalPoolInfos[i].colB, m_decalPoolInfos[i].colA);
		g_decalMan.AddPool(liquidType, decalRenderSettingIndex, decalRenderSettingCount, m_decalPoolInfos[i].vPos, m_decalPoolInfos[i].vNormal, m_decalPoolInfos[i].startSize, m_decalPoolInfos[i].endSize, m_decalPoolInfos[i].growthRate, m_decalPoolInfos[i].mtlId, col, false);
	}

	m_numDecalPoolInfos = 0;
}


///////////////////////////////////////////////////////////////////////////////
//	StoreDecalTrail
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreDecalTrail				(ptfxDecalTrailInfo& decalTrailInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeDecalTrailCS);
	if (m_numDecalTrailInfos<PTFX_MAX_STORED_DECAL_TRAILS)
	{
		// check we haven't stored a decal trail this frame already for this effect
		for (int i=0; i<m_numDecalTrailInfos; i++)
		{
			if (m_decalTrailInfos[i].pEffectInst == decalTrailInfo.pEffectInst)
			{
				return;
			}
		}

		m_decalTrailInfos[m_numDecalTrailInfos++] = decalTrailInfo;
	}
	else
	{
		ptfxWarningf("trying to add too many decal trails from particle effects this frame");
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredDecalTrails
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredDecalTrails		()
{
	for (int i=0; i<m_numDecalTrailInfos; i++)
	{
		if(m_decalTrailInfos[i].pEffectInst == NULL)
		{
			continue;
		}

		VfxTrailType_e trailType = VFXTRAIL_TYPE_GENERIC;
		s32 decalRenderSettingIndex = -1;
		s32 decalRenderSettingCount = 0;
		u8 colR = m_decalTrailInfos[i].colR;
		u8 colG = m_decalTrailInfos[i].colG;
		u8 colB = m_decalTrailInfos[i].colB;
		u8 colA = m_decalTrailInfos[i].colA;
		if (m_decalTrailInfos[i].liquidType>=0)
		{
			VfxLiquidType_e liquidType = (VfxLiquidType_e)m_liquidInfos[i].liquidType;
			VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(liquidType);

			trailType = (VfxTrailType_e)(VFXTRAIL_TYPE_LIQUID+liquidType);

			// multiply the liquid colour by the particle colour
			colR = static_cast<u8>(liquidInfo.decalColR * (m_decalTrailInfos[i].colR/255.0f));
			colG = static_cast<u8>(liquidInfo.decalColG * (m_decalTrailInfos[i].colG/255.0f));
			colB = static_cast<u8>(liquidInfo.decalColB * (m_decalTrailInfos[i].colB/255.0f));
			colA = static_cast<u8>(liquidInfo.decalColA * (m_decalTrailInfos[i].colA/255.0f));
		}
		else
		{
			g_decalMan.FindRenderSettingInfo(m_decalTrailInfos[i].decalRenderSettingId, decalRenderSettingIndex, decalRenderSettingCount);
		}

		VfxTrailInfo_t vfxTrailInfo;
		vfxTrailInfo.id							= fwIdKeyGenerator::Get(m_decalTrailInfos[i].pEffectInst, 0);
		vfxTrailInfo.type						= trailType;
		vfxTrailInfo.regdEnt					= NULL;
		vfxTrailInfo.componentId				= 0;
		vfxTrailInfo.vWorldPos					= m_decalTrailInfos[i].vPos;
		vfxTrailInfo.vNormal					= m_decalTrailInfos[i].vNormal;
		vfxTrailInfo.vDir						= Vec3V(V_ZERO);
		vfxTrailInfo.vForwardCheck				= Vec3V(V_ZERO);
		vfxTrailInfo.decalRenderSettingIndex	= decalRenderSettingIndex;
		vfxTrailInfo.decalRenderSettingCount	= decalRenderSettingCount;
		vfxTrailInfo.decalLife					= -1.0f;
		vfxTrailInfo.width						= m_decalTrailInfos[i].widthMin;
		vfxTrailInfo.col						= Color32(colR, colG, colB, colA);
		vfxTrailInfo.pVfxMaterialInfo			= NULL;
		vfxTrailInfo.mtlId			 			= m_decalTrailInfos[i].mtlId;
		vfxTrailInfo.liquidPoolStartSize		= 0.0f;
		vfxTrailInfo.liquidPoolEndSize			= 0.0f;
		vfxTrailInfo.liquidPoolGrowthRate		= 0.0f;
		vfxTrailInfo.createLiquidPools			= false;
		vfxTrailInfo.dontApplyDecal 			= false;

		g_vfxTrail.RegisterPoint(vfxTrailInfo);
	}

	m_numDecalTrailInfos = 0;
}


///////////////////////////////////////////////////////////////////////////////
//	StoreLiquid
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreLiquid				(ptfxLiquidInfo& liquidInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeLiquidCS);
	if (m_numLiquidInfos<PTFX_MAX_STORED_LIQUIDS)
	{
		// check we haven't stored a liquid this frame already for this effect
		for (int i=0; i<m_numLiquidInfos; i++)
		{
			if (m_liquidInfos[i].pEffectInst == liquidInfo.pEffectInst)
			{
				return;
			}
		}

		m_liquidInfos[m_numLiquidInfos++] = liquidInfo;
	}
	else
	{
		ptfxWarningf("trying to add too many liquids from particle effects this frame");
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredLiquids
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredLiquids		()
{
	for (int i=0; i<m_numLiquidInfos; i++)
	{
		if(m_liquidInfos[i].pEffectInst == NULL)
		{
			continue;
		}

		VfxLiquidType_e liquidType = (VfxLiquidType_e)m_liquidInfos[i].liquidType;
		VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(liquidType);

		// check for blowing up vehicles when petrol leaking from them is too close to a fire
		if (liquidType==VFXLIQUID_TYPE_PETROL)
		{
			fwEntity* pAttachEntity = ptfxAttach::GetAttachEntity(m_liquidInfos[i].pEffectInst);
			if (pAttachEntity)
			{
				CEntity* pEntity = static_cast<CEntity*>(pAttachEntity);
				if (pEntity->GetIsTypeVehicle())
				{
					CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
					if (pVehicle && !pVehicle->IsOnFire())
					{
						// check if there is a fire nearby
						float nearestDistSqr = 3.0f;
						if (g_fireMan.FindNearestFire(m_liquidInfos[i].vPos, nearestDistSqr))
						{
							// this is - start a vehicle fire at the effect location
							const float fBurnTime     = fwRandom::GetRandomNumberInRange(20.0f, 35.0f);
							const float fBurnStrength = fwRandom::GetRandomNumberInRange(0.75f, 1.0f);
							const float fPeakTime	  = fwRandom::GetRandomNumberInRange(0.5f, 1.0f);

							Vec3V vPosLcl = CVfxHelper::GetLocalEntityPos(*pVehicle, m_liquidInfos[i].pEffectInst->GetCurrPos());
							Vec3V vNormLcl = CVfxHelper::GetLocalEntityDir(*pVehicle, Vec3V(V_Z_AXIS_WZERO));

							g_fireMan.StartVehicleFire(pVehicle, -1, vPosLcl, vNormLcl, NULL, fBurnTime, fBurnStrength, fPeakTime, FIRE_DEFAULT_NUM_GENERATIONS, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);

							return;
						}
					}
				}
			}
		}

		// multiply the liquid colour by the particle colour
		u8 colR = static_cast<u8>(liquidInfo.decalColR * (m_liquidInfos[i].colR/255.0f));
		u8 colG = static_cast<u8>(liquidInfo.decalColG * (m_liquidInfos[i].colG/255.0f));
		u8 colB = static_cast<u8>(liquidInfo.decalColB * (m_liquidInfos[i].colB/255.0f));
		u8 colA = static_cast<u8>(liquidInfo.decalColA * (m_liquidInfos[i].colA/255.0f));

		VfxTrailInfo_t vfxTrailInfo;
		vfxTrailInfo.id							= fwIdKeyGenerator::Get(m_liquidInfos[i].pEffectInst, 0);
		vfxTrailInfo.type						= (VfxTrailType_e)(VFXTRAIL_TYPE_LIQUID+liquidType);;
		vfxTrailInfo.regdEnt					= NULL;
		vfxTrailInfo.componentId				= 0;
		vfxTrailInfo.vWorldPos					= m_liquidInfos[i].vPos;
		vfxTrailInfo.vNormal					= m_liquidInfos[i].vNormal;
		vfxTrailInfo.vDir						= Vec3V(V_ZERO);
		vfxTrailInfo.vForwardCheck				= Vec3V(V_ZERO);
		vfxTrailInfo.decalRenderSettingIndex	= -1;
		vfxTrailInfo.decalRenderSettingCount	= -1;
		vfxTrailInfo.decalLife					= -1.0f;
		vfxTrailInfo.width						= m_liquidInfos[i].trailWidthMin;
		vfxTrailInfo.col						= Color32(colR, colG, colB, colA);
		vfxTrailInfo.pVfxMaterialInfo			= NULL;
		vfxTrailInfo.mtlId			 			= m_liquidInfos[i].mtlId;
		vfxTrailInfo.liquidPoolStartSize		= m_liquidInfos[i].poolStartSize;
		vfxTrailInfo.liquidPoolEndSize			= m_liquidInfos[i].poolEndSize;
		vfxTrailInfo.liquidPoolGrowthRate		= m_liquidInfos[i].poolGrowthRate;
		vfxTrailInfo.createLiquidPools			= true;
		vfxTrailInfo.dontApplyDecal 			= false;

		g_vfxTrail.RegisterPoint(vfxTrailInfo);
	}

	m_numLiquidInfos = 0;
}


///////////////////////////////////////////////////////////////////////////////
//	StoreGlassImpact
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreGlassImpact			(ptfxGlassImpactInfo& glassImpactInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeGlassImpactCS);
	if (ptfxVerifyf(IsFiniteAll(glassImpactInfo.vPos), "glass impact position is invalid"))
	{
		if (m_numGlassImpactInfos<PTFX_MAX_STORED_GLASS_IMPACTS)
		{
			m_glassImpactInfos[m_numGlassImpactInfos++] = glassImpactInfo;
		}
		else
		{
			ptfxWarningf("trying to add too many glass impact particle effects this frame");
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredGlassImpacts
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredGlassImpacts	()
{
	for (int i=0; i<m_numGlassImpactInfos; i++)
	{
		if (m_glassImpactInfos[i].RegdEnt)
		{
			fwInteriorLocation interiorLocation;
			interiorLocation.MakeInvalid();
			g_vfxWeapon.TriggerPtFxBulletImpact(m_glassImpactInfos[i].pFxWeaponInfo, m_glassImpactInfos[i].RegdEnt, m_glassImpactInfos[i].vPos, m_glassImpactInfos[i].vNormal, -m_glassImpactInfos[i].vNormal, m_glassImpactInfos[i].isExitFx, NULL, false, interiorLocation, false);
		}
	}

	m_numGlassImpactInfos = 0;
}


///////////////////////////////////////////////////////////////////////////////
//	StoreGlassSmash
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreGlassSmash			(ptfxGlassSmashInfo& glassSmashInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeGlassSmashCS);
	if (ptfxVerifyf(IsFiniteAll(glassSmashInfo.vPosHit), "glass smash hit position is invalid") && 
		ptfxVerifyf(IsFiniteAll(glassSmashInfo.vPosCenter), "glass smash centre position is invalid"))
	{
		if (m_numGlassSmashInfos<PTFX_MAX_STORED_GLASS_SMASHES)
		{
			m_glassSmashInfos[m_numGlassSmashInfos++] = glassSmashInfo;
		}
		else
		{
			ptfxWarningf("trying to add too many glass smash particle effects this frame");
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredGlassSmashes
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredGlassSmashes	()
{
	for (int i=0; i<m_numGlassSmashInfos; i++)
	{
		if (m_glassSmashInfos[i].regdPhy)
		{
			g_vfxGlass.TriggerPtFxGlassSmash(m_glassSmashInfos[i].regdPhy, m_glassSmashInfos[i].vPosHit, m_glassSmashInfos[i].vNormal, m_glassSmashInfos[i].force);

			if (m_glassSmashInfos[i].isWindscreen)
			{
				g_vfxGlass.TriggerPtFxVehicleWindscreenSmash(m_glassSmashInfos[i].regdPhy, m_glassSmashInfos[i].vPosCenter, m_glassSmashInfos[i].vNormal, m_glassSmashInfos[i].force);
			}
			else
			{
				g_vfxGlass.TriggerPtFxVehicleWindowSmash(m_glassSmashInfos[i].regdPhy, m_glassSmashInfos[i].vPosCenter, m_glassSmashInfos[i].vNormal, m_glassSmashInfos[i].force);
			}
		}
	}

	m_numGlassSmashInfos = 0;
}


///////////////////////////////////////////////////////////////////////////////
//	StoreGlassShatter
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StoreGlassShatter			(ptfxGlassShatterInfo& glassShatterInfo)
{
	//Multiple particle update tasks could call this at the same time, so adding critical section
	SYS_CS_SYNC(m_storeGlassShatterCS);
	if (ptfxVerifyf(IsFiniteAll(glassShatterInfo.vPos), "glass shatter position is invalid"))
	{
		if (m_numGlassShatterInfos<PTFX_MAX_STORED_GLASS_SHATTERS)
		{
			// check if there isn't one already stored too close
			for (int i=0; i<m_numGlassShatterInfos; i++)
			{
				Vec3V vDiff = glassShatterInfo.vPos - m_glassShatterInfos[i].vPos;
				if (MagSquared(vDiff).Getf()<=VFXHISTORY_GLASS_HIT_GROUND_DIST_SQR)
				{
					return;
				}
			}

			m_glassShatterInfos[m_numGlassShatterInfos++] = glassShatterInfo;
		}
		// 	else
		// 	{
		// 		ptfxWarningf("trying to add too many glass shatter particle effects this frame");
		// 	}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	ProcessStoredGlassShatters
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredGlassShatters	()
{
	for (int i=0; i<m_numGlassShatterInfos; i++)
	{
		g_vfxGlass.TriggerPtFxGlassHitGround(m_glassShatterInfos[i].vPos);
	}

	m_numGlassShatterInfos = 0;
}

#if USE_SHADED_PTFX_MAP
///////////////////////////////////////////////////////////////////////////////
//	GetPtfxShadedParams
///////////////////////////////////////////////////////////////////////////////

Vec4V_Out		CPtFxManager::GetPtfxShadedParams			()
{

	ScalarV vMapZ = CVfxHelper::GetGameCamPos().GetZ();
	const ScalarV ptFxShadedRange	 =	(BANK_SWITCH(g_ptFxManager.m_gameDebug.GetShadowedPtFxRange(),		PTFX_SHADOWED_RANGE_DEFAULT));
	CPed *playerPed = FindPlayerPed();
	if (playerPed)
	{
		Vec3V playerPos = playerPed->GetTransform().GetPosition();

		// we only care about the XY plane for this calculation, height doesn't matter.  Just make sure they are the same
		playerPos.SetZ(vMapZ);
		ScalarV camDist = Sqrt(CVfxHelper::GetDistSqrToCameraV(playerPos));

		// if the camera is close enough to the player, use the players position.  This should always
		// be the case unless we are in a cutscene or something like that where the camera could be
		// far away form the actual player.
		if (IsLessThanAll(camDist, ptFxShadedRange * ScalarV(V_HALF)))
		{
			vMapZ = playerPed->GetTransform().GetPosition().GetZ();
		}
	}

	//BankFloat ptFxShadedRange	 =	BANK_SWITCH(g_ptFxManager.m_gameDebug.GetShadowedPtFxRange(),		PTFX_SHADOWED_RANGE_DEFAULT);
	//BankFloat ptFxShadedLoHeight =  BANK_SWITCH(g_ptFxManager.m_gameDebug.GetShadowedPtFxLoHeight(),	PTFX_SHADOWED_LOW_HEIGHT_DEFAULT);
	//BankFloat ptFxShadedHiHeight =  BANK_SWITCH(g_ptFxManager.m_gameDebug.GetShadowedPtFxHiHeight(),	PTFX_SHADOWED_HIGH_HEIGHT_DEFAULT);

	//Vector4 shaderParams(ptFxShadedRange, fMapZ + ptFxShadedLoHeight, fMapZ + ptFxShadedHiHeight, ptFxShadedRange * 0.8f);

	Vec4V ptfxShadedParams = Vec4V(PTFX_SHADOWED_RANGE_DEFAULT, PTFX_SHADOWED_LOW_HEIGHT_DEFAULT, PTFX_SHADOWED_HIGH_HEIGHT_DEFAULT, PTFX_SHADOWED_RANGE_DEFAULT * ScalarVConstant<FLOAT_TO_INT(0.8f)>());

#if __BANK
	ptfxShadedParams = Vec4V(
		g_ptFxManager.m_gameDebug.GetShadowedPtFxRange(),
		g_ptFxManager.m_gameDebug.GetShadowedPtFxLoHeight(),
		g_ptFxManager.m_gameDebug.GetShadowedPtFxHiHeight(),
		g_ptFxManager.m_gameDebug.GetShadowedPtFxRange() * ScalarVConstant<FLOAT_TO_INT(0.8f)>()
		);
#endif		
	ptfxShadedParams += Vec4V(ScalarV(V_ZERO), Vec2V(vMapZ), ScalarV(V_ZERO));
	return ptfxShadedParams;
}
#endif //USE_SHADED_PTFX_MAP

///////////////////////////////////////////////////////////////////////////////
//	ResolvePtfxMaps
///////////////////////////////////////////////////////////////////////////////
#if DEVICE_MSAA
void			CPtFxManager::ResolvePtfxMaps()
{
	static_cast<grcRenderTargetMSAA*>(m_particleDepthMap)->Resolve(m_particleDepthMapResolved);
	static_cast<grcRenderTargetMSAA*>(m_particleAlphaMap)->Resolve(m_particleAlphaMapResolved);
}
#endif //DEVICE_MSAA

///////////////////////////////////////////////////////////////////////////////
//	TryToSoakPed
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::TryToSoakPed					(CPed* pPed, ptxDataVolume& dataVolume)
{
	if (pPed)
	{
		// check if the ped is sheltered from the water
		bool isSheltered = false;
		if (pPed->GetIsInVehicle())
		{
			isSheltered = true;

			CVehicle* pVehicle = pPed->GetMyVehicle();
			if (pVehicle)
			{
				// go through the vehicle windows to see if any are down or smashed
				for (int i=VEH_FIRST_WINDOW; i<=VEH_LAST_WINDOW; i++)
				{
					eHierarchyId hierarchyId = (eHierarchyId)i;

					if (pVehicle->IsWindowDown(hierarchyId))
					{
						isSheltered = false;
						break;
					}

					s32 iWindowComponent = pVehicle->GetFragmentComponentIndex(hierarchyId);
					if (iWindowComponent>-1)
					{
						if (pVehicle->IsHiddenFlagSet(iWindowComponent))
						{
							isSheltered = false;
							break;
						}
					}
				}

				// check if the vehicle has a soft top roof that is down
				if (isSheltered)
				{
					if (pPed->GetAttachState()==ATTACH_STATE_PED_IN_CAR)
					{
						VehicleType vehType = pVehicle->GetVehicleType();
						if (vehType==VEHICLE_TYPE_CAR)
						{
							bool hasRoof = !pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_NO_ROOF);
							if (hasRoof)
							{
								if (pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
								{
									if (pVehicle->GetConvertibleRoofState()!=CTaskVehicleConvertibleRoof::STATE_RAISED)
									{
										isSheltered = false;
									}
								}
							}
						}
						else if (vehType==VEHICLE_TYPE_BIKE || vehType==VEHICLE_TYPE_BICYCLE || vehType==VEHICLE_TYPE_BOAT)
						{
							isSheltered = false;
						}
					}
				}
			}
		}

		if (isSheltered==false)
		{
			Vec3V_ConstRef vPedPos = pPed->GetTransform().GetMatrix().GetCol3();
			Vec3V_ConstRef vPedForward = pPed->GetTransform().GetMatrix().GetCol1();
			float strength = dataVolume.GetPointStrength(vPedPos, vPedForward, false, true, false, false);
			if (strength>0.0f)
			{
				float incLevel = 0.0f;

				u8 dataVolumeType = dataVolume.GetType();
				if (dataVolumeType==PTFX_DATAVOLUMETYPE_SOAK_SLOW)
				{
					incLevel = PTFX_SOAK_SLOW_INC_LEVEL_LOW + strength*(PTFX_SOAK_SLOW_INC_LEVEL_HIGH-PTFX_SOAK_SLOW_INC_LEVEL_LOW);
				}
				else if (dataVolumeType==PTFX_DATAVOLUMETYPE_SOAK_MEDIUM)
				{
					incLevel = PTFX_SOAK_MEDIUM_INC_LEVEL_LOW + strength*(PTFX_SOAK_MEDIUM_INC_LEVEL_HIGH-PTFX_SOAK_MEDIUM_INC_LEVEL_LOW);
				}
				else if (dataVolumeType==PTFX_DATAVOLUMETYPE_SOAK_FAST)
				{
					incLevel = PTFX_SOAK_FAST_INC_LEVEL_LOW + strength*(PTFX_SOAK_FAST_INC_LEVEL_HIGH-PTFX_SOAK_FAST_INC_LEVEL_LOW);
				}

				pPed->IncWetClothing(incLevel);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//	ShouldSceneSort
///////////////////////////////////////////////////////////////////////////////

#if PTFX_ALLOW_INSTANCE_SORTING
bool			CPtFxManager::ShouldSceneSort				(ptxEffectInst* BANK_ONLY(pFxInst))
{
	// scene sorting 
	bool sortMe = true;

#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetOnlyFlaggedSortedEffects())
	{
		sortMe = pFxInst->GetEffectRule()->GetGameFlag(0);
	}
	else if (g_ptFxManager.GetGameDebugInterface().GetDisableSortedEffects())
	{
		sortMe = false;
	}
	else if (g_ptFxManager.GetGameDebugInterface().GetForceSortedEffects())
	{
		sortMe = true;
	}
#endif

	return sortMe;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//	UpdateDataCapsule
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::UpdateDataCapsule				(ptxEffectInst* pFxInst)
{
	static atHashValue flareTrailHashValue = atHashValue("proj_heist_flare_trail");
	if (atHashValue(pFxInst->GetEffectRule()->GetName())==flareTrailHashValue)
	{
		return;
	}

	if (pFxInst->GetIgnoreDataCapsuleTests())
	{
		return;
	}

	ptxDataVolume dataVolume;
	pFxInst->GetDataVolume(&dataVolume);

	if (dataVolume.GetRadius()>0.0f)
	{
		u8 dataVolumeType = dataVolume.GetType();

		// fire
		if (dataVolumeType==PTFX_DATAVOLUMETYPE_FIRE_CONTAINED || dataVolumeType==PTFX_DATAVOLUMETYPE_FIRE || dataVolumeType==PTFX_DATAVOLUMETYPE_FIRE_FLAMETHROWER)
		{
			CPed* pProjectilePedOwner = NULL;
			if (NetworkInterface::IsGameInProgress())
			{
				ptfxAttachInfo* pAttachInfo = ptfxAttach::Find(pFxInst);
				if (pAttachInfo)
				{
					fwEntity* pfwEntity = pAttachInfo->m_pAttachEntity.Get();
					if (pfwEntity)
					{
						if (static_cast<CEntity*>(pfwEntity)->GetIsTypeObject())
						{
							CEntity* pAttachEntity = static_cast<CEntity*>(pfwEntity);
							if (pAttachEntity)
							{
								//Check if the projectile is valid.
								const CProjectile* pProjectile = static_cast<const CObject *>(pAttachEntity)->GetAsProjectile();
								if(pProjectile)
								{
									//Set the projectile owner.
									CEntity* pOwner = pProjectile->GetOwner();
									if (pOwner && pOwner->GetIsTypePed())
									{
										pProjectilePedOwner = static_cast<CPed*>(pOwner);
									}
								}
							}
						}
					}
				}
			}

			// register a fire here
			CEntity* pAttachEntity = static_cast<CEntity*>(ptfxAttach::GetAttachEntity(pFxInst));
			if (pAttachEntity && pAttachEntity->GetIsTypeObject())
			{
				Vec3V vPosLcl = CVfxHelper::GetLocalEntityPos(*pAttachEntity, dataVolume.GetPosWldStart());
				g_fireMan.RegisterPtFxFire(pAttachEntity, vPosLcl, pFxInst, 0, dataVolume.GetRadius(), dataVolumeType==PTFX_DATAVOLUMETYPE_FIRE_CONTAINED, dataVolumeType==PTFX_DATAVOLUMETYPE_FIRE_FLAMETHROWER, pProjectilePedOwner);
			}
			else
			{
				CPed* inflictor = nullptr;
				if(pAttachEntity && pAttachEntity->GetIsTypeVehicle())
				{
					inflictor = SafeCast(CVehicle, pAttachEntity)->GetDriver();
				}
				g_fireMan.RegisterPtFxFire(NULL, dataVolume.GetPosWldStart(), pFxInst, 0, dataVolume.GetRadius(), dataVolumeType==PTFX_DATAVOLUMETYPE_FIRE_CONTAINED, dataVolumeType==PTFX_DATAVOLUMETYPE_FIRE_FLAMETHROWER, inflictor);
			}		
		}
		
		// water extinguishes fires
		if (dataVolumeType==PTFX_DATAVOLUMETYPE_WATER)
		{
			// extinguish any fires
			g_fireMan.ExtinguishAreaSlowly(dataVolume);
		}

		// water soaks peds
		if (dataVolumeType==PTFX_DATAVOLUMETYPE_SOAK_SLOW || 
			dataVolumeType==PTFX_DATAVOLUMETYPE_SOAK_MEDIUM || 
			dataVolumeType==PTFX_DATAVOLUMETYPE_SOAK_FAST)
		{
			const bool TRY_TO_SOAK_ALL_PEDS = true;
			if (TRY_TO_SOAK_ALL_PEDS)
			{
				// try to soak all peds
				CPed::Pool* pPedPool = CPed::GetPool();
				CPed* pPed;
				s32 i = (s32)pPedPool->GetSize();
				while (i--)
				{
					pPed = pPedPool->GetSlot(i);
					TryToSoakPed(pPed, dataVolume);
				}
			}
			else
			{
				// try to soak the player ped
				CPed* pPed = CGameWorld::FindLocalPlayer();
				CPed* pLocalPlayerPed = pPed;
				TryToSoakPed(pPed, dataVolume);

				// try to soak peds flagged as able to get wet
				for (s32 i=0; i<CPed::GetNumPedsThatCanGetWetThisFrame(); i++)
				{
					pPed = CPed::GetPedThatCanGetWetThisFrame(i);
					TryToSoakPed(pPed, dataVolume);
				}

				// try to soak multiplayer player peds
				unsigned numNetworkPlayers = netInterface::GetNumPhysicalPlayers();
				const netPlayer* const* ppNetworkPlayers = netInterface::GetAllPhysicalPlayers();
				for (unsigned index=0; index<numNetworkPlayers; index++)
				{
					const CNetGamePlayer* pNetworkPlayer = SafeCast(const CNetGamePlayer, ppNetworkPlayers[index]);
					pPed = pNetworkPlayer->GetPlayerPed();
					if (pPed && pPed!=pLocalPlayerPed)
					{
						TryToSoakPed(pPed, dataVolume);
					}
				}
			}

			// extinguish any fires
			g_fireMan.ExtinguishAreaSlowly(dataVolume);
		}

		// lens effects
		if (dataVolumeType==PTFX_DATAVOLUMETYPE_WATER || 
			dataVolumeType==PTFX_DATAVOLUMETYPE_SAND || 
			dataVolumeType==PTFX_DATAVOLUMETYPE_MUD || 
			dataVolumeType==PTFX_DATAVOLUMETYPE_BLOOD || 
			dataVolumeType==PTFX_DATAVOLUMETYPE_DUST || 
			dataVolumeType==PTFX_DATAVOLUMETYPE_SMOKE ||
			dataVolumeType==PTFX_DATAVOLUMETYPE_BLAZE)
		{
			// check if the camera is inside the data volume
			if (!CVfxHelper::GetCamInVehicleFlag())
			{
				Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetPosition());
				Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix().b);
				float strength = dataVolume.GetPointStrength(vCamPos, vCamForward, true, true, true, true);
				if (pFxInst->HasEvoFromHash(ATSTRINGHASH("smoke", 0xB19506B0)))
				{
					// if the parent effect has a smoke evo then use this instead (for interior smoke effects)
					strength = pFxInst->GetEvoValueFromHash(ATSTRINGHASH("smoke", 0xB19506B0));
				}

				if (strength>0.0f)
				{
					if (dataVolumeType==PTFX_DATAVOLUMETYPE_WATER)
					{
						g_vfxLens.Register(VFXLENSTYPE_WATER, strength, 0.0f, 0.0f, 1.0f);
					}
// 					else if (dataVolumeType==PTFX_DATAVOLUMETYPE_SAND)
// 					{
// 						g_vfxLens.Register(VFXLENSTYPE_SAND, strength, 0.0f, 0.0f, 1.0f);
// 					}
// 					else if (dataVolumeType==PTFX_DATAVOLUMETYPE_MUD)
// 					{
// 						g_vfxLens.Register(VFXLENSTYPE_MUD, strength, 0.0f, 0.0f, 1.0f);
// 					}
// 					else if (dataVolumeType==PTFX_DATAVOLUMETYPE_BLOOD)
// 					{
// 						g_vfxLens.Register(VFXLENSTYPE_BLOOD, strength, 0.0f, 0.0f, 1.0f);
// 					}
// 					else if (dataVolumeType==PTFX_DATAVOLUMETYPE_DUST)
// 					{
// 						g_vfxLens.Register(VFXLENSTYPE_DUST, strength, 0.0f, 0.0f, 1.0f);
// 					}
// 					else if (dataVolumeType==PTFX_DATAVOLUMETYPE_SMOKE)
// 					{
// 						g_vfxLens.Register(VFXLENSTYPE_SMOKE, strength, 0.0f, 0.0f, 1.0f);
// 					}
					else if (dataVolumeType==PTFX_DATAVOLUMETYPE_BLAZE)
					{
						g_vfxLens.Register(VFXLENSTYPE_BLAZE, strength, 0.0f, 0.0f, 1.0f);
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessStoredData
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::ProcessStoredData				()
{
	// process the stored requests from the update thread now that it is finished
	ProcessStoredLights();

	if (!fwTimer::IsGamePaused())
	{
		ProcessStoredFires();
		ProcessStoredDecals();
		ProcessStoredDecalPools();
		ProcessStoredDecalTrails();
		ProcessStoredLiquids();

		ProcessStoredGlassImpacts();
		ProcessStoredGlassSmashes();
		ProcessStoredGlassShatters();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  BeginFrame
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::BeginFrame					()
{
	if (g_ptxManager::IsInstantiated())
	{
		{ PF_AUTO_PUSH_TIMEBAR("WaitForUpdateToFinish");
			WaitForUpdateToFinish();
		}

#if __BANK
		if (ptxDebug::sm_drawFxlistReferences)
		{
			ptfxManager::GetAssetStore().DebugDrawFxListReferences();
			RMPTFXMGR.DebugDrawFxListReferences();
		}
#endif

#if RMPTFX_THOROUGH_ERROR_CHECKING
		RMPTFXMGR.VerifyLists();
#endif
		{ PF_AUTO_PUSH_TIMEBAR("RMPTFXMGR.BeginFrame");
			RMPTFXMGR.BeginFrame();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  WaitForUpdateToFinish
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::WaitForUpdateToFinish()
{
	PF_FUNC(TimerExt_WaitForUpdateToFinish);
	// wait for the update thread to finish - if started
	if (m_startedUpdate)
	{
		sysIpcWaitSema(m_finishUpdateSema);
	}
	m_startedUpdate = false;
}

///////////////////////////////////////////////////////////////////////////////
//  WaitForUpdateToFinish
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::WaitForCallbackEffectsToFinish()
{
	// wait for the first part of the update thread to finish - if started
	if (m_startedUpdate)
	{
		RMPTFXMGR.WaitForCallbackEffects();
	}
}

///////////////////////////////////////////////////////////////////////////////
//  StartLocalEffectsUpdate
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::StartLocalEffectsUpdate()
{
	if (m_startedUpdate)
	{
		RMPTFXMGR.StartLocalEffectsUpdate();
	}	
	ptfxManager::GetAssetStore().SetIsSafeToRemoveFxLists(true);
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateThread
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::UpdateThread				(void* _that)
{
	USE_MEMBUCKET(MEMBUCKET_FX);

	if (PARAM_ptfxtimebar.Get())
	{
		PF_INIT_TIMEBARS("RMPTFX Update", 50, 3000);
	}

	CPtFxManager* that = (CPtFxManager*)_that;

	sysThreadType::AddCurrentThreadType(SYS_THREAD_PARTICLES);
	
	// 
	while (that->m_nextUpdateDelta != -999.0f) 
	{
		// wait until we get a signal to start the update
		sysIpcWaitSema(that->m_startUpdateSema);

		if (PARAM_ptfxtimebar.Get())
		{
			PF_FRAMEINIT_TIMEBARS(gDrawListMgr->GetUpdateThreadFrameNumber());
			PF_PUSH_TIMEBAR("ptfx update");
		}

		that->PreUpdateThread();
		ptfxManager::UpdateThread(that->m_nextUpdateDelta, &(that->m_currViewport));

		if (PARAM_ptfxtimebar.Get())
		{
			PF_POP_TIMEBAR();
		}

		// signal that the update is finished
		sysIpcSignalSema(that->m_finishUpdateSema);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  PreUpdateThread
///////////////////////////////////////////////////////////////////////////////
void			CPtFxManager::PreUpdateThread				()
{
	//Reset the game side ptfx states for the frame
#if !__SPU
	m_IsRefractPtfxPresent[RMPTFXTHREADER.GetUpdateBufferId()] = false;
#endif

}

///////////////////////////////////////////////////////////////////////////////
//  UpdateDepthBuffer
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::UpdateDepthBuffer				(grcRenderTarget* depthBuffer)
{
	RMPTFXMGR.SetDepthBuffer(depthBuffer);
}

#if RSG_PC
void			CPtFxManager::SetDepthWriteEnabled(bool depthWriteEnabled)
{
	RMPTFXMGR.SetDepthWriteEnabled(depthWriteEnabled);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  InitShadowedPtFxBuffer
///////////////////////////////////////////////////////////////////////////////

#if USE_SHADED_PTFX_MAP
void					CPtFxManager::InitShadowedPtFxBuffer  ()
{
	const int width		= CASCADE_SHADOWS_RES_VS_X;
	const int height	= CASCADE_SHADOWS_RES_VS_Y;

	// color
	grcTextureFactory::CreateParams params;
	params.Multisample = grcDevice::MSAA_None;
	params.HasParent = false;
	params.Parent = NULL; 
	params.UseFloat = false;
#if __PPU
	params.InTiledMemory = true;
	params.IsSwizzled = false;
#endif
	params.UseHierZ = false;
	params.IsSRGB = false;

	RTMemPoolID memPoolID = RTMEMPOOL_NONE;
	u8          rtPoolHeap = 0;

#if __PS3
	memPoolID = PSN_RTMEMPOOL_P2560_TILED_LOCAL_COMPMODE_DISABLED;
	rtPoolHeap = 13;
#elif __XENON
	memPoolID = XENON_RTMEMPOOL_GENERAL1;
	rtPoolHeap = kDepthQuarter;
#endif
	
	const int bpp = 16;
	params.Format = grctfG8R8;

#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
	{
		m_shadedParticleBuffer = CRenderTargets::CreateRenderTarget(memPoolID, "Shadowed PTX Buffer", grcrtPermanent, 1, 1, bpp, &params, rtPoolHeap);
	}
	else
#endif
	{
		m_shadedParticleBuffer = CRenderTargets::CreateRenderTarget(memPoolID, "Shadowed PTX Buffer", grcrtPermanent, width, height, bpp, &params, rtPoolHeap);
	}
	
	if(memPoolID != RTMEMPOOL_NONE)
	{
		m_shadedParticleBuffer->AllocateMemoryFromPool();
	}
}

///////////////////////////////////////////////////////////////////////////////
//  UpdateShadowedPtFxBuffer
///////////////////////////////////////////////////////////////////////////////
void					CPtFxManager::UpdateShadowedPtFxBuffer()
{
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
		return;
#endif

#if __BANK
	if (g_ptFxManager.m_gameDebug.GetDisableShadowedPtFx())
	{
		return;
	}
#endif

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));

	const float ptFxBlurKernelSize = BANK_SWITCH(g_ptFxManager.m_gameDebug.GetShadowedPtFxBlurKernelSize(), PTFX_SHADOWED_BLUR_KERNEL_SIZE_DEFAULT);

	BankFloat ptFxShadedRange	 =	(BANK_SWITCH(g_ptFxManager.m_gameDebug.GetShadowedPtFxRange(),		PTFX_SHADOWED_RANGE_DEFAULT)).Getf();
	
	// render the actual buffer
	const grcTexture* shadowMap = PS3_SWITCH(CRenderPhaseCascadeShadowsInterface::GetShadowMapPatched(), CRenderPhaseCascadeShadowsInterface::GetShadowMap());

	if (g_ptFxManager.m_shadedParticleBuffer && shadowMap && shadowMap->GetBitsPerPixel() == 32)
	{
		CRenderTargets::UnlockSceneRenderTargets();
		{
			CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();

			grcTextureFactory::GetInstance().LockRenderTarget(0, g_ptFxManager.m_shadedParticleBuffer, NULL);

			const CLightSource &dirLight = Lights::GetRenderDirectionalLight();
			const bool bNoDirLight = ((dirLight.GetIntensity() == 0.0f) || !dirLight.IsCastShadows());
			const tcKeyframeUncompressed& currKeyframe = g_timeCycle.GetCurrRenderKeyframe();
			const float cloudShadowAmount = 1.0f - currKeyframe.GetVar(TCVAR_CLOUD_SHADOW_OPACITY); // "fade to" value based on clouds
			static dev_float timeScale	= 2.0f;

			g_ptFxManager.SetPtfxShadedParams2(Vec4V((float)g_timeCycle.GetCurrCycleTime()*timeScale, cloudShadowAmount, 0.0f, 0.0f));
			g_ptFxManager.m_shadedParticleBufferShader->SetVar(g_ptFxManager.m_shadedParticleBufferShaderParamsId, g_ptFxManager.GetPtfxShadedParams());
			g_ptFxManager.m_shadedParticleBufferShader->SetVar(g_ptFxManager.m_shadedParticleBufferShaderParams2Id, g_ptFxManager.GetPtfxShadedParams2());
			g_ptFxManager.m_shadedParticleBufferShader->SetVar(g_ptFxManager.m_shadedParticleBufferShaderShadowMapId, shadowMap);

			g_ptFxManager.m_shadedParticleBufferShader->SetVar(g_ptFxManager.m_shadedParticleBufferShaderCloudSampler, CShaderLib::LookupTexture("cloudnoise"));
			g_ptFxManager.m_shadedParticleBufferShader->SetVar(g_ptFxManager.m_shadedParticleBufferShaderSmoothStepSampler, CRenderPhaseCascadeShadowsInterface::GetSmoothStepTexture());
			
			// if directional light is disabled, we actually are just going to clear this buffer to fully shadowed and then move on
			if (bNoDirLight)
			{
				GRCDEVICE.Clear(true, Color32(0.0f, 0.0f, 0.0f, 0.0f), false, 1.0f, 0);
				grcTextureFactory::GetInstance().UnlockRenderTarget(0);
			}
			else
			{
				grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Default);

				if (AssertVerify(g_ptFxManager.m_shadedParticleBufferShader->BeginDraw(grmShader::RMC_DRAW, false, g_ptFxManager.m_shadedParticleBufferShaderTechnique)))
				{
					g_ptFxManager.m_shadedParticleBufferShader->Bind(0);
					{
						const Color32 color = Color32(0);

						const float x0 = -1.0f;
						const float y0 = +1.0f;
						const float x1 = +1.0f;
						const float y1 = -1.0f;
						const float z  =  0.0f;

						grcBegin(drawTriStrip, 4);
						grcVertex(x0, y0, z, 0.0f, 0.0f, -1.0f, color, -ptFxShadedRange, -ptFxShadedRange);
						grcVertex(x0, y1, z, 0.0f, 0.0f, -1.0f, color, -ptFxShadedRange, ptFxShadedRange);
						grcVertex(x1, y0, z, 0.0f, 0.0f, -1.0f, color, ptFxShadedRange, -ptFxShadedRange);
						grcVertex(x1, y1, z, 0.0f, 0.0f, -1.0f, color, ptFxShadedRange, ptFxShadedRange);
						grcEnd();
					}
					g_ptFxManager.m_shadedParticleBufferShader->UnBind();
					g_ptFxManager.m_shadedParticleBufferShader->EndDraw();
				}

				grcResolveFlags resolveFlags;

				resolveFlags.BlurResult = true;
				resolveFlags.BlurKernelSize = ptFxBlurKernelSize;
				grcTextureFactory::GetInstance().UnlockRenderTarget(0,&resolveFlags);
			}
		}
		CRenderTargets::LockSceneRenderTargets();
	}
}
#endif //USE_SHADED_PTFX_MAP

#if PTFX_APPLY_DOF_TO_PARTICLES
void CPtFxManager::ClearPtfxDepthAlphaMap()
{
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()
	const Color32 color(0.0f, 0.0f, 0.0f, 0.0f);

#if RSG_ORBIS
	grcRenderTargetMSAA *const targetDepth = static_cast<grcRenderTargetMSAA*>(g_ptFxManager.GetPtfxDepthMap());
	grcRenderTargetMSAA *const targetAlpha = static_cast<grcRenderTargetMSAA*>(g_ptFxManager.GetPtfxAlphaMap());
	// no need to lock the targets, we only clear Cmask
	if (GRCDEVICE.ClearCmask(1, targetDepth->GetColorTarget(), color) &&
		GRCDEVICE.ClearCmask(2, targetAlpha->GetColorTarget(), color))
	{
		targetDepth->PushCmaskDirty();
		targetAlpha->PushCmaskDirty();
	}else
#endif //RSG_ORBIS
	{
		const grcRenderTarget* rendertargets[grcmrtColorCount] = {
			g_ptFxManager.GetPtfxDepthMap(),
			g_ptFxManager.GetPtfxAlphaMap(),
			NULL,
			NULL
		};
		grcTextureFactory::GetInstance().LockMRT(rendertargets, NULL);
		GRCDEVICE.Clear(true, color, false, 0, false, 0);
		grcTextureFactory::GetInstance().UnlockMRT();
	}
}

void CPtFxManager::LockPtfxDepthAlphaMap(bool useReadOnlyDepth)
{	
	const grcRenderTarget* rendertargets[grcmrtColorCount] = {
		CRenderTargets::GetBackBuffer(),
		g_ptFxManager.GetPtfxDepthMap(),
		g_ptFxManager.GetPtfxAlphaMap(),
		NULL
	};
	grcRenderTarget *depthRT = CRenderTargets::GetDepthBuffer();
#if __D3D11
	if(useReadOnlyDepth)
	{
		depthRT = (GRCDEVICE.IsReadOnlyDepthAllowed()) ? CRenderTargets::GetDepthBuffer_ReadOnly() : CRenderTargets::GetDepthBufferCopy();
	}
#endif

	grcTextureFactory::GetInstance().LockMRT(rendertargets, depthRT);
}

void CPtFxManager::UnLockPtfxDepthAlphaMap()
{
	grcTextureFactory::GetInstance().UnlockMRT();
}

void CPtFxManager::FinalizePtfxDepthAlphaMap()
{
	#if RSG_ORBIS
		// forced finalize Cmask clear
		GRCDEVICE.FinishRendering(
			static_cast<const grcRenderTargetGNM*>(g_ptFxManager.GetPtfxAlphaMap())->GetColorTarget(),
			true);
		// this will do nothing, since s_CmaskDirty has been set/reset several times by now since Clear()
		//static_cast<grcTextureFactoryGNM&>( grcTextureFactory::GetInstance() ).FinishRendering();
	#endif //RSG_ORBIS

	#if DEVICE_MSAA
	if ((GRCDEVICE.GetMSAA() && RSG_PC) || GRCDEVICE.GetMSAA()>1)
	{
		g_ptFxManager.ResolvePtfxMaps();
	}
	#endif //DEVICE_MSAA
}
#endif //PTFX_APPLY_DOF_TO_PARTICLES

///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::Render						(grcRenderTarget* pFrameBuffer, grcRenderTarget* pFrameSampler, bool bRenderIntoRefraction)
{	
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	bool bIsCameraUnderwater = Water::IsCameraUnderwater();

#if __BANK
	if (ptfxDebug::GetDisablePtFx() || m_gameDebug.GetDisablePtFxRender())
	{
		return;
	}

	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(true);
	}
#endif

	if(DRAWLISTMGR->IsExecutingMirrorReflectionDrawList())
	{
		g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION;
	}
	// Make sure we undo all viewport changes made below
	GRC_VIEWPORT_AUTO_PUSH_POP();

	SetGlobalConstants();

	RMPTFXMGR.SetFrameBuffer(pFrameBuffer );
	RMPTFXMGR.SetFrameBufferSampler(pFrameSampler );

	// set gIsInterior constant
	// CPortal::GetInteriorFxLevel() return value range is [0, 1], where:
	// 0 - fully in interior
	// 1 - fully outdoors
	// we don't want to consider in-between values (gIsInterior is a bool)
	const float cInteriorThreshold = 0.05f;
	bool bIsInterior = (CPortal::GetInteriorFxLevel() < cInteriorThreshold);
	CShaderLib::SetGlobalInInterior(bIsInterior);

	// Setup directional globals explicitly (don't involve clouds here, it's done in ptfxshaderparams2
	Lights::SetupDirectionalGlobals(1.0f);

	CRenderPhaseCascadeShadowsInterface::SetDeferredLightingShaderVars();
	CRenderPhaseCascadeShadowsInterface::SetSharedShaderVars();
	CRenderPhaseReflection::SetReflectionMap();

	// Setup fog values...	
	g_timeCycle.SetShaderData();

	CShaderLib::SetGlobalEmissiveScale(g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_PTFX_EMISSIVE_INTENSITY_MULT),true);
	
	Lights::BeginLightweightTechniques();
	grcLightState::SetEnabled(true);
	
#if RMPTFX_BANK
	// set shader overrides
	if (ptxDebug::sm_bEnableShaderDebugOverrideAlpha)
	{
		ptxShaderTemplateOverride::BindDebugTechniqueOverride(PTXSHADERDEBUG_ALPHA);
	}
	RMPTFXMGR.SetQualityState(PTX_QUALITY_HIRES);
#endif

	//If this is set then we're rendering the ptfx directly into the water refraction target. 
	//if not, then its the usual ptfx render
	if(bRenderIntoRefraction)
	{
		RenderPtfxIntoWaterRefraction();
	}
	else
	{
		//if camera is underwater
		RenderPtfxQualityLists(bIsCameraUnderwater);
		RenderRefractionBucket();
	}

#if RMPTFX_BANK
	// set shader overrides
	if (ptxDebug::sm_bEnableShaderDebugOverrideAlpha)
	{
		ptxShaderTemplateOverride::UnbindDebugTechniqueOverride();
	}
#endif

	Lights::EndLightweightTechniques();
	
	CShaderLib::SetGlobalEmissiveScale(1.0f);

	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_NORMAL;
#if __BANK
	if (m_gameDebug.GetRenderDebugCollisionPolys())
	{
		m_ptFxCollision.RenderDebug();
	}

	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(false);
	}
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  RenderRefractionBucket
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::RenderRefractionBucket()
{

	if(m_IsRefractPtfxPresent[RMPTFXTHREADER.GetDrawBufferId()] BANK_ONLY( && !m_gameDebug.GetDisableRefractionBucket()))
	{
		if(g_PtfxBuckerRenderStage != PTFX_BUCKET_RENDER_STAGE_SHADOWS)
		{
			g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_REFRACTION;
		}
		// We need to copy the backbuffer or refraction particles are drawn incorrectly over normal particles, if we're PC or using MSAA
		// we can use the backbuffer itself if we're on consoles and not using MSAA
		const bool bUseCopy = (GRCDEVICE.GetMSAA()>1) || RSG_PC; 
		RMPTFXMGR.SetFrameBufferSampler(bUseCopy?CRenderTargets::GetBackBufferCopy(true):CRenderTargets::GetBackBuffer());

		PF_PUSH_MARKER("Refraction Ptfx Draw List");
		const unsigned rti = g_RenderThreadIndex;
		m_isRenderingRefractionPtfxList[rti] = true;
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
		m_isRenderingRefractionPtfxList[rti] = false;
		PF_POP_MARKER();

		if(g_PtfxBuckerRenderStage != PTFX_BUCKET_RENDER_STAGE_SHADOWS)
		{
			g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_NORMAL;
		}
	}

}

///////////////////////////////////////////////////////////////////////////////
//  RenderPtfxIntoWaterRefraction
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::RenderPtfxIntoWaterRefraction	()
{

	// draw the underwater ptfx list 
	PF_PUSH_MARKER("Water Refraction Ptfx");
	bool bIsCameraUnderwater = Water::IsCameraUnderwater();
	if(PTFX_WATER_REFRACTION_ENABLED BANK_ONLY(&& !m_gameDebug.GetDisableWaterRefractionBucket()))
	{
		ptxShaderTemplate* pointShader = ptxShaderTemplateList::GetShader("ptfx_sprite");
		ptxShaderTemplate* trailShader = ptxShaderTemplateList::GetShader("ptfx_trail");

		const unsigned rti = g_RenderThreadIndex;

		g_PtfxWaterSurfaceRenderState = (u8)(bIsCameraUnderwater?PTFX_WATERSURFACE_RENDER_ABOVE_WATER:PTFX_WATERSURFACE_RENDER_BELOW_WATER);
		m_isRenderingIntoWaterRefraction[rti] = true;
		pointShader->SetShaderOverride(&m_ptxSpriteWaterRefractionShaderOverride);
		trailShader->SetShaderOverride(&m_ptxTrailWaterRefractionShaderOverride);
		
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));

		pointShader->SetShaderOverride(NULL);
		trailShader->SetShaderOverride(NULL);
		m_isRenderingIntoWaterRefraction[rti] = false;
		g_PtfxWaterSurfaceRenderState = PTFX_WATERSURFACE_RENDER_BOTH;
	}
	PF_POP_MARKER();

}

///////////////////////////////////////////////////////////////////////////////
//  RenderPtfxQualityLists
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::RenderPtfxQualityLists(bool bIsCameraUnderwater)
{
	u8 renderFlags = m_ptfxRenderFlags[gRenderThreadInterface.GetRenderBuffer()];

	//Render underwater ptfx along with normal ptfx if camera is underwater as it does
	//not need underwater fog processing

	g_PtfxWaterSurfaceRenderState = (u8)(bIsCameraUnderwater?PTFX_WATERSURFACE_RENDER_BELOW_WATER:PTFX_WATERSURFACE_RENDER_ABOVE_WATER);

	// draw the particle lists
	// high quality bucket
	PF_PUSH_MARKER("High Res Ptfx Draw List");
#if __BANK
	if (!m_gameDebug.GetDisableHighBucket() &&
		!m_gameDebug.GetRenderAllDownsampled()) // we only ever downsample hi-res if RAG toggle is forcing us to
#endif
	{
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));
	}
	PF_POP_MARKER();

	// draw the particle lists
	// medium quality bucket
	PF_PUSH_MARKER("Medium Res Ptfx Draw List");
#if __BANK
	if (!m_gameDebug.GetDisableMediumBucket())
#endif
	{
		if (!(renderFlags & PTFX_RF_MEDRES_DOWNSAMPLE))
		{
#if PTFX_DYNAMIC_QUALITY
			g_ptfxGPUTimerMedRes.Start();
#endif

			RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));

#if PTFX_DYNAMIC_QUALITY
			m_gpuTimeMedRes[gRenderThreadInterface.GetRenderBuffer()] = g_ptfxGPUTimerMedRes.Stop();
#endif
		}
	}
	PF_POP_MARKER();

	PF_PUSH_MARKER("Low Res Ptfx Draw List");
	// setup the downsampling
#if USE_DOWNSAMPLED_PARTICLES
	const bool shouldDownsample = ShouldDownsample();
	if (shouldDownsample)
	{
#if RMPTFX_BANK
		RMPTFXMGR.SetQualityState(PTX_QUALITY_LOWRES);
#endif
		InitDownsampleRender(CRenderTargets::GetBackBuffer());
	}
#endif

	// high quality bucket (debug downsampled)
#if __BANK
	if (!m_gameDebug.GetDisableHighBucket() && 
		m_gameDebug.GetRenderAllDownsampled()) // we only ever downsample hi-res if RAG toggle is forcing us to
#endif
	{
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));
	}

	// medium quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableMediumBucket())
#endif
	{
		if (renderFlags & PTFX_RF_MEDRES_DOWNSAMPLE)
		{
#if PTFX_DYNAMIC_QUALITY
			g_ptfxGPUTimerMedRes.Start();
#endif

			RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));

#if PTFX_DYNAMIC_QUALITY
			m_gpuTimeMedRes[gRenderThreadInterface.GetRenderBuffer()] = g_ptfxGPUTimerMedRes.Stop();
#endif
		}
	}

	// low quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableLowBucket())
#endif
	{
#if PTFX_DYNAMIC_QUALITY
		g_ptfxGPUTimerLowRes.Start();
#endif

		// this works assuming the downsampled target is 8bit
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));

#if PTFX_DYNAMIC_QUALITY
		m_gpuTimeLowRes[gRenderThreadInterface.GetRenderBuffer()] = g_ptfxGPUTimerLowRes.Stop();
#endif
	}

	// reset the downsampling
#if USE_DOWNSAMPLED_PARTICLES
	if (shouldDownsample)
	{
		ShutdownDownsampleRender();
	}
#endif

	g_PtfxWaterSurfaceRenderState = PTFX_WATERSURFACE_RENDER_BOTH;

	PF_POP_MARKER();

#if RMPTFX_BANK
	RMPTFXMGR.SetQualityState(PTX_QUALITY_HIRES);
#endif



}

///////////////////////////////////////////////////////////////////////////////
//  RenderSimple
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::RenderSimple			(grcRenderTarget* pBackBuffer, grcRenderTarget* pDepthBuffer)
{	

	PF_PUSH_MARKER("Ptfx Simple Render");
#if __BANK
	if (ptfxDebug::GetDisablePtFx() || m_gameDebug.GetDisablePtFxRender())
	{
		return;
	}
	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(true);
	}
#endif
	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_SIMPLE;
#if __PPU
	RMPTFXMGR.SetPatchedDepthBuffer(CRenderTargets::GetDepthBufferAsColor());
#endif 

	RMPTFXMGR.SetDepthBuffer(pDepthBuffer);

	RMPTFXMGR.SetFrameBuffer(pBackBuffer);
	RMPTFXMGR.SetFrameBufferSampler(pBackBuffer);

	// setup fog values
	grcLightState::SetEnabled(true);
	
	// draw the particle lists
	// high quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableHighBucket())
#endif
	{
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));
	}

	// medium quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableMediumBucket())
#endif
	{
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));
	}

	// low quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableLowBucket())
#endif
	{
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));
	}

#if __BANK
	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(false);
	}
#endif
	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_NORMAL;

	PF_POP_MARKER();
}


///////////////////////////////////////////////////////////////////////////////
//  RenderManual
///////////////////////////////////////////////////////////////////////////////

void		 	CPtFxManager::RenderManual					(ptxEffectInst* pFxInst BANK_ONLY(, bool debugRender))
{	
	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

	if(DRAWLISTMGR->IsExecutingMirrorReflectionDrawList())
	{
		if(CRenderPhaseMirrorReflectionInterface::GetIsMirrorUsingWaterReflectionTechnique_Render())
		{
			//Sometimes water reflections end up using mirror refletion renderphase
			//Disabling water refletions render in this case.
			return;
		}
		g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION;
	}

#if RSG_BANK && (RSG_ORBIS || RSG_DURANGO)
		(void)debugRender;
#endif

#if __BANK
	if (ptfxDebug::GetDisablePtFx() || m_gameDebug.GetDisablePtFxRender())
	{
		return;
	}

	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(true);
	}
#endif

#if RMPTFX_BANK
	// set shader overrides
	if (ptxDebug::sm_bEnableShaderDebugOverrideAlpha)
	{
		ptxShaderTemplateOverride::BindDebugTechniqueOverride(PTXSHADERDEBUG_ALPHA);
	}
#endif

#if RSG_PC
	// On PC we need the readonly depth bound here, as the driver imposes same read/write target validation.
	// For consoles we will be sampling from the same buffer that we write into. It's alright because particles don't write depth
	// and it's valid on consoles

	// We render these particles in two passes. In the normal pass we need to use MSAA depth buffer if applicable.
	// In the debug pass we need to set a non-msaa depth so base this on what's currently at the top of the stack.

	bool bUnlockDepthRO = true;
#if __BANK
	if (!(debugRender && GRCDEVICE.GetMSAA()))
#endif
	{
		grcViewport* pViewport = grcViewport::GetCurrent();
		Assert(pViewport);
		if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION && CRenderPhaseMirrorReflectionInterface::GetIsMirrorUsingWaterSurface_Render())
		{
			CRenderTargets::LockReadOnlyDepth(false, true, CMirrors::GetMirrorWaterDepthTarget(), CMirrors::GetMirrorWaterDepthTarget_Copy(), CMirrors::GetMirrorWaterDepthTarget_ReadOnly());
		}
		else
		if (g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION)
		{
			CRenderTargets::LockReadOnlyDepth(false, true, CMirrors::GetMirrorDepthTarget(), CMirrors::GetMirrorDepthTarget_Copy(), CMirrors::GetMirrorDepthTarget_ReadOnly());
		}
		else
		{
			if(RMPTFXMGR.GetDepthWriteEnabled())
				bUnlockDepthRO = false;
			else
				CRenderTargets::LockReadOnlyDepth(false, true, CRenderTargets::GetDepthBuffer(), CRenderTargets::GetDepthBufferCopy(), CRenderTargets::GetDepthBuffer_ReadOnly());
			
		}

		//resetting the window here as the LockReadOnlyDepth call resets the window to the default full screen. But mirrors can possible setup their own window size.		
		if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION && pViewport)
		{
			GRCDEVICE.SetWindow(pViewport->GetWindow());
		}
	}
#endif // RSG_PC

	// we flush-and-invalidate on orbis to avoid read-write hazard between alpha objects writing depth and particles doing depth blending
	ORBIS_ONLY( gfxc.triggerEvent(sce::Gnm::kEventTypeDbCacheFlushAndInvalidate) );

	bool bIsCameraUnderwater = Water::IsCameraUnderwater();
	g_PtfxWaterSurfaceRenderState = (u8)(bIsCameraUnderwater?PTFX_WATERSURFACE_RENDER_BELOW_WATER:PTFX_WATERSURFACE_RENDER_ABOVE_WATER);

	RMPTFXMGR.DrawManual(pFxInst PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));

	g_PtfxWaterSurfaceRenderState = PTFX_WATERSURFACE_RENDER_BOTH;

#if RSG_PC
#if __BANK
	if (!(debugRender && GRCDEVICE.GetMSAA()))
#endif // __BANK
	{
		if(bUnlockDepthRO)
			CRenderTargets::UnlockReadOnlyDepth();

		grcViewport* pViewport = grcViewport::GetCurrent();
		Assert(pViewport);

		//resetting the window here as the UnlockReadOnlyDepth call resets the window to the default full screen. But mirrors can possible setup their own window size.		
		if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION && pViewport)
		{
			GRCDEVICE.SetWindow(pViewport->GetWindow());
		}
	}
#endif //RSG_PC

#if RMPTFX_BANK
	// set shader overrides
	if (ptxDebug::sm_bEnableShaderDebugOverrideAlpha)
	{
		ptxShaderTemplateOverride::UnbindDebugTechniqueOverride();
	}
#endif

#if __BANK
	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(false);
	}
#endif

	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_NORMAL;
}


///////////////////////////////////////////////////////////////////////////////
//  SetGlobalConstants
///////////////////////////////////////////////////////////////////////////////
void CPtFxManager::SetGlobalConstants()
{
#if RMPTFX_USE_PARTICLE_SHADOWS	
	float globalshadowSlopeBias = m_GlobalShadowSlopeBias;
	float globalshadowBiasRange = m_GlobalShadowBiasRange;
	float globalshadowBiasRangeFalloff = m_GlobalshadowBiasRangeFalloff;

#if RMPTFX_BANK
	if(m_gameDebug.IsParticleShadowIntensityOverridden())
	{
		RMPTFXMGR.SetGlobalShadowIntensity(m_gameDebug.GetGlobalParticleShadowIntensity());
	}
	
	globalshadowSlopeBias = m_gameDebug.GetGlobalParticleShadowSlopeBias();
	globalshadowBiasRange = m_gameDebug.GetGlobalParticleShadowBiasRange();
	globalshadowBiasRangeFalloff = m_gameDebug.GetGlobalParticleShadowBiasRangeFalloff();
#endif

	float globalshadowBiasRangeDivision = 1.0f / (globalshadowBiasRange + globalshadowBiasRangeFalloff);
	Vec4V shadowBiasVec(globalshadowSlopeBias, globalshadowBiasRange, globalshadowBiasRangeFalloff, globalshadowBiasRangeDivision);
	grcEffect::SetGlobalVar( m_GlobalShadowBiasVar, shadowBiasVec);
#endif

#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		float ParticleDofAlphaScale = m_globalParticleDofAlphaScale;
	#if __BANK
		ParticleDofAlphaScale = m_gameDebug.GetGlobalParticleDofAlphaScale();
	#endif
		grcEffect::SetGlobalVar( m_GlobalParticleDofAlphaScaleVar, ParticleDofAlphaScale);
	}
#endif //PTFX_APPLY_DOF_TO_PARTICLES
}

#if RMPTFX_USE_PARTICLE_SHADOWS
///////////////////////////////////////////////////////////////////////////////
//  RenderShadows
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::RenderShadows(s32 UNUSED_PARAM(cascadeIndex))
{	
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
		return;
#endif

#if __BANK
	if (ptfxDebug::GetDisablePtFx() || m_gameDebug.GetDisablePtFxRender())
	{
		return;
	}

	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(true);
	}
#endif

	SetGlobalConstants();
	vfxAssertf(DRAWLISTMGR->IsExecutingCascadeShadowDrawList(), "Should be called from cascade shadows only");
	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_SHADOWS;

	ptxShaderTemplate* spriteShader = ptxShaderTemplateList::GetShader("ptfx_sprite");
	ptxShaderTemplate* trailShader = ptxShaderTemplateList::GetShader("ptfx_trail");

	spriteShader->SetRenderPass( ptxShaderTemplate::PTX_SHADOW_PASS ); 
	trailShader->SetRenderPass( ptxShaderTemplate::PTX_SHADOW_PASS ); 

	RMPTFXMGR.SetShadowPass(true);
	RMPTFXMGR.SetShadowSourcePos(g_sky.GetSunPosition());

	grcRenderTarget* prevDepthBuffer = RMPTFXMGR.GetDepthBuffer();
	RMPTFXMGR.SetDepthBuffer( CRenderPhaseCascadeShadowsInterface::GetShadowMap() );

	// set gIsInterior constant
	// CPortal::GetInteriorFxLevel() return value range is [0, 1], where:
	// 0 - fully in interior
	// 1 - fully outdoors
	// we don't want to consider in-between values (gIsInterior is a bool)
	const float cInteriorThreshold = 0.05f;
	bool bIsInterior = (CPortal::GetInteriorFxLevel() < cInteriorThreshold);
	CShaderLib::SetGlobalInInterior(bIsInterior);

	// Setup fog values...	
	g_timeCycle.SetShaderData();

	CShaderLib::SetGlobalEmissiveScale(g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_PTFX_EMISSIVE_INTENSITY_MULT),true);

	bool restoreLightState = grcLightState::IsEnabled();
	grcLightState::SetEnabled(false);

	u8 renderFlags = m_ptfxRenderFlags[gRenderThreadInterface.GetRenderBuffer()];

	// draw the particle lists
	// high quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableHighBucket())
#endif
	{
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH]  PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
	}

	// medium quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableMediumBucket())
#endif
	{
		if (renderFlags & PTFX_RF_MEDRES_DOWNSAMPLE)
		{
#if __PS3
			PostFX::SetLDR16bitHDR16bit();
#elif __XENON
			PostFX::SetLDR8bitHDR8bit();
#else
			PostFX::SetLDR16bitHDR16bit();
#endif
		}

		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
	}

	// low quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableLowBucket())
#endif
	{
#if __PS3
		PostFX::SetLDR16bitHDR16bit();
#elif __XENON
		PostFX::SetLDR8bitHDR8bit();
#else
		PostFX::SetLDR16bitHDR16bit();
#endif

		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW] PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL) RMPTFX_BANK_ONLY(, false));
	}

	PIXBegin(0, "ManualParticleShadows");

	// pfff.... sorted particle fx are specialz, so we need to store and restore those.
	grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
	grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
	grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;
	u8 ActiveStencilRef_Backup = grcStateBlock::ActiveStencilRef;
	u32 ActiveBlendFactors_Backup = grcStateBlock::ActiveBlendFactors;
	u64 ActiveSampleMask_Backup = grcStateBlock::ActiveSampleMask;

	u8 drawBufferId = RMPTFXTHREADER.GetDrawBufferId();
	for( u32 i = 0; i < NUM_PTFX_DRAWLISTS; i++)
	{
		ptxList* pList = &m_pDrawList[i]->m_effectInstList;
		ptxEffectInstItem* pCurrEffectInstItem = (ptxEffectInstItem*)pList->GetHeadMT(drawBufferId);
		
		while (pCurrEffectInstItem)
		{
			ptxEffectInst* pEffectInst = pCurrEffectInstItem->GetDataSB();

			if( pEffectInst->GetFlag(PTXEFFECTFLAG_MANUAL_DRAW))
			{
				if( pEffectInst->GetIsFinished() )
				{
					pCurrEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNextMT(drawBufferId);
					continue;
				}
				RMPTFXMGR.DrawManual(pEffectInst PTXDRAW_MULTIPLE_DRAWS_PER_BATCH_ONLY_PARAM(NULL));
			}
			pCurrEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNextMT(drawBufferId);
		}
	}

	grcStateBlock::SetDepthStencilState(DSS_Backup,ActiveStencilRef_Backup);
	grcStateBlock::SetRasterizerState(RS_Backup);
	grcStateBlock::SetBlendState(BS_Backup,ActiveBlendFactors_Backup,ActiveSampleMask_Backup);

	CShaderLib::SetGlobalEmissiveScale(1.0f);
	PIXEnd();

	grcLightState::SetEnabled(restoreLightState);

	RMPTFXMGR.SetDepthBuffer( prevDepthBuffer );
	RMPTFXMGR.SetShadowPass(false);

	spriteShader->SetRenderPass( ptxShaderTemplate::PTX_DEFAULT_PASS ); 
	trailShader->SetRenderPass( ptxShaderTemplate::PTX_DEFAULT_PASS ); 

#if __BANK
	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(false);
	}
#endif

	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_NORMAL;
}

#if PTXDRAW_MULTIPLE_DRAWS_PER_BATCH

void CPtFxManager::RenderShadowsAllCascades(s32 noOfCascades, grcViewport *pViewports, Vec4V *pWindows, bool bUseInstancedShadow)
{	
#if RSG_PC
	if (CSettingsManager::GetInstance().GetSettings().m_graphics.m_ShadowQuality == (CSettings::eSettingsLevel) (0))
		return;
#endif

	GRC_ALLOC_SCOPE_AUTO_PUSH_POP()

#if __BANK
	if (ptfxDebug::GetDisablePtFx() || m_gameDebug.GetDisablePtFxRender())
	{
		return;
	}

	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(true);
	}
#endif

	const unsigned rti = g_RenderThreadIndex;
	SetGlobalConstants();

	vfxAssertf(DRAWLISTMGR->IsExecutingCascadeShadowDrawList(), "Should be called from cascade shadows only");
	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_SHADOWS;
	Assertf(CRenderPhaseCascadeShadowsInterface::GetParticleShadowAccessFlagThreadID() == -1, "Some other thread has already accessed the Particle shadow render flag. Expect incorrect results. Access Flag Thread ID = %d, Current Flag Thread ID = %u", CRenderPhaseCascadeShadowsInterface::GetParticleShadowAccessFlagThreadID(), rti);
	ASSERT_ONLY(CRenderPhaseCascadeShadowsInterface::SetParticleShadowSetFlagThreadID((int)rti));
	SetParticleShadowsRenderedFlag(false);

	CPtfxShadowDrawToAllCascades toAllCascades(noOfCascades, pViewports, pWindows, bUseInstancedShadow);
	m_pMulitDrawInterface[rti] = &toAllCascades;

	ptxShaderTemplate* spriteShader = ptxShaderTemplateList::GetShader("ptfx_sprite");
	Assert(spriteShader != NULL);
	ptxShaderTemplate* trailShader = ptxShaderTemplateList::GetShader("ptfx_trail");
	Assert(trailShader != NULL);

#if GS_INSTANCED_SHADOWS
	bool bUseGeomShader = false;
#if PTFX_SHADOWS_INSTANCING_GEOMSHADER
	bUseGeomShader = bUseInstancedShadow;
#endif
	spriteShader->SetRenderPass( bUseGeomShader ? ptxShaderTemplate::PTX_INSTSHADOW_PASS : ptxShaderTemplate::PTX_SHADOW_PASS ); 
	trailShader->SetRenderPass( bUseGeomShader ? ptxShaderTemplate::PTX_INSTSHADOW_PASS : ptxShaderTemplate::PTX_SHADOW_PASS ); 

	// note that instanced drawmode tech set from SetInstancedDrawMode won't affect particle sprite/trail
	// since it has its own shader Begin mechanic.  This affects ptfx_model.
	if (bUseInstancedShadow)
	{
		grcViewport::SetInstVPWindow(CASCADE_SHADOWS_COUNT);
		grmShader::SetInstancedDrawMode(true);
		grcViewport::SetInstancing(true);
	}
#else
	spriteShader->SetRenderPass( ptxShaderTemplate::PTX_SHADOW_PASS ); 
	trailShader->SetRenderPass( ptxShaderTemplate::PTX_SHADOW_PASS ); 
#endif

	RMPTFXMGR.SetShadowPass(true);
	RMPTFXMGR.SetShadowSourcePos(g_sky.GetSunPosition());

	grcRenderTarget* prevDepthBuffer = RMPTFXMGR.GetDepthBuffer();
	RMPTFXMGR.SetDepthBuffer( CRenderPhaseCascadeShadowsInterface::GetShadowMap() );

	// set gIsInterior constant
	// CPortal::GetInteriorFxLevel() return value range is [0, 1], where:
	// 0 - fully in interior
	// 1 - fully outdoors
	// we don't want to consider in-between values (gIsInterior is a bool)
	const float cInteriorThreshold = 0.05f;
	bool bIsInterior = (CPortal::GetInteriorFxLevel() < cInteriorThreshold);
	CShaderLib::SetGlobalInInterior(bIsInterior);

	// Setup fog values...	
	g_timeCycle.SetShaderData();

	CShaderLib::SetGlobalEmissiveScale(g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_PTFX_EMISSIVE_INTENSITY_MULT),true);

	bool restoreLightState = grcLightState::IsEnabled();
	grcLightState::SetEnabled(false);

	u8 renderFlags = m_ptfxRenderFlags[gRenderThreadInterface.GetRenderBuffer()];

	// draw the particle lists
	// high quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableHighBucket())
#endif
	{
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH], &toAllCascades RMPTFX_BANK_ONLY(, false));
	}

	// medium quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableMediumBucket())
#endif
	{
		if (renderFlags & PTFX_RF_MEDRES_DOWNSAMPLE)
		{
#if __PS3
			PostFX::SetLDR16bitHDR16bit();
#elif __XENON
			PostFX::SetLDR8bitHDR8bit();
#else
			PostFX::SetLDR16bitHDR16bit();
#endif
		}

		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_MEDIUM], &toAllCascades RMPTFX_BANK_ONLY(, false));
	}

	// low quality bucket
#if __BANK
	if (!m_gameDebug.GetDisableLowBucket())
#endif
	{
		// this works assuming the downsampled target is 8bit
#if __PS3
		PostFX::SetLDR16bitHDR16bit();
#elif __XENON
		PostFX::SetLDR8bitHDR8bit();
#else
		PostFX::SetLDR16bitHDR16bit();
#endif
		RMPTFXMGR.Draw(m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW], &toAllCascades RMPTFX_BANK_ONLY(, false));
	}

	PIXBegin(0, "ManualParticleShadows");

	// pfff.... sorted particle fx are specialz, so we need to store and restore those.
	grcBlendStateHandle BS_Backup = grcStateBlock::BS_Active;
	grcDepthStencilStateHandle DSS_Backup = grcStateBlock::DSS_Active;
	grcRasterizerStateHandle RS_Backup = grcStateBlock::RS_Active;
	u8 ActiveStencilRef_Backup = grcStateBlock::ActiveStencilRef;
	u32 ActiveBlendFactors_Backup = grcStateBlock::ActiveBlendFactors;
	u64 ActiveSampleMask_Backup = grcStateBlock::ActiveSampleMask;

	u8 drawBufferId = RMPTFXTHREADER.GetDrawBufferId();
	for( u32 i = 0; i < NUM_PTFX_DRAWLISTS; i++)
	{
		ptxList* pList = &m_pDrawList[i]->m_effectInstList;
		ptxEffectInstItem* pCurrEffectInstItem = (ptxEffectInstItem*)pList->GetHeadMT(drawBufferId);

		while (pCurrEffectInstItem)
		{
			ptxEffectInst* pEffectInst = pCurrEffectInstItem->GetDataSB();

			if( pEffectInst->GetFlag(PTXEFFECTFLAG_MANUAL_DRAW))
			{
				if( pEffectInst->GetIsFinished() )
				{
					pCurrEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNextMT(drawBufferId);
					continue;
				}
				RMPTFXMGR.DrawManual(pEffectInst, &toAllCascades);
			}
			pCurrEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNextMT(drawBufferId);
		}
	}

	grcStateBlock::SetDepthStencilState(DSS_Backup,ActiveStencilRef_Backup);
	grcStateBlock::SetRasterizerState(RS_Backup);
	grcStateBlock::SetBlendState(BS_Backup,ActiveBlendFactors_Backup,ActiveSampleMask_Backup);

	CShaderLib::SetGlobalEmissiveScale(1.0f);
	PIXEnd();

	grcLightState::SetEnabled(restoreLightState);

	RMPTFXMGR.SetDepthBuffer( prevDepthBuffer );
	RMPTFXMGR.SetShadowPass(false);

#if GS_INSTANCED_SHADOWS
	if (bUseInstancedShadow)
	{
		grmShader::SetInstancedDrawMode(false);
		grcViewport::SetInstancing(false);
	}
#endif

	spriteShader->SetRenderPass( ptxShaderTemplate::PTX_DEFAULT_PASS ); 
	trailShader->SetRenderPass( ptxShaderTemplate::PTX_DEFAULT_PASS ); 

#if __BANK
	if(m_gameDebug.GetDrawWireframe())
	{
		grcStateBlock::SetWireframeOverride(false);
	}
#endif
	g_PtfxBuckerRenderStage = PTFX_BUCKET_RENDER_STAGE_NORMAL;

	m_pMulitDrawInterface[rti] = NULL;
}

#endif

 void			CPtFxManager::ResetParticleShadowsRenderedFlag()
{
	g_ptFxManager.SetParticleShadowsRenderedFlag(false);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CPtFxManager::InitWidgets						()
{
	ptfxManager::InitWidgets();
	m_gameDebug.InitWidgets();	
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ShouldDownsample
///////////////////////////////////////////////////////////////////////////////

#if USE_DOWNSAMPLED_PARTICLES
bool			CPtFxManager::ShouldDownsample()
{
#if __BANK
	if (m_gameDebug.GetDisableDownsampling())
	{
		return false;
	}
#endif

	if (m_disableDownsamplingThisFrame)
	{
		m_disableDownsamplingThisFrame = false;
		return false;
	}

	u8 renderFlags = m_ptfxRenderFlags[gRenderThreadInterface.GetRenderBuffer()];

	if (renderFlags & PTFX_RF_MEDRES_DOWNSAMPLE && 
		m_pDrawList[PTFX_DRAWLIST_QUALITY_HIGH]->m_isAnyActivePoints[RMPTFXTHREADER.GetDrawBufferId()])
	{
		return true;
	}

	if (renderFlags & PTFX_RF_LORES_DOWNSAMPLE && 
		m_pDrawList[PTFX_DRAWLIST_QUALITY_LOW]->m_isAnyActivePoints[RMPTFXTHREADER.GetDrawBufferId()])
	{
		return true;
	}

	return false;
}
#endif 

///////////////////////////////////////////////////////////////////////////////
//  InitDownsampleRender
///////////////////////////////////////////////////////////////////////////////

#if USE_DOWNSAMPLED_PARTICLES
void			CPtFxManager::InitDownsampleRender		(grcRenderTarget* pColourSampler)
{
	CRenderTargets::UnlockSceneRenderTargets();

#if __PS3
		PostFX::SetLDR16bitHDR16bit();
#elif __XENON
		PostFX::SetLDR8bitHDR8bit();
#else
	PostFX::SetLDR16bitHDR16bit();
#endif

	grcRenderTarget* smallColorRenderTarget = m_pLowResPtfxBuffer;
#if __XENON
	m_useDownrezzedPtfx = true;
#endif
	grcRenderTarget* smallDepthRenderTarget = CRenderTargets::GetDepthBufferQuarter();
	m_pPrevFrameBuffer = RMPTFXMGR.GetFrameBuffer();
	m_pPrevFrameBufferSampler = RMPTFXMGR.GetFrameBufferSampler();
	m_pPrevDepthBuffer = RMPTFXMGR.GetDepthBuffer();
#if __PPU	
	m_pPrevPatchedDepthBuffer = RMPTFXMGR.GetPatchedDepthBuffer();
#endif // __PPU	
	RMPTFXMGR.SetFrameBuffer(smallColorRenderTarget);
	RMPTFXMGR.SetFrameBufferSampler(pColourSampler);
	RMPTFXMGR.SetDepthBuffer(smallDepthRenderTarget);
#if __PPU	
	RMPTFXMGR.SetPatchedDepthBuffer(CRenderTargets::GetDepthBufferQuarterAsColor());
#endif // __PPU


	ptxShaderTemplateList::SetGlobalVars();

	CShaderLib::SetGlobalScreenSize(Vector2((float)smallColorRenderTarget->GetWidth(), (float)smallColorRenderTarget->GetHeight()));

	CRenderTargets::DownsampleDepth();

	grcTextureFactory::GetInstance().LockRenderTarget(0, smallColorRenderTarget, smallDepthRenderTarget);
	GRCDEVICE.Clear(true, Color32(0.0f, 0.0f, 0.0f, 0.0f), false, 1.0f, 0);

	GRCDEVICE.BeginConditionalQuery(m_PtfxDownsampleQuery);
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  ShutdownDownsampleRender
///////////////////////////////////////////////////////////////////////////////

#if USE_DOWNSAMPLED_PARTICLES
void			CPtFxManager::ShutdownDownsampleRender	()
{
#if __XENON
	m_useDownrezzedPtfx = false;
#endif
	GRCDEVICE.EndConditionalQuery(m_PtfxDownsampleQuery);
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

	GRCDEVICE.BeginConditionalRender(m_PtfxDownsampleQuery);

#if __XENON
	PostFX::SetLDR8bitHDR10bit();
#elif __PS3
	PostFX::SetLDR8bitHDR16bit();
#else
	PostFX::SetLDR16bitHDR16bit();
#endif

	UpsampleHelper::UpsampleParams upsampleParams;
	upsampleParams.blendType = grcCompositeRenderTargetHelper::COMPOSITE_RT_BLEND_COMPOSITE_ALPHA;
	upsampleParams.highResUpsampleDestination = m_pPrevFrameBuffer;
	upsampleParams.highResDepthTarget = m_pPrevDepthBuffer;
	upsampleParams.lowResUpsampleSource = m_pLowResPtfxBuffer;
	upsampleParams.lowResDepthTarget = CRenderTargets::GetDepthBufferQuarter();
	upsampleParams.useSeparateEdgeDetectionPass = false;
#if __PS3
	upsampleParams.highResPatchedDepthTarget = m_pPrevPatchedDepthBuffer;
	upsampleParams.lowResPatchedDepthTarget = CRenderTargets::GetDepthBufferQuarterAsColor();
#else
	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	upsampleParams.resolveFlags = &resolveFlags;
	upsampleParams.maskUpsampleByStencilAsColor = false;
#endif
	upsampleParams.lockTarget	= false;
	upsampleParams.unlockTarget	= false;
	
	CRenderTargets::LockSceneRenderTargets();
	UpsampleHelper::PerformUpsample(upsampleParams);

	CShaderLib::UpdateGlobalGameScreenSize();

	RMPTFXMGR.SetFrameBuffer(m_pPrevFrameBuffer);
	RMPTFXMGR.SetFrameBufferSampler(m_pPrevFrameBufferSampler);
	RMPTFXMGR.SetDepthBuffer(m_pPrevDepthBuffer);
#if __PPU
	RMPTFXMGR.SetPatchedDepthBuffer(m_pPrevPatchedDepthBuffer);
#endif // __PPU

	GRCDEVICE.EndConditionalRender(m_PtfxDownsampleQuery);

#if __XENON
	PostFX::SetLDR10bitHDR10bit();
#elif __PS3
	PostFX::SetLDR8bitHDR16bit();
#else
	PostFX::SetLDR16bitHDR16bit();
#endif
}
#endif // USE_DOWNSAMPLED_PARTICLES



///////////////////////////////////////////////////////////////////////////////
//  RelocateEntityPtFx
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::RelocateEntityPtFx			(CEntity* pEntity)
{
	u32 drawListCount = RMPTFXMGR.GetNumDrawLists();
	for(u32 i=0;i<drawListCount;i++)
	{
		ptxEffectInstItem* pCurrEffectInstItem = (ptxEffectInstItem*)RMPTFXMGR.GetDrawList(i)->m_effectInstList.GetHead();
		ptxEffectInstItem* pNextEffectInstItem = pCurrEffectInstItem;
		while (pCurrEffectInstItem)
		{
			pNextEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNext();
			ptxEffectInst* pFxInst = pCurrEffectInstItem->GetDataSB();

			if (ptfxAttach::GetIsAttachedTo(pFxInst, pEntity))
			{
				//pFxInst->Relocate();
				pFxInst->SetNeedsRelocated();
			}

			pCurrEffectInstItem = pNextEffectInstItem;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemovePtFxFromEntity
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::RemovePtFxFromEntity		(CEntity* pEntity, bool isRespawning)
{
	u32 drawListCount = RMPTFXMGR.GetNumDrawLists();
	for (u32 i=0;i<drawListCount;i++)
	{
		ptxEffectInstItem* pCurrEffectInstItem = (ptxEffectInstItem*)RMPTFXMGR.GetDrawList(i)->m_effectInstList.GetHead();
		ptxEffectInstItem* pNextEffectInstItem = pCurrEffectInstItem;
		while (pCurrEffectInstItem)
		{
			pNextEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNext();
			ptxEffectInst* pFxInst = pCurrEffectInstItem->GetDataSB();

			// some effects don't want to be removed when respawning (e.g. angel/devil glows)
			bool dontRemovePtFx = false;
			if (isRespawning && pFxInst->GetEffectRule())
			{
				atHashWithStringNotFinal hashName(pFxInst->GetEffectRule()->GetName());
				if (hashName==ATSTRINGHASH("scr_adversary_ped_glow", 0xF277B208) ||
					hashName==ATSTRINGHASH("scr_adversary_ped_light_bad", 0x511BCEF0) ||
					hashName==ATSTRINGHASH("scr_adversary_ped_light_good", 0xB3AF8FC))
				{
					dontRemovePtFx = true;
				}
			}

			if (!dontRemovePtFx)
			{
				if (ptfxAttach::GetIsAttachedTo(pFxInst, pEntity))
				{
					ptfxManager::RemovePtFx(pFxInst);
				}
			}

			pCurrEffectInstItem = pNextEffectInstItem;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  StopPtFxOnEntity
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::StopPtFxOnEntity			(CEntity* pEntity)
{
	u32 drawListCount = RMPTFXMGR.GetNumDrawLists();
	for (u32 i=0;i<drawListCount;i++)
	{
		ptxEffectInstItem* pCurrEffectInstItem = (ptxEffectInstItem*)RMPTFXMGR.GetDrawList(i)->m_effectInstList.GetHead();
		ptxEffectInstItem* pNextEffectInstItem = pCurrEffectInstItem;
		while (pCurrEffectInstItem)
		{
			pNextEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNext();
			ptxEffectInst* pFxInst = pCurrEffectInstItem->GetDataSB();

			if (ptfxAttach::GetIsAttachedTo(pFxInst, pEntity))
			{
				ptfxWrapper::StopEffectInst(pFxInst);
			}

			pCurrEffectInstItem = pNextEffectInstItem;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemovePtFxInRange
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::RemovePtFxInRange			(Vec3V_In vPos, float range)
{
	u32 drawListCount = RMPTFXMGR.GetNumDrawLists();
	for (u32 i=0;i<drawListCount;i++)
	{
		ptxEffectInstItem* pCurrEffectInstItem = (ptxEffectInstItem*)RMPTFXMGR.GetDrawList(i)->m_effectInstList.GetHead();
		ptxEffectInstItem* pNextEffectInstItem = pCurrEffectInstItem;
		while (pCurrEffectInstItem)
		{
			pNextEffectInstItem = (ptxEffectInstItem*)pCurrEffectInstItem->GetNext();
			ptxEffectInst* pFxInst = pCurrEffectInstItem->GetDataSB();

			Vec3V vFxPos = pFxInst->GetCurrPos();
			float distSqr = MagSquared(vFxPos-vPos).Getf();
			if (distSqr<range*range)
			{
				if (!pFxInst->GetFlag(PTFX_EFFECTINST_DONT_REMOVE))
				{
					ptfxManager::RemovePtFx(pFxInst);
				}
			}

			pCurrEffectInstItem = pNextEffectInstItem;
		}
	}
}



///////////////////////////////////////////////////////////////////////////////
//	SetForceVehicleInteriorFlag
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::SetForceVehicleInteriorFlag(bool bSet, scrThreadId scriptThreadId)
{
	if (vfxVerifyf(m_forceVehicleInteriorFlagScriptThreadId==THREAD_INVALID || m_forceVehicleInteriorFlagScriptThreadId==scriptThreadId, "trying to override force vehicle interior flag when this is already in use by another script")) 
	{
		m_forceVehicleInteriorFlag = bSet; 
		m_forceVehicleInteriorFlagScriptThreadId = scriptThreadId;
	}
}



///////////////////////////////////////////////////////////////////////////////
//	EffectInstStartFunctor
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::EffectInstStartFunctor			(ptxEffectInst* pFxInst)
{
	ptfxAssertf(pFxInst, "called with NULL effect instance");

	// scene sorting
#if PTFX_ALLOW_INSTANCE_SORTING
	if (ShouldSceneSort(pFxInst))
	{
		if (pFxInst->GetFlag(PTFX_RESERVED_SORTED))
		{
			// this effect has already been started and is now being restarted - ignore it
			ptfxAssertf(pFxInst->GetFlag(PTXEFFECTFLAG_MANUAL_DRAW), "CPtFxSortedManager::SortedOnStartFunctor - already started effect instance doesn't have its manual draw flag set");
			ptfxAssertf(pFxInst->GetFlag(PTXEFFECTFLAG_KEEP_RESERVED), "CPtFxSortedManager::SortedOnStartFunctor - already started effect instance doesn't have its reserved flag set");
			return;
		}

		CPtFxSortedManager::Add(pFxInst);
	}
#endif

	// audio
	g_AmbientAudioEntity.StartSpawnEffect(pFxInst);
}


///////////////////////////////////////////////////////////////////////////////
//	EffectInstStopFunctor
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::EffectInstStopFunctor				(ptxEffectInst* pFxInst)
{
	ptfxAssertf(pFxInst, "called with NULL effect instance");

	// audio
	g_AmbientAudioEntity.StopSpawnEffect(pFxInst);
}


///////////////////////////////////////////////////////////////////////////////
//	EffectInstDeactivateFunctor
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::EffectInstDeactivateFunctor		(ptxEffectInst* pFxInst)
{
	ptfxAssertf(pFxInst, "called with NULL effect instance");

	// audio
	g_AmbientAudioEntity.RecycleSpawnEffect(pFxInst);

	for (int i=0; i < g_ptFxManager.m_numLightInfos; ++i)
	{
		if(g_ptFxManager.m_lightInfos[i].pEffectInst == pFxInst)
		{
			g_ptFxManager.m_lightInfos[i].pEffectInst = NULL;
		}
	}

	for (int i=0; i < g_ptFxManager.m_numDecalInfos; ++i)
	{
		if(g_ptFxManager.m_decalInfos[i].pEffectInst == pFxInst)
		{
			g_ptFxManager.m_decalInfos[i].pEffectInst = NULL;
		}
	}

	for (int i=0; i < g_ptFxManager.m_numDecalTrailInfos; ++i)
	{
		if(g_ptFxManager.m_decalTrailInfos[i].pEffectInst == pFxInst)
		{
			g_ptFxManager.m_decalTrailInfos[i].pEffectInst = NULL;
		}
	}

	for (int i=0; i < g_ptFxManager.m_numLiquidInfos; ++i)
	{
		if(g_ptFxManager.m_liquidInfos[i].pEffectInst == pFxInst)
		{
			g_ptFxManager.m_liquidInfos[i].pEffectInst = NULL;
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//	EffectInstSpawnFunctor
///////////////////////////////////////////////////////////////////////////////

// bool			CPtFxManager::EffectInstSpawnFunctor			(ptxEffectInst* pFxInstParent, ptxEffectInst* pFxInstChild)
// {
// 	ptfxAssertf(pFxInstParent, "called with NULL parent effect instance");
// 	ptfxAssertf(pFxInstChild, "called with NULL child effect instance");
// 
// 	// scene sorting
// #if PTFX_ALLOW_INSTANCE_SORTING
// 	if (ShouldSceneSort(pFxInstParent))
// 	{
// 		CPtFxSortedManager::Add(pFxInstChild);
// 	}
// #endif
// 
// 	return true;
// }


///////////////////////////////////////////////////////////////////////////////
//	EffectInstDistToCamFunctor
///////////////////////////////////////////////////////////////////////////////

float			CPtFxManager::EffectInstDistToCamFunctor		(ptxEffectInst* pFxInst)
{	
	Vec3V_ConstRef vFxPos = pFxInst->GetCurrPos();
	if (ptfxVerifyf(IsFiniteAll(vFxPos), "effect inst position is not valid (%s)", pFxInst->GetEffectRule()->GetName()))
	{
		return rage::Sqrtf(CVfxHelper::GetDistSqrToCamera(vFxPos));
	}
	else
	{
		return 0.0f;
	}
}


///////////////////////////////////////////////////////////////////////////////
//	EffectInstOcclusionFunctor
///////////////////////////////////////////////////////////////////////////////

bool			CPtFxManager::EffectInstOcclusionFunctor		(Vec4V* pvCullSphere)
{	
	if (CVfxHelper::GetCamInteriorLocation().IsValid())
	{
		// we're in an interior (they don't support occlusion yet) - we're not culled
		return false;
	}

	spdAABB cullBox;
	cullBox.SetAsSphereV4(*pvCullSphere);

	// test the cull box against the occlusion system
	if(COcclusion::IsBoxVisibleFast(cullBox, g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SHADOWS))
	{
		// it's visible - we're not culled
		return false;
	}

	// we're culled
	return true;
}


///////////////////////////////////////////////////////////////////////////////
//	EffectInstPostUpdateFunctor
///////////////////////////////////////////////////////////////////////////////

void			CPtFxManager::EffectInstPostUpdateFunctor		(ptxEffectInst* pFxInst)
{
	ptfxAssertf(pFxInst, "called with NULL effect instance");

	// collision
	ptxEffectRule* pEffectRule = pFxInst->GetEffectRule();
	if (pEffectRule)
	{
		// check if collision is enabled on this effect
		if (pEffectRule->GetColnType()>0 && pEffectRule->GetColnRange()>0.0f)
		{
			// check if the effect out of range for performing collision (using the lod evo min dist as the collision range)
			bool outOfRange = false;
			if (pFxInst->GetLodEvoIdx()>=0 && pEffectRule->GetLodEvoDistMax()>0.0f)
			{
				float evoDistMin = pEffectRule->GetLodEvoDistMin();
				float distSqrToCamera = pFxInst->GetDataDB()->GetDistSqrToCamera();
				if (distSqrToCamera>evoDistMin*evoDistMin)
				{
					outOfRange = true;
				}
			}

			if (outOfRange)
			{
				// effect is out of range - clear any existing collision
				pFxInst->SetCollisionSet(NULL);
			}
			else
			{
				// effect is in range - process collisions
				g_ptFxManager.GetColnInterface().Register(pFxInst);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//	ParticleRuleInitFunctor
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::ParticleRuleInitFunctor(ptxParticleRule* pPtxParticleRule)
{
	u8 ptxRuleFlags = PTFX_WATERSURFACE_RENDER_BOTH;
	if(pPtxParticleRule != NULL)
	{
		ptxu_ZCull* pPtxZCullBehavior = static_cast<ptxu_ZCull*>(pPtxParticleRule->GetBehaviour("ptxu_zcull"));
		if(pPtxZCullBehavior != NULL)
		{
			int cullMode = pPtxZCullBehavior->GetMode();
			int referenceSpace = pPtxZCullBehavior->GetReferenceSpace();

			if(referenceSpace == PTXU_ZCULL_REFERENCESPACE_WATER_Z_CAMERA || referenceSpace == PTXU_ZCULL_REFERENCESPACE_WATER_Z_POINT)
			{
				if(cullMode == PTXU_ZCULL_MODE_CULL_UNDER)
				{
					ptxRuleFlags = PTFX_WATERSURFACE_RENDER_ABOVE_WATER;
				}
				else if(cullMode == PTXU_ZCULL_MODE_CULL_OVER)
				{
					ptxRuleFlags = PTFX_WATERSURFACE_RENDER_BELOW_WATER;
				}
			}
		}
		pPtxParticleRule->SetFlags(ptxRuleFlags);
	}
}

///////////////////////////////////////////////////////////////////////////////
//	ParticleRuleShouldBeRenderedFunctor
///////////////////////////////////////////////////////////////////////////////

bool CPtFxManager::ParticleRuleShouldBeRenderedFunctor(ptxParticleRule* pPtxParticleRule)
{
#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetAlwaysPassShouldBeRenderedRuleChecks())
	{
		return true;
	}
#endif

	if(pPtxParticleRule == NULL)
	{
		return false;
	}

	//Disabling screen space particle fx in mirror reflections and shadows
	if(pPtxParticleRule->GetIsScreenSpace() && (g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION || g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SHADOWS))
	{
		return false;
	}
	const bool bIsRefractionShader = pPtxParticleRule->GetShaderInst().GetIsRefract();
	u8 ptxFlags = pPtxParticleRule->GetAllFlags();
	//check if we need to skip drawing this particle based on water surface state
	bool validWaterSurfaceRenderMode = ((g_PtfxWaterSurfaceRenderState & ptxFlags) != 0);
	// Skip non-refract shaders during refract pass, skip refract shaders during non-refract pass AND if it has valid water surface renderstate
	return (g_ptFxManager.m_isRenderingRefractionPtfxList[g_RenderThreadIndex] == bIsRefractionShader && validWaterSurfaceRenderMode);

}

///////////////////////////////////////////////////////////////////////////////
//	EffectInstShouldBeRenderedFunctor
///////////////////////////////////////////////////////////////////////////////

bool CPtFxManager::EffectInstShouldBeRenderedFunctor(ptxEffectInst* pFxInst, bool bSkipManualDraw)
{
#if __BANK
	if (g_ptFxManager.GetGameDebugInterface().GetAlwaysPassShouldBeRenderedRuleChecks())
	{
		return true;
	}
#endif
	vfxAssertf(CSystem::IsThisThreadId(SYS_THREAD_RENDER), "Should be called from Render Thread");

	if(pFxInst == NULL)
	{
		return false;
	}

	ptxEffectInstDB* pDataDB = pFxInst->GetDataDBMT(RMPTFXTHREADER.GetDrawBufferId());
	const unsigned rti = g_RenderThreadIndex;

	ptxAssertf(!(pFxInst->GetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW) && pFxInst->GetFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW)), "Effect inst has both first person and non first person flags set");

	//Let's first check if the particle attached to something in the vehicle interior
	//only to be used when executing the draw scene draw list
	if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_NORMAL || g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SIMPLE)
	{
#if FPS_MODE_SUPPORTED
		//Skip NON first person tagged fx in normal mode if we're in first person view
		if(g_ptFxManager.m_isFirstPersonCamActive[gRenderThreadInterface.GetRenderBuffer()] && pFxInst->GetFlag(PTFX_EFFECTINST_RENDER_ONLY_NON_FIRST_PERSON_VIEW))
		{
			return false;
		}
#endif
		fwInteriorLocation interiorLocation;
		interiorLocation.SetAsUint32(pFxInst->GetInteriorLocation());
		bool bIsVehicleAlphaInterior = pDataDB->GetCustomFlag(PTFX_IS_VEHICLE_INTERIOR);
		bool bAlphaInteriorCheck = CRenderPhaseDrawSceneInterface::RenderAlphaInteriorCheck(interiorLocation, bIsVehicleAlphaInterior);
		//This part is for Late Interior Alpha Support. If we're in an interior, it will render the outside particles
		//first and then render the interior particles
		if(!bAlphaInteriorCheck)
		{
			return false;
		}

	}
	else if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION || g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SHADOWS)
	{
		//Skip first person tagged fx in mirror reflection and shadows when first person view is active
#if FPS_MODE_SUPPORTED
		if(g_ptFxManager.m_isFirstPersonCamActive[gRenderThreadInterface.GetRenderBuffer()] && pFxInst->GetFlag(PTFX_EFFECTINST_RENDER_ONLY_FIRST_PERSON_VIEW))
		{
			return false;
		}
#endif
	}
	//Skip the effect instance if we're rendering refraction and the effect inst does not contains refraction particles
	if(g_ptFxManager.m_isRenderingRefractionPtfxList[rti] && !pFxInst->GetFlag(PTFX_EFFECTINST_CONTAINS_REFRACT_RULE))
	{
		return false;
	}

	u8 effectInstGameCustomFlags = pDataDB->GetCustomFlags();

	//Render Manual also calls this function so make sure to use the appropriate flag to have the extra check against skipping manual draw 
	if((!bSkipManualDraw || pFxInst->GetFlag(PTXEFFECTFLAG_MANUAL_DRAW)==false) ||
		(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SIMPLE) ||
		(g_ptFxManager.m_isRenderingIntoWaterRefraction[rti]) || 
		(g_ptFxManager.m_isRenderingRefractionPtfxList[rti] && pFxInst->GetFlag(PTFX_EFFECTINST_CONTAINS_REFRACT_RULE) ))
	{
		if((g_PtfxWaterSurfaceRenderState & effectInstGameCustomFlags) != 0)
		{
			return true;
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////
//	EffectInstInitFunctor
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::EffectInstInitFunctor(ptxEffectInst* pFxInst)
{
	if(pFxInst == NULL)
	{
		return;
	}
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_REFRACT_RULE);
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_WATER_ABOVE_RULE);
	pFxInst->ClearFlag(PTFX_EFFECTINST_CONTAINS_WATER_BELOW_RULE);
	
	// override data if there is an effect rule
	if (pFxInst->GetEffectRule())
	{
		for (int i=0; i<pFxInst->GetNumEvents(); i++)
		{
			if (pFxInst->GetEffectRule()->GetTimeline().GetEvent(i)->GetType()==PTXEVENT_TYPE_EMITTER)
			{
#if !__SPU
				ptxParticleRule* pParticleRule = ((ptxEventEmitter*)pFxInst->GetEffectRule()->GetTimeline().GetEvent(i))->GetParticleRule();
				if(pParticleRule)
				{
					if(pParticleRule->GetShaderInst().GetIsRefract())
					{
						pFxInst->SetFlag(PTFX_EFFECTINST_CONTAINS_REFRACT_RULE);
					}
					if(pParticleRule->GetFlags(PTFX_WATERSURFACE_RENDER_ABOVE_WATER))
					{
						pFxInst->SetFlag(PTFX_EFFECTINST_CONTAINS_WATER_ABOVE_RULE);
					}
					if(pParticleRule->GetFlags(PTFX_WATERSURFACE_RENDER_BELOW_WATER))
					{
						pFxInst->SetFlag(PTFX_EFFECTINST_CONTAINS_WATER_BELOW_RULE);
					}
				}
#endif
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//	EffectInstUpdateSetupFunctor
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::EffectInstUpdateSetupFunctor (ptxEffectInst* pFxInst)
{
	if(pFxInst == NULL)
	{
		return;
	}


	pFxInst->GetDataDB()->ClearCustomFlag(PTFX_WATERSURFACE_RENDER_BOTH);
	u32 effectInstFlags = pFxInst->GetFlags();
	//if effect inst contains particles that renders in both, then check the distance
	//from water
	if((effectInstFlags & PTFX_EFFECTINST_CONTAINS_WATER_BOTH_RULE) == PTFX_EFFECTINST_CONTAINS_WATER_BOTH_RULE)
	{
		//Setting the both flags here and keeping the underwater checks commented out because 
		//the underwater test looks up the game waterheight map which contains only water bodies that
		//reflect. But for shallow water, it's not exactly reflective (it does cube map but not water reflection)
		//so the check would fail in cases where its closer to a higher water body. Disabling this optimization increases
		//the cost but it's very negligible. Once the IsUnderwater test checkes with all the water bodies then we'll re-enable it.
		pFxInst->GetDataDB()->SetCustomFlag(PTFX_WATERSURFACE_RENDER_BOTH);
		/*
		const ScalarV thresholdBelowWater = ScalarV(V_NEGFIVE);
		const ScalarV thresholdAboveWater = ScalarV(V_FIVE);
		if(CVfxHelper::IsUnderwater(pFxInst->GetCurrPos(), false, thresholdAboveWater))
		{
			pFxInst->GetDataDB()->SetCustomFlag(PTFX_WATERSURFACE_RENDER_BELOW_WATER);
		}

		if(!CVfxHelper::IsUnderwater(pFxInst->GetCurrPos(), false, thresholdBelowWater))
		{
			pFxInst->GetDataDB()->SetCustomFlag(PTFX_WATERSURFACE_RENDER_ABOVE_WATER);
		}
		*/
		
	}
	else
	{
		//Effects can have both above water and below water fx.
		if(effectInstFlags & PTFX_EFFECTINST_CONTAINS_WATER_ABOVE_RULE)
		{
			pFxInst->GetDataDB()->SetCustomFlag(PTFX_WATERSURFACE_RENDER_ABOVE_WATER);
		}

		if(effectInstFlags & PTFX_EFFECTINST_CONTAINS_WATER_BELOW_RULE)
		{
			pFxInst->GetDataDB()->SetCustomFlag(PTFX_WATERSURFACE_RENDER_BELOW_WATER);
		}

	}

}

///////////////////////////////////////////////////////////////////////////////
//	EffectInstUpdateFinalizeFunctor
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::EffectInstUpdateCullingFunctor (ptxEffectInst* pFxInst)
{
	// initialise the cull flags
	bool isEmitCulled = false;
	bool isUpdateCulled = false;
	bool isDrawCulled = false;
	bool isShadowDrawCulled = false;

	// calc the cull state
	ptxEffectCullState cullState = pFxInst->CalcCullState(&(g_ptFxManager.m_currViewport), true);
	//setting the cull earlier in case anything other calls back to pFxInst needs it and its not updated later in this function
	pFxInst->SetCullState(cullState);
	ptxEffectCullState shadowCullState = cullState;
	ptxEffectRule* pEffectRule = pFxInst->GetEffectRule();

	//If effect contains shadows lets do the test with the shadow viewports also
	//TODO: THis is disabled right now. We should implement the projected shadow OBB visibility test and make shadow cull state visible
	//only when the shadow would be visible in the main viewport.
#if PTFX_ENABLE_SHADOW_CULL_STATE_UPDATE
#if RMPTFX_BANK
	if (ptxDebug::sm_disableShadowCullStateCalcs==false)
#endif // RMPTFX_BANK
	{
		if((RMPTFX_EDITOR_ONLY(RMPTFXMGR.GetInterface().IsConnected() || ) (pEffectRule && !pEffectRule->GetHasNoShadows())))
		{
			for(int i=0; i<g_ptFxManager.m_currShadowViewports.GetCount(); i++)
			{
				shadowCullState = Min( pFxInst->CalcCullState(&(g_ptFxManager.m_currShadowViewports[i]), false),shadowCullState);
			}
		}
	}
#endif //PTFX_ENABLE_SHADOW_CULL_STATE_UPDATE

	// calc the lod alpha multiplier (relies on the cull state being calculated first)
	pFxInst->UpdateLODAlphaMultipler();

	// set the cull flags
	if (cullState!=CULLSTATE_NOT_CULLED) 
	{
		// we are either viewport or distance culled
		PF_INCREMENT(CounterEffectInstsCulled);

		// check if the cull state requires the effect to be killed
		if ((cullState==CULLSTATE_VIEWPORT_CULLED || cullState==CULLSTATE_OCCLUSION_CULLED) && (shadowCullState==CULLSTATE_VIEWPORT_CULLED || shadowCullState==CULLSTATE_OCCLUSION_CULLED) && pEffectRule->GetViewportCullingMode()==1)
		{
			// this will get into here multiple times until the effect is deactivated
			// initially the game may still have the inst reserved and it may take a few frames until it is no longer reserved
			pFxInst->Finish(false);
		}

		//We test for shadow cull so if the effect is out of the viewport, and its shadow is visible, we would get pops.
		//They still follow same distance cull rule

		// check if the cull state requires us to stop emitting, updating or drawing
		isEmitCulled = (cullState==CULLSTATE_DISTANCE_CULLED && shadowCullState==CULLSTATE_DISTANCE_CULLED && pEffectRule->GetEmitWhenDistanceCulled()==false) || 
			((cullState==CULLSTATE_VIEWPORT_CULLED || cullState==CULLSTATE_OCCLUSION_CULLED) && (shadowCullState==CULLSTATE_VIEWPORT_CULLED || shadowCullState==CULLSTATE_OCCLUSION_CULLED) && pEffectRule->GetEmitWhenViewportCulled()==false);

		isUpdateCulled = (cullState==CULLSTATE_DISTANCE_CULLED && shadowCullState==CULLSTATE_DISTANCE_CULLED && pEffectRule->GetUpdateWhenDistanceCulled()==false) || 
			((cullState==CULLSTATE_VIEWPORT_CULLED || cullState==CULLSTATE_OCCLUSION_CULLED) && (shadowCullState==CULLSTATE_VIEWPORT_CULLED || shadowCullState==CULLSTATE_OCCLUSION_CULLED) && pEffectRule->GetUpdateWhenViewportCulled()==false);

		isDrawCulled = (cullState==CULLSTATE_DISTANCE_CULLED && pEffectRule->GetRenderWhenDistanceCulled()==false) || 
			((cullState==CULLSTATE_VIEWPORT_CULLED || cullState==CULLSTATE_OCCLUSION_CULLED) && pEffectRule->GetRenderWhenViewportCulled()==false);

		//TODO: move this out of this if condition block because we will have cases where cullstate is not culled but shadow is culled. Performance improvement.
		isShadowDrawCulled = (shadowCullState==CULLSTATE_DISTANCE_CULLED && pEffectRule->GetRenderWhenDistanceCulled()==false) || 
			((shadowCullState==CULLSTATE_VIEWPORT_CULLED || shadowCullState==CULLSTATE_OCCLUSION_CULLED) && pEffectRule->GetRenderWhenViewportCulled()==false);

	}

	//Set the states back
	pFxInst->SetIsEmitCulled(isEmitCulled);
	pFxInst->SetIsUpdateCulled(isUpdateCulled);
	pFxInst->GetDataDB()->SetIsDrawCulled(isDrawCulled);
	if(isShadowDrawCulled)
	{
		pFxInst->GetDataDB()->SetCustomFlag(PTFX_DRAW_SHADOW_CULLED);
	}
	else
	{
		pFxInst->GetDataDB()->ClearCustomFlag(PTFX_DRAW_SHADOW_CULLED);
	}
}

///////////////////////////////////////////////////////////////////////////////
//	EffectInstUpdateFinalizeFunctor
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::EffectInstUpdateFinalizeFunctor(ptxEffectInst* pFxInst)
{

	if(pFxInst->GetFlag(PTFX_EFFECTINST_CONTAINS_REFRACT_RULE))
	{
		g_ptFxManager.m_IsRefractPtfxPresent[RMPTFXTHREADER.GetUpdateBufferId()] = true;
	}

}

///////////////////////////////////////////////////////////////////////////////
//	OverrideDepthStencilStateFunctor
///////////////////////////////////////////////////////////////////////////////
grcDepthStencilStateHandle	CPtFxManager::OverrideDepthStencilStateFunctor(bool bWriteDepth, bool bTestDepth, ptxProjectionMode projMode)
{
	//Adding check for IsRenderLateVehicleAlpha to disable stencil tests
	if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SHADOWS || projMode == PTXPROJMODE_NON_WATER || projMode == PTXPROJMODE_NUM || CRenderPhaseDrawSceneInterface::IsRenderLateVehicleAlpha())
	{
		return grcStateBlock::DSS_Invalid;
	}

	if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION || g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SIMPLE)
	{
		return g_ptFxManager.m_MirrorReflection_Simple_DepthStencilState[bWriteDepth][bTestDepth];
	}

	// if we are rendering late interior alpha, then we mark the stencil. Else return invalid state
	return (CRenderPhaseDrawSceneInterface::GetLateVehicleInterAlphaState_Render()?g_ptFxManager.m_VehicleInteriorAlphaDepthStencilStates[bWriteDepth][bTestDepth]:grcStateBlock::DSS_Invalid);

}

///////////////////////////////////////////////////////////////////////////////
//	OverrideBlendStateFunctor
///////////////////////////////////////////////////////////////////////////////
grcBlendStateHandle			CPtFxManager::OverrideBlendStateFunctor(int blendMode)
{
	//both color and alpha has the alpha blend state set for shadows. Else use default
#if RMPTFX_USE_PARTICLE_SHADOWS
	if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SHADOWS)
	{
		return g_ptFxManager.m_ShadowBlendState;
	}
#endif // RMPTFX_USE_PARTICLE_SHADOWS

	// Refraction particles dont really need depth of field
	if(g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_REFRACTION || g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_MIRROR_REFLECTION || g_PtfxBuckerRenderStage == PTFX_BUCKET_RENDER_STAGE_SIMPLE)
	{
		return grcStateBlock::BS_Invalid;
	}

#if PTFX_APPLY_DOF_TO_PARTICLES
	if (CPtFxManager::UseParticleDOF())
	{
		switch (blendMode)
		{
		case (grcbsNormal):
			return g_ptFxManager.m_ptfxDofBlendNormal;

		case (grcbsAdd):                
			return g_ptFxManager.m_ptfxDofBlendAdd;

		case (grcbsSubtract):
			return g_ptFxManager.m_ptfxDofBlendSubtract;

		case (grcbsCompositeAlpha):
			return g_ptFxManager.m_ptfxDofBlendCompositeAlpha;

		case (grcbsCompositeAlphaSubtract):
			return g_ptFxManager.m_ptfxDofBlendCompositeAlphaSubtract;

		default:
			return g_ptFxManager.m_ptfxDofBlendNormal;
		}
	}
	else
		return grcStateBlock::BS_Invalid;
#else // PTFX_APPLY_DOF_TO_PARTICLES
	return grcStateBlock::BS_Invalid;
#endif // PTFX_APPLY_DOF_TO_PARTICLES
}


///////////////////////////////////////////////////////////////////////////////
//	UpdateVisualDataSettings
///////////////////////////////////////////////////////////////////////////////

void CPtFxManager::UpdateVisualDataSettings()
{
	if (g_visualSettings.GetIsLoaded())
	{
#if RMPTFX_USE_PARTICLE_SHADOWS
		//This function is called before CPtfxManager::Init()
		m_GlobalShadowIntensityValue = g_visualSettings.Get("particles.shadowintensity", DEFAULT_SHADOW_INTENSITY);
#endif
	}
};







