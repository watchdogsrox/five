//-------------------------------------------------------------------------
// TaskVehicleBase.cpp
// Contains the base vehicle task that the high level FSM vehicle tasks inherit from
//
// Created on: 12/09/08
//
// Author: Phil Hooker
//-------------------------------------------------------------------------

#include "animation/EventTags.h"
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "debug/DebugScene.h"
#include "fwanimation/animdirector.h"
#include "grcore/debugdraw.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "Peds/Ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/Pedplacement.h"
#include "Peds/PedGeometryAnalyser.h"
#include "PedGroup/PedGroup.h"
#include "Peds/PedIntelligence.h"
#include "Scene/World/GameWorld.h"
#include "Streaming/PrioritizedClipSetStreamer.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Vehicles/Vehicle.h"
#include "Vehicles/Planes.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vfx/VehicleGlass/VehicleGlassManager.h"
#include "Task/Vehicle/TaskInVehicle.h"
#include "Task/Vehicle/TaskCar.h"

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

//-------------------------------------------------------------------------
// Baseclass for all vehicle subtasks, implements a basic state system
//-------------------------------------------------------------------------

////////////////////////////////////////////////////////////////////////////////

// Statics
CTaskVehicleFSM::Tunables CTaskVehicleFSM::ms_Tunables;
atHashWithStringNotFinal CTaskVehicleFSM::emptyAmbientContext("EMPTY",0xBBD57BED);

IMPLEMENT_VEHICLE_TASK_TUNABLES(CTaskVehicleFSM, 0x8a7cdf0e);

////////////////////////////////////////////////////////////////////////////////

const float CTaskVehicleFSM::INVALID_GROUND_HEIGHT_FIXUP = -999.0f;
fwMvClipSetVarId CTaskVehicleFSM::ms_CommonVarClipSetId("CommonVarClipSet",0x51E51C);
fwMvClipSetVarId CTaskVehicleFSM::ms_EntryVarClipSetId("EntryVarClipSet",0xF7E35115);
fwMvClipSetVarId CTaskVehicleFSM::ms_InsideVarClipSetId("InsideVarClipSet",0xA8A4ED80);
fwMvClipSetVarId CTaskVehicleFSM::ms_ExitVarClipSetId("ExitVarClipSet",0x3F51AE14);

fwMvClipId	 CTaskVehicleFSM::ms_Clip0Id("Clip0",0xF416C5E4);
fwMvClipId	 CTaskVehicleFSM::ms_Clip1Id("Clip1",0xD60C89C4);
fwMvClipId	 CTaskVehicleFSM::ms_Clip2Id("Clip2",0xB58F45C);
fwMvClipId	 CTaskVehicleFSM::ms_Clip3Id("Clip3",0xB193C0D3);
fwMvClipId	 CTaskVehicleFSM::ms_Clip4Id("Clip4",0xE6CD2B45);

fwMvClipId	 CTaskVehicleFSM::ms_Clip0OutId("Clip0Out",0x39304524);
fwMvClipId	 CTaskVehicleFSM::ms_Clip1OutId("Clip1Out",0x13144B86);
fwMvClipId	 CTaskVehicleFSM::ms_Clip2OutId("Clip2Out",0x1EAE6246);
fwMvClipId	 CTaskVehicleFSM::ms_Clip3OutId("Clip3Out",0x4A48331A);
fwMvClipId	 CTaskVehicleFSM::ms_Clip4OutId("Clip4Out",0xBDFC7890);

fwMvFloatId	 CTaskVehicleFSM::ms_Clip0PhaseId("Clip0Phase",0xCABCBBEE);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip1PhaseId("Clip1Phase",0x9E094ADE);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip2PhaseId("Clip2Phase",0x6A7D05E3);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip3PhaseId("Clip3Phase",0xE496DE9B);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip4PhaseId("Clip4Phase",0x9996B680);

fwMvFloatId	 CTaskVehicleFSM::ms_Clip0BlendWeightId("Clip0BlendWeight",0x54E5FC05);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip1BlendWeightId("Clip1BlendWeight",0xB708FFEB);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip2BlendWeightId("Clip2BlendWeight",0x66D0E3);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip3BlendWeightId("Clip3BlendWeight",0xD0CEEBAB);
fwMvFloatId	 CTaskVehicleFSM::ms_Clip4BlendWeightId("Clip4BlendWeight",0x3C9E8EC9);

const fwMvBooleanId CTaskVehicleFSM::ms_BlockRagdollActivationId("BlockRagdollActivation",0x4BAB759B);
const fwMvBooleanId CTaskVehicleFSM::ms_LegIkBlendOutId("LegIkBlendOut",0xB4E5CADD);
const fwMvBooleanId CTaskVehicleFSM::ms_LegIkBlendInId("LegIkBlendIn",0x965c1051);
const fwMvBooleanId CTaskVehicleFSM::ms_LerpTargetFromClimbUpToSeatId("LerpTargetFromClimbUpToSeat",0xf46de5fb);

float CTaskVehicleFSM::MIN_TIME_BEFORE_PLAYER_CAN_ABORT = 0.5f;

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskVehicleFSM::GetGenericClipId(s32 i)
{
	switch (i)
	{
		case 0: return ms_Clip0Id;
		case 1: return ms_Clip1Id;
		case 2: return ms_Clip2Id;
		case 3: return ms_Clip3Id;
		case 4: return ms_Clip4Id;
		default: taskAssert(0); break;
	}
	return CLIP_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskVehicleFSM::GetGenericClip(const CMoveNetworkHelper& moveNetworkHelper, s32 i)
{
	switch (i)
	{
		case 0: return moveNetworkHelper.GetClip(ms_Clip0OutId);
		case 1: return moveNetworkHelper.GetClip(ms_Clip1OutId);
		case 2: return moveNetworkHelper.GetClip(ms_Clip2OutId);
		case 3: return moveNetworkHelper.GetClip(ms_Clip3OutId);
		case 4: return moveNetworkHelper.GetClip(ms_Clip4OutId);
		default: taskAssert(0);
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskVehicleFSM::GetGenericClipPhaseClamped(const CMoveNetworkHelper& moveNetworkHelper, s32 i)
{
	switch (i)
	{
		case 0: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip0PhaseId), 0.0f, 1.0f);
		case 1: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip1PhaseId), 0.0f, 1.0f);
		case 2: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip2PhaseId), 0.0f, 1.0f);
		case 3: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip3PhaseId), 0.0f, 1.0f);
		case 4: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip4PhaseId), 0.0f, 1.0f);
		default: taskAssert(0);
	}
	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskVehicleFSM::GetGenericBlendWeightClamped(const CMoveNetworkHelper& moveNetworkHelper, s32 i)
{
	switch (i)
	{
		case 0: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip0BlendWeightId), 0.0f, 1.0f);
		case 1: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip1BlendWeightId), 0.0f, 1.0f);
		case 2: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip2BlendWeightId), 0.0f, 1.0f);
		case 3: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip3BlendWeightId), 0.0f, 1.0f);
		case 4: return rage::Clamp(moveNetworkHelper.GetFloat(ms_Clip4BlendWeightId), 0.0f, 1.0f);
		default: taskAssert(0);
	}
	return 0.0f;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::GetGenericClipBlendParams(sClipBlendParams& clipBlendParams, const CMoveNetworkHelper& moveNetworkHelper, u32 uNumClips, bool bZeroPhases)
{
	clipBlendParams.afPhases.clear();
	clipBlendParams.afBlendWeights.clear();
	clipBlendParams.apClips.clear();

	for (s32 i=0; i<uNumClips; ++i)
	{
		if (bZeroPhases)
		{
			clipBlendParams.afPhases.PushAndGrow(0.0f);
		}
		else
		{
			clipBlendParams.afPhases.PushAndGrow(GetGenericClipPhaseClamped(moveNetworkHelper, i));
		}
		const crClip* pClip = GetGenericClip(moveNetworkHelper, i);
		if (!pClip)
		{
			return false;
		}
		clipBlendParams.afBlendWeights.PushAndGrow(GetGenericBlendWeightClamped(moveNetworkHelper, i));
		clipBlendParams.apClips.PushAndGrow(pClip);
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::PreComputeStaticBlendWeightsFromSignal(CTaskVehicleFSM::sClipBlendParams& clipBlendParams, float fBlendSignal)
{
	if (fBlendSignal >= 0.0f && fBlendSignal < 0.25f)
	{
		clipBlendParams.afBlendWeights[0] = 1.0f - fBlendSignal / 0.25f;
		clipBlendParams.afBlendWeights[1] = fBlendSignal / 0.25f;
	}
	else if (fBlendSignal >= 0.25f && fBlendSignal < 0.5f)
	{
		fBlendSignal -= 0.25f;
		clipBlendParams.afBlendWeights[1] = 1.0f - fBlendSignal / 0.25f;
		clipBlendParams.afBlendWeights[2] = fBlendSignal / 0.25f;
	}
	else if (fBlendSignal >= 0.5f && fBlendSignal < 0.75f)
	{
		fBlendSignal -= 0.5f;
		clipBlendParams.afBlendWeights[2] = 1.0f - fBlendSignal / 0.25f;
		clipBlendParams.afBlendWeights[3] = fBlendSignal / 0.25f;
	}
	else if (fBlendSignal >= 0.75f && fBlendSignal <= 1.0f)
	{
		fBlendSignal -= 0.75f;
		clipBlendParams.afBlendWeights[3] = 1.0f - fBlendSignal / 0.25f;
		clipBlendParams.afBlendWeights[4] = fBlendSignal / 0.25f;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ComputePedGlobalTransformFromAttachOffsets(CPed* pPed, Vec3V_Ref vGlobalPos, QuatV_Ref qGlobalOrientation)
{
	taskFatalAssertf(pPed, "NULL ped pointer in CTaskVehicleFSM::UpdatePedVehicleAttachment");

	// Make sure we're attached before doing anything
	if (taskVerifyf(pPed->GetAttachParent(), "Ped wasn't attached in CTaskVehicleFSM::ComputePedGlobalTransformFromAttachOffsets"))
	{
		// Get attachment offsets
		Vec3V vLocalPosition = VECTOR3_TO_VEC3V(pPed->GetAttachOffset());
		QuatV qLocalOrientation = QUATERNION_TO_QUATV(pPed->GetAttachQuat());

		if (ComputeGlobalTransformFromLocal(*pPed, vLocalPosition, qLocalOrientation, vGlobalPos, qGlobalOrientation))
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ComputeGlobalTransformFromLocal(CPed& rPed, Vec3V_ConstRef vLocalPos, QuatV_ConstRef qLocalOrientation, Vec3V_Ref vGlobalPos, QuatV_Ref qGlobalOrientation)
{
	Mat34V parentMtx(V_IDENTITY);

	// Get the global attach transform
	if (rPed.GetGlobalAttachMatrix(parentMtx))
	{
		// Transform pos attach offset to global space
		vGlobalPos = Transform(parentMtx, vLocalPos);

		// Transform rot attach offset to global space
		QuatV qParentRot = QuatVFromMat34V(parentMtx);
		qGlobalOrientation = Multiply(qParentRot, qLocalOrientation);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::UpdatePedVehicleAttachment(CPed* pPed, Vec3V_ConstRef vGlobalPos, QuatV_ConstRef qGlobalOrientation, bool bDontSetTranslation)
{
	taskFatalAssertf(pPed, "NULL ped pointer in CTaskVehicleFSM::UpdatePedVehicleAttachment");

	Vec3V vLocalPosition(V_ZERO);
	QuatV qLocalOrientation(V_IDENTITY);

	if (ComputeLocalTransformFromGlobal(*pPed, vGlobalPos, qGlobalOrientation, vLocalPosition, qLocalOrientation))
	{
		// Set attachment offsets
		if (!bDontSetTranslation)
		{
			pPed->SetAttachOffset(VEC3V_TO_VECTOR3(vLocalPosition));
		}
		pPed->SetAttachQuat(QUATV_TO_QUATERNION(qLocalOrientation));
	}
}

bool CTaskVehicleFSM::ComputeLocalTransformFromGlobal(CPed& rPed, Vec3V_ConstRef vGlobalPos, QuatV_ConstRef qGlobalOrientation, Vec3V_Ref vLocalPos, QuatV_Ref qLocalOrientation)
{
	Mat34V parentMtx(V_IDENTITY);

	// Get the global attach transform
	if (rPed.GetGlobalAttachMatrix(parentMtx))
	{
		// Compute translation of ped relative to attach parent
		vLocalPos = UnTransformFull(parentMtx, vGlobalPos);

		// Compute rotation of ped relative to attach parent
		// q = Inv(q1) * q2 is the rotation that will rotate from the q1 orientation to the q2 orientation
		const QuatV qParentOrientation = Invert(QuatVFromMat34V(parentMtx));
		qLocalOrientation = Multiply(qParentOrientation, qGlobalOrientation);
		return true;
	}

	return false;
}

bool CTaskVehicleFSM::FindGroundPos(const CEntity* pEntity, const CPed *pPed, const Vector3 &vStart, float fTestDepth, Vector3& vIntersection, bool bIgnoreEntity, bool bIgnoreVehicleCheck)
{
	// Search for ground, but tell the probe to ignore the vehicle 
	vIntersection = vStart;
	Vector3 vLosEndPos = vStart - fTestDepth * ZAXIS;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestHitPoint probeIsect;
	WorldProbe::CShapeTestResults probeResult(probeIsect);
	probeDesc.SetStartAndEnd(vStart, vLosEndPos);
	probeDesc.SetResultsStructure(&probeResult);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
	if (bIgnoreEntity)
	{
		const CEntity* entitiesToExclude[2] = {pEntity, pPed};
		probeDesc.SetExcludeEntities(entitiesToExclude, 2, WorldProbe::EIEO_DONT_ADD_VEHICLE_OCCUPANTS);
	}
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		// Don't get popped up onto slow moving vehicles (should ragdoll)?
		const CEntity* pHitEntity = probeResult[0].GetHitEntity();
		if (!bIgnoreVehicleCheck && pHitEntity && pHitEntity->GetIsTypeVehicle() && pHitEntity != pEntity)
		{
			if (static_cast<const CVehicle*>(pHitEntity)->GetVelocity().Mag2() > rage::square(ms_Tunables.m_DisallowGroundProbeVelocity))
			{
				return false;
			}
		}

		vIntersection = probeResult[0].GetHitPosition();
		vIntersection.z += PED_HUMAN_GROUNDTOROOTOFFSET;
		return true;
	}

	return false;
}

// void CTaskVehicleFSM::CalculateAbsoluteMatrixFromClip(Matrix34& mAbsMatrixInOut, const crClip* pClip, const float fClipPhase)
// {
// 	// Compute total local translation for the clip playing at current phase
// 	Vector3 vTotalTranslation = fwAnimDirector::GetMoverTrackDisplacement(pClip, 0.0f, fClipPhase);
// 
// 	// Compute total local rotation for the clip playing at current phase
// 	Quaternion qOrientationDelta = fwAnimDirector::GetMoverTrackOrientationChange(pClip, 0.0f, fClipPhase);
// 
// 	// We're just interested in the heading i.e. the rotation about the z axis
// 	Vector3 vTotalRotation(0.0f,0.0f,0.0f);
// 	qOrientationDelta.ToEulers(vTotalRotation, "xyz");
// 
// 	// Calculate the absolute position and orientation to 'move' to
// 	mAbsMatrixInOut.Transform3x3(vTotalTranslation);
// 	mAbsMatrixInOut.d += vTotalTranslation;
// 	mAbsMatrixInOut.RotateLocalZ(vTotalRotation.z);
// }

#if __ASSERT
// Keep in sync with X:\gta5\script\dev_network\core\common\native\generic.sch
const char* CTaskVehicleFSM::GetScriptVehicleSeatEnumString(s32 iScriptedSeatEnum)
{
	switch (iScriptedSeatEnum)
	{
		case -2:  return "VS_ANY_PASSENGER";
		case -1:  return "VS_DRIVER";
		case  0:  return "VS_FRONT_RIGHT";
		case  1:  return "VS_BACK_LEFT";
		case  2:  return "VS_BACK_RIGHT";
		case  3:  return "VS_EXTRA_LEFT_1";
		case  4:  return "VS_EXTRA_RIGHT_1";
		case  5:  return "VS_EXTRA_LEFT_2";
		case  6:  return "VS_EXTRA_RIGHT_2";
		case  7:  return "VS_EXTRA_LEFT_3";
		case  8:  return "VS_EXTRA_RIGHT_3";
		default: break;
	}
	return "UNKNOWN_SEAT";
}
#endif // __ASSERT

#if __BANK
void CTaskVehicleFSM::SpewStreamingDebugToTTY(const CPed& rPed, const CVehicle& rVeh, s32 iSeatOrEntryIndex, const fwMvClipSetId clipSetId, const float fTimeInState)
{
	bool bRequestedInPrioritizedClipSetMgr = CPrioritizedClipSetRequestManager::GetInstance().HasClipSetBeenRequested(CPrioritizedClipSetRequestManager::RC_Vehicle, clipSetId);
	const CPrioritizedClipSetRequestHelper* pClipSetHelper = CPrioritizedClipSetRequestManager::GetInstance().GetPrioritizedClipSetRequestHelperForClipSet(CPrioritizedClipSetRequestManager::RC_Vehicle, clipSetId);
	const CPrioritizedClipSetRequest* pRequest = CPrioritizedClipSetStreamer::GetInstance().FindRequest(clipSetId);
	eStreamingPriority nPriority = pRequest ? pRequest->GetStreamingPriority() : SP_Invalid;
	aiDebugf1("Frame : %i, Ped %s (%p) waiting for clipset %s to be loaded, vehicle %s, seat or entry index %i", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed, clipSetId.GetCStr(), rVeh.GetDebugName(), iSeatOrEntryIndex);
	aiDebugf1("Frame : %i, Trying to stream clipset: %s (helper requesting %s), (in clipset mgr=%s, has req=%s, pri=%d)", fwTimer::GetFrameCount(), clipSetId.GetCStr(), pClipSetHelper ? pClipSetHelper->GetClipSetId().GetCStr() : "NULL", bRequestedInPrioritizedClipSetMgr ? "true" : "false", pRequest ? "true" : "false", nPriority); 

	if (rPed.IsLocalPlayer())
	{
		TUNE_GROUP_BOOL(VEHICLE_ANIM_STREAMING, ENABLE_UBER_STREAMING_SPEW, true);
		TUNE_GROUP_FLOAT(VEHICLE_ANIM_STREAMING, TIME_IN_STATE_TO_START_DETAILED_SPEW, 3.0f, 0.0f, 10.0f, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_ANIM_STREAMING, TIME_IN_STATE_TO_STOP_DETAILED_SPEW, 6.0f, 0.0f, 10.0f, 0.01f);
		if (ENABLE_UBER_STREAMING_SPEW)
		{
			if (fTimeInState > TIME_IN_STATE_TO_STOP_DETAILED_SPEW)
			{
				CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests = false;
			}
			else if (fTimeInState > TIME_IN_STATE_TO_START_DETAILED_SPEW)
			{
				CPrioritizedClipSetRequestManager::ms_bSpewDetailedRequests = true;
			}
		}
	}
}
#endif // __BANK

bool CTaskVehicleFSM::CheckIfPedWentThroughSomething(CPed& rPed, const CVehicle& rVeh)
{
	const s32 iSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(&rPed);
	if (rVeh.IsSeatIndexValid(iSeatIndex))
	{
		const s32 iEntryIndex = rVeh.GetDirectEntryPointIndexForSeat(iSeatIndex);
		if (rVeh.IsEntryIndexValid(iEntryIndex))
		{
			Vector3 vSeatPosition(Vector3::ZeroType);
			Quaternion qSeatOrientation(Quaternion::IdentityType);
			CModelSeatInfo::CalculateSeatSituation(&rVeh, vSeatPosition, qSeatOrientation, iEntryIndex, iSeatIndex);

			const CEntity* exclusionList[2];
			int nExclusions = 0;
			exclusionList[nExclusions++] = &rPed;
			exclusionList[nExclusions++] = &rVeh;

			Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());

			// Extend the test a bit because the mover may just be outside the collision, but switching to ragdoll may push us through the collision
			TUNE_GROUP_BOOL(EXIT_VEHICLE_TUNE, PRE_EMPTIVE_RAGDOLL_FOR_ALL, false);
			const bool bUsePrediction = PRE_EMPTIVE_RAGDOLL_FOR_ALL || rVeh.InheritsFromQuadBike() || rVeh.InheritsFromBike() || rVeh.InheritsFromAmphibiousQuadBike();
			Vector3 vToPedPos;
			float fScale;
			if (bUsePrediction)
			{
				vToPedPos = vPedPosition - vSeatPosition;
				TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MIN_FLAT_MAG, 0.15f, 0.0f, 5.0f, 0.01f);
				if (vToPedPos.XYMag2() > square(MIN_FLAT_MAG))
				{
					vToPedPos.Normalize();
					TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MIN_EXTRA_DIST, 0.5f, 0.0f, 5.0f, 0.01f);
					TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MAX_EXTRA_DIST, 2.0f, 0.0f, 5.0f, 0.01f);
					TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MAX_VELOCITY, 5.0f, 0.0f, 50.0f, 0.01f);
					const float fVehVelSqd = rVeh.GetVelocity().Mag2();
					const float fRatio = Clamp(fVehVelSqd / square(MAX_VELOCITY), 0.0f, 1.0f);
					fScale = (1.0f - fRatio) * MIN_EXTRA_DIST + fRatio * MAX_EXTRA_DIST;
					Vector3 vScale = vToPedPos;
					vScale.Scale(fScale);
					vPedPosition += vScale;
					TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, VEH_SPEED_SCALE, 1.0f, 0.0f, 5.0f, 0.01f);
					Vector3 vLocalVehSpeed = rVeh.GetLocalSpeed(Vector3(0.0f,0.0f,0.0f));
					vPedPosition += vLocalVehSpeed * fwTimer::GetTimeStep() * VEH_SPEED_SCALE;
				}
			}

			WorldProbe::CShapeTestFixedResults<> probeResult;
			s32 iTypeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetExcludeEntities(exclusionList, nExclusions);
			probeDesc.SetResultsStructure(&probeResult);
			probeDesc.SetStartAndEnd(vSeatPosition, vPedPosition);
			probeDesc.SetIncludeFlags(iTypeFlags);
			probeDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
			probeDesc.SetIsDirected(true);
			WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc);
			if (probeResult[0].GetHitDetected())
			{
#if __DEV
				CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(probeResult[0].GetHitPosition()), 0.025f, Color_red, 10000);
				CTask::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vSeatPosition), VECTOR3_TO_VEC3V(probeResult[0].GetHitPosition()), Color_red, 10000);
				CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(vPedPosition), 0.025f, Color_blue, 10000);
				aiDisplayf("Frame : %i, Ped %s(%p) CTaskVehicleFSM::CheckIfPedWentThroughSomething returning true", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed);
#endif // __DEV
				return true;
			}
#if __DEV
			aiDisplayf("Frame : %i, Ped %s(%p) CTaskVehicleFSM::CheckIfPedWentThroughSomething returning false", fwTimer::GetFrameCount(), rPed.GetModelName(), &rPed);
			CTask::ms_debugDraw.AddLine(VECTOR3_TO_VEC3V(vSeatPosition), VECTOR3_TO_VEC3V(vPedPosition), Color_green, 10000);
			CTask::ms_debugDraw.AddSphere(VECTOR3_TO_VEC3V(vPedPosition), 0.025f, Color_blue, 10000);
#endif // __DEV
		}
	}

	return false;
}

bool CTaskVehicleFSM::PedShouldWarpBecauseVehicleAtAwkwardAngle(const CVehicle& rVeh)
{
	TUNE_GROUP_BOOL(VEHICLE_EXIT_TUNE, USE_BAD_UPSIDE_DOWN_ANGLE_WARP, true);
	if (USE_BAD_UPSIDE_DOWN_ANGLE_WARP)
	{
		TUNE_GROUP_FLOAT(VEHICLE_EXIT_TUNE, BAD_ANGLE, 2.25f, 0.0f, 3.14f, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_EXIT_TUNE, BAD_ANGLE_LOWER_RANGE, 0.15f, 0.0f, 3.14f, 0.01f);
		TUNE_GROUP_FLOAT(VEHICLE_EXIT_TUNE, BAD_ANGLE_UPPER_RANGE, 0.5f, 0.0f, 3.14f, 0.01f);
		const float fRoll = Abs(rVeh.GetTransform().GetRoll());
		const float fLwrBnd = BAD_ANGLE - BAD_ANGLE_LOWER_RANGE;
		const float fUprBnd = BAD_ANGLE + BAD_ANGLE_UPPER_RANGE;
		if (fRoll > fLwrBnd && fRoll < fUprBnd)
		{
			return true;
		}
	}
	return false;
}

bool CTaskVehicleFSM::PedShouldWarpBecauseHangingPedInTheWay(const CVehicle& rVeh, s32 iEntryIndex)
{
	// Bit hacky hard coding all this, but then again, this solution is hacky
	TUNE_GROUP_BOOL(VEHICLE_ENTRY_TUNE, TEST_RANGER_WARP_IN_IN_SP, false);
	if (NetworkInterface::IsGameInProgress() || TEST_RANGER_WARP_IN_IN_SP)
	{
		static u32 RANGER_LAYOUT_HASH = atStringHash("LAYOUT_RANGER_SWAT");
		static u32 POLGRANGER_LAYOUT_HASH = atStringHash("LAYOUT_RANGER_POLGRANGER");
		static u32 ROOSEVELT_LAYOUT_HASH = atStringHash("LAYOUT_VAN_ROOSEVELT");
		static u32 ROOSEVELT2_LAYOUT_HASH = atStringHash("LAYOUT_VAN_ROOSEVELT2");
		static u32 LAYOUT_VAN_ARMORED_HASH = atStringHash("LAYOUT_VAN_ARMORED");
		const bool bIsRangerLayout = rVeh.GetLayoutInfo()->GetName() == RANGER_LAYOUT_HASH || rVeh.GetLayoutInfo()->GetName() == POLGRANGER_LAYOUT_HASH;
		const bool bIsRooseveltLayout = rVeh.GetLayoutInfo()->GetName() == ROOSEVELT_LAYOUT_HASH || rVeh.GetLayoutInfo()->GetName() == ROOSEVELT2_LAYOUT_HASH;
		const bool bIsArmoredVanLayout = rVeh.GetLayoutInfo()->GetName() == LAYOUT_VAN_ARMORED_HASH;
		if ((bIsRangerLayout || bIsRooseveltLayout || bIsArmoredVanLayout) && rVeh.IsEntryIndexValid(iEntryIndex))
		{
			static u32 iMaxInVehicleSeats = 4;
			static u32 iNumExtraSeatsRanger = 4;
			static u32 iNumExtraSeatsRoosevelt = 2;
			const s32 iSeatConnectedIndex = rVeh.GetEntryExitPoint(iEntryIndex)->GetSeat(SA_directAccessSeat);
			bool bShouldEvaluateSeat = iSeatConnectedIndex < iMaxInVehicleSeats;
			if (bIsRooseveltLayout)
			{
				if (iSeatConnectedIndex == 2 || iSeatConnectedIndex == 3)
				{
					bShouldEvaluateSeat = true;
				}
				else 
				{
					bShouldEvaluateSeat = false;
				}
			}
			if (bShouldEvaluateSeat)
			{
				const u32 iNumExtraSeats = bIsRangerLayout ? iNumExtraSeatsRanger : iNumExtraSeatsRoosevelt;
				// One side entry seat per in vehicle seat
				const s32 iBlockingSeat = iSeatConnectedIndex + iNumExtraSeats;
				if (rVeh.IsSeatIndexValid(iBlockingSeat))
				{
					const CVehicleSeatAnimInfo* pSeatAnimInfo = rVeh.GetSeatAnimationInfo(iBlockingSeat);
					if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
					{					
						if (CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, iBlockingSeat))
						{
							BANK_ONLY(aiDisplayf("Frame : %u - Ped warping out of vehicle %s - PedShouldWarpBecauseHangingPedInTheWay returned true", fwTimer::GetFrameCount(), rVeh.GetDebugName());)
							return true;
						}
					}
				}
				// Rear seats are also obstructed by front hanging peds
				if (!bIsRooseveltLayout && (iSeatConnectedIndex == 2 || iSeatConnectedIndex == 3))
				{
					const s32 iFrontBlockingSeat = iSeatConnectedIndex -2 + iMaxInVehicleSeats;
					if (rVeh.IsSeatIndexValid(iFrontBlockingSeat))
					{
						const CVehicleSeatAnimInfo* pSeatAnimInfo = rVeh.GetSeatAnimationInfo(iFrontBlockingSeat);
						if (pSeatAnimInfo && pSeatAnimInfo->GetKeepCollisionOnWhenInVehicle())
						{
							const bool bPedBlocking = CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, iFrontBlockingSeat) ? true : false;
#if __BANK
							if (bPedBlocking)
								aiDisplayf("Frame : %u - Ped warping out of vehicle %s - PedShouldWarpBecauseHangingPedInTheWay returned true", fwTimer::GetFrameCount(), rVeh.GetDebugName());
#endif //__BANK
							return bPedBlocking;
						}
					}
				}
			}
		}
	}
	return false;
}

float CTaskVehicleFSM::GetEntryRadiusMultiplier(const CEntity& rEnt)
{
	if (rEnt.GetIsTypeVehicle())
	{
		if (static_cast<const CVehicle&>(rEnt).InheritsFromBike())
		{
			return ms_Tunables.m_BikeEntryCapsuleRadiusScale;
		}
		else
		{
			return ms_Tunables.m_VehEntryCapsuleRadiusScale;
		}
	}
	return 1.0f;
}

bool CTaskVehicleFSM::ProcessBlockRagdollActivation(CPed& rPed, CMoveNetworkHelper& rMoveNetworkHelper)
{
	if (rMoveNetworkHelper.IsNetworkActive())
	{
		bool bRagdollActivationBlocked = rMoveNetworkHelper.GetBoolean(CTaskVehicleFSM::ms_BlockRagdollActivationId);

		if (!bRagdollActivationBlocked)
		{
			const CClipEventTags::CBlockRagdollTag* pProp = static_cast<const CClipEventTags::CBlockRagdollTag*>(rMoveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::BlockRagdoll));
			if (pProp)
			{
				bRagdollActivationBlocked = pProp->ShouldBlockAll();
			}
		}

		if (bRagdollActivationBlocked)
		{
			rPed.SetPedResetFlag(CPED_RESET_FLAG_BlockRagdollActivationInVehicle, true);
			return true;
		}
	}
	return false;
}

bool CTaskVehicleFSM::IsPedHeadingToLeftSideOfVehicle(const CPed& rPed, const CVehicle& rVeh)
{
	TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, WRONG_SIDE_MIN_CHECK_VELOCITY, 0.5f, 0.0f, 2.0f, 0.01f);
	if (rPed.GetVelocity().Mag2() > square(WRONG_SIDE_MIN_CHECK_VELOCITY))
	{
		TUNE_GROUP_FLOAT(VEHICLE_ENTRY_TUNE, AHEAD_CHECK_DISTANCE, 0.5f, 0.0f, 2.0f, 0.01f);
		const Vector3 vPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition()) + VEC3V_TO_VECTOR3(rPed.GetTransform().GetB()) * AHEAD_CHECK_DISTANCE;
		const Vector3 vToPedPos = vPos - VEC3V_TO_VECTOR3(rVeh.GetTransform().GetPosition());
		const Vector3 vVehFwd = VEC3V_TO_VECTOR3(rVeh.GetTransform().GetB());
		return vToPedPos.CrossZ(vVehFwd) > 0.0f ? false : true;
	}
	else
	{
		return IsPedOnLeftSideOfVehicle(rPed, rVeh);
	}
}

