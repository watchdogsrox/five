#ifndef _REPLAY_MODEL_MANAGER_H_
#define _REPLAY_MODEL_MANAGER_H_

#include "ReplaySettings.h"

#if GTA_REPLAY

#include "replay_channel.h"

#include "entity/archetypemanager.h"
#include "streaming/streamingrequest.h"
#include "streaming/streaming.h"

class CReplayModelManager
{
private:
	struct modelRequest
	{
		modelRequest()
			: m_lastPreloadTime(0)
			, m_modelHash(0xffffffff)
			, m_ModuleId(0xffffffff)
		{
			m_mapTypeDef.Invalidate();
		}
		modelRequest(u32 modelHash, strLocalIndex mapTypeDef)
			: m_modelHash(modelHash)
			, m_mapTypeDef(mapTypeDef)
			, m_lastPreloadTime(0)
			, m_ModuleId(0xffffffff)
		{}

		modelRequest(u32 modelHash, strLocalIndex localIndex, int moduleId)
			: m_lastPreloadTime(0)
			, m_modelHash(modelHash)
			, m_ModuleId(moduleId)
			, m_mapTypeDef(localIndex)
		{}

		void	Release()
		{
			if(m_strRequest.IsValid())
			{
				m_strRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
				m_strRequest.Release();
			}
			
			if(m_strModelRequest.IsValid())
			{
				m_strModelRequest.ClearRequiredFlags(STRFLAG_DONTDELETE);
				m_strModelRequest.Release();
			}

			m_lastPreloadTime = 0;
			m_modelHash = 0xffffffff;
			m_mapTypeDef.Invalidate();
		}

		bool	HasLoaded() /*const*/	// Can't be const'd due to HasLoaded in strRequest and strModelRequest
		{
			if(m_strModelRequest.IsValid())
			{
				return m_strModelRequest.HasLoaded();
			}
			if(m_strRequest.IsValid())
			{
				return m_strRequest.HasLoaded();
			}
			replayAssert(false);
			return false;
		}

		bool IsValid()
		{
			return m_strModelRequest.IsValid() || m_strRequest.IsValid();

		}

		bool	IsFree() const
		{
			return m_lastPreloadTime == 0 && m_modelHash == 0xffffffff && m_mapTypeDef.IsInvalid();
		}

		void	AddRef()	{	++m_refCount;	}
		bool	RemoveRef()	{	replayFatalAssertf(m_refCount != 0, "Decrementing past 0");	--m_refCount;	return m_refCount == 0;}

		strRequest		m_strRequest;
		strModelRequest m_strModelRequest;

		union
		{
			u32				m_lastPreloadTime;
			u32				m_refCount;
		};

		u32				m_modelHash;
		strLocalIndex	m_mapTypeDef;
		int				m_ModuleId;

		bool operator==(const modelRequest& other) const
		{
			return m_modelHash == other.m_modelHash && m_mapTypeDef.Get() == other.m_mapTypeDef.Get() && m_ModuleId == other.m_ModuleId;
		}
	};

public:

	void			Init(u32 preloadRequestSize);

	bool			PreloadRequest(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion, bool& requestIsLoaded, u32 currGameTime);
	bool			PreloadRequest(u32 modelHash, strLocalIndex localIndex, bool oldVersion, int streamingModuleId, bool& requestIsLoaded, u32 currGameTime);
private:
	modelRequest *	PreloadRequestInternal(modelRequest &inReq, bool &preloadSuccess, u32 currGameTime);
public:
	void			UpdatePreloadRequests(u32 time);

	void			AddLoadRequest(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion);
	void			RemoveLoadRequest(u32 modelHash, strLocalIndex mapTypeDef = strLocalIndex());
	void			FlushLoadRequests();

#if __BANK
	static void		GetModelInfo(fwModelId modelID, const char*& pName, const char*& pMount);
#endif // __BANK

	bool			LoadAsset(strLocalIndex requestID, int streamingModuleID, bool createUrgent, strRequest& request, u32 flags = 0);
	rage::fwModelId LoadModel(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion, bool createUrgent);
	bool			LoadModel(u32 modelHash, strLocalIndex mapTypeDef, bool oldVersion, bool createUrgent, strModelRequest& streamingModelReq, strRequest& streamingReq, u32 flags = 0);

private:




	atArray<modelRequest>	m_modelRequests;

	struct failStrModelInfo 
	{
		failStrModelInfo()
		{
			data.s1.hash = 0;
			data.s1.id = 0xFFFFFFFF;
		}
		failStrModelInfo(u32 hash, s32 id)
		{
			data.s1.hash = hash;
			data.s1.id = id;
		}
		failStrModelInfo(/*const*/ strModelRequest& req)
		{
			data.s1.hash = req.GetModelName().GetHash();
			data.s1.id = req.GetTypSlotIdx();
		}
		failStrModelInfo(const strRequest& req)
		{
			data.s1.hash = 0;
			data.s1.id = 0xFFFFFFFF;

			if(req.GetModuleId() != strRequest::INVALID_MODULE)
			{
				data.s2.index = req.GetIndex().Get();
				data.s2.moduleId = req.GetModuleId();
			}
		}

		bool operator==(const failStrModelInfo& other) const
		{
			return data.s1.hash == other.data.s1.hash && data.s1.id == other.data.s1.id;
		}

		union u
		{
			struct sm
			{
				u32 hash;
				s32 id;
			} s1;
			struct s
			{
				u32 index;
				u32 moduleId;
			} s2;
		} data;
		
	};
	atArray<failStrModelInfo>	m_failedStreamingRequests;

	float						m_modelLoadTimeout;
};


extern CReplayModelManager	s_replaySRLManager;

#endif // GTA_REPLAY

#endif // _REPLAY_MODEL_MANAGER_H_