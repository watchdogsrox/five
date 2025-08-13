#include "physics/PhysicsHelpers.h"

// Rage headers
#include "grcore/debugdraw.h"
#include "pheffects/wind.h"
#if __BANK
#include "vector/colors.h"
#endif

// Framework
#include "fwmaths/angle.h"

// Game headers
#include "physics/WorldProbe/worldprobe.h"
#include "scene/Physical.h"

PHYSICS_OPTIMISATIONS()

namespace
{
	bool PointSameDirection(float fDot, float fAngleVectorsPointSameDir)
	{
		return fDot >= fAngleVectorsPointSameDir;
	}

	bool PointOppositeDirection(float fDot, float fAngleVectorsPointSameDir)
	{
		return fDot <= -fAngleVectorsPointSameDir;
	}
}

const CCollisionRecord* PhysicsHelpers::GetCollisionRecord(const CPhysical* pPhysical, const CPhysical* pOtherPhysical)
{
	if (pPhysical && pOtherPhysical)
	{
		const CCollisionHistory* pColHistory = pPhysical->GetFrameCollisionHistory();
		return pColHistory->FindCollisionRecord(pOtherPhysical);
	}

	return nullptr;
}

bool PhysicsHelpers::GetCollisionRecordInfo(const CPhysical* pPhysical, const CPhysical* pOtherPhysical, float& out_fHitFwdDot, float& out_fHitRightDot)
{
	const CCollisionRecord* pCollisionRecord = GetCollisionRecord(pPhysical, pOtherPhysical);
	return GetCollisionRecordInfo(pPhysical, pCollisionRecord, out_fHitFwdDot, out_fHitRightDot);
}

bool PhysicsHelpers::GetCollisionRecordInfo(const CPhysical* pPhysical, const CCollisionRecord* pCollisionRecord, float& out_fHitFwdDot, float& out_fHitRightDot)
{
	if (pPhysical && pCollisionRecord)
	{
		const Vector3& vecNorm = pCollisionRecord->m_MyCollisionNormal;
		if (vecNorm.IsNonZero())
		{
			out_fHitFwdDot = DotProduct(vecNorm, VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetB()));
			out_fHitRightDot = DotProduct(vecNorm, VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetA()));
			return true;
		}
	}

	out_fHitFwdDot = 0.0f;
	out_fHitRightDot = 0.0f;

	return false;
}

bool PhysicsHelpers::FindVehicleCollisionFault(const CVehicle* victim, const CVehicle* damager, bool& out_isVictimFault, float fAngleVectorsPointSameDir, float fMinFaultVelocityThreshold, const CCollisionRecord** out_collisionRecord)
{
	const CCollisionRecord* pCollisionRecord = GetCollisionRecord(victim, damager);
	if (out_collisionRecord != nullptr)
	{
		out_collisionRecord = &pCollisionRecord;
	}

	return FindVehicleCollisionFault(victim, pCollisionRecord, out_isVictimFault, fAngleVectorsPointSameDir, fMinFaultVelocityThreshold);
}

