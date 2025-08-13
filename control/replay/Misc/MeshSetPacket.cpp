#include "MeshSetPacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayMeshSetManager.h"

//////////////////////////////////////////////////////////////////////////
void CPacketMeshSet::Store(const atFixedArray<ReplayMovieMeshSet, MOVIE_MESH_MAX_SETS> &info)
{
	CPacketEvent::Store(PACKETID_MESH_SET, sizeof(CPacketMeshSet));

	m_MeshSetInfo = info;
}


//////////////////////////////////////////////////////////////////////////
void CPacketMeshSet::Extract(const CEventInfo<void>&) const
{
	// Submit packets this frame to controller
	ReplayMeshSetManager::SubmitNewMeshSetData(m_MeshSetInfo);
}

#endif	//GTA_REPLAY
