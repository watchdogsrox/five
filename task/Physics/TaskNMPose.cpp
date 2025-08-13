// Filename   :	TaskNMPose.cpp
// Description:	Natural Motion pose class (FSM version)

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "crskeleton\Skeleton.h"
#include "fragment\Cache.h"
#include "fragment\Instance.h"
#include "fragment\Type.h"
#include "fragment\TypeChild.h"
#include "fragmentnm\messageparams.h"
#include "pharticulated/articulatedcollider.h"
#include "physics/shapetest.h"

// Framework headers
#include "fwanimation/animmanager.h"
#include "fwanimation/pointcloud.h"
#include "grcore/debugdraw.h"
#include "fwmaths\Angle.h"

// Game headers
#include "camera/CamInterface.h"
#include "Event\EventDamage.h"
#include "Network\NetworkInterface.h"
#include "Peds\Ped.h"
#include "Peds\PedIntelligence.h"
#include "Peds\PedPlacement.h"
#include "PedGroup\PedGroup.h"
#include "Physics\GtaInst.h"
#include "Physics\Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/GameWorld.h"
#include "Task\General\TaskBasic.h"
#include "Task\Movement\Jumping\TaskInAir.h"
#include "Task/Physics/TaskNMPose.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClonedNMPoseInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMPoseInfo::CClonedNMPoseInfo(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fStiffness, float fStiffnessDamping, int iStiffnessMask, bool bPlayClip)
: m_clipSetId(clipSetId)
, m_clipId(clipId)
, m_fStiffness(fStiffness)
, m_fStiffnessDamping(fStiffnessDamping)
, m_iStiffnessMask((u8)iStiffnessMask)
, m_bPlayClip(bPlayClip)
, m_bHasClip(clipSetId != CLIP_SET_ID_INVALID)
, m_bHasStiffness(fStiffness != NM_POSE_DEFAULT_STIFFNESS)
, m_bHasDamping(fStiffnessDamping != NM_POSE_DEFAULT_DAMPING)
{
	Assertf(iStiffnessMask >= 0 && iStiffnessMask < (1<<SIZEOF_STIFFNESS_MASK), "CClonedNMPoseInfo: StiffnessMask (%u) outwith synced range", iStiffnessMask);
}

CClonedNMPoseInfo::CClonedNMPoseInfo()
: m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_fStiffness(0.0f)
, m_fStiffnessDamping(0.0f)
, m_iStiffnessMask(0)
, m_bPlayClip(false)
, m_bHasClip(false)
, m_bHasStiffness(false)
, m_bHasDamping(false)
{
}

