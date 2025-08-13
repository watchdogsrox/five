#ifndef _REPLAYSUPPORTCLASSES_H_
#define _REPLAYSUPPORTCLASSES_H_

#include "control/replay/ReplaySettings.h"

#if GTA_REPLAY

#include "ReplayEnums.h"
#include "replay_channel.h"
#include "File/device_replay.h"

#include "math/amath.h"
#include "system/new.h"
#include "system/memops.h"
#include "system/criticalsection.h"
#include "system/stack.h"
#include "grcore/debugdraw.h"
#include "grcore/texture.h"
#include "ReplayBufferMarker.h"
#include "debug/Debug.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/ViewportFader.h"
#include "SaveLoad/savegame_photo_async_buffer.h"

namespace rage
{
	class audSound;
	class audWaveSlot;
	class atString;
}

class CPacketFrame;

#if !RSG_ORBIS
#pragma warning(push)
#pragma warning(disable:4201)	// Disable unnamed union warning
#endif //!RSG_ORBIS


//////////////////////////////////////////////////////////////////////////
// Header for replay file
//

// Version format
//
// ymddhmmX
// y - year in hex, 2010 is zero
// m - month in hex
// dd - day in hex
// hh - hour in hex
// mm - minutes in hex

//***CHANGE ME TO PREVENT OLD CLIP LOADING***
#define REPLAYVERSION			0x42190F20
#define HEADERVERSION_V1		1
#define HEADERVERSION_V2		2
#define HEADERVERSION_V3		3
#define HEADERVERSION_V4		4
#define HEADERVERSION_V5		5
#define CURRENTHEADERVERSION	HEADERVERSION_V5 // Clip will get updated to current version on load

#define REPLAYVERSIONDATELEN	12	// 'Mmm dd yyyy'
#define REPLAYVERSIONTIMELEN	9	// 'hh:mm:ss'


static const u32 SCREENSHOT_WIDTH					= 512;
static const u32 SCREENSHOT_HEIGHT					= 256;

static const u32 MAX_EXTRACONTENTHASHES				= 512;

#define REPLAY_USE_PER_BLOCK_THUMBNAILS		1

#if RSG_PC
// Use a list of render targets on PC to avoid stalls when copy data out.
#define	REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE 5
#else // RSG_PC
#define	REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE 1
#endif // RSG_PC

//////////////////////////////////////////////////////////////////////////
struct ReplayHeader
{
	u32 uFileType;
	u32 uFileVersion;

	u32 uSize;
	u32 uHeaderVersion;

	char replayVersionDate[REPLAYVERSIONDATELEN];
	char replayVersionTime[REPLAYVERSIONTIMELEN];

	float fClipTime;

	u32 PlatformType;
	u32	PhysicalBlockSize;
	u16 PhysicalBlockCount;
	u16	TotalNumberOfBlocks;
	s16	Unused0;			// Unused
	u32 MarkerCount;

	char creationDateTime[REPLAY_DATETIME_STRING_LENGTH];
	char MontageFilter[MAX_FILTER_LENGTH];
	char MissionNameFilter[MAX_FILTER_LENGTH];

	ReplayBufferMarker eventMarkers[REPLAY_HEADER_EVENT_MARKER_MAX];

	bool bSaved;
	bool bCompressed;
	bool bHasGuards;
	u8 uImportance;
	u8 uMission;

	u32 extraContentHashCount;
	u32 extraContentHashes[MAX_EXTRACONTENTHASHES];
	bool bFavourite;
	ClipUID uID;

	u32		previousSequenceHash;
	u32		nextSequenceHash;

	u32		frameFlags;

	bool	wasModifiedContent;

	atHashString	ActiveIslandHash;

	void Reset()
	{
		uSize			= sizeof(ReplayHeader);
		uFileType		= 'RPLY';
		uFileVersion	= GetVersionNumber();
		uHeaderVersion	= CURRENTHEADERVERSION;
		fClipTime		= 0.0f;
		PlatformType = 0;
		PhysicalBlockSize		= MIN_BYTES_PER_REPLAY_BLOCK;
		PhysicalBlockCount		= MIN_NUM_REPLAY_BLOCKS;
		TotalNumberOfBlocks		= 0;
		Unused0					= 0;
		memset(creationDateTime, 0, sizeof(creationDateTime));
		memset(MontageFilter,	 0, sizeof(MontageFilter));
		memset(MissionNameFilter,	 0, sizeof(MissionNameFilter));
		memset(replayVersionDate,	 0, sizeof(replayVersionDate));
		memset(replayVersionTime,	 0, sizeof(replayVersionTime));
		bSaved					= false;
		bCompressed				= false;
		bHasGuards				= REPLAY_GUARD_ENABLE == 1;
		uImportance				= 0;
		uMission				= 0;
		extraContentHashCount	= 0;
		memset(extraContentHashes, 0, sizeof(extraContentHashes));
		bFavourite = false;
		uID = 0;
		previousSequenceHash = 0;
		nextSequenceHash = 0;
		frameFlags = 0;
		wasModifiedContent = false;
		ActiveIslandHash = ATSTRINGHASH("", 0);
		
		//reset event markers.
		for(int i = 0; i < REPLAY_HEADER_EVENT_MARKER_MAX; ++i)
		{
			eventMarkers[i].Reset();
		}

		strcpy_s(replayVersionDate, REPLAYVERSIONDATELEN, __DATE__);
		strcpy_s(replayVersionTime, REPLAYVERSIONTIMELEN, __TIME__);
	}

	static u32 GetCurrentPlatform()
	{
#if RSG_PC
		return 0;
#elif RSG_DURANGO
		return 1;
#elif RSG_ORBIS
		return 2;
#else
		return 3;
#endif
	}

	static const char* GetPlatformString(u32 id)
	{
		switch(id)
		{
		case 0:
			return "PC";
		case 1:
			return "XBONE";
		case 2:
			return "PS4";
		default:
			return "UNKNOWN";
		}
	}

	static u32	GetVersionNumber()
	{
		return CDebug::GetReplayVersionNumber();
	}

	static void GenerateDateString(atString& string, u32& uid);
	static time_t					sm_SaveTimeStamp;
};

//////////////////////////////////////////////////////////////////////////
struct ReplayBlockHeader
{
	ReplayBlockHeader()
	{
		Reset();
	}

	u32 uSize;
	u32 uHeaderVersion;

	u32 m_CompressedSize;
	u8	m_Status;
	u16 m_SessionBlock;
	u32 m_SizeUsed;
	u32 m_SizeLost;
	u32	m_StartTime;
	u32 m_CheckSum;

	u32 blah;

	void Reset()
	{
		uSize = sizeof(ReplayBlockHeader);
		uHeaderVersion = 0;

		m_CompressedSize = 0;
		m_Status = 0;
		m_SessionBlock = 0;
		m_SizeUsed = 0;
		m_SizeLost = 0;
		m_StartTime = 0;
		m_CheckSum = 0;
	}

	void Validate()
	{
		GetCheckSum(m_CheckSum);
	}

	void GetCheckSum(u32 &value)
	{
		value = 0;
		value ^= m_CompressedSize;
		value ^= (u32)m_Status;
		value ^= m_SessionBlock;
		value ^= m_SizeUsed;
		value ^= m_SizeLost;
		value ^= m_StartTime;
		value ^= m_CompressedSize;
	}

	bool IsValid()
	{
		u32 value = 0;
		GetCheckSum(value);

		return ( m_CheckSum == value );
	}
};

//////////////////////////////////////////////////////////////////////////
// Generic replay Identifier
class CReplayID
{
public:
	CReplayID() : m_ID(-1)	{}
	CReplayID(s32 id) : m_ID(id)	{}
	CReplayID(s16 trackerID, s16 instID) : m_TrackerID(trackerID), m_InstID(instID) {}

	s16 GetEntityID() const							{	return m_TrackerID;			}
	s16 GetInstID() const							{	return m_InstID;			}

	s32		ToInt() const							{	return m_ID;				}
	operator int() const							{	return m_ID;				}

	bool operator==(const CReplayID& other)	const	{	return m_ID == other.m_ID;	}
	bool operator>=(const s32 i) const				{	return m_ID >= i;			}

private:
	union
	{
		struct
		{
			s16 m_TrackerID;
			s16 m_InstID;
		};
		s32 m_ID;
	};
};

const CReplayID	ReplayIDInvalid(-1);
const CReplayID	NoEntityID(-2);

class FrameRef
{
	static const u32 INVALID_FRAME_VALUE	= 0xFFFFFFFF;
public:
	static const FrameRef INVALID_FRAME_REF;

	FrameRef()
		: m_FrameRef(INVALID_FRAME_VALUE)
	{}

	FrameRef(u32 frameAsU32)
	{
		m_FrameRef = frameAsU32;
	}

	FrameRef(u16 block, u16 frame)
	{
		m_BlockIndex = block;
		m_Frame = frame;

		Assertf(m_FrameRef != INVALID_FRAME_VALUE, "Run out of space to store this in a frame ref!");
	}

	u32 GetAsU32() const { Assertf(m_FrameRef != INVALID_FRAME_VALUE, "This FrameRef has not been set!"); return m_FrameRef; }
	u16 GetFrame() const { Assertf(m_FrameRef != INVALID_FRAME_VALUE, "This FrameRef has not been set!"); return m_Frame; }
	u16 GetBlockIndex() const { Assertf(m_FrameRef != INVALID_FRAME_VALUE, "This FrameRef has not been set!"); return m_BlockIndex; }

	FrameRef GetAbsolutePosition(u16 startBlock, u16 blockCount) const
	{
 		s32 absBlock = 0;

		absBlock = ((s32)m_BlockIndex - (s32)startBlock) % blockCount;
		absBlock = absBlock < 0 ? absBlock + blockCount : absBlock;

		return FrameRef((u16)absBlock, m_Frame);
	}
	void IncrementBlock(s32 b, u16 f)	
	{ 
		m_BlockIndex = (u16)((s32)m_BlockIndex + b); 
		m_Frame = f;
	}
	void IncrementFrame(s32 f)
	{	
		m_Frame = (u16)((s32)m_Frame + f);
	}

