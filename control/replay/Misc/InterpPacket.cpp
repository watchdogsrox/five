//=====================================================================================================
// name:		InterpPacket.cpp
//=====================================================================================================

#include "control/replay/Misc/InterpPacket.h"

// Replay only setup for win32
#if GTA_REPLAY

//========================================================================================================================
void CPacketInterp::Store()
{
	REPLAY_GUARD_ONLY(m_InterpGuard = REPLAY_GUARD_INTERP;)

	m_NextBlockIndex = 0;
	m_NextPosition = INVALID_INTERP_PACKET;
}

void CPacketInterp::SetNextOffset(u16 blockIndex, u32 position)
{
	replayAssertf(position != INVALID_INTERP_PACKET && position != HAS_BEEN_DELETED, "Can't store this offset, it's too large!");

	m_NextBlockIndex = blockIndex;
	m_NextPosition = position;
}

#endif // GTA_REPLAY
