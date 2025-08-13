#if !__FINAL

//******************************************************************************************************
//	Filename    : PathServer_Debug.cpp
//	Author      : James Broad
//	Description : Somewhere to place the navmesh & pathserver debugging code, outside of the main file.
//******************************************************************************************************

#include "PathServer\PathServer.h"
#include "PathServer\NavMeshGta.h"
#include "PathServer\NavGenParam.h"


#if defined(GTA_ENGINE) && defined(DEBUG_DRAW) && DEBUG_DRAW

// Rage headers
#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"
#include "file/stream.h"
#include "file/asset.h"
#include "grcore/debugdraw.h"
#include "grcore/device.h"
#include "string/string.h"
#include "system/xtl.h"
#include "system/criticalsection.h"
#include "vector/colors.h"
#include "vector/geometry.h"
#include "spatialdata/sphere.h"

// Framework headers
#include "ai/navmesh/navmeshextents.h"
#include "ai/navmesh/tessellation.h"
#include "fwmaths/Random.h"
#include "fwscene/world/WorldLimits.h"

#ifdef GTA_ENGINE
// Game headers
// These includes for using within the GTA engine
#include "camera/CamInterface.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/viewports/Viewport.h"
#include "Debug/DebugScene.h"
#include "Objects/Object.h"
#include "Peds/PedDebugVisualiser.h"
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "Peds/PedPopulation.h"
#include "peds/Ped.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Scene/Physical.h"
#include "scene/world/GameWorld.h"
#include "Task/Movement/TaskNavMesh.h"
#include "vehicles/vehicle.h"
#include "camera/viewports/ViewportManager.h"

#include "Text/Text.h"
#include "Text/TextConversion.h"

#else	// GTA_ENGINE

// This include for when developing the system outside of GTA
#include "GTATypes.h"
#endif

NAVMESH_OPTIMISATIONS()

// Define __CATCH_BAD_NAVMESH_VERTS to check vertices when debug-drawing the navmesh
#define __CATCH_BAD_NAVMESH_VERTS	0

bool g_bTestBlockingObjectEnabled = false;
Vector3 g_vTestBlockingObjectPos(0.0f,0.0f,0.0f);
Vector3 g_vTestBlockingObjectSize(2.0f,2.0f,1.0f);
float g_fTestBlockingObjectRot = 0.0f;



bool PathTest_bPreferPavements = false;
bool PathTest_bNeverLeaveWater = false;
bool PathTest_bGoAsFarAsPossible = false;
bool PathTest_bNeverLeavePavements = false;
bool PathTest_bNeverClimbOverStuff = false;
bool PathTest_bNeverDropFromHeight = false;
bool PathTest_bNeverUseLadders = false;
bool PathTest_bFleeTarget = false;
bool PathTest_bWander = false;
bool PathTest_bNeverEnterWater = false;
bool PathTest_bDontAvoidDynamicObjects = false;
bool PathTest_bPreferDownhill = false;
bool PathTest_bReduceObjectBBoxes = false;
bool PathTest_bUseLargerSearchExtents = false;
bool PathTest_bMayUseFatalDrops = false;
bool PathTest_bIgnoreObjectsWithNoUprootLimit = false;
bool PathTest_bAllowToTravelUnderwater = false;
bool PathTest_bUseDirectionalCover = false;
bool PathTest_bCutSharpCorners = false;
bool PathTest_bAllowToNavigateUpSteepPolygons = false;
bool PathTest_bScriptedRoute = false;
bool PathTest_bPreserveZ = false;
bool PathTest_bRandomisePoints = false;

bool PathTest_bGetPointsFromMeasuringTool = false;
float PathTest_fPathTestMaxSlopeNavigable = 0.0f;

void PathSever_VerifyAdjacencies();

#if !__FINAL

// Offset from top-left of screen
// This is here so we can tweak the positioning in game on crap monitors which have no adjustment controls
Vector2 g_vPathserverDebugTopLeft(0.0, 0.0);

Vector2 ScreenPos(const Vector2 & vPos) { return Vector2( (float)vPos.x / (float)SCREEN_WIDTH, (float)vPos.y / (float)SCREEN_HEIGHT); }
Vector2 ScreenPos(const float ix, const float iy) { return Vector2( ((float)ix) / (float)SCREEN_WIDTH, ((float)iy) / (float)SCREEN_HEIGHT); }

const char * CPathServer::ms_pNavMeshVisModeTxt[NavMeshVis_NumModes] =
{
	"Off",
	"Wireframe",
	"Transparent",
	"Solid",
	"Pavements",
	"Roads",
	"Train Tracks",
	"Ped Density",
	"Disabled",
	"Shelter",
	"Special Links",
	"Too Steep",
	"Water Surface",
	"Audio Properties",
	"Cover Directions",
	"Car Nodes",
	"Interior",
	"Isolated",
	"Network Spawn Candidates",
	"No Wander",
	"Pavement-Checking Mode"
};

void
CPathServer::DrawPolyDebug(CNavMesh * pNavMesh, u32 iPolyIndex, Color32 iCol, float fAddHeight, bool bOutline, int filledAlpha, bool bDrawCentroidQuick)
{
#ifdef GTA_ENGINE

	TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPolyIndex);

	if(bOutline)
	{
		Vector3 vec, lastvec;

		pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, pPoly->GetNumVertices()-1), lastvec );

		lastvec.z += fAddHeight;

		for(u32 v=0; v<pPoly->GetNumVertices(); v++)
		{
			pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), vec );

#if __CATCH_BAD_NAVMESH_VERTS
			Assert(rage::FPIsFinite(vec.x));
			Assert(rage::FPIsFinite(vec.y));
			Assert(rage::FPIsFinite(vec.z));

			if(pNavMesh->m_pQuadTree)
			{
				Assert(vec.x >= pNavMesh->m_pQuadTree->m_Mins.x && vec.x <= pNavMesh->m_pQuadTree->m_Maxs.x);
				Assert(vec.y >= pNavMesh->m_pQuadTree->m_Mins.y && vec.y <= pNavMesh->m_pQuadTree->m_Maxs.y);
				Assert(vec.z >= pNavMesh->m_pQuadTree->m_Mins.z && vec.z <= pNavMesh->m_pQuadTree->m_Maxs.z);
			}
#endif

			vec.z += fAddHeight;

			grcDebugDraw::Line(lastvec, vec, iCol);
			lastvec = vec;
		}
	}
	else
	{
		u32 r = iCol.GetRed();
		u32 g = iCol.GetGreen();
		u32 b = iCol.GetBlue();

		Vector3 v0,v1,v2;

		pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, 0), v0 );
		v0.z += fAddHeight;

		for(u32 v=1; v<pPoly->GetNumVertices()-1; v++)
		{
			pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), v1 );
			pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v+1), v2 );

			v1.z += fAddHeight;
			v2.z += fAddHeight;

			Color32 polyCol;
			polyCol.Set(r,g,b,filledAlpha);

			grcDebugDraw::Poly(RCC_VEC3V(v0),RCC_VEC3V(v1),RCC_VEC3V(v2),polyCol);
		}
	}
	if(bDrawCentroidQuick && pNavMesh->GetIndexOfMesh() != NAVMESH_INDEX_TESSELLATION)
	{
		Vector3 vCentroid;
		pNavMesh->GetPolyCentroidQuick(pPoly, vCentroid);
		grcDebugDraw::Line(vCentroid, vCentroid + ZAXIS*2.0f, Color_beige, Color_white);
	}
#else	// GTA_ENGINE

	pNavMesh;
	iPolyIndex;
	iCol;
	fAddHeight;
	bOutline;

#endif	// GTA_ENGINE
}

void CPathServer::DrawPolyVolumeDebug(CNavMesh * pNavMesh, int iPolyIndex, Color32 iCol1, Color32 iCol2, float fSpaceAboveTopZ)
{
#ifndef GTA_ENGINE
	pNavMesh; iPolyIndex; iCol1; iCol2; fSpaceAboveTopZ;
	return;
#else

	DrawPolyDebug(pNavMesh, iPolyIndex, iCol1, m_fVisNavMeshVertexShiftAmount, true);

	TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPolyIndex);
	static Vector3 vPolyVerts[NAVMESHPOLY_MAX_NUM_VERTICES];
	u32 p;

	for(p=0; p<pPoly->GetNumVertices(); p++)
	{
		pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, p), vPolyVerts[p] );
	}

	for(p=0; p<pPoly->GetNumVertices(); p++)
	{
		grcDebugDraw::Line(vPolyVerts[p], Vector3(vPolyVerts[p].x, vPolyVerts[p].y, fSpaceAboveTopZ), iCol1, iCol2);
	}

	Vector3 v0 = Vector3(vPolyVerts[0].x, vPolyVerts[0].y, fSpaceAboveTopZ);

	for(p=1; p<pPoly->GetNumVertices()-1; p++)
	{
		Vector3 v1 = Vector3(vPolyVerts[p].x, vPolyVerts[p].y, fSpaceAboveTopZ);
		Vector3 v2 = Vector3(vPolyVerts[p+1].x, vPolyVerts[p+1].y, fSpaceAboveTopZ);

		grcDebugDraw::Poly(RCC_VEC3V(v2),RCC_VEC3V(v1),RCC_VEC3V(v0),iCol2);
	}
#endif
}

void
#if __WIN32PC
CPathServer::DebugPolyText(CNavMesh * pNavMesh, u32 iPolyIndex)
#else
CPathServer::DebugPolyText(CNavMesh * UNUSED_PARAM(pNavMesh), u32 UNUSED_PARAM(iPolyIndex))
#endif
{
#if __WIN32PC
	TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPolyIndex);
	Assert(pPoly->GetNavMeshIndex() == pNavMesh->GetIndexOfMesh());

	char tmp[256];

	sprintf(tmp, "NavMesh %i Poly %i (addr:0x%p)\n", pNavMesh->GetIndexOfMesh(), iPolyIndex, pPoly);
	OutputDebugString(tmp);

	sprintf(tmp, "NumVertices %i, PedDensity %i, Flags %i\n", pPoly->GetNumVertices(), pPoly->GetPedDensity(), pPoly->GetFlags());
	OutputDebugString(tmp);

	sprintf(tmp, "Area %.2f\n", pNavMesh->GetPolyArea(iPolyIndex));
	OutputDebugString(tmp);

	Vector3 vPolyNormal;
	pNavMesh->GetPolyNormal(vPolyNormal, pPoly);
	const float fDotUp = DotProduct(ZAXIS, vPolyNormal);
	sprintf(tmp, "PolyNormal (%.2f, %.2f, %.2f), DotUp=%.2f\n", vPolyNormal.x, vPolyNormal.y, vPolyNormal.z, fDotUp);
	OutputDebugString(tmp);

	u32 testFlags = NAVMESHPOLY_SMALL | NAVMESHPOLY_LARGE | NAVMESHPOLY_OPEN | NAVMESHPOLY_CLOSED | NAVMESHPOLY_TESSELLATED_FRAGMENT | NAVMESHPOLY_REPLACED_BY_TESSELLATION;

	if(pPoly->GetIsSmall()) OutputDebugString("(small)");
	if(pPoly->GetIsLarge()) OutputDebugString("(large)");
	if(pPoly->GetIsOpen()) OutputDebugString("(open)");
	if(pPoly->GetIsClosed()) OutputDebugString("(closed)");
	if(pPoly->GetIsTessellatedFragment()) OutputDebugString("(frag)");
	if(pPoly->GetReplacedByTessellation()) OutputDebugString("(replaced)");

	if(pPoly->TestFlags(testFlags)) OutputDebugString("\n");

	Vector3 vert, nextVert, vDiff, vNormal;
	int nextv, vi1, vi2;
	u32 v;

	Vector3 vMins(FLT_MAX, FLT_MAX, FLT_MAX);
	Vector3 vMaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);

	for(v=0; v<pPoly->GetNumVertices(); v++)
	{
		nextv = (v+1) % pPoly->GetNumVertices();

		vi1 = pNavMesh->GetPolyVertexIndex(pPoly, v);
		vi2 = pNavMesh->GetPolyVertexIndex(pPoly, nextv);

		pNavMesh->GetVertex(vi1, vert);
		pNavMesh->GetVertex(vi2, nextVert);

		if(vert.x < vMins.x) vMins.x = vert.x;
		if(vert.y < vMins.y) vMins.y = vert.y;
		if(vert.z < vMins.z) vMins.z = vert.z;
		if(vert.x > vMaxs.x) vMaxs.x = vert.x;
		if(vert.y > vMaxs.y) vMaxs.y = vert.y;
		if(vert.z > vMaxs.z) vMaxs.z = vert.z;

		vDiff = nextVert - vert;

		const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly(pPoly->GetFirstVertexIndex() + v);
		pNavMesh->GetPolyEdgeNormal(vNormal, pPoly, v);

		sprintf(tmp, "%i) [%i] (%.2f, %.2f, %.2f) \t adjType:%i (adjNav:%i, adjPoly:%i, origAdjNav:%i, origAdjPoly:%i) length : %.2f normal : (%.2f, %.2f, %.2f)\n",
			v,
			vi1,
			vert.x, vert.y, vert.z,
			adjPoly.GetAdjacencyType(),
			adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes()), adjPoly.GetPolyIndex(),
			adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes()), adjPoly.GetOriginalPolyIndex(),
			vDiff.Mag(),
			vNormal.x, vNormal.y, vNormal.z
			);
		OutputDebugString(tmp);
	}

	Vector3 vSize = vMaxs - vMins;
	sprintf(tmp, "Mins (%.1f, %.1f, %.1f) Maxs (%.1f, %.1f, %.1f) Size (%.1f, %.1f, %.1f)\n",
		vMins.x, vMins.y, vMins.z, vMaxs.x, vMaxs.y, vMaxs.z, vSize.x, vSize.y, vSize.z
		);
	OutputDebugString(tmp);

#endif
}

#ifdef GTA_ENGINE
void
CPathServer::ClearAllPathHistory()
{
	for(int p=0; p<MAX_NUM_PATH_REQUESTS; p++)
	{
		m_PathRequests[p].Clear();
		CPathServer::m_PathRequests[p].m_bSlotEmpty = true;
	}
}
void CPathServer::ClearAllOpenClosedFlags()
{
	LOCK_STORE_LOADED_MESHES;

	for(int n=0; n<m_pNavMeshStores[kNavDomainRegular]->GetLoadedMeshes().GetCount(); n++)
	{
		u32 iMesh = m_pNavMeshStores[kNavDomainRegular]->GetLoadedMeshes()[n];
		CNavMesh * pMesh = m_pNavMeshStores[kNavDomainRegular]->GetMeshByIndex(iMesh);
		if(pMesh)
		{
			for(int p=0; p<pMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly * pPoly = pMesh->GetPoly(p);
				pPoly->SetIsOpen(false);
				pPoly->SetIsClosed(false);
			}
		}
	}
	for(int p=0; p<GetTessellationNavMesh()->GetNumPolys(); p++)
	{
		TNavMeshPoly * pPoly = GetTessellationNavMesh()->GetPoly(p);
		pPoly->SetIsOpen(false);
		pPoly->SetIsClosed(false);
	}

#ifndef GTA_ENGINE
	UNLOCK_STORE_LOADED_MESHES;
#endif
}

void CPathServer::DebugDrawPedGenBlockingAreas()
{
	Color32 cols[4] = { Color_red, Color_red1, Color_red2, Color_red3 };

	s32 a;
	for(a=0; a<MAX_PEDGEN_BLOCKED_AREAS; a++)
	{
		if(m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_bActive)
		{
			if(m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_iType==CPedGenBlockedArea::ESphere)
			{
				grcDebugDraw::Sphere(m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Sphere.GetOrigin(), m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Sphere.m_fActiveRadius, cols[0], false);
			}
			else if(m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_iType==CPedGenBlockedArea::EAngledArea)
			{
				int lastp = 3;
				for(int p=0; p<4; p++)
				{
					Vector3 vTop(
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[p][0],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[p][1],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_fTopZ);

					Vector3 vLastTop(
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[lastp][0],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[lastp][1],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_fTopZ);

					Vector3 vBottom(
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[p][0],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[p][1],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_fBottomZ);

					Vector3 vLastBottom(
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[lastp][0],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_CornerPts[lastp][1],
						m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.m_fBottomZ);

					const Color32 col = cols[p];
					const Color32 lastcol = cols[lastp];

					// Draw the edges of the area
					grcDebugDraw::Line(vLastTop, vTop, lastcol, col);
					grcDebugDraw::Line(vLastBottom, vBottom, lastcol, col);
					grcDebugDraw::Line(vTop, vBottom, col);

					// Draw the normal of this face, from the centre of the face
					const Vector3 vFaceCentre = (vLastTop + vTop + vLastBottom + vBottom) * 0.25f;
					const Vector3 vNormal = m_AmbientPedGeneration.m_PedGenBlockedAreas[a].m_Area.GetNormal3(p);
					grcDebugDraw::Line(vFaceCentre, vFaceCentre + vNormal, col, Color_white);

					lastp = p;
				}
			}
		}
	}
}

void
CPathServer::DebugDrawPedSwitchAreas()
{
	s32 a;
	for(a=0; a<m_iNumPathRegionSwitches; a++)
	{
		if(m_PathRegionSwitches[a].m_iScriptUID != 0)
		{
			Vector3 vMid = (m_PathRegionSwitches[a].m_vMin + m_PathRegionSwitches[a].m_vMax) * 0.5f;

			if(m_PathRegionSwitches[a].m_Switch & SWITCH_NAVMESH_DISABLE_AMBIENT_PEDS)
			{
				grcDebugDraw::BoxAxisAligned(RCC_VEC3V(m_PathRegionSwitches[a].m_vMin), RCC_VEC3V(m_PathRegionSwitches[a].m_vMax), Color_red, false);
				grcDebugDraw::Text(vMid, Color_red, 0, 0, "NAVMESH NO WANDERING");
			}
			else if(m_PathRegionSwitches[a].m_Switch & SWITCH_NAVMESH_DISABLE_POLYGONS)
			{
				grcDebugDraw::BoxAxisAligned(RCC_VEC3V(m_PathRegionSwitches[a].m_vMin), RCC_VEC3V(m_PathRegionSwitches[a].m_vMax), Color_black, false);
				grcDebugDraw::Text(vMid, Color_black, 0, 0, "NAVMESH DISABLED!!!");
			}
			else
			{
				grcDebugDraw::BoxAxisAligned(RCC_VEC3V(m_PathRegionSwitches[a].m_vMin), RCC_VEC3V(m_PathRegionSwitches[a].m_vMax), Color_green, false);
			}
		}
	}
}
#endif

#endif // !__FINAL


#if !__FINAL

bool gOnlyVisualiseDynamicNavmeshes=false;

// This function displays the meshes for debugging purposes

float g_fClosestHierNodeDist2 = FLT_MAX;
CHierarchicalNavNode * g_pClosestHierNode = NULL;

#ifdef GTA_ENGINE
bool CPathServer::TransformWorldPosToScreenPos( const Vector3 & vWorldPos, Vector2 & vScreenPos )
{
	CViewport* pVP = gVpMan.GetGameViewport();;
	if(!pVP)
	{
		return false;
	}

	// look at rage code for clipping with grcViewport, and ???? private??? maybe just clip against near?
	bool isVisible = true;
	float fOutZ;
	if(pVP->GetGrcViewport().IsSphereVisible(vWorldPos.x, vWorldPos.y, vWorldPos.z, 0.1f, &fOutZ)==cullOutside)
	{
		isVisible = false;
	}

	pVP->GetGrcViewport().Transform(VECTOR3_TO_VEC3V(vWorldPos), vScreenPos.x, vScreenPos.y );

	// convert screen position to 0-1 space:
	vScreenPos.x /= SCREEN_WIDTH;
	vScreenPos.y /= SCREEN_HEIGHT;

	vScreenPos.x = rage::Clamp(vScreenPos.x, 0.0f, 1.0f);
	vScreenPos.y = rage::Clamp(vScreenPos.y, 0.0f, 1.0f);

	return isVisible;
}
#endif