bool CTaskVehicleFSM::IsPedOnLeftSideOfVehicle(const CPed& rPed, const CVehicle& rVeh)
{
	const Vector3 vToPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition() - rVeh.GetTransform().GetPosition());
	const Vector3 vVehFwd = VEC3V_TO_VECTOR3(rVeh.GetTransform().GetB());
	return vToPedPos.CrossZ(vVehFwd) > 0.0f ? false : true;
}

bool CTaskVehicleFSM::CanPedWarpIntoVehicle(const CPed& rPed, const CVehicle& rVeh)
{
	const CVehicleLayoutInfo* pLayoutInfo = rVeh.GetLayoutInfo();
	if (pLayoutInfo && (pLayoutInfo->GetWarpInAndOut() || (pLayoutInfo->GetWarpInWhenStoodOnTop() && rPed.GetGroundPhysical() == &rVeh)))
	{
		return true;
	}

	return false;
}

void CTaskVehicleFSM::ProcessAutoCloseDoor(const CPed& rPed, CVehicle& rVeh, s32 iSeatIndex)
{
	const s32 iEntryPointIndex = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iSeatIndex, &rVeh);
	const CVehicleEntryPointInfo* pEntryInfo =  rVeh.IsEntryIndexValid(iEntryPointIndex) ? rVeh.GetEntryInfo(iEntryPointIndex) : NULL;
	const s32 iPedsSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(&rPed);
	const bool bShouldAutoCloseThisDoor = rVeh.IsSeatIndexValid(iPedsSeatIndex) && iPedsSeatIndex == iSeatIndex && pEntryInfo && pEntryInfo->GetAutoCloseThisDoor();
	const bool bHasStairs = rVeh.GetLayoutInfo()->GetClimbUpAfterOpenDoor() || (pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry());

	const bool bInAutoCloseVehicle = rVeh.GetLayoutInfo()->GetAutomaticCloseDoor();

	if(!rPed.IsInFirstPersonVehicleCamera() || bInAutoCloseVehicle || bHasStairs )
	{
		if (!bShouldAutoCloseThisDoor)
		{
			if (pEntryInfo && !bInAutoCloseVehicle && !bHasStairs)
				return;

			if (bHasStairs && (rVeh.IsNetworkClone() || (!rVeh.IsInAir() && rVeh.GetVelocity().Mag2() < CTaskMotionInVehicle::ms_Tunables.m_MaxVehVelocityToKeepStairsDown)))
				return;
		}
	}
	else
	{
		if(!CTaskMotionInVehicle::CheckForClosingDoor(rVeh, rPed, false, iSeatIndex))
			return;
	}

	if (rPed.GetPedResetFlag(CPED_RESET_FLAG_IsExitingVehicle))
		return;
		
	if (!rVeh.IsSeatIndexValid(iSeatIndex))
		return;

	if (!rVeh.IsEntryIndexValid(iEntryPointIndex))
		return;

	const CComponentReservation* pComponentReservation = rVeh.GetComponentReservationMgr()->GetDoorReservation(iEntryPointIndex);
	if (pComponentReservation && !pComponentReservation->GetPedUsingComponent())
	{
		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&rVeh, iEntryPointIndex);
		if (pDoor && pDoor->GetIsIntact(&rVeh) && !pDoor->GetFlag(CCarDoor::DRIVEN_NORESET) && !pDoor->GetFlag(CCarDoor::DRIVEN_NORESET_NETWORK))
		{
			pDoor->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
		}

		CCarDoor* pDoor2 = rVeh.GetSecondDoorFromEntryPointIndex(iEntryPointIndex);
		if (pDoor2 && pDoor2->GetIsIntact(&rVeh) && !pDoor2->GetFlag(CCarDoor::DRIVEN_NORESET) && !pDoor2->GetFlag(CCarDoor::DRIVEN_NORESET_NETWORK))
		{
			pDoor2->SetTargetDoorOpenRatio(0.0f, CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN);
		}
	}
}

void CTaskVehicleFSM::ProcessLegIkBlendOut(CPed& rPed, float fBlendOutPhase, float fCurrentPhase)
{
	// Keep the leg ik on if we have a blend out tag, blend the ik out
	if (fBlendOutPhase > 0.0f && fCurrentPhase < fBlendOutPhase)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
		rPed.GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);
	}
}

void CTaskVehicleFSM::ProcessLegIkBlendIn(CPed& rPed, float fBlendInPhase, float fCurrentPhase)
{
	// Keep the leg ik off if we don't have a blend in tag
	if (fBlendInPhase >= 0.0f && fCurrentPhase >= fBlendInPhase)
	{
		rPed.SetPedResetFlag(CPED_RESET_FLAG_RequiresLegIk, true);
		rPed.GetIkManager().SetFlag(PEDIK_LEGS_AND_PELVIS_FADE_ON);
	}
}

bool CTaskVehicleFSM::ShouldClipSetBeStreamedOut(fwMvClipSetId clipSetId)
{
	if (clipSetId != CLIP_SET_ID_INVALID)
	{
		return CPrioritizedClipSetRequestManager::GetInstance().ShouldClipSetBeStreamedOut(CPrioritizedClipSetRequestManager::RC_Vehicle, clipSetId);
	}
	return true;
}

void CTaskVehicleFSM::SetScriptedVehicleEntryExitFlags(VehicleEnterExitFlags& vehicleFlags, s32 scriptedFlags)
{	
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::ResumeIfInterupted, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::WarpToEntryPosition, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::JackIfOccupied, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::Warp, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::DontWaitForCarToStop, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::DontCloseDoor, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::WarpIfDoorBlocked, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::JumpOut, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::DontDefaultWarpIfDoorBlocked, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::PreferLeftEntry, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::PreferRightEntry, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::JustPullPedOut, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::UseDirectEntryOnly, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::WarpIfShuffleLinkIsBlocked, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::DontJackAnyone, scriptedFlags);
	SetBitSetFlagIfSet(vehicleFlags, CVehicleEnterExitFlags::WaitForEntryToBeClearOfPeds, scriptedFlags);
	vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::ScriptedTask);
}

////////////////////////////////////////////////////////////////////////////////

CPed* CTaskVehicleFSM::GetPedInOrUsingSeat(const CVehicle& rVeh, s32 iSeatIndex, const CPed* pPedToExclude)
{
	if (rVeh.IsSeatIndexValid(iSeatIndex))
	{
		CPed* pPedUsingSeat = rVeh.GetSeatManager()->GetPedInSeat(iSeatIndex);
		if (!pPedUsingSeat)
		{
			CComponentReservation* pComponentReservation = const_cast<CVehicle&>(rVeh).GetComponentReservationMgr()->GetSeatReservationFromSeat(iSeatIndex);
			if (pComponentReservation && pComponentReservation->GetPedUsingComponent())
			{
				pPedUsingSeat = pComponentReservation->GetPedUsingComponent();
			}
		}

		if (pPedUsingSeat == pPedToExclude)
		{
			return NULL;
		}
		return pPedUsingSeat;
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::IsOpenSeatIsBlocked(const CVehicle& rVeh, s32 iSeatIndex)
{
	if (rVeh.InheritsFromBike() || rVeh.InheritsFromQuadBike() || rVeh.GetIsJetSki() || rVeh.InheritsFromAmphibiousQuadBike())
	{
		if (rVeh.IsSeatIndexValid(iSeatIndex))
		{
			s32 iBoneIndex = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeatIndex);

			if (taskVerifyf(iBoneIndex!=-1, "CheckIfOpenSeatIsBlocked - Bone does not exist"))
			{
				Matrix34 seatMtx(Matrix34::IdentityType);
				rVeh.GetGlobalMtx(iBoneIndex, seatMtx);

				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_CAPSULE_RADIUS, 0.15f, 0.0f, 2.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_CAPSULE_TEST_HEIGHT, 0.45f, 0.0f, 2.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_CAPSULE_TEST_HEIGHT_ONSIDE, 0.85f, 0.0f, 2.0f, 0.01f);
				TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_CAPSULE_TEST_START_HEIGHT_ONSIDE, 0.15f, 0.0f, 2.0f, 0.01f);
				CVehicle::eVehicleOrientation vo = CVehicle::GetVehicleOrientation(rVeh);
				const bool bUpright = vo == CVehicle::VO_Upright;
				const float fTestStartHeight = bUpright ? 0.0f : OPEN_SEAT_CAPSULE_TEST_START_HEIGHT_ONSIDE;
				const float fTestHeight = bUpright ? OPEN_SEAT_CAPSULE_TEST_HEIGHT : OPEN_SEAT_CAPSULE_TEST_HEIGHT_ONSIDE;
				Vector3 vStart = seatMtx.d + fTestStartHeight * ZAXIS;
				Vector3 vEnd = seatMtx.d  + fTestHeight * ZAXIS;

				// Bicycles are thin so we need to offset the capsule test when its leant against a wall
				if (rVeh.InheritsFromBicycle())
				{
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_MIN_ROLL_TO_APPLY_SIDE_OFFSET, 0.0f, 0.0f, 2.0f, 0.01f);
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_MAX_ROLL_TO_APPLY_SIDE_OFFSET, 1.0f, 0.0f, 2.0f, 0.01f);
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_MAX_SIDE_OFFSET_WHEN_ROLLED, 0.25f, -2.0f, 2.0f, 0.01f);
					const float fRoll = rVeh.GetTransform().GetRoll();
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, MULTTY, 1.0f, -1.0f, 1.0f, 1.0f);
					const float fSign = MULTTY * Sign(fRoll);
					const float fRollRatio = Clamp((Abs(fRoll) - OPEN_SEAT_MIN_ROLL_TO_APPLY_SIDE_OFFSET) / (OPEN_SEAT_MAX_SIDE_OFFSET_WHEN_ROLLED - OPEN_SEAT_MIN_ROLL_TO_APPLY_SIDE_OFFSET), 0.0f, 1.0f);
					const float fSideOffset = Lerp(fRollRatio, 0.0f, OPEN_SEAT_MAX_SIDE_OFFSET_WHEN_ROLLED);
					vStart += fSign * fSideOffset * seatMtx.a;
					vEnd += fSign * fSideOffset * seatMtx.a;
				}

				// If the bike is on it's side, pull the capsule test down towards the wheel in case the seat is up against a wall
				if (vo == CVehicle::VO_OnSide)
				{
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, OPEN_SEAT_SIDE_PULL_DOWN, 0.25f, 0.0f, 2.0f, 0.01f);
					vStart -= seatMtx.c * OPEN_SEAT_SIDE_PULL_DOWN;
					vEnd -= seatMtx.c * OPEN_SEAT_SIDE_PULL_DOWN;
				}

				WorldProbe::CShapeTestFixedResults<> capsuleResult;
				s32 iTypeFlags = ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
				WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
				capsuleDesc.SetResultsStructure(&capsuleResult);
				capsuleDesc.SetCapsule(vStart, vEnd, OPEN_SEAT_CAPSULE_RADIUS);
				capsuleDesc.SetIncludeFlags(iTypeFlags);
				capsuleDesc.SetTypeFlags(ArchetypeFlags::GTA_AI_TEST);
				capsuleDesc.SetIsDirected(false);
				capsuleDesc.SetDoInitialSphereCheck(true);
				capsuleDesc.SetExcludeEntity(&rVeh);

				bool bHitDetected = false;
				if (WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
				{
					// Go over all valid hits, accept the first valid hit that isn't a dynamic object
					for(WorldProbe::ResultIterator it = capsuleResult.begin(); it < capsuleResult.last_result(); ++it)
					{
						if(it->GetHitDetected()) 
						{
							const CEntity* pHitEntity = it->GetHitEntity();
							const bool bIsDynamicObject = pHitEntity && pHitEntity->GetIsTypeObject() && pHitEntity->GetIsDynamic();
							if (!bIsDynamicObject)
							{
								bHitDetected = true;
								break;
							}
						}
					}
				}

				if (bHitDetected)
				{
#if DEBUG_DRAW
					CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), OPEN_SEAT_CAPSULE_RADIUS, Color_red, 2500, 0, false);
#endif // DEBUG_DRAW
					return true;
				}

#if DEBUG_DRAW
				CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), OPEN_SEAT_CAPSULE_RADIUS, Color_green, 2500, 0, false);
#endif // DEBUG_DRAW
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ShouldWarpInAndOutInMP(const CVehicle& rVeh, s32 iEntryPointIndex)
{
	if ((NetworkInterface::IsGameInProgress() || ms_Tunables.m_AllowEntryToMPWarpInSeats) && rVeh.IsEntryIndexValid(iEntryPointIndex))
	{
		bool bMPWarpInOut		= rVeh.GetEntryInfo(iEntryPointIndex)->GetMPWarpInOut();
		bool bSPEntryAllowed	= rVeh.GetEntryInfo(iEntryPointIndex)->GetSPEntryAllowedAlso();
		bool bUseFullAnims		= rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_USE_FULL_ANIMS_FOR_MP_WARP_ENTRY_POINTS);

		if (bMPWarpInOut && bSPEntryAllowed && bUseFullAnims)
		{
			return false;
		}

#if __BANK
		if (bMPWarpInOut)
			aiDisplayf("Frame : %u - Ped potentially warping in/out of vehicle %s due to ShouldWarpInAndOutInMP (Entry %i, MPWarp %s, SPEntry %s, UseFullAnims %s)", fwTimer::GetFrameCount(), rVeh.GetDebugName(), iEntryPointIndex, bMPWarpInOut ? "TRUE" : "FALSE", bSPEntryAllowed ? "TRUE" : "FALSE", bUseFullAnims ? "TRUE" : "FALSE");
#endif //__BANK

		return bMPWarpInOut;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskVehicleFSM::GetExitToAimClipSetIdForPed(const CPed& ped)
{
	// Find out which clip set to request based on our equipped weapon or best weapon and then request it
	if(ped.GetVehiclePedInside() && ped.GetWeaponManager())
	{
		u32 uWeaponHash = ped.GetWeaponManager()->GetWeaponSelector()->GetBestPedWeapon(&ped, CWeaponInfoManager::GetSlotListBest(), true);
		if( uWeaponHash != 0 )
		{
			if(ped.GetInventory()->GetWeapon(uWeaponHash))
			{
				const CWeaponInfo* pBestWeaponInfo = ped.GetInventory()->GetWeapon(uWeaponHash)->GetInfo();
				if(pBestWeaponInfo && pBestWeaponInfo->GetIsGun())
				{
					int iSeatIndex = ped.GetMyVehicle()->GetSeatManager()->GetPedsSeatIndex(&ped);
					const CTaskExitVehicleSeat::ExitToAimSeatInfo* pExitToAimSeatInfo = CTaskExitVehicleSeat::GetExitToAimSeatInfo(ped.GetMyVehicle(), iSeatIndex, iSeatIndex);
					if(pExitToAimSeatInfo)
					{
						// Get our clip set and verify it exists and is streamed
						atHashString clipSetHash = pBestWeaponInfo->GetIsTwoHanded() ? pExitToAimSeatInfo->m_TwoHandedClipSetName : pExitToAimSeatInfo->m_OneHandedClipSetName;
						if(clipSetHash.IsNotNull())
						{
							return fwMvClipSetId(clipSetHash);
						}
					}
				}
			}
		}
	}
	return CLIP_SET_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::SetDoorDrivingFlags(CVehicle* pVehicle, s32 iEntryPointIndex)
{
	if (pVehicle && pVehicle->IsEntryIndexValid(iEntryPointIndex))
	{
		bool settingFlagOnDoor = false;

		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iEntryPointIndex);
		if (pDoor)
		{
			pDoor->SetFlag(CCarDoor::PED_DRIVING_DOOR);
			settingFlagOnDoor = true;
		}

		CCarDoor* pDoor2 = pVehicle->GetSecondDoorFromEntryPointIndex(iEntryPointIndex);
		if (pDoor2)
		{
			pDoor2->SetFlag(CCarDoor::PED_DRIVING_DOOR);
			settingFlagOnDoor = true;
		}

		if(settingFlagOnDoor)
		{
			// If we get here, we know that at least one of the doors must have PED_DRIVING_DOOR set.
			pVehicle->m_nVehicleFlags.bMayHavePedDrivingDoorFlagSet = true;
		}
	}
}

void CTaskVehicleFSM::ClearDoorDrivingFlags(CVehicle* pVehicle, s32 iEntryPointIndex)
{
	if (pVehicle && !pVehicle->IsNetworkClone() && pVehicle->IsEntryIndexValid(iEntryPointIndex))
	{
		bool clearingFlagOnDoor = false;

		CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVehicle, iEntryPointIndex);
		if (pDoor)
		{
			pDoor->ClearFlag(CCarDoor::PED_DRIVING_DOOR);
			pDoor->ClearFlag(CCarDoor::DRIVEN_SPECIAL);
			clearingFlagOnDoor = true;
		}

		CCarDoor* pDoor2 = pVehicle->GetSecondDoorFromEntryPointIndex(iEntryPointIndex);
		if (pDoor2)
		{
			pDoor2->ClearFlag(CCarDoor::PED_DRIVING_DOOR);
			pDoor->ClearFlag(CCarDoor::DRIVEN_SPECIAL);
			clearingFlagOnDoor = true;
		}

		if(clearingFlagOnDoor)
		{
			// If we cleared the flag off from one of the doors, we may or may not still
			// have other doors with PED_DRIVING_DOOR set, so we probably need to check
			// for that rather than always setting bMayHavePedDrivingDoorFlagSet to false.
			const int numDoors = pVehicle->GetNumDoors();
			bool flagSet = false;
			for(int i = 0; i < numDoors; i++)
			{
				CCarDoor* pDoor = pVehicle->GetDoor(i);
				if(pDoor->GetFlag(CCarDoor::PED_DRIVING_DOOR))
				{
					flagSet = true;
					break;
				}
			}
			pVehicle->m_nVehicleFlags.bMayHavePedDrivingDoorFlagSet = flagSet;
		}
	}
}

bool CTaskVehicleFSM::CheckForEnterExitVehicle(CPed& ped)
{
	if (ped.IsLocalPlayer())
	{
		const CControl* pControl = ped.GetControlFromPlayer();
		if (pControl)
		{
			return pControl->GetPedEnter().IsPressed();
		}
	}
	return false;
}

bool CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(const CVehicle& vehicle, Vector3& vTargetPos, Quaternion& qTargetOrientation, s32 iEntryPointIndex, CExtraVehiclePoint::ePointType pointType)
{
	if (vehicle.IsEntryIndexValid(iEntryPointIndex))
	{
		const CVehicleEntryPointInfo* pEntryPointInfo = vehicle.GetEntryInfo(iEntryPointIndex);
		taskAssert(pEntryPointInfo);

		const CVehicleExtraPointsInfo* pExtraVehiclePointsInfo = pEntryPointInfo->GetVehicleExtraPointsInfo();

		if (pExtraVehiclePointsInfo)
		{
			const CExtraVehiclePoint* pExtraVehiclePoint = pExtraVehiclePointsInfo->GetExtraVehiclePointOfType(pointType);
			if (pExtraVehiclePoint)
			{
				vTargetPos = pExtraVehiclePoint->GetPosition();
				qTargetOrientation.FromEulers(Vector3(pExtraVehiclePoint->GetPitch(),0.0f,pExtraVehiclePoint->GetHeading()));
				
				if (pExtraVehiclePoint->GetLocationType() == CExtraVehiclePoint::SEAT_RELATIVE || pExtraVehiclePoint->GetLocationType() == CExtraVehiclePoint::DRIVER_SEAT_RELATIVE)
				{
					const CModelSeatInfo* pModelSeatinfo = vehicle.GetVehicleModelInfo()->GetModelSeatInfo();
					taskAssert(pModelSeatinfo);

					s32 iSeatToUse = 0;
					if (pExtraVehiclePoint->GetLocationType() == CExtraVehiclePoint::SEAT_RELATIVE)
					{
						const CEntryExitPoint* pEntryExitPoint = pModelSeatinfo->GetEntryExitPoint(iEntryPointIndex);
						taskAssertf(pEntryExitPoint, "No Entry point for index %u", iEntryPointIndex);
						iSeatToUse = pEntryExitPoint->GetSeat(SA_directAccessSeat);
					}
					else
					{
						iEntryPointIndex = vehicle.GetDirectEntryPointIndexForSeat(iSeatToUse);
					}

					s32 iBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(iSeatToUse);
					Matrix34 seatMat;
					vehicle.GetGlobalMtx(iBoneIndex, seatMat);
					seatMat.Transform3x3(vTargetPos);

					Vector3 vSeatPos(Vector3::ZeroType);
					Quaternion qSeatOrientation(0.0f,0.0f,0.0f,1.0f);

					CModelSeatInfo::CalculateSeatSituation(&vehicle, vSeatPos, qSeatOrientation, iEntryPointIndex);
					vTargetPos += vSeatPos;
					qTargetOrientation.MultiplyFromLeft(qSeatOrientation);
					return true;
				}
				else if (pExtraVehiclePoint->GetLocationType() == CExtraVehiclePoint::WHEEL_GROUND_RELATIVE)
				{
					const crSkeletonData* pSkelData = vehicle.GetVehicleModelInfo()->GetFragType()->GetCommonDrawable()->GetSkeletonData();
					if (pSkelData && vehicle.GetWheel(0))	// Ensure we have at least one wheel
					{
						const crBoneData* pBoneData = pSkelData->FindBoneData(pExtraVehiclePoint->GetName());
						if (pBoneData)
						{
							TUNE_GROUP_FLOAT(VEHICLE_DEBUG, SPHERE_RADIUS, 0.05f, 0.0f, 1.0f, 0.01f);
							const CModelSeatInfo* pModelSeatinfo = vehicle.GetVehicleModelInfo()->GetModelSeatInfo();
							taskAssert(pModelSeatinfo);
							const CEntryExitPoint* pEntryExitPoint = pModelSeatinfo->GetEntryExitPoint(iEntryPointIndex);
							taskAssertf(pEntryExitPoint, "No Entry point for index %u", iEntryPointIndex);

							// Render Seat Position
							const s32 iSeatBoneIndex = pModelSeatinfo->GetBoneIndexFromSeat(pEntryExitPoint->GetSeat(SA_directAccessSeat));
							Matrix34 mGlobalSeatMat(Matrix34::IdentityType);
							vehicle.GetGlobalMtx(iSeatBoneIndex, mGlobalSeatMat);
#if DEBUG_DRAW
							CTask::ms_debugDraw.AddSphere(RCC_VEC3V(mGlobalSeatMat.d), SPHERE_RADIUS, Color_green, 1000);
#endif // DEBUG_DRAW
						
							const s32 iWheelBoneIndex = (s16)pBoneData->GetIndex();
#if DEBUG_DRAW
							// Render Wheel Position
							Matrix34 mGlobalWheelMat(Matrix34::IdentityType);
							vehicle.GetGlobalMtx(iWheelBoneIndex, mGlobalWheelMat);
							CTask::ms_debugDraw.AddSphere(RCC_VEC3V(mGlobalWheelMat.d), SPHERE_RADIUS, Color_orange, 1000);
#endif // DEBUG_DRAW
							
							// Get the wheel position relative to the seat
							Matrix34 wheelMtx = vehicle.GetObjectMtx(iWheelBoneIndex);
							Matrix34 seatObjMtx = vehicle.GetObjectMtx(iSeatBoneIndex);
							Vector3 vWheelPos = wheelMtx.d;
							seatObjMtx.UnTransform(vWheelPos);

#if DEBUG_DRAW
							// The z component should be the relative vertical distance from the seat to the wheel centre
							Vec3V vNewPos(seatObjMtx.d.x, seatObjMtx.d.y, seatObjMtx.d.z + vWheelPos.z);
							vNewPos = vehicle.GetTransform().Transform(vNewPos);
							CTask::ms_debugDraw.AddLine(RCC_VEC3V(mGlobalSeatMat.d), vNewPos, Color_blue, 1000);
							CTask::ms_debugDraw.AddLine(RCC_VEC3V(mGlobalWheelMat.d), vNewPos, Color_orange, 1000);
							CTask::ms_debugDraw.AddSphere(vNewPos, SPHERE_RADIUS, Color_blue, 1000);
#endif // DEBUG_DRAW

							// Subtract the wheel radius to get a stable reference position
							const CWheel* pWheel = vehicle.GetWheelFromId(VEH_WHEEL_LF);
							Vec3V vGroundPos(seatObjMtx.d.x, seatObjMtx.d.y, seatObjMtx.d.z + vWheelPos.z - pWheel->GetWheelRadius());
							vGroundPos = vehicle.GetTransform().Transform(vGroundPos);
#if DEBUG_DRAW
							CTask::ms_debugDraw.AddLine(vNewPos, vGroundPos, Color_purple, 1000);
							CTask::ms_debugDraw.AddSphere(vGroundPos, SPHERE_RADIUS, Color_purple, 1000);
#endif // DEBUG_DRAW

							// Compute the world space target offset and add the reference position
							float fWorldHeading = rage::Atan2f(-mGlobalSeatMat.b.x, mGlobalSeatMat.b.y);
							qTargetOrientation.FromEulers(Vector3(0.0f,0.0f,fWorldHeading), "xyz");	
							qTargetOrientation.Transform(vTargetPos);
							Quaternion qExtraOrientation;
							qExtraOrientation.FromEulers(Vector3(pExtraVehiclePoint->GetPitch(),0.0f,pExtraVehiclePoint->GetHeading()));
							qTargetOrientation.MultiplyFromLeft(qExtraOrientation);
							vTargetPos += VEC3V_TO_VECTOR3(vGroundPos);
#if DEBUG_DRAW
							CTask::ms_debugDraw.AddSphere(RCC_VEC3V(vTargetPos), SPHERE_RADIUS, Color_red, 1000);
#endif // DEBUG_DRAW
							return true;
						}
					}
				}
				else if (pExtraVehiclePoint->GetLocationType() == CExtraVehiclePoint::ENTITY_RELATIVE)
				{
					vTargetPos = pExtraVehiclePoint->GetPosition();
					qTargetOrientation.FromEulers(Vector3(pExtraVehiclePoint->GetPitch(),0.0f,pExtraVehiclePoint->GetHeading()));
					Quaternion qEntityOrientation = QUATV_TO_QUATERNION(vehicle.GetTransform().GetOrientation());
					Vector3 vEntityPos = VEC3V_TO_VECTOR3(vehicle.GetTransform().GetPosition());
					qEntityOrientation.Transform(vTargetPos);
					vTargetPos += vEntityPos;
					qTargetOrientation.MultiplyFromLeft(qEntityOrientation);
					return true;
				}
				else
				{
					taskAssertf(0,"Not wrote the code to handle other types yet");
				}
			}
		}
	}
	return false;
}

void CTaskVehicleFSM::ProcessDoorHandleArmIkHelper(const CVehicle& vehicle, CPed& ped, s32 iEntryPointIndex, const crClip* pClip, float fPhase, bool& bIkTurnedOn, bool& bIkTurnedOff)
{
	float fStartIkBlendInPhase = 0.0f;
	float fStopIkBlendInPhase = 1.0f;
	float fStartIkBlendOutPhase = 0.0f;
	float fStopIkBlendOutPhase = 1.0f;

	bool bRight = false;

	bool bIkShouldBeOn = false;
	bool bIkShouldBlendOut = false;

	float fBlendInDuration = NORMAL_BLEND_DURATION;
	float fBlendOutDuration = NORMAL_BLEND_DURATION; 

	const bool bFoundBlendInTags = CClipEventTags::FindIkTags(pClip, fStartIkBlendInPhase, fStopIkBlendInPhase, bRight, true);
	if (bFoundBlendInTags)
	{
		float fPhaseDifference = (fStopIkBlendInPhase - fStartIkBlendInPhase);
		taskAssert(fPhaseDifference >= 0);
		fBlendInDuration = pClip->ConvertPhaseToTime(fPhaseDifference);
	}

	const bool bFoundBlendOutTags = CClipEventTags::FindIkTags(pClip, fStartIkBlendOutPhase, fStopIkBlendOutPhase, bRight, false);
	if (bFoundBlendOutTags)
	{
		float fPhaseDifference = (fStopIkBlendOutPhase - fStartIkBlendOutPhase);
		taskAssert(fPhaseDifference >= 0);
		fBlendOutDuration = pClip->ConvertPhaseToTime(fPhaseDifference);
	}

	// If we've not turned the ik on, check if it should be on
	if (!bIkTurnedOn)
	{
		if (bFoundBlendInTags && fPhase >= fStartIkBlendInPhase && fPhase <= fStopIkBlendInPhase)
		{
			bIkShouldBeOn = true;
		}
	}
	// If we've turned the ik on, check if we need to turn it off
	else if (!bIkTurnedOff)
	{
		if (bFoundBlendOutTags && fPhase >= fStartIkBlendOutPhase && fPhase <= fStopIkBlendOutPhase)
		{
			bIkShouldBeOn = false;
			bIkTurnedOn = false;
			bIkShouldBlendOut = true;
		}
	}

	// If the ik should be on now, turn it on
	if (bIkShouldBeOn || bIkTurnedOn)
	{
		bIkTurnedOn = true;

		// Reach for the door handle if we have one 
		CTaskVehicleFSM::ProcessDoorHandleArmIk(vehicle, ped, iEntryPointIndex, bRight, fBlendInDuration);
	}
	// // Check for turning it off
	else if (bIkShouldBlendOut && !bIkTurnedOff)
	{
		bIkTurnedOff = true;	

		// Blend out
		CTaskVehicleFSM::ProcessDoorHandleArmIk(vehicle, ped, iEntryPointIndex, bRight, fBlendOutDuration);
	}
}

void CTaskVehicleFSM::ProcessDoorHandleArmIk(const CVehicle& vehicle, CPed& ped, s32 iEntryPointIndex, bool bRight, float fBlendInDuration, float fBlendOutDuration)
{
#if __BANK
	if (!CTaskEnterVehicle::ms_Tunables.m_DisableDoorHandleArmIk)
#endif // __BANK
	{
		const CEntryExitPoint* pEntryPoint = vehicle.GetEntryExitPoint(iEntryPointIndex);		
		s32 iDoorHandleBoneIndex = pEntryPoint->GetDoorHandleBoneIndex();
		if (iDoorHandleBoneIndex > -1)
		{
			TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, IK_BLEND_IN_RANGE, 0.0f, 0.0f, 2.0f, 0.01f);
			TUNE_GROUP_FLOAT(IN_VEHICLE_TUNE, IK_BLEND_OUT_RANGE, 0.2f, 0.0f, 2.0f, 0.01f);
			static Vector3 offset(VEC3_ZERO);
			if (bRight)
			{
				ped.GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::RIGHT_ARM, &vehicle, iDoorHandleBoneIndex, offset, AIK_TARGET_WRT_IKHELPER, fBlendInDuration, fBlendOutDuration, IK_BLEND_IN_RANGE, IK_BLEND_OUT_RANGE);
			}
			else
			{
				ped.GetIkManager().SetRelativeArmIKTarget(crIKSolverArms::LEFT_ARM, &vehicle, iDoorHandleBoneIndex, offset, AIK_TARGET_WRT_IKHELPER, fBlendInDuration, fBlendOutDuration, IK_BLEND_IN_RANGE, IK_BLEND_OUT_RANGE);
			}
		}
	}
}

