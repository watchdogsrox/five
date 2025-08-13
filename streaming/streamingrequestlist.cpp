//
// streaming/streamingrequestlist.cpp
//
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//


#include "streaming/streaming_channel.h"

STREAMING_OPTIMISATIONS()


#include "streamingrequestlist.h"

#include <set>
#include "bank/bank.h"

#include "atl/map.h"
#include "camera/camInterface.h"
#include "camera/viewports/ViewportManager.h"
#include "Cutscene/CutsceneManagerNew.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "parser/manager.h"
#include "parser/treenode.h"
#include "renderer/PostScan.h"
#include "streaming/zonedassetmanager.h"
#include "system/FileMgr.h"
#include "scene/texLod.h"

#include "entity/archetypemanager.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "fwscene/stores/boxstreamersearch.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/maptypesstore.h"
#include "fwscene/world/WorldLimits.h"
#include "fwsys/fileExts.h"
#include "fwsys/metadatastore.h"
#include "fwsys/timer.h"
#include "grcore/debugdraw.h"
#include "network/live/livemanager.h"
#include "renderer/PostScan.h"
#include "scene/FocusEntity.h"
#include "scene/ipl/IplCullBox.h"
#include "scene/LoadScene.h"
#include "scene/streamer/SceneStreamerList.h"
#include "scene/streamer/SceneStreamerMgr.h"
#include "scene/world/GameScan.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaming.h"
#include "streaming/streaming_channel.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingrequestlisteditor.h"
#include "streaming/streamingvisualize.h"
#include "string/stringutil.h"

#include "streamingrequestlist_shared.h"
#include "streamingrequestlist_parser.h"


float	g_lastWriteTime;
s32		g_requestListIndex;
s32		g_removeListIndex;
s32		g_retryNeeded;
s32		g_requestListIndexPrestream;
bool	g_bHDTexLoadedThisFrame = false;

const s32 DEFAULT_STREAMING_AHEAD_TIME = 3;
const s32 DEFAULT_STREAMING_AHEAD_TIME_PRESTREAM = 3;
const s32 DEFAULT_LOADSCENE_AHEAD_TIME = 3;
const s32 DEFAULT_LOADSCENE_AHEAD_TIME_PRESTREAM = 3;

s32 g_streamingAheadTime = DEFAULT_STREAMING_AHEAD_TIME;						//load ahead for objects
s32 g_streamingAheadTimePrestream = DEFAULT_STREAMING_AHEAD_TIME_PRESTREAM;		//prestream load ahead for objects 
s32 g_loadsceneAheadTime = DEFAULT_LOADSCENE_AHEAD_TIME;						//load ahead for imaps
s32 g_loadsceneAheadTimePrestream = DEFAULT_LOADSCENE_AHEAD_TIME_PRESTREAM;		//load ahead for imaps on first frame

extern bool g_HACK_GlobalSRLSkipGC;
bool g_disableForGTAO = false;													//disables HD textures and zoned assets for the GTAO intro

CStreamingRequestList gStreamingRequestList;
static atMap<atHashString, s32> s_AddRefedMaps;	// A list of objIndices of map datas for which we incremented the ref count.
												// Key is the model hash, value is the objIndex of the map type

#if __BANK
// from streamingrequestlist_rec_dbg.h
extern char g_recordingFilename[128];
extern f32 g_BoxStreamerThreshold;
bool g_DisableHDTxdRequests = false;
#endif // __BANK

#if __ASSERT
static atMap<atHashString, bool> s_RequestedAssets;
#endif // __ASSERT

PARAM(nosrl, "disable SRLs");
PARAM(addsvframemarkers, "Add a SV marker for every time we switch to a new recorded frame");

CStreamingRequestFrame::~CStreamingRequestFrame()
{
}

void CStreamingRequestFrame::OnPostLoad()
{
}

void CStreamingRequestFrame::OnPostSave(parTreeNode* NOTFINAL_ONLY(pNode))
{
#if !__FINAL
	pNode->GetElement().AddAttribute("frameIndex", pNode->GetParent()->FindNumChildren() - 1);
#endif // !__FINAL
}

bool CStreamingRequestFrame::AddMapDataRefCount(atHashString modelName, fwModelId modelId)
{
	strLocalIndex mapTypeSlotIndex = modelId.GetMapTypeDefIndex();
	if(mapTypeSlotIndex != fwModelId::TF_INVALID)
	{
		// Did we already do it?
		if (s_AddRefedMaps.Access(modelName))
		{
			// Yeah, we're good.
			return true;
		}

		// Is this thing resident?
		if (strStreamingEngine::GetInfo().GetStreamingInfoRef(g_MapTypesStore.GetStreamingIndex(mapTypeSlotIndex)).GetStatus() != STRINFO_LOADED)
		{
			// Not yet - we can't add a ref!!
			return false;
		}

		// It's resident - add the ref count and check it off.
		g_MapTypesStore.AddRef(mapTypeSlotIndex, REF_SCRIPT);
		s_AddRefedMaps.Insert(modelName, mapTypeSlotIndex.Get());
		return true;
	}

	// Doesn't have a map type, so screw it.
	return true;
}

void CStreamingRequestFrame::RemoveMapDataRefCount(atHashString modelName, fwModelId modelId)
{
	strLocalIndex mapTypeSlotIndex = modelId.GetMapTypeDefIndex();
	if(mapTypeSlotIndex != fwModelId::TF_INVALID)
	{
		// Only remove the ref if we added it earlier.
		if (s_AddRefedMaps.Access(modelName))
		{
			Assert(*s_AddRefedMaps.Access(modelName) == mapTypeSlotIndex.Get());
			g_MapTypesStore.RemoveRef(mapTypeSlotIndex, REF_SCRIPT);	// If this fails, something is wrong - we added a ref earlier, so it must be resident.
			s_AddRefedMaps.Delete(modelName);
		}
	}
}

CStreamingRequestFrame::AssetLoadState CStreamingRequestFrame::ProcessRequestList(const CStreamingRequestRecord &record) 
{
	bool canSkipToNext = true;
	bool missingAssets = false;

	atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
	atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
	CreateFullRequestList(record, tempRequestList);

	for (int i = 0; i < tempRequestList.GetCount(); ++i) {
		fwModelId modelId;
		if (fwArchetypeManager::GetArchetypeFromHashKey(tempRequestList[i], modelId)) {
			//s32 objIndex = modelId.ConvertToStreamingIndex();
			bool isModelLoaded = CModelInfo::HaveAssetsLoaded(modelId);
			AddMapDataRefCount(tempRequestList[i], modelId);

			// Wait here until the models are all loaded.
			if(!isModelLoaded) {
				// Not loaded yet, okay. Is it at least requested?
				if (!CModelInfo::AreAssetsRequested(modelId)) {
					streamSRLDebugf1("SRL - %s needs to be requested", CModelInfo::GetBaseModelInfoName(modelId));
				} else {
					streamSRLDebugf2("SRL - still waiting for %s", CModelInfo::GetBaseModelInfoName(modelId));				
				}

				isModelLoaded = !CModelInfo::RequestAssets(modelId, STRFLAG_CUTSCENE_REQUIRED);

				if (!isModelLoaded)
				{
#if __ASSERT
					{
						USE_DEBUG_MEMORY();
						s_RequestedAssets[tempRequestList[i]] = true;
					}
#endif // __ASSERT
				}
				else
				{
					missingAssets = true;
				}
			}
			else
			{
				if (gStreamingRequestList.GetEnsureCutsceneRequired())
				{
					CModelInfo::SetAssetRequiredFlag(modelId, STRFLAG_CUTSCENE_REQUIRED);

	#if __ASSERT
					{
						USE_DEBUG_MEMORY();
						s_RequestedAssets[tempRequestList[i]] = true;
					}
	#endif // __ASSERT
				}
			}
			canSkipToNext &= isModelLoaded;
		}
	}

	BANK_ONLY(if (!g_DisableHDTxdRequests))
	{

		for (int i = 0; i < m_PromoteToHDList.GetCount(); ++i) {
			fwModelId modelId;
			fwArchetype *pArchetype = fwArchetypeManager::GetArchetypeFromHashKey(m_PromoteToHDList[i], modelId);

			if ( pArchetype ) {
				bool isModelLoaded = CModelInfo::HaveAssetsLoaded(modelId);
				if(isModelLoaded) {
					CBaseModelInfo *pMI = CModelInfo::GetBaseModelInfo(modelId);
					if(pMI){
						CTexLod::AddHDTxdRequest(pMI->GetAssetLocation(), pArchetype->GetStreamSlot().Get());
#if !RSG_PC
						g_bHDTexLoadedThisFrame &= pMI->GetAssetLocation().GetIsBoundHD();
#if __BANK
						if(!pMI->GetAssetLocation().GetIsBoundHD())
							streamSRLDebugf1("SRL (HDTex) - %s has not Loaded", CModelInfo::GetBaseModelInfoName(modelId));
#endif
#endif
					}
				}
			}
		}
	}

	return canSkipToNext ? (missingAssets ? ALL_REQUESTED_BUT_MISSING : ALL_RESIDENT) : NONRESIDENT_ASSETS;
}

