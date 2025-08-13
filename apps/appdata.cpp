//
// apps/appdata.cpp
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

SCRIPT_OPTIMISATIONS()

#include "apps/appdata.h"

#include "bank/bkmgr.h"
#include "data/growbuffer.h"
#include "data/rson.h"
#include "rline/rl.h"
#include "rline/ros/rlros.h"
#include "rline/cloud/rlcloud.h"

#include "modelinfo/vehiclemodelinfo.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "renderer/Entities/VehicleDrawHandler.h"
#include "scene/world/GameWorld.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "vehicles/vehicle.h"
#include "script/script.h"
#include "script/script_channel.h"

#include <stdio.h>
#include <string.h>
#include <ctype.h>

#include "file/asset.h"

#include "data/aes_init.h"
AES_INIT_2;

PARAM(AppsDir, "Apps Debug Directory");

static rlCloudWatcher appdataCarWatcher(&appdataOnCarUpate);
static rlCloudWatcher appdataDogWatcher(&appdataOnDogUpate);

////////////////////////////////////////////////////////////////////////////////////////////////////
void DeleteCarAppData()
{
	CAppDataMgr::DeleteCloudData("car", "app");
}

void DeleteCarGameData()
{
	CAppDataMgr::DeleteCloudData("car", "game");
}

void WriteCarAppDataToFile()
{
	CAppDataMgr::WriteCloudDataToFile("car");
}

void WriteDogAppDataToFile()
{
	CAppDataMgr::WriteCloudDataToFile("dog");
}

////////////////////////////////////////////////////////////////////////////////////////////////////
CAppDataRequest::CAppDataRequest(CAppData *appdata) : 
	m_AppData(appdata),
	requestID(-1),
	requestStatus(APP_FILE_STATUS_PENDING)
{
	CloudManager::GetInstance().AddListener(this);
}

CAppDataRequest::~CAppDataRequest()
{
	CloudManager::GetInstance().RemoveListener(this);
	requestID = -1;
	m_AppData = NULL;
}

bool CAppDataRequest::RequestFile(const char * path, int localGamerIndex)
{
	Assert(rlCloud::IsInitialized());
	Assert(path);
	const rlRosCredentials & cred = rlRos::GetCredentials(localGamerIndex);
	if( cred.IsValid() && cred.GetRockstarId() != 0 )
	{
		rlCloudMemberId member(cred.GetRockstarId());

		requestID = CloudManager::GetInstance().RequestGetMemberFile(localGamerIndex, member, path, APP_MAX_DATA_LENGTH, eRequest_CacheAdd, RL_CLOUD_ONLINE_SERVICE_SC);

		requestType = APP_REQUEST_GET;

		return requestID != -1;
	}

	return false;
}

bool CAppDataRequest::PushFile( const char * path = NULL, int localGamerIndex = -1,  CAppData* appData = NULL)
{	
	const rlRosCredentials & cred = rlRos::GetCredentials(localGamerIndex);
	if( cred.IsValid() && cred.GetRockstarId() != 0 && appData )
	{
		unsigned int usDataSize;
		bool result = appData->GetFormattedData(m_Data, usDataSize, localGamerIndex);

		if( result )
		{
			requestID = CloudManager::GetInstance().RequestPostMemberFile(localGamerIndex, path, m_Data, usDataSize, eRequest_CacheOnPost, RL_CLOUD_ONLINE_SERVICE_SC);

			requestType = APP_REQUEST_POST;
			
			//we've just updated the cloud, set the modifed flag to false.
		}
		else
		{
			scriptAssertf(false, "APP_MAX_DATA_LENGTH (%d) is too small to send this much data to the cloud.", APP_MAX_DATA_LENGTH);
		}

		appData->SetModified(FALSE);

		return requestID != -1;
	}

	return false;
}

bool CAppDataRequest::DeleteFile( const char * path = NULL, int localGamerIndex = -1 )
{	
	const rlRosCredentials & cred = rlRos::GetCredentials(localGamerIndex);
	if( cred.IsValid() && cred.GetRockstarId() != 0 )
	{		
		requestID = CloudManager::GetInstance().RequestDeleteMemberFile(localGamerIndex, path, RL_CLOUD_ONLINE_SERVICE_SC);

		requestType = APP_REQUEST_DELETE;

		return true;		
	}

	return false;
}

void CAppDataRequest::Cancel()
{
	m_AppData = NULL;
}

const char * CAppDataRequest::GetData() const
{
	return m_Data;
}

