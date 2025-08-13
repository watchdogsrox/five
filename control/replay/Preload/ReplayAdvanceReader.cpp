#include "ReplayAdvanceReader.h"

#if GTA_REPLAY
#include "../ReplayInternal.h"


//////////////////////////////////////////////////////////////////////////
void CReplayClipScanner::UpdateScan(ReplayThreadData& threadData)
{
	// Check to reset our precaching extent...
	// this will happen if we jump about in the clip
	if(threadData.resetExtent)
	{
		m_currentPreloadExtent = threadData.currentFrameInfo;
		/*threadData.resetExtent = false;*/
		m_reachedExtent = false;
		Clear();
	}

	if(m_expandEnvelope != threadData.expandEnvelope || threadData.expandEnvelope == false)
	{
		m_expandEnvelope = threadData.expandEnvelope;
		m_reachedExtent = false;
	}

	u32 currentTime = threadData.currentFrameInfo.GetGameTime();
	u32 targetTime = currentTime + (threadData.mode * m_readAheadTime);

	// Clamp the target time between the very first and last frames
	targetTime = Min(targetTime, threadData.lastFrameInfo.GetGameTime());
	targetTime = Max(targetTime, threadData.firstFrameInfo.GetGameTime());

	// 		replayDebugf1("Preload (%d) currentFrame %u, %u currentTime %u, targetTime %u, extent %u", 
	// 			pThreadData->mode, 
	// 			pThreadData->currentFrameInfo.GetFramePacket()->GetFrameRef().GetBlockIndex(), 
	// 			pThreadData->currentFrameInfo.GetFramePacket()->GetFrameRef().GetFrame(), 
	// 			currentTime, 
	// 			targetTime, 
	// 			currentPreloadExtent.GetGameTime());

	while(true)
	{
		// Update the scan for the current frame...
		const CPacketFrame* pFramePacket = m_currentPreloadExtent.GetFramePacket();
		const CBlockInfo* pBlock = threadData.bufferInfo.FindBlockFromSessionIndex(pFramePacket->GetFrameRef().GetBlockIndex());
		if(!UpdateScanInternal(threadData, m_currentPreloadExtent.GetFramePacket(), pBlock, threadData.flags))
			break;

		if(threadData.expandEnvelope == false)
		{	// If we don't want to expand the envelope then bail here
			m_reachedExtent = true;
			return;
		}

		// Check whether we've reached the extent of the reader extent
		if(threadData.mode == CReplayAdvanceReader::ePreloadDirFwd)
		{
			if(targetTime <= m_currentPreloadExtent.GetGameTime())
			{
				m_reachedExtent = true;
				break;
			}
		}
		else if(threadData.mode == CReplayAdvanceReader::ePreloadDirBack)
		{
			if(targetTime >= m_currentPreloadExtent.GetGameTime())
			{
				m_reachedExtent = true;
				break;
			}
		}
		else
		{
			break;
		}

		// Get the current frame to preload...
		pFramePacket = CReplayMgrInternal::GetRelativeFrame(threadData.mode, m_currentPreloadExtent.GetFramePacket());
		pBlock = threadData.bufferInfo.FindBlockFromSessionIndex(pFramePacket->GetFrameRef().GetBlockIndex());

		// Update our extent
		m_currentPreloadExtent.SetFramePacket(pFramePacket);
		m_currentPreloadExtent.SetAddress(CAddressInReplayBuffer((CBlockInfo*)pBlock, (u32)((u8*)pFramePacket - pBlock->GetData())));
		m_currentPreloadExtent.SetHistoryAddress(CAddressInReplayBuffer((CBlockInfo*)pBlock, (u32)((u8*)pFramePacket - pBlock->GetData()) + pFramePacket->GetOffsetToEvents()));
	}
}





