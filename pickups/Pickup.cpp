// File header
#include "Pickups/Data/PickupDataManager.h"

// rage headers
#include "phbound/boundcomposite.h"

// game headers
#include "audio/ambience/ambientaudioentity.h"
#include "camera/CamInterface.h"
#include "control/replay/Replay.h"
#include "control/restart.h"
#include "frontend/MobilePhone.h"
#include "Network/Events/NetworkEventTypes.h"
#include "peds/Action/ActionManager.h"
#include "peds/Ped.h"
#include "peds/PedIntelligence.h"
#include "peds/PlayerInfo.h" 
#include "physics/gtaArchetype.h"
#include "physics/gtaInst.h"
#include "physics/physics.h"
#include "Pickups/Pickup.h"
#include "Pickups/PickupManager.h"
#include "Pickups/PickupRewards.h"
#include "renderer/OcclusionQueries.h"
#include "renderer/Water.h"
#include "script/script_channel.h"
#include "scene/world/gameworld.h"
#include "script/Handlers/GameScriptEntity.h"
#include "system/control.h"
#include "task/Combat/TaskCombatMelee.h"
#include "task/Default/TaskPlayer.h"
#include "text/messages.h"
#include "Vehicles/Submarine.h"
#include "Vehicles/AmphibiousAutomobile.h"
#include "vfx/misc/GameGlows.h"
#include "vfx/misc/Markers.h"
#include "weapons/WeaponFactory.h"
#include "script/script_hud.h"

// framework headers
#include "fwscript/scriptInterface.h"
#include "fwsys/timer.h"
#include "fwnet/netblender.h"
#include "fwnet/netblenderlininterp.h"
#include "fwscene/stores/staticboundsstore.h"

WEAPON_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()


extern dev_float ARROW_MARKER_Z_OFFSET;
extern dev_float ARROW_MARKER_SCALE;
#if __DEV
extern Color32 ARROW_MARKER_COL;
#else
extern const Color32 ARROW_MARKER_COL;
#endif
extern dev_float ARROW_MARKER_FADE_START_RANGE;
extern dev_float ARROW_MARKER_FADE_OUT_RANGE;
extern dev_bool ARROW_MARKER_BOUNCE;
extern dev_bool ARROW_MARKER_FACE_CAM;

//////////////////////////////////////////////////////////////////////////
// CPickup
//////////////////////////////////////////////////////////////////////////

#if __BANK && ENABLE_GLOWS
float CPickup::ms_customGlowR = 0.0f;
float CPickup::ms_customGlowG = 0.0f;
float CPickup::ms_customGlowB = 0.0f;
float CPickup::ms_customGlowI = 0.0f;
float CPickup::ms_customGlowRange = 0.0f;
float CPickup::ms_customGlowFadeDist = 0.0f;
float CPickup::ms_customGlowOffset = 0.0f;
#endif // __BANK && ENABLE_GLOWS

#define PICKUP_GLOW_FADE_DIST 5.0f
dev_float g_pickupRotationDelta = -TWO_PI / 6000.0f;

#define MAX_BUILDING_CLIFF_HEIGHT (500.0f)

INSTANTIATE_RTTI_CLASS(CPickup,0xad2bcc1a);
FW_INSTANTIATE_BASECLASS_POOL_SPILLOVER(CPickup, CONFIGURED_FROM_FILE, 0.60f, atHashString("CPickup",0xad2bcc1a), sizeof(CPickup));

bool		CPickup::ms_shareVehicleWeaponPickupsAmongstPassengers = false; 
CPickup*	CPickup::ms_pickupsPendingLocalCollection[MAX_PENDING_COLLECTIONS];
u32			CPickup::ms_numPickupsPendingLocalCollection = 0;

unsigned CPickup::m_ExtendedProbeCount = 0;
atRangeArray<CPickup::ExtendedProbeAreas, CPickup::MAX_NUM_EXTENDED_PROBE_AREAS> CPickup::m_AllExtendedProbeAreas;

u32 CPickup::PICKUP_ALPHA_WHEN_TRANSPARENT = 127;

const float CPickup::ACCESSIBLE_DISTANCE_FROM_YACHT = 80.0f;
const Vector3 CPickup::ALL_YACHT_LOCATIONS[SIZE_OF_ALL_YACHT_LOCATIONS] =
{
	Vector3(-3555.1155f,1473.0128f,9.7027f),
	Vector3(-3147.0488f,2827.0879f,9.7027f),
	Vector3(-3277.4729f,2159.8499f,9.7027f),
	Vector3(-2822.4194f,4054.8396f,9.7027f),
	Vector3(-3249.8491f,3704.6814f,9.7027f),
	Vector3(-2383.1934f,4685.0034f,9.7027f),
	Vector3(-3224.6863f,-215.9825f,9.7027f),
	Vector3(-3447.8765f,291.9275f,9.7027f),
	Vector3(-2713.0979f,-528.3185f,9.7027f),
	Vector3(-1981.6182f,-1537.2692f,9.7027f),
	Vector3(-2100.8169f,-2533.2332f,9.7027f),
	Vector3(-1599.6425f,-1891.2773f,9.7027f),
	Vector3(-733.6151f,-3916.9846f,9.7027f),
	Vector3(-363.3534f,-3568.5601f,9.7027f),
	Vector3(-1478.4360f,-3753.5378f,9.7027f),
	Vector3(1535.9740f,-3061.8774f,9.7027f),
	Vector3(2471.4185f,-2430.9297f,9.7027f),
	Vector3(2067.3708f,-2813.0103f,9.7027f),
	Vector3(3021.0881f,-1513.6022f,9.7027f),
	Vector3(3025.9556f,-704.3854f,9.7027f),
	Vector3(2961.8629f,-2007.6315f,9.7027f),
	Vector3(3398.1694f,1958.5214f,9.7027f),
	Vector3(3428.6812f,1202.0597f,9.7027f),
	Vector3(3787.8298f,2567.8838f,9.7027f),
	Vector3(4235.9463f,4004.2522f,9.7027f),
	Vector3(4245.1514f,4595.3750f,9.7027f),
	Vector3(4209.0571f,3392.7053f,9.7027f),
	Vector3(3738.8098f,5768.2524f,9.7027f),
	Vector3(3472.9656f,6315.2451f,9.7027f),
	Vector3(3693.4683f,5194.6587f,9.7027f),
	Vector3(572.9806f,7142.1382f,9.7027f),
	Vector3(2024.0360f,6907.5361f,9.7027f),
	Vector3(1377.2958f,6863.2305f,9.7027f),
	Vector3(-1169.3605f,6000.2139f,9.7027f),
	Vector3(-759.2205f,6573.9551f,9.7027f),
	Vector3(-373.8432f,6964.8599f,9.7027f),
};

CPickup::CPickup(const eEntityOwnedBy ownedBy, u32 hash, u32 customModelIndex, bool bCreateDefaultWeaponAttachments)
: CObject(ownedBy, customModelIndex != fwModelId::MI_INVALID ? customModelIndex : CPickupDataManager::GetPickupData(hash)->GetModelIndex())
, m_pickupHash(hash)
, m_pPickupData(CPickupDataManager::GetPickupData(hash))
, m_pPlacement(NULL)
, m_amount(0)
, m_amountCollected(0)
, m_StartingLinearVelocity(Vector3::ZeroType)
, m_StartingAngularVelocity(Vector3::ZeroType)
, m_pendingCollectionType(COLLECT_INVALID)
, m_pendingCollectionTimer(0)
, m_heightOffGround(0.0f)
, m_lastAccessibleLocation(VEC3_ZERO)
, m_lifeTime(0)
, m_glowOffset(0.5f)
, m_waterLevelNoWaves(-1.0f)
, m_teamPermits(MAX_UINT16)
, m_originalBoundRadius(0.0f)
, m_pWeaponItemForStreaming(NULL)
, m_includeProjectiles(false)
, m_customWeaponHash(0)
, m_allowNonScriptParticipantCollection(false)
#if ENABLE_NETWORK_LOGGING
, m_LastHasGlowFailReason(HasGlowFailReason::None)
#endif // ENABLE_NETWORK_LOGGING
#if __ASSERT
, m_ResetAccessiblePositionCallTimes()
#endif // __ASSERT
{
	Assert(m_pPickupData);

	const u32 modelIndex = customModelIndex!=fwModelId::MI_INVALID? customModelIndex : CPickupDataManager::GetPickupData(hash)->GetModelIndex();
	Assert(modelIndex != fwModelId::MI_INVALID);

	CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(rage::fwModelId(strLocalIndex(modelIndex)));
	if(pModelInfo)
	{
		pModelInfo->SetUseAmbientScale(true);
	}

	m_nObjectFlags.bIsPickUp = true;

	// Force setting as bIsPickup will change the behaviours.
	AssignBaseFlag(fwEntity::USE_SCREENDOOR, GetShouldUseScreenDoor());
	
	AssignBaseFlag(fwEntity::FORCE_ALPHA, true);
	
	if(!m_pPickupData->GetCanBeDamaged())
	{
		m_nPhysicalFlags.bNotDamagedByAnything = true;
	}

	GetLodData().SetResetDisabled(true);

	if (customModelIndex != fwModelId::MI_INVALID)
	{
		SetFlag(PF_HasCustomModel);
	}

	if (bCreateDefaultWeaponAttachments && (customModelIndex == fwModelId::MI_INVALID || customModelIndex == (u32)CPickupDataManager::GetPickupData(hash)->GetModelIndex()))
	{
		SetFlag(PF_CreateDefaultWeaponAttachments);
	}

	if (m_pPickupData->GetOrientateUpright())
	{
		SetFlag(PF_UprightOnGround);
	}

	REPLAY_ONLY(CReplayMgr::OnCreateEntity(this));
}

CPickup::~CPickup()
{
	if (m_pPlacement)
	{
		m_pPlacement->SetPickup(NULL);
	}

	if (IsFlagSet(PF_HasCustomModel) && IsFlagSet(PF_GotCustomArchetype))
	{
		CPickupManager::RemoveRefForExtraCustomArchetype(GetModelIndex());
	}

	if(m_pWeaponItemForStreaming)
	{
		delete m_pWeaponItemForStreaming;
		m_pWeaponItemForStreaming = NULL;
	}

	ClearPendingCollection(false);
}

void CPickup::SetSyncedFlags(u32 flags)
{ 
	u64 currFlags = m_flags.GetAllFlags();

	currFlags &= ~SYNCED_PICKUP_FLAGS_MASK;
	currFlags |= (u64)flags; 
	
	m_flags.SetAllFlags(currFlags);
}

void CPickup::SetAmountAndAmountCollected(u32 amount, u32 amountCollected)
{
	m_amount = amount;
	m_amountCollected = amountCollected;

	if(m_pPlacement)
	{
		m_pPlacement->SetAmount(static_cast<u16>(amount));
		m_pPlacement->SetAmountCollected(static_cast<u16>(amountCollected));
	}
}

void CPickup::SetHideWhenDetached(bool b)
{ 
	if (b != IsFlagSet(PF_HideWhenDetached))
	{
		ChangeFlag(PF_HideWhenDetached, b); 
	
		if (!b && !GetIsAttached() && !GetIsVisible())
		{
			Expose();
		}
	}
}


void CPickup::SetPlacement(CPickupPlacement* pPlacement)			
{ 
	Assert(!pPlacement || !m_pPlacement); 
	m_pPlacement = pPlacement; 

	if (m_pPlacement)
	{
		if (m_pPlacement->GetIsFixed())
		{
			m_nPhysicalFlags.bIgnoresExplosions = true;
		}
		else if (AssertVerify(m_pPickupData))
		{
			// only fixed pickups can be scaled
			Assertf(IsClose(m_pPickupData->GetScale(), 1.0f), "Only fixed pickups can be scaled: %s is not a fixed pickup", GetPickupData()->GetName());
		}
	}
}

void CPickup::SetLastOwner(CPed* pPed)
{
	Assert(!m_pPlacement);
	Assert(!m_lastOwner.Get());

	m_lastOwner = pPed;
}

void CPickup::SetCollected()
{
	SetFlag(PF_Collected);
	m_pendingCollectionType = COLLECT_INVALID;
}

void CPickup::SetPortable()
{
	if (Verifyf(GetPickupData()->GetAttachmentBone() != 0, "Trying to set a pickup as portable that has no attachment data specified in the meta file"))
	{
		// portable pickups are only created by scripts and are flagged as script objects so that they persist and extra state is synced
		SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);

		if (GetNetworkObject() && !GetNetworkObject()->IsClone())
		{
			GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPTOBJECT, true);

			// prevent the pickup from migrating to other machines not running the script that created it
			GetNetworkObject()->SetGlobalFlag(CNetObjGame::GLOBALFLAG_SCRIPT_MIGRATION, true);
		}

		if (IsAlwaysFixed())
		{
			CObject::SetFixedPhysics(true, false);
		}
		else
		{
			// force the pickup to activate whenever it is unfrozen, to prevent it being left floating in the air
			m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = true;
		}

		SetFlag(PF_Portable);

		SetOwnedBy(ENTITY_OWNEDBY_SCRIPT);

		if (IsUnderWater())
		{
			SetFlag(PF_UnderwaterPickup);
		}
	}
}

void CPickup::ClearDroppedInWater()
{
	ClearFlag(PF_DroppedInWater);

	m_nObjectFlags.bFloater = false;

	// make sure any portable pickups that have their physics enabled (due to being dropped in water) are fixed again
	if (IsAlwaysFixed())
	{
		if (GetCurrentPhysicsInst())
		{
			GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);
		}

		CObject::SetFixedPhysics(true, false);
	}
}

void CPickup::SetClonePortablePickupData(bool bInaccessible, const Vector3& lastAccessibleLoc, bool bLastAccessiblePosHasValidGround)
{
	bInaccessible ? SetFlag(PF_Inaccessible) : ClearFlag(PF_Inaccessible);
	m_lastAccessibleLocation = lastAccessibleLoc;
	bLastAccessiblePosHasValidGround ? SetFlag(PF_LastAccessiblePosHasValidGround) : ClearFlag(PF_LastAccessiblePosHasValidGround);
}

void CPickup::Init()
{
	if (!GetRequiresACustomArchetype())
	{
		SetPickupScale();
	}

	// add a looping sound for the pickup if it has one
	if (GetPickupData()->GetLoopingSoundHash())
	{
		audSoundInitParams initParams;
		initParams.Tracker = GetPlaceableTracker();
		naEnvironmentGroup *occlusionGroup = naEnvironmentGroup::Allocate("Pickup");
		initParams.EnvironmentGroup = occlusionGroup;

		audSound *sound;
		g_AmbientAudioEntity.CreateSound_LocalReference(GetPickupData()->GetLoopingSoundHash(), &sound, &initParams);
		if(sound)
		{
			sound->InvalidateEntity();
			sound->PrepareAndPlay();
		}

		if (occlusionGroup)
		{
			occlusionGroup->Init(NULL, 10.f);
			occlusionGroup->ForceSourceEnvironmentUpdate(this);
			occlusionGroup->SetSource(GetTransform().GetPosition());
		}
	}

	m_lastAccessibleLocation = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
}

void CPickup::ForceSetLastAccessibleLocation()
{
	m_lastAccessibleLocation = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	ClearFlag(PF_LastAccessiblePosHasValidGround);

#if __ASSERT
	// overuse tracking
	static const int MAX_CALL_INTERVAL_CHECK = 1000; // 1 sec
	for(int i = 0; i < m_ResetAccessiblePositionCallTimes.GetCount(); i++)
	{
		// remove the ones that were set more then a second ago
		if(m_ResetAccessiblePositionCallTimes[i] + MAX_CALL_INTERVAL_CHECK < fwTimer::GetTimeInMilliseconds())
		{
			m_ResetAccessiblePositionCallTimes.Delete(i);
			i--;
		}
	}
	if(m_ResetAccessiblePositionCallTimes.IsFull())
	{
		scriptAssertf(0, "%s 's last accessible position is being reset too frequently. It was called at least %d times in the last %d second", GetLogName(), MAX_CALL_TIMES, (MAX_CALL_INTERVAL_CHECK/1000));
	}
	else
	{
		m_ResetAccessiblePositionCallTimes.Push(fwTimer::GetTimeInMilliseconds());
	}
#endif // __ASSERT
}

