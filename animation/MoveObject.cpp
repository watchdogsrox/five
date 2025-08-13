#include "MoveObject.h"
#include "fwsys/config.h"

ANIM_OPTIMISATIONS()

fwMvRequestId CMoveObject::ms_SecondaryTransitionId("Secondary_Transition",0x33A2BFC3);
fwMvFlagId CMoveObject::ms_SecondaryOnId("Secondary_On",0x804B51C9);
fwMvFlagId CMoveObject::ms_SecondaryOffId("Secondary_Off",0x8BA859D1);
fwMvFloatId CMoveObject::ms_SecondaryDurationId("Secondary_Duration",0x83AEB18B);
fwMvFlagId CMoveObject::ms_SecondaryMechanismId("Secondary_Mechanism",0xF1569C6);
fwMvFrameId CMoveObject::ms_ExternalOverrideFrameId("ExternalOverrideFrame",0xbf29a13c);


CMoveObject::CMoveObject(CDynamicEntity& dynamicEntity)
: CMove(dynamicEntity)
, m_pExternalOverrideFrame(NULL)
, m_pEntity(&dynamicEntity)
{
	m_ClipHelper.SetInsertCallback(MakeFunctorRet(*this, &CMoveObject::InsertGenericClipNetwork));
}

CMoveObject::~CMoveObject()
{
	Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

void CMoveObject::Shutdown()
{
	if (m_pExternalOverrideFrame)
	{
		m_pExternalOverrideFrame->Release();
		m_pExternalOverrideFrame = NULL;
	}

	m_pEntity = NULL;

	CMove::Shutdown();
}

////////////////////////////////////////////////////////////////////////////////

void CMoveObject::Update(float UNUSED_PARAM(deltaTime))
{
	m_ClipHelper.Update();
}

////////////////////////////////////////////////////////////////////////////////

void CMoveObject::FinishUpdate()
{
	if (m_ClipHelper.IsNetworkActive())
	{
		m_ClipHelper.SetLastPhase(m_ClipHelper.GetPhase());
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::PostFinishUpdate()
{

}

//////////////////////////////////////////////////////////////////////////

bool CMoveObject::InsertGenericClipNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration, u32 )
{
	SetNetwork(pPlayer, blendDuration);
	return true;
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::SetNetwork( CMoveNetworkHelper& helper, float blendDuration )
{
	fwMoveNetworkPlayer* pPlayer = helper.GetNetworkPlayer();
	animAssertf(pPlayer, "The MoVE network helper has not created its player!");
	SetNetwork(pPlayer, blendDuration);
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::SetNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration )
{
	//send the move signals to insert the subnetwork into the network slot

	if (pPlayer)
	{
		SetSubNetwork( GetNetworkId(), pPlayer->GetSubNetwork() );
		pPlayer->SetSubNetworkBlendDuration(blendDuration);
	}
	else
	{
		SetSubNetwork( GetNetworkId(), NULL );
		SetSubNetworkBlendOutDuration(blendDuration);
	}

	m_MainNetwork.SetNetworkPlayer(pPlayer);
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::ClearNetwork( CMoveNetworkHelper& helper, float blendDuration )
{
	// Don't clear the network if our helper's player is not the one being played
	if (helper.IsNetworkActive() && helper.GetNetworkPlayer()==m_MainNetwork.GetNetworkPlayer())
	{
		return SetNetwork(NULL, blendDuration);
	}
}

//////////////////////////////////////////////////////////////////////////


void CMoveObject::ClearNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration )
{
	// Don't clear the network if our player is not the one being played
	if (m_MainNetwork.GetNetworkPlayer() == pPlayer)
	{
		SetNetwork(NULL, blendDuration);
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::ClearNetwork( float blendDuration )
{
	SetNetwork(NULL, blendDuration);
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::SetSecondaryNetwork( CMoveNetworkHelper& helper, float blendDuration )
{
	fwMoveNetworkPlayer* pPlayer = helper.GetNetworkPlayer();
	animAssertf(pPlayer, "The MoVE network helper has not created its player!");
	SetSecondaryNetwork(pPlayer, blendDuration);
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::SetSecondaryNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration )
{
	static fwMvNetworkId s_secondaryNetworkId("SecondarySlot",0x3E24A79B);

	// trigger the transition
	BroadcastRequest(ms_SecondaryTransitionId);
	SetFloat(ms_SecondaryDurationId, blendDuration);

	//send the move signals to insert the subnetwork into the network slot
	if (pPlayer)
	{
		SetSubNetwork( s_secondaryNetworkId, pPlayer->GetSubNetwork() );
		SetFlag(ms_SecondaryOffId, false);
		SetFlag(ms_SecondaryOnId, true);
		SetFlag(ms_SecondaryMechanismId, false);
	}
	else
	{
		SetFlag(ms_SecondaryOffId, true);
		SetFlag(ms_SecondaryOnId, false);
		SetFlag(ms_SecondaryMechanismId, false);
	}

	m_SecondaryNetwork.SetNetworkPlayer(pPlayer);
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::ClearSecondaryNetwork( CMoveNetworkHelper& helper, float blendDuration )
{
	if (helper.IsNetworkActive() && helper.GetNetworkPlayer()==m_SecondaryNetwork.GetNetworkPlayer())
	{
		return SetSecondaryNetwork(NULL, blendDuration);
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::ClearSecondaryNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration )
{
	if ( animVerifyf(m_SecondaryNetwork.GetNetworkPlayer() == pPlayer, "Network player not inserted!") )
	{
		SetSecondaryNetwork(NULL, blendDuration);
	}
}

//////////////////////////////////////////////////////////////////////////

void CMoveObject::ClearSecondaryNetwork( float blendDuration )
{
	SetSecondaryNetwork(NULL, blendDuration);
}

//////////////////////////////////////////////////////////////////////////

CGenericClipHelper* CMoveObject::FindGenericClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim)
{
	s32 dictIndex = fwAnimManager::FindSlotFromHashKey(dict.GetHash()).Get();
	if (m_ClipHelper.IsClipPlaying(dictIndex, anim.GetHash()))
	{
		return &m_ClipHelper;
	}
	return NULL;
}

//////////////////////////////////////////////////////////////////////////

crFrame* CMoveObject::GetExternalOverrideFrame()
{
	// lazily initialise the frame
	if (!m_pExternalOverrideFrame)
	{
		// On future projects this rage_new should be swapped to AllocateFrame/factory style allocation so that it's allocated from a private heap.
		m_pExternalOverrideFrame = rage_new crFrame(*m_pEntity->GetAnimDirector()->GetFrameBuffer().GetFrameData());
		SetFrame(ms_ExternalOverrideFrameId, m_pExternalOverrideFrame);
	}	

	return m_pExternalOverrideFrame;

}

//////////////////////////////////////////////////////////////////////////

FW_INSTANTIATE_CLASS_POOL_SPILLOVER(CMoveObjectPooledObject, CONFIGURED_FROM_FILE, 0.59f, atHashString("CMoveObject",0x72aaac3f));
