//
// streaming/streamingrequestlist_rec_dbg.cpp
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
#include "scene/texLod.h"
#include "scene/ExtraContent.h"
#include "streaming/packfilemanager.h"
#include "streaming/streaming.h"
#include "streaming/streaming_channel.h"
#include "streaming/streamingengine.h"
#include "streaming/streamingmodule.h"
#include "streaming/streamingrequestlisteditor.h"
#include "streaming/streamingvisualize.h"
#include "string/stringutil.h"
#include "streamingrequestlist_shared.h"

extern float	g_lastWriteTime;
extern s32		g_requestListIndex;
extern s32		g_removeListIndex;
extern s32		g_retryNeeded;
//
extern s32		g_streamingAheadTime;
//extern s32		g_loadsceneAheadTime;
//extern s32		g_loadsceneAheadTimePrestream;


char g_recordingFilename[128];
s32 g_requestThreshold = 100;
s32 g_unrequestFrameThreshold = 10;

#if __BANK
s32 g_RecordAllIndex;									// Index of SRL being recorded
f32 g_BoxStreamerThreshold = LONG_CAMERA_CUT_THRESHOLD;	// distance threshold in calculate look ahead

enum RAS_STATE
{
	RAS_IDLE,
	RAS_FIND_NEXT_CS,
	RAS_LOADING_CS,
} s_RecordAllState = RAS_IDLE;

bool s_RecordAllLastSecond;
#endif // __BANK

#if SRL_EDITOR
	CStreamingRequestListEditor s_SrlEditor;
#endif // SRL_EDITOR

#if USE_SRL_PROFILING_INFO

void CStreamingFrameProfilingInfo::EvaluateScene()
{
	CPostScan::WaitForSortPVSDependency();
	CGtaPvsEntry* pPvsStart = gPostScan.GetPVSBase();
	CGtaPvsEntry* pPvsStop = gPostScan.GetPVSEnd();

	USE_DEBUG_MEMORY();

	// Let's see what's visible here.
	while (pPvsStart != pPvsStop)
	{
		if (!(pPvsStart->GetVisFlags().GetOptionFlags() & VIS_FLAG_ONLY_STREAM_ENTITY))
		{
			const CEntity* pEntity = pPvsStart->GetEntity();

			if (pEntity)
			{
				// Skip the ones we don't care about.
				if (CStreamingLists::IsRequired(pEntity))
				{
					if(!pEntity->GetIsTypePed() && !pEntity->GetIsTypeVehicle())
					{
						atHashString modelId = pEntity->GetModelName();

						// Let's write this up.
						if (m_VisibleModels.Access(modelId) == NULL)
						{
							m_VisibleModels.Insert(modelId, true);
						}

						// Is this one resident?
						if (!CModelInfo::HaveAssetsLoaded(modelId))
						{
							// Visible but not resident - that's bad.
							if (m_MissingModels.Access(modelId) == NULL)
							{
								m_MissingModels.Insert(modelId, true);
							}
						}
					}
				}
			}
		}
		++pPvsStart;
	}
}


CStreamingRequestListProfilingInfo::CStreamingRequestListProfilingInfo()
{
	m_ReportGenerated = false;
}

#endif

bool CStreamingRequestFrame::IsRequested(const CStreamingRequestRecord &record, fwModelId id) const
{
	atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
	atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
	CreateFullRequestList(record, tempRequestList);

	return FindIndex(tempRequestList, id) != -1;
}

bool CStreamingRequestFrame::IsRequestedIgnoreCommon(atHashString modelName) const
{
	return FindIndex(m_AddList, modelName) != -1;
}


bool CStreamingRequestFrame::IsTexLodRequestedIgnoreCommon(atHashString modelName) const
{
	return FindIndex(m_PromoteToHDList, modelName) != -1;
}

bool CStreamingRequestFrame::IsRequested(const CStreamingRequestRecord &record, atHashString modelName) const
{
	atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
	atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
	CreateFullRequestList(record, tempRequestList);

	return FindIndex(tempRequestList, modelName) != -1;
}

bool CStreamingRequestFrame::IsUnrequested(fwModelId id) const
{
	return FindIndex(m_RemoveList, id) != -1;
}

bool CStreamingRequestFrame::IsUnrequested(atHashString modelName) const
{
	return FindIndex(m_RemoveList, modelName) != -1;
}

#if USE_SRL_PROFILING_INFO
void CStreamingRequestRecord::GenerateProfileReport(CStreamingRequestListProfilingInfo &profileInfo)
{
	USE_DEBUG_MEMORY();

	// Don't generate it twice.
	streamSRLAssert(!profileInfo.m_ReportGenerated);

	// Now we'll go through every frame, look for things that were missing, and things that were streamed
	// but not really needed.

	// Let's look at the missing models.
	int frameCount = GetFrameCount();

	// Keep track of the ones we already marked as redundant so we don't constantly re-report them
	atMap<atHashString, bool> redundantReport;

	for (int frameIndex=0; frameIndex<frameCount; frameIndex++)
	{
		CStreamingRequestFrame &frame = GetFrame(frameIndex);
		CStreamingFrameProfilingInfo &profileFrameInfo = profileInfo.GetFrame(frameIndex);

		const atMap<atHashString, bool> &frameMissingModels = profileFrameInfo.GetMissingModels();
		atMap<atHashString, bool>::ConstIterator it = frameMissingModels.CreateIterator();

		for (it.Start(); !it.AtEnd(); it.Next())
		{
			atHashString id = it.GetKey();

			// We're going to skip those we already processed.
			if (*frameMissingModels.Access(id))
			{
				// We got one. Let's gather information.
				CMissingModelReport &missingModel = profileInfo.m_MissingModels.Grow();

				missingModel.m_ModelId = id;
				missingModel.m_FirstMissing = frameIndex;

				// Now let's mark this one and all subsequent ones invalid so the report doesn't constantly whine about it.
				// AT the same time, check out when it finally became resident, if at all.
				int frameSearch = frameIndex;
				int foundResident = -1;

				do
				{
					// Make sure we don't process it again.
					CStreamingFrameProfilingInfo &searchProfilingInfo = profileInfo.GetFrame(frameSearch);
					atMap<atHashString, bool> &searchMissingModels = searchProfilingInfo.GetMissingModels();

					// No longer marked as missing?
					if (searchMissingModels.Access(id) == NULL)
					{
						// Is that because it became resident, or because we stopped caring?
						if (searchProfilingInfo.GetVisibleModels().Access(id) != NULL)
						{
							// It was actually resident!
							foundResident = frameSearch;
						}
						break;
					}

					searchMissingModels[id] = false;
					frameSearch++;
				}
				while (frameSearch < frameCount);

				missingModel.m_FrameResident = foundResident;

				// Now go back in time - when was it requested, when was it evicted?
				frameSearch = frameIndex;
				int foundRequested = -1;
				int foundUnrequested = -1;
				int foundPrevRequested = -1;
				int infoGathered = 0;		// Number of "found" elements we got. If we have 3 (requested/unrequested/prevrequested), we have everything we need. 

				while (frameSearch >= 0 && infoGathered == 0)
				{
					CStreamingRequestFrame &frame = GetFrame(frameSearch);

					// Was it requested this frame? (We don't care if we already covered two requests)
					if (frame.IsRequested(*this, id) && foundPrevRequested != -1)
					{
						// First or second time?
						if (foundRequested == -1)
						{
							// First time.
							foundRequested = frameSearch;
						}
						else
						{
							// Second time.
							foundPrevRequested = frameSearch;
						}

						infoGathered++;
					}

					// Was it unrequested?
					if (frame.IsUnrequested(id) && foundUnrequested == -1)
					{
						foundUnrequested = frameSearch;
						infoGathered++;
					}

					frameSearch--;
				}

				// Jot down all the information we just got.
				missingModel.m_LastRequest = foundRequested;
				missingModel.m_PrevLastUnrequest = foundPrevRequested;
				missingModel.m_LastUnrequest = foundUnrequested;
			}
		}

		// That's it for missing models, now we need to see about unnecessary requests. Every request needs to be accounted for.
		atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
		atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
		frame.CreateFullRequestList(*this, tempRequestList);

		const CStreamingRequestFrame::EntityList &requestList = tempRequestList;
		for (int x=0; x<requestList.GetCount(); x++)
		{
			// Okay, is this ever used at all before it gets unrequested?
			atHashString modelId = requestList[x];

			if (redundantReport.Access(modelId) == NULL)
			{
				int frameSearch = frameIndex;
				int frameUnrequested = -1;
				bool wasUsed = false;

				while (frameSearch < frameCount)
				{
					CStreamingRequestFrame &searchFrame = GetFrame(frameSearch);
					CStreamingFrameProfilingInfo &searchProfileInfo = profileInfo.GetFrame(frameSearch);

					if (searchProfileInfo.GetVisibleModels().Access(modelId) != NULL)
					{
						// It's been used. Requesting it is justified.
						wasUsed = true;
						break;
					}

					if (searchFrame.IsUnrequested(modelId))
					{
						// No use between request and unrequest - this was a completely redundant request.
						frameUnrequested = frameSearch;
						break;
					}

					frameSearch++;
				}

				if (!wasUsed)
				{
					CRedundantModelReport &redundantModel = profileInfo.m_RedundantModels.Grow();

					redundantModel.m_ModelId = modelId;
					redundantModel.m_Requested = frameIndex;
					redundantModel.m_Unrequested = frameUnrequested;
					redundantReport.Insert(modelId, true);
				}
			}
		}

		// Clear unrequested ones so we can start complaining about unnecessary requests again.
		const CStreamingRequestFrame::EntityList &unrequestList = frame.GetUnrequestList();
		for (int x=0; x<unrequestList.GetCount(); x++)
		{
			// Okay, is this ever used at all before it gets unrequested?
			atHashString modelId = unrequestList[x];

			if (redundantReport.Access(modelId) != NULL)
			{
				redundantReport.Delete(modelId);
			}
		}
	}

	profileInfo.m_ReportGenerated = true;
}

