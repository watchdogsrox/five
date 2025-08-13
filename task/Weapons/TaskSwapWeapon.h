#ifndef TASK_SWAP_WEAPON_H
#define TASK_SWAP_WEAPON_H

// Game headers
#include "Peds/Ped.h"
#include "Peds/QueriableInterface.h"
#include "Task/System/TaskFSMClone.h"
#include "Task/System/TaskHelpers.h"
#include "Task/System/Tuning.h"
#include "Weapons/Inventory/PedInventory.h"

//////////////////////////////////////////////////////////////////////////
// eSwapFlags
//////////////////////////////////////////////////////////////////////////

enum eSwapFlags
{
	// Config flags
	SWAP_HOLSTER			= BIT(1),
	SWAP_DRAW				= BIT(2),
	SWAP_TO_AIM				= BIT(3),
	SWAP_FROM_AIM			= BIT(4),
	SWAP_INSTANTLY			= BIT(5),
	SWAP_SECONDARY			= BIT(6),

	// Abort config flags
	SWAP_ABORT_SET_DRAWN	= BIT(7),

	// this must come after all the config flags
	SWAP_CONFIG_FLAGS_END	= BIT(8),

	// Runtime flags
	SWAP_WEAPON_HOLSTERING	= BIT(9),
	SWAP_WEAPON_THROWING	= BIT(10),
	SWAP_WEAPON_DRAWING		= BIT(11),
};

//////////////////////////////////////////////////////////////////////////
// CTaskSwapWeapon
//////////////////////////////////////////////////////////////////////////

class CTaskSwapWeapon : public CTaskFSMClone
{
public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE:	Task tuning parameter struct.
	struct Tunables : public CTuning
	{
		Tunables();

		float m_OnFootClipRate;
		float m_OnFootBlendInDuration;
		float m_LowCoverClipRate;
		float m_LowCoverBlendInDuration;
		float m_HighCoverClipRate;
		float m_HighCoverBlendInDuration;
		float m_ActionClipRate;
		float m_ActionBlendInDuration;
		float m_BlendOutDuration;
		float m_SwimmingClipRate;
		bool m_DebugSwapInstantly;
		bool m_SkipHolsterWeapon;

		PAR_PARSABLE;
	};

	// PURPOSE: Instance of tunable task params
	static Tunables	ms_Tunables;	

	////////////////////////////////////////////////////////////////////////////////

	CTaskSwapWeapon(s32 iFlags);
	CTaskSwapWeapon(s32 iFlags, u32 cloneEquippedWeapon);

	// Clone used to produce an exact copy of task
	virtual aiTask* Copy() const { return rage_new CTaskSwapWeapon(m_iFlags); }

	// Returns the task Id. Each task class has an Id for quick identification
	virtual s32 GetTaskTypeInternal() const { return CTaskTypes::TASK_SWAP_WEAPON; }

	// Clone task implementation
	virtual CTaskInfo* CreateQueriableState() const;
	virtual FSM_Return UpdateClonedFSM(const s32 iState, const FSM_Event iEvent);

	virtual void GetOverriddenGameplayCameraSettings(tGameplayCameraSettings& settings) const;

	static void GenerateShockingEvent(CPed& ped, u32 lastWeaponDrawn);

	const fwFlags32& GetFlags() const { return m_iFlags; }

protected:

	enum State
	{
		State_Init = 0,
		State_WaitForLoad,
		State_WaitToHolster,
		State_Holster,
		State_WaitToDraw,
		State_Draw,
		State_Quit,
	};

	virtual FSM_Return ProcessPreFSM();
	virtual FSM_Return UpdateFSM(const s32 iState, const FSM_Event iEvent);

	virtual	void CleanUp();
	virtual bool ShouldAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual void DoAbort(const AbortPriority iPriority, const aiEvent* pEvent);
	virtual s32 GetDefaultStateAfterAbort() const { return State_Quit; }

#if FPS_MODE_SUPPORTED
	virtual bool ProcessPostCamera();
#endif // FPS_MODE_SUPPORTED
	virtual bool ProcessMoveSignals();

#if !__FINAL
	// Inherited tasks must provide an accessor to a state name string given a state for debugging
	friend class CTaskClassInfoManager;
	static const char* GetStaticStateName(s32 iState);
#endif // !__FINAL

private:

	//
	// State functions
	//

	FSM_Return	StateInit_OnEnter();
	FSM_Return	StateInit_OnUpdate();
	FSM_Return	StateWaitForLoad_OnUpdate();
	FSM_Return	StateWaitToHolster_OnUpdate();
	FSM_Return	StateHolster_OnEnter();
	FSM_Return	StateHolster_OnUpdate();
	bool		StateHolster_OnProcessMoveSignals();
	FSM_Return	StateWaitToDraw_OnEnter();
	FSM_Return	StateWaitToDraw_OnUpdate();
	FSM_Return	StateDraw_OnEnter();
	FSM_Return	StateDraw_OnUpdate();
	bool		StateDraw_OnProcessMoveSignals();
	
	// Tell the inventory to instantiate the weapon object
	bool CreateWeapon();

	// Init the move network
	bool StartMoveNetwork(const CWeaponInfo* pWeaponInfo);

	// Hand pose?
	void SetGripClip(CPed* pPed, const CWeaponInfo* pWeaponInfo);

	// How fast should we play the clip
	float GetClipPlayBackRate(const CWeaponInfo* pWeaponInfo) const;

	// Query which filter to use
	fwMvFilterId GetDefaultFilterId(const CWeaponInfo* pWeaponInfo) const;

