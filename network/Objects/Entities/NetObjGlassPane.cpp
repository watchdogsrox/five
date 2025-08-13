//
// name:		NetObjGlassPane.h
// description:	Derived from netObject, this class handles all glass pane state related network object
//				calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney

#include "network/Objects/entities/NetObjGlassPane.h"

// game headers
#include "network/network.h"
#include "network/Debug/NetworkDebug.h"
#include "network/Events/NetworkEventTypes.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/NetworkObjectTypes.h"
#include "Network/Players/NetGamePlayer.h"
#include "scene/world/GameWorld.h"
#include "peds/ped.h"
#include "objects/DummyObject.h"
#include "objects/object.h"
#include "fwscene/search/SearchVolumes.h"
#include "renderer/DrawLists/DrawListNY.h"

#include "glassPaneSyncing/GlassPaneInfo.h"
#include "glassPaneSyncing/GlassPaneManager.h"
#include "glassPaneSyncing/GlassPane.h"

NETWORK_OPTIMISATIONS()

#if GLASS_PANE_SYNCING_ACTIVE /* defined in GlassPaneInfo.h */

FW_INSTANTIATE_CLASS_POOL(CNetObjGlassPane, MAX_NUM_NETOBJGLASSPANE, atHashString("CNetObjGlassPane",0xcaf2c737));
FW_INSTANTIATE_CLASS_POOL(CNetObjGlassPane::CGlassPaneSyncData, MAX_NUM_NETOBJGLASSPANE, atHashString("CGlassPaneSyncData",0x5a057e05));

#endif /* GLASS_PANE_SYNCING_ACTIVE */

CGlassPaneSyncTree*						CNetObjGlassPane::ms_glassPaneSyncTree	= NULL;
CNetObjGlassPane::CGlassPaneScopeData	CNetObjGlassPane::ms_glassPaneScopeData;

// ================================================================================================================
// CNetObjGlassPane
// ================================================================================================================

CNetObjGlassPane::CNetObjGlassPane(CGlassPane					*glassPane,
                                   const NetworkObjectType		type,
                                   const ObjectId				objectID,
                                   const PhysicalPlayerIndex    playerIndex,
                                   const NetObjFlags			localFlags,
                                   const NetObjFlags			globalFlags)
:	CNetObjProximityMigrateable(glassPane, type, objectID, playerIndex, localFlags, globalFlags)
{
}

void CNetObjGlassPane::CreateSyncTree()
{
    ms_glassPaneSyncTree = rage_new CGlassPaneSyncTree;
}

void CNetObjGlassPane::DestroySyncTree()
{
	ms_glassPaneSyncTree->ShutdownTree();
    delete ms_glassPaneSyncTree;
    ms_glassPaneSyncTree = 0;
}

bool CNetObjGlassPane::Update()
{
#if !__FINAL && DEBUG_RENDER_GLASS_PANE_MANAGER

	if(GetGlassPane())
	{
		//grcDebugDraw::Sphere(GetGlassPane()->GetPosition(), 1.0f, IsClone() ? Color_yellow : Color_green, false);
	}

#endif /* !__FINAL && && DEBUG_RENDER_GLASS_PANE_MANAGER */

	// if we're the owner, we need to destroy the network object here as it's not tied
	// to when the geometry streams out as the network objects can function independantly.
	if(!IsClone())
	{
		CPed* localPlayer = CGameWorld::FindLocalPlayer();

		if(localPlayer && GetGlassPane() && !GetGlassPane()->GetGeometry())
		{
			Vector3 objPos		= GetGlassPane()->GetDummyPosition();
			Vector3 playerPos	= VEC3V_TO_VECTOR3(localPlayer->GetTransform().GetPosition());

			Vector3 diff		= objPos - playerPos;
			float distSq		= diff.XYMag2();

			float scopeDist		= GetScopeData().m_scopeDistance;
			float scopeDistSq   = scopeDist * scopeDist;

			if(distSq > scopeDistSq)
			{
				// try and migrate to another player...
				return ( ! CGlassPaneManager::MigrateGlassPane(GetGlassPane()) );
			}
		}
	}

	return CNetObjProximityMigrateable::Update();
}

