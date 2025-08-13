#include "ReplayAttachmentManager.h"

#if GTA_REPLAY

#include "ReplayController.h"
#include "ReplayInternal.h"

//////////////////////////////////////////////////////////////////////////
void CPacketEntityAttachDetach::Store(ReplayAttachmentManager::Pairings& pairings)
{
	CPacketEvent::Store(PACKETID_ENTITYATTACHDETACH, sizeof(CPacketEntityAttachDetach));
	CPacketEntityInfo::Store(/*NoEntityID*/);

	m_basePacketSize = sizeof(CPacketEntityAttachDetach);

	Assign(m_numAttachments, pairings.GetAttachments().GetNumUsed());
	Assign(m_numDetachments, pairings.GetDetachments().GetNumUsed());

	CReplayID* ptr = GetAttachments();
	tPairingMap::ConstIterator attachIter = pairings.GetAttachments().CreateIterator();
	while(!attachIter.AtEnd())
	{
		*ptr = attachIter.GetKey();
		++ptr;
		*ptr = attachIter.GetData();
		++ptr;
		attachIter.Next();
	}

	replayAssert(GetDetachments() == ptr);
	tPairingMap::ConstIterator detachIter = pairings.GetDetachments().CreateIterator();
	while(!detachIter.AtEnd())
	{
		*ptr = detachIter.GetKey();
		++ptr;
		*ptr = detachIter.GetData();
		++ptr;
		detachIter.Next();
	}

	AddToPacketSize(sizeof(CReplayID) * (m_numAttachments + m_numDetachments) * 2);
}


//////////////////////////////////////////////////////////////////////////
void CPacketEntityAttachDetach::Extract(const CEventInfo<void>& eventInfo) const
{

	typedef void (ReplayAttachmentManager::*AttachDetachFunc)(const CReplayID&, const CReplayID&);
	AttachDetachFunc pForAttachFunc = NULL;
	AttachDetachFunc pForDetachFunc = NULL;

	if(eventInfo.GetPlaybackFlags().IsSet(REPLAY_DIRECTION_FWD))
	{
		pForAttachFunc = &ReplayAttachmentManager::AddAttachment;
		pForDetachFunc = &ReplayAttachmentManager::AddDetachment;
	}
	else
	{
		pForAttachFunc = &ReplayAttachmentManager::AddDetachment;
		pForDetachFunc = &ReplayAttachmentManager::AddAttachment;
	}



	const CReplayID* ptr = GetAttachments();
	for(int attachIndex = 0; attachIndex < m_numAttachments; ++attachIndex)
	{
		CReplayID child = *ptr; ++ptr;
		CReplayID parent = *ptr; ++ptr;

		(ReplayAttachmentManager::Get().*pForAttachFunc)(child, parent);
	}

	replayAssert(GetDetachments() == ptr);
	for(int detachIndex = 0; detachIndex < m_numDetachments; ++detachIndex)
	{
		CReplayID child = *ptr; ++ptr;
		CReplayID parent = *ptr; ++ptr;

		(ReplayAttachmentManager::Get().*pForDetachFunc)(child, parent);
	}
}


//////////////////////////////////////////////////////////////////////////
void CPacketEntityAttachDetach::PrintXML(FileHandle handle) const
{
	CPacketEvent::PrintXML(handle);	
	CPacketEntityInfo::PrintXML(handle);

	char str[1024];
	if(m_numAttachments > 0)
	{
		snprintf(str, 1024, "\t\t<Attachments>");
		CFileMgr::Write(handle, str, istrlen(str));

		const CReplayID* ptr = GetAttachments();
		for(int i = 0; i < m_numAttachments; ++i)
		{
			const CReplayID* ptr2 = &ptr[1];
			snprintf(str, 1024, "(0x%08X, 0x%08X)", ptr->ToInt(), ptr2->ToInt());
			CFileMgr::Write(handle, str, istrlen(str));

			ptr+=2;
		}

		snprintf(str, 1024, "</Attachments>\n");
		CFileMgr::Write(handle, str, istrlen(str));
	}

	if(m_numDetachments > 0)
	{
		snprintf(str, 1024, "\t\t<Detachments>");
		CFileMgr::Write(handle, str, istrlen(str));

		const CReplayID* ptr = GetDetachments();
		for(int i = 0; i < m_numDetachments; ++i)
		{
			const CReplayID* ptr2 = &ptr[1];
			snprintf(str, 1024, "(0x%08X, 0x%08X)", ptr->ToInt(), ptr2->ToInt());
			CFileMgr::Write(handle, str, istrlen(str));

			ptr+=2;
		}

		snprintf(str, 1024, "</Detachments>\n");
		CFileMgr::Write(handle, str, istrlen(str));
	}
}

//////////////////////////////////////////////////////////////////////////
void ReplayAttachmentManager::Reset()
{
	sysCriticalSection cs(m_cst);
	for(int i = 0; i < NumPairingBuffers; ++i)
		m_pairings[i].Reset();
	m_index = 0;
	m_pPairingsSaving = NULL;
}


