//
// NetworkGamerAttributes.h
//
// Copyright (C) 1999-2009 Rockstar Games. All Rights Reserved.
//

#ifndef __NETWORK_GAMER_ATTRIBUTES_H
#define __NETWORK_GAMER_ATTRIBUTES_H

#include "system/bit.h"
#include "atl/array.h"
#include "atl/singleton.h"
#include "net/status.h"
#include "rline/rlgamerinfo.h"

class CNetworkGamerAttributes
{
public:
	static const u32 MAX_SCRIPT_GAMER_ATTR = 3;
	static const u32 MAX_NUM_GAMERS        = 5;

public:
	enum sAttributes
	{
		 Attr_PrimaryId         = BIT(0)
		,Attr_PlayerAccountId   = BIT(1)
		,Attr_RockstarId        = BIT(2)
		,Attr_OnlineStatus      = BIT(3)
		,Attr_LastSeen          = BIT(4)
		,Attr_Publishers        = BIT(5)
		,Attr_Subscribers       = BIT(6)
		,Attr_Titles            = BIT(7)
		//Not reserved - can be set by clients.
		,Attr_GamerTag          = BIT(8)  //Can be used for script queries
		,Attr_ScNickname        = BIT(9)  //Can be used for script queries
		,Attr_RelayIp           = BIT(10)
		,Attr_RelayPort         = BIT(11)
		,Attr_MappedIp          = BIT(12)
		,Attr_MappedPort        = BIT(13)
		,Attr_PeerAddress       = BIT(14)
		,Attr_CrewId            = BIT(15) //Can be used for script queries
		,Attr_IsGameHost        = BIT(16)
		,Attr_GameSessionToken  = BIT(17)
		,Attr_GameSessionInfo   = BIT(18)
		,Attr_IsGameJoinable    = BIT(19)
		,Attr_IsPartyHost       = BIT(20)
		,Attr_PartySessionToken = BIT(21)
		,Attr_PartySessionInfo  = BIT(22)
		,Attr_IsPartyJoinable   = BIT(23)
		,Attr_MatchIds          = BIT(24)
		,Attr_GameSessionId     = BIT(25)
		,Attr_PartySessionId    = BIT(26)
		,Attr_SigninTime		= BIT(27)
	};


	class CAttrs
	{
		friend class CNetworkGamerAttributes;

	public:
		typedef atArray< rlScPresenceAttribute > AttrsList;

	private:
		AttrsList     m_Attrs;
		rlGamerHandle m_Handle;

	public:
		CAttrs()  {Clear();}
		~CAttrs() {Clear();}

		void Clear()
		{
			m_Attrs.clear();
			m_Attrs.Reset();
			m_Handle.Clear();
		}

		AttrsList::const_iterator  AttrsIterBegin() const {return m_Attrs.begin();}
		AttrsList::const_iterator  AttrsIterEnd()   const {return m_Attrs.end();}
	};

public:
	typedef atFixedArray< CAttrs, MAX_NUM_GAMERS > GamerAttrsList;

private:
	bool            m_Pending;
	bool            m_Failed;
	netStatus       m_Status;
	GamerAttrsList  m_GamerAttrs;

public:
	CNetworkGamerAttributes() : m_Pending(false), m_Failed(false)
	{;}
	~CNetworkGamerAttributes() 
	{
		Shutdown();
	}

	void       Init( );
	void   Shutdown( );
	void     Update( );
	void     Cancel( );

	bool    Pending( ) const {return m_Pending;}
	bool     Failed( ) const {return m_Failed;}

	bool   Download(const rlGamerHandle& handle, const u64 attrFlags);

	const CAttrs*    Get(const rlGamerHandle& handle)             const;
	bool          Exists(const rlGamerHandle& handle, int* index) const;

	GamerAttrsList::const_iterator  IterBegin() const {gnetAssert(!Pending());return m_GamerAttrs.begin();}
	GamerAttrsList::const_iterator    IterEnd() const {gnetAssert(!Pending());return m_GamerAttrs.end();}
};

typedef atSingleton < CNetworkGamerAttributes > NetworkGamerAttributesSingleton;
#define NETWORK_GAMERS_ATTRS NetworkGamerAttributesSingleton::GetInstance()

#endif // __NETWORK_GAMER_ATTRIBUTES_H