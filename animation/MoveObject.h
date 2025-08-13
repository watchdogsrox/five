// 
// animation/MoveObject.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef MOVE_OBJECT_H
#define MOVE_OBJECT_H

#include "animation/move.h"
#include "task/System/TaskHelpers.h"


//////////////////////////////////////////////////////////////////////////
//	Main move network for animated objects
//	Supports three methods of playing back animation
//	1.	Playback of animations from anim helpers via its simple blend helper.
//		Anim helpers are owned by tasks or other game objects and support
//		playback of anims at multiple priorities, etc.
//	2.	Playback of a single clip via the generic clip network helper. This
//		provides a fire and forget interface for playback of a single clip
//		with cross blends. Tends to be used by script commands and other
//		systems which don't take direct ownership of the anim.
//	3.	A custom move network player can be played back on the object.
//////////////////////////////////////////////////////////////////////////

class CMoveObject : public CMove
{
public:

	// PURPOSE: 
	CMoveObject(CDynamicEntity& dynamicEntity);

	// PURPOSE:
	virtual ~CMoveObject();

	// PURPOSE:
	void Shutdown();

	// PURPOSE:
	void Update(float deltaTime);

	// PURPOSE: Main thread finish update, (called prior to output buffer swap, after motion tree finished)
	virtual void FinishUpdate();

	// PURPOSE: Main thread finish post update, (called after output buffer swap, after motion tree finished)
	virtual void PostFinishUpdate();

	// generic clip helper interface
	inline CGenericClipHelper& GetClipHelper() { return m_ClipHelper; }
	inline const CGenericClipHelper& GetClipHelper() const { return m_ClipHelper; }
	virtual CGenericClipHelper* FindGenericClipHelper(atHashWithStringNotFinal dict, atHashWithStringNotFinal anim);
	bool InsertGenericClipNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, u32 flags );

	// TODO - remove this once we know nothing is trying to play old anim system tasks on objects and vehicles 
	CSimpleBlendHelper* FindAppropriateBlendHelperForClipHelper(CClipHelper& ) { animAssertf(0, "CMoveObject no longer has a simple blend helper!"); return NULL; }
	virtual CClipHelper* FindClipHelper(atHashWithStringNotFinal UNUSED_PARAM(dict), atHashWithStringNotFinal UNUSED_PARAM(anim)) {return NULL;}
	virtual CClipHelper* FindClipHelper(s32 UNUSED_PARAM(dictIndex), u32 UNUSED_PARAM(animHash)) {return NULL;}

	// PURPOSE: return the external override frame used to set custom dofs.
	crFrame* GetExternalOverrideFrame();

	//////////////////////////////////////////////////////////////////////////
	//	Main MoVE network slot
	//////////////////////////////////////////////////////////////////////////
	// PURPOSE: Sets the network to be played on the primary slot from a move network helper
	void SetNetwork( CMoveNetworkHelper& helper, float blendDuration );

	// PURPOSE: Sets the network to be played on the primary slot 
	void SetNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration );

	// PURPOSE: Stop the network belonging to this move network helper if it is playing
	void ClearNetwork( CMoveNetworkHelper& helper, float blendDuration );

	// PURPOSE: Stop this specific network if it exists in the network slot
	void ClearNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration );
	
	// PURPOSE: Stop the network currently inserted into the network slot
	void ClearNetwork( float blendDuration = 0.0f);

	//////////////////////////////////////////////////////////////////////////
	// Secondary MoVE network slot
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: Sets the network to be played on the primary slot from a move network helper
	void SetSecondaryNetwork( CMoveNetworkHelper& helper, float blendDuration );

	// PURPOSE: Sets the network to be played on the primary slot 
	void SetSecondaryNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration );

	// PURPOSE: Stop the network belonging to this move network helper if it is playing
	void ClearSecondaryNetwork( CMoveNetworkHelper& helper, float blendDuration );

	// PURPOSE: Stop this specific network if it exists in the network slot
	void ClearSecondaryNetwork( fwMoveNetworkPlayer* pPlayer, float blendDuration );

	// PURPOSE: Stop the network currently inserted into the network slot
	void ClearSecondaryNetwork( float blendDuration = 0.0f);

	//////////////////////////////////////////////////////////////////////////
	//	Helper functions
	//////////////////////////////////////////////////////////////////////////

	// PURPOSE: Returns true if a move network player is inserted
	bool IsAnimating() const { return m_MainNetwork.GetNetworkPlayer()!=NULL || m_SecondaryNetwork.GetNetworkPlayer()!=NULL; }

	// PURPOSE: Returns true if the provided move network player is inserted
	bool IsPlayingNetwork(fwMoveNetworkPlayer* pPlayer) const { return m_MainNetwork.GetNetworkPlayer()==pPlayer || m_SecondaryNetwork.GetNetworkPlayer()==pPlayer; }

	// PURPOSE: Set the synced scene network
	bool SetSynchronizedSceneNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration)
	{
		SetNetwork(pPlayer, blendDuration);
		return true;
	}

	void ClearSynchronizedSceneNetwork(fwMoveNetworkPlayer* pPlayer, float blendDuration, bool UNUSED_PARAM(tagSyncOut = false) )
	{
		ClearNetwork(pPlayer, blendDuration );
	}

protected:

	inline fwMvNetworkId GetNetworkId() { static fwMvNetworkId s_networkId("NetworkSlot",0x35FD5CD); return s_networkId;  }

protected:

	// helper to support simple playback of one clip
	CGenericClipHelper m_ClipHelper;

	// The main network slot
	CNetworkSlot m_MainNetwork;

	// The secondary network slot
	CNetworkSlot m_SecondaryNetwork;

	// The external override frame used to set expression dofs / etc
	crFrame* m_pExternalOverrideFrame;

	CDynamicEntity* m_pEntity;

	static fwMvRequestId ms_SecondaryTransitionId;
	static fwMvFlagId ms_SecondaryOnId;
	static fwMvFlagId ms_SecondaryOffId;
	static fwMvFloatId ms_SecondaryDurationId;

	static fwMvFlagId ms_SecondaryMechanismId;
	static fwMvFrameId ms_ExternalOverrideFrameId;
};

////////////////////////////////////////////////////////////////////////////////

class CMoveObjectPooledObject : public CMoveObject
{
public:

	CMoveObjectPooledObject(CDynamicEntity& dynamicEntity)
		: CMoveObject(dynamicEntity) 
	{
	}

	FW_REGISTER_CLASS_POOL(CMoveObjectPooledObject);
};

#endif // MOVE_OBJECT_H