void CStreamingRequestRecord::DisplayReport(CStreamingRequestListProfilingInfo &DEBUG_DRAW_ONLY(profileInfo))
{
#if DEBUG_DRAW
	if (profileInfo.m_ReportGenerated)
	{
		grcDebugDraw::AddDebugOutputEx(false, "REPORT for the streaming request list:");
		grcDebugDraw::AddDebugOutputEx(false, "FAILURE to stream in a timely manner:");
		grcDebugDraw::AddDebugOutputEx(false, "%20s %10s %10s %10s %10s %10s", "Model", "Frame Used", "Streamed In", "Last Request", "Last Unreq", "Prev Request");

		for (int x=0; x<profileInfo.m_MissingModels.GetCount(); x++)
		{
			const CMissingModelReport &model = profileInfo.m_MissingModels[x];

			grcDebugDraw::AddDebugOutputEx(false, "%-20s %-10d %-10d %-10d %-10d", model.m_ModelId.TryGetCStr(),
				model.m_FirstMissing, model.m_FrameResident, model.m_LastRequest, model.m_LastUnrequest, model.m_PrevLastUnrequest);
		}

		grcDebugDraw::AddDebugOutputEx(false, "REDUNDANT streaming requests:");
		grcDebugDraw::AddDebugOutputEx(false, "%20s %12s %12s", "Model", "Frame req'd", "Frame unreq'd");

		for (int x=0; x<profileInfo.m_RedundantModels.GetCount(); x++)
		{
			const CRedundantModelReport &model = profileInfo.m_RedundantModels[x];

			grcDebugDraw::AddDebugOutputEx(false, "%-20s %-12d %-12d", model.m_ModelId.TryGetCStr(),
				model.m_Requested, model.m_Unrequested);
		}
	}
#endif // DEBUG_DRAW
}

void CStreamingRequestRecord::AutofixList(CStreamingRequestListProfilingInfo &profileInfo)
{
	USE_DEBUG_MEMORY();

	if (!profileInfo.m_ReportGenerated)
	{
		streamSRLErrorf("No report generated. You need to play a cutscene and enable 'profile lists on playback' and 'show report at end' first.");
		return;
	}

	int changeCount = 0;

	// Step one - remove all redundant streaming operations.
	for (int x=0; x<profileInfo.m_RedundantModels.GetCount(); x++)
	{
		const CRedundantModelReport &model = profileInfo.m_RedundantModels[x];
		atHashString modelId = model.m_ModelId;
		const char *modelName = modelId.TryGetCStr();

		int firstReq = model.m_Requested;
		int lastReq = model.m_Unrequested;

		if (lastReq == -1)
		{
			// All the way to the end.
			lastReq = GetFrameCount() - 1;
		}

		for (int frameIndex=firstReq; frameIndex<=lastReq; frameIndex++)
		{
			CStreamingRequestFrame &frame = GetFrame(frameIndex);

			if (frame.IsRequested(*this, modelId))
			{
				streamSRLDisplayf("Removing request for %s from frame %d", modelName, frameIndex);
				frame.RemoveRequest(modelId);
				changeCount++;
			}

			// Removing an unrequest is technically not necessary in this case, but it's a good idea to keep the SRL as lean
			// as possible. It contains strings and takes up memory.
			if (frame.IsUnrequested(modelId))
			{
				streamSRLDisplayf("Removing unrequest for %s from frame %d", modelName, frameIndex);
				frame.RemoveUnrequest(modelId);
				changeCount++;
			}
		}

		// Here's the final coup de grace: See if we ever requested it. If so, we can unrequest it as soon as it's not needed anymore.
		// TODO: later.
	}

	// Step two - all those missed requests. Let's push the requests earlier, and remove unrequests.
	for (int x=0; x<profileInfo.m_MissingModels.GetCount(); x++)
	{
		CMissingModelReport &missingInfo = profileInfo.m_MissingModels[x];
		atHashString modelId = missingInfo.m_ModelId;
		const char *modelName = modelId.TryGetCStr();

		// Okay, let's scout out the situation. When was it requested? Unrequested?
		int missingFrame = missingInfo.m_FirstMissing;
		int searchFrame = missingFrame;
		bool adjustedRequest = false;
		int searchEnd = 0;

		while (searchFrame >= searchEnd)
		{
			CStreamingRequestFrame &frame = GetFrame(searchFrame);

			if (frame.IsUnrequested(modelId))
			{
				streamSRLDisplayf("Removing unrequest for %s in frame %d, it's non-resident but needed in frame %d", modelName, searchFrame, missingFrame);
				frame.RemoveUnrequest(modelId);
				changeCount++;
			}

			if (!adjustedRequest && frame.IsRequested(*this, modelId))
			{
				if (searchFrame > 0)
				{
					int targetFrame = Max(searchFrame-2, 0);
					searchEnd = targetFrame;
					streamSRLDisplayf("Pushing request for %s from frame %d to frame %d, it's still non-resident when needed in frame %d", modelName, searchFrame, targetFrame, missingFrame);
					frame.RemoveRequest(modelId);
					changeCount++;

					CStreamingRequestFrame &newFrame = GetFrame(targetFrame);
					if (newFrame.IsRequested(*this, modelId))
					{
						streamSRLDisplayf("Never mind, was already requested then.");
					}
					else
					{
						newFrame.RecordAdd(modelId);
						changeCount++;
					}
				}

				adjustedRequest = true;
			}
		}

		if (!adjustedRequest)
		{
			// No requests were found? We need to manually add it.
			// Let's just try two frames before it's needed. This may be too late, and another SRL analysis will detect
			// that and push it earlier a bit.
			int targetFrame = Max(missingFrame-2, 0);
			CStreamingRequestFrame &newFrame = GetFrame(targetFrame);
			streamSRLAssert(!newFrame.IsRequested(*this, modelId));
			newFrame.RecordAdd(modelId);
			changeCount++;

			streamSRLDisplayf("Add a new request for %s in frame %d to make sure it's resident by frame %d where it's needed.", modelName, targetFrame, missingFrame);
		}
	}

	if (changeCount)
	{
		streamSRLDisplayf("All adjustments have been made (changes made: %d). You can re-save the request list now.", changeCount);
	}
	else
	{
		streamSRLDisplayf("No changes were made to the request list. Seems like it's good enough.");
	}
}