CTaskVehicleFSM::eVehicleClipFlags CTaskVehicleFSM::ProcessBikeHandleArmIk(const CVehicle& vehicle, CPed& ped, const crClip* pClip, float fPhase, bool bOn, bool bRight, bool bUseOrientation)
{
#if __BANK
	if (!CTaskEnterVehicle::ms_Tunables.m_DisableBikeHandleArmIk)
#endif // __BANK
	{
		float fPhaseToStartIk = 0.0f;
		float fPhaseToStopIk = 1.0f;

		if (CClipEventTags::FindSpecificIkTags(pClip, fPhaseToStartIk, fPhaseToStopIk, bRight, bOn))
		{
			float fPhaseDifference = (fPhaseToStopIk - fPhaseToStartIk);
			taskAssert(fPhaseDifference >= 0);
			if (fPhase >= fPhaseToStartIk)
			{
				const float fBlendDuration = pClip->ConvertPhaseToTime(fPhaseDifference);
				int iBoneIdx = vehicle.GetBoneIndex(bRight ? VEH_HBGRIP_R : VEH_HBGRIP_L);
				if (iBoneIdx >= 0)
				{
					TUNE_GROUP_FLOAT(BIKE_PICK_UP_PULL_UP_TUNE, BLEND_IN_RANGE, 0.0f, 0.0f, 1.0f, 0.01f);
					TUNE_GROUP_FLOAT(BIKE_PICK_UP_PULL_UP_TUNE, BLEND_OUT_RANGE, 0.25f, 0.0f, 1.0f, 0.01f);

					s32 controlFlags = AIK_TARGET_WRT_IKHELPER | AIK_USE_FULL_REACH;

					if (bUseOrientation)
					{
						controlFlags |= AIK_USE_ORIENTATION;
					}

					if (bOn)
					{
						ped.GetIkManager().SetRelativeArmIKTarget(bRight ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM, &vehicle, iBoneIdx, VEC3_ZERO, controlFlags, fBlendDuration, PEDIK_ARMS_FADEOUT_TIME, BLEND_IN_RANGE, BLEND_OUT_RANGE);
						return VF_IK_Running;
					}
					else
					{
						ped.GetIkManager().SetRelativeArmIKTarget(bRight ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM, &vehicle, iBoneIdx, VEC3_ZERO, controlFlags, 0.0f, fBlendDuration, BLEND_IN_RANGE, BLEND_OUT_RANGE);
						return VF_IK_Blend_Out;
					}
				}
			}
			return VF_IK_Running;
		}
	}
	return VF_No_IK_Clip;
}

bool CTaskVehicleFSM::SeatRequiresHandGripIk(const CVehicle& rVeh, s32 iSeat)
{
	return rVeh.IsSeatIndexValid(iSeat) && rVeh.GetSeatAnimationInfo(iSeat)->IsStandingTurretSeat();
}

void CTaskVehicleFSM::ProcessTurretHandGripIk(const CVehicle& rVeh, CPed& rPed, const crClip* pClip, float fPhase, const CTurret& rTurret)
{
	const crSkeletonData& rSkelData = rVeh.GetSkeletonData();

	float fPhaseToStartIk = 0.0f;
	float fPhaseToStopIk = 1.0f;

	bool bRight = false;
	if (CClipEventTags::FindSpecificIkTags(pClip, fPhaseToStartIk, fPhaseToStopIk, bRight, true))
	{
		float fPhaseDifference = (fPhaseToStopIk - fPhaseToStartIk);
		taskAssert(fPhaseDifference >= 0);
		if (fPhase >= fPhaseToStartIk)
		{
			const float fBlendDuration = pClip->ConvertPhaseToTime(fPhaseDifference);
			CTaskVehicleFSM::IkHandToTurret(rPed, rVeh, rSkelData, bRight, fBlendDuration, rTurret);
		}
	}

	bRight = true;
	fPhaseToStartIk = 0.0f;
	fPhaseToStopIk = 1.0f;
	if (CClipEventTags::FindSpecificIkTags(pClip, fPhaseToStartIk, fPhaseToStopIk, bRight, true))
	{
		float fPhaseDifference = (fPhaseToStopIk - fPhaseToStartIk);
		taskAssert(fPhaseDifference >= 0);
		if (fPhase >= fPhaseToStartIk)
		{
			const float fBlendDuration = pClip->ConvertPhaseToTime(fPhaseDifference);
			CTaskVehicleFSM::IkHandToTurret(rPed, rVeh, rSkelData, bRight, fBlendDuration, rTurret);
		}
	}
}

void CTaskVehicleFSM::IkHandToTurret( CPed &rPed, const CVehicle& rVeh, const crSkeletonData& rSkelData, bool bRightHand, float fBlendDuration, const CTurret& rTurret )
{
	const s32 iFlags = AIK_TARGET_WRT_IKHELPER | AIK_USE_ORIENTATION | AIK_USE_FULL_REACH;
	const crBoneData* pBoneData = NULL;

	switch (rTurret.GetBaseBoneId())
	{
		case VEH_TURRET_1_BASE:
		{
			pBoneData = bRightHand ? rSkelData.FindBoneData("turret_grip_r") : rSkelData.FindBoneData("turret_grip_l");
		}
		break;
		case VEH_TURRET_2_BASE:
		{
			pBoneData = bRightHand ? rSkelData.FindBoneData("turret_grip2_r") : rSkelData.FindBoneData("turret_grip2_l");
		}
		break;
		case VEH_TURRET_3_BASE:
		{
			pBoneData = bRightHand ? rSkelData.FindBoneData("turret_grip3_r") : rSkelData.FindBoneData("turret_grip3_l");
		}
		break;
		case VEH_TURRET_4_BASE:
		{
			pBoneData = bRightHand ? rSkelData.FindBoneData("turret_grip4_r") : rSkelData.FindBoneData("turret_grip4_l");
		}
		break;
		default: 
			aiAssertf(0, "Unknown base turret bone id %i", rTurret.GetBaseBoneId());
			break;
	}

	if (pBoneData)
	{
		rPed.GetIkManager().SetRelativeArmIKTarget(bRightHand ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM, &rVeh, pBoneData->GetIndex(), Vector3(Vector3::ZeroType), iFlags, fBlendDuration);	
	}
}

void CTaskVehicleFSM::PointTurretInNeturalPosition(CVehicle& rVeh, const s32 iSeat)
{
	bool bVehicleHasResetHeadingFlag = rVeh.GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_RESET_TURRET_SEAT_HEADING );
	if (rVeh.GetVehicleWeaponMgr() && rVeh.IsSeatIndexValid(iSeat) && (bVehicleHasResetHeadingFlag || (!rVeh.GetSeatAnimationInfo(iSeat)->IsStandingTurretSeat() && !rVeh.GetSeatManager()->GetPedInSeat(iSeat))))
	{
		CTurret* pTurret = rVeh.GetVehicleWeaponMgr()->GetFirstTurretForSeat(iSeat);
		if (pTurret)
		{
			CFixedVehicleWeapon* pFixedWeapon = rVeh.GetVehicleWeaponMgr()->GetFirstFixedWeaponForSeat(iSeat);
			if (pFixedWeapon)
			{
				s32 nWeaponBoneIndex = rVeh.GetBoneIndex(pFixedWeapon->GetWeaponBone());
				if (nWeaponBoneIndex >= 0)
				{
					Matrix34 weaponMat;
					rVeh.GetGlobalMtx(nWeaponBoneIndex, weaponMat);
					Vector3 vAimAtPos = weaponMat.d + weaponMat.b;
					Matrix34 turretMat;
					pTurret->GetTurretMatrixWorld(turretMat, &rVeh);
					vAimAtPos.z = turretMat.d.z;
					pTurret->AimAtWorldPos(vAimAtPos, &rVeh, false, false);
				}
			}
		}
	}
}

bool CTaskVehicleFSM::CanPedShuffleToOrFromTurretSeat(const CVehicle* pVehicle, const CPed* pPed, s32& iTargetShuffleSeat)
{
	iTargetShuffleSeat = -1;

	if (!pVehicle)
		return false;

	bool bIsAPC = MI_CAR_APC.IsValid() && pVehicle->GetModelIndex() == MI_CAR_APC;

	if (!bIsAPC && !pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
		return false;

	TUNE_GROUP_FLOAT(TURRET_DEBUG, DRIVER_SHUFFLE_MAX_SPEED_LAND, 1.0f, 0.1f, 20.0f, 0.01f)
	TUNE_GROUP_FLOAT(TURRET_DEBUG, DRIVER_SHUFFLE_MAX_SPEED_WATER, 6.0f, 0.1f, 20.0f, 0.01f)
	if (pPed->GetIsDrivingVehicle())
	{
		float fMaxSpeed = pVehicle->GetIsInWater() ? DRIVER_SHUFFLE_MAX_SPEED_WATER : DRIVER_SHUFFLE_MAX_SPEED_WATER;
		Vector3 vRelativeVelocity = pVehicle->GetVelocityIncludingReferenceFrame();
		if (vRelativeVelocity.XYMag() > fMaxSpeed)
		{
			return false;
		}

		if (pVehicle->InheritsFromPlane())
		{
			const CPlane* pPlane = static_cast<const CPlane*>(pVehicle);
			if (pPlane && pPlane->IsInVerticalFlightMode() && pPlane->IsInAir())
			{
				return false;
			}
		}
	}

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, ALLOW_TURRET_SEAT_SHUFFLING_IN_SP, false);
#endif //__BANK

	if (!NetworkInterface::IsGameInProgress() BANK_ONLY(&& !ALLOW_TURRET_SEAT_SHUFFLING_IN_SP))
		return false;

	const s32 iCurrentSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (!pVehicle->IsSeatIndexValid(iCurrentSeatIndex))
		return false;

	const s32 iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iCurrentSeatIndex, pVehicle);
	if (!pVehicle->IsEntryIndexValid(iEntryPointIndex))
		return false;

	// See if we can shuffle to the first linked seat
	const s32 iShuffleSeat1 = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, iCurrentSeatIndex);
	if (pVehicle->IsSeatIndexValid(iShuffleSeat1))
	{
		if (CTaskVehicleFSM::CanPedManualShuffleToTargetSeat(pVehicle, pPed, iShuffleSeat1))
		{
			iTargetShuffleSeat = iShuffleSeat1;
			return true;
		}
	}
	
	// See if we can shuffle to the second linked seat
	const s32 iShuffleSeat2 = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, iCurrentSeatIndex, true);
	if (pVehicle->IsSeatIndexValid(iShuffleSeat2))
	{
		if (CTaskVehicleFSM::CanPedManualShuffleToTargetSeat(pVehicle, pPed, iShuffleSeat2))
		{
			iTargetShuffleSeat = iShuffleSeat2;
			return true;
		}
	}
	
	return false;
}

bool CTaskVehicleFSM::CanPedManualShuffleToTargetSeat(const CVehicle* pVehicle, const CPed* pPed, const s32 iTargetSeat)
{
	if (!pVehicle->IsSeatIndexValid(iTargetSeat))
		return false;

	s32 nDriverSeat = pVehicle->GetDriverSeat();
	if ((nDriverSeat == iTargetSeat) && !pVehicle->IsAnExclusiveDriverPedOrOpenSeat(pPed))
		return false;

	CPed* pOccupierSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, iTargetSeat);
	const CVehicleSeatAnimInfo* pTargetSeatClipInfo = pVehicle->GetSeatAnimationInfo(iTargetSeat);
	if(pTargetSeatClipInfo && pTargetSeatClipInfo->GetPreventShuffleJack() && pOccupierSeat)
		return false;

	const s32 iCurrentSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (!pVehicle->IsSeatIndexValid(iCurrentSeatIndex))
		return false;

	bool bIsAPC = MI_CAR_APC.IsValid() && pVehicle->GetModelIndex() == MI_CAR_APC;
	// If we're in a turret seat we can shuffle to a non turret seat, otherwise disallow shuffling
	const bool bAllowShufflingToNonTurretSeat = pVehicle->IsTurretSeat(iCurrentSeatIndex) || (bIsAPC && iCurrentSeatIndex == 1);

	if (pVehicle->IsTurretSeat(iTargetSeat) || (bIsAPC && iTargetSeat == 1) || bAllowShufflingToNonTurretSeat)
	{
		if (!pOccupierSeat || pOccupierSeat->ShouldBeDead())
		{
			return true;
		}
	}

	return false;
}

bool CTaskVehicleFSM::CanPedShuffleToOrFromExtraSeat(const CVehicle* pVehicle, const CPed* pPed, s32& iTargetShuffleSeat)
{
	iTargetShuffleSeat = -1;

	if (!pVehicle)
		return false;

	if (!pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_EXTRA_SHUFFLE_SEAT_ON_VEHICLE))
		return false;

	if (pPed->GetIsDrivingVehicle())
		return false;

#if __BANK
	TUNE_GROUP_BOOL(VEHICLE_DEBUG, ALLOW_EXTRA_SEAT_SHUFFLING_IN_SP, false);
#endif //__BANK

	// Only allow in MP (unless debug toggled in tunable)
	if (!NetworkInterface::IsGameInProgress() BANK_ONLY(&& !ALLOW_EXTRA_SEAT_SHUFFLING_IN_SP))
		return false;

	// Make sure current seat index is valid
	const s32 iCurrentSeatIndex = pVehicle->GetSeatManager()->GetPedsSeatIndex(pPed);
	if (!pVehicle->IsSeatIndexValid(iCurrentSeatIndex))
		return false;

	// Make sure current entry point index is valid
	const s32 iEntryPointIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(iCurrentSeatIndex, pVehicle);
	if (!pVehicle->IsEntryIndexValid(iEntryPointIndex))
		return false;

	// If we're in a extra shuffle seat we can shuffle to a normal seat, otherwise disallow shuffling
	const bool bAllowShuffleToNormalSeat = pVehicle->GetSeatInfo(iCurrentSeatIndex)->GetIsExtraShuffleSeat();

	// See if we can shuffle to the first linked seat
	const s32 iShuffleSeat1 = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, iCurrentSeatIndex);
	if (pVehicle->IsSeatIndexValid(iShuffleSeat1))
	{
		const bool bIsExtraShuffleSeat = pVehicle->GetSeatInfo(iShuffleSeat1)->GetIsExtraShuffleSeat();
		const bool bIsBlockedByTrailer = pVehicle->HasTrailer() && pVehicle->GetSeatInfo(iShuffleSeat1)->GetDisableWhenTrailerAttached();
		if ((bIsExtraShuffleSeat || bAllowShuffleToNormalSeat) && !bIsBlockedByTrailer)
		{
			CPed* pOccupierSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, iShuffleSeat1);
			if (!pOccupierSeat)
			{
				iTargetShuffleSeat = iShuffleSeat1;
				return true;
			}
		}
	}

	// See if we can shuffle to the second linked seat
	const s32 iShuffleSeat2 = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, iCurrentSeatIndex, true);
	if (pVehicle->IsSeatIndexValid(iShuffleSeat2))
	{
		const bool bIsExtraShuffleSeat = pVehicle->GetSeatInfo(iShuffleSeat2)->GetIsExtraShuffleSeat();
		const bool bIsBlockedByTrailer = pVehicle->HasTrailer() && pVehicle->GetSeatInfo(iShuffleSeat2)->GetDisableWhenTrailerAttached();
		if ((bIsExtraShuffleSeat || bAllowShuffleToNormalSeat) && !bIsBlockedByTrailer)
		{
			CPed* pOccupierSeat = CTaskVehicleFSM::GetPedInOrUsingSeat(*pVehicle, iShuffleSeat2);
			if (!pOccupierSeat)
			{
				iTargetShuffleSeat = iShuffleSeat2;
				return true;
			}
		}
	}

	return false;
}

bool CTaskVehicleFSM::CanPerformOnVehicleTurretJack(const CVehicle& rVeh, s32 iSeatIndex)
{
	bool bIsAATrailer = MI_TRAILER_TRAILERSMALL2.IsValid() && rVeh.GetModelIndex() == MI_TRAILER_TRAILERSMALL2;
	return bIsAATrailer || (rVeh.IsSeatIndexValid(iSeatIndex) && rVeh.GetSeatAnimationInfo(iSeatIndex)->IsStandingTurretSeat());
}

void CTaskVehicleFSM::ForceBikeHandleArmIk(const CVehicle& vehicle, CPed& ped, bool bOn, bool bRight, float fBlendDuration, bool bUseOrientation)
{
	int iBoneIdx = vehicle.GetBoneIndex(bRight ? VEH_HBGRIP_R : VEH_HBGRIP_L);
	if (iBoneIdx >= 0)
	{
		s32 controlFlags = AIK_TARGET_WRT_IKHELPER | AIK_USE_FULL_REACH;

		if (bUseOrientation)
		{
			controlFlags |= AIK_USE_ORIENTATION;
		}

		if (bOn)
		{
			ped.GetIkManager().SetRelativeArmIKTarget(bRight ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM, &vehicle, iBoneIdx, VEC3_ZERO, controlFlags, fBlendDuration);
		}
		else
		{
			ped.GetIkManager().SetRelativeArmIKTarget(bRight ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM, &vehicle, iBoneIdx, VEC3_ZERO, controlFlags, 0.0f, fBlendDuration);
		}
	}
}

bool CTaskVehicleFSM::ProcessSeatBoneArmIk(const CVehicle& vehicle, CPed& ped, s32 iSeatIndex, const crClip* pClip, float fPhase, bool bOn, bool bRight)
{
#if __BANK
	if (!CTaskEnterVehicle::ms_Tunables.m_DisableBikeHandleArmIk)
#endif // __BANK
	{
		float fPhaseToStartIk = 0.0f;
		float fPhaseToStopIk = 1.0f;

		if (CClipEventTags::FindSpecificIkTags(pClip, fPhaseToStartIk, fPhaseToStopIk, bRight, bOn))
		{
			float fPhaseDifference = (fPhaseToStopIk - fPhaseToStartIk);
			taskAssert(fPhaseDifference >= 0);
			if (fPhase >= fPhaseToStartIk)
			{
				const float fBlendDuration = pClip->ConvertPhaseToTime(fPhaseDifference);
				s32 iBoneIdx = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeatIndex);
				if (iBoneIdx >= 0)
				{
					if (bOn)
					{
						ped.GetIkManager().SetRelativeArmIKTarget(bRight ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM, &vehicle, iBoneIdx, VEC3_ZERO, AIK_TARGET_WRT_IKHELPER, fBlendDuration);
					}
					else
					{
						ped.GetIkManager().SetRelativeArmIKTarget(bRight ? crIKSolverArms::RIGHT_ARM : crIKSolverArms::LEFT_ARM, &vehicle, iBoneIdx, VEC3_ZERO, AIK_TARGET_WRT_IKHELPER, 0.0f, fBlendDuration);
						return true;
					}
				}
			}
		}
	}
	return false;
}

void  CTaskVehicleFSM::UpdatePedsMover(CPed* pPed, const Vector3 &vNewPos, const Quaternion& qNewOrientation)
{
	Vector3 vEulers(Vector3::ZeroType);
	qNewOrientation.ToEulers(vEulers);

	const float fNewHeading = fwAngle::LimitRadianAngle(vEulers.z);

	// Set the move blend data to match our updated orientation
	pPed->GetMotionData()->SetCurrentHeading(fNewHeading);
	pPed->GetMotionData()->SetDesiredHeading(fNewHeading);

	// Create the desired ped matrix from the params passed in
	Matrix34 mPedMat(Matrix34::IdentityType);
	mPedMat.FromQuaternion(qNewOrientation);
	mPedMat.d = vNewPos;

	// Set the desired matrix and update the ragdoll
	pPed->SetMatrix(mPedMat, true, true, false);
	pPed->UpdateRagdollMatrix(false);
}

// void CTaskVehicleFSM::ApplyInitialXOffsetFixUp(Matrix34& mRefMatInOut, float fClipPhase, const Vector3 &vOffset, float fStartPhase, float fEndPhase)
// {
// 	if (fClipPhase >= fStartPhase)
// 	{			
// 		const float fInitialRatio = MIN(( fClipPhase - fStartPhase ) / ( fEndPhase - fStartPhase ),1.0f);
// 		// Fix up the initial translational offset from where we started the get on to where we should have started it from
// 		Vector3 temp;
// 		mRefMatInOut.Transform3x3(vOffset, temp);
// 		mRefMatInOut.d += (fInitialRatio * temp);
// 	}
// }
// 
// void CTaskVehicleFSM::ApplyInitialRotOffsetFixUp(Matrix34& mRefMatInOut, float fClipPhase, const Quaternion &qRotationalOffset, float fStartPhase, float fEndPhase)
// {
// 	if (fClipPhase >= fStartPhase)
// 	{			
// 		// Fix up the initial rotational offset from where we started the get on to where we should have started it from
// 		const float fInitialRatio = MIN(( fClipPhase - fStartPhase ) / ( fEndPhase - fStartPhase ),1.0f);
// 		
// 		Quaternion qRefRotation;
// 		mRefMatInOut.ToQuaternion(qRefRotation);					// Get the reference matrix rotation
// 		Quaternion temp = qRotationalOffset;
// 		temp.Multiply(qRefRotation);					// Add the offset to the reference rotation to obtain the desired target rotation
// 		qRefRotation.SlerpNear(fInitialRatio, temp);	// Slerp to the target rotation
// 		mRefMatInOut.FromQuaternion(qRefRotation);					// Set the rotational part of the matrix
// 	}
// }
// 
// Vector3 CTaskVehicleFSM::ComputeExtraFixUp(const CVehicle* pVehicle, const crClip* pClip, s32 iFlags, s32 iTargetEntryPoint, float fPhase, const Matrix34& mInitialMatrix)
// {
// 	// Calculate the extra translation needed to get us to the seat due to the clip not moving us far enough
// 	// This could be part of the initial offset computed once at the start?
// 	Matrix34 mTargetRefMat(Matrix34::IdentityType);
// 	CalculateReferenceMatrix(iFlags, pVehicle, mTargetRefMat, iTargetEntryPoint);
// 	// Compute remaining translation
// 	Vector3 vTotalTranslation = fwAnimDirector::GetMoverTrackDisplacement(pClip, fPhase, 1.0f);
// 	mInitialMatrix.Transform3x3(vTotalTranslation);
// 	vTotalTranslation += mInitialMatrix.d;		
// 	return Vector3(mTargetRefMat.d - vTotalTranslation);
// }
// 
// void CTaskVehicleFSM::ApplyExtraFixUp(Matrix34& mRefMatInOut, const Vector3 &vExtraFixUp, float fClipPhase, float fEndPhase)
// {
// 	// Do this only after the initial translation fixup has been applied
// 	if (fClipPhase > fEndPhase)
// 	{
// 		const float fExtraRatio = MIN(( fClipPhase - fEndPhase ) / ( 1.0f - fEndPhase ),1.0f);
// 		mRefMatInOut.d += (fExtraRatio * vExtraFixUp);
// 	}
// }

void CTaskVehicleFSM::DriveBikeAngleFromClip(CVehicle* pVehicle, s32 iFlags, const float fCurrentPhase, const float fStartPhase, const float fEndPhase, float fOriginalLeanAngle, float fTargetLeanAngle)
{
	taskFatalAssertf(pVehicle, "NULL vehicle pointer");
	taskAssertf(pVehicle->InheritsFromBike(), "Vehicle isn't a bike");

	CCarDoor* pDoor = pVehicle->GetDoor(0);
	if (taskVerifyf(pDoor, "Bike doesn't have a 'door'"))
	{
		if (iFlags & VF_OpenDoor)
		{
			float fAnimLeanRatio = rage::Min(1.0f, (fCurrentPhase - fStartPhase) / (fEndPhase - fStartPhase));
			aiDebugf1("Anim Lean Ratio : %.4f", fAnimLeanRatio);
			const float fBikeLeanAngle = fAnimLeanRatio*fTargetLeanAngle + (1.0f - fAnimLeanRatio)*fOriginalLeanAngle;
			aiDebugf1("Bike Lean Angle : %.4f", fBikeLeanAngle);
			pDoor->SetTargetDoorOpenRatio(fBikeLeanAngle, CCarDoor::DRIVEN_AUTORESET);
			pDoor->SetBikeDoorOpenAmount(1.0f);
			pDoor->SetBikeDoorClosedAmount(0.0f);

		}
		else
		{
			pDoor->SetTargetDoorOpenRatio(fTargetLeanAngle, CCarDoor::DRIVEN_AUTORESET);
			pDoor->SetBikeDoorOpenAmount(1.0f);
			pDoor->SetBikeDoorClosedAmount(0.0f);
		}
	}
}

void CTaskVehicleFSM::ProcessOpenDoor(const crClip& clip, float fPhase, CVehicle& vehicle, s32 iEntryPointIndex, bool bProcessOnlyIfTagsFound, bool bBlockRearDoorDriving, CPed* UNUSED_PARAM(pEvaluatingPed), bool bDoorInFrontBeingUsed)
{
	CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&vehicle, iEntryPointIndex);
	if (pDoor)
	{
		static dev_float sfDefaultStartPhase = 0.1f;
		static dev_float sfDefaultEndPhase = 0.5f;

		float fPhaseToBeginOpenDoor = sfDefaultStartPhase;
		float fPhaseToEndOpenDoor = sfDefaultEndPhase;

		bool bStartTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(&clip, CClipEventTags::Door, CClipEventTags::Start, true, fPhaseToBeginOpenDoor);
		bool bEndTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(&clip, CClipEventTags::Door, CClipEventTags::Start, false, fPhaseToEndOpenDoor);

		if(!bProcessOnlyIfTagsFound || (bStartTagFound && bEndTagFound))
		{
			s32 iFlags = CTaskVehicleFSM::VF_OpenDoor;

			// If the door in front of us is being used, then limit the amount that we can open the door
			if (!bBlockRearDoorDriving && bDoorInFrontBeingUsed)
			{
				iFlags |= CTaskVehicleFSM::VF_FrontEntryUsed;
			}

			CTaskVehicleFSM::DriveDoorFromClip(vehicle, pDoor, iFlags, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor);

			// Buses have two doors which need to open simultaneously...
			CCarDoor* pDoor2 = vehicle.GetSecondDoorFromEntryPointIndex(iEntryPointIndex);
			if (pDoor2)
			{
				CTaskVehicleFSM::DriveDoorFromClip(vehicle, pDoor2, iFlags, fPhase, fPhaseToBeginOpenDoor, fPhaseToEndOpenDoor);
			}
		}
	}
}

bool CTaskVehicleFSM::CanDriveRearDoorPartially(const CVehicle& rVeh)
{
	if (rVeh.GetLayoutInfo()->GetDisableTargetRearDoorOpenRatio())
		return false;

	// TODO: Patch these vehicles?
	if (DoesVehicleHaveDoubleBackDoors(rVeh))
		return false;

	return true;
}

bool CTaskVehicleFSM::DoesVehicleHaveDoubleBackDoors(const CVehicle& rVeh)
{
	static u32 LAYOUT_VAN = atStringHash("LAYOUT_VAN");
	static u32 LAYOUT_VAN_BOXVILLE = atStringHash("LAYOUT_VAN_BOXVILLE");
	static u32 LAYOUT_VAN_MULE = atStringHash("LAYOUT_VAN_MULE");
	static u32 LAYOUT_VAN_POLICE = atStringHash("LAYOUT_VAN_POLICE");
	static u32 LAYOUT_VAN_PONY = atStringHash("LAYOUT_VAN_PONY");
	static u32 LAYOUT_RIOT_VAN = atStringHash("LAYOUT_RIOT_VAN");

	const CVehicleLayoutInfo* pLayoutInfo = rVeh.GetLayoutInfo();
	const u32 vehicleLayoutNameHash = pLayoutInfo->GetName().GetHash();

	if (vehicleLayoutNameHash == LAYOUT_VAN ||
		vehicleLayoutNameHash == LAYOUT_VAN_BOXVILLE ||
		vehicleLayoutNameHash == LAYOUT_VAN_MULE ||
		vehicleLayoutNameHash == LAYOUT_VAN_POLICE ||
		vehicleLayoutNameHash == LAYOUT_VAN_PONY ||
		vehicleLayoutNameHash == LAYOUT_RIOT_VAN)
	{
		return true;
	}
	return false;
}

s32 CTaskVehicleFSM::FindBestTurretSeatIndexForVehicle(const CVehicle& rVeh, const CPed& rPed, const CPed* pDriver, s32 iDriversSeat, bool bConsiderFriendlyOccupyingPeds, bool bOnlyConsiderIfPreferEnterTurretAfterDriver)
{
	const CVehicleModelInfo* pVehicleModelInfo = rVeh.GetVehicleModelInfo();
	if (!pVehicleModelInfo)
		return -1;

	// Only consider turreted vehicles
	const bool bAPC = MI_CAR_APC.IsValid() && rVeh.GetModelIndex() == MI_CAR_APC;
	const bool bHasTurret = pVehicleModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);
	if (!bAPC && !bHasTurret)
		return -1;

	s32 iBestTurretIndex = -1;
	float fClosestTurretDistanceSqr = FLT_MAX;

	if (bAPC)
	{
		iBestTurretIndex = 1;

		const CPed* pTurretOccupier = rVeh.GetSeatManager()->GetPedInSeat(iBestTurretIndex);
		const bool bCanEnterTurret = CanDisplacePedInTurretSeat(pTurretOccupier, bConsiderFriendlyOccupyingPeds);
		if (!bCanEnterTurret)
			return -1;
	}
	else
	{
		Vector3 vPedPos = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
		float fDistToSeatSqr = -1;
		float fDistToDriverSeatSqr = -1;
		if(rVeh.InheritsFromBoat() && rVeh.GetVehicleModelInfo() && rVeh.GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_CHECK_IF_DRIVER_SEAT_IS_CLOSER_THAN_TURRETS_WITH_ON_BOARD_ENTER ))
		{
			TUNE_GROUP_INT(ENTER_VEHICLE_TUNE, uOnBoardDriverSeat, 0, 0, 100, 10);
			Vector3 vDriverSeatPosition(Vector3::ZeroType);
			Quaternion qSeatRot;
			CModelSeatInfo::CalculateSeatSituation(&rVeh, vDriverSeatPosition, qSeatRot, rVeh.GetDirectEntryPointIndexForSeat(uOnBoardDriverSeat), uOnBoardDriverSeat);
			fDistToDriverSeatSqr =  vPedPos.Dist2(vDriverSeatPosition);
		}

		const s32 iNumSeats = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetNumSeats();
		for (s32 iSeatIndex = 0; iSeatIndex < iNumSeats; ++iSeatIndex)
		{
			const CVehicleSeatAnimInfo* pSeatAnimInfo = rVeh.GetSeatAnimationInfo(iSeatIndex);
			if (pSeatAnimInfo && pSeatAnimInfo->IsTurretSeat())
			{
				if (!rVeh.IsTurretSeat(iSeatIndex))
					continue;

				const CPed* pTurretOccupier = rVeh.GetSeatManager()->GetPedInSeat(iSeatIndex);
				const bool bCanEnterTurret = CanDisplacePedInTurretSeat(pTurretOccupier, bConsiderFriendlyOccupyingPeds);
				if (!bCanEnterTurret)
					continue;

				Vector3 vSeatPosition(Vector3::ZeroType);
				const CModelSeatInfo* pModelSeatinfo  = rVeh.GetVehicleModelInfo()->GetModelSeatInfo();
				s32 iSeatBoneIndex = pModelSeatinfo ? pModelSeatinfo->GetBoneIndexFromSeat(iSeatIndex) : -1;
				if (iSeatBoneIndex != -1)
				{
					Quaternion qSeatRot;
					CModelSeatInfo::CalculateSeatSituation(&rVeh, vSeatPosition, qSeatRot, rVeh.GetDirectEntryPointIndexForSeat(iSeatIndex), iSeatIndex);

					fDistToSeatSqr =  vPedPos.Dist2(vSeatPosition);

					if ((fDistToSeatSqr != -1) && (fDistToSeatSqr < fClosestTurretDistanceSqr))
					{
						fClosestTurretDistanceSqr = fDistToSeatSqr;
						iBestTurretIndex = iSeatIndex;
					}
				}
			}
		}

		if((fDistToDriverSeatSqr != -1) && (fClosestTurretDistanceSqr > fDistToDriverSeatSqr))
		{
			iBestTurretIndex = -1;
		}
	}

	if (iBestTurretIndex == -1)
		return -1;

	if (rPed.GetGroundPhysical() == &rVeh)
	{
		// Prefer turret if we're standing on a non-heli vehicle
		return rVeh.InheritsFromHeli() ? -1 : iBestTurretIndex;
	}
	else
	{
		// Decide if this is a vehicle we should prefer to enter the turret seat if if has a driver (or someone who will be moving into the drivers seat)

		// Potentially unsafe if drivers seat doesn't have a shuffle seat
		const s32 iCurrentEntryIndex = rVeh.GetDirectEntryPointIndexForSeat(iDriversSeat);
		const s32 iShuffleSeat = pVehicleModelInfo->GetModelSeatInfo()->GetShuffleSeatForSeat(iCurrentEntryIndex, iDriversSeat, false);

		const CPed* pPassenger = CTaskVehicleFSM::GetPedInOrUsingSeat(rVeh, iShuffleSeat);
		const bool bPassengerIsFriendly = pPassenger && pPassenger->GetPedIntelligence()->IsFriendlyWith(rPed);
		const bool bPassengerIsEntering = bPassengerIsFriendly && pPassenger->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE);		
		const bool bHasDriverOrPassenger = (pDriver && !pDriver->ShouldBeDead()) || bPassengerIsEntering;			
		const bool bPreferEnterTurretAfterDriver = bHasDriverOrPassenger && rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PREFER_ENTER_TURRET_AFTER_DRIVER);

		if (bPreferEnterTurretAfterDriver)
		{	
			// Enter turret seat if we've got a driver and told to prefer the turret seat					
			return iBestTurretIndex;
		}
		else if (!bOnlyConsiderIfPreferEnterTurretAfterDriver)
		{
			// Otherwise, allow to choose which seat we want based off distance
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(rPed.GetTransform().GetPosition());
			int iBestSeatIndex = -1;
			int iClosestTurretSeatIndex = -1;
			float fClosestDist = 10000.0f;
			float fCloestTurretSeatDist = 10000.0f;
			const s32 iNumSeats = pVehicleModelInfo->GetModelSeatInfo()->GetNumSeats();

			for (s32 iSeatIndex = 0; iSeatIndex < iNumSeats; iSeatIndex++)
			{	
				// Ignore none turret seats
				if (!rVeh.IsTurretSeat(iSeatIndex))
				{
					if (!bAPC || iSeatIndex != 1)
					{
						continue;
					}
				}

				// Just do a distance test to consider the best turret seat (currently only the valkyrie has more than one seat, note: we only evaluate the first direct entry point here)
				const s32 iDirectEntryPoint = rVeh.GetDirectEntryPointIndexForSeat(iSeatIndex);
				if (!rVeh.IsEntryIndexValid(iDirectEntryPoint))
				{
					continue;
				}

				const float fDist2EntryPoint = ComputeSquaredDistanceFromPedPosToEntryPoint(rPed, vPedPosition, rVeh, iDirectEntryPoint);
				if (fDist2EntryPoint < fCloestTurretSeatDist)
				{
					fCloestTurretSeatDist = fDist2EntryPoint;
					iClosestTurretSeatIndex = iSeatIndex;
				}

				if (fDist2EntryPoint < fClosestDist)
				{
					// Check we can actually enter this seat
					const CPed* pTurretOccupier = rVeh.GetSeatManager()->GetPedInSeat(iSeatIndex);
					if (CanDisplacePedInTurretSeat(pTurretOccupier, bConsiderFriendlyOccupyingPeds))
					{
						fClosestDist = fDist2EntryPoint;
						iBestSeatIndex = iSeatIndex;
					}
				}
			}

			// For some vehicles, return the best index no matter if it's closest or not
			bool bBarrage = MI_CAR_BARRAGE.IsValid() && rVeh.GetModelIndex() == MI_CAR_BARRAGE;

			// Only accept the closest turret seat if we have more than one (e.g. on valkyrie, we never consider the opposite turret seat)
			if (iClosestTurretSeatIndex == iBestSeatIndex || bBarrage)
			{
				return iBestSeatIndex;
			}
		}
	}
	return -1;
}

