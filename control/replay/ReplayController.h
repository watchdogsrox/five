#ifndef REPLAYPLAYBACKCONTROLLER_H_
#define REPLAYPLAYBACKCONTROLLER_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "ReplaySupportClasses.h"
#include "Misc\ReplayPacket.h"
#include "ReplayPacketIDs.h"

//////////////////////////////////////////////////////////////////////////
class ReplayController
{
public:
	ReplayController(CAddressInReplayBuffer& currAddress, CBufferInfo& bufferInfo, FrameRef startingFrame = FrameRef(0, 0));
	~ReplayController();

	bool			EnableRecording(u32 addressLimit, bool canGotoNextBlock);
	void			RenableRecording();
	void			DisableRecording();
	bool			IsThereSpaceToRecord(u32 volume, u32 blockSize) const;

	bool			EnablePlayback();
	void			DisablePlayback();

	bool			EnableModify();
	void			DisableModify();

	bool			IsNextFrame();
	bool			IsEndOfPlayback();

	bool			IsLastBlock() const				{	return m_bufferInfo.FindLastBlock() == m_currentAddress.GetBlock();	}

	bool			IsEndOfBlock() const			{	return GetCurrentPacketID() == PACKETID_END || GetCurrentPacketID() == PACKETID_END_HISTORY;	}

	eReplayPacketId	GetCurrentPacketID() const		{	return m_currentPacketID = m_currentAddress.GetPacketID();					}
	
	FrameRef		GetCurrentFrameRef() const		{	return FrameRef(m_currentAddress.GetSessionBlockIndex(), m_currentFrame);	}
	void			IncrementFrame(s32 f)			{	m_currentFrame = (u16)((s32)m_currentFrame + f);							}

	void			RecordBuffer(const u8* pBuffer, u32 memSize)
	{
		replayAssert(m_currentAddress.GetBlock()->IsLockedForWrite());
		memcpy(m_currentAddress.GetBufferPosition<u8>(), pBuffer, memSize);
		m_currentAddress.IncPosition(memSize);
	}
	//////////////////////////////////////////////////////////////////////////
	// Return the current packet as a specific type
	// Also set the current packet size correctly
	template<typename T>
	T const*		ReadCurrentPacket();

	template<typename T>
	T*				GetCurrentPacket();

	//////////////////////////////////////////////////////////////////////////
	// Same as above but set the size to different
	// This would be used if we wanted a base class of a packet but still need
	// to increment properly (size will be >= than sizeof(T))

	template<typename T, typename PARAM1>
	T*				RecordPacketWithParam(PARAM1 param1, u32 expectedSize = 0);

	template<typename T, typename PARAM1, typename PARAM2>
	T*				RecordPacketWithParams(PARAM1 param1, PARAM2 param2);

	template<typename T>
	T*				RecordPacket(u32 expectedSize = 0);

	template<typename T>
	T*				RecordPacket(const T& packet);

	void			RecordPacketBlock(u8* pData, u32 dataSize);

	template<typename T>
	T*				SetPacket();

	template<typename T>
	T*				SetPacket(const T& packet);



	inline	void	AdvancePacket()						{	m_currentAddress.IncPosition(m_currentPacketSize);	}
	template<typename T>
	inline	void	AdvancePacket(T* pPacket)			{	m_currentAddress.IncPosition(pPacket->GetPacketSize());	}
	inline	void	AdvanceUnknownPacket(u32 size)		{	m_currentAddress.IncPosition(size);				}
	void			AdvanceBlock();
	bool			GoToNextBlock();

	const CAddressInReplayBuffer&	GetAddress() const	{	return m_currentAddress;						}
	CBufferInfo&	GetBufferInfo()						{	return m_bufferInfo;							}
	const CBufferInfo&	GetBufferInfo() const			{	return m_bufferInfo;							}

	inline	CBlockInfo*	GetCurrentBlock() const			{	return m_currentAddress.GetBlock();				}
	inline	u8*		GetCurrentBuffer() const			{	return m_currentAddress.GetBlock()->GetData();	}
	inline	u32		GetCurrentPosition() const			{	return m_currentAddress.GetPosition();			}
	inline	u16		GetCurrentBlockIndex() const		{	return m_currentAddress.GetMemoryBlockIndex();	}
	inline	u32		GetAddressLimit() const				{	return m_addressLimit;							}