void CStreamingRequestList::UpdateListProfile()
{
	if (m_CurrentRecord && m_ProfileLists && m_ProfilingInfo)
	{
		bool isRecording = (m_stateFlags & STRREQ_RECORD_IN_PROGRESS) != 0;

		if (!isRecording)
		{
			if(g_removeListIndex < m_CurrentRecord->GetFrameCount())
			{
				m_ProfilingInfo->GetFrame(g_removeListIndex).EvaluateScene();
			}
		}
	}
}

void CStreamingRequestList::DiscardReport()
{
	if (m_CurrentRecord)
	{
		streamSRLAssertf(false, "This is an obsolete code path");
		delete m_CurrentRecord;
		m_CurrentRecord = NULL;
	}
}

void CStreamingRequestList::DumpReport()
{
	if (!m_ProfilingInfo)
	{
		return;
	}

	const CStreamingRequestListProfilingInfo &profileInfo = *m_ProfilingInfo;

	if (profileInfo.m_ReportGenerated)
	{
		Displayf("FAILURE to stream in a timely manner:");
		Displayf("Model\tFrame Used\tStreamed In\tLast Request\tLast Unrequest\tPrevious Request");

		for (int x=0; x<profileInfo.m_MissingModels.GetCount(); x++)
		{
			const CMissingModelReport &model = profileInfo.m_MissingModels[x];

			Displayf("%s\t%d\t%d\t%d\t%d\t%d", model.m_ModelId.TryGetCStr(),
				model.m_FirstMissing, model.m_FrameResident, model.m_LastRequest, model.m_LastUnrequest, model.m_PrevLastUnrequest);
		}

		Displayf("REDUNDANT streaming requests:");
		Displayf("Model\tFrame requested\tFrame unrequested");

		for (int x=0; x<profileInfo.m_RedundantModels.GetCount(); x++)
		{
			const CRedundantModelReport &model = profileInfo.m_RedundantModels[x];

			Displayf("%s\t%d\t%d", model.m_ModelId.TryGetCStr(), model.m_Requested, model.m_Unrequested);
		}
	}
}

void CStreamingRequestList::AutofixList()
{
	if (!m_CurrentRecord || !m_ProfilingInfo)
	{
		streamSRLErrorf("There is no active cutscene. You need to play a cutscene and enable 'profile lists on playback' and 'show report at end' first.");
		return;
	}

	m_CurrentRecord->AutofixList(*m_ProfilingInfo);
}

#endif // USE_SRL_PROFILING_INFO


#if __BANK
void CStreamingRequestFrame::DebugDrawList(const EntityList &list, int cursor, const bool nonResidentOnly) const
{
	for (int i = 0; i < list.GetCount(); ++i) {
		fwModelId modelId;

		if (CModelInfo::GetBaseModelInfoFromHashKey(list[i], &modelId)) {
			if(CModelInfo::HaveAssetsLoaded(modelId)){
				continue;
			}
			//const char *residentStatus = (CModelInfo::HaveAssetsLoaded(modelId)) ? "RESIDENT" : "unloaded";
			const char *reqStatus = (CModelInfo::AreAssetsRequested(modelId)) ? "REQ!" : "    ";
			const char *cursorStr = (cursor == i) ? ">>>" : "   ";
			//grcDebugDraw::AddDebugOutputEx(false, "%s %s %s %s", cursorStr, residentStatus, reqStatus, CModelInfo::GetBaseModelInfo(modelId)->GetModelName());
			grcDebugDraw::AddDebugOutputEx(false, "%s unloaded %s %s", cursorStr, reqStatus, CModelInfo::GetBaseModelInfo(modelId)->GetModelName());
		}
	}

	if(nonResidentOnly){
		return;
	}

	for (int i = 0; i < list.GetCount(); ++i) {
		fwModelId modelId;

		if (CModelInfo::GetBaseModelInfoFromHashKey(list[i], &modelId)) {
			if(!CModelInfo::HaveAssetsLoaded(modelId)){
				continue;
			}
			const char *reqStatus = (CModelInfo::AreAssetsRequested(modelId)) ? "REQ!" : "    ";
			const char *cursorStr = (cursor == i) ? ">>>" : "   ";
			grcDebugDraw::AddDebugOutputEx(false, "%s RESIDENT %s %s", cursorStr, reqStatus, CModelInfo::GetBaseModelInfo(modelId)->GetModelName());
		}
	}
}


void CStreamingRequestFrame::DebugDrawRequests(const CStreamingRequestRecord &record, const	bool nonResidentOnly) 
{
	int nonResidentCount = 0;

	atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
	atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
	CreateFullRequestList(record, tempRequestList);

	for (int i = 0; i < tempRequestList.GetCount(); ++i)
	{
		fwModelId modelId;

		if (CModelInfo::GetBaseModelInfoFromHashKey(tempRequestList[i], &modelId))
		{
			if (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				nonResidentCount++;
			}
		}
	}

	grcDebugDraw::AddDebugOutput("SRL: Requests: %d (%d to go), %d frames ahead ( max : %d)", tempRequestList.GetCount(), nonResidentCount, g_requestListIndex - g_removeListIndex, g_streamingAheadTime);
	int cursor = -1;

#if SRL_EDITOR
	if (s_SrlEditor.IsCursorInRequests())
	{
		cursor = s_SrlEditor.GetCursor();
	}
#endif // SRL_EDITOR

	DebugDrawList(tempRequestList, cursor, nonResidentOnly);
}

void CStreamingRequestFrame::DebugDrawUnrequests() 
{
	grcDebugDraw::AddDebugOutput("SRL: Unrequests: %d", m_RemoveList.GetCount());
	int cursor = -1;

#if SRL_EDITOR
	if (!s_SrlEditor.IsCursorInRequests())
	{
		cursor = s_SrlEditor.GetCursor();
	}
#endif // SRL_EDITOR

	DebugDrawList(m_RemoveList, cursor);
}
#endif

