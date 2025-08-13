#ifndef TASK_VEHICLE_ATTACK_H
#define TASK_VEHICLE_ATTACK_H

// Gta headers.
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "VehicleAi\racingline.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"

class CVehicle;
class CVehControls;
class CHeli;
class CVehicleWeapon;

//Rage headers.
#include "Vector/Vector3.h"

//
//
//
class CTaskVehicleAttack : public CTaskVehicleMissionBase
{
public:

	struct GeometryTestResults
	{
		explicit GeometryTestResults(CVehicle* pVehicle)
		: m_pVehicle(pVehicle)
		, m_bObstructed(false)
		{
		}
		
		CVehicle*	m_pVehicle;
		bool		m_bObstructed;
	};

	typedef enum
	{
		State_Invalid = -1,
		State_Attack = 0,
		State_Circle,
		State_Stop,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleAttack(const sVehicleMissionParams& params);
	~CTaskVehicleAttack();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_ATTACK; }
	aiTask*			Copy() const;

	// FSM implementations
	FSM_Return		ProcessPreFSM();
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Attack;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual void		Debug() const;
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleAttack>; }

	void		SetHelicopterSpecificParams(float fHeliRequestedOrientation, int iFlightHeight, int iMinHeightAboveTerrain, s32 iHeliFlags);
	void		SetSubmarineSpecificParams(int iMinHeightAboveTerrain, s32 iSubFlags );

	static bool IsAbleToFireWeapons			(CVehicle& vehicle, const CPed* pFiringPed);

private:

	// FSM function implementations
	// State_Attack
	void			Attack_OnEnter(CVehicle* pVehicle);
	FSM_Return		Attack_OnUpdate(CVehicle* pVehicle);
	// State_Stop
	void			Stop_OnEnter(CVehicle* pVehicle);
	FSM_Return		Stop_OnUpdate(CVehicle* pVehicle);
	// State_Circle
	void			Circle_OnEnter(CVehicle* pVehicle);
	FSM_Return		Circle_OnUpdate(CVehicle* pVehicle);

	void			FireAtTarget(CPed* pFiringPed, CVehicle* pVehicle, CVehicleWeapon* pVehicleWeapon, const CEntity* pTarget, const Vector3& vTargetPos);
	void			SelectBestVehicleWeapon(CVehicle& in_Vehicle);
	
	static bool IsTooCloseToGeometryToFire	(CVehicle* pVehicle, const Vector3& vPos, const float fRadius);
	static bool IsTooCloseToGeometryToFireCB(fwEntity* pEntity, void* pData);

	WorldProbe::CShapeTestSingleResult m_LOSResult;
	Vector3 m_TargetLastAlivePosition;
	bool m_bTargetWasAliveLastFrame;
	bool m_bHasLOS;

	u32	m_uTimeWeaponSwap;
	u32 m_uTimeLostLOS;

	// Optional helicopter params
	float	m_fHeliRequestedOrientation;
	int		m_iFlightHeight; 
	int		m_iMinHeightAboveTerrain;
	s32		m_iHeliFlags;

	// Optional Sub params
	s32		m_iSubFlags;

#if DEBUG_DRAW
	bool bLosHit;
	Vector3 vHitLocation;
	WorldProbe::CShapeTestProbeDesc m_LosProbe;
#endif
};

//
//
//
class CTaskVehicleAttackTank : public CTaskVehicleMissionBase
{

public:

	enum State
	{
		State_Start = 0,
		State_Approach,
		State_Stop,
		State_Flee,
		State_Finish,
	};

	// Constructor/destructor
	CTaskVehicleAttackTank(const sVehicleMissionParams& params);
	~CTaskVehicleAttackTank();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_ATTACK_TANK; }
	aiTask*			Copy() const;

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
	virtual void		Debug() const;
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleAttackTank>; }

private:

	FSM_Return	Start_OnUpdate();
	void		Approach_OnEnter();
	FSM_Return	Approach_OnUpdate();
	void		Stop_OnEnter();
	FSM_Return	Stop_OnUpdate();
	void		Flee_OnEnter();
	FSM_Return	Flee_OnUpdate();

};

#endif
