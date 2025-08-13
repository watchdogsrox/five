#include "LayoutManager.h"

#include "peds/ped.h"
#include "scene/scene_channel.h"
#include "scene/portals/Portal.h"
#include "bank/bkmgr.h"
#include "bank/list.h"

#include "math/random.h"

#define NAME_LENGTH 64
//char layoutFileName[NAME_LENGTH] = "dlc_mpApartment:/common/data/int_mp_h_04_GtaMloRoom001.xml";		//59 chars!

//char layoutPath[NAME_LENGTH] = "dlc_mpApartment:/common/data/";
//char layoutFileName[32] = "int_mp_h_04";

char layoutPath[NAME_LENGTH] = "_";
char layoutFileName[32] = "int_mp_h_04";

#define MAX_LAYOUT_ENTITIES 300

CLayoutManager g_LayoutManager;

CLayoutManager::LayoutHandleMap			CLayoutManager::ms_layoutHandles;
CLayoutManager::GroupHandleMap			CLayoutManager::ms_groupHandles;
CLayoutManager::RoomHandleMap			CLayoutManager::ms_roomHandles;
CLayoutManager::InteriorHandleMap		CLayoutManager::ms_interiorHandles;
CLayoutManager::RsRefHandleMap			CLayoutManager::ms_rsRefHandles;

mthRandom	CLayoutManager::ms_random;

#if __BANK
bkList* CLayoutManager::ms_roomsList = NULL;
bkList* CLayoutManager::ms_layoutsList = NULL;
bkList* CLayoutManager::ms_groupsList = NULL;
bkList* CLayoutManager::ms_subLayoutsList = NULL;
bkList* CLayoutManager::ms_subGroupsList = NULL;

u32		CLayoutManager::ms_currRoomHandle = 0;
u32		CLayoutManager::ms_currLayoutHandle = 0;
u32		CLayoutManager::ms_currGroupHandle = 0;
#endif //__BANK

//OPTIMISATIONS_OFF();

CLayoutManager::CLayoutManager()
{
	m_currentFileHandle = 0;
	m_pLayoutTarget = NULL;
	m_currentLocation.MakeInvalid();

	m_layoutEntities.Reset();
	m_layoutEntities.Reserve(MAX_LAYOUT_ENTITIES);

	m_activeIDs.Reset();
	m_activeIDs.Reserve(MAX_LAYOUT_ENTITIES);

	m_currentTransform.SetIdentity3x3();
}

CLayoutManager::~CLayoutManager()
{

}

// top level interface
void CLayoutManager::PrintInterior(CMiloInterior* interiorPtr)
{
	VisitInterior(interiorPtr, CLayoutManager::LM_PRINT_NODE);
}

void CLayoutManager::PopulateInterior(CMiloInterior* interiorPtr)
{
	VisitInterior(interiorPtr, CLayoutManager::LM_POPULATE_NODE);
}

void CLayoutManager::RandomizeActiveGroups(CMiloInterior* interiorPtr)
{
	VisitInterior(interiorPtr, CLayoutManager::LM_RANDOMIZE_ACTIVE);
}

void CLayoutManager::SetActiveStateForGroup(int groupID, bool bState)
{
	u32 currentIdx = m_activeIDs.Find(groupID);

	if (bState)
	{
		if (currentIdx == -1)
		{
			// make active if not already active
			m_activeIDs.Push(groupID);
		}
	}
	else
	{
		if (currentIdx != -1)
		{
			// make inactive if alread active
			m_activeIDs.Delete(currentIdx);
		}
	}
}

bool CLayoutManager::IsActiveGroupId(u32 groupID)
{
	return(m_activeIDs.Find(groupID) != -1);
}