void CStreamingRequestFrame::CreateFullRequestList(const CStreamingRequestRecord &record, atArray<atHashString> &outRequestList) const
{
	// Get the full count.
	int fullCount = m_AddList.GetCount();
	int commonCount = m_CommonAddSets.GetCount();

	for (int x=0; x<commonCount; x++)
	{
		fullCount += record.GetCommonSet(m_CommonAddSets[x]).GetCount();
	}

	outRequestList.Resize(fullCount);

	if (fullCount)
	{
		sysMemCpy(&outRequestList[0], m_AddList.GetElements(), sizeof(atHashString) * m_AddList.GetCount());

		int index = m_AddList.GetCount();
		for (int x=0; x<commonCount; x++)
		{
			const atArray<atHashString> &commonSet = record.GetCommonSet(m_CommonAddSets[x]);
			sysMemCpy(&outRequestList[index], commonSet.GetElements(), sizeof(atHashString) * commonSet.GetCount());
			index += commonSet.GetCount();
		}
	}
}

void CStreamingRequestFrame::SetListDeleteable()
{
	for (int i = 0; i < m_RemoveList.GetCount(); ++i) {
		fwModelId modelId;
		if (fwArchetypeManager::GetArchetypeFromHashKey(m_RemoveList[i], modelId)) {
			CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_CUTSCENE_REQUIRED);
			RemoveMapDataRefCount(m_RemoveList[i], modelId);

#if __ASSERT
			if (s_RequestedAssets.Access(m_RemoveList[i]))
			{
				s_RequestedAssets.Delete(m_RemoveList[i]);
			}
#endif // __ASSERT
		}
	}
}

void CStreamingRequestFrame::MakeRequestsDeleteable(CStreamingRequestRecord &record)
{
	atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
	atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
	CreateFullRequestList(record, tempRequestList);

	for (int i = 0; i < tempRequestList.GetCount(); ++i) {
		fwModelId modelId;
		if (fwArchetypeManager::GetArchetypeFromHashKey(tempRequestList[i], modelId)) {
			CModelInfo::ClearAssetRequiredFlag(modelId, STRFLAG_CUTSCENE_REQUIRED);
			RemoveMapDataRefCount(tempRequestList[i], modelId);

#if __ASSERT
			if (s_RequestedAssets.Access(tempRequestList[i]))
			{
				s_RequestedAssets.Delete(tempRequestList[i]);
			}
#endif // __ASSERT
		}
		else
		{
			Assertf(s_RequestedAssets.Access(tempRequestList[i]) == NULL, "Can't free up %s", tempRequestList[i].GetCStr());
		}
	}
}

bool CStreamingRequestFrame::AreAllLoadedOrUnrequested(const CStreamingRequestRecord &record) const
{
	atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
	atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
	CreateFullRequestList(record, tempRequestList);

	for (int i = 0; i < tempRequestList.GetCount(); ++i) {
		fwModelId modelId;
		
		if (CModelInfo::GetBaseModelInfoFromHashKey(tempRequestList[i], &modelId)){
			if(!CModelInfo::HaveAssetsLoaded(modelId) &&
				CModelInfo::AreAssetsRequested(modelId)){
					return false;
			}
		}
	}

	return true;
}

CStreamingRequestRecord::CStreamingRequestRecord()
: m_NewStyle(false)
{
}

CStreamingRequestRecord::~CStreamingRequestRecord()
{
	m_Frames.Reset();
}

void CStreamingRequestRecord::Clear()
{
	m_Frames.Reset();
}

class SRLMounter : public CDataFileMountInterface
{
	virtual bool LoadDataFile(const CDataFileMgr::DataFile & file)
	{
		switch(file.m_fileType)
		{
		case CDataFileMgr::STREAMING_REQUEST_LISTS_FILE:
			CStreamingRequestList::RegisterFromFile(file.m_filename);
		default:
			break;
		}
		return true;
	}

	virtual void UnloadDataFile(const CDataFileMgr::DataFile &file) 
	{
		switch(file.m_fileType)
		{
		case CDataFileMgr::STREAMING_REQUEST_LISTS_FILE:
			CStreamingRequestList::UnregisterFromFile(file.m_filename);
		default:
			break;
		}
	}

} g_SRLMounter;

//
// name:		CStreamingRequestList::CStreamingRequestList
// description:	Constructor
CStreamingRequestList::CStreamingRequestList()
{
	m_AllAssetsLoaded = true;
	m_stateFlags = 0;
	m_IsRunFromScript = false;
	m_IsLongJumpMode = false;
	m_PrestreamMode = SRL_PRESTREAM_DEFAULT;

	m_PostCutsceneCameraPos.ZeroComponents();

#if __BANK
	m_ShowStatus = true;
	m_SubstringSearch[0] = 0;
	m_ShowRequestDetails = false;
	m_ShowNonResidentOnly = true;
	m_Modified = false;
	m_DisableSRL = false;
	m_KeepSceneStreamerOn = false;
	m_SkipPrestream = false;
	m_ShowCurrentFrameInfo = false;
	m_ThoroughRecording = true;
	m_AddSVFrameMarkers = false;
	m_IsFramesOverride = false;
	m_framesAhead = DEFAULT_STREAMING_AHEAD_TIME;
	m_searchFramesAhead = DEFAULT_LOADSCENE_AHEAD_TIME;
	m_SkipTimedEntities = false;
	m_DisableHDTexRequests = false;
#endif // __BANK

#if USE_SRL_PROFILING_INFO
	m_ProfileLists = false;
	m_ShowProfileInfo = true;
	m_ShowReportAtEnd = false;
#endif // USE_SRL_PROFILING_INFO
}

