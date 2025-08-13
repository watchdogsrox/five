//
// scene/world/WorldDebugHelper.cpp
//
// Copyright (C) 1999-2009 Rockstar Games. All Rights Reserved.
//

#include "scene/world/WorldDebugHelper.h"

#if __BANK

// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "input/mouse.h"
#include "grprofile/ekg.h"
#include "grprofile/profiler.h"

// Framework headers
#include "fwscene/world/WorldMgr.h"
#include "fwscene/world/WorldLimits.h"
#include "fwscene/world/WorldRepMulti.h"
#include "fwscene/world/SceneGraphNode.h"
#include "fwscene/scan/Scan.h"

// Game headers
#include "camera/CamInterface.h"
#include "debug/debugscene.h"
#include "Text/Text.h"
#include "Text/TextConversion.h"
#include "scene/lod/LodMgr.h"
#include "scene/world/GameWorld.h"
#include "scene/world/GameWorldHeightMap.h"
#include "scene/world/GameWorldWaterHeight.h"
#include "peds/Ped.h"
#include "Renderer/Lights/LightEntity.h"
#include "renderer/sprite2d.h"

#define MAX_NUM_LIST_ENTRIES		30
#define MAX_LIST_TEXT_LENGTH		32

#define MAX_NUM_LOD_ENTRIES			20
#define MAX_LOD_TEXT_LENGTH			64

#define MAX_NUM_DETAIL_ENTRIES		15
#define MAX_DETAIL_TEXT_LENGTH		64

struct EntityListGuiInfo
{
	Vector2		mousePosition;

	u32			numListEntries;
	char		listText[ MAX_NUM_LIST_ENTRIES ][ MAX_LIST_TEXT_LENGTH ];
	Color32		listColor[ MAX_NUM_LIST_ENTRIES ];
	CEntity*	listPointers[ MAX_NUM_LIST_ENTRIES ];

	u32			numLodEntries;
	char		lodText[ MAX_NUM_LOD_ENTRIES ][ MAX_LOD_TEXT_LENGTH ];
	Color32		lodColor[ MAX_NUM_LOD_ENTRIES ];
	CEntity*	lodPointers[ MAX_NUM_LOD_ENTRIES ];

	u32			numDetailEntries;
	char		detailText[ MAX_NUM_DETAIL_ENTRIES ][ MAX_DETAIL_TEXT_LENGTH ];
	Color32		detailColor[ MAX_NUM_DETAIL_ENTRIES ];

	bool		boundingShapesAreValid;
	Vector3		entityBoundingSphereCenter;
	float		entityBoundingSphereRadius;
	spdAABB	entityBoundingBox;

	bool		boundingBoxInQuadRepIsValid;
	fwRect		entityBoundingBoxInQuadRep;

	EntityListGuiInfo() {
		memset( this, 0, sizeof(EntityListGuiInfo) );
	}
};

static EntityListGuiInfo			guiInfoBuffers[2];		// One for the Update thread, one for the Render thread

////////////////////
// Static members //
////////////////////

bool					CWorldDebugHelper::bShowDataFragmentation = false;
bool					CWorldDebugHelper::bShowEntityList = false;
bool					CWorldDebugHelper::bShowBoundingSphere = false;
bool					CWorldDebugHelper::bShowSolidBoundingSphere = false;
bool					CWorldDebugHelper::bShowBoundingBox = false;
bool					CWorldDebugHelper::bEkgEnabledAddsRemoves = false;
bool					CWorldDebugHelper::bDrawEntireMapAABB = false;
bool					CWorldDebugHelper::bEntityContainerStats = false;
int						CWorldDebugHelper::listOffset = 0;
int						CWorldDebugHelper::lodOffset = 0;

RegdEnt					CWorldDebugHelper::pEntity( NULL );

// used by early reject code
bool					CWorldDebugHelper::bEarlyRejectExtraDistCull = false;
int						CWorldDebugHelper::extraCullMaxDist = 1000;
int						CWorldDebugHelper::extraCullMinDist = 0;
int						CWorldDebugHelper::cullDepth = 50;

