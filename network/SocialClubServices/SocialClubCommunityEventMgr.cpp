//===========================================================================
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.             
//===========================================================================
#include "SocialClubCommunityEventMgr.h"

#include "net/http.h"
#include "rline/ros/rlrostitleid.h"

#include "fwnet/netchannel.h"

#include "Network/NetworkInterface.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Cloud/Tunables.h"

using namespace rage;

RAGE_DECLARE_CHANNEL(net);
RAGE_DEFINE_SUBCHANNEL(net, socialclubeventmgr)
#undef __net_channel
#define __net_channel net_socialclubeventmgr

NETWORK_OPTIMISATIONS();

PARAM(noscevent, "Ignore downloading the event data");
PARAM(sceventlocalpath, "Local path to download the schedule from");
PARAM(sceventremotename, "change the name on the cloud to check");

SocialClubEventMgr& SocialClubEventMgr::Get()
{
	typedef atSingleton<SocialClubEventMgr> SocialClubEventMgrSingleton;
	if (!SocialClubEventMgrSingleton::IsInstantiated())
	{
		SocialClubEventMgrSingleton::Instantiate();
	}

	return SocialClubEventMgrSingleton::GetInstance();
}

void SocialClubEventMgr::Init( const char* gameCode )
{
	gnetDebug1("Initializing SocialClubEventMgr for %s", gameCode);
	safecpy(m_gameCode, gameCode);

	// schedule a request when we start
	DoScheduleRequest();

	// flag initialised
	m_bInitialised = true; 
}

void SocialClubEventMgr::Update()
{
	m_lastUpdatePosixTime = rlGetPosixTime();
}

void SocialClubEventMgr::Shutdown()
{
	m_globalCloudReqId = -1;
}

bool SocialClubEventMgr::Pending() const
{
	return m_globalCloudReqId != -1;
}

bool SocialClubEventMgr::DoScheduleRequest()
{
	if (Pending())
	{
		gnetDebug1("Ignoring Cloud Info request because one is already in progress");
		return false;
	}

	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return false;
	}

    // wait until cloud is available
    if(!NetworkInterface::IsCloudAvailable())
    {
        return false;
    }
        
    if(PARAM_noscevent.Get())
	{
		gnetDebug1("Ignoring request of SC event schedule due to -noscevent");
		return false;
	}

#if __BANK
	if (PARAM_sceventlocalpath.Get())
	{
		const char* filePath = NULL;
		PARAM_sceventlocalpath.Get(filePath);
		if (filePath == NULL || strlen(filePath) == 0)
		{
			filePath = "X:/eventschedule.json";
		}
		return ProcessLocalFile(filePath);
	}
#endif

	//Request the global file.
	//Get the language for the local player
	const char* lang = NetworkUtils::GetLanguageCode();
	char globalPath[64];
#if !__FINAL
	const char* defaultBaseName = "eventschedule";
	if(PARAM_sceventremotename.Get(defaultBaseName))
	{
		formatf(globalPath, "sc/events/%s.json", defaultBaseName, lang);
	}
	else
#endif
	{
		formatf(globalPath, "sc/events/eventschedule-game-%s.json", lang);
	}

	gnetDebug1("Requesting GLOBAL event schedule at %s", globalPath);
	m_globalCloudReqId = CloudManager::GetInstance().RequestGetGlobalFile(globalPath, 1024, eRequest_AlwaysQueue | eRequest_CacheAddAndEncrypt);

	return m_globalCloudReqId != INVALID_CLOUD_REQUEST_ID;
}

unsigned SocialClubEventMgr::GetGlobalEventHash() const
{
	if(!Tunables::IsInstantiated())
		return 0;

	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("EVENT_WKLY", 0xc49477f5), 0);
}

unsigned SocialClubEventMgr::GetMembershipEventHash() const
{
#if NET_ENABLE_MEMBERSHIP_FUNCTIONALITY
	if(!Tunables::IsInstantiated())
		return 0;

	return Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("EVENT_MBSP", 0xcab7b4c9), 0);
#else
	return 0;
#endif
}

int SocialClubEventMgr::GetIndexOfActiveEvent(const char* eventType) const
{
	if( m_EventList.GetCount() == 0 )
		return -1;

	//Zip through the list comparing posix times
	for (int i = 0; i < m_EventList.GetCount(); i++)
	{
		if(m_EventList[i].IsEventActive(m_lastUpdatePosixTime) && m_EventList[i].IsEventType(eventType))
		{
			return i;
		}
	}
	
	return -1;
}

