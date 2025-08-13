////////////////////////////////////////////////////////////////////////////////////
// Title	:	Portal.h
// Author	:	John
// Started	:	28/10/05
//			:	Portal rendering using data from CInteriorInst & CInteriorInfo_DEPRECATED instances
////////////////////////////////////////////////////////////////////////////////////
#ifndef _CORE_SCENE_PORTAL_H_
#define _CORE_SCENE_PORTAL_H_



// Rage headers

// Framework headers
#include "fwscene/interiors/PortalTypes.h"

// Game headers
#include "scene/portals/interiorInst.h"


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

// one of these is required by the renderer in order to determine visibility correctly


class CPortal
{
	friend class CPortalDebugHelper;
	friend class CPortalTracker;

	// TODO: Consider replacing these friend declarations with accessors. /FF
	friend class CNavMeshDataExporterInGame;
	friend class CNavMeshDataExporterTool;

enum {
	PORTAL_UPDATE_NUM_FRAMES_TO_PROCESS		= 1,
};

public:

	struct modifierData {
		spdSphere portalBoundSphere;
		float strength;
#if __ASSERT
		atHashWithStringNotFinal mainModifierName;
		atHashWithStringNotFinal secondaryModifierName;
#endif
		int mainModifier;
		int secondaryModifier;
		float blendWeight;
	};

	typedef atFixedArray<modifierData,20> modifierQueryResults;


	static	void Init(unsigned initMode);
	static  void Shutdown(unsigned shutdownMode);
	static	void Update();
	static	void UpdateVisibility(CInteriorInst* pInterior);

	static fwModelId GetPortalInstModelId(void) { return(m_portalInstModelId); }

	static void AppendToActiveBuildingsArray( atArray<CEntity*>& entityArray );

	static void	AddToActiveInteriorList(CInteriorInst* pT) {if (!ms_activeInteriorList.IsMemberOfList(pT)) {ms_activeInteriorList.Add(pT);}}
	static void	RemoveFromActiveInteriorList(CInteriorInst *pT) { if (ms_activeInteriorList.IsMemberOfList(pT)) {ms_activeInteriorList.Remove(pT);}}

	static void	AddToActivePortalObjList(CInteriorInst* pT) {if (!ms_activePortalObjList.IsMemberOfList(pT)) {ms_activePortalObjList.Add(pT);}}
	static void	RemoveFromActivePortalObjList(CInteriorInst *pT) { if (ms_activePortalObjList.IsMemberOfList(pT)) {ms_activePortalObjList.Remove(pT);}}
	static fwPtrList& GetActiveActivePortalObjList(void) { return(ms_activePortalObjList);}
	static void	ClearActiveActivePortalObjList(void) {ms_activePortalObjList.Flush();}

	static bool	IsInteriorScene() { return(ms_bIsInteriorScene);}
	static bool IsPlayerInTunnel() { return(ms_bPlayerIsInTunnel); }
	static bool IsGPSAvailable()	{ return(ms_bGPSAvailable); }

	static float GetDoorOcclusion(const CInteriorInst* pIntInst, u32 globalPortalIdx);
	static void CalcAmbientScale(Vec3V_In coords, Vec3V_InOut baseAmbientScales, Vec3V_InOut bleedingAmbientScales, fwInteriorLocation intLoc);

	static void FindModifiersForInteriors(const grcViewport& viewPort, CInteriorInst* pIntInst, u32 roomId, modifierQueryResults *results);
	static void FindModifiersForInteriorsR(const grcViewport& viewPort, CInteriorInst* pIntInst, u32 roomId, modifierQueryResults *results, int recursionLevel, float parentfactor, const Vector3* parentPoartalSphereCentre);

	static int FindCrossedPortalRoom(const CInteriorInst* pIntInst, u32 roomId, Vec3V_In pos, Vec3V_In posDelta);
	
	static bool GetNoDirectionalLight();
	static bool GetNoExteriorLights();

	static void ActivateCollisions(Vector3& mloActivateStart, Vector3& mloActivateVec);

	static void CalcInteriorFxLevel();
	static float GetInteriorFxLevel();

	static void ActivateInteriorGroupsUsingCamera() { ms_bActivateInteriorGroupsUsingCamera = true; }

	static bool	ms_bIsWaterReflectionDisabled;

	static float ms_nearestDistToPortal;

	static int ms_interiorPopulationScanCode;

	static bool IsActiveGroup(u32 groupId) { return( ((1 << groupId) & ms_activeGroups) != 0); }

	static void ResetMetroRespawnTimer()	{ ms_metroRespawnTimer.Reset(); }

private:
	static void EnsurePlayerActivatesInteriors();
	static void	ManageInterior(CInteriorInst *pIntInst);		//handles conversion of dummies in MLO to real objects
	static void	ConvertToDummy(CInteriorInst *pIntInst);		//force contents of the MLO to become dummies
	static fwPtrListSingleLink ms_activeInteriorList;		// list of interiors which are currently active

	static fwPtrListSingleLink ms_activePortalObjList;		// list of interiors which have active portal objects (need to add collisions for)

	static fwPtrListSingleLink ms_intersectionList;			// working list of entities built from results of intersection tests

	static bool AddToIntersectionListCB(CEntity* pEntity, void* data);
	static void ActivateIntersectionList(void);

	static bool MloFxLevelCB(void* pItem, void* data);

	static void	UpdateInteriorScene();

	static void UpdateTunnelSections(u32 scanCode, CInteriorInst* pIntInst);

	static void UpdatePlayerVelocity();
	static bool InteriorFrustumCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT);
	static bool InteriorProximityCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT);
	static bool InteriorGroupCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT, s32 proxyID);
	static bool InteriorShortTunnelCheck(const CInteriorProxy* pIntProxy, CPortalTracker* pPT);

	static fwModelId m_portalInstModelId;

	static	bool	ms_InScanPhase;

	static bool		ms_bIsInteriorScene;
	static bool		ms_bPlayerIsInTunnel;
	static bool		ms_bGPSAvailable;
	static bool		ms_bActivateInteriorGroupsUsingCamera;

	static float	ms_interiorFxLevel;

	static Vector3	ms_lastPlayerVelocityOrigin;
	static Vector3	ms_lastPlayerVelocityDirection;

	static u32		ms_activeGroups;
	static u32		ms_newlyActiveGroups;

	static sysTimer ms_metroRespawnTimer;
};



#endif //_PORTAL_H_