CTaskFSMClone *CClonedNMPoseInfo::CreateCloneFSMTask()
{
	return rage_new CTaskNMPose(10000, 10000, m_clipSetId, m_clipId, m_fStiffness, m_fStiffnessDamping, (int)m_iStiffnessMask, m_bPlayClip);
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMPose
//////////////////////////////////////////////////////////////////////////

CTaskNMPose::CTaskNMPose(u32 nMinTime, u32 nMaxTime, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, float fStiffness, float fStiffnessDamping, int iStiffnessMask, bool bPlayClip)
: CTaskNMBehaviour(nMinTime, nMaxTime)
{
	m_clipId = clipId;
	m_clipSetId = clipSetId;
	m_fPhase = 0.0f;
	m_bPlayClip = bPlayClip;
	m_bHasPlayedFullPhase = false;

	// Set up default stiffness values
	for(int iBodyPartIndex = 0; iBodyPartIndex < NM_POSE_NUM_BODY_PARTS; iBodyPartIndex++)
	{
		m_fBodyPartStiffness[iBodyPartIndex] = 12.0f;
		m_fBodyPartDamping[iBodyPartIndex] = 1.0f;
	}

	// Now set stiffness values using mask
	SetStiffness(fStiffness,iStiffnessMask);
	SetDamping(fStiffnessDamping,iStiffnessMask);

	m_fStiffness = fStiffness;
	m_fDamping = fStiffnessDamping;
	m_mask = iStiffnessMask;
	SetInternalTaskType(CTaskTypes::TASK_NM_POSE);
}

CTaskNMPose::~CTaskNMPose()
{
}

aiTask* CTaskNMPose::Copy() const
{
	CTaskNMPose* pNewTask = rage_new CTaskNMPose(m_nMinTime, m_nMaxTime,m_clipSetId,m_clipId,0.0f,0.0f,0,m_bPlayClip);

	// need to set up the stiffness and damping values
	for(int iBodyPartIndex = 0; iBodyPartIndex < NM_POSE_NUM_BODY_PARTS; iBodyPartIndex++)
	{
		int iMask = (1 << iBodyPartIndex);
		pNewTask->SetStiffness(m_fBodyPartStiffness[iBodyPartIndex],iMask);
		pNewTask->SetDamping(m_fBodyPartDamping[iBodyPartIndex],iMask);
	}
	return pNewTask;
}

void CTaskNMPose::SetStiffness(float fStiffness, int poseBodyMask)
{
	int maskIterator = 1;
	for(int iBodyPartIndex = 0; iBodyPartIndex < NM_POSE_NUM_BODY_PARTS; iBodyPartIndex++)
	{
		if(poseBodyMask & maskIterator)
		{
			m_fBodyPartStiffness[iBodyPartIndex] = fStiffness;
		}
		maskIterator = maskIterator << 1;
	}
	m_bStiffnessValuesChanged = true;
}

void CTaskNMPose::SetDamping(float fDamping, int poseBodyMask)
{
	int maskIterator = 1;
	for(int iBodyPartIndex = 0; iBodyPartIndex < NM_POSE_NUM_BODY_PARTS; iBodyPartIndex++)
	{
		if(poseBodyMask & maskIterator)
		{
			m_fBodyPartDamping[iBodyPartIndex] = fDamping;
		}
		maskIterator = maskIterator << 1;
	}
	m_bStiffnessValuesChanged = true;
}

void CTaskNMPose::StartBehaviour(CPed* pPed)
{
	PostNMMessagesForStiffness(pPed);
}

void CTaskNMPose::PostNMMessagesForStiffness(CPed* pPed)
{
	// Loop over stiffness and damping arrays, posting a message
	// for each value along with its associated mask
	for(int iBodyPartIndex = 0; iBodyPartIndex < NM_POSE_NUM_BODY_PARTS; iBodyPartIndex++)
	{
		ART::MessageParams msg;
		const char* sNmValStr = GetNMMaskStrFromBodyPartIndex(iBodyPartIndex);
		msg.addString(NMSTR_PARAM(NM_SET_STIFFNESS_MASK),sNmValStr);
		msg.addFloat(NMSTR_PARAM(NM_SET_STIFFNESS_STIFFNESS),m_fBodyPartStiffness[iBodyPartIndex]);
		msg.addFloat(NMSTR_PARAM(NM_SET_STIFFNESS_DAMPING),m_fBodyPartDamping[iBodyPartIndex]);
		msg.addBool(NMSTR_PARAM(NM_START), true);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_SET_STIFFNESS_MSG),&msg);
	}
}

static const char* sBodyPartStrings[NM_POSE_NUM_BODY_PARTS] =
{
	"uc",
	"ul",
	"ur",
	"us",
	"un",
	"uw",
	"ll",
	"lr"
};

const char* CTaskNMPose::GetNMMaskStrFromBodyPartIndex(int iBodyPartIndex)
{
	Assertf(iBodyPartIndex < NM_POSE_NUM_BODY_PARTS, "Invalid body part index in CTaskNMPose::GetNMMaskStrFromBodyPartIndex");
	if(iBodyPartIndex < NM_POSE_NUM_BODY_PARTS)
	{
		return sBodyPartStrings[iBodyPartIndex];
	}
	return "";
}

void CTaskNMPose::ControlBehaviour(CPed* pPed)
{
	CTaskNMBehaviour::ControlBehaviour(pPed);

	if(m_clipId != CLIP_ID_INVALID)
	{
		const crClip* pClip = fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, m_clipId);
		if(pClip)
		{
			if(m_bPlayClip)
			{
				bool bIsLooped = true;

				const fwClipItem *clipItem = fwClipSetManager::GetClipItem(m_clipSetId, m_clipId);
				Assert(clipItem);
				if(clipItem)
				{
					bIsLooped = (GetClipItemFlags(clipItem) & APF_ISLOOPED) != 0;
				}

				m_fPhase += fwTimer::GetTimeStep() / pClip->GetDuration();
				if(m_fPhase > 1.0f)
				{
					if(bIsLooped)
					{
						m_fPhase -= 1.0f;
					}
					else
					{
						m_fPhase = 1.0f;
					}

					m_bHasPlayedFullPhase = true;
				}
			}
			pPed->GetRagdollInst()->SetActivePoseFromClip(pClip, m_fPhase);
		}
	}
	else
	{
		// Just use clips from move blender
		pPed->GetAnimDirector()->SetUseCurrentSkeletonForActivePose(true);
	}

	// post message to tell NM to grab TM's as pose to drive to
	ART::MessageParams msg;
	msg.addBool(NMSTR_PARAM(NM_START), true);
	pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ACTIVE_POSE_MSG), &msg);



	if(m_bStiffnessValuesChanged)
	{
		PostNMMessagesForStiffness(pPed);
		m_bStiffnessValuesChanged = false;
	}
}

bool CTaskNMPose::FinishConditions(CPed* pPed)
{
	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, 0);
}
