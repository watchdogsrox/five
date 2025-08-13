#ifndef __IPL_PACKET_H__
#define __IPL_PACKET_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"

#define MAX_IPL_HASHES	(256)		// Max is currently 239

class IPLChangeData
{
public:
	IPLChangeData()
	{
		m_Count = 0;
	}

	u32		m_Count;
	u32		m_IPLHashes[MAX_IPL_HASHES];
};

class CPacketIPL : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketIPL()	: CPacketEvent(PACKETID_IPL, sizeof(CPacketIPL))	{}

	void Store(const atArray<u32>& info, const atArray<u32>& previousinfo);
	void Extract(const CEventInfo<void>& eventInfo) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_IPL, "Validation of CPacketIPL Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_IPL;
	}

private:

	IPLChangeData	m_PreviousIPLData;
	IPLChangeData	m_IPLData;
};

#endif	//GTA_REPLAY

#endif	//__IPL_PACKET_H__
