// FILE :    TaskSearchBase.h
// PURPOSE : Base class for the search subtasks

// File header
#include "Task/Combat/Subtasks/TaskSearchBase.h"

// Rage headers
#include "grcore/debugdraw.h"

// Game headers
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedWeapons/PedTargeting.h"
#include "Vehicles/vehicle.h"

AI_OPTIMISATIONS()

//=========================================================================
// CTaskSearchBase
//=========================================================================

CTaskSearchBase::Tunables CTaskSearchBase::sm_Tunables;
IMPLEMENT_COMBAT_TASK_TUNABLES(CTaskSearchBase, 0x66e0588d);

CTaskSearchBase::CTaskSearchBase(const Params& rParams)
: m_Params(rParams)
{
	SetInternalTaskType(CTaskTypes::TASK_SEARCH);
}

CTaskSearchBase::~CTaskSearchBase()
{

}

#if !__FINAL
void CTaskSearchBase::Debug() const
{	
#if DEBUG_DRAW
	//Grab the position.
	Vec3V vPosition = m_Params.m_vPosition;
	
	//Draw the position.
	grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(vPosition), 0.25f, Color_red);
	
	//Calculate the end.
	float fDirection = m_Params.m_fDirection;
	float fX = ANGLE_TO_VECTOR_X(RtoD * fDirection);
	float fY = ANGLE_TO_VECTOR_Y(RtoD * fDirection);
	Vec3V vOffset(fX, fY, 0.0f);
	Vec3V vEnd = Add(vPosition, vOffset);
	
	//Draw the direction.
	grcDebugDraw::Arrow(vPosition, vEnd, 0.25f, Color_green);
#endif

	//Call the base class version.
	CTask::Debug();
}
#endif

void CTaskSearchBase::LoadDefaultParamsForPedAndTarget(const CPed& rPed, const CPed& rTarget, Params& rParams)
{
	//Set the default params.
	rParams.m_pTarget = &rTarget;
	rParams.m_vPosition = rPed.GetTransform().GetPosition();
	rParams.m_fDirection = rPed.GetCurrentHeading();
	rParams.m_fTimeToGiveUp = sm_Tunables.m_TimeToGiveUp;
	
	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = rPed.GetPedIntelligence()->GetTargetting(true);
	if(!pTargeting)
	{
		return;
	}
	
	//Set the position to the target's last seen position.
	Vec3V& vPosition = rParams.m_vPosition;
	if(pTargeting->GetTargetLastSeenPos(&rTarget, RC_VECTOR3(vPosition)))
	{
		//Apply the position variance.
		float fMaxPositionVariance = sm_Tunables.m_MaxPositionVariance;
		float fX = fwRandom::GetRandomNumberInRange(-fMaxPositionVariance, fMaxPositionVariance);
		float fY = fwRandom::GetRandomNumberInRange(-fMaxPositionVariance, fMaxPositionVariance);
		vPosition = Add(vPosition, Vec3V(fX, fY, 0.0f));
	}

	//Set the direction to the target's last seen direction.
	float& fDirection = rParams.m_fDirection;
	if(pTargeting->GetTargetDirectionLastSeenMovingIn(&rTarget, fDirection))
	{
		//Apply the direction variance.
		float fMaxDirectionVariance = sm_Tunables.m_MaxDirectionVariance;
		float fDirectionVariance = fwRandom::GetRandomNumberInRange(-fMaxDirectionVariance, fMaxDirectionVariance);
		fDirection = fwAngle::LimitRadianAngleSafe(fDirection + fDirectionVariance);
	}
}

CTask::FSM_Return CTaskSearchBase::ProcessPreFSM()
{
	//Ensure the target is valid.
	const CPed* pTarget = m_Params.m_pTarget;
	if(!pTarget)
	{
		return FSM_Quit;
	}
	
	//Ensure the targeting is valid.
	CPedTargetting* pTargeting = GetPed()->GetPedIntelligence()->GetTargetting(true);
	if(!pTargeting)
	{
		return FSM_Quit;
	}
	
	//Ensure the target's whereabouts are not known.
	if(pTargeting->AreTargetsWhereaboutsKnown(NULL, pTarget))
	{
		return FSM_Quit;
	}
	
	//Ensure we should not give up.
	if(ShouldGiveUp())
	{
		return FSM_Quit;
	}

	return FSM_Continue;
}

bool CTaskSearchBase::ShouldGiveUp() const
{
	//Ensure the time to give up is valid.
	float fTimeToGiveUp = m_Params.m_fTimeToGiveUp;
	if(fTimeToGiveUp <= 0.0f)
	{
		return false;
	}

	//Ensure we have exceeded the time to give up.
	float fTime = GetTimeRunning();
	if(fTime < fTimeToGiveUp)
	{
		return false;
	}

	return true;
}

//=========================================================================
// CTaskSearchInVehicleBase
//=========================================================================

CTaskSearchInVehicleBase::CTaskSearchInVehicleBase(const Params& rParams)
: CTaskSearchBase(rParams)
{

}

CTaskSearchInVehicleBase::~CTaskSearchInVehicleBase()
{

}

CVehicle* CTaskSearchInVehicleBase::GetVehicle() const
{
	return GetPed()->GetVehiclePedInside();
}

CTask::FSM_Return CTaskSearchInVehicleBase::ProcessPreFSM()
{
	//Call the base class version.
	CTask::FSM_Return nValue = CTaskSearchBase::ProcessPreFSM();
	if(nValue != FSM_Continue)
	{
		return nValue;
	}
	
	//Ensure the vehicle is valid.
	if(!GetVehicle())
	{
		return FSM_Quit;
	}
	
	//Grab the ped.
	const CPed* pPed = GetPed();
	
	//Ensure the ped is driving the vehicle.
	if(!pPed->GetIsDrivingVehicle())
	{
		return FSM_Quit;
	}
	
	return FSM_Continue;
}
