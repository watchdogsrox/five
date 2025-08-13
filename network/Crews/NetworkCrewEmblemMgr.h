//===========================================================================
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.             
//===========================================================================
#ifndef NETWORKCLANEMBLEMMGR_H
#define NETWORKCLANEMBLEMMGR_H

#include "atl/map.h"
#include "atl/slist.h"
#include "data/base.h"
#include "data/growbuffer.h"
#include "file/handle.h"
#include "rline/clan/rlclancommon.h"
#include "net/status.h"
#include "paging/dictionary.h"

#include "scene/DownloadableTextureManager.h"

namespace rage
{
	class grcTexture;
	class bkBank;
};

typedef u32 EmblemId;
typedef int EmblemType;

//===========================================================================

struct EmblemDescriptor
{
public:

	static const unsigned INVALID_EMBLEM_ID = 0;
	static const rlClanEmblemSize DEFAULT_EMBLEM_SIZE = RL_CLAN_EMBLEM_SIZE_128;

	enum 
	{
		EMBLEM_CLAN,
		EMBLEM_TOURNAMENT,
		EMBLEM_NUM_TYPES
	};

	EmblemDescriptor() { Invalidate(); }
	EmblemDescriptor(EmblemType type, EmblemId id, rlClanEmblemSize size = DEFAULT_EMBLEM_SIZE) : m_type(type), m_id(id), m_size((u32)size) 
	{
		gnetAssertf(IsValid(), "Trying to create in invalid emblem descriptor (%d,%d,%d)", type, id, size);
	}

	bool IsValid() const { return m_type >= 0 && m_type < EMBLEM_NUM_TYPES && m_id != INVALID_EMBLEM_ID && m_size < RL_CLAN_EMBLEM_NUM_SIZES; }
	void Invalidate() 
	{
		m_type = -1;
		m_id = INVALID_EMBLEM_ID;
		m_size = RL_CLAN_EMBLEM_NUM_SIZES;
	}

	bool operator==(const EmblemDescriptor& rhs) const
	{
		return m_type == rhs.m_type && m_id == rhs.m_id && m_size == rhs.m_size;
	}

	bool operator!=(const EmblemDescriptor& rhs) const
	{
		return !operator==(rhs);
	}

	const char* GetStr() const
	{
		static char name[64];
		formatf(name, "%d:%d", m_type, m_id);
		return name;
	}

	void Serialise(CSyncDataBase& serialiser)
	{
		static unsigned SIZEOF_TYPE = datBitsNeeded<EMBLEM_NUM_TYPES-1>::COUNT;
		static unsigned SIZEOF_ID = sizeof(EmblemId)<<3;
		static unsigned SIZEOF_SIZE = datBitsNeeded<RL_CLAN_EMBLEM_NUM_SIZES-1>::COUNT;

		SERIALISE_UNSIGNED(serialiser, m_type, SIZEOF_TYPE, "Emblem type");
		SERIALISE_UNSIGNED(serialiser, m_id, SIZEOF_ID, "Emblem id");

		bool bHasNonDefaultSize = m_size != DEFAULT_EMBLEM_SIZE;

		SERIALISE_BOOL(serialiser, bHasNonDefaultSize);

		if (!bHasNonDefaultSize || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_size, SIZEOF_SIZE, "Emblem size");
		}
		else
		{
			m_size = RL_CLAN_EMBLEM_SIZE_128;
		}
	}

	EmblemType m_type;
	EmblemId m_id; 
	u32 m_size;
};

// Entry for deferred texture dictionary reference removal. Manager contains kind of garbage collector
struct PendingDeRef
{
public:
	PendingDeRef(int txdSlot) : m_txdSlot(txdSlot), m_countdown(REMOVAL_NUM_FRAMES_DELAY)	{}
	PendingDeRef() : m_txdSlot(-1) , m_countdown(REMOVAL_NUM_FRAMES_DELAY) {}

	int m_txdSlot;
	int m_countdown;

private:
	enum {  REMOVAL_NUM_FRAMES_DELAY = 5 };
};

// PURPOSE: Specialized hash function functor for rlClanId keys.
namespace rage
{
    template <>	struct atMapHashFn<EmblemDescriptor>
    {
	    unsigned operator ()(EmblemDescriptor desc) const 
	    { 
			u64 shiftedType = (u64)desc.m_type << 63;
			u64 shiftedSize = (u64)desc.m_size << 60;
		    return (atHash64(shiftedType | shiftedSize | desc.m_id)); 
	    }
    };
}

class NetworkCrewEmblemMgr : public datBase
{
public:

	static const unsigned MAX_NUM_EMBLEMS = MAX_NUM_PHYSICAL_PLAYERS + (MAX_NUM_PHYSICAL_PLAYERS>>2); // crew emblems for each player + 25% with possible tournament emblems

	static int GetNumBytesForEmblemSize(rlClanEmblemSize size, bool sizeForStreaming);

	class EmblemData
	{
	public:
		EmblemData() 
		{
			Reset();
		}

		EmblemData(const EmblemDescriptor& emblemDescriptor) 
		{
			Reset();

			m_emblemDescriptor = emblemDescriptor;
		}

		~EmblemData()
		{
		}

		bool StartDownload();
		void Update();
		bool HandleEmblemReceived();
		void Destroy();

		bool Init();