void CLayoutManager::VisitInterior(CMiloInterior* interiorPtr, EProcessType op)
{
	u32 currentInterior = CreateHandle(interiorPtr);
	if(CMiloInterior* interior = GetInteriorFromHandle(currentInterior))
	{
		if (m_pLayoutTarget)
		{
			const fwTransform*	pTransform = m_pLayoutTarget->GetTransformPtr();
			m_currentTransform = pTransform->GetMatrix();
		}

		for(int i=0;i<interior->m_RoomList.GetCount();i++)
		{
			CMiloRoom* room = &interior->m_RoomList[i];
			VisitRoom(room, op);
		}
	}
}

void CLayoutManager::VisitRoom(CMiloRoom* roomPtr, EProcessType op)
{
	u32 currentRoom = CreateHandle(roomPtr);
	if(CMiloRoom* room = GetRoomFromHandle(currentRoom))
	{
		if (m_pLayoutTarget)
		{
			u32 roomIdx = m_pLayoutTarget->FindRoomByHashcode(atHashString(room->m_Name.c_str()));
			CInteriorProxy *pProxy = m_pLayoutTarget->GetProxy();
			m_currentLocation = CInteriorProxy::CreateLocation(pProxy, roomIdx);
		}
		else
		{
			m_currentLocation.MakeInvalid();
		}

		for(int i=0;i<room->m_LayoutNodeList.GetCount();i++)
		{
			CLayoutNode* node = &room->m_LayoutNodeList[i];
			u32 currentNode = CreateHandle(node);
			VisitLayout(GetLayoutNode(currentNode), op);
		}
	}
}

void CLayoutManager::VisitLayout(CLayoutNode* node, EProcessType op)
{
	if(node)
	{
		Mat34V oldTransform = m_currentTransform;

		if (m_pLayoutTarget)
		{
			Mat34V localTransform;

			Vector3 localAngles(node->m_Rotation);
			localAngles *= DtoR;

			Quaternion localQuat;
			localQuat.FromEulers(localAngles,  "xyz");
			QuatV localQuatV = QUATERNION_TO_QUATV(localQuat);

			Mat34VFromQuatV(localTransform, localQuatV, VECTOR3_TO_VEC3V(node->m_Translation));

			Transform(m_currentTransform, localTransform);
		}

		atArray<CGroup*>& groups = node->m_GroupList;
		s32 randomlySelectedGroupIdx = ms_random.GetRanged(0, groups.GetCount()-1);

		for(int j=0;j<groups.GetCount();j++)
		{
			CGroup* groupNode = groups[j];
			u32 currentNode = CreateHandle(groupNode);

			// yuk. do this better
			if ((op == LM_POPULATE_NODE) && !IsActiveGroupId(groupNode->m_Id))
			{
				continue;
			}

			if (op == LM_RANDOMIZE_ACTIVE)
			{
				SetActiveStateForGroup(groups[j]->m_Id, (randomlySelectedGroupIdx == j));
			}

			VisitGroup(GetGroupNode(currentNode), op);
		}

		m_currentTransform = oldTransform;		// restore transform on exit of branch
	}
}

void CLayoutManager::VisitGroup(CGroup* group, EProcessType op)
{
	if(group)
	{
		for (int i=0;i<group->m_RefList.GetCount();i++)
		{			
			CRsRef* rsRefNode = group->m_RefList[i];
			u32 currentRsRef = CreateHandle(rsRefNode);
			VisitRsRef(GetRsRefNode(currentRsRef), op);
		}
	}
}

