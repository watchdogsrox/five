// File header
#include "pickups/PickupManager.h"

// Rage headers
#include "physics/simulator.h"

// Framework headers
#include "fwnet/nettypes.h"
#include "fwscene/world/WorldLimits.h"
#include "fwutil/quadtree.h"
#include "fwscene/stores/drawablestore.h"

// Game headers
#include "audio/scriptaudioentity.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "Core/Game.h"
#include "Cutscene/CutSceneManagerNew.h"
#include "weapons/Weapon.h"
#include "event/EventGroup.h"
#include "event/EventNetwork.h"
#include "network/Network.h"
#include "network/Objects/Entities/NetObjGame.h"
#include "peds/PlayerInfo.h"
#include "pickups/Data/PickupDataManager.h"
#include "pickups/Data/scriptedglows_parser.h"
#include "pickups/Pickup.h"
#include "renderer/Lights/lights.h"
#include "scene/portals/InteriorInst.h"
#include "scene/world/GameWorld.h"
#include "script/Handlers/GameScriptEntity.h"
#include "script/commands_weapon.h"
#include "peds/PlayerInfo.h"
#include "peds/Ped.h"
#include "streaming/streamingengine.h"
#include "physics/gtaInst.h"
#include "physics/WorldProbe/shapetestcapsuledesc.h"
#include "Vfx/Misc/GameGlows.h"
#include "Weapons/WeaponFactory.h"

#if __BANK
#include "bank/bkmgr.h"
#include "scene/world/GameWorld.h"
#endif
WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

// Static variable initialisation
CPickupManager::PickupTypeInfoMap																		CPickupManager::ms_pickupTypeInfoMap;
atFixedArray<CPickupManager::CCustomArchetypeInfo, CPickupManager::MAX_EXTRA_CUSTOM_ARCHETYPES>			CPickupManager::ms_extraCustomArchetypes;
atQueue<CPickupManager::CCollectionInfo, CPickupManager::COLLECTION_HISTORY_SIZE>						CPickupManager::ms_collectionHistory;
fwQuadTreeNode*																							CPickupManager::ms_pQuadTree = NULL;
Vector3																									CPickupManager::ms_cachedPlayerPosition = Vector3(0.0f, 0.0f, 0.0f);
fwPtrListSingleLink																						CPickupManager::ms_pendingPickups;
fwPtrListSingleLink																						CPickupManager::ms_regeneratingPlacements;
fwPtrListSingleLink																						CPickupManager::ms_removedPlacements;
fwFlags32																								CPickupManager::ms_SuppressionFlags;
bool																									CPickupManager::ms_bShuttingDown = false;
ScriptedGlowList																						CPickupManager::ms_glowList;
u32																										CPickupManager::ms_prohibitedPickupModels[MAX_PROHIBITED_PICKUP_MODELS];
bool																									CPickupManager::ms_hasProhibitedPickupModels = false;
bool																									CPickupManager::ms_OnlyAllowAmmoCollectionWhenLowOnAmmo = false;
float																									CPickupManager::ms_fAmmoAmountScaler(1.0f);

FW_INSTANTIATE_BASECLASS_POOL(CPickupManager::CCollectionInfo, CPickupManager::COLLECTION_HISTORY_SIZE, atHashString("CCollectionInfo",0x857da83e), sizeof(CPickupManager::CCollectionInfo));
FW_INSTANTIATE_BASECLASS_POOL(CPickupManager::CRegenerationInfo, CONFIGURED_FROM_FILE, atHashString("CRegenerationInfo",0x24bead4e), sizeof(CPickupManager::CRegenerationInfo));

bool			CPickupManager::ms_AllowNetworkedMoneyPickup = false;
#if __BANK
bkBank*			CPickupManager::ms_pBank = NULL;
bkCombo*		CPickupManager::ms_pPickupNamesCombo = NULL;
int				CPickupManager::ms_pickupComboIndex = 0;
const char*		CPickupManager::ms_pickupComboNames[CPickupData::MAX_STORAGE];
int				CPickupManager::ms_numPickupComboNames = 0;
bool			CPickupManager::ms_fixedPickups = true;
bool			CPickupManager::ms_regeneratePickups = false;
bool			CPickupManager::ms_blipPickups = false;
bool			CPickupManager::ms_forceMPStyleWeaponPickups = false;
bool			CPickupManager::ms_createAllPickupsOnGround = false;
bool			CPickupManager::ms_createAllPickupsWithArrowMarkers = false;
bool			CPickupManager::ms_createAllPickupsUpright = false;
bool			CPickupManager::ms_allowPlayerToCarryPortablePickups = true;
bool			CPickupManager::ms_displayAllPickupPlacements = false;
bool			CPickupManager::ms_debugScriptedPickups = false;
bool			CPickupManager::ms_NoMoneyDrop = false;
#if ENABLE_GLOWS
bool			CPickupManager::ms_forcePickupGlow = false;
bool			CPickupManager::ms_forceOffsetPickupGlow = false;
#endif // ENABLE_GLOWS
#endif

// settings
dev_float ARROW_MARKER_Z_OFFSET = 1.0f;
dev_float ARROW_MARKER_SCALE = 0.5f;
#if __DEV
Color32 ARROW_MARKER_COL = Color32(0.05f, 0.45f, 0.05f, 0.5f);
#else
const Color32 ARROW_MARKER_COL = Color32(0.05f, 0.45f, 0.05f, 0.5f);
#endif
dev_float ARROW_MARKER_FADE_START_RANGE = 35.0f;
dev_float ARROW_MARKER_FADE_OUT_RANGE = 50.0f;
dev_bool ARROW_MARKER_BOUNCE = true;
dev_bool ARROW_MARKER_FACE_CAM = true;

bool CPickupManager::ms_forceRotatePickupFaceUp = false;

#define SCRIPTED_PICKUP_GLOW_FADE_NEAR 22.0f
#define SCRIPTED_PICKUP_GLOW_FADE_FAR 30.0f


void CPickupManager::GlowListLoad()
{
	if(ms_glowList.m_data.GetCount() == 0)
	{
		PARSER.LoadObject("common:/data/scriptedglow", "xml", ms_glowList, &parSettings::sm_StrictSettings);
	}
}

#if __BANK

void CPickupManager::GlowListSave()
{
	PARSER.SaveObject("common:/data/scriptedglow", "xml", &ms_glowList, parManager::XML);
}

void CPickupManager::GlowListAddWidgets(bkBank *bank)
{
	GlowListLoad();
	
	bank->PushGroup("Glows");
	
	bank->AddButton("Save",GlowListSave);
	
	for(int i=0;i<ms_glowList.m_data.GetCount();i++)
	{
		char title[128];
		sprintf(title,"Glow type %d",i);
		bank->PushGroup(title);
		ms_glowList.m_data[i].AddWidgets(bank);
		bank->PopGroup();
	}
	bank->PopGroup();
}
#endif // __BANK	

void CPickupManager::RenderGlow( Vec3V_In pos, float fadeNear, float fadeFar, Vector3 &col, float intensity, float range )
{
	Vec3V	camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
	float	distToCam = Mag(pos - camPos).Getf();

	if (distToCam < fadeFar && range > 0.0f)
	{
		// glow intensity scales depending on distance from the pickup to the camera
		intensity *= rage::Clamp((fadeNear - distToCam)/ (fadeFar - fadeNear),0.0f,1.0f);
		if( intensity > 0.0f)
		{
			GameGlows::Add(pos,range,col,intensity);
		}
	}
}

void CPickupManager::RenderScriptedGlow(Vec3V_In pos, int glowType)
{
#if !__FINAL
	Assertf(glowType < ms_glowList.m_data.GetCount(),"Trying to render a non existing scripted glow (%d/%d)",glowType,ms_glowList.m_data.GetCount());
	glowType = Clamp(glowType,0,ms_glowList.m_data.GetCount());
#endif
	ScriptedGlow &glow = ms_glowList.m_data[glowType];

	Vector3 col = Vector3(glow.color.GetRedf(),glow.color.GetGreenf(),glow.color.GetBluef());
	
	RenderGlow(pos, SCRIPTED_PICKUP_GLOW_FADE_NEAR, SCRIPTED_PICKUP_GLOW_FADE_FAR, col, glow.intensity, glow.range);
}

void CPickupManager::MovePortablePickupsFromOnePedToAnother(CPed& ped1, CPed& ped2)
{
	static const unsigned MAX_ATTACHED_PICKUPS = 20;

	CPickup* attachedPickups[MAX_ATTACHED_PICKUPS];
	u32 numAttachedPickups = 0;

	CEntity* pChild = static_cast<CEntity*>(ped1.GetChildAttachment());

	while (pChild)
	{
		if (pChild->GetIsTypeObject() && static_cast<CObject*>(pChild)->m_nObjectFlags.bIsPickUp)
		{
			if (AssertVerify(numAttachedPickups < MAX_ATTACHED_PICKUPS))
			{
				attachedPickups[numAttachedPickups++] = static_cast<CPickup*>(pChild);
			}
		}

		pChild = static_cast<CEntity*>(pChild->GetSiblingAttachment());
	}

	for (u32 i=0; i<numAttachedPickups; i++)
	{
		if (AssertVerify(attachedPickups[i]->IsFlagSet(CPickup::PF_Portable)))
		{
			attachedPickups[i]->DetachPortablePickupFromPed("Moved from one ped to another");
			attachedPickups[i]->AttachPortablePickupToPed(&ped2, "Moved from one ped to another");
		}
	}
}

void CPickupManager::DetachAllPortablePickupsFromPed(CPed& ped)
{
	static const unsigned MAX_ATTACHED_PICKUPS = 20;

	CPickup* attachedPickups[MAX_ATTACHED_PICKUPS];
	u32 numAttachedPickups = 0;

	CEntity* pChild = static_cast<CEntity*>(ped.GetChildAttachment());

	while (pChild)
	{
		if (pChild->GetIsTypeObject() && static_cast<CObject*>(pChild)->m_nObjectFlags.bIsPickUp)
		{
			if (AssertVerify(numAttachedPickups < MAX_ATTACHED_PICKUPS))
			{
				attachedPickups[numAttachedPickups++] = static_cast<CPickup*>(pChild);
			}
		}

		pChild = static_cast<CEntity*>(pChild->GetSiblingAttachment());
	}

	for (u32 i=0; i<numAttachedPickups; i++)
	{
		if (AssertVerify(attachedPickups[i]->IsFlagSet(CPickup::PF_Portable)))
		{
			if (!NetworkUtils::IsNetworkCloneOrMigrating(attachedPickups[i]))
			{
				attachedPickups[i]->DetachPortablePickupFromPed("DetachAllPortablePickupsFromPed(1)");
			}
			else
			{
				attachedPickups[i]->SetFlag(CPickup::PF_DetachWhenLocal);
			}
		}
	}
}

///////////////////////////////////////////////////////////////////////////////////////
// CPickupManager::CPickupPositionInfo
///////////////////////////////////////////////////////////////////////////////////////
CPickupManager::CPickupPositionInfo::CPickupPositionInfo( const Vector3 &srcPos ) 
: m_MinDist( 1.2f ), 
m_MaxDist( 1.5f ), 
m_SrcPos( srcPos ),
m_pickupHeightOffGround(0.0f),
m_bOnGround(false)
{

}

///////////////////////////////////////////////////////////////////////////////////////
// CPickupManager::CRegenerationInfo
///////////////////////////////////////////////////////////////////////////////////////

CPickupManager::CRegenerationInfo::CRegenerationInfo(CPickupPlacement* pPlacement)
: m_pPlacement(pPlacement), m_forceRegenerate(false)
{
	m_regenerationTime = NetworkInterface::GetSyncedTimeInMilliseconds() + pPlacement->GetRegenerationTime();
}

bool CPickupManager::CRegenerationInfo::IsReadyToRegenerate()
{
	if (m_forceRegenerate)
		return true;

	u32 currTime = NetworkInterface::GetSyncedTimeInMilliseconds();
	u32 signBit = 0x80000000u;

	if (currTime >= m_regenerationTime)
	{
		return true;
	}
	else if ((m_regenerationTime & signBit) & !(currTime & signBit)) // handles the time wrapping
	{
		return true;
	}

	return false;
}

///////////////////////////////////////////////////////////////////////////////////////
// CPickupManager::CCustomArchetypeInfo
///////////////////////////////////////////////////////////////////////////////////////

void CPickupManager::CCustomArchetypeInfo::Set(phArchetypeDamp& archetype, u32 modelIndex)
{
	Assert(!m_archetype);

	m_archetype	 = &archetype;
	m_modelIndex = modelIndex;
	m_refCount	 = 0;

	CBaseModelInfo* pMI = CModelInfo::GetModelInfoFromLocalIndex(modelIndex);

	if (pMI->GetHasLoaded() && pMI->GetHasBoundInDrawable())
	{
		pMI->AddRef();
		g_DrawableStore.AddRef(strLocalIndex(pMI->GetDrawableIndex()), REF_OTHER);
// 		char refString[16];
// 		g_DrawableStore.GetRefCountString(pMI->GetDrawableIndex(), refString, sizeof(refString));
// 		Displayf("Physics reference to %s raised to %s", pMI->GetModelName(), refString);
	}

	// add a ref here so that the archetype is kept until the pickup manager closes down
	m_archetype->AddRef();
}

void CPickupManager::CCustomArchetypeInfo::Clear()
{
	// If archetype exists release it and remove dictionary reference
	if (IsSet())
	{
		if (m_archetype)
		{
			m_archetype->Release();
			m_archetype = NULL;
		}

		CBaseModelInfo* pMI = CModelInfo::GetModelInfoFromLocalIndex(m_modelIndex.Get());
		if (pMI && pMI->GetHasLoaded() && pMI->GetHasBoundInDrawable())
		{
			pMI->RemoveRef();
			g_DrawableStore.RemoveRef(strLocalIndex(pMI->GetDrawableIndex()), REF_OTHER);
// 			char refString[16];
// 			g_DrawableStore.GetRefCountString(pMI->GetDrawableIndex(), refString, sizeof(refString));
// 			Displayf("Physics reference to %s dropped to %s", pMI->GetModelName(), refString);
		}

		m_modelIndex = fwModelId::MI_INVALID;
		m_refCount	 = 0;
	}
}

