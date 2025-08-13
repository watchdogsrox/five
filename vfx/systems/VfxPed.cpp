///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxPed.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	01 June 2006
//	WHAT:	 
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxPed.h"

// rage
#include "file/asset.h"
#include "file/stream.h"
#include "file/token.h"
#include "grcore/texture.h"
#include "math/random.h"
#include "system/param.h"
#include "physics/simulator.h"

// framework
#include "fwanimation/boneids.h"
#include "grcore/debugdraw.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/ptfx/ptfxwrapper.h"
#include "vfx/vfxwidget.h"

// game
#include "Audio/gameobjects.h"
#include "Audio/PedAudioEntity.h"
#include "Control/Replay/Replay.h"
#include "Control/Replay/Effects/ProjectedTexturePacket.h"
#include "Control/Replay/Effects/ParticlePedFxPacket.h"
#include "Control/Replay/Effects/ParticleMiscFxPacket.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "Debug/MarketingTools.h"
#include "Game/Localisation.h"
#include "Game/Weather.h"
#include "Objects/Object.h"
#include "Peds/Ped.h"
#include "Peds/PedDebugVisualiser.h"
#include "Physics/GtaInst.h"
#include "Physics/GtaMaterialManager.h"
#include "Scene/DataFileMgr.h"
#include "TimeCycle/TimeCycle.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Particles/PtFxDefines.h"
#include "Vfx/Particles/PtFxManager.h"
#include "Vfx/Systems/VfxTrail.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/VfxHelper.h"


///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
	
CVfxPed				g_vfxPed;


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

// rag tweakable settings
dev_float	VFXPED_BREATH_RATE_BASE					= 4.0f;
dev_float	VFXPED_BREATH_RATE_RUN					= 2.0f;
dev_float	VFXPED_BREATH_RATE_SPRINT				= 1.0f;

dev_float	VFXPED_BREATH_RATE_SPEED_SLOW_DOWN		= 0.15f;
dev_float	VFXPED_BREATH_RATE_SPEED_RUN			= 0.3f;
dev_float	VFXPED_BREATH_RATE_SPEED_SPRINT			= 0.6f;

dev_float	VFXPED_FLIPPER_DECAL_WIDTH_MULT			= 2.0f;
dev_float	VFXPED_FLIPPER_DECAL_LENGTH_MULT		= 2.0f;
dev_float	VFXPED_CLOWN_DECAL_WIDTH_MULT			= 2.0f;
dev_float	VFXPED_CLOWN_DECAL_LENGTH_MULT			= 2.0f;

