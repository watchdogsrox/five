/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/debug/SceneStreamerDebug.h
// PURPOSE : debug widgets related to the scene streamer
// AUTHOR :  Ian Kiigan
// CREATED : 13/10/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _SCENE_DEBUG_SCENESTREAMERDEBUG_H_
#define _SCENE_DEBUG_SCENESTREAMERDEBUG_H_

#if __BANK

#include "bank/bank.h"
#include "bank/bkmgr.h"

class CEntity;

// some random debug widgets pulled out of scene streamer mgr
class CSceneStreamerDebug
{
public:
	void AddWidgets(bkBank* pBank);
	bool IsHighPriorityEntity(const CEntity* pEntity);
	void Update();

	static inline bool TraceSelected()		{ return ms_bTraceSelected;	}
	static inline bool ColorizeScene()		{ return ms_bColorizeScene;	}
	static inline bool TrackLoadScene()		{ return ms_bTrackSceneLoader; }
	static inline void SetDrawRequestsOnVmap(bool enable)	{ ms_bDrawSceneStreamerRequestsOnVmap = enable; }
	static inline bool DrawRequestsOnVmap()	{ return ms_bDrawSceneStreamerRequestsOnVmap; }
	static inline bool DrawDeletionsOnVmap()	{ return ms_bDrawSceneStreamerDeletionsOnVmap; }
	static inline bool DrawRequiredOnVmap()	{ return ms_bDrawSceneStreamerRequiredOnVmap; }
	static inline bool DrawSelectedOnVmap()	{ return ms_bDrawSceneStreamerSelectedOnVmap; }
	static inline bool RequestAllPriorities()	{ return ms_SceneStreamerRequestsAllPriorities; }

private:
	static void FlushSceneNow();
	static void LoadSceneNow();

	static s32				ms_nSelectedLoader;
	static bool				ms_bTraceSelected;
	static bool				ms_bColorizeScene;
	static bool				ms_bTrackSceneLoader;
	static bool				ms_bAsyncLoadScenePending;

public:
	static bool				ms_bDrawSceneStreamerRequestsOnVmap;
	static bool				ms_bDrawSceneStreamerDeletionsOnVmap;
	static bool				ms_bDrawSceneStreamerRequiredOnVmap;
	static bool				ms_bDrawSceneStreamerSelectedOnVmap;
	static bool				ms_SceneStreamerRequestsAllPriorities;
};
extern CSceneStreamerDebug g_SceneStreamerDebug;

#endif //__BANK

#endif //_SCENE_DEBUG_SCENESTREAMERDEBUG_H_