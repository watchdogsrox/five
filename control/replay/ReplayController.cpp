#include "ReplayController.h"


#if GTA_REPLAY

#include "Control/Replay/Misc/ReplayPacket.h"
#include "Control/replay/Streaming/ReplayStreaming.h"

//////////////////////////////////////////////////////////////////////////
ReplayController::ReplayController(CAddressInReplayBuffer& currAddress, CBufferInfo& bufferInfo, FrameRef startingFrame)
: m_currentAddress(currAddress)
, m_bufferInfo(bufferInfo)
, m_mode(None)
, m_currentFrame(startingFrame.GetFrame())
, m_playbackFlags(0)
, m_fineScrubbing(false)
, m_isFirstFrame(false)
, m_interp(0.0f)
{
	ResetPrevs();
}


//////////////////////////////////////////////////////////////////////////
ReplayController::~ReplayController()
{
	replayFatalAssertf(m_mode == None, "");
}


//////////////////////////////////////////////////////////////////////////
bool ReplayController::EnableRecording(u32 addressLimit, bool canGotoNextBlock)
{
	replayFatalAssertf(m_mode == None, "Controller should have been in 'none' state");

	bool locked = m_currentAddress.GetBlock()->LockForWrite();
	if(!locked)
	{
		replayFatalAssertf(false, "");
		return false;
	}

	if(IsThereSpaceToRecord(addressLimit, m_currentAddress.GetBlock()->GetBlockSize()) == false)
	{
		if(canGotoNextBlock)
		{
			GoToNextBlock();
			m_currentAddress.GetBlock()->LockForWrite();
		}
		else
		{
			m_currentAddress.GetBlock()->UnlockBlock();
			return false;
		}
	}

	m_addressLimit = m_currentAddress.GetPosition() + addressLimit;
	m_currentAddress.SetIndexLimit(m_addressLimit);
	m_currentAddress.SetAllowPositionChange(true);

	ResetPrevs();

	m_mode = Recording;

	return true;
}


//////////////////////////////////////////////////////////////////////////
void ReplayController::RenableRecording()
{
	bool locked = m_currentAddress.GetBlock()->LockForWrite();
	if(!locked)
	{
		replayFatalAssertf(false, "");
		return;
	}

	m_currentAddress.SetIndexLimit(m_addressLimit);
	m_currentAddress.SetAllowPositionChange(true);

	m_mode = Recording;
}


//////////////////////////////////////////////////////////////////////////
void ReplayController::DisableRecording()
{
	replayFatalAssertf(m_mode == Recording, "Controller should have been in 'recording' state");

	m_currentAddress.ResetIndexLimit();
	m_currentAddress.SetAllowPositionChange(false);

	m_currentAddress.GetBlock()->UnlockBlock();

	m_mode = None;
}


//////////////////////////////////////////////////////////////////////////
bool ReplayController::IsThereSpaceToRecord(u32 volume, u32 blockSize) const
{
	return ((m_currentAddress.GetPosition() + volume) <= (blockSize - 16));
}


//////////////////////////////////////////////////////////////////////////
bool ReplayController::EnablePlayback()
{
	replayFatalAssertf(m_mode == None, "Controller should have been in 'none' state");

	bool locked = m_currentAddress.GetBlock()->LockForRead();
	if(!locked)
	{
		replayFatalAssertf(false, "");
		return false;
	}

	ResetPrevs();

	m_mode = Playing;
	return true;
}


//////////////////////////////////////////////////////////////////////////
void ReplayController::DisablePlayback()
{
	replayFatalAssertf(m_mode == Playing, "Controller should have been in 'playback' state");

	m_currentAddress.GetBlock()->UnlockBlock();

	m_mode = None;
}


