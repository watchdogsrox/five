#include "glassPaneSyncing/GlassPaneInfo.h"
#include "glassPaneSyncing/GlassPaneManager.h"
#include "scene/world/GameWorld.h"
#include "Peds/Ped.h"
#include "objects/DummyObject.h"
#include "Vfx/misc/LODLightManager.h" // hashing func in GenerateHash() 

NETWORK_OPTIMISATIONS()

#if GLASS_PANE_SYNCING_ACTIVE

atMap<u32, RegdObj> CGlassPaneManager::sm_glassPaneGeometryObjects;
atFixedArray<CGlassPane, CGlassPaneManager::MAX_NUM_GLASS_PANE_OBJECTS> CGlassPaneManager::sm_glassPanes;
atFixedArray<CGlassPaneManager::GlassPaneRegistrationData, CGlassPaneManager::MAX_NUM_GLASS_PANE_POTENTIALS_PER_FRAME> CGlassPaneManager::sm_potentialGlassPanes;

#endif /* GLASS_PANE_SYNCING_ACTIVE */

/* static */ void CGlassPaneManager::Init(unsigned UNUSED_PARAM(initMode))
{}

/* static */ void CGlassPaneManager::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{
#if GLASS_PANE_SYNCING_ACTIVE

	Assert(CGlassPaneManager::sm_glassPaneGeometryObjects.GetNumUsed() == 0);
	Assert(CGlassPaneManager::sm_glassPanes.empty());
	Assert(CGlassPaneManager::sm_potentialGlassPanes.empty());

#endif /* GLASS_PANE_SYNCING_ACTIVE */
}

/* static */ void CGlassPaneManager::Update()
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	ProcessPotentialGlassPanes();

#if !__FINAL && DEBUG_RENDER_GLASS_PANE_MANAGER

	RenderPanes();
	RenderGeometry();

#endif /* !__FINAL && && DEBUG_RENDER_GLASS_PANE_MANAGER */ 
}

/* static */ void CGlassPaneManager::InitPools()
{
#if GLASS_PANE_SYNCING_ACTIVE
	CGlassPane::InitPool();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
}

/* static */ void CGlassPaneManager::ShutdownPools()
{
#if GLASS_PANE_SYNCING_ACTIVE
	CGlassPane::ShutdownPool();
#endif /* GLASS_PANE_SYNCING_ACTIVE */
}

/* static */ CGlassPane* CGlassPaneManager::CreateGlassPane(void)
{	
	CGlassPane* pPane = NULL;

#if GLASS_PANE_SYNCING_ACTIVE

	int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
	for(int i = 0; i < numPanes; ++i)
	{
		if(!CGlassPaneManager::sm_glassPanes[i].IsReserved())
		{
			CGlassPaneManager::sm_glassPanes[i].SetReserved(true);
			return &CGlassPaneManager::sm_glassPanes[i];
		}
	}
	
	CGlassPane& pane = CGlassPaneManager::sm_glassPanes.Append();
	pane.SetReserved(true);

	pPane = &pane;

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return pPane;
}

