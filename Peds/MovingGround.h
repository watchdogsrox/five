// Title	:	MovingGround.h
// Author	:	Evgeny Adamenkov
// Started	:	12/5/12
// Purpose	:	Make navigation and covers work on moving ships and trains

#ifndef _MOVING_GROUND_H_
#define _MOVING_GROUND_H_

#include "ped.h"


namespace NMovingGround
{
	inline Vec3V_Out GetObjectVelocity(const CObject* pObject)
	{
		Vec3V vel = VECTOR3_TO_VEC3V(pObject->GetVelocity());
		const CPhysical* pGround = pObject->GetGroundPhysical();
		if (pGround)
		{
			vel -= VECTOR3_TO_VEC3V(pGround->GetVelocity());;
		}
		return vel;
	}

	inline Vec3V_Out GetPedAngVelocity(const CPed* pPed)
	{
		return VECTOR3_TO_VEC3V(pPed->GetAngVelocity()) - pPed->GetGroundAngularVelocity();
	}

	inline Vec3V_Out GetPedDesiredAngularVelocity(const CPed* pPed)
	{
		return VECTOR3_TO_VEC3V(pPed->GetDesiredAngularVelocity()) - pPed->GetGroundAngularVelocity();
	}

	inline Vec3V_Out GetPedDesiredVelocity(const CPed* pPed)
	{
		return VECTOR3_TO_VEC3V(pPed->GetDesiredVelocity()) - pPed->GetGroundVelocityIntegrated();
	}

	inline Vec3V_Out GetPedVelocity(const CPed* pPed)
	{
		return VECTOR3_TO_VEC3V(pPed->GetVelocity()) - pPed->GetGroundVelocityIntegrated();
	}

	inline void SetPedAngVelocity(CPed* pPed, Vec3V_In vVel)
	{
		pPed->SetAngVelocity(VEC3V_TO_VECTOR3(vVel + pPed->GetGroundAngularVelocity()));
	}

	inline void SetPedDesiredAngularVelocity(CPed* pPed, Vec3V_In vVel)
	{
		pPed->SetDesiredAngularVelocity(VEC3V_TO_VECTOR3(vVel + pPed->GetGroundAngularVelocity()));
	}

	inline void SetPedDesiredVelocity(CPed* pPed, Vec3V_In vVel)
	{
		pPed->SetDesiredVelocity(VEC3V_TO_VECTOR3(vVel + pPed->GetGroundVelocityIntegrated()));
	}

	inline void SetPedDesiredVelocityClamped(CPed* pPed, Vec3V_In vVel, float fAccelLimit)
	{
		pPed->SetDesiredVelocityClamped(VEC3V_TO_VECTOR3(vVel + pPed->GetGroundVelocityIntegrated()), fAccelLimit);
	}

	inline void SetPedVelocity(CPed* pPed, Vec3V_In vVel)
	{
		pPed->SetVelocity(VEC3V_TO_VECTOR3(vVel + pPed->GetGroundVelocityIntegrated()));
	}
}	// namespace NMovingGround


#endif	//	#ifndef _MOVING_GROUND_H_
