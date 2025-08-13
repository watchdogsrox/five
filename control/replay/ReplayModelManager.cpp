#include "ReplayModelManager.h"

#if GTA_REPLAY

#include "modelinfo/ModelInfo.h"
#include "system/timer.h"

#include "debug/gtapicker.h"
#include "streaming/packfilemanager.h"

CReplayModelManager	s_replaySRLManager;

/////////////////////////////////////////////////////////////////////////
void CReplayModelManager::Init(u32 preloadRequestSize)
{
	m_modelRequests.Reset();
	m_modelRequests.Resize(preloadRequestSize);

	m_modelLoadTimeout = REPLAY_LOAD_MODEL_TIME_MAX;
}


//////////////////////////////////////////////////////////////////////////
// Change to stop old clips crashing after the Casino DLC...
// Objects with a maptype index of 1137 or later can't be relied upon to load
// as the order is all out of whack.  New versions will store hashes instead
// of indexes.
bool CheckModel(strLocalIndex& mapTypeDef, bool oldVersion)
{
	if(oldVersion && mapTypeDef.Get() >= 1137)
	{
		// TF_INVALID is the default value when the model isn't in a maptype file.
		// However TF_INVALID resolves to 4095...
		if(mapTypeDef.Get() == fwModelId::TF_INVALID)
			return true;

		return false;
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayModelManager::PreloadRequest(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion, bool& requestIsLoaded, u32 currGameTime)
{
	if(!CheckModel(mapTypeDef, oldVersion))
		return false;

	modelRequest testReq(modelHash, mapTypeDef);
	modelRequest *pNewRequest = PreloadRequestInternal(testReq, requestIsLoaded, currGameTime);

	if(pNewRequest)
	{
		requestIsLoaded = LoadModel(modelHash, mapTypeDef, oldVersion, false, pNewRequest->m_strModelRequest, pNewRequest->m_strRequest, STRFLAG_DONTDELETE);

		if(requestIsLoaded)
			replayDebugf2("Loaded Model %u, %u", modelHash, mapTypeDef.Get());
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayModelManager::PreloadRequest(u32 modelHash, strLocalIndex localIndex, bool oldVersion, int streamingModuleId, bool& requestIsLoaded, u32 currGameTime)
{
	if(!CheckModel(localIndex, oldVersion))
		return false;

	modelRequest testReq(modelHash, localIndex, streamingModuleId);
	modelRequest *pNewRequest = PreloadRequestInternal(testReq, requestIsLoaded, currGameTime);

	if(pNewRequest)
	{
		requestIsLoaded = LoadAsset(localIndex, streamingModuleId, false, pNewRequest->m_strRequest, STRFLAG_DONTDELETE);

		if(requestIsLoaded)
			replayDebugf2("Loaded Asset %u, %d", localIndex.Get(), streamingModuleId);
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
CReplayModelManager::modelRequest *CReplayModelManager::PreloadRequestInternal(modelRequest &inReq, bool& preloadSuccess, u32 currGameTime)
{
	if(m_failedStreamingRequests.Find(failStrModelInfo(inReq.m_modelHash, inReq.m_mapTypeDef.Get())) != -1)
	{
		// Previously failed so don't try again
		return NULL;
	}

	int index = m_modelRequests.Find(inReq);
	if(index != -1)
	{
		// We do, update the time on it
		modelRequest& req = m_modelRequests[index];
		if(!req.IsValid())
		{
			// If failed remove it
			m_modelRequests.Delete(index);
			return NULL;
		}

		preloadSuccess = req.HasLoaded();

		if(preloadSuccess)
			replayDebugf2("Already loaded %u, %u, %u", inReq.m_modelHash, inReq.m_mapTypeDef.Get(), inReq.m_ModuleId);

		req.m_lastPreloadTime = currGameTime;
		return NULL;
	}

	// Don't currently have a request so ask to load and create a new request
	modelRequest* pNewReq = NULL;
	for(int i = 0; i < m_modelRequests.size(); ++i)
	{
		if(m_modelRequests[i].IsFree())
		{
			pNewReq = &m_modelRequests[i];
			break;
		}
	}

	replayAssert(pNewReq);
	if(pNewReq == NULL)
		return NULL;

	// Record the details.
	pNewReq->m_modelHash = inReq.m_modelHash;
	pNewReq->m_mapTypeDef = inReq.m_mapTypeDef;
	pNewReq->m_ModuleId = inReq.m_ModuleId;
	// Set the load time.
	pNewReq->m_lastPreloadTime = currGameTime;
	return pNewReq;
}


//////////////////////////////////////////////////////////////////////////
void CReplayModelManager::UpdatePreloadRequests(u32 time)
{
	for(int i = m_modelRequests.GetCount()-1; i >= 0; --i)
	{
		modelRequest& req = m_modelRequests[i];
		if(req.m_lastPreloadTime != 0 && abs((s64)time - (s64)req.m_lastPreloadTime) > (s64)((float)REPLAY_PRELOAD_TIME * 1.25f))
		{
			req.Release();
		}
	}
}



//////////////////////////////////////////////////////////////////////////
void CReplayModelManager::AddLoadRequest(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion)
{
	modelRequest testReq(modelHash, mapTypeDef);
	int index = m_modelRequests.Find(testReq);
	if(index != -1)
	{
		modelRequest& req = m_modelRequests[index];
		req.AddRef();
		return;
	}

	modelRequest* pNewReq = NULL;
	for(int i = 0; i < m_modelRequests.size(); ++i)
	{
		if(m_modelRequests[i].IsFree())
		{
			pNewReq = &m_modelRequests[i];
			break;
		}
	}

	if(replayVerifyf(LoadModel(modelHash, mapTypeDef, oldVersion, false, pNewReq->m_strModelRequest, pNewReq->m_strRequest), "Model failing to load"))
	{
		pNewReq->m_modelHash = modelHash;
		pNewReq->m_mapTypeDef = mapTypeDef;
		pNewReq->AddRef();
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayModelManager::RemoveLoadRequest(u32 modelHash, strLocalIndex mapTypeDef)
{
	modelRequest testReq(modelHash, mapTypeDef);
	int index = m_modelRequests.Find(testReq);
	if(index != -1)
	{
		modelRequest& req = m_modelRequests[index];
		if(req.RemoveRef())
		{
			req.Release();
		}
		return;
	}
}


//////////////////////////////////////////////////////////////////////////
void CReplayModelManager::FlushLoadRequests()
{
	for(int i = 0; i < m_modelRequests.size(); ++i)
	{
		if(!m_modelRequests[i].IsFree())
		{
			m_modelRequests[i].Release();
		}
	}

	m_failedStreamingRequests.clear();

	m_modelLoadTimeout = REPLAY_LOAD_MODEL_TIME_MAX;
}


//////////////////////////////////////////////////////////////////////////
bool CReplayModelManager::LoadAsset(strLocalIndex requestID, int streamingModuleID, bool createUrgent, strRequest& streamingReq, u32 flags)
{
	streamingReq.Request(requestID, streamingModuleID, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | flags);

	BANK_ONLY(sysTimer timer;)
	while(createUrgent && streamingReq.IsValid() && !streamingReq.HasLoaded())
	{
#if __BANK
		if(timer.GetMsTime() >= m_modelLoadTimeout)
		{
			m_modelLoadTimeout = REPLAY_LOAD_MODEL_TIME_FALLBACK;
			replayAssertf(false, "Failed to load asset for (%d) after %f ms of trying. Requestid:%u", requestID.Get(), m_modelLoadTimeout, streamingReq.GetRequestId().Get());
			return false;
		}
#endif //__BANK

		CStreaming::LoadAllRequestedObjects();
	}

	return true;
}


//////////////////////////////////////////////////////////////////////////
rage::fwModelId CReplayModelManager::LoadModel(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion, bool createUrgent)
{
	rage::strModelRequest streamingModelReq;
	rage::strRequest streamingReq;
	if(!LoadModel(modelHash, mapTypeDef, oldVersion, createUrgent, streamingModelReq, streamingReq))
	{
		return fwModelId();
	}

	fwArchetype* pArchtype = fwArchetypeManager::GetArchetypeFromHashKey(modelHash, NULL);
	replayAssert(pArchtype);
	fwModelId modelID = fwArchetypeManager::LookupModelId(pArchtype);

	return modelID;
}

#if __BANK
void CReplayModelManager::GetModelInfo(fwModelId modelID, const char*& pName, const char*& pMount)
{
	(void)pName;
#if !__NO_OUTPUT
	const char* p = CModelInfo::GetBaseModelInfoName(modelID);
	if(p)
	{
		pName = p;
	}
#endif 

	CBaseModelInfo* pBaseModelInfo = CModelInfo::GetBaseModelInfo(modelID);
	if (pBaseModelInfo)
	{
		strIndex streamingIndex = EntityDetails_StreamingIndex(pBaseModelInfo);

		if (streamingIndex.IsValid())
		{
			strStreamingInfo* pInfo = strStreamingEngine::GetInfo().GetStreamingInfo(streamingIndex);
			if(pInfo)
			{
				strStreamingFile* pFile = strPackfileManager::GetImageFileFromHandle(pInfo->GetHandle());

				if (pFile)
				{
					pMount = pFile->m_IsPatchfile ? pFile->m_mappedName.TryGetCStr() : pFile->m_name.TryGetCStr();
				}
			}
		}
	}
}
#endif

#define replayPreloadingDebugf	replayDebugf3
//////////////////////////////////////////////////////////////////////////
bool CReplayModelManager::LoadModel(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion, bool createUrgent, strModelRequest& streamingModelReq, strRequest& streamingReq, u32 flags)
{
	if(!CheckModel(mapTypeDef, oldVersion))
		return false;

	u32 archetypeStreamSlotIdx = fwArchetypeManager::GetArchetypeIndexFromHashKey(modelHash);
	bool ityp = false;
	if(archetypeStreamSlotIdx < fwModelId::MI_INVALID)
	{
		fwArchetype* pArcheType = fwArchetypeManager::GetArchetype(archetypeStreamSlotIdx);
		if(pArcheType->IsStreamedArchetype())
		{
			ityp = true;
		}
	}
	else
	{
		ityp = true;
	}

	if(ityp)
	{
		streamingModelReq.Request(mapTypeDef, modelHash, STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | flags);
		replayPreloadingDebugf("Preload request1 %u, %u, flags = %u", mapTypeDef.Get(), modelHash, flags);

		// Check whether we've failed on this before...
		if(m_failedStreamingRequests.Find(failStrModelInfo(streamingModelReq)) != -1)
			return !createUrgent;	// If its urgent then return false that we failed to load.  Else it's a preload request so return true so the request gets ditched.

		sysTimer timer;
		while(createUrgent && streamingModelReq.IsValid() && !streamingModelReq.HasLoaded())
		{
			if(timer.GetMsTime() >= m_modelLoadTimeout)
			{
#if __BANK
				const char* pName = "NoName";
				const char* pMount = "NoMount";

				fwModelId iModelId = CModelInfo::GetModelIdFromName(modelHash);
				if(iModelId.IsValid())
				{
					GetModelInfo(iModelId, pName, pMount);
					replayAssertf(false, "Failed to load model for (%u - %s - %s) after %f ms of trying. Requestid:%u", modelHash, pName, pMount, m_modelLoadTimeout, streamingModelReq.GetRequestId().Get());
				}
				else
				{
					replayAssertf(false, "Failed to load model for (%u - Unknown - %s) after %f ms of trying. Requestid:%u", modelHash, pMount, m_modelLoadTimeout, streamingModelReq.GetRequestId().Get());
				}
#endif
				m_modelLoadTimeout = REPLAY_LOAD_MODEL_TIME_FALLBACK;
				m_failedStreamingRequests.PushAndGrow(failStrModelInfo(streamingModelReq));
				return false;
			}

			CStreaming::LoadAllRequestedObjects();
		}

		bool wasValid = streamingModelReq.IsValid();
		if(!wasValid || !streamingModelReq.HasLoaded())
		{
			if(wasValid && !streamingModelReq.IsValid())
			{
				m_failedStreamingRequests.PushAndGrow(failStrModelInfo(modelHash, mapTypeDef.Get()));

#if __BANK
				const char* pName = "NoName";
				const char* pMount = "NoMount";
				fwModelId iModelId = CModelInfo::GetModelIdFromName(modelHash);
				if(iModelId.IsValid())
				{
					GetModelInfo(iModelId, pName, pMount);
					replayAssertf(false, "Failed to preload model as it doesn't appear to exist... (%u - %s - %s)", modelHash, pName, pMount);
				}
				else
				{
					replayAssertf(false, "Failed to load model for (%u - Unknown - %s)...unsure why. Requestid:%u", modelHash, pMount, streamingModelReq.GetRequestId().Get());
				}
#endif // __BANK
			}
			else
			{
				replayPreloadingDebugf("ITYP Model hasn't been loaded for object yet...");
			}
			return false;
		}
	}
	else
	{
		fwModelId iModelId = CModelInfo::GetModelIdFromName(modelHash);
		strLocalIndex transientLocalIdx = CModelInfo::AssignLocalIndexToModelInfo(iModelId);

		streamingReq.Request(transientLocalIdx, CModelInfo::GetStreamingModuleId(), STRFLAG_PRIORITY_LOAD | STRFLAG_FORCE_LOAD | flags);
		replayPreloadingDebugf("Preload request2  %u, flags = %u, %s", modelHash, flags, fwArchetypeManager::GetArchetypeName(iModelId.GetModelIndex()));

		// Check whether we've failed on this before...
		if(m_failedStreamingRequests.Find(failStrModelInfo(streamingReq)) != -1)
			return !createUrgent;	// If its urgent then return false that we failed to load.  Else it's a preload request so return true so the request gets ditched.

		sysTimer timer;
		while(createUrgent && streamingReq.IsValid() && !streamingReq.HasLoaded())
		{
			if(timer.GetMsTime() >= m_modelLoadTimeout)
			{
#if __BANK
				const char* pName = "NoName";
				const char* pMount = "NoMount";

				GetModelInfo(iModelId, pName, pMount);

				replayAssertf(false, "Failed to load model for (%u - %s - %s) after %f ms of trying. Requestid:%u", modelHash, pName, pMount, m_modelLoadTimeout, streamingReq.GetRequestId().Get());
#endif // __BANK
				m_modelLoadTimeout = REPLAY_LOAD_MODEL_TIME_FALLBACK;
				m_failedStreamingRequests.PushAndGrow(failStrModelInfo(streamingReq));
				return false;
			}

			CStreaming::LoadAllRequestedObjects();
		}

		if(!streamingReq.IsValid() || !streamingReq.HasLoaded())
		{
			replayPreloadingDebugf("Model hasn't been loaded for object yet...");
			return false;
		}
	}

	return true;
}


#endif // GTA_REPLAY
