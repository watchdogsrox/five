//
// filename:	netcrewmetadata.cpp
// description:	
//
#include "NetworkCrewMetadata.h"

// Rage headers
#include "net/http.h"
#include "rline/clan/rlclan.h"
#include "rline/cloud/rlcloud.h"
#include "vector/color32.h"			//for converting the crew color.
#include "fwnet/netcloudfiledownloadhelper.h"
#include "fwnet/netchannel.h"

//Game Headers
#include "Network/NetworkInterface.h"
#include "Network/Cloud/CloudManager.h"

#if __BANK
#include "bank/bank.h"
#endif

using namespace rage;

NETWORK_OPTIMISATIONS();

RAGE_DEFINE_SUBCHANNEL(net, crewmetadata)
#undef __net_channel
#define __net_channel net_crewmetadata

#define GAME_CREW_INFO_CLOUD_PATH "gta5/crewinfo/index.json"
#define GLOBAL_CREW_INFO_CLOUD_PATH "metadata.json"

NetworkCrewMetadata::NetworkCrewMetadata() 
: m_clanId(RL_INVALID_CLAN_ID)
, m_uReturnCount(0)
, m_uTotalCount(0)
, m_bMetaData_received(false)
, m_cloudGameMetadata(GAME_CREW_INFO_CLOUD_PATH)
{
}

NetworkCrewMetadata::~NetworkCrewMetadata()
{
	Clear();
}

void NetworkCrewMetadata::Clear()
{
	Cancel();
	m_clanId = RL_INVALID_CLAN_ID;

	m_uTotalCount = 0;
	m_uReturnCount = 0;
	m_bMetaData_received = false;
	m_metadata_netStatus.Reset();

	m_cloudGameMetadata.Clear();
}

void NetworkCrewMetadata::SetClanId( rlClanId clanID, bool /*depricated*/)
{
	if (clanID == m_clanId)
	{
		gnetDebug1("netCrewMetadata::SetClanId - Ignoring metadata request since data for crew is already here");
		return;
	}

	if(Pending())
	{
		gnetError("netCrewMetadata::SetClanId - Already have an request going");
		return;
	}

	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		gnetError("netCrewMetadata::SetClanId - Invalid local gamer index");
		return;
	}

	//Clear us out!
	Clear();
	
	//Update to the new clan ID
	m_clanId = clanID;

	
	//If the clan we requested is the invalid clan, we're done
	if (m_clanId == RL_INVALID_CLAN_ID)
	{
		gnetWarning("netCrewMetadata::SetClanId - m_clanId == RL_INVALID_CLAN_ID");
		return;
	}

	DoCrewInfoRequest();
}



bool NetworkCrewMetadata::DoCrewInfoRequest()
{
	//Make sure we're not already doing it.
	if (Pending())
	{
		gnetDebug1("Ignoring Cloud Info request because one is already in progress");
		return false;
	}

	//If the clan we requested is the invalid clan, we're done
	if (m_clanId == RL_INVALID_CLAN_ID)
	{
		return false;
	}

	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		gnetError("netCrewMetadata::DoCloudCrewInfoRequest Invalid local gamer index");
		return false;
	}

	//Request the metaData
	m_metadata_netStatus.Reset();
	m_bMetaData_received = false;
	gnetVerify(rlClan::GetMetadataForClan(localGamerIndex
		, 0
		, m_clanId
		, m_metaData
		, MAX_NUM_CLAN_METADATA_ENUMS
		, &m_uReturnCount
		, &m_uTotalCount
		, &m_metadata_netStatus));

	
	m_cloudGameMetadata.SetClanId(m_clanId);

	gnetDebug1("Starting NetworkCrewMetadata::DoCrewInfoRequest()");

	return true;
}

void NetworkCrewMetadata::Update()
{
	if (m_metadata_netStatus.Failed() || m_metadata_netStatus.Canceled())
	{
		m_bMetaData_received = false;
		gnetError("netCrewMetadata Failed or was cancelled");
	}
	else if (m_metadata_netStatus.Succeeded())
	{
		m_bMetaData_received = true;
		m_metadata_netStatus.Reset();

		gnetDebug1("netCrewMetadata received %d metadata items for crew %d", m_uReturnCount, (int)m_clanId);
	}
}


bool NetworkCrewMetadata::RequestRefresh()
{
	//If the clan we requested is the invalid clan, we're done
	if (m_clanId == RL_INVALID_CLAN_ID)
	{
		return false;
	}

	return DoCrewInfoRequest();
}