int CStreamingRequestFrame::FindIndex(const CStreamingRequestFrame::EntityList& list, fwModelId modelId)
{
#if __BANK
	const char *name = CModelInfo::GetBaseModelInfo(modelId)->GetModelName();	
#else
	atHashString name = CModelInfo::GetBaseModelInfo(modelId)->GetModelNameHash();	
#endif
	return FindIndex(list, name);
}

int CStreamingRequestFrame::FindIndex(const CStreamingRequestFrame::EntityList &list, const char *name)
{
	return FindIndex(list, atStringHash(name));
}

int CStreamingRequestFrame::FindIndex(const CStreamingRequestFrame::EntityList &list, atHashString name)
{
	int count = list.GetCount();
	for (int x=0; x<count; x++)
	{
		if (list[x] == name)
		{
			return x;
		}
	}

	return -1;
}

void CStreamingRequestFrame::AddCommonSet(int commonSet)
{
	m_CommonAddSets.Grow() = (u8) commonSet;
}

void CStreamingRequestFrame::RecordAdd(atHashString modelName)
{
	m_AddList.Grow() = modelName;
}

void CStreamingRequestFrame::RecordRemove(atHashString modelName)
{
	m_RemoveList.Grow() = modelName;
}

void CStreamingRequestFrame::RemoveRequest(atHashString modelName)
{
	int index = FindIndex(m_AddList, modelName);

	if (index != -1)
	{
		m_AddList.DeleteFast(index);
	}	
}

void CStreamingRequestFrame::RemoveUnrequest(atHashString modelName)
{
	int index = FindIndex(m_RemoveList, modelName);

	if (index != -1)
	{
		m_RemoveList.DeleteFast(index);
	}	
}

void CStreamingRequestFrame::TexLodAdd(atHashString modelName)
{
	m_PromoteToHDList.Grow() = modelName;
}

#if __BANK
void CStreamingRequestRecord::Save(const char *fileName)
{
	// Save the file in ASCII - the asset exporting system will pick it up
	// and create a binary resource out of it.
	USE_DEBUG_MEMORY();
	streamSRLVerifyf(PARSER.SaveObject(fileName, "", this, parManager::XML), "Unable to save SRL file to %s - please make sure that this file is checked out in Perforce, and not currently read-only.", fileName);

	streamSRLDisplayf("Saved streaming request list at %s", fileName);
}
#endif

bool CStreamingRequestRecord::MatchesTest(int frame, atHashString element, bool request) const
{
	if (request)
	{
		return m_Frames[frame].IsRequested(*this, element);
	}

	return m_Frames[frame].IsUnrequested(element);
}

int CStreamingRequestRecord::FindNext(int currentFrame, atHashString element, int direction, bool request) const
{
	int frameCount = m_Frames.GetCount();

	if (MatchesTest(currentFrame, element, request))
	{
		// See how long until it no longer matches.
		while (true)
		{
			int nextFrame = currentFrame + direction;

			if (nextFrame < 0 || nextFrame >= frameCount)
			{
				return currentFrame;
			}

			if (!MatchesTest(nextFrame, element, request))
			{
				return currentFrame;
			}

			currentFrame = nextFrame;
		}
	}

	// See when it does match.
	while (true)
	{
		int nextFrame = currentFrame + direction;

		if (nextFrame < 0 || nextFrame >= frameCount)
		{
			return currentFrame;
		}

		if (MatchesTest(nextFrame, element, request))
		{
			return nextFrame;
		}

		currentFrame = nextFrame;
	}
}

#if __BANK
void CStreamingRequestRecord::DebugDrawListDetail(const CStreamingRequestFrame::EntityList &list, int frame) const
{
	grcDebugDraw::AddDebugOutputEx(false, "%-30s %10s %5s %5s %5s %5s", "Asset", "Status", "1st R", "Lst R", "1st U", "Lst U");

	const char *substringSearch = gStreamingRequestList.GetSubstringSearch();
	bool hasFilter = substringSearch[0] != 0;

	for (int i = 0; i < list.GetCount(); ++i) {

		atHashString element = list[i];

		if (!hasFilter || stristr(element.TryGetCStr(), substringSearch))
		{
			// Get the first/last request and unrequest.
			int firstReq = FindNext(frame, element, -1, true);
			int lastReq = FindNext(frame, element, 1, true);
			int firstUnreq = FindNext(frame, element, -1, false);
			int lastUnreq = FindNext(frame, element, 1, false);

			const char *residentStatus;

			if (CModelInfo::GetBaseModelInfoFromHashKey(element, NULL))
			{
				residentStatus = (CModelInfo::HaveAssetsLoaded(element)) ? "RESIDENT" : "unloaded";
			}
			else
			{
				residentStatus = "** INVALID **";
			}

			grcDebugDraw::AddDebugOutputEx(false, "%-30s %10s %5d %5d %5d %5d",
				element.TryGetCStr(), residentStatus,
				firstReq, lastReq, firstUnreq, lastUnreq
				);
		}
	}
}
#endif

//
// name:		CStreamingRequestList::StartRecord
// description:	Start recording streaming requests
void CStreamingRequestList::StartRecord(const char* pFilename)
{
	if(pFilename)
		strcpy(g_recordingFilename, pFilename);
	else
		g_recordingFilename[0] = '\0';
	

	g_lastWriteTime = 0.0f;//fwTimer::GetSystemTimeInMilliseconds();
	streamSRLAssert(!m_CurrentRecord);
	m_CurrentRecord = rage_new CStreamingRequestRecord();
#if USE_SRL_PROFILING_INFO
	{
		USE_DEBUG_MEMORY();
		m_ProfilingInfo = rage_new CStreamingRequestListProfilingInfo();
	}
#endif // USE_SRL_PROFILING_INFO
	m_Index = strIndex();

#if __BANK
	if(!(m_stateFlags & STRREQ_REPLAY_ENABLED)) //never write out temporary replay SRLs, just delete them when done
		SaveRecording(g_recordingFilename);	
#endif
	//flush HD textures if are recording them
	//so that we do not get a lot of junk
	if( (m_stateFlags & STRREQ_RECORD_HD_TEX) ){
		CTexLod::FlushAllUpgrades();
	}
}

