///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	VfxHelper.cpp
//	BY	: 	Mark Nicholson 
//	FOR	:	Rockstar North
//	ON	:	12 Jun 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "VfxHelper.h"

// rage
#include "fragment/cachemanager.h"
#include "fragment/manager.h"
#include "phbound/boundcomposite.h"
#include "vector/geometry.h"
#include "vectormath/classfreefuncsv.h"

// framework
#include "entity/altskeletonextension.h"
#include "fwscene/stores/staticboundsstore.h"
#include "vfx/channel.h"

// game
#include "Audio/PedAudioEntity.h"
#include "Audio/PedFootStepAudio.h"
#include "Camera/Helpers/Frame.h"
#include "Camera/CamInterface.h"
#include "Camera/Gameplay/Aim/ThirdPersonAimCamera.h"
#include "Camera/Gameplay/Aim/ThirdPersonPedAimCamera.h"
#include "Camera/Gameplay/Follow/FollowCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "Camera/Helpers/ControlHelper.h"
#include "Control/GameLogic.h"
#include "control/replay/replay.h"
#include "Cutscene/CutsceneManagerNew.h"
#include "Frontend/MobilePhone.h"
#include "Frontend/PauseMenu.h"
#include "Network/NetworkInterface.h"
#include "Peds/Ped.h"
#include "Peds/PlayerInfo.h"
#include "Physics/gtaArchetype.h"
#include "Physics/Physics.h"
#include "Physics/WaterTestHelper.h"
#include "Renderer/PostProcessFXHelper.h"
#include "Renderer/River.h"
#include "Renderer/Water.h"
#include "Scene/DynamicEntity.h" 
#include "Scene/World/GameWorld.h"
#include "Scene/world/GameWorldWaterHeight.h"
#include "System/System.h"
#include "timecycle/TimeCycle.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Automobile.h"
#include "Weapons/Info/WeaponInfo.h"
#include "Peds/ped.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_PTFX_OPTIMISATIONS()
VFX_DECAL_OPTIMISATIONS()
VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////	


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

Vec3V				CVfxHelper::ms_vGameCamPos(0.0f, 0.0f, 0.0f);
Vec3V				CVfxHelper::ms_vGameCamForward(1.0f, 0.0f, 0.0f);
atArray<Vec2V>		CVfxHelper::ms_arrSinCos;
bool				CVfxHelper::ms_applySuspensionOnReplay = false;
float				CVfxHelper::ms_gameCamFOV = g_DefaultFov;
ScalarV				CVfxHelper::ms_vGameCamFOV = ScalarV(g_DefaultFov);
s32					CVfxHelper::ms_prevFollowCamViewMode = 0;
s32					CVfxHelper::ms_currFollowCamViewMode = 0;
float				CVfxHelper::ms_prevTimeStep = 1.0f/30.0f;

RegdVeh				CVfxHelper::ms_pShootoutBoat;
RegdVeh				CVfxHelper::ms_pRenderLastVehicle;
bool				CVfxHelper::ms_usingCinematicCam = false;
bool				CVfxHelper::ms_camInsideVehicle = false;
bool				CVfxHelper::ms_dontRenderInGameUI = false;
bool				CVfxHelper::ms_forceRenderInGameUI = false;

fwInteriorLocation	CVfxHelper::ms_camInteriorLocation;

float				CVfxHelper::ms_recentTimeSteps[NUM_RECENT_TIME_STEPS];
float				CVfxHelper::ms_smoothedTimeStep;

CVfxAsyncProbe		CVfxAsyncProbeManager::ms_probes[VFX_MAX_ASYNC_PROBES];

ScalarV			CVfxHelper::ms_vDefaultFovRecip	= ScalarV(g_DefaultFovRecip);