	bool operator==(const FrameRef& other) const	{	return m_FrameRef == other.m_FrameRef;	}
	bool operator<=(const FrameRef& other) const
	{	
		if(m_BlockIndex > other.m_BlockIndex)
			return false;
		else if(m_BlockIndex == other.m_BlockIndex && m_Frame > other.m_Frame)
			return false;

		return true;
	}
	bool operator>=(const FrameRef& other) const
	{	
		if(m_BlockIndex < other.m_BlockIndex)
			return false;
		else if(m_BlockIndex == other.m_BlockIndex && m_Frame < other.m_Frame)
			return false;

		return true;
	}
	bool operator>(const FrameRef& other) const
	{
		if(m_BlockIndex > other.m_BlockIndex)
			return true;
		else if(m_BlockIndex == other.m_BlockIndex && m_Frame > other.m_Frame)
			return true;

		return false;
	}
	bool operator<(const FrameRef& other) const
	{	
		if(m_BlockIndex < other.m_BlockIndex)
			return true;
		else if(m_BlockIndex == other.m_BlockIndex && m_Frame < other.m_Frame)
			return true;

		return false;
	}
	bool operator!=(const FrameRef& other) const
	{
		return m_FrameRef != other.m_FrameRef;
	}

private:

	union
	{
		struct
		{
			u16 m_BlockIndex;
			u16 m_Frame;
		};

		u32 m_FrameRef;
	};
};


template<typename T>
class CEntityGet
{
public:
	CEntityGet(const CReplayID& id)
		: m_entityID(id)
		, m_pEntity(NULL)
		, m_alreadyReported(false)
		, m_recentlyUnlinked(false)
	{}

	CReplayID	m_entityID;
	T*			m_pEntity;
	bool		m_alreadyReported;
	bool		m_recentlyUnlinked;
};


//////////////////////////////////////////////////////////////////////////
class CEntityCreationRecord
{
public:
	CEntityCreationRecord() : m_sReplayID(ReplayIDInvalid), m_sFrameCreation(-1)	{}

	void	Reset()				{ m_sReplayID = ReplayIDInvalid;	m_sFrameCreation = -1; }
	bool	IsEmpty() const		{ return (m_sReplayID == ReplayIDInvalid); }

	const CReplayID&	GetReplayID() const	{ return m_sReplayID; }
	s32		GetFrame() const	{ return m_sFrameCreation; }

	void	Set(s32 sReplayID, s32 sFrameCreated) { m_sReplayID = sReplayID; m_sFrameCreation = sFrameCreated; }

private:
	CReplayID	m_sReplayID;
	s32 m_sFrameCreation;
};


//////////////////////////////////////////////////////////////////////////

#if REPLAY_USE_PER_BLOCK_THUMBNAILS

class CReplayThumbnail
{
public:
	CReplayThumbnail()
	: m_pPixels(NULL)
	, m_Width(0)
	, m_Height(0)
	, m_IsPopulated(0x0)
	, m_IsLocked(false)
	, m_CaptureDelay(REPLAY_PER_FRAME_THUMBNAILS_LIST_SIZE)
	, m_failureFilterIndex(-1)
	{}

	~CReplayThumbnail()
	{
		DeInit();
	}

	void Initialize(u32 width, u32 height)
	{
		m_Width = width;
		m_Height = height;
		m_pPixels = (char*)sysMemManager::GetInstance().GetReplayAllocator()->Allocate(m_Width*m_Height*sizeof(u8)*3, 16);
	}

	bool IsInitialized() const
	{
		return m_Width != 0;
	}

	//////////////////////////////////////////////////////////////////////////

public:
	bool IsPopulated() const;
	void ClearIsPopulated();
	bool IsLocked() const	{	return m_IsLocked;	}
	void SetLocked(bool b)	{	m_IsLocked = b;		}

private:
	void SetIsPopulated();

public:
	static bool QueueForPopulation(CReplayThumbnail *pThumbnail);
	static void UnQueueForPopulation(CReplayThumbnail *pThumbnail);
	static void ProcessQueueForPopulation();
	static void ResetQueue();
	static void ResizeQueue(u32 size);

public:
	static void CreateReplayThumbnail();
	static grcTexture* GetReplayThumbnailTexture(u32 idx);
	static grcRenderTarget* GetReplayThumbnailRT(u32 idx);

	//////////////////////////////////////////////////////////////////////////

public:
	u32 GetWidth() const
	{
		return m_Width;
	}
	u32 GetHeight() const
	{
		return m_Height;
	}
	char *GetRawBuffer() const
	{
		return m_pPixels;
	}
	u32 GetPitch() const
	{
		return m_Width*sizeof(u8)*3;
	}

	static u32 GetThumbnailWidth();
	static u32 GetThumbnailHeight();

	void SetFailureReason(int index)	{	m_failureFilterIndex = index;	}
	void ResetFailureReason()			{	m_failureFilterIndex = -1;		}
	int GetFailureReason() const		{	return m_failureFilterIndex;	}

private:
	void DeInit()
	{
		if(m_pPixels)
			sysMemManager::GetInstance().GetReplayAllocator()->Free((void*)m_pPixels);

		m_pPixels = NULL;
		m_Width = 0;
		m_Height = 0;
		m_IsPopulated = 0x0;
		m_IsLocked = false;
		m_failureFilterIndex = -1;
	}
	char *m_pPixels;
	u32 m_Width;
	u32 m_Height;
	mutable u32 m_IsPopulated;
	bool m_IsLocked;

	u32	m_CaptureDelay;

	int m_failureFilterIndex;

	static u32 ms_CurrentIdx;
	static atArray <CReplayThumbnail *> ms_Queue;
	static sysCriticalSectionToken ms_CritSec;
};


class CReplayThumbnailRef
{
public:
	CReplayThumbnailRef()
	{
		Reset();
	}
	~CReplayThumbnailRef()
	{

	}
	void operator=(const CReplayThumbnailRef &other)
	{
		m_Index = other.m_Index;
	}

	void Reset() { m_Index = -1; }
	bool InitialiseAndQueueForPopulation();
	bool IsInitialAndQueued() const { return m_Index != -1; };
	bool IsPopulated() const;
	const CReplayThumbnail &GetThumbnail() const;
	CReplayThumbnail &GetThumbnail();

private:
	static void SetNoOfBlocksAllocated(u32 maxBlocks);

#if __ASSERT
	static bool IsValidThumbnailPtr(CReplayThumbnail  *pPtr);
#endif // __ASSERT
private:
	int m_Index;
	static int ms_NextIndex;
	static int ms_NoOfThumbnails;
	static CReplayThumbnail *ms_pThumbnails;

	friend class  CBufferInfo;
	friend class  CReplayThumbnail;
};

#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

//////////////////////////////////////////////////////////////////////////
#if GTA_REPLAY_OVERLAY
class CReplayBlockStats
{
public:
	void Reset(eReplayPacketId packetId) 
	{
		m_MemoryUse = 0;
		m_MonitoredMemoryUse = 0;
		m_PacketId = packetId;
		m_NumPackets = 0;
		m_NumMonitoredPackets = 0;
		m_PacketSize = 0;
	}
	void IncrementMemoryUse(u32 memUse, bool monitoredPacket) 
	{ 
		m_MemoryUse += memUse;
		if( monitoredPacket )
		{
			m_MonitoredMemoryUse += memUse;
			m_NumMonitoredPackets++;
		}
		m_NumPackets++;
		m_PacketSize = memUse;
	}

	u32 GetMemoryUse() const { return m_MemoryUse; }
	u32 GetMonitoredMemoryUse() const { return m_MonitoredMemoryUse; }

	eReplayPacketId GetPacketId() const { return m_PacketId;}
	u32 GetPacketSize() { return m_PacketSize;}

	u32 GetNumPackets() { return m_NumPackets;}
	void SetNumPackets(u32 numPackets) { m_NumPackets = numPackets;} 

	u32 GetNumMonitoredPackets() { return m_NumMonitoredPackets;}


private:
	u32 m_MemoryUse;
	u32 m_MonitoredMemoryUse;
	u32 m_NumPackets; //inclusive of monitored
	u32 m_NumMonitoredPackets;
	u32 m_PacketSize;
	eReplayPacketId		m_PacketId;
};
#endif //GTA_REPLAY_OVERLAY

//////////////////////////////////////////////////////////////////////////
const u16 InvalidBlockIndex = 0xffff;
class CBlockInfo
{	friend class CBufferInfo;
public:
	CBlockInfo()
	: m_pData(NULL)
	, m_status(REPLAYBLOCKSTATUS_EMPTY)
	, m_prevStatus(REPLAYBLOCKSTATUS_EMPTY)
	, m_lock(REPLAYBLOCKLOCKED_NONE)
	, m_readCount(0)
	, m_blockSize(0)
	, m_sizeUsed(0)
	, m_sizeLost(0)
	, m_timeSpanMS(0)
	, m_StartTime(0)
	, m_memoryBlockIndex(InvalidBlockIndex)
	, m_sessionBlockIndex(InvalidBlockIndex)
	, m_pNormalBlockEquiv(NULL)
	, m_pTempBlockEquiv(NULL)
	, m_frameCount(0)
	, m_isTempBlock(false)
	, m_isHeadBlock(false)
	, m_isBlockPlaying(false)
	, m_isSaved(false)
	, m_pPrevBlock(NULL)
	, m_pNextBlock(NULL)
#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	, m_genThumbnail(false)
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
	{}

	void				Reset()
	{
		replayAssert(m_lock == REPLAYBLOCKLOCKED_NONE);
		SetStatus(REPLAYBLOCKSTATUS_EMPTY);
		m_prevStatus		= REPLAYBLOCKSTATUS_EMPTY;
		m_lock				= REPLAYBLOCKLOCKED_NONE;
		m_readCount			= 0;
		m_frameCount		= 0;
		m_sizeUsed			= 0;
		m_sizeLost			= 0;
		m_timeSpanMS		= 0;
		m_StartTime			= 0;
		m_sessionBlockIndex	= InvalidBlockIndex;
		m_pNormalBlockEquiv	= NULL;
		m_pTempBlockEquiv	= NULL;
		m_isBlockPlaying	= false;
		m_isSaved			= false;
	#if REPLAY_USE_PER_BLOCK_THUMBNAILS
		m_genThumbnail		= false;
		m_thumbnailRef.Reset();
	#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
#if CLEAR_REPLAY_MEMORY
		memset(m_pData, 0xDE, BYTES_IN_REPLAY_BLOCK);
#endif // CLEAR_REPLAY_MEMORY

#if GTA_REPLAY_OVERLAY
		for( u32 i = 0; i < PACKETID_MAX; i++)
			m_Stats[i].Reset((eReplayPacketId)i);
#endif //GTA_REPLAY_OVERLAY
	}