void CStreamingRequestList::DistributeRequests()
{
	if (m_CurrentRecord)
	{
		int frameCount = m_CurrentRecord->GetFrameCount();
		atFixedArray<atHashString, 8192> newRequests;

		for (int x=1; x<frameCount; x++)
		{
			// Get a delta from the previous frame.
			newRequests.Resize(0);

			const CStreamingRequestFrame::EntityList &prevRequests = m_CurrentRecord->GetFrame(x-1).GetRequestList();

			int requests = m_CurrentRecord->GetFrame(x).GetRequestList().GetCount();

			for (int y=0; y<requests; y++)
			{
				atHashString request = m_CurrentRecord->GetFrame(x).GetRequestList()[y];

				if (prevRequests.Find(request) == -1)
				{
					// New request this frame.
					newRequests.Append() = request;
				}
			}

			if (newRequests.GetCount() > g_requestThreshold)
			{
				// Too many requests - let's offload some to the previous frames.
				// First of all, if any of them was recently unrequested, let's skip the unrequest.
				int unreqMinFrame = Max(0, x-8);

				for (int y=0; y<newRequests.GetCount(); y++)
				{
					atHashString request = newRequests[y];

					for (int z=x-1; z>unreqMinFrame; z--)
					{
						int unreqIndex = m_CurrentRecord->GetFrame(z).GetUnrequestList().Find(request);

						if (unreqIndex != -1)
						{
							// Let's remove the unrequest and call it even.
							m_CurrentRecord->GetFrame(z).GetUnrequestList().DeleteFast(unreqIndex);
							newRequests.DeleteFast(y);
							y--;
							break;
						}
					}
				}

				// Still too many?
				if (newRequests.GetCount() > g_requestThreshold)
				{
					int frameToAdd = x - 1;
					// Okay, let's offload some to the previous frame.
					for (int y=g_requestThreshold; y<newRequests.GetCount(); y++)
					{
						if (m_CurrentRecord->GetFrame(frameToAdd).GetRequestList().GetCount() < g_requestThreshold)
						{
							m_CurrentRecord->GetFrame(frameToAdd).RecordAdd(newRequests[y]);
						}
						else
						{
							// Even the previous one is full. Let's go back even further.
							if (frameToAdd > 0 && frameToAdd > x - 8)
							{
								frameToAdd--;
							}
							else
							{
								break;
							}
						}
					}
				}
			}
		}
	}
}

void CStreamingRequestList::CreateFinalUnrequest()
{
	if (m_CurrentRecord)
	{
		atMap<atHashString, bool> usedModel;

		// Create the final unrequest frame.
		CStreamingRequestFrame &finalFrame = m_CurrentRecord->AddFrame();

		int frameCount = m_CurrentRecord->GetFrameCount();

		for (int x=0; x<frameCount-1; x++)
		{
			CStreamingRequestFrame::EntityList &requestList = m_CurrentRecord->GetFrame(x).GetRequestList();
			int requests = m_CurrentRecord->GetFrame(x).GetAddListCount();
			for (int y=0; y<requests; y++)
			{
				atHashString request = requestList[y];
				if (usedModel.Access(request) == NULL)
				{
					usedModel.Insert(request, true);
					finalFrame.RecordRemove(request);
				}
			}
		}

		// Copy camera position and heading from the previous frame.
		if (frameCount > 1)
		{
			finalFrame.SetCamPos(m_CurrentRecord->GetFrame(frameCount-2).GetCamPos());
			finalFrame.SetCamDir(m_CurrentRecord->GetFrame(frameCount-2).GetCamDir());
			finalFrame.SetFlags(m_CurrentRecord->GetFrame(frameCount-2).GetFlags());
		}
	}
}

static bool CompareCount(const int& a, const int& b) { return a > b; }


void CStreamingRequestList::CreateCommonSets()
{
	if (m_CurrentRecord)
	{
		// Count the number of occurrences for each model.
		atMap<atHashString, int> modelFrameCount;
		atMap<atHashString, atBitSet> modelFramesUsed;

		int frameCount = m_CurrentRecord->GetFrameCount();
		int maxFrame = 0;

		for (int x=0; x<frameCount; x++)
		{
			int requests = m_CurrentRecord->GetFrame(x).GetAddListCount();
			for (int y=0; y<requests; y++)
			{
				atHashString request = m_CurrentRecord->GetFrame(x).GetRequestList()[y];

				if (modelFrameCount.Access(request))
				{
					modelFrameCount[request] = modelFrameCount[request] + 1;
				}
				else
				{
					modelFrameCount[request] = 1;
					modelFramesUsed[request].Init(frameCount);
				}

				modelFramesUsed[request].Set(x);

				maxFrame = Max(maxFrame, modelFrameCount[request]);
			}
		}

		atArray<int> counts;
		counts.Reserve(modelFrameCount.GetNumUsed());

		atMap<int, atArray<atHashString> > countToModel;

		atMap<atHashString, int>::Iterator entry = modelFrameCount.CreateIterator();
		for (entry.Start(); !entry.AtEnd(); entry.Next())
		{
			counts.Append() = entry.GetData();
			countToModel[entry.GetData()].Grow() = entry.GetKey();
		}

		std::sort(counts.GetElements(), &counts.GetElements()[counts.GetCount()], CompareCount);

		// We're using a u8 for reference, so we can't have more than 256.
		int countsToCheck = Min(counts.GetCount(), 256);

		for (int x=0; x<countsToCheck; x++)
		{
			int framesUsed = counts[x];

			if (framesUsed < 5 || m_CurrentRecord->GetCommonSetCount() > 255)
			{
				// Not really worth it.
				continue;
			}

			atArray<atHashString> &models = countToModel[framesUsed];
			atArray<u8> commonIndex;
			commonIndex.Resize(models.GetCount());

			for (int y=0; y<models.GetCount(); y++)
			{
				bool isNew = true;
				atHashString request = models[y];
				int commonSet = 0;

				// See if we can piggyback on a previous one.
				for (int z=0; z<y; z++)
				{
/*					atBitSet testSet;
					testSet.CopyFrom(modelFramesUsed[models[y]]);
					testSet.Intersect(modelFramesUsed[models[z]]);

					if (testSet == modelFramesUsed[models[z]])*/
					if (modelFramesUsed[request] == modelFramesUsed[models[z]])
					{
						m_CurrentRecord->GetCommonSet(commonIndex[z]).Grow() = request;
						commonIndex[y] = commonIndex[z];
						isNew = false;
						break;
					}		
				}

				if (isNew)
				{
					commonSet = m_CurrentRecord->GetCommonSetCount();
					m_CurrentRecord->AddCommonSet().Grow() = request;
					commonIndex[y] = (u8) commonSet;
				}

				for (int z=0; z<frameCount; z++)
				{
					if (m_CurrentRecord->GetFrame(z).IsRequestedIgnoreCommon(request))
					{
						m_CurrentRecord->GetFrame(z).RemoveRequest(request);

						if (isNew)
						{
							m_CurrentRecord->GetFrame(z).AddCommonSet(commonSet);
						}
					}
				}
			}
		}
	}
}

void CStreamingRequestList::CreateUnrequestList()
{
	if (m_CurrentRecord)
	{
		m_CurrentRecord->SetNewStyle(true);

		// Go through every streamable, find its last use, and add an unrequest there.
		int frameCount = m_CurrentRecord->GetFrameCount();

		for (int x=0; x<frameCount-1; x++)
		{
			int requests = m_CurrentRecord->GetFrame(x).GetAddListCount();
			for (int y=0; y<requests; y++)
			{
				atHashString request = m_CurrentRecord->GetFrame(x).GetRequestList()[y];

				// Are we going to need this again next frame?
				if (!m_CurrentRecord->GetFrame(x+1).IsRequestedIgnoreCommon(request))
				{
					// This is the last frame where we need this model. Let's see how long until we need it again.
					int lastTestFrame = Min(x+g_unrequestFrameThreshold, frameCount);
					bool found = false;

					for (int z=x+2; z<lastTestFrame; z++)
					{
						if (m_CurrentRecord->GetFrame(z).IsRequestedIgnoreCommon(request))
						{
							// Rats - it's used again. We can't unload it.
							found = true;
							break;
						}
					}

					if (!found)
					{
						// Not used in near future - we can let go of this model for now.
						m_CurrentRecord->GetFrame(x+1).RecordRemove(request);
					}
				}
			}
		}

		/*
		atMap<atHashString, bool> usedSet;

		m_CurrentRecord->SetNewStyle(true);

		// Go through every streamable, find its last use, and add an unrequest there.
		int frameCount = m_CurrentRecord->GetFrameCount();

		for (int x=0; x<frameCount; x++)
		{
			int requests = m_CurrentRecord->GetFrame(x).GetAddListCount();
			for (int y=0; y<requests; y++)
			{
				atHashString request = m_CurrentRecord->GetFrame(x).GetRequestList()[y];

				// Did we already process this one?
				if (!usedSet.Access(request))
				{
					usedSet.Insert(request, true);
					int lastUseFrame = x;

					// Find the last instance where we used it.
					for (int z=frameCount-1; z>x; z--)
					{
						if (CStreamingRequestFrame::FindIndex(m_CurrentRecord->GetFrame(z).GetRequestList(), request) != -1)
						{
							// Still in use here.
							lastUseFrame = z;
							break;
						}
					}

					// Create the unrequest right AFTER the last use.
					if (lastUseFrame < frameCount-1)
					{
						m_CurrentRecord->GetFrame(lastUseFrame+1).RecordRemove(request);
					}
				}
			}
		}*/
	}
}