bool g_VisPolyUnderCamera = false;
void
CPathServer::Visualise()
{
#ifdef GTA_ENGINE
	aiNavDomain domain = (aiNavDomain)m_iVisualiseDataSet;

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin			= debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
	float fHeightAboveMesh	= debugDirector.IsFreeCamActive() ? 30.0f : 3.0f;

	aiNavMeshStore* pStore = m_pNavMeshStores[domain];

	if(m_bVerifyAdjacencies)
	{
		if(fwTimer::GetFrameCount() & 0x5)
		{
			PathSever_VerifyAdjacencies();
		}
	}

	if(m_iVisualiseNavMeshes)
	{
		LOCK_NAVMESH_DATA;

		//********************************************
		//	Display NavMesh
		//********************************************

		if(!gOnlyVisualiseDynamicNavmeshes)
		{
			u32 iNavMeshIndices[10];

			static const float fSizeOfBlockX = CPathServerExtents::GetWorldWidthOfSector() * pStore->GetNumSectorsPerMesh();
			static const float fSizeOfBlockY = CPathServerExtents::GetWorldWidthOfSector() * pStore->GetNumSectorsPerMesh();

			iNavMeshIndices[0] = GetNavMeshIndexFromPosition(vOrigin + Vector3(-fSizeOfBlockX,-fSizeOfBlockY,0), domain);
			iNavMeshIndices[1] = GetNavMeshIndexFromPosition(vOrigin + Vector3(0,-fSizeOfBlockY,0), domain);
			iNavMeshIndices[2] = GetNavMeshIndexFromPosition(vOrigin + Vector3(fSizeOfBlockX,-fSizeOfBlockY,0), domain);
			iNavMeshIndices[3] = GetNavMeshIndexFromPosition(vOrigin + Vector3(-fSizeOfBlockX,0,0), domain);
			iNavMeshIndices[4] = GetNavMeshIndexFromPosition(vOrigin, domain);
			iNavMeshIndices[5] = GetNavMeshIndexFromPosition(vOrigin + Vector3(fSizeOfBlockX,0,0), domain);
			iNavMeshIndices[6] = GetNavMeshIndexFromPosition(vOrigin + Vector3(-fSizeOfBlockX,fSizeOfBlockY,0), domain);
			iNavMeshIndices[7] = GetNavMeshIndexFromPosition(vOrigin + Vector3(0,fSizeOfBlockY,0), domain);
			iNavMeshIndices[8] = GetNavMeshIndexFromPosition(vOrigin + Vector3(fSizeOfBlockY,fSizeOfBlockY,0), domain);

			iNavMeshIndices[9] = NAVMESH_INDEX_TESSELLATION;	// This is special navmesh used to store tessellated polys

			for(int i=0; i<10; i++)
			{
				u32 index = iNavMeshIndices[i];

				if(index == NAVMESH_NAVMESH_INDEX_NONE || (index != NAVMESH_INDEX_TESSELLATION && index >= pStore->GetMaxMeshIndex()))
					continue;

				if(index != NAVMESH_NAVMESH_INDEX_NONE)
				{
					CNavMesh * pMesh = GetNavMeshFromIndex(index, domain);
					VisualiseNavMesh(pMesh, vOrigin, fHeightAboveMesh, (i==4), domain);
				}
			}
		}

		if(CPathServer::m_iNumDynamicNavMeshesInExistence)
		{
			for(int n=0; n<CVehicle::GetPool()->GetSize(); n++)
			{
				CVehicle * pVehicle = CVehicle::GetPool()->GetSlot(n);
				if(pVehicle)
				{
					const CModelInfoNavMeshRef * pRef = CPathServer::m_DynamicNavMeshStore.GetByModelIndex(pVehicle->GetModelIndex());
					if(pRef && pRef->m_pBackUpNavmeshCopy)
					{
						pRef->m_pBackUpNavmeshCopy->SetMatrix( MAT34V_TO_MATRIX34(pVehicle->GetMatrix()) );
						VisualiseNavMesh(pRef->m_pBackUpNavmeshCopy, vOrigin, fHeightAboveMesh, false, domain);
					}
				}
			}
		}

		//******************************************************
		// Draw the dynamic objects as well.
		//******************************************************

		Color32 objColNormal(0x80,0x80,0xff,0xff);
		Color32 objColHasUprootLimit(0xf0,0x00,0xa0,0xff); 
		Color32 objColNoObstacle(0xb0,0x40,0x60,0xff);
		Color32 objColIsDoor(0x90,0xa0,0x20,0xff);
		Color32 objColIsUserObstable(0x50,0x10,0xaa,0xff);

		// Wait until dynamic objects are available
		LOCK_DYNAMIC_OBJECTS_DATA;

		Vector3 vTopPts[4];
		Vector3 vBottomPts[4];

		TDynamicObject * pCurr = m_PathServerThread.m_pFirstDynamicObject;
		while(pCurr)
		{
			if(pCurr->m_pEntity)
			{
				Vector3 vCenter(0.0f,0.0f,0.0f);
				for(s32 t=0; t<4; t++)
				{
					vTopPts[t] = Vector3(pCurr->m_vVertices[t].x, pCurr->m_vVertices[t].y, pCurr->m_NewBounds.GetTopZ());
					vBottomPts[t] = Vector3(pCurr->m_vVertices[t].x, pCurr->m_vVertices[t].y, pCurr->m_NewBounds.GetBottomZ());

					vCenter += vTopPts[t];
					vCenter += vBottomPts[t];
				}

				vCenter *= 0.125f;

				const Vector3 vDiff = vOrigin - vCenter;
				if(vDiff.Mag2() < CPathServer::ms_fDebugVisPolysMaxDist*CPathServer::ms_fDebugVisPolysMaxDist)
				{
					const float fRadius = (pCurr->m_vVertices[0] - Vector2(pCurr->m_Bounds.GetOrigin().x, pCurr->m_Bounds.GetOrigin().y)).Mag();
					if(camInterface::IsSphereVisibleInGameViewport(vCenter, fRadius))
					{
						const bool bIgnorable = pCurr->m_bInactiveDueToVelocity || (pCurr->m_bIsCurrentlyAnObstacle==false);
						Color32 objCol;

						if(pCurr->m_bIsUserAddedBlockingObject)
							objCol = bIgnorable ? objColNoObstacle : objColIsUserObstable;
						else if(pCurr->m_bIsOpenable)
							objCol = bIgnorable ? objColNoObstacle : objColIsDoor;
						else if(pCurr->m_bHasUprootLimit)
							objCol = bIgnorable ? objColNoObstacle : objColHasUprootLimit;
						else
							objCol = bIgnorable ? objColNoObstacle : objColNormal;

						grcDebugDraw::Line(vTopPts[0], vTopPts[1], objCol);
						grcDebugDraw::Line(vTopPts[1], vTopPts[2], objCol);
						grcDebugDraw::Line(vTopPts[2], vTopPts[3], objCol);
						grcDebugDraw::Line(vTopPts[3], vTopPts[0], objCol);

						grcDebugDraw::Line(vBottomPts[0], vBottomPts[1], objCol);
						grcDebugDraw::Line(vBottomPts[1], vBottomPts[2], objCol);
						grcDebugDraw::Line(vBottomPts[2], vBottomPts[3], objCol);
						grcDebugDraw::Line(vBottomPts[3], vBottomPts[0], objCol);

						grcDebugDraw::Line(vTopPts[0], vBottomPts[0], objCol);
						grcDebugDraw::Line(vTopPts[1], vBottomPts[1], objCol);
						grcDebugDraw::Line(vTopPts[2], vBottomPts[2], objCol);
						grcDebugDraw::Line(vTopPts[3], vBottomPts[3], objCol);

						if(ms_bDrawDynamicObjectGrids)
						{
							char tmp[64];
							sprintf(tmp, "obj %" PTRDIFFTFMT "i (0x%p)", pCurr - m_PathServerThread.m_DynamicObjectsStore, pCurr);

							grcDebugDraw::Text(vCenter, objCol, 0, 0, tmp);
						}
						/*
						if(ms_bDrawDynamicObjectAxes)
						{
							Matrix34 mat;
							mat.Identity();
							mat.a = pCurr->m_Bounds.m_vRight;
							mat.b = pCurr->m_Bounds.m_vForwards;
							mat.c = CrossProduct(mat.a, mat.b);
							mat.d = pCurr->m_Bounds.m_vOrigin;
							grcDebugDraw::Axis(mat, 1.0f, false);
						}
						*/
					}
					if(CPathServer::ms_bDrawDynamicObjectMinMaxs)
					{
						Vector3 vMin, vMax;
						pCurr->m_MinMax.GetAsFloats(vMin, vMax);

						Vector3 vCorners[8] = {
							Vector3(vMin.x, vMin.y, vMin.z), Vector3(vMax.x, vMin.y, vMin.z), Vector3(vMax.x, vMax.y, vMin.z), Vector3(vMin.x, vMax.y, vMin.z),
								Vector3(vMin.x, vMin.y, vMax.z), Vector3(vMax.x, vMin.y, vMax.z), Vector3(vMax.x, vMax.y, vMax.z), Vector3(vMin.x, vMax.y, vMax.z)
						};

						Color32 color(192,32,48,255);
						for(s32 v=0; v<4; v++)
						{
							grcDebugDraw::Line(vCorners[v], vCorners[(v+1)%4], color);
							grcDebugDraw::Line(vCorners[v+4], vCorners[((v+1)%4)+4], color);
							grcDebugDraw::Line(vCorners[v], vCorners[v+4], color);
						}
					}
				}
			}

			pCurr = pCurr->m_pNext;
		}

		if(ms_bDrawDynamicObjectGrids)
		{
			Color32 gridCol(0x80,0xa0,0xff,0xff);
			Color32 dynObjCol(0x90,0xa0,0x8f,0xff);

			int g,h;
			for(g=0; g<CDynamicObjectsContainer::GetNumGrids(); g++)
			{
				CDynamicObjectsGridCell * pGrid = CDynamicObjectsContainer::ms_DynamicObjectGrids[g];

				Vector3 vCellMin, vCellMax, vObjMin, vObjMax;

				pGrid->m_MinMaxOfGrid.GetAsFloats(vCellMin, vCellMax);
				pGrid->m_MinMaxOfObjectsInGrid.GetAsFloats(vObjMin, vObjMax);

				Vector3 vCellCorners[8] = {
					Vector3(vCellMin.x, vCellMin.y, vCellMin.z), Vector3(vCellMax.x, vCellMin.y, vCellMin.z), Vector3(vCellMax.x, vCellMax.y, vCellMin.z), Vector3(vCellMin.x, vCellMax.y, vCellMin.z),
						Vector3(vCellMin.x, vCellMin.y, vCellMax.z), Vector3(vCellMax.x, vCellMin.y, vCellMax.z), Vector3(vCellMax.x, vCellMax.y, vCellMax.z), Vector3(vCellMin.x, vCellMax.y, vCellMax.z)
				};
				Vector3 vObjCorners[8] = {
					Vector3(vObjMin.x, vObjMin.y, vObjMin.z), Vector3(vObjMax.x, vObjMin.y, vObjMin.z), Vector3(vObjMax.x, vObjMax.y, vObjMin.z), Vector3(vObjMin.x, vObjMax.y, vObjMin.z),
						Vector3(vObjMin.x, vObjMin.y, vObjMax.z), Vector3(vObjMax.x, vObjMin.y, vObjMax.z), Vector3(vObjMax.x, vObjMax.y, vObjMax.z), Vector3(vObjMin.x, vObjMax.y, vObjMax.z)
				};

				for(h=0; h<4; h++)
				{
					grcDebugDraw::Line(vCellCorners[h], vCellCorners[(h+1)%4], gridCol);
					grcDebugDraw::Line(vCellCorners[h+4], vCellCorners[((h+1)%4)+4], gridCol);
					grcDebugDraw::Line(vCellCorners[h], vCellCorners[h+4], gridCol);
				}
				for(h=0; h<4; h++)
				{
					grcDebugDraw::Line(vObjCorners[h], vObjCorners[(h+1)%4], dynObjCol);
					grcDebugDraw::Line(vObjCorners[h+4], vObjCorners[((h+1)%4)+4], dynObjCol);
					grcDebugDraw::Line(vObjCorners[h], vObjCorners[h+4], dynObjCol);
				}

				char tmp[64];
				sprintf(tmp, "gridcell %i (0x%p)", g, pGrid);

				grcDebugDraw::Text((vCellMin+vCellMax)*0.5f, gridCol, 0, 0, tmp);
			}
		}
	}


#if __HIERARCHICAL_NODES_ENABLED

	if(m_bVisualiseHierarchicalNodes)
	{
		g_fClosestHierNodeDist2 = FLT_MAX;
		g_pClosestHierNode = NULL;

		const float fHierVisDist = ms_fDebugVisNodesMaxDist;
		const u32 iOriginNodes = m_pNavNodesStore->GetMeshIndexFromPosition(vOrigin);
		s32 iNodesX, iNodesY;
		m_pNavNodesStore->MeshIndexToMeshCoords(iOriginNodes, iNodesX, iNodesY);

		for(int iHY=-1; iHY<=1; iHY++)
		{
			for(int iHX=-1; iHX<=1; iHX++)
			{
				const TNavMeshIndex iHierIndex = m_pNavNodesStore->MeshCoordsToMeshIndex(iNodesX+iHX, iNodesY+iHY);

				if(iHierIndex != NAVMESH_NODE_INDEX_NONE)
				{
					CHierarchicalNavData * pNodes = m_pNavNodesStore->GetMeshByIndex(iHierIndex);
					if(pNodes)
					{
						VisualiseHierarchicalNavData(pNodes, vOrigin, fHierVisDist);
					}
				}
			}
		}

		if(g_pClosestHierNode)
		{
			Vector3 vWldNodePos;
			Vector2 vScreenNodePos;
			const u32 iHierNavMesh = g_pClosestHierNode->GetNavMeshIndex();
			if(iHierNavMesh != NAVMESH_NAVMESH_INDEX_NONE && CPathServer::GetHierarchicalNavFromNavMeshIndex(iHierNavMesh))
			{
				const Vector3 & vMins = CPathServer::GetHierarchicalNavFromNavMeshIndex(iHierNavMesh)->GetMin();
				const Vector3 & vSize = CPathServer::GetHierarchicalNavFromNavMeshIndex(iHierNavMesh)->GetSize();

				g_pClosestHierNode->GetNodePosition(vWldNodePos, vMins, vSize);

				if(CPathServer::TransformWorldPosToScreenPos(vWldNodePos, vScreenNodePos))
				{
					char TempString[256];

					Color32 dbgCol = Color32(255, 255, 64, 255);
					const float fLineHeight = ((float)grcDebugDraw::GetScreenSpaceTextHeight()) / ((float)SCREEN_HEIGHT);

					sprintf(TempString, "%p", g_pClosestHierNode);
					grcDebugDraw::Text(ScreenPos(vScreenNodePos), dbgCol, TempString);
					vScreenNodePos.y += fLineHeight;

					sprintf(TempString, "PedDensity : %i", g_pClosestHierNode->GetPedDensity());
					grcDebugDraw::Text(vScreenNodePos, dbgCol, TempString);
					vScreenNodePos.y += fLineHeight;

					sprintf(TempString, "NodeRadius : %i", g_pClosestHierNode->GetNodeRadius());
					grcDebugDraw::Text(vScreenNodePos, dbgCol, TempString);
					vScreenNodePos.y += fLineHeight;

					sprintf(TempString, "NodeArea : %i", g_pClosestHierNode->GetAreaRepresented());
					grcDebugDraw::Text(vScreenNodePos, dbgCol, TempString);
					vScreenNodePos.y += fLineHeight;

					sprintf(TempString, "Flags : %u", g_pClosestHierNode->GetFlags());
					grcDebugDraw::Text(vScreenNodePos, dbgCol, TempString);
				}
			}
		}
	}
#endif // __HIERARCHICAL_NODES_ENABLED

	if(g_VisPolyUnderCamera)
	{
		camDebugDirector& debugDirector = camInterface::GetDebugDirector();
		if(debugDirector.IsFreeCamActive())
		{
			const Vector3& vPos = debugDirector.GetFreeCamFrame().GetPosition();

			CNavMesh * pNavMeshUnderfoot = CPathServer::GetNavMeshFromIndex( CPathServer::GetNavMeshIndexFromPosition(vPos, domain), domain );
			if(pNavMeshUnderfoot)
			{
				Vector3 vIsect;
				u32 iPolyIndexUnderfoot = pNavMeshUnderfoot->GetPolyBelowPoint(vPos, vIsect, 10.0f);
				if(iPolyIndexUnderfoot != NAVMESH_POLY_INDEX_NONE)
				{
					ASSERT_ONLY(TNavMeshPoly * pPolyUnderfoot = pNavMeshUnderfoot->GetPoly(iPolyIndexUnderfoot));
					Assert(pPolyUnderfoot->GetNavMeshIndex() == pNavMeshUnderfoot->GetIndexOfMesh());
					DrawPolyDebug(pNavMeshUnderfoot, iPolyIndexUnderfoot, Color_wheat, 0.15f, true);
				}

				bool bDbgTxt = false;
				if(bDbgTxt)
					DebugPolyText(pNavMeshUnderfoot, iPolyIndexUnderfoot);
			}
		}
	}

	//***************************************************************
	//
	//	Finally, let's draw all the paths which have been found
	//	and are still remaining in the store..
	//
	//***************************************************************

	if((m_iVisualiseNavMeshes != NavMeshVis_Off && m_iVisualiseNavMeshes != NavMeshVis_NavMeshTransparent)|| m_bVisualisePaths)
	{
		u32 iTotalNumReqs = MAX_NUM_PATH_REQUESTS + MAX_NUM_GRID_REQUESTS + MAX_NUM_LOS_REQUESTS;

		int c;
		s32 p;
		u32 iAlpha;
		float fAlpha, fAlphaMul;
		float fMinAlpha = 1.0f / ((float)iTotalNumReqs);

		struct TCtrlPts
		{
			Vector3 m_vCtrlPt2;
			Vector3 m_vCtrlPt3;
		};

		static TCtrlPts vSplineCtrlPts[MAX_NUM_PATH_POINTS];

		const Vector3 vRaiseAmt(0.0f, 0.0f, m_fVisNavMeshVertexShiftAmount+0.2f);

		for(c=0; c<MAX_NUM_PATH_REQUESTS; c++)
		{
			if(m_PathRequests[c].m_hHandleThisWas == 0)
				continue;

			if(ms_bOnlyDisplaySelectedRoute && CPathServer::m_iSelectedPathRequest != c)
				continue;

			fAlpha = 1.0f;
			iAlpha = (u32)(fAlpha * 255.0f);

			Color32 iCol1, iCol2;

			Color32 alphaMagenta = Color_magenta;
			alphaMagenta.SetAlpha(64);

			if(m_PathRequests[c].m_iNumPoints > 1)
			{
				// Record the spline ctrl points
				// Just so we can nicely visualise the paths
				for(p=0; p<m_PathRequests[c].m_iNumPoints; p++)
				{
					const int iSplineEnum = m_PathRequests[c].m_PathResultInfo.m_iPathSplineDistances[p];
					if(iSplineEnum >= 0)
					{
						const Vector3 & vLast = m_PathRequests[c].m_PathPoints[p-1];
						const Vector3 & vNext = m_PathRequests[c].m_PathPoints[p+1];
						const Vector3 & vCurrent = m_PathRequests[c].m_PathPoints[p];
						const Vector3 vLastToCurrent = vCurrent - vLast;
						const Vector3 vCurrentToNext = vNext - vCurrent;
						Vector3 vCtrlPt1, vCtrlPt4;
						CalcPathSplineCtrlPts(iSplineEnum, m_PathRequests[c].m_PathResultInfo.m_fPathCtrlPt2TVals[p], vLast, vNext, vLastToCurrent, vCurrentToNext, vCtrlPt1, vSplineCtrlPts[p].m_vCtrlPt2, vSplineCtrlPts[p].m_vCtrlPt3, vCtrlPt4);
					}
					else
					{
						vSplineCtrlPts[p].m_vCtrlPt2 = m_PathRequests[c].m_PathPoints[p];
						vSplineCtrlPts[p].m_vCtrlPt3 = m_PathRequests[c].m_PathPoints[p];
					}
				}

				// Draw node markers
				for(p=0; p<m_PathRequests[c].m_iNumPoints; p++)
				{
					Color32 iNodeCol = Color_MediumOrchid;
					if(m_PathRequests[c].m_WaypointFlags[p].m_iSpecialActionFlags)
						iNodeCol = Color_maroon;

					grcDebugDraw::Line(m_PathRequests[c].m_PathPoints[p], m_PathRequests[c].m_PathPoints[p] + Vector3(0,0,0.6f), iNodeCol, iNodeCol);
				}

				// Draw the straight line segment of each path.  We will do the spline curves (if any) in a sec..
				for(p=0; p<m_PathRequests[c].m_iNumPoints-1; p++)
				{
					grcDebugDraw::Line(vSplineCtrlPts[p].m_vCtrlPt3 + vRaiseAmt, vSplineCtrlPts[p+1].m_vCtrlPt2 + vRaiseAmt, Color_magenta, Color_magenta);
				}

				for(p=0; p<m_PathRequests[c].m_iNumPoints-1; p++)
				{
					// If we have a spline at this corner, then draw this as well
					const int iSplineEnum = m_PathRequests[c].m_PathResultInfo.m_iPathSplineDistances[p];
					if(iSplineEnum >= 0)
					{
						const Vector3 & vLast = m_PathRequests[c].m_PathPoints[p-1];
						const Vector3 & vNext = m_PathRequests[c].m_PathPoints[p+1];
						const Vector3 & vCurrent = m_PathRequests[c].m_PathPoints[p];
						const Vector3 vLastToCurrent = vCurrent - vLast;
						const Vector3 vCurrentToNext = vNext - vCurrent;

						Vector3 vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4;
						CalcPathSplineCtrlPts(iSplineEnum, m_PathRequests[c].m_PathResultInfo.m_fPathCtrlPt2TVals[p], vLast, vNext, vLastToCurrent, vCurrentToNext, vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4);

						// Draw the sharp un-splined corner in faint alpha col
						grcDebugDraw::Line(vCtrlPt2 + vRaiseAmt, vCurrent + vRaiseAmt, alphaMagenta, alphaMagenta);
						grcDebugDraw::Line(vCtrlPt3 + vRaiseAmt, vCurrent + vRaiseAmt, alphaMagenta, alphaMagenta);

						// Now draw the spline
						Vector3 vSplinePtA = vCtrlPt2;
						static const float fSplineStep = 1.0f / ((float)PATHSPLINE_NUM_TEST_SUBDIVISIONS);
						for(float u=fSplineStep; u<=1.0f; u+=fSplineStep)
						{
							const Vector3 vSplinePtB = PathSpline(vCtrlPt1, vCtrlPt2, vCtrlPt3, vCtrlPt4, u);
							grcDebugDraw::Line(vSplinePtA + vRaiseAmt, vSplinePtB + vRaiseAmt, Color_magenta, Color_magenta);
							vSplinePtA = vSplinePtB;
						}
					}
				}
			}
			for(int s=0; s<m_PathRequests[c].m_iNumInfluenceSpheres; s++)
			{
				Color32 iCol = (CPathServer::m_iSelectedPathRequest == c) ? Color_orange2 : Color_orange4;
				grcDebugDraw::Sphere(m_PathRequests[c].m_InfluenceSpheres[s].GetOrigin(), m_PathRequests[c].m_InfluenceSpheres[s].GetRadius(), iCol, false);
			}

			if(ms_bShowSearchExtents)
			{
				TShortMinMax minMax;
				Vector3 vSearchMin, vSearchMax;
				m_PathServerThread.CalculateSearchExtents(&m_PathRequests[c], minMax, vSearchMin, vSearchMax);

				grcDebugDraw::BoxAxisAligned(RCC_VEC3V(vSearchMin), RCC_VEC3V(vSearchMax), Color_yellow3, false, 1);
			}
		}

		for(c=0; c<MAX_NUM_LOS_REQUESTS; c++)
		{
			if(m_LineOfSightRequests[c].m_hHandleThisWas == 0)
				continue;

			u32 iHandleDiff = m_iNextHandle - m_LineOfSightRequests[c].m_hHandleThisWas;
			if(iHandleDiff < iTotalNumReqs)
			{
				fAlphaMul = ((float)iHandleDiff) / ((float)iTotalNumReqs);
				fAlpha = 1.0f - fAlphaMul;
			}
			else
			{
				fAlpha = fMinAlpha;
			}

			if(fAlpha < fMinAlpha) fAlpha = fMinAlpha;
			else if(fAlpha > 1.0f) fAlpha = 1.0f;

			iAlpha = (u32)(fAlpha * 255.0f);

			if(m_LineOfSightRequests[c].m_iNumPts > 1)
			{
				for(int l=0; l<m_LineOfSightRequests[c].m_iNumPts-1; l++)
				{
					Color32 col = m_LineOfSightRequests[c].m_bLosResults[l] ? Color_green : Color_red;
					col.SetAlpha(iAlpha);

					grcDebugDraw::Line(
						m_LineOfSightRequests[c].m_vPoints[l] + vRaiseAmt,
						m_LineOfSightRequests[c].m_vPoints[l+1] + vRaiseAmt,
						col
					);
				}
			}
		}

		for(c=0; c<MAX_NUM_GRID_REQUESTS; c++)
		{
			if(m_GridRequests[c].m_hHandleThisWas == 0)
				continue;

			u32 iHandleDiff = m_iNextHandle - m_GridRequests[c].m_hHandleThisWas;
			if(iHandleDiff < iTotalNumReqs)
			{
				fAlphaMul = ((float)iHandleDiff) / ((float)iTotalNumReqs);
				fAlpha = 1.0f - fAlphaMul;
			}
			else
			{
				fAlpha = fMinAlpha;
			}

			if(fAlpha < fMinAlpha) fAlpha = fMinAlpha;
			else if(fAlpha > 1.0f) fAlpha = 1.0f;

			iAlpha = (u32)(fAlpha * 128.0f);

			float fHalfSize = ((float)m_GridRequests[c].m_WalkRndObjGrid.m_iCentreCell) - 1.0f;

			Vector3 vGridOrigin = m_GridRequests[c].m_WalkRndObjGrid.m_vOrigin;

			// Don't display if it is too far away
			float fDistSqr = (vGridOrigin - vOrigin).Mag2();
			if(fDistSqr < 100.0f * 100.0f)
			{
				float fResolution = m_GridRequests[c].m_WalkRndObjGrid.m_fResolution;

				// Get the min/max bounds of the grid area to be sampled
				Vector3 vGridMin = vGridOrigin;
				vGridMin.x -= (fResolution * fHalfSize);
				vGridMin.y -= (fResolution * fHalfSize);

				Vector3 vGridMax = vGridOrigin;
				vGridMax.x += (fResolution * fHalfSize);
				vGridMax.y += (fResolution * fHalfSize);

				float x,y;
				Color32 iFlagCol;

				for(y=vGridMin.y; y<=vGridMax.y; y+=fResolution)
				{
					for(x=vGridMin.x; x<=vGridMax.x; x+=fResolution)
					{
						int ix = (int) ((x - vGridOrigin.x) / fResolution);
						int iy = (int) ((y - vGridOrigin.y) / fResolution);

						if(m_GridRequests[c].m_WalkRndObjGrid.IsWalkable(ix, iy))
						{
							iFlagCol.Set(0,0xff,0,iAlpha);
						}
						else
						{
							iFlagCol.Set(0xff,0,0,iAlpha);
						}

						grcDebugDraw::Line(
							Vector3(x,y,vGridOrigin.z),
							Vector3(x,y,vGridOrigin.z + 0.25f),
							iFlagCol);
					}
				}
			}
		}

		VisualiseTestPathEndPoints();
	}

	RenderMiscThings();

	if(g_bTestBlockingObjectEnabled)
	{
		Matrix34 mat;
		mat.Identity();
		mat.RotateZ(g_fTestBlockingObjectRot * DtoR);
		mat.d = g_vTestBlockingObjectPos;
		Vector3 vMin = g_vTestBlockingObjectSize * -0.5f;
		Vector3 vMax = g_vTestBlockingObjectSize * 0.5f;

		static bool bFlash = false;

		grcDebugDraw::BoxOriented(VECTOR3_TO_VEC3V(vMin), VECTOR3_TO_VEC3V(vMax), RCC_MAT34V(mat), bFlash ? Color_yellow : Color_navy, false);
	}

	//******************************************************************************
	//	Ok, also render the path start & end positions for all paths, plus the
	//	adjusted positions outside any dynamic objects.
	//******************************************************************************

	if(m_iVisualiseNavMeshes != NavMeshVis_Off && m_iVisualiseNavMeshes != NavMeshVis_NavMeshTransparent)
	{
		Color32 iUnadjustedPosCol, iAdjustedPosCol, iLinkCol, iFontCol;
		iUnadjustedPosCol.Set(0xa0,0xa0,0xa0,0xff);
		iAdjustedPosCol.Set(0xff,0xff,0xff,0xff);
		iLinkCol.Set(0xff,0xff,0xff,0x80);
		iFontCol.Set(0xff,0,0xff,0xff);
		s32 i;
		char szPathTxt[64];

		for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
		{
			CPathRequest * pPathReq = &m_PathRequests[i];
			if(pPathReq->m_hHandleThisWas != 0)
			{
				if(ms_bOnlyDisplaySelectedRoute && CPathServer::m_iSelectedPathRequest != i)
					continue;

				Vector3 vUnadjustedPathStart = pPathReq->m_vUnadjustedStartPoint;
				Vector3 vUnadjustedPathEnd = pPathReq->m_vUnadjustedEndPoint;
				Vector3 vPathStart = pPathReq->m_vPathStart;
				Vector3 vPathEnd = pPathReq->m_vPathEnd;

				// The UNADJUSTED positions (ie. those initially passed into the pathfinder)
				grcDebugDraw::Line(vUnadjustedPathStart - Vector3(1.0f, 0, 0), vUnadjustedPathStart + Vector3(1.0f, 0, 0), iUnadjustedPosCol);
				grcDebugDraw::Line(vUnadjustedPathStart - Vector3(0, 1.0f, 0), vUnadjustedPathStart + Vector3(0, 1.0f, 0), iUnadjustedPosCol);
				grcDebugDraw::Line(vUnadjustedPathEnd - Vector3(1.0f, 0, 0), vUnadjustedPathEnd + Vector3(1.0f, 0, 0), iUnadjustedPosCol);
				grcDebugDraw::Line(vUnadjustedPathEnd - Vector3(0, 1.0f, 0), vUnadjustedPathEnd + Vector3(0, 1.0f, 0), iUnadjustedPosCol);

				// The ADJUSTED positions (ie. those which were used to try to find the path)
				grcDebugDraw::Line(vPathStart - Vector3(1.0f, 0, 0), vPathStart + Vector3(1.0f, 0, 0), iAdjustedPosCol);
				grcDebugDraw::Line(vPathStart - Vector3(0, 1.0f, 0), vPathStart + Vector3(0, 1.0f, 0), iAdjustedPosCol);
				grcDebugDraw::Line(vPathEnd - Vector3(1.0f, 0, 0), vPathEnd + Vector3(1.0f, 0, 0), iAdjustedPosCol);
				grcDebugDraw::Line(vPathEnd - Vector3(0, 1.0f, 0), vPathEnd + Vector3(0, 1.0f, 0), iAdjustedPosCol);

				// Lines between the unadjusted & adjusted positions
				grcDebugDraw::Line(vPathStart, vUnadjustedPathStart, iAdjustedPosCol, iUnadjustedPosCol);
				grcDebugDraw::Line(vPathEnd, vUnadjustedPathEnd, iAdjustedPosCol, iUnadjustedPosCol);

				// The reference vector
				if(pPathReq->m_vReferenceVector.Mag2() > SMALL_FLOAT)
				{
					grcDebugDraw::Arrow(VECTOR3_TO_VEC3V(vPathStart + Vector3(0,0,0.5f)), VECTOR3_TO_VEC3V(vPathStart + Vector3(0,0,0.5f) + pPathReq->m_vReferenceVector), 0.2f, Color_grey32);
				}

				Vector2 vScreenStart, vScreenEnd;

				if(CPathServer::TransformWorldPosToScreenPos(vPathStart, vScreenStart))
				{
					sprintf(szPathTxt, "start (0x%x)", pPathReq->m_hHandleThisWas);
					grcDebugDraw::Text(vScreenStart, Color_white, szPathTxt);
				}
				if(CPathServer::TransformWorldPosToScreenPos(vPathEnd, vScreenEnd))
				{
					sprintf(szPathTxt, "end (0x%x)", pPathReq->m_hHandleThisWas);
					grcDebugDraw::Text(vScreenEnd, Color_white, szPathTxt);
				}

				// Draw a faint link between the adjusted start & endpoints
				grcDebugDraw::Line(vPathStart, vPathEnd, iLinkCol);
			}
		}
	}

#endif	// GTA_ENGINE

	//*******************************************
	//
	//	Visualise PathServer info ?
	//
	//*******************************************

	if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_PathRequests)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)(s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualisePathRequests(ypos, domain);
	}
	if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_LineOfSightRequests)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualiseLineOfSightRequests(ypos, domain);
	}
	if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_AudioRequests)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualiseAudioPropertiesRequests(ypos, domain);
	}
	if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_FloodFillRequests)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualiseFloodFillRequests(ypos, domain);
	}
	if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_ClearAreaRequests)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualiseClearAreaRequests(ypos, domain);
	}
	if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_ClosestPositionRequests)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualiseClosestPositionRequests(ypos, domain);
	}
	if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_Text)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualiseText(ypos, domain);
	}
	else if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_MemoryUsage)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		VisualiseMemoryUsage(ypos, domain);
	}
	else if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_CurrentNavMesh)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);

		s32 iCurrentMesh = pStore->GetMeshIndexFromPosition(vOrigin);
		CNavMesh * pCurrentNavMesh = pStore->GetMeshByIndex(iCurrentMesh);
		if(pCurrentNavMesh)
		{
			ypos = VisualiseCurrentNavMeshDetails(ypos, pCurrentNavMesh);
		}

		ypos += grcDebugDraw::GetScreenSpaceTextHeight() * 2;

		char tmp[256];

		const atArray<s32> & loadedMeshes = pStore->GetLoadedMeshes();
		for(s32 s=0; s<loadedMeshes.GetCount(); s++)
		{
			s32 iMesh = loadedMeshes[s];
			CNavMesh * pNavMesh = pStore->GetMeshByIndex(iMesh);
			if(pNavMesh)
			{
				int iMeshX, iMeshY;
				pStore->MeshIndexToMeshCoords(pNavMesh->GetIndexOfMesh(), iMeshX, iMeshY);
				iMeshX *= CPathServerExtents::m_iNumSectorsPerNavMesh;
				iMeshY *= CPathServerExtents::m_iNumSectorsPerNavMesh;

				sprintf(tmp, "[%i][%i] - polys=%i", iMeshX, iMeshY, pNavMesh->GetNumPolys());
				grcDebugDraw::Text( ScreenPos(g_vPathserverDebugTopLeft.x+40,(float)ypos), Color_white, tmp);
				ypos += grcDebugDraw::GetScreenSpaceTextHeight();
			}
		}
	}
	else if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_PolygonInfo)
	{
		int ypos = VisualisePathServerCommon(domain, (s32)g_vPathserverDebugTopLeft.y);
		ypos = Max(ypos, 64);
		s32 iCurrentMesh = pStore->GetMeshIndexFromPosition(vOrigin);
		CNavMesh * pCurrentNavMesh = pStore->GetMeshByIndex(iCurrentMesh);
		if(pCurrentNavMesh)
		{
			VisualisePolygonInfo(ypos, pCurrentNavMesh, vOrigin, domain);
		}
	}

	//*************************************************************************************
	//	Allow the toggling of navmesh display modes via the "I" key.  Also print up a
	//	little message to say which display mode is currently active.