	// Store the filter
	void SetFilter(const fwMvFilterId& filterId);

	// Should we draw a weapon?
	bool GetShouldDrawWeapon() const { return m_iFlags.IsFlagSet(SWAP_DRAW); }

	// Should we holster our equipped weapon?
	bool GetShouldHolsterWeapon() const { return m_iFlags.IsFlagSet(SWAP_HOLSTER); }

	// Should we skip clips and just equip the weapon
	bool GetShouldSwapInstantly(const CWeaponInfo* pWeaponInfo) const;

	// Are our animations streamed?
	bool GetIsAnimationStreamed(const u32 uWeaponHash, const fwMvClipSetId& clipSetIdOverride = CLIP_SET_ID_INVALID) const;

	// Get the clip set id we want to use
	fwMvClipSetId GetClipSet(u32 uWeaponHash) const;

	// Get the weapon info the weapon we are holstering
	const CWeaponInfo* GetHolsteringWeaponInfo() const;

	// Get the weapon info the weapon we are drawing
	const CWeaponInfo* GetDrawingWeaponInfo() const;

	// Get the clip id for the ped and state
	fwMvClipId GetSwapWeaponClipId(const CWeaponInfo* pWeaponInfo) const;

	// Get the clip id for the weapon
	fwMvClipId GetSwapWeaponClipIdForWeapon(const CWeaponInfo* pWeaponInfo) const;

	// 
	bool ShouldUseActionModeSwaps(const CPed& ped, const CWeaponInfo& weaponInfo) const;

	// Get the weapon type being drawn (used by the clone task). Also gets the task sequence id of the task info for this task
	u32 GetCloneDrawingWeapon() const;

	//
	// Members
	//

	// Move network helper
	CMoveNetworkHelper m_MoveNetworkHelper;

	// Filter
	crFrameFilterMultiWeight* m_pFilter;
	fwMvFilterId m_Filter;

	// Drawing clip/clipset
	fwMvClipSetId m_DrawClipSetId;
	fwMvClipId m_DrawClipId;

	// Flags
	fwFlags32 m_iFlags;

	// Weapon we are drawing
	u32 m_uDrawingWeapon;

	// Used by clone task:
	u32 m_uInitialDrawingWeapon;

	// The current phase
	float m_fPhase;

	bool m_bMoveBlendOut : 1;
	bool m_bMoveBlendOutLeftArm : 1;
	bool m_bMoveObjectDestroy : 1;
	bool m_bMoveObjectRelease : 1;
	bool m_bMoveInterruptToAim : 1;
	bool m_bMoveObjectCreate : 1;

	bool m_bFPSwappingToOrFromUnarmed : 1;
	bool m_bFPSSwappingFromProjectile : 1;

	//
	// Statics
	//

	// Clips
	static fwMvClipId ms_HolsterClipId;
	static fwMvClipId ms_UnholsterClipId;

	// Filters
	static fwMvFilterId ms_BothArmsNoSpineFilterId;
	static fwMvFilterId ms_RightArmNoSpineFilterId;

	// Move signals
	static const fwMvBooleanId ms_BlendOutId;
	static const fwMvBooleanId ms_ObjectCreateId;
	static const fwMvBooleanId ms_ObjectDestroyId;
	static const fwMvBooleanId ms_ObjectReleaseId;
	static const fwMvBooleanId ms_InterruptToAimId;
	static const fwMvBooleanId ms_BlendOutLeftArmId;
	static const fwMvClipId ms_ClipId;
	static const fwMvClipId ms_GripClipId;
	static const fwMvFilterId ms_FilterId;
	static const fwMvFlagId	ms_UseGripClipId;
	static const fwMvFloatId ms_PhaseId;
	static const fwMvFloatId ms_RateId;
};

//-------------------------------------------------------------------------
// Task info for CTaskSwapWeapon
//-------------------------------------------------------------------------
class CClonedSwapWeaponInfo : public CSerialisedFSMTaskInfo
{
public:
	CClonedSwapWeaponInfo(u32 drawingWeapon, u32 iFlags) : m_drawingWeapon(drawingWeapon), m_iFlags(iFlags) {}
	CClonedSwapWeaponInfo() : m_drawingWeapon(0), m_iFlags(0) {}
	~CClonedSwapWeaponInfo() {}

	virtual s32 GetTaskInfoType( ) const {return INFO_TYPE_SWAP_WEAPON;}

	virtual CTaskFSMClone *CreateCloneFSMTask();
	virtual CTask         *CreateLocalTask(fwEntity* UNUSED_PARAM(pEntity));

	u32 GetDrawingWeapon() const { return m_drawingWeapon; }

	void Serialise(CSyncDataBase& serialiser)
	{
		CSerialisedFSMTaskInfo::Serialise(serialiser);

		SERIALISE_UNSIGNED(serialiser, m_drawingWeapon, SIZEOF_DRAWING_WEAPON, "Drawing weapon");
		SERIALISE_UNSIGNED(serialiser, m_iFlags, SIZEOF_FLAGS, "Flags");
	}

protected:

	CClonedSwapWeaponInfo(const CClonedSwapWeaponInfo &);
	CClonedSwapWeaponInfo &operator=(const CClonedSwapWeaponInfo &);

	static const int SIZEOF_DRAWING_WEAPON = 32;
	static const int SIZEOF_FLAGS = datBitsNeeded<SWAP_CONFIG_FLAGS_END>::COUNT;

	u32 m_drawingWeapon;
	u32 m_iFlags;
};

#endif // TASK_SWAP_WEAPON_H
