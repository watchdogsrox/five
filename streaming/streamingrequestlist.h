//
// streaming/streamingrequestlist.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STREAMING_STREAMINGREQUESTLIST_H
#define STREAMING_STREAMINGREQUESTLIST_H

#include "parser/macros.h"
#include "atl/array.h"
#include "atl/hashstring.h"
#include "atl/map.h"
#include "atl/string.h"
#include "data/base.h"
#include "entity/archetypemanager.h"
#include "fwscene/stores/boxstreamerassets.h"
#include "fwscene/stores/boxstreamersearch.h"
#include "fwscene/world/InteriorLocation.h"
#include "streaming/streamingdefs.h"
#include "system/timer.h"

enum {
	STRREQ_RECORD = (1<<0),						// We're would like to record a SRL
	STRREQ_STARTED = (1<<1),					// We're in the process of playing or recording this SRL
	STRREQ_RECORDING_ENABLED = (1<<2),
	STRREQ_RECORD_IN_PROGRESS = (1<<3),
	STRREQ_STREAMING = (1<<4),					// This SRL is still being streamed in
	STRREQ_PAUSED = (1<<5),						// This SRL is in the pause/prestream phase - it will load assets, but not play back or unload anything
	STRREQ_RECORD_HD_TEX = (1<<6),				// We're would like to record HD TExtures in this SRL
	STRREQ_REPLAY_ENABLED = (1<<7),
};

enum SrlPrestreamMode
{
	SRL_PRESTREAM_DEFAULT = 0,
	SRL_PRESTREAM_FORCE_ON,		// 1
	SRL_PRESTREAM_FORCE_OFF,	// 2
	SRL_PRESTREAM_FORCE_COMPLETELY_OFF,	// 3
};


#define USE_SRL_PROFILING_INFO		(__BANK)

namespace rage {
	class bkBank;
	class parTreeNode;
};

class CGtaPvsEntry;
class CInteriorProxy;
class CStreamingRequestRecord;

#if USE_SRL_PROFILING_INFO

class CStreamingFrameProfilingInfo
{
public:
	void EvaluateScene();

	atMap<atHashString, bool> &GetMissingModels()				{ return m_MissingModels; }
	const atMap<atHashString, bool> &GetVisibleModels() const	{ return m_VisibleModels; }


private:
	atMap<atHashString, bool> m_VisibleModels;
	atMap<atHashString, bool> m_MissingModels;
};

struct CMissingModelReport
{
	// Model in question
	atHashString m_ModelId;

	// Frame in which it was missing
	int m_FirstMissing;

	// Frame when it was finally resident
	int m_FrameResident;

	// Frame when it was last requested before the missing occurrence
	int m_LastRequest;

	// Frame when it was last unrequested before the missing occurrence
	int m_LastUnrequest;

	// Previous frame when it was last requested before the missing occurrence, last occurence before m_LastRequest
	int m_PrevLastUnrequest;
};

struct CRedundantModelReport
{
	// Model in question.
	atHashString m_ModelId;

	// Frame in which it was requested
	int m_Requested;

	// Frame in which it was unrequested
	int m_Unrequested;
};

struct CStreamingRequestListProfilingInfo
{
	CStreamingRequestListProfilingInfo();

	CStreamingFrameProfilingInfo &GetFrame(int frame)		{ return m_PerFrameProfileInfo[frame]; }


	atArray<CStreamingFrameProfilingInfo> m_PerFrameProfileInfo;

	atArray<CMissingModelReport> m_MissingModels;
	atArray<CRedundantModelReport> m_RedundantModels;
	bool m_ReportGenerated;
};

#endif

/** An instance of this class represents a single frame of a SRL.
 *  It contains information about which entities to request and which ones
 *  to free up during this frame.
 */
