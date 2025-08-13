
#include "pathserver/PathServer_Store.h"
#include "pathserver/PathServer.h"
#include "pathserver/PathServerThread.h"
#include "fwscene/world/WorldLimits.h"	// TODO: find out the new way to get min/max possible world extents
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "modelinfo/VehicleModelInfo.h"

NAVMESH_OPTIMISATIONS()

//----------------------------------------------------------------------------

void DefaultAdjoinNavmeshesAcrossMapSwap(CNavMesh * pMainMapMesh, CNavMesh * pSwapMesh, eNavMeshEdge iSideOfMainMapMesh);

//-----------------------------------------------------------------------------

void aiNavMeshStoreInterfaceGta::FindVehicleForDynamicNavMesh(atHashString iNameHash, CModelInfoNavMeshRef &dynNavInf) const
{
	//****************************************************************************************************************
	// Go through modelinfos which might have an associated dynamic navmesh (vehicles only for now..) and look
	// for a string match.  Once we find the matching modelinfo, then store its model index in the dynNavInf entry.

	fwArchetypeDynamicFactory<CVehicleModelInfo> & vehStore = CModelInfo::GetVehicleModelInfoStore();
	atArray<CVehicleModelInfo*> vehicleTypes;
	vehStore.GatherPtrs(vehicleTypes);

	NAVMESH_OPTIMISATIONS_OFF_ONLY( ASSERT_ONLY(bool bFoundModel=false;) )

	int i;
	for(i=0; i<vehicleTypes.GetCount(); i++)
	{
		CVehicleModelInfo * pInfo = vehicleTypes[i];

		if(iNameHash == pInfo->GetHashKey())
		{
			dynNavInf.m_iModelIndex = CModelInfo::GetModelIdFromName(iNameHash).GetModelIndex();
			NAVMESH_OPTIMISATIONS_OFF_ONLY( ASSERT_ONLY(bFoundModel = true;) )
			break;
		}
	}

	NAVMESH_OPTIMISATIONS_OFF_ONLY( Assertf(bFoundModel, "The vehicle \"%s\" wasn't found in the game, although a navmesh for it exists in navmeshes.img.", iNameHash.GetCStr() ); )
}


void aiNavMeshStoreInterfaceGta::NavMeshesHaveChanged() const
{
	CPathServer::GetAmbientPedGen().NotifyNavmeshesHaveChanged();
	CPathServer::GetOpponentPedGen().NotifyNavmeshesHaveChanged();
}


void aiNavMeshStoreInterfaceGta::ApplyAreaSwitchesToNewlyLoadedNavMesh(CNavMesh &mesh, aiNavDomain domain) const
{
	// Apply any region switches to disable ped generation & wandering in this navmesh
	CPathServer::ApplyPedAreaSwitchesToNewlyLoadedNavMesh(&mesh, domain);
}

#if __HIERARCHICAL_NODES_ENABLED

void aiNavMeshStoreInterfaceGta::ApplyAreaSwitchesToNewlyLoadedNavNodes(CHierarchicalNavData &navNodes) const
{
	CPathServer::ApplyPedAreaSwitchesToNewlyLoadedNavNodes(&navNodes);
}

#endif	// __HIERARCHICAL_NODES_ENABLED