void CLayoutManager::VisitRsRef(CRsRef* refNode, EProcessType op)
{
	if(refNode)
	{
		Mat34V oldTransform = m_currentTransform;

		atArray<CLayoutNode*>& layouts = refNode->m_LayoutNodeList;

		Mat34V localTransform;

		Vector3 localAngles(refNode->m_Rotation);
		localAngles *= DtoR;

		Quaternion localQuat;
		localQuat.FromEulers(localAngles,  "xyz");
		QuatV localQuatV = QUATERNION_TO_QUATV(localQuat);
	
		Mat34VFromQuatV(localTransform, localQuatV, VECTOR3_TO_VEC3V(refNode->m_Translation));

		Transform(m_currentTransform, localTransform);

		m_entityPos = m_currentTransform.d();

		// actual processing of node
		switch(op)
		{
			case LM_POPULATE_NODE:
				g_LayoutManager.AddNodeEntity(refNode);
				break;
			case LM_PRINT_NODE:
				PrintNodeProcess(refNode);
				break;
			default:
				break;
		}

		// recurse processing into any further nodes
		for(int x = 0; x<layouts.GetCount(); x++)
		{
			CLayoutNode* node = layouts[x];
			u32 currentNode = CreateHandle(node);
			VisitLayout(GetLayoutNode(currentNode), op);
		}

		m_currentTransform = oldTransform;		// restore transform on exit of this branch
	}
}

void CLayoutManager::PrintNodeProcess(CRsRef* OUTPUT_ONLY(refNode))
{
	Displayf("\t\t\tGroup Node: %s",refNode->m_Name.c_str());
	Displayf("\t\t\tTranslation x: %.3f y : %.3f z: %.3f",refNode->m_Translation.x, refNode->m_Translation.y, refNode->m_Translation.z);
	Displayf("\t\t\tRotation x: %.3f y : %.3f z: %.3f",refNode->m_Rotation.x, refNode->m_Rotation.y, refNode->m_Rotation.z);
}

void CLayoutManager::AddNodeEntity(CRsRef* refNode)
{
	sceneAssert(m_pLayoutTarget);

	if (m_pLayoutTarget)
	{
		// create an entity from the refNode and add it to the target
		fwModelId modelId;
		CBaseModelInfo *pBMI = CModelInfo::GetBaseModelInfoFromName(refNode->m_Name.c_str(), &modelId);
		if (pBMI && (m_layoutEntities.GetCount() < m_layoutEntities.GetCapacity()))
		{
			CEntity* pEntity = static_cast<CEntity*>(pBMI->CreateEntity());

			pEntity->SetModelId(modelId);

			#if ENABLE_MATRIX_MEMBER			
			pEntity->SetTransform( m_currentTransform );
			pEntity->SetTransformScale(1.0f, 1.0f);
			#else
			fwTransform* trans = rage_new fwMatrixTransform;
			trans->SetMatrix(m_currentTransform);
			pEntity->SetTransform(trans);
			#endif

			pEntity->SetOwnedBy(ENTITY_OWNEDBY_LAYOUT_MGR);
			pEntity->SetBaseFlag(fwEntity::USE_SCREENDOOR);

			CGameWorld::Add(pEntity, m_currentLocation);	
			pEntity->SetAlpha(255);

			m_layoutEntities.Push(pEntity);
		}
	}
}



void CLayoutManager::Init()
{
	//LoadLayout("common:/data/int_mp_h_04_GtaMloRoom001.xml");
}

void CLayoutManager::UnloadLayout()
{
	ClearPopulationWidgets();

	for(InteriorHandleMap::Iterator it = ms_interiorHandles.Begin(); it!= ms_interiorHandles.End();++it)
	{
		delete (*it);
	}
	ms_interiorHandles.Reset();
	ms_roomHandles.Reset();
	ms_groupHandles.Reset();
	ms_layoutHandles.Reset();
	ms_rsRefHandles.Reset();

	m_currentFileHandle = 0;
}

u32 CLayoutManager::LoadLayout(const char* layoutName)
{
	ms_random.Reset(sysTimer::GetSystemMsTime());

	char filename[128];
	sprintf(filename, "%s%s.xml",layoutPath, layoutName);

	CMiloInterior* miloInterior = rage_new CMiloInterior;
	bool ret = PARSER.LoadObject(filename, "", *miloInterior);

	if(ret)
	{
		u32 handle = RegisterInteriorHandle(miloInterior);
		m_currentFileHandle = handle;

		InitRoomsListFromCurrentFile();

		return handle;
	}
	else
	{
		delete miloInterior;
	}
	return 0;
}

