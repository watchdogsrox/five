#ifndef LAYOUTMANAGER_H
#define LAYOUTMANAGER_H

// Game headers
#include "streaming/streamingrequest.h"

//rage
#include "math/random.h"

//fw
#include "fwscript/scripthandler.h"
#include "fwsys/metadatastore.h"
#include "fwscene/world/InteriorLocation.h"

//game
#include "Scene/RegdRefTypes.h"
#include "scene/portals/LayoutManagerParser.h"

class CInteriorInst;




class CLayoutManager
{
public:
	typedef void (NodeProcessCB)(CRsRef* pNode);

	enum EProcessType{
		LM_PRINT_NODE,
		LM_POPULATE_NODE,
		LM_POPULATE_WIDGETS_NODE,
		LM_RANDOMIZE_ACTIVE
	};

	CLayoutManager();
	~CLayoutManager();
	void Init();
	void Shutdown();
	u32 LoadLayout(const char* layoutName);
	void UnloadLayout();

	void PopulateInteriorWithLayout(CInteriorInst* pIntInst);
	void DeleteLayout(void);

	CMiloInterior*	GetInteriorFromHandle(u32 interiorHandle);
	CMiloRoom*		GetRoomFromHandle(u32 roomHandle);
	CLayoutNode*	GetLayoutNode(u32 layoutNodeHandle);
	CGroup*			GetGroupNode(u32 groupNodeHandle);
	CRsRef*			GetRsRefNode(u32 rsRefHandle);

	u32 RegisterInteriorHandle(CMiloInterior* node);
	u32 RegisterRoomHandle(CMiloRoom* node);
	u32 RegisterLayoutHandle(CLayoutNode* node);
	u32 RegisterGroupHandle(CGroup* node, atHashString parent);
	u32 RegisterGroupHandle(CGroup* node);
	u32 RegisterRsRefHandle(CRsRef* node);
	template <typename T>
	u32 CreateHandle(T* node)
	{
		return atDataHash ( (const char*)&node, sizeof(node), 0);
	}
	template <typename T>
	u32 InsertHandle(atBinaryMap<T*,u32> map, T* node)
	{
		u32 handle = atDataHash ( (const char*)&node, sizeof(node), 0);// CreateHandle(node);
		map.SafeInsert(handle,node);
		map.FinishInsertion();
		return handle;
	}

	void InitRoomsListFromCurrentFile(void);
	void ClearPopulationWidgets(void);

	void SetActiveStateForGroup(int groupID, bool bState);
	bool IsActiveGroupId(u32 groupID);

#if __BANK
	static void loadfileCB(void);
	static void GetInteriorName();
	static void randomizeActiveGroupsCB();
	static void populateLayoutCB();
	static void depopulateLayoutCB();
	static void saveActiveGroupsCB();
	static void restoreActiveGroupsCB();

	static void NextObjCB();
	static void PrevObjCB();

	void SaveActiveStateToFile();
	void LoadActiveStateFromFile();

	bool InitWidgets(void);
#endif// __BANK

private:
	void SelectRoomClickCB(s32 hash);
	void SelectLayoutClickCB(s32 hash);
	void ActivateGroupClickCB(s32 hash);

	void SelectSubLayoutClickCB(s32 hash);
	void ActivateSubGroupClickCB(s32 hash);

	void PrintInterior(CMiloInterior* interior);
	void PopulateInterior(CMiloInterior* interior);
	void RandomizeActiveGroups(CMiloInterior* interior);

	void VisitRsRef(CRsRef* ref, EProcessType op);
	void VisitGroup(CGroup* ref, EProcessType op);
	void VisitLayout(CLayoutNode* node, EProcessType op);
	void VisitRoom(CMiloRoom* room, EProcessType op);
	void VisitInterior(CMiloInterior* interior, EProcessType op);

	void AddNodeEntity(CRsRef* refNode);

	static void PrintNodeProcess(CRsRef* refNode);

	typedef atBinaryMap<CLayoutNode*, u32> LayoutHandleMap;
	typedef atBinaryMap<CGroup*, u32> GroupHandleMap;
	typedef atBinaryMap<CMiloRoom*, u32> RoomHandleMap;
	typedef atBinaryMap<CMiloInterior*, u32> InteriorHandleMap;
	typedef atBinaryMap<CRsRef*, u32> RsRefHandleMap;
	struct sRoomLayoutStruct
	{
		CMiloRoom* m_room;
		LayoutHandleMap layoutHandles;
		GroupHandleMap  groupHandles;
		RsRefHandleMap	rsRefHandles;
	};
	static atArray<sRoomLayoutStruct*> ms_rooms;
	static LayoutHandleMap ms_layoutHandles;
	static GroupHandleMap ms_groupHandles;
	static RoomHandleMap ms_roomHandles;
	static InteriorHandleMap ms_interiorHandles;
	static RsRefHandleMap ms_rsRefHandles;

	atArray<fwEntity*>			m_layoutEntities;
	atArray<u32>				m_activeIDs;
	CInteriorInst*				m_pLayoutTarget;
	u32							m_currentFileHandle;
		
	fwInteriorLocation			m_currentLocation;
	Mat34V						m_currentTransform;
	Vec3V						m_entityPos;

	static mthRandom			ms_random;

#if __BANK
	static bkList*						ms_roomsList;
	static bkList*						ms_layoutsList;
	static bkList*						ms_groupsList;

	static u32							ms_currRoomHandle;
	static u32							ms_currLayoutHandle;
	static u32							ms_currGroupHandle;

	static bkList*						ms_subLayoutsList;
	static bkList*						ms_subGroupsList;
#endif //__BANK

};

extern CLayoutManager g_LayoutManager;

#endif

