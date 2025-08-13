#ifndef __MOVIE_PACKET_H__
#define __MOVIE_PACKET_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"

#include "vfx/misc/MovieManager.h"

//////////////////////////////////////////////////////////////////////////

#define MAX_REPLAY_MOVIE_NAME_SIZE	(32)
#define MAX_REPLAY_MOVIE_NAME_SIZE_64	(64)

template<int namelength>
class ReplayMovieInfo
{
public:
	ReplayMovieInfo() 
	{
		movieName[0] = 0;
		movieTime = 0.0f;
	}

	char	movieName[namelength];
	float	movieTime;
	bool	isPlaying;
};



class CPacketMovie : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketMovie(atFixedArray<ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE>, MAX_ACTIVE_MOVIES> &info);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle fileID) const	{	CPacketEvent::PrintXML(fileID);	CPacketEntityInfo::PrintXML(fileID);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MOVIE, "Validation of CPacketMovie Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_MOVIE;
	}

private:

	atFixedArray<ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE>, MAX_ACTIVE_MOVIES> m_MovieInfo;
};

class CPacketMovie2 : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketMovie2(atFixedArray<ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE_64>, MAX_ACTIVE_MOVIES> &info);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle fileID) const	{	CPacketEvent::PrintXML(fileID);	CPacketEntityInfo::PrintXML(fileID);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MOVIE2, "Validation of CPacketMovie2 Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_MOVIE2;
	}

private:

	atFixedArray<ReplayMovieInfo<MAX_REPLAY_MOVIE_NAME_SIZE_64>, MAX_ACTIVE_MOVIES> m_MovieInfo;
};


/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Updated version of this packet, so backwards compatibility works, new data will be a CPacketMovieEntity_V2 packet type

class CPacketMovieEntity : public CPacketEvent, public CPacketEntityInfo<0>
{
public:

	CPacketMovieEntity(const char* movieName, bool frontendAudio, const CEntity *pEntity);

	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle fileID) const	{	CPacketEvent::PrintXML(fileID);	CPacketEntityInfo::PrintXML(fileID);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MOVIE_ENTITY, "Validation of CPacketMovieEntity Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_MOVIE_ENTITY;
	}

private:

	char	m_MovieName[MAX_REPLAY_MOVIE_NAME_SIZE];
	bool	m_FrontendAudio;

	u32					m_ModelNameHash;
	CPacketVector<3>	m_EntityPos;

	u32		m_movieHash;
};







//////////////////////////////////////////////////////////////////////////

#endif	//GTA_REPLAY

#endif // __TV_PACKET_H__