// HACK_GTA
#define HACK_NUM_ADJACENCIES	64
void aiNavMeshStoreInterfaceGta::ApplyHacksToNewlyLoadedNavMesh(CNavMesh * pNavMesh, aiNavDomain domain) const
{
	if(domain != kNavDomainRegular)
		return;

	Vector3 vAdjacenciesToDisable[HACK_NUM_ADJACENCIES];
	int iAdjacencyCount = 0;

	const TNavMeshIndex iMesh = pNavMesh->GetIndexOfMesh();

	switch(iMesh)
	{
		case 3133:
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-945.5f, -1249.5f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-944.5f, -1248.9f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-943.2f, -1248.2f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-941.6f, -1247.3f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-940.4f, -1246.7f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-939.4f, -1246.2f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-937.5f, -1246.0f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-935.2f, -1247.5f, 7.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-933.9f, -1250.0f, 7.0f);
			break;

		case 3428:
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1713.6f, -765.7f, 9.5f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1697.5f, -778.3f, 9.4f);
			break;

		case 3527:
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1803.1f, -688.5f, 9.9f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1803.8f, -687.9f, 9.9f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1804.4f, -687.6f, 9.8f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1828.1f, -667.6f, 9.8f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1829.3f, -666.5f, 9.8f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1840.6f, -657.1f, 9.9f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1841.3f, -656.4f, 10.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1841.8f, -656.1f, 9.8f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1903.7f, -600.9f, 11.1f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1904.3f, -600.6f, 11.2f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1904.5f, -600.3f, 11.0f);
			break;

		case 3528:
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1795.2f, -694.4f, 9.8f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1795.7f, -694.1f, 9.9f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1796.4f, -693.4f, 9.8f);
			break;

		case 3626:
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1959.7f, -553.9f, 11.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1960.2f, -553.6f, 11.2f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1960.9f, -552.9f, 11.1f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1967.6f, -548.1f, 11.1f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1968.3f, -547.4f, 11.2f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1968.8f, -547.1f, 11.1f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1995.6f, -524.7f, 11.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1996.5f, -524.1f, 11.0f);
			break;

		case 3627:
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1916.7f, -590.0f, 11.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1917.4f, -589.5f, 11.1f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1923.9f, -584.7f, 11.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1924.3f, -584.2f, 11.2f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1924.8f, -583.8f, 11.2f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1925.3f, -583.6f, 11.1f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1934.7f, -575.0f, 11.0f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1935.3f, -574.6f, 11.2f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1935.9f, -574.0f, 11.0f);

			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1936.5f, -574.1f, 11.1f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1938.0f, -572.9f, 11.0f);
			
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1947.2f, -564.4f, 11.1f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1947.7f, -564.1f, 11.2f);
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-1948.6f, -563.4f, 11.0f);
			break;

		case 4336:
			vAdjacenciesToDisable[iAdjacencyCount++] = Vector3(-513.5f, 573.6f, 119.4f);
			break;
	}

	Assert(iAdjacencyCount < HACK_NUM_ADJACENCIES);
	iAdjacencyCount = Min(iAdjacencyCount, HACK_NUM_ADJACENCIES);

	for(s32 d=0; d<iAdjacencyCount; d++)
	{
		Vector3 vPos = vAdjacenciesToDisable[d];
		TAdjPoly * adjPoly = NULL;

		bool bFoundAdjacency = pNavMesh->GetNonStandardAdjacancyAtPosition(vPos, 1.0f, adjPoly);

		Assertf(bFoundAdjacency, "Adjacency not found in navmesh %i, at (%.2f, %.2f, %.2f)", iMesh, vPos.x, vPos.y, vPos.z);
		if(bFoundAdjacency)
		{
			if(adjPoly)
			{
				adjPoly->SetAdjacencyType(ADJACENCY_TYPE_NORMAL);
				adjPoly->SetNavMeshIndex(NAVMESH_NAVMESH_INDEX_NONE, pNavMesh->GetAdjacentMeshes());
				adjPoly->SetOriginalNavMeshIndex(NAVMESH_NAVMESH_INDEX_NONE, pNavMesh->GetAdjacentMeshes());
				adjPoly->SetPolyIndex(NAVMESH_POLY_INDEX_NONE);
				adjPoly->SetOriginalPolyIndex(NAVMESH_POLY_INDEX_NONE);
			}
		}
	}
}


bool aiNavMeshStoreInterfaceGta::IsDefragmentCopyBlocked() const
{
	LOCK_REQUESTS

	if(CPathServer::GetPathServerThread()->GetNumPendingRequests() > 0)
		return true;

	return false;
}

#if __BANK
bool aiNavMeshStoreInterfaceGta::ms_bDebugMapSwapUnmatchedAdjacencies = false;
#endif