	void				PrepForRecording(u16 sessionIndex)
	{
		SetStatus(REPLAYBLOCKSTATUS_EMPTY);
		m_prevStatus		= REPLAYBLOCKSTATUS_EMPTY;
		m_lock				= REPLAYBLOCKLOCKED_NONE;
		m_readCount			= 0;
		m_frameCount		= 0;
		m_sizeUsed			= 0;
		m_sizeLost			= 0;
		m_timeSpanMS		= 0;
		m_StartTime			= 0;
		m_sessionBlockIndex	= sessionIndex;
		m_isBlockPlaying	= false;
		m_isSaved			= false;
	#if REPLAY_USE_PER_BLOCK_THUMBNAILS
		m_genThumbnail		= true;
		m_thumbnailRef.Reset();
	#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

#if GTA_REPLAY_OVERLAY
		for( u32 i = 0; i < PACKETID_MAX; i++)
			m_Stats[i].Reset((eReplayPacketId)i);
#endif //GTA_REPLAY_OVERLAY

	}
	void				SetStatus(eReplayBlockStatus status) 	{	replayDebugf3("Setting status of block %u from %s to %s", m_memoryBlockIndex, GetStatusAsString(m_status), GetStatusAsString(status));  m_status = status;								}
	eReplayBlockStatus	GetStatus() const						{	return m_status;								}
	eReplayBlockStatus	GetPrevStatus() const					{	return m_prevStatus;							}

	void				SetData(u8*	pData, u32 blockSize)		{	replayAssert((size_t)pData % 0x10 == 0); m_pData = pData; m_blockSize = blockSize;		}
	u8*					GetData()								{	return m_pData;									}
	const u8*			GetData() const							{	return m_pData;									}

	u32					GetBlockSize() const					{	return m_blockSize;								}
	void				SetSizeUsed(u32 size)					{	m_sizeUsed = size;								}
	u32					GetSizeUsed() const						{	return m_sizeUsed;								}
	void				SetSizeLost(u32 size)					{	m_sizeLost = size;								}
	u32					GetSizeLost() const						{	return m_sizeLost;								}

	void				SetTimeSpan(u32 span)					{	m_timeSpanMS = span;							}
	void				AddTimeSpan(u32 span)					{	m_timeSpanMS += span;							}
	u32					GetTimeSpan() const						{	return m_timeSpanMS;							}

	void				SetStartTime(u32 time)					{	m_StartTime = time;								}
	u32					GetStartTime() const					{	return m_StartTime;								}
	u32					GetEndTime() const						{	return m_StartTime + m_timeSpanMS;				}

	bool				IsLockedForRead() const					{	sysCriticalSection cs(m_csToken); return m_lock == REPLAYBLOCKLOCKED_READ;		}
	bool				IsLockedForWrite() const				{	sysCriticalSection cs(m_csToken); return m_lock == REPLAYBLOCKLOCKED_WRITE;		}
	bool				IsLocked() const						{	return IsLockedForRead() || IsLockedForWrite();	}

	bool				LockForRead()
	{	
		sysCriticalSection cs(m_csToken);

		if(m_lock == REPLAYBLOCKLOCKED_WRITE)
		{
#if !__FINAL
			replayDebugf1("Last write trace:");
			sysStack::PrintRegisteredBacktrace(m_lastWriteTrace);
#endif // !__FINAL
			replayFatalAssertf(false, "Trying to lock for read but lock is already locked for write");
			return false;
		}

		++m_readCount;
		m_lock = REPLAYBLOCKLOCKED_READ;
#if !__FINAL
		m_lastReadTrace = sysStack::RegisterBacktrace();
#endif // !__FINAL
		return true;
	}

	bool				LockForWrite()
	{	
		sysCriticalSection cs(m_csToken);
		replayAssert(m_readCount == 0);
		m_readCount = 0;
		if(m_lock != REPLAYBLOCKLOCKED_NONE)
		{
#if !__FINAL
			replayDebugf1("Last read trace:");
			sysStack::PrintRegisteredBacktrace(m_lastReadTrace);
			replayDebugf1("Last write trace:");
			sysStack::PrintRegisteredBacktrace(m_lastWriteTrace);
#endif // !__FINAL			
			replayAssertf(false, "Trying to lock for write but lock is already %u", (u8)m_lock);
		}

		m_lock = REPLAYBLOCKLOCKED_WRITE;

		NOTFINAL_ONLY(m_lastWriteTrace = sysStack::RegisterBacktrace();)
		return true;
	}
	
	eReplayBlockLock	UnlockBlock()
	{	
		sysCriticalSection cs(m_csToken);
		replayFatalAssertf(m_lock != REPLAYBLOCKLOCKED_NONE, "");
		eReplayBlockLock prevLock = m_lock;

		if(m_lock == REPLAYBLOCKLOCKED_READ)
		{
#if !__FINAL
			if(m_readCount < 1)
			{
				sysStack::PrintStackTrace();
			}
#endif // !__FINAL
			replayAssert(m_readCount > 0);
			--m_readCount;
		}

		if(m_lock == REPLAYBLOCKLOCKED_WRITE || m_readCount == 0)
			m_lock = REPLAYBLOCKLOCKED_NONE;

		return prevLock;
	}

	void				RelockBlock(eReplayBlockLock lock)
	{
		sysCriticalSection cs(m_csToken);
		if(lock == REPLAYBLOCKLOCKED_READ)
			LockForRead();
		else if(lock == REPLAYBLOCKLOCKED_WRITE)
			LockForWrite();
		else
		{	replayFatalAssertf(false, "Attempting to relock with no lock");	}
	}


	void				CopyFrom(const CBlockInfo& other)
	{
		sysCriticalSection cs(m_csToken);
		replayFatalAssertf(m_lock == REPLAYBLOCKLOCKED_WRITE, "This block should be locked for write");
		replayFatalAssertf(other.m_lock == REPLAYBLOCKLOCKED_READ, "Other block should be locked for read");
		sysMemCpy(m_pData, other.m_pData, m_blockSize);
	}

#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	bool				RequestThumbnail();
	bool				HasRequestedThumbNail()	const		{ return m_thumbnailRef.IsInitialAndQueued(); }
	bool				IsThumbnailPopulated() const		{ return m_thumbnailRef.IsPopulated(); }
	void				ResetThumbnailState()				{ return m_thumbnailRef.Reset(); }
	void				GenerateThumbnail()					{ m_genThumbnail = true; }
	void				SetThumbnailRef(const CReplayThumbnailRef& thumbNail){ m_thumbnailRef = thumbNail; }
	CReplayThumbnailRef	&GetThumbnailRef()					{ return m_thumbnailRef; }
	const CReplayThumbnailRef	&GetThumbnailRef() const	{ return m_thumbnailRef; }
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

	void				SetSaving()							{	m_prevStatus = m_status; SetStatus(REPLAYBLOCKSTATUS_SAVING);								}
	void				SetSaved()							{	replayAssert(GetStatus() == REPLAYBLOCKSTATUS_SAVING); BUFFERDEBUG("Set Saved in block %u", m_memoryBlockIndex); m_isSaved = true;	}
	void				UpdateSavedFlag()					{	if(m_isSaved)	{	SetStatus(m_prevStatus); m_isSaved = false;}	}
	bool				HasBeenSaved() const				{	return m_status != REPLAYBLOCKSTATUS_SAVING;												}

	void				SetMemoryBlockIndex(u16 i, bool temp)	{	m_memoryBlockIndex = i;	m_isTempBlock = temp;	}
	u16					GetMemoryBlockIndex() const			{	return m_memoryBlockIndex;							}
	
	void				SetIsTempBlock(bool b)				{	m_isTempBlock = b;								}
	bool				IsTempBlock() const					{	return m_isTempBlock;															}

	void				SetIsHeadBlock(bool b)				{	m_isHeadBlock = b;				}
	bool				IsHeadBlock() const					{	return m_isHeadBlock;			}

	void				SetSessionBlockIndex(u16 i)			{	m_sessionBlockIndex = i;		}
	u16					GetSessionBlockIndex() const		{	return m_sessionBlockIndex;		}

	void				SetFrameCount(u32 f)				{	m_frameCount = f;																			}
	u32					GetFrameCount() const				{	return m_frameCount;																		}

	void				SetNormalBlockEquiv(CBlockInfo* p)	
	{	
		replayAssert(p != this);
		replayAssert(IsTempBlock() || !p);
#if (0 && __ASSERT)
		if( m_isTempBlock == true )
		{
			// Assert that the pointer isn't NULL, as temp blocks must point to a Normal block equivalent.
 			replayFatalAssertf( p != NULL, "CBlockInfo::SetNormalBlockEquiv() - ERROR - Setting a temp blocks normal block equivalent to be NULL!");
			// If we passed that assert, we must have a valid pointer, check that pointer isn't to another temp block
			replayFatalAssertf( p->m_isTempBlock == false, "CBlockInfo::SetNormalBlockEquiv() - ERROR - Setting a temp blocks normal equivalent that is also a temp block!");
		}
#endif	//__ASSERT

		if(m_pNormalBlockEquiv)
			m_pNormalBlockEquiv->m_pTempBlockEquiv = NULL;

		m_pNormalBlockEquiv = p;

		if(m_pNormalBlockEquiv)
			m_pNormalBlockEquiv->m_pTempBlockEquiv = this;
	}

	CBlockInfo*			GetNormalBlockEquiv() const			{	replayFatalAssertf(IsTempBlock(), "");	return m_pNormalBlockEquiv;							}
	CBlockInfo*			GetTempBlockEquiv() const			{	replayFatalAssertf(!IsTempBlock(), "");	return m_pTempBlockEquiv;							}

	void				SetBlockPlaying(bool b)				{	m_isBlockPlaying = b;		}
	bool				IsBlockPlaying() const				{	return m_isBlockPlaying;	}

#if (DEBUG_DRAW || __ASSERT || !__NO_OUTPUT)
	static const char*			GetStatusAsString(eReplayBlockStatus status)
	{
		switch(status)
		{
		case REPLAYBLOCKSTATUS_EMPTY:
			return "Empty";
		case REPLAYBLOCKSTATUS_FULL:
			return "Full";
		case REPLAYBLOCKSTATUS_BEINGFILLED:
			return "Filling";
		case REPLAYBLOCKSTATUS_SAVING:
			return "Saving";
		default:
			return "";
		}
	}

	const char*			GetLockAsString() const
	{
		switch(m_lock)
		{
		case REPLAYBLOCKLOCKED_NONE:
			return "None";
		case REPLAYBLOCKLOCKED_READ:
			return "Read";
		case REPLAYBLOCKLOCKED_WRITE:
			return "Write";
		default:
			return "";
		}
	}
#endif // (DEBUG_DRAW || __ASSERT)

	CBlockInfo*			GetPrevBlock() const					{	return m_pPrevBlock;	}
	CBlockInfo*			GetNextBlock() const					{	return m_pNextBlock;	}

#if GTA_REPLAY_OVERLAY
	void				AddStats(eReplayPacketId packetId, u32 packetSize, bool monitoredPacket){ m_Stats[packetId].IncrementMemoryUse(packetSize, monitoredPacket); }
	const CReplayBlockStats& GetStat(eReplayPacketId packetId) const	{ return m_Stats[packetId]; }
#endif //GTA_REPLAY_OVERLAY

private:

	// Should never be called.
	CBlockInfo(const CBlockInfo&)
	{}
	CBlockInfo& operator=(const CBlockInfo&) {return *this;}

	void				SetPrevBlock(CBlockInfo* pBlock)		{	m_pPrevBlock = pBlock;	}
	void				SetNextBlock(CBlockInfo* pBlock)		{	m_pNextBlock = pBlock;	}

	u8*					m_pData;
	eReplayBlockStatus	m_status;
	eReplayBlockStatus	m_prevStatus;

	mutable sysCriticalSectionToken	m_csToken;
	eReplayBlockLock	m_lock;
	u8					m_readCount;

	u32					m_blockSize;
	u32					m_sizeUsed;
	u32					m_sizeLost;
	u32					m_timeSpanMS;
	u32					m_StartTime;

	u16					m_memoryBlockIndex;
	u16					m_sessionBlockIndex;

	CBlockInfo*			m_pNormalBlockEquiv;
	CBlockInfo*			m_pTempBlockEquiv;

	u32					m_frameCount;

	bool				m_isTempBlock;
	bool				m_isHeadBlock;

	bool				m_isBlockPlaying;
	bool				m_isSaved;

	CBlockInfo*			m_pPrevBlock;
	CBlockInfo*			m_pNextBlock;

#if !__FINAL
	u16					m_lastWriteTrace;
	u16					m_lastReadTrace;
#endif // !__FINAL

#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	bool				m_genThumbnail;
	CReplayThumbnailRef	m_thumbnailRef;
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

#if GTA_REPLAY_OVERLAY
	CReplayBlockStats	m_Stats[PACKETID_MAX];
#endif //GTA_REPLAY_OVERLAY
};


//////////////////////////////////////////////////////////////////////////
class CBufferInfo; // Forward declare

class CBlockProxy
{	friend class CReplayMgrInternal;
	friend class CBufferInfo;
	friend class ReplayController;
public:
	CBlockProxy()
		: m_pBlock(NULL)
	{}
	CBlockProxy(CBlockInfo* block)
		: m_pBlock(block)
	{}

	CBlockProxy		GetNextBlock();
	CBlockProxy		GetPrevBlock();

 	u32				GetStartTime() const			{	return m_pBlock->GetStartTime();	}
 	u32				GetEndTime() const				{	return m_pBlock->GetEndTime();		}
 	u32				GetTimeSpan() const				{	return m_pBlock->GetTimeSpan();		}

	u32				GetSizeUsed() const				{	return m_pBlock->GetSizeUsed();		}

	u16				GetMemoryBlockIndex() const		{	return m_pBlock->GetMemoryBlockIndex();	}	// Remove
	u16				GetSessionBlockIndex() const	{	return m_pBlock->GetSessionBlockIndex();}
 	eReplayBlockStatus	GetStatus() const			{	return m_pBlock->GetStatus();		}
	bool			IsTempBlock() const				{	return m_pBlock->IsTempBlock();		}
	bool			IsHeadBlock() const				{	return m_pBlock->IsHeadBlock();		}

	bool			IsEquivOfTemp() const			{	return m_pBlock->GetTempBlockEquiv() != NULL;	}


	void			SetSaving()						{	m_pBlock->SetSaving();				}
	void			SetSaved()						{	m_pBlock->SetSaved();				}
	void			UpdateSavedFlag()				{	m_pBlock->UpdateSavedFlag();		}

	bool			IsValid() const					{	return m_pBlock != NULL;			}

	//
	u8*				GetData()						{	return m_pBlock->GetData();			}

#if REPLAY_USE_PER_BLOCK_THUMBNAILS
	bool			HasRequestedThumbNail()	const		{	return m_pBlock->HasRequestedThumbNail();	}
	bool			IsThumbnailPopulated() const		{	return m_pBlock->IsThumbnailPopulated();	}
	void			ResetThumbnailState()				{	return m_pBlock->ResetThumbnailState();	}
	CReplayThumbnailRef	&GetThumbnailRef()				{	return m_pBlock->GetThumbnailRef();		}
	const CReplayThumbnailRef	&GetThumbnailRef() const	{	return m_pBlock->GetThumbnailRef();		}
#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS

	void			PopulateBlockHeader(ReplayBlockHeader& blockHeader) const
	{
		blockHeader.m_Status = (u8)m_pBlock->GetPrevStatus();
		blockHeader.m_SessionBlock = m_pBlock->GetSessionBlockIndex();
		blockHeader.m_SizeUsed = m_pBlock->GetSizeUsed();
		blockHeader.m_SizeLost = m_pBlock->GetSizeLost();
		blockHeader.m_StartTime = m_pBlock->GetStartTime();
	}

	void			SetFromBlockHeader(const ReplayBlockHeader& blockHeader)
	{
		m_pBlock->SetStatus((eReplayBlockStatus)blockHeader.m_Status);
		m_pBlock->SetSessionBlockIndex(blockHeader.m_SessionBlock);
		m_pBlock->SetSizeUsed(blockHeader.m_SizeUsed);
		m_pBlock->SetSizeLost(blockHeader.m_SizeLost);
		m_pBlock->SetStartTime(blockHeader.m_StartTime);
	}

	bool			operator==(const CBlockProxy& other) const	{	return m_pBlock == other.m_pBlock;	}
	bool			operator!=(const CBlockProxy& other) const	{	return m_pBlock != other.m_pBlock;	}

	bool			operator==(const CBlockInfo* pBlock) const	{	return m_pBlock == pBlock;			}

	static	void	SetBuffer(CBufferInfo& buffer)	{	pBuffer = &buffer;	}

private:
	CBlockInfo*		GetBlock()						{	return m_pBlock;					}

	CBlockInfo*	m_pBlock;

	static CBufferInfo* pBuffer;
};


//////////////////////////////////////////////////////////////////////////
class CBufferInfo
{	friend class CReplayMgrInternal;
	friend class ReplayController;
#if GTA_REPLAY_OVERLAY
	friend class CReplayOverlay;
#endif //GTA_REPLAY_OVERLAY
public:
	CBufferInfo()
		: m_numBlocks(0)
		, m_numTempBlocks(0)
		, m_pBlocks(NULL)
		, m_pNormalBuffer(NULL)
		, m_pTempBuffer(NULL)
		, m_numBlocksSet(0)
		, m_tempBlocksHaveLooped(false)
		, m_reLinkTempBlocks(false)
		, m_numBlocksAllocated(0)
	{}

	virtual ~CBufferInfo()
	{
		SetNumberOfBlocksAllocated(0);
	}

	void		Reset()
	{
		for(u16 i = 0; i < m_numBlocksAllocated; ++i)
			m_pBlocks[i].Reset();

		if( m_numBlocksSet > 0 )
		{
			m_pBlocks[0].SetSessionBlockIndex(0);
		}
	}

	void		SetAllBlocksNotPlaying()
	{
		for(u16 i = 0; i < m_numBlocksAllocated; ++i)
			m_pBlocks[i].SetBlockPlaying(false);
	}

	void		SetNumberOfBlocksAllocated(u16 maxBlocks)
	{
		m_numBlocksAllocated = maxBlocks;
		if(m_pBlocks)
		{
			sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
			delete[] m_pBlocks;
			m_pBlocks = NULL;
		}

		m_pNormalBuffer = NULL;
		m_pTempBuffer = NULL;

		if(m_numBlocksAllocated > 0)
		{
			sysMemAutoUseAllocator replayAlloc(*MEMMANAGER.GetReplayAllocator());
			m_pBlocks = rage_new CBlockInfo[maxBlocks];
		}

		m_numBlocksSet = 0;

	#if REPLAY_USE_PER_BLOCK_THUMBNAILS
		CReplayThumbnailRef::SetNoOfBlocksAllocated(maxBlocks);
	#endif // REPLAY_USE_PER_BLOCK_THUMBNAILS
	}
	

	void		SetBlockCount(u16 numBlocks, u16 numTempBlocks)
	{
		m_numBlocks		= m_currentNumBlocks		= numBlocks;
		m_numTempBlocks	= m_currentNumTempBlocks	= numTempBlocks;

		if((m_numBlocks + m_numTempBlocks) > 0)
		{
			// Set up the normal buffer
			ReassignNormalBufferHead(nullptr);
			CBlockInfo* pPrev = &m_pBlocks[m_numBlocks-1];
			for(u16 i = 0; i < m_numBlocks; ++i)
			{
				CBlockInfo* pBlock = &m_pBlocks[i];
				pBlock->SetMemoryBlockIndex(i, false);	// Normal blocks
				pBlock->SetPrevBlock(pPrev);

				pPrev->SetNextBlock(pBlock);
				pPrev = pBlock;
			}
			ReassignNormalBufferHead(&m_pBlocks[0]);

			ReassignTempBufferHead(nullptr);
			if(m_numTempBlocks)
			{
				// Set up the temp buffer
				pPrev = &m_pBlocks[m_numBlocks + m_numTempBlocks - 1];
				for (u16 i = 0; i < m_numTempBlocks; ++i)
				{
					CBlockInfo* pBlock = &m_pBlocks[m_numBlocks + i];
					pBlock->SetMemoryBlockIndex(m_numBlocks + i, true);	// temp blocks
					pBlock->SetPrevBlock(pPrev);

					pPrev->SetNextBlock(pBlock);
					pPrev = pBlock;
				}
				ReassignTempBufferHead(&m_pBlocks[m_numBlocks]);
			}
		}

		m_numBlocksSet = numBlocks + numTempBlocks;
		m_tempBlocksHaveLooped = false;
		m_pNormalBuffer->SetSessionBlockIndex(0);
	}



	CBlockInfo*	FindTempBlockFromSessionBlock(u16 sessionIndex) const
	{
		CBlockInfo* const pFirstTempBlock = GetFirstTempBlock();
		CBlockInfo* pBlock = pFirstTempBlock;
		do
		{
			if(pBlock->GetSessionBlockIndex() == sessionIndex)
				return pBlock;

			pBlock = GetNextBlock(pBlock);
		} while(pBlock != pFirstTempBlock);

		return pFirstTempBlock;
	}

	
	CBlockProxy				FindFirstBlock() const;
	CBlockProxy				FindLastBlock() const;
	CBlockProxy				FindFirstTempBlock() const;
	CBlockProxy				FindLastTempBlock() const;

