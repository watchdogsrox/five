// File header
#include "Task/System/TaskHelpers.h"

// Framework headers
#include "ai/aichannel.h"
#include "fwanimation/animdirector.h"
#include "fwanimation/directorcomponentmove.h"
#include "fwmaths/Angle.h"
#include "fwmaths/vectorutil.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "fwscene/stores/networkdefstore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "fwscene/world/WorldLimits.h"


// Game headers
#include "ai/AnimBlackboard.h"
#include "ai/Ambient/AmbientModelSetManager.h"
#include "ai/Ambient/ConditionalAnimManager.h"
#include "ai/stats.h"
#include "Animation/Anim_channel.h"
#include "Animation/AnimManager.h"
#include "Animation/PMDictionaryStore.h"
#include "animation/MovePed.h"
#include "animation/MoveObject.h"
#include "animation/MoveVehicle.h"
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/gameplay/aim/FirstPersonPedAimCamera.h"
#include "camera/gameplay/aim/FirstPersonShooterCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "camera/helpers/ControlHelper.h"
#include "camera/helpers/Frame.h"
#include "camera/helpers/FrameShaker.h"
#include "camera/viewports/Viewport.h"
#include "Debug/DebugScene.h"
#include "debug/VectorMap.h"
#include "event/EventShocking.h"
#include "event/Events.h"
#include "event/ShockingEvents.h"
#include "game/Riots.h"
#include "objects/Door.h"
#include "ModelInfo/ModelInfo.h"
#include "pathserver/PathServer.h"
#include "Peds/Ped.h"
#include "Peds/PedHelmetComponent.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIKSettings.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedplacement.h"
#include "Peds/pedpopulation.h"
#include "physics/gtaInst.h"
#include "Physics/Physics.h"
#include "scene/EntityIterator.h"
#include "scene/world/GameWorld.h"
#include "script/script_channel.h"
#include "Streaming/PrioritizedClipSetStreamer.h"
#include "Streaming/Streaming.h"
#include "Streaming/streamingvisualize.h"
#include "Task/Default/AmbientAnimationManager.h"
#include "Task/Default/AmbientAudio.h"
#include "Task/Default/Patrol/TaskPatrol.h"
#include "Task/Default/TaskChat.h"
#include "Task/Default/TaskSlopeScramble.h"
#include "Task/Combat/TaskInvestigate.h"
#include "Task/Combat/TaskNewCombat.h"
#include "Task/Movement/TaskGetUp.h"
#include "Task/Movement/TaskParachute.h"
#include "Task/Motion/Locomotion/TaskHumanLocomotion.h"
#include "Task/Motion/Locomotion/TaskMotionAiming.h"
#include "Task/Physics/TaskBlendFromNM.h"
#include "task/Response/TaskAgitated.h"
#include "Task/System/TaskHelperFSM.h"
#include "task/Vehicle/TaskCar.h"
#include "task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskVehicleBase.h"
#include "task/Weapons/Gun/TaskVehicleDriveBy.h"
#include "Vehicles/Heli.h"
#include "Vehicles/Metadata/AIHandlingInfo.h"
#include "Vehicles/Metadata/VehicleEntryPointInfo.h"
#include "Vehicles/Metadata/VehicleSeatInfo.h"
#include "Vehicles/Metadata/VehicleDebug.h"
#include "Vehicles/Metadata/VehicleLayoutInfo.h"
#include "Vehicles/Trailer.h"
#include "Vehicles/vehiclepopulation.h"
#include "Vehicles/VirtualRoad.h"
#include "VehicleAI/driverpersonality.h"
#include "vehicleai/VehicleNodeList.h"
#include "vehicleai/VehicleIntelligence.h"
#include "vehicleai/task/TaskVehicleMissionBase.h"
#include "VehicleAi/task/TaskVehicleCruise.h"
#include "VehicleAi/task/TaskVehicleEscort.h"
#include "vehicleAi/task/TaskVehicleAnimation.h"
#include "vehicleAi/task/TaskVehicleGoToHelicopter.h"
#include "weapons/projectiles/Projectile.h"

#include "Task/Motion/TaskMotionBase.h"

#include "math/angmath.h"

#include "camera/cinematic/camera/mounted/CinematicMountedCamera.h"

// rage includes
#include "crmetadata/tagiterators.h"
#include "crmetadata/property.h"
#include "crmetadata/propertyattributes.h"
#include "crmotiontree/nodeblend.h"
#include "crmotiontree/nodeblendn.h"
#include "crmotiontree/nodeclip.h"
#include "system/memops.h"

#if !__FINAL
#include "peds/Ped.h"
#include "Text/Text.h"
#include "Text/TextConversion.h"

AI_SCENARIO_OPTIMISATIONS()
AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()
VEHICLE_OPTIMISATIONS()

#define PARAM_BUFFER_WARNING_LENGTH 5
#define CREATE_NETWORK_WARNING_LENGTH 5

#endif

using namespace AIStats;

// This timer is defined in 'Vehicles/Automobile.cpp', on the "GTA Autos" EKG page.
EXT_PF_TIMER(FindTargetCoorsAndSpeed);

////////////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelperFunctions::SetMoveNetworkState(const fwMvRequestId& requestId, const fwMvBooleanId& onEnterId, const fwMvStateId& stateId, CTask* pTask, const CPed& BANK_ONLY(ped), bool UNUSED_PARAM(ignoreCloneCheck))
{
	taskFatalAssertf(pTask, "NULL task pointer in CTaskEnterVehicle::SetAnimState");

	CMoveNetworkHelper* pMoveNetworkHelper = pTask->GetMoveNetworkHelper();

	if (taskVerifyf(pMoveNetworkHelper, "Expected a valid move network helper, did you forget to implement GetMoveNetworkHelper()?"))
	{
		if (pTask->IsMoveTransitionAvailable(pTask->GetState()))
		{
			pMoveNetworkHelper->SendRequest(requestId);
			BANK_ONLY(taskDebugf2("Frame : %u - %s%s sent request %s from %s waiting for on enter event %s", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName(), requestId.GetCStr(), pTask->GetStateName(pTask->GetState()), onEnterId.GetCStr()));
		}
		else
		{
			taskAssertf(ped.IsNetworkClone(), "Forcing the move state for a non-network clone!");
			pMoveNetworkHelper->ForceStateChange(stateId);
			BANK_ONLY(taskDebugf2("Frame : %u - %s%s forced to state %s from %s waiting for on enter event %s", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName(), stateId.GetCStr(), pTask->GetStateName(pTask->GetState()), onEnterId.GetCStr()));
		}
		pMoveNetworkHelper->WaitForTargetState(onEnterId);
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CMoveNetworkHelperFunctions::IsMoveNetworkStateFinished(const fwMvBooleanId& animFinishedEventId, const fwMvFloatId& phaseId, const CTask* pTask, const CPed& BANK_ONLY(ped))
{
	const CMoveNetworkHelper* pMoveNetworkHelper = pTask->GetMoveNetworkHelper();

	if (taskVerifyf(pMoveNetworkHelper, "Expected a valid move network helper, did you forget to implement GetMoveNetworkHelper()?"))
	{
		if (pMoveNetworkHelper->GetBoolean(animFinishedEventId))
		{
			return true;
		}

		const float fAnimPhase = pMoveNetworkHelper->GetFloat(phaseId);
		BANK_ONLY(taskDebugf2("Frame : %u - %s%s : 0x%p in %s:%s : 0x%p is waiting for anim finished event %s, current anim phase %.2f", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName(), &ped, pTask->GetTaskName(), pTask->GetStateName(pTask->GetState()), pTask, animFinishedEventId.GetCStr(), fAnimPhase));
		if (pTask->GetTimeInState() > 0.0f && fAnimPhase >= 1.0f)
		{
			taskWarningf("Anim has reached the end without having received the anim finished event, task state %i, time in state %.2f, previous state %i", pTask->GetState(), pTask->GetTimeInState(), pTask->GetPreviousState());
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CMoveNetworkHelperFunctions::IsMoveNetworkStateValid(const CTask* pTask, const CPed& BANK_ONLY(ped))
{
	const CMoveNetworkHelper* pMoveNetworkHelper = pTask->GetMoveNetworkHelper();

	if (taskVerifyf(pMoveNetworkHelper, "Expected a valid move network helper, did you forget to implement GetMoveNetworkHelper()?"))
	{
		if (pMoveNetworkHelper->IsInTargetState())
		{
			return true;
		}

#if __BANK
		taskDebugf2("Frame : %u - %s%s is not in anim target state and in %s", fwTimer::GetFrameCount(), ped.IsNetworkClone() ? "Clone ped : " : "Local ped : ", ped.GetDebugName(), pTask->GetStateName(pTask->GetState()));
#endif // __ASSERT 
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

//
// Helper class to be included as a member of tasks that need to stream clips
//
CClipRequestHelper::CClipRequestHelper()
{
	m_nRequiredDictIndex = -1;
}

CClipRequestHelper::~CClipRequestHelper()
{
	ReleaseClips();
}

bool CClipRequestHelper::RequestClipsByIndex(s32 clipDictIndex)
{
	animAssert(clipDictIndex != -1);
	STRVIS_AUTO_CONTEXT(strStreamingVisualize::CLIPREQUEST);

	if(clipDictIndex!=m_nRequiredDictIndex)
	{
		ReleaseClips();
		m_nRequiredDictIndex = clipDictIndex;

		if(m_nRequiredDictIndex != -1)
		{
			m_request.Request(strLocalIndex(m_nRequiredDictIndex), fwAnimManager::GetStreamingModuleId(), STRFLAG_DONTDELETE);
		}
	}

	bool bHasLoaded = m_request.HasLoaded();
	if (bHasLoaded)
	{
		m_request.ClearRequiredFlags(STRFLAG_DONTDELETE);
	}

	return bHasLoaded;
}

void CClipRequestHelper::ReleaseClips()
{
	if(m_nRequiredDictIndex != -1)
	{
		m_request.ClearRequiredFlags(STRFLAG_DONTDELETE);
		m_request.Release();
		m_nRequiredDictIndex = -1;
	}
}


////////////////////////////////////////////////////////////////////////////////

//
// Helper class to be included as a member of tasks that need to stream get up sets
//
CGetupSetRequestHelper::CGetupSetRequestHelper()
{
	m_requiredGetupSetId = NMBS_INVALID;
	m_bIsReferenced = false;
}

CGetupSetRequestHelper::~CGetupSetRequestHelper()
{
	Release();
}

bool CGetupSetRequestHelper::Request(eNmBlendOutSet requestedSetId)
{
	if (IsLoaded() && m_requiredGetupSetId!= NMBS_INVALID && m_requiredGetupSetId != requestedSetId)
	{
		Release();
	}

	m_requiredGetupSetId = requestedSetId;

	if (!IsLoaded() && CNmBlendOutSetManager::RequestBlendOutSet(m_requiredGetupSetId))
	{
		CNmBlendOutSetManager::AddRefBlendOutSet(m_requiredGetupSetId);
		m_bIsReferenced = true;
	}

	return IsLoaded();
}

void CGetupSetRequestHelper::Release()
{
	if (IsLoaded() && m_requiredGetupSetId != NMBS_INVALID)
	{
		CNmBlendOutSetManager::ReleaseBlendOutSet(m_requiredGetupSetId);
	}

	m_bIsReferenced = false;
	m_requiredGetupSetId = NMBS_INVALID;
}

eNmBlendOutSet CGetupSetRequestHelper::GetRequiredGetUpSetId(void) const
{
	return m_requiredGetupSetId;
}

//-----------------------------------------------------------------------------------

CGetupProbeHelper::CGetupProbeHelper(float fCustomZProbeEndOffset) :
CShapeTestCapsuleDesc(),
m_iNumProbe(0),
m_fCustomZProbeEndOffset(fCustomZProbeEndOffset)
{
}

CGetupProbeHelper::~CGetupProbeHelper()
{
	ResetSearch();
}

bool CGetupProbeHelper::StartClosestPositionSearch(const float fSearchRadius, const u32 iFlags, const float fZWeightingAbove, const float fZWeightingAtOrBelow, const float fMinimumSpacing)
{
	// Save off the search origin
	return m_ClosestPositionHelper.StartClosestPositionSearch(GetStart(), fSearchRadius, iFlags, fZWeightingAbove, fZWeightingAtOrBelow, fMinimumSpacing);
}

void CGetupProbeHelper::SubmitProbes()
{
	for (int i = 0; i < m_iNumProbe; i++)
	{
		SetResultsStructure(&m_CapsuleResults[i]);
		SetCapsule(GetStart(), GetProbeEndPosition(i), GetRadius());
		WorldProbe::GetShapeTestManager()->SubmitTest(*this, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
}

bool CGetupProbeHelper::GetAllResultsReady()
{
	bool bAllResultsReady = true;

	if (m_ClosestPositionHelper.IsSearchActive())
	{
		SearchStatus searchResult;
		// If we're waiting on a pathserver query...
		if (!m_ClosestPositionHelper.GetSearchResults(searchResult, m_iNumProbe, m_vEndPosition, CClosestPositionRequest::MAX_NUM_RESULTS))
		{
			bAllResultsReady = false;
		}
		else
		{
			SubmitProbes();
		}
	}

	if (bAllResultsReady)
	{
		for (int i = 0; i < GetNumResult(); i++)
		{
			if (m_CapsuleResults[i].GetWaitingOnResults())
			{
				bAllResultsReady = false;
				break;
			}
		}
	}

	return bAllResultsReady;
}

bool CGetupProbeHelper::GetClosestPosition(Vector3& vClosestPosition, bool bForceLOSCheck, bool bDoDoorProbe) const
{
	float fClosestDist2 = FLT_MAX;
	float fClosestDistZ = FLT_MAX;
	bool bClosestHitDetected = true;
	for (int i = 0; i < GetNumResult(); i++)
	{
		Vector3 vDiff(GetEndPosition(i) - GetStart());
		float fDist2 = vDiff.Mag2();
		bool bHitDetected = m_CapsuleResults[i][0].GetHitDetected();
		// Try to prefer positions that have line-of-sight and are lower than the ped (i.e. positions on the floor below us rather than a floor above us)
		// If this position is closer and is below the ped or our current closest position is above the ped
		// Or if this position is below the ped and our current closest position is above the ped
		// Then we've found a new closest position
		if (((bClosestHitDetected && !bHitDetected) || (fDist2 < fClosestDist2 && (vDiff.z <= 0.0f || fClosestDistZ > 0.0f)) || (vDiff.z <= 0.0f && fClosestDistZ > 0.0f)) &&
			(!bForceLOSCheck || !bHitDetected) && (bClosestHitDetected || !bHitDetected))
		{
			// TODO: Make these probes asynchronous as well...
			if ((!bDoDoorProbe || CheckDoorProbe(i)) && CheckGroundNormal(i))
			{
				vClosestPosition = GetEndPosition(i);
				fClosestDist2 = fDist2;
				fClosestDistZ = vDiff.z;
				bClosestHitDetected = m_CapsuleResults[i][0].GetHitDetected();
			}
		}
	}

	if (fClosestDist2 < FLT_MAX)
	{
		return true;
	}

	return false;
}

bool CGetupProbeHelper::CheckDoorProbe(s32 i) const
{
	WorldProbe::CShapeTestCapsuleDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<12> probeResults;

	probeDesc.SetCapsule(GetStart(), GetEndPosition(i), GetRadius());
	probeDesc.SetTypeFlags(ArchetypeFlags::GTA_OBJECT_TYPE);
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetDoInitialSphereCheck(true);

	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		for (s32 i=0; i<probeResults.GetNumHits(); i++)
		{
			const CEntity* pEnt = probeResults[i].GetHitEntity();
			if (pEnt && pEnt->GetIsTypeObject())
			{
				const CObject* pObj = static_cast<const CObject*>(pEnt);
				if (pObj->IsADoor())
				{
					const CDoor* pDoor = static_cast<const CDoor*>(pEnt);
					if (pDoor->IsBaseFlagSet(fwEntity::IS_FIXED) && pDoor->IsDoorShut())
					{
						return false;
					}
				}
			}
		}
	}

	return true;
}

bool CGetupProbeHelper::CheckGroundNormal(s32 i) const
{
	Vector3 vNormal;
	bool bResult = false;
	WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, GetEndPosition(i), &bResult, &vNormal);

	if (bResult && CTaskSlopeScramble::GetIsTooSteep(vNormal))
	{
		return false;
	}

	return true;
}

const WorldProbe::CShapeTestHitPoint& CGetupProbeHelper::GetResult(int iResult) const
{
	Assert(iResult >= 0 && iResult < GetNumResult() && m_CapsuleResults[iResult].GetResultsReady());

	return m_CapsuleResults[iResult][0];
}

const Vector3& CGetupProbeHelper::GetEndPosition(int iResult) const
{
	Assert(iResult >= 0 && iResult < GetNumResult() && !m_ClosestPositionHelper.IsSearchActive());

	return m_vEndPosition[iResult];
}

Vector3 CGetupProbeHelper::GetProbeEndPosition(int iResult) const
{
	Vector3 vProbePosition(GetEndPosition(iResult));
	
	if (m_fCustomZProbeEndOffset != FLT_MAX)
	{
		vProbePosition.z += m_fCustomZProbeEndOffset;
	}
	else
	{
		// If the end position is within a certain tolerance height-wise as the start position then probe from the start height to the end height.
		// Otherwise, we use the start position's height. This is so that the probe doesn't clip ledges very close to the start/end position. 
		// If the heights are very far off then we want to probe to the actual end position found otherwise we might warp through geometry. 
		// If the heights are very close to each other there's less chance of hitting a ledge so probe to the actual end position
		float fZDiff = Abs(vProbePosition.z - GetStart().z);
		static bank_float sfMinimumProbeHeightThreshold = 0.25f;
		static bank_float sfMaximumProbeHeightThreshold = 2.0f;
		if (fZDiff <= sfMaximumProbeHeightThreshold && fZDiff >= sfMinimumProbeHeightThreshold)
		{
			vProbePosition.z = GetStart().z;
		}
		else
		{
			vProbePosition.z += sfMinimumProbeHeightThreshold;
		}
	}

	return vProbePosition;
}

void CGetupProbeHelper::ResetSearch()
{
	m_ClosestPositionHelper.ResetSearch();

	for (int i = 0; i < GetNumResult(); i++)
	{
		m_CapsuleResults[i].Reset();
	}

	m_iNumProbe = 0;
}

//-----------------------------------------------------------------------------------

#if USE_PM_STORE
//
// helper class to be included in tasks that wish to stream their paramatised motion templates and clips
// all you have to do is make the helper a member of the task, and then RequestPm by dictionary index and hashkey
//
CPmRequestHelper::CPmRequestHelper()
{
	m_bPmReferenced = false;
	m_nRequiredPmDict = -1;

}

CPmRequestHelper::~CPmRequestHelper()
{
	ReleasePm();
	taskAssert(!m_bPmReferenced);
}

bool CPmRequestHelper::RequestPm( s32 nRequiredPmDict, s32 flags )
{
	// If the dictionary has changed, remove any previously referenced dict
	if(nRequiredPmDict!=m_nRequiredPmDict)
	{
		if(m_bPmReferenced)
		{
			taskAssert(m_nRequiredPmDict != -1);
			g_PmDictionaryStore.RemoveRef(m_nRequiredPmDict, REF_OTHER);
			m_nRequiredPmDict = -1;
			m_bPmReferenced = false;
		}

		m_nRequiredPmDict = nRequiredPmDict;
	}

	// Try and stream the dictionary
	if(m_nRequiredPmDict!=-1)
	{
		// Is the current slot valid
		taskAssert(g_PmDictionaryStore.IsValidSlot(m_nRequiredPmDict));
		taskAssert(g_PmDictionaryStore.IsObjectInImage(m_nRequiredPmDict));
		if(!g_PmDictionaryStore.HasObjectLoaded(m_nRequiredPmDict))
		{
			g_PmDictionaryStore.StreamingRequest(m_nRequiredPmDict, flags);
		}
		// The pm is streamed in and ready to use
		else
		{
			if(!m_bPmReferenced)
			{
				g_PmDictionaryStore.AddRef(m_nRequiredPmDict, REF_OTHER);
				m_bPmReferenced = true;
			}
			return true;
		}
	}

	// pm is not available to use
	return false;
}
#endif // USE_PM_STORE

CPrioritizedClipSetRequestHelper::CPrioritizedClipSetRequestHelper()
: m_pRequest(NULL)
{

}

CPrioritizedClipSetRequestHelper::~CPrioritizedClipSetRequestHelper()
{
	//Release the clip set.
	ReleaseClipSet();
}

bool CPrioritizedClipSetRequestHelper::CanUseClipSet() const
{
	//Ensure the request is active.
	if(!IsActive())
	{
		return false;
	}

	//Ensure the clip set is streamed in.
	if(!IsClipSetStreamedIn())
	{
		return false;
	}

	return true;
}

fwClipSet* CPrioritizedClipSetRequestHelper::GetClipSet() const
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return NULL;
	}

	return m_pRequest->GetClipSet();
}

fwMvClipSetId CPrioritizedClipSetRequestHelper::GetClipSetId() const
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return CLIP_SET_ID_INVALID;
	}
	
	return m_pRequest->GetClipSetId();
}

bool CPrioritizedClipSetRequestHelper::IsActive() const
{
	//Ensure the request is valid.
	if(!m_pRequest)
	{
		return false;
	}

	return true;
}

bool CPrioritizedClipSetRequestHelper::IsClipSetStreamedIn() const
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return false;
	}
	
	return m_pRequest->IsClipSetStreamedIn();
}

bool CPrioritizedClipSetRequestHelper::IsClipSetTryingToStreamIn() const
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return false;
	}
	
	return m_pRequest->IsClipSetTryingToStreamIn();
}

bool CPrioritizedClipSetRequestHelper::IsClipSetTryingToStreamOut() const
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return false;
	}
	
	return m_pRequest->IsClipSetTryingToStreamOut();
}

void CPrioritizedClipSetRequestHelper::ReleaseClipSet()
{
	//Ensure the request is valid.
	if(!m_pRequest)
	{
		return;
	}
	
	//Remove the request.
	CPrioritizedClipSetStreamer::GetInstance().RemoveRequest(m_pRequest->GetClipSetId());
	
	//Clear the request.
	m_pRequest = NULL;
}

void CPrioritizedClipSetRequestHelper::RequestClipSet(fwMvClipSetId clipSetId)
{
	//Ensure the clip set is valid.
	if(!taskVerifyf(clipSetId != CLIP_SET_ID_INVALID, "The clip set is invalid."))
	{
		return;
	}

	//Check if the request is valid.
	if(m_pRequest)
	{
		//Check if the clip set matches.
		if(clipSetId == m_pRequest->GetClipSetId())
		{
			return;
		}

		//Release the clip set.
		ReleaseClipSet();
	}

	//Assert that the request is invalid.
	taskAssertf(!m_pRequest, "The request is valid.");
	
	//Add the request.
	m_pRequest = CPrioritizedClipSetStreamer::GetInstance().AddRequest(clipSetId);
}

void CPrioritizedClipSetRequestHelper::RequestClipSetIfExists(fwMvClipSetId clipSetId)
{
	//Ensure the clip set is valid.
	if(clipSetId == CLIP_SET_ID_INVALID)
	{
		return;
	}

	//Request the clip set.
	RequestClipSet(clipSetId);
}

void CPrioritizedClipSetRequestHelper::SetStreamingPriorityOverride(eStreamingPriority nStreamingPriority)
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return;
	}

	//Set the streaming priority override.
	m_pRequest->SetStreamingPriorityOverride(nStreamingPriority);
}

bool CPrioritizedClipSetRequestHelper::ShouldClipSetBeStreamedIn() const
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return false;
	}

	return m_pRequest->ShouldClipSetBeStreamedIn();
}

bool CPrioritizedClipSetRequestHelper::ShouldClipSetBeStreamedOut() const
{
	//Ensure the request is valid.
	if(!taskVerifyf(m_pRequest, "The request is invalid."))
	{
		return false;
	}

	return m_pRequest->ShouldClipSetBeStreamedOut();
}

bool CPrioritizedClipSetRequestHelper::ShouldUseClipSet() const
{
	//Ensure we can use the clip set.
	if(!CanUseClipSet())
	{
		return false;
	}

	//Ensure the clip set should be streamed in.
	if(!ShouldClipSetBeStreamedIn())
	{
		return false;
	}

	//Ensure the clip set should not be streamed out.
	if(ShouldClipSetBeStreamedOut())
	{
		return false;
	}

	return true;
}

//
// Ambient clipId helper class to be included as a member of tasks that need to stream clips
//

CAmbientClipRequestHelper::CAmbientClipRequestHelper()
: m_clipDictIndex(0)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipState(AS_noGroupSelected)
, m_bHasPermissionToLoad(false)
, m_bIsClipSetSynchronized(false)
{

}

CAmbientClipRequestHelper& CAmbientClipRequestHelper::operator=(const CAmbientClipRequestHelper& ambientClipRequestHelper) 
{
	m_clipSetId					=	ambientClipRequestHelper.m_clipSetId;
	m_bIsClipSetSynchronized	=	ambientClipRequestHelper.m_bIsClipSetSynchronized;
	m_bHasPermissionToLoad		=	ambientClipRequestHelper.m_bHasPermissionToLoad;
	m_clipDictIndex				=	ambientClipRequestHelper.m_clipDictIndex;
	m_clipState					=	ambientClipRequestHelper.m_clipState;
	
	if(ambientClipRequestHelper.m_clipRequestHelper.GetCurrentlyRequiredDictIndexConst()!=-1)
	{
		m_clipRequestHelper = ambientClipRequestHelper.m_clipRequestHelper;
	}

	if(m_bHasPermissionToLoad)
	{
		//The helper we are copying and about to be deleted had permission granted so register this helper too
		// This used to be a member variable m_iPriority which got set to 0 by the constructor,
		// and never changed:
		const int iPriority = 0;
		CAmbientClipStreamingManager::GetInstance()->RegisterClipDictionaryUse(m_clipDictIndex, iPriority);
	}

	return *this;
}

CAmbientClipRequestHelper::~CAmbientClipRequestHelper()
{
	ResetClipSelection();
	ReleaseClips();
	Assert(!m_bHasPermissionToLoad);
}

bool CAmbientClipRequestHelper::RequestClips(bool bRequiresManagerPermissionToLoad)
{
	if( GetState() == AS_groupStreamed )
	{
		return true;
	}

	aiFatalAssertf(m_clipState == AS_groupSelected, "Invalid state for clip request!");

	if( m_clipRequestHelper.GetCurrentlyRequiredDictIndex() != 0 &&
		m_clipRequestHelper.GetCurrentlyRequiredDictIndex() != m_clipDictIndex )
	{
		m_clipRequestHelper.ReleaseClips();
		m_bHasPermissionToLoad = false;
	}

	if( bRequiresManagerPermissionToLoad )
	{
		if(!m_bHasPermissionToLoad)
		{
			// This used to be a member variable m_iPriority which got set to 0 by the constructor,
			// and never changed:
			const int iPriority = 0;

			// Call to clipId manager to request loading a particular clip clipSetId
			if( CAmbientClipStreamingManager::GetInstance()->RequestClipDictionary(m_clipDictIndex, iPriority) )
			{
				m_bHasPermissionToLoad = true;
				CAmbientClipStreamingManager::GetInstance()->RegisterClipDictionaryUse(m_clipDictIndex, iPriority);
			}
		}
	}

	if(m_bHasPermissionToLoad || !bRequiresManagerPermissionToLoad)
	{
		if(m_clipRequestHelper.RequestClipsByIndex(m_clipDictIndex))
		{
			SetState(AS_groupStreamed);
		}
	}
	return false;
}

void CAmbientClipRequestHelper::ReleaseClips()
{
	// Call to clipId manager to reset interest in clipId clipSetId
	if( m_bHasPermissionToLoad )
	{
		CAmbientClipStreamingManager::GetInstance()->ReleaseClipDictionaryUse( m_clipRequestHelper.GetCurrentlyRequiredDictIndex() );
	}
	m_bHasPermissionToLoad = false;

	m_clipRequestHelper.ReleaseClips();
	ResetClipSelection();
	SetState(AS_noGroupSelected);
}


// Chooses a conditional clip from the supplied group, and then chooses an clip set from within that
// The clip set to stream in, and later choose clip clip from is stored within the helper
bool CAmbientClipRequestHelper::ChooseRandomConditionalClipSet(CPed * pPed, CConditionalAnims::eAnimType clipType, const char * pConditionalAnimsName)
{
	const CConditionalAnimsGroup * pConditionalAnimsGroup = CONDITIONALANIMSMGR.GetConditionalAnimsGroup(pConditionalAnimsName); 
	s32 ConditionalAnimChosen = -1;
	s32 chosenGroupPriority = -1;
	CScenarioCondition::sScenarioConditionData conditionData;
	conditionData.pPed = pPed;
	CAmbientAnimationManager::ChooseConditionalAnimations(conditionData, chosenGroupPriority, pConditionalAnimsGroup, ConditionalAnimChosen, CConditionalAnims::AT_VARIATION);
	if ( ConditionalAnimChosen>=0 )
	{
		return ChooseRandomClipSet(conditionData,  pConditionalAnimsGroup->GetAnims(ConditionalAnimChosen), clipType);
	}

	return false;
}

bool CAmbientClipRequestHelper::ChooseRandomClipSet(const CScenarioCondition::sScenarioConditionData& conditionalData, const CConditionalAnims * pConditionalAnims, CConditionalAnims::eAnimType clipType, u32 uChosenPropHash)
{
	ReleaseClips();

	aiAssertf(pConditionalAnims,"No conditional clips chosen\n");

	if ( pConditionalAnims )
	{
		const CConditionalAnims::ConditionalClipSetArray * pConditionalAnimsArray = pConditionalAnims->GetClipSetArray(clipType);

		const CConditionalClipSet * pConClipSet = NULL;

		if ( pConditionalAnimsArray )
		{
			pConClipSet = pConditionalAnims->ChooseClipSet(*pConditionalAnimsArray,conditionalData);
		}

		if ( pConClipSet )
		{
			m_clipSetId = fwMvClipSetId(pConClipSet->GetClipSetHash());

			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
			if(!taskVerifyf(pClipSet, "Clip Set '%s' not found", m_clipSetId.GetCStr()))
			{
				return false;
			}

			m_clipDictIndex = pClipSet->GetClipDictionaryIndex().Get();
			taskAssertf(m_clipDictIndex >=0, "Failed to find clip dictionary for clipset '%s'", m_clipSetId.GetCStr());

			// Check for and load any prop
			if( pConditionalAnims->GetPropSetHash() != 0 )
			{
				const CAmbientPropModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(pConditionalAnims->GetPropSetHash());

				if (!Verifyf(pPropSet, "Could not get a propset from conditional anim hash %s", pConditionalAnims->GetPropSetName()))
				{
					return false;
				}

				// If there was no specified prop hash, choose one ourselves. [4/23/2013 mdawe]
				if (uChosenPropHash == 0)
				{
					bool gotProp = false;
					strLocalIndex newPropModelIndex = strLocalIndex(0);

					// First, we check if we are already holding a prop that's in the required prop set.
					// If so, we wouldn't want to pick a random prop from the set.
					strLocalIndex uHeldPropModelIndex = CScenarioConditionHasProp::GetPropModelIndex(conditionalData.pPed);
					if(CModelInfo::IsValidModelInfo(uHeldPropModelIndex.Get()))
					{
						CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(uHeldPropModelIndex));
						aiAssert(pModelInfo);
						if(pPropSet->GetContainsModel(pModelInfo->GetHashKey()))
						{
							gotProp = true;
							newPropModelIndex = uHeldPropModelIndex;
						}
					}

					if(!gotProp)
					{
						// We don't already hold a prop from the desired set, decide on one randomly.
						newPropModelIndex = pPropSet->GetRandomModelIndex();
					}

					if( taskVerifyf( CModelInfo::IsValidModelInfo(newPropModelIndex.Get()), "Prop set [%s] with model index [%d] not a valid model index", pPropSet->GetName(), newPropModelIndex.Get() ) )
					{
						const CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(fwModelId(newPropModelIndex));
						Assert(pModelInfo);
						m_requiredProp.SetHash(pModelInfo->GetModelNameHash());
					}
				}
				else
				{
					m_requiredProp.SetHash(uChosenPropHash);
				}
			}

			m_bIsClipSetSynchronized = pConClipSet->GetIsSynchronized();

			//m_iInternalLoopRepeats = pAmbientSet->GetLoopCount();
			SetState(AS_groupSelected);
			return true;
		}
	}
	return false;
}

void  CAmbientClipRequestHelper::SetClipAndProp( fwMvClipSetId clipSetId, u32 propHash )
{
	ReleaseClips();

	m_clipSetId = clipSetId;

	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_clipSetId);
	taskFatalAssertf(pClipSet, "Clip Set '%s' not found", m_clipSetId.GetCStr());

	m_clipDictIndex = pClipSet->GetClipDictionaryIndex().Get();
	taskAssertf(m_clipDictIndex >=0, "Failed to find clip dictionary for clipset '%s'", m_clipSetId.GetCStr());

	if(propHash)
	{
		m_requiredProp.SetHash(propHash);
	}

	// Not sure, perhaps we should let the user pass this in, or maybe there is some
	// way of finding it from the data that's available. Right now, this function
	// isn't actually called.
	m_bIsClipSetSynchronized = false;

	SetState(AS_groupSelected);
}

void CAmbientClipRequestHelper::ResetClipSelection()
{
	m_clipDictIndex = 0;
	m_requiredProp.Clear();
	m_bIsClipSetSynchronized = false;
}

//-------------------------------------------------------------------------
// Tries to randomly select an ambient clipId to playback
//-------------------------------------------------------------------------
bool CAmbientClipRequestHelper::PickClipByGroup_FullConditions( s32 iClipDictIndex, u32& iClipHash )
{
	if( iClipDictIndex != 0 )
	{
		return PickClipByDict_FullConditions(iClipDictIndex, iClipHash);
	}
	return false;
}

//-------------------------------------------------------------------------
// Tries to randomly select an ambient clipId to playback
//-------------------------------------------------------------------------
bool CAmbientClipRequestHelper::PickClipByDict_FullConditions( s32 iClipDictIndex, u32& iClipHash )
{
	// Choose a random clipId from the clipSetId
	s32 iNoClipsInDict = fwAnimManager::CountAnimsInDict(iClipDictIndex);
	Assert( iNoClipsInDict <= MAX_EXAMINABLE_CLIPS );
	iNoClipsInDict = MIN( MAX_EXAMINABLE_CLIPS, iNoClipsInDict );
	u32 aPossibleClipHash[MAX_EXAMINABLE_CLIPS];
	s32 iNoClips = 0;

	// Form a list of valid ped clips (not prop ones)
	for( s32 i = 0; i < iNoClipsInDict; i++ )
	{
		const crClip* pClip = fwAnimManager::GetClipInDict( iClipDictIndex, i );
		const char* pClipName = pClip ? pClip->GetName() : NULL;
		if(pClipName)
		{
			s32 iPath = 0;
			for(s32 j = istrlen(pClipName)-1; j >= 0; j--)
			{
				if(pClipName[j] == '/' || pClipName[j] == '\\')
				{
					iPath = j;
					break;
				}
			}

			if(strncmp(&pClipName[iPath + 1], "prop_", 5))
			{
				aPossibleClipHash[iNoClips] = fwAnimManager::GetAnimHashInDict( iClipDictIndex, i );
				++iNoClips;
			}
		}
	}

	// Is there a valid clipId
	if( iNoClips > 0 )
	{
		// Pick a random clipId from the valid set
		s32 iRandom = fwRandom::GetRandomNumberInRange(0, iNoClips);
		iClipHash = aPossibleClipHash[iRandom];
		return true;
	}
	return false;
}

// Picks a random clip from the clipset to play. Always picks a clip if one exists.
bool CAmbientClipRequestHelper::PickRandomClipInClipSet(const fwMvClipSetId &clipSetId, u32& iClipHash)
{
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (aiVerifyf(pClipSet, "Invalid clipset '%s'", clipSetId.GetCStr()))
	{
		s32 iNumClipsInSet = pClipSet->GetClipItemCount();
		if( iNumClipsInSet == 0 )
		{
			return PickClipByDict_FullConditions( pClipSet->GetClipDictionaryIndex().Get(), iClipHash );
		}

		atFixedArray<u32, MAX_EXAMINABLE_CLIPS> aPossibleClipHash;
		s32 iNoClips = FillClipHashArrayWithExistingClips(aPossibleClipHash, pClipSet);

		// Is there a valid clipId
		if( iNoClips > 0 )
		{
			// Pick a random clipId from the valid set
			s32 iRandom = fwRandom::GetRandomNumberInRange(0,iNoClips);
			iClipHash = aPossibleClipHash[iRandom];
			return true;
		}
	}
	return false;
}

// Picks a random clip from the clipset to play, trying not to repeat the last animation played.
// Also attempts to pick a clip that's not been recently played by any entity.
// Can fail if the randomly selected clip was recently played.
bool CAmbientClipRequestHelper::PickRandomClipInClipSetUsingBlackBoard(const CPed* pPed, const fwMvClipSetId &clipSetId, u32& iClipHash, CAnimBlackboard::TimeDelayType eTimeDelayType)
{
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if (aiVerifyf(pClipSet, "Invalid clipset '%s'", clipSetId.GetCStr()))
	{
		if( pClipSet->GetClipItemCount() == 0 )
		{
			return PickClipByDict_FullConditions( pClipSet->GetClipDictionaryIndex().Get(), iClipHash );
		}

		atFixedArray<u32, MAX_EXAMINABLE_CLIPS> aPossibleClipHash;
		s32 iNoClips = FillClipHashArrayWithExistingClips(aPossibleClipHash, pClipSet);

		// Is there a valid clipId
		if( iNoClips > 0 )
		{
			// Pick a random clipId from the valid set
			s32 iRandom = fwRandom::GetRandomNumberInRange(0,iNoClips);

			CAnimBlackboard::AnimBlackboardItem lastPlayedAnim = CAnimBlackboard::AnimBlackboardItem();

			CPedIntelligence* pPedIntelligence = NULL;
			if (aiVerify(pPed))
			{
				pPedIntelligence = pPed->GetPedIntelligence();
				if (pPedIntelligence)
				{
					lastPlayedAnim = pPedIntelligence->GetLastBlackboardAnimPlayed();
				}
			}

			// Prevent playing the same clip twice [12/21/2012 mdawe]
			// If there's only one clip, this will still allow that clip to be played.
			if ( clipSetId.GetHash() == lastPlayedAnim.GetClipSetHash() 
				&& aPossibleClipHash[iRandom] == lastPlayedAnim.GetClipIndex() )
			{
				++iRandom;
				if (iRandom == iNoClips)
				{
					iRandom = 0;
				}
			}

			// Check to see how recently this anim has been played.
			CAnimBlackboard::AnimBlackboardItem blackboardItem(clipSetId.GetHash(), aPossibleClipHash[iRandom], eTimeDelayType);
			if (ANIMBLACKBOARD.AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(blackboardItem, pPed->GetPedIntelligence()))
			{
				iClipHash = aPossibleClipHash[iRandom];
				return true;
			}
			else
			{
				return false;
			}
		}
	}
	return false;
}

//Returns the number of clips found and put into the array
s32 CAmbientClipRequestHelper::FillClipHashArrayWithExistingClips(atFixedArray<u32, MAX_EXAMINABLE_CLIPS>& aPossibleClipHash, const fwClipSet* pClipSet)
{
	taskAssert(pClipSet);

	s32 iNoClips = 0;
	s32 iNumClipsInSet = pClipSet->GetClipItemCount();

	aiAssert( iNumClipsInSet <= MAX_EXAMINABLE_CLIPS );
	iNumClipsInSet = MIN( MAX_EXAMINABLE_CLIPS, iNumClipsInSet );

	// Form a list of valid ped clips (not prop ones)
	for( s32 i = 0; i < iNumClipsInSet; i++ )
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(pClipSet->GetClipDictionaryIndex().Get(), pClipSet->GetClipItemIdByIndex(i));
		const char* pClipName = pClip ? pClip->GetName() : NULL;
		if(pClipName)
		{
			s32 iPath = 0;
			for(s32 j = istrlen(pClipName)-1; j >= 0; j--)
			{
				if(pClipName[j] == '/' || pClipName[j] == '\\')
				{
					iPath = j;
					break;
				}
			}

			if(strncmp(&pClipName[iPath + 1], "prop_", 5))
			{
				aPossibleClipHash.Append() = pClipSet->GetClipItemIdByIndex(i);
				++iNoClips;
			}
		}
	}
	return iNoClips;
}

bool CAmbientClipRequestHelper::PickClipByGroup_SimpleConditions( const CDynamicEntity* ASSERT_ONLY(pDynamicEntity), const fwMvClipSetId &clipSetId, fwMvClipId& iClipID )
{
	fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	if(!taskVerifyf(pClipSet, "Invalid clipset '%s'", clipSetId.GetCStr()))
	{
		return false;
	}

	Assert(pDynamicEntity);
	Assert(pDynamicEntity->GetIsTypePed());

	//Printf("Choosing clip from set %s\n",pClipSet->GetName().GetCStr());

	// Choose a random clipId from the set
	s32 iNoClipsInSet = pClipSet->GetClipItemCount();
	if ( iNoClipsInSet > 0 )
	{
		// Pick a random clipId from the valid set
		s32 iRandom = fwRandom::GetRandomNumberInRange(0,iNoClipsInSet);
		iClipID.SetHash(pClipSet->GetClipItemIdByIndex(iRandom));

		//Printf("Chosen clip %s\n",pClipSet->GetClipItemArray()[iRandom].m_clip.GetCStr());
		return true;
	}

	return false;


/*	u32 auiAnimation[MAX_CLIPS];
	s32 iClipCount = 0;
	Assert(pDynamicEntity);
	Assert(pDynamicEntity->GetIsTypePed());
	bool bFoundGroupDef = false;
	// First try and find an clip association and pick the clipId from that set
	if( clipSetId != CLIP_SET_ID_INVALID )
	{
		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		taskAssert(pClipSet);
		
		// Choose a random clipId from the set
		s32 iNoClipsInSet = pClipSet->GetClipItemArray().GetCount();
		taskAssert( iNoClipsInSet <= MAX_EXAMINABLE_CLIPS );
		iNoClipsInSet = MIN( MAX_EXAMINABLE_CLIPS, iNoClipsInSet );

		// Check the clips to make sure they meet the current criteria (e.g. walking, standing, in cover)
		for( s32 i = 0; i < iNoClipsInSet; i++ )
		{
			u32 iThisClipHash = pClipSet->GetClipItemArray()[i].m_clip.GetHash();

			s32 iDictIndex = fwAnimManager::FindSlotFromHashKey(pClipSet->GetClipDictionaryName().GetHash());	
			const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, iThisClipHash);	
			if( pClip )
			{
				if( iDesiredClipHash != 0 && iThisClipHash == iDesiredClipHash )
				{
					iClipHash = iDesiredClipHash;
					return true;
				}
				Assert(iClipCount<MAX_CLIPS);
				if( iClipCount < MAX_CLIPS )
					auiAnimation[iClipCount++] = iThisClipHash;
			}
		}
	}

	// If a valid definition was not found for this clipSetId, add all clips from the clipId set
	if( !bFoundGroupDef )
	{
		s32 iDictIndex = -1;

		if (iSpecificDictIndex != -1)
		{
			iDictIndex = iSpecificDictIndex;
			
			s32 iNoClipsInDict = fwAnimManager::CountAnimsInDict(iDictIndex);
			for( s32 i = 0; i < iNoClipsInDict; i++ )
			{
				u32 uiClipHash = fwAnimManager::GetAnimHashInDict(iDictIndex, i);
				const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, uiClipHash);
				if( pClip )
				{
					Assert(iClipCount<MAX_CLIPS);
					if( iClipCount < MAX_CLIPS )
						auiAnimation[iClipCount++] = uiClipHash;
				}
			}
		}
		else
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
			taskAssert(pClipSet);
			iDictIndex = fwAnimManager::FindSlotFromHashKey(pClipSet->GetClipDictionaryName().GetHash());
			s32 iNoClipsInClipSet = pClipSet->GetClipItemArray().GetCount();
			for( s32 i = 0; i < iNoClipsInClipSet; i++ )
			{
				u32 uiClipHash = pClipSet->GetClipItemArray()[i].m_clip.GetHash();
				const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictIndex, uiClipHash);
				if( pClip )
				{
					Assert(iClipCount<MAX_CLIPS);
					if( iClipCount < MAX_CLIPS )
						auiAnimation[iClipCount++] = uiClipHash;
				}
			}
		}
	}

	if( iClipCount == 0 )
	{
		return false;
	}

	// Pick a random clipId from the valid set
	s32 iRandom = fwRandom::GetRandomNumberInRange(0,iClipCount);
	iClipHash = auiAnimation[iRandom];
	return true;*/
}

//
// Manager responsible for managing the number of loaded clip sets, grants or denies loading permission
// to CAmbientClipRequestHelper which should be used for loading all ambient clipId sets.
//

CAmbientClipStreamingManager CAmbientClipStreamingManager::ms_instance;

CAmbientClipStreamingManager::CAmbientClipStreamingManager()
{
	Init();
}

CAmbientClipStreamingManager::~CAmbientClipStreamingManager()
{
}

void CAmbientClipStreamingManager::Init()
{
	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_REQUESTS; i++ )
	{
		m_requestedAnimationDictionaries[i].m_iClipDict			= 0;
		m_requestedAnimationDictionaries[i].m_iNumberRequests		= 0;
		m_requestedAnimationDictionaries[i].m_iRequestPriority	= 0;
#if !__FINAL
		m_previousRequestedAnimationDictionaries[i] = m_requestedAnimationDictionaries[i];
#endif
	}

	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_IN_USE; i++ )
	{
		m_usedAnimationDictionaries[i].m_iClipDict	= 0;
		m_usedAnimationDictionaries[i].m_iNumberUses	= 0;
		m_usedAnimationDictionaries[i].m_iUsePriority	= 0;
	}
}

// Processes all the requests and grants those of highest priority
// meaning calls to RequestClipGroup will succeed next update
void CAmbientClipStreamingManager::Update()
{
	// Keep a debug record of all requests for rendering
#if !__FINAL
	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_REQUESTS; i++ )
	{
		m_previousRequestedAnimationDictionaries[i] = m_requestedAnimationDictionaries[i];
	}
#endif

	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_IN_USE; i++ )
	{
		// Remove any clipId groups no longer used
		CClipDictionaryUse* pClipUse = &m_usedAnimationDictionaries[i];
		if( pClipUse->m_iClipDict != 0 &&
			pClipUse->m_iNumberUses <= 0 )
		{
			pClipUse->m_iClipDict = 0;
		}

		// Try to fill any unused clipId uses with requests
		if( pClipUse->m_iClipDict == 0 )
		{
			CClipDictionaryRequest* pClipRequest = GetHighestPriorityRequest();
			if( pClipRequest )
			{
				pClipUse->m_iClipDict = pClipRequest->m_iClipDict;
				pClipUse->m_iNumberUses = 0;
				pClipUse->m_iUsePriority = 0;
				pClipRequest->m_iClipDict = 0;
			}
		}
	}

	// Reset any remaining requests
	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_REQUESTS; i++ )
	{
		m_requestedAnimationDictionaries[i].m_iClipDict			= 0;
		m_requestedAnimationDictionaries[i].m_iNumberRequests		= 0;
		m_requestedAnimationDictionaries[i].m_iRequestPriority	= 0;
	}
}

// called from CAmbientClipRequestHelper to request loading of an ambient clipId clipSetId
bool CAmbientClipStreamingManager::RequestClipDictionary(s32 clipDictIndex, s32 iPriority)
{
	// If this clipId clipSetId is in use, it is avaiable
	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_IN_USE; i++ )
	{
		if( m_usedAnimationDictionaries[i].m_iClipDict == clipDictIndex )
		{
			return true;
		}
	}

	// Find a free, or add to an existing request slot
	// these are processed during the update
	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_REQUESTS; i++ )
	{
		if( m_requestedAnimationDictionaries[i].m_iClipDict == 0 )
		{
			m_requestedAnimationDictionaries[i].m_iClipDict		= clipDictIndex;
			m_requestedAnimationDictionaries[i].m_iNumberRequests = 1;
			m_requestedAnimationDictionaries[i].m_iRequestPriority= iPriority;
			return false;
		}
		else if( m_requestedAnimationDictionaries[i].m_iClipDict == clipDictIndex )
		{
			++m_requestedAnimationDictionaries[i].m_iNumberRequests;
			m_requestedAnimationDictionaries[i].m_iRequestPriority	= MAX(iPriority, m_requestedAnimationDictionaries[i].m_iRequestPriority);
			return false;
		}
	}
	Assertf(0, "Ran out of CAmbientClipStreamingManager requests!");
	return false;
}


// called from CAmbientClipRequestHelper to register the use of an clipgroup (it is being requested for loading)
void CAmbientClipStreamingManager::RegisterClipDictionaryUse(s32 clipDictIndex, s32 iPriority)
{
	// Remove any clipId groups no longer used
	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_IN_USE; i++ )
	{
		if( m_usedAnimationDictionaries[i].m_iClipDict == clipDictIndex )
		{
			++m_usedAnimationDictionaries[i].m_iNumberUses;
			m_usedAnimationDictionaries[i].m_iUsePriority = MAX( m_usedAnimationDictionaries[i].m_iUsePriority, iPriority );
			return;
		}
	}
	Assertf(0, "CAmbientClipStreamingManager::RegisterClipDictionaryUse attempting to register invalid use!");
}


// called from CAmbientClipRequestHelper to release the use of clipgroup (it is no longer being used)
void CAmbientClipStreamingManager::ReleaseClipDictionaryUse(s32 clipDictIndex)
{
	// Remove any clipId groups no longer used
	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_IN_USE; i++ )
	{
		if( m_usedAnimationDictionaries[i].m_iClipDict == clipDictIndex )
		{
			--m_usedAnimationDictionaries[i].m_iNumberUses;
			return;
		}
	}
	Assertf(0, "CAmbientClipStreamingManager::ReleaseClipDictionaryUse attempting to release invalid use!");
}

// Returns the highest priority request
CClipDictionaryRequest* CAmbientClipStreamingManager::GetHighestPriorityRequest()
{
	s32 iHighestPriority = -1;
	s32 iHighestRequest = -1;

	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_REQUESTS; i++ )
	{
		if( m_requestedAnimationDictionaries[i].m_iClipDict != 0 )
		{
			if( iHighestRequest == -1 ||
				m_requestedAnimationDictionaries[i].m_iRequestPriority > iHighestPriority )
			{
				iHighestPriority = m_requestedAnimationDictionaries[i].m_iRequestPriority;
				iHighestRequest = i;
			}
		}
	}

	if( iHighestRequest != -1 )
	{
		return &m_requestedAnimationDictionaries[iHighestRequest];
	}
	return NULL;
}


#if !__FINAL
bool CAmbientClipStreamingManager::DISPLAY_AMBIENT_STREAMING_USE = false;

void CAmbientClipStreamingManager::DisplayAmbientStreamingUse()
{
#if !__FINAL
	int iNoOfTexts=0;
#endif

	CTextLayout textLayout;

	static Vector3 ScreenCoors(0.0f, 0.2f, 0.0f);

	// only want to do text for peds close to camera
	// Set origin to be the debug cam, or the player..
	/*
#if 1// #if __DEV 
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#else
	Vector3 vOrigin = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
#endif
	*/

#if !__FINAL
	float fVertDist = 0.065f;
#endif

	Vector2 vTextRenderScale(0.5f,0.5f);
	textLayout.SetScale(&vTextRenderScale);

	u8 iAlpha = 0xff;

	textLayout.SetColor(CRGBA(255,255,255,iAlpha));
	textLayout.SetOrientation(FONT_LEFT);
	textLayout.SetStyle(FONT_STYLE_STANDARD);
	textLayout.SetEdge(1);
	textLayout.SetWrap(Vector2(ScreenCoors.x - 1.0f, ScreenCoors.x + 1.0f));

	textLayout.Render( Vector2(ScreenCoors.x, ScreenCoors.y + iNoOfTexts*fVertDist), "CAmbientClipStreamingManager:");
	iNoOfTexts++;

	textLayout.Render( Vector2(ScreenCoors.x, ScreenCoors.y + iNoOfTexts*fVertDist), "Used Groups:");
	iNoOfTexts++;

	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_IN_USE; i++ )
	{
		if( m_usedAnimationDictionaries[i].m_iClipDict != 0 )
		{
			char tmp[100];
			const char *dictName = g_ClipDictionaryStore.GetName(strLocalIndex(m_usedAnimationDictionaries[i].m_iClipDict));
			if (dictName)
			{
				formatf( tmp, "%s, Uses(%d), Priority(%d)", dictName,m_usedAnimationDictionaries[i].m_iNumberUses,m_usedAnimationDictionaries[i].m_iUsePriority );
				textLayout.Render( Vector2(ScreenCoors.x, ScreenCoors.y + iNoOfTexts*fVertDist), tmp);
				iNoOfTexts++;
			}
		}
	}

	textLayout.Render( Vector2(ScreenCoors.x, ScreenCoors.y + iNoOfTexts*fVertDist), "Requested Groups:");
	iNoOfTexts++;

	for( s32 i = 0; i < MAX_ANIMATION_DICTIONARY_REQUESTS; i++ )
	{
		if( m_previousRequestedAnimationDictionaries[i].m_iClipDict != 0 )
		{
			char tmp[100];
			const char *dictName = g_ClipDictionaryStore.GetName(strLocalIndex(m_previousRequestedAnimationDictionaries[i].m_iClipDict));
			if (dictName)
			{
				formatf( tmp, "%s Requests(%d), Priority(%d)", dictName,m_previousRequestedAnimationDictionaries[i].m_iNumberRequests,m_previousRequestedAnimationDictionaries[i].m_iRequestPriority );
				textLayout.Render( Vector2(ScreenCoors.x, ScreenCoors.y + iNoOfTexts*fVertDist), tmp);
				iNoOfTexts++;
			}
		}
	}
}
#endif


//-------------------------------------------------------------------------
// Class defining an individual transition clipId, with a few matrices for
// comparison
//-------------------------------------------------------------------------
CTransitionClip::CTransitionClip()
{
	m_clipSetId = CLIP_SET_ID_INVALID;
	m_clipId = CLIP_ID_INVALID;
}


//-------------------------------------------------------------------------
// Setup this particular
//-------------------------------------------------------------------------
void CTransitionClip::Setup(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId)
{
	m_clipSetId = clipSetId;
	m_clipId = clipId;

	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, m_clipId);
	if( !pClip )
	{
		Assertf(0, "transition clipId not loaded!" );
		return;
	}

	for( s32 i = 0; i < MAX_BONES_TO_CHECK; i++ )
	{
//		u16 iRageBoneIdx = (u16)FindPlayerPed()->GetBoneIndexFromBoneTag(bonesToCheck[i]);
//		fwAnimManager::GetGlobalSpaceBoneMatrixFromClip(pAnimation, child->GetBoneId(), FindPlayerPed()->GetSkeletonData(), 0.0f, tempMat);
//
	}
}

//-------------------------------------------------------------------------
// Class use to choose which transitional clipId is closest to the current pose
//-------------------------------------------------------------------------
CTransitionClip CTransitionClipSelector::ms_aTransitionClips[MAX_TRANSITIONAL_CLIPS];
s32			CTransitionClipSelector::ms_iNumClips = 0;
bool			CTransitionClipSelector::ms_bInitialised = false;

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
void CTransitionClipSelector::AddPossibleClip( const fwMvClipSetId &clipSetId, const fwMvClipId &clipId )
{
	ms_aTransitionClips[ms_iNumClips++].Setup( clipSetId, clipId );

}

//-------------------------------------------------------------------------
// Helper used to return if there's a valid navmesh route
//-------------------------------------------------------------------------

CNavmeshRouteSearchHelper::CNavmeshRouteSearchHelper()
{
	m_bWaitingForSearchResult = false;
	m_hPathHandle = PATH_HANDLE_NULL;
	m_bRouteEndedInWater = false;
	m_bRouteEndedInInterior = false;
	m_fMaxNavigableAngle = 0.0f;
	m_fEntityRadius = PATHSERVER_PED_RADIUS;
}

CNavmeshRouteSearchHelper::~CNavmeshRouteSearchHelper()
{
	ResetSearch();
}

void CNavmeshRouteSearchHelper::ResetSearch( void )
{
	if( m_bWaitingForSearchResult )
	{
		if( m_hPathHandle != PATH_HANDLE_NULL )
		{
			CPathServer::CancelRequest(m_hPathHandle);
			m_hPathHandle = PATH_HANDLE_NULL;
		}
		m_bWaitingForSearchResult = false;
	}
}
bool CNavmeshRouteSearchHelper::StartSearch( CPed* pPed, const Vector3& vStart, const Vector3& vEnd, float fCompletionRadius, u64 iFlags, u32 iNumInfluenceSpheres, TInfluenceSphere * pInfluenceSpheres, float fReferenceDistance, const Vector3 * pvReferenceVector, const bool bIgnoreVehicles )
{
	Assertf(vStart.x > -50000.0f && vStart.x < 50000.0f && vStart.y > -50000.0f && vStart.y < 50000.0f && vStart.z > -50000.0f && vStart.z < 50000.0f, "Path request start-pos has crazy values");
	Assertf(vEnd.x > -50000.0f && vEnd.x < 50000.0f && vEnd.y > -50000.0f && vEnd.y < 50000.0f && vEnd.z > -50000.0f && vEnd.z < 50000.0f, "Path request end-pos has crazy values");

	Assertf( ((iFlags&PATH_FLAG_FLEE_TARGET)==0) || fReferenceDistance > 0.0f, "If requesting a flee path you must specify a reference distance (this is *not* the same as the path completion radius!)" );
	ResetSearch();

	u64 iSearchFlags = pPed ? pPed->GetPedIntelligence()->GetNavCapabilities().BuildPathStyleFlags()|pPed->GetCurrentMotionTask()->GetNavFlags() : 0;
	if( iFlags != 0 )
	{
		iSearchFlags = iFlags;
	}

	TRequestPathStruct reqStruct;
	reqStruct.m_vPathStart = vStart;
	reqStruct.m_vPathEnd = vEnd;
	reqStruct.m_fCompletionRadius = fCompletionRadius;
	reqStruct.m_fReferenceDistance = fReferenceDistance;
	if(pvReferenceVector)
		reqStruct.m_vReferenceVector = *pvReferenceVector;
	reqStruct.m_iFlags = iSearchFlags;
	reqStruct.m_iNumInfluenceSpheres = iNumInfluenceSpheres;
	sysMemCpy(reqStruct.m_InfluenceSpheres, pInfluenceSpheres, sizeof(TInfluenceSphere)*iNumInfluenceSpheres);
	reqStruct.m_pPed = pPed;
	reqStruct.m_fMaxSlopeNavigable = m_fMaxNavigableAngle;
	reqStruct.m_fEntityRadius = m_fEntityRadius;
	reqStruct.m_bIgnoreTypeVehicles = bIgnoreVehicles;

	if (pPed)
	{
		CPhysical* pPedGroundPhysical = pPed->GetGroundPhysical();
		if (pPedGroundPhysical && pPedGroundPhysical->GetIsTypeVehicle() && ((CVehicle*)pPedGroundPhysical)->m_nVehicleFlags.bHasDynamicNavMesh)
		{
			reqStruct.m_pEntityPathIsOn = pPedGroundPhysical;
			reqStruct.m_iFlags |= PATH_FLAG_DYNAMIC_NAVMESH_ROUTE;
		}
	}

	Assertf(m_hPathHandle==PATH_HANDLE_NULL, "CNavmeshRouteSearchHelper::StartSearch() was called whilst an existing search was already active.  Doing this could fill up the request queue, or indicate that something is spamming the pathfinder.");

	m_hPathHandle = CPathServer::RequestPath(reqStruct);

	if( m_hPathHandle != PATH_HANDLE_NULL )
	{
		m_bWaitingForSearchResult = true;
	}
	return m_bWaitingForSearchResult;
}

bool CNavmeshRouteSearchHelper::GetSearchResults( SearchStatus& searchResult, float &fDistance, Vector3& vLastPoint,
												  Vector3* pvPointsOnRoute, s32* piMaxNumPoints, TNavMeshWaypointFlag* pWaypointFlags)
{
	Assert(m_bWaitingForSearchResult);
	s32 iMaxNumPoints = 0;
	if( piMaxNumPoints )
	{
		iMaxNumPoints = *piMaxNumPoints;
		*piMaxNumPoints = 0;
	}
	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hPathHandle);
	if(ret == ERequest_Ready)
	{
		m_bWaitingForSearchResult = false;
		searchResult = SS_SearchSuccessful;
		// Lock the path & get pointer to result
		int iNumPts = 0;
		Vector3 * pPts = NULL;
		TNavMeshWaypointFlag * pWptFlags = NULL;
		TPathResultInfo	pathResultInfo;
		int pathRetVal = CPathServer::LockPathResult(m_hPathHandle, &iNumPts, pPts, pWptFlags, &pathResultInfo);

		m_vClosestPointFoundToTarget = pathResultInfo.m_vClosestPointFoundToTarget;
		m_bLastRoutePointIsTarget = pathResultInfo.m_bLastRoutePointIsTarget;

		// If a valid route is returned work out how far it is to the target point
		if( pathRetVal == PATH_FOUND && iNumPts > 0 )
		{
			fDistance = 0.0f;
			vLastPoint = pPts[0];
			Vector3 pt;
			// Recurse through all points on the route and calculate the actual distance
			int iNumValidPoints = 0;
			for(int p=0; p<iNumPts; p++)
			{
				if(p < iMaxNumPoints)
				{
					iNumValidPoints++;

					if(pvPointsOnRoute)
					{
						pvPointsOnRoute[p] = pPts[p];
					}

					if(pWaypointFlags)
					{
						pWaypointFlags[p] = pWptFlags[p];
					}
				}

				pt = pPts[p];
				fDistance += pt.Dist(vLastPoint);
				vLastPoint = pt;

				Assertf(pt.x > -50000.0f && pt.x < 50000.0f && pt.y > -50000.0f && pt.y < 50000.0f && pt.z > -50000.0f && pt.z < 50000.0f, "Pathsearch result has crazy values");
			}

			if(piMaxNumPoints)
			{
				*piMaxNumPoints = iNumValidPoints;
			}
			

			m_bRouteEndedInWater =
				((pWptFlags[iNumPts-1].GetSpecialActionFlags() & WAYPOINT_FLAG_IS_ON_WATER_SURFACE)!=0);
			m_bRouteEndedInInterior = ((pWptFlags[iNumPts-1].GetSpecialActionFlags() & WAYPOINT_FLAG_IS_INTERIOR)!=0);
		}
		else
		{
			// Inaccesible
			fDistance = 0.0f;
			vLastPoint.Zero();
			searchResult = SS_SearchFailed;
		}

		// Unlock & clear the path result, so it can be reused by another query
		CPathServer::UnlockPathResultAndClear(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;
		return true;
	}
	else if(ret == ERequest_NotFound)
	{
		// Inaccesible
		fDistance = 0.0f;
		vLastPoint.Zero();
		searchResult = SS_SearchFailed;
		m_bWaitingForSearchResult = false;
		m_hPathHandle = PATH_HANDLE_NULL;
		return true;
	}

	searchResult = SS_StillSearching;
	return false;
}

//////////////////////////////////////////////////////////////////////////
//
// boat avoidance helper.  exists outside of tasks because
// tasks jump state like crazy and we want to be able to keep some
// state across task transitions
//
////////////////////////////////////////////////////////////////od//////////

	static bool s_EnableBoatAvoidance = true;
	static float s_BoatFutureTime = 2.0f;
	static float s_BoatSliceAngleMax = 15.0f;
	static float s_BoatSliceAngleMin = 1.0f;
	static float s_BoatSliceSpeedToAngleRatio = 25.0f;
	static float s_BoatOffset = 0.2f;
	
	float CBoatAvoidanceHelper::ms_DefaultMaxAvoidanceOrientation = 180.0f;

	CBoatAvoidanceHelper::CBoatAvoidanceHelper(CVehicle& in_Vehicle)
		: m_bEnabled(s_EnableBoatAvoidance)
		, m_bEnableSteering(true)
		, m_bEnableSlowingDown(true)
		, m_bReverse(false)
		, m_AvoidanceFeelerScale(1.0f)
		, m_AvoidanceFeelerMaxLength(100.0f)
		, m_fBoatMaxAvoidanceOrientationLeft(-ms_DefaultMaxAvoidanceOrientation)
		, m_fBoatMaxAvoidanceOrientationRight(ms_DefaultMaxAvoidanceOrientation)
		, m_fBoatMaxAvoidanceOrientationLeftThisFrame(-ms_DefaultMaxAvoidanceOrientation)
		, m_fBoatMaxAvoidanceOrientationRightThisFrame(ms_DefaultMaxAvoidanceOrientation)
		, m_ProbeQueryIndex(0)
		, m_fTimeOutOfWater(0.0f)
	{
		m_fBoatAvoidanceOrientation = fwAngle::GetATanOfXY(in_Vehicle.GetTransform().GetForward().GetXf(), in_Vehicle.GetTransform().GetForward().GetYf());
		m_fBoatAvoidanceOrientation = fwAngle::LimitRadianAngleSafe(m_fBoatAvoidanceOrientation);
		m_fBoatDesiredOrientation = m_fBoatAvoidanceOrientation;
		m_fBoatPreviousAvoidanceDelta = 0.0f;

		m_NavmeshLOSHelpers.Resize(m_NavmeshLOSHelpers.GetMaxCount());
		m_NavmeshLOSClear.Resize(m_NavmeshLOSClear.GetMaxCount());
		for(int i = 0; i < m_NavmeshLOSClear.GetCount(); i++ )
		{
			m_NavmeshLOSClear[i] = true;
		}
	}

	CBoatAvoidanceHelper::~CBoatAvoidanceHelper()
	{

	}

	void CBoatAvoidanceHelper::SetAvoidanceOrientation(float in_AvoidanceOrientation)
	{
		m_fBoatAvoidanceOrientation = in_AvoidanceOrientation;
		Assert(FPIsFinite(m_fBoatAvoidanceOrientation));
	}

	void CBoatAvoidanceHelper::SetDesiredOrientation(float in_DesiredOrientation)
	{
		m_fBoatDesiredOrientation = in_DesiredOrientation;
		Assert(FPIsFinite(m_fBoatDesiredOrientation));
	}

	void CBoatAvoidanceHelper::SetMaxAvoidanceOrientationLeft(float in_MaxOrientationAngle)
	{
		m_fBoatMaxAvoidanceOrientationLeft = in_MaxOrientationAngle;
		Assert(FPIsFinite(m_fBoatMaxAvoidanceOrientationLeft));
	}

	void CBoatAvoidanceHelper::SetMaxAvoidanceOrientationRight(float in_MaxOrientationAngle)
	{
		m_fBoatMaxAvoidanceOrientationRight = in_MaxOrientationAngle;
		Assert(FPIsFinite(m_fBoatMaxAvoidanceOrientationRight));
	}

	void CBoatAvoidanceHelper::SetMaxAvoidanceOrientationLeftThisFrame(float in_MaxOrientationAngle)
	{
		m_fBoatMaxAvoidanceOrientationLeftThisFrame = in_MaxOrientationAngle;
		Assert(FPIsFinite(m_fBoatMaxAvoidanceOrientationLeftThisFrame));
	}

	void CBoatAvoidanceHelper::SetMaxAvoidanceOrientationRightThisFrame(float in_MaxOrientationAngle)
	{
		m_fBoatMaxAvoidanceOrientationRightThisFrame = in_MaxOrientationAngle;
		Assert(FPIsFinite(m_fBoatMaxAvoidanceOrientationRightThisFrame));
	}

	void CBoatAvoidanceHelper::SetAvoidanceFlag(AvoidanceFlag in_Flag)
	{
		m_AvoidanceFlags.SetFlag(static_cast<u8>(in_Flag));
	}

	bool CBoatAvoidanceHelper::IsAvoidanceFlagSet(AvoidanceFlag in_Flag) const
	{
		return m_AvoidanceFlags.IsFlagSet(static_cast<u8>(in_Flag));
	}

	float CBoatAvoidanceHelper::ComputeAvoidanceOrientation(float in_Orientation) const
	{
		if ( m_bEnabled && m_bEnableSteering )
		{	
			return m_fBoatAvoidanceOrientation;
		}
		return in_Orientation;
	}

	bool CBoatAvoidanceHelper::IsBlocked() const
	{
		for(int i = 1; i < m_NavmeshLOSClear.GetCount(); i++)
		{
			if ( m_NavmeshLOSClear[i] )
			{
				return false;
			}
		}
		return true;
	}

	void CBoatAvoidanceHelper::SetReverse(bool in_Value)
	{
		if ( m_bReverse != in_Value)
		{
			Reset();
			m_bReverse = in_Value;
		}
	}


	bool CBoatAvoidanceHelper::IsAvoiding() const
	{
		for(int i = 1; i < m_NavmeshLOSClear.GetCount(); i++)
		{
			if ( !m_NavmeshLOSClear[i] )
			{
				return true;
			}
		}
		return false;
	}


	bool CBoatAvoidanceHelper::IsBeached() const
	{
		static float s_fBeachedTime = 3.0f;
		return m_fTimeOutOfWater > s_fBeachedTime;
	}

	void CBoatAvoidanceHelper::ProcessBeached(CVehicle& in_Vehicle, float in_fElapsedTime)
	{
		// check that we have a valid propeller matrix
		const CVehicleModelInfo* pModelInfo = in_Vehicle.GetVehicleModelInfo();

		Vector3 vPosition = VEC3V_TO_VECTOR3(in_Vehicle.GetTransform().GetPosition());
		
		// get the water height at this position
		float waterZ;
		if(in_Vehicle.m_Buoyancy.GetWaterLevelIncludingRivers(vPosition, &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
		{
			for (int i = BOAT_STATIC_PROP; i < BOAT_MOVING_PROP2; i++ )
			{
				Matrix34 mMatProp;
				s32 propBoneId = pModelInfo->GetBoneIndex(i);
				if (propBoneId>-1)
				{
					in_Vehicle.GetGlobalMtx(propBoneId, mMatProp);

					// check if the propeller is under water
					if (waterZ>mMatProp.d.z)
					{
						m_fTimeOutOfWater = 0.0f;
						return;
					}
				}
			}
		}

		if (!in_Vehicle.GetIsAnyFixedFlagSet())
		{
			m_fTimeOutOfWater += in_fElapsedTime;	
		}
	}

	void CBoatAvoidanceHelper::Reset()
	{
		for ( int i = 0; i < m_NavmeshLOSClear.GetCount(); i++ )
		{
			m_NavmeshLOSHelpers[i].ResetTest();
			m_NavmeshLOSClear[i] = true;
		}
		m_fBoatAvoidanceOrientation = m_fBoatDesiredOrientation;
		m_ProbeQueryIndex = 0;
	}


	float CBoatAvoidanceHelper::ComputeAvoidanceSpeed(float in_Speed, float in_SlowDownScale) const
	{
		if (m_bEnabled && m_bEnableSlowingDown )
		{
			int count = 0;
			for(int i = 1; i < m_NavmeshLOSClear.GetCount(); i++)
			{
				if ( !m_NavmeshLOSClear[i] )
				{
					count++;
				}
			}			


			float scale = (count/static_cast<float>(m_NavmeshLOSClear.GetCount())) * in_SlowDownScale;
			return in_Speed - (in_Speed * scale);
		}
		return in_Speed;
	}

	float CBoatAvoidanceHelper::ComputeSliceAngle(const CVehicle& in_Vehicle) const
	{
		float fSpeed = in_Vehicle.GetVelocity().XYMag();	
		return Lerp(Clamp(fSpeed/s_BoatSliceSpeedToAngleRatio, 0.0f, 1.0f), s_BoatSliceAngleMax, s_BoatSliceAngleMin);
	}

	void CBoatAvoidanceHelper::Process(CVehicle& in_Vehicle, float in_fElapsedTime)
	{
		bool bDoDynamicEntityAvoidance = in_Vehicle.GetDriver() && in_Vehicle.GetDriver()->GetPedConfigFlag(ePedConfigFlags::CPED_CONFIG_FLAG_AIDriverAllowFriendlyPassengerSeatEntry);

		bool bClear = true;
		ProcessBeached(in_Vehicle, in_fElapsedTime);
		if ( m_bEnabled )
		{
			Vec3V vPosition = CVehicleFollowRouteHelper::GetVehicleBonnetPosition(&in_Vehicle, m_bReverse, true);		
			if ( m_bReverse )
			{
				Vec3V vBonnetDelta = vPosition - in_Vehicle.GetTransform().GetPosition();
				vPosition = in_Vehicle.GetTransform().GetPosition() - vBonnetDelta;
			}

			ScalarV fTime(s_BoatFutureTime);
			ScalarV fSpeed = ScalarV(in_Vehicle.GetVelocity().XYMag());	

			float fBoatAngleSlice = ComputeSliceAngle(in_Vehicle);
						
			if (CPathServer::IsNavMeshLoadedAtPosition(VEC3V_TO_VECTOR3(vPosition), kNavDomainRegular))
			{
				bClear = false;
				float fSteerRight = 0.0f;
				float fSteerLeft = 0.0f;
				bool bProcess = false;

				int iStartIndex = bDoDynamicEntityAvoidance ? 0 : m_ProbeQueryIndex;
				for (int i = iStartIndex; i < m_NavmeshLOSHelpers.GetCount(); i++ )
				{
					//Check if a LOS NavMesh test is active.
					//Always try to handle an active test, even if we aren't allowed to issue a new one this frame
					if(m_NavmeshLOSHelpers[i].IsTestActive())
					{
						//Get the results.
						SearchStatus searchResult;
						bool bLosIsClear;
						bool bNoSurfaceAtCoords;
						if(m_NavmeshLOSHelpers[i].GetTestResults(searchResult, bLosIsClear, bNoSurfaceAtCoords, 1, NULL))
						{
							if ( searchResult != SS_StillSearching )
							{
								bProcess = true;
								if (bLosIsClear || bNoSurfaceAtCoords)
								{
									m_NavmeshLOSClear[i] = true;
								}
								else
								{
									m_NavmeshLOSClear[i] = false;
								}						 
							}
						}
					}
					else 
					{
						float fOffset = (int)(( i + 1.5f )/2.0f) * s_BoatOffset;
						float fAngle = (int)(( i + 1.5f )/2.0f) * fBoatAngleSlice * DtoR;
						fAngle = (i & 0x01) > 0 ? -fAngle : fAngle;
						fOffset = (i & 0x01) > 0 ? fOffset : -fOffset;
						ScalarV fLength = Min(fSpeed * fTime * ScalarV(m_AvoidanceFeelerScale), ScalarV(m_AvoidanceFeelerMaxLength));
						Vec3V vTargetDir = RotateAboutZAxis(Vec3V(1.0f, 0.0f, 0.0f), ScalarV(m_fBoatAvoidanceOrientation + fAngle));
						Vec3V vTargetDirRight = Vec3V(vTargetDir.GetYf(), -vTargetDir.GetXf(), 0.0f);
						Vec3V vStartPosition = vPosition + vTargetDirRight * ScalarV(fOffset);
						Vec3V vFuturePosition = vStartPosition + vTargetDir * fLength;

						// get the water height at this position
						float waterZ;
						if(in_Vehicle.m_Buoyancy.GetWaterLevelIncludingRivers(VEC3V_TO_VECTOR3(vStartPosition), &waterZ, false, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
						{
							// query from water level
							static float s_OffsetUp = 1.0f;
							vStartPosition.SetZf(waterZ + s_OffsetUp);
							vFuturePosition.SetZf(waterZ + s_OffsetUp);

							if ( in_Vehicle.GetDriver() )
							{
								if ( in_Vehicle.GetDriver()->GetNavMeshTracker().GetIsValid() )
								{
									Vector3 vLastPosition = in_Vehicle.GetDriver()->GetNavMeshTracker().GetLastNavMeshIntersection();
									vStartPosition.SetZf(vLastPosition.z + s_OffsetUp);
									vFuturePosition.SetZf(vLastPosition.z + s_OffsetUp);
								}	
							}

							if(bDoDynamicEntityAvoidance && !m_bReverse)
							{
								CEntity* pExcludeEntity = &in_Vehicle;
								const float fBoundRadiusMultiplierToUse = in_Vehicle.InheritsFromBike() ? CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplierBike : CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplier;
								m_NavmeshLOSHelpers[i].StartLosTest(VEC3V_TO_VECTOR3(vStartPosition), VEC3V_TO_VECTOR3(vFuturePosition), in_Vehicle.GetBoundRadius() * fBoundRadiusMultiplierToUse
									, true, true, true, 1, &pExcludeEntity, CTaskVehicleGoToNavmesh::ms_fMaxNavigableAngleRadians);		
							}
							else
							{
								const float fBoundRadiusMultiplierToUse = in_Vehicle.InheritsFromBike() ? CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplierBike : CTaskVehicleGoToNavmesh::ms_fBoundRadiusMultiplier;
								m_NavmeshLOSHelpers[i].StartLosTest(VEC3V_TO_VECTOR3(vStartPosition), VEC3V_TO_VECTOR3(vFuturePosition), in_Vehicle.GetBoundRadius() * fBoundRadiusMultiplierToUse
									, false, true, true, 0, NULL, CTaskVehicleGoToNavmesh::ms_fMaxNavigableAngleRadians);		

								// only allow one query per frame
								m_ProbeQueryIndex = (m_ProbeQueryIndex + 1) % m_NavmeshLOSHelpers.GetCount();
								break;
							}
						}
					}
				}

				// check if we are completely clear or completely blocked
				bool bBlocked = true;
				bool bAvoid = false;
				for(int i = 1; i < m_NavmeshLOSClear.GetCount(); i++)
				{
					if ( m_NavmeshLOSClear[i] )
					{
						bBlocked = false;
					}
					else
					{
						bAvoid = true;

						float fAngle = (int)(( (m_NavmeshLOSClear.GetCount() - i ) + 1.5f )/2.0f) * fBoatAngleSlice * DtoR;

						if ( ( i & 0x01) > 0 )
						{
							fSteerRight = Max(fSteerRight, fAngle);		
						}
						else
						{
							fSteerLeft = Max(fSteerLeft, fAngle);												
						}
					}
				}

				// avoiding and los probes were updated
				if ( m_bEnableSteering && bAvoid && ( bProcess|| bBlocked) )
				{
					if ( fSteerLeft > fSteerRight )
					{
						m_fBoatAvoidanceOrientation -= fSteerLeft;
						m_fBoatPreviousAvoidanceDelta = -fSteerLeft;
					}
					else if ( fSteerRight > fSteerLeft )
					{
						m_fBoatAvoidanceOrientation += fSteerRight;					
						m_fBoatPreviousAvoidanceDelta = fSteerRight;
					}
					else if ( bBlocked )
					{
						m_fBoatAvoidanceOrientation += m_fBoatPreviousAvoidanceDelta;
					}
					else
					{
						// just steer back to desired
						bAvoid = false;
					}
				}
			
				// nothing to avoid then rotate back
				// to desired direction
				if ( !m_bEnableSteering || !bAvoid )
				{
					static float s_BoatSlideBackDegreesPerSecond = 180.0f;
					float fBoatSlideAngleBack = s_BoatSlideBackDegreesPerSecond;
					float fDeltaAngle = SubtractAngleShorter(m_fBoatDesiredOrientation, m_fBoatAvoidanceOrientation);
					float fAngle = Sign(fDeltaAngle) * Min(fabsf(fDeltaAngle), fBoatSlideAngleBack * DtoR * in_fElapsedTime);
					m_fBoatAvoidanceOrientation += fAngle;
					m_fBoatPreviousAvoidanceDelta = 0.0f;
				}
				
				//the Clamp check below assumes avoidance and desired orientations are the same sign
				//so we rotate m_fBoatAvoidanceOrientation by minus/two pi and then limit after clamping
				if(fabsf(m_fBoatAvoidanceOrientation - m_fBoatDesiredOrientation) > PI)
				{
					m_fBoatAvoidanceOrientation += m_fBoatAvoidanceOrientation > 0.0f ? -TWO_PI : TWO_PI;
				}

				float fMaxBoatMaxAvoidanceOrientationLeft = m_fBoatDesiredOrientation - (Max(m_fBoatMaxAvoidanceOrientationLeft, m_fBoatMaxAvoidanceOrientationLeftThisFrame) * DtoR);
				float fMaxBoatMaxAvoidanceOrientationRight = m_fBoatDesiredOrientation - (Min(m_fBoatMaxAvoidanceOrientationRight, m_fBoatMaxAvoidanceOrientationRightThisFrame) * DtoR);

				m_fBoatAvoidanceOrientation = Clamp(m_fBoatAvoidanceOrientation, Min(fMaxBoatMaxAvoidanceOrientationLeft, fMaxBoatMaxAvoidanceOrientationRight), Max(fMaxBoatMaxAvoidanceOrientationLeft, fMaxBoatMaxAvoidanceOrientationRight));
				m_fBoatAvoidanceOrientation = fwAngle::LimitRadianAngleSafe(m_fBoatAvoidanceOrientation);

				Assert(FPIsFinite(m_fBoatAvoidanceOrientation));
			}
		}

		if ( bClear )
		{
			Reset();
		}

		m_fBoatMaxAvoidanceOrientationLeftThisFrame = -ms_DefaultMaxAvoidanceOrientation;
		m_fBoatMaxAvoidanceOrientationRightThisFrame = ms_DefaultMaxAvoidanceOrientation;

	}

#if !__FINAL
	void CBoatAvoidanceHelper::Debug(const CVehicle& DEBUG_DRAW_ONLY(in_Vehicle)) const
	{
#if DEBUG_DRAW

		Vec3V vPosition = CVehicleFollowRouteHelper::GetVehicleBonnetPosition(&in_Vehicle, false, true);		
		if ( m_bReverse )
		{
			Vec3V vBonnetDelta = vPosition - in_Vehicle.GetTransform().GetPosition();
			vPosition = in_Vehicle.GetTransform().GetPosition() - vBonnetDelta;
		}

		if ( m_bEnabled )
		{
			ScalarV fTime(s_BoatFutureTime);
			ScalarV fSpeed = ScalarV(in_Vehicle.GetVelocity().XYMag());	

			float fBoatAngleSlice = ComputeSliceAngle(in_Vehicle);
			for(int i = 0; i < m_NavmeshLOSClear.GetCount(); i++)
			{
				float fOffset = (int)(( i + 1.5f )/2.0f) * s_BoatOffset;
				float fAngle = (int)(( i + 1.5f )/2.0f) * fBoatAngleSlice * DtoR;
				fAngle = (i & 0x01) > 0 ? -fAngle : fAngle;
				fOffset = (i & 0x01) > 0 ? fOffset : -fOffset;
				ScalarV fLength = Min(fSpeed * fTime * ScalarV(m_AvoidanceFeelerScale), ScalarV(m_AvoidanceFeelerMaxLength));
				Vec3V vTargetDir = RotateAboutZAxis(Vec3V(1.0f, 0.0f, 0.0f), ScalarV(m_fBoatAvoidanceOrientation + fAngle));
				Vec3V vTargetDirRight = Vec3V(vTargetDir.GetYf(), -vTargetDir.GetXf(), 0.0f);
				Vec3V vStartPosition = vPosition + vTargetDirRight * ScalarV(fOffset);
				Vec3V vFuturePosition = vStartPosition + vTargetDir * fLength;


				if ( m_NavmeshLOSClear[i] )
				{
					grcDebugDraw::Line(VEC3V_TO_VECTOR3(vStartPosition) + ZAXIS, VEC3V_TO_VECTOR3(vFuturePosition) + ZAXIS, Color_green, Color_green);
				}
				else
				{
					grcDebugDraw::Line(VEC3V_TO_VECTOR3(vStartPosition) + ZAXIS, VEC3V_TO_VECTOR3(vFuturePosition) + ZAXIS, Color_red, Color_red);
				}
			}
		}
		char text[128];
		sprintf(text, "Beached: %s", IsBeached() ? "True" : "False" );
		grcDebugDraw::Text(VEC3V_TO_VECTOR3(vPosition), Color_black, 0, 20, text);


#endif
	}
#endif //!__FINAL

//////////////////////////////////////////////////////////////////////////
//
// CBoatFollowRouteHelper
//
//////////////////////////////////////////////////////////////////////////


CBoatFollowRouteHelper::CBoatRoutePoint::CBoatRoutePoint()
	: m_Position(Vec2V(0.0f, 0.0f))
	, m_Width(-1.0f)
{
}

CBoatFollowRouteHelper::CBoatRoutePoint::CBoatRoutePoint(const CBoatRoutePoint& in_rhs)
	: m_Position(in_rhs.m_Position)
	, m_Width(in_rhs.m_Width)
{
}

CBoatFollowRouteHelper::CBoatRoutePoint::CBoatRoutePoint(Vec2V_In in_Position, ScalarV_In in_Width)
	: m_Position(in_Position)
	, m_Width(in_Width)
{
}

CBoatFollowRouteHelper::CBoatRoutePoint& CBoatFollowRouteHelper::CBoatRoutePoint::operator=(const CBoatRoutePoint& in_rhs)
{
	m_Position = in_rhs.m_Position;
	m_Width = in_rhs.m_Width;
	return *this;
}

//
// follow route helper
//
CBoatFollowRouteHelper::CBoatFollowRouteHelper()
	: m_SegmentTValue(0.0f)
{
	m_RoutePoints.Reserve(CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED + 2);
}

CBoatFollowRouteHelper::~CBoatFollowRouteHelper()
{

}

void CBoatFollowRouteHelper::ConstructFromStartEnd(CVehicle& in_Vehicle, const Vector3& in_Target)
{
	m_RoutePoints.clear();
	m_RoutePoints.Append() = CBoatFollowRouteHelper::CBoatRoutePoint(Vec2V(in_Target.x, in_Target.y), ScalarV(-1.0f));
	m_RoutePoints.Append() = CBoatFollowRouteHelper::CBoatRoutePoint( Vec2V(in_Vehicle.GetTransform().GetPosition().GetXf(), in_Vehicle.GetTransform().GetPosition().GetYf()), ScalarV(-1.0f));
	m_SegmentTValue = ScalarV(0.0f);
}

void CBoatFollowRouteHelper::ConstructFromNodeList(CVehicle& in_Vehicle, const CVehicleNodeList& in_NodeList, const Vector3& in_Target)
{
	const int iNumPathNodesInList = in_NodeList.FindNumPathNodes();
	m_RoutePoints.clear();
	m_RoutePoints.Append().m_Position = Vec2V(in_Target.x, in_Target.y);

	if ( iNumPathNodesInList > 0 )
	{
		const CPathNode *pPreviousNode = NULL;
		for( int i = iNumPathNodesInList-1; i >= 0; i-- )
		{
			CNodeAddress CurrentNodeAddress = in_NodeList.GetPathNodeAddr(i);
			const CPathNode *pCurrentNode = ThePaths.FindNodePointerSafe(CurrentNodeAddress);
			if ( pCurrentNode )
			{
				Vector3 vCoords;
				pCurrentNode->GetCoors(vCoords);
				m_RoutePoints.Append() = CBoatFollowRouteHelper::CBoatRoutePoint(Vec2V(vCoords.x, vCoords.y), ScalarV(1.0f));
				if ( pPreviousNode )
				{
					const CPathNodeLink* pLink = ThePaths.FindLinkBetween2Nodes(pPreviousNode, pCurrentNode);
					m_RoutePoints.back().m_Width = ScalarV(pLink->GetLaneWidth());
				}
				pPreviousNode = pCurrentNode;		
			}
		}
	}
	else
	{
		m_RoutePoints.Append().m_Position = Vec2V(in_Vehicle.GetTransform().GetPosition().GetXf(), in_Vehicle.GetTransform().GetPosition().GetYf());	
	}

	
	m_SegmentTValue = ScalarV(0.0f);
}

void CBoatFollowRouteHelper::OverrideTarget(const Vector3& in_Target)
{
	if ( m_RoutePoints.GetCount() >= 2 )
	{
		m_RoutePoints[m_RoutePoints.GetCount()-2].m_Position = Vec2V(in_Target.x, in_Target.y);
	}
}

void CBoatFollowRouteHelper::OverrideDestination(const Vector3& in_Target)
{
	if ( m_RoutePoints.GetCount() > 0 )
	{
		m_RoutePoints[0].m_Position = Vec2V(in_Target.x, in_Target.y);
	}
}

const Vector3& CBoatFollowRouteHelper::ComputeTarget(sVehicleMissionParams& params, const CVehicle& in_Vehicle) const
{
	Vector3 vTargetPos = params.GetTargetPosition();

	params.SetTargetPosition(ComputeTarget(vTargetPos, in_Vehicle));

	return params.GetTargetPosition();
}

const Vector3& CBoatFollowRouteHelper::ComputeTarget(Vector3& o_Vector, const CVehicle& in_Vehicle) const
{
	if ( m_RoutePoints.GetCount() >= 2 )
	{
		o_Vector = Vector3(m_RoutePoints[m_RoutePoints.GetCount()-2].m_Position.GetXf(), m_RoutePoints[m_RoutePoints.GetCount()-2].m_Position.GetYf(), in_Vehicle.GetTransform().GetPosition().GetZf());
	}
	return o_Vector;
}

const Vector3& CBoatFollowRouteHelper::ComputeSegmentTarget(Vector3& o_Vector, const CVehicle& in_Vehicle ) const
{
	if ( m_RoutePoints.GetCount() >= 2 )
	{
		Vec2V vPrevious = m_RoutePoints[m_RoutePoints.GetCount()-1].m_Position;
		Vec2V vCurrent = m_RoutePoints[m_RoutePoints.GetCount()-2].m_Position;
		Vec2V vPreviousToVCurrent = vCurrent - vPrevious;
		Vec2V vTarget = vPrevious + vPreviousToVCurrent * m_SegmentTValue;
		o_Vector = Vector3(vTarget.GetXf(), vTarget.GetYf(), in_Vehicle.GetTransform().GetPosition().GetZf());
	}
	return o_Vector;
}

Vec2V_Out CBoatFollowRouteHelper::ComputeSegmentDirection(Vec2V_Ref o_Direction) const
{
	if ( m_RoutePoints.GetCount() >= 2 )
	{
		Vec2V vPrevious = m_RoutePoints[m_RoutePoints.GetCount()-1].m_Position;
		Vec2V vCurrent = m_RoutePoints[m_RoutePoints.GetCount()-2].m_Position;
		o_Direction = Normalize(vCurrent - vPrevious);
	}
	return o_Direction;
}

const float CBoatFollowRouteHelper::ComputeSegmentWidth() const
{
	if ( m_RoutePoints.GetCount() >= 2 )
	{
		return m_RoutePoints[m_RoutePoints.GetCount()-2].m_Width.Getf();
	}
	return -1.0f;
}

bool CBoatFollowRouteHelper::Progress(CVehicle& in_Vehicle, float in_fDistance)
{
	static float s_FutureTime = 0.1f;
	bool bRetValue = false;
		
	ScalarV vDistance2 = ScalarV(square(in_fDistance));
	const Vec2V vVehiclePosV = CVehicleFollowRouteHelper::GetVehicleBonnetPosition(&in_Vehicle, false).GetXY();
	const Vec2V vVehicleVelV = Vec2V( in_Vehicle.GetVelocity().x, in_Vehicle.GetVelocity().y);
	const Vec2V vVehicleFuturePosV = vVehiclePosV + vVehicleVelV * ScalarV(s_FutureTime);
	
	while ( m_RoutePoints.GetCount() >= 2 )
	{
		Vec2V vCurrent = m_RoutePoints[m_RoutePoints.GetCount()-2].m_Position;
		Vec2V vPrevious = m_RoutePoints[m_RoutePoints.GetCount()-1].m_Position;
		
		Vec2V vPreviousToVVehicleFuture = vVehicleFuturePosV - vPrevious;
		Vec2V vPreviousToVCurrent = vCurrent - vPrevious;

		ScalarV vPreviousToCurrentMag2 = MagSquared(vPreviousToVCurrent);
		ScalarV vTValue = Dot(vPreviousToVCurrent, vPreviousToVVehicleFuture) / vPreviousToCurrentMag2;

		if ( (vTValue < ScalarV(1.0f)).Getb() )
		{
			Vec2V vClosestPointOnSegment = vPrevious + vPreviousToVCurrent * vTValue;

			ScalarV vDistanceToClosest2 = DistSquared(vVehicleFuturePosV, vClosestPointOnSegment);
			if ( (vDistanceToClosest2 < vDistance2).Getb() )
			{
				// use a^2 = b^2 + c^2
				// then compute target
				// then compute T value from new target
				ScalarV vOpposite = ScalarV(sqrt( (vDistance2 + vDistanceToClosest2).Getf() ));		
				Vec2V vSegmentTarget = vClosestPointOnSegment + Normalize(vPreviousToVCurrent) * vOpposite;
				vTValue = Mag(vSegmentTarget - vPrevious) / ScalarV(sqrt(vPreviousToCurrentMag2.Getf()));
			}
			
			if ( (vTValue < ScalarV(1.0f)).Getb() 
				||  m_RoutePoints.GetCount() <= 2 )
			{
				m_SegmentTValue = Clamp(vTValue, m_SegmentTValue, ScalarV(1.0f));
				break;
			}
		}

		m_SegmentTValue = ScalarV(0.0f);
		m_RoutePoints.Pop();
		bRetValue = true;
	}
	return bRetValue;
}

bool CBoatFollowRouteHelper::IsLastSegment() const
{
	return m_RoutePoints.GetCount() <= 2;
}

bool CBoatFollowRouteHelper::IsFinished() const
{
	return m_RoutePoints.GetCount() < 2;
}

#if !__FINAL
void CBoatFollowRouteHelper::Debug(const CVehicle& DEBUG_DRAW_ONLY(in_Vehicle)) const
{
#if DEBUG_DRAW
	for(int i = 0; i < m_RoutePoints.GetCount(); i++ )
	{
		float red = (i + 1 ) / static_cast<float>(m_RoutePoints.GetCount());
		float green = (m_RoutePoints.GetCount() - i ) / static_cast<float>(m_RoutePoints.GetCount());
		Vector3 vVector = Vector3(m_RoutePoints[i].m_Position.GetXf(), m_RoutePoints[i].m_Position.GetYf(), in_Vehicle.GetTransform().GetPosition().GetZf() + 1.0f );
		grcDebugDraw::Circle(vVector, m_RoutePoints[i].m_Width.Getf(), ::rage::Color32(red,green,0.00f), XAXIS, YAXIS );

		if ( i > 0 )
		{
			Vector3 vNext = Vector3(m_RoutePoints[i-1].m_Position.GetXf(), m_RoutePoints[i-1].m_Position.GetYf(), in_Vehicle.GetTransform().GetPosition().GetZf() + 1.0f );
			Vector3 vToNext = vNext - vVector;
			vToNext.Normalize();
			Vector3 vToNextRight = Vector3(vToNext.y, -vToNext.x, 0.0f);
			Vector3 vOffset = vToNextRight * Max(m_RoutePoints[i-1].m_Width.Getf(), 0.1f);

			grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vVector), VECTOR3_TO_VEC3V(vNext), 1.0f, Color_red2);
			grcDebugDraw::Quad(VECTOR3_TO_VEC3V(vVector - vOffset), VECTOR3_TO_VEC3V(vNext - vOffset), VECTOR3_TO_VEC3V(vNext + vOffset), VECTOR3_TO_VEC3V(vVector + vOffset), Color_blue,true,true);
			grcDebugDraw::Line(VECTOR3_TO_VEC3V(vVector + vOffset), VECTOR3_TO_VEC3V(vNext + vOffset), Color_red2);
			grcDebugDraw::Line(VECTOR3_TO_VEC3V(vVector - vOffset), VECTOR3_TO_VEC3V(vNext - vOffset), Color_red2);

		}

	}

#endif
}
#endif //!__FINAL


//-------------------------------------------------------------------------
// Helper used to return a vehicle path node search
//-------------------------------------------------------------------------

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CPathNodeRouteSearchHelper, CONFIGURED_FROM_FILE, 0.35f, atHashString("CPathNodeRouteSearchHelper",0x8e771f83));

const float CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForRoadStraightAhead = 0.62f;
const float CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForUsingJunctionNode = 0.75f;

CPathNodeRouteSearchHelper::CPathNodeRouteSearchHelper()
: m_vSearchStart(VEC3_ZERO)
, m_vTargetPosition(VEC3_ZERO)
, m_vStartOrientation(VEC3_ZERO)
, m_vStartVelocity(VEC3_ZERO)
, m_fStartNodeRejectDistMultiplier(2.0f)
, m_iSearchFlags(0)
, m_iMaxSearchDistance(999999)
, m_bActuallyFoundRoute(false)
{
	ResetStartNodeRejectDist();
}

CPathNodeRouteSearchHelper::~CPathNodeRouteSearchHelper()
{
}

void CPathNodeRouteSearchHelper::Reset()
{
	m_route.Clear();
	SetState(State_Start);
	m_vSearchStart		= VEC3_ZERO;
	m_vTargetPosition	= VEC3_ZERO;
	m_vStartOrientation = VEC3_ZERO;
	m_vStartVelocity	= VEC3_ZERO;
	m_iSearchFlags		= 0;
	m_iTargetNodeIndex  = 0;
	m_iCurrentNumberOfNodes = 0;
	m_PreferredTargetRoute.Clear();
	m_iMaxSearchDistance = 999999;
	m_bActuallyFoundRoute = false;
}

// State Machine Update Functions
CTaskHelperFSM::FSM_Return		CPathNodeRouteSearchHelper::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Start)
			FSM_OnUpdate
				return Start_OnUpdate();
	
		FSM_State(State_FindStartNodesInDirection)
			FSM_OnUpdate
				return FindStartNodesInDirection_OnUpdate();

		FSM_State(State_FindStartNodesAnyDirection)
			FSM_OnUpdate
				return FindStartNodesAnyDirection_OnUpdate();

		FSM_State(State_GenerateWanderRoute)
			FSM_OnUpdate
				return GenerateWanderRoute_OnUpdate();

		FSM_State(State_Searching)
			FSM_OnUpdate
				return Searching_OnUpdate();

		FSM_State(State_NoRoute)
			FSM_OnUpdate
				return FSM_Quit;

		FSM_State(State_ValidRoute)
			FSM_OnUpdate
				m_iCurrentNumberOfNodes = m_route.FindNumPathNodes();
				return FSM_Quit;

	FSM_End
}

bool CPathNodeRouteSearchHelper::VerifyNodesAreLoadedForArea() const
{
	float fSearchMinX;
	float fSearchMinY;
	float fSearchMaxX;
	float fSearchMaxY;

	const bool bWanderRoute = (m_iSearchFlags & Flag_WanderRoute) != 0;

	if (bWanderRoute)
	{
		const float fWanderVerificationRadius = 20.0f;

		fSearchMinX = m_vSearchStart.x - fWanderVerificationRadius;
		fSearchMinY = m_vSearchStart.y - fWanderVerificationRadius;
		fSearchMaxX = m_vSearchStart.x + fWanderVerificationRadius;
		fSearchMaxY = m_vSearchStart.y + fWanderVerificationRadius;
	}
	else
	{
		fSearchMinX = rage::Min(m_vSearchStart.x, m_vTargetPosition.x);
		fSearchMinY = rage::Min(m_vSearchStart.y, m_vTargetPosition.y);
		fSearchMaxX = rage::Max(m_vSearchStart.x, m_vTargetPosition.x);
		fSearchMaxY = rage::Max(m_vSearchStart.y, m_vTargetPosition.y);
	}

	const bool bLoaded = ThePaths.AreNodesLoadedForArea(fSearchMinX, fSearchMaxX, fSearchMinY, fSearchMaxY);

#if !__FINAL
	const bool bCanWaitForNodesToBeLoaded = (m_iSearchFlags & Flag_AllowWaitForNodesToLoad) != 0;

	if (!bLoaded && !bWanderRoute && !bCanWaitForNodesToBeLoaded)
	{
		bool bVehicleFound = false;

#if __BANK
		//find the vehicle that made us do this
		CVehicle* pBadVehicle = NULL;
		float fDistSqrToClosestBadVehicle = FLT_MAX;

		CVehicle::Pool *VehiclePool = CVehicle::GetPool();
		CVehicle* pVehicle = NULL;
		s32 i = static_cast<s32>(VehiclePool->GetSize());

		while (i--)
		{
			pVehicle = VehiclePool->GetSlot(i);

			if (!pVehicle)
			{
				continue;
			}

			const float fDistSqr = m_vSearchStart.Dist2(VEC3V_TO_VECTOR3(CVehicleFollowRouteHelper::GetVehicleBonnetPosition(pVehicle, false)));

			if (fDistSqr < 1.0f && fDistSqr < fDistSqrToClosestBadVehicle)
			{
				pBadVehicle = pVehicle;
				fDistSqrToClosestBadVehicle = fDistSqr;
			}
		}

		if (pBadVehicle)
		{
			bVehicleFound = true;

			pBadVehicle->GetIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);

			CPed* pDriver = pBadVehicle->GetDriver();

			if (pDriver)
			{
				pDriver->GetPedIntelligence()->GetTaskManager()->SpewAllTasks(aiTaskSpew::SPEW_STD);
			}

#if __ASSERT
			aiAssertf(0, "%s Vehicle %s %s at (%.2f, %.2f, %.2f) (created by %s) trying to pathfind to destination (%.2f, %.2f, %.2f) without nodes loaded.  Call REQUEST_PATH_NODES_IN_AREA_THIS_FRAME on an area encompassing the start and end of the route.", 
							pBadVehicle->PopTypeIsMission() ? "Mission" : "Non-mission", 
							pBadVehicle->GetVehicleModelInfo()->GetModelName(), pBadVehicle->GetDebugNameFromObjectID(), m_vSearchStart.x, m_vSearchStart.y, m_vSearchStart.z, 
							pBadVehicle->GetScriptThatCreatedMe() ? pBadVehicle->GetScriptThatCreatedMe()->GetScriptName() : "NULL", 
							m_vTargetPosition.x, m_vTargetPosition.y, m_vTargetPosition.z);
#else
			aiWarningf("%s Vehicle %s %s at (%.2f, %.2f, %.2f) (created by %s) trying to pathfind to destination (%.2f, %.2f, %.2f) without nodes loaded.  Call REQUEST_PATH_NODES_IN_AREA_THIS_FRAME on an area encompassing the start and end of the route.", 
							pBadVehicle->PopTypeIsMission() ? "Mission" : "Non-mission", 
							pBadVehicle->GetVehicleModelInfo()->GetModelName(), pBadVehicle->GetDebugNameFromObjectID(), m_vSearchStart.x, m_vSearchStart.y, m_vSearchStart.z, 
							pBadVehicle->GetScriptThatCreatedMe() ? pBadVehicle->GetScriptThatCreatedMe()->GetScriptName() : "NULL", 
							m_vTargetPosition.x, m_vTargetPosition.y, m_vTargetPosition.z);
#endif // __ASSERT
		}
#endif // __BANK

		if (!bVehicleFound)
		{
			aiErrorf("Vehicle at (%.2f, %.2f, %.2f) trying to pathfind to destination (%.2f, %.2f, %.2f) without nodes loaded.  Call REQUEST_PATH_NODES_IN_AREA_THIS_FRAME on an area encompassing the start and end of the route.", 
					m_vSearchStart.x, m_vSearchStart.y, m_vSearchStart.z, 
					m_vTargetPosition.x, m_vTargetPosition.y, m_vTargetPosition.z);
		}
	}
#endif // !__FINAL

	return bLoaded;
}

bool CPathNodeRouteSearchHelper::StartSearch( const Vector3& vStartPosition, const Vector3& vTargetPosition, const Vector3& vStartOrientation, const Vector3& vStartVelocity, u32 iSearchFlags, const CVehicleNodeList* pPreferredTargetRoute)
{
	Reset();

	m_vSearchStart		= vStartPosition;
	m_vTargetPosition	= vTargetPosition;
	m_vStartOrientation = vStartOrientation;
	m_vStartVelocity	= vStartVelocity;
	m_iSearchFlags		= iSearchFlags;
	m_iTargetNodeIndex	= 1;

	if (pPreferredTargetRoute)
	{
		sysMemCpy(&m_PreferredTargetRoute, pPreferredTargetRoute, sizeof(CVehicleNodeList));
	}
	else
	{
		m_PreferredTargetRoute.Clear();
	}

	VerifyNodesAreLoadedForArea();

	return true;
}

bool CPathNodeRouteSearchHelper::StartExtendCurrentRouteSearch( const Vector3& vStartPosition, const Vector3& vTargetPosition, const Vector3& vStartOrientation, const Vector3& vStartVelocity, s32 iTargetNodeIndex, u32 iSearchFlags, const CVehicleNodeList* pPreferredTargetRoute)
{
	SetState(State_Start);

	m_vSearchStart		= vStartPosition;
	m_vTargetPosition	= vTargetPosition;
	m_vStartOrientation = vStartOrientation;
	m_vStartVelocity	= vStartVelocity;
	m_iSearchFlags		= iSearchFlags|Flag_UseExistingNodes;
	m_iTargetNodeIndex	= iTargetNodeIndex;

	if (pPreferredTargetRoute)
	{
		sysMemCpy(&m_PreferredTargetRoute, pPreferredTargetRoute, sizeof(CVehicleNodeList));
	}
	else
	{
		m_PreferredTargetRoute.Clear();
	}

	VerifyNodesAreLoadedForArea();

	return true;
}

CTaskHelperFSM::FSM_Return CPathNodeRouteSearchHelper::FindStartNodesInDirection_OnUpdate()
{
	const bool bIgnoreDir = (m_iSearchFlags&Flag_JoinRoadInDirection) == 0;
	
	// If start nodes have been provided, don't join to road system (which finds new start nodes)
	if((m_iSearchFlags & Flag_StartNodesOverridden) || m_route.JoinToRoadSystem(m_vSearchStart, m_vStartOrientation, m_iSearchFlags&Flag_IncludeWaterNodes, 0, 1, m_fStartNodeRejectDist, bIgnoreDir) )
	{
		//we found a start node, reset the size of our search
		ResetStartNodeRejectDist();

		Assert(m_route.FindNumPathNodes() == 2);

		//if we're fleeing, try and swap around our start nodes so that we're going away from the target
		const bool bAvoidTurns = (m_iSearchFlags & Flag_AvoidTurns) != 0;
		if (!bAvoidTurns && (m_iSearchFlags & Flag_FindRouteAwayFromTarget))
		{
			CTaskVehicleFlee::SwapFirstTwoNodesIfPossible(&m_route, m_vSearchStart, m_vStartVelocity, m_vTargetPosition, (m_iSearchFlags & Flag_IsMissionVehicle) != 0);
		}

		Assert(m_route.FindNumPathNodes() == 2);

		m_iCurrentNumberOfNodes = 2;
		if( m_iSearchFlags & Flag_WanderRoute )
		{
			SetState(State_GenerateWanderRoute);
		}
		else
		{
			SetState(State_Searching);
		}
	}
	else
	{
		IncreaseStartNodeRejectDist();
		SetState(State_NoRoute);
	}
	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CPathNodeRouteSearchHelper::FindStartNodesAnyDirection_OnUpdate()
{
	if (CVehicleIntelligence::m_bEnableNewJoinToRoadInAnyDir)
	{
		return FindStartNodesInDirection_OnUpdate();
	}
	else
	{
		if( m_iSearchFlags & Flag_WanderRoute )
		{
			SetState(State_GenerateWanderRoute);
		}
		else
		{
			SetState(State_Searching);
		}
	}
	
	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CPathNodeRouteSearchHelper::Start_OnUpdate()
{
	bool bExtendedRoute = false;
	if( m_iSearchFlags & Flag_UseExistingNodes )
	{
		const s32 iRouteHistorytoMaintain = (m_iSearchFlags & Flag_CabAndTrailerExtendRoute) ? HISTORY_TO_MAINTAIN_WHEN_EXTENDING_CAB_TRAILER_ROUTE : HISTORY_TO_MAINTAIN_WHEN_EXTENDING_ROUTE;
		const s32 iNodesToShuffle = rage::Max((s32)m_iTargetNodeIndex-iRouteHistorytoMaintain, (s32)0);
		// Remove all but the target point and previous node
		for( s32 i = 0; i < iNodesToShuffle; i++ )
		{
			//we always want to leave 2 nodes, so break out before we go too far
			if (m_route.GetPathNodeAddr(2).IsEmpty())
			{
				break;
			}

			m_route.ShiftNodesByOne();
			--m_iCurrentNumberOfNodes;
			--m_iTargetNodeIndex;

			//we did something, so prevent us from joining in road
			//direction or restarting the search
			bExtendedRoute = true;
		}

		if (!bExtendedRoute && !(m_iSearchFlags & Flag_OnStraightLineUpdate))
		{
			const int iLastGoodNode = m_route.FindLastGoodNode();
			if (iLastGoodNode >= m_iTargetNodeIndex && m_iTargetNodeIndex > 0 && !m_route.GetPathNodeAddr(0).IsEmpty())
			{
				bExtendedRoute = true;
			}
		}
		m_route.SetTargetNodeIndex(m_iTargetNodeIndex);
		if( m_iSearchFlags & Flag_WanderRoute )
		{
			SetState(State_GenerateWanderRoute);
		}
		else
		{
			SetState(State_Searching);
		}
	}
	
	if(!bExtendedRoute && (m_iSearchFlags & Flag_JoinRoadInDirection) )
	{
		if(!(m_iSearchFlags & Flag_StartNodesOverridden))
		{
			m_route.Clear();
		}

		SetState(State_FindStartNodesInDirection);
		m_route.SetTargetNodeIndex(1);
		m_iTargetNodeIndex = 1;
	}
	else if (!bExtendedRoute)
	{
		if(!(CVehicleIntelligence::m_bEnableNewJoinToRoadInAnyDir && m_iSearchFlags & Flag_StartNodesOverridden))
		{
			m_route.Clear();
		}

		SetState(State_FindStartNodesAnyDirection);
		m_route.SetTargetNodeIndex(1);
		m_iTargetNodeIndex = 1;
	}
	return FSM_Continue;
}

// bool HelperFoundEmptyNodeBeforeGoodNode(CNodeAddress* pNodeAddrs)
// {
// 	bool bLookingForEmpty = true;
// 	for (int i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
// 	{
// 		if (bLookingForEmpty && pNodeAddrs[i].IsEmpty())
// 		{
// 			bLookingForEmpty = false;
// 		}
// 
// 		if (!bLookingForEmpty && !pNodeAddrs[i].IsEmpty())
// 		{
// 			return true;
// 		}
// 	}
// 
// 	return false;
// }
///////////////

CTaskHelperFSM::FSM_Return CPathNodeRouteSearchHelper::Searching_OnUpdate()
{
	m_bActuallyFoundRoute = false;

	//---------------------------------------------------------------
	// Copy all the nodes into a temporary array for the path search
	s32 n;
	s32 NumNodesFound = 0;
	CNodeAddress pathFindNodes[CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED];
	for(n=0; n<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
	{
		pathFindNodes[n] = m_route.GetPathNodeAddr(n);
	}
	const bool bDontGoAgainstTraffic = (m_iSearchFlags & Flag_DriveIntoOncomingTraffic)==0;
	const bool bBoat = (m_iSearchFlags & Flag_IncludeWaterNodes)!=0;
	const bool bUseShortcuts = (m_iSearchFlags & Flag_UseShortcutLinks)!=0;
	const bool bUseSwitchedOffNodes = (m_iSearchFlags & Flag_UseSwitchedOffNodes)!=0;
	const bool bUseExistingNodes = (m_iSearchFlags & Flag_UseExistingNodes)!=0;
	const bool bJoinInRoadDirection = (m_iSearchFlags & Flag_JoinRoadInDirection)!=0;
	const bool bAvoidHighways = (m_iSearchFlags & Flag_AvoidHighways) != 0;
	const bool bCanWaitForNodesToBeLoaded = (m_iSearchFlags & Flag_AllowWaitForNodesToLoad) != 0;
	const bool bFollowTrafficRules = (m_iSearchFlags & Flag_ObeyTrafficRules) != 0;
	const bool bAvoidRestrictedAreas = (m_iSearchFlags & Flag_AvoidRestrictedAreas) != 0;
	const bool bAvoidOffroad = (m_iSearchFlags & Flag_WanderOffroad) == 0;

	int iFirstNodeIndexToFind = 0;
	if (bUseExistingNodes)
	{
		iFirstNodeIndexToFind = m_iTargetNodeIndex;
	}
	else if (bJoinInRoadDirection || CVehicleIntelligence::m_bEnableNewJoinToRoadInAnyDir)
	{
		iFirstNodeIndexToFind = 1;
	}

	const CNodeAddress AvoidNodeAddress = bJoinInRoadDirection && iFirstNodeIndexToFind >= 1 
		? pathFindNodes[iFirstNodeIndexToFind-1]
		: EmptyNodeAddress;

	//***********************
#if AI_OPTIMISATIONS_OFF
	CNodeAddress pathFindNodesPreSearch[CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED];
	for (int i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		pathFindNodesPreSearch[i] = pathFindNodes[i];
	}
	const u32 nOldTargetNode = m_iTargetNodeIndex;
	nOldTargetNode;
#endif //AI_OPTIMISATIONS_OFF
	//**********************

	//clear out future nodes we won't be needing
	for (int i = iFirstNodeIndexToFind+1; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		pathFindNodes[i].SetEmpty();
	}

	CNodeAddress StartNodeAddress = iFirstNodeIndexToFind == 0 ? EmptyNodeAddress : pathFindNodes[iFirstNodeIndexToFind];

	const CVehicleNodeList* pPreferredTargetRoute = m_PreferredTargetRoute.FindLastGoodNode() > 0 ? &m_PreferredTargetRoute : NULL;

	const bool bNodesAreLoadedForThisArea = VerifyNodesAreLoadedForArea();

	if (!bNodesAreLoadedForThisArea && bCanWaitForNodesToBeLoaded)
	{
		SetState(State_Searching);
		return FSM_Continue;
	}

	float fPathDist = 0.0f;

	ThePaths.DoPathSearch(
		m_vSearchStart,
		StartNodeAddress,
		m_vTargetPosition,
		&pathFindNodes[iFirstNodeIndexToFind],
		&NumNodesFound,
		CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-iFirstNodeIndexToFind,
		&fPathDist,
		999999.9f,
		NULL,
		m_iMaxSearchDistance,
		bDontGoAgainstTraffic,
		AvoidNodeAddress,
		false,
		bBoat,
		false,
		false,
		false, // for gps
		true, // clear dist to target
		bUseShortcuts,
		!bUseSwitchedOffNodes,
		bFollowTrafficRules, 
		pPreferredTargetRoute,
		bAvoidHighways,
		NULL,
		NULL,
		bAvoidRestrictedAreas,
		bAvoidOffroad
	);

	//HACK, though actually this one is quite reasonable
	//say our input is 2 nodes representing our join to road system
	//and the target node found by DoPathSearch is node 0
	//since DoPathSearch takes in node 1 as the input, it then finds
	//the route back to zero, appends it to the route giving us a path
	//of 0-1-0, when really the path should just consist of node 0
	//doing  this in the task helper instead of the pathfind class
	//since to work it kinda needs to rely on the particulars of the way
	//it's used by tasks--ie, you can imagine a situation where movement is
	//limited where it might be helpful to get 0-1-0 back, but in practice
	//we've got things like distance thresholds and the straight line
	//task to handle it
	if (NumNodesFound == 2 && iFirstNodeIndexToFind == 1
		&& pathFindNodes[0] == pathFindNodes[2])
	{
		pathFindNodes[1].SetEmpty();
		pathFindNodes[2].SetEmpty();
		NumNodesFound = 0;
	}

	//--------------------------------------------------------------
	// Copy the path search resultant nodes back into the pNodeList

	for(n=0; n<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
	{
		m_route.SetPathNodeAddr(n, pathFindNodes[n]);
	}

#if AI_OPTIMISATIONS_OFF
	bool bFoundInvalid = false;
	for(n=0; n<CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
	{
		if (pathFindNodes[n].IsEmpty())
		{
			bFoundInvalid = true;
		}

		if (!pathFindNodes[n].IsEmpty() && bFoundInvalid)
		{
			Assertf(0, "Found gap in returned path. Please hit abort and tell JMart about this, you might be about to crash.");
		}
	}
#endif //AI_OPTIMISATIONS_OFF

	//special case--if the destination node from our pathfinding query is a junction
	//don't search past it
	const int iLastGoodNodeFromSearch = m_route.FindLastGoodNode();

	// This check is just verifying that whatever link information we have stored
	// at the beginning of our new path is the same as what FindLinksWithNodes() would
	// have produced, in the cases where the nodes are resident (if they are not, FindLinksWithNodes()
	// would have set them to -1, which is what we are trying to avoid here).
#if __ASSERT
	for(int i = 0; i < iFirstNodeIndexToFind && i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED - 1; i++)
	{
		if (
			!m_route.GetPathNodeAddr(i).IsEmpty()
			&& !m_route.GetPathNodeAddr(i + 1).IsEmpty()
			&& ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(i))
			/*&& ThePaths.IsRegionLoaded(m_Entries[i+1].m_Address)*/
			)
		{
			int lnk = ThePaths.FindRegionLinkIndexBetween2Nodes(m_route.GetPathNodeAddr(i), m_route.GetPathNodeAddr(i + 1));
			Assertf(lnk == m_route.GetPathLinkIndex(i) || m_route.GetPathLinkIndex(i) < 0, "Link mismatch: (%d, %d) between nodes %d:%d, %d:%d"
				, lnk, m_route.GetPathLinkIndex(i), m_route.GetPathNodeAddr(i).GetRegion(), m_route.GetPathNodeAddr(i).GetIndex()
				, m_route.GetPathNodeAddr(i + 1).GetRegion(), m_route.GetPathNodeAddr(i + 1).GetIndex());
		}
	}
#endif	// __ASSERT

	// Update the links and lane addresses
	m_route.FindLinksWithNodes(0); //previously used iFirstNodeIndexToFind, but why not validate all nodes?
	m_route.FindLanesWithNodes(0, &m_vSearchStart);		// Note: possibly we should pass in iFirstNodeIndexToFind instead of 0 here?

	if (m_iSearchFlags & Flag_ExtendPathWithResultsFromWander && iLastGoodNodeFromSearch > 0 
		&& !(m_route.GetPathNode(iLastGoodNodeFromSearch) && m_route.GetPathNode(iLastGoodNodeFromSearch)->NumLinks() > 2))
	{
		for (int n = iLastGoodNodeFromSearch+1; n < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; n++)
		{
			//run GetNextNode until we've filled up our path.
			//this will allow a virtual surface to be created even if the path query fails
			//or it succeeds with only 1-2 nodes
			CNodeAddress FromNode = m_route.GetPathNodeAddr(n - 2);

			if (!aiVerifyf(!FromNode.IsEmpty() && ThePaths.IsRegionLoaded(FromNode), "FromNode empty or region not loaded: %d:%d", FromNode.GetRegion(), FromNode.GetIndex()))
			{
				break;
			}

			s16 iLinkTemp=0;
			bool	bHappyWithNode = false;
			s32 LanesOnCurrentLink = ThePaths.FindLinkPointerSafe(FromNode.GetRegion(),m_route.GetPathLinkIndex(n - 2))->m_1.m_LanesToOtherNode;
			bool bComingOutOfOneWayStreet =(ThePaths.FindLinkPointerSafe(FromNode.GetRegion(), m_route.GetPathLinkIndex(n - 2))->m_1.m_LanesFromOtherNode == 0);

			//don't extend the path if we're going against traffic
			if (LanesOnCurrentLink <= 0)
			{
				break;
			}

			GetNextNode(n, LanesOnCurrentLink, iLinkTemp, bHappyWithNode, bComingOutOfOneWayStreet, 0, false, false, false, false, XAXIS);

			if (m_route.GetPathNode(n-1) && !m_route.GetPathNodeAddr(n).IsEmpty())
			{
				m_route.SetPathLinkIndex(n-1, ThePaths.FindRegionLinkIndexBetween2Nodes(m_route.GetPathNodeAddr(n-1), m_route.GetPathNodeAddr(n)));
			}

			if (!bHappyWithNode)
			{
				break;
			}

			//don't go past junctions--we might choose the wrong direction from the thing we're following
			//and junctions use the longer constraints anyway, so we should be fine with straight line nav
			if (!m_route.GetPathNode(n) || m_route.GetPathNode(n)->NumLinks() > 2)
			{
				break;
			}
		}
	}

	// Update the lane addresses
	m_route.FindLanesWithNodes(iLastGoodNodeFromSearch, &m_vSearchStart);

	// Not enough nodes to the target, so assume the route has been found
	if( NumNodesFound < 2 )
	{
		m_bActuallyFoundRoute = (fPathDist <= SMALL_FLOAT);

		SetState(State_NoRoute);
		//m_route.Clear();
		//still copy over pathFindNodes and set up links/lanes
		//there's a chance it may be empty, which means it will be identical to a clear
		//but if there's valid data still in the path, like previous/new nodes, we want to 
		//hold onto those

		m_iCurrentNumberOfNodes = m_route.FindNumPathNodes();

		if (m_iCurrentNumberOfNodes < 2)
		{
			SetTargetNodeIndex(0);
		}
		else
		{
			SetTargetNodeIndex(rage::Max(m_route.FindLastGoodNode(), 1));
		}

		return FSM_Continue;
	}

	m_bActuallyFoundRoute = true;
	SetState(State_ValidRoute);
	return FSM_Continue;
}
bool CPathNodeRouteSearchHelper::IsThisAnAppropriateNode( CNodeAddress VeryOldNode, CNodeAddress OldNode, CNodeAddress CandidateNode, 
												bool bGoingAgainstTraffic, bool /*bGoingDownOneWayStreet*/, bool /*bComingOutOfOneWayStreet*/)
{
	if(!ThePaths.IsRegionLoaded(CandidateNode)) 
	{
		return false;
	}
	
	if(VeryOldNode == CandidateNode) 
	{
		return false;
	}

	aiAssert(ThePaths.IsRegionLoaded(OldNode)&&ThePaths.IsRegionLoaded(CandidateNode));
	const CPathNode *pOldNode = ThePaths.FindNodePointer(OldNode);
	const CPathNode *pCandidateNode = ThePaths.FindNodePointer(CandidateNode);

	// If we're going from water to land or vice versa, the node would only be
	// appropriate for a hovercraft.
	if(pOldNode->m_2.m_waterNode != pCandidateNode->m_2.m_waterNode)
	{
		return false;
	}

	Vector3 candidateCoors, oldCoors;

	pCandidateNode->GetCoors(candidateCoors);
	pOldNode->GetCoors(oldCoors);

	switch(pCandidateNode->m_1.m_specialFunction)
	{
	case SPECIAL_PARKING_PARALLEL:
	case SPECIAL_PARKING_PERPENDICULAR:
	case SPECIAL_DRIVE_THROUGH:
	case SPECIAL_DRIVE_THROUGH_WINDOW:
	case SPECIAL_DROPOFF_GOODS:
	case SPECIAL_DROPOFF_GOODS_UNLOAD:
		return false;
	default:
		break;
	}

	if(pCandidateNode->m_2.m_deadEndness > pOldNode->m_2.m_deadEndness) 
	{
		return false;
	}

	if(pCandidateNode->m_2.m_switchedOff && !pOldNode->m_2.m_switchedOff) 
	{
		return false;
	}

		// This is so that alleys that were originally switched off still are avoided by cars that are on a 'script switched off' road. Needed for Neils uber recordings.
	if(pCandidateNode->m_2.m_switchedOffOriginal && !pOldNode->m_2.m_switchedOffOriginal) 
	{
		return false;
	}

	//try and avoid wandering onto NoGPS nodes
	if (pCandidateNode->m_2.m_noGps && !pOldNode->m_2.m_noGps)
	{
		return false;
	}

	// If we're coming out of a one street and going into another that is more than a 90 degree angle turn we are probably
	// going back into the same street(that has been been split up)
// 	if(bGoingDownOneWayStreet && bComingOutOfOneWayStreet)
// 	{
// 		if((!VeryOldNode.IsEmpty()) && ThePaths.IsRegionLoaded(VeryOldNode))
// 		{
// 			const CPathNode *pVeryOldNode = ThePaths.FindNodePointer(VeryOldNode);
// 			Vector3 veryOldCoors;
// 			pVeryOldNode->GetCoors(veryOldCoors);
// 
// 			Vector3	diff1 = candidateCoors - oldCoors;
// 			Vector3	diff2 = oldCoors - veryOldCoors;
// 			diff1.z = diff2.z = 0.0f;
// 			diff1.Normalize();
// 			diff2.Normalize();
// 			if(diff1.Dot(diff2) < -0.2f)
// 			{
// 				return false;
// 			}
// 		}
// 	}

	return !bGoingAgainstTraffic;
}

//searches for a given junction node and uses the given nodelist to populate the pre-entrance, entrance, and exit nodes
u32 CPathNodeRouteSearchHelper::FindUpcomingJunctionTurnDirection(const CNodeAddress& junctionNode, const CVehicleNodeList* pNodeList, bool *pSharpTurn, float *pLeftness, const float fDotThreshold, float *pDotProduct, int& iNumExitLanesOut)
{
	if (!pNodeList)
	{
		return BIT_NOT_SET;
	}

	if (junctionNode.IsEmpty())
	{
		return BIT_NOT_SET;
	}

	//stop iterating at 1 before the end because we need to add 1 to it to find the exit node
	for (int i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1; i++)
	{
		const CNodeAddress& thisAddr = pNodeList->GetPathNodeAddr(i);
		const CNodeAddress& nextAddr = pNodeList->GetPathNodeAddr(i+1);

		if (thisAddr.IsEmpty() || nextAddr.IsEmpty()
			|| !ThePaths.IsRegionLoaded(thisAddr)
			|| !ThePaths.IsRegionLoaded(nextAddr))
		{
			return BIT_NOT_SET;
		}

		if (i > 0 && thisAddr == junctionNode)
		{
			const s32 iExitLink = pNodeList->GetPathLinkIndex(i);
			const CPathNodeLink* pExitLink = iExitLink >= 0 ? ThePaths.FindLinkPointerSafe(thisAddr.GetRegion(), iExitLink) : NULL;
			if (pExitLink)
			{
				iNumExitLanesOut = pExitLink->m_1.m_LanesToOtherNode;
			}
			else
			{
				iNumExitLanesOut = 0;
			}
			const CNodeAddress& preEntranceNode = i > 1 ? pNodeList->GetPathNodeAddr(i-2) : EmptyNodeAddress;
			return FindJunctionTurnDirection(preEntranceNode, pNodeList->GetPathNodeAddr(i-1), pNodeList->GetPathNodeAddr(i), pNodeList->GetPathNodeAddr(i+1), pSharpTurn, pLeftness, fDotThreshold, pDotProduct);
		}
	}

	return BIT_NOT_SET;
}

//given a junction node, an entrance and an exit node, calculate the direction of the turn by looking at the links
//immediately before and after the junction
u32 CPathNodeRouteSearchHelper::FindJunctionTurnDirection(const CNodeAddress& junctionPreEntranceNode, const CNodeAddress& junctionEntranceNode
	, const CNodeAddress& junctionNode, const CNodeAddress& junctionExitNode, bool *pSharpTurn, float *pLeftness, const float fDotThreshold\
	, float *pDotProduct, const CPathFind & pathfind)
{
	if (junctionEntranceNode.IsEmpty() || junctionNode.IsEmpty() || junctionExitNode.IsEmpty() 
		|| !pathfind.IsRegionLoaded(junctionEntranceNode.GetRegion())
		|| !pathfind.IsRegionLoaded(junctionNode.GetRegion())
		|| !pathfind.IsRegionLoaded(junctionExitNode.GetRegion()))
	{
		return BIT_NOT_SET;
	}

	const CPathNode* pFromNode = pathfind.FindNodePointerSafe(junctionEntranceNode);
	const CPathNode* pCurrNode = pathfind.FindNodePointerSafe(junctionNode);
	const CPathNode* pNewNode = pathfind.FindNodePointerSafe(junctionExitNode);
	if (pFromNode && pNewNode && pCurrNode)
	{
		Vector2 vFromTemp, vCurrTemp, vNewTemp;
		pFromNode->GetCoors2(vFromTemp);
		pCurrNode->GetCoors2(vCurrTemp);
		pNewNode->GetCoors2(vNewTemp);
		Vector2 vFromToCurr = vCurrTemp - vFromTemp;
		Vector2 vCurrToNew = vNewTemp - vCurrTemp;

		s16 iOldLink = -1;
		pathfind.FindNodesLinkIndexBetween2Nodes(pFromNode, pCurrNode, iOldLink);
		CPathNodeLink OldLink;
		if (iOldLink >= 0)
		{
			OldLink = pathfind.GetNodesLink(pFromNode, iOldLink);
		}

		//try looking at the link right before the intersection
		if (!junctionPreEntranceNode.IsEmpty() && pathfind.IsRegionLoaded(junctionPreEntranceNode) && iOldLink >= 0 /*&& OldLink.m_1.m_bDontUseForNavigation*/)
		{
			//CNodeAddress OldOldNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 3);
			const CPathNode* pOldOldNode = pathfind.FindNodePointerSafe(junctionPreEntranceNode);
			if (pOldOldNode)
			{
				Vector2 vOldOldTemp;
				pOldOldNode->GetCoors2(vOldOldTemp);
				vFromToCurr = vFromTemp - vOldOldTemp;
			}
		}
		if (/*pLink.m_1.m_bDontUseForNavigation &&*/ pNewNode->NumLinks() == 2)
		{
			//only do this if the next node isn't a junction
			//if it is, we don't know enough about the route at this point
			//to make a decision about what direction to use, so just rely 
			//on the direction of this link
			CPathNodeLink NewNewLink = pathfind.GetNodesLink(pNewNode, 0);
			if (NewNewLink.m_OtherNode == junctionNode)
			{
				NewNewLink = pathfind.GetNodesLink(pNewNode, 1);
			}
			const CPathNode* pNewNewNode = pathfind.FindNodePointerSafe(NewNewLink.m_OtherNode);
			if (pNewNewNode)
			{
				Vector2 vNewNewTemp;
				pNewNewNode->GetCoors2(vNewNewTemp);
				vCurrToNew = vNewNewTemp - vNewTemp;
			}
		}

		return FindPathDirectionInternal(vFromToCurr, vCurrToNew, pSharpTurn, pLeftness, fDotThreshold, pDotProduct);
	}

	return BIT_NOT_SET;
}

void CPathNodeRouteSearchHelper::GetZerothLanePositionsForNodesWithLink(const CPathNode* pNode, const CPathNode* pNextNode, const CPathNodeLink* pLink, Vector3& vCurrPosOut, Vector3& vNextPosOut)
{
	Assert(pNode);
	Assert(pLink);
	Assert(pNextNode);
	if (!pNode || !pLink || !pNextNode)
	{
		return;
	}

	Assert(pLink->m_OtherNode == pNextNode->m_address);

	const Vector3 vNodePos = pNode->GetPos();
	const Vector3 vOtherNodePos = pNextNode->GetPos();
	const Vector3 vLinkDir = vOtherNodePos - vNodePos;
	Vector3 vLinkRight(vLinkDir.y, -vLinkDir.x, 0.0f);
	if(vLinkRight.Mag2() > SMALL_FLOAT)
		vLinkRight.Normalize();
	const float fOffset = pLink->InitialLaneCenterOffset();

	vCurrPosOut = vNodePos + (fOffset * vLinkRight);
	vNextPosOut = vOtherNodePos + (fOffset * vLinkRight);
}

void CPathNodeRouteSearchHelper::QuickReplan(const s32 iLane)
{
	const int iTargetPoint = GetNodeList().GetTargetNodeIndex();
	GetNodeList().ClearFrom(iTargetPoint + 1);

	for (int i = iTargetPoint+1; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		CNodeAddress FromNode = GetNodeList().GetPathNodeAddr(i - 2);

		if (!aiVerifyf(!FromNode.IsEmpty() && ThePaths.IsRegionLoaded(FromNode), "FromNode empty or region not loaded: %d:%d", FromNode.GetRegion(), FromNode.GetIndex()))
		{
			break;
		}

		const CPathNode* pFromNode = ThePaths.FindNodePointerSafe(FromNode);

		s16 iLinkTemp=0;
		bool	bHappyWithNode = false;
		s32 LanesOnCurrentLink = ThePaths.FindLinkPointerSafe(FromNode.GetRegion(),GetNodeList().GetPathLinkIndex(i - 2))->m_1.m_LanesToOtherNode;
		bool bComingOutOfOneWayStreet =(ThePaths.FindLinkPointerSafe(FromNode.GetRegion(),GetNodeList().GetPathLinkIndex(i - 2))->m_1.m_LanesFromOtherNode == 0);

		GetNextNode(i, LanesOnCurrentLink, iLinkTemp, bHappyWithNode, bComingOutOfOneWayStreet, iLane == 0 ? BIT_TURN_LEFT : BIT_TURN_RIGHT, pFromNode->m_2.m_leftOnly, pFromNode->m_1.m_cannotGoLeft, pFromNode->m_1.m_cannotGoRight, false, XAXIS);

		if (!GetNodeList().GetPathNode(i) || !bHappyWithNode)
		{
			break;
		}

		if (m_route.GetPathNode(i - 1) && !m_route.GetPathNodeAddr(i).IsEmpty())
		{
			m_route.SetPathLinkIndex(i - 1, ThePaths.FindRegionLinkIndexBetween2Nodes(m_route.GetPathNodeAddr(i - 1), m_route.GetPathNodeAddr(i)));
		}

		if(GetNodeList().GetPathNode(i)->IsJunctionNode() && !GetNodeList().GetPathNode(i)->IsFalseJunction())
		{
			break;
		}
	}

	GetNodeList().FindLanesWithNodes(iTargetPoint, NULL, false);
}

bool HelperIsNodeBlocked(const CPathNode* pNode, const CNodeAddress& SlipJunctionNode)
{
	Assert(pNode);

	//quickly find the node after the one in our sliplane
	CNodeAddress NextNodeInLane;
	for (int n = 0; n < pNode->NumLinks(); n++)
	{
		const CPathNodeLink& link = ThePaths.GetNodesLink(pNode, n);
		if (link.m_OtherNode == SlipJunctionNode)
		{
			continue;
		}
		if (link.IsShortCut())
		{
			continue;
		}

		NextNodeInLane = link.m_OtherNode;
	}

	Assert(!NextNodeInLane.IsEmpty());
	const CPathNode* pNextNode = ThePaths.FindNodePointerSafe(NextNodeInLane);
	if (!pNextNode)
	{
		return false;
	}
	
	const Vector3 vCoords = pNode->GetPos();
	const Vector3 vOtherCoords = pNextNode->GetPos();
	const Vector3 vSegment = vOtherCoords - vCoords;

	//grcDebugDraw::Sphere(vCoords, 0.4f, Color_orange, false, 60);
	//grcDebugDraw::Sphere(vOtherCoords, 0.4f, Color_orange, false, 60);
	//grcDebugDraw::Line(vCoords, vOtherCoords, Color_orange, Color_orange, 60);

	//first things first--find the node after pNode that will be
	//the end point of the segment we're going to test

	//iterate through every vehicle we know about and see if
	//there's a vehicle stopped over the first node in the sliplane
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) VehiclePool->GetSize();

	while(i--)
	{
		pVehicle = VehiclePool->GetSlot(i);

		//only consider stationary vehicles
		if(pVehicle && pVehicle->GetAiXYSpeed() < 0.25f)
		{
			//test the bounding box (plus some padding) against the node's coords
			Vector3 vBoxMin = pVehicle->GetBoundingBoxMin();
			Vector3 vBoxMax = pVehicle->GetBoundingBoxMax();	

			//enfatten the boxen.
			//increase the length more than the width
			vBoxMax += Vector3(0.5f, 0.5f, 0.5f);
			vBoxMin += Vector3(-0.5f, -0.5f, -0.5f);

			//transform the test position into car space
			const Vector3 vCoordsLocal = VEC3V_TO_VECTOR3(pVehicle->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vCoords)));
			const Vector3 vSegmentLocal = VEC3V_TO_VECTOR3(pVehicle->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vSegment)));

			if (geomBoxes::TestSegmentToBoxMinMax(vCoordsLocal, vCoordsLocal + vSegmentLocal, vBoxMin, vBoxMax))
			{
				return true;
			}
		}
	}

	return false;
}

bool HelperFindNextNode(const CNodeAddress& previousAddress, const CNodeAddress& currentAddress, CNodeAddress& outNextAddress BANK_ONLY(, bool useDebugDraw = false))
{
	outNextAddress.SetEmpty();

	const CPathNode* currentNode = ThePaths.FindNodePointer(currentAddress);

	if (!currentNode->IsJunctionNode() || currentNode->IsFalseJunction())
	{
		for (u32 i = 0; i < currentNode->NumLinks(); ++i)
		{
			const CPathNodeLink& link = ThePaths.GetNodesLink(currentNode, i);

			if (link.m_OtherNode == previousAddress)
			{
				continue;
			}

			if (link.IsShortCut())
			{
				continue;
			}

			if (link.IsDontUseForNavigation())
			{
				continue;
			}

			if (!ThePaths.IsRegionLoaded(link.m_OtherNode))
			{
				break;
			}

			const CPathNode* linkedNode = ThePaths.FindNodePointer(link.m_OtherNode);

			if (linkedNode->IsSwitchedOff())
			{
				continue;
			}

			outNextAddress = link.m_OtherNode;
			break;
		}
	}

#if __BANK
	if (useDebugDraw && outNextAddress.IsEmpty())
	{
		Vector3 current;
		ThePaths.FindNodePointer(currentAddress)->GetCoors(current);
		current += Vector3(0.0f, 0.0f, 5.0f);
		grcDebugDraw::Sphere(current, 0.5f, Color_red);
	}
#endif // __BANK

	return !outNextAddress.IsEmpty();
}

bool HelperIsVehicleInLaneSegment(const CNodeAddress& fromAddress, const CPathNodeLink& link, const s32 lane, const float maxLength, const bool reverseClampDirection = false BANK_ONLY(, bool useDebugDraw = false))
{
	Vector3 fromPos;
	Vector3 toPos;

	ThePaths.FindNodePointer(fromAddress)->GetCoors(fromPos);
	ThePaths.FindNodePointer(link.m_OtherNode)->GetCoors(toPos);

	Vector3 laneOffset = ThePaths.GetLinkLaneOffset(fromAddress, link, lane);

	fromPos += laneOffset;
	toPos += laneOffset;

	if (link.m_1.m_Distance > maxLength)
	{
		if (reverseClampDirection)
		{
			Vector3 linkDir = fromPos - toPos;
			linkDir.Normalize();
			fromPos = toPos + (linkDir * maxLength);
		}
		else
		{
			Vector3 linkDir = toPos - fromPos;
			linkDir.Normalize();
			toPos = fromPos + (linkDir * maxLength);
		}
	}

	CSpatialArrayNode* result = NULL;

	const float maxDistanceFromLaneCenter = link.GetLaneWidth() / 4.0f;

	const bool vehicleInLaneSegment = CVehicle::GetSpatialArray().FindNearSegment(VECTOR3_TO_VEC3V(fromPos), VECTOR3_TO_VEC3V(toPos), maxDistanceFromLaneCenter, &result, 1) != 0;

#if __BANK
	if (useDebugDraw)
	{
		Vector3 laneOffsetDir(laneOffset);
		laneOffsetDir.Normalize();

		Matrix34 mat;

		mat.Identity();
		mat.RotateTo(Vector3(1.0f, 0.0f, 0.0f), laneOffsetDir);
		mat.Translate((fromPos + toPos) * 0.5f);

		const float halfLength = Min(static_cast<float>(link.m_1.m_Distance), maxLength) * 0.5f;

		grcDebugDraw::BoxOriented(Vec3V(-maxDistanceFromLaneCenter, -halfLength, 0.25f), Vec3V(maxDistanceFromLaneCenter, halfLength, 5.0f), MATRIX34_TO_MAT34V(mat), vehicleInLaneSegment ? Color_red : Color_green, false);
	}
#endif // __BANK

	return vehicleInLaneSegment;
}

float HelperCalculateDistanceOnPath(const Vector3& fromPos, const Vector3& toPos, const Vector2& pathDir)
{
	const float fromDist = pathDir.Dot(Vector2(fromPos, Vector2::kXY));
	const float toDist = pathDir.Dot(Vector2(toPos, Vector2::kXY));

	return toDist - fromDist;
}

bool HelperIsAdjacentLaneOpen(CVehicle* vehicle, const float forwardSearchDistance, const float reverseSearchDistance, s32& outLane BANK_ONLY(, bool useDebugDraw = false))
{
	Assert(vehicle);

	const CVehicleNodeList* nodeList = vehicle->GetIntelligence()->GetNodeList();

	if (!nodeList)
	{
		return false;
	}

	const s32 targetPathIndex = nodeList->GetTargetNodeIndex();
	const CNodeAddress& targetAddress = nodeList->GetPathNodeAddr(targetPathIndex);
	const s16 targetLinkIndex = nodeList->GetPathLinkIndex(targetPathIndex);

	if (targetLinkIndex == -1 || targetAddress.IsEmpty() || !ThePaths.IsRegionLoaded(targetAddress))
	{
		return false;
	}

	const CPathNodeLink* targetLink = ThePaths.FindLinkPointerSafe(targetAddress.GetRegion(), targetLinkIndex);

	if (targetLink->m_OtherNode.IsEmpty() || !ThePaths.IsRegionLoaded(targetLink->m_OtherNode))
	{
		return false;
	}

	const CVehicleFollowRouteHelper* followRouteHelper = vehicle->GetIntelligence()->GetFollowRouteHelper();

	if (!followRouteHelper)
	{
		return false;
	}

	const CRoutePoint* routePoints = followRouteHelper->GetRoutePoints();

	if (!routePoints)
	{
		return false;
	}

	const s16 currentTargetPoint = followRouteHelper->GetCurrentTargetPoint();
	const s32 currentLane = static_cast<s32>(RoundToNearestInt(routePoints[currentTargetPoint].GetLane()).Getf());

	const s32 numLanes = targetLink->m_1.m_LanesToOtherNode;

	if (numLanes <= 1)
	{
		return false;
	}

	static const s32 FORWARD = 0;
	static const s32 REVERSE = 1;

	CNodeAddress currentAddress[2] = { targetLink->m_OtherNode, targetAddress };
	CNodeAddress previousAddress[2] = { targetAddress, targetLink->m_OtherNode };
	const float searchDistance[2] = { forwardSearchDistance, reverseSearchDistance };

	const Vector3 vehiclePos = VEC3V_TO_VECTOR3(vehicle->GetVehiclePosition());
	
	Vector3 targetPos;
	ThePaths.FindNodePointer(targetAddress)->GetCoors(targetPos);

	Vector3 futurePos;
	ThePaths.FindNodePointer(targetLink->m_OtherNode)->GetCoors(futurePos);

	Vector2 linkDir = Vector2(futurePos - targetPos, Vector2::kXY);
	linkDir.Normalize();

	const float distanceToTarget = HelperCalculateDistanceOnPath(vehiclePos, targetPos, linkDir);

	float distance[2] = { distanceToTarget, -distanceToTarget };

#if __BANK
	if (useDebugDraw)
	{
		char buffer[32];
		sprintf(buffer, "Distance to Target: %.1f", distanceToTarget);
		grcDebugDraw::Text(vehiclePos, Color_white, 0, 0, buffer);
		grcDebugDraw::Sphere(vehiclePos + Vector3(0.0f, 0.0f, 5.0f), 0.25f, Color_cyan);
		grcDebugDraw::Sphere(targetPos + Vector3(0.0f, 0.0f, 5.0f), 0.25f, Color_cyan);
		grcDebugDraw::Line(vehiclePos + Vector3(0.0f, 0.0f, 5.0f), targetPos + Vector3(0.0f, 0.0f, 5.0f), Color_cyan);
	}
#endif // __BANK

	bool searchConditionMet[2] = { false, false };
	bool laneToLeftOpen = currentLane > 0;
	bool laneToRightOpen = currentLane < numLanes - 1;

	bool firstForwardPass = true;

	while ((!searchConditionMet[FORWARD] || !searchConditionMet[REVERSE]) && (laneToLeftOpen || laneToRightOpen))
	{
		for (u32 i = 0; i < 2; ++i)
		{
			if (!searchConditionMet[i])
			{
				const bool forward = i == FORWARD;

				CNodeAddress nextAddress;

				if (!firstForwardPass)
				{
					HelperFindNextNode(previousAddress[i], currentAddress[i], nextAddress BANK_ONLY(, useDebugDraw));
				}
				else
				{
					nextAddress = currentAddress[i];
					currentAddress[i] = previousAddress[i];
					firstForwardPass = false;
				}

				const CNodeAddress& fromAddress = forward ? currentAddress[i] : nextAddress;
				const CNodeAddress& toAddress = forward ? nextAddress : currentAddress[i];

				const CPathNodeLink* link = NULL;
				
				if (!nextAddress.IsEmpty())
				{
					const CPathNode* fromNode = ThePaths.FindNodePointerSafe(fromAddress);
					const CPathNode* toNode = ThePaths.FindNodePointerSafe(toAddress);

					link = ThePaths.FindLinkBetween2Nodes(fromNode, toNode);
				}

				if (!link || link->m_1.m_LanesToOtherNode != static_cast<u32>(numLanes) || link->m_1.m_bDontUseForNavigation || !ThePaths.IsRegionLoaded(nextAddress))
				{
#if __BANK
					if (useDebugDraw)
					{
						Vector3 currentPos;
						ThePaths.FindNodePointer(currentAddress[i])->GetCoors(currentPos);

						grcDebugDraw::Sphere(currentPos + Vector3(0.0f, 0.0f, 5.0f), 0.1f, Color_red);
					}
#endif // __BANK
					return false;
				}

#if __BANK
				if (useDebugDraw)
				{
					Vector3 currentPos;
					Vector3 nextPos;
					ThePaths.FindNodePointer(currentAddress[i])->GetCoors(currentPos);
					ThePaths.FindNodePointer(nextAddress)->GetCoors(nextPos);

					currentPos += Vector3(0.0f, 0.0f, 5.0f);
					nextPos += Vector3(0.0f, 0.0f, 5.0f);

					grcDebugDraw::Sphere(currentPos, 0.1f, Color_cyan);
					grcDebugDraw::Line(currentPos, nextPos, Color_cyan);
				}
#endif // __BANK

				laneToLeftOpen = laneToLeftOpen && !HelperIsVehicleInLaneSegment(fromAddress, *link, currentLane - 1, searchDistance[i] - distance[i], !forward BANK_ONLY(, useDebugDraw));
				laneToRightOpen = laneToRightOpen && !HelperIsVehicleInLaneSegment(fromAddress, *link, currentLane + 1, searchDistance[i] - distance[i], !forward BANK_ONLY(, useDebugDraw));

				previousAddress[i] = currentAddress[i];
				currentAddress[i] = nextAddress;

				distance[i] += static_cast<float>(link->m_1.m_Distance);

				searchConditionMet[i] = distance[i] >= searchDistance[i];
			}
		}
	}

	if (laneToLeftOpen && laneToRightOpen)
	{
		if (fwRandom::GetRandomNumberInRange(0, 1) == 0)
		{
			outLane = currentLane - 1;
		}
		else
		{
			outLane = currentLane + 1;
		}
	}
	else if (laneToLeftOpen)
	{
		outLane = currentLane - 1;
	}
	else if (laneToRightOpen)
	{
		outLane = currentLane + 1;
	}

	return laneToLeftOpen || laneToRightOpen;
}

void CPathNodeRouteSearchHelper::GetNextNode(const s32 autopilotNodeIndex, const s32 LanesOnCurrentLink, s16 &iLinkTemp, bool &bHappyWithNode, bool bComingOutOfOneWayStreet, const u32 iForceDirectionAtJunction, bool bLeftTurnsOnly, const bool bDisallowLeft, const bool bDisallowRight, const bool bTryToMatchDir, const Vector3& vTryToMatchDir)
{
	bool	bGoingAgainstTraffic, bGoingIntoOneWayStreet;
	s16	regionLinkIndexFairlyHappyTemp=0, regionLinkIndexMarginallyHappyTemp=0;
	bool	bFairlyHappyWithNode = false, bMarginallyHappyWithNode = false;
	s32	Loop;
	s32 iLinkForTryToMatchDir = -1;
	float fBestDotProductForTryToMatchDir = -1.0f;

	// First we identify all the PathDirections so that when there are multiple 'straights' we can make a better decision.
	atRangeArray<CNodeAddress, PF_MAXLINKSPERNODE>	aNodeAddresses;
	atRangeArray<u32, PF_MAXLINKSPERNODE>			aPathDirections;
	atRangeArray<u32, PF_MAXLINKSPERNODE>			aOriginalPathDirections;
	atRangeArray<float, PF_MAXLINKSPERNODE>			aLeftness;
	atRangeArray<bool, PF_MAXLINKSPERNODE>			aSharpTurn;
	atRangeArray<float, PF_MAXLINKSPERNODE>			aDirectionDotProducts;

	CNodeAddress FairlyHappyNode, MarginallyHappyNode;

	Assert(autopilotNodeIndex > 0);

	// Find a new node to go to.
	CNodeAddress FromNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 2);
	CNodeAddress CurrNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 1);

	// JB: Col saw a crash here, in which the region was not loaded; I've changed this to a FindNodePointerSafe()
	const CPathNode* pCurrNode = ThePaths.FindNodePointerSafe(CurrNode);
	if(!pCurrNode)
		return;

	const CPathNode* pFromNode = ThePaths.FindNodePointerSafe(FromNode);

	const bool bCurrNodeIsRealJunction = pCurrNode->IsJunctionNode() && !pCurrNode->IsFalseJunction();
	const bool bCurrNodeIsJunction = pCurrNode->IsJunctionNode();
	const s32 currNodeNumLinks = pCurrNode->NumLinks();
	SemiShuffledSequence shuff(currNodeNumLinks);
	for(Loop=0; Loop<currNodeNumLinks; Loop++)
	{
		aNodeAddresses[Loop].SetEmpty();
		aPathDirections[Loop] = 0;
		aOriginalPathDirections[Loop] = 0;
		aLeftness[Loop] = 0.0f;
		aSharpTurn[Loop] = false;
		aDirectionDotProducts[Loop] = 0.0f;
	}

	//-----------------------------
	//	(1<<0) = straight
	//	(1<<1) = right
	//	(1<<2) = left

	u32	AllowedDirs = 0;

	const s8 iCurrentLane = m_route.GetPathLaneIndex(autopilotNodeIndex - 2);
	bool bForceLeftTurn = iCurrentLane == 0 && bLeftTurnsOnly;

	//don't allow a left turn if this node pair has a slip lane that isn't us
	if(iCurrentLane == 0 && !bDisallowLeft)
	{
		AllowedDirs |= BIT_TURN_LEFT;	// Left allowed
	}
	if(iCurrentLane ==(LanesOnCurrentLink - 1) && !bForceLeftTurn && !bDisallowRight)
	{
		AllowedDirs |= BIT_TURN_RIGHT;	// Right allowed
	}
	if(LanesOnCurrentLink >= 3 && AllowedDirs && bCurrNodeIsRealJunction)
	{
		//TODO: not sure about this.
		//it appears as if this logic is saying 
		//"if we are on a road with 3 or more lanes,
		//and we already think we should go right or left, don't allow
		//going straight.
		//but why would we not want guys going straight from the leftmost or 
		//rightmost lanes?
		//For now, allow straights anyway if this isn't a "real" junction
	}
	else if (!bForceLeftTurn)
	{
		AllowedDirs |= BIT_TURN_STRAIGHT_ON;	// Straight on allowed
	}

	// JB: Override AllowedDirs if iForceDirectionAtJunction is set, and this is a junction node

	if(pCurrNode->NumLinks() > 2 && iForceDirectionAtJunction)
	{
		Assert(iForceDirectionAtJunction==BIT_TURN_STRAIGHT_ON||iForceDirectionAtJunction==BIT_TURN_LEFT||iForceDirectionAtJunction==BIT_TURN_RIGHT);
		AllowedDirs = iForceDirectionAtJunction;
		if (iForceDirectionAtJunction == BIT_TURN_LEFT)
		{
			bLeftTurnsOnly = true;
			bForceLeftTurn = true;
		}
	}
	else if (!bForceLeftTurn && (m_iSearchFlags & Flag_AvoidTurns) != 0)
	{
		AllowedDirs = BIT_TURN_STRAIGHT_ON;
	}

	// JB: Check for filter lanes into junctions.
	//const int iSlipLaneLink = GetSlipLaneNodeLinkIndex(pCurrNode, FromNode);
	const int iHighwayExitLink = GetHighwayExitLinkIndex(pCurrNode, FromNode);

	static dev_float s_fExitHighwayPercentage = 0.25f;

	bool bAnyTrueLefts = false;
	s32 existingStraight = -1;
	s32 leftestStraight = -1;
	s32 iNumLanesOnLeftestStraight = 0;
	bool bMultipleStraights = false;
	bool bAnyUnloaded = false;
	bool bAnyNonSharp = false;
	bool bAnyStraightThatAllowBigVehs = false;
	bool bAnyNonRestricted = false;
	bool bAnyRestricted = false;
	for(Loop = 0; Loop < currNodeNumLinks; Loop++)
	{
		s32 iRandomLink = shuff.GetElement(Loop);
		aPathDirections[iRandomLink] = BIT_NOT_SET;
		aOriginalPathDirections[iRandomLink] = BIT_NOT_SET;
		CNodeAddress newNode = ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink);

		const CPathNode* pNewNode = ThePaths.FindNodePointerSafe(newNode);

		if (pNewNode && !pNewNode->IsRestrictedArea())
		{
			bAnyNonRestricted = true;
		}
		else if (pNewNode && pNewNode->IsRestrictedArea())
		{
			bAnyRestricted = true;
		}

		if(FromNode == newNode)		// Don't consider the node we were coming from
		{
			continue;
		}
		if(!ThePaths.IsRegionLoaded(newNode))
		{
			bAnyUnloaded = true;
			continue;
		}

		const CPathNodeLink & pLink = ThePaths.GetNodesLink(pCurrNode, iRandomLink);

		// Wandering cars should never consider shortcut links
		if(pLink.IsShortCut())
		{
			continue;
		}

		//if this is a sliplane and we can't go left, don't enter it
		if (pNewNode && pNewNode->IsSlipLane() && ((AllowedDirs & BIT_TURN_LEFT) == 0))
		{
			continue;
		}

		//if this is a sliplane on a highway, randomly decide not to enter it,
		//as most of the traffic should prefer to stay on the highway
		if (pNewNode && pNewNode->IsSlipLane() && pCurrNode->IsHighway()
			&& pFromNode && pFromNode->IsHighway()
			&& g_ReplayRand.GetFloat() > s_fExitHighwayPercentage)
		{
			continue;
		}

		//if we're not currently off-road, and our vehicle doens't support offroad wandering,
		//don't choose an off-road link
		if ((m_iSearchFlags & Flag_WanderOffroad) == 0 && !pCurrNode->m_1.m_Offroad
			&& pNewNode && pNewNode->m_1.m_Offroad)
		{
			continue;
		}

		//if we are off-road wandering, don't choose a non-offroad node
		if ((m_iSearchFlags & Flag_WanderOffroad) != 0 && pCurrNode->m_1.m_Offroad
			&& pNewNode && !pNewNode->m_1.m_Offroad)
		{
			continue;
		}

		if ((m_iSearchFlags & Flag_AvoidRestrictedAreas) != 0 && !pCurrNode->IsRestrictedArea()
			&& pNewNode && pNewNode->IsRestrictedArea())
		{
			continue;
		}

		//if we want to avoid turns, or stay on highways, don't exit
		//a highway. the reason we check avoidturns here is that highway exit
		//ramps read as straight ahead lanes, but we can be pretty sure there will be a turn
		//once they exit
		if (((m_iSearchFlags & Flag_HighwaysOnly) != 0 || (m_iSearchFlags & Flag_AvoidTurns) != 0) 
			&&  (iRandomLink == iHighwayExitLink))
		{
			continue;
		}

		// Possibility to avoid entering one-way streets.
		// Currently used to prevent considering slip-roads at junctions.
		// Even those these would be disregarded later, if they are processed at this
		// stage it can prevent us from seeing a valid exit route from the junction.
		if(iForceDirectionAtJunction && pCurrNode->NumLinks() > 2)
		{
			if((pLink.m_1.m_LanesToOtherNode == 0 && pLink.m_1.m_LanesFromOtherNode == 1))
				//|| (pLink.m_1.m_LanesToOtherNode == 1 && pLink.m_1.m_LanesFromOtherNode == 0))
				continue;
		}

		//if this is a sliplane and we'd otherwise allow it, make sure it isn't blocked by traffic
		if (pNewNode && pNewNode->IsSlipLane() && !pCurrNode->IsSlipLane() && HelperIsNodeBlocked(pNewNode, CurrNode))
		{
			continue;
		}

		aNodeAddresses[iRandomLink] = newNode;
		
		if (pFromNode && pNewNode)
		{
			Vector2 vFromTemp, vCurrTemp, vNewTemp;
			pFromNode->GetCoors2(vFromTemp);
			pCurrNode->GetCoors2(vCurrTemp);
			pNewNode->GetCoors2(vNewTemp);
			Vector2 vFromToCurr = vCurrTemp - vFromTemp;
			Vector2 vCurrToNew = vNewTemp - vCurrTemp;

			s16 iOldLink = -1;
			ThePaths.FindNodesLinkIndexBetween2Nodes(pFromNode, pCurrNode, iOldLink);
			CPathNodeLink OldLink;
			if (iOldLink >= 0)
			{
				OldLink = ThePaths.GetNodesLink(pFromNode, iOldLink);
			}

			//try looking at the link right before the intersection
			//only do this for nodes with the junction flag set.
			//if we have multiple links but are not on a junction,
			//it's probably a fork in the road, in which case
			//we'd rather use the direction of the DontUseForNavigation link,
			//since it's likely that the link after will be close to parallel
			//with the source link
			if (bCurrNodeIsRealJunction && autopilotNodeIndex > 2 && iOldLink >= 0 && OldLink.m_1.m_bDontUseForNavigation)
			{
				CNodeAddress OldOldNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 3);
				const CPathNode* pOldOldNode = ThePaths.FindNodePointerSafe(OldOldNode);
				if (pOldOldNode)
				{
					Vector2 vOldOldTemp;
					pOldOldNode->GetCoors2(vOldOldTemp);
					vFromToCurr = vFromTemp - vOldOldTemp;
				}
			}


			if (bCurrNodeIsRealJunction && pLink.m_1.m_bDontUseForNavigation && !pNewNode->m_2.m_slipJunction)
			{
				//TODO: handle shortcut links here. this might be responsible for some of the u-turns we're seeing

				//only do this if the next node isn't a junction
				//if it is, we don't know enough about the route at this point
				//to make a decision about what direction to use, so just rely 
				//on the direction of this link
				CPathNodeLink NewNewLink;

#if __DEV
				bool bFoundGoodNewNewLink = false;
#endif //__DEV
				
				for (int i = 0; i < pNewNode->NumLinks(); i++)
				{
					NewNewLink = ThePaths.GetNodesLink(pNewNode, i);

					if (NewNewLink.m_OtherNode != CurrNode 
						&& !NewNewLink.IsShortCut())
					{
						//we found the right link
#if __DEV
						bFoundGoodNewNewLink = true;
#endif //__DEV
						break;
					}
				}

#if __DEV
				Assert(bFoundGoodNewNewLink);
#endif //__DEV

				const CPathNode* pNewNewNode = ThePaths.FindNodePointerSafe(NewNewLink.m_OtherNode);
				if (pNewNewNode)
				{
					Vector2 vNewNewTemp;
					pNewNewNode->GetCoors2(vNewNewTemp);
					vCurrToNew = vNewNewTemp - vNewTemp;
				}
			}

			aPathDirections[iRandomLink] = FindPathDirectionInternal(vFromToCurr, vCurrToNew, &aSharpTurn[iRandomLink], &aLeftness[iRandomLink], sm_fDefaultDotThresholdForRoadStraightAhead, &aDirectionDotProducts[iRandomLink]);

			//save the index of the link that's closest to the target direction
			//only do this for junction, not forks in the road, as there are
			//otherwise no restrictions on which way to go at a fork,
			//so it will cause all traffic on a highway to do a bad merge
			if (bTryToMatchDir && bCurrNodeIsRealJunction
				&& !aSharpTurn[iRandomLink] 
			&& (AllowedDirs & aPathDirections[iRandomLink])
			&& pLink.m_1.m_LanesToOtherNode > 0)
			{
				Vector3 vCurrentPosWithLane, vNextPosWithLane, vDeltaWithLane;
				GetZerothLanePositionsForNodesWithLink(pCurrNode, pNewNode, &pLink, vCurrentPosWithLane, vNextPosWithLane);
				vDeltaWithLane = vNextPosWithLane - vCurrentPosWithLane;
				vDeltaWithLane.z = 0.0f;
				vDeltaWithLane.NormalizeSafe(ORIGIN);

				const float fDotThisLink = vDeltaWithLane.Dot(vTryToMatchDir);
				if (fDotThisLink > fBestDotProductForTryToMatchDir)
				{
					fBestDotProductForTryToMatchDir = fDotThisLink;
					iLinkForTryToMatchDir = iRandomLink;
				}
			}
		}
		else
		{
			aPathDirections[iRandomLink] = FindPathDirection(FromNode, CurrNode, newNode, &aSharpTurn[iRandomLink], &aLeftness[iRandomLink], sm_fDefaultDotThresholdForRoadStraightAhead, &aDirectionDotProducts[iRandomLink]);
		}

		if(aPathDirections[iRandomLink] == 0)		// This can sometimes happen when nodes have been streamed out or car doesn't have them. Assume straight.
		{
			aPathDirections[iRandomLink] = BIT_TURN_STRAIGHT_ON;
		}

		//save off the original direction
		aOriginalPathDirections[iRandomLink] = aPathDirections[iRandomLink];
		if (aPathDirections[iRandomLink] == BIT_TURN_LEFT && !aSharpTurn[iRandomLink])
		{
			bAnyTrueLefts = true;
		}

		if (!aSharpTurn[iRandomLink])
		{
			bAnyNonSharp = true;
		}

		//used for preventing big vehicles from getting into right-hand exit links,
		//that appear as straight but will eventually lead the vehicle into a turn
		if (aPathDirections[iRandomLink] == BIT_TURN_STRAIGHT_ON
			&& !pNewNode->BigVehiclesProhibited())
		{
			bAnyStraightThatAllowBigVehs = true;
		}

		const CPathNode* pExistingStraightTarget = existingStraight >= 0 ? ThePaths.GetNodesLinkedNode(pCurrNode, existingStraight) : NULL;
		const CPathNode* pLeftestStraightTarget = leftestStraight >= 0 ? ThePaths.GetNodesLinkedNode(pCurrNode, leftestStraight) : NULL;

		if(aPathDirections[iRandomLink] == BIT_TURN_STRAIGHT_ON 
			&& (!pExistingStraightTarget || !pExistingStraightTarget->IsSlipLane())
			&& (!pLeftestStraightTarget || !pLeftestStraightTarget->IsSlipLane())
			&& (!pNewNode || !pNewNode->IsSlipLane()) /*(slip-lanes will always be colinear)*/
			&& pLink.m_1.m_LanesToOtherNode > 0 /*don't let a sliplane in the opposing direction claim the "most straight" designation*/ 
			&& (pCurrNode->IsSwitchedOff() || !pNewNode->IsSwitchedOff())) 
		{
			if(existingStraight < 0)
			{
				existingStraight = iRandomLink;
				leftestStraight = iRandomLink;
				iNumLanesOnLeftestStraight = pLink.m_1.m_LanesToOtherNode;
			}
			else
			{
				// We have 2 'straights' one of them will have to change.
				bMultipleStraights = true;
				if (aLeftness[iRandomLink] > aLeftness[existingStraight])
				{
					leftestStraight = iRandomLink;
					iNumLanesOnLeftestStraight = pLink.m_1.m_LanesToOtherNode;
				}
				if(rage::Abs(aLeftness[iRandomLink]) < rage::Abs(aLeftness[existingStraight]))
				{
					if(aLeftness[existingStraight] > 0)
					{
						aPathDirections[existingStraight] = BIT_TURN_LEFT;
					}
					else
					{
						aPathDirections[existingStraight] = BIT_TURN_RIGHT;
					}
					existingStraight = iRandomLink;
				}
				else
				{
					if(aLeftness[iRandomLink] > 0.0f)
					{
						aPathDirections[iRandomLink] = BIT_TURN_LEFT;
					}
					else
					{
						aPathDirections[iRandomLink] = BIT_TURN_RIGHT;
					}
				}
			}
		}
		Assert(aPathDirections[iRandomLink] == BIT_TURN_LEFT || aPathDirections[iRandomLink] == BIT_TURN_RIGHT || aPathDirections[iRandomLink] == BIT_TURN_STRAIGHT_ON);
	}

	//if we didn't find any straight ahead turns,
	//and we're prevented from going left or right,
	//allow turns in the non-prevented direction
	if (existingStraight < 0 && bDisallowRight)
	{
		AllowedDirs |= BIT_TURN_LEFT;
	}
	else if (existingStraight < 0 && bDisallowLeft)
	{
		AllowedDirs |= BIT_TURN_RIGHT;
	}

	for(Loop = 0; Loop < currNodeNumLinks; Loop++)
	{
		int iRandomLink = shuff.GetElement(Loop);

		if(aPathDirections[iRandomLink] == 0 || aPathDirections[iRandomLink] == BIT_NOT_SET)
		{
			continue;
		}
		m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));
		if(FromNode == m_route.GetPathNodeAddr(autopilotNodeIndex))		// Don't consider the node we were coming from
		{
			continue;
		}

		if(!ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex)))
		{
			continue;
		}

		const CPathNode* pPotentialNewNode = ThePaths.FindNodePointerSafe(m_route.GetPathNodeAddr(autopilotNodeIndex));

		//if this is a sliplane, and our previous lane wasn't zero, we can't enter it
		if(pPotentialNewNode && pPotentialNewNode->IsSlipLane() && iCurrentLane != 0)
		{
			continue;
		}

		const u32 PathDirection = bCurrNodeIsRealJunction || !bMultipleStraights 
			? aPathDirections[iRandomLink]
			: aOriginalPathDirections[iRandomLink];
		Assert(PathDirection == BIT_TURN_LEFT || PathDirection == BIT_TURN_RIGHT || PathDirection == BIT_TURN_STRAIGHT_ON);

		//if this is a true highway junction, don't exit it so much
		//unless it's a straight ahead
		if (bCurrNodeIsJunction && pCurrNode->IsHighway()
			&& pFromNode && pFromNode->IsHighway()
			&& PathDirection != BIT_TURN_STRAIGHT_ON
			&& pPotentialNewNode && !pPotentialNewNode->IsHighway() && g_ReplayRand.GetFloat() > s_fExitHighwayPercentage)
		{
			continue;
		}

		//if we want to avoid turns, and we know we have a straight-ahead
		//link that allows us, avoid entering NoBigVehs nodes
		if ((m_iSearchFlags & Flag_AvoidTurns) != 0
			&& bAnyStraightThatAllowBigVehs
			&& pPotentialNewNode->BigVehiclesProhibited())
		{
			continue;
		}

		// If we are trying to force a turn in this direction, then disregard that its 'sharp'
		// But only if the direction doesn't double back completely (which might mean doing a U-turn at the junction)
		const float fDirectionDot = aDirectionDotProducts[iRandomLink];
		static dev_float s_fReallySharpTurnDotThreshold = -0.77f;
		const bool bSharpTurn =
			((PathDirection==iForceDirectionAtJunction || (PathDirection == BIT_TURN_LEFT && bForceLeftTurn)) && pCurrNode->NumLinks() > 2 && fDirectionDot > s_fReallySharpTurnDotThreshold) ? false : aSharpTurn[iRandomLink];

		// Work out whether we're going against traffic here
		bGoingAgainstTraffic = false;
		bGoingIntoOneWayStreet = false;
		iLinkTemp = static_cast<s16>(iRandomLink);
		const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pCurrNode, iRandomLink);

		if(pLink->m_1.m_LanesFromOtherNode == 0) bGoingIntoOneWayStreet = true;
		if(pLink->m_1.m_LanesToOtherNode == 0) bGoingAgainstTraffic = true;

		if(IsThisAnAppropriateNode(FromNode, CurrNode, m_route.GetPathNodeAddr(autopilotNodeIndex), bGoingAgainstTraffic, bGoingIntoOneWayStreet, bComingOutOfOneWayStreet))
		{	
			if((PathDirection & AllowedDirs) && !bSharpTurn)
			{
				//if we're trying to force a left turn, and we know that we have a good lane to turn into,
				//and this isn't it, don't consider ourselves truly happy
				if ((bForceLeftTurn && bAnyTrueLefts 
					&& aOriginalPathDirections[iRandomLink] != BIT_TURN_LEFT)
					|| (iLinkForTryToMatchDir >= 0 && iLinkForTryToMatchDir != iRandomLink))
				{
					bFairlyHappyWithNode = true;
					regionLinkIndexFairlyHappyTemp = iLinkTemp;
					FairlyHappyNode = m_route.GetPathNodeAddr(autopilotNodeIndex);
				}
				else if (bCurrNodeIsRealJunction || !bMultipleStraights 
					|| (PathDirection & BIT_TURN_STRAIGHT_ON)==0
					|| LanesOnCurrentLink < 2)
				{
					bHappyWithNode = true;		// Perfect. We have our node.
					break;
				}
				else
				{
					//we're at a fork in the road. depending on which lane we're in, we may not
					//be allowed to go this way, even if its PathDirection is BIT_TURN_STRAIGHT_ON
					//for example, a four-lane freeway forking into two two-lane freeways.
					//the left fork might be BIT_TURN_STRAIGHT_ON, but
					//if we're either of the two right lanes, we should prefer the right fork
					//EXCEPTION: when iNumLanesOnLeftestStraight is the same as LanesOnCurrentLink.
					//this usually signifies an offramp. in this case, prefer left for all
					//lanes except the rightmost, where we express no preference
					const bool bConsideringOffRamp = LanesOnCurrentLink == iNumLanesOnLeftestStraight
						&& iCurrentLane == LanesOnCurrentLink -1;
					const bool bShouldPreferLeft = iCurrentLane < iNumLanesOnLeftestStraight;
					const bool bIsThisLinkLeftestStraight = iRandomLink == leftestStraight;
					if (!bConsideringOffRamp && ((bShouldPreferLeft && bIsThisLinkLeftestStraight) 
						|| (!bShouldPreferLeft && !bIsThisLinkLeftestStraight)))
					{
						bHappyWithNode = true;
						break;
					}
					else
					{
						bFairlyHappyWithNode = true;
						regionLinkIndexFairlyHappyTemp = iLinkTemp;
						FairlyHappyNode = m_route.GetPathNodeAddr(autopilotNodeIndex);
					}
				}
			}
			else if(PathDirection & BIT_TURN_STRAIGHT_ON)
			{								// This one would do if we can't find a better one. Carry on looking.
				bFairlyHappyWithNode = true;
				regionLinkIndexFairlyHappyTemp = iLinkTemp;
				FairlyHappyNode = m_route.GetPathNodeAddr(autopilotNodeIndex);
			}
			else if (!bSharpTurn && bAnyNonSharp)
			{								// This one is in completely the wrong direction but if we can't find a better one we'll take it. Carry on looking.
				bMarginallyHappyWithNode = true;
				regionLinkIndexMarginallyHappyTemp = iLinkTemp;
				MarginallyHappyNode = m_route.GetPathNodeAddr(autopilotNodeIndex);
			}
		}
	}

	// If we haven't found a node in 100% the right direction but it is an acceptable node we use it.
	if(!bHappyWithNode)
	{
		if(bFairlyHappyWithNode)
		{
			bHappyWithNode = true;
			iLinkTemp = regionLinkIndexFairlyHappyTemp;
			m_route.SetPathNodeAddr(autopilotNodeIndex, FairlyHappyNode);
		}
	}
	if(!bHappyWithNode)
	{
		if(bMarginallyHappyWithNode)
		{
			bHappyWithNode = true;
			iLinkTemp = regionLinkIndexMarginallyHappyTemp;
			m_route.SetPathNodeAddr(autopilotNodeIndex, MarginallyHappyNode);
		}
	}

	//we want to avoid a restricted area, and we have a choice to make between
	//some restricted nodes and some non-restricted nodes
	//if we could legally choose a way around, we would have done so above
	//so if we make it this far, let's just choose the first non-restricted
	//node we find
	//this is really here to allow u-turning away from restricted areas
	if (!bHappyWithNode && (m_iSearchFlags & Flag_AvoidRestrictedAreas) != 0
		&& bAnyNonRestricted && bAnyRestricted)
	{
		for(Loop = 0; Loop < currNodeNumLinks; Loop++)
		{
			int iRandomLink = shuff.GetElement(Loop);
			m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));
			if(ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex)))
			{
				const CPathNode* pRandomNode = ThePaths.GetNodesLinkedNode(pCurrNode, iRandomLink);
				if (pRandomNode && !pRandomNode->IsRestrictedArea())
				{
					bHappyWithNode = true;
					break;
				}
			}
		}
	}

	//special case: if we're on a switched off node, try and look for one that's switched on
	//even if it's a dead end: the way the dead-end calculation works, it ignores switched
	//off nodes so we're likely to be getting back onto the road here.
	//only do this if the new node is switched on originally though
	if (!bHappyWithNode && ThePaths.FindNodePointer(FromNode)->m_2.m_switchedOff
		&& ThePaths.FindNodePointer(FromNode)->m_2.m_switchedOffOriginal)
	{
		for(Loop = 0; Loop < currNodeNumLinks; Loop++)
		{
			int iRandomLink = shuff.GetElement(Loop);
			m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));

			if(ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex)))
			{
				//					PathDirection = FindPathDirection(FromNode, CurrNode, pVeh->GetIntelligence()->aNodes[autopilotNodeIndex], &bSharpTurn);
				// Work out whether we're going against traffic here
				bGoingAgainstTraffic = false;
				bGoingIntoOneWayStreet = false;

				iLinkTemp = static_cast<s16>( iRandomLink );
				const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pCurrNode, iRandomLink);

				if(pLink->m_1.m_LanesFromOtherNode == 0) bGoingIntoOneWayStreet = true;
				if(pLink->m_1.m_LanesToOtherNode == 0) bGoingAgainstTraffic = true;

				if((FromNode != m_route.GetPathNodeAddr(autopilotNodeIndex)) &&
					(!bGoingAgainstTraffic) &&
					!ThePaths.FindNodePointer(m_route.GetPathNodeAddr(autopilotNodeIndex))->m_2.m_switchedOff &&
					!ThePaths.FindNodePointer(m_route.GetPathNodeAddr(autopilotNodeIndex))->m_2.m_switchedOffOriginal)
				{
					bHappyWithNode = true;
					break;
				}
			}
		}
	}

	//if we haven't found a good node yet, try finding one
	//that isn't switched off, isn't a dead end, and isn't against traffic
	//and isn't a u-turn
	if(!bHappyWithNode)
	{
		for(Loop = 0; Loop < currNodeNumLinks; Loop++)
		{
			int iRandomLink = shuff.GetElement(Loop);
			m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));

			if(ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex)))
			{
				//					PathDirection = FindPathDirection(FromNode, CurrNode, pVeh->GetIntelligence()->aNodes[autopilotNodeIndex], &bSharpTurn);
				// Work out whether we're going against traffic here
				bGoingAgainstTraffic = false;
				bGoingIntoOneWayStreet = false;

				iLinkTemp = static_cast<s16>( iRandomLink );
				const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pCurrNode, iRandomLink);

				if(pLink->m_1.m_LanesFromOtherNode == 0) bGoingIntoOneWayStreet = true;
				if(pLink->m_1.m_LanesToOtherNode == 0) bGoingAgainstTraffic = true;

				if((FromNode != m_route.GetPathNodeAddr(autopilotNodeIndex)) &&
					(!bGoingAgainstTraffic) &&
					!(ThePaths.FindNodePointer(m_route.GetPathNodeAddr(autopilotNodeIndex))->m_2.m_switchedOff && !ThePaths.FindNodePointer(FromNode)->m_2.m_switchedOff) &&
					(ThePaths.FindNodePointer(m_route.GetPathNodeAddr(autopilotNodeIndex))->m_2.m_deadEndness <= ThePaths.FindNodePointer(FromNode)->m_2.m_deadEndness))
				{
					bHappyWithNode = true;
					break;
				}
			}
		}
	}

	//if we haven't found a good node yet, try finding one
	//that isn't switched off, isn't against traffic
	//and isn't a u-turn
	if(!bHappyWithNode)
	{
		for(Loop = 0; Loop < currNodeNumLinks; Loop++)
		{
			int iRandomLink = shuff.GetElement(Loop);
			m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));

			if(ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex)))
			{
				//					PathDirection = FindPathDirection(FromNode, CurrNode, pVeh->GetIntelligence()->aNodes[autopilotNodeIndex], &bSharpTurn);
				// Work out whether we're going against traffic here
				bGoingAgainstTraffic = false;
				bGoingIntoOneWayStreet = false;

				iLinkTemp = static_cast<s16>( iRandomLink );
				const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pCurrNode, iRandomLink);

				if(pLink->m_1.m_LanesFromOtherNode == 0) bGoingIntoOneWayStreet = true;
				if(pLink->m_1.m_LanesToOtherNode == 0) bGoingAgainstTraffic = true;

				if((FromNode != m_route.GetPathNodeAddr(autopilotNodeIndex)) &&
					(!bGoingAgainstTraffic) &&
					!(ThePaths.FindNodePointer(m_route.GetPathNodeAddr(autopilotNodeIndex))->m_2.m_switchedOff && !ThePaths.FindNodePointer(FromNode)->m_2.m_switchedOff))
				{
					bHappyWithNode = true;
					break;
				}
			}
		}
	}

	//if we haven't found a good node yet, prefer one that isn't
	//a shortcut link, not against traffic
	//and isn't a u-turn
	if(!bHappyWithNode)
	{
		for(Loop = 0; Loop < currNodeNumLinks; Loop++)
		{
			int iRandomLink = shuff.GetElement(Loop);
			m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));

			if(ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex)))
			{
				//					PathDirection = FindPathDirection(FromNode, CurrNode, pVeh->GetIntelligence()->aNodes[autopilotNodeIndex], &bSharpTurn);
				// Work out whether we're going against traffic here
				bGoingAgainstTraffic = false;
				bGoingIntoOneWayStreet = false;

				iLinkTemp = static_cast<s16>( iRandomLink );
				const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pCurrNode, iRandomLink);

				if(pLink->m_1.m_LanesFromOtherNode == 0) bGoingIntoOneWayStreet = true;
				if(pLink->m_1.m_LanesToOtherNode == 0) bGoingAgainstTraffic = true;

				if((FromNode != m_route.GetPathNodeAddr(autopilotNodeIndex)) &&
					(!bGoingAgainstTraffic) &&
					!pLink->IsShortCut()
					)
				{
					bHappyWithNode = true;
					break;
				}
			}
		}
	}

	//if we still haven't found a good node, try finding one that's not against traffic
	//and isn't a u-turn
	if(!bHappyWithNode)
	{
		for(Loop = 0; Loop < currNodeNumLinks; Loop++)
		{
			s32 iRandomLink = shuff.GetElement(Loop);
			m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));

			if(ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex)))
			{
				//					PathDirection = FindPathDirection(FromNode, CurrNode, pVeh->GetIntelligence()->aNodes[autopilotNodeIndex], &bSharpTurn);
				// Work out whether we're going against traffic here
				bGoingAgainstTraffic = false;
				bGoingIntoOneWayStreet = false;

				iLinkTemp = static_cast<s16>( iRandomLink );
				const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pCurrNode, iRandomLink);

				if(pLink->m_1.m_LanesFromOtherNode == 0) bGoingIntoOneWayStreet = true;
				if(pLink->m_1.m_LanesToOtherNode == 0) bGoingAgainstTraffic = true;

				if((FromNode != m_route.GetPathNodeAddr(autopilotNodeIndex)) && !bGoingAgainstTraffic)
				{
					bHappyWithNode = true;
					break;
				}
			}	
		}
	}

	//if all else fails, prefer facing an unloaded region
	//(it's likely to be streamed in soon) over u-turning
	if (!bHappyWithNode && bAnyUnloaded)
	{
		for(Loop = 0; Loop < currNodeNumLinks; Loop++)
		{
			s32 iRandomLink = shuff.GetElement(Loop);
			m_route.SetPathNodeAddr(autopilotNodeIndex, ThePaths.GetNodesLinkedNodeAddr(pCurrNode, iRandomLink));

			if (!ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(autopilotNodeIndex))
				&& FromNode != m_route.GetPathNodeAddr(autopilotNodeIndex))
			{
				//but still don't consider ourselves "happy"
				bHappyWithNode = false;
				break;
			}
		}
	}
}

void CPathNodeRouteSearchHelper::GetNextNodeAvoidingPoint(const s32 autopilotNodeIndex, s32 /*LanesOnCurrentLink*/, s16 &iLinkTemp, bool &bHappyWithNode, bool /*bComingOutOfOneWayStreet*/, const u32 /*iForceDirectionAtJunction*/, const bool /*bLeftTurnsOnly*/, const bool /*bDisallowLeft*/, const bool /*bDisallowRight*/, const bool /*bDisallowSwitchedOff*/)
{
	Vector3 pointToAvoid(0.0f, 0.0f, 0.0f);

	// Find a new node to go to.
	CNodeAddress FromNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 2);
	CNodeAddress CurrNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 1);

	const CPathNode* pCurrNode = ThePaths.FindNodePointer(CurrNode);
	Assert(pCurrNode);

	const s32 currNodeNumLinks = pCurrNode->NumLinks();
	Vector3	currNodePos;
	pCurrNode->GetCoors(currNodePos);
	
	const CPathNode* pFromNode = ThePaths.FindNodePointerSafe(FromNode);
	const CPathNodeLink* pPrevLink = NULL;
	Vector3 vPrevLinkDir;
	if (pFromNode)
	{
		pPrevLink = ThePaths.FindLinkBetween2Nodes(pFromNode, pCurrNode);
		const Vector3 fromNodePos = pFromNode->GetPos();
		vPrevLinkDir = currNodePos - fromNodePos;
		vPrevLinkDir.z = 0.0f;
		vPrevLinkDir.NormalizeFast();
	}

	pointToAvoid = m_vTargetPosition;

	float	smallestScoreSoFar = FLT_MAX;
	CNodeAddress bestNodeSoFar;

	// We are only interested in moving away from the thing we are fleeing from.
	Vector3 vToAvoidPoint = pointToAvoid - currNodePos;
	vToAvoidPoint.z = 0.0f;
	vToAvoidPoint.Normalize();

	if ( m_iSearchFlags & Flag_FindWanderRouteTowardsTarget )
	{
		vToAvoidPoint.Negate();
	}

	Vector3 vVelocityFlatNormalized = m_vStartVelocity;
	vVelocityFlatNormalized.z = 0.0f;
	float fOurSpeed = 0.0f;
	if (vVelocityFlatNormalized.Mag2() > SMALL_FLOAT)
	{
		//there's no safe version of this function, protect against 
		//divide-by-zero
		fOurSpeed = NormalizeAndMag(vVelocityFlatNormalized);
	}

	static dev_float s_fMinSpeedToConsider = 10.0f;
	static dev_float s_fMaxSpeedToConsider = 50.0f;
	float fSpeedCoefficient = 0.0f;
	if (fOurSpeed > s_fMinSpeedToConsider)
	{
		fSpeedCoefficient = (fOurSpeed - s_fMinSpeedToConsider) / (s_fMaxSpeedToConsider - s_fMinSpeedToConsider);
		fSpeedCoefficient = Clamp(fSpeedCoefficient, 0.0f, 1.0f);
	}

	for(int Loop = 0; Loop < currNodeNumLinks; Loop++)
	{
		CNodeAddress testNode = ThePaths.GetNodesLinkedNodeAddr(pCurrNode, Loop);
		if(FromNode == testNode)		// Don't consider the node we were coming from
		{
			continue;
		}

		if(!ThePaths.IsRegionLoaded(testNode))
		{
			continue;
		}

		const CPathNode *pTestNode = ThePaths.FindNodePointer(testNode);
		Vector3	testNodePos;
		pTestNode->GetCoors(testNodePos);

		//if we're not currently off-road, and our vehicle doens't support offroad wandering,
		//don't choose an off-road link
		if ((m_iSearchFlags & Flag_WanderOffroad) == 0 && !pCurrNode->m_1.m_Offroad
			&& pTestNode->m_1.m_Offroad)
		{
			continue;
		}

		const CPathNodeLink& rLink = ThePaths.GetNodesLink(pCurrNode, Loop);

		//if this link is physically intraversable, don't allow fleeing there ever
		if (rLink.m_1.m_LanesToOtherNode < 1 && rLink.m_1.m_bBlockIfNoLanes)
		{
			continue;
		}

		if(!pTestNode->HasSpecialFunction_Driving())
		{
			// The only thing we want to avoid is to go on a switched off node.
			if(pCurrNode->m_2.m_switchedOff 
				|| !pTestNode->m_2.m_switchedOff 
				|| pTestNode->m_2.m_deadEndness <= pCurrNode->m_2.m_deadEndness)
			{
				Vector3	Diff = testNodePos - currNodePos;
				Diff.z = 0.0f;
				Diff.NormalizeFast();

				const Vector3 vCurrToNew = Diff;

				//if the testNode is a junction, and test link is 
				//ignore for nav, use the previous link direction
				if (pTestNode->IsJunctionNode() && rLink.IsDontUseForNavigation() && pPrevLink)
				{
					Diff = vPrevLinkDir;
				}

				const float	fDotWithAvoidPosition = Diff.Dot(vToAvoidPoint);
				float fScore = fDotWithAvoidPosition;

				//if this is a random vehicle, randomize the score a bit
				//but don't let it get too close to the score of a link going
				//directly back toward the target
				if ((m_iSearchFlags & Flag_IsMissionVehicle) == 0)
				{
					//fScore == 1.0 : going back toward the target
					//fScore == -1.0 : going away from the target exactly

					//prevent the current score + random score from ever getting
					//above 0.7
					static dev_float s_fStraightAheadMinScore = 0.6f;
					static dev_float s_fStraightAheadMaxScore = 1.1f;
					if (fDotWithAvoidPosition < -0.7)
					{
						fScore += g_ReplayRand.GetRanged(s_fStraightAheadMinScore, s_fStraightAheadMaxScore);
					}
					else if (fDotWithAvoidPosition < 0.2f)
					{
						fScore += g_ReplayRand.GetRanged(0.0f, 0.1f);
					}
				}
		
				static dev_float s_fNoLaneMultiplier = 1.0f;
				if (rLink.m_1.m_LanesToOtherNode < 1 
					&& pCurrNode->IsJunctionNode()
					&& pTestNode->IsSlipLane())
				{
					//never enter a sliplane in the opposing direction
					fScore += (180.0f * DtoR);
				}
				else if (rLink.m_1.m_LanesToOtherNode < 1)
				{
					//if there are no lanes in our direction, apply a slight penalty
					fScore += fabsf(fScore) * s_fNoLaneMultiplier;
				}

				//if this is a switched off node, apply a slight penalty
				//don't allow it for big vehicles
				static dev_float s_fSwitchedOffNodeMultiplier = 0.5f;
				if (pTestNode->IsSwitchedOff() && !pCurrNode->IsSwitchedOff())
				{
					//We're just not allowed to use switched off nodes at all
					if (CVehicleIntelligence::ms_bReallyPreventSwitchedOffFleeing)
					{
						continue;
					}

					const bool bAllowUseSwitchedOffNodesForFleeing = false;
// 					const bool bAllowUseSwitchedOffNodesForFleeing = 
// 						!bDisallowSwitchedOff && 
// 						(m_iSearchFlags & Flag_AvoidTurns) == 0 && 
// 							((m_iSearchFlags & Flag_AvoidingAdverseConditions) != 0 
// 							|| g_ReplayRand.GetFloat() < 0.5f);
					if (bAllowUseSwitchedOffNodesForFleeing)
					{
						fScore += fabsf(fScore) * s_fSwitchedOffNodeMultiplier;
					}
					else
					{
						fScore += (180.0f * DtoR);
					}
				}

				//if the nodes end up in a dead end, apply a bigger penalty
				static dev_float s_fDeadEndMultiplier = 1.0f;
				if(pTestNode->m_2.m_deadEndness > pCurrNode->m_2.m_deadEndness) 
				{
					//We're just not allowed to use dead end nodes at all
					if (CVehicleIntelligence::ms_bReallyPreventSwitchedOffFleeing)
					{
						continue;
					}

					fScore += fabsf(fScore) * s_fDeadEndMultiplier;
				}

				//If we are trying to avoid turns, apply a penalty to shortcut links
				//we don't want to completely exclude them, but the penalty should be such that
				//we never choose one if there's a non-shortcut link available
				static dev_float s_fShortcutLinkAdditionalScoreForNoTurns = 180.0f * DtoR;
				static dev_float s_fShorcutLinkMultiplier = 0.5f;
				static dev_float s_fShortcutLinkMultiplierAmbient = 90.0f * DtoR;
				if (((m_iSearchFlags & Flag_AvoidTurns) != 0 
						|| (m_iSearchFlags & Flag_AvoidingAdverseConditions) != 0
					)
					&& rLink.IsShortCut())
				{
					fScore += s_fShortcutLinkAdditionalScoreForNoTurns;
				}
				else if (rLink.IsShortCut() && (m_iSearchFlags & Flag_IsMissionVehicle) == 0)
				{
					fScore += s_fShortcutLinkMultiplierAmbient;
					fScore += fabsf(fScore) * s_fShorcutLinkMultiplier;
				}
				else if (rLink.IsShortCut())
				{
					fScore += fabsf(fScore) * s_fShorcutLinkMultiplier;
				}

				
				static dev_float s_fSharpTurnPenalty = 180.0f * DtoR;
				if (pFromNode && pCurrNode->NumLinks() > 2)
				{
					bool bSharpTurn = false;
					float fLeftness = 0.0f, fDotProduct = 0.0f;

					//CNodeAddress FromNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 2);
					const CNodeAddress preFromNode = autopilotNodeIndex > 2 ? m_route.GetPathNodeAddr(autopilotNodeIndex - 3) : EmptyNodeAddress;

					//Find the post-junction link, if any
					
					FindJunctionTurnDirection(preFromNode, FromNode, CurrNode, testNode, &bSharpTurn, &fLeftness, sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct);
					//FindPathDirectionInternal(Vector2(vPrevLinkDir.x, vPrevLinkDir.y), Vector2(vCurrToNew.x, vCurrToNew.y), &bSharpTurn, &fLeftness, sm_fDefaultDotThresholdForRoadStraightAhead, &fDotProduct);

					//vehicles that want to avoid turns should never consider a sharp turn
					//vehicles that are merely avoiding adverse conditions should avoid u-turns
					if ((fDotProduct < -0.25f && (m_iSearchFlags & Flag_AvoidTurns) != 0)
						||
						(fDotProduct < -0.5f && (m_iSearchFlags & Flag_AvoidingAdverseConditions) != 0))
					{
						fScore += s_fSharpTurnPenalty;
					}

					//if we're going above a certain speed, favor links that conserve momentum
	 				if (fOurSpeed > s_fMinSpeedToConsider)
	 				{
	 					//const float fDotWithVelocity = Diff.Dot(vVelocityFlatNormalized);
	 					const float fBaseScoreVelocity = 1.0f - fDotProduct;
	 
	 					static dev_float s_fVelocityMultiplierBase = 0.5f;
	 					const float fFinalScoreVelocity = (fBaseScoreVelocity * s_fVelocityMultiplierBase * fSpeedCoefficient);
	 					fScore += fFinalScoreVelocity;
	 				}
				}
			

				if(fScore < smallestScoreSoFar)
				{
					smallestScoreSoFar = fScore;
					bestNodeSoFar = testNode;
					iLinkTemp = static_cast<s16>(Loop);
				}
			}
		}
	}
	if(!bestNodeSoFar.IsEmpty())
	{
		bHappyWithNode = true;
		m_route.SetPathNodeAddr(autopilotNodeIndex, bestNodeSoFar);
	}
}

CTaskHelperFSM::FSM_Return CPathNodeRouteSearchHelper::GenerateWanderRoute_OnUpdate()
{
	// Start from 2 back from the start of the route
	// If joined from the road system we should have at least 2 nodes
	s32 iStart = m_iCurrentNumberOfNodes - 2;

	if( !aiVerifyf( iStart >= 0, "Car not correctly joined to the road system!") )
	{
		SetState(State_NoRoute);
		return FSM_Continue;
	}

	//the nodes we joined to may have been streamed out
	if (m_iTargetNodeIndex <= 1
		&& (!m_route.GetPathNode(0) || !m_route.GetPathNode(1)))
	{
		// Set some default values as we've failed badly
		m_route.ClearPathNodes();
		m_route.ClearLanes();

		SetState(State_NoRoute);
		return FSM_Continue;
	}

	const bool bPlayerIsOnMission = CTheScripts::GetPlayerIsOnAMission();
	bool bTryAndSteerTowardPlayer = false;
	Vector3 vDirToPlayer(XAXIS);
	const CPed *pPlayer = CGameWorld::FindLocalPlayer();
	TUNE_GROUP_BOOL(NEW_CRUISE_TASK, bAllowVehPopulationSteerTowardPlayer, true);

	// In Sandy shores this can cause a nasty issue with AI swirling round the player area.
	static Vector3 svSandyShores(1783.0f, 3773.0f, 32.0f);
	const bool bSandyShoresInSP = pPlayer && !CNetwork::IsGameInProgress() && (svSandyShores.Dist2(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition())) < rage::square(500.0f));
	if (aiVerify(pPlayer) && bAllowVehPopulationSteerTowardPlayer && !bSandyShoresInSP)
	{
		vDirToPlayer = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()) - m_vSearchStart;
		if (CVehiclePopulation::ms_iNumNodesOnPlayersRoad > 0)
		{
			const CPathNode* pPlayerNode = ThePaths.FindNodePointerSafe(CVehiclePopulation::ms_aNodesOnPlayersRoad[0]);
			if (pPlayerNode)
			{
				Vector3 vPlayerNodeCoors(Vector3::ZeroType);
				pPlayerNode->GetCoors(vPlayerNodeCoors);
				vDirToPlayer = vPlayerNodeCoors - m_vSearchStart;
			}
		}
		vDirToPlayer.z = 0.0f;
		vDirToPlayer.NormalizeSafe(ORIGIN);

		//don't do this if the player's in an interior
		//or we fail a random dice roll
		const float fRandomRoll = g_ReplayRand.GetFloat();
		static dev_float fSteerTowardPlayerThreshold = 0.5f;
		if (CPedPopulation::GetPopulationControlPointInfo().m_locationInfo==LOCATION_EXTERIOR
			&& fRandomRoll > fSteerTowardPlayerThreshold)
		{
			bTryAndSteerTowardPlayer = true;
		}
	}

	//we have some nodes provided for us (likely from vehicle migration)
	//in this case, we loop over the nodes provided to see if any pair matches the first two nodes in our current route
	//if so, we have a match and can copy over the rest of the nodes in the list
	//we also set bUpdateLaneInfo so that our lane information is updated correctly as well
	s32 iLastGoodNode = m_PreferredTargetRoute.FindLastGoodNode();
	if(iLastGoodNode > 0)
	{
// 		for(int i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; ++i)
// 		{
// 			if(!m_route.GetPathNodeAddr(i).IsEmpty())
// 			{
// 				Displayf("Start Route[%d]=%d:%d aLinks=%d aLane=%d", i, m_route.GetPathNodeAddr(i).GetRegion(), m_route.GetPathNodeAddr(i).GetIndex(),
// 					m_route.GetPathLinkIndex(i), m_route.GetPathLaneIndex(i));
// 			}
// 		}
// 
// 		Displayf("Start Route Target ID=%d\n", m_route.GetTargetNodeIndex());

		m_route.Clear();
		for(int i = 0; i <= iLastGoodNode; ++i)
		{
			if(!m_PreferredTargetRoute.GetPathNodeAddr(i).IsEmpty())
			{
				m_route.SetPathNodeAddr(i, m_PreferredTargetRoute.GetPathNodeAddr(i));
				if(m_PreferredTargetRoute.GetPathLinkIndex(i) >= 0)
				{
					m_route.SetPathLinkIndex(i, m_PreferredTargetRoute.GetPathLinkIndex(i));
					m_route.SetPathLaneIndex(i, m_PreferredTargetRoute.GetPathLaneIndex(i));
				}
// 				Displayf("Loading node from preferred list [%d]=%d:%d aLinks=%d aLane=%d", i, m_route.GetPathNodeAddr(i).GetRegion(), m_route.GetPathNodeAddr(i).GetIndex(),
// 									m_route.GetPathLinkIndex(i), m_route.GetPathLaneIndex(i));
			}
		}

// 		for(int i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; ++i)
// 		{
// 			if(!m_route.GetPathNodeAddr(i).IsEmpty())
// 			{
// 				Displayf("After Prefered Route [%d]=%d:%d aLinks=%d aLane=%d", i, m_route.GetPathNodeAddr(i).GetRegion(), m_route.GetPathNodeAddr(i).GetIndex(),
// 					m_route.GetPathLinkIndex(i), m_route.GetPathLaneIndex(i));
// 			}
// 		}
// 
// 		Displayf("Preferred Route Target ID=%d\n", m_route.GetTargetNodeIndex());
	}

	// Add nodes to the route to fill it up, the initial nodes should be set after joining the car to the 
	// road network
	for( s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-2; i++ )
	{
		const s32 iFromNodeIndex		= i;
		const s32 iCurrentNodeIndex		= i+1;
		const s32 iNextNodeIndex		= i+2;
		CNodeAddress FromNode = m_route.GetPathNodeAddr(iFromNodeIndex);
		CNodeAddress CurrNode = m_route.GetPathNodeAddr(iCurrentNodeIndex);
		CNodeAddress NextNode = m_route.GetPathNodeAddr(iNextNodeIndex);

		const CPathNode* pFromOld = ThePaths.FindNodePointerSafe(FromNode);
		const CPathNode* pCurrNode = ThePaths.FindNodePointerSafe(CurrNode);

		if( pFromOld && pCurrNode && NextNode.IsEmpty())
		{
			// In the first attempt we try to pick a link in the direction
			// that we have selected a lane for.
			// Work out(depending on our current lane) whether we should go
			// left, right or straight on(or a combination of those)
			s32 LanesOnCurrentLink = 0;
			bool bComingOutOfOneWayStreet = true;

			s16 iFromLinkIndex = m_route.GetPathLinkIndex(iFromNodeIndex);
			if( iFromLinkIndex >= 0 )
			{
				CPathNodeLink * pFromLink = ThePaths.FindLinkPointerSafe(FromNode.GetRegion(), iFromLinkIndex);
				if(!pFromLink)
				{
					SetState(State_NoRoute);
					return FSM_Continue;
				}
				else
				{
					LanesOnCurrentLink = pFromLink->m_1.m_LanesToOtherNode;
					bComingOutOfOneWayStreet = pFromLink->m_1.m_LanesFromOtherNode == 0;
				}
			}

			// We don't like a new node if:
			// It is the same as our old node
			// It is in a direction we're not in the right lane for
			// It is leading up to a dead end
			// The new node is switched off
			// It would mean going against one-way traffic
			// We're coming out of a one way and we're going into another one way(this causes problems when a 2-way road splits up into 2 1-way roads.
			bool	bHappyWithNode = false;//, bFairlyHappyWithNode = false, bMarginallyHappyWithNode = false;
			s16 iLinkTemp = 0;
			bool bSharpTurn = false;

			if ((m_iSearchFlags & Flag_FindRouteAwayFromTarget) || (m_iSearchFlags & Flag_FindWanderRouteTowardsTarget))
			{
				GetNextNodeAvoidingPoint(iNextNodeIndex, LanesOnCurrentLink, iLinkTemp, bHappyWithNode, bComingOutOfOneWayStreet, 0, pFromOld->m_2.m_leftOnly, pFromOld->m_1.m_cannotGoLeft, pFromOld->m_1.m_cannotGoRight, bPlayerIsOnMission);
			}
			else
			{
				const Vector3 vCurrNodePos3d = pCurrNode->GetPos();
				const bool bSteerTowardPlayerOnThisNode = bTryAndSteerTowardPlayer && !camInterface::IsSphereVisibleInGameViewport(vCurrNodePos3d, 1.0f);
				GetNextNode(iNextNodeIndex, LanesOnCurrentLink, iLinkTemp, bHappyWithNode, bComingOutOfOneWayStreet, 0, pFromOld->m_2.m_leftOnly, pFromOld->m_1.m_cannotGoLeft, pFromOld->m_1.m_cannotGoRight, bSteerTowardPlayerOnThisNode, vDirToPlayer);
			}

			if(!bHappyWithNode)
			{
				//don't u turn if the reason we aren't happy with the node is that
				//the region isn't loaded
				if (!m_route.GetPathNodeAddr(iNextNodeIndex).IsEmpty()
					&& !ThePaths.IsRegionLoaded(m_route.GetPathNodeAddr(iNextNodeIndex)))
				{
					m_route.FindLinksWithNodes();
					m_route.FindLanesWithNodes();

					SetState(State_NoRoute);
					return FSM_Continue;
				}
				else
				{
					m_route.SetPathNodeAddr(iNextNodeIndex, FromNode);

					const bool bFoundLink = ThePaths.FindNodesLinkIndexBetween2Nodes(CurrNode, m_route.GetPathNodeAddr(iNextNodeIndex), iLinkTemp);
					if(bFoundLink == false)
					{
						// Set some default values as we've failed badly
						m_route.ClearPathNodes();
						m_route.ClearLanes();

						SetState(State_NoRoute);
						return FSM_Continue;
					}
				}
			}

			m_route.SetPathLaneIndex(iCurrentNodeIndex, m_route.GetPathLaneIndex(iFromNodeIndex)); // Use previous lane
			const CPathNode *pNode2 = ThePaths.FindNodePointerSafe(m_route.GetPathNodeAddr(iNextNodeIndex));
			if (!pNode2)
			{
				//not sure if this is appropriate, but we found a case where FindNodePointer can fail.
				//this should be far off in the distance enough that it's never noticeable
				m_route.ClearPathNodes();
				m_route.ClearLanes();

				SetState(State_NoRoute);
				return FSM_Continue;
			}

			Vector2 crsNode1, crsNode2;
			pCurrNode->GetCoors2(crsNode1);
			pNode2->GetCoors2(crsNode2);

			const CPathNodeLink *pLink = &ThePaths.GetNodesLink(pCurrNode, iLinkTemp);
			if(!pLink)
			{
				m_route.ClearPathNodes();
				m_route.ClearLanes();

				SetState(State_NoRoute);
				return FSM_Continue;
			}

			s8 LanesToChooseFrom = pLink->m_1.m_LanesToOtherNode;

			// If we are going into a road with a larger number of nodes and it is not a junction we make sure to use all lanes.
			if((LanesToChooseFrom > LanesOnCurrentLink) && !pCurrNode->IsJunctionNode())
			{
				float fraction =(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) + m_route.GetPathLaneIndex(iCurrentNodeIndex)) / rage::Max(1, LanesOnCurrentLink);
				//Assertf(fraction >= 0.0f && fraction <= 1.0f, "fraction %.2f < 0.0f || >1.0f, crsNode1: %.2f, %.2f, crsNode2: %.2f, %.2f"
				//	, fraction, crsNode1.x, crsNode1.y, crsNode2.x, crsNode2.y);
				fraction = Clamp(fraction, 0.0f, 1.0f);
				m_route.SetPathLaneIndex(iCurrentNodeIndex, ((s8)(fraction * LanesToChooseFrom)) );
			}

			// If we are turning and we were in the furthest lane we make sure we still are in the furthest lane.
			// There were problems with cars picking the wrong lane going from a 2 way to a single direction road.
			if(pCurrNode->NumLinks() > 2)		// Only for junctions.
			{
				float	leftness;
				float	fDirectionDot;
				static dev_float thr = 0.92f;
				u32 PathDirection = FindPathDirection(FromNode, CurrNode, m_route.GetPathNodeAddr(iNextNodeIndex), &bSharpTurn, &leftness, thr, &fDirectionDot);

				if(m_route.GetPathLaneIndex(iFromNodeIndex) == LanesOnCurrentLink - 1)		// If we're in the outermost lane(to the right)
				{
					if(PathDirection == BIT_TURN_RIGHT)								// and we're turning right
					{
						s8 iLane = LanesToChooseFrom - 1;
						iLane = rage::Max(iLane, (s8)0);
						m_route.SetPathLaneIndex(iCurrentNodeIndex, iLane);	// go to the far right lane
					}
				}

				if(m_route.GetPathLaneIndex(iFromNodeIndex) == 0)							// If we're in the innermost lane(to the left)
				{
					if(PathDirection == BIT_TURN_LEFT)									// and we're turning left
					{
						m_route.SetPathLaneIndex(iCurrentNodeIndex, 0);		// go to the far left lane
					}
				}

				//and clamp it to our next link's number of nodes regardless
				s8 iCurrentPathLaneIndex = m_route.GetPathLaneIndex(iCurrentNodeIndex);
				//iNextPathLaneIndex = Clamp(iNextPathLaneIndex, (s8)0, (s8)(LanesToChooseFrom-1));
				iCurrentPathLaneIndex = Min(iCurrentPathLaneIndex, (s8)(LanesToChooseFrom-1));
				iCurrentPathLaneIndex = Max(iCurrentPathLaneIndex, (s8)0);
				m_route.SetPathLaneIndex(iCurrentNodeIndex, iCurrentPathLaneIndex);

				//whatever we chose for after the turn, apply that to the junction
				//m_route.SetPathLaneIndex(iCurrentNodeIndex, m_route.GetPathLaneIndex(iNextNodeIndex));
			}

			s8 iLane = m_route.GetPathLaneIndex(iCurrentNodeIndex);
			iLane = rage::Min(iLane, (s8)(LanesToChooseFrom - 1));
			iLane = rage::Max(iLane, (s8)0);

			m_route.SetPathLaneIndex(iCurrentNodeIndex, iLane);
			m_route.SetPathLaneIndex(iNextNodeIndex, iLane);
			m_route.SetPathLinkIndex(iCurrentNodeIndex, (pCurrNode->m_startIndexOfLinks + iLinkTemp));

			ThePaths.RegisterNodeUsage(m_route.GetPathNodeAddr(iNextNodeIndex));

#if __DEV
			m_route.CheckLinks();
#endif
		}
	}

	if (m_route.FindLastGoodNode() < 1)
	{
		m_route.Clear();
		SetState(State_NoRoute);
		return FSM_Continue;
	}

// 	if(m_PreferredTargetRoute.FindLastGoodNode() > 0)
// 	{
// 		for(int i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; ++i)
// 		{
// 			if(!m_route.GetPathNodeAddr(i).IsEmpty())
// 			{
// 				Displayf("Final Route [%d]=%d:%d aLinks=%d aLane=%d", i, m_route.GetPathNodeAddr(i).GetRegion(), m_route.GetPathNodeAddr(i).GetIndex(),
// 					m_route.GetPathLinkIndex(i), m_route.GetPathLaneIndex(i));
// 			}
// 		}
// 
// 		Displayf("Final Route Target ID=%d\n", m_route.GetTargetNodeIndex());
// 	}

	SetState(State_ValidRoute);
	return FSM_Continue;
}

void CPathNodeRouteSearchHelper::FixUpPathForTemplatedJunction(const CVehicle* pVehicle, const u32 iForceDirectionAtJunction)
{
	int iJunctionNodeIndex = m_iTargetNodeIndex;

	if (!aiVerify(pVehicle))
	{
		return;
	}

	bool bJunctionFound = false;
	const u32 iStartIter = m_iTargetNodeIndex == 0 ? 0 : rage::Max((u32)0, m_iTargetNodeIndex - 1);
	for (int i = iStartIter; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
	{
		if (!m_route.GetPathNodeAddr(i).IsEmpty() 
			&& pVehicle->GetIntelligence()->GetJunctionNode() == m_route.GetPathNodeAddr(i))
		{
			iJunctionNodeIndex = i;
			bJunctionFound = true;
			break;
		}
	}

	if (!bJunctionFound)
	{
		//grcDebugDraw::Line(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), Color_green);
		return;
	}

	//sometimes in network games this can get set to something out of bounds.
	//just return early if that's the case.
	if (!aiVerifyf(iJunctionNodeIndex >= 0 && iJunctionNodeIndex < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED, "iJunctionNodeIndex out of bounds: %u", iJunctionNodeIndex))
	{
#if __BANK && 0
		for (s32 i = 0; i < CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED; i++)
		{
			Displayf("aNodes[%d]=%d:%d aLinks[%d]=%d\n", i, m_route.GetPathNodeAddr(i).GetRegion(), m_route.GetPathNodeAddr(i).GetIndex(), i, m_route.GetPathLinkIndex(i));
		}
#endif //__ASSERT
		return;
	}

	//we need a preceding node in order to do anything
	//inside of the below for-loop, we'll try and index into
	//i-2 of the nodelist. iterstart is 1, so we need to make sure
	//iJunctionNodeIndex is 1 or greater before we enter
	if (iJunctionNodeIndex < 1)
	{
		aiWarningf("Somehow iJunctionNodeIndex is %d, returning from FixUpPathForTemplatedJunction early", iJunctionNodeIndex);
		return;
	}

	m_route.ClearFrom(iJunctionNodeIndex+1);

	const int iterStart = 1;
	for (int i = iterStart; i <= 3; i++)
	{
		const int autopilotNodeIndex = iJunctionNodeIndex+i;
		Assert(autopilotNodeIndex >= 0);
		if (autopilotNodeIndex < 0 || autopilotNodeIndex >= CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED)
		{
			break;
		}
		
		CNodeAddress FromNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 2);

		if (!aiVerifyf(!FromNode.IsEmpty() && ThePaths.IsRegionLoaded(FromNode), "FromNode empty or region not loaded: %d:%d", FromNode.GetRegion(), FromNode.GetIndex()))
		{
			break;
		}

		const s16 iLinkIndexToUse = ThePaths.FindRegionLinkIndexBetween2Nodes(FromNode, m_route.GetPathNodeAddr(autopilotNodeIndex-1));
		Assert(iLinkIndexToUse >= 0);

		s16 iLinkTemp=0;
		bool	bHappyWithNode = false;
		s32 LanesOnCurrentLink = ThePaths.FindLinkPointerSafe(FromNode.GetRegion(),iLinkIndexToUse)->m_1.m_LanesToOtherNode;
		bool bComingOutOfOneWayStreet =(ThePaths.FindLinkPointerSafe(FromNode.GetRegion(),iLinkIndexToUse)->m_1.m_LanesFromOtherNode == 0);

		GetNextNode(autopilotNodeIndex, LanesOnCurrentLink, iLinkTemp, bHappyWithNode, bComingOutOfOneWayStreet, iForceDirectionAtJunction, false, false, false, false, XAXIS);

		if(!bHappyWithNode)
		{
			CNodeAddress CurrNode = m_route.GetPathNodeAddr(autopilotNodeIndex - 1);
			m_route.SetPathNodeAddr(autopilotNodeIndex, FromNode);

			//grcDebugDraw::Line(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), Color_red);

			const bool bFoundLink = ThePaths.FindNodesLinkIndexBetween2Nodes(CurrNode, m_route.GetPathNodeAddr(autopilotNodeIndex), iLinkTemp);
			if(bFoundLink == false)
			{
				// Set some default values as we've failed badly
				m_route.Clear();
				m_iTargetNodeIndex = 0;

				SetState(State_NoRoute);
				return;
			}

			break;
		}
	}

	//grcDebugDraw::Line(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), Color_yellow);

	// Update the links and lane addresses
	m_route.FindLinksWithNodes(iJunctionNodeIndex);
	m_route.FindLanesWithNodes(iJunctionNodeIndex);

	//if we made it this far, tell the veh intelligence about it
	pVehicle->GetIntelligence()->SetHasFixedUpPathForCurrentJunction(true);
}

//----------------------------------------------------------------------------------------
// GetSlipLaneNodeLinkIndex: Check for filter lanes into junctions.
// These are characterised by there being 2 other colinear links, one of which is
// one-way and has a single lane.
// We won't want to enter the slip lane, unless we are already in lane 0.
// NB: CPathFind is passed in, because we want this function to be usable from BuildPaths,
// during which we will pass in the CPathFindBuild instance (derived from CPathFind).

rage::s32 CPathNodeRouteSearchHelper::GetSlipLaneNodeLinkIndex( const CPathNode * pCurrNode, const CNodeAddress & FromNode, const CPathFind & pathfind )
{
	if(pCurrNode->NumLinks() > 2)
	{
		Vector3 vFromNode, vLinkedCoords;
		pCurrNode->GetCoors(vFromNode);

		const int iNumLinks = pCurrNode->NumLinks();

		//first, see if any of the lanes look like a sliplane.
		//ie, 1 lane in forward direction, 0 in reverse, and width 0

		int iNumSlipLanes = 0;
#if __ASSERT
		int retVal = -1;
#endif
		for (int i=0; i < iNumLinks; i++)
		{
			const CPathNodeLink& link = pathfind.GetNodesLink(pCurrNode, i);

			//don't test the link we came from
			if (link.m_OtherNode == FromNode)
			{
				continue;
			}

			//don't test shortcut links
			if (link.IsShortCut())
			{
				continue;
			}

			if (link.m_1.m_LanesToOtherNode == 1 && link.m_1.m_LanesFromOtherNode == 0
				&& link.m_1.m_Width == 0)
			{
				const CPathNode* pSlipLinkOtherNode = pathfind.FindNodePointerSafe(link.m_OtherNode);
				Vector3 vSlipNodeCoords(Vector3::ZeroType);

				//this shouldn't happen, but if we don't have another node don't do anything else
				if (!pSlipLinkOtherNode)
				{
					continue;
				}

				Vector3 vSlipLaneDirection(Vector3::ZeroType);
				pSlipLinkOtherNode->GetCoors(vSlipNodeCoords);

				//if the potential sliplink is nonav, use 
				//the direction of the next link
				//but don't look beyond the next junction
				//bool bUseNextLinkDirection = false;
				Vector3 fromToI = vSlipNodeCoords - vFromNode;
				vSlipLaneDirection = fromToI;
				vSlipLaneDirection.z = 0.0f;
				vSlipLaneDirection.NormalizeFast();
				Vector3 vOriginalSlipLaneDirection = vSlipLaneDirection;
				if (link.m_1.m_bDontUseForNavigation && pathfind.FindNumberNonShortcutLinksForNode(pSlipLinkOtherNode) == 2)
				{
					const CPathNodeLink* pLinkAfterNext = NULL;
					for (int k = 0; k < pSlipLinkOtherNode->NumLinks(); k++)
					{
						pLinkAfterNext = &pathfind.GetNodesLink(pSlipLinkOtherNode, k);
						if (pLinkAfterNext->m_OtherNode == pCurrNode->m_address)
						{
							continue;
						}
						else if (pLinkAfterNext->IsShortCut())
						{
							continue;
						}
						else
						{
							break;
						}
					}

					const CPathNode* pNodeAfterNext = pathfind.FindNodePointerSafe(pLinkAfterNext->m_OtherNode);
					Vector3 vCoordsAfterNext(Vector3::ZeroType);

					if (pNodeAfterNext)
					{
						pNodeAfterNext->GetCoors(vCoordsAfterNext);
						vSlipLaneDirection = vCoordsAfterNext - vSlipNodeCoords;
						vSlipLaneDirection.z = 0.0f;
						vSlipLaneDirection.NormalizeFast();
					}
				}

				//this one could be a sliplane. test it for colinearity with each other link
				for (int j = 0; j < iNumLinks; j++)
				{
					//don't compare to ourselves
					if (i==j)
					{
						continue;
					}

					const CPathNodeLink& otherLink = pathfind.GetNodesLink(pCurrNode, j);

					//make sure we skip the node we came from here as well
					if (otherLink.m_OtherNode == FromNode)
					{
						continue;
					}

					if (otherLink.IsShortCut())
					{
						continue;
					}

					//the other link needs to have a positive width
					//or multiple lanes
					//or a lane in the opposite direction
					//but it can't have 0 lanes in the forward direction!
					if (otherLink.m_1.m_LanesToOtherNode == 0 
						|| (otherLink.m_1.m_Width == 0 && otherLink.m_1.m_LanesToOtherNode <= 1
						&& otherLink.m_1.m_LanesFromOtherNode == 0))
					{
						continue;
					}

					const CPathNode* pTestLinkOtherNode = pathfind.FindNodePointerSafe(otherLink.m_OtherNode);
					Vector3 vTestLinkCoords(Vector3::ZeroType);
					if (pTestLinkOtherNode)
					{
						pTestLinkOtherNode->GetCoors(vTestLinkCoords);
					}

					
					Vector3 fromToJ = vTestLinkCoords - vFromNode;
					fromToJ.z = 0.0f;
					fromToJ.NormalizeFast();

					Vector3 fromToJAdjustedForNoNav = fromToJ;

					if (otherLink.m_1.m_bDontUseForNavigation && pTestLinkOtherNode && pTestLinkOtherNode->NumLinks() == 2)
					{
						//work out which link to use.
						//if the link's other node is the one we came from, use the other one
						const CPathNodeLink* pLinkAfterNext = &pathfind.GetNodesLink(pTestLinkOtherNode, 0);
						if (pLinkAfterNext->m_OtherNode == pCurrNode->m_address)
						{
							pLinkAfterNext = &pathfind.GetNodesLink(pTestLinkOtherNode, 1);
						}

						const CPathNode* pNodeAfterNext = pathfind.FindNodePointerSafe(pLinkAfterNext->m_OtherNode);
						Vector3 vCoordsAfterNext(Vector3::ZeroType);

						if (pNodeAfterNext)
						{
							pNodeAfterNext->GetCoors(vCoordsAfterNext);
							fromToJAdjustedForNoNav = vCoordsAfterNext - vTestLinkCoords;
							fromToJAdjustedForNoNav.z = 0.0f;
							fromToJAdjustedForNoNav.NormalizeFast();
						}
					}

					//sliplanes can only go left. rule out any to the right, but add a little tolerance
					//so lines that are fairly close to parallel still get through
					const float fDot = DotProduct(vSlipLaneDirection, fromToJAdjustedForNoNav);
					float fLeftness = fromToJ.x * vOriginalSlipLaneDirection.y - fromToJ.y * vOriginalSlipLaneDirection.x;
					if (fDot >= CPathFind::sm_fSlipLaneDotThreshold && fLeftness > -0.05f)
					{
						//found a sliplane
						++iNumSlipLanes;
						//aiDisplayf("Found a sliplane--node %d, leftness: %.2f", pCurrNode->m_address.GetIndex(), fLeftness);

#if __ASSERT
						// In assert builds, store the first found as the return value, then keep going to catch bad data.
						if(iNumSlipLanes == 1)
						{
							retVal = i;
						}
						break;
#else
						// In non-assert builds, we would always just return the first lane we found, so there is nothing more to do.
						return i;
#endif	// __ASSERT


					}
				}
			}
		}

#if __ASSERT
		if (iNumSlipLanes == 0)
		{
			return -1;
		}
		else if (iNumSlipLanes == 1)
		{
			return retVal;
		}
		else
		{
			Assertf(0, "More than one potential sliplane found at junction (nodeIndex = %d). This sliplane may need to be reworked.", pCurrNode->GetAddrIndex());
			return retVal;
		}
#endif	// __ASSERT
	}

	return -1;
}

rage::s32 CPathNodeRouteSearchHelper::GetHighwayExitLinkIndex(const CPathNode * pCurrNode, const CNodeAddress & FromNode, const CPathFind & pathfind /* = ThePaths */)
{
	if (pCurrNode && pCurrNode->IsHighway() && pCurrNode->FindNumberNonShortcutLinks() == 3)
	{
		const int iNumLinks = pCurrNode->NumLinks();

		//first, see if any of the lanes look like a sliplane.
		//ie, 1 lane in forward direction, 0 in reverse, and width 0

		for (int i=0; i < iNumLinks; i++)
		{
			const CPathNodeLink& link = pathfind.GetNodesLink(pCurrNode, i);

			//don't test the link we came from
			if (link.m_OtherNode == FromNode)
			{
				continue;
			}

			//don't test shortcut links
			if (link.IsShortCut())
			{
				continue;
			}

			//potential exit
			if (link.m_1.m_LanesToOtherNode == 1 && link.m_1.m_LanesFromOtherNode == 0
				&& link.m_1.m_Width == 0)
			{
				const CPathNode* pSlipLinkOtherNode = pathfind.FindNodePointerSafe(link.m_OtherNode);

				//this shouldn't happen, but if we don't have another node don't do anything else
				if (!pSlipLinkOtherNode)
				{
					continue;
				}

				//this one could be a sliplane. test it for colinearity with each other link
				for (int j = 0; j < iNumLinks; j++)
				{
					//don't compare to ourselves
					if (i==j)
					{
						continue;
					}

					const CPathNodeLink& otherLink = pathfind.GetNodesLink(pCurrNode, j);

					//make sure we skip the node we came from here as well
					if (otherLink.m_OtherNode == FromNode)
					{
						continue;
					}

					if (otherLink.IsShortCut())
					{
						continue;
					}

					//the other link needs to have a positive width
					//or multiple lanes
					//or a lane in the opposite direction
					//but it can't have 0 lanes in the forward direction!
					if (otherLink.m_1.m_LanesToOtherNode == 0 
						|| (otherLink.m_1.m_Width == 0 && otherLink.m_1.m_LanesToOtherNode <= 1
						&& otherLink.m_1.m_LanesFromOtherNode == 0))
					{
						continue;
					}

					return i;
				}
			}
		}
	}
	return -1;
}

// 		int iCount=0;
// 		int l=0;
// 		for(l=0; l<iNumLinks; l++)
// 		{
// 			CNodeAddress newNode = ThePaths.GetNodesLinkedNodeAddr(pCurrNode, l);
// 			if(FromNode == newNode)
// 				continue;
// 			CPathNode * pLinkedNode = ThePaths.FindNodePointerSafe(newNode);
// 			if(pLinkedNode)
// 			{
// 				pLinkedNode->GetCoors(vLinkedCoords);
// 				vToLinked[iCount] = vLinkedCoords - vFromNode;
// 				vToLinked[iCount].z = 0.0f;
// 				vToLinked[iCount].Normalize();
// 				links[iCount] = ThePaths.GetNodesLink(pCurrNode, l);
// 				iLinkIndex[iCount] = l;
// 				iCount++;
// 			}
// 		}
// 		if(iCount==2)
// 		{
// 			// See if both lanes are exactly colinear
// 			const float fDot = DotProduct(vToLinked[0], vToLinked[1]);
// 			if(fDot >= 0.9f)
// 			{
// 				for(int i=0; i<2; i++)
// 				{
// 					// Test to see it link 'i' is a slip-lane into a junction filter.
// 					if(links[i].m_1.m_LanesToOtherNode == 1 &&
// 						links[i].m_1.m_LanesFromOtherNode == 0 &&
// 						links[i].m_1.m_Width == 0 &&
// 						links[!i].m_1.m_Width > 0)
// 					{
// 						return iLinkIndex[i];
// 					}
// 				}
// 			}
// 		}
// 	}
// 	return -1;
//}


//--------------------------------------------------------------------------
// FindPathDirection
// Used by wandering cars to classify whether a link is ahead, left or right.
// Also returns some additional info by parameter.

rage::u32 CPathNodeRouteSearchHelper::FindPathDirection( const CNodeAddress & oldNode, const CNodeAddress & currNode, const CNodeAddress & newNode, bool *pSharpTurn, float *pLeftness, const float fDotThreshold, float *pDotProduct )
{
	const CPathNode * pOldNode = ThePaths.FindNodePointerSafe(oldNode);
	if(!pOldNode)
		return 0;
	const CPathNode * pCurrNode = ThePaths.FindNodePointerSafe(currNode);
	if(!pCurrNode)
		return 0;
	const CPathNode * pNewNode = ThePaths.FindNodePointerSafe(newNode);
	if(!pNewNode)
		return 0;

	Vector2 vOld, vCurr, vNew;
	pOldNode->GetCoors2(vOld);
	pCurrNode->GetCoors2(vCurr);
	pNewNode->GetCoors2(vNew);

	Vector2 vOldToCurr = vCurr - vOld;
	Vector2 vCurrToNew = vNew - vCurr;

	return FindPathDirectionInternal(vOldToCurr, vCurrToNew, pSharpTurn, pLeftness, fDotThreshold, pDotProduct);
}

rage::u32 CPathNodeRouteSearchHelper::FindPathDirectionInternal(Vector2 vOldToCurr, Vector2 vCurrToNew, bool *pSharpTurn, float *pLeftness, const float fDotThreshold, float *pDotProduct)
{
	Assert(pSharpTurn);
	*pSharpTurn = false;

	vOldToCurr.Normalize();
	vCurrToNew.Normalize();

	const float fCross = vOldToCurr.x * vCurrToNew.y - vOldToCurr.y * vCurrToNew.x;
	*pLeftness = fCross;

	const float fDotProduct = vOldToCurr.Dot(vCurrToNew);
	*pDotProduct = fDotProduct;

	if(fDotProduct > fDotThreshold)
	{
		return BIT_TURN_STRAIGHT_ON;							// straight on
	}

	static dev_float s_fSharpnessThreshold = -0.3f;
	if(fDotProduct < s_fSharpnessThreshold)										// Sharp turn
	{
		*pSharpTurn = true;
	}
	if(fCross > 0.0f)
	{
		return BIT_TURN_LEFT;									// left ?
	}
	else
	{
		return BIT_TURN_RIGHT;									// right ?
	}
}

//-------------------------------------------------------------------------
// CVehicleFollowRoute
//		Defines a route for a vehicle to follow from a path or navmesh
//		or any other source
// CRoutePoint
//		A single point on that path
//-------------------------------------------------------------------------
dev_float	CVehicleLaneChangeHelper::ms_fChangeLanesVelocityRatio = 1.25f;
u32		CVehicleLaneChangeHelper::ms_TimeBeforeOvertake = 0;
u32		CVehicleLaneChangeHelper::ms_TimeBeforeUndertake = 1000;
dev_float CVehicleLaneChangeHelper::ms_fMinDist2ToLink = 2.0f * 2.0f;

#define ENABLE_LANECHANGE_DEBUGGING __BANK && 0

#if ENABLE_LANECHANGE_DEBUGGING
void HelperDisplayLaneChangeDebugTest(const CVehicle* const pVeh, char* szReason)
{
	if (!pVeh || !pVeh->GetDriver() || !pVeh->GetDriver()->IsPlayer())
	{
		return;
	}

	char szOutputText[80];
	sprintf(szOutputText, "[LaneChange] %d: %s", fwTimer::GetGameTimer().GetTimeInMilliseconds(), szReason);
	aiDisplayf(szOutputText);
}

void HelperDisplayLaneChangeDebugSphere(const CVehicle* const pVeh, const Vector3& pos, Color32 color)
{
	if (!pVeh || !pVeh->GetDriver() || !pVeh->GetDriver()->IsPlayer())
	{
		return;
	}

	grcDebugDraw::Sphere(pos, 0.75f, color, false);
}

void HelperDisplayLaneChangeDebugArrow(const CVehicle* const pVeh, const Vec3V& endPos, Color32 color)
{
	if (!pVeh || !pVeh->GetDriver() || !pVeh->GetDriver()->IsPlayer())
	{
		return;
	}

	grcDebugDraw::Arrow(pVeh->GetVehiclePosition(), endPos, 0.5f, color);
}

#endif //ENABLE_LANECHANGE_DEBUGGING

Vec3V_Out CVehicleFollowRouteHelper::GetVehicleBonnetPosition(const CVehicle* pVehicle, const bool bDriveInReverse, const bool bGetBottomOfCar)
{
	Assert(pVehicle);
	Vec3V vStartCoords = pVehicle->GetVehiclePosition();

	// 	if (!s_bUseBonnetPositionForVehicleRecordings)
	// 	{
	// 		return vStartCoords;
	// 	}

	float fBonnetOffsetScalar;
	if (!bDriveInReverse)
	{
		fBonnetOffsetScalar = pVehicle->GetVehicleModelInfo()->GetBoundingBoxMax().y;
	}
	else
	{
		fBonnetOffsetScalar = pVehicle->GetVehicleModelInfo()->GetBoundingBoxMin().y;
	}

	// Use the bonnet of the car as the leading point for working out the direction to head in
	Vec3V vBonnetOffset = pVehicle->GetVehicleForwardDirection() * ScalarV(fBonnetOffsetScalar);

	if (bGetBottomOfCar)
	{
		//bounding box min z should be negative, so multiply by the positive up direction
		vBonnetOffset += pVehicle->GetVehicleUpDirection() * ScalarV(pVehicle->GetVehicleModelInfo()->GetBoundingBoxMin().z);
	}

	return vStartCoords + vBonnetOffset;
}

//use a position inside of the car, so if it gets smashed, we won't try to generate navmesh
//queries using a position that may be out in front of the car.
//when we smash into a wall, this can end up resolving to the other side
Vec3V_Out CVehicleFollowRouteHelper::GetVehicleBonnetPositionForNavmeshQueries(const CVehicle* pVehicle, const bool bDriveInReverse)
{
	Assert(pVehicle);
	Vec3V vStartCoords = pVehicle->GetVehiclePosition();

	// 	if (!s_bUseBonnetPositionForVehicleRecordings)
	// 	{
	// 		return vStartCoords;
	// 	}

	float fBonnetOffsetScalar;
	if (!bDriveInReverse)
	{
		fBonnetOffsetScalar = pVehicle->GetVehicleModelInfo()->GetBoundingBoxMax().y * 0.33f;
	}
	else
	{
		fBonnetOffsetScalar = pVehicle->GetVehicleModelInfo()->GetBoundingBoxMin().y * 0.33f;
	}

	// Use the bonnet of the car as the leading point for working out the direction to head in
	const Vec3V vBonnetOffset = pVehicle->GetVehicleForwardDirection() * ScalarV(fBonnetOffsetScalar);

	return vStartCoords + vBonnetOffset;
}

Vec3V_Out CVehicleFollowRouteHelper::GetVehicleDriveDir(const CVehicle* pVehicle, const bool bDriveInReverse)
{
	Assert(pVehicle);

#if __ASSERT
	const ScalarV scMagSqr = MagSquared(pVehicle->GetVehicleForwardDirection());
	const ScalarV scEpsilon = ScalarV(V_FLT_SMALL_2);
	Assertf(IsCloseAll(scMagSqr, ScalarV(V_ONE), scEpsilon), "MagSqr: %.4f", scMagSqr.Getf());
#endif //__ASSERT

	return !bDriveInReverse ? pVehicle->GetVehicleForwardDirection() : -pVehicle->GetVehicleForwardDirection();
}

CVehicleFollowRouteHelper::CVehicleFollowRouteHelper()
: m_iNumPoints(0)
, m_iCurrentTargetPoint(0)
, m_fCurrentLane(0.0f)
, m_PathWaypointDataInitialized(false)
, m_FirstValidElementOfPathWaypointsAndSpeed(-1)
, m_TargetPointForPathWaypoints(-1)
, m_JunctionStringPullDistAdd(false)
, m_MinPossibleNodeSpeedOnPath(0.0f)
, m_GotStraightLineTargetPos(false)
, m_PathLengthSum(0.0f)
, m_PathLengthSumFirstElement(0)
, m_StraightLineTargetPos(V_ZERO)
, m_CurrentLaneA(0.0f)
, m_CurrentLaneB(0.0f)
, m_CurrentLaneC(0.0f)
, m_GotCurrentLane(false)
, m_bAllowUseForPhysicalConstraint(true)
, m_bDrivingInReverse(false)
{
	Assert(&sysMemAllocator::GetCurrent() == &sysMemAllocator::GetMaster());
	m_vPosWhenLastConstructedNodeList.ZeroComponents();
	m_nTimeLastGivenLaneSlack = 0;
	m_vRandomNodeOffset.ZeroComponents();
	m_bCurrentRouteContainsLaneChange = false;

	ResetDebugMaintainLaneForNodes();
}

CVehicleFollowRouteHelper::CVehicleFollowRouteHelper(const CVehicleFollowRouteHelper& otherFollowRoute)
: m_iNumPoints(otherFollowRoute.m_iNumPoints)
, m_iCurrentTargetPoint(otherFollowRoute.m_iCurrentTargetPoint)
, m_fCurrentLane(otherFollowRoute.m_fCurrentLane)
, m_PathWaypointDataInitialized(false)
, m_bAllowUseForPhysicalConstraint(otherFollowRoute.m_bAllowUseForPhysicalConstraint)
, m_bDrivingInReverse(otherFollowRoute.m_bDrivingInReverse)
, m_bCurrentRouteContainsLaneChange(otherFollowRoute.m_bCurrentRouteContainsLaneChange)
, m_vPosWhenLastConstructedNodeList(otherFollowRoute.m_vPosWhenLastConstructedNodeList)
, m_nTimeLastGivenLaneSlack(otherFollowRoute.m_nTimeLastGivenLaneSlack)
, m_vRandomNodeOffset(otherFollowRoute.m_vRandomNodeOffset)
, m_GotStraightLineTargetPos(otherFollowRoute.m_GotStraightLineTargetPos)
, m_JunctionStringPullDistAdd(otherFollowRoute.m_JunctionStringPullDistAdd)
, m_JunctionStringPullDistAddPoint1(otherFollowRoute.m_JunctionStringPullDistAddPoint1)
, m_JunctionStringPullDistAddPoint2(otherFollowRoute.m_JunctionStringPullDistAddPoint2)
, m_StraightLineTargetPos(otherFollowRoute.m_StraightLineTargetPos)
{
	ResetPrecalcData();

	Assert(&sysMemAllocator::GetCurrent() == &sysMemAllocator::GetMaster());
	for (int i = 0; i < MAX_ROUTE_SIZE; i++)
	{
		//m_RoutePoints stuff
		m_RoutePoints[i] = otherFollowRoute.m_RoutePoints[i];
	}
}

CVehicleFollowRouteHelper& CVehicleFollowRouteHelper::operator=(const CVehicleFollowRouteHelper& otherFollowRoute)
{
	m_iNumPoints = (otherFollowRoute.m_iNumPoints);
	m_iCurrentTargetPoint = (otherFollowRoute.m_iCurrentTargetPoint);
	m_fCurrentLane = (otherFollowRoute.m_fCurrentLane);
	m_bAllowUseForPhysicalConstraint = otherFollowRoute.m_bAllowUseForPhysicalConstraint;
	m_bDrivingInReverse = (otherFollowRoute.m_bDrivingInReverse);
	m_bCurrentRouteContainsLaneChange = (otherFollowRoute.m_bCurrentRouteContainsLaneChange);
	m_vPosWhenLastConstructedNodeList = (otherFollowRoute.m_vPosWhenLastConstructedNodeList);
	m_nTimeLastGivenLaneSlack = (otherFollowRoute.m_nTimeLastGivenLaneSlack);
	m_vRandomNodeOffset = (otherFollowRoute.m_vRandomNodeOffset);
	m_JunctionStringPullDistAdd = (otherFollowRoute.m_JunctionStringPullDistAdd);
	m_JunctionStringPullDistAddPoint1 = (otherFollowRoute.m_JunctionStringPullDistAddPoint1);
	m_JunctionStringPullDistAddPoint2 = (otherFollowRoute.m_JunctionStringPullDistAddPoint2);

	ResetPrecalcData();

	for (int i = 0; i < MAX_ROUTE_SIZE; i++)
	{
		//m_RoutePoints stuff
		m_RoutePoints[i] = otherFollowRoute.m_RoutePoints[i];
	}

	return *this;
}

CVehicleFollowRouteHelper::~CVehicleFollowRouteHelper()
{
}

CVehicleLaneChangeHelper::CVehicleLaneChangeHelper()
{
	//m_bShouldChangeLanesForTraffic = false;
	m_bWaitingToOvertakeThisFrame = false;
	m_bWaitingToUndertakeThisFrame = false;
	//m_NextTimeAllowedToChangeLanesForTraffic = 0;
	m_TimeWaitingForOvertake = 0;
	m_TimeWaitingForUndertake = 0;
	m_pEntityToChangeLanesFor = NULL;
	m_bForceUpdate = false;
}

CVehicleLaneChangeHelper::~CVehicleLaneChangeHelper()
{

}

void CVehicleLaneChangeHelper::ResetFrameVariables()
{
	m_bWaitingToOvertakeThisFrame = false;
	m_bWaitingToUndertakeThisFrame = false;
}

void CVehicleLaneChangeHelper::ClearEntityToChangeLanesFor()
{
	m_pEntityToChangeLanesFor = NULL;
}

void CVehicleLaneChangeHelper::DecayLaneChangeTimers(const CVehicle* const /*pVeh*/, u32 timeStepInMs)
{
	//if we didn't increment the timer inside of one of the avoidance functions, decay them here
	if (!m_bWaitingToOvertakeThisFrame)
	{
		m_TimeWaitingForOvertake = rage::Max(0, (int)m_TimeWaitingForOvertake - (int)timeStepInMs);
	}
	if (!m_bWaitingToUndertakeThisFrame)
	{
		m_TimeWaitingForUndertake = rage::Max(0, (int)m_TimeWaitingForUndertake - (int)timeStepInMs);
	}

//  	if (pVeh->GetDriver() && pVeh->GetDriver()->IsPlayer())
//  	{
//  		char buffOvertake[32];
//  		char buffUndertake[32];
//  
//  		sprintf(buffOvertake, "Overtake: %d", m_TimeWaitingForOvertake);
//  		sprintf(buffUndertake, "Undertake: %d", m_TimeWaitingForUndertake);
//  
//  		CVehicle::ms_debugDraw.AddText(pVeh->GetVehiclePosition(), 0, 8, buffOvertake, Color_white);
//  		CVehicle::ms_debugDraw.AddText(pVeh->GetVehiclePosition(), 0, 0, buffUndertake, Color_white);
//  	}
}

bool CVehicleLaneChangeHelper::FindEntityToChangeLanesFor(const CVehicle* const pVehicle, const CVehicleFollowRouteHelper* const pFollowRoute, const float fDesiredSpeed, const bool bDriveInReverse, CPhysical*& pObstructingEntityOut, float& fDistToObstructionOut, int& iObstructedSegmentOut)
{
	//if pOnlyTestForThisEntity is non-NULL, we only want to test it.
	//otherwise we'll search for an entity to test and use it.
	//the return param of this function simply tells us if we found a new vehicle or not
	//--it can never be true if pOnlyTestForThisEntity is non-NULL
	//if this function finds an intersection up ahead, it sets m_pEntityToChangeLanesFor and pOnlyTestForThisEntity
	//if not, it clears them

	iObstructedSegmentOut = -1;
	fDistToObstructionOut = FLT_MAX;

	bool bFoundNewObstacle = false;

	bool bIsAChildAttachment = false;
	//exclude shit we're attached to
	if (pObstructingEntityOut && pObstructingEntityOut->GetAttachParent())
	{
		if (pObstructingEntityOut->GetAttachParent() == pVehicle)
		{
			bIsAChildAttachment = true;
		}

		//exclude shit attached to shit we're attached to
		if (pObstructingEntityOut->GetAttachParent()->GetAttachParent()
			&& pObstructingEntityOut->GetAttachParent()->GetAttachParent() == pVehicle)
		{		
			bIsAChildAttachment = true;
		}	
	}

	//now test along our path ahead for this guy
	//if we don't hit him, clear out m_pEntityToChangeLanesFor and pObstructingEntityOut
	//if we didn't get an entity passed in
	if (pObstructingEntityOut)
	{
		if (pFollowRoute->GetCurrentTargetPoint() > 0 && !bIsAChildAttachment && ShouldChangeLanesForThisEntity(pVehicle, pObstructingEntityOut, fDesiredSpeed, bDriveInReverse))
		{
			const Vec3V vehDriveDirV = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVehicle, bDriveInReverse);

			//spherical tests for peds, bounding box for others
			const bool bUseSphericalTest = pObstructingEntityOut->GetIsTypePed();

			for (int i = pFollowRoute->GetCurrentTargetPoint(); i < pFollowRoute->GetNumPoints(); i++)
			{
				//test this segment for collision
				//this is copied from IsSegmentObstructedNew, but 
				//it was a little too integrated for me to easily refactor and use code in both places
				const CRoutePoint* pPoint = &pFollowRoute->GetRoutePoints()[i];
				const CRoutePoint* pLastPoint = &pFollowRoute->GetRoutePoints()[i-1];

				float laneCentreOffset = pLastPoint->m_fCentralLaneWidth + rage::round(pLastPoint->GetLane().Getf()) * pLastPoint->m_fLaneWidth;

				Vector2 side = Vector2(pPoint->GetPosition().GetYf() -  pLastPoint->GetPosition().GetYf(),  pLastPoint->GetPosition().GetXf() - pPoint->GetPosition().GetXf());
				side.Normalize();
				Vector3 laneCentreOffsetVec((side.x * laneCentreOffset),(side.y * laneCentreOffset), 0.0f);

				Vector3 vStartPos = VEC3V_TO_VECTOR3(pLastPoint->GetPosition()) + laneCentreOffsetVec;
				Vector3 vEndPos = VEC3V_TO_VECTOR3(pPoint->GetPosition()) + laneCentreOffsetVec;

				//raise this a little, so we don't go underneath any bounding boxes
				vStartPos.z += 0.25f;
				vEndPos.z += 0.25f;
				Vector3 vSegment = vEndPos - vStartPos;

				if (bUseSphericalTest)
				{
					//flatten
					Vector3 vPedPos = VEC3V_TO_VECTOR3(pObstructingEntityOut->GetTransform().GetPosition());
					vPedPos.z = vStartPos.z;

					const float fDebugDist2ToLink = geomDistances::Distance2SegToPoint(vStartPos, vSegment, vPedPos);
					if ( fDebugDist2ToLink < ms_fMinDist2ToLink )
					{
						//project pVeh -> pObstructingEntityOut onto pVeh.Forward
						fDistToObstructionOut = Dot(pObstructingEntityOut->GetTransform().GetPosition() - pVehicle->GetVehiclePosition(), vehDriveDirV).Getf();
						iObstructedSegmentOut = i;

						Assert(fDistToObstructionOut < LARGE_FLOAT);

						break;
					}
				}
				else
				{
					Vector3 vBoxMin = pObstructingEntityOut->GetBoundingBoxMin();
					Vector3 vBoxMax = pObstructingEntityOut->GetBoundingBoxMax();	

					//enfatten the boxen
					vBoxMax += Vector3(0.25f, 0.5f, 0.25f);
					vBoxMin += Vector3(-0.25f, -0.5f, -0.25f);

					//transform the segment into car space
					Vector3 vStartPosLocal = VEC3V_TO_VECTOR3(pObstructingEntityOut->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vStartPos)));
					Vector3 vSegmentLocal = VEC3V_TO_VECTOR3(pObstructingEntityOut->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vSegment)));

					if (geomBoxes::TestSegmentToBoxMinMax(vStartPosLocal, vStartPosLocal + vSegmentLocal, vBoxMin, vBoxMax))
					{
						//project pVeh -> pVehObstacle onto pVeh.Forward
						fDistToObstructionOut = Dot(pObstructingEntityOut->GetTransform().GetPosition() - pVehicle->GetVehiclePosition(), vehDriveDirV).Getf();
						iObstructedSegmentOut = i;

						Assert(fDistToObstructionOut < LARGE_FLOAT);
						break;
					}
				}
			}

			//if we didn't find this entity during the re-test, clear out hte pointer
			//this will cause us to reset everything later and return
			if (iObstructedSegmentOut < 0)
			{
				pObstructingEntityOut = NULL;
			}
		}
		else
		{
			pObstructingEntityOut = NULL;
		}
	}
	else
	{
		for (int i = pFollowRoute->GetCurrentTargetPoint(); i < pFollowRoute->GetNumPoints(); i++)
		{
			float fObstructingEntitySpeed = 0.0f;
			//this tests all entities and is expensive--hopefully we don't have to do it too often
			const ScalarV vLaneToTest = pFollowRoute->GetRoutePoints()[i].GetLane();
			const int iLaneToTest = rage::round(vLaneToTest.Getf());
			if (IsSegmentObstructed(i, pFollowRoute, pVehicle, iLaneToTest, fDesiredSpeed, fDistToObstructionOut, pObstructingEntityOut, fObstructingEntitySpeed, bDriveInReverse))
			{
				//only consider this a valid lane change if the vehicle is in front of us.
				//this isn't checked in IsSegmentObstructed since that's also called when looking backward
				//in adjacent lanes for fast moving vehicles that might block our turn
				if (fDistToObstructionOut > 0.0f)
				{
					iObstructedSegmentOut = i;
					break;
				}
				else
				{
					pObstructingEntityOut = NULL;
					fDistToObstructionOut = FLT_MAX;
				}
			}
		}
	} 

	if (!pObstructingEntityOut)
	{
		//didn't find anything, reset and return
		m_pEntityToChangeLanesFor = NULL;
		fDistToObstructionOut = FLT_MAX;
		iObstructedSegmentOut = -1;
		return bFoundNewObstacle;
	}

#if ENABLE_LANECHANGE_DEBUGGING
	char buff[64];
	sprintf(buff, "Found Entity To Change Lanes For: %p at distance %.2f, segment %d", pObstructingEntityOut, fDistToObstructionOut, iObstructedSegmentOut);
	HelperDisplayLaneChangeDebugTest(pVehicle, buff);
	if (pFollowRoute->CurrentRouteContainsLaneChange())
	{
		char buff2[64];
		sprintf(buff2, "Was already changing lanes for %p", m_pEntityToChangeLanesFor.Get());
		HelperDisplayLaneChangeDebugTest(pVehicle, buff2);
	}
	
#endif //ENABLE_LANECHANGE_DEBUGGING
	
	m_pEntityToChangeLanesFor = pObstructingEntityOut;
	return bFoundNewObstacle;
}

bool CVehicleConvertibleRoofHelper::IsRoofDoingAnimatedTransition(const CVehicle* pVehicle) const
{
	const int iRoofState = pVehicle->GetConvertibleRoofState();
	const bool bInstantRoofTransition = pVehicle->IsDoingInstantRoofTransition();
	if (!bInstantRoofTransition && (iRoofState == CTaskVehicleConvertibleRoof::STATE_LOWERING
		|| iRoofState == CTaskVehicleConvertibleRoof::STATE_RAISING
		|| iRoofState == CTaskVehicleConvertibleRoof::STATE_CLOSING_BOOT))
	{
		return true;
	}

	return false;
}

u32 CVehicleConvertibleRoofHelper::ms_uLastRoofChangeTime = 0;
static dev_u32 suMinTimeBetweenConversions = 5000;

void CVehicleConvertibleRoofHelper::PotentiallyRaiseOrLowerRoof(CVehicle* pVehicle, const bool bStoppedAtRedLight) const
{
	//don't start if we're moving
	if (pVehicle->GetAiXYSpeed() > 0.25f)
	{
		return;
	}

	if (!pVehicle->DoesVehicleHaveAConvertibleRoofAnimation())
	{
		return;
	}

	if (!bStoppedAtRedLight)
	{
		return;
	}

	TUNE_GROUP_BOOL(CAR_AI, EnableConvertibleRoofRaiseLower, true);
	if (EnableConvertibleRoofRaiseLower)
	{
		u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
		if((uCurrentTime - ms_uLastRoofChangeTime) > suMinTimeBetweenConversions)
		{
			//should the roof be up or down?
			const bool bShouldRoofBeDown = ShouldRoofBeDown(pVehicle);
			if (bShouldRoofBeDown && IsRoofFullyRaised(pVehicle))
			{
				LowerRoof(pVehicle, false);
				ms_uLastRoofChangeTime = uCurrentTime;
			}
			else if (!bShouldRoofBeDown && IsRoofFullyLowered(pVehicle))
			{
				RaiseRoof(pVehicle, false);
				ms_uLastRoofChangeTime = uCurrentTime;
			}
		}
	}
}

void CVehicleConvertibleRoofHelper::LowerRoof(CVehicle* pVehicle, bool bImmediate) const
{
	pVehicle->LowerConvertibleRoof(bImmediate);
}

void CVehicleConvertibleRoofHelper::RaiseRoof(CVehicle* pVehicle, bool bImmediate) const
{
	pVehicle->RaiseConvertibleRoof(bImmediate);
}

bool CVehicleConvertibleRoofHelper::IsRoofFullyLowered(CVehicle* pVehicle) const
{
	return pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_LOWERED;
}

bool CVehicleConvertibleRoofHelper::IsRoofFullyRaised(CVehicle* pVehicle) const
{
	return pVehicle->GetConvertibleRoofState() == CTaskVehicleConvertibleRoof::STATE_RAISED;
}

bool CVehicleConvertibleRoofHelper::ShouldRoofBeDown(CVehicle* pVehicle) const
{
	return pVehicle->ShouldLowerConvertibleRoof();
}

__forceinline Vec3V_Out FlattenXY(Vec3V_In vec)
{
	Vec3V out = vec;
	out.SetZ(ScalarV(V_ZERO));
	return out;
}

void CVehicleFollowRouteHelper::SetCurrentTargetPoint(s32 iCurrentTargetPoint)
{
	Assertf(iCurrentTargetPoint >=0 && iCurrentTargetPoint < m_iNumPoints, "Desired target point %d out of bounds. This might crash later.", iCurrentTargetPoint);
	m_iCurrentTargetPoint = (s16)iCurrentTargetPoint;
}

void CVehicleFollowRouteHelper::ConstructFromNodeList( const CVehicle* pVehicle, CVehicleNodeList& nodeList, const bool bUpdateLanesForTurns, CVehicleLaneChangeHelper* pLaneChangeHelper, const bool bUseStringPulling)
{
	m_bAllowUseForPhysicalConstraint = true;

	const int iNumPathNodesInList = (s16)nodeList.FindNumPathNodes();
	m_iNumPoints = 0;

	if (iNumPathNodesInList < 1)
	{
		ResetCurrentTargetPoint();
		m_bCurrentRouteContainsLaneChange = false;	
		return;
	}

	//if we have an entity to change lanes for,
	//set a required lane index to avoid it
	if (m_bCurrentRouteContainsLaneChange && pLaneChangeHelper && pLaneChangeHelper->GetEntityToChangeLanesFor())
	{
		 //should we just force an update here, or defer it until the next frame?
		//leaning towards defer
		pLaneChangeHelper->SetForceUpdate(true);

#if ENABLE_LANECHANGE_DEBUGGING
		//grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 0.75f, Color_orange, false);
		HelperDisplayLaneChangeDebugTest(pVehicle, "Re-planned in the middle of lane change, forcing update!");
		HelperDisplayLaneChangeDebugSphere(pVehicle, VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()), Color_orange);
#endif //ENABLE_LANECHANGE_DEBUGGING
	}
#if ENABLE_LANECHANGE_DEBUGGING
	else
	{	
		//grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 0.75f, Color_green, false);
		HelperDisplayLaneChangeDebugSphere(pVehicle, VEC3V_TO_VECTOR3(pVehicle->GetVehiclePosition()), Color_yellow);
	}
#endif //ENABLE_LANECHANGE_DEBUGGING

	//even if we forced an update just above, we'll let
	//the lane change helper set this to true when it 
	//actually builds the route
	m_bCurrentRouteContainsLaneChange = false;	

	bool bSetRandomOffset = false;

	for( s32 i = 0; i < iNumPathNodesInList; i++ )
	{
		// This will eventually be set in situations where a particular lane is required.
		m_RoutePoints[i].m_iRequiredLaneIndex = -1; 

		const bool bLastIteration = i == iNumPathNodesInList-1;

		CNodeAddress currentTargetNode = nodeList.GetPathNodeAddr(i);
		CNodeAddress previousTargetNode = i > 0 ? nodeList.GetPathNodeAddr(i - 1) : EmptyNodeAddress;
		CNodeAddress NextTargetNode = bLastIteration ? EmptyNodeAddress : nodeList.GetPathNodeAddr(i+1);
		const CPathNode *pCurrentNode = ThePaths.FindNodePointerSafe(currentTargetNode);
		const CPathNode *pPreviousNode = i > 0 ? ThePaths.FindNodePointerSafe(previousTargetNode) : NULL;
		const CPathNode *pNextNode = bLastIteration ? NULL : ThePaths.FindNodePointerSafe(NextTargetNode);

		if(!pCurrentNode /*|| !pNextNode && !bLastIteration*/ )
		{
			break;
		}
		const CPathNodeLink *pLink = NULL;
		
		if (bLastIteration && i > 0)
		{
			if (nodeList.GetPathLinkIndex(i - 1) >= 0)
			{
				pLink = ThePaths.FindLinkPointerSafe(nodeList.GetPathNodeAddr(i-1).GetRegion(),nodeList.GetPathLinkIndex(i-1));
			}
		}
		else if (nodeList.GetPathLinkIndex(i) >= 0)
		{
			pLink = ThePaths.FindLinkPointerSafe(currentTargetNode.GetRegion(),nodeList.GetPathLinkIndex(i));
		}

		aiAssert(pCurrentNode);
		//aiAssert(pLink);
		//aiAssert(pNextNode || bLastIteration);

		CPathNodeLink* pPrevLink = NULL;
		if (i > 0 && nodeList.GetPathLinkIndex(i-1) >= 0)
		{
			CNodeAddress prevNode = nodeList.GetPathNodeAddr(i-1);
			pPrevLink = ThePaths.FindLinkPointerSafe(prevNode.GetRegion(),nodeList.GetPathLinkIndex(i-1));
		}

		//If this is a junction node, and the previous node has a required lane, apply that to the junction as well
		//in order to prevent mid-junction lane changes
		if (pCurrentNode->IsJunctionNode() && i > 0 && m_RoutePoints[i-1].m_iRequiredLaneIndex > -1)
		{
			m_RoutePoints[i].m_iRequiredLaneIndex = nodeList.GetPathLaneIndex(i);
		}

		if (pVehicle->PopTypeGet() == POPTYPE_RANDOM_AMBIENT && !bUpdateLanesForTurns && pPreviousNode && pPreviousNode->IsJunctionNode())
		{
			CJunction* pJunction = CJunctions::FindJunctionFromNode(previousTargetNode);

			if (pJunction && pJunction->HasFlowControlNodes())
			{
				m_RoutePoints[i].m_iRequiredLaneIndex = nodeList.GetPathLaneIndex(i);
			}
		}

		Vector3 thisNodeCoors;
		Vector3 nextNodeCoors;
		pCurrentNode->GetCoors(thisNodeCoors);

		if (pNextNode)
		{
			pNextNode->GetCoors(nextNodeCoors);
		}

		if (!bSetRandomOffset)
		{
			Vector3 vRandomNodeOffset;
			CVehicleIntelligence::FindNodeOffsetForCar(pVehicle, vRandomNodeOffset);

			if (pLink && pLink->m_1.m_NarrowRoad)
			{
				vRandomNodeOffset *= 0.25f;
			}

			if (pLink && pNextNode && pLink->IsSingleTrack())
			{
				//maybe negate our random offset
				const Vector3 vDir = nextNodeCoors - thisNodeCoors;
				const Vector3 vRight(vDir.y, -vDir.x, 0.0f);
				if (vRandomNodeOffset.Dot(vRight) < 0.0f)
				{
					vRandomNodeOffset.Negate();
				}

				//allow greater deviation from the centre
				if (!pLink->m_1.m_NarrowRoad)
				{
					vRandomNodeOffset *= 1.5f;
				}
			}

			m_vRandomNodeOffset = VECTOR3_TO_VEC3V(vRandomNodeOffset);
			bSetRandomOffset = true;
		}

		//if we get this far, we're definitely adding the point
		++m_iNumPoints;

		if (pLink)
		{
			m_RoutePoints[i].m_fCentralLaneWidth = pLink->InitialLaneCenterOffset();
			m_RoutePoints[i].m_vPositionAndSpeed = Vec4V(VECTOR3_TO_VEC3V(thisNodeCoors), ScalarV(V_TEN));
			m_RoutePoints[i].m_iNoLanesThisSide = pLink->m_1.m_LanesToOtherNode;
			m_RoutePoints[i].m_iNoLanesOtherSide = pLink->m_1.m_LanesFromOtherNode;
			m_RoutePoints[i].m_fLaneWidth = pLink->GetLaneWidth();
			m_RoutePoints[i].m_iCamberThisSide = pLink->m_1.m_Tilt;
			m_RoutePoints[i].m_iCamberFalloffThisSide = pLink->m_1.m_TiltFalloff;
			m_RoutePoints[i].m_bIsSingleTrack = pLink->IsSingleTrack();
			m_RoutePoints[i].m_bIgnoreForNavigation = pLink->m_1.m_bDontUseForNavigation;
			m_RoutePoints[i].m_bJunctionNode = pCurrentNode->IsJunctionNode();
			m_RoutePoints[i].m_bHasLeftOnlyLane =  pCurrentNode->m_2.m_leftOnly;
			m_RoutePoints[i].m_bIsHighway = pCurrentNode->IsHighway();
			m_RoutePoints[i].m_bIsSlipLane = pCurrentNode->IsSlipLane();
			m_RoutePoints[i].m_bIsSlipJunction = pCurrentNode->m_2.m_slipJunction;

			// Camber on the other side
			// Start off with just the negation of camber on this side in case we don't find the opposing link below.
			m_RoutePoints[i].m_iCamberOtherSide = (s8)CPathFind::GetNegativeTiltIndex(m_RoutePoints[i].m_iCamberThisSide);
			m_RoutePoints[i].m_iCamberFalloffOtherSide = false;
			// Find a link in the other direction and get the camber value from there
			if(pCurrentNode && pNextNode)
			{
				s16 iReverseLinkInNode;
				if(ThePaths.FindNodesLinkIndexBetween2Nodes(pNextNode,pCurrentNode,iReverseLinkInNode))
				{
					int iReverseLink = pNextNode->m_startIndexOfLinks + iReverseLinkInNode;
					const CPathNodeLink & pReverseLink = *ThePaths.FindLinkPointerSafe(NextTargetNode.GetRegion(),iReverseLink);
					m_RoutePoints[i].m_iCamberOtherSide = pReverseLink.m_1.m_Tilt;
					m_RoutePoints[i].m_iCamberFalloffOtherSide = pReverseLink.m_1.m_TiltFalloff;
				}
			}
		}
		else
		{
			m_RoutePoints[i].m_fCentralLaneWidth = 0.0f;
			m_RoutePoints[i].m_vPositionAndSpeed = Vec4V(VECTOR3_TO_VEC3V(thisNodeCoors), ScalarV(V_TEN));
			m_RoutePoints[i].m_iNoLanesThisSide = 1;
			m_RoutePoints[i].m_iNoLanesOtherSide = 0;
			m_RoutePoints[i].m_fLaneWidth = LANEWIDTH;
			m_RoutePoints[i].m_iCamberThisSide = 0;
			m_RoutePoints[i].m_iCamberOtherSide = 0;
			m_RoutePoints[i].m_iCamberFalloffThisSide = 0;
			m_RoutePoints[i].m_iCamberFalloffOtherSide = 0;
			m_RoutePoints[i].m_bIsSingleTrack = false;
			m_RoutePoints[i].m_bIgnoreForNavigation = false;
			m_RoutePoints[i].m_bJunctionNode = pCurrentNode->IsJunctionNode();
			m_RoutePoints[i].m_bHasLeftOnlyLane = false;
			m_RoutePoints[i].m_bIsHighway = false;
			m_RoutePoints[i].m_bIsSlipLane = false;
			m_RoutePoints[i].m_bIsSlipJunction = false;
		}

#if __DEV
		if (i > 0)
		{
			if (!aiVerifyf((DistXY(m_RoutePoints[i-1].GetPosition(), m_RoutePoints[i].GetPosition()) > ScalarV(V_FLT_SMALL_4)).Getb(), "Trying to add a duplicate point %d to followRoute from Nodelist route", i))
			{
				const int iLastGoodNode = nodeList.FindLastGoodNode();
				for (int j = 0; j < iLastGoodNode; j++)
				{
					const CPathNode* pDebugThisPathNode = nodeList.GetPathNode(j);
					if (!pDebugThisPathNode)
					{
						break;
					}
					Displayf("point %d: %.2f, %.2f, %.2f  %d:%d", j, pDebugThisPathNode->GetPos().x, pDebugThisPathNode->GetPos().y, pDebugThisPathNode->GetPos().z
						, pDebugThisPathNode->GetAddrRegion(), pDebugThisPathNode->GetAddrIndex());
				}
			}
		}
#endif

		//we're hitting an assert in FindRoadBoundaries for having no lanes on either side,
		//putting this here to try and catch it earlier -JM
#if AI_OPTIMISATIONS_OFF
		Assert(m_RoutePoints[i].m_iNoLanesThisSide > 0 || m_RoutePoints[i].m_iNoLanesOtherSide > 0);
#endif //AI_OPTIMISATIONS_OFF

		const s8 iCurrentLane = nodeList.GetPathLaneIndex(i);
		const ScalarV scLane((float)nodeList.GetPathLaneIndex(i));

		//initialize the lane centre offset to the position of the zeroth lane
		//which is the position it should have for lane 0, not the origin
		Vec2V laneCenterOffsXYV;
		if (pNextNode)
		{
			Vec3V Dir = FlattenXY(RCC_VEC3V(nextNodeCoors) - RCC_VEC3V(thisNodeCoors));
			Dir = NormalizeFast(Dir);
			const Vec2V perpDirXY = GetFromTwo<Vec::Y1, Vec::X2>(Dir, Negate(Dir));

			laneCenterOffsXYV = perpDirXY * m_RoutePoints[i].GetCentralLaneWidthV() + perpDirXY * scLane * m_RoutePoints[i].GetLaneWidthV();
		}
		else
		{
			laneCenterOffsXYV = Vec2V(V_ZERO);
		}
		m_RoutePoints[i].SetLaneCenterOffsetAndNodeAddrAndLane(laneCenterOffsXYV, currentTargetNode, scLane);

		//now, if this is a junction, look backward and set the required lane indices for 
		//all the nodes leading up to it within a certain distance
		//but only if we're in the leftmost or rightmost lane
		const int iNumLanesThisDirection = pLink ? pLink->m_1.m_LanesToOtherNode : 1;
		if (pLink && pNextNode && pNextNode->m_2.m_slipJunction)
		{
			const bool bInMiddleLane = !(iCurrentLane == 0 || iCurrentLane == iNumLanesThisDirection-1);
			u32 pathDirFlags = BIT_NOT_SET;
			if (i < iNumPathNodesInList-2)
			{
				CNodeAddress nextNextNode = nodeList.GetPathNodeAddr(i+2);
				if (!nextNextNode.IsEmpty() && ThePaths.IsRegionLoaded(nextNextNode))
				{
					const CPathNode* pNextNextNode = ThePaths.FindNodePointer(nextNextNode);
					bool	bSharpTurn;
					float	fLeftness;
					float	fDirectionDot;
					static dev_float thr = 0.92f;
					
					Vector2 vFromTemp(thisNodeCoors.x, thisNodeCoors.y);
					Vector2 vCurrTemp(nextNodeCoors.x, nextNodeCoors.y);
					Vector2 vNewTemp;
					pNextNextNode->GetCoors2(vNewTemp);
					Vector2 vFromToCurr = vCurrTemp - vFromTemp;
					Vector2 vCurrToNew = vNewTemp - vCurrTemp;

					//try looking at the link right before the intersection
					if (i > 0 && pLink->m_1.m_bDontUseForNavigation)
					{
						CNodeAddress OldOldNode = nodeList.GetPathNodeAddr(i-1);
						const CPathNode* pOldOldNode = ThePaths.FindNodePointerSafe(OldOldNode);
						if (pOldOldNode)
						{
							Vector2 vOldOldTemp;
							pOldOldNode->GetCoors2(vOldOldTemp);
							vFromToCurr = vFromTemp - vOldOldTemp;
						}
					}

					if( i < iNumPathNodesInList-3)
					{
						const CPathNodeLink *pNewLink = ThePaths.FindLinkPointerSafe(nextNextNode.GetRegion(),nodeList.GetPathLinkIndex(i+2));
						aiAssert(pNewLink);
						if (pNewLink && pNewLink->m_1.m_bDontUseForNavigation)
						{
							CNodeAddress NextNextNextNode = nodeList.GetPathNodeAddr(i+3);
							const CPathNode* pNextNextNextNode = ThePaths.FindNodePointerSafe(NextNextNextNode);
							if (pNextNextNextNode)
							{
								Vector2 vNewNewTemp;
								pNextNextNextNode->GetCoors2(vNewNewTemp);
								vCurrToNew = vNewNewTemp - vNewTemp;
							}
						}
					}

					pathDirFlags = CPathNodeRouteSearchHelper::FindPathDirectionInternal(vFromToCurr, vCurrToNew, &bSharpTurn, &fLeftness, thr, &fDirectionDot);
				}
			}

			bool bSetRequiredLane = pathDirFlags != BIT_TURN_STRAIGHT_ON && pathDirFlags != BIT_NOT_SET && !bInMiddleLane;

			//if the right lane is forced to go right, obey our lane even
			//if we're going straight through, so we don't do avoidance
			//and end up in the right lane. technically this is a bit incorrect,
			//since if there are two straight ahead lanes we shouldn't have 
			//a preference between the two, but multiple required lanes aren't 
			//supported at the moment, and there are few enough of these junctions
			//that I think it'll be alright
			bool bRightLaneTurnsRightOnly = false;
			bool bLeftLaneTurnsLeftOnly = false;
			//if (!bSetRequiredLane)
			{
				if (pCurrentNode->m_2.m_leftOnly)
				{
					bSetRequiredLane = true;
					bLeftLaneTurnsLeftOnly = true;
				}
			}	

			//if (!bSetRequiredLane)
			{
				CJunction* pJunction = CJunctions::FindJunctionFromNode(NextTargetNode);	
				if (pJunction)
				{
					s32 iEntranceIndex = pJunction->FindEntranceIndexWithNode(currentTargetNode);
					if (iEntranceIndex != -1)
					{
						CJunctionEntrance& entrance = pJunction->GetEntrance(iEntranceIndex);
						if (entrance.m_bRightLaneIsRightOnly)
						{
							bRightLaneTurnsRightOnly = true;
							bSetRequiredLane = true;
						}
					}
				}	
			}
			
			if (bSetRequiredLane)
			{
				s8 newLane = iCurrentLane;
				if (bUpdateLanesForTurns)
				{
					if (pathDirFlags == BIT_TURN_LEFT)
					{
						newLane = 0;
					}
					else if (pathDirFlags == BIT_TURN_RIGHT)
					{
						newLane = (s8)rage::Max(0, iNumLanesThisDirection-1);
					}
					else if (pathDirFlags == BIT_TURN_STRAIGHT_ON)
					{
						if (bRightLaneTurnsRightOnly && iCurrentLane == iNumLanesThisDirection-1)
						{
							newLane = (s8)rage::Max(0, iNumLanesThisDirection-2);
						}
						else if (bLeftLaneTurnsLeftOnly && iCurrentLane == 0 && iNumLanesThisDirection > 1)
						{
							newLane = (s8)rage::Min(iNumLanesThisDirection, 1);
						}
					}
				}

				m_RoutePoints[i].m_iRequiredLaneIndex = newLane;

				//feed this back to the followroute
				nodeList.SetPathLaneIndex(i, newLane);

				static dev_float s_fBaseRequiredLaneDistance = 40.0f;
				static dev_float s_fMaxRequiredLaneDistance = 100.0f;
				const float fTestDistance = rage::Min(pNextNode->m_2.m_speed * s_fBaseRequiredLaneDistance, s_fMaxRequiredLaneDistance);
				const float fTestDistanceSqr = fTestDistance * fTestDistance;

				int iNode = i-1;
				while (iNode >= 0)
				{
					CNodeAddress nodeBack = nodeList.GetPathNodeAddr(iNode);
					if (nodeBack.IsEmpty() || !ThePaths.IsRegionLoaded(nodeBack))
					{
						break;
					}

					const CPathNode *pBackNode = ThePaths.FindNodePointer(nodeBack);
					Assert(pBackNode);

					//something has already set the required lane index.
					//since we're going from the beginning to the end of the path,
					//assume anything done before takes precedence
					if (m_RoutePoints[iNode].m_iRequiredLaneIndex != -1)
					{
						break;
					}

					if (pBackNode->GetPos().Dist2(pNextNode->GetPos()) > fTestDistanceSqr)
					{
						break;
					}

					//don't set required lanes inside a junction
					if (pBackNode->IsJunctionNode())
					{
						break;
					}

					//don't set required lanes for a junction exit node
					if (iNode > 0)
					{
						const CPathNode* pBackBackNode = ThePaths.FindNodePointerSafe(nodeList.GetPathNodeAddr(iNode-1));
						if (pBackBackNode && pBackBackNode->IsJunctionNode())
						{
							break;
						}
					}

					CPathNodeLink *pBackLink = ThePaths.FindLinkPointerSafe(nodeBack.GetRegion(),nodeList.GetPathLinkIndex(iNode));
					Assert(pBackLink);
					if(pBackLink)
					{
						//clamp to our current link's #lanes
						m_RoutePoints[iNode].m_iRequiredLaneIndex = Min(newLane, (s8)(pBackLink->m_1.m_LanesToOtherNode-1));
					}
					else
					{
						m_RoutePoints[iNode].m_iRequiredLaneIndex = newLane;
					}

					iNode--;
				}
			}
		}

		if (pCurrentNode->NumLinks() <= 2
			&& pPrevLink
			&& iNumLanesThisDirection > pPrevLink->m_1.m_LanesToOtherNode
			&& pPrevLink->m_1.m_LanesToOtherNode == 1)
		{
			//grcDebugDraw::Line(pVehicle->GetVehiclePosition(), pVehicle->GetVehiclePosition() + Vec3V(0.0f, 0.0f, 5.0f), Color_yellow, 15);
			//bSetRequiredLane = true;
			m_RoutePoints[i].m_iRequiredLaneIndex = iCurrentLane;
		}
	}


	//Assert(m_iNumPoints > 0);
	if (m_iNumPoints <= 0)
	{
		Invalidate();
		return;
	}

	//we may have received a nodelist whose target node is
	//not streamed in. in that case, clamp target position to
	//the number of streamed nodes.
	//const int iTargetNodeClamped = Clamp(nodeList.GetTargetNodeIndex(), 0, m_iNumPoints-1);
	int iTargetNodeClamped = rage::Min(nodeList.GetTargetNodeIndex(), m_iNumPoints-1);
	iTargetNodeClamped = rage::Max(iTargetNodeClamped, 0);
	SetCurrentTargetPoint(iTargetNodeClamped);

	m_vPosWhenLastConstructedNodeList = pVehicle->GetVehiclePosition();

	//TUNE_GROUP_BOOL(ASDF, GiveLaneSlackForAllVehicles, false);
	if (pVehicle->InheritsFromBike()/* || GiveLaneSlackForAllVehicles*/)
	{
		m_nTimeLastGivenLaneSlack = NetworkInterface::GetSyncedTimeInMilliseconds();
	}

	// Populate the route with desired speeds.
	CalculateDesiredSpeeds(pVehicle, bUseStringPulling);

	ResetPrecalcData();

	if (m_iNumPoints >= 2)
	{
		pVehicle->GetIntelligence()->m_BackupFollowRoute = *this;
	}

	pVehicle->GetIntelligence()->CacheJunctionInfo();
}

void CVehicleFollowRouteHelper::ConstructFromWaypointRecording(const CVehicle* pVehicle, CWaypointRecordingRoute& route, int iCurrentProgress, int iTargetProgress, bool bUseAISlowdown, bool bUseOverrideSpeed, float fOverrideSpeed)
{
	m_bAllowUseForPhysicalConstraint = true;
	m_bCurrentRouteContainsLaneChange = false;

	//do stuff with the route here
	//m_iNumPoints = Min((int)MAX_ROUTE_SIZE, route.GetSize());

	m_iNumPoints = 0;

	m_vRandomNodeOffset.ZeroComponents();

	//if currentprogress isn't 0, start our route 1 behind the current progress.
	//so if we're at the end of the route, we're guaranteed to have 2 points on the route
	const bool bStartAtPrevPoint = iCurrentProgress > 0;
	const float fCarWidth = pVehicle->GetBoundingBoxMax().x;

	for (int i = 0; i < MAX_ROUTE_SIZE; i++)
	{
		m_RoutePoints[i].m_iRequiredLaneIndex = -1; 

		const int routeIndex = bStartAtPrevPoint ? iCurrentProgress + i - 1 : iCurrentProgress + i;
		if (routeIndex >= route.GetSize())
		{
			//we reached the end of the waypoint recording
			break;
		}

		if (routeIndex > iTargetProgress && iTargetProgress != -1)
		{
			//we reached the last desired node last time,
			//don't go beyond it
			break;
		}

		m_RoutePoints[i].m_vPositionAndSpeed.SetXYZ(VECTOR3_TO_VEC3V(route.GetWaypoint(routeIndex).GetPosition()));
		m_RoutePoints[i].m_iNoLanesOtherSide = 0;
		m_RoutePoints[i].ClearLaneCenterOffsetAndNodeAddrAndLane();

#if __DEV
		if (i > 0)
		{
			aiAssertf((Dist(m_RoutePoints[i-1].GetPosition(), m_RoutePoints[i].GetPosition()) > ScalarV(V_FLT_EPSILON)).Getb()
				, "Trying to add a duplicate point to followRoute from Waypoint Recording route (point %d)", routeIndex);
		}
#endif

		if (bUseAISlowdown)
		{
			m_RoutePoints[i].SetSpeed(10.0f);
		}
		else
		{
			if (bUseOverrideSpeed)
			{
				m_RoutePoints[i].SetSpeed(fOverrideSpeed);
			}
			else
			{
				m_RoutePoints[i].SetSpeed(route.GetWaypoint(routeIndex).GetSpeed());
			}
		}

		m_RoutePoints[i].m_iCamberThisSide = 0;
		m_RoutePoints[i].m_iCamberOtherSide = 0;
		m_RoutePoints[i].m_bIgnoreForNavigation = false;
		m_RoutePoints[i].m_bJunctionNode = false;
		m_RoutePoints[i].m_bHasLeftOnlyLane = false;
		m_RoutePoints[i].m_bIsHighway = false;
		m_RoutePoints[i].m_bIsSlipLane = false;
		m_RoutePoints[i].m_bIsSlipJunction = false;
		
		const float fFreeSpaceToLeft = rage::Max(0.0f, route.GetWaypoint(routeIndex).GetFreeSpaceToLeft() - fCarWidth);
		const float fFreeSpaceToRight = rage::Max(0.0f,route.GetWaypoint(routeIndex).GetFreeSpaceToRight() - fCarWidth);
		if (fFreeSpaceToLeft < SMALL_FLOAT && fFreeSpaceToRight < SMALL_FLOAT)
		{
			m_RoutePoints[i].m_bIsSingleTrack = true;
			m_RoutePoints[i].m_iNoLanesThisSide = 1;
			m_RoutePoints[i].m_fCentralLaneWidth = 0.0f;
			m_RoutePoints[i].m_fLaneWidth = LANEWIDTH;
		}
		else
		{
			const u32 iMaxLanes = 16;
			const u32 iDefaultLaneWidth = 1;
			const float fTotalWidth = fFreeSpaceToLeft + fFreeSpaceToRight;
			Assert(fTotalWidth > 0.0f);
			m_RoutePoints[i].m_bIsSingleTrack = false;
			m_RoutePoints[i].m_fCentralLaneWidth = -fFreeSpaceToLeft;
			const u32 iNumLanes = rage::Min(iMaxLanes, (u32)ceilf(fTotalWidth) / iDefaultLaneWidth);
			m_RoutePoints[i].m_iNoLanesThisSide = (u8)iNumLanes;
			m_RoutePoints[i].m_fLaneWidth = iNumLanes / fTotalWidth;
		}
		

		++m_iNumPoints;
	}

	if (m_iNumPoints <= 1)
	{
		Invalidate();
		return;
	}

	SetCurrentTargetPoint(1);

	m_vPosWhenLastConstructedNodeList = pVehicle->GetTransform().GetPosition();

	if (bUseAISlowdown)
	{
		CalculateDesiredSpeeds(pVehicle, false);
	}

	ResetPrecalcData();

	if (m_iNumPoints >= 2)
	{
		pVehicle->GetIntelligence()->m_BackupFollowRoute = *this;
	}
}

#define HANDLE_FOLLOWROUTE_FOR_DUPE_POINTS 1

void CVehicleFollowRouteHelper::ConstructFromNavmeshRoute(const CVehicle* pVehicle, Vector3* pvPointsOnRoute, const s32 iNumPoints)
{
	m_bAllowUseForPhysicalConstraint = true;
	m_bCurrentRouteContainsLaneChange = false;

	m_iNumPoints = 0;

	m_vRandomNodeOffset.ZeroComponents();

#if HANDLE_FOLLOWROUTE_FOR_DUPE_POINTS
	u32 nSkippedPoints = 0;
	const ScalarV vFltEpsilonSqr(V_FLT_EPSILON * V_FLT_EPSILON);
#endif //HANDLE_FOLLOWROUTE_FOR_DUPE_POINTS

	for (int i = 0; i < MAX_ROUTE_SIZE; i++)
	{
		m_RoutePoints[i].m_iRequiredLaneIndex = -1; 

#if HANDLE_FOLLOWROUTE_FOR_DUPE_POINTS
		const int routeIndex = i + nSkippedPoints;
#else
		const int routeIndex = i;
#endif //HANDLE_FOLLOWROUTE_FOR_DUPE_POINTS
		if (routeIndex >= iNumPoints)
		{
			//we reached the end of the waypoint recording
			break;
		}

		const Vec3V vRoutePosition = VECTOR3_TO_VEC3V(pvPointsOnRoute[routeIndex]);

#if HANDLE_FOLLOWROUTE_FOR_DUPE_POINTS
		if (i > 0)
		{
			const Vec3V vPrevRoutePosition = m_RoutePoints[i-1].GetPosition();
			const ScalarV vDistSqr = DistSquared(vRoutePosition, vPrevRoutePosition);

#if (AI_OPTIMISATIONS_OFF) || (NAVMESH_OPTIMISATIONS_OFF) || (AI_VEHICLE_OPTIMISATIONS_OFF)
			if (!aiVerifyf( IsGreaterThanAll( vDistSqr, vFltEpsilonSqr), "Trying to add a duplicate point %d to followRoute from Navmesh route", i))
#else
			if (!IsGreaterThanAll( vDistSqr, vFltEpsilonSqr))
#endif
			{
				++nSkippedPoints;
				--i;	//retry the same followroute index

#if __DEV
				for (int j = 0; j < iNumPoints; j++)
				{
					Displayf("point %d: %.2f, %.2f, %.2f", j, pvPointsOnRoute[j].x, pvPointsOnRoute[j].y, pvPointsOnRoute[j].z);
				}
#endif //__DEV
				continue;
			}
		}
#endif //HANDLE_FOLLOWROUTE_FOR_DUPE_POINTS

		m_RoutePoints[i].m_fCentralLaneWidth = 0.0f;
		m_RoutePoints[i].m_vPositionAndSpeed.SetXYZ(vRoutePosition);
		m_RoutePoints[i].m_iNoLanesThisSide = 1;
		m_RoutePoints[i].m_iNoLanesOtherSide = 0;
		m_RoutePoints[i].m_fLaneWidth = LANEWIDTH_NARROW;
		m_RoutePoints[i].ClearLaneCenterOffsetAndNodeAddrAndLane();

		m_RoutePoints[i].SetSpeed(10.0f);	

		//TODO: get camber from navmesh normal
		m_RoutePoints[i].m_iCamberThisSide = 0;
		m_RoutePoints[i].m_iCamberOtherSide = 0;

		m_RoutePoints[i].m_bIsSingleTrack = false;
		m_RoutePoints[i].m_bIgnoreForNavigation = false;
		m_RoutePoints[i].m_bJunctionNode = false;
		m_RoutePoints[i].m_bHasLeftOnlyLane = false;
		m_RoutePoints[i].m_bIsHighway = false;
		m_RoutePoints[i].m_bIsSlipLane = false;
		m_RoutePoints[i].m_bIsSlipJunction = false;

		++m_iNumPoints;
	}

	SetCurrentTargetPoint(1);

	m_vPosWhenLastConstructedNodeList = pVehicle->GetTransform().GetPosition();

	CalculateDesiredSpeeds(pVehicle, false);

	ResetPrecalcData();

	if (m_iNumPoints >= 2)
	{
		pVehicle->GetIntelligence()->m_BackupFollowRoute = *this;
	}
}

void CVehicleFollowRouteHelper::ConstructFromRoutePointArray(const CVehicle* pVehicle, const CRoutePoint* pRoutePoints, const s32 iNumPoints, const s32 iDesiredTargetPoint)
{
	Assert(pRoutePoints);

	m_bAllowUseForPhysicalConstraint = true;
	m_bCurrentRouteContainsLaneChange = false;

	m_iNumPoints = 0;

	m_vRandomNodeOffset.ZeroComponents();

	for (int i = 0; i < MAX_ROUTE_SIZE; i++)
	{
		m_RoutePoints[i].m_iRequiredLaneIndex = -1; 

		const int routeIndex = i;
		if (routeIndex >= iNumPoints)
		{
			//we reached the end of the waypoint recording
			break;
		}

		const Vec3V vRoutePosition = pRoutePoints[i].GetPosition();

		m_RoutePoints[i].m_fCentralLaneWidth = 0.0f;
		m_RoutePoints[i].m_vPositionAndSpeed.SetXYZ(vRoutePosition);
		m_RoutePoints[i].m_iNoLanesThisSide = 1;
		m_RoutePoints[i].m_iNoLanesOtherSide = 0;
		m_RoutePoints[i].m_fLaneWidth = LANEWIDTH;
		m_RoutePoints[i].ClearLaneCenterOffsetAndNodeAddrAndLane();

		m_RoutePoints[i].SetSpeed(pRoutePoints[i].GetSpeed());	

		//TODO: get camber from scenario point normal
		m_RoutePoints[i].m_iCamberThisSide = 0;
		m_RoutePoints[i].m_iCamberOtherSide = 0;

		m_RoutePoints[i].m_bIsSingleTrack = true;
		m_RoutePoints[i].m_bIgnoreForNavigation = false;
		m_RoutePoints[i].m_bJunctionNode = false;
		m_RoutePoints[i].m_bHasLeftOnlyLane = false;
		m_RoutePoints[i].m_bIsHighway = false;
		m_RoutePoints[i].m_bIsSlipLane = false;
		m_RoutePoints[i].m_bIsSlipJunction = false;

#if __DEV
		if (i > 0)
		{
			aiAssertf((Dist(m_RoutePoints[i-1].GetPosition(), m_RoutePoints[i].GetPosition()) > ScalarV(V_FLT_SMALL_4)).Getb(), "Trying to add a duplicate point to followRoute from scenario chain at %.2f, %.2f, %.2f"
				, m_RoutePoints[i].GetPosition().GetXf(), m_RoutePoints[i].GetPosition().GetYf(), m_RoutePoints->GetPosition().GetZf());
		}
#endif

		++m_iNumPoints;
	}

	SetCurrentTargetPoint(iDesiredTargetPoint);

	m_vPosWhenLastConstructedNodeList = pVehicle->GetVehiclePosition();

	CalculateDesiredSpeeds(pVehicle, false);

	ResetPrecalcData();

	if (m_iNumPoints >= 2)
	{
		pVehicle->GetIntelligence()->m_BackupFollowRoute = *this;
	}
}

void CVehicleFollowRouteHelper::UpdateTargetPositionForStraightLineUpdate(const CVehicle* pVehicle, s32 iDrivingFlags)
{
	m_bDrivingInReverse = (iDrivingFlags & DF_DriveInReverse) != 0;

	if (m_iNumPoints < 2)
	{
		ResetCurrentTargetPoint();
	}
	else
	{
		//the iEnd param is non-inclusive here
		//so pass in m_iNumPoints - 1 so that m_iCurrentTargetPoint is guaranteed to be within-bounds
		Vec3V vBonnetPos = GetVehicleBonnetPosition(pVehicle, (iDrivingFlags & DF_DriveInReverse) != 0);
		const int iClosestPoint = FindClosestPointToPosition(vBonnetPos, rage::Max(0, GetCurrentTargetPoint() - 2));

		Assertf(iClosestPoint >= 0 && iClosestPoint < m_iNumPoints, "iClosestPoint out of range--NumPoints: %d", m_iNumPoints);

		//if the closest point is in front of the front bumper (bonnet), it's good enough to be used as a target position
		//if not, try using one in front of it -JM
		const int iNewTargetPoint = Dot(FlattenXY(m_RoutePoints[iClosestPoint].GetPositionWithRandomOffset(m_vRandomNodeOffset) - vBonnetPos)
			, FlattenXY(pVehicle->GetVehicleForwardDirection())).Getf() > 0.0f
			? iClosestPoint
			: rage::Min(iClosestPoint+1, m_iNumPoints - 1);

		Assertf(iNewTargetPoint >= 0 && iNewTargetPoint < m_iNumPoints, "iNewTargetPoint out of range--NumPoints: %d", m_iNumPoints);

		SetCurrentTargetPoint(iNewTargetPoint);
	}
}


bool CVehicleFollowRouteHelper::DoesPathPassNearPoint(Vec3V_In testPos,
		Vec3V_In currentVehPos, const float& distThreshold) const
{
	// Potentially this array is not initialized until FindTargetCoorsAndSpeed_NewIsolated is called,
	// which is one frame after the constructor.  For now the safest thing to do here is probably just return false.
	if (!m_PathWaypointDataInitialized)
	{
		return false;
	}

	const Vec3V testPosV = testPos;
	const Vec3V currentVehPosV = currentVehPos;

	// Get the current target point, if we can.
	int tgt = GetCurrentTargetPoint();
	if(tgt < 0)
	{
		tgt = 0;

	}
	const int numPts = GetNumPoints();

	// Load the distance threshold.
	const ScalarV distThresholdV = LoadScalar32IntoScalarV(distThreshold);
	const ScalarV distThresholdSqV = Scale(distThresholdV, distThresholdV);

	// Prepare for the first segment.
	Vec3V posOnPath1V;
	Vec3V posOnPath2V = currentVehPosV;

	// Loop over the segments.
	for(int i = tgt; i < numPts; i++)
	{
		posOnPath1V = posOnPath2V;

		// Note: the i + 1 stuff here seems to be how this array is set up,
		// it actually contains GetNumPoints() + 1 elements. I think...
		posOnPath2V = m_vPathWaypointsAndSpeed[i + 1].GetXYZ();

		// Compute the segment delta, needed by FindTValueSegToPoint().
		const Vec3V posDeltaV = Subtract(posOnPath2V, posOnPath1V);

		// Use FindTValueSegToPoint() to find the T value of the closest point.
		const ScalarV tV = geomTValues::FindTValueSegToPoint(posOnPath1V, posDeltaV, testPosV);

		// Compute the closest point on this segment.
		const Vec3V closestPosOnSegmentV = Lerp(tV, posOnPath1V, posOnPath2V);

		// Compute the distance to the test position.
		const ScalarV distSqV = DistSquared(closestPosOnSegmentV, testPosV);

		// Check if close enough.
		if(IsLessThanAll(distSqV, distThresholdSqV))
		{
			return true;
		}
	}

	// No segment was within the distance.
	return false;
}


s16 CVehicleFollowRouteHelper::FindClosestPointToPosition(Vec3V_In vPos, const int iStart, int iEnd) const
{
	Assert(iStart >= 0);
	Assert(iEnd <= m_iNumPoints);	//iEnd is the non-inclusive end of the iteration, so allow it to be equal to m_iNumPoints
	
	iEnd = (iEnd) ? iEnd : m_iNumPoints;

	int iClosestIndex = -1;
	const Vec3V vPosV( vPos );
	ScalarV fClosestDistSqr( V_FLT_MAX );
	
	for(int i=iStart; i<iEnd; i++)
	{
		const ScalarV fDistSqr = DistSquared(vPosV, m_RoutePoints[i].GetPosition());
		
		// FFTODO: Remove branching on vector comparison results, we should be able
		// to use vector selection operations here.
		if(IsLessThanAll(fDistSqr, fClosestDistSqr))
		{
			fClosestDistSqr = fDistSqr;
			iClosestIndex = i;
		}
	}
	//Assert (iClosestIndex >= 0);
	return (s16)iClosestIndex;
}

bool CVehicleFollowRouteHelper::GetBoundarySegmentsBetweenPoints(const int iFromPoint, const int iToPoint, const int iNextPoint 
	, bool bClampToCenterOfRoad, Vector3& vLeftEnd, Vector3& vRightEnd , Vector3& vLeftStartPos, Vector3& vRightStartPos
	, const float fVehicleHalfWidthToUseLeftSide, const float fVehicleHalfWidthToUseRightSide, const bool bAddExtraLaneToSlipForAvoidance) const
{
	Vector3 vFromNodeCoords = VEC3V_TO_VECTOR3(m_RoutePoints[iFromPoint].GetPosition());
	Vector3 vToNodeCoords = VEC3V_TO_VECTOR3(m_RoutePoints[iToPoint].GetPosition());
	Vector3 vFromToToNode = vToNodeCoords - vFromNodeCoords;
	
	float fRoadWidthLeft=0.0f, fRoadWidthRight=0.0f;
	m_RoutePoints[iFromPoint].FindRoadBoundaries(fRoadWidthLeft, fRoadWidthRight);

	//on single track roads, we hackily add some space on the right side to get cars to prefer passing that way
	//fRoadWidthRight += fHackExtraWidthForRightSide;

	Assert(fRoadWidthLeft >= 0.0f);
	Assert(fRoadWidthRight >= 0.0f);

	// Narrow the drive area a bit so that the cars don't get clipped so much.
	fRoadWidthLeft -= fVehicleHalfWidthToUseLeftSide;
	fRoadWidthLeft = rage::Max(0.0f, fRoadWidthLeft);
	fRoadWidthRight -= fVehicleHalfWidthToUseRightSide;
	fRoadWidthRight = rage::Max(0.0f, fRoadWidthRight);

	//if this is a sliplane, we're guaranteed at least
	//one extra lane to our right, so add that to the right side
	//(or left, if we're driving in reverse) to allow vehicles
	//more room to manouver during avoidance
	if (bAddExtraLaneToSlipForAvoidance)
	{
		//allow any combo except Junction && Junction
		const bool bFromPointIsAnyKindaJunction = m_RoutePoints[iFromPoint].m_bJunctionNode || m_RoutePoints[iFromPoint].m_bIsSlipJunction;
		const bool bToPointIsAnyKindaJunction = m_RoutePoints[iToPoint].m_bJunctionNode || m_RoutePoints[iToPoint].m_bIsSlipJunction;
		if ((m_RoutePoints[iFromPoint].m_bIsSlipLane && m_RoutePoints[iToPoint].m_bIsSlipLane)
			|| (m_RoutePoints[iFromPoint].m_bIsSlipLane && bToPointIsAnyKindaJunction)
			|| (bFromPointIsAnyKindaJunction && m_RoutePoints[iToPoint].m_bIsSlipLane))
		{
			if (m_RoutePoints[iFromPoint].m_iNoLanesThisSide > 0)
			{
				fRoadWidthRight += m_RoutePoints[iFromPoint].m_fLaneWidth;
			}
			else
			{
				fRoadWidthLeft += m_RoutePoints[iFromPoint].m_fLaneWidth;
			}
		}
	}

	//give a boost to ignorenav link size
	static dev_float s_fExtraWidthForIgnoreNavLinks = m_RoutePoints[iFromPoint].m_fLaneWidth * 0.5f;
	if (m_RoutePoints[iFromPoint].m_bIgnoreForNavigation)
	{
		fRoadWidthLeft += s_fExtraWidthForIgnoreNavLinks;
		fRoadWidthRight += s_fExtraWidthForIgnoreNavLinks;
	}

	Vector3 vRight;
	vRight = CrossProduct(vFromToToNode, ZAXIS);
	vRight.NormalizeSafe();

	vRightStartPos = vFromNodeCoords + vRight*fRoadWidthRight;

	//only clamp our search extents to this side of the road if we have lanes coming in the other direction
	if (bClampToCenterOfRoad && m_RoutePoints[iFromPoint].m_iNoLanesOtherSide > 0)
	{
		vLeftStartPos = vFromNodeCoords + vRight * (m_RoutePoints[iFromPoint].m_fCentralLaneWidth - 0.5f*m_RoutePoints[iFromPoint].m_fLaneWidth);
	}
	else
	{
		vLeftStartPos = vFromNodeCoords + -vRight*fRoadWidthLeft;
	}

	//CPathNode* pNext = FindNodePointerSafe(nextNode);
	if (iNextPoint >= 0 && !m_RoutePoints[iToPoint].m_bIgnoreForNavigation)
	{
		//if the user passed us a third node, use its start position as our end position
		Vector3 vNextNodeCoords = VEC3V_TO_VECTOR3(m_RoutePoints[iNextPoint].GetPosition());
		Vector3 vToToNext = vNextNodeCoords - vToNodeCoords;

		Vector3 vNextRight;
		vNextRight = CrossProduct(vToToNext, ZAXIS);
		vNextRight.NormalizeSafe();

 		float fNextRoadWidthLeft = fRoadWidthLeft;
 		float fNextRoadWidthRight = fRoadWidthRight;
		m_RoutePoints[iToPoint].FindRoadBoundaries(fNextRoadWidthLeft, fNextRoadWidthRight);

		// Narrow the drive area a bit so that the cars don't get clipped so much.
		fNextRoadWidthLeft -= fVehicleHalfWidthToUseLeftSide;
		fNextRoadWidthLeft = rage::Max(0.0f, fNextRoadWidthLeft);
		fNextRoadWidthRight -= fVehicleHalfWidthToUseRightSide;
		fNextRoadWidthRight = rage::Max(0.0f, fNextRoadWidthRight);
		
		fwGeom::fwLine rightLine(vRightStartPos, vRightStartPos + vFromToToNode*3.0f);
		rightLine.FindClosestPointOnLine(vToNodeCoords + vNextRight*fNextRoadWidthRight, vRightEnd);

		if (bClampToCenterOfRoad && m_RoutePoints[iFromPoint].m_iNoLanesOtherSide > 0)
		{
			vLeftEnd =  (vToNodeCoords + vNextRight*(m_RoutePoints[iFromPoint].m_fCentralLaneWidth - 0.5f*m_RoutePoints[iFromPoint].m_fLaneWidth));
		}
		else
		{
			fwGeom::fwLine leftLine(vLeftStartPos, vLeftStartPos + vFromToToNode*3.0f);
			leftLine.FindClosestPointOnLine(vToNodeCoords + -vNextRight*fNextRoadWidthLeft, vLeftEnd);
		}
			
	}
	else
	{
		//otherwise just use our delta vector
		vLeftEnd = vLeftStartPos + vFromToToNode;
		vRightEnd = vRightStartPos + vFromToToNode;
	}

	return true;
}

// bool CVehicleFollowRouteHelper::GetCurrentTargetPointPosition(Vector3& vTargetPos) const
// {
// 	if (GetCurrentTargetPoint() >= 0 && GetCurrentTargetPoint() < m_iNumPoints)
// 	{
// 		vTargetPos = m_RoutePoints[GetCurrentTargetPoint()].m_vPosition;
// 		return true;
// 	}
// 
// 	return false;
// }

bool CVehicleFollowRouteHelper::IsUpcomingLaneChangeToTheLeft() const
{
	Assertf(m_bCurrentRouteContainsLaneChange, "IsUpcomingLaneChangeToTheLeft() called with no upcoming lane change, this function has no meaning!");

	//find the next required lane
	int iRequiredLaneIndex = -1;
	int iIterationStart = rage::Max(1, GetCurrentTargetPoint()-1);
	for (int i = iIterationStart; i < m_iNumPoints; i++)
	{
		if (m_RoutePoints[i].m_iRequiredLaneIndex != -1)
		{
			iRequiredLaneIndex = i;
			break;
		}
	}

	//compare this lane to the lane before it
	if (iRequiredLaneIndex > 0)
	{
		const int iActualRequiredLane = rage::round(m_RoutePoints[iRequiredLaneIndex].GetLane().Getf());
		const int iLaneBeforeRequired = rage::round(m_RoutePoints[iRequiredLaneIndex-1].GetLane().Getf());

		return iActualRequiredLane <= iLaneBeforeRequired;
	}

	return true;
}

void CVehicleFollowRouteHelper::GiveLaneSlack()
{
	m_nTimeLastGivenLaneSlack = NetworkInterface::GetSyncedTimeInMilliseconds();
}

float CVehicleFollowRouteHelper::FindTargetCoorsAndSpeed_New( const CVehicle* pVehicle, float fDistanceAhead, float fDistanceToProgressRoute
	, Vec3V_In vStartCoords, Vec3V_InOut vOutTargetCoords, float& fOutMaxSpeed, const bool bUseSpeedAtCurrentPosition
	, const bool bUseCachedSpeedVel, const bool bUseStringPulling, const int iOverrideEndPointIndex, const bool bAllowLaneSlack)
{
	PF_FUNC(FindTargetCoorsAndSpeed);

	const CAIHandlingInfo* pAIHandlingInfo = pVehicle->GetAIHandlingInfo();
	Assert(pAIHandlingInfo);
	const CPed* pVehicleDriver = pVehicle->GetDriver();
	const Vec3V vVehiclePos = pVehicle->GetVehiclePosition();
	const Vec3V vVehicleVel = bUseCachedSpeedVel ? VECTOR3_TO_VEC3V(pVehicle->GetAiVelocity()) : VECTOR3_TO_VEC3V(pVehicle->GetVelocity());
#if __BANK
	const bool bRender = CVehicleIntelligence::m_bFindCoorsAndSpeed && CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(pVehicle);
#else
	const bool bRender = false;
#endif

	static float s_BoatOffset = 0.25f;

	const float handlingMinBrakeDistancef = pAIHandlingInfo->GetMinBrakeDistance();
	const float handlingMaxBrakeDistancef = pAIHandlingInfo->GetMaxBrakeDistance();
	const float handlingMaxSpeedAtBrakeDistancef = pAIHandlingInfo->GetMaxSpeedAtBrakeDistance();
	const float driverCurveSpeedModifierf = CDriverPersonality::GetDriverCornerSpeedModifier(pVehicleDriver, pVehicle, fOutMaxSpeed);
	const float driverNumSecondsToChangeLanesf = CDriverPersonality::GetTimeToChangeLanes(pVehicleDriver, pVehicle);
	const float fDriverRacingModifier = !pVehicleDriver ? 0.0f : pVehicleDriver->GetPedIntelligence()->GetDriverRacingModifier();
	const float fDriverInverseRacingModifier = pVehicle->InheritsFromBoat() ? (1.0f - fDriverRacingModifier) * (1.0f - s_BoatOffset) : 1.0f - fDriverRacingModifier;
	static dev_u32 s_nNumMSToReachCenterOfLane = 1000;

	//only allow a bit of tolerance from the center of the lane if we're a bike and we've constructed our new route recently
	const u32 nTimeInMS = NetworkInterface::GetSyncedTimeInMilliseconds();
	//ensure last given is never lower than current time
	m_nTimeLastGivenLaneSlack = Min(nTimeInMS, m_nTimeLastGivenLaneSlack);
	const float fHalfLane = 0.5f;
	const float fMaxDistanceAllowedFromLaneCenterInLaneSpace = 
		(!bAllowLaneSlack || (nTimeInMS > m_nTimeLastGivenLaneSlack + s_nNumMSToReachCenterOfLane)) 
		? 0.0f
		: (-fHalfLane / s_nNumMSToReachCenterOfLane) * (nTimeInMS - m_nTimeLastGivenLaneSlack) + fHalfLane;

	float retVal = FindTargetCoorsAndSpeed_NewIsolated(
		  handlingMinBrakeDistancef
		, handlingMaxBrakeDistancef
		, handlingMaxSpeedAtBrakeDistancef
		, driverCurveSpeedModifierf
		, driverNumSecondsToChangeLanesf
		, fDistanceAhead
		, fDistanceToProgressRoute
		, fOutMaxSpeed
		, vStartCoords
		, vOutTargetCoords
		, vVehiclePos
		, vVehicleVel
		, bUseSpeedAtCurrentPosition
		, bUseStringPulling
		, bRender
		, iOverrideEndPointIndex
		, fMaxDistanceAllowedFromLaneCenterInLaneSpace
		, fDriverInverseRacingModifier);


	if ( pVehicle->InheritsFromBoat() )
	{
		// minimizes boats cutting back and forth across path
		static float s_BoatPathTolerance = 2.0f;
		static float s_BoatPathToleranceRacing = 0.5f;
		float fPathTolerance = s_BoatPathTolerance;
		if ( pVehicle->m_nVehicleFlags.bIsRacing )
		{
			fPathTolerance = s_BoatPathToleranceRacing;
		}

		ModifyTargetForPathTolerance(vOutTargetCoords, *pVehicle, fPathTolerance);
	}

	if ( pVehicle->InheritsFromBike() )
	{
		// minimizes bikes cutting back and forth across path
		static float s_BikePathTolerance = 0.5f;
		static float s_BikePathToleranceRacing = 0.15f;
		float fPathTolerance = s_BikePathTolerance;
		if ( pVehicle->m_nVehicleFlags.bIsRacing )
		{
			fPathTolerance = s_BikePathToleranceRacing;
		}

		ModifyTargetForPathTolerance(vOutTargetCoords, *pVehicle, fPathTolerance);
	}

	return retVal;
}


float CVehicleFollowRouteHelper::FindTargetCoorsAndSpeed_NewIsolated(
		const float& handlingMinBrakeDistancef,
		const float& handlingMaxBrakeDistancef,
		const float& handlingMaxSpeedAtBrakeDistancef,
		const float& driverCurveSpeedModifierf,
		const float driverNumSecondsToChangeLanesf,
		const float& fDistanceAhead,
		const float& fDistanceToProgressRoute,
		float &fMaxSpeedInOut,
		Vec3V_In vStartCoords,
		Vec3V_InOut vOutTargetCoords,
		Vec3V_In /*vVehiclePos*/,
		Vec3V_In vVehicleVel,
		bool bUseSpeedAtCurrentPosition,
		const bool bUseStringPulling,
		const bool BANK_ONLY(bRender),
		const int iOverrideEndPointIndex,
		const float fMaxDistanceAllowedFromLaneCenterInLaneSpace,
		const float fInverseRacingModifier
		)
{
	m_PathWaypointDataInitialized = true;

	bool recompute = (m_TargetPointForPathWaypoints != GetCurrentTargetPoint()) || fMaxDistanceAllowedFromLaneCenterInLaneSpace > 0.0f;

	// If the last call to RecomputePathThroughLanes() stored a line we're supposed to be
	// close to in order for the path to be valid, check if we've diverged from it.
	if(!recompute && m_GotCurrentLane)
	{
		const float a = m_CurrentLaneA;
		const float b = m_CurrentLaneB;
		const float c = m_CurrentLaneC;
		const float x = vStartCoords.GetXf();
		const float y = vStartCoords.GetYf();

		// Compute the signed distance from the line, as a fraction
		// of the lane width (the coefficients have been scaled to support this).
		const float lateralDist = a*x + b*y + c;

		// MAGIC! If we've diverged from the line by more than 25% of the lane width,
		// we will recompute.
		static float s_LateralLaneDistThreshold = 0.25f;

		if(fabsf(lateralDist) > s_LateralLaneDistThreshold)
		{
			recompute = true;
		}

		// This can be enabled to draw the line, near the vehicle.
#if 0
		const float t = -(c + a*x + b*y)/(a*a + b*b);
		const float nearestX = x + a*t;
		const float nearestY = y + b*t;

		static float s_DrawDist = 10.0f;
		const float unnorm = 1.0f/sqrtf(a*a + b*b);
		const float seg1X = nearestX - unnorm*b*s_DrawDist;
		const float seg1Y = nearestY + unnorm*a*s_DrawDist;
		const float seg2X = nearestX + unnorm*b*s_DrawDist;
		const float seg2Y = nearestY - unnorm*a*s_DrawDist;

		const float z = vStartCoords.GetZf();
		Vector3 seg1(seg1X, seg1Y, z + 0.5f);
		Vector3 seg2(seg2X, seg2Y, z + 0.5f);
		grcDebugDraw::Line(seg1, seg2, Color_white);
#endif

	}

	// FFTODO: should probably not do this all the time now.
	PrefetchBuffer<sizeof(CRoutePoint)*CVehicleFollowRouteHelper::MAX_ROUTE_SIZE>(m_RoutePoints);

	// FFTODO: Check if we should make use of the lane-adjusted route points for this.
	// FFTODO: Also, check if we need to do this more frequently, it's not very accurate right now.
	RecomputePathThroughLanes(recompute, driverNumSecondsToChangeLanesf, vStartCoords, vVehicleVel, fMaxDistanceAllowedFromLaneCenterInLaneSpace);

	int iCurrentTargetPoint = GetCurrentTargetPoint();
	aiAssert(iCurrentTargetPoint >= -0x8000 && iCurrentTargetPoint <= 0x7fff);
	m_TargetPointForPathWaypoints = (s16)iCurrentTargetPoint;

	const int iNumPoints = m_iNumPoints;

	if(Unlikely(iCurrentTargetPoint >= iNumPoints))
	{
		// Note: not entirely sure that we are using the right index below, looks a bit suspicious. /FF
		const s32 iIterationStart = rage::Max(iCurrentTargetPoint - 1, 0);

		//make sure we initialize this to something, in case our current target position is the last on our list
		Vec3V vTargetCoords = m_RoutePoints[iCurrentTargetPoint].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		//add the lane offset to the output position
		vTargetCoords += m_RoutePoints[iIterationStart].GetLaneCenterOffset();

		vOutTargetCoords = vTargetCoords;

		// fMaxSpeedInOut already has the value we would expect.

		return 0.0f;
	}

	ScalarV scDistanceAhead = LoadScalar32IntoScalarV(fDistanceAhead);

	const ScalarV scZero(V_ZERO);


	//as we traverse the route, any time we reach a segment whose slope is less than the 
	//slope of the segment before it, save the index
// 	s32 iLastSegWithSlopeDecrease = -1;
// 	float fLastSegmentsSlopeSaved = 0.0f;
// 
// 	if (iIterationStart > 0)
// 	{
// 		ScalarV scDistBetweenPreviousPoints = Dist(m_vPathWaypointsAndSpeed[iIterationStart].GetXYZ(), m_vPathWaypointsAndSpeed[iIterationStart-1].GetXYZ());
// 		fLastSegmentsSlopeSaved = BoolV(scDistBetweenPreviousPoints >= ScalarV(V_FLT_EPSILON)).Getb() ? ((m_vPathWaypointsAndSpeed[iIterationStart].GetXYZ().GetZ() - m_vPathWaypointsAndSpeed[iIterationStart-1].GetXYZ().GetZ()) / scDistBetweenPreviousPoints).Getf() : 0.0f;
// 	}

	ScalarV scDistanceSearchedAhead(V_ZERO);

	if (bUseStringPulling && m_JunctionStringPullDistAdd)
	{
		//this isn't technically string pulling, as it was too slow to do the tests every frame.
		//now we just artificially increase the lookahead to cut corners when specified

		const Vec3V vCurrentTarget1 = m_JunctionStringPullDistAddPoint1;
		const Vec3V vCurrentTarget2 = m_JunctionStringPullDistAddPoint2;
		fwGeom::fwLine line(RCC_VECTOR3(vCurrentTarget1), RCC_VECTOR3(vCurrentTarget2));
		Vec3V vClosestPoint;
		line.FindClosestPointOnLineV(vStartCoords, vClosestPoint);
		scDistanceAhead += Mag(vCurrentTarget2 - vClosestPoint);
	}

	bool bProgressRoute = false;
	bool bSkippedSegments = false;
	Vec3V vPrevTarget1Modified;

	// For the first node, add the distance to the start position onto the lookahead
	ScalarV scDistFromStartCached;

	// Compare the distance to the current segment with the distance to the next segment, and see
	// if we should advance.
	do
	{
		taskAssert(iCurrentTargetPoint >= m_FirstValidElementOfPathWaypointsAndSpeed);

		const Vec3V vPrevTarget1Original = m_vPathWaypointsAndSpeed[iCurrentTargetPoint].GetXYZ();
		const Vec3V vCurrentTarget1Modified = m_vPathWaypointsAndSpeed[iCurrentTargetPoint + 1].GetXYZ();

		//if the distance between points is 0, the t value of vStartCoords will be NaN, not 0
		if (IsGreaterThanAll(DistSquared(vPrevTarget1Original, vCurrentTarget1Modified), ScalarV(V_FLT_EPSILON)))
		{
			fwGeom::fwLine line(RCC_VECTOR3(vPrevTarget1Original), RCC_VECTOR3(vCurrentTarget1Modified));
			line.FindClosestPointOnLineV(vStartCoords, vPrevTarget1Modified);
		}
		else
		{
			vPrevTarget1Modified = vPrevTarget1Original;
		}

		// Note: we used to have this and subtract it below, but it was set to zero in practice:
		//	static const ScalarV sscMinDistanceToLine(V_ZERO);

		const ScalarV scDistFromStart = Max( MagXY(vPrevTarget1Modified - vStartCoords) /* - sscMinDistanceToLine */, scZero );

		const ScalarV scInverseRacingModifier(fInverseRacingModifier);
		scDistanceSearchedAhead += (scDistFromStart * scInverseRacingModifier);

		scDistFromStartCached = scDistFromStart;

		Vec3V vCurrentTarget2Modified;
		if(iCurrentTargetPoint < iNumPoints - 1)
		{
			 vCurrentTarget2Modified = m_vPathWaypointsAndSpeed[iCurrentTargetPoint + 2].GetXYZ();
		}
		else
		{
			 vCurrentTarget2Modified = m_vPathWaypointsAndSpeed[iNumPoints].GetXYZ();
		}

		//subtract the overlap between ourselves and the next segment from scDistanceSearchedAhead
		fwGeom::fwLine nextLine(RCC_VECTOR3(vCurrentTarget1Modified), RCC_VECTOR3(vCurrentTarget2Modified));
		Vec3V vNearestPoint2;
		if (IsGreaterThanAll(DistSquared(vCurrentTarget1Modified, vCurrentTarget2Modified),ScalarV(V_FLT_EPSILON)))
		{
			nextLine.FindClosestPointOnLineV(vStartCoords, vNearestPoint2);
		}
		else
		{
			vNearestPoint2 = vCurrentTarget1Modified;
		}

		// FFTODO: it's maybe unnecessary to compute the closest point on a line and then measure the
		// distance between that and the start of the line - could have just computed the length along
		// the line using a dot product.
		const ScalarV scDistAlongNextSegment = Dist(vNearestPoint2, vCurrentTarget1Modified);
		scDistanceSearchedAhead -= (ScalarV(V_TWO) * scDistAlongNextSegment);

		const ScalarV scDistFromNext = Max( MagXY(vNearestPoint2 - vStartCoords) /* - sscMinDistanceToLine */, scZero);
		if(IsLessThanOrEqualAll(scDistFromNext, scDistFromStart))
		{
			// Regardless of whether we actually advanced or not, consider ourselves progressed
			// (to skip the alternative test further down).
			bSkippedSegments = true;

			// If we are not near the end of the path yet, advance iCurrentTargetPoint right now
			// and do another iteration.
			if(iCurrentTargetPoint < iNumPoints - 2)
			{
				iCurrentTargetPoint++;

				// Don't include any distance along the segments that we are now
				// advancing past. I think this makes sense, we want to find a new position
				// ahead of where we are currently at, right?
				scDistanceSearchedAhead.ZeroComponents();
			}
			else
			{
				// Try to do the final route progression at the end of this function, like how
				// we used to do it. That's a bit awkward but should hopefully ensure that
				// we deal with the last segment in the same way as we used to.
				bProgressRoute = true;
				break;
			}
		}
		else
		{
			// Shouldn't advance, we are not closer to the next segment yet.
			break;
		}
	} while(1);	// Keep looping if we didn't hit the break condition.

	if(bSkippedSegments)
	{
		// If we may have changed iCurrentTargetPoint above, write it back. It shouldn't be read
		// back by anything below.
		SetCurrentTargetPoint(iCurrentTargetPoint);
	}
	else	// If we overshoot but cross the next segment, this will ensure the route progresses
	{
		if(/*!sbUseNewRouteAdvancementMethod &&*/ iCurrentTargetPoint + 1 < iNumPoints)
		{
			const Vec3V vPrevTarget1Original = m_vPathWaypointsAndSpeed[iCurrentTargetPoint + 1].GetXYZ();
			const Vec3V vCurrentTarget1Modified = m_vPathWaypointsAndSpeed[iCurrentTargetPoint + 2].GetXYZ();

			fwGeom::fwLine line(RCC_VECTOR3(vPrevTarget1Original), RCC_VECTOR3(vCurrentTarget1Modified));
			Vec3V vNearestPoint;

			if (IsGreaterThanAll(DistSquared(vPrevTarget1Original, vCurrentTarget1Modified),ScalarV(V_FLT_EPSILON)))
			{
				line.FindClosestPointOnLineV(vStartCoords, vNearestPoint);
			}
			else
			{
				vNearestPoint = vPrevTarget1Original;
			}
			// FFTODO: Consider improving the distance computation above, probably can do it faster.

			// Note: we could store this pre-squared, but it would be a bit less user friendly.
			const ScalarV distanceToProgressRouteV = LoadScalar32IntoScalarV(fDistanceToProgressRoute);
			const ScalarV distanceToProgressRouteSqV = Scale(distanceToProgressRouteV, distanceToProgressRouteV);

			if(IsLessThanOrEqualAll(MagSquared(FlattenXY(vCurrentTarget1Modified - vNearestPoint)), distanceToProgressRouteSqV))
			{
				bProgressRoute = true;
			}
		}
	}

	const ScalarV distanceSearchAheadResetValV = scDistanceSearchedAhead;
	const Vec3V prevTarget1ModifiedResetValV = vPrevTarget1Modified;

	Assert(iCurrentTargetPoint + 1 < MAX_ROUTE_SIZE + 1);
	const ScalarV distanceToFirstPointV = Dist(vPrevTarget1Modified, m_vPathWaypointsAndSpeed[iCurrentTargetPoint + 1].GetXYZ());

	Vec3V vTargetCoords;
	bool bTargetPosSet = false;

	for(int i = iCurrentTargetPoint; i < iNumPoints; i++ )
	{
		ScalarV scDistanceBetweenPoints;
		if(i == iCurrentTargetPoint)
		{
			scDistanceBetweenPoints = distanceToFirstPointV;
		}
		else
		{
			scDistanceBetweenPoints = LoadScalar32IntoScalarV(m_DistBetweenPathWaypoints[i]);
		}

		const ScalarV scDistanceLeft = scDistanceAhead - scDistanceSearchedAhead;

		if(iOverrideEndPointIndex >= 0)
		{
			if(i == iOverrideEndPointIndex || i == iOverrideEndPointIndex + 1)
			{
				const ScalarV scMinDistSearchedToOverrideEndPoint(V_FIVE);
				const unsigned int bOverridePointTooClose = IsLessThanAll(scDistanceSearchedAhead - scDistFromStartCached, scMinDistSearchedToOverrideEndPoint);
				if(bOverridePointTooClose)
				{
					//if we were too close to the overridden point, don't look further ahead than the minimum searched distance
					if (i == iOverrideEndPointIndex)
					{
						scDistanceAhead = rage::Min(scMinDistSearchedToOverrideEndPoint, scDistanceAhead);
					}
				}
				else
				{
					if(i == iOverrideEndPointIndex + 1)
					{
						if(i == iCurrentTargetPoint)
						{
							vTargetCoords = prevTarget1ModifiedResetValV;
						}
						else
						{
							vTargetCoords = m_vPathWaypointsAndSpeed[i].GetXYZ();
						}
						//vTargetCoords = vPrevTarget1Modified;
						bTargetPosSet = true;
						break;
					}
				}
			}
		}

//		if( (scDistanceBetweenPoints > scZero).Getb() && (scDistanceBetweenPoints > scDistanceLeft).Getb())
		const ScalarV nonNegDistanceLeftV = Max(scZero, scDistanceLeft);
		if(IsGreaterThanAll(scDistanceBetweenPoints, nonNegDistanceLeftV))
		{
			const ScalarV tV = InvScale(nonNegDistanceLeftV, scDistanceBetweenPoints);

			// Should hold up:
			//	Assert(IsCloseAll(Saturate(scDistanceLeft / scDistanceBetweenPoints), tV, ScalarV(0.00001f)));

			Vec3V prevPtV;
			if(i == iCurrentTargetPoint)
			{
				prevPtV = prevTarget1ModifiedResetValV;
			}
			else
			{
				prevPtV = m_vPathWaypointsAndSpeed[i].GetXYZ();
			}

			const Vec3V vCurrentTarget1Modified = m_vPathWaypointsAndSpeed[i + 1].GetXYZ();

			vTargetCoords = Lerp(tV, prevPtV, vCurrentTarget1Modified);
			bTargetPosSet = true;
			break;
		}
		scDistanceSearchedAhead += scDistanceBetweenPoints;
	}

	if(!bTargetPosSet)
	{
		// Note: this seems to be a very uncommon case, but seems to happen on occasion.
		taskAssert(iNumPoints >= m_FirstValidElementOfPathWaypointsAndSpeed);
		vTargetCoords = m_vPathWaypointsAndSpeed[iNumPoints].GetXYZ();
	}

#if __ASSERT

	//this case is handled below, so removing the assert
	//Assert(vTargetCoords.GetXf() >= WORLDLIMITS_XMIN && vTargetCoords.GetXf() <= WORLDLIMITS_XMAX);
	//Assert(vTargetCoords.GetYf() >= WORLDLIMITS_YMIN && vTargetCoords.GetYf() <= WORLDLIMITS_YMAX);
	//Assert(vTargetCoords.GetZf() >= WORLDLIMITS_ZMIN && vTargetCoords.GetZf() <= WORLDLIMITS_ZMAX);

	if (vTargetCoords.GetXf() < WORLDLIMITS_XMIN || vTargetCoords.GetXf() > WORLDLIMITS_XMAX
		|| vTargetCoords.GetYf() < WORLDLIMITS_YMIN || vTargetCoords.GetYf() > WORLDLIMITS_YMAX
		|| vTargetCoords.GetZf() < WORLDLIMITS_ZMIN || vTargetCoords.GetZf() > WORLDLIMITS_ZMAX)
	{
		static bool b_PrintedAlready = false;
		if (!b_PrintedAlready)
		{
			b_PrintedAlready = true;
			aiDisplayf("Current Target: %d", iCurrentTargetPoint);
			aiDisplayf("Target Pos Set: %s", (bTargetPosSet ? "true" : "false"));
			aiDisplayf("Num Points: %d", iNumPoints);
			aiDisplayf("First valid element: %d", m_FirstValidElementOfPathWaypointsAndSpeed);
			for(int i = 0; i <= iNumPoints; i++ )
			{
				aiDisplayf("i: %d, coords = %.2f, %.2f, %.2f", i, m_vPathWaypointsAndSpeed[i].GetXf()
					, m_vPathWaypointsAndSpeed[i].GetYf(), m_vPathWaypointsAndSpeed[i].GetZf());
			}
			aiDisplayf("m_RoutePoints:");
			for(int i = 0; i < iNumPoints; i++ )
			{
				aiDisplayf("i: %d, coords = %.2f, %.2f, %.2f", i, m_RoutePoints[i].GetPosition().GetXf()
					, m_RoutePoints[i].GetPosition().GetYf(), m_RoutePoints[i].GetPosition().GetZf());
			}

			aiAssertf(false, "B*2068266 RouteHelper: Invalid target coords calculated. See logs for details.");
		}
	}
#endif //__ASSERT

	scDistanceSearchedAhead = distanceSearchAheadResetValV;
	vPrevTarget1Modified = prevTarget1ModifiedResetValV;

	ScalarV scOutMaxSpeed(fMaxSpeedInOut); // copy back to fMaxSpeedInOut before returning!
	ScalarV scSpeedAtCurrentPos = scOutMaxSpeed;

	//if we're running a vehicle recording, and want to use our nodes' speeds,
	//look for the closest point
	BoolV vbUseSpeedAtCurrentPosition(V_FALSE);
	if (bUseSpeedAtCurrentPosition)
	{
		// FFTODO: Check up on this, if we should use the lane-adjusted array
		// instead, or if we can get away without doing this on every frame.

		//find the route point closest to our vehicle's root position,
		//since this is where the vehicle recording was sampled from
		const int closestPointToStartPos = FindClosestPointToPosition(vStartCoords);
		if (closestPointToStartPos >= 0)
		{
			scSpeedAtCurrentPos = m_RoutePoints[closestPointToStartPos].GetSpeedV();
			vbUseSpeedAtCurrentPosition = BoolV(V_TRUE);
		}
	}

	const ScalarV scCurveSpeedModifier = LoadScalar32IntoScalarV(driverCurveSpeedModifierf);
	const ScalarV minBrakeDistance = LoadScalar32IntoScalarV(handlingMinBrakeDistancef);
	const ScalarV handlingMaxBrakeDistanceV = LoadScalar32IntoScalarV(handlingMaxBrakeDistancef);
	const ScalarV handlingMaxSpeedAtBrakeDistanceV = LoadScalar32IntoScalarV(handlingMaxSpeedAtBrakeDistancef);
	const ScalarV minNodeSpeedAnywhereOnPathV = LoadScalar32IntoScalarV(m_MinPossibleNodeSpeedOnPath);
	ScalarV distOnRestOfPathV = LoadScalar32IntoScalarV(m_PathLengthSum);

	// Note: this could be precomputed, since it doesn't depend on the route data.
	const ScalarV brakeFactorV = InvScale(handlingMaxSpeedAtBrakeDistanceV, handlingMaxBrakeDistanceV);

	const ScalarV minNodeSpeedToUseV = SelectFT(vbUseSpeedAtCurrentPosition, minNodeSpeedAnywhereOnPathV, scSpeedAtCurrentPos);

	const int pathLengthSumFirstElement = m_PathLengthSumFirstElement;
	for(int i = pathLengthSumFirstElement; i < iCurrentTargetPoint; i++)
	{
		const ScalarV distBetweenPointsV = LoadScalar32IntoScalarV(m_DistBetweenPathWaypoints[i]);
		distOnRestOfPathV = Subtract(distOnRestOfPathV, distBetweenPointsV);
	}

	for(int i = iCurrentTargetPoint; i < iNumPoints; i++ )
	{
		ScalarV scDistanceBetweenPoints;
		scDistanceBetweenPoints = LoadScalar32IntoScalarV(m_DistBetweenPathWaypoints[i]);
		distOnRestOfPathV = Subtract(distOnRestOfPathV, scDistanceBetweenPoints);

		if(i == iCurrentTargetPoint)
		{
			scDistanceBetweenPoints = distanceToFirstPointV;
		}

		scDistanceSearchedAhead += scDistanceBetweenPoints;

		const ScalarV scDistanceFromCorner = Max(scDistanceSearchedAhead - minBrakeDistance, scZero);
		const ScalarV scSpeedAtCurrentRoutePoint = m_vPathWaypointsAndSpeed[i].GetW();
		const ScalarV scNodeSpeed = SelectFT(vbUseSpeedAtCurrentPosition, scSpeedAtCurrentRoutePoint, scSpeedAtCurrentPos);

		const ScalarV maxSpeedApproachingCorner = Scale(scCurveSpeedModifier, AddScaled(scNodeSpeed, scDistanceFromCorner, brakeFactorV));

		Assertf((maxSpeedApproachingCorner >= scZero).Getb(), "maxSpeedApproachingCorner: %.2f, scNodeSpeed: %.2f, scDistanceFromCorner: %.2f \
		, scCurveSpeedModifier: %.2f", maxSpeedApproachingCorner.Getf(), scNodeSpeed.Getf(), scDistanceFromCorner.Getf(), scCurveSpeedModifier.Getf());

		scOutMaxSpeed = Min(scOutMaxSpeed, maxSpeedApproachingCorner);

#if __BANK
		if(bRender)
		{
			Vec3V vPrevTarget1Modified;
			if(i == iCurrentTargetPoint)
			{
				vPrevTarget1Modified = prevTarget1ModifiedResetValV;
			}
			else
			{
				vPrevTarget1Modified = m_vPathWaypointsAndSpeed[i].GetXYZ();
			}
			const Vec3V vCurrentTarget1Modified = m_vPathWaypointsAndSpeed[i + 1].GetXYZ();
			RenderCarAiTaskInfo(i, vPrevTarget1Modified, vCurrentTarget1Modified, maxSpeedApproachingCorner, scDistanceSearchedAhead);
		}
#endif //__BANK

		const ScalarV minMaxSpeedApproachingAnyFutureCornerV = scCurveSpeedModifier*(minNodeSpeedToUseV + scDistanceFromCorner*brakeFactorV);
		if(IsGreaterThanAll(minMaxSpeedApproachingAnyFutureCornerV, scOutMaxSpeed))
		{
			scDistanceSearchedAhead = Add(scDistanceSearchedAhead, distOnRestOfPathV);

#if __BANK
			if(bRender)
			{
				// We are skipping the rest of the path so we won't compute the speeds and distances for the points,
				// but we pass in -1 for them which will cause ?? to be displayed instead.
				for(int j = i + 1; j < iNumPoints; j++)
				{
					const Vec3V vPrevTarget1Modified = m_vPathWaypointsAndSpeed[j].GetXYZ();
					const Vec3V vCurrentTarget1Modified = m_vPathWaypointsAndSpeed[j + 1].GetXYZ();
					RenderCarAiTaskInfo(j, vPrevTarget1Modified, vCurrentTarget1Modified, ScalarV(V_NEGONE), ScalarV(V_NEGONE));
				}
			}
#endif	// __BANK
			break;
		}
	}


	// Write back values
	vOutTargetCoords = vTargetCoords;
	if(bProgressRoute && iCurrentTargetPoint < iNumPoints - 1)
	{
		SetCurrentTargetPoint(iCurrentTargetPoint + 1);
	}
	fMaxSpeedInOut = scOutMaxSpeed.Getf();

	return scDistanceSearchedAhead.Getf();
}

void CVehicleFollowRouteHelper::ModifyTargetForPathTolerance(Vec3V& io_targetPos, const CVehicle& in_Vehicle, float in_Tolerance ) const
{
	if ( in_Tolerance > 0.0f )
	{
		// TODO: Probably check this to make sure we don't go out of range:
		if ( m_iCurrentTargetPoint + 1 < m_iNumPoints && m_iCurrentTargetPoint >= m_FirstValidElementOfPathWaypointsAndSpeed )
		{
			Vec3V vClosestPointOnPath3d;
			const Vec3V vCurrent = m_vPathWaypointsAndSpeed[m_iCurrentTargetPoint].GetXYZ();
			const Vec3V vNext = m_vPathWaypointsAndSpeed[m_iCurrentTargetPoint + 1].GetXYZ();
			const Vec3V vCurrentSegment = vNext - vCurrent;
			const Vec3V vVehiclePos3d = GetVehicleBonnetPosition(&in_Vehicle, false);

			// make sure we have valid segment
			if ( ( MagSquared(vCurrentSegment.GetXY()) > ScalarV(FLT_EPSILON) ).Getb() )
			{
				if ( m_iCurrentTargetPoint > 0 )
				{
					//if the distance between points is 0, the t value of vStartCoords will be NaN, not 0
					if (IsGreaterThanAll(DistSquared(vCurrent, vNext), ScalarV(V_FLT_EPSILON)))
					{
						fwGeom::fwLine line(RCC_VECTOR3(vNext), RCC_VECTOR3(vCurrent));
						line.FindClosestPointOnLineV(vVehiclePos3d, vClosestPointOnPath3d);
					}
					else
					{
						vClosestPointOnPath3d = vCurrent;
					}
				}
				else 
				{
					vClosestPointOnPath3d = m_vPathWaypointsAndSpeed[m_iCurrentTargetPoint].GetXYZ();
				}

				const Vec2V vUnitSegment = Normalize(vCurrentSegment.GetXY()); 
				const Vec2V vUnitSegmentRight = Vec2V(vUnitSegment.GetY(), -vUnitSegment.GetX());

				const Vec2V vVehiclePos = in_Vehicle.GetTransform().GetPosition().GetXY();
				const Vec2V vClosestPointOnPath = vClosestPointOnPath3d.GetXY();
				const Vec2V vClosesPosToVehicle = vVehiclePos - vClosestPointOnPath;

				// basically assume the segment is s_MaxDistanceFromPath wide.  compute
				// closest point within that range and apply a scalar to pull us back to the center
				const ScalarV vProjectRight = Dot(vUnitSegmentRight, vClosesPosToVehicle);

				static float s_OffsetScalar = .6f;
				const ScalarV vSignRight = IsGreaterThan(vProjectRight, ScalarV(0.0f)).Getb() ? ScalarV(1.0f) : ScalarV(-1.0f);
				const ScalarV vRightOffset = Min(ScalarV(in_Tolerance), Abs( vProjectRight * ScalarV(s_OffsetScalar) ) ) * vSignRight;
				const Vec2V vOffset = vRightOffset * vUnitSegmentRight;

				io_targetPos += Vec3V(vOffset.GetXf(), vOffset.GetYf(), 0.0f);
			}
		}
	}
}


// this is just going to return vector from previous point to current point
const Vector3& CVehicleFollowRouteHelper::ComputeCurrentSegmentTangent(Vector3& o_Tangent) const
{
	if ( m_iCurrentTargetPoint < m_iNumPoints )
	{
		if ( m_iCurrentTargetPoint + 1 < m_iNumPoints )
		{
			o_Tangent = VEC4V_TO_VECTOR3(m_vPathWaypointsAndSpeed[m_iCurrentTargetPoint+1] - m_vPathWaypointsAndSpeed[m_iCurrentTargetPoint]);
			o_Tangent.Normalize();		
		}
		else 
		{
			o_Tangent = VEC4V_TO_VECTOR3(m_vPathWaypointsAndSpeed[m_iCurrentTargetPoint] - m_vPathWaypointsAndSpeed[m_iCurrentTargetPoint-1]);
			o_Tangent.Normalize();		
		}
	}
	return o_Tangent;
}


void CVehicleFollowRouteHelper::PrecalcInfoForRoutePoint(const int i, RoutePrecalcInfo& out) const
{
	const int iNumPoints = m_iNumPoints;

	// Calculate the vector between the two target positions
	const Vec3V vCurrentTarget1Before = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset);
	const Vec3V vCurrentTarget2Before = m_RoutePoints[i+1].GetPositionWithRandomOffset(m_vRandomNodeOffset);

	const bool bQualifiesAsJunction = m_RoutePoints[i].m_bIsSlipJunction;
	bool bJunctionIsStraightAhead = false;
	bool bSharpTurn = true;
	if (i > 0 && bQualifiesAsJunction)
	{
		Vector3 vOldToNew = i > 1 && m_RoutePoints[i-1].m_bIgnoreForNavigation ? VEC3V_TO_VECTOR3(m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset) - m_RoutePoints[i-2].GetPositionWithRandomOffset(m_vRandomNodeOffset))
			: VEC3V_TO_VECTOR3(vCurrentTarget1Before - m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset));
		Vector3 vNewToFuture = i < iNumPoints-2 && m_RoutePoints[i].m_bIgnoreForNavigation ? VEC3V_TO_VECTOR3(m_RoutePoints[i+2].GetPositionWithRandomOffset(m_vRandomNodeOffset) - vCurrentTarget2Before)
			: VEC3V_TO_VECTOR3(vCurrentTarget2Before - vCurrentTarget1Before);
		Vector2 vOldToNew2D(vOldToNew.x, vOldToNew.y);
		Vector2 vNewToFuture2D(vNewToFuture.x, vNewToFuture.y);
		
		float fLeftness = 0.0f, fDotProd = 0.0f;
		const int turnDir = CPathNodeRouteSearchHelper::FindPathDirectionInternal(vOldToNew2D, vNewToFuture2D, &bSharpTurn, &fLeftness, CPathNodeRouteSearchHelper::sm_fDefaultDotThresholdForUsingJunctionNode, &fDotProd);
		bJunctionIsStraightAhead = (turnDir == BIT_TURN_STRAIGHT_ON);
	}

	//fix up our direction heading into the junction
	Vec3V vCurrentTarget2Modified = vCurrentTarget2Before;
	if (i > 0 && m_RoutePoints[i].m_bIgnoreForNavigation && m_RoutePoints[i+1].m_bJunctionNode)
	{
		Vec3V vPrevDir = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset) - m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		vPrevDir = NormalizeSafe(vPrevDir, Vec3V(V_X_AXIS_WONE));

		const ScalarV svCurrentLength = Mag(vCurrentTarget2Before - vCurrentTarget1Before);

		// FFTODO: Change to AddScaled()
		vCurrentTarget2Modified = vCurrentTarget1Before + (vPrevDir * svCurrentLength);
	}

	//if we want to use string pulling at junctions, do it here.
	//this basically involves testing the line segment between the junction entry node
	//and its exit node, and if that segment doesn't cross any road edges,
	//set the position of the junction node to be along that segment
	bool bUsedIntersectionPoint = false;
	Vec3V vCurrentTarget1Modified = vCurrentTarget1Before;
	//bool bAttemptedJunctionFixup = false;
	if (i > 0 && bQualifiesAsJunction && !bJunctionIsStraightAhead && !bSharpTurn)
	{
		//bAttemptedJunctionFixup = true;

		Vec3V vEntryStart = m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		Vec3V vExitStart = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		Vec3V vEntryDir = FlattenXY(m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset) - m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset));
		Vec3V vExitDir = FlattenXY(m_RoutePoints[i+1].GetPositionWithRandomOffset(m_vRandomNodeOffset) - m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset));


		if (i > 1 && m_RoutePoints[i-1].m_bIgnoreForNavigation)
		{
			//handle when the entry link of a junction is NoNav
			vEntryStart = m_RoutePoints[i-2].GetPositionWithRandomOffset(m_vRandomNodeOffset);
			vEntryDir = FlattenXY(m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset) - m_RoutePoints[i-2].GetPositionWithRandomOffset(m_vRandomNodeOffset));
		}
		if (i < iNumPoints-2 && m_RoutePoints[i].m_bIgnoreForNavigation)
		{
			//handle when the exit link of a junction is NoNav
			vExitStart = m_RoutePoints[i+1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
			vExitDir = FlattenXY(m_RoutePoints[i+2].GetPositionWithRandomOffset(m_vRandomNodeOffset) - m_RoutePoints[i+1].GetPositionWithRandomOffset(m_vRandomNodeOffset));
		}

		//actually, it doesn't look like normalization is required for these guys. awesome.
		//vEntryDir = NormalizeFast(vEntryDir);
		//vExitDir = NormalizeFast(vExitDir);

		//now find the intersection point
		Vec3V vIntersectionPoint(V_ZERO);
		if (geom2D::TestLineToLineFastXY(vEntryStart, vEntryDir, vExitStart, vExitDir, vIntersectionPoint))
		{
			vCurrentTarget1Modified = vIntersectionPoint;
			bUsedIntersectionPoint = true;

			//grcDebugDraw::Sphere(vCurrentTarget1, 0.25f, Color_OrangeRed, false);
		}
	}

	//	Vec3V Dir = FlattenXY(vCurrentTarget2 - vCurrentTarget1);
	//	Dir = Normalize(Dir);
	const Vec3V Dir = NormalizeSafe(FlattenXY(vCurrentTarget2Modified - vCurrentTarget1Modified), Vec3V(V_X_AXIS_WONE));

	// Calculate and apply the lane centre offsets to both target positions
	int laneCentreIndex = i;
	Vec3V vLaneCentreDir = Dir;

	Vec3V vLaneCentreCurrentTarget1 = vCurrentTarget1Modified;
	Vec3V vLaneCentreCurrentTarget2 = vCurrentTarget2Modified;

	//if the current node is pointing to an ignore for nav link, we can't trust its direction
	//so get the direction between the two previous nodes.
	//we know this is safe since if i-1 is ignore for nav as well as i, we would have skipped it via the 
	//continue above.
	//exception: we already fixed up our points via the junction-specific code above
	if (!bUsedIntersectionPoint && laneCentreIndex > 0 && m_RoutePoints[laneCentreIndex].m_bIgnoreForNavigation)
	{
		vLaneCentreCurrentTarget2 = m_RoutePoints[laneCentreIndex].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		vLaneCentreCurrentTarget1 = m_RoutePoints[laneCentreIndex-1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		laneCentreIndex--;
		vLaneCentreDir = FlattenXY(vLaneCentreCurrentTarget2 - vLaneCentreCurrentTarget1);
		vLaneCentreDir = NormalizeSafe(vLaneCentreDir, Vec3V(V_X_AXIS_WONE));
	}
	else if (bUsedIntersectionPoint && laneCentreIndex < iNumPoints-2 && m_RoutePoints[laneCentreIndex].m_bIgnoreForNavigation)
	{
		vLaneCentreCurrentTarget2 = m_RoutePoints[laneCentreIndex+2].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		vLaneCentreCurrentTarget1 = m_RoutePoints[laneCentreIndex+1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		laneCentreIndex++;
		vLaneCentreDir = FlattenXY(vLaneCentreCurrentTarget2 - vLaneCentreCurrentTarget1);
		vLaneCentreDir = Normalize(vLaneCentreDir);
	}

	out.m_vCurrentTarget1 = vCurrentTarget1Modified;
	out.m_vCurrentTarget2 = vCurrentTarget2Modified;
	out.m_vDir = Dir;
	out.m_vLaneCentreCurrentTarget1 = vLaneCentreCurrentTarget1;
	out.m_vLaneCentreCurrentTarget2 = vLaneCentreCurrentTarget2;
	out.m_vLaneCentreDir = vLaneCentreDir;
	out.m_iLaneCentreIndex = laneCentreIndex;
	out.m_bQualifiesAsJunction = bQualifiesAsJunction;
	out.m_bJunctionIsStraightAhead = bJunctionIsStraightAhead;
	out.m_bSharpTurn = bSharpTurn;
	out.m_bSingleTrack = m_RoutePoints[i].m_bIsSingleTrack;
}


void CVehicleFollowRouteHelper::RecomputePathThroughLanes(
		bool bRecompute,
		const float driverNumSecondsToChangeLanesf,
		Vec3V_In vStartCoords,
		Vec3V_In vVehicleVel,
		const float fMaxDistanceAllowedFromLaneCenter)
{
	TUNE_GROUP_BOOL(CAR_AI, ObeyRequiredLanes_New, true);
	TUNE_GROUP_BOOL(CAR_AI, RecalculateLaneForEachSegment_New, false);

	ResetDebugMaintainLaneForNodes();

	const int iCurrentTargetPointBefore = GetCurrentTargetPoint();
	const int iNumPoints = m_iNumPoints;

	const ScalarV scZero(V_ZERO);

	const s32 iIterationStart = rage::Max(GetCurrentTargetPoint() - 1, 0);

	s32 iNextRequiredLane = -1;
	s32 iNodeForNextRequiredLane = -1;
	ScalarV scDistToLaneChange(V_ZERO);
	// FFTODO: Consider removing this as an option.
	if(ObeyRequiredLanes_New)
	{
		for (int i = iCurrentTargetPointBefore; i < iNumPoints-1; i++)
		{
			if (m_RoutePoints[i].m_iRequiredLaneIndex > -1 && !m_RoutePoints[i].m_bIgnoreForNavigation)
			{
				iNextRequiredLane = m_RoutePoints[i].m_iRequiredLaneIndex;
				iNodeForNextRequiredLane = i;
				scDistToLaneChange = Dist(vStartCoords, m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset));
				bRecompute = true;
				break;
			}
		}
	}

	if (!bRecompute)
	{
		return;
	}

	m_GotCurrentLane = false;

	const ScalarV scNextRequiredLane = IntToFloatRaw<0>(ScalarVFromS32(iNextRequiredLane));

	const ScalarV scVehicleSpeed = Mag(vVehicleVel);

	const ScalarV scNumSecondsToChangeLanes(driverNumSecondsToChangeLanesf);

	// FFTODO: See if can invert this, to avoid having to divide by it.
	const ScalarV scDistToChangeLanesAtCurrentSpeed = Scale(scNumSecondsToChangeLanes, scVehicleSpeed);

	// FFTODO: Make this go in the opposite direction, and stop once we've
	// found a non-ignored position.
	Vec3V vLastNonIgnoredPosition(V_ZERO);
	for (int i = 0; i < iIterationStart; i++)
	{
		if (!m_RoutePoints[i].m_bIgnoreForNavigation
			|| (i > 0 && m_RoutePoints[i].m_bIgnoreForNavigation && !m_RoutePoints[i-1].m_bIgnoreForNavigation))
		{
			vLastNonIgnoredPosition = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset);
			vLastNonIgnoredPosition += m_RoutePoints[i].GetLaneCenterOffset();
		}
	}

	Vec3V vPrevTarget1(V_ZERO);
	Vec3V vPrevTarget2(V_ZERO);

	m_JunctionStringPullDistAdd = false;

	// Keep track of the first element in m_vPathWaypointsAndSpeed that we initialized, since we don't
	// always start from zero, and it would be bad if some code ended up accessing it out of range.
	aiAssert(iIterationStart >= -0x8000 && iIterationStart <= 0x7fff);
	m_FirstValidElementOfPathWaypointsAndSpeed = (s16)iIterationStart;

	// FFTODO: Check if this value ever gets used, may want to not even store it.
	m_vPathWaypointsAndSpeed[iIterationStart] = VECTOR4_TO_VEC4V(Vector4(vPrevTarget1.GetXf(), vPrevTarget1.GetYf(), vPrevTarget1.GetZf(), m_RoutePoints[iIterationStart].GetSpeed()));

	bool bPrevJunctionStraightAhead = false;
	const ScalarV scMaxDistanceAllowedFromLaneCenterLeft(fMaxDistanceAllowedFromLaneCenter);
	const ScalarV scMaxDistanceAllowedFromLaneCenterRightBase(fMaxDistanceAllowedFromLaneCenter);
	const ScalarV scLaneSpaceBorderForMinMaxLanes(V_QUARTER);
	const ScalarV scAbsoluteLaneMinimum = rage::Max(scZero - scLaneSpaceBorderForMinMaxLanes, scZero - scMaxDistanceAllowedFromLaneCenterLeft);
	//const ScalarV scMaxDistanceAllowedFromLaneCenterRightSingleTrack(rage::Max(fMaxDistanceAllowedFromLaneCenterRight, 0.5f));

	for(int i = iIterationStart; i < iNumPoints-1; i++ )
	{
		RoutePrecalcInfo precalc;
		PrecalcInfoForRoutePoint(i, precalc);

		const Vec3V Dir = precalc.m_vDir;
		const Vec3V vLaneCentreCurrentTarget1 = precalc.m_vLaneCentreCurrentTarget1;
		// const Vec3V vLaneCentreCurrentTarget2 = precalc.m_vLaneCentreCurrentTarget2;
		const Vec3V vLaneCentreDir = precalc.m_vLaneCentreDir;
		const int laneCentreIndex = precalc.m_iLaneCentreIndex;
		const bool bQualifiesAsJunction = precalc.m_bQualifiesAsJunction;
		const bool bJunctionIsStraightAhead = precalc.m_bJunctionIsStraightAhead;
		const bool bSharpTurn = precalc.m_bSharpTurn;
		//const bool bSingleTrack = precalc.m_bSingleTrack;

		//TODO: bring this back
// 		const ScalarV scMaxDistanceAllowedFromLaneCenterRight = bSingleTrack 
// 			? scMaxDistanceAllowedFromLaneCenterRightSingleTrack
// 			: scMaxDistanceAllowedFromLaneCenterRightBase;
		const ScalarV scMaxDistanceAllowedFromLaneCenterRight = scMaxDistanceAllowedFromLaneCenterRightBase;

		bool ignored = false;	// FFTODO: check if we ended up needing this

		if (i > 0 && m_RoutePoints[i-1].m_bIgnoreForNavigation && m_RoutePoints[i].m_bIgnoreForNavigation
			&& (!bQualifiesAsJunction || bJunctionIsStraightAhead))
		{
			// FFTODO: Consider using a separate bool here rather than checking for zero, or make use of vector select.
			if (IsZeroAll(vPrevTarget1))
			{
				const Vec3V vCurrentTarget2Before = m_RoutePoints[i+1].GetPositionWithRandomOffset(m_vRandomNodeOffset);

				vPrevTarget1 = IsZeroAll(vLastNonIgnoredPosition) ? m_vPosWhenLastConstructedNodeList : vLastNonIgnoredPosition;
				vPrevTarget2 = vCurrentTarget2Before;

#if __ASSERT
				static const Vec3V svWorldLimitsMin(WORLDLIMITS_XMIN, WORLDLIMITS_YMIN, WORLDLIMITS_ZMIN);
				static const Vec3V svWorldLimitsMax(WORLDLIMITS_XMAX, WORLDLIMITS_YMAX, WORLDLIMITS_ZMAX);
				static const spdAABB sWorldLimitsBox(svWorldLimitsMin, svWorldLimitsMax);
				if (!sWorldLimitsBox.ContainsPoint(vPrevTarget1))
				{
					aiAssertf(false, "B*2068266 RouteHelper: Invalid target from zero prev target. LastNon %f %f %f ConstructedPos %f %f %f ", 
								vLastNonIgnoredPosition.GetXf(), vLastNonIgnoredPosition.GetYf(), vLastNonIgnoredPosition.GetZf(),
								m_vPosWhenLastConstructedNodeList.GetXf(), m_vPosWhenLastConstructedNodeList.GetYf(), m_vPosWhenLastConstructedNodeList.GetZf());
				}
#endif
			}

			//just use whatever came ahead
			//we need to set lane index even if we're ignoring the lane,
			//so the "maintain lane" code has the proper value to work with
			ScalarV scLane(V_ZERO);
			scLane = m_RoutePoints[i-1].GetLane();

			// FFTODO: Do we really need to do this? Seems like we shouldn't have set the lane in the route to an invalid value in the first place.
			ScalarV scMaxLaneNum = IntToFloatRaw<0>(ScalarVFromS32(rage::Max(m_RoutePoints[i].NumLanesToUseForPathFollowing()-1, 0)));
			const ScalarV scAbsoluteLaneMaximum = rage::Min(scMaxLaneNum + scLaneSpaceBorderForMinMaxLanes, scMaxLaneNum + scMaxDistanceAllowedFromLaneCenterRight);
			scLane = Clamp(scLane, scAbsoluteLaneMinimum, scAbsoluteLaneMaximum);

			//Cache the lane.
			m_RoutePoints[i].SetLane(scLane);

			ignored = true;
		}
		else
		{
			// FFTODO: Maybe put this option back in here again?
			//	if (bUseStringPulling)
			if(1)
			{
				if (i==iCurrentTargetPointBefore && i > 0 && bQualifiesAsJunction && !bJunctionIsStraightAhead && !bSharpTurn)
				{
					// TODO: Check up a bit more on this - not entirely sure that these are the right coordinates.
					// Also, things would be easier if we could just use the positions after the lane offset has been applied, etc.
					m_JunctionStringPullDistAddPoint1 = precalc.m_vCurrentTarget1;
					m_JunctionStringPullDistAddPoint2 = precalc.m_vCurrentTarget2;
					m_JunctionStringPullDistAdd = true;
				}
			}

			//if this link is the one we're currently on (m_CurrentTargetPos - 1), we want to use i and i+1
			//to determine which lane we're in.
			//if we're on m_CurrentTargetPos or further though, we want to be on the 

			const ScalarV scCentralLaneWidth = m_RoutePoints[laneCentreIndex].GetCentralLaneWidthV();
			const ScalarV scLaneWidth = m_RoutePoints[laneCentreIndex].GetLaneWidthV();

			//we don't want to change lanes in an intersection, but we also don't
			//want to neccesarily set a required lane--if there are multiple straight-ahead
			//lanes we don't care which one we take, just that we don't change lanes.
			//so if this is a junction node, just use the lane from our index
			bool bShouldSkipLaneRecalculation = false;
			if (i >= iCurrentTargetPointBefore && i>=2)
			{
				const int iNumLanesAheadInPreEntranceLink = m_RoutePoints[i-2].m_bHasLeftOnlyLane 
					? m_RoutePoints[i-2].NumLanesToUseForPathFollowing() -1 
					: m_RoutePoints[i-2].NumLanesToUseForPathFollowing();
				const int iNumLanesAheadInPrevLink = m_RoutePoints[i-1].m_bHasLeftOnlyLane 
					? m_RoutePoints[i-1].NumLanesToUseForPathFollowing() -1 
					: m_RoutePoints[i-1].NumLanesToUseForPathFollowing();
				const int iNumLanesAheadInThisLink = m_RoutePoints[i].NumLanesToUseForPathFollowing();

				if ((m_RoutePoints[i].m_bJunctionNode && bJunctionIsStraightAhead 
					&& iNumLanesAheadInThisLink == iNumLanesAheadInPrevLink
					&& iNumLanesAheadInPrevLink > 1) 
					|| 
					(m_RoutePoints[i-1].m_bJunctionNode && bPrevJunctionStraightAhead 
					&& iNumLanesAheadInPrevLink ==  iNumLanesAheadInPreEntranceLink
					&& iNumLanesAheadInPrevLink > 1))
				{
					bShouldSkipLaneRecalculation = true;
				}
			}	

#if __DEV
			m_bDebugMaintainLaneForNode[i] = bShouldSkipLaneRecalculation;
#endif //__DEV

			//if we have a required lane, apply it here
			ScalarV scLane(V_ZERO);

			if (ObeyRequiredLanes_New && m_RoutePoints[laneCentreIndex].m_iRequiredLaneIndex > -1 && !m_RoutePoints[laneCentreIndex].m_bIgnoreForNavigation)
			{
				// FFTODO: Check up on if it's really a good idea to use vectors for this int.
				scLane = IntToFloatRaw<0>(ScalarVFromS32(m_RoutePoints[laneCentreIndex].m_iRequiredLaneIndex));
			}
			else if (!bShouldSkipLaneRecalculation && (RecalculateLaneForEachSegment_New || laneCentreIndex <= iCurrentTargetPointBefore+1 
				|| (laneCentreIndex > 0 && m_RoutePoints[laneCentreIndex-1].m_bIgnoreForNavigation)))
			{
				const ScalarV scMaxLaneNum = IntToFloatRaw<0>(ScalarVFromS32(rage::Max(m_RoutePoints[laneCentreIndex].NumLanesToUseForPathFollowing()-1, 0)));
				const ScalarV scAbsoluteLaneMaximum = rage::Min(scMaxLaneNum + scLaneSpaceBorderForMinMaxLanes, scMaxLaneNum + scMaxDistanceAllowedFromLaneCenterRight);

				//otherwise try and follow the lane closest to us
				//const float fTotalRoadWidth = m_RoutePoints[i+1].NumLanesToUseForPathFollowing() * m_RoutePoints[i+1].m_fLaneWidth;
				const Vec3V vTestStart = FlattenXY(vLaneCentreCurrentTarget1);
				//const Vec3V vTestEnd =  FlattenXY(vLaneCentreCurrentTarget2);

				//if we're further ahead on our route, test the end point of the previous link, instead of our vehicle position itself
				Vec3V vTestPos;
				if(laneCentreIndex > 0 && laneCentreIndex >= iCurrentTargetPointBefore)
				{
					if (!m_RoutePoints[laneCentreIndex-1].m_bIgnoreForNavigation)
					{
						vTestPos = FlattenXY(vPrevTarget2);
					}
					else
					{
						vTestPos = FlattenXY(vPrevTarget1);

						//char szText[8];
						//sprintf(szText, "%d", i - 1);
						//grcDebugDraw::Text(vPrevTarget1, Color_purple, 0, 0, szText, true, -1);
						///////////////////////////////////
					}
				}
				else
				{
					vTestPos = FlattenXY(vStartCoords);

					// If we have a lane width and we are using the start coordinates (and it's not because m_bIgnoreForNavigation
					// is set for some route point further along the path), store the equation of a 2D line through our current
					// position.
					// FFTODO: This could be optimized further.
					if(laneCentreIndex <= 0 || (laneCentreIndex < iCurrentTargetPointBefore && !m_RoutePoints[laneCentreIndex-1].m_bIgnoreForNavigation))
					{
						const float laneWidth = scLaneWidth.Getf();
						if(laneWidth > 0.0f)
						{
							const float xx = vTestPos.GetXf();
							const float yy = vTestPos.GetYf();
							float a = vLaneCentreDir.GetYf();
							float b = -vLaneCentreDir.GetXf();
							float c = -a*xx - b*yy;
							a /= laneWidth;
							b /= laneWidth;
							c /= laneWidth;
							m_CurrentLaneA = a;
							m_CurrentLaneB = b;
							m_CurrentLaneC = c;
							m_GotCurrentLane = true;
						}
					}
				}

				const Vec3V perpDir = GetFromTwo<Vec::Y1, Vec::X2, Vec::Z1>(vLaneCentreDir, Negate(vLaneCentreDir)); // (y, -x, 0.0)
				const ScalarV scDistToNodeLine = Dot(perpDir, vTestPos - vTestStart);

				// FFTODO: Change to AddScaled(), etc.
				const ScalarV scDistFromCenterLaneEdge = scDistToNodeLine
					- scCentralLaneWidth
					+  (m_RoutePoints[laneCentreIndex].m_bIsSingleTrack ? scZero : scLaneWidth * ScalarV(V_HALF));
				// FFTODO: scDistFromCenterLaneEdge could be moved into the next block

				// FFTODO: Try to remove this vector branch.
				if (IsGreaterThanAll(scLaneWidth, scZero))
				{
					// FFTODO: Multiply by inverse rather than dividing here?
					const ScalarV scActualPositionOnRoad = scDistFromCenterLaneEdge / scLaneWidth;

					// FFTODO: scLane is sometimes overwritten, see below - may want to delay computation until after the if check
					//we see a lane change up ahead
					if (iNodeForNextRequiredLane != -1 && laneCentreIndex < iNodeForNextRequiredLane)
					{
						//clamp our lane offset to the lane change cone from this point
						const ScalarV scMaxLaneDiff = (scDistToLaneChange / Max(scDistToChangeLanesAtCurrentSpeed, ScalarV(V_FLT_EPSILON))) 
							/*+ ScalarV(V_HALF)*/;
						const ScalarV scCurrentLane = m_RoutePoints[laneCentreIndex].m_bIsSingleTrack 
							? scActualPositionOnRoad 
							:scActualPositionOnRoad - ScalarV(V_HALF);
						const bool bGoingRight = IsGreaterThanAll(scNextRequiredLane, scCurrentLane) != 0;
						static const ScalarV scLerpAmount(0.2f);

 						if (IsLessThanAll(Abs(scNextRequiredLane - scCurrentLane), scMaxLaneDiff))
 						{
							ScalarV scClosestLane = Clamp(RoundToNearestInt(scCurrentLane), scZero, scMaxLaneNum);

							if (IsTrue(Abs(scClosestLane - scCurrentLane) <= ScalarV(0.2f)))
							{
								scLane = scCurrentLane;
							}
							else
							{
								scLane = Lerp(scLerpAmount, scCurrentLane, scClosestLane);
							}
 						}
 						else
 						{
 							scLane = Lerp(scLerpAmount, scCurrentLane, scNextRequiredLane + (bGoingRight ? -scMaxLaneDiff : scMaxLaneDiff));
 						}

						//scLane = Clamp(scCurrentLane, scNextRequiredLane-scMaxLaneDiff, scNextRequiredLane+scMaxLaneDiff);
					}		
					else
					{
						//scLane = RoundToNearestIntNegInf(scActualPositionOnRoad);
						const ScalarV scCurrentLane = m_RoutePoints[laneCentreIndex].m_bIsSingleTrack 
							? scActualPositionOnRoad 
							:scActualPositionOnRoad - ScalarV(V_HALF);
						ScalarV scClosestLane = Clamp(RoundToNearestInt(scCurrentLane), scZero, scMaxLaneNum);

						if (IsTrue(Abs(scClosestLane - scCurrentLane) <= ScalarV(0.2f)))
						{
							scClosestLane = scCurrentLane;
						}

						scLane = Clamp(scCurrentLane, scClosestLane-scMaxDistanceAllowedFromLaneCenterLeft, scClosestLane+scMaxDistanceAllowedFromLaneCenterRight);
					}

					scLane = Clamp(scLane, scAbsoluteLaneMinimum, scAbsoluteLaneMaximum);
				}
				else
				{
					scLane = scZero;
				}
			}
			else
			{
				//just use whatever came ahead
				Assert(laneCentreIndex > 0);
				scLane = m_RoutePoints[laneCentreIndex-1].GetLane();

				// FFTODO: Do we really need to do this? Seems like we shouldn't have set the lane in the route to an invalid value in the first place.
				ScalarV scMaxLaneNum = IntToFloatRaw<0>(ScalarVFromS32(rage::Max(m_RoutePoints[laneCentreIndex].NumLanesToUseForPathFollowing()-1, 0)));
				const ScalarV scAbsoluteLaneMaximum = rage::Min(scMaxLaneNum + scLaneSpaceBorderForMinMaxLanes, scMaxLaneNum + scMaxDistanceAllowedFromLaneCenterRight);
				scLane = Clamp(scLane, scAbsoluteLaneMinimum, scAbsoluteLaneMaximum);
			}

			// FFTODO: Change to AddScaled()
			const ScalarV laneCentreOffset = scCentralLaneWidth + scLane * scLaneWidth;

			// FFTODO: looks like this has already been computed in some code paths above
			Vec3V perpDir = GetFromTwo<Vec::Y1, Vec::X2, Vec::Z1>(vLaneCentreDir, Negate(vLaneCentreDir)); // (y, -x, 0.0)

			const Vec3V vlaneCentreOffsetVec = Scale(perpDir, laneCentreOffset);

			//Vec3V zLift(0.0f, 0.0f, 0.1f);
			//grcDebugDraw::Line(vCurrentTarget1+zLift, vCurrentTarget2+zLift, Color_green, Color_yellow);

			// FFTODO: Try to get rid of the whole cached center offset thing, I don't think we need it.

			//Cache the lane offset and lane.
			m_RoutePoints[i].SetLaneCenterOffsetAndLane(vlaneCentreOffsetVec.GetXY(), scLane);

			/*const*/ Vec3V vCurrentTarget1Modified = precalc.m_vCurrentTarget1;
			/*const*/ Vec3V vCurrentTarget2Modified = precalc.m_vCurrentTarget2;
			vCurrentTarget1Modified += vlaneCentreOffsetVec;
			vCurrentTarget2Modified += vlaneCentreOffsetVec;

			if( i >= iCurrentTargetPointBefore )
			{
				// Because the direction between the nodes is used to calculate the offsets, we are constantly working
				// one node ahead.  Calculating the line between the previous start node and our start node, this means
				// The lane offset calculated and applied is consistent as you traverse the route

				// Extend the line out slightly and allow it to be capped to the previous end point
				if (i > 0 && !m_RoutePoints[i-1].m_bIgnoreForNavigation)
				{
					vCurrentTarget1Modified = AddScaled(vCurrentTarget1Modified, Dir, ScalarV(V_NEGFIVE)); //	vCurrentTarget1 += Dir*-5.0f;
				}

				fwGeom::fwLine line(RCC_VECTOR3(vCurrentTarget1Modified), RCC_VECTOR3(vCurrentTarget2Modified));
				line.FindClosestPointOnLineV(vPrevTarget2, vCurrentTarget1Modified);
			}

			if (!m_RoutePoints[i].m_bIgnoreForNavigation
				|| (i > 0 && m_RoutePoints[i].m_bIgnoreForNavigation && !m_RoutePoints[i-1].m_bIgnoreForNavigation))
			{
				vLastNonIgnoredPosition = vCurrentTarget1Modified;
			}

			vPrevTarget1 = vCurrentTarget1Modified;
			vPrevTarget2 = vCurrentTarget2Modified;
		}

		bPrevJunctionStraightAhead = bJunctionIsStraightAhead;

		// FFTODO: Clean up this assignment
		m_vPathWaypointsAndSpeed[i + 1] = VECTOR4_TO_VEC4V(Vector4(vPrevTarget1.GetXf(), vPrevTarget1.GetYf(), vPrevTarget1.GetZf(), m_RoutePoints[i + 1].GetSpeed()));
		m_bPathWaypointsIgnored[i] = ignored;
	}

	if (iNumPoints > 1)
	{
		m_bPathWaypointsIgnored[iNumPoints-1] = m_bPathWaypointsIgnored[iNumPoints-2];
	}
	if (iNumPoints > 0)
	{
		m_vPathWaypointsAndSpeed[iNumPoints] = VECTOR4_TO_VEC4V(Vector4(vPrevTarget2.GetXf(), vPrevTarget2.GetYf(), vPrevTarget2.GetZf(), m_RoutePoints[iNumPoints-1].GetSpeed()));;
		m_bPathWaypointsIgnored[iNumPoints] = m_bPathWaypointsIgnored[iNumPoints-1];
	}

	// Precompute stuff needed by the new version of FindTargetCoorsAndSpeed():
	if(m_PathWaypointDataInitialized)
	{
		Assert(iIterationStart <= 0xff);
		m_PathLengthSumFirstElement = (u8)iIterationStart;

		float minPossibleNodeSpeed = LARGE_FLOAT;

		ScalarV distSumV(V_ZERO);
		for(int i = iIterationStart; i < iNumPoints /*- 1*/; i++)
		{
			const float nodeSpeed = m_vPathWaypointsAndSpeed[i].GetWf();
			const Vec3V vPrevTarget1Original = m_vPathWaypointsAndSpeed[i].GetXYZ();
			const Vec3V vCurrentTarget1Modified = m_vPathWaypointsAndSpeed[i + 1].GetXYZ();
			const ScalarV scDistanceBetweenPoints = Dist(vPrevTarget1Original, vCurrentTarget1Modified);
#if !RSG_DURANGO
			distSumV = Add(distSumV, scDistanceBetweenPoints);
			Assert(i >= 0 && i < MAX_ROUTE_SIZE);
			StoreScalar32FromScalarV(m_DistBetweenPathWaypoints[i], scDistanceBetweenPoints);
#else	// (B*1302013) Re-arrange code to avoid compiler bug saving incorrect value to m_DistBetweenPathWaypoints[i]
			Assert(i >= 0 && i < MAX_ROUTE_SIZE);
			StoreScalar32FromScalarV(m_DistBetweenPathWaypoints[i], scDistanceBetweenPoints);
			distSumV = Add(distSumV, scDistanceBetweenPoints);
#endif // To be removed if/when Microsoft fix compiler bug

			minPossibleNodeSpeed = Min(minPossibleNodeSpeed, nodeSpeed);
		}

		StoreScalar32FromScalarV(m_PathLengthSum, distSumV);

		m_MinPossibleNodeSpeedOnPath = minPossibleNodeSpeed;
	}
}

#if __BANK && DEBUG_DRAW
void CVehicleFollowRouteHelper::DebugDrawRoute(const CVehicle & rVehicle, Vec3V_In vVehTestPos) const
{
	bool bIsFocusOrNoFocus = CDebugScene::FocusEntities_IsEmpty() || CDebugScene::FocusEntities_IsInGroup(&rVehicle);
	if((CVehicleAILodManager::ms_bDisplayDummyPath || CVehicleAILodManager::ms_bDisplayDummyPathDetailed) && bIsFocusOrNoFocus)
	{
		int iCurrentPoint = FindCurrentAtPoint(vVehTestPos);

		for(int i=0; i<m_iNumPoints-1; i++)
		{
			Vec3V vNode1Pos = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset);
			Vec3V vNode2Pos = m_RoutePoints[i+1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
			Color32 lineColor;
			if(i<iCurrentPoint)
			{
				lineColor = Color_pink;
			}
			else if(i<GetCurrentTargetPoint())
			{
				if(CVehicleAILodManager::ms_bDisplayDummyPathDetailed)
				{
					lineColor = Color_white;
				}
				else
				{
					continue;
				}
			}
			else
			{
				lineColor = Color_purple;
			}
			grcDebugDraw::Line(vNode1Pos,vNode2Pos,lineColor,-1);
			const Vec3V vOffset(0.0f,0.0f,0.10f);
			vNode1Pos += vOffset;
			vNode2Pos += vOffset;
			grcDebugDraw::Line(vNode1Pos,vNode2Pos,lineColor,-1);
			grcDebugDraw::Cross(vNode1Pos,0.25f,Color_grey70,-1);
		}
	}
}
#endif

int CVehicleFollowRouteHelper::FindCurrentAtPoint(Vec3V_In vVehiclePos, int iNumNodesToSearchBeforeCurrent) const
{
	const int iIterationStart = rage::Max(GetCurrentTargetPoint() - iNumNodesToSearchBeforeCurrent, 0);
	const int iNumNodesToSearchForward =  rage::Max(2, iNumNodesToSearchBeforeCurrent+1);
	const int newCurrentAtPoint = FindClosestPointToPosition(vVehiclePos, Max(iIterationStart-1,0), Min(iIterationStart+iNumNodesToSearchForward, (int)m_iNumPoints));
	Assert(newCurrentAtPoint >= 0);
	return newCurrentAtPoint;
}

#if __BANK

void CVehicleFollowRouteHelper::RenderCarAiTaskInfo(int i,
		Vec3V_In vPrevTarget1, Vec3V_In vCurrentTarget1,
		ScalarV_In maxSpeedApproachingCorner,
		ScalarV_In scDistanceSearchedAhead) const
{
	TUNE_GROUP_BOOL(CAR_AI, RenderCruiseTaskImmediately_New, false);
	TUNE_GROUP_BOOL(CAR_AI, RenderCruiseTaskText_New, false);

	Vec3V zAxis(V_Z_AXIS_WONE);

	Color32 pathColorPrev = /*m_RoutePoints[i].m_iRequiredLaneIndex == -1 ?*/ Color_blue /*: Color_red*/;
	Color32 pathColorCurr = GetRoutePoints()[i].m_iRequiredLaneIndex == -1 /*|| m_RoutePoints[i].m_bIgnoreForNavigation*/ ? Color_blue : Color_red;

#if __DEV
	if (m_bDebugMaintainLaneForNode[i])
	{
		pathColorCurr = Color_green;
	}
#endif //__DEV

	if (!RenderCruiseTaskImmediately_New)
	{
		//CVehicle::ms_debugDraw.AddCross(VECTOR3_TO_VEC3V(vPrevTarget1 + ZAXIS), 0.5f, Color_blue, 1, 0);
		CVehicle::ms_debugDraw.AddCross(vPrevTarget1 + zAxis, 0.5f, pathColorPrev, 1, 0);
		CVehicle::ms_debugDraw.AddCross(vCurrentTarget1 + zAxis, 0.5f, pathColorCurr, 1, 4);
		CVehicle::ms_debugDraw.AddLine(vPrevTarget1 + zAxis, vCurrentTarget1 + zAxis, pathColorCurr, 1);
	}
	else
	{
		//grcDebugDraw::Cross(VECTOR3_TO_VEC3V(vPrevTarget1 + ZAXIS), 0.5f, Color_blue);
		grcDebugDraw::Cross(vPrevTarget1 + zAxis, 0.5f, pathColorPrev);
		grcDebugDraw::Cross(vCurrentTarget1 + zAxis, 0.5f, pathColorCurr);
		grcDebugDraw::Line(vPrevTarget1 + zAxis, vCurrentTarget1 + zAxis, pathColorPrev, pathColorCurr, 1);
	}

	if (RenderCruiseTaskText_New)
	{
		float lane = GetRoutePoints()[i].GetLane().Getf();

		char text[128];
		if(maxSpeedApproachingCorner.Getf() >= 0.0f)
		{
			formatf(text, "Speed: %.2f, %.2f, Distance: %.2f, Lane: %.2f, i: %d", GetRoutePoints()[i].GetSpeed(), maxSpeedApproachingCorner.Getf(), scDistanceSearchedAhead.Getf(), lane, i);
		}
		else
		{
			formatf(text, "Speed: %.2f, ??, Distance: ??, Lane: %.2f, i: %d", GetRoutePoints()[i].GetSpeed(), lane, i);
		}
		CVehicle::ms_debugDraw.AddText(vCurrentTarget1, 0, 0, text, Color_white, 1);
	}
}

#endif	// __BANK

float CVehicleFollowRouteHelper::EstimatePathLength() const
{
	ScalarV svLength(V_ZERO);
	for (int i = 0; i < m_iNumPoints-1; i++)
	{
		const Vec3V& vCurrPoint = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		const Vec3V& vNextPoint = m_RoutePoints[i+1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		svLength += DistFast(vCurrPoint, vNextPoint);
	}

	return svLength.Getf();
}

float CVehicleFollowRouteHelper::FindDistanceToEndOfPath(const CVehicle* pVehicle) const
{
	Vec3V vStartCoords = pVehicle->GetVehiclePosition();

	ScalarV svDistSearchedAhead(V_ZERO);
	if (GetCurrentTargetPoint() < 1)
	{
		return 0.0f;
	}
	for( s32 i = GetCurrentTargetPoint(); i < m_iNumPoints-1; i++ )
	{
		Vec3V vPrevPoint = m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		Vec3V vCurrPoint = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		if( i == GetCurrentTargetPoint() )
		{
			//add the distance between us and the next point along the current link
			Vec3V vClosestPoint(V_ZERO);
			fwGeom::fwLine line(RCC_VECTOR3(vPrevPoint), RCC_VECTOR3(vCurrPoint));
			line.FindClosestPointOnLineV(vStartCoords, vClosestPoint);

			svDistSearchedAhead += DistXYFast(vClosestPoint, vCurrPoint);
		}
		else
		{
			//add the distance between points
			svDistSearchedAhead += DistXYFast(vPrevPoint, vCurrPoint);
		}
	}

	return svDistSearchedAhead.Getf();
}

float CVehicleFollowRouteHelper::FindDistanceToGivenNode(const CVehicle* pVehicle, const s32 iGivenNode, const bool bDriveInReverse) const
{
	Vec3V vStartCoords = GetVehicleBonnetPosition(pVehicle, bDriveInReverse);

	Assert(iGivenNode >= 0 && iGivenNode < m_iNumPoints);

	ScalarV svDistSearchedAhead(V_ZERO);
	if (GetCurrentTargetPoint() < 1)
	{
		return 0.0f;
	}

	for( s32 i = GetCurrentTargetPoint(); i <= iGivenNode; i++ )
	{
		Vec3V vPrevPoint = m_RoutePoints[i-1].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		Vec3V vCurrPoint = m_RoutePoints[i].GetPositionWithRandomOffset(m_vRandomNodeOffset);
		if( i == GetCurrentTargetPoint() )
		{
			//add the distance between us and the next point along the current link
			Vec3V vClosestPoint(V_ZERO);
			fwGeom::fwLine line(RCC_VECTOR3(vPrevPoint), RCC_VECTOR3(vCurrPoint));
			line.FindClosestPointOnLineV(vStartCoords, vClosestPoint);

			svDistSearchedAhead += DistXYFast(vClosestPoint, vCurrPoint);

			//also add in our distance to the line
			svDistSearchedAhead += DistXYFast(vClosestPoint, vStartCoords);
		}
		else
		{
			//add the distance between points
			svDistSearchedAhead += DistXYFast(vPrevPoint, vCurrPoint);
		}
	}

	return svDistSearchedAhead.Getf();
}

// void CVehicleFollowRouteHelper::GetPointAtDistanceAlongPath(const float fDistance, Vector3& vPointOut)
// {
// 	ScalarV svDistSearchedAhead(V_ZERO);
// 	for( s32 i = GetCurrentTargetPoint(); i < m_iNumPoints-1; i++ )
// 	{
// 	}
// }

bool CVehicleLaneChangeHelper::ShouldChangeLanesForThisEntity(const CVehicle* const pVeh, const CPhysical* const pAvoidEnt, const float fOriginalDesiredSpeed, const bool bDriveInReverse) const
{
	if (!pAvoidEnt)
	{
		return false;
	}

	float fDesiredSpeed = fOriginalDesiredSpeed;

	//TODO: bring this back--it doesn't work great right now because
	//if we've already started slowing down for someone in front of us, we'll never
	//decide to pass them since the subtask's desired speed will be below the threshold
	//if the subtask thinks we should go slower than the parent task, listen to it
	// 	CTaskVehicleGoToPointAutomobile* pSubTask = static_cast<CTaskVehicleGoToPointAutomobile*>(FindSubTaskOfType(CTaskTypes::TASK_VEHICLE_GOTO_POINT_AUTOMOBILE));
	// 	if (pSubTask)
	// 	{
	// 		const float fSubtaskDesiredSpeed = pSubTask->GetCruiseSpeed();
	// 		fDesiredSpeed = MIN(fSubtaskDesiredSpeed, fOriginalDesiredSpeed);
	// 	}

	//also, don't swerve if they're moving roughly perpendicular to us at a reasonable clip
	const Vec3V vehDriveDir = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, bDriveInReverse);

	const float fDistInFrontOfUs = Dot(pAvoidEnt->GetTransform().GetPosition() - pVeh->GetVehiclePosition(), vehDriveDir).Getf();

	const CVehicle* pAvoidEntAsVeh = pAvoidEnt->GetIsTypeVehicle() ? static_cast<const CVehicle*>(pAvoidEnt) : NULL;
	//compare velocities.  we'll only switch lanes if we're trying to go at least as fast
	//(let's say >10% faster) as what we're trying to avoid
	const Vector3 otherVehVelocity = pAvoidEntAsVeh ? pAvoidEntAsVeh->GetAiVelocity() : pAvoidEnt->GetVelocity();
	const float otherVehSpeed = pAvoidEntAsVeh ? pAvoidEntAsVeh->GetAiXYSpeed() : otherVehVelocity.Mag();

	if (fDistInFrontOfUs > pVeh->GetBoundingBoxMax().y*2.0f && fDesiredSpeed < otherVehSpeed * ms_fChangeLanesVelocityRatio)
	{
		return false;
	}

	Vector3 otherVehVelocityNormalized = otherVehVelocity;
	otherVehVelocityNormalized.NormalizeSafe();
	if (fabsf(VEC3V_TO_VECTOR3(vehDriveDir).Dot(otherVehVelocityNormalized)) < 0.7f && otherVehSpeed >= 1.0f)
	{
		return false;
	}

	if (pAvoidEntAsVeh)
	{
		const CVehicleIntelligence::AvoidanceCache& rCache = pVeh->GetIntelligence()->GetAvoidanceCache();
		const CVehicleIntelligence::AvoidanceCache& rOtherCache = pAvoidEntAsVeh->GetIntelligence()->GetAvoidanceCache();
		//don't change lanes for guys in our own convoy, regardless of position
		if (rCache.m_pTarget && (rCache.m_pTarget.Get() == rOtherCache.m_pTarget.Get()))
		{
			return false;
		}

		//don't change lanes for any entity we've got in the avoidance cache
		if (rCache.m_pTarget == pAvoidEntAsVeh)
		{
			return false;
		}

		//if this is a trailer, see if we're excluded from avoiding its parent,
		//and apply that to the trailer as well
		if (pAvoidEntAsVeh->InheritsFromTrailer())
		{
			CVehicle* pAvoidEntsParent = CTaskVehicleGoToPointWithAvoidanceAutomobile::GetTrailerParentFromVehicle(pAvoidEntAsVeh);
			if (pAvoidEntsParent && pAvoidEntsParent == rCache.m_pTarget)
			{
				return false;
			}
		}

		//don't change lanes if this guy is following us
		if (rOtherCache.m_pTarget == pVeh)
		{
			return false;
		}

		//if we're told to follow a trailer, and pAvoidEnt is its parent,
		//also exclude that
		if (rCache.m_pTarget && rCache.m_pTarget->GetIsTypeVehicle())
		{
			const CVehicle* pCacheTargetVehicle = static_cast<const CVehicle *>(rCache.m_pTarget.Get());
			if(pCacheTargetVehicle->InheritsFromTrailer() && pCacheTargetVehicle->m_nVehicleFlags.bHasParentVehicle)
			{
				if (pCacheTargetVehicle->GetAttachParentVehicle() == pAvoidEntAsVeh
					|| pCacheTargetVehicle->GetDummyAttachmentParent() == pAvoidEntAsVeh)
				{
					return false;
				}
			}
		}

		//if pAvoidEnt is trying to follow my trailer,
		//don't change lanes for it
		const CTrailer* pMyTrailer = pVeh->GetAttachedTrailer();
		if (pMyTrailer && pMyTrailer == rOtherCache.m_pTarget)
		{
			return false;
		}
		if (pVeh->HasDummyAttachmentChildren() && pVeh->GetDummyAttachmentChild(0) == rOtherCache.m_pTarget)
		{
			return false;
		}
	}

	return true;
}

bool CVehicleLaneChangeHelper::LaneChangesAllowedForThisVehicle(const CVehicle* const pVeh
	, const CVehicleFollowRouteHelper* const pFollowRoute) const
{
	const CPed* pDriver = pVeh->GetDriver();
	const bool bIsMission = pVeh->PopTypeIsMission() || (pDriver && pDriver->PopTypeIsMission());
	const bool bIsAggressiveEnough = CDriverPersonality::WillChangeLanes(pDriver, pVeh);
	if (!bIsMission && !bIsAggressiveEnough)
	{
		return false;
	}

	if (pFollowRoute->GetNumPoints() <= 1)
	{
		return false;
	}

	//if there's only one lane, we can't change lanes
	const int iCurrentPoint = pFollowRoute->GetCurrentTargetPoint();
	//Assert(iCurrentPoint > 0 && iCurrentPoint < pFollowRoute->GetNumPoints());
	if (iCurrentPoint <= 0 || iCurrentPoint >= pFollowRoute->GetNumPoints())
	{
		return false;
	}

	const CRoutePoint* pTargetPoint = &pFollowRoute->GetRoutePoints()[iCurrentPoint];
	//CRoutePoint* pCurrentPoint = &pFollowRoute->GetRoutePoints()[iCurrentPoint-1];

	if (pTargetPoint->m_iNoLanesThisSide <= 1)
	{
		return false;
	}

	//don't change lanes if there's an upcoming junction
	const CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();
	for (int i = iCurrentPoint; i < pFollowRoute->GetNumPoints(); i++)
	{
		const CPathNode* pNode = pNodeList->GetPathNode(i);
		if (pNode && 
			((pNode->IsHighway() && pNode->m_2.m_slipJunction)
			|| pNode->IsJunctionNode()))
		{
			return false;
		}
	}

	return true;
}
//this is responsible for checking conditions that depend on the entity in order to determine if we shoudl change lanes or not
//stuff like relative velocity, how long we've been waiting, etc
bool CVehicleLaneChangeHelper::CanSwitchLanesToAvoidObstruction(const CVehicle* const pVeh, const CVehicleFollowRouteHelper* const pFollowRoute, const CPhysical* const pAvoidEnt, const float fDesiredSpeed, const bool bDriveInReverse, u32 timeStepInMs) 
{
	//this should be called every frame in order to update the timers correctly.
	//pAvoidEnt may not necessarily be updated every frame, but that's ok

	//this should be checked at a higher level now
// 	if (!IsDrivingFlagSet(DF_ChangeLanesAroundObstructions))
// 	{
// 		return false;
// 	}
// 	if (!LaneChangesAllowedForThisVehicle(pVeh))
// 	{
// 		return false;
// 	}

	if (!pAvoidEnt)
	{
		return false;
	}

	//if there's only one lane, we can't change lanes
	const int iCurrentPoint = pFollowRoute->GetCurrentTargetPoint();
	Assert(iCurrentPoint > 0 && iCurrentPoint < pFollowRoute->GetNumPoints());
	const CRoutePoint* pTargetPoint = &pFollowRoute->GetRoutePoints()[iCurrentPoint];
	//CRoutePoint* pCurrentPoint = &pFollowRoute->GetRoutePoints()[iCurrentPoint-1];

	if (!ShouldChangeLanesForThisEntity(pVeh, pAvoidEnt, fDesiredSpeed, bDriveInReverse))
	{
		return false;
	}

	const int iCurrentLane = rage::round(pTargetPoint->GetLane().Getf());
	if (iCurrentLane < pTargetPoint->m_iNoLanesThisSide - 1)
	{
		m_bWaitingToUndertakeThisFrame = true;
		m_TimeWaitingForUndertake += timeStepInMs;
		m_TimeWaitingForUndertake = Min(m_TimeWaitingForUndertake, ms_TimeBeforeUndertake);
	}

	if (iCurrentLane > 0)
	{
		m_bWaitingToOvertakeThisFrame = true;
		m_TimeWaitingForOvertake += timeStepInMs;
		m_TimeWaitingForOvertake = Min(m_TimeWaitingForOvertake, ms_TimeBeforeOvertake);
	}

#if ENABLE_LANECHANGE_DEBUGGING
	HelperDisplayLaneChangeDebugArrow(pVeh, pAvoidEnt->GetTransform().GetPosition(), Color_LightBlue);
#endif

	return (m_bWaitingToOvertakeThisFrame && m_TimeWaitingForOvertake >= ms_TimeBeforeOvertake)
		|| (m_bWaitingToUndertakeThisFrame && m_TimeWaitingForUndertake >= ms_TimeBeforeUndertake);
}

int HelperFindSegmentAtDistance(const CVehicleFollowRouteHelper* pFollowRoute, const CPhysical* const pObstacle, const float fDistanceToDesiredLaneChange)
{
	Assert(pFollowRoute);
	Assert(pObstacle);

	//assume straight line
	return pFollowRoute->FindClosestPointToPosition(pObstacle->GetTransform().GetPosition() 
		+ (VECTOR3_TO_VEC3V(pObstacle->GetVelocity()) * ScalarV(fDistanceToDesiredLaneChange))
			, pFollowRoute->GetCurrentTargetPoint()+1);
}

void CVehicleLaneChangeHelper::UpdatePotentialLaneChange(const CVehicle* const pVeh, CVehicleFollowRouteHelper* pFollowRoute, const CPhysical* const pObstacle, const float fDesiredSpeed, const float fDistToInitialObstruction, const int iObstructedSegment, const bool bDriveInReverse)
{
	Assert(pFollowRoute);

	//const bool bWantsToChangeLanes = bCanProcessLaneChange && pGoToTask->WantsToChangeLanesForTraffic();
	const bool bWantsToOvertake = WantsToOvertake();
	const bool bWantsToUndertake = WantsToUndertake();

	if (!bWantsToOvertake && !bWantsToUndertake)
	{
		return;
	}

	//if we have a lane change coming up ahead, look for a better lane
	//if we find one, set the required lane index in our route.
	//this will allow the code below to pick up on it and change lanes smoothly
	//TODO: handle what happens if we re-plan in the middle of a lane change
	//PF_START(AI_LaneChange);

	const float fOriginalObstructingEntitySpeed = pObstacle->GetIsTypeVehicle() ? ((CVehicle*)pObstacle)->GetAiXYSpeed() : pObstacle->GetVelocity().XYMag();
	//const float fDistToInitialObstruction = sqrt(fDistSqrToObstruction);
	const int iTargetPoint = pFollowRoute->GetCurrentTargetPoint();
	const int iBlockedLane = rage::round(pFollowRoute->GetRoutePoints()[iTargetPoint].GetLane().Getf());
// 	int iObstructedSegment = iTargetPoint;
// 
// 	//find out where the obstruction is
// 	for (int i = iTargetPoint; i < m_iNumPoints; i++)
// 	{
// 		const float fStraightLineDist = DistXYFast(pVeh->GetVehiclePosition(), m_RoutePoints[i].GetPosition()).Getf();
// 		if (fStraightLineDist > fDistToInitialObstruction)
// 		{
// 			iObstructedSegment = i;
// 			break;
// 		}
// 	}

	////////////////////////////////////

	int potentialLeftLane = iBlockedLane, potentialRightLane = iBlockedLane;
	float fDistToObstructionInTestingLeftLane = 0.0f, fDistToObstructionInTestingRightLane = 0.0f;
	bool bFoundGoodLaneToLeft = false, bFoundGoodLaneToRight = false;
	CPhysical* pNewObstructingEntityLeft = NULL, *pNewObstructingEntityRight = NULL;
	float fObstructionInLeftLaneSpeed = 0.0f, fObstructionInRightLaneSpeed = 0.0f;

	while (potentialLeftLane > 0 && !bFoundGoodLaneToLeft)
	{
		--potentialLeftLane;
		for (int i = iTargetPoint; i < pFollowRoute->GetNumPoints(); i++)
		{
			if (IsSegmentObstructed(i, pFollowRoute, pVeh, potentialLeftLane, fDesiredSpeed, fDistToObstructionInTestingLeftLane, pNewObstructingEntityLeft, fObstructionInLeftLaneSpeed, bDriveInReverse))
			{
				break;
			}
		}

		if (fDistToObstructionInTestingLeftLane > fDistToInitialObstruction 
			&& fObstructionInLeftLaneSpeed > fOriginalObstructingEntitySpeed * 1.05f)
		{
			bFoundGoodLaneToLeft = true;
		}
#if ENABLE_LANECHANGE_DEBUGGING
		else if (pNewObstructingEntityLeft)
		{
			HelperDisplayLaneChangeDebugSphere(pVeh, VEC3V_TO_VECTOR3(pNewObstructingEntityLeft->GetTransform().GetPosition()), Color_red);
		}
#endif //ENABLE_LANECHANGE_DEBUGGING
	}	

	s8 iNumLanesAtObstructionPoint = pFollowRoute->GetRoutePoints()[iObstructedSegment].m_iNoLanesThisSide;
	while (potentialRightLane < iNumLanesAtObstructionPoint-1 && !bFoundGoodLaneToRight)
	{
		++potentialRightLane;
		for (int i = iTargetPoint; i < pFollowRoute->GetNumPoints(); i++)
		{
			if (IsSegmentObstructed(i, pFollowRoute, pVeh, potentialRightLane, fDesiredSpeed, fDistToObstructionInTestingRightLane, pNewObstructingEntityRight, fObstructionInRightLaneSpeed, bDriveInReverse))
			{
				break;
			}
		}

		if (fDistToObstructionInTestingRightLane > fDistToInitialObstruction
			&& fObstructionInRightLaneSpeed > fOriginalObstructingEntitySpeed * 1.05f)
		{
			bFoundGoodLaneToRight = true;
		}
#if ENABLE_LANECHANGE_DEBUGGING
		else if (pNewObstructingEntityRight)
		{
			HelperDisplayLaneChangeDebugSphere(pVeh, VEC3V_TO_VECTOR3(pNewObstructingEntityRight->GetTransform().GetPosition()), Color_red);
		}
#endif //ENABLE_LANECHANGE_DEBUGGING
	}

	//const float fAmountMoreFreeSpaceOnRight = fDistToObstructionInTestingRightLane - fDistToInitialObstruction;
	//const float fAmountMoreFreeSpaceOnLeft = fDistToObstructionInTestingLeftLane - fDistToInitialObstruction;
	const bool bFoundReallyGoodLaneToRight = bFoundGoodLaneToRight && fObstructionInRightLaneSpeed > fOriginalObstructingEntitySpeed * 1.25f;//fAmountMoreFreeSpaceOnRight > pVeh->GetBoundingBoxMax().y * 6.0f;
	const bool bFoundReallyGoodLaneToLeft = bFoundGoodLaneToLeft && fObstructionInLeftLaneSpeed > fOriginalObstructingEntitySpeed * 1.25f;//fAmountMoreFreeSpaceOnLeft > pVeh->GetBoundingBoxMax().y * 6.0f;

	if (!bFoundGoodLaneToLeft && !bFoundGoodLaneToRight)
	{
#if ENABLE_LANECHANGE_DEBUGGING
		HelperDisplayLaneChangeDebugTest(pVeh, "Found No Good Lane On Either Side");
#endif //__BANK
		return;
	}

	bool bGoingLeft = bFoundGoodLaneToLeft;

	if (!bFoundGoodLaneToLeft && !bFoundReallyGoodLaneToRight)
	{
#if ENABLE_LANECHANGE_DEBUGGING
		HelperDisplayLaneChangeDebugTest(pVeh, "Found No Good Lane On Either Side 2");
#endif //__BANK
		return;
	}

	//if we aren't allowed to move right yet, but we see a good opening on the right
	//and the left is blocked, don't do anything just yet.  we'll wait until the timer
	//runs out
	if (bFoundReallyGoodLaneToRight && !bFoundReallyGoodLaneToLeft)
	{
		if (bWantsToUndertake)
		{
			bGoingLeft = false;
		}
		else
		{
			//PF_STOP(AI_LaneChange);
			return;
		}
	}

	Assert(iObstructedSegment >= iTargetPoint);

	if (IsAreaClearToChangeLanes(pVeh, pFollowRoute, bGoingLeft, fDesiredSpeed, bDriveInReverse))
	{
		//const int iPointIndexToSetRequired = rage::Min(iObstructedSegment, ((iTargetPoint + iObstructedSegment) / 2) + 1);

		//const float fIdealTimeToChangeLanes = CDriverPersonality::GetTimeToChangeLanes(pVeh->GetDriver());
		//static dev_float s_fVelocityLaneChangeRatio = 1.0f;
		//const float fDistanceToDesiredLaneChange = rage::Max(fDistToInitialObstruction, (fOriginalObstructingEntitySpeed * fIdealTimeToChangeLanes * s_fVelocityLaneChangeRatio));
		//const int iPointIndexToSetRequired = rage::Min(HelperFindSegmentAtDistance(pFollowRoute, pObstacle,fDistanceToDesiredLaneChange)
		//	, pFollowRoute->GetNumPoints()-2);

		int iNumSegsToSetAhead = 1;
		if (fOriginalObstructingEntitySpeed < 1.0f)
		{
			iNumSegsToSetAhead = 1;
		}
		else if (fOriginalObstructingEntitySpeed < 10.0f)
		{
			iNumSegsToSetAhead = 2;
		}
		else if (fOriginalObstructingEntitySpeed < 20.0f)
		{
			iNumSegsToSetAhead = 3;
		}
		else
		{
			iNumSegsToSetAhead = 4;
		}

		const int iPointIndexToSetRequired = rage::Min(iObstructedSegment+iNumSegsToSetAhead, pFollowRoute->GetNumPoints()-2);

		//grcDebugDraw::Sphere(pVeh->GetVehiclePosition(), 2.5f, Color_red, false, 10);

		const int newLane = Clamp(iBlockedLane + (bGoingLeft ? -1 : 1), 0, rage::Max(pFollowRoute->GetRoutePoints()[iPointIndexToSetRequired].m_iNoLanesThisSide-1, 0));
		pFollowRoute->SetRequiredLaneIndex(iPointIndexToSetRequired, newLane);

		pFollowRoute->SetCurrentRouteContainsLaneChange(true);

		//also update our nodelist so we make better decisions at junctions
		CVehicleNodeList* pNodeList = pVeh->GetIntelligence()->GetNodeList();
		if (pNodeList)
		{
			//int iQuitOnNode = -1;
			const int iLastPointOnList = pNodeList->FindLastGoodNode();
			for (int i = iPointIndexToSetRequired; i <= iLastPointOnList; i++)
			{
				const CPathNode* pNode = pNodeList->GetPathNode(i);

				//stop if we reach a junction, unloaded node,
				//or something else has already set the required lane
				if (!pNode || pNode->IsJunctionNode()
					|| pNode->m_2.m_slipJunction
					|| pFollowRoute->GetRequiredLaneIndex(i) >= 0)
				{
					break;
				}

				const int iPathLinkIndex = pNodeList->GetPathLinkIndex(i);
				if (iPathLinkIndex >= 0)
				{
					const CPathNodeLink& rLink = *ThePaths.FindLinkPointerSafe(pNodeList->GetPathNodeAddr(i).GetRegion(),iPathLinkIndex);

					const int nodeListNewLane = Clamp(newLane, (int)0, (int)rLink.m_1.m_LanesToOtherNode-1);
					pNodeList->SetPathLaneIndex(i, (s8)nodeListNewLane);
				}
			}

// 			//shorten the route to force a re-plan
// 			if (iQuitOnNode >= 0)
// 			{
// 				pFollowRoute->SetNumPoints(rage::Min((int)pFollowRoute->GetNumPoints(), iQuitOnNode+1));
// 			}	
		}
		
#if ENABLE_LANECHANGE_DEBUGGING
		HelperDisplayLaneChangeDebugTest(pVeh, "Found a good lane, changing!!!!!!!!");
#endif //__BANK
	}
#if ENABLE_LANECHANGE_DEBUGGING
	else
	{
		HelperDisplayLaneChangeDebugTest(pVeh, "Area not clear to change lanes.");
	}
#endif //__BANK

	/////////////////////////////////////

	//PF_STOP(AI_LaneChange);
	return;
}

bool CVehicleLaneChangeHelper::IsAreaClearToChangeLanes(const CVehicle* const pVeh, const CVehicleFollowRouteHelper* const pFollowRoute, const bool bSearchLeft, const float fDesiredSpeed, /*float& fDistToObstructionOut,*/ const bool bDriveInReverse) const
{
	s32 iTargetNode = pFollowRoute->GetCurrentTargetPoint();

	//assume no obstruction
	//fDistToObstructionOut = FLT_MAX;

	//early out:  if we're already all the way to the left and trying to shift left
	//we can't do it

	const int iTargetLane = rage::round(pFollowRoute->GetRoutePoints()[iTargetNode].GetLane().Getf());
	if (bSearchLeft && iTargetLane == 0)
	{
		return false;
	}

	//if we're trying to go right and already all the way to the right, dont' allow that either
	if (!bSearchLeft && iTargetLane == pFollowRoute->GetRoutePoints()[iTargetNode].m_iNoLanesThisSide - 1)
	{
		return false;
	}

	s8 laneOffset = bSearchLeft ? -1 : 1;
	CPhysical* pObstructingEntity = NULL;
	float fDistToObstruction = FLT_MAX;
	float fObstructingEntitySpeed = 0.0f;

	s32 iOldNode = Clamp(iTargetNode - 4, 1, CVehicleNodeList::CAR_MAX_NUM_PATHNODES_STORED-1);
	for (int i = iOldNode; i <= iTargetNode; i++)
	{
		if (IsSegmentObstructed(i, pFollowRoute, pVeh, rage::round(pFollowRoute->GetRoutePoints()[i].GetLane().Getf())+laneOffset
			, fDesiredSpeed, fDistToObstruction, pObstructingEntity, fObstructingEntitySpeed, bDriveInReverse))
		{
			//first, test if they're actually physically going to be in our way if we were to immediately transplant ourselves
			//one lane to the right/left
			const float fBlockingDistance = pVeh->GetBoundingBoxMax().y * 2.0f;
			float fBlockingDistanceWithTrailer = fBlockingDistance;
			CTrailer* pTrailer = pVeh->GetAttachedTrailer();
			if (pTrailer)
			{
				fBlockingDistanceWithTrailer += pTrailer->GetBoundingBoxMax().y * 2.0f;
			}

			//add an extra 1.0 for the front of the car
			if (fDistToObstruction > -fBlockingDistanceWithTrailer && fDistToObstruction < fBlockingDistance + 1.0f)
			{
				//fDistToObstructionOut = fDistToObstruction;
				return false;
			}

			//only do this for guys behind us.
			if (fDistToObstruction < 0.0f)
			{
				//overestimate the other guy's velocity slightly
				const CPed* pOtherDriver = pObstructingEntity->GetIsTypeVehicle() ? static_cast<CVehicle*>(pObstructingEntity)->GetDriver(): NULL;
				const bool bGiveMoreLeewayToOtherVehicle = pObstructingEntity->PopTypeIsMission() 
					|| (pOtherDriver && (pOtherDriver->IsPlayer() || pOtherDriver->PopTypeIsMission()));
				static dev_float s_fAmountToOverestimateOtherVelocity = bGiveMoreLeewayToOtherVehicle ? 5.0f : 2.0f;
				Vector3 vOtherVelocity = pObstructingEntity->GetVelocity();
				Vector3 vOtherVelAddition = vOtherVelocity;
				vOtherVelAddition.NormalizeSafe();
				vOtherVelAddition *= s_fAmountToOverestimateOtherVelocity;
				vOtherVelocity += vOtherVelAddition;

				//now do some magic 1D intersection tests with the velocity we got, I guess?
				Vector3 vRelativeVelocity = vOtherVelocity - pVeh->GetAiVelocity();
				static dev_float s_fNumSecondsToLookAhead = 1.5f;
				vRelativeVelocity *= s_fNumSecondsToLookAhead;

				const bool bMovingSameDir = pObstructingEntity->GetVelocity().Dot(pVeh->GetAiVelocity()) >= 0.0f;
				const bool bObstructingEntityFasterThanMe = vRelativeVelocity.Dot(pVeh->GetAiVelocity()) >= 0.0f;

				//make sure to subtract out the half lengths of our respective vehicles
				const float fDistFromOurCenterToRear = fBlockingDistanceWithTrailer-pVeh->GetBoundingBoxMax().y;
				const float fDistToObstructionWithCarLengths = fDistToObstruction+fDistFromOurCenterToRear+pObstructingEntity->GetBoundingBoxMax().y;

				if (bMovingSameDir && bObstructingEntityFasterThanMe && vRelativeVelocity.Mag() > fabsf(rage::Min(0.0f, fDistToObstructionWithCarLengths)))
				{
					//grcDebugDraw::Sphere(pObstructingEntity->GetTransform().GetPosition(), 2.5f, Color_orange, false, 10);
					//grcDebugDraw::Arrow(pVeh->GetVehiclePosition(), pObstructingEntity->GetVehiclePosition(), 2.0f, Color_orange, 10);
					//fDistToObstructionOut = fDistToObstruction;
#if ENABLE_LANECHANGE_DEBUGGING
					HelperDisplayLaneChangeDebugSphere(pVeh, VEC3V_TO_VECTOR3(pObstructingEntity->GetTransform().GetPosition()), Color_purple);
#endif
					return false;
				}
			}
		}
	}

	return true;
}

bool CVehicleLaneChangeHelper::IsSegmentObstructed(const int iRouteIndex, const CVehicleFollowRouteHelper* const pFollowRoute, const CVehicle* const pVeh, const int laneToCheck, const float fDesiredSpeed, float& fDistToObstructionOut, CPhysical*& pObstructingEntityOut, float& fObstructionSpeedOut, const bool bDriveInReverse) const
{
	//assume no obstruction, therefore a really high distance
	fDistToObstructionOut = FLT_MAX;
	fObstructionSpeedOut = FLT_MAX;

	if (iRouteIndex <= 0)
	{
		return false;
	}

	const CRoutePoint* pPoint = &pFollowRoute->GetRoutePoints()[iRouteIndex];
	const CRoutePoint* pLastPoint = &pFollowRoute->GetRoutePoints()[iRouteIndex-1];

	bool bObstructed = false;

	const Vec3V vehDriveDirV = CVehicleFollowRouteHelper::GetVehicleDriveDir(pVeh, bDriveInReverse);

	//s16 linkIndex = -1;
	//ThePaths.FindNodesLinkIndexBetween2Nodes(pLastNode, pNode, linkIndex);
	//aiAssert(linkIndex > -1);
	//CPathNodeLink* pLink = &ThePaths.GetNodesLink(pLastNode, linkIndex);
	//Assert(pLink);
	//const int numLanesInRoad = pLink->m_1.m_LanesToOtherNode;

	//float laneCentreOffset = ThePaths.GetLinksLaneCentreOffset(*pLink, laneToCheck);
	float laneCentreOffset = pLastPoint->m_fCentralLaneWidth + laneToCheck * pLastPoint->m_fLaneWidth;

	Vector2 side = Vector2(pPoint->GetPosition().GetYf() -  pLastPoint->GetPosition().GetYf(),  pLastPoint->GetPosition().GetXf() - pPoint->GetPosition().GetXf());
	side.Normalize();
	Vector3 laneCentreOffsetVec((side.x * laneCentreOffset),(side.y * laneCentreOffset), 0.0f);

	Vector3 vStartPos = VEC3V_TO_VECTOR3(pLastPoint->GetPosition()) + laneCentreOffsetVec;
	Vector3 vEndPos = VEC3V_TO_VECTOR3(pPoint->GetPosition()) + laneCentreOffsetVec;

	//raise this a little, so we don't go underneath any bounding boxes
	vStartPos.z += 0.25f;
	vEndPos.z += 0.25f;
	Vector3 vSegment = vEndPos - vStartPos;

	CEntityScannerIterator entityList = pVeh->GetIntelligence()->GetVehicleScanner().GetIterator();
	for( CEntity* pEntity = entityList.GetFirst(); pEntity; pEntity = entityList.GetNext() )
	{
		//don't test ourselves
		if (pEntity == pVeh)
		{
			continue;
		}

		if (!pEntity)
		{
			continue;
		}

		CVehicle *pVehObstacle =(CVehicle *)pEntity;

		// Ignore trailers we're towing
		if(pVehObstacle->InheritsFromTrailer())
		{
			// check whether the trailer is attached to the parent vehicle
			if (CTaskVehicleGoToPointWithAvoidanceAutomobile::GetTrailerParentFromVehicle(pVehObstacle) == pVeh)
			{
				continue;
			}
		}

		//don't change lanes for trains
		if (pVehObstacle->InheritsFromTrain())
		{
			continue;
		}

		if (!ShouldChangeLanesForThisEntity(pVeh, pVehObstacle, fDesiredSpeed, bDriveInReverse))
		{
			continue;
		}

		Vector3 vBoxMin = pEntity->GetBoundingBoxMin();
		Vector3 vBoxMax = pEntity->GetBoundingBoxMax();	

		//enfatten the boxen
		vBoxMax += Vector3(0.25f, 0.5f, 0.25f);
		vBoxMin += Vector3(-0.25f, -0.5f, -0.25f);

		// 		if (pVeh->GetDriver() && pVeh->GetDriver()->IsPlayer())
		// 		{
		// 			grcDebugDraw::Line(vStartPos, vEndPos, Color_green, 1);
		// 			grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vBoxMin), VECTOR3_TO_VEC3V(vBoxMax), pEntity->GetMatrix(), Color_blue, false, 1);
		// 			//grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vBoxMin), VECTOR3_TO_VEC3V(vBoxMax), debugIdentity, Color_blue, false);
		// 		}

		//transform the segment into car space
		Vector3 vStartPosLocal = VEC3V_TO_VECTOR3(pEntity->GetTransform().UnTransform(VECTOR3_TO_VEC3V(vStartPos)));
		Vector3 vSegmentLocal = VEC3V_TO_VECTOR3(pEntity->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vSegment)));

		if (geomBoxes::TestSegmentToBoxMinMax(vStartPosLocal, vStartPosLocal + vSegmentLocal, vBoxMin, vBoxMax))
		{
			bObstructed = true;

			//project pVeh -> pVehObstacle onto pVeh.Forward
			fDistToObstructionOut = Dot(pVehObstacle->GetVehiclePosition() - pVeh->GetVehiclePosition(), vehDriveDirV).Getf();
			pObstructingEntityOut = pVehObstacle;
			fObstructionSpeedOut = pVehObstacle->GetAiVelocity().Dot(VEC3V_TO_VECTOR3(vehDriveDirV));

			Assert(fDistToObstructionOut < LARGE_FLOAT);

			// 			if (pVeh->GetDriver() && pVeh->GetDriver()->IsPlayer())
			// 			{
			// 				grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vBoxMin), VECTOR3_TO_VEC3V(vBoxMax), pEntity->GetMatrix(), Color_red, true, 1);
			// 			}

			//we don't need to check any more shapes once we've found one
			break;
		}
	}

	//no need to test peds if we already found something blocking this segment
	//we're going to change it anyway
	
	//don't test for peds if we're a taxi, they tend to get in the way when
	//we're pulling over to the side of the road
	const bool bSkipSearchingPeds = pVeh->IsTaxi();
	if (!bObstructed && !bSkipSearchingPeds)
	{
		CEntityScannerIterator pedList = pVeh->GetIntelligence()->GetPedScanner().GetIterator();
		for( CEntity* pEntity = pedList.GetFirst(); pEntity; pEntity = pedList.GetNext() )
		{
			CPed* pPed = static_cast<CPed*>(pEntity);

			if (!pPed)
			{
				continue;
			}

			if (pPed->GetIsInVehicle())
			{
				continue;
			}

			//flatten
			Vector3 vPedPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
			vPedPos.z = vStartPos.z;

			const float fDebugDist2ToLink = geomDistances::Distance2SegToPoint(vStartPos, vSegment, vPedPos);
			if ( fDebugDist2ToLink < ms_fMinDist2ToLink )
			{
				//project pVeh -> pPed onto pVeh.Forward
				fDistToObstructionOut = Dot(pPed->GetTransform().GetPosition() - pVeh->GetVehiclePosition(), vehDriveDirV).Getf();
				pObstructingEntityOut = pPed;
				fObstructionSpeedOut = pPed->GetVelocity().Dot(VEC3V_TO_VECTOR3(vehDriveDirV));

				Assert(fDistToObstructionOut < LARGE_FLOAT);

				bObstructed = true;
				break;
			}
		}
	}

	return bObstructed;
}


// Calculates the position the vehicle should be driving towards and the speed.
// Returns the distance to the end of the route
float CVehicleFollowRouteHelper::FindTargetCoorsAndSpeed( const CVehicle* pVehicle, float fDistanceAhead, float fDistanceToProgressRoute
	, Vec3V_In vStartCoords, const bool bDrivingInReverse, Vec3V_InOut vOutTargetCoords, float& fOutMaxSpeed, const bool bUseSpeedAtCurrentPosition
	, const bool bUseCachedSpeedVel, const bool bUseStringPulling, const int iOverrideEndPointIndex, const bool bAllowLaneSlack)
{
#if __BANK
	TUNE_GROUP_FLOAT(CAR_AI,fCruiseSpeedScale,1.0f,0.0f,10.0f,0.1f);
	fOutMaxSpeed *= fCruiseSpeedScale;
#endif

	m_bDrivingInReverse = bDrivingInReverse;

	//something's gone wrong, return the start position
	if (m_iNumPoints <= 0)
	{
		aiWarningf("Bailing out of FindTargetCoorsAndSpeed because no points on the followroute, vehicle %p might be doing something weird now", pVehicle);
		vOutTargetCoords = vStartCoords;
		return 0.0f;
	}

	return FindTargetCoorsAndSpeed_New(pVehicle, fDistanceAhead, fDistanceToProgressRoute, vStartCoords, vOutTargetCoords, fOutMaxSpeed, bUseSpeedAtCurrentPosition,
			bUseCachedSpeedVel, bUseStringPulling, iOverrideEndPointIndex, bAllowLaneSlack);
}

//copied from CVehicleNodeList
bool CVehicleFollowRouteHelper::GetClosestPosOnPath(const Vector3 & vPos, Vector3 & vClosestPos_out, int & iPathIndexClosest_out, float & iPathSegmentPortion_out) const
{
	iPathIndexClosest_out = -1;

	float fPathSegClosestDistSqr = FLT_MAX; //100000.0f;// Large sentinel value.
	Vector3 nodeAPos, nodeBPos;
	Vector3 vClosestPos;
	for(s32 iAutopilotNodeIndex = 0; iAutopilotNodeIndex < m_iNumPoints - 1; ++iAutopilotNodeIndex)
	{
		const CPathNode* pNodeA = ThePaths.FindNodePointerSafe(m_RoutePoints[iAutopilotNodeIndex].GetNodeAddress());
		if(!pNodeA){ continue; }

		const CPathNode* pNodeB = ThePaths.FindNodePointerSafe(m_RoutePoints[iAutopilotNodeIndex+1].GetNodeAddress());
		if(!pNodeB){ continue; }

		pNodeA->GetCoors(nodeAPos);
		pNodeB->GetCoors(nodeBPos);

		// Find the closest position to this knot and portion along it's segment to the next knot.
		const Vector3 nodeABDelta = nodeBPos - nodeAPos;
		const Vector3 nodeAToPosDelta(vPos - nodeAPos);
		const Vector3 nodeAToPosDeltaFlat(nodeAToPosDelta.x, nodeAToPosDelta.y, 0.0f);
		float dotPr = nodeAToPosDeltaFlat.Dot(nodeABDelta);
		Vector3 closestPointOnSegment = nodeAPos;
		float segmentPortion = 0.0f;
		if(dotPr > 0.0f)
		{
			const float length2 = nodeABDelta.Mag2();
			dotPr /= length2;
			if (dotPr >= 1.0f)
			{
				closestPointOnSegment = nodeBPos;
				segmentPortion = 1.0f;
			}
			else
			{
				closestPointOnSegment = nodeAPos + nodeABDelta * dotPr;
				segmentPortion = dotPr;
			}
		}
		closestPointOnSegment.z = 0.0f;

		// Check if this is the closest one so far.
		const Vector3 vPosFlat(vPos.x, vPos.y, 0.0f);
		const float distToSegSqr = (vPosFlat - closestPointOnSegment).Mag2();
		if(distToSegSqr < fPathSegClosestDistSqr)
		{
			iPathIndexClosest_out = iAutopilotNodeIndex;
			iPathSegmentPortion_out = segmentPortion;

			fPathSegClosestDistSqr = distToSegSqr;
			vClosestPos = nodeAPos + (nodeABDelta * segmentPortion);
		}
	}

	vClosestPos_out = vClosestPos;

	return (iPathIndexClosest_out != -1);
}

void CVehicleFollowRouteHelper::CalculateDesiredSpeeds(const CVehicle* pVehicle, const bool bUseStringPulling)
{
	CVehicleFollowRouteHelper::CalculateDesiredSpeeds(pVehicle, bUseStringPulling, m_RoutePoints, m_iNumPoints);
}

void CVehicleFollowRouteHelper::CalculateDesiredSpeeds(const CVehicle* pVehicle, const bool bUseStringPulling, CRoutePoint* pRoutePoints, int iNumPoints)
{
	Assert(pRoutePoints);

	const CAIHandlingInfo* pAIHandlingInfo = pVehicle->GetAIHandlingInfo();
	Assert(pAIHandlingInfo);

	float fPreviousOrientation = 0.0f;

	static dev_float s_fDefaultMaxCorneringSpeed = 62.0f;

	//still needs work before I can enable this. the offsets for junctions are wrong and vehicles are slowing down
	//excessively here
	TUNE_GROUP_BOOL(NEW_CRUISE_TASK, bUseActualLanePositionsForSlowdown, false);

	// Loop through all the points and calculate maximum speeds based on the turning angles
	for( s32 i = 0; i < iNumPoints-1; i++ )
	{
		Vec3V vThisPoint = bUseActualLanePositionsForSlowdown ? pRoutePoints[i].GetPosition() + pRoutePoints[i].GetLaneCenterOffset() : pRoutePoints[i].GetPosition();
		Vec3V vNextPoint = bUseActualLanePositionsForSlowdown ? pRoutePoints[i+1].GetPosition() + pRoutePoints[i+1].GetLaneCenterOffset() : pRoutePoints[i+1].GetPosition(); 
		Vector3 diff = VEC3V_TO_VECTOR3(vNextPoint - vThisPoint);
		diff.z = 0.0f;
		diff.NormalizeSafe(XAXIS);
		float	linkOrientation = fwAngle::GetATanOfXY(diff.x, diff.y);
		const float fOrigLinkOrientation = linkOrientation;

		//if we're exiting a junction, save off the old link orientation
		bool bUseOrigLinkOrientation = false;

		//static dev_float s_fStandardJunctionSize = 20.0f;
		// static dev_float s_fExtraLanesMultiplier = 0.2f;

		//if we're an ignore nav link but not a junction, use whatever the last good orientation was
		if (bUseStringPulling && i > 0 && pRoutePoints[i].m_bJunctionNode)
		{
			//direction = entry -> exit
			Vector3 vDiffNew = VEC3V_TO_VECTOR3((pRoutePoints[i+1].GetPosition() + pRoutePoints[i+1].GetLaneCenterOffset()) 
				- (pRoutePoints[i-1].GetPosition() + pRoutePoints[i-1].GetLaneCenterOffset()));
			vDiffNew.z = 0.0f;
			if (vDiffNew.Mag2() > SMALL_FLOAT)
			{
				//if we're string pulling, assuming that the link exiting the junction is the same direction
				//as the link before it, to ensure that we don't slow down at all
				linkOrientation = fPreviousOrientation;

				//but at the same time, set this guy in order to save off the actual direction
				//as fPreviousDirection, so we don't slow down for the link after this
				bUseOrigLinkOrientation = true;
			}	
		}
		else if (bUseStringPulling && i > 0 && i < iNumPoints-2 && pRoutePoints[i+1].m_bJunctionNode)
		{
			//direction = entry -> exit
			//direction = entry -> exit
			TUNE_GROUP_BOOL(NEW_CRUISE_TASK, bUseActualLanePositionsForStringPulling, false);
			Vector3 vDiffNew, vStartPoint, vMidPoint, vEndPoint;
			if (bUseActualLanePositionsForStringPulling)
			{
				//vDiffNew = 
				//- );
				vEndPoint = VEC3V_TO_VECTOR3(pRoutePoints[i+2].GetPosition() + pRoutePoints[i+2].GetLaneCenterOffset());
				vStartPoint = VEC3V_TO_VECTOR3(pRoutePoints[i].GetPosition() + pRoutePoints[i].GetLaneCenterOffset());
				vMidPoint = VEC3V_TO_VECTOR3(pRoutePoints[i+1].GetPosition() + pRoutePoints[i+1].GetLaneCenterOffset());
			}
			else
			{
				//vDiffNew = VEC3V_TO_VECTOR3((m_RoutePoints[i+2].GetPosition())
				//	- (m_RoutePoints[i].GetPosition()));
				vEndPoint = VEC3V_TO_VECTOR3(pRoutePoints[i+2].GetPosition());
				vStartPoint = VEC3V_TO_VECTOR3(pRoutePoints[i].GetPosition());
				vMidPoint = VEC3V_TO_VECTOR3(pRoutePoints[i+1].GetPosition());
			}
			vDiffNew = vEndPoint - vStartPoint;
			vDiffNew.z = 0.0f;
			if (vDiffNew.Mag2() > SMALL_FLOAT)
			{
				
#if 1
				vDiffNew.NormalizeFast();
				linkOrientation = fwAngle::GetATanOfXY(vDiffNew.x, vDiffNew.y);
#else
				//bring this stuff back after tweaking
				//const float fJunctionSize = vDiffNew.MagFastV().x;
				const float fJunctionSize = rage::Min((vEndPoint - vMidPoint).MagFastV().x
					,(vMidPoint - vStartPoint).MagFastV().x);

				vDiffNew.NormalizeFast();
				const float fNewLinkOrientation = fwAngle::GetATanOfXY(vDiffNew.x, vDiffNew.y);

				//scale the difference by some amount based on the size of the junction
				//larger junctions should assume less of a difference in angle
				const float fDifference = SubtractAngleShorter(fOrigLinkOrientation, fNewLinkOrientation);
				const float fNumLanesMultiplier = 1.0f - rage::Max(pRoutePoints[i+2].m_iNoLanesThisSide - 1, 0) * s_fExtraLanesMultiplier;

				linkOrientation = fNewLinkOrientation + fDifference * (s_fStandardJunctionSize / fJunctionSize) * fNumLanesMultiplier;
#endif //0
			}
		}
		else if( i == 0 || (pRoutePoints[i].m_bIgnoreForNavigation && !pRoutePoints[i].m_bJunctionNode))
		{
			linkOrientation = fPreviousOrientation;
			if (i==0)
			{
				bUseOrigLinkOrientation = true;
			}
		}
		else if (i < iNumPoints-2 && pRoutePoints[i].m_bIgnoreForNavigation && pRoutePoints[i].m_bJunctionNode)
		{
			Vector3 vThisPointNew = VEC3V_TO_VECTOR3(pRoutePoints[i+1].GetPosition());
			Vector3 vNextPointNew = VEC3V_TO_VECTOR3(pRoutePoints[i+2].GetPosition()); 
			Vector3 diffNew = vNextPointNew - vThisPointNew;
			diffNew.z = 0.0f;
			diffNew.NormalizeFast();
			float	nextOrientation = fwAngle::GetATanOfXY(diffNew.x, diffNew.y);
			linkOrientation = nextOrientation;
		}

		// Now also calculate an absolute maximum speed.
		float fAngle = ABS(fwAngle::LimitRadianAngle(linkOrientation-fPreviousOrientation));
		float fMinCorneringAngle = 0.0f;
		float fMaxCorneringAngle = 1.1f;
		float fMinCorneringSpeed = 11.0f;
		float fMaxCorneringSpeed = s_fDefaultMaxCorneringSpeed;

		// Go through the speed curves and work out which min/max values to use to lerp the speed value
		s32 iNumCurves = pAIHandlingInfo->GetCurvePointCount();
		for( s32 iCurve = 0; iCurve < iNumCurves; iCurve++ )
		{
			const CAICurvePoint* pCurvePoint = pAIHandlingInfo->GetCurvePoint(iCurve);
			Assert(pCurvePoint);
			if( fAngle < pCurvePoint->GetAngleRadians() )
			{
				fMaxCorneringAngle = pCurvePoint->GetAngleRadians();
				fMinCorneringSpeed = pCurvePoint->GetSpeed();
				break;
			}
			else
			{
				fMinCorneringAngle = pCurvePoint->GetAngleRadians();
				fMaxCorneringSpeed = pCurvePoint->GetSpeed();
			}
		}

		const float fSpeedForThisPoint = CVehicleIntelligence::FindSpeedMultiplier(pVehicle, fAngle, fMinCorneringAngle, fMaxCorneringAngle, fMinCorneringSpeed, fMaxCorneringSpeed);
		pRoutePoints[i].SetSpeed( fSpeedForThisPoint );
		Assert(fSpeedForThisPoint >= 0.0f);

		//if this is a significant-ish turn, check the slope of the previous two links. if the second link's slope is shallower than the first,
		//extend the stopping distance backward to it
		static dev_float s_fTurnThresholdRadians = PI / 4.0f;
		if (i > 1 && fAngle > s_fTurnThresholdRadians)
		{
			//get the slope from i-2 to i-1
			//get the slope from i-1 to i
			Vector3 vPrevPrevPoint = VEC3V_TO_VECTOR3(pRoutePoints[i-2].GetPosition());
			Vector3 vPrevPoint = VEC3V_TO_VECTOR3(pRoutePoints[i-1].GetPosition()); 
			Vector3 vCurrPoint = VEC3V_TO_VECTOR3(pRoutePoints[i].GetPosition()); 
			Vector3 vDiffPrev = vPrevPoint - vPrevPrevPoint;
			Vector3 vDiffCurr = vCurrPoint - vPrevPoint;

			const float fRisePrev = vDiffPrev.z;
			const float fRiseCurr = vDiffCurr.z;

			const float fRunPrev = vDiffPrev.XYMag();
			const float fRunCurr = vDiffCurr.XYMag();

			const float fSlopePrev = fRunPrev >= SMALL_FLOAT ? fRisePrev / fRunPrev : 0.0f;
			const float fSlopeCurr = fRunCurr >= SMALL_FLOAT ? fRiseCurr / fRunCurr : 0.0f;

			if (fSlopeCurr < fSlopePrev)
			{
				//grcDebugDraw::Sphere(pVehicle->GetVehiclePosition(), 2.0f, Color_red, false, 15);
				
				pRoutePoints[i-1].SetSpeed( rage::Min(fSpeedForThisPoint, pRoutePoints[i-1].GetSpeed()) );
			}
		}

		fPreviousOrientation = bUseOrigLinkOrientation ? fOrigLinkOrientation : linkOrientation;
	}

	//never slow down for the last point
	if (iNumPoints > 0)
	{
		pRoutePoints[iNumPoints-1].SetSpeed(s_fDefaultMaxCorneringSpeed);
	}
}

// NAME : FindRoadBoundaries
// PURPOSE : A helper function to determine the drivable left/right extremes as this route point
void CRoutePoint::FindRoadBoundaries(float & fLeftDistOut, float & fRightDistOut) const
{
	float fLeftDist, fRightDist;
	if(m_bIsSingleTrack)
	{
		fLeftDist = rage::Max(m_iNoLanesOtherSide * m_fLaneWidth, m_iNoLanesThisSide * m_fLaneWidth) * 0.5f;
		fRightDist = fLeftDist;
	}
	else if(m_iNoLanesThisSide == 0)
	{
		fLeftDist = (m_iNoLanesOtherSide * m_fLaneWidth) * 0.5f;
		fRightDist = fLeftDist;
	}
	else if(m_iNoLanesOtherSide == 0)
	{
		fLeftDist = (m_iNoLanesThisSide * m_fLaneWidth) * 0.5f;
		fRightDist = fLeftDist;
	}
	else
	{
		// m_fCentralLaneWidth is actually the offset to the center of the first lane.  Thus it equals half of the central median plus half of a lane width.  Here we only want to add the half central median, so subtract half of a lane width.
		fLeftDist = m_iNoLanesOtherSide * m_fLaneWidth + m_fCentralLaneWidth - m_fLaneWidth * 0.5f;
		fRightDist = m_iNoLanesThisSide * m_fLaneWidth + m_fCentralLaneWidth - m_fLaneWidth * 0.5f;
	}

	Assert(fLeftDist > 0.0f && fRightDist > 0.0f);
	fLeftDistOut = fLeftDist;
	fRightDistOut = fRightDist;
}

bool CVehicleFollowRouteHelper::CalculatePositionAndOrientationOnVirtualRoad
	(CVehicle * pVehicle, Mat34V_In matTest, const Vec3V * UNUSED_PARAM(pBonnetPos), bool bConsiderSettingWheelCompression, bool bUpdateDummyConstraint, 
	Vec3V_InOut vToFrontOut, Vec3V_InOut vToRightOut, Vec3V_InOut vAverageRoadToWheelOffsetOut, Vec3V * pNewPosConstrainedToRoadWidth, bool bPreventWheelCompression) const
{
	const ScalarV fZero( V_ZERO );
	const ScalarV fMinusOne( V_NEGONE );
	const ScalarV fOne( V_ONE );
	const ScalarV fSmallFloat( V_FLT_SMALL_6 );

	// Tuning parameters
	static ScalarV fZSlopeComparison( 0.08f );
	static const ScalarV fMaxCamberDelta( 0.39f ); // PI/8, or 22.5 degs
	static const ScalarV fLongVehicleHalfLength( V_FOUR );

	//if we don't have any points on the route, return false before setting
	//bDummyWheelImpactsSampled. that way, the CVehicle::ProcessSuperDummy can run
	//on the nodelist, if there is one
	if (m_iNumPoints <= 1)
	{
		return false;
	}

	const Vec3V vTestPos = matTest.GetCol3();//(pBonnetPos ? *pBonnetPos : matTest.GetCol3());	// Dummies use the center of the vehicle to find primary route info

	if (!IsFiniteAll(vTestPos) || !IsFiniteAll(matTest.GetCol3()))
	{
		return false;
	}

#if __BANK && DEBUG_DRAW
	DebugDrawRoute(*pVehicle,vTestPos);
#endif

	const int iCurrentAtPoint = FindCurrentAtPoint(vTestPos);

	//Assert( pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy) );
	if(pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy))
	{
		Assertf( !pVehicle->InheritsFromTrailer(), "Trailers should not be running tasks!  Trailer superdummy updates in CVehicle::ProcessSuperDummy()");
		Assertf( iCurrentAtPoint >= 0 && iCurrentAtPoint < MAX_ROUTE_SIZE, "iCurrentAtPoint out of bounds: %d", iCurrentAtPoint);
		Assertf( !pVehicle->GetDummyAttachmentParent() || !pVehicle->GetDummyAttachmentParent()->InheritsFromTrailer(), "Cargo vehicles shouldn't be running tasks.  Update in CVehicle::ProcessSuperDummy().");
	}

	const int iNumWheelsVehicle = pVehicle->GetNumWheels();
	int iNumWheels = Min(iNumWheelsVehicle, 4);
	//Assertf(iNumWheels == 2 || iNumWheels == 4, "SuperDummy mode doesn't yet support trikes or unicycles!");
	//if(iNumWheels != 2 && iNumWheels != 4)
	//{
	//	return false;
	//}

	//--------------------------------------------------------------------------------------

	// Create the TRouteInfo
	CVirtualRoad::TRouteInfo routeInfo;
	CVirtualRoad::TRouteInfo routeInfoRearWheels;
	bool bCalcConstraintParams = (bUpdateDummyConstraint || pNewPosConstrainedToRoadWidth);
	CVirtualRoad::HelperCreateRouteInfoFromFollowRouteAndCenterNode(*this,iCurrentAtPoint,bCalcConstraintParams,false,routeInfo, pVehicle->ShouldTryToUseVirtualJunctionHeightmaps());

	// Can we get away with sampling only the center of the vehicle?
	bool bProcessAllWheels = false;
	bool bUseAltRouteInfoForRearWheels = false;
	const int bLongVehicle = IsGreaterThanAll(RCC_VEC3V(pVehicle->GetBoundingBoxMax()).GetY(),fLongVehicleHalfLength);
	bool bUseSeparateRouteInfoForRearWheels = false;
	if(bLongVehicle)
	{
		bProcessAllWheels = true;
	}
	else if(routeInfo.m_bHasTwoSegments)
	{
		if(routeInfo.m_bConsiderJunction)
		{
			bProcessAllWheels = true;
			bUseAltRouteInfoForRearWheels = true;
		}
		// Must process all wheels if the slope between the two segments is significantly different
		// TODO : also should do so if the camber is significantly different at any route points or on the other side
		else if( !IsCloseAll( routeInfo.m_vNodeDirNormalised[0].GetZ(), routeInfo.m_vNodeDirNormalised[1].GetZ(), fZSlopeComparison ) ||
			!IsCloseAll( routeInfo.m_vCambers.GetX(), routeInfo.m_vCambers.GetZ(), fMaxCamberDelta ) ||
			!IsCloseAll( routeInfo.m_vCambers.GetY(), routeInfo.m_vCambers.GetW(), fMaxCamberDelta ))
		{
			bProcessAllWheels = true;
		}
		// If the previous check didn't make us use all wheels, and bMoreAccurateVirtualRoadProbes is set
		// (done for scenario vehicles now), do a more sophisticated check to see if we should probe all
		// the wheels.
		else if(pVehicle->m_nVehicleFlags.bMoreAccurateVirtualRoadProbes)
		{
			// Get the directions of the two segments.
			const Vec3V roadDir1V = routeInfo.m_vNodeDirNormalised[0];
			const Vec3V roadDir2V = routeInfo.m_vNodeDirNormalised[1];

			// Compute cross products with the Z axis.
			const Vec3V roadDirOrthNonNorm1V = CrossZAxis(roadDir1V);
			const Vec3V roadDirOrthNonNorm2V = CrossZAxis(roadDir2V);

			// Compute the normals of the planes through the directions.
			const Vec3V roadNormalNonNorm1V = Cross(roadDirOrthNonNorm1V, roadDir1V);
			const Vec3V roadNormalNonNorm2V = Cross(roadDirOrthNonNorm2V, roadDir2V);

			// Compute the dot product of the normals, without normalization, and then
			// compute what the dot product would have been if we had normalized before.
			const ScalarV dotNonNormV = Dot(roadNormalNonNorm1V, roadNormalNonNorm2V);
			const ScalarV dotNormV = Scale(dotNonNormV, InvSqrt(Scale(MagSquared(roadNormalNonNorm1V), MagSquared(roadNormalNonNorm2V))));

			// If the angle between the normals is more than the threshold, process all wheels.
			const ScalarV cosAngleThresholdV(0.9994f);	// cos(2 degrees)
			if(IsLessThanAll(dotNormV, cosAngleThresholdV))
			{
				bProcessAllWheels = true;
			}
		}
	}

	CVirtualRoad::TWheelCollision wheelCollisions[4];
	CVirtualRoad::TRouteConstraint routeConstraint;

	static const eHierarchyId iWheelIDs[] = { VEH_WHEEL_LF, VEH_WHEEL_LR, VEH_WHEEL_RF, VEH_WHEEL_RR };
	CWheel * pWheels[4] =
	{
		pVehicle->GetWheelFromId(VEH_WHEEL_LF),
		pVehicle->GetWheelFromId(VEH_WHEEL_LR),
		pVehicle->GetWheelFromId(VEH_WHEEL_RF),
		pVehicle->GetWheelFromId(VEH_WHEEL_RR)
	};

	CVehicleModelInfo * pModelInfo = (CVehicleModelInfo*)pVehicle->GetBaseModelInfo();

	Vec3V vWheelOffsets[4];
	ScalarV fWheelRadii[4];

	if(bProcessAllWheels)
	{
		int numWheels = iNumWheels;

		if( numWheels == 3 )
		{
			numWheels = 4;
		}

		for(int w=0; w<numWheels; w++)
		{
			if( pWheels[ w ] )
			{
				RC_VECTOR3(vWheelOffsets[w]) = CWheel::GetWheelOffset(pModelInfo, iWheelIDs[w]);
				fWheelRadii[w] = ScalarV( pWheels[w]->GetWheelRadius() );
			}
		}

		if( iNumWheels == 3 )
		{
			for(int w = 0; w < 4; w++)
			{
				if( !pWheels[ w ] )
				{
					if( w < 2 )
					{
						RC_VECTOR3( vWheelOffsets[ w ] ) = CWheel::GetWheelOffset(pModelInfo, iWheelIDs[ w + 2 ]);
						fWheelRadii[ w ] = fWheelRadii[ w + 2 ];
					}
					else
					{
						RC_VECTOR3( vWheelOffsets[ w ] ) = CWheel::GetWheelOffset(pModelInfo, iWheelIDs[ w - 2 ]);
						fWheelRadii[ w ] = fWheelRadii[ w - 2 ];
					}
				}
			}
			iNumWheels = 4;
		}

		if(!bUseAltRouteInfoForRearWheels && bLongVehicle)
		{
			const ScalarV fWheelbaseThresholdForRearRouteInfo(V_SIX);
			const ScalarV fWheelbase = Abs(vWheelOffsets[0].GetY() - vWheelOffsets[1].GetY());
 			bUseAltRouteInfoForRearWheels = IsGreaterThanAll(fWheelbase,fWheelbaseThresholdForRearRouteInfo) != 0;
		}

		// Long vehicles may require an addition TRouteInfo for the rear wheels
		if(bUseAltRouteInfoForRearWheels && CVehicleAILodManager::ms_bAllowAltRouteInfoForRearWheels)
		{
			// Could use average of the rear wheels, but probably doesn't matter
			const Vec3V vRearLeftWheelPosGlobal = Transform(matTest,vWheelOffsets[1]);
			int iNumNodesToSearchBeforeCurrent = 3; // Search back a bit farther since the wheels are known to be well before the front of the vehicle
			const int iRearWheelAtPoint = FindCurrentAtPoint(vRearLeftWheelPosGlobal,iNumNodesToSearchBeforeCurrent);
			if(iRearWheelAtPoint!=iCurrentAtPoint)
			{
				bUseSeparateRouteInfoForRearWheels = true;
				CVirtualRoad::HelperCreateRouteInfoFromFollowRouteAndCenterNode(*this,iRearWheelAtPoint,bCalcConstraintParams,false,routeInfoRearWheels, pVehicle->ShouldTryToUseVirtualJunctionHeightmaps());
			}
		}
	}

	Vec3V vAverageRoadToWheelOffset(V_ZERO);
	ScalarV fInvNumWheels(1.0f/(float)iNumWheels);
	Vec3V vToFront, vToFrontFlat, vToRight;

	enum
	{
		iWheelLeftFront, iWheelLeftRear, iWheelRightFront, iWheelRightRear
	};

	// 
	Assert(IsFiniteAll(matTest));
	//const Vec3V vVehicleRightCurrent = matTest.GetCol0();
	const Vec3V vVehicleForwardCurrent = matTest.GetCol1();
	const Vec3V vVehicleUpCurrent = matTest.GetCol2();
	const Vec3V vVehiclePosCurrent = matTest.GetCol3();
	const ScalarV fVehicleHeightAlongVehUp = Dot(vVehicleUpCurrent,vVehiclePosCurrent);
	ScalarV fHitHeightsLocal[4];

	//-------------------------------------------------------
	// Process wheel collisions against virtual road surface

	if(!bProcessAllWheels)
	{
		RC_VECTOR3(vWheelOffsets[0]) = CWheel::GetWheelOffset(pModelInfo, iWheelIDs[0]);
		fWheelRadii[0] = ScalarV(pWheels[0]->GetWheelRadius());

		const Vec3V vTestPosGlobal = vVehiclePosCurrent + vWheelOffsets[0].GetZ() * vVehicleUpCurrent;
		CVirtualRoad::ProcessRoadCollision(routeInfo, vTestPosGlobal, wheelCollisions[0], (bCalcConstraintParams ? &routeConstraint : NULL));

		const Vec3V vHitNormal( wheelCollisions[0].m_vNormal );
		Vec3V vHitPos( wheelCollisions[0].m_vPosition.GetXYZ() );

		fHitHeightsLocal[0] = Dot(vHitPos,vVehicleUpCurrent) - fVehicleHeightAlongVehUp;

		// Remember that "TWheelCollision::m_vPosition.w" holds the distance of the wheel above the surface plane
		vAverageRoadToWheelOffset = vHitNormal * (wheelCollisions[0].m_vPosition.GetW() - fWheelRadii[0]);

		// Project the front of the vehicle down onto the plane, and construct vToFront
		Vec3V vVehFrontOnRoad = vHitPos + vVehicleForwardCurrent - vHitNormal * Dot(vVehicleForwardCurrent,vHitNormal);
		vToFront = vVehFrontOnRoad - vHitPos;
#if __DEV
		if (!aiVerifyf(IsFiniteAll(vToFront), "vToFront bad after vToFront = vVehicleFront - vPos;"))
		{
			vHitPos.Print("vHitPos");
			vVehFrontOnRoad.Print("vVehFrontOnRoad");
		}
#endif //__DEV

		vToFront = Normalize(vToFront);
		Assertf(IsFiniteAll(vToFront), "vToFront bad after vToFront = Normalize(vToFront)");

		// Since vToFront & vHitNormal are orthonormal, vToRight is just the cross product
		vToRight = Cross(vToFront,vHitNormal);

#if DEBUG_DRAW && __BANK
		if(CVehicleAILodManager::ms_bDisplayDummyWheelHits)
		{
			grcDebugDraw::Cross( Vec3V(wheelCollisions[0].m_vPosition.GetIntrin128ConstRef()), 1.0f, Color_magenta, -1);
		}
#endif
	}
	else // bProcessAllWheels
	{
		// NOTE: Wheel offsets are filled-in above.

		int numWheels = iNumWheels;

		if( numWheels == 3 )
		{
			numWheels = 4;
		}

		for(int w=0; w<numWheels; w++)
		{
			if( pWheels[ w ] )
			{
				const Vec3V vWheelPosGlobal = Transform(matTest,vWheelOffsets[w]);

				const bool bIsFrontWheel = !(w & 0x1); // Rear wheels are 1 and 3
				if(bIsFrontWheel || !bUseSeparateRouteInfoForRearWheels)
				{
					CVirtualRoad::ProcessRoadCollision(routeInfo, vWheelPosGlobal, wheelCollisions[w], NULL);
				}
				else
				{
					CVirtualRoad::ProcessRoadCollision(routeInfoRearWheels, vWheelPosGlobal, wheelCollisions[w], NULL);
				}

				const Vec3V vHitNormal(wheelCollisions[w].m_vNormal);

				// This offset may have been an attempt to account for vehicles with wheels that have different 
				// different rest heights as measured in the up-axis of the vehicle.  But, in its current form the
				// distances weren't correct and were causing wheel offsets for large vehicles (e.g. busses and dump trucks B*746339)
				// without actually fixing any vehicles.
				//wheelCollisions[w].m_vPosition.SetZ( wheelCollisions[w].m_vPosition.GetZ() - (vWheelOffsets[w].GetZ() + fWheelRadii[w]) );

				fHitHeightsLocal[w] = Dot(wheelCollisions[w].m_vPosition.GetXYZ(),vVehicleUpCurrent) - fVehicleHeightAlongVehUp;

				// Remember that "TWheelCollision::m_vPosition.w" holds the distance of the wheel above the surface plane
				vAverageRoadToWheelOffset += vHitNormal * (wheelCollisions[w].m_vPosition.GetW() - fWheelRadii[w]);
			}
		}

		if( numWheels == 3 )
		{
			for(int w = 0; w < 4; w++)
			{
				if( !pWheels[ w ] )
				{
					if( w < 2 )
					{
						wheelCollisions[ w ] = wheelCollisions[ w + 2 ];
					}
					else
					{
						wheelCollisions[ w ] = wheelCollisions[ w - 2 ];
					}
				}
			}
		}

		vAverageRoadToWheelOffset *= fInvNumWheels;

#if DEBUG_DRAW && __BANK
		if(CVehicleAILodManager::ms_bDisplayDummyWheelHits)
		{
			for(int w=0; w<iNumWheels; w++)
			{
				grcDebugDraw::Cross( Vec3V(wheelCollisions[w].m_vPosition.GetIntrin128ConstRef()), 0.25f, Color_magenta, -1);
			}
		}
#endif

		//---------------------------------------------------------------
		// Work out pitch & roll required to keep vehicle on the road

		Vec3V vWheelsMid;
		if(iNumWheels == 2)
		{
			vWheelsMid = Vec3V(Average(wheelCollisions[iWheelLeftFront].m_vPosition, wheelCollisions[iWheelLeftRear].m_vPosition).GetIntrin128ConstRef());
			vToFront = Vec3V(wheelCollisions[iWheelLeftFront].m_vPosition.GetIntrin128ConstRef()) - vWheelsMid;

#if __DEV
			if (!aiVerifyf(IsFiniteAll(vToFront), "vToFront bad after vToFront = Vec3V(wheelCollisions[iWheelLeftFront].m_vPosition.GetIntrin128ConstRef()) - vWeelsMid"))
			{
				vWheelsMid.Print("vWheelsMid");
				for (int debugI = 0; debugI < iWheelRightRear; debugI++)
				{
					Displayf("wheelCollisions[%d].m_vPosition, %.2f, %.2f, %.2f", debugI, wheelCollisions[debugI].m_vPosition.GetXf(), wheelCollisions[debugI].m_vPosition.GetYf(), wheelCollisions[debugI].m_vPosition.GetZf());
				}
			}
#endif //__DEV
		}
		else
		{
			const Vec3V vWheelFrontMid = Vec3V(Average(wheelCollisions[iWheelLeftFront].m_vPosition, wheelCollisions[iWheelRightFront].m_vPosition).GetIntrin128ConstRef());
			const Vec3V vWheelRearMid = Vec3V(Average(wheelCollisions[iWheelLeftRear].m_vPosition, wheelCollisions[iWheelRightRear].m_vPosition).GetIntrin128ConstRef());
			vWheelsMid = Average(vWheelFrontMid, vWheelRearMid);
			vToFront = vWheelFrontMid - vWheelsMid;
#if __DEV
			if (!aiVerifyf(IsFiniteAll(vToFront), "vToFront bad after vToFront = vWheelFrontMid - vWeelsMid"))
			{
				Displayf("vWheelFrontMid, %.2f, %.2f, %.2f", vWheelFrontMid.GetXf(), vWheelFrontMid.GetYf(), vWheelFrontMid.GetZf());
				Displayf("vWheelRearMid, %.2f, %.2f, %.2f", vWheelRearMid.GetXf(), vWheelRearMid.GetYf(), vWheelRearMid.GetZf());

				for (int debugI = 0; debugI <= iWheelRightRear; debugI++)
				{
					Displayf("wheelCollisions[%d].m_vPosition, %.2f, %.2f, %.2f", debugI, wheelCollisions[debugI].m_vPosition.GetXf(), wheelCollisions[debugI].m_vPosition.GetYf(), wheelCollisions[debugI].m_vPosition.GetZf());
				}

				Displayf("m_iNumPoints = %d",m_iNumPoints);
				for(int i=0; i<m_iNumPoints; i++)
				{
					m_RoutePoints[i].GetPosition().Print("route point");
				}
			}
#endif //__DEV
		}

		vToFront = Normalize(vToFront);

		Assertf(IsFiniteAll(vToFront), "vToFront bad after vToFront = Normalize(vToFront);");

		//-----------------------------------------------------
		// Do the same for roll, since roads can have camber.

		if(iNumWheels==2)
		{
			vToRight = Cross( vToFront, RCC_VEC3V(ZAXIS) );
			const BoolV bTest = IsLessThan( MagSquared(vToRight), fSmallFloat );
			vToRight = SelectFT( bTest, vToRight, Cross( vVehicleUpCurrent, RCC_VEC3V(ZAXIS)) );
		}
		else
		{
			// Calculate vToRight.  vQ is the point on the line from the back-right wheel to the front-right wheel that is
			// on the plane perpendicular to vToFront and passes through vWheelsMid.  vToRight = vQ - vWheelsMid (not normalized yet).
			const Vec3V vWheelRightRear = wheelCollisions[iWheelRightRear].m_vPosition.GetXYZ();
			const Vec3V vWheelRightFront = wheelCollisions[iWheelRightFront].m_vPosition.GetXYZ();
			const Vec3V vWheelRightRearToFront = vWheelRightFront - vWheelRightRear;
			const ScalarV fT =  Dot(vToFront,vWheelsMid-vWheelRightRear) / Dot(vToFront,vWheelRightRearToFront);
			const Vec3V vQ = vWheelRightRear + fT * vWheelRightRearToFront;
			vToRight = vQ - vWheelsMid;
		}

		vToRight = Normalize(vToRight);
	}

	// Optionally set wheel compression
	bool bSetWheelCompression = !bPreventWheelCompression &&
								((bConsiderSettingWheelCompression && !pVehicle->GetFlagLodFarFromPopCenter()) || !pVehicle->GetFlagSuperDummyWheelCompressionSet());
	if(bSetWheelCompression)
	{
		// TODO: Is this a good place for the prefetch?
		for(int w=0; w<iNumWheels; w++)
		{
			if( pWheels[ w ] )
			{
				PrefetchObject(pWheels[w]);
			}
		}
	}

	// Update constraint or calc new position that is constraint to the road
	if(bCalcConstraintParams)
	{
		// If we are processing all the wheels, then we won't have a central sample and must sample the constraint explicitly.
		if(bProcessAllWheels)
		{
			const Vec3V vWheelPosGlobal = vVehiclePosCurrent + vWheelOffsets[0].GetZ() * vVehicleUpCurrent;
			CVirtualRoad::ProcessRouteConstraint(routeInfo, vWheelPosGlobal, routeConstraint, false);
		}

		const Vec3V vConstraintPos = routeConstraint.m_vConstraintPosOnRoute;
		if(bUpdateDummyConstraint)
		{
			// Create a physics constraint
			
			const ScalarV fCurrentDist = Dist(vVehiclePosCurrent,routeConstraint.m_vConstraintPosOnRoute);
			if(IsGreaterThanAll(fCurrentDist,routeConstraint.m_fConstraintRadiusOnRoute+ScalarV(100.0f))
				|| routeConstraint.m_bJunctionConstraint)
			{
				const Vec3V vWheelPosGlobal = vVehiclePosCurrent + vWheelOffsets[0].GetZ() * vVehicleUpCurrent;
				CVirtualRoad::ProcessRouteConstraint(routeInfo, vWheelPosGlobal, routeConstraint, false);
			}

			const float fConstraintMaxDistAsFloat = routeConstraint.m_fConstraintRadiusOnRoute.Getf();
			pVehicle->UpdateDummyConstraints(&routeConstraint.m_vConstraintPosOnRoute, &fConstraintMaxDistAsFloat);
		}
		else if(pNewPosConstrainedToRoadWidth)
		{
			Vec3V vVehPosAfterConstraintTeleport = vVehiclePosCurrent;
			// Teleport according to the constraint limits
			aiAssertf(pVehicle->GetIsStatic(),"Should only consider teleporting static vehicles.  Active vehicles should use physical constraints.");
			const ScalarV fConstraintMaxDist = routeConstraint.m_fConstraintRadiusOnRoute;
			const ScalarV fCurrentDistSq = DistSquared(vVehiclePosCurrent,vConstraintPos);
			if(IsGreaterThanAll(fCurrentDistSq,square(fConstraintMaxDist)))
			{
				const Vec3V vVehPosNew = Lerp(fConstraintMaxDist/Sqrt(fCurrentDistSq),vConstraintPos,vVehiclePosCurrent);
#if __BANK
				if(CVehicleAILodManager::ms_bValidateDummyConstraints)
				{
					Displayf("Super-dummy vehicle is outside of road width - at %0.2f %0.2f, length %0.2f, dist %0.2f",vVehiclePosCurrent.GetXf(),vVehiclePosCurrent.GetYf(),fConstraintMaxDist.Getf(),sqrtf(fCurrentDistSq.Getf()));
					DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(vVehiclePosCurrent,0.2f,Color_cyan,false,-1);)
					DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(vVehPosNew,0.2f,Color_magenta,false,-1);)
				}
				else
#endif
				{
					vVehPosAfterConstraintTeleport = vVehPosNew;
				}
			}
			*pNewPosConstrainedToRoadWidth = vVehPosAfterConstraintTeleport;
		}
	}

	if(bSetWheelCompression)
	{
		if(bProcessAllWheels)
		{
			for(int w=0; w<iNumWheels; w++)
			{
				if( pWheels[w] )
				{
					pWheels[w]->SetCompressionFromGroundHeightLocalAndHitPos(fHitHeightsLocal[w].Getf(),RCC_VECTOR3(wheelCollisions[w].m_vPosition),RCC_VECTOR3(vVehicleUpCurrent));
				}
			}
		}
		else
		{
			for(int w=0; w<iNumWheels; w++)
			{
				if( pWheels[ w ] )
				{
					Vec3V vWheelOffsetFlat;
					RC_VECTOR3(vWheelOffsetFlat) = CWheel::GetWheelOffset(pModelInfo, iWheelIDs[w]);
					vWheelOffsetFlat.SetZ(fZero);
					Vec3V vWorldHitPos = Transform(matTest,vWheelOffsetFlat);
					vWorldHitPos = vWorldHitPos + vVehicleUpCurrent * fHitHeightsLocal[0];
					pWheels[w]->SetCompressionFromGroundHeightLocalAndHitPos(fHitHeightsLocal[0].Getf(),RCC_VECTOR3(vWorldHitPos),RCC_VECTOR3(vVehicleUpCurrent));
				}
			}
		}
	
		// Middle wheels need to have compression set as well, otherwise they could pop the vehicle on transition back to dummy.
		if(iNumWheelsVehicle>=6)
		{
			Assertf(iNumWheelsVehicle==6 || iNumWheelsVehicle==8 || iNumWheelsVehicle==10,"Vehicle with %d wheel in ProcessSuperDummy",iNumWheelsVehicle);
			// TODO: For now, just reset, later it would be better to set them the same as other wheels 
			CWheel * pWheel;
			pWheel = pVehicle->GetWheelFromId(VEH_WHEEL_LM1);
			if(pWheel) pWheel->Reset();
			pWheel = pVehicle->GetWheelFromId(VEH_WHEEL_RM1);
			if(pWheel) pWheel->Reset();
			if(iNumWheelsVehicle>=8)
			{
				pWheel = pVehicle->GetWheelFromId(VEH_WHEEL_LM2);
				if(pWheel) pWheel->Reset();
				pWheel = pVehicle->GetWheelFromId(VEH_WHEEL_RM2);
				if(pWheel) pWheel->Reset();
				if(iNumWheelsVehicle>=10)
				{
					pWheel = pVehicle->GetWheelFromId(VEH_WHEEL_LM3);
					if(pWheel) pWheel->Reset();
					pWheel = pVehicle->GetWheelFromId(VEH_WHEEL_RM3);
					if(pWheel) pWheel->Reset();
				}
			}
		}

		pVehicle->SetFlagSuperDummyWheelCompressionSet(true);
	}


	const Vec3V vUp = Cross(vToRight, vToFront);
	vToRight = Cross(vToFront, vUp);

#if __ASSERT
	Mat34V orthoMtx;
	orthoMtx.SetCol0(vToRight);
	orthoMtx.SetCol1(vToFront);
	orthoMtx.SetCol2(vUp);
	aiAssertf(orthoMtx.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)),
			"Right/forward vectors not orthonormal in CalculatePositionAndOrientationOnVirtualRoad()? (vRight:%.3f,%.3f,%.3f - vForward:%.3f,%.3f,%.3f - bProcessAllWheels:%d - iNumWheels:%d)",
			vToRight.GetXf(), vToRight.GetYf(), vToRight.GetZf(),
			vToFront.GetXf(), vToFront.GetYf(), vToFront.GetZf(),
			(int)bProcessAllWheels, iNumWheels);
#endif	// __ASSERT

	// Output
	vToFrontOut = vToFront;
	vToRightOut = vToRight;
	vAverageRoadToWheelOffsetOut = vAverageRoadToWheelOffset;

	return true;
}

// NAME : ProcessSuperDummy
// PURPOSE : Process everything needed to place the super-dummy on the road
//			 Update steering inputs & set collision data into vehicle wheels
bool CVehicleFollowRouteHelper::ProcessSuperDummy(CVehicle * pVehicle, ScalarV_In fCruiseSpeed, s32 DrivingFlags, const float& fTimeStepIn) const
{
	const ScalarV fZero( V_ZERO );
	const ScalarV fMinusOne( V_NEGONE );
	const ScalarV fOne( V_ONE );
	const ScalarV fSmallFloat( V_FLT_SMALL_6 );

	// TODO: Create a tunables struct for CVehicleFollowRouteHelper and move these and other tunables into there.
	const ScalarV fMaxAccel(10.0f);
	const ScalarV fMaxCarAccel(5.0f);
	const ScalarV fMaxCarAccelNetwork(4.0f);
	const ScalarV fMaxDecel(15.0f);
	const ScalarV fPlaceOnRoadSpringConst(7.0f);
	const ScalarV fPlaceOnRoadSpeedMax(35.0f);
	const ScalarV fPlaceOnRoadSpeedMin(3.0f);

	const ScalarV fTimeStep(fTimeStepIn);

	//Don't process super dummies if they are supposed to be fixed.
	if(pVehicle->GetIsFixedFlagSet())
	{
		return false;
	}

	//we used to assert here, but it's expected in the general case that we'll be
	//calling the super dummy update twice a frame from within a task.
	//the only reason to not want to do this is that it's possible for tasks to
	//teleport an entity from within the task update. however, right now none of the
	//tasks that include one of these helpers do that.
	if (pVehicle->m_nVehicleFlags.bDummyWheelImpactsSampled)
	{
		return true;
	}

	const int iNumWheelsVehicle = pVehicle->GetNumWheels();
	if(iNumWheelsVehicle!=2 && iNumWheelsVehicle<4)
	{
		// ProcessSuperDummy doesn't support 0, 1, or 3 wheels yet.
		return false;
	}

	if (m_iNumPoints <= 1)
	{
		// ProcessSuperDummy requires route points
		return false;
	}

	const bool bIsStatic = pVehicle->GetIsStatic();
	bool bDoInactivePhysics = CVehicleAILodManager::ms_bDisablePhysicsInSuperDummyMode && bIsStatic;
	bool bDoTimeslicePhysics = bDoInactivePhysics && pVehicle->GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodTimeslicing);

 	const Vec3V vVehicleBonnetPos = GetVehicleBonnetPosition(pVehicle, (DrivingFlags & DF_DriveInReverse) != 0);

	// Constraint mode
 	aiAssertf(CVehicleAILodManager::GetSuperDummyConstraintMode() != CVehicleAILodManager::DCM_PhysicalDistanceAndRotation,"Super-dummies don't need rotation constraint.");
 	const bool bCreatePhysicalConstraint = 
 		(CVehicleAILodManager::GetSuperDummyConstraintMode()==CVehicleAILodManager::DCM_PhysicalDistance)
 		&& (!bIsStatic || !CVehicleAILodManager::ms_bDisableConstraintsForSuperDummyInactive);
 	const bool bConsiderTeleportingToConstrain = 
		(CVehicleAILodManager::GetSuperDummyConstraintMode()==CVehicleAILodManager::DCM_PhysicalDistance)
		&& bIsStatic && CVehicleAILodManager::ms_bDisableConstraintsForSuperDummyInactive; // TODO: Could add timeslicing to this check later.

	// 
	const Mat34V & matCurrent = pVehicle->GetTransform().GetMatrix();
 	Assert(IsFiniteAll(matCurrent));
	const Vec3V vVehicleRightCurrent = matCurrent.GetCol0();
	const Vec3V vVehicleForwardCurrent = matCurrent.GetCol1();
 	const Vec3V vVehiclePosCurrent = matCurrent.GetCol3();

	//
	Vec3V vToFront, vToRight(V_ZERO), vAverageRoadToWheelOffset(V_ZERO);
	Vec3V vPosAfterConstrainingToRoad = vVehiclePosCurrent;
	ScalarV fPitchRate(V_ZERO), fRollRate(V_ZERO);
	if(!bDoTimeslicePhysics)
	{
		//---------------------------------------------------------------------------
		// When not timeslicing, calculate the on-road position and orientation now
		bool bRoadSuccess = CalculatePositionAndOrientationOnVirtualRoad
			(pVehicle, matCurrent, &vVehicleBonnetPos, true, bCreatePhysicalConstraint, vToFront, vToRight, vAverageRoadToWheelOffset, (bConsiderTeleportingToConstrain ? &vPosAfterConstrainingToRoad : NULL));
		if(!bRoadSuccess)
		{
			return false;
		}

		//---------------------------------------------------------------------------
		// Find rate of pitch to keep the vehicle aligned to the road
		{
			// This assumes that vToFront didn't end up being 0 (or 0,0,z).  If that actually is a problem, then we can assign an arbitrary orientation.
			const Vec3V vToFrontFlat = NormalizeFast(Vec3V(vToFront.GetX(),vToFront.GetY(),fZero));

			// Rate of pitch comes from the delta between vehicle and road pitch, scaled up by some approach rate scale.
			const ScalarV fDotFront = Dot(RCC_VEC3V(ZAXIS), vToFront);
			const ScalarV fSign = SelectFT(IsGreaterThan(fDotFront,fZero),fMinusOne,fOne);
			const ScalarV fPitchDot = Clamp( Dot(vToFront, vToFrontFlat) , fMinusOne, fOne );
			const ScalarV fDesiredPitch = Scale( ArccosFast( fPitchDot), fSign );
			static ScalarV fPitchRateScale(3.0f);
			fPitchRate = (fDesiredPitch - ScalarV(pVehicle->GetTransform().GetPitch())) * fPitchRateScale;
		}

		//---------------------------------------------------------------------------
		// Do the same for roll, since roads can have camber.
		{
			const BoolV bTest = IsLessThan( Dot( vToFront, Cross(vToRight, vVehicleRightCurrent) ), fZero );
			const ScalarV fSign = SelectFT( bTest, fMinusOne, fOne);
			const ScalarV fRollDot = Clamp( Dot(vToRight, vVehicleRightCurrent), fMinusOne, fOne);
			fRollRate = Scale( ArccosFast(fRollDot), fSign);
		}
	}
	else
	{
		//---------------------------------------------------------------------------
		// When timeslicing the interpolation target will be calculated from the path and virtual road below.
		// Only the current forward vector is needed for calculating a new velocity.
		vToFront = vVehicleForwardCurrent;
	}

	//-----------------------------------------------------------------------------------------------------
	// Update speed
	// Work out a new forwards velocity based upon task's cruise speed, existing velocity & throttle/brake
	Vec3V vNewVelocity;
	ScalarV fNewVelMag;
	{
		ScalarV fThrottle(pVehicle->GetThrottle());
		ScalarV fBrake(pVehicle->GetBrake());

		const Vec3V vCurrentVel = VECTOR3_TO_VEC3V(pVehicle->GetAiVelocity());
		Assertf(IsFiniteAll(vCurrentVel) && IsLessThanAll(MagSquared(vCurrentVel), ScalarV(250000.0f)),"vCurrentVel is bad.");
		const ScalarV fCurrentVelToFrontMag = Dot(vCurrentVel, vToFront);

		fThrottle = Clamp(fThrottle, fZero, fOne);
		fBrake = Clamp(fBrake, fZero, fOne);
		ScalarV fDriveSpeed = Scale(fCruiseSpeed, fThrottle);

		vNewVelocity = Scale(vToFront, fDriveSpeed);
		Assertf(IsFiniteAll(vNewVelocity) && IsLessThanAll(MagSquared(vNewVelocity), ScalarV(250000.0f)), "vNewVelocity bad after vToFront * fDriveSpeed.");

		fNewVelMag = MagFast( vNewVelocity );

		// If driven velocity is less than current velocity, then scale up
		if(IsLessThanAll(fNewVelMag, fCurrentVelToFrontMag))
		{
			const BoolV bTest = IsGreaterThan(fNewVelMag, fZero);
			vNewVelocity = SelectFT(
				bTest,
				Scale(vToFront, fCurrentVelToFrontMag),
				Scale(vNewVelocity, InvScale(fCurrentVelToFrontMag, fNewVelMag) )
			);
			fNewVelMag = fCurrentVelToFrontMag;
			Assertf(IsFiniteAll(vNewVelocity) && IsLessThanAll(MagSquared(vNewVelocity), ScalarV(250000.0f)),"vNewVelocity bad after scaling up to fCurrentVelToFrontMag.");
		}

		// If less than cruise speed, then accelerate up to cruise speed
		if(IsLessThanAll(fNewVelMag,fCruiseSpeed))
		{
			// JB: No need to apply fAccel scale, so long as it remains 1.0
			//static const ScalarV fAccel(V_ONE);
			//vNewVelocity = vNewVelocity + Scale(vToFront, Scale(fAccel, fTimeStep));
			vNewVelocity = vNewVelocity + Scale(vToFront, fTimeStep);
			Assertf(IsFiniteAll(vNewVelocity) && IsLessThanAll(MagSquared(vNewVelocity), ScalarV(250000.0f)),"vNewVelocity bad after scaling up to fCruiseSpeed.");
			fNewVelMag = MagFast( vNewVelocity );
		}

		// If now greater than cruise speed, then reduce into range
		if(IsGreaterThanAll(fNewVelMag, fCruiseSpeed))
		{
			const BoolV bTest = IsGreaterThan(fNewVelMag, fZero);
			vNewVelocity = SelectFT(
				bTest,
				Scale(vToFront, fCruiseSpeed),
				Scale(vNewVelocity, InvScale(fCruiseSpeed, fNewVelMag))
			);
			fNewVelMag = fCruiseSpeed;
			Assertf(IsFiniteAll(vNewVelocity) && IsLessThanAll(MagSquared(vNewVelocity), ScalarV(250000.0f)),"vNewVelocity bad after scaling down to fCruiseSpeed.");
		}

		// Apply brake on top of all the above
		vNewVelocity = Scale(vNewVelocity, fOne - fBrake);
		Assertf(IsFiniteAll(vNewVelocity) && IsLessThanAll(MagSquared(vNewVelocity), ScalarV(250000.0f)),"vNewVelocity bad after applying brake.");
		fNewVelMag = MagFast( vNewVelocity );

		// Limit acceleration and deceleration
		{
			// Allow arbitrary change of direction (i.e. lateral grip is infinite) but limit speed increase or decrease
			const ScalarV fCurrentMaxCarAccel = NetworkInterface::IsGameInProgress() ? fMaxCarAccelNetwork : fMaxCarAccel;
			const ScalarV fMaxSpeedIncrease = ( pVehicle->GetIsLandVehicle() ? fCurrentMaxCarAccel : fMaxAccel ) * fTimeStep;
			const ScalarV fMaxSpeedDecrease = fMaxDecel * fTimeStep;
			const ScalarV fCurrentVelMag = MagFast(vCurrentVel);
			const ScalarV fDeltaSpeedDesired = fNewVelMag - fCurrentVelMag;
			if(IsGreaterThanAll(fDeltaSpeedDesired,fMaxSpeedIncrease) || IsLessThanAll(fDeltaSpeedDesired,-fMaxSpeedDecrease))
			{
				// This assumes that at least one of vNewVelocity or vCurrentVel is non-zero.  Which should be true given non-zero values for fMaxAccel and fMaxDecel.
				BoolV bNewVelSmall = (fNewVelMag<fSmallFloat);
				const Vec3V vNewDir = SelectFT(bNewVelSmall,vNewVelocity,vCurrentVel);
				const ScalarV fNewDirMag = SelectFT(bNewVelSmall,fNewVelMag,fCurrentVelMag);
				const ScalarV fNewVelClampedMag = (fCurrentVelMag + Clamp(fDeltaSpeedDesired,-fMaxSpeedDecrease,fMaxSpeedIncrease));
				const Vec3V vNewVelClamped = vNewDir * (fNewVelClampedMag / fNewDirMag);
				Assertf(IsFiniteAll(vNewVelClamped) && IsLessThanAll(MagSquared(vNewVelClamped), ScalarV(250000.0f)),"vNewVelClamped not finite. (%0.2f,%0.2f) %0.2f, (%0.2f,%0.2f) %0.2f",vCurrentVel.GetXf(),vCurrentVel.GetYf(),fCurrentVelMag.Getf(),vNewVelocity.GetXf(),vNewVelocity.GetYf(),fNewVelMag.Getf());
				vNewVelocity = vNewVelClamped;
				fNewVelMag = fNewVelClampedMag;
			}
		
			//superdummy uses desired cruise speed to calculate speed, but desired isn't limited to actual top speed
			//so ensure max velocity doesn't exceed our estimated max velocity
			if(pVehicle->GetIsLandVehicle() && pVehicle->pHandling)
			{
				Assertf(pVehicle->pHandling->m_fEstimatedMaxFlatVel > 0.0f, "%s does not have an estimated max velocity", pVehicle->GetModelName());
				ScalarV fMaxVelocity(pVehicle->pHandling->m_fEstimatedMaxFlatVel);
				if(IsGreaterThanAll(fNewVelMag, fMaxVelocity) && !IsEqualAll(fMaxVelocity, ScalarV(V_ZERO)))
				{
					vNewVelocity = Scale(vNewVelocity, fMaxVelocity / fNewVelMag);
					fNewVelMag = fMaxVelocity;
					Assertf(IsFiniteAll(vNewVelocity) && IsLessThanAll(MagSquared(vNewVelocity), ScalarV(250000.0f)),"vNewVelocity bad after scaling to max velocity.");
				}
			}
		}

		// During timeslicing, the interpolation target will be on the road, so we don't need to otherwise apply velocity toward the road
		if(!bDoTimeslicePhysics && IsGreaterThanAll(fTimeStep, ScalarV(V_ZERO)))
		{
			// Placing onto the road...
			// Set velocity in direction normal to the road to approach the road in proportion to the 
			// distance from the road (within a min-max range) and to not go past the road on any frame.
			// TODO: Later we may want to add more sophisticated behavior for an active super dummy, but
			// first lets get them on the road cleanly and then define a more physical behavior later if desired.
			const Vec3V vToUp = Cross(vToRight,vToFront);
			const ScalarV fCurSpeedToRoad = Dot(vNewVelocity,vToUp);
			const ScalarV fCurDistToRoad = Dot(vAverageRoadToWheelOffset,vToUp);
			const ScalarV fPlaceOnRoadSpeedDesired = Abs(fCurDistToRoad * fPlaceOnRoadSpringConst);
			const ScalarV fPlaceOnRoadSpeedToHitNextFrame = Abs(fCurDistToRoad / fTimeStep);
			const ScalarV fPlaceOnRoadSpeedClamped = rage::Min(fPlaceOnRoadSpeedToHitNextFrame,rage::Clamp(fPlaceOnRoadSpeedDesired,fPlaceOnRoadSpeedMin,fPlaceOnRoadSpeedMax));
			const ScalarV fPlaceOnRoadSpeed = SelectFT(fCurDistToRoad>fZero,fPlaceOnRoadSpeedClamped,-fPlaceOnRoadSpeedClamped);
			const ScalarV fPlaceOnRoadCorrection = fPlaceOnRoadSpeed - fCurSpeedToRoad;
			const Vec3V vPlaceOntoRoadVelocity = vToUp * fPlaceOnRoadCorrection;
			Assert(IsFiniteAll(vPlaceOntoRoadVelocity) && IsLessThanAll(MagSquared(vPlaceOntoRoadVelocity), ScalarV(250000.0f)));
			vNewVelocity += vPlaceOntoRoadVelocity;
		}
	}

	//-----------------------------------------------------------------------
	// Update turning
	// When timeslicing, heading comes from the path.  Otherwise it comes from steering inputs.
	ScalarV fYawRate(V_ZERO);
	if(!bDoTimeslicePhysics)
	{
		// Changes at fixed rate in proportion to AI steering angle, limited for low velocities
		const float fSteerAngleAi = pVehicle->GetSteerAngle();
		const ScalarV fHeadingRateDesired(fSteerAngleAi * CVehicleAILodManager::ms_fSuperDummySteerSpeed);

		// Limit yaw rate of change.  Grows from 0 at speed 0 to fYawRateMax at fYawRateMaxSpeed
		static dev_float fYawRateMaxFloat_Default = 1.0f;
		static dev_float fYawRateMaxSpeedFloat_Default = 3.0f;
		static dev_float fYawRateMaxFloat_Big = 0.40f;
		float fYawRateMaxEffective = fYawRateMaxFloat_Default;
		if( pVehicle->m_nVehicleFlags.bIsBig )
		{
			fYawRateMaxEffective = fYawRateMaxFloat_Big;
		}
		const ScalarV fYawRateMax(fYawRateMaxEffective);
		const ScalarV fYawRateMaxSpeed(fYawRateMaxSpeedFloat_Default);
		const ScalarV fYawRateMaxForSpeed = fYawRateMax * Min(fOne,fNewVelMag/fYawRateMaxSpeed);
		fYawRate = Clamp(fHeadingRateDesired,-fYawRateMaxForSpeed,fYawRateMaxForSpeed);
	}

	//-----------------------------------------------------------------------
	// Update the vehicle - by setting physical velocities, or teleporting, or updating the inerpolation target

	if(bDoTimeslicePhysics)
	{
		// Update interpolation target
		UpdateTimeslicedInterpolationTarget(*pVehicle, vNewVelocity, fTimeStep);
		// Clear the hit flag for timesliced vehicles.  It is causing physics pops on transition and there is no 
		// dummy transition surface for timeslice vehicles right now anyways so the hit isn't used.
		CWheel * const * ppWheels = pVehicle->GetWheels();
		for(int w=0; w<iNumWheelsVehicle; w++)
		{
			ppWheels[w]->GetDynamicFlags().ClearFlag(WF_HIT);
		}
	}
	else
	{
		// Rotation angles from desired pitch, roll, yaw rates
		Assert(IsFiniteAll(fPitchRate));
		Assert(IsFiniteAll(fRollRate));
		Assert(IsFiniteAll(fYawRate));
		const Vec3V vLocalRotAngles(fPitchRate,fRollRate,fYawRate);

		if(bDoInactivePhysics)
		{
			// Teleport the vehicle

			// Note: since we keep updating the position on every frame even when timeslicing,
			// we would probably never want to use the "AI" time step here, but rather some
			// position/physics time step matching the time since the last update, so fwTimer::GetTimeStep()
			// is intentionally used here.
			const ScalarV fPosTimeStep(fwTimer::GetTimeStep());

			Vec3V vXYZ = Mat34VToEulersXYZ(matCurrent);
			vXYZ += vLocalRotAngles * fPosTimeStep;
			Mat34V mFinal;
			Mat34VFromEulersXYZ(mFinal,vXYZ,vPosAfterConstrainingToRoad + vNewVelocity * fPosTimeStep);

			//Assert(IsLessThanAll(Dist(vVehiclePosCurrent, mFinal.d()), ScalarV(V_TEN)));

			static dev_bool bUpdateWorld = true;
			static dev_bool bUpdatePhysics = true;
			static dev_bool bWarp = false;
			pVehicle->SetMatrix(RCC_MATRIX34(mFinal), bUpdateWorld, bUpdatePhysics, bWarp);
			pVehicle->SetSuperDummyVelocity(RCC_VECTOR3(vNewVelocity));
		}
		else
		{
			// Update physical velocities

			// Have to clamp to max speed or physics will assert
			const ScalarV fSpeedScale = Mag(vNewVelocity) / ScalarV(pVehicle->GetCurrentPhysicsInst()->GetArchetype()->GetMaxSpeed());
			vNewVelocity = InvScale( vNewVelocity, SelectFT( IsGreaterThan(fSpeedScale, fOne), fOne, fSpeedScale ) );
			Assertf(IsFiniteAll(vNewVelocity),"vNewVelocity is bad after applying fSpeedScale.");

			// Set linear velocity
			pVehicle->SetVelocity(RCC_VECTOR3(vNewVelocity));

			// Rotate angular velocity into worldspace & apply
			const Vec3V vRotAngles = Transform3x3( matCurrent, vLocalRotAngles );
			pVehicle->SetAngVelocity( RCC_VECTOR3(vRotAngles) );
		}
	}

	pVehicle->m_nVehicleFlags.bDummyWheelImpactsSampled = true;

	return true;
}

#define DEBUG_DRAW_INTERPOLATION_TARGET 0

/*
PURPOSE
	Finalize the ProcessSuperDummy() update for a vehicle in timesliced mode, updating
	the vehicle's current matrix and computing a future position that we will interpolate
	towards while waiting for the next full update step.
PARAMS
	veh				- The vehicle to affect.
	newVelocityV	- The current desired velocity, as computed by ProcessSuperDummy().
	timeStepInV		- The time that has elapsed since the last full update.
*/
void CVehicleFollowRouteHelper::UpdateTimeslicedInterpolationTarget(CVehicle& veh,
		Vec3V_In newVelocityInV, ScalarV_In timeStepInV) const
{
	// Prepare some input parameters.
	const Vec3V newVelocityV = newVelocityInV;
	const ScalarV timeStepV = timeStepInV;
	const Mat34V& matCurrent = veh.GetTransform().GetMatrix();
	const Vec3V vVehiclePosCurrent = matCurrent.GetCol3();
	Vec3V vVehiclePosRoadHeight = vVehiclePosCurrent;
	const ScalarV DistanceAboveRoad(veh.GetHeightAboveRoad());
	vVehiclePosRoadHeight = SubtractScaled( vVehiclePosRoadHeight, matCurrent.GetCol2(), DistanceAboveRoad );

	// Check for inputs that could lead to teleports to bad positions.
	//aiAssertf(IsLessThanAll(MagSquared(newVelocityV), ScalarV(250000.0f)),
	//		"Got huge velocity: %.1f, %.1f, %.1f", newVelocityV.GetXf(), newVelocityV.GetYf(), newVelocityV.GetZf());
	aiAssertf(g_WorldLimits_AABB.ContainsPoint(vVehiclePosCurrent), "Vehicle's current position is outside world limits: %.1f, %.1f, %.1f",
			vVehiclePosCurrent.GetXf(), vVehiclePosCurrent.GetYf(), vVehiclePosCurrent.GetZf());
	aiAssertf(IsLessThanAll(timeStepInV, ScalarV(V_FIVE)) && IsGreaterThanOrEqualAll(timeStepInV, ScalarV(V_ZERO)),
			"Got large time step: %.1f", timeStepInV.Getf());

	// We will need to compute two matrices, the vehicle's new matrix for this frame,
	// and the interpolation target for future frames.
	Mat34V mNewVehicleMatrix(matCurrent);
	Mat34V mInterpolationTarget(matCurrent);

	// Compute how far ahead in time we should put the interpolation target. It probably wouldn't
	// work well to just use timeStepV, because that's the the time since the last update which may
	// not be the same as the time for the next full update, as there will be some variation in the
	// update frequency. So, we add some extra time to allow for that variation. Also, when we first
	// switch into timesliced mode, the very first time step may just be a single frame, so we also
	// make sure to use a minimum time. If our computed time step here is too small there will
	// be a potentially visible interruption in the movement. If it's larger than the actual next time
	// step it's not a problem, we will just do the next update based on however far we actually moved
	// at that point.
	const ScalarV minInterpolationTimeV(0.4f);		// MAGIC!
	const ScalarV extraInterpolationTimeV(0.15f);	// MAGIC!
	ScalarV futureTimeStepV = Max(Add(timeStepV, extraInterpolationTimeV), minInterpolationTimeV);

	// The stuff we did above isn't always very accurate - in particular, when we spawn in we tend to
	// do updates a couple of frames in a row, which means that the time step can get too small
	// to use for the immediate future. We don't let it go below what CVehicleAILodManager thinks
	// could happen.
	ScalarV expectedWorstCaseFromLodManagerV = ScalarV(CVehicleAILodManager::GetExpectedWorstTimeslicedTimestep());
	futureTimeStepV = Max(futureTimeStepV, expectedWorstCaseFromLodManagerV);

	// Compute a position based on extrapolating the new velocity for the computed time step.
	Vec3V extrapolatedPosV, extrapolatedForwardV(matCurrent.GetCol1()), extrapolatedRightV(matCurrent.GetCol0());
	extrapolatedPosV = AddScaled(vVehiclePosCurrent, newVelocityV, futureTimeStepV);
	bool bExtrapolatedPosOnPathFound = false;

#if __ASSERT
	Mat34V orthoMtx;
	orthoMtx.SetCol0(extrapolatedRightV);
	orthoMtx.SetCol1(extrapolatedForwardV);
	orthoMtx.SetCol2(Cross(extrapolatedRightV, extrapolatedForwardV));
	aiAssertf(orthoMtx.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)),
			"Right/forward vectors from input matrix not orthonormal enough? (vRight:%.3f,%.3f,%.3f - vForward:%.3f,%.3f,%.3f)", 
			extrapolatedRightV.GetXf(), extrapolatedRightV.GetYf(), extrapolatedRightV.GetZf(), 
			extrapolatedForwardV.GetXf(), extrapolatedForwardV.GetYf(), extrapolatedForwardV.GetZf());
#endif	// __ASSERT


	bool gotClosestPosOnSegment = false;
	Vec3V closestPosOnSegmentV;
	Vec3V closestPosDirOnSegmentV;

	// Check if we have a straight line position...
	if(m_GotStraightLineTargetPos)
	{
		// Consider a segment from the current position to the target.
		const Vec3V posOnPath1V = vVehiclePosCurrent;
		const Vec3V posOnPath2V = m_StraightLineTargetPos;

		// Compute the segment delta, needed by FindTValueSegToPoint().
		const Vec3V posDeltaV = Subtract(posOnPath2V, posOnPath1V);

		// Use FindTValueSegToPoint() to find the T value of the closest point.
		ScalarV tV = geomTValues::FindTValueSegToPoint(posOnPath1V, posDeltaV, extrapolatedPosV);

		// Use a T value of 1 if the segment is tiny.
		const BoolV tValidV = IsGreaterThan(MagSquared(posDeltaV), ScalarV(V_FLT_SMALL_4));
		tV = SelectFT(tValidV, ScalarV(V_ONE), tV);

		// Compute the closest point on this segment, and use that as the next extrapolated
		// position.
		closestPosOnSegmentV = Lerp(tV, posOnPath1V, posOnPath2V);
		closestPosOnSegmentV.SetZ(extrapolatedPosV.GetZ());	// Maybe safest to not mess with the Z for now.
		closestPosDirOnSegmentV = posDeltaV;
		gotClosestPosOnSegment = true;

	} // ... or a route.
	else if(m_PathWaypointDataInitialized)
	{
		const Vec3V testPosV = extrapolatedPosV;

		// Get the current target point, if we can.
		int tgt = GetCurrentTargetPoint();
		if(tgt < 0)
		{
			tgt = 0;
		}
		const int numPts = GetNumPoints();

		// This should probably hold up - m_FirstValidElementOfPathWaypointsAndSpeed tends to actually
		// not really be valid, so it's not good if we are reading from it.
		//	Assert(tgt > m_FirstValidElementOfPathWaypointsAndSpeed);

		// Prepare for the first segment.
		Vec3V posOnPath1V;
		Vec3V posOnPath2V = m_vPathWaypointsAndSpeed[tgt].GetXYZ();

		// Loop over the segments.
		for(int i = tgt; i < numPts; i++)
		{
			posOnPath1V = posOnPath2V;

			// Note: the i + 1 stuff here seems to be how this array is set up,
			// it actually contains GetNumPoints() + 1 elements. I think...
			posOnPath2V = m_vPathWaypointsAndSpeed[i + 1].GetXYZ();

			// Compute the segment delta, needed by FindTValueSegToPoint().
			Vec3V posDeltaV = Subtract(posOnPath2V, posOnPath1V);

			// Use FindTValueSegToPoint() to find the T value of the closest point.
			ScalarV tV = geomTValues::FindTValueSegToPoint(posOnPath1V, posDeltaV, testPosV);

			// Check to make sure this is not a segment that's behind us.
			// Note: this is a bit arbitrary, should perhaps always look a few steps ahead
			// on the path and use the closest position we found.
			// PH: Sometimes we seem to be getting duplicate points in the array meaning the vehicle 
			// wont be able to progress and will pop back and forwards.The DeltaThreshold below is 
			// an attempt to stop this affecting the interpolation but this is really a problem 
			// with the route generation that is bugged seperately.
			const ScalarV thresholdV(0.99f);
			const ScalarV DeltaThresholdV(0.1f);

#if DEBUG_DRAW_INTERPOLATION_TARGET
			if( CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(&veh) )
			{
				DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(posOnPath1V, 0.2f, Color_pink, false, -3));
				DEBUG_DRAW_ONLY(grcDebugDraw::Line(posOnPath1V,posOnPath2V, Color_orange, -3));
				DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(posOnPath2V, 0.2f, Color_orange, false, -3));
			}
#endif

			bool bIsLast = (i==numPts-1);
			bool bSegmentAheadAndNotShort = IsLessThanAll(tV, thresholdV) && IsGreaterThanAll(Mag(posDeltaV), DeltaThresholdV);

			if(bIsLast || bSegmentAheadAndNotShort)
			{
#if 0
				Vec3V dirYV;
				TUNE_BOOL(BlendDir, false);
				if(BlendDir)
				{
					const Vec3V closestPosV = AddScaled(posOnPath1V, posDeltaV, tV);
					//grcDebugDraw::Line(vVehiclePosRoadHeight, closestPosV, Color_yellow, 4);
					//grcDebugDraw::Line(posOnPath1V, posOnPath2V, Color_pink, 4);

					const ScalarV scaleFactorV(0.5f/4.0f);

					const ScalarV segLenV = Mag(posDeltaV);

					const ScalarV oneV(V_ONE);
					const ScalarV halfV(V_HALF);

					const ScalarV pV = tV;
					const ScalarV qV = Subtract(oneV, tV);

					const ScalarV segLenTimesScaleFactor = Scale(segLenV, scaleFactorV);

					const ScalarV blendPrevV = Subtract(halfV, Min(Max(pV, Scale(pV, segLenTimesScaleFactor)), halfV));
					const ScalarV blendNextV = Subtract(halfV, Min(Max(qV, Scale(qV, segLenTimesScaleFactor)), halfV));

					//	const float p = tV.Getf();
					//	const float q = 1.0f - p;
					//	const float blendPrev = 0.5f - Min(Max(p, p*0.5f*segLen/s_MaxBlendLen), 0.5f);
					//	const float blendNext = 0.5f - Min(Max(q, q*0.5f*segLen/s_MaxBlendLen), 0.5f);

					Vec3V basePosV = AddScaled(posOnPath1V, posDeltaV, tV) + Vec3V(0.0f, 0.0f, 2.0f);

					const ScalarV thresholdV(V_FLT_SMALL_6);

					const Vec3V thisDirV = NormalizeSafe(posDeltaV, oldForwardDirV);
					dirYV = thisDirV;
					if(IsGreaterThanAll(blendPrevV, thresholdV))
					{
						if(i > m_FirstValidElementOfPathWaypointsAndSpeed)
						{
							const Vec3V prevSegAV = m_vPathWaypointsAndSpeed[i - 1].GetXYZ();
							const Vec3V prevSegBV = m_vPathWaypointsAndSpeed[i].GetXYZ();

							const Vec3V prevDeltaV = Subtract(prevSegBV, prevSegAV);
							const Vec3V prevDirV = NormalizeFastSafe(prevDeltaV, thisDirV);

							dirYV = Lerp(blendPrevV, thisDirV, prevDirV);
							dirYV = NormalizeSafe(dirYV, thisDirV);

							//grcDebugDraw::Line(basePosV, basePosV + prevDirV*ScalarV(4.0f), Color_red, 4);
							//grcDebugDraw::Line(basePosV, basePosV + thisDirV*ScalarV(4.0f), Color_black, 4);
							//grcDebugDraw::Line(basePosV, basePosV + dirYV*ScalarV(4.0f), Color_white, 4);
							//char buff[256];
							//formatf(buff, "%.2f", blendPrevV.Getf());
							//grcDebugDraw::Text(basePosV, Color_red, buff, false, 4);
						}
					}
					else if(IsGreaterThanAll(blendNextV, thresholdV))
					{
						if(i < numPts - 1)
						{
							const Vec3V nextSegAV = m_vPathWaypointsAndSpeed[i + 1].GetXYZ();
							const Vec3V nextSegBV = m_vPathWaypointsAndSpeed[i + 2].GetXYZ();

							const Vec3V nextDeltaV = Subtract(nextSegBV, nextSegAV);
							const Vec3V nextDirV = NormalizeFastSafe(nextDeltaV, thisDirV);

							dirYV = Lerp(blendNextV, thisDirV, nextDirV);
							dirYV = NormalizeSafe(dirYV, thisDirV);

							//grcDebugDraw::Line(basePosV, basePosV + nextDirV*ScalarV(4.0f), Color_green, 4);
							//grcDebugDraw::Line(basePosV, basePosV + thisDirV*ScalarV(4.0f), Color_black, 4);
							//grcDebugDraw::Line(basePosV, basePosV + dirYV*ScalarV(4.0f), Color_white, 4);
							//char buff[256];
							//formatf(buff, "%.2f", blendNextV.Getf());
							//grcDebugDraw::Text(basePosV, Color_green, buff, false, 4);
						}
					}
				}
#endif

				// Might want to do this - that way, we don't interpolate towards the end point
				// of the closest segment, but towards the closest point on that segment from
				// the extrapolated position.
				//	posOnPath2V = Lerp(tV, posOnPath1V, posOnPath2V);

				// Override the position P1 with the cars position in case its actually ahead of the vehicle
				// Otherwise the car may be warped forwards
				posOnPath1V = vVehiclePosRoadHeight;
				// Compute the segment delta, needed by FindTValueSegToPoint().
				posDeltaV = Subtract(posOnPath2V, posOnPath1V);
				// Use FindTValueSegToPoint() to find the T value of the closest point.
				tV = geomTValues::FindTValueSegToPoint(posOnPath1V, posDeltaV, testPosV);

				// Compute the closest point on this segment, and use that as the next extrapolated
				// position.
				closestPosOnSegmentV = Lerp(tV, posOnPath1V, posOnPath2V);
				closestPosOnSegmentV.SetZ(extrapolatedPosV.GetZ());	// Maybe safest to not mess with the Z for now.
				closestPosDirOnSegmentV = posDeltaV;
				gotClosestPosOnSegment = true;

				if(bIsLast && !bSegmentAheadAndNotShort)
				{
					aiDebugf1("Forcing the use of this position because it is the last on the path (delta %.3f, %.3f, %.3f).", posDeltaV.GetXf(), posDeltaV.GetYf(), posDeltaV.GetZf());
#if DEBUG_DRAW_INTERPOLATION_TARGET
					bool bCullingPush = grcDebugDraw::GetDisableCulling();
					grcDebugDraw::SetDisableCulling(true);
					grcDebugDraw::Sphere(closestPosOnSegmentV,1.0f,Color_cyan,false,-50);
					grcDebugDraw::Sphere(vVehiclePosCurrent,1.0f,Color_cyan,false,-50);
					grcDebugDraw::SetDisableCulling(bCullingPush);
#endif
				}

				// Done looking at the path.
				break;
			}

			// This is pretty arbitrary, but probably good to have some sort of limit
			// to how far we look ahead.
			if(i > tgt + 6)
			{
				aiDebugf1("Stopped searching for closest segment (tgt = %d, numPts = %d)",tgt,numPts);
				break;
			}
		}
	}

	// If we now have a valid position and direction either along the route or towards the
	// straight line position, compute the interpolation target from the virtual road.
	if(gotClosestPosOnSegment)
	{
		// Create a matrix that points down the path and use that to test the virtual road
		const Vec3V oldForwardDirV = mInterpolationTarget.GetCol1();
		const Vec3V dirYV = NormalizeSafe(closestPosDirOnSegmentV, oldForwardDirV);
		const Vec3V dirXNonNormV = CrossZAxis(dirYV);
		const ScalarV dirXMagV = Mag(dirXNonNormV);
		if(IsGreaterThanAll(dirXMagV, ScalarV(0.001f)))
		{
			const Vec3V dirXV = InvScale(dirXNonNormV, dirXMagV);
			const Vec3V dirZV = Cross(dirXV, dirYV);
			mInterpolationTarget.SetCol0(dirXV);
			mInterpolationTarget.SetCol1(dirYV);
			mInterpolationTarget.SetCol2(dirZV);

			aiAssertf(mInterpolationTarget.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)), "Recomputed vehicle interpolation matrix isn't orthonormal (vRight:%.3f,%.3f,%.3f - vForward:%.3f,%.3f,%.3f - vUp:%.3f,%.3f,%.3f - posDelta:%.3f,%.3f,%.3f)", 
				mInterpolationTarget.GetCol0ConstRef().GetXf(), mInterpolationTarget.GetCol0ConstRef().GetYf(), mInterpolationTarget.GetCol0ConstRef().GetZf(), 
				mInterpolationTarget.GetCol1ConstRef().GetXf(), mInterpolationTarget.GetCol1ConstRef().GetYf(), mInterpolationTarget.GetCol1ConstRef().GetZf(), 
				mInterpolationTarget.GetCol2ConstRef().GetXf(), mInterpolationTarget.GetCol2ConstRef().GetYf(), mInterpolationTarget.GetCol2ConstRef().GetZf(),
				closestPosDirOnSegmentV.GetXf(), closestPosDirOnSegmentV.GetYf(), closestPosDirOnSegmentV.GetZf());
		}
		else
		{
			aiAssertf(mInterpolationTarget.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)), "Existing vehicle interpolation matrix isn't orthonormal (vRight:%.3f,%.3f,%.3f - vForward:%.3f,%.3f,%.3f - vUp:%.3f,%.3f,%.3f - posDelta:%.3f,%.3f,%.3f)", 
				mInterpolationTarget.GetCol0ConstRef().GetXf(), mInterpolationTarget.GetCol0ConstRef().GetYf(), mInterpolationTarget.GetCol0ConstRef().GetZf(), 
				mInterpolationTarget.GetCol1ConstRef().GetXf(), mInterpolationTarget.GetCol1ConstRef().GetYf(), mInterpolationTarget.GetCol1ConstRef().GetZf(), 
				mInterpolationTarget.GetCol2ConstRef().GetXf(), mInterpolationTarget.GetCol2ConstRef().GetYf(), mInterpolationTarget.GetCol2ConstRef().GetZf(),
				closestPosDirOnSegmentV.GetXf(), closestPosDirOnSegmentV.GetYf(), closestPosDirOnSegmentV.GetZf());
		}
		mInterpolationTarget.SetCol3(closestPosOnSegmentV);

		// 
		Vec3V averageRoadToWheelOffsetV;
		bool bRoadFound = CalculatePositionAndOrientationOnVirtualRoad
			(&veh,mInterpolationTarget,NULL,false,false,extrapolatedForwardV,extrapolatedRightV,averageRoadToWheelOffsetV,NULL);
		if(Verifyf(bRoadFound,"CalculatePositionAndOrientationOnVirtualRoad failed.  We shouldn't get this far if it will fail."))
		{
			extrapolatedPosV = closestPosOnSegmentV - averageRoadToWheelOffsetV;
			bExtrapolatedPosOnPathFound = true;
#if DEBUG_DRAW_INTERPOLATION_TARGET
			if( CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(&veh) )
			{
				DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(closestPosOnSegmentV, 0.2f, Color_purple, false, -3));
				DEBUG_DRAW_ONLY(grcDebugDraw::Line(closestPosOnSegmentV,extrapolatedPosV, Color_purple, -3));
			}
#endif
		}
	}

	if(!bExtrapolatedPosOnPathFound)
	{
		aiDebugf1("UpdateTimeslicedInterpolationTarget - No extrapolated position found for vehicle at (%0.2f,%0.2f,%0.2f), tgt = %d, numPts = %d",
			vVehiclePosCurrent.GetXf(),vVehiclePosCurrent.GetYf(),vVehiclePosCurrent.GetZf(),GetCurrentTargetPoint(),GetNumPoints());
	}

	// Compute the new vehicle matrix, accounting for this one frame's worth of movement.
	// TODO: Perhaps it would be better to just let the interpolation mechanism run also
	// during full update frames.
	const ScalarV singleFrameTimestepV(fwTimer::GetTimeStep());
	const Vec3V newVehiclePosV = AddScaled(vVehiclePosCurrent, newVelocityV, singleFrameTimestepV);
	mNewVehicleMatrix.SetCol3(newVehiclePosV);

	//Assert(IsLessThanAll(Dist(vVehiclePosCurrent, mNewVehicleMatrix.d()), ScalarV(V_TEN)));

	// Set the new matrix.
	static dev_bool bUpdateWorld = true;
	static dev_bool bUpdatePhysics = true;
	static dev_bool bWarp = false;
	veh.SetMatrix(RCC_MATRIX34(mNewVehicleMatrix), bUpdateWorld, bUpdatePhysics, bWarp);

	// Check if the new target is sufficiently different from the current position.
	// If not, we are more or less stationary and just clear the interpolation target
	// to stop interpolating, to help performance a bit.
	const ScalarV thresholdSqV(V_FLT_SMALL_3);	// 0.001 ~= 0.03^2
	const ScalarV distSqV = DistSquared(extrapolatedPosV, mNewVehicleMatrix.GetCol3());
	if(IsGreaterThanOrEqualAll(distSqV, thresholdSqV))
	{
		// Create the interpolation target matrix
		mInterpolationTarget.SetCol0(extrapolatedRightV);
		mInterpolationTarget.SetCol1(extrapolatedForwardV);
		mInterpolationTarget.SetCol2(Cross(extrapolatedRightV,extrapolatedForwardV));
		// mInterpolationTarget already has extrapolatedPosV as col3
		aiAssertf(mInterpolationTarget.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)),
				"Right/forward vectors not orthonormal? (vRight:%.3f,%.3f,%.3f - vForward:%.3f,%.3f,%.3f - vUp:%.3f,%.3f,%.3f)", 
				mInterpolationTarget.GetCol0ConstRef().GetXf(), mInterpolationTarget.GetCol0ConstRef().GetYf(), mInterpolationTarget.GetCol0ConstRef().GetZf(), 
				mInterpolationTarget.GetCol1ConstRef().GetXf(), mInterpolationTarget.GetCol1ConstRef().GetYf(), mInterpolationTarget.GetCol1ConstRef().GetZf(), 
				mInterpolationTarget.GetCol2ConstRef().GetXf(), mInterpolationTarget.GetCol2ConstRef().GetYf(), mInterpolationTarget.GetCol2ConstRef().GetZf());

		// Recompute the interpolation time step so that when we interpolate, we do so
		// using the same speed as we originally wanted to move at. This probably helps
		// a bit to make the speed more consistent when going through sharp curves.
		// Note: we need to use the new position here, not the position we had before the
		// teleport we just did.
		const ScalarV remainingDistV = Dist(newVehiclePosV, extrapolatedPosV);
		const ScalarV desiredSpeedV = Mag(newVelocityV);
		futureTimeStepV = InvScale(remainingDistV, Max(desiredSpeedV, ScalarV(V_FLT_SMALL_2)));

		static const ScalarV svMinTimestep(0.05f);
		futureTimeStepV = rage::Max(svMinTimestep, futureTimeStepV);

		const ScalarV vNewEstSpeed = remainingDistV / futureTimeStepV;
		const ScalarV scVelThresholdSqr = ScalarV(V_FLT_SMALL_2) * ScalarV(V_FLT_SMALL_2);
		if (IsGreaterThanAll(vNewEstSpeed * vNewEstSpeed, scVelThresholdSqr))
		{
			// Compute the quaternion for the target orientation, and update the target in CVehicle.
			QuatV tgtOrientation = QuatVFromMat34V(mInterpolationTarget);
			veh.SetInterpolationTargetAndTime(extrapolatedPosV, tgtOrientation, futureTimeStepV);

			aiAssertf(g_WorldLimits_AABB.ContainsPoint(extrapolatedPosV), "Setting timesliced interpolation target outside world limits: %.1f, %.1f, %.1f",
				extrapolatedPosV.GetXf(), extrapolatedPosV.GetYf(), extrapolatedPosV.GetZf());
		}
		else
		{
			veh.ClearInterpolationTargetAndTime();
		}
	}
	else
	{
		veh.ClearInterpolationTargetAndTime();
	}

#if DEBUG_DRAW_INTERPOLATION_TARGET
 	if( CVehicleIntelligence::AllowCarAiDebugDisplayForThisVehicle(&veh) )
	{
		DEBUG_DRAW_ONLY(grcDebugDraw::Line(extrapolatedPosV-extrapolatedRightV, extrapolatedPosV+extrapolatedRightV, Color_green, -3));
		DEBUG_DRAW_ONLY(grcDebugDraw::Line(extrapolatedPosV-extrapolatedForwardV, extrapolatedPosV+extrapolatedForwardV, Color_green, -3));
		DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(extrapolatedPosV, 0.21f, Color_green, false, -1));
		DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(mNewVehicleMatrix.GetCol3(), 0.2f, Color_white, false, -1));
 	}
#endif

	// Update the velocity.
	veh.SetSuperDummyVelocityFromInterpolationTarget();

	// TODO: To avoid unnatural-looking speeds, it would probably also be a good idea
	// to recompute the time step we pass in to SetInterpolationTargetAndTime(), based on
	// the distance to the interpolation target and our desired velocity. The way it is now,
	// if we snap the interpolation target to the path, we could in some cases shorten or lengthen
	// the distance we will travel during the futureTimeStepV. In practice I haven't seen too many
	// instances of this during normal operation.
}

bool CVehicleFollowRouteHelper::ComputeOnRoadPosition(const CVehicle& veh, Vec3V_Ref tgtPositionOut, bool bPreventWheelCompression /*, QuatV_Ref tgtOrientationOut*/) const
{
	// Prepare some input parameters, computing the position at the road level.
	const Mat34V& matCurrent = veh.GetTransform().GetMatrix();
	const Vec3V vehiclePosCurrentV = matCurrent.GetCol3();
	const ScalarV distanceAboveRoadV(veh.GetHeightAboveRoad());
	const Vec3V vehiclePosRoadHeightV = SubtractScaled(vehiclePosCurrentV, matCurrent.GetCol2(), distanceAboveRoadV);

	// Check for inputs that could lead to teleports to bad positions.
	aiAssertf(g_WorldLimits_AABB.ContainsPoint(vehiclePosCurrentV), "Vehicle's current position is outside world limits: %.1f, %.1f, %.1f",
			vehiclePosCurrentV.GetXf(), vehiclePosCurrentV.GetYf(), vehiclePosCurrentV.GetZf());

	// We will need to compute two matrices, the vehicle's new matrix for this frame,
	Mat34V mInterpolationTarget(matCurrent);
	mInterpolationTarget.SetCol3(vehiclePosRoadHeightV);

	// Initialize forward/right vectors. These are expected to be overwritten in a moment.
	Vec3V forwardV(matCurrent.GetCol1());
	Vec3V rightV(matCurrent.GetCol0());

	// Note: CalculatePositionAndOrientationOnVirtualRoad() needs a non-const CVehicle because it can update
	// the wheel compression and dummy constraint. The way it's used here, that shouldn't happen, so from
	// the point of view of this function's caller, it's proper to make it a const CVehicle even if it means
	// we have to cast here.
	Vec3V averageRoadToWheelOffsetV;
	bool bRoadFound = CalculatePositionAndOrientationOnVirtualRoad
		(&const_cast<CVehicle&>(veh), mInterpolationTarget, NULL, false, false, forwardV, rightV, averageRoadToWheelOffsetV, NULL, bPreventWheelCompression);

	if(!bRoadFound)
	{
		aiAssertf(!veh.GetVehicleAiLod().IsLodFlagSet(CVehicleAILod::AL_LodSuperDummy),
						"CalculatePositionAndOrientationOnVirtualRoad failed.  We shouldn't get this far if it will fail.");
		return false;
	}

	// Compute the position on the road.
	const Vec3V posOnRoadV = Subtract(mInterpolationTarget.GetCol3(), averageRoadToWheelOffsetV);

	// Create the interpolation target matrix (note: we haven't updated the position in the matrix here).
	//mInterpolationTarget.SetCol0(rightV);
	//mInterpolationTarget.SetCol1(forwardV);
	//mInterpolationTarget.SetCol2(Cross(rightV,forwardV));
	//aiAssertf(mInterpolationTarget.IsOrthonormal3x3(ScalarV(V_FLT_SMALL_2)),
	//		"Right/forward vectors not orthonormal? (vRight:%.3f,%.3f,%.3f - vForward:%.3f,%.3f,%.3f - vUp:%.3f,%.3f,%.3f)", 
	//		mInterpolationTarget.GetCol0ConstRef().GetXf(), mInterpolationTarget.GetCol0ConstRef().GetYf(), mInterpolationTarget.GetCol0ConstRef().GetZf(), 
	//		mInterpolationTarget.GetCol1ConstRef().GetXf(), mInterpolationTarget.GetCol1ConstRef().GetYf(), mInterpolationTarget.GetCol1ConstRef().GetZf(), 
	//		mInterpolationTarget.GetCol2ConstRef().GetXf(), mInterpolationTarget.GetCol2ConstRef().GetYf(), mInterpolationTarget.GetCol2ConstRef().GetZf());

	tgtPositionOut = posOnRoadV;
	//tgtOrientationOut = QuatVFromMat34V(mInterpolationTarget);

	return true;
}

//-------------------------------------------------------------------------
// Helper used to test if there is a navmesh line-of-sight
//-------------------------------------------------------------------------

CNavmeshLineOfSightHelper::CNavmeshLineOfSightHelper()
{
	m_bWaitingForLosResult = false;
	m_hPathHandle = PATH_HANDLE_NULL;
}
CNavmeshLineOfSightHelper::~CNavmeshLineOfSightHelper()
{
	ResetTest();
}

void CNavmeshLineOfSightHelper::ResetTest( void )
{
	if( m_hPathHandle != PATH_HANDLE_NULL )
	{
		CPathServer::CancelRequest(m_hPathHandle);
		m_hPathHandle = PATH_HANDLE_NULL;
	}
	m_bWaitingForLosResult = false;
}
bool CNavmeshLineOfSightHelper::StartLosTest(const Vector3 & vStart, const Vector3 & vEnd, const float fRadius, const bool bTestDynamicObjects, bool bNoLosAcrossWaterBoundary, bool bStartsInWater, const s32 iNumExcludeEntities, CEntity ** ppExcludeEntities, const float fMaxSlopeAngle)
{
	Vector3 vVerts[2] = { vStart, vEnd };
	return StartLosTest(vVerts, 2, fRadius, bTestDynamicObjects, true, bNoLosAcrossWaterBoundary, bStartsInWater, iNumExcludeEntities, ppExcludeEntities, fMaxSlopeAngle);
}
 
bool CNavmeshLineOfSightHelper::StartLosTest(Vector3 * pVerts, const int iNumVerts, const float fRadius, const bool bTestDynamicObjects, const bool bQuitAtFirstLOSFail, bool bNoLosAcrossWaterBoundary, bool bStartsInWater, const s32 iNumExcludeEntities, CEntity ** ppExcludeEntities, const float fMaxSlopeAngle)
{
	ResetTest();

	Assertf(m_hPathHandle==PATH_HANDLE_NULL, "CNavmeshLineOfSightHelper::StartLosTest() was called whilst an existing LOS test was already active.  Doing this could fill up the request queue, or indicate that something is spamming the pathfinder.");

	m_hPathHandle = CPathServer::RequestLineOfSight(pVerts, iNumVerts, fRadius, bTestDynamicObjects, bQuitAtFirstLOSFail, bNoLosAcrossWaterBoundary, bStartsInWater, iNumExcludeEntities, ppExcludeEntities, fMaxSlopeAngle, NULL);

	if( m_hPathHandle != PATH_HANDLE_NULL )
	{
		m_bWaitingForLosResult = true;
	}
	else
	{
		m_bWaitingForLosResult = false;
	}
	return m_bWaitingForLosResult;
}

bool CNavmeshLineOfSightHelper::GetTestResults( SearchStatus & searchResult, bool & bLosIsClear, bool& bNoSurfaceAtCoords, const int iNumLosResults, bool ** ppLosResults )
{
	Assert(m_bWaitingForLosResult);
	Assert(m_hPathHandle != PATH_HANDLE_NULL);

	bNoSurfaceAtCoords = false;

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hPathHandle);
	if(ret == ERequest_Ready)
	{
		m_bWaitingForLosResult = false;
		searchResult = SS_SearchSuccessful;

		EPathServerErrorCode errCode;
		if(ppLosResults)
		{
			errCode = CPathServer::GetLineOfSightResult(m_hPathHandle, bLosIsClear, *ppLosResults, iNumLosResults);
		}
		else
		{
			errCode = CPathServer::GetLineOfSightResult(m_hPathHandle, bLosIsClear);
		}

		m_hPathHandle = PATH_HANDLE_NULL;

		if(errCode == PATH_FOUND)
		{
			return true;
		}
		else
		{
			if (errCode == PATH_NO_SURFACE_AT_COORDINATES)
			{
				bNoSurfaceAtCoords = true;
			}

			bLosIsClear = false;
			searchResult = SS_SearchFailed;
			return true;
		}
	}
	else if(ret == ERequest_NotFound)
	{
		m_hPathHandle = PATH_HANDLE_NULL;
		m_bWaitingForLosResult = false;
		bLosIsClear = false;
		searchResult = SS_SearchFailed;
		return true;
	}

	searchResult = SS_StillSearching;
	return false;
}



CNavmeshClearAreaHelper::CNavmeshClearAreaHelper()
{
	m_bSearchActive = false;
	m_hSearchHandle = PATH_HANDLE_NULL;
}

CNavmeshClearAreaHelper::~CNavmeshClearAreaHelper()
{
	ResetSearch();
}

bool CNavmeshClearAreaHelper::StartClearAreaSearch(const Vector3 & vSearchOrigin, const float fSearchRadiusXY, const float fSearchDistZ, const float fClearAreaRadius, const u32 iFlags, const float fMinimumRadius)
{
	ResetSearch();

	Assertf(m_hSearchHandle==PATH_HANDLE_NULL, "CNavmeshClearAreaHelper::StartClearAreaSearch() was called whilst an existing search was already active.  Doing this could fill up the request queue, or indicate that something is spamming the pathfinder.");

	m_hSearchHandle = CPathServer::RequestClearArea(vSearchOrigin, fSearchRadiusXY, fSearchDistZ, fClearAreaRadius, fMinimumRadius, (iFlags&Flag_TestForObjects)!=0, (iFlags&Flag_AllowInteriors)!=0, (iFlags&Flag_AllowExterior)!=0, (iFlags&Flag_AllowWater)!=0, (iFlags&Flag_AllowSheltered)!=0, NULL, kNavDomainRegular);

	if( m_hSearchHandle != PATH_HANDLE_NULL )
	{
		m_bSearchActive = true;
	}
	return m_bSearchActive;
}

bool CNavmeshClearAreaHelper::GetSearchResults( SearchStatus & searchResult, Vector3 & vOut_ResultPosition )
{
	Assert(m_bSearchActive);

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hSearchHandle);
	if(ret == ERequest_Ready)
	{
		m_bSearchActive = false;
		searchResult = SS_SearchSuccessful;

		EPathServerErrorCode errCode = CPathServer::GetClearAreaResultAndClear(m_hSearchHandle, vOut_ResultPosition);
		m_hSearchHandle = PATH_HANDLE_NULL;

		if(errCode == PATH_FOUND)
		{
			return true;
		}
		else
		{
			searchResult = SS_SearchFailed;
			return true;
		}
	}
	else if(ret == ERequest_NotFound)
	{
		m_hSearchHandle = PATH_HANDLE_NULL;
		searchResult = SS_SearchFailed;
		return true;
	}

	searchResult = SS_StillSearching;
	return false;
}

void CNavmeshClearAreaHelper::ResetSearch()
{
	if( m_bSearchActive )
	{
		if( m_hSearchHandle != PATH_HANDLE_NULL )
		{
			CPathServer::CancelRequest(m_hSearchHandle);
			m_hSearchHandle = PATH_HANDLE_NULL;
		}
		m_bSearchActive = false;
	}
}

//-----------------------------------------------------------------------------------

CNavmeshClosestPositionHelper::CNavmeshClosestPositionHelper()
{
	m_bSearchActive = false;
	m_hSearchHandle = PATH_HANDLE_NULL;
}

CNavmeshClosestPositionHelper::~CNavmeshClosestPositionHelper()
{
	ResetSearch();
}

bool CNavmeshClosestPositionHelper::StartClosestPositionSearch( const Vector3 & vSearchOrigin, const float fSearchRadius, const u32 iFlags, const float fZWeightingAbove, const float fZWeightingAtOrBelow, const float fMinimumSpacing, const s32 iNumAvoidSpheres, const spdSphere * pAvoidSpheres, const s32 iMaxNumResults )
{
	ResetSearch();

	Assertf(m_hSearchHandle==PATH_HANDLE_NULL, "CNavmeshClosestPositionHelper::StartClosestPositionSearch() was called whilst an existing search was already active.  Doing this could fill up the request queue, or indicate that something is spamming the pathfinder.");
	Assertf( ((iFlags & Flag_ConsiderInterior)!=0) || ((iFlags & Flag_ConsiderExterior)!=0), "CNavmeshClosestPositionHelper::StartClosestPositionSearch() - you need to specify Flag_ConsiderInterior or Flag_ConsiderExterior, currently neither is specified");

	// Translate flags (there happens to be a 1:1 mapping between the two sets of flags currently, but that may not always be the case)
	u32 iReqFlags = 0;
	if((iFlags & Flag_ConsiderDynamicObjects)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderDynamicObjects;
	if((iFlags & Flag_ConsiderInterior)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderInterior;
	if((iFlags & Flag_ConsiderExterior)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderExterior;
	if((iFlags & Flag_ConsiderOnlyLand)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderOnlyLand;
	if((iFlags & Flag_ConsiderOnlyPavement)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderOnlyPavement;
	if((iFlags & Flag_ConsiderOnlyNonIsolated)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderOnlyNonIsolated;
	if((iFlags & Flag_ConsiderOnlySheltered)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderOnlySheltered;
	if((iFlags & Flag_ConsiderOnlyNetworkSpawn)!=0) iReqFlags |= CClosestPositionRequest::Flag_ConsiderOnlyNetworkSpawn;

	m_hSearchHandle = CPathServer::RequestClosestPosition(vSearchOrigin, fSearchRadius, iReqFlags, fZWeightingAbove, fZWeightingAtOrBelow, fMinimumSpacing, iNumAvoidSpheres, pAvoidSpheres, iMaxNumResults, NULL, kNavDomainRegular);

	if( m_hSearchHandle != PATH_HANDLE_NULL )
	{
		m_bSearchActive = true;
	}
	return m_bSearchActive;
}

bool CNavmeshClosestPositionHelper::GetSearchResults( SearchStatus & searchResult, s32 & iOut_NumPositions, Vector3 * pOut_Positions, const s32 iMaxNumPositions )
{
	Assert(m_bSearchActive);

	EPathServerRequestResult ret = CPathServer::IsRequestResultReady(m_hSearchHandle);
	if(ret == ERequest_Ready)
	{
		m_bSearchActive = false;
		searchResult = SS_SearchSuccessful;

		CClosestPositionRequest::TResults results;
		EPathServerErrorCode errCode = CPathServer::GetClosestPositionResultAndClear(m_hSearchHandle, results);
		m_hSearchHandle = PATH_HANDLE_NULL;

		if(errCode == PATH_FOUND)
		{
			iOut_NumPositions = Min(results.m_iNumResults, iMaxNumPositions);
			sysMemCpy(pOut_Positions, results.m_vResults.GetElements(), sizeof(Vector3)*iOut_NumPositions);
			return true;
		}
		else
		{
			searchResult = SS_SearchFailed;
			return true;
		}
	}
	else if(ret == ERequest_NotFound)
	{
		m_bSearchActive = false;
		m_hSearchHandle = PATH_HANDLE_NULL;
		searchResult = SS_SearchFailed;
		return true;
	}

	searchResult = SS_StillSearching;
	return false;
}

void CNavmeshClosestPositionHelper::ResetSearch()
{
	if( m_bSearchActive )
	{
		if( m_hSearchHandle != PATH_HANDLE_NULL )
		{
			CPathServer::CancelRequest(m_hSearchHandle);
			m_hSearchHandle = PATH_HANDLE_NULL;
		}
		m_bSearchActive = false;
	}
}

//-----------------------------------------------------------------------------------

CAsyncProbeHelper::CAsyncProbeHelper()
{
	m_bWaitingForResult = false;
}

CAsyncProbeHelper::~CAsyncProbeHelper()
{
	ResetProbe();
}

bool CAsyncProbeHelper::StartTestLineOfSight( WorldProbe::CShapeTestProbeDesc& probeData)
{
	Assertf(m_bWaitingForResult == false, "CAsyncProbeHelper::StartTestLineOfSight() was called whilst an existing LOS test was active.  This could fill up the request queue, or indicate that something is spamming the AsyncProbe system.");
	Assert(!m_hHandle.GetResultsWaitingOrReady());
	ResetProbe();

	probeData.SetResultsStructure(&m_hHandle);
	WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

	if( m_hHandle.GetResultsWaitingOrReady() )
	{
		m_bWaitingForResult = true;
	}
	return m_bWaitingForResult;
}

bool CAsyncProbeHelper::GetProbeResults( ProbeStatus& probeStatus )
{
	Assert(IsProbeActive());
	Assert(m_hHandle.GetResultsWaitingOrReady());

	if( m_hHandle.GetWaitingOnResults() )
	{
		probeStatus = PS_WaitingForResult;
		return false;	
	}
	
	if( m_hHandle.GetNumHits() > 0u )
	{
		probeStatus = PS_Blocked;
	}
	else
	{
		probeStatus = PS_Clear;
	}

	m_bWaitingForResult = false;
	m_hHandle.Reset();
	return true;
}

bool CAsyncProbeHelper::GetProbeResultsWithIntersection( ProbeStatus& probeStatus, Vector3* pIntersectionPos)
{
	Assert(IsProbeActive());
	Assert(m_hHandle.GetResultsWaitingOrReady());

	if( m_hHandle.GetWaitingOnResults() )
	{
		probeStatus = PS_WaitingForResult;
		return false;	
	}
	
	if( m_hHandle.GetNumHits() > 0u )
	{
		probeStatus = PS_Blocked;
		*pIntersectionPos = m_hHandle[0].GetHitPosition();
	}
	else
	{
		probeStatus = PS_Clear;
	}

	m_bWaitingForResult = false;
	m_hHandle.Reset();
	return true;
}

bool CAsyncProbeHelper::GetProbeResultsWithIntersectionAndNormal( ProbeStatus& probeStatus, Vector3* pIntersectionPos, Vector3* pIntersectionNormal)
{
	Assert(IsProbeActive());
	Assert(m_hHandle.GetResultsWaitingOrReady());

	if( m_hHandle.GetWaitingOnResults() )
	{
		probeStatus = PS_WaitingForResult;
		return false;	
	}
	
	if( m_hHandle.GetNumHits() > 0u )
	{
		probeStatus = PS_Blocked;
		*pIntersectionPos = m_hHandle[0].GetHitPosition();
		*pIntersectionNormal = m_hHandle[0].GetHitNormal();
	}
	else
	{
		probeStatus = PS_Clear;
	}

	m_bWaitingForResult = false;
	m_hHandle.Reset();
	return true;
}

bool CAsyncProbeHelper::GetProbeResultsVfx( ProbeStatus& probeStatus, Vector3* pIntersectionPos, Vector3* pIntersectionNormal, u16* pIntersectionPhysInstLevelIndex, u16* pIntersectionPhysInstGenerationId, u16* pIntersectionComponent, phMaterialMgr::Id* pIntersectionMtlId, float* pIntersectionTValue)
{
	Assert(IsProbeActive());
	Assert(m_hHandle.GetResultsWaitingOrReady());

	if( m_hHandle.GetWaitingOnResults() )
	{
		probeStatus = PS_WaitingForResult;
		return false;	
	}

	if( m_hHandle.GetNumHits() > 0u )
	{
		probeStatus = PS_Blocked;
		*pIntersectionPos = m_hHandle[0].GetHitPosition();
		*pIntersectionNormal = m_hHandle[0].GetHitNormal();
		*pIntersectionPhysInstLevelIndex = m_hHandle[0].GetLevelIndex();
		*pIntersectionPhysInstGenerationId = m_hHandle[0].GetGenerationID();
		*pIntersectionComponent = m_hHandle[0].GetHitComponent();
		*pIntersectionMtlId = m_hHandle[0].GetHitMaterialId();
		*pIntersectionTValue = m_hHandle[0].GetHitTValue();
	}
	else
	{
		probeStatus = PS_Clear;
	}

	m_bWaitingForResult = false;
	m_hHandle.Reset();
	return true;
}

void CAsyncProbeHelper::ResetProbe( void )
{
	if( m_bWaitingForResult )
	{
		m_hHandle.Reset();
		m_bWaitingForResult = false;
	}
}

// CClipHelper
// A helper class to simplify the task interface with the clip systems
// encapsulates all clipId playback functionality including the use of callbacks
// so this is no longer exposed to the tasks.

////////////////////////////////////////////////////////////////////////////////

float CClipHelper::ComputeHeadingDeltaFromLeft(float fFrom, float fTo)
{
	Assert(Abs(fFrom) <= PI && Abs(fTo) <= PI);
	float fReturnValue = fFrom;

	if (fFrom >= fTo)
	{
		fReturnValue = fTo-fFrom;
	}
	else
	{
		const float fLeftDiff = Abs(-PI - fFrom);
		const float fRightDiff = Abs(PI - fTo);
		fReturnValue = -(fLeftDiff + fRightDiff);
	}

	return fReturnValue;
}

////////////////////////////////////////////////////////////////////////////////

float CClipHelper::ComputeHeadingDeltaFromRight(float fFrom, float fTo)
{
	Assert(Abs(fFrom) <= PI && Abs(fTo) <= PI);
	float fReturnValue = fFrom;

	if (fFrom <= fTo)
	{
		fReturnValue = fTo-fFrom;
	}
	else
	{
		const float fLeftDiff = Abs(-PI - fTo);
		const float fRightDiff = Abs(PI - fFrom);
		fReturnValue = fLeftDiff + fRightDiff;
	}

	return fReturnValue;
}

////////////////////////////////////////////////////////////////////////////////

bool CClipHelper::ApproachFromRight(float& fCurrentVal, float fDesiredVal, float fRate, float fTimeStep)
{
	if (fCurrentVal < fDesiredVal || Abs(fCurrentVal - fDesiredVal) < SMALL_FLOAT)
	{
		// We don't need to wrap around, so just use Approach as normal
		return rage::Approach(fCurrentVal, fDesiredVal, fRate, fTimeStep);
	}
	else
	{
		// We need to take the long route, so continue approaching right
		if (fCurrentVal < PI)
		{
			(void)taskVerifyf(!rage::Approach(fCurrentVal, TWO_PI, fRate, fTimeStep), "Approach rate is too high");
		}

		// If we go pass PI, we wrap around
		if (fCurrentVal >= PI)
		{
			const float fDiff = Abs(fCurrentVal - PI);
			fCurrentVal = -PI + fDiff;

			// If we went over the desired value, return it as the approached value
			if (fCurrentVal > fDesiredVal)
			{
				fCurrentVal = fDesiredVal;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CClipHelper::ApproachFromLeft(float& fCurrentVal, float fDesiredVal, float fRate, float fTimeStep)
{
	if (fCurrentVal > fDesiredVal || Abs(fCurrentVal - fDesiredVal) < SMALL_FLOAT)
	{
		return rage::Approach(fCurrentVal, fDesiredVal, fRate, fTimeStep);
	}
	else
	{
		if (fCurrentVal > -PI)
		{
			(void)taskVerifyf(!rage::Approach(fCurrentVal, -TWO_PI, fRate, fTimeStep), "Approach rate is too high");
		}

		if (fCurrentVal <= -PI)
		{
			const float fDiff = Abs(fCurrentVal + PI);
			fCurrentVal = PI - fDiff;

			if (fCurrentVal < fDesiredVal)
			{
				fCurrentVal = fDesiredVal;
			}
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CClipHelper::CClipHelper(CTask* pTask)
: m_pAssociatedTask(pTask)
, m_pAssociatedEntity(NULL)
, m_iStatus(CClipHelper::Status_Unassigned)
, m_iPriority(AP_MEDIUM)
, m_pClip(NULL)
// Hopefully we can get rid of these...
, m_flags(0)
, m_clipId(CLIP_ID_INVALID)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipDictIndex(-1)
, m_clipHashKey(0)
, m_pBlendHelper(NULL)
{

}

CClipHelper::CClipHelper()
: m_pAssociatedTask(NULL)
, m_pAssociatedEntity(NULL)
, m_iStatus(CClipHelper::Status_Unassigned)
, m_iPriority(AP_MEDIUM)
, m_pClip(NULL)
, m_filterHash(BONEMASK_ALL)
// Hopefully we can get rid of these...
, m_flags(0)
, m_clipId(CLIP_ID_INVALID)
, m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipDictIndex(-1)
, m_clipHashKey(0)
, m_pBlendHelper(NULL)
{

}

CClipHelper::~CClipHelper()
{
	StopClip();
	
	SetPlaybackClip(NULL);
}

CSimpleBlendHelper& CClipHelper::GetBlendHelper() const
{
	animAssert(m_pBlendHelper);
	return *m_pBlendHelper;
}

void CClipHelper::SetFilter(atHashString filterHash)
{
	if (filterHash==BONEMASK_ALL)
	{
		m_filterHash.SetHash(0);
	}
	else
	{
		m_filterHash = filterHash;
	}
}

void CClipHelper::SetPlaybackClip(const crClip* pClip)
{
	if (pClip)
	{
		pClip->AddRef();
	}
	
	if (m_pClip)
	{
		m_pClip->Release();
		m_pClip = NULL;
	}

	m_pClip = pClip;
}

bool CClipHelper::StartClipBySetAndClip( CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fBlendDelta, eTerminationType terminationType, u32 appendFlags)
{
	Assertf(m_pAssociatedTask||m_pAssociatedEntity, "Helper must be associated with a task!");

	if(m_pAssociatedTask)
	{
		m_pAssociatedTask->ClearFlag(aiTaskFlags::SkipSetAnimationFlags);
	}

	StopClip(-fBlendDelta);
	Assert(pEntity);

	m_clipSetId = clipSetId;
	m_clipId = clipId;

	// Grab some flags from clipId metadata
	fwClipSet* clipSet = fwClipSetManager::GetClipSet(clipSetId);

	if (animVerifyf(clipSet, "Could not find clipId set %s", PrintClipInfo(clipSetId, clipId)))
	{
		animAssertf(pEntity->GetAnimDirector(), "Invalid AnimDirector : Model %s. clipId set %s", pEntity->GetModelName(), PrintClipInfo(clipSetId, clipId));

		m_clipDictIndex = fwAnimManager::FindSlotFromHashKey(clipSet->GetClipDictionaryName().GetHash());

		const crClip *clip = clipSet->GetClip(clipId);
		if (animVerifyf(clip, "Could not find clip %s!", PrintClipInfo(clipSetId, clipId)))
		{
			m_clipHashKey = clipId.GetHash();
			SetPlaybackClip(fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId));

			if (GetClip())
			{
				s32 flags = 0;
				eAnimPriority priority = AP_LOW;
				atHashString boneMask = atHashString::Null();

				const fwClipItem* clipItem = clipSet->GetClipItem(clipId);
				if(clipItem)
				{
					flags = GetClipItemFlags(clipItem);
					priority = GetClipItemPriority(clipItem);
					boneMask = GetClipItemBoneMask(clipItem);
				}

				SetFilter(boneMask);

				m_iPriority = priority;
				m_flags = flags;
				m_flags |= appendFlags;

				SetFlag(APF_ISPLAYING);
				if (terminationType==TerminationType_OnFinish)
				{
					SetFlag(APF_ISFINISHAUTOREMOVE);
				}
				else if (terminationType==TerminationType_OnDelete)
				{
					UnsetFlag(APF_ISFINISHAUTOREMOVE);
				}
				
				m_pBlendHelper = static_cast<CMove&>(pEntity->GetAnimDirector()->GetMove()).FindAppropriateBlendHelperForClipHelper(*this);

				bool success = false;

				if (animVerifyf(m_pBlendHelper, "Could not find a blend helper to play the clip!"))
				{
					success = m_pBlendHelper->BlendInClip(pEntity, this, m_iPriority, fBlendDelta, (u8)terminationType);

					if (success)
					{
						m_pBlendHelper->SetLooped(this, (flags & APF_ISLOOPED) != 0);
						crFrameFilter* frameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(m_filterHash));
						m_pBlendHelper->SetFilter(this, frameFilter);
					}
				}
				
				return success;
			}
		}
	}

	return false;
}

bool CClipHelper::StartClipBySetAndClip(CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, int flags, atHashString filterHash, int priority, float fBlendDelta, eTerminationType terminationType)
{
	Assertf(m_pAssociatedTask||m_pAssociatedEntity, "Helper must be associated with a task!");

	if(m_pAssociatedTask)
	{
		m_pAssociatedTask->ClearFlag(aiTaskFlags::SkipSetAnimationFlags);
	}

	StopClip(-fBlendDelta);
	Assert(pEntity);

	m_clipSetId  = clipSetId;
	m_clipId = clipId;

	m_iPriority = (eAnimPriority)priority;
	m_flags = flags;

	SetFlag(APF_ISPLAYING);
	if (terminationType==TerminationType_OnFinish)
	{
		SetFlag(APF_ISFINISHAUTOREMOVE);
	}
	else if (terminationType==TerminationType_OnDelete)
	{
		UnsetFlag(APF_ISFINISHAUTOREMOVE);
	}
	// Grab some flags from clipId metadata
	fwClipSet* clipSet = fwClipSetManager::GetClipSet(clipSetId);

	if (animVerifyf(clipSet, "Could not find clipId set %s", PrintClipInfo(clipSetId, clipId)))
	{
		animAssertf(pEntity->GetAnimDirector(), "Invalid AnimDirector : Model %s. clipId set %s", pEntity->GetModelName(), PrintClipInfo(clipSetId, clipId));

		m_clipDictIndex = fwAnimManager::FindSlotFromHashKey(clipSet->GetClipDictionaryName().GetHash());
		const crClip *clip = clipSet->GetClip(clipId);

		if (animVerifyf(clip, "Could not find clip %s! for entity %s", PrintClipInfo(clipSetId, clipId), pEntity->GetModelName()))
		{
			m_clipHashKey = clipId.GetHash();
			SetPlaybackClip(fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId));

			if (GetClip())
			{
				SetFilter(filterHash);

				m_pBlendHelper = static_cast<CMove&>(pEntity->GetAnimDirector()->GetMove()).FindAppropriateBlendHelperForClipHelper(*this);

				bool success = false;

				if (animVerifyf(m_pBlendHelper, "Could not find a blend helper to play the clip!"))
				{
					success = m_pBlendHelper->BlendInClip(pEntity, this, m_iPriority, fBlendDelta, (u8)terminationType);
					if (success)
					{
						m_pBlendHelper->SetLooped(this, (flags & APF_ISLOOPED) != 0);
						crFrameFilter* pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(m_filterHash));
						m_pBlendHelper->SetFilter(this, pFrameFilter);
					}
				}
				return success;
			}
		}
	}

	return false;
}

bool CClipHelper::StartClipByClip( CDynamicEntity* pEntity, const crClip* pClip, int flags, atHashString filterHash, int priority, float fBlendDelta, eTerminationType terminationType )
{
	Assertf(m_pAssociatedTask||m_pAssociatedEntity, "Helper must be associated with a task!");

	if(m_pAssociatedTask)
	{
		m_pAssociatedTask->ClearFlag(aiTaskFlags::SkipSetAnimationFlags);
	}

	StopClip(-fBlendDelta);
	Assert(pEntity);

	SetPlaybackClip(pClip);

	if (GetClip())
	{			
		animAssertf(pEntity->GetAnimDirector(), "Invalid AnimDirector : Model %s. clip %s", pEntity->GetDebugName(), GetClip()->GetName());

		m_iPriority = (eAnimPriority)priority;
		m_flags = flags;
		SetFlag(APF_ISPLAYING);
		if (terminationType==TerminationType_OnFinish)
		{
			SetFlag(APF_ISFINISHAUTOREMOVE);
		}
		else if (terminationType==TerminationType_OnDelete)
		{
			UnsetFlag(APF_ISFINISHAUTOREMOVE);
		}

		SetFilter(filterHash);

		m_pBlendHelper = static_cast<CMove&>(pEntity->GetAnimDirector()->GetMove()).FindAppropriateBlendHelperForClipHelper(*this);

		bool success = false;

		if (animVerifyf(m_pBlendHelper, "Could not find a blend helper to play the clip!"))
		{
			success = m_pBlendHelper->BlendInClip(pEntity, this, m_iPriority, fBlendDelta, (u8)terminationType);

			if (success)
			{
				m_pBlendHelper->SetLooped(this, (flags & APF_ISLOOPED) != 0);
				crFrameFilter* pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(m_filterHash));
				m_pBlendHelper->SetFilter(this, pFrameFilter);
			}
		}
		
		return success;
	}

	return false;

}

bool CClipHelper::StartClipByDictAndHash( CDynamicEntity* pEntity, s32 iDictionaryIndex, u32 clipHash, int flags, atHashString filterHash, int priority, float fBlendDelta, eTerminationType terminationType )
{
	Assertf(m_pAssociatedTask||m_pAssociatedEntity, "Helper must be associated with a task!");

	if(m_pAssociatedTask)
	{
		m_pAssociatedTask->ClearFlag(aiTaskFlags::SkipSetAnimationFlags);
	}

	StopClip(-fBlendDelta);
	Assert(pEntity);

	m_clipDictIndex = iDictionaryIndex;
	m_clipHashKey = clipHash;

	SetPlaybackClip(fwAnimManager::GetClipIfExistsByDictIndex(iDictionaryIndex, clipHash));

	if (GetClip())
	{			
		animAssertf(pEntity->GetAnimDirector(), "Invalid AnimDirector : Model %s. clip %s", pEntity->GetDebugName(), GetClip()->GetName());
		m_iPriority = (eAnimPriority)priority;
		m_flags = flags;
		SetFlag(APF_ISPLAYING);
		if (terminationType==TerminationType_OnFinish)
		{
			SetFlag(APF_ISFINISHAUTOREMOVE);
		}
		else if (terminationType==TerminationType_OnDelete)
		{
			UnsetFlag(APF_ISFINISHAUTOREMOVE);
		}

		SetFilter(filterHash);

		m_pBlendHelper = static_cast<CMove&>(pEntity->GetAnimDirector()->GetMove()).FindAppropriateBlendHelperForClipHelper(*this);

		bool success = false;

		if (animVerifyf(m_pBlendHelper, "Could not find a blend helper to play the clip!"))
		{
			success = m_pBlendHelper->BlendInClip(pEntity, this, m_iPriority, fBlendDelta, (u8)terminationType);
			
			if (success)
			{
				m_pBlendHelper->SetLooped(this, (flags & APF_ISLOOPED) != 0);
				crFrameFilter* pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(m_filterHash));
				m_pBlendHelper->SetFilter(this, pFrameFilter);
			}
		}

		return success;
	}

	return false;
}

void CClipHelper::StopClip(float fBlend, u32 uFlags)
{
	if (GetClip() && (GetStatus()==Status_BlendingIn || GetStatus()==Status_FullyBlended))
	{
		Assertf(m_pAssociatedTask||m_pAssociatedEntity, "Helper must be associated with a task!");

		// we don't want to send blend out signals if the entity is about to be deleted anyway.

		CSimpleBlendHelper& helper = GetBlendHelper();

		helper.BlendOutClip(this, fBlend, uFlags);
	}

	SetPlaybackClip(NULL);
}



//*************************************************************************************************************
// ProcessAchieveTargetHeading
// Call this function every frame with a world target heading to scale the extracted angular velocity
// If the clipId turns the wrong way wrt the shortest arc to the target heading, then clipId's angular velocity
// will be applied unmodified.

void CClipHelper::ProcessAchieveTargetHeading(CPed * pPed, const float fTargetHeading)
{

	//get the clip and phase from the network
	const crClip * pClip = GetClip();
	CSimpleBlendHelper& helper = GetBlendHelper();
	if(!pClip)
	{
		Assertf(pClip, "No clip is playing");
		return;
	}
	if(!fwAnimHelpers::ContributesToAngularMovement(*pClip))
	{
		Assertf(0, "Animation has no overall angular movement");
		return;
	}

	float phase = helper.GetPhase(this);

	const Quaternion qStart = fwAnimHelpers::GetMoverTrackRotation(*pClip, 0.f);
	const Quaternion qCurrent = fwAnimHelpers::GetMoverTrackRotation(*pClip, phase);
	const Quaternion qEnd = fwAnimHelpers::GetMoverTrackRotation(*pClip, 1.f);

	Vector3 vStartRot, vCurrentRot, vEndRot;
	qStart.ToEulers(vStartRot, "xyz");
	qCurrent.ToEulers(vCurrentRot, "xyz");
	qEnd.ToEulers(vEndRot, "xyz");

	vStartRot.z = fwAngle::LimitRadianAngle(vStartRot.z);
	vCurrentRot.z = fwAngle::LimitRadianAngle(vCurrentRot.z);
	vEndRot.z = fwAngle::LimitRadianAngle(vEndRot.z);

	const float fTotalRotZ = fwAngle::LimitRadianAngle(vEndRot.z - vStartRot.z);
	const float fRemainingRotZ = fwAngle::LimitRadianAngle(vEndRot.z - vCurrentRot.z);
	const float fPedDeltaZ = SubtractAngleShorter(fwAngle::LimitRadianAngle(fTargetHeading), pPed->GetCurrentHeading());
	const float fScale = (Abs(fRemainingRotZ) > 0.0001f) ? (fPedDeltaZ / fRemainingRotZ) : 1.0f;

	float fExtractedAngVel = pPed->GetAnimatedAngularVelocity();

	// Only tweak & scale the extracted velocity if this clip turns the ped in the
	// same direction as the fPedDeltaZ.  If not, then allow it to play out normally.
	if(fExtractedAngVel != 0.0f && Sign(fTotalRotZ) == Sign(fPedDeltaZ))
	{
		fExtractedAngVel *= fScale;

		if(fPedDeltaZ < 0.0f)
		{
			fExtractedAngVel = Max(fExtractedAngVel, fPedDeltaZ/fwTimer::GetTimeStep());
			fExtractedAngVel = Min(fExtractedAngVel, 0.0f);
		}
		else if(fPedDeltaZ > 0.0f)
		{
			fExtractedAngVel = Min(fExtractedAngVel, fPedDeltaZ/fwTimer::GetTimeStep());
			fExtractedAngVel = Max(fExtractedAngVel, 0.0f);
		}
		else
		{
			fExtractedAngVel = 0.0f;
		}

		pPed->SetAnimatedAngularVelocity( fExtractedAngVel );
	}
}

//////////////////////////////////////////////////////////////////////////
// Get the amount of phase used to increment the clipId.
// If we're currently being updated on the simple blend helper
// we can grab the last phase from there. Otherwise we'll need to
// take an educated guess...
float CClipHelper::GetPhaseUpdateAmount() const
{
	float lastPhase = 0.0f;
	float currentPhase = 0.0f;
	if (GetStatus()==Status_BlendingIn || GetStatus()==Status_FullyBlended)
	{
		CSimpleBlendHelper& helper  = GetBlendHelper();
		lastPhase = helper.GetLastPhase(this);
		currentPhase = helper.GetPhase(this);

		return currentPhase - lastPhase;
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

inline CDynamicEntity*	CClipHelper::GetAssociatedEntity() const		
{ 
	animAssert(m_pAssociatedEntity || m_pAssociatedTask);
	if (m_pAssociatedEntity)
	{
		return m_pAssociatedEntity;
	}
	else
	{
		return static_cast<CDynamicEntity*>(m_pAssociatedTask->GetEntity());
	}
}

//////////////////////////////////////////////////////////////////////////

// Get the amount of time used to increment the clipId
float CClipHelper::GetTimeUpdateAmount() const
{	
	if (GetClip())
	{
		return GetClip()->ConvertPhaseToTime(GetPhaseUpdateAmount());
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
void CClipHelper::Restart(float fBlendDelta)
{
	animAssert(GetStatus()==Status_BlendingOut || GetStatus()==Status_Terminated);
	animAssert(m_pAssociatedTask);
	CPed* pPed = m_pAssociatedTask->GetPed();
	animAssert(pPed && pPed->GetAnimDirector());

	StartClipByDictAndHash(pPed, m_clipDictIndex.Get(), m_clipHashKey, m_flags, m_filterHash, (s32)m_iPriority, fBlendDelta, TerminationType_OnFinish);
}

//////////////////////////////////////////////////////////////////////////

bool CClipHelper::IsEvent(const CClipEventTags::Key iEvent) const
{
	// Is the specified event in the output parameter buffer?
	return CClipEventTags::FindFirstEventTag(GetClip(), iEvent, GetLastPhase(), GetPhase())!=NULL;
}

float CClipHelper::GetBlend() const
{
	switch(GetStatus())
	{
	case CClipHelper::Status_BlendingIn:
		{
			float duration = (float)(m_transitionEnd - m_transitionStart);
			float elapsed = (float)fwTimer::GetTimeInMilliseconds() - m_transitionStart;
			return duration ? elapsed / duration : 1.0f;
		}
	case CClipHelper::Status_BlendingOut:
		{
			float duration = (float)(m_transitionEnd - m_transitionStart);
			float elapsed = (float)fwTimer::GetTimeInMilliseconds() - m_transitionStart;
			return duration ? 1.0f - (elapsed / duration) : 0.0f;
		}
	case CClipHelper::Status_FullyBlended:
		return 1.0f;
	default:
		return 0.0f;
	}
}

//////////////////////////////////////////////////////////////////////////
//	Simple blend helper implementation
//////////////////////////////////////////////////////////////////////////

CSimpleBlendHelper::CSimpleBlendHelper()
: m_insertCB(NULL)
, m_blendInCB(NULL)
, m_blendOutCB(NULL)
{
	for (int i=0; i< Task_Num; i++)
	{
		m_slots[i] = NULL;
	}
	ResetSlots();
}

//////////////////////////////////////////////////////////////////////////

CSimpleBlendHelper::~CSimpleBlendHelper()
{
	ResetSlots();
}

void CSimpleBlendHelper::ResetSlots()
{
	for (int i=0; i<Task_Num; i++)
	{		
		ResetSlot(i);
	}
	m_events = 0;
}

void CSimpleBlendHelper::ResetSlot( int i )
{
	if (m_slots[i])
	{
		CClipHelper* pHelper = m_slots[i];
		pHelper->m_iStatus = CClipHelper::Status_Unassigned;
	}
	m_slots[i] = NULL;
	m_lastPhase[i] = 0.0f;
	m_terminationTypes[i] = CClipHelper::TerminationType_OnDelete;
	m_weights[i] = 0.0f;
	m_weightDeltas[i] = 0.0f;
	m_rates[i] = 1.0f;
	m_helperStatus[i] = Status_HelperChanged;
	m_blendOutDeltaNextFrame[i] = 1.0f;
}

//////////////////////////////////////////////////////////////////////////

void CSimpleBlendHelper::StartNetworkPlayer(CDynamicEntity* pEntity, float transitionDuration, u32 flags)
{
	if (!IsNetworkActive() || IsNetworkBlendingOut() || HasNetworkExpired())
	{
		CreateNetworkPlayer(pEntity, CClipNetworkMoveInfo::ms_NetworkTask );

		// insert into the parent using the callback
		if (m_insertCB && m_insertCB(GetNetworkPlayer(), transitionDuration, flags))
		{

		}
		else
		{
			animAssertf(0, "Unable to start simple blend helper. The entity's main MoVE network does not support it.");
		}
	}
	// We've already created a network player but the network never accepted it
	// (this sometimes happens when tasks play an clip and then abort in the same frame.)
	// Send it again now
	else if (IsNetworkActive() && IsNetworkReadyForInsert())
	{
		if (m_insertCB)
		{
			if(!m_insertCB(GetNetworkPlayer(), transitionDuration, 0))
			{
				animAssertf(0, "Failed to start the generic clip helper.");
			}
		}
		else
		{
			animAssertf(0, "Failed to start the generic clip helper. No insert callback was provided");
		}
	}
}

//////////////////////////////////////////////////////////////////////////

bool CSimpleBlendHelper::BlendInClip( CDynamicEntity* pEntity, CClipHelper* pHelper, eAnimPriority animPriority, float fBlendDelta, u8 terminationType )
{
	animAssert(pEntity);
	animAssert(pHelper);

	float blendDuration = fwAnimHelpers::CalcBlendDuration(fBlendDelta);

	u32 flags = 0;

	// TODO - add a new clip player flag for this purpose
	if (pHelper->IsFlagSet(APF_TAG_SYNC_WITH_MOTION_TASK))
	{
#if __ASSERT
		atHashString dictionaryName;
		const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pHelper->GetClipSet());
		if(pClipSet)
		{
			// Try to work out which dictionary the clip is in
			const fwClipSet* pSet = pClipSet;
			while(pSet)
			{
				const crClip* pClip = pSet->GetClip(pHelper->GetClipId(), CRO_NONE);
				if(pClip)
					break;
				pSet = pSet->GetFallbackSet();
			}
			if(!pSet)
				pSet = pClipSet;
			dictionaryName = pSet->GetClipDictionaryName();
		}
		ASSERT_ONLY(CTaskHumanLocomotion::VerifyClip(pHelper->GetClip(), pHelper->IsFlagSet(APF_ISLOOPED) ? CTaskHumanLocomotion::VCF_MovementCycle : CTaskHumanLocomotion::VCF_OneShot, atHashWithStringNotFinal(pHelper->GetClipHashKey()).TryGetCStr(), dictionaryName.IsNotNull() ? dictionaryName.TryGetCStr() : pHelper->GetClipDictionaryMetadata().TryGetCStr(), NULL));
#endif // __ASSERT
		flags = Flag_TagSyncWithMotionTask;
	}

	StartNetworkPlayer(pEntity, blendDuration, flags);

	CSimpleBlendHelper::ePriority priority = GetTaskPriority(animPriority);

	animAssert(priority<Task_Num);

	if (m_slots[priority])
	{
		if (blendDuration==0.0f || m_slots[priority]->IsFlagSet(APF_ISFINISHAUTOREMOVE))
		{
			m_slots[priority]->SetStatus(CClipHelper::Status_Terminated);
		}
		else
		{
			m_slots[priority]->SetStatus(CClipHelper::Status_BlendingOut);
			m_slots[priority]->m_transitionStart = fwTimer::GetTimeInMilliseconds();
			m_slots[priority]->m_transitionEnd = m_slots[priority]->m_transitionStart + (u32)(blendDuration*1000.0f);
		}
	}

	float weight = m_weights[priority];

	// Make sure we have clean slot data
	ResetSlot(priority);

	m_weights[priority] = weight;

	m_slots[priority] = pHelper;
	m_terminationTypes[priority] = terminationType;
	m_weightDeltas[priority] = fBlendDelta;

	if (blendDuration==0.0f)
	{
		m_slots[priority]->SetStatus(CClipHelper::Status_FullyBlended);
		m_weights[priority]=1.0f;
	}
	else
	{
		m_slots[priority]->SetStatus(CClipHelper::Status_BlendingIn);
		m_slots[priority]->m_transitionStart = fwTimer::GetTimeInMilliseconds();
		m_slots[priority]->m_transitionEnd = m_slots[priority]->m_transitionStart + (u32)(blendDuration*1000.0f);
	}

	//	insert the new clip
	CMoveNetworkHelper::SetClip( pHelper->GetClip(), GetClipId( priority ) );
	//	transition
	SendRequest( GetTransitionId( priority ) );
	SetFloat( GetDurationId( priority ), blendDuration );

	if (pHelper->IsFlagSet(APF_UPPERBODYONLY))
	{
		SetFlag(true, GetUpperBodyIkId(priority));
	}
	else
	{
		SetFlag(false, GetUpperBodyIkId(priority));
	}

	// If a blend in callback has been specified, call it now
	if (m_blendInCB)
	{
		m_blendInCB(*this, fBlendDelta, 0);
	}

	return true;
}

CClipHelper* CSimpleBlendHelper::FindClip(const char * pClipDictName, const char * pClipHash)
{
	s32 clipDictIndex = fwAnimManager::FindSlot(pClipDictName).Get();
	animAssert(clipDictIndex>-1);
	if (clipDictIndex>-1)
	{
		u32 clipHash = atStringHash(pClipHash);
		return FindClipByIndexAndHash(clipDictIndex, clipHash);
	}
	return NULL;
}

CClipHelper* CSimpleBlendHelper::FindClipByClipId(fwMvClipId clipId)
{
	for (u32 i=0; i<Task_Num; i++)
	{
		if (m_slots[i] && m_slots[i]->m_clipId==clipId)
		{
			return m_slots[i];
		}
	}
	return NULL;
}

CClipHelper* CSimpleBlendHelper::FindClipByIndex(s32 iDictionaryIndex)
{
	for (u32 i=0; i<Task_Num; i++)
	{
		if (m_slots[i] && m_slots[i]->m_clipDictIndex==iDictionaryIndex)
		{
			return m_slots[i];
		}
	}
	return NULL;
}


CClipHelper* CSimpleBlendHelper::FindClipBySetId(const fwMvClipSetId &clipSetId)
{
	for (u32 i=0; i<Task_Num; i++)
	{
		if (m_slots[i] && m_slots[i]->m_clipSetId==clipSetId)
		{
			return m_slots[i];
		}
	}
	return NULL;
}


CClipHelper* CSimpleBlendHelper::FindClipByPriority(eAnimPriority priority)
{
	ePriority taskPriority = GetTaskPriority(priority);
	return m_slots[taskPriority];
}

CClipHelper* CSimpleBlendHelper::FindClipBySetAndClip(const fwMvClipSetId &clipSetId, fwMvClipId clipId)
{
	s32 clipDictIndex = fwAnimManager::GetClipDictIndex(clipSetId);
	animAssert(clipDictIndex>-1);
	if (clipDictIndex>-1)
	{
		const crClip *clip = fwClipSetManager::GetClip(clipSetId, clipId);

		if (clip)
		{
			return FindClipByIndexAndHash(clipDictIndex, clipId.GetHash());
		}
	}
	return NULL;
}

CClipHelper* CSimpleBlendHelper::FindClipByIndexAndHash(s32 iClipDictIndex, u32 iClipHash)
{
	if (iClipDictIndex!=-1)
	{
		for (u32 i=0; i<Task_Num; i++)
		{
			if (m_slots[i] && m_slots[i]->m_clipHashKey==iClipHash && m_slots[i]->m_clipDictIndex==iClipDictIndex)
			{
				return m_slots[i];
			}
		}
	}
	return NULL;
}

float CSimpleBlendHelper::GetTotalWeight()
{
	float totalWeight = 0.0f;

	//for each attached helper
	for (u32 i=0; i<Task_Num; i++)
	{	
		if (m_slots[i])
		{
			totalWeight += m_weights[i];
		}
	}

	return totalWeight;
}

u32 CSimpleBlendHelper::GetNumActiveClips()
{
	u32 numClips = 0;
	//for each attached helper
	for (u32 i=0; i<Task_Num; i++)
	{	
		if (m_slots[i])
		{
			numClips++;
		}
	}
	return numClips;
}

//////////////////////////////////////////////////////////////////////////
void CSimpleBlendHelper::Update( float deltaTime )
{
	if (IsNetworkActive())
	{
		if (GetNetworkPlayer()->HasExpired())
		{
			//for each attached helper
			for (u32 i=0; i<Task_Num; i++)
			{	
				if (m_slots[i])
				{
					CClipHelper* pHelper = m_slots[i];
					if (pHelper)
					{
						pHelper->SetStatus(CClipHelper::Status_Terminated);
						m_slots[i] = NULL;
					}
				}
			}
			ReleaseNetworkPlayer();
		}
		else
		{
			//for each attached helper
			for (u32 i=0; i<Task_Num; i++)
			{	
				if (m_slots[i])
				{
					CClipHelper* pHelper = m_slots[i];

					float currentPhase = GetPhase(pHelper);

					//Update some network properties based on the clipId flags

					if (pHelper->IsFlagSet(APF_ISPLAYING))
					{
						if (GetRate(pHelper)==0.0f)
						{
							SetRate(pHelper, m_rates[i]);
						}
					}
					else
					{
						if (abs(GetRate(pHelper))>0.0f)
						{
							SetRate(pHelper, 0.0f);
						}
					}

					if (pHelper->IsFlagSet(APF_ISLOOPED))
					{
						if (!GetLooped(pHelper))
						{
							SetLooped(pHelper, true);
						}
					}
					else
					{
						if (GetLooped(pHelper))
						{
							SetLooped(pHelper, false);
						}

						// check if we should blend out the clipId because it's finished playing
						if (pHelper->IsFlagSet(APF_ISFINISHAUTOREMOVE) && currentPhase>=1.0f )
						{
							pHelper->StopClip(SLOW_BLEND_OUT_DELTA);
						}
					}

					// The clip and request for this helper are about to be sent
					if (m_helperStatus[i]==Status_HelperChanged)
					{
						m_helperStatus[i] = Status_HelperSent;
					}
				}

				if(m_blendOutDeltaNextFrame[i]<=0.0f)
				{
					if (m_helperStatus[i]==Status_HelperDeferredBlendOut)
					{
						m_helperStatus[i]=Status_HelperChanged;
					}
					else
					{
						SendBlendOutSignals((ePriority)i, m_blendOutDeltaNextFrame[i], 0);
						m_blendOutDeltaNextFrame[i] = 1.0f;
					}
				}

				// update the overall weight on the slot
				m_weights[i] = Clamp(m_weights[i]+(m_weightDeltas[i]*deltaTime), 0.0f, 1.0f);

				//send the weight signal
				SetFloat(GetWeightId((ePriority)i), m_weights[i]);
			}
			
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CSimpleBlendHelper::UpdatePostMotionTree()
{
	if (IsNetworkActive())
	{
		//for each attached helper
		for (u32 i=0; i<Task_Num; i++)
		{	
			if (m_slots[i])
			{
				// The output parameters now match the current clipId helper
				if (m_helperStatus[i]==Status_HelperSent)
				{
					m_helperStatus[i] = Status_HelperSignalsValid;
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CSimpleBlendHelper::UpdatePreOutputBufferFlip()
{
	if (IsNetworkActive())
	{
		//for each attached helper
		for (u32 i=0; i<Task_Num; i++)
		{	
			if (m_slots[i])
			{
				m_lastPhase[i] = GetPhase(m_slots[i]);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CSimpleBlendHelper::BlendOutClip( CClipHelper* pHelper, float fBlendDelta, u32 uFlags )
{

	ePriority priority = FindPriority(pHelper);

	animAssert(priority < Task_Num);

	if ( priority<Task_Num )
	{		
		float blendDuration = fwAnimHelpers::CalcBlendDuration(fBlendDelta);
		
		if (blendDuration==0.0f || m_slots[priority]->IsFlagSet(APF_ISFINISHAUTOREMOVE))
		{
			m_slots[priority]->SetStatus(CClipHelper::Status_Terminated);
		}
		else
		{
			m_slots[priority]->SetStatus(CClipHelper::Status_BlendingOut);
			m_slots[priority]->m_transitionStart = fwTimer::GetTimeInMilliseconds();
			m_slots[priority]->m_transitionEnd = m_slots[priority]->m_transitionStart + (u32)(blendDuration*1000.0f);
		}

		if (!m_slots[priority]->IsFlagSet(APF_SKIP_NEXT_UPDATE_BLEND))
		{
			m_slots[priority] = NULL;
			SendBlendOutSignals(priority, fBlendDelta, uFlags);
		}
		else
		{
			//defer the blend out for a frame
			m_blendOutDeltaNextFrame[priority] = fBlendDelta;
			m_helperStatus[priority] = Status_HelperDeferredBlendOut;
		}

		m_slots[priority] = NULL;
	}
}

void CSimpleBlendHelper::SendBlendOutSignals(ePriority priority, float blendDelta, u32 uFlags)
{
	float blendDuration = fwAnimHelpers::CalcBlendDuration(blendDelta);

	if (IsNetworkActive())
	{
		// if we're deleting the entity we don't want to send the blend out signals
		const CEntity* pEnt = static_cast<const CEntity*>(&GetNetworkPlayer()->GetAnimDirector().GetEntity());

		if (pEnt && pEnt->GetIsTypePed())
		{
			if (static_cast<const CPed*>(pEnt)->GetPedConfigFlag(CPED_CONFIG_FLAG_PedBeingDeleted))
			{
				// early out here
				// we don't want to send blend out signals on an entity who's being deleted
				return;
			}
		}

		//	transition
		SendRequest( GetTransitionId( priority ) );
		SetFloat( GetDurationId( priority ), blendDuration );
		SetFlag(false, GetUpperBodyIkId(priority));
		CMoveNetworkHelper::SetClip( NULL, GetClipId( priority ) );
	}

	// set the weight delta for the slot
	m_weightDeltas[priority] = blendDelta;

	if (m_blendOutCB)
	{
		m_blendOutCB(*this, blendDelta, uFlags);
	}
}

//////////////////////////////////////////////////////////////////////////

void CSimpleBlendHelper::BlendOutClip(eAnimPriority priority, float fBlendDelta, u32 uFlags)
{
	ePriority taskPriority = GetTaskPriority(priority);

	if (taskPriority >= Task_Low && taskPriority < Task_Num && m_slots[taskPriority])
	{
		BlendOutClip(m_slots[taskPriority], fBlendDelta, uFlags);
	}
}

//////////////////////////////////////////////////////////////////////////

bool CSimpleBlendHelper::IsFlagSet(u32 iFlag) const
{
	for (u32 i=0 ; i<Task_Num; i++)
	{
		if (m_slots[i])
		{
			CClipHelper* pHelper = m_slots[i];
			if (pHelper->IsFlagSet(iFlag))
			{
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

const CClipHelper * CSimpleBlendHelper::GetHighestUpperBodyHelper()
{
	const CClipHelper* pBestHelper = NULL;
	for (u32 i=0 ; i<Task_Num; i++)
	{
		if (m_slots[i])
		{
			CClipHelper* pHelper = m_slots[i];
			if (pHelper->IsFlagSet(APF_UPPERBODYONLY) && pHelper->GetClip())
			{
				if (!pBestHelper || pHelper->GetBlend()>= pBestHelper->GetBlend())
				{
// Need to fix up the weapon unholster clips before enabling this assert
//#if __ASSERT					
//					crFrameDataSingleDof frameData(kTrackBoneRotation, (u16)BONETAG_SPINE_ROOT, kFormatTypeQuaternion);
//					crFrameSingleDof frame(frameData);
//					pHelper->GetClip()->Composite(frame, 0.f);
//					animAssert(frame.Valid());
//#endif //__ASSERT
					pBestHelper = pHelper;
				}
			}			
		}
	}
	return pBestHelper;
}

//////////////////////////////////////////////////////////////////////////

CSimpleBlendHelper::ePriority CSimpleBlendHelper::GetTaskPriority(eAnimPriority priority)
{
	switch(priority)
	{
	case AP_HIGH:
		return Task_High;
	case AP_MEDIUM:
		return Task_Medium;
	default:
		return Task_Low;
	}
}

//////////////////////////////////////////////////////////////////////////

CGenericClipHelper::CGenericClipHelper()
: m_pClip(NULL)
, m_AutoBlendOutDelta(0.0f)
, m_bHoldAtEnd(0)
, m_insertCB(NULL)
, m_lastPhase(0.0f)
{

}

//////////////////////////////////////////////////////////////////////////

CGenericClipHelper::~CGenericClipHelper()
{
	SetClip(NULL);
}

bool CGenericClipHelper::BlendInClipBySetAndClip( CDynamicEntity* pEntity, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fBlendInDelta, float fAutoBlendOutDelta, bool looped, bool holdAtEnd, bool autoInsert)
{
	const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, clipId);
	return BlendInClipByClip(pEntity, pClip, fBlendInDelta, fAutoBlendOutDelta, looped, holdAtEnd, autoInsert);
}

//////////////////////////////////////////////////////////////////////////

bool CGenericClipHelper::BlendInClipByDictAndHash( CDynamicEntity* pEntity, s32 iDictionaryIndex, u32 uClipHash, float fBlendInDelta, float fAutoBlendOutDelta, bool looped, bool holdAtEnd, bool autoInsert)
{
	const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(iDictionaryIndex, uClipHash);
	return BlendInClipByClip(pEntity, pClip, fBlendInDelta, fAutoBlendOutDelta, looped, holdAtEnd, autoInsert);
}

//////////////////////////////////////////////////////////////////////////

// simple interface to start off a clip
bool CGenericClipHelper::BlendInClipByClip( CDynamicEntity* pEntity, const crClip* pClip, float fBlendInDelta, float fAutoBlendOutDelta, bool looped, bool holdAtEnd, bool autoInsert )
{
	animAssert(pEntity);
	animAssert(pClip);

	float blendDuration = fwAnimHelpers::CalcBlendDuration(fBlendInDelta);

	StartNetworkPlayer(pEntity, blendDuration, autoInsert);

	//	insert the new clip
	CMoveNetworkHelper::SetClip( pClip, GetClipId() );
	//	transition
	SendRequest( GetTransitionId() );
	SetFloat( GetDurationId(), blendDuration );
	SetLooped( looped );

	m_bHoldAtEnd = holdAtEnd;
	if (!m_bHoldAtEnd)
	{
		m_AutoBlendOutDelta = fAutoBlendOutDelta;
	}

	SetClip( pClip );

	// at this point we need to invalidate any output parameters coming from the old clip
	atArray<fwMvId> idsToRemove;
	idsToRemove.PushAndGrow(GetPhaseOutId());
	idsToRemove.PushAndGrow(GetRateOutId());
	idsToRemove.PushAndGrow(GetLoopedOutId());
	GetNetworkPlayer()->ClearOutputParameters(idsToRemove);

	return true;
}

//////////////////////////////////////////////////////////////////////////

void CGenericClipHelper::BlendOutClip( float fBlendDelta )
{
	if (IsNetworkActive())
	{
		float blendDuration = fwAnimHelpers::CalcBlendDuration(fBlendDelta);

		//	insert the new clip
		CMoveNetworkHelper::SetClip( NULL, GetClipId( ) );
		//	transition
		SendRequest( GetTransitionId() );
		SetFloat( GetDurationId(), blendDuration );

		SetClip(NULL);	
	}						

}

//////////////////////////////////////////////////////////////////////////

bool CGenericClipHelper::IsEvent(const CClipEventTags::Key iEvent) const
{
	// Is the specified event in the output parameter buffer?
	return CClipEventTags::FindFirstEventTag(GetClip(), iEvent, GetLastPhase(), GetPhase())!=NULL;
}


//////////////////////////////////////////////////////////////////////////

void CGenericClipHelper::Update()
{
	if (IsNetworkActive())
	{
		if (GetNetworkPlayer()->HasExpired())
		{
			SetClip(NULL);
			ReleaseNetworkPlayer();
		}
		else if (GetClip())
		{
			CMoveNetworkHelper::SetClip(m_pClip, GetClipId());
			if (!m_bHoldAtEnd)
			{	
				float phase = GetPhase();

				if (phase >= 1.0f)
				{
					BlendOutClip(m_AutoBlendOutDelta);
				}
			}

			fwMoveNetworkPlayer *pNetworkPlayer = GetNetworkPlayer();
			if(pNetworkPlayer)
			{
				CEntity *pEntity = static_cast< CEntity * >(&pNetworkPlayer->GetAnimDirector().GetEntity());
				if(pEntity->GetIsPhysical() && !pEntity->GetIsTypePed())
				{
					fragInst *pFragInst = static_cast< CPhysical * >(pEntity)->GetFragInst();
					if(pFragInst && pFragInst->GetSkeleton() && pFragInst->GetCached())
					{
						pFragInst->PoseBoundsFromSkeleton(true, true);
					}
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

// PURPOSE: Creates the network player and attaches it to move
void CGenericClipHelper::StartNetworkPlayer(CDynamicEntity* pEntity, float transitionDuration, bool doInsert)
{
	if (!IsNetworkActive() || IsNetworkBlendingOut() || HasNetworkExpired() )
	{
		CreateNetworkPlayer(pEntity, CClipNetworkMoveInfo::ms_NetworkGenericClip );

		// insert into the appropriate slot in fwMove
		if (doInsert && m_insertCB)
		{
			if(!m_insertCB(GetNetworkPlayer(), transitionDuration, 0))
			{
				animAssertf(0, "Failed to start the generic clip helper. Insert callback returned false");
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CGenericClipHelper::SetFilter(atHashString filter)
{
	crFrameFilter* pFrameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(fwMvFilterId(filter));
	CMoveNetworkHelper::SetFilter( pFrameFilter, GetFilterId() );
}

//////////////////////////////////////////////////////////////////////////

// Allow 16 chat helpers
FW_INSTANTIATE_CLASS_POOL(CChatHelper, 16, atHashString("CChatHelper",0x411e7493));

bank_u32 CChatHelper::ms_iDefaultMinTimeBeforeChatReset		= 30000;		// 30 seconds
bank_u32 CChatHelper::ms_iDefaultMaxTimeBeforeChatReset		= 60000;		// 60 seconds
bank_u32 CChatHelper::ms_iDefaultValidityTime					= 3000;			// Ped is considered valid if we saw them 3 seconds ago

CChatHelper* CChatHelper::CreateChatHelper()
{
	if (_ms_pPool->GetNoOfFreeSpaces() > 0)
	{
		return rage_new CChatHelper();
	}
	
	return NULL;
}

CChatHelper::CChatHelper() : 
m_pInComingChatPed(NULL),
m_pOutGoingChatPed(NULL),
m_pBestChatTarget(NULL),
m_iTimeBestPedWasValid(0),
m_bIsSet(false),
m_bShouldChat(false),
m_iTimeDelayBeforeChatTimerReset(fwRandom::GetRandomNumberInRange(static_cast<s32>(ms_iDefaultMinTimeBeforeChatReset),static_cast<s32>(ms_iDefaultMaxTimeBeforeChatReset))),
m_bIsChatting(false),
m_iValidityTime(ms_iDefaultValidityTime)
{
}

CChatHelper::~CChatHelper()
{
}

void CChatHelper::ProcessChat(CPed* pPed)
{
	if (!m_bIsSet)
	{
		m_pThisPed	= pPed;
		m_bIsSet	= true;
		m_chatTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iTimeDelayBeforeChatTimerReset);
	}
	else if (!m_bIsChatting)
	{
		UpdateChat();
	}
}

bool CChatHelper::ShouldStartTaskAsInitiator()
{
	CPed* pOutGoingChatPed = GetOutGoingChatPed();
	if (pOutGoingChatPed)
	{
		return true;
	}

	CPed* pInComingChatPed = GetInComingChatPed();
	if (pInComingChatPed)
	{
		return false;
	}

	taskAssertf(0,"No outgoing or incoming chat ped set");
	return false;
}

void CChatHelper::FinishChat()
{
	m_pInComingChatPed	= NULL;
	m_pOutGoingChatPed	= NULL;
	m_bShouldChat		= false;
	m_bIsChatting		= false;
	m_chatTimer.Set(fwTimer::GetTimeInMilliseconds(),m_iTimeDelayBeforeChatTimerReset);
}

void CChatHelper::UpdateChat()
{
	// Update only if ped is not chatting
	if (m_bIsSet)
	{
		if (m_chatTimer.IsSet() && m_chatTimer.IsOutOfTime())
		{
			if (GetBestChatPed()) // We have a valid chat ped
			{
				m_bShouldChat = true;
			}
			else
			{
				m_bShouldChat = false;
			}
		}
	}
}

CPed* CChatHelper::GetBestChatPed() const 
{ 
	if (m_pBestChatTarget && !m_pBestChatTarget->IsPlayer()) 
	{
		u32 iTimeDiffChatPedWasValid = fwTimer::GetTimeInMilliseconds() - m_iTimeBestPedWasValid;
		if (iTimeDiffChatPedWasValid < m_iValidityTime)
		{
			return m_pBestChatTarget.Get(); 
		}	
	}
	return NULL;
}

CTask* CChatHelper::GetPedsTask(CPed* pPed)
{
	CTask* pPatrolTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PATROL);
	if (pPatrolTask)
	{
		return pPatrolTask;
	}

	CTask* pStandGuardTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_STAND_GUARD_FSM);
	if (pStandGuardTask)
	{
		return pStandGuardTask;
	}

	CTask* pInvestigateTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_INVESTIGATE);
	if (pInvestigateTask)
	{
		return pInvestigateTask;
	}

	CTask* pSearchForUnknownTask = pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_SEARCH_FOR_UNKNOWN_THREAT);
	if (pSearchForUnknownTask)
	{
		return pSearchForUnknownTask;
	}
	
	return NULL;
}

bool CChatHelper::ShouldGoToChatState()
{
	// We are trying to chat to someone
	if (m_bShouldChat && !m_bIsChatting)
	{
		// TODO: Run a probability thing here to decide if we should even consider chatting
		CPed* pOutGoingChatPed = GetBestChatPed();
		if (pOutGoingChatPed)
		{
			CTask* pOutGoingChatPedsTask = CChatHelper::GetPedsTask(pOutGoingChatPed);
			if (pOutGoingChatPedsTask)
			{
				CChatHelper* pOutGoingChatPedsChatHelper = pOutGoingChatPedsTask->GetChatHelper();
				if (pOutGoingChatPedsChatHelper && !pOutGoingChatPedsChatHelper->GetIsChatting()) // The ped isn't already chatting
				{
					pOutGoingChatPedsChatHelper->SetInComingChatPed(m_pThisPed); // Send the chat ped a request to chat
					m_pOutGoingChatPed = pOutGoingChatPed;						 // Set the ped we wish to chat to
					return true;
				}	
			}
		}
	}

	// Someone is trying to chat to us
	if (m_pInComingChatPed && !m_bIsChatting) 
	{
		return true;
	}
	else
	{
		m_pInComingChatPed = NULL;  // Remove new chat ped
	}

	return false;
}

#if !__FINAL
void CChatHelper::Debug() const
{	// Debug
	CPed* pBestChatPed = GetBestChatPed();
	if (m_pThisPed && pBestChatPed)
	{
		DEBUG_DRAW_ONLY(grcDebugDraw::Line(VEC3V_TO_VECTOR3(m_pThisPed->GetTransform().GetPosition()), VEC3V_TO_VECTOR3(pBestChatPed->GetTransform().GetPosition()), Color_yellow1, Color_red1));
	}
}
#endif // !__FINAL

CClearanceSearchHelper::CClearanceSearchHelper(float fDistanceToTest)
:
m_fDistanceToTest(fDistanceToTest),
m_vSearchPosition(Vector3(0.0f,0.0f,0.0f))
{
}

CClearanceSearchHelper::~CClearanceSearchHelper()
{
}

void CClearanceSearchHelper::Reset()
{
	m_bIsActive = false;
	m_bFinished = false;
	m_eCurrentProbeDirection = North;
	m_probeTestHelper.ResetProbe();
	for (int i = 0; i < MaxNumDirections; i++)
	{
		m_fDirectionVectorDistances[i] = 0.0f;
	}
}

void CClearanceSearchHelper::GetDirectionVector(ProbeDirection probeDirection, Vector3& vDirectionOut) const
{
	vDirectionOut = Vector3(0.0f,0.0f,0.0f);

	// Determine the direction vector for a given direction
	switch (probeDirection)
	{
	case North:
		vDirectionOut = Vector3(1.0f,0.0f,0.0f);
		break;
	case NorthEast:
		vDirectionOut = Vector3(0.707f,-0.707f,0.0f);
		break;
	case East:
		vDirectionOut = Vector3(0.0f,-1.0f,0.0f);	
		break;
	case SouthEast:
		vDirectionOut = Vector3(-0.707f,-0.707f,0.0f);
		break;
	case South:
		vDirectionOut = Vector3(-1.0f,0.0f,0.0f);
		break;
	case SouthWest:
		vDirectionOut = Vector3(-0.707f,0.707f,0.0f);
		break;
	case West: 
		vDirectionOut = Vector3(0.0f,1.0f,0.0f);
		break;
	case NorthWest:
		vDirectionOut = Vector3(0.707f,0.707f,0.0f);
		break;
	default:
		taskAssertf(0,"The probe direction is incorrectly set");
		break;
	}
}

void CClearanceSearchHelper::Start(const Vector3 &vPositionToSearchFrom)
{
	Reset();

	m_vSearchPosition = vPositionToSearchFrom;
	// Get the end vector for probing and start probing
	Vector3 vDirection;
	GetDirectionVector(m_eCurrentProbeDirection, vDirection);
	Vector3 vPositionToEndSearch = vPositionToSearchFrom + m_fDistanceToTest * vDirection;
	IssueWorldProbe(vPositionToSearchFrom,vPositionToEndSearch);
	m_bIsActive = true;
}

void CClearanceSearchHelper::IssueWorldProbe(const Vector3& vPositionToSearchFrom, const Vector3& vPositionToEndSearch)
{
	WorldProbe::CShapeTestProbeDesc probeData;
	probeData.SetStartAndEnd(vPositionToSearchFrom,vPositionToEndSearch);
	probeData.SetContext(WorldProbe::EMovementAI);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE);
	probeData.SetIsDirected(true);
	m_probeTestHelper.StartTestLineOfSight(probeData);
}

#if !__FINAL
void CClearanceSearchHelper::Debug() const
{
#if DEBUG_DRAW
	for (int i = 0; i < MaxNumDirections; i++)
	{
		Vector3 vDirection;
		GetDirectionVector(static_cast<ProbeDirection>(i),vDirection);
		DEBUG_DRAW_ONLY(grcDebugDraw::Line(m_vSearchPosition,m_vSearchPosition+m_fDirectionVectorDistances[i]*vDirection, Color_blue, Color_green));
		DEBUG_DRAW_ONLY(grcDebugDraw::Sphere(m_vSearchPosition + m_fDirectionVectorDistances[i]*vDirection,0.05f, Color_blue, false));
	}
#endif // DEBUG_DRAW
}
#endif // !FINAL

void CClearanceSearchHelper::GetShortestDirection(Vector3& vShortestDirectionOut)
{
	ProbeDirection shortestDirection = North;
	for (int i = 1; i < MaxNumDirections; i++)
	{
		if (m_fDirectionVectorDistances[i] < m_fDirectionVectorDistances[shortestDirection])
		{
			shortestDirection = static_cast<ProbeDirection>(i);
		}
	}
	Vector3 vDirection;
	GetDirectionVector(shortestDirection,vDirection);
	vShortestDirectionOut = m_fDirectionVectorDistances[shortestDirection]*vDirection;
}

void CClearanceSearchHelper::GetDirection(int iDirection, Vector3& vDirectionOut)
{
	GetDirectionVector(static_cast<ProbeDirection>(iDirection),vDirectionOut);
	vDirectionOut *= m_fDirectionVectorDistances[iDirection];
}

void CClearanceSearchHelper::GetLongestDirection(Vector3& vLongestDirectionOut)
{
	ProbeDirection longestDirection = North;
	for (int i = 1; i < MaxNumDirections; i++)
	{
		if (m_fDirectionVectorDistances[i] > m_fDirectionVectorDistances[longestDirection])
		{
				longestDirection = static_cast<ProbeDirection>(i);
		}
	}
	Vector3 vDirection;
	GetDirectionVector(longestDirection,vDirection);
	vLongestDirectionOut = m_fDirectionVectorDistances[longestDirection]*vDirection;
}

void CClearanceSearchHelper::GetLongestDirectionAway(Vector3& vLongestDirectionOut, Vector3& vAvoidDirection)
{
	ProbeDirection longestDirection = North;
	for (int i = 1; i < MaxNumDirections; i++)
	{
		if (m_fDirectionVectorDistances[i] > m_fDirectionVectorDistances[longestDirection])
		{
			// If we're avoiding a direction, test to see if the vector is facing the avoidance direction
			vAvoidDirection.Normalize();
			Vector3 vTempDirection;
			GetDirectionVector(static_cast<ProbeDirection>(i),vTempDirection);
			vTempDirection.Normalize();
			if (vTempDirection.Dot(vAvoidDirection) > 0.0f)
			{
				// It's facing away from the direction, which is good
				longestDirection = static_cast<ProbeDirection>(i);
			}
		}
	}
	Vector3 vDirection;
	GetDirectionVector(longestDirection,vDirection);
	vLongestDirectionOut = m_fDirectionVectorDistances[longestDirection]*vDirection;
}

void CClearanceSearchHelper::Update()
{
	// If helper is active and we've not finished probing
	if (m_bIsActive && !m_bFinished)
	{
		// If we're waiting for a sight test try and get the results
		if( m_probeTestHelper.IsProbeActive() )
		{
			ProbeStatus probeStatus;
			Vector3 vIntersectionPos;

			if( m_probeTestHelper.GetProbeResultsWithIntersection(probeStatus,&vIntersectionPos) )
			{
				Vector3 vDirection;
				GetDirectionVector(m_eCurrentProbeDirection,vDirection);
				
				// If position is clear, the direction vector is the test distance times direction vector
				if (probeStatus == PS_Clear)
				{
					m_fDirectionVectorDistances[m_eCurrentProbeDirection] = m_fDistanceToTest;
				}
				else
				{
					// Otherwise find the distance from intersection and store the direction vector
					float fDistFromPositionToIntersection = m_vSearchPosition.Dist(vIntersectionPos);
					m_fDirectionVectorDistances[m_eCurrentProbeDirection] = fDistFromPositionToIntersection;
				}

				// We've checked all directions, no need for further probes
				if (m_eCurrentProbeDirection == NorthWest)
				{
					m_bFinished = true;
					m_bIsActive = false;
					m_probeTestHelper.ResetProbe();
				}
				else
				{
					// Set the next probe direction
					m_eCurrentProbeDirection = static_cast<ProbeDirection>(m_eCurrentProbeDirection + 1);
					// Get the end vector for probing and start probing
					Vector3 vDirection;
					GetDirectionVector(m_eCurrentProbeDirection, vDirection);
					Vector3 vPositionToEndSearch = m_vSearchPosition + m_fDistanceToTest * vDirection;
					IssueWorldProbe(m_vSearchPosition,vPositionToEndSearch);
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// MoVE network helper implementation
//////////////////////////////////////////////////////////////////////////

const fwMvStateId CMoveNetworkHelper::ms_DefaultStateMachineId("StateMachine",0x1997DF3C);

CMoveNetworkHelper::CMoveNetworkHelper()
: m_pPlayer(NULL)
, m_deferredInsertParent(NULL)
, m_deferredInsertNetworkId(u32(0))
, m_deferredInsertDuration(0.0f)
#if __DEV
, m_numNetworkPlayersCreatedThisFrame(0)
, m_lastNetworkPlayerCreatedFrame(0)
#endif // __DEV
{
}

//////////////////////////////////////////////////////////////////////////

CMoveNetworkHelper::~CMoveNetworkHelper()
{
	ReleaseNetworkPlayer();
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
// Network def streaming interface

#if __BANK
mvNetworkDef* CMoveNetworkHelper::LoadNetworkDefFromDisk(const char * filename)
{
	mvNetworkDef* pNetworkDef = rage_new mvNetworkDef;
	bool success = pNetworkDef->Load(filename);
	if (!success)
	{
		animAssertf(0, "Unable to load MoVE network file %s", filename);
	}
	return pNetworkDef;
}

#endif //__BANK

//////////////////////////////////////////////////////////////////////////

bool CMoveNetworkHelper::RequestNetworkDef(const fwMvNetworkDefId &networkDefId)
{
	return fwAnimDirector::RequestNetworkDef(networkDefId);
}

//////////////////////////////////////////////////////////////////////////

bool CMoveNetworkHelper::IsNetworkDefStreamedIn(const fwMvNetworkDefId &networkDefId)
{
	return fwAnimDirector::IsNetworkDefLoaded(networkDefId);
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::ForceLoadNetworkDef(const fwMvNetworkDefId &networkDefId)
{
	fwAnimDirector::ForceLoadNetworkDef(networkDefId);
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::CreateNetworkPlayer(const CDynamicEntity* pEntity, const fwMvNetworkDefId &networkDefId)
{
	taskAssertf(g_NetworkDefStore.FindSlotFromHashKey(networkDefId.GetHash())!=-1, "Trying to create network player from non existant network '%s'(%u)!", networkDefId.TryGetCStr()? networkDefId.GetCStr():"UNKNOWN", networkDefId.GetHash());
	taskAssertf(IsNetworkDefStreamedIn(networkDefId), "Trying to create network player from network '%s'(%u), which is not streamed in!", networkDefId.TryGetCStr()? networkDefId.GetCStr():"UNKNOWN", networkDefId.GetHash());
	
	const mvNetworkDef& networkDef = fwAnimDirector::GetNetworkDef(networkDefId);

	ReleaseNetworkPlayer();

	if (taskVerifyf(pEntity->GetAnimDirector(), "Error trying to create network player from network '%s' on entity '%s'. The entity has no anim director.", networkDef.GetName(), pEntity->GetModelName()))
	{
		// If the entity is currently frozen we shouldn't be doing this, as the unused network players will not be being cleaned up.
		taskAssertf(pEntity->m_nDEflags.bFrozen==false || (pEntity->GetAnimDirector() && pEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentMove>()->GetNumNetworkPlayersAddedSinceLastDelete()<CREATE_NETWORK_WARNING_LENGTH), "A task is trying to create MoVE network '%s' whilst its entity '%s' is frozen!", networkDefId.GetCStr(), pEntity->GetModelName() );

	#if __DEV
		if (!taskVerifyf(m_numNetworkPlayersCreatedThisFrame < CREATE_NETWORK_WARNING_LENGTH || m_lastNetworkPlayerCreatedFrame!=fwTimer::GetFrameCount(), "A task is creating multiple network players in the same frame! Entity: %s, Move network:%s", pEntity->GetModelName(), fwAnimDirector::FindNetworkDefName(&networkDef)))
		{
			taskDisplayf("Creating multiple network players: Entity: %s, Move network:%s, frame:%u", pEntity->GetModelName(), fwAnimDirector::FindNetworkDefName(&networkDef), fwTimer::GetFrameCount());
		}
		if (m_lastNetworkPlayerCreatedFrame!=fwTimer::GetFrameCount())
		{
			m_lastNetworkPlayerCreatedFrame = fwTimer::GetFrameCount();
			m_numNetworkPlayersCreatedThisFrame = 0;
		}
		else
		{
			m_numNetworkPlayersCreatedThisFrame++;
		}
	#endif //__DEV

		m_pPlayer = pEntity->GetAnimDirector()->CreateNetworkPlayer( networkDef );

		m_pPlayer->AddRef();
	}
}

//////////////////////////////////////////////////////////////////////////
#if __BANK
void CMoveNetworkHelper::CreateNetworkPlayer(const CDynamicEntity* pEntity, const mvNetworkDef* networkDef)
{
	ReleaseNetworkPlayer();

#if __DEV
	if (!taskVerifyf(m_numNetworkPlayersCreatedThisFrame < CREATE_NETWORK_WARNING_LENGTH || m_lastNetworkPlayerCreatedFrame!=fwTimer::GetFrameCount(), "A task is creating multiple network players in the same frame! Entity: %s, Move network:%s", pEntity->GetModelName(), fwAnimDirector::FindNetworkDefName(networkDef)))
	{
		taskDisplayf("Creating multiple network players: Entity: %s, Move network:%s, frame:%u", pEntity->GetModelName(), fwAnimDirector::FindNetworkDefName(networkDef), fwTimer::GetFrameCount());
	}
	if (m_lastNetworkPlayerCreatedFrame!=fwTimer::GetFrameCount())
	{
		m_lastNetworkPlayerCreatedFrame = fwTimer::GetFrameCount();
		m_numNetworkPlayersCreatedThisFrame = 0;
	}
	else
	{
		m_numNetworkPlayersCreatedThisFrame++;
	}
#endif //__DEV

	// If the entity is currently frozen we shouldn't be doing this, as the unused network players will not be being cleaned up.
	animAssert(pEntity->m_nDEflags.bFrozen==false || (pEntity->GetAnimDirector() && pEntity->GetAnimDirector()->GetComponent<fwAnimDirectorComponentMove>()->GetNumNetworkPlayersAddedSinceLastDelete()<CREATE_NETWORK_WARNING_LENGTH));

	m_pPlayer = pEntity->GetAnimDirector()->CreateNetworkPlayer( *networkDef );

	m_pPlayer->AddRef();
}
#endif //__BANK

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::ReleaseNetworkPlayer()
{
	if (m_pPlayer)
	{
		m_pPlayer->Release();
		m_pPlayer = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetSubNetwork(fwMoveNetworkPlayer* player, const fwMvNetworkId &networkId)
{
	mvSubNetwork* pSubNetwork = NULL;

	if (player!=NULL)
	{
		pSubNetwork = player->GetSubNetwork();
	}

	m_pPlayer->SetSubNetwork(networkId, pSubNetwork);
}

//////////////////////////////////////////////////////////////////////////
void CMoveNetworkHelper::SetSubNetworkCrossBlend( /*fwMove::Id id,*/ float duration)
{
	m_pPlayer->SetSubNetworkBlendDuration(/*id,*/ duration);
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SendRequest( const fwMvRequestId &requestId )
{
	if( taskVerifyf(m_pPlayer!=NULL, "Request sent but no network active!") )
	{
		m_pPlayer->BroadcastRequest( requestId );
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetClip( const crClip* pClip, const fwMvClipId &clipId )
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		taskAssertf(m_pPlayer->GetAnimDirector().GetComponent<fwAnimDirectorComponentMove>()->HasNetworkPlayer(m_pPlayer), "mvMoveNetworkPlayer isn't attached to the clipId director it thought it was!");
		m_pPlayer->SetClip( clipId, pClip);
	}
}


//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetClipSet(const fwMvClipSetId &clipSetId, const fwMvClipSetVarId &clipSetVarId /* = CLIP_SET_VAR_ID_DEFAULT */)
{
	taskAssertf(clipSetVarId!=CLIP_SET_VAR_ID_INVALID, "Invalid clipSetVarId. clipSetVarId = %u %s", clipSetVarId.GetHash(), clipSetVarId.TryGetCStr());
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to set an clipId set on a non existent network!") )
	{
#if __DEV
		auto currentClipSetId = GetClipSetId(clipSetVarId);
		if (currentClipSetId != clipSetId)
		{
			Displayf("CMoveNetworkHelper::SetClipSet: clipsetId: %u %s, clipSetVarId: %u %s, CMoveNetworkHelper: 0x%p", clipSetId.GetHash(), clipSetId.TryGetCStr(), clipSetVarId.GetHash(), clipSetVarId.TryGetCStr(), this);
		}		
#endif
		m_pPlayer->SetClipSet(clipSetId, clipSetVarId);
	}
}

//////////////////////////////////////////////////////////////////////////

const fwClipSet* CMoveNetworkHelper::GetClipSet(const fwMvClipSetVarId &clipSetVarId) const 
{ 
	return m_pPlayer->GetClipSet(clipSetVarId);
}

//////////////////////////////////////////////////////////////////////////

const fwMvClipSetId CMoveNetworkHelper::GetClipSetId(const fwMvClipSetVarId &clipSetVarId) const 
{ 
	if (GetNetworkPlayer())
	{
		return GetNetworkPlayer()->GetClipSetId(clipSetVarId); 
	}
	return CLIP_SET_ID_INVALID;
}

//////////////////////////////////////////////////////////////////////////

const crClip* CMoveNetworkHelper::GetClip( const fwMvClipId &clipId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a clip signal parameter but no network active!") )
	{
		return m_pPlayer->GetClip( clipId );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetFloat( const fwMvFloatId &floatId, const float fValue )
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		m_pPlayer->SetFloat( floatId, fValue);
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetBoolean( const fwMvBooleanId &booleanId, const bool bValue )
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		m_pPlayer->SetBoolean( booleanId, bValue );
	}
}

//////////////////////////////////////////////////////////////////////////

float CMoveNetworkHelper::GetFloat( const fwMvFloatId &floatId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a signal but no network active!") )
	{
		return m_pPlayer->GetFloat( floatId );
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////

bool CMoveNetworkHelper::GetFloat( const fwMvFloatId &floatId, float& val) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a signal but no network active!") )
	{
		return m_pPlayer->GetFloat( floatId, val );
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////

bool CMoveNetworkHelper::GetBoolean( const fwMvBooleanId &booleanId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a signal but no network active!") )
	{
		return m_pPlayer->GetBoolean( booleanId );
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CMoveNetworkHelper::GetBoolean( const fwMvBooleanId &booleanId, bool& val) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a signal but no network active!") )
	{
		return m_pPlayer->GetBoolean( booleanId, val );
	}
	return false;
}


//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetFlag( bool bflagValue, const fwMvFlagId &flagId)
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		m_pPlayer->SetFlag( flagId, bflagValue);
	}
}


//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetAnimation( const crAnimation* pAnim, const fwMvAnimId &animId )
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		m_pPlayer->SetAnimation( animId, pAnim);
	}
}

//////////////////////////////////////////////////////////////////////////

const crAnimation* CMoveNetworkHelper::GetAnimation( const fwMvAnimId &animId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get an animation signal parameter but no network active!") )
	{
		return m_pPlayer->GetAnimation( animId );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetParameterizedMotion( const crpmParameterizedMotion* pParam, const fwMvParameterizedMotionId &pmId)
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		crpmParameterizedMotion* pPm = const_cast<crpmParameterizedMotion*>(pParam);
		m_pPlayer->SetParameterizedMotion( pmId, pPm);
	}
}

//////////////////////////////////////////////////////////////////////////

const crpmParameterizedMotion* CMoveNetworkHelper::GetParameterizedMotion( const fwMvParameterizedMotionId &pmId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a PM signal parameter but no network active!") )
	{
		return m_pPlayer->GetParameterizedMotion( pmId );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetExpressions( crExpressions* pExpression, const fwMvExpressionsId &exprId)
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		m_pPlayer->SetExpressions( exprId, pExpression);
	}
}

//////////////////////////////////////////////////////////////////////////

const crExpressions* CMoveNetworkHelper::GetExpressions( const fwMvExpressionsId &exprId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get an expressions signal parameter but no network active!") )
	{
		return m_pPlayer->GetExpressions( exprId );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetFilter( crFrameFilter* pFilter, const fwMvFilterId &filterId)
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		m_pPlayer->SetFilter( filterId, pFilter);
	}
}

//////////////////////////////////////////////////////////////////////////

const crFrameFilter* CMoveNetworkHelper::GetFilter( const fwMvFilterId &filterId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a filter signal parameter but no network active!") )
	{
		return m_pPlayer->GetFilter( filterId );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::SetFrame(crFrame* pFrame, const fwMvFrameId &frameId)
{
	if( taskVerifyf(m_pPlayer!=NULL, "Signal sent but no network active!") )
	{
		m_pPlayer->SetFrame( frameId, pFrame);
	}
}

//////////////////////////////////////////////////////////////////////////

const crFrame* CMoveNetworkHelper::GetFrame( const fwMvFrameId &frameId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a frame signal parameter but no network active!") )
	{
		return m_pPlayer->GetFrame( frameId );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

const crProperty* CMoveNetworkHelper::GetProperty( const fwMvPropertyId &propId) const
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to get a frame signal parameter but no network active!") )
	{
		return m_pPlayer->GetProperty( propId );
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

//int CMoveNetworkHelper::GetEventsForPhase(const crClip* pClip, const float fStartPhase, const float fStopPhase)
//{
//	taskAssert(fStartPhase>=0.0f);
//	taskAssert(fStopPhase<=1.0f);
//	taskAssert(fStartPhase<=fStopPhase);
//
//	int iEvents = 0;
//
//		if( taskVerifyf(pClip, "pClip is NULL") )
//		{
//			if( pClip->HasTags() )
//			{
//				CTagIteratorGtaEvents it(*pClip->GetTags(), fStartPhase, fStopPhase);
//				CTagAnalyserGta<CTagIteratorGtaEvents> analyzer(it);
//				iEvents = analyzer.GetEvents(false, true);
//			}
//		}
//
//	return iEvents;
//}
//
////////////////////////////////////////////////////////////////////////////
//
//bool CMoveNetworkHelper::FindEventPhase(int iEventToFind, float& fEventPhase, const crClip* pClip, const float fStartPhase, const float fStopPhase)
//{
//	taskAssert(fStartPhase>=0.0f);
//	taskAssert(fStopPhase<=1.0f);
//	taskAssert(fStartPhase<=fStopPhase);
//
//		if( taskVerifyf(pClip, "pClip is NULL") )
//		{
//			if( pClip->HasTags() )
//			{
//				CTagIteratorGtaEvents it(*pClip->GetTags(), fStartPhase, fStopPhase);
//				CTagAnalyserGta<CTagIteratorGtaEvents> analyzer(it, u32(iEventToFind));
//				return analyzer.GetEvents(true, false, NULL, &fEventPhase);
//			}
//		}
//
//	return false;
//}

//////////////////////////////////////////////////////////////////////////

void CMoveNetworkHelper::ForceStateChange( const fwMvStateId &targetStateId, const fwMvStateId &stateMachineId /* = ms_DefaultStateMachineId */)
{
	if( taskVerifyf(m_pPlayer!=NULL, "Trying to force a state change but no network active!") )
	{
		m_pPlayer->ForceState(stateMachineId, targetStateId );
	}
}

void CMoveNetworkHelper::SetupDeferredInsert(CMoveNetworkHelper* pParent, const fwMvBooleanId &parentReadyEventId, const fwMvNetworkId &insertNetworkId, float blendDuration)
{
	m_deferredInsertParent = pParent;
	m_deferredInsertNetworkId = insertNetworkId;
	m_deferredInsertDuration = blendDuration;

	if (parentReadyEventId.GetHash() != 0)
	{
		pParent->WaitForTargetState(parentReadyEventId);
	}
}

void CMoveNetworkHelper::DeferredInsert()
{
	//animAssert(IsParentReadyForInsert());
	animAssert(m_deferredInsertParent);
	animAssert(m_deferredInsertNetworkId.GetHash());
	animAssert(m_pPlayer);

	m_pPlayer->SetSubNetworkBlendDuration(m_deferredInsertDuration);
	m_deferredInsertParent->SetSubNetwork(m_pPlayer, m_deferredInsertNetworkId);
	m_flags.SetFlag(Flag_IsInsertedAsChild);
}

//////////////////////////////////////////////////////////////////////////

dev_float TIME_TO_STREAM_REACTION_VARIATION_MIN = 1.0f;
dev_float TIME_TO_STREAM_REACTION_VARIATION_MAX = 5.0f;

CReactionClipVariationHelper::CReactionClipVariationHelper()
: m_reactionVariationRequestTimer(TIME_TO_STREAM_REACTION_VARIATION_MIN,TIME_TO_STREAM_REACTION_VARIATION_MAX)
{

}

CReactionClipVariationHelper::~CReactionClipVariationHelper()
{

}

void CReactionClipVariationHelper::Update(CPed* pPed)
{
	aiAssert(pPed);
	switch(m_reactionVariationRequestHelper.GetState())
	{
		case CAmbientClipRequestHelper::AS_noGroupSelected:
			// Intermittently try to select a variation clipSetId
			if( m_reactionVariationRequestTimer.Tick() )
			{
				m_reactionVariationRequestHelper.ChooseRandomConditionalClipSet(pPed, CConditionalAnims::AT_VARIATION,"REACTION_VARIATIONS");
				m_reactionVariationRequestTimer.Reset();
			}
			break;
		case CAmbientClipRequestHelper::AS_groupSelected:
			m_reactionVariationRequestHelper.RequestClips();
			break;
		case CAmbientClipRequestHelper::AS_groupStreamed:
			break;
	}
}

s32 CReactionClipVariationHelper::GetSelectedDictionaryIndex() const
{
	return 0;
	//return CLIP_SET_R_GUARDR_HIGH_1;//m_reactionVariationRequestHelper.GetClipGroupId();
}

void CReactionClipVariationHelper::CleanUp()
{
	m_reactionVariationRequestHelper.ReleaseClips();
	m_reactionVariationRequestTimer.Reset();
}

CPlayAttachedClipHelper::CPlayAttachedClipHelper() 
: m_vInitialOffset(Vector3::ZeroType)
, m_vTargetPosition(Vector3::ZeroType)
, m_qInitialRotationalOffset(0.0f,0.0f,0.0f,1.0f)
, m_qTargetRotation(0.0f,0.0f,0.0f,1.0f)
{
	m_Flags.SetAllFlags();
#if __DEV
	m_Flags.ClearFlag(ACH_TargetSet);
#endif
}

CPlayAttachedClipHelper::~CPlayAttachedClipHelper()
{

}

void CPlayAttachedClipHelper::SetInitialOffsets(const CPed& ped, bool bIgnoreTranslation)
{
#if __DEV
		aiAssertf(m_Flags.IsFlagSet(ACH_TargetSet), "Target has not been set");
#endif

	const Matrix34& mPedMat = RCC_MATRIX34(ped.GetMatrixRef());

	// Get the initial orientation of the ped
	Quaternion qInitialOrientation;
	mPedMat.ToQuaternion(qInitialOrientation);

	// Store the difference offset in orientation (target rotation - initial orientation)
	m_qInitialRotationalOffset.MultiplyInverse(m_qTargetRotation, qInitialOrientation);

	// Get the peds current matrix and store the position and orientation
	if (!bIgnoreTranslation)
	{
		// Store the initial position as an offset from the target position, relative to the target orientation
		m_vInitialOffset = m_vTargetPosition - mPedMat.d;
		m_qTargetRotation.UnTransform(m_vInitialOffset);
	}
}

void CPlayAttachedClipHelper::ComputeIdealStartSituation(const crClip* pClip, Vector3& vIdealStartPosition, Quaternion& qIdealStartOrientation)
{
	aiAssert(pClip);

#if __DEV
	aiAssert(m_Flags.IsFlagSet(ACH_TargetSet));
#endif

	// Get the total backwards translation change
	if(pClip)
	{
		vIdealStartPosition = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, 0.f, 1.f);
	}
	else
	{
		vIdealStartPosition.Zero();
	}
	
	// Compute the ideal starting position by rotating the translational offset relative to the target orientation
	m_qTargetRotation.Transform(vIdealStartPosition);

	// Get the total backwards orientation change
	Quaternion qRotEnd = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 0.f, 1.f);

	// Compute the ideal starting orientation by adding the backwards orientation change to the target rotation
	qIdealStartOrientation = qRotEnd;
	qIdealStartOrientation.Multiply(m_qTargetRotation);

	// and adding the target position
	vIdealStartPosition += m_vTargetPosition;
}

bool CPlayAttachedClipHelper::ComputeIdealEndSituation(CPed* pPed, const Vector3& vInitialPosition, const Quaternion& qInitialOrientation, const crClip* pClip, Vector3& vIdealEndPosition, Quaternion& qIdealEndOrientation, float& fZShift, bool bFindGroundPos, bool bCheckVehicles)
{
	// Get the offset the clipId translates us to
	Vector3 vOffset(Vector3::ZeroType);
	if(pClip)
	{
		vOffset = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, 0.0f, 1.0f);
	}
	
	// Make it relative to the initial orientation	
	qInitialOrientation.Transform(vOffset);

	// Compute position the clipId would leave us in
	vIdealEndPosition = vInitialPosition + vOffset;

	bool bGroundCheckSucceeded = true;

	// If testing for the ground
	if (bFindGroundPos)
	{
		static const float s_fProbeOffset = 0.25f;
		static const float s_fProbeLength = 5.00f;

		Vector3 vGroundPos(Vector3::ZeroType);
		Vector3 vTestPosition = vIdealEndPosition;
		vTestPosition.z += s_fProbeOffset;
		if (FindGroundPos(pPed, vTestPosition, s_fProbeLength, vGroundPos, bCheckVehicles))
		{
			fZShift = vGroundPos.z - vIdealEndPosition.z;
			vIdealEndPosition.z = vGroundPos.z;
			bGroundCheckSucceeded = true;
		}
		else
		{
			bGroundCheckSucceeded = false;
			fZShift = 0.0f;
		}
	}
	else
	{
		fZShift = 0.0f;
	}

	// Get the rotational offset
	qIdealEndOrientation = fwAnimHelpers::GetMoverTrackRotationDiff(*pClip, 0.f, 1.f);

	// Add to the initial orientation to get the final orientation
	qIdealEndOrientation.Multiply(qInitialOrientation);

	return bGroundCheckSucceeded;
}


bool CPlayAttachedClipHelper::FindGroundPos(CPed* pPed, const Vector3& vTestPosition, float fTestDepth, Vector3& vGroundPos, bool bCheckVehicles)
{
	// Search for ground, but tell the probe to ignore the vehicle 
	Vector3 vLosEndPos = vTestPosition - fTestDepth * ZAXIS;

	WorldProbe::CShapeTestProbeDesc probeDesc;
	WorldProbe::CShapeTestFixedResults<> probeResult;
	probeDesc.SetStartAndEnd(vTestPosition, vLosEndPos);
	probeDesc.SetResultsStructure(&probeResult);
	if (bCheckVehicles)
	{
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_VEHICLE_TYPE);
	}
	else
	{
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	}
	
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);

	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
	{
		vGroundPos = probeResult[0].GetHitPosition();
		vGroundPos.z += pPed->GetCapsuleInfo()->GetGroundToRootOffset();
		return true;
	}

	return false;
}

void CPlayAttachedClipHelper::ComputeInitialSituation(Vector3& vInitialPosition, Quaternion& qInitialRotation, bool bIgnoreTranslation)
{
#if __DEV
	aiAssertf(m_Flags.IsFlagSet(ACH_TargetSet), "Target not set");
#endif 

	// Initial orientation is the difference between target orientation and the initial offset from the target orientation
	qInitialRotation = m_qInitialRotationalOffset;
	qInitialRotation.Inverse();
	qInitialRotation.Multiply(m_qTargetRotation);

	if (!bIgnoreTranslation)
	{
		vInitialPosition = m_vInitialOffset;
		// We stored the initial offset relative to the target orientation so transform it back
		m_qTargetRotation.Transform(vInitialPosition);
		vInitialPosition = m_vTargetPosition - vInitialPosition;
	}
}

void  CPlayAttachedClipHelper::ComputeSituationFromClip(const crClip* pClip, float fStartPhase, float fClipPhase, Vector3& vNewPosition, Quaternion& qNewOrientation, float fTimeStep, CPed* pPed, Quaternion* qOrignalOrientation)
{
	// Add the total translation in the clipId for the current phase
	Vector3 vTotalTranslation(Vector3::ZeroType);
	if(pClip)
	{
		vTotalTranslation = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, fStartPhase, fClipPhase);
	}

	if (qOrignalOrientation)
	{
		(*qOrignalOrientation).Transform(vTotalTranslation);	// Rotate by the original orientation to make the translation relative
	}
	else
	{
		qNewOrientation.Transform(vTotalTranslation);
	}
	vNewPosition += vTotalTranslation;

	// Add the total rotation in the clipId for the current phase
	if (!pPed)
	{
		Quaternion qOrientationDelta = fwAnimHelpers::GetMoverTrackRotationDelta(*pClip, fStartPhase, fClipPhase);
		qNewOrientation.Multiply(qOrientationDelta);
	}
	else
	{
		Quaternion qOrientationDelta;		
		// Displayf("Animated Angular : %.4f", pPed->GetAnimatedAngularVelocity());
		qOrientationDelta.FromRotation(ZAXIS, pPed->GetAnimatedAngularVelocity()*fTimeStep);
		qNewOrientation.Multiply(qOrientationDelta);
	}
}

void CPlayAttachedClipHelper::ComputeOffsets(const crClip* pClip, float fClipPhase, float fEndPhase, const Vector3& vNewPosition, const Quaternion& qNewOrientation, Vector3& vOffset, Quaternion& qOffset, Quaternion* qOptionalSeatOrientation, const Vector3* vScale, bool bClipHasRotation)
{
	TUNE_GROUP_BOOL(ATTACHED_CLIP_DEBUG, DISABLE_TRANSLATIONAL_OFFSET, false);
	if (!DISABLE_TRANSLATIONAL_OFFSET && m_Flags.IsFlagSet(ACH_EnableTOffset))
	{
		// Compute the translation left in the clipId
		Vector3 vTotalTranslationLeft(Vector3::ZeroType);
		if(pClip)
		{
			if (bClipHasRotation)
			{
				vTotalTranslationLeft = fwAnimHelpers::GetMoverTrackTranslationDiff(*pClip, fClipPhase, fEndPhase);
				//vTotalTranslationLeft.z=-vTotalTranslationLeft.z;
			}
			else				
				vTotalTranslationLeft = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, fClipPhase, fEndPhase);
		}

		if(vScale)
		{
			vTotalTranslationLeft.Multiply(*vScale);
		}

		// Rotate the translation relative to the initial rotation
		// Rotate the translation relative to the initial rotation				
		if (qOptionalSeatOrientation != NULL)
			qOptionalSeatOrientation->Transform(vTotalTranslationLeft);
		else
			qNewOrientation.Transform(vTotalTranslationLeft);
		
		if (bClipHasRotation)
		{
			vTotalTranslationLeft = m_vTargetPosition - vTotalTranslationLeft;
			vOffset = vTotalTranslationLeft - vNewPosition;
		}
		else
		{
			vTotalTranslationLeft += vNewPosition;
			// Compute the difference to the target position, this is the total translational fixup we need to apply
			vOffset = m_vTargetPosition - vTotalTranslationLeft;
		}
	}

	TUNE_GROUP_BOOL(ATTACHED_CLIP_DEBUG, DISABLE_ROTATIONAL_OFFSET, false);
	if (!DISABLE_ROTATIONAL_OFFSET && m_Flags.IsFlagSet(ACH_EnableROffset))
	{
		// Compute remaining rotation left in the clipId
		Quaternion qTotalRotationLeft = fwAnimHelpers::GetMoverTrackRotationDelta(*pClip, fClipPhase, fEndPhase);

		// Find the orientation we would end up in without any fixup
		qTotalRotationLeft.Multiply(qNewOrientation);		

		// Compute the difference to the target orientation, this is the total rotational fixup we need to apply
		qOffset = qTotalRotationLeft;
		qOffset.Inverse();
		qOffset.Multiply(m_qTargetRotation);
	}
}

float  CPlayAttachedClipHelper::ComputeFixUpThisFrame(float fTotalFixUp, float fClipPhase, float fStartPhase, float fEndPhase)
{
	float fExtraFixUp = 0.0f;

	// Do this only after the initial translation fixup has been applied
	if (fClipPhase > fStartPhase && fClipPhase < fEndPhase)
	{
		// Cut off the start phase
		const float fTempClipPhase = fClipPhase - fStartPhase;
		const float fTempFixUpRange = fEndPhase - fStartPhase;

		const float fExtraRatio = MIN(fTempClipPhase / fTempFixUpRange,1.0f);
		fExtraFixUp = (fExtraRatio * fTotalFixUp);
	}
	else if (fClipPhase >= fEndPhase)
	{
		fExtraFixUp = fTotalFixUp;
	}
	// else Do Nothing

	return fExtraFixUp;
}

Quaternion CPlayAttachedClipHelper::ComputeRotationalFixUpThisFrame(const Quaternion& qTotalRotationalFixUp, float fClipPhase, float fStartPhase, float fEndPhase)
{
	Quaternion qExtraRotation(0.0f, 0.0f, 0.0f, 1.0f);

	if (fClipPhase > fStartPhase && fClipPhase < fEndPhase)
	{			
		// Cut off the start phase
		const float fTempClipPhase = fClipPhase - fStartPhase;
		const float fTempFixUpRange = fEndPhase - fStartPhase;
		const float fExtraRatio = MIN(fTempClipPhase / fTempFixUpRange ,1.0f);

		qExtraRotation.SlerpNear(fExtraRatio, qTotalRotationalFixUp);	// Slerp to the target rotation
	}
	else if (fClipPhase >= fEndPhase)
	{
		qExtraRotation = qTotalRotationalFixUp;
	}
	// else Do Nothing

	return qExtraRotation;
}

void CPlayAttachedClipHelper::ApplyOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase)
{
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_X_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_X_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_Y_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_Y_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_Z_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_Z_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_ROT_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_ROT_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);

	float fXFixUpStartPhase = DEFAULT_X_FIX_UP_START_PHASE;
	float fXFixUpEndPhase = DEFAULT_X_FIX_UP_END_PHASE;
	float fYFixUpStartPhase = DEFAULT_Y_FIX_UP_START_PHASE;
	float fYFixUpEndPhase = DEFAULT_Y_FIX_UP_END_PHASE;
	float fZFixUpStartPhase = DEFAULT_Z_FIX_UP_START_PHASE;
	float fZFixUpEndPhase = DEFAULT_Z_FIX_UP_END_PHASE;
	float fRotFixUpStartPhase = DEFAULT_ROT_FIX_UP_START_PHASE;
	float fRotFixUpEndPhase = DEFAULT_ROT_FIX_UP_END_PHASE;

	if (pClip && pClip->GetTags())
	{
		CClipEventTags::CTagIterator<CClipEventTags::CMoverFixupEventTag> it (*pClip->GetTags(), CClipEventTags::MoverFixup);
	
		while (*it)
		{
			const CClipEventTags::CMoverFixupEventTag* pTag = (*it)->GetData();
			if (pTag->ShouldFixupTranslation())
			{
				if (pTag->ShouldFixupX())
				{
					fXFixUpStartPhase = (*it)->GetStart();
					fXFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupY())
				{
					fYFixUpStartPhase = (*it)->GetStart();
					fYFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupZ())
				{
					fZFixUpStartPhase = (*it)->GetStart();
					fZFixUpEndPhase = (*it)->GetEnd();
				}
			}
			
			if (pTag->ShouldFixupRotation())
			{
				fRotFixUpStartPhase = (*it)->GetStart();
				fRotFixUpEndPhase = (*it)->GetEnd();
			}
			++it;
		}
	}

	// Apply x fix up
	vCurrentPos.x += ComputeFixUpThisFrame(vOffset.x, fClipPhase, fXFixUpStartPhase, fXFixUpEndPhase);

	// Apply y fix up
	vCurrentPos.y += ComputeFixUpThisFrame(vOffset.y, fClipPhase, fYFixUpStartPhase, fYFixUpEndPhase);

	// Apply z fix up
	vCurrentPos.z += ComputeFixUpThisFrame(vOffset.z, fClipPhase, fZFixUpStartPhase, fZFixUpEndPhase);

	// Apply rotational fix up
	qCurrentOrientation.Multiply(ComputeRotationalFixUpThisFrame(qOffset, fClipPhase, fRotFixUpStartPhase, fRotFixUpEndPhase));
}

void CPlayAttachedClipHelper::ApplyMountOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase)
{
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_X_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_X_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_Y_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_Y_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_Z_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_Z_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_ROT_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, DEFAULT_MOUNT_ROT_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);

	float fXFixUpStartPhase = DEFAULT_MOUNT_X_FIX_UP_START_PHASE;
	float fXFixUpEndPhase = DEFAULT_MOUNT_X_FIX_UP_END_PHASE;
	float fYFixUpStartPhase = DEFAULT_MOUNT_Y_FIX_UP_START_PHASE;
	float fYFixUpEndPhase = DEFAULT_MOUNT_Y_FIX_UP_END_PHASE;
	float fZFixUpStartPhase = DEFAULT_MOUNT_Z_FIX_UP_START_PHASE;
	float fZFixUpEndPhase = DEFAULT_MOUNT_Z_FIX_UP_END_PHASE;
	float fRotFixUpStartPhase = DEFAULT_MOUNT_ROT_FIX_UP_START_PHASE;
	float fRotFixUpEndPhase = DEFAULT_MOUNT_ROT_FIX_UP_END_PHASE;

	if (pClip && pClip->GetTags())
	{
		CClipEventTags::CTagIterator<CClipEventTags::CMoverFixupEventTag> it (*pClip->GetTags(), CClipEventTags::MoverFixup);

		while (*it)
		{
			const CClipEventTags::CMoverFixupEventTag* pTag = (*it)->GetData();
			if (pTag->ShouldFixupTranslation())
			{
				if (pTag->ShouldFixupX())
				{
					fXFixUpStartPhase = (*it)->GetStart();
					fXFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupY())
				{
					fYFixUpStartPhase = (*it)->GetStart();
					fYFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupZ())
				{
					fZFixUpStartPhase = (*it)->GetStart();
					fZFixUpEndPhase = (*it)->GetEnd();
				}
			}

			if (pTag->ShouldFixupRotation())
			{
				fRotFixUpStartPhase = (*it)->GetStart();
				fRotFixUpEndPhase = (*it)->GetEnd();
			}
			++it;
		}
	}

	// Apply x fix up
	vCurrentPos.x += ComputeFixUpThisFrame(vOffset.x, fClipPhase, fXFixUpStartPhase, fXFixUpEndPhase);

	// Apply y fix up
	vCurrentPos.y += ComputeFixUpThisFrame(vOffset.y, fClipPhase, fYFixUpStartPhase, fYFixUpEndPhase);

	// Apply z fix up
	vCurrentPos.z += ComputeFixUpThisFrame(vOffset.z, fClipPhase, fZFixUpStartPhase, fZFixUpEndPhase);

	// Apply rotational fix up
	qCurrentOrientation.Multiply(ComputeRotationalFixUpThisFrame(qOffset, fClipPhase, fRotFixUpStartPhase, fRotFixUpEndPhase));
}

void CPlayAttachedClipHelper::ApplyBikePickPullEntryOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase)
{
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_X_FIX_UP_START_PHASE, 0.85f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_X_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_Y_FIX_UP_START_PHASE, 0.85f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_Y_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_Z_FIX_UP_START_PHASE, 0.9f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_Z_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_ROT_FIX_UP_START_PHASE, 0.9f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_DEFAULT_ROT_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);

	float fXFixUpStartPhase = VEH_BIKE_DEFAULT_X_FIX_UP_START_PHASE;
	float fXFixUpEndPhase = VEH_BIKE_DEFAULT_X_FIX_UP_END_PHASE;
	float fYFixUpStartPhase = VEH_BIKE_DEFAULT_Y_FIX_UP_START_PHASE;
	float fYFixUpEndPhase = VEH_BIKE_DEFAULT_Y_FIX_UP_END_PHASE;
	float fZFixUpStartPhase = VEH_BIKE_DEFAULT_Z_FIX_UP_START_PHASE;
	float fZFixUpEndPhase = VEH_BIKE_DEFAULT_Z_FIX_UP_END_PHASE;
	float fRotFixUpStartPhase = VEH_BIKE_DEFAULT_ROT_FIX_UP_START_PHASE;
	float fRotFixUpEndPhase = VEH_BIKE_DEFAULT_ROT_FIX_UP_END_PHASE;

	if (pClip && pClip->GetTags())
	{
		CClipEventTags::CTagIterator<CClipEventTags::CMoverFixupEventTag> it (*pClip->GetTags(), CClipEventTags::MoverFixup);

		while (*it)
		{
			const CClipEventTags::CMoverFixupEventTag* pTag = (*it)->GetData();
			if (pTag->ShouldFixupTranslation())
			{
				if (pTag->ShouldFixupX())
				{
					fXFixUpStartPhase = (*it)->GetStart();
					fXFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupY())
				{
					fYFixUpStartPhase = (*it)->GetStart();
					fYFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupZ())
				{
					fZFixUpStartPhase = (*it)->GetStart();
					fZFixUpEndPhase = (*it)->GetEnd();
				}
			}

			if (pTag->ShouldFixupRotation())
			{
				fRotFixUpStartPhase = (*it)->GetStart();
				fRotFixUpEndPhase = (*it)->GetEnd();
			}
			++it;
		}
	}

	// Apply x fix up
	vCurrentPos.x += ComputeFixUpThisFrame(vOffset.x, fClipPhase, fXFixUpStartPhase, fXFixUpEndPhase);

	// Apply y fix up
	vCurrentPos.y += ComputeFixUpThisFrame(vOffset.y, fClipPhase, fYFixUpStartPhase, fYFixUpEndPhase);

	// Apply z fix up
	vCurrentPos.z += ComputeFixUpThisFrame(vOffset.z, fClipPhase, fZFixUpStartPhase, fZFixUpEndPhase);

	// Apply rotational fix up
	qCurrentOrientation.Multiply(ComputeRotationalFixUpThisFrame(qOffset, fClipPhase, fRotFixUpStartPhase, fRotFixUpEndPhase));
}


void CPlayAttachedClipHelper::ApplyVehicleEntryOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase, bool bApplyIfTagsNotFound, bool bGettingOnDirectWhenOnSide, const CVehicle* pVehicle)
	{
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_X_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_X_FIX_UP_END_PHASE, 0.7f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_Y_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_Y_FIX_UP_END_PHASE, 0.7f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_Z_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_Z_FIX_UP_END_PHASE, 0.3f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_ROT_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_DEFAULT_ROT_FIX_UP_END_PHASE, 0.7f, 0.0f, 1.0f, 0.01f);

	CTaskVehicleFSM::sMoverFixupInfo moverFixupInfo;
	moverFixupInfo.fXFixUpStartPhase = VEH_DEFAULT_X_FIX_UP_START_PHASE;
	moverFixupInfo.fXFixUpEndPhase = VEH_DEFAULT_X_FIX_UP_END_PHASE;
	moverFixupInfo.fYFixUpStartPhase = VEH_DEFAULT_Y_FIX_UP_START_PHASE;
	moverFixupInfo.fYFixUpEndPhase = VEH_DEFAULT_Y_FIX_UP_END_PHASE;
	moverFixupInfo.fZFixUpStartPhase = VEH_DEFAULT_Z_FIX_UP_START_PHASE;
	moverFixupInfo.fZFixUpEndPhase = VEH_DEFAULT_Z_FIX_UP_END_PHASE;
	moverFixupInfo.fRotFixUpStartPhase = VEH_DEFAULT_ROT_FIX_UP_START_PHASE;
	moverFixupInfo.fRotFixUpEndPhase = VEH_DEFAULT_ROT_FIX_UP_END_PHASE;
	moverFixupInfo.bFoundXFixup = false;
	moverFixupInfo.bFoundYFixup = false;
	moverFixupInfo.bFoundZFixup = false;
	moverFixupInfo.bFoundRotFixup = false;

	CTaskVehicleFSM::GetMoverFixupInfoFromClip(moverFixupInfo, pClip, pVehicle ? pVehicle->GetVehicleModelInfo()->GetModelNameHash() : 0);

	if (bGettingOnDirectWhenOnSide)
	{
		TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_ON_SIDE_X_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_ON_SIDE_X_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_ON_SIDE_Y_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_ON_SIDE_Y_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_ON_SIDE_Z_FIX_UP_START_PHASE, 0.1f, 0.0f, 1.0f, 0.01f);
		TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_BIKE_ON_SIDE_Z_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
		moverFixupInfo.fXFixUpStartPhase = VEH_BIKE_ON_SIDE_X_FIX_UP_START_PHASE;
		moverFixupInfo.fXFixUpEndPhase = VEH_BIKE_ON_SIDE_X_FIX_UP_END_PHASE;
		moverFixupInfo.fYFixUpStartPhase = VEH_BIKE_ON_SIDE_Y_FIX_UP_START_PHASE;
		moverFixupInfo.fYFixUpEndPhase = VEH_BIKE_ON_SIDE_Y_FIX_UP_END_PHASE;
		moverFixupInfo.fZFixUpStartPhase = VEH_BIKE_ON_SIDE_Z_FIX_UP_START_PHASE;
		moverFixupInfo.fZFixUpEndPhase = VEH_BIKE_ON_SIDE_Z_FIX_UP_END_PHASE;
	}

	// Apply x fix up
	if (bApplyIfTagsNotFound || moverFixupInfo.bFoundXFixup)
	{
		vCurrentPos.x += ComputeFixUpThisFrame(vOffset.x, fClipPhase, moverFixupInfo.fXFixUpStartPhase, moverFixupInfo.fXFixUpEndPhase);
	}

	// Apply y fix up
	if (bApplyIfTagsNotFound || moverFixupInfo.bFoundYFixup)
	{
		vCurrentPos.y += ComputeFixUpThisFrame(vOffset.y, fClipPhase, moverFixupInfo.fYFixUpStartPhase, moverFixupInfo.fYFixUpEndPhase);
	}

	// Apply z fix up
	if (bApplyIfTagsNotFound || moverFixupInfo.bFoundZFixup)
	{
		vCurrentPos.z += ComputeFixUpThisFrame(vOffset.z, fClipPhase, moverFixupInfo.fZFixUpStartPhase, moverFixupInfo.fZFixUpEndPhase);
	}

	// Apply rotational fix up
	if (bApplyIfTagsNotFound || moverFixupInfo.bFoundRotFixup)
	{
		qCurrentOrientation.Multiply(ComputeRotationalFixUpThisFrame(qOffset, fClipPhase, moverFixupInfo.fRotFixUpStartPhase, moverFixupInfo.fRotFixUpEndPhase));
	}
}

void CPlayAttachedClipHelper::ApplyVehicleShuffleOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase)
	{
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_X_FIX_UP_START_PHASE, 0.4f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_X_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_Y_FIX_UP_START_PHASE, 0.4f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_Y_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_Z_FIX_UP_START_PHASE, 0.4f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_Z_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_ROT_FIX_UP_START_PHASE, 0.4f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_SHUFFLE_DEFAULT_ROT_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);

	float fXFixUpStartPhase = VEH_SHUFFLE_DEFAULT_X_FIX_UP_START_PHASE;
	float fXFixUpEndPhase = VEH_SHUFFLE_DEFAULT_X_FIX_UP_END_PHASE;
	float fYFixUpStartPhase = VEH_SHUFFLE_DEFAULT_Y_FIX_UP_START_PHASE;
	float fYFixUpEndPhase = VEH_SHUFFLE_DEFAULT_Y_FIX_UP_END_PHASE;
	float fZFixUpStartPhase = VEH_SHUFFLE_DEFAULT_Z_FIX_UP_START_PHASE;
	float fZFixUpEndPhase = VEH_SHUFFLE_DEFAULT_Z_FIX_UP_END_PHASE;
	float fRotFixUpStartPhase = VEH_SHUFFLE_DEFAULT_ROT_FIX_UP_START_PHASE;
	float fRotFixUpEndPhase = VEH_SHUFFLE_DEFAULT_ROT_FIX_UP_END_PHASE;

	if (pClip && pClip->GetTags())
{
		CClipEventTags::CTagIterator<CClipEventTags::CMoverFixupEventTag> it (*pClip->GetTags(), CClipEventTags::MoverFixup);

		while (*it)
		{
			const CClipEventTags::CMoverFixupEventTag* pTag = (*it)->GetData();
			if (pTag->ShouldFixupTranslation())
			{
				if (pTag->ShouldFixupX())
				{
					fXFixUpStartPhase = (*it)->GetStart();
					fXFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupY())
				{
					fYFixUpStartPhase = (*it)->GetStart();
					fYFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupZ())
				{
					fZFixUpStartPhase = (*it)->GetStart();
					fZFixUpEndPhase = (*it)->GetEnd();
				}
			}
			
			if (pTag->ShouldFixupRotation())
			{
				fRotFixUpStartPhase = (*it)->GetStart();
				fRotFixUpEndPhase = (*it)->GetEnd();
			}
			++it;
		}
	}

	// Apply x fix up
	vCurrentPos.x += ComputeFixUpThisFrame(vOffset.x, fClipPhase, fXFixUpStartPhase, fXFixUpEndPhase);

	// Apply y fix up
	vCurrentPos.y += ComputeFixUpThisFrame(vOffset.y, fClipPhase, fYFixUpStartPhase, fYFixUpEndPhase);

	// Apply z fix up
	vCurrentPos.z += ComputeFixUpThisFrame(vOffset.z, fClipPhase, fZFixUpStartPhase, fZFixUpEndPhase);

	// Apply rotational fix up
	qCurrentOrientation.Multiply(ComputeRotationalFixUpThisFrame(qOffset, fClipPhase, fRotFixUpStartPhase, fRotFixUpEndPhase));
	}

void CPlayAttachedClipHelper::ApplyVehicleExitOffsets(Vector3& vCurrentPos, Quaternion& qCurrentOrientation, const crClip* pClip, const Vector3& vOffset, const Quaternion& qOffset, float fClipPhase)
	{
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_X_FIX_UP_START_PHASE, 0.6f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_X_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_Y_FIX_UP_START_PHASE, 0.6f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_Y_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_Z_FIX_UP_START_PHASE, 0.6f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_Z_FIX_UP_END_PHASE, 1.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_ROT_FIX_UP_START_PHASE, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(ATTACHED_CLIP_DEBUG, VEH_EXIT_ROT_FIX_UP_END_PHASE, 0.2f, 0.0f, 1.0f, 0.01f);
	float fXFixUpStartPhase = VEH_EXIT_X_FIX_UP_START_PHASE;
	float fXFixUpEndPhase = VEH_EXIT_X_FIX_UP_END_PHASE;
	float fYFixUpStartPhase = VEH_EXIT_Y_FIX_UP_START_PHASE;
	float fYFixUpEndPhase = VEH_EXIT_Y_FIX_UP_END_PHASE;
	float fZFixUpStartPhase = VEH_EXIT_Z_FIX_UP_START_PHASE;
	float fZFixUpEndPhase = VEH_EXIT_Z_FIX_UP_END_PHASE;
	float fRotFixUpStartPhase = VEH_EXIT_ROT_FIX_UP_START_PHASE;
	float fRotFixUpEndPhase = VEH_EXIT_ROT_FIX_UP_END_PHASE;

	if (pClip && pClip->GetTags())
{
		CClipEventTags::CTagIterator<CClipEventTags::CMoverFixupEventTag> it (*pClip->GetTags(), CClipEventTags::MoverFixup);

		while (*it)
		{
			const CClipEventTags::CMoverFixupEventTag* pTag = (*it)->GetData();
			if (pTag->ShouldFixupTranslation())
			{
				if (pTag->ShouldFixupX())
				{
					fXFixUpStartPhase = (*it)->GetStart();
					fXFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupY())
				{
					fYFixUpStartPhase = (*it)->GetStart();
					fYFixUpEndPhase = (*it)->GetEnd();
				}
				if (pTag->ShouldFixupZ())
				{
					fZFixUpStartPhase = (*it)->GetStart();
					fZFixUpEndPhase = (*it)->GetEnd();
				}
			}
			
			if (pTag->ShouldFixupRotation())
			{
				fRotFixUpStartPhase = (*it)->GetStart();
				fRotFixUpEndPhase = (*it)->GetEnd();
			}
			++it;
		}
	}

	// Apply x fix up
	vCurrentPos.x += ComputeFixUpThisFrame(vOffset.x, fClipPhase, fXFixUpStartPhase, fXFixUpEndPhase);

	// Apply y fix up
	vCurrentPos.y += ComputeFixUpThisFrame(vOffset.y, fClipPhase, fYFixUpStartPhase, fYFixUpEndPhase);

	// Apply z fix up
	vCurrentPos.z += ComputeFixUpThisFrame(vOffset.z, fClipPhase, fZFixUpStartPhase, fZFixUpEndPhase);

	// Apply rotational fix up
	qCurrentOrientation.Multiply(ComputeRotationalFixUpThisFrame(qOffset, fClipPhase, fRotFixUpStartPhase, fRotFixUpEndPhase));
}

bool CPlayAttachedClipHelper::FindFixUpEventPhase(int UNUSED_PARAM(iEventToFind), float& UNUSED_PARAM(fEventPhase), const crClip* UNUSED_PARAM(pClip), const float UNUSED_PARAM(fStartPhase) /* = 0.0f */, const float UNUSED_PARAM(fStopPhase) /* = 1.0f */) const
{
/*
	taskAssert(fStartPhase>=0.0f);
	taskAssert(fStopPhase<=1.0f);
	taskAssert(fStartPhase<=fStopPhase);

	if( taskVerifyf(pClip, "pClip is NULL") )
	{
		if( pClip->HasTags() )
		{
			// Loop through the tags searching for the given event
			crTagIterator it(*pClip->GetTags(), fStartPhase, fStopPhase, crTag::CalcKey("GenericInt"));
			while(*it)
			{
				const crTag* pTag = *it;
				const crPropertyAttribute* attribSubType = pTag->GetProperty().GetAttribute("SubType");
				if(attribSubType && attribSubType->GetType() == crPropertyAttribute::kTypeInt && static_cast<const crPropertyAttributeInt*>(attribSubType)->GetInt() == AT_FIX_UP_EVENTS)
				{
					const crPropertyAttribute* attrib = pTag->GetProperty().GetAttribute("Int");
					if(attrib && attrib->GetType() == crPropertyAttribute::kTypeInt && (static_cast<const crPropertyAttributeInt*>(attrib)->GetInt() & iEventToFind))
					{
						fEventPhase = pTag->GetMid();
					}
				}
				++it;
			}
		}
	}
*/
	return false;
}

////////////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CVehicleClipRequestHelper, CONFIGURED_FROM_FILE, 0.68f, atHashString("CVehicleClipRequestHelper",0xff74e9fd));

// Statics
CVehicleClipRequestHelper::Tunables CVehicleClipRequestHelper::ms_Tunables;

IMPLEMENT_VEHICLE_TASK_TUNABLES(CVehicleClipRequestHelper, 0xff74e9fd);

////////////////////////////////////////////////////////////////////////////////

CVehicleClipRequestHelper::CVehicleClipRequestHelper()
: m_pPed(NULL)
, m_iNumClipSetsRequested(0)
, m_fTimeSinceLastUpdate(0.0f)
, m_iNumVehiclesConsidered(0)
, m_pConditionalClipSet(NULL)
, m_pTargetVehicle(NULL)
, m_iTargetEntryPointIndex(-1)
, m_fScanDistance(ms_Tunables.m_MinDistanceToScanForNearbyVehicle)
, m_fUpdateFrequency(ms_Tunables.m_MinDistUpdateFrequency)
, m_fScanArc(ms_Tunables.m_MinDistScanArc)
{
	ClearAllRequests();

#if __BANK
	for (s32 i=0; i<MAX_NUM_VEHICLES_TO_CONSIDER; i++)
	{
		m_apVehiclesConsidered[i] = NULL;
	}
#endif
}

////////////////////////////////////////////////////////////////////////////////

CVehicleClipRequestHelper::~CVehicleClipRequestHelper()
{
	ClearAllRequests();
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleClipRequestHelper::Update(float fTimeStep)
{
	if (!m_pPed || ms_Tunables.m_DisableStreamedVehicleAnimRequestHelper)
	{
		return;
	}

	const float fMbr = m_pPed->IsStrafing() ? 0.5f : m_pPed->GetMotionData()->GetCurrentMbrY();
	const float fMbrNorm = rage::Clamp(fMbr * ONE_OVER_MOVEBLENDRATIO_SPRINT, 0.0f, 1.0f);
	
	m_fScanDistance = rage::Lerp(fMbrNorm, ms_Tunables.m_MinDistanceToScanForNearbyVehicle, ms_Tunables.m_MaxDistanceToScanForNearbyVehicle);
	m_fUpdateFrequency = rage::Lerp(fMbrNorm, ms_Tunables.m_MinDistUpdateFrequency, ms_Tunables.m_MaxDistUpdateFrequency);

	if (m_fScanDistance >= ms_Tunables.m_MinDistPercentageToScaleScanArc)
	{
		m_fScanArc = rage::Lerp(fMbrNorm, ms_Tunables.m_MinDistScanArc, ms_Tunables.m_MaxDistScanArc);
	}
	else
	{
		m_fScanArc = ms_Tunables.m_MaxDistScanArc;
	}

	m_fTimeSinceLastUpdate -= fTimeStep;

	// Only check periodically
	if (m_fTimeSinceLastUpdate <= 0.0f)
	{
		m_fTimeSinceLastUpdate = m_fUpdateFrequency;

		const CVehicle* pTargetVehicle = StreamAnimsForNearbyVehiclesAndFindTargetVehicle();

		if (pTargetVehicle)
		{
			m_pTargetVehicle = pTargetVehicle;

			// Request the direct access entry variations for the target seat 
			if (ms_Tunables.m_EnableStreamedEntryVariationAnims)
			{
				s32 iTargetEntryPointIndex = FindTargetEntryPointIndex();

				// Try to choose a new streamed variation
				UpdateStreamedVariations(pTargetVehicle, iTargetEntryPointIndex);

				// Request the anims for the streamed variation
				RequestEntryVariationClipSet(m_pConditionalClipSet);

				// B*2023900: stream in new generic FPS helmet anims if vehicle is an aircraft
				if (m_pPed->IsLocalPlayer() && m_pTargetVehicle->GetIsAircraft())
				{
					const fwMvClipSetId sClipSetId = CPedHelmetComponent::GetAircraftPutOnTakeOffHelmetFPSClipSet(m_pTargetVehicle);
					RequestClipSet(sClipSetId);
				}

				const bool bCNCForceMichaelVehicleEntryAnims = CTaskEnterVehicle::CNC_ShouldForceMichaelVehicleEntryAnims(*m_pPed);

				s32 iPlayerIndex = -1;
				if (!NetworkInterface::IsGameInProgress())
				{
					if (m_pPed->IsLocalPlayer())
					{
						iPlayerIndex = CPlayerInfo::GetPlayerIndex(*m_pPed);
					}
				}
				else if (m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_UseLockpickVehicleEntryAnimations))
				{
					iPlayerIndex = 1;
				}
				else if (bCNCForceMichaelVehicleEntryAnims)
				{
					// CNC: Stream in Michael's jack anims for cops to use (used via CTaskEnterVehicle::GetAlternateClipInfoForPed).
					iPlayerIndex = 0;
				}

				if (iPlayerIndex > -1 && m_pTargetVehicle->IsEntryIndexValid(iTargetEntryPointIndex))
				{
					const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = m_pTargetVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(iTargetEntryPointIndex);
					if (pEntryPointClipInfo)
					{
						if (m_pTargetVehicle && m_pTargetVehicle->GetCarDoorLocks() == CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED && !bCNCForceMichaelVehicleEntryAnims)
						{
							fwMvClipSetId forcedBreakInClipSet = pEntryPointClipInfo->GetCustomPlayerBreakInClipSet(iPlayerIndex);
							if (forcedBreakInClipSet != CLIP_SET_ID_INVALID)
							{
								RequestClipSet(forcedBreakInClipSet);
							}
						}
						else if (!NetworkInterface::IsGameInProgress() || bCNCForceMichaelVehicleEntryAnims)
						{
							s32 iSeatIndex = m_pTargetVehicle->GetEntryExitPoint(iTargetEntryPointIndex)->GetSeat(SA_directAccessSeat);
							CPed* pSeatOccupier = m_pTargetVehicle->GetSeatManager()->GetPedInSeat(iSeatIndex);
							if (pSeatOccupier && !pSeatOccupier->IsDead() && pSeatOccupier != m_pPed)
							{
								fwMvClipSetId jackAlivePedClipSet = pEntryPointClipInfo->GetCustomPlayerJackingClipSet(iPlayerIndex);
								if (jackAlivePedClipSet != CLIP_SET_ID_INVALID)
								{
									RequestClipSet(jackAlivePedClipSet);
								}
							}
						}
					}
				}
			}
		}
	}
	else
	{
		for (s32 i=0; i<m_iNumClipSetsRequested; ++i)
		{
			CTaskVehicleFSM::RequestVehicleClipSet(m_aClipSetIdsRequested[i], m_pPed->IsPlayer() ? SP_High : SP_Medium);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

const CVehicle* CVehicleClipRequestHelper::StreamAnimsForNearbyVehiclesAndFindTargetVehicle()
{
	const CVehicle* pTargetVehicle = NULL;

	// Reset the number of vehicles being considered and clear the anim requests out
	m_iNumVehiclesConsidered = 0;
	m_iTargetEntryPointIndex = -1;
	ClearAllRequests();

	// If we're in a vehicle already we'll use this vehicle to try to stream exit variations
	if (m_pPed->GetIsInVehicle())
	{
		pTargetVehicle = m_pPed->GetMyVehicle();
#if __BANK
		m_apVehiclesConsidered[m_iNumVehiclesConsidered] = pTargetVehicle;
#endif
		++m_iNumVehiclesConsidered;


		s32 iFlags = 0;

		// Keep the entry anims streamed in for players as they are unpredictable
		if (m_pPed->IsPlayer())
		{
			iFlags |= AF_StreamExitClips; // The InVehicle task already requests the seat anims AF_StreamSeatClips;
			iFlags |= AF_StreamEntryClips;
		}
		else 
		{
			TUNE_GROUP_FLOAT(VEHICLE_STREAMING, MAX_DISTANCE_ON_SCREEN_TO_PRESTREAM_ENTRY_ANIMS_SQD, 400.0f, 0.0f, 2000.0f, 0.1f);
			TUNE_GROUP_FLOAT(VEHICLE_STREAMING, MAX_DISTANCE_OFF_SCREEN_TO_PRESTREAM_ENTRY_ANIMS_SQD, 100.0f, 0.0f, 2000.0f, 0.1f);
			const bool bOnScreen = m_pPed->GetIsOnScreen();
			const float fMaxDistToCamSqd = bOnScreen ? MAX_DISTANCE_ON_SCREEN_TO_PRESTREAM_ENTRY_ANIMS_SQD : MAX_DISTANCE_OFF_SCREEN_TO_PRESTREAM_ENTRY_ANIMS_SQD;
			const Vector3 vCamPosition = camInterface::GetGameplayDirector().GetFrame().GetPosition();
			const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
			if (vCamPosition.Dist2(vPedPosition) < fMaxDistToCamSqd)
			{
				iFlags |= AF_StreamExitClips;
			}
		}

		// Should we stream the connected seat's anims too?
		if (ms_Tunables.m_StreamConnectedSeatAnims)
		{
			iFlags |= AF_StreamConnectedSeatAnims;
		}

		s32 iSeatIndex = pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(m_pPed);
		RequestClipsFromVehicle(pTargetVehicle, iFlags, iSeatIndex); // Request clips for target seat
		return pTargetVehicle;
	}

	if (!pTargetVehicle)
	{
		const ScalarV vMaxScanDistance = ScalarVFromF32(m_fScanDistance*m_fScanDistance);

		m_pTargetVehicle = NULL;
		ScalarV vClosestDistSqr = ScalarVFromF32(LARGE_FLOAT);
		ScalarV vAlwaysConsiderDistSqr = ScalarVFromF32(rage::square((1.0f - ms_Tunables.m_MinDistPercentageToScaleScanArc) * ms_Tunables.m_MinDistanceToScanForNearbyVehicle + ms_Tunables.m_MaxDistanceToScanForNearbyVehicle * ms_Tunables.m_MinDistPercentageToScaleScanArc));

		const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(m_pPed->GetDesiredHeading());
		const Vec3V vPedDesiredDirection = RotateAboutZAxis(Vec3V(V_Y_AXIS_WONE), ScalarVFromF32(fDesiredHeading));
		const Vec3V vPedPos = m_pPed->GetTransform().GetPosition();

		const CEntityScannerIterator nearbyVehicles = m_pPed->GetPedIntelligence()->GetNearbyVehicles();
		for(const CEntity* pEnt = nearbyVehicles.GetFirst(); pEnt; pEnt = nearbyVehicles.GetNext())
		{
			const CVehicle * pVehicle = static_cast<const CVehicle*>(pEnt);
			if (pVehicle)
			{
				const Vec3V vVehPos = pVehicle->GetTransform().GetPosition();
				ScalarV vDistSqr = DistSquared(vVehPos, vPedPos);

				// Reject vehicles outside maximum scan range
				if (IsGreaterThanOrEqualAll(vDistSqr, vMaxScanDistance))
				{
#if __DEV
					if (CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eVehicleEntryDebug)
					{
						grcDebugDraw::Sphere(vVehPos + Vec3V(0.0f, 0.0f, 1.0f), 0.25f, Color_red, true, 5);
					}
#endif // __DEV
					continue;
				}

				// Try to reject vehicles outside minimum scan range and outside our scan arc
				if (IsGreaterThanOrEqualAll(vDistSqr, vAlwaysConsiderDistSqr))
				{
					const Vec3V vToVehicle = Normalize(vVehPos - vPedPos);
					if (IsLessThanAll(Dot(vPedDesiredDirection, vToVehicle), ScalarVFromF32(m_fScanArc)))
					{
#if __DEV
						if (CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eVehicleEntryDebug)
						{
							grcDebugDraw::Sphere(vVehPos + Vec3V(0.0f, 0.0f, 1.0f), 0.25f, Color_orange, true, 5);
						}
#endif // __DEV
						continue;
					}
				}

#if __DEV
				if (CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eVehicleEntryDebug)
				{
					grcDebugDraw::Sphere(vVehPos + Vec3V(0.0f, 0.0f, 1.0f), 0.25f, Color_green, true, 5);
				}
#endif // __DEV
				if (IsLessThanAll(vDistSqr, vClosestDistSqr))
				{
					vClosestDistSqr = vDistSqr;
					pTargetVehicle = pVehicle;
				}
#if __BANK
				m_apVehiclesConsidered[m_iNumVehiclesConsidered] = pVehicle;
#endif
				++m_iNumVehiclesConsidered;

				s32 iFlags = AF_StreamEntryClips;

				if (m_pPed->IsLocalPlayer())
				{
					iFlags |= AF_StreamExitClips;
				}

				// Should we stream the direct access seat's anims for this entry point?
				// DMKH. Boats use seat anims for get ins (as all seats are available from every entry point), so we need to 
				// stream them in up front. We can revisit this if necessary.
				if (ms_Tunables.m_StreamInVehicleAndEntryAnimsTogether || pVehicle->InheritsFromBoat())
				{
					iFlags |= AF_StreamSeatClips;
				}

				// Should we stream the connected seat's anims too?
				if (ms_Tunables.m_StreamConnectedSeatAnims)
				{
					iFlags |= AF_StreamConnectedSeatAnims;
				}

				s32 iTargetEntryPointIndex = GetClosestTargetEntryPointIndexForPed(pVehicle, m_pPed, -1, false);
				if (pVehicle->IsEntryIndexValid(iTargetEntryPointIndex))
				{
					const CEntryExitPoint* pEntryExitPoint = pVehicle->GetEntryExitPoint(iTargetEntryPointIndex);
					taskFatalAssertf(pEntryExitPoint, "NULL entry point associated with entrypoint %i", iTargetEntryPointIndex);
					s32 iTargetSeat = pEntryExitPoint->GetSeat(SA_directAccessSeat);
					if (pVehicle->IsSeatIndexValid(iTargetSeat))
					{
						RequestClipsFromVehicle(pVehicle, iFlags, iTargetSeat);	// Request clips for target seat
					}
				}
			}

			// Reached max num vehicles to check
			if (m_iNumVehiclesConsidered >= MAX_NUM_VEHICLES_TO_CONSIDER)
			{
				break;
			}
		}
	}

	return pTargetVehicle;
}

////////////////////////////////////////////////////////////////////////////////

s32 CVehicleClipRequestHelper::FindTargetEntryPointIndex() const
{
	// If we're in a vehicle already we'll use this vehicle to try to stream exit variations
	if (m_pPed->GetIsInVehicle() && m_pTargetVehicle)
	{
		const s32 iSeatIndex = m_pTargetVehicle->GetSeatManager()->GetPedsSeatIndex(m_pPed);
		if (m_pTargetVehicle->IsSeatIndexValid(iSeatIndex))
		{
			s32 iDirectEntryPointIndex = m_pTargetVehicle->GetDirectEntryPointIndexForSeat(iSeatIndex);
			if (m_pTargetVehicle->IsEntryIndexValid(iDirectEntryPointIndex))
			{
				return iDirectEntryPointIndex;
			}
		}
	}
	else if (m_pTargetVehicle)
	{
		return GetClosestTargetEntryPointIndexForPed(m_pTargetVehicle, m_pPed, 0);
	}
	return -1;
}

////////////////////////////////////////////////////////////////////////////////

void CVehicleClipRequestHelper::UpdateStreamedVariations(const CVehicle* pTargetVehicle, s32 iTargetEntryPointIndex)
{
	// If we're changing our target vehicle or entry point, check the conditions and choose our new conditional clipset
	if (m_pTargetVehicle != pTargetVehicle || m_iTargetEntryPointIndex != iTargetEntryPointIndex)
	{
		const CConditionalClipSet* pConditionalClipSet = ChooseVariationClipSetFromEntryIndex(pTargetVehicle, iTargetEntryPointIndex);

		m_pTargetVehicle = pTargetVehicle;
		m_iTargetEntryPointIndex = iTargetEntryPointIndex;

		CScenarioCondition::sScenarioConditionData conditionData;
		conditionData.pPed = m_pPed;
		conditionData.pTargetVehicle = m_pTargetVehicle;
		conditionData.iTargetEntryPoint = m_iTargetEntryPointIndex;

		if (!m_pConditionalClipSet || !m_pConditionalClipSet->CheckConditions(conditionData) || (m_pConditionalClipSet != pConditionalClipSet))
		{
			m_pConditionalClipSet = pConditionalClipSet;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleClipRequestHelper::RequestClipsFromVehicle(const CVehicle* pVehicle, s32 iFlags, s32 iTargetSeat)
{
	// Return true if not streaming clips, they will be loaded in with the vehicle model when created
	// (See CVehicleLayoutInfo::GetAllStreamedClipDictionaries)
	if (pVehicle->GetLayoutInfo() && !pVehicle->GetLayoutInfo()->GetStreamAnims())
	{
		return true;
	}

	if (!aiVerifyf(pVehicle->IsSeatIndexValid(iTargetSeat), "Target Seat Passed In %i Is Invalid", iTargetSeat))
	{
		return false;
	}

	bool bAllClipsLoaded = true;

	// Request the direct access entry clips for the target seat 
	if (iFlags & AF_StreamEntryClips)
	{
		if (!RequestDirectAccessEntryClipsForSeat(pVehicle, iTargetSeat))
		{
			bAllClipsLoaded = false;
		}
	}

	if (iFlags & AF_StreamExitClips)
	{
		if (!RequestDirectAccessExitClipsForSeat(pVehicle, iTargetSeat))
		{
			bAllClipsLoaded = false;
		}
	}

	// Request the seat clips for the target seat
	if (iFlags & AF_StreamSeatClips)
	{
		if (!RequestClipsFromSeat(pVehicle, iTargetSeat))
		{
			bAllClipsLoaded = false;
		}
	}

	if (m_pPed && m_pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_CanBeIncapacitated))
	{
		if (!RequestStunnedClipsFromSeat(pVehicle, iTargetSeat))
		{
			bAllClipsLoaded = false;
		}
	}

	// Early out if its the tourbus
	s32 iDirectSeatEntryIndex = pVehicle->GetDirectEntryPointIndexForSeat(iTargetSeat);
	if (!pVehicle->IsEntryIndexValid(iDirectSeatEntryIndex))
	{
		return false;
	}

	// Check if the seat has a shuffle seat
	s32 iShuffleSeatEntryIndex = pVehicle->GetIndirectEntryPointIndexForSeat(iTargetSeat);
	if (pVehicle->IsEntryIndexValid(iShuffleSeatEntryIndex) && (iFlags & AF_StreamConnectedSeatAnims))
	{
		const CEntryExitPoint* pShuffleEntryExitPoint = pVehicle->GetEntryExitPoint(iShuffleSeatEntryIndex);
		if (pShuffleEntryExitPoint)
		{
			// Request the direct access entry clips for the shuffle seat
			s32 iShuffleSeat = pShuffleEntryExitPoint->GetSeat(SA_directAccessSeat);

			if (iFlags & AF_StreamEntryClips)
			{
				if (!RequestDirectAccessEntryClipsForSeat(pVehicle, iShuffleSeat))
				{
					bAllClipsLoaded = false;
				}
			}

			if (iFlags & AF_StreamSeatClips)
			{
				// Request the seat clips for the shuffle seat
				if (!RequestClipsFromSeat(pVehicle, iShuffleSeat))
				{
					bAllClipsLoaded = false;
				}
			}
		}
	}
	return bAllClipsLoaded;
}

void CVehicleClipRequestHelper::ClearAllRequests()
{
	m_iNumClipSetsRequested = 0;

	for (s32 i=0; i<MAX_NUM_CLIPSET_REQUESTS; i++)
	{
		m_aClipSetIdsRequested[i] = CLIP_SET_ID_INVALID;
	}
}

bool CVehicleClipRequestHelper::HasClipSetBeenRequested(fwMvClipSetId clipSetId) const
{
	for (s32 i=0; i<m_iNumClipSetsRequested; i++)
	{
		if (m_aClipSetIdsRequested[i] == clipSetId)
		{
			return true;
		}
	}
	return false;
}

bool CVehicleClipRequestHelper::RequestDirectAccessEntryClipsForSeat(const CVehicle* pVehicle, s32 iSeat)
{
	bool bAllClipsLoaded = true;

	// We need to check all entry points since there might be more than one access point for one seat (e.g. bikes)
	for( s32 i = 0; i < pVehicle->GetNumberEntryExitPoints(); i++ )
	{
		//if (pVehicle->GetEntryExitPoint(i)->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle)
		//{
			if (pVehicle->GetEntryExitPoint(i)->GetSeat(SA_directAccessSeat) == iSeat)
			{
				if (!RequestClipsFromEntryPoint(pVehicle, i))
				{
					bAllClipsLoaded = false;
				}
				if (!RequestCommonClipsFromEntryPoint(pVehicle, i))
				{
					bAllClipsLoaded = false;
				}
			}
		//}
	}
	return bAllClipsLoaded;
}


bool CVehicleClipRequestHelper::RequestDirectAccessExitClipsForSeat(const CVehicle* pVehicle, s32 iSeat)
{
	bool bAllClipsLoaded = true;

	// We need to check all entry points since there might be more than one access point for one seat (e.g. bikes)
	for( s32 i = 0; i < pVehicle->GetNumberEntryExitPoints(); i++ )
	{
		if (pVehicle->GetEntryExitPoint(i)->GetSeat(SA_directAccessSeat) == iSeat)
		{
			if (!RequestClipsFromExitPoint(pVehicle, i))
			{
				bAllClipsLoaded = false;
			}
		}
	}
	return bAllClipsLoaded;
}

const CConditionalClipSet* CVehicleClipRequestHelper::ChooseVariationClipSetFromEntryIndex(const CVehicle* pVehicle, s32 iEntryIndex)
{
	if (pVehicle->IsEntryIndexValid(iEntryIndex))
	{
		//if (pVehicle->GetEntryExitPoint(iEntryIndex)->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle)
		//{
			const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(iEntryIndex);
			if (!pEntryPointClipInfo || !pEntryPointClipInfo->GetEntryAnimVariations())
			{
				return NULL;
			}

			CEntryAnimVariations* pEntryVariations = pEntryPointClipInfo->GetEntryAnimVariations();
			if (pEntryVariations)
			{
				CEntryAnimVariations::eAnimType eAnimType = CEntryAnimVariations::AT_ENTRY_VARIATION;

				CScenarioCondition::sScenarioConditionData conditionData;
				conditionData.pPed = m_pPed;
				conditionData.pTargetVehicle = pVehicle;
				conditionData.iTargetEntryPoint = iEntryIndex;

				s32 iSeatIndex = pVehicle->GetEntryExitPoint(iEntryIndex)->GetSeat(SA_directAccessSeat);
				CPed* pSeatOccupier = pVehicle->GetSeatManager()->GetPedInSeat(iSeatIndex);
				if (pSeatOccupier && pSeatOccupier->IsDead())
				{
					return NULL;	// No dead variations
				}
				else if (pSeatOccupier && pSeatOccupier != m_pPed && CTaskVehicleFSM::CanJackPed(m_pPed, pSeatOccupier))
				{
					const eCarLockState lockState = pVehicle->GetDoorLockStateFromEntryPoint(iEntryIndex);
					if (lockState != CARLOCK_LOCKED && lockState != CARLOCK_LOCKOUT_PLAYER_ONLY)
					{
						conditionData.pOtherPed = pSeatOccupier;
						eAnimType = CEntryAnimVariations::AT_JACKING_VARIATION;
					}
				}
				else if (m_pPed->GetIsInVehicle())
				{
					eAnimType = CEntryAnimVariations::AT_EXIT_VARIATION;
				}

				return pEntryVariations->ChooseClipSet(*pEntryVariations->GetClipSetArray(eAnimType), conditionData);
			}
		//}
	}
	return NULL;
}

void CVehicleClipRequestHelper::RequestEntryVariationClipSet(const CConditionalClipSet* pConditionalClipSet)
{
	if (pConditionalClipSet && m_iNumClipSetsRequested < MAX_NUM_CLIPSET_REQUESTS)
	{
		// HACK, don't stream skydive exits for non player peds, keep the request though so the exit task can still run it
		// though this requires the scripters to prestream the clipset if they want it to run immediately
		if (m_pPed->IsAPlayerPed() || !pConditionalClipSet->GetIsSkyDive())
		{
			if (!HasClipSetBeenRequested(pConditionalClipSet->GetClipSetId()))
			{
				m_aClipSetIdsRequested[m_iNumClipSetsRequested++] = pConditionalClipSet->GetClipSetId();
				CTaskVehicleFSM::RequestVehicleClipSet(pConditionalClipSet->GetClipSetId(), SP_High); // Should be variation, but we have important anims here atm
			}
		}
	}
}

bool CVehicleClipRequestHelper::RequestCommonClipsFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex)
{
	const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(iEntryPointIndex);
	if (pEntryPointClipInfo && m_iNumClipSetsRequested < MAX_NUM_CLIPSET_REQUESTS)
	{
		if (!HasClipSetBeenRequested(pEntryPointClipInfo->GetCommonClipSetId()))
		{
			m_aClipSetIdsRequested[m_iNumClipSetsRequested++] = pEntryPointClipInfo->GetCommonClipSetId();
			return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(pEntryPointClipInfo->GetCommonClipSetId(), SP_High);
		}
	}

	return false;
}

bool CVehicleClipRequestHelper::RequestClipsFromEntryPoint(const CVehicle* pVehicle, s32 iEntryPointIndex)
{
	const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(iEntryPointIndex);
	if (pEntryPointClipInfo && m_iNumClipSetsRequested < MAX_NUM_CLIPSET_REQUESTS)
	{
		if (!HasClipSetBeenRequested(pEntryPointClipInfo->GetEntryPointClipSetId()))
		{
			m_aClipSetIdsRequested[m_iNumClipSetsRequested++] = pEntryPointClipInfo->GetEntryPointClipSetId();
			return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(pEntryPointClipInfo->GetEntryPointClipSetId(), SP_High);
		}
	}

	return false;
}

bool CVehicleClipRequestHelper::RequestClipsFromExitPoint(const CVehicle* pVehicle, s32 iEntryPointIndex)
{
	const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(iEntryPointIndex);
	if (pEntryPointClipInfo && m_iNumClipSetsRequested < MAX_NUM_CLIPSET_REQUESTS)
	{
		if (!HasClipSetBeenRequested(pEntryPointClipInfo->GetExitPointClipSetId()))
		{
			m_aClipSetIdsRequested[m_iNumClipSetsRequested++] = pEntryPointClipInfo->GetExitPointClipSetId();
			return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(pEntryPointClipInfo->GetExitPointClipSetId(), m_pPed->IsPlayer() ? SP_High : SP_Medium);
		}
	}

	return false;
}

bool CVehicleClipRequestHelper::RequestClipsFromSeat(const CVehicle* pVehicle, s32 iSeatIndex)
{
	const CVehicleSeatAnimInfo* pSeatClipInfo = pVehicle->GetVehicleModelInfo()->GetSeatAnimationInfo(iSeatIndex);
	if (pSeatClipInfo && m_iNumClipSetsRequested < MAX_NUM_CLIPSET_REQUESTS)
	{
		if (!HasClipSetBeenRequested(pSeatClipInfo->GetSeatClipSetId()))
		{
			m_aClipSetIdsRequested[m_iNumClipSetsRequested++] = pSeatClipInfo->GetSeatClipSetId();
			return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(pSeatClipInfo->GetSeatClipSetId(), SP_High, m_fUpdateFrequency * 2.0f);
		}
	}

	return false;
}

bool CVehicleClipRequestHelper::RequestStunnedClipsFromSeat(const CVehicle* UNUSED_PARAM(pVehicle), s32 UNUSED_PARAM(iSeatIndex))
{
#if CNC_MODE_ENABLED
	const CVehicleSeatAnimInfo* pSeatClipInfo = pVehicle->GetVehicleModelInfo()->GetSeatAnimationInfo(iSeatIndex);
	if (pSeatClipInfo && m_iNumClipSetsRequested < MAX_NUM_CLIPSET_REQUESTS)
	{
		fwMvClipSetId stunnedClipset = pSeatClipInfo->GetStunnedClipSet();
		if (stunnedClipset.GetHash() == 0)	// Some vehicles will not have stunned clipsets (i.e. bikes) where ragdolling is expected instead.
		{
			return true;
		}
		if (!HasClipSetBeenRequested(stunnedClipset))
		{
			m_aClipSetIdsRequested[m_iNumClipSetsRequested++] = pSeatClipInfo->GetSeatClipSetId();
			return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(pSeatClipInfo->GetSeatClipSetId(), SP_High, m_fUpdateFrequency * 2.0f);
		}
	}
#endif

	return false;
}

bool CVehicleClipRequestHelper::RequestClipSet(fwMvClipSetId clipSetId)
{
	if (m_iNumClipSetsRequested < MAX_NUM_CLIPSET_REQUESTS)
	{
		if (!HasClipSetBeenRequested(clipSetId))
		{
			m_aClipSetIdsRequested[m_iNumClipSetsRequested++] = clipSetId;
			return CTaskVehicleFSM::RequestVehicleClipSetReturnIsLoaded(clipSetId, SP_High, m_fUpdateFrequency * 2.0f);
		}
	}

	return false;
}

s32 CVehicleClipRequestHelper::GetClosestTargetEntryPointIndexForPed(const CVehicle* pVehicle, CPed* pPed, s32 iSeat, bool bConsiderNonDriverAccess) const
{
	ScalarV vClosestDistSqr = ScalarVFromF32(LARGE_FLOAT);
	s32 iClosestTargetIndex = -1;

	// We need to check all entry points since there might be more than one access point for one seat (e.g. bikes)
	for( s32 i = 0; i < pVehicle->GetNumberEntryExitPoints(); i++ )
	{
		//if (pVehicle->GetEntryExitPoint(i)->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle)
		//{
			if (pVehicle->IsSeatIndexValid(iSeat))
			{
				if (pVehicle->GetEntryExitPoint(i)->CanSeatBeReached(iSeat) == SA_invalid)
				{
					continue;
				}
			}
			else
			{
				// If we're only interested in the driver seat ignore this entry point if it can't reach it
				SeatAccess seatAccess = pVehicle->GetEntryExitPoint(i)->CanSeatBeReached(0);
				if (!bConsiderNonDriverAccess && seatAccess == SA_invalid)
				{
					continue;
				}

				// Don't consider shuffle entry points on bikes
				if (pPed->IsLocalPlayer() && !NetworkInterface::IsGameInProgress() && pVehicle->GetIsSmallDoubleSidedAccessVehicle() && seatAccess == SA_indirectAccessSeat)
				{
					continue;
				}
			}

			Vector3 vEntryPosition(Vector3::ZeroType);
			if (pVehicle->GetEntryExitPoint(i)->GetEntryPointPosition(pVehicle, pPed, vEntryPosition, true))
			{
				ScalarV vDistSqr = DistSquared(RCC_VEC3V(vEntryPosition), pPed->GetTransform().GetPosition());

				if (IsLessThanAll(vDistSqr, vClosestDistSqr))
				{
					iClosestTargetIndex = i;
					vClosestDistSqr = vDistSqr;
				}
			}
		//}
	}

	return iClosestTargetIndex;
}

#if __BANK
void CVehicleClipRequestHelper::Debug() const
{
	if (m_pPed && (CVehicleDebug::ms_bRenderAnimStreamingDebug || CPedDebugVisualiser::GetDebugDisplay()==CPedDebugVisualiser::eVehicleEntryDebug))
	{
		Vector3 vecPedRight(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetA()));
		Vector3 vecPedForward(VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetB()));

		const Vector3 vPedPosition = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
		const float fNormSinceLastUpdate = m_fTimeSinceLastUpdate / m_fUpdateFrequency;
		Color32 lastCheckedGreenColor(0, 0, 0);
		lastCheckedGreenColor.SetGreen((u8)((1.0f - fNormSinceLastUpdate) * 255.0f));
		lastCheckedGreenColor.SetAlpha((u8)(fNormSinceLastUpdate * 255.0f));

		TUNE_GROUP_BOOL(VEHICLE_ANIM_STREAM_DEBUG, ENABLE_SCAN_RENDER, false);
		const float fRadius = rage::Lerp(1.0f - fNormSinceLastUpdate, 0.0f, m_fScanDistance); 
		if (ENABLE_SCAN_RENDER)
			grcDebugDraw::Circle(vPedPosition, fRadius, lastCheckedGreenColor, vecPedRight, vecPedForward, false);

		grcDebugDraw::Circle(vPedPosition, ms_Tunables.m_MinDistanceToScanForNearbyVehicle, Color_cyan, vecPedRight, vecPedForward, false);
		grcDebugDraw::Circle(vPedPosition, ms_Tunables.m_MaxDistanceToScanForNearbyVehicle, Color_blue, vecPedRight, vecPedForward, false);

		if (ENABLE_SCAN_RENDER)
			grcDebugDraw::Circle(vPedPosition, m_fScanDistance, Color_orange, vecPedRight, vecPedForward, false);

		TUNE_FLOAT(DEBUG_SEARCH_RADIUS, 30.0f, 0.0f, 50.0f, 0.1f);
		grcDebugDraw::Circle(vPedPosition, DEBUG_SEARCH_RADIUS, Color_red, vecPedRight, vecPedForward, false);

		const float fDesiredHeading = fwAngle::LimitRadianAngleSafe(m_pPed->GetDesiredHeading());

		Vec3V vXAxis = RotateAboutZAxis(Vec3V(V_Y_AXIS_WONE), ScalarVFromF32(fDesiredHeading));
		Vec3V vYAxis = RotateAboutZAxis(vXAxis, ScalarVFromF32(HALF_PI));
		const Vec3V vPedPos = m_pPed->GetTransform().GetPosition();

		//Vec3V vXAxisPos = RCC_VEC3V(vPedPosition) + vXAxis;
		//Vec3V vYAxisPos = RCC_VEC3V(vPedPosition) + vYAxis;

		Vec3V vArcStart = RotateAboutZAxis(vXAxis, ScalarVFromF32(fwAngle::LimitRadianAngle(-m_fScanArc)));
		Vec3V vMinArcPos = RCC_VEC3V(vPedPosition) + vArcStart;
		Vec3V vArcEnd = RotateAboutZAxis(vXAxis, ScalarVFromF32(fwAngle::LimitRadianAngle(m_fScanArc)));
		Vec3V vMaxArcPos = RCC_VEC3V(vPedPosition) + vArcEnd;

		//grcDebugDraw::Line(RCC_VEC3V(vPedPosition), vXAxisPos , Color_cyan);
		//grcDebugDraw::Line(RCC_VEC3V(vPedPosition), vYAxisPos , Color_orange);

		grcDebugDraw::Line(RCC_VEC3V(vPedPosition), vMinArcPos, Color_blue);
		grcDebugDraw::Line(RCC_VEC3V(vPedPosition), vMaxArcPos, Color_purple);

		if (ENABLE_SCAN_RENDER)
		{
			// I always end up having to render the arcs in two pieces, probably should write a wrapper for this stuff
			grcDebugDraw::Arc(vPedPos, fRadius, lastCheckedGreenColor, vXAxis, vYAxis, fwAngle::LimitRadianAngle(-m_fScanArc), 0.0f, true);
			grcDebugDraw::Arc(vPedPos, fRadius, lastCheckedGreenColor, vXAxis, vYAxis, 0.0f, fwAngle::LimitRadianAngle(m_fScanArc), true);
		}

		if (CVehicleDebug::ms_bRenderAnimStreamingText)
		{
			char szDebugText[128];
			sprintf(szDebugText, "Closest Vehicle %s, Closest Index : %u", m_pTargetVehicle ? m_pTargetVehicle->GetModelName() : "NULL", m_iTargetEntryPointIndex);
			grcDebugDraw::Text(vPedPosition, Color_orange, 0, 0, szDebugText);

			if (m_pTargetVehicle && m_pTargetVehicle->IsEntryIndexValid(m_iTargetEntryPointIndex))
			{
				if (m_pConditionalClipSet)
				{
					fwClipSet* pClipSet = fwClipSetManager::GetClipSet(m_pConditionalClipSet->GetClipSetId());
					if (pClipSet)
					{
						sprintf(szDebugText, "Variation clipset %s %s", m_pConditionalClipSet->GetClipSetId().GetCStr(), pClipSet->IsStreamedIn_DEPRECATED() ? "Streamed in" : "Not Streamed in");
						grcDebugDraw::Text(vPedPosition, pClipSet->IsStreamedIn_DEPRECATED() ? Color_green : Color_red, 0, 10, szDebugText);
					}
				}
			}
		}

		const CEntityScannerIterator nearbyVehicles = m_pPed->GetPedIntelligence()->GetNearbyVehicles();
		for(const CEntity* pEnt = nearbyVehicles.GetFirst(); pEnt; pEnt = nearbyVehicles.GetNext())
		{
			const CVehicle * pVehicle = static_cast<const CVehicle*>(pEnt);

			const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

			const float f2 = (vVehiclePosition-vPedPosition).Mag2();
			if(f2 < DEBUG_SEARCH_RADIUS*DEBUG_SEARCH_RADIUS)
			{
				grcDebugDraw::Sphere(vVehiclePosition + Vector3(0.0f,0.0f, 1.0f), 0.05f, Color_blue);

				const s32 iNumEntryExitPoints = pVehicle->GetNumberEntryExitPoints();
				for (s32 j=0; j<iNumEntryExitPoints; j++)
				{
					const CVehicleEntryPointAnimInfo* pEntryPointClipInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointAnimInfo(j);
					if (pEntryPointClipInfo)
					{
						Color32 iColor = Color_orange;

						Vector3 vEntryPosition(Vector3::ZeroType);
						Quaternion qEntryOrientation(0.0f,0.0f,0.0f,1.0f);
						CModelSeatInfo::CalculateEntrySituation(pVehicle, m_pPed, vEntryPosition, qEntryOrientation, j);

						char szDebugText[128];

						const fwMvClipSetId commonClipSetId = CTaskEnterVehicleAlign::GetCommonClipsetId(pVehicle, j);
						const fwMvClipSetId entryPointClipSetId = CTaskEnterVehicleAlign::GetEntryClipsetId(pVehicle, j, m_pPed);

						const bool bCommonClipSetRequested = HasClipSetBeenRequested(commonClipSetId);
						const bool bCommonClipSetLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(commonClipSetId);

						if (bCommonClipSetRequested && bCommonClipSetLoaded)
						{
							iColor = Color_green;
							sprintf(szDebugText, "Common %u ClipSet REQUESTED and LOADED", j);
						}
						else if (bCommonClipSetRequested && !bCommonClipSetLoaded)
						{
							iColor = Color_orange;
							sprintf(szDebugText, "Common %u ClipSet REQUESTED but NOT LOADED", j);
						}
						else if (!bCommonClipSetRequested && bCommonClipSetLoaded)
						{
							iColor = Color_DarkGreen;
							sprintf(szDebugText, "Common %u ClipSet NOT REQUESTED but LOADED", j);
						}
						else if (!bCommonClipSetRequested && !bCommonClipSetLoaded)
						{
							iColor = Color_red;
							sprintf(szDebugText, "Common %u ClipSet NOT REQUESTED and NOT LOADED", j);
						}

						grcDebugDraw::Sphere(vEntryPosition + Vector3(0.0f, 0.0f, 0.2f), 0.05f, iColor);	
						if (CVehicleDebug::ms_bRenderAnimStreamingText)
						{
							grcDebugDraw::Text(vEntryPosition, iColor, 0, -30, szDebugText);
						}

						const bool bEntryClipSetRequested = HasClipSetBeenRequested(entryPointClipSetId);
						const bool bEntryClipSetLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(entryPointClipSetId);

						if (bEntryClipSetRequested && bEntryClipSetLoaded)
						{
							iColor = Color_green;
							sprintf(szDebugText, "Entry %u ClipSet REQUESTED and LOADED", j);
						}
						else if (bEntryClipSetRequested && !bEntryClipSetLoaded)
						{
							iColor = Color_orange;
							sprintf(szDebugText, "Entry %u ClipSet REQUESTED but NOT LOADED", j);
						}
						else if (!bEntryClipSetRequested && bEntryClipSetLoaded)
						{
							iColor = Color_DarkGreen;
							sprintf(szDebugText, "Entry %u ClipSet NOT REQUESTED but LOADED", j);
						}
						else if (!bEntryClipSetRequested && !bEntryClipSetLoaded)
						{
							iColor = Color_red;
							sprintf(szDebugText, "Entry %u ClipSet NOT REQUESTED and NOT LOADED", j);
						}

						grcDebugDraw::Sphere(vEntryPosition + Vector3(0.0f, 0.0f, 0.1f), 0.05f, iColor);	

						if (CVehicleDebug::ms_bRenderAnimStreamingText)
						{
							grcDebugDraw::Text(vEntryPosition, iColor, 0, -20, szDebugText);
						}

						if (CVehicleDebug::ms_bRenderAnimStreamingText)
						{
							grcDebugDraw::Text(vEntryPosition, iColor, 0, -20, szDebugText);
							grcDebugDraw::Text(vEntryPosition, iColor, 0, -10, commonClipSetId.GetCStr());
							grcDebugDraw::Text(vEntryPosition, iColor, entryPointClipSetId.GetCStr());
						}
					}

					const CVehicleEntryPointInfo* pEntryPointInfo = pVehicle->GetVehicleModelInfo()->GetEntryPointInfo(j);
					if (pEntryPointInfo)
					{
						const CEntryExitPoint* pEntryExitPoint = pVehicle->GetVehicleModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(j);
						//if (pEntryExitPoint && pEntryExitPoint->GetSeatAccessor().GetSeatAccessType() == CSeatAccessor::eSeatAccessTypeSingle)
						//{			
							const CVehicleSeatAnimInfo* pSeatClipInfo = pVehicle->GetVehicleModelInfo()->GetSeatAnimationInfo(pEntryExitPoint->GetSeat(SA_directAccessSeat));
							if (pSeatClipInfo)
							{
								Vector3 vSeatPosition(Vector3::ZeroType);
								Quaternion qSeatOrientation(0.0f,0.0f,0.0f,1.0f);
								CModelSeatInfo::CalculateSeatSituation(pVehicle, vSeatPosition, qSeatOrientation, j);
								vSeatPosition += Vector3(0.0f,0.0f,1.0f);

								Color32 iColor = Color_orange;

								char szDebugText[128];

								const bool bInVehicleClipSetRequested = HasClipSetBeenRequested(pSeatClipInfo->GetSeatClipSetId());
								const bool bInVehicleClipSetLoaded = CTaskVehicleFSM::IsVehicleClipSetLoaded(pSeatClipInfo->GetSeatClipSetId());

								if (bInVehicleClipSetRequested && bInVehicleClipSetLoaded)
								{
									iColor = Color_green;
									sprintf(szDebugText, "Seat %u ClipSet REQUESTED and LOADED", j);
								}
								else if (bInVehicleClipSetRequested && !bInVehicleClipSetLoaded)
								{
									iColor = Color_orange;
									sprintf(szDebugText, "Seat %u ClipSet REQUESTED but NOT LOADED", j);
								}
								else if (!bInVehicleClipSetRequested && bInVehicleClipSetLoaded)
								{
									iColor = Color_DarkGreen;
									sprintf(szDebugText, "Seat %u ClipSet NOT REQUESTED but LOADED", j);
								}
								else if (!bInVehicleClipSetRequested && !bInVehicleClipSetLoaded)
								{
									iColor = Color_red;
									sprintf(szDebugText, "Seat %u ClipSet NOT REQUESTED and NOT LOADED", j);
								}

								grcDebugDraw::Sphere(vSeatPosition, 0.05f, iColor);

								if (CVehicleDebug::ms_bRenderAnimStreamingText)
								{
									grcDebugDraw::Text(vSeatPosition, iColor, 0, -10, szDebugText);
									grcDebugDraw::Text(vSeatPosition, iColor, pSeatClipInfo->GetSeatClipSetId().GetCStr());	
								}
							}
						//}
					}
				}
			}
		}
	}
}
#endif	//__BANK

/////////////////////////////////////////////////////////////////////////////////

bool CVehicleShockingEventHelper::ProcessShockingEvents(CVehicle *pVehicle,Vector3 &eventPosition, float &desiredSpeed)
{
	Assert(pVehicle);
	//Check if the vehicle is valid to slow down 

	//Direction - If you're not driving in the same direction don't slow down
	Vector3 vVehicleForward = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetForward());
	vVehicleForward.Normalize();
	Vector3 vDirectionToVehicle = eventPosition - VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	vDirectionToVehicle.Normalize();

	if ( vVehicleForward.Dot(vDirectionToVehicle) < 0.1f)
	{
		return false;
	}

	//Speed - Don't slow down if the vehicle is going at an excessive speed 
	if(pVehicle->GetVelocity().Mag2() > rage::square(20.f))
	{
		return false;
	}
	
	//Slow down the vehicle as you get closer to the target
	Vector3 vCurrentPosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
	Vector3 vTargetPosition = eventPosition;
	float fDistToStop = rage::square((vCurrentPosition - vTargetPosition).Mag2()) + pVehicle->GetBoundingBoxMax().y;

	if (fDistToStop >= 2.0f &&  fDistToStop < 30.f)
	{
		float fRatio = fDistToStop / 30.0f;
		fRatio = Clamp(fRatio, 0.0f, 1.0f);
		desiredSpeed = fRatio * desiredSpeed;
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////

CVehicleJunctionHelper::CVehicleJunctionHelper()
{
	m_StoppedReason = Stopped_For_Invalid;
	m_WaitingForTrafficLight = false;
}

CVehicleJunctionHelper::~CVehicleJunctionHelper()
{

}

u32 CVehicleJunctionHelper::GetTimeToStopForCurrentReason(const CVehicle *pVehicle) const
{
	Assert(pVehicle);

	switch (m_StoppedReason)
	{
	case Stopped_For_Light:
		return CDriverPersonality::FindDelayBeforeAcceleratingAfterLightsGoneGreen(pVehicle->GetDriver(), pVehicle);
	case Stopped_For_Traffic:
		return 0;
	case Stopped_For_Giveway:
		return CDriverPersonality::FindDelayBeforeAcceleratingAfterLightsGoneGreen(pVehicle->GetDriver(), pVehicle);
	default:
		aiAssertf(0, "Invalid Reason to be stopped at a junction!");
		break;
	}

	return 0;
}

bool CVehicleJunctionHelper::ProcessTrafficLights(CVehicle *pVehicle, float &desiredSpeed, const bool bShouldStopForLights, const bool bWasStoppedForTraffic)
{
	TUNE_GROUP_BOOL		(FOLLOW_PATH_AI_STEER,			handleStopLightsNew, true);

	m_WaitingForTrafficLight = false;

	if(handleStopLightsNew)
	{
		//const CVehicleNodeList* pNodeList = pVehicle->GetIntelligence()->GetNodeList();

		// If the car is cruising and approaching a green light it may accelerate.
		desiredSpeed = HandleAccelerateForStopLights(desiredSpeed, pVehicle, bShouldStopForLights);

		//float speedMultiplier = 1.0f;
		if( bShouldStopForLights )
		{
			//float trafficLightMult = 1.0f;
			bool bTurningRight = AreWeTurningRightAtJunction(pVehicle, bShouldStopForLights);
			const bool bCanTurnRightOnStopLight = (bTurningRight && pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO);

			CJunction * pJunction = pVehicle->GetIntelligence()->GetJunction();
			if(pJunction)
			{
				int iEntrance = -1;
				float fDist = 0.0f;
				pJunction->CalculateDistanceToJunction(pVehicle, &iEntrance, fDist);
				bool bGoingRightOnRed = false;
				ETrafficLightCommand iCmd = pJunction->GetTrafficLightCommand(pVehicle, iEntrance, bGoingRightOnRed, true, NULL, false);

				bool bApproachingGiveWay = false;
				if (iEntrance > -1)
				{
					const CPathNode* pEntranceNode = ThePaths.FindNodePointerSafe(pJunction->GetEntrance(iEntrance).m_Node);
					if (pEntranceNode && pEntranceNode->IsGiveWay())
					{
						bApproachingGiveWay = true;
					}
				}

				if(iCmd == TRAFFICLIGHT_COMMAND_STOP)
				{
					m_WaitingForTrafficLight = true;
				}

				if((iCmd == TRAFFICLIGHT_COMMAND_STOP && !bCanTurnRightOnStopLight)
					|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_LIGHTS
					|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC
					)
				{
					const float fMaxSpeedToApproachRedLight = GetMaxSpeedToApproachRedLight(pVehicle, desiredSpeed, fDist);
					// 					char text[64];
					// 					sprintf(text, "desired: %.2f max: %.2f", desiredSpeed, fMaxSpeedToApproachRedLight);
					// 					grcDebugDraw::Text(pVehicle->GetTransform().GetPosition(), Color_yellow, 0, 15, text);

					desiredSpeed = Min(desiredSpeed, fMaxSpeedToApproachRedLight);
					if (desiredSpeed <= 0.0f)
					{
						if (pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_WAIT_FOR_TRAFFIC)
						{
							m_StoppedReason = Stopped_For_Traffic;
						}
						else
						{
							m_StoppedReason = Stopped_For_Light;
						}

						pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();
						pVehicle->GetIntelligence()->InvalidateCachedNodeList();
						return true;
					}
					else if (desiredSpeed <= 1.0f)
					{
						pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();
					}
				}
				else if (bApproachingGiveWay)
				{
					const bool bCanGo = pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO || bWasStoppedForTraffic;
					const float fMaxSpeedToApproachGiveWay = GetMaxSpeedToApproachGiveWayNode(pVehicle, desiredSpeed, fDist, bCanGo);

					desiredSpeed = Min(desiredSpeed, fMaxSpeedToApproachGiveWay);

					if (desiredSpeed <= 0.0f)
					{
						m_StoppedReason = Stopped_For_Giveway;

						pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();
						pVehicle->GetIntelligence()->InvalidateCachedNodeList();
						return true;
					}
					else if (desiredSpeed <= 1.0f)
					{
						pVehicle->GetIntelligence()->UpdateCarHasReasonToBeStopped();
					}
				}
				else if ((pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_APPROACHING
					|| pVehicle->GetIntelligence()->GetJunctionCommand() == JUNCTION_COMMAND_GO)
					&&
					(iCmd == TRAFFICLIGHT_COMMAND_FILTER_RIGHT
					|| iCmd == TRAFFICLIGHT_COMMAND_FILTER_LEFT
					|| iCmd == TRAFFICLIGHT_COMMAND_FILTER_MIDDLE)
					)
				{
					u32 iDirFlags = 0;
					switch(iCmd)
					{
					case TRAFFICLIGHT_COMMAND_FILTER_LEFT:
						iDirFlags = BIT_TURN_LEFT;
						break;
					case TRAFFICLIGHT_COMMAND_FILTER_MIDDLE:
						iDirFlags = BIT_TURN_STRAIGHT_ON;
						break;
					case TRAFFICLIGHT_COMMAND_FILTER_RIGHT:
						iDirFlags = BIT_TURN_RIGHT;
						break;
					default: Assertf(false, "WTF?"); break;
					}

					//---------------------------------------------------------------------------------------
					// If we're cruising, then clear out nodes & add new ones -
					// PickOneNewNodeForCruising will pickup on the JUNCTION_COMMAND_FILTER_RIGHT (or similar)
					// command and will chose new nodes which turn right from the junction.

					CTaskVehicleMissionBase * pTask = pVehicle->GetIntelligence()->GetActiveTask();
					if (pTask && pTask->GetTaskType()==CTaskTypes::TASK_VEHICLE_CRUISE_NEW)
					{
						((CTaskVehicleCruiseNew*)pTask)->FixUpPathForTemplatedJunction(iDirFlags);
					}
				}
			}
		}
	}

	// since the 'cruise speed' in a task is an integer, never let get below 1.0f if we're not intending to stop for these lights
	// otherwise it will truncate to zero - and the car will stop.
	if(desiredSpeed > 0.0f)
	{
		desiredSpeed = Max(desiredSpeed, 1.0f);
	}

	return false;//we are not stopping at this traffic light.
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ApproachingGreenOrAmberLight
// PURPOSE :  Returns true if the next 4 or so nodes for this car contain a traffic light
//			  that is green or amber. The car may be tempted to speed up a bit if this is
//			  the case.
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CVehicleJunctionHelper::ApproachingGreenOrAmberLight(const CVehicle *pVehicle) 
{
	if (!pVehicle->GetIntelligence()->GetJunctionNode().IsEmpty())
	{
		CJunction* pJunction = pVehicle->GetIntelligence()->GetJunction();
		if (pJunction)
		{
			CNodeAddress entranceNode = pVehicle->GetIntelligence()->GetJunctionEntranceNode();
			if (entranceNode.IsEmpty())
			{
				return false;
			}
			const CPathNode* pEntrance = ThePaths.FindNodePointerSafe(entranceNode);
			const bool bEntranceIsLightNode = pEntrance && pEntrance->IsTrafficLight();
			if (!bEntranceIsLightNode)
			{
				return false;
			}
			s32 iEntrance = pJunction->FindEntranceIndexWithNode(entranceNode);
			bool bGoingRightOnRed = false;
			ETrafficLightCommand iCmd = pJunction->GetTrafficLightCommand(pVehicle, iEntrance, bGoingRightOnRed);
			if (iCmd == TRAFFICLIGHT_COMMAND_AMBERLIGHT || iCmd == TRAFFICLIGHT_COMMAND_GO || iCmd == TRAFFICLIGHT_COMMAND_FILTER_LEFT
				|| iCmd == TRAFFICLIGHT_COMMAND_FILTER_RIGHT || iCmd == TRAFFICLIGHT_COMMAND_FILTER_MIDDLE)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ApproachingRedLight
// PURPOSE :  Returns true if the next 4 or so nodes for this car contain a traffic light
//			  that is green or amber. 
// RETURNS :
/////////////////////////////////////////////////////////////////////////////////
bool CVehicleJunctionHelper::ApproachingRedLight(const CVehicle *pVehicle)
{
	if (!pVehicle->GetIntelligence()->GetJunctionNode().IsEmpty())
	{
		CJunction* pJunction = pVehicle->GetIntelligence()->GetJunction();
		if (pJunction)
		{
			CNodeAddress entranceNode = pVehicle->GetIntelligence()->GetJunctionEntranceNode();
			if (entranceNode.IsEmpty())
			{
				return false;
			}
			s32 iEntrance = pJunction->FindEntranceIndexWithNode(entranceNode);
			bool bGoingRightOnRed = false;
			ETrafficLightCommand iCmd = pJunction->GetTrafficLightCommand(pVehicle, iEntrance, bGoingRightOnRed);
			if (iCmd == TRAFFICLIGHT_COMMAND_STOP || bGoingRightOnRed)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return false;
}

bool CVehicleJunctionHelper::ApproachingLight(const CVehicle *pVehicle, const bool bConsiderGreen/* =true */, const bool bConsiderAmber/* =true */, const bool bConsiderRed/* =true */) 
{
	if (!pVehicle->GetIntelligence()->GetJunctionNode().IsEmpty())
	{
		CJunction* pJunction = pVehicle->GetIntelligence()->GetJunction();
		if (pJunction)
		{
			CNodeAddress entranceNode = pVehicle->GetIntelligence()->GetJunctionEntranceNode();
			if (entranceNode.IsEmpty())
			{
				return false;
			}
			s32 iEntrance = pJunction->FindEntranceIndexWithNode(entranceNode);
			bool bGoingRightOnRed = false;
			ETrafficLightCommand iCmd = pJunction->GetTrafficLightCommand(pVehicle, iEntrance, bGoingRightOnRed);
			if ((bConsiderAmber && iCmd == TRAFFICLIGHT_COMMAND_AMBERLIGHT) 
				|| 
				(bConsiderGreen && (iCmd == TRAFFICLIGHT_COMMAND_GO || iCmd == TRAFFICLIGHT_COMMAND_FILTER_LEFT
				|| iCmd == TRAFFICLIGHT_COMMAND_FILTER_RIGHT || iCmd == TRAFFICLIGHT_COMMAND_FILTER_MIDDLE))
				||
				(bConsiderRed && (iCmd == TRAFFICLIGHT_COMMAND_STOP || bGoingRightOnRed))
				)
			{
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return false;
}

float	CVehicleJunctionHelper::HandleAccelerateForStopLights(float currentDesiredSpeed, CVehicle* pVehicle, const bool bShouldObeyTrafficLights) const
{
	float desiredSpeed = currentDesiredSpeed;

	if( bShouldObeyTrafficLights )
	{
		if(	ApproachingGreenOrAmberLight(pVehicle) && (desiredSpeed > 0.0f))
		{
			desiredSpeed += CDriverPersonality::AccelerateSpeedToCatchGreenLight(pVehicle->GetDriver(), pVehicle);
		}
	}
	return desiredSpeed;
}

float CVehicleJunctionHelper::GetMaxSpeedToApproachRedLight(const CVehicle *pVehicle, const float fDesiredSpeed, const float fDistToLight) const
{
	Assert(pVehicle);

	//const float fActualSpeed = pVehicle->GetAiVelocity().Mag();
	const float fDistToStop = Max(pVehicle->GetAIHandlingInfo()->GetDistToStopAtCurrentSpeed(fDesiredSpeed)
		,  pVehicle->GetAIHandlingInfo()->GetMinBrakeDistance());

	const u32 turnDir = pVehicle->GetIntelligence()->GetJunctionTurnDirection();

	const float fVehHalfLengthMultiplier = pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG) ? 1.0f : 2.0f;
	const bool bWouldRunThisLight = CDriverPersonality::RunsAmberLights(pVehicle->GetDriver(), pVehicle, turnDir);
	const float fVehicleHalfLength = pVehicle->GetBoundingBoxMax().y;
	const bool bAlreadyStopped = pVehicle->GetAiXYSpeed() < 0.1f;
	const float fMaxAmountOverTheLineAllowed = !bWouldRunThisLight || bAlreadyStopped
		? fVehHalfLengthMultiplier * fVehicleHalfLength 
		: 0.0f;

	TUNE_GROUP_FLOAT(CAR_AI, fExtraStopForLightsDist, 1.0f, -10.0f, 10.0f, 0.1f); 
	const float fExtraDistance = /*fVehicleHalfLength +*/ fExtraStopForLightsDist;

	if (fDistToLight >= -fMaxAmountOverTheLineAllowed &&  fDistToLight < fDistToStop+fExtraDistance)
	{
		if (fDistToLight < fExtraDistance)
		{
			return 0.0f;
		}
		else
		{
			float fRatio = (fDistToLight - fExtraDistance) / fDistToStop;
			fRatio = Clamp(fRatio, 0.0f, 1.0f);
			return fRatio * fDesiredSpeed;
		}
	}

	return LARGE_FLOAT;
}

float CVehicleJunctionHelper::GetMaxSpeedToApproachGiveWayNode(const CVehicle *pVehicle, const float fDesiredSpeed, const float fDistToLight, const bool bHasGoSignalFromJunction) const
{
	const float fDistToStop = Max(pVehicle->GetAIHandlingInfo()->GetDistToStopAtCurrentSpeed(fDesiredSpeed)
		,  pVehicle->GetAIHandlingInfo()->GetMinBrakeDistance());
	//const float fVehicleHalfLength = pVehicle->GetBoundingBoxMax().y;
	TUNE_GROUP_FLOAT(CAR_AI, fExtraStopForLightsDist, 1.0f, -10.0f, 10.0f, 0.1f); 
	const bool bDoesCaliforniaRoll = CDriverPersonality::RollsThroughStopSigns(pVehicle->GetDriver(), pVehicle);
	const float fExtraDistance = bDoesCaliforniaRoll ? 0.0f : /*fVehicleHalfLength +*/ fExtraStopForLightsDist;
	float fMaxSpeedToApproachJunction = fDesiredSpeed;
	if (fDistToLight >= 0.0f &&  fDistToLight < fDistToStop+fExtraDistance)
	{
		if (fDistToLight < fExtraDistance)
		{
			fMaxSpeedToApproachJunction = 0.0f;
		}
		else
		{
			float fRatio = (fDistToLight - fExtraDistance) / fDistToStop;
			static dev_float s_fRatioMult = 3.0f;
			static dev_float s_fMinSpeedToApproach = 1.0f;
			fRatio *= s_fRatioMult;
			fRatio = Clamp(fRatio, 0.0f, 1.0f);
			fMaxSpeedToApproachJunction = fRatio * fDesiredSpeed;
			fMaxSpeedToApproachJunction = Max(s_fMinSpeedToApproach, fMaxSpeedToApproachJunction);
		}
	}

	//const float fActualSpeed = pVehicle->GetAiVelocity().Mag();

	//do different things based on driver aggression level
	if (CDriverPersonality::RunsStopSigns(pVehicle->GetDriver(), pVehicle))
	{
		//grcDebugDraw::Sphere(pVehicle->GetTransform().GetPosition(), 1.5f, Color_green, false);

		return fDesiredSpeed;
	}
	else if (bDoesCaliforniaRoll)
	{
		//grcDebugDraw::Sphere(pVehicle->GetTransform().GetPosition(), 1.5f, Color_yellow, false);

		if (bHasGoSignalFromJunction || fDistToLight < SMALL_FLOAT)
		{
			return fDesiredSpeed;
		}
		else
		{
			return Max(2.0f, fMaxSpeedToApproachJunction);
		}
	}
	else
	{
		//grcDebugDraw::Sphere(pVehicle->GetTransform().GetPosition(), 1.5f, Color_red, false);

		if (bHasGoSignalFromJunction)
		{
			return fDesiredSpeed;
		}
		else
		{
			return fMaxSpeedToApproachJunction;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AreWeTurningRightAtJunction
// PURPOSE :  works out if we are turning right at the next junction
//			  
/////////////////////////////////////////////////////////////////////////////////
bool CVehicleJunctionHelper::AreWeTurningRightAtJunction(CVehicle* pVeh, const bool bShouldStopForLights) const
{
	if(bShouldStopForLights)
	{
		//no junction
		if (pVeh->GetIntelligence()->GetJunctionNode().IsEmpty())
		{
			return false;
		}
		const CPathNode		*pEntranceNode, *pJunctionNode, *pExitNode;
		CNodeAddress entranceNode, exitNode;
		s32 iEntryLane, iExitLane;
		Vector3			DiffIn, DiffOut;
		CJunction* pJunction = pVeh->GetIntelligence()->GetJunction();
		if (!pJunction)
		{
			return false;
		}

		if (!pJunction->FindEntryAndExitNodes(pVeh, entranceNode, exitNode, iEntryLane, iExitLane))
		{
			return false;
		}

		pEntranceNode = ThePaths.FindNodePointerSafe(entranceNode);
		pJunctionNode = ThePaths.FindNodePointerSafe(pVeh->GetIntelligence()->GetJunctionNode());
		pExitNode = ThePaths.FindNodePointerSafe(exitNode);

		if (!pEntranceNode || !pJunctionNode || !pExitNode)
		{
			return false;
		}

		Vector3 coors1, coors2, coors3;
		pEntranceNode->GetCoors(coors1);
		pJunctionNode->GetCoors(coors2);
		pExitNode->GetCoors(coors3);

		DiffIn = coors2 - coors1;
		DiffOut = coors3 - coors2;
		DiffIn.z = 0.0f;
		DiffOut.z = 0.0f;
		DiffIn.Normalize();
		DiffOut.Normalize();

		if(DiffIn.Dot(DiffOut) < 0.25f)
		{		
			// Reasonably sharp turn
			float CrossUp = DiffIn.CrossZ(DiffOut);

			if(CrossUp < 0.0f)
			{
				return true; //turning right
			}
		}

		return false;
	}

	return false;
}

CVehicleWaypointRecordingHelper::CVehicleWaypointRecordingHelper()
{
	m_pRoute = NULL;
	m_iProgress = -1;
	m_iTargetProgress = -1;
	m_iFlags = 0;
	m_fOverrideSpeed = -1.0f;
	m_bLoopPlayback = false;
	m_bWantsToPause = false;
	m_bWantsToResumePlayback = false;
	m_bOverrideSpeed = false;
	m_bJustOverriddenSpeed = false;
}

CVehicleWaypointRecordingHelper::~CVehicleWaypointRecordingHelper()
{
	DeInit();
}

bool CVehicleWaypointRecordingHelper::Init(CWaypointRecordingRoute * pRoute, const int iInitialProgress, const u32 iFlags, const int iTargetProgress)
{
	AssertMsg(iTargetProgress == -1 || iTargetProgress >= iInitialProgress, "target progress cannot be less than initial progress!");

	m_pRoute = pRoute;
	m_iProgress = iInitialProgress;
	m_iFlags = iFlags;
	m_iTargetProgress = iTargetProgress;
	m_fOverrideSpeed = -1.0f;
	m_bLoopPlayback = false;
	m_bWantsToPause = false;
	m_bWantsToResumePlayback = false;
	m_bOverrideSpeed = false;
	m_bJustOverriddenSpeed = false;

#if __BANK
	if(m_pRoute != CWaypointRecording::GetRecordingRoute())
	{
		return CWaypointRecording::IncReferencesToRecording(m_pRoute);
	}
	return true;
#else
	return CWaypointRecording::IncReferencesToRecording(m_pRoute);
#endif
}

void CVehicleWaypointRecordingHelper::DeInit()
{
#if __BANK
	if(m_pRoute != CWaypointRecording::GetRecordingRoute())
		CWaypointRecording::DecReferencesToRecording(m_pRoute);
#else
	CWaypointRecording::DecReferencesToRecording(m_pRoute);
#endif
}

void CVehicleWaypointRecordingHelper::SetPause()
{
	//Assertf(GetState()!=State_Pause && GetState()!=State_PauseAndFacePlayer, "Trying to pause, but waypoint playback is already paused!");

	m_bWantsToResumePlayback = false;
	m_bWantsToPause = true;

	Assert(!(m_bWantsToPause&&m_bWantsToResumePlayback));
}
void CVehicleWaypointRecordingHelper::SetResume()
{
	//Assertf(GetState()==State_Pause || GetState()==State_PauseAndFacePlayer, "Trying to resume, but waypoint recording is not paused!");

	m_bWantsToPause = false;
	m_bWantsToResumePlayback = true;

	Assert(!(m_bWantsToPause&&m_bWantsToResumePlayback));
}
void CVehicleWaypointRecordingHelper::SetOverrideSpeed(const float fSpeed)
{
	m_bOverrideSpeed = true;
	m_bJustOverriddenSpeed = true;
	m_fOverrideSpeed = fSpeed;
}
void CVehicleWaypointRecordingHelper::UseDefaultSpeed()
{
	m_bOverrideSpeed = false;
	m_bJustOverriddenSpeed = true;
}

CVehicleDummyHelper::CVehicleDummyHelper()
{
}

CVehicleDummyHelper::~CVehicleDummyHelper()
{
}

void CVehicleDummyHelper::Process(CVehicle& rVehicle, fwFlags8 uFlags)
{
	//Check if the vehicle should be real this frame.
	bool bShouldBeRealThisFrame = false;
	if(uFlags.IsFlagSet(ForceReal))
	{
		bShouldBeRealThisFrame = true;
	}
	else if(uFlags.IsFlagSet(ForceRealUnlessNoCollision))
	{
		bShouldBeRealThisFrame = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(rVehicle.GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);
	}
	
	//Ensure the vehicle should be real this frame.
	if(!bShouldBeRealThisFrame)
	{
		return;
	}
	
	//Prevent being a dummy this frame.
	rVehicle.m_nVehicleFlags.bPreventBeingDummyThisFrame		= true;
	rVehicle.m_nVehicleFlags.bPreventBeingSuperDummyThisFrame	= true;
}

////////////////////////////////////////////////////////////////////////////////

CDirectionHelper::CDirectionHelper()
{

}

CDirectionHelper::~CDirectionHelper()
{

}

CDirectionHelper::Direction CDirectionHelper::CalculateClosestDirection(Vec3V_In vFromPosition, float fFromHeading, Vec3V_In vToPosition)
{
	return CalculateClosestDirectionFromValue(CalculateDirectionValue(vFromPosition, fFromHeading, vToPosition));
}

CDirectionHelper::Direction CDirectionHelper::CalculateClosestDirectionFromValue(float fValue)
{
	//0.00->0.125	- BackLeft
	//0.125->0.375	- Left
	//0.375->0.5	- FrontLeft
	//0.5->0.625	- FrontRight
	//0.625->0.875	- Right
	//0.875->1.0	- BackRight

	//Assert that the value is normalized.
	Assertf(fValue >= 0.0f && fValue <= 1.0f, "Value is not normalized: %.2f.", fValue);

	//Check the value.
	if(fValue <= 0.125)
	{
		return D_BackLeft;
	}
	else if(fValue <= 0.375f)
	{
		return D_Left;
	}
	else if(fValue <= 0.5f)
	{
		return D_FrontLeft;
	}
	else if(fValue <= 0.625f)
	{
		return D_FrontRight;
	}
	else if(fValue <= 0.875f)
	{
		return D_Right;
	}
	else
	{
		return D_BackRight;
	}
}

float CDirectionHelper::CalculateDirectionValue(Vec3V_In vFromPosition, float fFromHeading, Vec3V_In vToPosition)
{
	//Calculate the to heading.
	float fToHeading = fwAngle::GetRadianAngleBetweenPoints(vToPosition.GetXf(), vToPosition.GetYf(), vFromPosition.GetXf(), vFromPosition.GetYf());
	
	return CalculateDirectionValue(fFromHeading, fToHeading);
}

float CDirectionHelper::CalculateDirectionValue(float fFromHeading, float fToHeading)
{
	//Limit the headings.
	fFromHeading = fwAngle::LimitRadianAngleSafe(fFromHeading);
	fToHeading = fwAngle::LimitRadianAngleSafe(fToHeading);
	
	//Normalize the heading to [-1, 1].
	float fHeadingDelta = SubtractAngleShorter(fFromHeading, fToHeading);
	fHeadingDelta = Clamp(fHeadingDelta / PI, -1.0f, 1.0f);

	//Normalize the heading to [0, 1].
	return ((fHeadingDelta * 0.5f) + 0.5f);
}

CDirectionHelper::Direction CDirectionHelper::MoveDirectionToLeft(Direction nDirection)
{
	//Check the direction.
	switch(nDirection)
	{
		case D_FrontLeft:
		case D_FrontRight:
		{
			return D_Left;
		}
		case D_BackLeft:
		case D_BackRight:
		{
			return D_Right;
		}
		case D_Left:
		{
			return D_BackLeft;
		}
		case D_Right:
		{
			return D_FrontRight;
		}
		default:
		{
			Assertf(false, "Invalid direction: %d.", nDirection);
			return D_Invalid;
		}
	}
}

CDirectionHelper::Direction CDirectionHelper::MoveDirectionToRight(Direction nDirection)
{
	//Check the direction.
	switch(nDirection)
	{
		case D_FrontLeft:
		case D_FrontRight:
		{
			return D_Right;
		}
		case D_BackLeft:
		case D_BackRight:
		{
			return D_Left;
		}
		case D_Left:
		{
			return D_FrontLeft;
		}
		case D_Right:
		{
			return D_BackRight;
		}
		default:
		{
			Assertf(false, "Invalid direction: %d.", nDirection);
			return D_Invalid;
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CVehicleDriftHelper::CVehicleDriftHelper()
: m_fValue(0.0f)
, m_fRate(0.0f)
{
}

CVehicleDriftHelper::~CVehicleDriftHelper()
{
}

void CVehicleDriftHelper::Update(float fMinValueForCorrection, float fMaxValueForCorrection, float fMinRate, float fMaxRate)
{
	//Keep track of whether to calculate a new rate.
	bool bCalculateNewRate = false;
	
	//Keep track of whether to randomize the sign.
	bool bRandomizeSign = false;

	//Check if the rate has been set.
	if(m_fRate != 0.0f)
	{
		//Apply the rate.
		m_fValue += (m_fRate * fwTimer::GetTimeStep());

		//Check if the value has exceeded the threshold.
		float fValueForCorrection = fwRandom::GetRandomNumberInRange(fMinValueForCorrection, fMaxValueForCorrection);
		if(Abs(m_fValue) > fValueForCorrection)
		{
			//Calculate a new rate.
			bCalculateNewRate = true;
			
			//Do not randomize the sign.
			bRandomizeSign = false;
		}
	}
	else
	{
		//Calculate a new rate.
		bCalculateNewRate = true;
		
		//Randomize the sign.
		bRandomizeSign = true;
	}

	//Check if we should calculate a new rate.
	if(bCalculateNewRate)
	{
		//Calculate a new rate.
		m_fRate = fwRandom::GetRandomNumberInRange(fMinRate, fMaxRate);
		
		//Check if we should randomize the sign.
		if(bRandomizeSign)
		{
			//Randomize the sign.
			if(fwRandom::GetRandomTrueFalse())
			{
				m_fRate = -m_fRate;
			}
		}
		else
		{
			//Choose the sign that will make the value go in the opposite direction.
			if(m_fValue > 0.0f)
			{
				m_fRate = -m_fRate;
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CTalkHelper::CTalkHelper()
{

}

CTalkHelper::~CTalkHelper()
{

}

bool CTalkHelper::Talk(CPed& rTalker, const Options& rOptions)
{
	//Ensure the talker can talk.
	if(!CanPedTalk(rTalker))
	{
		return false;
	}
	
	//Find a ped to talk to.
	CAmbientAudioExchange exchange;
	CPed* pPedToTalkTo = FindPedToTalkTo(rTalker, rOptions, exchange);
	if(!pPedToTalkTo)
	{
		return false;
	}

	//Ensure the audio is valid.
	CAmbientAudio& rAudio = exchange.m_Initial;
	if(!rAudio.IsValid())
	{
		return false;
	}

	//Say the audio.
	if(!Talk(rTalker, *pPedToTalkTo, exchange))
	{
		return false;
	}
	
	return true;
}

bool CTalkHelper::Talk(CPed& rTalker, const CAmbientAudio& rAudio)
{
	//Say the context.
	bool bForcePlay		= rAudio.GetFlags().IsFlagSet(CAmbientAudio::ForcePlay);
	bool bAllowRepeat	= rAudio.GetFlags().IsFlagSet(CAmbientAudio::AllowRepeat);
	if(!rTalker.NewSay(rAudio.GetFinalizedHash(), 0, bForcePlay, bAllowRepeat))
	{
		//Note that the talker failed to talk.
		OnPedFailedToTalk(rTalker, rAudio);
		return false;
	}

	return true;
}

bool CTalkHelper::Talk(CPed& rTalker, CPed& rPedToTalkTo, const CAmbientAudio& rAudio)
{
	//Say the audio.
	if(!Talk(rTalker, rAudio))
	{
		return false;
	}

	//Note that the ped talked to the other ped.
	OnPedTalkedToPed(rTalker, rPedToTalkTo, rAudio);

	return true;
}

bool CTalkHelper::Talk(CPed& rTalker, CPed& rPedToTalkTo, const CAmbientAudioExchange& rExchange)
{
	//Grab the initial audio.
	const CAmbientAudio& rInitialAudio = rExchange.m_Initial;

	//Grab the response audio.
	const CAmbientAudio& rResponseAudio = rExchange.m_Response;

	//Check if we should NOT do an exchange.  We will NOT do an exchange if:
	//	1) The ped we are talking to is a player.  We want the player to respond on their own (via button press).
	//	2) The talker is the player, and the initial line is insulting.  We want the agitated system to handle the response.
	//	3) The response is invalid.
	//	4) The response is ignored.
	//	5) The ped we are talking to is fleeing.
	//	6) The ped we are talking to is reacting to a shocking event.
	bool bIsTalkingToPlayer					= rPedToTalkTo.IsPlayer();
	bool bIsTalkerAPlayer					= rTalker.IsPlayer();
	bool bIsInitialLineInsulting			= (rInitialAudio.GetFlags().IsFlagSet(CAmbientAudio::IsInsulting));
	bool bIsResponseValid					= rResponseAudio.IsValid();
	bool bIsResponseIgnored					= rResponseAudio.GetFlags().IsFlagSet(CAmbientAudio::IsIgnored);
	bool bIsTargetFleeing					= rPedToTalkTo.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_SMART_FLEE, true);
	bool bIsTargetReactingToShockingEvent	= (rPedToTalkTo.GetPedIntelligence()->GetCurrentEvent() && rPedToTalkTo.GetPedIntelligence()->GetCurrentEvent()->IsShockingEvent());
	bool bNoExchange = (bIsTalkingToPlayer || (bIsTalkerAPlayer && bIsInitialLineInsulting) ||
		!bIsResponseValid || bIsResponseIgnored || bIsTargetFleeing || bIsTargetReactingToShockingEvent);

	if(bIsTalkingToPlayer)
		rTalker.SetPedResetFlag(CPED_RESET_FLAG_TalkingToPlayer, true);

	if(bNoExchange)
	{
		//Say the audio.
		if(!Talk(rTalker, rPedToTalkTo, rInitialAudio))
		{
			return false;
		}

		//Check if we are talking to a player, and the response is valid.
		if(bIsTalkingToPlayer && bIsResponseValid)
		{
			//Set the talk response.
			rTalker.GetPedIntelligence()->SetTalkResponse(rResponseAudio);
		}

		return true;
	}
	else
	{
		//Do the exchange.
		bool bForcePlay		= rInitialAudio.GetFlags().IsFlagSet(CAmbientAudio::ForcePlay);
		bool bAllowRepeat	= rInitialAudio.GetFlags().IsFlagSet(CAmbientAudio::AllowRepeat);
		if(!rTalker.NewSay(rInitialAudio.GetFinalizedHash(), 0, bForcePlay, bAllowRepeat, -1, &rPedToTalkTo, rResponseAudio.GetFinalizedHash()))
		{
			//Note that the talker failed to talk.
			OnPedFailedToTalk(rTalker, rInitialAudio);
			return false;
		}

		//Note that the ped talked.
		OnPedTalkedToPed(rTalker, rPedToTalkTo, rInitialAudio);

		//Note that the ped responded.
		OnPedTalkedToPed(rPedToTalkTo, rTalker, rResponseAudio);

		return true;
	}
}

Vec3V_Out CTalkHelper::CalculateDirectionForPedToTalk(const CPed& rPed)
{
	//Check if the ped is flipping someone off.
	Vec3V vDirection;
	if(IsPedFlippingSomeoneOff(rPed, vDirection))
	{
		return vDirection;
	}
	
	//Use the forward direction.
	vDirection = rPed.GetTransform().GetForward();
	
	return vDirection;
}

bool CTalkHelper::CanPedTalk(const CPed& rPed)
{
	//Ensure the ped is a player.
	if(!aiVerifyf(rPed.IsPlayer(), "The ped is not a player."))
	{
		return false;
	}
	
	//Ensure the speech entity is valid.
	const audSpeechAudioEntity* pAudioEntity = rPed.GetSpeechAudioEntity();
	if(!aiVerifyf(pAudioEntity, "The audio entity is invalid."))
	{
		return false;
	}
	
	//Ensure the ped is not talking.
	if(pAudioEntity->IsAmbientSpeechPlaying())
	{
		return false;
	}

	//Ensure the flag is not set.
	if(rPed.GetPedResetFlag(CPED_RESET_FLAG_DisableTalk))
	{
		return false;
	}
	
	return true;
}

bool CTalkHelper::CanScorePedToTalkTo(const CPed& rTalker, const CPed& rPedToTalkTo, const Options& rOptions, CAmbientAudioExchange& rExchange)
{
	//Ensure the peds are not in different interiors.
	if(rTalker.GetPortalTracker()->GetInteriorNameHash() != rPedToTalkTo.GetPortalTracker()->GetInteriorNameHash())
	{
		return false;
	}

	//Ensure the ped can be talked to.
	if(!rPedToTalkTo.GetTaskData().GetIsFlagSet(CTaskFlags::CanBeTalkedTo))
	{
		return false;
	}

	//Check if the talker is on foot.
	if(rTalker.GetIsOnFoot())
	{
		//Check if the ped is in a vehicle.
		const CVehicle* pVehicle = rPedToTalkTo.GetVehiclePedInside();
		if(pVehicle)
		{
			//Check if the vehicle is moving quickly.
			float fSpeedSq = pVehicle->GetVelocity().XYMag2();
			static dev_float s_fMaxSpeed = 4.0f;
			float fMaxSpeedSq = square(s_fMaxSpeed);
			if(fSpeedSq > fMaxSpeedSq)
			{
				return false;
			}
		}
	}

	//Get the exchange for the ped.
	GetExchangeForPed(rTalker, rPedToTalkTo, rOptions, rExchange);

	//Ensure the audio is valid.
	const CAmbientAudio& rAudio = rExchange.m_Initial;
	if(!rAudio.IsValid())
	{
		return false;
	}
	
	//Grab the talker position.
	Vec3V vTalkerPosition = rTalker.GetTransform().GetPosition();
	
	//Grab the ped position.
	Vec3V vPedPosition = rPedToTalkTo.GetTransform().GetPosition();

	//Ensure the distance has not exceeded the threshold.
	ScalarV scDistSq = DistSquared(vTalkerPosition, vPedPosition);
	ScalarV scMaxDistSq = ScalarVFromF32(square(rOptions.m_fMaxDistance));
	if(IsGreaterThanAll(scDistSq, scMaxDistSq))
	{
		return false;
	}

	//Calculate the direction for the talker to talk.
	Vec3V vTalkerDirection = CalculateDirectionForPedToTalk(rTalker);
	
	//Calculate the direction from the talker to the ped.
	Vec3V vTalkerToPed = Subtract(vPedPosition, vTalkerPosition);
	Vec3V vTalkerToPedDirection = NormalizeFastSafe(vTalkerToPed, Vec3V(V_ZERO));

	//Ensure the direction has not exceeded the threshold.
	ScalarV scDot = Dot(vTalkerDirection, vTalkerToPedDirection);
	ScalarV scMinDot = ScalarVFromF32(rOptions.m_fMinDot);
	if(IsLessThanAll(scDot, scMinDot))
	{
		return false;
	}

	return true;
}

bool CTalkHelper::CanTalkToPed(const CPed& rPedToTalkTo)
{
	//Ensure the flag is not set.
	if(rPedToTalkTo.GetPedConfigFlag(CPED_CONFIG_FLAG_DisableTalkTo))
	{
		return false;
	}

	//Ensure the ped is not in a conversation.
	CTaskAmbientClips* pTaskAmbientClips = static_cast<CTaskAmbientClips *>(
		rPedToTalkTo.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AMBIENT_CLIPS));
	if(pTaskAmbientClips && pTaskAmbientClips->IsInConversation())
	{
		return false;
	}

	//Can't talk to peds who are cowering.
	if (rPedToTalkTo.GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COWER_SCENARIO, true))
	{
		return false;
	}

	return true;
}

CPed* CTalkHelper::FindPedToTalkTo(const CPed& rTalker, const Options& rOptions, CAmbientAudioExchange& rExchange)
{
	//Keep track of the best ped.
	CPed* pBestPed = NULL;
	float fBestScore = FLT_MIN;

	//Note: We can no longer use the nearby peds list as it does not include dead peds.

	//Iterate over the nearby peds.
	Vec3V vPosition = rTalker.GetTransform().GetPosition();
	CEntityIterator entityIterator(IteratePeds|CheckDistance, &rTalker, &vPosition, rOptions.m_fMaxDistance);
	for(CEntity* pEntity = entityIterator.GetNext(); pEntity != NULL; pEntity = entityIterator.GetNext())
	{
		//Grab the ped.
		aiAssert(pEntity->GetIsTypePed());
		CPed* pPed = static_cast<CPed *>(pEntity);

		//Ensure we can talk to the ped.
		CAmbientAudioExchange exchange;
		if(!CanScorePedToTalkTo(rTalker, *pPed, rOptions, exchange))
		{
			continue;
		}

		//Ensure the score is better.
		float fScore = ScorePedToTalkTo(rTalker, *pPed, rOptions);
		if(pBestPed && (fScore <= fBestScore))
		{
			continue;
		}

		//Set the best ped.
		pBestPed	= pPed;
		fBestScore	= fScore;
		rExchange	= exchange;
	}

	//Ensure the best ped is valid.
	if(!pBestPed)
	{
		return NULL;
	}

	//Ensure we can talk to the ped.
	if(!CanTalkToPed(*pBestPed))
	{
		return NULL;
	}

	return pBestPed;
}

void CTalkHelper::GetExchangeForPed(const CPed& rTalker, const CPed& rPedToTalkTo, const Options& rOptions, CAmbientAudioExchange& rExchange)
{
	//Create the options.
	CAmbientAudioManager::Options options;

	//Check if the audio can be friendly or unfriendly.
	bool bCanBeFriendly		= rOptions.m_uFlags.IsFlagSet(Options::CanBeFriendly);
	bool bCanBeUnfriendly	= rOptions.m_uFlags.IsFlagSet(Options::CanBeUnfriendly);

	//Check if the audio must be friendly.
	bool bMustBeFriendly = bCanBeFriendly && !bCanBeUnfriendly;
	if(bMustBeFriendly)
	{
		options.m_uFlags.SetFlag(CAmbientAudioManager::Options::MustBeFriendly);
	}

	//Check if the audio must be unfriendly.
	bool bMustBeUnfriendly = !bCanBeFriendly && bCanBeUnfriendly;
	if(bMustBeUnfriendly)
	{
		options.m_uFlags.SetFlag(CAmbientAudioManager::Options::MustBeUnfriendly);
	}

	//Create the follow-up options.
	CAmbientAudioManager::Options followUpOptions(options);
	followUpOptions.m_uFlags.SetFlag(CAmbientAudioManager::Options::OnlyFollowUps);

	//Check if the follow-up is valid.
	const CAmbientAudioExchange* pExchange = CAmbientAudioManager::GetInstance().GetExchange(rTalker, rPedToTalkTo, followUpOptions);
	if(pExchange)
	{
		//Set the exchange.
		rExchange = *pExchange;
		return;
	}

	//Get the response for the ped.
	CAmbientAudio audio;
	GetResponseForPed(rTalker, rPedToTalkTo, audio);
	if(audio.IsValid())
	{
		//Set the initial audio for the exchange (it is one-sided if it is a response).
		rExchange.m_Initial = audio;
		return;
	}

	//Check if the exchange is valid.
	pExchange = CAmbientAudioManager::GetInstance().GetExchange(rTalker, rPedToTalkTo, options);
	if(pExchange)
	{
		//Set the exchange.
		rExchange = *pExchange;
		return;
	}
}

void CTalkHelper::GetResponseForPed(const CPed& UNUSED_PARAM(rTalker), const CPed& rPedToTalkTo, CAmbientAudio& rAudio)
{
	//Check if the response is valid.
	const CAmbientAudio& rResponse = rPedToTalkTo.GetPedIntelligence()->GetTalkResponse();
	if(rResponse.IsValid())
	{
		//Set the audio.
		rAudio = rResponse;
	}
}

bool CTalkHelper::IsPedFlippingSomeoneOff(const CPed& rPed, Vec3V_InOut vDirection)
{
	//Ensure the ped is doing a drive-by.
	CTaskAimGunVehicleDriveBy* pTask = static_cast<CTaskAimGunVehicleDriveBy *>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_VEHICLE_DRIVE_BY));
	if(!pTask)
	{
		return false;
	}
	
	//Ensure the ped is flipping someone off.
	if(!pTask->IsFlippingSomeoneOff())
	{
		return false;
	}
	
	//Grab the ped position.
	Vec3V vPedPosition = rPed.GetTransform().GetPosition();
	
	//Grab the ped forward vector.
	Vec3V vPedForward = rPed.GetTransform().GetForward();
	
	//Grab the target position.
	Vec3V vTargetPosition;
	pTask->GetTarget().GetPosition(RC_VECTOR3(vTargetPosition));
	
	//Calculate the direction.
	Vec3V vPedToTarget = Subtract(vTargetPosition, vPedPosition);
	vDirection = NormalizeFastSafe(vPedToTarget, vPedForward);
	
	return true;
}

void CTalkHelper::OnPedFailedToTalk(const CPed& OUTPUT_ONLY(rTalker), const CAmbientAudio& OUTPUT_ONLY(rAudio))
{
	//Note that we failed to talk.
	taskDisplayf("Ped with model: %s failed to say: %s with finalized hash: %d, partial hash: %d, force play: %s, allow repeat: %s.",
		rTalker.GetModelName(),
		atHashWithStringNotFinal(rAudio.GetFinalizedHash()).TryGetCStr(), rAudio.GetFinalizedHash(), rAudio.GetPartialHash(),
		rAudio.GetFlags().IsFlagSet(CAmbientAudio::ForcePlay) ? "yes" : "no",
		rAudio.GetFlags().IsFlagSet(CAmbientAudio::AllowRepeat) ? "yes" : "no");
}

void CTalkHelper::OnPedTalkedToPed(CPed& rTalker, CPed& rPedToTalkTo, const CAmbientAudio& rAudio)
{
	//Check if the talker is a player.
	if(rTalker.IsPlayer())
	{
		//Note: For now, AI peds only become agitated with players.
		//		This may need to change in the future.

		//Check if the audio was insulting.
		if(rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsHostile))
		{
			//Notify the target that the talker is hostile.
			{
				CEventAgitated event(&rTalker, AT_Hostile);
				rPedToTalkTo.GetPedIntelligence()->AddEvent(event);
			}
		}
		//Check if the audio was insulting.
		else if(rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsInsulting))
		{
			//Notify the target that they were insulted.
			{
				CEventAgitated event(&rTalker, AT_Insulted);
				rPedToTalkTo.GetPedIntelligence()->AddEvent(event);
			}

			//Emit a shocking event for the insult.
			{
				CEventShockingSeenInsult event(rTalker, &rPedToTalkTo);
				CShockingEventsManager::Add(event);
			}
		}
		//Check if the audio was harassing.
		else if(rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsHarassing))
		{
			//Notify the target that they are being harassed.
			{
				CEventAgitated event(&rTalker, AT_Harassed);
				rPedToTalkTo.GetPedIntelligence()->AddEvent(event);
			}
		}
	}
	else
	{
		//Check if the audio is a rant.
		if(rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsRant))
		{
			//Notify the talker that they are ranting.
			{
				CEventAgitated event(&rPedToTalkTo, AT_Ranting);
				rTalker.GetPedIntelligence()->AddEvent(event);
			}
		}
	}

	//Note that the talker talked to a ped.
	CPedIntelligence::TalkInfo talkedToPedInfo(&rPedToTalkTo, rAudio);
	rTalker.GetPedIntelligence()->SetTalkedToPedInfo(talkedToPedInfo);

	//Look at the ped to talk to.
	rTalker.GetIkManager().LookAt(0, &rPedToTalkTo, 5000, BONETAG_HEAD, NULL);

	//Check if the audio is not ignored.
	bool bIsIgnored = rAudio.GetFlags().IsFlagSet(CAmbientAudio::IsIgnored);
	if(!bIsIgnored)
	{
		//Note that the ped to talk to was talked to.
		CPedIntelligence::TalkInfo pedTalkedToUsInfo(&rTalker, rAudio);
		rPedToTalkTo.GetPedIntelligence()->SetPedTalkedToUsInfo(pedTalkedToUsInfo);

		//Look at the talker.
		rPedToTalkTo.GetIkManager().LookAt(0, &rTalker, 5000, BONETAG_HEAD, NULL);
	}

	//Clear the talk response.
	rPedToTalkTo.GetPedIntelligence()->ClearTalkResponse();
}

float CTalkHelper::ScorePedToTalkTo(const CPed& rTalker, const CPed& rPedToTalkTo, const Options& rOptions)
{
	aiAssert(rTalker.IsPlayer() || NetworkInterface::IsInSpectatorMode());

	//Calculate the distance.
	Vec3V vTalkerToPedToTalkTo = Subtract(rPedToTalkTo.GetTransform().GetPosition(), rTalker.GetTransform().GetPosition());
	ScalarV scDistSq = MagSquared(vTalkerToPedToTalkTo);
	ScalarV scMaxDistSq = ScalarVFromF32(square(rOptions.m_fMaxDistance));
	scDistSq = Clamp(scDistSq, ScalarV(V_ZERO), scMaxDistSq);

	//Calculate the facing dot.
	Vec3V vForward = VECTOR3_TO_VEC3V(camInterface::GetGameplayDirector().GetFrame().GetFront());
	Vec3V vTalkerToPedToTalkToDirection = NormalizeFastSafe(vTalkerToPedToTalkTo, Vec3V(V_ZERO));
	ScalarV scDot = Dot(vForward, vTalkerToPedToTalkToDirection);

	//Calculate the score.
	ScalarV scScore = Subtract(scMaxDistSq, scDistSq);
	ScalarV scDotSq = Scale(scDot, scDot);
	scScore = Scale(scScore, scDotSq);

	//Give a bonus to the last ped we talked to.
	if(&rPedToTalkTo == rTalker.GetPedIntelligence()->GetTalkedToPedInfo().m_pPed)
	{
		static dev_float s_fMultiplier = 1.25f;
		scScore = Scale(scScore, ScalarVFromF32(s_fMultiplier));
	}

	return scScore.Getf();
}


////////////////////////////////////////////////////////////////////////////////

fwMvNetworkId	 CUpperBodyAimPitchHelper::ms_UpperBodyAimNetworkId("UpperBodyAimNetwork",0xE7D67809);
fwMvRequestId	 CUpperBodyAimPitchHelper::ms_BlendInUpperBodyAimRequestId("BlendInUpperBodyAim",0xC69950E0);
fwMvFloatId		 CUpperBodyAimPitchHelper::ms_UpperBodyBlendInDurationId("UpperBodyBlendInDuration",0xAF8B32DC);
fwMvFloatId		 CUpperBodyAimPitchHelper::ms_PitchId("Pitch",0x3F4BB8CC);
fwMvFloatId		 CUpperBodyAimPitchHelper::ms_AimBlendInDurationId("AimBlendInDuration",0x13DEEF5E);
fwMvFloatId		 CUpperBodyAimPitchHelper::ms_GripBlendInDurationId("GripBlendInDuration",0x54C2A14A);

////////////////////////////////////////////////////////////////////////////////

void CUpperBodyAimPitchHelper::CalculateAimVector(const CPed* pPed, Vector3& vStart, Vector3& vEnd, Vector3& vTargetPos, const bool bForceUseCameraIfPlayer)
{
	const CWeapon* pWeapon = pPed->GetWeaponManager()->GetEquippedWeapon();

	if (pPed->GetIsInCover())
	{
		const CWeapon* pObjectWeapon = pPed->GetWeaponManager()->GetEquippedWeaponFromObject();
		if (pObjectWeapon)
		{
			pWeapon = pObjectWeapon;
		}
	}

	if (!pWeapon)
	{
		return;
	}

	float fWeaponRange = pWeapon->GetRange();

	const camGameplayDirector& gameplayDirector = camInterface::GetGameplayDirector();
	const bool isAimCameraActive = gameplayDirector.IsAiming(pPed)  FPS_MODE_SUPPORTED_ONLY(|| camInterface::GetGameplayDirector().GetFirstPersonShooterCamera());

	if(pPed->IsLocalPlayer() && !pPed->GetPlayerInfo()->AreControlsDisabled() && (isAimCameraActive || bForceUseCameraIfPlayer))
	{
		const camFrame& aimFrame = camInterface::GetPlayerControlCamAimFrame();

		if(pPed->GetPlayerResetFlag(CPlayerResetFlags::PRF_ASSISTED_AIMING_ON))
		{
			CEntity* pLockOnTarget = pPed->GetPlayerInfo()->GetTargeting().GetLockOnTarget();
			if(pLockOnTarget)
			{
				vStart = aimFrame.GetPosition();
				vEnd = VEC3V_TO_VECTOR3(pLockOnTarget->GetTransform().GetPosition());
				return;
			}
		}

		vStart = aimFrame.GetPosition();
		Vector3	vecAim = aimFrame.GetFront();
		vecAim.Normalize();
		vEnd = vStart + (fWeaponRange * vecAim);
		vTargetPos = vEnd;
	}
	else
	{
		if(pPed->GetWeaponManager()->GetEquippedWeaponAttachPoint() == CPedEquippedWeapon::AP_RightHand)
		{
			vStart = pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_R_HAND);	
		}
		else
		{
			vStart = pPed->GetBonePositionCached((eAnimBoneTag)BONETAG_L_HAND);	
		}

		// This is a bit nasty, perhaps we should store a target entity external from tasks?
		CTaskGun* pGunTask = static_cast<CTaskGun*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GUN));
		if(pGunTask)
		{
			if(pGunTask->GetTarget().GetIsValid())
				pGunTask->GetTarget().GetPositionWithFiringOffsets(pPed, vEnd);
			vTargetPos = vEnd;
		}
		else
		{
			CTaskInCover* pCoverTask = static_cast<CTaskInCover*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_IN_COVER));
			if(pCoverTask)
			{
#if __ASSERT
				if(pCoverTask->GetTarget().GetIsValid())
				{
					taskAssertf(pCoverTask->GetTarget().GetPosition(vEnd), "Couldn't find cover target position");
				}
#endif // __ASSERT
				vTargetPos = vEnd;
			}
			else
			{
				CTaskGetUp* pGetUpTask = static_cast<CTaskGetUp*>(pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_GET_UP));
				if (pGetUpTask && pGetUpTask->GetTarget().GetIsValid())
				{
					pGetUpTask->GetTarget().GetPosition(vEnd);
				}
				else
				{
					// Use the last position
					vEnd = vTargetPos;
				}
			}
		}

		static dev_float TOO_CLOSE_DIST_SQR = 1.5f;
		if(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()).Dist2(vEnd) < TOO_CLOSE_DIST_SQR)
		{
			if (pPed->GetPedResetFlag(CPED_RESET_FLAG_IsAiming))
			{
				CTask* pMotionTask = pPed->GetCurrentMotionTask(false);
				if (pMotionTask && pMotionTask->GetTaskType()==CTaskTypes::TASK_MOTION_AIMING) 
				{						
					static_cast<CTaskMotionAiming*>(pMotionTask)->SetOverwriteMaxPitchChange(CTaskMotionAiming::GetTunables().m_OverwriteMaxPitch);
				}
			}
		}
	}

	if(CTaskAimGun::ms_bUseIntersectionPos)
	{
		// Fire a probe from the camera to find intersections
		WorldProbe::CShapeTestFixedResults<> probeResults;
		WorldProbe::CShapeTestProbeDesc probeDesc;

		const s32 iNumExclusions = 1;
		const CEntity* exclusionList[iNumExclusions] = { pPed };
		const u32 includeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_PED_TYPE;
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetStartAndEnd(vStart, vEnd);
		probeDesc.SetIncludeFlags(includeFlags);
		probeDesc.SetExcludeEntities(exclusionList, iNumExclusions);
		probeDesc.SetContext(WorldProbe::LOS_Weapon);

		if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
		{
			vEnd = probeResults[0].GetHitPosition();
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

float CUpperBodyAimPitchHelper::ComputeDesiredPitchSignal(const CPed& ped, float* pfOverrideDesiredPitch, const bool bForceUseCameraIfPlayer, const bool bUntranformRelativeToPed)
{
	const CWeapon* pWeapon = ped.GetWeaponManager() ? ped.GetWeaponManager()->GetEquippedWeapon() : NULL;
	if (pWeapon && pWeapon->GetWeaponInfo())
	{
		bool bShouldUseAimingInfo = true;
#if FPS_MODE_SUPPORTED
		// Check if the aiming info is only valid in FPS.
		if(!ped.IsFirstPersonShooterModeEnabledForPlayer(false) && pWeapon->GetWeaponInfo()->GetOnlyUseAimingInfoInFPS())
		{
			bShouldUseAimingInfo = false;
		}
#endif	// FPS_MODE_SUPPORTED
		const CAimingInfo* pAimingInfo = pWeapon->GetWeaponInfo()->GetAimingInfo();
		if (pAimingInfo && bShouldUseAimingInfo)
		{
			// Just update pitch parameter, yaw is handled by torso ik

			// Get the sweep ranges from the weapon info
			const float fSweepPitchMin = pAimingInfo->GetSweepPitchMin() * DtoR;
			const float fSweepPitchMax = pAimingInfo->GetSweepPitchMax() * DtoR;

			// Calculate the aim vector (this determines the heading and pitch angles to point the clips in the correct direction)
			Vector3 vStart(Vector3::ZeroType);
			Vector3 vEnd(Vector3::ZeroType);
			Vector3 vTarget(Vector3::ZeroType);
			CalculateAimVector(&ped, vStart, vEnd, vTarget, bForceUseCameraIfPlayer);

			// Calculate the desired pitch.
			const float fDesiredPitch = pfOverrideDesiredPitch ? *pfOverrideDesiredPitch : CTaskAimGun::CalculateDesiredPitch(&ped, vStart, vEnd, bUntranformRelativeToPed);

			// Map the angle to the range 0.0->1.0
			float fDesiredPitchSignal = -1.0f;
			if (fDesiredPitch < 0.0f)
			{
				fDesiredPitchSignal = ((fSweepPitchMin - fDesiredPitch) / fSweepPitchMin) * 0.5f;
			}
			else
			{
				fDesiredPitchSignal = (fDesiredPitch / fSweepPitchMax) * 0.5f + 0.5f;
			}

			static dev_float EXTRA_PITCH = 0.0225f;
			if(fDesiredPitchSignal < 0.5f)
			{
				fDesiredPitchSignal += fDesiredPitchSignal * 2.f * EXTRA_PITCH;
			}
			else
			{
				fDesiredPitchSignal += (1.f - (fDesiredPitchSignal - 0.5f) * 2.f) * EXTRA_PITCH;
			}
			return fDesiredPitchSignal;
		}
	}
	return -1.0f;
}

////////////////////////////////////////////////////////////////////////////////

CUpperBodyAimPitchHelper::CUpperBodyAimPitchHelper()
: m_fCurrentPitchSignal(-1.0f)
, m_ClipSetId(CLIP_SET_ID_INVALID)
{

}

////////////////////////////////////////////////////////////////////////////////

CUpperBodyAimPitchHelper::~CUpperBodyAimPitchHelper()
{
	
}

////////////////////////////////////////////////////////////////////////////////

bool CUpperBodyAimPitchHelper::BlendInUpperBodyAim(CMoveNetworkHelper& parentMoveNetworkHelper, const CPed& ped, float fBlendInDuration, const bool bForceUseCameraIfPlayer, float fAimTransitionBlend, float fGripTransitionBlend)
{
	if (ped.GetWeaponManager())
	{
		fwMvClipSetId clipset = m_ClipSetId != CLIP_SET_ID_INVALID ? m_ClipSetId : GetDefaultClipSet(ped);
		if (clipset != CLIP_SET_ID_INVALID)
		{
			if (m_moveNetworkHelper.IsNetworkActive())
			{
				m_moveNetworkHelper.ReleaseNetworkPlayer();
			}

			if (!m_moveNetworkHelper.IsNetworkActive())
			{
				m_moveNetworkHelper.CreateNetworkPlayer(&ped, CClipNetworkMoveInfo::ms_NetworkUpperBodyAimPitch);
			}

			// We assume our parent network doesn't have any intermediate transitions and our set subnetwork will succeed the next frame
			parentMoveNetworkHelper.SendRequest(ms_BlendInUpperBodyAimRequestId);
			parentMoveNetworkHelper.SetSubNetwork(m_moveNetworkHelper.GetNetworkPlayer(), ms_UpperBodyAimNetworkId);

			// Need to set params initially
			parentMoveNetworkHelper.SetFloat(ms_UpperBodyBlendInDurationId, fBlendInDuration);
			m_moveNetworkHelper.SetClipSet(clipset);

#if FPS_MODE_SUPPORTED
			TUNE_GROUP_BOOL(FIRST_PERSON_TUNE, ENABLE_ATT_GRIP_IN_UPPERBODY_AIM_HELPER, true);
			if (ENABLE_ATT_GRIP_IN_UPPERBODY_AIM_HELPER)
			{
				const CWeapon* pWeapon = ped.GetWeaponManager() ? ped.GetWeaponManager()->GetEquippedWeapon() : NULL;
				bool bFPSModeEnabled = ped.UseFirstPersonUpperBodyAnims(false);
				bool bFPSModeEnabledLocal = bFPSModeEnabled && ped.IsLocalPlayer();
				bool bFPSUseGripAttachment = bFPSModeEnabledLocal && pWeapon && pWeapon->HasGripAttachmentComponent();
				m_moveNetworkHelper.SetFlag(bFPSUseGripAttachment, CTaskMotionAiming::ms_FPSUseGripAttachment);
			}
#endif // FPS_MODE_SUPPORTED
			m_moveNetworkHelper.SetFloat(ms_AimBlendInDurationId, fAimTransitionBlend);
			m_moveNetworkHelper.SetFloat(ms_GripBlendInDurationId, fGripTransitionBlend);


#if __ASSERT
			const fwMvClipId lowLoopClipId("AIM_LOW_LOOP",0xD2BA280E);
			taskAssertf(fwClipSetManager::GetClip(clipset, lowLoopClipId), "Couldn't find clip %s from clipset %s", lowLoopClipId.GetCStr(), clipset.GetCStr());
#endif // __ASSERT

			// Update the aim pitch param
			UpdatePitchParameter(ped, bForceUseCameraIfPlayer);
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

void CUpperBodyAimPitchHelper::Update(const CPed& ped, const bool bForceUseCameraIfPlayer)
{
	if (m_moveNetworkHelper.IsNetworkActive())
	{
		UpdatePitchParameter(ped, bForceUseCameraIfPlayer);
	}
}

////////////////////////////////////////////////////////////////////////////////

void CUpperBodyAimPitchHelper::UpdatePitchParameter(const CPed& ped, const bool bForceUseCameraIfPlayer)
{
	const float fDesiredPitchSignal = ComputeDesiredPitchSignal(ped, NULL, bForceUseCameraIfPlayer);
	
	// The desired heading is smoothed so it doesn't jump too much in a timestep
	if (m_fCurrentPitchSignal >= 0.0f)
	{	
		m_fCurrentPitchSignal = CTaskAimGun::SmoothInput(m_fCurrentPitchSignal, fDesiredPitchSignal, CTaskMotionAiming::GetTunables().m_PitchChangeRate);
	}
	else
	{
		m_fCurrentPitchSignal = fDesiredPitchSignal;
	}
	m_moveNetworkHelper.SetFloat(ms_PitchId, m_fCurrentPitchSignal);
}

////////////////////////////////////////////////////////////////////////////////

fwMvClipSetId CUpperBodyAimPitchHelper::GetDefaultClipSet(const CPed& ped) const
{
	const CWeaponInfo* pWeaponInfo = ped.GetWeaponManager()->GetEquippedWeaponInfo();

	if (ped.GetIsInCover())
	{
		const CWeaponInfo* pObjectWeaponInfo = ped.GetWeaponManager()->GetEquippedWeaponInfoFromObject();
		if (pObjectWeaponInfo)
		{
			pWeaponInfo = pObjectWeaponInfo;
		}
	}

#if FPS_MODE_SUPPORTED
	FPSStreamingFlags fpsStreamingFlag = FPS_StreamDefault;
	if(ped.IsFirstPersonShooterModeEnabledForPlayer(false) && ped.GetPedResetFlag(CPED_RESET_FLAG_InCoverTaskActive))
	{
		bool bFPSScopeActive = pWeaponInfo && !pWeaponInfo->GetDisableFPSScope() && (ped.GetPlayerInfo()->GetInFPSScopeState() || ped.GetMotionData()->GetIsFPSScope());
		fpsStreamingFlag = (bFPSScopeActive) ? FPS_StreamScope : FPS_StreamLT;
		// Don't want to force lt anims when transitioning to aiming directly in cover
		TUNE_GROUP_BOOL(REAR_COVER_AIM_BUGS, DONT_FORCE_LT_AIMING, true);
		if (DONT_FORCE_LT_AIMING && CTaskCover::IsPlayerAimingDirectlyInFirstPerson(ped))
		{
			fpsStreamingFlag = FPS_StreamDefault;
		}
	}
#endif

	if (pWeaponInfo)
	{
		fwMvClipSetId clipset = pWeaponInfo->GetAppropriateWeaponClipSetId(&ped FPS_MODE_SUPPORTED_ONLY(, fpsStreamingFlag));
		return clipset;
	}
	return CLIP_SET_ID_INVALID;
}

////////////////////////////////////////////////////////////////////////////////

CEquipWeaponHelper::CEquipWeaponHelper()
{

}

CEquipWeaponHelper::~CEquipWeaponHelper()
{

}

void CEquipWeaponHelper::EquipBestMeleeWeapon(CPed& rPed, const EquipBestMeleeWeaponInput& rInput)
{
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Equip the best melee weapon.
	pWeaponManager->EquipBestMeleeWeapon(rInput.m_bForceIntoHand, rInput.m_bProcessWeaponInstructions);
}

void CEquipWeaponHelper::EquipBestWeapon(CPed& rPed, const EquipBestWeaponInput& rInput)
{
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	//Equip the best weapon.
	pWeaponManager->EquipBestWeapon(rInput.m_bForceIntoHand, rInput.m_bProcessWeaponInstructions);
}

void CEquipWeaponHelper::EquipWeaponForTarget(CPed& rPed, const CPed& rTarget, const EquipWeaponForTargetInput& rInput)
{
	//Choose the response from the attributes.
	Response nResponse = ChooseResponseFromAttributes(rPed, rTarget);
	if(nResponse == R_None)
	{
		//Choose the response from the friends.
		nResponse = ChooseResponseFromFriends(rPed);
		if(nResponse == R_None)
		{
			//Choose the weapon response state from the personality.
			nResponse = ChooseResponseFromPersonality(rPed, rTarget);
		}
	}

	//Check the weapon response.
	switch(nResponse)
	{
		case R_Keep:
		{
			//Keep the weapon.
			break;
		}
		case R_MatchTarget:
		{
			//Match the target weapon.
			MatchTargetWeapon(rPed, rTarget, rInput.m_EquipBestWeaponInput, rInput.m_EquipBestMeleeWeaponInput);
			break;
		}
		case R_None:
		case R_EquipBest:
		default:
		{
			//Equip the best weapon.
			EquipBestWeapon(rPed, rInput.m_EquipBestWeaponInput);
			break;
		}
	}
}

void CEquipWeaponHelper::MatchTargetWeapon(CPed& rPed, const CPed& rTarget, const EquipBestWeaponInput& rBestWeaponInput, const EquipBestMeleeWeaponInput& rBestMeleeWeaponInput)
{
	//Calculate the equipped weapon state for the ped.
	CWeaponInfoHelper::State nEquippedWeaponStateForPed = CWeaponInfoHelper::CalculateState(rPed, CWeaponInfoHelper::A_Equipped);

	//Calculate the equipped weapon state for the target.
	CWeaponInfoHelper::State nEquippedWeaponStateForTarget = CWeaponInfoHelper::CalculateState(rTarget, CWeaponInfoHelper::A_Equipped);

	//Check if the weapon is already sufficient (equal or better).
	bool bIsWeaponSufficient = (nEquippedWeaponStateForPed >= nEquippedWeaponStateForTarget) && (nEquippedWeaponStateForPed > CWeaponInfoHelper::S_NonViolent);
	if(bIsWeaponSufficient)
	{
		//Unarmed opponents are allowed to pull melee weapons on other unarmed opponents.
		bIsWeaponSufficient = !((nEquippedWeaponStateForPed <= CWeaponInfoHelper::S_Unarmed) && (nEquippedWeaponStateForTarget <= CWeaponInfoHelper::S_Unarmed));
		if(bIsWeaponSufficient)
		{
			//The weapon is sufficient, no need to change.
			return;
		}
	}

	//Check the target armament.
	switch(nEquippedWeaponStateForTarget)
	{
		case CWeaponInfoHelper::S_Armed:
		{
			//Equip the best weapon.
			EquipBestWeapon(rPed, rBestWeaponInput);
			break;
		}
		case CWeaponInfoHelper::S_None:
		case CWeaponInfoHelper::S_NonViolent:
		case CWeaponInfoHelper::S_Unarmed:
		case CWeaponInfoHelper::S_Melee:
		default:
		{
			//Equip the best melee weapon.
			EquipBestMeleeWeapon(rPed, rBestMeleeWeaponInput);
			break;
		}
	}
}

void CEquipWeaponHelper::RequestEquippedWeaponAnims(CPed& rPed)
{
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	CPedEquippedWeapon* pEquippedWeapon = pWeaponManager->GetPedEquippedWeapon();
	if(!pEquippedWeapon)
	{
		return;
	}

	//Release the previously requested weapon anims
	pEquippedWeapon->ReleaseWeaponAnims();

	//Request the weapon anims
	pEquippedWeapon->RequestWeaponAnims();
}

void CEquipWeaponHelper::ReleaseEquippedWeaponAnims(CPed& rPed)
{
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return;
	}

	CPedEquippedWeapon* pEquippedWeapon = pWeaponManager->GetPedEquippedWeapon();
	if(!pEquippedWeapon)
	{
		return;
	}

	//Release the previously requested weapon anims
	pEquippedWeapon->ReleaseWeaponAnims();
}

bool CEquipWeaponHelper::AreEquippedWeaponAnimsStreamedIn(CPed& rPed)
{
	//Ensure the weapon manager is valid.
	CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	CPedEquippedWeapon* pEquippedWeapon = pWeaponManager->GetPedEquippedWeapon();
	if(!pEquippedWeapon)
	{
		return false;
	}
	
	return pEquippedWeapon->HaveWeaponAnimsBeenRequested() && pEquippedWeapon->AreWeaponAnimsStreamedIn();
}

bool CEquipWeaponHelper::ShouldPedSwapWeapon(const CPed& rPed)
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	//Ensure the weapon manager requires a weapon switch.
	if(!pWeaponManager->GetRequiresWeaponSwitch())
	{
		return false;
	}

	//Ensure we aren't already waiting for a weapon switch
	if(pWeaponManager->GetCreateWeaponObjectWhenLoaded())
	{
		return false;
	}

	return true;
}

CEquipWeaponHelper::Response CEquipWeaponHelper::ChooseResponseFromAttributes(const CPed& rPed, const CPed& rTarget)
{
	//Check if the ped should always keep their weapon.
	if(ShouldAlwaysKeepWeapon(rPed, rTarget))
	{
		return R_Keep;
	}
	//Check if the ped should always match the target's weapon.
	else if(ShouldAlwaysMatchTargetWeapon(rPed, rTarget))
	{
		return R_MatchTarget;
	}
	//Check if the ped should always equip their best weapon.
	else if(ShouldAlwaysEquipBestWeapon(rPed, rTarget))
	{
		return R_EquipBest;
	}
	else
	{
		return R_None;
	}
}

CEquipWeaponHelper::Response CEquipWeaponHelper::ChooseResponseFromFriends(const CPed& rPed)
{
	//Check if we have a combat director.
	const CCombatDirector* pCombatDirector = rPed.GetPedIntelligence()->GetCombatDirector();
	if(pCombatDirector)
	{
		//Check if there is more than one ped (more than just us).
		if(pCombatDirector->GetNumPeds() > 1)
		{
			//Check if any of the peds have a gun.
			if(pCombatDirector->GetNumPedsWithGuns() > 0)
			{
				return R_EquipBest;
			}
			else
			{
				return R_Keep;
			}
		}
	}

	return R_None;
}

CEquipWeaponHelper::Response CEquipWeaponHelper::ChooseResponseFromPersonality(const CPed& rPed, const CPed& rTarget)
{
	//Calculate the equipped weapon state for the target.
	CWeaponInfoHelper::State nEquippedWeaponStateForTarget = CWeaponInfoHelper::CalculateState(rTarget, CWeaponInfoHelper::A_Equipped);

	//Grab the threat response.
	const CPedModelInfo::PersonalityThreatResponse* pThreatResponse = NULL;
	switch(nEquippedWeaponStateForTarget)
	{
		
		case CWeaponInfoHelper::S_Melee:
		{
			pThreatResponse = &rPed.GetPedModelInfo()->GetPersonalitySettings().GetThreatResponseMelee();
			break;
		}
		case CWeaponInfoHelper::S_Armed:
		{
			pThreatResponse = &rPed.GetPedModelInfo()->GetPersonalitySettings().GetThreatResponseArmed();
			break;
		}
		case CWeaponInfoHelper::S_None:
		case CWeaponInfoHelper::S_NonViolent:
		case CWeaponInfoHelper::S_Unarmed:
		default:
		{
			pThreatResponse = &rPed.GetPedModelInfo()->GetPersonalitySettings().GetThreatResponseUnarmed();
			break;
		}
	}

	//Generate the array of weights.
	atRangeArray<float, R_Max> aWeights;
	aWeights[R_Keep]		= !ShouldNeverKeepWeapon(rPed, rTarget)			? pThreatResponse->m_Fight.m_Weights.m_KeepWeapon			: 0.0f;
	aWeights[R_MatchTarget]	= !ShouldNeverMatchTargetWeapon(rPed, rTarget)	? pThreatResponse->m_Fight.m_Weights.m_MatchTargetWeapon	: 0.0f;
	aWeights[R_EquipBest]	= !ShouldNeverEquipBestWeapon(rPed, rTarget)	? pThreatResponse->m_Fight.m_Weights.m_EquipBestWeapon		: 0.0f;

	//Generate the total weight.
	float fTotalWeight = 0.0f;
	for(int iState = R_Keep; iState < R_Max; ++iState)
	{
		fTotalWeight += aWeights[iState];
	}

	//Ensure the total weight is valid.
	if(fTotalWeight == 0.0f)
	{
		return R_None;
	}

	//Generate a random number.
	float fRand = fwRandom::GetRandomNumberInRange(0.0f, fTotalWeight);

	//Choose the state.
	for(int iState = R_Keep; iState < R_Max; ++iState)
	{
		//Grab the weight.
		const float fWeight = aWeights[iState];
		if(fWeight <= 0.0f)
		{
			continue;
		}

		//Subtract the weight.
		fRand -= fWeight;
		if(fRand <= 0.0f)
		{
			return (Response)iState;
		}
	}

	return R_None;
}

bool CEquipWeaponHelper::IsArmed(const CPed& rPed)
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	return pWeaponManager->GetIsArmed();
}

bool CEquipWeaponHelper::IsArmedWithGun(const CPed& rPed)
{
	//Ensure the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(!pWeaponManager)
	{
		return false;
	}

	return pWeaponManager->GetIsArmedGun();
}

bool CEquipWeaponHelper::IsHelpingWithFistFight(const CPed& rPed, const CPed& rTarget)
{
	//Ensure the current event is valid.
	const CEvent* pEvent = rPed.GetPedIntelligence()->GetCurrentEvent();
	if(!pEvent)
	{
		return false;
	}

	//Check if we are helping a ped.
	const CPed* pPedThatNeedsHelp = NULL;
	switch(pEvent->GetEventType())
	{
		case EVENT_SHOUT_TARGET_POSITION:
		case EVENT_INJURED_CRY_FOR_HELP:
		{
			//Assert that the events inherit from the same class.
			taskAssertf(dynamic_cast<const CEventRequestHelp *>(pEvent), "The event does not inherit from the expected class.");

			//Ensure the ped that needs help is valid.
			pPedThatNeedsHelp = static_cast<const CEventRequestHelp *>(pEvent)->GetPedThatNeedsHelp();
			if(!pPedThatNeedsHelp)
			{
				return false;
			}

			break;
		}
		default:
		{
			return false;
		}
	}

	//Ensure the ped we are helping is not armed with a gun.
	if(IsArmedWithGun(*pPedThatNeedsHelp))
	{
		return false;
	}

	//Ensure the target is not armed with a gun.
	if(IsArmedWithGun(rTarget))
	{
		return false;
	}

	return true;
}

bool CEquipWeaponHelper::ShouldAlwaysEquipBestWeapon(const CPed& rPed, const CPed& UNUSED_PARAM(rTarget))
{
	//All peds in MP games will equip their best weapon.
	if(NetworkInterface::IsGameInProgress())
	{
		return true;
	}

	//All law enforcement peds will equip their best weapon.
	if(rPed.IsLawEnforcementPed())
	{
		return true;
	}

	//All mission peds that are NOT armed will equip their best weapon.
	if(rPed.PopTypeIsMission() && !IsArmed(rPed))
	{
		return true;
	}

	//All peds should choose their best weapon during riots.
	if(CRiots::GetInstance().GetEnabled())
	{
		return true;
	}

	//Check if the flag is set.
	if(rPed.GetPedIntelligence()->GetCombatBehaviour().IsFlagSet(CCombatData::BF_AlwaysEquipBestWeapon))
	{
		return true;
	}

	//Check if we have brandished our weapon.
	if(rPed.GetPedConfigFlag(CPED_CONFIG_FLAG_HasBrandishedWeapon))
	{
		return true;
	}

	return false;
}

bool CEquipWeaponHelper::ShouldAlwaysKeepWeapon(const CPed& rPed, const CPed& UNUSED_PARAM(rTarget))
{
	//All conditions must be true for the ped to keep their weapon:
	//	1) The game type must be single player.
	//	2) The ped must be mission type.
	//	3) The ped must not be law enforcement.
	//	4) The ped must be armed.
	if(!NetworkInterface::IsGameInProgress() && rPed.PopTypeIsMission() && !rPed.IsLawEnforcementPed() && IsArmed(rPed))
	{
		return true;
	}

	return false;
}

bool CEquipWeaponHelper::ShouldAlwaysMatchTargetWeapon(const CPed& UNUSED_PARAM(rPed), const CPed& UNUSED_PARAM(rTarget))
{
	return false;
}

bool CEquipWeaponHelper::ShouldNeverEquipBestWeapon(const CPed& rPed, const CPed& rTarget)
{
	//Check if we are helping with a fist fight.
	if(IsHelpingWithFistFight(rPed, rTarget))
	{
		return true;
	}

	return false;
}

bool CEquipWeaponHelper::ShouldNeverKeepWeapon(const CPed& UNUSED_PARAM(rPed), const CPed& UNUSED_PARAM(rTarget))
{
	return false;
}

bool CEquipWeaponHelper::ShouldNeverMatchTargetWeapon(const CPed& UNUSED_PARAM(rPed), const CPed& UNUSED_PARAM(rTarget))
{
	return false;
}

////////////////////////////////////////////////////////////////////////////////

CWeaponInfoHelper::CWeaponInfoHelper()
{

}

CWeaponInfoHelper::~CWeaponInfoHelper()
{

}

CWeaponInfoHelper::State CWeaponInfoHelper::CalculateState(const CPed& rPed, Armament nArmament)
{
	//Check if the weapon manager is valid.
	const CPedWeaponManager* pWeaponManager = rPed.GetWeaponManager();
	if(pWeaponManager)
	{
		CVehicle* pTargetVehicle = rPed.GetVehiclePedInside();
		if(pTargetVehicle)
		{
			const CVehicleWeapon* pEquippedVehicleWeapon = rPed.GetWeaponManager()->GetEquippedVehicleWeapon();
			if(pEquippedVehicleWeapon && !pEquippedVehicleWeapon->IsWaterCannon())
			{
				return S_Armed;
			}
		}

		//Get the weapon info.
		const CWeaponInfo* pWeaponInfo = NULL;
		switch(nArmament)
		{
			case A_Equipped:
			{
				//If the ped is in a vehicle and has a projectile equipped, use that.
				bool bUseEquippedWeaponInfo = (rPed.GetIsInVehicle() && pWeaponManager->GetEquippedWeaponInfo() &&
					pWeaponManager->GetEquippedWeaponInfo()->GetIsProjectile());
				if(bUseEquippedWeaponInfo)
				{
					//Use the equipped weapon info.
					pWeaponInfo = pWeaponManager->GetEquippedWeaponInfo();
				}
				else
				{
					//Use the equipped weapon object (in the ped's hand).
					pWeaponInfo = pWeaponManager->GetEquippedWeapon() ? pWeaponManager->GetEquippedWeapon()->GetWeaponInfo() : NULL;
				}

				break;
			}
			case A_Best:
			{
				pWeaponInfo = pWeaponManager->GetBestWeaponInfo(true);
				break;
			}
			default:
			{
				break;
			}
		}

		//Check if the weapon info is valid.
		if(pWeaponInfo)
		{
			//Check the info type.
			if(pWeaponInfo->GetIsNonViolent() || pWeaponInfo->GetIsNotAWeapon())
			{
				return S_NonViolent;
			}
			else if(pWeaponInfo->GetIsUnarmed() || pWeaponInfo->GetIsMeleeFist())
			{
				//Treat fist weapons (i.e. Knuckle Dusters) as unarmed in order to not provoke threat responses
				return S_Unarmed;
			}
			else if(pWeaponInfo->GetIsMelee())
			{
				return S_Melee;
			}
			else if(pWeaponInfo->GetIsArmed())
			{
				return S_Armed;
			}
		}
	}

	return S_None;
}

////////////////////////////////////////////////////////////////////////////////

// Statics
static float FLOODFILL_SEARCH_RADIUS = 65.0f;
static float FLOODFILL_SEARCH_RADIUS_LARGE = 115.0f;
static float MAX_TIME_BETWEEN_SEARCHES = 7.5f;
static float MIN_DISTANCE_TO_RESTART_SEARCH = 10.0f;

CFindNearestCarNodeHelper::CFindNearestCarNodeHelper()
: m_pathHandle(PATH_HANDLE_NULL)
, m_vNearestCarNodePosition(Vector3::ZeroType)
, m_vLastSearchPosition(Vector3::ZeroType)
, m_vSearchPosition(Vector3::ZeroType)
, m_NearestCarNode()
, m_fTimeSinceLastSearch(0.0f)
, m_bStartNewSearch(false)
, m_bActive(false)
{

}

CFindNearestCarNodeHelper::~CFindNearestCarNodeHelper()
{

}

bool CFindNearestCarNodeHelper::GetNearestCarNode(CNodeAddress& rNode) const
{
	//Ensure we are active.
	if(!m_bActive)
	{
		return false;
	}

	//Check if the search succeeded.
	if(GetState() == State_Success)
	{
		rNode = m_NearestCarNode;
		return true;
	}
	//Check if the search failed.
	else if(GetState() == State_Failed)
	{
		//Check if the nearest car node is not empty.
		if(!m_NearestCarNode.IsEmpty())
		{
			rNode = m_NearestCarNode;
			return true;
		}
	}

	return false;
}

bool CFindNearestCarNodeHelper::GetNearestCarNodePosition(Vector3& vNodePosition) const
{
	//Ensure we are active.
	if(!m_bActive)
	{
		return false;
	}

	//Check if the search succeeded.
	if(GetState() == State_Success)
	{
		vNodePosition = m_vNearestCarNodePosition;
		return true;
	}
	//Check if the search failed.
	else if(GetState() == State_Failed)
	{
		//Check if the nearest car node is not empty.
		if(!m_NearestCarNode.IsEmpty())
		{
			//Check if the path node is valid.
			const CPathNode* pNode = ThePaths.FindNodePointerSafe(m_NearestCarNode);
			if(pNode)
			{
				//Load the position.
				pNode->GetCoors(vNodePosition);
				return true;
			}
		}
	}

	return false;
}

void CFindNearestCarNodeHelper::SetSearchPosition(const Vector3& vSearchPosition)
{
	m_vSearchPosition = vSearchPosition;
	m_bStartNewSearch = true;
}

void CFindNearestCarNodeHelper::Reset()
{
	m_bActive = false;
}

bool CFindNearestCarNodeHelper::GenerateRandomPositionNearRoadNode(const GenerateRandomPositionNearRoadNodeInput& rInput, Vec3V_InOut vPosition)
{
	//Ensure the node is valid.
	if(!taskVerifyf(!rInput.m_Node.IsEmpty(), "The node is invalid."))
	{
		return false;
	}

	//Generate a random distance.
	float fDistance = fwRandom::GetRandomNumberInRange(0.0f, rInput.m_fMaxDistance);

	//Generate a random direction.
	float fAngle = fwRandom::GetRandomNumberInRange(-rInput.m_fMaxAngle, rInput.m_fMaxAngle);
	Vector2 vDirection(cos(fAngle), sin(fAngle));

	//Create the input for finding a node in a direction.
	CPathFind::FindNodeInDirectionInput input(rInput.m_Node, Vector3(vDirection, Vector2::kXY), fDistance);
	input.m_bIncludeSwitchedOffNodes = true;
	input.m_bIncludeDeadEndNodes = true;
	input.m_bCanFollowIncomingLinks = true;
	input.m_bCanFollowOutgoingLinks = true;

	//Find a node in the direction.
	CPathFind::FindNodeInDirectionOutput output;
	if(!ThePaths.FindNodeInDirection(input, output))
	{
		return false;
	}

	//Ensure the node is valid.
	const CPathNode* pNode = ThePaths.FindNodePointerSafe(output.m_Node);
	if(!pNode)
	{
		return false;
	}

	//Ensure the previous node is valid.
	const CPathNode* pPreviousNode = ThePaths.FindNodePointerSafe(output.m_PreviousNode);
	if(!pPreviousNode)
	{
		return false;
	}

	//Grab the node position.
	Vector3 vNodePosition;
	pNode->GetCoors(vNodePosition);

	//Grab the previous node position.
	Vector3 vPreviousNodePosition;
	pPreviousNode->GetCoors(vPreviousNodePosition);

	//Generate a random weight.
	float fValue = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);

	//Calculate the position.
	vPosition = VECTOR3_TO_VEC3V(Lerp(fValue, vNodePosition, vPreviousNodePosition));

	return true;
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_SearchNotStarted)
			FSM_OnUpdate
			return SearchNotStarted_OnUpdate();	

		FSM_State(State_StartingSearch)
			FSM_OnUpdate
			return StartingSearch_OnUpdate();	

		FSM_State(State_FloodFillSearchInProgress)
			FSM_OnUpdate
			return FloodFillSearchInProgress_OnUpdate();

		FSM_State(State_StartingLargerSearch)
			FSM_OnUpdate
			return StartingLargerSearch_OnUpdate();

		FSM_State(State_LargerFloodFillSearchInProgress)
			FSM_OnUpdate
			return LargerFloodFillSearchInProgress_OnUpdate();

		FSM_State(State_Failed)
			FSM_OnUpdate
			return Failed_OnUpdate();

		FSM_State(State_Success)
			FSM_OnUpdate
			return Success_OnUpdate();

		FSM_State(State_Finished)
			FSM_OnUpdate
			return FSM_Quit;

	FSM_End
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::SearchNotStarted_OnUpdate()
{
	if (m_bStartNewSearch)
	{
		m_bActive = true;
		SetState(State_StartingSearch);
	}
	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::StartingSearch_OnUpdate()
{
	// Reset the start new search variable
	m_bStartNewSearch = false;

	// Begin a search at the current position
	m_vLastSearchPosition = m_vSearchPosition;
	// Requests a flood-fill search, to find the closest accessible carnode within the given radius
	m_pathHandle = CPathServer::RequestClosestCarNodeSearch(m_vLastSearchPosition, FLOODFILL_SEARCH_RADIUS);

	if (m_pathHandle != PATH_HANDLE_NULL)
	{
		SetState(State_FloodFillSearchInProgress);
	}
	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::FloodFillSearchInProgress_OnUpdate()
{
	// Wait for the search results
	aiAssert(m_pathHandle != PATH_HANDLE_NULL);
	{
		EPathServerErrorCode pathCode = CPathServer::GetClosestCarNodeSearchResultAndClear(m_pathHandle, m_vNearestCarNodePosition);
		if (pathCode == PATH_FOUND)
		{
			//Set the closest car node.
			m_NearestCarNode = ThePaths.FindNodeClosestToCoors(m_vNearestCarNodePosition);

			SetState(State_Success);
			m_fTimeSinceLastSearch = 0.0f;
		}
		else if (pathCode != PATH_STILL_PENDING)
		{
			CPathServer::CancelRequest(m_pathHandle);
			SetState(State_StartingLargerSearch);
		}
	}
	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::StartingLargerSearch_OnUpdate()
{
	// Begin a search at the current position
	m_vLastSearchPosition = m_vSearchPosition;
	// Requests a flood-fill search, to find the closest accessible carnode within the given radius
	m_pathHandle = CPathServer::RequestClosestCarNodeSearch(m_vLastSearchPosition, FLOODFILL_SEARCH_RADIUS_LARGE);
	if (m_pathHandle != PATH_HANDLE_NULL)
	{
		SetState(State_LargerFloodFillSearchInProgress);
	}
	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::LargerFloodFillSearchInProgress_OnUpdate()
{
	// Wait for the search results
	aiAssert(m_pathHandle != PATH_HANDLE_NULL);
	{
		EPathServerErrorCode pathCode = CPathServer::GetClosestCarNodeSearchResultAndClear(m_pathHandle, m_vNearestCarNodePosition);
		if (pathCode == PATH_FOUND)
		{
			//Set the closest car node.
			m_NearestCarNode = ThePaths.FindNodeClosestToCoors(m_vNearestCarNodePosition);

			SetState(State_Success);
			m_fTimeSinceLastSearch = 0.0f;
		}
		else if (pathCode != PATH_STILL_PENDING)
		{
			//Set the closest car node.
			m_NearestCarNode = ThePaths.FindNodeClosestToCoors(m_vSearchPosition);

			CPathServer::CancelRequest(m_pathHandle);
			SetState(State_Failed);
			m_fTimeSinceLastSearch = 0.0f;
		}
	}
	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::Failed_OnUpdate()
{
	if (!m_bActive)
	{
		SetState(State_SearchNotStarted);
		return FSM_Continue;
	}

	//Increase the time since the last search.
	m_fTimeSinceLastSearch += fwTimer::GetTimeStep();

	//Check if we should start a new search.
	if(ShouldStartNewSearch())
	{
		//Start a new search.
		SetState(State_StartingSearch);
	}

	return FSM_Continue;
}

CTaskHelperFSM::FSM_Return CFindNearestCarNodeHelper::Success_OnUpdate()
{
	if (!m_bActive)
	{
		SetState(State_SearchNotStarted);
		return FSM_Continue;
	}

	//Increase the time since the last search.
	m_fTimeSinceLastSearch += fwTimer::GetTimeStep();

	//Check if we should start a new search.
	if(ShouldStartNewSearch())
	{
		//Start a new search.
		SetState(State_StartingSearch);
	}

	return FSM_Continue;
}

bool CFindNearestCarNodeHelper::ShouldStartNewSearch() const
{
	//Ensure we can start a new search.
	if(!m_bStartNewSearch)
	{
		return false;
	}

	//Check if the time since the last search has exceeded the threshold.
	if(m_fTimeSinceLastSearch > MAX_TIME_BETWEEN_SEARCHES)
	{
		return true;
	}

	//Check if the distance has exceeded the threshold.
	if(m_vSearchPosition.Dist2(m_vLastSearchPosition) > MIN_DISTANCE_TO_RESTART_SEARCH * MIN_DISTANCE_TO_RESTART_SEARCH)
	{
		return true;
	}

	return false;
}

#if !__FINAL
const char* CFindNearestCarNodeHelper::GetStateName() const
{
	switch (GetState())
	{
		case State_SearchNotStarted :					return "State_SearchNotStarted";
		case State_StartingSearch :						return "State_StartingSearch";
		case State_StartingLargerSearch :				return "State_StartingLargerSearch";
		case State_LargerFloodFillSearchInProgress :	return "State_LargerFloodFillSearchInProgress";
		case State_Failed :								return "State_Failed";
		case State_Success :							return "State_Success";
		case State_Finished :							return "State_Finished";
	}

	return "State_Invalid";
}

void CFindNearestCarNodeHelper::Debug(s32  BANK_ONLY(iIndentLevel)) const
{
#if __BANK
	if (CTaskHelperFSM::ms_bRenderStateDebug)
	{
		CTaskHelperFunctions::AddDebugStateNameWithIndent(Color_cyan, iIndentLevel, GetName(), GetStateName());
	}
#endif

	if (GetState() == State_Success )
	{
		CVectorMap::DrawCircle(m_vNearestCarNodePosition, 3.0f, Color_green, false);
		CVectorMap::DrawCircle(m_vSearchPosition, 3.0f, Color_blue, false);
		CVectorMap::DrawLine(m_vSearchPosition, m_vNearestCarNodePosition, Color_blue, Color_green);
	}
	// The search has failed, return the nearest node to the position
	else if (GetState() == State_Failed )
	{
		CVectorMap::DrawCircle(m_vSearchPosition, 3.0f, Color_red, false);
	}
	else
	{
		CVectorMap::DrawCircle(m_vSearchPosition, 3.0f, Color_orange, false);
	}
}
#endif // !__FINAL

////////////////////////////////////////////////////////////////////////////////

void CReactToBeingJackedHelper::CheckForAndMakeOccupantsReactToBeingJacked(const CPed& rJacker, CEntity& rEntity, int iEntryPoint, bool bIgnoreIfPedInSeatIsLaw)
{
	if (rJacker.GetPedConfigFlag(CPED_CONFIG_FLAG_IsHandCuffed))
	{
		return;
	}

	//Check if the flag has been set to not jack the ped
	CTaskEnterVehicle* pTaskEnterVehicle = static_cast<CTaskEnterVehicle *>(
		rJacker.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_ENTER_VEHICLE));
	if (pTaskEnterVehicle && pTaskEnterVehicle->IsFlagSet(CVehicleEnterExitFlags::DontJackAnyone))
	{
		return;
	}

	//Ensure the entry point is valid.
	if(iEntryPoint < 0)
	{
		return;
	}

	const CSeatManager* pSeatManager = NULL;//m_pVehicle->GetSeatManager();
	s32 iDirectSeatIndex = -1;// m_pVehicle->GetEntryExitPoint(m_iEntryPointIndex)->GetSeat(SA_directAccessSeat);
	if (rEntity.GetIsTypePed())
	{
		iDirectSeatIndex = static_cast<const CPed*>( &rEntity )->GetPedModelInfo()->GetModelSeatInfo()->GetEntryExitPoint(iEntryPoint)->GetSeat(SA_directAccessSeat);
		pSeatManager = static_cast<const CPed*>( &rEntity )->GetSeatManager();
	} 
	else if (rEntity.GetIsTypeVehicle())
	{
		pSeatManager = static_cast<const CVehicle*>( &rEntity )->GetSeatManager();
		iDirectSeatIndex = static_cast<const CVehicle*>( &rEntity )->GetEntryExitPoint(iEntryPoint)->GetSeat(SA_directAccessSeat);
	}
	else return;

	if (iDirectSeatIndex > -1 && rEntity.GetIsTypeVehicle()) // no peds for now
	{
		//If there is no ped being jacked AND the jacker was the last ped in this seat, it is probably just an ambient ped
		//getting back in their car after a weird event -- no need for the old passengers to flee from them!
		CPed* pJackedPed = pSeatManager->GetPedInSeat(iDirectSeatIndex);
		if(!pJackedPed && (&rJacker == pSeatManager->GetLastPedInSeat(iDirectSeatIndex)))
		{
			return;
		}

		// If we are told to ignore when the direct entry ped is a law enforcement and said ped is law, then return
		if(bIgnoreIfPedInSeatIsLaw && pJackedPed && pJackedPed->IsLawEnforcementPed())
		{
			return;
		}

		for (s32 i=0; i<pSeatManager->GetMaxSeats(); i++)
		{
			CPed* pPedInSeat = pSeatManager->GetPedInSeat(i);
			if (pPedInSeat && !pPedInSeat->IsNetworkClone() && (!pPedInSeat->IsLocalPlayer() || rJacker.IsPlayer()) && !pPedInSeat->IsDead() && !pPedInSeat->GetPedConfigFlag(CPED_CONFIG_FLAG_PreventPedFromReactingToBeingJacked))
			{
				//Check if we should react.
				bool bReact = (!pPedInSeat->GetPedIntelligence()->IsFriendlyWith(rJacker) ||
					(pPedInSeat->PopTypeIsRandom() && rJacker.IsPlayer()));
				if(bReact)
				{
					//Note that our vehicle is being jacked.
					CEventPedJackingMyVehicle event(static_cast<CVehicle *>(&rEntity), pJackedPed, &rJacker);
					pPedInSeat->GetPedIntelligence()->AddEvent(event);
				}
			}
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

CCoverPointStatusHelper::CCoverPointStatusHelper()
: m_ProbeResults(1)
, m_CoverResults(20)
, m_VantageResults(20)
{
	Reset();
}

CCoverPointStatusHelper::~CCoverPointStatusHelper()
{

}

void CCoverPointStatusHelper::Reset()
{
	//Clear the cover point.
	m_pCoverPoint = NULL;

	//Clear the probe values.
	m_fProbedGroundHeight = 0.0f;
	m_bHasProbedGroundHeight = false;

	//Clear the status.
	m_uStatus = 0;

	//Clear the failed flag.
	m_bFailed = false;

	//Set the reset flag.
	m_bReset = true;

	//Move back to the waiting state.
	Update();
}

void CCoverPointStatusHelper::Start(CCoverPoint* pCoverPoint)
{
	//Assert that the cover point is valid.
	aiAssert(pCoverPoint);

	//Assert that the state is valid.
	aiAssert(GetState() == State_Waiting);

	//Set the cover point.
	m_pCoverPoint = pCoverPoint;
}

#if !__FINAL
const char* CCoverPointStatusHelper::GetStateName() const
{
	switch(GetState())
	{
		case State_Waiting:			return "State_Waiting";
		case State_CheckProbe:		return "State_CheckProbe";
		case State_CheckCover:		return "State_CheckCover";
		case State_CheckVantage:	return "State_CheckVantage";
		case State_Finished:		return "State_Finished";
	}

	return "State_Invalid";
}
#endif

bool CCoverPointStatusHelper::IsCoverPointValid() const
{
	if(!m_pCoverPoint)
	{
		return false;
	}

	if(m_pCoverPoint->GetType() == CCoverPoint::COVTYPE_NONE)
	{
		return false;
	}

	Vector3 vCoverPosition;
	if(!m_pCoverPoint->GetCoverPointPosition(vCoverPosition))
	{
		return false;
	}

	return true;
}

CTaskHelperFSM::FSM_Return CCoverPointStatusHelper::UpdateFSM(const s32 iState, const FSM_Event iEvent)
{
	FSM_Begin

		FSM_State(State_Waiting)
			FSM_OnUpdate
				return Waiting_OnUpdate();

		FSM_State(State_CheckProbe)
			FSM_OnEnter
				CheckProbe_OnEnter();
			FSM_OnUpdate
				return CheckProbe_OnUpdate();
			FSM_OnExit
				CheckProbe_OnExit();

		FSM_State(State_CheckCover)
			FSM_OnEnter
				CheckCover_OnEnter();
			FSM_OnUpdate
				return CheckCover_OnUpdate();
			FSM_OnExit
				CheckCover_OnExit();

		FSM_State(State_CheckVantage)
			FSM_OnEnter
				CheckVantage_OnEnter();
			FSM_OnUpdate
				return CheckVantage_OnUpdate();
			FSM_OnExit
				CheckVantage_OnExit();

		FSM_State(State_Finished)
			FSM_OnEnter
				Finished_OnEnter();
			FSM_OnUpdate
				return Finished_OnUpdate();

	FSM_End
}

CTaskHelperFSM::FSM_Return CCoverPointStatusHelper::Waiting_OnUpdate()
{
	m_bReset = false;

	if(IsCoverPointValid())
	{
		SetState(State_CheckProbe);
	}

	return FSM_Continue;
}

void CCoverPointStatusHelper::CheckProbe_OnEnter()
{
	// If the coverpoint was not generated with the navmesh
	// and is currently marked as clear
	if( m_pCoverPoint->GetType() != CCoverPoint::COVTYPE_POINTONMAP /*&& (m_CoverPointStatus & COVSTATUS_Clear) > 0*/ )
	{
		Vector3 vCoverPosition;
		m_pCoverPoint->GetCoverPointPosition(vCoverPosition);

		// Use a simple line probe down from just above to just below cover position.
		dev_float SURFACE_TEST_BELOW_HEIGHT = 0.75f;
		float fProbeDownDistance = SURFACE_TEST_BELOW_HEIGHT;
		if( m_pCoverPoint->GetType() == CCoverPoint::COVTYPE_SCRIPTED )
		{
			dev_float SURFACE_TEST_BELOW_HEIGHT_SCRIPTED = 1.50f; // scripters are placing points by reading ped coords, so probe down must be large to compensate
			fProbeDownDistance = SURFACE_TEST_BELOW_HEIGHT_SCRIPTED;
		}
		WorldProbe::CShapeTestProbeDesc probeDesc;

		dev_float SURFACE_TEST_ABOVE_HEIGHT = 0.75f;
		probeDesc.SetStartAndEnd(vCoverPosition + Vector3(0.0f,0.0f,SURFACE_TEST_ABOVE_HEIGHT), vCoverPosition + Vector3(0.0f,0.0f,-fProbeDownDistance));
		// for the ground testing, include objects, vehicles, and map types
		s32 iGroundCheckTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_ALL_MAP_TYPES;
		probeDesc.SetIncludeFlags(iGroundCheckTypeFlags);
		probeDesc.SetExcludeEntity(m_pCoverPoint->GetEntity());
		// looking for the first hit, if any
		probeDesc.SetResultsStructure(&m_ProbeResults);

		// Is there surface at the cover position?
		WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	}
}

CTaskHelperFSM::FSM_Return CCoverPointStatusHelper::CheckProbe_OnUpdate()
{
	if(m_bReset)
	{
		SetState(State_Waiting);
	}
	else if(!IsCoverPointValid())
	{
		m_bFailed = true;

		SetState(State_Finished);
	}
	else if(!m_ProbeResults.GetWaitingOnResults())
	{
		if(m_ProbeResults.GetResultsReady())
		{
			Vector3 vCoverPosition;
			m_pCoverPoint->GetCoverPointPosition(vCoverPosition);

			// Check for a support surface beneath the coverpoint
			bool bCoverHasValidSupportSurface = true;

			bool bCoverPositionSurface = (m_ProbeResults.GetNumHits() > 0);
			if(bCoverPositionSurface)
			{
				// Check if the surface Z height is too different from the coverpoint height to be valid
				m_fProbedGroundHeight = m_ProbeResults[0].GetHitPosition().z;
				m_bHasProbedGroundHeight = true;
				dev_float SURFACE_TEST_HEIGHT_TOLERANCE = 0.50f;
				if( ABS(m_fProbedGroundHeight - vCoverPosition.z) > SURFACE_TEST_HEIGHT_TOLERANCE )
				{
					// @TEMP HACK while we decide what to do about vehicles on slopes all blocking from this test (dmorales)
					//bCoverHasValidSupportSurface = false;
				}
			}
			else // nothing was hit in the test
			{
				// @TEMP HACK while we decide what to do about vehicles on slopes all blocking from this test (dmorales)
				//bCoverHasValidSupportSurface = false;
			}

			// If the cover does not have valid surface
			if( !bCoverHasValidSupportSurface )
			{
				// consider this coverpoint position blocked
				m_uStatus |= CCoverPoint::COVSTATUS_PositionBlocked;
				m_pCoverPoint->SetStatus(CCoverPoint::COVSTATUS_PositionBlocked);
#if __DEV
				// This bool controls whether we render or not
				TUNE_GROUP_BOOL(COVER_DEBUG, RENDER_PROBE_BLOCKING_TEST, false);

				bool bRenderDebugSphere = RENDER_PROBE_BLOCKING_TEST;
				// If the coverpoint was created by script, warn the scripts
				if( m_pCoverPoint->GetType() == CCoverPoint::COVTYPE_SCRIPTED )
				{
					bRenderDebugSphere = true;

					if( bCoverPositionSurface )
					{
						scriptAssertf(0, "SCRIPTED cover point 0x%p at <<%.2f,%.2f,%.2f>> is floating above surface (Z height should be: %.2f)", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z, m_fProbedGroundHeight);
					}
					// Scripted points can be created before the map finishes loading in, so don't spam as the point may be fine once collision loads (dmorales)
					//else
					//{
					//scriptAssertf(0, "SCRIPTED cover point 0x%X at <<%.2f,%.2f,%.2f>> is hovering in space (probe down %.2f from cover hit nothing!)", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z, fProbeDownDistance);
					//}
				}
				if( bRenderDebugSphere )
				{
					dev_float SURFACE_TEST_HEIGHT_TOLERANCE = 0.50f;
					CTask::ms_debugDraw.AddSphere(RCC_VEC3V(vCoverPosition), SURFACE_TEST_HEIGHT_TOLERANCE, Color_red, 1000, 0, false);
				}
#endif // __DEV

				SetState(State_Finished);
				return FSM_Continue;
			}
		}

		SetState(State_CheckCover);
	}

	return FSM_Continue;
}

void CCoverPointStatusHelper::CheckProbe_OnExit()
{
	m_ProbeResults.Reset();
}

void CCoverPointStatusHelper::CheckCover_OnEnter()
{
	Vector3 vCoverPosition;
	m_pCoverPoint->GetCoverPointPosition(vCoverPosition);

	dev_float COVER_TEST_LOW_HEIGHT = 0.35f;
	dev_float COVER_TEST_HIGH_HEIGHT = 1.5f;

	// Define the cover position capsule start and end points
	m_vCoverCapsuleStart = vCoverPosition;
	m_vCoverCapsuleEnd = vCoverPosition;
	if( m_bHasProbedGroundHeight )
	{
		m_vCoverCapsuleStart.z = m_fProbedGroundHeight + COVER_TEST_LOW_HEIGHT;
		m_vCoverCapsuleEnd.z = m_fProbedGroundHeight + COVER_TEST_HIGH_HEIGHT;
	}
	else
	{
		m_vCoverCapsuleStart.z += COVER_TEST_LOW_HEIGHT;
		m_vCoverCapsuleEnd.z += COVER_TEST_HIGH_HEIGHT;
	}

	// Define the types to test for
	s32 iCoverCapsuleTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE;
	// Map collision points only check against objects and vehicles, the point is already assumed to be correct in the world
	if( m_pCoverPoint->GetType() != CCoverPoint::COVTYPE_POINTONMAP )
	{
		iCoverCapsuleTypeFlags |= ArchetypeFlags::GTA_ALL_MAP_TYPES;
	}

	// Set up the cover capsule desc
	WorldProbe::CShapeTestCapsuleDesc coverCapsuleDesc;

	dev_float COVER_TEST_CAPSULE_RADIUS = 0.2f;
	coverCapsuleDesc.SetCapsule(m_vCoverCapsuleStart, m_vCoverCapsuleEnd, COVER_TEST_CAPSULE_RADIUS);
	coverCapsuleDesc.SetIsDirected(true);
	coverCapsuleDesc.SetDoInitialSphereCheck(true);
	coverCapsuleDesc.SetIncludeFlags(iCoverCapsuleTypeFlags); 
	coverCapsuleDesc.SetExcludeEntity(m_pCoverPoint->GetEntity());
	coverCapsuleDesc.SetResultsStructure(&m_CoverResults);

	WorldProbe::GetShapeTestManager()->SubmitTest(coverCapsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
}

CTaskHelperFSM::FSM_Return CCoverPointStatusHelper::CheckCover_OnUpdate()
{
	if(m_bReset)
	{
		SetState(State_Waiting);
	}
	else if(!IsCoverPointValid())
	{
		m_bFailed = true;

		SetState(State_Finished);
	}
	else if(!m_CoverResults.GetResultsWaitingOrReady())
	{
		m_bFailed = true;

		SetState(State_Finished);
	}
	else if(m_CoverResults.GetResultsReady())
	{
		// Is the cover position blocked?
		bool bCoverPositionBlocked = (m_CoverResults.GetNumHits() > 0);

		// If the cover is blocked in some way
		if(bCoverPositionBlocked)
		{
#if __DEV
			// Check if this is a scripted cover point blocked by geometry
			bool bIsScriptPointBlockedByMapGeometry = false;

			// If the cover position is obstructed, warn the scripts
			if ( bCoverPositionBlocked && m_pCoverPoint->GetType() == CCoverPoint::COVTYPE_SCRIPTED )
			{
				for (int i = 0; i < m_CoverResults.GetNumHits(); ++i)
				{
					const CEntity* pEntity = CPhysics::GetEntityFromInst(m_CoverResults[i].GetHitInst());
					if (pEntity && (pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeMLO()) )
					{
						bIsScriptPointBlockedByMapGeometry = true;

						Vector3 vCoverPosition;
						m_pCoverPoint->GetCoverPointPosition(vCoverPosition);

						scriptAssertf(0, "SCRIPTED cover point 0x%p at <<%.2f,%.2f,%.2f>> is being blocked by map geometry", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z);
					}
				}
			}

			// This bool controls whether we render or not
			TUNE_GROUP_BOOL(COVER_DEBUG, RENDER_COVER_BLOCKING_TEST, false);

			// If this is a script point blocked by geometry
			// OR we are debug rendering the tests
			if (bIsScriptPointBlockedByMapGeometry || RENDER_COVER_BLOCKING_TEST)
			{
				if( bCoverPositionBlocked )
				{
					dev_float COVER_TEST_CAPSULE_RADIUS = 0.2f;
					CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(m_vCoverCapsuleStart), RCC_VEC3V(m_vCoverCapsuleEnd), COVER_TEST_CAPSULE_RADIUS, Color_red, 1000, 0, false);
				}
			}
#endif // __DEV

			// cover is blocked by something
			m_uStatus |= CCoverPoint::COVSTATUS_PositionBlocked;
			m_pCoverPoint->SetStatus(CCoverPoint::COVSTATUS_PositionBlocked);

			SetState(State_Finished);
		}
		else
		{
			// If this is corner cover with a step out position, perform additional tests for clearance
			bool bDoCornerCoverTests = ( m_pCoverPoint->GetUsage() == CCoverPoint::COVUSE_WALLTOLEFT || m_pCoverPoint->GetUsage() == CCoverPoint::COVUSE_WALLTORIGHT );
			if(bDoCornerCoverTests)
			{
				SetState(State_CheckVantage);
			}
			else
			{
				// cover is clear
				m_uStatus |= CCoverPoint::COVSTATUS_Clear;
				m_pCoverPoint->SetStatus(CCoverPoint::COVSTATUS_Clear);

#if __DEV
				// This bool controls whether we render or not
				TUNE_GROUP_BOOL(COVER_DEBUG, RENDER_COVER_CLEAR_TEST, false);
				if (RENDER_COVER_CLEAR_TEST)
				{
					dev_float COVER_TEST_CAPSULE_RADIUS = 0.2f;
					CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(m_vCoverCapsuleStart), RCC_VEC3V(m_vCoverCapsuleEnd), COVER_TEST_CAPSULE_RADIUS, Color_green, 1000, 0, false);
				}
#endif // __DEV

				SetState(State_Finished);
			}
		}
	}

	return FSM_Continue;
}

void CCoverPointStatusHelper::CheckCover_OnExit()
{
	m_CoverResults.Reset();
}

void CCoverPointStatusHelper::CheckVantage_OnEnter()
{
	// Set up the vantage position capsule if appropriate
	WorldProbe::CShapeTestCapsuleDesc vantageCapsuleDesc;
	Vector3 vVantagePosition;

	Vector3 vCoverPosition;
	m_pCoverPoint->GetCoverPointPosition(vCoverPosition);

	dev_float VANTAGE_TEST_HEIGHT = 0.9f;
	dev_float VANTAGE_TEST_CAPSULE_RADIUS = 0.30f;

	// Set up the probe from the cover position to the vantage position
	Vector3 vCoverDirection = VEC3V_TO_VECTOR3(m_pCoverPoint->GetCoverDirectionVector(NULL));
	CCover::CalculateVantagePointForCoverPoint(m_pCoverPoint, vCoverDirection, vCoverPosition, vVantagePosition);
	Vector3 vVantageDir = vVantagePosition - vCoverPosition;
	vVantageDir.z = 0.0f;
	vVantageDir.NormalizeSafe();
	m_vVantageCapsuleStart = vCoverPosition;
	m_vVantageCapsuleStart += (vVantageDir * VANTAGE_TEST_CAPSULE_RADIUS);
	m_vVantageCapsuleEnd = vVantagePosition;
	if( m_bHasProbedGroundHeight )
	{
		m_vVantageCapsuleStart.z = m_fProbedGroundHeight + VANTAGE_TEST_HEIGHT;
		m_vVantageCapsuleEnd.z = m_fProbedGroundHeight + VANTAGE_TEST_HEIGHT;
	}
	else
	{
		m_vVantageCapsuleStart.z = vCoverPosition.z + VANTAGE_TEST_HEIGHT;
		m_vVantageCapsuleEnd.z = vCoverPosition.z + VANTAGE_TEST_HEIGHT;
	}
	vantageCapsuleDesc.SetCapsule(m_vVantageCapsuleStart, m_vVantageCapsuleEnd, VANTAGE_TEST_CAPSULE_RADIUS);
	vantageCapsuleDesc.SetIsDirected(true);
	vantageCapsuleDesc.SetDoInitialSphereCheck(true);
	// for the vantage testing, include objects, vehicles, and map types
	s32 iVantageCapsuleTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_ALL_MAP_TYPES;
	vantageCapsuleDesc.SetIncludeFlags(iVantageCapsuleTypeFlags);
	// exclude the cover user, if any
	vantageCapsuleDesc.SetExcludeEntity(m_pCoverPoint->GetEntity());
	vantageCapsuleDesc.SetResultsStructure(&m_VantageResults);

	WorldProbe::GetShapeTestManager()->SubmitTest(vantageCapsuleDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
}

CTaskHelperFSM::FSM_Return CCoverPointStatusHelper::CheckVantage_OnUpdate()
{
	if(m_bReset)
	{
		SetState(State_Waiting);
	}
	else if(!IsCoverPointValid())
	{
		m_bFailed = true;

		SetState(State_Finished);
	}
	else if(!m_VantageResults.GetResultsWaitingOrReady())
	{
		m_bFailed = true;

		SetState(State_Finished);
	}
	else if(m_VantageResults.GetResultsReady())
	{
		bool bVantagePositionBlocked = (m_VantageResults.GetNumHits() > 0);
		if(bVantagePositionBlocked)
		{
#if __DEV
			// Check if this is a scripted cover point blocked by geometry
			bool bIsScriptPointBlockedByMapGeometry = false;

			// If the cover vantage position is obstructed, warn the scripts
			if ( bVantagePositionBlocked && m_pCoverPoint->GetType() == CCoverPoint::COVTYPE_SCRIPTED )
			{
				for (int i = 0; i < m_VantageResults.GetNumHits(); ++i)
				{
					const CEntity* pEntity = CPhysics::GetEntityFromInst(m_VantageResults[i].GetHitInst());
					if (pEntity && (pEntity->GetIsTypeBuilding() || pEntity->GetIsTypeMLO()) )
					{
						Vector3 vCoverPosition;
						m_pCoverPoint->GetCoverPointPosition(vCoverPosition);

						bIsScriptPointBlockedByMapGeometry = true;
						scriptAssertf(0, "SCRIPTED cover point 0x%p at <<%.2f,%.2f,%.2f>> has vantage position blocked by map geometry", this, vCoverPosition.x, vCoverPosition.y, vCoverPosition.z);
					}
				}
			}

			// This bool controls whether we render or not
			TUNE_GROUP_BOOL(COVER_DEBUG, RENDER_VANTAGE_BLOCKING_TEST, false);

			// If this is a script point blocked by geometry
			// OR we are debug rendering the tests
			if (bIsScriptPointBlockedByMapGeometry || RENDER_VANTAGE_BLOCKING_TEST)
			{
				if( bVantagePositionBlocked )
				{
					dev_float VANTAGE_TEST_CAPSULE_RADIUS = 0.35f;
					CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(m_vVantageCapsuleStart), RCC_VEC3V(m_vVantageCapsuleEnd), VANTAGE_TEST_CAPSULE_RADIUS, Color_red, 1000, 0, false);
				}
			}
#endif // __DEV

			// cover is blocked by something
			m_uStatus |= CCoverPoint::COVSTATUS_PositionBlocked;
			m_pCoverPoint->SetStatus(CCoverPoint::COVSTATUS_PositionBlocked);
		}
		else
		{
			// cover is clear
			m_uStatus |= CCoverPoint::COVSTATUS_Clear;
			m_pCoverPoint->SetStatus(CCoverPoint::COVSTATUS_Clear);

#if __DEV
			// This bool controls whether we render or not
			TUNE_GROUP_BOOL(COVER_DEBUG, RENDER_VANTAGE_CLEAR_TEST, false);
			if (RENDER_VANTAGE_CLEAR_TEST)
			{
				dev_float VANTAGE_TEST_CAPSULE_RADIUS = 0.35f;
				CTask::ms_debugDraw.AddCapsule(RCC_VEC3V(m_vVantageCapsuleStart), RCC_VEC3V(m_vVantageCapsuleEnd), VANTAGE_TEST_CAPSULE_RADIUS, Color_green, 1000, 0, false);
			}
#endif // __DEV
		}

		SetState(State_Finished);
	}

	return FSM_Continue;
}

void CCoverPointStatusHelper::CheckVantage_OnExit()
{
	m_VantageResults.Reset();
}

void CCoverPointStatusHelper::Finished_OnEnter()
{
	m_bReset = false;
}

CTaskHelperFSM::FSM_Return CCoverPointStatusHelper::Finished_OnUpdate()
{
	if(m_bReset)
	{
		SetState(State_Waiting);
	}

	return FSM_Continue;
}

////////////////////////////////////////////////////////////////////////////////

CPed* CNearestPedInCombatHelper::Find(const CPed& rTarget, fwFlags8 uOptionFlags)
{
	//Iterate over the nearby peds.
	CEntityScannerIterator pedList = rTarget.GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = pedList.GetFirst(); pEntity != NULL; pEntity = pedList.GetNext())
	{
		//Grab the ped.
		CPed* pPed = static_cast<CPed *>(pEntity);

		//Check if the ped is required to be law enforcement, and is not law enforcement.
		if(uOptionFlags.IsFlagSet(OF_MustBeLawEnforcement) && !pPed->IsLawEnforcementPed())
		{
			continue;
		}

		//Check if the ped is required to be on foot, and is not on foot.
		if(uOptionFlags.IsFlagSet(OF_MustBeOnFoot) && !pPed->GetIsOnFoot())
		{
			continue;
		}

		//Check if the ped is required to be on-screen, and is not on-screen.
		if(uOptionFlags.IsFlagSet(OF_MustBeOnScreen)&& !pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_VisibleOnScreen))
		{
			continue;
		}

		//Ensure the ped is running combat.
		CTaskCombat* pTask = static_cast<CTaskCombat *>(
			pPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COMBAT));
		if(!pTask)
		{
			continue;
		}

		//Ensure the target matches.
		if(pTask->GetPrimaryTarget() != &rTarget)
		{
			continue;
		}

		return pPed;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

void CPositionFixupHelper::Apply(CPed& rPed, Vec3V_In vTarget, const crClip& rClip, float fPhase, float fMaxPositionChangePerSecond, float fTimeStep, fwFlags8 uFlags)
{
	//Calculate the remaining translation from the clip.
	Vec3V vRemainingTranslation = VECTOR3_TO_VEC3V(fwAnimHelpers::GetMoverTrackDisplacement(rClip, fPhase, 1.0f));

	//Calculate the end position.
	Vec3V vEnd = rPed.GetTransform().Transform(vRemainingTranslation);

	//Generate a vector from the end to the target.
	Vec3V vEndToTarget = Subtract(vTarget, vEnd);

	//Check if we should ignore Z.
	if(uFlags.IsFlagSet(F_IgnoreZ))
	{
		vEndToTarget.SetZ(ScalarV(V_ZERO));
	}

	//Calculate the additional velocity.
	Vec3V vMaxAdditionalVelocity = NormalizeFastSafe(vEndToTarget, Vec3V(V_ZERO));
	vMaxAdditionalVelocity = Scale(vMaxAdditionalVelocity, ScalarVFromF32(fMaxPositionChangePerSecond));
	Vec3V vIdealVelocity = vEndToTarget;
	vIdealVelocity = Scale(vIdealVelocity, Invert(ScalarVFromF32(fTimeStep)));
	Vec3V vAdditionalVelocity = IsLessThanAll(MagSquared(vIdealVelocity), MagSquared(vMaxAdditionalVelocity)) ?
		vIdealVelocity : vMaxAdditionalVelocity;

	//Set the desired velocity.
	Vec3V vDesiredVelocity = VECTOR3_TO_VEC3V(rPed.GetDesiredVelocity());
	vDesiredVelocity = Add(vDesiredVelocity, vAdditionalVelocity);
	rPed.SetDesiredVelocity(VEC3V_TO_VECTOR3(vDesiredVelocity));
}

////////////////////////////////////////////////////////////////////////////////

void CHeadingFixupHelper::Apply(CPed& rPed, Vec3V_In vTarget, const crClip& rClip, float fPhase, float fMaxHeadingChangePerSecond, float fTimeStep)
{
	//Calculate the max heading change.
	float fMaxHeadingChange = fMaxHeadingChangePerSecond * fTimeStep;

	Apply(rPed, vTarget, rClip, fPhase, fMaxHeadingChange);
}

void CHeadingFixupHelper::Apply(CPed& rPed, Vec3V_In vTarget, const crClip& rClip, float fPhase, float fMaxHeadingChange)
{
	//Calculate the target heading.
	const float fTargetHeading = fwAngle::GetRadianAngleBetweenPoints(vTarget.GetXf(), vTarget.GetYf(),
		rPed.GetTransform().GetPosition().GetXf(), rPed.GetTransform().GetPosition().GetYf());

	Apply(rPed, fTargetHeading, rClip, fPhase, fMaxHeadingChange);
}

void CHeadingFixupHelper::Apply(CPed& rPed, float fTargetHeading, const crClip& rClip, float fPhase, float fMaxHeadingChangePerSecond, float fTimeStep)
{
	//Calculate the max heading change.
	float fMaxHeadingChange = fMaxHeadingChangePerSecond * fTimeStep;

	Apply(rPed, fTargetHeading, rClip, fPhase, fMaxHeadingChange);
}

void CHeadingFixupHelper::Apply(CPed& rPed, float fTargetHeading, const crClip& rClip, float fPhase, float fMaxHeadingChange)
{
	//Calculate the remaining heading change.
	const Quaternion qCurrent	= fwAnimHelpers::GetMoverTrackRotation(rClip, fPhase);
	const Quaternion qEnd		= fwAnimHelpers::GetMoverTrackRotation(rClip, 1.0f);

	Vector3 vCurrent;
	qCurrent.ToEulers(vCurrent, "xyz");
	Vector3 vEnd;
	qEnd.ToEulers(vEnd, "xyz");

	vCurrent.z	= fwAngle::LimitRadianAngle(vCurrent.z);
	vEnd.z		= fwAngle::LimitRadianAngle(vEnd.z);

	const float fRemainingHeadingChange = fwAngle::LimitRadianAngle(vEnd.z - vCurrent.z);

	//Calculate the required heading change.
	const float fRequiredHeadingChange = SubtractAngleShorter(fwAngle::LimitRadianAngle(fTargetHeading), rPed.GetCurrentHeading());

	//Calculate the heading change for this frame.
	float fHeadingChange = fwAngle::LimitRadianAngle(fRequiredHeadingChange - fRemainingHeadingChange);
	fHeadingChange = Clamp(fHeadingChange, -fMaxHeadingChange, fMaxHeadingChange);

	//Apply the heading change.
	float fHeading = rPed.GetCurrentHeading();
	fHeading += fHeadingChange;
	rPed.SetHeading(fwAngle::LimitRadianAngleSafe(fHeading));
}

////////////////////////////////////////////////////////////////////////////////

bool CExplosionHelper::CalculateTimeUntilExplosion(const CEntity& rEntity, float& fTime)
{
	//Check if the entity is a vehicle.
	if(rEntity.GetIsTypeVehicle())
	{
		//Grab the vehicle.
		const CVehicle& rVehicle = static_cast<const CVehicle &>(rEntity);

		return rVehicle.CalculateTimeUntilExplosion(fTime);
	}
	//Check if the entity is an object.
	else if(rEntity.GetIsTypeObject())
	{
		//Grab the object.
		const CObject& rObject = static_cast<const CObject &>(rEntity);

		//Check if the object is a projectile.
		const CProjectile* pProjectile = rObject.GetAsProjectile();
		if(pProjectile)
		{
			return pProjectile->CalculateTimeUntilExplosion(fTime);
		}
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////
// CScenarioClipHelper
// Collection of code to help a scenario ped pick the best clip to play in a 
// given situation.
///////////////////////////////////////////////////////////////////////////////////

BANK_ONLY(bool CScenarioClipHelper::sm_bRenderProbeChecks = false;)

CScenarioClipHelper::Tunables CScenarioClipHelper::sm_Tunables;
IMPLEMENT_SCENARIO_TASK_TUNABLES(CScenarioClipHelper, 0x57ad3406);

CScenarioClipHelper::CScenarioClipHelper() 
: CExpensiveProcess(PPT_ScenarioExitDistributer)
, m_iCurrentExitClipIndex(0)
, m_pCapsuleResults(NULL)
{
	ResetPreviousWinners();
	RegisterSlot();
}

CScenarioClipHelper::~CScenarioClipHelper()
{
	UnregisterSlot();

	Reset();
}

void CScenarioClipHelper::Reset()
{
	m_iCurrentExitClipIndex = 0;

	if (m_pCapsuleResults)
	{
		m_pCapsuleResults->Reset();
		delete m_pCapsuleResults;
		m_pCapsuleResults = NULL;
	}
}

void CScenarioClipHelper::ResetPreviousWinners()
{
	for(int i=0; i < sm_WinnerArraySize; i++)
	{
		m_aPreviousWinners[i] = -1;
	}

	// Reset the translation array as well here.
	ResetTranslationArray();
}

void CScenarioClipHelper::StorePreviousWinner(s8 iIndex)
{
	for(int i=0; i < sm_WinnerArraySize; i++)
	{
		if (m_aPreviousWinners[i] == -1)
		{
			m_aPreviousWinners[i] = iIndex;
			break;
		}
	}
}

bool CScenarioClipHelper::IsIndexInPreviousWinners(s8 iIndex) const
{
	for(int i=0; i < sm_WinnerArraySize; i++)
	{
		if (m_aPreviousWinners[i] == iIndex)
		{
			return true;
		}
	}
	return false;
}

bool CScenarioClipHelper::CanStoreAnyMoreWinningIndices() const
{
	return m_aPreviousWinners[sm_WinnerArraySize - 1] == -1;
}

// We need to build a sorted array of clips, with index 0 being the clip that best rotates towards the target.
// fHeading is the rotation the "ideal" clip would bring, so try to minimize abs(fHeading - fClipHeading).
int ClipItemCompareFunction(const CScenarioClipHelper::sClipSortStruct* clipA, const CScenarioClipHelper::sClipSortStruct* clipB)
{
	float fDiff = clipA->fClipDelta - clipB->fClipDelta;
	if (fabs(fDiff) < VERY_SMALL_FLOAT)
	{
		return 0;
	}
	else if (fDiff < 0.0f)
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

// Build an array that corresponds to the clips in the clipset ordered by nearest rotation to some desired heading.
void CScenarioClipHelper::BuildTranslationArray(const fwClipSet& rClipSet, float fHeading, bool bRotateToTarget)
{
	const int iClipCount = Min(rClipSet.GetClipItemCount(), sm_IndexTranslationArraySize);
	atArray<sClipSortStruct> aSortStructs(iClipCount, iClipCount);
	
	// Build an array of sortable structs to determine the best clip according to rotation to target.
	for(int i=0; i < iClipCount; i++)
	{
		m_aTranslationArray[i] = (s8)i;
		aSortStructs[i].iClipIndex = (s8)i;
		fwClipItem* pClipItem = rClipSet.GetClipItemByIndex(i);
		float fClipHeading = 0.0f;
		if (pClipItem)
		{
			fClipHeading = GetClipItemDirection(pClipItem);
		}
		aSortStructs[i].fClipDelta = fabs(SubtractAngleShorter(fClipHeading, fHeading));
	}

	if (bRotateToTarget)
	{
		// Sort clips by closest to the "ideal rotation."
		aSortStructs.QSort(0, iClipCount, ClipItemCompareFunction);

		// Assign this ordering to the translation table.
		for(int i=0; i < iClipCount; i++)
		{
			m_aTranslationArray[i] = aSortStructs[i].iClipIndex;
		}
	}

	// Note - if no preference in rotation was specified, then the translation array is identical to the original ordering of clips.
}

bool CScenarioClipHelper::HasBuiltTranslationArray() const
{
	return m_aTranslationArray[0] != -1;
}

void CScenarioClipHelper::ResetTranslationArray()
{
	for(int i=0; i < sm_IndexTranslationArraySize; i++)
	{
		m_aTranslationArray[i] = -1;
	}
}

CScenarioClipHelper::ScenarioClipCheckType CScenarioClipHelper::PickClipFromSet(const CPed* pPed, const CEntity* pScenarioEntity, const CAITarget& rTarget, fwMvClipSetId clipSetId, fwMvClipId &chosenClipId, 
																				bool bRotateToTarget, bool bDoProbeChecks, bool bForceOneFrameCheck, bool bCheckFullLengthOfAnimation, bool bIgnoreWorldGeometryInCheck,
																				float fFinalZOverride, float fCapsuleRadiusOverride)
{
	PF_FUNC(PickClipFromSet);

	// Check if we have been given a slice to run.
	bool bAllowProcessing = bForceOneFrameCheck || ShouldBeProcessedThisFrame();
	if (!bAllowProcessing)
	{
		// Couldn't run this frame.
		return ClipCheck_Pending;
	}

	chosenClipId = CLIP_ID_INVALID;

	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	float fHeading = 0.0f;
	if (bRotateToTarget)
	{
		Assertf(rTarget.GetIsValid(), "Scenario reaction target was not valid - clone:  %d clipsetid:  %s, Ped name %s", 
			(int)pPed->IsNetworkClone(),
			clipSetId.GetCStr(),
			pPed->GetDebugName());

		if (rTarget.GetEntity() && rTarget.GetEntity()->GetIsTypeBuilding())
		{
			bRotateToTarget = false;
		}
		else
		{
			Vector3 vTarget;
			rTarget.GetPosition(vTarget);
			Vec3V vPosition = pPed->GetTransform().GetPosition();

			fHeading = fwAngle::GetRadianAngleBetweenPoints(vTarget.x, vTarget.y, vPosition.GetXf(), vPosition.GetYf());
			//Relative to the ped's current heading.
			fHeading = SubtractAngleShorter(fHeading, pPed->GetCurrentHeading());
		}
	}
	
	int iClipsInClipset = Min(pClipSet->GetClipItemCount(), sm_IndexTranslationArraySize);
	int iMax = bForceOneFrameCheck ? iClipsInClipset : Min(iClipsInClipset, m_iCurrentExitClipIndex + 1);
	if (pClipSet)
	{
		Assertf(pClipSet->IsStreamedIn_DEPRECATED(), "Scenario clipset not streamed in (%s)", pClipSet->GetClipDictionaryName().GetCStr());

		if (!HasBuiltTranslationArray())
		{
			BuildTranslationArray(*pClipSet, fHeading, bRotateToTarget);
		}

		// Try to find a free direction to stand up in if it isn't yet known.
		while (m_iCurrentExitClipIndex < iMax)
		{
			// Translate the current clip index to the test index.
			const s8 iClipIndex = m_aTranslationArray[m_iCurrentExitClipIndex];

			if (IsIndexInPreviousWinners(iClipIndex))
			{
				// We've seen this winning clip before.
				// Skip it and try something else as it probably didn't work out for other reasons (no valid path perhaps).
				m_iCurrentExitClipIndex++;
				iMax = Min(iClipsInClipset, iMax + 1);
				continue;
			}
			fwMvClipId testClipId = pClipSet->GetClipItemIdByIndex(iClipIndex);
			const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(clipSetId, testClipId);
			Assertf(pClip, "Scenario accessing unknown clip: (%s, %s, %i)", pClipSet->GetClipDictionaryName().GetCStr(), testClipId.GetCStr(), iClipIndex);

			bool bValidPos = true;

			if (bDoProbeChecks)
			{
				Vector3 vTranslation(Vector3::ZeroType);
				const bool bUseCustomOffsetZ = fabs(fFinalZOverride) > SMALL_FLOAT;
				const bool bUseCustomCapsuleRadius = fabs(fCapsuleRadiusOverride) > SMALL_FLOAT;
				if (pClip)
				{
					vTranslation = fwAnimHelpers::GetMoverTrackDisplacement(*pClip, 0.0f, 1.0f);
				}
				vTranslation.RotateZ(pPed->GetCurrentHeading());

				if (bUseCustomOffsetZ)
				{
					vTranslation.SetZ(fFinalZOverride);
				}

				const Vector3 vPedPosition = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
				Vector3 vEndPosition = vPedPosition + vTranslation;
				const Vector3 vStartPosition = bCheckFullLengthOfAnimation ? vPedPosition : (vPedPosition + vEndPosition) / 2.0;
				static const float s_fCapsuleCheckEpsilon = 0.1f;
				static const float s_fZEpsilon = 0.25f;

				// Angle the probe downwards slightly to try and catch short objects (like other chairs) in the way.
				if (!bUseCustomOffsetZ)
				{
					vEndPosition.z -= s_fZEpsilon;
				}
				const float fCapsuleRadius = bUseCustomCapsuleRadius ? fCapsuleRadiusOverride : pPed->GetCapsuleInfo()->GetHalfWidth() + s_fCapsuleCheckEpsilon;

				if (!m_pCapsuleResults)
				{
					// No test is active - fire a new one off.
					// Start the probe at the average between the ped position and the end of the anim position.

					FireCapsuleTest(pPed, pScenarioEntity, vStartPosition, vEndPosition, fCapsuleRadius, !bForceOneFrameCheck, bIgnoreWorldGeometryInCheck);
				}

				// Check the test to see if it is done.
				CScenarioClipHelper::ScenarioClipCheckType iProbeResult = CheckCapsuleTest();

				if (iProbeResult == ClipCheck_Pending)
				{
					// Async probe not done, try again later.
					return iProbeResult;
				}

				// The async probe was ready - see what the result is.
				bValidPos = iProbeResult == ClipCheck_Success;// && CPathServer::TestNavMeshPolyUnderPosition(vGetupPos, true, false);

#if __BANK
				if (sm_bRenderProbeChecks) 
				{
					if (bValidPos)
					{
						grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vStartPosition), VECTOR3_TO_VEC3V(vEndPosition), fCapsuleRadius, Color_green, false, -30);
					}
					else
					{
						grcDebugDraw::Capsule(VECTOR3_TO_VEC3V(vStartPosition), VECTOR3_TO_VEC3V(vEndPosition), fCapsuleRadius, Color_red, false, -30);
					}

				}
#endif
			}

			if (bValidPos)
			{
				// Success!
				chosenClipId = pClipSet->GetClipItemIdByIndex(iClipIndex);
				// Keep track of the winning clip in case we need to revisit this scenario point again later.
				StorePreviousWinner(iClipIndex);
				return ClipCheck_Success;
			}
			
			m_iCurrentExitClipIndex++;
		}

		// Done with looping this frame - are we done?  Only if this was the last clip in the set.
		if (m_iCurrentExitClipIndex == iClipsInClipset)
		{
			return ClipCheck_Failed;
		}
		else
		{
			//Not done yet.
			return ClipCheck_Pending;
		}
	}
	else
	{
		// Failure because the clipset did not exist.
		return ClipCheck_Failed;
	}
}

void CScenarioClipHelper::FireCapsuleTest(const CPed* pPed, const CEntity* pScenarioEntity, const Vector3& vStart, const Vector3& vEnd, float fRadius, bool bAsync, bool bIgnoreWorldGeometryDuringExitProbeCheck)
{
	Assert(!m_pCapsuleResults);

	m_pCapsuleResults = rage_new WorldProbe::CShapeTestResults(sm_NumberOfResults);

	static const s32 iNumExceptions = 2;
	const CEntity* ppExceptions[iNumExceptions] = {pPed, pScenarioEntity};

	WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
	if (bIgnoreWorldGeometryDuringExitProbeCheck)
	{
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	}
	else
	{
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES | ArchetypeFlags::GTA_VEHICLE_TYPE | ArchetypeFlags::GTA_BOX_VEHICLE_TYPE | ArchetypeFlags::GTA_OBJECT_TYPE);
	}
	capsuleDesc.SetExcludeEntities(ppExceptions, iNumExceptions);
	capsuleDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);
	capsuleDesc.SetResultsStructure(m_pCapsuleResults);
	capsuleDesc.SetMaxNumResultsToUse(sm_NumberOfResults);
	capsuleDesc.SetIsDirected(true);
	capsuleDesc.SetCapsule(vStart, vEnd, fRadius);
	if (bAsync)
	{
		capsuleDesc.SetDoInitialSphereCheck(true);
	}

	PF_START(ScenarioExitHelperSubmitAsyncProbe);

	WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc, bAsync ? WorldProbe::PERFORM_ASYNCHRONOUS_TEST : WorldProbe::PERFORM_SYNCHRONOUS_TEST);

	PF_STOP(ScenarioExitHelperSubmitAsyncProbe);
}

CScenarioClipHelper::ScenarioClipCheckType CScenarioClipHelper::CheckCapsuleTest()
{
	Assert(m_pCapsuleResults);

	bool bReady = m_pCapsuleResults->GetResultsReady();

	if (bReady)
	{
		// Iterate over the objects the capsule collided with.
		// If it was a "big enough" object, then reject this as a potential exit.
		bool bHitBigObject = false;
		WorldProbe::ResultIterator it;
		for(it=m_pCapsuleResults->begin(); it < m_pCapsuleResults->last_result(); it++)
		{
			const CEntity* pHitEntity = it->GetHitEntity();

			if (pHitEntity && pHitEntity->GetIsTypeObject())
			{
				// The hit entity was of type object, check and see if it is an ambient prop.
				const CObject* pHitObject = static_cast<const CObject*>(pHitEntity);
				if (!pHitObject->m_nObjectFlags.bAmbientProp)
				{
					bHitBigObject = true;
				}
			}
			else
			{
				// Something other than an object was hit, from our exclusions set when firing the probe
				// that means it is either a vehicle or world geometry.
				bHitBigObject = true;
			}

			if (bHitBigObject)
			{
				break;
			}
		}
		// Reset the capsule results.
		m_pCapsuleResults->Reset();

		// Free up the memory used to store the results.
		delete m_pCapsuleResults;
		m_pCapsuleResults = NULL;
		
		// It hit something large - this clip did not work out.
		return bHitBigObject ? ClipCheck_Failed : ClipCheck_Success;
	}
	else
	{
		// The results aren't ready yet.
		return ClipCheck_Pending;
	}
}

void CScenarioClipHelper::PickFrontFacingClip(fwMvClipSetId clipSetId, fwMvClipId &chosenClipId)
{
	const fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
	float fBestHeading = LARGE_FLOAT;
	int iWinningIndex = -1;
	if (pClipSet)
	{
		for(int i=0; i < pClipSet->GetClipItemCount(); i++)
		{
			fwClipItem* pClipItem = pClipSet->GetClipItemByIndex(i);
			float fClipHeading = LARGE_FLOAT;
			if (pClipItem)
			{
				fClipHeading = GetClipItemDirection(pClipItem);
			}

			if (fabs(fClipHeading) < fabs(fBestHeading) || iWinningIndex == -1)
			{
				fBestHeading = fClipHeading;
				iWinningIndex = i;
			}
		}
	}

	if (iWinningIndex == -1)
	{
		chosenClipId = CLIP_ID_INVALID;
	}
	else
	{
		chosenClipId = pClipSet->GetClipItemIdByIndex(iWinningIndex);
	}
}

////////////////////////////////////////////////////////////////////////////////
// CScenarioSpeedHelper
////////////////////////////////////////////////////////////////////////////////

// The circle inside of which you are fine to maintain the speed of the target.
const float CScenarioSpeedHelper::ms_fInnerCircle = 0.5f;

// The circle inside of which you need to speed up / slow down a small amount.
// Outside of this circle you need to speed up / slow down a large amount.
const float CScenarioSpeedHelper::ms_fOuterCircle = 2.0f;

// How fast is "a lot" of MBR boost to catch up.
const float CScenarioSpeedHelper::ms_fSpeedChangeBig = 1.0f;

// How fast is "a small amount" of MBR boost to catch up.
const float CScenarioSpeedHelper::ms_fSpeedChangeSmall = 0.5f;

// Used to check the current mbr of the follower vs. the desired mbr to achieve its speed.
const float CScenarioSpeedHelper::ms_fSmallSpeedDiffThreshold = 0.2f;

bool CScenarioSpeedHelper::ShouldUseSpeedMatching(const CPed& ped)
{
	if (!ped.GetTaskData().GetIsFlagSet(CTaskFlags::DependentAmbientFriend))
	{
		return false;
	}

	if (!ped.GetPedConfigFlag(CPED_CONFIG_FLAG_LinkMBRToOwnerOnChain))
	{
		return false;
	}

	return true;
}

float CScenarioSpeedHelper::GetMBRToMatchSpeed(const CPed& ped, const CPed& target, Vec3V_In vDestinationScenarioPoint)
{
	Vec3V vTarget = target.GetTransform().GetPosition();
	Vec3V vPosition = ped.GetTransform().GetPosition();
	
	Vec3V vToTarget = vTarget - vPosition;
	vToTarget.SetZf(0.0f);

	// The direction is vector from the following ped to its scenario point.
	Vec3V vDirection = vDestinationScenarioPoint - vPosition;
	vDirection.SetZf(0.0f);
	vDirection = NormalizeFastSafe(vDirection, Vec3V(V_ZERO));

	// Project the distance to the target to be along our direction vector.
	ScalarV vProjectedLength = Dot(vToTarget, vDirection);

	//grcDebugDraw::Sphere(VEC3V_TO_VECTOR3((vProjectedLength * vDirection) + vPosition), 0.25f, Color_white, true, -1);
	//grcDebugDraw::Circle(VEC3V_TO_VECTOR3(vPosition), ms_fInnerCircle, Color_green, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), false, false, -1);
	//grcDebugDraw::Circle(VEC3V_TO_VECTOR3(vPosition), ms_fOuterCircle, Color_red, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), false, false, -1);

	ScalarV vIdealThresholdDistance(ms_fInnerCircle);

	// Figure out what MBR to set to match the target's speed perfectly.
	float fEntitySpeed = target.GetVelocity().Mag();
	float fMBRToMatch = CTaskMoveBase::ComputeMoveBlend(const_cast<CPed&>(ped), fEntitySpeed);
	
	const float fDesiredMBR = ped.GetMotionData()->GetDesiredMbrY();
	const bool bSpeedDiffIsSmall = fabs(fDesiredMBR - fMBRToMatch) < ms_fSmallSpeedDiffThreshold;

	if (IsLessThanAll(Abs(vProjectedLength), vIdealThresholdDistance) 
		|| (bSpeedDiffIsSmall && IsLessThanAll(Abs(vProjectedLength), ScalarV(ms_fOuterCircle))))
	{
		// Close to the target, maintain speed.
		if (bSpeedDiffIsSmall)
		{
			return fDesiredMBR;
		}
		else
		{
			return fMBRToMatch;
		}
	}
	else
	{
		if (IsGreaterThanAll(vProjectedLength, ScalarV(ms_fOuterCircle)))
		{
			// Behind target a lot - speed up a lot.
			fMBRToMatch += ms_fSpeedChangeBig;
		}
		else if (IsGreaterThanAll(vProjectedLength, ScalarV(0.0f)))
		{
			// Behind target by a little - speed up a little.
			fMBRToMatch += ms_fSpeedChangeSmall;
		}
		else if (IsLessThanAll(vProjectedLength, ScalarV(-ms_fOuterCircle)))
		{
			// In front of the target by a lot - slow down a lot.
			fMBRToMatch -= ms_fSpeedChangeBig;
		}
		else
		{
			// In front of the target by a little - slow down a little.
			fMBRToMatch -= ms_fSpeedChangeSmall;
		}
	}

	fMBRToMatch = Clamp(fMBRToMatch, 0.5f, MOVEBLENDRATIO_SPRINT);
	return fMBRToMatch;
}

////////////////////////////////////////////////////////////////////////////////

void CFleeHelpers::InitializeSecondaryTarget(const CAITarget& rTarget, CAITarget& rSecondaryTarget)
{
	//Ensure the secondary target is not valid.
	if(rSecondaryTarget.GetIsValid())
	{
		return;
	}

	//Check if the flee target is an object.
	if(rTarget.GetEntity() && rTarget.GetEntity()->GetIsTypeObject())
	{
		//Check if the flee target is a projectile.
		const CProjectile* pProjectile = static_cast<const CObject *>(rTarget.GetEntity())->GetAsProjectile();
		if(pProjectile)
		{
			//Check if the owner is valid.
			CEntity* pOwner = pProjectile->GetOwner();
			if(pOwner)
			{
				//Set the secondary target.
				rSecondaryTarget.SetEntity(pOwner);
				return;
			}
		}
	}
}

bool CFleeHelpers::IsPositionOfTargetValid(const CAITarget& rTarget)
{
	//Ensure the target is valid.
	if(!rTarget.GetIsValid())
	{
		return false;
	}

	//Ensure the position is valid.
	Vec3V vPosition;
	if(!rTarget.GetPosition(RC_VECTOR3(vPosition)))
	{
		return false;
	}

	return (IsPositionValid(vPosition));
}

bool CFleeHelpers::IsPositionValid(Vec3V_In vPosition)
{
	return (!IsCloseAll(vPosition, Vec3V(V_ZERO), ScalarV(V_FLT_SMALL_2)));
}

bool CFleeHelpers::ProcessTargets(CAITarget& rTarget, CAITarget& rSecondaryTarget)
{
	//Check if the target is invalid.
	if(!rTarget.GetIsValid() || !IsPositionOfTargetValid(rTarget))
	{
		//Check if the secondary target is invalid.
		if(!rSecondaryTarget.GetIsValid() || !IsPositionOfTargetValid(rSecondaryTarget))
		{
			return false;
		}
		else
		{
			//Set the target.
			rTarget = rSecondaryTarget;
			rSecondaryTarget.Clear();
		}
	}

	//Assert that the target is valid.
	aiAssert(rTarget.GetIsValid());
	aiAssert(IsPositionOfTargetValid(rTarget));

	return true;
}

bool CFleeHelpers::ProcessTargets(CAITarget& rTarget, CAITarget& rSecondaryTarget, Vec3V_InOut vLastTargetPosition)
{
	//Process the targets.
	if(!ProcessTargets(rTarget, rSecondaryTarget))
	{
		//Check if the last target position is invalid.
		if(!IsPositionValid(vLastTargetPosition))
		{
			return false;
		}
		else
		{
			//Set the target.
			rTarget.SetPosition(VEC3V_TO_VECTOR3(vLastTargetPosition));
		}
	}

	//Assert that the target is valid.
	aiAssert(rTarget.GetIsValid());
	aiAssert(IsPositionOfTargetValid(rTarget));

	//Set the last target position.
	rTarget.GetPosition(RC_VECTOR3(vLastTargetPosition));

	return true;
}

////////////////////////////////////////////////////////////////////////////////

u32 CTimeHelpers::GetTimeSince(u32 uTime)
{
	//Check if the time is before the current time.
	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	if(uTime <= uCurrentTime)
	{
		return (uCurrentTime - uTime);
	}
	else
	{
		return (MAX_UINT32 - uTime) + uCurrentTime;
	}
}

////////////////////////////////////////////////////////////////////////////////

bool CLastNavMeshIntersectionHelper::Get(const CAITarget& rTarget, Vec3V_InOut vPosition)
{
	//Ensure the entity is valid.
	const CEntity* pEntity = rTarget.GetEntity();
	if(!pEntity)
	{
		return false;
	}

	return (Get(*pEntity, vPosition));
}

bool CLastNavMeshIntersectionHelper::Get(const CEntity& rEntity, Vec3V_InOut vPosition)
{
	//Ensure the entity is a ped.
	if(!rEntity.GetIsTypePed())
	{
		return false;
	}

	return (Get(static_cast<const CPed &>(rEntity), vPosition));
}

bool CLastNavMeshIntersectionHelper::Get(const CPed& rPed, Vec3V_InOut vPosition)
{
	//Ensure the last nav mesh intersection is valid.
	if(!rPed.GetNavMeshTracker().IsLastNavMeshIntersectionValid())
	{
		return false;
	}

	//Set the position.
	vPosition = VECTOR3_TO_VEC3V(rPed.GetNavMeshTracker().GetLastNavMeshIntersection());

	//HACK: The nav mesh intersection position seems a bit low.
	static dev_float s_fExtraZ = 0.5f;
	vPosition.SetZf(vPosition.GetZf() + s_fExtraZ);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

bool CHeliRefuelHelper::Process(const CPed& rPed, const CPed& rTarget)
{
	// Don't do refueling if the player flag to disable it is set
	if(rTarget.GetPlayerResetFlag(CPlayerResetFlags::PRF_DISABLE_DISPATCHED_HELI_REFUEL))
	{
		return false;
	}

	//Ensure the ped is random.
	if(!rPed.PopTypeIsRandom())
	{
		return false;
	}

	//Ensure the ped is law enforcement.
	if(!rPed.IsLawEnforcementPed())
	{
		return false;
	}

	//Ensure the vehicle is valid.
	const CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(!pVehicle)
	{
		return false;
	}

	//Ensure the vehicle is a heli.
	if(!pVehicle->InheritsFromHeli())
	{
		return false;
	}

	//Ensure the ped is driving.
	if(!pVehicle->IsDriver(&rPed))
	{
		return false;
	}

	//Ensure the wanted is valid.
	CWanted* pWanted = rTarget.GetPlayerWanted();
	if(!pWanted)
	{
		return false;
	}

	//Ensure the heli needs to refuel.
	float fTimeBefore;
	float fDelay;
	if(!pWanted->CalculateHeliRefuel(fTimeBefore, fDelay))
	{
		return false;
	}

	//Ensure we have run out of fuel.
	if(CTimeHelpers::GetTimeSince(pVehicle->m_TimeOfCreation) < (fTimeBefore * 1000.0f))
	{
		return false;
	}

	//Note that a heli needed to refuel.
	pWanted->OnHeliRefuel(fDelay);

	return true;
}

////////////////////////////////////////////////////////////////////////////////

CTask* CHeliFleeHelper::CreateTaskForHeli(const CHeli& rHeli, const CAITarget& rTarget)
{
	//Get the target position.
	taskAssert(rTarget.GetIsValid());
	Vector3 vTargetPosition(VEC3_ZERO);
	rTarget.GetPosition(vTargetPosition);

	vTargetPosition.SetX(IsTrue(rHeli.GetTransform().GetPosition().GetX() < ScalarV(vTargetPosition.x)) ? WORLDLIMITS_REP_XMIN : WORLDLIMITS_REP_XMAX);
	vTargetPosition.SetY(IsTrue(rHeli.GetTransform().GetPosition().GetY() < ScalarV(vTargetPosition.y)) ? WORLDLIMITS_REP_YMIN : WORLDLIMITS_REP_YMAX);
	vTargetPosition.SetZ(40.0f);

	//Create the parameters.
	sVehicleMissionParams params;
	params.m_fCruiseSpeed = 35.0f;
	params.SetTargetPosition(vTargetPosition);

	//Create the task.
	CTask* pTask = rage_new CTaskVehicleGoToHelicopter(params, 0, -1.0f, 40);

	return pTask;
}

CTask* CHeliFleeHelper::CreateTaskForPed(const CPed& rPed, const CAITarget& rTarget)
{
	//Ensure the vehicle is valid.
	CVehicle* pVehicle = rPed.GetVehiclePedInside();
	if(!taskVerifyf(pVehicle, "The vehicle is invalid."))
	{
		return NULL;
	}

	//Ensure the vehicle is a heli.
	if(!taskVerifyf(pVehicle->InheritsFromHeli(), "The vehicle is not a heli."))
	{
		return NULL;
	}

	//Ensure the ped is driving the vehicle.
	if(!taskVerifyf(pVehicle->IsDriver(&rPed), "The ped is a passenger."))
	{
		return NULL;
	}

	//Create the vehicle task.
	CTask* pVehicleTask = CreateTaskForHeli(*static_cast<CHeli *>(pVehicle), rTarget);

	//Create the task.
	CTask* pTask = rage_new CTaskControlVehicle(pVehicle, pVehicleTask);

	return pTask;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleUndriveableHelper::IsUndriveable(const CVehicle& rVehicle)
{
	//Check if the vehicle is wrecked.
	if(rVehicle.GetStatus() == STATUS_WRECKED)
	{
		return true;
	}

	//Check if the driveable state is valid.
	bool bIsCar = (rVehicle.GetVehicleType() == VEHICLE_TYPE_CAR);
	bool bIsBike = rVehicle.InheritsFromBike();
	bool bCheckDriveableState = (bIsCar || bIsBike);
	if(bCheckDriveableState && !rVehicle.IsInDriveableState())
	{
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CVehicleStuckDuringCombatHelper::IsStuck(const CVehicle& rVehicle)
{
	//Check if we are making a three point turn.
	if(rVehicle.GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY,
		CTaskTypes::TASK_VEHICLE_THREE_POINT_TURN))
	{
		return true;
	}

	//Check if we are in a task capable of making a three point turn.
	if(rVehicle.GetIntelligence()->GetTaskManager()->FindTaskByTypeActive(VEHICLE_TASK_TREE_PRIMARY,
		CTaskTypes::TASK_VEHICLE_GOTO_POINT_WITH_AVOIDANCE_AUTOMOBILE))
	{
		//Check if we are stuck.
		static dev_u16 s_uMinTime = 250;
		if(rVehicle.GetVehicleDamage()->GetIsStuck(VEH_STUCK_JAMMED, s_uMinTime))
		{
			return true;
		}

		//Check if we haven't been moving.
		if(rVehicle.GetIntelligence()->MillisecondsNotMoving > 0)
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

CPed* CFindClosestFriendlyPedHelper::Find(const CPed& rPed, float fMaxDistance)
{
	//Get the max distance.
	ScalarV scMaxDistSq = ScalarVFromF32(square(fMaxDistance));

	//Iterate over the nearby peds.
	CEntityScannerIterator pedList = rPed.GetPedIntelligence()->GetNearbyPeds();
	for(CEntity* pEntity = pedList.GetFirst(); pEntity != NULL; pEntity = pedList.GetNext())
	{
		//Grab the ped.
		CPed* pPed = static_cast<CPed *>(pEntity);

		//Ensure the ped is alive.
		if(pPed->IsInjured())
		{
			continue;
		}

		//Ensure the distance is valid.
		ScalarV scDistSq = DistSquared(rPed.GetTransform().GetPosition(), pPed->GetTransform().GetPosition());
		if(IsGreaterThanAll(scDistSq, scMaxDistSq))
		{
			break;
		}

		//Ensure the ped is friendly.
		if(!rPed.GetPedIntelligence()->IsFriendlyWith(*pPed))
		{
			continue;
		}

		return pPed;
	}

	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

#if FPS_MODE_SUPPORTED

CFirstPersonIkHelper::CFirstPersonIkHelper(eFirstPersonType eType)
: m_vOffset(V_ZERO)
, m_pWeaponInfo(NULL)
, m_eFirstPersonType(eType)
, m_eBlendInRate(ARMIK_BLEND_RATE_FAST)
, m_eBlendOutRate(ARMIK_BLEND_RATE_FASTEST)
, m_ExtraFlags(0)
, m_bActive(false)
, m_uArm(FP_ArmRight)
, m_bUseRecoil(true)
, m_bUseAnimBlockingTags(true)
, m_bUseSecondaryMotion(false)
, m_bUseLook(false)
, m_fMinDrivebyYaw(-PI)
, m_fMaxDrivebyYaw(PI)
{
	Reset();
}

CFirstPersonIkHelper::~CFirstPersonIkHelper()
{
	Reset();
}

void CFirstPersonIkHelper::Process(CPed* pPed)
{
	u32 flags = AIK_TARGET_WRT_CONSTRAINTHELPER | AIK_USE_ORIENTATION | AIK_USE_FULL_REACH | AIK_APPLY_ALT_SRC_RECOIL | AIK_HOLD_HELPER | m_ExtraFlags;

	if (m_bUseAnimBlockingTags)
	{
		flags |= AIK_USE_ANIM_BLOCK_TAGS;
	}

	Mat34V mtxEntity(pPed->GetMatrix());
	Mat34V mtxSource;
	Mat34V mtxSourceTP;
	QuatV qAdditive(V_IDENTITY);

	// Look support
	if (m_bUseLook)
	{
		ProcessLook(pPed, mtxEntity);
	}

	// Constraint helper weight
	float fCHWeight;
	Vec3V vCHTranslation;
	QuatV qCHRotation;

	const bool bValid = pPed->GetConstraintHelperDOFs((m_uArm == FP_ArmRight), fCHWeight, vCHTranslation, qCHRotation);
	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, ACCEPT_ANY_CH_WEIGHT, true);
	const bool bAcceptAnyCHWeight = (AcceptAnyCHWeightForCover(*pPed) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_ForceFPSIKWithUpperBodyAnim) || pPed->GetPedResetFlag(CPED_RESET_FLAG_IsEnteringVehicle) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_IsSwitchingHelmetVisor)) ? ACCEPT_ANY_CH_WEIGHT : false;
	const bool bCHWeightInValid = fCHWeight < 1.0 && (!bAcceptAnyCHWeight || fCHWeight <= 0.0f);
	if (!bValid || bCHWeightInValid)
	{
		return;
	}

	// Setup
	switch(m_eFirstPersonType)
	{
	case(FP_OnFoot):
		{
			taskAssertf(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, false, true, false), "Ped not in first person mode");
			const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
			const camFirstPersonPedAimCamera* pAltCamera = camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera();

			if (pCamera)
			{
				camBaseFrameShaker* frameShaker = camInterface::GetGameplayDirector().GetFrameShaker();
				if(frameShaker && frameShaker->GetNameHash() == ATSTRINGHASH("DRUNK_SHAKE", 0x38208c89))
				{
					mtxSource = MATRIX34_TO_MAT34V(camInterface::GetGameplayDirector().GetFrame().GetWorldMatrix());
					UnTransformFull(mtxSource, mtxEntity, mtxSource);
				}
				else if (!pCamera->GetObjectSpaceCameraMatrix(pPed, RC_MATRIX34(mtxSource)))
				{
					mtxSource = MATRIX34_TO_MAT34V(pCamera->GetFrame().GetWorldMatrix());
					UnTransformFull(mtxSource, mtxEntity, mtxSource);
				}
			
				// High-heel adjustment
				mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), UnTransform3x3Ortho(mtxEntity, Vec3V(V_UP_AXIS_WZERO)), ScalarV(-pCamera->GetHighHeelOffset()));

				// Offset and rotation support
				const ScalarV vEpsilon(V_FLT_SMALL_4);
				const Vec3V vZero(V_ZERO);
				const Vec3V vScopeOffset = VECTOR3_TO_VEC3V(pCamera->GetScopeOffset());
				const Vec3V vScopeRotationOffset = VECTOR3_TO_VEC3V(pCamera->GetScopeRotationOffset());
				const Vec3V vNonScopeOffset = VECTOR3_TO_VEC3V(pCamera->GetNonScopeOffset());
				const Vec3V vNonScopeRotationOffset = VECTOR3_TO_VEC3V(pCamera->GetNonScopeRotationOffset());

				if ((IsCloseAll(vScopeOffset, vZero, vEpsilon) == 0) || (IsCloseAll(vNonScopeOffset, vZero, vEpsilon) == 0))
				{
					// Cancel forward offset applied by camera
					mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol1(), Negate(vScopeOffset.GetY()));

					// Apply lateral and vertical offset
					mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol0(), Add(vScopeOffset.GetX(), vNonScopeOffset.GetX()));
					mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol2(), Add(vScopeOffset.GetZ(), vNonScopeOffset.GetZ()));
				}

				if ((IsCloseAll(vScopeRotationOffset, vZero, vEpsilon) == 0) || (IsCloseAll(vNonScopeRotationOffset, vZero, vEpsilon) == 0))
				{
					// Additive rotation
					const ScalarV vToRadians(V_TO_RADIANS);

					qAdditive = QuatVFromAxisAngle(mtxSource.GetCol1(), Scale(Add(vScopeRotationOffset.GetY(), vNonScopeRotationOffset.GetY()), vToRadians));
					qAdditive = Multiply(qAdditive, QuatVFromAxisAngle(mtxSource.GetCol2(), Scale(Add(vScopeRotationOffset.GetZ(), vNonScopeRotationOffset.GetZ()), vToRadians)));
					qAdditive = Multiply(qAdditive, QuatVFromAxisAngle(mtxSource.GetCol0(), Scale(Add(vScopeRotationOffset.GetX(), vNonScopeRotationOffset.GetX()), vToRadians)));
				}
			}
			else if (pAltCamera)
			{
				if (!pAltCamera->GetObjectSpaceCameraMatrix(pPed, RC_MATRIX34(mtxSource)))
				{
					mtxSource = MATRIX34_TO_MAT34V(pAltCamera->GetFrame().GetWorldMatrix());
					UnTransformFull(mtxSource, mtxEntity, mtxSource);
				}
			}
			else
			{
				// Allow arm IK to blend out first person camera is not valid
				return;
			}
		}
		break;
	case(FP_OnFootDriveBy):
		{
			taskAssertf(pPed->IsFirstPersonShooterModeEnabledForPlayer(false), "Ped not in first person mode");
			const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();

			if (pCamera)
			{
				if (!pCamera->GetObjectSpaceCameraMatrix(pPed, RC_MATRIX34(mtxSource)))
				{
					mtxSource = MATRIX34_TO_MAT34V(pCamera->GetFrame().GetWorldMatrix());
					UnTransformFull(mtxSource, mtxEntity, mtxSource);
				}
			}
			else
			{
				// Allow arm IK to blend out if first person shooter camera is not valid (eg. first person sniper rifle)
				return;
			}
		}
		break;
	case(FP_MobilePhone):
		{
			taskAssertf(pPed->IsFirstPersonShooterModeEnabledForPlayer(false, false, false, true, false), "Ped not in first person mode");
			const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();

			if (pCamera)
			{
				if (!pCamera->GetObjectSpaceCameraMatrix(pPed, RC_MATRIX34(mtxSource)))
				{
					mtxSource = MATRIX34_TO_MAT34V(pCamera->GetFrame().GetWorldMatrix());
					UnTransformFull(mtxSource, mtxEntity, mtxSource);
				}

				//! Remove any relative pitch we have applied from camera.
				TUNE_GROUP_FLOAT(ARM_IK, fPitchClampMin, 0.01f, -PI, PI, 0.01f);
				TUNE_GROUP_FLOAT(ARM_IK, fPitchClampMax, 0.25f, -PI, PI, 0.01f);	
				TUNE_GROUP_FLOAT(ARM_IK, fDesiredPitchAtMax, 0.15f, -PI, PI, 0.01f);	

				float fRelativePitch = pCamera->GetRelativePitch();

				float fDesiredPitch = abs(fRelativePitch);

				if(abs(fRelativePitch) > fPitchClampMin) 
				{
					float fScale = Clamp( (abs(fRelativePitch) - fPitchClampMin) / (fPitchClampMax-fPitchClampMin), 0.0f, 1.0f);
					fScale = SlowOut(fScale);
					fDesiredPitch = fPitchClampMin + ( ( fDesiredPitchAtMax - fPitchClampMin) * fScale );
				}

				//flip.
				if(fRelativePitch < 0.0f)
					fDesiredPitch = -fDesiredPitch;

				//get diff.
				float fRotatePitch = fRelativePitch-fDesiredPitch;

				Mat34VRotateLocalX(mtxSource, ScalarV(-fRotatePitch));
			}
			else
			{
				return;
			}
		}
		break;
	case(FP_DriveBy):
		{
			taskAssertf(pPed->IsInFirstPersonVehicleCamera(), "Ped not in first person mode");

			const camCinematicMountedCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonVehicleCamera();
			taskAssert(pCamera);
			mtxSource = MATRIX34_TO_MAT34V(pCamera->GetFrame().GetWorldMatrix());

			Vector3 vDir = VEC3V_TO_VECTOR3(pPed->GetTransform().UnTransform3x3(mtxSource.GetCol1()));
			vDir.z=0;
			vDir.Normalize();
			float fRelativeYaw = rage::Atan2f(vDir.x, vDir.y);
			fRelativeYaw = fwAngle::LimitRadianAngle(fRelativeYaw);

			float fDesiredHeading = fRelativeYaw;

			TUNE_GROUP_FLOAT(ARM_IK, fHeadingClampDiffMin, 0.1f, -PI, PI, 0.01f);
			TUNE_GROUP_FLOAT(ARM_IK, fHeadingClampDiffMax, 0.5f, -PI, PI, 0.01f);

			float fHeadingClampMinLeft = Min(m_fMinDrivebyYaw + fHeadingClampDiffMin, 0.0f);
			float fHeadingClampMaxLeft = Max(m_fMinDrivebyYaw - fHeadingClampDiffMax, -PI);
			float fDesiredHeadingAtMaxLeft = m_fMinDrivebyYaw;

			float fHeadingClampMinRight = Max(m_fMaxDrivebyYaw - fHeadingClampDiffMin, 0.0f);
			float fHeadingClampMaxRight = Min(m_fMaxDrivebyYaw + fHeadingClampDiffMax, PI);
			float fDesiredHeadingAtMaxRight = m_fMaxDrivebyYaw;

			if( (m_fMinDrivebyYaw > -PI) && (fRelativeYaw < fHeadingClampMinLeft) ) 
			{
				float fScale = Clamp( (abs(fRelativeYaw) - abs(fHeadingClampMinLeft)) / (abs(fHeadingClampMaxLeft)-abs(fHeadingClampMinLeft)), 0.0f, 1.0f);
				fScale = SlowOut(fScale);
				fDesiredHeading = fHeadingClampMinLeft + ( ( fDesiredHeadingAtMaxLeft - fHeadingClampMinLeft) * fScale );
			}
			else if( (m_fMaxDrivebyYaw < PI) && (fRelativeYaw > fHeadingClampMinRight))
			{
				float fScale = Clamp( (abs(fRelativeYaw) - abs(fHeadingClampMinRight)) / (abs(fHeadingClampMaxRight)-abs(fHeadingClampMinRight)), 0.0f, 1.0f);
				fScale = SlowOut(fScale);
				fDesiredHeading = fHeadingClampMinRight + ( ( fDesiredHeadingAtMaxRight - fHeadingClampMinRight) * fScale );
			}

			float fRotateHeading = fRelativeYaw-fDesiredHeading;

			Mat34VRotateLocalZ(mtxSource, ScalarV(fRotateHeading));

			UnTransformFull(mtxSource, mtxEntity, mtxSource);
		}
		break;
	default:
		taskAssertf(0, "First person type not supported!");
		break;
	}

	// Add in specified offset.
	mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol0(), m_vOffset.GetX());
	mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol1(), m_vOffset.GetY());
	mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol2(), m_vOffset.GetZ());
	
	// Recoil support
	if (m_bUseRecoil)
	{
		m_RecoilData.m_bActive = ProcessRecoil(mtxSource);
	}

	// Secondary motion
	ProcessSecondaryMotion(pPed, mtxSource, qAdditive);

	// Third person setup
	mtxSourceTP = mtxSource;

	if (m_eFirstPersonType == FP_OnFoot)
	{
		const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
		const camFirstPersonPedAimCamera* pAltCamera = camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera();

		if (pCamera)
		{
			const Vector3& vThirdPersonOffset = pCamera->GetThirdPersonOffset();

			mtxSourceTP.GetCol3Ref() = AddScaled(mtxSourceTP.GetCol3(), mtxSourceTP.GetCol0(), ScalarV(vThirdPersonOffset.GetX()));
			mtxSourceTP.GetCol3Ref() = AddScaled(mtxSourceTP.GetCol3(), mtxSourceTP.GetCol1(), ScalarV(vThirdPersonOffset.GetY()));
			mtxSourceTP.GetCol3Ref() = AddScaled(mtxSourceTP.GetCol3(), mtxSourceTP.GetCol2(), ScalarV(vThirdPersonOffset.GetZ()));
		}
		else if (pAltCamera)
		{
			const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
			const CWeaponInfo* pEquippedWeaponInfo = pWeaponManager ? pWeaponManager->GetEquippedWeaponInfo() : NULL;

			if (pEquippedWeaponInfo)
			{
				Vec3V vThirdPersonOffsetV;

				camFirstPersonShooterCamera::GetFirstPersonAsThirdPersonOffset(pPed, pEquippedWeaponInfo, RC_VECTOR3(vThirdPersonOffsetV));
				mtxSourceTP.GetCol3Ref() = AddScaled(mtxSourceTP.GetCol3(), mtxSourceTP.GetCol0(), vThirdPersonOffsetV.GetX());
				mtxSourceTP.GetCol3Ref() = AddScaled(mtxSourceTP.GetCol3(), mtxSourceTP.GetCol1(), vThirdPersonOffsetV.GetY());
				mtxSourceTP.GetCol3Ref() = AddScaled(mtxSourceTP.GetCol3(), mtxSourceTP.GetCol2(), vThirdPersonOffsetV.GetZ());
			}
		}
	}

	// Request
	CIkManager& ikManager = pPed->GetIkManager();
	IkArm arm = (m_uArm == FP_ArmRight) ? IK_ARM_RIGHT : IK_ARM_LEFT;

	// First person
	CIkRequestArm ikRequest(arm, pPed, BONETAG_INVALID, mtxSource.GetCol3Ref(), QuatVFromMat34V(mtxSource), flags);
	ikRequest.SetAdditive(qAdditive);
	ikRequest.SetBlendInRate(m_eBlendInRate);
	ikRequest.SetBlendOutRate(m_eBlendOutRate);
	ikManager.Request(ikRequest, CIkManager::IK_PASS_FPS);

	// Third person
	CIkRequestArm ikRequestTP(arm, pPed, BONETAG_INVALID, mtxSourceTP.GetCol3Ref(), QuatVFromMat34V(mtxSourceTP), flags);
	ikRequestTP.SetAdditive(qAdditive);
	ikRequestTP.SetBlendInRate(m_eBlendInRate);
	ikRequestTP.SetBlendOutRate(m_eBlendOutRate);
	ikManager.Request(ikRequestTP, CIkManager::IK_PASS_MAIN);

	m_bActive = true;
}

void CFirstPersonIkHelper::Reset(CPed* pPed, bool bResetSolver)
{
	m_pWeaponInfo = NULL;

	m_SecondaryMotionData.m_Movement[0].Reset(0.0f);
	m_SecondaryMotionData.m_Movement[1].Reset(0.0f);
	m_SecondaryMotionData.m_Aiming[0].Reset(0.0f);
	m_SecondaryMotionData.m_Aiming[1].Reset(FLT_MAX);
	m_SecondaryMotionData.m_Bounce[0].Reset(0.0f);
	m_SecondaryMotionData.m_Bounce[1].Reset(0.0f);
	m_SecondaryMotionData.m_Bounce[2].Reset(0.0f);
	m_SecondaryMotionData.m_BounceRotation[0].Reset(0.0f);
	m_SecondaryMotionData.m_BounceRotation[1].Reset(0.0f);
	m_SecondaryMotionData.m_BounceRotation[2].Reset(0.0f);
	m_SecondaryMotionData.m_fCurrentMbrY = 0.0f;
	m_SecondaryMotionData.m_fCurrentTranslationBlend = 0.0f;
	m_SecondaryMotionData.m_fCurrentRollScale = 0.0f;
	m_SecondaryMotionData.m_fCurrentPitchScale = 0.0f;
	m_SecondaryMotionData.m_fCurrentBounceScale = 0.0f;
	m_SecondaryMotionData.m_LeftArmRollBoneIdx = -1;
	m_SecondaryMotionData.m_LeftForeArmRollBoneIdx = -1;

	m_RecoilData.m_fDisplacement = 0.0f;
	m_RecoilData.m_fDuration = 0.0f;
	m_RecoilData.m_fMaxDisplacement = 0.0f;
	m_RecoilData.m_fMaxDuration = 0.3f;
	m_RecoilData.m_fScaleBackward = 1.0f;
	m_RecoilData.m_fScaleVertical = 0.5f;
	m_RecoilData.m_bActive = false;

	if (pPed && bResetSolver && m_bActive)
	{
		CIkRequestResetArm ikRequest((m_uArm == FP_ArmRight) ? IK_ARM_RIGHT : IK_ARM_LEFT);
		pPed->GetIkManager().Request(ikRequest);
	}

	m_bActive = false;
}

void CFirstPersonIkHelper::OnFireEvent(const CPed* pPed, const CWeaponInfo* pWeaponInfo)
{
	if (pWeaponInfo == NULL)
	{
		return;
	}

	if (!pWeaponInfo->GetUseFPSAimIK() || pWeaponInfo->GetUseFPSAnimatedRecoil())
	{
		return;
	}

	// Extract fire recoil animation length
	if (pWeaponInfo != m_pWeaponInfo)
	{
		m_pWeaponInfo = pWeaponInfo;

		fwMvClipSetId clipSetId = pWeaponInfo->GetAppropriateWeaponClipSetId(pPed);
		taskAssert(clipSetId != CLIP_SET_ID_INVALID);

		fwClipSet* pClipSet = fwClipSetManager::GetClipSet(clipSetId);
		if (pClipSet)
		{
			static const fwMvClipId kFireClipId("FIRE_RECOIL", 0x16156B9F);
			crClip* pClip = pClipSet->GetClip(kFireClipId);
			float fMaxDuration = 0.3f;

			if (pClip)
			{
				fMaxDuration = pClip->GetDuration();

				if (pClip->GetTags())
				{
					float fTagStart = 0.0f;
					u32 uCount = 0;
	
					crTagIterator it(*pClip->GetTags());
					while (*it)
					{
						const crTag* pTag = *it;

						if (pTag->GetKey() == CClipEventTags::MoveEvent)
						{
							static const crProperty::Key MoveEventKey = crProperty::CalcKey("MoveEvent", 0x7EA9DFC4);
							const crPropertyAttribute* pAttrib = pTag->GetProperty().GetAttribute(MoveEventKey);

							if (pAttrib && (pAttrib->GetType() == crPropertyAttribute::kTypeHashString))
							{
								static const atHashString s_FireHash("Fire", 0xD30A036B);

								if (s_FireHash.GetHash() == static_cast<const crPropertyAttributeHashString*>(pAttrib)->GetHashString().GetHash())
								{
									if (uCount > 0)
									{
										fMaxDuration *= pTag->GetStart() - fTagStart;
										break;
									}
									else
									{
										++uCount;
										fTagStart = pTag->GetStart();
									}
								}
							}
						}

						++it;
					}
				}
			}

			m_RecoilData.m_fMaxDuration = fMaxDuration;
		}
	}

	const CPedMotionData* pMotionData = pPed->GetMotionData();

	m_RecoilData.m_fMaxDisplacement = (pMotionData && pMotionData->GetIsFPSScope()) ? pWeaponInfo->GetIkRecoilDisplacementScope() : pWeaponInfo->GetIkRecoilDisplacement();
	m_RecoilData.m_fScaleBackward = pWeaponInfo->GetIkRecoilDisplacementScaleBackward();
	m_RecoilData.m_fScaleVertical = pWeaponInfo->GetIkRecoilDisplacementScaleVertical();

	m_RecoilData.m_fDisplacement = fwRandom::GetRandomNumberInRange(-m_RecoilData.m_fMaxDisplacement, m_RecoilData.m_fMaxDisplacement);
	m_RecoilData.m_fDuration = 0.0f;
	m_RecoilData.m_bActive = true;
}

float CFirstPersonIkHelper::SampleResponseCurve(float fInterval)
{
	// Interval that the curve is at maximum (1.0f)
	const float kfIntervalAtMaximum = 0.2f;

	float fSample = 0.0f;
	float fAngle;

	// Curve is Cos for [0, kfIntervalAtMaximum] and Cos^3 for [kfIntervalAtMaximum, 1]
	if (fInterval < kfIntervalAtMaximum)
	{
		fAngle = (fInterval / kfIntervalAtMaximum) * HALF_PI;

		// Ramp from 0.0f -> 1.0f
		fSample = rage::Cosf(HALF_PI - fAngle);
	}
	else
	{
		fAngle = ((fInterval - kfIntervalAtMaximum) / (1.0f - kfIntervalAtMaximum)) * HALF_PI;

		// Ramp from 1.0f -> 0.0f
		fSample = rage::Cosf(fAngle);
		fSample *= fSample * fSample;
	}

	return fSample;
}

bool CFirstPersonIkHelper::ProcessRecoil(Mat34V& mtxSource)
{
	if (m_RecoilData.m_bActive)
	{
		const float fTimeStep = fwTimer::GetTimeStep();
		m_RecoilData.m_fDuration += fTimeStep;

		const float fInterval = Clamp(m_RecoilData.m_fDuration / m_RecoilData.m_fMaxDuration, 0.0f, 1.0f);
		const float fSample = SampleResponseCurve(fInterval);

		const float fBackwardScale = (m_RecoilData.m_fMaxDisplacement > 0.0f) ? (1.0f - Clamp(fabsf(m_RecoilData.m_fDisplacement) / m_RecoilData.m_fMaxDisplacement, 0.0f, 1.0f)) * m_RecoilData.m_fScaleBackward : 0.0f;

		const ScalarV vLateral(fSample * m_RecoilData.m_fDisplacement);
		const ScalarV vBackward(fSample * fBackwardScale * -m_RecoilData.m_fMaxDisplacement);
		const ScalarV vVertical(Scale(Abs(vLateral), ScalarV(m_RecoilData.m_fScaleVertical)));

		mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol0(), vLateral);
		mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol1(), vBackward);
		mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol2(), vVertical);

		return (fInterval != 1.0f);
	}

	return false;
}

bool CFirstPersonIkHelper::ProcessSecondaryMotion(CPed* pPed, Mat34V& mtxSource, QuatV& qAdditive)
{
	TUNE_GROUP_BOOL(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_DISABLE, false);
	TUNE_GROUP_BOOL(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_OUTPUT, false);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MOVEMENT_ROLL_SPRING, 100.0f, 0.0f, 800.0f, 1.0f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MOVEMENT_ROLL_DAMPING, 0.772f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MOVEMENT_PITCH_SPRING, 20.0f, 0.0f, 800.0f, 1.0f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MOVEMENT_PITCH_DAMPING, 0.95f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MOVEMENT_ROLL_MAX, 1.5f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MOVEMENT_PITCH_MAX, 1.5f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MOVEMENT_PITCH_TOL, 0.10f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_AIMING_ROLL_SPRING, 80.0f, 0.0f, 800.0f, 1.0f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_AIMING_ROLL_DAMPING, 0.90f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_AIMING_PITCH_SPRING, 100.0f, 0.0f, 800.0f, 5.0f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_AIMING_PITCH_DAMPING, 0.40f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_AIMING_ROLL_MAX, 3.0f, 0.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_AIMING_PITCH_MAX, 1.5f, 0.0f, 180.0f, 0.1f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_SPRING, 200.0f, 0.0f, 500.0f, 1.0f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_DAMPING, 0.9f, 0.0f, 2.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_SCALE, 1.0f, 0.0f, 1.0f, 0.001f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_FIRING_SCALE, 0.75f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MBR_BLENDRATE_Y, 7.0f, 0.0f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_TRANS_BLENDRATE, 8.0f, 0.0f, 20.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_ROLL_SCALE_BLENDRATE, 8.0f, 0.0f, 30.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_PITCH_SCALE_BLENDRATE, 8.0f, 0.0f, 30.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_SCALE_BLENDRATE, 8.0f, 0.0f, 30.0f, 0.01f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MBR_X, 0.0f, -5.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MBR_Y, 0.0f, -5.0f, 5.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_MBR_Y_BLD, 0.0f, -5.0f, 5.0f, 0.01f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_ROLL_SCALE_BLD, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_PITCH_SCALE_BLD, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_SCALE_BLD, 0.0f, 0.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_TRANS_BLD, 0.0f, 0.0f, 1.0f, 0.01f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_ROLL, 0.0f, -180.0f, 180.0f, 1.0f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_PITCH, 0.0f, -180.0f, 180.0f, 1.0f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_X, 0.0f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_Y, 0.0f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_Z, 0.0f, -1.0f, 1.0f, 0.01f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_ROT_X, 0.0f, -180.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_ROT_Y, 0.0f, -180.0f, 180.0f, 0.1f);
	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_BOUNCE_ROT_Z, 0.0f, -180.0f, 180.0f, 0.1f);

	TUNE_GROUP_FLOAT(FIRST_PERSON_SECONDARY_MOTION, SECONDARY_MOTION_ATTENUATION_SCALE, 0.75f, 0.0f, 1.0f, 0.001f);

	static float afSecondaryMotionRollScale[CPedMotionData::FPS_MAX] = { 1.0f, 1.0f, 1.0f, 0.25f, 0.0f };
	static float afSecondaryMotionPitchScale[CPedMotionData::FPS_MAX] = { 0.30f, 0.25f, 0.15f, 0.05f, 0.0f };
	static float afSecondaryMotionBounceScale[CPedMotionData::FPS_MAX] = { 1.0f, 1.0f, 0.5f, 0.25f, 0.0f };

	CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
	const CWeapon* pWeapon = pWeaponManager ? pWeaponManager->GetEquippedWeapon() : NULL;
	const CWeaponModelInfo* pWeaponModelInfo = pWeapon ? pWeapon->GetWeaponModelInfo() : NULL;
	const crSkeleton* pWeaponSkeleton = pWeapon && pWeapon->GetEntity() ? pWeapon->GetEntity()->GetSkeleton() : NULL;
	const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();

	if (!m_bUseSecondaryMotion || !pWeaponModelInfo || !pWeaponSkeleton || !pCamera || !pCamera->IsIkSecondaryMotionAllowed() || SECONDARY_MOTION_DISABLE)
	{
		m_SecondaryMotionData.m_Movement[0].Reset(0.0f);
		m_SecondaryMotionData.m_Movement[1].Reset(0.0f);
		m_SecondaryMotionData.m_Aiming[0].Reset(0.0f);
		m_SecondaryMotionData.m_Aiming[1].Reset(FLT_MAX);
		m_SecondaryMotionData.m_Bounce[0].Reset(0.0f);
		m_SecondaryMotionData.m_Bounce[1].Reset(0.0f);
		m_SecondaryMotionData.m_Bounce[2].Reset(0.0f);
		m_SecondaryMotionData.m_BounceRotation[0].Reset(0.0f);
		m_SecondaryMotionData.m_BounceRotation[1].Reset(0.0f);
		m_SecondaryMotionData.m_BounceRotation[2].Reset(0.0f);
		m_SecondaryMotionData.m_fCurrentMbrY = 0.0f;
		m_SecondaryMotionData.m_fCurrentTranslationBlend = 0.0f;
		m_SecondaryMotionData.m_fCurrentRollScale = 0.0f;
		m_SecondaryMotionData.m_fCurrentPitchScale = 0.0f;
		m_SecondaryMotionData.m_fCurrentBounceScale = 0.0f;

		return false;
	}

	const camControlHelper* pControl = pCamera->GetControlHelper();
	const CPedMotionData* pMotionData = pPed->GetMotionData();
	const bool bAllow = !pMotionData->GetIsSprinting();
	const bool bFiring = pPed->GetPlayerInfo() && pPed->GetPlayerInfo()->IsFiring();
	const float fFiringScale = bFiring ? SECONDARY_MOTION_FIRING_SCALE : 1.0f;
	const float fTimeStep = fwTimer::GetTimeStep();

	const int iFPSState = Clamp(pMotionData->GetFPSState(), (int)CPedMotionData::FPS_IDLE, (int)(CPedMotionData::FPS_MAX - 1));
	m_SecondaryMotionData.m_fCurrentRollScale = Lerp(Min(SECONDARY_MOTION_ROLL_SCALE_BLENDRATE * fTimeStep, 1.0f), m_SecondaryMotionData.m_fCurrentRollScale, afSecondaryMotionRollScale[iFPSState]);
	m_SecondaryMotionData.m_fCurrentPitchScale = Lerp(Min(SECONDARY_MOTION_PITCH_SCALE_BLENDRATE * fTimeStep, 1.0f), m_SecondaryMotionData.m_fCurrentPitchScale, afSecondaryMotionPitchScale[iFPSState]);
	m_SecondaryMotionData.m_fCurrentBounceScale = Lerp(Min(SECONDARY_MOTION_BOUNCE_SCALE_BLENDRATE * fTimeStep, 1.0f), m_SecondaryMotionData.m_fCurrentBounceScale, afSecondaryMotionBounceScale[iFPSState]);

	if(pCamera && pCamera->IsIkSecondaryMotionAttenuated())
	{
		m_SecondaryMotionData.m_fCurrentRollScale   *= SECONDARY_MOTION_ATTENUATION_SCALE;
		m_SecondaryMotionData.m_fCurrentPitchScale  *= SECONDARY_MOTION_ATTENUATION_SCALE;
		m_SecondaryMotionData.m_fCurrentBounceScale *= SECONDARY_MOTION_ATTENUATION_SCALE;
	}

	// Movement contribution
	const float fDesiredMbrX = pMotionData->GetDesiredMbrX();
	const float fDesiredMbrY = pMotionData->GetDesiredMbrY();

	m_SecondaryMotionData.m_fCurrentMbrY = Lerp(Min(SECONDARY_MOTION_MBR_BLENDRATE_Y * fTimeStep, 1.0f), m_SecondaryMotionData.m_fCurrentMbrY, fDesiredMbrY);

	float fDesiredRollMovement = 0.0f;
	float fDesiredPitchMovement = 0.0f;

	if (bAllow)
	{
		if (fDesiredMbrX != 0.0f)
		{
			fDesiredRollMovement = fDesiredMbrX * (SECONDARY_MOTION_MOVEMENT_ROLL_MAX * m_SecondaryMotionData.m_fCurrentRollScale * fFiringScale) * DtoR;
		}

		if (!pMotionData->GetIsFPSScope() && (fabsf(fDesiredMbrY) > 0.01f) && (fabsf(fDesiredMbrY - m_SecondaryMotionData.m_fCurrentMbrY) > SECONDARY_MOTION_MOVEMENT_PITCH_TOL))
		{
			fDesiredPitchMovement = -m_SecondaryMotionData.m_fCurrentMbrY * (SECONDARY_MOTION_MOVEMENT_PITCH_MAX * fFiringScale) * DtoR;
		}
	}

	m_SecondaryMotionData.m_Movement[0].Update(fDesiredRollMovement, SECONDARY_MOTION_MOVEMENT_ROLL_SPRING, SECONDARY_MOTION_MOVEMENT_ROLL_DAMPING);
	m_SecondaryMotionData.m_Movement[1].Update(fDesiredPitchMovement, SECONDARY_MOTION_MOVEMENT_PITCH_SPRING, SECONDARY_MOTION_MOVEMENT_PITCH_DAMPING);

	fDesiredRollMovement = m_SecondaryMotionData.m_Movement[0].GetResult();
	fDesiredPitchMovement = m_SecondaryMotionData.m_Movement[1].GetResult();

	// Bounce contribution
	Vec3V vDesiredBounce(V_ZERO);
	Vec3V vDesiredBounceRotation(V_ZERO);

	crSkeleton* pSkeleton = pPed->GetSkeleton();
	if (pSkeleton)
	{
		if (m_SecondaryMotionData.m_LeftArmRollBoneIdx == -1)
		{
			pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_L_ARMROLL, m_SecondaryMotionData.m_LeftArmRollBoneIdx);
		}

		if (m_SecondaryMotionData.m_LeftForeArmRollBoneIdx == -1)
		{
			pSkeleton->GetSkeletonData().ConvertBoneIdToIndex((u16)BONETAG_L_FOREARMROLL, m_SecondaryMotionData.m_LeftForeArmRollBoneIdx);
		}

		if (m_SecondaryMotionData.m_LeftArmRollBoneIdx >= 0)
		{
			vDesiredBounce = Scale(Mat34VToEulersXYZ(pSkeleton->GetLocalMtx(m_SecondaryMotionData.m_LeftArmRollBoneIdx)), ScalarV(V_TO_DEGREES)); 
			vDesiredBounce = Scale(vDesiredBounce, ScalarV(SECONDARY_MOTION_BOUNCE_SCALE * fFiringScale));
		}

		if (m_SecondaryMotionData.m_LeftForeArmRollBoneIdx >= 0)
		{
			vDesiredBounceRotation = Mat34VToEulersXYZ(pSkeleton->GetLocalMtx(m_SecondaryMotionData.m_LeftForeArmRollBoneIdx)); 
			vDesiredBounceRotation = Scale(vDesiredBounceRotation, ScalarV(SECONDARY_MOTION_BOUNCE_SCALE * fFiringScale));
		}
	}

	m_SecondaryMotionData.m_Bounce[0].Update(vDesiredBounce.GetXf(), SECONDARY_MOTION_BOUNCE_SPRING, SECONDARY_MOTION_BOUNCE_DAMPING);
	m_SecondaryMotionData.m_Bounce[1].Update(vDesiredBounce.GetYf(), SECONDARY_MOTION_BOUNCE_SPRING, SECONDARY_MOTION_BOUNCE_DAMPING);
	m_SecondaryMotionData.m_Bounce[2].Update(vDesiredBounce.GetZf(), SECONDARY_MOTION_BOUNCE_SPRING, SECONDARY_MOTION_BOUNCE_DAMPING);

	vDesiredBounce.SetXf(m_SecondaryMotionData.m_Bounce[0].GetResult());
	vDesiredBounce.SetYf(m_SecondaryMotionData.m_Bounce[1].GetResult());
	vDesiredBounce.SetZf(m_SecondaryMotionData.m_Bounce[2].GetResult());

	m_SecondaryMotionData.m_BounceRotation[0].Update(vDesiredBounceRotation.GetXf(), SECONDARY_MOTION_BOUNCE_SPRING, SECONDARY_MOTION_BOUNCE_DAMPING);
	m_SecondaryMotionData.m_BounceRotation[1].Update(vDesiredBounceRotation.GetYf(), SECONDARY_MOTION_BOUNCE_SPRING, SECONDARY_MOTION_BOUNCE_DAMPING);
	m_SecondaryMotionData.m_BounceRotation[2].Update(vDesiredBounceRotation.GetZf(), SECONDARY_MOTION_BOUNCE_SPRING, SECONDARY_MOTION_BOUNCE_DAMPING);

	vDesiredBounceRotation.SetXf(m_SecondaryMotionData.m_BounceRotation[0].GetResult());
	vDesiredBounceRotation.SetYf(m_SecondaryMotionData.m_BounceRotation[1].GetResult());
	vDesiredBounceRotation.SetZf(m_SecondaryMotionData.m_BounceRotation[2].GetResult());

	// Aiming contribution
	Vec3V vEulersSource(Mat34VToEulersXYZ(mtxSource));
	const ScalarV vPitchLimit(60.0f);
	const ScalarV vToDegrees(V_TO_DEGREES);
	const ScalarV vPitchDiff(Clamp(Scale(vEulersSource.GetX(), vToDegrees), Negate(vPitchLimit), vPitchLimit));

	float fDesiredRollAiming = 0.0f;
	float fDesiredPitchAiming = vPitchDiff.Getf();

	if (bAllow)
	{
		const float fDesiredAimX = pControl ? pControl->GetNormLookAroundHeadingOffset() : 0.0f;

		if (fDesiredAimX != 0.0f)
		{
			fDesiredRollAiming = -fDesiredAimX * (SECONDARY_MOTION_AIMING_ROLL_MAX * m_SecondaryMotionData.m_fCurrentRollScale * fFiringScale) * DtoR;
		}
	}
	else
	{
		m_SecondaryMotionData.m_Aiming[1].Reset(FLT_MAX);
	}

	m_SecondaryMotionData.m_Aiming[0].Update(fDesiredRollAiming, SECONDARY_MOTION_AIMING_ROLL_SPRING, SECONDARY_MOTION_AIMING_ROLL_DAMPING); 

	if (m_SecondaryMotionData.m_Aiming[1].GetResult() != FLT_MAX)
	{
		m_SecondaryMotionData.m_Aiming[1].Update(fDesiredPitchAiming, SECONDARY_MOTION_AIMING_PITCH_SPRING, SECONDARY_MOTION_AIMING_PITCH_DAMPING);
	}
	else
	{
		m_SecondaryMotionData.m_Aiming[1].Reset(fDesiredPitchAiming);
	}

	fDesiredRollAiming = m_SecondaryMotionData.m_Aiming[0].GetResult();
	fDesiredPitchAiming = (m_SecondaryMotionData.m_Aiming[1].GetResult() - fDesiredPitchAiming) * (m_SecondaryMotionData.m_fCurrentPitchScale * fFiringScale) * DtoR;

	// Apply
	const float fDesiredRoll = fDesiredRollMovement + fDesiredRollAiming;
	const float fDesiredPitch = fDesiredPitchMovement + fDesiredPitchAiming;

	m_SecondaryMotionData.m_fCurrentTranslationBlend = Lerp(Min(SECONDARY_MOTION_TRANS_BLENDRATE * fTimeStep, 1.0f), m_SecondaryMotionData.m_fCurrentTranslationBlend, !pMotionData->GetIsFPSIdle() ? 1.0f : 0.0f);

	if ((fDesiredRoll != 0.0f) || (fDesiredPitch != 0.0f) || IsGreaterThanAll(Mag(vDesiredBounce), ScalarV(V_FLT_SMALL_3)) || IsGreaterThanAll(Mag(vDesiredBounceRotation), ScalarV(V_FLT_SMALL_3)))
	{
		vDesiredBounce = Scale(vDesiredBounce, ScalarV(m_SecondaryMotionData.m_fCurrentBounceScale));
		vDesiredBounceRotation = Scale(vDesiredBounceRotation, ScalarV(m_SecondaryMotionData.m_fCurrentBounceScale));

		qAdditive = Multiply(qAdditive, QuatVFromAxisAngle(mtxSource.GetCol1(), ScalarV(fDesiredRoll + vDesiredBounceRotation.GetYf())));
		qAdditive = Multiply(qAdditive, QuatVFromAxisAngle(mtxSource.GetCol2(), ScalarV(vDesiredBounceRotation.GetZf())));
		qAdditive = Multiply(qAdditive, QuatVFromAxisAngle(mtxSource.GetCol0(), ScalarV(fDesiredPitch + vDesiredBounceRotation.GetXf())));

		// Translate effector to compensate for additive rotation
		ScalarV vWeaponLength(0.05f);
		ScalarV vWeaponHeight(0.05f);

		int muzzleBoneIdx = pWeaponModelInfo->GetBoneIndex(WEAPON_MUZZLE);
		int gripBoneIdx = pWeaponModelInfo->GetBoneIndex(WEAPON_GRIP_R);

		if ((muzzleBoneIdx != -1) && (gripBoneIdx != -1))
		{
			const Mat34V& mtxMuzzle = pWeaponSkeleton->GetSkeletonData().GetDefaultTransform(muzzleBoneIdx);
			const Mat34V& mtxGrip = pWeaponSkeleton->GetSkeletonData().GetDefaultTransform(gripBoneIdx);

			Vec3V vDifference(Subtract(mtxMuzzle.GetCol3(), mtxGrip.GetCol3()));
			vWeaponLength = MagXY(vDifference);
			vWeaponHeight = MagYZ(vDifference);
		}

		ScalarV vLateralOffset(Negate(Scale(Tan(ScalarV(fDesiredRoll)), vWeaponHeight)));
		ScalarV vVerticalOffset(Negate(Scale(Tan(ScalarV(fDesiredPitch)), vWeaponLength)));

		const ScalarV vTranslationBlend(m_SecondaryMotionData.m_fCurrentTranslationBlend);
		vLateralOffset = Scale(vLateralOffset, vTranslationBlend);
		vVerticalOffset = Scale(vVerticalOffset, vTranslationBlend);

		mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol0(), vLateralOffset);
		mtxSource.GetCol3Ref() = AddScaled(mtxSource.GetCol3(), mtxSource.GetCol2(), vVerticalOffset);

		// Add bounce contribution
		mtxSource.GetCol3Ref() = Add(mtxSource.GetCol3(), vDesiredBounce);
	}

#if __BANK
	if (SECONDARY_MOTION_OUTPUT)
	{
		SECONDARY_MOTION_MBR_X = fDesiredMbrX;
		SECONDARY_MOTION_MBR_Y = fDesiredMbrY;
		SECONDARY_MOTION_MBR_Y_BLD = m_SecondaryMotionData.m_fCurrentMbrY;
		SECONDARY_MOTION_TRANS_BLD = m_SecondaryMotionData.m_fCurrentTranslationBlend;
		SECONDARY_MOTION_ROLL_SCALE_BLD = m_SecondaryMotionData.m_fCurrentRollScale;
		SECONDARY_MOTION_PITCH_SCALE_BLD = m_SecondaryMotionData.m_fCurrentPitchScale;
		SECONDARY_MOTION_BOUNCE_SCALE_BLD = m_SecondaryMotionData.m_fCurrentBounceScale;
		SECONDARY_MOTION_ROLL = fDesiredRoll * RtoD;
		SECONDARY_MOTION_PITCH = fDesiredPitch * RtoD;
		SECONDARY_MOTION_BOUNCE_X = m_SecondaryMotionData.m_Bounce[0].GetResult();
		SECONDARY_MOTION_BOUNCE_Y = m_SecondaryMotionData.m_Bounce[1].GetResult();
		SECONDARY_MOTION_BOUNCE_Z = m_SecondaryMotionData.m_Bounce[2].GetResult();
		SECONDARY_MOTION_BOUNCE_ROT_X = m_SecondaryMotionData.m_BounceRotation[0].GetResult() * RtoD;
		SECONDARY_MOTION_BOUNCE_ROT_Y = m_SecondaryMotionData.m_BounceRotation[1].GetResult() * RtoD;
		SECONDARY_MOTION_BOUNCE_ROT_Z = m_SecondaryMotionData.m_BounceRotation[2].GetResult() * RtoD;
	}
#endif // __BANK

	return true;
}

bool CFirstPersonIkHelper::AcceptAnyCHWeightForCover(const CPed& rPed)
{
	if (!rPed.GetIsInCover())
		return false;

	TUNE_GROUP_BOOL(FIRST_PERSON_COVER_TUNE, APPLY_SHOTGUN_OUTRO_FIX, false);
	if (!APPLY_SHOTGUN_OUTRO_FIX)
		return true;

	if (!rPed.GetEquippedWeaponInfo() || !rPed.GetEquippedWeaponInfo()->GetNeedsGunCockingInCover())
		return true;

	// Hack B*2102364 : Blend out the fps ik earlier for certain weapons in high left cover during the aim outro as the left hand arm ik cannot reach the grip
	const CTaskCover* pCoverTask = static_cast<const CTaskCover*>(rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_COVER));
	const bool bHighLeftCover = pCoverTask ? (pCoverTask->IsCoverFlagSet(CTaskCover::CF_FacingLeft) && pCoverTask->IsCoverFlagSet(CTaskCover::CF_TooHighCoverPoint)) : false;
	if (!bHighLeftCover)
		return true;

	if (!rPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_AIM_GUN_FROM_COVER_OUTRO))
	{
		return true;
	}

	return false;
}

bool CFirstPersonIkHelper::ProcessLook(CPed* pPed, Mat34V& mtxEntity)
{
	taskAssertf(pPed->IsFirstPersonShooterModeEnabledForPlayer(false), "Ped not in first person mode");
	const camFirstPersonShooterCamera* pCamera = camInterface::GetGameplayDirector().GetFirstPersonShooterCamera();
	const camFirstPersonPedAimCamera* pAltCamera = camInterface::GetGameplayDirector().GetFirstPersonPedAimCamera();

	Mat34V mtxSource(V_IDENTITY);

	if (pCamera)
	{
		if (!pCamera->GetObjectSpaceCameraMatrix(pPed, RC_MATRIX34(mtxSource)))
		{
			mtxSource = MATRIX34_TO_MAT34V(pCamera->GetFrame().GetWorldMatrix());
			UnTransformFull(mtxSource, mtxEntity, mtxSource);
		}
	}
	else if (pAltCamera)
	{
		if (!pAltCamera->GetObjectSpaceCameraMatrix(pPed, RC_MATRIX34(mtxSource)))
		{
			mtxSource = MATRIX34_TO_MAT34V(pAltCamera->GetFrame().GetWorldMatrix());
			UnTransformFull(mtxSource, mtxEntity, mtxSource);
		}
	}

	if (pCamera || pAltCamera)
	{
		Vec3V vLookOffset(AddScaled(mtxSource.GetCol3(), mtxSource.GetCol1(), ScalarV(3.0f)));

		CIkRequestBodyLook ikRequestLook(pPed, BONETAG_INVALID, vLookOffset, LOOKIK_DISABLE_EYE_OPTIMIZATION, CIkManager::IK_LOOKAT_VERY_LOW);
		ikRequestLook.SetRotLimHead(LOOKIK_ROT_ANGLE_YAW, LOOKIK_ROT_LIM_WIDE);
		ikRequestLook.SetRotLimHead(LOOKIK_ROT_ANGLE_PITCH, LOOKIK_ROT_LIM_WIDE);
		ikRequestLook.SetHeadAttitude(LOOKIK_HEAD_ATT_LOW);
		ikRequestLook.SetTurnRate(LOOKIK_TURN_RATE_FAST);

		CIkManager& ikManager = pPed->GetIkManager();
		ikManager.Request(ikRequestLook, CIkManager::IK_PASS_MAIN);
		ikManager.Request(ikRequestLook, CIkManager::IK_PASS_FPS);

		return true;
	}

	return false;
}

#endif // FPS_MODE_SUPPORTED

////////////////////////////////////////////////////////////////////////////////

bool CTargetInWaterHelper::IsInWater(const CEntity& rEntity)
{
	const CEntity* pRelevantEntity = &rEntity;
	if(rEntity.GetIsTypePed())
	{
		const CPed& rPed = static_cast<const CPed &>(rEntity);
		if(rPed.GetIsInVehicle())
		{
			pRelevantEntity = rPed.GetVehiclePedInside();
		}
		else if(rPed.GetIsOnMount())
		{
			pRelevantEntity = rPed.GetMountPedOn();
		}
	}

	if(pRelevantEntity->GetIsPhysical())
	{
		const CPhysical* pPhysical = static_cast<const CPhysical *>(pRelevantEntity);

		if(pPhysical->m_Buoyancy.GetStatus() != NOT_IN_WATER)
		{
			return true;
		}
		else if(pPhysical->GetIsInWater())
		{
			return true;
		}
	}

	if(pRelevantEntity->GetIsTypeVehicle())
	{
		const CVehicle* pVehicle = static_cast<const CVehicle *>(pRelevantEntity);

		if(pVehicle->GetIntelligence()->HasBeenInWater() && (pVehicle->GetIntelligence()->GetTimeSinceLastInWater() < 3000))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

bool CCreekHackHelperGTAV::IsAffected(const CVehicle& rVehicle)
{
	const CPed* pDriver = rVehicle.GetDriver();

	bool bIsAffected = (pDriver && pDriver->PopTypeIsRandom() && pDriver->IsLawEnforcementPed()) ||
		(!pDriver && rVehicle.PopTypeIsRandom() && rVehicle.IsLawEnforcementVehicle());
	if(!bIsAffected)
	{
		return false;
	}

	const CVehicleNodeList* pNodeList = rVehicle.GetIntelligence()->GetNodeList();
	if(pNodeList)
	{
		const s32 iTargetNodeIndex = pNodeList->GetTargetNodeIndex();
		const CPathNode* pTargetNode = pNodeList->GetPathNode(iTargetNodeIndex);
		if(pTargetNode && pTargetNode->m_1.m_Offroad && (pTargetNode->m_2.m_speed == 1))
		{
			return true;
		}
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////
// CLASS: CPedsWaitingToBeOutOfSightManager::CPedInfo
// PURPOSE: Manage peds waiting for being out of players sight to execute some action (Eg: repath if they are currently fleeing out of pavement).
/////////////////////////////////////////////////////////////////////////////

// PURPOSE: Init the queue of peds waiting to repath
void CPedsWaitingToBeOutOfSightManager::Init(const u32 uQueueSize, u32 uMinMillisecondsBetweenShapeTests, u32 uNumConsecutiveChecksToSucceed, FnIsValidCB fnIsValid, FnOnOutOfSightCB fnOnOutOfSight)
{
	taskAssert(uQueueSize > 0 && fnIsValid && fnOnOutOfSight);

	m_fnIsValid = fnIsValid; 
	m_uLastLOSCheckTS = 0u;
	m_uMinMillisecondsBetweenShapeTests = uMinMillisecondsBetweenShapeTests;
	m_uNumConsecutiveChecksToSucceed = uNumConsecutiveChecksToSucceed;
	n_fnOnOutOfSight = fnOnOutOfSight;
	m_aPeds.AllocateNodes(uQueueSize);
}

// PURPOSE: Release the queue of peds waiting to repath
void CPedsWaitingToBeOutOfSightManager::Done()
{
	m_aPeds.Reset();
	m_aPeds.DeallocateNodes();
}

//
bool CPedsWaitingToBeOutOfSightManager::Add(CPed& rPed)
{
	if (!taskVerifyf(NetworkInterface::IsGameInProgress(), "CPedsWaitingToRepathManager::Add, CPedsWaitingToRepathManager should be used just for multiplayer games"))
	{
		return false;
	}

	if (!taskVerifyf(!Find(rPed), "CPedsWaitingToRepathManager::Add, ped already in the queue!"))
	{
		return false;
	}

	if (m_aPeds.GetNumNodes() >= m_aPeds.GetMaxNodes())
	{
		taskErrorf("CPedsWaitingToRepathManager::Add, the queue is full! Discarding ped.");
		return false;
	}

	atSNode<CPedInfo>* pNode = m_aPeds.RequestNode();
	taskAssert(pNode);

	pNode->Data.Init(rPed);

	m_aPeds.Append(*pNode);
	pNode->Data.OnQueued();

	return true;
}

//
void CPedsWaitingToBeOutOfSightManager::Remove(CPed& rPed)
{
	atSNode<CPedInfo>* pNode = Find(rPed);
	if (pNode)
	{
		Remove(*pNode);
	}
}

//
void CPedsWaitingToBeOutOfSightManager::Remove(atSNode<CPedInfo>& rNode)
{
	rNode.Data.Done();
	m_aPeds.Remove(rNode);
	m_aPeds.RecycleNode(&rNode);
}

//
void CPedsWaitingToBeOutOfSightManager::ReQueue(atSNode<CPedInfo>& rNode)
{
	if (m_aPeds.GetNumItems() > 1)
	{
		m_aPeds.Remove(rNode);
		m_aPeds.Append(rNode);
	}

	rNode.Data.OnQueued();
}

//
atSNode<CPedsWaitingToBeOutOfSightManager::CPedInfo>* CPedsWaitingToBeOutOfSightManager::Find(const CPed& rPed)
{
	atSNode<CPedInfo>* pFoundNode = NULL;

	for (atSNode<CPedInfo>* pNode = m_aPeds.GetHead(); pNode; pNode = pNode->GetNext())
	{
		if (taskVerifyf(pNode, "Null Node") && pNode->Data.GetPed() == &rPed)
		{
			pFoundNode = pNode;
			break;
		}
	}

	return pFoundNode;
}

// PURPOSE: Process peds waiting to repath
void CPedsWaitingToBeOutOfSightManager::Process()
{
	if (!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	RemoveInvalidNodes();

	// Process next node on the queue
	atSNode<CPedInfo>* pNode = m_aPeds.GetHead();
	if (pNode)
	{
		ProcessNode(pNode);
	}
}


//
void CPedsWaitingToBeOutOfSightManager::ProcessNode(atSNode<CPedInfo>* pNode)
{
	taskAssert(pNode);

	CPedInfo& rPedInfo = pNode->Data;

	switch (rPedInfo.GetState())
	{
	case CPedInfo::SState::QUEUED: 
		{
			rPedInfo.OnStartProcessing();
			rPedInfo.SetState(CPedInfo::SState::IN_PROCESS);

			UpdateNodeState(pNode);
		}
		break;

	case CPedInfo::SState::IN_PROCESS:
		{
			UpdateNodeState(pNode);
		}
		break;

	case CPedInfo::SState::UNDEFINED:
	case CPedInfo::SState::COUNT:
		{
			taskAssert(false);
		}
		break;
	}
}

//
void CPedsWaitingToBeOutOfSightManager::UpdateNodeState(atSNode<CPedInfo>* pNode)
{
	taskAssert(pNode);

	CPedInfo& rPedInfo = pNode->Data;

	CPedInfo::STestResult::Enum eResult = rPedInfo.UpdateCurrentTest(*this);
	switch (eResult)
	{
	case CPedInfo::STestResult::SUCCESS: 
		{
			if (rPedInfo.GetCurrentShapeTestPlayerIdx() + 1 >= netInterface::GetNumActivePlayers())
			{
				rPedInfo.IncNumConsecutiveChecksPassed();
				if (rPedInfo.GetNumConsecutiveChecksPassed() >= m_uNumConsecutiveChecksToSucceed)
				{
					n_fnOnOutOfSight(*rPedInfo.GetPed());
					Remove(*pNode);
				}
				else
				{
					ReQueue(*pNode);
				}
			}
			else
			{
				rPedInfo.IncCurrentShapeTestPlayerIdx();
			}
		}
		break;

	case CPedInfo::STestResult::FAILURE:
	case CPedInfo::STestResult::TEST_ERROR:
		{
			rPedInfo.OnTestFailed();
			ReQueue(*pNode);
		}
		break;

	case CPedInfo::STestResult::COUNT:
		{
			taskAssert(false);
		}
		break;

	default:
		{
			// Nothing to do. Keep processing this ped
		}
		break;
	}
}

// PURPOSE: Remove invalid peds (destroyed, not fleeing, etc.) from the queue
void CPedsWaitingToBeOutOfSightManager::RemoveInvalidNodes()
{
	for (atSNode<CPedInfo>* pNode = m_aPeds.GetHead(); pNode; )
	{
		atSNode<CPedInfo>* pNextNode = pNode->GetNext();

		CPedInfo& rPedInfo = pNode->Data;
		if (!rPedInfo.IsValid() || !m_fnIsValid(*rPedInfo.GetPed()))
		{
			rPedInfo.Done();
			m_aPeds.Remove(*pNode);
			m_aPeds.RecycleNode(pNode);
		}

		pNode = pNextNode;
	}
}

//
void CPedsWaitingToBeOutOfSightManager::SubmitShapeTest(const Vector3& vStart, const Vector3& vEnd, WorldProbe::CShapeTestSingleResult* phCurrentShapeTestHandle)
{
	taskAssert(phCurrentShapeTestHandle);

	WorldProbe::CShapeTestProbeDesc probeData;
	phCurrentShapeTestHandle->Reset();
	probeData.SetResultsStructure(phCurrentShapeTestHandle);

	taskAssert(!vStart.IsZero() && !vEnd.IsZero());
	probeData.SetStartAndEnd(vStart, vEnd);
	probeData.SetContext(WorldProbe::LOS_GeneralAI);
	probeData.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
	probeData.SetIsDirected(true);

	WorldProbe::GetShapeTestManager()->SubmitTest(probeData, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);
	m_uLastLOSCheckTS = fwTimer::GetTimeInMilliseconds();
}

#if __BANK

//
void CPedsWaitingToBeOutOfSightManager::DEBUG_RenderList()
{
	if (!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	static const Vector3 ScreenCoors(0.1f, 0.2f, 0.0f);
	static const Vector2 vTextRenderScale(0.3f, 0.3f);
	static const float fVertDist = 0.025f;

	const unsigned uNumActivePlayers = netInterface::GetNumActivePlayers();

	int iNoOfTexts=0;
	Vector2 vTextRenderPos(ScreenCoors.x, ScreenCoors.y);

	Color32 txtCol(Color_white);
	char debugText[256];

	for (atSNode<CPedInfo>* pNode = m_aPeds.GetHead(); pNode; pNode = pNode->GetNext())
	{
		CPedInfo& rPedInfo = pNode->Data;

		if (rPedInfo.IsValid())
		{
			formatf( debugText, "Ped [%s], State = %s, Check [%d/%d], Player Test [%d/%d], LastTestResult = %s, InManagerTime [%f], InProcessTime [%f]", 
				rPedInfo.GetPed()->GetDebugName(), 
				CPedInfo::SState::GetName(rPedInfo.GetState()), 
				rPedInfo.GetNumConsecutiveChecksPassed(),
				m_uNumConsecutiveChecksToSucceed,
				rPedInfo.GetCurrentShapeTestPlayerIdx(), 
				uNumActivePlayers, 
				CPedInfo::STestResult::GetName(rPedInfo.GetLastTestResult()),
				rPedInfo.GetInManagerTime(), 
				rPedInfo.GetInProcessTime());
			vTextRenderPos.y = ScreenCoors.y + (iNoOfTexts++)*fVertDist;
			grcDebugDraw::Text(vTextRenderPos, txtCol, debugText);
		}
	}

}

#endif // __BANK

//----------------------------------------------------------------------------
// CLASS: CPedsWaitingToBeOutOfSightManager::CPedInfo
//----------------------------------------------------------------------------

//
const char* CPedsWaitingToBeOutOfSightManager::CPedInfo::SState::GetName(Enum eState)
{
	static const char* apszNames[] =
	{
		"Undefined",
		"Queued",
		"InProcess"
	};

	CompileTimeAssert(sizeof(apszNames) / sizeof(const char*) == COUNT);

	const u32 uStateIdx = static_cast<u32>(eState);
	return uStateIdx < COUNT ? apszNames[uStateIdx] : "[Invalid State]";
}

//
const char* CPedsWaitingToBeOutOfSightManager::CPedInfo::STestResult::GetName(Enum eResult)
{
	static const char* apszNames[] =
	{
		"Undefined",
		"Success",
		"Failure",
		"Delayed",
		"WaitingResult",
		"Error"
	};

	CompileTimeAssert(sizeof(apszNames) / sizeof(const char*) == COUNT);

	const u32 uResultIdx = static_cast<u32>(eResult);
	return uResultIdx < COUNT ? apszNames[uResultIdx] : "[Invalid Result]";
}

//
CPedsWaitingToBeOutOfSightManager::CPedInfo::CPedInfo() 
	: m_uAddedToManagerTS(0u)
	, m_uStartProcessedTS(0u)
	, m_iCurrentShapeTestPlayerIdx(0)
	, m_uNumConsecutiveChecksPassed(0u)
	, m_eState(SState::UNDEFINED)
	, m_eLastTestResult(STestResult::UNDEFINED)
{

}


//
void CPedsWaitingToBeOutOfSightManager::CPedInfo::Init(CPed& rPed)
{
	m_pPed = &rPed;
	m_uAddedToManagerTS = fwTimer::GetTimeInMilliseconds();
	m_uStartProcessedTS = 0u;
	m_iCurrentShapeTestPlayerIdx = 0; 
	m_uNumConsecutiveChecksPassed = 0u;
	m_eState = SState::UNDEFINED;
	m_eLastTestResult = STestResult::UNDEFINED;
}

//
void CPedsWaitingToBeOutOfSightManager::CPedInfo::Done()
{
	m_pPed = NULL;
	m_hCurrentShapeTestHandle.Reset(); 
	m_uStartProcessedTS = 0u;
	m_iCurrentShapeTestPlayerIdx = 0; 
	m_uNumConsecutiveChecksPassed = 0u;
}

//
bool CPedsWaitingToBeOutOfSightManager::CPedInfo::IsValid()
{
	return m_pPed != NULL;
}

//
CPedsWaitingToBeOutOfSightManager::CPedInfo::STestResult::Enum CPedsWaitingToBeOutOfSightManager::CPedInfo::UpdateCurrentTest(CPedsWaitingToBeOutOfSightManager& rManager)
{
	m_eLastTestResult = STestResult::TEST_ERROR;

	// Error conditions
	if (!taskVerifyf(GetCurrentShapeTestPlayerIdx() >= 0, "Invalid player Idx!"))
	{
		return m_eLastTestResult;
	}

	const unsigned uNumActivePlayers = netInterface::GetNumActivePlayers();
	if (GetCurrentShapeTestPlayerIdx() >= uNumActivePlayers)
	{
		return m_eLastTestResult;
	}

	// Automatic failure!
	if (GetPed()->GetIsVisibleInSomeViewportThisFrame())
	{
		m_eLastTestResult = STestResult::FAILURE;
		return m_eLastTestResult;
	}

	// Update current check
	const netPlayer* const* apAllActivePlayers = netInterface::GetAllActivePlayers();
	const CNetGamePlayer* const pPlayer = SafeCast(const CNetGamePlayer, apAllActivePlayers[GetCurrentShapeTestPlayerIdx()]);
	if (pPlayer)
	{	
		grcViewport *pPlayerViewport = pPlayer->GetPlayerPed() && pPlayer->GetPlayerPed()->IsLocalPlayer() ? &gVpMan.GetGameViewport()->GetNonConstGrcViewport() : NetworkUtils::GetNetworkPlayerViewPort(*pPlayer);
		if (pPlayerViewport)
		{
			const Vector3 vCameraPos = VEC3V_TO_VECTOR3(pPlayerViewport->GetCameraPosition());
			const Vector3 vPedPos = VEC3V_TO_VECTOR3(m_pPed->GetTransform().GetPosition());
			const float fCameraToPedDistance2 = vCameraPos.Dist2(vPedPos); 

			static const float fMAX_CHECK_DISTANCE_2 = 150.0f * 150.0f;
			if (fCameraToPedDistance2 > fMAX_CHECK_DISTANCE_2)
			{
				// Automatic success!
				m_eLastTestResult = STestResult::SUCCESS;
			}
			else
			{
				// Check LOS
				bool bShapeTestInProgress = (m_hCurrentShapeTestHandle.GetResultsStatus() != WorldProbe::TEST_NOT_SUBMITTED);
				if (!bShapeTestInProgress)
				{
					if (rManager.CanSubmitShapeTest())
					{
						rManager.SubmitShapeTest(vCameraPos, vPedPos, &m_hCurrentShapeTestHandle);
						bShapeTestInProgress = true;
					}
					else
					{
						m_eLastTestResult = STestResult::DELAYED;
					}
				}

				if (bShapeTestInProgress)
				{
					switch (m_hCurrentShapeTestHandle.GetResultsStatus())
					{
					case WorldProbe::WAITING_ON_RESULTS: { m_eLastTestResult = STestResult::WAITING_RESULT; } break;
					case WorldProbe::RESULTS_READY:		 
						{ 
							m_eLastTestResult = ProcessShapeTestResult(m_hCurrentShapeTestHandle); 
						} 
						break;
					default: { m_eLastTestResult = STestResult::TEST_ERROR; } break;
					}
				}
			}
		}
	}

	return m_eLastTestResult;
}


//
CPedsWaitingToBeOutOfSightManager::CPedInfo::STestResult::Enum CPedsWaitingToBeOutOfSightManager::CPedInfo::ProcessShapeTestResult(WorldProbe::CShapeTestSingleResult& rResults)
{
	STestResult::Enum eResult = STestResult::FAILURE;

	if (rResults.GetNumHits() > 0)
	{
		const WorldProbe::CShapeTestHitPoint& rResult = rResults[0];
		if (taskVerifyf(!rResult.GetHitPosition().IsZero(), "CPedsWaitingToBeOutOfSightManager::ProcessLOSCheckResult: Invalid hit position!"))
		{
			eResult = STestResult::SUCCESS;
		}
	}

	rResults.Reset();

	return eResult;
}