#ifndef MOVE_PED_H
#define MOVE_PED_H

#include "animation/move.h"
#include "task/System/TaskHelpers.h"

class CMovePed : public CMove 
{
public:

	enum MotionTaskSlotFlags {
		MotionTask_None = 0,
		MotionTask_TagSyncTransition = BIT0,
	};

	enum TaskSlotFlags {
		Task_None = 0,
		Task_TagSyncContinuous = BIT0,
		Task_TagSyncTransition = BIT1,
		Task_DontDisableParamUpdate = BIT2,
		Task_IgnoreMoverBlendRotation = BIT3,
		Task_IgnoreMoverBlend = BIT4
	};

	enum SecondarySlotFlags {
		Secondary_None = 0,
		Secondary_TagSyncContinuous = BIT0,
		Secondary_TagSyncTransition = BIT1,
	};

	enum BlendStatus {
		IsBlendingIn = BIT0,
		IsFullyBlended = BIT1,
		IsBlendingOut = BIT2,
		IsFullyBlendedOut = BIT3,
		IsSecondaryBlendingIn = BIT4,
		IsSecondaryFullyBlended = BIT5,
		IsSecondaryBlendingOut = BIT6,
		IsSecondaryFullyBlendedOut = BIT7,
		IsFacialBlendingIn = BIT8,
		IsFacialFullyBlended = BIT9,

		IsUsingStandardFacial = BIT10,
		IsVoiceDrivenMouthMovement = BIT11,

		TaskSlotMask = IsBlendingIn | IsFullyBlended | IsBlendingOut | IsFullyBlendedOut,
		SecondarySlotMask = IsSecondaryBlendingIn | IsSecondaryFullyBlended | IsSecondaryBlendingOut | IsSecondaryFullyBlendedOut,
		FacialSlotMask = IsFacialBlendingIn | IsFacialFullyBlended
	};

	enum ePedState
	{
		kStateAnimated,
		kStateStaticFrame,
		kStateRagdoll
	};

	// PURPOSE: Custom frame filter - blocks all except face bones, ANMs and control values
	class RagdollFilter : public crFrameFilterBoneBasic
	{
	public:

		// PURPOSE: Constructor
		RagdollFilter();

		// PURPOSE: Constructor
		RagdollFilter(datResource&);

		// PURPOSE: Filter type registration
		CR_DECLARE_FRAME_FILTER_TYPE(RagdollFilter);

		// PURPOSE: Filter DOF override
		virtual bool FilterDof(u8 track, u16 id, float& inoutWeight);

	protected:

		// PURPOSE: Signature calculation
		virtual u32 CalcSignature() const;
	};

	// PURPOSE: Custom frame filter - blocks all except face bones
	class StaticFrameFilter : public crFrameFilter
	{
	public:

		// PURPOSE: Constructor
		StaticFrameFilter();

		// PURPOSE: Constructor
		StaticFrameFilter(datResource&);

		// PURPOSE: Filter type registration
		CR_DECLARE_FRAME_FILTER_TYPE(StaticFrameFilter);

		// PURPOSE: Filter DOF override
		virtual bool FilterDof(u8 track, u16 id, float& inoutWeight);

	protected:

		// PURPOSE: Signature calculation
		virtual u32 CalcSignature() const;
	};

	//
	static fwMvFilterId s_UpperBodyFilterId;

	static fwMvFilterId s_UpperBodyFilterHashId;

	// PURPOSE: 
	CMovePed(CDynamicEntity& dynamicEntity);

	// PURPOSE:
	virtual ~CMovePed();

	// PURPOSE:
	virtual void Init(fwEntity& dynamicEntity, const mvNetworkDef& definition, const fwAnimDirector& owner);

	// PURPOSE:
	void Shutdown();

	// PURPOSE:
	void Update(float deltaTime);

	// PURPOSE:
	virtual void PostUpdate();

	// PURPOSE: Main thread finish update, (called prior to output buffer swap, after motion tree finished)
	virtual void FinishUpdate();

	// PURPOSE: Main thread finish post update, (called after output buffer swap, after motion tree finished)
	virtual void PostFinishUpdate();

	virtual bool IsUsingRagDoll(fwEntity &entity) const;

	virtual bool GetShouldBeLowLOD(fwEntity &entity) const;

