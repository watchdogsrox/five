// 
// /savegame_datadict_script.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "commands_datafile.h"

// Standard Library headers
#include <memory>

// Rage headers
#include "data/rson.h"
#include "fwnet/netchannel.h"
#include "net/task.h"
#include "rline/cloud/rlcloud.h"
#include "script/wrapper.h"
#include "system/controlmgr.h"
#include "system/nelem.h"
#include "system/threadpool.h"

// Game headers
#include "event/EventNetwork.h"
#include "network/NetworkInterface.h"
#include "network/Cloud/CloudManager.h"
#include "network/Cloud/UserContentManager.h"
#include "SaveLoad/savegame_scriptdatafile.h"
#include "SaveLoad/savegame_photo_buffer.h"
#include "SaveLoad/savegame_photo_manager.h"
#include "script/script.h"
#include "script/script_channel.h"

NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, datafile, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_datafile

XPARAM(outputCloudFiles);
PARAM(validateCreateData, "Will validate JSON data we upload via create");
PARAM(disableDatafileIndexFixing, "[scriptcommands] Prevent natives that take a datafile index from setting it to 0 if it is invalid");

static sysThreadPool::Thread s_CloudFileThread;
static sysThreadPool s_CloudFileThreadPool;

using namespace rage;

// If we need to hang on to any file-specific info we can do it here. Technically not necessary right now but
// this is so we can get the types right
class sveFile : public sveDict 
{
public:
	static std::unique_ptr<sveFile> LoadFile(const char* buf, const unsigned bufLen, const char* BANK_ONLY(filename))
    {
        DIAG_CONTEXT_MESSAGE("Reading JSON document from %s", filename);

#if __BANK
		if(PARAM_outputCloudFiles.Get())
		{
			fiStream* pStream(ASSET.Create("cloud_LoadFile.log", ""));
			if (pStream)
			{
				pStream->Write(buf, bufLen);
				pStream->Close();
			}
		}
#endif

        if (!RsonReader::ValidateJson(buf, bufLen))
		{
			return nullptr;
		}
		RsonReader reader;
		if (!reader.Init(buf, 0, bufLen))
		{
			return nullptr;
		}

		return sveFile::ReadJsonFileValue(reader);
    }

	static std::unique_ptr<sveFile> LoadFile(fiStream* stream)
	{
		char* buf = rage_new char[stream->Size()];
		stream->Read(buf, stream->Size());

		auto file = sveFile::LoadFile(buf, stream->Size(), stream->GetName());

        delete[] buf;

        return file;
	}

	static std::unique_ptr<sveFile> LoadFile(const char* filename)
	{
		fiStream* stream = ASSET.Open(filename, "");
		if (!stream)
		{
			return nullptr;
		}
		auto newFile = sveFile::LoadFile(stream);
		stream->Close();

		return newFile;
	}

	static std::unique_ptr<sveFile> ReadJsonFileValue(RsonReader& reader)
	{
		std::unique_ptr<sveFile> newFile(rage_new sveFile);

		RsonReader child;

		if (!reader.GetFirstMember(&child))
		{
			return newFile;
		}

		do
		{
			char nameBuf[128];
			if (child.GetName(nameBuf, 128))
			{
				sveNode* childNode = sveNode::ReadJsonValue(child);
				if (childNode)
				{
					newFile->Insert(atFinalHashString(nameBuf), childNode);
				}
				else
				{
					return nullptr;
				}
			}
		}
		while(child.GetNextSibling(&child));

		return newFile;
	}
};

struct sFileRequest
{
	CloudRequestID m_CloudRequestID; 
	bool m_HasCompleted; 
	std::unique_ptr<sCloudFile> m_File;
	sFileRequest(CloudRequestID id) 
		: m_CloudRequestID(id)
		, m_HasCompleted(false)
	{}
};

atArray<CloudRequestID> g_CloudRequestsToWatch;
atArray<sFileRequest*> g_CloudFiles;
int g_ActiveFile = -1;

class netLoadCloudFileWorkItem : public sysThreadPool::WorkItem
{
public:

	netLoadCloudFileWorkItem()
		: m_CloudRequestID(INVALID_CLOUD_REQUEST_ID)
		, m_pData(nullptr)
		, m_DataSize(0)
		, m_pFileName(nullptr)
	{}

	virtual ~netLoadCloudFileWorkItem() {}

	bool Configure(CloudRequestID nCloudRequestID, void* pData, unsigned nDataSize, const char* szFileName)
	{
		m_CloudRequestID = nCloudRequestID;
		m_pData = pData;
		m_DataSize = nDataSize;
		m_pFileName = szFileName;

		gnetDebug1("netLoadCloudFileWorkItem::Configure :: Data: %p, Size: %u, Filename: %s", pData, nDataSize, szFileName);
		return true;
	}

	virtual void DoWork() 
	{
		gnetDebug1("netLoadCloudFileWorkItem :: Starting file load of %s (size: %u) for Watch Request %d", m_pFileName, m_DataSize, m_CloudRequestID);
		m_pCloudFile = sCloudFile::LoadFile(m_pData, m_DataSize, m_pFileName);
		gnetDebug1("netLoadCloudFileWorkItem :: Completed file load of %s for Watch Request %d. Succeeded: %s", m_pFileName, m_CloudRequestID, m_pCloudFile != NULL ? "True" : "False");
	}

	CloudRequestID m_CloudRequestID; 
	void* m_pData;
	unsigned m_DataSize;
	const char* m_pFileName;
	std::unique_ptr<sCloudFile> m_pCloudFile;
};

namespace datafile_commands
{
	class SveFileObject;
	std::unique_ptr<sCloudFile> LoadOfflineInternal(const char* szContentID);

	typedef int DatafileIndex;

	// Stores an sveObject and allows it to be cleaned up if game is restarted while it is still valid
	class SveFileObject : public CScriptOp
	{
	public:
		SveFileObject() : m_fileRequestId(INVALID_CLOUD_REQUEST_ID) {}

		bool SetFile(std::unique_ptr<sveDict> file)
		{
			if (!gnetVerifyf(m_file == nullptr, "SveFileObject :: SetFile: File already set!"))
			{
				gnetWarning("SveFileObject :: SetFile: File already set to %p (file request id %d)", m_file.get(), m_fileRequestId);
				return false;
			}

			m_file.swap(file);
			m_fileRequestId = INVALID_CLOUD_REQUEST_ID;
			CTheScripts::GetScriptOpList().AddOp(this);
			gnetDebug1("SveFileObject :: SetFile: Setting file to %p", m_file.get());
			return true;
		}

		bool SetFileFromFileRequest(sFileRequest& fileRequest)
		{
			if (IsSetFromFileRequest(fileRequest.m_CloudRequestID)) {
				// Double-sets are fine in this case.
				return true;
			}

			if (!gnetVerifyf(m_file == nullptr, "SveFileObject :: SetFile: File already set!")) {
				gnetWarning("SveFileObject :: SetFileFromRequest: File already set to %p (file request id %d)", m_file.get(), m_fileRequestId);
				return false;
			}

			SetFile(std::move(fileRequest.m_File));
			m_fileRequestId = fileRequest.m_CloudRequestID;
			gnetDebug1("SveFileObject :: SetFileFromFileRequest: File request id: %d", m_fileRequestId);
			return true;
		}

		bool IsSetFromFileRequest() const {
			return m_fileRequestId != INVALID_CLOUD_REQUEST_ID;
		}

		bool IsSetFromFileRequest(CloudRequestID id) const {
			return IsSetFromFileRequest() && m_fileRequestId == id;
		}

		CloudRequestID GetFileRequestId() const {
			return m_fileRequestId;
		}

		void ClearFile()
		{
			if (m_file.get())
			{
				gnetDebug1("SveFileObject :: ClearFile: %p, Request Id: %d", m_file.get(), m_fileRequestId);
				m_file.reset(nullptr);
				m_fileRequestId = INVALID_CLOUD_REQUEST_ID;
				CTheScripts::GetScriptOpList().RemoveOp(this);
			}
		}

		sveDict const* GetFile() const { return m_file.get(); }

		std::unique_ptr<sveDict> TakeFile()
		{
			if (gnetVerifyf(m_file != nullptr, "SveFileObject :: TakeFile: No file!"))
			{
				gnetDebug1("SveFileObject :: TakeFile: Releasing file %p, Request Id: %d", m_file.get(), m_fileRequestId);
				CTheScripts::GetScriptOpList().RemoveOp(this);
				m_fileRequestId = INVALID_CLOUD_REQUEST_ID;
				return std::move(m_file);
			}

			return nullptr;
		}

		void Flush() override
		{
			ClearFile();
		}

	private:
		std::unique_ptr<sveDict> m_file;
		CloudRequestID m_fileRequestId;
	};

	class SveFileObjectArray
	{
	public:
		static const s32 MAX_NUM_NORMAL_DATA_FILES = 4;
		static const s32 MAX_NUM_ADDITIONAL_DATA_FILES = 1;
		static const s32 MAX_NUM_DATA_FILES = MAX_NUM_NORMAL_DATA_FILES + MAX_NUM_ADDITIONAL_DATA_FILES;

		SveFileObject* GetFile(DatafileIndex index)
		{
			if (index < 0 || index >= MAX_NUM_DATA_FILES) {
				return nullptr;
			}

			return &m_SveFileObjects[index];
		}

		SveFileObject* GetFileByCloudRequest(CloudRequestID cloudRequestId)
		{
			for (SveFileObject& sveFileObj : m_SveFileObjects) {
				if (sveFileObj.IsSetFromFileRequest(cloudRequestId)) {
					return &sveFileObj;
				}
			}

			return nullptr;
		}

	private:
		atRangeArray<SveFileObject, MAX_NUM_DATA_FILES> m_SveFileObjects;
	};
	SveFileObjectArray g_SveFileObjects;
}

class netLoadUgcOfflineFileWorkItem : public netLoadCloudFileWorkItem
{
public:
	netLoadUgcOfflineFileWorkItem() : m_contentId(nullptr) {}

	bool Configure(CloudRequestID cloudRequestId, const char* contentId)
	{
		m_CloudRequestID = cloudRequestId;
		m_contentId = contentId;
		return true;
	}

	virtual void DoWork() override
	{
		gnetDebug1("netLoadCloudFileWorkItem :: Starting offline file load of %s (size: %u) for Watch Request %d", m_contentId, m_DataSize, m_CloudRequestID);
		m_pCloudFile = datafile_commands::LoadOfflineInternal(m_contentId);
		gnetDebug1("netLoadCloudFileWorkItem :: Completed offline file load of %s for Watch Request %d. Succeeded: %s", m_contentId, m_CloudRequestID, m_pCloudFile != NULL ? "True" : "False");
	}

private:
	const char* m_contentId;
};

class netLoadCloudFileTask : public netTask
{
public:
	NET_TASK_DECL(netLoadCloudFileTask);
	NET_TASK_USE_CHANNEL(net_datafile);

	netLoadCloudFileTask()
		: m_State(STATE_BEGIN_REQUEST)
		, m_pFileRequest(nullptr)
		, m_pData(nullptr)
		, m_DataSize(0)
		, m_WorkItem(nullptr)
	{
		m_CloudFilePath[0] = '\0';
	}

	virtual ~netLoadCloudFileTask()
	{
		CloudFree(m_pData);

		if (netTaskVerifyf(m_WorkItem, "Invalid work item"))
		{
			m_WorkItem->~netLoadCloudFileWorkItem();
			CloudFree(m_WorkItem);
		}
	}

	bool Configure(sFileRequest* pFileRequest, void* pData, unsigned nDataSize, const char* szFileName)
	{
		m_pFileRequest = pFileRequest;
		m_pData = CloudAllocate(nDataSize+1);
		m_DataSize = nDataSize;

		m_WorkItem = reinterpret_cast<netLoadCloudFileWorkItem*>(CloudAllocate(sizeof(netLoadCloudFileWorkItem)));
		new (m_WorkItem) netLoadCloudFileWorkItem();

		m_WorkItem->Configure(pFileRequest->m_CloudRequestID, pData, nDataSize, szFileName);

		if(!m_pData)
		{
			netTaskDebug("Failed to allocate data (Size: %u)", m_DataSize);
			return false;
		}
			
		static_cast<char*>(m_pData)[nDataSize] ='\0';
		memcpy(m_pData, pData, nDataSize);
		safecpy(m_CloudFilePath, szFileName);
		return true;
	}

