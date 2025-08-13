// Filename   :	AnimPoseHelper.cpp
// Description:	Helper class embedded in CTaskNMControl to play an authored clip on part of a ragdoll
//              while NM controls the rest (FSM version)

// --- Include Files ------------------------------------------------------------

// Rage headers:
#include "animation/AnimManager.h"
#include "fragmentnm/messageparams.h"
#include "fwanimation/directorcomponentragdoll.h"
#include "fwanimation/pointcloud.h"
#include "bank/slider.h"
// Game headers:
#include "Peds/Ped.h"
#include "Task/Physics/AnimPoseHelper.h"
#include "Task/Physics/TaskNM.h"

// ------------------------------------------------------------------------------

AI_OPTIMISATIONS()

// Tunable parameters ////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#if __BANK
bool CClipPoseHelper::ms_bDebugClipMatch = false;
float CClipPoseHelper::ms_fClipMatchAcc = 0.0f;
bool CClipPoseHelper::ms_bLockClipFirstFrame = false;
bool CClipPoseHelper::ms_bUseWorldCoords = false;
#endif //__BANK
float CClipPoseHelper::m_sfAPmuscleStiffness = 9.0f;
float CClipPoseHelper::m_sfAPstiffness = 9.0f;
float CClipPoseHelper::m_sfAPdamping = 1.0f;
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClipPoseHelper::CClipPoseHelper()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
: m_eAnimPoseTypeHash(CTaskNMBehaviour::ANIM_POSE_ATTACH_DEFAULT),
m_nEffectorMask(NM_MASK_UPPER_BODY),
m_bClipsStreamed(false),
m_bTaskSetsStiffnessValues(false),
m_connectedLeftFoot(-2),
m_connectedRightFoot(-2),
m_connectedLeftHand(0),
m_connectedRightHand(0),
m_ClipStartTime(0),
m_pClip(NULL),
m_bLooped(false)
{
	// Initialise all clip identifiers to show that a clip has not been selected yet.
	// Call one of the overloaded SelectClip() functions to do this.
	m_clipSetId = CLIP_SET_ID_INVALID;
	m_clipId = CLIP_ID_INVALID;
	m_nClipDictionaryIndex = CLIP_DICT_INDEX_INVALID;
	m_nClipNameHash = CLIP_HASH_INVALID;

	m_pAttachParent = NULL;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
CClipPoseHelper::~CClipPoseHelper()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	SetClip(NULL);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::SelectClip(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_clipSetId = clipSetId;
	m_clipId = clipId;
	m_nClipDictionaryIndex = CLIP_DICT_INDEX_INVALID;
	m_nClipNameHash = CLIP_HASH_INVALID;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::SelectClip(s32 nClipDictionaryIndex, u32 nClipNameHash)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	m_clipSetId = CLIP_SET_ID_INVALID;
	m_clipId = CLIP_ID_INVALID;
	m_nClipDictionaryIndex = nClipDictionaryIndex;
	m_nClipNameHash = nClipNameHash;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::SelectClip(const strStreamingObjectName pClipDictName, const char *pClipName)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Instead of looking up the dictionary index and name hash before construction, accept a string
	// for both dictionary and clip name and look it up here.

	m_clipSetId = CLIP_SET_ID_INVALID;
	m_clipId = CLIP_ID_INVALID;
	m_nClipDictionaryIndex = CLIP_DICT_INDEX_INVALID;
	m_nClipNameHash = CLIP_HASH_INVALID;

	m_nClipDictionaryIndex = fwAnimManager::FindSlot(pClipDictName).Get();
	m_nClipNameHash = atStringHash(pClipName);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::SetEffectorMask(eEffectorMaskStrings nEffectorMask)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Configure which body parts will be driven by clip. The other parts will be under physics control.

	m_nEffectorMask = nEffectorMask;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::TaskSetsStiffnessValues(bool bValue)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// If set to true, expect the an AI task to control the stiffness values of the ragdoll. Otherwise, just use
	// defaults for the whole body.

	m_bTaskSetsStiffnessValues = bValue;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClipPoseHelper::RequestClips()
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	//taskAssertf(m_clipSetId != CLIP_SET_ID_INVALID, "Must provide an fwClipSetId and fwMvClipId using SelectClip().");
	//add a check here to see if we have a clip dictionary and a clip before we request using 
	if (m_nClipDictionaryIndex != CLIP_DICT_INDEX_INVALID)
	{
		m_bClipsStreamed = true; 
	}
	else
	{
		m_bClipsStreamed = m_ClipSetRequestHelper.Request(m_clipSetId);
	}

	return m_bClipsStreamed;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClipPoseHelper::StartClip(CPed *)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Kick off the clip which was passed in at construction and use it to set the NM incoming transforms.

	m_ClipStartTime = fwTimer::GetTimeInMilliseconds();

	if(m_nClipDictionaryIndex != CLIP_DICT_INDEX_INVALID && m_nClipNameHash != CLIP_HASH_INVALID)
	{
		// Clip was supplied by dictionary and name.
		const crClip* pClip = fwAnimManager::GetClipIfExistsByDictIndex(m_nClipDictionaryIndex, m_nClipNameHash);
		SetClip(pClip);
		return pClip!=NULL;
	}
	else if(m_clipSetId != CLIP_SET_ID_INVALID && m_clipId != CLIP_ID_INVALID)
	{
		// Clip was supplied by clip set and clip.
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, m_clipId);

		// check for a looped flag
		fwClipSet* pSet = fwClipSetManager::GetClipSet(m_clipSetId);
		if (pSet)
		{
			const fwClipItem* pItm = pSet->GetClipItem(m_clipId);
			if (pItm && pItm->IsClipItemWithProps())
			{
				if (static_cast<const fwClipItemWithProps*>(pItm)->GetFlags() & APF_ISLOOPED)
				{
					m_bLooped = true;
				}
			}
		}

		SetClip(pClip);
		return pClip!=NULL;	
	}
	else
	{
		// Clip hasn't been selected or something else has gone wrong.
		taskAssertf(false, "Clip hasn't been set up properly.");
		return false;
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::StopClip(float )
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// Stop any clip if playing and clear up the resources.
	SetClip(NULL);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClipPoseHelper::Enable(CPed *pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	switch (m_eAnimPoseTypeHash)
	{
	case CTaskNMBehaviour::ANIM_POSE_DEFAULT:
		CTaskNMBehaviour::sm_Tunables.m_animPoseDefault.Post(*pPed, NULL);
		break;
	case CTaskNMBehaviour::ANIM_POSE_ATTACH_DEFAULT:
		CTaskNMBehaviour::sm_Tunables.m_animPoseAttachDefault.Post(*pPed, NULL);
		break;		
	case CTaskNMBehaviour::ANIM_POSE_ATTACH_TO_VEHICLE:
		CTaskNMBehaviour::sm_Tunables.m_animPoseAttachToVehicle.Post(*pPed, NULL);
		break;
	case CTaskNMBehaviour::ANIM_POSE_HANDS_CUFFED:
		CTaskNMBehaviour::sm_Tunables.m_animPoseHandsCuffed.Post(*pPed, NULL);
		break;
	default:
		AssertMsg(0, "state not handled");
		break;
	}

	// Send the attach parent's level index into NM if attached to a vehicle
	if (m_eAnimPoseTypeHash == CTaskNMBehaviour::ANIM_POSE_ATTACH_TO_VEHICLE && m_pAttachParent)
	{
		ART::MessageParams msg;
		msg.addInt("dampenSideMotionInstanceIndex", m_pAttachParent->GetCurrentPhysicsInst()->GetLevelIndex());
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ANIM_POSE_MSG), &msg);
	}

	taskAssertf( ((m_nClipDictionaryIndex != CLIP_DICT_INDEX_INVALID) && (m_nClipNameHash != CLIP_HASH_INVALID)) ||
		((m_clipSetId != CLIP_SET_ID_INVALID) && (m_clipId != CLIP_ID_INVALID)), "No clip selected!" );

	StopClip(INSTANT_BLEND_OUT_DELTA);

	return StartClip(pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::Disable(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	ART::MessageParams msg;
	msg.addBool(NMSTR_PARAM(NM_START), false);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ANIM_POSE_MSG), &msg);

	StopClip(INSTANT_BLEND_OUT_DELTA);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::StartBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	Enable(pPed);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::ControlBehaviour(CPed* pPed)
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
#if __BANK
	if(ms_bDebugClipMatch)
	{
		// Compute some kind of measure of accuracy for NM's pose compared to the incoming transforms.
		fwPoseMatcher::Filter filter;
		// Add this point cloud to the test samples (population = 1).
		filter.AddKey(m_clipSetId, m_clipId);
		if (filter.GetKeyCount() > 0)
		{
			Matrix34 animMatrix(M34_IDENTITY);
			bool bFoundMatch = false;
			if(pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().FindSample(m_clipSetId, m_clipId) != NULL)
			{
				bFoundMatch = pPed->GetAnimDirector()->GetComponent<fwAnimDirectorComponentRagDoll>()->GetPoseMatcher().FindBestMatch(*pPed->GetSkeleton(), m_clipSetId, m_clipId, animMatrix, &filter);
			}

			taskAssertf(bFoundMatch, "Clip doesn't seem to have a point cloud created.");
		}
	}
#endif //__BANK

	// Grab the skeleton from the clip and send this to the NM Agent as the incomingTransforms matrices.
	if(m_pClip)
	{
		const crClip* pClip = m_pClip;
		taskAssert(pClip);

		float fPhase = GetPhase();

#if __BANK
		if(ms_bLockClipFirstFrame)
		{
			pPed->GetRagdollInst()->SetActivePoseFromClip(pClip, 0.0f, ms_bUseWorldCoords);
		}
		else
		{
			pPed->GetRagdollInst()->SetActivePoseFromClip(pClip, fPhase, ms_bUseWorldCoords);
		}
#else
		pPed->GetRagdollInst()->SetActivePoseFromClip(pClip, fPhase);
#endif
	}
	else
	{
		// Clip must have finished. It is up to the behaviour which uses this helper to decide whether to blend
		// out, repeat, or whatever using FinishConditions().

		// TODO RA: Currently, the helper holds the last frame and blats over the pose of NM agent! If this is not
		// the desired result, fix this.
	}
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool CClipPoseHelper::FinishConditions(CPed* UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	// This function exists for parity with the NM behaviour tasks. Use it to query when the clip has finished.

	if (m_ClipStartTime>0 && !m_pClip)
	{
		return true;
	}

	if(GetPhase()>=1.0f && !m_bLooped)
	{
		// Clip is held at phase == 1.0 and not looping.
		return true;
	}

	return false;
}
#if __BANK
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void CClipPoseHelper::InitClipWeights(const CPed *UNUSED_PARAM(pPed))
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
{
	/*
	crWeightSet *pClipWeightSet = CPointCloudManager::sm_pPointCloudWeightSet;
	taskAssert(pClipWeightSet);

	const crSkeletonData &skelData = pPed->GetSkeletonData();
	pClipWeightSet->Init(skelData);

	// Initialise all bone weights.
	for(int i = 0; i < skelData.GetNumBones(); i++)
	{
		pClipWeightSet->SetClipWeight(i, 0.0f);
	}
	*/
}
#endif //__BANK