bool SocialClubEventMgr::IsEventActive(const char* eventType) const
{
	int activeIndex = GetIndexOfActiveEvent(eventType);
	return activeIndex >= 0;
}

bool SocialClubEventMgr::IsEventActive( int eventId ) const
{
	for (int i = 0; i < m_EventList.GetCount(); ++i)
	{
		if(m_EventList[i].m_eventId == eventId)
		{
			return m_EventList[i].IsEventActive(m_lastUpdatePosixTime);
		}
	}

	return false;
}

#if RSG_ORBIS
bool SocialClubEventMgr::HasActivePlayStationPlusPromotion() 
{
    if(m_PlusPromotionEventId == INVALID_EVENT_ID)
        return false;
        
    return IsEventActive(m_PlusPromotionEventId);
}
#endif

int SocialClubEventMgr::GetNumberActiveEvents( const char *eventType ) const
{
	if( m_EventList.GetCount() == 0 )
		return 0;

	int activeEventCount = 0;

	//Zip through the list comparing posix times
	for (int i = 0; i < m_EventList.GetCount(); i++)
	{
		if(m_EventList[i].IsEventActive(m_lastUpdatePosixTime) && m_EventList[i].IsEventType(eventType))
		{
			activeEventCount++;
		}
	}

	return activeEventCount;
}

int SocialClubEventMgr::GetActiveEventCode(const char* eventType) const
{
	int activeIndex = GetIndexOfActiveEvent(eventType);
	if (activeIndex >= 0)
	{
		return m_EventList[activeIndex].m_eventId;
	}

	return 0;
}

void SocialClubEventMgr::OnCloudEvent( const CloudEvent* pEvent )
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_globalCloudReqId)
				return;

			if (pEventData->bDidSucceed 
				&& gnetVerifyf(RsonReader::ValidateJson((const char*)pEventData->pData, pEventData->nDataSize), "Failed json validation for '%s'", (const char*)pEventData->pData))
			{
				gnetDebug2("Received Event schedule of size %d", pEventData->nDataSize);
				
				m_bDataReceived = true;
				RsonReader rr((const char*)pEventData->pData, pEventData->nDataSize);
				ProcessReceivedSchedule(rr);
				m_globalCloudReqId = -1;
			}
			else //Failure
			{
				// we treat a 404 as 'successful'
				m_bDataReceived = (pEventData->nResultCode == NET_HTTPSTATUS_NOT_FOUND);

#if !__NO_OUTPUT
				if (m_bDataReceived)
				{
					gnetDebug2("Event Schedule request complete with no file");
				}
				else
				{
					gnetDebug2("Event Schedule request failed");
				}
#endif
				m_globalCloudReqId = -1;
			}
		}
        break; 

	case CloudEvent::EVENT_AVAILABILITY_CHANGED:
		
		// grab event data
		if(m_bInitialised)
		{
			const CloudEvent::sAvailabilityChangedEvent* pEventData = pEvent->GetAvailabilityChangedData();

			// if we now have cloud and don't have the cloud file, request it
			if(pEventData->bIsAvailable && !m_bDataReceived)
			{
				gnetDebug2("Availability Changed - Requesting Cloud File");
				DoScheduleRequest();
			}
		}
		break;
	}
}

#if __BANK
bool SocialClubEventMgr::ProcessLocalFile( const char* fileName )
{
	gnetDebug1("loading local event file %s", fileName);

	fiStream* pStream = ASSET.Open(fileName, "");

	if(!pStream)
	{
		gnetError("Failed to open '%s'",fileName);
		return false;
	}

	// allocate data to read the file
	unsigned nDataSize = pStream->Size();
	char* pData = rage_new char[nDataSize+1];
	pData[nDataSize] = '\0';

	// read out the contents
	pStream->Read(pData, nDataSize);

	m_bDataReceived = true;
	m_globalCloudReqId = -1;
	RsonReader rr((const char*)pData, nDataSize);
	bool bSuccess = ProcessReceivedSchedule(rr);

	// punt data
	delete[] pData; 

	// close file
	pStream->Close();

	return bSuccess;
}
#endif