void CPickupManager::CCustomArchetypeInfo::RemoveRef()
{
	if (AssertVerify(IsSet()) && AssertVerify(m_refCount > 0))
	{
		m_refCount--;

		if (m_refCount == 0)
		{
			Clear();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////
// CPickupManager
///////////////////////////////////////////////////////////////////////////////////////
void CPickupManager::InitPools()
{
	CPickup::InitPool( MEMBUCKET_GAMEPLAY );
	CPickupPlacement::InitPool( MEMBUCKET_GAMEPLAY );
	CPickupPlacement::CPickupPlacementCustomScriptData::InitPool( MEMBUCKET_GAMEPLAY );
	CPickupManager::CCollectionInfo::InitPool( MEMBUCKET_GAMEPLAY );
	CPickupManager::CRegenerationInfo::InitPool( MEMBUCKET_GAMEPLAY );
}

void CPickupManager::ShutdownPools()
{
	CPickup::ShutdownPool();
	CPickupPlacement::ShutdownPool();
	CPickupPlacement::CPickupPlacementCustomScriptData::ShutdownPool();
	CPickupManager::CCollectionInfo::ShutdownPool();
	CPickupManager::CRegenerationInfo::ShutdownPool();
}

// Name			:	InitLevel
// Purpose		:	Initialises the manager when a level starts up.
void CPickupManager::Init(unsigned /*initMode*/)
{
	gnetAssert(ms_pickupTypeInfoMap.GetNumUsed() == 0);

	ms_extraCustomArchetypes.Resize(MAX_EXTRA_CUSTOM_ARCHETYPES);

	Assert(!ms_pQuadTree);

	fwRect worldBB(WORLDLIMITS_REP_XMIN, WORLDLIMITS_REP_YMIN, WORLDLIMITS_REP_XMAX, WORLDLIMITS_REP_YMAX);
	ms_pQuadTree = rage_new fwQuadTreeNode(worldBB, 4);

	ms_cachedPlayerPosition = Vector3(0.0f, 0.0f, 0.0f);
	
	// load scripted glow definitions
	GlowListLoad();

	for (u32 i=0; i<MAX_PROHIBITED_PICKUP_MODELS; i++)
	{
		ms_prohibitedPickupModels[i] = 0;
	}

	ms_hasProhibitedPickupModels = false;

	ms_AllowNetworkedMoneyPickup = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("ALLOW_NETWORKED_MONEY_PICKUPS", 0xAC79260F), false);
}

// Name			:	InitLevel
// Purpose		:	Initialises the manager when a level starts up.
void CPickupManager::InitDLCCommands()
{
	UpdatePickupTypes();
}

// Name			:	ShutdownLevel
// Purpose		:	Shuts down the manager when a level ends.
void CPickupManager::Shutdown(unsigned /*shutdownMode*/)
{
	Assert(!ms_bShuttingDown);

	ms_bShuttingDown = true;

	RemoveAllPickups(true);

	// destroy all the pickups now, they will not be cleaned up properly otherwise
	s32 i = CPickup::GetPool()->GetSize();

	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup)
		{
			CObjectPopulation::DestroyObject(pPickup);
		}
	}

	PickupTypeInfoMap::Iterator entry = ms_pickupTypeInfoMap.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		entry.GetData().m_customArchetype.Clear();
	}

	ms_pickupTypeInfoMap.Reset();

	for (u32 i=0; i<MAX_EXTRA_CUSTOM_ARCHETYPES; i++)
	{
		ms_extraCustomArchetypes[i].Clear();
	}

	ms_extraCustomArchetypes.Reset();
	ms_collectionHistory.Reset();

	Assert(ms_pendingPickups.CountElements() == 0);
	Assert(ms_regeneratingPlacements.CountElements() == 0);
	Assert(ms_removedPlacements.CountElements() == 0);
	
	ms_pendingPickups.Flush();
	ms_regeneratingPlacements.Flush();
	ms_removedPlacements.Flush();

	delete ms_pQuadTree;
	ms_pQuadTree = NULL;

	ms_bShuttingDown = false;
}

// Name			:	Update
// Purpose		:	Main processing. Called once a frame.
void CPickupManager::Update()
{
	if ((CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning()) 
			REPLAY_ONLY(|| CReplayMgr::IsEditModeActive()) )
	{
		ProcessInactiveState();
	}
	else
	{
		ProcessRemovedPlacements();
		ProcessPickupsInScope();
		ProcessPendingPlacements();
		ProcessRegeneratingPlacements();
		UpdatePickups();
	}
	
	ms_fAmmoAmountScaler = 1.0f;	// reset it every frame.

#if __BANK
	DebugScriptedGlow();
	DebugDisplay();
#endif // __BANK
	
}

void CPickupManager::SessionReset()
{
	ClearSuppressionFlags();

	PickupTypeInfoMap::Iterator entry = ms_pickupTypeInfoMap.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		entry.GetData().m_prohibitedCollections = 0;
	}

	for (u32 i=0; i<MAX_PROHIBITED_PICKUP_MODELS; i++)
	{
		ms_prohibitedPickupModels[i] = 0;
	}

	ms_OnlyAllowAmmoCollectionWhenLowOnAmmo = false;
}


void CPickupManager::UpdatePickupTypes()
{
	ms_pickupTypeInfoMap.Reset();

	for (u32 i=0; i<CPickupDataManager::GetNumPickupTypes(); i++)
	{
		CPickupData* pPickupData = CPickupDataManager::GetPickupDataInSlot(i);

		// make sure that the hash isn't conflicting with one of the old pickup types
		Assert(pPickupData->GetHash() >= NUM_PICKUP_TYPES);

		CPickupTypeInfo newInfo;
		ms_pickupTypeInfoMap.Insert(pPickupData->GetHash(), newInfo);

		if( pPickupData->GetDarkGlowIntensity() == -1.0f )
		{
			pPickupData->SetDarkGlowIntensity(pPickupData->GetGlowIntensity());
		}

		if( pPickupData->GetMPGlowIntensity() == -1.0f )
		{
			pPickupData->SetMPGlowIntensity(pPickupData->GetGlowIntensity());
		}

		if( pPickupData->GetMPDarkGlowIntensity() == -1.0f )
		{
			pPickupData->SetMPDarkGlowIntensity(pPickupData->GetDarkGlowIntensity());
		}

#if __BANK
		ms_pickupComboNames[i] = pPickupData->GetName();
#endif
	}

#if __BANK
	ms_numPickupComboNames = CPickupDataManager::GetNumPickupTypes();

	if (ms_pPickupNamesCombo && ms_numPickupComboNames > 0)
	{
		ms_pPickupNamesCombo->UpdateCombo ("Pickup", &ms_pickupComboIndex, ms_numPickupComboNames, ms_pickupComboNames);
	}
#endif
}

// Name			:	RegisterPickupPlacement
// Purpose		:	Pickup registration. Pickup placement data is added to the quad tree.
// Parameters	:	pickupHash - the hash of the pickup type
//					pickupPosition - the position of the pickup
//					pickupOrientation - the orientation of the pickup (in eulers)
//					flags - placement flags determining whether the pickup is fixed, regenerates or is blipped.
//					pNetObj - the network object to be assigned to the new placement (only used for network clones)
// Returns		:	the unique newly created pickup placement
CPickupPlacement* CPickupManager::RegisterPickupPlacement(u32 pickupHash, 
														  Vector3& pickupPosition,
														  Vector3& pickupOrientation,
														  PlacementFlags flags, 
														  CNetObjPickupPlacement* pNetObj,
														  u32 customModelIndex)
{
	const CPickupData* pPickupData = CPickupDataManager::GetPickupData(pickupHash);

	if (Verifyf(CPickupPlacement::GetPool()->GetNoOfFreeSpaces() > 0, "Can't register pickup - no spaces left in pool") &&
		Verifyf(pPickupData, "Can't register pickup - pickup metadata does not exist for this pickup type (%u)", pickupHash))
	{
#if __ASSERT
		// check to see that there are no other placements at this position
		int i = CPickupPlacement::GetPool()->GetSize();

		// test pickup placements
		while(i--)
		{
			CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

			if (pPlacement && !ms_removedPlacements.IsMemberOfList(pPlacement))
			{
				Vector3 placementPos = pPlacement->GetPickupPosition();
				Vector3 vDiff = placementPos - pickupPosition;

				if (vDiff.Mag2() < 0.01f)
				{
					Assertf(0, "A pickup of type %s is being created on top of another pickup of type %s at %f, %f, %f (%s)", pPickupData->GetName(), pPlacement->GetPickupData()->GetName(), placementPos.x, placementPos.y, placementPos.z, pPlacement->GetNetworkObject() ? pPlacement->GetNetworkObject()->GetLogName() : "");
					break;
				}
			}
		}
#endif
		CPickupPlacement* pPlacement = rage_new CPickupPlacement(pickupHash, pickupPosition, pickupOrientation, flags, customModelIndex);

		if (Verifyf(pPlacement, "Failed to allocate a new pickup placement"))
		{
			Vector2 position2D(pickupPosition.x, pickupPosition.y);
			fwRect bb(position2D, 1.0f);

			Assertf(!(pPlacement->GetRegenerates() && pPickupData->GetRegenerationTime() == 0.0f), "RegisterPickupPlacement: %s set to regenerate has no regeneration time!", pPickupData->GetName());

			ms_pQuadTree->AddItem(pPlacement, bb);

			if (pNetObj)
			{
				pPlacement->SetNetworkObject(pNetObj);
			}
			else if (!pPlacement->GetIsMapPlacement() && NetworkInterface::IsGameInProgress() && (pPlacement->GetFlags() & CPickupPlacement::PLACEMENT_CREATION_FLAG_LOCAL_ONLY)==0)
			{
				// placements are always created by scripts so they automatically become script network objects 
				NetworkInterface::GetObjectManager().RegisterGameObject(pPlacement, 0, CNetObjGame::GLOBALFLAG_SCRIPTOBJECT|CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION|CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT);
				Assert(pPlacement->GetNetworkObject());
			}
			
			// if the placement is networked, or in scope of the player, place on pending list so that its pickup object is created now
			if (pPlacement->GetInScope() && !pPlacement->GetIsCollected() && !pPlacement->GetHasPickupBeenDestroyed())
			{
				Assert(!ms_pendingPickups.IsMemberOfList(pPlacement));
				ms_pendingPickups.Add(pPlacement);
			}

			return pPlacement;
		}
	}

	return NULL;
}

// Name			:	FindPickupPlacement
// Purpose		:	Finds the pickup placement at the given position
// Parameters	:	pickupPosition - the position of the pickup
CPickupPlacement* CPickupManager::FindPickupPlacement(const Vector3 &pickupPosition)
{
#define FIND_RANGE 0.2f

	Vector2 pos2d(pickupPosition.x, pickupPosition.y);
	fwRect bb(pos2d, FIND_RANGE);

	// linked list of current pickup placements in scope
	fwPtrListSingleLink pickupsInScope;

	ms_pQuadTree->GetAllMatching(bb, pickupsInScope);

	fwPtrNode* pNode=pickupsInScope.GetHeadPtr();

	while(pNode)
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(pNode->GetPtr());

		Vector3 diff = pPlacement->GetPickupPosition() - pickupPosition;

		if (diff.Mag2() < FIND_RANGE*FIND_RANGE)
			return pPlacement;

		pNode=pNode->GetNextPtr();
	}

	return NULL;
}

// Name			:	RemovePickupPlacement
// Purpose		:	Removes the pickup placement with the given script info.
// Parameters	:	info - the script info of the pickup to remove
// Returns		:	Nothing
void CPickupManager::RemovePickupPlacement(const CGameScriptObjInfo& info)
{
	CPickupPlacement* pPlacement = GetPickupPlacementFromScriptInfo(info);

	Assertf(pPlacement, "RemovePickupPlacement: pickup does not exist with this script info");

	if (pPlacement)
	{
		bool bIsLocal = !pPlacement->GetNetworkObject() || (!pPlacement->IsNetworkClone() && !pPlacement->GetNetworkObject()->IsPendingOwnerChange()) || pPlacement->GetIsMapPlacement();

		if (Verifyf(bIsLocal, "Trying to remove a pickup placement that is controlled by another machine"))
		{
			RemovePickupPlacement(pPlacement);
		}
	}
}

// Name			:	RemovePickupPlacement
// Purpose		:	Removes the given pickup placement
// Parameters	:	pPlacement - the pickup placement
// Returns		:	Nothing
void CPickupManager::RemovePickupPlacement(CPickupPlacement *pPlacement)
{
	if (AssertVerify(pPlacement))
	{
		bool bIsLocal = !pPlacement->GetNetworkObject() || (!pPlacement->IsNetworkClone() && !pPlacement->GetNetworkObject()->IsPendingOwnerChange()) || pPlacement->GetIsMapPlacement();

		if (Verifyf(bIsLocal, "Trying to remove a pickup placement that is controlled by another machine"))
		{
			pPlacement->SetIsBeingDestroyed();

			if (pPlacement->GetScriptHandler())
			{
				pPlacement->GetScriptHandler()->UnregisterScriptObject(static_cast<scriptHandlerObject&>(*pPlacement));
			}

			if (pPlacement->GetNetworkObject())
			{
				NetworkInterface::GetObjectManager().UnregisterObject(pPlacement, pPlacement->IsNetworkClone());
			}

			// remove placement immediately if shutting down, if not add to removal list
			if (ms_bShuttingDown)
			{
				DestroyPickupPlacement(pPlacement);
			}
			else if (!ms_removedPlacements.IsMemberOfList(pPlacement))
			{
				ms_removedPlacements.Add(pPlacement);
			}
		}
	}
}

// Name			:	CreatePickup
// Purpose		:	Tries to creates a new pickup object. May fail if the model is not streamed in.
// Parameters	:	pickupHash - the hash of the new pickup type
//					placementMatrix - the matrix specifying the position and orientation of the pickup
//					pNetObj - if a network object is passed in this is assigned to the new pickup, otherwise it is registered as a new network object
//					bRegisterAsNetworkObject - if true the pickup is registered as a network object and synced across the network
//					customModelIndex - create a pickup with the given model index
// Returns		:   A pointer to the new pickup, if the creation was successful
CPickup* CPickupManager::CreatePickup(u32 pickupHash, const Matrix34& placementMatrix, netObject* pNetObj, bool bRegisterAsNetworkObject, u32 customModelIndex, bool bCreateDefaultWeaponAttachments, bool bCreateAsScriptEntity)
{
	// Don't try to create a pickup if there are no objects free,
	if(CObject::GetPool()->GetNoOfFreeSpaces() == 0)
	{
#if !__FINAL
		Errorf("Object pool is full. Unable to create a pickup \n");
		CObject::DumpObjectPool();
#endif
		return NULL;
	}

	CPickup* pPickup = NULL;

	const CPickupData* pPickupData = CPickupDataManager::GetPickupData(pickupHash);

	if (Verifyf(pPickupData, "Can't create pickup - pickup metadata does not exist for this pickup hash (%u)", pickupHash))
	{
		u32 modelIndex = customModelIndex != fwModelId::MI_INVALID ? customModelIndex : pPickupData->GetModelIndex();

		if (modelIndex == fwModelId::MI_INVALID || !CModelInfo::GetStreamingModule()->IsObjectInImage(strLocalIndex(modelIndex)))
		{
			Errorf("Model for %s does not exist! (modelIndex=%d)", pPickupData->GetName(), modelIndex);
			Assertf(0, "Model for %s does not exist! (modelIndex=%d)", pPickupData->GetName(), modelIndex);
		}
		else
		{
			u32 numRewards = pPickupData->GetNumRewards();

			// Some pickups can have no rewards: these are pickups created a script that have rewards dictated by the script when it detects that the
			// pickup has been collected. These must be created.
			if (numRewards > 0)
			{
				bool bFoundValidReward = false;

				// Walk through the pickup type rewards and make sure at least 1 reward exists
				for( u32 i = 0; i < numRewards; i++ )
				{
					const CPickupRewardData* pReward = pPickupData->GetReward(i);
					if( !IsSuppressionFlagSet( pReward->GetType() ) )
					{
						bFoundValidReward = true;
						break;
					}
				}

				// Did we fail to find a non-suppressed reward?
				if( !bFoundValidReward && !pNetObj)
				{
					Errorf("Can't create pickup - all rewards suppressed");
					Assertf(!NetworkInterface::IsGameInProgress(), "Can't create pickup - all rewards suppressed");
					return NULL;
				}
			}
#if __DEV
			// Have to set this flag to ensure objects are only created through the appropriate methods
			CPickup::bInObjectCreate = true;
#endif // __DEV

			if (pPickupData)
			{
				ClearSpaceInPickupPool(bRegisterAsNetworkObject);

				if (CPickup::GetPool()->GetNoOfFreeSpaces() > 0 && 
					(!NetworkInterface::IsGameInProgress() || CNetObjPickup::GetPool()->GetNoOfFreeSpaces() > 0))
				{
					CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(rage::fwModelId(strLocalIndex(modelIndex)));

					if(pModelInfo == NULL || Verifyf(pModelInfo->GetFragType() == NULL,"Could not create pickup '%s' because it is a fragment ('%s').",pPickupData->GetName(),pModelInfo->GetFragType()->GetBaseName()))
					{
						pPickup = rage_new CPickup(ENTITY_OWNEDBY_RANDOM, pickupHash, customModelIndex, bCreateDefaultWeaponAttachments);
						pPickup->SetForceAlphaAndUseAmbientScale();
						pPickup->SetUseLightOverrideForGlow();
					}
					else
					{
						Errorf("Could not create pickup '%s' because it is a fragment ('%s').",pPickupData->GetName(),pModelInfo->GetFragType()->GetBaseName());
					}
				}
				else
				{
#if __DEV
					static bool sbDumped = false;
					// Dump pickup pool. Only do this once.
					if (!sbDumped)
					{
						CPickup::_ms_pPool->PoolFullCallback();
						sbDumped = true;
					}
#endif // __DEV

					if (CPickup::GetPool()->GetNoOfFreeSpaces() == 0)
					{
						Errorf("Can't create pickup - no spaces left in CPickup pool, see TTY for details");
						Assertf(0, "Can't create pickup - no spaces left in CPickup pool, see TTY for details");
					}
					else
					{
						Errorf("Can't create pickup - no spaces left in CNetObjPickup pool, see TTY for details");
						Assertf(0, "Can't create pickup - no spaces left in CNetObjPickup pool, see TTY for details");
					}
				}
			}

#if __DEV
			CPickup::bInObjectCreate = false;
#endif // __DEV

			if(pPickup)
			{
				static dev_float TINY_PICKUP_OFFSET = 0.001f;
				Matrix34 pickupMat(placementMatrix);
				pickupMat.d.z += TINY_PICKUP_OFFSET;
				pickupMat.Normalize();

				// Set the matrix
				pPickup->SetMatrix(pickupMat, true, true, true);

				if (pPickup->GetPickupData()->GetAlwaysFixed())
				{
					pPickup->SetFixedPhysics(true);
				}

				// Add to the world, if no custom archetype needs assigned. If it does, it will be added later.
				if (!pPickup->GetRequiresACustomArchetype())
				{
					CGameWorld::Add(pPickup, CGameWorld::OUTSIDE );

					// pickups without placements have been dropped, so activate physics
					if (!pPickup->GetPlacement() && !pPickup->GetPickupData()->GetAlwaysFixed())
					{
						pPickup->ActivatePhysics();
					}
				}

				// Update portal tracker
				if(pPickup->GetPortalTracker())
				{
					pPickup->GetPortalTracker()->SetLoadsCollisions(false);
					pPickup->GetPortalTracker()->RequestRescanNextUpdate();
					pPickup->GetPortalTracker()->Update(pickupMat.d);
				}

				// Initialise any stuff after pickup has been added to the world
				pPickup->Init();

				if (bCreateAsScriptEntity)
				{
					pPickup->SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);
				}

				// assign or create a network object for this pickup
				if (pNetObj)
				{
					Assert(pNetObj->IsClone());

					// if a network object ptr is being passed in, then this is a clone pickup
					pPickup->SetNetworkObject(pNetObj);

					if (static_cast<CNetObjGame*>(pNetObj)->IsScriptObject())
					{
						pPickup->SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);
					}
				}
				else if (bRegisterAsNetworkObject && NetworkInterface::IsGameInProgress())
				{
					// placements are always created by scripts, so make placement pickups script objects too
					NetObjFlags globalFlags = pPickup->GetPlacement() ? (CNetObjGame::GLOBALFLAG_SCRIPTOBJECT|CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION|CNetObjGame::GLOBALFLAG_CLONEONLY_SCRIPT) : 0;
						
					if (bCreateAsScriptEntity)
					{
						globalFlags |= CNetObjGame::GLOBALFLAG_SCRIPTOBJECT;
					}

					if (!NetworkInterface::RegisterObject(pPickup, 0, globalFlags))
					{
						Errorf("Can't create pickup - failed to register it as a network object");

						// pickup could not be registered as a network object (due to maximum number of CNetObjPickups being exceeded)
						pPickup->Destroy();
						pPickup = NULL;

						NETWORK_QUITF(!bCreateAsScriptEntity, "Failed to register script pickup with network code - ran out of network objects");
					}
					else
					{
						Assert(pPickup->GetNetworkObject());
					}
				}
			}
		}
	}

	return pPickup;
}

