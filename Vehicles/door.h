// Title	:	Door.h
// Author	:	Bill Henderson
// Started	:	06/01/2000

#ifndef _DOOR_H_
#define _DOOR_H_

// std headers
#include <string.h>	// for memset

// rage headers
#include "fragment/instance.h"
#include "vector/vector3.h"
#include "pharticulated/articulatedcollider.h"
#include "fwsys/timer.h"

// game headers
#include "control/replay/replaysettings.h"
#include "debug/debug.h"
#include "Pathserver/PathServer_DataTypes.h"
#include "renderer/HierarchyIds.h"
#include "vehicles/VehicleDefines.h"

class CVehicle;

enum eScriptDoorList
{
	SC_DOOR_INVALID = -1,
	SC_DOOR_FRONT_LEFT = 0,
	SC_DOOR_FRONT_RIGHT,
	SC_DOOR_REAR_LEFT,
	SC_DOOR_REAR_RIGHT,
	SC_DOOR_BONNET,
	SC_DOOR_BOOT,
	SC_DOOR_EXTRA,
	SC_DOOR_NUM
};


enum eScriptWindowList
{
	SC_WINDOW_FRONT_LEFT = 0,
	SC_WINDOW_FRONT_RIGHT,
	SC_WINDOW_REAR_LEFT,
	SC_WINDOW_REAR_RIGHT,
	SC_WINDOW_MIDDLE_LEFT,
	SC_WINDOW_MIDDLE_RIGHT,
	SC_WINDSCREEN_FRONT,
	SC_WINDSCREEN_REAR,
	SC_WINDOW_NUM
};


class CCarDoor
{
public:
	enum eDoorFlags
	{
		AXIS_X = BIT(0),
		AXIS_Y = BIT(1),
		AXIS_Z = BIT(2),
		AXIS_SLIDE_X = BIT(3),
		AXIS_SLIDE_Y = BIT(4),
		AXIS_SLIDE_Z = BIT(5),
		AXIS_MASK = AXIS_X | AXIS_Y | AXIS_Z | AXIS_SLIDE_X | AXIS_SLIDE_Y | AXIS_SLIDE_Z,

		DRIVEN_NORESET_NETWORK	  = BIT(7),
		DRIVEN_SHUT				  = BIT(8),
		DRIVEN_AUTORESET		  = BIT(9),
		DRIVEN_NORESET			  = BIT(10),
		DRIVEN_GAS_STRUT		  = BIT(11),
		DRIVEN_SWINGING			  = BIT(12),
		DRIVEN_SMOOTH			  = BIT(13),
		DRIVEN_SPECIAL			  = BIT(14), // Used to tag rear vehicle doors that open only under special conditions (blown open or seat jack)
		DRIVEN_USE_STIFFNESS_MULT = BIT(15),
		DRIVEN_MASK				  = DRIVEN_NORESET_NETWORK | DRIVEN_SHUT | DRIVEN_AUTORESET | DRIVEN_NORESET | DRIVEN_GAS_STRUT | DRIVEN_SWINGING | DRIVEN_SMOOTH | DRIVEN_SPECIAL | DRIVEN_USE_STIFFNESS_MULT,
		DRIVEN_NOT_SPECIAL_MASK	  = DRIVEN_NORESET_NETWORK | DRIVEN_SHUT | DRIVEN_AUTORESET | DRIVEN_NORESET | DRIVEN_GAS_STRUT | DRIVEN_SWINGING |									 DRIVEN_USE_STIFFNESS_MULT,

		WILL_LOCK_SWINGING	= BIT(16), // Locks if the door swings shut at sufficient speed
		WILL_LOCK_DRIVEN	= BIT(17), // Locks if the door is successfully driven to 0 
		IS_ARTICULATED		= BIT(18),
		IS_BROKEN_OFF		= BIT(19),
		BIKE_DOOR			= BIT(20),
		AERO_BONNET			= BIT(21),
		GAS_BOOT			= BIT(22),
		DONT_BREAK			= BIT(23),
		PROCESS_FORCE_AWAKE	= BIT(24),