//////////////////////////////////////////////////////////////////////////
CReplayAdvanceReader::CReplayAdvanceReader(const CFrameInfo& firstFrame, const CFrameInfo& lastFrame, CBufferInfo& bufferInfo)
	: m_fwdThreadData(ePreloadDirFwd, m_exitThreads, firstFrame, lastFrame, m_currentFrameInfo, bufferInfo)
	, m_backThreadData(ePreloadDirBack, m_exitThreads, firstFrame, lastFrame, m_currentFrameInfo, bufferInfo)
{
	m_exitThreads = false;	
	m_running = false;	
}


//////////////////////////////////////////////////////////////////////////
CReplayAdvanceReader::~CReplayAdvanceReader()
{
	CReplayClipScanner* p = m_fwdThreadData.pFirstScanner;
	while(p)
	{
		CReplayClipScanner* toDelete = p;
		p = p->m_pNextScanner;

		delete toDelete;
	}
	m_fwdThreadData.pFirstScanner = NULL;

	p = m_backThreadData.pFirstScanner;
	while(p)
	{
		CReplayClipScanner* toDelete = p;
		p = p->m_pNextScanner;

		delete toDelete;
	}
	m_backThreadData.pFirstScanner = NULL;
}


//////////////////////////////////////////////////////////////////////////
// Create the threads for each direction
void CReplayAdvanceReader::Init()
{
	m_fwdThreadData.Create("Fwd Thread", &CReplayAdvanceReader::ThreadFunc);
	m_backThreadData.Create("Back Thread", &CReplayAdvanceReader::ThreadFunc);
}


//////////////////////////////////////////////////////////////////////////
// Shutting down so destroy the threads
void CReplayAdvanceReader::Shutdown()
{
	m_exitThreads = true;

	m_fwdThreadData.Destroy();
	m_backThreadData.Destroy();
}


