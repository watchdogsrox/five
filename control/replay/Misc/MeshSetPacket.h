#ifndef __MESH_SET_PACKET_H__
#define __MESH_SET_PACKET_H__

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "control/replay/Misc/ReplayPacket.h"
#include "Control/Replay/PacketBasics.h"

#include "vfx/misc/MovieMeshManager.h"

#define		MAX_MOVIE_SET_FILENAME_LEN	(32)

class ReplayMovieMeshSet
{
public:
	void	SetName(const char *pName)
	{
		strncpy(m_SetName, pName, MAX_MOVIE_SET_FILENAME_LEN);
	}
	char	m_SetName[MAX_MOVIE_SET_FILENAME_LEN];
};

class CPacketMeshSet : public CPacketEvent, public CPacketEntityInfo<0>
{
public:
	CPacketMeshSet()	: CPacketEvent(PACKETID_MESH_SET, sizeof(CPacketMeshSet))	{}

	void Store(const atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS> &info);
	void Extract(const CEventInfo<void>&) const;
	ePreloadResult Preload(const CEventInfo<void>&) const							{ return PRELOAD_SUCCESSFUL; }
	ePreplayResult	Preplay(const CEventInfo<void>&) const							{ return PREPLAY_SUCCESSFUL; }

	void PrintXML(FileHandle handle) const	{	CPacketEvent::PrintXML(handle);	CPacketEntityInfo::PrintXML(handle);			}
	bool ValidatePacket() const
	{
		replayAssertf(GetPacketID() == PACKETID_MESH_SET, "Validation of CPacketMeshSet Failed!, 0x%08X", GetPacketID());
		return CPacketBase::ValidatePacket() && CPacketEntityInfo::ValidatePacket() && GetPacketID() == PACKETID_MESH_SET;
	}

private:

	atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS> m_MeshSetInfo;
};



#endif	//GTA_REPLAY

#endif	//__MESH_SET_PACKET_H__
