#include "ReplaySupportClasses.h"

#if GTA_REPLAY

#include <time.h>
#include "misc/replayPacket.h"
#include "ReplayRecorder.h"
#include "file/device.h"
#include "fwsys/timer.h"
#include "grcore/quads.h"
#include "renderer/sprite2d.h"
#include "renderer/PostProcessFXHelper.h"
#include "system/tls.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "text/TextConversion.h"

#if RSG_PC
#include "grcore/texture_d3d11.h"
#endif // RSG_PC

#include "camera/CamInterface.h"

#if REPLAY_RECORD_SAFETY
__THREAD u8* CReplayRecorder::sm_scratch = NULL;
u8*	CReplayRecorder::sm_scratchBuffers[sm_scatchBufferCount] = {NULL};
#endif // REPLAY_RECORD_SAFETY

time_t ReplayHeader::sm_SaveTimeStamp = 0;

const FrameRef FrameRef::INVALID_FRAME_REF;

CBufferInfo* CBlockProxy::pBuffer = NULL;

//////////////////////////////////////////////////////////////////////////
void ReplayHeader::GenerateDateString(atString& string, u32& uid)
{
	//auto generate a file name using the current time, if we haven't got one
	char filename[REPLAY_DATETIME_STRING_LENGTH] = {0};

#if RSG_PC
	_tzset();
#endif

	time(&sm_SaveTimeStamp);
	tm* date = localtime(&sm_SaveTimeStamp);

	char uidBuffer[REPLAY_DATETIME_STRING_LENGTH] = {0};

	fiDevice::SystemTime currentTime;
	fiDevice::GetLocalSystemTime( currentTime );
	CTextConversion::ConvertSystemTimeToFileNameString(filename, REPLAY_DATETIME_STRING_LENGTH, currentTime);

	sprintf_s(uidBuffer, REPLAY_DATETIME_STRING_LENGTH, "%02d-%02d-%02d-%02d-%02d-%02d", date->tm_mon+1, date->tm_mday, date->tm_year-100, date->tm_hour, date->tm_min, date->tm_sec);	
	uid = atHashString(uidBuffer);

	string.Clear();
	string += filename;
}


//////////////////////////////////////////////////////////////////////////
#if REPLAY_USE_PER_BLOCK_THUMBNAILS

bool CBlockInfo::RequestThumbnail()
{
	if( !m_genThumbnail )
		return false;

	// push photo request on to queue, reset populated state as we want a fresh image
	bool success = m_thumbnailRef.InitialiseAndQueueForPopulation();

	if( success )
		m_genThumbnail = false;

	return success;
}

#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

CBlockProxy	CBlockProxy::GetNextBlock()
{
	return CBlockProxy(pBuffer->GetNextBlock(m_pBlock));
}

CBlockProxy	CBlockProxy::GetPrevBlock()
{
	return CBlockProxy(pBuffer->GetPrevBlock(m_pBlock));
}

//////////////////////////////////////////////////////////////////////////
CBlockProxy	CBufferInfo::FindFirstBlock() const
{
	return CBlockProxy(FindFirstBlockInt(GetFirstBlock()));
}

CBlockProxy	CBufferInfo::FindLastBlock() const
{
	return CBlockProxy(FindLastBlockInt(GetFirstBlock()));
}

CBlockProxy	CBufferInfo::FindFirstTempBlock() const
{
	return CBlockProxy(FindFirstBlockInt(GetFirstTempBlock()));
}

CBlockProxy	CBufferInfo::FindLastTempBlock() const
{
	return CBlockProxy(FindLastBlockInt(GetFirstTempBlock()));
}


//////////////////////////////////////////////////////////////////////////
void CBufferInfo::RelinkTempBlocks()
{
	replayDebugf3("Relinking blocks...");
	CBlockInfo* pFirstTempBlock = FindFirstTempBlock().GetBlock();
	CBlockInfo* pTempBlock = pFirstTempBlock;
	CBlockInfo* pNormalBlock = FindFirstBlock().GetBlock();
	int count = 0;
	do 
	{
		replayDebugf3("Linking %u with %d", pTempBlock->GetMemoryBlockIndex(), pNormalBlock ? (int)pNormalBlock->GetMemoryBlockIndex() : -1);
		pTempBlock->SetNormalBlockEquiv(pNormalBlock);

		pTempBlock = GetNextBlock(pTempBlock);
		pNormalBlock = GetNextBlock(pNormalBlock);

		++count;
		if(count >= m_numTempBlocks)
		{
			pNormalBlock = nullptr;
		}

		if(pTempBlock == pFirstTempBlock)
			break;
	} while (true);

	SetTempBlocksHaveLooped(true);
}