u32 CLayoutManager::RegisterInteriorHandle(CMiloInterior* node)
{
	u32 handle = atDataHash ( (const char*)&node, sizeof(node), 0); 
	ms_interiorHandles.SafeInsert(handle,node);
	ms_interiorHandles.FinishInsertion();
	for (int i=0;i<node->m_RoomList.GetCount();i++)
	{
		RegisterRoomHandle(&(node->m_RoomList[i]));
	}
	return handle;
}

u32 CLayoutManager::RegisterRoomHandle(CMiloRoom* node)
{
	u32 handle = atDataHash ( (const char*)&node, sizeof(node), 0); 
	ms_roomHandles.SafeInsert(handle,node);
	ms_roomHandles.FinishInsertion();
	for (int i=0;i<node->m_LayoutNodeList.GetCount();i++)
	{
		RegisterLayoutHandle(&(node->m_LayoutNodeList[i]));
	}
	return handle;
}

u32 CLayoutManager::RegisterLayoutHandle(CLayoutNode* node)
{
	u32 handle = atDataHash ( (const char*)&node, sizeof(node), 0); 
	ms_layoutHandles.SafeInsert(handle,node);
	ms_layoutHandles.FinishInsertion();
	for (int i=0;i<node->m_GroupList.GetCount();i++)
	{
		RegisterGroupHandle(node->m_GroupList[i]);
	}
	return handle;
}
u32 CLayoutManager::RegisterGroupHandle(CGroup* node)
{
	u32 handle = atDataHash ( (const char*)&node, sizeof(node), 0); 
	ms_groupHandles.SafeInsert(handle,node);
	ms_groupHandles.FinishInsertion();
	for (int i=0;i<node->m_RefList.GetCount();i++)
	{
		RegisterRsRefHandle(node->m_RefList[i]);
	}
	return handle;
}
u32 CLayoutManager::RegisterRsRefHandle(CRsRef* node)
{
	u32 handle = atDataHash ( (const char*)&node, sizeof(node), 0); 
	ms_rsRefHandles.SafeInsert(handle,node);
	ms_rsRefHandles.FinishInsertion();
	for (int i=0;i<node->m_LayoutNodeList.GetCount();i++)
	{
		RegisterLayoutHandle(node->m_LayoutNodeList[i]);
	}
	return handle;
}

CMiloInterior* CLayoutManager::GetInteriorFromHandle(u32 interiorHandle)
{
	if(ms_interiorHandles.Access(interiorHandle))
	{
		CMiloInterior* interior = ms_interiorHandles[interiorHandle];
		return interior;
	}
	else
	{
		return NULL;
	}
}

CMiloRoom* CLayoutManager::GetRoomFromHandle(u32 roomHandle)
{
	if(ms_roomHandles.Access(roomHandle))
	{
		CMiloRoom* room = ms_roomHandles[roomHandle];

		return room;
	}
	else
	{
		return NULL;
	}
}

CLayoutNode* CLayoutManager::GetLayoutNode(u32 layoutHandle)
{
	if(ms_layoutHandles.Access(layoutHandle))
	{
		return ms_layoutHandles[layoutHandle];
	}
	else
	{
		return NULL;
	}
}

CGroup* CLayoutManager::GetGroupNode(u32 groupHandle)
{
	if(ms_groupHandles.Access(groupHandle))
	{
		return ms_groupHandles[groupHandle];
	}
	else
	{
		return NULL;
	}
}

CRsRef* CLayoutManager::GetRsRefNode(u32 rsRefHandle)
{
	if(ms_rsRefHandles.Access(rsRefHandle))
	{
		return ms_rsRefHandles[rsRefHandle];
	}
	else
	{
		return NULL;
	}
}