#ifdef GTA_ENGINE
	static u32 iLastTimeNavMeshModeToggled = 0x7FFFFFFF;
	// We don't want the I key to toggle between ALL the modes, there are too many of them
	const int iNumQuickModes = 8;
	static const int iVisModes[iNumQuickModes+1] =
	{
		NavMeshVis_Off,
		NavMeshVis_NavMeshTransparent,
		NavMeshVis_NavMeshWireframe,
		NavMeshVis_NavMeshSolid,
		NavMeshVis_Pavements,
		NavMeshVis_PedDensity,
		NavMeshVis_AudioProperties,
		NavMeshVis_PavementCheckingMode,
		-1
	};
	static int iVisModeIndex = 0;

	if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_I, KEYBOARD_MODE_DEBUG, "Increment navmesh display")) 
	{
		iVisModeIndex++;
		if(iVisModes[iVisModeIndex] == -1)
			iVisModeIndex = 0;
		iLastTimeNavMeshModeToggled = fwTimer::GetTimeInMilliseconds();
		m_iVisualiseNavMeshes = iVisModes[iVisModeIndex];
	}
	else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_I, KEYBOARD_MODE_DEBUG_SHIFT, "Decrement navmesh display"))
	{
		iVisModeIndex--;
		if(iVisModeIndex < 0)
			iVisModeIndex += iNumQuickModes;
		iLastTimeNavMeshModeToggled = fwTimer::GetTimeInMilliseconds();
		m_iVisualiseNavMeshes = iVisModes[iVisModeIndex];
	}
	else if(CControlMgr::GetKeyboard().GetKeyJustDown(KEY_I, KEYBOARD_MODE_DEBUG_CTRL, "Toggle navmesh display off"))
	{
		iVisModeIndex = 0;
		iLastTimeNavMeshModeToggled = fwTimer::GetTimeInMilliseconds();
		m_iVisualiseNavMeshes = iVisModes[iVisModeIndex];
	}

	if(fwTimer::GetTimeInMilliseconds() - iLastTimeNavMeshModeToggled < 4000)
	{
		//if(grcViewport::GetCurrent())
		{
			char text[256];
			sprintf(text, "NavMesh Display : %s", ms_pNavMeshVisModeTxt[CPathServer::m_iVisualiseNavMeshes]);
			Vector2 vTextRenderPos(0.0f, (float)(SCREEN_HEIGHT - 32));
			grcDebugDraw::Text(ScreenPos(vTextRenderPos), Color_white, text);
		}
	}

	m_iNumImmediateModeLosCallsThisFrame = 0;
	m_iNumImmediateModeWanderPathsThisFrame = 0;
	m_iNumImmediateModeFleePathsThisFrame = 0;
	m_iNumImmediateModeSeekPathsThisFrame = 0;
	m_fTimeSpentOnImmediateModeCallsThisFrame = 0.0f;

#endif // GTA_ENGINE
}
#endif

#if !__FINAL
#ifdef GTA_ENGINE

void
CPathServer::VisualiseNavMesh(CNavMesh * pNavMesh, const Vector3 & vOrigin, const float fHeightAboveMesh, bool bIsMainNavMeshUnderFoot, aiNavDomain domain)
{
	u32 t;
	u32 v;
	Vector3 v1, v2, v3, vCenter;

	static Color32 polyCol(0x80,0x80,0xff,0);
	static Color32 pavementCol(0xb0,0x10,0xb0,0x80);
	static Color32 roadCol(0xa0,0x30,0x30,0x80);
	static Color32 trainTracksCol(0xa0,0xa0,0x30,0x80);
	static Color32 polyColMarked(0xff,0,0xff,0xff);

	static Color32 selCol(0xff,0,0,0xff);
	static Color32 adjCol(0xff,0xff,0xff,0xff);
	static Color32 openCol(0,0xff,0,0xff);
	static Color32 closedCol(0xff,0,0,0xff);

	static Color32 coverEdgeCol(0xff,0xff,0,0);

	static Color32 coverPointBaseCol(0xff,0,0,0);
	static Color32 coverPointTopCol(0xff,0xff,0xff,0);

	static Color32 shelterCol(0x4f, 0x78, 0x20, 0);

	static Color32 specialLinksCol(0xaf, 0x98, 0x60, 0);
	static Color32 tooSteepCol(0xff, 0x18, 0x18, 0);

	static Color32 waterCol(0x20, 0x20, 0xff, 0);
	static Color32 waterColShallow(0xa0, 0xa0, 0xff, 0);
	static Color32 spaceAboveCol1(0x30, 0x30, 0x90, 0xff);
	static Color32 spaceAboveCol2(0x50, 0x50, 0xf0, 0xff);
	static Color32 carNodeCol(0x90, 0x90, 0x30, 0);
	static Color32 interiorCol(0xa0, 0xa0, 0, 0);
	static Color32 isolatedCol(0x60, 0, 0, 0);
	static Color32 networkSpawnCandidateCol(0, 0x60, 0xa0, 0xff);
	static Color32 noWanderCol(0x90, 0x10, 0x0, 0);

	float fMaxDist = ms_fDebugVisPolysMaxDist;
	float fNormalHeight = m_fVisNavMeshVertexShiftAmount;
	float fCoverHeight = fNormalHeight + 0.08f;
	float fHeightExtra = fNormalHeight; //0.1f;

	if(pNavMesh)
	{
		if(m_bVisualiseQuadTrees)
		{
			reinterpret_cast<CNavMeshGTA*>(pNavMesh)->VisualiseQuadTree(m_bVisualiseAllCoverPoints);
		}

		if(m_bDebugShowSpecialLinks)
		{
			VisualiseSpecialLinks(pNavMesh, domain);
		}

		if(m_iVisualiseNavMeshes != NavMeshVis_Off)
		{
			// Use a colour based upon the NavMesh's address.
			// It'll be important to be able to distinguish clearly between adjacent meshes.
//			u32 iRandVal = static_cast<u32>(reinterpret_cast<size_t>(pNavMesh));
//			iRandVal ^= (pNavMesh->GetIndexOfMesh() * pNavMesh->GetIndexOfMesh());

			Color32 meshCol = ((pNavMesh->GetFlags()&NAVMESH_DLC)==0) ? Color_SlateGray1 : Color_LimeGreen;
			
//			meshCol.SetRed((iRandVal & 0xFF000000)>>24);
//			meshCol.SetGreen((iRandVal & 0x00FF0000)>>16);
//			meshCol.SetBlue((iRandVal & 0x0000FF00)>>8);
//			meshCol.SetAlpha(iRandVal & 0x000000FF);

			Color32 col;

			for(t=0; t<pNavMesh->GetNumPolys(); t++)
			{
				TNavMeshPoly * pPoly = pNavMesh->GetPoly(t);

				if(pPoly->GetReplacedByTessellation())
					continue;

				if(pPoly->GetNavMeshIndex() == NAVMESH_INDEX_TESSELLATION)
				{
					if(!pPoly->GetIsTessellatedFragment())
						continue;
				}

				float fRecip = 1.0f / ((float)pPoly->GetNumVertices() );
				vCenter = Vector3(0,0,0);

				for(v=0; v<pPoly->GetNumVertices(); v++)
				{
					pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), v1 );
					vCenter += v1;
				}
				vCenter *= fRecip;

				float fDistSqr = (vCenter - vOrigin).Mag2();
				if(fDistSqr < CPathServer::ms_fDebugVisPolysMaxDist*CPathServer::ms_fDebugVisPolysMaxDist)
				{
					float fRadius = 10.0f;
					if((m_bDebugShowPolyVolumesAbove || m_bDebugShowPolyVolumesBelow)
						&& pNavMesh->GetPoly(t)->GetIsWater())
						fRadius += 100.0f;

					if(!camInterface::IsSphereVisibleInGameViewport(vCenter, fRadius))
						continue;

					col = meshCol;

					if(m_iVisualiseNavMeshes != NavMeshVis_NavMeshTransparent)	// Skip the OPEN/CLOSED draw in Transparent mode, to keep it as a clean view of the static data.
					{
						if(pNavMesh->GetPoly(t)->TestFlags(NAVMESHPOLY_OPEN))
						{
							col = openCol;
						}
						else if(pNavMesh->GetPoly(t)->TestFlags(NAVMESHPOLY_CLOSED))
						{
							col = closedCol;
						}

						if(pPoly->GetNavMeshIndex()==NAVMESH_INDEX_TESSELLATION)
							col = Lerp(0.75f, col, Color_black);
					}
					else
					{
						if(m_iVisualisePathServerInfo == CPathServer::PathServerVis_PolygonInfo)
						{
							if(pPoly->GetLiesAlongEdgeOfMesh())
							{
								col = Lerp(0.5f, col, Color_black);
							}
						}
					}

					col.SetAlpha(0);

					float fAlpha;
					if(m_iVisualiseNavMeshes == NavMeshVis_NavMeshTransparent)
					{
						// Not much use in fading the lines in this mode, at least not unless we also fade the polygons.
						fAlpha = 1.0f;
					}
					else
					{
						// Fade out nicely..
						float fDist = rage::Sqrtf(fDistSqr);
						fAlpha = fDist / fMaxDist;
						if(fAlpha < 0.0f) fAlpha = 0.0f;
						else if(fAlpha > 0.999f) fAlpha = 0.999f;
						fAlpha = 1.0f - (fAlpha*fAlpha);	// inv sqr falloff
					}

					u32 iAlpha = (u32)(fAlpha * 255.0f);

					col.SetAlpha(iAlpha);

					if(ms_bMarkVisitedPolys && pPoly->GetDebugMarked())
					{
						polyColMarked.SetAlpha(iAlpha);
						DrawPolyDebug(pNavMesh, t, polyColMarked, fNormalHeight, false);
					}

					if(m_iVisualiseNavMeshes==NavMeshVis_PavementCheckingMode)
					{
						static u32 iLastFlashTime = 0;
						static bool bFlashToggle = false;

						if(iLastFlashTime + 250 < fwTimer::GetTimeInMilliseconds())
						{
							iLastFlashTime = fwTimer::GetTimeInMilliseconds();
							bFlashToggle = !bFlashToggle;
						}

						// Mark in flashing red if there is ped-density here, but NO pavement.
						// This can cause peds to wander freely around the map & into the road.
						// This is generally ok only for parks, wasteground, etc.
						if(pPoly->GetPedDensity()>0 && !pPoly->GetIsPavement())
						{
							if(bFlashToggle)
							{
								DrawPolyDebug(pNavMesh, t, Color_red, fNormalHeight + 0.1f, false);
							}
						}
						else
						{
							// Mark areas of pavement
							if(pPoly->GetIsPavement())
							{
								DrawPolyDebug(pNavMesh, t, Color_magenta1, fNormalHeight + 0.1f, false);
							}
						}

						continue;
					}

					const bool bDrawAllPolys = true;
						//m_iVisualiseNavMeshes != NavMeshVis_WaterSurface;

					if(bDrawAllPolys)
					{
						if(m_iVisualiseNavMeshes!=NavMeshVis_NavMeshSolid && m_iVisualiseNavMeshes!=NavMeshVis_NavMeshTransparent)
						{
							DrawPolyDebug(pNavMesh, t, col, fNormalHeight, true);
						}
						else
						{
							Color32 outlineCol(0,0,0,(s32)iAlpha);
							DrawPolyDebug(pNavMesh, t, outlineCol, fNormalHeight, true);
							DrawPolyDebug(pNavMesh, t, col, fNormalHeight, false, m_iVisualiseNavMeshes == NavMeshVis_NavMeshTransparent ? 128 : 255);
						}
					}

					int lastv = pPoly->GetNumVertices()-1;
					for(v=0; v<pPoly->GetNumVertices(); v++)
					{
						const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly( (u32)pPoly->GetFirstVertexIndex() + lastv );

						Vector3 vEdgeVec, vLastEdgeVec;
						pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, lastv), vLastEdgeVec );
						pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, v), vEdgeVec );

						vLastEdgeVec.z += fCoverHeight;
						vEdgeVec.z += fCoverHeight;

						// If this poly has an edge which provides cover, then highlight it
						if(adjPoly.GetEdgeProvidesCover())
						{
							Color32 iTmpCol = coverEdgeCol;
							iTmpCol.SetAlpha(iAlpha);
							grcDebugDraw::Line(vLastEdgeVec, vEdgeVec, iTmpCol);
						}

						// If this poly has a non-standard adjacency to the next one,
						// then draw a visual indication of it..
						if(adjPoly.GetAdjacencyType() == ADJACENCY_TYPE_NORMAL)
						{
							lastv = v;
							continue;
						}

						CNavMesh * pAdjNavMesh = GetNavMeshFromIndex(adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes()), domain);
						if(!pAdjNavMesh)
						{
							lastv = v;
							continue;
						}

						Vector3 vHalfWayAlongThisEdge = (vLastEdgeVec + vEdgeVec) * 0.5f;

						Vector3 vEdge = vEdgeVec - vLastEdgeVec;
						vEdge.z = 0;
						vEdge.Normalize();

						Vector3 vEdgeNormal = CrossProduct(vEdge, Vector3(0,0,1.0f));
						vEdgeNormal.Normalize();

						Vector3 vAdjPolyCentroid;
						pAdjNavMesh->GetPolyCentroid(adjPoly.GetPolyIndex(), vAdjPolyCentroid);

						// Visualise the adjacency link into the next poly, if a special type
						VisualiseAdjacency(vHalfWayAlongThisEdge, vAdjPolyCentroid, (eNavMeshPolyAdjacencyType)adjPoly.GetAdjacencyType(), adjPoly.GetHighDropOverEdge(), adjPoly.GetAdjacencyDisabled(), iAlpha);

						lastv = v;
					}

					if(m_iVisualiseNavMeshes == NavMeshVis_Pavements)
					{
						if(pPoly->TestFlags(NAVMESHPOLY_IS_PAVEMENT))
						{
							DrawPolyDebug(pNavMesh, t, pavementCol, fHeightExtra, false);
						}
					}

					else if(m_iVisualiseNavMeshes == NavMeshVis_Roads)
					{
						if(pPoly->GetIsRoad())
						{
							DrawPolyDebug(pNavMesh, t, pavementCol, fHeightExtra, false);
						}
					}

					else if(m_iVisualiseNavMeshes == NavMeshVis_TrainTracks)
					{
						if(pPoly->GetIsTrainTrack())
						{
							DrawPolyDebug(pNavMesh, t, trainTracksCol, fHeightExtra, false);
						}
					}

					else if(m_iVisualiseNavMeshes == NavMeshVis_PedDensity)
					{
						float fDensity = (float)pPoly->GetPedDensity();
						if(fDensity != 0.0f && CPedPopulation::GetAdjustPedSpawnDensity()!=0)
						{
							if( vCenter.IsGreaterOrEqualThan(CPedPopulation::GetAdjustPedSpawnDensityMin()) &&
								CPedPopulation::GetAdjustPedSpawnDensityMax().IsGreaterThan(vCenter) )
							{
								fDensity += CPedPopulation::GetAdjustPedSpawnDensity();
								fDensity = Clamp(fDensity, 0.0f, (float)MAX_PED_DENSITY);
							}
						}

						Color32 iDensityCol;
						float s = fDensity / (float)MAX_PED_DENSITY;
						int c = (int) (s * 255.0f);
						iDensityCol = Color32(c, 0, 0, 255);
						iDensityCol.SetAlpha(iAlpha);
						DrawPolyDebug(pNavMesh, t, iDensityCol, fHeightExtra, false);

						static const bool bPrintDensity=true;
						if(bPrintDensity)
						{
							// Draw the enumeration value
							static char txt[8];
							Vector2 vScreenPos;
							if(CPathServer::TransformWorldPosToScreenPos(vCenter, vScreenPos))
							{
								s32 iDensity = (s32)fDensity;
								if(CPedPopulation::GetAdjustPedSpawnDensity() > 0)
									sprintf(txt, "%u (%u +%i)", iDensity, pPoly->GetPedDensity(), CPedPopulation::GetAdjustPedSpawnDensity());
								else if(CPedPopulation::GetAdjustPedSpawnDensity() < 0)
									sprintf(txt, "%u (%u %i)", iDensity, pPoly->GetPedDensity(), CPedPopulation::GetAdjustPedSpawnDensity());
								else
									sprintf(txt, "%u", pPoly->GetPedDensity());

								grcDebugDraw::Text(vScreenPos, Color_white, txt);
							}
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_Disabled)
					{
						if(pPoly->GetIsDisabled())
						{
							DrawPolyDebug(pNavMesh, t, Color_black, fHeightExtra, false);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_Shelter)
					{
						if(pPoly->TestFlags(NAVMESHPOLY_IN_SHELTER))
						{
							DrawPolyDebug(pNavMesh, t, shelterCol, fHeightExtra, false);
						}
					}

					else if(m_iVisualiseNavMeshes == NavMeshVis_HasSpecialLinks)
					{
						if(pPoly->GetNumSpecialLinks()!=0)
						{
							DrawPolyDebug(pNavMesh, t, specialLinksCol, fHeightExtra, false);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_TooSteep)
					{
						if(pPoly->TestFlags(NAVMESHPOLY_TOO_STEEP_TO_WALK_ON))
						{
							DrawPolyDebug(pNavMesh, t, tooSteepCol, fHeightExtra, false);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_WaterSurface)
					{
						if(pPoly->GetIsWater())
						{
							if(pPoly->GetIsShallow())
							{
								DrawPolyDebug(pNavMesh, t, waterColShallow, fHeightExtra, false);
							}
							else
							{
								DrawPolyDebug(pNavMesh, t, waterCol, fHeightExtra, false);
							}
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_AudioProperties)
					{
						// Draw the enumeration value
						u32 iAudioProperty = pPoly->GetAudioProperties();
						static char txt[8];
						Vector2 vScreenPos;
						if(CPathServer::TransformWorldPosToScreenPos(vCenter, vScreenPos))
						{
							sprintf(txt, "%u", iAudioProperty);
							grcDebugDraw::Text(vScreenPos, Color_white, txt);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_CoverDirections)
					{
						// Draw the bitfield
						u32 iCoverDirections = pPoly->GetCoverDirections();
						static char txt[8];
						Vector2 vScreenPos;
						if(CPathServer::TransformWorldPosToScreenPos(vCenter, vScreenPos))
						{
							sprintf(txt, "%u", iCoverDirections);
							grcDebugDraw::Text(vScreenPos, Color_white, txt);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_CarNodes)
					{
						if(pPoly->GetIsNearCarNode())
						{
							DrawPolyDebug(pNavMesh, t, carNodeCol, fHeightExtra, false);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_Interior)
					{
						if(pPoly->GetIsInterior())
						{
							DrawPolyDebug(pNavMesh, t, interiorCol, fHeightExtra, false);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_Isolated)
					{
						if(pPoly->GetIsIsolated())
						{
							DrawPolyDebug(pNavMesh, t, isolatedCol, fHeightExtra, false);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_NetworkSpawnCandidates)
					{
						if(pPoly->GetIsNetworkSpawnCandidate())
						{
							DrawPolyDebug(pNavMesh, t, networkSpawnCandidateCol, fHeightExtra, false);
						}
					}
					else if(m_iVisualiseNavMeshes == NavMeshVis_NoWander)
					{
						if(pPoly->GetIsSwitchedOffForPeds())
						{
							DrawPolyDebug(pNavMesh, t, noWanderCol, fHeightExtra, false);
						}
					}

					/*
					if(pNavMesh->GetPolyVolumeInfos())
					{
						if((m_bDebugShowPolyVolumesAbove && pPoly->GetIsUnderwater()) ||
							(m_bDebugShowPolyVolumesBelow && (pPoly->GetIsWater()&&!pPoly->GetIsUnderwater())))
						{
							Assert(pNavMesh->GetFlags() & NAVMESH_HAS_POLY_VOLUME_SECTION);
							Assert(pNavMesh->GetPolyVolumeInfos());

							spaceAboveCol1.SetAlpha(iAlpha);
							spaceAboveCol2.SetAlpha(iAlpha);

							const s16 iFreeSpaceTopZ = pNavMesh->GetPolyVolumeInfos()[t].GetFreeSpaceTopZ();
							if(iFreeSpaceTopZ != TPolyVolumeInfo::ms_iFreeSpaceNone)
							{
								DrawPolyVolumeDebug(pNavMesh, t, spaceAboveCol1, spaceAboveCol2, (float)iFreeSpaceTopZ);
							}
						}
					}
					*/
				}
			}

			if(bIsMainNavMeshUnderFoot && m_iVisualiseNavMeshes != NavMeshVis_NavMeshTransparent)
			{
				CNavMesh * pNavMeshUnderfoot = pNavMesh;
				TNavMeshPoly * pPoly;

				// The poly under foot
				Vector3 vTmp;
				u32 iPoly = pNavMeshUnderfoot->GetPolyBelowPoint(vOrigin, vTmp, fHeightAboveMesh);
				// Or maybe a little way above ?
				if(iPoly == NAVMESH_POLY_INDEX_NONE)
				{
					// If we didn't find one exactly below, then try again with a volume test
					Vector3 vNewPos;
					TNavMeshAndPoly navMeshAndPoly;
					m_PathServerThread.FindApproxNavMeshPolyForRouteEndPoints(vOrigin, 10.0f, navMeshAndPoly, false, NAVMESH_NAVMESH_INDEX_NONE, domain);
					
					pNavMeshUnderfoot = GetNavMeshFromIndex(navMeshAndPoly.m_iNavMeshIndex, domain);
					iPoly = navMeshAndPoly.m_iPolyIndex;
				}

				if(iPoly != NAVMESH_POLY_INDEX_NONE)
				{
					pPoly = pNavMeshUnderfoot->GetPoly(iPoly);

					if(pPoly->GetReplacedByTessellation())
					{
						pNavMeshUnderfoot = m_pTessellationNavMesh;
						iPoly = pNavMeshUnderfoot->GetPolyBelowPoint(vOrigin, vTmp, fHeightAboveMesh);

						if(iPoly != NAVMESH_POLY_INDEX_NONE)
						{
							pPoly = pNavMeshUnderfoot->GetPoly(iPoly);
						}
						else
						{
							Vector3 vNewPos;
							TNavMeshAndPoly navMeshAndPoly;
							m_PathServerThread.FindApproxNavMeshPolyForRouteEndPoints(vOrigin, 10.0f, navMeshAndPoly, true, NAVMESH_NAVMESH_INDEX_NONE, domain);

							pNavMeshUnderfoot = GetNavMeshFromIndex(navMeshAndPoly.m_iNavMeshIndex, domain);
							iPoly = navMeshAndPoly.m_iPolyIndex;
						}

						pPoly = pNavMeshUnderfoot->GetPoly(iPoly);
					}

					if(pPoly)
					{
						if(CPathServer::m_bDebugPolyUnderfoot)
						{
							DebugPolyText(pNavMeshUnderfoot, iPoly);
							CPathServer::m_bDebugPolyUnderfoot = false;
						}

						// The adjacent polys
						for(u32 a=0; a<pPoly->GetNumVertices(); a++)
						{
							const TAdjPoly & adjPoly = pNavMeshUnderfoot->GetAdjacentPoly((u32) pPoly->GetFirstVertexIndex() + a);

							CNavMesh * pAdjNavMesh = GetNavMeshFromIndex(adjPoly.GetNavMeshIndex(pNavMeshUnderfoot->GetAdjacentMeshes()), domain);

							if(pAdjNavMesh)
							{
								DrawPolyDebug(pAdjNavMesh, adjPoly.GetPolyIndex(), adjCol, fHeightExtra + 0.1f, true);
							}
						}

						DrawPolyDebug(pNavMeshUnderfoot, iPoly, selCol, fHeightExtra + 0.1f, true);
					}
				}
			}
		}
	}
}

#if !__HIERARCHICAL_NODES_ENABLED
void CPathServer::VisualiseHierarchicalNavData(CHierarchicalNavData * UNUSED_PARAM(pHierNav), const Vector3 & UNUSED_PARAM(vOrigin), const float UNUSED_PARAM(fVisDist))
{
#else
void CPathServer::VisualiseHierarchicalNavData(CHierarchicalNavData * pHierNav, const Vector3 & vOrigin, const float fVisDist)
{
	if(!pHierNav)
		return;

	Vector3 vNodeVec(0.0f, 0.0f, 2.0f);
	Vector3 vNodeLinkA(0.0f, 0.0f, 1.0f);
	Vector3 vNodeLinkB(0.0f, 0.0f, 1.15f);

	const Vector3 & vMins = pHierNav->GetMin();
	const Vector3 & vSize = pHierNav->GetSize();
	Vector3 vNodePos, vDestNodePos;

	static const float fDrawRangeSqr = fVisDist*fVisDist;

	u32 n,l;
	for(n=0; n<pHierNav->GetNumNodes(); n++)
	{
		CHierarchicalNavNode * pNode = pHierNav->GetNode(n);
		pNode->GetNodePosition(vNodePos, vMins, vSize);

		const float fD2 = (vNodePos - vOrigin).Mag2();

		if(fD2 < fDrawRangeSqr)
		{
			if(fD2 < g_fClosestHierNodeDist2)
			{
				g_fClosestHierNodeDist2 = fD2;
				g_pClosestHierNode = pNode;
			}

			{
				if(pNode->GetIsDebugMarked())
				{
					grcDebugDraw::Line(vNodePos, vNodePos + (vNodeVec*2.0f), Color_yellow2, Color_red2);
				}
				else
				{
					grcDebugDraw::Line(vNodePos, vNodePos + vNodeVec, Color_grey0, Color_white);
				}

				const u32 iNumLinks = pNode->GetNumLinks();
				for(l=0; l<iNumLinks; l++)
				{
					CHierarchicalNavLink * pLink = pHierNav->GetLink(pNode->GetStartOfLinkData()+l);
					if(pLink->GetNavMeshIndex()!=NAVMESH_NAVMESH_INDEX_NONE)
					{
						const TNavMeshIndex iNavMesh = pLink->GetNavMeshIndex();
						Assert(iNavMesh<=NAVMESH_MAX_MAP_INDEX);

						if(iNavMesh<=NAVMESH_MAX_MAP_INDEX)
						{
							int iX,iY;
							m_pNavMeshStore->GetSectorCoordsFromMeshIndex(iNavMesh, iX, iY);
							if(iX >= 0 && iY >= 0 && iX < CPathServerExtents::GetWorldWidthInSectors() && iY < CPathServerExtents::GetWorldDepthInSectors())
							{
								const int iNodesIndex = m_pNavNodesStore->GetMeshIndexFromSectorCoords(iX, iY);
								if(iNodesIndex != m_pNavNodesStore->GetMeshIndexNone())
								{
									CHierarchicalNavData * pNodes = m_pNavNodesStore->GetMeshByIndex(iNodesIndex);
									if(pNodes)
									{
										const Vector3 & vDestMins = pNodes->GetMin();
										const Vector3 & vDestSize = pNodes->GetSize();

										CHierarchicalNavNode * pDestNode = pNodes->GetNode(pLink->GetNodeIndex());
										pDestNode->GetNodePosition(vDestNodePos, vDestMins, vDestSize);

										grcDebugDraw::Line(vNodePos + vNodeLinkA, vDestNodePos + vNodeLinkB, Color_yellow, Color_orange);

										if(CPathServer::m_bVisualiseLinkWidths)
										{
											Vector3 vLinkVec = vDestNodePos - vNodePos;
											vLinkVec.Normalize();
											const Vector3 vTangent = CrossProduct(vLinkVec, ZAXIS);

											if(pLink->GetWidthToLeft() != 0)
											{
												const float fWidthLeft = (float) pLink->GetWidthToLeft();
												const Vector3 vLeft1 = vNodePos+vNodeLinkA - (vTangent*fWidthLeft);
												const Vector3 vLeft2 = vDestNodePos+vNodeLinkB - (vTangent*fWidthLeft);

												grcDebugDraw::Line(vNodePos + vNodeLinkA, vLeft1, Color_DarkGoldenrod4, Color_DarkGoldenrod4);
												grcDebugDraw::Line(vDestNodePos + vNodeLinkB, vLeft2, Color_DarkGoldenrod4, Color_DarkGoldenrod4);
												grcDebugDraw::Line(vLeft1, vLeft2, Color_yellow4, Color_orange4);

												grcDebugDraw::Poly(vNodePos + vNodeLinkA, vLeft1, vLeft2, Color_DarkGoldenrod2);
											}
											if(pLink->GetWidthToRight() != 0)
											{
												const float fWidthRight = (float) pLink->GetWidthToRight();
												const Vector3 vRight1 = vNodePos+vNodeLinkA + (vTangent*fWidthRight);
												const Vector3 vRight2 = vDestNodePos+vNodeLinkB + (vTangent*fWidthRight);

												grcDebugDraw::Line(vNodePos + vNodeLinkA, vRight1, Color_DarkGoldenrod4, Color_DarkGoldenrod4);
												grcDebugDraw::Line(vDestNodePos + vNodeLinkB, vRight2, Color_DarkGoldenrod4, Color_DarkGoldenrod4);
												grcDebugDraw::Line(vRight1, vRight2, Color_yellow4, Color_orange4);

												grcDebugDraw::Poly(vNodePos + vNodeLinkA, vRight1, vRight2, Color_DarkGoldenrod2);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
#endif // __HIERARCHICAL_NODES_ENABLED
}

#endif

void
CPathServer::VisualiseAdjacency(const Vector3 & vPosStart, const Vector3 & vPosEnd, eNavMeshPolyAdjacencyType adjacencyType, bool bIsHighDrop, bool bIsDisabled, s32 iAlpha)
{
#ifdef GTA_ENGINE
	static Color32 climbHighCol1(0,0,0xff,0);
	static Color32 climbHighCol2(0x40,0x40,0xff,0);
	static Color32 climbHighCol3(0x80,0x80,0xff,0);
	static Color32 climbHighCol4(0xc0,0xc0,0xff,0);

	static Color32 climbLowCol1(0,0,0x40,0);
	static Color32 climbLowCol2(0x40,0x40,0xa0,0);
	static Color32 climbLowCol3(0x80,0x80,0xa0,0);
	static Color32 climbLowCol4(0xc0,0xc0,0xa0,0);

	static Color32 dropDownCol1(0,0,0xff,0);
	static Color32 dropDownCol2(0,0x40,0xff,0);
	static Color32 dropDownCol3(0,0x80,0xff,0);
	static Color32 dropDownCol4(0,0xc0,0xff,0);

	Color32 colStart, colAboveStart, colAboveEnd, colEnd;
	Vector3 vPosAboveStart, vPosAboveEnd, vArrowTip1, vArrowTip2;

	bool bDrawAdjacency = false;

	switch(adjacencyType)
	{
		case ADJACENCY_TYPE_CLIMB_HIGH:
		{
			if(CPathServer::m_bDebugShowHighClimbOvers)
			{
				vPosAboveStart = vPosStart + Vector3(0,0,4.0f);
				vPosAboveEnd = vPosEnd + Vector3(0,0,4.0f);

				colStart = climbHighCol1;
				colAboveStart = climbHighCol2;
				colAboveEnd = climbHighCol2;
				colEnd = climbHighCol3;
				bDrawAdjacency = true;
			}
			break;
		}
		case ADJACENCY_TYPE_CLIMB_LOW:
		{
			if(CPathServer::m_bDebugShowLowClimbOvers)
			{
				vPosAboveStart = vPosStart + Vector3(0,0,2.0f);
				vPosAboveEnd = vPosEnd + Vector3(0,0,2.0f);

				colStart = climbLowCol1;
				colAboveStart = climbLowCol2;
				colAboveEnd = climbLowCol2;
				colEnd = climbLowCol3;
				bDrawAdjacency = true;
			}
			break;
		}
		case ADJACENCY_TYPE_DROPDOWN:
		{
			if(CPathServer::m_bDebugShowDropDowns)
			{
				vPosAboveStart = vPosStart + Vector3(0,0,2.0f);
				vPosAboveEnd = vPosEnd + Vector3(0,0,2.0f);

				colStart = dropDownCol1;
				colAboveStart = dropDownCol2;
				colAboveEnd = dropDownCol3;
				colEnd = dropDownCol4;
				bDrawAdjacency = true;
			}
			break;
		}
		default:
		{
			Assert(0);
			return;
		}
	}

	if(!bDrawAdjacency)
		return;

	if(bIsHighDrop)
	{
		colEnd = Color32(0xFF,0,0,iAlpha);
	}
	if(bIsDisabled)
	{
		colStart = colAboveStart = Color32(0x90,0,0,iAlpha);
		colEnd = colAboveEnd = Color32(0x60,0,0,iAlpha);
	}

	colStart.SetAlpha(iAlpha);
	colAboveStart.SetAlpha(iAlpha);
	colAboveEnd.SetAlpha(iAlpha);
	colEnd.SetAlpha(iAlpha);

	float fHeighestVal = Max(vPosAboveStart.z, vPosAboveEnd.z);
	vPosAboveStart.z = fHeighestVal;
	vPosAboveEnd.z = fHeighestVal;

	grcDebugDraw::Line(vPosStart, vPosAboveStart, colStart, colAboveStart);
	grcDebugDraw::Line(vPosAboveStart, vPosAboveEnd, colAboveStart, colAboveEnd);
	grcDebugDraw::Line(vPosAboveEnd, vPosEnd, colAboveEnd, colEnd);
#else // GTA_ENGINE
	vPosStart; vPosEnd; adjacencyType; iAlpha; bIsHighDrop; bIsDisabled;
#endif // GTA_ENGINE
}

#endif


#ifdef GTA_ENGINE
#if !__FINAL
void
CPathServer::VisualiseSpecialLinks(CNavMesh * pNavMesh, aiNavDomain domain)
{
	u32 s;
	for(s=0; s<pNavMesh->GetNumSpecialLinks(); s++)
	{
		const CSpecialLinkInfo & specLink = pNavMesh->GetSpecialLinks()[s];
		if(specLink.GetIsDisabled())
			continue;
		 
		CNavMesh * pOriginalFromNavMesh = GetNavMeshFromIndex(specLink.GetOriginalLinkFromNavMesh(), domain);
		CNavMesh * pOriginalToNavMesh = GetNavMeshFromIndex(specLink.GetOriginalLinkToNavMesh(), domain);

		Assert(pOriginalToNavMesh->GetIndexOfMesh()==pNavMesh->GetIndexOfMesh());

		Vector3 vFromPos, vToPos;
		pOriginalFromNavMesh->DecompressVertex(vFromPos, specLink.GetLinkFromPosX(), specLink.GetLinkFromPosY(), specLink.GetLinkFromPosZ());
		pOriginalToNavMesh->DecompressVertex(vToPos, specLink.GetLinkToPosX(), specLink.GetLinkToPosY(), specLink.GetLinkToPosZ());

		vFromPos.z += 0.5f;

		Vector3 vFromPolyCentroid, vToPolyCentroid;
		pOriginalFromNavMesh->GetPolyCentroidQuick( pOriginalFromNavMesh->GetPoly(specLink.GetOriginalLinkFromPoly()), vFromPolyCentroid);
		pOriginalToNavMesh->GetPolyCentroidQuick( pOriginalToNavMesh->GetPoly(specLink.GetOriginalLinkToPoly()), vToPolyCentroid);

		switch(specLink.GetLinkType())
		{
			case NAVMESH_LINKTYPE_CLIMB_LADDER:
			case NAVMESH_LINKTYPE_DESCEND_LADDER:
			{
				const bool bBlocked = m_PathServerThread.m_BlockedLadders.IsLadderBlocked( vFromPos.z < vToPos.z ? vFromPos : vToPos );

				grcDebugDraw::Line( vFromPolyCentroid, vFromPos, bBlocked ? Color_red3 : Color_turquoise, bBlocked ? Color_red3 : Color_DarkTurquoise );
				grcDebugDraw::Line( vFromPos, vToPos, bBlocked ? Color_red2 : Color_DarkTurquoise);
				grcDebugDraw::Line( vToPos, vToPolyCentroid, bBlocked ? Color_red3 : Color_DeepSkyBlue4 );
				break;
			}
			case NAVMESH_LINKTYPE_CLIMB_OBJECT:
			{
				grcDebugDraw::Line( vFromPolyCentroid, vFromPos, Color_tan, Color_tan );
				grcDebugDraw::Line( vFromPos, vFromPos+(ZAXIS*2.0f), Color_tan );
				grcDebugDraw::Line( vFromPos+(ZAXIS*2.0f), vToPos+(ZAXIS*2.0f), Color_tan2 );
				grcDebugDraw::Line( vToPos+(ZAXIS*2.0f), vToPos, Color_tan4 );
				grcDebugDraw::Line( vToPos, vToPolyCentroid, Color_tan4 );
				break;
			}
			default:
				Assert(false);
				break;
		}

		// Draw heading
		float fHeading = (((float)specLink.GetLinkFlags()) / 255.0f) * TWO_PI;
		fHeading = fwAngle::LimitRadianAngle(fHeading);
		Vector3 vWaypointNormal(-rage::Sinf(fHeading),rage::Cosf(fHeading),0.0f);

		grcDebugDraw::Arrow( VECTOR3_TO_VEC3V(vFromPos + ZAXIS), VECTOR3_TO_VEC3V(vFromPos + ZAXIS + (vWaypointNormal * 0.5f)), 0.2f, Color_orange);
	}

}
#endif
#endif



#if !__FINAL

int
CPathServer::VisualisePathServerCommon(aiNavDomain domain, int ypos)
{
#ifdef GTA_ENGINE

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	//****************************************
	// Ready for text drawing
	//****************************************

	char TempString[256];
	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 40;

	//********************************************
	//	Get navmesh
	//********************************************

	int iSectorX = CPathServerExtents::GetWorldToSectorX(vOrigin.x);
	int iSectorY = CPathServerExtents::GetWorldToSectorY(vOrigin.y);

	aiNavMeshStore* pStore = m_pNavMeshStores[domain];

	u32 index = pStore->GetMeshIndexFromPosition(vOrigin);
	CNavMesh * pNavMesh = (index == NAVMESH_NAVMESH_INDEX_NONE) ? NULL : pStore->GetMeshByIndex(index);

	if(!pNavMesh)
	{
		int iMeshX, iMeshY;
		pStore->MeshIndexToMeshCoords(index, iMeshX, iMeshY);
		iMeshX *= CPathServerExtents::m_iNumSectorsPerNavMesh;
		iMeshY *= CPathServerExtents::m_iNumSectorsPerNavMesh;

		sprintf(
			TempString,
			"NavMesh[%i][%i] Index:%i - not loaded",
			iMeshX, iMeshY, index
			);

		//Sector(%i, %i)  iSectorX, iSectorY, 

		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);

		ypos += 16;
		return ypos;
	}

	//********************************************
	//	Display PathServer info
	//********************************************

	// draw main caption
	if(pNavMesh)
	{
		int iMeshX, iMeshY;
		pStore->MeshIndexToMeshCoords(index, iMeshX, iMeshY);
		iMeshX *= CPathServerExtents::m_iNumSectorsPerNavMesh;
		iMeshY *= CPathServerExtents::m_iNumSectorsPerNavMesh;

		s32 iSizeInKb = (s32) (((float)pNavMesh->GetTotalMemoryUsed()) / 1024.0f);
		sprintf(
			TempString,
			"NavMesh[%i][%i] Index:%i (%iKb)",
			iMeshX, iMeshY, index, iSizeInKb
			);

		//Sector(%i, %i)  iSectorX, iSectorY, 

		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
		ypos += 16;

		sprintf(
			TempString,
			"NumPolys: %i, NumVerts: %i, SizeOfPools: %i",
			pNavMesh->GetNumPolys(), pNavMesh->GetNumVertices(), pNavMesh->GetSizeOfPools()
			);

		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
		ypos += 16;
	}
	else
	{
		sprintf(
			TempString,
			"PathServer - Sector(%i, %i) : no NavMesh at this location",
			iSectorX, iSectorY
			);

		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
		ypos += 16;
	}

#endif	// GTA_ENGINE

	return ypos;
}





void
CPathServer::VisualiseText(int ypos, aiNavDomain domain)
{
	ypos += 16;

#ifdef GTA_ENGINE

	char TempString[256];
	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 48;

	sprintf(TempString, "MSecs to last extract coverpoints : %.2f", m_fLastTimeTakenToExtractCoverPointsMs);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "MSecs to add/remove/update objects : %.2f",  m_fTimeTakenToAddRemoveUpdateObjects);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "MSecs to process ped-gen algorithm : %.2f",  m_fTimeTakenToDoPedGenInMSecs);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "MSecs spent on requests this game turn : %.2f",  m_fTimeSpentProcessingThisGameTurn);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "MSecs main game spent on thread-stalls : %.2f",  m_fMainGameTimeSpendOnThreadStalls);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;
	
	sprintf(TempString, "ms_iBlockRequestsFlags : %i",  ms_iBlockRequestsFlags);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	// Render some extra info
	sprintf(
		TempString,
		"[ThreadStalls on NavMeshes:%i] [ThreadStalls on Objects:%i] [Couldn't update objects:%i]",
		m_iNumThreadStallsForNavMeshes,
		m_iNumThreadStallsForDynamicObjects,
		m_iNumTimesCouldntUpdateObjectsDueToGameThreadAccess
		);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	// Render some extra info
	sprintf(
		TempString,
		"[DynObjects:%i] [ObjGrids:%i] [GaveTime:%i] [Scripted:%i]",
		m_iNumDynamicObjects,
		CDynamicObjectsContainer::GetNumGrids(),
		m_iNumTimesGaveTime,
		m_iNumScriptBlockingObjects
	);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "Num tessellation polys : %i / %i (peaked at %i)", m_iCurrentNumTessellationPolys, CNavMesh::ms_iNumPolysInTesselationNavMesh, fwPathServer::ms_iPeakNumTessellationPolys);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

#if __BANK
	for(s32 r=0; r<NAVMESH_MAX_REQUIRED_REGIONS; r++)
	{
		sprintf(TempString, "Region %i) Active=%s (%.1f, %.1f) radius=%.1f, threadId=%u, scriptName=\"%s\"",
			r,
			m_NavMeshRequiredRegions[r].m_bActive ? "true" : "false",
			m_NavMeshRequiredRegions[r].m_vOrigin.x,
			m_NavMeshRequiredRegions[r].m_vOrigin.y,
			m_NavMeshRequiredRegions[r].m_fNavMeshLoadRadius,
			m_NavMeshRequiredRegions[r].m_iThreadId,
			m_NavMeshRequiredRegions[r].m_ScriptName
		);
		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
		ypos += 16;
	}
#endif

	bool bAllLoaded = AreAllNavMeshRegionsLoaded(domain);
	sprintf(TempString, "All requested regions loaded : %s", bAllLoaded ? "true" : "false");
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), bAllLoaded ? Color_white : Color_red, TempString);
	ypos += 16;

	sprintf(TempString, "ImmediateMode calls : Time(%.2f ms) Los(%i) Wander(%i) Flee(%i) Seek(%i)",
		m_fTimeSpentOnImmediateModeCallsThisFrame, m_iNumImmediateModeLosCallsThisFrame, m_iNumImmediateModeWanderPathsThisFrame, m_iNumImmediateModeFleePathsThisFrame, m_iNumImmediateModeSeekPathsThisFrame);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "Tracking (crit sec: %.2f), (track all peds: %.2f)", m_fTimeToLockDataForTracking, m_fTimeToTrackAllPeds);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "WorldMin (%.1f, %.1f, %.1f)  Sector (%i, %i)",
		g_WorldLimits_MapDataExtentsAABB.GetMinVector3().x, g_WorldLimits_MapDataExtentsAABB.GetMinVector3().y, g_WorldLimits_MapDataExtentsAABB.GetMinVector3().z,
		CPathServerExtents::GetWorldToSectorX(g_WorldLimits_MapDataExtentsAABB.GetMinVector3().x), CPathServerExtents::GetWorldToSectorY(g_WorldLimits_MapDataExtentsAABB.GetMinVector3().y));
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "WorldMax (%.1f, %.1f, %.1f)  Sector (%i, %i)",
		g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().x, g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().y, g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().z,
		CPathServerExtents::GetWorldToSectorX(g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().x), CPathServerExtents::GetWorldToSectorY(g_WorldLimits_MapDataExtentsAABB.GetMaxVector3().y));
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

	sprintf(TempString, "PedGenCoords (ShouldUseStartupMode:%s, InUse:%s Complete:%s Consumed:%s NumCoords:%i)",
		CPedPopulation::ShouldUseStartupMode()?"true":"false",
		GetAmbientPedGen().m_PedGenCoords.GetInUse()?"true":"false",
		GetAmbientPedGen().m_PedGenCoords.GetIsComplete()?"true":"false",
		GetAmbientPedGen().m_PedGenCoords.GetIsConsumed()?"true":"false",
		GetAmbientPedGen().m_PedGenCoords.GetNumCoords()
	);
	grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), Color_white, TempString);
	ypos += 16;

#endif
}


void
CPathServer::VisualiseLineOfSightRequests(int ypos, aiNavDomain domain)
{
#ifdef GTA_ENGINE

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	//********************************************
	//	Get navmesh
	//********************************************

	u32 iNavMeshIndex = GetNavMeshIndexFromPosition(vOrigin, domain);
	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);

	if(!pNavMesh)
		return;

	//********************************************
	//	Display PathServer info
	//********************************************

	char TempString[256];
	char FullString[256];
	char InfoString[256];
	char TimeTakenString[256];
	char StatusString[256];

	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 16;
	int iHandlePos = (s32)g_vPathserverDebugTopLeft.x + 32;
	int iAddrPos = (s32)g_vPathserverDebugTopLeft.x + 128;
	int iStatusPos = (s32)g_vPathserverDebugTopLeft.x + 200;

	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	// draw titles for each column
	grcDebugDraw::Text(ScreenPos((float)iHandlePos,(float)ypos), Color_white, "Handle");
	grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_white, "Status");

	ypos += 16;

	int i;
	int index = 0;
	for(i=0; i<MAX_NUM_LOS_REQUESTS; i++)
	{
		CLineOfSightRequest * pLosRequest = &m_LineOfSightRequests[i];

		Color32 iDbgCol;
		if(i==m_iSelectedLosRequest)
			iDbgCol = Color32(255,255,255,255);
		else
			iDbgCol = Color32(128,128,128,255);
		sprintf(TempString, "%i)", index++);
		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), iDbgCol, TempString);


		sprintf(TempString, "%X", pLosRequest->m_hHandle);
		grcDebugDraw::Text(ScreenPos(float(iHandlePos + 16), (float)ypos), Color_white, TempString);

		// Draw the path request's 'context' - this is usually the CPed pointer
		sprintf(TempString, "%p", pLosRequest->m_pContext);
		grcDebugDraw::Text(ScreenPos((float)iAddrPos, (float)ypos), Color_white, TempString);

		if(pLosRequest->m_bRequestPending)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_yellow, "Pending");
		}
		else if(pLosRequest->m_bRequestActive)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_red, "Active");
		}
		else if(pLosRequest->m_bComplete)
		{
			// Create the string for how long the request took to process
			TimeTakenString[0] = 0;
			InfoString[0] = 0;
			StatusString[0] = 0;
			FullString[0] = 0;

			sprintf(
				TimeTakenString, "Took %.2f ms (%.2f, %.2f)",
				pLosRequest->m_fTotalProcessingTimeInMillisecs,
				pLosRequest->m_fMillisecsToFindPolys,
				pLosRequest->m_fMillisecsToFindLineOfSight);

			switch((int)pLosRequest->m_iCompletionCode)
			{
				case PATH_FOUND:
				{
					sprintf(StatusString, "%s", "COMPLETE");
					break;
				}
				case PATH_NO_SURFACE_AT_COORDINATES:
				{
					sprintf(StatusString, "%s", "PATH_NO_SURFACE_AT_COORDINATES");
					break;
				}
				default:
					sprintf(StatusString, "%s", "UNKNOWN_ERR_MSG");
					break;
			}

			sprintf(InfoString, "LOS %s, pts:%i", pLosRequest->m_bLineOfSightExists ? "CLEAR" : "BLOCKED", pLosRequest->m_iNumPts);
			sprintf(FullString, "%s, %s, %s", StatusString, InfoString, TimeTakenString);

			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_green, FullString);
		}

		ypos += ySpacing;
	}