DefragPool_SGapInfo*	CWorldDebugHelper::m_asBankGapBuffer = NULL;
u32						CWorldDebugHelper::m_nBankNumGaps;


///////////////////////
// Utility functions //
///////////////////////

template <typename TagType> class Tree
{
private:

	Tree*			parent;
	atSList<Tree*>	children;

	TagType			tag;

public:

	Tree(TagType tag)
		: tag(tag), parent(NULL), children()
	{ }

	Tree* GetFather() const {
		return parent;
	}

	TagType GetTag() const {
		return tag;
	}

	s32 GetNumChildren() const {
		return children.GetNumItems();
	}

	Tree* GetChildByIndex(const s32 i) const {
		return children[i];
	}

	Tree* GetChildByTag(const TagType& tag) const
	{
		for (s32 c = 0; c < GetNumChildren(); c++)
		{
			Tree* const		pChild = GetChildByIndex(c);
			
			if ( pChild->tag == tag )
				return pChild;
		}

		return NULL;
	}

	void AttachChild(Tree* pChild)
	{
		atSNode<Tree*>*		node = rage_new atSNode<Tree*>( pChild );
		children.Append( *node );
		pChild->parent = this;
	}

	~Tree()
	{
		atSNode<Tree*>*		pCurrentNode = children.GetHead();
		atSNode<Tree*>*		pNextNode;

		while ( pCurrentNode )
		{
			pNextNode = pCurrentNode->GetNext();

			delete pCurrentNode->Data;
			delete pCurrentNode;
			
			pCurrentNode = pNextNode;
		}
	}
};

//////////////////////////////////
// Method to add widgets to RAG //
//////////////////////////////////

void CWorldDebugHelper::AddBankWidgets(bkBank* pBank)
{
	Assert(pBank);

	pBank->PushGroup( "Entity list", false );
	pBank->AddToggle("Show entity list for current quad tree", &bShowEntityList );
	pBank->AddToggle("Show bounding sphere for selected entity", &bShowBoundingSphere );
	pBank->AddToggle("Show solid bounding sphere", &bShowSolidBoundingSphere );
	pBank->AddToggle("Show bounding box for selected entity", &bShowBoundingBox );
	pBank->PopGroup();

	pBank->PushGroup("Early Reject", false);
	pBank->AddToggle("Extra dist cull", &bEarlyRejectExtraDistCull);
	pBank->AddSlider("Min Dist", &extraCullMinDist, 0, 16000, 50);
	pBank->AddSlider("Max Dist", &extraCullMaxDist, 0, 16000, 50);
	pBank->AddSeparator();
	pBank->AddSlider("Offset", &extraCullMinDist, 0, 6000, 50, datCallback(CullDepthAltered));
	pBank->AddSlider("Depth", &cullDepth, 0, 6000, 50, datCallback(CullDepthAltered));
	pBank->PopGroup();

	pBank->PushGroup("World add/remove/update", false);
	pBank->AddToggle("Show data fragmentation", &bShowDataFragmentation);
	STATS_ONLY( pBank->AddToggle("Enable EKG Profiler", &bEkgEnabledAddsRemoves, datCallback(ToggleEkgActiveAddsRemoves)); )
	pBank->PopGroup();

	fwSceneUpdate::AddWidgets(*pBank);

	pBank->PushGroup("World AABB");
		pBank->AddToggle("bDrawEntireMapAABB", &bDrawEntireMapAABB, datCallback(ToggleDrawEntireMapAABB));
	pBank->PopGroup();

	pBank->PushGroup("Entity Containers");
		pBank->AddToggle("Container stats", &bEntityContainerStats);
	pBank->PopGroup();

	pBank->PushGroup("World Height Map");
		CGameWorldHeightMap::AddWidgets(pBank);
	pBank->PopGroup();

	pBank->PushGroup("World Water Height");
		CGameWorldWaterHeight::AddWidgets(pBank);
	pBank->PopGroup();

#if __PS3
	pBank->PushGroup("World Water Boundary");
		extern void WaterBoundary_AddWidgets(bkBank* pBank);
		WaterBoundary_AddWidgets(pBank);
	pBank->PopGroup();
#endif // __PS3
}