void CStreamingRequestList::RegisterFromFile(const char* fileName)
{
	if (!CStreaming::ShouldLoadStaticData())
	{
		return;
	}
	CStreamingRequestMasterList masterList;
	Displayf("Loading SRL file %s", fileName);
	if (streamSRLVerifyf(PARSER.LoadObject(fileName, "meta", masterList), "Unable to load streaming request list file: %s",fileName))
	{
		// Get all those files and register them.
		for (int x=0; x<masterList.m_Files.GetCount(); x++)
		{
			const char *name = masterList.m_Files[x].c_str();

			streamSRLAssertf(strlen(name) > 4 && stricmp(&name[strlen(name)-4], "_srl") == 0,
				"A streaming request list resource must end with '_srl' - check %s", name);

			char rawFileName[RAGE_MAX_PATH];
			char finalName[RAGE_MAX_PATH];
			formatf(rawFileName, "%s.%s", name, META_FILE_EXT);
			DATAFILEMGR.ExpandFilename(rawFileName,finalName,RAGE_MAX_PATH);
			strPackfileManager::RegisterIndividualFile(finalName, true, ASSET.FileName(finalName), false,true);
		}
	}
}

void CStreamingRequestList::UnregisterFromFile(const char* fileName)
{
	if (!CStreaming::ShouldLoadStaticData())
	{
		return;
	}
	CStreamingRequestMasterList masterList;
	if (streamVerifyf(PARSER.LoadObject(fileName, "meta", masterList), "Unable to load streaming request list file: %s",fileName))
	{
		// Get all those files and register them.
		for (int x=0; x<masterList.m_Files.GetCount(); x++)
		{
			const char *name = masterList.m_Files[x].c_str();

			streamSRLAssertf(strlen(name) > 4 && stricmp(&name[strlen(name)-4], "_srl") == 0,
				"A streaming request list resource must end with '_srl' - check %s", name);

			char rawFileName[RAGE_MAX_PATH] = {0};
			char finalName[RAGE_MAX_PATH] = {0};
			formatf(rawFileName, "%s.%s", name, META_FILE_EXT);
			DATAFILEMGR.ExpandFilename(rawFileName,finalName,RAGE_MAX_PATH);
			strPackfileManager::InvalidateIndividualFile(finalName);
		}
	}
}

void CStreamingRequestList::Init(unsigned initMode)
{
	if (initMode == INIT_SESSION)
	{
		CDataFileMount::RegisterMountInterface(CDataFileMgr::STREAMING_REQUEST_LISTS_FILE, &g_SRLMounter,eDFMI_UnloadFirst);
		if (CStreaming::ShouldLoadStaticData())
		{
			const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::STREAMING_REQUEST_LISTS_FILE);

			while(DATAFILEMGR.IsValid(pData))
			{
				if(!pData->m_disabled)
				{
					RegisterFromFile(pData->m_filename);
				}
				pData = DATAFILEMGR.GetNextFile(pData);
			}
		}
	}
}

void CStreamingRequestList::Shutdown(unsigned initMode)
{
	if (initMode == INIT_CORE || (initMode == INIT_SESSION && CStreaming::GetReloadPackfilesOnRestart()))
	{

	}
}

//
// name:		CStreamingRequestList::EnableRecording
// description:	Enable recording if we have started recording
void CStreamingRequestList::EnableRecording(bool BANK_ONLY(bRecord))
{
#if __BANK
	if(bRecord)
		m_stateFlags |= STRREQ_RECORDING_ENABLED;
	else
		m_stateFlags &= ~STRREQ_RECORDING_ENABLED;
#endif // __BANK
}

//
// name:		CStreamingInfoManager::StartRecordingStreamingRequests
// description:	Start recording streaming requests
#if SRL_ENABLED
void CStreamingRequestList::BeginPrestream(const char* cutsceneName, bool fromScript)
{
	Assertf(!m_IsRunFromScript || !fromScript, "Calling PREFETCH_SRL on %s, but there's already a scripted SRL active. PREFETCH_SRL should only be called once per cutscene, and then released with END_SRL once the cutscene is over.", cutsceneName);
	// There may be a left-over previous recording if we were currently showing the generated
	// report of a profiled cutscene playback.
	if(!(m_stateFlags & STRREQ_REPLAY_ENABLED))//if we are enabled for replay then we don't  call this 
		DeleteRecording();

	m_AllAssetsLoaded = false;
	m_IsRunFromScript = fromScript;

	m_stateFlags |= STRREQ_STARTED | STRREQ_PAUSED;
#if __BANK
	if (m_DisableSRL)
	{
		m_stateFlags &= ~STRREQ_STARTED;
		// Don't even load anything if SRLs are disabled.
		return;
	}

	if(m_stateFlags & STRREQ_RECORD)
	{
		StartRecord(cutsceneName);
		m_stateFlags |= STRREQ_RECORD_IN_PROGRESS;
		CIplCullBox::SetEnabled(false);

		CutSceneManager::GetInstance()->SetExternalTimeStep(1.0f / 30.0f);
		CutSceneManager::GetInstance()->SetUseExternalTimeStep(true);
	}
	else
#endif // __BANK
	{
		StartPlayback(cutsceneName);
	}
}
#else
void CStreamingRequestList::BeginPrestream(const char*){}
#endif	//SRL_ENABLED

void CStreamingRequestList::Start()
{
	m_stateFlags &= ~STRREQ_PAUSED;
}

void CStreamingRequestList::Pause()
{
	m_stateFlags |= STRREQ_PAUSED;
}


#if SRL_ENABLED

//
// name:		CStreamingInfoManager::FinishRecordingStreamingRequests
// description:	Finish recording streaming requests
void CStreamingRequestList::Finish()
{
	const bool bIsPlaybackActive = IsPlaybackActive();

	m_PostCutsceneCameraPos.ZeroComponents();

	if (m_CurrentRecord)
	{
		g_streamingAheadTime = DEFAULT_STREAMING_AHEAD_TIME;
		g_streamingAheadTimePrestream = DEFAULT_STREAMING_AHEAD_TIME_PRESTREAM;
		g_loadsceneAheadTime = DEFAULT_LOADSCENE_AHEAD_TIME;
		g_loadsceneAheadTimePrestream = DEFAULT_LOADSCENE_AHEAD_TIME_PRESTREAM;
		m_IsLongJumpMode = false;
		m_PrestreamMode = SRL_PRESTREAM_DEFAULT;
	}

	g_HACK_GlobalSRLSkipGC = false;
	m_stateFlags &= ~STRREQ_STARTED;

	if(m_stateFlags & STRREQ_RECORD_IN_PROGRESS)
	{
		FinishRecord();
	}
	else
	{
		FinishPlayback(bIsPlaybackActive);
	}

	m_IsRunFromScript = false;
}

#else
void CStreamingRequestList::Finish(){}
#endif	//SRL_ENABLED

//
// name:		CStreamingRequestList::Update
// description:	Update streaming request list if active
void CStreamingRequestList::Update()
{
#if __BANK
	UpdateRecordAll();
#endif // __BANK

	if(IsActive())
	{
		// If we're run from a script, we have to keep track of time ourselves.
		if (m_IsRunFromScript)
		{

			if (m_stateFlags & STRREQ_PAUSED && !(m_stateFlags & STRREQ_REPLAY_ENABLED))
			{
				m_time = 0.0f;
			}
			else
			{
				// In case the script doesn't update it for us (which it should), we'll update ourselves. We just assume that each tick is 1/30 of a second,
				// which is good enough as a general guideline. The script needs to sychronize via SET_SRL_TIME, or else it will never work. This tick here will
				// just keep us afloat if the script forgets to tick for a frame or two.
				m_time += TIME.GetSeconds();
			}
		}

		if(m_stateFlags & STRREQ_RECORD_IN_PROGRESS)
		{
			UpdateRecord(m_time);
		}
		else
		{
			UpdatePlayback(m_time);
		}
	}

#if __BANK
	DebugDraw();
#endif // __BANK

#if USE_SRL_PROFILING_INFO
	if (m_CurrentRecord && m_ShowReportAtEnd && m_ProfilingInfo)
	{
		m_CurrentRecord->DisplayReport(*m_ProfilingInfo);
	}
#endif // USE_SRL_PROFILING_INFO
}