#else	// GTA_ENGINE
	ypos;
#endif	// GTA_ENGINE

}


void
CPathServer::VisualiseAudioPropertiesRequests(int ypos, aiNavDomain domain)
{
#ifdef GTA_ENGINE

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	//********************************************
	//	Get navmesh
	//********************************************

	u32 iNavMeshIndex = GetNavMeshIndexFromPosition(vOrigin, domain);
	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);

	if(!pNavMesh)
		return;

	//********************************************
	//	Display PathServer info
	//********************************************

	char TempString[256];
	char FullString[256];
	char InfoString[256];
	char TimeTakenString[256];
	char StatusString[256];

	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 16;
	int iHandlePos = (s32)g_vPathserverDebugTopLeft.x + 32;
	int iAddrPos = (s32)g_vPathserverDebugTopLeft.x + 128;
	int iStatusPos = (s32)g_vPathserverDebugTopLeft.x + 200;

	// Set render state for text

	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	// draw titles for each column
	grcDebugDraw::Text(ScreenPos((float)iHandlePos,(float)ypos), Color_white, "Handle");
	grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_white, "Status");

	ypos += 16;

	int i;
	int index = 0;
	for(i=0; i<MAX_NUM_AUDIO_REQUESTS; i++)
	{
		CAudioRequest * pAudioRequest = &m_AudioRequests[i];

		sprintf(TempString, "%i)", index++);
		Color32 dbgCol;
		if(i==m_iSelectedLosRequest)
			dbgCol = Color32(255,255,255,255);
		else
			dbgCol = Color32(128,128,128,255);

		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), dbgCol, TempString);

		sprintf(TempString, "%X", pAudioRequest->m_hHandle);
		grcDebugDraw::Text(ScreenPos(float(iHandlePos + 16), (float)ypos), Color_white, TempString);

		// Draw the path request's 'context' - this is usually the CPed pointer
		sprintf(TempString, "%p", pAudioRequest->m_pContext);
		grcDebugDraw::Text(ScreenPos((float)iAddrPos, (float)ypos), Color_white, TempString);

		if(pAudioRequest->m_bRequestPending)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_yellow, "Pending");
		}
		else if(pAudioRequest->m_bRequestActive)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_red, "Active");
		}
		else if(pAudioRequest->m_bComplete)
		{
			// Create the string for how long the request took to process
			TimeTakenString[0] = 0;
			InfoString[0] = 0;
			StatusString[0] = 0;
			FullString[0] = 0;

			sprintf(TimeTakenString, "Took %.2f ms", pAudioRequest->m_fTotalProcessingTimeInMillisecs);

			switch((int)pAudioRequest->m_iCompletionCode)
			{
				case PATH_FOUND:
				{
					sprintf(StatusString, "%s", "COMPLETE");
					break;
				}
				case PATH_NOT_FOUND:
				{
					sprintf(StatusString, "%s", "NOT_FOUND");
					break;
				}
				default:
					sprintf(StatusString, "%s", "UNKNOWN_ERR_MSG");
					break;
			}

			sprintf(FullString, "%s, %s, %s", StatusString, InfoString, TimeTakenString);
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_green, FullString);
		}

		ypos += ySpacing;
	}

#else	// GTA_ENGINE
	ypos;
#endif	// GTA_ENGINE

}


void
CPathServer::VisualiseFloodFillRequests(int ypos, aiNavDomain domain)
{
#ifdef GTA_ENGINE

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	//********************************************
	//	Get navmesh
	//********************************************

	u32 iNavMeshIndex = GetNavMeshIndexFromPosition(vOrigin, domain);
	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);

	if(!pNavMesh)
		return;

	//********************************************
	//	Display PathServer info
	//********************************************

	char TempString[256];
	char FullString[256];
	char InfoString[256];
	char TimeTakenString[256];
	char StatusString[256];
	char ReqTypeString[256];

	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 16;
	int iHandlePos = (s32)g_vPathserverDebugTopLeft.x + 32;
	int iAddrPos = (s32)g_vPathserverDebugTopLeft.x + 128;
	int iStatusPos = (s32)g_vPathserverDebugTopLeft.x + 200;

	// Set render state for text

	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	// draw titles for each column
	grcDebugDraw::Text(ScreenPos((float)iHandlePos, (float)ypos), Color_white, "Handle");
	grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_white, "Status");

	ypos += 16;

	int i;
	int index = 0;
	for(i=0; i<MAX_NUM_FLOODFILL_REQUESTS; i++)
	{
		CFloodFillRequest * pFloodFillRequest = &m_FloodFillRequests[i];

		sprintf(TempString, "%i)", index++);
		grcDebugDraw::Text(ScreenPos((float)iIndexPos, (float)ypos), Color_grey, TempString);

		sprintf(TempString, "%X", pFloodFillRequest->m_hHandle);
		grcDebugDraw::Text(ScreenPos(float(iHandlePos + 16), (float)ypos), Color_white, TempString);

		// Draw the path request's 'context' - this is usually the CPed pointer
		sprintf(TempString, "%p", pFloodFillRequest->m_pContext);
		grcDebugDraw::Text(ScreenPos((float)iAddrPos, (float)ypos), Color_white, TempString);

		if(pFloodFillRequest->m_FloodFillType==CFloodFillRequest::EAudioPropertiesFloodFill)
		{
			sprintf(ReqTypeString, "Audio");
		}
		else if(pFloodFillRequest->m_FloodFillType==CFloodFillRequest::EFindClosestCarNodeFloodFill)
		{
			sprintf(ReqTypeString, "CarNode");
		}
		else if(pFloodFillRequest->m_FloodFillType==CFloodFillRequest::EFindClosestShelteredPolyFloodFill)
		{
			sprintf(ReqTypeString, "FindShelter");
		}
		else if(pFloodFillRequest->m_FloodFillType==CFloodFillRequest::ECalcAreaUnderfoot)
		{
			sprintf(ReqTypeString, "CalcArean");
		}
		else if(pFloodFillRequest->m_FloodFillType==CFloodFillRequest::EFindNearbyPavementFloodFill)
		{
			sprintf(ReqTypeString, "FindPavement");
		}
		else
		{
			sprintf(ReqTypeString, "Unknown");
		}

		if(pFloodFillRequest->m_bRequestPending)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_yellow, "Pending");
		}
		else if(pFloodFillRequest->m_bRequestActive)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_red, "Pending");
		}
		else if(pFloodFillRequest->m_bComplete)
		{
			// Create the string for how long the request took to process
			TimeTakenString[0] = 0;
			InfoString[0] = 0;
			StatusString[0] = 0;
			FullString[0] = 0;

			sprintf(TimeTakenString, "Took %.2f ms", pFloodFillRequest->m_fTotalProcessingTimeInMillisecs);

			switch((int)pFloodFillRequest->m_iCompletionCode)
			{
			case PATH_FOUND:
				{
					sprintf(StatusString, "%s", "COMPLETE");
					break;
				}
			case PATH_NOT_FOUND:
				{
					sprintf(StatusString, "%s", "NOT_FOUND");
					break;
				}
			default:
				sprintf(StatusString, "%s", "UNKNOWN_ERR_MSG");
				break;
			}

			sprintf(FullString, "(%s), %s, %s, %s", StatusString, InfoString, TimeTakenString, ReqTypeString);
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_green, FullString);
		}

		ypos += ySpacing;
	}