bool CTaskVehicleFSM::CanDisplacePedInTurretSeat(const CPed* pTurretOccupier, bool bConsiderFriendlyOccupyingPeds)
{
	return !pTurretOccupier || pTurretOccupier->ShouldBeDead() || pTurretOccupier->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_SEAT_SHUFFLE) || bConsiderFriendlyOccupyingPeds;
}

bool CTaskVehicleFSM::CanForcePedToShuffleAcross(const CPed& rPed, const CPed& rOtherPed, const CVehicle& rVeh)
{
	if (!rPed.IsPlayer())
		return false;

	if (!rOtherPed.IsPlayer())
		return false;

	if (rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE))
	{
		const s32 iOtherSeatIndex = rVeh.GetSeatManager()->GetPedsSeatIndex(&rOtherPed);
		if (rVeh.IsTurretSeat(iOtherSeatIndex))
		{
			return false;
		}
	}

	if (rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_REAR_SEAT_ACTIVITIES) || rVeh.m_bSpecialEnterExit)
	{
		return false;
	}

	if (rOtherPed.GetPedConfigFlag(CPED_CONFIG_FLAG_DontShuffleInVehicleToMakeRoom))
	{
		return false;
	}

	return true;
}

float CTaskVehicleFSM::ComputeSquaredDistanceFromPedPosToEntryPoint(const CPed &rPed, const Vector3& vPedPos, const CVehicle& rVeh, s32 iEntryPointIndex)
{
	Vector3 vEntryPos;
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	CModelSeatInfo::CalculateEntrySituation(&rVeh, &rPed, vEntryPos, qEntryOrientation, iEntryPointIndex);		
	return vPedPos.Dist2(vEntryPos);
}

float CTaskVehicleFSM::ComputeSquaredDistanceFromPedPosToSeat(const Vector3& vPedPos, const CVehicle& rVeh, s32 iEntryPointIndex, s32 iSeatIndex)
{
	Vector3 vSeatPos;
	Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
	CModelSeatInfo::CalculateSeatSituation(&rVeh, vSeatPos, qEntryOrientation, iEntryPointIndex, iSeatIndex);		
	return vPedPos.Dist2(vSeatPos);
}

void CTaskVehicleFSM::DriveDoorFromClip(CVehicle& vehicle, CCarDoor* pDoor, s32 iFlags, const float fCurrentPhase, const float fStartPhase, const float fEndPhase)
{
	taskFatalAssertf(pDoor, "NULL door pointer");

	taskAssertf(iFlags & VF_OpenDoor || iFlags & VF_CloseDoor, "No door flag was set");

	float fMaxOpenRatioInitial = 1.0f;

	if (iFlags & VF_UseOscillation)
	{
		fMaxOpenRatioInitial = (iFlags & VF_FromOutside) ? CTaskEnterVehicle::ms_Tunables.m_MaxOpenRatioForOpenDoorInitialOutside : CTaskEnterVehicle::ms_Tunables.m_MaxOpenRatioForOpenDoorInitialInside;
	}

	const bool bFrontEntryUsed = (iFlags & VF_FrontEntryUsed) != 0;
	const bool bOpeningDoor = (iFlags & VF_OpenDoor) != 0;
	TUNE_GROUP_FLOAT(DOOR_INTERSECTION_TUNE, MAX_FRONT_ENTRY_USED_OPEN_DOOR_RATIO_MP, 0.3f, 0.0f, 1.0f, 0.01f);
	const float fMaxFrontEntryUsedDoorRation = NetworkInterface::IsGameInProgress() ? MAX_FRONT_ENTRY_USED_OPEN_DOOR_RATIO_MP : CTaskEnterVehicle::ms_Tunables.m_TargetRearDoorOpenRatio;
	float fFullTargetRatio = (bFrontEntryUsed && CanDriveRearDoorPartially(vehicle)) ? fMaxFrontEntryUsedDoorRation : fMaxOpenRatioInitial;
	bool bDoorBeingForcedOpen = pDoor->GetIsFullyOpen(0.05f) && pDoor->GetFlag(CCarDoor::DRIVEN_NORESET);

	float fTargetRatio = (bOpeningDoor && !bDoorBeingForcedOpen) ? 0.0f : fMaxOpenRatioInitial;
	u32 uCloseDoorFlags = 0;

	if (fCurrentPhase >= fEndPhase)
	{
		TUNE_GROUP_BOOL(DOOR_INTERSECTION_TUNE, CLAMP_FULL_TARGET_RATIO_IF_ALREADY_OPEN_AFTER_ANIM_TAG, true);
		if (CLAMP_FULL_TARGET_RATIO_IF_ALREADY_OPEN_AFTER_ANIM_TAG && bOpeningDoor && fFullTargetRatio < pDoor->GetDoorRatio())
		{
			fFullTargetRatio = pDoor->GetDoorRatio();
		}

		if (bOpeningDoor)
		{
			// Drive the door only part way if we're driving the rear door and someones getting out the front
			fTargetRatio = fFullTargetRatio;
			uCloseDoorFlags = CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_SWINGING;
		}
		else 
		{
			fTargetRatio = 0.0f;
			uCloseDoorFlags = CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN;
		}
	}
	else if (fCurrentPhase >= fStartPhase)
	{
		TUNE_GROUP_BOOL(DOOR_INTERSECTION_TUNE, CLAMP_FULL_TARGET_RATIO_IF_ALREADY_OPEN_DURING_ANIM_TAG, true);
		if (CLAMP_FULL_TARGET_RATIO_IF_ALREADY_OPEN_DURING_ANIM_TAG && bOpeningDoor && fFullTargetRatio < pDoor->GetDoorRatio())
		{
			fFullTargetRatio = pDoor->GetDoorRatio();
		}

		if (bOpeningDoor)
		{
			fTargetRatio = rage::Min(fFullTargetRatio, (fCurrentPhase - fStartPhase) / (fEndPhase - fStartPhase));
			fTargetRatio = MAX(pDoor->GetDoorRatio(), fTargetRatio);
			uCloseDoorFlags = CCarDoor::DRIVEN_AUTORESET|CCarDoor::DRIVEN_SPECIAL|CCarDoor::WILL_LOCK_SWINGING;
		}
		else
		{
			fTargetRatio = rage::Min(1.0f, 1.0f - (fCurrentPhase - fStartPhase) / (fEndPhase - fStartPhase));
			fTargetRatio = rage::Min(pDoor->GetDoorRatio(), fTargetRatio);
			uCloseDoorFlags = CCarDoor::DRIVEN_AUTORESET|CCarDoor::WILL_LOCK_DRIVEN|CCarDoor::DRIVEN_SPECIAL;
		}
	}

	if (CTaskEnterVehicle::ms_Tunables.m_UseSlowInOut)
	{
		SlowInOut(fTargetRatio);
	}

	// Don't break the latch if not intended to open the door
	if(fTargetRatio != 0.0f || !pDoor->GetIsLatched(&vehicle))
	{
		// If script are already forcing the door open, make sure that doesn't reset when we call SetTargetDoorOpenRatio below
		if (bDoorBeingForcedOpen)
		{
			uCloseDoorFlags |= CCarDoor::DRIVEN_NORESET;
		}

		pDoor->SetTargetDoorOpenRatio(fTargetRatio, uCloseDoorFlags);
		pDoor->SetFlag(CCarDoor::PED_DRIVING_DOOR|CCarDoor::DRIVEN_SPECIAL);

		vehicle.m_nVehicleFlags.bMayHavePedDrivingDoorFlagSet = true;
	}
}

CTaskVehicleFSM::CTaskVehicleFSM( CVehicle* pVehicle, SeatRequestType iSeatRequestType, s32 iSeat, const VehicleEnterExitFlags& iRunningFlags, s32 iTargetEntryPoint )
:
m_iRunningFlags(iRunningFlags),
m_pVehicle(pVehicle),
m_bRepeatStage(false),
m_iTargetEntryPoint(iTargetEntryPoint),
m_iSeatRequestType(iSeatRequestType),
m_iSeat(iSeat),
m_fMoveBlendRatio(MOVEBLENDRATIO_RUN),
m_bWarping(iRunningFlags.BitSet().IsSet(CVehicleEnterExitFlags::Warp)),
m_bWaitForSeatToBeEmpty(false),
m_bWaitForSeatToBeOccupied(false),
m_bTimerRanOut(false)
{
}

CTaskVehicleFSM::~CTaskVehicleFSM()
{
}


void CTaskVehicleFSM::SetTimerRanOut( bool bTimerRanOut )
{
	m_bTimerRanOut = bTimerRanOut;
	m_bWarping = m_bWarping || IsFlagSet(CVehicleEnterExitFlags::Warp) || (m_bTimerRanOut && (IsFlagSet(CVehicleEnterExitFlags::WarpAfterTime)));
}

bool CTaskVehicleFSM::GetWillUseDoor( CPed* UNUSED_PARAM(pPed) )
{
	// Add checks in temporarily until we have some metadata with vehicle clip sets.
	if( m_pVehicle->InheritsFromBike() ||
		m_pVehicle->InheritsFromTrain() || 
		m_pVehicle->InheritsFromPlane() ||
		m_pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_CLOSE_DOOR_UPON_EXIT))
	{
		return false;
	}

	return true;
}

CCarDoor* CTaskVehicleFSM::GetDoorForEntryPointIndex(CVehicle* pVehicle, s32 iEntryPointIndex)
{
	if (pVehicle)
	{
		s32 iBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);

		if (iBoneIndex > -1)
		{
			return pVehicle->GetDoorFromBoneIndex(iBoneIndex);
		}
	}
	return NULL;
}

const CCarDoor* CTaskVehicleFSM::GetDoorForEntryPointIndex(const CVehicle* pVehicle, s32 iEntryPointIndex)
{
	if (pVehicle)
	{
		s32 iBoneIndex = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);

		if (iBoneIndex > -1)
		{
			return pVehicle->GetDoorFromBoneIndex(iBoneIndex);
		}
	}
	return NULL;
}

CCarDoor* CTaskVehicleFSM::GetDoorForEntryPointIndex(s32 iEntryPointIndex)
{
	s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);

	if (iBoneIndex > -1)
	{
		return m_pVehicle->GetDoorFromBoneIndex(iBoneIndex);
	}
	return NULL;
}

const CCarDoor* CTaskVehicleFSM::GetDoorForEntryPointIndex(s32 iEntryPointIndex) const
{
	s32 iBoneIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);

	if (iBoneIndex > -1)
	{
		return m_pVehicle->GetDoorFromBoneIndex(iBoneIndex);
	}
	return NULL;
}

bool CTaskVehicleFSM::ProcessApplyForce(CMoveNetworkHelper& moveNetworkHelper, CVehicle& vehicle, const CPed& ped)
{
	if (moveNetworkHelper.GetNetworkPlayer())
	{
		const CClipEventTags::CApplyForceTag* pProp = static_cast<const CClipEventTags::CApplyForceTag*>(moveNetworkHelper.GetNetworkPlayer()->GetProperty(CClipEventTags::ApplyForce));
		if (pProp)
		{
			float fForce = pProp->GetForce();
			if (Abs(fForce) > 0.0f)
			{
				float fForceMultDueToPassengers = 1.0f;
				if(vehicle.GetNumberOfPassenger() > 0)
				{
					fForceMultDueToPassengers /= vehicle.GetNumberOfPassenger() + (vehicle.GetDriver() ? 1 : 0);
				}

				TUNE_GROUP_FLOAT(VEHICLE_FORCE, fDefaultOffsetX, 0.0f, 0.0f, 100.0f, 0.1f);
				TUNE_GROUP_FLOAT(VEHICLE_FORCE, fDefaultOffsetY, -0.1f, 0.0f, 100.0f, 0.1f);

				float fOffsetX = fDefaultOffsetX;
				float fOffsetY = fDefaultOffsetY;

				pProp->GetXOffset(fOffsetX);
				pProp->GetYOffset(fOffsetY);

				Vector3 vForceOffset(Vector3::ZeroType);
				vForceOffset += VEC3V_TO_VECTOR3(vehicle.GetTransform().GetA()) * fOffsetX;
				vForceOffset += VEC3V_TO_VECTOR3(vehicle.GetTransform().GetB()) * fOffsetY;

				vehicle.ApplyImpulse(fForce*fForceMultDueToPassengers*ped.GetMassForApplyVehicleForce()*VEC3V_TO_VECTOR3(vehicle.GetTransform().GetC()), vForceOffset);
				return true;
			}
		}
	}
	return false;
}

bool CTaskVehicleFSM::ApplyForceForDoorClosed(CVehicle& vehicle, const CPed& ped, s32 iEntryPointIndex, float fForceMultiplier)
{
	CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&vehicle, iEntryPointIndex);
	if (pDoor && pDoor->GetIsClosed())
	{
		const Matrix34& matrix = RCC_MATRIX34(vehicle.GetMatrixRef());
		Vector3 vLocalOffset;
		matrix.UnTransform(VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition()), vLocalOffset);
		vLocalOffset.z = 0;
		float fForceMultDueToPassengers = 1.0f;
		if(vehicle.GetNumberOfPassenger() > 0)
		{
			fForceMultDueToPassengers /= vehicle.GetNumberOfPassenger() + (vehicle.GetDriver() ? 1 : 0);
		}
		if((vLocalOffset).Mag2() < rage::square(vehicle.GetBoundRadius()))
		{
			vehicle.ApplyImpulse(Vector3(0.0f,0.0f, fForceMultiplier * ped.GetMassForApplyVehicleForce() * fForceMultDueToPassengers), vLocalOffset);
			return true;
		}
	}
	return false;
}

void CTaskVehicleFSM::UnreserveDoor(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex)
{
	taskAssertf(!rPed.IsNetworkClone(), "Can't unreserve door on a clone ped!");
	if (rVeh.IsEntryPointIndexValid(iEntryPointIndex))
	{
		CComponentReservation* pComponentReservation = rVeh.GetComponentReservationMgr()->GetDoorReservation(iEntryPointIndex);
		if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(&rPed))
		{
			CComponentReservationManager::UnreserveVehicleComponent(&rPed, pComponentReservation);
		}
	}
}

void CTaskVehicleFSM::UnreserveSeat(CPed& rPed, CVehicle& rVeh, s32 iEntryPointIndex)
{
	taskAssertf(!rPed.IsNetworkClone(), "Can't unreserve seat on a clone ped!");
	if (rVeh.IsEntryPointIndexValid(iEntryPointIndex))
	{
		CComponentReservation* pComponentReservation = rVeh.GetComponentReservationMgr()->GetSeatReservation(iEntryPointIndex, SA_directAccessSeat);
		if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(&rPed))
		{
			CComponentReservationManager::UnreserveVehicleComponent(&rPed, pComponentReservation);
		}
	}
}

bool CTaskVehicleFSM::ApplyForceForDoorClosed(CVehicle& vehicle, s32 iBoneIndex, float fForceMultiplier)
{
	CCarDoor* pDoor = vehicle.GetDoorFromBoneIndex(iBoneIndex);
	if (pDoor && pDoor->GetIsClosed())
	{
		Matrix34 localMatrix;
		if(pDoor->GetLocalMatrix(&vehicle, localMatrix))
		{
			Vector3 vLocalOffset = localMatrix.d;
			vLocalOffset.z = 0;
			if((vLocalOffset).Mag2() < rage::square(vehicle.GetBoundRadius()))
			{
				CPed* pPed = CGameWorld::FindLocalPlayer();
				if (pPed)
				{
					float fForceMultDueToPassengers = 1.0f;
					if(vehicle.GetNumberOfPassenger() > 0)
					{
						fForceMultDueToPassengers /= vehicle.GetNumberOfPassenger() + (vehicle.GetDriver() ? 1 : 0);
					}
					float fPedMass = pPed->GetMassForApplyVehicleForce();
					vehicle.ApplyImpulse(Vector3(0.0f,0.0f, fForceMultiplier * fPedMass * fForceMultDueToPassengers), vLocalOffset, 0, true );
					return true;
				}
			}
		}
	}
	return false;
}