		RELEASE_AFTER_DRIVEN		= BIT(25),
		SET_TO_FORCE_OPEN			= BIT(26),
		PED_DRIVING_DOOR			= BIT(27),
		PED_USING_DOOR_FOR_COVER	= BIT(28),
		LOOSE_LATCHED_DOOR			= BIT(29),
		DRIVEN_BY_PHYSICS			= BIT(30), // Used to indicate a door that should still be driven by physics even when the vehicle has animated joints
		RENDER_INVISIBLE			= BIT(31)  // Used, for example to have turret hatch doors hidden for better camera angles.
	};

#if __BANK
	static const char* GetDoorFlagString(eDoorFlags flag)
	{
		switch (flag)
		{
			case AXIS_X						: return "AXIS_X";
			case AXIS_SLIDE_X				: return "AXIS_SLIDE_X";
			case AXIS_SLIDE_Y				: return "AXIS_SLIDE_Y";
			case AXIS_SLIDE_Z				: return "AXIS_SLIDE_Z";
			case DRIVEN_NORESET_NETWORK		: return "DRIVEN_NORESET_NETWORK";
			case DRIVEN_SHUT				: return "DRIVEN_SHUT";
			case DRIVEN_AUTORESET			: return "DRIVEN_AUTORESET";
			case DRIVEN_NORESET				: return "DRIVEN_NORESET";
			case DRIVEN_GAS_STRUT			: return "DRIVEN_GAS_STRUT";
			case DRIVEN_SWINGING			: return "DRIVEN_SWINGING";
			case DRIVEN_SMOOTH				: return "DRIVEN_SMOOTH";
			case DRIVEN_SPECIAL				: return "DRIVEN_SPECIAL";
			case WILL_LOCK_SWINGING			: return "WILL_LOCK_SWINGING";
			case WILL_LOCK_DRIVEN			: return "WILL_LOCK_DRIVEN";
			case IS_ARTICULATED				: return "IS_ARTICULATED";
			case IS_BROKEN_OFF				: return "IS_BROKEN_OFF";
			case BIKE_DOOR					: return "BIKE_DOOR";
			case AERO_BONNET				: return "AERO_BONNET";
			case GAS_BOOT					: return "GAS_BOOT";
			case DONT_BREAK					: return "DONT_BREAK";
			case PROCESS_FORCE_AWAKE		: return "PROCESS_FORCE_AWAKE";
			case RELEASE_AFTER_DRIVEN		: return "RELEASE_AFTER_DRIVEN";
			case PED_DRIVING_DOOR			: return "PED_DRIVING_DOOR";
			case PED_USING_DOOR_FOR_COVER	: return "PED_USING_DOOR_FOR_COVER";
			case LOOSE_LATCHED_DOOR			: return "LOOSE_LATCHED_DOOR";
			case DRIVEN_BY_PHYSICS			: return "DRIVEN_BY_PHYSICS";
			default: break;
		}
		return "Invalid Flag";
	}

	void PrintDoorFlags( CVehicle* pParent );
#endif // __BANK

	CCarDoor();
	~CCarDoor();

	void Init(CVehicle *pParent, eHierarchyId nId, float fOpen, float fClosed, u32 nExtraFlags);
	void ApplyDeformation(CVehicle* pParentVehicle, const void* basePtr);

	void ProcessPhysics(CVehicle *pParent, float fTimeStep, int nTimeSlice);
	void ProcessPreRender2(CVehicle *pParent);
	void ProcessPreRender(CVehicle *pParent);

	void SetTargetDoorOpenRatio(float fRatio, u32 nFlags, CVehicle *pParent = NULL);

	eHierarchyId GetHierarchyId() const {return m_DoorId;} 
	s8 GetFragGroup() const {return m_nFragGroup;}
	s8 GetFragChild() const {return m_nFragChild;}

	float GetDoorRatio() const {return m_fCurrentRatio;}
	float GetTargetDoorRatio() const {return m_fTargetRatio;}
	bool GetIsClosed(float fConsiderClosed = 0.01f) const {return (m_fCurrentRatio<fConsiderClosed && GetIsIntact(NULL));}
	bool GetIsFullyOpen(float error = 0.01f) const {return (m_fCurrentRatio > (1.0f - error));}
	float GetDoorAngle() const {return (m_fCurrentRatio*m_fOpenAmount + (1.0f - m_fCurrentRatio)*m_fClosedAmount);}
	float GetCurrentSpeed() const {return m_fCurrentSpeed;}
	float GetOpenAmount() const { return m_fOpenAmount; }
	float GetClosedAmount() const { return m_fClosedAmount; }
	float GetOldRatio() const { return m_fOldRatio; }

#if GTA_REPLAY
	void SetDoorRatio(float ratio) { m_fCurrentRatio = ratio; }
	void SetDoorOldAudioRatio(float ratio) { m_fOldAudioRatio = ratio; }
	void ResetLastAudioTime() {	m_nLastAudioTime = 0;	}
	u32 GetFlags() const { return m_nDoorFlags; }
	void SetFlags(u32 flags);
	void ReplayUpdateDoors(CVehicle* pParent);
#endif // GTA_REPLAY