//////////////////////////////////////////////////////////////////////
// Structs and data to manage display of quadtree rep / entity list //
//////////////////////////////////////////////////////////////////////

enum ClickState
{
	CLICK_TITLE,
	CLICK_LIST,
	CLICK_LOD,
	CLICK_DETAIL,
	CLICK_NONE,			// User clicked, but not on any "sensible" area
	CLICK_NULL			// User didn't click
};

static const fwRect		titleArea( 50, 150, 250, 200 );
static const fwRect		listArea( 50, 200, 250, 500 );
static const fwRect		lodArea( 250, 150, 550, 350 );
static const fwRect		detailArea( 250, 350, 550, 500 );
static const fwRect		quadtreeArea( 700, 100, 700 + 512, 100 + 512 );

////////////////////////
// UpdateDebug method //
////////////////////////

void CWorldDebugHelper::UpdateDebug()
{
#if __BANK
	fwSceneUpdate::DisplaySceneUpdateList();
#endif // __BANK

	if (bEntityContainerStats)
	{
		PrintEntityContainerStats();
	}

	EntityListGuiInfo&		guiInfo = guiInfoBuffers[ gRenderThreadInterface.GetUpdateBuffer() ];

	//////////////////// Fetch mouse input

	const Vector2			mousePosition = Vector2( static_cast<float>(ioMouse::GetX()), static_cast<float>(ioMouse::GetY()) );
	const s32				mouseWheel = ioMouse::GetDZ();
	ClickState				clickState = CLICK_NULL;

	if ( ioMouse::GetButtons() != 0 )
	{
		if ( bShowEntityList && titleArea.IsInside(mousePosition) )
			clickState = CLICK_TITLE;
		else if ( bShowEntityList && listArea.IsInside(mousePosition) )
			clickState = CLICK_LIST;
		else if ( bShowEntityList && lodArea.IsInside(mousePosition) )
			clickState = CLICK_LOD;
		else if ( bShowEntityList && detailArea.IsInside(mousePosition) )
			clickState = CLICK_DETAIL;
	}

	//////////////////// Process input for list area

	if ( bShowEntityList )
	{
		if ( listArea.IsInside( mousePosition ) )
			listOffset += mouseWheel * -5;

		if ( clickState == CLICK_LIST )
		{
			const s32		selectedOffset = (s32)( mousePosition.y - listArea.bottom ) / 10;	// Every entity caption is 10 pixel high
			if ( guiInfo.listPointers[ selectedOffset ] )
				pEntity = guiInfo.listPointers[ selectedOffset ];
		}
	}

	//////////////////// Process input for LOD area

	if ( bShowEntityList )
	{
		if ( lodArea.IsInside( mousePosition ) )
			lodOffset += mouseWheel * -5;

		if ( clickState == CLICK_LOD )
		{
			const s32		selectedOffset = (s32)( mousePosition.y - lodArea.bottom ) / 10;	// Every entity caption is 10 pixel high
			if ( guiInfo.lodPointers[ selectedOffset ] )
			{
				pEntity = guiInfo.lodPointers[ selectedOffset ];
				lodOffset = 0;
			}
		}
	}

	//////////////////// Get text for list area

	guiInfo.numListEntries = 0;
	memset( guiInfo.listText, 0, sizeof(char) * MAX_NUM_LIST_ENTRIES * MAX_LIST_TEXT_LENGTH );
	memset( guiInfo.listColor, 0, sizeof(Color32) * MAX_NUM_LIST_ENTRIES );
	memset( guiInfo.listPointers, 0, sizeof(CEntity*) * MAX_NUM_LIST_ENTRIES );
	
	//if ( bShowEntityList && pNode && pNode->GetStaticContainer()->GetEntityCount() > 0 )
	//{
	//	const s32					numEntityDescs = (s32) pNode->GetStaticContainer()->GetEntityCount();
	//	const fwEntityDesc* const	entityDescs = pNode->GetStaticContainer()->GetEntityDescArray();
	//	
	//	listOffset = rage::Clamp( listOffset, 0, numEntityDescs - 1 );

	//	s32		e;
	//	for (e = 0; e < MAX_NUM_LIST_ENTRIES && listOffset + e < numEntityDescs; e++)
	//	{
	//		const fwEntityDesc* const	pEntityDesc = &entityDescs[ listOffset + e ];
	//		CEntity* const				pEntity = static_cast<CEntity*>(pEntityDesc->GetEntity());

	//		const s32					modelIndex = pEntity->GetModelIndex();
	//		const char* const			modelName = CModelInfo::GetBaseModelInfoName( modelIndex );

	//		sprintf( guiInfo.listText[e], "%d. %s", listOffset + e, modelName );

	//		guiInfo.listColor[e] = Color32( 255, 255, 255, 255 );

	//		guiInfo.listPointers[e] = pEntity;
	//	}

	//	guiInfo.numListEntries = e;
	//}

	//////////////////// Get text for LOD area

	guiInfo.numLodEntries = 0;
	memset( guiInfo.lodText, 0, sizeof(char) * MAX_NUM_LOD_ENTRIES * MAX_LOD_TEXT_LENGTH );
	memset( guiInfo.lodColor, 0, sizeof(Color32) * MAX_NUM_LOD_ENTRIES );
	memset( guiInfo.lodPointers, 0, sizeof(CEntity*) * MAX_NUM_LOD_ENTRIES );

// 	if ( bShowEntityList && pEntity )
// 	{
// 		Tree<CEntity*>*		pTree = GetLodTreeForEntity( pEntity );
// 		s32				entriesToIgnore = lodOffset;
// 		FillLodList( pTree, &guiInfo, entriesToIgnore );
// 		delete pTree;
// 	}
	
	//////////////////// Get text for detail area

	guiInfo.numDetailEntries = 0;
	memset( guiInfo.detailText, 0, sizeof(char) * MAX_NUM_DETAIL_ENTRIES * MAX_DETAIL_TEXT_LENGTH );
	memset( guiInfo.detailColor, 0, sizeof(Color32) * MAX_NUM_DETAIL_ENTRIES );

	if ( bShowEntityList && pEntity )
	{
		const char* const		modelName = pEntity->GetBaseModelInfo()->GetModelName();

		sprintf( guiInfo.detailText[0], "%s", modelName );
		sprintf( guiInfo.detailText[1], "Address: %p", pEntity.Get() );
		sprintf( guiInfo.detailText[2], "IsStatic: %d, IsPhysical: %d", pEntity->GetIsStatic(), pEntity->GetIsPhysical() );
		
		const Vector3			position( VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()) );

		sprintf( guiInfo.detailText[3], "Position: (%.2f, %.2f, %.2f)", position.x, position.y, position.z );
		sprintf( guiInfo.detailText[4], "Heading: %f", pEntity->GetTransform().GetHeading() );
		sprintf( guiInfo.detailText[5], "Entity type: %s", GetEntityTypeStr(pEntity->GetType()) );

		s32		currentEntry = 6;
//		CEntity*	pCurrentEntity = pEntity;
// 		while ( pCurrentEntity )
// 		{
// 			const float			fadeDistance = pCurrentEntity->GetLodDist();
// 			Vector3				cameraDelta = pCurrentEntity->GetPosition();
// 			cameraDelta.Subtract( camInterface::GetPos() );
// 			const float			cameraDistance = cameraDelta.Mag();
// 			
// 			sprintf( guiInfo.detailText[ currentEntry ], "Fading values: %9.2f %9.2f %d", fadeDistance, cameraDistance, pCurrentEntity->GetAlphaFade() );
// 
// 			pCurrentEntity = pCurrentEntity->__GetLod();
// 			currentEntry++;
// 		}
		
		guiInfo.numDetailEntries = currentEntry;
	}

	//////////////////// Get bounding shapes for selected entity

	guiInfo.boundingShapesAreValid = false;

	if ( pEntity )
	{
		if ( bShowBoundingSphere )
		{
			guiInfo.entityBoundingSphereCenter = pEntity->GetBoundCentre();
			guiInfo.entityBoundingSphereRadius = pEntity->GetBoundRadius();
		}

		if ( bShowBoundingBox )
			guiInfo.entityBoundingBox = pEntity->GetBoundBox( guiInfo.entityBoundingBox );

		guiInfo.boundingShapesAreValid = true;
	}
}

