//
// apps/appdata.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved.
//

#ifndef APP_DATA_H
#define APP_DATA_H

#if RSG_XBL
	#if RSG_XENON
		#define CONFIG_CAR_GAME_JSON	"GTA5/car/gamexbl.json"
		#define CONFIG_CAR_APP_JSON		"GTA5/car/appxbl.json"
		#define CONFIG_DOG_GAME_JSON	"GTA5/dog/gamexbl.json"
		#define CONFIG_DOG_APP_JSON		"GTA5/dog/appxbl.json"
	#elif RSG_DURANGO
		#define CONFIG_CAR_GAME_JSON	"GTA5/car/gamexblxboxone.json"
		#define CONFIG_CAR_APP_JSON		"GTA5/car/appxblxboxone.json"
		#define CONFIG_DOG_GAME_JSON	"GTA5/dog/gamexblxboxone.json"
		#define CONFIG_DOG_APP_JSON		"GTA5/dog/appxblxboxone.json"
	#endif 
#elif RSG_NP
	#if RSG_ORBIS
		#define CONFIG_CAR_GAME_JSON	"GTA5/car/gamenpps4.json"
		#define CONFIG_CAR_APP_JSON		"GTA5/car/appnpps4.json"
		#define CONFIG_DOG_GAME_JSON	"GTA5/dog/gamenps4.json"
		#define CONFIG_DOG_APP_JSON		"GTA5/dog/appnpps4.json"
	#elif RSG_PS3
		#define CONFIG_CAR_GAME_JSON	"GTA5/car/gamenp.json"
		#define CONFIG_CAR_APP_JSON		"GTA5/car/appnp.json"
		#define CONFIG_DOG_GAME_JSON	"GTA5/dog/gamenp.json"
		#define CONFIG_DOG_APP_JSON		"GTA5/dog/appnp.json"
	#endif 
#elif RSG_PC
#define CONFIG_CAR_GAME_JSON	"GTA5/car/gamesc.json"
#define CONFIG_CAR_APP_JSON		"GTA5/car/appsc.json"
#define CONFIG_DOG_GAME_JSON	"GTA5/dog/gamesc.json"
#define CONFIG_DOG_APP_JSON		"GTA5/dog/appsc.json"
#endif

#define APP_MAX_DATA_LENGTH (52*1024)

#include "atl/map.h"
#include "atl/string.h"
#include "data/growbuffer.h"
#include "net/status.h"
#include "rline/rlpresence.h"
#include "Network/Cloud/CloudManager.h"

class CAppDataBlock;
class CAppData;

////////////////////////////////////////////////////////////////////////////////////////////////////
namespace rage
{
	class bkBank;
	class RsonReader;
}

static const s32 MAX_NUM_POLL_REQUESTS = 10;

typedef enum
{
	APP_FILE_STATUS_NONE = 0,
	APP_FILE_STATUS_PENDING,
	APP_FILE_STATUS_SUCCEEDED,
	APP_FILE_STATUS_FAILED,
	APP_FILE_STATUS_DOESNT_EXIST,
}
appDataFileStatus;

typedef enum
{
	APP_REQUEST_POST = 0,
	APP_REQUEST_GET,
	APP_REQUEST_DELETE,
}
AppRequestType;

struct sPollRequest
{
public:
	sPollRequest() {}
	
	sPollRequest(atString path, CAppData* appData, AppRequestType requestType ) :
		path(path),
		appData(appData),
		requestType(requestType)
	{
	}