void CPickup::Update()
{
	// pickup may be about to be removed
	if (IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD) || IsFlagSet(PF_Destroyed))
	{
		CModelInfo::GetStreamingModule()->ClearRequiredFlag(GetModelIndex(), STRFLAG_DONTDELETE);
		return;
	}
	// hide pickups when the player is taking a photo
	if (IsFlagSet(PF_HideInPhotos) && CPhoneMgr::CamGetState())
	{
		SetIsVisibleForModule(SETISVISIBLE_MODULE_CAMERA, false, true); 
	}
	else
	{
		SetIsVisibleForModule(SETISVISIBLE_MODULE_CAMERA, true, true); 
	}

	if (!GetArchetype()->GetHasLoaded())
	{
		CModelInfo::GetStreamingModule()->StreamingRequest(strLocalIndex(GetModelIndex()), STRFLAG_PRIORITY_LOAD | STRFLAG_DONTDELETE);
	}
	else if (GetRequiresACustomArchetype() && !IsFlagSet(PF_GotCustomArchetype))
	{
		CreateDrawable();
		AssignCustomArchetype();

		if(GetNetworkObject())
		{
			CNetObjPickup* pickup = SafeCast(CNetObjPickup, GetNetworkObject());
			if(pickup && pickup->GetDisableCollisionCompletely())
			{
				DisableCollision(nullptr, true);
			}
		}
	}
	else if (GetPlacement() && GetPlacement()->GetRoomHash() != 0 && !GetIsInInterior())
	{
		MoveIntoInterior();
	}
	else
	{
		if (!IsFlagSet(PF_Initialised))
		{
			if (!GetIsAttached() && IsNetworkClone() && GetNetworkObject()->GetNetBlender())
			{
				// the net blender may have predicted the pickup under the map while it had no physics, so move it back to the last received position
				// or it may fall through the map
				Vector3 lastPosReceived = static_cast<netBlenderLinInterp*>(GetNetworkObject()->GetNetBlender())->GetLastPositionReceived();
				Vector3 lastVelReceived = static_cast<netBlenderLinInterp*>(GetNetworkObject()->GetNetBlender())->GetLastVelocityReceived();

				// move the pickup up very slightly as the quantization of the position may have pushed the pickup down into the map slightly and it may fall through
				lastPosReceived.z += 0.02f;

				SetPosition(lastPosReceived);
				SetVelocity(lastVelReceived);
			}

			if (IsFlagSet(PF_DroppedFromLocalPlayer))
			{
				SetFlag(PF_LocalPlayerCollision);
			}
		}

		// pickup is now setup and added to world correctly
		SetFlag(PF_Initialised);

		// ambient pickups fade out and are removed after a while
		HandleFade();

		if(GetHealth() <= 0.0f && !IsFlagSet(PF_Destroyed) && GetPickupData()->GetCanBeDamaged())
		{
			AddExplosion();
		}

		if (!GetIsAttached() && IsFlagSet(PF_PlaceOnGround) && !IsFlagSet(PF_PlacedOnGround) && !IsFlagSet(PF_WarpToAccessibleLocation) && !GetPickupData()->GetIsAirbornePickup())
		{
			if (g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
			{
				if(PlaceOnGroundProperly(3.0f, IsFlagSet(PF_OrientToGround), m_heightOffGround, GetPlacement()==NULL, IsFlagSet(PF_UprightOnGround)))
				{
					SetFlag(PF_PlacedOnGround);
				}
			}
		}

		// sanity check collection states match between the pickup and its placement
		if (GetPlacement() && IsFlagSet(PF_Collected) != GetPlacement()->GetIsCollected() && GetNetworkObject() && GetPlacement()->GetNetworkObject())
		{
			if (IsFlagSet(PF_Collected))
			{
				Assertf(0, "%s is flagged as not collected but its pickup %s is flagged as collected", GetPlacement()->GetNetworkObject()->GetLogName(), GetNetworkObject()->GetLogName());
			}
			else
			{
				Assertf(0, "%s is flagged as collected but its pickup %s is flagged as not collected", GetPlacement()->GetNetworkObject()->GetLogName(), GetNetworkObject()->GetLogName());
			}
		}

		CPed* pPendingCarrier = m_pendingCarrier.Get();

		if (pPendingCarrier)
		{
			AttachPortablePickupToPed(pPendingCarrier, "Pending carrier");
		}

#if __ASSERT
		if (IsAlwaysFixed() && !WasDroppedInWater() && GetCurrentPhysicsInst() && GetCurrentPhysicsInst()->IsInLevel())
		{
			if (GetCurrentPhysicsInst()->GetInstFlag(phInst::FLAG_NEVER_ACTIVATE))
			{
				Assertf(!CPhysics::GetLevel()->IsActive(GetCurrentPhysicsInst()->GetLevelIndex()), "Fixed pickup %s has active physics with FLAG_NEVER_ACTIVATE flag set!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "");
			}
			else
			{
				Assertf(0, "Fixed pickup %s has FLAG_NEVER_ACTIVATE flag cleared!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "");
			}
		}
#endif // __ASSERT

		if (!IsFlagSet(PF_LocalPlayerCollision))
		{
			if(IsFlagSet(PF_HelpTextDisplayed))
			{
				CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, true);
			}
			ClearFlag(PF_HelpTextDisplayed);
			ClearFlag(PF_DroppedFromLocalPlayer);
		}

		ClearFlag(PF_LocalPlayerCollision);
	}

	// if we are waiting to collect this pickup, do it now if we own it
	if (GetIsPendingCollection())
	{
		if (m_pendingCollectionType == COLLECT_REMOTE)
		{
			// remote pending collections time out after a while
			m_pendingCollectionTimer -= (s16)fwTimer::GetTimeStepInMilliseconds();

			if (m_pendingCollectionTimer <= 0)
			{
				ClearPendingCollection();
			}
		}
		else if (!(GetPlacement() && GetPlacement()->GetIsMapPlacement()))
		{
			netObject* pNetObj = GetPlacement() ? GetPlacement()->GetNetworkObject() : GetNetworkObject();

			if (!pNetObj || (!pNetObj->IsClone() && !pNetObj->IsPendingOwnerChange()))
			{
				ProcessCollectionResponse(NULL, true);
			}	
		}
	}

	if (IsFlagSet(PF_Portable))
	{
		UpdatePortablePickup();
	}


	if (!IsFlagSet(PF_Collected) && (IsFlagSet(PF_TransparentWhenUncollectable) || IsFlagSet(PF_AddArrowMarker)))
	{
		CPed* pPed = CGameWorld::FindLocalPlayer();

		static const unsigned FULL_ALPHA = 255;
	
		if (pPed && (CanCollectScript(pPed) || IsFlagSet(PF_AllowArrowMarkerWhenUncollectable)))
		{
			if (IsFlagSet(PF_AddArrowMarker))
			{
				Vec3V vPickupPos = GetTransform().GetPosition();
				Vec3V vCamPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
				float distToPickup = Dist(vCamPos, vPickupPos).Getf();
				if (distToPickup<ARROW_MARKER_FADE_OUT_RANGE)
				{
					float alphaMult = 1.0f;
					if (distToPickup>ARROW_MARKER_FADE_START_RANGE)
					{
						alphaMult = (ARROW_MARKER_FADE_OUT_RANGE-distToPickup) / (ARROW_MARKER_FADE_OUT_RANGE-ARROW_MARKER_FADE_START_RANGE);
					}

					u8 alpha = static_cast<u8>(alphaMult*ARROW_MARKER_COL.GetAlphaf()*GetAlpha());

					MarkerInfo_t markerInfo;
					markerInfo.type = MARKERTYPE_ARROW;
					markerInfo.vPos = vPickupPos + Vec3V(0.0f, 0.0f, ARROW_MARKER_Z_OFFSET);
					markerInfo.vRot = Vec4V(PI, 0.0f, 0.0f, 0.0f);
					markerInfo.vScale = Vec3V(ARROW_MARKER_SCALE, ARROW_MARKER_SCALE, ARROW_MARKER_SCALE);
					markerInfo.col = ARROW_MARKER_COL;
					markerInfo.col.SetAlpha(alpha);
					markerInfo.faceCam = ARROW_MARKER_FACE_CAM;
					markerInfo.bounce = ARROW_MARKER_BOUNCE;
					g_markers.Register(markerInfo);
				}
			}

			if (IsFlagSet(PF_TransparentWhenUncollectable))
			{
				if (GetAlpha() < FULL_ALPHA)
				{
					SetAlpha(FULL_ALPHA);
				}
			}
		}
		else if (IsFlagSet(PF_TransparentWhenUncollectable))
		{
			// fade out pickups that have their collection prohibited by script 
			SetAlpha(PICKUP_ALPHA_WHEN_TRANSPARENT);
		}
	}

	if (IsFlagSet(PF_CreateDefaultWeaponAttachments))
	{
		// Find out what weapon this is
		u32 weaponHash = m_customWeaponHash;

		if(weaponHash == 0)
		{
			u32 numRewards = m_pPickupData->GetNumRewards();
			for (u32 i=0; i<numRewards; i++)
			{
				const CPickupRewardData* pReward = GetPickupData()->GetReward(i);
				if (pReward && pReward->GetType() == PICKUP_REWARD_TYPE_WEAPON)
				{
					const CPickupRewardWeapon* pWeaponReward = static_cast<const CPickupRewardWeapon*>(pReward);
					weaponHash = pWeaponReward->GetWeaponHash();
					break;
				}
			}
		}

		if (weaponHash != 0)
		{
			// Ensure drawable is ready.
			if(GetDrawable())
			{
				CWeapon* pWeapon = GetWeapon();

				// Setup the weapon
				if (!pWeapon)
				{
					pWeapon = WeaponFactory::Create(weaponHash, 1, this, "CPickup::Update");
					SetWeapon(pWeapon);
				}

				// Ensure its streamed in (re-using weapon streaming code)
				if (!m_pWeaponItemForStreaming && CInventoryItem::GetPool()->GetNoOfFreeSpaces() != 0)
				{
					m_pWeaponItemForStreaming = rage_new CWeaponItem(0, weaponHash, NULL);
				}

#if __ASSERT
				if(!m_pWeaponItemForStreaming)
				{
					CInventoryItem::SpewPoolUsage();
					Assertf(0, "Inventory item pool is full, check tty for pool usage spew!");
				}
#endif // __ASSERT

				Assert(m_pWeaponItemForStreaming && m_pWeaponItemForStreaming->GetInfo()->GetHash() == weaponHash);

				if (m_pWeaponItemForStreaming && m_pWeaponItemForStreaming->GetIsStreamedIn())
				{
					if (m_pWeaponItemForStreaming->GetInfo())
					{
						CPedEquippedWeapon::SetupAsWeapon(this, m_pWeaponItemForStreaming->GetInfo(), 1, true, NULL, pWeapon);
						SetForceAlphaAndUseAmbientScale();
						SetUseLightOverrideForGlow();
						ClearFlag(PF_CreateDefaultWeaponAttachments);
					}
				}
			}
#if !__NO_OUTPUT
			else
			{
				Printf("GetDrawable() is NULL for pickup [%s] 0x%p, Placement 0x%p\n", m_pPickupData->GetName(), this, GetPlacement());
			}
#endif // !__NO_OUTPUT
		}
		else
		{
			Printf("Not creating weapon for pickup [%s] 0x%p, Placement 0x%p\n", m_pPickupData->GetName(), this, GetPlacement());

			// No weapon reward, flag to not do this again
			ClearFlag(PF_CreateDefaultWeaponAttachments);
		}
	}
	else
	{
		if (m_pWeaponItemForStreaming)
		{
			delete m_pWeaponItemForStreaming;
			m_pWeaponItemForStreaming = NULL;
		}
	}

	if (!GetIsAttached())
	{
		if( GetPickupData()->GetRotates() || (GetPlacement() && GetPlacement()->GetShouldRotate()) )
		{ 
			const float time = (float)fwTimer::GetSystemTimeInMilliseconds();
			const float angle = fwAngle::LimitRadianAngleSafe(time * g_pickupRotationDelta);

			// Centre. We only centre if we're rotating on a placement
			if( GetPlacement() )
			{
				Vector3 offset = VEC3V_TO_VECTOR3(GetBaseModelInfo()->GetBoundingBox().GetCenter());

				Vector3 pickupOrigPosition = GetPlacement()->GetPickupPosition();

				if (GetPickupData()->GetCollectableInBoat())
				{
					// pickups positioned above the water need to move up and down with the waves, so we need to calculate the wave offset and add it
					// to the pickup's z position
					float waveLevelWithWaves = 0.0f;

					if (m_waterLevelNoWaves < 0.0f)
					{
						Water::GetWaterLevelNoWaves(pickupOrigPosition, &m_waterLevelNoWaves, 2.0f, 4.0f, NULL);
					}

					if (m_waterLevelNoWaves >= 0.0f && Water::GetWaterLevel(pickupOrigPosition, &waveLevelWithWaves, false, 2.0f, 4.0f, NULL))
					{
						float waveHeight = waveLevelWithWaves-m_waterLevelNoWaves;
						pickupOrigPosition.z += waveHeight;
					}
				}

				Matrix44 offsetMtx(Matrix44::IdentityType);
				offsetMtx.d.Set(Vector4(-offset.x, -offset.y, 0.0f, 1.0f));

				Matrix44 rotationMtx(Matrix44::IdentityType);
				
				Matrix44 ToWorldPosMtx(Matrix44::IdentityType);
				ToWorldPosMtx.d.Set(Vector4(pickupOrigPosition.GetX(), pickupOrigPosition.GetY(), pickupOrigPosition.GetZ(), 1.0f));

				bool forceFaceUp = CPickupManager::GetForceRotatePickupFaceUp();

				if(!forceFaceUp)
				{
					rotationMtx.MakeRotZ(angle);

					
					Matrix44 finalMtx = offsetMtx;
					finalMtx.Dot(rotationMtx);
					finalMtx.Dot(ToWorldPosMtx);

					Matrix34 final34Mtx;
					finalMtx.ToMatrix34(final34Mtx);
					SetMatrix(final34Mtx);
				}
				else
				{
					Matrix34 matrx;
					Vec3V vecForward(GetTransform().GetB());
					Vec3V_Out cross = Cross(Vec3V(0.0f, 0.0f, 1.0f), vecForward);
					matrx.a.Set(Vector3(cross.GetXf(), cross.GetYf(), cross.GetZf()));
					matrx.b.Set(Vector3(0.0f, 0.0f, 1.0f));
					matrx.c.Set(Vector3(vecForward.GetXf(), vecForward.GetYf(), vecForward.GetZf()));
					matrx.d.Set(GetTransform().GetPosition().GetXf(), GetTransform().GetPosition().GetYf(), GetTransform().GetPosition().GetZf());
					matrx.Normalize();
					SetMatrix(matrx);

					rotationMtx.MakeRotX(angle);

					Matrix44 finalMtx = offsetMtx;
					finalMtx.Dot(rotationMtx);
					finalMtx.Dot(ToWorldPosMtx);

					Matrix34 final34Mtx;
					finalMtx.ToMatrix34(final34Mtx);
					SetMatrix(final34Mtx);
				}
			}
			else
			{
				SetHeading(angle);
			}
		}

		if( GetPickupData()->GetFacePlayer() || (GetPlacement() && GetPlacement()->GetShouldFacePlayer()) )
		{ 
			const Vec3V pos = GetTransform().GetPosition();
			const Vec3V	lookAtPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
			const Vec3V dir = lookAtPos - pos;
			const Vec3V b = Normalize(dir);
			const Vec3V up = Vec3V(V_Z_AXIS_WZERO);
			const Vec3V a = NormalizeSafe( Cross(b,up), Vec3V(V_Y_AXIS_WZERO), Vec3V(V_FLT_EPSILON) ) ;
			const Vec3V c = Cross(a,b);

			Mat34V mtx;
			mtx.SetCol0(a);		
			mtx.SetCol1(b);
			mtx.SetCol2(c);
			mtx.SetCol3(pos);

			SetMatrix(MAT34V_TO_MATRIX34(mtx));
		}
	}

	if (!IsFlagSet(PF_SleepThresholdsInitialized) && GetCollider())
	{
		static float sm_DefaultVelTolerance2 = 0.05f; //0.005f;
		static float sm_DefaultAngVelTolerance2 = 1.0f; //0.01f;
		GetCollider()->GetSleep()->SetVelTolerance2(sm_DefaultVelTolerance2);
		GetCollider()->GetSleep()->SetAngVelTolerance2(sm_DefaultAngVelTolerance2);
		SetFlag(PF_SleepThresholdsInitialized);
	}

	// If it's loaded, one of the code paths above should have created a dependency by now, so we can safely remove the DONTDELETE flag.
	if (GetArchetype()->GetHasLoaded())
	{
		CModelInfo::GetStreamingModule()->ClearRequiredFlag(GetModelIndex(), STRFLAG_DONTDELETE);
	}
}

void CPickup::AttachPortablePickupToPed(CPed* pPed, const char* reason)
{
	netLoggingInterface* pLog = GetNetworkLog();

	// the pickup cannot be attached until it has it's custom archetype
	if (GetRequiresACustomArchetype() && !IsFlagSet(PF_GotCustomArchetype))
	{
		m_pendingCarrier = pPed;

		if (pPed->GetPlayerInfo())
		{
			if (pLog)
			{
				pLog->Log("PortablePickupPending set to true: AttachPortablePickupToPed\n");
			}
			Assert(!pPed->GetPlayerInfo()->PortablePickupPending);
			pPed->GetPlayerInfo()->PortablePickupPending = true;
		}
	}
	else
	{
		eAnimBoneTag attachmentBone = GetPickupData()->GetAttachmentBone();
		// const Vector3 attachOffset = GetPickupData()->GetAttachmentOffset();
		// const Vector3 attachRotation = GetPickupData()->GetAttachmentRotation();

		if (Verifyf(attachmentBone != BONETAG_INVALID, "Trying to attach a portable pickup that has no attachment data specified in the meta file"))
		{
			const Vector3 attachOffset = GetPickupData()->GetAttachmentOffset();
			const Vector3 attachRotation = GetPickupData()->GetAttachmentRotation();

			Quaternion quatRotate;
			quatRotate.Identity();
			if(attachRotation.IsNonZero())
			{
				CScriptEulers::QuaternionFromEulers(quatRotate, DtoR * attachRotation, static_cast<EulerAngleOrder>(EULER_XYZ));
			}

			if (GetIsAttached())
			{
				DetachFromParent(0);
			}

			u32 nBasicAttachFlags = ATTACH_STATE_BASIC|ATTACH_FLAG_INITIAL_WARP;
			AttachToPhysicalBasic( pPed, (s16)pPed->GetBoneIndexFromBoneTag(attachmentBone), nBasicAttachFlags, &attachOffset, &quatRotate );

			if (pLog)
			{
				pLog->WriteDataValue("Attach reason", reason);
			}
		}
	}
}

void CPickup::DetachPortablePickupFromPed(const char* LOGGING_ONLY(reason), bool bPlaceOnGround)
{
	if (GetAttachParent() && Verifyf(static_cast<CEntity*>(GetAttachParent())->GetIsTypePed(), "DetachPortablePickupFromPed: the pickup is not attached to a ped"))
	{
		CPed* pPed = static_cast<CPed*>(GetAttachParent());

		bool bInWater = IsInWater(*pPed);

		Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

		if (pPed->GetIsInVehicle() && pPed->GetMyVehicle())
		{
			// prevent the pickup from colliding with the ped's vehicle 
			SetNoCollisionEntity(pPed->GetMyVehicle());

			if (GetPickupData()->GetCollectableInVehicle())
			{
				SetFlag(PF_CollisionsWithPedVehicleDisabled);
			}
		}
		else
		{
			// prevent the pickup from colliding with the ped
			SetNoCollisionEntity(pPed);
			SetFlag(PF_CollisionsWithPedDisabled);
		}
		
		netLoggingInterface* pLog = GetNetworkLog();

#if ENABLE_NETWORK_LOGGING

		if (pLog && GetNetworkObject())
		{
			NetworkLogUtils::WriteLogEvent(*pLog, "DETACHING_PORTABLE_PICKUP", GetNetworkObject()->GetLogName());
			pLog->WriteDataValue("Reason", "%s", reason);
			pLog->WriteDataValue("Detached from", "%s", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "??");
			pLog->WriteDataValue("Detached at", "%f, %f, %f", pickupPos.x, pickupPos.y, pickupPos.z);
			pLog->WriteDataValue("Last accessible pos", "%f, %f, %f", m_lastAccessibleLocation.x, m_lastAccessibleLocation.y, m_lastAccessibleLocation.z);
			pLog->WriteDataValue("Inaccessible", "%s", IsFlagSet(PF_Inaccessible) ? "True" : "False");

			if (!IsFlagSet(PF_Inaccessible))
			{
				pLog->WriteDataValue("Has valid ground", "%s", IsFlagSet(PF_LastAccessiblePosHasValidGround) ? "true" : "false");

				if (bInWater)
				{
					pLog->WriteDataValue("In water", "true");
				}

				if (IsFlagSet(PF_LyingOnFixedObject))
				{
					pLog->WriteDataValue("Lying on fixed object", "true");
				}

				if (IsFlagSet(PF_LyingOnUnFixedObject))
				{
					pLog->WriteDataValue("Lying on unfixed object", "true");
				}
			}			
		}
#endif // ENABLE_NETWORK_LOGGING

		if (pLog && (CGameWorld::IsEntityBeingRemoved(this) || pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_PedBeingDeleted )))
		{
			pLog->WriteDataValue("Reason", "Ped being deleted");
		}

		if(GetPickupHash()==PICKUP_JETPACK)
		{
			CObject::DetachFromParent(DETACH_FLAG_ACTIVATE_PHYSICS|DETACH_FLAG_APPLY_VELOCITY|DETACH_FLAG_NO_COLLISION_UNTIL_CLEAR);
		}
		else
		{
			CObject::DetachFromParent(0);

			bool forceActivatePhysics = false;
			if(IsFlagSet(PF_ForceActivatePhysicsOnDropNearSubmarine))
			{
				if(IsPickupDroppedNearSubmarine(pickupPos))
				{
					forceActivatePhysics = true;
				}
			}

			if (forceActivatePhysics || IsFlagSet(PF_ForceActivatePhysicsOnPickup) || (!IsAlwaysFixed() && (IsFlagSet(PF_LyingOnFixedObject) || IsFlagSet(PF_LyingOnUnFixedObject))))
			{
				ActivatePhysicsOnPortablePickup();
				bPlaceOnGround = false;
			}
			else
			{
				CObject::SetFixedPhysics(true, false);

				m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = false;

				SetFlag(PF_OrientToGround);
			}

			if (GetPickupData()->GetIsAirbornePickup())
			{
				static float MIN_DIST_FROM_GROUND = 15.0f;

				// pickups collectable by plane are always left in the air 
				if (pLog)
				{
					pLog->WriteDataValue("Airborne pickup", "True");
				}
				
				bool bFoundGround = false;

				// only leave in air if the plane is a certain distance above the ground
				float groundZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, pickupPos.x, pickupPos.y, pickupPos.z+MAX_BUILDING_CLIFF_HEIGHT, &bFoundGround);

				float waterZ = 0.0f;
				
				Water::GetWaterLevelNoWaves(pickupPos, &waterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL);

				// use water level if the ground is under water
				if (groundZ < waterZ || bInWater)
				{
					groundZ = waterZ;
					bFoundGround = true;
				}

				if (pLog)
				{
					if (bFoundGround)
					{
						pLog->WriteDataValue("Found ground / water at", "%0.2f", groundZ);
						pLog->WriteDataValue("Difference", "%0.2f", pickupPos.z - groundZ);
					}
					else
					{
						pLog->WriteDataValue("Found ground", "False");
					}
				}

				if (bFoundGround && (groundZ+MIN_DIST_FROM_GROUND > pickupPos.z))
				{
					pickupPos.z = groundZ+MIN_DIST_FROM_GROUND;

					if (pLog)
					{
						pLog->WriteDataValue("Too near ground at", "%f", groundZ);
						pLog->WriteDataValue("Pickup z now", "%f", pickupPos.z);
					}

					Teleport(pickupPos);
				}
			}
			else if (bInWater)
			{
				DropInWater();
			}
			else if (bPlaceOnGround)	
			{
				Vector3 droppedPos = m_lastAccessibleLocation;

				if (!IsFlagSet(PF_Inaccessible))
				{
					if (!IsFlagSet(PF_LastAccessiblePosHasValidGround))
					{
						float groundZ = 0.0f;
						bool bFoundGround = false;

						bool mapLoadedAboutPos = g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER);

						if (mapLoadedAboutPos)
						{
							groundZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, droppedPos.x, droppedPos.y, droppedPos.z+0.5f, &bFoundGround);

							if (bFoundGround)
							{
								if (pLog)
								{
									if (groundZ > droppedPos.z)
									{
										pLog->WriteDataValue("Below map", "True");
									}

									pLog->WriteDataValue("Ground Z found", "%f", groundZ);
								}

								droppedPos.z = groundZ + m_heightOffGround;
							}
							else
							{
								if(pLog)
								{
									pLog->WriteDataValue("Failed to find ground", "True");
								}

								// the ground was not found or is too far away
								SetFlag(PF_Inaccessible);
							}
						}
						else 
						{
							if (pLog)
							{
								pLog->WriteDataValue("No map collision", "True");
							}

							// the ground was not found 
							SetFlag(PF_Inaccessible);
						}
					}
				}

				if (IsNetworkClone() && IsFlagSet(PF_DroppedInWater))
				{
					DropInWater(false);
				}

				if (IsFlagSet(PF_Inaccessible))
				{
					if (pPed->GetIsInInterior())
					{
						SetFlag(PF_DroppedInInterior);
					}

					if (!IsNetworkClone() REPLAY_ONLY(&& GetOwnedBy() != ENTITY_OWNEDBY_REPLAY))
					{
						MoveToAccessibleLocation();
					}
				}
				else 
				{
					FindSuitablePortablePickupDropLocation(droppedPos);
				}
			}
		}
	
		ForceUpdateOfDroppedPortablePickupData();

		OnPedDetachment(pPed);

		if (GetNetworkObject() && pPed->GetNetworkObject())
		{
			NetworkSetPositionOfPlaneDroppedPortablePickup(pPed, pPed->GetMyVehicle());

			CNetObjPickup* pPickupObj = static_cast<CNetObjPickup*>(GetNetworkObject());

			if (pPickupObj->GetPendingAttachmentObjectID() == pPed->GetNetworkObject()->GetObjectID())
			{
				pPickupObj->ClearPendingAttachmentData();
			}
		}

		// this is a hacky GTAV-specific fix due to camManager::ModifyPedVisibility
		// we expect this to be unnecessary once we have a more correct fix available
		if(!GetRenderPhaseVisibilityMask().IsFlagSet(VIS_PHASE_MASK_GBUF))
		{
			if (pLog)
			{
				pLog->WriteDataValue("Reset GBuffer Visibility Mask", "True");
			}
			Displayf("Reset GBuffer Visibility Mask for pickup: %s", GetLogName());

			GetRenderPhaseVisibilityMask().SetFlag(VIS_PHASE_MASK_GBUF);
		}
	}
}

void CPickup::Destroy()
{
	// Destroy() can be called more than once before the pickup is removed so check for this.
	if (!IsFlagSet(PF_Destroyed) && AssertVerify(!IsBaseFlagSet(fwEntity::REMOVE_FROM_WORLD))) 
	{
		SetFlag(PF_Destroyed);

		FlagToDestroyWhenNextProcessed();
		
		// hide pickup and remove collision to prevent further collection detection
		Hide(true);

		CTheScripts::UnregisterEntity(this, false);

		if (GetNetworkObject() && !IsNetworkClone())
		{
			NetworkInterface::UnregisterObject(this);
		}

		if (m_pPlacement)
		{
			m_pPlacement->SetPickup(NULL);
			m_pPlacement = NULL;
		}

		if (IsFlagSet(PF_HelpTextDisplayed))
		{
			CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, true);
			ClearFlag(PF_HelpTextDisplayed);
		}
	}
}

bool CPickup::IsPickupDroppedNearSubmarine(Vector3 pickupPos)
{
	const float MIN_SUBMARINE_DISTANCE_FOR_PHYSICS_ACTIVATION = 100.0f;

	CVehicle::Pool* pool = CVehicle::GetPool();
	s32 i = (s32) pool->GetSize();
	while (i--)
	{
		CVehicle* veh = pool->GetSlot(i);
		if(veh && veh->GetModelNameHash() == ATSTRINGHASH("Kosatka", 0x4FAF0D70))
		{
			Vector3 vehPos(veh->GetVehiclePosition().GetXf(), veh->GetVehiclePosition().GetYf(), veh->GetVehiclePosition().GetZf());
			if(pickupPos.Dist(vehPos) <MIN_SUBMARINE_DISTANCE_FOR_PHYSICS_ACTIVATION)
			{
				return true;
			}
		}
	}
	return false;
}

netLoggingInterface* CPickup::GetNetworkLog() const
{
	netLoggingInterface* pLog = NULL;

	LOGGING_ONLY(pLog = NetworkInterface::IsGameInProgress() ? &NetworkInterface::GetObjectManager().GetLog() : NULL);

	return pLog;
}


void CPickup::OnActivate(phInst* pInst, phInst* pOtherInst)
{
	CObject::OnActivate(pInst, pOtherInst);

	if (phCollider* collider = GetCollider())
	{
		collider->DisablePushCollisions();
	}
}

//PURPOSE: special pickup constructor
bool CPickup::CreateDrawable()
{
	bool rt = CObject::CreateDrawable();
	// Somehow pickups are being created without a skeleton but being flagged as skinned. The shadow pass don't like that
	// It pushes them thru as skinned but without a matrixset
	if(GetSkeleton() == NULL)
		ClearRenderFlags(RenderFlag_IS_SKINNED);
	return rt;
}

// Name			:	RequiresProcessControl
// Purpose		:	Some pickups need to be on the process control list
bool CPickup::RequiresProcessControl() const
{
	return (!IsAlwaysFixed() || GetPickupData()->GetLoopingSoundHash());
}

ePrerenderStatus CPickup::PreRender(const bool bIsVisibleInMainViewport)
{
	return 	CObject::PreRender(bIsVisibleInMainViewport);
}

