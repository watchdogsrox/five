#ifndef INC_LANDING_GEAR_H
#define INC_LANDING_GEAR_H

// Rage headers
#include "fwtl/pool.h"
#include "physics/contactiterator.h"

// Framework headers
#include "fwmaths/vector.h"

// Game headers
#include "control/replay/replaysettings.h"
#include "renderer/hierarchyids.h"


#if __BANK
#include "bank/bank.h"
#endif

// Forward declarations
class CVehicle;
class fragInstGta;
class CWheel;

///////////////////////////////////////////////////////////////////////////
// Landing gear system
//
// Notes on use:
// Any vehicle that wishes to make use of landing gear needs to initialise a 
// CLandingGear object and call the relevent setup function (InitPhys, ProcessPhysics etc)
// 
// The CLandingGear itself owns a collection of LandingGearParts. The landing gear runs a state
// machine and calls certain instructions on the parts at various points on the state machine.
// This allows different types of landing gear deployment to work in a CLandingGear without 
// the CLandingGear needing to worry about the underlying implementation.
// e.g. In future we could implement landing gear parts that just moves wheels manually without physics.

class CLandingGearPartBase
{
public:

	FW_REGISTER_CLASS_POOL(CLandingGearPartBase);

	// Terminology:
	// Open = Deployed
	// Closed = Retracted

	enum eLandingGearInstruction
	{
		// Ideally we wouldn't need seperate lock instructions
		// for open / closed states, but we can only latch joints
		// in authored position so need to deal with other cases differently
		LOCK_UP,			// Lock retracted gear
		UNLOCK_UP,			// Unlock retracted gear
		LOCK_DOWN,			// Lock deployed gear
		UNLOCK_DOWN,		// Unlock deployed gear
		DRIVE_DOWN,			// Deploy gear
		DRIVE_UP,			// Retract gear
		SWING_FREELY,		// Set to swing freely
		FORCE_UP,			// Snap collision down
		FORCE_DOWN,			// Snap collision up
		NUM_INSTRUCTIONS
	};

	virtual ~CLandingGearPartBase() {}

	virtual void InitPhys(CVehicle* UNUSED_PARAM(pParent), eHierarchyId UNUSED_PARAM(nId)) {}
	virtual void InitCompositeBound(CVehicle* UNUSED_PARAM(pParent)) {}

    virtual void Fix() {}
	virtual void ApplyDeformationToPart(CVehicle* UNUSED_PARAM(pParentVehicle), const void *UNUSED_PARAM(basePtr)){};
	virtual void ProcessPreRenderWheels(CVehicle *UNUSED_PARAM(pParent)){};

	// Returns true if wheel position may have moved
	// fCurrentOpenRatio: 0 = closed, 1 = open
	virtual bool Process(CVehicle* pParent, eLandingGearInstruction nInstruction, float& fCurrentOpenRatio) = 0;
	virtual void InitWheel(CVehicle *UNUSED_PARAM(pParent)) {}
	virtual int GetWheelIndex() const {return -1;}
	virtual Vector3 GetWheelDeformation(const CVehicle *UNUSED_PARAM(pParent)) const {return VEC3_ZERO;}

};

class CLandingGearPartPhysical : public CLandingGearPartBase
{
	// A note on latching, locking
	// There are two ways to restrict an articulated bodies joints:
	//	1) Latch the joints
	//		This actually removes the link in the articulated body making the system totally rigid.
	//		This method has the advantage of making the joint completely solid, so there is no danger of a 
	//		large impulse breaking the landing gear. However it can only be done in the authored position
	//		and *may* affect the handling of the plane.
	//	2) Lock the joints
	//		This keeps the articualted link in the body but just sets the constraint limits so that the 
	//		body part should not use. However due to the mass difference in the landing gear and the chasis
	//		it can produce low quality results. This is used when the landing gear is closed because we can't 
	//		latch joints in non-authored positions. It also should not affect handling (locked vs unlocked)
	//	Future work:
	//		Need to just have one method for doing this

public:
	CLandingGearPartPhysical();

	virtual void InitPhys(CVehicle* pParent, eHierarchyId nId);
	virtual void InitCompositeBound(CVehicle* pParent);

	// Latch all the landing gear's joints
	virtual void LatchPart(CVehicle* pParent);
	virtual void UnlatchPart(CVehicle* pParent);

    virtual void Fix(){ m_bIsLatched = true; m_vLandingGearDeformation = VEC3_ZERO; m_vRetractedWheelDeformation = VEC3_ZERO;}