dev_float	VFXPED_WADE_DECAL_ALPHA_MIN				= 0.15f;
dev_float	VFXPED_WADE_DECAL_ALPHA_MAX				= 0.50f;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS VfxPed
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::Init								(unsigned initMode)
{
	if (initMode==INIT_CORE)
	{
		m_footVfxLodRangeScaleScriptThreadId = THREAD_INVALID;
		m_footVfxLodRangeScale = 1.0f;
		m_footPtFxOverrideHashNameScriptThreadId = THREAD_INVALID;
		m_footPtFxOverrideHashName.Clear();
		m_useSnowFootVfxWhenUnsheltered = false;

#if __BANK
		m_bankInitialised = false;

		// vfx skeleton
		m_renderDebugVfxSkeleton = false;
		m_renderDebugVfxSkeletonBoneScale = 0.05f;
		m_renderDebugVfxSkeletonBoneIdx = -1;
		m_renderDebugVfxSkeletonBoneWadeInfo = false;
		m_renderDebugVfxSkeletonBoneWaterInfo = false;

		// footprints
		m_renderDebugFootPrints = false;

		// wades
		m_renderDebugWades = false;
		m_disableWadeLegLeft = false;
		m_disableWadeLegRight = false;
		m_disableWadeArmLeft = false;
		m_disableWadeArmRight = false;
		m_disableWadeSpine = false;
		m_disableWadeTrails = false;
		m_disableWadeTextures = false;
		m_disableWadeParticles = false;
		m_useZUpWadeTexNormal = false;
#endif
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
    {
		g_vfxPedInfoMgr.SetupDecalRenderSettings();
    }
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::Shutdown							(unsigned shutdownMode)
{
    if(shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
    {
    }
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::RemoveScript						(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_footVfxLodRangeScaleScriptThreadId)
	{
		m_footVfxLodRangeScale = 1.0f;
		m_footVfxLodRangeScaleScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_footPtFxOverrideHashNameScriptThreadId)
	{
		m_footPtFxOverrideHashName.Clear();
		m_footPtFxOverrideHashNameScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetFootVfxLodRangeScale
///////////////////////////////////////////////////////////////////////////////


void			CVfxPed::SetFootVfxLodRangeScale			(float val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_footVfxLodRangeScaleScriptThreadId==THREAD_INVALID || m_footVfxLodRangeScaleScriptThreadId==scriptThreadId, "trying to set bullet impact user zoom when this is already in use by another script")) 
	{
		m_footVfxLodRangeScale = val; 
		m_footVfxLodRangeScaleScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetFootPtFxOverrideHashName
///////////////////////////////////////////////////////////////////////////////


void			CVfxPed::SetFootPtFxOverrideHashName		(atHashWithStringNotFinal hashName, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_footPtFxOverrideHashNameScriptThreadId==THREAD_INVALID || m_footPtFxOverrideHashNameScriptThreadId==scriptThreadId, "trying to set footstep ptfx override when this is already in use by another script")) 
	{
		m_footPtFxOverrideHashName = hashName; 
		m_footPtFxOverrideHashNameScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxPedFootStep
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::ProcessVfxPedFootStep				(CPed* pPed, phMaterialMgr::Id pedFootMtlId, VfxGroup_e decalVfxGroup, VfxGroup_e ptfxVfxGroup, Mat34V_In vFootStepMtx_In, const u32 foot, float decalLife, float decalAlpha, bool isLeft, bool BANK_ONLY(isHind))
{
	// don't process for invisible peds or peds with no vfx data
	if (!pPed->GetIsVisible() || pPed->GetPedVfx() == NULL)
	{
		return;
	}

	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// get the vfx ped foot info
	const VfxPedFootInfo* pVfxPedFootInfo_DecalVfxGroup = pVfxPedInfo->GetFootVfxInfo(decalVfxGroup);
	const VfxPedFootInfo* pVfxPedFootInfo_PtFxVfxGroup = pVfxPedInfo->GetFootVfxInfo(ptfxVfxGroup);

	// get the vfx ped wade infos
//	const VfxPedWadeInfo* pVfxPedWadeInfo_DecalVfxGroup = pVfxPedInfo->GetWadeVfxInfo(decalVfxGroup);
	const VfxPedWadeInfo* pVfxPedWadeInfo_PtFxVfxGroup = pVfxPedInfo->GetWadeVfxInfo(ptfxVfxGroup);

//	const VfxPedWadeDecalInfo* pVfxPedWadePtFxInfo = pVfxPedWadeInfo_PtFxVfxGroup->GetDecalInfo();
	const VfxPedWadePtFxInfo* pVfxPedWadePtFxInfo_PtFxVfxGroup = pVfxPedWadeInfo_PtFxVfxGroup->GetPtFxInfo();
	
	// don't do footstep vfx if the ped is too deep into a deep surface
	float surfaceDepth = pPed->GetDeepSurfaceInfo().GetDepth();
	if (surfaceDepth>pVfxPedWadePtFxInfo_PtFxVfxGroup->m_ptFxDepthEvoMin)
	{
		return;
	}




	// calculate the size of the foot (using the decal info)
	const VfxPedFootDecalInfo* pVfxPedFootDecalInfo;
#if __BANK
	if (g_vfxPed.GetDebugFootPrints())
	{
		pVfxPedFootDecalInfo = pVfxPedInfo->GetFootVfxInfo(VFXGROUP_LIQUID_BLOOD)->GetDecalInfo();
	}
	else
#endif
	{
		pVfxPedFootDecalInfo = pVfxPedFootInfo_DecalVfxGroup->GetDecalInfo();
	}

	ShoeTypes shoeType = pPed->GetPedAudioEntity()->GetFootStepAudio().GetShoeType();
	float footWidth = pVfxPedFootDecalInfo->m_decalWidth;
	float footLength = pVfxPedFootDecalInfo->m_decalLength;
	if (shoeType==SHOE_TYPE_FLIPPERS)
	{
		footWidth *= VFXPED_FLIPPER_DECAL_WIDTH_MULT;
		footLength *= VFXPED_FLIPPER_DECAL_LENGTH_MULT;
	}
	else if (shoeType==SHOE_TYPE_CLOWN)
	{
		footWidth *= VFXPED_CLOWN_DECAL_WIDTH_MULT;
		footLength *= VFXPED_CLOWN_DECAL_LENGTH_MULT;		
	}

	// calculate the centre of the foot
	Mat34V vFootStepMtx = vFootStepMtx_In;
	ScalarV vHalfShoeLength = ScalarVFromF32(footLength*0.5f);
	Vec3V vHeelPos = vFootStepMtx.GetCol3();
	Vec3V vFootForward = vFootStepMtx.GetCol1();
	Vec3V vFootCentrePos = vHeelPos + (vFootForward*vHalfShoeLength);

	// reposition the matrix at the centre of the foot
	vFootStepMtx.SetCol3(vFootCentrePos);





	// trigger the particle effects
	float depthEvo = CVfxHelper::GetInterpValue(surfaceDepth, pVfxPedWadePtFxInfo_PtFxVfxGroup->m_ptFxDepthEvoMin, 0.0f);
	TriggerPtFxPedFootStep(pPed, pVfxPedFootInfo_PtFxVfxGroup->GetPtFxInfo(), vFootStepMtx, foot, depthEvo);

	// render footprint texture
	Vec3V vFootStepPos = vFootStepMtx.GetCol3();
	Vec3V vFootStepUp = vFootStepMtx.GetCol2();
	Vec3V vFootStepSide = vFootStepMtx.GetCol0();

#if __BANK
	if (g_vfxPed.GetDebugFootPrints())
	{
		Color32 col(pVfxPedFootDecalInfo->m_decalColR, 
					pVfxPedFootDecalInfo->m_decalColG, 
					pVfxPedFootDecalInfo->m_decalColB, 
					255);

		g_decalMan.AddFootprint(pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe, 
								pVfxPedFootDecalInfo->m_decalRenderSettingCountShoe, 
								footWidth,  
								footLength, 
								col,
								(phMaterialMgr::Id)(-1), vFootStepPos, vFootStepUp, vFootStepSide, isLeft, -1.0f);

		grcDebugDraw::Axis(vFootStepMtx, 0.5f, false, -300);

		if (isHind)
		{
			if (isLeft)
			{
				grcDebugDraw::Sphere(vFootStepPos, 0.05f, Color32(1.0f, 0.5f, 0.5f, 1.0f), true, -300);
			}
			else
			{
				grcDebugDraw::Sphere(vFootStepPos, 0.05f, Color32(1.0f, 0.5f, 0.0f, 1.0f), true, -300);
			}
		}
		else
		{
			if (isLeft)
			{
				grcDebugDraw::Sphere(vFootStepPos, 0.05f, Color32(1.0f, 1.0f, 0.5f, 1.0f), true, -300);
			}
			else
			{
				grcDebugDraw::Sphere(vFootStepPos, 0.05f, Color32(1.0f, 1.0f, 0.0f, 1.0f), true, -300);
			}
		}
	}
	else
#endif
	{
		// only place textures on materials that aren't shoot thru (unless it's glass)
		if (PGTAMATERIALMGR->GetIsShootThrough(pedFootMtlId)==false || PGTAMATERIALMGR->GetIsGlass(pedFootMtlId))
		{
			// return if foot decals aren't enabled
			if (pVfxPedInfo->GetFootDecalsEnabled()==false)
			{
				return;
			}

			// get the decal settings for the correct footwear (NOTE - using PV_COMP_LOWR instead of PV_COMP_FEET as instructed by Rick Stirling)
			ShoeTypes shoeType = pPed->GetPedAudioEntity()->GetFootStepAudio().GetShoeType();
			s32 decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe;
			s32 decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountShoe;
			if (shoeType==SHOE_TYPE_BARE)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexBare;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountBare;
			}
			else if (shoeType==SHOE_TYPE_BOOTS)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexBoot;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountBoot;
			}
			else if (shoeType==SHOE_TYPE_SHOES)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountShoe;
			}
			else if (shoeType==SHOE_TYPE_HIGH_HEELS)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexHeel;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountHeel;
			}
			else if (shoeType==SHOE_TYPE_FLIPFLOPS)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexFlipFlop;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountFlipFlop;
			}
			else if (shoeType==SHOE_TYPE_FLIPPERS)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexFlipper;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountFlipper;
			}
			else if (shoeType==SHOE_TYPE_TRAINERS)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountShoe;
			}
			else if (shoeType==SHOE_TYPE_CLOWN)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountShoe;
			}
			else if (shoeType==SHOE_TYPE_GOLF)
			{
				decalRenderSettingIndex = pVfxPedFootDecalInfo->m_decalRenderSettingIndexShoe;
				decalRenderSettingCount = pVfxPedFootDecalInfo->m_decalRenderSettingCountShoe;
			}
			else
			{
				vfxAssertf(0, "unrecognised footwear type found %d - defaulting to shoes", shoeType);
			}

			// place the footprint decal
			if (decalRenderSettingIndex>-1)
			{	
				Color32 col;
				float currWetness = g_weather.GetWetness();
				if (!pPed->GetIsShelteredFromRain() && currWetness==0.0f)
				{
					col = Color32(pVfxPedFootDecalInfo->m_decalColR, 
								  pVfxPedFootDecalInfo->m_decalColG, 
								  pVfxPedFootDecalInfo->m_decalColB, 
								  static_cast<u8>(decalAlpha*255));
				}
				else
				{
					col = Color32(static_cast<u8>(pVfxPedFootDecalInfo->m_decalColR + currWetness*(pVfxPedFootDecalInfo->m_decalWetColR-pVfxPedFootDecalInfo->m_decalColR)), 
								  static_cast<u8>(pVfxPedFootDecalInfo->m_decalColG + currWetness*(pVfxPedFootDecalInfo->m_decalWetColG-pVfxPedFootDecalInfo->m_decalColG)), 
								  static_cast<u8>(pVfxPedFootDecalInfo->m_decalColB + currWetness*(pVfxPedFootDecalInfo->m_decalWetColB-pVfxPedFootDecalInfo->m_decalColB)), 
								  static_cast<u8>(decalAlpha*255));
				}

				g_decalMan.AddFootprint(decalRenderSettingIndex, 
										decalRenderSettingCount, 
										footWidth, 
										footLength, 
										col,  
										PGTAMATERIALMGR->UnpackMtlId(pedFootMtlId), vFootStepPos, vFootStepUp, vFootStepSide, isLeft, decalLife);

#if GTA_REPLAY
				if(CReplayMgr::ShouldRecord())
				{
					CReplayMgr::RecordFx<CPacketPTexFootPrint>(
									CPacketPTexFootPrint(decalRenderSettingIndex, 
									decalRenderSettingCount, 
									footWidth, 
									footLength, 
									col,  
									PGTAMATERIALMGR->UnpackMtlId(pedFootMtlId), RCC_VECTOR3(vFootStepPos), RCC_VECTOR3(vFootStepUp), RCC_VECTOR3(vFootStepSide), isLeft, decalLife));
				}
#endif // GTA_REPLAY
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxPedFootStep
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::TriggerPtFxPedFootStep				(CPed* pPed, const VfxPedFootPtFxInfo* pVfxPedFootPtFxInfo, Mat34V_In vFootStepMtx, const u32 foot, float depthEvo)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if foot ptfx aren't enabled
	if (pVfxPedInfo->GetFootPtFxEnabled()==false)
	{
		return;
	}

	// return if the ped is off screen
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// don't process in water
// 	if (pPed->GetIsInWater())
// 	{
// 		return;
// 	}

	u32 footFxHashName = pVfxPedFootPtFxInfo->m_ptFxDryName;
	if (!pPed->GetIsShelteredFromRain() && g_weather.GetWetness()>=pVfxPedFootPtFxInfo->m_ptFxWetThreshold/* && (pPed->GetMotionData()->GetIsRunning() || pPed->GetMotionData()->GetIsSprinting())*/)
	{
		// no wet foot effects in interiors
		if ((pPed && pPed->GetIsInInterior()) || g_timeCycle.GetStaleNoWeatherFX()>0.0f)
		{
			// keep dry effect
		}
		else
		{
			// play wet effect instead
			footFxHashName = pVfxPedFootPtFxInfo->m_ptFxWetName;
		}
	}

	if (m_footPtFxOverrideHashName.IsNotNull() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseOverrideFootstepPtFx))
	{
		footFxHashName = m_footPtFxOverrideHashName;
	}

	if (footFxHashName)
	{
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(footFxHashName);
		if (pFxInst)
		{
			pFxInst->SetBaseMtx(vFootStepMtx);

			// depth evo
 			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

			// speed evo
			float speedEvo = CVfxHelper::GetInterpValue(pPed->GetVelocity().Mag(), pVfxPedInfo->GetFootPtFxSpeedEvoMin(), pVfxPedInfo->GetFootPtFxSpeedEvoMax());
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

			pFxInst->SetLODScalar(m_footVfxLodRangeScale);
			pFxInst->SetUserZoom(pVfxPedFootPtFxInfo->m_ptFxScale);

			pFxInst->Start();

#if GTA_REPLAY
 			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketPedFootFx>(
					CPacketPedFootFx(footFxHashName, 
						depthEvo,
						speedEvo,
						pVfxPedFootPtFxInfo->m_ptFxScale,
						foot),
					pPed);
			}
#else
			(void)foot;
#endif // GTA_REPLAY
//			// add collision
//			g_ptFxManager.GetColnInterface().RegisterUserPlane(pFxInst, vFxPos, vFxNormal, 0.4f, 0);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxPedSurfaceWade
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::ProcessVfxPedSurfaceWade				(CPed* pPed, VfxGroup_e vfxGroup)
{
	// don't process for invisible peds or peds with no vfx data
	if (!pPed->GetIsVisible() || pPed->GetPedVfx() == NULL)
	{
		return;
	}

	// don't process in cutscenes
	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
	{
		return;
	}

	// don't process in vehicles
	if (pPed->GetIsInVehicle())
	{
		return;
	}
	
 	// don't process in water
// 	if (pPed->GetIsInWater())
// 	{
// 		return;
// 	}

	// return if the ped is off screen
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}
	
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if wade vfx aren't enabled
	if (pVfxPedInfo->GetWadePtFxEnabled()==false && pVfxPedInfo->GetWadeDecalsEnabled()==false)
	{
		return;
	}

	// get the wade infos
	const VfxPedWadeInfo* pVfxPedWadeInfo = pVfxPedInfo->GetWadeVfxInfo(vfxGroup);

	// don't do wade vfx if the ped isn't deep enough into the surface
	float surfaceDepth = pPed->GetDeepSurfaceInfo().GetDepth();
	if (pVfxPedWadeInfo==NULL || surfaceDepth<=pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxDepthEvoMin)
	{
		return;
	}

	// return if we're too far away to do wade effects or wade textures
	float maxRangeSqr = Max(pVfxPedInfo->GetWadePtFxRangeSqr(), pVfxPedInfo->GetWadeDecalsRangeSqr());
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>maxRangeSqr)
	{
		return;
	}

	// get some surface info from the ped