bool CPickup::HasGlow() const
{
	LOGGING_ONLY(HasGlowFailReason failReason = HasGlowFailReason::None;)

	bool	bHasGlow = m_pPickupData->GetHasGlow();
	
	LOGGING_ONLY(if(!bHasGlow) failReason = HasGlowFailReason::PickupData;)

	if (!(CanCollectScript(CGameWorld::FindLocalPlayer()) || IsFlagSet(PF_GlowOnProhibitCollection)) || IsFlagSet(PF_DontGlow) )
	{
		bHasGlow = false;
		LOGGING_ONLY(
			if(IsFlagSet(PF_DontGlow)) failReason = HasGlowFailReason::DoNotGlowFlag;
			else failReason = HasGlowFailReason::CanNotCollectScript;
		)
	}
	
	// stop pickups glowing that our player is prohibited from collecting 
	if (bHasGlow && NetworkInterface::IsGameInProgress() && NetworkInterface::GetLocalPlayer())
	{
		// portable pickups can only be collected by the participants of the script that created them
		if (IsFlagSet(PF_Portable) && !IsFlagSet(PF_DebugCreated))
		{
			const CScriptEntityExtension* pScriptExtension = GetExtension<CScriptEntityExtension>();

			if (pScriptExtension)
			{
				scriptHandler* pScriptHandler = pScriptExtension->GetScriptHandler();

				if (!m_allowNonScriptParticipantCollection
					&& (!pScriptHandler || 
					!pScriptHandler->GetNetworkComponent() ||
					pScriptHandler->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING ||
					!pScriptHandler->GetNetworkComponent()->IsPlayerAParticipant(*NetworkInterface::GetLocalPlayer())))
				{
					bHasGlow = false;
					LOGGING_ONLY(failReason = HasGlowFailReason::NotParticipant;)
				}
			}
			else
			{
				bHasGlow = false;
				LOGGING_ONLY(failReason = HasGlowFailReason::NoScriptExtension;)
			}
		}

		if (bHasGlow)
		{
			// some pickups can only be collected by certain teams, so only glow for those teams. They always glow regardless of whether the
			// local player is permitted to collect them.
			if (IsFlagSet(PF_TeamPermitsSet))
			{
				if (NetworkInterface::GetLocalPlayer()->GetTeam() == INVALID_TEAM ||
					(!(m_teamPermits & (1<<NetworkInterface::GetLocalPlayer()->GetTeam())) && !GetGlowWhenInSameTeam()))
				{
					bHasGlow = false;
					LOGGING_ONLY(failReason = HasGlowFailReason::NotForTeam;)
				}
			}
			else if (!IsFlagSet(PF_GlowOnProhibitCollection) && CPickupManager::IsCollectionProhibited(GetPickupHash(), NetworkInterface::GetLocalPlayer()->GetPhysicalPlayerIndex()))
			{
				bHasGlow = false;
				LOGGING_ONLY(failReason = HasGlowFailReason::ProhibitCollection;)
			}
		}
	}
	
#if ENABLE_NETWORK_LOGGING
	if(m_LastHasGlowFailReason != failReason)
	{
		m_LastHasGlowFailReason = failReason;
		if(NetworkInterface::IsGameInProgress())
		{
		netLoggingInterface &log = NetworkInterface::GetObjectManager().GetLog();
		NetworkLogUtils::WriteLogEvent(log, "PICKUP_CANNOT_GLOW", "%s", GetLogName());
		log.WriteDataValue("Reason", "%s", GetCannotGlowReason(m_LastHasGlowFailReason));
		}
	}
#endif // ENABLE_NETWORK_LOGGING
	return bHasGlow;
}

bool CPickup::HasLifetime() const
{
	return (!GetPlacement() && 
		!IsFlagSet(PF_Portable) && 
		!IsFlagSet(PF_DontFadeOut) && 
		GetOwnedBy() != ENTITY_OWNEDBY_SCRIPT &&
		!GetExtension<CScriptEntityExtension>());
}

// Name			:	PreRender2
// Purpose		:	Handles pickup rendering effects (pickup glow)
// Parameters	:	None
// Returns		:   Nothing
void CPickup::PreRender2(const bool bIsVisibleInMainViewport)
{
	CObject::PreRender2(bIsVisibleInMainViewport);
}

void CPickup::Teleport(const Vector3& vecSetCoors, float fSetHeading, bool bCalledByPedTask, bool bTriggerPortalRescan, bool bCalledByPedTask2, bool bWarp, bool UNUSED_PARAM(bKeepRagdoll), bool UNUSED_PARAM(bResetPlants))
{
	CObject::Teleport(vecSetCoors, fSetHeading, bCalledByPedTask, bTriggerPortalRescan, bCalledByPedTask2, bWarp);

	if (!IsNetworkClone())
	{
		ClearDroppedInWater();
	}

	SetPlaceOnGround(IsFlagSet(PF_OrientToGround), IsFlagSet(PF_UprightOnGround));

	// stop any search for a new respawn location in case this is getting called by script
	ClearFlag(PF_WarpToAccessibleLocation);

	if (GetNetworkObject() && !GetNetworkObject()->IsClone() && !GetNetworkObject()->IsPendingOwnerChange())
	{
		static_cast<CNetObjPickup*>(GetNetworkObject())->SetOverridingRemoteVisibility(false, false, "Teleporting pickup");
	}
}

#if __BANK
void CPickup::DebugAttachToPhysicalBasic(const char *strCodeFile, const char* strCodeFunction, u32 nCodeLine, CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone)
{
	fwEntity* pAttachParent = GetAttachParent();

	CObject::DebugAttachToPhysicalBasic(strCodeFile, strCodeFunction, nCodeLine, pPhysical, nEntBone, nAttachFlags, pVecOffset, pQuatOrientation, nMyBone);

	if (GetAttachParent() == pPhysical && pAttachParent != GetAttachParent() && pPhysical->GetIsTypePed())
	{
		OnPedAttachment(static_cast<CPed*>(pPhysical));
	}
}
#else
void CPickup::AttachToPhysicalBasic(CPhysical* pPhysical, s16 nEntBone, u32 nAttachFlags, const Vector3* pVecOffset, const Quaternion* pQuatOrientation, s16 nMyBone)
{
	CObject::AttachToPhysicalBasic(pPhysical, nEntBone, nAttachFlags, pVecOffset, pQuatOrientation, nMyBone);

	if (pPhysical->GetIsTypePed() && AssertVerify(GetAttachParent() == pPhysical))
	{
		OnPedAttachment(static_cast<CPed*>(pPhysical));
	}
}
#endif

void CPickup::SetFixedPhysics(bool bFixed, bool bNetwork)
{
	// don't allow other systems or script to tinker with the fixed state of permanently fixed pickups
	if (!IsAlwaysFixed())
	{
#if ENABLE_NETWORK_LOGGING
		netLoggingInterface* pLog = GetNetworkLog();

		if (pLog && GetNetworkObject() && IsFlagSet(PF_Portable))
		{
			if (bFixed)
			{
				NetworkLogUtils::WriteLogEvent(*pLog, "PORTABLE_PICKUP_BEING_FIXED", GetNetworkObject()->GetLogName());
			}
			else
			{
				NetworkLogUtils::WriteLogEvent(*pLog, "PORTABLE_PICKUP_BEING_UNFIXED", GetNetworkObject()->GetLogName());
			}
		}
#endif // ENABLE_NETWORK_LOGGING

		CObject::SetFixedPhysics(bFixed, bNetwork);
	}
}

u32 CPickup::GetDefaultInstanceIncludeFlags(phInst* pInst) const
{
	u32 includeFlags = CObject::GetDefaultInstanceIncludeFlags(pInst);

	if (GetRequiresACustomArchetype() && IsFlagSet(CPickup::PF_Initialised))
	{
		phArchetypeDamp* pCustomArchetype = CPickupManager::GetCustomArchetype(*this);

		if (AssertVerify(pCustomArchetype))
		{
			includeFlags = pCustomArchetype->GetIncludeFlags();
		}
	}

	if( m_includeProjectiles )
	{
		includeFlags |= ArchetypeFlags::GTA_PROJECTILE_TYPE;
	}

	return includeFlags;
}

void CPickup::DetachFromParent(u16 nDetachFlags)
{
	if(GetCurrentPhysicsInst() && !GetCurrentPhysicsInst()->IsInLevel())
	{
		AddPhysics();
	}

	CEntity* pAttachParent = SafeCast(CEntity, GetAttachParent());

	if (pAttachParent && pAttachParent->GetIsTypePed())
	{
		if( !(nDetachFlags & DETACH_FLAG_DONT_REMOVE_BASIC_ATTACHMENTS ))
		{
			if (CGameWorld::IsEntityBeingRemoved(this))
			{
				CObject::DetachFromParent(nDetachFlags);
				OnPedDetachment((CPed*)pAttachParent);
			}
			else
			{
				DetachPortablePickupFromPed("Detach from parent");
			}
		}
	}
	else
	{
		if (IsAlwaysFixed())
		{
			nDetachFlags &= ~DETACH_FLAG_ACTIVATE_PHYSICS;
		}

		CObject::DetachFromParent(nDetachFlags);

		if(IsNetworkClone() && GetNetworkObject())
		{
			CNetObjPickup* netPickup = SafeCast(CNetObjPickup, GetNetworkObject());
			if(netPickup->GetNetBlender())
			{
				netPickup->GetNetBlender()->GoStraightToTarget();
			}
		}

		if (IsAlwaysFixed())
		{
			if(GetCurrentPhysicsInst())
			{
				GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, true);
			}
		}
	}
}

bool CPickup::TestNoCollision(const phInst *pOtherInst)
{
	if (IsFlagSet(PF_CollisionsWithPedDisabled) || IsFlagSet(PF_CollisionsWithPedVehicleDisabled))
	{
		const CEntity * pOtherEntity = CPhysics::GetEntityFromInst(pOtherInst);
		const CEntity* pNoCollisionEntity = (const CEntity*)GetNoCollisionEntity();

		if (pOtherEntity && pNoCollisionEntity && pOtherEntity == pNoCollisionEntity)
		{
			if (IsFlagSet(PF_CollisionsWithPedDisabled) && pNoCollisionEntity->GetIsTypePed())
			{
				const CPed* pPed = static_cast<const CPed*>(pOtherEntity);

				if (!pPed->IsDead())
				{
					SetNoCollisionEntity(NULL);
					ClearFlag(PF_CollisionsWithPedDisabled);
				}
			}
			else if (IsFlagSet(PF_CollisionsWithPedVehicleDisabled) && pNoCollisionEntity->GetIsTypeVehicle())
			{
				// when a ped dies in a vehicle and drops the pickup, collisions are disabled between the vehicle and pickup to prevent physics glitches. We need to enable them again if the vehicle is being driven
				// by a local alive ped, so that the pickup can be collected again
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pOtherEntity);

				if (pVehicle->GetDriver() && !pVehicle->GetDriver()->IsNetworkClone() && !pVehicle->GetDriver()->IsDead())
				{
					SetNoCollisionEntity(NULL);
					ClearFlag(PF_CollisionsWithPedVehicleDisabled);
				}
			}
		}
	}

	return CObject::TestNoCollision(pOtherInst);
}

#if ENABLE_NETWORK_LOGGING
void CPickup::ShouldFindImpactsCalled()
{
	NetworkInterface::GetObjectManagerLog().Log("** ShouldFindImpactsCallback called on %s collision with local player **\n", GetNetworkObject()->GetLogName());
}

void CPickup::ShouldFindImpactsFailed(const char* reason)
{
	NetworkInterface::GetObjectManagerLog().Log("** FAILED: %s **\n", reason);
}

void CPickup::ShouldFindImpactsSuccess()
{
	NetworkInterface::GetObjectManagerLog().Log("** ShouldFindImpactsCallback successfully returned true for this pickup **\n", GetNetworkObject()->GetLogName());
}

const char* CPickup::GetCannotGlowReason(HasGlowFailReason reason) const
{
	switch (reason)
	{
	case HasGlowFailReason::None:
		return "None";
	case HasGlowFailReason::PickupData:
		return "PickupData";
	case HasGlowFailReason::DoNotGlowFlag:
		return "DoNotGlowFlag";
	case HasGlowFailReason::CanNotCollectScript:
		return "CanNotCollectScript";
	case HasGlowFailReason::NotParticipant:
		return "NotParticipant";
	case HasGlowFailReason::NoScriptExtension:
		return "NoScriptExtension";
	case HasGlowFailReason::NotForTeam:
		return "NotForTeam";
	case HasGlowFailReason::ProhibitCollection:
		return "ProhibitCollection";
	default:
		return "Unknown";
	}

}
#endif // ENABLE_NETWORK_LOGGING

// Name			:	ProcessPreComputeImpacts
// Purpose		:	Called whenever there is a collision with the pickup. Detects "on foot" and "in car" collection. 
// Parameters	:	impacts - the collision impact information
// Returns		:   Nothing
void CPickup::ProcessPreComputeImpacts(phContactIterator impacts)
{
	////////////////////////////////////////
	// ON FOOT & IN CAR COLLECTION DETECTION
	////////////////////////////////////////

	impacts.Reset();
	while(!impacts.AtEnd())
	{		
		// don't allow collection if the pickup is hidden
		if (!GetIsVisibleForModule(SETISVISIBLE_MODULE_PICKUP))
		{
			impacts.DisableImpact();
			impacts++;
			continue;
		}

		if (GetRequiresACustomArchetype())
		{
			// Crash if archetype is set up wrong
			physicsAssertf(impacts.GetMyComponent() == 0 || GetPhysArch()->GetBound()->GetType() == phBound::COMPOSITE, "Invalid bound type in pickup archetype");
			
			const int iSphereComponent = (GetPhysArch()->GetBound()->GetType() == phBound::COMPOSITE) ? static_cast<phBoundComposite*>(GetCurrentPhysicsInst()->GetArchetype()->GetBound())->GetNumBounds()-1 : 0;

			// always disable the impact for the additional sphere we added to the pickup's bounds for detecting collection
			if (impacts.GetMyComponent() == iSphereComponent)
			{
				impacts.DisableImpact();
			}
			else
			{
				const float kfPickupFriction = 0.5f;
				impacts.SetFriction(kfPickupFriction);
			}
		}

		CEntity* pOtherEntity = impacts.GetOtherInstance() ? static_cast<CEntity*>(impacts.GetOtherInstance()->GetUserData()) : NULL;
		CPed* pOtherPed = NULL;
		eCollectionType collectionType = COLLECT_ONFOOT;
		unsigned failureCode = PCC_NONE;

		if (pOtherEntity)
		{
			if (pOtherEntity->GetIsTypePed())
			{
				pOtherPed = (CPed*)pOtherEntity;
			}
			else if (pOtherEntity->GetIsTypeVehicle())
			{
				CVehicle* pOtherVehicle = static_cast<CVehicle*>(pOtherEntity);
				pOtherPed = pOtherVehicle->GetDriver();

				if( !pOtherPed || !pOtherPed->IsLocalPlayer())
				{
					// Search for the local player
					for(int iSeatIndex = 0; iSeatIndex < pOtherVehicle->GetSeatManager()->GetMaxSeats(); iSeatIndex++)
					{
						pOtherPed = pOtherVehicle->GetSeatManager()->GetPedInSeat(iSeatIndex);
						if(pOtherPed && pOtherPed->IsLocalPlayer())
						{
							break;
						}
					}
				}

				collectionType = COLLECT_INCAR;
			}
		}

		if( pOtherPed && !pOtherPed->IsLocalPlayer() && NetworkInterface::IsGameInProgress() && !IsAlwaysFixed() && IsFlagSet(PF_Portable) )
		{
			impacts.DisableImpact();
			impacts++;
			continue;
		}

		LOGGING_ONLY(netLoggingInterface* pLog = NetworkInterface::IsGameInProgress() ? &NetworkInterface::GetObjectManagerLog() : NULL);

		if (pOtherPed && pOtherPed->IsLocalPlayer())
		{
			LOGGING_ONLY(if (pLog) pLog->Log("** ProcessPreComputeImpacts called on %s collision with local player **\n", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "-map pickup-"));

#if FPS_MODE_SUPPORTED
			bool bFirstPerson = pOtherPed->IsInFirstPersonVehicleCamera() || pOtherPed->IsFirstPersonShooterModeEnabledForPlayer(false);

			if (GetPhysArch() && GetPhysArch()->GetBound())
			{
				const int iSphereComponent = (GetPhysArch()->GetBound()->GetType() == phBound::COMPOSITE) ? static_cast<phBoundComposite*>(GetCurrentPhysicsInst()->GetArchetype()->GetBound())->GetNumBounds()-1 : 0;

				// always disable the impact for the additional sphere we added to the pickup's bounds for detecting collection
				if (impacts.GetMyComponent() == iSphereComponent)
				{
					float fCollisionRadiusDiff = GetPickupData()->GetCollectionRadiusFirstPerson() - GetPickupData()->GetCollectionRadius();

					if( (fCollisionRadiusDiff > 0.0f && !bFirstPerson) || (fCollisionRadiusDiff < 0.0f && bFirstPerson) )
					{
						fCollisionRadiusDiff = abs(fCollisionRadiusDiff);
						float fDepth = impacts.GetDepth();
						if(fDepth < fCollisionRadiusDiff)
						{
							impacts.DisableImpact();
							impacts++; 
							LOGGING_ONLY(if (pLog) pLog->Log("** Impact rejected due to 1st/3rd person collection sphere check (1st person radius: %f, 3rd person: %f, depth = %f) **\n", GetPickupData()->GetCollectionRadiusFirstPerson(), GetPickupData()->GetCollectionRadius(), fDepth));
							continue;
						}
					}
				}
			}			
#endif

			if (CanCollect(pOtherPed, collectionType, &failureCode))
			{
				LOGGING_ONLY(if (pLog) pLog->Log("** CanCollect returned true **\n"));

				if (collectionType == COLLECT_ONFOOT)
				{
					CollectOnFoot(pOtherPed);
				}
				else
				{
					CollectInCar(pOtherPed);
				}
			}
			else 
			{
				if(failureCode == PCC_ON_THE_PHONE)
				{
					if(IsFlagSet(PF_HelpTextDisplayed))
					{
						CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, true);
						ClearFlag(PF_HelpTextDisplayed);
					}
				}

				// Display help text for manual pickups.
				if(failureCode == PCC_BUTTON_NOT_PRESSED)
				{
					if (!IsFlagSet(PF_HelpTextDisplayed) && !IsFlagSet(PF_Destroyed))
					{
						bool bIsPetrolCan = false;
						const u32 uPickupWeaponHash = GetPickupData()->GetFirstWeaponReward();
						if (uPickupWeaponHash != 0)
						{
							const CWeaponInfo* pPickupWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uPickupWeaponHash);
							if (pPickupWeaponInfo && pPickupWeaponInfo->GetGroup() == WEAPONGROUP_PETROLCAN)
							{
								bIsPetrolCan = true;
							}
						}

						if(GetPickupData()->GetHash() == PICKUP_JETPACK)
						{
							CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, TheText.Get("PU_JET_HLP"));
							SetFlag(PF_HelpTextDisplayed);
						}
						else if (bIsPetrolCan)
						{
							if (!pOtherPed->GetInventory()->GetWeapon(uPickupWeaponHash))
							{	
								char* pJerryCanHelpText;
								if(GetPickupData()->GetHash() == PICKUP_WEAPON_FERTILIZERCAN)
								{
									pJerryCanHelpText = TheText.Get("PU_FER_HLP");
								}
								else
								{
									pJerryCanHelpText = TheText.Get("PU_JER_HLP");
								}

								// B*2186514: Don't try to set the petrol can help text if we're already displaying it
								bool bAlreadyDisplayingJerryCanText = false;
								if (strcmp(CHelpMessage::GetMessageText(HELP_TEXT_SLOT_STANDARD), pJerryCanHelpText) == 0)
								{
									bAlreadyDisplayingJerryCanText = true;
									SetFlag(PF_HelpTextDisplayed);
								}

								if (!bAlreadyDisplayingJerryCanText)
								{
									CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, pJerryCanHelpText);
									SetFlag(PF_HelpTextDisplayed);
								}
							}
						}
						else
						{
							CHelpMessage::SetMessageText(HELP_TEXT_SLOT_STANDARD, TheText.Get("PU_HLP"));
							SetFlag(PF_HelpTextDisplayed);
						}
					}

					pOtherPed->SetPedResetFlag(CPED_RESET_FLAG_TemporarilyBlockWeaponSwitching, true);
				}

#if ENABLE_NETWORK_LOGGING
				if (pLog)
				{
					const char* failureReason = GetCanCollectFailureString(failureCode);

					if (GetNetworkObject())
					{
						pLog->Log("** CAN'T COLLECT %s %s: %s\n", GetNetworkObject()->GetLogName(), collectionType == COLLECT_ONFOOT ? "ON FOOT" : "IN VEHICLE", failureReason);
					}
					else 
					{
						Vector3 pos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
						pLog->Log("** CAN'T COLLECT PICKUP AT %f, %f, %f %s: %s\n", pos.x, pos.y, pos.z, collectionType == COLLECT_ONFOOT ? "ON FOOT" : "IN VEHICLE", failureReason);
					}
				}
#endif // ENABLE_NETWORK_LOGGING

				if (IsFlagSet(PF_DroppedFromLocalPlayer) || (NetworkInterface::IsGameInProgress() && !IsAlwaysFixed() && IsFlagSet(PF_Portable)) )
				{
					impacts.DisableImpact();
				}
			}

			SetFlag(PF_LocalPlayerCollision);
		}

		bool bCollectableInVehicle = GetPickupData() ? GetPickupData()->GetCollectableInVehicle() : false;
		bool bOtherEntityIsVehicle = false;
		
		if(pOtherEntity && pOtherEntity->GetIsTypeVehicle())
		{
			bool disableKosatkaCollisionCheck = Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("DISABLE_KOSATKA_PICKUP_COLLISION_CHECK", 0xBD6EAC14), false);
			if(disableKosatkaCollisionCheck || !MI_SUB_KOSATKA.IsValid() || pOtherEntity->GetModelIndex() != MI_SUB_KOSATKA)
			{
				bOtherEntityIsVehicle = true;
			}
		}

		if ((!IsCollideable() && !IsFlagSet(PF_Portable)) || IsFlagSet(PF_Collected) || (bCollectableInVehicle && bOtherEntityIsVehicle))
		{
			impacts.DisableImpact();
		}

		impacts++;
	}
}

// Name			:	ProcessWeaponImpact
// Purpose		:	Called whenever a weapon hits the pickup. Detects "on shot" collection. 
// Parameters	:	pParentEntity - the entity using the weapon
// Returns		:   True if the impact is to be ignored by the rest of the weapon impact code
bool CPickup::ProcessWeaponImpact(CEntity* pParentEntity)
{
	////////////////////////////////
	// ON SHOT COLLECTION DETECTION
	////////////////////////////////

	// ignore clone shots
	if (NetworkInterface::IsGameInProgress())
	{
		netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pParentEntity);

		if (pNetObj && pNetObj->IsClone())
		{
			return false;
		}
	}

	bool bIgnoreImpact = IsAlwaysFixed();

	if (pParentEntity->GetIsTypePed() && GetPickupData()->GetCollectableOnShot())
	{
		// pickup is being collected by shooting
		CPed* pPed = (CPed*)pParentEntity;

		unsigned failureCode = PCC_NONE;

		if (pPed->IsLocalPlayer())
		{
			if (CanCollect(pPed, COLLECT_ONSHOT, &failureCode))
			{
				CollectOnShot(pPed);
				bIgnoreImpact = true;
			}
			else
			{
#if ENABLE_NETWORK_LOGGING
				const char* failureReason = GetCanCollectFailureString(failureCode);

				if (GetNetworkObject())
				{
					NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s BY SHOT: %s\n", GetNetworkObject()->GetLogName(), failureReason);
				}
				/* else
				{
					Vector3 position = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
				} */
#endif // ENABLE_NETWORK_LOGGING
			}
		}
	}

	if (GetPickupData()->GetCanBeDamaged())
	{
		AddExplosion(pParentEntity);
	}

	return bIgnoreImpact;
}