	virtual CClipHelper* FindClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim);
	virtual CClipHelper* FindClipHelper(s32 dictIndex, u32 animHash);
	virtual CGenericClipHelper* FindGenericClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim);

	// Ped specific move network slots

	// PURPOSE: Blend in a network at the Motion task level. 
	// PARAMS:	player - A pointer to the network player to insert.
	//			blendType - The blend duration to use
	bool SetMotionTaskNetwork(fwMoveNetworkPlayer *player, float blendDuration = 0.0f, MotionTaskSlotFlags flags = MotionTask_None);
	bool SetMotionTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f, MotionTaskSlotFlags flags = MotionTask_None);
	fwMoveNetworkPlayer* GetMotionTaskNetwork() { return m_MotionTaskNetwork.GetNetworkPlayer(); }

	// PURPOSE: Blend out the network currently in the motion task slot
	bool ClearMotionTaskNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f);
	bool ClearMotionTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f);

	// PURPOSE: Inserts the provided network player into the peds task MoVE network slot
	bool SetTaskNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f, u32 flags = Task_None, fwMvFilterId filter = FILTER_ID_INVALID);
	bool SetTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f, u32 flags = Task_None, fwMvFilterId filter = FILTER_ID_INVALID);
	fwMoveNetworkPlayer* GetTaskNetwork() { return m_TaskNetwork.GetNetworkPlayer(); }

	bool IsTaskNetworkFullyBlended() { return m_networkSlotFlags.IsFlagSet(IsFullyBlended); }
	bool IsTaskNetworkFullyBlendedOut() { return m_networkSlotFlags.IsFlagSet(IsFullyBlendedOut); }
	bool IsSecondaryTaskNetworkFullyBlended() { return m_networkSlotFlags.IsFlagSet(IsSecondaryFullyBlended); }
	bool IsSecondaryTaskNetworkFullyBlendedOut() { return m_networkSlotFlags.IsFlagSet(IsSecondaryFullyBlendedOut); }
	bool IsFacialNetworkFullyBlended() { return m_networkSlotFlags.IsFlagSet(IsFacialFullyBlended); }

	// PURPOSE: Blend out the network currently in the task slot
	bool ClearTaskNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f, u32 flags = Task_None);
	bool ClearTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f, u32 flags = Task_None);

	// PURPOSE: Inserts the provided network player into the peds upper body task MoVE network slot
	bool SetSecondaryTaskNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f, u32 flags = Secondary_None, fwMvFilterId filter = FILTER_ID_INVALID);
	bool SetSecondaryTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f, u32 flags = Secondary_None, fwMvFilterId filter = FILTER_ID_INVALID);
	fwMoveNetworkPlayer* GetSecondaryTaskNetwork() { return m_SecondaryTaskNetwork.GetNetworkPlayer(); }

	// PURPOSE: Blend out the network currently in the upper body slot
	bool ClearSecondaryTaskNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f, u32 flags = Secondary_None);
	bool ClearSecondaryTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f, u32 flags = Secondary_None);

	// PURPOSE: Inserts the provided network player into the peds additive MoVE network slot
	bool SetAdditiveNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f);
	bool SetAdditiveNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f);

	// PURPOSE: Blend out the network currently in the additive slot
	bool ClearAdditiveNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f);
	bool ClearAdditiveNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f);

	//////////////////////////////////////////////////////////////////////////
	//	Standard ped facial methods
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: Set the facial idle animation to be played back. This anim will loop continuously for as long as standard facial
	// anim playback is enabled on the ped.
	void SetFacialIdleClip(const crClip* pClip, float blendDuration);

	// PURPOSE: Play a one-shot facial animation. The anim will override the currently playing idle ,and will automatically blend out at the end
	void PlayOneShotFacialClip(const crClip* pClip, float blendDuration, bool bBlockOneShotFacial = false);
	void ClearOneShotFacialBlock();

	// PURPOSE: Play back a viseme animation. Overrides both idle and single shot anims. Only for use by the audio synced facial anim playback system.
	void PlayVisemeClip(const crClip* pClip, float blendDuration, float rate = 1.0f);

	bool IsPlayingStandardFacial() { return m_networkSlotFlags.IsFlagSet(IsUsingStandardFacial); }

	//////////////////////////////////////////////////////////////////////////
	//	Custom facial move networks
	//	Note: The calls below will disable standard facial anim playback.
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: Inserts the provided network player into the peds facial MoVE network slot
	bool SetFacialNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f);
	bool SetFacialNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f);

	// PURPOSE: Remove the facial animation network
	bool ClearFacialNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration = 0.0f);
	bool ClearFacialNetwork(CMoveNetworkHelper& networkHelper, float blendDuration = 0.0f);

	//////////////////////////////////////////////////////////////////////////
	//	Custom hand pose animations
	//	Used to control the hands when in ragdoll.
	//  Only visible on the ped in ragdoll mode
	//////////////////////////////////////////////////////////////////////////

	enum ePedHand
	{
		kHandLeft,
		kHandRight,
		kNumHands
	};

	void PlayHandAnimation(const crClip* pClip, float blendDuration, ePedHand hand);
	void ClearHandAnimation(float blendDuration, ePedHand hand);

	// PURPOSE: Set the synced scene network
	bool SetSynchronizedSceneNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration)
	{
		static bool s_ForceTagSyncIn = false;
		u32 flags = Task_None;
		if (s_ForceTagSyncIn)
		{
			flags = Task_TagSyncTransition;
		}
		SetTaskNetwork(pPlayer, blendDuration, flags);
		return true;
	}

	void ClearSynchronizedSceneNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, bool tagSyncOut = false )
	{
		u32 flags = Task_None;

		if (blendDuration>0.0f)
		{
			flags = Task_DontDisableParamUpdate;
		}
	
		if (tagSyncOut)
		{
			flags |= Task_TagSyncTransition;
		}

		ClearTaskNetwork( pPlayer, blendDuration, flags );
	}

	// PURPOSE: Initialize class
	static void InitClass();

	// PURPOSE: Shutdown class
	static void ShutdownClass();

	// PURPOSE: Return the simple blend helper
	const CSimpleBlendHelper& GetBlendHelper() const { return m_BlendHelper; }
	CSimpleBlendHelper& GetBlendHelper() { return m_BlendHelper; }

	const CSimpleBlendHelper& GetSecondaryBlendHelper() const { return m_SecondaryBlendHelper; }
	CSimpleBlendHelper& GetSecondaryBlendHelper() { return m_SecondaryBlendHelper; }

	CSimpleBlendHelper* GetSimpleBlendHelper() { return &m_BlendHelper; }

	CSimpleBlendHelper* FindAppropriateBlendHelperForClipHelper(CClipHelper& helper)
	{
		if (helper.IsFlagSet(APF_USE_SECONDARY_SLOT))
		{
			return &m_SecondaryBlendHelper;
		}
		else
		{
			return &m_BlendHelper; 
		}
	}


	// PURPOSE: Return the additive anim helper
	CGenericClipHelper& GetAdditiveHelper() { return m_AdditiveHelper; }

	// PURPOSE: Return the facial anim helper
	CGenericClipHelper& GetFacialHelper() { return m_FacialHelper; }

	// PURPOSE:
	void SetTaskMoverFrame(const Vector3& translationDelta, const Quaternion& rotationDelta);

	// PURPOSE:
	void SetMotionTaskMoverFrame(const Vector3& translationDelta, const Quaternion& rotationDelta);

	// PURPOSE: Set the desired weight for the upper body fixup
	void SetUpperBodyFixupWeight(float upperBodyFixupWeight) { m_upperBodyFixupWeightTarget = upperBodyFixupWeight; }

	// PURPOSE:
	ePedState GetState() const;