void CLayoutManager::PopulateInteriorWithLayout(CInteriorInst* pIntInst)
{
	if (pIntInst && !pIntInst->m_bIsLayoutPopulated && m_currentFileHandle)
	{
		m_pLayoutTarget = pIntInst;

		CLayoutManager::PopulateInterior(GetInteriorFromHandle(m_currentFileHandle));

		pIntInst->m_bIsLayoutPopulated = true;
	}
}

void CLayoutManager::DeleteLayout(void)
{
	if (m_pLayoutTarget && m_pLayoutTarget->m_bIsLayoutPopulated)
	{
		for(u32 i = 0; i<m_layoutEntities.GetCount(); i++)
		{
			if (m_layoutEntities[i])
			{
				CGameWorld::Remove(static_cast<CEntity*>(m_layoutEntities[i]));	
				delete(m_layoutEntities[i]);
				m_layoutEntities[i] = NULL;
			}
		}

		m_layoutEntities.Reset();
		m_layoutEntities.Reserve(MAX_LAYOUT_ENTITIES);

		m_pLayoutTarget->m_bIsLayoutPopulated = false;
		m_pLayoutTarget = NULL;
	}
}

void CLayoutManager::InitRoomsListFromCurrentFile()
{
#if __BANK
	CMiloInterior* pInterior = GetInteriorFromHandle(m_currentFileHandle);

	if (pInterior)
	{
		for(s32 i=0; i<pInterior->m_RoomList.GetCount(); i++)
		{
			CMiloRoom* pRoom = &pInterior->m_RoomList[i];
			u32 roomHandle = CreateHandle(pRoom);
			ms_roomsList->AddItem(roomHandle, 0, (int)i);
			ms_roomsList->AddItem(roomHandle, 1, pRoom->m_Name.c_str());
		}
	}
#endif //__BANK
}

void CLayoutManager::ClearPopulationWidgets()
{
#if __BANK
	SelectRoomClickCB(0);
	SelectLayoutClickCB(0);

	CMiloInterior* pInterior = GetInteriorFromHandle(m_currentFileHandle);

	if (pInterior)
	{
		for(s32 i=0; i<pInterior->m_RoomList.GetCount(); i++)
		{
			CMiloRoom* pRoom = &pInterior->m_RoomList[i];
			u32 roomHandle = CreateHandle(pRoom);
			ms_roomsList->RemoveItem(roomHandle);
		}
	}
#endif //__BANK
}

#if __BANK

void CLayoutManager::loadfileCB()
{
	g_LayoutManager.DeleteLayout();

	g_LayoutManager.UnloadLayout();
	g_LayoutManager.LoadLayout(layoutFileName);


	//Displayf("[ROOMLAYOUT] name: %s",miloInterior->m_Name.c_str());
	//PrintInterior(GetInteriorFromHandle(handle));
}

void CLayoutManager::GetInteriorName()
{
	CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if (pPlayer)
	{
		if (pPlayer->GetIsInInterior())
		{
#if __DEV
			strncpy(layoutFileName, pPlayer->GetPortalTracker()->GetInteriorName(), NAME_LENGTH);
#endif //__DEV
		}
	}
}

void CLayoutManager::populateLayoutCB()
{
	CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if (pPlayer)
	{
		if (pPlayer->GetIsInInterior())
		{
			g_LayoutManager.PopulateInteriorWithLayout(pPlayer->GetPortalTracker()->GetInteriorInst());
		}
	}
}

void CLayoutManager::depopulateLayoutCB()
{
	CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
	if (pPlayer)
	{
		if (pPlayer->GetIsInInterior())
		{
			g_LayoutManager.DeleteLayout();
		}
	}
}

void CLayoutManager::randomizeActiveGroupsCB()
{
	g_LayoutManager.RandomizeActiveGroups(g_LayoutManager.GetInteriorFromHandle(g_LayoutManager.m_currentFileHandle));

	// refresh display
	g_LayoutManager.SelectLayoutClickCB(g_LayoutManager.ms_currLayoutHandle);

	// depopulate and repopulate
	depopulateLayoutCB();
	populateLayoutCB();
}