class CStreamingRequestFrame
{
public:
	enum AssetLoadState
	{
		NONRESIDENT_ASSETS,				// Some assets aren't resident yet
		ALL_REQUESTED_BUT_MISSING,		// All assets have been requested, but some don't exist, either because of missing .typ files or because of asset problems
		ALL_RESIDENT,					// All resident
	};

	// Possible values for m_Flags
	enum
	{
		IS_INSIDE = BIT0,				// The given position is inside an interior
		HAS_EXTRA_INFO = BIT1,			// If true, IS_INSIDE is valid - otherwise, it will always be 0.
	};


	typedef atArray<atHashString> EntityList;
	friend class CStreamingRequestRecord;
	CStreamingRequestFrame() : m_Flags(0){ }
	~CStreamingRequestFrame();

	void OnPostLoad(); 

	void OnPostSave(parTreeNode* pNode); 

	AssetLoadState ProcessRequestList(const CStreamingRequestRecord &record);

	void SetListDeleteable();
	void MakeRequestsDeleteable(CStreamingRequestRecord &record);
	
	bool AreAllLoadedOrUnrequested(const CStreamingRequestRecord &record) const;

	int GetAddListCount() const					{ return m_AddList.GetCount(); }
	int GetRemoveListCount() const				{ return m_RemoveList.GetCount(); }

	EntityList &GetRequestList()				{ return m_AddList; }
	EntityList &GetUnrequestList()				{ return m_RemoveList; }

	void CreateFullRequestList(const CStreamingRequestRecord &record, atArray<atHashString> &outRequestList) const;

	Vec3V_Out GetCamPos() const					{ return m_CamPos; }
	Vec3V_Out GetCamDir() const					{ return m_CamDir; }

	void SetCamPos(Vec3V_In camPos)				{ m_CamPos = camPos; }
	void SetCamDir(Vec3V_In camDir)				{ m_CamDir = camDir; }

	bool IsInside() const						{ return (m_Flags & IS_INSIDE) != 0; }
	bool HasExtraInfo() const					{ return (m_Flags & HAS_EXTRA_INFO) != 0; }
	u32 GetFlags() const						{ return m_Flags; }
	void SetFlags(u32 flags)					{ m_Flags = flags; }

#if __BANK
	void DebugDrawRequests(const CStreamingRequestRecord &record, const bool nonResidentOnly = false);
	void DebugDrawUnrequests();

	void DebugDrawList(const EntityList &list, int cursor, const bool nonResidentOnly = false) const;
#endif // __BANK
	void AddCommonSet(int commonSet);

	void TexLodAdd(atHashString modelName);

	static int FindIndex(const EntityList &list, const char *modelName);
	static int FindIndex(const EntityList &list, fwModelId modelId);
	static int FindIndex(const CStreamingRequestFrame::EntityList &list, atHashString name);

	void RecordAdd(atHashString modelName);
	void RecordRemove(atHashString modelName);
	
	bool IsRequestedIgnoreCommon(atHashString modelName) const;

	bool IsTexLodRequestedIgnoreCommon(atHashString modelName) const;
	bool IsRequested(const CStreamingRequestRecord &record, atHashString modelName) const;
	bool IsRequested(const CStreamingRequestRecord &record, fwModelId id) const;
	bool IsUnrequested(fwModelId id) const;
	bool IsUnrequested(atHashString modelName) const;
	void RemoveRequest(atHashString modelName);
	void RemoveUnrequest(atHashString modelName);

private:
	bool AddMapDataRefCount(atHashString modelName, fwModelId modelId);
	void RemoveMapDataRefCount(atHashString modelName, fwModelId modelId);

	PAR_SIMPLE_PARSABLE;

	EntityList m_AddList;
	EntityList m_RemoveList;
	EntityList m_PromoteToHDList;

	Vec3V m_CamPos;
	Vec3V m_CamDir;

	// Index into the common sets in the CStreamingRequestRecord for additional elements in the add list
	atArray<u8> m_CommonAddSets;

	u32 m_Flags;
};