	bool GetFlag(u32 nFlag) const {return (m_nDoorFlags & nFlag) != 0;}
	u32 MaskFlag(u32 nFlag) const { return (m_nDoorFlags & nFlag); }
	void SetFlag(u32 nFlag);
	void ClearFlag(u32 nFlag);
	int GetAxis() const {return (m_nDoorFlags &AXIS_MASK);}

	bool GetLocalMatrix(const CVehicle * pParent, Matrix34 & mat, const bool bForceFullyOpen=false) const;
	bool GetLocalBoundingBox(const CVehicle * pParent, Vector3 & vMinOut, Vector3 & vMaxOut) const;

	void SetSwingingFree(CVehicle* pParent);
	void SetShutAndLatched(CVehicle* pParent, bool bApplyForceForDoorClosed = true);
	void SetOpenAndUnlatched(CVehicle* pParent, u32 flags);
	void SetLooseLatch(CVehicle* pParent);

	bool RecentlySetShutAndLatched(const unsigned delta);
	bool RecentlyBeingUsed(const unsigned delta);

	bool GetIsIntact(const CVehicle* pParent) const;
	// Returns the fraginst for the door broken off
	fragInst* BreakOff(CVehicle* pParent, bool bNetEvent = true);
	void BreakOffNextUpdate() 
	{ 
		m_bBreakOffNextUpdate = true; 
	}
	bool GetBreakOffNextUpdate() { return m_bBreakOffNextUpdate; }
	void Fix(CVehicle* pParent);

	// special bike use of doors
	void SetBikeDoorUnConstrained(float fCurrentAngle);
	void SetBikeDoorConstrained();
	void SetBikeDoorOpenAmount(float fOpenAmount) { m_fOpenAmount = fOpenAmount; }
	void SetBikeDoorClosedAmount(float fClosedAmount) { m_fClosedAmount = fClosedAmount; }
	void SetHasBeenKnockedOpen(bool bKnockedOpen) { m_bDoorHasBeenKnockedOpen = bKnockedOpen; }
	void AudioUpdate();
	void TriggeredAudio(const u32 timeInMs);
	u32 GetLastAudioTime() const {return m_nLastAudioTime;}
	f32 GetOldAudioRatio() const {return m_fOldAudioRatio;}
	bool GetJustLatched() const  {return m_bJustLatched;}
	bool GetIsLatched(CVehicle* pParent);
	bool GetEverKnockedOpen() const {return m_bDoorHasBeenKnockedOpen;}

    void SetDoorAllowedToBeBrokenOff(bool doorAllowedToBeBrokenOff, CVehicle* pParent);
    bool GetDoorAllowedToBeBrokenOff() { return m_bDoorAllowedToBeBrokenOff && !GetFlag( DONT_BREAK ); }

	void BreakLatch(CVehicle* pParent);
	
	void UnlatchAnimatedDoors(CVehicle* pParent);   

	void SetForceOpenForNavigationInfinite(bool b) { if(b) m_bForceOpenForNavigation = 4; else m_bForceOpenForNavigation = 0; }
	void SetForceOpenForNavigation(bool b) { if(b) m_bForceOpenForNavigation = 3; else m_bForceOpenForNavigation = 0; }
	bool GetForceOpenForNavigation() const { return (m_bForceOpenForNavigation!=0); }
	bool GetDoesPlayerStandOnDoor() const {return m_bPlayerStandsOnDoor;}

	bool IsNetworkDoorOpenAllowed();

	s8 GetBoneIndex() const { return m_nBoneIndex; }

	const bool GetPedRemoteUsingDoor() const { return m_pedRemoteUsingDoor; }
	void SetPedRemoteUsingDoor(bool value) { m_pedRemoteUsingDoor = value; }
	void SetRecentlyBeingUsed(){ m_uTimeSinceBeingUsed = fwTimer::GetSystemTimeInMilliseconds();}

	void OverrideDoorTorque( bool overrideTorque ) { m_overrideDoorTorque = overrideTorque; }
 
#if __BANK
private:
	size_t	m_lastSetFlagsCallstack[32];
	char	m_lastSetFlagsScript[256];
	bool	m_drivenNoResetFlagWasSet;
public:
	static bool sm_debugDoorPhysics;