// Create explosion if a valid explosion tag is set. Destroy this pickup after explosion.
void CPickup::AddExplosion(CEntity* explosionOwner)
{
	eExplosionTag tag = GetPickupData()->GetExplosionTag();
	if(tag != EXP_TAG_DONTCARE)
	{
		Vector3 vFireDirection(0.0f, 0.0f, 1.0f);

		if (!explosionOwner)
		{
			if (GetWeaponDamageEntity())
				explosionOwner = GetWeaponDamageEntity();
			else
				explosionOwner = this;
		}

		CExplosionManager::CExplosionArgs explosionArgs(tag, VEC3V_TO_VECTOR3(GetTransform().GetPosition()));

		explosionArgs.m_pEntExplosionOwner = explosionOwner;
		explosionArgs.m_weaponHash = WEAPONTYPE_EXPLOSION;
		explosionArgs.m_bInAir = false;
		explosionArgs.m_vDirection = vFireDirection;
		explosionArgs.m_originalExplosionTag = tag;
		explosionArgs.m_bIsLocalOnly = true;

		CExplosionManager::AddExplosion(explosionArgs);

		if (GetPlacement())
		{
			GetPlacement()->SetPickupHasBeenDestroyed(true);
		}
		else if (GetNetworkObject())
		{
			CPickupDestroyedEvent::Trigger(*this);
		}

		if (!GetPlacement() && GetNetworkObject() && GetNetworkObject()->IsClone())
		{
			NetworkInterface::UnregisterObject(this, true);
		}
		else
		{
			Destroy();
		}
	}
}

// Check if this pickup can be damaged by fire.
// Use the original bound radius to ignore the extra sphere bound manually added for pickup.
bool CPickup::CanBeDamagedByFire(CFire* pFire)
{
	// portable pickups cannot be damaged while attached
	if (IsFlagSet(PF_Portable) && GetIsAttached())
	{
		return false;
	}

	if(pFire && GetPickupData()->GetCanBeDamaged())
	{
		Vector3 vFirePos = VEC3V_TO_VECTOR3(pFire->GetPositionWorld());
		float fFireRadius = pFire->CalcCurrRadius();
		Vector3 vPickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

		if((vFirePos - vPickupPos).Mag() <= (m_originalBoundRadius + fFireRadius))
		{
			return true;
		}
	}

	return false;
}

bool CPickup::CanPhysicalBeDamaged(const CEntity* pInflictor, u32 nWeaponUsedHash, bool bDoNetworkCloneCheck, bool bDisableDamagingOwner NOTFINAL_ONLY(, u32* uRejectionReason)) const
{
	// portable pickups cannot be damaged while attached
	if (IsFlagSet(PF_Portable) && GetIsAttached())
	{
		NOTFINAL_ONLY(if (uRejectionReason) { *uRejectionReason = CPedDamageCalculator::DRR_AttachedPortablePickup; })
		return false;
	}

	return CObject::CanPhysicalBeDamaged(pInflictor, nWeaponUsedHash, bDoNetworkCloneCheck, bDisableDamagingOwner NOTFINAL_ONLY(, uRejectionReason));
}


// Name			:	GetInScope
// Purpose		:	Pickups out of scope are too far away from the player and have to be removed
// Parameters	:	None
// Returns		:   True if in scope of the player
bool CPickup::GetInScope(const CPed& playerPed) const
{
	float scopeRange = IsFlagSet(PF_UseExtendedScope) ? (float)PICKUP_EXTENDED_SCOPE_RANGE : (float)PICKUP_DEFAULT_SCOPE_RANGE;

	// use the placement scope calculation function, so pickup removal is consistent with collection
	return CPickupPlacement::GetInScope(playerPed, VEC3V_TO_VECTOR3(GetTransform().GetPosition()), scopeRange);
}

// Name			:	IsFixed
// Purpose		:	Returns true if the pickup is always fixed and has no physics 
bool CPickup::IsAlwaysFixed() const
{
	if (GetPlacement() && GetPlacement()->GetIsFixed())
	{
		return true;
	}

	if((GetPickupData() && GetPickupData()->GetRotates())
	|| (GetPlacement()  && GetPlacement()->GetShouldRotate()))
	{
		return true;
	}

	if (IsFlagSet(PF_UnderwaterPickup) && IsUnderWater())
	{
		return false;
	}

	if(WasDroppedInWater())
	{
		return false;
	}

	return (GetPickupData()->GetAlwaysFixed());
}

// Name			:	IsFixed
// Purpose		:	Returns true if the pickup never collides with other entities
bool CPickup::IsCollideable() const
{
	return !IsAlwaysFixed();
}

// Name			:	GetRequiresACustomArchetype
// Purpose		:	Pickups collectable on foot or in car and with a collection sphere need to have that sphere added to their collision archetype 
// Parameters	:	None
// Returns		:   True if a custom archetype is required
bool CPickup::GetRequiresACustomArchetype()	const
{
	if (GetPickupData()->GetCollectionRadius() > 0.0f)
	{
		return (GetPickupData()->GetCollectableOnFoot() || GetAllowCollectionInVehicle());
	}

	return false;
}

bool CPickup::GetAllowCollectionInVehicle() const
{
	return GetPickupData()->GetCollectableInCar() || IsFlagSet(PF_AllowCollectionInVehicle);
}

bool CPickup::CanCollectCritical(const CPed* pPed, eCollectionType collectionType, unsigned *failureCode)
{
	if(GetNetworkObject())
	{
		CNetObjPickup* netPickup = SafeCast(CNetObjPickup, GetNetworkObject());
		if(netPickup && !netPickup->IsAllowedForLocalPlayer())
		{
			if (failureCode) *failureCode = PCC_NOT_ALLOWED_FOR_THIS_PLAYER;
			return false;
		}
	}

	if (!IsFlagSet(PF_Initialised))
	{
		if (failureCode) *failureCode = PCC_NOT_INITIALISED;
		return false;
	}

	// if this flag is set we are waiting for a collection approval from another machine
	if (GetIsPendingCollection())
	{
		if (failureCode) *failureCode = PCC_PENDING_COLLECTION;
		return false;
	}

	if (GetHealth() < 0.001f)
	{
		if (failureCode) *failureCode = PCC_DESTROYED;
		return false;
	}

	if (!CanCollectScript(pPed, failureCode))
	{
		return false;
	}

	// don't allow collection when the ped is dead
	if (pPed->GetIsDeadOrDying() || pPed->ShouldBeDead() || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByStealth) || pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_KilledByTakedown))
	{
		if (failureCode) *failureCode = PCC_PED_IS_DEAD;
		return false;
	}

	// Ignore certain checks when doing a team transfer - ie detaching the pickup from one dead player in a vehicle and attaching it to another passenger.
	// The pickup is still attached to the other player at this point.
	if (collectionType != COLLECT_TEAM_TRANSFER)
	{
		// collected pickups are not destroyed immediately, so can trigger collection a number of times before removal
		if (IsFlagSet(PF_Collected))
		{
			if (failureCode) *failureCode = PCC_ALREADY_COLLECTED;
			return false;
		}

		// pickups being carried cannot be collected
		if (GetAttachParent() && SafeCast(CEntity, GetAttachParent())->GetIsTypePed())
		{
			if (failureCode) *failureCode = PCC_ATTACHED;
			return false;
		}
	}

	// Don't allow collection if the pickup is part of a synced scene
	CTask* pActiveTask = GetObjectIntelligence() ? GetObjectIntelligence()->GetTaskActive() : NULL;

	if (pActiveTask && pActiveTask->GetTaskType() == CTaskTypes::TASK_SYNCHRONIZED_SCENE)
	{
		if (failureCode) *failureCode = PCC_IN_SYNCED_SCENE;
		return false;
	}

	// Some player models are restricted from picking anything up.
	if (pPed->IsAPlayerPed() && !CPlayerInfo::CanPlayerPedCollectAnyPickups(*pPed))
	{
		if (failureCode) *failureCode = PCC_INVALID_PED_MODEL;
		return false;
	}

	return true;
}

bool CPickup::CanCollectScript(const CPed* pPed, unsigned *failureCode) const
{
	if (IsFlagSet(PF_CollectionProhibited) || (!pPed->IsNetworkClone() && IsFlagSet(PF_LocalCollectionProhibited)))
	{
		if (failureCode) *failureCode = PCC_COLLECTION_INSTANCE_PROHIBITED;
		return false;
	}

	CNetGamePlayer* pPlayerOwner = pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetPlayerOwner() : NULL;

	if (pPed->IsAPlayerPed())
	{
		if (pPlayerOwner)
		{
			// some pickups can only be collected by certain teams
			if (IsFlagSet(PF_TeamPermitsSet) && 
				(pPlayerOwner->GetTeam() == INVALID_TEAM ||
				!(m_teamPermits & (1<<pPlayerOwner->GetTeam()))))
			{
				if (failureCode) *failureCode = PCC_WRONG_TEAM;
				return false;
			}

			// certain players can be prohibited from collecting certain types of pickup in MP
			PhysicalPlayerIndex playerIndex = pPlayerOwner->GetPhysicalPlayerIndex();

			if (playerIndex != INVALID_PLAYER_INDEX && CPickupManager::IsCollectionProhibited(GetPickupHash(), playerIndex))
			{
				if (failureCode) *failureCode = PCC_COLLECTION_TYPE_PROHIBITED;
				return false;
			}
		}
	
		if (pPed->IsLocalPlayer())
		{
			// scripts can prohibit collection of certain pickup models
			CBaseModelInfo *modelInfo = GetBaseModelInfo();
			if(AssertVerify(modelInfo))
			{
				if (CPickupManager::IsPickupModelCollectionProhibited(modelInfo->GetHashKey()))
				{
					if (failureCode) *failureCode = PCC_PICKUP_MODEL_TYPE_PROHIBITED;
					return false;
				}
			}
	
			if (IsFlagSet(PF_Portable))
			{
				CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();

				if (pPlayerInfo->m_maxPortablePickupsCarried >= 0 && pPlayerInfo->m_numPortablePickupsCarried >= pPlayerInfo->m_maxPortablePickupsCarried)
				{
#if ENABLE_NETWORK_LOGGING
					if (GetNetworkObject())
					{
						if (pPlayerInfo->m_maxPortablePickupsCarried == 0)
						{
							NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s : the player has been blocked from collecting portable pickups by the scripts (m_maxPortablePickupsCarried is 0)\n", GetNetworkObject()->GetLogName());
						}
						else
						{
							NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s : the player is carrying too many pickups (%d) - m_maxPortablePickupsCarried is %d\n", GetNetworkObject()->GetLogName(), pPlayerInfo->m_numPortablePickupsCarried, pPlayerInfo->m_maxPortablePickupsCarried);
						}
					}
#endif
					if (failureCode) *failureCode = PCC_PORTABLE_PICKUPS_BLOCKED;
					return false;
				}

				// the scripts can limit the amount of portable pickups that can be carried
				if (pPlayerInfo && !pPlayerInfo->CanCarryPortablePickupOfType(GetModelIndex(), false))
				{
					if (failureCode) *failureCode = PCC_MAX_PORTABLE_PICKUPS;
					return false;
				}
	
				if (!IsFlagSet(PF_DebugCreated) && pPlayerOwner)
				{
					// portable pickups can only be collected by the participants of the script that created them
					const CScriptEntityExtension* pScriptExtension = GetExtension<CScriptEntityExtension>();

					if (pScriptExtension)
					{
						scriptHandler* pScriptHandler = pScriptExtension->GetScriptHandler();

						if (!m_allowNonScriptParticipantCollection
							&& (!pScriptHandler || 
							!pScriptHandler->GetNetworkComponent() ||
							pScriptHandler->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING ||
							!pScriptHandler->GetNetworkComponent()->IsPlayerAParticipant(*pPlayerOwner)))
						{
							if (failureCode) *failureCode = PCC_NON_PARTICIPANT;
							return false;
						}
					}
					else
					{
						if (failureCode) *failureCode = PCC_DETACHED_FROM_SCRIPT;
						return false;
					}
				}
			}
		}
	}

	return true;
}

// Name			:	AssignCustomArchetype
// Purpose		:	Tries to assign the custom archetype
void CPickup::AssignCustomArchetype()
{
	// make sure current current archetype is loaded (if one exists)
	if ( (GetPhysArch() && GetCurrentPhysicsInst()) || !GetBaseModelInfo()->GetHasBoundInDrawable() )
	{
		// does a custom archetype already exist for this pickup type?
		phArchetypeDamp* pCustomArchetype = CPickupManager::GetCustomArchetype(*this);

		if (!pCustomArchetype)
		{
			// no, so create a new one
			pCustomArchetype = CreateCustomArchetype();	

			// store the archetype for use by other pickups of this type
			CPickupManager::StoreCustomArchetype(*this, pCustomArchetype);
		}

		if (IsFlagSet(PF_UprightOnGround))
		{
			m_heightOffGround = fabs(GetBoundingBoxMin().z);
		}
		else 
		{
			m_heightOffGround = fabs(GetBoundingBoxMin().y);
		}
	
		if (AssertVerify(pCustomArchetype))
		{
			bool bAddPhysics = true;

			// assign the custom archetype to the pickup  
			if (!GetPhysArch())
			{
				// give the pickup a phys inst first if it has none
				phInstGta* pNewInst = rage_new phInstGta(PH_INST_OBJECT);
				Assertf(pNewInst, "Failed to allocate a phInstGta for a new pickup.");
				Matrix34 m = MAT34V_TO_MATRIX34(GetMatrix());
				pNewInst->Init(*pCustomArchetype, m);
				Assert(GetCurrentPhysicsInst() == NULL);
				SetPhysicsInst(pNewInst, false);
				m_originalBoundRadius = GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
			}
			else
			{
				if (AssertVerify(GetCurrentPhysicsInst()->GetArchetype()) &&
					AssertVerify(GetCurrentPhysicsInst()->GetArchetype()->GetBound()) &&
					AssertVerify(pCustomArchetype->GetBound()))
				{
					// If the bound radius changes we need to inform the physics level
					m_originalBoundRadius = GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();
					GetCurrentPhysicsInst()->SetArchetype(pCustomArchetype);
					float fNewRadius = GetCurrentPhysicsInst()->GetArchetype()->GetBound()->GetRadiusAroundCentroid();

					if(CPhysics::GetLevel()->IsInLevel(GetCurrentPhysicsInst()->GetLevelIndex()))
					{
						// Let the level know that we switched archetypes.
						CPhysics::GetLevel()->UpdateObjectArchetype(GetCurrentPhysicsInst()->GetLevelIndex());

						if(NetworkInterface::IsGameInProgress() && GetNetworkObject())
						{
							CNetObjEntity* netEnt = SafeCast(CNetObjEntity, GetNetworkObject());
							if(netEnt && netEnt->GetDisableCollisionCompletely())
							{
								DisableCollision(NULL, true);
							}
						}

						if(m_originalBoundRadius != fNewRadius)
						{
							CPhysics::GetLevel()->UpdateObjectLocationAndRadius(GetCurrentPhysicsInst()->GetLevelIndex(),(Mat34V_Ptr)(NULL));
						}

						// Don't need to add physics because its already been done, probably by CObject::InitPhys
						// When an object requests its collision, it is added to the physics world as soon as collision is loaded
						// as part of the physics request list.
						// Therefore to simplify stuff here we would have to handle the streaming of pickups seperate to CObjects
						bAddPhysics = false;
					}
				}
				else
				{
					return;
				}
			}

			// fix the pickup and place on ground properly if a portable one
			if (IsAlwaysFixed())
			{
				GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE,true);

				m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = false;
			}

			// Make sure RPG rocket falling on ground since it has zero gravity factor by default. 
			if(GetPickupHash() == PICKUP_AMMO_MISSILE_MP)
			{
				pCustomArchetype->SetGravityFactor(1.0f);
			}

			if (!GetIsRetainedByInteriorProxy())
			{
				if (Verifyf(GetOwnerEntityContainer() == NULL, "Pickup %s with a custom archetype is already in the world", GetNetworkObject() ? GetNetworkObject()->GetLogName() : ""))
				{
					// now we can add the pickup to the world
					CGameWorld::Add(this, CGameWorld::OUTSIDE, !bAddPhysics );
					bAddPhysics = false;
					if (GetPlacement() && GetPlacement()->GetRoomHash())
					{
						MoveIntoInterior();
					}
				}

				// pickups without placements have been dropped, so activate physics
				if (!GetPlacement() && !IsAlwaysFixed())
				{
					if (bAddPhysics)
					{
						CPhysical::AddPhysics();
					}
					ActivatePhysics();
				}
			}
			else if (bAddPhysics)
			{
				// we still need to add physics for the pickup, when it comes off the retain list the interior code will not add physics for it
				CPhysical::AddPhysics();
			}

			// Apply the incoming velocity
			if ((!m_StartingLinearVelocity.IsZero() || !m_StartingAngularVelocity.IsZero()) && 
				m_StartingLinearVelocity.x == m_StartingLinearVelocity.x && m_StartingLinearVelocity.x*0.0f == 0.0f &&
				m_StartingAngularVelocity.x == m_StartingAngularVelocity.x && m_StartingAngularVelocity.x*0.0f == 0.0f)
			{
				float linMag = m_StartingLinearVelocity.Mag();
				float angMag = m_StartingAngularVelocity.Mag();
				if (linMag == linMag && linMag*0.0f == 0.0f &&
					angMag == angMag && angMag*0.0f == 0.0f)
				{
					linMag = Clamp(linMag, 0.0f, 20.0f);
					angMag = Clamp(angMag, 0.0f, 20.0f);
					m_StartingLinearVelocity.NormalizeSafe();
					m_StartingAngularVelocity.NormalizeSafe();
					GetCollider()->SetVelocity(m_StartingLinearVelocity * linMag);
					GetCollider()->SetAngVelocity(m_StartingAngularVelocity * angMag);

					// Apply the velocity now to avoid a one-frame freeze
					ScalarV scal;
					scal.Setf(fwTimer::GetTimeStep());
					GetCollider()->UpdatePositionFromVelocity(scal.GetIntrin128Ref());
					CDynamicEntity::SetMatrix(RCC_MATRIX34(GetCurrentPhysicsInst()->GetMatrix()));
					ScalarV orthoError(0.01f);
					Assert(GetCurrentPhysicsInst()->GetMatrix().IsOrthonormal3x3(orthoError));
					Assert(RCC_MATRIX34(GetCurrentPhysicsInst()->GetMatrix()).d.Mag() < 70000.0f);
				}
			}

			// tell the pickup to add itself to any interiors it may be in
			if (NetworkInterface::IsGameInProgress())
			{
				GetPortalTracker()->SetProbeType(CPortalTracker::PROBE_TYPE_NEAR);
			}
			GetPortalTracker()->RequestRescanNextUpdate();

			SetPickupScale();

			// flag that this pickup has modified collision bounds
			m_nPhysicalFlags.bModifiedBounds = true;

			SetFlag(PF_GotCustomArchetype);

			// inform the pickup manager that we are using this custom archetype
			if (IsFlagSet(PF_HasCustomModel))
			{
				CPickupManager::AddRefForExtraCustomArchetype(pCustomArchetype);
			}

			// do an initial collision test to see if the local player is colliding with the pickup as it is spawned. This is to fix the bug where a player may
			// be standing on the pickup but it will not be collected until the player moves and ProcessPreComputeImpacts called
			CPed* pPlayerPed = CGameWorld::FindLocalPlayer();

			if (pPlayerPed)
			{
				Vector3 diff = VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()) - VEC3V_TO_VECTOR3(GetTransform().GetPosition());

				float fCollectionRadius;
#if FPS_MODE_SUPPORTED				
				if(pPlayerPed->IsFirstPersonShooterModeEnabledForPlayer(false, true))
				{
					fCollectionRadius = GetPickupData()->GetCollectionRadiusFirstPerson();
				}
				else
#endif
				{
					fCollectionRadius =  GetPickupData()->GetCollectionRadius();
				}

				if (diff.Mag() <= fCollectionRadius*1.5f)
				{
					// activating the physics will result in ProcessPreComputeImpacts getting called when the physics system detects the collision
					pPlayerPed->ActivatePhysics();
				}
			}
		}
	}
}

// Name			:	MoveIntoInterior
// Purpose		:	Tries to move the pickup into its designated interior
void CPickup::MoveIntoInterior()
{
	const Vector3 vThisPosition = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	CInteriorInst* pIntInst = CInteriorInst::GetInstancedInteriorAtCoords(vThisPosition);

	// if there is at least one instance interior in the area, then start probing for correct interior to insert into
	if (pIntInst && pIntInst->CanReceiveObjects())
	{
		s32 probeRoomIdx;
		CInteriorInst*	pProbedIntInst = NULL;
		CPortalTracker::ProbeForInterior(vThisPosition, pProbedIntInst, probeRoomIdx, NULL, CPortalTracker::SHORT_PROBE_DIST);

		if (pProbedIntInst && pProbedIntInst->CanReceiveObjects())
		{
#if __ASSERT
			if (pProbedIntInst->FindRoomByHashcode(GetPlacement()->GetRoomHash()) == -1)
			{
				Assertf(false, "Pickup %s at (%.2f,%.2f,%.2f) failed to find room in interior %s for placement hash code %d", 
						GetModelName(),
						vThisPosition.x, vThisPosition.y, vThisPosition.z,
						pProbedIntInst->GetRoomName(probeRoomIdx), 
						GetPlacement()->GetRoomHash());						
			}
			else
			{
				// Remove above assert	
				Assertf(pProbedIntInst->GetRoomHashcode(probeRoomIdx) == GetPlacement()->GetRoomHash(), 
						"[SCRIPT BUG] : Pickup @ (%.2f,%.2f,%.2f) using model %s is probably set to wrong room.  Probe resolved room to %s : %d."
						"Its placement hash was %d.",
						vThisPosition.x, vThisPosition.y, vThisPosition.z,
						GetModelName(),
						pProbedIntInst->GetRoomName(probeRoomIdx), 
						probeRoomIdx, 
						GetPlacement()->GetRoomHash());
			}
#endif
			CGameWorld::Remove(this);	// remove from external world

			fwInteriorLocation	interiorLocation = CInteriorInst::CreateLocation( pProbedIntInst, probeRoomIdx );
			CGameWorld::Add(this, interiorLocation);		// put object back into interior in world	

			// pickups without placements have been dropped, so activate physics
			if (!GetPlacement() && !IsFlagSet(PF_Portable))
			{
				ActivatePhysics();
			}

			ClearFlag(PF_PlacedOnGround);
		}
	}
}

void CPickup::HandleFade()
{
	if (HasLifetime() && !fwTimer::IsGamePaused())
	{
		m_lifeTime += fwTimer::GetTimeStepInMilliseconds();

		if (m_lifeTime >= (AMBIENT_PICKUP_LIFETIME - PICKUP_FADEOUT_TIME) && !GetIsPendingCollection())
		{
			if (m_lifeTime < AMBIENT_PICKUP_LIFETIME)
			{
				u8 alpha = static_cast<u8>((((float)(AMBIENT_PICKUP_LIFETIME - m_lifeTime) / (float) PICKUP_FADEOUT_TIME) * 255.0f));

				SetAlpha(alpha);
			}
			else
			{
				SetAlpha(0);

				if (!IsNetworkClone() && !(GetNetworkObject() && GetNetworkObject()->IsPendingOwnerChange()))
				{
					Destroy();
				}
			}
		}
	}
}

