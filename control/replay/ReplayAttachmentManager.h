#ifndef REPLAYATTACHMENTMANAGER_H
#define REPLAYATTACHMENTMANAGER_H


#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "Misc/ReplayPacket.h"
#include "atl/map.h"

class CEntity;
class ReplayController;

typedef atMap<CReplayID, CReplayID>	tPairingMap;


class ReplayAttachmentManager
{
public:
	
	static ReplayAttachmentManager&	Get()	{	static ReplayAttachmentManager mgr;	return mgr;	}

	void	Reset();
	void	OnRecordStart();

	void	RecordData(ReplayController& controller/*, bool blockChange*/);
	s32		GetMemoryUsageForFrame(bool /*blockChange*/);

	void	Process();			// Called each frame during playback

	void	AddAttachment(const CReplayID& child, const CReplayID& parent);
	void	AddDetachment(const CReplayID& child, const CReplayID& parent);

	class Pairings
	{
	public:
		void	Reset()
		{
			m_attachments.Reset();
			m_detachments.Reset();
		}

		void	AddAttachment(const CReplayID& child, const CReplayID& parent)
		{
			CReplayID* storedDetachParentID = m_detachments.Access(child);
			if(storedDetachParentID)
			{
				m_detachments.Delete(child);
				//return;	// Attachment added after a detachment....don't throw away the new attachment!
			}
			m_attachments[child] = parent;
		}
		void	AddDetachment(const CReplayID& child, const CReplayID& parent)
		{
			CReplayID* storedAttachParentID = m_attachments.Access(child);
			if(storedAttachParentID)
			{
				m_attachments.Delete(child);
				return;
			}
			m_detachments[child] = parent;
		}

		s32		GetMemoryUsageForFrame() const
		{
			return (m_attachments.GetNumUsed() + m_detachments.GetNumUsed()) * sizeof(CReplayID) * 2;
		}

		const tPairingMap&	GetAttachments() const	{	return m_attachments;	}
		const tPairingMap&	GetDetachments() const	{	return m_detachments;	}
	private:
		tPairingMap	m_attachments;
		tPairingMap	m_detachments;
	};
private:

	Pairings&	GetCurrentPairings()		{	return m_pairings[m_index];	}
	Pairings&	GetSavingPairings()			{	return *m_pPairingsSaving;	}

	const static int NumPairingBuffers = 2;
	Pairings m_pairings[NumPairingBuffers];

	int			m_index;
	Pairings*	m_pPairingsSaving;

	sysCriticalSectionToken	m_cst;
};


class CPacketEntityAttachDetach : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	void Store(ReplayAttachmentManager::Pairings& pairings);
	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const					{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult Preplay(const CEventInfo<void>&) const					{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const;
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_ENTITYATTACHDETACH, "Validation of CPacketEntityAttachDetach Failed!, 0x%08X", GetPacketID());
		return CPacketEvent::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_ENTITYATTACHDETACH;
	}

private:
	CPacketEntityAttachDetach() : CPacketEvent(PACKETID_ENTITYATTACHDETACH, sizeof(CPacketEntityAttachDetach)) {}
	CReplayID*	GetAttachments()	{	return (CReplayID*)((u8*)this + m_basePacketSize);	}
	CReplayID*	GetDetachments()	{	return (CReplayID*)((u8*)GetAttachments() + (m_numAttachments * sizeof(CReplayID) * 2));	}

	const CReplayID*	GetAttachments() const	{	return (const CReplayID*)((u8*)this + m_basePacketSize);	}
	const CReplayID*	GetDetachments() const	{	return (const CReplayID*)((u8*)GetAttachments() + (m_numAttachments * sizeof(CReplayID) * 2));	}

	u32		m_basePacketSize;
	s16		m_numAttachments;
	s16		m_numDetachments;

};


#endif // GTA_REPLAY

#endif // REPLAYATTACHMENTMANAGER_H
