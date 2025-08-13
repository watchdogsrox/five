//
// streaming/streaming.h
//
// Copyright (C) 1999-2009 Rockstar Games.  All Rights Reserved.
//

#ifndef STREAMING_STREAMING_H
#define STREAMING_STREAMING_H


#include "diag/channel.h"
#include "paging/streamer.h"

#include "control/replay/ReplaySettings.h"
#include "scene/streamer/SceneLoader.h"
#include "streaming/streamingdefs.h"
#include "streaming/streamingcleanup.h"
#include "streaming/streamingdebug.h"
#include "streaming/streamingvisualize.h"

namespace rage
{
	class pgBase;
	class Vector3;
}


class CStreaming 
{
public:
	static void InitLevelWithMapUnloaded();

//	This function is currently not being called.
//	Maybe the call to it was lost when the new init/shutdown system went in
//	static void ShutdownLevelWithMapLoaded();

	static void InitStreamingEngine();

	static void InitStreamingVisualize();

	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static strIndex RegisterDummyObject(const char* name, strLocalIndex objectIndex, s32 module);

	static void LoadStreamingMetaFile();	
	static void SetStreamingFile(const char* streamingFile);

	static void Update();
 //   static void RequestNearbyObjects();

	static void DisableStreaming(); 
	static void EnableStreaming(); 
	static bool IsStreamingDisabled(); 
	static void EnableSceneStreaming(bool bEnable) { ms_enableSceneStreaming = bEnable;}
	static bool IsSceneStreamingDisabled() {return (ms_enableSceneStreaming == false); }
#if !__FINAL
	static inline bool IsStreamingPaused() { return ms_pauseStreaming; }
	static inline void SetStreamingPaused(bool bPaused) { ms_pauseStreaming = bPaused; }
	static void EndRecordingStartupSequence();
	static void BeginRecordingStartupSequence();
#endif
	static const char *GetStarupSequenceRecordingName(char *outBuffer, size_t bufferSize);
	static void ReplayStartupSequenceRecording();


	static s32 GetNumberObjectsRequested();
	static void LoadAllRequestedObjects(bool bPriorityRequestsOnly = false);
#if GTA_REPLAY
	static void LoadAllRequestedObjectsWithTimeout(bool bPriorityRequestsOnly = false, u32 timeoutMS = 0);
#endif
	static void Flush();
	static void FlushRequestList();
	static void PurgeRequestList(u32 ignoreFlags=STR_DONTDELETE_MASK|STRFLAG_FORCE_LOAD);

	static bool RequestObject(strLocalIndex index, s32 module, s32 flags);
	static void RemoveObject(strLocalIndex index, s32 module);

	static void ClearRequiredFlag(strLocalIndex objIndex, s32 module, s32 flags);
	static bool LoadObject(strLocalIndex index, s32 module, bool defraggable=false, bool bCanBeDeleted=false);

	static void ClearFlagForAll(u32 flag);
	static void SetObjectIsNotDeletable(strLocalIndex objIndex, s32 module);
	static void SetObjectIsDeletable(strLocalIndex objIndex, s32 module);
	static bool GetObjectIsDeletable(strLocalIndex objIndex, s32 module);
	static void SetDoNotDefrag(strLocalIndex index, s32 module);
	static void SetMissionDoesntRequireObject(strLocalIndex objIndex, s32 module);
	static void SetCutsceneDoesntRequireObject(strLocalIndex objIndex, s32 module);
	static void SetInteriorDoesntRequireObject(strLocalIndex objIndex, s32 module);

#if !__FINAL
	static bool GetReloadPackfilesOnRestart();
#else // !__FINAL
	__forceinline static bool GetReloadPackfilesOnRestart()		{ return false; }
#endif // !__FINAL

	// Called from within an Init() function. Returns true if we should load data that never changes, false otherwise.
	// Will return false if we're doing a game restart right now, unless if we have to reload the data for debugging purposes
	// (because GetReloadPackfilesOnRestart is true).
	static bool ShouldLoadStaticData();

	static void PostLoadRpf(int index);

	static u32 GetObjectFlags(strLocalIndex index, s32 module);
	static u32 GetObjectVirtualSize(strLocalIndex index, s32 module);
	static u32 GetObjectPhysicalSize(strLocalIndex index, s32 module);
	static void GetObjectAndDependenciesSizes(strLocalIndex index, s32 module, u32& virtualSize, u32& physicalSize, const strIndex* ignoreList=NULL, s32 numIgnores=0, bool mayFail = false);
	static void GetObjectAndDependencies(strLocalIndex index, s32 module, atArray<strIndex>& allDeps, const strIndex* ignoreList, s32 numIgnores);
	static bool HasObjectLoaded(strLocalIndex index, s32 module);
	static bool IsObjectRequested(strLocalIndex index, s32 module);
	static bool IsObjectLoading(strLocalIndex index, s32 module);
	static bool IsObjectInImage(strLocalIndex index, s32 module);
	static bool IsObjectNew(strLocalIndex index, s32 module);

	template <typename T> static bool HasObjectLoaded(T , s32 )			{ T::__Not_A_Valid_Function(); return(false); }
	template <typename T> static bool RequestObject(T , s32 , s32 )		{ T::__Not_A_Valid_Function(); return(false); }
	template <typename T> static void RemoveObject(T , s32 )			{ T::__Not_A_Valid_Function();  }
	template <typename T> static void ClearRequiredFlag(T, s32 , s32 )	{ T::__Not_A_Valid_Function();  }
	template <typename T> static bool LoadObject(T, s32, bool defraggable=false, bool bCanBeDeleted=false)    { T::__Not_A_Valid_Function(); return(false); }