//
// name:		CStreamingRequestList::StartPlayback
// description:	Start playback of streaming request list
void CStreamingRequestList::StartPlayback(const char* cutsceneName)
{
	if(cutsceneName)
	{
		if(!LoadRecording(cutsceneName))
		{
			m_stateFlags &= ~STRREQ_STARTED;
			return;
		}
	}
	else
	{
		m_stateFlags &= ~STRREQ_STREAMING; //we are not streaming the srl. it should already be resident in memory
	}	
	g_lastWriteTime = 0.0f;
	g_requestListIndex = 0;
	g_removeListIndex = 0;
	g_requestListIndexPrestream = 0;
	g_retryNeeded = -1;
}

//
// name:		CStreamingRequestList::FinishPlayback
// description:	Finish playback of streaming request list
void CStreamingRequestList::FinishPlayback(bool bResetFocus)
{
	streamSRLDisplayf("Finish playback of SRL");

#if STREAMING_VISUALIZE
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	STRVIS_SET_MARKER_TEXT("End SRL playback");
#endif // STREAMING_VISUALIZE

	//during replay playback don't touch the scene streamer
	if((m_stateFlags & STRREQ_REPLAY_ENABLED) == 0)
		g_SceneStreamerMgr.GetStreamer()->SetEnableIssueRequests(true);

	if (bResetFocus)
	{
		CFocusEntityMgr::GetMgr().SetDefault();
	}

#if __PS3 || __XENON
	CZonedAssetManager::SetEnabled(true);	
#else
	if (g_disableForGTAO){
		CZonedAssetManager::SetEnabled(true);	
	}
#endif //__PS3 || __XENON

	if (g_disableForGTAO == true)
	{
		CTexLod::EnableStateSwapper(true);		
		g_disableForGTAO  = false;
	}
	

	CIplCullBox::SetEnabled(true);

	// Be sure to unpin all interiors.
	atMap<CInteriorProxy *, int>::Iterator it = m_PinnedInteriors.CreateIterator();

	for (it.Start(); !it.AtEnd(); it.Next())
	{
		it.GetKey()->SetRequestedState(CInteriorProxy::RM_CUTSCENE, CInteriorProxy::PS_NONE);
	}

	m_PinnedInteriors.Reset();
	

	if (m_CurrentRecord)
	{
		int frameCount = m_CurrentRecord->GetFrameCount();

		streamSRLDisplayf("Freeing up all SRL requests");

		m_CurrentRecord->GetFrame(frameCount - 1).SetListDeleteable();

#if USE_SRL_PROFILING_INFO
		// If we want a report at the end, don't delete this object yet! We'll need it for the report!
		if (m_ShowReportAtEnd && m_ShowProfileInfo && m_ProfilingInfo)
		{
			m_CurrentRecord->GenerateProfileReport(*m_ProfilingInfo);
		}
		else
#endif // USE_SRL_PROFILING_INFO
		{
			DeleteRecording();
		}
	}
	else
	{
		streamSRLDisplayf("No active SRL");

		// The recording might still be in the process of being streamed.
		if (m_Index.IsValid())
		{
			streamSRLDisplayf("SRL presumed in flight - removing now");
			strStreamingEngine::GetInfo().ClearRequiredFlag(m_Index, STRFLAG_CUTSCENE_REQUIRED);
			strStreamingEngine::GetInfo().RemoveObject(m_Index);
			m_Index = strIndex();
		}
	}

#if __ASSERT
	if (s_RequestedAssets.GetNumUsed())
	{
		streamSRLErrorf("First leak: %s", s_RequestedAssets.CreateIterator().GetKey().GetCStr());
	}

	streamSRLAssertf(s_RequestedAssets.GetNumUsed() == 0, "SRL is leaking %d asset(s). This is a memory leak.", s_RequestedAssets.GetNumUsed());
#endif // __ASSERT

	// Remove all remaining references to maps. There shouldn't be any, the final list deletion should have cleared them all
	// out, but I'd rather be safe than have a memory leak.
	if (s_AddRefedMaps.GetNumUsed() > 0)
	{
		atMap<atHashString, s32>::Iterator entry = s_AddRefedMaps.CreateIterator();
		for (entry.Start(); !entry.AtEnd(); entry.Next())
		{
			streamSRLErrorf("Left-over reference to %s (map type %d)", entry.GetKey().TryGetCStr(), entry.GetData());
			g_MapTypesStore.RemoveRef(strLocalIndex(entry.GetData()), REF_SCRIPT);
		}

		streamSRLAssertf(false, "There are %d map(s) with SRL-based ref counts. There shouldn't be any, but that's okay, we can clean them up.",s_AddRefedMaps.GetNumUsed());

		s_AddRefedMaps.Reset();
	}
}

void CStreamingRequestList::SetSrlReadAheadTimes(int prestreamMap, int /*prestreamAssets*/, int playbackMap, int playbackAssets)
{
	g_streamingAheadTime = (playbackAssets == -1) ? DEFAULT_STREAMING_AHEAD_TIME : playbackAssets;
	g_loadsceneAheadTime = (playbackMap == -1) ? DEFAULT_LOADSCENE_AHEAD_TIME : playbackMap;
	g_loadsceneAheadTimePrestream = (prestreamMap == -1) ? DEFAULT_LOADSCENE_AHEAD_TIME_PRESTREAM : prestreamMap;
	if(g_loadsceneAheadTimePrestream < DEFAULT_LOADSCENE_AHEAD_TIME_PRESTREAM)
	{
		g_streamingAheadTimePrestream = g_loadsceneAheadTimePrestream;
	}
	else{
		g_streamingAheadTimePrestream = DEFAULT_STREAMING_AHEAD_TIME_PRESTREAM;
	}
	 
}

void CStreamingRequestList::SetPostCutsceneCamera(Vec3V_In camPos, Vec3V_In camDir)
{
	m_PostCutsceneCameraPos = camPos;
	m_PostCutsceneCameraDir = camDir;

	STRVIS_SET_MARKER_TEXT("** POST CUTSCENE INFO SET ** %.2f %.2f .%2f", camPos.GetXf(), camPos.GetYf(), camPos.GetZf());
}

void CStreamingRequestList::DeleteRecording()
{
	if (m_CurrentRecord && (m_stateFlags & STRREQ_STARTED))
	{
		// Be sure to clean it up. Note that this will call DeleteRecording() again,
		// but that time with the stateFlags not set to STRREQ_STARTED, so that's cool.
		Finish();
	}

	// Did we stream it in?
	if (m_Index.IsValid())
	{
		streamSRLDisplayf("Removing SRL resource");
		
		if (m_CurrentRecord)
		{
			streamSRLDisplayf("SRL unref");
			g_fwMetaDataStore.RemoveRef(g_fwMetaDataStore.GetObjectIndex(m_Index), REF_SCRIPT);
			g_fwMetaDataStore.RemoveRef(g_fwMetaDataStore.GetObjectIndex(m_Index), REF_SCRIPT);
		}
		strStreamingEngine::GetInfo().ClearRequiredFlag(m_Index, STRFLAG_CUTSCENE_REQUIRED);
		strStreamingEngine::GetInfo().RemoveObject(m_Index);
		m_Index = strIndex();
	}
	else
	{
		//if we are recording for replay then we should infact be being old style SRLS 
		//so ignore the assert below
		if(!(m_stateFlags & STRREQ_REPLAY_ENABLED))
		{		
			streamSRLAssertf(!m_CurrentRecord || (m_stateFlags & STRREQ_RECORD), "We shouldn't have old-school SRLs anymore");			
		}
		// Or did we create it ourselves?
		delete m_CurrentRecord;
	}

	m_CurrentRecord = NULL;

#if USE_SRL_PROFILING_INFO
	USE_DEBUG_MEMORY();
	delete m_ProfilingInfo;
	m_ProfilingInfo = NULL;
#endif // USE_SRL_PROFILING_INFO
}