bool PhysicsHelpers::FindVehicleCollisionFault(const CVehicle* pVictim, const CCollisionRecord* pCollisionRecord, bool& out_isVictimFault, float fAngleVectorsPointSameDir, float fMinFaultVelocityThreshold)
{
	out_isVictimFault = false;

	float victimHitFwdDot;
	float victimHitRightDot;

	gnetDebug1("............  Collision History:");
	if (GetCollisionRecordInfo(pVictim, pCollisionRecord, victimHitFwdDot, victimHitRightDot))
	{
		gnetDebug1("....................... Hit: victim=<Fwd=%f, Right=%f>", victimHitFwdDot, victimHitRightDot);
	}
	else
	{
		gnetDebug1("....................... Failed to retrieve collision record info");
		return false;
	}

	if (pVictim->GetVelocity().Mag2() >= square(fMinFaultVelocityThreshold))
	{
		Vector3 victimVelocityNorm = pVictim->GetVelocity();
		victimVelocityNorm.NormalizeFast();

		if (PointOppositeDirection(victimHitFwdDot, fAngleVectorsPointSameDir)) //Victim: Crash in Front Side
		{
			const float fwdDot = DotProduct(VEC3V_TO_VECTOR3(pVictim->GetTransform().GetB()), victimVelocityNorm);
			if (PointSameDirection(fwdDot, fAngleVectorsPointSameDir))
			{
				gnetWarning("....................... Hit->Front and going forward");
				out_isVictimFault = true;
			}
			else if (PointOppositeDirection(fwdDot, fAngleVectorsPointSameDir))
			{
				gnetDebug1("........................ Hit->Front and going backward");
				out_isVictimFault = false;
			}
		}
		else if (PointSameDirection(victimHitFwdDot, fAngleVectorsPointSameDir)) //Victim: Crash in Back Side
		{
			const float fwdDot = DotProduct(VEC3V_TO_VECTOR3(pVictim->GetTransform().GetB()), victimVelocityNorm);
			if (PointSameDirection(fwdDot, fAngleVectorsPointSameDir))
			{
				gnetDebug1("........................ Hit->Back and going Forward");
				out_isVictimFault = false;
			}
			else if (PointOppositeDirection(fwdDot, fAngleVectorsPointSameDir))
			{
				gnetWarning("....................... Hit->Back and going backward");
				out_isVictimFault = true;
			}
		}
		else if (PointOppositeDirection(victimHitRightDot, fAngleVectorsPointSameDir)) //Victim: Crash in Right Side
		{
			const float rightDot = DotProduct(VEC3V_TO_VECTOR3(pVictim->GetTransform().GetA()), victimVelocityNorm);
			if (PointSameDirection(rightDot, fAngleVectorsPointSameDir))
			{
				gnetWarning("....................... Hit->Right and going to the right");
				out_isVictimFault = true;
			}
			else if (PointOppositeDirection(rightDot, fAngleVectorsPointSameDir))
			{
				gnetDebug1("........................ Hit->Right and going to the left");
				out_isVictimFault = false;
			}
		}
		else if (PointSameDirection(victimHitRightDot, fAngleVectorsPointSameDir)) //Victim: Crash in Left Side
		{
			const float rightDot = DotProduct(VEC3V_TO_VECTOR3(pVictim->GetTransform().GetA()), victimVelocityNorm);
			if (PointSameDirection(rightDot, fAngleVectorsPointSameDir))
			{
				gnetDebug1("........................ Hit->Left and going to the right");
				out_isVictimFault = false;
			}
			else if (PointOppositeDirection(rightDot, fAngleVectorsPointSameDir))
			{
				gnetWarning("....................... Hit->Left and going to the left");
				out_isVictimFault = true;
			}
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// CFlightModelHelper
//////////////////////////////////////////////////////////////////////////

// Scale drag values up for editing so rag can deal with them
static const float fDragTweakCoeffV = 1000.0f;
static const float fDragTweakCoeffAV = 1.0f;

CFlightModelHelper::CFlightModelHelper()
{
	m_fThrustMult = 0.0f;
	m_fYawRate = 0.0f;
	m_fPitchRate = 0.0f;
	m_fRollRate = 0.0f;
	m_fYawStabilise = 0.0f;
	m_fRollStabilise = 0.0f;
	m_fPitchStabilise = 0.0f;		
	m_fFormLiftMult = 0.0f;
	m_fAttackLiftMult = 0.0f;
	m_fAttackDiveMult = 0.0f;
	for(int i =0; i < NumDragTypes; i++)
	{
		m_vDragCoeff[i].Zero();
	}
	BANK_ONLY(m_fLastEffectiveThrottle = 0.0f);
#if __BANK
	m_bRender = false;
	m_bRenderOrigin = false;
	m_bRenderTail = false;
#endif
}

CFlightModelHelper::CFlightModelHelper(float fThrustMult, 
									   float fYawRate, 
									   float fPitchRate, 
									   float fRollRate, 
									   float fYawStabilise, 
									   float fPitchStabilise, 
									   float fRollStabilise, 
									   float fFormLiftMult, 
									   float fAttackLiftMult,
									   float fAttackDiveMult,
									   float fSideSlipMult,
									   float fThrottleFallOff,
									   const Vector3 &vDragConstV,
									   const Vector3 &vDragLinearV,
									   const Vector3 &vDragSquaredV,
									   const Vector3 &vDragConstAV,
									   const Vector3 &vDragLinearAV,
									   const Vector3 &vDragSquaredAV)
{
	m_fThrustMult = fThrustMult;
	m_fYawRate = fYawRate;
	m_fPitchRate = fPitchRate;
	m_fRollRate = fRollRate;
	m_fYawStabilise = fYawStabilise;
	m_fRollStabilise = fRollStabilise;
	m_fPitchStabilise = fPitchStabilise;
	m_fFormLiftMult = fFormLiftMult;
	m_fAttackLiftMult = fAttackLiftMult;
	m_fAttackDiveMult = fAttackDiveMult;
	m_fSideSlipMult = fSideSlipMult;
	m_fThrottleFallOff = fThrottleFallOff;
	m_vDragCoeff[FT_Constant_V] = vDragConstV;
	m_vDragCoeff[FT_Linear_V] = vDragLinearV;
	m_vDragCoeff[FT_Squared_V] = vDragSquaredV;
	m_vDragCoeff[FT_Constant_AV] = vDragConstAV;
	m_vDragCoeff[FT_Linear_AV] = vDragLinearAV;
	m_vDragCoeff[FT_Squared_AV] = vDragSquaredAV;
	BANK_ONLY(m_fLastEffectiveThrottle = 0.0f);
#if __BANK
	m_bRender = false;
	m_bRenderOrigin = false;
	m_bRenderTail = false;
#endif
}

void CFlightModelHelper::ProcessFlightModel(
	CPhysical* pPhysical, 
	float fThrust, 
	float fPitch, 
	float fRoll, 
	float fYaw, 
	const Vector3& vAirSpeed, 
	bool bIgnoreWind, 
	float fOveralMult, 
	float fDragMult,
	bool bFlattenLocalSpeed) const
{
	Assert(pPhysical);

	const float fMass = pPhysical->GetMass();
	const Matrix34 objMat = MAT34V_TO_MATRIX34(pPhysical->GetMatrix());
	const Vector3 vBoundBoxMin = pPhysical->GetBoundingBoxMin();
	const Vector3 vecAngInertia = pPhysical->GetAngInertia();

	Vector3 vVelocity = pPhysical->GetVelocity() - vAirSpeed;

	Vector3 vWindSpeed;
	
	if(bIgnoreWind)
	{
		vWindSpeed = VEC3_ZERO; 
	}
	else
	{
		WIND.GetLocalVelocity(pPhysical->GetTransform().GetMatrix().GetCol3(),RC_VEC3V(vWindSpeed), false, false);
	}

	vVelocity -= vWindSpeed;

#if __BANK && DEBUG_DRAW
	if(m_bRender && m_bRenderOrigin)
	{
		grcDebugDraw::Sphere(objMat.d, 0.25f, Color_red);
	}
#endif

	// MISC STUFF
	Vector3 vecRudderArm = objMat.b;
	Vector3 vecTailOffset = vecRudderArm * vBoundBoxMin.y;
	float fForwardSpd = vVelocity.Dot(vecRudderArm);

	Vector3 vLocalSpeed;
	if(!bFlattenLocalSpeed)
	{
		vLocalSpeed = pPhysical->GetLocalSpeed(vecTailOffset);
	}
	else
	{
		vLocalSpeed = objMat.b;
	}


#if __BANK && DEBUG_DRAW
	if(m_bRender && m_bRenderTail)
	{
		grcDebugDraw::Line(objMat.d, objMat.d + vecTailOffset, Color_red, Color_orange);
	}
#endif

	// THRUST

	// apply thrust from propeller
	Vector3 vecThrust = objMat.b;
	// Make throttle fall off at high speed
	float fFallOff = m_fThrottleFallOff*fForwardSpd*fForwardSpd / 1000.0f;		// divide by 1000.0 so the number doesn't have to be tiny (widgets hate tiny numbers)
	fThrust = (1.0f - fFallOff);
	BANK_ONLY(m_fLastEffectiveThrottle = fThrust;)
		vecThrust *= m_fThrustMult;
	ApplyForceCgSafe(vecThrust*-GRAVITY*fThrust*fMass*fOveralMult,pPhysical);

	// SIDEFORCES
	// first do sideslip angle of attack
	float fSideSlipAngle = -1.0f*vVelocity.Dot(objMat.a);

	// doing sideways stuff
	Vector3 vecRudderForce = objMat.a;

	// sideways force from sidesliping
	vecRudderForce *= m_fSideSlipMult*fSideSlipAngle*rage::Abs(fSideSlipAngle);
	ApplyForceCgSafe(vecRudderForce*fMass*-GRAVITY*fOveralMult,pPhysical);

	// control force from steering and stabilising force from rudder
	vecRudderForce = objMat.a;
	fSideSlipAngle = -1.0f*vLocalSpeed.Dot(objMat.a);

	vecRudderForce *= (m_fYawRate*-fYaw*fForwardSpd + m_fYawStabilise*fSideSlipAngle*rage::Abs(fSideSlipAngle))*vecAngInertia.z*-GRAVITY;
	ApplyTorqueSafe(vecRudderForce, vecTailOffset*fOveralMult,pPhysical);

	// ROLL
	vecRudderForce = objMat.b;
	vecRudderForce *= m_fRollRate*fRoll*fForwardSpd*vecAngInertia.y*-GRAVITY;

	ApplyTorqueSafe(vecRudderForce*fOveralMult,pPhysical);

	// also need to add some stabilisation for roll (so plane naturally levels out)
	vecRudderForce.Cross(objMat.b, Vector3(0.0f,0.0f,1.0f));
	float fRollOffset = 1.0f;
	if(objMat.c.z > 0.0f)
	{
		if(objMat.a.z > 0.0f)
			fRollOffset = -1.0f;
	}
	else
	{
		vecRudderForce *= -1.0f;
		if(objMat.a.z > 0.0f)
			fRollOffset = -1.0f;
	}

	// Stabilisation recedes as we level out
	fRollOffset *= 1.0f - objMat.a.Dot(vecRudderForce);
	// roll stabilisation recedes as we go vertical
	fRollOffset *= 1.0f - rage::Abs(objMat.b.z);
	ApplyTorqueSafe(-m_fRollStabilise*fRollOffset*vecAngInertia.y*0.5f*-GRAVITY*fOveralMult*objMat.b,pPhysical);

	// PITCH
	// calculate tailplane angle of attack
	vecRudderForce = objMat.c;
	float fAngleOfAttack = -1.0f*vLocalSpeed.Dot(objMat.c);

	vecRudderForce *= (m_fPitchRate*fPitch*fForwardSpd + m_fPitchStabilise*fAngleOfAttack*rage::Abs(fAngleOfAttack))*vecAngInertia.x*-GRAVITY;
	ApplyTorqueSafe(vecRudderForce*fOveralMult, vecTailOffset,pPhysical);

	// LIFT
	// calculate main wing angle of attack
	fAngleOfAttack = vVelocity.Dot(objMat.c) / rage::Max(0.01f, vVelocity.Mag());
	fAngleOfAttack = -1.0f*rage::Asinf(rage::Max(-1.0f, rage::Min(1.0f, fAngleOfAttack)));

	// uplift from wing shape
	float fFormLift = m_fFormLiftMult*fForwardSpd*fForwardSpd*fMass*-GRAVITY;
	vecRudderForce = fFormLift*objMat.c;
	ApplyForceCgSafe(vecRudderForce*fOveralMult,pPhysical);

	float fWingForce = m_fAttackLiftMult*fAngleOfAttack*fForwardSpd*fForwardSpd*fMass*-GRAVITY;

	vecRudderForce = fWingForce*objMat.c;
	ApplyForceCgSafe(vecRudderForce*fOveralMult,pPhysical);

	// DRAG
	ApplyDamping(pPhysical,m_vDragCoeff, fOveralMult * fDragMult);
}

void CFlightModelHelper::ApplyForceCgSafe(const Vector3 &vForce_, CPhysical* pPhysical)
{
	Vector3 vForce = vForce_;
	if(vForce.Mag2() >= square(DEFAULT_IMPULSE_LIMIT * rage::Max(1.0f, pPhysical->GetMass())))
	{
		vForce.Scale(149.9f * rage::Max(1.0f, pPhysical->GetMass())/ vForce.Mag());
	}

	pPhysical->ApplyForceCg(vForce);
}

void CFlightModelHelper::ApplyTorqueSafe(const Vector3 &vTorque_, const Vector3 &vOffset_, CPhysical* pPhysical) 
{
	Vector3 vTorque = vTorque_;
	if(vTorque.Mag2() >= square(DEFAULT_IMPULSE_LIMIT * rage::Max(1.0f, pPhysical->GetMass())))
	{
		vTorque.Scale(149.9f * rage::Max(1.0f, pPhysical->GetMass()) / vTorque.Mag());
	}

	Vector3 vWorldPos = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition()) + vOffset_;
	Vector3 vWorldCentroid;
	float fCentroidRadius = 0.0f;
	if(pPhysical->GetCurrentPhysicsInst() && pPhysical->GetCurrentPhysicsInst()->GetArchetype())
	{
		vWorldCentroid = VEC3V_TO_VECTOR3(pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetWorldCentroid(pPhysical->GetCurrentPhysicsInst()->GetMatrix()));
		fCentroidRadius = pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
	}
	else
	{
		fCentroidRadius = pPhysical->GetBoundCentreAndRadius(vWorldCentroid);
	}

	Vector3 vOffset = vOffset_;
	if(vWorldPos.Dist2(vWorldCentroid) > square(fCentroidRadius))
	{
		vOffset *= (fCentroidRadius - 0.1f) / vWorldPos.Dist(vWorldCentroid);
	}

	pPhysical->ApplyTorque(vTorque,vOffset);
}

void CFlightModelHelper::ApplyTorqueSafe(const Vector3 &vTorque_, CPhysical* pPhysical) 
{
	Vector3 vTorque = vTorque_;
	Vector3 vecAngAccel = VEC3V_TO_VECTOR3(pPhysical->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vTorque)));
	const float fAngAccel = vecAngAccel.Dot(pPhysical->GetCurrentPhysicsInst()->GetArchetype()->GetInvAngInertia());
	if(fAngAccel > 10.0f)
	{
		vTorque.Scale( 9.9999f / fAngAccel);
	}

	pPhysical->ApplyTorque(vTorque);
}