void CNetObjGlassPane::LogAdditionalData(netLoggingInterface &UNUSED_PARAM(log)) const
{
    // Intentionally empty function to override LogAdditionalData for CNetObjProximityMigrateable,
    // which is not valid for a glass pane object
}

netINodeDataAccessor *CNetObjGlassPane::GetDataAccessor(u32 dataAccessorType)
{
	netINodeDataAccessor *dataAccessor = 0;

	if(dataAccessorType == IGlassPaneNodeDataAccessor::DATA_ACCESSOR_ID())
	{
		dataAccessor = (IGlassPaneNodeDataAccessor *)this;
	}
	else
	{
		dataAccessor = CNetObjProximityMigrateable::GetDataAccessor(dataAccessorType);
	}

	return dataAccessor;
}

bool	CNetObjGlassPane::IsInInterior() const
{
	if( GetGlassPane() )
	{ 
		return GetGlassPane()->GetIsInInterior();
	}

	return false;
}

Vector3 CNetObjGlassPane::GetPosition() const
{
	if(GetGlassPane())
	{
		return GetGlassPane()->GetDummyPosition();
	}

	gnetAssertf(false, "ERROR : CNetObjGlassPane::GetPosition : No glass pane connected with this net object?!");
	return VEC3_ZERO;
}

void	CNetObjGlassPane::SetPosition( const Vector3& UNUSED_PARAM(pos) )
{
	gnetAssertf(false, "ERROR : CNetObjGlassPane::SetPosition : Setting pos on inanimate object?!");
}

const scriptObjInfoBase* CNetObjGlassPane::GetScriptObjInfo() const
{
	return NULL;
}

void CNetObjGlassPane::SetScriptObjInfo(scriptObjInfoBase* UNUSED_PARAM(info))
{}

scriptHandlerObject* CNetObjGlassPane::GetScriptHandlerObject() const
{
	return NULL;
}

void CNetObjGlassPane::CleanUpGameObject(bool UNUSED_PARAM(bDestroyObject))
{
    CGlassPane* glassPane = GetGlassPane();
	VALIDATE_GAME_OBJECT(glassPane);

	if (!glassPane)
	{
		SetGameObject(0);
		return;
	}

	if(!CGlassPaneManager::DestroyGlassPane(glassPane))
	{
		gnetAssert(false);
	}

    glassPane->SetNetworkObject(NULL);
    SetGameObject(0);
}

void CNetObjGlassPane::GetGlassPaneCreateData(CGlassPaneCreationDataNode& data) const
{
	CGlassPane* glassPane = GetGlassPane();

	if(glassPane)
	{	
		CObject const* geometry = glassPane->GetGeometry();

		// The geometry may or may not be streamed in...
		if(geometry)
		{
			// if so, use the data from the object itself...
			gnetAssert(geometry->GetBaseModelInfo());

			// if it is, get the dummy object that spawned it (dummies don't move)...
			CDummyObject* dummy = const_cast<CObject*>(geometry)->GetRelatedDummy();
			gnetAssert(dummy);

			if(dummy)
			{
				data.m_position		= VEC3V_TO_VECTOR3(dummy->GetTransform().GetPosition());
			}
			else
			{
				data.m_position		= glassPane->GetDummyPosition();
			}

			data.m_radius			= geometry->GetBaseModelInfo()->GetBoundingSphereRadius();
			data.m_isInside			= geometry->GetIsInInterior();		
		}
		else
		{
			// if not, use the data that was passed in on creation (gets updated when geometry does stream in)...
			data.m_position			= glassPane->GetDummyPosition();
			data.m_radius			= glassPane->GetRadius();
			data.m_isInside			= glassPane->GetIsInInterior();
		}

		data.m_geometryObjHash		= CGlassPaneManager::GenerateHash(data.m_position);
		data.m_hitPositionOS		= glassPane->GetHitPositionOS();
		data.m_hitComponent			= glassPane->GetHitComponent();
	}
}

