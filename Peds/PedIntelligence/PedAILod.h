// FILE :    PedAILod.h
// PURPOSE : Controls the LOD of the peds AI update.
// CREATED : 30-09-2009

#ifndef PED_AILOD_H
#define PED_AILOD_H

// Rage headers
#include "system/bit.h"

// Framework headers
#include "fwutil/Flags.h"

// Game headers
#include "Peds/PedIntelligence/PedAILodManager.h"

// Forward Definitions
namespace rage { class bkBank; }
class CPed;

// CPedAILod controls the allocation
class CPedAILod
{
	friend CPedAILodManager;
public:

	// Lodding flags - there are matching debug strings stored in ms_aLodFlagNames to update when adding/re-ordering flags
	enum AiLodFlags
	{ 
		AL_LodEventScanning					= BIT0,		// Core AI event scanning
		AL_LodMotionTask					= BIT1,		// Full ped motion task, if this isn't active the ped drops down to a simple ped motion task
		AL_LodPhysics						= BIT2,		// The ped moves around using physical application of forces, with this disabled they are moved on the navmesh surface
		AL_LodNavigation					= BIT3,		// This will be used to drop the quality of navmesh paths, making navigation requests less expensive
		AL_LodEntityScanning				= BIT4,		// Perform entity scanning
		AL_LodRagdollInVehicle				= BIT5,		// The ped can run nm attached to vehicle task if the seat allows them to
		AL_LodTimesliceIntelligenceUpdate	= BIT6,		// Timeslice updates of tasks and related processing
		AL_LodTimesliceAnimUpdate			= BIT7,		// Timeslice animation updates
		// Note: update kNumAiLodFlags below if you add something.

		AL_DefaultLodFlags					= 0,

		AL_DefaultBlockedLodFlags			= AL_LodMotionTask|AL_LodPhysics|AL_LodTimesliceIntelligenceUpdate,
		AL_DefaultMissionBlockedLodFlags	= AL_DefaultBlockedLodFlags|AL_LodEventScanning,
		AL_DefaultRandomBlockedLodFlags		= AL_DefaultBlockedLodFlags,
		AL_DefaultActiveLawBlockedLodFlags	= AL_DefaultBlockedLodFlags|AL_LodEventScanning,
	};
	static const int kNumAiLodFlags = 8;	// Number of AiLodFlags bits (AL_LodEventScanning...AL_LodTimesliceIntelligenceUpdate).

	// How long to wait between repeat safe to switch checks
	static const int kRepeatSafeToSwitchCheckDelayMS = 250; // milliseconds

	static const u8 kMaxSkipCount = 7; // max value of bitwidth: 3 (see m_NavmeshTrackerUpdate_OnScreenSkipCount, m_NavmeshTrackerUpdate_OffScreenSkipCount)

	CPedAILod();
	~CPedAILod();

	void		ResetAllLodFlags();

	// Sets/Gets a new set of LOD flags
	bool		IsLodFlagSet(const s32 iFlag) const		{ return m_LodFlags.IsFlagSet(iFlag); }

	// Disable a lod state - forcing lod state will override this. These flags will be reset every frame
	bool		IsBlockedFlagSet(const s32 iFlag) const	{ return m_BlockedLodFlags.IsFlagSet(iFlag); }
	void		SetBlockedLodFlags(const s32 val)			{ m_BlockedLodFlags.SetAllFlags(val); }
	void		SetBlockedLodFlag(const s32 iFlag)		{ m_BlockedLodFlags.SetFlag(iFlag); }
	void		ClearBlockedLodFlag(const s32 iFlag)		{ m_BlockedLodFlags.ClearFlag(iFlag); }

	// Force a lod state - these flags will be reset every frame
	void		SetForcedLodFlags(const s32 val)			{ m_ForcedLodFlags.SetAllFlags(val); }
	void		SetForcedLodFlag(const s32 iFlag)			{ m_ForcedLodFlags.SetFlag(iFlag); }

	void		SetBlockMotionLodChangeTimer(float fTime)	{ m_fBlockMotionLodChangeTimer = fTime; }
	float		GetBlockMotionLodChangeTimer() const		{ return m_fBlockMotionLodChangeTimer; }

	// PURPOSE:	If set to true, prevent a timesliced update on the next frame.
	void		SetForceNoTimesliceIntelligenceUpdate(bool b)	{ m_bForceNoTimesliceIntelligenceUpdate = b; }

