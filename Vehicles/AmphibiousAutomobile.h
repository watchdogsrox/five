#ifndef __AMPHIBIOUS_AUTOMOBILE_H__
#define __AMPHIBIOUS_AUTOMOBILE_H__

#include "vehicles/vehicle.h"
#include "vehicles/Automobile.h"
#include "vehicles/Boat.h"
//#include "vehicles/Propeller.h"

#include "control/replay/ReplaySettings.h"

//
//
//
class CAmphibiousAutomobile : public CAutomobile
{
protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CAmphibiousAutomobile( const eEntityOwnedBy ownedBy, u32 popType );
	CAmphibiousAutomobile( const eEntityOwnedBy ownedBy, u32 popType, VehicleType vehType );
	friend class CVehicleFactory;

	virtual void UpdatePhysicsFromEntity(bool bWarp=false);
	virtual bool PlaceOnRoadAdjustInternal(float HeightSampleRangeUp, float HeightSampleRangeDown, bool bJustSetCompression);

public:

	~CAmphibiousAutomobile();

// 	// physics

	//virtual void InitDoors();
	virtual void SetModelId(fwModelId modelId){ CAutomobile::SetModelId(modelId);
												m_BoatHandling.SetModelId(this, modelId);	}
													 
	virtual int InitPhys();
	virtual void AddPhysics();

	// Physics
	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	//virtual const eHierarchyId* GetExtraBonesToDeform(int& extraBoneCount);
	virtual bool WantsToBeAwake();

	// Damage
	virtual void Fix(bool resetFrag = true, bool allowNetwork = false);

	// Audio
	void CheckForAudioModeSwitch(bool isFocusVehicle BANK_ONLY(, bool forceBoat));

	// Control
	virtual void DoProcessControl(bool fullUpdate, float fFullUpdateTimeStep);
	//void  ProcessBuoyancy(){ m_SubHandling.ProcessBuoyancy(this); }

	CBoatHandling* GetBoatHandling() { return &m_BoatHandling; }
	const CBoatHandling* GetBoatHandling() const { return &m_BoatHandling; }
	CAnchorHelper& GetAnchorHelper() { return m_AnchorHelper; }

	// Render
	virtual ePrerenderStatus PreRender(const bool bIsVisibleInMainViewport = true);

	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	static dev_float sm_fJetskiRiderResistanceMultMax;
	static dev_float sm_fJetskiRiderResistanceMultMin;

	// How many milliseconds the propeller has to be out of water for it to count as not submerged
	static dev_float sm_fPropellerTimeOutOfWaterTolerance;

	bool IsPropellerSubmerged();

	CTransmission* GetSecondTransmission() { return &m_AmphibiousAutomobileTransmission; }

	bool GetShouldReverseBrakeAndThrottle() { return m_bReverseBrakeAndThrottle; }
	void SetReverseBrakeAndThrottle(bool bNewValue) { m_bReverseBrakeAndThrottle = bNewValue; }

	virtual void ApplyDeformationToBones(const void* basePtr);

	float GetBoatEngineThrottle();

	bool GetShouldUseBoatNetworkBlend() { return m_bUseBoatNetworkBlend; }

private:
	void SetDamping();
	bool ProcessLowLodBuoyancy();
	void ComputeLowLodBuoyancyPlaneNormal(const Matrix34& boatMat, Vector3& vPlaneNormal);

	void InitAmphibiousAutomobile();

	CBoatHandling		m_BoatHandling;
	CAnchorHelper		m_AnchorHelper;
	u32					m_LastEngineModeSwitchTime;

	CTransmission m_AmphibiousAutomobileTransmission;

	// Flag for network clones to indicate when to start/stop using boat network blending instead of car blending
	bool m_bUseBoatNetworkBlend;

	// When going backwards fast and pressing accelerate, cars apply a brake first to lower the speed and then apply a throttle
	// This is undesirable for the boat transmission, so we need a way to know when it's happening
	bool m_bReverseBrakeAndThrottle;
};

class CAmphibiousQuadBike : public CAmphibiousAutomobile
{
protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CAmphibiousQuadBike( const eEntityOwnedBy ownedBy, u32 popType );
	friend class CVehicleFactory;

	virtual ePhysicsResult ProcessPhysics(float fTimeStep, bool bCanPostpone, int nTimeSlice);
	void ProcessPostPhysics();

	virtual void PreRender2(const bool bIsVisibleInMainViewport = true);

	virtual void ProcessPreComputeImpacts(phContactIterator impacts);

public:
	~CAmphibiousQuadBike();

	// Toggles whether the wheels should tuck in or out
	void ToggleTuckInWheels();

	// Returns the amounts of tilt and shift inwards that the wheels should render with
	float GetWheelTiltAmount() const { return m_fWheelTiltAmount; }
	float GetWheelShiftAmount() const { return m_fWheelShiftAmount; }
	
	// If wheels are titled or shifted, we don't want to spin and steer them
	bool ShouldRenderWheelSteerAndSpin() const { return m_fSuspensionRaiseAmount <= sm_fWheelRaiseForTiltAndShift; }
	
	// Returns true if wheels are either fully out or fully tucked in
	bool IsWheelTransitionFinished() const;

	// Returns true if wheels are tucking in or are tucked in
	bool ShouldTuckWheelsIn() const { return m_bShouldTuckInWheels; }

	void SetTuckWheelsIn(bool tuckIn) { m_bShouldTuckInWheels = tuckIn; }

	// Returns true if wheels are not tucked in at all
	bool IsWheelsFullyOut() const { return !ShouldTuckWheelsIn() && IsWheelTransitionFinished(); }

	// Returns true if wheels are fully tucked in
	bool IsWheelsFullyIn() const { return ShouldTuckWheelsIn() && IsWheelTransitionFinished(); }

	// Returns true if wheels are fully raised (but not necessarily tilted/shifted)
	bool IsWheelsFullyRaised() const { return m_fSuspensionRaiseAmount == sm_fWheelRetractRaiseAmount; }

	// Force the wheels to be not retracted
	void ForceWheelsOut();

	// Force the wheels to be not retracted
	void ForceWheelsIn();
	
	virtual bool WantsToBeAwake();

	bool GetRetractButtonHasBeenUp() { return m_bRetractButtonHasBeenUp; }
	void SetRetractButtonHasBeenUp(bool bNewValue) { m_bRetractButtonHasBeenUp = bNewValue; }
	void ToggleTuckInWheelsFromNetwork() { m_bToggleTuckInWheelsFromNetwork = true; }

private:
	static dev_float sm_fWheelRaiseForTiltAndShift; // How much the wheels need to be raised before it's safe to tilt and shift them
	static dev_float sm_fWheelRetractRaiseAmount; // How much the wheels should be raised when tucking them in
	static dev_float sm_fWheelRetractShiftAmount; // How much the wheels should shift inwards when tucking them in
	static dev_float sm_fWheelRetractTiltAmount; // How much the wheels should tilt inwards when tucking them in

	// State to control whether the wheels should tuck in or not
	bool m_bShouldTuckInWheels;

	// Indicates if the button has been held up
	bool m_bRetractButtonHasBeenUp;

	float m_fWheelTiltAmount; // Amount that the wheels are currently tilted
	float m_fWheelShiftAmount; // Amount that the wheels are currently shifted
	float m_fSuspensionRaiseAmount; // Amount that the wheels are currently raised by

	float m_fWheelsFullyRetractedTime;

};

#endif //__AMPHIBIOUS_AUTOMOBILE_H__