//	Vec3V_ConstRef vBottomSurfacePos = RCC_VEC3V(pPed->GetGroundPos());
//	Vec3V_ConstRef vBottomSurfaceNormal = RCC_VEC3V(pPed->GetGroundNormal());

	// calculate point on top surface (approx)
	Vec3V vTopSurfaceNormal = pPed->GetDeepSurfaceInfo().GetNormalWld();
	Vec3V vTopSurfacePos = pPed->GetDeepSurfaceInfo().GetPosWld();	

#if __BANK
	if (m_renderDebugWades)
	{
		grcDebugDraw::Sphere(vTopSurfacePos, 0.1f, Color32(0.0f, 1.0f, 0.0f, 1.0f), false, -1);
		grcDebugDraw::Line(vTopSurfacePos, vTopSurfacePos+vTopSurfaceNormal, Color32(0.0f, 1.0f, 0.0f, 1.0f), -1);
	}
#endif 

	// calculate the plane d
	float planeD = -Dot(vTopSurfacePos, vTopSurfaceNormal).Getf();

	// calculate the top surface plane
	Vec4V vTopSurfacePlane(vTopSurfaceNormal.GetXf(), vTopSurfaceNormal.GetYf(), vTopSurfaceNormal.GetZf(), planeD);

	// initialise the limb depths array
	VfxPedLimbInfo_s limbInfos[VFXPEDLIMBID_NUM];
	for (s32 i=0; i<VFXPEDLIMBID_NUM; i++)
	{
		limbInfos[i].depth = 0.0f;
		limbInfos[i].isIntersecting = false;
		limbInfos[i].vfxSkeletonBoneIdx = -1;
	}

	// go through the bones and gather information about them intersecting the top surface
	for (s32 i=0; i<pVfxPedInfo->GetPedSkeletonBoneNumInfos(); i++)
	{
		const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);
		const VfxPedBoneWadeInfo* pBoneWadeInfo = pSkeletonBoneInfo->GetBoneWadeInfo();
		if (pBoneWadeInfo && pBoneWadeInfo->m_sizeEvo>0.0f)
		{
#if __BANK
			if ((pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_SPINE && m_disableWadeSpine) ||
				(pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_LEG_LEFT && m_disableWadeLegLeft) ||
				(pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_LEG_RIGHT && m_disableWadeLegRight) ||
				(pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_ARM_LEFT && m_disableWadeArmLeft) ||
				(pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_ARM_RIGHT && m_disableWadeArmRight))
			{
				continue;
			}
#endif

			float boneDepth;
			Vec3V vBoneContactPos;
			if (GetBonePlaneData(*pPed, vTopSurfacePlane, pSkeletonBoneInfo->m_boneTagA, pSkeletonBoneInfo->m_boneTagB, vBoneContactPos, &boneDepth))
			{
				// update the limb depths if this is deeper than the current limb value
				VfxPedLimbId_e limbId = (VfxPedLimbId_e)(pSkeletonBoneInfo->m_limbId);
				if (boneDepth > limbInfos[limbId].depth)
				{
					// invalidate any current deepest bone on this limb
					limbInfos[limbId].depth = boneDepth;
					limbInfos[limbId].isIntersecting = true;
					limbInfos[limbId].vContactPos = vBoneContactPos;
					limbInfos[limbId].vfxSkeletonBoneIdx = i;
				}
			}
		}
	}

// 	// override the spine and leg depths with the ped depth (only interested in arms really)
// 	static bool OVERRIDE_DEPTHS = true;
// 	if (OVERRIDE_DEPTHS)
// 	{
// 		limbInfos[VFXPEDLIMBID_SPINE].depth = surfaceDepth;
// 		limbInfos[VFXPEDLIMBID_LEG_LEFT].depth = surfaceDepth;
// 		limbInfos[VFXPEDLIMBID_LEG_RIGHT].depth = surfaceDepth;
// 	}
	
	// create a buffer zone between the legs and spine so only one plays at a time 
	if (pPed->GetPedVfx()->GetIsWadeSpineIntersecting())
	{
		// spine was intersecting - if is still is then clear the leg intersections
		if (limbInfos[VFXPEDLIMBID_SPINE].isIntersecting)
		{
			// clear the leg intersections
			limbInfos[VFXPEDLIMBID_LEG_LEFT].isIntersecting = false;
			limbInfos[VFXPEDLIMBID_LEG_RIGHT].isIntersecting = false;
			pPed->GetPedVfx()->SetIsWadeSpineIntersecting(true);
		}
		// otherwise - it is no longer intersecting
		else
		{
			pPed->GetPedVfx()->SetIsWadeSpineIntersecting(false);
		}
	}
	else 
	{
		// spine was not intersecting - check if it is now
		if (limbInfos[VFXPEDLIMBID_SPINE].isIntersecting)
		{
			// it is - if either leg is still intersecting then clear the spine intersection
			if (limbInfos[VFXPEDLIMBID_LEG_LEFT].isIntersecting || limbInfos[VFXPEDLIMBID_LEG_RIGHT].isIntersecting)
			{
				limbInfos[VFXPEDLIMBID_SPINE].isIntersecting = false;
			}
			// if not the we're ok to use the spine intersection again
			else
			{
				pPed->GetPedVfx()->SetIsWadeSpineIntersecting(true);
			}
		}
	}

	// go through the limbs and update the visual effects
	float distSqrToCam = CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition());
	for (s32 i=0; i<VFXPEDLIMBID_NUM; i++)
	{
		if (limbInfos[i].isIntersecting)
		{
			const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(limbInfos[i].vfxSkeletonBoneIdx);
			const VfxPedBoneWadeInfo* pBoneWadeInfo = pSkeletonBoneInfo->GetBoneWadeInfo();
			if (pBoneWadeInfo)
			{
				// calculate the velocity of the component that the bone is attached to
				Vec3V vComponentVel = Vec3V(V_ZERO);
				if (pPed->GetRagdollInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE)
				{
					// get the component id of this bone
					s32 componentId = pPed->GetRagdollComponentFromBoneTag(pSkeletonBoneInfo->m_boneTagA);

					// get the composite bound and check the composite id validity
					phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pPed->GetRagdollInst()->GetArchetype()->GetBound());
					Assertf(componentId > -1 && componentId < pBoundComp->GetNumBounds(),"invalid component id in the FX data");

					// get the current and last component positions
					Vec3V vCurrComponentPos = pBoundComp->GetCurrentMatrix(componentId).GetCol3();
					Vec3V vPrevComponentPos = pBoundComp->GetLastMatrix(componentId).GetCol3();

					// transform these into world space
					Mat34V_ConstRef currRagdollMtx = pPed->GetRagdollInst()->GetMatrix();
					vCurrComponentPos = Transform(currRagdollMtx, vCurrComponentPos);
					Mat34V_ConstRef prevRagdollMtx = PHSIM->GetLastInstanceMatrix(pPed->GetRagdollInst());
					vPrevComponentPos = Transform(prevRagdollMtx, vPrevComponentPos);

					// calculate the velocity of this component
					vComponentVel = (vCurrComponentPos - vPrevComponentPos) * ScalarVFromF32(fwTimer::GetInvTimeStep());
				}
				else
				{
					Assertf(pPed->GetRagdollInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE, "Unexpected ragdoll bound type");
				}

				// when aiming a gun the component velocities aren't correct due to a double animation update
				// in these cases just zero out the component velocity
				static float aimingSpeedThresh = 0.1f;
				if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming) && pPed->GetVelocity().Mag()<aimingSpeedThresh)
				{
					vComponentVel = Vec3V(V_ZERO);
				}

				// calc the depth and speed evos
				float depthEvo;
				if (i==VFXPEDLIMBID_ARM_LEFT || i==VFXPEDLIMBID_ARM_RIGHT)
				{
					// use a zero depth trigger from the arms
					depthEvo = CVfxHelper::GetInterpValue(limbInfos[i].depth, pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxDepthEvoMax, 0.0f);
				}
				else
				{
					depthEvo = CVfxHelper::GetInterpValue(limbInfos[i].depth, pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxDepthEvoMax, pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxDepthEvoMin);
				}
				depthEvo *= pBoneWadeInfo->m_depthMult;
				vfxAssertf(depthEvo>=0.0f, "Depth evo out of range");

				float boneSpeed = Mag(vComponentVel).Getf();
				float speedEvo = CVfxHelper::GetInterpValue(boneSpeed, pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxSpeedEvoMin, pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxSpeedEvoMax);
				speedEvo *= pBoneWadeInfo->m_speedMult;
				vfxAssertf(speedEvo<=1.0f, "Depth ratio out of range");

				// particle effects