//
// name:		CStreamingRequestList::FinishRecord
// description:	Finish recording streaming requests and save to file
void CStreamingRequestList::FinishRecord()
{
	CIplCullBox::SetEnabled(true);
#if __BANK
	CutSceneManager::GetInstance()->SetUseExternalTimeStep(false);
#endif 
	CreateUnrequestList();
	DistributeRequests();
	CreateFinalUnrequest();
	CreateCommonSets();

	//we don't save or delete when replay is enabled
	if(!(m_stateFlags & STRREQ_REPLAY_ENABLED))
	{	
#if __BANK
		SaveRecording(g_recordingFilename);
#endif
		DeleteRecording();
	}

	CFocusEntityMgr::GetMgr().SetDefault();
	m_stateFlags &= ~(STRREQ_RECORD|STRREQ_RECORD_IN_PROGRESS|STRREQ_RECORD_HD_TEX);
}
//
// name:		CStreamingRequestList::UpdateRecord
// description:	Update record. If time step has elapsed then flush recorded requests
void CStreamingRequestList::UpdateRecord(float /*time*/)
{
}

void CStreamingRequestList::RecordPvs()
{
	// Bail if we're not currently recording anything.
	if (!m_CurrentRecord)
	{
		return;
	}

//	if((m_stateFlags & (STRREQ_RECORD|STRREQ_STARTED|STRREQ_PAUSED)) != (STRREQ_RECORD|STRREQ_STARTED))
	if((m_stateFlags & (STRREQ_RECORD|STRREQ_STARTED)) != (STRREQ_RECORD|STRREQ_STARTED))
	{
		return;
	}

	if (m_time == 0.0f && (m_stateFlags & STRREQ_PAUSED))
	{
		return;
	}

	CFocusEntityMgr::GetMgr().SetPosAndVel(camInterface::GetPos(), camInterface::GetVelocity());
	//INSTANCE_STORE.GetBoxStreamer().EnsureLoadedAboutPos(RCC_VEC3V(camInterface::GetPos()));

	// Make sure we have all requests loaded
	if (m_ThoroughRecording)
	{
		CStreaming::LoadAllRequestedObjects();
	}

	CStreamingRequestRecord *record = m_CurrentRecord;
	streamSRLAssertf(record, "Trying to record a streaming request list, but there is no active list being recorded");

	// Get the frame we're recording now.
	u32 frameIndex = (u32) (m_time / (float)STREAMING_REQUEST_UPDATE);

	CStreamingRequestFrame *frame;
	
	if (record->GetFrameCount() <= frameIndex)
	{
		frame = &record->AddFrame();

		frame->SetCamPos(RCC_VEC3V(g_SceneToGBufferPhaseNew->GetCameraPositionForFade()));
		frame->SetCamDir(RCC_VEC3V(g_SceneToGBufferPhaseNew->GetCameraDirection()));

		u32 flags = CStreamingRequestFrame::HAS_EXTRA_INFO;
		fwInteriorLocation loc = CPortalVisTracker::GetInteriorLocation();
		if (loc.IsValid())
		{
			flags |= CStreamingRequestFrame::IS_INSIDE;
		}

		frame->SetFlags(flags);

#if USE_SRL_PROFILING_INFO
		if (m_ProfilingInfo)
		{
			USE_DEBUG_MEMORY();
			m_ProfilingInfo->m_PerFrameProfileInfo.Grow(256);
		}
#endif // USE_SRL_PROFILING_INFO
	}
	else
	{
		frame = &record->GetFrame(frameIndex);
	}

	CPostScan::WaitForSortPVSDependency();
	for(u32 i=0; i<fwVisibilityFlags::MAX_PHASE_VISIBILITY_BITS; i++)
	{
		fwRenderListDesc& rlDesc	= gRenderListGroup.GetRenderListForPhase(i);
		CRenderPhase *renderPhase	= (CRenderPhase *) rlDesc.GetRenderPhaseNew();

		// the streaming cull shapes aren't backed by a real render phase
		if ( !renderPhase || !renderPhase->IsActive() )
		{
			continue;
		}

		atMap<const CEntity *, bool> usedEntities;

		for (int pass=0; pass<RPASS_NUM_RENDER_PASSES; pass++)
		{
			int count = rlDesc.GetNumberOfEntities(pass);

			for (int x=0; x<count; x++)
			{
				const CEntity* pEntity = (const CEntity *) rlDesc.GetEntity((u32) x, pass);

				if (pEntity && pEntity->GetArchetype() && usedEntities.Access(pEntity) == NULL)
				{
					{
						USE_DEBUG_MEMORY();
						usedEntities.Insert(pEntity, true);
					}

					// Skip the ones we don't care about.
					if (CStreamingLists::IsRequired(pEntity))
					{
						if(!pEntity->GetIsTypePed() && !pEntity->GetIsTypeVehicle())
						{
							// Also ignore cutscene assets, those are already managed by the CS system itself.
							u32 modelIndex = pEntity->GetModelIndex();
							strIndex streamIndex = fwArchetypeManager::GetStreamingModule()->GetStreamingIndex(strLocalIndex(modelIndex));

							if (!(strStreamingEngine::GetInfo().GetStreamingInfoRef(streamIndex).GetFlags() & STRFLAG_CUTSCENE_REQUIRED))
							{
#if __BANK
								atHashString modelName = pEntity->GetModelName();
#else
								atHashString modelName = pEntity->GetBaseModelInfo()->GetModelNameHash();
#endif
								// Add the request if it isn't already in the list.
								if (!frame->IsRequestedIgnoreCommon(modelName))
								{
									frame->RecordAdd(modelName);
								}
							}
						}
					}
#if __BANK
					//if required record HD texture requests
					if(m_stateFlags & STRREQ_RECORD_HD_TEX && !pEntity->GetIsTypePed() && !pEntity->GetIsTypeVehicle())
					{
						CBaseModelInfo *pMI = pEntity->GetBaseModelInfo();
						if(CTexLod::AssetHasRequest(pMI->GetAssetLocation()))
						{
							if ( !frame->IsTexLodRequestedIgnoreCommon(pMI->GetModelName()) )
								frame->TexLodAdd(pMI->GetModelName());
						}						
					}
#endif
				}
			}
		}

		{
			USE_DEBUG_MEMORY();
			usedEntities.Reset();
		}
	}
}