	virtual netTaskStatus OnUpdate(int* /*resultCode*/)
	{
		netTaskStatus status = NET_TASKSTATUS_PENDING;

		switch(m_State)
		{
		case STATE_BEGIN_REQUEST:
			if(WasCanceled())
			{
				netTaskDebug("Canceled - bailing...");
				m_pFileRequest->m_HasCompleted = true;
				status = NET_TASKSTATUS_FAILED;
			}
			else if(CreateRequest())
			{
				netTaskDebug("Created request. Waiting...");
				m_State = STATE_WAIT_REQUEST;
			}
			else
			{
				netTaskDebug("Failed to create request");
				m_pFileRequest->m_HasCompleted = true;
				status = NET_TASKSTATUS_FAILED;
			}
			break;

		case STATE_WAIT_REQUEST:
			if(m_WorkItem->Finished())
			{
				if(WasCanceled())
				{
					netTaskDebug("Canceled - bailing...");
					m_pFileRequest->m_HasCompleted = true;
					status = NET_TASKSTATUS_FAILED;
				}
				else
				{
					netTaskDebug("Request completed");
					m_pFileRequest->m_File = std::move(m_WorkItem->m_pCloudFile);
					m_pFileRequest->m_HasCompleted = true;
					OnWorkItemFinished();
					status = NET_TASKSTATUS_SUCCEEDED;
				}
			}
		}

		return status;
	}

protected:

	//cloud file has been moved into the filerequest at this point
	virtual void OnWorkItemFinished() {}

	virtual bool CreateRequest()
	{
		if(!m_WorkItem->Configure(m_pFileRequest->m_CloudRequestID, m_pData, m_DataSize, m_CloudFilePath))
		{
			netTaskDebug("Failed to configure WorkItem");
			return false;
		}
		
		if(!s_CloudFileThreadPool.QueueWork(m_WorkItem))
		{
			netTaskDebug("Failed to queue WorkItem");
			return false;
		}
		return true;
	}

	virtual void OnCancel()
	{
		netTaskDebug("Canceled while in state %d", m_State);
		m_WorkItem->Cancel();
	}

protected:

	enum State
	{
		STATE_BEGIN_REQUEST,
		STATE_WAIT_REQUEST
	};

	State m_State;

	sFileRequest* m_pFileRequest;
	void* m_pData;
	unsigned m_DataSize;
	char m_CloudFilePath[RAGE_MAX_PATH];

	netLoadCloudFileWorkItem* m_WorkItem;
};

class netLoadUgcFileTask : public netLoadCloudFileTask
{
	NET_TASK_DECL(netLoadUgcFileTask);
	NET_TASK_USE_CHANNEL(net_datafile);

public:
	netLoadUgcFileTask() : m_contentId(nullptr), m_datafileIndex(-1) {}

	bool Configure(sFileRequest* pFileRequest, const char* contentId, const datafile_commands::DatafileIndex datafileIndex)
	{
		m_pFileRequest = pFileRequest;
		m_contentId = contentId;
		m_datafileIndex = datafileIndex;
		m_WorkItem = reinterpret_cast<netLoadUgcOfflineFileWorkItem*>(CloudAllocate(sizeof(netLoadUgcOfflineFileWorkItem)));
		new (m_WorkItem) netLoadUgcOfflineFileWorkItem();

		static_cast<netLoadUgcOfflineFileWorkItem*>(m_WorkItem)->Configure(pFileRequest->m_CloudRequestID, contentId);

		return true;
	}

protected:
	virtual void OnWorkItemFinished() override
	{
		if (m_pFileRequest->m_File)
		{
			using namespace datafile_commands;

			SveFileObject* fileObj = g_SveFileObjects.GetFile(m_datafileIndex);

			if (!fileObj)
			{
				netTaskWarning("OnWorkItemFinished :: File object not found for datafile index: %d", m_datafileIndex);
				return;
			}
			
			fileObj->SetFileFromFileRequest(*m_pFileRequest);
			netTaskDebug("OnWorkItemFinished :: Moved file from filerequest to fileobject. Datafile index: %d", m_datafileIndex);
		}
	}

	virtual bool CreateRequest() override
	{
		netLoadUgcOfflineFileWorkItem* workItem = static_cast<netLoadUgcOfflineFileWorkItem*>(m_WorkItem);
		if (!workItem->Configure(m_pFileRequest->m_CloudRequestID, m_contentId))
		{
			netTaskDebug("Failed to configure WorkItem");
			return false;
		}

		if (!s_CloudFileThreadPool.QueueWork(m_WorkItem))
		{
			netTaskDebug("Failed to queue WorkItem");
			return false;
		}
		return true;
	}

private:
	const char* m_contentId;
	datafile_commands::DatafileIndex m_datafileIndex;
};

class DatafileCloudWatcher : public CloudListener
{
	void OnCloudEvent(const CloudEvent* pEvent)
	{
		if(!pEvent)
		{
			gnetError("OnCloudEvent :: Invalid event!");
			return;
		}
			
		// we only care about requests
		if(pEvent->GetType() != CloudEvent::EVENT_REQUEST_FINISHED)
			return;

		// grab event data
		const CloudEvent::sRequestFinishedEvent* pEventData = pEvent->GetRequestFinishedData();
		if(!pEventData)
		{
			gnetError("OnCloudEvent :: Invalid event data!");
			return;
		}

		bool bWatching = false;

		// find out if we're interested in this file
		int nRequestsToWatch = g_CloudRequestsToWatch.GetCount();
		for(int i = 0; i < nRequestsToWatch; i++)
		{
			if(g_CloudRequestsToWatch[i] == pEventData->nRequestID)
			{
				bWatching = true;
				g_CloudRequestsToWatch.Delete(i);
				break;
			}
		}

		// not interested... bail out
		if(!bWatching)
			return;

		// wasn't successful... bail out
		if(!pEventData->bDidSucceed)
		{
			gnetError("OnCloudEvent :: Watch Request %d for %s failed", pEventData->nRequestID, pEventData->szFileName);
			return;
		}

		gnetDebug1("OnCloudEvent :: Watch Request %d for %s completed. Size: %d", pEventData->nRequestID, pEventData->szFileName, pEventData->nDataSize);

		// create a new file request
		sFileRequest* pRequest = _CloudAllocateAndConstruct(sFileRequest, pEventData->nRequestID);
		
		// uses streaming memory, make sure we have enough room
		if(!pRequest)
		{
			gnetError("OnCloudEvent :: Failed to allocate file request (Size: %d)", static_cast<int>(sizeof(sFileRequest)));
			return;
		}

		// add to tracking
		g_CloudFiles.PushAndGrow(pRequest);

		// create a task to load the file data
		netFireAndForgetTask<netLoadCloudFileTask>* task = nullptr;
		if(!netTask::Create(&task)	
				|| !task->Configure(pRequest, pEventData->pData, pEventData->nDataSize, pEventData->szFileName)
				|| !netTask::Run(task))	
		{
			gnetError("OnCloudEvent :: Failed to create or start netLoadCloudFileTask");
			
			// destroy the task
			netTask::Destroy(task);

			// if we couldn't create or start the task, we can consider this request completed (and failed)
			// this allows script to clean up on their side in good time
			pRequest->m_HasCompleted = true;
		}
	}
};

DatafileCloudWatcher* g_DatafileCloudWatcher = NULL;

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sveDict);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sveArray);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sveFile);
SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sCloudFile);

namespace datafile_commands 
{        
    SveFileObject g_MissionHeader;

	bool CheckDatafileIndex(DatafileIndex& index OUTPUT_ONLY(, const char* context)) {
		if (index < 0 || index >= SveFileObjectArray::MAX_NUM_DATA_FILES) {
			gnetAssertf(false, "Invalid datafile index %d passed to %s", index, context);
			gnetError("%s : Datafile index is invalid: %d", context, index);
			if (PARAM_disableDatafileIndexFixing.Get()) {
				return false;
			}
			gnetWarning(
				"Setting datafile index to 0. Use %s to disable this behaviour.",
				PARAM_disableDatafileIndexFixing.GetName());
			index = 0;
		}
		return true;
	}

#define _CheckDatafileIndex(index, context) CheckDatafileIndex(index OUTPUT_ONLY(, context))

	// Stores data related to an asyncronous load from cloud operation and allows it to be cleaned up if game is restarted in the middle of the process
	class LoadFromCloudOp : public CScriptOp
	{
	public:
		LoadFromCloudOp(sysMemAllocator* allocator) 
        : m_FileHandle(fiHandleInvalid)
        {
            m_GrowBuffer.Init(allocator, 0);
        }

		// PURPOSE: Start load from cloud operation
		// PARAMETERS:filename - name of file in the cloud
		// RETURNS: success
		bool Start(const char* pFilename, 
			const int localGamerIndex,
			const rlCloudMemberId& cloudMemberId) 
		{
			if (!rlRos::GetCredentials(localGamerIndex).IsValid())
			{
				return false;
			}

			fiDeviceCloud& cloudDevice = fiDeviceCloud::GetInstance();
			cloudDevice.SetAccessParameters(localGamerIndex, RL_CLOUD_NAMESPACE_MEMBERS, RL_CLOUD_ONLINE_SERVICE_NATIVE, cloudMemberId);
            m_FileHandle = cloudDevice.Open(pFilename, true);
            if(m_FileHandle != fiHandleInvalid)
			{
				m_nRetries = 0;
				m_cloudMemberId = cloudMemberId;
				snprintf(m_szFileName, kMAX_FILENAME, "%s", pFilename);

				CTheScripts::GetScriptOpList().AddOp(this);
				return true;
			}
			return false;
		}

		// PURPOSE: Return if operation is still in progress
		bool Pending() {return m_FileHandle != fiHandleInvalid;}

		// PURPOSE: Update state of operation
		// RETURNS: returns false if is has been unsuccessful
		bool Update(DatafileIndex datafileToStoreResultIn) 
		{
			fiDeviceCloud& cloudDevice = fiDeviceCloud::GetInstance();
			
            //Read a chunk into our grow buffer
            //Poll at the same speed as files are downloaded
            u8 readBuffer[rlCloud::s_FileReadSize];
            const int amountRead = cloudDevice.Read(m_FileHandle, readBuffer, COUNTOF(readBuffer));
            
            //Append what we read to our grow buffer
            if (amountRead < 0 
                || !m_GrowBuffer.AppendOrFail(readBuffer, amountRead))
            {
                // If we failed to append the data, or the read failed, then we're done
                Flush();
                return false;
            }

            // wait for all the data to load
			if(cloudDevice.Pending(m_FileHandle))
			{
				return true;
			}

			bool rt = cloudDevice.Succeeded(m_FileHandle);
			if(rt)
			{
				std::unique_ptr<sveFile> file = sveFile::LoadFile((const char*)m_GrowBuffer.GetBuffer(), m_GrowBuffer.Length(), m_szFileName);
				g_SveFileObjects.GetFile(datafileToStoreResultIn)->SetFile(std::move(file));
			}

			Flush();
			return rt;
		}

		// PURPOSE: Force the operation to finish
		void Flush() 
		{
            if(m_FileHandle != fiHandleInvalid)
			{
                fiDeviceCloud::GetInstance().Close(m_FileHandle);
                m_FileHandle = fiHandleInvalid;
				CTheScripts::GetScriptOpList().RemoveOp(this);
			}
            // Get rid of the contents of our grow buffer now that we're done
            m_GrowBuffer.Clear();
			m_nRetries = 0;
		}

	private:
		static const unsigned kMAX_FILENAME = 128;
		static const unsigned kMAX_RETRIES = 3;

		int m_nRetries;
		char m_szFileName[kMAX_FILENAME];
		rlCloudMemberId m_cloudMemberId;
        //The grow buffer we read the cloud file into
        datGrowBuffer m_GrowBuffer;
        fiHandle m_FileHandle;

	} g_LoadFromCloudOp(NULL);

