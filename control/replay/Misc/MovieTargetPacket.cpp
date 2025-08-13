#include "MovieTargetPacket.h"

#if GTA_REPLAY

#include "control/replay/ReplayMovieController.h"

tPacketVersion CPacketMovieTarget_V1 = 1;
//////////////////////////////////////////////////////////////////////////
CPacketMovieTarget::CPacketMovieTarget(atHashString targetModelHash, u32 movieHandle, Color32 colour)
	: CPacketEvent(PACKETID_MOVIE_TARGET, sizeof(CPacketMovieTarget), CPacketMovieTarget_V1)
{
	m_Info.m_TargetModelHash = targetModelHash;
	m_Info.m_MovieHandle = movieHandle;
	m_Info.m_colour = colour;
}

//////////////////////////////////////////////////////////////////////////
void CPacketMovieTarget::Extract(const CEventInfo<void>&) const
{
	MovieDestinationInfo info = m_Info;
	if(GetPacketVersion() < CPacketMovieTarget_V1)
	{
		// Before Version 1 this packet didn't have colour stored so force white
		info.m_colour.Set(255, 255, 255, 255);
	}

	ReplayMovieController::SubmitMovieTargetThisFrame(info);
}

#endif	//GTA_REPLAY