#if __BANK

void CStreamingRequestList::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Streaming Request Lists");
	EXTRACONTENT.AddWorkingDLCCombo(&bank);
	bank.AddToggle("DISABLE SRLs", &m_DisableSRL);
	bank.AddToggle("DISABLE HD Tex Requests", &m_DisableHDTexRequests);	
	bank.AddToggle("Record streaming requests", &m_stateFlags, STRREQ_RECORD);
	bank.AddToggle("Record HD Textures", &m_stateFlags, STRREQ_RECORD_HD_TEX);
	bank.AddToggle("Show status", &m_ShowStatus);
	bank.AddToggle("Show request details", &m_ShowRequestDetails);
	bank.AddToggle("Show Non Resident Files", &m_ShowNonResidentOnly);	
	bank.AddToggle("Keep scene streamer enabled", &m_KeepSceneStreamerOn);
	bank.AddToggle("Long jump mode", &m_IsLongJumpMode);
	bank.AddToggle("Ensure cutscene required", &m_EnsureCutsceneRequired);
	bank.AddToggle("Skip timed entities", &m_SkipTimedEntities);
	bank.AddText("Substring Search", m_SubstringSearch, sizeof(m_SubstringSearch));

	bank.AddSlider("Cutscene Index to start auto-record from", &g_RecordAllIndex, 0, 2000, 1);
	bank.AddButton("RECORD ALL THE THINGS", datCallback(MFA(CStreamingRequestList::RecordAllCutscenes), this));
	bank.AddButton("Abort recording all", datCallback(MFA(CStreamingRequestList::AbortRecordAll), this));

	bank.AddSlider("Distance Threshold", &g_BoxStreamerThreshold, 0.0, 2000.0f, 1.0f);

	bank.AddToggle("Override Max Frames Ahead", &m_IsFramesOverride);
	bank.AddSlider("Max Frames Ahead", &m_framesAhead, 0, 20, 1);
	bank.AddSlider("Max Search Frames Ahead", &m_searchFramesAhead, 0, 20, 1);

#if USE_SRL_PROFILING_INFO
	bank.AddToggle("Profile lists on playback", &m_ProfileLists);
	bank.AddToggle("Show profile information", &m_ShowProfileInfo);
	bank.AddToggle("Show report at end", &m_ShowReportAtEnd);
	bank.AddToggle("Don't wait for SRL to prestream", &m_SkipPrestream);
	bank.AddToggle("Show current frame info", &m_ShowCurrentFrameInfo);
	bank.AddToggle("Thorough (slower) recording", &m_ThoroughRecording);
	bank.AddToggle("Add StreamingVisualize frame markers", &m_AddSVFrameMarkers);
	bank.AddButton("Discard report", datCallback(MFA(CStreamingRequestList::DiscardReport), this));
	bank.AddButton("Auto-fix list", datCallback(MFA(CStreamingRequestList::AutofixList), this));
	bank.AddButton("Dump report to TTY", datCallback(MFA(CStreamingRequestList::DumpReport), this));
#endif // USE_SRL_PROFILING_INFO


#if SRL_EDITOR
	s_SrlEditor.AddWidgets(bank);
#endif // SRL_EDITOR

	bank.PopGroup();
}

void CStreamingRequestList::DebugDraw()
{
	IF_COMMERCE_STORE_RETURN();

	if (m_ShowStatus)
	{
		CStreamingRequestRecord *record = m_CurrentRecord;

		if (m_CurrentRecord)
		{
			bool isRecording = (m_stateFlags & STRREQ_RECORD_IN_PROGRESS) != 0;

			if (isRecording)
			{
				grcDebugDraw::AddDebugOutput("Now recording streaming request list, at frame %d", record->GetFrameCount());

				if (record->GetFrameCount())
				{
					CStreamingRequestFrame &frame = record->GetFrame(record->GetFrameCount() - 1);
					grcDebugDraw::AddDebugOutput("This frame: add count %d, remove count %d", frame.GetAddListCount(), frame.GetRemoveListCount());
				}
			}
			else
			{
				grcDebugDraw::AddDebugOutput("Now playing back %s streaming request list from %s, requests at frame %d, removals at frame %d, load scene at %d, total count %d, retry %d",
					(m_CurrentRecord->IsNewStyle()) ? "NEW STYLE" : "OLD STYLE", (m_IsRunFromScript) ? "script" : "cutscene",
					g_requestListIndex, g_removeListIndex, g_requestListIndex, record->GetFrameCount(), g_retryNeeded);
			}
		}

		if (m_stateFlags & STRREQ_PAUSED)
		{
			if (!m_CurrentRecord)
			{
				if (m_stateFlags & STRREQ_STREAMING)
				{
					grcDebugDraw::AddDebugOutput("Streaming Request List is streaming the SRL file...");
				}
				else
				{
					grcDebugDraw::AddDebugOutput("No SRL recorded for this cutscene.");
				}
			}
			else
			{
				grcDebugDraw::AddDebugOutput("%s Style SRL, %s at %.2f", m_CurrentRecord->IsNewStyle() ? "NEW" : "OLD", m_AllAssetsLoaded ? "All assets loaded" : "Still prestreaming assets...", m_time);
			}
		}

		if (m_CurrentRecord)
		{
			bool isRecording = (m_stateFlags & STRREQ_RECORD_IN_PROGRESS) != 0;
			int frameCount = record->GetFrameCount();
			int requestFrame = (isRecording) ? (frameCount - 1) : g_requestListIndex;
			int unrequestFrame = (isRecording) ? requestFrame : g_removeListIndex;
			if (IsModified())
			{
				grcDebugDraw::AddDebugOutput("CURRENT STREAMING REQUEST LIST HAS BEEN MODIFIED -- DON'T FORGET TO SAVE!!");
			}

			if (m_ShowCurrentFrameInfo)
			{
				// Let's go to the frame where we really are, not where we're currently requesting.
				int currentFrame = (int) (m_time / (float)STREAMING_REQUEST_UPDATE);

				if (currentFrame < frameCount)
				{
					grcDebugDraw::AddDebugOutput("%.2f is recorded as frame %d", m_time, currentFrame);

					CStreamingRequestFrame &frame = m_CurrentRecord->GetFrame(currentFrame);

					atHashString tempStorage[MAX_REQUESTS_PER_FRAME];
					atUserArray<atHashString> tempRequestList(tempStorage, MAX_REQUESTS_PER_FRAME);
					frame.CreateFullRequestList(*m_CurrentRecord, tempRequestList);

					grcDebugDraw::AddDebugOutputEx(false, "REQUESTS (%d):", tempRequestList.GetCount());
					m_CurrentRecord->DebugDrawListDetail(tempRequestList, currentFrame);

					grcDebugDraw::AddDebugOutputEx(false, "UNREQUESTS (%d):", frame.GetUnrequestList().GetCount());
					m_CurrentRecord->DebugDrawListDetail(frame.GetUnrequestList(), currentFrame);
				}
				else
				{
					grcDebugDraw::AddDebugOutput("Currently at %.2f (=%d), outside frame count of %d", m_time, currentFrame, frameCount);
				}
			}

			if (m_ShowRequestDetails || m_ShowNonResidentOnly)
			{
				if(requestFrame >= 0 && requestFrame < frameCount)
				{
					m_CurrentRecord->GetFrame(requestFrame).DebugDrawRequests(*m_CurrentRecord, m_ShowNonResidentOnly);
				}
			}

			if (m_ShowRequestDetails)
			{

				if(unrequestFrame >= 0 && unrequestFrame < frameCount)
				{
					m_CurrentRecord->GetFrame(unrequestFrame).DebugDrawUnrequests();
				}
			}
		}
	}

#if USE_SRL_PROFILING_INFO
	if (m_ShowProfileInfo && !IsEnabledForReplay())
	{
		ShowProfileInfo();
	}
#endif // USE_SRL_PROFILING_INFO
}