bool CTaskVehicleFSM::RequestClipSetFromSeat(const CVehicle* pVehicle, s32 iSeatIndex)
{
	if (pVehicle && pVehicle->IsSeatIndexValid(iSeatIndex))
	{
		const CVehicleSeatAnimInfo* pSeatAnimInfo = pVehicle->GetSeatAnimationInfo(iSeatIndex);
		{
			if (taskVerifyf(pSeatAnimInfo, "NULL seat anim info for vehicle %s, seat %i", pVehicle->GetDebugName(), iSeatIndex))
			{
				if (!taskVerifyf(pSeatAnimInfo->GetSeatClipSetId() != CLIP_SET_ID_INVALID, "Invalid seat clipset for vehicle %s, seat %i", pVehicle->GetDebugName(), iSeatIndex))
				{
					return false;
				}

				fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pSeatAnimInfo->GetSeatClipSetId());
				if (!taskVerifyf(pClipSet, "Couldn't find clipset for vehicle %s, seat %i", pVehicle->GetDebugName(), iSeatIndex))
				{
					return false;
				}

				if (pClipSet->Request_DEPRECATED())
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool CTaskVehicleFSM::RequestClipSetFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex)
{
	if (pVehicle && pVehicle->IsEntryIndexValid(iEntryPointIndex))
	{
		const CVehicleEntryPointAnimInfo* pEntryAnimInfo = pVehicle->GetEntryAnimInfo(iEntryPointIndex);
		if (taskVerifyf(pEntryAnimInfo, "NULL entry anim info for vehicle %s, entry point %i", pVehicle->GetDebugName(), iEntryPointIndex))
		{
			if (!taskVerifyf(pEntryAnimInfo->GetEntryPointClipSetId() != CLIP_SET_ID_INVALID, "Invalid entry point clipset for vehicle %s, entry point %i", pVehicle->GetDebugName(), iEntryPointIndex))
			{
				return false;
			}

			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pEntryAnimInfo->GetEntryPointClipSetId());
			if (!taskVerifyf(pClipSet, "Couldn't find clipset vehicle %s, entry point %i", pVehicle->GetDebugName(), iEntryPointIndex))
			{
				return false;
			}

			if (pClipSet->Request_DEPRECATED())
			{
				return true;
			}
		}

	}
	return false;
}

fwMvClipSetId CTaskVehicleFSM::GetClipSetIdFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex)
{
	if (pVehicle && pVehicle->IsEntryIndexValid(iEntryPointIndex))
	{
		const CVehicleEntryPointAnimInfo* pEntryAnimInfo = pVehicle->GetEntryAnimInfo(iEntryPointIndex);
		if (taskVerifyf(pEntryAnimInfo, "NULL entry anim info for vehicle %s, entry point %i", pVehicle->GetDebugName(), iEntryPointIndex))
		{
			return pEntryAnimInfo->GetEntryPointClipSetId();
		}
	}
	return CLIP_SET_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskVehicleFSM::FindRearEntryForEntryPoint(CVehicle* pVehicle, s32 iEntryPointIndex)
{
	// Possibly worth caching the returned index?
	if (pVehicle && pVehicle->IsEntryIndexValid(iEntryPointIndex))
	{
		const CEntryExitPoint* pEntryExitPoint = pVehicle->GetEntryExitPoint(iEntryPointIndex);
		if (pEntryExitPoint)
		{
			s32 iSeatIndex = pEntryExitPoint->GetSeat(SA_directAccessSeat);
			if (pVehicle->IsSeatIndexValid(iSeatIndex))
			{
				const CVehicleSeatInfo* pSeatInfo = pVehicle->GetSeatInfo(iSeatIndex);
				if (pSeatInfo && pSeatInfo->GetRearLink())
				{
					s32 iNumSeats = pVehicle->GetSeatManager()->GetMaxSeats();
					for (s32 i=0; i<iNumSeats; ++i)
					{
						if (pVehicle->GetSeatInfo(i) == pSeatInfo->GetRearLink())
						{
							return pVehicle->GetDirectEntryPointIndexForSeat(i);
						}
					}
				}
			}
		}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskVehicleFSM::FindFrontEntryForEntryPoint(CVehicle* pVehicle, s32 iEntryPointIndex)
{
	// Possibly worth caching the returned index?
	if (pVehicle && pVehicle->IsEntryIndexValid(iEntryPointIndex))
	{
		const CEntryExitPoint* pEntryExitPoint = pVehicle->GetEntryExitPoint(iEntryPointIndex);
		if (pEntryExitPoint)
		{
			s32 iSeatIndex = pEntryExitPoint->GetSeat(SA_directAccessSeat);
			if (pVehicle->IsSeatIndexValid(iSeatIndex))
			{
				const CVehicleSeatInfo* pSeatInfo = pVehicle->GetSeatInfo(iSeatIndex);
				if (pSeatInfo)
				{
					s32 iNumSeats = pVehicle->GetSeatManager()->GetMaxSeats();
					for (s32 i=0; i<iNumSeats; ++i)
					{
						if (pVehicle->GetSeatInfo(i) && pVehicle->GetSeatInfo(i)->GetRearLink() == pSeatInfo)
						{
							return pVehicle->GetDirectEntryPointIndexForSeat(i);
						}
					}
				}
			}
		}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskVehicleFSM::GetEntryPointInFrontIfBeingUsed(CVehicle* pVehicle, s32 iThisEntryPointIndex, CPed* pEvaluatingPed)
{
	if (pVehicle)
	{
		s32 iFrontEntryIndex = CTaskVehicleFSM::FindFrontEntryForEntryPoint(pVehicle, iThisEntryPointIndex);
		TUNE_GROUP_BOOL(DOOR_INTERSECTION_TUNE, ALLOW_HALF_REAR_DOOR_DRIVING_IN_MP, true);
		if (pVehicle->IsEntryIndexValid(iFrontEntryIndex) && (ALLOW_HALF_REAR_DOOR_DRIVING_IN_MP || !NetworkInterface::IsGameInProgress()))
		{
			CComponentReservation* pComponentReservation = pVehicle->GetComponentReservationMgr()->GetDoorReservation(iFrontEntryIndex);
			if (pComponentReservation && pComponentReservation->GetPedUsingComponent() != NULL)
			{
				if (pComponentReservation->GetPedUsingComponent()->IsLocalPlayer())
				{
					const CTaskEnterVehicle* pEnterTask = static_cast<const CTaskEnterVehicle*>(pComponentReservation->GetPedUsingComponent()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
					if (pEnterTask && pEnterTask->IsFlagSet(CVehicleEnterExitFlags::HasJackedAPed))
					{
						return -1;
					}
				}
				else 
				{
					const CTaskExitVehicle* pExitTask = static_cast<const CTaskExitVehicle*>(pComponentReservation->GetPedUsingComponent()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE));
					if (pExitTask && pExitTask->IsFlagSet(CVehicleEnterExitFlags::IsFleeExit))
					{
						return -1;
					}
				}
				return iFrontEntryIndex;
			}

			TUNE_GROUP_BOOL(DOOR_INTERSECTION_TUNE, ALLOW_HALF_REAR_DOOR_DRIVING_WHEN_DOOR_NOT_CLEAR, true);
			if (pEvaluatingPed && pEvaluatingPed->IsAPlayerPed())
			{
				if (!CTaskCloseVehicleDoorFromInside::IsDoorClearOfPeds(*pVehicle, *pEvaluatingPed, iThisEntryPointIndex))
				{
					return iFrontEntryIndex;
				}
			}
		}
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::SetRemotePedUsingDoors(const CPed& rPed, CVehicle* pVeh, s32 iEntryPointIndex, bool bTrue)
{
	// Don't spam this function, SetPedRemoteUsingDoor is a toggle
	if (!pVeh)
		return;

	if (!rPed.IsNetworkClone())
		return;

	if (!pVeh->IsEntryIndexValid(iEntryPointIndex))
		return;

	CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(pVeh, iEntryPointIndex);
	if (pDoor && pDoor->GetIsIntact(pVeh))
	{
		pDoor->SetPedRemoteUsingDoor(bTrue);
#if __BANK
		vehicledoorDebugf2("CTaskVehicleFSM::SetRemotePedUsingDoors1-->SetPedRemoteUsingDoor(%s)", AILogging::GetBooleanAsString(bTrue));
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED_REF(rPed, "[VehicleDoors] - Door1 - SetPedRemoteUsingDoor %s, EntryPoint %i, DoorRatio %.2f\n", AILogging::GetBooleanAsString(bTrue), iEntryPointIndex, CTaskVehicleFSM::GetDoorRatioForEntryPoint(*pVeh, iEntryPointIndex));
#endif // __BANK
	}

	CCarDoor* pDoor2 = pVeh->GetSecondDoorFromEntryPointIndex(iEntryPointIndex);
	if (pDoor2 && pDoor2->GetIsIntact(pVeh))
	{
		pDoor2->SetPedRemoteUsingDoor(bTrue);
#if __BANK
		vehicledoorDebugf2("CTaskVehicleFSM::SetRemotePedUsingDoors2-->SetPedRemoteUsingDoor(%s)", AILogging::GetBooleanAsString(bTrue));
		AI_LOG_WITH_ARGS_IF_SCRIPT_OR_PLAYER_PED_REF(rPed, "[VehicleDoors] - Door2 - SetPedRemoteUsingDoor %s, EntryPoint %i\n", AILogging::GetBooleanAsString(bTrue), iEntryPointIndex);
#endif // __BANK
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ShouldAlterEntryPointForOpenDoor(const CPed& rPed, const CVehicle& rVeh, s32 iEntryPointIndex, s32 iEntryFlags)
{
	if (rPed.IsNetworkClone())
	{
		// Once we've picked our align point, don't change it
		const CTaskEnterVehicle* pEnterTask = static_cast<const CTaskEnterVehicle*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
		if (pEnterTask && pEnterTask->GetState() == CTaskEnterVehicle::State_Align)
		{
			if (pEnterTask->GetSubTask() && pEnterTask->GetSubTask()->GetState() == CTaskEnterVehicleAlign::State_OrientatedAlignment)
			{
				return pEnterTask->GetCachedShouldAlterEntryForOpenDoor();
			}
		}
	}

	const CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&rVeh, iEntryPointIndex);
	const bool bDontAlterForBrokenDoor = (MI_PLANE_BOMBUSHKA.IsValid() && rVeh.GetModelIndex() == MI_PLANE_BOMBUSHKA) || (MI_PLANE_VOLATOL.IsValid() && rVeh.GetModelIndex() == MI_PLANE_VOLATOL);
	const float fDoorConsideredOpenRatio = CVehicle::GetDoorRatioToConsiderDoorOpen(rVeh, (iEntryFlags & CModelSeatInfo::EF_IsCombatEntry) != 0);

	if (pDoor && !pDoor->GetIsIntact(&rVeh) && bDontAlterForBrokenDoor)
	{
		return false;
	}
	else if (pDoor && (!pDoor->GetIsIntact(&rVeh) || pDoor->GetDoorRatio() >= fDoorConsideredOpenRatio || (iEntryFlags & CModelSeatInfo::EF_ForceUseOpenDoor)))
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskVehicleFSM::GetDoorRatioForEntryPoint(const CVehicle& rVeh, s32 iEntryPointIndex)
{
	const CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&rVeh, iEntryPointIndex);
	if (!pDoor || !pDoor->GetIsIntact(&rVeh))
		return -1.0f;

	return pDoor->GetDoorRatio();
}

////////////////////////////////////////////////////////////////////////////////

float CTaskVehicleFSM::GetTargetDoorRatioForEntryPoint(const CVehicle& rVeh, s32 iEntryPointIndex)
{
	const CCarDoor* pDoor = CTaskVehicleFSM::GetDoorForEntryPointIndex(&rVeh, iEntryPointIndex);
	if (!pDoor || !pDoor->GetIsIntact(&rVeh))
		return -1.0f;

	return pDoor->GetTargetDoorRatio();
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ComputeIdealEndTransformRelative(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset)
{
	return ComputeRemainingAnimTransform(pClip, vPosOffset, qRotOffset, 0.0f);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ComputeIdealEndTransformRelative(const sClipBlendParams& clipBlendParams, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset)
{
	return ComputeRemainingAnimTransform(clipBlendParams, vPosOffset, qRotOffset);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ComputeCurrentAnimTransformRelative(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset, float fCurrentPhase)
{
	if (pClip)
	{
		vPosOffset = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.0f, fCurrentPhase));
		QuatV qStartRotation = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.0f));
		ScalarV scInitialHeadingOffset = -QuatVTwistAngle(qStartRotation, Vec3V(V_Z_AXIS_WONE));
		// Account for any initial z rotation
		vPosOffset = RotateAboutZAxis(vPosOffset, scInitialHeadingOffset);
		// Get the total z axis rotation over the course of the anim excluding the initial rotation
		QuatV qRotTotal = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 0.0f, fCurrentPhase));	
		ScalarV scTotalHeadingRotation = QuatVTwistAngle(qRotTotal, Vec3V(V_Z_AXIS_WONE));
		//vPosOffset = RotateAboutZAxis(vPosOffset, scTotalHeadingRotation);
		qRotOffset = QuatVFromZAxisAngle(scTotalHeadingRotation);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ComputeRemainingAnimTransform(const sClipBlendParams& clipBlendParams, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset)
{
	vPosOffset = Vec3V(V_ZERO);
	qRotOffset = QuatV(V_IDENTITY);

	const u32 uNumClips = clipBlendParams.apClips.GetCount();
	if (taskVerifyf(uNumClips > 0, "No clips found"))
	{
		// This doesn't handle blends between anims that rotation > 180 degrees
		float fTotalHeading = 0.0f;
		s32 iFirstNonZeroClip = -1;
		for (u32 i=0; i<uNumClips; ++i)
		{
			if (clipBlendParams.afBlendWeights[i] > 0.0f)
			{
				if (iFirstNonZeroClip == -1)
				{
					iFirstNonZeroClip = i;
				}
				const crClip* pClip = clipBlendParams.apClips[i];
				const float fPhase = clipBlendParams.afPhases[i];
				taskAssertf(pClip, "NULL Clip %i", i);
				Vec3V vClipOffset= VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, fPhase, 1.0f));
				QuatV qStartRotation = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.0f));
				ScalarV scInitialHeadingOffset = -QuatVTwistAngle(qStartRotation, Vec3V(V_Z_AXIS_WONE));
				// Account for any initial z rotation
				vClipOffset = RotateAboutZAxis(vClipOffset, scInitialHeadingOffset);
				// Get the total z axis rotation over the course of the anim excluding the initial rotation
				QuatV qClipOffset = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, clipBlendParams.afPhases[i], 1.0f));			
				// Accumulate each clip's offsets
				vPosOffset += ScalarVFromF32(clipBlendParams.afBlendWeights[i]) * vClipOffset;
				qRotOffset = Multiply(qRotOffset, Normalize(QuatVScaleAngle(qClipOffset, ScalarVFromF32(clipBlendParams.afBlendWeights[i]))));
			}
		}

		// Correct orientation for initial prediction
		TUNE_GROUP_BOOL(EXIT_TO_AIM_TUNE, CORRECT_PREDICTED_ORIENTATION, true);
		if (CORRECT_PREDICTED_ORIENTATION && iFirstNonZeroClip > -1 && iFirstNonZeroClip < uNumClips && clipBlendParams.afPhases[iFirstNonZeroClip] == 0.0f)
		{
			aiDebugf1("Computed Total Heading : %.4f", QuatVTwistAngle(qRotOffset, Vec3V(V_Z_AXIS_WONE)).Getf());
			const crClip* pClip1 = clipBlendParams.apClips[iFirstNonZeroClip];
			const crClip* pClip2 = clipBlendParams.apClips[iFirstNonZeroClip+1];
			const float fClip1BlendWeight = clipBlendParams.afBlendWeights[iFirstNonZeroClip];
			const float fClip2BlendWeight = clipBlendParams.afBlendWeights[iFirstNonZeroClip+1];
			QuatV qClip1Offset = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotationDiff(*pClip1, 0.0f, 1.0f));	
			QuatV qClip2Offset = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotationDiff(*pClip2, 0.0f, 1.0f));	
			const float fClip1Heading = QuatVTwistAngle(qClip1Offset, Vec3V(V_Z_AXIS_WONE)).Getf();
			const float fClip2Heading = QuatVTwistAngle(qClip2Offset, Vec3V(V_Z_AXIS_WONE)).Getf();
			aiDebugf1("Clip %i Heading : %.4f", iFirstNonZeroClip, fClip1Heading);
			aiDebugf1("Clip %i Heading : %.4f", iFirstNonZeroClip+1, fClip2Heading);
			// Handle anims that rotate more than 180 degrees, hackity hack.
			TUNE_GROUP_FLOAT(EXIT_TO_AIM_TUNE, ROTATION_TOLERANCE, 0.1f, 0.0f, 1.0f, 0.01f);
			if (Abs(fClip1Heading) > ROTATION_TOLERANCE && Abs(fClip2Heading) > ROTATION_TOLERANCE)
			{
				fTotalHeading = fwAngle::LimitRadianAngle0to2PiSafe(fClip1Heading) * fClip1BlendWeight + fwAngle::LimitRadianAngle0to2PiSafe(fClip2Heading) * fClip2BlendWeight;
			}
			else
			{
				fTotalHeading = fwAngle::LimitRadianAngle(fClip1Heading) * fClip1BlendWeight + fwAngle::LimitRadianAngle(fClip2Heading) * fClip2BlendWeight;
			}
			aiDebugf1("Total Heading : %.4f", fTotalHeading);
			fTotalHeading = fwAngle::LimitRadianAngle(fTotalHeading);
			qRotOffset = QuatVFromZAxisAngle(ScalarVFromF32(fTotalHeading));
		}
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ComputeRemainingAnimTransform(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset, float fCurrentPhase)
{
	if (pClip)
	{
		vPosOffset = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, fCurrentPhase, 1.0f));
		QuatV qStartRotation = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.0f));
		ScalarV scInitialHeadingOffset = -QuatVTwistAngle(qStartRotation, Vec3V(V_Z_AXIS_WONE));
		// Account for any initial z rotation
		vPosOffset = RotateAboutZAxis(vPosOffset, scInitialHeadingOffset);
		// Get the total z axis rotation over the course of the anim excluding the initial rotation
		QuatV qRotTotal = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, fCurrentPhase, 1.0f));	
		ScalarV scTotalHeadingRotation = QuatVTwistAngle(qRotTotal, Vec3V(V_Z_AXIS_WONE));
		//vPosOffset = RotateAboutZAxis(vPosOffset, scTotalHeadingRotation);
		qRotOffset = QuatVFromZAxisAngle(scTotalHeadingRotation);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(Vec3V_ConstRef vTargetPosWorldIn, QuatV_ConstRef qTargetRotWorldIn, Vec3V_Ref vPosOffsetInWorldPosOut, QuatV_Ref qRotOffsetInWorldRotOut)
{
	Mat34V targetMtx(V_IDENTITY);
	Mat34VFromQuatV(targetMtx, qTargetRotWorldIn, vTargetPosWorldIn);
	vPosOffsetInWorldPosOut = Transform(targetMtx, vPosOffsetInWorldPosOut);
	QuatV qRotParent = QuatVFromMat34V(targetMtx);
	qRotOffsetInWorldRotOut = Multiply(qRotOffsetInWorldRotOut, qRotParent);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::UnTransformFromWorldSpaceToRelative(const CVehicle& vehicle, Vec3V_ConstRef vTargetPosWorldIn, QuatV_ConstRef qTargetRotWorldIn, Vec3V_Ref vWorldInPosOffsetOut, QuatV_Ref qWorldInRotOffsetOut)
{
	// Compute offsets relative to the target transform
	Mat34V parentMtx(V_IDENTITY);
	Mat34VFromQuatV(parentMtx, qTargetRotWorldIn, vTargetPosWorldIn);
	vWorldInPosOffsetOut = UnTransformFull(parentMtx, vWorldInPosOffsetOut);
	QuatV qParentOrientation = Invert(QuatVFromMat34V(vehicle.GetTransform().GetMatrix()));
	qWorldInRotOffsetOut = Multiply(qParentOrientation, qWorldInRotOffsetOut);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ApplyGroundAndOrientationFixup(const CVehicle& vehicle, const CPed &ped, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut, float& fCachedGroundFixUpHeight, float fOverrideFixupHeight)
{
	// Remove the roll / pitch from orientation, we don't want dat shit when entering/exiting
	ScalarV scHeadingRotation = QuatVTwistAngle(qIdealRotOut, Vec3V(V_Z_AXIS_WONE));
	qIdealRotOut = QuatVFromZAxisAngle(scHeadingRotation);
	return ApplyGroundFixup(vehicle, ped, vIdealPosOut, fCachedGroundFixUpHeight, fOverrideFixupHeight);
}
////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ApplyGroundFixup(const CVehicle& vehicle, const CPed& ped, Vec3V_Ref vIdealPosOut, float& fCachedGroundFixUpHeight, float fOverrideFixupHeight)
{
	if (!IsClose(fCachedGroundFixUpHeight, CTaskVehicleFSM::INVALID_GROUND_HEIGHT_FIXUP, FLT_EPSILON))
	{
		vIdealPosOut.SetZf(fCachedGroundFixUpHeight);
		return false;
	}

	float fFixupHeight = fOverrideFixupHeight > -1.0f ? fOverrideFixupHeight : 2.5f;
	// Probe for a ground position to fixup to
	Vector3 vGroundPos(Vector3::ZeroType);
	if (CTaskVehicleFSM::FindGroundPos(&vehicle, &ped, VEC3V_TO_VECTOR3(vIdealPosOut), fFixupHeight, vGroundPos))
	{
		fCachedGroundFixUpHeight = vGroundPos.z;
		vIdealPosOut.SetZf(fCachedGroundFixUpHeight);
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::ApplyMoverFixup(CPed& ped, const CVehicle& vehicle, s32 iEntryPointIndex, const sClipBlendParams& clipBlendParams, float UNUSED_PARAM(fTimeStep), Vec3V_Ref vTargetOffset, QuatV_Ref qTargetRotOffset, bool bFixUpIfTagsNotFound)
{
	TUNE_BOOL(DISABLE_EXIT_VEHICLE_MOVER_FIXUP, false);
	if (DISABLE_EXIT_VEHICLE_MOVER_FIXUP || !clipBlendParams.apClips[0] || !vehicle.IsEntryIndexValid(iEntryPointIndex))
	{
		return false;
	}

	Vec3V vPredictedPos(V_ZERO);
	QuatV qPredictedRot(V_IDENTITY);

	// Compute the remaining animation in mover track for the blend of clips
	if (CTaskVehicleFSM::ComputeRemainingAnimTransform(clipBlendParams, vPredictedPos, qPredictedRot))
	{
		// Get the current attach offsets these are relative to our attach pos
		Vec3V vCurrentAttachOffset = VECTOR3_TO_VEC3V(ped.GetAttachOffset());
		Vec3V vPredictedAttachOffset = vCurrentAttachOffset;
		QuatV qCurrentAttachOffset = QUATERNION_TO_QUATV(ped.GetAttachQuat());
		QuatV qPredictedAttachOffset = qCurrentAttachOffset;

		// Compute where we think we'll be once the animation has finished
		qPredictedAttachOffset = Normalize(Multiply(qPredictedAttachOffset, qPredictedRot));
		vPredictedAttachOffset += vPredictedPos;

		// Get the world space attach transform
		Vec3V vAttachWorld(V_ZERO);
		QuatV qAttachWorld(V_IDENTITY);
		CTaskVehicleFSM::GetAttachWorldTransform(vehicle, iEntryPointIndex, vAttachWorld, qAttachWorld);

#if DEBUG_DRAW
		TUNE_BOOL(RENDER_PREDICTED_TRANSFORM, false);
		if (RENDER_PREDICTED_TRANSFORM)
		{
			Vec3V vDebugPredictedWorld = vPredictedAttachOffset;
			QuatV qDebugPredictedWorld = qPredictedAttachOffset;
			CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(vAttachWorld, qAttachWorld, vDebugPredictedWorld, qDebugPredictedWorld);
			CTaskAttachedVehicleAnimDebug::RenderMatrixFromVecQuat(vDebugPredictedWorld, qDebugPredictedWorld, 100, 0);
		}
#endif // DEBUG_DRAW


		// Compute target relative to our attach pos, we'll fix up in attach space
		CTaskVehicleFSM::UnTransformFromWorldSpaceToRelative(vehicle, vAttachWorld, qAttachWorld, vTargetOffset, qTargetRotOffset);

		// Compute the difference between predicted and desired attach offset
		Vec3V vError = vTargetOffset - vPredictedAttachOffset;

		//Displayf("Ideal Pos (%.2f,%.2f,%.2f) / Pred Pos (%.2f,%.2f,%.2f)", vIdealPos.GetXf(), vIdealPos.GetYf(), vIdealPos.GetZf(), vPredictedAttachOffset.GetXf(), vPredictedAttachOffset.GetYf(), vPredictedAttachOffset.GetZf());
		//Displayf("Error (%.2f,%.2f,%.2f)", vError.GetXf(), vError.GetYf(), vError.GetZf());

		QuatV qPredictedInv = Invert(qPredictedAttachOffset);
		QuatV qError = Normalize(Multiply(qPredictedInv, qTargetRotOffset));
		qError = PrepareSlerp(QuatV(V_IDENTITY), qError);

		s32 iFixUpClipIndex = 0; //FindClipIndexWithHighestBlendWeight(clipBlendParams); Don't really want to tag each anim right now...
		const crClip* pFixUpClip = clipBlendParams.apClips[iFixUpClipIndex];
		const float fFixUpClipPhase = clipBlendParams.afPhases[iFixUpClipIndex];

		// TODO: Should this be taken from the most blended in anim, or a weighted average?
		sMoverFixupInfo moverFixupInfo;
		GetMoverFixupInfoFromClip(moverFixupInfo, pFixUpClip, vehicle.GetVehicleModelInfo()->GetHashKey());
		
		// Translation fixup is a percentage of the total translation error, when ratio is one, we eliminate all the error
		Vec3V vFixup(V_ZERO);
		if (moverFixupInfo.bFoundXFixup || bFixUpIfTagsNotFound)
			vFixup.SetXf(ComputeTranslationFixUpThisFrame(vError.GetXf(), fFixUpClipPhase, moverFixupInfo.fXFixUpStartPhase, moverFixupInfo.fXFixUpEndPhase));
		if (moverFixupInfo.bFoundYFixup || bFixUpIfTagsNotFound)
			vFixup.SetYf(ComputeTranslationFixUpThisFrame(vError.GetYf(), fFixUpClipPhase, moverFixupInfo.fYFixUpStartPhase, moverFixupInfo.fYFixUpEndPhase));
		if (moverFixupInfo.bFoundZFixup || bFixUpIfTagsNotFound)
			vFixup.SetZf(ComputeTranslationFixUpThisFrame(vError.GetZf(), fFixUpClipPhase, moverFixupInfo.fZFixUpStartPhase, moverFixupInfo.fZFixUpEndPhase));

		// Add the fixup to current offset
		vCurrentAttachOffset = vCurrentAttachOffset + vFixup;

		// Rotation fixup is a percentage of the total rotation error, when ratio is one, we eliminate all the error
		QuatV qFixup(V_IDENTITY);
		if (moverFixupInfo.bFoundRotFixup)
			qFixup = ComputeOrientationFixUpThisFrame(qError, fFixUpClipPhase, moverFixupInfo.fRotFixUpStartPhase, moverFixupInfo.fRotFixUpEndPhase);

		VecBoolV bIsFinite = IsFinite(qFixup);
		if (!taskVerifyf(IsEqualIntAll(bIsFinite, VecBoolV(V_T_T_T_T)), "QNAN detected, check ComputeOrientationFixUpThisFrame for failure qError (%.2f, %.2f, %.2f, %.2f)", qError.GetXf(), qError.GetYf(), qError.GetZf(), qError.GetWf()))
		{
			return false;
		}

		// Add the fixup to current offset
		qCurrentAttachOffset = Normalize(Multiply(qCurrentAttachOffset, qFixup));

		// Re-apply offsets
		TUNE_BOOL(APPLY_TRANS_FIXUP, true);
		if (APPLY_TRANS_FIXUP) 
			ped.SetAttachOffset(VEC3V_TO_VECTOR3(vCurrentAttachOffset));

		TUNE_BOOL(APPLY_ROT_FIXUP, true);
		if (APPLY_ROT_FIXUP) 
			ped.SetAttachQuat(QUATV_TO_QUATERNION(qCurrentAttachOffset));

		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

s32 CTaskVehicleFSM::FindClipIndexWithHighestBlendWeight(const sClipBlendParams& clipBlendParams)
{
	float fHighestBlendWeight = 0.0f;
	s32 iHighestBlendWeightClipIndex = 0;

	for (s32 i=0; i<clipBlendParams.apClips.GetCount(); ++i)
	{
		if (clipBlendParams.afBlendWeights[i] > fHighestBlendWeight)
		{
			fHighestBlendWeight = clipBlendParams.afBlendWeights[i];
			iHighestBlendWeightClipIndex = i;
		}
	}
	return iHighestBlendWeightClipIndex;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::GetMoverFixupInfoFromClip(sMoverFixupInfo& moverFixupInfo, const crClip* pClip, u32 uVehicleModelHash)
{
	if (pClip->GetTags())
	{
		CClipEventTags::CTagIterator<CClipEventTags::CMoverFixupEventTag> it1(*pClip->GetTags(), CClipEventTags::MoverFixup);

		bool bHasSpecificFixupTags = false;
		TUNE_GROUP_BOOL(FIRST_PERSON_VEHICLE_TUNE, ENABLE_VEHICLE_SPECIFIC_MOVER_FIXUPS, true);
		if (ENABLE_VEHICLE_SPECIFIC_MOVER_FIXUPS)
		{
			while (*it1)
			{
				const CClipEventTags::CMoverFixupEventTag* pTag = (*it1)->GetData();
				const u32 uSpecificVehicleModelHashToApplyFixUpTo = pTag->GetSpecificVehicleModelHashToApplyFixUpTo();
				if (uSpecificVehicleModelHashToApplyFixUpTo != 0 && uVehicleModelHash == uSpecificVehicleModelHashToApplyFixUpTo)
				{
					bHasSpecificFixupTags = true;
					break;
				}
				else
				{
					++it1;
				}
			}
		}

		CClipEventTags::CTagIterator<CClipEventTags::CMoverFixupEventTag> it (*pClip->GetTags(), CClipEventTags::MoverFixup);
		while (*it)
		{
			const CClipEventTags::CMoverFixupEventTag* pTag = (*it)->GetData();
			const u32 uSpecificVehicleModelHashToApplyFixUpTo = pTag->GetSpecificVehicleModelHashToApplyFixUpTo();
			bool bUseThisTag = false;
			if (bHasSpecificFixupTags)
			{
				if (uSpecificVehicleModelHashToApplyFixUpTo == uVehicleModelHash)
				{
					bUseThisTag = true;
				}
			}
			else if (uSpecificVehicleModelHashToApplyFixUpTo == 0)
			{
				bUseThisTag = true;
			}

			if (bUseThisTag)
			{
				if (pTag->ShouldFixupTranslation())
				{
					if (pTag->ShouldFixupX())
					{
#if __BANK
						if (!moverFixupInfo.bFoundXFixup)
							taskDebugf3("Duplicate X translation tags found in clip %s, using trans x start (%.2f) / end (%.2f)", pClip->GetName(), (*it)->GetStart(), (*it)->GetEnd());
#endif // __BANK
						moverFixupInfo.fXFixUpStartPhase = (*it)->GetStart();
						moverFixupInfo.fXFixUpEndPhase = (*it)->GetEnd();
						moverFixupInfo.bFoundXFixup = true;
					}
					if (pTag->ShouldFixupY())
					{
#if __BANK
						if (!moverFixupInfo.bFoundYFixup)
							taskDebugf3("Duplicate Y translation tags found in clip %s, using trans y start (%.2f) / end (%.2f)", pClip->GetName(), (*it)->GetStart(), (*it)->GetEnd());
#endif // __BANK
						moverFixupInfo.fYFixUpStartPhase = (*it)->GetStart();
						moverFixupInfo.fYFixUpEndPhase = (*it)->GetEnd();
						moverFixupInfo.bFoundYFixup = true;
					}
					if (pTag->ShouldFixupZ())
					{
#if __BANK
						if (!moverFixupInfo.bFoundZFixup)
							taskDebugf3("Duplicate Z translation tags found in clip %s, using trans z start (%.2f) / end (%.2f)", pClip->GetName(), (*it)->GetStart(), (*it)->GetEnd());
#endif // __BANK
						moverFixupInfo.fZFixUpStartPhase = (*it)->GetStart();
						moverFixupInfo.fZFixUpEndPhase = (*it)->GetEnd();
						moverFixupInfo.bFoundZFixup = true;
					}
				}

				if (pTag->ShouldFixupRotation())
				{
#if __BANK
					if (!moverFixupInfo.bFoundRotFixup)
						taskDebugf3("Duplicate rotation tags found in clip %s, using rot start (%.2f) / end (%.2f)", pClip->GetName(), (*it)->GetStart(), (*it)->GetEnd());
#endif // __BANK
					moverFixupInfo.fRotFixUpStartPhase = (*it)->GetStart();
					moverFixupInfo.fRotFixUpEndPhase = (*it)->GetEnd();
					moverFixupInfo.bFoundRotFixup = true;
				}
			}
			++it;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

float CTaskVehicleFSM::ComputeTranslationFixUpThisFrame(float fTotalFixUp, float fClipPhase, float fStartPhase, float fEndPhase)
{
	float fExtraFixUp = 0.0f;

	// Do this only after the initial translation fixup has been applied
	if (fClipPhase > fStartPhase && fClipPhase < fEndPhase)
	{
		// Cut off the start phase
		const float fTempClipPhase = fClipPhase - fStartPhase;
		const float fTempFixUpRange = fEndPhase - fStartPhase;

		const float fExtraRatio = rage::Min(fTempClipPhase / fTempFixUpRange,1.0f);
		fExtraFixUp = (fExtraRatio * fTotalFixUp);
	}
	else if (fClipPhase >= fEndPhase)
	{
		fExtraFixUp = fTotalFixUp;
	}
	// else Do Nothing

	return fExtraFixUp;
}

////////////////////////////////////////////////////////////////////////////////

QuatV CTaskVehicleFSM::ComputeOrientationFixUpThisFrame(QuatV_ConstRef qError, float fClipPhase, float fStartPhase, float fEndPhase)
{
	//Vector3 vErrorFixup(Vector3::ZeroType);
	//QUATV_TO_QUATERNION(qError).ToEulers(vErrorFixup, "xyz");
	//Displayf("Rot Error Total - X : %.2f, Y : %.2f, Z : %.2f", vErrorFixup.x, vErrorFixup.y, vErrorFixup.z);

	QuatV qOrientationFixup(V_IDENTITY);

	if (fClipPhase > fStartPhase)
	{			
		// Cut off the start phase
		const float fTempClipPhase = fClipPhase - fStartPhase;
		const float fTempFixUpRange = fEndPhase - fStartPhase;
		const float fLerpRatio = rage::Min(fTempClipPhase / fTempFixUpRange, 1.0f);
		//Displayf("Lerp Ratio : %.2f", fLerpRatio);
		ScalarV scLerpRatio = ScalarVFromF32(fLerpRatio);

		// Rotation fixup is a percentage of the total rotation error, when ratio is one, we eliminate all the error
		// Seem to get qnans if these are really close, should slerp near handle this?
		TUNE_FLOAT(SLERP_TOL, 0.01f, 0.0f, 0.5f, 0.01f);
		if (IsCloseAll(qOrientationFixup, qError, ScalarVFromF32(SLERP_TOL)))
		{
			qOrientationFixup = qError;
		}
		else
		{
			qOrientationFixup = SlerpNear(scLerpRatio, qOrientationFixup, qError);
			qOrientationFixup = Normalize(qOrientationFixup);
		}
		//Displayf("Scaled Angle : %.2f", GetAngle(qOrientationFixup).Getf());
	}
	// else Do Nothing

	return qOrientationFixup;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::GetAttachWorldTransform(const CVehicle& vehicle, s32 iEntryPointIndex, Vec3V_Ref vAttachWorld, QuatV_Ref qAttachWorld)
{
	// Compute attach matrix, in this case we always attach to a seat node, we untransform relative to this
	Vector3 vPos; Quaternion qRot;
	CModelSeatInfo::CalculateSeatSituation(&vehicle, vPos, qRot, iEntryPointIndex);
	vAttachWorld = RCC_VEC3V(vPos); qAttachWorld = RCC_QUATV(qRot);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::CanPedWarpIntoHoveringVehicle(const CPed* pPed, const CVehicle& rVeh, s32 iEntryIndex, bool bTestPedHeightToSeat)
{
	bool bPassesPedTest = !pPed || pPed->IsLocalPlayer();

	if (NetworkInterface::IsGameInProgress() && bPassesPedTest && rVeh.InheritsFromHeli() && rVeh.GetDriver())
	{
		// Needs to be a player ped driving
		if (!rVeh.GetDriver()->IsAPlayerPed())
		{
			return false;
		}

		// If no index is passed in (we may not know yet), pass the test
		if (!rVeh.IsEntryIndexValid(iEntryIndex))
		{
			return true;
		}

		const s32 iSeatIndex = rVeh.GetEntryExitPoint(iEntryIndex)->GetSeat(SA_directAccessSeat);
		if (rVeh.IsSeatIndexValid(iSeatIndex))
		{
			const CVehicleSeatInfo* pSeatInfo = rVeh.GetSeatInfo(iSeatIndex);
			if (pSeatInfo && !pSeatInfo->GetIsFrontSeat())
			{
				// Don't warp in if theres a chance we can anim succesfully in
				if (bTestPedHeightToSeat && pPed)
				{
					Vector3 vSeatPosition(Vector3::ZeroType);
					Quaternion qSeatOrientation(Quaternion::IdentityType);
					CModelSeatInfo::CalculateSeatSituation(&rVeh, vSeatPosition, qSeatOrientation, iEntryIndex, iSeatIndex);
					
					const float fSeatHeight = vSeatPosition.z;
					const float fPedHeight = pPed->GetTransform().GetPosition().GetZf();
					const float fZDiffToSeat = fSeatHeight - fPedHeight;
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, REAR_HELI_WARP_DIFF, 1.0f, -10.0f, 10.0f, 0.01f);
					TUNE_GROUP_FLOAT(ENTER_VEHICLE_TUNE, REAR_HELI_WARP_DIFF_HIGH, 1.5f, -10.0f, 10.0f, 0.01f);
					bool bShouldUseHighDiff = (MI_HELI_HUNTER.IsValid() && rVeh.GetModelIndex() == MI_HELI_HUNTER) || (MI_HELI_AKULA.IsValid() && rVeh.GetModelIndex() == MI_HELI_AKULA);
					const float fMaxDiff = bShouldUseHighDiff ? REAR_HELI_WARP_DIFF_HIGH : REAR_HELI_WARP_DIFF;
					if (fZDiffToSeat < fMaxDiff)
					{
						return false;
					}
					
				}
				return true;
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::VehicleContainsFriendlyPassenger( const CPed* pPed, const CVehicle* pVehicle )
{
	for( s32 iSeat = 0; iSeat < pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++ )
	{
		CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
		if( pPassenger && pPassenger->PopTypeIsMission() &&
			ArePedsFriendly(pPassenger, pPed) )
		{
			if( pPassenger->GetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar ) ||
				(pPed->IsControlledByLocalOrNetworkPlayer() && pPassenger->GetPedConfigFlag( CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar )) )
			{
				return true;
			}
		}
	}
	return false;
}

//-------------------------------------------------------------------------
// Returns true if this ped can use an entry point to a vehicle giving direct access
// to the seat the other ped is in, pOtherPed can be NULL.
//-------------------------------------------------------------------------
bool CTaskVehicleFSM::CanUsePedEntryPoint( const CPed* pPed, const CPed* pOtherPed, VehicleEnterExitFlags iFlags )
{
    return CanDisplacePed(pPed, pOtherPed, iFlags, true);
}

//-------------------------------------------------------------------------
// Returns true if this ped can jack the other ped in order to get to a target seat
//-------------------------------------------------------------------------
bool CTaskVehicleFSM::CanJackPed( const CPed* pPed, const CPed* pOtherPed, VehicleEnterExitFlags iFlags )
{
    return CanDisplacePed(pPed, pOtherPed, iFlags, false);
}

//-------------------------------------------------------------------------
// Returns true if this ped can displace the other ped, pOtherPed can be NULL
//-------------------------------------------------------------------------
bool CTaskVehicleFSM::CanDisplacePed( const CPed* pPed, const CPed* pOtherPed, VehicleEnterExitFlags iFlags, bool doEnterExitChecks )
{
	if (!aiVerifyf(pPed, "CTaskVehicleFSM::CanDisplacePed called with NULL ped"))
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s (NULL) can't displace %s\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	if( pOtherPed == NULL ||
		pPed == pOtherPed )
	{
		return true;
	}

	if( iFlags.BitSet().IsSet(CVehicleEnterExitFlags::DontJackAnyone) )
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - DontJackAynone\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}
	
	bool bDontDragOtherPedOut = pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar ) ||
								(pPed->IsControlledByLocalOrNetworkPlayer() && pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PlayersDontDragMeOutOfCar ));

	if( bDontDragOtherPedOut && !pOtherPed->IsInjured() )		
	{
        if( !doEnterExitChecks ||
            ( !pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
		      !pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) ) )
		{
			AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - bDontDragOtherPedOut. pOtherPed->DontDragMeOutCar - %s\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed),AILogging::GetBooleanAsString(pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_DontDragMeOutCar )));
			return false;
		}
	}

	// Cant jack people on trains
	if( pOtherPed && pOtherPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pOtherPed->GetMyVehicle() && 
		pOtherPed->GetMyVehicle()->InheritsFromTrain() )
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - Don't jack from trains\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	// Can't jack RCs
	if(pOtherPed->GetMyVehicle() && pOtherPed->GetMyVehicle()->pHandling->mFlags & MF_IS_RC)
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - Don't jack from RCs\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	// Cant jack a ped if we are handcuffed.
	if(pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - CPED_CONFIG_FLAG_IsHandCuffed\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	// Cant jack a ped if we are not allowed to jack anyone.
	if(pOtherPed->IsPlayer() && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_NotAllowedToJackAnyPlayers))
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - CPED_CONFIG_FLAG_NotAllowedToJackAnyPlayers\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	// Cops can't jack other peds.
	if( pOtherPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsInCustody) && pPed->GetPedType() == PEDTYPE_COP )
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - CPED_CONFIG_FLAG_IsInCustody\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	// In MP interactions are disabled between certain players (ghosted players in passive mode, etc)
	if (NetworkInterface::IsGameInProgress() && NetworkInterface::AreInteractionsDisabledInMP(*pPed, *pOtherPed))
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - NetworkInterface::AreInteractionsDisabledInMP\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	// Force jack even if the ped is friendly
	if ( (iFlags.BitSet().IsSet(CVehicleEnterExitFlags::JackIfOccupied) ) )
	{
        return true;
	}

	// For MP races script can force player jacks regardless of relationships
	if(NetworkInterface::IsGameInProgress() && pPed->IsPlayer() && pOtherPed->IsPlayer() 
		&& pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_WillJackAnyPlayer))
	{
		return true;
	}

	CPedGroup* pPedGroup = pPed->GetPedsGroup();

	const bool bConsiderJackingFriendlyPed = iFlags.BitSet().IsSet(CVehicleEnterExitFlags::ConsiderJackingFriendlyPeds);
	
	// If we're not considering jacking particular peds, but the ped can drive us as a passenger, don't let us jack them
	if (!bConsiderJackingFriendlyPed &&
		pPed->IsLocalPlayer() &&
		pOtherPed->GetPedConfigFlag(CPED_CONFIG_FLAG_AICanDrivePlayerAsRearPassenger) && 
		pOtherPed->GetMyVehicle() && 
		pOtherPed->GetMyVehicle()->GetSeatManager()->GetDriver() == pOtherPed &&
		!pPed->GetPedIntelligence()->IsThreatenedBy(*pOtherPed))
	{
		AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - CPED_CONFIG_FLAG_AICanDrivePlayerAsRearPassenger\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
		return false;
	}

	// peds can't jack other friendly peds to get to a seat, unless we choose to in mp or trying to get into a taxi in sp
	if( !pOtherPed->IsInjured() && 
		!bConsiderJackingFriendlyPed &&
		(ArePedsFriendly(pPed, pOtherPed) || (pPedGroup && pPedGroup->GetGroupMembership()->IsMember(pOtherPed))))
	{
        if( !doEnterExitChecks ||
            ( !pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) &&
		      !pOtherPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_EXIT_VEHICLE) ) )
        {
			AI_LOG_WITH_ARGS("[EntryExit][CanDisplacePed] %s can't displace %s - Don't jack friendly peds to get to a seat\n",AILogging::GetDynamicEntityNameSafe(pPed), AILogging::GetDynamicEntityNameSafe(pOtherPed));
			return false;
        }
	}

	return true;
}

//-------------------------------------------------------------------------
// Returns true if the peds are friendly or share the same custody group
//-------------------------------------------------------------------------
bool CTaskVehicleFSM::ArePedsFriendly(const CPed* pPed, const CPed* pOtherPed)
{
	if (pPed->GetPedIntelligence()->IsFriendlyWith(*pOtherPed))
	{
		return true;
	}

	CPed* pCustodian = pPed->GetCustodian();
	CPed* pOtherCustodian = pOtherPed->GetCustodian();

	if (pCustodian)
	{
		if ((pCustodian == pOtherCustodian)
			|| pOtherPed->GetPedIntelligence()->IsFriendlyWith(*pCustodian))
		{
			return true;
		}
	}

	if (pOtherCustodian)
	{
		if (pPed->GetPedIntelligence()->IsFriendlyWith(*pOtherCustodian))
		{
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// Helper function to retrieve clip set request helper for a ped.
//-------------------------------------------------------------------------
const CVehicleClipRequestHelper *CTaskVehicleFSM::GetVehicleClipRequestHelper(const CPed *pPed)
{
	if(pPed->IsLocalPlayer())
	{
		return &pPed->GetPlayerInfo()->GetVehicleClipRequestHelper();
	}
	else
	{
		const CTaskMotionInVehicle* pMotionTask = static_cast<const CTaskMotionInVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveMotionByType(CTaskTypes::TASK_MOTION_IN_VEHICLE));
		if (pMotionTask)
		{
			return &pMotionTask->GetVehicleRequestHelper();
		}
	}

	return NULL;
}

//-------------------------------------------------------------------------
// Returns true if the given seat is valid
//-------------------------------------------------------------------------
SeatValidity CTaskVehicleFSM::GetSeatValidity( SeatRequestType seatRequestType, s32 iSeatRequested, s32 iSeatToValidate )
{
	// Setup the preferred seats
	bool bSeatPreferred = (iSeatRequested == iSeatToValidate);

	if( bSeatPreferred )
	{
		return SV_Preferred;
	}
	else if( seatRequestType == SR_Any || seatRequestType == SR_Prefer )
	{
		return SV_Valid;
	}
	return SV_Invalid;
}



u32 CTaskVehicleFSM::TIME_LAST_PLAYER_GROUP_MEMBER_ENTERED = 0;
static float MP_NEARBY_DRIVER_PRIORITY_DISTANCE = 7.5f;

bool CTaskVehicleFSM::IsAnyGroupMemberUsingEntrypoint( const CPed* pPed, const CVehicle* pVehicle, s32 iEntryPoint, VehicleEnterExitFlags UNUSED_PARAM(iFlags), const bool bOnlyConsiderIfCloser, const Vector3& vEntryPos, int* piActualEntryPointUsed )
{
	// Build up some information about the vehicle to use later to resolve conflicts
	s32 iDirectDriverEntryPoint = -1;
	s32 iIndirectDriverEntryPoint = -1;
	bool bDriverSeatOccupied = false;
	if( pVehicle )
	{
		aiAssert(pVehicle->GetVehicleModelInfo());
		s32 iDriverSeat = pVehicle->GetDriverSeat();
		const CPed* pDriverSeatOccupier = pVehicle->GetSeatManager()->GetPedInSeat(iDriverSeat);
		bDriverSeatOccupied = pDriverSeatOccupier != NULL && ArePedsFriendly(pPed, pDriverSeatOccupier);
		const CModelSeatInfo* pModelSeatInfo = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if( pModelSeatInfo )
		{
			iDirectDriverEntryPoint = pVehicle->GetDirectEntryPointIndexForSeat(iDriverSeat);
			iIndirectDriverEntryPoint = pVehicle->GetIndirectEntryPointIndexForSeat(iDriverSeat);
		}
	}
	s32 iThisPedsState = 0;
	if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
	{
		iThisPedsState = pPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
	}

	// PH - Debug code to try and diagnose enter car issues.
#if 0
	static dev_s32 iDebugEntryPoint = 0;
	static dev_s32 iAltDebugEntryPoint = 0;
	static dev_bool bFakeEntryPointUsage = false;
	static dev_bool bForceDriverSeatOccupied = false;
	if( bFakeEntryPointUsage )
	{
		if( (iEntryPoint == iDirectDriverEntryPoint && iDebugEntryPoint == iIndirectDriverEntryPoint) ||
			(iEntryPoint == iIndirectDriverEntryPoint && iDebugEntryPoint == iIndirectDriverEntryPoint && (bDriverSeatOccupied||bForceDriverSeatOccupied) ) ||
			iEntryPoint!=iIndirectDriverEntryPoint && iDebugEntryPoint == iEntryPoint )
		{
			return true;
		}
		else if( (iEntryPoint == iDirectDriverEntryPoint && iAltDebugEntryPoint == iIndirectDriverEntryPoint) ||
			(iEntryPoint == iIndirectDriverEntryPoint && iAltDebugEntryPoint == iIndirectDriverEntryPoint && (bDriverSeatOccupied||bForceDriverSeatOccupied) ) ||
			iEntryPoint!=iIndirectDriverEntryPoint && iAltDebugEntryPoint == iEntryPoint )
		{
			return true;
		}

	}
#endif


	// Make sure no other friendly ped is trying to enter it
	const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
	for( const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
	{
		const CPed* pNearbyPed = static_cast<const CPed*>(pEnt);
		if( pNearbyPed != pPed )
		{
			// In MP games, if another player is trying to use the same door and is closer, we use this to prioritise another door
			if( pPed->IsLocalPlayer() )
			{
				if( pNearbyPed->IsAPlayerPed() )
				{
					if( ArePedsFriendly(pPed, pNearbyPed) )
					{
						if( pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
						{
							CEntity* pTargetEntity = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
							if( pTargetEntity == pVehicle )
							{
								s32 iOtherPedsState = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetStateForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
								// If we're trying to get in the driver seat but this ped is going through the other door, consider them for prioritising
								// If we're trying to get in the passenger seat and they are likely to stay in the passengers seat, consider them for prioritising
								// Otherwise if we're just trying to get in any seat, consider them for prioritising
								s32 iTargetEntryPoint = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetEntryPointForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
								if( piActualEntryPointUsed )
								{
									*piActualEntryPointUsed = iTargetEntryPoint;
								}

								const bool bOtherPedEnteringIndirectly = (iEntryPoint == iDirectDriverEntryPoint && iTargetEntryPoint == iIndirectDriverEntryPoint);
								Vector3 vVehicleToDriverSeat = vEntryPos - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
								const bool bDriverSeatOnLeft = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()).Dot(vVehicleToDriverSeat) <= 0.0f;
								Vector3 vVehicleToPed = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
								const bool bPedOnLeft = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()).Dot(vVehicleToPed) <= 0.0f;
								const float fDistPedToSeatSq =  VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist2(vEntryPos);
								if( (iEntryPoint == iDirectDriverEntryPoint && iTargetEntryPoint == iIndirectDriverEntryPoint) ||
									(iEntryPoint == iIndirectDriverEntryPoint && iTargetEntryPoint == iIndirectDriverEntryPoint && bDriverSeatOccupied ) ||
									iTargetEntryPoint == iEntryPoint )
								{
									bool bNearbyPedHasPriority = false;
									TUNE_GROUP_BOOL(MP_VEHICLE_ENTRY_SELECTION, ENABLE_NEW_PREDICTION, true);
									if (ENABLE_NEW_PREDICTION && !bOtherPedEnteringIndirectly)
									{
										const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
										if (pEnterVehicleTask &&( iOtherPedsState == CTaskEnterVehicle::State_GoToDoor))
										{
											const float fTimeNearbyPedGoingToDoor = pEnterVehicleTask->GetTimeInState();
											if (fTimeNearbyPedGoingToDoor > ms_Tunables.m_MinTimeToConsiderPedGoingToDoorPriority && fTimeNearbyPedGoingToDoor < ms_Tunables.m_MaxTimeToConsiderPedGoingToDoorPriority)
											{
												if (VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition()).Dist2(vEntryPos) < square(ms_Tunables.m_MaxDistToConsiderPedGoingToDoorPriority))
												{
													bNearbyPedHasPriority = true;
												}
											}
										}
									}

									if( !bOnlyConsiderIfCloser )
									{
										return true;
									}
									else if (bNearbyPedHasPriority)
									{
										return true;
									}
									// If the other ped is directly entering too, base the priority on state and/or distance
									else if( !bOtherPedEnteringIndirectly &&
											  ( ( iOtherPedsState > CTaskEnterVehicle::State_GoToDoor && 
												iOtherPedsState <= CTaskEnterVehicle::State_ShuffleToSeat&& 
												iThisPedsState <= CTaskEnterVehicle::State_GoToDoor ) ||
												( VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition()).Dist2(vEntryPos) < 
												VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist2(vEntryPos) ) ) )
									{
										return true;
									}
									// If the other ped needs to shuffle, this ped takes priority if he is nearby the car
									else if( bOtherPedEnteringIndirectly &&
												( ( iOtherPedsState == CTaskEnterVehicle::State_ShuffleToSeat &&
												iThisPedsState <= CTaskEnterVehicle::State_GoToDoor ) ||
												( fDistPedToSeatSq > rage::square(MP_NEARBY_DRIVER_PRIORITY_DISTANCE ) ||
												bDriverSeatOnLeft != bPedOnLeft ) ) )
									{
										return true;
									}
								}
							}
						}
					}
				}
			}
			else if( ArePedsFriendly(pPed, pNearbyPed) )
			{
				if( pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
				{
					CEntity* pTargetEntity = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
					if( pTargetEntity == pVehicle )
					{
						const s32 iTargetEntryPoint = CTaskVehicleFSM::GetEntryPedIsTryingToEnter(pNearbyPed);

						// Allow selection of entry points that are only transitively used
						bool bIsLeaderPedGoingToDirectSeat = false;
						const CPed* pLeader = GetPedsGroupPlayerLeader(*pPed);
						if (pLeader == pNearbyPed)
						{
							const s32 iSeatIndex = CTaskVehicleFSM::GetSeatPedIsTryingToEnter(pNearbyPed);
							const s32 iDirectSeatIndex = pVehicle->GetEntryExitPoint(iEntryPoint)->GetSeat(SA_directAccessSeat);
							bIsLeaderPedGoingToDirectSeat = (iSeatIndex == iDirectSeatIndex) ? true : false;
						}

						if (iTargetEntryPoint == iEntryPoint && bIsLeaderPedGoingToDirectSeat)
						{
							if( piActualEntryPointUsed )
							{
								*piActualEntryPointUsed = iTargetEntryPoint;
							}
							return true;
						}
					}
				}
			}
		}
	}
	return false;
}


bool CTaskVehicleFSM::CheckPlayerExitConditions(CPed& ped, CVehicle& vehicle, VehicleEnterExitFlags iFlags, float fTimeRunning, bool bExiting, Vector3 *pvTestPosOverride)
{
	if (!ped.IsLocalPlayer())
	{
		return false;
	}

	if (!(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::PlayerControlledVehicleEntry)) && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_AllowPlayerToInterruptVehicleEntryExit))
	{
		return false;
	}

	CControl* pControl = ped.GetControlFromPlayer();

	// Abort if X is pressed.
	if( pControl->GetPedJump().IsPressed() )
	{
		return true;
	}

	// Only abort if a certain time has passed
	if (fTimeRunning < MIN_TIME_BEFORE_PLAYER_CAN_ABORT)
	{
		return false;
	}

	Vector3 vTestPos = VEC3V_TO_VECTOR3(vehicle.GetTransform().GetPosition());
	if (vehicle.IsEntryIndexValid(m_iTargetEntryPoint))
	{
		if(vehicle.GetEntryInfo(m_iTargetEntryPoint))
		{
			const CVehicleEntryPointInfo *pEntryInfo = vehicle.GetEntryInfo(m_iTargetEntryPoint);
			if(pEntryInfo && pEntryInfo->GetBreakoutTestPoint() != CExtraVehiclePoint::POINT_TYPE_MAX)
			{
				Quaternion qNewTargetOrientation(0.0f,0.0f,0.0f,1.0f);
				CTaskVehicleFSM::ComputeTargetTransformInWorldSpace(*m_pVehicle, vTestPos, qNewTargetOrientation, m_iTargetEntryPoint, pEntryInfo->GetBreakoutTestPoint());
			}
		}
	}

	Vec3V vVehTestPos = pvTestPosOverride ? VECTOR3_TO_VEC3V(*pvTestPosOverride) : VECTOR3_TO_VEC3V(vTestPos);

	Vector3 vAwayFromVehicle = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition() - vVehTestPos);
	float fDist = vAwayFromVehicle.Mag();

	// if this is a boat, reduce the distance by the bounding radius (cos some of them are massive)
	if (vehicle.GetVehicleType() == VEHICLE_TYPE_BOAT || vehicle.GetVehicleType() == VEHICLE_TYPE_SUBMARINE || vehicle.GetVehicleType() == VEHICLE_TYPE_PLANE)
	{
		fDist -= vehicle.GetBoundRadius();
	}

	// Check the disance to the vehicle isn't too far.
	if (fDist > CPlayerInfo::SCANNEARBYVEHICLES*1.5f)
	{
		return true;
	}

	float fTaskHeading=fwAngle::GetRadianAngleBetweenPoints(vAwayFromVehicle.x,vAwayFromVehicle.y,0.0f,0.0f);
	fTaskHeading = fwAngle::LimitRadianAngle(fTaskHeading - PI);
	float fCarHeading=vehicle.GetTransform().GetHeading();

	Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
	float fInputDirn = rage::Atan2f(-vecStick.x, vecStick.y) + camInterface::GetHeading(); // // careful.. this looks dodgy to me - and could be ANY camera - DW
	//float fInputDirn = fwAngle::GetRadianAngleBetweenPoints(0.0f, 0.0f, -vecStick.x, vecStick.y) - camInterface::GetOrientation();
	float fInputMag = vecStick.Mag();

	if (fInputDirn > fTaskHeading + PI)
		fInputDirn -= TWO_PI;
	else if(fInputDirn < fTaskHeading - PI)
		fInputDirn += TWO_PI;

	if( bExiting )
	{
		if (pControl->GetPedSprintIsDown() && 
			rage::Abs(fInputDirn - fTaskHeading) > QUARTER_PI && 
			rage::Abs(fInputDirn - fCarHeading) > QUARTER_PI )
		{
			return true;
		}
	}
	else if (fInputMag > 0.75f && rage::Abs(fInputDirn - fTaskHeading) > QUARTER_PI)
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::CheckEarlyExitConditions(CPed& ped, CVehicle& vehicle, VehicleEnterExitFlags iFlags, bool& bWalkStart, bool bUseWiderAngleTolerance)
{
	if (ped.IsLocalPlayer())
	{
		if (!(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::PlayerControlledVehicleEntry)) && !ped.GetPedConfigFlag(CPED_CONFIG_FLAG_AllowPlayerToInterruptVehicleEntryExit))
		{
			return false;
		}

		CControl* pControl = ped.GetControlFromPlayer();

		// Abort if X is pressed.
		if( pControl->GetPedJump().IsPressed() )
		{
			return true;
		}

		// Abort if the player is trying to aim or fire.
		if(CTaskExitVehicle::IsPlayerTryingToAimOrFire(ped))
		{
			return true;
		}

		Vector3 vAwayFromVehicle = VEC3V_TO_VECTOR3(ped.GetTransform().GetPosition() - vehicle.GetTransform().GetPosition());
		float fDist = vAwayFromVehicle.Mag();

		// if this is a boat, reduce the distance by the bounding radius (cos some of them are massive)
		if (vehicle.GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			fDist -= vehicle.GetBoundRadius();
		}

		float fTaskHeading=fwAngle::GetRadianAngleBetweenPoints(vAwayFromVehicle.x,vAwayFromVehicle.y,0.0f,0.0f);
		fTaskHeading = fwAngle::LimitRadianAngle(fTaskHeading - PI);
		float fCarHeading=vehicle.GetTransform().GetHeading();

		Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
		float fInputDirn = rage::Atan2f(-vecStick.x, vecStick.y) + camInterface::GetHeading(); // // careful.. this looks dodgy to me - and could be ANY camera - DW
		//float fInputDirn = fwAngle::GetRadianAngleBetweenPoints(0.0f, 0.0f, -vecStick.x, vecStick.y) - camInterface::GetOrientation();
		float fInputMag = vecStick.Mag();

		//Displayf("Input Mag : %.2f", fInputMag);
		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MIN_MAG_FOR_EARLY_BREAKOUT, 0.5f, 0.0f, 1.0f, 0.01f);
		if (fInputMag < MIN_MAG_FOR_EARLY_BREAKOUT)
		{
			return false;
		}

		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, LARGE_BREAKOUT_ANGLE, 0.0f, -HALF_PI, HALF_PI, 0.01f);
		const float fEarlyExitAngle = bUseWiderAngleTolerance ? LARGE_BREAKOUT_ANGLE : QUARTER_PI;

		if (fInputDirn > fTaskHeading + PI)
			fInputDirn -= TWO_PI;
		else if(fInputDirn < fTaskHeading - PI)
			fInputDirn += TWO_PI;

		if (rage::Abs(fInputDirn - fTaskHeading) > fEarlyExitAngle && 
			rage::Abs(fInputDirn - fCarHeading) > fEarlyExitAngle )
		{
			if (pControl->GetPedSprintIsDown())
			{
				bWalkStart = false;
			}
			else
			{
				bWalkStart = true;
			}
			return true;
		}

		return false;
	}
	else
	{
		if(ped.GetPedConfigFlag(CPED_CONFIG_FLAG_PedIgnoresAnimInterruptEvents) || ped.IsPlayer())
		{
			return false;
		}
		else
		{
			return true;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::CheckForPlayerExitInterrupt(const CPed& rPed, const CVehicle& rVeh, VehicleEnterExitFlags iFlags, Vec3V_ConstRef vSeatOffset, s32 iExitInterruptFlagss)
{
	if (rPed.IsLocalPlayer())
	{
		// If we're a player given a scripted exit vehicle task, only allow the player to interrupt if they have control
		if (!(iFlags.BitSet().IsSet(CVehicleEnterExitFlags::PlayerControlledVehicleEntry)) && rPed.GetPlayerInfo()->AreControlsDisabled())
		{
			return false;
		}

		// No control, no interrupt
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (!pControl)
		{
			return false;
		}

		// Abort if the player is trying to aim or fire.
		if (CTaskExitVehicle::IsPlayerTryingToAimOrFire(rPed))
		{
			return true;
		}

		TUNE_GROUP_BOOL(VEHICLE_EXIT_TUNE, INTERRUPT_NO_DOOR_EXIT_FOR_WEAPON_SWITCH, true);
		if (INTERRUPT_NO_DOOR_EXIT_FOR_WEAPON_SWITCH)
		{
			if (rVeh.InheritsFromBike() || rVeh.InheritsFromQuadBike() || rVeh.InheritsFromAmphibiousQuadBike())
			{
				const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
				if(pWeaponManager && pWeaponManager->GetRequiresWeaponSwitch())
				{
					return true;
				}
			}
		}

		// Want to interrupt if we want to get back in.
		if (pControl->GetPedEnter().HistoryPressed(ms_Tunables.m_TimeToConsiderEnterInputValid))
		{
			return true;
		}

		// Want to interrupt if jump is pressed.
		if (pControl->GetPedJump().IsPressed())
		{
			return true;
		}

		// We were/are trying to sprint, interrupt as soon as possible if flagged to always interrupt
		const bool bWantsToSprint = rPed.GetPlayerInfo()->ControlButtonSprint(CPlayerInfo::SPRINT_ON_FOOT, true) >= 1.0f;
		if ((iExitInterruptFlagss & EF_TryingToSprintAlwaysInterrupts) && bWantsToSprint)
		{
			return true;
		}

		// If we're accepting any stick input direction interrupt, make sure its bigger than the dead zone
		if ((iExitInterruptFlagss & EF_AnyStickInputInterrupts))
		{
			if (Abs(pControl->GetPedWalkLeftRight().GetNorm()) > ms_Tunables.m_DeadZoneAnyInputDirection || Abs(pControl->GetPedWalkUpDown().GetNorm()) > ms_Tunables.m_DeadZoneAnyInputDirection)
			{
				return true;
			}	
		}

		// We are trying to pull away from the vehicle to move, so interrupt
		if ((bWantsToSprint || !(iExitInterruptFlagss & EF_OnlyAllowInterruptIfTryingToSprint)) && CheckForPlayerPullingAwayFromVehicle(rPed, rVeh, vSeatOffset, iExitInterruptFlagss))
		{
			return true;
		}

		if(rPed.WantsToUseActionMode())
		{
			if (Abs(pControl->GetPedWalkLeftRight().GetNorm()) > ms_Tunables.m_DeadZoneAnyInputDirection || Abs(pControl->GetPedWalkUpDown().GetNorm()) > ms_Tunables.m_DeadZoneAnyInputDirection)
			{
				return true;
			}
		}
	}
	else
	{
		if(rPed.IsNetworkClone() && rPed.IsPlayer())
		{
			if (rVeh.InheritsFromBike() || rVeh.InheritsFromQuadBike() || rVeh.InheritsFromAmphibiousQuadBike())
			{
				if(rPed.GetPedIntelligence() && rPed.GetPedIntelligence()->GetQueriableInterface())
				{
					// if the owner player has earlied out to play a swap weapon (see if(pWeaponManager && pWeaponManager->GetRequiresWeaponSwitch()) above), a clone player needs to do that too...
					if(rPed.GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_SWAP_WEAPON, PED_TASK_PRIORITY_DEFAULT, false))
					{
						return true;
					}
				}
			}
		}

		// CPED_CONFIG_FLAG_PedIgnoresAnimInterruptEvents defaults to true on scripted peds
		// so they should always be trying to do a fast exit
		if (rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_PedIgnoresAnimInterruptEvents) || rPed.IsPlayer())
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::CheckForPlayerPullingAwayFromVehicle(const CPed& rPed, const CVehicle& rVeh, Vec3V_ConstRef vSeatOffset, s32 iExitInterruptFlags)
{
	if (rPed.IsLocalPlayer())
	{
		const CControl* pControl = rPed.GetControlFromPlayer();
		if (pControl)
		{
			const Vector2 vStick(pControl->GetPedWalkLeftRight().GetNorm(), -pControl->GetPedWalkUpDown().GetNorm());
			if(vStick.Mag2() > ms_Tunables.m_DeadZoneAnyInputDirection)
			{
				const float fStickHeading = rage::Atan2f(-vStick.x, vStick.y) + camInterface::GetHeading();
				const Vec3V vVehDirection = rVeh.GetTransform().GetB();
				const Vec3V vStickDirection = RotateAboutZAxis(Vec3V(V_Y_AXIS_WONE), ScalarVFromF32(fStickHeading));
				Vec3V vSeatGlobalPos = Transform(rVeh.GetTransform().GetMatrix(), vSeatOffset);
				vSeatGlobalPos.SetZ(ScalarV(V_ZERO));
				Vec3V vPedPos = rPed.GetTransform().GetPosition();
				vPedPos.SetZ(ScalarV(V_ZERO));
				const Vec3V vAwayFromSeat = Normalize(vPedPos - vSeatGlobalPos);
				// Assume door opens infront
				const Vec3V vDoorPos = vPedPos + vVehDirection;
				const Vec3V vAwayFromDoor = Normalize(vPedPos - vDoorPos);

				// If we're allowing interrupts in the direction of the open door (or no door) or we're closing the door, increase the interrupt angle
				if (IsGreaterThanAll(Dot(vStickDirection, vAwayFromSeat), ScalarVFromF32(ms_Tunables.m_PushAngleDotTolerance)))
				{
					if ((iExitInterruptFlags & EF_IgnoreDoorTest) || IsGreaterThanAll(Dot(vStickDirection, vAwayFromDoor), ScalarVFromF32(ms_Tunables.m_TowardsDoorPushAngleDotTolerance)))
					{	
						return true;
					}
				}
			}
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskVehicleFSM::CheckForPlayerMovement(CPed& ped)
{
	if (ped.IsLocalPlayer())
	{
		CControl* pControl = ped.GetControlFromPlayer();

		if (pControl->GetPedJump().IsPressed())
		{
			return true;
		}

		Vector2 vecStick(pControl->GetPedWalkLeftRight().GetNorm(), - pControl->GetPedWalkUpDown().GetNorm());
		float fInputMag = vecStick.Mag();

		TUNE_GROUP_FLOAT(EXIT_VEHICLE_TUNE, MOVEMENT_BREAKOUT, 0.1f, 0.0f, 1.0f, 0.01f);
		if (fInputMag > MOVEMENT_BREAKOUT)
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::UnreserveDoor( CPed* pPed )
{
	taskAssertf(!pPed->IsNetworkClone(), "Can't unreserve door on a clone ped!");
	if (m_pVehicle && m_pVehicle->IsEntryPointIndexValid(m_iTargetEntryPoint))
	{
		CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetDoorReservation(m_iTargetEntryPoint);
		if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
		{
			CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskVehicleFSM::UnreserveSeat(CPed* pPed)
{
	taskAssertf(!pPed->IsNetworkClone(), "Can't unreserve seat on a clone ped!");
	if (m_pVehicle && m_pVehicle->IsEntryPointIndexValid(m_iTargetEntryPoint))
	{
		CComponentReservation* pComponentReservation = m_pVehicle->GetComponentReservationMgr()->GetSeatReservation(m_iTargetEntryPoint, SA_directAccessSeat);
		if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(pPed))
		{
			CComponentReservationManager::UnreserveVehicleComponent(pPed, pComponentReservation);
		}
	}
}

CTaskInfo* CTaskVehicleFSM::CreateQueriableState() const
{
	return NULL;
}

CTask::FSM_Return CTaskVehicleFSM::UpdateClonedFSM( s32 UNUSED_PARAM(iState), CTask::FSM_Event UNUSED_PARAM(iEvent) )
{
	return FSM_Quit;
}

void CTaskVehicleFSM::ReadQueriableState( CClonedFSMTaskInfo* pTaskInfo )
{
	CClonedVehicleFSMInfoBase* pVehicleTaskInfo = dynamic_cast<CClonedVehicleFSMInfoBase*>(pTaskInfo);
	Assert(pVehicleTaskInfo);

    m_pVehicle          = pVehicleTaskInfo->GetVehicle();
	m_iTargetEntryPoint = pVehicleTaskInfo->GetTargetEntryPoint();
    m_iRunningFlags     = pVehicleTaskInfo->GetFlags();
    m_iSeatRequestType  = pVehicleTaskInfo->GetSeatRequestType();
    m_iSeat             = pVehicleTaskInfo->GetSeat();

	// Call the base implementation - syncs the state
	CTaskFSMClone::ReadQueriableState(pTaskInfo);
}

bool CTaskVehicleFSM::IsAnyPlayerEnteringDirectlyAsDriver( const CPed* pPed, const CVehicle* pVehicle )
{
	// Build up some information about the vehicle to use later to resolve conflicts
	s32 iDirectDriverEntryPoint = -1;
	s32 iIndirectDriverEntryPoint = -1;
	bool bDriverSeatOccupied = false;
	if( pVehicle )
	{
		aiAssert(pVehicle->GetVehicleModelInfo());
		s32 iDriverSeat = pVehicle->GetDriverSeat();
		const CPed* pDriverSeatOccupier = pVehicle->GetSeatManager()->GetPedInSeat(iDriverSeat);
		bDriverSeatOccupied = pDriverSeatOccupier != NULL && ArePedsFriendly(pPed, pDriverSeatOccupier);
		const CModelSeatInfo* pModelSeatInfo = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo();
		if( pModelSeatInfo )
		{
			iDirectDriverEntryPoint = pVehicle->GetDirectEntryPointIndexForSeat(iDriverSeat);
			iIndirectDriverEntryPoint = pVehicle->GetIndirectEntryPointIndexForSeat(iDriverSeat);
		}
	}

	if( iDirectDriverEntryPoint != -1 )
	{
		// Update the target situation
		Vector3 vDriverPosition(Vector3::ZeroType);
		Quaternion qDriverOrientation(0.0f,0.0f,0.0f,1.0f);
		CModelSeatInfo::CalculateSeatSituation(pVehicle, vDriverPosition, qDriverOrientation, iDirectDriverEntryPoint);

		// Make sure no other friendly ped is trying to enter it
		const CEntityScannerIterator entityList = pPed->GetPedIntelligence()->GetNearbyPeds();
		for( const CEntity* pEnt = entityList.GetFirst(); pEnt; pEnt = entityList.GetNext() )
		{
			const CPed* pNearbyPed = static_cast<const CPed*>(pEnt);
			if( pNearbyPed != pPed )
			{
				// In MP games, if another player is trying to use the same door and is closer, we use this to prioritise another door
				if( pPed->IsLocalPlayer() )
				{
					if( pNearbyPed->IsAPlayerPed() )
					{
						if( pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
						{
							CEntity* pTargetEntity = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
							if( pTargetEntity == pVehicle )
							{
								// If we're trying to get in the driver seat but this ped is going through the other door, consider them for prioritising
								// If we're trying to get in the passenger seat and they are likely to stay in the passengers seat, consider them for prioritising
								// Otherwise if we're just trying to get in any seat, consider them for prioritising
								s32 iTargetEntryPoint = pNearbyPed->GetPedIntelligence()->GetQueriableInterface()->GetTargetEntryPointForTaskType(CTaskTypes::TASK_ENTER_VEHICLE);
								if( iTargetEntryPoint == iDirectDriverEntryPoint )
								{
									Vector3 vVehicleToDriverSeat = vDriverPosition - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
									const bool bDriverSeatOnLeft = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()).Dot(vVehicleToDriverSeat) <= 0.0f;
									Vector3 vVehicleToPed = VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
									const bool bPedOnLeft = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetA()).Dot(vVehicleToPed) <= 0.0f;
									const float fDistPedToSeatSq =  VEC3V_TO_VECTOR3(pNearbyPed->GetTransform().GetPosition()).Dist2(vDriverPosition);
									if( fDistPedToSeatSq < rage::square(MP_NEARBY_DRIVER_PRIORITY_DISTANCE) &&
										bDriverSeatOnLeft == bPedOnLeft )
									{
										return true;
									}
								}
							}
						}
					}
				}
			}
		}
	}
	return false;
}

bool CTaskVehicleFSM::IsVehicleLockedForPlayer(const CVehicle& rVeh, const CPed& rPed)
{
#if __BANK
	TUNE_GROUP_BOOL(LOCKED_VEHICLE_DEBUG, PRETEND_VEHICLE_LOCKED, false);
	if (PRETEND_VEHICLE_LOCKED)
	{
		return true;
	}
#endif // __BANK

	if (NetworkInterface::IsGameInProgress())
	{
		// networked vehicles can be locked for individual players
		if (rPed.IsAPlayerPed() && AssertVerify(rPed.GetNetworkObject()))
		{
			CNetGamePlayer* pPlayer = rPed.GetNetworkObject()->GetPlayerOwner();

			if (pPlayer && AssertVerify(pPlayer->GetPhysicalPlayerIndex() != INVALID_PLAYER_INDEX))
			{
				if (!rVeh.IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) && rVeh.GetNetworkObject() && static_cast<CNetObjVehicle*>(rVeh.GetNetworkObject())->IsLockedForPlayer(pPlayer->GetPhysicalPlayerIndex()))
				{
					return true;
				}
			}
		}
	}
	return false;
}

bool CTaskVehicleFSM::ShouldDestroyVehicleWeaponManager(const CVehicle& rVeh, bool& bIsTurretedVehicle)
{
	bIsTurretedVehicle = rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_HAS_TURRET_SEAT_ON_VEHICLE);
	bool bHasPersistantManager = rVeh.GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_CREATE_WEAPON_MANAGER_ON_SPAWN);

	if (!bIsTurretedVehicle)
	{
		if(rVeh.GetDriver() == NULL && !rVeh.IsTank() && !bHasPersistantManager)
		{
			return true;
		}
	}

	return false;
}

Vector3 CTaskVehicleFSM::ComputeWorldSpacePosFromLocal(const CVehicle& rVeh, const Vector3& vLocalRefPos)
{
	Vec3V vLocalReferencePos = VECTOR3_TO_VEC3V(vLocalRefPos);
	QuatV qUnused;
	CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(rVeh.GetTransform().GetPosition(), rVeh.GetTransform().GetOrientation(), vLocalReferencePos, qUnused);
	return VEC3V_TO_VECTOR3(vLocalReferencePos);
}

Matrix34 CTaskVehicleFSM::ComputeIdealSeatMatrixForPosition( const CTurret& rTurret, const CVehicle& rVeh, const Vector3& vLocalRefPos, s32 iSeatIndex )
{
	aiAssert(rVeh.IsSeatIndexValid(iSeatIndex));

	Vector3 vWorldRefPos = ComputeWorldSpacePosFromLocal(rVeh, vLocalRefPos);

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_WORLDREFPOS, false);
	static float fTurretSeatMatrixCalculationsSphereR = 0.025f;
	static u32 uTurretSeatMatrixCalculationsTTL = 150;
	if (RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_WORLDREFPOS)
	{
		grcDebugDraw::Sphere(vWorldRefPos,fTurretSeatMatrixCalculationsSphereR,Color_red, false,uTurretSeatMatrixCalculationsTTL);
	}
#endif // __BANK

	s32 iBoneIndex = rVeh.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromSeat(iSeatIndex);
	Matrix34 m_BoneMat;
	rVeh.GetGlobalMtx(iBoneIndex, m_BoneMat);		

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_SEATBONE, false);
	if (RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_SEATBONE)
	{
		grcDebugDraw::Axis(m_BoneMat, 1.0f, true, uTurretSeatMatrixCalculationsTTL);
	}
#endif // __BANK

	Matrix34 currentWorldTurretMtx;
	rTurret.GetTurretMatrixWorld(currentWorldTurretMtx, &rVeh);

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_CURRENTWORLDMAT, false);
	if (RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_CURRENTWORLDMAT)
	{
		grcDebugDraw::Axis(currentWorldTurretMtx, 1.0f, true, uTurretSeatMatrixCalculationsTTL);
	}
#endif // __BANK

	Vector3 vTurretPos = currentWorldTurretMtx.d;
	Vector3 vToPedPos = vWorldRefPos - vTurretPos;
	vToPedPos.RotateZ(PI);
	Vector3 vAimPos = vTurretPos + vToPedPos;

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_AIMPOS, false);
	if (RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_AIMPOS)
	{
		grcDebugDraw::Sphere(vAimPos,fTurretSeatMatrixCalculationsSphereR, Color_blue, false, uTurretSeatMatrixCalculationsTTL);
	}
#endif // __BANK

	Vector3 vLocalDir = rTurret.ComputeLocalDirFromWorldPos(vAimPos, &rVeh);
	float fDesiredHeading = rage::Atan2f(-vLocalDir.x, vLocalDir.y);		

	Matrix34 invCurrentWorldTurretMtx = currentWorldTurretMtx;
	invCurrentWorldTurretMtx.Inverse();
	Matrix34 currentSeatLocalMtx = m_BoneMat;
	currentSeatLocalMtx.Dot(invCurrentWorldTurretMtx);

	Matrix34 predictedWorldTurretMtx = currentWorldTurretMtx;
	const float fRelativeHeadingDelta = rTurret.ComputeRelativeHeadingDelta(&rVeh);
	const float fVehicleHeading = rVeh.GetTransform().GetHeading();
	const float fPredictedHeading = fwAngle::LimitRadianAngle(fDesiredHeading + fRelativeHeadingDelta + fVehicleHeading);
	predictedWorldTurretMtx.MakeRotateZ(fPredictedHeading);
	predictedWorldTurretMtx.d = currentWorldTurretMtx.d;

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_PREDICTEDWORLDMAT, false);
	if (RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_PREDICTEDWORLDMAT)
	{
		grcDebugDraw::Axis(predictedWorldTurretMtx, 1.0f, true, uTurretSeatMatrixCalculationsTTL);
	}
#endif // __BANK

	Vector3 vPredictedSeatPos = currentSeatLocalMtx.d;
	predictedWorldTurretMtx.Transform(vPredictedSeatPos);

#if __BANK
	TUNE_GROUP_BOOL(TURRET_DEBUG, RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_PREDICTEDSEATPOS, false);
	if (RENDER_IDEAL_TURRET_SEAT_CALCULATIONS_PREDICTEDSEATPOS)
	{
		grcDebugDraw::Sphere(vPredictedSeatPos,fTurretSeatMatrixCalculationsSphereR, Color_orange, false, uTurretSeatMatrixCalculationsTTL);
	}
#endif // __BANK

	Matrix34 idealMtx;
	idealMtx.Set3x3(predictedWorldTurretMtx);
	idealMtx.d = vPredictedSeatPos;
	return idealMtx;
}