//////////////////////////////////////////////////////////////////////////
void ReplayAttachmentManager::OnRecordStart()
{
	Reset();
}


//////////////////////////////////////////////////////////////////////////
void ReplayAttachmentManager::RecordData(ReplayController& controller/*, bool blockChange*/)
{
	if(GetSavingPairings().GetAttachments().GetNumUsed() > 0 || GetSavingPairings().GetDetachments().GetNumUsed() > 0)
	{
		controller.RecordPacketWithParam<CPacketEntityAttachDetach>(GetSavingPairings());
	}
}


//////////////////////////////////////////////////////////////////////////
s32 ReplayAttachmentManager::GetMemoryUsageForFrame(bool /*blockChange*/)
{
	{
		sysCriticalSection cs(m_cst);
		m_pPairingsSaving = &GetCurrentPairings();
		m_index = (m_index+1) % NumPairingBuffers;
		GetCurrentPairings().Reset();
	}

	return m_pPairingsSaving->GetMemoryUsageForFrame() + sizeof(CPacketEntityAttachDetach);
}


//////////////////////////////////////////////////////////////////////////
// Process any attachment or detachments that were retrieved from this frame...
// Do the detachments first as there was a case where the attachments can swap 
// in a single frame (EntityA parented to EntityB...swapped in one frame to be
// EntityB parented to EntityA.......see parachutes!).
#define ATTACHMENTDEBUG(...) //replayDebugf1(__VA_ARGS__)
void ReplayAttachmentManager::Process()
{
	tPairingMap::ConstIterator detachIter = GetCurrentPairings().GetDetachments().CreateIterator();
	while(!detachIter.AtEnd())
	{
		CReplayID childID = detachIter.GetKey();
		//CReplayID parentID = detachIter.GetData();

		CEntity* pChild = CReplayMgrInternal::GetEntityAsEntity(childID.ToInt());
		//CPhysical* pParent = (CPhysical*)CReplayMgrInternal::GetEntityAsEntity(parentID.ToInt());

		if(pChild)
		{
			((CPhysical*)pChild)->DetachFromParent(0);
			ATTACHMENTDEBUG("Detaching 0x%08X", ((CPhysical*)pChild)->GetReplayID().ToInt());
		}

		detachIter.Next();
	}


	tPairingMap::ConstIterator attachIter = GetCurrentPairings().GetAttachments().CreateIterator();
	while(!attachIter.AtEnd())
	{
		CReplayID childID = attachIter.GetKey();
		CReplayID parentID = attachIter.GetData();
		
		CEntity* pChild = CReplayMgrInternal::GetEntityAsEntity(childID.ToInt());
		CEntity* pParent = CReplayMgrInternal::GetEntityAsEntity(parentID.ToInt());

		if(pChild && pParent)
		{
			// Try to detect cyclic attachement before it happens...if we go in here then
			// we missed a record to detach somewhere.
			if(((CPhysical*)pChild)->GetIsPhysicalAParentAttachment((CPhysical*)pParent))
			{
				replayDebugf1("Forcing a detach as this would cause a cyclic attachment");
				((CPhysical*)pChild)->DetachFromParent(0);
			}
			if(((CPhysical*)pParent)->GetIsPhysicalAParentAttachment((CPhysical*)pChild))
			{
				replayDebugf1("Forcing a detach as this would cause a cyclic attachment");
				((CPhysical*)pParent)->DetachFromParent(0);
			}
			((CPhysical*)pChild)->AttachToPhysicalBasic((CPhysical*)pParent, -1, ATTACH_FLAG_DONT_ASSERT_ON_MATRIX_CHANGE | ATTACH_STATE_BASIC, NULL, NULL);
			ATTACHMENTDEBUG("Attaching 0x%08X to 0x%08X", childID.ToInt(), parentID.ToInt());
		}
		else
		{
			if(!pChild)
			{
				ATTACHMENTDEBUG("Can't attach child 0x%08X", childID.ToInt());
			}

			if(!pParent)
			{
				ATTACHMENTDEBUG("Can't attach child 0x%08X", parentID.ToInt());
			}
		}

		attachIter.Next();
	}

	GetCurrentPairings().Reset();
}


//////////////////////////////////////////////////////////////////////////
void ReplayAttachmentManager::AddAttachment(const CReplayID& child, const CReplayID& parent)
{
	sysCriticalSection cs(m_cst);
	GetCurrentPairings().AddAttachment(child, parent);
}


//////////////////////////////////////////////////////////////////////////
void ReplayAttachmentManager::AddDetachment(const CReplayID& child, const CReplayID& parent)
{
	sysCriticalSection cs(m_cst);
	GetCurrentPairings().AddDetachment(child, parent);
}
#endif // GTA_REPLAY