void NetworkCrewMetadata::Cancel()
{
	if (m_metadata_netStatus.Pending())
	{
		rlClan::Cancel(&m_metadata_netStatus);
	}

	m_cloudGameMetadata.Cancel();
}

#if __BANK
void NetworkCrewMetadata::DebugPrint()
{
	gnetDebug1("================ MetaData (Crew ID %d) ===============", (int)m_clanId);
	if (m_bMetaData_received)
	{
		gnetDebug1("Crew %d metadata: %d of %d ", (int)m_clanId, m_uReturnCount, m_uTotalCount);
		for (unsigned int i = 0; i< m_uReturnCount; ++i)
		{
			gnetDebug1("%d: %s - %s", i, m_metaData[i].m_SetName, m_metaData[i].m_EnumName);
		}
	}
	else
	{
		gnetDebug1("No Crew MetaData Recevied");
	}

	gnetDebug1("\n================ Cloud crew info ===============");
	if (m_cloudGameMetadata.Succeeded())
	{
		const datGrowBuffer& ggb = m_cloudGameMetadata.GetGrowBuffer();
		gnetDebug1("GAME Crew Info has %d btyes of data", ggb.Length());
		if (ggb.Length() > 0)
		{
			gnetDebug1("%s", (const char*)ggb.GetBuffer());
		}
	}
	else
	{
		gnetDebug1("No GAME cloud crew data received");
	}

}
#endif


int NetworkCrewMetadata::GetCountForSetName( const char* setName ) const
{
	int count = 0;

	u32 setNameHash = atStringHash(setName);
	for (unsigned int i = 0; i< m_uReturnCount; ++i)
	{
		if (setNameHash == atStringHash(m_metaData[i].m_SetName))
		{
			count++;
		}
	}

	return count;
}

bool NetworkCrewMetadata::GetIndexValueForSetName( const char* setName, int index, char* out_string, int maxSTR ) const
{
	int count = 0;

	//Find the i-th (index) for the given set name
	u32 setNameHash = atStringHash(setName);
	for (unsigned int i = 0; i< m_uReturnCount; ++i)
	{
		if (setNameHash == atStringHash(m_metaData[i].m_SetName))
		{
			if (count == index)
			{
				safecpy(out_string, m_metaData[i].m_EnumName, maxSTR);
				return true;
			}
			count++;
		}
	}
	return false;
}
static const u32 sCREW_ATTRIBUTE_SETNAME_HASH = ATSTRINGHASH("CrewAttributes", 0x5F7DBBA3);
bool NetworkCrewMetadata::GetCrewHasCrewAttribute( const char* attributeName ) const
{
	const u32 attributeHash = atStringHash(attributeName);
	for (unsigned int i = 0; i< m_uReturnCount; ++i)
	{
		if (sCREW_ATTRIBUTE_SETNAME_HASH == atStringHash(m_metaData[i].m_SetName))
		{
			if (attributeHash == atStringHash(m_metaData[i].m_EnumName))
			{
				return true;
			}
		}
	}

	return false;
}

bool NetworkCrewMetadata::GetCrewInfoValueString( const char* valueName, char* out_value, int maxStr ) const
{
	return m_cloudGameMetadata.GetCrewInfoValueString(valueName, out_value, maxStr);
}

bool NetworkCrewMetadata::GetCrewInfoValueInt( const char* valueName, int& out_value ) const
{
	return m_cloudGameMetadata.GetCrewInfoValueInt(valueName, out_value);
}

