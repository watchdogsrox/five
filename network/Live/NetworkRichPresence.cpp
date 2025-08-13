//
// NetworkRichPresence.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//


// --- Include Files ------------------------------------------------------------

//Rage Headers
#include "diag/seh.h"
#include "snet/session.h"
#include "rline/rlgamerinfo.h"
#include "rline/rlpresence.h"
#include "net/status.h"
#include "system/memory.h"

//Framework headers
#include "fwnet/netchannel.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"

//Network Headers
#include "Network/Network.h"
#include "NetworkRichPresence.h"
#include "Network/NetworkInterface.h"
#include "Network/xlast/presenceutil.h"

namespace rage
{

// --- Defines ------------------------------------------------------------------

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, rich_presence, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_rich_presence

FW_INSTANTIATE_BASECLASS_POOL(richPresenceMgr::PresenceMsg, MAX_PRESENCE_MSG, atHashString("PresenceMsg",0x675a0a1), sizeof(PresenceMsg));
FW_INSTANTIATE_CLASS_POOL(richPresenceMgr::atDPresenceNode, MAX_PRESENCE_MSG, atHashString("atDPresenceNode",0x978489a9));

//Presence can only be activated every 1 minute
static const unsigned DEFAULT_PRESENCE_INTERVAL = 1*60*1000;
static unsigned s_ActivationTimer = 0;

// --- Structure Implementation -------------------------------------------------

void
richPresenceMgr::PresenceMsg::Init(ORBIS_ONLY( const char* data ))
{
#if RSG_NP
	sysMemSet(m_Data, 0, sizeof(m_Data));

	if (data && strlen(data) <= RL_PRESENCE_MAX_BUF_SIZE)
	{
		formatf(m_Data, RL_PRESENCE_MAX_BUF_SIZE, data);
	}
#endif

#if RSG_DURANGO
	gnetAssert(!m_Schema);
	m_Schema = rage_new rlSchema();
	m_Schema->Clear();
#endif // RSG_DURANGO

#if RSG_DURANGO
	m_PresenceStr[0] = '\0';
	m_NumFields = 0;
	sysMemSet(m_FieldStat, 0, sizeof(m_FieldStat));
#endif

	gnetAssert(!m_MyStatus);
	m_MyStatus = rage_new netStatus();
	m_MyStatus->Reset();
}

void
richPresenceMgr::PresenceMsg::Shutdown()
{
#if RSG_DURANGO
	if (m_Schema)
	{
		m_Schema->Clear();
		delete m_Schema;
	}
#endif

	if (m_MyStatus)
	{
		m_MyStatus->Reset();
		delete m_MyStatus;
	}
}

#if RSG_DURANGO

bool
richPresenceMgr::PresenceMsg::ActivatePresence(ActivatePresenceCallback& pCB DURANGO_ONLY(,ActivatePresenceContextCallback& cCB))
{
	bool result = false;

	if (!gnetVerify(pCB.IsBound()))
	{
		gnetError("Failed to Activate Presence - %d - FAILED presence callback is not bound", m_Id);
		return result;
	}

#if RSG_DURANGO
	if (!gnetVerify(cCB.IsBound()))
	{
		gnetError("Failed to Activate Presence - %d - FAILED context callback is not bound", m_Id);
		return result;
	}
#endif

	if (!gnetVerify(rlPresence::IsOnline(m_LocalGamerInfo)))
	{
		gnetError("Failed to Activate Presence - %d - FAILED, Gamer=%d not online", m_Id, m_LocalGamerInfo);
		return result;
	}

	rlGamerInfo gi;
	if (!gnetVerify(rlPresence::GetGamerInfo(m_LocalGamerInfo, &gi)))
	{
		gnetError("Failed to Activate Presence - %d - FAILED, Gamer=%d does not have a gamer info.", m_Id, m_LocalGamerInfo);
		return result;
	}

	if(!gnetVerify(m_Schema))
	{
		m_Schema = rage_new rlSchema();
		m_Schema->Clear();
	}

	if(!gnetVerify(m_MyStatus))
	{
		m_MyStatus = rage_new netStatus();
	}

	m_MyStatus->Reset();

#if RSG_DURANGO
	result = cCB(gi, m_FieldStat, m_NumFields) && pCB(gi, m_PresenceStr, m_MyStatus);
#else
	result = pCB(gi, m_Id, *m_Schema, m_MyStatus);
#endif

	if (!result)
	{
		m_MyStatus->SetPending();
		m_MyStatus->SetFailed();
	}

	gnetDebug1("Activate Presence - %d - %s", m_Id, (result ? "SUCCEDED" : "FAILED"));

	return result;
}

#elif RSG_NP

bool
richPresenceMgr::PresenceMsg::ActivatePresence(ActivatePresenceCallback& pCB)
{
	bool result = false;

	if (gnetVerify(pCB.IsBound()))
	{
		if(!gnetVerify(m_MyStatus))
		{
			m_MyStatus = rage_new netStatus();
		}
		m_MyStatus->Reset();
		m_MyStatus->SetPending();

		gnetAssert(strlen(m_Data) > 0);
		result = pCB(m_Data);

		gnetDebug1("Activate Presence - %d - %s", m_Id, (result ? "SUCCEDED" : "FAILED"));

		if (result)
		{
			m_MyStatus->SetSucceeded();
		}
		else
		{
			m_MyStatus->SetFailed();
		}
	}

	return result;
}

#else

bool richPresenceMgr::PresenceMsg::ActivatePresence(ActivatePresenceCallback& )
{
	return false;
}

#endif // RSG_DURANGO

bool
richPresenceMgr::PresenceMsg::Pending()
{
	return m_MyStatus && m_MyStatus->Pending();
}

int
richPresenceMgr::PresenceMsg::GetStatus()
{
	if (m_MyStatus)
	{
		return m_MyStatus->GetStatus();
	}

	return netStatus::NET_STATUS_NONE;
}

// --- Public Class Implementation ----------------------------------------------

richPresenceMgr::~richPresenceMgr()
{
	Shutdown(SHUTDOWN_CORE);
}

void
richPresenceMgr::Init(ActivatePresenceCallback& pCB DURANGO_ONLY(, ActivatePresenceContextCallback& cCB), sysMemAllocator* allocator)
{
	if (m_Initialized)
		return;

	gnetAssert(pCB.IsBound());
#if RSG_DURANGO
	gnetAssert(cCB.IsBound());
#endif

	gnetAssert(allocator);
	if(!allocator)
		return;

	m_Initialized = true;

	m_Allocator = allocator;

	sysMemAllocator* previousallocator = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(*m_Allocator);

	PresenceMsg::InitPool( MEMBUCKET_NETWORK );
	atDPresenceNode::InitPool( MEMBUCKET_NETWORK );

	sysMemAllocator::SetCurrent(*previousallocator);

	m_ActivatePresenceCB = pCB;

#if RSG_DURANGO
	m_ActivatePresenceContextCB = cCB;
#endif

	s_ActivationTimer = 0;
}

void
richPresenceMgr::Shutdown(const u32 shutdownMode)
{
	if(!m_Initialized)
		return;

	if (shutdownMode != SHUTDOWN_CORE)
		return;

	atDNode< PresenceMsg*, datBase >* node = m_PresenceList.GetHead();
	while (node)
	{
		if (node->Data)
		{
			delete node->Data;
			node->Data = NULL;
		}
		node = node->GetNext();
	}
	m_PresenceList.DeleteAll();

	sysMemAllocator* previousallocator = &sysMemAllocator::GetCurrent();
	sysMemAllocator::SetCurrent(*m_Allocator);

	PresenceMsg::ShutdownPool();
	atDPresenceNode::ShutdownPool();

	sysMemAllocator::SetCurrent(*previousallocator);
	previousallocator = 0;

	m_Allocator = NULL;

	m_ActivatePresenceCB.Unbind();

#if RSG_DURANGO
	m_ActivatePresenceContextCB.Unbind();
#endif

	m_Initialized = false;

	s_ActivationTimer = 0;
}


void
richPresenceMgr::Update()
{
	if(!m_Initialized)
		return;

	atDNode< PresenceMsg*, datBase >* node = m_PresenceList.GetHead();

	//Check for pending requests.
	while (node)
	{
		PresenceMsg* presence = node->Data;
		if (presence && presence->Pending())
		{
			return;
		}

		node = node->GetNext();
	}

	//Every minute activate a queued Presence.
	if (fwTimer::GetSystemTimeInMilliseconds() > s_ActivationTimer)
	{
		node = m_PresenceList.GetHead();
		while (node)
		{
			PresenceMsg* presence = node->Data;
			if (presence && netStatus::NET_STATUS_NONE == presence->GetStatus())
			{
#if RSG_DURANGO
				gnetVerify( presence->ActivatePresence(m_ActivatePresenceCB, m_ActivatePresenceContextCB) );
#else
				gnetVerify( presence->ActivatePresence(m_ActivatePresenceCB));
#endif
				s_ActivationTimer = fwTimer::GetSystemTimeInMilliseconds() + DEFAULT_PRESENCE_INTERVAL;
				node = NULL;
			}
			else
			{
				node = node->GetNext();
			}
		}
	}

	//Deletes finished presence messages and frees space in the pool.
	node = m_PresenceList.GetHead();
	atDNode< PresenceMsg*, datBase >* nextNode = 0;
	while (node)
	{
		nextNode = node->GetNext();

		PresenceMsg* presence = node->Data;

		//Delete finished Presence
		if (gnetVerify(presence)
			&& !presence->Pending()
			&& netStatus::NET_STATUS_NONE != presence->GetStatus())
		{

			gnetDebug1("Creating space in Presence queue. Removed Presence id=\"%d\"", presence->GetID());

			delete presence;
			m_PresenceList.PopNode(*node);
			delete node;
		}

		node = nextNode;
	}
}

#if RSG_DURANGO

void
richPresenceMgr::SetPresenceStatus(const int localGamerInfo, 
								   const int id, 
								   const int* fieldIds, 
								   const DURANGO_ONLY(richPresenceFieldStat*) fieldData, 
								   const unsigned numFields)
{
	PresenceMsg* pPresence = NULL;

	rtry
	{
		rverify(m_Initialized,
			catchall,
			gnetWarning("Failed to set rich presence: Manager Not initialized."));

		rlGamerInfo gi;

		rverify(player_schema::PresenceUtil::GetCountOfFields(id) == (int)numFields,
			catchall,
			gnetWarning("Invalid number of fields %d, it should be %d.", numFields, player_schema::PresenceUtil::GetCountOfFields(id)));

		rverify(rlPresence::IsOnline(localGamerInfo),
			catchall,
			gnetWarning("Failed to set rich presence: Gamer is offline."));

		rverify(rlPresence::GetGamerInfo(localGamerInfo, &gi),
			catchall,
			gnetWarning("Failed to set rich presence: Invalid Gamer Info."));

		rverify((CreateFreeSpaces()),
			catchall,
			gnetWarning("Failed to set rich presence: Reached maximun number \"%d\" of queued rich presence messages.", MAX_PRESENCE_MSG));

		rverify(0 != (pPresence = rage_new PresenceMsg(id, localGamerInfo)), catchall,);

		//Setup all fields.
		if (fieldIds && fieldData && 0 < numFields)
		{
			for (unsigned i=0; i<numFields; i++)
			{
				rverify (-1 != pPresence->GetSchemaP()->AddField(fieldIds[i]),
					catchall,
					gnetWarning("Failed to set rich presence: Failed to add Field id=\"%d\".", fieldIds[i]));

				rverify(pPresence->AddPresenceFieldStat(&fieldData[i]), catchall, );
				rverify (pPresence->GetSchemaP()->SetFieldData(i, &fieldData[i], sizeof(int)),
					catchall,
					gnetWarning("Failed to set rich presence: Failed to set data for Field index=%d id=\"%d\".", i, fieldIds[i]));
			}
		}

		gnetDebug1("Set rich presence GamerIndex=\"%d\" id=\"%s:%d\" numFields=\"%u\"", localGamerInfo, player_schema::PresenceUtil::GetLabel(id), id, numFields);

		pPresence->SetPresenceString(player_schema::PresenceUtil::GetLabel(id));

#if !__NO_OUTPUT
		for (int i=0; i<numFields;i++)
			gnetDebug1(" ...... index=\"%d\" m_FieldStat=\"%s\", m_PresenceId=\"%d\".", i, fieldData[i].m_FieldStat, fieldData[i].m_PresenceId);
#endif

		//Append new node
		atDNode< PresenceMsg*, datBase >* pNode = rage_new atDPresenceNode;
		pNode->Data = pPresence;
		m_PresenceList.Append(*pNode);
	}
	rcatchall
	{
		if (pPresence)
		{
			delete pPresence;
		}
	}
}

#elif RSG_NP

void
richPresenceMgr::SetPresenceStatus(const int id, const char* status)
{
	PresenceMsg* pPresence = NULL;

	rtry
	{
		rverify(m_Initialized,
			catchall,
			gnetWarning("Failed to set rich presence: Manager Not initialized."));

		rverify(status,
			catchall,
			gnetWarning("Failed to set rich presence: Invalid Data."));

		rverify(strlen(status) < RL_PRESENCE_MAX_BUF_SIZE,
			catchall,
			gnetWarning("Failed to set rich presence: Invalid presence size \"%u\", max size is %u.", ustrlen(status), RL_PRESENCE_MAX_BUF_SIZE));

		rverify((CreateFreeSpaces()),
			catchall,
			gnetWarning("Failed to set rich presence: Reached maximun number \"%d\" of queued rich presence messages.", MAX_PRESENCE_MSG));

		rverify(0 != (pPresence = rage_new PresenceMsg(id, status)), catchall,);

		gnetDebug1("Set rich presence id=\"%s:%d\" status=\"%s\"", player_schema::PresenceUtil::GetLabel(id), id, status);

		//Append new node
		atDNode< PresenceMsg*, datBase >* pNode = rage_new atDPresenceNode;
		pNode->Data = pPresence;
		m_PresenceList.Append(*pNode);
	}
	rcatchall
	{
		if (pPresence)
		{
			delete pPresence;
		}
	}
}

#endif // RSG_DURANGO

bool
richPresenceMgr::CreateFreeSpaces( )
{
	bool result = (PresenceMsg::GetPool()->GetNoOfFreeSpaces() != 0);

	if (!result)
	{
		gnetDebug1("Creating free spaces: ");

		atDNode<PresenceMsg*, datBase>* pNode     = m_PresenceList.GetHead();
		atDNode<PresenceMsg*, datBase>* pNextNode = 0;

		while (pNode)
		{
			pNextNode = pNode->GetNext();

			PresenceMsg* pEvent = pNode->Data;

			gnetDebug1("Rich presence id=\"%s:%d\" status=\"%s\", \"%s\"", player_schema::PresenceUtil::GetLabel(pEvent->GetID()), pEvent->GetID(), pEvent->Pending() ? "Pending" : "Not Pending", pEvent->Pending() ? "" : "deleted");

			//Find an event which can be removed
			if (!pEvent->Pending())
			{
				delete pEvent;
				m_PresenceList.PopNode(*pNode);
				delete pNode;
			}

			pNode = pNextNode;
		}

		result = (PresenceMsg::GetPool()->GetNoOfFreeSpaces() != 0);
	}

	return result;
}

#if RSG_DURANGO
void richPresenceMgr::PresenceMsg::SetPresenceString(const char * pString) 
{ 
	safecpy(m_PresenceStr, pString); 
}

bool richPresenceMgr::PresenceMsg::AddPresenceFieldStat(const richPresenceFieldStat* fieldStat)
{
	bool bSuccess = false;

	rtry
	{
		rverify(m_NumFields < RL_PRESENCE_MAX_FIELDS, catchall, );
		m_FieldStat[m_NumFields].m_PresenceId = fieldStat->m_PresenceId;
		safecpy(m_FieldStat[m_NumFields].m_FieldStat, fieldStat->m_FieldStat, RL_PRESENCE_MAX_STAT_SIZE);
		m_NumFields++;
		bSuccess = true;
	}
	rcatchall
	{

	}

	return bSuccess;
}
#endif

} // namespace rage

// eof

