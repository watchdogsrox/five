#ifndef _PED_FLAGS_H_
#define _PED_FLAGS_H_

// Rage Includes
#include "atl/bitset.h"

// Game includes
#include "Peds/PedFlagsMeta.h"
#include "Peds/PedMoveBlend/PedMoveBlendTypes.h" 

// Forward definitions
class CPed;


//
// Ped Config Flags
// Description:	Relatively static flags to configure peds abilities, options, and special settings
// Usage:		Mostly runtime flags maintained by ped intelligence and other systems
//
class CPedConfigFlags
{
public:
	void Init(CPed* pPed);
	void Reset(CPed* pPed);

public:
	
	inline void SetCantBeKnockedOffVehicle(unsigned int mode)					{ nCantBeKnockedOffVehicle = mode; }
	inline unsigned int GetCantBeKnockedOffVehicle() const						{ return nCantBeKnockedOffVehicle; }

	inline void SetPedLegIkMode(unsigned int mode)							{ nPedLegIkMode = mode; }
	inline unsigned int GetPedLegIkMode() const								{ return nPedLegIkMode; }

	inline void SetPedGestureMode(unsigned int mode)							{ nPedGestureMode = mode; }
	inline unsigned int GetPedGestureMode() const								{ return nPedGestureMode; }

	inline void SetPassengerIndexToUseInAGroup(int index)					{ m_iPassengerIndexToUseInAGroup = index; }
	inline int GetPassengerIndexToUseInAGroup() const						{ return m_iPassengerIndexToUseInAGroup; }

	inline void SetClimbRateOverride(const float f)							{ fClimbRateOverride = f; }
	inline float GetClimbRateOverride() const								{ return fClimbRateOverride; }


	inline void SetFlag(const ePedConfigFlags flag, const bool value)		{ m_Flags.BitSet().Set( flag, value ); }
	__forceinline bool GetFlag(const ePedConfigFlags flag) const			{ return m_Flags.BitSet().IsSet(flag); }

	inline bool AreConfigFlagsSet(const ePedConfigFlagsBitSet& configFlags) const { return configFlags.BitSet().IsSubsetOf(m_Flags.BitSet()); }

protected:
	// non single bit variables
	unsigned int	nCantBeKnockedOffVehicle :2;								// 0=Default(harder for mission peds) 1=Never 2=Easy 3=Hard
	unsigned int	nPedLegIkMode : 2;										//	Enable the leg ik on a non player, leg IK types defined in IkManager.h
	unsigned int	nPedGestureMode : 2;									//	What is the gesture mode normal, obey "allow" or "blocking" tags in anims

	int				m_iPassengerIndexToUseInAGroup : 5;						// If this ped is in a group, this is the seat they will use when entering vehicles

	float			fClimbRateOverride;										// Controls how fast the climb animations are played

	ePedConfigFlagsBitSet	m_Flags;										// rage::atFixedBitSet<CPED_CONFIG_FLAG_NUM_FLAGS, rage::u32>
};


//
// Ped Reset Flags
// Description:	Flags that will auto reset every frame
// Usage:		Mostly notifier flags that ai tasks can set to trigger code elsewhere
//
class CPedResetFlags
{
public:
	static void StaticInit();
	void Init(CPed* pPed);
	void Reset(CPed* pPed);
	void ResetPreAIPostInventory();					// pre ai,			post inventory
	void ResetPreTask();							// pre task,		post event handling
	void ResetPrePhysics(CPed* pPed);				// pre physics,		post proccontrol
	void ResetPostPhysics(CPed* pPed);				// post physics,	pre script
	void ResetPostMovement(CPed* pPed);				// post script,		pre prerender
	void ResetPostPreRender(CPed* pPed);			// post prerender,	pre render
	void ResetPostCamera(CPed* pPed);				// post prerender,	pre render

	inline void SetIsWalkingRoundPlayer()									{ SetFlag(CPED_RESET_FLAG_InternalWalkingRndPlayer, true); }

public:	
	inline void SetFlag(const ePedResetFlags flag, const bool value)		{ m_Flags.BitSet().Set( flag, value ); }
	inline bool GetFlag(const ePedResetFlags flag) const					{ return m_Flags.BitSet().IsSet(flag); }

	inline void SetKnockedByDoor(rage::u8 val)								{ m_nKnockedByDoor = val; }
	inline rage::u8 GetKnockedByDoor()	const								{ return m_nKnockedByDoor; }

	inline void SetEntityZFromGround(rage::u8 val)							{ m_nSetEntityZFromGround = val; }
	inline rage::u8 GetEntityZFromGround() const							{ return m_nSetEntityZFromGround; }

	inline void SetEntityZFromGroundZHeight(float fHeight, float fThreshold = 1.0f) { m_fEntityZFromGroundZHeight = fHeight; m_fEntityZFromGroundZThreshold = fThreshold; }
	inline float GetEntityZFromGroundZHeight() const						{ return m_fEntityZFromGroundZHeight; }
	inline float GetEntityZFromGroundZThreshold() const						{ return m_fEntityZFromGroundZThreshold; }

	inline void SetAnimRotationModifier(const float f)						{ m_fAnimRotationModifier = f; }
	inline float GetAnimRotationModifier() const							{ return m_fAnimRotationModifier; }

	inline void SetRootCorrectionModifer(const float f)						{ m_fRootCorrectionModifer = f; }
	inline float GetRootCorrectionModifer() const							{ return m_fRootCorrectionModifer; }