void CFlightModelHelper::MakeCircularInputSquare( Vector2& vInAndOut )
{
	// stuff to convert from a nice circular joystick input, into a nice square representation
	float fJoyTheta = rage::Atan2f(vInAndOut.y, vInAndOut.x);

	float fMaxMag = 1.0f;
	if( fJoyTheta > -PI/4.0f && fJoyTheta <= PI/4.0f )
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta);
	else if( fJoyTheta > PI/4.0f && fJoyTheta <= 0.75f * PI)
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta - PI/2.0f);
	else if( fJoyTheta > 0.75f * PI )
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta - PI);
	else if( fJoyTheta  <= -0.75f*PI )
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta + PI);
	else if( fJoyTheta > -0.75f*PI && fJoyTheta < -PI/4.0f)
		fMaxMag = 1.0f / rage::Cosf(fJoyTheta + PI/2.0f);


	vInAndOut.x *= fMaxMag;
	vInAndOut.y *= fMaxMag;

	vInAndOut.x = rage::Clamp(vInAndOut.x,-1.0f,1.0f);
	vInAndOut.y = rage::Clamp(vInAndOut.y,-1.0f,1.0f);
}

void CFlightModelHelper::ApplyDamping(CPhysical* pPhysical,const Vector3 vDragCoeff[NumDragTypes], float fOveralMult)
{
	// DRAG
	// Velocity based
	Vector3 vVelocity = pPhysical->GetVelocity();
	if(vVelocity.IsNonZero())
	{
		Vector3 vDragForce;

		Vector3 vVelLocal = VEC3V_TO_VECTOR3(pPhysical->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vVelocity)));

		Vector3 vVelDirLocal = vVelLocal;
		vVelDirLocal.Normalize();

		Vector3 vTemp;
		vTemp.Multiply(-vVelDirLocal,vDragCoeff[FT_Constant_V]);
		vDragForce = vTemp;

		vTemp.Multiply(-vVelLocal,vDragCoeff[FT_Linear_V]);
		vDragForce += vTemp;

		Vector3 vVelSq;
		vVelSq.Multiply(vVelLocal,vVelLocal);

		vTemp.Multiply(-vVelSq,vDragCoeff[FT_Squared_V]);

		// Need to take into account sign of Vel, since square removes it
		vVelDirLocal.x = Sign(vVelDirLocal.x);
		vVelDirLocal.y = Sign(vVelDirLocal.y);
		vVelDirLocal.z = Sign(vVelDirLocal.z);

		vTemp.Multiply(vVelDirLocal);
		vDragForce += vTemp;

		vDragForce.Scale(pPhysical->GetMass() / fDragTweakCoeffV);

		// Need to transform back to world space
		vDragForce = VEC3V_TO_VECTOR3(pPhysical->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDragForce)));

		ApplyForceCgSafe(vDragForce*fOveralMult,pPhysical);
	}


	Vector3 vAngVel = pPhysical->GetAngVelocity();
	if(vAngVel.IsNonZero())
	{
		Vector3 vDragForce;

		Vector3 vAngVelLocal = VEC3V_TO_VECTOR3(pPhysical->GetTransform().UnTransform3x3(VECTOR3_TO_VEC3V(vAngVel)));

		// Angular velocity based
		Vector3 vVelDirLocal = vAngVelLocal;
		vVelDirLocal.Normalize();

		Vector3 vTemp;
		vTemp.Multiply(-vVelDirLocal,vDragCoeff[FT_Constant_AV]);
		vDragForce = vTemp;

		vTemp.Multiply(-vAngVelLocal,vDragCoeff[FT_Linear_AV]);
		vDragForce += vTemp;

		Vector3 vAngVelSq;
		vAngVelSq.Multiply(vAngVelLocal,vAngVelLocal);

		vTemp.Multiply(-vAngVelSq,vDragCoeff[FT_Squared_AV]);

		// Need to take into account sign of angVel, since square removes it
		vVelDirLocal.x = Sign(vVelDirLocal.x);
		vVelDirLocal.y = Sign(vVelDirLocal.y);
		vVelDirLocal.z = Sign(vVelDirLocal.z);

		vTemp.Multiply(vVelDirLocal);
		vDragForce += vTemp;

		vDragForce.Multiply(pPhysical->GetAngInertia() / fDragTweakCoeffAV);

		// Need to transform back to world space
		vDragForce = VEC3V_TO_VECTOR3(pPhysical->GetTransform().Transform3x3(VECTOR3_TO_VEC3V(vDragForce)));

		ApplyTorqueSafe(vDragForce*fOveralMult,pPhysical);
	}	
}