class CStreamingRequestCommonSet
{
public:
	CStreamingRequestFrame::EntityList m_Requests;

	PAR_SIMPLE_PARSABLE;
};

/** PURPOSE: This class represents one streaming request list. There is typically
 *  only one instance of this class resident in memory at any time, if at all.
 *
 *  This file is serialized through the parser.
 */
class CStreamingRequestRecord
{
public:
	CStreamingRequestRecord();
	~CStreamingRequestRecord();

	CStreamingRequestFrame::EntityList &GetCommonSet(int commonSet)				{ return m_CommonSets[commonSet].m_Requests; }
	const CStreamingRequestFrame::EntityList &GetCommonSet(int commonSet) const	{ return m_CommonSets[commonSet].m_Requests; }

	void Clear();

	void Update(float UNUSED_PARAM(fTime)) {};
	void Start() {};
	void Finish() {};
	int GetFrameCount() const		{ return m_Frames.GetCount(); }

	bool IsNewStyle() const			{ return m_NewStyle; }
	void SetNewStyle(bool newStyle)	{ m_NewStyle = newStyle; }

	CStreamingRequestFrame &GetFrame(int frame)		{ return m_Frames[frame]; }

#if __BANK
	void Save(const char *fileName);		
	void DebugDrawListDetail(const CStreamingRequestFrame::EntityList &list, int frame) const;
#endif // __BANK
	CStreamingRequestFrame::EntityList &AddCommonSet()		{ return m_CommonSets.Grow().m_Requests; }
	int GetCommonSetCount() const							{ return m_CommonSets.GetCount(); }
	CStreamingRequestFrame &AddFrame()						{ return m_Frames.Grow(256); }

#if USE_SRL_PROFILING_INFO
	void GenerateProfileReport(CStreamingRequestListProfilingInfo &profileInfo);
	void DisplayReport(CStreamingRequestListProfilingInfo &profileInfo);
	void AutofixList(CStreamingRequestListProfilingInfo &profileInfo);
#endif // USE_SRL_PROFILING_INFO

private:
	int FindNext(int currentFrame, atHashString element, int direction, bool request) const;
	bool MatchesTest(int frame, atHashString element, bool request) const;

	PAR_SIMPLE_PARSABLE;
	atArray<CStreamingRequestFrame> m_Frames;
	atArray<CStreamingRequestCommonSet> m_CommonSets;

	bool m_NewStyle;
};

/** PURPOSE: The master list is a temporary object that is loaded in through
 *  the data file manager. It contains a list of all known streaming request lists.
 */
struct CStreamingRequestMasterList
{
	atArray<atString> m_Files;

private:
	PAR_SIMPLE_PARSABLE;
};

/** PURPOSE: This is the manager class for streaming request lists. It is responsible
 *  for initiating the loading and saving of SRLs, and providing the heartbeat function.
 */
class CStreamingRequestList : public datBase
{
public:
	CStreamingRequestList();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);
	static void RegisterFromFile(const char* fileName);
	static void UnregisterFromFile(const char* fileName);

	void BeginPrestream(const char* cutsceneName, bool fromScript);
	void Start();
	void Pause();
	void Finish();
	void SetPostCutsceneCamera(Vec3V_In camPos, Vec3V_In camDir);
	void SetLongJumpMode(bool enabled)				{ m_IsLongJumpMode = enabled; }
	bool IsLongJumpMode() const						{ return m_IsLongJumpMode; }
	void SetPrestreamMode(SrlPrestreamMode prestreamMode)			{ m_PrestreamMode = prestreamMode; }
	SrlPrestreamMode GetPrestreamMode() const					{ return m_PrestreamMode; }
	void SetSrlReadAheadTimes(int prestreamMap, int prestreamAssets, int playbackMap, int playbackAssets);
	void SetTime(float time) {m_time = time;}
	float GetTime() const		{ return m_time; }
	void Update();
	bool IsActive() { return (m_stateFlags & STRREQ_STARTED)!=0;}
	bool IsPlaybackActive() const { return m_CurrentRecord && ((m_stateFlags & (STRREQ_STARTED|STRREQ_PAUSED)) == STRREQ_STARTED); }
	bool IsAboutToFinish() const;
	bool IsRunFromScript() const					{ return m_IsRunFromScript; }
	void EnableRecording(bool bRecord);