//
// name:		CStreamingRequestList::SaveRecording
// description:	Save recording
void CStreamingRequestList::SaveRecording(const char* cutsceneName)
{
	if(!cutsceneName)
		return;

	streamAssertf(m_CurrentRecord, "Trying to save a streaming request list, but we're not recording one");
	char fileName[RAGE_MAX_PATH]={0};
	char assetPath[RAGE_MAX_PATH]={0};
	EXTRACONTENT.GetAssetPathForWorkingDLC(assetPath);
	formatf(fileName, "%sexport/levels/gta5/streaming/%s_srl.pso.meta",assetPath, cutsceneName);
	ASSET.CreateLeadingPath(fileName);
	m_CurrentRecord->Save(fileName);

	m_Modified = false;
}

void CStreamingRequestList::SaveCurrentList()
{
	if (!m_CurrentRecord)
	{
		streamSRLErrorf("No streaming request list is currently active.");
		return;
	}

	if(!(m_stateFlags & STRREQ_REPLAY_ENABLED))
	{
		SaveRecording(g_recordingFilename);
	}
}

void CStreamingRequestList::RecordAllCutscenes()
{
	s_RecordAllState = RAS_FIND_NEXT_CS;
}

void CStreamingRequestList::AbortRecordAll()
{
	s_RecordAllState = RAS_IDLE;
	Displayf("Aborting recording - current cutscene will finish, but that's it.");
}

void CStreamingRequestList::UpdateRecordAll()
{
	switch (s_RecordAllState)
	{
	case RAS_IDLE:
		break;

	case RAS_FIND_NEXT_CS:
		{
			// Get the next slot with an SRL.
			while (true)
			{
				if (g_RecordAllIndex >= g_fwMetaDataStore.GetCount())
				{
					s_RecordAllState = RAS_IDLE;
					g_RecordAllIndex = 0;
					Displayf("DONE RECORDING SRLs.");
					return;
				}

				const char *name = g_fwMetaDataStore.GetName(strLocalIndex(g_RecordAllIndex));
				int len = istrlen(name);

				if (len > 5)
				{
					if (stricmp(&name[len-4], "_srl") == 0)
					{
						// This one is an SRL. Let's use it.
						s_RecordAllState = RAS_LOADING_CS;
						m_stateFlags |= STRREQ_RECORD;
						s_RecordAllLastSecond = false;

						// Load it.
						CutSceneManager* pManager = CutSceneManager::GetInstance();

						char cutsceneName[256];
						safecpy(cutsceneName, name);
						Assert(len < sizeof(cutsceneName));
						cutsceneName[len-4] = 0;

						strLocalIndex cutSceneIdx = g_CutSceneStore.FindSlot(cutsceneName);
						if(cutSceneIdx.IsValid())
						{
							strIndex idx = g_CutSceneStore.GetStreamingIndex(cutSceneIdx);
							strStreamingInfo* inf = strStreamingEngine::GetInfo().GetStreamingInfo(idx);
							if(inf)
							{
								char workingDevice[RAGE_MAX_PATH]={0};
								EXTRACONTENT.GetWorkingDLCDeviceName(workingDevice);
								const char* cutsceneSource = strPackfileManager::GetImageFileNameFromHandle(inf->GetHandle());
								bool isSelected = strncmp(cutsceneSource,workingDevice,strlen(workingDevice)) == 0;
								if(isSelected)
								{
									// This one is an SRL. Let's use it.
									Displayf("Begin recording SRL for %s", cutsceneName);
									s_RecordAllState = RAS_LOADING_CS;
									m_stateFlags |= STRREQ_RECORD;
									s_RecordAllLastSecond = false;
									pManager->PretendStartedFromWidget();
									pManager->RequestCutscene(cutsceneName, false, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, cutsManager::SKIP_FADE, THREAD_INVALID, CUTSCENE_REQUESTED_FROM_WIDGET);
									return;
								}
								else
								{
									//The cutscene we found isn't int he dlc pack we've selected, moving on
									g_RecordAllIndex++;
									continue;									
								}
							}
							else
							{
								Displayf("Failed loading cutscene: %s",cutsceneName);
							}
						}
						g_RecordAllIndex++;
						return;
					}
				}

				g_RecordAllIndex++;
			}
		}
		break;

	case RAS_LOADING_CS:
		{
			// Has it loaded yet? Are we done? Is it even active? It won't be active if the cutscene couldn't be loaded.
			CutSceneManager* pManager = CutSceneManager::GetInstance();
			if (pManager->IsActive() && (m_stateFlags & STRREQ_RECORD))
			{
				// No, still working.
				break;
			}
		}

		Displayf("SRL recorded - moving on to next cutscene");
		s_RecordAllState = RAS_FIND_NEXT_CS;
		break;
	}
}

#if USE_SRL_PROFILING_INFO
void CStreamingRequestList::ShowProfileInfo()
{
	if (!m_CurrentRecord || m_CurrentRecord->GetFrameCount() == 0 || !m_ProfilingInfo)
	{
		return;
	}

	grcDebugDraw::AddDebugOutput("Non-resident visible models in the last free frames:");

	int curFrame = Min(g_removeListIndex, m_CurrentRecord->GetFrameCount() - 1);

	// Check the last three frames (or fewer, if we don't have as many)
	int frameCount = Min(3, curFrame + 1);

	// We need to collect the data first, then show it so we don't show duplicates.
	atMap<atHashString, bool> missingModels;

	for (int x=0; x<frameCount; x++, curFrame--)
	{
		CStreamingFrameProfilingInfo &profileInfo = m_ProfilingInfo->GetFrame(curFrame);
		const atMap<atHashString, bool> &frameMissingModels = profileInfo.GetMissingModels();

		atMap<atHashString, bool>::ConstIterator it = frameMissingModels.CreateIterator();
		for (it.Start(); !it.AtEnd(); it.Next())
		{
			atHashString id = it.GetKey();

			if (missingModels.Access(id) == NULL)
			{
				missingModels.Insert(id, true);
			}
		}		
	}

	// Now that we collected the information, display it.
	atMap<atHashString, bool>::Iterator missingIt = missingModels.CreateIterator();

	for (missingIt.Start(); !missingIt.AtEnd(); missingIt.Next())
	{
		atHashString id = missingIt.GetKey();

		grcDebugDraw::AddDebugOutput("NOT RESIDENT: %s", id.TryGetCStr());
	}
}
#endif // USE_SRL_PROFILING_INFO

#endif // __BANK

void CStreamingRequestList::EnableForReplay() {m_stateFlags |= STRREQ_REPLAY_ENABLED;}
void CStreamingRequestList::RecordForReplay() {m_stateFlags |= STRREQ_RECORD;}
void CStreamingRequestList::DisableForReplay() {m_stateFlags = 0;}
bool CStreamingRequestList::IsEnabledForReplay() {return (m_stateFlags & STRREQ_REPLAY_ENABLED) != 0;}