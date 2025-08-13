
// all script commands which manipulate or modify contents of interiors

// Rage headers
#include "script/wrapper.h"

// Game headers
#include "camera/viewports/ViewportManager.h"
#include "camera/CamInterface.h"
#include "frontend/PauseMenu.h"
#include "Objects/object.h"
#include "Peds/ped.h"
#include "pickups/PickupManager.h"
#include "scene/portals/InteriorInst.h"
#include "scene/portals/Portal.h"
#include "scene/portals/PortalTracker.h"
#include "scene/portals/InteriorProxy.h"
#include "fwscene/search/SearchVolumes.h"
#include "fwsys/metadatastore.h"
#include "renderer/PostScan.h"
#include "scene/world/GameWorld.h"
#include "script/Handlers/GameScriptResources.h"
#include "script/script.h"
#include "script/script_channel.h"
#include "script/script_helper.h"
#include "script/commands_graphics.h"
#include "shaders/CustomShaderEffectInterior.h"
#include "scene/portals/LayoutManager.h"
#include "control/replay/replay.h"
#include "control/replay/Misc/InteriorPacket.h"
#include "replaycoordinator/ReplayCoordinator.h"
#include "Vfx/Decals/DecalManager.h"

struct sInteriorFile
{
	scrValue nameHash;
	scrValue numberOfRooms;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInteriorFile);

struct sInteriorRoom
{
	scrValue nameHash;
	scrValue numberOfLayoutNodes;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInteriorRoom);

struct sInteriorLayoutNode
{
	scrValue	nameHash;
	scrVector	translation;
	scrVector	rotation;
	scrValue	purchasable;
	scrValue numberOfGroupNodes;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInteriorLayoutNode);

struct sInteriorGroupNode
{
	scrValue numberOfRsRefNodes;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInteriorGroupNode);

struct sInteriorRsRefNode
{
	scrValue	nameHash;
	scrVector	translation;
	scrVector	rotation;
	scrValue	numberOfLayoutNodes;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sInteriorRsRefNode);

struct sRsRefShopData
{
	scrValue	name;
	scrValue	price;
	scrValue	description;
};

SCR_DEFINE_NEW_POINTER_PARAM_AND_RET(sRsRefShopData);

namespace interior_commands
{
	s32 CommandLayoutLoadInteriorInfo(const char* roomFileName)
	{
		u32 handle = g_LayoutManager.LoadLayout(roomFileName);
		return handle;	
	}
	s32 CommandGetRoomHandleFromInterior(int interiorHandle, int roomIndex)
	{
		CMiloInterior* interiorNode = g_LayoutManager.GetInteriorFromHandle(interiorHandle);
		if(roomIndex >=0 && roomIndex < interiorNode->m_RoomList.GetCount())
		{
			CMiloRoom* node = &interiorNode->m_RoomList[roomIndex];
			u32 handle = g_LayoutManager.CreateHandle(node);
			Displayf("room handle : %d",handle);
			return handle;
		}		
		return 0;
	}
	s32 CommandGetLayoutHandleFromRoom(int roomHandle, int layoutIndex)
	{
		CMiloRoom* roomNode = g_LayoutManager.GetRoomFromHandle(roomHandle);
		if(layoutIndex >=0 && layoutIndex < roomNode->m_LayoutNodeList.GetCount())
		{
			CLayoutNode* node = &roomNode->m_LayoutNodeList[layoutIndex];
			u32 handle = g_LayoutManager.CreateHandle(node);
			Displayf("layout handle : %d",handle);
			return handle;
		}		
		return 0;
	}
	s32 CommandGetGroupHandleFromLayout(int layoutHandle, int groupIndex)
	{
		CLayoutNode* layoutNode = g_LayoutManager.GetLayoutNode(layoutHandle);
		if(groupIndex >=0 && groupIndex < layoutNode->m_GroupList.GetCount())
		{
			CGroup* node = layoutNode->m_GroupList[groupIndex];
			return g_LayoutManager.CreateHandle(node);
		}		
		return 0;
	}
	s32 CommandGetRsRefHandleFromGroup(int groupHandle, int rsRefIndex)
	{
		CGroup* groupNode = g_LayoutManager.GetGroupNode(groupHandle);
		if(rsRefIndex >=0 && rsRefIndex < groupNode->m_RefList.GetCount())
		{
			CRsRef* node = groupNode->m_RefList[rsRefIndex];
			return g_LayoutManager.CreateHandle(node);
		}		
		return 0;
	}
	s32 CommandGetLayoutHandleFromRsRef(int rsRefHandle, int layoutIndex)
	{
		CRsRef* refNode = g_LayoutManager.GetRsRefNode(rsRefHandle);
		if(layoutIndex >=0 && layoutIndex < refNode->m_LayoutNodeList.GetCount())
		{
			CLayoutNode* node = refNode->m_LayoutNodeList[layoutIndex];
			return g_LayoutManager.CreateHandle(node);
		}		
		return 0;
	}
	bool CommandLayoutGetInteriorInfo(int interiorHandle, sInteriorFile* interiorInfo)
	{
		Displayf("INTERIOR HANDLE: %d %u", interiorHandle, (u32)interiorHandle);
		CMiloInterior* interiorNode = g_LayoutManager.GetInteriorFromHandle(interiorHandle);
		if(interiorNode)
		{
			interiorInfo->numberOfRooms.Int = interiorNode->m_RoomList.GetCount();
			return true;
		}
		return false;
	}
	bool CommandLayoutGetRoomInfo(int roomHandle, sInteriorRoom* roomInfo)
	{
		Displayf("ROOM HANDLE: %d %u", roomHandle, (u32)roomHandle);
		CMiloRoom* roomNode = g_LayoutManager.GetRoomFromHandle(roomHandle);
		if(roomNode)
		{
			roomInfo->numberOfLayoutNodes.Int = roomNode->m_LayoutNodeList.GetCount();
			return true;
		}
		return false;
	}
	bool CommandLayoutGetLayoutInfo(int layoutHandle, sInteriorLayoutNode* layoutInfo)
	{
		CLayoutNode* layoutNode = g_LayoutManager.GetLayoutNode(layoutHandle);
		if(layoutNode)
		{
			layoutInfo->nameHash.Int = atHashString(layoutNode->m_Name).GetHash();
			layoutInfo->purchasable.Int = static_cast<int>(layoutNode->m_Purchasable);
			layoutInfo->rotation.x = layoutNode->m_Rotation.x;
			layoutInfo->rotation.y = layoutNode->m_Rotation.y;
			layoutInfo->rotation.z = layoutNode->m_Rotation.z;
			layoutInfo->translation.x = layoutNode->m_Translation.x;
			layoutInfo->translation.y = layoutNode->m_Translation.y;
			layoutInfo->translation.z = layoutNode->m_Translation.z;
			layoutInfo->numberOfGroupNodes.Int = layoutNode->m_GroupList.GetCount();
			return true;
		}
		return false;
	}
	bool CommandLayoutGetGroupInfo(int groupHandle, sInteriorGroupNode* groupInfo)
	{
		CGroup* groupNode = g_LayoutManager.GetGroupNode(groupHandle);
		if(groupNode)
		{
			groupInfo->numberOfRsRefNodes.Int = groupNode->m_RefList.GetCount();
			return true;
		}
		return false;
	}
	void CommandLayoutUnloadInteriorInfo()
	{
		g_LayoutManager.UnloadLayout();
	}
	bool CommandLayoutGetRsRefInfo(int rsRefHandle, sInteriorRsRefNode* rsRefInfo, sRsRefShopData* rsRefShopData)
	{
		CRsRef* rsRefNode = g_LayoutManager.GetRsRefNode(rsRefHandle);
		if(rsRefNode)
		{
			rsRefInfo->nameHash.Int			= atHashString(rsRefNode->m_Name);
			rsRefInfo->rotation.x			= rsRefNode->m_Rotation.x;
			rsRefInfo->rotation.y			= rsRefNode->m_Rotation.y;
			rsRefInfo->rotation.z			= rsRefNode->m_Rotation.z;
			rsRefInfo->translation.x		= rsRefNode->m_Translation.x;
			rsRefInfo->translation.y		= rsRefNode->m_Translation.y;
			rsRefInfo->translation.z		= rsRefNode->m_Translation.z;
			rsRefShopData->description.Int	= atHashString(rsRefNode->m_ShopData.m_Description);
			rsRefShopData->name.Int			= atHashString(rsRefNode->m_ShopData.m_Name);
			rsRefShopData->price.Float		= rsRefNode->m_ShopData.m_Price;
			rsRefInfo->numberOfLayoutNodes.Int	= rsRefNode->m_LayoutNodeList.GetCount();
			return true;
		}
		return false;
	}