#if !__FINAL
	__forceinline void RecordRequest(strIndex /*index*/) {} 
#endif // !__FINAL

	// RETURNS: TRUE if all assets for the current frame have been loaded.
	// Ignore (=return true) if we're not playing anything back, or if we're in the process of recording.
	bool AreAllAssetsLoaded() const						{ return (!(m_stateFlags & STRREQ_STARTED)) || GetPrestreamMode() == SRL_PRESTREAM_FORCE_COMPLETELY_OFF || (m_stateFlags & STRREQ_RECORD) || m_AllAssetsLoaded BANK_ONLY(|| m_SkipPrestream); }

	// Get frame index of the request list currently being processed
	int GetRequestListIndex() const;

	// Get frame index of the unrequested list currently being processed
	int GetUnrequestListIndex() const;

	// Called from the box streamer to get a list of additional places to load map data from.
	void AddSearches(atArray<fwBoxStreamerSearch>& searchList, fwBoxStreamerAssetFlags supportedAssetFlags);

	void ComputeSearchesAndFocus();

	bool GetEnsureCutsceneRequired() const	{ return m_EnsureCutsceneRequired; }

	bool HasValidInteriorCutHint() const { return m_bValidInteriorHint; }
	fwInteriorLocation GetInteriorCutHint() const { return m_locInteriorHint; }
	Vec3V_Out GetInteriorCutHintPos() const { return m_posInteriorHint; }

#if USE_SRL_PROFILING_INFO
	void UpdateListProfile();
#else // USE_SRL_PROFILING_INFO
	__forceinline void UpdateListProfile() {}
#endif // USE_SRL_PROFILING_INFO

#if __BANK

	void InitWidgets(bkBank& bank);
	void DebugDraw();
	bool IsRecording() const				{ return (m_stateFlags & (STRREQ_STARTED|STRREQ_RECORD)) == (STRREQ_STARTED|STRREQ_RECORD); }
	bool IsSkipTimedEntities() const		{ return m_SkipTimedEntities; }
	bool IsShowStatus() const				{ return m_ShowStatus; }
	bool IsModified() const					{ return m_Modified; }
	void MarkModified()						{ m_Modified = true; }

	const char *GetSubstringSearch() const	{ return m_SubstringSearch; }

#if USE_SRL_PROFILING_INFO
	void ShowProfileInfo();
#endif // USE_SRL_PROFILING_INFO

#endif // __BANK

#if __BANK
	void SaveCurrentList();
#endif // __BANK
	void RecordPvs();

	// Get the current list being recorded or played back.
	CStreamingRequestRecord *GetCurrentRecord() const	{ return m_CurrentRecord; }

	static void CreateSrlFilename(const char *srcFilename, char *outBuffer, size_t bufferSize);

	void DeleteRecording();


	void EnableForReplay();
	void RecordForReplay();
	void DisableForReplay();
	bool IsEnabledForReplay();

	void StartRecord(const char* pFilename);
private:
	// Remove the currently resident recording from memory	
	void StartPlayback(const char* cutsceneName);
	void FinishPlayback(bool bResetFocus);
	void UpdatePlayback(float time);
	void UpdateNewStylePlayback(float time);

	bool LoadRecording(const char* cutsceneName);
	bool IsCamPosValid(Vec3V_In camPos) const;

	void FinishRecord();
	void UpdateRecord(float time);
	void CreateUnrequestList();
	void CreateCommonSets();
	void CreateFinalUnrequest();
	void DistributeRequests();		