bool CTaskVehicleFSM::CanPassengerLookAtPlayer(CPed* pPed)
{
	CTaskInVehicleBasic* pTask = static_cast<CTaskInVehicleBasic *>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_VEHICLE_BASIC));
	if(pTask && (pTask->GetState() == CTaskInVehicleBasic::State_Idle || pTask->GetState() == CTaskInVehicleBasic::State_PlayingAmbients))
	{
		return true;
	}

	return false;
}

void CTaskVehicleFSM::SetPassengersToLookAtPlayer(const CPed* pPlayerPed, CVehicle* pVehicle, bool bIncludeDriver)
{
	TUNE_GROUP_BOOL(IN_CAR_LOOK_AT, bEnableEnterExitCarLookAt, true);
	if(!bEnableEnterExitCarLookAt)
	{
		return;
	}
	TUNE_GROUP_INT(IN_CAR_LOOK_AT, uLookHoldTimeMin, 3000, 0, 10000, 10);
	TUNE_GROUP_INT(IN_CAR_LOOK_AT, uLookHoldTimeMax, 5000, 0, 10000, 10);
	TUNE_GROUP_INT(IN_CAR_LOOK_AT, sBlendInTime, 750, 0, 10000, 10);
	TUNE_GROUP_INT(IN_CAR_LOOK_AT, sBlendOutTime, 500, 0, 10000, 10);
	static const u32 uLookAtHash = ATSTRINGHASH("LookAtPlayerEnterOrExitVehicle", 0xAC803D8);

	for(int iSeat=0; iSeat<pVehicle->GetSeatManager()->GetMaxSeats(); iSeat++)
	{
		CPed* pPassenger = pVehicle->GetSeatManager()->GetPedInSeat(iSeat);
		if(pPassenger && pPassenger != pPlayerPed && (bIncludeDriver || !pPassenger->GetIsDrivingVehicle()) && CanPassengerLookAtPlayer(pPassenger))
		{
			u32 lookAtFlags = LF_FROM_SCRIPT | LF_WHILE_NOT_IN_FOV;
			if(fwRandom::GetRandomTrueFalse())
			{
				// Random between fast and normal
				if(fwRandom::GetRandomTrueFalse())
				{
					lookAtFlags |= LF_FAST_TURN_RATE;
				}
			}
			else
			{
				// Random between slow and normal
				if(fwRandom::GetRandomTrueFalse())
				{
					lookAtFlags |= LF_SLOW_TURN_RATE;
				}
			}

			switch (fwRandom::GetRandomNumberInRange(0, 3))
			{
			case 0:
				// Use the default normal limit. Do nothing.
				break;
			case 1:
				lookAtFlags |= LF_WIDE_YAW_LIMIT;
				break;
			case 2:
				lookAtFlags |= LF_WIDEST_YAW_LIMIT;
				break;
				//case 3:
				//	lookAtFlags |= LF_NARROW_YAW_LIMIT;
				//	break;
				//case 4:
				//	lookAtFlags |= LF_NARROWEST_YAW_LIMIT;
				//	break;
			default:
				;
			}
			s32 uHoldTime = fwRandom::GetRandomNumberInRange(uLookHoldTimeMin, uLookHoldTimeMax);
			pPassenger->GetIkManager().LookAt(uLookAtHash, pPlayerPed, uHoldTime, BONETAG_HEAD, NULL, lookAtFlags, sBlendInTime, sBlendOutTime, CIkManager::IK_LOOKAT_LOW);
		}
	}
}