	CBlockInfo*				FindBlockFromSessionIndex(u16 sessionIndex) const
	{
		if(IsAnyTempBlockOn())
		{
			CBlockInfo* pFirstBlock = GetFirstTempBlock();
			CBlockInfo* pBlock = pFirstBlock;
			do 
			{
				if(pBlock->GetSessionBlockIndex() == sessionIndex)
					return pBlock;
				pBlock = GetNextBlock(pBlock);
			} while (pBlock != pFirstBlock);
		}

		CBlockInfo* pFirstBlock = GetFirstBlock();
		CBlockInfo* pBlock = pFirstBlock;
		do 
		{
			if(pBlock->GetSessionBlockIndex() == sessionIndex)
				return pBlock;
			pBlock = GetNextBlock(pBlock);
		} while (pBlock != pFirstBlock);

		return NULL;
	}

	void				SetAllSaved()
	{
		CBlockInfo* pFirstBlock = GetFirstBlock();
		CBlockInfo* pBlock = pFirstBlock;
		do 
		{
			pBlock->SetSaved();
			pBlock = GetNextBlock(pBlock);
		}
		while(pBlock != pFirstBlock);
	}

	int					ForeachBlock(int(func)(CBlockInfo&, void*), void* pParam = NULL, u16 numBlocks = 0)
	{
		u16 count = (numBlocks > 0 && numBlocks < m_numBlocksAllocated) ? numBlocks : m_numBlocksAllocated;
		for(int i = 0; i < count; ++i)
		{
			int result = (*func)(m_pBlocks[i], pParam);
			if(result != 0)
				return result;
		}
		return 0;
	}

	int					ForeachBlockReverse(int(func)(CBlockInfo&, void*), void* pParam = NULL, u16 numBlocks = 0)
	{
		u16 count = (numBlocks > 0 && numBlocks < m_numBlocksAllocated) ? numBlocks : m_numBlocksAllocated;
		for(int i = count-1; i >= 0; --i)
		{
			int result = (*func)(m_pBlocks[i], pParam);
			if(result != 0)
				return result;
		}
		return 0;
	}

	int					ForeachBlock(int(func)(const CBlockInfo&, void*), void* pParam = NULL, u16 numBlocks = 0) const
	{
		u16 count = (numBlocks > 0 && numBlocks < m_numBlocksAllocated) ? numBlocks : m_numBlocksAllocated;
		for(int i = 0; i < count; ++i)
		{
			int result = (*func)(m_pBlocks[i], pParam);
			if(result != 0)
				return result;
		}
		return 0;
	}

	int					ForeachBlock(int(func)(CBlockProxy, void*), void* pParam = NULL, u16 numBlocks = 0)
	{
		u16 count = (numBlocks > 0 && numBlocks < m_numBlocksAllocated) ? numBlocks : m_numBlocksAllocated;
		for(int i = 0; i < count; ++i)
		{
			CBlockProxy proxy(&m_pBlocks[i]);
			int result = (*func)(proxy, pParam);
			if(result != 0)
				return result;
		}
		return 0;
	}
	

	CBlockInfo*			GetPrevBlock(const CBlockInfo* pCurrBlock ASSERT_ONLY(, bool doCheck = true)) const
	{
		if(pCurrBlock == NULL)
			return NULL;
		replayAssertf(!doCheck || (pCurrBlock->IsTempBlock() == pCurrBlock->GetPrevBlock()->IsTempBlock()), "Blocks mismatch in type %u is %s and %u is %s", 
							pCurrBlock->GetMemoryBlockIndex(), pCurrBlock->IsTempBlock() ? "Temp" : "Normal", 
							pCurrBlock->GetPrevBlock()->GetMemoryBlockIndex(), pCurrBlock->GetPrevBlock()->IsTempBlock() ? "Temp" : "Normal");
		return pCurrBlock->GetPrevBlock();
	}

	CBlockInfo*			GetNextBlock(const CBlockInfo* pCurrBlock ASSERT_ONLY(, bool doCheck = true)) const
	{
		if(pCurrBlock == NULL)
			return NULL;
		replayAssertf(!doCheck || (pCurrBlock->IsTempBlock() == pCurrBlock->GetNextBlock()->IsTempBlock()), "Blocks mismatch in type %u is %s and %u is %s", 
			pCurrBlock->GetMemoryBlockIndex(), pCurrBlock->IsTempBlock() ? "Temp" : "Normal", 
			pCurrBlock->GetNextBlock()->GetMemoryBlockIndex(), pCurrBlock->GetNextBlock()->IsTempBlock() ? "Temp" : "Normal");
		return pCurrBlock->GetNextBlock();
	}

	CBlockInfo*			GetFirstTempBlock() const							{	return m_pTempBuffer;		}
	CBlockInfo*			GetLastTempBlock() const							{	return GetPrevBlock(GetFirstTempBlock());	}
	CBlockInfo*			GetFirstBlock() const								{	return m_pNormalBuffer;		}

	bool				IsAnyTempBlockOn() const
	{
		CBlockInfo* pFirstBlock = GetFirstTempBlock();
		if (!pFirstBlock)
			return false;
		CBlockInfo* pBlock = pFirstBlock;
		do
		{
			if(pBlock->GetStatus() == REPLAYBLOCKSTATUS_BEINGFILLED)
				return true;
			pBlock = GetNextBlock(pBlock);
		}
		while(pBlock != pFirstBlock);

		return false;
	}

	bool				IsAnyBlockSaving() const
	{
		CBlockInfo* pFirstBlock = GetFirstBlock();
		CBlockInfo* pBlock = pFirstBlock;
		do
		{
			if(pBlock->GetStatus() == REPLAYBLOCKSTATUS_SAVING)
				return true;
			pBlock = GetNextBlock(pBlock);
		}
		while(pBlock != pFirstBlock);

		return false;
	}

	void				ResetNormalBlocksAfterTempLoop() const
	{
		// When we were saving last the 'temp' buffer looped.  This may have
		// just been the default 6 blocks or it may have been 'extended' as the 
		// save progressed.  Either way that 'temp' buffer now makes up the active
		// portion of the normal buffer...any other block needs to be Reset here.
		// Best way to check is the sequentiality of the session block indexes...
		BUFFERDEBUG("Temp blocks have looped...resetting blocks...");
		CBlockInfo* pLastBlock = FindLastBlock().GetBlock();
		if(!pLastBlock)	return;

		CBlockInfo* pBlock = pLastBlock;
		int blockIndex = pBlock->GetSessionBlockIndex() + 1;
		bool resetBlocks = false;
		do 
		{
			if(blockIndex != pBlock->GetSessionBlockIndex() + 1)
			{
				resetBlocks = true;
			}
			else
			{
				blockIndex = pBlock->GetSessionBlockIndex();
				BUFFERDEBUG(" Keeping %d", pBlock->GetMemoryBlockIndex());
			}

			if(resetBlocks)
			{
				BUFFERDEBUG(" Resetting %d", pBlock->GetMemoryBlockIndex());
				pBlock->Reset();
			}

			pBlock = GetPrevBlock(pBlock);
		} while (pBlock != pLastBlock);


// 		CBlockInfo* pLastToReset = GetFirstBlock();
//  		for(int i = 0; i < GetNumTempBlocks(); ++i)
//  			pLastToReset = GetPrevBlock(pLastToReset);
// 
// 		replayDebugf1("Temp blocks have looped...resetting blocks...");
// 		CBlockInfo* pBlock = GetFirstBlock();
// 		while(pBlock != NULL && pBlock != pLastToReset)
// 		{
// 			replayDebugf1(" %d", pBlock->GetMemoryBlockIndex());
// 			pBlock->Reset();
// 			pBlock = GetNextBlock(pBlock);
// 		}
	}



