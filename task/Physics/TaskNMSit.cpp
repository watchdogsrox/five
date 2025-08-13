// Filename   :	TaskNMSit.cpp
// Description:	Natural Motion sit class (FSM version)

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
#include "Task/Physics/TaskNMSit.h"
#include "vehicles/vehicle.h"
#include "Vfx\Misc\Fire.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CClonedNMSitInfo
//////////////////////////////////////////////////////////////////////////

CClonedNMSitInfo::CClonedNMSitInfo(const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, const Vector3& vecLookPos, CEntity* pLookEntity, const Vector3& vecSitPos)
: m_clipSetId(clipSetId)
, m_clipId(clipId)
, m_vecLookPos(vecLookPos)
, m_vecSitPos(vecSitPos)
, m_bHasLookAtEntity(false)
{
	if (pLookEntity && pLookEntity->GetIsDynamic() && static_cast<CDynamicEntity*>(pLookEntity)->GetNetworkObject())
	{
		m_lookAtEntity = static_cast<CDynamicEntity*>(pLookEntity)->GetNetworkObject()->GetObjectID();
		m_bHasLookAtEntity = true;
	}
}

CClonedNMSitInfo::CClonedNMSitInfo()
: m_clipSetId(CLIP_SET_ID_INVALID)
, m_clipId(CLIP_ID_INVALID)
, m_vecLookPos(Vector3(0.0f, 0.0f, 0.0f))
, m_vecSitPos(Vector3(0.0f, 0.0f, 0.0f))
, m_bHasLookAtEntity(false)
{

}

CTaskFSMClone *CClonedNMSitInfo::CreateCloneFSMTask()
{
	CEntity *pLookAtEntity = NULL;

	if (m_bHasLookAtEntity)
	{
		netObject* pObj = NetworkInterface::GetNetworkObject(m_lookAtEntity);

		if (pObj)
		{
			pLookAtEntity = pObj->GetEntity();
		}
	}

	return rage_new CTaskNMSit(10000, 10000, m_clipSetId, m_clipId, m_vecLookPos, pLookAtEntity, m_vecSitPos);
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMSit
//////////////////////////////////////////////////////////////////////////

CTaskNMSit::CTaskNMSit(u32 nMinTime, u32 nMaxTime, const fwMvClipSetId &clipSetId, const fwMvClipId &clipId, const Vector3& vecLookPos, CEntity* pLookEntity, const Vector3& vecSitPos)
:	CTaskNMBehaviour(nMinTime, nMaxTime),
m_clipSetId(clipSetId),
m_clipId(clipId),
m_pLookAtEntity(pLookEntity),
m_vecLooktAtPos(vecLookPos),
m_vecSitPos(vecSitPos),
m_bActivePoseRunning(false),
m_ClipSetRequestHelper()
{
	if(m_clipSetId!=CLIP_SET_ID_INVALID)
		m_ClipSetRequestHelper.Request(m_clipSetId);
	SetInternalTaskType(CTaskTypes::TASK_NM_SIT);
}

CTaskNMSit::~CTaskNMSit()
{
}

dev_float sfForceSitRelax = 0.0f;
dev_bool sbDoSitCatchFall = true;
dev_bool sbDoSitHeadLook = true;
//
void CTaskNMSit::StartBehaviour(CPed* pPed)
{
	const crClip* pClip = NULL;
	if(m_clipSetId!=CLIP_SET_ID_INVALID&& m_clipId!=CLIP_ID_INVALID && m_ClipSetRequestHelper.Request(m_clipSetId))
		pClip = fwAnimManager::GetClipIfExistsBySetId(m_clipSetId, m_clipId);

	if(sfForceSitRelax > 0.0f)
	{
		ART::MessageParams msgRelax;
		msgRelax.addBool(NMSTR_PARAM(NM_START), true);
		msgRelax.addFloat(NMSTR_PARAM(NM_RELAX_RELAXATION), sfForceSitRelax);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_RELAX_MSG), &msgRelax);
	}
	else if(pClip)
	{
		pPed->GetRagdollInst()->SetActivePoseFromClip(pClip, 0.0f);

		// post message to tell NM to grab TM's as pose to drive to
		ART::MessageParams msgPose;
		msgPose.addBool(NMSTR_PARAM(NM_START), true);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_ACTIVE_POSE_MSG), &msgPose);

		m_bActivePoseRunning = true;
	}

	if(sbDoSitCatchFall)
	{
		ART::MessageParams msgCatchFall;
		msgCatchFall.addBool(NMSTR_PARAM(NM_START), true);
		msgCatchFall.addBool(NMSTR_PARAM(NM_CATCHFALL_USE_HEADLOOK), false);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CATCHFALL_MSG), &msgCatchFall);
	}

	UpdateLookAt(pPed, true);
}

void CTaskNMSit::ControlBehaviour(CPed* pPed)
{
	UpdateLookAt(pPed);
}

dev_float sfSitFailureDistSqr = 4.0f;
//
bool CTaskNMSit::FinishConditions(CPed* pPed)
{
	// check if ped has moved too far from their seat
	if((VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()) - m_vecSitPos).Mag2() > sfSitFailureDistSqr)
	{
		m_nSuggestedNextTask = CTaskTypes::TASK_NM_RELAX;
		return true;
	}

	return CTaskNMBehaviour::ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_VEL_CHECK|FLAG_RELAX_AP_LOW_HEALTH);
}

void CTaskNMSit::UpdateLookAt(CPed* pPed, bool bStart)
{
	if(!sbDoSitHeadLook)
		return;

	Vector3 vecLookAtPos = m_vecLooktAtPos;
	if(m_pLookAtEntity)
	{
		CPed* pPed = NULL;
		if(m_pLookAtEntity->GetIsTypePed())
			pPed = (CPed*)m_pLookAtEntity.Get();

		if(m_pLookAtEntity->GetIsTypeVehicle() && ((CVehicle*)m_pLookAtEntity.Get())->GetDriver())
			pPed = ((CVehicle*)m_pLookAtEntity.Get())->GetDriver();

		if(pPed)
			((CPed*)m_pLookAtEntity.Get())->GetBonePosition(vecLookAtPos, BONETAG_HEAD);
		else 
		{
			vecLookAtPos = VEC3V_TO_VECTOR3(m_pLookAtEntity->GetTransform().GetPosition());
			if(pPed)
				vecLookAtPos.Add(0.7f*ZAXIS);
		}
	}

	if(vecLookAtPos.IsNonZero())
	{
		ART::MessageParams msgLook;
		if(bStart)
			msgLook.addBool(NMSTR_PARAM(NM_START), true);

		msgLook.addVector3(NMSTR_PARAM(NM_HEADLOOK_POS), vecLookAtPos.x, vecLookAtPos.y, vecLookAtPos.z);
		pPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_HEADLOOK_MSG), &msgLook);
	}
}
