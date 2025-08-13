#ifndef __MOVIE_TARGET_PACKET_H__
#define __MOVIE_TARGET_PACKET_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"

class MovieDestinationInfo
{
public:
	MovieDestinationInfo()
		: m_TargetModelHash((u32)0)
		, m_MovieHandle(0)
		, m_colour(255, 255, 255, 255)
	{}

	void Reset()
	{
		m_TargetModelHash = (u32)0;
		m_MovieHandle = 0;
		m_colour = Color32(255, 255, 255, 255);
	}

	atHashString	m_TargetModelHash;
	u32		m_MovieHandle;
	Color32 m_colour;
};

class CPacketMovieTarget : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketMovieTarget(atHashString targetModelHash, u32 movieHandle, Color32 colour);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle fileID) const	{	CPacketEvent::PrintXML(fileID);	CPacketEntityInfo::PrintXML(fileID);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MOVIE_TARGET, "Validation of CPacketMovieTarget Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_MOVIE_TARGET;
	}

private:

	MovieDestinationInfo m_Info;
};

#endif	// GTA_REPLAY

#endif	//__MOVIE_TARGET_PACKET_H__in 