	void				SwapTempBlocks(CBlockInfo*& /*pCurrBlock*/)
	{
		CBlockInfo* pFirstTempBlock = FindFirstTempBlock().GetBlock();
		CBlockInfo* pFirstEquivBlock = NULL;
		CBlockInfo* pLastTempBlock = GetPrevBlock(pFirstTempBlock);
		CBlockInfo* pLastEquivBlock = NULL;

		BUFFERDEBUG("Entering SwapTempBlocks... first Temp is %u, last temp is %u", pFirstTempBlock->GetMemoryBlockIndex(), pLastTempBlock->GetMemoryBlockIndex());

		// Find all the equivalent normal blocks linked to the temp blocks
		// Max will be the normal size of the temp buffer
		CBlockInfo* pBlock = pFirstTempBlock;
		CBlockInfo** pEquivs = Alloca(CBlockInfo*, m_numTempBlocks);
		int count = 0;
		do 
		{
			if(pBlock->GetNormalBlockEquiv())
			{
				pEquivs[count++] = pBlock->GetNormalBlockEquiv();
				BUFFERDEBUG(" Equiv found... %u for %u", pBlock->GetNormalBlockEquiv()->GetMemoryBlockIndex(), pBlock->GetMemoryBlockIndex());
			}
			pBlock = GetNextBlock(pBlock);
		} while (pBlock != pFirstTempBlock);
		replayFatalAssertf(count <= m_numTempBlocks, "Found more than %u Equiv blocks...(%u)", m_numTempBlocks, count);

		// Go through this list of equivalent blocks and find the 
		// first and last one.  This is determined by finding the ones that
		// have links to blocks which aren't in this list.
		// 
		//	Temp buffer -      |------|
		//	Equiv blocks -	---|------|---
		//				 First ^	  ^ Last
		for(int i = 0; i < count; ++i)
		{
			CBlockInfo* pP = pEquivs[i]->GetPrevBlock();
			CBlockInfo* pN = pEquivs[i]->GetNextBlock();
			bool foundPrev = false;
			bool foundNext = false;
			for(int j = 0; j < count; ++j)
			{
				if(pEquivs[j] == pP)
					foundPrev = true;
				if(pEquivs[j] == pN)
					foundNext = true;
			}

			if(!foundPrev)  // Previous isn't in the list so this must be the first
				pFirstEquivBlock = pEquivs[i];
			if(!foundNext)	// Next isn't in the list so this must be the last
				pLastEquivBlock = pEquivs[i];

			// It's possible these could be equal (1 block)
			if(pFirstEquivBlock != NULL && pLastEquivBlock != NULL)
				break;
		}

		//our temp buffers and normal buffers have swapped
		//so we don't need to re-link them.  Instead just set the normal
		//blocks to temp and vise versa then reassign the head pointers.
		if( !pFirstEquivBlock && !pLastEquivBlock )
		{
			pBlock = m_pTempBuffer;
			do 
			{
				BUFFERDEBUG(" Block %u", pBlock->GetMemoryBlockIndex());
				pBlock->SetIsTempBlock(false);
				pBlock = GetNextBlock(pBlock ASSERT_ONLY(, false));
			} while (pBlock != m_pTempBuffer);

			ReassignNormalBufferHead(FindFirstBlockInt(m_pTempBuffer));

			BUFFERDEBUG("New temp buffer...");
			CBlockInfo* firstEquivBlock = FindFirstBlockInt(pEquivs[0]);
			pBlock = firstEquivBlock;
			do 
			{
				BUFFERDEBUG(" Block %u", pBlock->GetMemoryBlockIndex());
			
				pBlock->Reset();// Clean the block....don't care about it's contents
				pBlock->SetIsTempBlock(true);
				pBlock = GetNextBlock(pBlock ASSERT_ONLY(, false));
			} while (pBlock != firstEquivBlock);

			ReassignTempBufferHead(firstEquivBlock);
			
			return;
		}

		// We could have less than the normal count of equivs (fast saving)
		// so find the next few blocks until we have the amount we need for
		// a NEW TEMP buffer.
		while(count < m_numTempBlocks)
		{
			pLastEquivBlock = GetNextBlock(pLastEquivBlock);
			++count;
		}


#if !__FINAL
		// The block we've changed to may be being appropriated as a NEW TEMP
		// so see if we can find it and then find the equiv old temp block
		// to use instead.
		// (NOTE: Maybe we shouldn't jump to the next block until all this function is done...)
		BUFFERDEBUG("Equiv range %u - %u", pFirstEquivBlock->GetMemoryBlockIndex(), pLastEquivBlock->GetMemoryBlockIndex());
		BUFFERDEBUG("  Prev - %u", pFirstEquivBlock->GetPrevBlock()->GetMemoryBlockIndex());
		pBlock = pFirstEquivBlock;
		do 
		{
			BUFFERDEBUG(" Block %u", pBlock->GetMemoryBlockIndex());
			pBlock = GetNextBlock(pBlock ASSERT_ONLY(, false));
		} while (pBlock != pLastEquivBlock->GetNextBlock());
		BUFFERDEBUG("  Next - %u", pLastEquivBlock->GetNextBlock()->GetMemoryBlockIndex());


		BUFFERDEBUG("Temp range %u - %u", pFirstTempBlock->GetMemoryBlockIndex(), pLastTempBlock->GetMemoryBlockIndex());
		BUFFERDEBUG("  Prev - %u", pFirstTempBlock->GetPrevBlock()->GetMemoryBlockIndex());
		pBlock = pFirstTempBlock;
		do 
		{
			BUFFERDEBUG(" Block %u (equiv %u)", pBlock->GetMemoryBlockIndex(), pBlock->GetNormalBlockEquiv() ? pBlock->GetNormalBlockEquiv()->GetMemoryBlockIndex() : -1);
			pBlock = GetNextBlock(pBlock ASSERT_ONLY(, false));
		} while (pBlock != pFirstTempBlock);
		BUFFERDEBUG("  Next - %u", pLastTempBlock->GetNextBlock()->GetMemoryBlockIndex());
#endif // !__FINAL			
		
		// Now link the old temp buffer into the NORMAL BUFFER in place of
		// the NEW TEMP
		pFirstTempBlock->SetPrevBlock(pFirstEquivBlock->GetPrevBlock());
		pFirstEquivBlock->GetPrevBlock()->SetNextBlock(pFirstTempBlock);

		pLastTempBlock->SetNextBlock(pLastEquivBlock->GetNextBlock());
		pLastEquivBlock->GetNextBlock()->SetPrevBlock(pLastTempBlock);


		// Go through the NEW TEMP buffer and set the blocks up accordingly
		BUFFERDEBUG("New temp buffer...");
		m_currentNumTempBlocks = 0;
		pBlock = pFirstEquivBlock;
		do 
		{
			BUFFERDEBUG(" Block %u", pBlock->GetMemoryBlockIndex());
			if(m_pNormalBuffer == pBlock)
			{	// If one of the blocks to be set in our NEW TEMP buffer
				// is the normal buffer head then move that on a space
				ReassignNormalBufferHead(GetNextBlock(m_pNormalBuffer));
			}
			pBlock->Reset();	// Clean the block....don't care about it's contents
			pBlock->SetIsTempBlock(true);
			++m_currentNumTempBlocks;
			pBlock = GetNextBlock(pBlock ASSERT_ONLY(, false));
		} while (pBlock != GetNextBlock(pLastEquivBlock ASSERT_ONLY(, false)));

		// Circularise the new temp buffer
		pFirstEquivBlock->SetPrevBlock(pLastEquivBlock);
		pLastEquivBlock->SetNextBlock(pFirstEquivBlock);

		// Reassign the temp head to the start of the NEW TEMP buffer
		ReassignTempBufferHead(pFirstEquivBlock);


		// Go through the NEW NORMAL buffer
		BUFFERDEBUG("New normal buffer...");
		pBlock = pFirstTempBlock;
		m_currentNumBlocks = 0;
		do 
		{
			BUFFERDEBUG(" Block %u", pBlock->GetMemoryBlockIndex());
			replayAssert(pBlock->IsLocked() == false);
			pBlock->SetIsTempBlock(false);
			pBlock->SetNormalBlockEquiv(NULL);
			++m_currentNumBlocks;
			pBlock = GetNextBlock(pBlock ASSERT_ONLY(, false));
		} while (pBlock != pFirstTempBlock);

		// Final checks that these head pointers are good
		replayAssert(m_pNormalBuffer->IsTempBlock() == false);
		replayAssert(m_pTempBuffer->IsTempBlock() == true);
		replayAssertf((m_currentNumTempBlocks == m_numTempBlocks) && (m_currentNumBlocks == m_numBlocks), "Incorrect block counts after swapping back from the temp buffer (%u, %u)", m_currentNumBlocks, m_currentNumTempBlocks);
	}


	u16					GetNumNormalBlocks() const									{	return m_numBlocks;					}

	void				ReassignNormalBufferHead(CBlockInfo* pBlock)
	{
		BUFFERDEBUG("ReassignNormalBufferHead %u > %u", m_pNormalBuffer ? m_pNormalBuffer->GetMemoryBlockIndex() : -1, pBlock ? pBlock->GetMemoryBlockIndex() : -1);
		replayAssert(pBlock ? pBlock->IsTempBlock() == false : true);
		if(m_pNormalBuffer)
			m_pNormalBuffer->SetIsHeadBlock(false);
		m_pNormalBuffer = pBlock;
		if(m_pNormalBuffer)
			m_pNormalBuffer->SetIsHeadBlock(true);
	}

	void				ReassignTempBufferHead(CBlockInfo* pBlock)
	{
		BUFFERDEBUG("ReassignTempBufferHead %u > %u", m_pTempBuffer ? m_pTempBuffer->GetMemoryBlockIndex() : -1, pBlock ? pBlock->GetMemoryBlockIndex() : -1);
		replayAssert(pBlock ? pBlock->IsTempBlock() == true : true);
		if(m_pTempBuffer)
			m_pTempBuffer->SetIsHeadBlock(false);
		m_pTempBuffer = pBlock;
		if(m_pTempBuffer)
			m_pTempBuffer->SetIsHeadBlock(true);
	}

	//////////////////////////////////////////////////////////////////////////
	// Find the first available normal block that we can reuse as a temp block
	// This should be the one after the last block in the normal buffer if it
	// is empty.  Else it's the first block in the clip....if it's been saved
	// Otherwise return no block...none available
	CBlockProxy			FindAvailableNormalBlock() const
	{
		CBlockProxy firstBlock = FindFirstBlock();
		CBlockProxy lastBlock = FindLastBlock();

		CBlockProxy block = firstBlock;
		while((block.GetStatus() == REPLAYBLOCKSTATUS_SAVING || block.IsEquivOfTemp()) && block != lastBlock)
		{
			block = block.GetNextBlock();
		}

		if(block.GetStatus() != REPLAYBLOCKSTATUS_SAVING && !block.IsEquivOfTemp())
		{
			replayAssert(block.IsTempBlock() == false);
			BUFFERDEBUG("Available normal block is %u", block.GetMemoryBlockIndex());
			return block;
		}

		BUFFERDEBUG("Failed to find an available normal block...");
		return CBlockProxy();
	}


	//////////////////////////////////////////////////////////////////////////
	// Remove the given block from it's linked list.  This effectively leaves
	// us with a dangling block which doesn't belong to either list.
	void				DecoupleNormalBlock(CBlockProxy block)
	{
		replayAssert(block.IsTempBlock() == false);
		replayAssert(block.GetStatus() != REPLAYBLOCKSTATUS_SAVING);
		BUFFERDEBUG("Decouple Normal Block %u", block.GetMemoryBlockIndex());

		CBlockInfo* pBlock = block.GetBlock();
		CBlockInfo* pPrevBlock = pBlock->GetPrevBlock();
		CBlockInfo* pNextBlock = pBlock->GetNextBlock();

		pPrevBlock->SetNextBlock(pNextBlock);
		pNextBlock->SetPrevBlock(pPrevBlock);
		if(m_pNormalBuffer == pBlock)
		{
			replayAssert(pNextBlock->IsTempBlock() == false);
			BUFFERDEBUG("Reassigning normal head via DecoupleNormalBlock");
			ReassignNormalBufferHead(pNextBlock);
		}

		pBlock->SetNextBlock(NULL);
		pBlock->SetPrevBlock(NULL);
		pBlock->Reset();
	}



	void				ExtendTempBuffer(CBlockProxy block)
	{
		CBlockProxy lastBlock = FindLastTempBlock();
		replayAssert(lastBlock.IsTempBlock());
		replayAssert(lastBlock.GetStatus() == REPLAYBLOCKSTATUS_FULL);

		CBlockInfo* pLastBlock = lastBlock.GetBlock();
		CBlockInfo* pNextBlock = pLastBlock->GetNextBlock();

		CBlockInfo* pExtendBlock = block.GetBlock();
		replayAssert(pExtendBlock->GetPrevBlock() == NULL && pExtendBlock->GetNextBlock() == NULL);
		pExtendBlock->SetPrevBlock(pLastBlock);
		pExtendBlock->SetNextBlock(pNextBlock);

		pLastBlock->SetNextBlock(pExtendBlock);
		pNextBlock->SetPrevBlock(pExtendBlock);

		--m_currentNumBlocks;
		++m_currentNumTempBlocks;
		pExtendBlock->SetIsTempBlock(true);

		BUFFERDEBUG("Extended temp buffer...now %u blocks in size (%u normal blocks, block %u was used)", m_currentNumTempBlocks, m_currentNumBlocks, pExtendBlock->GetMemoryBlockIndex());
	}

private:
	void				RelinkTempBlocks();

	u16					GetNumTempBlocks() const									{	return m_numTempBlocks;				}
	u16					GetNumBlocksSet() const										{	return m_numBlocksSet;				}
	u16					GetNumberOfBlocksAllocated() const							{	return m_numBlocksAllocated;		}

