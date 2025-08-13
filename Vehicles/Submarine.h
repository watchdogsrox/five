#ifndef __SUBMARINE_H__
#define __SUBMARINE_H__

#include "Vehicles/Boat.h"
//#include "vehicles/vehicle.h"
#include "vehicles/Automobile.h"
#include "vehicles/Propeller.h"

#include "network\objects\entities\netObjSubmarine.h"

#include "control/replay/ReplaySettings.h"

#define DISPLAY_CRUSH_DEPTH_HELP_TEXT 1

class audBoatAudioEntity;
class audCarAudioEntity;

class CSubmarineHandling
{
public:
	CSubmarineHandling();
	~CSubmarineHandling();

	void SetModelId(CVehicle *pVehicle, fwModelId modelId);

	// Physics
	ePhysicsResult ProcessPhysics(CVehicle *pVehicle, float fTimeStep, bool bCanPostpone, int nTimeSlice);
	const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount);
	bool WantsToBeAwake(CVehicle *pVehicle);

	// Damage
	void ApplyDeformationToBones(CVehicle *pVehicle, const void* basePtr);
	void Fix(CVehicle *pVehicle, bool resetFrag = true);

	// Control
	void DoProcessControl(CVehicle *pVehicle, bool fullUpdate, float fFullUpdateTimeStep);
	void ProcessBuoyancy(CVehicle *pVehicle);

	// Render
	void PreRender(CVehicle *pVehicle, const bool bIsVisibleInMainViewport = true);
	void PreRender2(CVehicle *pVehicle, const bool bIsVisibleInMainViewport = true);

	// Flight control functions
	// Maybe want a superclass (CAircraft?) so that CPlane uses the same stuff
	void SetYawControl(float fYawControl) { m_fYawControl = fYawControl;}
	void SetDiveControl(float fDiveControl) { m_fDiveControl = fDiveControl; }
	void SetPitchControl(float fPitchControl) { m_fPitchControl = fPitchControl; }
	float GetYawControl() const { return m_fYawControl; }
	float GetDiveControl() const { return m_fDiveControl; }
	float GetPitchControl() const { return m_fPitchControl; }

	void UpdateRudder(CVehicle *pVehicle);
	void UpdateElevators(CVehicle *pVehicle);

	void ForceThrottleForTime( float in_fThrottle, float in_fSeconds );

	// Crush depth code.
	void ProcessCrushDepth(CVehicle *pVehicle);
	void Implode(CVehicle *pVehicle, bool fullyImplode);
	float GetCurrentDepth(CVehicle* pVehicle) const;
	bool IsUnderDesignDepth(CVehicle* pVehicle) const { return GetCurrentDepth(pVehicle) < m_fDesignDepth; }
	bool IsUnderAirLeakDepth(CVehicle* pVehicle) const { return GetCurrentDepth(pVehicle) < m_fAirLeakDepth; }
	bool IsUnderCrushDepth(CVehicle* pVehicle) const { return GetCurrentDepth(pVehicle) < m_fCrushDepth; }
	void SetCrushDepths(bool bEnableCrushDamage, float fDesignDamageDepth, float fAirLeakDepth, float fCrushDepth);
#if DISPLAY_CRUSH_DEPTH_HELP_TEXT
	void DisplayCrushDepthMessages(CVehicle *pVehicle);