	// Static utility functions	
	static void SetJointSwingFreely(fragInstGta* pFragInst, int iChildIndex);
	virtual void ApplyDeformationToPart(CVehicle* pParentVehicle, const void *basePtr);
	virtual void ProcessPreRenderWheels(CVehicle *pParent);
	virtual void InitWheel(CVehicle *pParent);
	virtual int GetWheelIndex() const {return m_iWheelIndex;}
	virtual Vector3 GetWheelDeformation(const CVehicle *pParent) const;

protected:
	int m_iFragChild;
	bool m_bIsLatched;
	Vector3 m_vLandingGearDeformation;
	Vector3 m_vRetractedWheelDeformation;
	Vector3 m_vRetractedWheelOffset;
	int m_iWheelIndex;
	eHierarchyId m_eId;
};


class CLandingGearPartPhysicalRot : public CLandingGearPartPhysical
{
public:

	virtual bool Process(CVehicle* pParent, eLandingGearInstruction nInstruction, float& fCurrentOpenRatio);

	void DriveToTargetOpenRatio(CVehicle* pParent, float fTargetOpenRatio, float& fCurrentOpenRatio);

	// Freeze and unfreeze joints by adjusting the constraint limits
	// Shouldn't be called if joints are already locked
	void LockPart(CVehicle* pParent);
	void UnlockPart(CVehicle* pParent);

	// Static utility functions
	static void FindDefaultJointLimits(fragInstGta* pFragInst, int iChildIndex, float &fMin, float &fMax, Vector3 *pAxis = NULL);
	static void RestoreDefaultJointLimits(fragInstGta* pFragInst, int iChildIndex);
	static void Freeze1DofJointInCurrentPosition(fragInstGta* pFragInst, int iChildIndex);
	static void Snap1DofJointToOpenRatio(fragInstGta* pFragInst, int iChildIndex, float fOpenRatio);
};

/*
class CLandingGearPartTransPhysical : public CLandingGearPartPhysical
{
virtual bool Process(eLandingGearState eCurrentState);

int m_iFragChild;
};

// For non physical attachments

class CLandingGearPartTransBasic
{

};

// For non physical attachments
class CLandingGearPartRotBasic
{

};
*/

class CLandingGearPartFactory
{
public:
	static CLandingGearPartBase* CreatePart(CVehicle* pParent, eHierarchyId nId);
};

// Deals with landing gear
// Landing gear should have physics and joint limits specifying rotation axis
class CLandingGear
{

	// More states than I really need here because need to deal with enter /exit as seperate states
	enum eLandingGearState
	{
		GEAR_INIT,
		GEAR_LOCKED_DOWN,
		GEAR_LOCKED_DOWN_RETRACT,		// Gear wants to retract
		GEAR_LOCKED_DOWN_INIT,			// Enter the locked down state
		GEAR_LOCKED_DOWN_POST_INIT,		// Enter the post locked down state
		GEAR_RETRACTING,
		GEAR_DEPLOYING,
		GEAR_RETRACT_INSTANT,
		GEAR_DEPLOY_INSTANT,
		GEAR_LOCKED_UP_DEPLOY,			// Gear wants to deploy
		GEAR_LOCKED_UP,
		GEAR_LOCKED_UP_INIT,			// Enter the locked up state
        GEAR_LOCKED_UP_POST_INIT,		// Enter the post locked up state
		GEAR_BROKEN						// Gear free to move about physically
	};

public:

#if GTA_REPLAY
	//Allow replay to override the internal state on playback
	friend class CPacketPlaneUpdate;
#endif 

	// External systems don't need to know about internal states
	// Keep in sync with commands_vehicle.sch!
	enum eLandingGearPublicStates
	{
		STATE_LOCKED_DOWN,
		STATE_RETRACTING,
		STATE_RETRACTING_INSTANT,
		STATE_DEPLOYING,
		STATE_LOCKED_UP,
		STATE_BROKEN,
		NUM_PUBLIC_STATES
	};
	

	// Keep in sync with commands_vehicle.sch!
	enum eCommands
	{
		COMMAND_DEPLOY,
		COMMAND_RETRACT,
		COMMAND_DEPLOY_INSTANT,
		COMMAND_RETRACT_INSTANT,
		COMMAND_BREAK,
		NUM_COMMANDS
	};