#else	// GTA_ENGINE
	ypos;
#endif	// GTA_ENGINE

}


void CPathServer::VisualiseClearAreaRequests(int ypos, aiNavDomain domain)
{
#ifdef GTA_ENGINE

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	//********************************************
	//	Get navmesh
	//********************************************

	u32 iNavMeshIndex = GetNavMeshIndexFromPosition(vOrigin, domain);
	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);

	if(!pNavMesh)
		return;

	//********************************************
	//	Display PathServer info
	//********************************************

	char TempString[256];
	char FullString[256];
	char InfoString[256];
	char TimeTakenString[256];
	char StatusString[256];

	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 16;
	int iHandlePos = (s32)g_vPathserverDebugTopLeft.x + 32;
	int iAddrPos = (s32)g_vPathserverDebugTopLeft.x + 128;
	int iStatusPos = (s32)g_vPathserverDebugTopLeft.x + 200;

	// Set render state for text

	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	// draw titles for each column
	grcDebugDraw::Text(ScreenPos((float)iHandlePos,(float)ypos), Color_white, "Handle");
	grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_white, "Status");

	ypos += 16;

	int i;
	int index = 0;
	for(i=0; i<MAX_NUM_CLEARAREA_REQUESTS; i++)
	{
		CClearAreaRequest * pClearAreaRequest = &m_ClearAreaRequests[i];

		sprintf(TempString, "%i)", index++);
		Color32 dbgCol;
		if(i==m_iSelectedLosRequest)
			dbgCol = Color32(255,255,255,255);
		else
			dbgCol = Color32(128,128,128,255);

		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), dbgCol, TempString);

		sprintf(TempString, "%X", pClearAreaRequest->m_hHandleThisWas);
		grcDebugDraw::Text(ScreenPos(float(iHandlePos + 16), (float)ypos), Color_white, TempString);

		// Draw the path request's 'context' - this is usually the CPed pointer
		sprintf(TempString, "%p", pClearAreaRequest->m_pContext);
		grcDebugDraw::Text(ScreenPos((float)iAddrPos, (float)ypos), Color_white, TempString);

		if(pClearAreaRequest->m_bRequestPending)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_yellow, "Pending");
		}
		else if(pClearAreaRequest->m_bRequestActive)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_red, "Active");
		}
		else if(pClearAreaRequest->m_bComplete)
		{
			// Create the string for how long the request took to process
			TimeTakenString[0] = 0;
			InfoString[0] = 0;
			StatusString[0] = 0;
			FullString[0] = 0;

			sprintf(InfoString, "slept:%i", pClearAreaRequest->m_iNumTimesSlept);

			sprintf(TimeTakenString, "Took %.2f ms", pClearAreaRequest->m_fTotalProcessingTimeInMillisecs);

			switch((int)pClearAreaRequest->m_iCompletionCode)
			{
				case PATH_FOUND:
				{
					sprintf(StatusString, "%s", "COMPLETE");
					break;
				}
				case PATH_NOT_FOUND:
				{
					sprintf(StatusString, "%s", "NOT_FOUND");
					break;
				}
				default:
					sprintf(StatusString, "%s", "UNKNOWN_ERR_MSG");
					break;
			}

			sprintf(FullString, "%s, %s, %s", StatusString, InfoString, TimeTakenString);
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_green, FullString);
		}

		ypos += ySpacing;
	}

#else	// GTA_ENGINE
	ypos;
#endif	// GTA_ENGINE

}

void CPathServer::VisualiseClosestPositionRequests(int ypos, aiNavDomain domain)
{
#ifdef GTA_ENGINE

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	//********************************************
	//	Get navmesh
	//********************************************

	u32 iNavMeshIndex = GetNavMeshIndexFromPosition(vOrigin, domain);
	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);

	if(!pNavMesh)
		return;

	//********************************************
	//	Display PathServer info
	//********************************************

	char TempString[256];
	char FullString[256];
	char InfoString[256];
	char TimeTakenString[256];
	char StatusString[256];

	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 16;
	int iHandlePos = (s32)g_vPathserverDebugTopLeft.x + 32;
	int iAddrPos = (s32)g_vPathserverDebugTopLeft.x + 128;
	int iStatusPos = (s32)g_vPathserverDebugTopLeft.x + 200;

	// Set render state for text

	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	// draw titles for each column
	grcDebugDraw::Text(ScreenPos((float)iHandlePos,(float)ypos), Color_white, "Handle");
	grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_white, "Status");

	ypos += 16;

	int i;
	int index = 0;
	for(i=0; i<MAX_NUM_CLOSESTPOSITION_REQUESTS; i++)
	{
		CClosestPositionRequest * pClosestPosRequest = &m_ClosestPositionRequests[i];

		sprintf(TempString, "%i)", index++);
		Color32 dbgCol = Color32(255,255,255,255);

		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), dbgCol, TempString);

		sprintf(TempString, "%X", pClosestPosRequest->m_hHandleThisWas);
		grcDebugDraw::Text(ScreenPos(float(iHandlePos + 16), (float)ypos), Color_white, TempString);

		// Draw the path request's 'context' - this is usually the CPed pointer
		sprintf(TempString, "%p", pClosestPosRequest->m_pContext);
		grcDebugDraw::Text(ScreenPos((float)iAddrPos, (float)ypos), Color_white, TempString);

		if(pClosestPosRequest->m_bRequestPending)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_yellow, "Pending");
		}
		else if(pClosestPosRequest->m_bRequestActive)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_red, "Active");
		}
		else if(pClosestPosRequest->m_bComplete)
		{
			// Create the string for how long the request took to process
			TimeTakenString[0] = 0;
			InfoString[0] = 0;
			StatusString[0] = 0;
			FullString[0] = 0;

			sprintf(InfoString, "slept:%i", pClosestPosRequest->m_iNumTimesSlept);

			sprintf(TimeTakenString, "Took %.2f ms", pClosestPosRequest->m_fTotalProcessingTimeInMillisecs);

			switch((int)pClosestPosRequest->m_iCompletionCode)
			{
				case PATH_FOUND:
				{
					sprintf(StatusString, "%s", "COMPLETE");
					break;
				}
				case PATH_NOT_FOUND:
				{
					sprintf(StatusString, "%s", "NOT_FOUND");
					break;
				}
				default:
					sprintf(StatusString, "%s", "UNKNOWN_ERR_MSG");
					break;
			}

			sprintf(FullString, "%s, %s, %s", StatusString, InfoString, TimeTakenString);
			grcDebugDraw::Text(ScreenPos((float)iStatusPos, (float)ypos), Color_green, FullString);
		}

		ypos += ySpacing;
	}

#else	// GTA_ENGINE
	ypos;
#endif	// GTA_ENGINE

}

#ifdef GTA_ENGINE

void
CPathServer::VisualisePathDetails(CPathRequest * pPathReq, int ypos)
{
	char TempString[256];
	float fX = g_vPathserverDebugTopLeft.x + (SCREEN_WIDTH / 40.0f) + 8;
	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	//*****************************************
	// Type
	if(pPathReq->m_bWander)
		sprintf(TempString, "Type:Wander");
	else if(pPathReq->m_bFleeTarget)
		sprintf(TempString, "Type:Flee");
	else
		sprintf(TempString, "Type:Normal");

	if(pPathReq->m_bDontAvoidDynamicObjects)
		strcat(TempString, " NoAvoidance");
	if(pPathReq->m_bDoPostProcessToPreserveSlopeInfo)
		strcat(TempString, " PreserveZ");
	if(pPathReq->m_bSmoothSharpCorners)
		strcat(TempString, " Smooth");
	if(pPathReq->m_bGoAsFarAsPossibleIfNavMeshNotLoaded)
		strcat(TempString, " GoFarAsPoss");
	if(pPathReq->m_bUseLargerSearchExtents)
		strcat(TempString, " LargerSearchExtents");
	if(pPathReq->m_bDontLimitSearchExtents)
		strcat(TempString, " DontLimitSearchExtents");

	if(pPathReq->m_iNumInfluenceSpheres)
	{
		char ispheres[64];
		sprintf(ispheres, " iSpheres:%i", pPathReq->m_iNumInfluenceSpheres);
		strcat(TempString, ispheres);
	}

	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	//**********************************************
	// Time stats

	sprintf(TempString, "Total Time : %.3f, Frame Issued: %u, Frame Started: %u, Frame Completed: %u", pPathReq->m_fMillisecsToFindPath, pPathReq->m_iFrameRequestIssued, pPathReq->m_iFrameRequestStarted, pPathReq->m_iFrameRequestCompleted);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	sprintf(TempString, "FindEndPolys: %.3f, FindPolygonPath: %.3f, StringPullPath: %.3f, PostProcessZ: %.3f", pPathReq->m_fMillisecsToFindPolys, pPathReq->m_fMillisecsToFindPath, pPathReq->m_fMillisecsToRefinePath, pPathReq->m_fMillisecsToPostProcessZHeights);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	sprintf(TempString, "MinimisePathLength: %.3f, PullOutFromEdges: %.3f, SmoothPath: %.3f", pPathReq->m_fMillisecsToMinimisePathLength, pPathReq->m_fMillisecsToPullOutFromEdges, pPathReq->m_fMillisecsToSmoothPath);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	sprintf(TempString, "TimeInTessellation: %.3f, TimeInDetessellation: %.3f", pPathReq->m_fMillisecsSpentInTessellation, pPathReq->m_fMillisecsSpentInDetessellation);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	//********************************************************
	// Counter stats

	sprintf(TempString, "NumVisitedPolys:%i, NumTessellations:%i, NumTestDynObjLOS:%i, NumTestNavMeshLOS:%i",
		pPathReq->m_iNumVisitedPolygons,
		pPathReq->m_iNumTessellations,
		pPathReq->m_iNumTestDynamicObjectLOS,
		pPathReq->m_iNumTestNavMeshLOS);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	sprintf(TempString, "NumGetObjectsIntersectingRegion:%i, DynObjCacheHits:%i",
		pPathReq->m_iNumGetObjectsIntersectingRegion,
		pPathReq->m_iNumCacheHitsOnDynamicObjects);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	sprintf(TempString, "Num tessellation polys used : %i / %i", pPathReq->m_iNumTessellatedPolys, CNavMesh::GetNumPolysInTesselationMesh() );
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;

	sprintf(TempString, "Entity radius : %f", pPathReq->m_fEntityRadius);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, TempString);
	ypos += ySpacing;
}

#endif	// GTA_ENGINE


void
CPathServer::VisualisePathRequests(int ypos, aiNavDomain domain)
{
#ifdef GTA_ENGINE

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	//********************************************
	//	Get navmesh
	//********************************************

	u32 iNavMeshIndex = GetNavMeshIndexFromPosition(vOrigin, domain);
	CNavMesh * pNavMesh = GetNavMeshFromIndex(iNavMeshIndex, domain);

	if(!pNavMesh)
		return;

	//********************************************
	//	Display PathServer info
	//********************************************

	char TempString[256];
	char FullString[256];
	char InfoString[256];
	char TimeTakenString[256];
	char StatusString[256];

	int iIndexPos = (s32)g_vPathserverDebugTopLeft.x + 38;
	int iHandlePos = (s32)g_vPathserverDebugTopLeft.x + 48;
	int iAddrPos = (s32)g_vPathserverDebugTopLeft.x + 136;
	int iFramePos = (s32)g_vPathserverDebugTopLeft.x + 240;
	int iStatusPos = (s32)g_vPathserverDebugTopLeft.x + 320;

	// Set render state for text

	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	// draw titles for each column
	grcDebugDraw::Text(ScreenPos((float)iHandlePos,(float)ypos), Color_white, "Handle");
	grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_white, "Status");

	ypos += 16;

	int i;
	int index = 0;
	for(i=0; i<MAX_NUM_PATH_REQUESTS; i++)
	{
		CPathRequest * pPathRequest = &m_PathRequests[i];

		sprintf(TempString, "%i)", index++);

		Color32 dbgCol;

		if(i==m_iSelectedPathRequest)
			dbgCol = Color32(255,255,255,255);
		else
			dbgCol = Color32(128,128,128,255);
		grcDebugDraw::Text(ScreenPos((float)iIndexPos,(float)ypos), dbgCol, TempString);

		if(pPathRequest->m_bFleeTarget)
		{
			sprintf(TempString, "%x:flee", pPathRequest->m_hHandleThisWas);
		}
		else if(pPathRequest->m_bWander)
		{
			sprintf(TempString, "%x:wdr", pPathRequest->m_hHandleThisWas);
		}
		else
		{
			sprintf(TempString, "%x", pPathRequest->m_hHandleThisWas);
		}

		grcDebugDraw::Text(ScreenPos(float(iHandlePos+16),(float)ypos), Color_white, TempString);

		// Draw the path request's 'context' - this is usually the CPed pointer
		sprintf(TempString, "%p", pPathRequest->m_pContext);
		grcDebugDraw::Text(ScreenPos((float)iAddrPos,(float)ypos), Color_white, TempString);

		// Draw the path request's request frame
		sprintf(TempString, "%u", pPathRequest->m_iFrameRequestIssued);
		grcDebugDraw::Text(ScreenPos((float)iFramePos,(float)ypos), Color_white, TempString);

		if(pPathRequest->m_bWasAborted)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_tomato, "Aborted");
		}
		else if(pPathRequest->m_bRequestPending)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_yellow, "Pending");
		}
		else if(pPathRequest->m_bRequestActive)
		{
			grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_red, "Active");
		}
		else if(pPathRequest->m_bComplete)
		{
			// Create the string for how long the request took to process
			TimeTakenString[0] = 0;
			InfoString[0] = 0;
			StatusString[0] = 0;
			FullString[0] = 0;

			sprintf(
				TimeTakenString, "Took %.2f ms (%.1f, %.1f, %.1f, %.1f, %.1f, %.1f)",
				pPathRequest->m_fTotalProcessingTimeInMillisecs,
				pPathRequest->m_fMillisecsToFindPolys,
				pPathRequest->m_fMillisecsToFindPath,
				pPathRequest->m_fMillisecsToRefinePath,
				pPathRequest->m_fMillisecsToMinimisePathLength,
				pPathRequest->m_fMillisecsToPullOutFromEdges,
				pPathRequest->m_fMillisecsToPostProcessZHeights
			);

			switch((int)pPathRequest->m_iCompletionCode)
			{
				case PATH_FOUND:
				{
					sprintf(StatusString, "%s", "COMPLETE");
					break;
				}
				case PATH_NOT_FOUND:
					sprintf(StatusString, "%s", "PATH_NOT_FOUND");
					break;
				case PATH_TIMED_OUT:
					sprintf(StatusString, "%s", "PATH_TIMED_OUT");
					break;
				case PATH_REACHED_MAX_PATH_POLYS:
					sprintf(StatusString, "%s", "PATH_REACHED_MAX_PATH_POLYS");
					break;
				case PATH_INVALID_COORDINATES:
					sprintf(StatusString, "%s", "PATH_INVALID_COORDINATES");
					break;
				case PATH_NAVMESH_NOT_LOADED:
					sprintf(StatusString, "%s", "PATH_NAVMESH_NOT_LOADED");
					break;
				case PATH_ENDPOINTS_INSIDE_OBJECTS:
					sprintf(StatusString, "%s", "ENDPOINTS_INSIDE_OBJECTS");
					break;
				case PATH_NO_SURFACE_AT_COORDINATES:
					sprintf(StatusString, "%s", "PATH_NO_SURFACE");
					break;
				case PATH_RAN_OUT_OF_PATH_STACK_SPACE:
					sprintf(StatusString, "%s", "EXCEEDED_PATH_STACK");
					break;
				case PATH_RAN_OUT_VISITED_POLYS_SPACE:
					sprintf(StatusString, "%s", "EXCEEDED_VISITED_POLYS");
					break;
				case PATH_CANCELLED:
					sprintf(StatusString, "%s", "PATH_CANCELLED");
					break;
				case PATH_ERROR_INVALID_HANDLE:
					sprintf(StatusString, "%s", "PATH_ERROR_INVALID_HANDLE");
					break;
				case PATH_NO_POINTS:
					sprintf(StatusString, "%s", "PATH_NO_POINTS");
					break;
				case PATH_STILL_PENDING:
					sprintf(StatusString, "%s", "PATH_STILL_PENDING");
					break;
				case PATH_ABORTED_DUE_TO_ANOTHER_THREAD:
					sprintf(StatusString, "%s", "PATH_ABORTED_DUE_TO_ANOTHER_THREAD");
					break;
				default:
					sprintf(StatusString, "%s", "UNKNOWN_ERR_MSG");
					break;
			}

			sprintf(InfoString, "pts:%i tess:%i slept:%i visited:%i iters(%i,%i)",
				pPathRequest->m_iNumPoints,
				pPathRequest->m_iNumTessellations,
				pPathRequest->m_iNumTimesSlept,
				pPathRequest->m_iNumVisitedPolygons,
				pPathRequest->m_iNumFindPathIterations,
				pPathRequest->m_iNumRefinePathIterations);

			sprintf(FullString, "%s %s %s", StatusString, TimeTakenString, InfoString);
			grcDebugDraw::Text(ScreenPos((float)iStatusPos,(float)ypos), Color_NavyBlue, FullString);
		}

		ypos += ySpacing;
	}

	CPathRequest * pPathReq = &m_PathRequests[m_iSelectedPathRequest];
	VisualisePathDetails(pPathReq, ypos + ySpacing);

#else	// GTA_ENGINE
	ypos;
#endif	// GTA_ENGINE

}

#endif


#if !__FINAL
void CPathServer::RenderMiscThings()
{
#ifdef GTA_ENGINE
	if(ms_bDrawPedGenBlockingAreas)
	{
		DebugDrawPedGenBlockingAreas();
	}
	if(m_iVisualiseNavMeshes && ms_bDrawPedSwitchAreas)
	{
		DebugDrawPedSwitchAreas();
	}
	if(ms_bVisualiseOpponentPedGeneration)
	{
		CPathServerOpponentPedGen & pedGen = GetOpponentPedGen();
		if(pedGen.GetState() != CPathServerOpponentPedGen::STATE_IDLE)
		{
			Vector3 vStart, vEnd;
			vStart = vEnd = pedGen.m_vSearchOrigin;
			vStart.z -= pedGen.m_fDistZ;
			vEnd.z += pedGen.m_fDistZ;

			grcDebugDraw::Cylinder(RCC_VEC3V(vStart), RCC_VEC3V(vEnd), pedGen.m_fSearchRadiusXY, Color_chartreuse, false, false);

			s32 iNum = pedGen.m_iNumCoords;
			for(s32 i=0; i<iNum; i++)
			{
				float s = ((float)i) / ((float)iNum);
				Color32 col = Lerp(s, Color_green, Color_red);

				grcDebugDraw::Arrow(RCC_VEC3V(pedGen.m_SpawnCoords[i].m_vPos), VECTOR3_TO_VEC3V(pedGen.m_SpawnCoords[i].m_vPos+(ZAXIS*2.0f)), 0.4f, col);
			}
		}
	}
	if(m_iVisualiseNavMeshes == NavMeshVis_PedDensity)
	{
		if(CPedPopulation::GetAdjustPedSpawnDensity())
		{
			grcDebugDraw::BoxAxisAligned(VECTOR3_TO_VEC3V(CPedPopulation::GetAdjustPedSpawnDensityMin()), VECTOR3_TO_VEC3V(CPedPopulation::GetAdjustPedSpawnDensityMax()), Color_red, false);
		}
	}
	if(m_iVisualiseNavMeshes)
	{
		// Draw the water edges.
		int iNumWaterEdges;
		Vector3 vWaterEdges[SIZE_OF_WATEREDGES_BUFFER];

		if(GetWaterEdgePoints(iNumWaterEdges, &vWaterEdges[0]))
		{
			for(int v=0; v<iNumWaterEdges; v++)
			{
				Vector3 vTop = vWaterEdges[v] + Vector3(0.0f,0.0f,8.0f);
				grcDebugDraw::Line(vWaterEdges[v], vTop, Color_SeaGreen, Color_SeaGreen2);
				vTop.z -= 1.0f;

				grcDebugDraw::Line(vTop - Vector3(1.0f,0.0f,0.0f), vTop + Vector3(1.0f,0.0f,0.0f), Color_SeaGreen3);
				grcDebugDraw::Line(vTop - Vector3(0.0f,1.0f,0.0f), vTop + Vector3(0.0f,1.0f,0.0f), Color_SeaGreen3);
			}
		}
		else
		{
			printf("Couldn't call CPathServer::GetWaterEdgePoints() - it was busy.\n");
		}
	}

	if(PathTest_bGetPointsFromMeasuringTool)
	{
		ms_vPathTestStartPos = CPhysics::g_vClickedPos[0];
		ms_vPathTestEndPos = CPhysics::g_vClickedPos[1];
	}

#endif
}
#endif

#if !__FINAL
void
CPathServer::VisualiseMemoryUsage(int UNUSED_PARAM(ypos), aiNavDomain domain)
{
	Vector3 v = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
	if(m_bVisualiseHierarchicalNodes)
	{
#if __HIERARCHICAL_NODES_ENABLED
		m_pNavNodesStore->Visualise(&v);
#endif
	}
	else
	{

		m_pNavMeshStores[domain]->Visualise(&v);
	}
}

void GetQuadTreeDetails(CNavMeshQuadTree * pTree, int & iQuadTreeNumPolyIndices, int & iQuadTreeNumCoverPoints)
{
	if(pTree->m_pLeafData)
	{
		iQuadTreeNumPolyIndices += pTree->m_pLeafData->m_iNumPolys;
		iQuadTreeNumCoverPoints += pTree->m_pLeafData->m_iNumCoverPoints;
		return;
	}
	for(int c=0; c<4; c++)
	{
		if(pTree->m_pChildren[c])
		{
			GetQuadTreeDetails(pTree->m_pChildren[c], iQuadTreeNumPolyIndices, iQuadTreeNumCoverPoints);
		}
	}
}

void CPathServer::VisualisePolygonInfo(int ypos, CNavMesh * pNavMesh, const Vector3 & vOrigin, aiNavDomain domain)
{
	char tmp[256];
	float fX = g_vPathserverDebugTopLeft.x + (SCREEN_WIDTH / 40.0f) + 8;
	int ySpacing = grcDebugDraw::GetScreenSpaceTextHeight();

	Vector3 vIntersect;
	u32 iPoly = pNavMesh->GetPolyBelowPoint(vOrigin, vIntersect, 50.0f);
	if(iPoly == NAVMESH_POLY_INDEX_NONE)
		return;
	TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPoly);
	if(pPoly->GetReplacedByTessellation())
	{
		pNavMesh = fwPathServer::GetTessellationNavMesh();
		iPoly = pNavMesh->GetPolyBelowPoint(vOrigin, vIntersect, 50.0f);
		if(iPoly == NAVMESH_POLY_INDEX_NONE)
			return;
		pPoly = pNavMesh->GetPoly(iPoly);
	}

	sprintf(tmp, "Poly underfoot: 0x%p ( %i : %i )", pPoly, pPoly->GetNavMeshIndex(), iPoly);
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, tmp);
	ypos += ySpacing;

	sprintf(tmp, "Num verts: %i, Flags: 0x%X", pPoly->GetNumVertices(), pPoly->GetFlags());
	grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, tmp);
	ypos += ySpacing;
	ypos += ySpacing;

	Vector3 vVec;

	for(int a=0; a<pPoly->GetNumVertices(); a++)
	{
		const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly((u32) pPoly->GetFirstVertexIndex() + a);

		pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, a), vVec );

		sprintf(tmp, "  %i) Adjacent ( %i : %i )  Original ( %i : %i )",
			a, adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes()), adjPoly.GetPolyIndex(),
			adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes()), adjPoly.GetOriginalPolyIndex() );
		grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, tmp);
		ypos += ySpacing;

		if(adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes()) != NAVMESH_NAVMESH_INDEX_NONE)
		{
			CNavMesh * pAdjNavMesh = GetNavMeshFromIndex( adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes()), domain);
			if(pAdjNavMesh)
			{
				TNavMeshPoly * pAdjPoly = pAdjNavMesh->GetPoly(adjPoly.GetPolyIndex());

				sprintf(tmp, "      0x%p  Flags: 0x%X", pAdjPoly, pAdjPoly->GetFlags());
				grcDebugDraw::Text(ScreenPos((float)fX,(float)ypos), Color_white, tmp);
				ypos += ySpacing;
			}
		}
	}

	DrawPolyDebug(pNavMesh, iPoly, Color_magenta, 0.1f, true, 255, true);

	for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
	{
		pPoly = pNavMesh->GetPoly(p);

		Vector3 vCentroid;
		pNavMesh->GetPolyCentroid(p, vCentroid);
		if((vCentroid - vOrigin).Mag2() > 50.0f*50.0f)
			continue;

		for(int a=0; a<pPoly->GetNumVertices(); a++)
		{
			const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly((u32) pPoly->GetFirstVertexIndex()+a);
			pNavMesh->GetVertex( pNavMesh->GetPolyVertexIndex(pPoly, a), vVec );

			float fFreeSpace = adjPoly.GetFreeSpaceAroundVertex();
			if(fFreeSpace > 0.0f)
				grcDebugDraw::Sphere(vVec, fFreeSpace, Color_SeaGreen2, false);
			else
				grcDebugDraw::Sphere(vVec, 0.2f, Color_red, false);
		}
	}
}

int CPathServer::VisualiseCurrentNavMeshDetails(int ypos, CNavMesh * pNavMesh)
{
	Vector3 vMin = pNavMesh->GetNonResourcedData()->m_vMinOfNavMesh;
	Vector3 vMax = pNavMesh->GetNonResourcedData()->m_vMinOfNavMesh + pNavMesh->GetExtents();

	Color32 iCol = Color_yellow1;
	iCol.SetAlpha(128);

	grcDebugDraw::BoxAxisAligned(
		RCC_VEC3V(vMin),
		RCC_VEC3V(vMax),
		iCol, false);

	const float fStep = pNavMesh->GetExtents().x / 10.0f;
	float fVal=0.0f;

	for(int i=1; i<10; i++)
	{
		grcDebugDraw::Line( Vector3(vMin.x, vMin.y+fVal, vMin.z), Vector3(vMin.x, vMin.y+fVal, vMax.z), iCol );
		grcDebugDraw::Line( Vector3(vMax.x, vMin.y+fVal, vMin.z), Vector3(vMax.x, vMin.y+fVal, vMax.z), iCol );
		grcDebugDraw::Line( Vector3(vMin.x+fVal, vMin.y, vMin.z), Vector3(vMin.x+fVal, vMin.y, vMax.z), iCol );
		grcDebugDraw::Line( Vector3(vMin.x+fVal, vMax.y, vMin.z), Vector3(vMin.x+fVal, vMax.y, vMax.z), iCol );

		fVal += fStep;
	}

	// compressed vertices
	int iVertexMem = pNavMesh->GetNumVertices() * 6;
	int iPolyMem = pNavMesh->GetNumPolys() * sizeof(TNavMeshPoly);
	// u16 for each vertex index
	int iVertexIndexPoolMem = pNavMesh->GetSizeOfPools() * 2;
	int iAdjPolyPoolMem = pNavMesh->GetSizeOfPools() * sizeof(TAdjPoly);

	int iQuadTreeNumPolyIndices = 0;
	int iQuadTreeNumCoverPoints = 0;
	GetQuadTreeDetails(pNavMesh->GetQuadTree(), iQuadTreeNumPolyIndices, iQuadTreeNumCoverPoints);
	int iQuadTreePolyIndexMem = iQuadTreeNumPolyIndices * 2;
	int iQuadTreeCoverPointsMem = iQuadTreeNumCoverPoints * sizeof(CNavMeshCoverPoint);

	int iSpecialLinksMem = pNavMesh->GetNumSpecialLinks() * sizeof(CSpecialLinkInfo);
	int iPolyVolumeInfoMem = pNavMesh->GetNumPolys() * sizeof(TPolyVolumeInfo);

	int iTotal =
		iVertexMem + iPolyMem + iVertexIndexPoolMem + iAdjPolyPoolMem +
		iQuadTreePolyIndexMem + iQuadTreeCoverPointsMem +
		iSpecialLinksMem + iPolyVolumeInfoMem;

	char txt[256];

	Vector2 vTextPos(g_vPathserverDebugTopLeft.x, (float)ypos);
	int iTextInc = grcDebugDraw::GetScreenSpaceTextHeight();

	sprintf(txt, "%i Vertices : %i kb", pNavMesh->GetNumVertices(), iVertexMem/1024);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	sprintf(txt, "%i Polys : %i kb", pNavMesh->GetNumPolys(), iPolyMem/1024);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	sprintf(txt, "%i VertexIndices : %i kb", pNavMesh->GetSizeOfPools(), iVertexIndexPoolMem/1024);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	sprintf(txt, "%i AdjPolys : %i kb", pNavMesh->GetSizeOfPools(), iAdjPolyPoolMem/1024);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	sprintf(txt, "%i QuadTree PolyIndeces : %i kb", iQuadTreeNumPolyIndices, iQuadTreePolyIndexMem/1024);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	sprintf(txt, "%i QuadTree CoverPoints : %i kb", iQuadTreeNumCoverPoints, iQuadTreeCoverPointsMem/1024);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	sprintf(txt, "%i SpecialLinks : %i kb", pNavMesh->GetNumSpecialLinks(), iSpecialLinksMem/1024);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	sprintf(txt, "Total Memory (excluding CNavMesh itself) : %i kb (%.1fmb)", iTotal/1024, ((float)(iTotal/1024))/1024.0f);
	grcDebugDraw::Text(ScreenPos(vTextPos.x, vTextPos.y), Color_white, txt);
	vTextPos.y += iTextInc;

	return (s32)vTextPos.y;
}

