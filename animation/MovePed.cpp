#include "MovePed.h"

//rage includes
#include "crmotiontree/nodeblend.h"
#include "crmotiontree/nodeclip.h"
#include "crmetadata/tag.h"
#include "crmetadata/tagiterators.h"
#include "crmetadata/propertyattributes.h"
#include "system/memmanager.h"

#if __PPU && !__TOOL
#include "system/tinyheap.h"
#endif

//game includes
#if __DEV
#include "animation/debug/animviewer.h"
#endif //__DEV
#include "fwanimation/animdirector.h"
#include "fwanimation/clipsets.h"
#include "fwscene/stores/framefilterdictionarystore.h"
#include "data/resource.h"
#include "peds/ped.h"
#include "scene/DynamicEntity.h"
#include "streaming/streaming.h"
#include "streaming/streamingrequest.h"
#include "task/System/TaskHelpers.h"

ANIM_OPTIMISATIONS()

static void* s_PageMemBuffer = NULL;
static const u32 kPageMemSize = MOVEPED_HEAP_SIZE;

//////////////////////////////////////////////////////////////////////////
// MovePed
////////////////////////////////////////////////////////////////////////////////


fwMvFilterId CMovePed::s_UpperBodyFilterId("UpperBodyFilter",0x6F5D10AB);

fwMvFilterId CMovePed::s_UpperBodyFilterHashId("Upperbody_filter",0x35B1D869);

CMovePed::RagdollFilter CMovePed::sm_Filter;
CMovePed::StaticFrameFilter CMovePed::sm_StaticFrameFilter;
const fwMvRequestId CMovePed::sm_ChangePedState("ChangePedState",0xA9976948);
const fwMvFlagId CMovePed::sm_PedStateRagdoll("PedStateRagdoll",0xACA58F21);
const fwMvFlagId CMovePed::sm_PedStateAnimated("PedStateAnimated",0x26F41ACE);
const fwMvFlagId CMovePed::sm_PedStateStaticFrame("PedStateStaticFrame",0x507BA131);
const fwMvFlagId CMovePed::sm_UseInvalidRagdollFilter("UseInvalidRagdollFilter",0x0f09a78a);
const fwMvFloatId CMovePed::sm_PedStateBlendDuration("PedStateBlendDuration",0xF73CC9EE);
const fwMvFilterId CMovePed::sm_RagdollFilterId("RagdollFilter",0x88D7A146);
const fwMvFilterId CMovePed::sm_InvalidRagdollFilterId("InvalidRagdollFilter",0xF4A7435);
const fwMvFilterId CMovePed::sm_StaticFrameFilterId("StaticFrameFilter",0x8E50781A);
const fwMvFilterId CMovePed::sm_ExtrapolateFilterId("RootOnly_filter",0x48EBA8C5);
const fwMvRequestId CMovePed::sm_InstantRestartAnimated("InstantRestartAnimated",0xb1becb23);

CMovePed::CMovePed(CDynamicEntity& dynamicEntity)
: CMove(dynamicEntity)
, m_upperBodyFixupWeight(1.0f)
, m_upperBodyFixupWeightTarget(1.0f)
, m_networkSlotFlags(IsFullyBlendedOut | IsSecondaryFullyBlendedOut)
, m_State(kStateAnimated)
, m_iBlockOneShotFacialRefCount(0)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	m_BlendHelper.SetInsertCallback(MakeFunctorRet(*this, &CMovePed::InsertBlendHelperNetwork));
	m_BlendHelper.SetClipBlendInCallback(MakeFunctor(*this, &CMovePed::BlendHelperBlendInAnim));
	m_BlendHelper.SetClipBlendOutCallback(MakeFunctor(*this, &CMovePed::BlendHelperBlendOutAnim));

	m_SecondaryBlendHelper.SetInsertCallback(MakeFunctorRet(*this, &CMovePed::InsertSecondaryNetwork));
	m_SecondaryBlendHelper.SetClipBlendInCallback(MakeFunctor(*this, &CMovePed::SecondaryHelperBlendInAnim));
	m_SecondaryBlendHelper.SetClipBlendOutCallback(MakeFunctor(*this, &CMovePed::SecondaryHelperBlendOutAnim));

	m_AdditiveHelper.SetInsertCallback(MakeFunctorRet(*this, &CMovePed::InsertAdditiveHelperNetwork));
	m_FacialHelper.SetInsertCallback(MakeFunctorRet(*this, &CMovePed::InsertFacialHelperNetwork));
}

////////////////////////////////////////////////////////////////////////////////