//////////////////////
// DrawDebug method //
//////////////////////

void CWorldDebugHelper::DrawDebug()
{
	EntityListGuiInfo&		guiInfo = guiInfoBuffers[ gRenderThreadInterface.GetRenderBuffer() ];

	// This macro's purpose is to convert from absolute screen coords (i.e. actual pixel coordinates)
	// to relative screen coords (i.e. floats in range [0,1] ) and create a Vector2 out of them.
	// It presumes a screen resolution of 720p: change if needed.

	CTextLayout DebugTextLayout;

#undef SCREEN_COORDS
#define SCREEN_COORDS(x,y)		Vector2( ( static_cast<float>(x) / 1280.0f ), ( static_cast<float>(y) / 720.0f ) )

	//////////////////// Init

	DebugTextLayout.SetScale( Vector2( 0.4f, 0.4f ) );

	//////////////////// Draw pool defrag table, with selected node (if applicable)

	if ( bShowDataFragmentation )
	{
		fwDescPool* pDefragPool = fwEntityContainer::GetDefragPool();
		Assert( pDefragPool );
		DefragPoolCopyGapsToBuffer( pDefragPool );

		static const float fMapScreenX = 250.0f;
		static const float fMapScreenY = 200.0f;
		static const float fMapScreenW = 400.0f;
		static const float fMapScreenH = 400.0f;
		static const u32 nTotalEntries = pDefragPool->GetMaxEntries();
		static const float fIdealNumEntriesAlongSide = sqrt((float)nTotalEntries);

		bool bSquare = true;
		u32 nNumEntriesAcross = (u32) fIdealNumEntriesAlongSide;
		u32 nNumEntriesHigh = (u32) fIdealNumEntriesAlongSide;

		if ((nNumEntriesAcross * nNumEntriesAcross) != nTotalEntries)
		{
			// not perfect square
			bSquare = false;
			nNumEntriesHigh++;
		}

		float fCellScreenW = fMapScreenW / (float) nNumEntriesAcross;
		float fCellScreenH = fMapScreenH / (float) nNumEntriesHigh;

		// draw background (allocated space)
		fwRect bgRect(fMapScreenX, fMapScreenY+fMapScreenH, fMapScreenX+fMapScreenW, fMapScreenY);
		CSprite2d::DrawRectSlow(bgRect, Color32(0, 165, 0, 120));

		// draw gaps
		Vector2 pos(fMapScreenX, fMapScreenY);
		DefragPoolDrawDebugAllGaps(pos, fCellScreenW, fCellScreenH, nNumEntriesAcross);

		if (!bSquare)
		{
			// TODO - draw the trailing unused area if memory size isn't perfect square
		}
	}

	//////////////////// Draw entity list GUI windows and title

	if ( bShowEntityList )
	{
		const Color32	titleBackground( 0, 0, 0, 180 );
		const Color32	listBackground( 60, 0, 0, 180 );
		const Color32	lodBackground( 0, 60, 0, 180 );
		const Color32	detailBackground( 0, 0, 60, 180 );

		CSprite2d::DrawRectSlow( titleArea, titleBackground );
		CSprite2d::DrawRectSlow( listArea, listBackground );
		CSprite2d::DrawRectSlow( lodArea, lodBackground );
		CSprite2d::DrawRectSlow( detailArea, detailBackground );

		const char		titleText[3][64] = { "ENTITY LIST", "Use mouse wheel to scroll", "Click onto entities for details" };

		for (int i=0; i<3; i++)
		{
			DebugTextLayout.SetColor( Color32( 255, 255, 255, 255 ) );
			DebugTextLayout.Render( SCREEN_COORDS( titleArea.left, titleArea.bottom + 10 * i ), titleText[i] );
		}
	}

	//////////////////// Draw the list area text

	if ( bShowEntityList )
	{
		Vector2			textPosition = SCREEN_COORDS( listArea.left, listArea.bottom );

		for (s32 e = 0; e < (s32) guiInfo.numListEntries; e++)
		{
			DebugTextLayout.SetColor( guiInfo.listColor[e] );
			DebugTextLayout.Render( textPosition, guiInfo.listText[e] );

			textPosition += SCREEN_COORDS( 0, 10 );
		}
	}

	//////////////////// Draw the detail area text

	if ( bShowEntityList )
	{
		Vector2			textPosition = SCREEN_COORDS( detailArea.left, detailArea.bottom );

		for (s32 e = 0; e < (s32) guiInfo.numDetailEntries; e++)
		{
			DebugTextLayout.SetColor( Color32( 255, 255, 255, 255 ) );
			DebugTextLayout.Render( textPosition, guiInfo.detailText[e] );
			textPosition += SCREEN_COORDS( 0, 10 );
		}
	}

	//////////////////// Draw the LOD area text

	if ( bShowEntityList )
	{
		Vector2			textPosition = SCREEN_COORDS( lodArea.left, lodArea.bottom );

		for (s32 e = 0; e < (s32) guiInfo.numLodEntries; e++)
		{
			DebugTextLayout.SetColor( Color32( 255, 255, 255, 255 ) );
			DebugTextLayout.Render( textPosition, guiInfo.lodText[e] );

			textPosition += SCREEN_COORDS( 0, 10 );
		}
	}

	//////////////////// Draw the selected entity bounding shapes

	if ( bShowBoundingSphere && guiInfo.boundingShapesAreValid )
		grcDebugDraw::Sphere( guiInfo.entityBoundingSphereCenter, guiInfo.entityBoundingSphereRadius, Color32( 255, 0, 0, bShowSolidBoundingSphere ? 127 : 255 ), bShowSolidBoundingSphere );

	if ( bShowBoundingBox && guiInfo.boundingShapesAreValid )
		grcDebugDraw::BoxAxisAligned( guiInfo.entityBoundingBox.GetMin(), guiInfo.entityBoundingBox.GetMax(), Color32( 255, 0, 0, 255 ), false );

	// draw global AABB for entire map
	if (bDrawEntireMapAABB && g_WorldLimits_MapDataExtentsAABB.IsValid())
		grcDebugDraw::BoxAxisAligned(g_WorldLimits_MapDataExtentsAABB.GetMin(), g_WorldLimits_MapDataExtentsAABB.GetMax(), Color32(255, 0, 0, 255), false);

	CGameWorldHeightMap::DebugDraw(VECTOR3_TO_VEC3V(camInterface::GetPos()));
	CGameWorldWaterHeight::DebugDraw(VECTOR3_TO_VEC3V(camInterface::GetPos()));

	//////////////////// Finish

	CText::Flush();
}

