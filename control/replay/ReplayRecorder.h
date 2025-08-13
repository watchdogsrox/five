#ifndef REPLAYRECORDER_H_
#define REPLAYRECORDER_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "system/memmanager.h"
#include "system/tls.h"
#include "Misc/ReplayPacket.h"

#define REPLAY_RECORD_SAFETY		1
#define REPLAY_RECORD_SCRATCH_SIZE	350*1024	// Probably tweakable down...need to look into it.
#define REPLAY_DEBUG_MEM_FILL		1 && __BANK

class CReplayRecorder
{
public:
	CReplayRecorder()
		: m_pBuffer(NULL)
		, m_bufferSize(0)
		, m_pPosition(NULL)
	{}

	CReplayRecorder(u8* pBuffer, u32 size)
		: m_pBuffer(pBuffer)
		, m_bufferSize(size)
		, m_pPosition(m_pBuffer)
	{}

	template<typename PACKETTYPE, typename PARAM1>
	PACKETTYPE*	RecordPacketWithParam(PARAM1 param1)
	{
		PACKETTYPE* pPacket = NULL;
#if REPLAY_RECORD_SAFETY
		pPacket = (PACKETTYPE*)GetScratch(m_pPosition);
		pPacket->Store(param1);
		pPacket->StoreExtensions(param1);
		replayAssert(pPacket->GetPacketSize() < REPLAY_RECORD_SCRATCH_SIZE);

		REPLAY_CHECK(m_bufferSize - (m_pPosition - m_pBuffer) > pPacket->GetPacketSize(), NULL, "Cannot store packet as buffer in Recorder is too small (%u)", m_bufferSize);

		sysMemCpy(m_pPosition, pPacket, pPacket->GetPacketSize());

		pPacket = (PACKETTYPE*)m_pPosition;
#else
		pPacket = (PACKETTYPE*)m_pPosition;
		pPacket->Store(param1);
		pPacket->StoreExtensions(param1);
#endif // REPLAY_RECORD_SAFETY

		pPacket->ValidatePacket();
		m_pPosition += pPacket->GetPacketSize();
		return BoundsCheck(m_pPosition) ? pPacket : NULL;
	}

	template<typename PACKETTYPE, typename PARAM1, typename PARAM2>
	PACKETTYPE*	RecordPacketWithParams(PARAM1 param1, PARAM2 param2)
	{
		PACKETTYPE* pPacket = NULL;
#if REPLAY_RECORD_SAFETY
		pPacket = (PACKETTYPE*)GetScratch(m_pPosition);
		pPacket->Store(param1, param2);
		pPacket->StoreExtensions(param1, param2);
		replayAssert(pPacket->GetPacketSize() < REPLAY_RECORD_SCRATCH_SIZE);

		REPLAY_CHECK(m_bufferSize - (m_pPosition - m_pBuffer) > pPacket->GetPacketSize(), NULL, "Cannot store packet as buffer in Recorder is too small (%u)", m_bufferSize);

		sysMemCpy(m_pPosition, pPacket, pPacket->GetPacketSize());

		pPacket = (PACKETTYPE*)m_pPosition;
#else
		pPacket = (PACKETTYPE*)m_pPosition;
		pPacket->Store(param1, param2);
		pPacket->StoreExtensions(param1, param2);
#endif // REPLAY_RECORD_SAFETY

		pPacket->ValidatePacket();
		m_pPosition += pPacket->GetPacketSize();
		return BoundsCheck(m_pPosition) ? pPacket : NULL;
	}


	template<typename PACKETTYPE, typename PARAM1, typename PARAM2, typename PARAM3>
	PACKETTYPE*	RecordPacketWithParams(PARAM1 param1, PARAM2 param2, PARAM3 param3)
	{
		PACKETTYPE* pPacket = NULL;
#if REPLAY_RECORD_SAFETY
		pPacket = (PACKETTYPE*)GetScratch(m_pPosition);
		pPacket->Store(param1, param2, param3);
		pPacket->StoreExtensions(param1, param2, param3);
		replayAssert(pPacket->GetPacketSize() < REPLAY_RECORD_SCRATCH_SIZE);

		REPLAY_CHECK(m_bufferSize - (m_pPosition - m_pBuffer) > pPacket->GetPacketSize(), NULL, "Cannot store packet as buffer in Recorder is too small (%u)", m_bufferSize);

		sysMemCpy(m_pPosition, pPacket, pPacket->GetPacketSize());

		pPacket = (PACKETTYPE*)m_pPosition;
#else
		pPacket = (PACKETTYPE*)m_pPosition;
		pPacket->Store(param1, param2, param3);
		pPacket->StoreExtensions(param1, param2, param3);
#endif // REPLAY_RECORD_SAFETY

		pPacket->ValidatePacket();
		m_pPosition += pPacket->GetPacketSize();
		return BoundsCheck(m_pPosition) ? pPacket : NULL;
	}