CMovePed::~CMovePed()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::Init(fwEntity& dynamicEntity, const mvNetworkDef& definition, const fwAnimDirector& owner)
{
	CMove::Init(dynamicEntity, definition, owner);
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::Shutdown()
{
	CMove::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::Update(float deltaTime)
{
	static fwMvBooleanId taskTransitionFinishedId("Task_TransitionComplete",0x7BE656BC);
	static fwMvBooleanId secondaryTaskTransitionFinishedId("SecondaryTask_TransitionComplete",0xBECA7DEA);
	static fwMvBooleanId facialTransitionFinishedId("Facial_TransitionComplete",0x55816763);

#if 0
	crFrameFilter *frameFilter = g_FrameFilterDictionaryStore.FindFrameFilter(s_UpperBodyFilterHashId);
	if(frameFilter)
	{
		SetFilter(s_UpperBodyFilterId, frameFilter);
	}
#endif

	//////////////////////////////////////////////////////////////////////////
	// Update the blend helper for the task slot
	m_BlendHelper.Update( deltaTime );

	if (m_BlendHelper.IsNetworkActive() && m_BlendHelper.GetNetworkPlayer()->HasExpired())
	{
		m_BlendHelper.ReleaseNetworkPlayer();
	}

	if (GetBoolean(taskTransitionFinishedId))
	{
		if (m_networkSlotFlags.IsFlagSet(IsBlendingIn))
		{
			m_networkSlotFlags.SetFlag(IsFullyBlended);
		}
		else if (m_networkSlotFlags.IsFlagSet(IsBlendingOut))
		{
			m_networkSlotFlags.SetFlag(IsFullyBlendedOut);
		}
	}
	
	//////////////////////////////////////////////////////////////////////////
	// Update the blend helper and weight for the upper body slot
	m_SecondaryBlendHelper.Update( deltaTime );
	
	if (m_SecondaryBlendHelper.IsNetworkActive() && m_SecondaryBlendHelper.GetNetworkPlayer()->HasExpired())
	{
		m_SecondaryBlendHelper.ReleaseNetworkPlayer();
	}

	if (GetBoolean(secondaryTaskTransitionFinishedId))
	{
		if (m_networkSlotFlags.IsFlagSet(IsSecondaryBlendingIn))
		{
			m_networkSlotFlags.SetFlag(IsSecondaryFullyBlended);
		}
		else if (m_networkSlotFlags.IsFlagSet(IsSecondaryBlendingOut))
		{
			m_networkSlotFlags.SetFlag(IsSecondaryFullyBlendedOut);
		}
	}

	if (m_networkSlotFlags.IsFlagSet(IsFacialBlendingIn) && GetBoolean(facialTransitionFinishedId))
	{
		m_networkSlotFlags.SetFlag(IsFacialFullyBlended);
	}

	// Update the facial and additive helpers
	m_AdditiveHelper.Update();
	m_FacialHelper.Update();

	// Update the upperbody fixup weight
	UpdateUpperBodyFixupWeight(deltaTime);
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::PostUpdate()
{
	static const  fwMvFrameId taskMoverFrameId("TaskMoverFrame",0x30B33AA7);
	SetFrame(taskMoverFrameId, NULL);
	
	static const  fwMvFrameId motionTaskMoverframeId("MotionTaskMoverFrame",0x9BC0873C);
	SetFrame(motionTaskMoverframeId, NULL);
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::FinishUpdate()
{
	m_BlendHelper.UpdatePreOutputBufferFlip();
	m_SecondaryBlendHelper.UpdatePreOutputBufferFlip();

	if (m_AdditiveHelper.IsNetworkActive())
	{
		m_AdditiveHelper.SetLastPhase(m_AdditiveHelper.GetPhase());
	}
	if (m_FacialHelper.IsNetworkActive())
	{
		m_FacialHelper.SetLastPhase(m_FacialHelper.GetPhase());
	}
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::PostFinishUpdate()
{
	CMove::PostFinishUpdate();

	m_BlendHelper.UpdatePostMotionTree();
	m_SecondaryBlendHelper.UpdatePostMotionTree();
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetMotionTaskNetwork(fwMoveNetworkPlayer *player, float blendDuration, MotionTaskSlotFlags flags)
{
	static const fwMvNetworkId motionNetworkId("MotionTasks",0x3653E912);
	static const fwMvFloatId motionTaskDuration("MotionTaskDuration",0x3FF540D);

	static const fwMvRequestId motionTaskRequest("Motion",0xCF5665AC);

	static const fwMvFlagId motionTaskTagSyncedTransitionFlag("MotionTask_TagSyncTransition",0xEF37865);

	// Do we want to tag sync the transition?
	SetFlag(motionTaskTagSyncedTransitionFlag, (flags & MotionTask_TagSyncTransition)>0);

	SetFloat(motionTaskDuration, blendDuration);
	BroadcastRequest(motionTaskRequest);

	if (player==NULL)
	{
		SetSubNetwork(motionNetworkId, NULL);
	}
	else
	{
		SetSubNetwork(motionNetworkId, player->GetSubNetwork());
	}

	m_MotionTaskNetwork.SetNetworkPlayer(player);

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetMotionTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration, MotionTaskSlotFlags flags)
{
	fwMoveNetworkPlayer* pPlayer = networkHelper.GetNetworkPlayer();
	animAssertf(pPlayer, "The MoVE network helper has not created its player!");
	SetMotionTaskNetwork(pPlayer, blendDuration, flags);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearMotionTaskNetwork(fwMoveNetworkPlayer* player, float blendDuration)
{
	if (m_MotionTaskNetwork.GetNetworkPlayer()==player)
	{
		return SetMotionTaskNetwork(NULL, blendDuration);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearMotionTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration)
{
	if (networkHelper.IsNetworkActive() && networkHelper.GetNetworkPlayer()==m_MotionTaskNetwork.GetNetworkPlayer())
	{
		return SetMotionTaskNetwork(NULL, blendDuration);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::SetFacialIdleClip(const crClip* pClip, float blendDuration)
{

	static const fwMvRequestId facialIdleClipChangedId("FacialIdleClipChanged",0xD26C98C4);
	static const fwMvClipId facialIdleClipId("FacialIdleClip",0x5DCCE343);
	static const fwMvFloatId facialIdleClipBlendDuration("FacialIdleClipBlendDuration",0x46C8BCEB);

	RequestFacialState(StandardFacial, blendDuration);

	BroadcastRequest(facialIdleClipChangedId);
	SetClip(facialIdleClipId, pClip);
	SetFloat(facialIdleClipBlendDuration, blendDuration);

}

//////////////////////////////////////////////////////////////////////////

void CMovePed::PlayOneShotFacialClip(const crClip* pClip, float blendDuration, bool bBlockOneShotFacial /* = false */)
{

	static const fwMvRequestId PlayOneShotId("PlayFacialOneShot", 0x1BBF5559);
	static const fwMvClipId facialClipId("FacialOneShotClip", 0x493C9BB8);
	static const fwMvFloatId facialClipBlendDuration("FacialOneShotClipBlendDuration", 0x9FF1248C);

	if (bBlockOneShotFacial)
	{
		if (BANK_ONLY(!CAnimViewer::m_pBank && ) !animVerifyf(NetworkInterface::IsGameInProgress(), "One shot facial can only be blocked in a network game!"))
		{
			bBlockOneShotFacial = false;
		}
	}

	if (m_iBlockOneShotFacialRefCount == 0)
	{
		RequestFacialState(StandardFacial, blendDuration);

		BroadcastRequest(PlayOneShotId);
		SetClip(facialClipId, pClip);
		SetFloat(facialClipBlendDuration, blendDuration);
	}

	if (bBlockOneShotFacial)
	{
		m_iBlockOneShotFacialRefCount++;
	}
}

void CMovePed::ClearOneShotFacialBlock()
{
	if (BANK_ONLY(CAnimViewer::m_pBank || ) animVerifyf(NetworkInterface::IsGameInProgress(), "One shot facial can only be blocked in a network game!"))
	{
		if (animVerifyf(m_iBlockOneShotFacialRefCount > 0, "One shot facial was not already blocked!"))
		{
			m_iBlockOneShotFacialRefCount--;
		}
	}
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::PlayVisemeClip(const crClip* pClip, float blendDuration, float rate /* = 1.0f */)
{
	static const fwMvRequestId PlayVisemeId("PlayViseme",0xB503EEEF);
	static const fwMvClipId facialClipId("FacialVisemeClip",0x7839CEA);
	static const fwMvFloatId facialClipBlendDurationId("FacialVisemeClipBlendDuration",0x730B1F19);
	static const fwMvFloatId facialVisemeRateId("FacialVisemeRate",0x35F5F7AA);

	RequestFacialState(StandardFacial, blendDuration);

	BroadcastRequest(PlayVisemeId);
	SetClip(facialClipId, pClip);
	SetFloat(facialClipBlendDurationId, blendDuration);
	SetFloat(facialVisemeRateId, rate);
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::RequestFacialState(eFacialState state, float blendDuration)
{
	static const fwMvRequestId facialRequest("Facial",0x511E56C2);
	static const fwMvFloatId facialDuration("FacialDuration",0x514BDFE9);

	static const fwMvFlagId standardFlag("Facial_Standard",0xA6536B39);
	static const fwMvFlagId noNetworkFlag("Facial_NoFacial",0x3DDF2C9B);
	static const fwMvFlagId networkFlag("Facial_FacialNetwork",0x1A3E403E);

	BroadcastRequest(facialRequest);

	if (state==StandardFacial)
	{
		m_networkSlotFlags.SetFlag(IsUsingStandardFacial);
	}
	else
	{
		m_networkSlotFlags.ClearFlag(IsUsingStandardFacial);
	}

	SetFlag(noNetworkFlag, state==NoFacial);
	SetFlag(networkFlag, state==CustomFacialNetwork);
	SetFlag(standardFlag, state==StandardFacial);

	SetFloat(facialDuration, blendDuration);

	m_FacialNetwork.SetNetworkPlayer(NULL);
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetFacialNetwork(fwMoveNetworkPlayer *player, float blendDuration)
{
	static const fwMvNetworkId facialNetworkId("Facials",0x45BB32FF);

	RequestFacialState( player!=NULL ? CustomFacialNetwork : NoFacial, blendDuration);

	m_networkSlotFlags.ClearFlag(FacialSlotMask);

	if (player!=NULL)
	{
		SetSubNetwork(facialNetworkId, player->GetSubNetwork());

		if (blendDuration>0.0f)
		{
			m_networkSlotFlags.SetFlag(IsSecondaryBlendingIn);
		}
		else
		{
			m_networkSlotFlags.SetFlag(IsSecondaryFullyBlended);
		}
	}

	m_FacialNetwork.SetNetworkPlayer(player);

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetFacialNetwork(CMoveNetworkHelper& networkHelper, float blendDuration)
{
	fwMoveNetworkPlayer* pPlayer = networkHelper.GetNetworkPlayer();
	animAssertf(pPlayer, "The MoVE network helper has not created its player!");
	SetFacialNetwork(pPlayer, blendDuration);
	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearFacialNetwork(fwMoveNetworkPlayer* player, float blendDuration)
{
	if (m_FacialNetwork.GetNetworkPlayer()==player)
	{
		return SetFacialNetwork(NULL, blendDuration);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearFacialNetwork(CMoveNetworkHelper& networkHelper, float blendDuration)
{
	if (networkHelper.IsNetworkActive() && networkHelper.GetNetworkPlayer()==m_FacialNetwork.GetNetworkPlayer())
	{
		return SetFacialNetwork(NULL, blendDuration);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
void CMovePed::PlayHandAnimation(const crClip* pClip, float blendDuration, ePedHand hand)
{
	static fwMvClipId s_HandAnimClipId[kNumHands] = {fwMvClipId("HandPoseClipLeft",0xC6F66E2C), fwMvClipId("HandPoseClipRight",0x9B94E22E)};
	static fwMvRequestId s_HandAnimRequestId[kNumHands] = {fwMvRequestId("HandPoseChangedLeft",0x694FB21B), fwMvRequestId("HandPoseChangedRight",0xA7EF9B1B)};
	static fwMvFlagId s_HandPoseAnimId[kNumHands] = {fwMvFlagId("HandPoseAnimLeft",0xB1142E87), fwMvFlagId("HandPoseAnimRight",0x173CD410)};
	static fwMvFloatId s_HandPoseDurationId[kNumHands] = {fwMvFloatId("HandPoseDurationLeft",0x7D4EF6BC), fwMvFloatId("HandPoseDurationRight",0x6FE34B82)};

	BroadcastRequest(s_HandAnimRequestId[hand]);
	SetFlag(s_HandPoseAnimId[hand], pClip!=NULL);
	SetFloat(s_HandPoseDurationId[hand], blendDuration);
	if (pClip!=NULL)
	{
		SetClip(s_HandAnimClipId[hand], pClip);
	}
}

//////////////////////////////////////////////////////////////////////////
void CMovePed::ClearHandAnimation(float blendDuration, ePedHand hand)
{
	PlayHandAnimation(NULL, blendDuration, hand);
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration, u32 flags, fwMvFilterId filter)
{
	fwMoveNetworkPlayer* pPlayer = networkHelper.GetNetworkPlayer();
	animAssertf(pPlayer, "The MoVE network helper has not created its player!");
	SetTaskNetwork(pPlayer, blendDuration, flags, filter);
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetTaskNetwork(fwMoveNetworkPlayer* player, float blendDuration, u32 flags, fwMvFilterId filter)
{
	static const fwMvNetworkId taskNetwork("Tasks",0xAF53DED1);
	static const fwMvFloatId taskDuration("TaskDuration",0x1CD2C3FB);
	static const fwMvFilterId taskFilter("TaskFilter",0xC0A4A24);

	static const fwMvRequestId taskRequest("Task",0xB9FCBE75);

	static const fwMvFlagId noTaskFlag("Task_NoTask",0x6E217726);
	static const fwMvFlagId taskFlag("Task_Task",0x3C8D4F86);
	static const fwMvFlagId taskTagSyncedFlag("Task_TagSynced",0xFCFB175A);
	static const fwMvFlagId taskTagSyncedTransitionFlag("Task_TagSyncTransition",0x5DDA6169);
	static const fwMvFlagId taskDontDisableParamUpdateFlag("Task_DontDisableParamUpdate",0x75E32E59);
	static const fwMvFlagId taskIgnoreMoverBlendRotation("Task_IgnoreMoverBlendRotation",0x7C41CB1D);
	static const fwMvFlagId taskIgnoreMoverBlend("Task_IgnoreMoverBlend",0x69EFF817);

	// Do we want to tag sync the transition?
	SetFlag(taskTagSyncedTransitionFlag, (flags & Task_TagSyncTransition)>0);
	SetFlag(taskDontDisableParamUpdateFlag, (flags & Task_DontDisableParamUpdate)>0);
	SetFlag(taskIgnoreMoverBlendRotation, (flags & Task_IgnoreMoverBlendRotation)>0);
	SetFlag(taskIgnoreMoverBlend, (flags & Task_IgnoreMoverBlend)>0);

#if __ASSERT
	if (player && GetState()!=kStateAnimated)
		smart_cast<const CPed*>(&(AnimDirector().GetEntity()))->SpewRagdollTaskInfo();
#endif //__ASSERT
	animAssertf(!player || GetState()==kStateAnimated, "Trying to insert a move network on a ped in the wrong animation state: state:%s", GetStateName(GetState()));
	SetFloat(taskDuration, blendDuration);
	
	BroadcastRequest(taskRequest);
	if (filter!=FILTER_ID_INVALID)
	{
		crFrameFilter* pFilter = g_FrameFilterDictionaryStore.FindFrameFilter(filter);
		SetFilter(taskFilter, pFilter);
	}

	m_networkSlotFlags.ClearFlag(TaskSlotMask);

	if (player==NULL)
	{
		SetFlag(noTaskFlag, true);
		SetFlag(taskFlag, false);
		SetFlag(taskTagSyncedFlag, false);

		if (blendDuration>0.0f)
		{
			m_networkSlotFlags.SetFlag(IsBlendingOut);
		}
		else
		{
			m_networkSlotFlags.SetFlag(IsFullyBlendedOut);
		}
	}
	else
	{
		SetFlag(noTaskFlag, false);
		SetFlag(taskFlag, !(flags & Task_TagSyncContinuous) );
		SetFlag(taskTagSyncedFlag, flags & Task_TagSyncContinuous);
		SetSubNetwork(taskNetwork, player->GetSubNetwork());
		
		if (blendDuration>0.0f)
		{
			m_networkSlotFlags.SetFlag(IsBlendingIn);
		}
		else
		{
			m_networkSlotFlags.SetFlag(IsFullyBlended);
		}
	}

	m_TaskNetwork.SetNetworkPlayer(player);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearTaskNetwork(fwMoveNetworkPlayer* player, float blendDuration, u32 flags)
{
	if (m_TaskNetwork.GetNetworkPlayer()==player)
	{
		return SetTaskNetwork(NULL, blendDuration, flags);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration, u32 flags)
{
	if (networkHelper.IsNetworkActive() && networkHelper.GetNetworkPlayer()==m_TaskNetwork.GetNetworkPlayer())
	{
		return SetTaskNetwork(NULL, blendDuration, flags);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetSecondaryTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration, u32 flags, fwMvFilterId filter)
{
	fwMoveNetworkPlayer* pPlayer = networkHelper.GetNetworkPlayer();
	animAssertf(pPlayer, "The MoVE network helper has not created its player!");
	SetSecondaryTaskNetwork(pPlayer, blendDuration, flags, filter);
	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetSecondaryTaskNetwork(fwMoveNetworkPlayer* player, float blendDuration, u32 flags, fwMvFilterId filter)
{
	static const fwMvNetworkId upperBodyNetwork("SecondaryTasks",0xD6B53085);
	static const fwMvFloatId upperBodyDuration("SecondaryDuration",0xBCCC20B8);

	static const fwMvRequestId SecondaryRequest("Secondary",0xC9B4499C);
	static const fwMvFilterId SecondaryFilter("SecondaryFilter",0x21E7299E);

	static const fwMvFlagId noSecondaryFlag("Secondary_None",0xDE513DCB);
	static const fwMvFlagId upperBodyFlag("Secondary_SecondaryTask",0xDCD89684);
	static const fwMvFlagId SecondaryTagSyncedFlag("Secondary_TagSynced",0xC37595A1);
	static const fwMvFlagId SecondaryTagSyncedTransitionFlag("Secondary_TagSyncTransition",0xBA9AD64C);

	static const fwMvFloatId SecondaryWeightId("SecondaryWeight",0x4EC37DB1);

	SetFlag(SecondaryTagSyncedTransitionFlag, (flags & Secondary_TagSyncTransition)>0);
	
#if __ASSERT
	if (player && GetState()!=kStateAnimated)
	{
		smart_cast<const CPed*>(&(AnimDirector().GetEntity()))->SpewRagdollTaskInfo();
		animWarningf("Trying to insert a move network on a ped in the wrong animation state: state:%s", GetStateName(GetState()));
	}
#endif //__ASSERT

	SetFloat(upperBodyDuration, blendDuration);
	BroadcastRequest(SecondaryRequest);
	SetFilter(SecondaryFilter, g_FrameFilterDictionaryStore.FindFrameFilter(filter));

	SetFloat(SecondaryWeightId, 1.0f);

	m_networkSlotFlags.ClearFlag(SecondarySlotMask);

	if (player==NULL)
	{
		SetFlag(noSecondaryFlag, true);
		SetFlag(upperBodyFlag, false);
		SetFlag(SecondaryTagSyncedFlag, false);

		if (blendDuration>0.0f)
		{
			m_networkSlotFlags.SetFlag(IsSecondaryBlendingOut);
		}
		else
		{
			m_networkSlotFlags.SetFlag(IsSecondaryFullyBlendedOut);
		}
	}
	else
	{
		SetFlag(noSecondaryFlag, false);
		SetFlag(upperBodyFlag, !(flags & Secondary_TagSyncContinuous) );
		SetFlag(SecondaryTagSyncedFlag, flags & Secondary_TagSyncContinuous);
		SetSubNetwork(upperBodyNetwork, player->GetSubNetwork());

		if (blendDuration>0.0f)
		{
			m_networkSlotFlags.SetFlag(IsSecondaryBlendingIn);
		}
		else
		{
			m_networkSlotFlags.SetFlag(IsSecondaryFullyBlended);
		}
	}

	m_SecondaryTaskNetwork.SetNetworkPlayer(player);

	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearSecondaryTaskNetwork(fwMoveNetworkPlayer* player, float blendDuration, u32 flags)
{
	if (m_SecondaryTaskNetwork.GetNetworkPlayer()==player)
	{
		return SetSecondaryTaskNetwork(NULL, blendDuration, flags);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearSecondaryTaskNetwork(CMoveNetworkHelper& networkHelper, float blendDuration, u32 flags)
{
	if (networkHelper.IsNetworkActive() && networkHelper.GetNetworkPlayer()==m_SecondaryTaskNetwork.GetNetworkPlayer())
	{
		return SetSecondaryTaskNetwork(NULL, blendDuration, flags);
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////


bool CMovePed::SetAdditiveNetwork(fwMoveNetworkPlayer* player, float blendDuration)
{
	static const fwMvNetworkId additiveNetworkId("Additives",0xE752C9FE);
	static const fwMvFloatId additiveDurationId("AdditiveDuration",0x1e0f6fff);
	static const fwMvRequestId additiveRequestId("Additive",0xdbe0ee64);
	static const fwMvFlagId additiveOnId("Additive_Additive",0x792b465e);
	static const fwMvFlagId additiveOffId("Additive_NoAdditive",0x6217b868);

	BroadcastRequest(additiveRequestId);
	SetFloat(additiveDurationId, blendDuration);

	if (player==NULL)
	{
		SetFlag(additiveOffId, true);
		SetFlag(additiveOnId, false);
	}
	else
	{
		SetFlag(additiveOffId, false);
		SetFlag(additiveOnId, true);
		SetSubNetwork(additiveNetworkId, player->GetSubNetwork());
	}

	m_AdditiveNetwork.SetNetworkPlayer(player);

	return true;
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::SetAdditiveNetwork(CMoveNetworkHelper& networkHelper, float blendDuration)
{
	fwMoveNetworkPlayer* pPlayer = networkHelper.GetNetworkPlayer();
	animAssertf(pPlayer, "The MoVE network helper has not created its player!");
	SetAdditiveNetwork(pPlayer, blendDuration);
	return true;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearAdditiveNetwork(fwMoveNetworkPlayer* player, float blendDuration)
{
	if (m_AdditiveNetwork.GetNetworkPlayer()==player)
	{
		return SetAdditiveNetwork(NULL, blendDuration);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////
bool CMovePed::ClearAdditiveNetwork(CMoveNetworkHelper& networkHelper, float blendDuration)
{
	if (networkHelper.IsNetworkActive() && networkHelper.GetNetworkPlayer()==m_AdditiveNetwork.GetNetworkPlayer())
	{
		return SetAdditiveNetwork(NULL, blendDuration);
	}

	return false;
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::BlendHelperBlendInAnim(CSimpleBlendHelper& helper, float UNUSED_PARAM(blendDelta), u32 UNUSED_PARAM(uFlags))
{
	if (helper.GetNetworkPlayer() && helper.GetNetworkPlayer()->IsInserted())
	{
		//if (m_TaskWeightDelta<blendDelta)
		//{
		//	m_TaskWeightDelta=blendDelta;
		//}
	}
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::BlendHelperBlendOutAnim(CSimpleBlendHelper& helper, float blendDelta, u32 uFlags)
{
	if (helper.GetNetworkPlayer() && helper.GetNetworkPlayer()->IsInserted() && helper.GetNumActiveClips()==0 && !helper.IsNetworkBlendingOut())
	{
		ClearTaskNetwork(helper, fwAnimHelpers::CalcBlendDuration(blendDelta), uFlags);
	}
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::SecondaryHelperBlendInAnim(CSimpleBlendHelper& helper, float UNUSED_PARAM(blendDelta), u32 UNUSED_PARAM(uFlags))
{
	if (helper.GetNetworkPlayer() && helper.GetNetworkPlayer()->IsInserted())
	{
		//if (m_UpperBodyWeightDelta<blendDelta)
		//{
		//	m_UpperBodyWeightDelta=blendDelta;
		//}
	}
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::SecondaryHelperBlendOutAnim(CSimpleBlendHelper& helper, float blendDelta, u32 uFlags)
{
	if (helper.GetNetworkPlayer() && helper.GetNetworkPlayer()->IsInserted() && helper.GetNumActiveClips()==0 && !helper.IsNetworkBlendingOut())
	{
		ClearSecondaryTaskNetwork(helper, fwAnimHelpers::CalcBlendDuration(blendDelta), uFlags);
	}
}

//////////////////////////////////////////////////////////////////////////

CClipHelper* CMovePed::FindClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim)
{
	s32 dictIndex = fwAnimManager::FindSlotFromHashKey(dict.GetHash()).Get();
	return FindClipHelper(dictIndex, anim.GetHash());
}
//////////////////////////////////////////////////////////////////////////
CClipHelper* CMovePed::FindClipHelper(s32 dictIndex, u32 animHash)
{
	CClipHelper* pResult = m_BlendHelper.FindClipByIndexAndHash(dictIndex, animHash);
	if (pResult)
	{
		return pResult;
	}
	pResult = m_SecondaryBlendHelper.FindClipByIndexAndHash(dictIndex, animHash);
	return pResult;
}
//////////////////////////////////////////////////////////////////////////

CGenericClipHelper* CMovePed::FindGenericClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim)
{
	s32 dictIndex = fwAnimManager::FindSlotFromHashKey(dict.GetHash()).Get();
	
	if (m_AdditiveHelper.IsClipPlaying(dictIndex, anim.GetHash()))
	{
		return &m_AdditiveHelper;
	}
	if (m_FacialHelper.IsClipPlaying(dictIndex, anim.GetHash()))
	{
		return &m_FacialHelper;
	}
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////

class BufferPool : public atPoolBase
{
public:
	BufferPool(int bufSize)
		: atPoolBase(bufSize) {}

	void Shutdown() 
	{ 
		ReleaseMem(); 
	}

	void* New() 
	{ 
		return atPoolBase::New(GetElemSize());
	}
};

static void TogglePauseMode()
{
	if(fwTimer::IsGamePaused())
	{
		fwTimer::EndUserPause();
	}
	else
	{
		fwTimer::StartUserPause();
	}
}

static void SingleStep()
{
	fwTimer::SetWantsToSingleStepNextFrame();
}

static void SetTimeScale(float BANK_ONLY(scale))
{
#if __BANK
	fwTimer::SetDebugTimeScale(scale);
#endif // __BANK
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::InitClass()
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

#if __PPU && !__TOOL
	// EJ: Pre-allocate memory from flex allocator
	s_PageMemBuffer = sysMemManager::GetInstance().GetMovePedMemory();
#else
	s_PageMemBuffer = rage_new u8[kPageMemSize];
#endif

	mvMove::InitParams moveParams;
	moveParams.PageMemBuffer = s_PageMemBuffer;
	moveParams.PageMemBufferSize = kPageMemSize;
	moveParams.NetworkRegistrySize = 75; //This determines the maximum number of network defs we can store

	mvClipLoader clipLoader = MakeFunctorRet(&fwAnimDirector::RetrieveClip);
	mvExpressionLoader expressionLoader = MakeFunctorRet(&fwAnimDirector::RetrieveExpression);
	mvNetworkDefLoader networkDefLoader = MakeFunctorRet(&fwAnimDirector::RetrieveNetworkDef);
	mvFilterAllocator filterAllocator = MakeFunctorRet(&fwAnimDirector::RetrieveFilter);
	mvTransitionFilterAllocator transitionFilterAllocator = MakeFunctorRet(&fwAnimDirector::RetrieveTransitionFilter);
	mvMove::InitClass(&moveParams, NULL, &clipLoader, &expressionLoader, NULL, &filterAllocator, &transitionFilterAllocator, &networkDefLoader);

	mvMove::RegisterPauseChanged(MakeFunctor(TogglePauseMode));
	mvMove::RegisterSingleStep(MakeFunctor(SingleStep));
	mvMove::RegisterTimeScaleChanged(MakeFunctor(SetTimeScale));

#if __ASSERT
	mvNetworkDef::SetNetworkDefNameFunctor(MakeFunctorRet(fwAnimDirector::FindNetworkDefName));
#endif //__ASSERT
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::ShutdownClass()
{
	mvMove::Shutdown();

#if __PPU && !__TOOL
	// EJ: Pre-allocate memory from flex allocator
	s_PageMemBuffer = NULL;
#else
	delete [] (char*) s_PageMemBuffer;
#endif
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::SetTaskMoverFrame(const Vector3& translationDelta, const Quaternion& rotationDelta)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	static u8 track[] = {(u8)kTrackMoverTranslation, (u8)kTrackMoverRotation};
	static u16 id[] = {(u16)0, (u16)0};
	static u8 type[] = {(u8)kFormatTypeVector3, (u8)kFormatTypeQuaternion};
	static const  fwMvFrameId frameId("TaskMoverFrame",0x30B33AA7);
	crFrame *pTaskMoverFrame = rage_new crFrame();
	pTaskMoverFrame->InitCreateDofs(2, &track[0], &id[0], &type[0], true, NULL);
	pTaskMoverFrame->SetVector3(kTrackMoverTranslation, 0, VECTOR3_TO_VEC3V(translationDelta )); 
	pTaskMoverFrame->SetQuaternion(kTrackMoverRotation, 0, QUATERNION_TO_QUATV(rotationDelta)); 
	SetFrame(frameId, pTaskMoverFrame);
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::SetMotionTaskMoverFrame(const Vector3& translationDelta, const Quaternion& rotationDelta)
{
	USE_MEMBUCKET(MEMBUCKET_ANIMATION);

	static u8 track[] = {(u8)kTrackMoverTranslation, (u8)kTrackMoverRotation};
	static u16 id[] = {(u16)0, (u16)0};
	static u8 type[] = {(u8)kFormatTypeVector3, (u8)kFormatTypeQuaternion};
	static const fwMvFrameId frameId("MotionTaskMoverFrame",0x9BC0873C);
	crFrame *pMotionTaskMoverFrame = rage_new crFrame();
	pMotionTaskMoverFrame->InitCreateDofs(2, &track[0], &id[0], &type[0], true, NULL);
	pMotionTaskMoverFrame->SetVector3(kTrackMoverTranslation, 0, VECTOR3_TO_VEC3V(translationDelta)); 
	pMotionTaskMoverFrame->SetQuaternion(kTrackMoverRotation, 0, QUATERNION_TO_QUATV(rotationDelta)); 
	SetFrame(frameId, pMotionTaskMoverFrame);
}

////////////////////////////////////////////////////////////////////////////////

CMovePed::ePedState CMovePed::GetState() const
{
	return m_State;
}

////////////////////////////////////////////////////////////////////////////////

#if !__NO_OUTPUT

const char *CMovePed::GetStateName(ePedState state)
{
	switch(state)
	{
	case kStateAnimated: return "kStateAnimated";
	case kStateRagdoll: return "kStateRagdoll";
	case kStateStaticFrame: return "kStateStaticFrame";
	default: return "unknown state!";
	}
}

#endif // !__NO_OUTPUT

////////////////////////////////////////////////////////////////////////////////

void CMovePed::SwitchToStaticFrame(float blendDuration)
{
#if __DEV
	animDebugf2("[%u] CMovePed::SwitchToStaticFrame - Entity:%s(%p) duration: %.3f, previousState: %d", fwTimer::GetTimeInMilliseconds(), AnimDirector().GetEntity().GetModelName(), &AnimDirector().GetEntity(), blendDuration, m_State);
#else
	animDebugf2("[%u] CMovePed::SwitchToStaticFrame - Duration: %.3f, previousState: %d", fwTimer::GetTimeInMilliseconds(), blendDuration, m_State);
#endif // __DEV

	BroadcastRequest(sm_ChangePedState);
	SetFlag(sm_PedStateRagdoll, false);
	SetFlag(sm_PedStateAnimated, false);
	SetFlag(sm_PedStateStaticFrame, true);
	SetFilter(sm_StaticFrameFilterId, &sm_StaticFrameFilter);
	SetFloat(sm_PedStateBlendDuration, blendDuration);

	m_State=kStateStaticFrame;
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::SwitchToRagdoll(crFrameFilter* pInvalidRagdollFilter)
{
#if __DEV
	animDebugf2("[%u] CMovePed::SwitchToRagdoll - Entity:%s(%p) previousState: %d", fwTimer::GetTimeInMilliseconds(), AnimDirector().GetEntity().GetModelName(), &AnimDirector().GetEntity(), m_State);
#else
	animDebugf2("[%u] CMovePed::SwitchToRagdoll - PreviousState: %d", fwTimer::GetTimeInMilliseconds(), m_State);
#endif // __DEV

	BroadcastRequest(sm_ChangePedState);
	SetFlag(sm_PedStateRagdoll, true);
	SetFlag(sm_PedStateAnimated, false);
	SetFlag(sm_PedStateStaticFrame, false);
	SetFilter(sm_RagdollFilterId, &sm_Filter);
	SetFlag(sm_UseInvalidRagdollFilter, pInvalidRagdollFilter!=NULL);
	SetFilter(sm_InvalidRagdollFilterId, pInvalidRagdollFilter);

	m_State = kStateRagdoll;
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::SwitchToAnimated(float blendDuration, bool extrapolate, bool bInstantRestart)
{
#if __DEV
	animDebugf2("[%u] CMovePed::SwitchToAnimated - Entity:%s(%p), duration: %.3f, previousState: %d, extrapolate: %s", fwTimer::GetTimeInMilliseconds(), AnimDirector().GetEntity().GetModelName(), &AnimDirector().GetEntity(), blendDuration, m_State, extrapolate ? "true" : "false");
#else
	animDebugf2("[%u] CMovePed::SwitchToAnimated - Duration: %.3f, previousState: %d, extrapolate: %s", fwTimer::GetTimeInMilliseconds(), blendDuration, m_State, extrapolate ? "true" : "false");
#endif // __DEV

	m_networkSlotFlags = IsFullyBlendedOut | IsSecondaryFullyBlendedOut;

	BroadcastRequest(sm_ChangePedState);
	SetFlag(sm_PedStateRagdoll, false);
	SetFlag(sm_PedStateAnimated, true);
	SetFlag(sm_PedStateStaticFrame, false);
	SetFloat(sm_PedStateBlendDuration, blendDuration);
	SetFilter(sm_RagdollFilterId, extrapolate ? g_FrameFilterDictionaryStore.FindFrameFilter(sm_ExtrapolateFilterId) : NULL);

	if (bInstantRestart)
		BroadcastRequest(sm_InstantRestartAnimated);

	m_State = kStateAnimated;
}

////////////////////////////////////////////////////////////////////////////////

bool CMovePed::IsUsingRagDoll(fwEntity &entity) const
{
	const CPed* pPed = static_cast<CPed*>(&entity);
	return pPed->GetUsingRagdoll();
}

////////////////////////////////////////////////////////////////////////////////

bool CMovePed::GetShouldBeLowLOD(fwEntity &entity) const
{
	CPed* pPed = static_cast<CPed*>(&entity);
	bool isPlayer = pPed->IsLocalPlayer();

	// LOD the motion tree according to the model LOD, with the exception of player. 
	// player always uses the high LOD for anim
	return (!isPlayer && !pPed->GetPedDrawHandler().GetVarData().GetIsHighLOD());
}

//////////////////////////////////////////////////////////////////////////

void CMovePed::ResetBlendHelperCallbacks()
{
	m_BlendHelper.SetInsertCallback(MakeFunctorRet(*this, &CMovePed::InsertBlendHelperNetwork));
	m_BlendHelper.SetClipBlendInCallback(MakeFunctor(*this, &CMovePed::BlendHelperBlendInAnim));
	m_BlendHelper.SetClipBlendOutCallback(MakeFunctor(*this, &CMovePed::BlendHelperBlendOutAnim));
}

//////////////////////////////////////////////////////////////////////////

bool CMovePed::InsertBlendHelperNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, u32 flags)
{
	return SetTaskNetwork(pPlayer, blendDuration, (flags & CSimpleBlendHelper::Flag_TagSyncWithMotionTask) ? Task_TagSyncContinuous : Task_None );
}

////////////////////////////////////////////////////////////////////////////////

void CMovePed::UpdateUpperBodyFixupWeight(float deltaTime)
{
	static fwMvFloatId upperBodyFixupWeightId("UpperBodyFixupWeight",0xC493CA1);

	static dev_float APPROACH_RATE = 2.0f;
	Approach(m_upperBodyFixupWeight, m_upperBodyFixupWeightTarget, APPROACH_RATE, deltaTime);
	SetFloat(upperBodyFixupWeightId, m_upperBodyFixupWeight);
}

////////////////////////////////////////////////////////////////////////////////

CR_IMPLEMENT_FRAME_FILTER_TYPE(CMovePed::RagdollFilter, crFrameFilter::eFrameFilterType(crFrameFilter::kFrameFilterTypeFrameworkFilters+2), crFrameFilter);

////////////////////////////////////////////////////////////////////////////////

CMovePed::RagdollFilter::RagdollFilter()
: crFrameFilterBoneBasic(eFrameFilterType(crFrameFilter::kFrameFilterTypeFrameworkFilters+2))
{
	SetSignature(ATSTRINGHASH("RagdollFilter", 0x88D7A146));
}

////////////////////////////////////////////////////////////////////////////////

CMovePed::RagdollFilter::RagdollFilter(datResource& rsc)
: crFrameFilterBoneBasic(rsc)
{
}

////////////////////////////////////////////////////////////////////////////////

bool CMovePed::RagdollFilter::FilterDof(u8 track, u16 id, float& inoutWeight)
{
	return track!=kTrackBoneScale 
		// Allow root bone translation so that while ragdolled we always have valid data for the root DOF
		// The previous pose is used while ragdolled for any DOF that isn't controlled by the ragdoll rig
		// so since we're constantly posing by the previous pose we're essentially holding the last animated pose
		// for those DOFs
		// While ragdolled we overwrite the root DOF just before syncing the skeleton to the bounds so we really only
		// need the root translation to come through for those cases where we're accessing the skeleton pre-physics while ragdolled (i.e. CTaskGetUp)
		&& (track!=kTrackBoneTranslation || id==BONETAG_ROOT)
		// If we haven't initialized the weight set then we let all bone rotation through - otherwise only let through the bone rotation
		// for the bones allowed by the weight set
		&& (track!=kTrackBoneRotation || m_WeightSet == NULL || crFrameFilterBoneBasic::FilterDof(track, id, inoutWeight))
		&& track!=kTrackMoverRotation
		&& track!=kTrackMoverTranslation
		&& track!=kTrackMoverScale
		&& track!=kTrackVisemes
		&& track!=kTrackFacialControl 
		&& track!=kTrackFacialTranslation 
		&& track!=kTrackFacialRotation 
		&& track!=kTrackFacialScale
		&& track!=kTrackCameraRotation
		&& track!=kTrackCameraTranslation
		&& track!=kTrackCameraLimit
		&& track!=kTrackFirstPersonCameraWeight;
}

////////////////////////////////////////////////////////////////////////////////

u32 CMovePed::RagdollFilter::CalcSignature() const
{
	static const u32 s_RagdollFilterSignature = ATSTRINGHASH("RagdollFilter", 0x88D7A146);

	u32 signature = crFrameFilterBoneBasic::CalcSignature();
	signature = crc32(signature, (const u8*)&s_RagdollFilterSignature, sizeof(s_RagdollFilterSignature));

	return signature;
}

////////////////////////////////////////////////////////////////////////////////

CR_IMPLEMENT_FRAME_FILTER_TYPE(CMovePed::StaticFrameFilter, crFrameFilter::eFrameFilterType(crFrameFilter::kFrameFilterTypeFrameworkFilters+3), crFrameFilter);

////////////////////////////////////////////////////////////////////////////////

CMovePed::StaticFrameFilter::StaticFrameFilter()
: crFrameFilter(eFrameFilterType(crFrameFilter::kFrameFilterTypeFrameworkFilters+3))
{
	SetSignature(ATSTRINGHASH("StaticFrameFilter", 0x8E50781A));
}

////////////////////////////////////////////////////////////////////////////////

CMovePed::StaticFrameFilter::StaticFrameFilter(datResource& rsc)
: crFrameFilter(rsc)
{
}

////////////////////////////////////////////////////////////////////////////////

bool CMovePed::StaticFrameFilter::FilterDof(u8 track, u16, float&)
{
	return track!=kTrackFacialControl 
		&& track!=kTrackFacialTranslation 
		&& track!=kTrackFacialRotation 
		&& track!=kTrackFacialScale
		&& track!=kTrackVisemes;
}

////////////////////////////////////////////////////////////////////////////////

u32 CMovePed::StaticFrameFilter::CalcSignature() const
{
	return GetSignature();
}

////////////////////////////////////////////////////////////////////////////////

void CSimpleBlendIterator::Start()
{
	m_CumulativeBlend = 0.0f;
	m_CombinedEvents = 0;
}

float CSimpleBlendIterator::CalcVisibleBlend(crmtNode * pNode)
{
	
	if (pNode==m_ReferenceNode)
	{
		return 1.0f;
	}

	float weight = 1.0f;

	crmtNode* pParent = pNode->GetParent();

	while (pParent!=m_ReferenceNode)
	{
		if (pParent->GetNodeType()==crmtNode::kNodeBlend)
		{
			crmtNodePairWeighted* pPair = static_cast<crmtNodePairWeighted*>(pParent);

			float blendWeight = Clamp(pPair->GetWeight(), 0.0f, 1.0f);

			if (pNode==pPair->GetFirstChild())
			{
				weight*=(1.0f - blendWeight);
			}
			else
			{
				weight*=blendWeight;
			}
		}

		pNode = pParent;
		pParent = pNode->GetParent();
	}

	return weight;
}

// PURPOSE: Iterator visit override
void CSimpleBlendIterator::Visit(crmtNode& node)
{
	if (node.GetNodeType()==crmtNode::kNodeClip)
	{
		crClipPlayer& player = static_cast<crmtNodeClip*>(&node)->GetClipPlayer();
		const crClip* pClip = player.GetClip();
		if (pClip)
		{
			m_CumulativeBlend+=CalcVisibleBlend(&node);
		}
	}
}

////////////////////////////////////////////////////////////////////////////////

#if COMMERCE_CONTAINER
	FW_INSTANTIATE_CLASS_POOL_NO_FLEX_SPILLOVER(CMovePedPooledObject, MAXNOOFPEDS , 0.26f, atHashString("CMovePed",0x5047b2c4));
#else
	FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CMovePedPooledObject, MAXNOOFPEDS, 0.26f, atHashString("CMovePed",0x5047b2c4));
#endif