#if __STATS
void CWorldDebugHelper::ToggleEkgActiveAddsRemoves()
{
	if (bEkgEnabledAddsRemoves)
	{
		GetRageProfileRenderer().GetEkgMgr(0)->SetPage(&PFPAGE_WorldMgr_AddRemovePage);
	}
	else
	{
		GetRageProfileRenderer().GetEkgMgr(0)->SetPage(NULL);
	}
}
#endif // __STATS

void CWorldDebugHelper::ToggleDrawEntireMapAABB()
{
	if (bDrawEntireMapAABB)
	{
		Displayf(
			"g_WorldLimits_MapDataExtentsAABB = [%f,%f,%f][%f,%f,%f]",
			g_WorldLimits_MapDataExtentsAABB.GetMinVector3().x,
			g_WorldLimits_MapDataExtentsAABB.GetMinVector3().y,
			g_WorldLimits_MapDataExtentsAABB.GetMinVector3().z,
			g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().x,
			g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().y,
			g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().z
		);
	}
}

void CWorldDebugHelper::CullDepthAltered()
{
	extraCullMaxDist = extraCullMinDist+cullDepth;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DefragPoolCopyGapsToBuffer
// PURPOSE:		Debug use, copies full list of gaps to intermediate buffer.
//				This is used by debug draw code.
//////////////////////////////////////////////////////////////////////////
void CWorldDebugHelper::DefragPoolCopyGapsToBuffer(fwDescPool* pPool)
{
	if ( m_asBankGapBuffer == NULL )
		m_asBankGapBuffer = (DefragPool_SGapInfo *) rage_new char[(pPool->GetMaxEntries() / 2) * sizeof(DefragPool_SGapInfo)];

	m_nBankNumGaps = 0;
	for (int i=0; i<pPool->GetGapInfo().size(); i++)
	{
		m_asBankGapBuffer[m_nBankNumGaps] = pPool->GetGapInfo()[i];
		m_nBankNumGaps++;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DefragPoolDrawDebugAllGaps
// PURPOSE:		Draws a visual representation of all gaps in pool
//////////////////////////////////////////////////////////////////////////
void CWorldDebugHelper::DefragPoolDrawDebugAllGaps(Vector2& vTopLeft, float fCellW, float fCellH, u32 nGridW)
{
	u32 nGridX, nGridY, nIndex, nLength, nSubLength;

	for (u32 i=0; i<m_nBankNumGaps;  i++)
	{
		DefragPool_SGapInfo * psGap = &m_asBankGapBuffer[i];

		nIndex = psGap->nIndex;
		nLength = psGap->nLength;

		while (nLength)
		{
			nGridX = nIndex % nGridW;
			nGridY = nIndex / nGridW;

			nSubLength = nLength;
			if ((nGridX + nLength) >= nGridW)
			{
				nSubLength = nGridW - nGridX;
			}

			fwRect bgRect(
				vTopLeft.x + nGridX * fCellW,						// left
				vTopLeft.y + (nGridY + 1) * fCellH,					// bottom
				vTopLeft.x + (nGridX + nSubLength) * fCellW,		// right
				vTopLeft.y + nGridY * fCellH);						// top
			CSprite2d::DrawRectSlow(bgRect, Color32(0, 0, 0, 120));

			nIndex += nSubLength;
			nLength -= nSubLength;
		}
	}
}

// AP: After the containers refactoring some parts of the following method are not applicable anymore (since we don't have different
// types of entity containers now
void CWorldDebugHelper::PrintEntityContainerStats()
{
	fwEntityContainer::Pool* pPool = fwEntityContainer::GetPool();
	Assert(pPool);

	// total number of containers
	s32 totalNumEntityContainers = (s32)pPool->GetNoOfUsedSpaces();

	//// number of containers by type
	//u32 aNumContainersOfType[fwEntityContainer::CONTAINER_TYPE_TOTAL];
	//memset(aNumContainersOfType, 0, fwEntityContainer::CONTAINER_TYPE_TOTAL*sizeof(u32));
	//for (u32 i=0; i<pPool->GetSize(); i++)
	//{
	//	fwEntityContainer* pContainer = pPool->GetSlot(i);
	//	if (pContainer)
	//	{
	//		u32 containerType = pContainer->GetType();
	//		Assert(containerType<fwEntityContainer::CONTAINER_TYPE_TOTAL);
	//		aNumContainersOfType[containerType] += 1;
	//	}
	//}

	// average size of container
	float fTotalSize = 0.0f;
	for (u32 i=0; i<pPool->GetSize(); i++)
	{
		fwEntityContainer* pContainer = pPool->GetSlot(i);
		if (pContainer)
		{
			fTotalSize += pContainer->GetEntityCount();
		}
	}
	float fAverageSize = (fTotalSize / (float)totalNumEntityContainers);

	// num containers with just one entry
	u32 numContainersWithOneEntry = 0;
	for (u32 i=0; i<pPool->GetSize(); i++)
	{
		fwEntityContainer* pContainer = pPool->GetSlot(i);
		if (pContainer && pContainer->GetEntityCount()==1)
		{
			numContainersWithOneEntry += 1;
		}
	}

	// entity type breakdown of single-entry containers
	u32 aEntityTypeCount[ENTITY_TYPE_TOTAL];
	memset(aEntityTypeCount, 0, ENTITY_TYPE_TOTAL*sizeof(u32));
	for (u32 i=0; i<pPool->GetSize(); i++)
	{
		fwEntityContainer* pContainer = pPool->GetSlot(i);
		if (pContainer && pContainer->GetEntityCount()==1)
		{
			fwEntity* pEntity = pContainer->GetEntity(0);
			if (pEntity)
			{
				aEntityTypeCount[pEntity->GetType()] += 1;
			}
		}
	}

	// find largest container
	u32 largestEntityCount = 0;
	for (u32 i=0; i<pPool->GetSize(); i++)
	{
		fwEntityContainer* pContainer = pPool->GetSlot(i);
		if (pContainer)
		{
			largestEntityCount = Max(largestEntityCount, pContainer->GetEntityCount());
		}
	}


	// debug draw
	//grcDebugDraw::AddDebugOutput("Total number of entity containers in use: %d", totalNumEntityContainers);
	//const char* apszTypes[] = 
	//{
	//	"INVALID",
	//	"ROOT",
	//	"QUAD_TREE_NODE",
	//	"OTHER_EXTERIOR",
	//	"INTERIOR",
	//	"ROOM",
	//	"PORTAL"
	//};
	//for (u32 i=0; i<fwEntityContainer::CONTAINER_TYPE_TOTAL; i++)
	//{
	//	grcDebugDraw::AddDebugOutput("Number of entity containers of type %s: %d", apszTypes[i], aNumContainersOfType[i]);
	//}
	grcDebugDraw::AddDebugOutput("Average container size: %f", fAverageSize);
	grcDebugDraw::AddDebugOutput("Total number of single-entry containers: %d", numContainersWithOneEntry);

	enum
	{
		ENTITY_TYPE_NOTHING,
		ENTITY_TYPE_BUILDING,			// CBuilding                             : CEntity
		ENTITY_TYPE_ANIMATED_BUILDING,	// CAnimatedBuilding    : CDynamicEntity : CEntity
		ENTITY_TYPE_VEHICLE,			// CVehicle : CPhysical : CDynamicEntity : CEntity
		ENTITY_TYPE_PED,				// CPed     : CPhysical : CDynamicEntity : CEntity
		ENTITY_TYPE_OBJECT,				// CObject  : CPhysical : CDynamicEntity : CEntity
		ENTITY_TYPE_DUMMY_OBJECT,		// CDummyObject                          : CEntity
		ENTITY_TYPE_PORTAL,				// CPortalInst   : CBuilding : CEntity
		ENTITY_TYPE_MLO,				// CInteriorInst : CBuilding : CEntity
		ENTITY_TYPE_NOTINPOOLS,			// Only used to trigger audio
		ENTITY_TYPE_PARTICLESYSTEM,		// ?
		ENTITY_TYPE_LIGHT,				// CLightEntity : CEntity
		ENTITY_TYPE_COMPOSITE,			// CCompEntity  : CEntity
		ENTITY_TYPE_INSTANCE_LIST,		// CEntityBatch : CEntityBatchBase : CEntity
		ENTITY_TYPE_GRASS_INSTANCE_LIST,// CGrassBatch : CEntityBatchBase : CEntity

		ENTITY_TYPE_TOTAL
	};
	const char* apszEntityTypes[] =
	{
		"ENTITY_TYPE_NOTHING",
		"ENTITY_TYPE_BUILDING",
		"ENTITY_TYPE_ANIMATED_BUILDING",
		"ENTITY_TYPE_VEHICLE",
		"ENTITY_TYPE_PED",
		"ENTITY_TYPE_OBJECT",
		"ENTITY_TYPE_DUMMY_OBJECT",
		"ENTITY_TYPE_PORTAL",
		"ENTITY_TYPE_MLO",
		"ENTITY_TYPE_NOTINPOOLS",
		"ENTITY_TYPE_PARTICLESYSTEM",
		"ENTITY_TYPE_LIGHT",
		"ENTITY_TYPE_COMPOSITE",
		"ENTITY_TYPE_INSTANCE_LIST",
		"ENTITY_TYPE_GRASS_INSTANCE_LIST"
	};
	for (u32 i=0; i<ENTITY_TYPE_TOTAL; i++)
	{
		grcDebugDraw::AddDebugOutput("Number of single-entry containers with entity type %s: %d", apszEntityTypes[i], aEntityTypeCount[i]);
	}
	grcDebugDraw::AddDebugOutput("Largest container size: %d", largestEntityCount);
}

#endif // __BANK