#if __BANK
void CFlightModelHelper::AddWidgetsToBank(bkBank* pBank)
{
	pBank->AddSlider("Thrust mult",&m_fThrustMult,0.0f,10000.0f,0.01f);
	pBank->AddSlider("Yaw rate",&m_fYawRate,0.0f,1000.0f,0.001f);
	pBank->AddSlider("Yaw stabilise",&m_fYawStabilise,0.0f,1000.0f,0.001f);
	pBank->AddSlider("Pitch rate",&m_fPitchRate,0.0f,1000.0f,0.001f);
	pBank->AddSlider("Pitch stabilise",&m_fPitchStabilise,0.0f,1000.0f,0.001f);
	pBank->AddSlider("Roll rate",&m_fRollRate,0.0f,1000.0f,0.001f);
	pBank->AddSlider("Roll stabilise",&m_fRollStabilise,0.0f,1000.0f,0.001f);
	pBank->AddSlider("Form lift",&m_fFormLiftMult,0.0f,100.0f,0.001f);
	pBank->AddSlider("Attack lift",&m_fAttackLiftMult,0.0f,100.0f,0.001f);
	pBank->AddSlider("Attack dive",&m_fAttackDiveMult,0.0f,100.0f,0.001f);
	pBank->AddSlider("Side slip mult",&m_fSideSlipMult,0.0f,100.0f,0.001f);
	pBank->AddSlider("Throttle fall off",&m_fThrottleFallOff,0.0f,100.0f,0.001f);	

	const char* sDragTypeNames[] = 
	{
		"ConstantV",
		"LinearV",
		"SquaredV",
		"ConstantAV",
		"LinearAV",
		"SquaredAV"
	};
	const float fMaxDrag = 10000.0f;
	const Vector3 vMax = Vector3(fMaxDrag, fMaxDrag, fMaxDrag);

	pBank->PushGroup("Drag",false);
	for(int i = 0; i < NumDragTypes; i++)
	{
		pBank->AddSlider(sDragTypeNames[i],&m_vDragCoeff[i],VEC3_ZERO,vMax,vMax/fMaxDrag);		
	}
	pBank->PopGroup();

	pBank->PushGroup("Render");

		pBank->AddToggle("Render", &m_bRender);
		pBank->AddToggle("Render Origin", &m_bRenderOrigin);
		pBank->AddToggle("Render Tail", &m_bRenderTail);

	pBank->PopGroup();
}