//
// name:		CStreamingRequestList::UpdatePlayback
// description:	Update playback of streaming request list
void CStreamingRequestList::UpdatePlayback(float time)
{
	// Have we finished streaming yet?
	if (m_stateFlags & STRREQ_STREAMING)
	{
		strStreamingInfo &info = strStreamingEngine::GetInfo().GetStreamingInfoRef(m_Index);

		streamSRLAssertf(info.GetStatus() != STRINFO_NOTLOADED,
			"SRL %s is trying to play, but not streaming", strStreamingEngine::GetObjectName(m_Index));

		if (info.GetStatus() != STRINFO_LOADED)
		{
#if __BANK
			if (gStreamingRequestList.IsShowStatus())
			{
				grcDebugDraw::AddDebugOutput("Waiting for Streaming Request List to stream...");
			}
#endif // __BANK

			// Still waiting...
			return;
		}

#if STREAMING_VISUALIZE
		STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
		STRVIS_SET_MARKER_TEXT("Begin SRL playback");
#endif // STREAMING_VISUALIZE

		// We finished streaming - let's grab the data.
		strLocalIndex objIndex = g_fwMetaDataStore.GetObjectIndex(m_Index);
		m_CurrentRecord = g_fwMetaDataStore.Get(objIndex)->GetObject<CStreamingRequestRecord>();
		streamSRLAssert(m_CurrentRecord);

		g_fwMetaDataStore.AddRef(objIndex, REF_SCRIPT);
		g_fwMetaDataStore.AddRef(objIndex, REF_SCRIPT);
		
		streamSRLDisplayf("Got SRL asset, ref'd");

		m_stateFlags &= ~STRREQ_STREAMING;

		Assertf(m_CurrentRecord->IsNewStyle(), "Old-style SRLs are no longer supported");

		int frameCount = m_CurrentRecord->GetFrameCount();

		if (frameCount == 0)
		{
			streamSRLWarningf("SRL is empty");
			// Actually empty - let's ignore it then.
			m_stateFlags &= ~STRREQ_STARTED;
			DeleteRecording();
			return;
		}

#if !__FINAL
		for (int x=0; x<frameCount; x++)
		{
			if (m_CurrentRecord->GetFrame(x).GetRequestList().GetCount() > 10000)
			{
				streamSRLAssertf(false, "Streaming request list %s seems to contain garbage data. I'll disable it so it won't crash. The PSO reading probably went wrong.",
					g_fwMetaDataStore.GetName(objIndex));

				m_stateFlags &= ~STRREQ_STARTED;
				DeleteRecording();
				return;
			}
		}
#endif // !__FINAL

#if USE_SRL_PROFILING_INFO
		Assert(!m_ProfilingInfo);
		{
			USE_DEBUG_MEMORY();
			m_ProfilingInfo = rage_new CStreamingRequestListProfilingInfo();
			m_ProfilingInfo->m_PerFrameProfileInfo.Resize(m_CurrentRecord->GetFrameCount());
		}
#endif // USE_SRL_PROFILING_INFO

		STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
		STRVIS_SET_MARKER_TEXT("SRL prestreamed");
	}

	STRVIS_SET_CONTEXT(strStreamingVisualize::SRL);

	streamSRLAssert(m_CurrentRecord);
	// If this is a new-style SRL, use the new-style playback.
	//if (m_CurrentRecord->IsNewStyle())
	{
		UpdateNewStylePlayback(time);
	}

	STRVIS_SET_CONTEXT(strStreamingVisualize::NONE);
}

bool CStreamingRequestList::IsCamPosValid(Vec3V_In camPos) const
{
	if (MagSquared(camPos).Getf() < 1.0f)
	{
		return false;
	}

	return g_WorldLimits_AABB.ContainsPoint(camPos);
}


atFixedArray<fwBoxStreamerSearch, 10> s_BoxSearches;