	enum eLandingGearParts
	{
		Part_Front,
		Part_MiddleR,
		Part_MiddleL,
		Part_RearR,
		Part_RearL,
		Part_RearM,
		MAX_NUM_PARTS
	};

	CLandingGear();
	~CLandingGear();

	//////////////////////////////////////////////////////////////////////////
	// Control functions
	void ControlLandingGear(CVehicle* pVehicle, eCommands nCommand, bool bNetCheck = true);

	//////////////////////////////////////////////////////////////////////////
	// Handling adjustment queries
	float GetLiftMult(CVehicle* pParent) const;
	float GetDragMult(CVehicle* pParent) const;

	//////////////////////////////////////////////////////////////////////////
	// CVehicle hooks
	// These functions need hooking up to vehicle process loop
	void InitPhys(CVehicle* pParent, eHierarchyId nFrontId, eHierarchyId nMiddleRId, eHierarchyId nMiddleLId, eHierarchyId nRearRId, eHierarchyId nRearLId, eHierarchyId nRearMId);

	void InitCompositeBound(CVehicle* pParent);
	void InitWheels(CVehicle *pParent);

	void ProcessControl(CVehicle* pParent);
	void PreRenderDoors(CVehicle* pParent);
	void PreRenderWheels(CVehicle* pParent);
	void ProcessPhysics(CVehicle* pParent);

	bool WantsToBeAwake(CVehicle* pParent);

	void Fix(CVehicle* pParent);		

	eLandingGearPublicStates GetPublicState() const;
	void SetFromPublicState(CVehicle* pParent, const u32 landingGearPublicState);

	float GetGearDeployRatio() const { return m_fGearDeployRatio; }
	int GetNumGearParts() const {return m_iNumGearParts;}
	
#if !__FINAL
	const char* GetDebugStateName() const;
#endif

#if __BANK
	static void InitWidgets(bkBank& bank);
	static void SendCommandCB();
	static bool ms_bDisableLandingGear;
	static eCommands ms_nCommand;
#endif
	
	static u32 ms_deployingRumbleDuration;
	static float ms_deployingRumbleIntensity;
	static u32 ms_deployedRumbleDuration;
	static float ms_deployedRumbleIntensity;

	static u32 ms_retractingRumbleDuration;
	static float ms_retractingRumbleIntensity;
	static u32 ms_retractedRumbleDuration;
	static float ms_retractedRumbleIntensity;

	void ApplyDeformation(CVehicle* pParentVehicle, const void *basePtr);
	bool GetWheelDeformation(const CVehicle *pParentVehicle, const CWheel *pWheel, Vector3 &vDeformation) const;
	float GetAuxDoorOpenRatio() const {return m_fAuxDoorOpenRatio;}
	void ProcessPreComputeImpacts(const CVehicle *pParentVehicle, phContactIterator impacts);

protected:

	// These functions are for internal use only
	// External systems should interface through ControlLandingGear
	void DeployGear(CVehicle* pParent);
	void RetractGear(CVehicle* pParent);
	void DeployGearInstant(CVehicle* pParent);
	void RetractGearInstant(CVehicle* pParent);
	void BreakGear(CVehicle* pParent);
	
	void SetState(eLandingGearState nNewState) { m_nPreviousState = m_nState; m_nState = nNewState; }
	eLandingGearState GetInternalState() const { return m_nState; }

	CLandingGearPartBase* m_pLandingGearParts[MAX_NUM_PARTS];
	s32 m_iNumGearParts;

	// These doors are non physical, they are animated by rotating bones to match the physically driven ones
	// They are not attached directly to the physically driven ones because they rotate aroudn different axis.
	// 0 = closed
	// 1 = open
	float m_fAuxDoorOpenRatio;

	// Cache how far the gear has deployed
	// 0 = fully retracted
	// 1 = down
	float m_fGearDeployRatio;

	eLandingGearState m_nState;
	eLandingGearState m_nPreviousState;

	// If an event has caused a change in state need to force a skeleton update
	bool m_bForceSkeletonUpdateThisFrame;

	// Set to true if instant retract is called on the same frame the gears are init'd
	bool m_bLandingGearRetractAfterInit;

	int m_iAuxDoorComponentFL;
	int m_iAuxDoorComponentFR;

	Vector3 vFrontLeftDoorDeformation;
	Vector3 vFrontRightDoorDeformation;

	// Static members
	static bank_float ms_fFrontDoorSpeed;

};

#endif // INC_LANDING_GEAR_H