void CLayoutManager::saveActiveGroupsCB()
{
	g_LayoutManager.SaveActiveStateToFile();
}

void CLayoutManager::restoreActiveGroupsCB()
{
	g_LayoutManager.LoadActiveStateFromFile();

	// refresh display
	g_LayoutManager.SelectLayoutClickCB(g_LayoutManager.ms_currLayoutHandle);

	// depopulate and repopulate
	depopulateLayoutCB();
	populateLayoutCB();
}

void CLayoutManager::SelectRoomClickCB(s32 hash)
{
	if ((u32)hash != ms_currRoomHandle)
	{
		SelectLayoutClickCB(0);			// if changing room, then reset layout selection
	}

	// remove old data
	if (ms_currRoomHandle != 0)
	{
		CMiloRoom* pRoom = GetRoomFromHandle(ms_currRoomHandle);

		for(int i=0;i<pRoom->m_LayoutNodeList.GetCount();i++)
		{
			CLayoutNode* pLayout = &pRoom->m_LayoutNodeList[i];
			u32 layoutHash = CreateHandle(pLayout);

			ms_layoutsList->RemoveItem(layoutHash);
		}
		ms_currRoomHandle = 0;
	}

	CMiloRoom* pRoom = GetRoomFromHandle(hash);

	if (pRoom)
	{
		ms_currRoomHandle = hash;

		for(int i=0;i<pRoom->m_LayoutNodeList.GetCount();i++)
		{
			CLayoutNode* pLayout = &pRoom->m_LayoutNodeList[i];
			u32 layoutHash = CreateHandle(pLayout);

			ms_layoutsList->AddItem(layoutHash, 0, i);
			ms_layoutsList->AddItem(layoutHash, 1, pLayout->m_Name.c_str());
		}
	}
}

void CLayoutManager::SelectLayoutClickCB(s32 hash)
{
	// remove old data
	if (ms_currLayoutHandle != 0)
	{
		CLayoutNode* pLayout = GetLayoutNode(ms_currLayoutHandle);

		for(int i=0;i<pLayout->m_GroupList.GetCount();i++)
		{
			CGroup* pGroup = pLayout->m_GroupList[i];
			u32 groupHash = CreateHandle(pGroup);

			ms_groupsList->RemoveItem(groupHash);
		}

		ms_currLayoutHandle = 0;
	}

	CLayoutNode* pLayout = GetLayoutNode(hash);

	if (pLayout)
	{
		ms_currLayoutHandle = hash;

		for(int i=0;i<pLayout->m_GroupList.GetCount();i++)
		{
			CGroup* pGroup = pLayout->m_GroupList[i];
			u32 groupHash = CreateHandle(pGroup);

			ms_groupsList->AddItem(groupHash, 0, i);
			ms_groupsList->AddItem(groupHash, 1, pGroup->m_Name);

			//if (pGroup->m_active) {
			if (IsActiveGroupId(pGroup->m_Id))
			{
				ms_groupsList->AddItem(groupHash, 2, "(*)");
			}

			// any subgroups?
			for(s32 j=0; j<pGroup->m_RefList.GetCount(); j++)
			{
				CRsRef* pRef = pGroup->m_RefList[j];
				if (pRef->m_LayoutNodeList.GetCount() > 0)
				{
					ms_groupsList->AddItem(groupHash, 3, "(+)");
				}
			}

			ms_groupsList->AddItem(groupHash, 4, pGroup->m_Id);
		}
	}
}