// Name			:	CreateCustomArchetype
// Purpose		:	Pickups that have a collection sphere require a custom archetype that is created here. The collision sphere is added to the
//					existing collision archetype.
// Parameters	:	None
// Returns		:   The new archetype
phArchetypeDamp* CPickup::CreateCustomArchetype()
{
	phArchetypeDamp* pNewArchetype = NULL;
	phBound* pNewBound = NULL;

	if (AssertVerify(GetRequiresACustomArchetype()) && AssertVerify(!IsFlagSet(PF_GotCustomArchetype)))
	{
		rage::phBoundSphere* pNewSphere = rage_new rage::phBoundSphere(GetPickupData()->GetCollectionRadius());		

		float fCollectionRadius;
#if FPS_MODE_SUPPORTED				
		fCollectionRadius = Max(GetPickupData()->GetCollectionRadiusFirstPerson(), GetPickupData()->GetCollectionRadius());
#else
		fCollectionRadius = GetPickupData()->GetCollectionRadius();
#endif

		pNewSphere->SetSphereRadius(fCollectionRadius);

		phBound* pExistingBound = GetPhysArch() ? GetPhysArch()->GetBound() : NULL;

#if __ASSERT
		// Verify that we aren't actually just waiting for a physics archetype to stream in
		if(!GetPhysArch())
		{
			// If we DONT have an archetype but we DO have a valid physics dictionary index
			// then we should wait on the archetype streaming in before calling this function
			Assertf(!GetBaseModelInfo()->GetHasBoundInDrawable(),"Trying to create custom archetype before physics has streamed in for pickup");
		}
#endif

		if(!aiVerifyf((!pExistingBound || pExistingBound->GetType() != phBound::BVH),
			"Pickup model has incomptabile bounds for pickup system: cannot be BVH!"))
		{
			// Pretend we have no bound if we have an incompatible type
			pExistingBound = NULL;
		}

		// Set up the include flags here because we need them for the composite bound and the new archetype
		// set flags in the sphere bound to say what type of physics object we wish it to collide with
		u32 pickupSphereTypeFlags = ArchetypeFlags::GTA_PICKUP_TYPE;
		u32 pickupSphereIncludeFlags = 0;

		if (GetPickupData()->GetCollectableOnFoot())
			pickupSphereIncludeFlags |= ArchetypeFlags::GTA_PED_TYPE;

		if (GetPickupData()->GetCollectableInBoat() || GetPickupData()->GetCollectableInPlane() || GetAllowCollectionInVehicle())
			pickupSphereIncludeFlags |= ArchetypeFlags::GTA_VEHICLE_TYPE;

		if (GetPickupData()->GetCollectableOnShot())
		{
			pickupSphereIncludeFlags |= ArchetypeFlags::GTA_WEAPON_TEST;
			pickupSphereIncludeFlags |= ArchetypeFlags::GTA_RAGDOLL_TYPE;  // We want dropped weapons to collide with ragdoll bounds.  In precomputeImpacts we'll filter out contacts between a pickup (intended usage is a dropped weapon) and the ped's arms, head, feet and chest since otherwise bad collision situations can occur
		}

		// Set up the collision flags for the original object that will still collide with the map and possibly other types
		u32 collisionObjectTypeFlags = ArchetypeFlags::GTA_OBJECT_TYPE;
		u32 collisionObjectIncludeFlags = ArchetypeFlags::GTA_OBJECT_INCLUDE_TYPES;

		// Only weapons need to collide with ragdoll bounds.  In precomputeImpacts we'll filter out contacts between a pickup (intended usage is a dropped weapon) and the ped's arms, head, feet and chest since otherwise bad collision situations can occur
		if(GetPickupData()->GetDoesntCollideWithRagdolls())
		{
			collisionObjectIncludeFlags &= ~(ArchetypeFlags::GTA_RAGDOLL_TYPE);
		}
		if(GetPickupData()->GetDoesntCollideWithPeds())
		{
			collisionObjectIncludeFlags &= ~(ArchetypeFlags::GTA_PED_TYPE);
		}
		if(GetPickupData()->GetDoesntCollideWithVehicles())
		{
			// Don't exclude BVH type since that acts as a "ground". The inside of the cargo plane and boats for example. 
			collisionObjectIncludeFlags &= ~(ArchetypeFlags::GTA_VEHICLE_NON_BVH_TYPE | ArchetypeFlags::GTA_WHEEL_TEST);
		}

		if( !m_includeProjectiles )
		{
			collisionObjectIncludeFlags &= ~(ArchetypeFlags::GTA_PROJECTILE_TYPE);
		}

		// Prevent camera shapetests from detecting pickups
		// NOTE: We could use the instance include flags to do this only for 'always fixed' pickups, which disable all impacts in
		// ProcessPreComputeImpacts. However, as the pickup objects are generally small, we really want to prevent the camera from popping
		// around them as a result of occlusion anyway
		// TODO: Modify the bound such that the camera system can avoid clipping whilst ignoring occlusion
		collisionObjectIncludeFlags &= ~(ArchetypeFlags::GTA_CAMERA_TEST);

		if (!pExistingBound)
		{
			// No existing bound: a new sphere bound is all our archetype needs

			// pickups with no initial archetype must always be fixed, otherwise they will fall through the map when activated
			Assert(IsAlwaysFixed());
			
			pNewBound = pNewSphere;

			// create a new archetype to hold this bound
			pNewArchetype = rage_new phArchetypeDamp();

			pNewArchetype->SetMass(1.0f);
			pNewArchetype->SetAngInertia(Vector3(1.0f, 1.0f, 1.0f));
		}
		else
		{
			// We want to use our existing bound, so copy its contents into a new composite bound
			// then add a sphere at the end

			rage::phBoundComposite* pNewComposite = rage_new rage::phBoundComposite();
			int nNumOriginalParts = 0;

			// clone the existing archetype for this pickup
			pNewArchetype = static_cast<phArchetypeDamp*>(GetPhysArch()->Clone());

			Vec3V oldCGOffset(V_ZERO);
			if (GetPhysArch()->GetBound()->GetType() != rage::phBound::COMPOSITE)
			{
				pNewComposite->Init(2);

				phBound* oldBound = GetPhysArch()->GetBound();
				pNewComposite->SetBound(0, oldBound);
				oldCGOffset = oldBound->GetCGOffset();

				pNewComposite->SetBound(1, pNewSphere);
				pNewSphere->Release();

				nNumOriginalParts = 1;
			}
			else
			{
				rage::phBoundComposite* pOldComposite = static_cast<rage::phBoundComposite*>(GetPhysArch()->GetBound());
				nNumOriginalParts = pOldComposite->GetNumBounds();

				pNewComposite->Init(nNumOriginalParts+1);      // Need an extra slot for the sphere

				// Copy the original bounds into the new composite
				for(int i = 0; i < nNumOriginalParts; i++)
				{
					pNewComposite->SetBound(i, pOldComposite->GetBound(i));
					pNewComposite->SetCurrentMatrix(i, pOldComposite->GetCurrentMatrix(i));
					pNewComposite->SetLastMatrix(i, pOldComposite->GetLastMatrix(i));
				}
				oldCGOffset = pOldComposite->GetCGOffset();

				// Now add the sphere at the end
				pNewComposite->SetBound(nNumOriginalParts, pNewSphere);
				pNewSphere->Release();
			}

			pNewComposite->AllocateTypeAndIncludeFlags();

			for(int i = 0; i < nNumOriginalParts; i++)
			{
				pNewComposite->SetIncludeFlags(i,collisionObjectIncludeFlags);
				pNewComposite->SetTypeFlags(i,collisionObjectTypeFlags);
			}

			// Set the sphere type up as a pickup
			const int iSphereBoundIndex = nNumOriginalParts;			

			Assert(pickupSphereIncludeFlags);

			pNewComposite->SetIncludeFlags(iSphereBoundIndex, pickupSphereIncludeFlags);
			pNewComposite->SetTypeFlags(iSphereBoundIndex, pickupSphereTypeFlags);
			pNewComposite->CalculateCompositeExtents();
			pNewComposite->UpdateBvh(true);

			pNewComposite->SetCGOffset(oldCGOffset);

			pNewBound = pNewComposite;
		}

		if (pNewArchetype)
		{
			// setup the archetype
			pNewArchetype->SetBound(pNewBound);
			pNewBound->Release();

			float fCollectionRadius;
#if FPS_MODE_SUPPORTED				
			fCollectionRadius = Max(GetPickupData()->GetCollectionRadiusFirstPerson(), GetPickupData()->GetCollectionRadius());
#else
			fCollectionRadius = GetPickupData()->GetCollectionRadius();
#endif

			// We know the proper radius of the pickup, use that instead of the one the physics computes for us.
			// The physics doesn't know that the pickup is completely contained in the sphere, so it makes a
			// sphere around the bounding box, which is no bueno.
			pNewBound->SetRadiusAroundCentroid(ScalarV(fCollectionRadius));

			int combinedTypeFlags = pickupSphereTypeFlags;
			int combinedIncludeFlags = pickupSphereIncludeFlags;
			
			if(pNewBound && pNewBound->GetType() == phBound::COMPOSITE)
			{
				// Make our archetype collide like a pickup AND an object, so we get all the collisions we need
				combinedTypeFlags |= collisionObjectTypeFlags;
				combinedIncludeFlags |= collisionObjectIncludeFlags;
			}
			// Else we have just made a sphere bound so keep the type and include flags PICKUP_TYPE only
			
			pNewArchetype->SetTypeFlags(combinedTypeFlags);
			pNewArchetype->SetIncludeFlags(combinedIncludeFlags);

			// Add some default damping.
			pNewArchetype->ActivateDamping(phArchetypeDamp::LINEAR_C,Vector3(0.05f,0.05f,0.05f));
			pNewArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V,Vector3(0.05f,0.05f,0.05f));
			pNewArchetype->ActivateDamping(phArchetypeDamp::LINEAR_V2,Vector3(0.05f,0.05f,0.05f));
			pNewArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C,Vector3(40.0f,40.0f,40.0f));
			pNewArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V,Vector3(10.0f,10.0f,10.0f));
			pNewArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V2,Vector3(1.5f,1.5f,1.5f));

			// Apply reasonable amount of damping for golf club. B*1905063
			if(GetPickupData()->GetHash() == PICKUP_WEAPON_GOLFCLUB)
			{
				pNewArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_C,Vector3(0.02f,0.02f,0.02f));
				pNewArchetype->ActivateDamping(phArchetypeDamp::ANGULAR_V,Vector3(0.1f,0.1f,0.1f));
			}
		}
	}

	return pNewArchetype;
}

// Name			:	SetPickupScale
// Purpose		:	Scales the pickup model
bank_float pickupPlacedUpScale = 1.3f;
bank_float pickupNonPlacedUpScale = 1.0f;

void CPickup::SetPickupScale()
{
	float scale = GetPickupData()->GetScale();
	
	if( NetworkInterface::IsGameInProgress() && scale == 1.0f )
	{
		if( GetPlacement() )
			scale *= pickupPlacedUpScale;
		else
			scale *= pickupNonPlacedUpScale;
	}

	ScaleObject(scale);
}

// Name			:	UpdatePortablePickup
// Purpose		:	Portable pickup specific stuff
void CPickup::UpdatePortablePickup()
{
	// make sure it remains hidden
	if (GetIsAttached())
	{
		SetIsVisibleForModule(SETISVISIBLE_MODULE_PICKUP, false, true); 
	} 
	else if (!IsFlagSet(PF_HideWhenDetached))
	{
		SetIsVisibleForModule(SETISVISIBLE_MODULE_PICKUP, true, true); 
	}

	if (IsNetworkClone())
	{
		if (IsFlagSet(PF_WarpToAccessibleLocation))
		{
			ClearFlag(PF_WarpToAccessibleLocation);

			if (!GetIsAttached())
			{
				Expose();
			}
		}
	}
	else
	{
		if (GetIsAttached())
		{
			CEntity* pAttachParent = static_cast<CEntity*>(GetAttachParent());

			if (pAttachParent->GetIsTypePed())
			{
				CPed* pAttachPed = static_cast<CPed*>(pAttachParent);

				UpdateLastAccessibleLocation(*pAttachPed);

				if (!GetNetworkObject() || !GetNetworkObject()->IsPendingOwnerChange())
				{
					// detach portable pickups from dead peds or players not running the script anymore
					if (pAttachPed->GetIsDeadOrDying())
					{
						bool bDetach = true;

						// if the dying ped is sharing a vehicle with another player, pass control of the pickup to him, or if this is our player and
						// we are on the same team, attach the pickup to our player locally
						if (pAttachPed->GetIsInVehicle())
						{
							CVehicle* pMyVehicle = pAttachPed->GetMyVehicle();

							if (pMyVehicle && pMyVehicle->GetStatus() != STATUS_WRECKED)
							{
								CNetGamePlayer* pLocalNetPlayer = NetworkInterface::GetLocalPlayer();
								CPed* pLocalPlayerPed = pLocalNetPlayer ? pLocalNetPlayer->GetPlayerPed() : NULL;

								if (pLocalPlayerPed)
								{
									if (pAttachPed == pLocalPlayerPed)
									{
										// possibly pass control of the pickup to another team mate in the car
										for (int i=0; i<pMyVehicle->GetSeatManager()->GetMaxSeats(); i++)
										{
											CPed* pOccupant = pMyVehicle->GetSeatManager()->GetPedInSeat(i);

											if (pOccupant && pOccupant != pLocalPlayerPed && pOccupant->IsAPlayerPed() && !pOccupant->GetIsDeadOrDying())
											{
												CNetGamePlayer* pRemoteNetPlayer = pOccupant->GetNetworkObject() ? pOccupant->GetNetworkObject()->GetPlayerOwner() : NULL;

												if (pRemoteNetPlayer && (pRemoteNetPlayer->GetTeam() == pLocalNetPlayer->GetTeam() || IsFlagSet(PF_DebugCreated))) 
												{
													netLoggingInterface* pLog = GetNetworkLog();

													if (pLog)
													{
														NetworkLogUtils::WriteLogEvent(*pLog, "MIGRATING_PORTABLE_PICKUP", GetNetworkObject()->GetLogName());
														pLog->WriteDataValue("To player", "%s", pRemoteNetPlayer->GetLogName());
														pLog->WriteDataValue("Reason", "Team mate in vehicle");
													}

													CGiveControlEvent::Trigger(*pRemoteNetPlayer, GetNetworkObject(), MIGRATE_FORCED);
													break;
												}
											}
										}
									}
									else if (pLocalPlayerPed->GetIsInVehicle() && pLocalPlayerPed->GetMyVehicle() == pAttachPed->GetMyVehicle() && !pLocalPlayerPed->GetIsDeadOrDying())
									{
										CNetGamePlayer* pRemoteNetPlayer = pAttachPed->GetNetworkObject() ? pAttachPed->GetNetworkObject()->GetPlayerOwner() : NULL;

										if (pRemoteNetPlayer && (pLocalNetPlayer->GetTeam() == pRemoteNetPlayer->GetTeam() || IsFlagSet(PF_DebugCreated))) 
										{
											if (CanCollect(pLocalPlayerPed, COLLECT_TEAM_TRANSFER))
											{
												DetachPortablePickupFromPed("Ped is dead. Reattaching to local player in car.", false);
												CollectInCar(pLocalPlayerPed);
											}

											bDetach = false;
										}
									}
								}
							}
						}
						
						if (bDetach)
						{
							DetachPortablePickupFromPed("Ped is dead");
						}
					}
					else if (IsFlagSet(PF_DetachWhenLocal))
					{
						DetachPortablePickupFromPed("PF_DetachWhenLocal");
					}
					else if (pAttachPed->IsAPlayerPed())
					{
						CPlayerInfo* pPlayerInfo = pAttachPed->GetPlayerInfo();

						// the scripts can limit the amount of portable pickups that can be carried
						if (AssertVerify(pPlayerInfo) && !pPlayerInfo->CanCarryPortablePickupOfType(GetModelIndex(), true) && pAttachPed->IsLocalPlayer())
						{
							DetachPortablePickupFromPed("CanCarryPortablePickupOfType returned false");
						}
						else if (pAttachPed->GetNetworkObject() && !IsFlagSet(PF_DebugCreated))
						{
							CScriptEntityExtension* pScriptExtension = GetExtension<CScriptEntityExtension>();

							if (pScriptExtension)
							{
								netPlayer* pPlayer = pAttachPed->GetNetworkObject()->GetPlayerOwner();

								if (pPlayer)
								{
									scriptHandler* pScriptHandler = pScriptExtension->GetScriptHandler();

									if (!m_allowNonScriptParticipantCollection
										&& (!pScriptHandler || 
										!pScriptHandler->GetNetworkComponent() ||
										pScriptHandler->GetNetworkComponent()->GetState() != scriptHandlerNetComponent::NETSCRIPT_PLAYING ||
										!pScriptHandler->GetNetworkComponent()->IsPlayerAParticipant(*pPlayer)))
									{
										DetachPortablePickupFromPed("Attach player not a script participant");
									}
								}
							}
							else 
							{
								DetachPortablePickupFromPed("Parent script not running");
							}
						}
					}
				}
			}
		}
		else
		{
			Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

#if __ASSERT
			if (IsAlwaysFixed() && GetOwnedBy() == ENTITY_OWNEDBY_SCRIPT)
			{
				Assertf(IsBaseFlagSet(fwEntity::IS_FIXED), "Portable pickup %s has lost its fixed state", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "");
			}
#endif // __ASSERT

			// code to handle warping the pickup to the nearest accessible position on the network respawn nav mesh
			if (IsFlagSet(PF_WarpToAccessibleLocation))
			{
				netLoggingInterface* pLog = GetNetworkLog();

				if (!IsFlagSet(PF_SearchingForAccessibleLocation))
				{
					if (!CNetRespawnMgr::IsSearchActive())
					{
						SetFlag(PF_SearchingForAccessibleLocation);

						Vector3 diff = m_lastAccessibleLocation - pickupPos;
						float distToLastAccessibleLocation = diff.Mag();
						float radiusForSearch = distToLastAccessibleLocation * 0.7f;

						if (IsFlagSet(PF_DroppedInInterior))
						{
							CNetRespawnMgr::StartSearch(pickupPos, CNetRespawnMgr::ms_vSearchTargetPosNone, CNetRespawnMgr::ms_fSearchMinDistDefault, radiusForSearch, CNetRespawnMgr::FLAG_MAY_SPAWN_IN_INTERIOR|CNetRespawnMgr::FLAG_MAY_SPAWN_IN_EXTERIOR|CNetRespawnMgr::FLAG_IGNORE_Z);
						}
						else
						{
							CNetRespawnMgr::StartSearch(pickupPos, CNetRespawnMgr::ms_vSearchTargetPosNone, CNetRespawnMgr::ms_fSearchMinDistDefault, radiusForSearch, CNetRespawnMgr::FLAG_MAY_SPAWN_IN_EXTERIOR|CNetRespawnMgr::FLAG_IGNORE_Z);
						}
					}
				}
				else if (CNetRespawnMgr::IsSearchComplete()) 
				{
					ClearFlag(PF_WarpToAccessibleLocation);

					if (GetNetworkObject() && pLog)
					{
						NetworkLogUtils::WriteLogEvent(*pLog, "WARPING_PORTABLE_PICKUP", GetNetworkObject()->GetLogName());
					}

					Expose();

					if (CNetRespawnMgr::WasSearchSuccessful())
					{
						if (pLog)
						{
							pLog->WriteDataValue("Search", "successful");
						}

						int numResults = CNetRespawnMgr::GetNumSearchResults();

						Vector3 diff = pickupPos - m_lastAccessibleLocation;

						Vector3 closestPos = m_lastAccessibleLocation;
						float closestDist = diff.Mag2();
						bool bFoundCloserLocation = false;

						for (int i=0; i<numResults; i++)
						{
							Vector3 searchPos;
							float searchHeading;

							CNetRespawnMgr::GetSearchResults(i, searchPos, searchHeading);

							if(IsPositionInsidePrologueArea(searchPos) || IsPositionInsideBlockedBuildingArea(searchPos))
							{
								continue;
							}

							Vector3 diff = searchPos - pickupPos;
							float dist = diff.Mag2();

							if (dist < closestDist)
							{
								closestPos = searchPos;
								closestDist = dist;
								bFoundCloserLocation = true;
							}
						}

						if (pLog)
						{
							if (bFoundCloserLocation)
							{
								pLog->WriteDataValue("New pos", "%f, %f, %f", closestPos.x, closestPos.y, closestPos.z);
							}
							else
							{
								pLog->WriteDataValue("Still use last pos", "%f, %f, %f", closestPos.x, closestPos.y, closestPos.z);
							}
						}

						closestPos.z += m_heightOffGround;

						m_lastAccessibleLocation = closestPos;

						FindSuitablePortablePickupDropLocation(m_lastAccessibleLocation);
					}
					else
					{
						if(pLog)
						{
							pLog->WriteDataValue("Search", "**unsuccessful!**");
						}

						MoveToAccessibleLocation(true);
					}

					ClearFlag(PF_Inaccessible);

					ForceUpdateOfDroppedPortablePickupData();
				}				
			}
		}
	}

	// Activate non fixed portable pickups under water when the player is nearby. 
	// This is a hack to fix a bug where they can sometimes be created by script and end up on the ocean floor asleep.
	if (!IsAlwaysFixed() && 
		!GetIsAttached() && 
		GetCurrentPhysicsInst() && 
		GetCurrentPhysicsInst()->IsInLevel() && 
		CPhysics::GetLevel()->IsInactive(GetCurrentPhysicsInst()->GetLevelIndex()))
	{
		static const float ACTIVATE_RANGE = 100.0f;
		Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		const netPlayer* pUnused = NULL;

		if (NetworkUtils::IsCloseToAnyPlayer(pickupPos, ACTIVATE_RANGE, pUnused))
		{
			if (IsUnderWater())
			{
				ActivatePhysics();
			}
		}
	}
}