//////////////////////////////////////////////////////////////////////////
void CAddressInReplayBuffer::PrepareForRecording()
{
	GetBlock()->LockForWrite();
	GetBlock()->SetStatus(REPLAYBLOCKSTATUS_BEINGFILLED);

	SetAllowPositionChange(true);
	SetPosition(0);
	SetAllowPositionChange(false);

	// Stick a zero at the end in case we decide to do a replay
	SetPacket(CPacketEnd());
	GetBlock()->UnlockBlock();
}


//////////////////////////////////////////////////////////////////////////
void CFrameInfo::SetAddress(const CAddressInReplayBuffer& address)	
{	
	m_replayAddress = address;	
	CPacketBase* pPacket = m_replayAddress.GetBufferPosition<CPacketBase>();
	if(!pPacket->ValidatePacket())
	{
		DumpReplayPackets((char*)pPacket);
		replayAssertf(false, "Failed to validate packet in CFrameInfo::SetAddress");
	}
	replayFatalAssertf(address.GetSessionBlockIndex() == m_pFramePacket->GetFrameRef().GetBlockIndex(), "");
}


//////////////////////////////////////////////////////////////////////////
void CFrameInfo::SetHistoryAddress(const CAddressInReplayBuffer& address)
{	
	m_historyAddress = address;
	CPacketBase* pPacket = m_replayAddress.GetBufferPosition<CPacketBase>();
	if(!pPacket->ValidatePacket())
	{
		DumpReplayPackets((char*)pPacket);
		replayAssertf(false, "Failed to validate packet in CFrameInfo::SetHistoryAddress");
	}
	replayFatalAssertf(address.GetSessionBlockIndex() == m_pFramePacket->GetFrameRef().GetBlockIndex(), "");
}


//////////////////////////////////////////////////////////////////////////
FrameRef CFrameInfo::GetFrameRef() const
{ 
	//replayFatalAssertf(m_pFramePacket != NULL, "Current Frame is NULL in CFrameInfo::GetFrameRef()");
	REPLAY_CHECK(m_pFramePacket != NULL, FrameRef(), "Current Frame is NULL in CFrameInfo::GetFrameRef()");
	return m_pFramePacket->GetFrameRef(); 
}


//////////////////////////////////////////////////////////////////////////
void CFrameInfo::SetFramePacket(const CPacketFrame* pFrame)
{	
	m_pFramePacket = pFrame;
	m_GameTime = m_SystemTime = 0;

	if(m_pFramePacket)
	{
		REPLAY_CHECK(m_pFramePacket->ValidatePacket(), NO_RETURN_VALUE, "Failed to validate frame packet passed into CFrameInfo::SetFramePacket %p", m_pFramePacket);

		const CPacketBase* pPacket = (const CPacketBase*)((u8*)m_pFramePacket + pFrame->GetPacketSize());
		while(pPacket->GetPacketID() != PACKETID_GAMETIME)
		{
			pPacket = (const CPacketBase*)((u8*)pPacket + pPacket->GetPacketSize());
			if(!pPacket->ValidatePacket())
			{
				DumpReplayPackets((char*)pPacket, (u32)((char*)pPacket - (char*)m_pFramePacket));
				replayAssertf(false, "Failed to validate packet while hunting for gametime packet in CFrameInfo::SetFramePacket %p", pPacket);
				return;
			}
		}
		const CPacketGameTime* pGameTimePacket = (const CPacketGameTime*)pPacket;
		
		if(pGameTimePacket)
		{
			if(!pGameTimePacket->ValidatePacket())
			{
				DumpReplayPackets((char*)pGameTimePacket, (u32)((char*)pGameTimePacket - (char*)m_pFramePacket));
				replayAssertf(false, "Failed to validate packet in CFrameInfo::SetFramePacket");
			}

			m_GameTime		= pGameTimePacket->GetGameTime();
			m_SystemTime	= pGameTimePacket->GetSystemTime();
		}
	}
}