void CFlightModelHelper::DebugDrawControlInputs(const CPhysical* pPhysical, float fThrust, float fPitch, float fRoll, float fYaw) const
{
	static Vector2 vRollDebugPos(0.65f,0.25f);
	static Vector2 vPitchDebugPos(0.75f,0.15f);
	static Vector2 vYawDebugPos(0.65f,0.5f);
	static Vector2 vThrottleDebugPos(0.5f,0.15f);
	static Vector2 vMoveSpeedMeterPos(0.2f,0.15f);
	static float fVelocityScale = 50.0f;
	float fScale = 0.2f;
	float fEndWidth = 0.01f;

	grcDebugDraw::Meter(vPitchDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"Pitch");
	grcDebugDraw::MeterValue(vPitchDebugPos, Vector2(0.0f,1.0f), fScale, fPitch, fEndWidth, Color32(255,0,0));	

	grcDebugDraw::Meter(vRollDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Roll");
	grcDebugDraw::MeterValue(vRollDebugPos, Vector2(1.0f,0.0f), fScale, fRoll, fEndWidth, Color32(255,0,0));	

	grcDebugDraw::Meter(vYawDebugPos,Vector2(1.0f,0.0f),fScale,fEndWidth,Color32(255,255,255),"Yaw");
	grcDebugDraw::MeterValue(vYawDebugPos, Vector2(1.0f,0.0f), fScale, fYaw, fEndWidth, Color32(255,0,0));	

	grcDebugDraw::Meter(vThrottleDebugPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),"fThrust");
	grcDebugDraw::MeterValue(vThrottleDebugPos, Vector2(0.0f,1.0f), fScale, 2.0f*(0.5f-fThrust), fEndWidth, Color32(255,0,0));	
	grcDebugDraw::MeterValue(vThrottleDebugPos, Vector2(0.0f,1.0f), fScale, 2.0f*(0.5f-m_fLastEffectiveThrottle), fEndWidth, Color32(255,255,255));	

	float fVelocity = pPhysical->GetVelocity().Mag();
	const float fVelocityMetersPerSec = fVelocity;
	fVelocity /= fVelocityScale;
	fVelocity = rage::Clamp(fVelocity,0.0f,1.0f);

	float fVelKph = fVelocityMetersPerSec * 3.6f;

	char velString[32];
	formatf(velString,"Velocity: %.2f m/s | %.2f km/h",fVelocityMetersPerSec,fVelKph);
	grcDebugDraw::Meter(vMoveSpeedMeterPos,Vector2(0.0f,1.0f),fScale,fEndWidth,Color32(255,255,255),velString);
	grcDebugDraw::MeterValue(vMoveSpeedMeterPos, Vector2(0.0f,1.0f),fScale, 2.0f*(fVelocity-0.5f), fEndWidth, Color32(255,0,0));	


}