void SocialClubEventMgr::ProcessEventFromSchedule(RsonReader& eventRR, u64 currentPosixTime)
{
	int eventId = 0;
	if (!gnetVerifyf(eventRR.ReadInt("eventId", eventId), "'eventId' member not found for SC event"))
	{
		return;
	}

	//check to see if our title is in the list
	char titleName[16];
	RsonReader titlesArray;
	if(!eventRR.GetMember("eventRosGameTypes", &titlesArray))
	{
		gnetError("Non title found in event entry ID '%d'", eventId);
		return;
	}

	RsonReader iter;
	bool success = titlesArray.GetFirstMember(&iter);
	bool titleFound = false;
	while (success && !titleFound)
	{
		success = iter.AsString(titleName);
		if (success)
		{
			titleFound = (stricmp(m_gameCode, titleName) == 0);
			success = iter.GetNextSibling(&iter);
		}
	}

	if (!titleFound)
	{
		gnetDebug3("Ignoring event ID '%d' because not for our title", eventId);
		return;
	}

	//If this is defined for specific platforms, see if ours in in the list
	RsonReader platTypesArray;
	char platString[16];
	if (eventRR.GetMember("eventPlatformTypes", &platTypesArray))
	{
		const char* ourPlat = rlRosTitleId::GetPlatformName();

		bool platformFound = false;
		success = platTypesArray.GetFirstMember(&iter);
		while (success && !platformFound)
		{
			success = iter.AsString(platString);
			if (success)
			{
				//Check to see if this platform matches.
				platformFound = stricmp(platString, ourPlat) == 0;
				success = iter.GetNextSibling(&iter);
			}
		}

		//If we didn't find our platform int he list, bail.
		if (!platformFound)
		{
			gnetDebug3("skipping event id '%d' since our platform isn't found", eventId);
			return;
		}
	}
	else
	{
		gnetDebug3("No 'eventPlatformTypes' member found on 'eventId' %d", eventId);
	}

	//Compare the start and end times to make sure it's valid
	u64 startPosix = 0, endPosix = 0;
	eventRR.ReadUns64("posixStartTime", startPosix);
	eventRR.ReadUns64("posixEndTime", endPosix);

	//Make sure we got one time
	if (!gnetVerifyf(startPosix > 0 || endPosix > 0, "No start or end time set for event '%d'", eventId))
	{
		return;
	}

	//Make sure it didn't end in the past first
	if (currentPosixTime > endPosix)
	{
		gnetDebug1("Ignoring event '%d' because it's in the past", eventId);
		return;
	}

	//Create an object for it and add it to the list.
	EventEntry& newEntry = m_EventList.Grow(8);
	newEntry.m_startPosix = startPosix;
	newEntry.m_endPosix = endPosix;
	newEntry.m_eventId = eventId;

	// 	if the event had an extraData member, grab and store it whole hog.
	if (eventRR.HasMember("extraData"))
	{
		RsonReader extraData;
		int strLen = 0;
		const char* memberData = eventRR.GetRawMemberValue("extraData", strLen);
		if(memberData != NULL && strLen > 0)
		{
			newEntry.m_extraData.Set(memberData, strLen, 0);
		}
	}

	//Update the display Name
	if (eventRR.HasMember("displayName"))
	{
		RsonReader displayName;
		int strLen = 0;
		const char* displayNameStr = eventRR.GetRawMemberValue("displayName", strLen);
		if(displayNameStr != NULL && strLen > 0)
		{
			newEntry.m_displayName.Set(displayNameStr, strLen, 0);
		}
	}
	else
	{
		gnetDebug3("Event %d has no 'displayName' attribute", eventId);
	}

	bool isFeaturedEvent = false; 
	if(eventRR.ReadBool("featuredEvent", isFeaturedEvent) && isFeaturedEvent)
	{
		gnetDebug1("Event %d is a featured event", eventId);
		m_FeaturedEventId = eventId;
	}

	bool isFeaturedJob = false; 
	if(eventRR.ReadBool("featuredJob", isFeaturedJob) && isFeaturedJob)
	{
        gnetDebug1("Event %d is a featured Job", eventId);
		m_FeaturedJobId = eventId;
	}

	gnetDebug1("Adding event id '%d'", eventId);
}