	void				SetTempBlocksHaveLooped(bool b)								{	m_tempBlocksHaveLooped = b;			}
	bool				GetTempBlocksHaveLooped() const								{	return m_tempBlocksHaveLooped;		}

	void				SetReLinkTempBlocks(bool b)									{	m_reLinkTempBlocks = b;				}
	bool				GetReLinkTempBlocks() const									{	return m_reLinkTempBlocks;			}

protected:
	//////////////////////////////////////////////////////////////////////////
	CBlockInfo*				FindFirstBlockInt(CBlockInfo* pBuffer) const
	{
		if( pBuffer == NULL )
			return NULL;

		u32 time = MAX_UINT32;
		CBlockInfo* pFirstBlock = pBuffer;
		CBlockInfo* pReturnBlock = pFirstBlock;
		CBlockInfo* pBlock = pFirstBlock;
		do
		{
			if((pBlock->GetStatus() == REPLAYBLOCKSTATUS_SAVING && pBlock->GetPrevStatus() == REPLAYBLOCKSTATUS_EMPTY)
				|| pBlock->GetStatus() == REPLAYBLOCKSTATUS_EMPTY)
			{
				pBlock = pBlock->GetNextBlock();
				continue;
			}

			if( pBlock->GetStartTime() < time )
			{
				time = pBlock->GetStartTime();
				pReturnBlock = pBlock;
			}

			pBlock = pBlock->GetNextBlock();
		}
		while(pBlock != pFirstBlock);

		return pReturnBlock;
	}

	CBlockInfo*				FindLastBlockInt(CBlockInfo* pBuffer) const
	{		
		if( pBuffer == NULL )
			return NULL;

		u32 time = 0;
		CBlockInfo* pFirstBlock = pBuffer;
		CBlockInfo* pReturnBlock = pFirstBlock;
		CBlockInfo* pBlock = pFirstBlock;
		do
		{
			if((pBlock->GetStatus() == REPLAYBLOCKSTATUS_SAVING && pBlock->GetPrevStatus() == REPLAYBLOCKSTATUS_EMPTY)
				|| pBlock->GetStatus() == REPLAYBLOCKSTATUS_EMPTY)
			{
				pBlock = pBlock->GetNextBlock();
				continue;
			}

			if(pBlock->GetStartTime() > time)
			{
				time = pBlock->GetStartTime();
				pReturnBlock = pBlock;
			}

			pBlock = pBlock->GetNextBlock();
		}
		while(pBlock != pFirstBlock);

		return pReturnBlock;
	}

	u16			m_numBlocks;
	u16			m_numTempBlocks;
	u16			m_currentNumBlocks;
	u16			m_currentNumTempBlocks;
	CBlockInfo*	m_pBlocks;

	CBlockInfo*	m_pNormalBuffer;
	CBlockInfo*	m_pTempBuffer;

	u16			m_numBlocksSet;

	u16			m_numBlocksAllocated;

	bool		m_tempBlocksHaveLooped;
	bool		m_reLinkTempBlocks;
};



//////////////////////////////////////////////////////////////////////////
class CAddressInReplayBuffer
{
public:
	CAddressInReplayBuffer() 
		: m_position(0)
		, m_pBlock(NULL)
		, m_allowPositionChange(true)
		, m_positionLimit(0)
	{}

	CAddressInReplayBuffer(CBlockInfo* pBlock, u32 position)
		: m_position(position)
		, m_pBlock(pBlock)
		, m_allowPositionChange(true)
		, m_positionLimit(0)
	{}

	void	Reset()							
	{	
		m_position				= 0; 
		m_pBlock				= NULL;
		m_allowPositionChange	= true;
		m_positionLimit			= 0;
	}
	bool	IsEmpty() const					{	return (m_pBlock == NULL);						}

	u32		GetPosition() const				{	return m_position;								}
	void	SetPosition(u32 index)				
	{	
		replayFatalAssertf(m_allowPositionChange == true, "");
		m_position = index;	
		if(m_positionLimit > 0) 
			replayFatalAssertf(m_position <= m_positionLimit, "Failure in CAddressInReplayBuffer::SetPosition (pos: %u, limit: %u)", m_position, m_positionLimit);
	}
	void	IncPosition(u32 val)	
	{	
		replayFatalAssertf(m_allowPositionChange == true, "");
		m_position += val;	
		if(m_positionLimit > 0) 
			replayFatalAssertf(m_position <= m_positionLimit, "Failure in CAddressInReplayBuffer::IncPosition (pos: %u, limit: %u)", m_position, m_positionLimit);
	}

	CBlockInfo*	GetBlock() const					{	return m_pBlock;								}
	void		SetBlock(CBlockInfo* pBlock)		
	{	
		m_pBlock = pBlock;	
// 		replayFatalAssertf(m_pBlock != NULL && m_pBlock->IsTempBlock() == false || m_pBlock->GetNormalBlockEquiv() != NULL, 
// 			"Error setting block, mem block idx %u, session block idx %u, status %s",
// 				m_pBlock->GetMemoryBlockIndex(), m_pBlock->GetSessionBlockIndex(), m_pBlock->GetStatusAsString(m_pBlock->GetStatus()));
	}

	void		SetAllowPositionChange(bool b)		{	m_allowPositionChange = b;						}
	template<typename T>
	T*			GetBufferPosition() const		
	{	
		return reinterpret_cast<T*>(&GetBlock()->GetData()[GetPosition()]);
	}

	eReplayPacketId	GetPacketID() const				{	return (eReplayPacketId)*((tPacketID*)&(GetBlock()->GetData()[GetPosition()]));}
	template<typename T>
	void		SetPacket(const T& packet)			{	sysMemCpy(&(GetBlock()->GetData()[GetPosition()]), &packet, sizeof(T));	}

	u16			GetMemoryBlockIndex() const			{	return m_pBlock->GetMemoryBlockIndex();			}
	u16			GetSessionBlockIndex() const		{	return m_pBlock->GetSessionBlockIndex();		}

	void		SetIndexLimit(u32 limit)			{	m_positionLimit = limit;						}
	void		ResetIndexLimit()					{	m_positionLimit = 0;							}
	u32			GetIndexLimit() const				{	return m_positionLimit;							}

	void		PrepareForRecording();

	bool		operator==(const CAddressInReplayBuffer& other) const
	{
		return m_position == other.m_position && m_pBlock == other.m_pBlock;
	}
protected:
	u32			m_position;
	CBlockInfo*	m_pBlock;

	bool		m_allowPositionChange;
	u32			m_positionLimit;
};


//////////////////////////////////////////////////////////////////////////
class CFrameInfo
{
public:
	CFrameInfo() 
		: m_GameTime(0)
		, m_SystemTime(0)
		, m_DeltaTime(0)
		, m_fSystemToGameTimeScale(0.0f)
		, m_pFramePacket(NULL)
	{}

	void	Reset()
	{ 
		m_GameTime = m_SystemTime = m_DeltaTime = 0; 
		m_fSystemToGameTimeScale = 0.0f;
		//m_FrameRef = FrameRef();
		m_pFramePacket = NULL;
		m_replayAddress.Reset();
	}

	u32		GetGameTime() const			{ return m_GameTime; }
	u64		GetGameTime16BitShifted() const	{	return (u64)GetGameTime() << 16;	}
	void	SetGameTime(u32 uTime)		{ m_GameTime = uTime; }

	u32		GetSystemTime() const		{ return m_SystemTime; }
	void	SetSystemTime(u32 uTime)	{ m_SystemTime = uTime; }

	u32		GetGameDelta() const		{ return m_DeltaTime; }
	void	SetGameDelta(u32 uDelta)	{ m_DeltaTime = uDelta; }

	FrameRef GetFrameRef() const;//			{ return m_pCurrentFrame->GetFrameRef()/*m_FrameRef*/; }
	//void	SetFrameRef(FrameRef uFrame)	{ m_FrameRef = uFrame; }

	float	GetSysToGameRatio() const		{ return m_fSystemToGameTimeScale; }
	void	SetSysToGameRatio(float fRatio)	{ m_fSystemToGameTimeScale = fRatio; }

	CAddressInReplayBuffer&	GetAddress() 		   { return m_replayAddress; }
	const CAddressInReplayBuffer& GetHistoryAddress() const	   { return m_historyAddress; }
	CAddressInReplayBuffer	GetModifiableAddress() { CAddressInReplayBuffer address = m_replayAddress; address.SetAllowPositionChange(true); return address; }

	void	SetAddress(const CAddressInReplayBuffer& address);
	void	SetHistoryAddress(const CAddressInReplayBuffer& address);

	bool	operator==(const CFrameInfo& other) const
	{
		return GetGameTime() == other.GetGameTime() && GetGameDelta() == other.GetGameDelta();
	}

	void			SetFramePacket(const CPacketFrame* pFrame);
	const CPacketFrame*	GetFramePacket() const					{	return m_pFramePacket;		}

private:
	u32		m_GameTime;
	u32		m_SystemTime;
	u32		m_DeltaTime;
	float	m_fSystemToGameTimeScale;
	/*FrameRef m_FrameRef;*/

	CAddressInReplayBuffer	m_replayAddress;
	CAddressInReplayBuffer	m_historyAddress;

	const CPacketFrame*		m_pFramePacket;
};

//////////////////////////////////////////////////////////////////////////
// Contains the current state of the replay engine...e.g. playing/paused, 
// forwards/backwards, caching or not.
class CReplayState
{
public:
	explicit CReplayState(u32 state = 0)
		: m_state(0)
		, m_playbackSpeed(1.0f)
		, m_precaching(false)
		{
			SetState(state);
		}

	CReplayState( const CReplayState& other)
		: m_state( 0 )
		, m_playbackSpeed(other.m_playbackSpeed)
		, m_precaching( other.m_precaching )
	{
		SetState( other.m_state );
	}

	CReplayState& operator=( const CReplayState& other)
	{
		m_state = 0;
		SetState( other.m_state );
		m_playbackSpeed = other.m_playbackSpeed;
		m_precaching = other.m_precaching;

		return *this;
	}

	void	SetState(u32 state)
	{
		if((state & REPLAY_STATE_MASK) != 0)
			m_state &= ~REPLAY_STATE_MASK;

		if((state & REPLAY_CURSOR_MASK) != 0)
			m_state &= ~REPLAY_CURSOR_MASK;

		if((state & REPLAY_DIRECTION_MASK) != 0)
			m_state &= ~REPLAY_DIRECTION_MASK;

#if USE_SRLS_IN_REPLAY
		if((state & REPLAY_SRL_MASK) != 0)
			m_state &= ~REPLAY_SRL_MASK;
#endif
		// Set New Replay Playback State
		m_state |= state;
	}
	u32		GetState(u32 mask) const
	{
		return m_state & mask;
	}