	void CommandToggleGroupInLayout(int groupHandle)
	{
		CGroup* groupNode = g_LayoutManager.GetGroupNode(groupHandle);
		if (SCRIPT_VERIFY(groupNode, "ACTIVATE_GROUP_IN_LAYOUT - invalid group handle"))
		{
			bool bCurrVal = g_LayoutManager.IsActiveGroupId(groupNode->m_Id);
			g_LayoutManager.SetActiveStateForGroup(groupNode->m_Id, !bCurrVal);
		}
	}

	bool CommandIsLayoutGroupActive(int groupHandle)
	{
		CGroup* groupNode = g_LayoutManager.GetGroupNode(groupHandle);
		if (SCRIPT_VERIFY(groupNode, "IS_GROUP_ACTIVE_IN_LAYOUT - invalid group handle"))
		{
			return(g_LayoutManager.IsActiveGroupId(groupNode->m_Id));
		}
		return false;
	}

	void CommandPopulateLayout(int InteriorProxyIndex)
	{
		// get the interior 
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
		if (SCRIPT_VERIFY(pIntProxy, "POPULATE_LAYOUT - Interior proxy doesn't exist"))
		{
			CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();
			if (SCRIPT_VERIFY(pIntInst, "POPULATE_LAYOUT - Interior instance doesn't exist yet"))
			{
				g_LayoutManager.PopulateInteriorWithLayout(pIntInst);
			}
		}
	}

	void CommandDepopulateLayout()
	{
		g_LayoutManager.DeleteLayout();
	}

	float CommandGetInteriorHeading( int InteriorProxyIndex )
	{
		float fHeading = 0.0f;
		// get the interior 
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
		if (SCRIPT_VERIFY(pIntProxy, "GET_INTERIOR_HEADING - Interior proxy doesn't exist"))
		{
			fHeading = pIntProxy->GetHeading();
		}
		return fHeading;
	}

	void CommandGetInteriorLocAndNamehash( int InteriorProxyIndex, Vector3& position, int& nameHash)
	{
		position = Vector3(0.0f, 0.0f, 0.0f);
		nameHash = 0;

		// get the interior 
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
		if (SCRIPT_VERIFY(pIntProxy, "GET_INTERIOR_LOCATION_AND_NAMEHASH- Interior proxy doesn't exist"))
		{
			nameHash = pIntProxy->GetNameHash();
			Vec3V vecPosition;
			pIntProxy->GetPosition(vecPosition);
			position = VEC3V_TO_VECTOR3(vecPosition);
		}
	}

	int CommandGetInteriorGroupID( int InteriorProxyIndex )
	{
		int groupID = 0;
		// get the interior 
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
		if (SCRIPT_VERIFY(pIntProxy, "GET_INTERIOR_GROUP_ID- Interior proxy doesn't exist"))
		{
			groupID = pIntProxy->GetGroupId();
		}
		return groupID;
	}

	scrVector CommandGetOffsetFromInteriorInWorldCoords(int InteriorProxyIndex, const scrVector & scrVecOffset)
	{
		// new code
		CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
		Vector3 NewCoors = scrVecOffset;

		// get the interior 
		if (SCRIPT_VERIFY(pIntProxy, "GET_OFFSET_FROM_INTERIOR_IN_WORLD_COORDS - Interior proxy doesn't exist"))
		{
			Mat34V proxyMat = pIntProxy->GetMatrix();
			Vec3V newCoorsVec(NewCoors);
			newCoorsVec = Transform(proxyMat, newCoorsVec);
			NewCoors = VEC3V_TO_VECTOR3(newCoorsVec);
		}

		return NewCoors;
	}

	bool CommandIsInteriorScene()
	{
		return CPortal::IsInteriorScene();
	}

