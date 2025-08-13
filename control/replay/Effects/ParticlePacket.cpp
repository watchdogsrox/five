//=====================================================================================================
// name:		ParticlePacket.cpp
//=====================================================================================================

#include "ParticlePacket.h"

// Replay only setup for win32
#if GTA_REPLAY

#include "string.h"
#include "system/FileMgr.h"
#include "control/replay/misc/ReplayPacket.h"



//////////////////////////////////////////////////////////////////////////
void CPacketEventInterp::PrintXML(FileHandle handle) const
{
	CPacketEvent::PrintXML(handle);

	char str[1024];
	snprintf(str, 1024, "\t\t<PFXKey>%u</PFXKey>\n", m_uPfxKey);
	CFileMgr::Write(handle, str, istrlen(str));
}

void CPacketEventInterp::Init()
{
	REPLAY_GUARD_ONLY(m_ParticleInterpGuard = REPLAY_GUARD_PARTICLEINTERP;)

	m_NextBlockIndex = 0;
	m_NextPosition = INVALID_INTERP_PACKET;
}

void CPacketEventInterp::SetNextOffset(u16 blockIndex, u32 position)
{
	replayAssertf(position != INVALID_INTERP_PACKET && position != HAS_BEEN_DELETED, "Can't store this offset, it's too large!");

	m_NextBlockIndex = blockIndex;
	m_NextPosition = position;
}

#endif // GTA_REPLAY