void CPickup::UpdateLastAccessibleLocation(CPed& attachPed, bool bForceValid)
{
	static float INACCESSIBLE_TOLERANCE_DISTANCE_SQR = 2.0f * 2.0f;

	// this should never be called on clones, we must use the synced accessible location
	if (IsNetworkClone())
		return;

	if (attachPed.GetIsDeadOrDying())
		return;

	Vector3 navMeshPosition;

	// roads and interiors are always valid locations
	bool bValidLocation = IsFlagSet(PF_LyingOnFixedObject);
		
	if (attachPed.GetNavMeshTracker().GetIsValid())
	{
		navMeshPosition = attachPed.GetNavMeshTracker().GetLastNavMeshIntersection();

		// reject nav mesh positions on the water (this is to fix B* 2830118, when the ped is on a yacht, which has no nav mesh, and the nav mesh on the sea is used instead)
		if (navMeshPosition.z > 0.0f)
		{
			bValidLocation = true;
		}
	}

	bool bInWater = IsInWater(attachPed);
			
	CPhysical* pObject = attachPed.GetGroundPhysical() ? attachPed.GetGroundPhysical() : attachPed.GetClimbPhysical();

	bool bOnFixedObject = pObject && pObject->GetIsFixedFlagSet();
	bool bOnUnfixedObject = pObject && !pObject->GetIsFixedFlagSet();
	bool bOnObjectWithFixedParent = false;
	if(pObject && !bOnFixedObject)
	{
		if(Tunables::GetInstance().TryAccess(CD_GLOBAL_HASH, ATSTRINGHASH("USE_FIXED_PARENT_CHECK_FOR_PORTABLE_PICKUP", 0x32D61595), true))
		{
			CPhysical* attachParent = (CPhysical*)pObject->GetAttachParent();
			if(attachParent && attachParent->GetIsFixedFlagSet())
			{
				bOnObjectWithFixedParent = true;
			}
		}
	}

	// water, roads and interiors are always valid locations
	bool bAlwaysValid = bForceValid || bInWater || (!IsFlagSet(PF_LyingOnUnFixedObject) && GetIsInInterior());

	if (bValidLocation && !bAlwaysValid && attachPed.GetNavMeshTracker().GetIsValid())
	{
		bAlwaysValid = !IsFlagSet(PF_LyingOnUnFixedObject) && (attachPed.GetNavMeshTracker().GetNavPolyData().m_bIsRoad || attachPed.GetNavMeshTracker().GetNavPolyData().m_bInterior);
	}

	bool bUpdateLastAccessibleLocation = true;

	aiTask* pActiveLeafTask = attachPed.GetPedIntelligence()->GetTaskManager()->GetActiveLeafTask(PED_TASK_TREE_PRIMARY);

	if (pActiveLeafTask)
	{
		// if the attach ped is vaulting or climbing a ladder, this is always valid
		switch (pActiveLeafTask->GetTaskType())
		{
		case CTaskTypes::TASK_VAULT:
		case CTaskTypes::TASK_JUMPVAULT:
		case CTaskTypes::TASK_JUMP:
			if (bOnUnfixedObject)
			{
				// not valid if the ped is climbing a non-fixed physical
				break;
			}
			bUpdateLastAccessibleLocation = false;
		case CTaskTypes::TASK_CLIMB_LADDER:
			bAlwaysValid = true;
			break;
		}
	}

	if (!bAlwaysValid)
	{
		// if the attach ped is dropping down off a ledge, this is always valid
		if (attachPed.GetPedIntelligence()->GetTaskManager()->FindTaskByTypeActive(PED_TASK_TREE_PRIMARY, CTaskTypes::TASK_DROP_DOWN))
		{
			bAlwaysValid = true;
		}
	}

	bool bValidCarryingState = true;

	if (!bAlwaysValid)
	{
		// check whether the vehicle the ped is in has left the ground
		if (attachPed.GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
		{
			CVehicle* pVehicle = attachPed.GetMyVehicle();

			if (pVehicle && pVehicle->HasContactWheels() == false)
			{
				bValidCarryingState = false;
			}

			bInWater = pVehicle->GetIsInWater();
		}
		else if (attachPed.GetUsingRagdoll() )
		{
			// the ped's position isn't valid if he is ragdolling (he may be falling down a hill, etc)
			bValidCarryingState = false;
		}
		else if (!attachPed.GetIsStanding() || !attachPed.IsOnGround() || attachPed.GetGroundPos().z < 0.0f)
		{
			// the ped's position isn't valid if he isn't standing/walking/running on the ground
			bValidCarryingState = false;
		}
		else if (bOnUnfixedObject && !bOnObjectWithFixedParent)
		{
			// the ped's position isn't valid if he is standing / climbing on a non-fixed object
			bValidCarryingState = false;
		}
		else if (attachPed.IsAPlayerPed())
		{
			// players scrambling up or down a slope are not allowed to drop a package as it may be inaccessible
			CTaskPlayerOnFoot* pTaskPlayer = static_cast<CTaskPlayerOnFoot*>(attachPed.GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_PLAYER_ON_FOOT));
			if(pTaskPlayer && pTaskPlayer->IsSlopeScrambling())
			{
				bValidCarryingState = false;
			}	
		}

		for(int i = 0; i < SIZE_OF_ALL_YACHT_LOCATIONS; i++)
		{
			if((ALL_YACHT_LOCATIONS[i] - VEC3V_TO_VECTOR3(attachPed.GetTransform().GetPosition())).XYMag2() < (ACCESSIBLE_DISTANCE_FROM_YACHT*ACCESSIBLE_DISTANCE_FROM_YACHT))
			{
				bAlwaysValid = true;
				break;
			}
		}
	}

	if (IsFlagSet(PF_Inaccessible))
	{
		if (bAlwaysValid)
		{
			ClearFlag(PF_Inaccessible);
		}
		else if (bValidLocation && bValidCarryingState)
		{
			// players can get themselves into inaccessible locations, so we need additional checks here
			if (attachPed.IsPlayer() && attachPed.GetNavMeshTracker().GetIsValid())
			{
				// if the player has not moved too far from his last accessible location, treat this as accessible, this is to stop the 
				// accessibility code from temporarily switching to false and not going back to true again (eg when dropping off a small ledge or crossing areas
				// where the navmesh tracker temporarily returns an invalid location)
				Vector3 diff = navMeshPosition - m_lastAccessibleLocation;

				if (diff.Mag2() < INACCESSIBLE_TOLERANCE_DISTANCE_SQR)
				{
					ClearFlag(PF_Inaccessible);
				}
					
				// Use the network respawn nav mesh to determine whether we are back in a reachable location. 
				if (attachPed.GetNavMeshTracker().GetNavPolyData().m_bNetworkSpawn)
				{
					ClearFlag(PF_Inaccessible);
				}
			}
			else
			{
				ClearFlag(PF_Inaccessible);
			}
		}
	}
	else if (!bValidCarryingState)
	{
		SetFlag(PF_Inaccessible);
	}

	if (!IsFlagSet(PF_Inaccessible) && bUpdateLastAccessibleLocation)
	{
		Vector3 newPosition = VEC3V_TO_VECTOR3(attachPed.GetTransform().GetPosition());

		ClearFlag(PF_LastAccessiblePosHasValidGround);

		if (!bInWater)
		{
			const float groundToRootOffset = attachPed.GetCapsuleInfo() ? attachPed.GetCapsuleInfo()->GetGroundToRootOffset() : 1.0f;

			if (bValidLocation)
			{
				if (IsFlagSet(PF_LyingOnFixedObject))
				{
					newPosition.z -= groundToRootOffset;	// take off ped's pelvis height
				}
				else
				{
					newPosition = navMeshPosition;
				}
						
				newPosition.z += m_heightOffGround;		// add on z adjustment
					
				SetFlag(PF_LastAccessiblePosHasValidGround);
			}
			else if (!attachPed.GetUsingRagdoll())
			{
				if (attachPed.GetGroundPos().z > PED_GROUNDPOS_RESET_Z)
				{
					newPosition = attachPed.GetGroundPos();
				}
				else
				{
					newPosition.z -= groundToRootOffset;	// take off ped's pelvis height
				}

				newPosition.z += m_heightOffGround;		// add on z adjustment
			}
		}

		if (!attachPed.GetIsDeadOrDying())
		{
			ClearFlag(PF_LyingOnFixedObject);
			ClearFlag(PF_LyingOnUnFixedObject);

			// the ground physical gets cleared on death, so we need to cache it here
			if (bOnFixedObject || bOnObjectWithFixedParent)
				SetFlag(PF_LyingOnFixedObject);
			else if (bOnUnfixedObject)
				SetFlag(PF_LyingOnUnFixedObject);
		}

		// keep a track of the last accessible position the ped was at
		m_lastAccessibleLocation = newPosition;
	}
}

void CPickup::FindSuitablePortablePickupDropLocation(Vector3& initialPos)
{
	CPickupManager::CPickupPositionInfo dropInfo(initialPos);
	dropInfo.m_MinDist = 1.5f;
	dropInfo.m_bOnGround = true;
	dropInfo.m_pickupHeightOffGround = m_heightOffGround;

	bool bDroppedInWater = false;

	if (IsFlagSet(PF_LyingOnFixedObject))
	{
		// we can't call CalculateDroppedPickupPosition as it only probes for the ground
		dropInfo.m_Pos = initialPos;
	}
	else
	{
		float waterZ = 0.0f;

		if (CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(initialPos, &waterZ, false) && waterZ > initialPos.z)
		{
			dropInfo.m_Pos = initialPos;
			dropInfo.m_Pos.z = waterZ;
			DropInWater(false);
			bDroppedInWater = true;
		}
		else if (!CPickupManager::CalculateDroppedPickupPosition(dropInfo, true))
		{
			// expand the drop radius if we can't find a valid position
			dropInfo.m_MinDist = 4.0f;
			CPickupManager::CalculateDroppedPickupPosition(dropInfo, true);
		}
	}

#if ENABLE_NETWORK_LOGGING
	if (GetNetworkLog())
	{
		GetNetworkLog()->WriteDataValue("Dropped at", "%f, %f, %f", dropInfo.m_Pos.x, dropInfo.m_Pos.y, dropInfo.m_Pos.z);
	}
#endif

	Teleport(dropInfo.m_Pos);

	if (!bDroppedInWater)
	{
		if (g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(GetTransform().GetPosition(), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
		{
			PlaceOnGroundProperly(3.0f, true, m_heightOffGround, GetPlacement()==NULL, IsFlagSet(PF_UprightOnGround));
		}
		else
		{
			SetPlaceOnGround(true, IsFlagSet(PF_UprightOnGround));
		}	
	}
}

void CPickup::ActivatePhysicsOnPortablePickup()
{
	if (GetCurrentPhysicsInst())
		GetCurrentPhysicsInst()->SetInstFlag(phInst::FLAG_NEVER_ACTIVATE, false);

	m_nObjectFlags.bActivatePhysicsAsSoonAsUnfrozen = true;

	CObject::SetFixedPhysics(false, false);

	AddPhysics();
	ActivatePhysics();
}

bool CPickup::IsInWater(CPed& ped) const
{
	bool bInWater = ped.GetIsInWater();

	CVehicle* pPedVehicle = ped.GetIsInVehicle() ? ped.GetMyVehicle() : NULL;

	if (pPedVehicle)
	{
		if (pPedVehicle->GetIsInWater() || pPedVehicle->GetWasInWater() || pPedVehicle->m_nVehicleFlags.bIsDrowning)
		{
			bInWater = true;
		}
	}

	return bInWater;
}

void CPickup::DropInWater(bool bFindWaterLevel)
{
	ActivatePhysicsOnPortablePickup();

	SetDroppedInWater();

	m_nObjectFlags.bFloater = true;

	if (bFindWaterLevel)
	{
		if (!IsFlagSet(PF_UnderwaterPickup))
		{
			Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
			float waterLevel = 0.0f;
			if (CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(pickupPos, &waterLevel, false))
			{
				// Probe upwards to see if there is anything in our way between the pickup's current
				// position and the surface. If there is, we just detach.
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetStartAndEnd(pickupPos, Vector3(pickupPos.x, pickupPos.y, waterLevel));
				probeDesc.SetIsDirected(true);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
				if(!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc))
				{
#if ENABLE_NETWORK_LOGGING
					if (GetNetworkLog())
					{
						GetNetworkLog()->WriteDataValue("Dropped at water surface", "true");
					}
#endif
					pickupPos.z = waterLevel;
					SetPosition(pickupPos);
				}
				else
				{
					ClearFlag(PF_PlaceOnGround);

#if ENABLE_NETWORK_LOGGING
					if (GetNetworkLog())
					{
						GetNetworkLog()->WriteDataValue("Dropped under water", "true");
					}
#endif
				}
			}
		}
		else
		{
#if ENABLE_NETWORK_LOGGING
			if (GetNetworkLog())
			{
				GetNetworkLog()->WriteDataValue("Dropped under water (underwater pickup)", "true");
			}
#endif
		}
	}
}

void CPickup::SetLocalPendingCollection(CPed& pendingCollector, eCollectionType collectionType)
{
	m_pendingCollector = &pendingCollector;
	m_pendingCollectionType = collectionType;

	// hide the pickup when pending collection (ie. pretend it is collected because it is about to be). This is done so that the pickup disappears
	// immediately when someone is trying to collect it).
	Hide();

	for (u32 i=0; i<ms_numPickupsPendingLocalCollection; i++)
	{
		if (ms_pickupsPendingLocalCollection[i] == this)
		{
			Assertf(0, "Pickup %s already on pending collection list", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "");
			return;
		}
	}

	if (AssertVerify(ms_numPickupsPendingLocalCollection<MAX_PENDING_COLLECTIONS))
	{
		ms_pickupsPendingLocalCollection[ms_numPickupsPendingLocalCollection++] = this;
	}
}

void CPickup::ClearPendingCollection(bool bExpose)
{
	m_pendingCollector = NULL;
	m_pendingCollectionType = COLLECT_INVALID;
	m_pendingCollectionTimer = 0;

	if (bExpose && !GetIsAttached())
	{
		Expose();
	}

	for (u32 i=0; i<ms_numPickupsPendingLocalCollection; i++)
	{
		if (ms_pickupsPendingLocalCollection[i] == this)
		{
			for (u32 j=i; j<ms_numPickupsPendingLocalCollection; j++)
			{
				ms_pickupsPendingLocalCollection[j] = ms_pickupsPendingLocalCollection[j+1];
			}

			ms_numPickupsPendingLocalCollection--;
		}
	}
}

// Name			:	Hide
// Purpose		:	Hides the pickup (makes it uncollideable and invisible)
void CPickup::Hide(bool bMakeUncollideable)
{
	SetIsVisibleForModule(SETISVISIBLE_MODULE_PICKUP, false, true); 

	if (bMakeUncollideable)
	{
		SetFlag(PF_DisabledCollision);
		DisableCollision(NULL, true);
	}

	if (!GetIsAttached() && GetNetworkObject() && !GetNetworkObject()->IsClone() && !GetNetworkObject()->IsPendingOwnerChange() && !m_pendingCollector)
	{
		static_cast<CNetObjPickup*>(GetNetworkObject())->SetOverridingRemoteVisibility(true, false, "Hiding pickup");
	}
}

// Name			:	Expose
// Purpose		:	Exposes the pickup again, after Hide() has been called
void CPickup::Expose()
{
	if (!IsFlagSet(PF_HideWhenDetached))
	{
		if (Verifyf(!GetIsAttached(), "%s is being exposed while attached!", GetNetworkObject() ? GetNetworkObject()->GetLogName() : "Pickup"))
		{
			SetIsVisibleForModule(SETISVISIBLE_MODULE_PICKUP, true, true); 

			if (IsFlagSet(PF_DisabledCollision))
			{
				EnableCollision();
				ClearFlag(PF_DisabledCollision);
			}

			if (GetNetworkObject() && !GetNetworkObject()->IsClone() && !GetNetworkObject()->IsPendingOwnerChange() && static_cast<CNetObjPickup*>(GetNetworkObject())->GetOverridingRemoteVisibility())
			{
				static_cast<CNetObjPickup*>(GetNetworkObject())->SetOverridingRemoteVisibility(false, true, "Hiding pickup");
			}
		}
	}
}

void  CPickup::ForceUpdateOfDroppedPortablePickupData()
{
	if (GetNetworkObject() && !IsNetworkClone())
	{
		CPickupSyncTree* pPickupSyncTree = SafeCast(CPickupSyncTree, GetNetworkObject()->GetSyncTree());

		if (pPickupSyncTree)
		{
			pPickupSyncTree->DirtyNode(GetNetworkObject(), *pPickupSyncTree->GetSectorNode());
			pPickupSyncTree->DirtyNode(GetNetworkObject(), *pPickupSyncTree->GetPickupSectorPosNode());

			// forcibly send out an update with the attachment state and new position together, otherwise the position may be sent before the attachment
			// state and not get applied
			GetNetworkObject()->GetSyncTree()->Update(GetNetworkObject(), GetNetworkObject()->GetActivationFlags(), netInterface::GetSynchronisationTime());
			GetNetworkObject()->GetSyncTree()->ForceSendOfSyncUpdateNodes(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetNetworkObject()->GetActivationFlags(), GetNetworkObject());
		}
	}
}

// Checks if pickup is networked local has been dropped from plane and sets the matrix to identity to ensure it isnt left with plane orientation 
// and checks for best position with respect to nearness to buildings and ground and reposition the pickup if necessary
void  CPickup::NetworkSetPositionOfPlaneDroppedPortablePickup(const CPed* pPed, const CVehicle* pVehicle)
{
	if (GetNetworkObject() && !IsNetworkClone() && GetPickupData()->GetIsAirbornePickup())
	{
		static float CAPSULE_PROBE_RADIUS = 15.0f; //same as distance MIN_DIST_FROM_GROUND for now

		float fTestRadius = CAPSULE_PROBE_RADIUS;

		//Test for space that should allow room for this plane to get into
		if(pVehicle && pVehicle->GetBaseModelInfo())
		{
			fTestRadius = 2.0f * pVehicle->GetBaseModelInfo()->GetBoundingSphereRadius();
		}	

		Vector3 vDropPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
		Vector3 vStartProbePos = vDropPos;
		vStartProbePos.z+=MAX_BUILDING_CLIFF_HEIGHT;  //raise probe up about height of highest building in city

		WorldProbe::CShapeTestCapsuleDesc capsuleDesc;
		WorldProbe::CShapeTestHitPoint probeIsect;
		WorldProbe::CShapeTestResults probeResult(probeIsect);
		capsuleDesc.SetCapsule(vStartProbePos, vDropPos, fTestRadius);
		capsuleDesc.SetResultsStructure(&probeResult);
		capsuleDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);
		capsuleDesc.SetIsDirected(true);
		capsuleDesc.SetDoInitialSphereCheck(true);

		static const s32 MAX_NUM_EXCEPTIONS = 2;
		s32 iNumExceptions =0;
		const CEntity* ppExceptions[MAX_NUM_EXCEPTIONS] = { NULL };

		if(pPed)
		{
			ppExceptions[0] = pPed;
			iNumExceptions++;
		}
		if(pVehicle)
		{
			ppExceptions[1] = pVehicle;
			iNumExceptions++;
		}

		if(iNumExceptions >0)
		{
			capsuleDesc.SetExcludeEntities(ppExceptions,iNumExceptions);
		}

		capsuleDesc.SetContext(WorldProbe::LOS_Unspecified);

		if(WorldProbe::GetShapeTestManager()->SubmitTest(capsuleDesc))
		{
			Vector3  vHitPos = VEC3V_TO_VECTOR3(probeResult[0].GetHitPositionV());
			vDropPos.z = vHitPos.z + CAPSULE_PROBE_RADIUS; //lift the pick up above the collision pos

			Assertf(vDropPos.z > WORLDLIMITS_ZMIN && vDropPos.z < WORLDLIMITS_ZMAX, "%s plane pickup being placed out of WORLDLIMITS_Z range %.2f", GetDebugName(), vDropPos.z );
		}

		//Make sure pick up orientates straight e.g. not adopting crashing planes last orientation
		Matrix34 newMat;
		newMat.Identity();
		newMat.d.Set(vDropPos);
		SetMatrix(newMat);
	}
}

bool CPickup::IsUnderWater() const
{
	Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	bool bBelowWater = pickupPos.z < 0.0f;

	if (!bBelowWater)
	{
		float waterZ = 0.0f;

		if (CWaterTestHelper::GetWaterHeightAtPositionIncludingRivers(pickupPos, &waterZ, false))
		{
			bBelowWater = pickupPos.z < waterZ;
		}
	}

	return bBelowWater;
}

bool CPickup::IsPositionInsidePrologueArea(Vector3& pos)
{
	// TODO_UI: This may cause issues with Island X.
	// Check this once we've had some time to test things on the island and update
	// or remove based on what's required.
	const float MAP_LIMIT_WEST_PROLOGUE = 1935.0f;
	const float MAP_LIMIT_EAST_PROLOGUE = 7850.0f;
	const float MAP_LIMIT_NORTH_PROLOGUE = -3561.0f;
	const float MAP_LIMIT_SOUTH_PROLOGUE = -5295.0f;

	if(pos.x > MAP_LIMIT_WEST_PROLOGUE
	&& pos.x < MAP_LIMIT_EAST_PROLOGUE
	&& pos.y > MAP_LIMIT_NORTH_PROLOGUE
	&& pos.y < MAP_LIMIT_SOUTH_PROLOGUE
	&& pos.z > 100.0f)
	{
		return true;
	}
	return false;
}

bool CPickup::IsPositionInsideBlockedBuildingArea(Vector3 pos)
{
	const float POS_Z_BOTTOM  = 214.0f;
	const float POS_Z_TOP	  = 252.0f;
	const float CORNER_Y_ONE = -926.0f;
	const float CORNER_Y_TWO = -1030.0f;
	const float CORNER_X_ONE = -119.0;
	const float CORNER_X_TWO = -200.0f;


	if(pos.z < POS_Z_BOTTOM || pos.z > POS_Z_TOP)
	{
		return false;
	}
	else
	{
		if(pos.y < CORNER_Y_ONE
			&& pos.y > CORNER_Y_TWO
			&& pos.x < CORNER_X_ONE
			&& pos.x > CORNER_X_TWO)
		{
			return true;
		}
	}
	
	return false;
}


bool CPickup::AddExtendedProbeArea(Vector3 pos, float radius)
{
	if(gnetVerifyf(m_ExtendedProbeCount < MAX_NUM_EXTENDED_PROBE_AREAS, "Extended probe area array is full"))
	{
		gnetDebug1("ADDING_EXTENDED_PROBE_AREA - %f, %f, %f with Radius Sqr: %.f", pos.GetX(), pos.GetY(), pos.GetZ(), radius * radius);
		ExtendedProbeAreas newArea(pos, radius);
		m_AllExtendedProbeAreas[m_ExtendedProbeCount] = newArea;
		m_ExtendedProbeCount++;
		return true;
	}
	else
	{
		return false;
	}
}

void CPickup::ClearExtendedProbeAreas()
{
	m_ExtendedProbeCount = 0;

	for(int i = 0; i < MAX_NUM_EXTENDED_PROBE_AREAS; i++)
	{
		m_AllExtendedProbeAreas[i].Reset();
	}
}