#ifdef GTA_ENGINE
void
CPathServer::VisualiseTestPathEndPoints()
{
	Vector3 vEndPoints[3] = { ms_vPathTestStartPos, ms_vPathTestEndPos, ms_vPathTestCoverOrigin };
	const float s = 0.25f;
	
	static int iFlashTime = 0;
	static bool bFlashToggle = false;
	if(fwTimer::GetTimeInMilliseconds() - iFlashTime > 150)
	{
		iFlashTime = fwTimer::GetTimeInMilliseconds();
		bFlashToggle = !bFlashToggle;
	}
	Color32 iCols[3] = {
		bFlashToggle ? Color_LightSeaGreen : Color_white,
		bFlashToggle ? Color_OrangeRed: Color_white,
		bFlashToggle ? Color_red: Color_red3
	};

	for(int i=0; i<3; i++)
	{
		if(!vEndPoints[i].IsZero())
		{
			grcDebugDraw::Line(vEndPoints[i]-Vector3(s, 0.0f, 0.0f), vEndPoints[i]+Vector3(s, 0.0f, 0.0f), iCols[i]);
			grcDebugDraw::Line(vEndPoints[i]-Vector3(0.0f, s, 0.0f), vEndPoints[i]+Vector3(0.0f, s, 0.0f), iCols[i]);
			grcDebugDraw::Line(vEndPoints[i]-Vector3(0.0f, 0.0f, s), vEndPoints[i]+Vector3(0.0f, 0.0f, s), iCols[i]);

			if(i==0 || i==1)
			{
				Color32 iCol = iCols[i];
				iCol.SetAlpha(128);
				grcDebugDraw::Sphere(vEndPoints[i], ms_fPathTestEntityRadius, iCol, false);
			}
		}
	}

	Color32 iRefVecCol = bFlashToggle ? Color_yellow1 : Color_tan;
	grcDebugDraw::Line(ms_vPathTestStartPos, ms_vPathTestStartPos+(ms_vPathTestReferenceVector*ms_fPathTestReferenceDist), iRefVecCol);

	Color32 iSphereCol = bFlashToggle ? Color_GreenYellow : Color_LightSeaGreen;
	for(int s=0; s<CPathServer::ms_iPathTestNumInfluenceSpheres; s++)
	{
		grcDebugDraw::Sphere(CPathServer::ms_PathTestInfluenceSpheres[s].GetOrigin(), CPathServer::ms_PathTestInfluenceSpheres[s].GetRadius(), iSphereCol, false);
	}
}
#endif	// GTA_ENGINE

#endif // !__FINAL



void AddUserDynamicObjectAtCameraPos()
{
	if(!camInterface::GetDebugDirector().IsFreeCamActive())
		return;
	const Vector3 vOrigin = camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition();
	const Vector3 vFwd = camInterface::GetDebugDirector().GetFreeCamFrame().GetFront();
	const float fHeading = fwAngle::GetRadianAngleBetweenPoints(vFwd.x, vFwd.y, 0.0f, 0.0f);

	static Vector3 vSize(10.0f, 5.0f, 5.0f);
	TDynamicObjectIndex iObj = CPathServerGta::AddBlockingObject(vOrigin, vSize, fHeading, TDynamicObject::BLOCKINGOBJECT_ALL_PATH_TYPES);
	
	Displayf("TDynamicObject = 0x%x", iObj);
}



void PathServer_ToggleDrawPathNodes(void)
{

}

#if __BANK
void OnSelectObjAvoidanceCombo(void)
{
	CPathServer::m_eObjectAvoidanceMode = (CPathServer::EObjectAvoidanceMode)CPathServer::m_iObjAvoidanceModeComboIndex;
}
#endif

void PathSever_VerifyAdjacencies()
{
	LOCK_NAVMESH_DATA;
	LOCK_STORE_LOADED_MESHES;

	aiNavMeshStore * pStore = CPathServer::GetNavMeshStore(kNavDomainRegular);
	const atArray<s32> & meshes = pStore->GetLoadedMeshes();

	for(s32 m=0; m<meshes.GetCount(); m++)
	{
		s32 iMesh = meshes[m];
		CNavMesh * pMesh = CPathServer::GetNavMeshFromIndex(iMesh, kNavDomainRegular);
		if(pMesh)
		{
			for(s32 p=0; p<pMesh->GetNumPolys(); p++)
			{
				TNavMeshPoly * pPoly = pMesh->GetPoly(p);

				for(s32 a=0; a<pPoly->GetNumVertices(); a++)
				{
					const TAdjPoly & adjPoly = pMesh->GetAdjacentPoly( pMesh->GetPolyVertexIndex( pPoly, a ) );
					const s32 iAdjacentMesh = adjPoly.GetNavMeshIndex(pMesh->GetAdjacentMeshes());
					if(iAdjacentMesh != NAVMESH_NAVMESH_INDEX_NONE)
					{
						Assert(iAdjacentMesh != NAVMESH_INDEX_TESSELLATION);	// no tessellations, since this is outside of a pathsearch
						
						CNavMesh * pAdjMesh = CPathServer::GetNavMeshFromIndex(iAdjacentMesh, kNavDomainRegular);
						if(pAdjMesh)
						{
							Assert(adjPoly.GetPolyIndex() < pAdjMesh->GetNumPolys());
						}
					}
				}
			}
		}
	}

	UNLOCK_NAVMESH_DATA
	UNLOCK_STORE_LOADED_MESHES
}

/*
void PathServer_TogglePathServerThreadRunning(void)
{
#ifdef GTA_ENGINE
#if !__FINAL
#if __BANK
	// Rage widgets toggle the boolean variable they're associated with before calling the associated callback.
	// This isn't what we want, so we have to flip the variable again here!
	CPathServer::ms_bPathServerThreadIsRunning = !CPathServer::ms_bPathServerThreadIsRunning;

	if(CPathServer::ms_bPathServerThreadIsRunning)
	{
		CPathServer::SuspendThread();
	}
	else
	{
		CPathServer::ResumeThread();
	}
#endif
#endif
#endif
}
*/
void PathServer_RepeatPathQuery(void)
{
#ifdef GTA_ENGINE
#if !__FINAL
#if __BANK
	CPathRequest * pRequest = CPathServer::GetPathRequest(CPathServer::m_iSelectedPathRequest);

	pRequest->m_bComplete = false;
	pRequest->m_bRequestPending = true;
	pRequest->m_bSlotEmpty = false;
	pRequest->m_hHandle = 1;
#endif
#endif
#endif
}

void PathServer_OutputPathQuery(void)
{
#ifdef GTA_ENGINE
#if !__FINAL && __BANK
	CPathServer::OutputPathRequestProblem(CPathServer::m_iSelectedPathRequest);
#endif
#endif
}

void PathServer_RequestPathButDontRetrieve(void)
{
#ifdef GTA_ENGINE
#if !__FINAL && __BANK
	Vector3 vStart(0.0f,0.0f,0.0f);
	Vector3 vEnd(10.0f,10.0f,0.0f);
	CPathServer::RequestPath(vStart, vEnd, 0);
	//#endif
#endif
#endif
}

void PathServer_TestLosStackOverflow()
{
#ifdef GTA_ENGINE
#if !__FINAL && __BANK
	// Navmesh must be [62,74]
	// Teleport to (152,710,11)

	Vector3 vStart(169.735565f, 680.479065f, 7.24429321f);
	Vector3 vEnd(168.991272f, 662.717468f, 7.14869213f);

	TPathHandle h = CPathServer::RequestLineOfSight(vStart, vEnd, 0.0f);
	while(CPathServer::IsRequestResultReady(h)==ERequest_NotReady)
	{
		sysIpcYield(0);
	}
	bool bRet;
	CPathServer::GetLineOfSightResult(h, bRet);
	//#endif
#endif
#endif
}

void PathServer_OnSelectPrevPathRequest()
{
	switch(CPathServer::m_iVisualisePathServerInfo)
	{
		case CPathServer::PathServerVis_PathRequests:
			CPathServer::m_iSelectedPathRequest--;
			if(CPathServer::m_iSelectedPathRequest < 0) CPathServer::m_iSelectedPathRequest = MAX_NUM_PATH_REQUESTS-1;
			break;
		case CPathServer::PathServerVis_LineOfSightRequests:
			CPathServer::m_iSelectedLosRequest--;
			if(CPathServer::m_iSelectedLosRequest < 0) CPathServer::m_iSelectedLosRequest = MAX_NUM_LOS_REQUESTS-1;
			break;
	}
}

void PathServer_OnSelectNextPathRequest()
{
	switch(CPathServer::m_iVisualisePathServerInfo)
	{
		case CPathServer::PathServerVis_PathRequests:
			CPathServer::m_iSelectedPathRequest++;
			if(CPathServer::m_iSelectedPathRequest >= MAX_NUM_PATH_REQUESTS) CPathServer::m_iSelectedPathRequest = 0;
			break;
		case CPathServer::PathServerVis_LineOfSightRequests:
			CPathServer::m_iSelectedLosRequest++;
			if(CPathServer::m_iSelectedLosRequest >= MAX_NUM_LOS_REQUESTS) CPathServer::m_iSelectedLosRequest = 0;
			break;
	}
}

void CPathServer::DebugDetessellateNow()
{
	fwPathServer::DetessellateAllPolys();
}

#ifdef GTA_ENGINE
#if !__FINAL
#if __BANK

void PathServer_TestColPolyVsPaths()
{
	CPed * pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		Vector3 vPts[3];
		vPts[0] = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
		vPts[1] = vPts[0] + XAXIS;
		vPts[2] = vPts[0] + YAXIS;

		ThePaths.CollisionTriangleIntersectsRoad(vPts, false);		
	}
}

#endif
#endif
#endif

void PathServer_TestLOS()
{
#if defined(GTA_ENGINE) && !__FINAL && __BANK && __WIN32PC && (!IS_CONSOLE)

	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	Vector3 vStart = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if(!debugDirector.IsFreeCamActive()) return;
	const Vector3& vEnd = debugDirector.GetFreeCamFrame().GetPosition();

	TPathHandle hLosPath = CPathServer::RequestLineOfSight(vStart, vEnd, 0.0f);
	while(CPathServer::IsRequestResultReady(hLosPath)==ERequest_NotReady) {
		Sleep(0);
	}

	bool bIsLosClear = false;
	EPathServerErrorCode eRet = CPathServer::GetLineOfSightResult(hLosPath, bIsLosClear);
	eRet;

#endif
}

void PathServer_TestDebugPolyUnderCamera()
{
#if defined(GTA_ENGINE) && __BANK && __DEV

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if(!debugDirector.IsFreeCamActive()) return;
	const Vector3& vPos = debugDirector.GetFreeCamFrame().GetPosition();

	const aiNavDomain domain = kNavDomainRegular;

	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex( CPathServer::GetNavMeshIndexFromPosition(vPos, domain), domain );
	if(!pNavMesh)
		return;

	int iMeshX, iMeshY;
	CPathServer::GetNavMeshStore(domain)->MeshIndexToMeshCoords(pNavMesh->GetIndexOfMesh(), iMeshX, iMeshY);
	iMeshX *= CPathServerExtents::m_iNumSectorsPerNavMesh;
	iMeshY *= CPathServerExtents::m_iNumSectorsPerNavMesh;

	char tmp[256];
	sprintf(tmp, "NavMesh [%i][%i]\n", iMeshX, iMeshY);
	Displayf("%s", tmp);

	Vector3 vIsect;
	u32 iPolyIndex = pNavMesh->GetPolyBelowPoint(vPos, vIsect, 10.0f);
	if(iPolyIndex != NAVMESH_POLY_INDEX_NONE)
	{
		TNavMeshPoly * pPoly = pNavMesh->GetPoly(iPolyIndex);
		Assert(pPoly->GetNavMeshIndex() == pNavMesh->GetIndexOfMesh());

		sprintf(tmp, "NavMesh %i Poly %i (addr:0x%p)\n", pNavMesh->GetIndexOfMesh(), iPolyIndex, pPoly);
		Displayf("%s", tmp);

		sprintf(tmp, "NumVertices %i, PedDensity %i, Flags %i\n", pPoly->GetNumVertices(), pPoly->GetPedDensity(), pPoly->GetFlags());
		Displayf("%s", tmp);

		sprintf(tmp, "Area %.2f\n", pNavMesh->GetPolyArea(iPolyIndex));
		Displayf("%s", tmp);

		Vector3 vPolyNormal;
		pNavMesh->GetPolyNormal(vPolyNormal, pPoly);
		sprintf(tmp, "PolyNormal (%.2f, %.2f, %.2f)\n", vPolyNormal.x, vPolyNormal.y, vPolyNormal.z);
		Displayf("%s", tmp);


		u32 testFlags = NAVMESHPOLY_SMALL | NAVMESHPOLY_LARGE | NAVMESHPOLY_OPEN | NAVMESHPOLY_CLOSED | NAVMESHPOLY_TESSELLATED_FRAGMENT | NAVMESHPOLY_REPLACED_BY_TESSELLATION;

		if(pPoly->GetIsSmall()) Displayf("(small)");
		if(pPoly->GetIsLarge()) Displayf("(large)");
		if(pPoly->GetIsOpen()) Displayf("(open)");
		if(pPoly->GetIsClosed()) Displayf("(closed)");
		if(pPoly->GetIsTessellatedFragment()) Displayf("(frag)");
		if(pPoly->GetReplacedByTessellation()) Displayf("(replaced)");

		if(pPoly->TestFlags(testFlags)) Displayf("\n");

		Vector3 vert, nextVert, vDiff, vEdgeNormal;
		int nextv, vi1, vi2;
		u32 v;

		Vector3 vMins(FLT_MAX, FLT_MAX, FLT_MAX);
		Vector3 vMaxs(-FLT_MAX, -FLT_MAX, -FLT_MAX);

		for(v=0; v<pPoly->GetNumVertices(); v++)
		{
			nextv = (v+1) % pPoly->GetNumVertices();

			vi1 = pNavMesh->GetPolyVertexIndex(pPoly, v);
			vi2 = pNavMesh->GetPolyVertexIndex(pPoly, nextv);

			pNavMesh->GetVertex(vi1, vert);
			pNavMesh->GetVertex(vi2, nextVert);

			if(vert.x < vMins.x) vMins.x = vert.x;
			if(vert.y < vMins.y) vMins.y = vert.y;
			if(vert.z < vMins.z) vMins.z = vert.z;
			if(vert.x > vMaxs.x) vMaxs.x = vert.x;
			if(vert.y > vMaxs.y) vMaxs.y = vert.y;
			if(vert.z > vMaxs.z) vMaxs.z = vert.z;

			vDiff = nextVert - vert;

			const TAdjPoly & adjPoly = pNavMesh->GetAdjacentPoly((u32) pPoly->GetFirstVertexIndex()+v);
			pNavMesh->GetPolyEdgeNormal(vEdgeNormal, pPoly, v);

			sprintf(tmp, "%i) [%i] (%.1f, %.1f, %.1f) \t adjType:%i (adjNav:%i, adjPoly:%i, origAdjNav:%i, origAdjPoly:%i) length : %.2f normal : (%.2f, %.2f, %.2f)\n",
				v,
				vi1,
				vert.x, vert.y, vert.z,
				adjPoly.GetAdjacencyType(),
				adjPoly.GetNavMeshIndex(pNavMesh->GetAdjacentMeshes()), adjPoly.GetPolyIndex(),
				adjPoly.GetOriginalNavMeshIndex(pNavMesh->GetAdjacentMeshes()), adjPoly.GetOriginalPolyIndex(),
				vDiff.Mag(),
				vEdgeNormal.x, vEdgeNormal.y, vEdgeNormal.z
				);
			Displayf("%s", tmp);
		}

		Vector3 vSize = vMaxs - vMins;
		sprintf(tmp, "Mins (%.1f, %.1f, %.1f) Maxs (%.1f, %.1f, %.1f) Size (%.1f, %.1f, %.1f)\n",
			vMins.x, vMins.y, vMins.z, vMaxs.x, vMaxs.y, vMaxs.z, vSize.x, vSize.y, vSize.z);
		Displayf("%s", tmp);
	}
#endif
}

void PathServer_TestAudioPropertiesRequest()
{
#if defined(GTA_ENGINE) && __BANK

	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	TPathHandle hAudioReq = CPathServer::RequestAudioProperties(vPos);
	while(CPathServer::IsRequestResultReady(hAudioReq)==ERequest_NotReady)
	{
		sysIpcYield(0);
	}

	u32 iAudioProperties;
	EPathServerErrorCode eRet = CPathServer::GetAudioPropertiesResult(hAudioReq, iAudioProperties);

	if(eRet==PATH_NO_ERROR)
	{
		printf("AudioProperties:%i\n", iAudioProperties);
	}
	else
	{
		printf("AudioProperties couldn't be queried.  No navmesh poly nearby?\n");
	}

	(void)eRet;	// stop VS from complaining

#endif
}

void PathServer_TestClearAreaRequest()
{
#if defined(GTA_ENGINE) && __BANK

	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	static float fSearchRadius = 100.0f;
	static float fDesiredClearRadius = 10.0f;
	static float fMinimumRadius = 0.0f;

	static bool bTestObjects = true;
	static bool bInteriors = false;
	static bool bExteriors = true;
	static bool bWater = false;
	static bool bSheltered = false;

	u32 iFlags = 0;
	if(bTestObjects) iFlags |= CNavmeshClearAreaHelper::Flag_TestForObjects;
	if(bInteriors) iFlags |= CNavmeshClearAreaHelper::Flag_AllowInteriors;
	if(bExteriors) iFlags |= CNavmeshClearAreaHelper::Flag_AllowExterior;
	if(bWater) iFlags |= CNavmeshClearAreaHelper::Flag_AllowWater;
	if(bSheltered) iFlags |= CNavmeshClearAreaHelper::Flag_AllowSheltered;

	grcDebugDraw::Sphere(vPos, fSearchRadius, Color_orange3, false, 50);

	CNavmeshClearAreaHelper searchHelper;
	searchHelper.StartClearAreaSearch(vPos, fSearchRadius, fSearchRadius, fDesiredClearRadius, iFlags, fMinimumRadius);

	Vector3 vClearPos;
	SearchStatus ss;
	while(!searchHelper.GetSearchResults( ss, vClearPos ))
	{
		sysIpcYield(0);
	}

	if(ss == SS_SearchSuccessful)
	{
		grcDebugDraw::Sphere(vClearPos, fDesiredClearRadius, Color_cyan, false, 50);
		printf("Clear position at (%.1f, %.1f, %.1f)\n", vClearPos.x, vClearPos.y, vClearPos.z);
	}
	else
	{
		printf("Failed to find clear position\n");
	}

#endif
}

void PathServer_TestClosestPositionRequest()
{
#if defined(GTA_ENGINE) && __BANK

	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	static float fSearchRadius = 100.0f;

	static u32 iFlags = CClosestPositionRequest::Flag_ConsiderInterior|CClosestPositionRequest::Flag_ConsiderExterior|CClosestPositionRequest::Flag_ConsiderDynamicObjects;
	static float fMinimumSpacing = 0.0f;
	static float fZWeightingAbove = 1.0f;
	static float fZWeightingAtOrBelow = 1.0f;
	static s32 iMaxNumResults = CClosestPositionRequest::MAX_NUM_RESULTS;
	static s32 iNumAvoidSpheres = 0;
	
	spdSphere avoidSpheres[1];
	static float fSphereRadius = 10.0f;
	avoidSpheres[0].Set(VECTOR3_TO_VEC3V(vPos), ScalarV(fSphereRadius));

	CNavmeshClosestPositionHelper helper;
	helper.StartClosestPositionSearch(vPos, fSearchRadius, iFlags, fZWeightingAbove, fZWeightingAtOrBelow, fMinimumSpacing, iNumAvoidSpheres, avoidSpheres, iMaxNumResults);

	s32 iNumPositions;
	Vector3 vPositions[CClosestPositionRequest::MAX_NUM_RESULTS];
	SearchStatus ss;

	while(1)
	{
		sysIpcYield(0);
		if(helper.GetSearchResults(ss, iNumPositions, vPositions, CClosestPositionRequest::MAX_NUM_RESULTS))
		{
			if(ss == SS_SearchSuccessful)
			{
				Vector3 vRGB(0.0f,1.0f,0.0f);
				for(s32 r=0; r<iNumPositions; r++)
				{
					Color32 col(vRGB);
					grcDebugDraw::Sphere(vPositions[r] + ZAXIS * 0.5f, 0.5f, col, false, 500, 16);
					vRGB *= 0.9f;
				}
			}
			break;
		}
	}

	/*
	TPathHandle hReq = CPathServer::RequestClosestPosition(vPos, fSearchRadius, iFlags, fZWeightingAbove, fZWeightingAtOrBelow, fMinimumSpacing, iNumAvoidSpheres, avoidSpheres, iMaxNumResults, NULL, kNavDomainRegular);
	if(hReq != PATH_HANDLE_NULL)
	{
		while(!CPathServer::IsRequestResultReady(hReq))
		{
			sysIpcYield(0);
		}

		CClosestPositionRequest::TResults results;
		EPathServerErrorCode ret = CPathServer::GetClosestPositionResultAndClear(hReq, results);
		
		if(ret == PATH_FOUND)
		{
			Vector3 vRGB(0.0f,1.0f,0.0f);
			for(s32 r=0; r<results.m_iNumResults; r++)
			{
				Color32 col(vRGB);
				grcDebugDraw::Sphere(results.m_vResults[r] + ZAXIS * 0.5f, 0.5f, col, false, 500, 16);
				vRGB *= 0.9f;
			}
		}
	}
	*/

#endif
}

void TestEvictDynamicObjectForScript()
{
	s32 iObjectIndex = CPathServer::EvictDynamicObjectForScript();
	if(iObjectIndex != -1)
	{
		const TDynamicObject & obj = CPathServer::GetPathServerThread()->GetDynamicObject((TDynamicObjectIndex)iObjectIndex);
		Vector3 vObjPos = obj.m_Bounds.GetOrigin();

		grcDebugDraw::Sphere(vObjPos, 4.0, Color_green, false, 4000, 12);
		grcDebugDraw::Arrow(FindPlayerPed()->GetTransform().GetPosition(), VECTOR3_TO_VEC3V(vObjPos), 0.5f, Color_green, 4000);
	}
}

void PathServer_TestDisablePolysAroundCamera()
{
	static float fSize = 8.0f;

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if(!debugDirector.IsFreeCamActive()) return;
	const Vector3& vPos = debugDirector.GetFreeCamFrame().GetPosition();

	Vector3 vMin = vPos - Vector3(fSize,fSize,fSize);
	Vector3 vMax = vPos + Vector3(fSize,fSize,fSize);
	CPathServer::DisableNavMeshInArea(vMin, vMax, THREAD_INVALID);
}
void PathServer_TestTurnOffPolysForPedsAroundCamera()
{
	static float fSize = 8.0f;

	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if(!debugDirector.IsFreeCamActive()) return;
	const Vector3& vPos = debugDirector.GetFreeCamFrame().GetPosition();

	Vector3 vMin = vPos - Vector3(fSize,fSize,fSize);
	Vector3 vMax = vPos + Vector3(fSize,fSize,fSize);
	CPathServer::SwitchPedsOffInArea(vMin, vMax, THREAD_INVALID);
}


void PathServer_TestFindClosestPos(
#if __BANK && !__FINAL && defined(GTA_ENGINE)
	bool bOnPavement, bool bClearOfObjects, bool bExtraThorough, bool bUseFloodFill
#else
	bool, bool, bool, bool
#endif
								   )
{
#if __BANK && !__FINAL && defined(GTA_ENGINE)
	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	u32 iFlags = 0;
	if(bOnPavement) iFlags |= GetClosestPos_OnlyPavement;
	if(bClearOfObjects) iFlags |= GetClosestPos_ClearOfObjects;
	if(bExtraThorough) iFlags |= GetClosestPos_ExtraThorough;
	if(bUseFloodFill) iFlags |= GetClosestPos_UseFloodFill;

	sysPerformanceTimer timer("GetClosestPosForPed");
	timer.Start();

	Vector3 vOut;
	static const float fFindPolyRadius = 30.0f;
	EGetClosestPosRet eRet = CPathServer::GetClosestPositionForPed(vPos, vOut, fFindPolyRadius, iFlags);

	timer.Stop();
	const float fTimeTaken = timer.GetTimeMS();
	Printf("CPathServer::GetClosestPositionForPed with radius %.1fm took : %.4f ms\n", fFindPolyRadius, fTimeTaken);

	if(eRet!=ENoPositionFound)
	{
		grcDebugDraw::Line(vOut - Vector3(1.0f, 0.0f, 0.0f), vOut + Vector3(1.0f, 0.0f, 0.0f), Color_red3);
		grcDebugDraw::Line(vOut - Vector3(0.0f, 1.0f, 0.0f), vOut + Vector3(0.0f, 1.0f, 0.0f), Color_red3);
		grcDebugDraw::Line(vOut, vOut + Vector3(0.0f, 0.0f, 3.0f), Color_red3, Color_red2);
	}

#endif
}

void PathServer_TestFindClosestPosOnPavement() { PathServer_TestFindClosestPos(true, false, false, false); }
void PathServer_TestFindClosestPosOnAnySurface() { PathServer_TestFindClosestPos(false, false, false, false); }
void PathServer_TestFindClosestPosOnPavementClearOfObjects() { PathServer_TestFindClosestPos(true, true, false, false); }
void PathServer_TestFindClosestPosOnAnySurfaceClearOfObjects() { PathServer_TestFindClosestPos(false, true, false, false); }
void PathServer_TestFindClosestPosOnAnySurfaceClearOfObjectsExtraThorough() { PathServer_TestFindClosestPos(false, true, true, true); }

#if __BANK && defined(GTA_ENGINE)
void PathServer_AddRequiredRegion()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if(!debugDirector.IsFreeCamActive()) return;
	const Vector3& vPos = debugDirector.GetFreeCamFrame().GetPosition();
	CPathServer::AddNavMeshRegion( NMR_Script, static_cast<scrThreadId>(0xABCD), vPos.x, vPos.y, 250.0f );
}
void PathServer_RemoveRequiredRegion()
{
	CPathServer::RemoveNavMeshRegion( NMR_Script, static_cast<scrThreadId>(0xABCD) );
}
#endif

