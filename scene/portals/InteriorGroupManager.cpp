#include "scene/portals/InteriorGroupManager.h"
#include "scene/portals/PortalInst.h"

#if __BANK
#include "bank/bkmgr.h"
#include "grcore/debugdraw.h"
#endif // __BANK

#include "fwmaths/geomutil.h"

SCENE_OPTIMISATIONS()

atMap< u32, atArray< const CInteriorInst* > >	CInteriorGroupManager::ms_exits;
atMap< const CInteriorInst*, atArray< spdAABB > >		CInteriorGroupManager::ms_portalBounds;

#if __BANK
bool		CInteriorGroupManager::ms_useGroupPolicyForPopulation = true;
bool		CInteriorGroupManager::ms_showExitPortals = false;
#endif // __BANK

void CInteriorGroupManager::Init()
{
	ms_exits.Kill();
	ms_portalBounds.Kill();
}

void CInteriorGroupManager::Shutdown()
{
	ms_exits.Kill();
	ms_portalBounds.Kill();
}

void CInteriorGroupManager::RegisterGroupExit(const CInteriorInst* pIntInst)
{
	Assertf( pIntInst->GetGroupId() != 0, "This interior is not in a group" );
	Assertf( pIntInst->GetNumExitPortals() > 0, "This interior has no view to the outside" );
	
	if ( ms_portalBounds.Access( pIntInst ) )
	{
		return;
	}

	const u32	groupId = pIntInst->GetGroupId();

	if ( !ms_exits.Access( groupId ) )
		ms_exits[ groupId ].Reset();
	ms_exits[ groupId ].Grow() = pIntInst;
	
	atArray< spdAABB >&	portalBounds = ms_portalBounds[ pIntInst ];
	portalBounds.Reset();
	for (u32 i = 0; i < pIntInst->GetRoomZeroPortalInsts().GetCount(); ++i)
	{
		CPortalInst*	portalInst = pIntInst->GetRoomZeroPortalInsts()[i];

		if ( portalInst && !portalInst->IsLinkPortal() && !portalInst->GetIsOneWayPortal() )
		{
			spdAABB	tempBox;
			const spdAABB &box = portalInst->GetBoundBox( tempBox );
			portalBounds.Grow() = box;
		}
	}
}

void CInteriorGroupManager::UnregisterGroupExit(const CInteriorInst* pIntInst)
{
	Assertf( pIntInst->GetGroupId() != 0, "This interior is not in a group" );
	Assertf( pIntInst->GetNumExitPortals() > 0, "This interior has no view to the outside" );

	if (!ms_portalBounds.Access( pIntInst )) 
	{
		return;
	}

	ms_exits[ pIntInst->GetGroupId() ].DeleteMatches( pIntInst );
	ms_portalBounds.Delete( pIntInst );
}

bool CInteriorGroupManager::HasGroupExits(const u32 groupId) {
	return ms_exits.Access( groupId ) && ms_exits[ groupId ].GetCount() > 0;
}

bool CInteriorGroupManager::RayIntersectsExitPortals(const u32 groupId, const Vector3& rayStart, const Vector3& rayDirection)
{
	if ( ms_exits.Access( groupId ) )
	{
		const atArray< const CInteriorInst* >&	interiors = ms_exits[ groupId ];
		for (u32 i = 0; i < interiors.GetCount(); ++i)
		{
			const CInteriorInst*		interior = interiors[i];
			const atArray< spdAABB >&		portalBounds = ms_portalBounds[ interior ];
			for (u32 j = 0; j < portalBounds.GetCount(); ++j)
			{
				const spdAABB&	bound = portalBounds[j];
				Vector3			v0, v1;

				if ( fwGeom::IntersectBoxRay( bound, rayStart, rayDirection ) )
					return true;
			}
		}
	}

	return false;
}

#if __BANK
void CInteriorGroupManager::AddWidgets(bkBank* pBank)
{
	Assert(pBank);
	pBank->PushGroup( "Interior Groups", false );
	pBank->AddToggle( "Use group policy for interior population", &ms_useGroupPolicyForPopulation );
	pBank->AddToggle( "Show group exit portals", &ms_showExitPortals );
	pBank->PopGroup();
}

void CInteriorGroupManager::DrawDebug()
{
	if ( ms_showExitPortals )
	{
		atMap< const CInteriorInst*, atArray< spdAABB > >::Iterator	interiorIter = ms_portalBounds.CreateIterator();
		for (interiorIter.Start(); !interiorIter.AtEnd(); interiorIter.Next())
		{
			const atArray< spdAABB >&		bounds = interiorIter.GetData();
			for (u32 i = 0; i < bounds.GetCount(); ++i)
			{
				const spdAABB&	bound = bounds[i];

				grcDebugDraw::BoxAxisAligned( bound.GetMin(), bound.GetMax(), Color_red, false );
			}
		}
	}
}
#endif // __BANK