void CLayoutManager::ActivateGroupClickCB(s32 hash)
{
	bool bWasActive = false;

	CGroup* pCurrGroup = GetGroupNode(hash);
	if (pCurrGroup && IsActiveGroupId(pCurrGroup->m_Id)) //pCurrGroup->m_active)
	{
		bWasActive = true;
	}

	// remove active group
	if (ms_currLayoutHandle != 0)
	{
		CLayoutNode* pLayout = GetLayoutNode(ms_currLayoutHandle);
		for(int i=0;i<pLayout->m_GroupList.GetCount();i++)
		{
			CGroup* pClearGroup = pLayout->m_GroupList[i];
			if (pClearGroup)
			{
				//pClearGroup->m_active = false;
				SetActiveStateForGroup(pClearGroup->m_Id, false);
			}
		}
	}

	// toggle state of selected group
	if (pCurrGroup && !bWasActive)
	{
		//pCurrGroup->m_active = true;
		SetActiveStateForGroup(pCurrGroup->m_Id, true);
	}

	// refresh display
	SelectLayoutClickCB(ms_currLayoutHandle);

	// remove and recreate the layout entities
	depopulateLayoutCB();
	populateLayoutCB();
}

void CLayoutManager::NextObjCB()
{
	Displayf("Next");
}

void CLayoutManager::PrevObjCB()
{
	Displayf("Prev");
}

void CLayoutManager::SelectSubLayoutClickCB(s32 /*hash*/)
{
	Displayf("select sublayout");
}

void CLayoutManager::ActivateSubGroupClickCB(s32 /*hash*/)
{
	Displayf("select subgroup");
}

#define BUFSIZE (MAX_LAYOUT_ENTITIES + 2)


void CLayoutManager::SaveActiveStateToFile()
{
	u32 outBuffer[BUFSIZE];
	memset(outBuffer, 0, sizeof(outBuffer));

	u32 numEntries = m_activeIDs.GetCount();

	CMiloInterior* pInterior = GetInteriorFromHandle(m_currentFileHandle);
	outBuffer[0] = atStringHash(pInterior->m_File);
	outBuffer[1] = numEntries;
	for(u32 i=0; i<numEntries; i++)
	{
		outBuffer[2 + i] = m_activeIDs[i];
	}

	// create an output stream
	fiStream* stream(ASSET.Create("common:/non_final/activeLayoutGroups", "lay"));
	if (stream)
	{
		stream->WriteInt(outBuffer, BUFSIZE);

		stream->Close();

		Displayf("Activated layout group saved");
	}
}

void CLayoutManager::LoadActiveStateFromFile()
{
	CMiloInterior* pInterior = GetInteriorFromHandle(m_currentFileHandle);
	if (!pInterior)
	{
		return;
	}

	// create an output stream
	fiStream* stream(ASSET.Open("common:/non_final/activeLayoutGroups", "lay"));
	if (stream)
	{
		u32 inBuffer[BUFSIZE];
		stream->ReadInt(inBuffer, BUFSIZE);

		u32 bMatchingFileHash = (inBuffer[0] == atStringHash(pInterior->m_File));

		Assertf(bMatchingFileHash, "Layout data was not generated from %s", pInterior->m_File.c_str());
		if (bMatchingFileHash)
		{
			u32 numEntries = inBuffer[1];

			m_activeIDs.Reset();
			m_activeIDs.Reserve(MAX_LAYOUT_ENTITIES);

			for(u32 i=0; i<numEntries; i++)
			{
				m_activeIDs.Push(inBuffer[2+i]);
			}

			Displayf("Activated layout group loaded");
		}

		stream->Close();
	}
	
}