// Name			:	CanCollect
// Purpose		:	Returns true if the given ped is allowed to collect the pickup
// Parameters	:	pPed - the ped trying to collect the pickup
//					collectionType - how the pickup is being collected
// Returns		:   True if the given ped is allowed to collect the pickup
bool CPickup::CanCollect(const CPed* pPed, eCollectionType collectionType, unsigned *failureCode) 
{
	// do collection checks for both local and remote peds
	if (!CanCollectCritical(pPed, collectionType, failureCode))
	{
		return false;
	}

	if (pPed->IsNetworkClone())
	{
		return true;
	}

	bool bCanCollect = false;

	CVehicle *pPedVehicle = pPed->GetVehiclePedInside();

	// pickup type: PICKUP_PORTABLE_CRATE_UNFIXED_INAIRVEHICLE_WITH_PASSENGERS can be collected from 4 meters away while in a plane
	// on foor this pickup distance should still be 1.5m. Fail if too far
	if(GetPickupData()->GetHash() == PICKUP_PORTABLE_CRATE_UNFIXED_INAIRVEHICLE_WITH_PASSENGERS)
	{
		if(!pPedVehicle || !(pPedVehicle->InheritsFromHeli() || pPedVehicle->InheritsFromPlane()) || !pPedVehicle->IsInDriveableState())
		{
			static const f32 ON_FOOT_COLLECTION_DIST_SQR = (1.5f * 1.5f);
			f32 distSquare = VEC3V_TO_VECTOR3(this->GetTransform().GetPosition()).Dist2(VEC3V_TO_VECTOR3(pPed->GetTransform().GetPosition()));
			if(distSquare > ON_FOOT_COLLECTION_DIST_SQR)
			{
				if (failureCode) *failureCode = PCC_FAR_FOR_ON_FOOT_COLLECTION;
				return false;
			}
		}
	}

	if (IsFlagSet(PF_DroppedFromLocalPlayer) && pPed->IsLocalPlayer())
	{
		if (failureCode) *failureCode = PCC_DROPPED_FROM_LOCAL_PLAYER;
		return false;
	}

	if (collectionType == COLLECT_ONFOOT)
	{
		if (!GetPickupData()->GetCollectableOnFoot())
		{
			if (failureCode) *failureCode = PCC_COLLECTABLE_IN_VEHICLE;
			return false;
		}

		if (pPedVehicle)
		{
			if (failureCode) *failureCode = PCC_COLLECTABLE_ON_FOOT;
			return false;
		}
	}
	else if (collectionType == COLLECT_INCAR)
	{
		if (!pPedVehicle)
		{
			if (failureCode) *failureCode = PCC_COLLECTABLE_IN_VEHICLE;
			return false;
		}
		else if (!GetPickupData()->GetCollectableInCarByPassengers() && pPedVehicle->GetDriver() != pPed)
		{
			if (failureCode) *failureCode = PCC_NOT_DRIVER;
			return false;
		}
		else
		{
			bool bCanCollect = false;

			switch (pPedVehicle->GetVehicleType())
			{
			case VEHICLE_TYPE_CAR:
			case VEHICLE_TYPE_QUADBIKE:
			case VEHICLE_TYPE_BIKE:
			case VEHICLE_TYPE_BICYCLE:			
			case VEHICLE_TYPE_TRAIN:
			case VEHICLE_TYPE_TRAILER:
			case VEHICLE_TYPE_DRAFT:
				bCanCollect = GetAllowCollectionInVehicle();
				break;
			case VEHICLE_TYPE_PLANE:
			case VEHICLE_TYPE_HELI:
			case VEHICLE_TYPE_AUTOGYRO: 
			case VEHICLE_TYPE_BLIMP:
				bCanCollect = GetPickupData()->GetCollectableInPlane();	
				break;
			case VEHICLE_TYPE_BOAT:
			case VEHICLE_TYPE_SUBMARINE:	
				bCanCollect = GetPickupData()->GetCollectableInBoat();	
				break;
			case VEHICLE_TYPE_SUBMARINECAR: 
				{
					CSubmarineCar* submarineCar = static_cast< CSubmarineCar* >( pPedVehicle );

					if( submarineCar->IsInSubmarineMode() )
					{
						bCanCollect = GetPickupData()->GetCollectableInBoat();	
					}
					else
					{
						bCanCollect = GetPickupData()->GetCollectableInCar();	
					}
					break;
				}
			case VEHICLE_TYPE_AMPHIBIOUS_AUTOMOBILE:
			case VEHICLE_TYPE_AMPHIBIOUS_QUADBIKE:
				{
					bCanCollect = GetPickupData()->GetCollectableInCar();	
				}
				break;
			default:
				Assertf(0, "CPickup::CanCollect: Unrecognised vehicle type");
			}

			if (!bCanCollect)
			{
				if (failureCode) *failureCode = PCC_WRONG_VEHICLE;
				return false;
			}
		}
	}

	// Ignore certain checks when doing a team transfer - ie detaching the pickup from one dead player in a vehicle and attaching it to another passenger.
	// The pickup is still attached to the other player at this point.
	if (collectionType != COLLECT_TEAM_TRANSFER)
	{
		// pickups in a different interior cannot be collected
		if (GetInteriorLocation().GetAsUint32() != pPed->GetInteriorLocation().GetAsUint32())
		{
			if (failureCode) *failureCode = PCC_DIFFERENT_INTERIORS;
			return false;
		}

		// don't allow collection after ped gets on a vehicle
		if (pPed->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_ENTER_VEHICLE) && pPed->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			if (failureCode) *failureCode = PCC_PED_ENTERING_VEHICLE;
			return false;
		}

		// hidden pickups cannot be collected
		if (!GetIsVisibleForModule(SETISVISIBLE_MODULE_PICKUP))
		{
			if (failureCode) *failureCode = PCC_HIDDEN;
			return false;
		}
	}

	// can't collect faded out ambient pickups
	if (!GetPlacement() && m_lifeTime > AMBIENT_PICKUP_LIFETIME)
	{
		if (failureCode) *failureCode = PCC_FADED_OUT_AMBIENT;
		return false;
	}

	// Don't allow collection during stealth kills / takedowns
	CTaskMeleeActionResult* pMeleeActionTask = pPed->GetPedIntelligence()->GetTaskMeleeActionResult();

	if (pMeleeActionTask)
	{
		const CActionResult* pActionResult = pMeleeActionTask ? pMeleeActionTask->GetActionResult() : NULL;

		if(pActionResult && (pActionResult->GetIsAStealthKill() || pActionResult->GetIsATakedown()))
		{
			if (failureCode) *failureCode = PCC_DOING_STEALTH_KILL;
			return false;
		}
	}

	if (IsFlagSet(PF_Portable) && pPed->IsLocalPlayer())
	{
		CPlayerInfo* pPlayerInfo = pPed->GetPlayerInfo();

		// don't allow collection if we have another pending
		if (pPlayerInfo->PortablePickupPending)
		{
			if (GetNetworkObject())
			{
				NetworkInterface::GetObjectManagerLog().Log("** CAN'T COLLECT %s : PortablePickupPending set to true!\n", GetNetworkObject()->GetLogName());
			}
			if (failureCode) *failureCode = PCC_PORTABLE_PICKUP_PENDING;
			return false;
		}
	}

	if(IsFlagSet(PF_RequiresLineOfSight) && pPed->IsLocalPlayer())
	{
		if(!pPed->OurPedCanSeeThisEntity(this))
		{
			return false;
		}
	}

	u32 uNewWeaponHash = GetPickupData()->GetFirstWeaponReward();

	// a weapon is being picked up
	if (uNewWeaponHash != 0)
	{
		if (pPed->IsLocalPlayer() && (collectionType == COLLECT_ONFOOT|| (collectionType == COLLECT_INCAR && IsFlagSet(PF_AllowCollectionInVehicle))) && GetPickupData()->GetManualPickUp())
		{	
			const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uNewWeaponHash);
			if (pWeaponInfo && pPed->GetWeaponManager() && pPed->GetInventory())
			{
				// If we're picking up the same weapon as we're using, don't set the last picked up weapon
				if (pPed->GetWeaponManager()->GetEquippedWeaponHash() != uNewWeaponHash)
				{
					// If the weapon is different from one we already have a weapon of that type in the slot, only add it if the player actively presses for it
					// or we are flagged to do so
					const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeaponBySlot(pWeaponInfo->GetSlot());
					if ((pWeaponItem && pWeaponItem->GetInfo()->GetHash() != pWeaponInfo->GetHash()) || (m_pPickupData->GetRequiresButtonPressToPickup() && !m_pPickupData->GetAutoCollectIfInInventory()))
					{
						const CControl* pControl = pPed->GetControlFromPlayer();

						// Can't pick it up whilst on the phone.
						if(pPed->GetPedIntelligence()->FindTaskActiveByTreeAndType(PED_TASK_TREE_SECONDARY, CTaskTypes::TASK_MOBILE_PHONE) != NULL)
						{
							if (failureCode) *failureCode = PCC_ON_THE_PHONE;
							return false;
						}
						else if(GetPickupHash()==PICKUP_JETPACK)
						{
							if(!pControl->GetPedEnter().IsPressed())
							{
								if (failureCode) *failureCode = PCC_BUTTON_NOT_PRESSED;
								return false;
							}
						}
						else if (!pControl->GetPedCollectPickup().IsPressed())
						{
							if (failureCode) *failureCode = PCC_BUTTON_NOT_PRESSED;
							return false;
						}
					}
				}
			}
		}
	}

	if (AssertVerify(pPed) && 
		AssertVerify(m_pPickupData))
	{
		if (m_pPickupData->GetNumRewards() > 0 &&
			!(ms_shareVehicleWeaponPickupsAmongstPassengers && m_pPickupData->GetShareWithPassengers())) // we may not be able to collect the rewards but our passengers might]
		{
			// allow collection if any of the rewards can be given
			for (u32 i=0; i<m_pPickupData->GetNumRewards(); i++)
			{
				const CPickupRewardData* pReward = m_pPickupData->GetReward(i);

				if (AssertVerify(pReward))
				{
					if (pReward->CanGive(this, pPed))
					{
						bCanCollect = true;
					}
					else if (pReward->PreventCollectionIfCantGive(this, pPed))
					{
						bCanCollect = false;
						break;
					}
				}
			}
		}
		else
		{
			bCanCollect = true;
		}
	}

	bool bEventSent = false;

	if (!bCanCollect)
	{
		if (failureCode) *failureCode = PCC_REWARDS_CANT_BE_GIVEN;
		return false;
	}

	if (NetworkInterface::IsGameInProgress())
	{
		// if the pickup is owned by another machine, we must request collection of it. It will be collected later if the request is granted
		if (GetNetworkObject() && (GetNetworkObject()->IsClone() || GetNetworkObject()->IsPendingOwnerChange()))
		{
			if (ms_numPickupsPendingLocalCollection == MAX_PENDING_COLLECTIONS)
			{
				if (failureCode) *failureCode = PCC_PENDING_COLLECTIONS_EXCEEDED;
				return false;
			}

			if (IsFlagSet(PF_Portable) && pPed->GetPlayerInfo())
			{
				NetworkInterface::GetObjectManagerLog().Log("PortablePickupPending set to true: CanCollect\n");
				Assert(!pPed->GetPlayerInfo()->PortablePickupPending);
				pPed->GetPlayerInfo()->PortablePickupPending = true;
			}

			SetLocalPendingCollection(*const_cast<CPed*>(pPed), collectionType);
			CRequestPickupEvent::Trigger(pPed, this);
			bEventSent = true;
		}
		else if (GetPlacement() && GetPlacement()->GetIsMapPlacement())
		{
			if (!GetPlacement()->GetLocalOnly())
			{
				SetLocalPendingCollection(*const_cast<CPed*>(pPed), collectionType);
				CRequestMapPickupEvent::Trigger(GetPlacement());
				bEventSent = true;
			}
		}
		else if (GetPlacement() && GetPlacement()->GetNetworkObject() && (GetPlacement()->GetNetworkObject()->IsClone() || GetPlacement()->GetNetworkObject()->IsPendingOwnerChange()))
		{
			if (ms_numPickupsPendingLocalCollection == MAX_PENDING_COLLECTIONS)
			{
				if (failureCode) *failureCode = PCC_PENDING_COLLECTIONS_EXCEEDED;
				return false;
			}

			SetLocalPendingCollection(*const_cast<CPed*>(pPed), collectionType);
			CRequestPickupEvent::Trigger(pPed, GetPlacement());
			bEventSent = true;
		}

		if (bEventSent)
		{
			if (failureCode) *failureCode = PCC_CLONE_REQUEST_SENT;
			bCanCollect = false;
		}
	}

	return bCanCollect;
}

// Name			:	CollectOnFoot
// Purpose		:	Called when the pickup is collected on foot. Triggers the on foot collection actions and gives the pickup rewards to the ped.
// Parameters	:	pPed - the ped collecting the pickup
// Returns		:   None
void CPickup::CollectOnFoot(CPed* pPed)
{
	if (AssertVerify(pPed) && 
		AssertVerify(m_pPickupData) && 
		AssertVerify(m_pPickupData->GetCollectableOnFoot()))
	{
		if (m_pPickupData->GetManualPickUp() && pPed->IsLocalPlayer())
		{	
			u32 uNewWeaponHash = m_pPickupData->GetFirstWeaponReward();
			if (uNewWeaponHash !=0)
			{
				const CWeaponInfo* pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uNewWeaponHash);
				if (pWeaponInfo)
				{
					bool bSetLastPickedUpWeapon = true;

					// If we're picking up the same weapon as we're using, don't set the last picked up weapon
					if (pPed->GetWeaponManager()->GetEquippedWeaponHash() == uNewWeaponHash)
					{
						bSetLastPickedUpWeapon = false;
					}
					else
					{
						// If the weapon is different from one we already have a weapon of that type in the slot, only add it if the player actively presses for it
						// or we are flagged to do so
 						const CWeaponItem* pWeaponItem = pPed->GetInventory()->GetWeaponBySlot(pWeaponInfo->GetSlot());
 						if ((pWeaponItem && pWeaponItem->GetInfo()->GetHash() != pWeaponInfo->GetHash()) || m_pPickupData->GetRequiresButtonPressToPickup())
						{
							// If we're switching for a weapon in the same slot we've got equipped we will auto equip, so
							// don't set the quick switch help text
							if (pPed->GetWeaponManager()->GetEquippedWeaponSlot() == pWeaponInfo->GetSlot())
							{
								bSetLastPickedUpWeapon = false;
							}
						}
					}

					// Add the reward and record it, this allows us to switch to the
					// last picked up weapon if we picked it up within the past X seconds
					if (bSetLastPickedUpWeapon)
					{
						pPed->GetPlayerInfo()->SetLastWeaponHashPickedUp(uNewWeaponHash);
					}
				}
			}
		}
	
		u32 numOnFootActions = m_pPickupData->GetNumOnFootActions();

		for (u32 i=0; i<numOnFootActions; i++)
		{
			const CPickupActionData* pAction = m_pPickupData->GetOnFootAction(i);

			if (AssertVerify(pAction))
			{
				pAction->Apply(this, pPed);
			}
		}

		scriptDebugf3(">>> Pickup of type %d collected on foot <<<\n", GetPickupHash());

		GiveRewards(pPed);
		Collect(pPed);
	}
}

// Name			:	CollectInCar
// Purpose		:	Called when the pickup is collected in a vehicle. Triggers the in car collection actions and gives the pickup rewards to the ped.
// Parameters	:	pPed - the ped collecting the pickup
// Returns		:   None
void CPickup::CollectInCar(CPed* pPed)
{
	if (AssertVerify(pPed) && 
		AssertVerify(m_pPickupData) && 
		AssertVerify(m_pPickupData->GetCollectableInBoat() || m_pPickupData->GetCollectableInPlane() || GetAllowCollectionInVehicle()))
	{
		for (u32 i=0; i<m_pPickupData->GetNumInCarActions(); i++)
		{
			const CPickupActionData* pAction = m_pPickupData->GetInCarAction(i);

			if (AssertVerify(pAction))
			{
				pAction->Apply(this, pPed);
			}
		}

		scriptDebugf3(">>> Pickup of type %d collected in car <<<\n", GetPickupHash());

		GiveRewards(pPed);

		if (ms_shareVehicleWeaponPickupsAmongstPassengers && m_pPickupData->GetShareWithPassengers())
		{
			GiveRewardsToPassengers(pPed);
		}

		Collect(pPed);
	}
}

// Name			:	CollectOnShot
// Purpose		:	Called when the pickup is being shot. Triggers the on shot collection actions and gives the pickup rewards to the ped.
// Parameters	:	pPed - the ped collecting the pickup
// Returns		:   None
void CPickup::CollectOnShot(CPed* pPed)
{
	if (AssertVerify(pPed) && 
		AssertVerify(m_pPickupData) && 
		AssertVerify(m_pPickupData->GetCollectableOnShot()))
	{
		for (u32 i=0; i<m_pPickupData->GetNumOnShotActions(); i++)
		{
			const CPickupActionData* pAction = m_pPickupData->GetOnShotAction(i);

			if (AssertVerify(pAction))
			{
				pAction->Apply(this, pPed);
			}
		}

		scriptDebugf3(">>> Pickup of type %d collected on shot <<<\n", GetPickupHash());

		GiveRewards(pPed);
		Collect(pPed);
	}
}

// Name			:	GiveRewards
// Purpose		:	Called when the pickup is being collected. Gives the pickup goodies to the ped.
// Parameters	:	pPed - the ped collecting the pickup
// Returns		:   None
void CPickup::GiveRewards(CPed* pPed)
{
	scriptDebugf3(">>> Rewards given for pickup of type %d <<<\n", GetPickupHash());

	if (AssertVerify(pPed) && 
		AssertVerify(m_pPickupData))
	{
		u32 numRewards = m_pPickupData->GetNumRewards();

		for (u32 i=0; i<numRewards; i++)
		{
			const CPickupRewardData* pReward = m_pPickupData->GetReward(i);
			
			if (AssertVerify(pReward))
			{
				if (CPickupManager::IsSuppressionFlagSet( pReward->GetType() ))
				{
					scriptDebugf3(">>> ** Couldn't give reward type %d, it is supressed! ** \n <<<", pReward->GetType() );
				}
				else if (pReward->CanGive(this, pPed))
				{
					pReward->Give(this, pPed);
				}
				else
				{
					scriptDebugf3(">>> ** Couldn't give reward type %d, CanGive() returned false ** \n <<<", pReward->GetType());
				}
			}
		}
	}
}

void CPickup::GiveRewardsToPassengers(CPed* pPed)
{
	PlayerFlags passengerFlags = 0;

	if (AssertVerify(pPed))
	{
		if (pPed->GetIsInVehicle() && pPed->GetMyVehicle())
		{
			for (u32 i=0; i<pPed->GetMyVehicle()->GetSeatManager()->GetMaxSeats(); i++)
			{
				CPed* pPassenger = pPed->GetMyVehicle()->GetSeatManager()->GetPedInSeat(i);

				if (pPassenger &&
					pPassenger != pPed && 
					pPassenger->GetNetworkObject() && 
					pPassenger->IsAPlayerPed())
				{
					passengerFlags |= (1<<pPassenger->GetNetworkObject()->GetPlayerOwner()->GetPhysicalPlayerIndex());
				}
			}
		}

		if (passengerFlags != 0 &&
			AssertVerify(m_pPickupData) &&
			AssertVerify(m_pPickupData->GetShareWithPassengers()))
		{
			u32 numRewards = m_pPickupData->GetNumRewards();

			for (u32 i=0; i<numRewards; i++)
			{
				const CPickupRewardData* pReward = m_pPickupData->GetReward(i);

				if (AssertVerify(pReward) && 
					!CPickupManager::IsSuppressionFlagSet( pReward->GetType() ) &&
					pReward->CanGiveToPassengers(this))
				{
					CGivePickupRewardsEvent::Trigger(passengerFlags, pReward->GetHash());
				}
			}
		}
	}
}

// Name			:	Collect
// Purpose		:	Called when the pickup is being collected by any method.
// Parameters	:	pPed - the ped collecting the pickup
// Returns		:   None
void CPickup::Collect(CPed* pPed)
{
	scriptDebugf3(">>> Pickup of type %d collected <<<\n", GetPickupHash());

	if (AssertVerify(!IsFlagSet(PF_Collected)))
	{
		SetCollected();

		bool bIsPetrolCan = false;
		const u32 uPickupWeaponHash = GetPickupData()->GetFirstWeaponReward();
		if (uPickupWeaponHash != 0)
		{
			const CWeaponInfo* pPickupWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(uPickupWeaponHash);
			if (pPickupWeaponInfo && pPickupWeaponInfo->GetGroup() == WEAPONGROUP_PETROLCAN)
			{
				bIsPetrolCan = true;
			}
		}

		// Clear the help message
		if(bIsPetrolCan && (strcmp(CHelpMessage::GetMessageText(HELP_TEXT_SLOT_STANDARD), TheText.Get("PU_JER_HLP")) == 0 || strcmp(CHelpMessage::GetMessageText(HELP_TEXT_SLOT_STANDARD), TheText.Get("PU_FER_HLP")) == 0))
		{
			CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, true);
		}

		// Clear the help message
		if(GetPickupData()->GetHash() == PICKUP_JETPACK && strcmp(CHelpMessage::GetMessageText(HELP_TEXT_SLOT_STANDARD), TheText.Get("PU_JET_HLP")) == 0)
		{
			CHelpMessage::Clear(HELP_TEXT_SLOT_STANDARD, true);
		}

		if (m_pPlacement)
		{
			// inform the manager
			CPickupManager::RegisterPickupCollection(m_pPlacement, pPed);
		}
		else 
		{
			// Add network script event for the pickup event.
			if(pPed && pPed->IsPlayer() && !IsFlagSet(PF_Portable))
			{
				u32 modelHash = 0;

				CBaseModelInfo *modelInfo = GetBaseModelInfo();
				if(AssertVerify(modelInfo))
				{
					modelHash = modelInfo->GetHashKey();
				}

				int objectId = NETWORK_INVALID_OBJECT_ID;
				if (GetNetworkObject())
				{
					objectId = GetNetworkObject()->GetObjectID();
				}
				else
				{
					CScriptEntityExtension* pScriptExtension = GetExtension<CScriptEntityExtension>();
					if (pScriptExtension && pScriptExtension->GetScriptInfo())
					{
						objectId = pScriptExtension->GetScriptInfo()->GetObjectId();
					}
				}

				if (pPed->GetNetworkObject())
				{
					// *** THIS IS NOT A NETWORK EVENT!! Should be on AI queue ***
					CEventNetworkPlayerCollectedAmbientPickup pickupEvent(*pPed->GetNetworkObject()->GetPlayerOwner(), 
						GetPickupHash(), 
						objectId, 
						GetAmount(), 
						modelHash, 
						IsFlagSet(PF_PlayerGift),
						IsFlagSet(PF_DroppedByPed),
						(GetAmountCollected() > 0) ? GetAmountCollected() : GetAmount(),
						 CTheScripts::GetGUIDFromEntity(*this));
					GetEventScriptNetworkGroup()->Add(pickupEvent);
				}
				else
				{
					// *** THIS IS NOT A NETWORK EVENT!! Should be on AI queue ***
					CEventNetworkPlayerCollectedAmbientPickup pickupEvent(GetPickupHash(), 
						objectId, 
						GetAmount(), 
						modelHash, 
						IsFlagSet(PF_PlayerGift), 
						IsFlagSet(PF_DroppedByPed),
						(GetAmountCollected() > 0) ? GetAmountCollected() : GetAmount(),
						CTheScripts::GetGUIDFromEntity(*this));
					GetEventScriptNetworkGroup()->Add(pickupEvent);
				}
			}

			// some pickups are attached to the ped after collection
			if (IsFlagSet(PF_Portable))
			{
				AttachPortablePickupToPed(pPed, "Local collection");
			}
			else if (!NetworkUtils::IsNetworkCloneOrMigrating(this)) // clones will be cleared up by their owner
			{
				// there is no placement for this pickup, so it destroys itself
				Destroy();
			}
			else if (GetNetworkObject())
			{
				SafeCast(CNetObjPickup, GetNetworkObject())->DestroyWhenLocal();
			}
		}
	}
}

void CPickup::SetRemotePendingCollection(CPed& ped)
{
	static const unsigned REMOTE_PENDING_COLLECTION_TIMER = 2000;

	if (m_pendingCollectionType == COLLECT_INVALID)
	{
		m_pendingCollectionType = COLLECT_REMOTE;
		m_pendingCollector = &ped;

		// hide the pickup when pending collection (ie. pretend it is collected because it is about to be). This is done so that the pickup disappears
		// immediately when someone is trying to collect it).
		Hide();
	}

	if (m_pendingCollectionType == COLLECT_REMOTE)
	{
		m_pendingCollectionTimer = REMOTE_PENDING_COLLECTION_TIMER;
	}
}