void CStreamingRequestList::ComputeSearchesAndFocus()
{
	s_BoxSearches.Resize(0);

	if (m_CurrentRecord)
	{
		atFixedArray<fwBoxStreamerSearch, 10>& searchList = s_BoxSearches;

#if __BANK
		const float thresholdSquared = (m_stateFlags & STRREQ_PAUSED) ? LONG_CAMERA_CUT_THRESHOLD_PRESTREAM * LONG_CAMERA_CUT_THRESHOLD_PRESTREAM : g_BoxStreamerThreshold * g_BoxStreamerThreshold;
#else
		const float thresholdSquared = (m_stateFlags & STRREQ_PAUSED) ? LONG_CAMERA_CUT_THRESHOLD_PRESTREAM * LONG_CAMERA_CUT_THRESHOLD_PRESTREAM : LONG_CAMERA_CUT_THRESHOLD * LONG_CAMERA_CUT_THRESHOLD;
#endif 
		int frameCounter = TIME.GetFrameCount();

		// The first frame is used for pre-streaming, so let's use that. Otherwise, during playback we'll want the subsequent ones.
		int firstFrame = (g_removeListIndex == 0) ? 0 : g_removeListIndex + 1;

		int readAhead = firstFrame ? g_loadsceneAheadTimePrestream : g_loadsceneAheadTime;
#if __BANK
		if(m_IsFramesOverride)
		{
			readAhead = m_searchFramesAhead;
		}
#endif

		int maxLoadScene = Min(firstFrame + readAhead, m_CurrentRecord->GetFrameCount());
		Vec3V focusPos = VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPos());
		int minFrameForBoxSearch = 0;
		int maxFrameForBoxSearch = 0x7fffffff;
		bool focusOverride = false;

		fwInteriorLocation loc = CPortalVisTracker::GetInteriorLocation();
		bool isInside = (loc.IsValid());

		m_bValidInteriorHint = false;

		// Are we currently inside an interior?
		if (IsPlaybackActive())
		{
			Vec3V curCamPos = VECTOR3_TO_VEC3V(camInterface::GetPos());


			if (isInside)
			{
#if STREAMING_VISUALIZE
//				CInteriorProxy* pIntProxy = reinterpret_cast<CInteriorProxy*>(pNode->GetPtr());
//				STRVIS_SET_MARKER_TEXT("Start Frame %d at %.2f: Inside interior (%s) at %.2f %.2f", g_removeListIndex, m_time / 1000.f, pIntProxy->GetName().GetCStr(), curCamPos.GetXf(), curCamPos.GetYf());
//				STRVIS_SET_MARKER_TEXT("Start Frame %d at %.2f: Inside interior (%d) at %.2f %.2f %.2f", g_removeListIndex, m_time / 1000.f, loc.GetRoomIndex(), curCamPos.GetXf(), curCamPos.GetYf(), curCamPos.GetZf());
#endif // STREAMING_VISUALIZE

				// We're currently inside an interior.
				// Being inside is a special case, IF we're also about to make a long camera cut.
				// Let's look at the upcoming cuts and see if there's a long cut.
				for (int x=firstFrame; x<maxLoadScene; x++)
				{
					Vec3V nextPos = m_CurrentRecord->GetFrame(x).GetCamPos();

					if (IsCamPosValid(nextPos) && DistSquared(curCamPos, nextPos).Getf() >= thresholdSquared)
					{
						bool nextIsInside = m_CurrentRecord->GetFrame(x).IsInside();

						if (!m_CurrentRecord->GetFrame(x).HasExtraInfo())
						{
							// Now, special case - what if we're cutting into another interior?
							// In that case, pretend it didn't happen. Interior-to-interior can happen when the camera sneaks into an interior.
							fwIsSphereIntersecting nextIntersection(spdSphere(nextPos, ScalarV(V_FLT_SMALL_1)));
							fwPtrListSingleLink	nextScanList;
							CInteriorProxy::FindActiveIntersecting(nextIntersection, nextScanList);

							fwPtrNode* nextNode = nextScanList.GetHeadPtr();
							nextIsInside = nextNode != NULL && nextNode->GetPtr() != NULL;
						}

						if (!nextIsInside)
						{
							// Yes, there's a long camera cut coming up.
							// First off, we need to make sure the current focus is set to the next shot and we pin the current interior.
							// Don't add any more searches until the focus has been set.
							Vec3V currentFocus = VECTOR3_TO_VEC3V(CFocusEntityMgr::GetMgr().GetPos());
							focusOverride = true;

							const bool bLeadOut = !( g_requestListIndex < m_CurrentRecord->GetFrameCount()-1 ) ;
							if ( !bLeadOut && (DistSquared(currentFocus, nextPos).Getf() > 0.0001f))
							{
								// Let's set the focus position first before adding more searches.
								CFocusEntityMgr::GetMgr().SetPosAndVel(RCC_VECTOR3(nextPos), ORIGIN);
								maxFrameForBoxSearch = x - 1;
								break;
							}

							// We can add more box searches, but only after the camera cut.
							minFrameForBoxSearch = x;
							break;
						}
					}
				}
			}
		}

		for (int loadSceneFrame = firstFrame; loadSceneFrame < maxLoadScene-1; loadSceneFrame++)
		{
			CStreamingRequestFrame &frame = m_CurrentRecord->GetFrame(loadSceneFrame);
			Vec3V futureCamPos = frame.GetCamPos();
			if (IsCamPosValid(futureCamPos))
			{
				bool nextIsInside = m_CurrentRecord->GetFrame(loadSceneFrame).IsInside();

				// Find all the interiors there.
				fwIsSphereIntersecting intersection(spdSphere(futureCamPos, ScalarV(V_FLT_SMALL_1)));
				fwPtrListSingleLink	scanList;
				CInteriorProxy::FindActiveIntersecting(intersection, scanList);

				fwPtrNode* pNode = scanList.GetHeadPtr();

				if (!m_CurrentRecord->GetFrame(loadSceneFrame).HasExtraInfo())
				{
					nextIsInside = pNode != NULL;
				}

				// Are we about to go into an interior?
				if (IsPlaybackActive() && maxFrameForBoxSearch == 0x7fffffff && minFrameForBoxSearch == 0 && loadSceneFrame > 0)
				{
					// Is this position in an interior?
					if (nextIsInside)
					{
						//CInteriorProxy* pIntProxy = reinterpret_cast<CInteriorProxy*>(pNode->GetPtr());
						//STRVIS_SET_MARKER_TEXT("Frame %d: Inside interior (%s) at %.2f %.2f %.2f", loadSceneFrame, pIntProxy->GetName().GetCStr(), futureCamPos.GetXf(), futureCamPos.GetYf(), futureCamPos.GetZf());

						// Is this going to be a camera cut? If so, just pin the interior, don't do a full box search there.
						Vec3V prevPos = m_CurrentRecord->GetFrame(loadSceneFrame-1).GetCamPos();

						if (DistSquared(futureCamPos, prevPos).Getf() >= thresholdSquared)
						{
							// We did. That means we need to stop adding box searches, or else the pools might blow over.
							// We'll continue to pin the interior, so that's fine.
							maxFrameForBoxSearch = loadSceneFrame - 1;
						}
					}
				}

				// try to pin all these interiors
				bool bPinnedInteriors = false;
				while(pNode != NULL)
				{
					// for each found proxy, pull in the .imap file and collisions
					CInteriorProxy* pIntProxy = reinterpret_cast<CInteriorProxy*>(pNode->GetPtr());
					if (pIntProxy )
					{
						pIntProxy->SetRequestedState(CInteriorProxy::RM_CUTSCENE, CInteriorProxy::PS_FULL);
						m_PinnedInteriors[pIntProxy] = frameCounter;
						bPinnedInteriors = true;
					}

					pNode = pNode->GetNextPtr();
				}

				//////////////////////////////////////////////////////////////////////////
				// generate hint for interior we're about to cut to - for drawable creation
				if (!m_bValidInteriorHint && bPinnedInteriors)
				{
					CInteriorProxy* targetInteriorProxy = NULL;
					s32 probedRoomIdx =	-1;

					bool bProbeRet = CPortalTracker::ProbeForInteriorProxy(VEC3V_TO_VECTOR3(futureCamPos), targetInteriorProxy, probedRoomIdx, NULL, CPortalTracker::FAR_PROBE_DIST);

					if (bProbeRet && targetInteriorProxy)
					{
						m_posInteriorHint = futureCamPos;
						m_bValidInteriorHint = true;
						m_locInteriorHint = CInteriorProxy::CreateLocation(targetInteriorProxy, probedRoomIdx);
					}
				}
				//////////////////////////////////////////////////////////////////////////


				// How far are we from the focus position?

				// Request the mapdata, UNLESS for long camera cuts. We can't fit two scenes in memory at the same time.
				if (IsPlaybackActive() || (GetPrestreamMode() != SRL_PRESTREAM_FORCE_OFF && GetPrestreamMode() != SRL_PRESTREAM_FORCE_COMPLETELY_OFF && (GetPrestreamMode() == SRL_PRESTREAM_FORCE_ON || DistSquared(focusPos, futureCamPos).Getf() < thresholdSquared)))
				{
					//STRVIS_SET_MARKER_TEXT("Add frame %d (%d-%d): %.2f %.2f", loadSceneFrame, minFrameForBoxSearch, maxFrameForBoxSearch, frame.GetCamPos().GetXf(), frame.GetCamPos().GetYf());
					if (loadSceneFrame >= minFrameForBoxSearch && loadSceneFrame <= maxFrameForBoxSearch)
					{
						fwBoxStreamerSearch search(
							frame.GetCamPos(),
							fwBoxStreamerSearch::TYPE_SRL,
							fwBoxStreamerAsset::MASK_MAPDATA,
							true
							);

						if (m_IsLongJumpMode)
						{
							search.SetCamPosAndDir(frame.GetCamPos(), frame.GetCamDir());
							search.SetIgnoreCulling(true);
						}

						searchList.Append() = search;
					}
				}
			}
		}

		// Now unpin every interior that wasn't needed this frame.
		atMap<CInteriorProxy *, int>::Iterator it = m_PinnedInteriors.CreateIterator();
		atFixedArray<CInteriorProxy *, 1024> staleInteriors;

		for (it.Start(); !it.AtEnd(); it.Next())
		{
			if (it.GetData() != frameCounter)
			{
				// Stale interior.
				it.GetKey()->SetRequestedState(CInteriorProxy::RM_CUTSCENE, CInteriorProxy::PS_NONE);
				staleInteriors.Append() = it.GetKey();
			}
		}

		// Finally, remove the entries from the map.
		int count = staleInteriors.GetCount();

		for (int x=0; x<count; x++)
		{
			m_PinnedInteriors.Delete(staleInteriors[x]);
		}

		if (!focusOverride && IsPlaybackActive())
		{
			if(g_requestListIndex < m_CurrentRecord->GetFrameCount() - 1)
			{
				CFocusEntityMgr::GetMgr().SetPosAndVel(camInterface::GetPos(), camInterface::GetVelocity());
			}
		}
	}
}