void PathServer_TestFloodFillToClosestCarNode()
{
#ifdef GTA_ENGINE
	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());

	Vector3 vOut;
	static const float fMaxRadius = 50.0f;

	TPathHandle hHandle = CPathServer::RequestClosestCarNodeSearch(vPos, fMaxRadius, pPlayer);
	if(hHandle != PATH_HANDLE_NULL)
	{
		while(CPathServer::IsRequestResultReady(hHandle)==ERequest_NotReady)
		{
			sysIpcSleep(0);
		}

		Vector3 vClosestCarNode;
		EPathServerErrorCode errCode = CPathServer::GetClosestCarNodeSearchResultAndClear(hHandle, vClosestCarNode);
		if(errCode==PATH_NO_ERROR)
		{
			grcDebugDraw::Line(vClosestCarNode - Vector3(1.0f, 0.0f, 0.0f), vClosestCarNode + Vector3(1.0f, 0.0f, 0.0f), Color_red3);
			grcDebugDraw::Line(vClosestCarNode - Vector3(0.0f, 1.0f, 0.0f), vClosestCarNode + Vector3(0.0f, 1.0f, 0.0f), Color_red3);
			grcDebugDraw::Line(vClosestCarNode, vClosestCarNode + Vector3(0.0f, 0.0f, 3.0f), Color_red3, Color_red2);
		}
	}
#endif
}


void PathServer_TestTurnOffWandering()
{
#ifdef GTA_ENGINE
	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	const Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	const float fSize = 20.0f;
	const Vector3 vMin = vPos - Vector3(fSize,fSize,fSize);
	const Vector3 vMax = vPos + Vector3(fSize,fSize,fSize);
	CPathServer::SwitchPedsOffInArea(vMin, vMax, THREAD_INVALID);
#endif
}

bool bOpponentPedGen_AllowExterior = true;
bool bOpponentPedGen_AllowInteriors = false;
bool bOpponentPedGen_AllowNonNetworkSpawn = false;
bool bOpponentPedGen_AllowIsolated = false;
bool bOpponentPedGen_AllowRoad = false;
bool bOpponentPedGen_OnlyCoverEdges = false;
float fOpponentPedGen_RadiusXY = 20.0f;
float fOpponentPedGen_CutoffZ = 200.0f;
float fOpponentPedGen_MinimumSpacing = 0.0f;
s32 iOpponentPedGen_TimeoutMS = 0;

void Pathserver_StartOpponentPedGen()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	if(debugDirector.IsFreeCamActive())
	{
		Vector3 vOrigin	= debugDirector.GetFreeCamFrame().GetPosition();

		u32 iFlags = 0;
		if(bOpponentPedGen_AllowExterior) iFlags |= CPathServerOpponentPedGen::FLAG_MAY_SPAWN_IN_EXTERIOR;
		if(bOpponentPedGen_AllowInteriors) iFlags |= CPathServerOpponentPedGen::FLAG_MAY_SPAWN_IN_INTERIOR;
		if(bOpponentPedGen_AllowNonNetworkSpawn) iFlags |= CPathServerOpponentPedGen::FLAG_ALLOW_NOT_NETWORK_SPAWN_CANDIDATE_POLYS;
		if(bOpponentPedGen_AllowIsolated) iFlags |= CPathServerOpponentPedGen::FLAG_ALLOW_ISOLATED_POLYS;
		if(bOpponentPedGen_AllowRoad) iFlags |= CPathServerOpponentPedGen::FLAG_ALLOW_ROAD_POLYS;
		if(bOpponentPedGen_OnlyCoverEdges) iFlags |= CPathServerOpponentPedGen::FLAG_ONLY_POINTS_AGAINST_EDGES;
		
	
		CPathServer::GetOpponentPedGen().StartSearch(vOrigin, fOpponentPedGen_RadiusXY, fOpponentPedGen_CutoffZ, iFlags, fOpponentPedGen_MinimumSpacing, iOpponentPedGen_TimeoutMS);

	}
}
void Pathserver_StartOpponentPedGenAngledArea()
{
	u32 iFlags = 0;
	if(bOpponentPedGen_AllowExterior) iFlags |= CPathServerOpponentPedGen::FLAG_MAY_SPAWN_IN_EXTERIOR;
	if(bOpponentPedGen_AllowInteriors) iFlags |= CPathServerOpponentPedGen::FLAG_MAY_SPAWN_IN_INTERIOR;
	if(bOpponentPedGen_AllowNonNetworkSpawn) iFlags |= CPathServerOpponentPedGen::FLAG_ALLOW_NOT_NETWORK_SPAWN_CANDIDATE_POLYS;
	if(bOpponentPedGen_AllowIsolated) iFlags |= CPathServerOpponentPedGen::FLAG_ALLOW_ISOLATED_POLYS;
	if(bOpponentPedGen_AllowRoad) iFlags |= CPathServerOpponentPedGen::FLAG_ALLOW_ROAD_POLYS;
	if(bOpponentPedGen_OnlyCoverEdges) iFlags |= CPathServerOpponentPedGen::FLAG_ONLY_POINTS_AGAINST_EDGES;

	CPathServer::GetOpponentPedGen().StartSearch(CPhysics::g_vClickedPos[0], CPhysics::g_vClickedPos[1], fOpponentPedGen_RadiusXY, iFlags, fOpponentPedGen_MinimumSpacing, iOpponentPedGen_TimeoutMS);
}

void Pathserver_CompleteOpponentPedGen()
{
	if(!CPathServer::GetOpponentPedGen().IsSearchFailed())
	{
		Vector3 vSpawnPos;
		s32 iNum = CPathServer::GetOpponentPedGen().GetNumResults();
		for(s32 i=0; i<iNum; i++)
		{
			CPathServer::GetOpponentPedGen().GetResult(i, vSpawnPos);
		}
	}
	CPathServer::GetOpponentPedGen().CancelSeach();
}

void PathServer_AddPedGenBlockingArea()
{
#ifdef GTA_ENGINE
	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	const Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	const float fSize = 20.0f;
	CPathServer::GetAmbientPedGen().AddPedGenBlockedArea(vPos, fSize, 10000);
#endif
}

#if __DEV
void CPathServer::PathServer_TestCalcPolyAreas()
{
#ifdef GTA_ENGINE
	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	const Vector3 vPos = VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition());
	const aiNavDomain domain = kNavDomainRegular;
	TNavMeshIndex iIndex = CPathServer::GetNavMeshIndexFromPosition(vPos, domain);
	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iIndex, domain);
	if(pNavMesh)
	{
		for(u32 p=0; p<pNavMesh->GetNumPolys(); p++)
		{
			const float fArea = pNavMesh->GetPolyArea(p);
			Assert(rage::FPIsFinite(fArea));
		}
	}
#endif
}
#endif

/*
void PathServer_TestDecompressVerts()
{
#ifdef GTA_ENGINE
	CPed * pPlayer = FindPlayerPed();
	if(!pPlayer) return;
	const Vector3 vPos = pPlayer->GetPosition();

	TNavMeshIndex iNavMesh = CPathServer::GetNavMeshIndexFromPosition(vPos);
	CNavMesh * pNavMesh = CPathServer::GetNavMeshFromIndex(iNavMesh);
	if(pNavMesh)
	{
		pNavMesh->TestDecompressAllVertices();
	}
#endif
}
*/

/*
#define TESTNUM	512

void PathServer_TestMinMax()
{
	TShortMinMax minMax[TESTNUM];

	fwRandom::SetRandomSeed(12345678);
	for(int t=0; t<TESTNUM; t++)
	{
		minMax[t].m_iMinX = (u16) fwRandom::GetRandomNumberInRange(-4000.0f, 4000.0f);
		minMax[t].m_iMinY = (u16) fwRandom::GetRandomNumberInRange(-4000.0f, 4000.0f);
		minMax[t].m_iMinZ = (u16) fwRandom::GetRandomNumberInRange(-4000.0f, 4000.0f);
		minMax[t].m_iMaxX = (u16) fwRandom::GetRandomNumberInRange(-4000.0f, 4000.0f);
		minMax[t].m_iMaxY = (u16) fwRandom::GetRandomNumberInRange(-4000.0f, 4000.0f);
		minMax[t].m_iMaxZ = (u16) fwRandom::GetRandomNumberInRange(-4000.0f, 4000.0f);

		if(minMax[t].m_iMinX > minMax[t].m_iMaxX) SWAP(minMax[t].m_iMinX, minMax[t].m_iMaxX);
		if(minMax[t].m_iMinY > minMax[t].m_iMaxY) SWAP(minMax[t].m_iMinY, minMax[t].m_iMaxY);
		if(minMax[t].m_iMinZ > minMax[t].m_iMaxZ) SWAP(minMax[t].m_iMinZ, minMax[t].m_iMaxZ);
	}

	int t1,t2;
	static bool bCheckResults=false;
	if(bCheckResults)
	{
		for(t1=0; t1<TESTNUM; t1++)
		{
			for(t2=0; t2<TESTNUM; t2++)
			{
				const bool bResultA = minMax[t1].Intersects(minMax[t2]);
				const bool bResultB = minMax[t1].IntersectsXenon(minMax[t2]);
				Assert(bResultA==bResultB);
			}
		}
	}

	sysPerformanceTimer timer("test");
	timer.Start();

	for(t1=0; t1<TESTNUM; t1++)
	{
		for(t2=0; t2<TESTNUM; t2++)
		{
			if(CNavMesh::ms_bUseVmxOptimisations)
			{
				minMax[t1].IntersectsXenon(minMax[t2]);
			}
			else
			{
				minMax[t1].Intersects(minMax[t2]);
			}
		}
	}

	timer.Stop();
	float fTimeMs = timer.GetTimeMS();
	printf("Time taken %.4f\n", fTimeMs);
}
*/

//***********************************************************************************
//	PathServer_SetUpPerfTest
//	This simply sets up a repeatable situation in GTAIV.  The player is placed at
//	a fixed location, and is told to navmesh to an unreachable target.  A lot of
//	polys & objects will be visited during the course of the search.
//	The processing times of these paths requests should be a pretty stable yardstick
//	for optimising the pathsearch algorithms.
//***********************************************************************************
void PathServer_SetUpPerfTest()
{
#ifdef GTA_ENGINE

	Vector3 vStartPos(821.0f, -302.0f, 17.0f);
	Vector3 vEndPos(814.8f, -301.5f, 26.0f);

	CPed * pPlayer = FindPlayerPed();
	if(pPlayer)
	{
		pPlayer->Teleport(vStartPos, pPlayer->GetCurrentHeading());

		CTaskMoveFollowNavMesh * pNavTask = rage_new CTaskMoveFollowNavMesh(MOVEBLENDRATIO_RUN, vEndPos);
		// Don't limit extents - visit as many polys/objects as possible
		pNavTask->SetUseLargerSearchExtents(true);

		CTaskComplexControlMovement * pCtrlMove = rage_new CTaskComplexControlMovement(pNavTask);

		pPlayer->GetPedIntelligence()->AddTaskAtPriority(pCtrlMove, PED_TASK_PRIORITY_PRIMARY, true);
	}

#endif
}

void OnSelectThreadRunModeCombo()
{
	#ifdef GTA_ENGINE
	Assert(CPathServer::m_iDebugThreadRunMode==0 || CPathServer::m_iDebugThreadRunMode==1);

	if(CPathServer::m_iDebugThreadRunMode==0)
	{
		CPathServer::SuspendThread();
	}
	else
	{
		CPathServer::ResumeThread();
	}
#endif
}

#ifdef GTA_ENGINE

u64 InitPathTestFlags()
{
	u64 iFlags=0;
	if(PathTest_bPreferPavements) iFlags |= PATH_FLAG_PREFER_PAVEMENTS;
	if(PathTest_bNeverLeaveWater) iFlags |= PATH_FLAG_NEVER_LEAVE_WATER;
	if(PathTest_bGoAsFarAsPossible) iFlags |= PATH_FLAG_USE_BEST_ALTERNATE_ROUTE_IF_NONE_FOUND;
	if(PathTest_bNeverLeavePavements) iFlags |= PATH_FLAG_NEVER_LEAVE_PAVEMENTS;
	if(PathTest_bNeverClimbOverStuff) iFlags |= PATH_FLAG_NEVER_CLIMB_OVER_STUFF;
	if(PathTest_bNeverDropFromHeight) iFlags |= PATH_FLAG_NEVER_DROP_FROM_HEIGHT;
	if(PathTest_bNeverUseLadders) iFlags |= PATH_FLAG_NEVER_USE_LADDERS;
	if(PathTest_bFleeTarget) iFlags |= PATH_FLAG_FLEE_TARGET;
	if(PathTest_bWander) iFlags |= PATH_FLAG_WANDER;
	if(PathTest_bNeverEnterWater) iFlags |= PATH_FLAG_NEVER_ENTER_WATER;
	if(PathTest_bDontAvoidDynamicObjects) iFlags |= PATH_FLAG_DONT_AVOID_DYNAMIC_OBJECTS;
	if(PathTest_bPreferDownhill) iFlags |= PATH_FLAG_PREFER_DOWNHILL;
	if(PathTest_bReduceObjectBBoxes) iFlags |= PATH_FLAG_REDUCE_OBJECT_BBOXES;
	if(PathTest_bUseLargerSearchExtents) iFlags |= PATH_FLAG_USE_LARGER_SEARCH_EXTENTS;
	if(PathTest_bMayUseFatalDrops) iFlags |= PATH_FLAG_MAY_USE_FATAL_DROPS;
	if(PathTest_bIgnoreObjectsWithNoUprootLimit) iFlags |= PATH_FLAG_IGNORE_NON_SIGNIFICANT_OBJECTS;
	if(PathTest_bCutSharpCorners) iFlags |= PATH_FLAG_CUT_SHARP_CORNERS;
	if(PathTest_bAllowToNavigateUpSteepPolygons) iFlags |= PATH_FLAG_ALLOW_TO_NAVIGATE_UP_STEEP_POLYGONS;
	if(PathTest_bScriptedRoute) iFlags |= PATH_FLAG_SCRIPTED_ROUTE;
	if(PathTest_bPreserveZ) iFlags |= PATH_FLAG_PRESERVE_SLOPE_INFO_IN_PATH;
	if(PathTest_bRandomisePoints) iFlags |= PATH_FLAG_RANDOMISE_POINTS;

	

	if(PathTest_bUseDirectionalCover)
	{
		iFlags |= PATH_FLAG_USE_DIRECTIONAL_COVER;
	}

	return iFlags;
}

void SetPathTestStartPos()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	CPathServer::ms_vPathTestStartPos = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() :
		CPlayerInfo::ms_cachedMainPlayerPos;
}
void SetPathTestEndPos()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	CPathServer::ms_vPathTestEndPos = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() :
		CPlayerInfo::ms_cachedMainPlayerPos;
}
void SetPathTestCoverOriginPos()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	CPathServer::ms_vPathTestCoverOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() :
		CPlayerInfo::ms_cachedMainPlayerPos;
}
void SetPathTestReferenceVector()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vCameraOrigin = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() :
		CPlayerInfo::ms_cachedMainPlayerPos;

	CPathServer::ms_vPathTestReferenceVector - vCameraOrigin - CPathServer::ms_vPathTestStartPos;
	CPathServer::ms_vPathTestReferenceVector.Normalize();
}
void PathTestAddInfluenceSphere()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vPos = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() :
		CPlayerInfo::ms_cachedMainPlayerPos;

	if(CPathServer::ms_iPathTestNumInfluenceSpheres < MAX_NUM_INFLUENCE_SPHERES)
	{
		TInfluenceSphere & sphere = CPathServer::ms_PathTestInfluenceSpheres[CPathServer::ms_iPathTestNumInfluenceSpheres];
		sphere.Init(vPos, CPathServer::ms_fPathTestInfluenceSphereRadius, CPathServer::ms_fPathTestInfluenceMin, CPathServer::ms_fPathTestInfluenceMax);
		CPathServer::ms_iPathTestNumInfluenceSpheres++;
	}
}
void PathTestRemoveInfluenceSphere()
{
	if(CPathServer::ms_iPathTestNumInfluenceSpheres > 0)
		CPathServer::ms_iPathTestNumInfluenceSpheres--;
}

void PathTest_FindPath()
{
	Vector3 vRef(0.0f,0.0f,0.0f);
	u64 iFlags = InitPathTestFlags();

	TRequestPathStruct reqStruct;
	reqStruct.m_iFlags = iFlags;
	reqStruct.m_vPathStart = CPathServer::ms_vPathTestStartPos;
	reqStruct.m_vPathEnd = CPathServer::ms_vPathTestEndPos;
	reqStruct.m_vReferenceVector = CPathServer::ms_vPathTestReferenceVector;
	reqStruct.m_fReferenceDistance = CPathServer::ms_fPathTestReferenceDist;
	reqStruct.m_vCoverOrigin = CPathServer::ms_vPathTestCoverOrigin;
	reqStruct.m_fCompletionRadius = CPathServer::ms_fPathTestCompletionRadius;
	reqStruct.m_fEntityRadius = CPathServer::ms_fPathTestEntityRadius;
	reqStruct.m_iNumInfluenceSpheres = Min(CPathServer::ms_iPathTestNumInfluenceSpheres, MAX_NUM_INFLUENCE_SPHERES);
	reqStruct.m_fMaxSlopeNavigable = PathTest_fPathTestMaxSlopeNavigable;
	sysMemCpy(reqStruct.m_InfluenceSpheres, CPathServer::ms_PathTestInfluenceSpheres, sizeof(TInfluenceSphere)*reqStruct.m_iNumInfluenceSpheres);

	TPathHandle hPath = CPathServer::RequestPath(reqStruct);

	if(hPath != PATH_HANDLE_NULL)
	{
		while(CPathServer::IsRequestResultReady(hPath)==ERequest_NotReady)
		{
			sysIpcSleep(10);
		}

		CPathServer::CancelRequest(hPath);
	}
}
void PathTest_TestLOS()
{
	TPathHandle hLos = CPathServer::RequestLineOfSight(CPathServer::ms_vPathTestStartPos, CPathServer::ms_vPathTestEndPos, CPathServer::ms_fPathTestEntityRadius - PATHSERVER_PED_RADIUS, true, false, false, 0, NULL, PathTest_fPathTestMaxSlopeNavigable);

	if(hLos != PATH_HANDLE_NULL)
	{
		while(CPathServer::IsRequestResultReady(hLos)==ERequest_NotReady)
		{
			sysIpcSleep(10);
		}

		CPathServer::CancelRequest(hLos);
	}
}

void OnTestBlockingObjectSetFromCamera()
{
	camDebugDirector& debugDirector = camInterface::GetDebugDirector();
	Vector3 vOrigin	= debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetPosition() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());
	Vector3 vFwd = debugDirector.IsFreeCamActive() ? debugDirector.GetFreeCamFrame().GetFront() : VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetForward());
	g_vTestBlockingObjectPos = vOrigin;
	g_fTestBlockingObjectRot = fwAngle::GetRadianAngleBetweenPoints(vFwd.x, vFwd.y, 0.0f, 0.0f);
}

void OnCreateTestBlockingObject()
{
	float fRot = g_fTestBlockingObjectRot * DtoR;
	fRot = fwAngle::LimitRadianAngleSafe(fRot);

	CPathServerGta::AddBlockingObject(g_vTestBlockingObjectPos, g_vTestBlockingObjectSize, fRot);
}

#endif	// GTA_ENGINE

#if __BANK