void CFlightModelHelper::DebugDrawAltitudeCircular(const CPhysical* pPhysical)
{
	// draw altitude
	static Vector2 vAltDrawPos(0.7f,0.75f);
	static dev_float fBigHandLength = 20.0f/240.0f;
	static dev_float fSmallHandLength = 10.0f/240.0f;
	static dev_float fBigHandScale = 100.0f;
	static dev_float fSmallHandScale = 1000.0f;
	static const Vector2 UnitVector = grcDebugDraw::Get2DAspect();

	static dev_bool bUseProbe = false;

	const Vector3 vPhysicalPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());
	float fGroundZ = 0.0f;
	if(bUseProbe)
	{
		float distToTopSurface = 0.0f;

		fGroundZ = WorldProbe::FindGroundZFor3DCoord(distToTopSurface, vPhysicalPosition);
	}

	const float fCurrentAltitude = vPhysicalPosition.z - fGroundZ;

	Vector2 vDrawPoint;

	// Draw some markers for reference
	static dev_s32 iNumMarkers = 10;
	static dev_float fMarkerDrawScale = 0.05f;
	for(int i = 0; i < iNumMarkers; i++)
	{
		float fAngle = (((float)i)/(float)iNumMarkers) * TWO_PI;
		vDrawPoint.Set(rage::Sinf(fAngle),-rage::Cosf(fAngle));
		vDrawPoint.Scale(fBigHandLength);
		vDrawPoint.Multiply(UnitVector);
		Vector2 vDrawPoint1 = vAltDrawPos;
		vDrawPoint1.AddScaled(vDrawPoint,1.0f - fMarkerDrawScale);
		grcDebugDraw::Line(vDrawPoint1,vAltDrawPos+vDrawPoint,Color_white);
		grcDebugDraw::Line(vDrawPoint1,vAltDrawPos+vDrawPoint,Color_white);

		vDrawPoint1 = vAltDrawPos;
		vDrawPoint1.AddScaled(vDrawPoint,1.0f + fMarkerDrawScale);
		grcDebugDraw::Line(vDrawPoint1,vAltDrawPos+vDrawPoint,Color_black);
		grcDebugDraw::Line(vDrawPoint1,vAltDrawPos+vDrawPoint,Color_black);
	}

	// Draw altitude on a dial
	// Big hand 
	float fAngle1 = (fCurrentAltitude * TWO_PI / fBigHandScale);
	// Small hand
	float fAngle2 = (fCurrentAltitude * TWO_PI / fSmallHandScale);

	vDrawPoint.Set(rage::Sinf(fAngle1),-rage::Cosf(fAngle1));
	vDrawPoint.Scale(fBigHandLength);
	vDrawPoint.Multiply(UnitVector);
	grcDebugDraw::Line(vAltDrawPos,vAltDrawPos+vDrawPoint,Color_red);

	vDrawPoint.Set(rage::Sinf(fAngle2),-rage::Cosf(fAngle2));
	vDrawPoint.Scale(fSmallHandLength);
	vDrawPoint.Multiply(UnitVector);
	grcDebugDraw::Line(vAltDrawPos,vAltDrawPos+vDrawPoint,Color_red2);


}