	inline void SetMoveAnimRate(const float f)								{ m_fMoveAnimRate = f; }
	inline float GetMoveAnimRate() const									{ return m_fMoveAnimRate; }

	inline void SetScriptedScaleBetweenSeatsDefaultDistance(const float f)	{ m_fScriptedScaleBetweenSeatsDefaultDistance = f; }
	inline float GetScriptedScaleBetweenSeatsDefaultDistance() const		{ return m_fScriptedScaleBetweenSeatsDefaultDistance; }

	inline void SetHeadIkBlocked()											{ m_nDontAcceptIKLookAts = 3; }
	inline bool GetHeadIkBlocked() const									{ return m_nDontAcceptIKLookAts>0; }

	inline void SetCodeHeadIkBlocked()										{ m_nDontAcceptCodeIKLookAts = 3; }
	inline bool GetCodeHeadIkBlocked() const								{ return m_nDontAcceptCodeIKLookAts>0; }

	inline void SetHasJustLeftVehicle(u32 iNumFramesToConsiderJustLeft)		{ m_nHasJustLeftVehicle = iNumFramesToConsiderJustLeft; }
	inline bool GetHasJustLeftVehicle() const								{ return m_nHasJustLeftVehicle>0; }

	inline void SetIsInCover(u32 iNumFramesToConsiderInCover)				{ m_nIsInCover = iNumFramesToConsiderInCover; }
	inline bool GetIsInCover() const										{ return m_nIsInCover>0; }
	

#if __DEV
	inline void SetForceUpdateCalledLastFrame(const unsigned int v)			{ iForceUpdateCalledLastFrame = v; }
	inline unsigned int GetForceUpdateCalledLastFrame() const				{ return iForceUpdateCalledLastFrame; }
#endif

private:
	u8				m_nKnockedByDoor;			// if ped is getting pushed out of the way by the player pushing a door
	u8				m_nSetEntityZFromGround;	// use the ground position to snap the height of the ped to the correct distance above the ground (don't want to do this all the time)

	// also have flags that count down to avoid issues with ordering of reseting and setting
	// so these flags should be set to 2 and then reset should count down to zero
	unsigned int	m_nDontAcceptIKLookAts :2;
	unsigned int	m_nDontAcceptCodeIKLookAts:2;
	unsigned int	m_nHasJustLeftVehicle:4;
	unsigned int	m_nIsInCover:2;

	// rotation modifier, used to allow tasks to override the amount of rotation applied from anims, reset each frame to 1
	float			m_fAnimRotationModifier;

	// root correction modifier, how much of the root correction is applied to the ped, reset to 1 each frame
	float			m_fRootCorrectionModifer; // Reset in applyroot

	// Overall control of speed at which movement anims play, default is 1.0f
	float			m_fMoveAnimRate;

	// The magnitude by which peds are pull back onto their routes when following navmesh or wandering.
	// This is reset to the default of ROUTE_PULL_FACTOR (0.75f) at the start of each frame.
	// float			m_fRoutePullFactor;

	// Modifier set each frame by script, sets a distance a ped should be between seats to be applied when in a vehicle
	float			m_fScriptedScaleBetweenSeatsDefaultDistance;

	//! EntityZ ground height. Used to prevent snapping z unless we are close to this height.
	float m_fEntityZFromGroundZHeight;
	float m_fEntityZFromGroundZThreshold;

#if __DEV
	unsigned int	iForceUpdateCalledLastFrame :2;
#endif

	// Bitset containing (most of) the flags
	//rage::atFixedBitSet<CPedResetFlags::NUM_FLAGS, rage::u32>			m_Flags;
	ePedResetFlagsBitSet												m_Flags;

	// Bitsets containing the flag masks determining which bits to clear in the reset functions.  Bits are 1 for all flags that should be cleared
	//static rage::atFixedBitSet<CPedResetFlags::NUM_FLAGS, rage::u32>	sm_ResetFlagMask;
	//static rage::atFixedBitSet<CPedResetFlags::NUM_FLAGS, rage::u32>	sm_PrePhysicsFlagMask;
	//static rage::atFixedBitSet<CPedResetFlags::NUM_FLAGS, rage::u32>	sm_PostPhysicsFlagMask;
	//static rage::atFixedBitSet<CPedResetFlags::NUM_FLAGS, rage::u32>	sm_PostMovementFlagMask;
	////static rage::atFixedBitSet<CPedResetFlags::NUM_FLAGS, rage::u32>	sm_PostPreRenderFlagMask;
	//static rage::atFixedBitSet<CPedResetFlags::NUM_FLAGS, rage::u32>	sm_PostCameraFlagMask;

	static ePedResetFlagsBitSet											sm_ResetFlagMask;
	static ePedResetFlagsBitSet											sm_PreAIPostInventoryResetFlagMask;
	static ePedResetFlagsBitSet											sm_PreTaskResetFlagMask;
	static ePedResetFlagsBitSet											sm_PrePhysicsFlagMask;
	static ePedResetFlagsBitSet											sm_PostPhysicsFlagMask;
	static ePedResetFlagsBitSet											sm_PostMovementFlagMask;
	static ePedResetFlagsBitSet											sm_PostCameraFlagMask;
	static ePedResetFlagsBitSet											sm_PostPreRenderFlagMask;
};

#endif	// #ifndef _PED_FLAGS_H_