	template <typename T> static void SetObjectIsNotDeletable(T , s32 )			{ T::__Not_A_Valid_Function();  }
	template <typename T> static void SetObjectIsDeletable(T , s32 )			{ T::__Not_A_Valid_Function();  }
	template <typename T> static bool GetObjectIsDeletable(T , s32 )			{ T::__Not_A_Valid_Function();  return(false); }
	template <typename T> static void SetDoNotDefrag(T , s32 )				{ T::__Not_A_Valid_Function();  }
	template <typename T> static void SetMissionDoesntRequireObject(T , s32 )	{ T::__Not_A_Valid_Function();  }
	template <typename T> static void SetCutsceneDoesntRequireObject(T , s32 )	{ T::__Not_A_Valid_Function();  }
	template <typename T> static void SetInteriorDoesntRequireObject(T , s32 )	{ T::__Not_A_Valid_Function();  }

	template <typename T> static u32 GetObjectFlags(T , s32 )					{ T::__Not_A_Valid_Function();  return(0); }
	template <typename T> static u32 GetObjectVirtualSize(T , s32 )			{ T::__Not_A_Valid_Function();  return(0); }
	template <typename T> static u32 GetObjectPhysicalSize(T , s32 )			{ T::__Not_A_Valid_Function();  return(0); }
	template <typename T> static void GetObjectAndDependenciesSizes(T , s32 , u32& , u32& , const strIndex* ignoreList=NULL, s32 numIgnores=0, bool mayFail = false) { T::__Not_A_Valid_Function();  }
	template <typename T> static void GetObjectAndDependencies(T , s32 , atArray<strIndex>& , const strIndex* , s32 )		{ T::__Not_A_Valid_Function(); }
	template <typename T> static bool IsObjectRequested(T , s32 )				{ T::__Not_A_Valid_Function();  return(false); }
	template <typename T> static bool IsObjectLoading(T , s32 )				{ T::__Not_A_Valid_Function();  return(false); }
	template <typename T> static bool IsObjectInImage(T , s32 )				{ T::__Not_A_Valid_Function();  return(false); }
	template <typename T> static bool IsObjectNew(T , s32 )					{ T::__Not_A_Valid_Function();  return(false); }

	static u64 GetImageTime(strLocalIndex index);
	static const char* GetImageFilename(strLocalIndex index);
	static void SetImageIsNew(strLocalIndex index);
	static bool GetImageIsNew(strLocalIndex index);

#if ENABLE_DEFRAG_CALLBACK
	static bool IsObjectReadyToDelete(void* ptr);
#endif

#if __BANK
	static void InitWidgets();
#endif

	// access to sub modules.
	static CStreamingCleanup& GetCleanup() {return ms_cleanup;}

#if !__FINAL
	static CStreamingDebug& GetDebug() {return ms_debug;}
#endif

    static void SetKeepAliveCallback(void (*callback)(), u32 intervalInMS);

	static pgBase* PlaceDrawable(datResourceInfo& info,datResourceMap &map, const char *debugName, bool isDefrag);
	static pgBase* DefragmentDrawable(datResourceMap& map);

	static pgBase* PlaceFragType(datResourceInfo& info,datResourceMap &map, const char *debugName, bool isDefrag);
	static pgBase* DefragmentFragType(datResourceMap& map);

	static pgBase* PlaceDwd(datResourceInfo& info,datResourceMap &map, const char *debugName, bool isDefrag);
	static pgBase* DefragmentDwd(datResourceMap& map);

	static void RegisterStreamingFiles();

	static void MarkFirstInitComplete(bool complete = true)			{ ms_FirstInitComplete = complete; }

	static void ScriptSuppressHdImapsThisFrame() { ms_scriptSuppressingHdImaps = true; }

	static void SetIsPlayerPositioned(bool val) { ms_isPlayerPositioned = val; }
	static bool IsPlayerPositioned() { return ms_isPlayerPositioned; }
	static void RequestRenderThreadFlush() { ms_bRequestRenderThreadFlush=true; }

#if RSG_PC
	static void SetForceEnableHdMapStreaming(bool bForce) { ms_bForceEnableHdImaps = bForce; }
#endif	//RSG_PC

private:
	static const char *GetStreamingContext();
#if STREAMING_VISUALIZE
	static int EntityGuidGetter(const fwEntity *entity);
	static float EntityBoundSphereGetter(const fwEntity *entity, Vector3 &outVector);
	static void AddStackContext();
#endif // STREAMING_VISUALIZE
	static char					ms_streamingFilePath[RAGE_MAX_PATH];
	static CStreamingCleanup	ms_cleanup;
	static bool					ms_FirstInitComplete;

#if !__FINAL
	static CStreamingDebug		ms_debug;
	static bool					ms_pauseStreaming;
#endif
	static bool					ms_enableSceneStreaming;

#if RSG_PC
	static bool					ms_bForceEnableHdImaps;
#endif
	
	static bool					ms_scriptSuppressingHdImaps;
	static bool					ms_isPlayerPositioned;
	static bool					ms_bRequestRenderThreadFlush;

	static Vector3				ms_oldFocusPosn;
};

#endif // STREAMING_STREAMING_H