Vector2 vAltDrawPosBar(0.75f,0.85f);
void CFlightModelHelper::DebugDrawAltitudeBar(const CPhysical* pPhysical)
{
	// draw altitude
	static dev_float fAxisLength = 0.2f;
	static dev_float fEndPointSize = 0.01f;

	static dev_bool bUseProbe = false;

	const Vector3 vPhysicalPosition = VEC3V_TO_VECTOR3(pPhysical->GetTransform().GetPosition());
	float fGroundZ = 0.0f;
	if(bUseProbe)
	{
		float distToTopSurface = 0.0f;

		fGroundZ = WorldProbe::FindGroundZFor3DCoord(distToTopSurface, vPhysicalPosition);
	}

	float fCurrentAltitude = vPhysicalPosition.z - fGroundZ;

	static dev_float fScale = 1000.0f;
	fCurrentAltitude /= fScale;

	// values are drawn from -1 to 1 so need to rescale the current altitude
	fCurrentAltitude *= 2.0f;
	fCurrentAltitude -= 1.0f;

	grcDebugDraw::Meter(vAltDrawPosBar,Vector2(0.0f,-1.0f),fAxisLength,fEndPointSize,Color_white,"Altitude");
	grcDebugDraw::MeterValue(vAltDrawPosBar,Vector2(0.0f,-1.0f),fAxisLength,fCurrentAltitude,fEndPointSize,Color_red);

}

#endif		// __BANK