s32 CTaskVehicleFSM::GetSeatPedIsTryingToEnter(const CPed* pPed)
{
	if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
	{
		const CClonedEnterVehicleInfo* pClonedEnterVehicleInfo = static_cast<const CClonedEnterVehicleInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX));
		if (pClonedEnterVehicleInfo)
		{
			return pClonedEnterVehicleInfo->GetSeat();
		}
	}
	else
	{
		const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
		if(pEnterVehicleTask)
		{
			return pEnterVehicleTask->GetTargetSeat();
		}
	}
	return -1;
}

s32 CTaskVehicleFSM::GetEntryPedIsTryingToEnter(const CPed* pPed)
{
	if( pPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_ENTER_VEHICLE) )
	{
		const CClonedEnterVehicleInfo* pClonedEnterVehicleInfo = static_cast<const CClonedEnterVehicleInfo*>(pPed->GetPedIntelligence()->GetQueriableInterface()->GetTaskInfoForTaskType(CTaskTypes::TASK_ENTER_VEHICLE, PED_TASK_PRIORITY_MAX));
		if (pClonedEnterVehicleInfo)
		{
			return pClonedEnterVehicleInfo->GetTargetEntryPoint();
		}
	}
	else
	{
		const CTaskEnterVehicle* pEnterVehicleTask = static_cast<const CTaskEnterVehicle*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
		if(pEnterVehicleTask)
		{
			return pEnterVehicleTask->GetTargetEntryPoint();
		}
	}
	return -1;
}

const CPed* CTaskVehicleFSM::GetPedsGroupPlayerLeader(const CPed& rPed)
{
	if (rPed.IsAPlayerPed())
		return NULL;

	const CPedGroup* pPedGroup = rPed.GetPedsGroup();
	if (!pPedGroup)
		return NULL;

	const CPed* pLeader = pPedGroup->GetGroupMembership()->GetLeader();
	if (!pLeader)
		return NULL;

	if (pLeader == &rPed)
		return NULL;

	if (!pLeader->IsAPlayerPed())
		return NULL;

	return pLeader;
}

const CPed* CTaskVehicleFSM::GetPedThatHasDoorReservation(const CVehicle& rVeh, s32 iEntryPointIndex)
{
	const CComponentReservation* pComponentReservation = const_cast<CComponentReservationManager*>(rVeh.GetComponentReservationMgr())->GetDoorReservation(iEntryPointIndex);		
	if (pComponentReservation)
	{
		return pComponentReservation->GetPedUsingComponent();
	}
	return NULL;
}

bool CTaskVehicleFSM::AnotherPedHasDoorReservation(const CVehicle& rVeh, const CPed& rPed, s32 iEntryPointIndex)
{
	const CPed* pPedThatHasDoorReservation = GetPedThatHasDoorReservation(rVeh, iEntryPointIndex);
	if (pPedThatHasDoorReservation && pPedThatHasDoorReservation != &rPed)
	{
		return true;
	}
	return false;
}

bool CTaskVehicleFSM::PedHasSeatReservation(const CVehicle& rVeh, const CPed& rPed, s32 iEntryPointIndex, s32 iTargetSeatIndex)
{
	const CComponentReservation* pComponentReservation = const_cast<CComponentReservationManager*>(rVeh.GetComponentReservationMgr())->GetSeatReservation(iEntryPointIndex, SA_directAccessSeat, iTargetSeatIndex);
	if (pComponentReservation && pComponentReservation->IsComponentReservedForPed(&rPed))
	{
		return true;
	}
	return false;
}

bool CTaskVehicleFSM::ShouldUseClimbUpAndClimbDown(const CVehicle& rVeh, s32 iEntryPointIndex)
{
	// ClimbUpAfterOpenDoor is a layout-flag, not seat specific, so exclude rear doors of ZHABA
	if (MI_CAR_ZHABA.IsValid() && rVeh.GetModelIndex() == MI_CAR_ZHABA && (iEntryPointIndex == 4 || iEntryPointIndex == 5))
	{
		return false;
	}

	if (rVeh.GetLayoutInfo()->GetClimbUpAfterOpenDoor())
	{
		return true;
	}

	const CVehicleEntryPointInfo* pEntryInfo = rVeh.IsEntryIndexValid(iEntryPointIndex) ? rVeh.GetEntryInfo(iEntryPointIndex) : NULL;
	if (pEntryInfo && pEntryInfo->GetIsPlaneHatchEntry())
	{
		return true;
	}

	return false;
}

#if __BANK

////////////////////////////////////////////////////////////////////////////////

fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_AlignRequestId("Align",0xE22E0892);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_ClimbUpRequestId("ClimbUp",0xADD765B8);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_PreGetInTransitionRequestId("PreGetInTransition",0xBB9DB4A2);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_ForcedEntryRequestId("ForcedEntry",0x6ABF7836);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_GetInRequestId("GetIn",0x6B1829B4);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_IdleRequestId("Idle",0x71C21326);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_CloseDoorFromInsideRequestId("CloseDoorFromInside",0x7755E70D);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_MoveToNextSeatRequestId("MoveToNextSeat",0x58770965);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_GetOutRequestId("GetOut",0xB9B5A2FB);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_PostGetOutTransitionRequestId("PostGetOutTransition",0xE180A43C);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_ClimbDownRequestId("ClimbDown",0x5330B3EE);
fwMvRequestId CTaskAttachedVehicleAnimDebug::ms_OnBikeRequestId("OnBike",0xC68998ED);

fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_AlignOnEnterId("Align_OnEnter",0x7E4761D5);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_ClimbUpOnEnterId("ClimbUp_OnEnter",0x32CE5E9F);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_PreGetInTransitionOnEnterId("PreGetInTransition_OnEnter",0x17EA120F);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_ForcedEntryOnEnterId("ForcedEntry_OnEnter",0x28C10362);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_GetInOnEnterId("GetIn_OnEnter",0xD90CE571);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_IdleOnEnterId("Idle_OnEnter",0xCA902721);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_CloseDoorFromInsideOnEnterId("CloseDoorFromInside_OnEnter",0x3D1AB017);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_MoveToNextSeatOnEnterId("MoveToNextSeat_OnEnter",0x5E4A8CEC);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_GetOutOnEnterId("GetOut_OnEnter",0x3503B401);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_PostGetOutTransitionOnEnterId("PostGetOutTransition_OnEnter",0x3F2BBA8F);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_ClimbDownOnEnterId("ClimbDown_OnEnter",0x79E9D063);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_OnBikeOnEnterId("OnBike_OnEnter",0x29D1FEF9);

fwMvFlagId	 CTaskAttachedVehicleAnimDebug::ms_ToAimFlagId("ToAim",0xAFF25888);
fwMvFlagId	 CTaskAttachedVehicleAnimDebug::ms_FastFlagId("Fast",0x64B8047E);

fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_Clip0OnLoopId("Clip0_OnLoop",0xF9F8D121);
fwMvBooleanId CTaskAttachedVehicleAnimDebug::ms_Clip0OnEndedId("Clip0_OnEnded",0xCA5A3E23);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_InvalidStateId("State_Invalid",0xD2425644);
fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_InvalidPhaseId("Phase_Invalid",0x719489D1);
fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_BlendDurationId("BlendDuration",0x5E36F77F);

fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_PreGetInPhaseId("PreGetInPhase",0xA6763DFC);
fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_ForcedEntryPhaseId("ForcedEntryPhase",0x9C86331E);
fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_GetInPhaseId("GetInPhase",0xD902EA06);
fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_GetOutPhaseId("GetOutPhase",0x9BC5AE6A);
fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_ClimbDownPhaseId("ClimbDownPhase",0x755B0FC9);
fwMvFloatId  CTaskAttachedVehicleAnimDebug::ms_LeanId("Lean",0x3443D4C6);
fwMvFloatId	 CTaskAttachedVehicleAnimDebug::ms_AimBlendDirectionId("AimBlendDirection",0xF8DC25E3);

fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_AlignStateId("Align",0xE22E0892);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_ClimbUpStateId("ClimbUp",0xADD765B8);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_PreGetInTransitionStateId("PreGetInTransition",0xBB9DB4A2);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_ForcedEntryStateId("ForcedEntry",0x6ABF7836);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_GetInStateId("GetIn",0x6B1829B4);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_IdleStateId("Idle",0x71C21326);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_CloseDoorFromInsideStateId("CloseDoorFromInsideState",0x9ED25913);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_MoveToNextSeatStateId("MoveToNextSeat",0x58770965);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_GetOutStateId("GetOut",0xB9B5A2FB);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_PostGetOutTransitionStateId("PostGetOutTransition",0xE180A43C);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_ClimbDownStateId("ClimbDown",0x5330B3EE);
fwMvStateId	 CTaskAttachedVehicleAnimDebug::ms_OnBikeStateId("OnBike",0xC68998ED);

float CTaskAttachedVehicleAnimDebug::ms_fAlignBlendInDuration = INSTANT_BLEND_DURATION;
float CTaskAttachedVehicleAnimDebug::ms_fPreGetInBlendInDuration = NORMAL_BLEND_DURATION;
float CTaskAttachedVehicleAnimDebug::ms_fGetInBlendInDuration = NORMAL_BLEND_DURATION;
float CTaskAttachedVehicleAnimDebug::ms_fIdleInBlendInDuration = NORMAL_BLEND_DURATION;
float CTaskAttachedVehicleAnimDebug::ms_fCloseDoorFromInsideBlendInDuration = NORMAL_BLEND_DURATION;
float CTaskAttachedVehicleAnimDebug::ms_fGetOutBlendInDuration = NORMAL_BLEND_DURATION;
float CTaskAttachedVehicleAnimDebug::ms_fPostGetOutBlendInDuration = NORMAL_BLEND_DURATION;

////////////////////////////////////////////////////////////////////////////////

CTaskAttachedVehicleAnimDebug::CTaskAttachedVehicleAnimDebug(CVehicle* pVehicle, s32 iSeat, s32 iTargetEntryPoint, const atArray<s32>& aDesiredStates)
: m_pVehicle(pVehicle)
, m_iSeatIndex(iSeat)
, m_iEntryPointIndex(iTargetEntryPoint)
, m_EntryClipSetId(CLIP_SET_ID_INVALID)
, m_SeatClipSetId(CLIP_SET_ID_INVALID)
, m_iDesiredStateIndex(0)
, m_aDesiredStates(aDesiredStates)
, m_bAppliedOnce(false)
, m_fLeanParameter(0.5f)
{
	SetInternalTaskType(CTaskTypes::TASK_ATTACHED_VEHICLE_ANIM_DEBUG);
}

////////////////////////////////////////////////////////////////////////////////