void CAppDataRequest::OnCloudEvent(const CloudEvent* pEvent)
{
	if( !pEvent )
	{
		return;
	}

	// we only care about requests
	if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
		return;

	// grab event data
	const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();

	if( !pEventData )
	{
		requestStatus = APP_FILE_STATUS_FAILED;
		return;
	}

	// check if we're interested
	if(pEventData->nRequestID != requestID)
		return;

	if (pEventData->bDidSucceed )
	{
		if( pEventData->pData )
		{
			if (pEventData->nDataSize <= APP_MAX_DATA_LENGTH)
			{
				safecpy(m_Data, static_cast<char*>(pEventData->pData), APP_MAX_DATA_LENGTH);
			}		
		}

		requestStatus = APP_FILE_STATUS_SUCCEEDED;
	}
	else
	{
		if( pEventData->nResultCode == 404 )
		{
			requestStatus = APP_FILE_STATUS_DOESNT_EXIST;
		}
		else
		{
			requestStatus = APP_FILE_STATUS_FAILED;
		}		
	}
}

////////////////////////////////////////////////////////////////////////////////////////////////////
CAppDataBlock::CAppDataBlock()
: m_Modified(false)
{
}

CAppDataBlock * CAppDataBlock::GetBlock( const char * name )
{
	return m_NestedBlocks.Access(atString(name));
}

CAppDataBlock * CAppDataBlock::SetBlock(const char * name)
{
	CAppDataBlock * dataBlock = m_NestedBlocks.Access(atString(name));

	if( !dataBlock )	
	{
		dataBlock = &m_NestedBlocks.Insert(atString(name)).data;
	}

	return dataBlock;
}

void CAppDataBlock::Update(const RsonReader & reader)
{
	if( m_Modified )
		return;

	RsonReader member, nestedMember;
	const unsigned int MAX_NAME_LENGTH = 128;
	const unsigned int MAX_VALUE_LENGTH = 128;
	char nameBuffer[MAX_NAME_LENGTH];
	char valueBuffer[MAX_VALUE_LENGTH];

	bool gotMember = reader.GetFirstMember(&member);

	while(gotMember)
	{
		if (member.GetName(nameBuffer, MAX_NAME_LENGTH))
		{
			atString nameString(nameBuffer);

			if (member.GetFirstMember(&nestedMember))
			{
				CAppDataBlock* dataBlock = m_NestedBlocks.Access(nameString);
				if (!dataBlock)
					dataBlock = &(m_NestedBlocks.Insert(nameString).data);
				
				dataBlock->Update(member);
			}
			else if (member.GetValue(valueBuffer, MAX_VALUE_LENGTH))
			{
				m_Members[nameString] = atString(valueBuffer);
			}
		}
		gotMember = member.GetNextSibling(&member);
	}
}

bool CAppDataBlock::GetInt(const char *name, int * value) const
{
	const atString * str = m_Members.Access(atString(name));
	if(!str)
	{
		return false;
	}
	const char* p = str->c_str();
	return p && sscanf(p, "%i", value) > 0;
}

bool CAppDataBlock::GetFloat(const char *name, float * value) const
{
	const atString * str = m_Members.Access(atString(name));
	if(!str)
	{
		return false;
	}
	const char* p = str->c_str();
	return p && sscanf(p, "%g", value) > 0;
}

bool CAppDataBlock::GetString(const char *name, const char **value) const
{
	const atString * str = m_Members.Access(atString(name));
	if(!str)
	{
		return false;
	}
	*value = str->c_str();
	return true;
}

const rage::atMap<atString, atString> &CAppDataBlock::GetMemberData() const
{
	return m_Members;
}

const rage::atMap<atString, CAppDataBlock> &CAppDataBlock::GetNestedMembers() const
{
	return m_NestedBlocks;
}

bool CAppDataBlock::IsModified() const
{
	return m_Modified;
}

void CAppDataBlock::Reset()
{
	m_Modified = false;
}

void CAppDataBlock::ClearMembers()
{
	m_Members.Reset();

	atMap<atString, CAppDataBlock>::Iterator blockDataIt = m_NestedBlocks.CreateIterator();

	while( !blockDataIt.AtEnd() )
	{
		CAppDataBlock appDataBlock = blockDataIt.GetData();

		appDataBlock.Reset();
	
		blockDataIt.Next();
	}

	m_NestedBlocks.Reset();

	//set to modified as we have just cleared the data and want this to be reflected in the cloud.
	m_Modified = true;
}