/* static */ bool CGlassPaneManager::DestroyGlassPane(CGlassPane* GLASS_PARAM(glassPane))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(glassPane)
	{
		if(IsGlassPaneRegistered(glassPane))
		{
			glassPane->Reset();
			return true;
		}
		else
		{
			Assert(false);
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

// we register glass objects (the actual window geometry object that's part of a building....
/* static */ bool CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkOwner(CObject const * GLASS_PARAM(object), Vector3 const& GLASS_PARAM(hitPosWS), u8 const GLASS_PARAM(hitComponent))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(!NetworkInterface::IsGameInProgress())
	{
		return false;
	}

	if(!object)
	{
		return false;
	}

	if(!object->m_nDEflags.bIsBreakableGlass)
	{
		return false;
	}

	// Something like an explosion will try to register the same object multiple times
	// Because the glass can be walked through after one hit, we have to only care 
	// about the first...
	if(CGlassPaneManager::IsGeometryRegisteredWithGlassPane(object) || CGlassPaneManager::IsGeometryPendingRegistrationWithGlassPane(object))
	{
		return NULL != GetGlassPaneByObject(object);
	}

	// Add it to the list of potentials to be processed after the physics has updated the graphical glass models....
	sm_potentialGlassPanes.Push(CGlassPaneManager::GlassPaneRegistrationData(const_cast<CObject*>(object), hitPosWS, hitComponent));
	return true;

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

/* static */ CGlassPane const * CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkClone(CObject const * GLASS_PARAM(object), GlassPaneInitData const& GLASS_PARAM(glassPaneInitData))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(!NetworkInterface::IsGameInProgress())
	{
		return NULL;
	}

	if(object)
	{
		Assertf(!CGlassPaneManager::IsGeometryRegisteredWithGlassPane(object), "%s : object already registered!\n", __FUNCTION__);
		Assertf(!CGlassPaneManager::IsGeometryPendingRegistrationWithGlassPane(object), "%s : object pending on clone?!\n", __FUNCTION__);
	}

	CGlassPane* newPane = CreateGlassPane();
	Assertf(newPane, "%s : Failed to create a new pane space", __FUNCTION__);
	
	if(newPane)
	{
		newPane->SetHitComponent(glassPaneInitData.m_hitComponent);
		newPane->SetHitPositionOS(glassPaneInitData.m_hitPosOS);

		if(object)
		{
			// Sets pos, radius, name hash
			newPane->SetGeometry(object);
		}
		else
		{
			newPane->SetGeometry(NULL);
			newPane->SetProxyGeometryHash(glassPaneInitData.m_geomHash);
			newPane->SetProxyPosition(glassPaneInitData.m_pos);
			newPane->SetProxyIsInside(glassPaneInitData.m_isInside);
			newPane->SetProxyRadius(glassPaneInitData.m_radius);
		}

		return newPane;
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}


bool CGlassPaneManager::RegisterGlassGeometryObject(CObject const* GLASS_PARAM(pObject))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(NetworkInterface::IsGameInProgress() &&pObject)
	{
		CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo();
		if(pModelInfo)
		{
			if( pModelInfo->GetFragType() && pModelInfo->GetFragType()->GetNumGlassPaneModelInfos() > 0)
			{
				//---
				// Generate the hash from this objects position and add to our map of active windows (check we've not added it before)..
				Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
				u32 hash = GenerateHash(pos);
#if __BANK
				if(sm_glassPaneGeometryObjects.Access(hash) != NULL)
				{
					Displayf("%s Trying to register: %p %s : pos %f %f %f : hash %d", __FUNCTION__, pObject, pObject ? pObject->GetDebugName() : "Null Obj", pos.x, pos.y, pos.z, hash);

					RegdObj* obj = sm_glassPaneGeometryObjects.Access(hash);
					Vector3 objPos(VEC3_ZERO);
					if(obj->Get())
					{
						objPos = VEC3V_TO_VECTOR3(obj->Get()->GetTransform().GetPosition());
					}
					Displayf("%s Existing Object: %p %s : pos %f %f %f : hash %d", __FUNCTION__, obj->Get(), obj->Get() ? obj->Get()->GetDebugName() : "Null Obj", objPos.x, objPos.y, objPos.z, hash);

					DebugPrintRegisteredGlassGeometryObjects();
				}
#endif /* __BANK */

				Assertf(sm_glassPaneGeometryObjects.Access(hash) == NULL, "%s Object %s : %f %f %f : already registered / object pos hash not unique?!", __FUNCTION__, pObject->GetDebugName(), pos.x, pos.y, pos.z);
				atMapEntry<u32, RegdObj>& entry = sm_glassPaneGeometryObjects.Insert(hash);
				entry.data = const_cast<CObject*>(pObject);

				//---

				CGlassPane* glassPane = CGlassPaneManager::AccessGlassPaneByGeometryHash(hash);
				if(glassPane)
				{
					// we've previously registered but streamed out so definitey shouldn't have a object pointer...
					Assert(!glassPane->GetGeometry());

					// this is the geometry we're expecting to be hooked up with?
					Assert(glassPane->GetGeometryHash() == hash);
	
					glassPane->SetGeometry(pObject);
					glassPane->ApplyDamage();

					return true;
				}
			}
		}
	}
	
#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

/* static */ bool CGlassPaneManager::UnregisterGlassGeometryObject(CObject const* GLASS_PARAM(object))
{
	bool removed = false;

#if GLASS_PANE_SYNCING_ACTIVE

	if(object)
	{
		// object may be dynamic and may have moved so we can't hash position (object may also not have a dummy)
		// iterate through the lot, find the one with the same value object pointer and get rid of it...
		for(geometry_itor cur_itor = sm_glassPaneGeometryObjects.CreateIterator(); !cur_itor.AtEnd(); cur_itor.Next())
		{
			// Same object...
			CObject* cur_object = cur_itor.GetData();

			if(cur_object == object) 
			{
				sm_glassPaneGeometryObjects.Delete(cur_itor.GetKey());
				removed = true;
				break;
			}
		}

		// CObjectPopulation can try to destroy a glass object before it's been registered
		// as we currently only register an object when we shoot it....
		if(IsGeometryRegisteredWithGlassPane(object))
		{
			CGlassPane* pane = AccessGlassPaneByObject(object);
			Assert(pane);

			if(pane)
			{
				pane->SetGeometry(NULL);
				removed = true;
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return removed;	
}

#if __BANK
/* static */ void CGlassPaneManager::DebugPrintRegisteredGlassGeometryObjects()
{
#if GLASS_PANE_SYNCING_ACTIVE

	Displayf("//------------------------------");
	Displayf("%s", __FUNCTION__);

	for(geometry_itor cur_itor = sm_glassPaneGeometryObjects.CreateIterator(); !cur_itor.AtEnd(); cur_itor.Next())
	{
		CObject* object = cur_itor.GetData();
		Vector3 pos(VEC3_ZERO);
		if(object)
		{
			pos = VEC3V_TO_VECTOR3(object->GetTransform().GetPosition());
		}
		Displayf("%p %s : %f %f %f", object, object ? object->GetDebugName() : "NULL", pos.x, pos.y, pos.z);
	}

	Displayf("//------------------------------");

#endif /* GLASS_PANE_SYNCING_ACTIVE */
}
#endif /* __BANK */

/* static */ bool CGlassPaneManager::IsGeometryObjectRegistered(CObject const * GLASS_PARAM(object))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(object)
	{
		for(geometry_itor cur_itor = sm_glassPaneGeometryObjects.CreateIterator(); !cur_itor.AtEnd(); cur_itor.Next())
		{
			// Same object...
			CObject* cur_object = cur_itor.GetData();

			if(cur_object == object) 
			{
				return true;
			}

			// Same position....
			u32 cur_hash = cur_itor.GetKey();

			Vector3 pos = VEC3V_TO_VECTOR3(object->GetTransform().GetPosition());
			u32 hash = GenerateHash(pos);

			if(cur_hash == hash)
			{
				return true;
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

/* static */ bool CGlassPaneManager::IsGeometryRegisteredWithGlassPane(CObject const * GLASS_PARAM(object))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(object)
	{
		int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
		for(int i = 0; i < numPanes; ++i)
		{
			if(object == CGlassPaneManager::sm_glassPanes[i].GetGeometry())
			{
				return true;
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

/* static */ bool CGlassPaneManager::IsGeometryPendingRegistrationWithGlassPane(CObject const * GLASS_PARAM(object))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(object)
	{
		// are we already registered for testing next frame?
		potential_itor itor		= sm_potentialGlassPanes.begin();
		potential_itor end_itor = sm_potentialGlassPanes.end();

		while(itor != end_itor)
		{
			if((*itor).m_object == object)
			{
				return true;
			}

			++itor;
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

/* static */ bool CGlassPaneManager::IsGlassPaneRegistered(CGlassPane const * GLASS_PARAM(glassPane))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(glassPane)
	{
		int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
		for(int i = 0; i < numPanes; ++i)
		{
			if(glassPane == &CGlassPaneManager::sm_glassPanes[i])
			{
				return true;
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

/* static */ CGlassPane const* CGlassPaneManager::GetGlassPaneByGeometryHash(u32 const GLASS_PARAM(hash))
{
#if GLASS_PANE_SYNCING_ACTIVE

	int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
	for(int i = 0; i < numPanes; ++i)
	{
		if(CGlassPaneManager::sm_glassPanes[i].IsReserved())
		{
			if(CGlassPaneManager::sm_glassPanes[i].GetGeometryHash() == hash)
			{
				return &CGlassPaneManager::sm_glassPanes[i];
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}

/* static */ CGlassPane* CGlassPaneManager::AccessGlassPaneByGeometryHash(u32 const GLASS_PARAM(hash))
{
#if GLASS_PANE_SYNCING_ACTIVE

	int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
	for(int i = 0; i < numPanes; ++i)
	{
		if(CGlassPaneManager::sm_glassPanes[i].IsReserved())
		{
			if(CGlassPaneManager::sm_glassPanes[i].GetGeometryHash() == hash)
			{
				return &CGlassPaneManager::sm_glassPanes[i];
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}

/* static */ CGlassPane const* CGlassPaneManager::GetGlassPaneByIndex(u32 const GLASS_PARAM(index))
{
#if GLASS_PANE_SYNCING_ACTIVE

	Assertf(index < CGlassPaneManager::sm_glassPanes.GetCount(), "CGlassPaneManager::GetGlassPane : Invalid Index");

	return &CGlassPaneManager::sm_glassPanes[index];

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}

/* static */ CGlassPane*	CGlassPaneManager::AccessGlassPaneByIndex(u32 const GLASS_PARAM(index))
{
#if GLASS_PANE_SYNCING_ACTIVE

	Assertf(index < CGlassPaneManager::sm_glassPanes.GetCount(), "CGlassPaneManager::AccessGlassPane : Invalid Index");

	return &CGlassPaneManager::sm_glassPanes[index];

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}


/* static */ CGlassPane const* CGlassPaneManager::GetGlassPaneByObject(CObject const* GLASS_PARAM(object))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(object)
	{
		int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
		for(int i = 0; i < numPanes; ++i)
		{
			if(CGlassPaneManager::sm_glassPanes[i].IsReserved())
			{
				if(CGlassPaneManager::sm_glassPanes[i].GetGeometry() == object)
				{
					return &CGlassPaneManager::sm_glassPanes[i];
				}
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}

/* static */ CGlassPane* CGlassPaneManager::AccessGlassPaneByObject(CObject const* GLASS_PARAM(object))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(object)
	{
		int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
		for(int i = 0; i < numPanes; ++i)
		{
			if(CGlassPaneManager::sm_glassPanes[i].IsReserved())
			{
				if(CGlassPaneManager::sm_glassPanes[i].GetGeometry() == object)
				{
					return &CGlassPaneManager::sm_glassPanes[i];
				}
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}

/* static */ bool CGlassPaneManager::MigrateGlassPane(CGlassPane const* GLASS_PARAM(glassPane))
{
#if GLASS_PANE_SYNCING_ACTIVE

	// Try and transfer the phantom glass pane object to another player...
	if(glassPane)
	{
		CNetObjGlassPane* glassPaneNetObj = SafeCast(CNetObjGlassPane, glassPane->GetNetworkObject());

		if(glassPaneNetObj && !glassPaneNetObj->IsClone())
		{
			const netPlayer* pNearestVisiblePlayer = NULL;
			float NETWORK_OFFSCREEN_REMOVAL_RANGE = CNetObjGlassPane::GetStaticScopeData().m_scopeDistance;
			float NETWORK_ONSCREEN_REMOVAL_RANGE  = CNetObjGlassPane::GetStaticScopeData().m_scopeDistance;
			if(NetworkInterface::IsVisibleOrCloseToAnyRemotePlayer(glassPane->GetDummyPosition(), 
																	glassPane->GetRadius(),
																	NETWORK_ONSCREEN_REMOVAL_RANGE, 
																	NETWORK_OFFSCREEN_REMOVAL_RANGE,
																	&pNearestVisiblePlayer
																	) )
			{
				if( ! ((CNetObjGlassPane*)glassPane->GetNetworkObject())->TryToPassControlOutOfScope() ) 
				{
					return true;
				}
			}
		}
	}	

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return false;
}

/* static */ Vector2 CGlassPaneManager::ConvertWorldToGlassSpace(CObject const* object, Vector3 const& posWS)
{
	if(object)
	{
		CDummyObject* dummy = const_cast<CObject*>(object)->GetRelatedDummy();
		
		Matrix34 m;

		if(dummy)
		{
			m = MAT34V_TO_MATRIX34(dummy->GetTransform().GetMatrix());
		}
		else 
		{
			m = MAT34V_TO_MATRIX34(object->GetTransform().GetMatrix());
		}
		
		m.Inverse();
		Vector3 temp = m * posWS;

		return Vector2(temp.x, temp.z);
	}

	Assertf(false, "ERROR : CGlassPaneManager::ConvertWorldToGlassSpace : Trying to register a geometry object with no geometry?!");
	return Vector2(0.f, 0.f);
}

/* static */ void CGlassPaneManager::ProcessPotentialGlassPanes()
{
#if GLASS_PANE_SYNCING_ACTIVE

	potential_itor itor		= sm_potentialGlassPanes.begin();
	potential_itor end_itor = sm_potentialGlassPanes.end();
	
	while(itor != end_itor)
	{
		GlassPaneRegistrationData const& registerData = (*itor);
		CObject const* obj		= registerData.m_object;
		u8 const hitComponent	= registerData.m_hitComponent;
		Vector3 const& hitPosWS = registerData.m_hitPos;

		if(obj)
		{
			fragInst* fragInstance = obj->GetFragInst();
			if(fragInstance)
			{
				if(fragInstance->GetType() && fragInstance->GetType()->GetNumGlassPaneModelInfos() > 0)
				{
					Assert(hitComponent < fragInstance->GetTypePhysics()->GetNumChildren());
					fragTypeChild* pChild = fragInstance->GetTypePhysics()->GetAllChildren()[hitComponent];
					Assert(pChild);
					int groupIndex = pChild->GetOwnerGroupPointerIndex();

					fragCacheEntry* fragCache = fragInstance->GetCacheEntry();
					if(fragCache)
					{
						if(fragCache->GetHierInst()->groupBroken->IsSet(groupIndex))
						{              
							//----

							CGlassPane* newPane = CreateGlassPane();
							Assertf(newPane, "%s : Failed to create a new pane space", __FUNCTION__);
							
							if(newPane)
							{
								// also sets pos, hash, isInside...
								newPane->SetHitPositionOS(ConvertWorldToGlassSpace(obj, hitPosWS));
								newPane->SetHitComponent(hitComponent);
								newPane->SetGeometry(obj);

								u16 globalFlags = 0;
								if (!NetworkInterface::GetObjectManager().RegisterGameObject(newPane, 0, globalFlags))
								{
									newPane->Reset();
									newPane->SetNetworkObject(NULL);

									Assertf(false, "%s : Failed to register game object", __FUNCTION__);
								}
							}

							//----
						}
					}
				}
			}
		}

		++itor;
	}

	// clear out all the potentials...
	sm_potentialGlassPanes.clear();

#endif /* GLASS_PANE_SYNCING_ACTIVE */
}

/* static */ u32 CGlassPaneManager::GenerateHash(Vector3 const& pos)
{
	u32 components[3];

	// multiply the position by 100 to go from metres to cm
	components[0] = u32(pos.x * 100.f);
	components[1] = u32(pos.y * 100.f);
	components[2] = u32(pos.z * 100.f);

	return CLODLightManager::hash(	components, 3, 0 );
}

/* static */ CObject* CGlassPaneManager::GetGeometryObject(u32 const GLASS_PARAM(hash))
{
#if GLASS_PANE_SYNCING_ACTIVE

	if(sm_glassPaneGeometryObjects.Access(hash))
	{
		return *(sm_glassPaneGeometryObjects.Access(hash));
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */

	return NULL;
}

#if !__FINAL && DEBUG_RENDER_GLASS_PANE_MANAGER

/* static */ void CGlassPaneManager::RenderPanes()
{
	if(!NetworkInterface::IsGameInProgress())
	{
		return;
	}

	CNetGamePlayer* pLocalPlayer = NetworkInterface::GetLocalPlayer();	
	Vector3 playerPos = VEC3V_TO_VECTOR3(pLocalPlayer->GetPlayerPed()->GetTransform().GetPosition());
	char buffer[2028] = "\0";

	int numPanesInUse = 0;

	int numPanes = CGlassPaneManager::sm_glassPanes.GetCount();
	for(int i = 0; i < numPanes; ++i)
	{
		CGlassPane const& pane = CGlassPaneManager::sm_glassPanes[i];

		// if the pane is in use....
		if(pane.IsReserved())
		{
			++numPanesInUse;

			char section[124]	= "\0";
			char line[1024]		= "\0";

			sprintf(section, "%d", i);
			strcat(line, section);

			Vector3 dummy_pos	= pane.GetDummyPosition();
			Vector3 geom_pos	= pane.GetGeometryPosition();
			float radius = pane.GetRadius();
	
//			Don't assert that the hash from the geometry position is the same as the one we've recieved over the network because 
//			the pane may not have any geometry so the position used to generate a hash is the serialised / less accurate one (i.e wrong)
//			u32 hash = GenerateHash(pane.GetGeometryPosition());
//			Assert(hash == pane.GetGeometryHash());
//			sprintf(section, ": pos %f %f %f", pos.x, pos.y, pos.z);
//			strcat(line, section);

			sprintf(section, ": pane %x : p %.2f %.2f %.2f : h %d : r %.2f", &pane, dummy_pos.x, dummy_pos.y, dummy_pos.z, pane.GetGeometryHash(), radius);
			strcat(line, section);

			sprintf(section, ": geom %x", pane.GetGeometry());
			strcat(line, section);

			sprintf(section, ": net %x", pane.GetNetworkObject());
			strcat(line, section);
			
			Color32 col = !pane.GetNetworkObject()->IsClone() ? Color_blue : Color_red;

			if(pane.GetNetworkObject())
			{
				grcDebugDraw::Sphere(geom_pos, 3.0f, col, false);
			}

			if(pane.GetGeometry())
			{
				CDummyObject* dummyObj = const_cast<CObject*>(pane.GetGeometry())->GetRelatedDummy();

				if(dummyObj)
				{
					grcDebugDraw::Line(geom_pos, VEC3V_TO_VECTOR3(dummyObj->GetTransform().GetPosition()), col, Color_purple);
					grcDebugDraw::Sphere(dummyObj->GetTransform().GetPosition(), 1.0f, Color_purple, false);
				}
			}
			
			if(pane.GetGeometry())
			{
				Matrix34 m = MAT34V_TO_MATRIX34(pane.GetGeometry()->GetTransform().GetMatrix());
				Vector3 v3posOS(pane.GetHitPositionOS(), Vector2::kXZ);
				m.Transform(v3posOS);

				grcDebugDraw::Sphere(v3posOS, 0.3f, Color_orange, false);
			}

			sprintf(section, ": dist %f", playerPos.Dist(dummy_pos));
			strcat(line, section);

			strcat(buffer, line);
			strcat(buffer, "\n");
		}
	}
	
	char temp[100];
	sprintf(temp, "Glass Pane Manager : Num Panes = %d\n", numPanesInUse);
	strcat(buffer, temp);
	grcDebugDraw::Text(CGameWorld::FindLocalPlayer()->GetTransform().GetPosition(), Color_white, buffer);

}

/* static */ void CGlassPaneManager::RenderGeometry(void)
{
#if GLASS_PANE_SYNCING_ACTIVE

	u32 numSlots = sm_glassPaneGeometryObjects.GetNumSlots();
	for(int s = 0; s < numSlots; ++s)
	{
		atMapEntry<u32, RegdObj>* entry  = sm_glassPaneGeometryObjects.GetEntry(s);

		if(entry)
		{
			CObject* pObject = entry->data;

			if(pObject)
			{
			//	if(AccessGlassPaneByObject(pObject))
				{
					spdAABB aabb;
					pObject->GetAABB(aabb);
				
					if(pObject->GetIsInInterior())
					{
						grcDebugDraw::BoxOriented(aabb.GetMin(), aabb.GetMax(), MATRIX34_TO_MAT34V(M34_IDENTITY), Color_blue, false);
					}
					else
					{
						grcDebugDraw::BoxOriented(aabb.GetMin(), aabb.GetMax(), MATRIX34_TO_MAT34V(M34_IDENTITY), Color_orange, false);
					}

					fragInst* objFragInst = pObject->GetFragInst();
					
					u8 numComponents = 0;

					if(objFragInst)
					{
						numComponents = objFragInst->GetTypePhysics()->GetNumChildren();
					}

					char buffer[1024];
					Vector3 pos = VEC3V_TO_VECTOR3(pObject->GetTransform().GetPosition());
					//sprintf(buffer, "%x\n%d\n%d\n%s %s %d\n%f %f %f\n", (int)pObject, numComponents, GenerateHash(pos), pObject->GetModelName(), pModelInfo->GetModelName(), pModelInfo->GetModelNameHash(), pos.x, pos.y, pos.z);
					//grcDebugDraw::Text(pObject->GetTransform().GetPosition(), Color_white, buffer);
			
					sprintf(buffer, "%x\n%d\n", (int)pObject, numComponents);
					CPed *pLocalPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
					grcDebugDraw::Text(VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()), Color_white, buffer);
				}
			}
		}
	}

#endif /* GLASS_PANE_SYNCING_ACTIVE */
}

#endif /* !__FINAL && DEBUG_RENDER_GLASS_PANE_MANAGER */