	// check if the given instance index is valid
	bool CommandIsValidInterior(int InteriorProxyIndex) {
		return InteriorProxyIndex > NULL_IN_SCRIPTING_LANGUAGE;
	}

// camera related
	//Interior rooms
	void SetRoomForGameViewportByKey(int UNUSED_PARAM(roomKey))
	{
		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(SCRIPT_VERIFY(gameViewport, "SET_ROOM_FOR_GAME_VIEWPORT_BY_* - The game viewport does not exist"))
		{
			//Get the portal tracker from the viewport and make sure it is in an interior.
			int count = RENDERPHASEMGR.GetRenderPhaseCount();
			for(s32 i=0; i<count; i++)
			{
				CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(i);

				if (renderPhase.IsScanRenderPhase())
				{
					CRenderPhaseScanned &scanPhase = (CRenderPhaseScanned &) renderPhase;

					if (scanPhase.IsUpdatePortalVisTrackerPhase())
					{
						CPortalTracker* portalTracker = scanPhase.GetPortalVisTracker();
						if(SCRIPT_VERIFY(portalTracker, "SET_ROOM_FOR_GAME_VIEWPORT_BY_* - The game viewport does not have a portal tracker"))
						{
							Vector3 viewportPosition = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());
							portalTracker->RequestRescanNextUpdate();
							portalTracker->Update(viewportPosition);
							CInteriorInst* pIntInst = portalTracker->GetInteriorInst();
							if(pIntInst) {
								pIntInst->ForceFadeIn();
							}
						}
					}
				}
			}
		}
	}

	void SetRoomForGameViewportByName(const char* roomName)
	{
		int roomKey = atStringHash(roomName);
		SetRoomForGameViewportByKey(roomKey);
	}

	int GetRoomKeyForGameViewport()
	{
		int roomKey = 0;

		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(SCRIPT_VERIFY(gameViewport, "GET_ROOM_KEY_FOR_GAME_VIEWPORT - The game viewport does not exist"))
		{
			if (g_SceneToGBufferPhaseNew)
			{
				CPortalTracker* portalTracker = g_SceneToGBufferPhaseNew->GetPortalVisTracker();
				if(SCRIPT_VERIFY(portalTracker, "GET_ROOM_KEY_FOR_GAME_VIEWPORT - The game viewport does not have a portal tracker"))
				{
					roomKey = (portalTracker->GetInteriorInst() != NULL) ? (int)portalTracker->GetCurrRoomHash() : -1;
				}
			}
		}

		return(roomKey);
	}

	void ClearRoomForGameViewport()
	{
		CViewport* gameViewport = gVpMan.GetGameViewport();
		if(SCRIPT_VERIFY(gameViewport, "CLEAR_ROOM_FOR_GAME_VIEWPORT - The game viewport does not exist"))
		{
			int count = RENDERPHASEMGR.GetRenderPhaseCount();
			for(s32 i=0; i<count; i++)
			{
				CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(i);

				if (renderPhase.IsScanRenderPhase())
				{
					CRenderPhaseScanned &scanPhase = (CRenderPhaseScanned &) renderPhase;

					if (scanPhase.IsUpdatePortalVisTrackerPhase())
					{
						CPortalTracker* portalTracker = scanPhase.GetPortalVisTracker();
						if(SCRIPT_VERIFY(portalTracker, "CLEAR_ROOM_FOR_GAME_VIEWPORT - The game viewport does not have a portal tracker"))
						{
							if(portalTracker && portalTracker->GetInteriorInst())
							{
								portalTracker->RemoveFromInterior();
								portalTracker->Teleport();			//Prevent a rescan overwriting newly set room.
							}
						}
					}
				}
			}
		}
	}

	int CommandGetInteriorFromPrimaryView(void)
	{
		int ReturnInteriorProxyIndex = NULL_IN_SCRIPTING_LANGUAGE;

		CInteriorProxy* pIntProxy = CPortalVisTracker::GetPrimaryInteriorProxy();
		if (pIntProxy){
			ReturnInteriorProxyIndex = CInteriorProxy::GetPool()->GetIndex(pIntProxy);
		}

		return ReturnInteriorProxyIndex;
	}

	int GetProxyAtCoords(const scrVector & scrVecInCoors, int typeKey)
	{
		int ReturnInteriorProxyIndex = NULL_IN_SCRIPTING_LANGUAGE;
		Vector3 vecPositionToCheck(scrVecInCoors);
		const CInteriorProxy* pClosest = NULL;
		float closestDist2 = FLT_MAX;
		Vec3V vPositionToCheck = RCC_VEC3V(vecPositionToCheck);
		

		const spdSphere sphere(vPositionToCheck, ScalarV(V_FLT_SMALL_1));
		const CInteriorProxy::Pool* pPool = CInteriorProxy::GetPool();
		const CInteriorProxy* pIntProxy = NULL;
		s32 poolIndex = 0;
		s32 poolSize = pPool->GetSize();

		while (poolIndex < poolSize)
		{
			pIntProxy = pPool->GetSlot(poolIndex++);
			if (!pIntProxy)
				continue;

			//check hash first
			if (typeKey != 0 && pIntProxy->GetNameHash() != (u32)typeKey)
				continue;

			if (!pIntProxy->IsContainingImapActive())
				continue;

			//check if point is in sphere bounds of interior
			spdSphere proxySphere;
			pIntProxy->GetBoundSphere(proxySphere);
			if (!sphere.IntersectsSphere(proxySphere))
				continue;

			//check if this interior is closest than our previously closest found
			Vec3V proxyPosition;
			pIntProxy->GetPosition(proxyPosition);
			float dist2 = DistSquared(proxyPosition, vPositionToCheck).Getf();

			if (dist2 < closestDist2)
			{
				pClosest = pIntProxy;
				closestDist2 = dist2;
			}
		}

		if (pClosest){
			ReturnInteriorProxyIndex = CInteriorProxy::GetPool()->GetIndex(pClosest);
		}
		return ReturnInteriorProxyIndex;
	}

	int CommandGetInteriorAtCoords(const scrVector & scrVecInCoors)
	{
		Vector3 vecPositionToCheck(scrVecInCoors);
		int ReturnProxyIndex = GetProxyAtCoords(vecPositionToCheck, 0);
		return ReturnProxyIndex;
	}

	void CommandClearRoomForEntity(int EntityIndex)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);

		if(pEntity)
		{	
			// in an interior?
			CPortalTracker* pPT = pEntity->GetPortalTracker();
			CInteriorInst* pIntInst = pPT->GetInteriorInst();
			if (pEntity->m_nFlags.bInMloRoom && pPT && pIntInst && pPT->m_roomIdx != 0) 
			{
				CGameWorld::Remove( pEntity, true );
				pPT->Teleport();
				CGameWorld::Add( pEntity , CGameWorld::OUTSIDE, true );
				scriptAssertf(pEntity->m_nFlags.bInMloRoom == false, "%s:CLEAR_ROOM_FOR_ENTITY - Entity is still in a room interior", CTheScripts::GetCurrentScriptNameAndProgramCounter());
			}
		}
	}

	// check if interior is in a state ready for use by other script commands
	bool CommandIsInteriorReady(int InteriorProxyIndex)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "GET_IS_INTERIOR_READY - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "GET_IS_INTERIOR_READY - Interior proxy doesn't exist"))
			{
// #if !__NO_OUTPUT
// 				Displayf("***");
// 				Displayf("Is interior<%s> (slot idx: %d) ready?", pIntProxy->GetModelName(), pIntProxy->GetMapDataSlotIndex().Get());
// 				Displayf("States - Current:%d Requested by:%d", pIntProxy->GetCurrentState(), pIntProxy->GetRequestingModule());
// 				Displayf("Required by loadscene: %d", pIntProxy->IsRequiredByLoadScene());
// 				Displayf("Requested states: [%d][%d][%d][%d][%d][%d][%d][%d]", pIntProxy->GetRequestedState(0), pIntProxy->GetRequestedState(1), pIntProxy->GetRequestedState(2), pIntProxy->GetRequestedState(3),
// 					pIntProxy->GetRequestedState(4),pIntProxy->GetRequestedState(5),pIntProxy->GetRequestedState(6),pIntProxy->GetRequestedState(7));
// 				Displayf("***");
// #endif //!__NO_OUTPUT
				if (pIntProxy->GetCurrentState() >= CInteriorProxy::PS_FULL_WITH_COLLISIONS)
				{
					return(true);
				}
			}
		}
		return(false);
	}
	void CommandForceRoomForEntity(int EntityIndex, int InteriorProxyIndex, int RoomKey)
	{
		if(SCRIPT_VERIFY(CommandIsInteriorReady(InteriorProxyIndex),"FORCE_ROOM_FOR_ENTITY - Interior isn't ready, Ensure that it is ready with IS_INTERIOR_READY"))
		{
			CPhysical	*pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

			if ( pEntity )
			{
				CInteriorProxy*		pInteriorProxy = CInteriorProxy::GetPool()->GetAt( InteriorProxyIndex );
				if ( SCRIPT_VERIFY( pInteriorProxy, "FORCE_ROOM_FOR_ENTITY - Interior proxy doesn't exist" ) )
				{
					CInteriorInst* pInterior = pInteriorProxy->GetInteriorInst();
					if ( SCRIPT_VERIFY( pInterior, "FORCE_ROOM_FOR_ENTITY - Interior doesn't exist" ) )
					{
						const s32					roomIdx = pInterior->FindRoomByHashcode( RoomKey );
						const fwInteriorLocation	intLoc = CInteriorInst::CreateLocation( pInterior, roomIdx );

						CGameWorld::Remove( pEntity, true );
						pEntity->GetPortalTracker()->Teleport();
						CGameWorld::Add( pEntity, intLoc, true );
						scriptAssertf( pEntity->GetInteriorLocation().IsSameLocation( intLoc ), "%s:FORCE_ROOM_FOR_ENTITY - Entity still isn't in the correct room", CTheScripts::GetCurrentScriptNameAndProgramCounter() );

						pEntity->GetPortalTracker()->EnableProbeFallback( intLoc );

#if GTA_REPLAY
						if(CReplayMgr::ShouldRecord())
						{
							CReplayMgr::RecordPersistantFx<CPacketForceRoomForEntity>(CPacketForceRoomForEntity(pInteriorProxy, RoomKey), CTrackedEventInfo<int>(EntityIndex), pEntity, true);
						}
#endif

					}
				}
			}
		}
	}

	void CommandForceRoomForGameViewport(int InteriorProxyIndex, int RoomKey)
	{
		CInteriorProxy*	pInteriorProxy = CInteriorProxy::GetPool()->GetAt( InteriorProxyIndex );
		if ( SCRIPT_VERIFY( pInteriorProxy, "FORCE_ROOM_FOR_GAME_VIEWPORT - Interior proxy doesn't exist" ) )
		{
			CInteriorInst* pInterior = pInteriorProxy->GetInteriorInst();
			if ( SCRIPT_VERIFY( pInterior, "FORCE_ROOM_FOR_GAME_VIEWPORT - Interior doesn't exist" ) )
			{
				const s32					roomIdx = pInterior->FindRoomByHashcode( RoomKey );
				const fwInteriorLocation	intLoc = CInteriorInst::CreateLocation( pInterior, roomIdx );

				CViewport* gameViewport = gVpMan.GetGameViewport();
				if(SCRIPT_VERIFY(gameViewport, "FORCE_ROOM_FOR_GAME_VIEWPORT - The game viewport does not exist"))
				{
					//Get the portal tracker from the viewport and make sure it is in an interior.
					int count = RENDERPHASEMGR.GetRenderPhaseCount();
					for(s32 i=0; i<count; i++)
					{
						CRenderPhase &renderPhase = (CRenderPhase &) RENDERPHASEMGR.GetRenderPhase(i);

						if (renderPhase.IsScanRenderPhase())
						{
							CRenderPhaseScanned &scanPhase = (CRenderPhaseScanned &) renderPhase;

							if (scanPhase.IsUpdatePortalVisTrackerPhase())
							{
								CPortalTracker* portalTracker = scanPhase.GetPortalVisTracker();
								if(SCRIPT_VERIFY(portalTracker, "FORCE_ROOM_FOR_GAME_VIEWPORT - The game viewport does not have a portal tracker"))
								{
									Vector3 viewportPosition = VEC3V_TO_VECTOR3(gameViewport->GetGrcViewport().GetCameraPosition());

									Assert( portalTracker->m_pOwner == NULL );
									portalTracker->MoveToNewLocation( intLoc );
									portalTracker->EnableProbeFallback( intLoc );
									portalTracker->Teleport();
									portalTracker->Update(viewportPosition);
									CInteriorInst* pIntInst = portalTracker->GetInteriorInst();
									if(pIntInst) {
										pIntInst->ForceFadeIn();
									}
#if GTA_REPLAY
									if(CReplayMgr::ShouldRecord())
									{
										CReplayMgr::RecordFx<CPacketForceRoomForGameViewport>(CPacketForceRoomForGameViewport(pInteriorProxy, RoomKey));
									}
#endif
								}
							}
						}
					}
				}
			}
		}
	}

	int CommandGetKeyForEntityInRoom(int EntityIndex)
	{
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);
		if(EntityIndex)
		{	
			CPortalTracker* pPT = const_cast<CPortalTracker*>(pEntity->GetPortalTracker());
			CInteriorInst* pIntInst = pPT->GetInteriorInst();
			if ((!pIntInst) || (pPT->m_roomIdx == 0)){
				return 0 ;
			}

			return pPT->GetCurrRoomHash();
		}
		return 0;
	}

	int CommandGetInteriorFromEntity(int EntityIndex)
	{
		int ReturnInteriorProxyIndex = NULL_IN_SCRIPTING_LANGUAGE;

		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex, CTheScripts::GUID_ASSERT_FLAGS_NO_DEAD_CHECK);
		if (pEntity)
		{
			if (pEntity->GetIsRetainedByInteriorProxy()){
				CInteriorProxy *pInteriorProxy = CInteriorProxy::GetPool()->GetSlot(pEntity->GetRetainingInteriorProxy());
				if (scriptVerifyf(pInteriorProxy, "GET_INTERIOR_FROM_ENTITY - failed to get a pointer to the retaining interior proxy"))
				{
					ReturnInteriorProxyIndex = CInteriorProxy::GetPool()->GetIndex(pInteriorProxy);
				}
			} else 
			{
				fwInteriorLocation loc = pEntity->GetInteriorLocation();
				if (loc.IsValid()){
					CInteriorProxy* pIntProxy = CInteriorProxy::GetFromLocation(loc);
					if (pIntProxy){
						ReturnInteriorProxyIndex = CInteriorProxy::GetPool()->GetIndex(pIntProxy);
					}
				}
			}
		}
		return ReturnInteriorProxyIndex;
	}

	void CommandRetainEntityInInterior(int EntityIndex, int InteriorProxyIndex)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if(pEntity)
		{
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);

			// get the interior 
			if (SCRIPT_VERIFY(pIntProxy, "RETAIN_ENTITY_IN_INTERIOR - Interior proxy doesn't exist"))
			{
				pIntProxy->MoveToRetainList(pEntity);
			}
		}
	}

	void CommandClearInteriorStateOfEntity(int EntityIndex)
{
	CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
	if(pEntity)
	{
		CPortalTracker::ClearInteriorStateForEntity(pEntity);
	}
}

	void CommandForceActivatingTracker(int EntityIndex, bool bValue)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if(pEntity)
		{
			CPortalTracker* pPT = pEntity->GetPortalTracker();
			pPT->SetIsForcedToBeActivating(bValue);
		}
	}

	int  CommandGetRoomKeyFromEntity(int EntityIndex)
	{
		int key = 0;
		const CPhysical *pEntity = CTheScripts::GetEntityToQueryFromGUID<CPhysical>(EntityIndex);

		if(pEntity)
		{
			fwInteriorLocation loc = pEntity->GetInteriorLocation();
			if (loc.IsValid()){
				CInteriorProxy* pIntProxy = CInteriorProxy::GetFromLocation( loc );
				if (SCRIPT_VERIFY(pIntProxy, "GET_ROOM_KEY_FROM_ENTITY - Interior proxy doesn't exist"))
				{
					s32 roomId = loc.GetRoomIndex();
					CInteriorInst* pIntInst = pIntProxy->GetInteriorInst();
					if (SCRIPT_VERIFY(pIntInst, "GET_ROOM_KEY_FROM_ENTITY - Interior Instance doesn't exist"))
					{
						key = pIntInst->GetRoomHashcode(roomId);
					}
				}
			}
		}
		return key;
	}

	void CommandAddPickupToInteriorRoomByName(int PickupID, const char* pName)
	{
		if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
		{
			CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID));

			if (SCRIPT_VERIFY(pPlacement, "ADD_PICKUP_TO_INTERIOR_ROOM_BY_NAME - Pickup doesn't exist"))
			{
				pPlacement->SetRoomHash(atStringHash(pName));
			}
		}
	}


	int CommandGetRoomKeyFromPickup(int PickupID)
	{
		int key = 0;

		if (scriptVerify(CTheScripts::GetCurrentGtaScriptHandler()))
		{
			CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(CTheScripts::GetCurrentGtaScriptHandler()->GetScriptObject(PickupID));

			if (SCRIPT_VERIFY(pPlacement, "GET_ROOM_KEY_FROM_PICKUP - Pickup doesn't exist"))
			{
				key = pPlacement->GetRoomHash();
			}
		}

		return key;
	}