#if __BANK
	void RecordAllCutscenes();
	void AbortRecordAll();
	void UpdateRecordAll();	
	void SaveRecording(const char* cutsceneName);
#endif // __BANK	

#if USE_SRL_PROFILING_INFO
	void DiscardReport();
	void DumpReport();
	void AutofixList();
#endif // USE_SRL_PROFILING_INFO
	/*
	float	m_LastWriteTime;
	s32		m_RequestListIndex;
	s32		m_RemoveListIndex;
	s32		m_RetryNeeded;
	*/
	// This is where we expect the camera to be after the cutscene is over, or 0/0/0 if we don't have that information.
	Vec3V m_PostCutsceneCameraPos;

	// This is where we expect the camera to face after the cutscene is over. Valid only if m_PostCutsceneCamerPos is valid.
	Vec3V m_PostCutsceneCameraDir;

	// All locations that we currently force to be resident. The value is the frame counter, interiors
	// with stale frame counters will be unpinned.
	atMap<CInteriorProxy *, int> m_PinnedInteriors;

	// The current request list being recorded or played back.
	CStreamingRequestRecord *m_CurrentRecord;

	u32 m_stateFlags;
	float m_time;

	// Index of the file currently being streamed in
	strIndex m_Index;

	// Whether or not to prestream a SRL, or whether to use the distance.
	SrlPrestreamMode m_PrestreamMode;

	// True if all assets for the current frame have been loaded
	bool m_AllAssetsLoaded;

	// True if this SRL was triggered by a script - in that case, we'll need to keep track of the time ourselves.
	bool m_IsRunFromScript;

	// If true, we'll take measures to make long cuts possible.
	bool m_IsLongJumpMode;

	// If true, we'll make sure all assets are marked CUTSCENE_REQUIRED, even those that were arlready resident. We should always be doing this, but
	// this is too big of a change before sub, so this will only be enabled in selected cutscenes that need it.
	bool m_EnsureCutsceneRequired;

	// Interior cut hint - some lookahead for cuts into interiors, so that we can call CreateDrawable on entities and make sure attached lights
	// are constructed ahead of time
	bool m_bValidInteriorHint;
	fwInteriorLocation m_locInteriorHint;
	Vec3V m_posInteriorHint;
		

#if __BANK
	bool m_ShowStatus;

	bool m_ShowRequestDetails;

	bool m_ShowNonResidentOnly;

	// Set to true if this list has been modified with the editor.
	bool m_Modified;

	// Global SRL circuit breaker
	bool m_DisableSRL;

	// Keep the scene streamer alive
	bool m_KeepSceneStreamerOn;

	char m_SubstringSearch[64];

	// Don't block a cutscene while the SRL is prestreaming
	bool m_SkipPrestream;
		

	// Add a SV marker for every time we switch to a new recorded frame
	bool m_AddSVFrameMarkers;

	// Show details about the current SRL, at the current stage
	bool m_ShowCurrentFrameInfo;

	// If true, don't record timed entities that aren't currently active
	bool m_SkipTimedEntities;

	//lets us know if we should override the script/global value of max frames ahead an SRL
	//can load data in from
	bool m_IsFramesOverride;
	u32	m_framesAhead;
	u32	m_searchFramesAhead;

	// Disable HD Texture Requests
	bool m_DisableHDTexRequests;
		
#endif // __BANK

	// Slower frame-by-frame playback to make sure the SRL contains everything
	bool m_ThoroughRecording;

#if USE_SRL_PROFILING_INFO
	CStreamingRequestListProfilingInfo *m_ProfilingInfo;

	bool m_ProfileLists;
	bool m_ShowProfileInfo;
	bool m_ShowReportAtEnd;
#endif // USE_SRL_PROFILING_INFO

};

extern CStreamingRequestList gStreamingRequestList;

#endif // STREAMING_STREAMINGREQUESTLIST_H