bool SocialClubEventMgr::ProcessReceivedSchedule( RsonReader& in_rr/*,rlClanId clanId = RL_INVALID_CLAN*/ )
{
	//Reset featured event / Job id
	m_FeaturedEventId = INVALID_EVENT_ID;
	m_FeaturedJobId = INVALID_EVENT_ID;

#if RSG_ORBIS
    m_PlusPromotionEventId = INVALID_EVENT_ID;
#endif

	//Iterate over each items in the schedule and add an event if the time is correct
	u64 currentPosix = rlGetPosixTime();

	RsonReader eventList;
	if(gnetVerifyf(in_rr.GetMember("multiplayerEvents", &eventList), "Event listing is missing 'multiplayerEvents' object"))
	{
		RsonReader iter;
		bool success = eventList.GetFirstMember(&iter);
		//Each one will presumable be an object
		while (success)
		{
			ProcessEventFromSchedule(iter, currentPosix);

			success = iter.GetNextSibling(&iter);
		}
	}

	if(m_FeaturedEventId == INVALID_EVENT_ID)
	{
		int eventIdx = GetIndexOfActiveEvent("FeaturedPlaylist");
		if(eventIdx >= 0)
		{
			m_FeaturedEventId = m_EventList[eventIdx].m_eventId;
			gnetDebug3("Event %d (Id: %d) is a featured playlist - setting as featured event", eventIdx, m_EventList[eventIdx].m_eventId);
		}
	}

    if(m_FeaturedJobId == INVALID_EVENT_ID)
    {
        int eventIdx = GetIndexOfActiveEvent("FeaturedJob");
        if(eventIdx >= 0)
        {
            m_FeaturedJobId = m_EventList[eventIdx].m_eventId;
            gnetDebug3("Event %d (Id: %d) is a featured job - setting as featured job", eventIdx, m_EventList[eventIdx].m_eventId);
        }
    }

#if RSG_ORBIS
    if(m_PlusPromotionEventId == INVALID_EVENT_ID)
    {
        int eventIdx = GetIndexOfActiveEvent("PlusPromotion");
        if(eventIdx >= 0)
        {
            m_PlusPromotionEventId = m_EventList[eventIdx].m_eventId;
            gnetDebug3("Event %d (Id: %d) is a PS+ promotion", eventIdx, m_EventList[eventIdx].m_eventId);
        }
    }
#endif

#if __BANK
	DumpEvents();
#endif

	return true;
}

bool SocialClubEventMgr::EventEntry::GetExtraDataMember( const char* name, RsonReader& rr ) const
{
	if(m_extraData.GetLength() > 0)
	{
		RsonReader extraRR(m_extraData.c_str(), m_extraData.GetLength());
		return extraRR.GetMember(name, &rr);
	}

	return false;
}

bool SocialClubEventMgr::EventEntry::IsEventType(const char* eventType) const
{
	if(eventType)
	{
		RsonReader eventTypeMember;
		char eventTypeValue[64];

		return GetExtraDataMember("eventType", eventTypeMember) && eventTypeMember.AsString(eventTypeValue) && stricmp(eventTypeValue,eventType)==0;
	}

	// If no type provided, be conservative
	return true;
}

#if __BANK
void SocialClubEventMgr::EventEntry::DebugPrint() const
{
	u64 currentPosix = rlGetPosixTime();

	gnetDebug1("EventID : %d (%s)", m_eventId, IsEventActive(currentPosix) ? "ACTIVE" : "NOT ACTIVE");
	gnetDebug1("Time - [%" I64FMT "d:%" I64FMT "d]", m_startPosix, m_endPosix);
	gnetDebug1("Display Name: %s", m_displayName.GetLength() > 0 ? m_displayName.c_str() : "N/A");
	if (m_extraData.GetLength() > 0)
	{
		gnetDebug1("Extra Data:");
		RsonReader extraData(m_extraData.c_str(), m_extraData.GetLength());
		RsonReader iter;
		bool success = extraData.GetFirstMember(&iter);
		while (success)
		{
			char nameStr[64];
			if(iter.GetName(nameStr))
			{
				atString valueStr;
				bool hasValue = iter.AsAtString(valueStr);
				gnetDebug1("    %s : %s", nameStr, hasValue ? valueStr.c_str() : "UNKNOWN");
			}
			success = iter.GetNextSibling(&iter);
		}
	}
}