// ---- commands to modify  / interrogate new interior state

	// inform interior management code that given interior is going to be required by script
	void CommandPinInteriorInMemory(int InteriorProxyIndex)
	{
		if (SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "PIN_INTERIOR_IN_MEMORY - Interior index isn't valid"))
		{
			CScriptResource_MLO mlo(InteriorProxyIndex);
			CTheScripts::GetCurrentGtaScriptHandler()->RegisterScriptResource(mlo);

		//	Displayf("### Pin : %s", CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex)->GetModelName());
		}
	}

	// infrom interior management code that interior is no longer required by script
	void CommandUnpinInterior(int InteriorProxyIndex)
	{
		if (SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "UNPIN_INTERIOR_IN_MEMORY - Interior index isn't valid"))
		{
			CTheScripts::GetCurrentGtaScriptHandler()->RemoveScriptResource(CGameScriptResource::SCRIPT_RESOURCE_MLO, InteriorProxyIndex);

	//		Displayf("### Unpin : %s", CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex)->GetModelName());
		}
	}

	void CommandSetInteriorInUse(int InteriorProxyIndex)
	{
		if (SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "SET_INTERIOR_IN_USE - Interior index isn't valid"))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "SET_INTERIOR_IN_USE - Interior proxy doesn't exist"))
			{
                if (pIntProxy->GetInteriorInst())
                    pIntProxy->GetInteriorInst()->m_bInUse = true;
			}
		}
	}

	int CommandGetInteriorAtCoordsOfType(const scrVector & scrVecInCoors, const char* typeName)
	{
		int ReturnProxyIndex = NULL_IN_SCRIPTING_LANGUAGE;
		Vector3 vecPositionToCheck(scrVecInCoors);

		int typeKey = 0;
		if (typeName != NULL){
			typeKey = atStringHash(typeName);
		}

		ReturnProxyIndex = GetProxyAtCoords(vecPositionToCheck, typeKey);

		return ReturnProxyIndex;
	}

	int CommandGetInteriorAtCoordsOfTypeHash(const scrVector & scrVecInCoors, int typeHash)
	{
		int ReturnProxyIndex = NULL_IN_SCRIPTING_LANGUAGE;
		Vector3 vecPositionToCheck(scrVecInCoors);
		ReturnProxyIndex = GetProxyAtCoords(vecPositionToCheck, typeHash);

		return ReturnProxyIndex;
	}

	void CommandActivateInteriorGroupsUsingCamera(void)
	{
		CPortal::ActivateInteriorGroupsUsingCamera();
	}