void CStreamingRequestList::AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags)
{
	if (supportedAssetFlags & fwBoxStreamerAsset::MASK_MAPDATA)
	{
		if(!(m_stateFlags & STRREQ_RECORD))
		{
			for (int x=0; x<s_BoxSearches.GetCount(); x++)
			{
				searchList.Grow() = s_BoxSearches[x];
			}
		}
	}
}

bool CStreamingRequestList::IsAboutToFinish() const
{
	return g_requestListIndex >= m_CurrentRecord->GetFrameCount() - 1;
}


void CStreamingRequestList::UpdateNewStylePlayback(float time)
{
	if(!m_CurrentRecord){
		return;
	}
	int frameCount = m_CurrentRecord->GetFrameCount();
	g_bHDTexLoadedThisFrame =  true;
	BANK_ONLY(g_DisableHDTxdRequests = m_DisableHDTexRequests);

	// During prestreaming, we only request stuff in the current frame, we don't unrequest anything
	// or advance time.
	if (m_stateFlags & STRREQ_PAUSED)
	{
		// Handle branching cutscenes!
		g_lastWriteTime = time;
		g_removeListIndex = g_requestListIndex = (s32)(time / (float) STREAMING_REQUEST_UPDATE);
		g_retryNeeded = -1;

		if (g_requestListIndex < frameCount)
		{
			if (GetPrestreamMode() != SRL_PRESTREAM_FORCE_COMPLETELY_OFF)
			{
				//preload the first few frames of data if required
				g_requestListIndexPrestream = Max(g_requestListIndex, g_requestListIndexPrestream);
				if (g_requestListIndexPrestream >= frameCount){
					g_requestListIndexPrestream = frameCount-1;
				}
				CStreamingRequestFrame::AssetLoadState loadState = m_CurrentRecord->GetFrame(g_requestListIndexPrestream).ProcessRequestList(*m_CurrentRecord);
				m_AllAssetsLoaded = (loadState != CStreamingRequestFrame::NONRESIDENT_ASSETS) && g_bHDTexLoadedThisFrame;				
				//the box streamer won't advance if GetPrestreamMode() == SRL_PRESTREAM_FORCE_OFF or SRL_PRESTREAM_FORCE_COMPLETELY_OFF
				//so don't advance under those conditions
				if(m_AllAssetsLoaded && g_requestListIndexPrestream-g_requestListIndex < g_streamingAheadTimePrestream)// && GetPrestreamMode() != SRL_PRESTREAM_FORCE_OFF)
				{
					g_requestListIndexPrestream++;
				}
			}
			else
			{
				m_AllAssetsLoaded = true;
			}
		}
		else
		{
			streamErrorf("Cutscene begins at %.2f seconds - that's outside the recorded SRL (length=%d)", m_time, frameCount);
			m_AllAssetsLoaded = true;
		}
	}
	else
	{
#if __BANK
		bool changed = false;

		// Support rewinding.
		while (g_removeListIndex > 0 && time < g_lastWriteTime)
		{
			g_removeListIndex--;
			g_requestListIndex = g_removeListIndex;
			g_retryNeeded = -1;
			g_lastWriteTime -= (float)STREAMING_REQUEST_UPDATE;
			changed = true;
		}

		bool keepSceneStreamerOn = m_KeepSceneStreamerOn;
#else // __BANK
		bool keepSceneStreamerOn = false;
#endif // __BANK

		
		if(g_disableForGTAO)
		{
				CTexLod::EnableStateSwapper(false);
		}

		while(g_removeListIndex < frameCount - 1 && time - g_lastWriteTime > (float)STREAMING_REQUEST_UPDATE)
		{
			m_CurrentRecord->GetFrame(g_removeListIndex).SetListDeleteable();

			g_removeListIndex++;
			g_lastWriteTime += (float)STREAMING_REQUEST_UPDATE;
			BANK_ONLY(changed = true;)
		}

		// We can't let the requests fall behind.
		if (g_requestListIndex < g_removeListIndex)
		{
			STRVIS_SET_MARKER_TEXT("SRL is lagging");
		}

		g_requestListIndex = Max(g_requestListIndex, g_removeListIndex);

		// Do we need to move onto the next request list
		{
			if(g_requestListIndex < frameCount - 1)
			{
				//during replay playback don't touch the scene streamer
				if((m_stateFlags & STRREQ_REPLAY_ENABLED) == 0)
					g_SceneStreamerMgr.GetStreamer()->SetEnableIssueRequests(keepSceneStreamerOn);

				if (m_IsLongJumpMode)
				{
					CIplCullBox::SetEnabled(false);
				}

				// Are all the requests in memory?
				CStreamingRequestFrame &frame = m_CurrentRecord->GetFrame(g_requestListIndex);
				CStreamingRequestFrame::AssetLoadState loadState = frame.ProcessRequestList(*m_CurrentRecord);
				m_AllAssetsLoaded = loadState != CStreamingRequestFrame::NONRESIDENT_ASSETS;

				// Where should we be at this point? Consider branches.
				float maxOffset = (float) g_streamingAheadTime;
#if __BANK
				if(m_IsFramesOverride)
				{
					maxOffset = (float) m_framesAhead;
				}
#endif

				if (!m_IsRunFromScript)
				{
					maxOffset = CutSceneManager::GetInstance()->CalculateStreamingOffset(maxOffset, (float) g_removeListIndex);
				}

				// Try this frame again later if some things couldn't be loaded (possibly do to non-resident .ctyp files)
				if (loadState == CStreamingRequestFrame::ALL_REQUESTED_BUT_MISSING && g_retryNeeded != -1)
				{
					g_retryNeeded = g_requestListIndex;
				}

				//Displayf("At %.2f (%.2f), offset=%.2f", (float) g_removeListIndex, time, maxOffset);

				if (m_AllAssetsLoaded && g_requestListIndex < g_removeListIndex + (int) maxOffset)
				{
					g_requestListIndex++;
					BANK_ONLY(changed = true;)

					// Remind the zoned asset manager not to create any requests.
					if (g_requestListIndex < frameCount - 1)
					{

#if __PS3 || __XENON
						CZonedAssetManager::ClearRequests();
						CZonedAssetManager::SetEnabled(false);
#else
						if(g_disableForGTAO)
						{
							CZonedAssetManager::ClearRequests();
							CZonedAssetManager::SetEnabled(false);
						}
#endif
					}

				}
				else
				{
					// If we're stuck, try the older assets again that didn't load the first time.
					if (g_retryNeeded != -1)
					{
						g_requestListIndex = g_retryNeeded;
						g_retryNeeded = -1;
					}
				}

				if (g_requestListIndex == g_removeListIndex)
				{
					STRVIS_SET_MARKER_TEXT("SRL is not ahead");
				}
			}
			else
			{
				CIplCullBox::SetEnabled(true);
				bool hasPostCutsceneInfo = MagSquared(m_PostCutsceneCameraPos).Getf() > 0.0f;

#if STREAMING_VISUALIZE
				if (!g_SceneStreamerMgr.GetStreamer()->GetEnableIssueRequests())
				{
					STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);

					if (hasPostCutsceneInfo)
					{
						STRVIS_SET_MARKER_TEXT("Re-enabling scene streamer, CS almost over - has post-cutscene camera");
					}
					else
					{
						STRVIS_SET_MARKER_TEXT("Re-enabling scene streamer, CS almost over - no post-cutscene info");
					}
				}
#endif // STREAMING_VISUALIZE

				// If we're done with our requests, we can turn the scene streamer back on to make the transition into
				// gameplay easier.				
				if((m_stateFlags & STRREQ_REPLAY_ENABLED) == 0) //during replay playback don't touch the scene streamer
					g_SceneStreamerMgr.GetStreamer()->SetEnableIssueRequests(true);

#if __PS3 || __XENON
				CZonedAssetManager::SetEnabled(true);				
#else
				if(g_disableForGTAO)
				{
					CZonedAssetManager::SetEnabled(true);
				}
#endif

				// Override the focus position if we know where to.
				if (hasPostCutsceneInfo)
				{
					// If this is really far away, then something must be wrong - maybe this is a stale position, or
					// the designer made a mistake.
#if __ASSERT
					if (g_removeListIndex < m_CurrentRecord->GetFrameCount())
					{
						CStreamingRequestFrame &frame = m_CurrentRecord->GetFrame(g_removeListIndex);
						Vec3V futureCamPos = frame.GetCamPos();

						if (IsCamPosValid(futureCamPos) && DistSquared(m_PostCutsceneCameraPos, futureCamPos).Getf() > 100.0f * 100.0f)
						{
							streamSRLWarningf("The SRL post cutscene position and current camera position are quite far away (%.2f, %.2f, %.2f) "
								"and (%.2f, %.2f, %.2f).",
								m_PostCutsceneCameraPos.GetXf(), m_PostCutsceneCameraPos.GetYf(), m_PostCutsceneCameraPos.GetZf(),
								futureCamPos.GetXf(), futureCamPos.GetYf(), futureCamPos.GetZf());
						}

						Assertf(!IsCamPosValid(futureCamPos) || DistSquared(m_PostCutsceneCameraPos, futureCamPos).Getf() < 350.0f * 350.0f,
							"The SRL post cutscene position and current camera position are very far away (%.2f, %.2f, %.2f) "
							"and (%.2f, %.2f, %.2f). Is the post cutscene position correct?!",
							m_PostCutsceneCameraPos.GetXf(), m_PostCutsceneCameraPos.GetYf(), m_PostCutsceneCameraPos.GetZf(),
							futureCamPos.GetXf(), futureCamPos.GetYf(), futureCamPos.GetZf());
					}
#endif // __ASSERT

					CFocusEntityMgr::GetMgr().SetPosAndVel(RCC_VECTOR3(m_PostCutsceneCameraPos), ORIGIN);
				}
				else
				{
					CFocusEntityMgr::GetMgr().SetDefault();
				}
			}
		}

//		STRVIS_SET_MARKER_TEXT("SRL: time: %.2f, rem=%d, req=%d, retry=%d", m_time, g_removeListIndex, g_requestListIndex, g_retryNeeded);

#if __BANK
		if (changed && ( m_AddSVFrameMarkers || PARAM_addsvframemarkers.Get()))
		{
			char markerText[128];
			formatf(markerText, "SRL: Unreq=%d, Req=%d", g_removeListIndex, g_requestListIndex);
			STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
			STRVIS_SET_MARKER_TEXT(markerText);
		}
#endif // __BANK
	}
}

