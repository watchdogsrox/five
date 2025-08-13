////////////////////////////////////////////////////////////////////////////////////
// Title	:	PortalDebugHelper.h
// Author	:	John
// Started	:	10/2/2010
//			:	Portal debug code
////////////////////////////////////////////////////////////////////////////////////
#ifndef _CORE_SCENE_PORTAL_DEBUG_HELPER_H_
#define _CORE_SCENE_PORTAL_DEBUG_HELPER_H_



// Rage headers

// Game headers
#include "scene\portals\interiorInst.h"
#include "fwscene/interiors/PortalTypes.h"




#if __BANK
// portal rendering stats
extern s32 numMLOsInScene;
extern s32 numLowLODRenders;
extern s32 numHighLODRenders;
extern s32 numIntModelsRendered;

extern s32 numCurrIntModelsRendered;
extern s32 numCurrIntTrackers;
extern s32 numCurrIntRooms;
extern s32 numCurrIntModels;

// these are used to iterate through interiors during navmesh export
extern void SelectFirstMlo();
extern bool SelectNextMlo();
extern bool MloTeleportPlayer(bool bSelectLow);
extern bool MloGetTeleportPos(bool bSelectLow, Vector3 & vTeleportPos);
extern CMloModelInfo * GetSelectedInterior();
extern const char * GetSelectedInteriorName();
#endif //__BANK


class CEntity;
class CInteriorInst;
class CViewport;
class CPortalVisTracker;

namespace rage {
	class fwPortalCorners;
}

struct roomDebugData
{
	u8	red;
	u8	green;
	u8	blue;
	bool	bRenderObjects;
	bool	bRenderObjPts;
	bool	bRenderPortals;
};

class CPortalDebugHelper
{
	friend class CPortal;
	friend class CPortalTracker;
	friend class CNavMeshDataExporter;

public:

	static	void Init(unsigned initMode);
	static	void Shutdown(unsigned shutdownMode);
	static	void Update();

#if __BANK
	static bool	InitWidgets(void);
	static void InitInteriorWidgets(void);

	static void DebugPosition(const Vector3& pos, u32 fade, u32 roomIdx);

	static void DebugRoom(CInteriorInst* pInterior, u32 currRoomIdx);

	static void UpdateMloList(void);
	static void UpdateEntitySetListCB(void);
	static void GetCurrentInteriorCB(void);
	static void ChangeActiveSetCB(void);
	static void ChangeInactiveSetCB(void);

	static void RefreshInteriorCB(void);

	static void DumpMLODebugData(void);
	static void DumpActiveInteriorList(void);

	static void DisplayMemUse(void);

	static void DebugOutput(void);
#endif //__BANK


#if __DEV
	static bool BuildCamLocationString(char* cTempString);
#endif //__DEV
};



#endif //_CORE_SCENE_PORTAL_DEBUG_HELPER_H_