	u32		GetState() const
	{
		return m_state;
	}
	bool	IsSet(u32 state) const
	{
		return (m_state & state) != 0;
	}
	void ClearState(u32 state)
	{
		m_state &= ~state;
	}

	void	SetPrecaching(bool b)	{	m_precaching = b;		}
	bool	IsPrecaching() const	{	return m_precaching;	}

	void	SetPlaybackSpeed(float s)	{	m_playbackSpeed = s;	}
	float	GetPlaybackSpeed() const	{	return m_playbackSpeed;	}

private:
	u32		m_state;
	float	m_playbackSpeed;
	bool	m_precaching;
};


//////////////////////////////////////////////////////////////////////////
class CPreparingSound
{
public:
	CPreparingSound() : m_pSound(NULL), m_pWaveSlot(NULL), m_ReplayId(-1), m_ShouldAllowLoad(false)
	{
	}

	CPreparingSound( audSound *pSound, audWaveSlot *pWaveSlot, s32 replayId, bool shouldAllowLoad ) :	
	m_pSound(pSound), m_pWaveSlot(pWaveSlot), m_ReplayId(replayId), m_ShouldAllowLoad(shouldAllowLoad)
	{
	}

	~CPreparingSound() {}

	void Set( audSound* pSound, audWaveSlot* pWaveSlot, s32 replayId, bool allowLoad )
	{
		m_pSound			= pSound;
		m_pWaveSlot			= pWaveSlot;
		m_ReplayId			= replayId;
		m_ShouldAllowLoad	= allowLoad;
	}

	void Clear()
	{
		m_pSound			= NULL;
		m_pWaveSlot			= NULL;
		m_ReplayId			= -1;
		m_ShouldAllowLoad	= false;
	}

	void				SetSound(audSound* pSound)			{ m_pSound			= pSound;	 }
	void				SetWaveSlot(audWaveSlot* pWaveSlot)	{ m_pWaveSlot		= pWaveSlot; }
	void				SetReplayId(s32 replayId)			{ m_ReplayId		= replayId;	 }
	void				SetAllowLoad(bool allowLoad)		{ m_ShouldAllowLoad	= allowLoad; }

	audSound*			GetSound()		const { return m_pSound;			}
	audWaveSlot*		GetWaveSlot()	const { return m_pWaveSlot;			}
	s32					GetReplayId()	const { return m_ReplayId;			}
	bool				GetAllowLoad()	const { return m_ShouldAllowLoad;	}

private:
	audSound*			m_pSound;
	audWaveSlot*		m_pWaveSlot;
	s32					m_ReplayId;
	bool				m_ShouldAllowLoad;
};


//////////////////////////////////////////////////////////////////////////
class CSoundHandle
{
public:
	CSoundHandle() : m_pData( NULL ), m_IsPersistent( false )
	{
	}

	CSoundHandle(audSound* pSound) : m_pSound( pSound ), m_IsPersistent( false )
	{
	}

	CSoundHandle(audSound** ppSound) : m_ppSound( ppSound ), m_IsPersistent( true )
	{
	}

	void Reset()									
	{
		if( m_IsPersistent && m_ppSound && *m_ppSound )
		{
			replayAssertf(false, "CSoundHandle::Reset not working");
			//TODO4FIVE(*m_ppSound)->SetReplayId( -1, NULL );
		}
		else if( !m_IsPersistent && m_pSound )
		{
			replayAssertf(false, "CSoundHandle::Reset not working");
			//TODO4FIVEm_pSound->SetReplayId( -1, NULL );
		}

		m_pData			= NULL;
		m_IsPersistent	= false;
	}

	void	Set( void* pData )						{ m_pData		 = pData;		 m_IsPersistent = false;	}
	void	Set( audSound* pSound )					{ m_pSound		 = pSound;		 m_IsPersistent = false;	}
	void	Set( audSound** ppSound )				{ m_ppSound		 = ppSound;		 m_IsPersistent = true;		}
	void	SetIsPersistent( bool isPersistent )	{ m_IsPersistent = isPersistent; }

	void*		Get()						const	{ return m_pData;			}
	audSound*	GetSoundPtr()				const	{ return m_pSound;			}
	audSound**	GetSoundPtrPtr()			const	{ return m_ppSound;			}
	bool		GetIsPersistent()			const	{ return m_IsPersistent;	}

	bool		IsUsed()					const	{ return m_pData ? true : false; }

private:
	// IMPORTANT: If the member variables ever change, we need to change the corresponding
	//             CReplaySoundHandle class in sound.h
	union
	{
		audSound*	m_pSound;
		audSound**	m_ppSound;
		void*		m_pData;
	};

	bool			m_IsPersistent;
};


class CEventInfoBase
{
public:
	virtual ~CEventInfoBase(){}
};
//////////////////////////////////////////////////////////////////////////
// Used to pass information to a simple replay packet during playback
template<typename ET>
class CEventInfo : public CEventInfoBase
{
public:
	CEventInfo()
		: playbackFlags(0)
	{
		Reset();
	}

	void				Reset()										{	pEntity[0] = pEntity[1] = NULL;	replayTime = 0; preloadOffsetTime = 0; isFirstFrame = false;} 
	void				ResetEntities()								{	pEntity[0] = pEntity[1] = NULL;	}

	void				SetEntity(ET* p, u8 i = 0)					{	pEntity[i] = p;			}
	ET*					GetEntity(u8 i = 0) const					{	return pEntity[i];		}

	void				SetPlaybackFlags(const CReplayState& f)		{	playbackFlags = f;		}
	const CReplayState&	GetPlaybackFlags() const					{	return playbackFlags;	}

	const u32			GetReplayTime() const						{	return replayTime;		}
	void				SetReplayTime(const u32 time)				{	replayTime = time;		}

	const s32			GetPreloadOffsetTime() const				{	return preloadOffsetTime;		}
	void				SetPreloadOffsetTime(const s32 time)		{	preloadOffsetTime = time;		}

	void				SetIsFirstFrame(bool b)						{	isFirstFrame = b;		}
	bool				GetIsFirstFrame() const						{	return isFirstFrame;	}

private:
	ET*					pEntity[2];

	CReplayState		playbackFlags;
	u32					replayTime;
	s32					preloadOffsetTime;
	bool				isFirstFrame;
};

//////////////////////////////////////////////////////////////////////////
// Used to pass information to an interpolated replay packet during playback
template<typename PT, typename ET>
class CInterpEventInfo : public CEventInfo<ET>
{
public:
	CInterpEventInfo()
		: CEventInfo<ET>()
		, interp(0.0f)
		, pNextPacket(NULL)
	{
		Reset();
	}

	template<typename PT2, typename ET2>
	CInterpEventInfo(const CInterpEventInfo<PT2, ET2>& other)
	{
		if(other.GetEntity(0) != NULL)
			CEventInfo<ET>::SetEntity(verify_cast<ET*>(other.GetEntity(0)), 0);
		if(other.GetEntity(1) != NULL)
			CEventInfo<ET>::SetEntity(verify_cast<ET*>(other.GetEntity(1)), 1);
		if(other.GetNextPacket())
			pNextPacket	= verify_cast<const PT*>(other.GetNextPacket());

		CEventInfo<ET>::SetPlaybackFlags(other.GetPlaybackFlags());
		CEventInfo<ET>::SetReplayTime(other.GetReplayTime());
		CEventInfo<ET>::SetPreloadOffsetTime(other.GetPreloadOffsetTime());
		CEventInfo<ET>::SetIsFirstFrame(other.GetIsFirstFrame());

		interp			= other.GetInterp();
	}

	void		Reset()						{	pNextPacket = NULL;	CEventInfo<ET>::Reset();	}

	void		SetInterp(float i)			{	interp = i;				}
	float		GetInterp() const			{	return interp;			}

	void		SetNextPacket(PT const* p)	{	pNextPacket = p;		}
	PT const*	GetNextPacket() const		{	return pNextPacket;		}

	void				SetFrameInfos(const CFrameInfo& first, const CFrameInfo& current, const CFrameInfo& next,  const CFrameInfo& last)	{	m_firstFrame = first; m_currentFrame = current;	m_nextFrame = next; m_lastFrame = last;	}
	const CFrameInfo&	GetFirstFrameInfo() const					{	return m_firstFrame;		}
	const CFrameInfo&	GetCurentFrameInfo() const					{	return m_currentFrame;		}
	const CFrameInfo&	GetLastFrameInfo() const					{	return m_lastFrame;			}

private:
	float		interp;
	PT const*	pNextPacket;

	CFrameInfo			m_firstFrame;
	CFrameInfo			m_currentFrame;
	CFrameInfo			m_nextFrame;
	CFrameInfo			m_lastFrame;
};


class CPacketBase;
struct preloadData
{
	preloadData()
		: pPacket(NULL)
		, preloadOffsetTime(0)
		, systemTime(0)
		{}
	preloadData(CPacketBase const* p)
		: pPacket(p)
		, preloadOffsetTime(0)
		, systemTime(0)
		{}

	CPacketBase const* pPacket;
	s32 preloadOffsetTime;
	u32	systemTime;

	PRELOADDEBUG_ONLY(atString failureReason;)
};

typedef atFixedArray<preloadData, REPLAY_MAX_PRELOADS>	tPreloadRequestArray;
typedef atFixedArray<s32, REPLAY_MAX_PRELOADS>			tPreloadSuccessArray;



class CReplayInterfaceManager;

class replayCamFrame
{
public:
	camFrame		m_Frame;
	RegdConstEnt	m_Target;
	CViewportFader	m_Fader;

	u32				m_DominantCameraNameHash;
	u32				m_DominantCameraClassIDHash;
};


class replayPopulationValues
{
public:
	float m_innerBandRadiusMin;
	float m_innerBandRadiusMax;
	float m_outerBandRadiusMin;
	float m_outerBandRadiusMax;
};

class replayTimeWrapValues
{
public:
	float m_timeWrapUI;
	float m_timeWrapScript;
	float m_timeWrapCamera;
	float m_timeWrapSpecialAbility;
};


class CReplayFrameData
{
public:
	replayCamFrame			m_cameraData;
	replayPopulationValues	m_populationData;
	replayTimeWrapValues    m_timeWarpData;
	u32						m_instancePriority;
};


class CReplayClipData
{
public:
	void		Reset()
	{
		m_clipFlags = 0;
	}
	u32						m_clipFlags;
};

#if !RSG_ORBIS
#pragma warning(pop)
#endif //!RSG_ORBIS

#endif // GTA_REPLAY
#endif // _REPLAYSUPPORTCLASSES_H_