///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxAsyncProbe
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxAsyncProbe::Init				()
{
	m_probe.ResetProbe();
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxAsyncProbe::Shutdown			()
{
	m_probe.ResetProbe();
}


///////////////////////////////////////////////////////////////////////////////
//  SubmitWorldProbe
///////////////////////////////////////////////////////////////////////////////

void			CVfxAsyncProbe::SubmitWorldProbe	(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, u32 stateIncludeFlags)
{
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
	probeDesc.SetIncludeFlags(includeFlags);
	probeDesc.SetStateIncludeFlags(static_cast<u8>(stateIncludeFlags));

	m_probe.StartTestLineOfSight(probeDesc);
}


///////////////////////////////////////////////////////////////////////////////
//  SubmitEntityProbe
///////////////////////////////////////////////////////////////////////////////

// void			CVfxAsyncProbe::SubmitEntityProbe	(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, CEntity* pEntity)
// {
// 	WorldProbe::CShapeTestProbeDesc probeDesc;
// 	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
// 	probeDesc.SetIncludeFlags(includeFlags);
// 	probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
// 	probeDesc.SetIncludeEntity(pEntity);
// 
// 	m_probe.StartTestLineOfSight(probeDesc);
// }


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxAsyncProbeManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxAsyncProbeManager::Init				()
{
	for (s8 i=0; i<VFX_MAX_ASYNC_PROBES; i++)
	{
		ms_probes[i].Init();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CVfxAsyncProbeManager::Shutdown			()
{
	for (s8 i=0; i<VFX_MAX_ASYNC_PROBES; i++)
	{
		ms_probes[i].Shutdown();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SubmitWorldProbe
///////////////////////////////////////////////////////////////////////////////

s8				CVfxAsyncProbeManager::SubmitWorldProbe	(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, u32 stateIncludeFlags)
{
	for (s8 i=0; i<VFX_MAX_ASYNC_PROBES; i++)
	{
		if (ms_probes[i].m_probe.IsProbeActive()==false)
		{
			ms_probes[i].SubmitWorldProbe(vStartPos, vEndPos, includeFlags, stateIncludeFlags);

			if (ms_probes[i].m_probe.IsProbeActive())
			{
				return i;
			}
			else
			{
				return -1;
			}
		}
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
//  SubmitEntityProbe
///////////////////////////////////////////////////////////////////////////////

// s8				CVfxAsyncProbeManager::SubmitEntityProbe	(Vec3V_In vStartPos, Vec3V_In vEndPos, u32 includeFlags, CEntity* pEntity)
// {
// 	for (s8 i=0; i<VFX_MAX_ASYNC_PROBES; i++)
// 	{
// 		if (ms_probes[i].m_probe.IsProbeActive()==false)
// 		{
// 			ms_probes[i].SubmitEntityProbe(vStartPos, vEndPos, includeFlags, pEntity);
// 			return i;
// 		}
// 	}
// 
// 	return -1;
// }


///////////////////////////////////////////////////////////////////////////////
//  QueryProbe
///////////////////////////////////////////////////////////////////////////////

bool			CVfxAsyncProbeManager::QueryProbe		(s8 probeId, VfxAsyncProbeResults_s& probeResults)
{
	// make sure the probe is active
	if (vfxVerifyf(ms_probes[probeId].m_probe.IsProbeActive(), "querying a probe that isn't active"))
	{
		ProbeStatus probeStatus;
		if (ms_probes[probeId].m_probe.GetProbeResultsVfx(probeStatus, &RC_VECTOR3(probeResults.vPos), &RC_VECTOR3(probeResults.vNormal), &probeResults.physInstLevelIndex, &probeResults.physInstGenerationId, &probeResults.componentId, &probeResults.mtlId, &probeResults.tValue))
		{
			// the probe is finished - store the results
			if (probeStatus==PS_Blocked)
			{
				probeResults.hasIntersected = true;
			}
			else if (probeStatus==PS_Clear)
			{
				probeResults.hasIntersected = false;
			}
			else
			{
				vfxAssertf(0, "unexpected probe status found");
			}

			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  AbortProbe
///////////////////////////////////////////////////////////////////////////////

void			CVfxAsyncProbeManager::AbortProbe		(s8 probeId)
{
	// make sure the probe is active
	if (vfxVerifyf(ms_probes[probeId].m_probe.IsProbeActive(), "aborting a probe that isn't active"))
	{
		ms_probes[probeId].Shutdown();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetEntity
///////////////////////////////////////////////////////////////////////////////

CEntity*		CVfxAsyncProbeManager::GetEntity		(VfxAsyncProbeResults_s& probeResults)
{
	u16 levelIndex = probeResults.physInstLevelIndex;
	u16 generationId = probeResults.physInstGenerationId;

	CEntity* pEntity = NULL;
	if (CPhysics::GetLevel()->LegitLevelIndex(levelIndex) && !CPhysics::GetLevel()->IsNonexistent(levelIndex) && CPhysics::GetLevel()->GetInstance(levelIndex))
	{
		// the phys inst is valid - check it's the correct one
		phInst* pPhysInst = CPhysics::GetLevel()->GetInstance(levelIndex);
		if (pPhysInst->GetLevelIndex()==levelIndex && CPhysics::GetLevel()->GetGenerationID(pPhysInst->GetLevelIndex())==generationId)
		{
			pEntity = CPhysics::GetEntityFromInst(pPhysInst);
		}
	}

	return pEntity;
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxDeepSurfaceInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxDeepSurfaceInfo::Init				()
{
	m_vPosWld = Vec3V(V_ZERO); 
	m_vNormalWldXYZDepthW = Vec4V(V_Z_AXIS_WZERO); 
}


///////////////////////////////////////////////////////////////////////////////
//  Process
///////////////////////////////////////////////////////////////////////////////

void			CVfxDeepSurfaceInfo::Process			(Vec3V_In vPos, Vec3V_In vNormal, phMaterialMgr::Id mtlId, float rangeSqr)
{	
	// check that this is a 'deep' material
	if (PGTAMATERIALMGR->GetIsDeepWading(mtlId))
	{
		// check we're in range
		if (CVfxHelper::GetDistSqrToCamera(vPos)<=rangeSqr)
		{
			Vec3V vStartPos = vPos + (vNormal*ScalarVFromF32(0.95f));
			Vec3V vEndPos = vPos - (vNormal*ScalarVFromF32(0.05f));

			WorldProbe::CShapeTestHitPoint probeResult;
			WorldProbe::CShapeTestResults probeResults(probeResult);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_WEAPON);
			if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				for (int i=0; i<probeResults.GetNumHits(); i++)
				{
					phMaterialMgr::Id probeMtlId = PGTAMATERIALMGR->UnpackMtlId(probeResults[i].GetHitMaterialId());
					if (probeMtlId==mtlId)
					{
						// success - set the deep surface info
						m_vPosWld = probeResults[i].GetHitPositionV();
						const Vec3V normal = probeResults[i].GetHitNormalV();
						const ScalarV depth = Dist(vPos, probeResults[i].GetHitPositionV());
						m_vNormalWldXYZDepthW = GetFromTwo<Vec::X1,Vec::Y1,Vec::Z1,Vec::W2>(Vec4V(normal),Vec4V(depth));
						return;
					}
				}
			}
		}
	}

	// failed - default the deep surface info
	Init();
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS CVfxHelper
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::Init								()
{
	ms_pShootoutBoat = NULL;
	ms_pRenderLastVehicle = NULL;

	ms_arrSinCos.ResizeGrow(VFX_SINCOS_LOOKUP_TOTAL_ENTRIES);
	CalcSinCosTable(&ms_arrSinCos[0], VFX_SINCOS_LOOKUP_TOTAL_ENTRIES);

	ms_smoothedTimeStep = 1.0f/30.0f;
	for (int i=0; i<NUM_RECENT_TIME_STEPS; i++)
	{
		ms_recentTimeSteps[i] = ms_smoothedTimeStep;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetDistSqrToCamera
///////////////////////////////////////////////////////////////////////////////

float			CVfxHelper::GetDistSqrToCamera					(Vec3V_In vPos)
{
	// get the vector from the camera to the test position
	Vec3V vCamToPos = vPos - ms_vGameCamPos;

	// calculate the fov scale
	float fovScale = ms_gameCamFOV * g_DefaultFovRecip;
	fovScale = Min(fovScale, 1.0f);

	vfxAssertf(fovScale>=0.0f, "Negative fovScale calculated (%.3f, %.3f, %.3f)", fovScale, ms_gameCamFOV, g_DefaultFov);

	// return the sqr distance
	float distSqr = MagSquared(vCamToPos).Getf() * fovScale*fovScale;

	vfxAssertf(distSqr>=0.0f, "Negative distSqr calculated (distSqr:%.3f, fovScale:%.3f, pos:%.3f %.3f %.3f, gameCamPos:%.3f %.3f %.3f)", distSqr, fovScale, vPos.GetXf(), vPos.GetYf(), vPos.GetZf(), ms_vGameCamPos.GetXf(), ms_vGameCamPos.GetYf(), ms_vGameCamPos.GetZf());

	// increase the range if the cinematic camera is running
	// MN - commented this out for now - the fov scaling should be enough for this to work
// 	if (ms_usingCinematicCam)
// 	{
// 		// the range should now be increased by 5x
// 		distSqr *= 1.0f/25.0f;
// 	}

	return distSqr;
}

ScalarV_Out		CVfxHelper::GetDistSqrToCameraV					(Vec3V_In vPos)
{
	// get the vector from the camera to the test position
	const Vec3V vCamToPos = vPos - ms_vGameCamPos;

	// calculate the fov scale
	const ScalarV fovScale = Min(ms_vGameCamFOV * ms_vDefaultFovRecip, ScalarV(V_ONE));

	vfxAssertf(IsGreaterThanOrEqualAll(fovScale, ScalarV(V_ZERO)), "Negative fovScale calculated (%.3f, %.3f, %.3f)", fovScale.Getf(), ms_gameCamFOV, g_DefaultFov);

	// return the sqr distance
	const ScalarV distSqr = MagSquared(vCamToPos) * fovScale * fovScale;

	vfxAssertf(IsGreaterThanOrEqualAll(distSqr, ScalarV(V_ZERO)), "Negative distSqr calculated (distSqr:%.3f, fovScale:%.3f, pos:%.3f %.3f %.3f, gameCamPos:%.3f %.3f %.3f)", distSqr.Getf(), fovScale.Getf(), vPos.GetXf(), vPos.GetYf(), vPos.GetZf(), ms_vGameCamPos.GetXf(), ms_vGameCamPos.GetYf(), ms_vGameCamPos.GetZf());

	// increase the range if the cinematic camera is running
	// MN - commented this out for now - the fov scaling should be enough for this to work
// 	if (ms_usingCinematicCam)
// 	{
// 		// the range should now be increased by 5x
// 		distSqr *= 1.0f/25.0f;
// 	}

	return distSqr;
}

float			CVfxHelper::GetDistSqrToCamera					(Vec3V_In vPosA, Vec3V_In vPosB)
{
	// calculate the fov scale
	float fovScale = ms_gameCamFOV * g_DefaultFovRecip;
	fovScale = Min(fovScale, 1.0f);

	vfxAssertf(fovScale>=0.0f, "Negative fovScale calculated (%.3f, %.3f, %.3f)", fovScale, ms_gameCamFOV, g_DefaultFov);

	// return the sqr distance
	Vector3 posA = RCC_VECTOR3(vPosA);
	Vector3 posB = RCC_VECTOR3(vPosB);
	float distSqr = geomDistances::Distance2SegToPoint(posA, posB-posA, RCC_VECTOR3(ms_vGameCamPos)) * fovScale*fovScale;

	vfxAssertf(distSqr>=0.0f, "Negative distSqr calculated (distSqr:%.3f, fovScale:%.3f, posA:%.3f %.3f %.3f, posB:%.3f %.3f %.3f, gameCamPos:%.3f %.3f %.3f)", distSqr, fovScale, vPosA.GetXf(), vPosA.GetYf(), vPosA.GetZf(), vPosB.GetXf(), vPosB.GetYf(), vPosB.GetZf(), ms_vGameCamPos.GetXf(), ms_vGameCamPos.GetYf(), ms_vGameCamPos.GetZf());

	// increase the range if the cinematic camera is running
	// MN - commented this out for now - the fov scaling should be enough for this to work
// 	if (ms_usingCinematicCam)
// 	{
// 		// the range should now be increased by 5x
// 		distSqr *= 1.0f/25.0f;
// 	}

	return distSqr;
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateCurrFollowCamViewMode
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::UpdateCurrFollowCamViewMode			()
{
	camFollowCamera* pFollowCamera = camInterface::GetGameplayDirector().GetFollowCamera();
	if (pFollowCamera)
	{
		camControlHelper* pControlHelper = pFollowCamera->GetControlHelper();
		if (pControlHelper)
		{
			ms_prevFollowCamViewMode = ms_currFollowCamViewMode;
			ms_currFollowCamViewMode = pControlHelper->GetViewMode();

			return;
		}
	}

	ms_prevFollowCamViewMode = 0;
	ms_currFollowCamViewMode = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  HasCameraSwitched
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::HasCameraSwitched					(bool ignoreWhenLookingBehind, bool ignoreWhenTogglingViewMode)
{
	if (ignoreWhenLookingBehind)
	{
		camFollowCamera* pFollowCamera = camInterface::GetGameplayDirector().GetFollowCamera();
		if (pFollowCamera)
		{
			camControlHelper* pControlHelper = pFollowCamera->GetControlHelper();
			if (pControlHelper)
			{
				const bool wasLookingBehind = pControlHelper->WasLookingBehind(); 
				const bool isLookingBehind = pControlHelper->IsLookingBehind(); 

				if (isLookingBehind != wasLookingBehind)
				{
					return false;
				}
			}
		}
	}

	if (ignoreWhenTogglingViewMode)
	{
		if (ms_currFollowCamViewMode!=ms_prevFollowCamViewMode)
		{
			return false;
		}
	}

	// the camera system isn't capable of telling us this information so we have to guess
	// ideally we would be able to just query the camera system

	// calc how far the camera has moved
	Vec3V_ConstRef vCamDelta = RCC_VEC3V(camInterface::GetPosDelta());
	float camMoveDist = Mag(vCamDelta).Getf();

	// get camera's rate of change of accn
	Vec3V vCamDeltaAccn = RCC_VEC3V(camInterface::GetRateOfChangeOfAcceleration());
	float camDeltaAccnMag = Mag(vCamDeltaAccn).Getf();

	static float CAM_DELTA_POS_THRESH = 10.0f;
	static float CAM_DELTA_ACCN_THRESH = 10000.0f;
	return camMoveDist>CAM_DELTA_POS_THRESH && camDeltaAccnMag>CAM_DELTA_ACCN_THRESH;
}


///////////////////////////////////////////////////////////////////////////////
//  IsBehindCamera
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::IsBehindCamera						(Vec3V_In vPos)
{
	Vec3V vCamToTestPos = vPos - ms_vGameCamPos;
	float dot = Dot(ms_vGameCamForward, vCamToTestPos).Getf();
	if (dot<=0.0f)
	{
		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetMatrixFromBoneTag
///////////////////////////////////////////////////////////////////////////////

s32		CVfxHelper::GetMatrixFromBoneTag						(Mat34V_InOut boneMtx, const CEntity* pEntity, eAnimBoneTag boneTag)
{
	if (boneTag==BONETAG_ROOT || pEntity->GetIsDynamic()==false)
	{
		boneMtx = pEntity->GetNonOrthoMatrix();
		return -1;
	}

	vfxAssertf(pEntity->GetIsDynamic(), "trying to get a bone matrix from a non dynamic entity");
	const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);
	
	const crSkeleton* pSkeleton = pDynamicEntity->GetSkeleton();
	const crSkeletonData* pSkeletonData = NULL;
	if (pSkeleton)
	{
		pSkeletonData = &pSkeleton->GetSkeletonData();
	}
	else if (pDynamicEntity->GetFragInst())
	{
		// try to get a shared one from the frag
		pSkeletonData = &pDynamicEntity->GetFragInst()->GetType()->GetSkeletonData();
	}

	if (pSkeletonData)
	{
		int boneIndex = CPedBoneTagConvertor::GetBoneIndexFromBoneTag(*pSkeletonData, boneTag);

		if(vfxVerifyf(boneIndex>-1, "invalid bone tag passed in (%s %d %d)", pEntity->GetDebugName(), boneTag, boneIndex))
		{
			CVfxHelper::GetMatrixFromBoneIndex(boneMtx, pEntity, boneIndex);
			return boneIndex;
		}
	}
	
	boneMtx = Mat34V(V_ZERO);
	return -1;
}


///////////////////////////////////////////////////////////////////////////////
//  GetMatrixFromBoneIndex
///////////////////////////////////////////////////////////////////////////////

void		CVfxHelper::GetMatrixFromBoneIndex						(Mat34V_InOut boneMtx, bool* isDamaged, const CEntity* pEntity, s32 boneIndex, bool useAltSkeleton)
{
	*isDamaged = false;

	boneMtx = pEntity->GetNonOrthoMatrix();

	if (boneIndex!=-1 && pEntity->GetIsDynamic())
	{
		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);

		if (pDynamicEntity->GetIsTypeVehicle())
		{
			CVehicle* pVehicle = (CVehicle*)pDynamicEntity;
			if( pVehicle->GetModelIndex() == MI_PLANE_BOMBUSHKA &&
				pVehicle->GetThrustMatrixFromBoneIndex(boneMtx, boneIndex))
			{
				return;
			}
		    if (pVehicle->GetExhaustMatrixFromBoneIndex(boneMtx, boneIndex))
			{
                return;
			}
		}
		else if (pDynamicEntity->GetIsTypeObject())
		{
			CObject* pObject = (CObject*)pDynamicEntity;
			if (pObject && pObject->m_nObjectFlags.bVehiclePart)
			{
				if ((0xf0000000 & ((u32)boneIndex)) != 0)
				{
					// it's a modded exhaust bone on an object (i.e. it's broken off a vehicle)
					return;
				}
			}
		}

		const fragInst* pFragInst = pDynamicEntity->GetFragInst();
		if (pFragInst)
		{
			const fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();
			if (pFragCacheEntry)
			{
				// All frag cache entries should have an undamaged skeleton.
				const fragHierarchyInst *const pFragHierarchyInst = pFragCacheEntry->GetHierInst();
				vfxAssert(pFragHierarchyInst);
				const crSkeleton *const pUndamagedSkel = pFragHierarchyInst->skeleton;
				vfxAssert(pUndamagedSkel);
				vfxAssert((unsigned)boneIndex < pUndamagedSkel->GetBoneCount());

				if (vfxVerifyf(boneIndex>=0 && (unsigned)boneIndex<pUndamagedSkel->GetBoneCount(), "GetMatrixFromBoneIndex - bone index out of range (%s idx:%d max:%d)", pEntity->GetDebugName(), boneIndex, pUndamagedSkel->GetBoneCount()))
				{
					GetGlobalMtx(pEntity, pUndamagedSkel, boneIndex, boneMtx, useAltSkeleton);

					// If we have a damaged skeleton, then check if the matrix from
					// the undamaged skeleton was zero, and if it was, grab the
					// matrix from the damaged skeleton instead.
					const crSkeleton *const pDamagedSkel = pFragHierarchyInst->damagedSkeleton;
					if (pDamagedSkel && IsZeroAll(boneMtx.GetCol0()))
					{
						*isDamaged = true;
						GetGlobalMtx(pEntity, pDamagedSkel, boneIndex, boneMtx, useAltSkeleton);
					}
				}

				return;
			}

			const fragType *const pFragType = pFragInst->GetType();
			vfxAssert(pFragType);
			const crSkeletonData *const pSkeletonData = &pFragType->GetSkeletonData();
			if (pSkeletonData)
			{
				vfxAssert((unsigned)boneIndex < (unsigned)pSkeletonData->GetNumBones());

				Mat34V objectMtx;
				pSkeletonData->ComputeObjectTransform(boneIndex, objectMtx);
				Transform(boneMtx, pEntity->GetNonOrthoMatrix(), objectMtx);
				return;
			}
		}
		else
		{
			const crSkeleton* pSkel = pDynamicEntity->GetSkeleton();
			if (pSkel)
			{
				if (vfxVerifyf(boneIndex>=0 && (unsigned)boneIndex<pSkel->GetBoneCount(), "GetMatrixFromBoneIndex - bone index out of range (%s idx:%d max:%d)", pEntity->GetDebugName(), boneIndex, pSkel->GetBoneCount()))
				{
					GetGlobalMtx(pEntity, pSkel, boneIndex, boneMtx, useAltSkeleton);
				}

				return;
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetMatrixFromBoneIndex_Skinned
///////////////////////////////////////////////////////////////////////////////

void		CVfxHelper::GetMatrixFromBoneIndex_Skinned					(Mat34V_InOut boneMtx, bool* isDamaged, const CEntity* pEntity, s32 boneIndex)
{
	*isDamaged = false;

    // for dynamic drawables we want to use the entity matrix, the decal offset is based on the entity matrix
    // even though it uses a bone index for the request.
	const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);
	fragInst* pFragInst = pDynamicEntity->GetFragInst();
	if (!pDynamicEntity->GetIsTypeVehicle() && (!pFragInst || !pFragInst->GetType() || !pFragInst->GetType()->GetCommonDrawable()))
	{
        boneMtx = pEntity->GetNonOrthoMatrix();
        return;
	}

	// if the object is skinned then we need to get a matrix differently
	if (boneIndex>=0)
	{
		vfxAssertf(pEntity->GetIsDynamic(), "can't get skinned bone info from a non dynamic entity");
		const CDynamicEntity* pDynamicEntity = static_cast<const CDynamicEntity*>(pEntity);
		fragInst* pFragInst = pDynamicEntity->GetFragInst();
		if (pFragInst && pFragInst->GetType() && pFragInst->GetType()->GetCommonDrawable())
		{
			bool isSkinned = pFragInst->GetType()->IsModelSkinned();
			if (isSkinned)
			{
				const crSkeletonData* pSkeletonData;
				Mat34V objectMtx;

				const fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();
				if (pFragCacheEntry)
				{
					// All frag cache entries should have an undamaged skeleton.
					const fragHierarchyInst *const pFragHierarchyInst = pFragCacheEntry->GetHierInst();
					vfxAssert(pFragHierarchyInst);
					const crSkeleton *const pUndamagedSkel = pFragHierarchyInst->skeleton;
					vfxAssert(pUndamagedSkel);
					vfxAssert((unsigned)boneIndex < pUndamagedSkel->GetBoneCount());
					objectMtx = pUndamagedSkel->GetObjectMtx(boneIndex);
					pSkeletonData = &pUndamagedSkel->GetSkeletonData();

					// If we have a damaged skeleton, then check if the matrix from
					// the undamaged skeleton was zero, and if it was, grab the
					// matrix from the damaged skeleton instead.
					const crSkeleton *const pDamagedSkel = pFragHierarchyInst->damagedSkeleton;
					if (pDamagedSkel && IsZeroAll(objectMtx.GetCol0()))
					{
						*isDamaged = true;
						objectMtx = pDamagedSkel->GetObjectMtx(boneIndex);
						pSkeletonData = &pDamagedSkel->GetSkeletonData();
					}
				}
				else
				{
					const fragType *const pFragType = pFragInst->GetType();
					vfxAssert(pFragType);
					pSkeletonData = &pFragType->GetSkeletonData();
					pSkeletonData->ComputeObjectTransform(boneIndex, objectMtx);
				}

				vfxAssert(pSkeletonData);
				vfxAssert((unsigned)boneIndex < (unsigned)pSkeletonData->GetNumBones());

				// get the local skinned bone matrix
				CVfxHelper::GetSkinnedLocalBoneMtx(boneMtx, objectMtx, boneIndex, *pSkeletonData);

				// transform to get the global skinned bone matrix
				Transform(boneMtx, pEntity->GetNonOrthoMatrix(), boneMtx);
				return;
			}
		}
	}

	GetMatrixFromBoneIndex(boneMtx, isDamaged, pEntity, boneIndex);
}


///////////////////////////////////////////////////////////////////////////
//  ApplyVehicleSuspensionToMtx
///////////////////////////////////////////////////////////////////////////

void CVfxHelper::ApplyVehicleSuspensionToMtx(Mat34V_InOut vMtx, const fwEntity* pEntity)
{		
	// vehicles may need an extra offset for the suspension
	const CEntity* pActualEntity = static_cast<const CEntity*>(pEntity);
	if (pActualEntity && pActualEntity->GetIsTypeVehicle() && ((const CVehicle*)pActualEntity)->InheritsFromAutomobile())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pActualEntity);
		const CAutomobile* pAutoEntity = (const CAutomobile*)pActualEntity;
		float yRotation = ((CAutomobile*)pVehicle)->GetFakeRotationY();
		Mat34VRotateLocalY(vMtx, ScalarV(yRotation));

		float fFakeSuspensionLoweringAmount = pAutoEntity->GetFakeSuspensionLoweringAmount();
		Translate(vMtx, vMtx, Vec3V(0.0f, 0.0f, -fFakeSuspensionLoweringAmount));
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetMatrixFromBoneIndex_Smash
///////////////////////////////////////////////////////////////////////////////

void		CVfxHelper::GetMatrixFromBoneIndex_Smash					(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneIndex)
{
	// check for this window being rolled down
	if (pEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
		if (pVehicle)
		{
			for (int i=VEH_FIRST_WINDOW; i<=VEH_LAST_WINDOW; i++)
			{
				eHierarchyId hierarchyId = (eHierarchyId)i;
				int windowBoneIndex = pVehicle->GetBoneIndex(hierarchyId);
				if (windowBoneIndex>-1 && windowBoneIndex==boneIndex)
				{
					if (pVehicle->IsWindowDown(hierarchyId))
					{
						boneMtx = Mat34V(V_ZERO);
						return;
					}
				}
			}
		}
	}

	// get the matrix
	CVfxHelper::GetMatrixFromBoneIndex_Skinned(boneMtx, pEntity, boneIndex);
	vfxAssertf(IsFiniteAll(boneMtx), "smash group matrix is invalid");

	// check for a zeroed matrix
	if (IsZeroAll(boneMtx.GetCol0()))
	{
		// matrix has been zeroed - must be from a frag object whose component is removed
		// get the matrix from the bound instead of the precalced render matrices
		GetDefaultToWorldMatrix(boneMtx, *pEntity, boneIndex);
		vfxAssertf(IsFiniteAll(boneMtx), "smash group default matrix is invalid");
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetMatrixFromBoneTag_Lights
///////////////////////////////////////////////////////////////////////////////

// - specific version for lights
// - if the object is a frag but is currently a dummy object then the frag info
//   will be got from the model info's shared skeleton

void			CVfxHelper::GetMatrixFromBoneTag_Lights			(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneTag)
{
	crSkeleton* pSkel = NULL;

	// try to get the skeleton from the dynamic entity
	// if it is a frag inst then this will be got instead
	if (pEntity->GetIsDynamic())
	{
		const CDynamicEntity* pDynEntity = static_cast<const CDynamicEntity*>(pEntity);
		pSkel = const_cast<crSkeleton*>(pDynEntity->GetSkeleton());
	}

	// if we have a skeleton then return the global matrix
	if (pSkel)
	{
		int boneIndex = 0;
		if(!pSkel->GetSkeletonData().ConvertBoneIdToIndex((u16)boneTag, boneIndex))
		{
			vfxAssertf(false, "Trying to use a bone tag that doesn't exist on this model (%s)", pEntity->GetModelName());
		}

		GetGlobalMtx(pEntity, pSkel, boneIndex, boneMtx);

		return;
	}

	// if we couldn't get a skeleton then check the model info to see if this is a frag
	CBaseModelInfo* pModelinfo = pEntity->GetBaseModelInfo();
	gtaFragType* pFragType = pModelinfo->GetFragType();
	if (pFragType)
	{
		const crSkeletonData& skeletonData = pFragType->GetSkeletonData();

		int boneIndex = 0;
		if (!skeletonData.ConvertBoneIdToIndex((u16)boneTag, boneIndex))
		{
			vfxAssertf(false, "Trying to use a bone tag that doesn't exist on this model (%s)", pEntity->GetModelName());
		}

		Mat34V objectMtx;
		skeletonData.ComputeObjectTransform(boneIndex, objectMtx);
		Transform(boneMtx, pEntity->GetNonOrthoMatrix(), objectMtx);
		return;
	}

	// if all else fails return the entity matrix
	boneMtx = pEntity->GetNonOrthoMatrix();
}


///////////////////////////////////////////////////////////////////////////////
//  GetMatrixFromBoneIndex_Lights
///////////////////////////////////////////////////////////////////////////////

// - same as earlier, but using an index.

void			CVfxHelper::GetMatrixFromBoneIndex_Lights		(Mat34V_InOut boneMtx, const CEntity* pEntity, s32 boneIndex)
{
	crSkeleton* pSkel = NULL;

	// try to get the skeleton from the dynamic entity
	// if it is a frag inst then this will be got instead
	if (pEntity->GetIsDynamic())
	{
		const CDynamicEntity* pDynEntity = static_cast<const CDynamicEntity*>(pEntity);
		pSkel = const_cast<crSkeleton*>(pDynEntity->GetSkeleton());
	}

	// if we have a skeleton then return the global matrix
	if (pSkel)
	{
		GetGlobalMtx(pEntity ,pSkel, boneIndex, boneMtx);
		return;
	}

	// if we couldn't get a skeleton then check the model info to see if this is a frag
	CBaseModelInfo* pModelinfo = pEntity->GetBaseModelInfo();
	gtaFragType* pFragType = pModelinfo->GetFragType();
	if (pFragType)
	{
		Mat34V objectMtx;
		pFragType->GetSkeletonData().ComputeObjectTransform(boneIndex, objectMtx);
		Transform(boneMtx, pEntity->GetNonOrthoMatrix(), objectMtx);

		return;
	}

	// if all else fails return the entity matrix
	boneMtx = pEntity->GetNonOrthoMatrix();
}

///////////////////////////////////////////////////////////////////////////////
//  GetDefaultToWorldMatrix
///////////////////////////////////////////////////////////////////////////////
//
//  Returns the matrix that will take vertex buffer verts into world space
//  Uses only the bounds to calculate this matrix
//
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::GetDefaultToWorldMatrix				(Mat34V_InOut returnMtx, const CEntity& entity, s32 boneIndex)
{
	fragInst* pFragInst = entity.GetFragInst();
	if (pFragInst)
	{		
		phArchetype* pArchetype = pFragInst->GetArchetype();
		if (vfxVerifyf(pArchetype, "Cannot get archetype from fragment instance"))
		{
			phBound* pBound = pArchetype->GetBound();
			if (vfxVerifyf(pBound, "Cannot get bound from archetype"))
			{
				if (vfxVerifyf(pBound->GetType()==phBound::COMPOSITE, "Fragment instance has non composite bound"))
				{
					// find the fragment child index (the composite bound id) from the bone matrix
					phBoundComposite* pBoundComposite = static_cast<phBoundComposite*>(pBound);
					s32 childId = pFragInst->GetComponentFromBoneIndex(boneIndex);
					s32 groupId = pFragInst->GetType()->GetPhysics(0)->GetAllChildren()[childId]->GetOwnerGroupPointerIndex();
					if (pFragInst->GetCacheEntry() && pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(groupId))
					{
						if (vfxVerifyf(childId>-1, "Invalid child id for bone index"))
						{
							const CVehicle* pVehicle = entity.GetIsTypeVehicle() ? static_cast<const CVehicle*>(&entity) : NULL;
							const crSkeletonData* pSkeletonData = NULL;
							Vector3 deformation = VEC3_ZERO;

							if (pVehicle && pFragInst)
							{
								const fragCacheEntry* pFragCacheEntry = pFragInst->GetCacheEntry();
								const fragHierarchyInst *const pFragHierarchyInst = pFragCacheEntry->GetHierInst();
								vfxAssert(pFragHierarchyInst);
								const crSkeleton *const pUndamagedSkel = pFragHierarchyInst ? pFragHierarchyInst->skeleton : NULL;
								vfxAssert(pUndamagedSkel);
								pSkeletonData = pUndamagedSkel ? &pUndamagedSkel->GetSkeletonData() : NULL;
							}

							bool isChildOfDoor = false;

							//Grab the door bone deformation, if the current glass component is a child of a door (i.e. its parent bone)
							for(int doorIndex = 0; pVehicle && pSkeletonData && (doorIndex < pVehicle->GetNumDoors()); doorIndex++)
							{
								const CCarDoor* pDoor = pVehicle->GetDoor(doorIndex);

								int nDoorBoneIndex = pVehicle->GetBoneIndex(pDoor->GetHierarchyId());
								int nParentBoneIndex = pSkeletonData->GetBoneData(boneIndex)->GetParent()->GetIndex();

								if((nDoorBoneIndex > -1) && (nParentBoneIndex == nDoorBoneIndex))
								{
									s32 doorChildId = pFragInst->GetComponentFromBoneIndex(nDoorBoneIndex);
									Mat34V dBoundOrig = pFragInst->GetTypePhysics()->GetCompositeBounds()->GetCurrentMatrix(doorChildId);

									// Find the composite bound component matrix of the original (default pose / authored) bound
									// Verts are authored in this space
									// This matrix goes from the entity space to the child's space in its default orientation and translation
									// In this case we are finding it for the doorBound rather than the windowBound
									returnMtx = dBoundOrig;

									// Go from this space back to entity space
									// So take the inverse of the default pose matrix
									// e.g. change to default door space -> entity space
									InvertTransformOrtho(returnMtx, returnMtx);

									Mat34V dBoneOrig = pSkeletonData->GetDefaultTransform(nDoorBoneIndex);
									Mat34V dBoneCurrent = RCC_MAT34V(pVehicle->GetObjectMtx(nDoorBoneIndex));

									//Multiply doorBoundOriginalMatrix * doorBoneOriginalMatrixInverse * doorBoneCurrentMatrix to get the doorBoundCurrentMatrix
									//NOTE:  we are doing this because the bounds are inconsistent as to whether they contain the damage deformation or not and the skeleton always has it
									InvertTransformOrtho(dBoneOrig, dBoneOrig);
									Mat34V dBoundCurrrent;
									Transform(dBoundCurrrent, dBoneOrig, dBoundOrig);
									Transform(dBoundCurrrent, dBoneCurrent, dBoundCurrrent);

									// Now transform by the CURRENT matrix
									// e.g. so our matrix is now equal to original door pose -> current door pose
									Transform(returnMtx, dBoundCurrrent, returnMtx);

									isChildOfDoor = true;
									break;
								}
							}

							if (!isChildOfDoor)
							{
								// Find the composite bound component matrix of the original (default pose / authored) bound
								// Verts are authored in this space
								// This matrix goes from the entity space to the child's space in its default orientation and translation
								// e.g. for a car door, this is the transform from entity -> door space
								returnMtx = pFragInst->GetTypePhysics()->GetCompositeBounds()->GetCurrentMatrix(childId);
								vfxAssertf(IsFiniteAll(returnMtx), "CVfxHelper::GetDefaultToWorldMatrix - non-finite matrix 1");

								// Go from this space back to entity space
								// So take the inverse of the default pose matrix
								// e.g. change to default door space -> entity space
								InvertTransformOrtho(returnMtx, returnMtx);
								vfxAssertf(IsFiniteAll(returnMtx), "CVfxHelper::GetDefaultToWorldMatrix - non-finite matrix 2");

								// Now transform by the CURRENT matrix
								// e.g. so our matrix is now equal to original door pose -> current door pose
								Mat34V_ConstRef currentMatrix = pBoundComposite->GetCurrentMatrix(childId);
								vfxAssertf(IsFiniteAll(currentMatrix), "CVfxHelper::GetDefaultToWorldMatrix - non-finite matrix 3");
								Transform(returnMtx, currentMatrix, returnMtx);

								// Don't apply the suspension offset again if Replay is in control as the recorded bone for the window will
								// already have this applied.  See url:bugstar:2914257
								REPLAY_ONLY(if(!CReplayMgr::IsReplayInControlOfWorld() || ms_applySuspensionOnReplay))
								{
									ApplyVehicleSuspensionToMtx(returnMtx, &entity);
								}
								vfxAssertf(IsFiniteAll(returnMtx), "CVfxHelper::GetDefaultToWorldMatrix - non-finite matrix 5");
							}

							vfxAssertf(IsFiniteAll(returnMtx), "CVfxHelper::GetDefaultToWorldMatrix - non-finite matrix 4");

							// Now put it in world space
							Transform(returnMtx, entity.GetMatrix(), returnMtx);
							vfxAssertf(IsFiniteAll(returnMtx), "CVfxHelper::GetDefaultToWorldMatrix - non-finite matrix 6");

							return;
						}
						else
						{
							returnMtx = Mat34V(V_ZERO);
							return;
						}
					}
					else
					{
						returnMtx = Mat34V(V_ZERO);
						return;
					}
				}
			}
		}
	}
		
	returnMtx = entity.GetMatrix();
	vfxAssertf(IsFiniteAll(returnMtx), "CVfxHelper::GetDefaultToWorldMatrix - non-finite matrix 7");
}


///////////////////////////////////////////////////////////////////////////////
//  GetDefaultToBoundMatrix
///////////////////////////////////////////////////////////////////////////////
//
//  Returns the matrix that will take vertex buffer verts into bound space
//  Uses only the bounds to calculate this matrix
//
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::GetDefaultToBoundMatrix				(Mat34V_InOut returnMtx, const CEntity& entity, s32 componentId)
{
	fragInst* pFragInst = entity.GetFragInst();
	if (pFragInst)
	{		
		phArchetype* pArchetype = pFragInst->GetArchetype();
		if (vfxVerifyf(pArchetype, "Cannot get archetype from fragment instance"))
		{
			phBound* pBound = pArchetype->GetBound();
			if (vfxVerifyf(pBound, "Cannot get bound from archetype"))
			{
				if (vfxVerifyf(pBound->GetType()==phBound::COMPOSITE, "Fragment instance has non composite bound"))
				{
					// Find the composite bound component matrix of the original (default pose / authored) bound
					// Verts are authored in this space
					// This matrix goes from the entity space to the child's space in its default orientation and translation
					// e.g. for a car door, this is the transform from entity -> door space
					returnMtx = pFragInst->GetTypePhysics()->GetCompositeBounds()->GetCurrentMatrix(componentId);

					// Go from this space back to entity space
					// So take the inverse of the default pose matrix
					// e.g. change to default door space -> entity space
					//returnMtx.Transpose3x4();
					InvertTransformOrtho(returnMtx, returnMtx);

					return;
				}
			}
		}
	}

	returnMtx = entity.GetMatrix();
}


///////////////////////////////////////////////////////////////////////////////
//  GetIsBoneBroken
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetIsBoneBroken						(const CEntity& entity, s32 boneIndex)
{
	fragInst* pFragInst = entity.GetFragInst();
	if (pFragInst)
	{	
		s32 groupIndex = pFragInst->GetGroupFromBoneIndex(boneIndex);
		if (groupIndex!=-1)
		{
			if (pFragInst->GetCacheEntry())
			{
				vfxAssertf(pFragInst->GetCacheEntry()->GetHierInst(), "Cannot hierarchy instance from cache entry");
				return pFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsSet(groupIndex);
			}
		}
	}
	
	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetSkinnedLocalBoneMtx
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::GetSkinnedLocalBoneMtx				(Mat34V_InOut skinnedMtx, Mat34V_In objectMtx, u32 boneIndex, const crSkeletonData& skeletonData) 
{
	vfxAssertf(boneIndex<skeletonData.GetNumBones(), "Called with out of range bone index");

	Mat34V_ConstRef transMtx = skeletonData.GetCumulativeInverseJoints()[boneIndex];
	Transform(skinnedMtx, objectMtx, transMtx);	
}


///////////////////////////////////////////////////////////////////////////////
//  GetGlobalMtx
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::GetGlobalMtx						(const CEntity* pEntity, const crSkeleton* pSkeleton, u32 boneIdx, Mat34V_InOut outMtx, bool useAltSkeleton) 
{
	// check if we're requesting the 'alternative' skeleton
	// when a ped is in a first person view they have 2 skeletons
	// the first person skeleton is the main one 
	// the third person skeleton is the alternative one
	if (useAltSkeleton)
	{
		// check for the entity having an alternative skeleton extension
		// objects/weapons that can be attached to the ped will have this single offset matrix 
		// this world space matrix denotes the bone that the object/weapon is attached to
		const fwAltSkeletonExtension* pExt = pEntity->GetExtension<fwAltSkeletonExtension>();
		if (pExt)
		{
			// we need to get the local bone matrix of the object/weapon attached to the ped
			// then transform it by the 'alternative' matrix of the bone it's attached to
			Mat34V vLclMtx = pSkeleton->GetObjectMtx(boneIdx);
			rage::Transform(outMtx, pExt->m_offset, vLclMtx);
			return;
		}

		// if we don't have an alternative skeleton extension then it's because the entity is the ped itself
		// i.e. not an object attached to the ped
		// in this case we need query for the 'alternative' bone position of the ped
		extern const crSkeleton* GetSkeletonForDraw(const CEntity *entity);
		GetSkeletonForDraw(pEntity)->GetGlobalMtx(boneIdx, outMtx);
		return;
	}

	// use the normal bone matrix
	pSkeleton->GetGlobalMtx(boneIdx, outMtx);
}


///////////////////////////////////////////////////////////////////////////////
//  CreateMatFromVecY
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::CreateMatFromVecY				(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir_In)
{	
	vfxAssertf(IsEqualAll(vPos, vPos), "Pos is not a valid number");
	vfxAssertf(IsEqualAll(vDir_In, vDir_In), "Dir is not a valid number");
	vfxAssertf(IsZeroAll(vDir_In)==false, "Dir is zero");

	Vec3V vDir = Normalize(vDir_In);

	vfxAssertf(IsEqualAll(vDir, vDir), "Normalised dir is not a valid number");
	vfxAssertf(Abs(Mag(vDir)).Getf()>0.50f && Abs(Mag(vDir)).Getf()<1.50f, "Dir is not normalised");

	// set up the matrix as far as we can so far
	returnMtx.SetIdentity3x3();
	returnMtx.SetCol3(vPos);
	returnMtx.SetCol1(vDir);

	// create a random vector to calculate the a component
	for(int i=0; i<2; i++)
	{
		Vec3V vRandVec;
		GetRandomVector(vRandVec);

		returnMtx.SetCol0(Cross(returnMtx.GetCol1(), vRandVec));

		if (MagSquared(returnMtx.GetCol0()).Getf() > 0.05f*0.05f)
			break;
	} 

	returnMtx.SetCol0(Normalize(returnMtx.GetCol0()));

	// do a cross product to set the c component
	returnMtx.SetCol2(Cross(returnMtx.GetCol0(), returnMtx.GetCol1()));

	if (!returnMtx.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)))
	{
		ReOrthonormalize3x3(returnMtx, returnMtx);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CreateMatFromVecYAlign
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::CreateMatFromVecYAlign		(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir_In, Vec3V_In vAlign)
{	
	vfxAssertf(IsEqualAll(vPos, vPos), "Pos is not a valid number");
	vfxAssertf(IsEqualAll(vDir_In, vDir_In), "Dir is not a valid number");
	vfxAssertf(IsZeroAll(vDir_In)==false, "Dir is zero");

	Vec3V vDir = Normalize(vDir_In);

	vfxAssertf(IsEqualAll(vDir, vDir), "Normalised dir is not a valid number");
	vfxAssertf(Abs(Mag(vDir)).Getf()>0.50f && Abs(Mag(vDir)).Getf()<1.50f, "Dir is not normalised");

	// set up the matrix as far as we can so far
	returnMtx.SetIdentity3x3();
	returnMtx.SetCol3(vPos);
	returnMtx.SetCol1(vDir);

	// create a random vector to calculate the a component
	bool useAlignVec = true;
	for(int i=0; i<2; i++)
	{
		Vec3V vAlignVec;
		if (useAlignVec)
		{
			vAlignVec = vAlign;
			useAlignVec = false;
		}
		else
		{
			GetRandomVector(vAlignVec);
		}

		returnMtx.SetCol0(Cross(returnMtx.GetCol1(), vAlignVec));

		if (MagSquared(returnMtx.GetCol0()).Getf() > 0.05f*0.05f)
			break;
	} 

	returnMtx.SetCol0(Normalize(returnMtx.GetCol0()));

	// do a cross product to set the c component
	returnMtx.SetCol2(Cross(returnMtx.GetCol0(), returnMtx.GetCol1()));

	if (!returnMtx.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)))
	{
		ReOrthonormalize3x3(returnMtx, returnMtx);
	}
}

///////////////////////////////////////////////////////////////////////////////
//  CreateMatFromVecZ
///////////////////////////////////////////////////////////////////////////////
void			CVfxHelper::CreateMatFromVecYAlignUnSafe		(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir_In, Vec3V_In vAlign)
{	
	vfxAssertf(IsEqualAll(vPos, vPos), "Pos is not a valid number");
	vfxAssertf(IsEqualAll(vDir_In, vDir_In), "Dir is not a valid number");
	
	// Careful here : dir goes to v1 and not v0, as one would expect.
	const Vec3V v1 = vDir_In;
	const Vec3V v0 = Normalize(Cross(v1, vAlign));
	const Vec3V v2 = Normalize(Cross(v0, v1));

	returnMtx.SetCol0(v0);
	returnMtx.SetCol1(v1);
	returnMtx.SetCol2(v2);
	returnMtx.SetCol3(vPos);
}

///////////////////////////////////////////////////////////////////////////////
//  CreateMatFromVecZ
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::CreateMatFromVecZ					(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir_In)
{	
	vfxAssertf(IsEqualAll(vPos, vPos), "Pos is not a valid number");
	vfxAssertf(IsEqualAll(vDir_In, vDir_In), "Dir is not a valid number");
	vfxAssertf(IsZeroAll(vDir_In)==false, "Dir is zero");

	Vec3V vDir = Normalize(vDir_In);

	vfxAssertf(IsEqualAll(vDir, vDir), "Normalised dir is not a valid number");
	vfxAssertf(Abs(Mag(vDir)).Getf()>0.50f && Abs(Mag(vDir)).Getf()<1.50f, "Dir is not normalised");

	// set up the matrix as far as we can so far
	returnMtx.SetIdentity3x3();
	returnMtx.SetCol3(vPos);
	returnMtx.SetCol2(vDir);

	// create a random vector to calculate the a component
	for(int i=0; i<2; i++)
	{
		Vec3V vRandVec;
		GetRandomVector(vRandVec);

		returnMtx.SetCol0(Cross(vRandVec, returnMtx.GetCol2()));

		if (MagSquared(returnMtx.GetCol0()).Getf() > 0.05f*0.05f)
			break;
	} 

	returnMtx.SetCol0(Normalize(returnMtx.GetCol0()));

	// do a cross product to set the b component
	returnMtx.SetCol1(Cross(returnMtx.GetCol2(), returnMtx.GetCol0()));

	if (!returnMtx.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)))
	{
		ReOrthonormalize3x3(returnMtx, returnMtx);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CreateMatFromVecZAlign
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::CreateMatFromVecZAlign				(Mat34V_InOut returnMtx, Vec3V_In vPos, Vec3V_In vDir_In, Vec3V_In vAlign)
{	
	vfxAssertf(IsEqualAll(vPos, vPos), "Pos is not a valid number");
	vfxAssertf(IsEqualAll(vDir_In, vDir_In), "Dir is not a valid number");
	vfxAssertf(IsZeroAll(vDir_In)==false, "Dir is zero");

	Vec3V vDir = Normalize(vDir_In);

	vfxAssertf(IsEqualAll(vDir, vDir), "Normalised dir is not a valid number");
	vfxAssertf(Abs(Mag(vDir)).Getf()>0.50f && Abs(Mag(vDir)).Getf()<1.50f, "Dir is not normalised");
	// set up the matrix as far as we can so far
	returnMtx.SetIdentity3x3();
	returnMtx.SetCol3(vPos);
	returnMtx.SetCol2(vDir);

	// create a random vector to calculate the a component
	bool useAlignVec = true;
	for(int i=0; i<2; i++)
	{
		Vec3V vAlignVec;
		if (useAlignVec)
		{
			vAlignVec = vAlign;
			useAlignVec = false;
		}
		else
		{
			GetRandomVector(vAlignVec);
		}

		returnMtx.SetCol0(Cross(vAlignVec, returnMtx.GetCol2()));

		if (MagSquared(returnMtx.GetCol0()).Getf() > 0.05f*0.05f)
			break;
	} 

	returnMtx.SetCol0(Normalize(returnMtx.GetCol0()));

	// do a cross product to set the b component
	returnMtx.SetCol1(Cross(returnMtx.GetCol2(), returnMtx.GetCol0()));

	if (!returnMtx.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)))
	{
		ReOrthonormalize3x3(returnMtx, returnMtx);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CreateRelativeMat
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::CreateRelativeMat			(Mat34V_InOut returnMtx, Mat34V_In parentMtx, Mat34V_In wldMtx)
{
	Mat34V invParentMtx;
	InvertTransformOrtho(invParentMtx, parentMtx);
	Transform(returnMtx, invParentMtx, wldMtx);
}


///////////////////////////////////////////////////////////////////////////////
//  GetLocalEntityPos
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out		CVfxHelper::GetLocalEntityPos		(const CEntity& entity, Vec3V_In vWldPos)
{
	Vec3V vLclPos = entity.GetTransform().UnTransform(vWldPos);
	return vLclPos;
}


///////////////////////////////////////////////////////////////////////////////
//  GetLocalEntityDir
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out		CVfxHelper::GetLocalEntityDir		(const CEntity& entity, Vec3V_In vWldDir)
{
	Vec3V vLclDir = entity.GetTransform().UnTransform3x3(vWldDir);
	return vLclDir;
}


///////////////////////////////////////////////////////////////////////////////
//  GetLocalEntityBonePos
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetLocalEntityBonePos	(const CEntity& entity, s32 boneIndex, Vec3V_In vWldPos, Vec3V_InOut vLclPos)
{
	// get the entity bone matrix
	Mat34V boneMtx;
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, &entity, boneIndex);

	// check the matrix is valid
	if (IsZeroAll(boneMtx.GetCol0())==false)
	{
		// it's non-zero - bring the position and direction into local space
		vLclPos = UnTransformOrtho(boneMtx, vWldPos);	

		// success
		return true;
	}

	// failure - zero matrix generated
	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetLocalEntityBoneDir
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetLocalEntityBoneDir	(const CEntity& entity, s32 boneIndex, Vec3V_In vWldDir, Vec3V_InOut vLclDir)
{
	// check the input direction is valid
	vfxAssertf(IsZeroAll(vWldDir)==false, "vWldDir is zero");
	vfxAssertf(IsEqualAll(vWldDir, vWldDir), "vWldDir is not a valid number");
	vfxAssertf(Abs(Mag(vWldDir)).Getf()>0.50f && Abs(Mag(vWldDir)).Getf()<1.50f, "vWldDir is not normalised");

	// get the entity bone matrix
	Mat34V boneMtx;
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, &entity, boneIndex);

	// check the matrix is valid
	if (IsZeroAll(boneMtx.GetCol0())==false)
	{
		// it's non-zero - bring the position and direction into local space	
		vLclDir = UnTransform3x3Ortho(boneMtx, vWldDir);

		// check the output direction is valid
		vfxAssertf(IsZeroAll(vLclDir)==false, "vLclDir is zero");
		vfxAssertf(IsEqualAll(vLclDir, vLclDir), "vLclDir is not a valid number");
		vfxAssertf(Abs(Mag(vLclDir)).Getf()>0.50f && Abs(Mag(vLclDir)).Getf()<1.50f, "vLclDir is not normalised");

		// success
		return true;
	}

	// failure - zero matrix generated
	return false;
}



///////////////////////////////////////////////////////////////////////////////
//  GetLocalEntityBoneDir
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetLocalEntityBonePosDir(const CEntity& entity, s32 boneIndex, Vec3V_In vWldPos, Vec3V_In vWldDir, Vec3V_InOut vLclPos, Vec3V_InOut vLclDir)
{
	// check the input direction is valid
	vfxAssertf(IsZeroAll(vWldDir)==false, "vWldDir is zero");
	vfxAssertf(IsEqualAll(vWldDir, vWldDir), "vWldDir is not a valid number");
	vfxAssertf(Abs(Mag(vWldDir)).Getf()>0.50f && Abs(Mag(vWldDir)).Getf()<1.50f, "vWldDir is not normalised (%.3f, %.3f, %.3f)", vWldDir.GetXf(), vWldDir.GetYf(), vWldDir.GetZf());

	// get the entity bone matrix
	Mat34V boneMtx;
	CVfxHelper::GetMatrixFromBoneIndex(boneMtx, &entity, boneIndex);

	// check the matrix is valid
	if (IsZeroAll(boneMtx.GetCol0())==false)
	{
		// it's non-zero - bring the position and direction into local space
		vLclPos = UnTransformOrtho(boneMtx, vWldPos);	
		vLclDir = UnTransform3x3Ortho(boneMtx, vWldDir);

		// check the output direction is valid
		vfxAssertf(IsZeroAll(vLclDir)==false, "vLclDir is zero");
		vfxAssertf(IsEqualAll(vLclDir, vLclDir), "vLclDir is not a valid number");
		vfxAssertf(Abs(Mag(vLclDir)).Getf()>0.50f && Abs(Mag(vLclDir)).Getf()<1.50f, 
				   "vLclDir is not normalised (vWldDir %.3f, %.3f, %.3f, vLclDir %.3f, %.3f, %.3f, boneMtx0 %.3f, %.3f, %.3f, boneMtx1 %.3f, %.3f, %.3f, boneMtx2 %.3f, %.3f, %.3f, boneMtx3 %.3f, %.3f, %.3f, entity %s, boneIndex %d)", 
					vWldDir.GetXf(), vWldDir.GetYf(), vWldDir.GetZf(), 																																			
					vLclDir.GetXf(), vLclDir.GetYf(), vLclDir.GetZf(), 																																			
				    boneMtx.GetCol0().GetXf(), boneMtx.GetCol0().GetYf(), boneMtx.GetCol0().GetZf(), 																										
				    boneMtx.GetCol1().GetXf(), boneMtx.GetCol1().GetYf(), boneMtx.GetCol1().GetZf(), 																											
				    boneMtx.GetCol2().GetXf(), boneMtx.GetCol2().GetYf(), boneMtx.GetCol2().GetZf(), 																											
				    boneMtx.GetCol3().GetXf(), boneMtx.GetCol3().GetYf(), boneMtx.GetCol3().GetZf(),
					entity.GetDebugName(),
					boneIndex);

		// success
		return true;
	}

	// failure - zero matrix generated
	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetRandomTangent
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetRandomTangent			(Vec3V_InOut vTangent, Vec3V_In vNormal)
{
#if __ASSERT
	float normalMag = Mag(vNormal).Getf();
	if (normalMag<=0.95f || normalMag>=1.05f)
	{
		vfxAssertf(0, "Non unit normal passed in (%.5f)", normalMag);
	}
#endif

	// only loop looking for tangents a max 2 times, then bomb out with 
	for (int i=0; i<5; i++)
	{
		Vec3V vRandVec;
		GetRandomVector(vRandVec);
		vTangent = Cross(vNormal, vRandVec);

		// found a valid tangent
		if (MagSquared(vTangent).Getf() > 0.05f*0.05f)
		{
			vTangent = Normalize(vTangent);
			break;
		}
		else if (i==4)
		{
			return false;
		}
	}


#if __ASSERT
	float tangentMag = Mag(vTangent).Getf();
	if (tangentMag<=0.95f || tangentMag>=1.05f)
	{
		vfxAssertf(0, "Non unit tangent calculated (%.5f)", tangentMag);
	}
	vfxAssertf(Abs(Dot(vTangent, vNormal)).Getf()<0.05f, "Non perpendicular tangent and normal calculated");
#endif

	return true;
} 


///////////////////////////////////////////////////////////////////////////////
//  GetRandomTangentAlign
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetRandomTangentAlign		(Vec3V_InOut vTangent, Vec3V_In vNormal, Vec3V_In vAlign)
{
#if __ASSERT
	float normalMag = Mag(vNormal).Getf();
	if (normalMag<=0.95f || normalMag>=1.05f)
	{
		vfxAssertf(0, "Non unit normal passed in (%.5f)", normalMag);
	}
#endif

	// only loop looking for tangents a max 2 times, then bomb out with 
	for (int i=0; i<5; i++)
	{
		vTangent = Cross(vNormal, vAlign);

		// found a valid tangent
		if (MagSquared(vTangent).Getf() > 0.05f*0.05f)
		{
			vTangent = Normalize(vTangent);
			break;
		}
		else if (i==4)
		{
			return false;
		}
	}


#if __ASSERT
	float tangentMag = Mag(vTangent).Getf();
	if (tangentMag<=0.95f || tangentMag>=1.05f)
	{
		vfxAssertf(0, "Non unit tangent calculated (%.5f)", tangentMag);
	}
	vfxAssertf(Abs(Dot(vTangent, vNormal)).Getf()<0.05f, "Non perpendicular tangent and normal calculated");
#endif

	return true;
} 


///////////////////////////////////////////////////////////////////////////////
//  GetInterpValue
///////////////////////////////////////////////////////////////////////////////

float			CVfxHelper::GetInterpValue				(float currVal, float startVal, float endVal, bool clamp)
{	
	const float unClampedRatio = (currVal-startVal) / (endVal-startVal);
	const float ratio = clamp ? Clamp(unClampedRatio, 0.0f, 1.0f) : unClampedRatio;

	return ratio;
}


///////////////////////////////////////////////////////////////////////////////
//  GetRatioValue
///////////////////////////////////////////////////////////////////////////////

float			CVfxHelper::GetRatioValue				(float ratio, float startVal, float endVal, bool clamp)
{	
	const float unClampedVal = startVal + (ratio*(endVal-startVal));
	const float val = clamp ? Clamp(unClampedVal, startVal, endVal) : unClampedVal;

	return val;
}


///////////////////////////////////////////////////////////////////////////////
//  IsInInterior
///////////////////////////////////////////////////////////////////////////////

bool CVfxHelper::IsInInterior(const CEntity* pEntity) 
{
	vfxAssert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));

	bool bInterior;

	if (pEntity)
	{
		// Check if entity is an MLO or if it's in an MLO room
		bInterior = pEntity->GetIsTypeMLO() || pEntity->m_nFlags.bInMloRoom;

		// We might be dealing with a dummy entity owned by static bounds (in that case,
		// the previous check is invalid)
		if (bInterior == false && pEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS)
		{
			strLocalIndex boundsStoreIndex = strLocalIndex(pEntity->GetIplIndex());
			if (g_StaticBoundsStore.IsValidSlot(boundsStoreIndex))
			{
				InteriorProxyIndex proxyId = -1;
				strLocalIndex depSlot;
				g_StaticBoundsStore.GetDummyBoundData(boundsStoreIndex, proxyId, depSlot);

				if (proxyId > -1 && proxyId < CInteriorProxy::GetPool()->GetSize())
				{
					CInteriorProxy* pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(proxyId);
					if (pInteriorProxy)
					{
						bInterior = true;
					}
				}
			}
		}
	}
	else
	{
		bInterior =  ms_camInteriorLocation.IsValid();
	}

	return bInterior;
}

///////////////////////////////////////////////////////////////////////////////
//  GetInteriorLocationFromStaticBounds
///////////////////////////////////////////////////////////////////////////////
// There are three cases this function covers:
//
// 1. Not in an interior													- returns ITR_NOT_IN_INTERIOR
// 2. In an interior, known interior location								- returns ITR_KNOWN_INTERIOR
// 3. In an interior, but can't provide an interior location (probably		- returns ITR_UNKNOWN_INTERIOR
//	  because the collision probe didn't hit ground - i.e.: roomId == 0)
// 4. Entity is not owned by static bounds									- returns ITR_NOT_STATIC_BOUNDS
VfxInteriorTest_e CVfxHelper::GetInteriorLocationFromStaticBounds(const CEntity* pIntersectionEntity, s32 intersectionRoomIdx, fwInteriorLocation& interiorLocation)
{
	interiorLocation.MakeInvalid();
	
	// Collided with bounds
	if (pIntersectionEntity && pIntersectionEntity->GetOwnedBy() == ENTITY_OWNEDBY_STATICBOUNDS)
	{
		// new style - the interior physics is part of the static bounds. Extract interior proxy from collision data if possible...
		strLocalIndex boundsStoreIndex = strLocalIndex(pIntersectionEntity->GetIplIndex());
		Assert(g_StaticBoundsStore.IsValidSlot(boundsStoreIndex));

		InteriorProxyIndex proxyId = -1;
		strLocalIndex depSlot;
		g_StaticBoundsStore.GetDummyBoundData(boundsStoreIndex, proxyId, depSlot);

		// we're definitely not in an interior
		if (proxyId == -1)
			return ITR_NOT_IN_INTERIOR;

		CInteriorProxy* pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(proxyId);

		// we're definitely not in an interior
		if (pInteriorProxy == NULL)
			return ITR_NOT_IN_INTERIOR;


		//We set the interior location no matter what as the walls seem to have the room ID set to 0
		//i'm not sure if this is a bug in Art or in the asset pipeline but just to be safe, i'm
		//allowing this no matter what
		interiorLocation = CInteriorProxy::CreateLocation(pInteriorProxy, intersectionRoomIdx);

		// we are in an interior but, unless we hit ground, the room index will be 0, and we won't
		// be able to create a valid interior location. 
		if (intersectionRoomIdx != 0)
		{
			return ITR_KNOWN_INTERIOR;
		}
		else
			return ITR_UNKNOWN_INTERIOR;
	}

	return ITR_NOT_STATIC_BOUNDS;
}

bool				CVfxHelper::IsEntityInCurrentVehicle		(CEntity* pEntity)
{
	if(pEntity == NULL)
	{
		return false;
	}

	if(pEntity->GetModelNameHash() == ATSTRINGHASH("Lux_Prop_Champ_Flute_LUXE", 0xBC374AF2) || pEntity->GetModelNameHash() == ATSTRINGHASH("LUX_P_CHAMP_FLUTE_S", 0xE61117BD))
	{
		return true;
	}

	CPed* pPed = NULL;
	if(pEntity->GetIsTypeObject())
	{
		CObject* pObject = static_cast<CObject*>(pEntity);
		if(pObject && pObject->GetWeapon())
		{
			CWeapon* pWeapon = pObject->GetWeapon();
			pPed = const_cast<CPed*>(pWeapon->GetOwner());
		}

	}
	else if(pEntity->GetIsTypePed())
	{
		pPed = static_cast<CPed*>(pEntity);
	}

	CVehicle* pVehicle = NULL;
	if(pPed && pPed->GetIsInVehicle())
	{
		pVehicle = pPed->GetMyVehicle();
	}
	else if(pEntity->GetIsTypeVehicle())
	{
		pVehicle = static_cast<CVehicle*>(pEntity);
	}

	const fwAttachmentEntityExtension *extension = pEntity->GetAttachmentExtension();
	if (extension)
	{
		fwEntity *parent = extension->GetAttachParent();
		if (parent && (parent->GetType() == ENTITY_TYPE_VEHICLE))
		{
			pVehicle = static_cast<CVehicle*>(parent);
		}
	}

	if(pVehicle)
	{
		return (pVehicle->IsCamInsideVehicleInterior());

	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetHashFromInteriorLocation
///////////////////////////////////////////////////////////////////////////////
u64				CVfxHelper::GetHashFromInteriorLocation			(fwInteriorLocation interiorLocation)
{
	return CInteriorProxy::GenerateUniqueHashFromLocation(interiorLocation);

}


///////////////////////////////////////////////////////////////////////////////
//  IsNearWater
///////////////////////////////////////////////////////////////////////////////
bool				CVfxHelper::IsNearWater					(Vec3V_In vPos)
{
	float waterHeight = 0.0f;
	bool isNearWater = CWaterTestHelper::IsNearRiver(vPos) || Water::GetWaterLevelNoWaves(VEC3V_TO_VECTOR3(vPos), &waterHeight, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
	return isNearWater;
}

///////////////////////////////////////////////////////////////////////////////
//  IsUnderwater
///////////////////////////////////////////////////////////////////////////////
bool				CVfxHelper::IsUnderwater				(Vec3V_In vPos, bool bUseCameraWaterZ, ScalarV_In vDifferenceThreshold)
{
	float waterHeight = 0.0f;
	if(bUseCameraWaterZ)
	{
		waterHeight = GetGameCamWaterZ();
	}
	else
	{
		//Check if position is near river/ocean
		if(!IsNearWater(vPos))
		{
			return false;	
		}

		waterHeight = CGameWorldWaterHeight::GetWaterHeightAtPos(vPos);

	}

	return (IsLessThanOrEqualAll(vPos.GetZ() - ScalarVFromF32(waterHeight), vDifferenceThreshold)?true:false);

}

///////////////////////////////////////////////////////////////////////////////
//  GetWaterDepth
///////////////////////////////////////////////////////////////////////////////

WaterTestResultType	CVfxHelper::GetWaterDepth				(Vec3V_In vPos, float& waterDepth, fwEntity* pEntity)
{	
	float waterZ;
	WaterTestResultType waterType = GetWaterZ(vPos, waterZ, pEntity);
	waterDepth = Max(0.0f, waterZ-vPos.GetZf());
	return waterType;
}


///////////////////////////////////////////////////////////////////////////////
//  GetWaterZ
///////////////////////////////////////////////////////////////////////////////

WaterTestResultType	CVfxHelper::GetWaterZ					(Vec3V_In vPos, float& waterZ, fwEntity* pEntity)
{	
	waterZ = -9999.0f;

	static bool useEntityLookup = true;
	if (pEntity && useEntityLookup)
	{
		CEntity* pActualEntity = static_cast<CEntity*>(pEntity);
		if (pActualEntity->GetIsPhysical())
		{
			CPhysical* pPhysical = static_cast<CPhysical*>(pActualEntity);
			if (pPhysical)
			{
				float floaterWaterZ;
				WaterTestResultType waterType = pPhysical->m_Buoyancy.GetWaterLevelIncludingRivers(RCC_VECTOR3(vPos), &floaterWaterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
				if (waterType>WATERTEST_TYPE_NONE)
				{
					waterZ = floaterWaterZ;
					return waterType;
				}

				return WATERTEST_TYPE_NONE;
			}
		}
	}

	float oceanWaterZ = 0.0f;
	bool foundOceanWater = GetOceanWaterZ(vPos, oceanWaterZ);
	if (foundOceanWater)
	{
		waterZ = oceanWaterZ;
		return WATERTEST_TYPE_OCEAN;
	}

	float riverWaterZ = 0.0f;
	bool foundRiverWater = GetRiverWaterZ(vPos, riverWaterZ);
	if (foundRiverWater)
	{
		waterZ = riverWaterZ;
		return WATERTEST_TYPE_RIVER;
	}

	return WATERTEST_TYPE_NONE;
}


///////////////////////////////////////////////////////////////////////////////
//  GetOceanWaterZ
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetOceanWaterZ				(Vec3V_In vPos, float& oceanWaterZ)
{	
	return Water::GetWaterLevel(RCC_VECTOR3(vPos), &oceanWaterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL);
}


///////////////////////////////////////////////////////////////////////////////
//  GetOceanWaterZAndRiseSpeed
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetOceanWaterZAndSpeedUp	(Vec3V_In vPos, float& oceanWaterZ, float& oceanWaterSpeedUp)
{	
	if (!Water::GetWaterLevelNoWaves(RCC_VECTOR3(vPos), &oceanWaterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
	{
		return false;
	}

	Vec3V vNormal;
	Water::AddWaveToResult(vPos.GetXf(), vPos.GetYf(), &oceanWaterZ, &RC_VECTOR3(vNormal), &oceanWaterSpeedUp);

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  GetRiverWaterZ
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::GetRiverWaterZ				(Vec3V_In vPos, float& riverWaterZ)
{	
	float riverHeightAtPos;
	if (CWaterTestHelper::GetRiverHeightAtPosition(RCC_VECTOR3(vPos), &riverHeightAtPos))
	{
		riverWaterZ = riverHeightAtPos;

		return true;
	}

	return false;

// 	WorldProbe::CShapeTestProbeDesc probeTest;
// 	WorldProbe::CShapeTestHitPoint result[32];
// 	WorldProbe::CShapeTestResults results(result, 32);
// 	probeTest.SetResultsStructure(&results);
// 	probeTest.SetContext(WorldProbe::ERiverBoundQuery);
// 	probeTest.SetIsDirected(false);
// 	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_RIVER_TYPE);
// 
// 	Vec3V vStartPos = vPos;
// 	vStartPos.SetZf(-50.0f);
// 	Vec3V vEndPos = vPos;
// 	vEndPos.SetZf(200.0f);
// 	probeTest.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
// 
// 	bool foundRiverWater = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
// 	if (foundRiverWater)
// 	{
// 		riverWaterZ = results[0].GetHitPosition().z;
// 		for (s32 i=1; i<results.GetNumHits(); i++)
// 		{
// 			riverWaterZ = Max(riverWaterZ, results[i].GetHitPosition().z);
// 		}
// 
// 		return true;
// 	}
// 
// 	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  TestLineAgainstWater
///////////////////////////////////////////////////////////////////////////////

WaterTestResultType	CVfxHelper::TestLineAgainstWater		(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_InOut vWaterPos)
{
	Vec3V vOceanWaterPos;
	bool foundOceanWater = TestLineAgainstOceanWater(vStartPos, vEndPos, vOceanWaterPos);
	if (foundOceanWater)
	{
		vWaterPos = vOceanWaterPos;
		return WATERTEST_TYPE_OCEAN;
	}

	Vec3V vRiverWaterPos;
	bool foundRiverWater = TestLineAgainstRiverWater(vStartPos, vEndPos, vRiverWaterPos);
	if (foundRiverWater)
	{
		vWaterPos = vRiverWaterPos;
		return WATERTEST_TYPE_RIVER;
	}

	return WATERTEST_TYPE_NONE;
}


///////////////////////////////////////////////////////////////////////////////
//  TestLineAgainstOceanWater
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::TestLineAgainstOceanWater	(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_InOut vOceanWaterPos)
{
	return Water::TestLineAgainstWater(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos), &RC_VECTOR3(vOceanWaterPos));
}


///////////////////////////////////////////////////////////////////////////////
//  TestLineAgainstRiverWater
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::TestLineAgainstRiverWater	(Vec3V_In vStartPos, Vec3V_In vEndPos, Vec3V_InOut vRiverWaterPos)
{
	WorldProbe::CShapeTestProbeDesc probeTest;
	WorldProbe::CShapeTestHitPoint result;
	WorldProbe::CShapeTestResults results(result);
	probeTest.SetResultsStructure(&results);
	probeTest.SetContext(WorldProbe::ERiverBoundQuery);
	probeTest.SetIsDirected(false);
	probeTest.SetIncludeFlags(ArchetypeFlags::GTA_RIVER_TYPE);
	probeTest.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));

	bool foundRiverWater = WorldProbe::GetShapeTestManager()->SubmitTest(probeTest);
	if (foundRiverWater)
	{
		vRiverWaterPos = Vec3V(results[0].GetHitPosition());
		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPedFootMtlId
///////////////////////////////////////////////////////////////////////////////

phMaterialMgr::Id	CVfxHelper::GetPedFootMtlId				(CPed* pPed)
{
#if __DEV
	static bool useAudioMethod = true;
	if (useAudioMethod)
	{
		return pPed->GetPedAudioEntity()->GetFootStepAudio().GetCurrentPackedMaterialId();
	}
	else
	{
		return pPed->GetPackedGroundMaterialId();
	}
#else
	return pPed->GetPedAudioEntity()->GetFootStepAudio().GetCurrentPackedMaterialId();
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ShouldRenderInGameUI
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::ShouldRenderInGameUI			()
{
	if (ms_forceRenderInGameUI)
	{
		return true;
	}

	CPed* pPlayerPed = FindPlayerPed();
	bool isPlayerDead = pPlayerPed && pPlayerPed->ShouldBeDead();
	bool isPlayerArrested = pPlayerPed && pPlayerPed->GetIsArrested();
	bool isCutsceneRunning = CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning();
	bool isGameStatePlaying = CGameLogic::IsGameStateInPlay();
	bool isNetworkGame = NetworkInterface::IsGameInProgress();
	bool isPauseMenuFading = PAUSEMENUPOSTFXMGR.IsFading();
	bool isTakingPhoto = CPhoneMgr::CamGetState();
	if (ms_dontRenderInGameUI || isCutsceneRunning || isPauseMenuFading || isTakingPhoto ||
		(!isNetworkGame && (isPlayerDead || isPlayerArrested || !isGameStatePlaying)))
	{
		return false;
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////////
//  SetLastTimeStep
///////////////////////////////////////////////////////////////////////////////

void			CVfxHelper::SetLastTimeStep					(float timeStep)
{
	float accumTimeStep = timeStep;

	for (int i=0; i<NUM_RECENT_TIME_STEPS-1; i++)
	{
		accumTimeStep += ms_recentTimeSteps[i];
		ms_recentTimeSteps[i] = ms_recentTimeSteps[i+1];
	}

	ms_recentTimeSteps[NUM_RECENT_TIME_STEPS-1] = timeStep;

	ms_smoothedTimeStep = accumTimeStep / NUM_RECENT_TIME_STEPS;
}


///////////////////////////////////////////////////////////////////////////////
//  IsPlayerInAccurateModeWithScope
///////////////////////////////////////////////////////////////////////////////

bool			CVfxHelper::IsPlayerInAccurateModeWithScope	()
{

	bool isAccuratelyAiming = false;
	camThirdPersonAimCamera* pAimCamera = camInterface::GetGameplayDirector().GetThirdPersonAimCamera();
	if(pAimCamera)
	{
		isAccuratelyAiming = pAimCamera->IsInAccurateMode();
	}

	if (isAccuratelyAiming == false)
	{
		return false;
	}


	const CPed *pPlayerPed = CGameWorld::FindLocalPlayer();
	if (!pPlayerPed)
	{
		return false;
	}

	const CWeaponInfo* pWeaponInfo = camGameplayDirector::ComputeWeaponInfoForPed(*pPlayerPed);
	if(pWeaponInfo)
	{	
		float extraZoom = pAimCamera->ComputeExtraZoomFactorForAccurateMode(*pWeaponInfo);
		return extraZoom != 1.0f;
	}

	return false;	

}

#if !__SPU
///////////////////////////////////////////////////////////////////////////////
//  GetWaterFogParams
///////////////////////////////////////////////////////////////////////////////

Vec4V_Out			CVfxHelper::GetWaterFogParams				()
{
	return Vec4V(
		Water::GetWaterOpacityUnderwaterVfx(), 
		GetGameCamWaterZ(), 
		g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_WATER_FOGLIGHT),
		Water::GetMainWaterTunings().m_FogPierceIntensity);
}

#endif //
