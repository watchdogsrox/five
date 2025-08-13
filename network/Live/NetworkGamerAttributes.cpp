//
// NetworkGamerAttributes.cpp
//
// Copyright (C) 1999-2009 Rockstar Games. All Rights Reserved.
//

#include "rline/rlpresence.h"
#include "fwnet/netchannel.h"
#include "optimisations.h"
#include "NetworkGamerAttributes.h"
#include "Network/NetworkInterface.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, gamer_attr, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_gamer_attr

void 
CNetworkGamerAttributes::Init( )
{
	m_Status.Reset();
	m_GamerAttrs.Reset();
}

void
CNetworkGamerAttributes::Shutdown( )
{
	m_Pending = false;
	m_Failed = false;

	Cancel();

	m_Status.Reset();

	m_GamerAttrs.clear();
}

void
CNetworkGamerAttributes::Update( )
{
	if(!NetworkGamerAttributesSingleton::IsInstantiated())
		return;

	if (m_Pending)
	{
		if (!m_Status.Pending())
		{
			m_Pending = false;

			m_Failed = m_Status.Failed();

			if (m_Failed)
			{
				m_GamerAttrs[0].Clear();
				m_GamerAttrs.Delete(0);
			}

			m_Status.Reset();
		}
	}
}

void
CNetworkGamerAttributes::Cancel( )
{
	if (m_Status.Pending())
	{
		rlPresence::CancelQuery(&m_Status);
	}
}

bool
CNetworkGamerAttributes::Download(const rlGamerHandle& handle, const u64 attrFlags)
{
	if (!gnetVerify(handle.IsValid()))
		return false;

	if (!gnetVerify(!Exists(handle, NULL)))
		return false;

	if (!gnetVerify(!Pending()))
		return false;

	if (m_GamerAttrs.IsFull())
	{
		m_GamerAttrs.Top().Clear();
		m_GamerAttrs.Pop();
	}

	CAttrs ga;
	ga.m_Handle = handle;

	//const u32 attrCount = CountOnBits(attrFlags);
	//ga.m_Attrs.Resize(attrCount);

	if(attrFlags & Attr_PrimaryId      ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::PrimaryId.Name, ""));}
	if(attrFlags & Attr_PlayerAccountId) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::PlayerAccountId.Name, ""));}
	if(attrFlags & Attr_RockstarId     ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::RockstarId.Name, ""));}
	if(attrFlags & Attr_OnlineStatus   ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::OnlineStatus.Name, ""));}
	if(attrFlags & Attr_LastSeen       ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::LastSeen.Name, ""));}
	if(attrFlags & Attr_Publishers     ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::Publishers.Name, ""));}
	if(attrFlags & Attr_Subscribers    ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::Subscribers.Name, ""));}
	if(attrFlags & Attr_Titles         ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::Titles.Name, ""));}
	//Not reserved - can be set by clients.
	if(attrFlags & Attr_GamerTag         ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::GamerTag.Name, ""));}
	if(attrFlags & Attr_ScNickname       ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::ScNickname.Name, ""));}
	if(attrFlags & Attr_RelayIp          ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::RelayIp.Name, ""));}
	if(attrFlags & Attr_RelayPort        ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::RelayPort.Name, ""));}
	if(attrFlags & Attr_MappedIp         ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::MappedIp.Name, ""));}
	if(attrFlags & Attr_MappedPort       ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::MappedPort.Name, ""));}
	if(attrFlags & Attr_PeerAddress      ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::PeerAddress.Name, ""));}
	if(attrFlags & Attr_CrewId           ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::CrewId.Name, ""));}
	if(attrFlags & Attr_IsGameHost       ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::IsGameHost.Name, ""));}
	if(attrFlags & Attr_GameSessionToken ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::GameSessionToken.Name, ""));}
	if(attrFlags & Attr_GameSessionInfo  ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::GameSessionInfo.Name, ""));}
	if(attrFlags & Attr_IsGameJoinable   ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::IsGameJoinable.Name, ""));}
	if(attrFlags & Attr_IsPartyHost      ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::IsPartyHost.Name, ""));}
	if(attrFlags & Attr_PartySessionToken) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::PartySessionToken.Name, ""));}
	if(attrFlags & Attr_PartySessionInfo ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::PartySessionInfo.Name, ""));}
	if(attrFlags & Attr_IsPartyJoinable  ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::IsPartyJoinable.Name, ""));}
	if(attrFlags & Attr_MatchIds         ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::HostedMatchIds.Name, ""));}
	if(attrFlags & Attr_GameSessionId    ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::GameSessionId.Name, ""));}
	if(attrFlags & Attr_PartySessionId   ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::PartySessionId.Name, ""));}
	if(attrFlags & Attr_SigninTime		 ) {ga.m_Attrs.PushAndGrow(rlScPresenceAttribute(rlScAttributeId::SignInTime.Name, ""));}

	m_GamerAttrs.Insert(0) = ga;

	m_Status.Reset();

	m_Pending = rlPresence::GetAttributesForGamer(NetworkInterface::GetLocalGamerIndex()
													,m_GamerAttrs[0].m_Handle
													,m_GamerAttrs[0].m_Attrs.GetElements()
													,m_GamerAttrs[0].m_Attrs.GetCount()
													,&m_Status);

	m_Failed = !m_Pending;

	return m_Pending;
}

const CNetworkGamerAttributes::CAttrs*
CNetworkGamerAttributes::Get(const rlGamerHandle& handle) const
{
	int index = 0;

	if (gnetVerify(!Pending()) && gnetVerify(handle.IsValid()) && gnetVerify(Exists(handle, &index)))
	{
		if(gnetVerify(index>-1) && gnetVerify(index<m_GamerAttrs.GetCount()))
		{
			return &m_GamerAttrs[index];
		}
	}

	return 0;
}

bool
CNetworkGamerAttributes::Exists(const rlGamerHandle& handle, int* index) const
{
	bool exists = false;

	if (gnetVerify(handle.IsValid()))
	{
		for (int i=0; i<m_GamerAttrs.GetCount() && !exists; i++)
		{
			if (gnetVerify(m_GamerAttrs[i].m_Handle.IsValid()))
			{
				exists = (m_GamerAttrs[i].m_Handle == handle);

				if (exists && index)
				{
					*index = i;
				}
			}
		}
	}

	return exists;
}

// EOF