//////////////////////////////////////////////////////////////////////////
void CReplayAdvanceReader::Clear()
{
	CReplayClipScanner* p = m_fwdThreadData.pFirstScanner;
	while(p)
	{
		p->Clear();
		p = p->m_pNextScanner;
	}

	p = m_backThreadData.pFirstScanner;
	while(p)
	{
		p->Clear();
		p = p->m_pNextScanner;
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayAdvanceReader::AddScanner(CReplayClipScanner* pScanner, int direction)
{
	CReplayClipScanner** p = &m_fwdThreadData.pFirstScanner;
	if(direction == ePreloadDirBack)
	{
		p = &m_backThreadData.pFirstScanner;
	}

	// Find the pointer to the last in the list...
	while(*p)
	{
		p = &((*p)->m_pNextScanner);
	}

	// Assign it to the end
	*p = pScanner;
}


//////////////////////////////////////////////////////////////////////////
void CReplayAdvanceReader::Kick(const CFrameInfo& currentFrame, bool& resetFwd, bool& resetBack, bool expandEnvelope, int flags)
{
	REPLAY_CHECK(m_running == false, NO_RETURN_VALUE, "Preload threads are already running");

	m_currentFrameInfo = currentFrame;

	if(resetFwd)
	{
		m_fwdThreadData.resetExtent = true;
		m_fwdThreadData.expandEnvelope = expandEnvelope;
	}

	if(resetBack)
	{
		m_backThreadData.resetExtent = true;
		m_backThreadData.expandEnvelope = expandEnvelope;
	}

	m_fwdThreadData.expandEnvelope = expandEnvelope;
	m_backThreadData.expandEnvelope = expandEnvelope;
	m_fwdThreadData.flags = flags;
	m_backThreadData.flags = flags;

	m_running = true;
	m_fwdThreadData.TriggerThread();
	m_backThreadData.TriggerThread();

	resetFwd = false;
	resetBack = false;
}


//////////////////////////////////////////////////////////////////////////
void CReplayAdvanceReader::WaitForAllScanners()
{
	if(m_running)
	{
 		sysIpcWaitSema(m_fwdThreadData.threadDone);
 		sysIpcWaitSema(m_backThreadData.threadDone);
	}
	m_running = false;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayAdvanceReader::HandleResults(int scannerTypes, const CReplayState& replayFlags, const u32 replayTime, bool forceLoading, int flags)
{
	WaitForAllScanners();

	bool result = true;

	CReplayClipScanner* p = m_fwdThreadData.pFirstScanner;
	while(p)
	{
		if(p->m_scannerType & scannerTypes)
			result &= p->ProcessResults(replayFlags, replayTime, forceLoading, flags);

		p = p->m_pNextScanner;
	}

	p = m_backThreadData.pFirstScanner;
	while(p)
	{
		if(p->m_scannerType & scannerTypes)
			result &= p->ProcessResults(replayFlags, replayTime, forceLoading, flags);

		p = p->m_pNextScanner;
	}

	return result;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayAdvanceReader::IsFrameWithinCurrentEnvelope(int scannerTypes, const CFrameInfo& frame)
{
	CReplayClipScanner* p = m_fwdThreadData.pFirstScanner;
	while(p)
	{
		if(p->m_scannerType & scannerTypes)
		{
			if(p->m_currentPreloadExtent.GetFramePacket() == NULL)
				return false;

			if(frame.GetFrameRef() > p->m_currentPreloadExtent.GetFrameRef())
				return false;

			break;
		}

		p = p->m_pNextScanner;
	}

	p = m_backThreadData.pFirstScanner;
	while(p)
	{
		if(p->m_scannerType & scannerTypes)
		{
			if(p->m_currentPreloadExtent.GetFramePacket() == NULL)
				return false;

			if(frame.GetFrameRef() < p->m_currentPreloadExtent.GetFrameRef())
				return false;

			break;
		}

		p = p->m_pNextScanner;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayAdvanceReader::HasReachedExtents(int scannerTypes)
{
	CReplayClipScanner* p = m_fwdThreadData.pFirstScanner;
	while(p)
	{
		if(p->m_scannerType & scannerTypes)
		{
			if(p->m_reachedExtent == false)
			{
				return false;
			}
		}

		p = p->m_pNextScanner;
	}

	p = m_backThreadData.pFirstScanner;
	while(p)
	{
		if(p->m_scannerType & scannerTypes)
		{
			if(p->m_reachedExtent == false)
			{
				return false;
			}
		}

		p = p->m_pNextScanner;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
void CReplayAdvanceReader::GetBlockDetails(char* pStr, bool& err) const
{
	err = false;
	atString string;
	CReplayClipScanner* p = m_fwdThreadData.pFirstScanner;
	while(p)
	{
		p->GetBlockDetails(string);

		p = p->m_pNextScanner;
	}

	p = m_backThreadData.pFirstScanner;
	while(p)
	{
		p->GetBlockDetails(string);

		p = p->m_pNextScanner;
	}

	sprintf(pStr, "%s", string.c_str());
}


//////////////////////////////////////////////////////////////////////////
void CReplayAdvanceReader::ThreadFunc(void* pData)
{
	ReplayThreadData* pThreadData = (ReplayThreadData*)pData;
	
	while(true)
	{
		// Wait for someone to tell us to go
		sysIpcWaitSema(pThreadData->triggerThread);

		// Check to quit the thread
		if(pThreadData->quit == true)
			break;

		// Update each scanner on this thread
		CReplayClipScanner* pScanner = pThreadData->pFirstScanner;
		while(pScanner)
		{
			pScanner->UpdateScan(*pThreadData);

			pScanner = pScanner->m_pNextScanner;
		}

		pThreadData->resetExtent = false;

		// Signal that we've finished on this thread for now
		sysIpcSignalSema(pThreadData->threadDone);
	}

	// Thread will now exit...Bye!
	sysIpcSignalSema(pThreadData->threadEnded);
}

#endif // GTA_REPLAY
