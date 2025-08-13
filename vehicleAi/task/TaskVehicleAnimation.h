#ifndef TASK_VEHICLE_ANIMATION_H
#define TASK_VEHICLE_ANIMATION_H

#include "streaming/streamingrequest.h"

// Gta headers.
#include "Task/System/Task.h"
#include "Task/System/TaskComplex.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/TaskTypes.h"
#include "VehicleAi/task/TaskVehicleMissionBase.h"

class CVehicle;
class CVehControls;

//Rage headers.
#include "Vector/Vector3.h"

#define DISPLAY_HELP_TEXT 0
//
//
//
class CTaskVehicleAnimation : public CTaskVehicleMissionBase
{
public:
	typedef enum
	{
		State_Start = -1,
		State_PlayingAnim = 0,
		State_Finish,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleAnimation(const strStreamingObjectName pAnimDictName, const char* pAnimName, float playBackRate = 1.0f, float playBackPhase = 0.0f, bool bHoldatEnd = true, bool useExtractedVelocity = false);
	CTaskVehicleAnimation(u32 animDictIndex, u32 animHashKey, float playBackRate = 1.0f, float playBackPhase = 0.0f, bool bHoldatEnd = true, bool useExtractedVelocity = false );
    ~CTaskVehicleAnimation();


	int					GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_ANIMATION; }
	CTask*				Copy() const {return rage_new CTaskVehicleAnimation(m_animDictIndex, m_animHashKey);}

	// FSM implementations
	FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32					GetDefaultStateAfterAbort()	const {return State_Start;}
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

    bool        IsAnimFinished          () const;

protected:

	// Play an animation
	FSM_Return	Start_OnUpdate			(CVehicle* pVehicle);
	void		PlayingAnim_OnEnter		(CVehicle* pVehicle);
	FSM_Return	PlayingAnim_OnUpdate	(CVehicle* pVehicle);
	void		PlayingAnim_OnExit		(CVehicle* pVehicle);

	void		StartAnim				(CVehicle* pVehicle);

	// Anim slot index and animation hash key
	// We can use these to get the animation data 
	// We need to get the animation priority and the animation flags separately
	s32			m_animDictIndex;
	u32			m_animHashKey;

	float		m_playBackRate;
    float       m_playBackPhase;
    bool        m_holdAtEndOfAnimation;//hold the animation on the last frame
	bool		m_useExtractedVelocity;//apply the velocity from the animation to the vehicle?

#if __DEV
	char	m_animName[ANIM_NAMELEN];
	strStreamingObjectName m_animDictName;
#endif //__DEV
};



//
//
//
class CTaskVehicleConvertibleRoof : public CTaskVehicleMissionBase
{
public:

    // External systems don't need to know about internal states
    // Keep in sync with commands_vehicle.sch!
    enum eConvertibleStates
    {
        STATE_RAISED,
        STATE_LOWERING,
        STATE_LOWERED,
        STATE_RAISING,
		STATE_CLOSING_BOOT,
		STATE_ROOF_STUCK_RAISED,
		STATE_ROOF_STUCK_LOWERED
    };

	typedef enum
	{
        State_Invalid = -1,
        State_Roof_Raised = 0,
        State_Lower_Roof,
        State_Roof_Lowered,
        State_Raise_Roof,
		State_Closing_Boot,
		State_Roof_Stuck_Raised,
		State_Roof_Stuck_Lowered,
        State_Finish,
	} VehicleControlState;

	// Constructor/destructor
	CTaskVehicleConvertibleRoof( bool bInstantlyMoveRoof = false, bool bRoofLowered = false );
	~CTaskVehicleConvertibleRoof();

	int					GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_CONVERTIBLE_ROOF; }
	CTask*				Copy() const {return rage_new CTaskVehicleConvertibleRoof();}

	// FSM implementations
	void				CleanUp();
	FSM_Return			UpdateFSM(const s32 iState, const FSM_Event iEvent);
	s32					GetDefaultStateAfterAbort()	const {return GetState();} // just return the current state as this is a physical state and should remain where it is when aborted.
#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

    eConvertibleStates  GetRoofState();
	float GetRoofProgress();

    static float	ms_fMaximumSpeedToOpenRoofSq;
	static u32		ms_uTimeToHoldButtonDown;
    
    static bool		GetIsLatched			(CVehicle* pParent, s32 roofFragGroup);
    static void		SetLatch				(CVehicle* pParent, s32 roofFragGroup);

    static void     LatchRoof               (CVehicle* pVehicle);
    static void     UnLatchRoof             (CVehicle* pVehicle);

	void MoveImmediately() { m_bInstantlyMoveRoof = true;}
	bool IsInstantTransition() const {return m_bInstantlyMoveRoof;}