void
CPathServer::InitWidgets()
{
	static const char * pPathServerVisMode[] =
	{
		"Off",
		"Path Requests",
		"LOS Requests",
		"Audio Requests",
		"FloodFill Requests",
		"ClearArea Requests",
		"ClosestPosition Requests",
		"Info",
		"Memory Usage",
		"Current NavMesh",
		"Polygon Info"
	};
	static const char * pObjAdvoidanceMode[] =
	{
		"None",
		"Tessellation",
		"Clipping"
	};
	static const char * pThreadRunMode[] =
	{
		"Suspended", "Running"
	};
	/* static const char * pRelativeThreadPriority[] =
	{
		"-15 : THREAD_PRIORITY_IDLE",
		" -2 : THREAD_PRIORITY_LOWEST",
		" -1 : THREAD_PRIORITY_BELOW_NORMAL",
		"  0 : THREAD_PRIORITY_NORMAL",
		"  1 : THREAD_PRIORITY_ABOVE_NORMAL",
		"  2 : THREAD_PRIORITY_HIGHEST",
		" 15 : THREAD_PRIORITY_TIME_CRITICAL"
	}; */
	static const char * pDataSetNames[] =
	{
		"Regular",			// kNavDomainRegular
		"Height Meshes"		// kNavDomainHeightMeshes
	};
	CompileTimeAssert(NELEM(pDataSetNames) == kNumNavDomains);

	bkBank & bank = BANKMGR.CreateBank("Navigation");

	//****************************************
	// Basic navigation widgets:

	const char ** ppNavMeshMode = &ms_pNavMeshVisModeTxt[0];
	bank.AddCombo(
		"NavMesh :",
		&CPathServer::m_iVisualiseNavMeshes,
		CPathServer::NavMeshVis_NumModes,
		ppNavMeshMode,
		NullCB,	//OnSelectTaskCombo,
		"Selects NavMesh visualisation mode"
	);
	bank.AddCombo(
		"PathServer :",
		&CPathServer::m_iVisualisePathServerInfo,
		CPathServer::PathServerVis_NumModes,
		pPathServerVisMode,
		NullCB,	//OnSelectTaskCombo,
		"Selects PathServer visualisation mode"
	);
	bank.AddCombo("Data Set :",
		&CPathServer::m_iVisualiseDataSet,
		(int)kNumNavDomains,
		pDataSetNames,
		NullCB,
		"Selects data set to visualise"
	);
	bank.AddToggle("Draw Hierarchical Nodes :", &CPathServer::m_bVisualiseHierarchicalNodes);
	bank.AddToggle("Draw LinkWidths :", &CPathServer::m_bVisualiseLinkWidths);
	

#ifdef GTA_ENGINE
	bank.PushGroup("Test Tool", false);
	{
		bank.AddToggle("PathTest_bGetPointsFromMeasuringTool", &PathTest_bGetPointsFromMeasuringTool);
		bank.AddButton("Set start pos", SetPathTestStartPos);
		bank.AddButton("Set end pos", SetPathTestEndPos);
		bank.AddButton("Set reference vector", SetPathTestReferenceVector);
		bank.AddSlider("Reference dist", &CPathServer::ms_fPathTestReferenceDist, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Completion radius", &CPathServer::ms_fPathTestCompletionRadius, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Entity radius", &CPathServer::ms_fPathTestEntityRadius, PATHSERVER_PED_RADIUS, PATHSERVER_MAX_PED_RADIUS, 0.1f);
		
		bank.AddSlider("Max slope navigable", &PathTest_fPathTestMaxSlopeNavigable, 0, PI, 0.05f);

		bank.AddToggle("PreferPavements", &PathTest_bPreferPavements);
		bank.AddToggle("NeverLeaveWater", &PathTest_bNeverLeaveWater);
		bank.AddToggle("GoAsFarAsPossible", &PathTest_bGoAsFarAsPossible);
		bank.AddToggle("NeverLeavePavements", &PathTest_bNeverLeavePavements);
		bank.AddToggle("NeverClimbOverStuff", &PathTest_bNeverClimbOverStuff);
		bank.AddToggle("NeverDropFromHeight", &PathTest_bNeverDropFromHeight);
		bank.AddToggle("NeverUseLadders", &PathTest_bNeverUseLadders);
		bank.AddToggle("FleeTarget", &PathTest_bFleeTarget);
		bank.AddToggle("Wander", &PathTest_bWander);    
		bank.AddToggle("NeverEnterWater", &PathTest_bNeverEnterWater);
		bank.AddToggle("DontAvoidDynamicObjects", &PathTest_bDontAvoidDynamicObjects);
		bank.AddToggle("PreferDownhill", &PathTest_bPreferDownhill);
		bank.AddToggle("ReduceObjectBBoxes", &PathTest_bReduceObjectBBoxes);
		bank.AddToggle("DontLimitSearchExtents", &PathTest_bUseLargerSearchExtents);
		bank.AddToggle("MayUseFatalDrops", &PathTest_bMayUseFatalDrops);
		bank.AddToggle("IgnoreObjectsWithNoUprootLimit", &PathTest_bIgnoreObjectsWithNoUprootLimit);
		bank.AddToggle("AllowToTravelUnderwater", &PathTest_bAllowToTravelUnderwater);
		bank.AddToggle("AllowToTravelUnderwater", &PathTest_bAllowToTravelUnderwater);
		bank.AddToggle("UseDirectionalCover", &PathTest_bUseDirectionalCover);
		bank.AddToggle("CutSharpCorners", &PathTest_bCutSharpCorners);
		bank.AddToggle("AllowToNavigateUpSteepPolygons", &PathTest_bAllowToNavigateUpSteepPolygons);

		bank.AddToggle("PathTest_bScriptedRoute", &PathTest_bScriptedRoute);
		bank.AddToggle("PathTest_bPreserveZ", &PathTest_bPreserveZ);
		bank.AddToggle("PathTest_bRandomisePoints", &PathTest_bRandomisePoints);

		bank.AddButton("Set cover origin", SetPathTestCoverOriginPos);

		bank.AddButton("Add influence sphere", PathTestAddInfluenceSphere);
		bank.AddButton("Remove influence sphere", PathTestRemoveInfluenceSphere);
		bank.AddSlider("Influence sphere radius", &CPathServer::ms_fPathTestInfluenceSphereRadius, 1.0f, 50.0f, 0.5f);
		bank.AddSlider("Influence sphere min influence", &CPathServer::ms_fPathTestInfluenceMin, -50.0f, 50.0f, 0.5f);
		bank.AddSlider("Influence sphere max influence", &CPathServer::ms_fPathTestInfluenceMax, -50.0f, 50.0f, 0.5f);

		bank.AddButton("Find path", PathTest_FindPath);
		bank.AddButton("Test LOS", PathTest_TestLOS);
		bank.PopGroup();
	}
#endif

	bank.PushGroup("Display", false);
	{
		bank.AddSlider("Pathserver Text top-left (X)", &g_vPathserverDebugTopLeft.x, -1024.0f, 1024.0f, 1.0f);
		bank.AddSlider("Pathserver Text top-left (Y)", &g_vPathserverDebugTopLeft.y, -1024.0f, 1024.0f, 1.0f);
#ifdef GTA_ENGINE
		bank.AddToggle("Show Ped-Gen Blocking Areas", &CPathServer::ms_bDrawPedGenBlockingAreas);
		bank.AddToggle("Show Ped-Wandering Blocking Areas", &CPathServer::ms_bDrawPedSwitchAreas);
		bank.AddToggle("Visualise Paths", &CPathServer::m_bVisualisePaths);
		bank.AddToggle("Only visualise dynamic navmeshes", &gOnlyVisualiseDynamicNavmeshes);
		bank.AddButton("Clear all path history", CPathServer::ClearAllPathHistory);
		bank.AddButton("Clear all open/closed flags", CPathServer::ClearAllOpenClosedFlags);
#endif
		bank.AddToggle("Show QuadTrees", &CPathServer::m_bVisualiseQuadTrees, NullCB, NULL);
		bank.AddToggle("Show All CoverPts", &CPathServer::m_bVisualiseAllCoverPoints, NullCB, NULL);

		bank.AddSlider("Extrude-Up Amt", &CPathServer::m_fVisNavMeshVertexShiftAmount, 0.0f, 1.0f, 0.02f, NullCB, NULL);
		bank.AddSlider("Debug Polys Range", &CPathServer::ms_fDebugVisPolysMaxDist, 10.0f, 400.0f, 1.0f, NullCB, NULL);
		bank.AddSlider("Debug Nodes Range", &CPathServer::ms_fDebugVisNodesMaxDist, 10.0f, 400.0f, 1.0f, NullCB, NULL);

		bank.AddToggle("Draw Dynamic Object MinMax's", &CPathServer::ms_bDrawDynamicObjectMinMaxs, NullCB, NULL);
		bank.AddToggle("Draw Dynamic Object Grids", &CPathServer::ms_bDrawDynamicObjectGrids, NullCB, NULL);
		bank.AddToggle("Draw dynamic objects axes", &CPathServer::ms_bDrawDynamicObjectAxes, NullCB, NULL);
		bank.AddToggle("Show Climb-Overs (low)", &CPathServer::m_bDebugShowLowClimbOvers, NullCB, NULL);
		bank.AddToggle("Show Climb-Overs (high)", &CPathServer::m_bDebugShowHighClimbOvers, NullCB, NULL);
		bank.AddToggle("Show Drop-Downs", &CPathServer::m_bDebugShowDropDowns, NullCB, NULL);

		bank.AddToggle("Show Special Links", &CPathServer::m_bDebugShowSpecialLinks, NullCB, NULL);
		bank.AddToggle("Show Poly Volumes (above)", &CPathServer::m_bDebugShowPolyVolumesAbove, NullCB, NULL);
		bank.AddToggle("Show Poly Volumes (below)", &CPathServer::m_bDebugShowPolyVolumesBelow, NullCB, NULL);
		

		bank.PopGroup();
	}

	//****************************************
	// Advanced navigation widgets:

	bank.PushGroup("PathFinding", false);
	{
		bank.AddCombo(
			"Obj Avoidance :",
			&CPathServer::m_iObjAvoidanceModeComboIndex,
			3,
			pObjAdvoidanceMode,
			OnSelectObjAvoidanceCombo,
			"Selects the object avoidance method"
		);

		bank.AddSlider("Dynamic object velocity threshold", &ms_fDynamicObjectVelocityThreshold, 0.0f, 1000.0f, 0.1f);

		bank.AddSlider("Non-pavement penalty (wander)", &CPathServerThread::ms_fNonPavementPenalty_Wander, 0.0f, 50000.0f, 1.0f);
		bank.AddSlider("Non-pavement penalty (shortest path)", &CPathServerThread::ms_fNonPavementPenalty_ShortestPath, 0.0f, 50000.0f, 1.0f);
		bank.AddSlider("Non-pavement penalty (flee)", &CPathServerThread::ms_fNonPavementPenalty_Flee, 0.0f, 50000.0f, 1.0f);

		bank.AddSlider("Climb multiplier (wander)", &CPathServerThread::ms_fClimbMultiplier_Wander, 0.0f, 50.0f, 1.0f);
		bank.AddSlider("Climb multiplier (shortest path)", &CPathServerThread::ms_fClimbMultiplier_ShortestPath, 0.0f, 50.0f, 1.0f);
		bank.AddSlider("Climb multiplier (flee)", &CPathServerThread::ms_fClimbMultiplier_Flee, 0.0f, 50.0f, 1.0f);

		bank.AddSlider("Override Ped Radius", &CTaskNavBase::ms_fOverrideRadius, 0.1f, 16.0f, 0.1f);
		bank.AddToggle("Do poly tessellation", &CPathServer::ms_bDoPolyTessellation, NullCB, NULL);
		bank.AddToggle("Do poly untessellation", &CPathServer::ms_bDoPolyUnTessellation, NullCB, NULL);
		bank.AddToggle("Do preemptive tessellation", &CPathServer::ms_bDoPreemptiveTessellation, NullCB, NULL);
		bank.AddToggle("Allow tessellation of open nodes", &CPathServer::ms_bAllowTessellationOfOpenNodes);
		bank.AddToggle("Use objects grid cell cache", &CPathServer::ms_bUseGridCellCache);
		bank.AddToggle("Use tessellated poly object cache", &CPathServer::ms_bUseTessellatedPolyObjectCache);
		bank.AddToggle("Use LOS to early our path requests", &CPathServer::ms_bUseLosToEarlyOutPathRequests);
		bank.AddSlider("\'small\' poly area when tessellating", &CPathServerThread::ms_fSmallPolySizeWhenTessellating, 0.1f, 1000.0f, 0.1f);
		bank.AddToggle("Smooth routes using splines", &CPathServer::ms_bSmoothRoutesUsingSplines);
		bank.AddToggle("Pull path out from edges", &CPathServer::ms_bPullPathOutFromEdges);
		bank.AddToggle("Pull path out from edges twice", &CPathServer::ms_bPullPathOutFromEdgesTwice);
		bank.AddToggle("Pull path out from edges string pull", &CPathServer::ms_bPullPathOutFromEdgesStringPull);
		bank.AddSlider("Pull Distance", &CPathServerThread::ms_fPullOutFromEdgesDistance, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("Pull Distance Extra", &CPathServerThread::ms_fPullOutFromEdgesDistanceExtra, 0.0f, 10.0f, 0.1f);

//		bank.AddToggle("Allow object climbing", &CPathServer::ms_bAllowObjectClimbing, NullCB, NULL);
		bank.AddToggle("Allow navmesh climbs", &CPathServer::ms_bAllowNavMeshClimbs, NullCB, NULL);
//		bank.AddToggle("Allow object pushing", &CPathServer::ms_bAllowObjectPushing, NullCB, NULL);
		bank.AddToggle("Exclude vehicle doors", &fwPathServer::ms_bExcludeVehicleDoorsFromNavigation);

		bank.AddToggle("Disallow climbable object links", &CPathServer::ms_bDisallowClimbObjectLinks);

		bank.AddToggle("Mark visited polys", &CPathServer::ms_bMarkVisitedPolys, NullCB, NULL);
		bank.AddToggle("Debug poly underfoot", &CPathServer::m_bDebugPolyUnderfoot, NullCB, NULL);
		bank.AddToggle("Detailed Debug PathFinding", &CPathServer::ms_bDebugPathFinding, NullCB, NULL);
		bank.AddToggle("Print Out Path Results", &CPathServer::ms_bOutputDetailsOfPaths, NullCB, NULL);

		//bank.AddToggle("Early out sphere/ray tests on dynamic objects", &CPathServer::ms_bSphereRayEarlyOutTestOnObjects);
		bank.AddSlider("Default plane epsilon for DynObjLOS", &CPathServer::ms_fDefaultDynamicObjectPlaneEpsilon, -10.0f, 10.0f, 0.05f);
		bank.AddSlider("Height above navmesh for DynObjLOS", &CPathServerThread::ms_fHeightAboveNavMeshForDynObjLOS, 0.0f, 1.0f, 0.1f);
		bank.AddSlider("Height above navmesh for 2nd DynObjLOS", &CPathServerThread::ms_fHeightAboveFirstTestFor2ndDynObjLOS, 0.0f, 1.0f, 0.1f);
		
		bank.AddSlider("Max mass of pushable object", &CPathServerThread::ms_fMaxMassOfPushableObject, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Min mass of significant object", &CPathServerThread::ms_fMinMassOfSmallSignificant, 0.0f, 1000.0f, 1.0f);
		bank.AddSlider("Min volume of significant object", &CPathServerThread::ms_fMinVolumeOfSignificant, 0.0f, 10.0f, 0.1f);
		bank.AddSlider("Big volume of significant object (these are always significant)", &CPathServerThread::ms_fBigVolumeOfSignificant, 0.0f, 10.0f, 0.1f);

		bank.AddToggle("Don't revisit open nodes", &CPathServer::ms_bDontRevisitOpenNodes, NullCB, NULL);

		bank.AddSlider("Max pathfind search dist from origin:", &CPathServer::m_PathServerThread.m_Vars.m_fMaxPathFindDistFromOrigin, 10.0f, 200.0f, 1.0f);
	
		bank.AddToggle("Check every poly for objects", &CPathServer::ms_bTestObjectsForEveryPoly, NullCB, NULL);
		bank.AddToggle("Use GetPolyCentroidQuick() when possible", &CPathServer::ms_bUseOptimisedPolyCentroids, NullCB, NULL);
		bank.AddToggle("Always use more points in polys", &CPathServer::ms_bAlwaysUseMorePointsInPolys);
		bank.AddSlider("Smallest tesselated edge threshold sqr",  &CNavMesh::ms_fSmallestTessellatedEdgeThresholdSqr, 0.1f, 64.0f, 0.1f);

		bank.AddToggle("Use events to signal path-requests", &CPathServer::ms_bUseEventsForRequests);
		bank.AddToggle("Process all pending paths at once", &CPathServer::ms_bProcessAllPendingPathsAtOnce);

		bank.AddToggle("Sleep thread during long requests", &CPathServer::m_bSleepPathServerThreadOnLongPathRequests);
		bank.AddSlider("Sleep on long path-requests (ms)", &CPathServer::m_iTimeToSleepDuringLongRequestsMs, 0, 1000, 1);
		bank.AddSlider("Path-request duration before sleeping", &CPathServer::m_PathServerThread.ms_iPathRequestNumMillisecsBeforeSleepingThread, 1, 1000, 1);
		bank.AddSlider("Thread sleep time between requests (ms)", &CPathServer::m_iTimeToSleepWaitingForRequestsMs, 0, 1000, 1);
		bank.AddToggle("Use new path priority scoring", &CPathServer::ms_bUseNewPathPriorityScoring);

		bank.AddSlider("Prefer-downhill scale factor", &CPathServerThread::ms_fPreferDownhillScaleFactor, 0.0f, 100.0f, 0.25f);
		bank.AddSlider("Wanderer turn weight", &CPathServerThread::ms_fWanderTurnWeight, 0.0f, 10000.0f, 1.0f);
		bank.AddSlider("Wanderer turn plane weight", &CPathServerThread::ms_fWanderTurnPlaneWeight, 0.0f, 10000.0f, 1.0f);

#if __BANK && defined(GTA_ENGINE)
		bank.AddToggle("Only display the selected route", &CPathServer::ms_bOnlyDisplaySelectedRoute);
		bank.AddToggle("Show path search extents", &CPathServer::ms_bShowSearchExtents);
#endif
#if __XENON
		bank.AddToggle("XTrace the next path request", &CPathServer::ms_bUseXTraceOnTheNextPathRequest);
#endif
		bank.AddButton(">>> Next Path Request", datCallback(CFA(PathServer_OnSelectNextPathRequest)));
		bank.AddButton("<<< Prev Path Request", datCallback(CFA(PathServer_OnSelectPrevPathRequest)));

		bank.AddButton("Repeat Path Query", datCallback(CFA(PathServer_RepeatPathQuery)));
		bank.AddButton("Output \'.prob\' PathRequest File", datCallback(CFA(PathServer_OutputPathQuery)));
		bank.AddButton("Request path but don't retrieve", PathServer_RequestPathButDontRetrieve);
		bank.AddButton("Create LOS Stack Overflow Bug", PathServer_TestLosStackOverflow);
		

#if __DEV
		static const char * pVisitedPolysMode[] =
		{
			"List",
			"TimeStamp",
			"Both (Debug)"
		};
		bank.AddCombo(
			"Visited Poly Mode :",
			&CPathServerThread::m_eVisitedPolyMarkingMode,
			3,
			pVisitedPolysMode,
			NullCB,
			"Selects how pathserver deals with resetting visited polygon flags"
		);
#endif
		bank.AddCombo(
			"Thread :",
			&CPathServer::m_iDebugThreadRunMode,
			2,
			pThreadRunMode,
			OnSelectThreadRunModeCombo,
			"Suspends/Resumes the main pathserver thread"
		);

		bank.AddToggle("Disable PathFinding", &CPathServer::m_bDisablePathFinding);
		bank.AddToggle("Disable Object Avoidance", &CPathServer::m_bDisableObjectAvoidance);
		bank.AddToggle("Disable Audio-Reqs", &CPathServer::m_bDisableAudioRequests);
		bank.AddToggle("Disable GetClostestPos", &CPathServer::m_bDisableGetClosestPositionForPed);
		bank.AddToggle("Stress-Test Paths", &CPathServer::m_bStressTestPathFinding);
		bank.AddToggle("Stress-Test GetClostestPos", &CPathServer::m_bStressTestGetClosestPos);
		bank.AddToggle("Visualise failed polygon requests", &CPathServer::m_bVisualisePolygonRequests);

		bank.PopGroup();
	}

	bank.PushGroup("Default A* heuristics", false);
	{
		bank.AddSlider("ms_fDefaultCostFromTargetMultiplier", &CPathFindMovementCosts::ms_fDefaultCostFromTargetMultiplier, 0.0f, 200.0f, 0.1f);
		bank.AddSlider("ms_fDefaultDistanceTravelledMultiplier", &CPathFindMovementCosts::ms_fDefaultDistanceTravelledMultiplier, 0.0f, 200.0f, 0.1f);
		bank.AddSlider("ms_fDefaultClimbHighPenalty", &CPathFindMovementCosts::ms_fDefaultClimbHighPenalty, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultClimbLowPenalty", &CPathFindMovementCosts::ms_fDefaultClimbLowPenalty, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultDropDownPenalty", &CPathFindMovementCosts::ms_fDefaultDropDownPenalty, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultDropDownPenaltyPerMetre", &CPathFindMovementCosts::ms_fDefaultDropDownPenaltyPerMetre, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultClimbLadderPenalty", &CPathFindMovementCosts::ms_fDefaultClimbLadderPenalty, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultClimbLadderPenaltyPerMetre", &CPathFindMovementCosts::ms_fDefaultClimbLadderPenaltyPerMetre, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultEnterWaterPenalty", &CPathFindMovementCosts::ms_fDefaultEnterWaterPenalty, 0.0f, 10000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultLeaveWaterPenalty", &CPathFindMovementCosts::ms_fDefaultLeaveWaterPenalty, 0.0f, 5000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultBeInWaterPenalty", &CPathFindMovementCosts::ms_fDefaultBeInWaterPenalty, 0.0f, 2000.0f, 1.0f);
		//bank.AddSlider("ms_fDefaultFixedCostToPushObject", &CPathFindMovementCosts::ms_fDefaultFixedCostToPushObject, 0.0f, 2000.0f, 1.0f);
		//bank.AddSlider("ms_fDefaultFixedCostToClimbObject", &CPathFindMovementCosts::ms_fDefaultFixedCostToClimbObject, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultClimbObjectPenaltyPerMetre", &CPathFindMovementCosts::ms_fDefaultClimbObjectPenaltyPerMetre, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultWanderPenaltyForZeroPedDensity", &CPathFindMovementCosts::ms_fDefaultWanderPenaltyForZeroPedDensity, 0.0f, 2000.0f, 0.1f);
		bank.AddSlider("ms_fDefaultMoveOntoSteepSurfacePenalty", &CPathFindMovementCosts::ms_fDefaultMoveOntoSteepSurfacePenalty, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultPenaltyForNoDirectionalCover", &CPathFindMovementCosts::ms_fDefaultPenaltyForNoDirectionalCover, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_fDefaultPenaltyForFavourCoverPerUnsetBit", &CPathFindMovementCosts::ms_fDefaultPenaltyForFavourCoverPerUnsetBit, 0.0f, 2000.0f, 1.0f);
		bank.AddSlider("ms_iDefaultPenaltyForMovingToLowerPedDensity", &CPathFindMovementCosts::ms_iDefaultPenaltyForMovingToLowerPedDensity, 0, 2000, 1);
		bank.AddSlider("ms_fFleePathDoubleBackCost", &CPathFindMovementCosts::ms_fFleePathDoubleBackCost, 0.0f, 2000.0f, 0.1f);
		bank.AddSlider("ms_fFleePathDoubleBackCostSofter", &CPathFindMovementCosts::ms_fFleePathDoubleBackCostSofter, 0.0f, 2000.0f, 0.1f);
		bank.AddSlider("ms_fDefaultAvoidTrainTracksPenalty", &CPathFindMovementCosts::ms_fDefaultAvoidTrainTracksPenalty, 0.0f, 100000.0f, 1.0f);

		bank.PopGroup();
	}

	bank.PushGroup("Ped Generation", false);
	{
		bank.PushGroup("Ambient", false);
			bank.AddButton("Turn off wandering at player pos", datCallback(CFA(PathServer_TestTurnOffWandering)));
			bank.AddButton("Add pedgen blocking area at player pos", datCallback(CFA(PathServer_AddPedGenBlockingArea)));
			bank.AddSlider("PedGen node floodfill dist", &CPathServer::ms_fPedGenAlgorithmNodeFloodFillDist, 0.0f, 50.0f, 1.0f);
			bank.AddSlider("Percentage of ped-density polys per navmesh", &CPathServer::m_AmbientPedGeneration.m_iPedGenPercentagePerNavMesh, 0, 100, 1);
			bank.AddSlider("Debug adjust spawn density in AABB (from measuring tool)", &CPedPopulation::m_iDebugAdjustSpawnDensity, -MAX_PED_DENSITY, MAX_PED_DENSITY, 1);
		bank.PopGroup();

		bank.PushGroup("Opponent", false);
			bank.AddToggle("Visualise search", &CPathServer::ms_bVisualiseOpponentPedGeneration);
			bank.AddButton("Start opponent pedgen (camera pos)", Pathserver_StartOpponentPedGen);
			bank.AddButton("Start opponent pedgen (angled area - measuring rool)", Pathserver_StartOpponentPedGenAngledArea);
			bank.AddButton("Complete opponent pedgen", Pathserver_CompleteOpponentPedGen);
			bank.AddToggle("bOpponentPedGen_AllowExterior", &bOpponentPedGen_AllowExterior);
			bank.AddToggle("bOpponentPedGen_AllowInteriors", &bOpponentPedGen_AllowInteriors);
			bank.AddToggle("bOpponentPedGen_AllowNonNetworkSpawn", &bOpponentPedGen_AllowNonNetworkSpawn);
			bank.AddToggle("bOpponentPedGen_AllowIsolated", &bOpponentPedGen_AllowIsolated);
			bank.AddToggle("bOpponentPedGen_AllowRoad", &bOpponentPedGen_AllowRoad);
			bank.AddToggle("bOpponentPedGen_OnlyCoverEdges", &bOpponentPedGen_OnlyCoverEdges);
			bank.AddSlider("fOpponentPedGen_RadiusXY", &fOpponentPedGen_RadiusXY, 0.0f, 250.f, 1.0f);
			bank.AddSlider("fOpponentPedGen_CutoffZ", &fOpponentPedGen_CutoffZ, -1000.0f, 1000.f, 10.0f);
			bank.AddSlider("fOpponentPedGen_MinimumSpacing", &fOpponentPedGen_MinimumSpacing, 0.0f, 100.0f, 1.0f);
			bank.AddSlider("fOpponentPedGen_TimeoutMS", &iOpponentPedGen_TimeoutMS, 0, 20000, 1);
		bank.PopGroup();

		bank.PopGroup();
	}

	bank.PushGroup("Misc", false);
	{
		bank.AddSlider("Request and Evict Freq (ms)", &ms_iRequestAndEvictFreqMS, 0, 10000, 1);
		bank.AddToggle("Disable Streaming", &ms_bStreamingDisabled);
		bank.AddToggle("Verify Adjacencies", &m_bVerifyAdjacencies);
		bank.AddButton("Detessellate now", datCallback(CFA(DebugDetessellateNow)));
		bank.AddToggle("Display NavMesh Load Times", &CNavMesh::ms_bDisplayTimeTakenToLoadNavMeshes);
		bank.AddToggle("Force use of decompressed vertices", &CNavMesh::ms_bForceAllNavMeshesToUseUnCompressedVertices);
		bank.AddToggle("Vis poly under camera", &g_VisPolyUnderCamera);
		bank.AddButton("PathServer_TestColPolyVsPaths", PathServer_TestColPolyVsPaths);
		bank.AddButton("Print poly under debug-camera", datCallback(CFA(PathServer_TestDebugPolyUnderCamera)));
		bank.AddButton("Test NavMesh LOS from Player to Camera", datCallback(CFA(PathServer_TestLOS)));
		bank.AddButton("Add Required Region At Camera", datCallback(CFA(PathServer_AddRequiredRegion)));
		bank.AddButton("Remove Required Region", datCallback(CFA(PathServer_RemoveRequiredRegion)));
		bank.AddButton("Test Audio Properties request for player", datCallback(CFA(PathServer_TestAudioPropertiesRequest)));
		bank.AddButton("Test FloodFill for closest car node from player", datCallback(CFA(PathServer_TestFloodFillToClosestCarNode)));
		bank.AddButton("Test clear area request at player position", datCallback(CFA(PathServer_TestClearAreaRequest)));
		bank.AddButton("Test closest position request at player position", datCallback(CFA(PathServer_TestClosestPositionRequest)));

		bank.AddButton("Test Evict Dynamic Object For Script", TestEvictDynamicObjectForScript);
		
		bank.AddSlider("Navmesh load proximity", &CPathServer::m_fNavMeshLoadProximity, 0.0f, 1000.0f, 10.0f);
		bank.AddSlider("Hierarchical-Nodes load proximity", &CPathServer::m_fHierarchicalNodesLoadProximity, 0.0f, 1000.0f, 10.0f);
		
		bank.AddButton("Test switch off polygons for ped around camera", datCallback(CFA(PathServer_TestTurnOffPolysForPedsAroundCamera)));
		bank.AddButton("Test disable polygons around camera", datCallback(CFA(PathServer_TestDisablePolysAroundCamera)));

#ifdef GTA_ENGINE
		bank.AddToggle("Count Thread-Stalls", &CPathServer::m_bCheckForThreadStalls);
		bank.AddToggle("Display Add DynObject Times", &CPathServer::m_bDisplayTimeTakenToAddDynamicObjects);
		bank.AddButton("Find closest safe-pos to player", datCallback(CFA(PathServer_TestFindClosestPosOnAnySurface)));
		bank.AddButton("Find closest safe-pos to player (pavement)", datCallback(CFA(PathServer_TestFindClosestPosOnPavement)));
		bank.AddButton("Find closest safe-pos to player (no objects)", datCallback(CFA(PathServer_TestFindClosestPosOnAnySurfaceClearOfObjects)));
		bank.AddButton("Find closest safe-pos to player (pavement & no objects)", datCallback(CFA(PathServer_TestFindClosestPosOnPavementClearOfObjects)));
		bank.AddButton("Find closest safe-pos to player (pavement & no objects, extra-thorough, flood-fill)", datCallback(CFA(PathServer_TestFindClosestPosOnAnySurfaceClearOfObjectsExtraThorough)));
		
#if __DEV
		bank.AddButton("Test calc poly areas", PathServer_TestCalcPolyAreas);
#endif
		bank.AddToggle("Use prefetching", &CPathServerThread::ms_bUsePrefetching);
		bank.AddButton("Set up perf test", datCallback(CFA(PathServer_SetUpPerfTest)));
		bank.AddToggle("Draw ped tracking avoidance probes", &CNavMeshTrackedObject::ms_bDrawPedTrackingProbes);

#if __XENON && !__FINAL
		bank.AddToggle("Use VMX optimisations", &CNavMesh::ms_bUseVmxOptimisations);
#endif
		bank.AddToggle("Use prefetching", &CNavMesh::ms_bUsePrefetching);
		//bank.AddButton("Test vmx minmax", datCallback(CFA(PathServer_TestMinMax)));
#endif
		bank.AddSlider("Navmesh request/evict freq(ms)", &CPathServer::ms_iRequestAndEvictMeshesFreqMs, 0, 10000, 1);

		bank.AddButton("Add user dynamic object at camera", AddUserDynamicObjectAtCameraPos);

		bank.PopGroup();
	}

	bank.PushGroup("Audio", false);
	{
		bank.AddSlider("Min poly size for radius search:", &CPathServer::m_fAudioMinPolySizeForRadiusSearch, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Velocity cube size:", &CPathServer::m_fAudioVelocityCubeSize, 0.0f, 100.0f, 0.1f);
		bank.AddSlider("Velocity dot product:", &CPathServer::m_fAudioVelocityDotProduct, 0.0f, 1.0f, 0.01f);

		bank.PopGroup();
	}

	bank.PushGroup("Blocking Bounds", false);
	{
		bank.AddToggle("Enable", &g_bTestBlockingObjectEnabled);

		bank.AddSlider("Position X", &g_vTestBlockingObjectPos.x, WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_XMAX, 0.1f);
		bank.AddSlider("Position Y", &g_vTestBlockingObjectPos.y, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_YMAX, 0.1f);
		bank.AddSlider("Position Z", &g_vTestBlockingObjectPos.z, -1000.0f, 1000.0f, 0.1f);
		bank.AddSlider("Rotation (Z)", &g_fTestBlockingObjectRot, -180.0f, 180.0f, 1.0f);
		bank.AddSeparator();
		bank.AddButton("Set from player/camera", OnTestBlockingObjectSetFromCamera);
		bank.AddSeparator();
		bank.AddSlider("Width (X)", &g_vTestBlockingObjectSize.x, 0.25f, 20.0f, 0.1f);
		bank.AddSlider("Depth (Y)", &g_vTestBlockingObjectSize.y, 0.25f, 20.0f, 0.1f);
		bank.AddSlider("Height (Z)", &g_vTestBlockingObjectSize.z, 0.25f, 20.0f, 0.1f);
		bank.AddSeparator();

		bank.AddButton("Create blocking object here", OnCreateTestBlockingObject);

		bank.PopGroup();
	}

#ifdef GTA_ENGINE
#if NAVMESH_EXPORT

	{
		bank.PushGroup("Extra", false);
		bank.AddSlider("Num sectors per block", &CPathServerExtents::m_iNumSectorsPerNavMesh, 0, 12, 1);
		//bank.AddButton("Set defaults for animviewer!", datCallback(CFA(PathServer_SetDefaultsForAnimViewer)));
		bank.PopGroup();
	}


#endif	// NAVMESH_EXPORT
#endif //GTA_ENGINE
}

#endif //__BANK

#endif	// DEBUG_DRAW

#if __BANK
void CPathServer::DebugOutPathRequestResult(CPathRequest * pPathRequest)
{
	const char * pPathType;
	if(pPathRequest->m_bWander) pPathType = "Wander Path";
	else if(pPathRequest->m_bFleeTarget) pPathType = "Flee Path";
	else pPathType = "Normal Path";

	Displayf("***********************************************************\n");
	Displayf("Path Handle : 0x%x\n", pPathRequest->m_hHandle);
	Displayf("Completion Code : %i\n", pPathRequest->m_iCompletionCode);
	Displayf("Context : %p\n", pPathRequest->m_pContext);
	Displayf("Path Type : \"%s\"\n", pPathType);
	Displayf("Total Time Taken : %.4f\n", pPathRequest->m_fTotalProcessingTimeInMillisecs);
	Displayf("Time To Find Start/End Polys : %.4f\n", pPathRequest->m_fMillisecsToFindPolys);
	Displayf("Time To Find Polygon Path : %.4f\n", pPathRequest->m_fMillisecsToFindPath);
	Displayf("Time To Refine Path : %.4f\n", pPathRequest->m_fMillisecsToRefinePath);
	Displayf("Time To PostProcess Z Heights : %.4f\n", pPathRequest->m_fMillisecsToPostProcessZHeights);	
	Displayf("Time To Minimise Path Length : %.4f\n", pPathRequest->m_fMillisecsToMinimisePathLength);
	Displayf("Time To Pull Out From Edges : %.4f\n", pPathRequest->m_fMillisecsToPullOutFromEdges);
	Displayf("Num Visited Polys : %i\n", pPathRequest->m_iNumVisitedPolygons);
	Displayf("Num Times Called TessellatePoly() : %i\n", pPathRequest->m_iNumTessellations);
	Displayf("Num Times Called TestDynamicObjectLOS() : %i\n", pPathRequest->m_iNumTestDynamicObjectLOS);
	Displayf("***********************************************************\n");
}


#endif

#endif