	// Stores data related to an asynchronous save to cloud operation and allows it to be cleaned up if game is restarted in the middle of the process
	class SaveToCloudOp : public CScriptOp
	{
	public:
		SaveToCloudOp() : m_pStream(NULL) {}

		// PURPOSE: Start save to cloud operation
		// PARAMETERS:	filename - name of file in the cloud
		//				file - sveFile structure to save
		// RETURNS: success
		bool Start(const char* filename, sveDict const* file) 
		{
			Assert(m_pStream == NULL);
			int localPLayerIndex = CControlMgr::GetMainPlayerIndex();
			if (!rlRos::GetCredentials(localPLayerIndex).IsValid())
			{
				return false;
			}

			fiDeviceCloud& cloudDevice = fiDeviceCloud::GetInstance();
			cloudDevice.SetAccessParameters(localPLayerIndex, RL_CLOUD_NAMESPACE_MEMBERS, RL_CLOUD_ONLINE_SERVICE_NATIVE);
			m_pStream = fiStream::Create(filename, cloudDevice);
			if(m_pStream)
			{
				file->WriteJson(*m_pStream, 0, false);
				m_pStream->Flush();
#if __BANK
				if(PARAM_outputCloudFiles.Get())
				{
					fiStream* pStream(ASSET.Create("cloud_SaveToCloudOp.log", ""));
					if (pStream)
					{
						file->WriteJson(*pStream, 0, true);
						pStream->Flush();
						pStream->Close();
					}
				}
#endif
				fiHandle handle = m_pStream->GetLocalHandle();
				cloudDevice.Commit(handle);

				CTheScripts::GetScriptOpList().AddOp(this);
				return true;
			}
			return false;
		}

		// PURPOSE: Return if operation is still in progress
		bool Pending() {return (m_pStream != NULL);}
		// PURPOSE: Update state of operation
		// RETURNS: returns false if is has been unsuccessful
		bool Update() {
			bool rt = true;
			Assert(m_pStream);
			fiDeviceCloud& cloudDevice = fiDeviceCloud::GetInstance();
			fiHandle handle = m_pStream->GetLocalHandle();
			if(!cloudDevice.Pending(handle))
			{
				rt = cloudDevice.Succeeded(handle);
				m_pStream->Close();
				m_pStream = NULL;
				CTheScripts::GetScriptOpList().RemoveOp(this);
			}
			return rt;
		}
		// PURPOSE: Force the operation to finish
		void Flush() {
			if(m_pStream)
			{
				m_pStream->Close();
				m_pStream = NULL;
				CTheScripts::GetScriptOpList().RemoveOp(this);
			}
		}

	private:
		fiStream* m_pStream;

	} g_SaveToCloudOp;

	// Stores data related to an asynchronous delete from cloud operation and allows it to be cleaned up if game is restarted in the middle of the process
	class DeleteFromCloudOp : public CScriptOp
	{
	public:
		DeleteFromCloudOp() { m_Status.Reset(); m_bAdded = false; }

		// PURPOSE: Start delete from cloud operation
		// PARAMETERS: filename - name of file in the cloud
		// RETURNS: success
		bool Start(const char* filename) 
		{
			gnetDebug1("DeleteFromCloudOp :: Started");

			int localPlayerIndex = CControlMgr::GetMainPlayerIndex();
			if (!rlRos::GetCredentials(localPlayerIndex).IsValid())
			{
				return false;
			}

			if(rlCloud::DeleteMemberFile(localPlayerIndex, RL_CLOUD_ONLINE_SERVICE_NATIVE, filename, NULL, &m_Status))
			{
				if(!m_bAdded)
					CTheScripts::GetScriptOpList().AddOp(this);

				m_bAdded = true; 
				return true;
			}
			return false;
		}

		// PURPOSE: Return if operation is still in progress
		bool Pending() { return m_Status.Pending(); }

		// PURPOSE: Update state of operation
		// RETURNS: returns false if is has been unsuccessful
		bool Update() {
			bool bSucceeded = true;
			if(!m_Status.Pending())
			{
				gnetDebug1("DeleteFromCloudOp :: Completed. Success: %s", m_Status.Succeeded() ? "True" : "False");
				bSucceeded = m_Status.Succeeded();
				m_Status.Reset(); 
				CTheScripts::GetScriptOpList().RemoveOp(this);
				m_bAdded = false; 
			}
			return bSucceeded;
		}
		// PURPOSE: Force the operation to finish
		void Flush() {
			if(m_Status.Pending())
			{
				
			}
			gnetDebug1("DeleteFromCloudOp :: Flushed.");
			CTheScripts::GetScriptOpList().RemoveOp(this);
			m_bAdded = false; 
		}

	private:
		bool m_bAdded; 
		netStatus m_Status;

	} g_DeleteFromCloudOp;

	//////////////////////////////////////////////////////////////////////////
	// File level operations
	void CommandDatafileWatchRequestID(int nCloudRequestID)
	{
		// create a cloud watcher 
		if(!g_DatafileCloudWatcher)
			g_DatafileCloudWatcher = rage_new DatafileCloudWatcher();

		gnetDebug1("CommandDatafileWatchRequestID :: Watch request added for %d", nCloudRequestID);
		g_CloudRequestsToWatch.PushAndGrow(nCloudRequestID);
	}

	void CommandDatafileClearWatchList()
	{
		gnetDebug1("CommandDatafileClearWatchList");
		g_CloudRequestsToWatch.Reset();
	}

	bool CommandDatafileIsValidRequestID(int nCloudRequestID)
	{
		// check active cloud files for this request ID
		int nCloudFiles = g_CloudFiles.GetCount();
		for(int i = 0; i < nCloudFiles; i++)
		{
			if(g_CloudFiles[i]->m_CloudRequestID == nCloudRequestID)
				return true;
		}

		// invalid
		return false; 
	}

	bool CommandDatafileHasLoadedFileData(int nCloudRequestID)
	{
		// check active cloud files for this request ID
		int nCloudFiles = g_CloudFiles.GetCount();
		for(int i = 0; i < nCloudFiles; i++)
		{
			if(g_CloudFiles[i]->m_CloudRequestID == nCloudRequestID)
			{
				if(g_CloudFiles[i]->m_HasCompleted)
					return true;
			}
		}

		// invalid
		return false; 
	}

	bool HasValidFileData(sFileRequest const& fileRequest, SveFileObject const* sveFileObject) {
		if (sveFileObject != nullptr &&
			sveFileObject->IsSetFromFileRequest(fileRequest.m_CloudRequestID))
		{
			// CommandDatafileSelectActiveFile was used to set this SveFileObject.
			// That is, the ownership of the file data has been transferred from the sFileRequest to
			// the SveFileObject.
			return sveFileObject->GetFile() != nullptr;
		}

		return fileRequest.m_File != nullptr;
	}

	bool CommandDatafileHasValidFileData(int nCloudRequestID)
	{
		// check active cloud files for this request ID
		for(auto const& cloudFile : g_CloudFiles)
		{
			if (cloudFile->m_CloudRequestID == nCloudRequestID)
		{
				return HasValidFileData(
					*cloudFile, 
					g_SveFileObjects.GetFileByCloudRequest(nCloudRequestID));
			}
		}

		// invalid ID
		return false; 
	}

	bool CommandDatafileSelectActiveFile(int nCloudRequestID, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_SELECT_ACTIVE_FILE")) {
			return false;
		}

		gnetDebug1("CommandDatafileSelectActiveFile :: WatchRequestId: %d, datafileIndex: %d", nCloudRequestID, datafileIndex);

		SveFileObject* sveFileObject = g_SveFileObjects.GetFile(datafileIndex);

		// check active cloud files for this request ID
		for(sFileRequest* fileRequest : g_CloudFiles)
		{
			if(fileRequest->m_CloudRequestID == nCloudRequestID)
			{
				if (!gnetVerifyf(HasValidFileData(*fileRequest, sveFileObject), "CommandDatafileSelectActiveFile :: File data invalid for %d. Check with DATAFILE_HAS_VALID_FILE_DATA", nCloudRequestID))
				{
					// File was not loaded, or loaded incorrectly, or some weirdness has gone on with the SveFileObject.
					gnetDebug1("CommandDatafileSelectActiveFile :: File data invalid for %d. Check with DATAFILE_HAS_VALID_FILE_DATA", nCloudRequestID);
                    return false;
				}

				return sveFileObject->SetFileFromFileRequest(*fileRequest);
			}
		}

		gnetError("CommandDatafileSelectActiveFile :: Watch request not found for %d", nCloudRequestID);

