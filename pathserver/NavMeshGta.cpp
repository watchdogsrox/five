// Rage headers
#include "math/simpleMath.h"
#include "spatialdata/sphere.h"

// Framework headers
#include "fwmaths/vector.h"

// Game headers
#include "PathServer/NavMeshGta.h"
#include "data/safestruct.h"
#include "data/struct.h"
#include "file/asset.h"
#include "file/device.h"
#include "file/stream.h"
#include "paging/rscbuilder.h"
#include "vector/geometry.h"
#include "vector/vector2.h"
#include "vector/color32.h"

#ifdef GTA_ENGINE
#include "debug/debug.h"
#ifndef RAGEBUILDER
#include "grcore/debugdraw.h"
#endif // RAGEBUILDER
#endif

#if !__FINAL
#ifdef GTA_ENGINE
#ifndef RAGEBUILDER
void
CNavMeshGTA::VisualiseQuadTree(bool bDrawCoverpoints)
{
	if(m_pQuadTree)
	{
		VisualiseQuadTree(m_pQuadTree, bDrawCoverpoints);
	}
}

void
CNavMeshGTA::VisualiseQuadTree(CNavMeshQuadTree * DEBUG_DRAW_ONLY(pTree), bool DEBUG_DRAW_ONLY(bDrawCoverpoints))
{
#if DEBUG_DRAW
	if(pTree->m_pLeafData)
	{
		//***********************************************
		// Draw the quadtree leaf's extents

		float fMinZ = m_pQuadTree->m_Mins.z;
		float fMaxZ = m_pQuadTree->m_Maxs.z;

		Vector3 vPts[8] =
		{
			Vector3(pTree->m_Mins.x, pTree->m_Mins.y, fMinZ),
			Vector3(pTree->m_Maxs.x, pTree->m_Mins.y, fMinZ),
			Vector3(pTree->m_Maxs.x, pTree->m_Maxs.y, fMinZ),
			Vector3(pTree->m_Mins.x, pTree->m_Maxs.y, fMinZ),

			Vector3(pTree->m_Mins.x, pTree->m_Mins.y, fMaxZ),
			Vector3(pTree->m_Maxs.x, pTree->m_Mins.y, fMaxZ),
			Vector3(pTree->m_Maxs.x, pTree->m_Maxs.y, fMaxZ),
			Vector3(pTree->m_Mins.x, pTree->m_Maxs.y, fMaxZ)
		};

		Color32 lineCol(0xff,0x80,0xff,0xff);

		grcDebugDraw::Line(vPts[0], vPts[1], lineCol);
		grcDebugDraw::Line(vPts[1], vPts[2], lineCol);
		grcDebugDraw::Line(vPts[2], vPts[3], lineCol);
		grcDebugDraw::Line(vPts[3], vPts[0], lineCol);

		grcDebugDraw::Line(vPts[4], vPts[5], lineCol);
		grcDebugDraw::Line(vPts[5], vPts[6], lineCol);
		grcDebugDraw::Line(vPts[6], vPts[7], lineCol);
		grcDebugDraw::Line(vPts[7], vPts[4], lineCol);

		grcDebugDraw::Line(vPts[0], vPts[4], lineCol);
		grcDebugDraw::Line(vPts[1], vPts[5], lineCol);
		grcDebugDraw::Line(vPts[2], vPts[6], lineCol);
		grcDebugDraw::Line(vPts[3], vPts[7], lineCol);

		//*********************************************
		// Draw the coverpoints in this leaf

		static const Color32 iCovCol1 = Color32(0.0f,0.0f,1.0f,1.0f);	//Color_BlueViolet;
		static const Color32 iCovCol2 = Color32(0.0f,1.0f,0.0f,1.0f);	//Color_SpringGreen;

		if(bDrawCoverpoints)
		{
			Vector3 vCoverPt;
			int c;
			for(c=0; c<pTree->m_pLeafData->m_iNumCoverPoints; c++)
			{
				const CNavMeshCoverPoint & cp = pTree->m_pLeafData->m_CoverPoints[c];
				DecompressVertex(vCoverPt, cp.m_iX, cp.m_iY, cp.m_iZ);
				grcDebugDraw::Line(vCoverPt, vCoverPt + Vector3(0.0f, 0.0f, 0.75f), iCovCol1, iCovCol2);
			}
		}

		return;
	}

	for(s32 c=0; c<4; c++)
	{
		VisualiseQuadTree(pTree->m_pChildren[c], bDrawCoverpoints);
	}
#endif // DEBUG_DRAW
}
#endif
#endif
#endif