// Name			:	CreatePickUpFromCurrentWeapon
// Purpose		:	Tries to creates a new pickup object from a peds current weapon. May fail if the model is not streamed in.
// Parameters	:	pPed - the ped who is holding the weapon
// Returns		:   Nothing
CPickup* CPickupManager::CreatePickUpFromCurrentWeapon(CPed* pPed, bool bUsePedAmmo, bool bDropWeaponIfUnarmed)
{
	Assert( pPed );

	// clones never generate pickups from dropped weapons
	if (pPed->IsNetworkClone())
	{
		return NULL;
	}

	// If random peds are allowed to drop weapons, or we are the player, or we are a mission ped then do so.
	if( CPed::GetRandomPedsDropWeapons() || pPed->IsLocalPlayer() || pPed->PopTypeIsMission() )
	{
		const CPedWeaponManager* pWeaponManager = pPed->GetWeaponManager();
		if(pWeaponManager)
		{
			// Try to drop a weapon when a cop or a mission ped is killed unarmed.
			// 1. Drop the last equipped weapon if it's non-unarmed. 2. Drop the best weapon if he never had a weapon equipped. 
			const CWeaponInfo* pDroppedWeaponInfo = NULL;
			bool bUseMPPickups = ShouldUseMPPickups(pPed);

			const CObject* pWeaponObject = pWeaponManager->GetEquippedWeaponObject();
			if(bDropWeaponIfUnarmed)
			{
				// If no equipped weapon or the equipped weapon is unarmed
				const CWeaponInfo* pEquippedWeaponInfo  = pWeaponManager->GetEquippedWeaponInfo();
				if(!pWeaponObject || !pEquippedWeaponInfo || pEquippedWeaponInfo->GetIsUnarmed())
				{
					pDroppedWeaponInfo = pPed->GetWeaponManager()->GetLastEquippedWeaponInfo();
					// Use the best weapon if the last equipped weapon is NULL or doesn't have a valid pickup hash.
					if(!pDroppedWeaponInfo ||  !pPed->GetInventory()->GetWeapon(pDroppedWeaponInfo->GetHash()) || (bUseMPPickups ? (pDroppedWeaponInfo->GetMPPickupHash() == 0) : (pDroppedWeaponInfo->GetPickupHash() == 0)))
					{
						pDroppedWeaponInfo = pPed->GetWeaponManager()->GetBestWeaponInfo();
						if(pDroppedWeaponInfo && (bUseMPPickups ? (pDroppedWeaponInfo->GetMPPickupHash() == 0) : (pDroppedWeaponInfo->GetPickupHash() == 0)))
						{
							pDroppedWeaponInfo = NULL;
						}
					}
				}
			}

			if(pWeaponObject || pDroppedWeaponInfo)
			{
				const CWeapon* pWeapon = pWeaponObject ? pWeaponObject->GetWeapon() : NULL;
				const CWeaponInfo* pWeaponInfo = bDropWeaponIfUnarmed ? pDroppedWeaponInfo : (pWeapon ? pWeapon->GetWeaponInfo() : NULL);
				if(pWeaponInfo)
				{
					u32 uPickupHash = bUseMPPickups ? pWeaponInfo->GetMPPickupHash() : pWeaponInfo->GetPickupHash();
					if( Verifyf( uPickupHash != 0, "Weapon [%s] doesn't have a pickup associated with it. DropWeaponIfUnarmed: %s", pWeaponInfo->GetName(), bDropWeaponIfUnarmed ? "T":"F" ) )
					{
						// Only create a pickup if not a player, or player has ammo for their gun
						if( !pPed->IsAPlayerPed() || (pWeapon && pWeapon->GetAmmoTotal() > 0) || pWeaponInfo->GetIsMelee() || bUsePedAmmo || pDroppedWeaponInfo )
						{
							const CPickupData* pPickupData = CPickupDataManager::GetPickupData( uPickupHash );

							const bool bReusingWeaponModel = !pDroppedWeaponInfo && pPickupData && pWeaponInfo->GetModelHash() == pPickupData->GetModelHash();

							Matrix34 m;
							if(pDroppedWeaponInfo)
							{
								m.Identity();
								m.d = VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition());
							}
							else
							{
								m = MAT34V_TO_MATRIX34(pWeaponObject->GetMatrix());
							}

							// We don't want the pickup is created on the top of a car if the ped is killed inside.
							if(pPed->GetIsAttachedInCar())
							{
								CVehicle* pMyVehicle = pPed->GetMyVehicle();
								if(pMyVehicle && pMyVehicle->GetVehicleType() == VEHICLE_TYPE_CAR)
								{
									CPickupPositionInfo pos( m.d );
									pos.m_MinDist = 0.0f;
									pos.m_MaxDist = 2.0f;
									CalculateDroppedPickupPosition( pos, true );

									m.d = pos.m_Pos;
								}
							}

							strLocalIndex customModelIndex = strLocalIndex(fwModelId::MI_INVALID);

							if (!bUseMPPickups && pWeapon) 
							{
								// If we have a variation model component on the weapon, use that model for the pickup
								const CWeaponComponentVariantModelInfo* pVariantInfo = pWeapon->GetVariantModelComponent() ? pWeapon->GetVariantModelComponent()->GetInfo() : NULL;
								if (pVariantInfo)
								{
									// Make sure the model exists
									fwModelId variantModelId;
									CModelInfo::GetBaseModelInfoFromHashKey(pVariantInfo->GetModelHash(), &variantModelId);	
									if (weaponVerifyf(variantModelId.IsValid(), "ModelId is invalid for VariantModel component on [%s], using original model for pickup instead.", pWeaponInfo->GetName()))
									{
										customModelIndex = variantModelId.GetModelIndex();
									}
								}
							}

							CPickup* pPickup = CPickupManager::CreatePickup( uPickupHash, m, NULL, true, customModelIndex.Get(), !bReusingWeaponModel );
							if( pPickup )
							{
								// prevent the pickup from displaying help text immediately
								if (pPed->IsLocalPlayer())
								{
									pPickup->SetFlag(CPickup::PF_HelpTextDisplayed);
									pPickup->SetFlag(CPickup::PF_LocalPlayerCollision);
									if( !NetworkInterface::IsGameInProgress() )
										pPickup->SetFlag(CPickup::PF_DontGlow);
								}
								else if (pPed->PopTypeIsMission() || (pPed->GetNetworkObject() && pPed->GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT)))
								{
									pPickup->SetDroppedByPed();
								}

								if(!pDroppedWeaponInfo)
								{
									// The total ammo can be set to -1 for infinite ammo.
									if( bUsePedAmmo && pWeapon->GetAmmoTotal() > 0 )
									{
										// Use the weapon ammo for the pickup
										pPickup->SetAmount(pWeapon->GetAmmoTotal());
									}
									else if( bUseMPPickups )
									{
										u32 maxAmmoInClip = pWeapon->GetClipSize();

										// The total ammo can be set to -1 for infinite ammo. Ensure we don't pass -1 into the unsigned pickup amount.
										s32 iAmmoInWeapon = pWeapon->GetAmmoTotal();

										if (iAmmoInWeapon == 0)
										{
											delete pPickup;
											return NULL;
										}

										u32 uWeaponToDecrementAmmoFrom = pWeapon->GetWeaponHash();
										s32 sWeaponToDecrementAmmoFromTotal = iAmmoInWeapon;

										if( pWeaponInfo->GetHash() == WEAPONTYPE_STUNGUN )
										{
											uWeaponToDecrementAmmoFrom = pPed->GetWeaponManager()->GetBestWeaponHash( pWeaponInfo->GetHash() );
											sWeaponToDecrementAmmoFromTotal = pPed->GetInventory()->GetWeaponAmmo( uWeaponToDecrementAmmoFrom );
										}

										// As infinite ammo, use max ammo.
										if(sWeaponToDecrementAmmoFromTotal < 0)
										{
											sWeaponToDecrementAmmoFromTotal = maxAmmoInClip;
										}

										// Initialise the pickup to be the amount of ammo in the current peds weapon, up to a max of MAX_AMMO_MP
										u32 uAmmoAmount = maxAmmoInClip;

										if (uPickupHash == PICKUP_AMMO_MISSILE_MP || 
											uPickupHash == PICKUP_AMMO_GRENADELAUNCHER_MP ||
											uPickupHash == PICKUP_WEAPON_GRENADE || 
											uPickupHash == PICKUP_WEAPON_SMOKEGRENADE || 
											uPickupHash == PICKUP_WEAPON_STICKYBOMB || 
											uPickupHash == PICKUP_WEAPON_MOLOTOV)
										{
											uAmmoAmount = 1;
										}
										else if( !pPed->IsAPlayerPed() )
										{
											// Randomise the amount for AI peds
											uAmmoAmount = fwRandom::GetRandomNumberInRange(1, maxAmmoInClip);

											// some peds are told how much ammo to drop by script
											if (pPed->GetNetworkObject())
											{
												u32 ammoToDrop = static_cast<CNetObjPed*>(pPed->GetNetworkObject())->GetAmmoToDrop();

												if (ammoToDrop > 0)
												{
													uAmmoAmount = ammoToDrop;
												}
											}
										}
										else
										{
											if(iAmmoInWeapon > -1)
											{
												uAmmoAmount = Min((u32)iAmmoInWeapon, maxAmmoInClip);
											}
											else	// As infinite ammo, use max ammo.
											{
												uAmmoAmount = maxAmmoInClip;
											}
										}

										// some weapons (like the minigun) only have a single large clip, so cap how much can be dropped
										// matches hard-coded values in script func GET_AMMO_AMOUNT_FOR_MP_PICKUP of how much ammo is removed from player on kill
										u32 uAmmoDroppedCap = 0;

										if (pWeaponInfo->GetHash() == WEAPONTYPE_MINIGUN)
										{
											uAmmoDroppedCap = 200;
										}
										else if (pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_RAYCARBINE", 0x476BF155))
										{
											uAmmoDroppedCap = 100; // iAW_WEAPON_CLIP_CAPACITY_UNHOLY_HELLBRINGER in mp_globals_tunables.sch
										}
										else if (pWeaponInfo->GetHash() == ATSTRINGHASH("WEAPON_RAYMINIGUN", 0xB62D1F67))
										{
											uAmmoDroppedCap = 50; // iAW_WEAPON_CLIP_CAPACITY_WIDOWMAKER in mp_globals_tunables.sch
										}

										if (uAmmoDroppedCap > 0 && uAmmoAmount > uAmmoDroppedCap)
										{
											uAmmoAmount = uAmmoDroppedCap;
										}

										// Set the pickup to only give this amount of ammo
										Assert(uAmmoAmount < 65535);
										pPickup->SetAmount(uAmmoAmount);

										// Decrement this amount of ammo from the ped dropping the weapon
										if( pPed->GetInventory() )
										{
											// Decrement one ammo for explosive weapons, otherwise decrement the ammo given to the pickup
											u32 uAmmoDecrement = uAmmoAmount;
											const CWeaponInfo* pWeaponInfoToDecrementFrom = CWeaponInfoManager::GetInfo<CWeaponInfo>(uWeaponToDecrementAmmoFrom);
											if( pWeaponInfoToDecrementFrom && pWeaponInfoToDecrementFrom->GetDamageType() == DAMAGE_TYPE_EXPLOSIVE )
											{ 
												uAmmoDecrement = 1;
											}
											pPed->GetInventory()->NotifyAmmoChange( uWeaponToDecrementAmmoFrom, sWeaponToDecrementAmmoFromTotal-uAmmoDecrement );
										}
									}

									SetStartingVelocities(pPickup, pPed, RAGDOLL_HAND_RIGHT, RAGDOLL_CLAVICLE_RIGHT);

									// Only create and set weapon for weapon type pickups. 
									bool bIsWeaponPickup = false;
									for (u32 i=0; i<pPickupData->GetNumRewards(); i++)
									{
										const CPickupRewardData* pReward = pPickupData->GetReward(i);
										if (pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
										{
											bIsWeaponPickup = true;
											break;
										}
									}

									if( bIsWeaponPickup && !pPickup->GetWeapon() )
									{
										pPickup->SetWeapon(WeaponFactory::Create(pWeapon->GetWeaponHash(), pWeapon->GetAmmoTotal(), pPickup, "CPickupManager::CreatePickUpFromCurrentWeapon"));
										if (pWeapon->GetIsCooking())  //if this weapon was cooking transfer the love
										{
											pPickup->GetWeapon()->StartCookTimer(pWeapon->GetCookTime(), pPed);
										}

										if(pPickup->GetWeapon())
										{
											pPickup->GetWeapon()->GetAudioComponent().SetWasDropped(true);
										}
									}

									// Only do this if the pickup uses the same model as the original weapon
									if( bReusingWeaponModel )
									{
										// Make sure pickup has correct attachments
										const CWeapon::Components& components = pWeapon->GetComponents();
										for( s32 i = 0; i < components.GetCount(); i++ )
										{
											CPedEquippedWeapon::CreateWeaponComponent( components[i]->GetInfo()->GetHash(), pPickup );
										}

										bool bBlockFireOnImpact = false;
										if( pWeapon->GetWeaponInfo()->GetCreateVisibleOrdnance() )
										{
											if( CPedEquippedWeapon::GetProjectileOrdnance( pWeaponObject ) )
											{
												CPedEquippedWeapon::CreateProjectileOrdnance( pPickup, NULL );
											}
											bBlockFireOnImpact = true;
										}

										if( !pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_StopWeaponFiringOnImpact ) && 
											!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_KilledByStealth ) &&							
											pPickup->GetDrawable() &&
											!bBlockFireOnImpact )
										{
											// Only shoot on impacts 50% of the time
											pPickup->m_nObjectFlags.bWeaponWillFireOnImpact = fwRandom::GetRandomTrueFalse();
											weaponDebugf1("Pickup [0x%p] dropped by ped [0x%p]: bWeaponWillFireOnImpact %d", pPickup, pPed, pPickup->m_nObjectFlags.bWeaponWillFireOnImpact);
										}
										else
										{
											pPickup->m_nObjectFlags.bWeaponWillFireOnImpact = false;
										}
									}

									// Set the correct rendering flags now whole hierarchy has been made (so attachments are set).
									pPickup->SetForceAlphaAndUseAmbientScale();
									// Set up the glow too.
									pPickup->SetUseLightOverrideForGlow();

									// Make sure pickup has correct tint
									if( pPickup->GetWeapon() )
									{
										pPickup->GetWeapon()->UpdateShaderVariables( pWeapon->GetTintIndex() );
									}
								}
								pPickup->SetLastOwner(pPed);
								return pPickup;
							}
						}
					}
				}
			}
		}
	}

	return NULL;
}