	void	OutputDoorStuckDebug();
#endif

private:
	void SetLatch(CVehicle* pParent);
	void DriveDoorOpen(float Angle, phJoint* joint, CVehicle* pParent, phArticulatedCollider* pArticulatedCollider);
	float ClampDoorOpenRatio(float ratio) const;
	float ComputeClampedRatio(float position) const;
	float ComputeRatio(float position) const;
	bool NeedsToAnimateOpen( CVehicle* pParent, bool bIgnoreCurrentRatio = false );

public:
	static eHierarchyId GetScriptDoorId(eScriptDoorList nScDoor) {Assert(nScDoor >= 0); Assert(nScDoor < SC_DOOR_NUM); return ms_aScriptDoorIds[nScDoor];}
	static eHierarchyId GetScriptWindowId(eScriptWindowList nScWindow) {Assert(nScWindow < SC_WINDOW_NUM); return ms_aScriptWindowIds[nScWindow];}

	inline TDynamicObjectIndex GetPathServerDynamicObjectIndex() const { return m_iPathServerDynObjId; }
	inline void SetPathServerDynamicObjectIndex(const TDynamicObjectIndex i) { m_iPathServerDynObjId = i; }

	const Vector3& GetDeformation() const { return m_vDeformation; }

	void SetLinkedDoor( eHierarchyId linkedDoor ) { m_linkedDoorId = linkedDoor; }

public:
	eCarLockState m_eDoorLockState;

private:

    s8 m_nBoneIndex;
	s8 m_nFragGroup;
	s8 m_nFragChild;
	u8 m_bJustLatched				:1;
	u8 m_bDoorAllowedToBeBrokenOff	:1;
	u8 m_bDoorRatioHasChanged		:1;
	u8 m_bDoorWindowIsRolledDown	:1;
	u8 m_bDoorHasBeenKnockedOpen	:1;
	u8 m_bForceOpenForNavigation	:3;
	u8 m_bPlayerStandsOnDoor		:1;
	u8 m_bBreakOffNextUpdate		:1;
	u8 m_bResetComponentToLinkIndex :1;
	u8 m_pedRemoteUsingDoor			:1;
	u8 m_overrideDoorTorque			:1;

	eHierarchyId m_DoorId : 16;
	eHierarchyId m_linkedDoorId;

	TDynamicObjectIndex m_iPathServerDynObjId;
	
	u32 m_nDoorFlags;
	
	float m_fOpenAmount;
	float m_fClosedAmount;
	float m_fTargetRatio;
	float m_fOldTargetRatio;
	float m_fCurrentRatio;
	float m_fOldRatio;
	float m_fCurrentSpeed;
	float m_fOldAudioRatio;
	u32 m_nLastAudioTime;
	u32 m_nNetworkTimeOpened;
	float m_fAnglePastLimits;
	u32 m_nPastLimitsTimeThreshold;
	u32 m_nOpenPastLimitsTime;

	u32 m_uTimeSetShutAndLatched;
	u32 m_uTimeSinceBeingUsed;

	Vector3 m_vDeformation;

	static eHierarchyId ms_aScriptDoorIds[SC_DOOR_NUM];
	static eHierarchyId ms_aScriptWindowIds[SC_WINDOW_NUM];
};



class CBouncingPanel
{
public:
	enum eBounceAxis {
		BOUNCE_AXIS_X = 0,
		BOUNCE_AXIS_NEG_X,
		BOUNCE_AXIS_Y,
		BOUNCE_AXIS_NEG_Y,
		BOUNCE_AXIS_Z,
		BOUNCE_AXIS_NEG_Z
	};

	s16 m_nComponentIndex;
	eBounceAxis m_nBounceAxis;
	
	float m_fBounceApplyMult;
	Vector3 m_vecBounceAngle;
	Vector3 m_vecBounceTurnSpeed;
	
	static float BOUNCE_SPRING_DAMP_MULT;
	static float BOUNCE_SPRING_RETURN_MULT;
	static float BOUNCE_ACCEL_LIMIT;
	
	static float BOUNCE_HANGING_DAMP_MULT;
	static float BOUNCE_HANGING_RETURN_MULT;

public:
	CBouncingPanel() {m_nComponentIndex = -1;}
	
	void ResetPanel();
	void SetPanel(s32 nIndex, eBounceAxis nAxis, float fMult);
	void ProcessPanel(CVehicle *pVehicle, const Vector3& vecOldMoveSpeed, const Vector3& vecOldTurnSpeed, float fReturnMult=-1.0f, float fDampMult=-1.0f);
	
	s16 GetCompIndex(){ return m_nComponentIndex; }
};


#endif