bool CLayoutManager::InitWidgets(void)
{
	bkBank *pBank = BANKMGR.FindBank("ExtraContent");
	Assert(pBank);

	ASSERT_ONLY(bkGroup *pBankGroup = )pBank->PushGroup("Interior Layout Manager", false);
	Assert(pBankGroup);
	
	pBank->AddText("Layout file", layoutFileName ,NAME_LENGTH, false, &CLayoutManager::loadfileCB);
	pBank->AddButton("Copy players current interior name", &CLayoutManager::GetInteriorName);
	pBank->AddButton("Load layout data", &CLayoutManager::loadfileCB );
	pBank->AddButton("Randomize active groups", &CLayoutManager::randomizeActiveGroupsCB);
	pBank->AddButton("Populate using current layout", &CLayoutManager::populateLayoutCB);
	pBank->AddButton("Depopulate using current layout", &CLayoutManager::depopulateLayoutCB);
	pBank->AddButton("Save active groups to file", &CLayoutManager::saveActiveGroupsCB);
	pBank->AddButton("Restore active groups from file", &CLayoutManager::restoreActiveGroupsCB);

	bkList::ClickItemFuncType selectRoomClickCB;
	selectRoomClickCB.Reset<CLayoutManager, &CLayoutManager::SelectRoomClickCB>(this);
	ms_roomsList = pBank->AddList("Rooms in current file:");
	ms_roomsList->SetSingleClickItemFunc(selectRoomClickCB);
	ms_roomsList->AddColumnHeader(0, "Room Idx", bkList::INT);
	ms_roomsList->AddColumnHeader(1, "Name", bkList::STRING);

	bkList::ClickItemFuncType selectLayoutClickCB;
	selectLayoutClickCB.Reset<CLayoutManager, &CLayoutManager::SelectLayoutClickCB>(this);
	ms_layoutsList = pBank->AddList("Layout in current room:");
	ms_layoutsList->SetSingleClickItemFunc(selectLayoutClickCB);
	ms_layoutsList->AddColumnHeader(0, "Layout Idx", bkList::INT);
	ms_layoutsList->AddColumnHeader(1, "Name", bkList::STRING);

	bkList::ClickItemFuncType activateGroupClickCB;
	activateGroupClickCB.Reset<CLayoutManager, &CLayoutManager::ActivateGroupClickCB>(this);
	ms_groupsList = pBank->AddList("Group in current layout:");
	ms_groupsList->SetSingleClickItemFunc(activateGroupClickCB);
	ms_groupsList->AddColumnHeader(0, "Group Idx", bkList::INT);
	ms_groupsList->AddColumnHeader(1, "Name", bkList::STRING);
	ms_groupsList->AddColumnHeader(2, "Active", bkList::STRING);
	ms_groupsList->AddColumnHeader(3, "Subgroups", bkList::STRING);
	ms_groupsList->AddColumnHeader(4, "Id", bkList::INT);

	pBank->AddButton("Next object", &CLayoutManager::NextObjCB );
	pBank->AddButton("Previous object", &CLayoutManager::PrevObjCB );

	pBank->PushGroup("Sub layouts", false);

	bkList::ClickItemFuncType selectSubLayoutClickCB;
	activateGroupClickCB.Reset<CLayoutManager, &CLayoutManager::SelectSubLayoutClickCB>(this);
	ms_subLayoutsList = pBank->AddList("Sub layout in current group:");
	ms_subLayoutsList->SetSingleClickItemFunc(selectSubLayoutClickCB);
	ms_subLayoutsList->AddColumnHeader(0, "Layout Idx", bkList::STRING);
	ms_subLayoutsList->AddColumnHeader(0, "Name", bkList::STRING);

	bkList::ClickItemFuncType activateSubGroupClickCB;
	activateGroupClickCB.Reset<CLayoutManager, &CLayoutManager::ActivateSubGroupClickCB>(this);
	ms_subGroupsList = pBank->AddList("Sub Group in current sub layout:");
	ms_subGroupsList->SetSingleClickItemFunc(activateSubGroupClickCB);
	ms_subGroupsList->AddColumnHeader(0, "Group Idx", bkList::STRING);
	ms_subGroupsList->AddColumnHeader(0, "Id", bkList::STRING);

	pBank->PopGroup(); // sub layouts
	pBank->PopGroup(); // Interior Layout Manager

	return(true);
}

#endif //__BANK
