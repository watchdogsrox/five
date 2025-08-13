/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/ipl/IplCullBoxEditor.cpp
// PURPOSE : debug tools to allow management and editing of map cull boxes
// AUTHOR :  Ian Kiigan
// CREATED : 05/04/2011
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/ipl/IplCullBoxEditor.h"

SCENE_OPTIMISATIONS();

#if __BANK

#include "camera/CamInterface.h"
#include "grcore/debugdraw.h"
#include "fwscene/world/WorldLimits.h"
#include "fwscene/stores/mapdatastore.h"
#include "scene/ipl/IplCullBox.h"

bool CIplCullBoxEditor::ms_bDisplayAllBoxes = false;
bool CIplCullBoxEditor::ms_bDisplayCulled = false;
bool CIplCullBoxEditor::ms_bDisplayIntersectingBoxes = false;
float CIplCullBoxEditor::ms_fSizeX = 1.0f;
float CIplCullBoxEditor::ms_fSizeY = 1.0f;
float CIplCullBoxEditor::ms_fSizeZ = 1.0f;
float CIplCullBoxEditor::ms_fPosX = 0.0f;
float CIplCullBoxEditor::ms_fPosY = 0.0f;
float CIplCullBoxEditor::ms_fPosZ = 0.0f;
CUiGadgetSimpleListAndWindow* CIplCullBoxEditor::ms_pBoxListWindow = NULL;
CUiGadgetSimpleListAndWindow* CIplCullBoxEditor::ms_pContainerListWindow = NULL;
char CIplCullBoxEditor::ms_achCurrentBoxName[BOX_NAME_MAX];
char CIplCullBoxEditor::ms_achSearchBoxName[BOX_NAME_MAX];
u32 CIplCullBoxEditor::ms_lastIndex = 0;


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		creates widgets for editing / displaying cull boxes
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::InitWidgets(bkBank& bank)
{
	CIplCullBox::ms_containerList.QSort(0, -1, CompareContainerNameCB);

	bank.PushGroup("IPL Cull Boxes");
	bank.AddButton("Register Containers", datCallback(CIplCullBox::RegisterContainers));
	bank.AddToggle("Draw boxes", &ms_bDisplayAllBoxes, datCallback(UiGadgetsCB));
	bank.AddToggle("Show currently culled", &ms_bDisplayCulled);
	bank.AddToggle("Show boxes at camera pos", &ms_bDisplayIntersectingBoxes);
	bank.AddToggle("Culling enabled", &(CIplCullBox::ms_bEnabled), datCallback(EnableCullingCB));
	bank.AddToggle("Test against camera pos instead of player", &(CIplCullBox::ms_bTestAgainstCameraPos));
	bank.AddToggle("Check for leaks", &(CIplCullBox::ms_bCheckForLeaks));
	bank.AddButton("Cull all", datCallback(CullAllCB));
	bank.AddButton("Clear all", datCallback(ClearAllCB));
	bank.AddButton("Create box at camera pos", datCallback(CreateBoxAtCameraPosCB));
	bank.AddButton("Delete current box", datCallback(DeleteCurrentBoxCB));
	bank.AddButton("Toggle box enabled", datCallback(ToggleBoxEnabledCB));
	bank.AddButton("Duplicate box", datCallback(DuplicateCB));
	bank.AddSeparator();

	bank.AddText("Find box by name", ms_achSearchBoxName, 256, datCallback(FindByNameCB) );
	bank.AddButton("Find next", datCallback(FindNextCB));
	bank.AddSeparator();

	bank.AddTitle("Tweak currently selected");
	bank.AddText("Box name", ms_achCurrentBoxName, 256, datCallback(SetNameCB));
	bank.AddSlider("Pos X", &ms_fPosX, WORLDLIMITS_XMIN, WORLDLIMITS_XMAX, 1.0f);
	bank.AddSlider("Pos Y", &ms_fPosY, WORLDLIMITS_YMIN, WORLDLIMITS_YMAX, 1.0f);
	bank.AddSlider("Pos Z", &ms_fPosZ, WORLDLIMITS_ZMIN, WORLDLIMITS_ZMAX, 1.0f);
	bank.AddSlider("Size X", &ms_fSizeX, 1.0f, WORLDLIMITS_XMAX, 1.0f);
	bank.AddSlider("Size Y", &ms_fSizeY, 1.0f, WORLDLIMITS_YMAX, 1.0f);
	bank.AddSlider("Size Z", &ms_fSizeZ, 1.0f, WORLDLIMITS_ZMAX, 1.0f);
	bank.AddSeparator();

	bank.AddButton("Create Cull List from Current Box", datCallback(CreateFromCurrentBox));
	bank.AddButton("Generate Cull List for Current Box", datCallback(GenerateCullList));
	bank.AddTitle("Save or load common:/data/mapdatacullboxes.meta");
	bank.AddButton("Save box list", datCallback(CIplCullBox::Save));
	bank.AddButton("Load box list", datCallback(Load));

	bank.PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateDebug
// PURPOSE:		handles debug draw etc for editing / display cull boxes
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::UpdateDebug()
{
	if (ms_bDisplayCulled)
	{
		DisplayCulledIpls();
	}
	if (ms_bDisplayIntersectingBoxes)
	{
		DisplayIntersectingBoxes();
	}
	if (ms_bDisplayAllBoxes)
	{
		DisplayBoxes();

		if (ms_pBoxListWindow->GetCurrentIndex()!=-1)
		{
			if (ms_pBoxListWindow->UserProcessClick())
			{
				RefreshTweakVals();
			}
			else
			{
				// use the tweaked floats
				CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
				entry.m_aabb.Set(
					Vec3V(ms_fPosX-ms_fSizeX/2.0f, ms_fPosY-ms_fSizeY/2.0f, ms_fPosZ-ms_fSizeZ/2.0f),
					Vec3V(ms_fPosX+ms_fSizeX/2.0f, ms_fPosY+ms_fSizeY/2.0f, ms_fPosZ+ms_fSizeZ/2.0f)
					);
			}

			//////////////////////////////////////////////////////////////////////////
			if (ms_pContainerListWindow->UserProcessClick() && ms_pContainerListWindow->GetCurrentIndex()!=-1)
			{
				CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
				entry.SetActive(false);
				u32 containerHash = CIplCullBox::ms_containerList[ms_pContainerListWindow->GetCurrentIndex()].m_name.GetHash();
				if (entry.IsCullingContainer(containerHash))
				{
					entry.RemoveContainer(containerHash);
				}
				else
				{
					entry.AddContainer(containerHash);
				}
				entry.GenerateCullList();
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayBoxes
// PURPOSE:		debug draw, display all cull boxes
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::DisplayBoxes()
{
	for (s32 i=0; i<CIplCullBox::ms_boxList.Size(); i++)
	{
		const CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(i);
		bool bCurrentSelection = (i==ms_pBoxListWindow->GetCurrentIndex());

		if (entry.m_bEnabled)
			grcDebugDraw::BoxAxisAligned(entry.m_aabb.GetMin(), entry.m_aabb.GetMax(), bCurrentSelection?Color32(0,255,255):Color32(255, 0, 0), false);
		else
			grcDebugDraw::BoxAxisAligned(entry.m_aabb.GetMin(), entry.m_aabb.GetMax(), bCurrentSelection?Color32(0,100,100):Color32(100, 0, 0), false);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayCulledIpls
// PURPOSE:		debug draw, display a list of all the currently culled IPLs
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::DisplayCulledIpls()
{
	for (u32 i=0; i<CIplCullBox::ms_boxList.Size(); i++)
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(i);
		if (entry.m_bActive && entry.m_bEnabled)
		{
			atArray<s32> iplSlotList;
			entry.m_cullList.GetSlotList(iplSlotList);

			for (u32 j=0; j<iplSlotList.size(); j++)
			{
				INSTANCE_DEF* pDef = INSTANCE_STORE.GetSlot(strLocalIndex(iplSlotList[j]));

				if (pDef)
				{
					grcDebugDraw::AddDebugOutput("Box %s currently culls IPL: %s", entry.m_name.GetCStr(), pDef->m_name.GetCStr());
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayIntersectingBoxes
// PURPOSE:		print debug text with names of boxes the camera intersects with
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::DisplayIntersectingBoxes()
{
	const Vector3& vPos = camInterface::GetPos();

	for (u32 i=0; i<CIplCullBox::ms_boxList.Size(); i++)
	{
		const CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(i);
		if (entry.m_aabb.ContainsPoint(RCC_VEC3V(vPos)))
		{
			grcDebugDraw::AddDebugOutput("Box name: %s", entry.m_name.GetCStr());
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UiGadgetsCB
// PURPOSE:		construct / destroy any debug ui elements required
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::UiGadgetsCB()
{
	if (ms_bDisplayAllBoxes)
	{
		CIplCullBox::RegisterContainers();
		if (!ms_pBoxListWindow)
		{
			ms_pBoxListWindow = rage_new CUiGadgetSimpleListAndWindow("IPL Cull Boxes", 700.0f, 60.0f, 170.0f, 40);
			ms_pBoxListWindow->SetNumEntries(CIplCullBox::ms_boxList.Size());
			ms_pBoxListWindow->SetUpdateCellCB(UpdateBoxCellCB);
		}
		if (!ms_pContainerListWindow)
		{
			ms_pContainerListWindow = rage_new CUiGadgetSimpleListAndWindow("Containers", 870.0f, 60.0f, 170.0f, 40);
			ms_pContainerListWindow->SetNumEntries(CIplCullBox::ms_containerList.size());
			ms_pContainerListWindow->SetUpdateCellCB(UpdateContainerCellCB);
			ms_pContainerListWindow->SetSelectorEnabled(false);
		}
	}
	else
	{
		if (ms_pBoxListWindow)
		{
			delete ms_pBoxListWindow;
			ms_pBoxListWindow = NULL;
		}
		if (ms_pContainerListWindow)
		{
			delete ms_pContainerListWindow;
			ms_pContainerListWindow = NULL;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateBoxCellCB
// PURPOSE:		callback to set cell contents for box list
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::UpdateBoxCellCB(CUiGadgetText* pResult, u32 row, u32 UNUSED_PARAM(col), void * UNUSED_PARAM(extraCallbackData) )
{
	if (row<CIplCullBox::ms_boxList.Size())
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(row);
		if (!entry.m_bEnabled)
		{
			pResult->SetString(entry.m_name.GetCStr());
		}
		else
		{
			char achTmp[256];
			sprintf(achTmp, "X %s", entry.m_name.GetCStr());
			pResult->SetString(achTmp);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateContainerCellCB
// PURPOSE:		callback to set cell contents for container list
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::UpdateContainerCellCB(CUiGadgetText* pResult, u32 row, u32 UNUSED_PARAM(col), void * UNUSED_PARAM(extraCallbackData) )
{
	if (row<CIplCullBox::ms_containerList.size())
	{
		if (ms_pBoxListWindow->GetCurrentIndex() != -1)
		{
			CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
			u32 containerHash = CIplCullBox::ms_containerList[row].m_name.GetHash();

			if (entry.IsCullingContainer(containerHash))
			{
				char achTmp[100];
				sprintf(achTmp, "X %s", CIplCullBox::ms_containerList[row].m_name.GetCStr());
				pResult->SetString(achTmp);
			}
			else
			{
				pResult->SetString(CIplCullBox::ms_containerList[row].m_name.GetCStr());
			}
		}
		else
		{
			pResult->SetString("");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CreateBoxAtCameraPosCB
// PURPOSE:		spawn a new cull box at the current camera position, which
//				the user can then adjust, add containers to be culled etc
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::CreateBoxAtCameraPosCB()
{
	if (ms_pBoxListWindow)
	{
		const Vector3& vPos = camInterface::GetPos();
		CIplCullBoxEntry newEntry;
		newEntry.m_aabb.Set
		(
			Vec3V(vPos.x-DEFAULT_BOX_SIZE, vPos.y-DEFAULT_BOX_SIZE, vPos.z-DEFAULT_BOX_SIZE),
			Vec3V(vPos.x+DEFAULT_BOX_SIZE, vPos.y+DEFAULT_BOX_SIZE, vPos.z+DEFAULT_BOX_SIZE)
		);
		newEntry.m_name.SetFromString("New Box");
		CIplCullBox::AddNewBox(newEntry);
		ms_pBoxListWindow->SetNumEntries(CIplCullBox::ms_boxList.Size());
		ms_pBoxListWindow->SetCurrentIndex(CIplCullBox::ms_boxList.Size()-1);
		RefreshTweakVals();

		ms_pBoxListWindow->RequestSetViewportToShowSelected();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DeleteCurrentBoxCB
// PURPOSE:		deletes the currently selected box
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::DeleteCurrentBoxCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex()!=-1)
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
		entry.SetActive(false);
		CIplCullBox::ms_boxList.Remove(ms_pBoxListWindow->GetCurrentIndex());
		ms_pBoxListWindow->SetNumEntries(CIplCullBox::ms_boxList.Size());
		RefreshTweakVals();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ToggleBoxEnabledCB
// PURPOSE:		debug option to toggle whether a cull box is enabled or not
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::ToggleBoxEnabledCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex()!=-1)
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
		entry.m_bEnabled = !entry.m_bEnabled;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DuplicateCB
// PURPOSE:		duplicate selected cull box
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::DuplicateCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex()!=-1)
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());

		CIplCullBoxEntry newEntry = entry;

		char achTmp[RAGE_MAX_PATH];
		sprintf(achTmp, "%s (Copy)", entry.m_name.GetCStr());
		newEntry.m_name.SetFromString(achTmp);

		CIplCullBox::AddNewBox(newEntry);
		ms_pBoxListWindow->SetNumEntries(CIplCullBox::ms_boxList.Size());
		ms_pBoxListWindow->SetCurrentIndex(CIplCullBox::ms_boxList.Size()-1);
		RefreshTweakVals();

		ms_pBoxListWindow->RequestSetViewportToShowSelected();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindByNameCB
// PURPOSE:		find a cull box by name
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::FindByNameCB()
{
	ms_lastIndex = 0;
	FindNextCB();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindNextCB
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::FindNextCB()
{
	if (!strlen(ms_achSearchBoxName))
		return;

	bool bFound = false;

	if (ms_lastIndex>=CIplCullBox::ms_boxList.Size())
	{
		ms_lastIndex = 0;
	}
	
	atString searchString = atString(ms_achSearchBoxName);
	searchString.Uppercase();

	for (u32 i=0; i<CIplCullBox::ms_boxList.Size(); i++)
	{
		const u32 testIndex = ( ms_lastIndex + i ) % CIplCullBox::ms_boxList.Size();

		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(testIndex);
		atString boxName = atString(entry.m_name.GetCStr());
		boxName.Uppercase();

		if ( boxName.IndexOf(searchString)!=-1 && testIndex!=(u32)ms_pBoxListWindow->GetCurrentIndex() )
		{
			bFound = true;
			ms_lastIndex = testIndex;
			break;
		}
	}

	if (bFound)
	{
		ms_pBoxListWindow->SetCurrentIndex(ms_lastIndex);
		RefreshTweakVals();
		ms_pBoxListWindow->RequestSetViewportToShowSelected();
	}
	else
	{
		// set nothing selected
		ms_lastIndex = 0;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetNameCB
// PURPOSE:		sets a new name for the current selected box entry
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::SetNameCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex()!=-1)
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
		entry.m_name.SetFromString(ms_achCurrentBoxName);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	RefreshTweakVals
// PURPOSE:		update the tweak rag controls to current selected box
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::RefreshTweakVals() 
{
	// if user has changed current selected box, initialise the tweakable floats
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex()!=-1)
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
		Vector3 vCentre = VEC3V_TO_VECTOR3(entry.m_aabb.GetCenter());
		sprintf(ms_achCurrentBoxName, "%s", entry.m_name.GetCStr());
		ms_fPosX = vCentre.x;
		ms_fPosY = vCentre.y;
		ms_fPosZ = vCentre.z;
		Assertf(entry.m_aabb.IsValid(), "CIplCullBoxEditor has invalid bbox");
		const Vector3 vSize = entry.m_aabb.GetMaxVector3() - entry.m_aabb.GetMinVector3();
		ms_fSizeX = vSize.x;
		ms_fSizeY = vSize.y;
		ms_fSizeZ = vSize.z;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Load
// PURPOSE:		loads the meta file containing box list data, and updates ui
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::Load()
{
	CIplCullBox::Load();
	if (ms_pBoxListWindow)
	{
		ms_pBoxListWindow->SetNumEntries(CIplCullBox::ms_boxList.Size());

		if (CIplCullBox::ms_boxList.Size() > 0)
		{
			RefreshTweakVals();
		}
	}
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CreateFromCurrentBox
// PURPOSE:		Creates a new cullbox by finding all map contents that don't intersect with the cullbox itself
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::CreateFromCurrentBox()
{
#if RSG_PC
	if (ms_pBoxListWindow->GetCurrentIndex() != -1)
	{
		CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
		entry.Reset();

		for(CIplCullBoxContainer const& container : CIplCullBox::ms_containerList)
		{
			s32 iplSlot = g_MapDataStore.FindSlotFromHashKey( container.m_name.GetHash() ).Get();
			fwMapDataDef* slot = INSTANCE_STORE.GetSlot(strLocalIndex(iplSlot));
			if(slot)
			{
				bool addToList = true;
				fwMapDataContents const* pContents = slot->GetContents();
				if(pContents)
				{
					fwMapData const* mapData = pContents->GetMapData_public();
					if(mapData)
					{
						spdAABB mapDataExtents;
						mapData->GetPhysicalExtents(mapDataExtents);

						if(entry.m_aabb.IntersectsAABB(mapDataExtents))
						{
							addToList = false;
						}
					}
				}
				if(addToList)
				{
					entry.AddContainer(container.m_name.GetHash());
				}
			}
		}

		INSTANCE_STORE.CreateChildrenCache();
		entry.GenerateCullList();
		INSTANCE_STORE.DestroyChildrenCache();
	}
#else
	Assertf(false, "PC functionality only!");
#endif
}


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GenerateCullList
// PURPOSE:		
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::GenerateCullList()
{
	CIplCullBoxEntry& entry = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
	entry.GenerateCullList();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CompareContainerNameCB
// PURPOSE:		callback used for sorting container names, easier to read etc
//////////////////////////////////////////////////////////////////////////
int CIplCullBoxEditor::CompareContainerNameCB(const CIplCullBoxContainer* pContainer1, const CIplCullBoxContainer* pContainer2)
{
	const char* pszName1 = pContainer1->m_name.GetCStr();
	const char* pszName2 = pContainer2->m_name.GetCStr();
	return stricmp(pszName1, pszName2);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	EnableCullingCB
// PURPOSE:		if the user disables update, ensure nothing is being culled
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::EnableCullingCB()
{
	if (!CIplCullBox::ms_bEnabled)
	{
		CIplCullBox::ResetCulled();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CullAllCB
// PURPOSE:		for selected cullbox, mark all containers as culled
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::CullAllCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex()!=-1)
	{
		CIplCullBoxEntry& box = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
		box.m_culledContainerHashes.Reset();
		for (s32 i=0; i<CIplCullBox::ms_containerList.GetCount(); i++)
		{
			u32 containerHash = CIplCullBox::ms_containerList[i].m_name.GetHash();
			box.AddContainer(containerHash);
		}
		box.GenerateCullList();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ClearAllCB
// PURPOSE:		for selected cullbox, mark all containers as not culled
//////////////////////////////////////////////////////////////////////////
void CIplCullBoxEditor::ClearAllCB()
{
	if (ms_pBoxListWindow && ms_pBoxListWindow->GetCurrentIndex()!=-1)
	{
		CIplCullBoxEntry& box = CIplCullBox::ms_boxList.Get(ms_pBoxListWindow->GetCurrentIndex());
		box.m_culledContainerHashes.Reset();
		box.GenerateCullList();
	}
}

#endif	//__BANK