#if !__NO_OUTPUT
	// PURPOSE:
	static const char * GetStateName(ePedState state);
#endif //__NO_OUTPUT

	// PURPOSE: switch the ped network to ragdoll mode
	void SwitchToRagdoll(crFrameFilter* pInvalidRagdollFilter);

	// PURPOSE: switch the ped network to animated mode
	void SwitchToAnimated(float blendDuration, bool extrapolate = true, bool bInstantRestart = false);

	// PURPOSE: switch the ped network to static frame mode
	void SwitchToStaticFrame(float blendDuration);

	// PURPOSE: resets the blend helper callbacks to their defaults. Called from CTaskAmbientClips::CleanUp (as we use some custom callbacks for lowrider ambient idles).
	void ResetBlendHelperCallbacks();

protected:

	void UpdateUpperBodyFixupWeight(float deltaTime);

	bool InsertBlendHelperNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, u32 flags);

	bool InsertSecondaryNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, u32)
	{
		return SetSecondaryTaskNetwork(pPlayer, blendDuration);
	}

	bool InsertAdditiveHelperNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, u32)
	{
		return SetAdditiveNetwork(pPlayer, blendDuration);
	}

	bool InsertFacialHelperNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, u32)
	{
		return SetFacialNetwork(pPlayer, blendDuration);
	}

	void BlendHelperBlendInAnim(CSimpleBlendHelper&, float blendDelta, u32 uFlags);

	void BlendHelperBlendOutAnim(CSimpleBlendHelper&, float blendDelta, u32 uFlags);

	void SecondaryHelperBlendInAnim(CSimpleBlendHelper&, float blendDelta, u32 uFlags);

	void SecondaryHelperBlendOutAnim(CSimpleBlendHelper&, float blendDelta, u32 uFlags);

	enum eFacialState{
		NoFacial,
		StandardFacial,
		CustomFacialNetwork
	};

	void RequestFacialState(eFacialState, float blendDuration);