bool NetworkCrewMetadata::GetCrewInfoCrewRankTitle( int inRank, char* out_rankString, int maxStr ) const
{
	if (!m_cloudGameMetadata.Succeeded()) 
	{
		return false;
	}

	const datGrowBuffer& gb = m_cloudGameMetadata.GetGrowBuffer();
	if (gb.Length() == 0)
	{
		return false;
	}

	RsonReader rr((const char*)gb.GetBuffer(),gb.Length());

	RsonReader crewRankArray;
	//NEW VERSION
	//{ "titles": [ { "mxrk": 9, // max rank "ttl": "Thug" }, ... ]
	if (rr.GetMember("titles", &crewRankArray))
	{
		//Iterate through the list looking for the
		RsonReader rankItem; 
		int lowestGoodRank = 0x7FFFFFFF;
		int highestRank = 0;
		bool hasFoundHigherRank = false;
		bool success = false;

		for(bool ok = crewRankArray.GetFirstMember(&rankItem); ok; ok = rankItem.GetNextSibling(&rankItem))
		{
			//See what rank it is.
			RsonReader rank;
			int endRank = 0;
			if (rankItem.GetMember("mxrk", &rank) && rank.AsInt(endRank))
			{
				//See if this rank is in the range for this title
				if(inRank <= endRank)
				{
					hasFoundHigherRank = true;
					if(endRank <= lowestGoodRank)
					{
						if(rankItem.GetMember("ttl", &rank))
						{
							success = rank.AsString(out_rankString, maxStr);
							lowestGoodRank = endRank;
						}
					}
				}
				else if(!hasFoundHigherRank)
				{
					if(endRank >= highestRank)
					{
						if(rankItem.GetMember("ttl", &rank))
						{
							success = rank.AsString(out_rankString, maxStr);
							highestRank = endRank;
						}
					}
				}
			}
		}

		return success;
	}
#if __BANK
	//OLD VERSION
	//"CrewRankTitles":[{"Rank":"10","CrewTitle":"Thug"},
	//					{"Rank":"20","CrewTitle":"Hustler"},
	//					...,
	//					{"Rank":"100","CrewTitle":"Kingpin"}]
	else if(rr.GetMember("CrewRankTitles", &crewRankArray))
	{
		gnetError("Crew [ID '%" I64FMT "d'] has old style and unsupported metadata for CrewRankTitles", m_clanId);
	}
#endif

	return false;
}

#if __BANK
void NetworkCrewMetadata::DebugAddWidgets(bkGroup* pBank)
{
	pBank->AddSlider("Received Metadata Count", &m_uReturnCount, 0, MAX_NUM_CLAN_METADATA_ENUMS, 0);
	pBank->AddSlider("Total Metadata Count", &m_uTotalCount, 0, MAX_NUM_CLAN_METADATA_ENUMS, 0);
	for(int i=0; i<MAX_NUM_CLAN_METADATA_ENUMS; ++i)
	{
		char name[16];
		formatf(name,16, "Metadata #%d", i);
		bkGroup* pNewGroup = pBank->AddGroup(name);
		pNewGroup->AddText("Enum Id", &m_metaData[i].m_Id,true);
		pNewGroup->AddText("Enum Name", m_metaData[i].m_EnumName, RL_CLAN_ENUM_MAX_CHARS);
		pNewGroup->AddText("Set Name", m_metaData[i].m_SetName, RL_CLAN_ENUM_MAX_CHARS);
	}
}



#endif //__BANK

//////////////////////////////////////////////////////////////////////////
//	
//	CrewMetaDataCloudFile
//
//////////////////////////////////////////////////////////////////////////
NetworkCrewMetadata::CrewMetaDataCloudFile::CrewMetaDataCloudFile( const char* crewCloudFilePath )
	: CloudListener()
	, m_bDataReceived(false)
	, m_crewCloudFilePath(crewCloudFilePath) //Yes, the const char*, not a copy.
	, m_clanId(RL_INVALID_CLAN_ID)
{
		m_cloudWatcher.Bind(this, &NetworkCrewMetadata::CrewMetaDataCloudFile::OnCloudFileModified);
}

bool NetworkCrewMetadata::CrewMetaDataCloudFile::DoRequest()
{
	//Make sure we're not already doing it.
	if (Pending())
	{
		gnetDebug1("Ignoring Cloud Info request because one is already in progress");
		return false;
	}

	//If the clan we requested is the invalid clan, we're done
	if (m_clanId == RL_INVALID_CLAN_ID)
	{
		return false;
	}

	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		gnetError("netCrewMetadata::DoCloudCrewInfoRequest Invalid local gamer index");
		return false;
	}

	rlCloudMemberId crewCloudId(m_clanId);
	m_cloudReqID = CloudManager::GetInstance().RequestGetCrewFile(localGamerIndex,
		crewCloudId,
		m_crewCloudFilePath, 
		1024, 
        eRequest_CacheAddAndEncrypt,
		RL_CLOUD_ONLINE_SERVICE_SC);

	return m_cloudReqID != -1;
}

