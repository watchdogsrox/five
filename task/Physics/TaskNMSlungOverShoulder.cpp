// Filename   :	TaskNMSlungOverShoulder.cpp
// Description:	Natural Motion SlungOverShoulder class (FSM version)


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
#include "Physics/constrainthinge.h"
#include "physics/simulator.h"
#include "phbound/boundcomposite.h"

// Framework headers
#include "grcore/debugdraw.h"

// Game headers
#include "Peds\Ped.h"
#include "Task/Physics/NmDebug.h"
#include "Task/Physics/TaskNMSlungOverShoulder.h"
#include "task/motion/TaskMotionBase.h"
#include "Physics/physics.h"
#include "physics/gtaInst.h"
#include "physics/constraintfixed.h"

AI_OPTIMISATIONS()

CTaskNMSlungOverShoulder::CTaskNMSlungOverShoulder(CPed *slungPed, CPed *carrierPed)
:	CTaskNMBehaviour(30000, 30000),
m_slungPed(slungPed),
m_carrierPed(carrierPed),
m_bCarryClipsLoaded(false)
{
	m_Handle.Reset();
	SetInternalTaskType(CTaskTypes::TASK_NM_SLUNG_OVER_SHOULDER);
}

CTaskNMSlungOverShoulder::~CTaskNMSlungOverShoulder()
{
	PHLEVEL->AddInstanceIncludeFlags(m_slungPed->GetRagdollInst()->GetLevelIndex(), ArchetypeFlags::GTA_PED_TYPE);

	if (m_Handle.IsValid())
	{
		CPhysics::GetSimulator()->GetConstraintMgr()->Remove(m_Handle);
		m_Handle.Reset();
	}

	// reset the motion state
	if (m_carrierPed && m_carrierPed->GetPrimaryMotionTask())
	{
		m_carrierPed->GetPrimaryMotionTask()->ResetOnFootClipSet();
	}
}

CTaskInfo* CTaskNMSlungOverShoulder::CreateQueriableState() const 
{ 
	 // this task is not networked yet
	Assert(!NetworkInterface::IsGameInProgress()); 
	return rage_new CTaskInfo(); 
}

