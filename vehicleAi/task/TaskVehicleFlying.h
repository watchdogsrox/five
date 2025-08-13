#ifndef TASK_VEHICLE_FLYING_H
#define TASK_VEHICLE_FLYING_H

// Gta headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task\System\TaskTypes.h"
#include "Task/System/Tuning.h"
#include "VehicleAi\task\TaskVehicleMissionBase.h"
#include "VehicleAi\task\TaskVehicleGotoHelicopter.h"

class CVehicle;
class CVehControls;
class CHeli;
class CPlane;

//Rage headers.
#include "Vector/Vector3.h"

//
//
//
class CTaskVehicleCrash : public CTaskVehicleMissionBase
{
public:

	struct Tunables : CTuning
	{
		Tunables();

		float	m_MinSpeedForWreck;

		PAR_PARSABLE;
	};

	// mission flags
	enum CrashFlags
	{
		CF_BlowUpInstantly	= BIT(0),	
		CF_InACutscene		= BIT(1),														
		CF_AddExplosion		= BIT(2),
		CF_ExplosionAdded	= BIT(3),
		CF_HitByConsecutiveExplosion	= BIT(4),
	};

public:
	typedef enum
	{
		State_Invalid = -1,
		State_Crash = 0,
		State_Wrecked,
		State_Exit,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleCrash(CEntity *pCulprit = NULL, s32 crashFlags = CF_AddExplosion, u32 weaponHash = WEAPONTYPE_EXPLOSION );
	~CTaskVehicleCrash();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_CRASH; }
	aiTask*			Copy() const 
	{
		CEntity *pCulprit =  const_cast<CEntity*>(m_culprit.GetEntity());
		return rage_new CTaskVehicleCrash(pCulprit, m_iCrashFlags, m_iWeaponHash);
	}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Exit;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL
	
	// Crash flags
	inline bool IsCrashFlagSet(s32 val) const { return (m_iCrashFlags&val)==val; }
	inline s32 GetCrashFlags() const { return m_iCrashFlags; }
	void SetCrashFlag(s32 val, bool bSet) { bSet ? (m_iCrashFlags |= val) : (m_iCrashFlags &= ~val); }

	void DoBlowUpVehicle(CVehicle* pVehicle);

	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleCrash>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		m_culprit.Serialise(serialiser);
	}

private:

	// FSM function implementations
	// State_Crash
	void			Crash_OnEnter					(CVehicle* pVehicle);
	FSM_Return		Crash_OnUpdate					(CVehicle* pVehicle);
	// State_Wrecked
	void			Wrecked_OnEnter					(CVehicle* pVehicle);
	FSM_Return		Wrecked_OnUpdate				(CVehicle* pVehicle);
	void			Wrecked_OnExit					(CVehicle* pVehicle);
 
	FSM_Return		Plane_SteerToCrashAndBurn		(CPlane *pPlane);
	FSM_Return		Helicopter_SteerToCrashAndBurn	(CHeli* pHeli);

	// Helper functions:
	bool CheckForCollisionWithGroundWhenFarFromPlayer(CVehicle* pVehicle);

	CSyncedEntity	m_culprit;
	u32		m_iWeaponHash;
	s32		m_iCrashFlags;

	float m_fOutOfControlRoll;
	float m_fOutOfControlYaw;
	float m_fOutOfControlPitch;
	float m_fOutOfControlThrottle;
	u32 m_uOutOfControlStart;
	u32 m_uOutOfControlRecoverStart;
	s32 m_iOutOfControlRecoverPeriods;
	bool m_bOutOfControlRecovering;
	
private:

	static Tunables sm_Tunables;
	
};



//
//Currently only working on Helicopters
//
class CTaskVehicleLand : public CTaskVehicleGoToHelicopter
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Land = 0
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleLand(const sVehicleMissionParams& params, const s32 iHeliFlags = 0, const float fHeliRequestedOrientation = -1.0f);
	~CTaskVehicleLand();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_LAND; }
	aiTask*			Copy() const {return rage_new CTaskVehicleLand(m_Params, m_iHeliFlags, m_fHeliRequestedOrientation);}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Land;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

private:

	bool CheckForVehicleCollision(CVehicle& in_Vehicle) const;
	bool CheckForPedCollision(CVehicle& in_Vehicle) const;

	enum eGroundProbes
	{
		eGroundProbe_Front,
		eGroundProbe_Back,
		eGroundProbe_Left,
		eGroundProbe_Right,
		eNumGroundProbes
	};

	// FSM function implementations
	// State_Land
	void			Land_OnEnter(CVehicle* pVehicle);
	FSM_Return		Land_OnUpdate(CVehicle* pVehicle);

	WorldProbe::CShapeTestSingleResult* m_ShapeTestResults;

	float   m_fTimeOnTheGround;
	float	m_fPreviousTargetHeight;

	int		m_iMinHeightAboveTerrainCached;
};