//
// name:		ReadArray/WriteArray
// description:	Helper functions for writing arrays in the load and save routines
template<class _T> void ReadArray(FileHandle fid, atArray<_T>& array)
{
	s32 value;
	CFileMgr::Read(fid, (char*)&value, 4);
	array.Reset();
	array.Resize(value);
	CFileMgr::Read(fid, (char*)array.GetElements(), sizeof(_T)*array.GetCount());
}

template<class _T> void WriteArray(FileHandle fid, atArray<_T>& array)
{
	s32 value = array.GetCount();
	CFileMgr::Write(fid, (char*)&value, 4);
	CFileMgr::Write(fid, (char*)array.GetElements(), sizeof(_T)*array.GetCount());
}

//
// name:		ReadModelInfoArray/WriteModelInfoArray
// description:	Helper functions for writing arrays in the load and save routines
template<class _T> void ReadModelInfoArray(FileHandle fid, atArray<_T>& array)
{
	s32 value;
	CFileMgr::Read(fid, (char*)&value, 4);
	array.Reset();
	array.Resize(value);
	for(s32 i=0; i<array.GetCount(); i++)
	{
		u32 hashKey;
		CFileMgr::Read(fid, (char*)&hashKey, 4);

		fwModelId modelId;
		CModelInfo::GetBaseModelInfoFromHashKey(hashKey, &modelId);
		array[i] = modelId.GetModelIndex();
	}
}

template<class _T> void WriteModelInfoArray(FileHandle fid, atArray<_T>& array)
{
	s32 value = array.GetCount();
	CFileMgr::Write(fid, (char*)&value, 4);
	for(s32 i=0; i<array.GetCount(); i++)
	{
		u32 hashKey = CModelInfo::GetBaseModelInfo(fwModelId(array[i]))->GetHashKey();
		CFileMgr::Write(fid, (char*)&hashKey, 4);		
	}
}


void CStreamingRequestList::CreateSrlFilename(const char* srcFilename, char *outBuffer, size_t bufferSize)
{
	formatf(outBuffer, bufferSize, "%s_srl", srcFilename);
}

//
// name:		CStreamingRequestList::LoadRecording
// description:	Load recording
bool CStreamingRequestList::LoadRecording(const char* cutsceneName)
{
#if __BANK
	// Remember the name of the last list we loaded so we can save it again
	// through the editor.
	safecpy(g_recordingFilename, cutsceneName);
#endif // __BANK

	streamSRLAssert(!m_CurrentRecord);

	char srlName[RAGE_MAX_PATH];
	CStreamingRequestList::CreateSrlFilename(cutsceneName, srlName, sizeof(srlName));

	// Find the streaming index.
	strLocalIndex objIndex = g_fwMetaDataStore.FindSlotFromHashKey(atStringHash(srlName));

//	if (!(streamVerifyf(objIndex != -1, "Unable to find streaming request list %s - make sure it's mentioned in the .meta file", srlName)))
	if (objIndex == -1 || PARAM_nosrl.Get())
	{
		return false;
	}

	streamSRLDisplayf("Loading SRL %s", cutsceneName);

	// HACK: Only enable the "force cutscene required" mode in cutscenes that need it. We /should/ always be doing it though.
	m_EnsureCutsceneRequired = strstr(cutsceneName, "armenian_1_int") != NULL;

	if (strnicmp(cutsceneName, "gtao_intro", 10) == 0)
	{
		g_HACK_GlobalSRLSkipGC = true;
		g_disableForGTAO = true;
	}

	m_Index = g_fwMetaDataStore.GetStreamingIndex(objIndex);

	strStreamingEngine::GetInfo().RequestObject(m_Index, STRFLAG_FORCE_LOAD | STRFLAG_CUTSCENE_REQUIRED);
	m_stateFlags |= STRREQ_STREAMING;

#if STREAMING_VISUALIZE
	STRVIS_ADD_MARKER(strStreamingVisualize::MARKER);
	char markerText[128];
	formatf(markerText, "Loading SRL %s", cutsceneName);
	STRVIS_SET_MARKER_TEXT(markerText);
#endif // STREAMING_VISUALIZE

	return true;
}

// Get frame index of the request list currently being processed
int CStreamingRequestList::GetRequestListIndex() const
{
	return g_requestListIndex;
}

// Get frame index of the unrequest list currently being processed
int CStreamingRequestList::GetUnrequestListIndex() const
{
	return g_removeListIndex;
}