bool CAppDataBlock::SetInt(atString name, int value)
{
	char buffer[16];

	sprintf(buffer, "%i", value);

	atString data =  atString(buffer);

	if( m_Members[name] != data )
	{
		m_Members[name] = data;
		m_Modified = true;
		return true;
	}

	return false;
}

bool CAppDataBlock::SetFloat(atString name, float value)
{
	char buffer[32];

	sprintf(buffer, "%f", value);

	atString data =  atString(buffer);

	if( m_Members[name] != data )
	{
		m_Members[name] = data;
		m_Modified = true;
		return true;
	}

	return false;
}

bool CAppDataBlock::SetString(atString name, atString value)
{
	if( m_Members[name] != value )
	{
		m_Members[name] = value;
		m_Modified = true;
		return true;
	}

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
CAppData::CAppData()
: m_Modified(false),
  m_HasSyncedData(false)
{
}

void CAppData::Update(const RsonReader & reader)
{
	if( !m_HasSyncedData || !IsModified() )
	{
		char blockNameBuffer[256];
		if(reader.GetName(blockNameBuffer, NELEM(blockNameBuffer)))
		{
			if( strcmp("ownerID", blockNameBuffer) != 0 )
			{
				atString blockName = atString(blockNameBuffer);
				CAppDataBlock * dataBlock = m_Blocks.Access(blockName);
				if(!dataBlock)
				{
					dataBlock = &m_Blocks.Insert(blockName).data;
				}
				dataBlock->Update(reader);
			}
		}
	}
}

rage::atMap<atString, CAppDataBlock> *CAppData::GetBlockData()
{
	return &m_Blocks;
}

CAppDataBlock* CAppData::GetBlock()
{
	scriptAssertf(m_BlockQueue.GetCount() != 0, "%s - There are currently no blocks set", scrThread::GetCurrentCmdName());

	if( m_BlockQueue.GetCount() == 0 )
	{
		return NULL;
	}

	return m_BlockQueue.Top();
}

void CAppData::SetBlock(const char * blockName)
{
	if( m_BlockQueue.GetCount() == 0 || !m_BlockQueue.Top() )
	{
		CAppDataBlock * dataBlock = m_Blocks.Access(atString(blockName));
		
		if( !dataBlock )
		{
			dataBlock = &m_Blocks.Insert(atString(blockName)).data;
		}

		m_BlockQueue.PushAndGrow(dataBlock);

		return;	
	}
	
	CAppDataBlock* topBlock = m_BlockQueue.Top()->GetBlock( blockName );

	if( !topBlock )
	{
		topBlock = m_BlockQueue.Top()->SetBlock( blockName );
	}

	m_BlockQueue.PushAndGrow(topBlock);
}

void CAppData::CloseBlock()
{
	if ( m_BlockQueue.GetCount() > 0 )
	{
		m_BlockQueue.Pop();
	}
}

bool CAppData::AppendString(char **data, const char* end, const char* strToAppend)
{
	int result = snprintf(*data, end - *data, "%s", strToAppend);

	if( result < 0 || result + *data >= end )
		return false;

	*data += result;

	return true;
}

bool CAppData::AppendString(char **data, const char* end, const char* formatStr, const char* strToAppend)
{
	int result = snprintf(*data, end - *data, formatStr, strToAppend);

	if( result < 0 ||  result + *data >= end )
		return false;

	*data += result;
	
	return true;
}

bool CAppData::AppendString(char **data, const char* end, const char* formatStr, const char* strToAppend1, const char* strToAppend2)
{
	int result = snprintf(*data, end - *data, formatStr, strToAppend1, strToAppend2);

	if( result < 0 ||  result + *data >= end )
		return false;

	*data += result;

	return true;
}

bool CAppData::GetFormattedData( CAppDataBlock *block, atString blockName, char **data, char* end )
{
	atMap<atString, CAppDataBlock>::ConstIterator blockDataIt = block->GetNestedMembers().CreateIterator();

	const rage::atMap<atString, atString> &map = block->GetMemberData();
	
	if( !AppendString(data, end, "\"%s\":{", blockName.c_str()) )
		return false;	

	while( !blockDataIt.AtEnd() )
	{
		atString blockName = blockDataIt.GetKey();

		CAppDataBlock *appDataBlock = const_cast<CAppDataBlock *>(&blockDataIt.GetData());

		if( !GetFormattedData(appDataBlock, blockName, data, end) )
			return false;

		blockDataIt.Next();

		if( !blockDataIt.AtEnd())
			if( !AppendString(data, end, ",") )
				return false;
	}

	atMap<atString, atString>::ConstIterator it = map.CreateIterator();

	while(!it.AtEnd())
	{
		if( !AppendString(data, end, "\"%s\":\"%s\"", it.GetKey().c_str(), it.GetData().c_str()))
			return false;

		it.Next();

		if( !it.AtEnd())
			if( !AppendString(data, end, ",") )
				return false;
	}

	block->Reset();

	if( !AppendString(data, end, "}") )
		return false;

	return true;
}

bool CAppData::GetFormattedData( char *data, unsigned int &dataSize, int localGamerIndex = -1 )
{
	char* start = data;
	char* end = data + APP_MAX_DATA_LENGTH - 1;

	if( !AppendString(&data, end, "{\"version\":1,") )
		return false;

	const rlRosCredentials & cred = rlRos::GetCredentials(localGamerIndex);
	
	if( cred.IsValid() && cred.GetRockstarId() != 0 )
	{
		if( !AppendString(&data, end, "\"ownerID\":\"%s\",", cred.GetRockstarAccount().m_Nickname) )
			return false;
	}

	atMap<atString, CAppDataBlock>::Iterator blockDataIt = m_Blocks.CreateIterator();

	while(!blockDataIt.AtEnd())
	{
		CAppDataBlock *appDataBlock = &blockDataIt.GetData();

		atString blockName = blockDataIt.GetKey();

		if( !GetFormattedData( appDataBlock, blockName, &data, end) )
			return false;

		blockDataIt.Next();

		if( !blockDataIt.AtEnd())
				if( !AppendString(&data, end, ",") )
					return false;
	}

	if( !AppendString(&data, end, "}") )
		return false;

	dataSize = ustrlen( start ) + 1;

	return true;
}

CAppDataRequestMgr::CAppDataRequestMgr() :
	m_Request(NULL)
{
}

void CAppDataRequestMgr::ShutDown()
{
	m_requestQueue.Reset();

	if( m_Request )
	{
		m_Request->Cancel();
		delete m_Request;
		m_Request = NULL;
	}

	currentPollRequest = NULL;
}

void CAppDataRequestMgr::AddRequest(sPollRequest request)
{
	Assert(m_requestQueue.GetCount() <= 100);

	//add request
	m_requestQueue.PushAndGrow(request);
}

void CAppDataRequestMgr::Update()
{
	switch (m_currentState)
	{
	case CAppDataRequestMgr::REQUEST_STATE_IDLE:
		//get the next request
		if( m_requestQueue.GetCount() > 0 )
		{
			//always get the first index
			currentPollRequest = &m_requestQueue[0];
		}

		//change state to the correct type depending on whether we are
		//send on receiving data
		if( currentPollRequest )
		{			
			Assert(!m_Request);
			
			if( !m_Request )
			{
				USE_MEMBUCKET(MEMBUCKET_NETWORK);
				m_Request = rage_new CAppDataRequest(currentPollRequest->appData);
			}					

			//handle the request if it fails to allocate.
			if( !m_Request )
			{
				currentPollRequest = NULL;
				m_currentState = REQUEST_STATE_IDLE;
				break;
			}

			//we are requesting data from the cloud so move to the fetch state
			switch(currentPollRequest->requestType)
			{
			case APP_REQUEST_GET:
				{
					m_currentState = REQUEST_STATE_FETCH;
					break;
				}
			case APP_REQUEST_POST:
				{
					m_currentState = REQUEST_STATE_PUSH;
					break;
				}
			case APP_REQUEST_DELETE:
				{
					m_currentState = REQUEST_STATE_DELETE;
					break;
				}
			default:
				{
					m_currentState = REQUEST_STATE_CLEAN_UP;
					break;
				}
			}
		}
		break;
	case CAppDataRequestMgr::REQUEST_STATE_FETCH:
		{
			Assert(m_Request);

			bool result = m_Request->RequestFile(currentPollRequest->path, CAppDataMgr::sm_LocalGamerIndex);

			m_currentState = result ? REQUEST_STATE_UPDATE : REQUEST_STATE_CLEAN_UP;
		}
		break;
	case CAppDataRequestMgr::REQUEST_STATE_PUSH:
		{
			Assert(m_Request);

			bool result = m_Request->PushFile(currentPollRequest->path, CAppDataMgr::sm_LocalGamerIndex, currentPollRequest->appData);

			m_currentState = result ? REQUEST_STATE_UPDATE : REQUEST_STATE_CLEAN_UP;
		}
		break;
	case CAppDataRequestMgr::REQUEST_STATE_DELETE:
		{
			Assert(m_Request);

			bool result = m_Request->DeleteFile(currentPollRequest->path, CAppDataMgr::sm_LocalGamerIndex);

			m_currentState = result ? REQUEST_STATE_UPDATE : REQUEST_STATE_CLEAN_UP;
		}
		break;
	case CAppDataRequestMgr::REQUEST_STATE_UPDATE:
		{
			Assert(m_Request);

			appDataFileStatus status = m_Request->GetRequestStatus();
			
			switch (status)
			{
			case APP_FILE_STATUS_NONE:
				{
					//net status is none, clean up
					m_currentState = REQUEST_STATE_CLEAN_UP;
				}
				break;
			case APP_FILE_STATUS_PENDING:
				{
					//wait here whilst we process the request
				}
				break;
			case APP_FILE_STATUS_SUCCEEDED:
				{
					RequestSucceeded(m_Request);

					m_currentState = REQUEST_STATE_CLEAN_UP;
				}
				break;
			case APP_FILE_STATUS_FAILED:
				{
					RequestFailed( m_Request );
			 		//we've failed to process the file
					m_currentState = REQUEST_STATE_CLEAN_UP;
				}
				break;
			case APP_FILE_STATUS_DOESNT_EXIST:
				{
					RequestFileNotFound( m_Request );

					m_currentState = REQUEST_STATE_CLEAN_UP;
				}
				break;			
			default:
				break;
			}
		}
		break;	
	case REQUEST_STATE_CLEAN_UP:
		{
			Assert(m_Request);
			//clean up the requests
			if( m_Request )
			{
				delete m_Request;
				m_Request = NULL;
			}
			currentPollRequest = NULL;

			Assert(m_requestQueue.GetCount() > 0);

			if( m_requestQueue.GetCount() > 0 )
				//remove the request we just processed
				m_requestQueue.Delete(0);

			m_currentState = REQUEST_STATE_IDLE;
		}
		break;
	default:

		break;
	}
}

void CAppDataRequestMgr::RequestSucceeded(CAppDataRequest *request)
{
	Assert(request);

	switch(request->GetRequestType())
	{
	case APP_REQUEST_GET:
		{
			CAppData* appData = m_Request->GetAppData();

			const char  * data = m_Request->GetData();

			if( data && appData )
			{
				RsonReader reader, blockReader;

				reader.Init(data, 0, (unsigned) strlen(data));

				bool headerFound = false;
				if( reader.GetFirstMember(&blockReader))
				{
					char blockNameBuffer[256];
					if(blockReader.GetName(blockNameBuffer, NELEM(blockNameBuffer)) )
					{
						if( strcmp("version", blockNameBuffer) == 0 )
						{
							headerFound  = true;
							while( blockReader.GetNextSibling(&blockReader) )
							{
								appData->Update(blockReader);
							}
						}
					}
				}

				Assertf( headerFound, "Weird data in appdata file.(No 'version' found)\n");

				//tell the app data we have synced
				appData->SetSyncedData( TRUE );
			}

			break;
		}

	case APP_REQUEST_POST:
		{
			//stub if we need to do anything special here
			break;
		}
	case APP_REQUEST_DELETE:
		{
			//stub if we need to do anything special here
			CAppDataMgr::SetDeletedFileStatus(APP_FILE_STATUS_SUCCEEDED);
			break;
		}
	default:
		{
			break;
		}						
	}			
}

void CAppDataRequestMgr::RequestFailed(CAppDataRequest *request)
{
	Assert(request);

	switch(request->GetRequestType())
	{
	case APP_REQUEST_GET:
		{
			break;
		}

	case APP_REQUEST_POST:
		{
			//stub if we need to do anything special here
			break;
		}
	case APP_REQUEST_DELETE:
		{
			//stub if we need to do anything special here
			CAppDataMgr::SetDeletedFileStatus(APP_FILE_STATUS_FAILED);
			break;
		}
	default:
		{
			break;
		}						
	}
}

void CAppDataRequestMgr::RequestFileNotFound(CAppDataRequest *request)
{
	Assert(request);

	switch(request->GetRequestType())
	{
	case APP_REQUEST_GET:
		{
			
			break;
		}

	case APP_REQUEST_POST:
		{
			//stub if we need to do anything special here
			break;
		}
	case APP_REQUEST_DELETE:
		{
			//stub if we need to do anything special here
			CAppDataMgr::SetDeletedFileStatus(APP_FILE_STATUS_DOESNT_EXIST);
			break;
		}
	default:
		{
			break;
		}						
	}	

	CAppData* appData = request->GetAppData();

	//theres no file in the cloud so mark the app data as synced.
	if( appData )
	{
		appData->SetSyncedData( TRUE );
	}
};

void CAppDataMgr::Init(unsigned int UNUSED_PARAM(initMode))
{
	sm_Apps.Insert(atString("car"));
	sm_Apps.Insert(atString("dog"));

	rlPresence::AddDelegate(&sm_PresenceDelegate);
	
	rlCloud::WatchMemberItem(sm_LocalGamerIndex, RL_CLOUD_ONLINE_SERVICE_SC, CONFIG_CAR_APP_JSON, &appdataCarWatcher);
	rlCloud::WatchMemberItem(sm_LocalGamerIndex, RL_CLOUD_ONLINE_SERVICE_SC, CONFIG_DOG_APP_JSON, &appdataDogWatcher);
}

void CAppDataMgr::InitCloudFiles()
{
	//add these requests to the request manager
	//fetch dog data
	//FileUpdated(CONFIG_DOG_GAME_JSON, "dog");
	FileUpdated(CONFIG_DOG_APP_JSON, "dog");

	//fetch car data
	FileUpdated(CONFIG_CAR_GAME_JSON, "car");
	FileUpdated(CONFIG_CAR_APP_JSON, "car");
}

void CAppDataMgr::Update()
{
	if( CAppDataMgr::IsOnline() )
	{
		bool hasScAcc = HasSocialClubAccount();
		if( sm_LastHasScAccount != hasScAcc )
		{
			if( hasScAcc )
			{
				rlPresence::RemoveDelegate(&sm_PresenceDelegate);	
				rlCloud::UnwatchMemberItem(&appdataDogWatcher);
				rlCloud::UnwatchMemberItem(&appdataCarWatcher);

				rlPresence::AddDelegate(&sm_PresenceDelegate);

				rlCloud::WatchMemberItem(sm_LocalGamerIndex, RL_CLOUD_ONLINE_SERVICE_SC, CONFIG_CAR_APP_JSON, &appdataCarWatcher);
				rlCloud::WatchMemberItem(sm_LocalGamerIndex, RL_CLOUD_ONLINE_SERVICE_SC, CONFIG_DOG_APP_JSON, &appdataDogWatcher);
				
				sm_ReloadWhenCloudAvailable = true;
			}
		}
		sm_LastHasScAccount = hasScAcc;
	}
	else
	{
		sm_LastHasScAccount = false;
	}

	if( sm_ReloadWhenCloudAvailable )
	{
		const rlRosCredentials & cred = rlRos::GetCredentials(sm_LocalGamerIndex);
		if( cred.IsValid() && cred.GetRockstarId() != 0 )
		{
			//reload the cloud files as we have just changed credentials
			CAppDataMgr::InitCloudFiles();

			sm_ReloadWhenCloudAvailable = false;			
		}
	}

	sm_requestManager.Update();

#if __BANK
	Bank_Update();
#endif
}

void CAppDataMgr::Shutdown(unsigned int UNUSED_PARAM(shutdownMode))
{
	rlPresence::RemoveDelegate(&sm_PresenceDelegate);	
	rlCloud::UnwatchMemberItem(&appdataDogWatcher);
	rlCloud::UnwatchMemberItem(&appdataCarWatcher);

	sm_requestManager.ShutDown();
}

void CAppDataMgr::OnPresenceEvent(const rlPresenceEvent* evt)
{
	(void)evt;

    //if(PRESENCE_EVENT_SIGNIN_STATUS_CHANGED == evt->GetId())
    //{
	//	const rlPresenceEventSigninStatusChanged* s = evt->m_SigninStatusChanged;
	//	if(s->SignedOnline())
	//	{
	//		sm_ReloadWhenCloudAvailable = true;
	//	}
	//}
}

CAppData* CAppDataMgr::GetAppData(const char * appName)
{
	return sm_Apps.Access(atString(appName));
}

void CAppDataMgr::FileUpdated(const char * path, const char * appName )
{	
	atString usePath = atString(path);

	CAppData* appData = GetAppData(appName);

	sPollRequest pollRequest(usePath, appData, APP_REQUEST_GET);

	sm_requestManager.AddRequest(pollRequest);
}

void CAppDataMgr::PushFile( const char * appName )
{	
	CAppData* appData = GetAppData(appName);

	if( appData && appData->IsModified() )
	{
		atString usePath = atString("");

		if( strcmp(appName, "car") == 0 )
		{
			usePath = atString(CONFIG_CAR_GAME_JSON);
		}
		else if( strcmp(appName, "dog") == 0 )
		{
			usePath = atString(CONFIG_DOG_GAME_JSON);
		}

		//add the request to the request manager.
		sPollRequest pollRequest(usePath, appData, APP_REQUEST_POST);
		sm_requestManager.AddRequest(pollRequest);
	}		
}

bool CAppDataMgr::DeleteCloudData( const char* appName, const char* fileName )
{	
	char path[128];
#if RSG_XBL
	#if RSG_XENON
			sprintf(path, "GTA5/%s/%sxbl.json", appName, fileName);
	#elif RSG_DURANGO
		sprintf(path, "GTA5/%s/%sxblxboxone.json", appName, fileName);
	#endif
#elif RSG_NP
	#if RSG_ORBIS	
		sprintf(path, "GTA5/%s/%snpps4.json", appName, fileName);
	#elif
		sprintf(path, "GTA5/%s/%snp.json", appName, fileName);
	#endif
#elif __WIN32PC || RSG_DURANGO
	sprintf(path, "GTA5/%s/%ssc.json", appName, fileName);
#endif

	//add the request to the request manager.
	sPollRequest pollRequest(atString(path), NULL, APP_REQUEST_DELETE);
	sm_requestManager.AddRequest(pollRequest);
	sm_DeleteAppDataStatus = APP_FILE_STATUS_PENDING;

	return false;
}

bool CAppDataMgr::IsLinkedToSocialClub()
{
	const rlRosCredentials & cred = rlRos::GetCredentials(sm_LocalGamerIndex);
	return cred.IsValid() && ( cred.GetRockstarId() != 0 );
}

appDataFileStatus CAppDataMgr::GetDeletedFileStatus()
{
	return sm_DeleteAppDataStatus;
}

void CAppDataMgr::SetDeletedFileStatus(appDataFileStatus status)
{
	sm_DeleteAppDataStatus = status;
}

void CAppDataMgr::WriteCloudDataToFile( const char* appName )
{
	const char *pParamValue;
	if(!PARAM_AppsDir.GetParameter(pParamValue))
	{
		Assertf(0, "-AppsDir has not been set in the command line");
		return;
	}

	char logFilename[RAGE_MAX_PATH];
	sprintf(logFilename, "%s/Apps/%sData.txt", pParamValue, appName);

	ASSET.CreateLeadingPath(logFilename);
	fiStream* pStream = ASSET.Create(logFilename, "");

	CAppData*appData = CAppDataMgr::GetAppData(appName);

	if( appData )
	{
		char data[APP_MAX_DATA_LENGTH] = {0};
		unsigned int dataLength;

		bool result = appData->GetFormattedData(data, dataLength, sm_LocalGamerIndex);

		if( !result )
		{
			scriptAssertf(false, "APP_MAX_DATA_LENGTH (%d) is too small to send this much data to the cloud.", APP_MAX_DATA_LENGTH);
		}

		if(pStream)
		{
			pStream->Write(data, dataLength);
			pStream->Close();
		}
	}
	else
	{
		Assertf(0, "There is no data for app %s", appName);
	}
}

bool CAppDataMgr::HasSocialClubAccount()
{
	return( NetworkInterface::HasValidRockstarId() );
}

bool CAppDataMgr::IsOnline()
{
	return( NetworkInterface::IsLocalPlayerOnline() && NetworkInterface::IsOnlineRos() );
}

void appdataOnCarUpate(const char* msgA, const char* msgB)
{	
	(void)msgB;
	CAppDataMgr::FileUpdated(msgA, "car");

#if __BANK
	CAppDataMgr::Bank_PresenceUpdateCar(msgA,msgB);
#endif

}

void appdataOnDogUpate(const char* msgA, const char* msgB)
{
	(void)msgB;
	CAppDataMgr::FileUpdated(msgA, "dog");

#if __BANK
	CAppDataMgr::Bank_PresenceUpdateDog(msgA,msgB);
#endif

}

rlPresence::Delegate CAppDataMgr::sm_PresenceDelegate(&CAppDataMgr::OnPresenceEvent);

int CAppDataMgr::sm_LocalGamerIndex = 0;
bool CAppDataMgr::sm_ReloadWhenCloudAvailable = false;
bool CAppDataMgr::sm_LastHasScAccount = false;
rage::atMap<atString, CAppData> CAppDataMgr::sm_Apps;
CAppDataRequestMgr CAppDataMgr::sm_requestManager;
appDataFileStatus CAppDataMgr::sm_DeleteAppDataStatus;

////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
#if __BANK

#define APPDATA_BANK_HAS_SC_ACCOUNT_MAX			64
#define APPDATA_BANK_PRESENCEUPDATEMSGCAR_MAX	128
#define APPDATA_BANK_PRESENCEUPDATEMSGDOG_MAX	128
static int	appdataPresenceUpdateCountCar = 0;
static int	appdataPresenceUpdateCountDog = 0;
static char	appdataBankHasScAccount[APPDATA_BANK_HAS_SC_ACCOUNT_MAX];
static char	appdataPresenceUpdateMsgCar[APPDATA_BANK_PRESENCEUPDATEMSGCAR_MAX];
static char	appdataPresenceUpdateMsgDog[APPDATA_BANK_PRESENCEUPDATEMSGDOG_MAX];

void CAppDataMgr::InitWidgets()
{
	bkBank* bank = &BANKMGR.CreateBank("Apps", 0, 0, false); 
	if(Verifyf(bank, "Failed to create Apps bank"))
	{
		bank->AddButton("Create Apps Widgets", datCallback(CFA1(CAppDataMgr::AddWidgets), bank));
	}
}

void CAppDataMgr::AddWidgets(bkBank& bank)
{
	// destroy first widget which is the create button
	bkWidget* widget = BANKMGR.FindWidget("Apps/Create Apps Widgets");
	if(widget == NULL)
	{
		return;
	}
	widget->Destroy();

	bank.AddButton("Write car app data to file", WriteCarAppDataToFile);
	bank.AddButton("Write dog app data to file", WriteDogAppDataToFile);
	bank.AddButton("Delete car app data", DeleteCarAppData);
	bank.AddButton("Delete car game data", DeleteCarGameData);

	memset(appdataBankHasScAccount,0,sizeof(appdataBankHasScAccount));
	memset(appdataPresenceUpdateMsgCar,0,sizeof(appdataPresenceUpdateMsgCar));
	memset(appdataPresenceUpdateMsgDog,0,sizeof(appdataPresenceUpdateMsgDog));

	bank.AddText( "GameState                :", appdataBankHasScAccount, sizeof(appdataBankHasScAccount), true);
	bank.AddText( "Last Presence Msg (Car)  :", appdataPresenceUpdateMsgCar, sizeof(appdataPresenceUpdateMsgCar), true);
	bank.AddText( "Last Presence Msg (Dog)  :", appdataPresenceUpdateMsgDog, sizeof(appdataPresenceUpdateMsgDog), true);
}

void CAppDataMgr::Bank_PresenceUpdateCar(const char* msgA, const char* msgB)
{
	appdataPresenceUpdateCountCar++;
	formatf( appdataPresenceUpdateMsgCar, sizeof(appdataPresenceUpdateMsgCar),"[%d] %s / %s", appdataPresenceUpdateCountCar, msgA, msgB );
}

void CAppDataMgr::Bank_PresenceUpdateDog(const char* msgA, const char* msgB)
{
	appdataPresenceUpdateCountDog++;
	formatf( appdataPresenceUpdateMsgDog, sizeof(appdataPresenceUpdateMsgDog),"[%d] %s / %s", appdataPresenceUpdateCountDog, msgA, msgB );
}

void CAppDataMgr::Bank_Update()
{
	char tmpHasSocialClubAccount[32];
	if( HasSocialClubAccount() )
	{
		strcpy( tmpHasSocialClubAccount, "Yes" );
	}
	else
	{
		strcpy( tmpHasSocialClubAccount, "No" );
	}

	char tmpIsOnline[32];
	if( IsOnline() )
	{
		strcpy( tmpIsOnline, "Yes" );
	}
	else
	{
		strcpy( tmpIsOnline, "No" );
	}

	formatf( appdataBankHasScAccount, sizeof(appdataBankHasScAccount), "Online? : %s, HasScAccount? : %s", tmpIsOnline, tmpHasSocialClubAccount );
}

#endif