// Name			:	GetPickupPlacementFromScriptInfo
// Purpose		:	Returns the pickup placement corresponding to the given script info
// Parameters	:	info - the pickup script info
// Returns		:   A pointer to the pickup placement.
CPickupPlacement* CPickupManager::GetPickupPlacementFromScriptInfo(const CGameScriptObjInfo& info)
{

	s32 i = CPickupPlacement::GetPool()->GetSize();

	// test pickup objects
	while(i--)
	{
		CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

		if (pPlacement && pPlacement->GetScriptInfo() && *pPlacement->GetScriptInfo() == info)
		{
			return pPlacement;
		}
	}

	return NULL;
}


// 
// Name			:	RegisterPickupCollection
// Purpose		:	Called when the pickup for the given placement is collected
// Parameters	:	pPlacement - the pickup placement that spawned the pickup that was collected
//					pPed - the ped who collected it
// Returns		:   Nothing
void CPickupManager::RegisterPickupCollection(CPickupPlacement* pPlacement,  CPed* pPed)
{
	if (AssertVerify(pPlacement) && AssertVerify(!pPlacement->GetIsCollected()))
	{
		if (pPlacement->GetScriptInfo())
		{
			// add to collection history

			// TODO: remove this history. The script event should be used instead. At the moment the scripts are still using the old code and 
			// need to listen for the event instead.
			
			CCollectionInfo collectInfo(*pPlacement->GetScriptInfo(), pPed);

			if (ms_collectionHistory.IsFull())
			{
				ms_collectionHistory.Drop();
			}

			ms_collectionHistory.Push(collectInfo);

			// Add network script event for the pickup event.
			if (pPed && pPed->GetNetworkObject() && (pPed->IsLocalPlayer() || pPed->IsNetworkPlayer()))
			{
				CEventNetworkPlayerCollectedPickup pickupEvent(*pPed->GetNetworkObject()->GetPlayerOwner(), 
					pPlacement->GetScriptInfo()->GetObjectId(), 
					pPlacement->GetPickupHash(), 
					pPlacement->GetAmount(), 
					pPlacement->GetCustomModelHash(),
					(pPlacement->GetAmountCollected() > 0) ? pPlacement->GetAmountCollected() : pPlacement->GetAmount());

				GetEventScriptNetworkGroup()->Add(pickupEvent);
				if(NetworkInterface::IsGameInProgress() && (pPed->IsNetworkPlayer() || pPed->IsNetworkBot()))
				{
					g_ScriptAudioEntity.PlayNPCPickup(pPlacement->GetPickupHash(),pPlacement->GetPickupPosition());
				}
			}
			else
			{
				CEventNetworkPlayerCollectedPickup pickupEvent(pPlacement->GetScriptInfo()->GetObjectId(), 
					pPlacement->GetPickupHash(), 
					pPlacement->GetAmount(), 
					pPlacement->GetCustomModelHash(),
					(pPlacement->GetAmountCollected() > 0) ? pPlacement->GetAmountCollected() : pPlacement->GetAmount());
				GetEventScriptNetworkGroup()->Add(pickupEvent);
			}
		}

		// if we are waiting to collect this pickup, then do it now
		if (pPlacement->IsNetworkClone())
		{
			pPlacement->Collect(pPed);
		}
		else
		{
			// if this is a regenerating placement, add to the regenerating list, otherwise it is removed
			if (pPlacement->GetRegenerates()) 
			{
				AddPlacementToRegenerationList(pPlacement);
			}	

			pPlacement->Collect(pPed);
		}
	}
}

// 
// Name			:	RegisterPickupDestruction
// Purpose		:	Called when the pickup for the given placement is destroyed
// Parameters	:	pPlacement - the pickup placement that spawned the pickup that was destroyed
// Returns		:   Nothing
void CPickupManager::RegisterPickupDestruction(CPickupPlacement *pPlacement)
{
	if (AssertVerify(pPlacement) && pPlacement->GetRegenerates()) 
	{
		AddPlacementToRegenerationList(pPlacement);
	}
}

// Name			:	HasPickupBeenCollected
// Purpose		:	Returns true if the pickup with the given script info has been picked up.
// Parameters	:	scriptInfo - the script info 
//					pPedWhoGotIt - set to the ped who collected the pickup, if it was collected
// Returns		:   True if the pickup has been collected
bool CPickupManager::HasPickupBeenCollected(const CGameScriptObjInfo& scriptInfo, CPed** ppPedWhoGotIt)
{
	int index;
	CCollectionInfo info(scriptInfo);

	if (ms_collectionHistory.Find(info, &index))
	{
		if (ppPedWhoGotIt)
			*ppPedWhoGotIt = ms_collectionHistory[index].GetPed();

		// remove the collection history once it has been queried
		ms_collectionHistory.Delete(index);

		return true;
	}

	return false;
}

// Name			:	GetRegenerationTime
// Purpose		:	Returns the time the given placement will regenerate its pickup object
u32 CPickupManager::GetRegenerationTime(CPickupPlacement *pPlacement)
{
	fwPtrNode* pNode = ms_regeneratingPlacements.GetHeadPtr();

	while (pNode)
	{
		CRegenerationInfo* pRegenInfo = static_cast<CRegenerationInfo*>(pNode->GetPtr());
		fwPtrNode* pNextNode = pNode->GetNextPtr();

		if (pRegenInfo->GetPlacement() == pPlacement)
		{
			return pRegenInfo->GetRegenerationTime();
		}

		pNode = pNextNode;
	}

	if (AssertVerify(pPlacement))
	{
		if (pPlacement->GetNetworkObject())
		{
			Assertf(0, "CPickupManager::GetRegenerationTime: %s is not regenerating", pPlacement->GetNetworkObject()->GetLogName());
		}
		else
		{
			Assertf(0, "CPickupManager::GetRegenerationTime: placement at %f, %f, %f is not regenerating", pPlacement->GetPickupPosition().x, pPlacement->GetPickupPosition().y, pPlacement->GetPickupPosition().z);
		}
	}

	return 0;
}

// Name			:	MigratePlacementOwnership
// Purpose		:	Called when the ownership of a placement migrates locally in a network game
// Parameters	:	pPlacement - the placement
// 				:	regenerationTime - the time the given placement will regenerate its pickup object. 
void CPickupManager::MigratePlacementOwnership(CPickupPlacement *pPlacement, u32 regenerationTime)
{
	if (AssertVerify(pPlacement))
	{
		// if we are waiting to collect this pickup, then do it now
		if (!pPlacement->GetIsCollected() && pPlacement->GetPickup() && pPlacement->GetPickup()->GetIsPendingCollection())
		{
			 pPlacement->GetPickup()->ProcessCollectionResponse(NULL, true);
		}

		if ((pPlacement->GetIsCollected() || pPlacement->GetHasPickupBeenDestroyed()) && 
			pPlacement->GetRegenerates() &&
			!IsPlacementOnRegenerationList(pPlacement))
		{
			CRegenerationInfo* pRegenInfo = AddPlacementToRegenerationList(pPlacement);

			if (regenerationTime != 0)
			{
				pRegenInfo->SetRegenerationTime(regenerationTime);
			}
		}

		// if the placement is in scope of the player, place on pending list so that its pickup object is created now
		if (pPlacement->GetInScope() && !pPlacement->GetIsCollected() && !pPlacement->GetHasPickupBeenDestroyed())
		{
			if (!ms_pendingPickups.IsMemberOfList(pPlacement))
			{
				ms_pendingPickups.Add(pPlacement);
			}
		}
	}
}

// Name			:	GetIsAnyPickupInBounds
// Purpose		:	Returns true if any pickup objects or placements exist within the given bounding box
// Parameters	:	bounds - the bounding box
// Returns		:   True if any pickup objects exists within the given bounding box
bool CPickupManager::GetIsAnyPickupInBounds(const spdAABB& bounds)
{
	s32 i = CPickup::GetPool()->GetSize();

	// test pickup objects
	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && bounds.ContainsPoint(pPickup->GetTransform().GetPosition()))
		{
			return true;
		}
	}

	i = CPickupPlacement::GetPool()->GetSize();

	// test pickup placements
	while(i--)
	{
		CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

		if (pPlacement && bounds.ContainsPoint(RCC_VEC3V(pPlacement->GetPickupPosition())))
		{
			return true;
		}
	}

	return false;
}

// Name			:	RemoveAllPickups
// Purpose		:	Removes all pickup objects
// Parameters	:	bIncludeScriptPickups - if set all script created pickups and placements are also removed
// Returns		:   Nothing
void CPickupManager::RemoveAllPickups(bool bIncludeScriptPickups, bool bImmediately)
{
	s32 i = CPickup::GetPool()->GetSize();

	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && !pPickup->IsNetworkClone() && (!pPickup->GetNetworkObject() || pPickup->GetNetworkObject()->CanDelete()))
		{
			if ((!pPickup->GetNetworkObject() || pPickup->GetNetworkObject()->CanDelete()) &&
				(bIncludeScriptPickups || !pPickup->GetExtension<CScriptEntityExtension>()))
			{
				if (bImmediately)
				{
					if (pPickup->GetPlacement())
					{
						pPickup->GetPlacement()->SetPickup(NULL);
						pPickup->SetPlacement(NULL);
					}

					CObjectPopulation::DestroyObject(pPickup);
				}
				else
				{
					pPickup->Destroy();
				}

			}
		}
	}

	if (bIncludeScriptPickups)
	{
		s32 i = CPickupPlacement::GetPool()->GetSize();

		// remove placements
		while(i--)
		{
			CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

			if (pPlacement && !pPlacement->IsNetworkClone())
			{
				RemovePickupPlacement(pPlacement);
			}
		}
	}
}

// Name			:	RemoveAllPickupsOfType
// Purpose		:	Removes all pickup objects and placements with the given type
// Parameters	:	pickupHash - the hash of the pickup type
//					bIncludeScriptPickups - if set all script created pickups and placements are also removed
// Returns		:   Nothing
void CPickupManager::RemoveAllPickupsOfType(u32 pickupHash, bool bIncludeScriptPickups)
{
	s32 i = CPickup::GetPool()->GetSize();

	// remove pickups that have no corresponding placements
	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && !pPickup->IsNetworkClone() && !pPickup->GetPlacement() && pPickup->GetPickupHash() == pickupHash)
		{
			if ((!pPickup->GetNetworkObject() || pPickup->GetNetworkObject()->CanDelete()) &&
				(bIncludeScriptPickups || !pPickup->GetExtension<CScriptEntityExtension>()))
			{
				pPickup->Destroy();
			}
		}
	}

	if (bIncludeScriptPickups)
	{
		i = CPickupPlacement::GetPool()->GetSize();

		// remove placements
		while(i--)
		{
			CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

			if (pPlacement && !pPlacement->IsNetworkClone() && pPlacement->GetPickupHash() == pickupHash)
			{
				RemovePickupPlacement(pPlacement);
			}
		}
	}
}

// Name			:	RemoveAllPickupsInBounds
// Purpose		:	Removes all pickup objects and placements within the given bounding box
// Parameters	:	bounds - the bounding box
//					bLowPriorityOnly - if true only pickups flagged as low priority are removed
//					bIncludeScriptPickups - if set all script created pickups and placements are also removed
// Returns		:   Nothing
void CPickupManager::RemoveAllPickupsInBounds(const spdAABB& bounds, bool bLowPriorityOnly, bool bIncludeScriptPickups)
{
	s32 i = CPickup::GetPool()->GetSize();

	// remove pickups that have no corresponding placements
	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && !pPickup->IsNetworkClone() && !pPickup->GetPlacement() && bounds.ContainsPoint(pPickup->GetTransform().GetPosition()) && (pPickup->GetPickupData()->GetLowPriority() || !bLowPriorityOnly ))
		{
			if ((!pPickup->GetNetworkObject() || pPickup->GetNetworkObject()->CanDelete()) &&
				(bIncludeScriptPickups || !pPickup->GetExtension<CScriptEntityExtension>()))
			{
				pPickup->Destroy();
			}
		}
	}

	if (bIncludeScriptPickups)
	{
		i = CPickupPlacement::GetPool()->GetSize();

		// remove placements
		while(i--)
		{
			CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

			if (pPlacement && !pPlacement->IsNetworkClone() )
			{
				const CPickupData* pData = CPickupDataManager::GetPickupData(pPlacement->GetPickupHash());

				if (AssertVerify(pData) && bounds.ContainsPoint(RCC_VEC3V(pPlacement->GetPickupPosition())) && (pData->GetLowPriority() || !bLowPriorityOnly ))
				{
					RemovePickupPlacement(pPlacement);
				}
			}
		}
	}
}

// Name			:	DetachAllPortablePickupsFromPed
// Purpose		:	Finds and detaches all portable pickups from the given ped
void CPickupManager::DetachAllPortablePickupsFromPed(CPed* pPed)
{
	s32 i = CPickup::GetPool()->GetSize();

	// remove pickups that have no corresponding placements
	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && pPickup->IsFlagSet(CPickup::PF_Portable) && pPickup->GetAttachParent() == pPed && !pPickup->IsNetworkClone())
		{
			pPickup->DetachPortablePickupFromPed("DetachAllPortablePickupsFromPed(2)");
		}
	}
}

// Name			:	CountPickupsOfType
// Purpose		:	Counts the number of placements (and pickups without a placement) with the given type
// Parameters	:	pickupHash - the pickup hash
// Returns		:   The count
s32 CPickupManager::CountPickupsOfType(u32 pickupHash)
{
	s32 count = 0;

	s32 i = CPickup::GetPool()->GetSize();

	// count pickups that have no corresponding placements
	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && !pPickup->GetPlacement() && pPickup->GetPickupHash() == pickupHash)
		{
			count++;
		}
	}

	i = CPickupPlacement::GetPool()->GetSize();

	// count placements
	while(i--)
	{
		CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

		if (pPlacement && pPlacement->GetPickupHash() == pickupHash)
		{
			count++;
		}
	}

	return count;
}