#endif // DISPLAY_CRUSH_DEPTH_HELP_TEXT

	void ComputeTimeForNextImplosionEvent();
	u32 GetTimeForNextImplosionEvent() const { return m_nNextImplosionForceTime; }

	float ProcessProps(CVehicle *pVehicle, float fTimeStep, float fDriveInWater);//Returns the drive force
	void ProcessSubmarineHandling(CVehicle *pVehicle, float fTimeStep, float fDriveInWater);
	void ProcessDepthLimit(CVehicle *pVehicle);

	s32 GetNumberOfAirLeaks() const { return m_nAirLeaks; }
	bool IsSubLeakingAir() const { return (GetNumberOfAirLeaks() > 0); }

	bool PassengersShouldDrown(CVehicle *pVehicle) const { return pVehicle->GetStatus()==STATUS_WRECKED || GetNumberOfAirLeaks()>4; }

	bool GetSinksWhenWrecked() const		{ return m_nSubmarineFlags.bSinkWhenWrecked==1; }
	void SetSinksWhenWrecked(bool bSink)	{ m_nSubmarineFlags.bSinkWhenWrecked = bSink ? 1 : 0; }

	bool GetForceSurfaceMode() const { return m_nSubmarineFlags.bForceSurfaceMode==1; }
	void SetForceSurfaceMode(bool bForce) { m_nSubmarineFlags.bForceSurfaceMode = bForce ? 1 : 0; }

	bool GetIsDisplayingCrushWarning() const { return m_nSubmarineFlags.bDisplayingCrushWarning==1; }
	void SetDisplayingCrushWarningFlag(bool bValue) { m_nSubmarineFlags.bDisplayingCrushWarning = bValue ? 1 : 0; }

	bool GetIsPropellerSubmerged() const { return m_nSubmarineFlags.bPropellerSubmarged==1; }

	// Allow external code to force neutral buoyancy (to prevent sub from rising with no driver).
	void ForceNeutralBuoyancy(u32 nTimeMs) { m_iForcingNeutralBuoyancyTime = fwTimer::GetTimeInMilliseconds() + nTimeMs; }

	CAnchorHelper& GetAnchorHelper() { return m_AnchorHelper; }

	void SetMinDepthToWreck( float depth ) { m_fMinWreckDepth = depth; }

	int	GetNumPropellors() const			{ return m_iNumPropellers; }
	float GetPropellorSpeed(int i) const	{ return m_propellers[i].GetSpeed();	}
	void SetPropellorSpeed(int i, float spd){ m_propellers[i].SetSpeed(spd);	}
	int GetPropellorBoneIndex(int i) const	{ return m_propellers[i].GetBoneIndex(); }
	void PreRenderPropellers( CVehicle* pVehicle );

	float GetRudderAngle() const			{ return m_fRudderAngle; }
	void SetRudderAngle(float angle)		{ m_fRudderAngle = angle;}
	float GetElevatorAngle() const			{ return m_fElevatorAngle;}
	void SetElevatorAngle(float angle)		{ m_fElevatorAngle = angle;}

	float m_fPitchControl;
	float m_fYawControl;
	float m_fDiveControl;	// -1.0f = dive, 0.0f = maintain depth, 1.0 = surface
	float m_fMaxDepthReached;
	s32	  m_nAirLeaks;

protected:

	CAnchorHelper m_AnchorHelper;

    float m_fRudderAngle;
    float m_fElevatorAngle;

	// For rendering propellers
	CPropeller m_propellers[SUB_NUM_PROPELLERS];
	int m_iNumPropellers;

	float m_fDesignDepth;
	float m_fAirLeakDepth;
	float m_fCrushDepth;
	float m_fMinWreckDepth;

	float m_fForceThrottleValue;
	float m_fForceThrottleSeconds;

	bool m_bEnableCrushDamage;
	bool m_bCrushDepthsModifiedThisFrame;

	u32 m_iForcingNeutralBuoyancyTime;
	u32 m_nNextImplosionForceTime;

	// We need to make a per-instance copy of the buoyancy info for each submarine since we change the buoyancy constant.
	CBuoyancyInfo* m_pBuoyancyOverride;

	struct SubmarineFlags
	{
		u8 bSinkWhenWrecked			: 1;
		u8 bForceSurfaceMode		: 1;
		u8 bDisplayingCrushWarning	: 1;
		u8 bPropellerSubmarged		: 1;
	};
	SubmarineFlags m_nSubmarineFlags;

    static dev_float ms_fControlSmoothSpeed;	// Change rate of rudder, elevator angles
	static dev_float ms_fSubSinkForceMultStep;	// If sinking when wrecked, this what to decrement the buoyancy force multiplier by each frame.
	static dev_float ms_fCrushStepIncrementSize;
	static dev_float ms_fImplosionDamageAmount;
	static dev_float ms_fSubCarPerImplosionDamageAmount;
	static dev_u32 ms_nNumberOfImplosionVectors;
	static dev_u32 ms_nMinIntervalBeforeNextImplisionEvent;
	static dev_u32 ms_nMaxIntervalBeforeNextImplisionEvent;
};

//
//
//
class CSubmarine : public CVehicle
{
public:
	CSubmarine(const eEntityOwnedBy ownedBy, u32 popType);
	~CSubmarine(){}

	virtual void InitCompositeBound();
	virtual void InitDoors();
	virtual void SetModelId(fwModelId modelId){ 	CVehicle::SetModelId(modelId);
													m_SubHandling.SetModelId(this, modelId); }
	
	virtual void BlowUpCar(CEntity *pCulprit, bool bInACutscene, bool bAddExplosion, bool ASSERT_ONLY(bNetCall), u32 weaponHash, bool bDelayedExplosion);