	template<typename PACKETTYPE>
	PACKETTYPE*	RecordPacket()
	{
		PACKETTYPE* pPacket = NULL;
#if REPLAY_RECORD_SAFETY
		pPacket = (PACKETTYPE*)GetScratch(m_pPosition);
		pPacket->Store();
		pPacket->StoreExtensions();
		replayAssert(pPacket->GetPacketSize() < REPLAY_RECORD_SCRATCH_SIZE);

		REPLAY_CHECK(m_bufferSize - (m_pPosition - m_pBuffer) > pPacket->GetPacketSize(), NULL, "Cannot store packet as buffer in Recorder is too small (%u)", m_bufferSize);

		sysMemCpy(m_pPosition, pPacket, pPacket->GetPacketSize());

		pPacket = (PACKETTYPE*)m_pPosition;
#else
		pPacket = (PACKETTYPE*)m_pPosition;		
		pPacket->Store();
		pPacket->StoreExtensions();
#endif // REPLAY_RECORD_SAFETY

		pPacket->ValidatePacket();
		m_pPosition += pPacket->GetPacketSize();
		return BoundsCheck(m_pPosition) ? pPacket : NULL;
	}

	template<typename PACKETTYPE>
	PACKETTYPE*	RecordPacket(const PACKETTYPE& packet)
	{
		REPLAY_CHECK(m_bufferSize - (m_pPosition - m_pBuffer) > packet.GetPacketSize(), NULL, "Cannot store packet as buffer in Recorder is too small (%u)", m_bufferSize);

		sysMemCpy(m_pPosition, &packet, packet.GetPacketSize());

		PACKETTYPE* pPacket = (PACKETTYPE*)m_pPosition;
		pPacket->ValidatePacket();
		m_pPosition += pPacket->GetPacketSize();
		return BoundsCheck(m_pPosition) ? pPacket : NULL;
	}

	bool	IsSpaceForPacket(u32 packetSize) const		{	return (m_bufferSize - GetMemUsed()) > packetSize;	}
	void	Reset()										{	m_pPosition = m_pBuffer;				}
	u8*		GetBuffer()									{	return m_pBuffer;						}
	u32		GetMemUsed() const							{	return (u32)(m_pPosition - m_pBuffer);	}
	u32		GetBufferSize() const						{	return m_bufferSize;					}

	template<typename PACKETTYPE>
	PACKETTYPE*	GetFirstPacket()
	{
		if(m_pBuffer == m_pPosition)
			return NULL;

		PACKETTYPE* pPacket = (PACKETTYPE*)m_pBuffer;
		pPacket->ValidatePacket();
		return pPacket;
	}

	template<typename PACKETTYPE>
	const PACKETTYPE*	GetFirstPacket() const
	{
		if(m_pBuffer == m_pPosition)
			return NULL;

		const PACKETTYPE* pPacket = (const PACKETTYPE*)m_pBuffer;
		pPacket->ValidatePacket();
		return pPacket;
	}

	template<typename PACKETTYPE>
	PACKETTYPE* GetNextPacket(const CPacketBase* pCurrPacket)
	{
		if(((u8*)pCurrPacket + pCurrPacket->GetPacketSize()) == m_pPosition)
			return NULL;

		PACKETTYPE* pPacket = (PACKETTYPE*)((u8*)pCurrPacket + pCurrPacket->GetPacketSize());
		pPacket->ValidatePacket();
		return pPacket;
	}

	template<typename PACKETTYPE>
	const PACKETTYPE* GetNextPacket(const CPacketBase* pCurrPacket) const
	{
		if(((u8*)pCurrPacket + pCurrPacket->GetPacketSize()) == m_pPosition)
			return NULL;

		const PACKETTYPE* pPacket = (const PACKETTYPE*)((u8*)pCurrPacket + pCurrPacket->GetPacketSize());
		if(!pPacket->ValidatePacket())
		{
			DumpReplayPackets((char*)pCurrPacket, (u32)((u8*)pCurrPacket - m_pBuffer));
			replayFatalAssertf(false, "ReplayRecorder: Failed to validate packet after going to the next one (current %p)...see log", pCurrPacket);
		}
		return pPacket;
	}