void SocialClubEventMgr::DumpEvents()
{
	if (m_EventList.GetCount() == 0)
	{
		gnetDebug1("No Social Club events");
		return;
	}

	gnetDebug1("--------------Social Club Events-----------------");
	gnetDebug1("Current Posix Time: %d\n", (u32)rlGetPosixTime());
	for (int i = 0; i < m_EventList.GetCount(); i++)
	{
		m_EventList[i].DebugPrint();
		gnetDebug1("\n");
	}
}

void SocialClubEventMgr::AddWidgets( bkBank& bank )
{
	bank.PushGroup("Social Club Event Mgr");
		bank.AddButton("Dump Event Info", datCallback(MFA(SocialClubEventMgr::DumpEvents), (datBase*)this));
	bank.PopGroup();
}

#endif

bool SocialClubEventMgr::GetExtraDataMember( const char* name, RsonReader& rr, const char* eventType ) const
{
	//Get the active event
	const int activeIndex = GetIndexOfActiveEvent(eventType);
	return activeIndex >= 0 && m_EventList[activeIndex].GetExtraDataMember(name, rr);
}

bool SocialClubEventMgr::GetExtraEventData( const char* name, int& value, const char* eventType ) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMember(name, extraDataMember, eventType))
	{
		return extraDataMember.AsInt(value);
	}

	return false;
}

bool SocialClubEventMgr::GetExtraEventData( const char* name, float& value, const char* eventType ) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMember(name, extraDataMember, eventType))
	{
		return extraDataMember.AsFloat(value);
	}

	return false;
}

bool SocialClubEventMgr::GetExtraEventData( const char* name, char* str, int maxStr, const char* eventType ) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMember(name, extraDataMember, eventType))
	{
		return extraDataMember.AsString(str,maxStr);
	}

	return false;
}


bool SocialClubEventMgr::GetExtraDataMemberById( int eventID, const char* name, RsonReader& rr ) const
{
	for (int i = 0; i < m_EventList.GetCount(); ++i)
	{
		if(m_EventList[i].m_eventId == eventID)
		{
			if(m_EventList[i].IsEventActive(m_lastUpdatePosixTime))
			{
				return m_EventList[i].GetExtraDataMember(name, rr);
			}
		}
	}

	return false;
}


bool SocialClubEventMgr::GetExtraEventData( int eventId,const char* name, int& value ) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMemberById(eventId, name, extraDataMember))
	{
		return extraDataMember.AsInt(value);
	}

	return false;
}

bool SocialClubEventMgr::GetExtraEventData( int eventId,const char* name, float& value ) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMemberById(eventId, name, extraDataMember))
	{
		return extraDataMember.AsFloat(value);
	}

	return false;
}

bool SocialClubEventMgr::GetExtraEventData( int eventId,const char* name, char* str, int maxStr ) const
{
	RsonReader extraDataMember;
	if (GetExtraDataMemberById(eventId, name, extraDataMember))
	{
		return extraDataMember.AsString(str, maxStr);
	}

	return false;
}

bool SocialClubEventMgr::GetEventDisplayName( char* str, int maxStr, const char* eventType ) const
{
	//Get the active event
	const int activeIndex = GetIndexOfActiveEvent(eventType);
	if (activeIndex >= 0)
	{
		const atString& rDisplayName = m_EventList[activeIndex].m_displayName;
		if(rDisplayName.GetLength() > 0)
		{
			safecpy(str, rDisplayName.c_str(), maxStr);
			return true;
		}
	}

	return false;
}

bool SocialClubEventMgr::HasEventDisplayNameById(int eventId) const
{
    for(int i = 0; i < m_EventList.GetCount(); ++i)
    {
        if(m_EventList[i].m_eventId == eventId)
        {
            if(m_EventList[i].IsEventActive(m_lastUpdatePosixTime))
            {
                const atString& rDisplayName = m_EventList[i].m_displayName;
                if(rDisplayName.GetLength() > 0)
                {
                    return true;
                }
            }
        }
    }

    return false;
}

bool SocialClubEventMgr::GetEventDisplayNameById( int eventId, char* str, int maxStr ) const
{
	for (int i = 0; i < m_EventList.GetCount(); ++i)
	{
		if(m_EventList[i].m_eventId == eventId)
		{
			if(m_EventList[i].IsEventActive(m_lastUpdatePosixTime))
			{
				const atString& rDisplayName = m_EventList[i].m_displayName;
				if(rDisplayName.GetLength() > 0)
				{
					safecpy(str, rDisplayName.c_str(), maxStr);
					return true;
				}
			}
		}
	}

	return false;
}