//=====================================================================================================

#if REPLAY_USE_PER_BLOCK_THUMBNAILS

u32 CReplayThumbnail::ms_CurrentIdx = 0;
sysCriticalSectionToken CReplayThumbnail::ms_CritSec;
atArray <CReplayThumbnail *> CReplayThumbnail::ms_Queue;

#if RSG_PC
#define REPLY_THUMBNAIL_QUEUE_MAX_BACKLOG_BEFORE_ALLOWING_POTENTIAL_STALL 4
#endif // RSG_PC

bool CReplayThumbnail::IsPopulated() const
{
	return (sysInterlockedAnd(&m_IsPopulated, 0x1) != 0);
}

void CReplayThumbnail::ClearIsPopulated()
{
	sysInterlockedAnd(&m_IsPopulated, 0x0);
}

void CReplayThumbnail::SetIsPopulated()
{
	sysInterlockedOr(&m_IsPopulated, 0x1);
}


bool CReplayThumbnail::QueueForPopulation(CReplayThumbnail *pThumbnail)
{
	sysCriticalSection cs(CReplayThumbnail::ms_CritSec);
	replayAssertf(CReplayThumbnailRef::IsValidThumbnailPtr(pThumbnail), "CReplayThumbnail::QueueForPopulation()...Not a valud thumbnail pointer!");
	
	// Don`t queue up thumbnail if already queued.
	if(ms_Queue.Find(pThumbnail) != -1)
		return true;

	pThumbnail->m_CaptureDelay = REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE;

	ms_Queue.Append() = pThumbnail;
	return true;
}


void CReplayThumbnail::UnQueueForPopulation(CReplayThumbnail *pThumbnail)
{
	sysCriticalSection cs(CReplayThumbnail::ms_CritSec);

	for(int i=0; i<ms_Queue.GetCount(); i++)
	{
		if(ms_Queue[i] == pThumbnail)
		{
			ms_Queue[i] = NULL;
			return;
		}
	}
}


void CReplayThumbnail::ProcessQueueForPopulation()
{
	int activeIndex = -1;
	if(ANIMPOSTFXMGR.AreReplayRecordingRestrictedStacksAtIdle(true, &activeIndex) == false || camInterface::GetFader().IsFadedOut() )
	{
		for(int queueIdx = 0; queueIdx < ms_Queue.GetCount(); ++queueIdx)
		{
			if(ms_Queue[queueIdx] != nullptr && ms_Queue[queueIdx]->GetFailureReason() == -1)
			{
				ms_Queue[queueIdx]->SetFailureReason(activeIndex);
			}
		}
		return;
	}

	sysCriticalSection cs(CReplayThumbnail::ms_CritSec);

	// Have we nothing queued ?
	if(ms_Queue.GetCount() == 0)
		return;


	// Access the oldest thumbnail to avoid stalling pipeline on x64.
	grcTexture *pTex = GetReplayThumbnailTexture((ms_CurrentIdx + 1) % REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE);

	if(pTex)
	{
	#if RSG_PC
		bool dontStall = true;

		// Allow a potential stall if the queue gets too full.
		if( ms_Queue.GetCount() >= REPLY_THUMBNAIL_QUEUE_MAX_BACKLOG_BEFORE_ALLOWING_POTENTIAL_STALL)
			dontStall = false;

		if (!reinterpret_cast<grcTextureDX11*>(pTex)->MapStagingBufferToBackingStore(dontStall))
		{
			//replayDisplayf("CReplayThumbnail::ProcessQueueForPopulation()...Waiting for MapStagingBufferToBackingStore (%d)", ms_Queue.GetCount());
			return;
		}
	#endif // RSG_PC

		grcTextureLock lock;

		if(pTex->LockRect(0, 0, lock, grcsRead))
		{

			for(int queueIdx=0; queueIdx<ms_Queue.GetCount(); queueIdx++)
			{
				if(ms_Queue[queueIdx] != NULL && ms_Queue[queueIdx]->m_CaptureDelay-- == 0 )
				{
					replayAssertf(CReplayThumbnailRef::IsValidThumbnailPtr(ms_Queue[queueIdx]), "CReplayThumbnail::ProcessQueueForPopulation()...Not a valud thumbnail pointer!");
					replayAssertf(pTex->GetWidth() == (int)ms_Queue[queueIdx]->GetWidth(), "CReplayThumbnail::ProcessQueueForPopulation()...Dimensions don't match");
					replayAssertf(pTex->GetHeight() == (int)ms_Queue[queueIdx]->GetHeight(), "CReplayThumbnail::ProcessQueueForPopulation()...Dimensions don't match");

					u8 *pSrc = (u8 *)lock.Base;
					u8 *pDest = (u8 *)ms_Queue[queueIdx]->GetRawBuffer();

					for(u32 i=0; i<pTex->GetHeight(); i++)
					{
						Color32 *pSrcPixels = (Color32 *)pSrc;

						for(u32 j=0; j<pTex->GetWidth(); j++)
						{
							*pDest++ = pSrcPixels->GetRed();
							*pDest++ = pSrcPixels->GetGreen();
							*pDest++ = pSrcPixels->GetBlue();
							pSrcPixels++;
						}
						pSrc += lock.Pitch;
					}
					ms_Queue[queueIdx]->SetIsPopulated();

					// Remove processed thumbnails from the queue.
					ms_Queue.Delete(queueIdx);
					queueIdx--;
				}
			}
			pTex->UnlockRect(lock);
		}
	}
}