	template<typename PACKETTYPE>
	PACKETTYPE*	RemovePacket(const CPacketBase* pPacketToRemove)
	{
#if REPLAY_DEBUG_MEM_FILL && REPLAY_GUARD_ENABLE
		const size_t offsetToGuard = pPacketToRemove->GetGuardOffset();
#endif // REPLAY_DEBUG_MEM_FILL

		replayFatalAssertf((u8*)pPacketToRemove < m_pPosition, "Trying to remove a packet that is after the position? %p %p, Previous %p", pPacketToRemove, m_pPosition, m_pPreviouslyRemoved);

		pPacketToRemove->ValidatePacket();
		tPacketSize sizeToRemove = pPacketToRemove->GetPacketSize();
		if(m_pPosition == (u8*)pPacketToRemove + sizeToRemove)
		{
			m_pPosition -= sizeToRemove;
#if REPLAY_DEBUG_MEM_FILL && REPLAY_GUARD_ENABLE
			memset(m_pPosition + offsetToGuard, 0xEE, 4);	// Set the guard to 0xEEEEEEEE so we know it's deleted
			replayDebugf3("Removing last packet in the monitor buffer... %p %u %p, Previous %p", pPacketToRemove, sizeToRemove, m_pPosition, m_pPreviouslyRemoved);
			m_pPreviouslyRemoved = (u8*)pPacketToRemove;
#endif // REPLAY_DEBUG_MEM_FILL
			return NULL;
		}

		size_t sizeToMove = m_pPosition - (u8*)pPacketToRemove - sizeToRemove;
		u8* destination = (u8*)pPacketToRemove;
		u8* source = (u8*)pPacketToRemove + sizeToRemove;

		BoundsCheck(destination);
		BoundsCheck(source);

		if(source < m_pBuffer || source > m_pPosition ||
			(source + sizeToMove) > m_pPosition ||
			destination < m_pBuffer || destination > m_pPosition ||
			(destination + sizeToMove) > m_pPosition ||
			sizeToMove > m_bufferSize)
		{
			replayFatalAssertf(false, "m_pPosition:%p, m_pBuffer:%p, source:%p, destination:%p, pPacketToRemove:%p, sizeToMove:%u, sizeToRemove:%u, Previous %p", m_pPosition, m_pBuffer, source, destination, pPacketToRemove, (u32)sizeToMove, sizeToRemove, m_pPreviouslyRemoved);
		}
		memmove(destination, source, sizeToMove);
		
		m_pPosition -= sizeToRemove;
#if REPLAY_DEBUG_MEM_FILL && REPLAY_GUARD_ENABLE
		memset(m_pPosition + offsetToGuard, 0xEE, 4);	// Set the guard to 0xEEEEEEEE so we know it's deleted
#endif // REPLAY_DEBUG_MEM_FILL

		PACKETTYPE* pPacket = (PACKETTYPE*)destination;
		replayFatalAssertf(pPacket != NULL, "m_pPosition:%p, m_pBuffer:%p, source:%p, destination:%p, pPacketToRemove:%p, sizeToMove:%u, sizeToRemove:%u, Previous %p", m_pPosition, m_pBuffer, source, destination, pPacketToRemove, (u32)sizeToMove, sizeToRemove, m_pPreviouslyRemoved);
		if(!pPacket->ValidatePacket() || pPacket != pPacketToRemove)
		{
			DumpReplayPackets((char*)pPacket, (u32)((u8*)pPacket - m_pBuffer));
			replayFatalAssertf(false, "ReplayRecorder: Failed to validate packet after removing one (removed %p)...see log (%p, %u, %p, Previous %p)", pPacketToRemove, pPacket, pPacket->GetPacketSize(), m_pPosition, m_pPreviouslyRemoved);
		}

		m_pPreviouslyRemoved = (u8*)pPacketToRemove;
		return pPacket;
	}

private:

	bool	BoundsCheck(const u8* ptr) const
	{
		REPLAY_CHECK(ptr >= m_pBuffer && ptr < (m_pBuffer + m_bufferSize), false, "Replay Recorder Bounds Failure");
		return true;
	}

	u8*		m_pBuffer;
	u32		m_bufferSize;

	u8*		m_pPosition;
	u8*		m_pPreviouslyRemoved;

#if REPLAY_RECORD_SAFETY
	static __THREAD u8*	sm_scratch;
	static const int sm_scatchBufferCount = 3;
	static u8*	sm_scratchBuffers[sm_scatchBufferCount];
	static u8*	GetScratch(u8* destPtr)
	{
		replayFatalAssertf(sm_scratch != NULL, "Failed to allocate scratch buffer");

		size_t diff = (size_t)destPtr - ((size_t)destPtr & ~(REPLAY_DEFAULT_ALIGNMENT-1));
		return (u8*)(REPLAY_ALIGN((size_t)sm_scratch, REPLAY_DEFAULT_ALIGNMENT) + diff);
	}

public:
	static void AllocateScratch()
	{
		for(int i = 0; i < sm_scatchBufferCount; ++i)
		{
			const u16 aligment = 16;				
			if(sm_scratchBuffers[i] == NULL)
			{
				sm_scratchBuffers[i] = (u8*)sysMemManager::GetInstance().GetReplayAllocator()->Allocate(REPLAY_RECORD_SCRATCH_SIZE, aligment);
			}
		}
	}

	static void	FreeScratch()
	{
		for(int i = 0; i < sm_scatchBufferCount; ++i)
		{
			sysMemManager::GetInstance().GetReplayAllocator()->Free(sm_scratchBuffers[i]);
			sm_scratchBuffers[i] = NULL;
		}
	}

	static void GetScratch(int threadIndex)
	{
		replayFatalAssertf(threadIndex >= 1, "Invalid thread index %d", threadIndex);
		sm_scratch = sm_scratchBuffers[threadIndex-1];
	}
#endif // REPLAY_RECORD_SAFETY
};


#endif // GTA_REPLAY

#endif // REPLAYRECORDER_H_
