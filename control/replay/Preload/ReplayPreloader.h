#ifndef REPLAYPRELOADER_H
#define REPLAYPRELOADER_H

#include "../ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayAdvanceReader.h"
#include "../ReplaySupportClasses.h"

class CReplayEventManager;
class CReplayInterfaceManager;

class CReplayPreloader : public CReplayClipScanner
{
public:
	const static int ScannerType = 1;
	enum flags
	{
		Entity = 1 << 0,
		Event = 1 << 1,
	};

	CReplayPreloader(CReplayEventManager& eventManager, CReplayInterfaceManager& interfaceManager);

	bool		ProcessResults(const CReplayState& replayFlags, const u32 replayTime, bool forceLoading, int flags);

	void		Clear();

	void		GetBlockDetails(atString& str) const;

private:

	bool		UpdateScanInternal(ReplayThreadData& threadData, const CPacketFrame* pFramePacket, const CBlockInfo* pBlock, int flags);

	bool		ProcessEntityPreloadingRequests(bool forceLoading, u32 currentGameTime);
	bool		ProcessEventPreloadingRequests(const CReplayState& replayFlags, const u32 replayTime, bool forceLoading);
	void		ClearPreloadingRequests();

	CReplayEventManager&	m_eventManager;
	CReplayInterfaceManager&	m_interfaceManager;

	tPreloadRequestArray	m_preloadRequestsEntity;
	tPreloadRequestArray	m_preloadRequestsEvent;

	u32					m_preloadRequestsEntityCount;
	u32					m_preloadSuccessEntityCount;
	u32					m_preloadRequestsEventCount;
	u32					m_preloadSuccessEventCount;
};


#endif // GTA_REPLAY

#endif // REPLAYPRELOADER_H