private:

	// PURPOSE: A simple blend helper that plays back anims on the task network slot
	CSimpleBlendHelper m_BlendHelper;

	// PURPOSE: A simple blend helper that plays back anims on the upper body network slot
	CSimpleBlendHelper m_SecondaryBlendHelper;

	// PURPOSE: Allows playback of a single additive clip (with cross blends)
	CGenericClipHelper m_AdditiveHelper;

	// PURPOSE: Allows playback of a single facial clip (with cross blends)
	CGenericClipHelper m_FacialHelper;

	// the network player currently inserted into the task slot
	CNetworkSlot m_TaskNetwork;

	// the network player currently inserted into the secondary task slot
	CNetworkSlot m_SecondaryTaskNetwork;

	// the network player currently inserted into the motion task slot
	CNetworkSlot m_MotionTaskNetwork;

	// the network player currently inserted into the additive slot
	CNetworkSlot m_AdditiveNetwork;

	// the network player currently inserted into the facial slot
	CNetworkSlot m_FacialNetwork;

	// the weight of the upperbody fixup expression
	float m_upperBodyFixupWeight;
	// the desired weight of the upperbody fixup expression
	float m_upperBodyFixupWeightTarget;

	// store some flags to track the state of the network
	fwFlags16 m_networkSlotFlags;

	ePedState m_State;

	int m_iBlockOneShotFacialRefCount;

	static RagdollFilter sm_Filter;
	static StaticFrameFilter sm_StaticFrameFilter;
	static const fwMvRequestId sm_ChangePedState;
	static const fwMvFlagId sm_PedStateRagdoll;
	static const fwMvFlagId sm_PedStateAnimated;
	static const fwMvFlagId sm_PedStateStaticFrame;
	static const fwMvFlagId sm_UseInvalidRagdollFilter;
	static const fwMvFloatId sm_PedStateBlendDuration;
	static const fwMvFilterId sm_RagdollFilterId;
	static const fwMvFilterId sm_InvalidRagdollFilterId;
	static const fwMvFilterId sm_StaticFrameFilterId;
	static const fwMvFilterId sm_ExtrapolateFilterId;
	static const fwMvRequestId sm_InstantRestartAnimated;
};

//////////////////////////////////////////////////////////////////////////

class CSimpleBlendIterator : public crmtIterator
{	
public:
	CSimpleBlendIterator( crmtNode* referenceNode = NULL ) 
		: m_CumulativeBlend(0.0f)
		, m_CombinedEvents(0)
		, m_ReferenceNode(referenceNode)
		{};

	~CSimpleBlendIterator() {};

	// PURPOSE: Iterator start override
	virtual void Start();

	// PURPOSE: Iterator visit override
	virtual void Visit(crmtNode&);

	float	GetBlend() { return m_CumulativeBlend; }
	u32		GetEvents() { return m_CombinedEvents; }

protected:

	float CalcVisibleBlend(crmtNode* pNode );

	float m_CumulativeBlend;	// the total blend of the clips based on the current transitions
	u32	  m_CombinedEvents;		// The combined flags generated by the clips in the last update
	crmtNode* m_ReferenceNode;	// The node to use as the reference parent for the blend
								 // Leave blank for the entire tree
};

////////////////////////////////////////////////////////////////////////////////

class CMovePedPooledObject : public CMovePed
{
public:

	CMovePedPooledObject(CDynamicEntity& dynamicEntity)
		: CMovePed(dynamicEntity) 
	{
	}

	FW_REGISTER_CLASS_POOL(CMovePedPooledObject);
};

#endif // MOVE_PED_H
