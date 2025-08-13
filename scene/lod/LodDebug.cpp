/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    scene/lod/LodDebug.cpp
// PURPOSE : useful debug widgets for lod hierarchy analysis
// AUTHOR :  Ian Kiigan
// CREATED : 04/03/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "scene/lod/LodDebug.h"

#if __BANK

#include "atl/string.h"
#include "Renderer/Lights/LightEntity.h"
#include "scene/AnimatedBuilding.h"
#include "Objects/object.h"
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "debug/DebugScene.h"
#include "debug/editing/LiveEditing.h"
#include "debug/GtaPicker.h"
#include "grcore/debugdraw.h"
#include "grcore/setup.h"
#include "input/mouse.h"
#include "renderer/sprite2d.h"
#include "Text/Text.h"
#include "Text/TextConversion.h"
#include "scene/Building.h"
#include "scene/Entity.h"
#include "fwscene/search/SearchVolumes.h"
#include "scene/world/GameWorld.h"
#include "scene/lod/LodMgr.h"
#include "scene/lod/LodScale.h"
#include "objects/DummyObject.h"
#include "fwscene/stores/fragmentstore.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/mapdatastore.h"
#include "system/controlMgr.h"
#include "input/mapper_defs.h"
#include "fwscene/stores/dwdstore.h"

#include "debug/UiGadget/UiGadgetBase.h"
#include "debug/UiGadget/UiGadgetBox.h"
#include "debug/UiGadget/UiGadgetText.h"

#include "fwmaths/Rect.h"
#include "parser/restparserservices.h"
#include "vector/color32.h"

#include "debug/Editing/MapEditRESTInterface.h"

SCENE_OPTIMISATIONS();

#define NODE_SIZE		(16.0f)
#define NODE_BORDER		(2.0f)
#define NODE_INNERSIZE	(NODE_SIZE - NODE_BORDER)
#define TREE_WIDTH		(1000.0f)
#define CLICK_DURATION	(30)

//////////////////////////////////////////////////////////////////////////
// LOD TREE info
bool CLodDebug::ms_bDisplayInfoOnSelected = false;
bool CLodDebug::ms_bDisplayTreeForSelected = false;
CEntity* CLodDebug::ms_pNextSelectedEntity = NULL;
s32 CLodDebug::ms_nClickCooloff = 0;
bool CLodDebug::ms_bInitialised = false;

//////////////////////////////////////////////////////////////////////////
// LOD distance tweaking
CEntity* CLodDistTweaker::pLastSelectedEntity = NULL;
bool CLodDistTweaker::ms_bEnableFadeDistTweak = false;
float CLodDistTweaker::ms_afTweakFadeDists[LODTYPES_DEPTH_TOTAL];
bool CLodDistTweaker::bTweakLodDist = false;
u32 CLodDistTweaker::nTweakedLodDist = 0;
u32 CLodDistTweaker::nTweakedChildLodDist = 0;
u32 CLodDistTweaker::nOverrideLodDistHD = 100;
u32 CLodDistTweaker::nOverrideLodDistOrphanHD = 100;
u32 CLodDistTweaker::nOverrideLodDistLOD = 500;
u32 CLodDistTweaker::nOverrideLodDistSLOD1 = 1000;
u32 CLodDistTweaker::nOverrideLodDistSLOD2 = 2000;
u32 CLodDistTweaker::nOverrideLodDistSLOD3 = 4500;
u32 CLodDistTweaker::nOverrideLodDistSLOD4 = 4500;
bool CLodDistTweaker::bWriteLodDistHD = false;
bool CLodDistTweaker::bWriteLodDistOrphanHD = false;
bool CLodDistTweaker::bWriteLodDistLOD = false;
bool CLodDistTweaker::bWriteLodDistSLOD1 = false;
bool CLodDistTweaker::bWriteLodDistSLOD2 = false;
bool CLodDistTweaker::bWriteLodDistSLOD3 = false;
bool CLodDistTweaker::bWriteLodDistSLOD4 = false;

//////////////////////////////////////////////////////////////////////////
// LOD scale tweaking
float CLodScaleTweaker::ms_fRootScale = 1.0f;
float CLodScaleTweaker::ms_fGlobalScale = 1.0f;
float CLodScaleTweaker::ms_fGlobalLodDistScaleOverrideDuration = 10.0f;
float CLodScaleTweaker::ms_afPerLodLevelScales[LODTYPES_DEPTH_TOTAL];
float CLodScaleTweaker::ms_fGrassScale = 1.0f;
bool CLodScaleTweaker::ms_bOverrideScales = false;
bool CLodScaleTweaker::ms_bDisplayScales = false;

#if RSG_PC
bool CLodScaleTweaker::ms_bEnableAimingScales = true;
#endif	//RSG_PC

//////////////////////////////////////////////////////////////////////////
// LOD misc debug
bool CLodDebugMisc::ms_bShowLevelsAllowingTimeBasedFade = false;
bool CLodDebugMisc::ms_bEnableTimeBasedFade = true;
bool CLodDebugMisc::ms_bGetInfoWhenLodForced = false;
bool CLodDebugMisc::ms_bForcedLodInfoAvailable = false;
bool CLodDebugMisc::ms_bFakeLodForce = false;
bool CLodDebugMisc::ms_bShowDetailedForcedLodInfo = false;
atArray<atString> CLodDebugMisc::ms_aForcedLodInfo;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AllowOrphanScalingCB
// PURPOSE:		Flush data store on allow orphan scaling widget change
//////////////////////////////////////////////////////////////////////////
#if RSG_PC
void CLodDebug::AllowOrphanScalingCB()
{
	g_MapDataStore.SafeRemoveAll(true);
}
#endif // RSG_PC


