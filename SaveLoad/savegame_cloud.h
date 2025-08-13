//
// savegame_cloud.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef SAVEGAME_CLOUD_H
#define SAVEGAME_CLOUD_H

//rage headers
#include "net/status.h"
#include "data/growbuffer.h"
#include "rline/cloud/rlcloud.h"

//Absolute Maximum allowed files for the savegame.
const u32 MAX_CLOUD_BUFFER_SIZE = (105 * 1024);

//PURPOSE
//  Manage cloud savegames for the cloud saves. File extension is .save and the header must respect:
//    - 4 bytes = 'g' 't' 'a' '5'.
//    - Remaining bytes are the actual buffer saved.
class CSaveGameCloudData
{
public:
	enum eServiceType
	{ 
		SERVICE_INVALID
		,SERVICE_REQUEST
		,SERVICE_POST
	};

private:
	netStatus      m_Status;
	eServiceType   m_ServiceType;
	datGrowBuffer  m_ReceiveBuffer;
	void*          m_SendBuffer;
	rlCloudFileInfo m_CloudFileInfo;

protected:
	u32            m_SendBufferSize;

public:
	CSaveGameCloudData()
		: m_ServiceType(SERVICE_INVALID)
		, m_SendBufferSize(0)
		, m_SendBuffer(0)
	{;}
	~CSaveGameCloudData() { Shutdown(); }

	void  Shutdown();
	void  Cancel();

	bool  RequestFile(const char* path, const int localGamerIndex, const rlCloudMemberId& member, const rlCloudOnlineService service);
	bool  PostFile(void* data, const u32 dataSize, const char* path, const int localGamerIndex, const rlCloudOnlineService service);

	bool  Pending() const;
	bool  Succeeded() const;
	int   GetResultCode() const;

	void* GetBuffer();
	u32   GetBufferLength() const;
	eServiceType GetServiceType() const { return m_ServiceType; }

	void  CheckBufferSize() const;

};

#endif // SAVEGAME_CLOUD_H

// EOF