// --- probing world for interior data

	bool IsCollisionMarkedAsOutside(const scrVector & scrVecInCoors)
	{
		s32 dummyRoomIdx;
		CInteriorInst* pIntInst = NULL;
		Vector3 vecPositionToCheck(scrVecInCoors);
		CPortalTracker::ProbeForInterior(vecPositionToCheck, pIntInst, dummyRoomIdx, NULL, CPortalTracker::SHORT_PROBE_DIST);
		return(pIntInst == NULL);
	}

	int GetInteriorIDFromCollision(const scrVector & scrVecInCoors)
	{
		int ReturnInteriorProxyIndex = NULL_IN_SCRIPTING_LANGUAGE;

		s32 dummyRoomIdx;
		CInteriorInst* pIntInst = NULL;
		Vector3 vecPositionToCheck(scrVecInCoors);
		CPortalTracker::ProbeForInterior(vecPositionToCheck, pIntInst, dummyRoomIdx, NULL, CPortalTracker::SHORT_PROBE_DIST);

		if (pIntInst != NULL){
			CInteriorProxy* pIntProxy = pIntInst->GetProxy();
			if (SCRIPT_VERIFY( pIntProxy, "GET_INTERIOR_ID_FROM_COLLISION - interior proxy isn't valid"))
			{
				ReturnInteriorProxyIndex = CInteriorProxy::GetPool()->GetIndex(pIntProxy);
			}
		}

		return(ReturnInteriorProxyIndex);
	}

	void EnableStadiumProbesThisFrame(bool bEnable)
	{
		scriptDebugf1("EnableStadiumProbesThisFrame - Params: %s", bEnable?"true":"false");

		CPortalTracker::EnableStadiumProbesThisFrame(bEnable);
		g_decalMan.ApplyWheelTrailHackForArenaMode(bEnable);
	}

