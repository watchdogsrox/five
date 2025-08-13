// Filename   :	TaskNMThroughWindscreen.cpp
// Description:	Natural Motion through windscreen class (FSM version)
 
// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "fragmentnm/messageparams.h"

// Framework headers

// Game headers
#include "audio/scriptaudioentity.h"
#include "Peds/Ped.h"
#include "Scene/World/GameWorld.h"
#include "Task/Physics/TaskNMThroughWindscreen.h"

AI_OPTIMISATIONS()

// Tunable parameters. ///////////////////////////////////////////////////
CTaskNMThroughWindscreen::Tunables CTaskNMThroughWindscreen::sm_Tunables;
IMPLEMENT_PHYSICS_TASK_TUNABLES(CTaskNMThroughWindscreen, 0xaee70dff);
//////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CClonedNMThroughWindscreenInfo
//////////////////////////////////////////////////////////////////////////
CTaskFSMClone *CClonedNMThroughWindscreenInfo::CreateCloneFSMTask()
{
    CTaskNMThroughWindscreen *pNMTask = rage_new CTaskNMThroughWindscreen(10000, 10000, m_bExtraGravity, NULL);

    if(pNMTask)
    {
        pNMTask->SetInitialVelocity(m_InitVelocity);
    }

	return pNMTask;
}

//////////////////////////////////////////////////////////////////////////
// CTaskNMThroughWindscreen
//////////////////////////////////////////////////////////////////////////
CTaskNMThroughWindscreen::CTaskNMThroughWindscreen(u32 nMinTime, u32 nMaxTime, bool extraGravity, CVehicle* pVehicle)
:	CTaskNMBehaviour(nMinTime, nMaxTime)
, m_pVehicle(pVehicle)
, m_bAllowDisablingContacts(true)
, m_vecInitialVelocity(VEC3_ZERO)
{
	m_ExtraGravity = extraGravity;

	SetInternalTaskType(CTaskTypes::TASK_NM_THROUGH_WINDSCREEN);
}

CTaskNMThroughWindscreen::~CTaskNMThroughWindscreen()
{
}

bool CTaskNMThroughWindscreen::ProcessPhysics(float UNUSED_PARAM(fTimeStep), int UNUSED_PARAM(nTimeSlice))
{
	return true;
}

void CTaskNMThroughWindscreen::StartBehaviour(CPed* pPed)
{
	// we have to use the root rather than the entity position here, beccause the entity matrix can be offset significantly from the 
	// ped while the ragdoll is simulating, depending on the animation at the point when it first activated.
	pPed->GetBonePosition(m_vecInitialPosition, BONETAG_ROOT);

	AddTuningSet(&sm_Tunables.m_Start);

    if(!pPed->IsNetworkClone())
    {
        m_vecInitialVelocity = pPed->GetVelocity();
    }
    else if(!m_pVehicle)
    {
        m_pVehicle = pPed->GetMyVehicle();

        // apply a force to get the ped to the initial velocity synced over the network
        UpdateClonePedInitialVelocity(pPed);
    }

	// Don't allow contacts to be disabled for jetskis or bikes
	if (m_pVehicle && (m_pVehicle->GetIsJetSki() || m_pVehicle->InheritsFromBike() || m_pVehicle->InheritsFromQuadBike() || m_pVehicle->InheritsFromAmphibiousQuadBike()))
	{
		m_bAllowDisablingContacts = false;

        // disable network blending for clones for a short while. This prevents the clone being
        // pulled back into the vehicle as they are exiting through the windscreen
        if(pPed->IsNetworkClone())
        {
            TUNE_INT(TIME_TO_DELAY_BLENDING, 100, 0, 500, 1);
            NetworkInterface::OverrideNetworkBlenderForTime(pPed, TIME_TO_DELAY_BLENDING);
        }
	}

	g_ScriptAudioEntity.RegisterThrownFromVehicle(pPed);
}

void CTaskNMThroughWindscreen::ControlBehaviour(CPed* pPed)
{
	AddTuningSet(&sm_Tunables.m_Update);

	// Ensure that the ped doesn't bounce into the air (like when bailing from very fast vehicles)
	phArticulatedCollider* pCollider = dynamic_cast<phArticulatedCollider*>(CPhysics::GetSimulator()->GetCollider(pPed->GetRagdollInst()));
	static float fMinSpeedForExtraGravity = 3.0f;
	if(m_ExtraGravity && pCollider && pPed->GetVelocity().Mag2() > fMinSpeedForExtraGravity*fMinSpeedForExtraGravity)
	{
		// Find the distance above the initial jump "plane"
		Vector3 toPed;
		pPed->GetBonePosition(toPed, BONETAG_ROOT);
		toPed -= m_vecInitialPosition;
		Vector3 vUp = Vector3(0.0f,0.0f,1.0f);
		float distAbovePlane = toPed.Dot(vUp);
		if(distAbovePlane > sm_Tunables.m_StartForceDownHeight)
		{
			pCollider->GetBody()->ApplyGravityToLinks(vUp * (-sm_Tunables.m_GravityScale) * distAbovePlane, ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), pCollider->GetGravityFactor());
		}
	}

	CNmMessageList list;    
	ApplyTuningSetsToList(list);
	list.Post(pPed->GetRagdollInst());
}

//////////////////////////////////////////////////////////////////////////
bool CTaskNMThroughWindscreen::HandleRagdollImpact(float fMag, const CEntity* pEntity, const Vector3& vPedNormal, int nComponent, phMaterialMgr::Id nMaterialId)
//////////////////////////////////////////////////////////////////////////
{
	// Handle impacts with the vehicle we're ejecting from
	if (pEntity != NULL && pEntity == m_pVehicle)
	{
		return true;
	}

	return CTaskNMBehaviour::HandleRagdollImpact(fMag, pEntity, vPedNormal, nComponent, nMaterialId);
}


bool CTaskNMThroughWindscreen::FinishConditions(CPed* pPed)
{
	return ProcessFinishConditionsBase(pPed, MONITOR_FALL, FLAG_VEL_CHECK | FLAG_SKIP_DEAD_CHECK | FLAG_RELAX_AP_LOW_HEALTH);
}

void CTaskNMThroughWindscreen::UpdateClonePedInitialVelocity(CPed* pPed)
{
    if(pPed)
    {
        phArticulatedCollider *collider = dynamic_cast<phArticulatedCollider*>(CPhysics::GetSimulator()->GetCollider(pPed->GetRagdollInst()));

        Vector3 vecCurrentVel   = collider ? RCC_VECTOR3(collider->GetVelocity()) : VEC3_ZERO;
        Vector3 vecAcceleration = (m_vecInitialVelocity - vecCurrentVel) / fwTimer::GetTimeStep();

        if(vecAcceleration.Mag2() > rage::square(75.0f))
        {
            vecAcceleration.Normalize();
            vecAcceleration.Scale(75.0f);
        }

        if(collider)
        {
            taskAssertf(vecAcceleration.x == vecAcceleration.x, "Acceleration is not finite!");
            collider->GetBody()->ApplyGravityToLinks(vecAcceleration, ScalarV(fwTimer::GetTimeStep() / CPhysics::GetNumTimeSlices()).GetIntrin128ConstRef(), collider->GetGravityFactor());
        }
    }
}