void CReplayThumbnail::ResetQueue()
{
	sysCriticalSection cs(CReplayThumbnail::ms_CritSec);

	ms_Queue.ResetCount();
}


void CReplayThumbnail::ResizeQueue(u32 size)
{
	sysCriticalSection cs(CReplayThumbnail::ms_CritSec);
	ms_Queue.Reset();
	ms_Queue.Resize(size);
	ms_Queue.ResetCount();
}


//////////////////////////////////////////////////////////////////////////
void CReplayThumbnail::CreateReplayThumbnail()
{
//	if(ANIMPOSTFXMGR.AreReplayRecordingRestrictedStacksAtIdle() == false || camInterface::GetFader().IsFadedOut() )
//		return;

	sysCriticalSection cs(CReplayThumbnail::ms_CritSec);

	// Move on in the list of thumbnails captured per frame.
	ms_CurrentIdx = ((ms_CurrentIdx + 1) % REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE);

	float widthRatio = 1.0f;
	float heightRatio = 1.0f;
	const grcTexture* backBuffer = CRenderTargets::GetUIBackBuffer();

#if RSG_PC
	float width = static_cast<float>(backBuffer->GetWidth());
	float height = static_cast<float>(backBuffer->GetHeight());

	const float idealAspectRatio = 0.5625f;
	widthRatio = height / width;
	if (widthRatio < idealAspectRatio)
	{
		float newWidth = height / idealAspectRatio;
		widthRatio = width / newWidth; 
	}
	else 
	{
		if (widthRatio > idealAspectRatio)
		{
			float newHeight = width * idealAspectRatio;
			heightRatio = height / newHeight;
		}
		widthRatio = 1.0f;	
	}
#endif

	// Get render target + signal we have an on-going image (see GetReplayThumbnailRT()).
	grcTextureFactory::GetInstance().LockRenderTarget(0, GetReplayThumbnailRT(ms_CurrentIdx), NULL);

	CSprite2d copyBuffer;
	copyBuffer.BeginCustomList(CSprite2d::CS_BLIT, backBuffer);
	grcDrawSingleQuadf(-widthRatio, heightRatio, widthRatio, -heightRatio, 0.0f, 0.0, 0.0, 1.0f, 1.0f,Color32(255, 255, 255, 255));
	copyBuffer.EndCustomList();
	grcTextureFactory::GetInstance().UnlockRenderTarget(0);

#if RSG_PC
	static_cast<grcTextureDX11*>(GetReplayThumbnailTexture(ms_CurrentIdx))->CopyFromGPUToStagingBuffer();
#endif // RSG_PC
}