// Name			:	TestForPickupsInSphere
// Purpose		:	Tests to see if any pickups exist within the given sphere
// Parameters	:	SpherePos - the centre of the sphere
//					SphereRadius - the radius of the sphere
// Returns		:	True if pickups exist with the sphere
bool CPickupManager::TestForPickupsInSphere(const Vector3& spherePos, float sphereRadius)
{
	s32 i = CPickup::GetPool()->GetSize();

	float sphereRadiusSqr = sphereRadius*sphereRadius;

	// count pickups that have no corresponding placements
	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && !pPickup->GetPlacement() && (VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()) - spherePos).Mag2() < sphereRadiusSqr)
		{
			return true;
		}
	}

	i = CPickupPlacement::GetPool()->GetSize();

	// count placements
	while(i--)
	{
		CPickupPlacement* pPlacement = CPickupPlacement::GetPool()->GetSlot(i);

		if (pPlacement && (pPlacement->GetPickupPosition() - spherePos).Mag2() < sphereRadiusSqr)
		{
			return true;
		}
	}

	return false;
}

// Name			:	CalculateDroppedPickupPosition
// Purpose		:	Generates some coords suitable for placing a new pickup on
// Parameters	:	targetCoords - position that we ideally want the pickup to be placed
//					result - the closest position to the target coords that a pickup could be placed at
//					requestedDist - the furthest distance away from the target coords that we will consider for the result
// Returns		:   True if a valid position was found
bool CPickupManager::CalculateDroppedPickupPosition( CPickupPositionInfo &dat, bool bIgnoreInvalidePosWarning /*= false*/ )
{
	Vector3 PickUpCoors, Temp, Temp2;
	Vector3 localTargetCoords = dat.m_SrcPos;
	s32	i;
	bool	bOnGround, bOnGround2;

	float pickupHeightOffGround = dat.m_pickupHeightOffGround > 0.0f ? dat.m_pickupHeightOffGround : 0.25f;

	if (!dat.m_bOnGround)
	{
		// Snap the startcoors to the map.
		// 1.5 m up from us, downwards
		float newZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, localTargetCoords.x, localTargetCoords.y, localTargetCoords.z+1.5f, &bOnGround);
		// If it hits ground in less than 1.5m from our start pos
		if (bOnGround && rage::Abs(localTargetCoords.z-newZ) < 1.5f)
		{

			dat.m_bOnGround = true;
			// Now we do a second line scan down - just below our original ped pos.   
			float newZ2 = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, localTargetCoords.x, localTargetCoords.y, localTargetCoords.z - 0.05f, &bOnGround2);
			// This is to fix the rare cases where the pickup ends up on the floor above the dying ped.
			// If delta is closer than the original then original probably hit the ceiling above.  Obviously still fails if we are > halfway between the floors
			if (bOnGround2 && (rage::Abs(localTargetCoords.z-newZ2) < rage::Abs(localTargetCoords.z-newZ)))
			{
				localTargetCoords.z = newZ2;
			}
			else
			{
				// This is our new ground
				localTargetCoords.z = newZ;
			}
		}

		// Make sure the pickup above the ground. 
		localTargetCoords.z += pickupHeightOffGround;
	}

#define TRIES (16)	

	float radius = dat.m_MinDist;
	float RandomAngle = fwRandom::GetRandomNumberInRange(1.0f, 359.0f) * PI/180.0f;

	CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	Vector3 localPlayerCoords = pLocalPlayer ? VEC3V_TO_VECTOR3(pLocalPlayer->GetTransform().GetPosition()) : CGameWorld::FindLocalPlayerCoors();

	float minDistFromLocalPlayer = dat.m_MinDist*dat.m_MinDist*0.95f;

	if (!pLocalPlayer || pLocalPlayer->IsDead())
	{
		minDistFromLocalPlayer = 0.0f;
	}

	for (i = 0; i < TRIES; i++)
	{
		PickUpCoors = localTargetCoords;

		if ( radius > dat.m_MaxDist )
		{
			radius = dat.m_MinDist;
		}

		if (i!=0)
		{
			PickUpCoors.x += radius * rage::Sinf(RandomAngle);
			PickUpCoors.y += radius * rage::Cosf(RandomAngle);
		}

		// Set the z coordinate to be above the ground a bit
		PickUpCoors.z = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, PickUpCoors.x, PickUpCoors.y, PickUpCoors.z+1.0f, &bOnGround);
		if (bOnGround)
		{
			// We don't want to place pickups behind walls so:
			// Do a test to make sure the LOS between the ped and the pickup is free.
			Temp = localTargetCoords;
			Temp.z += 0.3f;
			// extend line a little bit
			Temp2 = PickUpCoors;
			Temp2.z += 0.3f;		// Do linetest a bit above ground or the collision test will always fail.
			Temp2 = Temp + (Temp2 - Temp) * (((Temp2 - Temp).Mag() + 0.3f) / (Temp2 - Temp).Mag());

			// Make sure these coordinates are reasonably far away from the player
			Vector3 vToLocalPlayer = PickUpCoors - localPlayerCoords;
			float DistToLocalPlayer = vToLocalPlayer.Mag2();

			if (DistToLocalPlayer > minDistFromLocalPlayer || i > (TRIES/2))
			{
				if (i > (TRIES/2) || !TestForPickupsInSphere(PickUpCoors, 1.3f))
				{
					// If the los from the ped to the potential pickup is clear we're laughing (test for cars for the first 16 go's)
					int flags=ArchetypeFlags::GTA_ALL_MAP_TYPES|ArchetypeFlags::GTA_MAP_TYPE_WEAPON;
					if(i < TRIES/2)
					{
						//						flags|=ArchetypeFlags::GTA_VEHICLE_TYPE;
						flags|=ArchetypeFlags::GTA_OBJECT_TYPE;
					}
					WorldProbe::CShapeTestProbeDesc probeDesc;
					probeDesc.SetStartAndEnd(Temp, Temp2);
					probeDesc.SetIncludeFlags(flags);
					probeDesc.SetContext(WorldProbe::LOS_Unspecified);
					if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
					{
						// Do an extra check for collisions with cars. These should be avoided. Use a capsule to detect if the pickup is being placed underneath a vehicle, and possibly inaccessible
						Vector3 capsuleStart = PickUpCoors;
						Vector3 capsuleEnd = PickUpCoors;
						float capsuleRadius = 0.5f;
						float capsuleHeight = 2.0f;

						capsuleStart.z -= capsuleRadius;
						capsuleEnd.z += capsuleHeight+capsuleRadius;

						WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
						capsuleDesc.SetCapsule(capsuleStart, capsuleEnd, capsuleRadius);
						capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_VEHICLE_TYPE);
						if(!WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
						{
							dat.m_Pos = PickUpCoors;
							dat.m_Pos.z += pickupHeightOffGround; // Make sure the pickup above the ground.
							dat.m_bOnGround = true;
							return true;
						}
					}
				}
			}
		}

		RandomAngle += (137.1f * PI/180.0f);
		RandomAngle = fwAngle::LimitRadianAngle(RandomAngle);

		// And expand us out a bit
		radius += 0.05f;
	}

	if(!bIgnoreInvalidePosWarning)
	{
		Assertf(0, "CPickupManager::CalculateDroppedPickupPosition - Failed to find valid pickup position:%f,%f,%f,MinDist:%f,MaxDist:%f,Ground:%i,PlayerPos:%f,%f\n",
			dat.m_SrcPos.x,dat.m_SrcPos.y,dat.m_SrcPos.z,
			dat.m_MinDist,dat.m_MaxDist,dat.m_bOnGround,
			CGameWorld::FindLocalPlayerCoors().x,CGameWorld::FindLocalPlayerCoors().y);
	}
	else
	{
		aiWarningf("CPickupManager::CalculateDroppedPickupPosition - Failed to find valid pickup position:%f,%f,%f,MinDist:%f,MaxDist:%f,Ground:%i,PlayerPos:%f,%f\n",
			dat.m_SrcPos.x,dat.m_SrcPos.y,dat.m_SrcPos.z,
			dat.m_MinDist,dat.m_MaxDist,dat.m_bOnGround,
			CGameWorld::FindLocalPlayerCoors().x,CGameWorld::FindLocalPlayerCoors().y);
	}

	dat.m_Pos = localTargetCoords;

	return false;
}

// Name			:	CreateSomeMoney
// Purpose		:	Generates a bunch of money pickups up to the given amount, around the given coordinates
// Parameters	:	PickUpCoors - the position the money has to be generated around
//					Amount - the total value of the money dropped
//					MaxNumPickups - the max number of cash pickups dropped
// Returns		:   Nothing
void CPickupManager::CreateSomeMoney(const Vector3& targetCoords, u32 Amount, u32 MaxNumPickups, u32 customModelIndex, CPed* pPedForVelocity)
{
#if __BANK
	if(ms_NoMoneyDrop)
	{
		return;
	}
#endif // __BANK
	s32	i;

	const int maxMoneyPickups = 8;

	Assertf(MaxNumPickups <= maxMoneyPickups, "CPickupManager::CreateSomeMoney - can only create a maximum of %d money pickups", maxMoneyPickups);

	int AmountPerPickup = Amount / MaxNumPickups;
	if (Amount % MaxNumPickups)
	{
		AmountPerPickup = 1 + (Amount / MaxNumPickups);
	}

	for (i = 0; i < maxMoneyPickups; i++)
	{			
		if (Amount == 0)
			break;

		bool createPickup = false;
		Matrix34 pickupM;
		pickupM.Identity();

		if(pPedForVelocity)
		{
			pPedForVelocity->GetBonePosition(pickupM.d,BONETAG_SPINE3);
			// Add in some extra offset to avoid phLevel duplicate placement asserts. 
			float extraOffset = ((float)i)*0.001f;
			pickupM.d += Vector3(0.0f, extraOffset, 0.05f);
			createPickup = true;
		}
		else
		{
			// The money pickup originates from inside the ped 
			CPickupPositionInfo pos( targetCoords );
			pos.m_MinDist = pPedForVelocity ? 0.1f : 1.5f;
			pos.m_MaxDist = pPedForVelocity ? 0.15f : 1.55f;
			CalculateDroppedPickupPosition( pos, true );

			if(pos.m_bOnGround)
			{
				pickupM.d = pos.m_Pos;
				pickupM.d.z += 0.05f;
				createPickup = true;
			}
		}

		if (createPickup)
		{
			CPickup* pPickup = CreatePickup(PICKUP_MONEY_VARIABLE, pickupM, NULL, false, customModelIndex);

			if (pPickup)
			{
				int AmountThisPickup = MIN(AmountPerPickup, Amount);

				pPickup->SetAmount(AmountThisPickup);

				if (Amount > AmountThisPickup)
					Amount -= AmountThisPickup;
				else 
					Amount = 0;

				bool bUseInitVelocities = false;
				if(pPedForVelocity && pPedForVelocity->GetIsVisibleInSomeViewportThisFrame())
				{
					const ScalarV rotationAngle = ScalarVFromF32(fwRandom::GetRandomNumberInRange(0.0f,2.0f*PI));
					const Vec3V extraVelocityDir = RotateAboutZAxis(Vec3V(V_X_AXIS_WZERO),rotationAngle);
					TUNE_GROUP_FLOAT(PICKUP,PED_CASH_HORIZONTAL_VEL,3.5f,0.0f,10.0f,1.0f);
					TUNE_GROUP_FLOAT(PICKUP,PED_CASH_VERTICAL_VEL, -3.5f,-10.0f,10.0f,1.0f);
					const Vec3V extraVelocity = Add(Scale(ScalarVFromF32(PED_CASH_VERTICAL_VEL),Vec3V(V_Z_AXIS_WZERO)), Scale(ScalarVFromF32(PED_CASH_HORIZONTAL_VEL),extraVelocityDir));
					bUseInitVelocities = SetStartingVelocities(pPickup, pPedForVelocity, RAGDOLL_SPINE0, RAGDOLL_SPINE3, VEC3V_TO_VECTOR3(extraVelocity));
					pPickup->SetStartingAngularVelocity(VEC3_ZERO);

					// Don't collide with ped until they don't touch each.
					pPickup->SetNoCollision(pPedForVelocity, NO_COLLISION_RESET_WHEN_NO_IMPACTS);
				}

				if(!bUseInitVelocities)
				{
					pPickup->SetPlaceOnGround();
				}

				if (pPedForVelocity && !pPedForVelocity->IsAPlayerPed() && (pPedForVelocity->PopTypeIsMission() || (pPedForVelocity->GetNetworkObject() && pPedForVelocity->GetNetworkObject()->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_WAS_SCRIPTOBJECT))))
				{
					pPickup->SetDroppedByPed();

					// use extended scope for money pickups dropped by dead script peds, so they are not cleaned up immediately when killed via a sniper rifle
					pPickup->SetFlag(CPickup::PF_UseExtendedScope);
				}

				physicsDebugf1("CPickupManager::CreateSomeMoney - Pos(%f,%f,%f), Vel(%f,%f,%f), AVel(%f,%f,%f)", VEC3V_ARGS(RCC_VEC3V(pickupM.d)), VEC3V_ARGS(RCC_VEC3V(pPickup->m_StartingLinearVelocity)), VEC3V_ARGS(RCC_VEC3V(pPickup->m_StartingAngularVelocity)));
			}
			else
			{
				break;
			}
		}
	}
}

// Name			:	GetCustomArchetype
// Purpose		:	Returns the stored custom archetype (the archetype containing an extra sphere bound used to detect collection) for the given pickup.
// Parameters	:	pickup - the pickup
// Returns		:   The custom archetype
phArchetypeDamp* CPickupManager::GetCustomArchetype(const CPickup& pickup)
{
	if (pickup.IsFlagSet(CPickup::PF_HasCustomModel))
	{
		for (u32 i=0; i<MAX_EXTRA_CUSTOM_ARCHETYPES; i++)
		{
			if (ms_extraCustomArchetypes[i].IsSet())
			{
				if (ms_extraCustomArchetypes[i].GetModelIndex().Get() == (s32)pickup.GetModelIndex())
				{
					return ms_extraCustomArchetypes[i].GetArchetype();
				}
			}
		}
	}
	else
	{
		CPickupTypeInfo* pTypeInfo = ms_pickupTypeInfoMap.Access(pickup.GetPickupHash());

		if (AssertVerify(pTypeInfo) && pTypeInfo->m_customArchetype.IsSet())
		{
			return pTypeInfo->m_customArchetype.GetArchetype();
		}
	}

	return NULL;
}

// Name			:	StoreCustomArchetype
// Purpose		:	Stores the custom archetype (the archetype containing an extra sphere bound used to detect collection) for the given pickup type.
// Parameters	:	pickup - the pickup 
//					pArchetype - the custom archetype
// Returns		:   Nothing
void CPickupManager::StoreCustomArchetype(CPickup& pickup, phArchetypeDamp* pArchetype)
{
	if (pickup.IsFlagSet(CPickup::PF_HasCustomModel))
	{
		for (u32 i=0; i<MAX_EXTRA_CUSTOM_ARCHETYPES; i++)
		{
			if (!ms_extraCustomArchetypes[i].IsSet())
			{
				ms_extraCustomArchetypes[i].Set(*pArchetype, pickup.GetModelIndex());
				return;
			}
		}

		Assertf(0, "CPickupManager: Ran out of extra custom archetypes for custom pickup models");
	}
	else
	{
		CPickupTypeInfo* pTypeInfo = ms_pickupTypeInfoMap.Access(pickup.GetPickupHash());

		if (AssertVerify(pTypeInfo))
		{
			pTypeInfo->m_customArchetype.Set(*pArchetype, pickup.GetModelIndex());
		}
	}
}