#if __BANK
				if (!m_disableWadeParticles)
#endif
				{
					// check if ped wade ptfx are enabled
					if (pVfxPedInfo->GetWadePtFxEnabled() && pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxName.GetHash()!=0)
					{
						// check if the ped is within ptfx creation range
						if (distSqrToCam<=pVfxPedInfo->GetWadePtFxRangeSqr())
						{
							if (boneSpeed>0.0f && boneSpeed>=pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxSpeedEvoMin)
							{
								Vec3V vDir = vComponentVel;
								vDir = Normalize(vDir);

								UpdatePtFxSurfaceWade(pPed, pVfxPedWadeInfo, limbInfos[i], (VfxPedLimbId_e)i, vDir, vTopSurfaceNormal, depthEvo, speedEvo, pBoneWadeInfo->m_sizeEvo);
							}
						}
					}
				}

				// wade decal trails
#if __BANK
				if (!m_disableWadeTrails)
#endif			
				{
					// check if ped wade decals are enabled
					if (pVfxPedInfo->GetWadeDecalsEnabled() && pVfxPedWadeInfo->GetDecalInfo()->m_decalId!=0)
					{
						// check if the ped is within fx creation range
						if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())<=pVfxPedInfo->GetWadeDecalsRangeSqr())
						{
							const VfxPedWadeDecalInfo* pVfxPedWadeDecalInfo = pVfxPedWadeInfo->GetDecalInfo();
							if (pVfxPedWadeDecalInfo->m_decalRenderSettingIndex>-1)
							{ 
								float t = pBoneWadeInfo->m_widthRatio * (1.0f-depthEvo);
								float width = pVfxPedWadeDecalInfo->m_decalSizeMin + (t *(pVfxPedWadeDecalInfo->m_decalSizeMax-pVfxPedWadeDecalInfo->m_decalSizeMin));
								//width *= pBoneWadeInfo->m_sizeEvo;

								float alphaMult = VFXPED_WADE_DECAL_ALPHA_MIN + ((1.0f-depthEvo)*(VFXPED_WADE_DECAL_ALPHA_MAX-VFXPED_WADE_DECAL_ALPHA_MIN));

#if __BANK
								if (m_useZUpWadeTexNormal)
								{
									vTopSurfaceNormal = Vec3V(V_Z_AXIS_WZERO);
								}
#endif

								VfxTrailInfo_t vfxTrailInfo;
								vfxTrailInfo.id							= fwIdKeyGenerator::Get(pPed, VFXTRAIL_PED_OFFSET_WADE+i);
								vfxTrailInfo.type						= VFXTRAIL_TYPE_WADE;
								vfxTrailInfo.regdEnt					= pPed->GetGroundPhysical();
								vfxTrailInfo.componentId				= pPed->GetGroundPhysicalComponent();
								vfxTrailInfo.vWorldPos					= limbInfos[i].vContactPos;
								vfxTrailInfo.vNormal					= vTopSurfaceNormal;
								vfxTrailInfo.vDir						= Vec3V(V_ZERO);			// not required as we have an id
								vfxTrailInfo.vForwardCheck				= pPed->GetTransform().GetB();
								vfxTrailInfo.decalRenderSettingIndex	= pVfxPedWadeDecalInfo->m_decalRenderSettingIndex;
								vfxTrailInfo.decalRenderSettingCount	= pVfxPedWadeDecalInfo->m_decalRenderSettingCount;
								vfxTrailInfo.decalLife					= -1.0f;
								vfxTrailInfo.width						= width;
								vfxTrailInfo.col						= Color32(pVfxPedWadeDecalInfo->m_decalColR, pVfxPedWadeDecalInfo->m_decalColG, pVfxPedWadeDecalInfo->m_decalColB, static_cast<u8>(alphaMult*255));
								vfxTrailInfo.pVfxMaterialInfo			= NULL;
								vfxTrailInfo.mtlId			 			= 0;
								vfxTrailInfo.liquidPoolStartSize		= 0.0f;
								vfxTrailInfo.liquidPoolEndSize			= 0.0f;
								vfxTrailInfo.liquidPoolGrowthRate		= 0.0f;
								vfxTrailInfo.createLiquidPools			= false;
								vfxTrailInfo.dontApplyDecal				= false;

								g_vfxTrail.RegisterPoint(vfxTrailInfo);
							}
						}
					}
				}
			}
		}

		UpdatePtFxSurfaceWadeDist(pPed, (VfxPedLimbId_e)i, distSqrToCam);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetBonePlaneData
///////////////////////////////////////////////////////////////////////////////