protected:

	void InitAnimation(CVehicle* pVehicle);
	
    void		RoofRaised_OnEnter			(CVehicle* pVehicle);
    FSM_Return	RoofRaised_OnUpdate			(CVehicle* pVehicle);

	void		LowerRoof_OnEnter			(CVehicle* pVehicle);
	FSM_Return	LowerRoof_OnUpdate			(CVehicle* pVehicle);

    void		RoofLowered_OnEnter			(CVehicle* pVehicle);
    FSM_Return	RoofLowered_OnUpdate		(CVehicle* pVehicle);

	void		RaiseRoof_OnEnter			(CVehicle* pVehicle);
	FSM_Return	RaiseRoof_OnUpdate			(CVehicle* pVehicle);

	void		ClosingBoot_OnEnter			(CVehicle* pVehicle);
	FSM_Return	ClosingBoot_OnUpdate		(CVehicle* pVehicle);
	void		ClosingBoot_OnExit			(CVehicle* pVehicle);

	void		RoofStuckRaised_OnEnter		(CVehicle* pVehicle);
	FSM_Return	RoofStuckRaised_OnUpdate	(CVehicle* pVehicle);

	void		RoofStuckLowered_OnEnter	(CVehicle* pVehicle);
	FSM_Return	RoofStuckLowered_OnUpdate	(CVehicle* pVehicle);


	strRequest	m_animRequester;


    bool        m_bInstantlyMoveRoof;//signify whether the next lowering/raising action should be instant
#if DISPLAY_HELP_TEXT
    bool        m_HelpMessageDisplayed;
    u32         m_MessageTimeDisplayedFor;
#endif

	VehicleControlState m_DesiredStateAfterClosingBoot;
	u32 m_nStuckCounter; // Number of times to try opening / closing the roof when it's stuck.
	bool m_bOkToDecrementCounter;
	bool m_bWaitingOnDoorClosing;
	bool m_bMechanismShouldJam;

	bool m_bHasButtonBeenUp;
};

class CTaskVehicleTransformToSubmarine : public CTaskVehicleMissionBase
{

public:

	enum eTransformStates
	{
		STATE_CAR,
		STATE_TRANSFORMING_TO_SUBMARINE,
		STATE_SUBMARINE,
		STATE_TRANSFORMING_TO_CAR,
	};

	typedef enum
	{
		State_Invalid = -1,
		State_Car = 0,
		State_Transform_To_Submarine,
		State_Submarine,
		State_Transform_To_Car,

		State_Finish,
	} VehicleTransformControlState;

	// Constructor/destructor
	CTaskVehicleTransformToSubmarine( bool bInstantlyTransform = false );
	~CTaskVehicleTransformToSubmarine() {}

	int			GetTaskTypeInternal() const { return CTaskTypes::TASK_VEHICLE_TRANSFORM_TO_SUBMARINE; }
	CTask*		Copy() const { return rage_new CTaskVehicleTransformToSubmarine(); }

	FSM_Return	UpdateFSM( const s32 iState, const FSM_Event iEvent );
	s32			GetDefaultStateAfterAbort()	const { return State_Car; }
	eTransformStates  GetTransformState();

	void		SetTransformRateForNextAnimation(float fRate) { m_fTransformRateForNextAnimation = fRate; }
	void		SetUseAlternateInput( bool useAlternateInput ) { m_bUseAlternateInput = useAlternateInput; }

#if !__FINAL
	friend class CTaskClassInfoManager;
	static const char * GetStaticStateName( s32  );
#endif //!__FINAL

protected:
	void		Car_OnEnter			(CVehicle* pVehicle);
	FSM_Return	Car_OnUpdate		(CVehicle* pVehicle);

	void		TransformToSubmarine_OnEnter			(CVehicle* pVehicle);
	FSM_Return	TransformToSubmarine_OnUpdate			(CVehicle* pVehicle);

	void		Submarine_OnEnter		(CVehicle* pVehicle);
	FSM_Return	Submarine_OnUpdate		(CVehicle* pVehicle);

	void		TransformToCar_OnEnter			(CVehicle* pVehicle);
	FSM_Return	TransformToCar_OnUpdate			(CVehicle* pVehicle);

	//bool ShouldVehicleTransformToCar( CVehicle* pVehicle );
	//bool ShouldVehicleTransformToSubmarine( CVehicle* pVehicle );

	bool m_bHasButtonBeenUp;
	bool m_bInstantlyTransform;
	bool m_bUseAlternateInput;
	float m_fTransformRateForNextAnimation;

};


#endif