void CPickupManager::AddRefForExtraCustomArchetype(phArchetypeDamp* pArchetype)
{
	for (u32 i=0; i<MAX_EXTRA_CUSTOM_ARCHETYPES; i++)
	{
		CCustomArchetypeInfo* pInfo = &ms_extraCustomArchetypes[i];

		if (pInfo->IsSet() && pInfo->GetArchetype() == pArchetype)
		{
			pInfo->AddRef();
			return;
		}
	}

	Assertf(0, "CPickupManager::AddRefForExtraCustomArchetype - Archetype not found!!");
}

void CPickupManager::RemoveRefForExtraCustomArchetype(u32 modelIndex)
{
	for (u32 i=0; i<MAX_EXTRA_CUSTOM_ARCHETYPES; i++)
	{
		CCustomArchetypeInfo* pInfo = &ms_extraCustomArchetypes[i];

		if (pInfo->IsSet() && pInfo->GetModelIndex().Get() == (s32)modelIndex)
		{
			pInfo->RemoveRef();
			return;
		}
	}

#if __ASSERT
	for (u32 i=0; i<MAX_EXTRA_CUSTOM_ARCHETYPES; i++)
	{
		CCustomArchetypeInfo* pInfo = &ms_extraCustomArchetypes[i];

		if(pInfo->IsSet())
		{
			Displayf("ms_extraCustomArchetypes[%d]: Model Index: %d", i, pInfo->GetModelIndex().Get());
		}

		if (pInfo->GetModelIndex().Get() == (s32)modelIndex)
		{
			if(!pInfo->IsSet())
			{
				Warningf("ms_extraCustomArchetypes[%d]: Pickup archetype has been deleted", i);
			}
		}
	}

	Assertf(0, "CPickupManager::RemoveRefForExtraCustomArchetype - Archetype not found!! Model index: %d", modelIndex);
#endif // __ASSERT
}

bool CPickupManager::IsSuppressionFlagSet( const s32 iFlag )
{ 
	if( iFlag == PICKUP_REWARD_TYPE_NONE )
		return ms_SuppressionFlags.GetAllFlags() == 0;

	return ms_SuppressionFlags.IsFlagSet( iFlag ); 
}


// prevents a certain player from collecting a certain type of pickup 
void CPickupManager::ProhibitCollectionForPlayer(u32 pickupHash, PhysicalPlayerIndex playerIndex)
{
	CPickupTypeInfo* pTypeInfo = ms_pickupTypeInfoMap.Access(pickupHash);

	if (AssertVerify(pTypeInfo) && AssertVerify(playerIndex != INVALID_PLAYER_INDEX))
	{
		pTypeInfo->m_prohibitedCollections |= (1<<playerIndex);

#if ENABLE_NETWORK_LOGGING
		if (NetworkInterface::IsGameInProgress())
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("pTypeInfo", "0x%p", pTypeInfo);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("m_prohibitedCollections", "%d", pTypeInfo->m_prohibitedCollections);
		}
#endif
	}
	else
	{
#if ENABLE_NETWORK_LOGGING
		if (NetworkInterface::IsGameInProgress())
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Failed", "true");
		}
#endif
	}
}

// allows collection of a certain type of pickup for the given player
void CPickupManager::AllowCollectionForPlayer(u32 pickupHash, PhysicalPlayerIndex playerIndex)
{
	CPickupTypeInfo* pTypeInfo = ms_pickupTypeInfoMap.Access(pickupHash);

	if (AssertVerify(pTypeInfo) && AssertVerify(playerIndex != INVALID_PLAYER_INDEX))
	{
		pTypeInfo->m_prohibitedCollections &= ~(1<<playerIndex);

#if ENABLE_NETWORK_LOGGING
		if (NetworkInterface::IsGameInProgress())
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("pTypeInfo", "0x%p", pTypeInfo);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("m_prohibitedCollections", "%d", pTypeInfo->m_prohibitedCollections);
		}
#endif
	}
	else
	{
#if ENABLE_NETWORK_LOGGING
		if (NetworkInterface::IsGameInProgress())
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Failed", "true");
		}
#endif
	}
}

// returns true if the given player is prohibited from collecting a certain type of pickup
bool CPickupManager::IsCollectionProhibited(u32 pickupHash, PhysicalPlayerIndex playerIndex)
{
	bool bProhibited = false;

	CPickupTypeInfo* pTypeInfo = ms_pickupTypeInfoMap.Access(pickupHash);

	if (AssertVerify(pTypeInfo) && AssertVerify(playerIndex != INVALID_PLAYER_INDEX))
	{
		bProhibited = (pTypeInfo->m_prohibitedCollections & (1<<playerIndex)) != 0;
	}

	return bProhibited;
}

// removes all prohibited collection for the given pickup type
void CPickupManager::RemoveProhibitedCollection(u32 pickupHash)
{
	CPickupTypeInfo* pTypeInfo = ms_pickupTypeInfoMap.Access(pickupHash);

	if (AssertVerify(pTypeInfo))
	{
		pTypeInfo->m_prohibitedCollections = 0;
	}
}

// adds the given player to all prohibited collections for all pickup types
void CPickupManager::AddPlayerToAllProhibitedCollections(PhysicalPlayerIndex playerIndex)
{
	PickupTypeInfoMap::Iterator entry = ms_pickupTypeInfoMap.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		entry.GetData().m_prohibitedCollections |= (1<<playerIndex);
	}
}

// removes the given player from all prohibited collections for all pickup types
void CPickupManager::RemovePlayerFromAllProhibitedCollections(PhysicalPlayerIndex playerIndex)
{
	PickupTypeInfoMap::Iterator entry = ms_pickupTypeInfoMap.CreateIterator();

	for (entry.Start(); !entry.AtEnd(); entry.Next())
	{
		entry.GetData().m_prohibitedCollections &= ~(1<<playerIndex);
	}
}

// prevents the local player from collecting pickups with the given model  
void CPickupManager::ProhibitPickupModelCollectionForLocalPlayer(u32 modelHash, const char* scriptName)
{
	int freeSlot = -1;

	if (!Verifyf(modelHash != 0, "CPickupManager::ProhibitModelCollectionForLocalPlayer - modelHash is 0"))
	{
		return;
	}

	u32 numProhibited = 0;

	if (ms_hasProhibitedPickupModels)
	{
		for (u32 i=0; i<MAX_PROHIBITED_PICKUP_MODELS; i++)
		{
			if (ms_prohibitedPickupModels[i] == modelHash)
			{
				return;
			}
			else if (ms_prohibitedPickupModels[i] == 0)
			{
				if (freeSlot == -1)
				{
					freeSlot = i;
				}
			}
			else
			{
				numProhibited++;
			}
		}
	}
	else
	{
		freeSlot = 0;
	}

	if (freeSlot != -1)
	{
		if (NetworkInterface::IsGameInProgress())
		{
			NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "ADD_PICKUP_MODEL_PROHIBIT", scriptName);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Model", "%d", modelHash);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Slot", "%d", freeSlot);
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Curr num prohibited", "%d", numProhibited);
		}

		Assert(ms_prohibitedPickupModels[freeSlot] == 0);

		ms_prohibitedPickupModels[freeSlot] = modelHash;

		ms_hasProhibitedPickupModels = true;
	}
	else
	{
		Assertf(0, "CPickupManager::ProhibitModelCollectionForLocalPlayer - can't prohibit this model, too many prohibited models");
	}
}

// returns true if the local player is prohibited from collecting pickups with the given model
bool CPickupManager::IsPickupModelCollectionProhibited(u32 modelHash)
{
	if (ms_hasProhibitedPickupModels)
	{
		for (u32 i=0; i<MAX_PROHIBITED_PICKUP_MODELS; i++)
		{
			if (ms_prohibitedPickupModels[i] == modelHash)
			{
				return true;
			}
		}
	}

	return false;
}

// removes all prohibited collection for the given pickup model
void CPickupManager::RemoveProhibitedPickupModelCollection(u32 modelHash, const char* scriptName)
{
	if (ms_hasProhibitedPickupModels)
	{
		u32 numProhibited = 0;

		for (u32 i=0; i<MAX_PROHIBITED_PICKUP_MODELS; i++)
		{
			if (ms_prohibitedPickupModels[i] == modelHash)
			{
				ms_prohibitedPickupModels[i] = 0;

				if (NetworkInterface::IsGameInProgress())
				{
					NetworkLogUtils::WriteLogEvent(NetworkInterface::GetObjectManager().GetLog(), "REMOVE_PICKUP_MODEL_PROHIBIT", scriptName);
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Model", "%d", modelHash);
					NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Slot", "%d", i);
				}

				numProhibited++;
			}
			else if (ms_prohibitedPickupModels[i] != 0)
			{
				numProhibited++;
			}
		}

		if (numProhibited==0)
		{
			ms_hasProhibitedPickupModels = false;
		}

		if (NetworkInterface::IsGameInProgress())
		{
			NetworkInterface::GetObjectManager().GetLog().WriteDataValue("Curr num prohibited", "%d", numProhibited);
		}
	}
}

#if __BANK
void CPickupManager::DebugScriptedGlow()
{
	if( ms_debugScriptedPickups )
	{
		CPed * playerPed = FindPlayerPed();

		if(playerPed)
		{
			float segment = (PI * 2.0f) / (float)ms_glowList.m_data.GetCount();

			for (u32 i=0; i<ms_glowList.m_data.GetCount();i++)
			{
				Vec3V pickupPosition = playerPed->GetTransform().GetPosition();

				if (ms_createAllPickupsOnGround)
					pickupPosition.SetZf(playerPed->GetGroundPos().z);
				
				pickupPosition += Vec3V(5.0f * rage::Sinf(i*segment),5.0f * rage::Cosf(i*segment),0.0f);

				RenderScriptedGlow(pickupPosition,i);
			}
		}
	}
}

void CPickupManager::DebugDisplay()
{
	const float VISUALISE_RANGE	= 30.0f;

	if (ms_displayAllPickupPlacements)
	{
		Vector3 camPos = camInterface::GetPos();

		for (s32 i=0; i<CPickupPlacement::GetPool()->GetSize(); i++)
		{
			CPickupPlacement* pPickupPlacement = CPickupPlacement::GetPool()->GetSlot(i);

			if (pPickupPlacement)
			{
				Vector3 vDiff = camPos - pPickupPlacement->GetPickupPosition();

				float fDist = vDiff.Mag();

				if(fDist >= VISUALISE_RANGE)
					continue;

				char debugStr[100];
				int line = -1;

				sprintf(debugStr, "%s", pPickupPlacement->GetPickupData()->GetName());
				grcDebugDraw::Text( pPickupPlacement->GetPickupPosition(), Color_red, 0, (line++)*grcDebugDraw::GetScreenSpaceTextHeight(), debugStr);

				sprintf(debugStr, "Has pickup: %s", pPickupPlacement->GetPickup() ? "Yes" : "No");
				grcDebugDraw::Text( pPickupPlacement->GetPickupPosition(), Color_red, 0, (line++)*grcDebugDraw::GetScreenSpaceTextHeight(), debugStr);

				sprintf(debugStr, "Collected: %s", pPickupPlacement->GetIsCollected() ? "Yes" : "No");
				grcDebugDraw::Text( pPickupPlacement->GetPickupPosition(), Color_red, 0, (line++)*grcDebugDraw::GetScreenSpaceTextHeight(), debugStr);

				sprintf(debugStr, "In scope: %s", pPickupPlacement->GetInScope() ? "Yes" : "No");
				grcDebugDraw::Text( pPickupPlacement->GetPickupPosition(), Color_red, 0, (line++)*grcDebugDraw::GetScreenSpaceTextHeight(), debugStr);
			}
		}
	}
}

void CPickupManager::PickupsBank_CreatePickup()
{
	CPed * playerPed = FindPlayerPed();

	if(playerPed)
	{
		const CPickupData* pData = CPickupDataManager::GetPickupDataInSlot(ms_pickupComboIndex);

		if (Verifyf(pData, "Pickup metadata does not exist"))
		{
			u32 modelIndex = pData->GetModelIndex();
			fwModelId modelId((strLocalIndex(modelIndex)));
			while (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				strStreamingEngine::GetLoader().LoadAllRequestedObjects(true);
			}

			Matrix34 createM;
			createM.Identity();
			createM.d = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(playerPed->GetTransform().GetB()) * 2.0f);

			CPickup* pPickup = CreatePickup(pData->GetHash(), createM);

			if (pPickup)
			{
				if (playerPed->GetIsInInterior() && !pPickup->GetRequiresACustomArchetype())
				{
					CGameWorld::Remove(pPickup);
					fwInteriorLocation	interiorLocation = CInteriorInst::CreateLocation( playerPed->GetPortalTracker()->GetInteriorInst(), playerPed->GetPortalTracker()->m_roomIdx );
					CGameWorld::Add(pPickup, interiorLocation);
				}

				if (pData->GetAttachmentBone() != 0)
				{
					pPickup->SetPortable();
				}

				if (ms_createAllPickupsOnGround)
				{
					pPickup->SetPlaceOnGround(true, ms_createAllPickupsUpright);
				}

				if (ms_createAllPickupsWithArrowMarkers)
				{
					pPickup->SetFlag(CPickup::PF_AddArrowMarker);
				}

				pPickup->SetDebugCreated();
			}
		}
	}
}

void CPickupManager::PickupsBank_CreatePickupPlacement()
{
	CPed * playerPed = FindPlayerPed();

	if(playerPed)
	{
		const CPickupData* pData = CPickupDataManager::GetPickupDataInSlot(ms_pickupComboIndex);

		if (Verifyf(pData, "Pickup metadata does not exist"))
		{
			u32 modelIndex = pData->GetModelIndex();

			fwModelId modelId((strLocalIndex(modelIndex)));
			while (!CModelInfo::HaveAssetsLoaded(modelId))
			{
				CModelInfo::RequestAssets(modelId, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
				strStreamingEngine::GetLoader().LoadAllRequestedObjects(true);
			}

			Vector3 pickupPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition()) + (VEC3V_TO_VECTOR3(playerPed->GetTransform().GetB()) * 2.0f);
			Vector3 pickupOrientation = Vector3(1.0f, 1.0f, 1.0f);
			PlacementFlags flags = 0;

			if (ms_createAllPickupsOnGround)
				pickupPosition.z = playerPed->GetGroundPos().z;


			if (ms_fixedPickups)
				flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_FIXED;
			if (ms_regeneratePickups)
				flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_REGENERATES;
			if (ms_blipPickups)
				flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_BLIPPED_SIMPLE;
			if (ms_createAllPickupsOnGround)
				flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_SNAP_TO_GROUND | CPickupPlacement::PLACEMENT_CREATION_FLAG_ORIENT_TO_GROUND;
			if (ms_createAllPickupsUpright)
				flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_UPRIGHT;

			CPickupPlacement* pPlacement = RegisterPickupPlacement(pData->GetHash(), pickupPosition, pickupOrientation, flags); 

			if (playerPed->GetIsInInterior() && AssertVerify(pPlacement))
			{
				pPlacement->SetRoomHash(playerPed->GetPortalTracker()->GetInteriorInst()->GetRoomHashcode(playerPed->GetPortalTracker()->m_roomIdx));
			}

			pPlacement->SetDebugCreated();
		}
	}
}

void CPickupManager::PickupsBank_CreateAllPickups()
{
	CPed * playerPed = FindPlayerPed();

	if(playerPed)
	{
		float segment = (PI * 2.0f) / CPickupDataManager::GetNumPickupTypes();

		for (u32 i=0; i<CPickupDataManager::GetNumPickupTypes(); i++)
		{
			const CPickupData* pData = CPickupDataManager::GetPickupDataInSlot(ms_pickupComboIndex);

			if (Verifyf(pData, "Pickup metadata does not exist"))
			{
				Vector3 pickupPosition = VEC3V_TO_VECTOR3(playerPed->GetTransform().GetPosition());

				if (ms_createAllPickupsOnGround)
					pickupPosition.z = playerPed->GetGroundPos().z;
				
				Vector3 pickupOrientation = Vector3(1.0f, 1.0f, 1.0f);

				pickupPosition.x += 5.0f * rage::Sinf(i*segment);
				pickupPosition.y += 5.0f * rage::Cosf(i*segment);

				PlacementFlags flags = 0;

				if (ms_fixedPickups)
					flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_FIXED;
				if (ms_regeneratePickups)
					flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_REGENERATES;
				if (ms_blipPickups)
					flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_BLIPPED_SIMPLE;
				if (ms_createAllPickupsOnGround)
					flags |= CPickupPlacement::PLACEMENT_CREATION_FLAG_SNAP_TO_GROUND | CPickupPlacement::PLACEMENT_CREATION_FLAG_ORIENT_TO_GROUND;

				RegisterPickupPlacement(pData->GetHash(), pickupPosition, pickupOrientation, flags); 
			}
		}
	}
}