//////////////////////////////////////////////////////////////////////////
bool ReplayController::EnableModify()
{
	replayFatalAssertf(m_mode == Playing || m_mode == None, "Controller should have been in 'playback' or 'none' state");

	m_prevMode = m_mode;
	m_mode = Modify;

	if(m_prevMode == Playing)
		m_currentAddress.GetBlock()->UnlockBlock();

	m_currentAddress.GetBlock()->LockForWrite();

	ResetPrevs();

	return true;
}


//////////////////////////////////////////////////////////////////////////
void ReplayController::DisableModify()
{
	replayFatalAssertf(m_mode == Modify, "Controller should have been in 'modify' state");

	m_currentAddress.GetBlock()->UnlockBlock();
	m_mode = m_prevMode;

	if(m_mode == Playing)
		m_currentAddress.GetBlock()->LockForRead();
}


//////////////////////////////////////////////////////////////////////////
bool ReplayController::IsNextFrame()
{
	m_currentPacketID = m_currentAddress.GetPacketID();
	if(m_currentPacketID == PACKETID_NEXTFRAME)
	{
		m_currentPacketSize = sizeof(CPacketNextFrame);
		return true;
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
// Check whether the current packet is the 'None' packet which marks the end
// of a block
// If it is and the block is 'being filled' then we should return true as
// this is the end of playback
// If not then advance to the next block and return false
bool ReplayController::IsEndOfPlayback()
{
	if(IsEndOfBlock())
	{
		if(IsLastBlock())
			return true;
		else
		{
			AdvanceBlock();
			return IsEndOfBlock();
		}
	}

	return false;
}


//////////////////////////////////////////////////////////////////////////
void ReplayController::RecordPacketBlock(u8* pData, u32 dataSize)
{
	replayFatalAssertf(m_mode == Recording, "Controller is not in recording mode");
	u8* p = m_currentAddress.GetBufferPosition<u8>();
	sysMemCpy(p, pData, dataSize);

	m_currentAddress.IncPosition(dataSize);
}


//////////////////////////////////////////////////////////////////////////
void ReplayController::AdvanceBlock()
{
	eReplayBlockLock prevLock = m_currentAddress.GetBlock()->UnlockBlock();

	m_currentAddress.SetPosition(0);
	m_currentAddress.SetBlock(m_bufferInfo.GetNextBlock(m_currentAddress.GetBlock()));

	m_currentAddress.GetBlock()->RelockBlock(prevLock);

	m_currentFrame = m_playbackFlags.IsSet(REPLAY_DIRECTION_FWD) ? 0 : (u16)m_currentAddress.GetBlock()->GetFrameCount()-1;
}

#if __BANK
void PrintInfo(CBlockInfo* firstBlock, CBlockInfo* info, int* numBlocks)
{
	if( info && *numBlocks > 0 )
	{
		replayDisplayf("______________________________________________________");
		replayDisplayf("BLOCK DATA");
		replayDisplayf("Prev Index  --  Memory Index  --  Next Index");
		s32 prevIndex = info->GetPrevBlock() ? s32(info->GetPrevBlock()->GetMemoryBlockIndex()) : -1;
		s32 nextIndex = info->GetNextBlock() ? s32(info->GetNextBlock()->GetMemoryBlockIndex()) : -1;

		replayDisplayf("    %i		<-		 %u		  ->	  %i", prevIndex, info->GetMemoryBlockIndex(), nextIndex);

		if( info->IsTempBlock() )
		{
			replayDisplayf("	IsTempBlock: YES");

			CBlockInfo* normEquiv = info->GetNormalBlockEquiv();

			if( normEquiv )
			{
				replayDisplayf("    NORMAL EQUIV INFO");
				replayDisplayf("    Prev Index  --  Memory Index  --  Next Index");

				s32 prevIndex = normEquiv->GetPrevBlock() ? s32(normEquiv->GetPrevBlock()->GetMemoryBlockIndex()) : -1;
				s32 nextIndex = normEquiv->GetNextBlock() ? s32(normEquiv->GetNextBlock()->GetMemoryBlockIndex()) : -1;

				replayDisplayf("		%i		<-		 %u		  ->	  %i", prevIndex, normEquiv->GetMemoryBlockIndex(), nextIndex);
			}
		}
		
		info = info->GetNextBlock();

		if( firstBlock != info )
		{
			PrintInfo(firstBlock, info, &(--(*numBlocks)));
		}
	}
}
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
bool ReplayController::GoToNextBlock()
{
	sysCriticalSection criticalSection(m_CriticalSectionToken);

	replayAssert(m_currentAddress.GetBlock()->IsLockedForWrite());

	// Set the current block to full
	m_currentAddress.SetPacket(CPacketEnd());
	m_currentAddress.GetBlock()->SetStatus(REPLAYBLOCKSTATUS_FULL);
	m_currentAddress.GetBlock()->UnlockBlock();
	m_currentAddress.GetBlock()->SetSizeLost(m_currentAddress.GetBlock()->GetBlockSize() - m_currentAddress.GetBlock()->GetSizeUsed());

	BUFFERDEBUG(" Going to next block... current is %u", m_currentAddress.GetBlock()->GetMemoryBlockIndex());
	u16 currSessionBlockIndex = m_currentAddress.GetBlock()->GetSessionBlockIndex();
	CBlockInfo* pNextBlock = NULL;
	if(m_currentAddress.GetBlock()->IsTempBlock())
	{	// Current block is a temp block so if possible we want to switch back to a normal block.
		// if we can't then we switch to the next temp block
 		CBlockInfo* pTempBlock = m_currentAddress.GetBlock();

		// Get the next normal block...
		pNextBlock = m_bufferInfo.GetNextBlock(pTempBlock->GetNormalBlockEquiv());
		BUFFERDEBUG("  ...is a temp block so get next equiv %u", pNextBlock ? pNextBlock->GetMemoryBlockIndex() : -1);
	}
	else
	{
		replayFatalAssertf(m_currentAddress.GetBlock()->IsTempBlock() == false, "");
		pNextBlock = m_bufferInfo.GetNextBlock(m_currentAddress.GetBlock());
		BUFFERDEBUG("  ...not a temp block so get next %u", pNextBlock->GetMemoryBlockIndex());
	}

	if(m_bufferInfo.IsAnyBlockSaving() == true)
	{
		// Next block hasn't been saved yet so we can't use it...so switch to the next temp block
		CBlockInfo* pNextTempBlock = m_bufferInfo.GetNextBlock(m_currentAddress.GetBlock());
		BUFFERDEBUG("  Still saving so get next temp block %u", pNextTempBlock->GetMemoryBlockIndex());

		if(pNextTempBlock == m_bufferInfo.GetFirstTempBlock())
		{
			CBlockProxy possibleNextTemp = m_bufferInfo.FindAvailableNormalBlock();
			if(possibleNextTemp.IsValid())
			{
				m_bufferInfo.DecoupleNormalBlock(possibleNextTemp);
				m_bufferInfo.ExtendTempBuffer(possibleNextTemp);
				pNextTempBlock = possibleNextTemp.GetBlock(); 
				pNextBlock = NULL; // Going outside the normal range of temp blocks so there won't be an equiv here...

				BUFFERDEBUG("   Found available normal block - %u", possibleNextTemp.GetMemoryBlockIndex());
			}
			else
			{
				m_bufferInfo.SetReLinkTempBlocks(true);
				BUFFERDEBUG("   No available normal blocks so we'll loop and set to relink");
			}
		}
		
		pNextTempBlock->SetNormalBlockEquiv(pNextBlock);

		pNextBlock = pNextTempBlock;
/*
		replayDisplayf("______________________________________________________________________");
		replayDisplayf("PRINT NORMAL INFO");
		replayDisplayf("");
		int numBlocks = m_bufferInfo.m_numBlocks;
		PrintInfo(m_bufferInfo.m_pNormalBuffer, m_bufferInfo.m_pNormalBuffer, &numBlocks);

		replayDisplayf("______________________________________________________________________");
		replayDisplayf("PRINT TEMP INFO");
		replayDisplayf("");
		int numTempBlocks = m_bufferInfo.m_numBlocks;
		PrintInfo(pNextTempBlock, pNextTempBlock, &numTempBlocks);
*/

		if(pNextTempBlock == m_bufferInfo.GetFirstTempBlock())
		{
			BUFFERDEBUG(" Temp blocks have looped!");
			m_bufferInfo.SetTempBlocksHaveLooped(true);
		}

		if(m_bufferInfo.GetFirstBlock() == pNextTempBlock->GetNormalBlockEquiv())
		{	// Not sure this ever gets hit
			BUFFERDEBUG("Bumping normal head from %u to %u", m_bufferInfo.GetFirstBlock()->GetMemoryBlockIndex(), m_bufferInfo.GetNextBlock(pNextTempBlock->GetNormalBlockEquiv())->GetMemoryBlockIndex());
			m_bufferInfo.ReassignNormalBufferHead(m_bufferInfo.GetNextBlock(pNextTempBlock->GetNormalBlockEquiv()));
		}

		BUFFERDEBUG("Going to next temp block (%u>%u) Relink is %s, Equiv is %d", m_currentAddress.GetMemoryBlockIndex(), pNextTempBlock->GetMemoryBlockIndex(), m_bufferInfo.GetReLinkTempBlocks() ? "Yes" : "No", pNextTempBlock->GetNormalBlockEquiv() ? pNextTempBlock->GetNormalBlockEquiv()->GetMemoryBlockIndex() : -1);
	}
	else
	{
		m_bufferInfo.SetReLinkTempBlocks(false);

		if(pNextBlock == NULL)
		{
			CBlockProxy possibleNextTemp = m_bufferInfo.FindAvailableNormalBlock();
			pNextBlock = possibleNextTemp.GetBlock();
		}

		BUFFERDEBUG("Going to next block (%u>%u)", m_currentAddress.GetMemoryBlockIndex(), pNextBlock->GetMemoryBlockIndex());
	}

	pNextBlock->PrepForRecording(currSessionBlockIndex + 1);
	m_currentAddress.SetBlock(pNextBlock);

	m_currentAddress.PrepareForRecording();

	if(m_bufferInfo.GetReLinkTempBlocks())
	{
		m_bufferInfo.RelinkTempBlocks();
	}

	m_currentFrame = 0;

	return true;
}


//////////////////////////////////////////////////////////////////////////
template<>
CPacketNextFrame const* ReplayController::ReadCurrentPacket<CPacketNextFrame>()
{
	replayFatalAssertf(m_mode != None, "");
	m_currentPacketSize = sizeof(CPacketNextFrame);
	CPacketNextFrame const* pPacket = m_currentAddress.GetBufferPosition<CPacketNextFrame>();
	if(CheckPacket(pPacket) == false)
	{
		return NULL;
	}
	m_currentPacketID = pPacket->GetPacketID();
	m_currentFrame += m_playbackFlags.IsSet(REPLAY_DIRECTION_FWD) ? 1 : -1;
	ShufflePrev(pPacket);
	return pPacket;
}


//////////////////////////////////////////////////////////////////////////
template<>
CPacketNextFrame* ReplayController::GetCurrentPacket<CPacketNextFrame>()
{
	replayFatalAssertf(m_mode != None && m_mode != Playing, "");
	m_currentPacketSize = sizeof(CPacketNextFrame);
	CPacketNextFrame* pPacket = m_currentAddress.GetBufferPosition<CPacketNextFrame>();
	if(CheckPacket(pPacket) == false)
	{
		return NULL;
	}
	m_currentPacketID = pPacket->GetPacketID();
	m_currentFrame += m_playbackFlags.IsSet(REPLAY_DIRECTION_FWD) ? 1 : -1;
	ShufflePrev(pPacket);
	return pPacket;
}




#endif // GTA_REPLAY