	// PURPOSE:	Check if timesliced updates have been prevented for the next frame.
	bool		GetForceNoTimesliceIntelligenceUpdate() const	{ return m_bForceNoTimesliceIntelligenceUpdate; }

	// PURPOSE:	Specify that we should bypass the distance/count check and let the ped go to low AI timeslicing LOD.
	void		SetUnconditionalTimesliceIntelligenceUpdate(bool b)	{ m_bUnconditionalTimesliceIntelligenceUpdate = b; }

	// PURPOSE:	Check if we should bypass the distance/count check and let the ped go to low AI timeslicing LOD.
	bool		GetUnconditionalTimesliceIntelligenceUpdate() const	{ return m_bUnconditionalTimesliceIntelligenceUpdate; }

	// PURPOSE:	If set to true, prevent a timesliced update on the next frame due to external task assignment.
	void		SetTaskSetNeedIntelligenceUpdate(bool b)	{ m_bTaskSetNeedIntelligenceUpdate = b; }

	// PURPOSE:	Check if timesliced updates have been prevented for the next frame due to external task assignment.
	bool		GetTaskSetNeedIntelligenceUpdate() const	{ return m_bTaskSetNeedIntelligenceUpdate; }

	// PURPOSE: Set the on screen skip counter for navmesh tracker update skipping
	void		SetNavmeshTrackerUpdate_OnScreenSkipCount(u8 skipCount) { if(AssertVerify(skipCount <= kMaxSkipCount)) { m_NavmeshTrackerUpdate_OnScreenSkipCount = skipCount; } }

	// PURPOSE: Get the on screen skip counter for navmesh tracker update skipping
	u8			GetNavmeshTrackerUpdate_OnScreenSkipCount() const { return m_NavmeshTrackerUpdate_OnScreenSkipCount; }

	// PURPOSE: Set the off screen skip counter for navmesh tracker update skipping
	void		SetNavmeshTrackerUpdate_OffScreenSkipCount(u8 skipCount) { if(AssertVerify(skipCount <= kMaxSkipCount)) { m_NavmeshTrackerUpdate_OffScreenSkipCount = skipCount; } }

	// PURPOSE: Get the off screen skip counter for navmesh tracker update skipping
	u8			GetNavmeshTrackerUpdate_OffScreenSkipCount() const { return m_NavmeshTrackerUpdate_OffScreenSkipCount; }

	// Process a change in the flags
	void		ProcessLodChange(const s32 iOldFlags, const s32 iNewFlags);

	// Check for ped lodding exclusions - returning true means the ped can't be switched from their current state
	bool		CanChangeLod() const;

	// Detect whether ped in intersecting any nearby objects or vehicles
	bool		IsSafeToSwitchToFullPhysics();

	// Set the ped AI's owner
	void		SetOwner(CPed* pOwner) { m_pOwner = pOwner; }
	
	void		ClearLowLodPhysicsFlag()				{ SetLodFlags(m_LodFlags&~AL_LodPhysics); }

	// PURPOSE:	Check if this ped is supposed to update animations on this frame.
	bool		ShouldUpdateAnimsThisFrame() const		{ return m_AnimUpdatePhase == 0; }

	// PURPOSE:	If we update animations on this frame, get the time step we should use.
	float		GetUpdateAnimsTimeStep() const			{ Assert(ShouldUpdateAnimsThisFrame()); return m_fTimeSinceLastAnimUpdate; }

	// PURPOSE:	If using animation timeslicing, get the current animation update period, in frames.
	int			GetAnimUpdatePeriod() const				{ return m_AnimUpdatePeriod; }

	u8			GetAiScoreOrder() const					{ return m_AiScoreOrder; }

	bool		MaySkipVisiblePositionUpdatesInLowLod() const { return m_bMaySkipVisiblePositionUpdatesInLowLod; }

#if __BANK
	// Debug rendering for this lod info
	void		DebugRender() const;

	// Debug widget setup
	static void	AddDebugWidgets(bkBank& bank);
#endif // __BANK

private:

	void		ClearForcedLodFlag(const s32 iFlag)		{ m_ForcedLodFlags.ClearFlag(iFlag); }
	bool		IsForcedFlagSet(const s32 iFlag) const	{ return m_ForcedLodFlags.IsFlagSet(iFlag); }