// Name			:	ProcessCollectionResponse
// Purpose		:	Called from a network event when we get a reply for a collection request from the machine which controls the pickup
// Parameters	:	bCollected - if true, collection was successful
void CPickup::ProcessCollectionResponse(const netPlayer* fromPlayer, bool bCollected)
{
	CPed* pCollector = m_pendingCollector.Get();
	eCollectionType collectionType = m_pendingCollectionType;
	bool bMapPickup = GetPlacement() ? GetPlacement()->GetIsMapPlacement() : false;

	CNetGamePlayer* pCurrOwner = GetNetworkObject() ? GetNetworkObject()->GetPlayerOwner() : NULL;

	// if collection is rejected but the pickup has migrated, try again with the new owner
	if (!bMapPickup && !bCollected && fromPlayer && pCurrOwner && pCurrOwner != fromPlayer && !pCurrOwner->IsLocal() && m_pendingCollector)
	{
		CRequestPickupEvent::Trigger(m_pendingCollector, this);
		return;
	}

	// have to clear these here, because the collection will destroy the pickup, which will call CNetObjPickup::CanDelete. This will fail because
	// it will still think the pickup is pending collection. 
	ClearPendingCollection();

	if (IsFlagSet(PF_Portable) && pCollector && pCollector->GetPlayerInfo())
	{
		if (pCollector->GetPlayerInfo()->PortablePickupPending)
		{
			NetworkInterface::GetObjectManagerLog().Log("PortablePickupPending set to false: process collection response\n");
		}

		pCollector->GetPlayerInfo()->PortablePickupPending = false;
	}

	// the pickup may already be collected by our player if we received an attachment update for it before the request collection event response
	if (bCollected && !IsFlagSet(PF_Collected) && pCollector)
	{
		switch (collectionType)
		{
			case COLLECT_ONFOOT:
				CollectOnFoot(pCollector);
				break;
			case COLLECT_INCAR:
				CollectInCar(pCollector);
				break;
			case COLLECT_ONSHOT:
				CollectOnShot(pCollector);
				break;
			default:
				Assert(0);
		}
	}
}

void CPickup::OnPedAttachment(CPed* pPed)
{
#if GTA_REPLAY
	if(CReplayMgr::IsReplayInControlOfWorld())
		return;
#endif // GTA_REPLAY

	SetFlag(PF_Collected);
	ClearFlag(PF_Inaccessible);
	ClearFlag(PF_LastAccessiblePosHasValidGround);
	ClearFlag(PF_LyingOnFixedObject);
	ClearFlag(PF_LyingOnUnFixedObject);
	ClearFlag(PF_WarpToAccessibleLocation);
	ClearFlag(PF_SearchingForAccessibleLocation);

	netLoggingInterface* pLog = GetNetworkLog();

	if (pLog && GetNetworkObject() && IsFlagSet(PF_Portable))
	{
		NetworkLogUtils::WriteLogEvent(*pLog, "ATTACHING_PORTABLE_PICKUP", GetNetworkObject()->GetLogName());
		pLog->WriteDataValue("Attached to", "%s", pPed->GetNetworkObject() ? pPed->GetNetworkObject()->GetLogName() : "??");
	}

	// assume that the location the ped is in starts as a valid location
	if (!IsNetworkClone())
	{
		UpdateLastAccessibleLocation(*pPed, true);
	}

	if (GetPickupData()->GetInvisibleWhenCarried())
	{
		Hide(true);
	}

	if (pPed->IsAPlayerPed())
	{
		if (!pPed->IsNetworkClone() && pPed->GetPlayerInfo())
		{
			pPed->GetPlayerInfo()->PortablePickupCollected(GetModelIndex());
		}

		CScriptEntityExtension* pScriptExtension = GetExtension<CScriptEntityExtension>();

		if (pScriptExtension)
		{
			u32 modelHash = 0;

			CBaseModelInfo *modelInfo = GetBaseModelInfo();
			if(AssertVerify(modelInfo))
			{
				modelHash = modelInfo->GetHashKey();
			}

			if (pPed->GetNetworkObject() && GetNetworkObject())
			{
				CEventNetworkPlayerCollectedPortablePickup pickupEvent(*pPed->GetNetworkObject()->GetPlayerOwner(), pScriptExtension->GetScriptInfo()->GetObjectId(), GetNetworkObject()->GetObjectID(), modelHash);
				GetEventScriptNetworkGroup()->Add(pickupEvent);
			}
			else
			{
				CEventNetworkPlayerCollectedPortablePickup pickupEvent(pScriptExtension->GetScriptInfo()->GetObjectId(), modelHash);
				GetEventScriptNetworkGroup()->Add(pickupEvent);
			}	
		}
	}


	// If we are attaching the pickup from the local player, inform the network object, which keeps track of pending attachments.
	// This is to avoid the network object immediately detaching the pickup if it has no pending pickup info
	if (pPed->IsLocalPlayer())
	{
		CNetObjPickup* pPickupNetObj = static_cast<CNetObjPickup*>(GetNetworkObject());

		if (pPickupNetObj && pPed->IsLocalPlayer())
		{
			pPickupNetObj->PortablePickupLocallyAttached();
		}
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasPortablePickupAttached, true);

	ClearDroppedInWater();

	ClearFlag(PF_DroppedInInterior);

	m_pendingCarrier = NULL;

	m_nObjectFlags.bFloater = false;

	if (GetNetworkObject() && !IsNetworkClone())
	{
		// forcibly send out an update with the attachment state a.s.a.p
		GetNetworkObject()->GetSyncTree()->Update(GetNetworkObject(), GetNetworkObject()->GetActivationFlags(), netInterface::GetSynchronisationTime());
		GetNetworkObject()->GetSyncTree()->ForceSendOfSyncUpdateNodes(SERIALISEMODE_FORCE_SEND_OF_DIRTY, GetNetworkObject()->GetActivationFlags(), GetNetworkObject());
	}
}

void CPickup::OnPedDetachment(CPed* pPed)
{
	ClearFlag(PF_Collected);
	ClearFlag(PF_DetachWhenLocal);
	ClearFlag(PF_PlacedOnGround);
	ClearFlag(PF_LastAccessiblePosHasValidGround);

	// make sure the pickup is not still set invisible by the gameplay module (this can happen when attached to the local player who is sniping)
	SetIsVisibleForModule(SETISVISIBLE_MODULE_GAMEPLAY, true);
	// make sure the pickup is not still set invisible by the respawn module (this can happen when being detached from a respawning player)
	SetIsVisibleForModule(SETISVISIBLE_MODULE_RESPAWN, true);
	// make sure the pickup is not still set invisible by the script module (this can happen when being detached from an invisible player)
	if (!IsFlagSet(PF_HiddenByScript))
	{
		SetIsVisibleForModule(SETISVISIBLE_MODULE_SCRIPT, true);
	}

	if (GetPickupData()->GetInvisibleWhenCarried() && !IsFlagSet(PF_WarpToAccessibleLocation))
	{
		Expose();
	}

	if (pPed->IsAPlayerPed())
	{
		if (!pPed->IsNetworkClone() && pPed->GetPlayerInfo())
		{
			pPed->GetPlayerInfo()->PortablePickupDropped(GetModelIndex());
		}

		CScriptEntityExtension* pScriptExtension = GetExtension<CScriptEntityExtension>();

		if (pScriptExtension && pScriptExtension->GetScriptInfo())
		{
			if (pPed->GetNetworkObject() && GetNetworkObject())
			{
				CEventNetworkPlayerDroppedPortablePickup pickupEvent(*pPed->GetNetworkObject()->GetPlayerOwner(), pScriptExtension->GetScriptInfo()->GetObjectId(), GetNetworkObject()->GetObjectID());
				GetEventScriptNetworkGroup()->Add(pickupEvent);
			}
			else
			{
				CEventNetworkPlayerDroppedPortablePickup pickupEvent(pScriptExtension->GetScriptInfo()->GetObjectId());
				GetEventScriptNetworkGroup()->Add(pickupEvent);
			}	
		}
	}

	if (IsNetworkClone())
	{
		// hack to get clones to go to the right position after detachment. This is to fix 370348
		GetNetworkObject()->GetNetBlender()->SetLastSyncMessageTime(NetworkInterface::GetNetworkTime());
	}

	pPed->SetPedConfigFlag(CPED_CONFIG_FLAG_HasPortablePickupAttached, false);
}

bool CPickup::PlaceOnGroundProperly(float fMaxRange, bool bAlign, float UNUSED_PARAM(heightOffGround), bool bIncludeWater, bool bUpright, bool *pInWater, bool bIncludeObjects, bool useExtendedProbe)
{
	bool success = false;
	bool inWater = false;

	if (bIncludeWater && !pInWater)
	{
		pInWater = &inWater;
	}

	if (GetIsInInterior() || IsFlagSet(PF_LyingOnFixedObject))
	{
		bIncludeObjects = true;
	}

	if (bIncludeWater && IsFlagSet(PF_UnderwaterPickup))
	{
		bIncludeWater = false;
	}

	useExtendedProbe = false;
	Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());
	for(int i = 0; i < m_ExtendedProbeCount; i++)
	{
		if(gnetVerifyf(i < MAX_NUM_EXTENDED_PROBE_AREAS, "Extended probe count(%d) exceeded maximum allowed(%d)", m_ExtendedProbeCount, MAX_NUM_EXTENDED_PROBE_AREAS))
		{
			Vector3 diff = pickupPos - m_AllExtendedProbeAreas[i].m_Position;
			if(diff.Mag2() < m_AllExtendedProbeAreas[i].m_RadiusSqrd)
			{
				useExtendedProbe = true;
				gnetDebug1("PICKUP_USES_EXTENDED_PROBE - %s Because of Area pos: %f, %f, %f. Area Radius Sqr: %.f", GetLogName(), m_AllExtendedProbeAreas[i].m_Position.GetX(), m_AllExtendedProbeAreas[i].m_Position.GetY(), m_AllExtendedProbeAreas[i].m_Position.GetZ(),  m_AllExtendedProbeAreas[i].m_RadiusSqrd);
				break;
			}
		}
	}

	success = CObject::PlaceOnGroundProperly(fMaxRange, bAlign, m_heightOffGround, bIncludeWater, bUpright, pInWater, bIncludeObjects, useExtendedProbe);

	if (success)
	{
		// B*3823574 - No idea if this is correct or not but sometimes pickups have been intentionally removed from the physics level and never added back in.
		// seems like a network or general pickup issue, we'll just hack this in here and hope if fixes the bug and doesn't break anything else
		if( GetCurrentPhysicsInst() &&
			!GetCurrentPhysicsInst()->IsInLevel() )
		{
			AddPhysics();
		}

		if (IsAlwaysFixed())
		{
			// fixed pickups become unfixed in water so they can float to the surface
			if (bIncludeWater && pInWater && *pInWater)
			{
				DropInWater();
			}
		}
		else
		{
			// Active physics after placing the pickup on the ground. This fixes the pickup floating in the air since the bounding box size is bigger than the object's actual size.
			ActivatePhysics();
		}
	}

#if ENABLE_NETWORK_LOGGING
	netLoggingInterface* pLog = GetNetworkLog();

	if (pLog && GetNetworkObject())
	{
		NetworkLogUtils::WriteLogEvent(*pLog, "PICKUP_PLACE_ON_GROUND", GetNetworkObject()->GetLogName());

		Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

		if (success)
		{
			pLog->WriteDataValue("Placed at", "%f, %f, %f", pickupPos.x, pickupPos.y, pickupPos.z);

			if (WasDroppedInWater())
			{
				pLog->WriteDataValue("In water", "true");
			}
		}
		else
		{
			pLog->WriteDataValue("**FAILED**", "%f, %f, %f", pickupPos.x, pickupPos.y, pickupPos.z);
			pLog->WriteDataValue("Include water", "%s", bIncludeWater?"TRUE":"FALSE");
		}
	}
#endif // ENABLE_NETWORK_LOGGING

	return success;
}

void CPickup::MoveToAccessibleLocation(bool bUseLastLocation)
{
	// the maximum distance a portable pickup is allowed to travel away from its last accessible position before a new one is found
	static const float MAX_INACCESSIBLE_RANGE_SQR = 40.0f*40.0f;

	Vector3 pickupPos = VEC3V_TO_VECTOR3(GetTransform().GetPosition());

	Vector3 diff = pickupPos - m_lastAccessibleLocation;

	netLoggingInterface* pLog = GetNetworkLog();

	if (pLog)
	{
		pLog->WriteDataValue("MoveToAccessibleLocation", "true");
	}

	if (bUseLastLocation || diff.Mag2() < MAX_INACCESSIBLE_RANGE_SQR)
	{
		if (pLog)
		{
			pLog->WriteDataValue("Using last accessible pos", "True");
		}

		bool bFoundGround = IsFlagSet(PF_LastAccessiblePosHasValidGround);

		if (!IsFlagSet(PF_LastAccessiblePosHasValidGround))
		{
			float groundZ = WorldProbe::FindGroundZFor3DCoord(TOP_SURFACE, m_lastAccessibleLocation.x, m_lastAccessibleLocation.y, m_lastAccessibleLocation.z+1.5f, &bFoundGround);

			if (bFoundGround)
			{
				if(pLog)
				{
					pLog->WriteDataValue("Found ground", "%f", groundZ);
				}

				m_lastAccessibleLocation.z = groundZ + m_heightOffGround;
			}
		}

		FindSuitablePortablePickupDropLocation(m_lastAccessibleLocation);
	
		ClearFlag(PF_Inaccessible);
	}
	else
	{
		SetFlag(PF_WarpToAccessibleLocation);
		ClearFlag(PF_SearchingForAccessibleLocation);

		// hide the pickup until it is repositioned
		Hide();

		if (pLog)
		{
			pLog->WriteDataValue("Searching for new accessible pos", "True");
		}
	}
}

#if ENABLE_NETWORK_LOGGING
const char *CPickup::GetCanCollectFailureString(unsigned failureCode)
{
	switch (failureCode)
	{
	case PCC_NONE:
		Assertf(0, "CPickup::CanCollect failed to return a valid failure code");
		return "Not specified";		
	case PCC_COLLECTABLE_ON_FOOT:
		return "Pickup only collectable on foot";		
	case PCC_COLLECTABLE_IN_VEHICLE:
		return "Pickup only collectable in a vehicle";
	case PCC_NOT_DRIVER:	
		return "The ped is not the driver of the vehicle";
	case PCC_WRONG_VEHICLE:	
		return "This type of vehicle is not permitted to collect the pickup";
	case PCC_ALREADY_COLLECTED:	
		return "The pickup is already flagged as collected";
	case PCC_PED_ENTERING_VEHICLE:	
		return "The ped is entering a vehicle";
	case PCC_FADED_OUT_AMBIENT:	
		return "The ambient pickup has faded out";
	case PCC_PED_IS_DEAD:		
		return "The ped is dead";
	case PCC_PENDING_COLLECTION:	
		return "The pickup is pending collection";
	case PCC_HIDDEN:	
		return "The pickup is hidden";
	case PCC_WRONG_TEAM:					
		return "The player is on the wrong team";
	case PCC_PORTABLE_PICKUP_PENDING:	
		return "The player is already pending a portable pickup collection";
	case PCC_PORTABLE_PICKUPS_BLOCKED:	
		return "The player has been prevented from collecting portable pickups by the script (SET_MAX_NUM_PORTABLE_PICKUPS_CARRIED_BY_PLAYER)";
	case PCC_MAX_PORTABLE_PICKUPS:	
		return "The player is carrying the maximum number of portable pickups allowed";
	case PCC_NON_PARTICIPANT:	
		return "The player is not a script participant";
	case PCC_DETACHED_FROM_SCRIPT:	
		return "The pickup has been unregistered from the script that created it";
	case PCC_COLLECTION_INSTANCE_PROHIBITED:
		return "Collection of this individual pickup has been blocked by the script (PREVENT_COLLECTION_OF_PORTABLE_PICKUP)";		
	case PCC_COLLECTION_TYPE_PROHIBITED:		
		return "Collection of this type of pickup has been blocked by the script (SET_PLAYER_PERMITTED_TO_COLLECT_PICKUPS_OF_TYPE)";
	case PCC_PICKUP_MODEL_TYPE_PROHIBITED:
		return "Collection of pickups with this model have been blocked by the script (SET_LOCAL_PLAYER_PERMITTED_TO_COLLECT_PICKUPS_WITH_MODEL)";
	case PCC_ATTACHED:			
		return "The pickup is attached";
	case PCC_WEAPON_SWITCHING_BLOCKED:
		return "The ped is temporarily blocking weapon switching";
	case PCC_BUTTON_NOT_PRESSED:
		return "The pickup requires a button press to pickup and the button is not pressed";
	case PCC_REWARDS_CANT_BE_GIVEN:
		return "The pickup rewards can't be given";
	case PCC_CLONE_REQUEST_SENT:			
		return "The pickup is a clone: a collection request has been sent";
	case PCC_MAP_REQUEST_SENT:			
		return "The pickup is a map pickup: a collection request has been sent";
	case PCC_DOING_STEALTH_KILL:
		return "The player is doing stealth kill or takedown.";
	case PCC_IN_SYNCED_SCENE:			
		return "The pickup is part of a synchronised scene";
	case PCC_ON_THE_PHONE:
		return "The player is on the phone";
	case PCC_DROPPED_FROM_LOCAL_PLAYER:
		return "The pickup has just been dropped by the local player";
	case PCC_NOT_INITIALISED:
		return "The pickup has not been initialised";
	case PCC_DIFFERENT_INTERIORS:
		return "The pickup is in a different interior";
	case PCC_DESTROYED:
		return "The pickup is destroyed";
	case PCC_INVALID_PED_MODEL:
		return "The ped model for the player is invalid";
	case PCC_PENDING_COLLECTIONS_EXCEEDED:
		return "The max number of pending collections has been exceeded";
	case PCC_NOT_ALLOWED_FOR_THIS_PLAYER:
		return "Script blocked local player from collecting this pickup";
	case PCC_FAR_FOR_ON_FOOT_COLLECTION:
		return "Too far for on foot collection for this type of pickup";
	default:
		Assertf(0, "Unrecognised CanCollect failure code %d", failureCode);
	}

	return 0;
}
#endif // ENABLE_NETWORK_LOGGING

#if __DEV
template<> void fwPool<CPickup>::PoolFullCallback() 
{
	s32 size = GetSize();
	int iIndex = 0;
	while(size--)
	{
		CPickup* pPickup = GetSlot(size);
		if(pPickup)
		{
			if (pPickup->GetNetworkObject())
			{
				Displayf("%i, \"%s\", Pos: (%.2f,%.2f,%.2f), Net obj: %s, Clone: %s, Pending owner change: %s, Placement: %s, Lifetime: %u, Destroyed: %s",
					iIndex,
					pPickup->GetPickupData()->GetName(),
					VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()).x, 
					VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()).y, 
					VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()).z,
					pPickup->GetNetworkObject()->GetLogName(),
					pPickup->GetNetworkObject()->IsClone() ? "true" : "false",
					pPickup->GetNetworkObject()->IsPendingOwnerChange() ? "true" : "false",
					pPickup->GetPlacement() ? "true" : "false",
					pPickup->GetLifeTime(),
					pPickup->IsFlagSet(CPickup::PF_Destroyed) ? "true" : "false");

			}
			else
			{
				Displayf("%i, \"%s\", Pos: (%.2f,%.2f,%.2f), Placement: %s, Lifetime: %u, Destroyed: %s",
					iIndex,
					pPickup->GetPickupData()->GetName(),
					VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()).x, 
					VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()).y, 
					VEC3V_TO_VECTOR3(pPickup->GetTransform().GetPosition()).z,
					pPickup->GetPlacement() ? "true" : "false",
					pPickup->GetLifeTime(),
					pPickup->IsFlagSet(CPickup::PF_Destroyed) ? "true" : "false");
			}
		}
		else
		{
			Displayf("%i, NULL pickup", iIndex);
		}
		iIndex++;
	}
}
#endif // __DEV


// Sets rendering flags for the pick up.
void CPickup::SetForceAlphaAndUseAmbientScale()
{
	bool bRenderDeferred = false;

	CPickupPlacement *pPickupPlacement = GetPlacement();
	if(pPickupPlacement)
	{
		bRenderDeferred = pPickupPlacement->GetForceDeferredModel();
	}

	SetForceAlphaAndUseAmbientScaleTraverseHierarchy(static_cast<CEntity*>(this), bRenderDeferred);
}


void CPickup::SetUseLightOverrideForGlow()
{
	SetUseLightOverride(true);
	SetUseLightOverrideTraverseHierarchy(static_cast<CEntity*>(GetChildAttachment()), true);
	SetUseLightOverrideTraverseHierarchy(static_cast<CEntity*>(GetSiblingAttachment()), true);
}


void CPickup::ClearUseLightOverrideForGlow()
{
	SetUseLightOverride(false);
	SetUseLightOverrideTraverseHierarchy(static_cast<CEntity*>(this), false);
}


void CPickup::SetForceAlphaAndUseAmbientScaleTraverseHierarchy(CEntity *pEntity, bool bRenderDeferred)
{
	if(pEntity)
	{
		CBaseModelInfo *pMdlInfo = pEntity->GetBaseModelInfo();

		if(pMdlInfo)
			pMdlInfo->SetUseAmbientScale(true);

		if(bRenderDeferred)
		{	// deferred mode:
			pEntity->AssignBaseFlag(fwEntity::USE_SCREENDOOR, true);
			pEntity->AssignBaseFlag(fwEntity::FORCE_ALPHA, false);
		}
		else
		{	// forward mode:
			pEntity->AssignBaseFlag(fwEntity::USE_SCREENDOOR, false);
			pEntity->AssignBaseFlag(fwEntity::FORCE_ALPHA, true);
		}

		SetForceAlphaAndUseAmbientScaleTraverseHierarchy(static_cast<CEntity*>(pEntity->GetChildAttachment()), bRenderDeferred);
		SetForceAlphaAndUseAmbientScaleTraverseHierarchy(static_cast<CEntity*>(pEntity->GetSiblingAttachment()), bRenderDeferred);
	}
}


void CPickup::SetUseLightOverrideTraverseHierarchy(CEntity *pEntity, bool bValue)
{
	if(pEntity)
	{
		Assertf(pEntity->GetIsTypeObject(), "CPickup::SetUseLightOverrideTraverseHierarchy()...Expecting an object.");
		static_cast<CObject *>(pEntity)->SetUseLightOverride(bValue);
		SetUseLightOverrideTraverseHierarchy(static_cast<CEntity*>(pEntity->GetChildAttachment()), bValue);
		SetUseLightOverrideTraverseHierarchy(static_cast<CEntity*>(pEntity->GetSiblingAttachment()), bValue);
	}
}


CPickup *CPickup::GetParentPickUp(CObject *pObject)
{
	do 
	{
		if(pObject->IsPickup())
			return static_cast<CPickup *>(pObject);
	} while ((pObject = static_cast<CObject *>(pObject->GetAttachParent())) != NULL);

	return NULL;
}

void CPickup::SetIncludeProjectileFlag( bool includeProjectiles )
{
	if( GetCurrentPhysicsInst() )
	{
		u16 levelIndex = GetCurrentPhysicsInst()->GetLevelIndex();

		if( CPhysics::GetLevel()->IsInLevel( levelIndex ) )
		{
			u32 nOrigIncludeFlags = CPhysics::GetLevel()->GetInstanceIncludeFlags( levelIndex );

			if( includeProjectiles )
			{
				nOrigIncludeFlags |= ArchetypeFlags::GTA_PROJECTILE_TYPE;
			}
			else
			{
				nOrigIncludeFlags &= ~ArchetypeFlags::GTA_PROJECTILE_TYPE;
			}

			CPhysics::GetLevel()->SetInstanceIncludeFlags( levelIndex, nOrigIncludeFlags );
		}
	}

	m_includeProjectiles = includeProjectiles;
}