void CPickupManager::PickupsBank_RemoveAllPickups()
{
	RemoveAllPickups(true);
}

void CPickupManager::PickupsBank_DetachAllPickups()
{
	s32 i = CPickup::GetPool()->GetSize();

	while(i--)
	{
		CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

		if (pPickup && pPickup->IsFlagSet(CPickup::PF_Portable) && pPickup->GetAttachParent() == FindPlayerPed())
		{
			pPickup->DetachPortablePickupFromPed("PickupsBank_DetachAllPickups");
		}
	}
}

void CPickupManager::PickupsBank_ReloadPickupData()
{
	CPickupDataManager::Shutdown(SHUTDOWN_SESSION);
	CPickupDataManager::Init(INIT_SESSION);
}

void CPickupManager::PickupsBank_RecalculatePickupsInScope()
{
	ms_cachedPlayerPosition = Vector3(0.0f, 0.0f, 0.0f);
}

void CPickupManager::PickupsBank_SetPlayerCanCarryPortablePickups()
{
	static s8 maxPickupsCarried = 0;

	if (ms_allowPlayerToCarryPortablePickups)
	{
		CGameWorld::GetMainPlayerInfo()->m_maxPortablePickupsCarried = maxPickupsCarried;
	}
	else
	{
		maxPickupsCarried = CGameWorld::GetMainPlayerInfo()->m_maxPortablePickupsCarried;
		CGameWorld::GetMainPlayerInfo()->m_maxPortablePickupsCarried = 0;
	}
}

void CPickupManager::PickupsBank_MakePickupFromPlayerWeapon()
{
	CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

	int pedId = CTheScripts::GetGUIDFromEntity(*pPlayerPed);

	static Vector3 offset(0.0f, 0.5f, 0.0f);

	weapon_commands::CommandPedDropsInventoryWeapon(pedId, WEAPONTYPE_GOLFCLUB, offset, 3);
}

void CPickupManager::InitLevelWidgets()
{
	if (AssertVerify(!ms_pBank))
	{
		ms_pBank = &BANKMGR.CreateBank("Pickups", 0, 0, false);

		ms_pPickupNamesCombo = ms_pBank->AddCombo ("Pickup", &ms_pickupComboIndex, ms_numPickupComboNames, ms_pickupComboNames);

		ms_pBank->AddToggle("Fix", &ms_fixedPickups);
		ms_pBank->AddToggle("Regenerate", &ms_regeneratePickups);
		ms_pBank->AddToggle("Blip", &ms_blipPickups);
		ms_pBank->AddToggle("Force MP Style Weapon Pickups", &ms_forceMPStyleWeaponPickups);
		ms_pBank->AddToggle("Snap to ground", &ms_createAllPickupsOnGround);
		ms_pBank->AddToggle("Add Arrow Marker", &ms_createAllPickupsWithArrowMarkers);
		ms_pBank->AddToggle("Upright", &ms_createAllPickupsUpright);
		ms_pBank->AddToggle("Allow player to carry portable pickups", &ms_allowPlayerToCarryPortablePickups, datCallback(PickupsBank_SetPlayerCanCarryPortablePickups));
		ms_pBank->AddToggle("No Money Drop", &ms_NoMoneyDrop);
		ms_pBank->AddButton("Create pickup", datCallback(PickupsBank_CreatePickup));
		ms_pBank->AddButton("Create pickup placement", datCallback(PickupsBank_CreatePickupPlacement));
		ms_pBank->AddToggle("Display all pickup placements", &ms_displayAllPickupPlacements);
		ms_pBank->AddButton("Create all pickups", datCallback(PickupsBank_CreateAllPickups));
		ms_pBank->AddButton("Remove all pickups", datCallback(PickupsBank_RemoveAllPickups));
		ms_pBank->AddButton("Detach all pickups from player", datCallback(PickupsBank_DetachAllPickups));
		ms_pBank->AddToggle("Render all scripted glow", &ms_debugScriptedPickups);
		ms_pBank->AddButton("Make pickup from players equipped weapon", datCallback(PickupsBank_MakePickupFromPlayerWeapon));
#if ENABLE_GLOWS
		ms_pBank->AddToggle("Always Render pickup glows", &ms_forcePickupGlow);
		ms_pBank->AddToggle("Always Offset Glows", &ms_forceOffsetPickupGlow);
#endif // ENABLE_GLOWS	
#if __DEV
		ms_pBank->AddSlider("Arrow Marker - Z Offset", &ARROW_MARKER_Z_OFFSET, 0.0f, 5.0f, 0.01f);
		ms_pBank->AddSlider("Arrow Marker - Scale", &ARROW_MARKER_SCALE, 0.0f, 5.0f, 0.01f);
		ms_pBank->AddColor("Arrow Marker - Colour", &ARROW_MARKER_COL);
		ms_pBank->AddSlider("Arrow Marker - Fade Start Range", &ARROW_MARKER_FADE_START_RANGE, 0.0f, 50.0f, 0.1f);
		ms_pBank->AddSlider("Arrow Marker - Fade Out Range", &ARROW_MARKER_FADE_OUT_RANGE, 0.0f, 50.0f, 0.1f);
		ms_pBank->AddToggle("Arrow Marker - Bounce", &ARROW_MARKER_BOUNCE);
		ms_pBank->AddToggle("Arrow Marker - Face Cam", &ARROW_MARKER_FACE_CAM);
#endif
		ms_pBank->AddButton("Reload pickup data", datCallback(PickupsBank_ReloadPickupData));

#if __BANK && ENABLE_GLOWS
		ms_pBank->PushGroup("Pickup Glow", false);
		ms_pBank->AddSlider("Glow Red", &CPickup::ms_customGlowR, 0.0f, 1.0f, 0.05f, NullCB);
		ms_pBank->AddSlider("Glow Green", &CPickup::ms_customGlowG, 0.0f, 1.0f, 0.05f, NullCB);
		ms_pBank->AddSlider("Glow Blue", &CPickup::ms_customGlowB, 0.0f, 1.0f, 0.05f, NullCB);
		ms_pBank->AddSlider("Glow Intensity", &CPickup::ms_customGlowI, 0.0f, 50.0f, 1.0f, NullCB);
		ms_pBank->AddSlider("Glow Range", &CPickup::ms_customGlowRange, 0.0f, 5.0f, 0.1f, NullCB);
		ms_pBank->AddSlider("Glow Fade Distance", &CPickup::ms_customGlowFadeDist, 0.0f, 50.0f, 1.0f, NullCB);
		ms_pBank->AddSlider("Glow Offsert", &CPickup::ms_customGlowOffset, -3.0f, 3.0f, 0.1f, NullCB);
		ms_pBank->PopGroup();
#endif // __BANK && ENABLE_GLOWS
		
		GlowListAddWidgets(ms_pBank);		
	}
}

void CPickupManager::ShutdownLevelWidgets()
{
	if (ms_pBank)
	{
		BANKMGR.DestroyBank(*ms_pBank);
		ms_pBank = NULL;
		ms_pPickupNamesCombo = NULL;
	}
}

#endif

// Name			:	AddPlacementToRegenerationList
// Purpose		:	Adds a collected placement to the regeneration list
CPickupManager::CRegenerationInfo* CPickupManager::AddPlacementToRegenerationList(CPickupPlacement *pPlacement)
{
	Assertf(CRegenerationInfo::GetPool()->GetNoOfFreeSpaces() > 0, "CPickupManager: Ran out of regeneration infos");

	CRegenerationInfo* pNewInfo = NULL;

#if __ASSERT
	// sanity check that it is not already on the list
	if (IsPlacementOnRegenerationList(pPlacement, &pNewInfo))
	{
		Assertf(0, "CPickupManager::AddPlacementToRegenerationList: placement is already on the list");
		return pNewInfo;
	}
#endif // __ASSERT

	if (CRegenerationInfo::GetPool()->GetNoOfFreeSpaces() > 0)
	{
		pNewInfo = rage_new CRegenerationInfo(pPlacement);
		ms_regeneratingPlacements.Add(pNewInfo);
	}
	else
	{
		Assertf(0, "Ran out of CRegenerationInfos in CPickupManager");
	}

	return pNewInfo;
}

// Name			:	RemovePlacementFromRegenerationList
// Purpose		:	Removes a placement from the regeneration list
void CPickupManager::RemovePlacementFromRegenerationList(CPickupPlacement *pPlacement, bool ASSERT_ONLY(bAssert))
{
	fwPtrNode* pNode = ms_regeneratingPlacements.GetHeadPtr();

	while (pNode)
	{
		CRegenerationInfo* pRegenInfo = static_cast<CRegenerationInfo*>(pNode->GetPtr());
		fwPtrNode* pNextNode = pNode->GetNextPtr();

		if (pRegenInfo->GetPlacement() == pPlacement)
		{
			ms_regeneratingPlacements.Remove(pRegenInfo);

			delete pRegenInfo;

			return;
		}

		pNode = pNextNode;
	}

	Assertf(!bAssert || pNode, "CPickupManager::RemovePlacementToRegenerationList: placement is not on the list");
}

// Name			:	ForcePlacementFromRegenerationListToRegenerate
// Purpose		:	Sets flag on CRegenerationInfo to force regenerate pickup even if time left on timer
void CPickupManager::ForcePlacementFromRegenerationListToRegenerate(CPickupPlacement& pPlacement)
{
	fwPtrNode* pNode = ms_regeneratingPlacements.GetHeadPtr();

	while (pNode)
	{
		CRegenerationInfo* pRegenInfo = static_cast<CRegenerationInfo*>(pNode->GetPtr());
		fwPtrNode* pNextNode = pNode->GetNextPtr();

		if (pRegenInfo->GetPlacement() == &pPlacement)
		{
			pRegenInfo->SetForceRegenerate();
			return;
		}
		pNode = pNextNode;
	}

	Assertf(0, "Can't find pickup placement(ID=%d)(NetID=%s) in regenerating placements list.", pPlacement.GetScriptInfo() ? pPlacement.GetScriptInfo()->GetObjectId() : -1, pPlacement.GetNetworkObject() ? pPlacement.GetNetworkObject()->GetLogName() : "N/A");
}

// Name			:	IsPlacementOnRegenerationList
// Purpose		:	Returns true if the placement is on the regeneration list
bool CPickupManager::IsPlacementOnRegenerationList(CPickupPlacement *pPlacement, CRegenerationInfo** pInfo)
{
	fwPtrNode* pNode = ms_regeneratingPlacements.GetHeadPtr();

	while (pNode)
	{
		CRegenerationInfo* pRegenInfo = static_cast<CRegenerationInfo*>(pNode->GetPtr());

		if (pRegenInfo->GetPlacement() == pPlacement)
		{
			if (pInfo)
			{
				*pInfo = pRegenInfo;
			}
			return true;
		}

		pNode = pNode->GetNextPtr();
	}

	return false;
}

void CPickupManager::RemovePickupPlacementFromCollectionHistory(CPickupPlacement *pPlacement)
{
	// remove from the collection history 
	if (pPlacement && pPlacement->GetScriptInfo())
	{
		int index;
		CCollectionInfo info(*pPlacement->GetScriptInfo());

		if (ms_collectionHistory.Find(info, &index))
		{
			ms_collectionHistory.Delete(index);
		}
	}
}


// Name			:	DestroyPickupPlacement
// Purpose		:	Destroys a placement 
void CPickupManager::DestroyPickupPlacement(CPickupPlacement *pPlacement)
{
	if (ms_removedPlacements.IsMemberOfList(pPlacement))
	{
		ms_removedPlacements.Remove(pPlacement);
	}

	if (ms_pendingPickups.IsMemberOfList(pPlacement))
	{
		ms_pendingPickups.Remove(pPlacement);
	}

	RemovePlacementFromRegenerationList(pPlacement, false);

	ms_pQuadTree->DeleteItem(pPlacement);

	delete pPlacement;
}

// Name			:	ProcessInactiveState
// Purpose		:	Called when the pickup manager is inactive and no pickups should exist
void CPickupManager::ProcessInactiveState()
{
	if(CutSceneManager::GetInstance() && !(CutSceneManager::GetInstance()->GetOptionFlags().IsFlagSet(CUTSCENE_DO_NOT_CLEAR_PICKUPS)))
	{
		REPLAY_ONLY(if(CReplayMgr::IsOkayToDelete()))
			RemoveAllPickups(false);

		// flush pending list
		ms_pendingPickups.Flush();
	}

	// uninitialized script pickups still need to be processed in MP, so that they can get created on other machines. Otherwise the script
	// that created them will get stuck waiting for them.
	if(CPickup::GetPool()->GetNoOfUsedSpaces() > 0)
	{
		for (s32 i=0; i<CPickup::GetPool()->GetSize(); i++)
		{
			CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

			if (pPickup && 
				!pPickup->IsFlagSet(CPickup::PF_Initialised) && 
				pPickup->GetNetworkObject() && 
				pPickup->GetNetworkObject()->IsScriptObject()) 
			{
				pPickup->Update();
			}
		}
	}

	// reset cached player pos so pickup scope is immediately recalculated when manager becomes active again
	ms_cachedPlayerPosition = Vector3(0.0f, 0.0f, 0.0f);
}

// Name			:	ProcessPickupsInScope
// Purpose		:	Determines which pickups are in scope and adds them to the pending list
void CPickupManager::ProcessPickupsInScope()
{
	if (AssertVerify(FindPlayerPed()))
	{
		Vector3 playerPosition;
		
		playerPosition = VEC3V_TO_VECTOR3(FindPlayerPed()->GetTransform().GetPosition());

		if (NetworkInterface::IsGameInProgress())
		{
			CNetGamePlayer* localPlayer = NetworkInterface::GetLocalPlayer();

			if (localPlayer)
			{
				playerPosition = NetworkInterface::GetPlayerFocusPosition(*localPlayer);
			}
		}
	
		// do we need to recalculate the current pickup placements in scope with the player?
		Vector3 playerDist = playerPosition - ms_cachedPlayerPosition;
		float scope = static_cast<float>(PLACEMENT_SCOPE_RANGE*PLACEMENT_SCOPE_RANGE);

		if (ms_cachedPlayerPosition.Mag2() == 0.0f || playerDist.Mag2() >= scope)
		{
			// recalculate current pickup placements in scope with the player
			Vector2 playerPos2d(playerPosition.x, playerPosition.y);
			fwRect bb(playerPos2d, static_cast<float>(MAX_PLACEMENT_GENERATION_RANGE));

			// linked list of current pickup placements in scope
			fwPtrListSingleLink pickupsInScope;

			ms_pQuadTree->GetAllMatching(bb, pickupsInScope);

			ms_cachedPlayerPosition = playerPosition;

			ms_pendingPickups.Flush();

			fwPtrNode* pNode=pickupsInScope.GetHeadPtr();
			while(pNode)
			{
				CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(pNode->GetPtr());

				bool bClone = pPlacement->GetNetworkObject() && pPlacement->GetNetworkObject()->IsClone();

				if ((!bClone || !pPlacement->GeneratesNetworkedPickups()) &&
					!pPlacement->GetPickup() && 
					!pPlacement->GetIsCollected() &&
					!pPlacement->GetHasPickupBeenDestroyed() &&
					!ms_pendingPickups.IsMemberOfList(pPlacement))
				{
					Vector2 pos2d(pPlacement->GetPickupPosition().x, pPlacement->GetPickupPosition().y);
					Vector2 diff = pos2d - playerPos2d;
					float dist = diff.Mag2();	
					float generationRange = pPlacement->GetGenerationRange();

					if (dist < generationRange*generationRange)
					{
						// place pickup on pending list
						ms_pendingPickups.Add(pPlacement);
					}
				}

				pNode=pNode->GetNextPtr();
			}
		}
	}
}