	// Only settable via the lod manager
	void		SetLodFlags(const s32 iFlags);
	void		SetLodFlag(const s32 iFlag)				{ SetLodFlags(m_LodFlags|iFlag); }
	void		ClearLodFlag(const s32 iFlag)				{ SetLodFlags(m_LodFlags&~iFlag); }

#if __BANK
	static void	ToggleFocusPedDebugFlag();
	static void ClearFocusPedDebugFlag();
	static void	SetAllPedsToLowLodDebugFlag();
	static void	SetAllPedsToHighLodDebugFlag();
	static void ClearAllPedsDebugFlags();
#endif // __BANK

	CPed* m_pOwner;						// Intentionally not regreffed - this class is a member of a ped
	fwFlags32 m_LodFlags;				// Stores the current set of LOD flags
	fwFlags32 m_BlockedLodFlags;		// Stores a set of blocked lod flags, reset each frame
	fwFlags32 m_ForcedLodFlags;			// Stores a set of forced lod flags, set until disabled

	// PURPOSE: Block the motion task changing lod state within this time
	float m_fBlockMotionLodChangeTimer;

	// PURPOSE:	Time since we last updated animations for this ped, in seconds, used as a time step for animation timeslicing.
	float	m_fTimeSinceLastAnimUpdate;

	// PURPOSE: The next time after which we may repeat the checks for safe switch to full physics, for performance.
	u32	m_uNextTimeToCheckSwitchToFullPhysicsMS; // milliseconds

	// PURPOSE:	Ai score order for this ped.
	u8		m_AiScoreOrder;

	// PURPOSE:	For timesliced animation updates, this is the current period assigned to this ped, in frames.
	u8		m_AnimUpdatePeriod : 6;

	// PURPOSE:	True if this ped is allowed to skip position updates, when visible and in low LOD.
	// NOTES:	Possibly, this could be made into its own LOD bit instead, AL_LodPosUpdates or something.
	u8		m_bMaySkipVisiblePositionUpdatesInLowLod : 1;

	// PURPOSE: If set, prevent a timesliced update on the next frame, even if the AL_LodTimesliceIntelligenceUpdate blocking flag is not set.
	// This flag is cleared at the end of the ped's task update, as we want to update immediately following task assignments,
	// but not if the assignment was made as part of the peds task update itself.
	u8		m_bTaskSetNeedIntelligenceUpdate : 1;

	// PURPOSE:	For timesliced animation updates, this is the number of frames until the next time we get to update.
	//			If 0, we are updating on this frame.
	u8		m_AnimUpdatePhase;

	// PURPOSE: Keep track of the number of skipped updates while on screen
	u8	m_NavmeshTrackerUpdate_OnScreenSkipCount : 3;

	// PURPOSE: Keep track of the number of skipped updates while off screen
	u8	m_NavmeshTrackerUpdate_OffScreenSkipCount : 3;
	
	// PURPOSE:	If set, prevent a timesliced update on the next frame, even if the AL_LodTimesliceIntelligenceUpdate blocking flag is not set.
	u8	m_bForceNoTimesliceIntelligenceUpdate : 1;

	// PURPOSE:	If set, this ped doesn't need to pass a distance check in order to switch to low AI timeslicing LOD.
	u8	m_bUnconditionalTimesliceIntelligenceUpdate : 1;

#if __BANK
	fwFlags32 m_DebugLowLodFlags;		// Override default lod flags
	fwFlags32 m_DebugHighLodFlags;		// Override default lod flags
#endif // __BANK

#if __BANK
	// Debug strings for flags
	static const s32 ms_iMaxFlagNames = 32;
	static const char* ms_aLodFlagNames[ms_iMaxFlagNames];

	// Rag widget combo index
	static s32 ms_iDebugFlagIndex;
#endif // __BANK
};

////////////////////////////////////////////////////////////////////////////////

inline void	CPedAILod::SetLodFlags(const s32 iFlags)
{
	s32 iNewFlags = iFlags;
	iNewFlags |= m_ForcedLodFlags;
#if __BANK
	iNewFlags &= ~m_DebugHighLodFlags;
	iNewFlags |=  m_DebugLowLodFlags;
#endif // __BANK
	s32 iOldFlags = m_LodFlags;
	m_LodFlags.SetAllFlags(iNewFlags);
	ProcessLodChange(iOldFlags, iNewFlags);
}

#endif //PED_AILOD_H
