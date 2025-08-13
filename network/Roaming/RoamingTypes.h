//
// name:        RoamingTypes.h
// description: Common types and defines used by the roaming code
// written by:  John Gurney
//

#ifndef ROAMING_TYPES_H
#define ROAMING_TYPES_H

#include "data/bitbuffer.h"
#include "fwnet/netserialisers.h"

static const unsigned MAX_BUBBLES				= 10;
static const unsigned INVALID_BUBBLE_ID			= MAX_BUBBLES;
static const unsigned INVALID_BUBBLE_PLAYER_ID	= MAX_NUM_PHYSICAL_PLAYERS;

// Holds membership info for a player that is a member of a roaming bubble 
class CRoamingBubbleMemberInfo
{
public:

	static const unsigned SIZEOF_BUBBLE_ID = datBitsNeeded<MAX_BUBBLES>::COUNT;
	static const unsigned SIZEOF_PLAYER_ID = datBitsNeeded<MAX_NUM_PHYSICAL_PLAYERS>::COUNT;

public:

	CRoamingBubbleMemberInfo() : m_BubbleID(INVALID_BUBBLE_ID), m_PlayerID(INVALID_BUBBLE_PLAYER_ID) {}
	CRoamingBubbleMemberInfo(unsigned bubbleID, unsigned playerID) : m_BubbleID(bubbleID), m_PlayerID(playerID) {}
	CRoamingBubbleMemberInfo(const CRoamingBubbleMemberInfo& info) { Copy(info); }

	bool	 IsValid() const	 { return m_BubbleID != INVALID_BUBBLE_ID; }
    bool     IsEqualTo(const CRoamingBubbleMemberInfo &info) const { return (info.m_BubbleID == m_BubbleID && info.m_PlayerID == m_PlayerID); }
	void	 Invalidate()		 { m_BubbleID = INVALID_BUBBLE_ID; m_PlayerID = INVALID_BUBBLE_PLAYER_ID; }
	unsigned GetBubbleID() const { return m_BubbleID; }
	unsigned GetPlayerID() const { return m_PlayerID; }

	void Copy(const CRoamingBubbleMemberInfo& info) { m_BubbleID = info.m_BubbleID; m_PlayerID = info.m_PlayerID; }

	void Serialise(CSyncDataBase& serialiser)
	{
		SERIALISE_UNSIGNED(serialiser, m_BubbleID, SIZEOF_BUBBLE_ID, "Bubble ID");
		SERIALISE_UNSIGNED(serialiser, m_PlayerID, SIZEOF_PLAYER_ID, "Player ID");
	}

	template <class MsgBuffer> 
	bool SerialiseMsg(MsgBuffer &bb)
	{
		return (bb.SerUns(m_BubbleID, SIZEOF_BUBBLE_ID) &&
				bb.SerUns(m_PlayerID, SIZEOF_PLAYER_ID));
	}

	template <class MsgBuffer> 
	bool SerialiseMsg(MsgBuffer &bb) const
	{
		return const_cast<CRoamingBubbleMemberInfo*>(this)->SerialiseMsg(bb);
	}

protected:

	unsigned m_BubbleID;         // The ID of the bubble the player is currently a member of
	unsigned m_PlayerID;         // The ID assigned to the player by the owner of the bubble

private:

	CRoamingBubbleMemberInfo& operator=(const CRoamingBubbleMemberInfo&);
};

#endif // ROAMING_TYPES_H