// Name			:	ProcessPendingPlacements
// Purpose		:	Processes the pending list (placements waiting to generate pickups)
void CPickupManager::ProcessPendingPlacements()
{
	fwPtrNode* pNode = ms_pendingPickups.GetHeadPtr();

	while (pNode)
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(pNode->GetPtr());
		pNode = pNode->GetNextPtr();

		bool bClone = pPlacement->GetNetworkObject() && pPlacement->GetNetworkObject()->IsClone();

		// leave on the pending list if the placement is a clone - either the pickup will be created for us or the placement will 
		// migrate to our machine and we can create one ourselves.
		if (bClone && (pPlacement->GetIsCollected() || pPlacement->GetHasPickupBeenDestroyed()))
		{
			ms_pendingPickups.Remove(pPlacement);
		}
		else if (!pPlacement->GeneratesNetworkedPickups())
		{
			// remove from pending list if placement is controlled by another machine, goes out of scope or is successfully generated
			if (!pPlacement->GetInScope() || pPlacement->GetPickup() || pPlacement->CreatePickup())
			{
				ms_pendingPickups.Remove(pPlacement);
			}
		}
	}
}

// Name			:	ProcessRegeneratingPlacements
// Purpose		:	Processes placements that are due to regenerate a pickup
void CPickupManager::ProcessRegeneratingPlacements()
{
	fwPtrNode* pNode = ms_regeneratingPlacements.GetHeadPtr();

	while (pNode)
	{
		CRegenerationInfo* pRegenInfo = static_cast<CRegenerationInfo*>(pNode->GetPtr());
		CPickupPlacement* pPlacement = pRegenInfo->GetPlacement();
		netObject* pNetObj = pPlacement->GetNetObject();

		pNode = pNode->GetNextPtr();

		if (pNetObj)
		{
			if (pNetObj->IsClone())
			{
				// clones are told to regenerate by the machine which owns them (a placement may migrate or change network object after being placed on the list)
				ms_regeneratingPlacements.Remove(pRegenInfo);
				delete pRegenInfo;
				pRegenInfo = NULL;
			}
			else if (!pNetObj->IsPendingOwnerChange() && pPlacement->GetPickup())
			{
				// it's possible for a placement to be on the regenerating list with a pickup if it migrated locally before the pickup was destroyed 
				pPlacement->DestroyPickup();
			}
		}

		if (pRegenInfo)
		{
			if (!pPlacement->GetIsCollected() && !pPlacement->GetHasPickupBeenDestroyed())
			{
				if (pPlacement->GetNetObject())
				{
					Assertf(0, "%s is regenerating but is marked as not collected or destroyed!", pPlacement->GetNetObject()->GetLogName());
				}
				else
				{
					Assertf(0, "The pickup at %f, %f, %f is regenerating but is marked as not collected or destroyed!", pPlacement->GetPickupPosition().x, pPlacement->GetPickupPosition().y, pPlacement->GetPickupPosition().z);
				}
			}

			if (pRegenInfo->IsReadyToRegenerate())
			{
				if (pPlacement->Regenerate())
				{
					ms_regeneratingPlacements.Remove(pRegenInfo);
					delete pRegenInfo;
				}
			}
		}
	}
}

// Name			:	ProcessRemovedPlacements
// Purpose		:	Processes placements that are waiting to be removed
void CPickupManager::ProcessRemovedPlacements()
{
	fwPtrNode* pNode = ms_removedPlacements.GetHeadPtr();

	while (pNode)
	{
		CPickupPlacement* pPlacement = static_cast<CPickupPlacement*>(pNode->GetPtr());
		pNode = pNode->GetNextPtr();

		DestroyPickupPlacement(pPlacement);
	}
}

// Name			:	UpdatePickups
// Purpose		:	Updates all the current pickups
void CPickupManager::UpdatePickups()
{
	if(CPickup::GetPool()->GetNoOfUsedSpaces() > 0)
	{
		for (s32 i=0; i<CPickup::GetPool()->GetSize(); i++)
		{
			CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

			if (pPickup) 
			{
				bool bCanDestroy = false;

				// destroy pickups that have gone out of scope
				if (!pPickup->GetNetworkObject() || (!pPickup->IsNetworkClone() && pPickup->GetNetworkObject()->CanDelete()))
				{
					if (pPickup->GetPlacement())
					{
						if (!pPickup->GetPlacement()->GetInScope())
						{
							bCanDestroy = true;
						}
					}
					else 
					{
						if (!pPickup->IsAScriptEntity() && FindPlayerPed() && !pPickup->GetInScope(*FindPlayerPed()))
						{
							if (pPickup->GetNetworkObject() && static_cast<CNetObjGame*>(pPickup->GetNetworkObject())->IsScriptObject())
							{
								Assertf(0, "%s is not set as OWNEDBY_SCRIPT but has a script network object", pPickup->GetNetworkObject()->GetLogName());
							}
							else if (!pPickup->GetNetworkObject() || static_cast<CNetObjPickup*>(pPickup->GetNetworkObject())->TryToPassControlOutOfScope())
							{
								bCanDestroy = true;
							}
						}
					}
				}

				if (bCanDestroy)
				{
					pPickup->Destroy();
				}
				else
				{
					pPickup->Update();
				}
			}
		}

		ms_forceRotatePickupFaceUp  = false; // reset
	}

	// Map placement pickups are only allowed in networked games
	if(CNetwork::IsGameInProgress())
	{
		for (s32 i=0; i<CPickupPlacement::GetPool()->GetSize(); i++)
		{
			CPickupPlacement* pPickupPlacement = CPickupPlacement::GetPool()->GetSlot(i);

			// update map placements only
			if (pPickupPlacement && pPickupPlacement->GetIsMapPlacement())
			{
				pPickupPlacement->Update();
			}
		}
	}
}

void CPickupManager::ClearSpaceInPickupPool(bool network)
{
#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld())
		return;
#endif // GTA_REPLAY

	const s32 MAX_PICKUPS = 200;
	const s32 NUM_PICKUPS_TO_FADE = 16;
	const s32 NUM_PICKUPS_TO_DESTROY = 8;
	
	u32 poolSize = CPickup::GetPool()->GetSize();
	u32 usedSpaces = CPickup::GetPool()->GetNoOfUsedSpaces();

	// Update pool size and used spaces in MP
	if (network && NetworkInterface::IsGameInProgress())
	{
		poolSize = Min(poolSize, (u32)CNetObjPickup::GetPool()->GetSize());
		usedSpaces = Max(usedSpaces, (u32)CNetObjPickup::GetPool()->GetNoOfUsedSpaces());
	}

	u32 desiredNumPickups = Min(poolSize-NUM_PICKUPS_TO_FADE, (u32)((float)poolSize * 0.9f));
	
	// if the pickup pool is nearly full, try to dump some old pickups
	if (usedSpaces > desiredNumPickups)
	{
		u32 numPickupsToFadeout = usedSpaces - desiredNumPickups;
		u32 numPickupsToDestroy = 0;

		CPickup* sortedPickups[MAX_PICKUPS];
		u32 numPickupsToSort = 0;

		s32 i = CPickup::GetPool()->GetSize();

		// test pickup objects
		while(i--)
		{
			CPickup* pPickup = CPickup::GetPool()->GetSlot(i);

			if (pPickup && 
				!pPickup->IsFlagSet(CPickup::PF_Destroyed) && 
				!pPickup->GetPlacement() && 
				!pPickup->IsNetworkClone() && 
				!(pPickup->GetNetworkObject() && pPickup->GetNetworkObject()->IsPendingOwnerChange()))
			{
				if (AssertVerify(numPickupsToSort<MAX_PICKUPS-1))
				{
					sortedPickups[numPickupsToSort++] = pPickup;
				}
			}
		}	

		std::sort(&sortedPickups[0], &sortedPickups[numPickupsToSort], PickupCompare());

		s32 startFadeTime = CPickup::AMBIENT_PICKUP_LIFETIME - CPickup::PICKUP_FADEOUT_TIME;

		if (numPickupsToFadeout > numPickupsToSort)
		{
			numPickupsToFadeout = numPickupsToSort;
		}

		// if the pool is getting dangerously full, start destroying pickups immediately
		if (usedSpaces > poolSize - NUM_PICKUPS_TO_DESTROY)
		{
			numPickupsToDestroy = NUM_PICKUPS_TO_DESTROY - (poolSize - usedSpaces);
		}

		for (i=0; i<numPickupsToFadeout; i++)
		{
			if (AssertVerify(sortedPickups[i]))
			{
				if (i<numPickupsToDestroy)
				{
					sortedPickups[i]->Destroy();
				}
				else if (sortedPickups[i]->GetLifeTime() < startFadeTime)
				{
					sortedPickups[i]->SetLifeTime(startFadeTime);
				}
			}
		}
	}
}

bool CPickupManager::SetStartingVelocities(CPickup* pPickup, CPed *pPed, RagdollComponent primaryBone, RagdollComponent secondaryBone, const Vector3& vExtraLinearVelocity)
{
	bool bVelocitySet = false;

	if(AssertVerify(pPed && pPickup))
	{
		// Initialize the pickup weapon's velocity from the primary hand that held it
		if( pPed->GetRagdollInst()->GetARTAssetID() >= 0 && pPed->GetRagdollInst()->GetCurrentPhysicsLOD() == fragInst::RAGDOLL_LOD_HIGH )
		{
			if( pPed->GetRagdollState() >= RAGDOLL_STATE_PHYS_ACTIVATE )
			{
				phArticulatedBody *body = pPed->GetRagdollInst()->GetCacheEntry()->GetHierInst()->body;
				Assert(body);
				if (body)
				{
					static dev_float maxRelativeVelocity = 8.0f;
					const Vec3V rootVelocity = body->GetLinearVelocity(0);
					const Vec3V handVelocity = body->GetLinearVelocity(primaryBone);
					const Vec3V relativeVelocity = ClampMag(Subtract(handVelocity,rootVelocity),ScalarV(V_ZERO),ScalarVFromF32(maxRelativeVelocity));
					const Vector3 startingLinearVelocity = VEC3V_TO_VECTOR3(Add(rootVelocity,relativeVelocity)) + vExtraLinearVelocity;
					pPickup->SetStartingLinearVelocity(startingLinearVelocity);
					pPickup->SetStartingAngularVelocity(VEC3V_TO_VECTOR3(body->GetAngularVelocity(primaryBone)));
					Assertf(startingLinearVelocity.Dist2(pPed->GetVelocity()) < 20.0f*20.0f || startingLinearVelocity.Mag2() < 5.0f*5.0f, "Setting large relative pickup velocity (RAGDOLL_STATE_PHYS_ACTIVATE). PickupVel: <%f, %f, %f>, PedVel: <%f, %f, %f>",VEC3V_ARGS(RCC_VEC3V(startingLinearVelocity)),VEC3V_ARGS(RCC_VEC3V(pPed->GetVelocity())));
				}
			}
			else
			{
				static bool forceInitialVelocity = true;
				if (forceInitialVelocity)
				{
					phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
					Assert(bound);
					Matrix34 clavMat, handMat;
					clavMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(secondaryBone)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
					handMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(primaryBone)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 

					Vector3 toGun = handMat.d - clavMat.d;
					toGun.z = 0.0f;
					toGun.NormalizeSafe();

					static float linMag = 1.0f;
					static float angMag = 2.5f;

					const Vector3 startingLinearVelocity = toGun * linMag + vExtraLinearVelocity;
					pPickup->SetStartingLinearVelocity(startingLinearVelocity);
					Vector3 vSide;
					vSide.Cross(toGun, Vector3(0.0f,0.0f,-1.0f));
					pPickup->SetStartingAngularVelocity( vSide * angMag );
					Assertf(startingLinearVelocity.Dist2(pPed->GetVelocity()) < 20.0f*20.0f || startingLinearVelocity.Mag2() < 5.0f*5.0f, "Setting large relative pickup velocity (forceInitialVelocity). PickupVel: <%f, %f, %f>, PedVel: <%f, %f, %f>",VEC3V_ARGS(RCC_VEC3V(startingLinearVelocity)),VEC3V_ARGS(RCC_VEC3V(pPed->GetVelocity())));
				}
				else
				{
					phBoundComposite *bound = pPed->GetRagdollInst()->GetCacheEntry()->GetBound();
					Assert(bound);
					Matrix34 lastMat, currMat;
					lastMat.Dot(RCC_MATRIX34(bound->GetCurrentMatrix(primaryBone)), RCC_MATRIX34(pPed->GetRagdollInst()->GetMatrix())); 
					currMat.Dot(RCC_MATRIX34(bound->GetLastMatrix(primaryBone)), RCC_MATRIX34(PHSIM->GetLastInstanceMatrix(pPed->GetRagdollInst()))); 

					Vector3 linVel = (currMat.d - lastMat.d) * fwTimer::GetInvTimeStep();

					Matrix34 lastToCurrent;
					lastToCurrent.DotTranspose(currMat, lastMat);
					Quaternion tempQuat;
					lastToCurrent.ToQuaternion(tempQuat);
					Vector3 angVel;
					float angle;
					tempQuat.ToRotation(angVel, angle);
					angVel.Scale(angle * fwTimer::GetInvTimeStep());

					pPickup->SetStartingLinearVelocity( linVel + vExtraLinearVelocity);
					pPickup->SetStartingAngularVelocity( angVel );
				}
			}

			bVelocitySet = true;
		}
	}

	return bVelocitySet;
}

bool CPickupManager::CanCreatePickupIfUnarmed(const CPed* pPed)
{
	if(pPed->IsAPlayerPed())
	{
		return false;
	}

	// Ignore animals
	if(pPed->GetPedType() == PEDTYPE_ANIMAL)
	{
		return false;
	}

	// Always try to drop the last equipped weapon if it's non-unarmed.
	// For cops or mission peds, they will try to drop the best weapon if they never equipped a non-unarmed weapon.
	bool bCanCreatePickup = false;
	const CPedWeaponManager* pWeaponMgr = pPed->GetWeaponManager();

	if(pWeaponMgr)
	{
		bool bUseMPPickups = ShouldUseMPPickups(pPed);
		const CWeaponInfo* pLastWeaponInfo = pWeaponMgr->GetLastEquippedWeaponInfo();

		if(pLastWeaponInfo && pPed->GetInventory()->GetWeapon(pLastWeaponInfo->GetHash()) && (bUseMPPickups ? (pLastWeaponInfo->GetMPPickupHash() != 0) : (pLastWeaponInfo->GetPickupHash() != 0)))
		{
			bCanCreatePickup = true;
		}
		else if(!pPed->IsAPlayerPed() && (pPed->GetPedType() == PEDTYPE_COP || pPed->PopTypeIsMission()))
		{
			const CWeaponInfo* pBestWeaponInfo = pWeaponMgr->GetBestWeaponInfo();
			if(pBestWeaponInfo)
			{
				bCanCreatePickup = bUseMPPickups ? (pBestWeaponInfo->GetMPPickupHash() != 0) : (pBestWeaponInfo->GetPickupHash() != 0);
			}
		}
	}

	return bCanCreatePickup;
}

bool CPickupManager::ShouldUseMPPickups(const CPed* pPed)
{
	bool bUseMPPickups = false; 

	// only the player drops MP pickups now
	if (NetworkInterface::IsGameInProgress())
	{
		if (pPed)
		{
			bUseMPPickups = pPed->IsLocalPlayer();
		}
		else
		{
			// If pPed is null return true if any ped is using MP pickups
			bUseMPPickups = true;
		}
	}
#if __BANK
	if(ms_forceMPStyleWeaponPickups)
		bUseMPPickups = true;
#endif // __BANK

	return bUseMPPickups;
}

