#include "objects/object.h"
#include "glassPaneSyncing/GlassPaneInfo.h"
#include "glassPaneSyncing/GlassPane.h"
#include "glassPaneSyncing/GlassPaneManager.h"
#include "Objects/DummyObject.h"

#if GLASS_PANE_SYNCING_ACTIVE
FW_INSTANTIATE_BASECLASS_POOL(CGlassPane, CGlassPane::MAX_PANES, atHashString("CGlassPane",0x7f06ad7f), sizeof(CGlassPane));
#endif

CGlassPane::CGlassPane()
:
	m_geometry(NULL),
	m_hitPosOS(0.0f, 0.0f),
	m_hitComponent(0),
	m_reserved(false)
{}

CGlassPane::~CGlassPane()
{}

Vector3::Return CGlassPane::GetGeometryPosition(void) const
{
	if(m_geometry)
	{
		return VEC3V_TO_VECTOR3(m_geometry->GetTransform().GetPosition());
	}

	return m_geometryProxy.m_pos;
}

Vector3::Return	CGlassPane::GetDummyPosition(void) const
{
	if(m_geometry)
	{
		CDummyObject* dummy = m_geometry->GetRelatedDummy();
		Assert(dummy);

		if(dummy)
		{
			return VEC3V_TO_VECTOR3(dummy->GetTransform().GetPosition());
		}
	}

	return m_geometryProxy.m_pos;
}

u32 CGlassPane::GetGeometryHash(void) const
{
	if(m_geometry)
	{
		Vector3 geomPos = GetGeometryPosition();
		return CGlassPaneManager::GenerateHash(geomPos);
	}

	return m_geometryProxy.m_geomHash;
}

float CGlassPane::GetRadius(void) const
{
	if(m_geometry)
	{
		CBaseModelInfo* pModelInfo = m_geometry->GetBaseModelInfo();
		Assert(pModelInfo);
		Assert(pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0);

		return pModelInfo->GetBoundingSphereRadius();
	}

	return m_geometryProxy.m_radius;
}

bool CGlassPane::GetIsInInterior(void) const
{
	if(m_geometry)
	{
		return m_geometry->GetIsInInterior();
	}

	return m_geometryProxy.m_isInside;
}

void CGlassPane::Reset(void)
{
	m_geometry	= NULL;
	m_geometryProxy.Reset();
	
	m_hitPosOS.Zero();
	m_hitComponent = 0;
	
	m_reserved  = false;
}

void CGlassPane::SetGeometry(CObject const* object)	
{
	m_geometry = const_cast<CObject*>(object);		

	if(m_geometry)
	{
		CBaseModelInfo* pModelInfo = m_geometry->GetBaseModelInfo();
		Assert(pModelInfo);
		Assert(pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0);

		// When setting geometry on object creation the dummy may not be set yet.
		// Can't just wait until dummy has been set as objects are not always created
		// set from dummies if the player teleports from far enough away.
		// Can just use geometry position as, it it's on create, geometry hasn't moved yet...
		CDummyObject* dummy = m_geometry->GetRelatedDummy();
		
		Vector3 position(VEC3_ZERO);

		if(dummy)
		{
			position = VEC3V_TO_VECTOR3(dummy->GetTransform().GetPosition());	
		}
		else
		{
			position = VEC3V_TO_VECTOR3(m_geometry->GetTransform().GetPosition());
		}

		SetProxyPosition(position);
		SetProxyIsInside(m_geometry->GetIsInInterior());
		SetProxyRadius(pModelInfo->GetBoundingSphereRadius());
		SetProxyGeometryHash(CGlassPaneManager::GenerateHash(position));
	}
}

void CGlassPane::ApplyDamage(void)
{
	if(m_geometry)
	{
		fragInst* objFragInst = m_geometry->GetFragInst();

		if(objFragInst)
		{
			// group Index...
			fragTypeChild* pChild = objFragInst->GetTypePhysics()->GetAllChildren()[m_hitComponent];
			int groupIndex = pChild->GetOwnerGroupPointerIndex();

			// position
			Vector3 pos(m_hitPosOS, Vector2::kXZ);
			Vec3V_In posWS = m_geometry->GetTransform().Transform(VECTOR3_TO_VEC3V(pos));

			// impulse
			Vector3 impactWS(-10.f, 0.0f, 0.0f);

			// if we have already broken this piece off...
			if( (objFragInst->GetCacheEntry() && objFragInst->GetCacheEntry()->GetHierInst() && !objFragInst->GetCacheEntry()->GetHierInst()->groupBroken->IsClear(groupIndex) ) )
			{
				return;
			}

			// Set the group damage health...
			objFragInst->ReduceGroupDamageHealth(groupIndex, 0.0f, posWS, VECTOR3_TO_VEC3V(impactWS) FRAGMENT_EVENT_PARAM(true));
		}
	}
}