// --- entity set functions

	void ActivateInteriorEntitySet(int InteriorProxyIndex, const char* entitySetName)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "ACTIVATE_INTERIOR_ENTITY_SET - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "ACTIVATE_INTERIOR_ENTITY_SET - Interior proxy doesn't exist"))
			{
				pIntProxy->ActivateEntitySet(atHashString(entitySetName));
			}
		}
	}

	void DeactivateInteriorEntitySet(int InteriorProxyIndex, const char*  entitySetName)	
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "DEACTIVATE_INTERIOR_ENTITY_SET - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "DEACTIVATE_INTERIOR_ENTITY_SET - Interior proxy doesn't exist"))
			{
				pIntProxy->DeactivateEntitySet(atHashString(entitySetName));
			}
		}
	}

	bool IsInteriorEntitySetActive(int InteriorProxyIndex, const char*  entitySetName)	
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "IS_INTERIOR_ENTITY_SET_ACTIVE - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "IS_INTERIOR_ENTITY_SET_ACTIVE - Interior proxy doesn't exist"))
			{
				return(pIntProxy->IsEntitySetActive(atHashString(entitySetName)));
			}
		}
		return(false);
	}

	void SetInteriorEntitySetTintIndex(int InteriorProxyIndex, const char*  entitySetName, int tintIdx)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "SET_INTERIOR_ENTITY_SET_TINT_INDEX - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "SET_INTERIOR_ENTITY_SET_TINT_INDEX - Interior proxy doesn't exist"))
			{
				pIntProxy->SetEntitySetTintIndex(atHashString(entitySetName), (UInt32)tintIdx);
			}
		}
	}

	void RefreshInterior(int InteriorProxyIndex)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "REFRESH_INTERIOR - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "REFRESH_INTERIOR - Interior proxy doesn't exist"))
			{
				CInteriorInst* pPrimaryIntInst = CPortalVisTracker::GetPrimaryInteriorInst();
#if GTA_REPLAY
				// During replay loading we can't rely on the 'camInterface::IsFadedOut()' being correct.
				bool replayLoadingScreenActive = CReplayCoordinator::IsExportingToVideoFile() ? CReplayCoordinator::ShouldShowLoadingScreen() : (CVideoEditorPlayback::IsActive() && CVideoEditorPlayback::IsLoading());
#endif // GTA_REPLAY
				if(pPrimaryIntInst && (pPrimaryIntInst->GetProxy() == pIntProxy) && !camInterface::IsFadedOut() && !CPauseMenu::GetPauseRenderPhasesStatus() REPLAY_ONLY(&& !replayLoadingScreenActive))
				{
					SCRIPT_ASSERTF(false, "REFRESH_INTERIOR - refresh called on interior the camera is in (%s). This will cause a scene drop out.",pPrimaryIntInst->GetModelName());
				}

				pIntProxy->RefreshInterior();
			}
		}
	}

	void CommandEnableExteriorCullSphereThisFrame(const scrVector & vPos, float fRadius)
	{
		g_PostScanCull.EnableSphereThisFrame( spdSphere(Vec3V(vPos), ScalarV(fRadius)) );
	}

	void CommandEnableExteriorCullModelThisFrame(s32 modelNameHash)
	{
		g_PostScanCull.EnableModelCullThisFrame((u32) modelNameHash);

#if GTA_REPLAY
		if(CReplayMgr::IsRecording())
		{
			CPacketDisabledThisFrame::RegisterModelCullThisFrame((u32) modelNameHash);
		}
#endif

	}

	void CommandEnableShadowCullModelThisFrame(s32 modelNameHash)
	{
		g_PostScanCull.EnableModelShadowCullThisFrame((u32) modelNameHash);

#if GTA_REPLAY
		if(CReplayMgr::IsRecording())
		{
			CPacketDisabledThisFrame::RegisterModelShadowCullThisFrame((u32) modelNameHash);
		}
#endif

	}

	// high level state control
	void CommandDisableInterior(int InteriorProxyIndex, bool val)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "DISABLE_INTERIOR - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "DISABLE_INTERIOR - Interior proxy doesn't exist"))
			{
				CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
				if(pPlayer && pPlayer->GetIsInInterior() && ((s32)pIntProxy->GetInteriorProxyPoolIdx() == pPlayer->GetInteriorLocation().GetInteriorProxyIndex()))
				{
					if (val == true)
					{
						SCRIPT_ASSERTF(false, "DISABLE_INTERIOR - called on interior the player is in (%s). This will cause a scene drop out.",pIntProxy->GetName().GetCStr());
					}
				}
				else
				{
					pIntProxy->SetIsDisabled(val);
				}
			}
		}
	}

	bool CommandIsInteriorDisabled(int InteriorProxyIndex)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "IS_INTERIOR_DISABLED - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "IS_INTERIOR_DISABLED - Interior proxy doesn't exist"))
			{
				return(pIntProxy->GetIsDisabled());
			}
		}
		return(false);
	}

	void CommandCapInterior(int InteriorProxyIndex, bool val)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "CAP_INTERIOR - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "CAP_INTERIOR - Interior proxy doesn't exist"))
			{
				CPed* pPlayer = CPedFactory::GetFactory()->GetLocalPlayer();
				if(pPlayer && pPlayer->GetIsInInterior() && ((s32)pIntProxy->GetInteriorProxyPoolIdx() == pPlayer->GetInteriorLocation().GetInteriorProxyIndex()))
				{
					if (val == true)
					{
						SCRIPT_ASSERTF(false, "CAP_INTERIOR - called on interior the player is in (%s). This will cause a scene drop out.",pIntProxy->GetName().GetCStr());
					}
				}
				else
				{
					pIntProxy->SetIsCappedAtPartial(val);
				}
			}
		}
	}

	bool CommandIsInteriorCapped(int InteriorProxyIndex)
	{
		if ( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "IS_INTERIOR_CAPPED - Interior index isn't valid" ))
		{
			// get the interior 
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "IS_INTERIOR_CAPPED - Interior proxy doesn't exist"))
			{
				return(pIntProxy->GetIsCappedAtPartial());
			}
		}
		return(false);
	}

	// high level state control
	void CommandDisableMetroSystem(bool val)
	{
		CInteriorProxy::DisableMetro(val);
	}


	static CCustomShaderEffectInterior* GetCseInteriorFromEntity(CEntity *pEntity)
	{
		fwCustomShaderEffect *fwCSE = pEntity->GetDrawHandler().GetShaderEffect();
		if(fwCSE && fwCSE->GetType()==CSE_INTERIOR)
		{
			return static_cast<CCustomShaderEffectInterior*>(fwCSE);
		}

		return NULL;
	}

	bool CommandSetDlcInteriorTexture(int InteriorProxyIndex, const char *pTextureDictionaryName, const char *pTextureName)
	{
		if( SCRIPT_VERIFY( CommandIsValidInterior(InteriorProxyIndex), "SET_DLC_INTERIOR_TEXTURE - Interior index isn't valid" ))
		{
			CInteriorProxy *pIntProxy = CInteriorProxy::GetPool()->GetAt(InteriorProxyIndex);
			if (SCRIPT_VERIFY(pIntProxy, "SET_DLC_INTERIOR_TEXTURE - Interior proxy doesn't exist"))
			{
				CInteriorInst* pInteriorInst = pIntProxy->GetInteriorInst();
				if (SCRIPT_VERIFY(pInteriorInst, "SET_DLC_INTERIOR_TEXTURE - Interior inst not valid"))
				{
					CEntity *pEntity = (CEntity*)pInteriorInst;
					if (pEntity)
					{
						CCustomShaderEffectInterior *pIntCSE = GetCseInteriorFromEntity(pEntity);
						if(pIntCSE)
						{
							grcTexture* newTexture = graphics_commands::GetTextureFromStreamedTxd(pTextureDictionaryName, pTextureName);
							pIntCSE->SetDiffuseTexture(newTexture);
							return true;
						}
					}
				}
			}
		}

		return false;
	}

	void CommandSetIsExteriorOnly(int EntityIndex, bool bIsExteriorOnly)
	{
		CPhysical *pEntity = CTheScripts::GetEntityToModifyFromGUID<CPhysical>(EntityIndex);
		if(SCRIPT_VERIFY(pEntity, "SET_IS_EXTERIOR_ONLY - invalid entity"))
		{	
			CPortalTracker* pPT = pEntity->GetPortalTracker();
			if (SCRIPT_VERIFY(pPT, "SET_IS_EXTERIOR_ONLY - entity has no portal tracker"))
			{
				pPT->SetIsExteriorOnly(bIsExteriorOnly);
			}
		}
	}


	void SetupScriptCommands()
	{
		// layout 
		SCR_REGISTER_UNUSED(LAYOUT_LOAD_INTERIOR_INFO,0xe61ef85fdce3fd95, CommandLayoutLoadInteriorInfo);
		SCR_REGISTER_UNUSED(LAYOUT_UNLOAD_INTERIOR_INFO,0xc9195ffe0c69c64e, CommandLayoutUnloadInteriorInfo);

		SCR_REGISTER_UNUSED(GET_ROOM_HANDLE_FROM_INTERIOR,0xc03e564395653366, CommandGetRoomHandleFromInterior);
		SCR_REGISTER_UNUSED(GET_LAYOUT_HANDLE_FROM_ROOM,0xbd5de9fff59992cc, CommandGetLayoutHandleFromRoom);
		SCR_REGISTER_UNUSED(GET_GROUP_HANDLE_FROM_LAYOUT,0x61b049e16b26a42f, CommandGetGroupHandleFromLayout);
		SCR_REGISTER_UNUSED(GET_RSREF_HANDLE_FROM_GROUP,0xa1bd9a2db65affc9, CommandGetRsRefHandleFromGroup);
		SCR_REGISTER_UNUSED(GET_LAYOUT_HANDLE_FROM_RSREF,0x818176db352c7bf7, CommandGetLayoutHandleFromRsRef);

		SCR_REGISTER_UNUSED(LAYOUT_GET_INTERIOR_INFO,0x9c6405b81de92961, CommandLayoutGetInteriorInfo);
		SCR_REGISTER_UNUSED(LAYOUT_GET_ROOM_INFO,0xb46862002175d6cf, CommandLayoutGetRoomInfo);
		SCR_REGISTER_UNUSED(LAYOUT_GET_LAYOUT_INFO,0xaec06141ff66d2bf, CommandLayoutGetLayoutInfo);
		SCR_REGISTER_UNUSED(LAYOUT_GET_GROUP_INFO,0x86764d41e90443d8, CommandLayoutGetGroupInfo);
		SCR_REGISTER_UNUSED(LAYOUT_GET_RSREF_INFO,0x1af10e1287826db5, CommandLayoutGetRsRefInfo);

		SCR_REGISTER_UNUSED(TOGGLE_GROUP_IN_LAYOUT,0x93d1793cedaf3ecc, CommandToggleGroupInLayout);
		SCR_REGISTER_UNUSED(IS_GROUP_ACTIVE_IN_LAYOUT,0x4b817cc5848dd8e3, CommandIsLayoutGroupActive);
		SCR_REGISTER_UNUSED(POPULATE_LAYOUT,0x748fd8e1c75c2f0b, CommandPopulateLayout);
		SCR_REGISTER_UNUSED(DEPOPULATE_LAYOUT,0x4ece4c828595026c, CommandDepopulateLayout);

		// misc
		SCR_REGISTER_SECURE(GET_INTERIOR_HEADING,0xa88bc56dfd999b0a, CommandGetInteriorHeading);
		SCR_REGISTER_SECURE(GET_INTERIOR_LOCATION_AND_NAMEHASH,0xd0434c3b2d654157, CommandGetInteriorLocAndNamehash);
		SCR_REGISTER_SECURE(GET_INTERIOR_GROUP_ID,0xa57f6a48f0eb95a0, CommandGetInteriorGroupID);
		SCR_REGISTER_SECURE(GET_OFFSET_FROM_INTERIOR_IN_WORLD_COORDS,0x56a14d0fd5a4e290, CommandGetOffsetFromInteriorInWorldCoords);
		SCR_REGISTER_SECURE(IS_INTERIOR_SCENE,0x479bccf712b8168e, CommandIsInteriorScene);
		SCR_REGISTER_SECURE(IS_VALID_INTERIOR,0xbcc73b466e2b2350, CommandIsValidInterior);

		// entity
		SCR_REGISTER_SECURE(CLEAR_ROOM_FOR_ENTITY,0x3c46be5cff0f7003, CommandClearRoomForEntity);
		SCR_REGISTER_SECURE(FORCE_ROOM_FOR_ENTITY,0x9ee5df347f5d97ca, CommandForceRoomForEntity);
		SCR_REGISTER_SECURE(GET_ROOM_KEY_FROM_ENTITY,0x782b98242b6bbb25, CommandGetRoomKeyFromEntity);
		SCR_REGISTER_SECURE(GET_KEY_FOR_ENTITY_IN_ROOM,0xfa8d15b7448eeca0, CommandGetKeyForEntityInRoom);
		SCR_REGISTER_SECURE(GET_INTERIOR_FROM_ENTITY,0x6d7ee245ad1e10b0, CommandGetInteriorFromEntity);
		SCR_REGISTER_SECURE(RETAIN_ENTITY_IN_INTERIOR,0xee4b783969c74ba1,	CommandRetainEntityInInterior);
		SCR_REGISTER_SECURE(CLEAR_INTERIOR_STATE_OF_ENTITY,0x63d8eb71d32dde73,	CommandClearInteriorStateOfEntity);

		SCR_REGISTER_SECURE(FORCE_ACTIVATING_TRACKING_ON_ENTITY,0xd7e367cfe1819ec5, CommandForceActivatingTracker);

		// camera related
		SCR_REGISTER_SECURE(FORCE_ROOM_FOR_GAME_VIEWPORT,0xd05075dd5280976b,	CommandForceRoomForGameViewport);
		SCR_REGISTER_SECURE(SET_ROOM_FOR_GAME_VIEWPORT_BY_NAME,0x8d3da8896d24c6ea,	SetRoomForGameViewportByName);
		SCR_REGISTER_SECURE(SET_ROOM_FOR_GAME_VIEWPORT_BY_KEY,0xf5b6e4d9edf2ad50,	SetRoomForGameViewportByKey);
		SCR_REGISTER_SECURE(GET_ROOM_KEY_FOR_GAME_VIEWPORT,0x7bad097af659c6b3,	GetRoomKeyForGameViewport);
		SCR_REGISTER_SECURE(CLEAR_ROOM_FOR_GAME_VIEWPORT,0x994f51a40d714799,	ClearRoomForGameViewport);

		SCR_REGISTER_SECURE(GET_INTERIOR_FROM_PRIMARY_VIEW,0xb04e49b85e6d01af, CommandGetInteriorFromPrimaryView);

		// from commands_objects.cpp
		SCR_REGISTER_SECURE(GET_INTERIOR_AT_COORDS,0xa0f62c1038208492,				CommandGetInteriorAtCoords);
		SCR_REGISTER_SECURE(ADD_PICKUP_TO_INTERIOR_ROOM_BY_NAME,0x28692df126dac03e,			CommandAddPickupToInteriorRoomByName);
		SCR_REGISTER_UNUSED(GET_ROOM_KEY_FROM_PICKUP,0xb4fe57b6f5564d99,				CommandGetRoomKeyFromPickup);

		// interior state setting / interrogating
		SCR_REGISTER_SECURE(PIN_INTERIOR_IN_MEMORY,0x62eb051f5ed6dd41,				CommandPinInteriorInMemory);
		SCR_REGISTER_SECURE(UNPIN_INTERIOR,0xaed5221f05dae55e,				CommandUnpinInterior);
		SCR_REGISTER_SECURE(IS_INTERIOR_READY,0xea02b859de237081,				CommandIsInteriorReady);
		SCR_REGISTER_SECURE(SET_INTERIOR_IN_USE,0x65f295aa405549a0,				CommandSetInteriorInUse);
		SCR_REGISTER_SECURE(GET_INTERIOR_AT_COORDS_WITH_TYPE,0x63ac7efb770fcb6f,				CommandGetInteriorAtCoordsOfType);
		SCR_REGISTER_SECURE(GET_INTERIOR_AT_COORDS_WITH_TYPEHASH,0x1d94209cb3495367,			CommandGetInteriorAtCoordsOfTypeHash);
		SCR_REGISTER_SECURE(ACTIVATE_INTERIOR_GROUPS_USING_CAMERA,0x8f010262eb7d058c,			CommandActivateInteriorGroupsUsingCamera);

		// probing world for interior data
		SCR_REGISTER_SECURE(IS_COLLISION_MARKED_OUTSIDE,0x15616e8442d3d1e8,			IsCollisionMarkedAsOutside);
		SCR_REGISTER_SECURE(GET_INTERIOR_FROM_COLLISION,0x6f7d2ff0780e66be,			GetInteriorIDFromCollision);
		SCR_REGISTER_SECURE(ENABLE_STADIUM_PROBES_THIS_FRAME,0xebdc7112e01946e8,	EnableStadiumProbesThisFrame);

		// entity set control
		SCR_REGISTER_SECURE(ACTIVATE_INTERIOR_ENTITY_SET,0x9f9fadbc78e66b6a,		ActivateInteriorEntitySet);
		SCR_REGISTER_SECURE(DEACTIVATE_INTERIOR_ENTITY_SET,0xd9b6dbdde360d161,		DeactivateInteriorEntitySet);
		SCR_REGISTER_SECURE(IS_INTERIOR_ENTITY_SET_ACTIVE,0x9b28deec684da500,		IsInteriorEntitySetActive);
		SCR_REGISTER_SECURE(SET_INTERIOR_ENTITY_SET_TINT_INDEX,0x915522919b63782f,	SetInteriorEntitySetTintIndex);
		SCR_REGISTER_SECURE(REFRESH_INTERIOR,0xd665fc4f45f67b23,					RefreshInterior);

		// exterior cull sphere - for MP apartment
		SCR_REGISTER_UNUSED(ENABLE_EXTERIOR_CULL_SPHERE_THIS_FRAME,0x9a8714e076868699, CommandEnableExteriorCullSphereThisFrame);
		SCR_REGISTER_SECURE(ENABLE_EXTERIOR_CULL_MODEL_THIS_FRAME,0xca2d19dba96cd177, CommandEnableExteriorCullModelThisFrame);
		SCR_REGISTER_SECURE(ENABLE_SHADOW_CULL_MODEL_THIS_FRAME,0xa1b02d03fd7abfdf, CommandEnableShadowCullModelThisFrame);

		// entity set control
		SCR_REGISTER_SECURE(DISABLE_INTERIOR,0x219e451df8882834,			CommandDisableInterior);
		SCR_REGISTER_SECURE(IS_INTERIOR_DISABLED,0xb45cdd8a96e17099,			CommandIsInteriorDisabled);
		SCR_REGISTER_SECURE(CAP_INTERIOR,0x1bca778ece172164,			CommandCapInterior);
		SCR_REGISTER_SECURE(IS_INTERIOR_CAPPED,0xa7e703fa558cb0a2,			CommandIsInteriorCapped);

		SCR_REGISTER_SECURE(DISABLE_METRO_SYSTEM,0x011ea64bbd3a849c,			CommandDisableMetroSystem);

		SCR_REGISTER_UNUSED(SET_DLC_INTERIOR_TEXTURE,0xef5dd455eb445169,		CommandSetDlcInteriorTexture);

		SCR_REGISTER_SECURE(SET_IS_EXTERIOR_ONLY,0x1a6aa12fc82e9a8f,			CommandSetIsExteriorOnly);
	}
}