void NetworkCrewMetadata::CrewMetaDataCloudFile::OnCloudEvent( const CloudEvent* pEvent )
{
	switch(pEvent->GetType())
	{
	case CloudEvent::EVENT_REQUEST_FINISHED:
		{
			// grab event data
			const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

			// check if we're interested
			if(pEventData->nRequestID != m_cloudReqID)
				return;

			if (pEventData->bDidSucceed)
			{
				gnetDebug1("Received Crew Cloud Info of size %d", pEventData->nDataSize);

				m_bDataReceived = true;
				m_cloudDataGB.Init(NULL, datGrowBuffer::NULL_TERMINATE);
				if (pEventData->nDataSize > 0 
					&& gnetVerifyf(RsonReader::ValidateJson((const char*)pEventData->pData, pEventData->nDataSize), "Failed json validation for '%s'", (const char*)pEventData->pData)
					&& gnetVerify(m_cloudDataGB.Preallocate(pEventData->nDataSize)))
				{
					gnetVerify(m_cloudDataGB.AppendOrFail(pEventData->pData, pEventData->nDataSize));
				}
				m_cloudReqID = -1;
			}
			else //Failure
			{
				// we treat a 404 as 'successful'
				m_bDataReceived = (pEventData->nResultCode == NET_HTTPSTATUS_NOT_FOUND);

#if !__NO_OUTPUT
				if (m_bDataReceived)
				{
					gnetDebug1("Crew Cloud Info request complete with no file");
				}
				else
				{
					gnetDebug1("Crew Cloud Info request failed");
				}
#endif
				m_cloudReqID = -1;
				m_cloudDataGB.Clear();
			}
		}
		break;

	default:
		break;
	}
}

NetworkCrewMetadata::CrewMetaDataCloudFile::~CrewMetaDataCloudFile()
{
	Cancel();
	Clear();
}

void NetworkCrewMetadata::CrewMetaDataCloudFile::Clear()
{
	
	if (m_clanId != RL_INVALID_CLAN_ID)
	{
		rlCloud::UnwatchCrewItem(&m_cloudWatcher);
	}
	
	m_clanId = RL_INVALID_CLAN_ID;
	m_cloudDataGB.Clear();
	m_bDataReceived = false;
	m_cloudReqID = -1;
}

void NetworkCrewMetadata::CrewMetaDataCloudFile::SetClanId( rlClanId clanId )
{
	//Clear us out!
	Clear();

	//Update to the new clan ID
	m_clanId = clanId;

	//If the clan we requested is the invalid clan, we're done
	if (m_clanId == RL_INVALID_CLAN_ID)
	{
		return;
	}
	
	//IF the clanId is valid, just clear and request the data.
	int localGamerIndex = NetworkInterface::GetLocalGamerIndex();
	if (!RL_IS_VALID_LOCAL_GAMER_INDEX(localGamerIndex))
	{
		return;
	}

	rlCloud::WatchCrewItem(localGamerIndex, m_clanId, RL_CLOUD_ONLINE_SERVICE_SC, m_crewCloudFilePath, &m_cloudWatcher);

	DoRequest();
}

void NetworkCrewMetadata::CrewMetaDataCloudFile::Cancel()
{
	if (CloudManager::GetInstance().IsRequestActive(m_cloudReqID))
	{
		netCloudRequestHelper* pRequest = CloudManager::GetInstance().GetRequestByID(m_cloudReqID);
		pRequest->Cancel();
		m_cloudReqID = -1;
	}
}



bool NetworkCrewMetadata::CrewMetaDataCloudFile::GetCrewInfoValueString( const char* valueName, char* out_value, int maxStr ) const
{
	if (!m_bDataReceived || m_cloudDataGB.Length() == 0) 
	{
		return false;
	}

	RsonReader rr((const char*)m_cloudDataGB.GetBuffer(),m_cloudDataGB.Length());

	return rr.ReadString(valueName, out_value, maxStr);
}
bool NetworkCrewMetadata::CrewMetaDataCloudFile::GetCrewInfoValueInt( const char* valueName, int& out_value ) const
{
	if (!m_bDataReceived || m_cloudDataGB.Length() == 0) 
	{
		return false;
	}

	RsonReader rr((const char*)m_cloudDataGB.GetBuffer(),m_cloudDataGB.Length());

	return rr.ReadInt(valueName, out_value);
}

void NetworkCrewMetadata::CrewMetaDataCloudFile::OnCloudFileModified( const char* OUTPUT_ONLY(pathSpec), const char* /*fullPath*/ )
{
	gnetDebug1("Received cloud file changed event for %s", pathSpec);
	if (Pending())
	{
		gnetDebug1("Request is already currently pending for '%s'", pathSpec);
		return;
	}

	DoRequest();
}


