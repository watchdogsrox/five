#ifndef __PHYSICS_HELPERS_H__
#define __PHYSICS_HELPERS_H__

#include "vector/vector3.h"

struct CCollisionRecord;
class CPhysical;
class CVehicle;

class PhysicsHelpers
{
public:
	static const CCollisionRecord* GetCollisionRecord(const CPhysical* pPhysical, const CPhysical* pOtherPhysical);
	static bool GetCollisionRecordInfo(const CPhysical* pPhysical, const CPhysical* pOtherPhysical, float& out_fHitFwdDot, float& out_fHitRightDot);
	static bool GetCollisionRecordInfo(const CPhysical* pPhysical, const CCollisionRecord* pCollisionRecord, float& out_fHitFwdDot, float& out_fHitRightDot);
	static bool FindVehicleCollisionFault(const CVehicle* victim, const CVehicle* damager, bool& out_isVictimFault, float fAngleVectorsPointSameDir, float fMinFaultVelocityThreshold, const CCollisionRecord** out_collisionRecord = nullptr);
	static bool FindVehicleCollisionFault(const CVehicle* pVictim, const CCollisionRecord* pCollisionRecord, bool& out_isVictimFault, float fAngleVectorsPointSameDir, float fMinFaultVelocityThreshold);
};

//////////////////////////////////////////////////////////////////////////
// Class:	CFlightModelHelper
// Purpose: Helper class to handle physics processing of flying objects
//			Currently just models a plane's flight model. Could add new models
//			in future if needed
class CFlightModelHelper
{
public:

	enum eDragType
	{
		FT_Constant_V,
		FT_Linear_V,
		FT_Squared_V,
		FT_Constant_AV,
		FT_Linear_AV,
		FT_Squared_AV,
		NumDragTypes
	};

	CFlightModelHelper();
	CFlightModelHelper(float fThrustMult,
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
		const Vector3 &vDragConstV = VEC3_ZERO,
		const Vector3 &vDragLinearV = VEC3_ZERO,
		const Vector3 &vDragSquaredV = VEC3_ZERO,
		const Vector3 &vDragConstAV = VEC3_ZERO,
		const Vector3 &vDragLinearAV = VEC3_ZERO,
		const Vector3 &vDragSquaredAV = VEC3_ZERO
		);

	void ProcessFlightModel(CPhysical* pPhysical, float fThrust, float fPitch, float fRoll, float fYaw, const Vector3& vAirSpeed = VEC3_ZERO, bool bIgnoreWind = false, float fOveralMult = 1.0f, float fDragMult = 1.0f, bool bFlattenLocalSpeed=false) const;

	static void ApplyForceCgSafe(const Vector3 &vForce, CPhysical* pPhysical);

	// Torque that would be applied if a force was applied at a certain offset
	static void ApplyTorqueSafe(const Vector3 &vForce, const Vector3 &vOffset, CPhysical* pPhysical);
	static void ApplyTorqueSafe(const Vector3 &vTorque, CPhysical* CPhysical);

	// Utility to turn a circular pad input vector into a square one
	static void MakeCircularInputSquare(Vector2& vInAndOut);

	static void ApplyDamping(CPhysical* pPhysical, const Vector3 vDragCoeff[NumDragTypes], float fOveralMult);

	float GetThrustMult() const { return m_fThrustMult; }
	float GetYawRate() const { return m_fYawRate; }
	float GetPitchRate() const { return m_fPitchRate; }
	float GetRollRate() const { return m_fRollRate; }
	float GetYawStabilise() const { return m_fYawStabilise; }
	float GetRollStabilise() const { return m_fRollStabilise; }
	float GetPitchStabilise() const { return m_fPitchStabilise; }
	float GetFormLiftMult() const { return m_fFormLiftMult; }
	float GetAttackLiftMult() const { return m_fAttackLiftMult; }
	float GetAttackDiveMult() const { return m_fAttackDiveMult; }
	float GetSideSlipMult() const { return m_fSideSlipMult; }
	float GetThrottleFallOff() const { return m_fThrottleFallOff;}

	void SetThrustMult(float fThrustMult) { m_fThrustMult = fThrustMult; }
	void SetYawRate(float fYawRate) { m_fYawRate = fYawRate; }
	void SetPitchRate(float fPitchRate) { m_fPitchRate = fPitchRate; }
	void SetRollRate(float fRollRate) { m_fRollRate = fRollRate; }
	void SetYawStabilise(float fYawStabilise) { m_fYawStabilise = fYawStabilise; }
	void SetPitchStabilise(float fPitchStabilise) { m_fPitchStabilise = fPitchStabilise; }
	void SetRollStabilise(float fRollStabilise) { m_fRollStabilise = fRollStabilise; }
	void SetFormLiftMult(float fFormLiftMult) {m_fFormLiftMult = fFormLiftMult; }
	void SetAttackLiftMult(float fAttackLiftMult) { m_fAttackLiftMult = fAttackLiftMult; }
	void SetAttackDiveMult(float fAttackDiveMult) { m_fAttackDiveMult = fAttackDiveMult; }
	void SetSideSlipMult(float fSideSlipMult) { m_fSideSlipMult = fSideSlipMult; }
	void SetThrottleFallOff(float fThrottleFallOff) { m_fThrottleFallOff = fThrottleFallOff; }

	const Vector3& GetDragCoeff(eDragType iType) const { return m_vDragCoeff[iType]; }
	void SetDragCoeff(const Vector3& vDragCoeff, eDragType iType) { m_vDragCoeff[iType] = vDragCoeff; }

#if __BANK
	void AddWidgetsToBank(bkBank* pBank);
	void DebugDrawControlInputs(const CPhysical* pPhysical,float fThrust, float fPitch, float fRoll, float fYaw) const;
	void DebugDrawAltitudeCircular(const CPhysical* pPhysical);
	void DebugDrawAltitudeBar(const CPhysical* pPhysical);
#endif

private:

	float m_fThrustMult;
	float m_fYawRate;
	float m_fPitchRate;
	float m_fRollRate;
	float m_fYawStabilise;
	float m_fRollStabilise;
	float m_fPitchStabilise;
	float m_fFormLiftMult;
	float m_fAttackLiftMult;
	float m_fAttackDiveMult;
	float m_fSideSlipMult;
	float m_fThrottleFallOff;

	// Friction coeffs in physical's local space
	Vector3 m_vDragCoeff[NumDragTypes];

#if __BANK
	// For debugging
	mutable float m_fLastEffectiveThrottle;

	bool m_bRender;
	bool m_bRenderOrigin;
	bool m_bRenderTail;
#endif
};

#endif // __PHYSICS_HELPERS_H__