CTaskAttachedVehicleAnimDebug::~CTaskAttachedVehicleAnimDebug()
{

}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::CleanUp()
{
	CPed* pPed = GetPed();
	pPed->SetPedOutOfVehicle(0);
	pPed->DetachFromParent(0);
	pPed->GetMovePed().ClearTaskNetwork(m_moveNetworkHelper);
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ProcessPreFSM()
{
	if (!m_pVehicle)
		return FSM_Quit;

	if (!m_pVehicle->IsSeatIndexValid(m_iSeatIndex))
		return FSM_Quit;

	CPed* pPed = GetPed();

	// This flag causes ProcessPhysics to get called so we can fixup any error to get to our targets
	pPed->SetPedResetFlag(CPED_RESET_FLAG_ProcessPhysicsTasks, true);

	m_bAppliedOnce = false;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::ProcessPhysics(float fTimeStep, int UNUSED_PARAM(nTimeSlice))
{
	CPed& ped = *GetPed();

	if (!ped.GetIsAttached())
	{
		return false;
	}

	// Update the attachment offsets from the previous frames animations (usually gets done after this in ped.cpp)
	ped.ApplyAnimatedVelocityWhilstAttached(fTimeStep);

	// TODO: Target Fixup Code
	if (!m_bAppliedOnce)
	{
		const s32 iState = GetState();
		if (ShouldApplyGroundAndOrientationFixupForState(iState))
		{
			Vec3V vIdealPos(V_ZERO);
			QuatV qIdealRot(V_IDENTITY);

			// Compute world space target location where we'd like to be
			if (ComputeIdealEndTransformForState(iState, vIdealPos, qIdealRot))
			{
				// Get information about the blend of clips playing to compute how much we need to fixup by
				CTaskVehicleFSM::sClipBlendParams clipBlendParams;
				if (GetClipBlendParamsForState(iState, clipBlendParams, true))
				{
					// Do the fixup
					CTaskVehicleFSM::ApplyMoverFixup(ped, *m_pVehicle, m_iEntryPointIndex, clipBlendParams, fTimeStep, vIdealPos, qIdealRot);
				}
			}
		}
		m_bAppliedOnce = true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::IsMoveTransitionAvailable(s32 iNextState) const
{
	const s32 iPreviousState = GetPreviousState();
	switch(iNextState)
	{
		case State_Align:
			{
				switch (iPreviousState)
				case State_StreamAssets:
					return true;
			}
			break;
		case State_ClimbUp:
			{
				switch (iPreviousState)
				case State_StreamAssets:
				case State_Align:	
					return true;
			}
			break;
		case State_TryLockedDoor:
			{
				switch (iPreviousState)
				case State_StreamAssets:
				case State_Align:	
					return true;
			}
		case State_ForcedEntry:
			{
				switch (iPreviousState)
				case State_StreamAssets:
				case State_TryLockedDoor:	
					return true;
			}
		case State_OpenDoor:
		case State_AlignToGetIn:
			{
				switch (iPreviousState)
				case State_StreamAssets:
				case State_Align:
				case State_ClimbUp:
					return true;
			}
			break;
		case State_GetIn:
			{
				switch (iPreviousState)
				case State_OpenDoor:
				case State_AlignToGetIn:
				case State_ForcedEntry:
					return true;
			}
			break;
		case State_Idle:
			{
				switch (iPreviousState)
				case State_GetIn:
				case State_CloseDoorFromInside:
					return true;
			}
			break;
		case State_CloseDoorFromInside:
			{
				switch (iPreviousState)
				case State_GetIn:
				case State_Shuffle:
				case State_Idle:
					return true;
			}
			break;
		case State_Shuffle:
			{
				switch (iPreviousState)
				case State_Idle:
				case State_GetIn:
				case State_Shuffle:
					return true;
			}
			break;
		case State_GetOut:
		case State_GetOutToAim:
			{
				switch (iPreviousState)
				case State_Idle:
				case State_Shuffle:
				case State_GetIn:
				case State_CloseDoorFromInside:
					return true;
			}
			break;
		case State_CloseDoorFromOutside:
		case State_GetOutToIdle:
			{
				switch (iPreviousState)
				case State_GetOut:
					return true;
			}
			break;
		case State_ClimbDown:
			{
				switch (iPreviousState)
				case State_CloseDoorFromOutside:
				case State_GetOutToIdle:
					return true;
			}
			break;
		case State_OnBike:
			return true;
		default:
			taskAssertf(0, "Unexpected state!");
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	if (iState > State_StreamAssets && iState < State_Finish && iEvent == OnEnter)
	{
		// Display the offsets relative to attach point (seat node)
		if (CVehicleDebug::ms_bOutputSeatRelativeOffsets)
		{
			Vec3V vTargetPos(V_ZERO);
			QuatV qTargetRot(V_IDENTITY);
			ComputeIdealEndTransformForState(GetState(), vTargetPos, qTargetRot);

			Vec3V vAttachWorld(V_ZERO);
			QuatV qAttachWorld(V_IDENTITY);
			CTaskVehicleFSM::GetAttachWorldTransform(*m_pVehicle, m_iEntryPointIndex, vAttachWorld, qAttachWorld);

			CTaskVehicleFSM::UnTransformFromWorldSpaceToRelative(*m_pVehicle, vAttachWorld, qAttachWorld, vTargetPos, qTargetRot);
			Displayf("Seat Relative Ideal Start Offsets For State %s", GetStaticStateName(GetState()));
			Displayf("x=\"%.4f\" y=\"%.4f\" z=\"%.4f\" />", vTargetPos.GetXf(), vTargetPos.GetYf(), vTargetPos.GetZf());
			ScalarV scHeadingRotation = QuatVTwistAngle(qTargetRot, Vec3V(V_Z_AXIS_WONE));
			Displayf("value=\"%.4f\"", scHeadingRotation.Getf());
		}
	}

	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();	

		FSM_State(State_StreamAssets)
			FSM_OnUpdate
				return StreamAssets_OnUpdate();
		
		FSM_State(State_Align)
			FSM_OnEnter
				return Align_OnEnter();
			FSM_OnUpdate
				return Align_OnUpdate();

		FSM_State(State_ClimbUp)
			FSM_OnEnter
				return ClimbUp_OnEnter();
			FSM_OnUpdate
				return ClimbUp_OnUpdate();

		FSM_State(State_OpenDoor)
			FSM_OnEnter
				return OpenDoor_OnEnter();
			FSM_OnUpdate
				return OpenDoor_OnUpdate();

		FSM_State(State_AlignToGetIn)
			FSM_OnEnter
				return AlignToGetIn_OnEnter();
			FSM_OnUpdate
				return AlignToGetIn_OnUpdate();

		FSM_State(State_TryLockedDoor)
			FSM_OnEnter
				return TryLockedDoor_OnEnter();
			FSM_OnUpdate
				return TryLockedDoor_OnUpdate();

		FSM_State(State_ForcedEntry)
			FSM_OnEnter
				return ForcedEntry_OnEnter();
			FSM_OnUpdate
				return ForcedEntry_OnUpdate();

		FSM_State(State_GetIn)
			FSM_OnEnter
				return GetIn_OnEnter();
			FSM_OnUpdate
				return GetIn_OnUpdate();

		FSM_State(State_Idle)
			FSM_OnEnter
				return Idle_OnEnter();
			FSM_OnUpdate
				return Idle_OnUpdate();

		FSM_State(State_CloseDoorFromInside)
			FSM_OnEnter
				return CloseDoorFromInside_OnEnter();
			FSM_OnUpdate
				return CloseDoorFromInside_OnUpdate();

		FSM_State(State_Shuffle)
			FSM_OnEnter
				return Shuffle_OnEnter();
			FSM_OnUpdate
				return Shuffle_OnUpdate();

		FSM_State(State_GetOut)
			FSM_OnEnter
				return GetOut_OnEnter();
			FSM_OnUpdate
				return GetOut_OnUpdate();

		FSM_State(State_GetOutToAim)
			FSM_OnEnter
				return GetOutToAim_OnEnter();
			FSM_OnUpdate
				return GetOutToAim_OnUpdate();

		FSM_State(State_CloseDoorFromOutside)
			FSM_OnEnter
				return CloseDoorFromOutside_OnEnter();
			FSM_OnUpdate
				return CloseDoorFromOutside_OnUpdate();

		FSM_State(State_GetOutToIdle)
			FSM_OnEnter
				return GetOutToIdle_OnEnter();
			FSM_OnUpdate
				return GetOutToIdle_OnUpdate();

		FSM_State(State_ClimbDown)
			FSM_OnEnter
				return ClimbDown_OnEnter();
			FSM_OnUpdate
				return ClimbDown_OnUpdate();

		FSM_State(State_OnBike)
			FSM_OnEnter
				return OnBike_OnEnter();
			FSM_OnUpdate
				return OnBike_OnUpdate();

		FSM_State(State_Finish)
			FSM_OnUpdate
				return FSM_Quit;

	FSM_End
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ProcessPostFSM()
{
	TUNE_BOOL(RENDER_TARGET_MATRIX, true);
	if (RENDER_TARGET_MATRIX)
	{
		DEBUG_DRAW_ONLY(RenderTargetMatrixForState();)
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::Start_OnUpdate()
{
	if (!m_pVehicle->IsEntryIndexValid(m_iEntryPointIndex))
	{
		SetState(State_Finish);
		return FSM_Continue;
	}
	
	UpdateClipSets();

	m_aDesiredStates.PushAndGrow(State_Finish);

	if (m_aDesiredStates.GetCount() == 0)
	{
		taskAssertf(0, "No States Set");
		SetTaskState(State_Finish);
		return FSM_Continue;
	}
	
	SetTaskState(State_StreamAssets);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::StreamAssets_OnUpdate()
{
	if (m_EntryClipSetRequestHelper.Request(m_EntryClipSetId) && m_SeatClipSetRequestHelper.Request(m_SeatClipSetId))
	{
		CPed* pPed = GetPed();
		const bool bIsBikeTest = m_pVehicle->InheritsFromBike() && CVehicleDebug::ms_bTestOnBike;
		if (bIsBikeTest)
		{
			if (CVehicleDebug::ms_bAttachToBike)
			{
				pPed->AttachPedToEnterCar(m_pVehicle, ATTACH_STATE_PED_IN_CAR, m_iSeatIndex, m_iEntryPointIndex);
			}
		}
		else
		{
			if (!pPed->GetAttachParent())
			{
				AttachPedForState(m_aDesiredStates[m_iDesiredStateIndex], ATTACH_STATE_PED_ENTER_CAR);
			}
		}

		if (GetTimeInState() > 2.0f)
		{
			m_moveNetworkHelper.CreateNetworkPlayer(pPed, CClipNetworkMoveInfo::ms_NetworkTaskAttachedVehicleAnimDebug);
			pPed->GetMovePed().SetTaskNetwork(m_moveNetworkHelper, INSTANT_BLEND_DURATION);
			if (bIsBikeTest)
			{
				SetTaskState(State_OnBike);
				return FSM_Continue;
			}
			else
			{
				SetTaskStateAndUpdateDesiredState();
				return FSM_Continue;
			}
		}
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::Align_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_ENTER_CAR);
	SetMoveNetworkState(ms_AlignRequestId, ms_AlignOnEnterId, ms_AlignStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::Align_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ClimbUp_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_ENTER_CAR);
	SetMoveNetworkState(ms_ClimbUpRequestId, ms_ClimbUpOnEnterId, ms_ClimbUpStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ClimbUp_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::OpenDoor_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_ENTER_CAR);
	SetMoveNetworkState(ms_PreGetInTransitionRequestId, ms_PreGetInTransitionOnEnterId, ms_PreGetInTransitionStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::OpenDoor_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	ProcessOpenDoor(*m_pVehicle, m_iEntryPointIndex, GetClipForState(), GetClipPhaseForState());

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::AlignToGetIn_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_ENTER_CAR);
	SetMoveNetworkState(ms_PreGetInTransitionRequestId, ms_PreGetInTransitionOnEnterId, ms_PreGetInTransitionStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::AlignToGetIn_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::TryLockedDoor_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_ENTER_CAR);
	SetMoveNetworkState(ms_PreGetInTransitionRequestId, ms_PreGetInTransitionOnEnterId, ms_PreGetInTransitionStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::TryLockedDoor_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ForcedEntry_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_ENTER_CAR);
	SetMoveNetworkState(ms_ForcedEntryRequestId, ms_ForcedEntryOnEnterId, ms_ForcedEntryStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ForcedEntry_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	// Open the door based on clip tags
	ProcessOpenDoor(*m_pVehicle, m_iEntryPointIndex, GetClipForState(), GetClipPhaseForState());

	// Smash the window based on clip tags
	ProcessSmashWindow(*m_pVehicle, m_iEntryPointIndex, GetClipForState(), GetClipPhaseForState());

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetIn_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_ENTER_CAR);
	SetMoveNetworkState(ms_GetInRequestId, ms_GetInOnEnterId, ms_GetInStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetIn_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::Idle_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_IN_CAR);
	SetMoveNetworkState(ms_IdleRequestId, ms_IdleOnEnterId, ms_IdleStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::Idle_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (GetTimeInState() > 3.0f)
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::CloseDoorFromInside_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_IN_CAR);
	SetMoveNetworkState(ms_CloseDoorFromInsideRequestId, ms_CloseDoorFromInsideOnEnterId, ms_CloseDoorFromInsideStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::CloseDoorFromInside_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	ProcessCloseDoor(*m_pVehicle, m_iEntryPointIndex, GetClipForState(), GetClipPhaseForState());

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::Shuffle_OnEnter()
{
	// Update targets
	s32 iEntryPointIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryPointIndexForSeat(m_iSeatIndex, m_pVehicle);
	taskFatalAssertf(m_pVehicle->GetEntryExitPoint(iEntryPointIndex), "NULL entry point associated with seat %i", m_iSeatIndex);
	m_iSeatIndex = m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetShuffleSeatForSeat(iEntryPointIndex, m_iSeatIndex);
	m_iEntryPointIndex = m_pVehicle->GetDirectEntryPointIndexForSeat(m_iSeatIndex);
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_IN_CAR, true);
	SetMoveNetworkState(ms_MoveToNextSeatRequestId, ms_MoveToNextSeatOnEnterId, ms_MoveToNextSeatStateId);
	UpdateClipSets();
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::Shuffle_OnUpdate()
{
	// TODO: Ensure they get streamed in? Maybe add a func to prestream all clipsets before hand
	m_SeatClipSetRequestHelper.Request(m_SeatClipSetId);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetOut_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_EXIT_CAR);
	SetMoveNetworkState(ms_GetOutRequestId, ms_GetOutOnEnterId, ms_GetOutStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetOut_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	ProcessOpenDoor(*m_pVehicle, m_iEntryPointIndex, GetClipForState(), GetClipPhaseForState());

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetOutToAim_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_EXIT_CAR);
	SetMoveNetworkState(ms_GetOutRequestId, ms_GetOutOnEnterId, ms_GetOutStateId);
	m_moveNetworkHelper.SetFlag(true, ms_ToAimFlagId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetOutToAim_OnUpdate()
{
	m_moveNetworkHelper.SetFloat(ms_AimBlendDirectionId, CVehicleDebug::ms_fAimBlendDirection);

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	ProcessOpenDoor(*m_pVehicle, m_iEntryPointIndex, GetClipForState(), GetClipPhaseForState());

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::CloseDoorFromOutside_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_EXIT_CAR);
	SetMoveNetworkState(ms_PostGetOutTransitionRequestId, ms_PostGetOutTransitionOnEnterId, ms_PostGetOutTransitionStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::CloseDoorFromOutside_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	ProcessCloseDoor(*m_pVehicle, m_iEntryPointIndex, GetClipForState(), GetClipPhaseForState());

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetOutToIdle_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_EXIT_CAR);
	SetMoveNetworkState(ms_PostGetOutTransitionRequestId, ms_PostGetOutTransitionOnEnterId, ms_PostGetOutTransitionStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::GetOutToIdle_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ClimbDown_OnEnter()
{
	SetUpMoveParamsAndAttachState(ATTACH_STATE_PED_EXIT_CAR);
	SetMoveNetworkState(ms_ClimbDownRequestId, ms_ClimbDownOnEnterId, ms_ClimbDownStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::ClimbDown_OnUpdate()
{
	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	if (IsMoveNetworkStateFinished(ms_Clip0OnEndedId, ms_InvalidPhaseId))
	{
		SetTaskStateAndUpdateDesiredState();
		return FSM_Continue;
	}
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::OnBike_OnEnter()
{
	m_moveNetworkHelper.SetClipSet(m_SeatClipSetId);
	SetMoveNetworkState(ms_OnBikeRequestId, ms_OnBikeOnEnterId, ms_OnBikeStateId);
	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CTask::FSM_Return CTaskAttachedVehicleAnimDebug::OnBike_OnUpdate()
{
	m_moveNetworkHelper.SetFlag(CVehicleDebug::ms_bUseFastBikeAnims, ms_FastFlagId);

	// Compute and send lean parameter
	const CPed& player = *CGameWorld::FindLocalPlayer();
	const CControl *pControl = player.GetControlFromPlayer();
	if (pControl)
	{
		float fDesiredLeanParameter = pControl->GetVehicleSteeringLeftRight().GetNorm();
		fDesiredLeanParameter += 1.0f;
		fDesiredLeanParameter *= 0.5f;
		Displayf("Desired Lean Param: %.2f", fDesiredLeanParameter);
		TUNE_FLOAT(APPROACH_RATE_ON_BIKE, 1.0f, 0.0f, 5.0f, 0.01f);
		rage::Approach(m_fLeanParameter, fDesiredLeanParameter, APPROACH_RATE_ON_BIKE, fwTimer::GetTimeStep());

		TUNE_BOOL(USE_OVERRIDEN_LEAN_PARAM, false);
		if (USE_OVERRIDEN_LEAN_PARAM)
		{
			TUNE_FLOAT(OVERRIDEN_LEAN_PARAM, 0.5f, 0.0f, 1.0f, 0.01f);
			m_fLeanParameter = OVERRIDEN_LEAN_PARAM;
		}
		Displayf("Actual Lean Param: %.2f", m_fLeanParameter);
		m_moveNetworkHelper.SetFloat(ms_LeanId, m_fLeanParameter);
	}

	if (!IsMoveNetworkStateValid())
		return FSM_Continue;

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskAttachedVehicleAnimDebug::GetBlendDurationForState(s32 iState)
{
	switch (iState)
	{
		case State_Align:					return ms_fAlignBlendInDuration;
		case State_OpenDoor:				
		case State_AlignToGetIn:	
		case State_TryLockedDoor:
		case State_ForcedEntry:
											return ms_fPreGetInBlendInDuration;
		case State_GetIn:					return ms_fGetInBlendInDuration;
		case State_Idle:					return ms_fIdleInBlendInDuration;
		case State_CloseDoorFromInside:		return ms_fCloseDoorFromInsideBlendInDuration;
		case State_GetOut:					
		case State_GetOutToAim:
											return ms_fGetOutBlendInDuration;
		case State_CloseDoorFromOutside:	return ms_fPostGetOutBlendInDuration;
		case State_GetOutToIdle:			return ms_fPostGetOutBlendInDuration;
		default: break;
	}
	return INSTANT_BLEND_DURATION;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipId CTaskAttachedVehicleAnimDebug::GetClipIdForState(s32 iState)
{
	switch (iState)
	{
		case State_Align:					
			{
				TUNE_BOOL(USE_SHORT_ALIGN, true);
				if (USE_SHORT_ALIGN)
					return fwMvClipId("stand_align_0",0xD3A1D7D);
				else
					return fwMvClipId("align",0xE22E0892);
			}
		case State_ClimbUp:					return fwMvClipId("climb_up",0xCCFC7841);
		case State_OpenDoor:				return fwMvClipId("d_open_out",0x85FC1628);
		case State_AlignToGetIn:			return fwMvClipId("d_open_out_no_door",0xB9B2788);
		case State_TryLockedDoor:			return fwMvClipId("d_locked",0x7BA3A553);
		case State_ForcedEntry:				return fwMvClipId("d_force_entry",0xE0BD68AF);
		case State_GetIn:					return fwMvClipId("get_in",0x1B922298);
		case State_Idle:					return fwMvClipId("sit",0x5577AB18);
		case State_CloseDoorFromInside:		return fwMvClipId("d_close_in",0x9DB8D5A3);
		case State_Shuffle:					return fwMvClipId("shuffle_seat",0x7FAA58C9);
		case State_GetOut:					return fwMvClipId("get_out",0xEBC3C022);
		case State_GetOutToAim:				return fwMvClipId("std_ds_get_out_west",0xDD2E1232);
		case State_CloseDoorFromOutside:	return fwMvClipId("d_close_out",0x974EBE24);
		case State_GetOutToIdle:			return fwMvClipId("d_close_out_no_door",0x403F8F43);
		case State_ClimbDown:				return fwMvClipId("climb_down",0x7DABC1A6);
		default: taskAssertf(0, "Couldn't find clipId for unhandled state %s", GetStaticStateName(iState)); break;
	}
	return CLIP_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::GetClipIdArrayForState(s32 iState, atArray<fwMvClipId>& aClipIds)
{
	aClipIds.clear();

	switch (iState)
	{
		case State_GetOutToAim:
		{
			aClipIds.PushAndGrow(fwMvClipId("std_ds_get_out_east_ccw",0xF5C5C9FE));
			aClipIds.PushAndGrow(fwMvClipId("std_ds_get_out_south",0x73D40C14));
			aClipIds.PushAndGrow(fwMvClipId("std_ds_get_out_west",0xDD2E1232));
			aClipIds.PushAndGrow(fwMvClipId("std_ds_get_out_north",0x50DF4A73));
			aClipIds.PushAndGrow(fwMvClipId("std_ds_get_out_east_cw",0x6C21537D));
			return true;
		}
		default: taskAssertf(0, "Couldn't find clipId array for unhandled state %s", GetStaticStateName(iState)); break;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CTaskAttachedVehicleAnimDebug::GetClipSetIdForState(s32 iState) const
{
	switch (iState)
	{
		// Intentional Fall Through
		case State_Align:
		case State_ClimbUp:
		case State_OpenDoor:	
		case State_AlignToGetIn:
		case State_TryLockedDoor:
		case State_ForcedEntry:
		case State_GetIn:	
		case State_CloseDoorFromInside:
		case State_Shuffle:		
		case State_GetOut:
		case State_CloseDoorFromOutside:
		case State_GetOutToIdle:
		case State_ClimbDown:
			return m_EntryClipSetId;
		case State_GetOutToAim:
			return fwMvClipSetId("EXIT_VEHICLE_INTO_AIM_2H",0xF8E24E62);
		case State_Idle:
			return m_SeatClipSetId;
		default: taskAssertf(0, "Couldn't find clipsetId for unhandled state %s", GetStaticStateName(iState)); break;
	}
	return CLIP_SET_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

const crClip* CTaskAttachedVehicleAnimDebug::GetClipForState(s32 iState) const
{
	fwMvClipSetId clipSetId = GetClipSetIdForState(iState);
	if (clipSetId != CLIP_SET_ID_INVALID)
	{
		fwMvClipId clipId = GetClipIdForState(iState);
		if (clipId != CLIP_ID_INVALID)
		{
			const crClip* pClip = fwClipSetManager::GetClip(clipSetId, clipId);
			if (taskVerifyf(pClip, "Couldn't find clip %s in clipset %s", clipId.GetCStr(), clipSetId.GetCStr()))
			{
				return pClip;
			}
		}
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::GetClipArrayForState(s32 iState, atArray<const crClip*>& aClips) const
{
	fwMvClipSetId clipSetId = GetClipSetIdForState(iState);
	if (clipSetId != CLIP_SET_ID_INVALID)
	{
		atArray<fwMvClipId> aClipIds;
		if (GetClipIdArrayForState(iState, aClipIds))
		{
			for (s32 i=0; i<aClipIds.GetCount(); ++i)
			{
				if (aClipIds[i] != CLIP_ID_INVALID)
				{
					const crClip* pClip = fwClipSetManager::GetClip(clipSetId, aClipIds[i]);
					if (taskVerifyf(pClip, "Couldn't find clip %s in clipset %s", aClipIds[i].GetCStr(), clipSetId.GetCStr()))
					{
						aClips.PushAndGrow(pClip);
					}
				}
			}
			return aClips.GetCount() > 0 ? true : false;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

float CTaskAttachedVehicleAnimDebug::GetClipPhaseForState() const
{
	const s32 iState = GetState();
	switch (iState)
	{
		case State_OpenDoor:	return rage::Clamp(m_moveNetworkHelper.GetFloat(ms_PreGetInPhaseId), 0.0f, 1.0f);
		case State_ForcedEntry: return rage::Clamp(m_moveNetworkHelper.GetFloat(ms_ForcedEntryPhaseId), 0.0f, 1.0f);
		case State_GetIn:		return rage::Clamp(m_moveNetworkHelper.GetFloat(ms_GetInPhaseId), 0.0f, 1.0f);
		case State_GetOut:		return rage::Clamp(m_moveNetworkHelper.GetFloat(ms_GetOutPhaseId), 0.0f, 1.0f);
		case State_ClimbDown:	return rage::Clamp(m_moveNetworkHelper.GetFloat(ms_ClimbDownPhaseId), 0.0f, 1.0f);
		case State_Align:
		case State_TryLockedDoor:
		case State_CloseDoorFromInside: 
		case State_CloseDoorFromOutside: 
		case State_GetOutToAim:
			return CTaskVehicleFSM::GetGenericClipPhaseClamped(m_moveNetworkHelper, 0);
		default: taskAssertf(0, "Unhandled phase, state %s", GetStaticStateName(iState)); break;
	}

	return CTaskVehicleFSM::GetGenericClipPhaseClamped(m_moveNetworkHelper, 0);
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::GetClipBlendParamsForState(s32 iState, CTaskVehicleFSM::sClipBlendParams& clipBlendParams, bool bStartedClips)
{
	switch (iState)
	{
		case State_GetOutToAim:
			{
				clipBlendParams.afPhases.clear();
				clipBlendParams.afBlendWeights.clear();
				clipBlendParams.apClips.clear();

				for (s32 i=0; i<5; ++i)
				{
					if (!bStartedClips)
					{
						clipBlendParams.afPhases.PushAndGrow(0.0f);
					}
					else
					{
						clipBlendParams.afPhases.PushAndGrow(CTaskVehicleFSM::GetGenericClipPhaseClamped(m_moveNetworkHelper, i));
					}
					clipBlendParams.afBlendWeights.PushAndGrow(CTaskVehicleFSM::GetGenericBlendWeightClamped(m_moveNetworkHelper, i));
				}

				if (GetClipArrayForState(iState, clipBlendParams.apClips))
				{
					//if (!bStartedClips)
					{
						CTaskVehicleFSM::PreComputeStaticBlendWeightsFromSignal(clipBlendParams, CVehicleDebug::ms_fAimBlendDirection);		
					}
					return true;
				}
			}
		default: 
			{
				clipBlendParams.afPhases.PushAndGrow(CTaskVehicleFSM::GetGenericClipPhaseClamped(m_moveNetworkHelper, 0));
				clipBlendParams.afBlendWeights.PushAndGrow(1.0f);
				clipBlendParams.apClips.PushAndGrow(GetClipForState(iState));
			}
	}
	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::ComputeIdealStartTransformForState(s32 iState, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut)
{
	Vec3V vTemp(V_ZERO);
	QuatV qTemp(V_IDENTITY);

	switch (iState)
	{
		case State_Align:
		case State_ClimbUp:
		case State_OpenDoor:
		case State_AlignToGetIn:
		case State_TryLockedDoor:
		case State_ForcedEntry:
		case State_GetIn:
		case State_Idle:
		case State_Shuffle:
		case State_ClimbDown:
		case State_CloseDoorFromOutside:
		case State_GetOutToIdle:
		case State_GetOut:
			{
				// Get the relative ideal offsets to end target (just the animated offsets)
				if (ComputeIdealStartTransformRelative(GetClipForState(iState), vIdealPosOut, qIdealRotOut))
				{
					// Get the world space target transform (beginning transform of next anim)
					ComputeIdealEndTransformForState(iState, vTemp, qTemp);

					// Transform the offsets to world space
					CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(vTemp, qTemp, vIdealPosOut, qIdealRotOut);

					// Adjust the transform in case the vehicle/ped needs to be constrained
					if (ShouldApplyGroundAndOrientationFixupForState(iState))
					{
						ApplyGroundAndOrientationFixupForState(iState, vIdealPosOut, qIdealRotOut);
					}
					return true;
				}
				break;
			}
		default: break;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::ComputeIdealEndTransformForState(s32 iState, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut)
{
	switch (iState)
	{
		case State_TryLockedDoor:
			{
				ComputeIdealStartTransformForState(State_ForcedEntry, vIdealPosOut, qIdealRotOut);
				return true;
			}
		case State_Align:
			{
				s32 iTargetState = State_OpenDoor;

				const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(m_iEntryPointIndex);
				if (pEntryPointClipInfo)
				{
					if (m_pVehicle->InheritsFromBike())
					{
						iTargetState = State_GetIn;
					}
					else if (pEntryPointClipInfo->GetHasClimbUp())
					{
						iTargetState = State_ClimbUp;
					}
					else if (m_pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(m_iEntryPointIndex) <= 0)
					{
						iTargetState = State_GetIn;
					}
				}
				ComputeIdealStartTransformForState(iTargetState, vIdealPosOut, qIdealRotOut);
				return true;
			}
		case State_ClimbUp:
			{
				// Climb up anims go into the opendoor, so our target is the ideal start transform for the opendoor
				ComputeIdealStartTransformForState(State_OpenDoor, vIdealPosOut, qIdealRotOut);
				return true;
			}
		case State_ForcedEntry:
		case State_OpenDoor:
		case State_AlignToGetIn:
			{
				// Open door anim goes into the get_in, so our target is the ideal start transform for the get_in
				ComputeIdealStartTransformForState(State_GetIn, vIdealPosOut, qIdealRotOut);
				return true;
			}
		case State_GetIn:
		case State_Idle:
			{
				// Get in anim should end with mover on seat node
				Vector3 vPos; Quaternion qRot;
				CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vPos, qRot, m_iEntryPointIndex);
				vIdealPosOut = RCC_VEC3V(vPos); qIdealRotOut = RCC_QUATV(qRot);
				return true;
			}
		case State_Shuffle:
			{
				// TODO: Should change the target seat once shuffled rather than at the beginning
				// This should then use the indirect seat
				// Get in anim should end with mover on seat node
				Vector3 vPos; Quaternion qRot;
				CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vPos, qRot, m_iEntryPointIndex);
				vIdealPosOut = RCC_VEC3V(vPos); qIdealRotOut = RCC_QUATV(qRot);
				return true;
			}
		case State_GetOut:
			{
				CTaskVehicleFSM::sClipBlendParams clipBlendParams;
				if (GetClipBlendParamsForState(iState, clipBlendParams, false))
				{
					if (CTaskVehicleFSM::ComputeIdealEndTransformRelative(clipBlendParams, vIdealPosOut, qIdealRotOut))
					{
						// From Initial Seat Transform To End Of Anim
						Vector3 vPos; Quaternion qRot;
						CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vPos, qRot, m_iEntryPointIndex);
						Vec3V vInitial = RCC_VEC3V(vPos);
						QuatV qInitial = RCC_QUATV(qRot);

						CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(vInitial, qInitial, vIdealPosOut, qIdealRotOut);

						if (ShouldApplyGroundAndOrientationFixupForState(iState))
						{
							ApplyGroundAndOrientationFixupForState(iState, vIdealPosOut, qIdealRotOut);
						}

						return true;
					}
				}

				return false;
			}
		case State_GetOutToAim:
			{
				CTaskVehicleFSM::sClipBlendParams clipBlendParams;	
				if (GetClipBlendParamsForState(iState, clipBlendParams, false))
				{
					if (CTaskVehicleFSM::ComputeIdealEndTransformRelative(clipBlendParams, vIdealPosOut, qIdealRotOut))
					{
						// From Initial Seat Transform To End Of Anim
						Vector3 vPos; Quaternion qRot;
						CModelSeatInfo::CalculateSeatSituation(m_pVehicle, vPos, qRot, m_iEntryPointIndex);
						Vec3V vInitial = RCC_VEC3V(vPos);
						QuatV qInitial = RCC_QUATV(qRot);

						CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(vInitial, qInitial, vIdealPosOut, qIdealRotOut);

						if (ShouldApplyGroundAndOrientationFixupForState(iState))
						{
							ApplyGroundAndOrientationFixupForState(iState, vIdealPosOut, qIdealRotOut);
						}

						return true;
					}
				}

				return false;
			}
		case State_CloseDoorFromOutside:
		case State_GetOutToIdle:
			{
				CTaskVehicleFSM::sClipBlendParams clipBlendParams;
				if (GetClipBlendParamsForState(iState, clipBlendParams, false))
				{
					if (CTaskVehicleFSM::ComputeIdealEndTransformRelative(clipBlendParams, vIdealPosOut, qIdealRotOut))
					{
						Vec3V vInitial(V_ZERO);
						QuatV qInitial(V_IDENTITY);

						// From Initial GetOut Transform To End Of Anim
						if (ComputeIdealEndTransformForState(State_GetOut, vInitial, qInitial))
						{
							CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(vInitial, qInitial, vIdealPosOut, qIdealRotOut);

							if (ShouldApplyGroundAndOrientationFixupForState(iState))
							{
								ApplyGroundAndOrientationFixupForState(iState, vIdealPosOut, qIdealRotOut);
							}
						}

						return true;
					}
				}

				return false;
			}
		case State_ClimbDown:
			{
				CTaskVehicleFSM::sClipBlendParams clipBlendParams;
				if (GetClipBlendParamsForState(iState, clipBlendParams, false))
				{
					if (CTaskVehicleFSM::ComputeIdealEndTransformRelative(clipBlendParams, vIdealPosOut, qIdealRotOut))
					{
						Vec3V vInitial(V_ZERO);
						QuatV qInitial(V_IDENTITY);

						// From Initial CloseDoor Transform To End Of Anim
						if (ComputeIdealEndTransformForState(State_CloseDoorFromOutside, vInitial, qInitial))
						{
							CTaskVehicleFSM::TransformRelativeOffsetsToWorldSpace(vInitial, qInitial, vIdealPosOut, qIdealRotOut);

							if (ShouldApplyGroundAndOrientationFixupForState(iState))
							{
								ApplyGroundAndOrientationFixupForState(iState, vIdealPosOut, qIdealRotOut);
							}
						}

						return true;
					}
				}

				return false;
			}
		default: break;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::ShouldApplyGroundAndOrientationFixupForState(s32 iState)
{
	if (CVehicleDebug::ms_bDisableGroundAndOrientationFixup)
	{
		return false;
	}

	switch (iState)
	{
		case State_Align:
		case State_GetIn:
		case State_ClimbDown:
		case State_GetOutToAim:
			return true;
		case State_GetOut:
			{
				if (!m_pVehicle->InheritsFromPlane())
				{
					return true;
				}
			}
		default: break;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::ApplyGroundAndOrientationFixupForState(s32 iState, Vec3V_Ref vIdealPosOut, QuatV_Ref qIdealRotOut)
{
	if (iState == State_Align || iState == State_AlignToGetIn || iState == State_GetOut || iState == State_GetOutToAim)
	{
		CPed& ped = *GetPed();
		float fTemp = -999.0f;
		CTaskVehicleFSM::ApplyGroundAndOrientationFixup(*m_pVehicle, ped, vIdealPosOut, qIdealRotOut, fTemp, -1.0f);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CTaskAttachedVehicleAnimDebug::ComputeIdealStartTransformRelative(const crClip* pClip, Vec3V_Ref vPosOffset, QuatV_Ref qRotOffset)
{
	if (pClip)
	{
		vPosOffset = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 1.0f, 0.0f));
		QuatV qStartRotation = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.0f));
		ScalarV scInitialHeadingOffset = -QuatVTwistAngle(qStartRotation, Vec3V(V_Z_AXIS_WONE));
		// Account for any initial z rotation
		vPosOffset = RotateAboutZAxis(vPosOffset, scInitialHeadingOffset);
		// Get the total z axis rotation over the course of the anim excluding the initial rotation
		QuatV qRotTotal = QUATERNION_TO_QUATV(fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 1.0f, 0.0f));	
		ScalarV scTotalHeadingRotation = QuatVTwistAngle(qRotTotal, Vec3V(V_Z_AXIS_WONE));
		vPosOffset = RotateAboutZAxis(vPosOffset, scTotalHeadingRotation);
		qRotOffset = QuatVFromZAxisAngle(scTotalHeadingRotation);
		return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::AttachPedForState(s32 iState, s32 iAttachState, bool bJustSetAttachStateIfAttached)
{
	CPed* pPed = GetPed();

	if (bJustSetAttachStateIfAttached && pPed->GetAttachParent())
	{
		fwAttachmentEntityExtension* pExtension = pPed->GetAttachmentExtension();
		if (pExtension)
		{
			pExtension->SetAttachState(iAttachState);
		}
	}
	else
	{
		Vec3V vStartPos(V_ZERO);
		QuatV qStartRot(V_IDENTITY);

		// Compute world space ideal start pos
		if (ComputeIdealStartTransformForState(iState, vStartPos, qStartRot))
		{
			// Warp ped to position
			const float fHeading = QuatVTwistAngle(qStartRot, Vec3V(V_Z_AXIS_WONE)).Getf();
			pPed->SetPosition(VEC3V_TO_VECTOR3(vStartPos));
			pPed->SetHeading(fHeading);
			pPed->SetDesiredHeading(fHeading);

			DEBUG_DRAW_ONLY(RenderMatrixFromVecQuat(vStartPos, qStartRot, 2000, atStringHash(GetStaticStateName(GetState())));)

			Vec3V vAttachWorld(V_ZERO);
			QuatV qAttachWorld(V_IDENTITY);
			CTaskVehicleFSM::GetAttachWorldTransform(*m_pVehicle, m_iEntryPointIndex, vAttachWorld, qAttachWorld);

			// Untransform relative to vehicle for attach
			CTaskVehicleFSM::UnTransformFromWorldSpaceToRelative(*m_pVehicle, vAttachWorld, qAttachWorld, vStartPos, qStartRot);
	
			// Do attach
			const Vector3 vOffset = RCC_VECTOR3(vStartPos);
			const Quaternion qOffset = RCC_QUATERNION(qStartRot);
			pPed->AttachPedToEnterCar(m_pVehicle, iAttachState, m_iSeatIndex, m_iEntryPointIndex, &vOffset, &qOffset);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::SetTaskStateAndUpdateDesiredState()
{
	if (taskVerifyf(m_iDesiredStateIndex < m_aDesiredStates.GetCount(), "Invalid Desired State Index %i", m_iDesiredStateIndex))
	{
		SetTaskState(m_aDesiredStates[m_iDesiredStateIndex++]);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::SetUpMoveParamsAndAttachState(s32 iAttachState, bool bForceAttachUpdate)
{
	const s32 iState = GetState();
	const bool bJustSetAttachStateIfAttached = bForceAttachUpdate || !IsMoveTransitionAvailable(iState) ?  false : true;
	AttachPedForState(iState, iAttachState, bJustSetAttachStateIfAttached);
	if (iState == State_GetOutToAim)
	{
		atArray<const crClip*> aClips;
		if (GetClipArrayForState(aClips))
		{
			for (s32 i=0; i<aClips.GetCount(); ++i)
			{
				m_moveNetworkHelper.SetClip(aClips[i], CTaskVehicleFSM::GetGenericClipId(i));
			}
		}
	}
	else
	{
		m_moveNetworkHelper.SetClip(GetClipForState(iState), CTaskVehicleFSM::ms_Clip0Id);
	}
	m_moveNetworkHelper.SetFloat(ms_BlendDurationId, GetBlendDurationForState(iState));
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::UpdateClipSets()
{
	const CVehicleEntryPointAnimInfo* pEntryAnimInfo = m_pVehicle->GetEntryAnimInfo(m_iEntryPointIndex);
	if (pEntryAnimInfo)
	{
		m_EntryClipSetId = pEntryAnimInfo->GetEntryPointClipSetId();
		taskAssert(m_EntryClipSetId != CLIP_SET_ID_INVALID);
	}

	const CVehicleSeatAnimInfo* pSeatAnimInfo = m_pVehicle->GetSeatAnimationInfo(m_iSeatIndex);
	if (pSeatAnimInfo)
	{
		m_SeatClipSetId = pSeatAnimInfo->GetSeatClipSetId();
		taskAssert(m_SeatClipSetId != CLIP_SET_ID_INVALID);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::ProcessOpenDoor(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase)
{
	ProcessDoorAngle(vehicle, iEntryPoint, pClip, fPhase, false);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::ProcessCloseDoor(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase)
{
	ProcessDoorAngle(vehicle, iEntryPoint, pClip, fPhase, true);
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::ProcessDoorAngle(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase, bool bClose)
{
	if (pClip)
	{
		CCarDoor* pDoor = GetDoorForEntryPointIndex(vehicle, iEntryPoint);
		if (pDoor)
		{
			float fStartPhase	= 0.0f;
			float fEndPhase		= 1.0f;

			const bool bStartTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, true, fStartPhase);
			const bool bEndTagFound = CClipEventTags::FindEventPhase<crPropertyAttributeBool>(pClip, CClipEventTags::Door, CClipEventTags::Start, false, fEndPhase);

			if (!bStartTagFound || !bEndTagFound)
			{
				animAssertf(CClipEventTags::HasMoveEvent(pClip, CClipEventTags::Door.GetCStr()), "Vehicle : %s, Clip : %s,Doesn't Have Open/Close Door Tags, Please Add A Bug To Default Anim InGame", vehicle.GetDebugName(), pClip->GetName());
				return;
			}

			CTaskVehicleFSM::DriveDoorFromClip(vehicle, pDoor, bClose ? CTaskVehicleFSM::VF_CloseDoor : CTaskVehicleFSM::VF_OpenDoor, fPhase, fStartPhase, fEndPhase);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

void CTaskAttachedVehicleAnimDebug::ProcessSmashWindow(CVehicle& vehicle, s32 iEntryPoint, const crClip* pClip, float fPhase)
{
	if (pClip)
	{
		float fSmashWindowPhase = 1.0f;
		taskVerifyf(CClipEventTags::FindEventPhase(pClip, CClipEventTags::Smash, fSmashWindowPhase), "Clip %s doesn't have smash window event", pClip->GetName());
		if (fPhase >= fSmashWindowPhase)
		{
			eHierarchyId windowId = vehicle.GetVehicleModelInfo()->GetEntryPointInfo(iEntryPoint)->GetWindowId();
			if (windowId != VEH_INVALID_ID)
			{
				vehicle.SmashWindow(windowId, VEHICLEGLASSFORCE_KICK_ELBOW_WINDOW);
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CCarDoor* CTaskAttachedVehicleAnimDebug::GetDoorForEntryPointIndex(CVehicle& vehicle, s32 iEntryPointIndex)
{
	s32 iBoneIndex = vehicle.GetVehicleModelInfo()->GetModelSeatInfo()->GetBoneIndexFromEntryPoint(iEntryPointIndex);

	if (iBoneIndex > -1)
	{
		return vehicle.GetDoorFromBoneIndex(iBoneIndex);
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#if DEBUG_DRAW

void CTaskAttachedVehicleAnimDebug::RenderTargetMatrixForState()
{
	const s32 iState = GetState();
	if (iState > State_StreamAssets)
	{
		Vec3V vTargetPos(V_ZERO);
		QuatV qTargetRot(V_IDENTITY);
		ComputeIdealEndTransformForState(GetState(), vTargetPos, qTargetRot);
		RenderMatrixFromVecQuat(vTargetPos, qTargetRot, 2000, atStringHash(GetStaticStateName(GetState())));
		char szText[128];
		formatf(szText, "Target For %s", GetStaticStateName(iState));
		CTask::ms_debugDraw.AddText(vTargetPos, 0, 0, szText, Color_blue, 2000);
	}
}

void CTaskAttachedVehicleAnimDebug::RenderMatrixFromVecQuat(Vec3V_Ref vPosition, QuatV_Ref qOrientation, s32 iDuration, u32 uKey)
{
	Mat34V mtx(V_IDENTITY);
	Mat34VFromQuatV(mtx, qOrientation, vPosition);
	CTask::ms_debugDraw.AddAxis(mtx, 0.5f, true, iDuration, uKey);
}

#endif // DEBUG_DRAW

////////////////////////////////////////////////////////////////////////////////

#endif // __BANK