	template<typename T>
	void			GetNextPacket(T* pPrevPacket, T*& pNextPacket) const
	{
		pNextPacket = NULL;

		if(pPrevPacket->GetNextPosition() == (u32)-1)
		{
			return;
		}

		CAddressInReplayBuffer rBufferAddress;
		rBufferAddress.SetPosition(pPrevPacket->GetNextPosition());
		rBufferAddress.SetBlock(m_bufferInfo.FindBlockFromSessionIndex(pPrevPacket->GetNextBlockIndex()));

		if(!rBufferAddress.GetBlock())
		{
			return;
		}
		REPLAY_CHECK(rBufferAddress.GetBlock()->GetSessionBlockIndex() == pPrevPacket->GetNextBlockIndex(), NO_RETURN_VALUE, "Block is incorrect here... expected %u, but got %u", pPrevPacket->GetNextBlockIndex(), rBufferAddress.GetBlock()->GetSessionBlockIndex());

		pNextPacket = rBufferAddress.GetBufferPosition<T>();
		if(!pNextPacket->ValidatePacket())
		{
			DumpReplayPackets((char*)pNextPacket);
			replayAssertf(false, "Failed to validate packet in ReplayController::GetNextPacket");
		}
	}

	void	SetPlaybackFlags(CReplayState flags)		{	m_playbackFlags = flags;	}
	const	CReplayState&	GetPlaybackFlags() const	{	return m_playbackFlags;		}
	CReplayState&	GetPlaybackFlags()					{	return m_playbackFlags;		}
	void	SetFineScrubbing(bool b)					{	m_fineScrubbing = b; }
	bool	IsFineScrubbing() const						{	return m_fineScrubbing; }

	void	SetIsFirstFrame(bool b)						{	m_isFirstFrame = b;			}
	bool	GetIsFirstFrame() const						{	return m_isFirstFrame;		}

	void	SetInterp(float f)							{	m_interp = f;				}
	float	GetInterp() const							{	return m_interp;			}

	void	SetFrameInfos(const CFrameInfo& first, const CFrameInfo& current, const CFrameInfo& next, const CFrameInfo& last)	{	m_firstFrameInfo = first; m_currentFrameInfo = current;	m_nextFrameInfo = next; m_lastFrameInfo = last;	}
	const CFrameInfo&	GetFirstFrameInfo() const		{	return m_firstFrameInfo;		}
	const CFrameInfo&	GetCurrentFrameInfo() const		{	return m_currentFrameInfo;		}
	const CFrameInfo&	GetNextFrameInfo() const		{	return m_nextFrameInfo;		}
	const CFrameInfo&	GetLastFrameInfo() const		{	return m_lastFrameInfo;			}

private:
	// Prevent copying...prevent passing by value
	ReplayController(const ReplayController& other)
	: m_currentAddress(other.m_currentAddress)
	, m_bufferInfo(other.m_bufferInfo)
	{}

	template<typename T>
	T*				PrepareToSetPacket();

	template<typename T>
	void			PostSetPacket(const T* pPacket);

	template<typename T>
	bool			CheckPacket(const T* pPacket)
	{
		if(pPacket->ValidatePacket() == false)
		{
			DumpReplayPackets((char*)pPacket, GetPrev() != NULL ? (u32)((char*)pPacket - (char*)GetPrev()) : 0);
			replayFatalAssertf(false, "Packet is invalid - %u", (u32)pPacket->GetPacketID());
			return false;
		}
		return true;
	}

	CAddressInReplayBuffer&	m_currentAddress;
	CBufferInfo&			m_bufferInfo;
	sysCriticalSectionToken m_CriticalSectionToken;

	mutable eReplayPacketId	m_currentPacketID;
	u32						m_currentPacketSize;
	
#define STORE_PREV_PACKETS 0 && __BANK
#if STORE_PREV_PACKETS
	static const int		prevCount = 50;
	void					ShufflePrev(const CPacketBase* p)
	{
		for(int i = prevCount-1; i > 0; --i)
		{
			m_prevPacket[i] = m_prevPacket[i-1];
		}
		m_prevPacket[0] = p;
	}
	void					ResetPrevs()
	{
		for(int i = 0; i < prevCount; ++i)
			m_prevPacket[i] = NULL;
	}
	const CPacketBase*		GetPrev() const						{ return m_prevPacket[0]; }
	const CPacketBase*		m_prevPacket[prevCount];
#else
	void					ShufflePrev(const CPacketBase* p)	{ m_prevPacket = p; }
	void					ResetPrevs()						{ m_prevPacket = NULL; }
	const CPacketBase*		GetPrev() const						{ return m_prevPacket; }
	const CPacketBase*		m_prevPacket;
#endif // STORE_PREV_PACKETS

	enum eControllerMode
	{
		None,
		Recording,
		Modify,
		Playing,
	};
	eControllerMode			m_mode;
	eControllerMode			m_prevMode;

	u16						m_currentFrame;

	CReplayState			m_playbackFlags;
	bool					m_fineScrubbing;

	bool					m_isFirstFrame;
	u32						m_addressLimit;

	float					m_interp;

	CFrameInfo				m_firstFrameInfo;
	CFrameInfo				m_currentFrameInfo;
	CFrameInfo				m_nextFrameInfo;
	CFrameInfo				m_lastFrameInfo;
};