void CTaskNMSlungOverShoulder::StartBehaviour(CPed* /*pPed*/)
{
	// Have slung Ped only collider with carrier Ped's ragdoll bounds
	//phArchetypeDamp * slungArch = m_slungPed->GetRagdollInst()->GetCacheEntry()->GetPhysArchetype();
	ASSERT_ONLY(phArchetypeDamp * carrierArchRagdoll = m_carrierPed->GetRagdollInst()->GetCacheEntry()->GetPhysArchetype());
	ASSERT_ONLY(phArchetype * carrierArchClip = m_carrierPed->GetAnimatedInst()->GetArchetype());

	Assert(carrierArchRagdoll->GetTypeFlag(ArchetypeFlags::GTA_RAGDOLL_TYPE));
	Assert(carrierArchRagdoll->GetIncludeFlag(ArchetypeFlags::GTA_RAGDOLL_TYPE));

	Assert(carrierArchClip->GetTypeFlag(ArchetypeFlags::GTA_PED_TYPE));

	//Assert(slungArch->GetTypeFlag(ArchetypeFlags::GTA_RAGDOLL_TYPE));
	//Assert(slungArch->GetIncludeFlag(ArchetypeFlags::GTA_PED_TYPE));
	//Assert(slungArch->GetIncludeFlag(ArchetypeFlags::GTA_OBJECT_TYPE));
	//Assert(slungArch->GetIncludeFlag(ArchetypeFlags::GTA_RAGDOLL_TYPE));

	PHLEVEL->ClearInstanceIncludeFlags(m_slungPed->GetRagdollInst()->GetLevelIndex(), ArchetypeFlags::GTA_PED_TYPE);

	ART::MessageParams msg;
	msg.addBool(NMSTR_PARAM(NM_START), true);		
	m_slungPed->GetRagdollInst()->PostARTMessage(NMSTR_MSG(NM_CARRIED_MSG), &msg);

	// Stream in the carrying clips
	fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;
	const char *szClipGroupName = "MOVE_CARRY_PED";
	if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipGroupName)))
		clipSet.SetFromString(szClipGroupName);
	if (clipSet.GetHash() && fwClipSetManager::Request_DEPRECATED(clipSet))
	{
		m_carrierPed->GetPrimaryMotionTask()->SetOnFootClipSet(clipSet);
		m_bCarryClipsLoaded = true;
	}
	else
		m_bCarryClipsLoaded = false;

	// Pin ped to the carrier
	Assert(!m_Handle.IsValid());
	phConstraintFixed::Params constraint;
	constraint.instanceA = m_slungPed->GetRagdollInst();
	constraint.instanceB = m_carrierPed->GetRagdollInst();
	constraint.componentA = RAGDOLL_BUTTOCKS;
	constraint.componentB = RAGDOLL_CLAVICLE_LEFT;
	m_Handle = PHCONSTRAINT->Insert(constraint);

	// Don't let the constraint activate the carrier
	if (m_Handle.IsValid())
	{
		phConstraintFixed* constraintFixed = static_cast<phConstraintFixed*>( PHCONSTRAINT->GetTemporaryPointer(m_Handle) );
		if (constraintFixed)
			constraintFixed->SetAllowForceActivation(false);
	}
}
void CTaskNMSlungOverShoulder::ControlBehaviour(CPed* pPed)
{
	if (!m_bCarryClipsLoaded)
	{
		fwMvClipSetId clipSet = CLIP_SET_ID_INVALID;
		const char *szClipGroupName = "MOVE_CARRY_PED";
		if (fwClipSetManager::GetClipSet(fwMvClipSetId(szClipGroupName)))
			clipSet.SetFromString(szClipGroupName);
		if (clipSet.GetHash() && fwClipSetManager::Request_DEPRECATED(clipSet))
		{
			m_carrierPed->GetPrimaryMotionTask()->SetOnFootClipSet(clipSet);
			m_bCarryClipsLoaded = true;
		}
	}

	// make sure the ragdoll bounds for the animated ped are up to date before the physics update (so the constraint processes more accurately)
	m_carrierPed->GetAnimDirector()->WaitForPrePhysicsUpdateToComplete();
	m_carrierPed->UpdateRagdollBoundsFromAnimatedSkel();

	CTaskNMBehaviour::ControlBehaviour(pPed);
}

CTask::FSM_Return CTaskNMSlungOverShoulder::ProcessPreFSM()
{
	CPed *pPed = GetPed();

	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPhysicsTasks, true );
	pPed->SetPedResetFlag( CPED_RESET_FLAG_ProcessPostMovement, true );

	return CTaskNMBehaviour::ProcessPreFSM();
}

bool CTaskNMSlungOverShoulder::ProcessPhysics( float /*fTimeStep*/, int UNUSED_PARAM(nTimeSlice) )
{
	return true;
}

bool CTaskNMSlungOverShoulder::ProcessPostMovement()
{

	// HACK HACK HACK
	// Because the slung peds ragdoll inst is attached to the 
	// carrier ped's ragdoll inst by a physical constraint,
	// we can't currently give the physics update an up to date
	// position for the carrier peds radgoll inst (the ragdoll inst is not active,
	// and the physics update itself will do the work to move the animated inst.)

	// For now, we'll live with the position being an update out and fake update the
	// slung peds entity position based on the final physical velocity of the carrier ped

	if (m_slungPed && m_carrierPed)
	{		
		Vec3V oldPosition = m_slungPed->GetMatrix().d();

		Vector3 newPosition = m_carrierPed->GetVelocity();
		newPosition *= fwTimer::GetTimeStep();
		newPosition+=RC_VECTOR3(oldPosition);

		m_slungPed->SetPosition(newPosition, true, false, false);
		return true;
	}

	return false;
}

bool CTaskNMSlungOverShoulder::FinishConditions(CPed* pPed)
{
	if (pPed->IsDead() || pPed->GetIsSwimming())
		return true;

	if (m_carrierPed && m_carrierPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE)
	{
		if (m_Handle.IsValid())
		{
			CPhysics::GetSimulator()->GetConstraintMgr()->Remove(m_Handle);
			m_Handle.Reset();
		}

		// reset the motion state
		if (m_carrierPed->GetPrimaryMotionTask())
		{
			m_carrierPed->GetPrimaryMotionTask()->ResetOnFootClipSet();
		}

		return true;
	}

	return false;
}