//////////////////////////////////////////////////////////////////////////
// FUNCTION:	InitWidgets
// PURPOSE:		add widgets to Rag
//////////////////////////////////////////////////////////////////////////
void CLodDebug::AddWidgets(bkBank* pBank)
{
	Assert(pBank);
	pBank->PushGroup("LOD Debug", false);
	{
		pBank->PushGroup("Tree Info", false);
		{
			pBank->AddToggle("Display info on selected", &ms_bDisplayInfoOnSelected);
			pBank->AddToggle("Display tree for selected", &ms_bDisplayTreeForSelected);
		}
		pBank->PopGroup();

#if RSG_PC
		pBank->AddToggle("Allow Orphan Scaling", fwMapDataContents::GetAllowOrphanScalingPtr(), AllowOrphanScalingCB);
#endif // RSG_PC

		CLodDistTweaker::AddWidgets(pBank);
		CLodScaleTweaker::AddWidgets(pBank);
		CLodDebugMisc::AddWidgets(pBank);
	}
	pBank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		updates search window with new search results etc
//////////////////////////////////////////////////////////////////////////
void CLodDebug::Update()
{
	// register REST interface if required
	if (!ms_bInitialised)
	{
		ms_bInitialised = true;
	}

	if (ms_bDisplayInfoOnSelected)
	{
		DisplayInfoOnSelected();
	}
	if (ms_bDisplayTreeForSelected)
	{
		DisplayTreeForSelected();
	}

	CLodDebugMisc::Update();
	CLodDistTweaker::Update();
	CLodScaleTweaker::Update();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetDistFromCamera
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
float CLodDebug::GetDistFromCamera(CEntity* pEntity)
{
	if (pEntity)
	{
		if (pEntity->GetIsTypeLight())
		{
			CLightEntity *lightEntity = (CLightEntity*)pEntity;

			Vector3 vPos;
			lightEntity->GetWorldPosition(vPos);
			float fDist = (vPos - g_LodMgr.GetCameraPosition()).Mag();
			return fDist;
		}

		Vector3 vPos;
		g_LodMgr.GetBasisPivot(pEntity, vPos);
		float fDist = (vPos - g_LodMgr.GetCameraPosition()).Mag();
		return fDist;
	}
	return 0.0f;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	FindChildren
// PURPOSE:		gets all loaded children for the specified parent
//////////////////////////////////////////////////////////////////////////
void CLodDebug::FindChildren(CEntity* pLod, atArray<CEntity*>* pResults)
{
	if (pLod)
	{
		CBuilding::Pool* pBuildingPool = CBuilding::GetPool();
		for (u32 i=0; i<pBuildingPool->GetSize(); i++)
		{
			CBuilding* pBuilding = pBuildingPool->GetSlot(i);
			if (pBuilding && pBuilding->GetLod()==pLod)
			{
				pResults->PushAndGrow(pBuilding);
			}
		}
		CDummyObject::Pool* pDummyPool = CDummyObject::GetPool();
		for (u32 i=0; i<pDummyPool->GetSize(); i++)
		{
			CDummyObject* pDummy = pDummyPool->GetSlot(i);
			if (pDummy && pDummy->GetLod()==pLod)
			{
				pResults->PushAndGrow(pDummy);
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetChildLodDist
// PURPOSE:		returns max lod dist of lod children of specified entity
//////////////////////////////////////////////////////////////////////////
u32 CLodDebug::GetChildLodDist(CEntity* pEntity)
{
	u32 retVal = 0;
	if (pEntity && pEntity->GetLodData().HasChildren())
	{
		retVal = pEntity->GetLodData().GetChildLodDistance();
	}
	return retVal;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	GetNumChildren
// PURPOSE:		returns number of lod children
//////////////////////////////////////////////////////////////////////////
u32 CLodDebug::GetNumChildren(CEntity* pEntity)
{
	u32 retVal = 0;
	if (pEntity && pEntity->GetLodData().HasChildren())
	{
		retVal = pEntity->GetLodData().GetNumChildren();
	}
	return retVal;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayInfoOnSelected
// PURPOSE:		prints some debug info about the selected entity
//////////////////////////////////////////////////////////////////////////
void CLodDebug::DisplayInfoOnSelected()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (pEntity)
	{
		CEntity* pLod = (CEntity*) pEntity->GetLod();
		if (pLod)
		{
			grcDebugDraw::AddDebugOutput( "PARENT: %s lodDist=%dm, numChildren=%d, alpha=%d",
				pLod->GetModelName(), pLod->GetLodDistance(), GetNumChildren(pLod) , pLod->GetAlpha() );
		}

		grcDebugDraw::AddDebugOutput( "SELECTED: %s lodDist=%dm, numChildren=%d, alpha=%d",
			pEntity->GetModelName(), pEntity->GetLodDistance(), GetNumChildren(pEntity) , pEntity->GetAlpha() );

		if (pEntity->GetLodData().HasChildren())
		{
			atArray<CEntity*> children;
			FindChildren(pEntity, &children);
			for (s32 i=0; i<children.GetCount(); i++)
			{
				CEntity* pChild = children[i];
				if (pChild)
				{
					grcDebugDraw::AddDebugOutput( "CHILD: %s lodDist=%dm, numChildren=%d, alpha=%d",
						pChild->GetModelName(), pChild->GetLodDistance(), GetNumChildren(pChild) , pChild->GetAlpha() );
				}
			}
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DisplayTreeForSelected
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodDebug::DisplayTreeForSelected()
{
	CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
	if (!pEntity)
		return;

	ms_pNextSelectedEntity = NULL;

	DrawInfo(pEntity, 640.0f, 360.0f);

	if (pEntity->GetLodData().HasLod())
	{
		CEntity* pLod = (CEntity*) pEntity->GetLod();
		if (pLod)
		{
			DrawLink(640.0f, 260.0f, 640.0f, 360.0f);
			DrawInfo(pLod, 640.0f, 260.0f);

			if (pLod->GetLodData().HasLod())
			{
				DrawLink(640.0f, 160.0f, 640.0f, 260.0f);
			}
		}
	}

	if (pEntity->GetLodData().HasChildren())
	{
		atArray<CEntity*> children;
		FindChildren(pEntity, &children);

		s32 numChildren = Max(children.GetCount(), (s32)GetNumChildren(pEntity));

		float fWidthPerNode = TREE_WIDTH / numChildren;
		float fStartX = 640.0f - TREE_WIDTH/2;

		if (fWidthPerNode >= NODE_SIZE*2)
		{
			for (s32 i=0; i<numChildren; i++)
			{
				float fChildX = fStartX + i * fWidthPerNode + fWidthPerNode/2;
				float fChildY = 460.0f;
				CEntity* pChild = (i<children.GetCount()) ? children[i] : NULL;
				DrawInfo(pChild, fChildX, fChildY);
				DrawLink(640.0f, 360.0f, fChildX, fChildY);

				if (pChild && pChild->GetLodData().HasChildren())
				{
					DrawLink(fChildX, fChildY, fChildX, 560.0f);
				}
			}
		}
	}

	if (ms_nClickCooloff)
	{
		ms_nClickCooloff--;
	}
	if (ms_pNextSelectedEntity && ms_nClickCooloff==0)
	{
		ms_nClickCooloff = CLICK_DURATION;
		g_PickerManager.AddEntityToPickerResults(ms_pNextSelectedEntity, true, true);
		g_PickerManager.SetCooloff(CLICK_DURATION);
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawInfo
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodDebug::DrawInfo(CEntity* pEntity, float fX, float fY)
{
	char achTmp[256];

	const Vector2 mousePosition = Vector2( static_cast<float>(ioMouse::GetX()), static_cast<float>(ioMouse::GetY()) );

	bool bSelectedEntity = pEntity && pEntity==g_PickerManager.GetSelectedEntity();

	// draw box
	const float fSize = NODE_SIZE;
	const float fInnerSize = NODE_INNERSIZE;

	Vector2 vMin( (fX-fSize) /(float)GRCDEVICE.GetWidth(), (fY-fSize) / (float)GRCDEVICE.GetHeight() );
	Vector2 vMax( (fX+fSize) /(float)GRCDEVICE.GetWidth(), (fY+fSize) / (float)GRCDEVICE.GetHeight() );
	grcDebugDraw::RectAxisAligned(vMin, vMax, bSelectedEntity ? Color_cyan : Color_white, pEntity!=NULL);
	
	if (pEntity)
	{
		grcDebugDraw::TextFontPush(grcSetup::GetMiniFixedWidthFont());

		// draw alpha
		sprintf(achTmp, "a%d", pEntity->GetAlpha());
		Vector2 alphaPos( (fX-fInnerSize) /(float)GRCDEVICE.GetWidth(), (fY-fInnerSize) / (float)GRCDEVICE.GetHeight() );
		grcDebugDraw::Text(alphaPos, Color_black, achTmp);

		// draw lod distance
		sprintf(achTmp, "d%d", pEntity->GetLodDistance());
		Vector2 lodDistPos( (fX-fInnerSize) /(float)GRCDEVICE.GetWidth(), (fY) / (float)GRCDEVICE.GetHeight() );
		grcDebugDraw::Text(lodDistPos, Color_black, achTmp);

		// if mouse is hovering, draw model name (and whether it is loaded)
		fwRect intersectRect(Vector2(fX, fY), fSize);
		if (intersectRect.IsInside(mousePosition))
		{
			CBaseModelInfo* pMI = pEntity->GetBaseModelInfo();
			if (pMI)
			{
				sprintf(achTmp, "%s: %s%s", pEntity->GetLodData().GetLodTypeName(), pEntity->GetModelName(),
					( pMI->GetHasLoaded() ? "" : " (unloaded)" ) );
				Vector2 namePos( (fX-fInnerSize) / (float)GRCDEVICE.GetWidth() , (fY+30.0f) / (float)GRCDEVICE.GetHeight() );
				grcDebugDraw::Text(namePos, Color_yellow, achTmp);

				if (!bSelectedEntity && ioMouse::GetPressedButtons()&ioMouse::MOUSE_LEFT)
				{
					ms_pNextSelectedEntity = pEntity;
				}
			}
		}

		grcDebugDraw::TextFontPop();
	}

}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawLink
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodDebug::DrawLink(float fParentX, float fParentY, float fChildX, float fChildY)
{
	Vector2 vParentPos( (fParentX) /(float)GRCDEVICE.GetWidth(), (fParentY+NODE_SIZE) / (float)GRCDEVICE.GetHeight() );
	Vector2 vChildPos( (fChildX) /(float)GRCDEVICE.GetWidth(), (fChildY-NODE_SIZE) / (float)GRCDEVICE.GetHeight() );
	grcDebugDraw::Line(vParentPos, vChildPos, Color32(0, 220, 0, 90), Color32(50, 50, 50, 90));
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddWidgets
// PURPOSE:		debug widgets for lod distance tweaking
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::AddWidgets(bkBank* pBank)
{
	pBank->PushGroup("LOD Distances", false);
	{
		pBank->AddTitle("Adjust individual entity");
		pBank->AddToggle("Enable LOD dist tweaking", &bTweakLodDist);
		pBank->AddSlider("Dist for selected entity", &nTweakedLodDist, 0, LODTYPES_MAX_LODDIST, 1);
		pBank->AddSlider("Dist for children of selected entity", &nTweakedChildLodDist, 0, LODTYPES_MAX_LODDIST, 1);
		pBank->AddSeparator();
		pBank->AddTitle("Override everything");
		pBank->AddToggle("Enable override HD", &bWriteLodDistHD);
		pBank->AddSlider("HD lod dist", &nOverrideLodDistHD, 0, 4500, 1);
		pBank->AddToggle("Enable override orphan HD", &bWriteLodDistOrphanHD);
		pBank->AddSlider("Orphan HD lod dist", &nOverrideLodDistOrphanHD, 0, 4500, 1);
		pBank->AddToggle("Enable override LOD", &bWriteLodDistLOD);
		pBank->AddSlider("LOD lod dist", &nOverrideLodDistLOD, 0, 4500, 1);
		pBank->AddToggle("Enable override SLOD1", &bWriteLodDistSLOD1);
		pBank->AddSlider("SLOD1 lod dist", &nOverrideLodDistSLOD1, 0, 4500, 1);
		pBank->AddToggle("Enable override SLOD2", &bWriteLodDistSLOD2);
		pBank->AddSlider("SLOD2 lod dist", &nOverrideLodDistSLOD2, 0, 4500, 1);
		pBank->AddToggle("Enable override SLOD3", &bWriteLodDistSLOD3);
		pBank->AddSlider("SLOD3 lod dist", &nOverrideLodDistSLOD3, 0, 4500, 1);	
		pBank->AddToggle("Enable override SLOD4", &bWriteLodDistSLOD4);
		pBank->AddSlider("SLOD4 lod dist", &nOverrideLodDistSLOD4, 0, 4500, 1);	
		
		pBank->AddButton("Write selected dists", WriteAllDistsNow);
		pBank->AddSeparator();

		for (s32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			ms_afTweakFadeDists[i] = LODTYPES_FADE_DISTF;
		}
		pBank->AddToggle("Tweak fade distances", &ms_bEnableFadeDistTweak, datCallback(EnableFadeDistTweakCB));
		pBank->AddSlider("Fade dist hd", &(ms_afTweakFadeDists[LODTYPES_DEPTH_HD]), 20, 200, 1);
		pBank->AddSlider("Fade dist lod", &(ms_afTweakFadeDists[LODTYPES_DEPTH_LOD]), 20, 200, 1);
		pBank->AddSlider("Fade dist slod1", &(ms_afTweakFadeDists[LODTYPES_DEPTH_SLOD1]), 20, 200, 1);
		pBank->AddSlider("Fade dist slod2", &(ms_afTweakFadeDists[LODTYPES_DEPTH_SLOD2]), 20, 200, 1);
		pBank->AddSlider("Fade dist slod3", &(ms_afTweakFadeDists[LODTYPES_DEPTH_SLOD3]), 20, 200, 1);
		pBank->AddSlider("Fade dist slod4", &(ms_afTweakFadeDists[LODTYPES_DEPTH_SLOD4]), 20, 200, 1);
		pBank->AddSlider("Fade dist orphan hd", &(ms_afTweakFadeDists[LODTYPES_DEPTH_ORPHANHD]), 20, 200, 1);
	}
	pBank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		update lod dist tweaking
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::Update()
{
	if (ms_bEnableFadeDistTweak)
	{
		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			g_LodMgr.OverrideFadeDistance(i, ms_afTweakFadeDists[i]);
		}
	}

	if (bTweakLodDist)
	{
		UpdateLodDistTweaking();
	}

	// entity deletes... this could be moved elsewhere later as it isn't really lod related
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_DELETE, KEYBOARD_MODE_DEBUG_SHIFT, "Delete map object"))
	{
		CEntity* pEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
		if (pEntity)
		{
			AddDeletedEntityToRestList(pEntity);
		}
	}

	// push the matrices of all selected entities
	g_MapEditRESTInterface.m_entityMatrixOverride.ResetCount();
	for (s32 i=0; i<g_PickerManager.GetNumberOfEntities(); i++)
	{
		CEntity* pEntity = (CEntity*) g_PickerManager.GetEntity(i);
		if (pEntity)
		{
			AddSelectedEntityToMatrixList(pEntity);
		}
	}

	g_MapEditRESTInterface.Update();	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateLodDistTweaking
// PURPOSE:		update adjustment of selected entity lod distance
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::UpdateLodDistTweaking()
{
	if (g_PickerManager.GetSelectedEntity() != pLastSelectedEntity)
	{
		pLastSelectedEntity = (CEntity*) g_PickerManager.GetSelectedEntity();
		if (pLastSelectedEntity)
		{
			nTweakedLodDist = pLastSelectedEntity->GetLodDistance();
			nTweakedChildLodDist = CLodDebug::GetChildLodDist(pLastSelectedEntity);
		}
	}
	else if (pLastSelectedEntity)
	{
		if (pLastSelectedEntity->GetLodDistance() != nTweakedLodDist)
		{
			ChangeEntityLodDist(pLastSelectedEntity, nTweakedLodDist);
			UpdateRestOverrideList(pLastSelectedEntity, nTweakedLodDist);
		}
		if (CLodDebug::GetChildLodDist(pLastSelectedEntity) != nTweakedChildLodDist)
		{
			ChangeAllChildrenLodDists(pLastSelectedEntity, nTweakedChildLodDist);
			UpdateRestChildOverrideList(pLastSelectedEntity, nTweakedChildLodDist);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddDeletedEntityToRestList
// PURPOSE:		adds an entity to be deleted to the correct REST interface array
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::AddDeletedEntityToRestList(CEntity* pEntity)
{
	if (pEntity && IsEntityFromMapData(pEntity))
	{
		g_MapEditRESTInterface.InitIfRequired();

		atArray<CLodDebugEntityDelete>& list = g_MapEditRESTInterface.m_mapEntityDeleteList;
		CLodDebugEntityDelete newEntry(pEntity);

		for (s32 i=0; i<list.GetCount(); i++)
		{
			CLodDebugEntityDelete& listEntry = list[i];
			if (listEntry == newEntry)
			{
				return;
			}
		}
		list.Grow() = newEntry;

		// have live edit system delete the entity
		g_mapLiveEditing.DeleteMapEntity( newEntry.m_guid, newEntry.m_imapName.GetCStr() );
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddSelectedEntityToMatrixList
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::AddSelectedEntityToMatrixList(CEntity* pEntity)
{
	if (pEntity && IsEntityFromMapData(pEntity))
	{
		g_MapEditRESTInterface.InitIfRequired();

		atArray<CEntityMatrixOverride>& list = g_MapEditRESTInterface.m_entityMatrixOverride;
		CEntityMatrixOverride newEntry(pEntity);

		for (s32 i=0; i<list.GetCount(); i++)
		{
			CEntityMatrixOverride& listEntry = list[i];
			if (listEntry == newEntry)
			{
				return;
			}
		}

		list.Grow() = newEntry;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateRestOverrideList
// PURPOSE:		adds or updates entry in REST lod dist override list
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::UpdateRestOverrideList(CEntity* pEntity, u32 dist)
{
	if (!pEntity)
		return;

	if (!IsEntityFromMapData(pEntity))
		return;

	atArray<CLodDebugLodOverride>& list = g_MapEditRESTInterface.m_distanceOverride;
	CLodDebugLodOverride newEntry(pEntity, (float) dist);

	for (s32 i=0; i<list.GetCount(); i++)
	{
		CLodDebugLodOverride& listEntry = list[i];
		if (listEntry == newEntry)
		{
			listEntry.m_distance = (float) dist;
			return;
		}
	}
	list.Grow() = newEntry;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateRestChildOverrideList
// PURPOSE:		adds or updates entry in REST child lod dist override list
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::UpdateRestChildOverrideList(CEntity* pEntity, u32 childDist)
{
	if (!pEntity)
		return;

	if (!IsEntityFromMapData(pEntity))
		return;

	atArray<CLodDebugLodOverride>& list = g_MapEditRESTInterface.m_childDistanceOverride;
	CLodDebugLodOverride newEntry(pEntity, (float) childDist);

	for (s32 i=0; i<list.GetCount(); i++)
	{
		CLodDebugLodOverride& listEntry = list[i];
		if (listEntry == newEntry)
		{
			listEntry.m_distance = (float) childDist;
			return;
		}
	}
	list.Grow() = newEntry;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsEntityFromMapData
// PURPOSE:		returns true if the specified entity was created from map data
//////////////////////////////////////////////////////////////////////////
bool CLodDistTweaker::IsEntityFromMapData(CEntity* pEntity)
{
	if (pEntity)
	{
		if (pEntity->GetIplIndex()>0)
		{
			return true;
		}
		else if (pEntity->GetIsTypeObject())
		{
			CDummyObject* pDummy = ((CObject*) pEntity)->GetRelatedDummy();
			if (pDummy && pDummy->GetIplIndex()>0)
			{
				return true;
			}
		}
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WriteAllDistsNow
// PURPOSE:		update selectd pools in world with overridden LOD distances
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::WriteAllDistsNow()
{
	CBuilding::GetPool()->ForAll(WriteAllDistsForPool<CBuilding>, NULL);
	CDummyObject::GetPool()->ForAll(WriteAllDistsForPool<CDummyObject>, NULL);
	CObject::GetPool()->ForAll(WriteAllDistsForPool<CObject>, NULL);
	CAnimatedBuilding::GetPool()->ForAll(WriteAllDistsForPool<CAnimatedBuilding>, NULL);
	CLightEntity::GetPool()->ForAll(WriteAllDistsForPool<CLightEntity>, NULL);
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	WriteAllDistsForPool
// PURPOSE:		update entity in world with overridden LOD distances
//////////////////////////////////////////////////////////////////////////
template<typename poolEntityType>
bool CLodDistTweaker::WriteAllDistsForPool(void* pItem, void* UNUSED_PARAM(data))
{
	poolEntityType* pEntity = static_cast<poolEntityType*>(pItem);

	if (pEntity && pEntity->GetArchetype())
	{
		switch (pEntity->GetLodData().GetLodType())
		{
		case LODTYPES_DEPTH_ORPHANHD:
			if (bWriteLodDistOrphanHD)
			{
				ChangeEntityLodDist(pEntity, nOverrideLodDistOrphanHD);
			}
			break;

		case LODTYPES_DEPTH_HD:
			if (bWriteLodDistHD)
			{
				ChangeEntityLodDist(pEntity, nOverrideLodDistHD);
			}
			break;

		case LODTYPES_DEPTH_LOD:
			if (bWriteLodDistLOD)
			{
				ChangeEntityLodDist(pEntity, nOverrideLodDistLOD);
			}
			break;

		case LODTYPES_DEPTH_SLOD1:
			if (bWriteLodDistSLOD1)
			{
				ChangeEntityLodDist(pEntity, nOverrideLodDistSLOD1);
			}
			break;

		case LODTYPES_DEPTH_SLOD2:
			if (bWriteLodDistSLOD2)
			{
				ChangeEntityLodDist(pEntity, nOverrideLodDistSLOD2);
			}
			break;

		case LODTYPES_DEPTH_SLOD3:
			if (bWriteLodDistSLOD3)
			{
				ChangeEntityLodDist(pEntity, nOverrideLodDistSLOD3);
			}
			break;

		case LODTYPES_DEPTH_SLOD4:
			if (bWriteLodDistSLOD4)
			{
				ChangeEntityLodDist(pEntity, nOverrideLodDistSLOD4);
			}
			break;

		default:
			break;
		}
	}

	return true;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	EnableFadeDistTweakCB
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::EnableFadeDistTweakCB()
{
	if (ms_bEnableFadeDistTweak)
	{
		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			ms_afTweakFadeDists[i] = g_LodMgr.GetFadeDistance(i);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ChangeAllChildrenLodDists
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::ChangeAllChildrenLodDists(CEntity* pEntity, u32 dist)
{
	if (pEntity && pEntity->GetLodData().HasChildren())
	{
		atArray<CEntity*> children;

		CLodDebug::FindChildren(pEntity, &children);

		for (s32 i=0; i<children.GetCount(); i++)
		{
			ChangeEntityLodDist(children[i], dist);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ChangeEntityLodDist
// PURPOSE:		overrides the specified entities LOD distance
//////////////////////////////////////////////////////////////////////////
void CLodDistTweaker::ChangeEntityLodDist(CEntity* pEntity, u32 dist)
{
	Assert(pEntity);

	// write back tweaked lod distance
	pEntity->SetLodDistance((u16) dist);

	// update cached lod distance in descriptor
	fwBaseEntityContainer* pContainer = pEntity->GetOwnerEntityContainer();
	if (pContainer)
	{
		u32 index = pContainer->GetEntityDescIndex(pEntity);
		if (index != fwBaseEntityContainer::INVALID_ENTITY_INDEX)
		{
			pContainer->SetLodDistCached(index, dist);
		}
	}

	// if has lod parent, generate max child lod dist etc
	if (!pEntity->GetLodData().IsOrphanHd() )
	{
		CEntity* pLod = (CEntity*) pEntity->GetLod();
		if (pLod)
		{
			pLod->GetLodData().SetChildLodDistance(dist);
			pLod->GetLodData().SetKnowsChildLodDist(true);
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddWidgets
// PURPOSE:		lod scale widgets
//////////////////////////////////////////////////////////////////////////
void CLodScaleTweaker::AddWidgets(bkBank* pBank)
{
	pBank->PushGroup("LOD Scales", false);
	{
		pBank->AddToggle("Display scales", &ms_bDisplayScales);
		pBank->AddToggle("Override scales", &ms_bOverrideScales);

#if RSG_PC
		pBank->AddToggle("Overridden scale when aiming", &ms_bEnableAimingScales);
#endif	//RSG_PC

		pBank->AddSlider("Root Scale", &ms_fRootScale, 0.1f, 10.0f, 0.01f);
		pBank->AddSlider("Global LOD scale", &ms_fGlobalScale, 0.1f, 10.0f, 0.01f);
		pBank->AddSlider("Global LOD scale blend out duration ", &ms_fGlobalLodDistScaleOverrideDuration, 0.1f, 20.0f, 0.1f);
		pBank->AddSeparator("");

		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			pBank->AddSlider(fwLodData::ms_apszLevelNames[i], &ms_afPerLodLevelScales[i], 0.1f, 10.0f, 0.01f);
		}

		pBank->AddSlider("Grass batch", &ms_fGrassScale, 0.1f, 10.0f, 0.01f);
	}
	pBank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		display all lod scales onscreen
//////////////////////////////////////////////////////////////////////////
void CLodScaleTweaker::Update()
{
	if (ms_bDisplayScales)
	{
		grcDebugDraw::AddDebugOutput("Root scale = %4.2f", g_LodScale.GetRootScale());
		grcDebugDraw::AddDebugOutput("Global LOD scale = %4.2f", g_LodScale.GetGlobalScale());
		grcDebugDraw::AddDebugOutput("Grass batch scale = %4.2f", g_LodScale.GetGrassScale_In());
		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			grcDebugDraw::AddDebugOutput("%s scale = %4.2f", fwLodData::ms_apszLevelNames[i], g_LodScale.GetPerLodLevelScale_In(i));
		}

		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			grcDebugDraw::AddDebugOutput("Final %s scale = %4.2f", fwLodData::ms_apszLevelNames[i], g_LodScale.GetPerLodLevelScale(i));
		}
		grcDebugDraw::AddDebugOutput("Final grass batch scale = %4.2f", g_LodScale.GetGrassBatchScale());
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AdjustScales
// PURPOSE:		allow user tweaking of all lod scales
//////////////////////////////////////////////////////////////////////////
void CLodScaleTweaker::AdjustScales()
{
	if (ms_bOverrideScales)
	{
		g_LodScale.SetRootScale(ms_fRootScale);
		g_LodScale.SetGlobalScaleFromScript(ms_fGlobalScale);
		g_LodScale.SetGrassScale_In(ms_fGrassScale);

		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			g_LodScale.SetPerLodLevelScale_In(i, ms_afPerLodLevelScales[i]);
		}
	}
	else
	{
		if (ms_fRootScale != g_LodScale.GetRootScale() )
		{
			ms_fRootScale = g_LodScale.GetRootScale();
		}

		if (ms_fGlobalScale != g_LodScale.GetGlobalScale())
		{
			ms_fGlobalScale = g_LodScale.GetGlobalScale();
		}
		
		for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
		{
			if (ms_afPerLodLevelScales[i] != g_LodScale.GetPerLodLevelScale_In(i))
			{
				ms_afPerLodLevelScales[i] = g_LodScale.GetPerLodLevelScale_In(i);
			}
		}
		
		if (ms_fGrassScale != g_LodScale.GetGrassScale_In())
		{
			ms_fGrassScale = g_LodScale.GetGrassScale_In();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddWidgets
// PURPOSE:		misc lod debug widgets
//////////////////////////////////////////////////////////////////////////
void CLodDebugMisc::AddWidgets(bkBank* pBank)
{
	pBank->PushGroup("LOD Misc", false);
	{
		pBank->AddToggle("Show levels allowed time-based fade", &ms_bShowLevelsAllowingTimeBasedFade);
		pBank->AddToggle("Get info when alpha is forced up", &ms_bGetInfoWhenLodForced, datCallback(ForcedLodInfoEnabledCB));
		pBank->AddToggle("Show detailed info", &ms_bShowDetailedForcedLodInfo);
		pBank->AddToggle("Fake forced lod", &ms_bFakeLodForce);
		pBank->AddToggle("Enable time-based fade", &ms_bEnableTimeBasedFade);
		extern bool s_bShowLODAndCrossfades;
		extern float g_crossFadeDistance[2];
		extern int s_ForceLODOneOnShadow;
		extern int s_ForceLODCrossFade;
		extern int s_ForceLODCrossFadeHighLod;

		pBank->AddToggle("Show LOD & cross fade values", &s_bShowLODAndCrossfades);
		pBank->AddSlider("Crossfade distance 1", &g_crossFadeDistance[0], 0.01f, 20.f, 1.f);	
		pBank->AddSlider("Last Lod Crossfade distance 2", &g_crossFadeDistance[1], 0.01f, 20.f, 1.f);	

		pBank->AddSlider("Force fixed LOD On Shadows",&s_ForceLODOneOnShadow,-1,3,1);
		pBank->AddSlider("Force Cross fade base LOD",&s_ForceLODCrossFadeHighLod,-1,3,1);
		pBank->AddSlider("Force Cross fade ",&s_ForceLODCrossFade,-1,255,1);
	}
	pBank->PopGroup();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	Update
// PURPOSE:		update misc lod debug widgets
//////////////////////////////////////////////////////////////////////////
void CLodDebugMisc::Update()
{
	if (ms_bGetInfoWhenLodForced && ms_bForcedLodInfoAvailable)
	{
		for (s32 i=0; i<ms_aForcedLodInfo.GetCount(); i++)
		{
			if (i<2)	//draw the forced lod and reason info in red, the children details in default colour
			{
				grcDebugDraw::AddDebugOutputEx(false, Color32(255,0,0), ms_aForcedLodInfo[i].c_str());
			}
			else
			{
				grcDebugDraw::AddDebugOutputEx(false, ms_aForcedLodInfo[i].c_str());
			}
		}
	}

	if ( ms_bShowLevelsAllowingTimeBasedFade )
	{
		if ( g_LodMgr.AllowTimeBasedFadingThisFrame() )
		{
			for (u32 i=0; i<LODTYPES_DEPTH_TOTAL; i++)
			{
				if ( g_LodMgr.LodLevelMayTimeBasedFade(i) )
				{
					grcDebugDraw::AddDebugOutput("Allow fade for %s", fwLodData::ms_apszLevelNames[i]);
				}
			}
		}
		else
		{
			grcDebugDraw::AddDebugOutput("Time base fading disabled!");
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SubmitForcedLodInfo
// PURPOSE:
//////////////////////////////////////////////////////////////////////////
void CLodDebugMisc::SubmitForcedLodInfo(CEntity* pLod, CEntity* pUnloadedChild, CGtaPvsEntry* pStart, CGtaPvsEntry* pStop)
{
	if (pLod && CModelInfo::IsValidModelInfo(pLod->GetModelIndex()))
	{
		ms_aForcedLodInfo.Reset();
		char achTmp[256];

		const char* apszLodTypes[] = { "lodded hd", "lod", "slod1", "slod2", "slod3", "orphan hd" };

		// parent entity
		sprintf(achTmp, "%s alpha forced to 255 (Visible:%s LdDst:%d, NmChldrn:%d, LodLvl:%s)",
			pLod->GetModelName(),
			( IsEntityInPvsAndVisibleInGbuf(pLod, pStart, pStop) ? "Y" : "N" ),
			pLod->GetLodDistance(),
			pLod->GetLodData().GetNumChildren(),
			apszLodTypes[pLod->GetLodData().GetLodType()]
		);
		ms_aForcedLodInfo.Grow() = atString(achTmp);

		// reason
		sprintf(achTmp, "because %s is visible but not loaded",
			(pUnloadedChild ? pUnloadedChild->GetModelName() : "UNKNOWN")
			);
		ms_aForcedLodInfo.Grow() = atString(achTmp);

		// children entity
		atArray<CEntity*> aChildren;
		CLodDebug::FindChildren(pLod, &aChildren);

		for (s32 i=0; i<aChildren.GetCount(); i++)
		{
			CEntity* pEntity = aChildren[i];
			if (pEntity && CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
			{
				CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();

				// general entity info
				sprintf(achTmp, "%sChild %d %s (Visible:%s LdDst:%d, NmChldrn:%d, LodLvl:%s, Alpha:%d, Loaded:%s, HasDrwbl:%s, Missing:%s)",
					( pEntity==pUnloadedChild ? "*" : ""),
					(i+1),
					pEntity->GetModelName(),
					( IsEntityInPvsAndVisibleInGbuf(pEntity, pStart, pStop) ? "Y" : "N" ),
					pEntity->GetLodDistance(),
					pEntity->GetLodData().GetNumChildren(),
					apszLodTypes[pEntity->GetLodData().GetLodType()],
					pEntity->GetAlpha(),
					( pModelInfo->GetHasLoaded() ? "Y" : "N" ),
					( pEntity->GetDrawHandlerPtr() ? "Y" : "N" ),
					( pModelInfo->IsModelMissing() ? "Y" : "N" )
					);
				ms_aForcedLodInfo.Grow() = atString(achTmp);	
			}
		}

		// asset details etc
		if (ms_bShowDetailedForcedLodInfo)
		{
			for (s32 i=0; i<aChildren.GetCount(); i++)
			{
				CEntity* pEntity = aChildren[i];
				if (pEntity && CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
				{
					// drawable info
					CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
					switch (pModelInfo->GetDrawableType())
					{
					case fwArchetype::DT_UNINITIALIZED:
						sprintf(achTmp, "%sDrawable %d DT_UNINITIALIZED",
							( pEntity==pUnloadedChild ? "*" : "" ),
							(i+1)
							);
						break;
					case fwArchetype::DT_FRAGMENT:
						{
							strLocalIndex fragIndex = strLocalIndex(pModelInfo->GetFragmentIndex());
							fwFragmentDef* pDef = g_FragmentStore.GetSlot(fragIndex);
							Fragment* pFrag = g_FragmentStore.Get(fragIndex);
							sprintf(achTmp, "%sDrawable %d DT_FRAGMENT %d (Name:%s, Loaded:%s)",
								( pEntity==pUnloadedChild ? "*" : "" ),
								(i+1),
								fragIndex.Get(), pDef->m_name.GetCStr(),
								( pFrag ? "Y" : "N" )
								);
						}
						break;
					case fwArchetype::DT_DRAWABLE:
						{
							strLocalIndex drawableIndex = strLocalIndex(pModelInfo->GetDrawableIndex());
							fwDrawableDef* pDef = g_DrawableStore.GetSlot(drawableIndex);
							Drawable* pDrawable = g_DrawableStore.Get(drawableIndex);
							sprintf(achTmp, "%sDrawable %d DT_DRAWABLE %d (Name:%s, Loaded:%s)",
								( pEntity==pUnloadedChild ? "*" : "" ),
								(i+1),
								drawableIndex.Get(),
								pDef->m_name.GetCStr(),
								( pDrawable ? "Y" : "N" )
								);
						}
						break;
					case fwArchetype::DT_DRAWABLEDICTIONARY:
						{
							strLocalIndex dictIndex = strLocalIndex(pModelInfo->GetDrawDictIndex());
							DwdDef* pDwdDef = g_DwdStore.GetSlot(dictIndex);
							Dwd* pDwd = g_DwdStore.Get(dictIndex);
							bool bValid = pDwd && (pModelInfo->GetDrawDictDrawableIndex()<pDwd->GetCount());
							sprintf(achTmp, "%sDrawable %d DT_DRAWABLEDICTIONARY %d (Name:%s, Loaded:%s), DrwblIdx:%d (Valid:%s)",
								( pEntity==pUnloadedChild ? "*" : "" ),
								(i+1),
								dictIndex.Get(),
								pDwdDef->m_name.GetCStr(),
								( pDwd ? "Y" : "N" ),
								pModelInfo->GetDrawDictDrawableIndex(),
								( bValid ? "Y" : "N" )
								);
						}
						break;
					default:
						break;
					}
					ms_aForcedLodInfo.Grow() = atString(achTmp);
				}
			}

			for (s32 i=0; i<aChildren.GetCount(); i++)
			{
				CEntity* pEntity = aChildren[i];
				if (pEntity && CModelInfo::IsValidModelInfo(pEntity->GetModelIndex()))
				{
					// txd info
					CBaseModelInfo* pModelInfo = pEntity->GetBaseModelInfo();
					strLocalIndex txdIndex = strLocalIndex(pModelInfo->GetAssetParentTxdIndex());

					if (txdIndex != -1)
					{
						sprintf(achTmp, "%sTexDict %d TXD %d (Name:%s, Loaded:%s",
							( pEntity==pUnloadedChild ? "*" : "" ),
							(i+1),
							txdIndex.Get(),
							g_TxdStore.GetSlot(txdIndex)->m_name.GetCStr(),
							( g_TxdStore.GetPtr(txdIndex) ? "Y" : "N" )
							);
					}
					else
					{
						sprintf(achTmp, "TexDict n/a");
					}
					ms_aForcedLodInfo.Grow() = atString(achTmp);
				}
			}
		}
		ms_bForcedLodInfoAvailable = true;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	IsEntityInPvsAndVisibleInGbuf
// PURPOSE:		returns true if specified entity is in pvs and visible in gbuffer phase
//////////////////////////////////////////////////////////////////////////
bool CLodDebugMisc::IsEntityInPvsAndVisibleInGbuf(CEntity* pTestEntity, CGtaPvsEntry* pStart, CGtaPvsEntry* pStop)
{
	u32	gbufPhaseFlag;

	gbufPhaseFlag = 1 << g_SceneToGBufferPhaseNew->GetEntityListIndex();

	CGtaPvsEntry* pPvsEntry = pStart;
	while (pPvsEntry != pStop)
	{
		CEntity* pEntity = pPvsEntry->GetEntity();
		fwVisibilityFlags& visFlags = pPvsEntry->GetVisFlags();

		if (pEntity && pEntity==pTestEntity && (visFlags&gbufPhaseFlag)!=0)
		{
			return true;
		}
		pPvsEntry++;
	}
	return false;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ForcedLodInfoEnabledCB
// PURPOSE:		callback for toggle to enable / disable info on forced lods
//////////////////////////////////////////////////////////////////////////
void CLodDebugMisc::ForcedLodInfoEnabledCB()
{
	if (!ms_bGetInfoWhenLodForced)
	{
		ms_bForcedLodInfoAvailable = false;
	}
}

#endif	//__BANK