//
//Currently only working on Helicopters
//
class CTaskVehicleHover : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Start = 0,
		State_HoverHeli,
		State_HoverVtol,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleHover();
	~CTaskVehicleHover();

	int				GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_HOVER; }
	aiTask*			Copy() const {return rage_new CTaskVehicleHover();}

	// FSM implementations
	FSM_Return		UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32			GetDefaultStateAfterAbort()	const {return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

	virtual bool IsSyncedAcrossNetwork() const { return true; }

private:

	// FSM function implementations

	// State_Start
	void			Start_OnEnter(CVehicle* pVehicle);
	FSM_Return		Start_OnUpdate(CVehicle* pVehicle);

	// State_Hover
	void			HoverHeli_OnEnter(CVehicle* pVehicle);
	FSM_Return		HoverHeli_OnUpdate(CVehicle* pVehicle);
	void			HoverHeli_OnExit(CVehicle* pVehicle);


	// State_Hover
	void			HoverVtol_OnEnter(CVehicle* pVehicle);
	FSM_Return		HoverVtol_OnUpdate(CVehicle* pVehicle);
};

class CTaskVehicleFleeAirborne : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Invalid = -1,
		State_Flee,
		//State_Land,//not implemented yet.
	} VehicleControlState;

	static const unsigned DEFAULT_FLIGHT_HEIGHT = 30;
	static const unsigned DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN = 20;


	static int ComputeFlightHeightAboveTerrain(const CVehicle& in_Vehicle, int iMinHeight);

	// Constructor/destructor
	CTaskVehicleFleeAirborne( const sVehicleMissionParams& params,
		const int iFlightHeight = DEFAULT_FLIGHT_HEIGHT, 
		const int iMinHeightAboveTerrain = DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN,
		const bool bUseDesiredOrientation = false,
		const float fDesiredOrientation = 0.0f,
		const float fDesiredSlope = 0.0f);

	virtual ~CTaskVehicleFleeAirborne();

	virtual int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_FLEE_AIRBORNE; }
	virtual aiTask*		Copy() const {return rage_new CTaskVehicleFleeAirborne(m_Params,
		m_iFlightHeight,
		m_iMinHeightAboveTerrain,
		m_bUseDesiredOrientation,
		m_fDesiredOrientation,
		m_fDesiredSlope);}

	// FSM implementations
	virtual FSM_Return	UpdateFSM					(const s32 iState, const FSM_Event iEvent);

	s32					GetDefaultStateAfterAbort	()	const {return State_Flee;}

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char*  GetStaticStateName			( s32 iState );

	//virtual void		Debug() const;
#endif //!__FINAL

	// network:
	virtual bool IsSyncedAcrossNetwork() const { return true; }
	virtual CTaskVehicleSerialiserBase*	GetTaskSerialiser() const { return rage_new CTaskVehicleSerialiser<CTaskVehicleFleeAirborne>; }

	void Serialise(CSyncDataBase& serialiser)
	{
		static const unsigned SIZEOF_FLIGHT_HEIGHT = 12;
		static const unsigned SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN = 12;
		static const unsigned SIZEOF_DESIRED_ORIENTATION = 10;

		CTaskVehicleMissionBase::Serialise(serialiser);

		bool hasDefaultParams = (m_iFlightHeight == DEFAULT_FLIGHT_HEIGHT && 
								m_iMinHeightAboveTerrain == DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN &&
								!m_bUseDesiredOrientation);

		SERIALISE_BOOL(serialiser, hasDefaultParams);

		if (!hasDefaultParams || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_iFlightHeight, SIZEOF_FLIGHT_HEIGHT, "Flight height");
			SERIALISE_UNSIGNED(serialiser, m_iMinHeightAboveTerrain, SIZEOF_MIN_HEIGHT_ABOVE_TERRAIN, "Min height above terrain");
			SERIALISE_BOOL(serialiser, m_bUseDesiredOrientation);

			if (m_bUseDesiredOrientation || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fDesiredOrientation, TWO_PI, SIZEOF_DESIRED_ORIENTATION, "Desired orientation");
			}
		}
		else
		{
			m_iFlightHeight				= DEFAULT_FLIGHT_HEIGHT;
			m_iMinHeightAboveTerrain	= DEFAULT_MIN_HEIGHT_ABOVE_TERRAIN;
			m_bUseDesiredOrientation	= false;
			m_fDesiredOrientation		= 0.0f;
		}
	}

protected:

	// FSM function implementations
	// State_Flee
	void		Flee_OnEnter						(CVehicle* pVehicle);
	FSM_Return	Flee_OnUpdate						(CVehicle* pVehicle);

	Vector3::Return CalculateTargetCoords(const CVehicle* pVehicle);

	float	m_fDesiredSlope;
	float	m_fDesiredOrientation;
	int		m_iFlightHeight; 
	int		m_iMinHeightAboveTerrain;
	bool	m_bUseDesiredOrientation;
};

#endif