		return false;
	}

	bool CommandDatafileDeleteFile(int nCloudRequestID)
	{
		gnetDebug1("CommandDatafileDeleteFile :: Cloud Request Id - %d", nCloudRequestID);

		// check active cloud files for this request ID
		for(int i = 0; i < (int)g_CloudFiles.size(); i++)
		{
			sFileRequest* pRequest = g_CloudFiles[i];

			if(pRequest->m_CloudRequestID == nCloudRequestID)
			{
				if(gnetVerifyf(pRequest->m_HasCompleted, "CommandDatafileDeleteFile :: Cannot delete request which has not completed yet!"))
				{
					SveFileObject* fileObj = g_SveFileObjects.GetFileByCloudRequest(nCloudRequestID);
					if (fileObj != nullptr)
					{
						fileObj->ClearFile();
					}

					CloudSafeDelete(pRequest);
					g_CloudFiles.Delete(i);

					return true;
				}
			}
		}

		return false;
	}

	struct FileData
	{
		std::unique_ptr<char[]> data;
		unsigned dataSize;

		FileData() : dataSize(0) {}
		FileData(FileData&& other) { data = std::move(other.data); dataSize = other.dataSize; }
	private:
		FileData(FileData const&);
	};

	bool IsValid(FileData const& fileData)
	{
		return fileData.data != nullptr && fileData.dataSize > 0;
	}

	static FileData ReadSveFileObject(SveFileObject& pObject, const char* pStreamName, bool prettyPrint = false)
	{
		if(!pObject.GetFile())
			return FileData();

		// create stream to count JSON file size
		fiStream* pCountStream = ASSET.Create("count:", "");

		// write the JSON data into the stream - call flush to update
		pObject.GetFile()->WriteJson(*pCountStream, 0, prettyPrint);
		pCountStream->Flush(); 

		// create data
		unsigned nDataSize = pCountStream->Size();
		if(!gnetVerifyf(nDataSize > 0, "Data dictionary empty!"))
		{
			pCountStream->Close();
			return FileData();
		}

		// allocate data to read the file
		std::unique_ptr<char[]> pData(rage_new char[nDataSize + 1]);

		// close the counter stream
		pCountStream->Close(); 

		// make memory file name
		char szMemFileName[RAGE_MAX_PATH];
		fiDevice::MakeMemoryFileName(szMemFileName, RAGE_MAX_PATH, pData.get(), nDataSize, false, pStreamName);

		// open stream for memory
		fiStream* pStream = ASSET.Create(szMemFileName, "");

		// write the JSON, flush and close. Data is freed by calling function.
		pObject.GetFile()->WriteJson(*pStream, 0, prettyPrint);
		pStream->Flush();
		pStream->Close();

		// string must be null terminated
		pData[nDataSize] = '\0';

		FileData file;
		file.data = std::move(pData);
		file.dataSize = nDataSize;
		return file;
	}

	static const char* ReadSveFileObject(SveFileObject* pObject, unsigned* pDataSize, const char* pStreamName)
	{
		if (!pObject)
			return nullptr;

		FileData file = ReadSveFileObject(*pObject, pStreamName);

		if (file.data == nullptr)
			return nullptr;

		if (pDataSize)
			*pDataSize = file.dataSize;

		return file.data.release();
	}

    // script text label size for cloud paths
    static const unsigned CLOUD_PATH_TEXT_LABEL_SIZE = 64;
	static const unsigned UGC_MAX_FILES = 2;

    bool CommandDatafileCreateUGC(int& hPathData, int nFiles, const char* szDisplayName, int& descriptionStruct, int& tagsStruct, const char* szContentType, bool bPublish, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "UGC2_CREATE_CONTENT")) {
			return false;
		}

		// check that we aren't already publishing
		if(!Verifyf(!UserContentManager::GetInstance().IsCreatePending(), "CreateUGC :: Operation pending!"))
			return false;

		// check that we have valid header data
		if(!Verifyf(g_SveFileObjects.GetFile(datafileIndex)->GetFile(), "CreateUGC :: Dictionary pointer invalid"))
			return false;

		// check that the content type string is valid
		rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
		if(!Verifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "CreateUGC :: Invalid content string %s", szContentType))
			return false;

        unsigned nValidFiles = 0;
        
        // set up our source file info
        rlUgc::SourceFileInfo* srcFiles = NULL;
        if(nFiles > 0)
        {
            if(NULL == (srcFiles = Alloca(rlUgc::SourceFileInfo, nFiles)))
            {
                gnetError("CreateUGC :: Failed to alloc src files buffer");
                return false;
            }

            // buffer to read from, skip the start of the content array
            char* pDataBuffer = reinterpret_cast<char*>(&hPathData);
            pDataBuffer += sizeof(scrValue);

			int aFileIDs[UGC_MAX_FILES] = { eFile_Mission, eFile_HiResPhoto, };

            // read in each path
            for(int i = 0; i < nFiles; i++)
            {
                // read in path from script
                char szPath[RLUGC_MAX_CLOUD_ABS_PATH_CHARS];
                formatf(szPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS, pDataBuffer);
                pDataBuffer += CLOUD_PATH_TEXT_LABEL_SIZE;
                
                // convert to absolute path
                if(CUserContentManager::GetAbsPathFromRelPath(szPath, srcFiles[nValidFiles].m_CloudAbsPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS))
                {
                    // allocate file ID
                    srcFiles[nValidFiles].m_FileId = aFileIDs[i];
                    nValidFiles++;
                }
            }
        }

        // updated dataJSON is optional
        const char* pData = ReadSveFileObject(g_SveFileObjects.GetFile(datafileIndex), 0, "CreateUGC");

        // make sure name is valid
        char szName[RLUGC_MAX_CONTENT_NAME_CHARS];
        formatf(szName, szDisplayName);
        szName[RLUGC_MAX_CONTENT_NAME_CHARS - 1] = '\0';

		// read in description & tags string
		char szDesc[CUserContentManager::MAX_DESCRIPTION];
		char szTags[CUserContentManager::MAX_DESCRIPTION];

		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)descriptionStruct, szDesc, CUserContentManager::MAX_DESCRIPTION);
		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)tagsStruct, szTags, CUserContentManager::MAX_DESCRIPTION);

		// call into UGC
		bool bSuccess = UserContentManager::GetInstance().Create(szDisplayName, szDesc, srcFiles, nValidFiles, szTags, pData, kType, bPublish);

		// punt data
		delete[] pData; 

		// return whether the call succeeded or not
		return bSuccess;
	}

    bool CommandDatafileCreateMissionUGC(const char* szDisplayName, int& descriptionStruct, int& tagsStruct, const char* szContentType, bool bPublish, DatafileIndex datafileIndex)
    {
		if (!_CheckDatafileIndex(datafileIndex, "UGC2_CREATE_CONTENT")) {
			return false;
		}

        // check that we aren't already publishing
        if(!Verifyf(!UserContentManager::GetInstance().IsCreatePending(), "CreateMissionUGC :: Operation pending!"))
            return false;

        // check that we have valid mission data
        if(!Verifyf(g_MissionHeader.GetFile(), "CreateMissionUGC :: Mission header (data JSON) invalid"))
            return false;

        // check that we have valid mission data
        if(!Verifyf(g_SveFileObjects.GetFile(datafileIndex), "CreateMissionUGC :: Dictionary pointer invalid"))
            return false;

        // check that the content type string is valid
        rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
        if(!Verifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "CreateMissionUGC :: Invalid content string %s", szContentType))
            return false;

        // file array
		rlUgc::SourceFile aUgcFiles[UGC_MAX_FILES];
        s32 nFiles = 0;

        // for data arrays
        unsigned nDataSize = 0;

        // first item is mission content
        const char* pData = ReadSveFileObject(g_SveFileObjects.GetFile(datafileIndex), &nDataSize, "CreateMissionData");
        
        // validation
        if(!Verifyf(pData, "CreateMissionUGC :: Invalid data generated from mission data"))
            return false;
        if(!Verifyf(nDataSize > 0, "CreateMissionUGC :: Zero length generated from mission data"))
            return false;

#if __BANK
		// validate the JSON file
		bool bWriteCloudFile = false;
		if(PARAM_validateCreateData.Get())
		{
			if(!gnetVerifyf(RsonReader::ValidateJson(static_cast<const char*>(pData), nDataSize), "CreateMissionUGC :: Error parsing data for %s. Include cloud_CreateMissionData.log in the logs", szDisplayName))
			{
				gnetError("CreateMissionUGC :: Error parsing data for %s. Include cloud_CreateMissionData.log in the logs", szDisplayName);
				bWriteCloudFile = true; 
			}
		}

        if(bWriteCloudFile || PARAM_outputCloudFiles.Get())
        {
            fiStream* pStream(ASSET.Create("cloud_CreateMissionData.log", ""));
            if(pStream)
            {
                pStream->Write(pData, nDataSize);
                pStream->Close();
            }
        }
#endif

        aUgcFiles[nFiles] = rlUgc::SourceFile(eFile_Mission, reinterpret_cast<const u8*>(pData), static_cast<s32>(nDataSize));
        nFiles++;

        // second item is photo
        CPhotoBuffer* pScreenShot = CPhotoManager::GetMissionCreatorPhoto();
        if(pScreenShot && pScreenShot->BufferIsAllocated() && pScreenShot->GetExactSizeOfJpegData() > 0)
        {
            aUgcFiles[nFiles] = rlUgc::SourceFile(eFile_HiResPhoto, pScreenShot->GetJpegBuffer(), pScreenShot->GetExactSizeOfJpegData());
            nFiles++;
        }

        // updated dataJSON is optional
        const char* pHeader = ReadSveFileObject(&g_MissionHeader, &nDataSize, "CreateMissionHeader");

#if __BANK
		// validate the JSON file
		bWriteCloudFile = false;
		if(PARAM_validateCreateData.Get() && nDataSize > 0)
		{
			if(!gnetVerifyf(RsonReader::ValidateJson(static_cast<const char*>(pHeader), nDataSize), "CreateMissionUGC :: Error parsing data for %s. Include cloud_CreateMissionHeader.log in the logs", szDisplayName))
			{
				gnetError("CreateMissionUGC :: Error parsing data for %s. Include cloud_CreateMissionHeader.log in the logs", szDisplayName);
				bWriteCloudFile = true; 
			}
		}

		if((bWriteCloudFile || PARAM_outputCloudFiles.Get()) && nDataSize > 0)
        {
            fiStream* pStream(ASSET.Create("cloud_CreateMissionHeader.log", ""));
            if(pStream)
            {
                pStream->Write(pHeader, nDataSize);
                pStream->Close();
            }
        }
#endif

        // make sure name is valid
        char szName[RLUGC_MAX_CONTENT_NAME_CHARS];
        formatf(szName, szDisplayName);
        szName[RLUGC_MAX_CONTENT_NAME_CHARS - 1] = '\0';

        // read in description & tags string
		char szDesc[CUserContentManager::MAX_DESCRIPTION];
		char szTags[CUserContentManager::MAX_DESCRIPTION];

		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)descriptionStruct, szDesc, CUserContentManager::MAX_DESCRIPTION);
		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)tagsStruct, szTags, CUserContentManager::MAX_DESCRIPTION);

        // call into UGC
        bool bSuccess = UserContentManager::GetInstance().Create(szDisplayName, szDesc, aUgcFiles, nFiles, szTags, pHeader, kType, bPublish);

        // punt mission header
        if(g_MissionHeader.GetFile())
            g_MissionHeader.ClearFile();

        // punt data
        delete[] pData; 
        delete[] pHeader; 

        // return whether the call succeeded or not
        return bSuccess;
    }

    bool CommandDatafileUpdateUGC(const char* szContentID, int& hPathData, int nFiles, const char* szDisplayName, int& descriptionStruct, int& tagsStruct, const char* szContentType, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "UGC_UPDATE_CONTENT"))
			return false;

		// check that we aren't already modifying
		if(!Verifyf(!UserContentManager::GetInstance().IsModifyPending(), "UpdateUGC :: Operation pending!"))
			return false;

		// check that the content type string is valid
		rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
		if(!Verifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "UpdateUGC :: Invalid content string %s", szContentType))
			return false;

		// updated dataJSON is optional
        const char* pData = ReadSveFileObject(g_SveFileObjects.GetFile(datafileIndex), NULL, "UpdateUGC");

        unsigned nValidFiles = 0;

        // set up our source file info
        rlUgc::SourceFileInfo* srcFiles = NULL;
        if(nFiles > 0)
        {
            if(NULL == (srcFiles = Alloca(rlUgc::SourceFileInfo, nFiles)))
            {
                gnetError("UpdateUGC :: Failed to alloc src files buffer");
                return false;
            }

            // buffer to read from, skip the start of the content array
            char* pDataBuffer = reinterpret_cast<char*>(&hPathData);
            pDataBuffer += sizeof(scrValue);

			int aFileIDs[UGC_MAX_FILES] = { eFile_Mission, eFile_HiResPhoto, };

            // read in each path
            for(int i = 0; i < nFiles; i++)
            {
                // read in path from script
                char szPath[RLUGC_MAX_CLOUD_ABS_PATH_CHARS];
                formatf(szPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS, pDataBuffer);
                pDataBuffer += CLOUD_PATH_TEXT_LABEL_SIZE;

                // convert to absolute path
                if(CUserContentManager::GetAbsPathFromRelPath(szPath, srcFiles[nValidFiles].m_CloudAbsPath, RLUGC_MAX_CLOUD_ABS_PATH_CHARS))
                {
                    // allocate file ID
                    srcFiles[nValidFiles].m_FileId = aFileIDs[i];
                    nValidFiles++;
                }
            }
        }

        // make sure name is valid
        char szName[RLUGC_MAX_CONTENT_NAME_CHARS];
        formatf(szName, szDisplayName);
        szName[RLUGC_MAX_CONTENT_NAME_CHARS - 1] = '\0';

		// read in description & tags string
		char szDesc[CUserContentManager::MAX_DESCRIPTION];
		char szTags[CUserContentManager::MAX_DESCRIPTION];

		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)descriptionStruct, szDesc, CUserContentManager::MAX_DESCRIPTION);
		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)tagsStruct, szTags, CUserContentManager::MAX_DESCRIPTION);

  		// call into UGC
		bool bSuccess = UserContentManager::GetInstance().UpdateContent(szContentID, szName, szDesc, pData, srcFiles, nValidFiles, szTags, kType);

		// punt data
		delete[] pData; 

		// return whether the call succeeded or not
		return bSuccess;
	}

    bool CommandDatafileUpdateMissionUGC(const char* szContentID, const char* szDisplayName, int& descriptionStruct, int& tagsStruct, const char* szContentType, DatafileIndex datafileIndex)
    {
		if (!_CheckDatafileIndex(datafileIndex, "UGC_UPDATE_MISSION"))
			return false;

        // check that we aren't already modifying
        if(!Verifyf(!UserContentManager::GetInstance().IsModifyPending(), "UpdateMissionUGC :: Operation pending!"))
            return false;

        // check that the content type string is valid
        rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
        if(!Verifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "UpdateMissionUGC :: Invalid content string %s", szContentType))
            return false;

        // file array
        static const unsigned UGC_MAX_FILES = 2;
        rlUgc::SourceFile aUgcFiles[UGC_MAX_FILES];
        s32 nFiles = 0;

        // for data arrays
        unsigned nDataSize = 0;

        // first item is mission content
        const char* pData = ReadSveFileObject(g_SveFileObjects.GetFile(datafileIndex), &nDataSize, "UpdateMissionData");
        if(pData && nDataSize > 0)
        {
            aUgcFiles[nFiles] = rlUgc::SourceFile(eFile_Mission, reinterpret_cast<const u8*>(pData), static_cast<s32>(strlen(pData)));
            nFiles++;
        }