//////////////////////////////////////////////////////////////////////////
// Return the current packet as a specific type
// Also set the current packet size correctly
template<typename T>
T const* ReplayController::ReadCurrentPacket()
{
	replayFatalAssertf(m_mode != None, "");
	T* pPacket = m_currentAddress.GetBufferPosition<T>();
	if(CheckPacket(pPacket) == false)
	{
		return NULL;
	}
	m_currentPacketSize = pPacket ? pPacket->GetPacketSize() : 0;
	m_currentPacketID = pPacket ? pPacket->GetPacketID() : PACKETID_INVALID;
	ShufflePrev(pPacket);
	return pPacket;
}


template<typename T>
T* ReplayController::GetCurrentPacket()
{
	replayFatalAssertf(m_mode != None && m_mode != Playing, "");
	T* pPacket = m_currentAddress.GetBufferPosition<T>();
	if(CheckPacket(pPacket) == false)
	{
		return NULL;
	}
	m_currentPacketSize = pPacket ? pPacket->GetPacketSize() : 0;
	m_currentPacketID = pPacket ? pPacket->GetPacketID() : PACKETID_INVALID;
	ShufflePrev(pPacket);
	return pPacket;
}


template<typename T, typename PARAM1>
T* ReplayController::RecordPacketWithParam(PARAM1 param1, u32 ASSERT_ONLY(expectedSize))
{
	replayFatalAssertf(m_mode == Recording, "Controller is not in recording mode");
	ASSERT_ONLY(u32 currPos = GetCurrentPosition();)

	T* pPacket = PrepareToSetPacket<T>();
	pPacket->Store(param1);
	PostSetPacket(pPacket);

	AdvancePacket(pPacket);

	replayFatalAssertf(expectedSize == 0 || (GetCurrentPosition() - currPos) == expectedSize, "Incorrect size...expected %u, got %u while recording packet 0x%X", expectedSize, GetCurrentPosition() - currPos, pPacket->GetPacketID());

	return pPacket;
}

template<typename T, typename PARAM1, typename PARAM2>
T* ReplayController::RecordPacketWithParams(PARAM1 param1, PARAM2 param2)
{
	replayFatalAssertf(m_mode == Recording, "Controller is not in recording mode");
	T* pPacket = PrepareToSetPacket<T>();
	pPacket->Store(param1, param2);
	PostSetPacket(pPacket);

	AdvancePacket(pPacket);

	return pPacket;
}


template<typename T>
T* ReplayController::RecordPacket(u32 ASSERT_ONLY(expectedSize))
{
	replayFatalAssertf(m_mode == Recording, "Controller is not in recording mode");
	ASSERT_ONLY(u32 currPos = GetCurrentPosition();)
	T* pPacket = SetPacket<T>();

	AdvancePacket();

	replayFatalAssertf(expectedSize == 0 || (GetCurrentPosition() - currPos) == expectedSize, "Incorrect size...expected %u, got %u while recording packet 0x%X", expectedSize, GetCurrentPosition() - currPos, pPacket->GetPacketID());

	return pPacket;
}

template<typename T>
T* ReplayController::RecordPacket(const T& packet)
{
	T* pPacket = SetPacket<T>(packet);

	AdvancePacket();

	return pPacket;
}

template<typename T>
T* ReplayController::SetPacket()
{
	replayFatalAssertf(m_mode == Recording, "Controller is not in recording mode");
	T* pPacket = PrepareToSetPacket<T>();
	pPacket->Store();
	PostSetPacket(pPacket);

	return pPacket;
}


template<typename T>
T* ReplayController::SetPacket(const T& packet)
{
	replayFatalAssertf(m_mode == Recording, "Controller is not in recording mode");
	T* pPacket = PrepareToSetPacket<T>();
	*pPacket = packet;
	PostSetPacket(pPacket);

	return pPacket;
}


template<typename T>
T* ReplayController::PrepareToSetPacket()
{
	u32 sizeOfPacketToRecord = sizeof(T);
	replayFatalAssertf(m_currentAddress.GetIndexLimit() - m_currentAddress.GetPosition() >= sizeOfPacketToRecord, "Not enough space to store packet");
	replayFatalAssertf(m_mode == Recording, "Controller trying to record but is not in recording mode");

	m_currentPacketSize = sizeOfPacketToRecord;
	return m_currentAddress.GetBufferPosition<T>();
}

template<typename T>
void ReplayController::PostSetPacket(const T* pPacket)
{
	m_currentPacketID = pPacket->GetPacketID();
	replayFatalAssertf(pPacket->ValidatePacket(), "Packet is invalid %u", (u32)m_currentPacketID);
}


#endif // GTA_REPLAY

#endif // REPLAYPLAYBACKCONTROLLER_H