	// Physics	
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice){ return m_SubHandling.ProcessPhysics(this, fTimeStep, bCanPostpone, nTimeSlice); }
	virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount){ return m_SubHandling.GetExtraBonesToDeform(extraBoneCount); }
	virtual bool WantsToBeAwake(){ return m_SubHandling.WantsToBeAwake(this); }

	// Damage
	virtual void ApplyDeformationToBones(const void* basePtr){ m_SubHandling.ApplyDeformationToBones(this, basePtr); }
	virtual void Fix(bool resetFrag = true, bool allowNetwork = false){	m_SubHandling.Fix(this, resetFrag); CVehicle::Fix(resetFrag, allowNetwork); }

	// Control
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep){	m_SubHandling.DoProcessControl(this, fullUpdate, fFullUpdateTimeStep); 
																				CVehicle::DoProcessControl(fullUpdate, fFullUpdateTimeStep);			}

	void  ProcessBuoyancy(){ m_SubHandling.ProcessBuoyancy(this); }

	// Render
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true){ m_SubHandling.PreRender(this, bIsVisibleInMainViewport); 
																					return CVehicle::PreRender(bIsVisibleInMainViewport);		}

	virtual void PreRender2(const bool bIsVisibleInMainViewport = true){	m_SubHandling.PreRender2(this, bIsVisibleInMainViewport); 
																			CVehicle::PreRender2(bIsVisibleInMainViewport);				}

	virtual CSubmarineHandling* GetSubHandling() { return &m_SubHandling; }
    virtual const CSubmarineHandling* GetSubHandling() const { return &m_SubHandling; }

protected:

	NETWORK_OBJECT_TYPE_DECL( CNetObjSubmarine, NET_OBJ_TYPE_SUBMARINE );

	virtual bool PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression);

private:
	CSubmarineHandling m_SubHandling;

	// arrays of wheels and doors
	CCarDoor m_aSubDoors[NUM_VEH_DOORS_MAX];
};

//
//
//
class CSubmarineCar : public CAutomobile
{
protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CSubmarineCar(const eEntityOwnedBy ownedBy, u32 popType);
	friend class CVehicleFactory;

public:

	typedef enum
	{
		State_Invalid = -1,
		State_StartToSubmarine = 0,
		State_MoveWheelsUp = State_StartToSubmarine,
		State_MoveWheelCoversOut,
		State_FinishToSub,

		State_StartToCar,
		State_MoveWheelCoversIn = State_StartToCar,
		State_MoveWheelsDown,
		State_FinishToCar,

		State_Finished,

	} AnimationState;

	~CSubmarineCar();

// 	// physics
 	virtual void ProcessPostPhysics();

	//virtual void InitDoors();
	virtual void SetModelId(fwModelId modelId){ 	CVehicle::SetModelId(modelId);
													m_SubHandling.SetModelId(this, modelId);	}
													 

	// Physics
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount);
	virtual bool WantsToBeAwake();

	// Damage
	virtual void ApplyDeformationToBones(const void* basePtr){ m_SubHandling.ApplyDeformationToBones(this, basePtr); }
	virtual void Fix(bool resetFrag = true, bool allowNetwork = false);

	// Audio
	void CheckForAudioModeSwitch(bool isFocusVehicle BANK_ONLY(, bool forceBoat));

	// Control
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	void  ProcessBuoyancy(){ m_SubHandling.ProcessBuoyancy(this); }

	// Render
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);

	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	virtual CSubmarineHandling* GetSubHandling() { return &m_SubHandling; }
    virtual const CSubmarineHandling* GetSubHandling() const { return &m_SubHandling; }

	virtual bool IsInSubmarineMode() const { return m_InSubmarineMode; }
	bool AreWheelsActive();
	void SetSubmarineMode( bool submarine, bool forceWheelPosition );
	void SetTransformingToSubmarine(bool instantTransform = false);
	void SetTransformingToCar(bool instantTransform = false);
	void SetTransformingComplete() { m_AnimationState = State_Finished; }
	float GetAnimationPhase() { return m_AnimationPhase; }
	AnimationState GetAnimationState() { return m_AnimationState; }

	virtual void ProcessPreComputeImpacts(phContactIterator impacts);

private:
	void SetAnimationState(AnimationState newState, bool instantTransform = false);
	void UpdateAnimationPhase();
	void UpdateSuspensionPositions();
	void UpdateSteeringAngle();
	void UpdateWheelCovers( float animationPhase );
	void UpdateWheels( float animationPhase );
	void UpdateExtraCollision();

	CSubmarineHandling m_SubHandling;
	float	m_AnimationPhase;
	float	m_PrevElevatorRotation;
	float	m_PrevYawControl;
	bool	m_RudderAngleClunkRetriggerValid;
	bool	m_InSubmarineMode;
	bool	m_SetWheelsDown;
	bool	m_SetWheelsUp;
    bool    m_UpdateCollision;
	u32		m_LastEngineModeSwitchTime;
	AnimationState m_AnimationState;

	static float ms_wheelUpOffset;
	static float ms_animationSpeed;
	static float ms_wheelInset;
	static float ms_wheelCoverInset;
	static float ms_wheelCoverZOffset;
	static float ms_wheelCoverRotation;
	static float ms_wheelSuspensionOffset;
	static float ms_rearWheelUpFactor;

};

#endif //__SUBMARINE_H__