#if __BANK
		// validate the JSON file
		bool bWriteCloudFile = false;
		if(PARAM_validateCreateData.Get() && nDataSize > 0)
		{
			if(!gnetVerifyf(RsonReader::ValidateJson(static_cast<const char*>(pData), nDataSize), "UpdateMissionUGC :: Error parsing data for %s. Include cloud_UpdateMissionData.log in the logs", szDisplayName))
			{
				gnetError("UpdateMissionUGC :: Error parsing data for %s. Include cloud_UpdateMissionData.log in the logs", szDisplayName);
				bWriteCloudFile = true; 
			}
		}

		if((bWriteCloudFile || PARAM_outputCloudFiles.Get() )&& nDataSize > 0)
        {
			fiStream* pStream(ASSET.Create("cloud_UpdateMissionData.log", ""));
            if(pStream)
            {
                pStream->Write(pData, nDataSize);
                pStream->Close();
            }
        }
#endif

        // second item is photo
        CPhotoBuffer* pScreenShot = CPhotoManager::GetMissionCreatorPhoto();
        if(pScreenShot && pScreenShot->GetJpegBuffer() && pScreenShot->GetExactSizeOfJpegData() > 0)
        {
            aUgcFiles[nFiles] = rlUgc::SourceFile(eFile_HiResPhoto, pScreenShot->GetJpegBuffer(), pScreenShot->GetExactSizeOfJpegData());
            nFiles++;
        }

        // updated dataJSON is optional
        const char* pHeader = ReadSveFileObject(&g_MissionHeader, &nDataSize, "UpdateMissionHeader");

#if __BANK
		// validate the JSON file
		bWriteCloudFile = false;
		if(PARAM_validateCreateData.Get() && nDataSize > 0)
		{
			if(!gnetVerifyf(RsonReader::ValidateJson(static_cast<const char*>(pHeader), nDataSize), "UpdateMissionUGC :: Error parsing data for %s. Include cloud_UpdateMissionHeader.log in the logs", szDisplayName))
			{
				gnetError("UpdateMissionUGC :: Error parsing data for %s. Include cloud_UpdateMissionHeader.log in the logs", szDisplayName);
				bWriteCloudFile = true; 
			}
		}

		if((bWriteCloudFile || PARAM_outputCloudFiles.Get()) && nDataSize > 0)
        {
            fiStream* pStream(ASSET.Create("cloud_UpdateMissionHeader.log", ""));
            if(pStream)
            {
                pStream->Write(pHeader, nDataSize);
                pStream->Close();
            }
        }