		bool Pending() const { return m_reqState == STATE_RECEIVING; }
		bool Success() const { return m_reqState == STATE_RECVD || m_reqState == STATE_RECVD_NOTHING; }
		bool Failed() const	 { return m_reqState ==  STATE_ERROR; }
		bool IsRetrying() const	 { return !Success() && m_numRetries >0; }

		bool Started() const { return m_reqState != STATE_NONE;}

		int GetEmblemTXDSlot() const;
		const char *GetEmblemStr() const { return m_emblemDescriptor.GetStr(); }
		const char *GetEmblemTextureName(bool bReturnNameEvenIfEmpty = true);
		rlClanEmblemSize GetSize() const { return (rlClanEmblemSize)m_emblemDescriptor.m_size; }

		void AddRef( ASSERT_ONLY(const char* requesterName) );
		void DecRef( ASSERT_ONLY(const char* releaserName) );
		bool ReadyForDelete() const { return GetRefCount() == 0; }
		u32 GetRefCount() const { return m_refCount; }
#if __BANK
		void DebugPrint();
		EmblemId GetEmblemId() const { return m_emblemDescriptor.m_id; }
#endif

	private:
		void Reset()
		{
			m_emblemDescriptor.Invalidate();
			m_cTextureName[0] = '\0';
			m_reqState = STATE_NONE;
			m_refCount = 0;
			m_iNextRetryTime = 0;
			m_numRetries = 0;

			m_requestHandle = CTextureDownloadRequest::INVALID_HANDLE;
			m_TextureDictionarySlot = -1;
		}

		void CreateTextureCacheName();

		bool RegisterEmblemAsTXD();
		void UnregisterEmblemAsTXD();

		enum RequestState { STATE_NONE, STATE_RECEIVING, STATE_RECVD, STATE_RECVD_NOTHING, STATE_FAILED_RETRY_WAIT, STATE_ERROR };
		RequestState m_reqState;
		EmblemDescriptor m_emblemDescriptor;
		char m_cTextureName[128];

		int m_numRetries;
		u32 m_iNextRetryTime;

		u32 m_refCount;

		//New stuff
		CTextureDownloadRequestDesc m_requestDesc;
		TextureDownloadRequestHandle m_requestHandle;
		int m_TextureDictionarySlot;

#if __ASSERT
		atMap<atString, u32> m_refCountOwners;
#endif

	};

	NetworkCrewEmblemMgr();
	virtual ~NetworkCrewEmblemMgr();

	bool RequestEmblem(const EmblemDescriptor& emblemDesc  ASSERT_ONLY(, const char* requesterName) );
	void ReleaseEmblem(const EmblemDescriptor& emblemDesc  ASSERT_ONLY(, const char* releaserName) );

	int GetEmblemTXDSlot(const EmblemDescriptor& emblemDesc);
	const char* GetEmblemName(const EmblemDescriptor& emblemDesc);
	bool GetEmblemName(const EmblemDescriptor& emblemDesc, const char*& pszResult);

	bool RequestPending(const EmblemDescriptor& emblemDesc) const;
	bool RequestSucceeded(const EmblemDescriptor& emblemDesc) const;
	bool RequestFailed(const EmblemDescriptor& emblemDesc) const;
	bool RequestRetrying(const EmblemDescriptor& emblemDesc) const;

	void Init();
	void Shutdown();
	void Update();

	// Adds an emblem to the list of cloud requests that 304'd, so we can force use the cache next time it's requested.
	void AddUnmodifiedEmblem(const EmblemDescriptor& emblemDesc);
	// Checks if an emblem is in the list of cloud requests that 304'd, so we can force use the cache next time it's requested.
	bool IsKnownUnmodifiedEmblem(const EmblemDescriptor& emblemDesc) const;

private:

	bool HandleNewEmblemRequest(const EmblemDescriptor& emblemDesc);
	void DeleteAndRemove(const EmblemDescriptor& emblemDesc, EmblemData* pData);

	bool ContentPrivsAreOk();

	static const unsigned sm_NUMBER_SIMULTANEOUS_EMBLEM_DOWNLOADS = 5;  //Each 128x128 emblems is about 22k

	typedef atMap<EmblemDescriptor, EmblemData*, atMapHashFn<EmblemDescriptor> > EmblemMap;
	EmblemMap m_emblemMap;

	atArray<PendingDeRef> m_txdsPendingToDeRef;

	// Array of emblems with cloud requests that 304'd, so we can force use the cache next time it's requested.
	atArray<EmblemDescriptor> m_unmodifiedEmblemIds;

	bool m_bAllowEmblemRequests;

#if __BANK
public:

	void Bank_InitWidgets(bkBank *bank, const char* name);
	void Bank_Update();
	void Bank_DumpState();
	void Bank_RequestLocalPrimary();

	void Bank_ShowNextEmblemTexture();
	void Bank_RequestCrewId();
	void Bank_UnrequestCrewEmblemCrewId();
	void Bank_DumpWidgetRequestList();
	char m_bankMemPoolStatusStr[64];
	int  m_bankCrewIdRequest;
private:
	char m_bankEmblemPreviewTextureName[128];
	int m_pBankLastEmblemTXDSlotPreview;
	typedef atMap<int,int, atMapHashFn<u32>, atMapEquals<u32> > WidgetEmblemRequestMap ;
	WidgetEmblemRequestMap m_widgetRequests;
#endif

};

#endif // NETWORKCLANEMBLEMMGR_H

