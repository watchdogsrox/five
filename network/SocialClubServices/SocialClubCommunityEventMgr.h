//===========================================================================
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.             
//===========================================================================

#ifndef INC_NETSOCIALCLUBEVENTMGR_H_
#define INC_NETSOCIALCLUBEVENTMGR_H_

#include "atl/array.h"
#include "atl/string.h"

#include "Network/Cloud/CloudManager.h"

namespace rage
{
	class RsonReader;
}

class SocialClubEventMgr : public datBase, public CloudListener
{
public:

	static SocialClubEventMgr& Get();
	
	SocialClubEventMgr() 
        : m_globalCloudReqId(-1)
        , m_bDataReceived(false)
        , m_lastUpdatePosixTime(0)
        , m_bInitialised(false)
        , m_FeaturedEventId(INVALID_EVENT_ID)
        , m_FeaturedJobId(INVALID_EVENT_ID) 
#if RSG_ORBIS
        , m_PlusPromotionEventId(INVALID_EVENT_ID)
#endif
    {

    }

	void Init(const char* gameCode);
	void Update();
	void Shutdown();

	bool IsEventActive(int eventId) const;
	bool IsEventActive(const char *eventType) const;
	bool HasFeaturedEvent() { return m_FeaturedEventId != INVALID_EVENT_ID; }
	int GetFeaturedEventId() { return m_FeaturedEventId; }
	bool HasFeaturedJob() { return m_FeaturedJobId != INVALID_EVENT_ID; }
	int GetFeaturedJobId() { return m_FeaturedJobId; }
    
	int GetActiveEventCode( const char *eventType) const;

#if RSG_ORBIS
    bool HasActivePlayStationPlusPromotion();
    int GetPlayStationPlusPromotionEventId() { return m_PlusPromotionEventId; }
#endif

	int GetNumberActiveEvents(const char *eventType) const;

	bool Pending() const;

	bool GetExtraEventData(int eventId,const char* name, int& value) const;
	bool GetExtraEventData(int eventId,const char* name, float& value) const;
	bool GetExtraEventData(int eventId,const char* name, char* str, int maxStr) const;
	template<int SIZE>
	bool GetExtraEventData(int eventId,const char* name, char (&buf)[SIZE]) const
	{
		return GetExtraEventData(eventId, name, buf, SIZE);
	}
    bool HasEventDisplayNameById(int eventId) const;
    bool GetEventDisplayNameById(int eventId, char* str, int maxStr) const;
	template<int SIZE>
	bool GetEventDisplayNameById(int eventId, char (&buf)[SIZE]) const
	{
		return GetEventDisplayNameById(eventId, buf, SIZE);
	}

	bool GetExtraEventData(const char* name, int& value, const char* eventType) const;
	bool GetExtraEventData(const char* name, float& value, const char* eventType) const;
	bool GetExtraEventData(const char* name, char* str, int maxStr, const char* eventType) const;
	template<int SIZE>
	bool GetExtraEventData(const char* name, char (&buf)[SIZE], const char* eventType) const
	{
		return GetExtraEventData(name, buf, SIZE, eventType);
	}

	bool GetEventDisplayName(char* str, int maxStr, const char* eventType) const;
	template<int SIZE>
	bool GetEventDisplayName(char (&buf)[SIZE], const char* eventType) const
	{
		return GetEventDisplayName(buf, SIZE, eventType);
	}

#if __BANK
	void DumpEvents();
	void AddWidgets(bkBank& bank);
#endif

	bool DoScheduleRequest();
	bool GetDataReceived() const {return m_bDataReceived;}

	// these event hashes come from the tunables file but 
	unsigned GetGlobalEventHash() const;
	unsigned GetMembershipEventHash() const;

protected:
	//PURPOSE:
	//Handler for downloading a cloud file.
	virtual void OnCloudEvent(const CloudEvent* pEvent);

	bool GetExtraDataMember(const char* name, RsonReader& rr, const char* eventType) const;
	bool GetExtraDataMemberById(int eventID, const char* name, RsonReader& rr) const;

	bool ProcessReceivedSchedule(RsonReader& rr /*,rlClanId clanId = RL_INVALID_CLAN*/ );
	void ProcessEventFromSchedule(RsonReader& evevntRR, u64 currentPosixTime);
	int GetIndexOfActiveEvent(const char* eventType) const;

#if __BANK
	bool ProcessLocalFile(const char* fileName);
#endif

	class EventEntry
	{
	public:
		EventEntry() : m_startPosix(0), m_endPosix(0), m_eventId(0) 
		{
		}

		u64 m_startPosix;
		u64 m_endPosix;
		int m_eventId;
		atString m_extraData;
		atString m_displayName;
		
		bool IsEventActive(u64 currentPosixTime) const 
		{
			return currentPosixTime < m_endPosix && currentPosixTime >= m_startPosix;
		}

		bool GetExtraDataMember(const char* name, RsonReader& rr) const;
		bool IsEventType(const char* eventType) const;

#if __BANK
		void DebugPrint() const;
#endif
	};

	char m_gameCode[5];
	atArray<EventEntry> m_EventList;
	u64 m_lastUpdatePosixTime;

	CloudRequestID m_globalCloudReqId;
	bool m_bDataReceived;
	bool m_bInitialised;

	static const int INVALID_EVENT_ID = -1;
	int m_FeaturedEventId;
	int m_FeaturedJobId;

#if RSG_ORBIS
    int m_PlusPromotionEventId;
#endif
};

#endif // !INC_NETSOCIALCLUBEVENTMGR_H_