#endif

        // make sure name is valid
        char szName[RLUGC_MAX_CONTENT_NAME_CHARS];
        formatf(szName, szDisplayName);
        szName[RLUGC_MAX_CONTENT_NAME_CHARS - 1] = '\0';

		// read in description & tags string
		char szDesc[CUserContentManager::MAX_DESCRIPTION];
		char szTags[CUserContentManager::MAX_DESCRIPTION];

		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)descriptionStruct, szDesc, CUserContentManager::MAX_DESCRIPTION);
		UserContentManager::GetInstance().GetStringFromScriptDescription((CUserContentManager::sScriptDescriptionStruct&)tagsStruct, szTags, CUserContentManager::MAX_DESCRIPTION);

        // call into UGC
        bool bSuccess = UserContentManager::GetInstance().UpdateContent(szContentID, szName, szDesc, pHeader, aUgcFiles, nFiles, szTags, kType);

        // punt mission header
        if(g_MissionHeader.GetFile())
            g_MissionHeader.ClearFile();

        // punt data
        delete[] pData; 
        delete[] pHeader;

        // return whether the call succeeded or not
        return bSuccess;
    }

    bool CommandDatafileUGCSetPlayerData(const char* szContentID, float fRating, const char* szContentType, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "UGC_SET_PLAYER_DATA"))
			return false;

		// check that we aren't already modifying
		if(!Verifyf(!UserContentManager::GetInstance().IsModifyPending(), "SetPlayerDataUGC :: Operation pending!"))
			return false;

		// check that the content type string is valid
		rlUgcContentType kType = rlUgcContentTypeFromString(szContentType);
		if(!Verifyf(kType != RLUGC_CONTENT_TYPE_UNKNOWN, "SetPlayerDataUGC :: Invalid content string %s", szContentType))
			return false;

		// updated dataJSON is optional
        const char* pData = ReadSveFileObject(g_SveFileObjects.GetFile(datafileIndex), NULL, "SetPlayerData");

		// treat any negative rating as no rating
        float* pRating = NULL;
        if(fRating >= 0)
            pRating = &fRating;

        // call into UGC
        bool bSuccess = UserContentManager::GetInstance().SetPlayerData(szContentID, pData, pRating, kType);

		// punt data
		delete[] pData; 

		// return whether the call succeeded or not
		return bSuccess;
	}

	void CheckValidation(const char* pData, unsigned* pDataSize)
	{
		// validation does not appreciate a null terminator
		if(pData[*pDataSize - 1] == '\0')
			*pDataSize -= 1;

		// sometimes, the closing ']' CDATA blocks are included in the data size
		while(pData[*pDataSize - 1] == ']')
			*pDataSize -= 1;
	}

	int CommandDatafileGetUGCDataSize(int nContentIndex)
	{
		unsigned nDataSize = UserContentManager::GetInstance().GetContentDataSize(nContentIndex);
		return static_cast<int>(nDataSize);
	}

	bool CommandDatafileSelectUGCData(int nContentIndex, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_SELECT_UGC_DATA"))
			return false;

		// grab UGC data
		const char* pData = UserContentManager::GetInstance().GetContentData(nContentIndex);
		unsigned nDataSize = UserContentManager::GetInstance().GetContentDataSize(nContentIndex);

		// check that we have content
		if(!(pData && nDataSize > 0))
			return false;

		// development validation errors
		CheckValidation(pData, &nDataSize);

		// load file - we are given a null terminator and the validation does not approve of this
		// chop down the size by 1
		std::unique_ptr<sCloudFile> pFile = sCloudFile::LoadFile(pData, nDataSize, "UGC Data");
		if(!pFile)
			return false;

		// apply file
		g_SveFileObjects.GetFile(datafileIndex)->SetFile(std::move(pFile));

		// success
		return true;
	}

	int CommandDatafileGetUGCStatsSize(int nContentIndex, bool UNUSED_PARAM(bXv))
	{
		unsigned nDataSize = UserContentManager::GetInstance().GetContentStatsSize(nContentIndex);
		return static_cast<int>(nDataSize);
	}

	bool CommandDatafileSelectUGCStats(int nContentIndex, bool UNUSED_PARAM(bXv), DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_SELECT_UGC_STATS"))
			return false;

		// grab UGC data
		const char* pData = UserContentManager::GetInstance().GetContentStats(nContentIndex);
		unsigned nDataSize = UserContentManager::GetInstance().GetContentStatsSize(nContentIndex);

		// check that we have content
		if(!(pData && nDataSize > 0))
			return false;

		// development validation errors
		CheckValidation(pData, &nDataSize);

		// load file - we are given a null terminator and the validation does not approve of this
		// chop down the size by 1
		std::unique_ptr<sCloudFile> pFile = sCloudFile::LoadFile(pData, nDataSize, "UGC Stats");
		if(!pFile)
			return false;

		// apply file
		g_SveFileObjects.GetFile(datafileIndex)->SetFile(std::move(pFile));

		// success
		return true;
	}

	int CommandDatafileGetUGCPlayerDataSize(int nContentIndex)
	{
		unsigned nDataSize = UserContentManager::GetInstance().GetPlayerDataSize(nContentIndex);
		return static_cast<int>(nDataSize);
	}

	bool CommandDatafileSelectUGCPlayerData(int nContentIndex, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_SELECT_UGC_PLAYER_DATA"))
			return false;

		// grab UGC data
		const char* pData = UserContentManager::GetInstance().GetPlayerData(nContentIndex);
		unsigned nDataSize = UserContentManager::GetInstance().GetPlayerDataSize(nContentIndex);

		// check that we have content
		if(!(pData && nDataSize > 0))
			return false;

		// development validation errors
		CheckValidation(pData, &nDataSize);

		// load file - we are given a null terminator and the validation does not approve of this
		// chop down the size by 1
		std::unique_ptr<sCloudFile> pFile = sCloudFile::LoadFile(pData, nDataSize, "UGCPlayerData");
		if(!pFile)
			return false;

		// apply file
		g_SveFileObjects.GetFile(datafileIndex)->SetFile(std::move(pFile));

		// success
		return true;
	}

    int CommandDatafileGetCreatorStatsSize(int nCreatorIndex)
    {
        unsigned nDataSize = UserContentManager::GetInstance().GetCreatorStatsSize(nCreatorIndex);
        return static_cast<int>(nDataSize);
    }

    bool CommandDatafileSelectCreatorStats(int nCreatorIndex, DatafileIndex datafileIndex)
    {
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_SELECT_CREATOR_STATS"))
			return false;

        // grab UGC data
        const char* pData = UserContentManager::GetInstance().GetCreatorStats(nCreatorIndex);
        unsigned nDataSize = UserContentManager::GetInstance().GetCreatorStatsSize(nCreatorIndex);

        // check that we have content
        if(!(pData && nDataSize > 0))
            return false;

        // development validation errors
        CheckValidation(pData, &nDataSize);

        // load file - we are given a null terminator and the validation does not approve of this
        // chop down the size by 1
        std::unique_ptr<sCloudFile> pFile = sCloudFile::LoadFile(pData, nDataSize, "UGCCreatorStats");
        if(!pFile)
            return false;

        // apply file
        g_SveFileObjects.GetFile(datafileIndex)->SetFile(std::move(pFile));

        // success
        return true;
    }

	std::unique_ptr<sCloudFile> LoadOfflineInternal(const char* szContentId)
	{
		// get content path
		static const unsigned kMAX_PATH = 256;
		char szFileName[kMAX_PATH];

		gnetDebug1("LoadOfflineUGC :: Requesting %s", szContentId);

		// use the default directory first
		UserContentManager::GetInstance().GetOfflineContentPath(szContentId, 0, false, szFileName, kMAX_PATH);

		// open the stream
		fiStream* pStream = ASSET.Open(szFileName, "", true);
		if(pStream == nullptr)
		{
			// log failure
			gnetDebug1("LoadOfflineUGC :: Failed to open %s in path %s!", szContentId, szFileName);
			
			// try the update path
			UserContentManager::GetInstance().GetOfflineContentPath(szContentId, 0, true, szFileName, kMAX_PATH);
			pStream = ASSET.Open(szFileName, "", true);

			// if it fails this time, bail and assert
			if(pStream == nullptr)
			{
				gnetAssertf(0, "LoadOfflineUGC :: Failed to open %s in path %s!", szContentId, szFileName);
				return nullptr;
			}
		}

		// log asset path
		gnetDebug1("LoadOfflineUGC :: Opened %s in path %s!", szContentId, szFileName);
		
		// allocate data to read the file
		char* pData = rage_new char[pStream->Size() + 1];
		if(pData == nullptr)
		{
			gnetAssertf(0, "LoadOfflineUGC :: Could not allocate memory for: %s!", szFileName);
			pStream->Close();
			return nullptr;
		}

		// json reader requires null-terminated data
		pData[pStream->Size()] = 0;
		
		// read stream
		pStream->Read(pData, pStream->Size());

		// load file
		std::unique_ptr<sCloudFile> pFile = sCloudFile::LoadFile(pData, pStream->Size(), "UGC Offline");
		if(!pFile)
		{
			gnetAssertf(0, "LoadOfflineUGC :: Could not load file for: %s!", szFileName);
			delete[] pData;
			pStream->Close();
			return nullptr;
		}

		// punt data
		if(pData)
			delete[] pData; 

		// close stream
		pStream->Close();

		// success
		return pFile;
	}

	CloudRequestID CommandDatafileLoadOfflineUGCAsync(const char* contentId, DatafileIndex fileIndex)
	{
		if (!_CheckDatafileIndex(fileIndex, "DATAFILE_LOAD_OFFLINE_UGC"))
			return false;

		CloudRequestID requestId = CloudManager::GetInstance().GenerateRequestID();
		sFileRequest* pRequest = _CloudAllocateAndConstruct(sFileRequest, requestId);
		g_CloudFiles.PushAndGrow(pRequest);

		// create a task to load the file data
		netFireAndForgetTask<netLoadUgcFileTask>* task = nullptr;
		if (!netTask::Create(&task)
			|| !task->Configure(pRequest, contentId, fileIndex)
			|| !netTask::Run(task))
		{
			gnetError("CommandDataFileLoadOfflineUGCAsync :: Failed to create or start netLoadUgcFileTask");

			// destroy the task
			netTask::Destroy(task);

			// if we couldn't create or start the task, we can consider this request completed (and failed)
			// this allows script to clean up on their side in good time
			pRequest->m_HasCompleted = true;
		}

		return true;
	}

	bool CommandDatafileLoadOfflineUGC(const char* szContentID, DatafileIndex fileIndex)
	{
		if (!_CheckDatafileIndex(fileIndex, "DATAFILE_LOAD_OFFLINE_UGC"))
			return false;

		std::unique_ptr<sCloudFile> pFile = LoadOfflineInternal(szContentID);
		if (pFile)
		{
			// apply file
			g_SveFileObjects.GetFile(fileIndex)->SetFile(std::move(pFile));
			return true;
		}

		return false;
	}

	PARAM(datafilePrettyPrintLocal, "DATAFILE_SAVE_OFFLINE_UGC will pretty-print the JSON file");

	bool CommandDatafileSaveOfflineUGC(const char* szContentID, DatafileIndex fileIndex)
	{
		if (!_CheckDatafileIndex(fileIndex, "DATAFILE_SAVE_OFFLINE_UGC"))
			return false;

		gnetDebug1("DATAFILE_SAVE_OFFLINE_UGC :: Content ID: %s", szContentID);

		FileData file = ReadSveFileObject(*g_SveFileObjects.GetFile(fileIndex), "DATAFILE_SAVE_OFFLINE_UGC", PARAM_datafilePrettyPrintLocal.Get());

		gnetDebug1(
			"DATAFILE_SAVE_OFFLINE_UGC :: Size: %u, Json: %s", 
			file.dataSize, 
			RsonReader::ValidateJson(file.data.get(), file.dataSize) ? "Valid" : "Invalid");

		if (!gnetVerifyf(IsValid(file), "DATAFILE_SAVE_OFFLINE_UGC :: Datafile is null or empty"))
		{
			return false;
		}

		// get content path (use the update directory)
		static const unsigned kMAX_PATH = 256;
		char szFileName[kMAX_PATH];
		UserContentManager::GetInstance().GetOfflineContentPath(szContentID, 0, true, szFileName, kMAX_PATH);
		gnetDebug1("DATAFILE_SAVE_OFFLINE_UGC :: Creating new file '%s'...", szFileName);
		const char* extension = "";
		fiStream* pStream = ASSET.Create(szFileName, extension);
		if (!gnetVerifyf(pStream, "DATAFILE_SAVE_OFFLINE_UGC :: Could not create file '%s'", szFileName))
		{
			return false;
		}

		OUTPUT_ONLY(int const bytesWritten = ) pStream->Write(file.data.get(), file.dataSize);
		pStream->Flush();
		pStream->Close();

		gnetDebug1("DATAFILE_SAVE_OFFLINE_UGC :: Wrote %d bytes to file %s", bytesWritten, szFileName);
		return true;
	}

	void FileCreate(DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_CREATE")) {
			return;
		}
		gnetDebug1("DATAFILE_CREATE :: Creating new file %d", datafileIndex);
		g_SveFileObjects.GetFile(datafileIndex)->SetFile(std::unique_ptr<sveFile>(rage_new sveFile));
	}

	void FileDestroy(DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_DESTROY")) {
			return;
		}
		gnetDebug1("DATAFILE_DESTROY :: Destroying file %d", datafileIndex);

		g_SveFileObjects.GetFile(datafileIndex)->ClearFile();
	}

    void CommandDatafileStoreHeader(DatafileIndex datafileIndex)
    {
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_STORE_MISSION_HEADER")) {
			return;
		}
		gnetDebug1("DATAFILE_STORE_MISSION_HEADER :: Storing file %d as mission header", datafileIndex);

        // store the file in the mission header
        g_MissionHeader.SetFile(g_SveFileObjects.GetFile(datafileIndex)->TakeFile());
    }

    void CommandDatafileFlushHeader()
    {
        // wipe the file if it exists
        if(g_MissionHeader.GetFile())
            g_MissionHeader.ClearFile();
    }

	bool FileStartLoadFromCloud(const char* filename)
	{
		if(!Verifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
			return false; 

		if(!Verifyf(!g_LoadFromCloudOp.Pending(), "Load from cloud operation already in progress"))
			return false;

		return g_LoadFromCloudOp.Start(filename, NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId());
	}

	bool FileStartLoadFromCloudMember(const char* filename, int& handleData)
	{
		if(!Verifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
			return false; 

		if(!Verifyf(!g_LoadFromCloudOp.Pending(), "Load from cloud operation already in progress"))
			return false;

		rlGamerHandle hGamer;
		if(!CTheScripts::ImportGamerHandle(&hGamer, handleData, SCRIPT_GAMER_HANDLE_SIZE ASSERT_ONLY(, "DATAFILE_START_LOAD_FROM_CLOUD")))
			return false;

		return g_LoadFromCloudOp.Start(filename, NetworkInterface::GetLocalGamerIndex(), rlCloudMemberId(hGamer));
	}

	bool FileUpdateLoadFromCloud(int&, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_UPDATE_LOAD_FROM_CLOUD"))
	{
			return false;
		}

		if(g_LoadFromCloudOp.Pending())
		{
			return g_LoadFromCloudOp.Update(datafileIndex);
		}

		return false;
	}

	bool FileIsLoadPending()
	{
		return g_LoadFromCloudOp.Pending();
	}

	bool FileStartSaveToCloud(const char* filename, DatafileIndex datafileIndex)
	{
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_START_SAVE_TO_CLOUD"))
			return false;

		if(!Verifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
			return false; 

		if(!Verifyf(g_SveFileObjects.GetFile(datafileIndex), "FileStartSaveToCloud - Bad dictionary pointer. Filename is %s", filename?filename:"NULL"))
			return false;

		if(!Verifyf(!g_SaveToCloudOp.Pending(), "Save to cloud operation already in progress"))
			return false;

		return g_SaveToCloudOp.Start(filename, g_SveFileObjects.GetFile(datafileIndex)->GetFile());
	}

	bool FileUpdateSaveToCloud(int& success)
	{
		if(g_SaveToCloudOp.Pending())
		{
			success = g_SaveToCloudOp.Update();
			if(!success)
				return false;
			return true;
		}
		return false;
	}

	bool FileIsSavePending()
	{
		return g_SaveToCloudOp.Pending();
	}
	
	void FileStartDeleteFromCloud(const char* filename)
	{
		if(!Verifyf(NetworkInterface::IsCloudAvailable(), "Cloud not available - use NETWORK_IS_CLOUD_AVAILABLE!"))
			return; 

		if(!Verifyf(!g_DeleteFromCloudOp.Pending(), "Delete from cloud operation already in progress"))
			return;

		g_DeleteFromCloudOp.Start(filename);
	}

	bool FileUpdateDeleteFromCloud(int& success)
	{
		if(g_DeleteFromCloudOp.Pending())
		{
			success = g_DeleteFromCloudOp.Update();
			if(!success)
				return false;
			return true;
		}
		return false;
	}

	bool FileIsDeletePending()
	{
		return g_DeleteFromCloudOp.Pending();
	}

	// This is a no-op function for now, but could be filled in later if we need a higher level
	// sveFile data structure of some sort
	const char * GetFileDict(DatafileIndex datafileIndex)	
	{	
		if (!_CheckDatafileIndex(datafileIndex, "DATAFILE_GET_FILE_DICT")) {
			return nullptr;
		}

		return (const char *)g_SveFileObjects.GetFile(datafileIndex)->GetFile(); 	
	}

	//////////////////////////////////////////////////////////////////////////
	// Operations on additional UGC files
	// Required so that Ata Tabrizi can load a file containing DJ/dancing data at the same time 
	//	as the freemode scripts call the original commands to load UGC mission data
	// Now they just redirect to the normal commands, but offset into the array so that the normal commands can 
	// have 0 as the default index.

	bool CommandDatafileLoadOfflineUGCForAdditionalDataFile(DatafileIndex fileIndex, const char* szContentID)
	{
		gnetDebug1("DATAFILE_LOAD_OFFLINE_UGC_FOR_ADDITIONAL_DATA_FILE called with file index %d", fileIndex);

		return CommandDatafileLoadOfflineUGC(szContentID, fileIndex + SveFileObjectArray::MAX_NUM_NORMAL_DATA_FILES);
	}

	void FileDestroyForAdditionalDataFile(DatafileIndex fileIndex)
	{
		gnetDebug1("DATAFILE_DESTROY_FOR_ADDITIONAL_DATA_FILE called with file index %d", fileIndex);

		FileDestroy(fileIndex + SveFileObjectArray::MAX_NUM_NORMAL_DATA_FILES);
		}

	const char *GetFileDictForAdditionalDataFile(DatafileIndex fileIndex)
	{
		gnetDebug1("DATAFILE_GET_FILE_DICT_FOR_ADDITIONAL_DATA_FILE called with file index %d", fileIndex);

		return GetFileDict(fileIndex + SveFileObjectArray::MAX_NUM_NORMAL_DATA_FILES);
	}

	//////////////////////////////////////////////////////////////////////////
	// Operations on dictionaries

	//////////////////////////////////////////////////////////////////////////
	// Dictionary setters

	template<typename _SveType, typename _CppType>
	void DictSet(sveDict* dict, const char* name, _CppType val)
	{
		if (!Verifyf(name, "DictSet - NULL name pointer"))
		{
			return;
		}

		if (!Verifyf(strlen(name) > 0, "DictSet - Empty name string"))
		{
			return;
		}

		if (!Verifyf(dict, "DictSet - Bad dictionary pointer (NULL). Name within dictionary is %s", name))
		{
			return;
		}

		atFinalHashString finalHashStringOfName(name);
		sveNode* oldNode = (*dict)[finalHashStringOfName];
		if (oldNode)
		{
			if (oldNode->GetType() == _SveType::GetStaticType())
			{
				_SveType* oldTypedNode = smart_cast<_SveType*>(oldNode);
				oldTypedNode->SetValue(val);
				return;
			}
			dict->Remove(finalHashStringOfName);
		}

		dict->Insert(finalHashStringOfName, val);
	}

	void DictSetBool(sveDict* dict, const char* name, bool val)			{ 	DictSet<sveBool>(dict, name, val); }
	void DictSetInt(sveDict* dict, const char* name, int val)			{	DictSet<sveInt>(dict, name, val); }
	void DictSetFloat(sveDict* dict, const char* name, float val)		{ 	DictSet<sveFloat>(dict, name, val); }
	void DictSetString(sveDict* dict, const char* name, const char* val){ 	DictSet<sveString>(dict, name, val); }
	void DictSetVector(sveDict* dict, const char* name, const scrVector &val)	{ 	Vec3V vec = (Vec3V)val; DictSet<sveVec3, Vec3V_In>(dict, name, vec); }

	const char * DictCreateDict(sveDict* parent, const char* name)
	{
		if (!Verifyf(name, "DictCreateDict - NULL name pointer"))
		{
			return NULL;
		}

		if (!Verifyf(strlen(name) > 0, "DictCreateDict - Empty name string"))
		{
			return NULL;
		}

		if (!Verifyf(parent, "DictCreateDict - Bad dictionary pointer (NULL). Name within dictionary is %s", name))
		{
			return NULL;
		}

		atFinalHashString finalHashStringOfName(name);

		sveNode* oldNode = (*parent)[finalHashStringOfName];
		if (oldNode)
		{
			parent->Remove(finalHashStringOfName);
		}

		sveDict* dict = rage_new sveDict;
		parent->Insert(finalHashStringOfName, dict);
		return (const char *) dict;
	}

	const char * DictCreateArray(sveDict* parent, const char* name)
	{
		if (!Verifyf(name, "DictCreateArray - NULL name pointer"))
		{
			return NULL;
		}

		if (!Verifyf(strlen(name) > 0, "DictCreateArray - Empty name string"))
		{
			return NULL;
		}

		if (!Verifyf(parent, "DictCreateArray - Bad dictionary pointer (NULL). Name within dictionary is %s", name))
		{
			return NULL;
		}

		atFinalHashString finalHashStringOfName(name);

		sveNode* oldNode = (*parent)[finalHashStringOfName];
		if (oldNode)
		{
			parent->Remove(finalHashStringOfName);
		}

		sveArray* array = rage_new sveArray;
		parent->Insert(finalHashStringOfName, array);
		return (const char *) array;
	}

	//////////////////////////////////////////////////////////////////////////
	// Dictionary getters

	template<typename _SveType, typename _CppType>
	_CppType DictGet(sveDict* dict, const char* name, _CppType def)
	{
		if (!Verifyf(dict, "DictGet - Bad dictionary pointer (NULL). Name within dictionary is %s", name?name:"NULL"))
		{
			return def;
		}
		sveNode* valNode = (*dict)[atFinalHashString(name)];
		if (!valNode)
		{
			Warningf("Couldn't find a value named %s", name);
			return def;
		}

		_SveType* realValNode = valNode->Downcast<_SveType>();
		return realValNode ? realValNode->GetValue() : def;
	}

	bool		DictGetBool(sveDict* dict, const char* name)	{ return DictGet<sveBool>(dict, name, false); }
	int			DictGetInt(sveDict* dict, const char* name)		{ return DictGet<sveInt>(dict, name, 0); }
	float		DictGetFloat(sveDict* dict, const char* name)	{ return DictGet<sveFloat>(dict, name, 0.0f); }
	const char* DictGetString(sveDict* dict, const char* name)	{ return DictGet<sveString>(dict, name, ""); }
	scrVector	DictGetVector(sveDict* dict, const char* name)	{ return DictGet<sveVec3, Vec3V>(dict, name, Vec3V(V_ZERO)); }


	const char * DictGetDict(sveDict* parent, const char* name)
	{
		if (!Verifyf(parent, "DictGetDict - Bad dictionary pointer (NULL). Name within dictionary is %s", name?name:"NULL"))
		{
			return NULL;
		}
		sveNode* node = (*parent)[atFinalHashString(name)];
		
		return node ? (const char *) (node->Downcast<sveDict>()) : NULL;
	}

	const char * DictGetArray(sveDict* parent, const char* name)
	{
		if (!Verifyf(parent, "DictGetArray - Bad dictionary pointer (NULL). Name within dictionary is %s", name?name:"NULL"))
		{
			return NULL;
		}
		sveNode* node = (*parent)[atFinalHashString(name)];

		return node ? (const char *) (node->Downcast<sveArray>()) : NULL;
	}

	//////////////////////////////////////////////////////////////////////////
	// Other dictionary ops

	void DictDeleteItem(sveDict* dict, const char* name)
	{
		if (!Verifyf(dict, "DictDeleteItem - Bad dictionary pointer (NULL). Name within dictionary is %s", name?name:"NULL"))
		{
			return;
		}
		dict->Remove(atFinalHashString(name));
	}

	int DictGetType(sveDict* dict, const char* name)
	{
		if (!Verifyf(dict, "DictGetType - Bad dictionary pointer (NULL). Name within dictionary is %s", name?name:"NULL"))
		{
			return 0;
		}
		sveNode* node = (*dict)[atFinalHashString(name)];
		return node ? (int)node->GetType() : sveNode::SVE_NONE;
	}

	//////////////////////////////////////////////////////////////////////////
	// Operations on arrays

	bool SanityCheckArray(sveArray* array, int index)
	{
		if (!array)
		{
			Errorf("Bad array pointer");
			return false;
		}
		if (index < 0 || index >= array->GetCount())
		{
			Errorf("Bad index %d for array, array has %d items", index, array->GetCount());
			return false;
		}
		return true;
	}

	//////////////////////////////////////////////////////////////////////////
	// Array appending
	template<typename _Type>
	void ArrayAdd(sveArray* array, _Type val)
	{
		if (!Verifyf(array, "Bad array pointer"))
		{
			return;
		}
		array->Append(val);
	}

	void ArrayAddBool(sveArray* array, bool val)				{ ArrayAdd(array, val); }
	void ArrayAddInt(sveArray* array, int val)					{ ArrayAdd(array, val); }
	void ArrayAddFloat(sveArray* array, float val)				{ ArrayAdd(array, val); }
	void ArrayAddString(sveArray* array, const char* val)		{ ArrayAdd(array, val); }
	void ArrayAddVector(sveArray* array, const scrVector &val)			{ Vec3V vec = (Vec3V)val; ArrayAdd<Vec3V_In>(array, vec); }	

	const char * ArrayAddDict(sveArray* array)
	{
		sveDict* child = rage_new sveDict;

		if (!Verifyf(array, "Bad array pointer"))
		{
			return NULL;
		}
		array->Append(child);
		return (const char *) child;
	}


	const char * ArrayAddArray(sveArray* array)
	{
		sveArray* child = rage_new sveArray;

		if (!Verifyf(array, "Bad array pointer"))
		{
			return NULL;
		}
		array->Append(child);
		return (const char *) child;
	}

	//////////////////////////////////////////////////////////////////////////
	// Array setters

	template<typename _SveType, typename _CppType>
	void ArraySet(sveArray* array, int index, _CppType val)
	{
		if (!SanityCheckArray(array, index))
		{
			return;
		}

		sveNode*& oldNode = (*array)[index];

		if (_SveType::GetStaticType() == oldNode->GetType())
		{
			_SveType* oldTypedNode = smart_cast<_SveType*>(oldNode);
			oldTypedNode->SetValue(val);
		}
		else
		{
			delete oldNode;
			oldNode = rage_new _SveType(val);
		}
	}

	void ArraySetBool(sveArray* array, int index, bool val)				{ ArraySet<sveBool>(array, index, val); }
	void ArraySetInt(sveArray* array, int index, int val)				{ ArraySet<sveInt>(array, index, val); }
	void ArraySetFloat(sveArray* array, int index, float val)			{ ArraySet<sveFloat>(array, index, val); }
	void ArraySetString(sveArray* array, int index, const char* val)	{ ArraySet<sveString>(array, index, val); }
	void ArraySetVector(sveArray* array, int index, const scrVector &val)		{ Vec3V vec = (Vec3V)val; ArraySet<sveVec3>(array, index, vec); }

	const char * ArrayCreateDict(sveArray* array, int index)
	{
		if (!SanityCheckArray(array, index))
		{
			return NULL;
		}

		sveNode*& oldNode = (*array)[index];
		delete oldNode;

		sveDict* newDict = rage_new sveDict;

		oldNode = newDict;

		return (const char *) newDict;
	}

	const char * ArrayCreateArray(sveArray* array, int index)
	{
		if (!SanityCheckArray(array, index))
		{
			return NULL;
		}

		sveNode*& oldNode = (*array)[index];
		delete oldNode;

		sveArray* newArr = rage_new sveArray;

		oldNode = newArr;

		return (const char *) newArr;
	}

	//////////////////////////////////////////////////////////////////////////
	// Array getters

	template<typename _SveType, typename _CppType>
	_CppType ArrayGet(sveArray* array, int index, _CppType def)
	{
		if (!SanityCheckArray(array, index))
		{
			return def;
		}

		sveNode* valNode = (*array)[index];

		_SveType* realValNode = valNode->Downcast<_SveType>();
		return realValNode ? realValNode->GetValue() : def;
	}

	bool		ArrayGetBool(sveArray* array, int index)	{ return ArrayGet<sveBool>(array, index, false); }
	int			ArrayGetInt(sveArray* array, int index)		{ return ArrayGet<sveInt>(array, index, 0); }
	float		ArrayGetFloat(sveArray* array, int index)	{ return ArrayGet<sveFloat>(array, index, 0.0f); }
	const char* ArrayGetString(sveArray* array, int index)	{ return ArrayGet<sveString>(array, index, ""); }
	scrVector	ArrayGetVector(sveArray* array, int index)	{ return ArrayGet<sveVec3>(array, index, Vec3V(V_ZERO)); }

	const char * ArrayGetDict(sveArray* array, int index)
	{
		if (!SanityCheckArray(array, index))
		{
			return NULL;
		}

		sveNode* valNode = (*array)[index];

		return (const char *) (valNode->Downcast<sveDict>());
	}

	const char * ArrayGetArray(sveArray* array, int index)
	{
		if (!SanityCheckArray(array, index))
		{
			return NULL;
		}

		sveNode* valNode = (*array)[index];

		return (const char *) (valNode->Downcast<sveArray>());
	}


	//////////////////////////////////////////////////////////////////////////
	// Array other operations

	void ArrayDeleteItem(sveArray* array, int index)
	{
		if (!SanityCheckArray(array, index))
		{
			return;
		}


		array->Remove(index);
	}

	int ArrayGetCount(sveArray* array)
	{
		if (!Verifyf(array, "Bad array pointer"))
		{
			return 0;
		}
		return array->GetCount();
	}

	int ArrayGetType(sveArray* array, int index)
	{
		if (!SanityCheckArray(array, index))
		{
			return (int)sveNode::SVE_NONE;
		}

		return (int)(*array)[index]->GetType();
	}


	//////////////////////////////////////////////////////////////////////////
	// Registration function

	void SetupScriptCommands()
	{
		SCR_REGISTER_SECURE(DATAFILE_WATCH_REQUEST_ID,0x35c49a098cce05ca, CommandDatafileWatchRequestID);
		SCR_REGISTER_SECURE(DATAFILE_CLEAR_WATCH_LIST,0x8b66950593ce7823, CommandDatafileClearWatchList);
		SCR_REGISTER_SECURE(DATAFILE_IS_VALID_REQUEST_ID,0x267b35a75af41658, CommandDatafileIsValidRequestID);
		SCR_REGISTER_SECURE(DATAFILE_HAS_LOADED_FILE_DATA,0x13268820d6544012, CommandDatafileHasLoadedFileData);
		SCR_REGISTER_SECURE(DATAFILE_HAS_VALID_FILE_DATA,0x9c976b35944b749d, CommandDatafileHasValidFileData);
		SCR_REGISTER_SECURE(DATAFILE_SELECT_ACTIVE_FILE,0xa4070cbbf5694ec8, CommandDatafileSelectActiveFile);
		SCR_REGISTER_SECURE(DATAFILE_DELETE_REQUESTED_FILE,0x7233df6dd0b0e205, CommandDatafileDeleteFile);

		SCR_REGISTER_SECURE(UGC_CREATE_CONTENT,0xfbb798e2f410d4ab, CommandDatafileCreateUGC);
        SCR_REGISTER_SECURE(UGC_CREATE_MISSION,0x1e69765a21e6c2af, CommandDatafileCreateMissionUGC);
        SCR_REGISTER_SECURE(UGC_UPDATE_CONTENT,0x9d886ed5ccb4d1bb, CommandDatafileUpdateUGC);
        SCR_REGISTER_SECURE(UGC_UPDATE_MISSION,0xe588c5baaf3797de, CommandDatafileUpdateMissionUGC);
        SCR_REGISTER_SECURE(UGC_SET_PLAYER_DATA,0x25dbc2aaeb2f3602, CommandDatafileUGCSetPlayerData);
		SCR_REGISTER_UNUSED(DATAFILE_GET_UGC_DATA_SIZE,0x1b77f78143bdde0c, CommandDatafileGetUGCDataSize);
		SCR_REGISTER_SECURE(DATAFILE_SELECT_UGC_DATA,0x3b307f0fe01b03d2, CommandDatafileSelectUGCData);
		SCR_REGISTER_UNUSED(DATAFILE_GET_UGC_STATS_SIZE,0xae297e17b7d96c68, CommandDatafileGetUGCStatsSize);
		SCR_REGISTER_SECURE(DATAFILE_SELECT_UGC_STATS,0xf58db7bc2763da56, CommandDatafileSelectUGCStats);
		SCR_REGISTER_UNUSED(DATAFILE_GET_UGC_PLAYER_DATA_SIZE,0xaeebb0a55d44201b, CommandDatafileGetUGCPlayerDataSize);
		SCR_REGISTER_SECURE(DATAFILE_SELECT_UGC_PLAYER_DATA,0x43d13a207591add7, CommandDatafileSelectUGCPlayerData);
        SCR_REGISTER_UNUSED(DATAFILE_GET_CREATOR_STATS_SIZE,0x9a438c3fdb09b18c, CommandDatafileGetCreatorStatsSize);
        SCR_REGISTER_SECURE(DATAFILE_SELECT_CREATOR_STATS,0xb04a9141590dce90, CommandDatafileSelectCreatorStats);

		SCR_REGISTER_SECURE(DATAFILE_LOAD_OFFLINE_UGC,0x938e755f4f72b35e, CommandDatafileLoadOfflineUGC);
		SCR_REGISTER_UNUSED(DATAFILE_LOAD_OFFLINE_UGC_ASYNC, 0x91963295e78c9721, CommandDatafileLoadOfflineUGCAsync);
		SCR_REGISTER_UNUSED(DATAFILE_SAVE_OFFLINE_UGC,0x80837aae0a271b3b, CommandDatafileSaveOfflineUGC);

		SCR_REGISTER_SECURE(DATAFILE_CREATE,0x280f5fc1db79e757, FileCreate);
		SCR_REGISTER_SECURE(DATAFILE_DELETE,0x0222406293ea78ca, FileDestroy);
        SCR_REGISTER_SECURE(DATAFILE_STORE_MISSION_HEADER,0xca09ed6e4be3a61c, CommandDatafileStoreHeader);
        SCR_REGISTER_SECURE(DATAFILE_FLUSH_MISSION_HEADER,0xaf42862c16a8f3ec, CommandDatafileFlushHeader);
        SCR_REGISTER_SECURE(DATAFILE_GET_FILE_DICT,0x16307ef1b7664424, GetFileDict);
		SCR_REGISTER_UNUSED(DATAFILE_START_LOAD_FROM_CLOUD,0x97e61a679e81efc3, FileStartLoadFromCloud);
		SCR_REGISTER_UNUSED(DATAFILE_START_LOAD_FROM_CLOUD_MEMBER,0xc56d710394df153c, FileStartLoadFromCloudMember);
		SCR_REGISTER_UNUSED(DATAFILE_UPDATE_LOAD_FROM_CLOUD,0x294c1c6a969a33f5, FileUpdateLoadFromCloud);
		SCR_REGISTER_UNUSED(DATAFILE_IS_LOAD_PENDING,0x58940e8edd1412ef, FileIsLoadPending);
		SCR_REGISTER_SECURE(DATAFILE_START_SAVE_TO_CLOUD,0xf15dad6d0291a996, FileStartSaveToCloud);
		SCR_REGISTER_SECURE(DATAFILE_UPDATE_SAVE_TO_CLOUD,0x735b09bad85f258f, FileUpdateSaveToCloud);
		SCR_REGISTER_SECURE(DATAFILE_IS_SAVE_PENDING,0x665245305cd9e866, FileIsSavePending);
		SCR_REGISTER_UNUSED(DATAFILE_DELETE_FILE,0x7be371f010a51bde, FileStartDeleteFromCloud);
		SCR_REGISTER_UNUSED(DATAFILE_UPDATE_DELETE_FILE,0x9bb40c554501c51d, FileUpdateDeleteFromCloud);
		SCR_REGISTER_UNUSED(DATAFILE_IS_DELETE_PENDING,0xb7ac4df201400128, FileIsDeletePending);

		SCR_REGISTER_SECURE(DATAFILE_LOAD_OFFLINE_UGC_FOR_ADDITIONAL_DATA_FILE,0xce3f2e170c5c4544, CommandDatafileLoadOfflineUGCForAdditionalDataFile);
		SCR_REGISTER_SECURE(DATAFILE_DELETE_FOR_ADDITIONAL_DATA_FILE,0xc1df883f184fcabc, FileDestroyForAdditionalDataFile);
		SCR_REGISTER_SECURE(DATAFILE_GET_FILE_DICT_FOR_ADDITIONAL_DATA_FILE,0x0d7d14b5fa90e730, GetFileDictForAdditionalDataFile);

		SCR_REGISTER_SECURE(DATADICT_SET_BOOL,0x7d75cd6838d91c65, DictSetBool);
		SCR_REGISTER_SECURE(DATADICT_SET_INT,0x59ebbea4150e286f, DictSetInt);
		SCR_REGISTER_SECURE(DATADICT_SET_FLOAT,0xb60ad82fa227f055, DictSetFloat);
		SCR_REGISTER_SECURE(DATADICT_SET_STRING,0x34619b36929bd4e5, DictSetString);
		SCR_REGISTER_SECURE(DATADICT_SET_VECTOR,0x418e560d6ca5b93c, DictSetVector);
		SCR_REGISTER_SECURE(DATADICT_CREATE_DICT,0x34a4dec252e422df, DictCreateDict);
		SCR_REGISTER_SECURE(DATADICT_CREATE_ARRAY,0x4b088bbe020c20cc, DictCreateArray);

		SCR_REGISTER_SECURE(DATADICT_GET_BOOL,0xea1c8198294061c8, DictGetBool);
		SCR_REGISTER_SECURE(DATADICT_GET_INT,0xe6a0afebb87cc96c, DictGetInt);
		SCR_REGISTER_SECURE(DATADICT_GET_FLOAT,0x28bbe7d39916b197, DictGetFloat);
		SCR_REGISTER_SECURE(DATADICT_GET_STRING,0xb1473708b3bc6ed2, DictGetString);
		SCR_REGISTER_SECURE(DATADICT_GET_VECTOR,0xd25353c9f68a9be0, DictGetVector);
		SCR_REGISTER_SECURE(DATADICT_GET_DICT,0xdd51dcf7a4c06797, DictGetDict);
		SCR_REGISTER_SECURE(DATADICT_GET_ARRAY,0x61d9cdbfec321364, DictGetArray);

		SCR_REGISTER_UNUSED(DATADICT_DELETE,0x758afd407520a47b, DictDeleteItem);
		SCR_REGISTER_SECURE(DATADICT_GET_TYPE,0x1d0d99adc275c523, DictGetType);

		SCR_REGISTER_SECURE(DATAARRAY_ADD_BOOL,0xfc476fca97221928, ArrayAddBool);
		SCR_REGISTER_SECURE(DATAARRAY_ADD_INT,0x13855f4ad74314bf, ArrayAddInt);
		SCR_REGISTER_SECURE(DATAARRAY_ADD_FLOAT,0x75ff0b10c341e814, ArrayAddFloat);
		SCR_REGISTER_SECURE(DATAARRAY_ADD_STRING,0x94cd9229338f719a, ArrayAddString);
		SCR_REGISTER_SECURE(DATAARRAY_ADD_VECTOR,0x558cb010292c768b, ArrayAddVector);
		SCR_REGISTER_SECURE(DATAARRAY_ADD_DICT,0xc7aff6dc015e7732, ArrayAddDict);
		SCR_REGISTER_UNUSED(DATAARRAY_ADD_ARRAY,0xf87ba4abcf763583, ArrayAddArray);

		SCR_REGISTER_UNUSED(DATAARRAY_SET_BOOL,0xe9c20ec5f492e1a7, ArraySetBool);
		SCR_REGISTER_UNUSED(DATAARRAY_SET_INT,0x1b31e063459f84ed, ArraySetInt);
		SCR_REGISTER_UNUSED(DATAARRAY_SET_FLOAT,0x56ebff0d4e810583, ArraySetFloat);
		SCR_REGISTER_UNUSED(DATAARRAY_SET_STRING,0x56ddf458d514587f, ArraySetString);
		SCR_REGISTER_UNUSED(DATAARRAY_SET_VECTOR,0xd5c2d3e23002712a, ArraySetVector);
		SCR_REGISTER_UNUSED(DATAARRAY_CREATE_DICT,0xce5ed6ee3e772ee2, ArrayCreateDict);
		SCR_REGISTER_UNUSED(DATAARRAY_CREATE_ARRAY,0x395f4e27e36df585, ArrayCreateArray);

		SCR_REGISTER_SECURE(DATAARRAY_GET_BOOL,0xd6d7eb548f1030be, ArrayGetBool);
		SCR_REGISTER_SECURE(DATAARRAY_GET_INT,0x347f5e3d631a03ed, ArrayGetInt);
		SCR_REGISTER_SECURE(DATAARRAY_GET_FLOAT,0xcba5860362bbb689, ArrayGetFloat);
		SCR_REGISTER_SECURE(DATAARRAY_GET_STRING,0xb4f4434c791d9a40, ArrayGetString);
		SCR_REGISTER_SECURE(DATAARRAY_GET_VECTOR,0x7b5d30273df6afc7, ArrayGetVector);
		SCR_REGISTER_SECURE(DATAARRAY_GET_DICT,0x69fd7fb97594957b, ArrayGetDict);
		SCR_REGISTER_UNUSED(DATAARRAY_GET_ARRAY,0x0351f41fbc38f344, ArrayGetArray);

		SCR_REGISTER_SECURE(DATAARRAY_GET_COUNT,0x3aeb680423f056cf, ArrayGetCount);
		SCR_REGISTER_UNUSED(DATAARRAY_DELETE,0x5dea74f75dd57863, ArrayDeleteItem);
		SCR_REGISTER_SECURE(DATAARRAY_GET_TYPE,0x3e7dec6c8d20a13b, ArrayGetType);
	}

#if RSG_XBOX || RSG_SCE || RSG_XENON
	static const int THREAD_CPU_ID = 5;
#else
	static const int THREAD_CPU_ID = 0;
#endif

	void InitialiseThreadPool()
	{
		gnetDebug1("InitialiseThreadPool");
		s_CloudFileThreadPool.Init();
		s_CloudFileThreadPool.AddThread(&s_CloudFileThread, "CloudFile", THREAD_CPU_ID, (32 * 1024) << __64BIT);
	}

	void ShutdownThreadPool()
	{
		gnetDebug1("ShutdownThreadPool");
		s_CloudFileThreadPool.Shutdown();
	}

#if __DEV && 0 // Test code
	int g_ValidJson = 0;
	int g_InvalidJson = 0;

	void EachJsonFile(const fiFindData& data, void* rootDir)
	{
		ASSET.PushFolder((char*)rootDir);
		if (!strncmp(data.m_Name + strlen(data.m_Name) - 5, ".json", 5))
		{
			sveFile* file = sveFile::LoadFile(data.m_Name);
			if (file)
			{
				g_ValidJson++;
				fiStream* out = ASSET.Create(data.m_Name, "out_ok");
				if (out)
				{
					file->WriteJson(*out, 0, false);
					out->Close();
				}
				delete file;
			}
			else
			{
				Warningf("Couldn't load file %s/%s", rootDir, data.m_Name);
				fiStream* out = ASSET.Create(data.m_Name, "out_bad");
				if (out)
				{
					out->Close();
				}
				g_InvalidJson++;
			}
		}
		ASSET.PopFolder();
	}

	void TestLoadFilesInDir(const char* dirName)
	{
		ASSET.EnumFiles(dirName, EachJsonFile, (void*)dirName);
		Displayf("Finished test: %d valid, %d invalid", g_ValidJson, g_InvalidJson);
	}
#endif

} // namespace sveScriptBindings