grcTexture* CReplayThumbnail::GetReplayThumbnailTexture(u32 idx)
{
	return CRenderTargets::GetReplayThumbnailTexture(idx);
}

grcRenderTarget* CReplayThumbnail::GetReplayThumbnailRT(u32 idx)
{
	return CRenderTargets::GetReplayThumbnailRT(idx);
}


//////////////////////////////////////////////////////////////////////////

u32 CReplayThumbnail::GetThumbnailWidth()
{
	return SCREENSHOT_WIDTH;
}


u32 CReplayThumbnail::GetThumbnailHeight()
{
	return SCREENSHOT_HEIGHT;
}

//=====================================================================================================

int CReplayThumbnailRef::ms_NextIndex = 0;
int CReplayThumbnailRef::ms_NoOfThumbnails = 0;
CReplayThumbnail *CReplayThumbnailRef::ms_pThumbnails = NULL;


bool CReplayThumbnailRef::InitialiseAndQueueForPopulation()
{
	if(IsInitialAndQueued())
		return true;

	// Just loop round the buffer.
	CReplayThumbnail* pThumbnail = nullptr;
	do 
	{
		m_Index = ms_NextIndex++;

		while(ms_NextIndex >= ms_NoOfThumbnails)
			ms_NextIndex -= ms_NoOfThumbnails;

		pThumbnail = &GetThumbnail();
	} while (pThumbnail->IsLocked()); // Keep going until we find one that's not locked (there should only be one locked at a time)
	
	// Queue for population.
	pThumbnail->ClearIsPopulated();
	return CReplayThumbnail::QueueForPopulation(pThumbnail);
}


bool CReplayThumbnailRef::IsPopulated() const
{
	if(IsInitialAndQueued() == false)
		return false;

	return GetThumbnail().IsPopulated();
}


const CReplayThumbnail &CReplayThumbnailRef::GetThumbnail() const
{
	replayAssertf(m_Index != -1, "CReplayThumbnailRef::GetThumbnail()...Not initialised and queued for population.");
	replayAssertf(ms_pThumbnails != NULL, "CReplayThumbnailRef::GetThumbnail()...Thumbnail buffer not initialised.");
	return ms_pThumbnails[m_Index];
}

CReplayThumbnail &CReplayThumbnailRef::GetThumbnail()
{
	replayAssertf(m_Index != -1, "CReplayThumbnailRef::GetThumbnail()...Not initialised and queued for population.");
	replayAssertf(ms_pThumbnails != NULL, "CReplayThumbnailRef::GetThumbnail()...Thumbnail buffer not initialised.");
	return ms_pThumbnails[m_Index];
}


void CReplayThumbnailRef::SetNoOfBlocksAllocated(u32 maxBlocks)
{
	// In case the queue has entries using the previous buffer of thumbnails.
	CReplayThumbnail::ResetQueue();

	if(ms_pThumbnails)
	{
		sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
		delete [] ms_pThumbnails;
		ms_pThumbnails = NULL;
	}

	ms_NoOfThumbnails = maxBlocks;

	if( ms_NoOfThumbnails > 0 )
	{
		ms_NoOfThumbnails = ms_NoOfThumbnails + 1; // Add an extra one to account for one being locked while saving

		// Allocate enough for one thumbnail per block in existence.
		sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
		ms_pThumbnails = rage_new CReplayThumbnail[ms_NoOfThumbnails];
	}

	for(int i = 0; i < ms_NoOfThumbnails; ++i)
	{
		ms_pThumbnails[i].Initialize(SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT);
	}

	// Reset round robin.
	ms_NextIndex = 0;

	// Make queue size match number of thumb nails.
	CReplayThumbnail::ResizeQueue(ms_NoOfThumbnails);
}


#if __ASSERT
bool CReplayThumbnailRef::IsValidThumbnailPtr(CReplayThumbnail *pPtr)
{
	if(((u64)pPtr < (u64)&ms_pThumbnails[0]) || ((u64)pPtr >= (u64)&ms_pThumbnails[ms_NoOfThumbnails]))
		return false;
	return true;
}
#endif // __ASSERT

#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

#endif // GTA_REPLAY