bool		 	CVfxPed::GetBonePlaneData						(const CPed& ped, Vec4V_In vPlane, eAnimBoneTag boneTagA, eAnimBoneTag boneTagB, Vec3V_InOut vContactPos, float* pBoneDepth)
{
	*pBoneDepth = 0.0f;
	
	int boneIndexA = ped.GetBoneIndexFromBoneTag(boneTagA);
	int boneIndexB = ped.GetBoneIndexFromBoneTag(boneTagB);

	if (ptxVerifyf(boneIndexA!=BONETAG_INVALID, "invalid bone index generated from bone tag (A) (%s - %d)", ped.GetDebugName(), boneTagA))
	{
		if (ptxVerifyf(boneIndexB!=BONETAG_INVALID, "invalid bone index generated from bone tag (B) (%s - %d)", ped.GetDebugName(), boneTagB))
		{
			Mat34V boneMtxA, boneMtxB;
			CVfxHelper::GetMatrixFromBoneIndex(boneMtxA, &ped, boneIndexA);
			CVfxHelper::GetMatrixFromBoneIndex(boneMtxB, &ped, boneIndexB);

			Vec3V vPosA = boneMtxA.GetCol3();
			Vec3V vPosB = boneMtxB.GetCol3();

			// special case - if the bone is the root bone then don't use this - use a matrix under the pelvis bone instead
			if (boneTagB==BONETAG_ROOT)
			{
				vPosB = vPosA;
				vPosB.SetZf(vPosB.GetZf()-0.1f);
			}

#if __BANK
			if (m_renderDebugWades)
			{
				grcDebugDraw::Line(RCC_VECTOR3(vPosA), RCC_VECTOR3(vPosB), Color32(1.0f, 1.0f, 0.0f, 1.0f), -1);

				if (ped.GetRagdollInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE)
				{
					s32 componentIdA = ped.GetRagdollComponentFromBoneTag(boneTagA);
					s32 componentIdB = ped.GetRagdollComponentFromBoneTag(boneTagB);

					phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(ped.GetRagdollInst()->GetArchetype()->GetBound());
					Assertf(componentIdA > -1 && componentIdA < pBoundComp->GetNumBounds(),"Invalid component id in the FX data");
					Assertf(componentIdB > -1 && componentIdB < pBoundComp->GetNumBounds(),"Invalid component id in the FX data");

					Vec3V vCurrBonePosA = pBoundComp->GetCurrentMatrix(componentIdA).GetCol3();
					Vec3V vCurrBonePosB = pBoundComp->GetCurrentMatrix(componentIdB).GetCol3();

					Vec3V vLastBonePosA = pBoundComp->GetLastMatrix(componentIdA).GetCol3();
					Vec3V vLastBonePosB = pBoundComp->GetLastMatrix(componentIdB).GetCol3();

					// Need to multiply by last matrix of the instance to get it in world space
					Mat34V_ConstRef matRagdollCurr = ped.GetRagdollInst()->GetMatrix();
					vCurrBonePosA = Transform(matRagdollCurr, vCurrBonePosA);
					vCurrBonePosB = Transform(matRagdollCurr, vCurrBonePosB);

					Mat34V_ConstRef matRagdollLast = PHSIM->GetLastInstanceMatrix(ped.GetRagdollInst());
					vLastBonePosA = Transform(matRagdollLast, vLastBonePosA);
					vLastBonePosB = Transform(matRagdollLast, vLastBonePosB);

					grcDebugDraw::Line(RCC_VECTOR3(vCurrBonePosA), RCC_VECTOR3(vLastBonePosA), Color32(1.0f, 0.0f, 0.0f, 1.0f), -1);
					grcDebugDraw::Line(RCC_VECTOR3(vCurrBonePosB), RCC_VECTOR3(vLastBonePosB), Color32(1.0f, 0.0f, 0.0f, 1.0f), -1);
				}
				else
				{
					Assertf(ped.GetRagdollInst()->GetArchetype()->GetBound()->GetType() == phBound::COMPOSITE, "Unexpected ragdoll bound type");
				}
			}
#endif

			// test the line against the plane
			Vec3V vPlaneNormal = vPlane.GetXYZ();
			float sideA = Dot(vPlaneNormal, vPosA).Getf() + vPlane.GetWf();
			float sideB = Dot(vPlaneNormal, vPosB).Getf() + vPlane.GetWf();

			// test if the two sides are opposite
			if (sideA*sideB<0.0f)
			{
				float t = -sideA/(sideB-sideA);
				vContactPos = vPosA + (vPosB-vPosA)*ScalarVFromF32(t);

				*pBoneDepth = -Min(sideA, sideB);
				vfxAssertf(*pBoneDepth>0.0f, "Invalid bone depth calculated");

				return true;
			}
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSurfaceWade
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::UpdatePtFxSurfaceWade					(CPed* pPed, const VfxPedWadeInfo* pVfxPedWadeInfo, const VfxPedLimbInfo_s& limbInfo, VfxPedLimbId_e limbId, Vec3V_In vDir, Vec3V_In vNormal, float depthEvo, float speedEvo, float sizeEvo)
{
	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_SURFACE_WADE+limbId, true, pVfxPedWadeInfo->GetPtFxInfo()->m_ptFxName, justCreated);
	if (pFxInst)
	{
		pFxInst->SetInvertAxes(0);
		if (limbId==VFXPEDLIMBID_LEG_LEFT || limbId==VFXPEDLIMBID_ARM_LEFT)
		{
			pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
		}

		// we want a matrix that has z in the direction of the top surface normal and y in the general direction of dir (the bone velocity)
		ScalarV vDot = (ScalarV(V_ONE)-Abs(Dot(vDir, vNormal)));
		Vec3V vOffset = SelectFT(IsLessThanOrEqual(vDot, ScalarV(V_FLT_SMALL_2)), Vec3V(V_ZERO), Vec3V(V_FLT_SMALL_2));
		Vec3V vSideDir = Cross(vDir, vNormal+vOffset);

		vSideDir = Normalize(vSideDir);
		Vec3V vForwardDir = Cross(vNormal, vSideDir);
		vForwardDir = Normalize(vForwardDir);

		Mat34V wldFxMat;
		wldFxMat.SetCol0(vSideDir);
		wldFxMat.SetCol1(vForwardDir);
		wldFxMat.SetCol2(vNormal);
		wldFxMat.SetCol3(limbInfo.vContactPos);

		// calc the offset matrix
		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pPed->GetMatrix(), wldFxMat);

		pFxInst->SetOffsetMtx(relFxMat);
		ptfxAttach::Attach(pFxInst, pPed, -1);

		// depth evo
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

		// speed evo
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// size evo
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("size", 0xC052DEA7), sizeEvo);

		if (justCreated)
		{
			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxSurfaceWadeDist
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::UpdatePtFxSurfaceWadeDist					(CPed* pPed, VfxPedLimbId_e limbId, float distSqrToCam)
{
	// find the fx system
	ptfxRegInst* pRegFxInst = ptfxReg::Find(pPed, PTFXOFFSET_PED_SURFACE_WADE+limbId);
	if (pRegFxInst)
	{
		if (pRegFxInst->m_pFxInst)
		{
			// override dist
			static float LIMB_SORTING_OFFSET = 0.001f;
			pRegFxInst->m_pFxInst->SetOverrideDistSqrToCamera(distSqrToCam+(LIMB_SORTING_OFFSET*limbId), true);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxPedParachutePedTrails
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::ProcessVfxPedParachutePedTrails			(CPed* pPed)
{
	// don't process for invisible peds or peds with no vfx data
	if (!pPed->GetIsVisible() || pPed->GetPedVfx() == NULL)
	{
		return;
	}

	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if parachute trail vfx aren't enabled
	if (pVfxPedInfo->GetParachutePedTrailPtFxEnabled()==false)
	{
		return;
	}

	for (int i=0; i<pVfxPedInfo->GetParachutePedTrailPtFxNumInfos(); i++)
	{
		// get the parrachute trail infos
		const VfxPedParachutePedTrailInfo* pVfxPedParachutePedTrailInfo = &pVfxPedInfo->GetParachutePedTrailPtFxInfo(i);

		UpdatePtFxParachutePedTrail(pPed, pVfxPedParachutePedTrailInfo, i);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfxPedParachuteCanopyTrails
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::ProcessVfxPedParachuteCanopyTrails			(CPed* pPed, CObject* pParachuteObj, s32 canopyBoneIndexL, s32 canopyBoneIndexR)
{
	// don't process for invisible peds or peds with no vfx data
	if (!pPed->GetIsVisible() || pPed->GetPedVfx() == NULL)
	{
		return;
	}

	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if parachute trail vfx aren't enabled
	if (pVfxPedInfo->GetParachuteCanopyTrailPtFxEnabled()==false)
	{
		return;
	}

	UpdatePtFxParachuteCanopyTrail(pPed, pVfxPedInfo, pParachuteObj, canopyBoneIndexL, 0);
	UpdatePtFxParachuteCanopyTrail(pPed, pVfxPedInfo, pParachuteObj, canopyBoneIndexR, 1);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPedWetness
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::UpdatePtFxPedWetness				(CPed* pPed, int vfxSkeletonBoneIdx, float outOfWaterTimer)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(vfxSkeletonBoneIdx);
	const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();

	// return if water drip ptfx aren't enabled
	if (pBoneWaterInfo->m_waterDripPtFxEnabled==false)
	{
		return;
	}

	// return if the camera is underwater
	if (CVfxHelper::GetGameCamWaterDepth()>0.0f)
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>pBoneWaterInfo->m_waterDripPtFxRange*pBoneWaterInfo->m_waterDripPtFxRange)
	{
		return;
	}

	// get the bone index to attach to
	const s32 boneIndex = pPed->GetBoneIndexFromBoneTag(pSkeletonBoneInfo->m_boneTagA);
	if (vfxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index found (%s - %d)", pPed->GetDebugName(), pSkeletonBoneInfo->m_boneTagA))
	{
		// register the fx system
		bool justCreated = false;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_WATER_DRIP+vfxSkeletonBoneIdx, true, pVfxPedInfo->GetWaterDripPtFxName(), justCreated);
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pPed, boneIndex);

			pFxInst->SetInvertAxes(0);
			if (pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_LEG_LEFT || pSkeletonBoneInfo->m_limbId==VFXPEDLIMBID_ARM_LEFT)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_Z);
			}

			// timer evo
			float timerEvo = CVfxHelper::GetInterpValue(outOfWaterTimer, pVfxPedInfo->GetWaterDripPtFxTimer(), 0.0f);
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("timer", 0x7E909EC5), timerEvo);

			// in water evo
			float inWaterEvo = pPed->GetIsInWater() ? 1.0f : 0.0f;
			pFxInst->SetEvoValueFromHash(ATSTRINGHASH("inwater", 0x3612EE1E), inWaterEvo);

			if (justCreated)
			{
				pFxInst->Start();
			}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			else
			{
				// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
				vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
			}
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxShot
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::TriggerPtFxShot						(CPed* pPed, Vec3V_In vPositionWld, Vec3V_In vNormalWld)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if ptfx aren't enabled
	if (pVfxPedInfo->GetShotPtFxEnabled()==false)
	{
		return;
	}

	// return if the ped is off screen
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>pVfxPedInfo->GetShotPtFxRangeSqr())
	{
		return;
	}

	// get the bone index to attach to
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetShotPtFxName());
	if (pFxInst)
	{
		Mat34V fxMat;
		CVfxHelper::CreateMatFromVecYAlign(fxMat, vPositionWld, vNormalWld, Vec3V(V_Z_AXIS_WZERO));

		Mat34V relFxMat;
		CVfxHelper::CreateRelativeMat(relFxMat, pPed->GetMatrix(), fxMat);

		pFxInst->SetOffsetMtx(relFxMat);	
		ptfxAttach::Attach(pFxInst, pPed, -1);

		pFxInst->Start();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxParachuteDeploy
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::TriggerPtFxParachuteDeploy				(CPed* pPed)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if ptfx aren't enabled
	if (pVfxPedInfo->GetParachuteDeployPtFxEnabled()==false)
	{
		return;
	}

	// return if the ped is off screen
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>pVfxPedInfo->GetParachuteDeployPtFxRangeSqr())
	{
		return;
	}

	// get the bone index to attach to
	const s32 boneIndex = pPed->GetBoneIndexFromBoneTag(pVfxPedInfo->GetParachuteDeployPtFxBoneTag());
	if (vfxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index found (%s - %d)", pPed->GetDebugName(), pVfxPedInfo->GetParachuteDeployPtFxBoneTag()))
	{
		ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetParachuteDeployPtFxName());
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pPed, boneIndex);

			pFxInst->Start();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxParachuteSmoke
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::UpdatePtFxParachuteSmoke				(CPed* pPed, Color32 colour)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if breath water ptfx aren't enabled
	if (pVfxPedInfo->GetParachuteSmokePtFxEnabled()==false)
	{
		return;
	}

	if (!pPed->GetIsVisible())
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>pVfxPedInfo->GetParachuteSmokePtFxRangeSqr())
	{
		return;
	}

	// get the bone index to attach to
	const s32 boneIndex = pPed->GetBoneIndexFromBoneTag(pVfxPedInfo->GetParachuteSmokePtFxBoneTag());
	if (vfxVerifyf(boneIndex!=BONETAG_INVALID, "invalid bone index found (%s - %d)", pPed->GetDebugName(), pVfxPedInfo->GetParachuteSmokePtFxBoneTag()))
	{
		atHashWithStringNotFinal ptfxHashName = pVfxPedInfo->GetParachuteSmokePtFxName();
		if (colour.GetRed()==0 && colour.GetBlue()==0 && colour.GetGreen()==0)
		{
			ptfxHashName = atHashWithStringNotFinal("mp_parachute_smoke_indep");
		}

		// register the fx system
		bool justCreated = false;
		ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_PARACHUTE_SMOKE, true, ptfxHashName, justCreated);
		if (pFxInst)
		{
			ptfxAttach::Attach(pFxInst, pPed, boneIndex);

			if (justCreated)
			{
				ptfxWrapper::SetColourTint(pFxInst, colour.GetRGB());

				pFxInst->Start();
			}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
			else
			{
				// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
				vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
			}
#endif

#if GTA_REPLAY
			if(CReplayMgr::ShouldRecord())
			{
				CReplayMgr::RecordFx<CPacketParacuteSmokeFx>(
					CPacketParacuteSmokeFx(ptfxHashName, boneIndex, VEC3V_TO_VECTOR3(colour.GetRGB())),
					pPed);
			}
#endif

		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxParachutePedTrail
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::UpdatePtFxParachutePedTrail				(CPed* pPed, const VfxPedParachutePedTrailInfo* pVfxPedParachutePedTrailInfo, int id)
{
	// set position of effect
	eAnimBoneTag boneTagA = pVfxPedParachutePedTrailInfo->m_boneTagA;
	eAnimBoneTag boneTagB = pVfxPedParachutePedTrailInfo->m_boneTagB;

	s32 boneIndexA = pPed->GetBoneIndexFromBoneTag(boneTagA);
	s32 boneIndexB = pPed->GetBoneIndexFromBoneTag(boneTagB);

	if (ptxVerifyf(boneIndexA!=BONETAG_INVALID, "invalid bone index generated from bone tag (A) (%s - %d)", pPed->GetDebugName(), boneTagA))
	{
		if (ptxVerifyf(boneIndexB!=BONETAG_INVALID, "invalid bone index generated from bone tag (B) (%s - %d)", pPed->GetDebugName(), boneTagB))
		{
			bool justCreated;
			ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_PARACHUTE_PED_TRAIL+id, true, pVfxPedParachutePedTrailInfo->m_ptFxName, justCreated);
			if (pFxInst)
			{
				Mat34V boneMtxA, boneMtxB;
				CVfxHelper::GetMatrixFromBoneIndex(boneMtxA, pPed, boneIndexA);
				CVfxHelper::GetMatrixFromBoneIndex(boneMtxB, pPed, boneIndexB);

				Vec3V vDir = boneMtxB.GetCol3() - boneMtxA.GetCol3();
				Vec3V vPos = boneMtxA.GetCol3() + (vDir*ScalarV(V_HALF));
				vDir = Normalize(vDir);

				Mat34V fxMat;
				CVfxHelper::CreateMatFromVecYAlign(fxMat, vPos, vDir, Vec3V(V_Z_AXIS_WZERO));

				// make the fx matrix relative to the bone 1
				Mat34V relFxMat;
				CVfxHelper::CreateRelativeMat(relFxMat, boneMtxA, fxMat);

				pFxInst->SetOffsetMtx(relFxMat);	

				ptfxAttach::Attach(pFxInst, pPed, boneIndexA);

				// check if the effect has just been created 
				if (justCreated)
				{
					// it has just been created - start it and set it's position
					pFxInst->Start();
				}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
				else
				{
					// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
					vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
				}
#endif
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxParachuteCanopyTrail
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::UpdatePtFxParachuteCanopyTrail				(CPed* pPed, const CVfxPedInfo* pVfxPedInfo, CObject* pParachuteObj, s32 canopyBoneIndex, int id)
{
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_PARACHUTE_CANOPY_TRAIL+id, true, pVfxPedInfo->GetParachuteCanopyTrailPtFxName(), justCreated);
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pParachuteObj, canopyBoneIndex);

		// check if the effect has just been created 
		if (justCreated)
		{
			if (id==0)
			{
				pFxInst->SetInvertAxes(PTXEFFECTINVERTAXIS_X);
			}

			// it has just been created - start it and set it's position
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxPedBreath
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::TriggerPtFxPedBreath					(CPed* pPed, s32 boneIndex)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if breath ptfx aren't enabled
	if (pVfxPedInfo->GetBreathPtFxEnabled()==false)
	{
		return;
	}

	// return if the ped is off screen or doens't have vfx data
	if (!pPed->GetIsVisibleInSomeViewportThisFrame() || pPed->GetPedVfx() == NULL)
	{
		return;
	}

	// check if the ped is within fx creation range
	Vec3V vPedPos = pPed->GetTransform().GetPosition();
	if (CVfxHelper::GetDistSqrToCamera(vPedPos)>pVfxPedInfo->GetBreathPtFxRangeSqr())
	{
		return;
	}

	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pVfxPedInfo->GetBreathPtFxName());
	if (pFxInst)
	{
		ptfxAttach::Attach(pFxInst, pPed, boneIndex);

		// speed evo
		float speedEvo = CVfxHelper::GetInterpValue(pPed->GetVelocity().Mag(), pVfxPedInfo->GetBreathPtFxSpeedEvoMin(), pVfxPedInfo->GetBreathPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// temperature evo
		float tempEvo = CVfxHelper::GetInterpValue(g_weather.GetTemperature(vPedPos), pVfxPedInfo->GetBreathPtFxTempEvoMin(), pVfxPedInfo->GetBreathPtFxTempEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("temp", 0x6CE0A51A), tempEvo);

		// rate evo
		float rateEvo = CVfxHelper::GetInterpValue(pPed->GetPedVfx()->GetBreathRate(), pVfxPedInfo->GetBreathPtFxRateEvoMin(), pVfxPedInfo->GetBreathPtFxRateEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("rate", 0x7E68C088), rateEvo);

		pFxInst->Start();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPedBreathWater
///////////////////////////////////////////////////////////////////////////////

void		 	CVfxPed::UpdatePtFxPedBreathWater			(CPed* pPed, s32 boneIndex)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	bool isScubaPed = pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsScuba);

	// return if breath water ptfx aren't enabled
	bool ptFxEnabled = isScubaPed ? pVfxPedInfo->GetBreathScubaPtFxEnabled() : pVfxPedInfo->GetBreathWaterPtFxEnabled();
	if (ptFxEnabled==false)
	{
		return;
	}

	// return if the ped is off screen
	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
	{
		return;
	}

	// check if the ped is within fx creation range
	float ptFxRangeSqr = isScubaPed ? pVfxPedInfo->GetBreathScubaPtFxRangeSqr() : pVfxPedInfo->GetBreathWaterPtFxRangeSqr();
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>ptFxRangeSqr)
	{
		return;
	}

	Mat34V mouthMtx;
	CVfxHelper::GetMatrixFromBoneIndex(mouthMtx, pPed, boneIndex);

	// return if the ped is not under water
	Vec3V vMouthPos = mouthMtx.GetCol3();

	float waterDepth;
	CVfxHelper::GetWaterDepth(vMouthPos, waterDepth);
	if (waterDepth==0.0f)
	{
		return;
	}

	// register the fx system
	bool justCreated = false;
	atHashWithStringNotFinal ptFxName = isScubaPed ? pVfxPedInfo->GetBreathScubaPtFxName() : pVfxPedInfo->GetBreathWaterPtFxName();
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_WATER_BUBBLES, true, ptFxName, justCreated);
	
	if (pFxInst)
	{
		// speed evo
		float ptFxSpeedEvoMin = isScubaPed ? pVfxPedInfo->GetBreathScubaPtFxSpeedEvoMin() : pVfxPedInfo->GetBreathWaterPtFxSpeedEvoMin();
		float ptFxSpeedEvoMax = isScubaPed ? pVfxPedInfo->GetBreathScubaPtFxSpeedEvoMax() : pVfxPedInfo->GetBreathWaterPtFxSpeedEvoMax();
		float speedEvo = CVfxHelper::GetInterpValue(pPed->GetVelocity().Mag(), ptFxSpeedEvoMin, ptFxSpeedEvoMax);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);
		
		// depth evo
		float ptFxDepthEvoMin = isScubaPed ? pVfxPedInfo->GetBreathScubaPtFxDepthEvoMin() : pVfxPedInfo->GetBreathWaterPtFxDepthEvoMin();
		float ptFxDepthEvoMax = isScubaPed ? pVfxPedInfo->GetBreathScubaPtFxDepthEvoMax() : pVfxPedInfo->GetBreathWaterPtFxDepthEvoMax();
		float depth = Min(waterDepth, 2.0f);
		float depthEvo = CVfxHelper::GetInterpValue(depth, ptFxDepthEvoMin, ptFxDepthEvoMax);
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("depth", 0xBCA972D6), depthEvo);

		ptfxAttach::Attach(pFxInst, pPed, boneIndex);

		if (justCreated)
		{
			pFxInst->Start();
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "Effect attachment error");
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxPedExhaleSmoke
///////////////////////////////////////////////////////////////////////////////

// void		 	CVfxPed::TriggerPtFxPedExhaleSmoke				(CPed* pPed, s32 boneIndex)
// {
// 	if (!pPed->GetIsVisibleInSomeViewportThisFrame())
// 	{
// 		return;
// 	}
// 
// 	// check if the ped is within fx creation range
// 	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>VFXRANGE_PED_EXHALE_SMOKE_SQR)
// 	{
// 		return;
// 	}
// 
// 	atHashWithStringNotFinal effectRuleHashName = atHashWithStringNotFinal("ped_breath_smoke");
// 
// 	ptxEffectRule* pFxRule = ptfxWrapper::GetEffectRule(effectRuleHashName);
// 	if (pFxRule && pFxRule->GetNumActiveInstances()>=20)
// 	{
// 		return;
// 	}
// 
// 	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(effectRuleHashName);
// 	if (pFxInst)
// 	{
// 		ptfxAttach::Attach(pFxInst, pPed, boneIndex);
// 
// 		pFxInst->Start();
// 	}
// }


///////////////////////////////////////////////////////////////////////////////
//  TriggerPtFxMeleeTakedown
///////////////////////////////////////////////////////////////////////////////

void				CVfxPed::TriggerPtFxMeleeTakedown			(const VfxMeleeTakedownInfo_s* pMeleeTakedownInfo)
{
	// get the matrix to attach the fx to
	Mat34V boneMtx;
	s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pMeleeTakedownInfo->pAttachEntity, (eAnimBoneTag)pMeleeTakedownInfo->attachBoneTag);

	// return if we don't have a valid matrix
	if (IsZeroAll(boneMtx.GetCol0()))
	{
		return;
	}

	// try to create the fx system
	ptxEffectInst* pFxInst = ptfxManager::GetTriggeredInst(pMeleeTakedownInfo->ptFxHashName);
	if (pFxInst)
	{
		// fx system created ok - start it up and set its position and orientation
		ptfxAttach::Attach(pFxInst, pMeleeTakedownInfo->pAttachEntity, boneIndex);

		pFxInst->SetOffsetPos(pMeleeTakedownInfo->vOffsetPos);
		pFxInst->SetOffsetRot(pMeleeTakedownInfo->vOffsetRot);

		pFxInst->SetUserZoom(pMeleeTakedownInfo->scale);	
		pFxInst->Start();	
	}
	// try to apply the damage pack
	PEDDAMAGEMANAGER.AddPedDamagePack(static_cast<CPed*>(pMeleeTakedownInfo->pAttachEntity), pMeleeTakedownInfo->dmgPackHashName, 0, 1.0);
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxMeleeTakedown
///////////////////////////////////////////////////////////////////////////////

void				CVfxPed::UpdatePtFxMeleeTakedown			(const VfxMeleeTakedownInfo_s* pMeleeTakedownInfo)
{
	// get the matrix to attach the fx to
	Mat34V boneMtx;
	s32 boneIndex = CVfxHelper::GetMatrixFromBoneTag(boneMtx, pMeleeTakedownInfo->pAttachEntity, (eAnimBoneTag)pMeleeTakedownInfo->attachBoneTag);

	// return if we don't have a valid matrix
	if (IsZeroAll(boneMtx.GetCol0()))
	{
		return;
	}

	// try to create the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pMeleeTakedownInfo->pAttachEntity, PTFXOFFSET_PED_MELEE_TAKEDOWN, true, pMeleeTakedownInfo->ptFxHashName, justCreated);
	if (pFxInst)
	{
		// fx system created ok - start it up and set its position and orientation
		ptfxAttach::Attach(pFxInst, pMeleeTakedownInfo->pAttachEntity, boneIndex);

		if (justCreated)
		{
			pFxInst->SetOffsetPos(pMeleeTakedownInfo->vOffsetPos);
			pFxInst->SetOffsetRot(pMeleeTakedownInfo->vOffsetRot);

			pFxInst->SetUserZoom(pMeleeTakedownInfo->scale);	
			pFxInst->Start();	
		}
#if __ASSERT && PTFX_ENABLE_ATTACH_CHECK
		else
		{
			// if this assert is hit then the chances are that the effect is now attached to a new entity with the same pointer
			vfxAssertf(pFxInst->GetFlag(PTFX_RESERVED_ATTACHED), "effect attachment error");
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdatePtFxPedUnderwaterDisturb
///////////////////////////////////////////////////////////////////////////////

void			CVfxPed::UpdatePtFxPedUnderwaterDisturb			(CPed* pPed, Vec3V_In vPos, Vec3V_In vNormal, VfxDisturbanceType_e vfxDisturbanceType)
{
	// get the vfx ped info
	CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pPed);
	if (pVfxPedInfo==NULL)
	{
		return;
	}

	// return if water disturbance ptfx aren't enabled
	if (pVfxPedInfo->GetUnderwaterDisturbPtFxEnabled()==false)
	{
		return;
	}

	// return if the ped is invisible
	if (!pPed->GetIsVisible())
	{
		return;
	}

	// check if the ped is within fx creation range
	if (CVfxHelper::GetDistSqrToCamera(pPed->GetTransform().GetPosition())>pVfxPedInfo->GetUnderwaterDisturbPtFxRangeSqr())
	{
		return;
	}

	atHashWithStringNotFinal fxHashName = u32(0);
	if (vfxDisturbanceType==VFX_DISTURBANCE_DEFAULT)
	{
		fxHashName = pVfxPedInfo->GetUnderwaterDisturbPtFxNameDefault();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_SAND)
	{
		fxHashName = pVfxPedInfo->GetUnderwaterDisturbPtFxNameSand();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_DIRT)
	{
		fxHashName = pVfxPedInfo->GetUnderwaterDisturbPtFxNameDirt();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_WATER)
	{
		fxHashName = pVfxPedInfo->GetUnderwaterDisturbPtFxNameWater();
	}
	else if (vfxDisturbanceType==VFX_DISTURBANCE_FOLIAGE)
	{
		fxHashName = pVfxPedInfo->GetUnderwaterDisturbPtFxNameFoliage();
	}

	if (fxHashName.GetHash()==0)
	{
		return;
	}

	// register the fx system
	bool justCreated;
	ptxEffectInst* pFxInst = ptfxManager::GetRegisteredInst(pPed, PTFXOFFSET_PED_UNDERWATER_DISTURB, true, fxHashName, justCreated);

	if (pFxInst)
	{
		Mat34V vMtx;
		CVfxHelper::CreateMatFromVecZAlign(vMtx, vPos, vNormal, pPed->GetTransform().GetB());
		pFxInst->SetBaseMtx(vMtx);

		//pFxInst->SetEvoValue("distance", distEvo);

		float speedEvo = CVfxHelper::GetInterpValue(pPed->GetVelocity().Mag(), pVfxPedInfo->GetUnderwaterDisturbPtFxSpeedEvoMin(), pVfxPedInfo->GetUnderwaterDisturbPtFxSpeedEvoMax());
		pFxInst->SetEvoValueFromHash(ATSTRINGHASH("speed", 0xF997622B), speedEvo);

		// check if the effect has just been created 
		if (justCreated)
		{
			// it has just been created - start it and set its position
			pFxInst->Start();
		}
	}
}



///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK

const char* GetLimbName(VfxPedLimbId_e vfxPedLimbId)
{
	if (vfxPedLimbId==VFXPEDLIMBID_LEG_LEFT)		return "leg left";
	else if (vfxPedLimbId==VFXPEDLIMBID_LEG_RIGHT)	return "leg right";
	else if (vfxPedLimbId==VFXPEDLIMBID_ARM_LEFT)	return "arm left";
	else if (vfxPedLimbId==VFXPEDLIMBID_ARM_RIGHT)	return "arm right";
	else if (vfxPedLimbId==VFXPEDLIMBID_SPINE)		return "spine";
	else return NULL;
}

void		 	CVfxPed::RenderDebug						()
{
	if (m_renderDebugVfxSkeleton)
	{
		CPed* pFocusPed = CPedDebugVisualiserMenu::GetFocusPed();
		if (pFocusPed)
		{
			// get the vfx ped info
			CVfxPedInfo* pVfxPedInfo = g_vfxPedInfoMgr.GetInfo(pFocusPed);
			if (pVfxPedInfo)
			{
				// go through the bones and gather information about them intersecting the top surface
				for (s32 i=0; i<pVfxPedInfo->GetPedSkeletonBoneNumInfos(); i++)
				{
					const VfxPedSkeletonBoneInfo* pSkeletonBoneInfo = pVfxPedInfo->GetPedSkeletonBoneInfo(i);

					int boneIndexA = pFocusPed->GetBoneIndexFromBoneTag(pSkeletonBoneInfo->m_boneTagA);
					int boneIndexB = pFocusPed->GetBoneIndexFromBoneTag(pSkeletonBoneInfo->m_boneTagB);
					vfxAssertf(boneIndexA!=-1, "Invalid bone index found (A)");
					vfxAssertf(boneIndexB!=-1, "Invalid bone index found (B)");
					if (boneIndexA!=-1 && boneIndexB!=-1)
					{
						Mat34V vBoneMtxA, vBoneMtxB;
						CVfxHelper::GetMatrixFromBoneIndex(vBoneMtxA, pFocusPed, boneIndexA);
						CVfxHelper::GetMatrixFromBoneIndex(vBoneMtxB, pFocusPed, boneIndexB);

						grcDebugDraw::Axis(vBoneMtxA, m_renderDebugVfxSkeletonBoneScale);
						grcDebugDraw::Axis(vBoneMtxB, m_renderDebugVfxSkeletonBoneScale);

						if (m_renderDebugVfxSkeletonBoneIdx>-1 && m_renderDebugVfxSkeletonBoneIdx==i)
						{
							grcDebugDraw::Line(vBoneMtxA.GetCol3(), vBoneMtxB.GetCol3(), Color32(1.0f, 1.0f, 0.0f, 1.0f));

							const char* pBoneNameA = pFocusPed->GetSkeletonData().GetBoneData(boneIndexA)->GetName();
							const char* pBoneNameB = pFocusPed->GetSkeletonData().GetBoneData(boneIndexB)->GetName();
							grcDebugDraw::Text(vBoneMtxA.GetCol3(), Color32(1.0f, 0.5f, 1.0f, 1.0f), 0, 0, pBoneNameA, false);
							grcDebugDraw::Text(vBoneMtxB.GetCol3(), Color32(1.0f, 0.5f, 1.0f, 1.0f), 0, 0, pBoneNameB, false);

							char displayTxt[512];
							char tempTxt[512];
							displayTxt[0] = '\0';

							formatf(tempTxt, 512, "limbName %s\nboneWadeInfoName %s\n", GetLimbName(pSkeletonBoneInfo->m_limbId), pSkeletonBoneInfo->m_boneWadeInfoName.GetCStr());
							formatf(displayTxt, 512, "%s", tempTxt);

							if (m_renderDebugVfxSkeletonBoneWadeInfo)
							{
								const VfxPedBoneWadeInfo* pBoneWadeInfo = pSkeletonBoneInfo->GetBoneWadeInfo();
								if (pSkeletonBoneInfo)
								{
									formatf(tempTxt, 512, "%s sizeEvo %.3f\n depthMult %.3f\n speedMult %.3f\n widthRatio %.3f\n", displayTxt, pBoneWadeInfo->m_sizeEvo, pBoneWadeInfo->m_depthMult, pBoneWadeInfo->m_speedMult, pBoneWadeInfo->m_widthRatio);
									formatf(displayTxt, 512, "%s", tempTxt);
								}
							}

							formatf(tempTxt, 512, "%sboneWaterInfoName %s\n", displayTxt, pSkeletonBoneInfo->m_boneWaterInfoName.GetCStr());
							formatf(displayTxt, 512, "%s", tempTxt);

							if (m_renderDebugVfxSkeletonBoneWaterInfo)
							{
								const VfxPedBoneWaterInfo* pBoneWaterInfo = pSkeletonBoneInfo->GetBoneWaterInfo();
								if (pBoneWaterInfo)
								{
									formatf(tempTxt, 512, "%s sampleSize %.3f\n boneSize %.3f\n splashInPtFx %d\n splashOutPtFx %d\n splashWadePtFx %d\n splashTrailPtFx %d\n splashInPtFxRange %.2f\n splashOutPtFxRange %.2f\n splashWadePtFxRange %.2f\n splashTrailPtFxRange %.2f\n", displayTxt, pBoneWaterInfo->m_sampleSize, pBoneWaterInfo->m_boneSize, pBoneWaterInfo->m_splashInPtFxEnabled, pBoneWaterInfo->m_splashOutPtFxEnabled, pBoneWaterInfo->m_splashWadePtFxEnabled, pBoneWaterInfo->m_splashTrailPtFxEnabled, pBoneWaterInfo->m_splashInPtFxRange, pBoneWaterInfo->m_splashOutPtFxRange, pBoneWaterInfo->m_splashWadePtFxRange, pBoneWaterInfo->m_splashTrailPtFxRange);
									formatf(displayTxt, 512, "%s", tempTxt);
								}
							}

							grcDebugDraw::Text(vBoneMtxB.GetCol3(), Color32(1.0f, 0.5f, 1.0f, 1.0f), 25, 25, displayTxt, false);	
						}
						else
						{
							grcDebugDraw::Line(vBoneMtxA.GetCol3(), vBoneMtxB.GetCol3(), Color32(1.0f, 1.0f, 1.0f, 1.0f));
						}
					}
				}
			}
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void		 	CVfxPed::InitWidgets						()
{
	if (m_bankInitialised==false)
	{
		bkBank* pVfxBank = vfxWidget::GetBank();
		pVfxBank->PushGroup("Vfx Ped", false);
		{
			pVfxBank->AddTitle("");

#if __DEV
			pVfxBank->AddTitle("SETTINGS");
			pVfxBank->AddSlider("Breath Rate Base", &VFXPED_BREATH_RATE_BASE, 0.0f, 10.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Breath Rate Run", &VFXPED_BREATH_RATE_RUN, 0.0f, 10.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Breath Rate Sprint", &VFXPED_BREATH_RATE_SPRINT, 0.0f, 10.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Breath Rate Speed Slow Down", &VFXPED_BREATH_RATE_SPEED_SLOW_DOWN, 0.0f, 2.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Breath Rate Speed Run", &VFXPED_BREATH_RATE_SPEED_RUN, 0.0f, 2.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Breath Rate Speed Sprint", &VFXPED_BREATH_RATE_SPEED_SPRINT, 0.0f, 2.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Flipper Decal Width Mult", &VFXPED_FLIPPER_DECAL_WIDTH_MULT, 0.0f, 10.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Flipper Decal Length Mult", &VFXPED_FLIPPER_DECAL_LENGTH_MULT, 0.0f, 10.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Clown Decal Width Mult", &VFXPED_CLOWN_DECAL_WIDTH_MULT, 0.0f, 10.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Clown Decal Length Mult", &VFXPED_CLOWN_DECAL_LENGTH_MULT, 0.0f, 10.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Wade Decal Alpha Min", &VFXPED_WADE_DECAL_ALPHA_MIN, 0.0f, 1.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Wade Decal Alpha Max", &VFXPED_WADE_DECAL_ALPHA_MAX, 0.0f, 1.0f, 0.01f, NullCB, "");
#endif

			pVfxBank->AddTitle("VFX SKELETON");
			pVfxBank->AddToggle("Render Debug", &m_renderDebugVfxSkeleton);
			pVfxBank->AddSlider("Bone Scale", &m_renderDebugVfxSkeletonBoneScale, 0.0f, 1.0f, 0.01f, NullCB, "");
			pVfxBank->AddSlider("Bone Index", &m_renderDebugVfxSkeletonBoneIdx, -1, 32, 1, NullCB, "");
			pVfxBank->AddToggle("Render Wade Info", &m_renderDebugVfxSkeletonBoneWadeInfo);
			pVfxBank->AddToggle("Render Water Info", &m_renderDebugVfxSkeletonBoneWaterInfo);

			pVfxBank->AddTitle("DEBUG FOOT VFX");
			pVfxBank->AddSlider("Foot Vfx Lod/Range Scale", &m_footVfxLodRangeScale, 0.0f, 20.0f, 0.5f);
			pVfxBank->AddToggle("Use Snow Foot Vfx When Unsheltered", &m_useSnowFootVfxWhenUnsheltered);
			pVfxBank->AddToggle("Render Debug", &m_renderDebugFootPrints);

			pVfxBank->AddTitle("DEBUG WADES");
			pVfxBank->AddToggle("Render Debug", &m_renderDebugWades);
			pVfxBank->AddToggle("Disable Leg Left", &m_disableWadeLegLeft);
			pVfxBank->AddToggle("Disable Leg Right", &m_disableWadeLegRight);
			pVfxBank->AddToggle("Disable Arm Left", &m_disableWadeArmLeft);
			pVfxBank->AddToggle("Disable Arm Right", &m_disableWadeArmRight);
			pVfxBank->AddToggle("Disable Spine", &m_disableWadeSpine);
			pVfxBank->AddToggle("Disable Trails", &m_disableWadeTrails);
			pVfxBank->AddToggle("Disable Textures", &m_disableWadeTextures);
			pVfxBank->AddToggle("Disable Particles", &m_disableWadeParticles);
			pVfxBank->AddToggle("Use Z Up Texture Normal", &m_useZUpWadeTexNormal);
			
			pVfxBank->AddTitle("");
			pVfxBank->AddTitle("DATA FILE");
			pVfxBank->AddButton("Reload", Reset);
		}
		pVfxBank->PopGroup();

		m_bankInitialised = true;
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
// Reset
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CVfxPed::Reset								()
{
	g_vfxPed.Shutdown(SHUTDOWN_WITH_MAP_LOADED);
	g_vfxPed.Init(INIT_AFTER_MAP_LOADED);
}
#endif