void CNetObjGlassPane::SetGlassPaneCreateData(const CGlassPaneCreationDataNode& data)
{
	CGlassPane* pane = CGlassPaneManager::AccessGlassPaneByGeometryHash(data.m_geometryObjHash);

	if(!pane)
	{
		// see if we can find the actual glass geometry object in the world.
		// it may be streamed in, it may not be streamed in...
		CObject const * obj = CGlassPaneManager::GetGeometryObject(data.m_geometryObjHash);

		// Set up the glass pane, either with the (dummy) geometry itself or if that's not streamed in, the data used to find it....
		if(obj)
		{
			CBaseModelInfo const* pModelInfo = obj->GetBaseModelInfo();										
			gnetAssert(pModelInfo);

			CDummyObject* dummy = const_cast<CObject*>(obj)->GetRelatedDummy();
			gnetAssert(dummy);

			if(dummy)
			{
				CGlassPaneManager::GlassPaneInitData initData(VEC3V_TO_VECTOR3(dummy->GetTransform().GetPosition()), data.m_geometryObjHash, pModelInfo->GetBoundingSphereRadius(), obj->GetIsInInterior(), data.m_hitPositionOS, data.m_hitComponent);
				pane = const_cast<CGlassPane*>(CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkClone(obj, initData));
			}
			else
			{
				CGlassPaneManager::GlassPaneInitData initData(data.m_position, data.m_geometryObjHash, pModelInfo->GetBoundingSphereRadius(), obj->GetIsInInterior(), data.m_hitPositionOS, data.m_hitComponent);
				pane = const_cast<CGlassPane*>(CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkClone(obj, initData));
			}

			pane->SetNetworkObject(this);
			SetGameObject((void*) pane);

			pane->ApplyDamage();
		}
		else
		{
			CGlassPaneManager::GlassPaneInitData initData(data.m_position, data.m_geometryObjHash, data.m_radius, data.m_isInside, data.m_hitPositionOS, data.m_hitComponent);
			pane = const_cast<CGlassPane*>(CGlassPaneManager::RegisterGlassGeometryObject_OnNetworkClone(obj, initData));		

			pane->SetNetworkObject(this);
			SetGameObject((void*) pane);
		}		
	}
	else
	{
		// this pane is being created multiple times?
		gnetAssertf(pane, "CNetObjGlassPane::SetGlassPaneCreateData : Pane is being created multiple times?");
	}
	
	gnetAssertf(pane, "CNetObjGlassPane::SetGlassPaneCreateData : Failed to find or create pane object?!");
}

bool CNetObjGlassPane::CanDelete(unsigned* LOGGING_ONLY(UNUSED_PARAM(reason)))
{
    // Objects can always be deleted at any point as we
    // don't have control of which objects are removed by the IPL system
    return true;
}

void CNetObjGlassPane::DisplayNetworkInfo()
{
#if __BANK

	const NetworkDebug::NetworkDisplaySettings &displaySettings = NetworkDebug::GetDebugDisplaySettings();

	if (!displaySettings.m_displayObjectInfo)
		return;

	if (!GetGlassPane())
		return;

	float   scale = 1.0f;
	Vector2 screenCoords;
	if(NetworkUtils::GetScreenCoordinatesForOHD(GetPosition(), screenCoords, scale))
	{
		const float DEFAULT_CHARACTER_HEIGHT = 0.065f; // normalised value, CText::GetDefaultCharacterHeight() returns coordinates relative to screen dimensions
		const float DEBUG_TEXT_Y_INCREMENT   = (DEFAULT_CHARACTER_HEIGHT * scale * 0.5f); // amount to adjust screen coordinates to move down a line

		screenCoords.y += DEBUG_TEXT_Y_INCREMENT*4; // move below display info for pickup object

		// draw the text over objects owned by a player in the default colour for the player
		// based on their ID. This is so we can distinguish between objects owned by different
		// players on the same team
		NetworkColours::NetworkColour colour = NetworkUtils::GetDebugColourForPlayer(GetPlayerOwner());
		Color32 playerColour = NetworkColours::GetNetworkColour(colour);

		char str[100];
		sprintf(str, "%s%s", GetLogName(), IsPendingOwnerChange() ? "*" : "");
		DLC ( CDrawNetworkDebugOHD_NY, (screenCoords, playerColour, scale, str));
	}
#endif // __BANK
}