	CAppData * appData;
	atString path;	
	atString blockName;
	AppRequestType requestType;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class CAppDataRequest : public CloudListener

{
public:
	CAppDataRequest(CAppData *appdata);
	~CAppDataRequest();

	bool RequestFile(const char * path, int localGamerIndex);
	bool PushFile( const char * path, int localGamerIndex,  CAppData* appData );
	bool DeleteFile( const char * path, int localGamerIndex );

	const char * GetData() const;	
	void Cancel();
	CAppData* GetAppData() { return m_AppData; }
	
	CloudRequestID GetRequestID() const { return requestID; }
	AppRequestType GetRequestType() { return requestType; }
	appDataFileStatus GetRequestStatus() { return requestStatus; }
	virtual void OnCloudEvent(const CloudEvent* pEvent);

private:	
	CAppData * m_AppData;	
	char m_Data[APP_MAX_DATA_LENGTH];

	CloudRequestID requestID;
	AppRequestType requestType;
	appDataFileStatus requestStatus;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class CAppDataBlock
{
public:
	CAppDataBlock();
	void Update(const RsonReader & reader);
	bool GetInt(const char * name, int * value) const;
	bool GetFloat(const char * name, float * value) const;
	bool GetString(const char * name, const char** value) const;

	CAppDataBlock * SetBlock(const char * name);
	CAppDataBlock* GetBlock( const char * name );

	bool SetInt(atString name, int value);
	bool SetFloat(atString name, float value);
	bool SetString(atString name, atString value);

	const rage::atMap<atString, atString> &GetMemberData() const;
	const rage::atMap<atString, CAppDataBlock> &GetNestedMembers() const;
	bool IsModified() const;
	void Reset();
	void ClearMembers();

private:
	rage::atMap<atString, atString> m_Members;
	rage::atMap<atString, CAppDataBlock> m_NestedBlocks;
	bool m_Modified;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class CAppData
{
public:
	CAppData();
	void Update(const RsonReader & reader);
	rage::atMap<atString, CAppDataBlock> *GetBlockData();

	bool HasSyncedData() const { return m_HasSyncedData; }
	void SetSyncedData(bool value){ m_HasSyncedData = value; }

	bool AppendString(char **data, const char* end, const char* strToAppend);
	bool AppendString(char **data, const char* end, const char* formatStr, const char* strToAppend);
	bool AppendString(char **data, const char* end, const char* formatStr, const char* strToAppend1, const char* strToAppend2);
	
	bool GetFormattedData( CAppDataBlock *block, atString blockName, char **data, char* end );
	bool GetFormattedData( char *data, unsigned int &dataSize, int localGamerIndex );

	CAppDataBlock* GetBlock();
	void SetBlock(const char * blockName);
	void CloseBlock();
	void SetModified(bool modified){ m_Modified = modified; }
	bool IsModified() const { return m_Modified; }

private:
	rage::atMap<atString, CAppDataBlock> m_Blocks;
	rage::atArray<CAppDataBlock*> m_BlockQueue;

	bool m_HasSyncedData;
	bool m_Modified;
};

class CAppDataRequestMgr
{
public:
	typedef enum
	{
		REQUEST_STATE_IDLE = 0,
		REQUEST_STATE_FETCH,
		REQUEST_STATE_PUSH,
		REQUEST_STATE_DELETE,
		REQUEST_STATE_UPDATE,
		REQUEST_STATE_CLEAN_UP,
	}
	REQUEST_STATE;

	CAppDataRequestMgr();

	void Update();

	void RequestSucceeded(CAppDataRequest *request);
	void RequestFailed(CAppDataRequest *request);
	void RequestFileNotFound(CAppDataRequest *request);

	void AddRequest(sPollRequest request);

	void ShutDown();

private:	
	REQUEST_STATE m_currentState;
	rage::atArray<sPollRequest> m_requestQueue;
	sPollRequest* currentPollRequest;
	CAppDataRequest* m_Request;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
class CAppDataMgr
{
public:
#if __BANK
	static void InitWidgets();
	static void AddWidgets(bkBank& bank);
	static void Bank_PresenceUpdateCar(const char* msgA, const char* msgB);
	static void Bank_PresenceUpdateDog(const char* msgA, const char* msgB);
	static void Bank_Update();
#endif

	static void Init(unsigned int initMode);
	static void Update();
	static void Shutdown(unsigned int shutdownMode);
	static void OnPresenceEvent(const rlPresenceEvent* evt);

	static CAppData* GetAppData(const char * appName);

	static void Unlock(const char * name);

	static void FileUpdated(const char * path, const char * appName);

	static void PushFile( const char * appName );

	static bool DeleteCloudData( const char* appName, const char* fileName );

	static void InitCloudFiles();

	static bool IsLinkedToSocialClub();

	static appDataFileStatus GetDeletedFileStatus();

	static void SetDeletedFileStatus(appDataFileStatus status);

	static void WriteCloudDataToFile( const char* appName );

	static bool HasSocialClubAccount();
	static bool IsOnline();

	static int sm_LocalGamerIndex;

private:
	static rlPresence::Delegate sm_PresenceDelegate;

	static bool sm_ReloadWhenCloudAvailable;
	static bool sm_LastHasScAccount;
	static datGrowBuffer sm_ReceiveBuffer;
	static netStatus sm_ReceiveStatus;

	static appDataFileStatus sm_DeleteAppDataStatus;
	 
	static rage::atMap<atString, CAppData> sm_Apps;

	static CAppDataRequestMgr sm_requestManager;
};


void appdataOnCarUpate(const char* msgA, const char* msgB);
void appdataOnDogUpate(const char* msgA, const char* msgB);

#endif	//	APP_DATA_H

