#ifndef REPLAYPREPLAYER_H
#define REPLAYPREPLAYER_H

#include "../ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayAdvanceReader.h"
#include "../ReplaySupportClasses.h"

class CReplayEventManager;
class CPacketEvent;

class CReplayPreplayer : public CReplayClipScanner
{
public:
	const static int ScannerType = 2;

	CReplayPreplayer(CReplayEventManager& eventManager);

	bool		ProcessResults(const CReplayState& replayFlags, const u32 replayTime, bool forceLoading, int flags);

	void		Clear();

	void		GetBlockDetails(atString& str) const;

private:

	bool		UpdateScanInternal(ReplayThreadData& threadData, const CPacketFrame* pFramePacket, const CBlockInfo* pBlock, int flags);

	CReplayEventManager&	m_eventManager;

	atArray<const CPacketEvent*> m_preplayRequests;
};



#endif // GTA_REPLAY

#endif // REPLAYPREPLAYER_H