void aiNavMeshStoreInterfaceGta::AdjoinNavmeshesAcrossMapSwap(CNavMesh * pMainMapMesh, CNavMesh * pSwapMesh, eNavMeshEdge iSideOfMainMapMesh) const
{
	DefaultAdjoinNavmeshesAcrossMapSwap(pMainMapMesh, pSwapMesh, iSideOfMainMapMesh);
}

struct TExternalEdgePolys
{
	Vector3 vPoly_v1;
	Vector3 vPoly_v2;
	TAdjPoly * pAdj;
	s32 pindex;
};	// 48 bytes

atArray<TExternalEdgePolys> externalNav2Polys(0, 256);	// 12kb

void aiNavMeshStoreInterfaceGta::RestoreNavmeshesAcrossMapSwap(CNavMesh * pMainMapMesh, CNavMesh * pSwappedBackMesh, eNavMeshEdge iSideOfMainMapMesh) const
{
	Assert( pMainMapMesh->GetDLCGroup()==-1 );
	Assert( pSwappedBackMesh->GetDLCGroup()==-1 );

	const float fSameVertEpsXYSqr = square(0.05f);
	const float fSameVertEpsZ = 0.25f;

	const float fSharedEdgeCoord =
		(iSideOfMainMapMesh == eNegX) ? pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.x :
		(iSideOfMainMapMesh == ePosX) ? pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.x + pMainMapMesh->GetExtents().x :
		(iSideOfMainMapMesh == eNegY) ? pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.y :
		pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.y + pMainMapMesh->GetExtents().y;

	s32 p1;
	s32 a1;

	pMainMapMesh->SetAdjoinedDLCGroup(iSideOfMainMapMesh, pSwappedBackMesh->GetDLCGroup());
	pSwappedBackMesh->SetAdjoinedDLCGroup(GetOppositeEdge(iSideOfMainMapMesh), pMainMapMesh->GetDLCGroup());

	pMainMapMesh->SetFlags( pMainMapMesh->GetFlags() | NAVMESH_MODIFIED_FOR_DLC_SWAP );
	pSwappedBackMesh->SetFlags( pSwappedBackMesh->GetFlags() | NAVMESH_MODIFIED_FOR_DLC_SWAP );

	//-------------------------------------------------------------------------------
	// Restore all non-standard adjacencies from the pMainMapMesh to the pSwappedBackMesh

	for(a1=0; a1<pMainMapMesh->GetSizeOfPools(); a1++)
	{
		TAdjPoly * pAdj = pMainMapMesh->GetAdjacentPolysArray().Get(a1);

		if( pAdj->GetOriginalNavMeshIndex(pMainMapMesh->GetAdjacentMeshes()) == pSwappedBackMesh->GetIndexOfMesh() )
		{
			if( pAdj->GetAdjacencyType() != ADJACENCY_TYPE_NORMAL )
			{
				pAdj->SetAdjacencyDisabled(false);
			}
		}
	}

	//------------------------------------------------------------------
	// Disable all special-links from the pMainMapMesh to the pSwappedBackMesh

	for(s32 s=0; s<pMainMapMesh->GetNumSpecialLinks(); s++)
	{
		CSpecialLinkInfo & specialLink = pMainMapMesh->GetSpecialLinks()[s];
		if(specialLink.GetOriginalLinkToNavMesh() == pSwappedBackMesh->GetIndexOfMesh())
		{
			specialLink.SetIsDisabled(false);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------------------------
	// Cache a list of which polygons are along the appropriate side of pSwapMesh, so as to avoid doing this for every poly in pMainMapMesh

	externalNav2Polys.clear();

	{
		Vector3 vPoly2_v1, vPoly2_v2;
		s32 p2, a2;

		for(p2=0; p2<pSwappedBackMesh->GetNumPolys(); p2++)
		{
			TNavMeshPoly * pPoly2 = pSwappedBackMesh->GetPoly(p2);
			if(pPoly2->GetLiesAlongEdgeOfMesh())
			{
				for(a2=0; a2<pPoly2->GetNumVertices(); a2++)
				{
					TAdjPoly * pAdj2 = pSwappedBackMesh->GetAdjacentPolysArray().Get( pPoly2->GetFirstVertexIndex() + a2 );
					if(pAdj2->GetIsExternalEdge()) // && pAdj2->GetNavMeshIndex(pSwappedBackMesh->GetAdjacentMeshes())==NAVMESH_NAVMESH_INDEX_NONE)
					{
						pSwappedBackMesh->GetVertex( pSwappedBackMesh->GetPolyVertexIndex(pPoly2, a2), vPoly2_v1 );

						if( VertexLiesAlongEdge(vPoly2_v1, iSideOfMainMapMesh, fSharedEdgeCoord) )
						{
							pSwappedBackMesh->GetVertex( pSwappedBackMesh->GetPolyVertexIndex(pPoly2, (a2+1)%pPoly2->GetNumVertices()), vPoly2_v2 );

							if( VertexLiesAlongEdge(vPoly2_v2, iSideOfMainMapMesh, fSharedEdgeCoord))
							{
								TExternalEdgePolys externalPoly;
								externalPoly.pAdj = pAdj2;
								externalPoly.vPoly_v1 = vPoly2_v1;
								externalPoly.vPoly_v2 = vPoly2_v2;
								externalPoly.pindex = p2;

								externalNav2Polys.PushAndGrow(externalPoly);
							}
						}
					}
				}
			}
		}
	}

	//-------------------------------------------
	// Retarget matching adjacencies along edge

	Vector3 vPoly1_v1, vPoly1_v2;

	for(p1=0; p1<pMainMapMesh->GetNumPolys(); p1++)
	{
		TNavMeshPoly * pPoly1 = pMainMapMesh->GetPoly(p1);
		if(pPoly1->GetLiesAlongEdgeOfMesh())
		{
			for(a1=0; a1<pPoly1->GetNumVertices(); a1++)
			{
				TAdjPoly * pAdj1 = pMainMapMesh->GetAdjacentPolysArray().Get( pPoly1->GetFirstVertexIndex() + a1 );
				if(pAdj1->GetIsExternalEdge())
				{
					bool bAdjacencyFound = false;

					pMainMapMesh->GetVertex( pMainMapMesh->GetPolyVertexIndex(pPoly1, a1), vPoly1_v1 );

					if(!VertexLiesAlongEdge(vPoly1_v1, iSideOfMainMapMesh, fSharedEdgeCoord))
						continue;

					pMainMapMesh->GetVertex( pMainMapMesh->GetPolyVertexIndex(pPoly1, (a1+1)%pPoly1->GetNumVertices()), vPoly1_v2 );

					if(!VertexLiesAlongEdge(vPoly1_v2, iSideOfMainMapMesh, fSharedEdgeCoord))
						continue;

					for(s32 f=0; f<externalNav2Polys.GetCount(); f++)
					{
						TExternalEdgePolys & edgePoly2 = externalNav2Polys[f];
						PrefetchDC(&externalNav2Polys.GetElements()[f+1]);

						if( (IsStitchVertSame( vPoly1_v1, edgePoly2.vPoly_v2, fSameVertEpsXYSqr, fSameVertEpsZ ) && IsStitchVertSame( vPoly1_v2, edgePoly2.vPoly_v1, fSameVertEpsXYSqr, fSameVertEpsZ ) ) ||
							(IsStitchVertSame( vPoly1_v1, edgePoly2.vPoly_v1, fSameVertEpsXYSqr, fSameVertEpsZ ) && IsStitchVertSame( vPoly1_v2, edgePoly2.vPoly_v2, fSameVertEpsXYSqr, fSameVertEpsZ ) ) )
						{
							pAdj1->SetOriginalNavMeshIndex( pSwappedBackMesh->GetIndexOfMesh(), pMainMapMesh->GetAdjacentMeshes() );
							pAdj1->SetNavMeshIndex( pSwappedBackMesh->GetIndexOfMesh(), pMainMapMesh->GetAdjacentMeshes() );
							pAdj1->SetOriginalPolyIndex( edgePoly2.pindex );
							pAdj1->SetPolyIndex( edgePoly2.pindex );
							pAdj1->SetAdjacencyType( ADJACENCY_TYPE_NORMAL );

							edgePoly2.pAdj->SetOriginalNavMeshIndex( pMainMapMesh->GetIndexOfMesh(), pSwappedBackMesh->GetAdjacentMeshes() );
							edgePoly2.pAdj->SetNavMeshIndex( pMainMapMesh->GetIndexOfMesh(), pSwappedBackMesh->GetAdjacentMeshes() );
							edgePoly2.pAdj->SetOriginalPolyIndex( p1 );
							edgePoly2.pAdj->SetPolyIndex( p1 );
							edgePoly2.pAdj->SetAdjacencyType( ADJACENCY_TYPE_NORMAL );

							bAdjacencyFound = true;
							break;
						}
					}

					//Assert(bAdjacencyFound);
					if(!bAdjacencyFound)
					{
						pAdj1->SetOriginalNavMeshIndex( NAVMESH_NAVMESH_INDEX_NONE, pMainMapMesh->GetAdjacentMeshes() );
						pAdj1->SetNavMeshIndex( NAVMESH_NAVMESH_INDEX_NONE, pMainMapMesh->GetAdjacentMeshes() );
						pAdj1->SetOriginalPolyIndex( NAVMESH_POLY_INDEX_NONE );
						pAdj1->SetPolyIndex( NAVMESH_POLY_INDEX_NONE );
						pAdj1->SetAdjacencyType( ADJACENCY_TYPE_NORMAL );

#if __BANK
						if(ms_bDebugMapSwapUnmatchedAdjacencies)
						{
							grcDebugDraw::Line(vPoly1_v1 + (ZAXIS*0.2f), vPoly1_v2 + (ZAXIS*0.2f), Color_green, Color_green, 4000);
						}
#endif
					}
				}
			}
		}
	}
}

// TODO : Add a new parameter, passed in from calling function to avoid repeatedly iterating edge polygons in pMainMapMesh for each side which required stitching
//		const atArray<TExternalEdgePolys> & inputEdgePolys

void DefaultAdjoinNavmeshesAcrossMapSwap(CNavMesh * pMainMapMesh, CNavMesh * pSwapMesh, eNavMeshEdge iSideOfMainMapMesh)
{
	//Assert( (pMainMapMesh->GetFlags()&NAVMESH_DLC)==0 );
	//Assert( (pSwapMesh->GetFlags()&NAVMESH_DLC)!=0 );

	Assert( pMainMapMesh->GetDLCGroup()==-1 );
	//Assert( pSwapMesh->GetSwapGroup()==-1 );

	const float fSameVertEpsXYSqr = square(0.05f);
	const float fSameVertEpsZ = 0.25f;

	const float fSharedEdgeCoord =
		(iSideOfMainMapMesh == eNegX) ? pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.x :
		(iSideOfMainMapMesh == ePosX) ? pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.x + pMainMapMesh->GetExtents().x :
		(iSideOfMainMapMesh == eNegY) ? pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.y :
		pMainMapMesh->GetNonResourcedData()->m_vMinOfNavMesh.y + pMainMapMesh->GetExtents().y;


	s32 p1;
	s32 a1;

	pMainMapMesh->SetAdjoinedDLCGroup(iSideOfMainMapMesh, pSwapMesh->GetDLCGroup());
	pSwapMesh->SetAdjoinedDLCGroup(GetOppositeEdge(iSideOfMainMapMesh), pMainMapMesh->GetDLCGroup());

	pMainMapMesh->SetFlags( pMainMapMesh->GetFlags() | NAVMESH_MODIFIED_FOR_DLC_SWAP );
	pSwapMesh->SetFlags( pSwapMesh->GetFlags() | NAVMESH_MODIFIED_FOR_DLC_SWAP );

	//-------------------------------------------------------------------------------
	// Disable all non-standard adjacencies from the pMainMapMesh to the pSwapMesh

	for(a1=0; a1<pMainMapMesh->GetSizeOfPools(); a1++)
	{
		TAdjPoly * pAdj = pMainMapMesh->GetAdjacentPolysArray().Get(a1);

		if( pAdj->GetOriginalNavMeshIndex(pMainMapMesh->GetAdjacentMeshes()) == pSwapMesh->GetIndexOfMesh() )
		{
			if( pAdj->GetAdjacencyType() == ADJACENCY_TYPE_NORMAL )
			{
				pAdj->SetOriginalNavMeshIndex( NAVMESH_NAVMESH_INDEX_NONE, pMainMapMesh->GetAdjacentMeshes() );
				pAdj->SetNavMeshIndex( NAVMESH_NAVMESH_INDEX_NONE, pMainMapMesh->GetAdjacentMeshes() );
				pAdj->SetOriginalPolyIndex( NAVMESH_POLY_INDEX_NONE );
				pAdj->SetPolyIndex( NAVMESH_POLY_INDEX_NONE );
			}
			else
			{
				pAdj->SetAdjacencyDisabled(true);
			}
		}
	}

	//------------------------------------------------------------------
	// Disable all special-links from the pMainMapMesh to the pSwapMesh

	for(s32 s=0; s<pMainMapMesh->GetNumSpecialLinks(); s++)
	{
		CSpecialLinkInfo & specialLink = pMainMapMesh->GetSpecialLinks()[s];
		if(specialLink.GetOriginalLinkToNavMesh() == pSwapMesh->GetIndexOfMesh())
		{
			specialLink.SetIsDisabled(true);
		}
	}

	//--------------------------------------------------------------------------------------------------------------------------------------
	// Cache a list of which polygons are along the appropriate side of pSwapMesh, so as to avoid doing this for every poly in pMainMapMesh

	externalNav2Polys.clear();

	{
		Vector3 vPoly2_v1, vPoly2_v2;
		s32 p2, a2;

		for(p2=0; p2<pSwapMesh->GetNumPolys(); p2++)
		{
			TNavMeshPoly * pPoly2 = pSwapMesh->GetPoly(p2);
			if(pPoly2->GetIsZeroAreaStichPolyDLC())
			{
				for(a2=0; a2<pPoly2->GetNumVertices(); a2++)
				{
					TAdjPoly * pAdj2 = pSwapMesh->GetAdjacentPolysArray().Get( pPoly2->GetFirstVertexIndex() + a2 );
					if(pAdj2->GetIsExternalEdge()) // && pAdj2->GetNavMeshIndex(pSwapMesh->GetAdjacentMeshes())==NAVMESH_NAVMESH_INDEX_NONE)
					{
						pSwapMesh->GetVertex( pSwapMesh->GetPolyVertexIndex(pPoly2, a2), vPoly2_v1 );

						if( VertexLiesAlongEdge(vPoly2_v1, iSideOfMainMapMesh, fSharedEdgeCoord) )
						{
							pSwapMesh->GetVertex( pSwapMesh->GetPolyVertexIndex(pPoly2, (a2+1)%pPoly2->GetNumVertices()), vPoly2_v2 );

							if( VertexLiesAlongEdge(vPoly2_v2, iSideOfMainMapMesh, fSharedEdgeCoord))
							{
								TExternalEdgePolys externalPoly;
								externalPoly.pAdj = pAdj2;
								externalPoly.vPoly_v1 = vPoly2_v1;
								externalPoly.vPoly_v2 = vPoly2_v2;
								externalPoly.pindex = p2;

								externalNav2Polys.PushAndGrow(externalPoly);
							}
						}
					}
				}
			}
		}
	}

	//-------------------------------------------
	// Retarget matching adjacencies along edge

	Vector3 vPoly1_v1, vPoly1_v2;

	for(p1=0; p1<pMainMapMesh->GetNumPolys(); p1++)
	{
		TNavMeshPoly * pPoly1 = pMainMapMesh->GetPoly(p1);
		
		if(pPoly1->GetLiesAlongEdgeOfMesh())
		{
			for(a1=0; a1<pPoly1->GetNumVertices(); a1++)
			{
				TAdjPoly * pAdj1 = pMainMapMesh->GetAdjacentPolysArray().Get( pPoly1->GetFirstVertexIndex() + a1 );
				if(pAdj1->GetIsExternalEdge())
				{
					bool bAdjacencyFound = false;

					pMainMapMesh->GetVertex( pMainMapMesh->GetPolyVertexIndex(pPoly1, a1), vPoly1_v1 );

					if(!VertexLiesAlongEdge(vPoly1_v1, iSideOfMainMapMesh, fSharedEdgeCoord))
						continue;

					pMainMapMesh->GetVertex( pMainMapMesh->GetPolyVertexIndex(pPoly1, (a1+1)%pPoly1->GetNumVertices()), vPoly1_v2 );

					if(!VertexLiesAlongEdge(vPoly1_v2, iSideOfMainMapMesh, fSharedEdgeCoord))
						continue;

					for(s32 f=0; f<externalNav2Polys.GetCount(); f++)
					{
						TExternalEdgePolys & edgePoly2 = externalNav2Polys[f];

						PrefetchDC(&externalNav2Polys.GetElements()[f+1]);

						if( (IsStitchVertSame( vPoly1_v1, edgePoly2.vPoly_v2, fSameVertEpsXYSqr, fSameVertEpsZ ) && IsStitchVertSame( vPoly1_v2, edgePoly2.vPoly_v1, fSameVertEpsXYSqr, fSameVertEpsZ ) ) ||
							(IsStitchVertSame( vPoly1_v1, edgePoly2.vPoly_v1, fSameVertEpsXYSqr, fSameVertEpsZ ) && IsStitchVertSame( vPoly1_v2, edgePoly2.vPoly_v2, fSameVertEpsXYSqr, fSameVertEpsZ ) ) )
						{
							pAdj1->SetOriginalNavMeshIndex( pSwapMesh->GetIndexOfMesh(), pMainMapMesh->GetAdjacentMeshes() );
							pAdj1->SetNavMeshIndex( pSwapMesh->GetIndexOfMesh(), pMainMapMesh->GetAdjacentMeshes() );
							pAdj1->SetOriginalPolyIndex( edgePoly2.pindex );
							pAdj1->SetPolyIndex( edgePoly2.pindex );
							pAdj1->SetAdjacencyType( ADJACENCY_TYPE_NORMAL );

							edgePoly2.pAdj->SetOriginalNavMeshIndex( pMainMapMesh->GetIndexOfMesh(), pSwapMesh->GetAdjacentMeshes() );
							edgePoly2.pAdj->SetNavMeshIndex( pMainMapMesh->GetIndexOfMesh(), pSwapMesh->GetAdjacentMeshes() );
							edgePoly2.pAdj->SetOriginalPolyIndex( p1 );
							edgePoly2.pAdj->SetPolyIndex( p1 );
							edgePoly2.pAdj->SetAdjacencyType( ADJACENCY_TYPE_NORMAL );

							bAdjacencyFound = true;
							break;
						}
					}

					//Assert(bAdjacencyFound);
					if(!bAdjacencyFound)
					{
						pAdj1->SetOriginalNavMeshIndex( NAVMESH_NAVMESH_INDEX_NONE, pMainMapMesh->GetAdjacentMeshes() );
						pAdj1->SetNavMeshIndex( NAVMESH_NAVMESH_INDEX_NONE, pMainMapMesh->GetAdjacentMeshes() );
						pAdj1->SetOriginalPolyIndex( NAVMESH_POLY_INDEX_NONE );
						pAdj1->SetPolyIndex( NAVMESH_POLY_INDEX_NONE );
						pAdj1->SetAdjacencyType( ADJACENCY_TYPE_NORMAL );

#if __BANK && defined(GTA_ENGINE)
						static dev_bool bDebugMapSwapUnmatchedAdjacencies = false;
						if(bDebugMapSwapUnmatchedAdjacencies)
						{
							grcDebugDraw::Line(vPoly1_v1 + (ZAXIS*0.25f), vPoly1_v2 + (ZAXIS*0.25f), Color_red, Color_red, 4000);
						}
#endif
					}
				}
			}
		}
	}
}



//-----------------------------------------------------------------------------
