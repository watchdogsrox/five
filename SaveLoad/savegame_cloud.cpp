//
// savegame_cloud.cpp
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

//Rage headers
#include "system/param.h"
#include "rline/ros/rlros.h"
#include "rline/ros/rlroscommon.h"

//Game headers
#include "savegame_cloud.h"
#include "SaveLoad/savegame_channel.h"
#include "network/Network.h"

SAVEGAME_OPTIMISATIONS()

PARAM(noSavegameCloud, "[savegame_cloud] Disable cloud saves for savegame.");

RAGE_DEFINE_SUBCHANNEL(savegame, cloud, DIAG_SEVERITY_DEBUG3)
#undef __savegame_channel
#define __savegame_channel savegame_cloud

// ----------- CSaveGameCloudData

void 
CSaveGameCloudData::Shutdown()
{
	if(Pending())
	{
		rlCloud::Cancel(&m_Status);
	}

	m_SendBuffer = 0;
	m_SendBufferSize = 0;
	m_ServiceType = SERVICE_INVALID;
	m_ReceiveBuffer.Clear();
}

bool 
CSaveGameCloudData::RequestFile(const char* path, const int localGamerIndex, const rlCloudMemberId& member, const rlCloudOnlineService service)
{
	bool result = false;

	if (savegameVerify(path) && savegameVerify(rlCloud::IsInitialized()) NOTFINAL_ONLY(&& !PARAM_noSavegameCloud.Get()))
	{
		const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
		if(cred.IsValid())
		{
			savegameAssertf(!m_ReceiveBuffer.GetBuffer(), "Invalid buffer - Not NULL");
			m_ReceiveBuffer.Init(CNetwork::GetNetworkHeap(), datGrowBuffer::NULL_TERMINATE);

			savegameDebugf1("Request cloud savegame:");
			savegameDebugf1("               Path = \"%s\"", path);
			savegameDebugf1("    Buffer Capacity = \"%u\"", m_ReceiveBuffer.GetCapacity());

			m_ServiceType = SERVICE_REQUEST;

			m_CloudFileInfo.Clear();

			result = rlCloud::GetMemberFile(localGamerIndex,
											member,
											service,
											path,
                                            RLROS_SECURITY_DEFAULT,
											0,  //ifModifiedSincePosixTime
                                            NET_HTTP_OPTIONS_NONE,
											m_ReceiveBuffer.GetFiDevice(),
											m_ReceiveBuffer.GetFiHandle(),
											&m_CloudFileInfo,
                                            NULL,   //allocator
											&m_Status);
		}
	}

	if (!result)
	{
		if(Pending())
		{
			rlCloud::Cancel(&m_Status);
		}

		m_Status.Reset();
		m_Status.SetPending();
		m_Status.SetFailed();
	}

	return result;
}

bool 
CSaveGameCloudData::PostFile(void* data, const u32 dataSize, const char* path, const int localGamerIndex, const rlCloudOnlineService service)
{
	bool result = false;

	if (savegameVerify(data) && savegameVerify(dataSize) && savegameVerify(path) && savegameVerify(rlCloud::IsInitialized()) NOTFINAL_ONLY(&& !PARAM_noSavegameCloud.Get()))
	{
		const rlRosCredentials& cred = rlRos::GetCredentials(localGamerIndex);
		if(cred.IsValid())
		{
			m_SendBuffer = data;
			m_SendBufferSize = dataSize;

			savegameDebugf1("Post cloud savegame:");
			savegameDebugf1("               Path = \"%s\"", path);
			savegameDebugf1("    Buffer Capacity = \"%u\"", m_ReceiveBuffer.GetCapacity());
			savegameDebugf1("        Buffer size = \"%u\"", m_SendBufferSize);

			savegameAssertf(m_SendBufferSize <= MAX_CLOUD_BUFFER_SIZE
							,"Trying to post a multiplayer cloud file name '%s' with invalid size '%u' bigger than the max value of '%u'"
							,path
							,m_SendBufferSize
							,MAX_CLOUD_BUFFER_SIZE);

			if (m_SendBufferSize > MAX_CLOUD_BUFFER_SIZE)
			{
				savegameErrorf("Trying to post a multiplayer cloud file name '%s' with invalid size '%u' bigger than the max value of '%u'", path, m_SendBufferSize, MAX_CLOUD_BUFFER_SIZE);
				result = false;
			}
			else
			{
				m_ServiceType = SERVICE_POST;

				m_CloudFileInfo.Clear();

				result = rlCloud::PostMemberFile(localGamerIndex,
												service,
												path,
												m_SendBuffer,
												m_SendBufferSize,
												rlCloud::POST_REPLACE,
												&m_CloudFileInfo,
												NULL,   //allocator
												&m_Status);
			}
		}
	}

	if (!result)
	{
		if(Pending())
		{
			rlCloud::Cancel(&m_Status);
		}

		m_Status.Reset();
		m_Status.SetPending();
		m_Status.SetFailed();
	}

	return result;
}

void  
CSaveGameCloudData::Cancel()
{
	if(Pending())
	{
		rlCloud::Cancel(&m_Status);
	}

	m_ServiceType = SERVICE_INVALID;
}

bool 
CSaveGameCloudData::Pending() const
{
	return m_Status.Pending();
}

bool 
CSaveGameCloudData::Succeeded() const
{
	return m_Status.Succeeded();
}

int
CSaveGameCloudData::GetResultCode() const
{
	return m_Status.GetResultCode();
}

void* 
CSaveGameCloudData::GetBuffer()
{
	return m_ReceiveBuffer.GetBuffer();
}

u32 
CSaveGameCloudData::GetBufferLength() const
{
	return m_ReceiveBuffer.Length();
}

void
CSaveGameCloudData::CheckBufferSize() const
{
	savegameAssertf(m_CloudFileInfo.m_ContentLength < MAX_CLOUD_BUFFER_SIZE
					,"CSaveGameCloudData::CheckBufferSize() - cloud file with path '%s' has invalid size '%u' bigger than the max value of '%u'"
					,m_CloudFileInfo.m_Location
					,m_CloudFileInfo.m_ContentLength
					,MAX_CLOUD_BUFFER_SIZE);

	savegameDebugf1("        ContentLength = '%u'", m_CloudFileInfo.m_ContentLength);
	savegameDebugf1("LastModifiedPosixTime = '%lu'", m_CloudFileInfo.m_LastModifiedPosixTime);
	savegameDebugf1("           m_Location = '%s'", m_CloudFileInfo.m_Location);
	savegameDebugf1("               m_ETag = '%s'", m_CloudFileInfo.m_ETag);
	savegameDebugf1("      m_HaveSignature = '%s'", m_CloudFileInfo.m_HaveSignature ? "true":"false");
	savegameDebugf1("           m_HaveHash = '%s'", m_CloudFileInfo.m_HaveHash ? "true":"false");
}

// EOF
