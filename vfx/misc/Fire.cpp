///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Fire.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	14 March 2006
//	WHAT:	Fire processing and rendering
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "Fire.h"

// rage
#include "crskeleton/skeleton.h"
#include "rmptfx/ptxmanager.h"
#include "grprofile/pix.h"

// framework
#include "fwscene/world/worldmgr.h"
#include "fwscene/search/Search.h"
#include "fwsys/timer.h"
#include "vfx/channel.h"
#include "vfx/ptfx/ptfxattach.h"
#include "vfx/ptfx/ptfxreg.h"
#include "vfx/vfxutil.h"
#include "vfx/vfxwidget.h"

// game
#include "Camera/CamInterface.h"
#include "camera/base/BaseCamera.h"
#include "Camera/Gameplay/GameplayDirector.h"
#include "control/replay/replay.h"
#include "control/replay/effects/ParticleFireFxPacket.h"
#include "Core/Game.h"
#include "Game/Localisation.h"
#include "Event/EventShocking.h"
#include "Event/ShockingEvents.h"
#include "Game/Dispatch/Incidents.h"
#include "Game/Dispatch/IncidentManager.h"
#include "Game/ModelIndices.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Network/Events/NetworkEventTypes.h"
#include "Network/General/NetworkUtil.h"
#include "Network/NetworkInterface.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "pathserver/PathServer.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedEventScanner.h"
#include "Peds/Ped.h"
#include "Peds/Rendering/PedVariationDebug.h"
#include "Physics/GtaArchetype.h"
#include "Physics/GtaMaterialManager.h"
#include "Physics/Physics.h"
#include "physics/WorldProbe/worldprobe.h"
#include "Renderer/Water.h"
#include "Scene/World/GameWorld.h"
#include "Script/Script.h"
#include "system/TamperActions.h"
#include "System/TaskScheduler.h"
#include "Vehicles/Automobile.h"
#include "Vfx/Decals/DecalManager.h"
#include "Vfx/Systems/VfxFire.h"
#include "Vfx/Systems/VfxVehicle.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Weapons/Weapon.h"
#include "fwscene/world/WorldRepMulti.h"
#include "Pickups/Pickup.h"
#include "Pickups/Data/PickupData.h"
#include "fwnet/netchannel.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

RAGE_DEFINE_SUBCHANNEL(net, fire, DIAG_SEVERITY_DEBUG3)
#undef __net_channel
#define __net_channel net_fire

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define SAFETY_FIRES					(6)											// number of fires to leave behind in case required script use
#define FIRE_REMOVAL_DIST				(60.0f)										// distance at which a fire is removed (from closest camera)
#define FIRE_REMOVAL_DIST_SQR			(FIRE_REMOVAL_DIST*FIRE_REMOVAL_DIST)		// distance at which a fire is removed (from closest camera)
#define PETROL_REMOVAL_DIST				(200.0f)									// distance at which a petrol fire is removed (from closest camera)
#define PETROL_REMOVAL_DIST_SQR			(PETROL_REMOVAL_DIST*PETROL_REMOVAL_DIST)	// distance at which a petrol fire is removed (from closest camera)
#define WRECKED_REMOVAL_DIST			(100.0f)									// distance at which a wrecked fire is removed (from closest camera)
#define WRECKED_REMOVAL_DIST_SQR		(WRECKED_REMOVAL_DIST*WRECKED_REMOVAL_DIST)	// distance at which a wrecked fire is removed (from closest camera)
#define	FIRE_AVERAGE_BURNTIME			(7.0f)										// average burntime of a fire
#define FIRE_BURNOUT_RATIO				(0.7f)										// ratio of life at which fire starts to burn out
#define FIRE_DAMAGE_MULT_DEFAULT		(2.0f)										// value that gets multiplied by the fire radius to work out the damage a default fire does
#define FIRE_DAMAGE_MULT_PETROL			(5.0f)										// value that gets multiplied by the fire radius to work out the damage a petrol fire does
#define SPREAD_FIRE_SEARCH_MAX_ENTITY	(128)										// Maximum entity count the fire spreading search can hold
#define FLAMETHROWER_DAMAGE_MULTIPLIER  (0.4f)										// Flat starting value for the damage multiplier of fires caused with flamethrower (instead of based on the radius of the sphere)

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CFireManager	g_fireMan;

dev_float FIRE_MAX_RADIUS_DEFAULT					= 0.5f;
dev_float FIRE_MAX_RADIUS_MAP						= 1.0f;
dev_float FIRE_MAX_RADIUS_PETROL					= 0.75f;
dev_float FIRE_MAX_RADIUS_PED						= 1.0f;
dev_float FIRE_MAX_RADIUS_OBJ_GENERIC				= 0.5f;
dev_float FIRE_MAX_RADIUS_VEH_GENERIC				= 0.5f;
dev_float FIRE_MAX_RADIUS_PTFX						= 1.0f;

dev_s32 FIRE_MAX_CHILDREN							= 5;
dev_s32 FIRE_STATUS_GRID_CELL_SIZE					= 1;
dev_float FIRE_SPREAD_CHANCE_SECS_PER_RAND_TRY		= 0.03333333f;
dev_float FIRE_SPREAD_CHANCE_LO_FLAM				= 1.5f;
dev_float FIRE_SPREAD_CHANCE_HI_FLAM				= 5.0f;
dev_float FIRE_SPREAD_GROUND_DIST_RADIUS_MULT_MIN	= 1.0f;
dev_float FIRE_SPREAD_GROUND_DIST_RADIUS_MULT_MAX	= 1.7f;
dev_float FIRE_SPREAD_GROUND_OUTWARD_DIR_MULT		= 0.5f;
dev_float FIRE_SPREAD_UP_DIST_RADIUS_UP_MULT_MIN	= 1.0f;
dev_float FIRE_SPREAD_UP_DIST_RADIUS_UP_MULT_MAX	= 1.5f;
dev_float FIRE_SPREAD_UP_DIST_RADIUS_OUT_MULT_MIN	= 1.0f;
dev_float FIRE_SPREAD_UP_DIST_RADIUS_OUT_MULT_MAX	= 2.0f;
dev_float FIRE_SPREAD_HEIGHT_RADIUS_OFFSET			= 1.0f;									// how much to move the ground probe up from the current fire position (as a multiple of the radius)
dev_float FIRE_SPREAD_HEIGHT_RADIUS_MULT			= 2.0f;									// how far to probe downwards for ground (as a multiple of the radius)
dev_float FIRE_SPREAD_OBJECT_REJECT_DIST			= 1.0f;

dev_float FIRE_FUEL_SPREAD_CHANCE					= 10.0f;
dev_float FIRE_FUEL_FLAMMABILITY_MULT				= 1.0f;
dev_float FIRE_FUEL_BURN_TIME_MULT					= 10.0f;
dev_float FIRE_FUEL_BURN_STRENGTH_MULT				= 1.0f;
dev_float FIRE_EXTINGUISH_TIME_MULT					= 5.0f;									// the multiplier used to make a fire extinguish quicker

dev_float FIRE_PED_BURN_TIME						= 10.0f;
dev_float FIRE_PLAYER_BURN_TIME						= 2.0f;
dev_float FIRE_PLAYER_BURN_TIME_EXPLOSION			= 7.0f;

dev_float FIRE_PETROL_BURN_TIME						= 7.5f;
dev_float FIRE_PETROL_BURN_STRENGTH					= 1.0f;
dev_float FIRE_PETROL_POOL_PEAK_TIME				= 1.0f;
dev_float FIRE_PETROL_TRAIL_PEAK_TIME				= 3.0f;
dev_float FIRE_PETROL_SPREAD_LIFE_RATIO				= 0.2f;
dev_float FIRE_PETROL_SEARCH_DIST					= 2.5f;

dev_u32 FIRE_ENTITY_TEST_INTERVAL					= 500;									// how often we're allowed to test for intersections with entities
dev_u32 FIRE_ENTITY_TEST_INTERVAL_FLAMETHROWER		= 50;									// how often we're allowed to check for intersections of specifically flamethrower fires with other entities

///////////////////////////////////////////////////////////////////////////////
//  STATIC VARIABLES
///////////////////////////////////////////////////////////////////////////////

atArray<CFireManager::EntityNextToFire> CFireManager::m_EntitiesNextToFires;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS CFire
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

bool			CFire::Init									(const CFireSettings& fireSettings)
{
	vfxAssertf(IsZeroAll(fireSettings.vNormLcl)==false, "trying to start a fire with a zero normal");

	// general
	m_vPosLcl = fireSettings.vPosLcl;
	m_vNormLcl = fireSettings.vNormLcl;
	m_vParentPosWld = Vec3V(V_ZERO);
	m_fireType = fireSettings.fireType;
	m_parentFireType = fireSettings.parentFireType;
	m_mtlId = fireSettings.mtlId;
	m_regdEntity = fireSettings.pEntity;
	m_boneIndex = fireSettings.boneIndex;
	m_regdCulprit = fireSettings.pCulprit;
	m_weaponHash = fireSettings.weaponHash;

	// hierarchy
#if __BANK
	m_vRootPosWld = Vec3V(V_ZERO);
#endif
	Assign(m_numGenerations, fireSettings.numGenerations);

#if __BANK
	if (g_fireMan.m_ignoreGenerations)
	{
		m_numGenerations = FIRE_DEFAULT_NUM_GENERATIONS;
	}
#endif

	m_vParentPosWld = fireSettings.vParentPos;
#if __BANK
	m_vRootPosWld = fireSettings.vRootPos;
#endif

	// timed specific
	m_totalBurnTime = fireSettings.burnTime;
	m_maxStrength = fireSettings.burnStrength;
	m_maxStrengthTime = fireSettings.peakTime;
	m_spreadChanceAccumTime = 0.0f;

	//Scale burn time by weapon effect modifier
	if (auto pWeaponInfo = CWeaponInfoManager::GetInfo<CWeaponInfo>(m_weaponHash))
	{
		m_totalBurnTime *= pWeaponInfo->GetEffectDurationModifier();
		m_maxStrength *= pWeaponInfo->GetAoEModifier();
	}

	// registered specific
	m_regOwner = fireSettings.regOwner;
	m_regOffset = fireSettings.regOffset;

	// fuel
	m_fuelLevel = fireSettings.fuelLevel;
	m_fuelBurnRate = fireSettings.fuelBurnRate;

	// update rate
	m_entityTestTime = fwTimer::GetTimeInMilliseconds() + fwRandom::GetRandomNumberInRange(0, FIRE_ENTITY_TEST_INTERVAL);
	
	// current state
	m_currBurnTime = 0.0f;
	m_currStrength = 0.0f;	
	m_numChildren = 0;

	m_maxRadius = 0.0f;

	if (NetworkInterface::IsGameInProgress())
	{
		bool bWrecked = ((m_fireType >= FIRETYPE_TIMED_VEH_WRECKED) && (m_fireType <= FIRETYPE_TIMED_VEH_GENERIC));
		bool bWheel   = (m_fireType == FIRETYPE_REGD_VEH_WHEEL);
		bool bPetrol  = (m_fireType == FIRETYPE_REGD_VEH_PETROL_TANK);

		if (bWrecked || bWheel || bPetrol)
		{
			if (fireSettings.pEntity && fireSettings.pEntity->GetIsTypeVehicle() && NetworkUtils::IsNetworkClone(fireSettings.pEntity))
			{
				CVehicle* pVehicle = static_cast<CVehicle*>(fireSettings.pEntity);
				if (pVehicle)
				{
					if (bWheel)
					{
						const CWheel* pWheel = pVehicle->GetWheel(0);
						if (pWheel)
							m_maxRadius = pWheel->GetWheelRadius();
					}
					else
					{
						CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
						if (pVfxVehicleInfo)
							m_maxRadius = bWrecked ? pVfxVehicleInfo->GetWreckedFirePtFxRadius() : pVfxVehicleInfo->GetPetrolTankFirePtFxRadius();
					}
				}
			}
		}
	}

	// audio
	m_audioRegistered = false;
	
	// flags
	m_flags = 0;
	if (fireSettings.regOwner) 
	{
		SetFlag(FIRE_FLAG_IS_REGISTERED);
		SetFlag(FIRE_FLAG_REGISTERED_THIS_FRAME);
	}
	if (fireSettings.pEntity)
	{
		// set up the entity on fire and set a flag on the entity
		fireSettings.pEntity->m_nFlags.bIsOnFire = true;
		SetFlag(FIRE_FLAG_IS_ATTACHED);
	}
	if (fireSettings.isContained)
	{
		SetFlag(FIRE_FLAG_IS_CONTAINED);
	}
	if (fireSettings.isFlameThrower)
	{
		SetFlag(FIRE_FLAG_IS_FLAME_THROWER);
	}
	if (fireSettings.hasSpreadFromScriptedFire)
	{
		SetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED);
	}
	if (fireSettings.hasCulpritBeenCleared)
	{
		SetFlag(FIRE_FLAG_CULPRIT_CLEARED);
	}

	// ped specific
	m_vLimbBurnRatios.SetXf(g_DrawRand.GetRanged(0.85f, 1.0f));
	m_vLimbBurnRatios.SetYf(g_DrawRand.GetRanged(0.85f, 1.0f));
	m_vLimbBurnRatios.SetZf(g_DrawRand.GetRanged(0.85f, 1.0f));
	m_vLimbBurnRatios.SetWf(g_DrawRand.GetRanged(0.85f, 1.0f));

	// script
	m_scriptRefIndex = 1;

	// audio
	if (!fireSettings.pEntity || (fireSettings.pEntity && !fireSettings.pEntity->GetIsTypeVehicle()))
	{
		m_pAudioEntity = rage_new audFireAudioEntity();
		m_pAudioEntity->Init(this);
	}

	// network
	// only send network events about fires once we've successfully created them. Fires registered every frame are not networked.  
	bool fireActivated = false;
	if (!GetFlag(FIRE_FLAG_REGISTERED_THIS_FRAME) && NetworkInterface::IsGameInProgress())
	{
		if (fireSettings.pNetIdentifier)
		{
			m_networkIdentifier = *fireSettings.pNetIdentifier;
		}
		else
		{
			// fires without an identifier are assigned to our machine during a network game
			m_networkIdentifier.Set(NetworkInterface::GetLocalPhysicalPlayerIndex(), CNetFXIdentifier::GetNextFXId());
		}

		// don't activate fires on non-networked objects in MP
		bool bActivateFire = true;
		bool bAllowNetEvent = !m_regdEntity || NetworkUtils::GetNetworkObjectFromEntity(m_regdEntity);
		
		if (!bAllowNetEvent && m_regdEntity->GetIsTypeObject())
		{	
			bActivateFire = false;
		}

		// update with any requested overrides. Network clones use the received burn time
		// and burn strength values directly, so the fires are synchronised, the randomness
		// has already been applied by the host when creating the fire.
		if (GetNetworkIdentifier().IsClone())
		{
			m_totalBurnTime = fireSettings.burnTime;
			m_maxStrength = fireSettings.burnStrength;
			m_maxStrengthTime = 0.2f*m_totalBurnTime*FIRE_BURNOUT_RATIO;

			SetFlag(FIRE_FLAG_IS_ACTIVE);
			g_fireMan.ActivateFireCB(this);
			fireActivated = true;
		}
		else if (bActivateFire)
		{
			SetFlag(FIRE_FLAG_IS_ACTIVE);
			
			if (bAllowNetEvent)
			{
				CFireEvent::Trigger(*this);
			}
			
			g_fireMan.ActivateFireCB(this);
			fireActivated = true;
		}
	}
	else
	{
		SetFlag(FIRE_FLAG_IS_ACTIVE);
		// update the manager's active fire index array
		g_fireMan.ActivateFireCB(this);
		fireActivated = true;
	}

	// path server
	m_pathServerDynObjId = DYNAMIC_OBJECT_INDEX_NONE;
	CPathServerGta::AddFireObject(this);

	// init the asynch probes
	m_scorchMarkVfxProbeId = -1;
	m_groundInfoVfxProbeId = -1;
	m_upwardVfxProbeId = -1;

	// add a scorch mark for map fires
	if (fireSettings.fireType==FIRETYPE_TIMED_MAP)
	{
#if __BANK
		if (!g_fireMan.m_disableDecals)
#endif
		{
			TriggerScorchMarkProbe();

			// we could trigger the decal this way (instead of probing) if we had the collision entity
			// unfortunately we don't at the moment so what have to probe instead
// 			VfxCollInfo_s vfxCollInfo;
// 			vfxCollInfo.regdEnt = NULL;
// 			vfxCollInfo.vPositionWld = m_vPosLcl;
// 			vfxCollInfo.vNormalWld = m_vNormLcl;
// 			vfxCollInfo.vDirectionWld = -vfxCollInfo.vNormalWld;
// 			vfxCollInfo.materialId = m_mtlId;
// 			vfxCollInfo.componentId = -1;
// 			vfxCollInfo.weaponGroup = WEAPON_EFFECT_GROUP_FIRE;
// 			vfxCollInfo.force = VEHICLEGLASSFORCE_NOT_USED;
// 			vfxCollInfo.isBloody = false;
// 			vfxCollInfo.isWater = false;
// 			vfxCollInfo.isExitFx = false;
// 
// 			// play any weapon impact effects
// 			g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, DAMAGE_TYPE_FIRE, m_regdCulprit);
		}
	}

	//Search for spreading the fire
	//
	//Init the fire search - The search will store a maximum of SPREAD_FIRE_SEARCH_MAX_ENTITY entity around the fire
	m_search = rage_new fwSearch(SPREAD_FIRE_SEARCH_MAX_ENTITY);
	//No search requested
	m_searchRequested = false;
	//No result available
	m_searchDataReady = false;

	return fireActivated;
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CFire::Shutdown							(bool finishPtFx)
{
	// clear the flags
	bool isDormant = GetFlag(FIRE_FLAG_IS_DORMANT);
	bool isScripted = GetFlag(FIRE_FLAG_IS_SCRIPTED);
	m_flags = 0;
	if (isScripted)
	{
		// this was a scripted fire - set the flag again - the fire must remain reserved until the script frees it up
		SetFlag(FIRE_FLAG_IS_SCRIPTED);
	}

	// unregister the fire particle effects
	if (GetRegOwner())
	{
		// fire effects registered to other entities (i.e. wheels or vehicles) should be left alone
		// it's up to the other entity to remove them
	}
	else
	{
		ptfxReg::UnRegister(this, finishPtFx);
	}

	if(IsAudioRegistered())
	{
		if(m_regdEntity && m_regdEntity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			CVehicle* pVehicle = static_cast<CVehicle*>(m_regdEntity.Get());
			pVehicle->GetVehicleAudioEntity()->GetVehicleFireAudio().UnregisterVehFire(this);
		}
	}

	// abort any active probes
	if (m_scorchMarkVfxProbeId>-1)
	{
		CVfxAsyncProbeManager::AbortProbe(m_scorchMarkVfxProbeId);
		m_scorchMarkVfxProbeId = -1;
	}

	if (m_groundInfoVfxProbeId>-1)
	{
		CVfxAsyncProbeManager::AbortProbe(m_groundInfoVfxProbeId);
		m_groundInfoVfxProbeId = -1;
	}

	if (m_upwardVfxProbeId>-1)
	{
		CVfxAsyncProbeManager::AbortProbe(m_upwardVfxProbeId);
		m_upwardVfxProbeId = -1;
	}

	// shutdown the fire audio
	if (m_pAudioEntity)
	{
		m_pAudioEntity->Shutdown();
		delete m_pAudioEntity;
		m_pAudioEntity = NULL;
	}

	// remove the fire from the paths 
	CPathServerGta::RemoveFireObject(this);

	if (m_reportedIncident)
	{
		Assert(m_reportedIncident->GetType() == CIncident::IT_Fire);
		Assert(dynamic_cast<CFireIncident*>(m_reportedIncident.Get()));
		//static_cast<CFireIncident*>(m_reportedIncident.Get())->SetFire(NULL);
		static_cast<CFireIncident*>(m_reportedIncident.Get())->RemoveFire(this);
		m_reportedIncident = NULL;
		//CIncidentManager::GetInstance().ClearIncident(m_reportedIncident);
	}

	// update the manager's active fire index array
	if (isDormant==false)
	{
		g_fireMan.DeactivateFireCB(this);
	}
	
	// tidy up any entity flags - if there are no more fires on this entity then reset the flag
	if (m_regdEntity)
	{
		if (g_fireMan.GetEntityFire(m_regdEntity)==false)
		{
			m_regdEntity->m_nFlags.bIsOnFire = false;
		}
	}

	//No need foe the fire search anymore
	if (m_search)
	{
		delete m_search;
		m_search = NULL;
	}
	//No search requested
	m_searchRequested = false;
	//No result available
	m_searchDataReady = false;

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordPersistantFx<CPacketStopFireFx>(CPacketStopFireFx(), CTrackedEventInfo<ptxFireRef>(ptxFireRef((size_t)this)), NULL, false);
		CReplayMgr::StopTrackingFx(CTrackedEventInfo<ptxFireRef>(ptxFireRef((size_t)this)));
	}
#endif
}

// Start asynchronous search
void CFire::StartAsyncFireSearch()
{
	//////////////////////////////////////////////////////////////////////////
	// sphere around the player
	if ( m_search && m_searchRequested)
	{
		PF_AUTO_PUSH_TIMEBAR("StartAsyncFireSearch");

		fwIsSphereIntersecting testSphere(spdSphere(GetPositionWorld(), ScalarVFromF32(CalcCurrRadius())));

		m_search->Start
			(
			fwWorldRepMulti::GetSceneGraphRoot(),
			&testSphere,
			ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT,
			SEARCH_LOCATION_EXTERIORS | SEARCH_LOCATION_INTERIORS,
			SEARCH_LODTYPE_HIGHDETAIL,
			SEARCH_OPTION_DYNAMICS_ONLY /*| SEARCH_OPTION_FORCE_PPU_CODEPATH*/,
			WORLDREP_SEARCHMODULE_DEFAULT
			);
	}
}

// End asynchronous search
void CFire::EndAsyncFireSearch()
{
	if ( m_search && m_searchRequested)
	{
		PF_AUTO_PUSH_TIMEBAR("EndAsyncFireSearch");
		m_search->Finalize();

		//The search is done
		m_searchRequested = false;

		//Data is ready to be read
		m_searchDataReady = true;

		//Collect the search results if any
		CollectFireSearchResults();
	}
}

// Call call back function
void CFire::CollectFireSearchResults()
{
	if ( m_search && m_searchDataReady)
	{
		PF_AUTO_PUSH_TIMEBAR("CollectFireSearchResults");
		m_search->ExecuteCallbackOnResult(CFireManager::FireNextToEntityCB, this);

		//No need for the results anymore
		m_searchDataReady = false;
	}
}

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void			CFire::Update							(float deltaTime)
{
#if __ASSERT
	if (m_regdCulprit)
	{
		AssertEntityPointerValid(m_regdCulprit);
	}
#endif

	bool bFirstUpdate = false;
	if (GetFlag(FIRE_FLAG_IS_REGISTERED))
	{
		if (GetFlag(FIRE_FLAG_REGISTERED_THIS_FRAME)==false)
		{
			// the registered fire hasn't been registered this frame - shut it down
			Shutdown();
			return;
		}
	}
	else
	{
		bFirstUpdate = (m_currBurnTime == 0.0f);
		// timed fire
		// update the burn time
		if (GetFlag(FIRE_FLAG_IS_BEING_EXTINGUISHED))
		{
			m_currBurnTime += deltaTime*FIRE_EXTINGUISH_TIME_MULT;
			ClearFlag(FIRE_FLAG_IS_BEING_EXTINGUISHED);
		}
		else if (!GetFlag(FIRE_FLAG_IS_SCRIPTED))
		{
			bool dontUpdateTime = GetFlag(FIRE_FLAG_DONT_UPDATE_TIME_THIS_FRAME) && m_currStrength==m_maxStrength;
			if (!dontUpdateTime)
			{
				m_currBurnTime += deltaTime;
			}
		}

		// check if the fire is out of range
		bool outOfRange = false;
		if (!GetFlag(FIRE_FLAG_IS_SCRIPTED) && !GetFlag(FIRE_FLAG_IS_REGISTERED))
		{
			float fireRemovalDistSqr = FIRE_REMOVAL_DIST_SQR;
			if (m_fireType>=FIRETYPE_TIMED_VEH_WRECKED && m_fireType<=FIRETYPE_TIMED_VEH_WRECKED_3)
			{
				fireRemovalDistSqr = WRECKED_REMOVAL_DIST_SQR;
			}
			else if (m_fireType==FIRETYPE_TIMED_PETROL_TRAIL || m_fireType==FIRETYPE_TIMED_PETROL_POOL)
			{
				fireRemovalDistSqr = PETROL_REMOVAL_DIST_SQR;
			}

			if (CVfxHelper::GetDistSqrToCamera(GetPositionWorld())>fireRemovalDistSqr) 
			{
				outOfRange = true;
			}
		}

		// remove any non scripted fires that are either burnt out or out of range
		if (m_currBurnTime>m_totalBurnTime || outOfRange)
		{
			Shutdown();
			return;
		}
	}

	// clear the 'don't update time' flag - this must be set each frame the fire doesn't need updated
	ClearFlag(FIRE_FLAG_DONT_UPDATE_TIME_THIS_FRAME);

	// remove any entity fires that no longer have an entity or valid matrix
	if (GetFlag(FIRE_FLAG_IS_ATTACHED))
	{
		if (m_regdEntity.Get()==NULL)
		{
			Shutdown();
			return;
		}
		else if (m_boneIndex>0)
		{
			// check if the bone matrix we're attached to is zero
			Mat34V vBoneMtx;
			CVfxHelper::GetMatrixFromBoneIndex(vBoneMtx, m_regdEntity, m_boneIndex);
			if (IsZeroAll(vBoneMtx.GetCol0()))
			{
				// it is - this frag has broken
				// check the broken frag data to see if we can re-attach to the broken off bit
				bool fireEntityRelocated = false;
				for (int i=0; i<vfxUtil::GetNumBrokenFrags(); i++)
				{
					const vfxBrokenFragInfo& brokenFragInfo = vfxUtil::GetBrokenFragInfo(i);

					if (brokenFragInfo.pParentEntity==m_regdEntity && brokenFragInfo.pChildEntity!=NULL)
					{
						// found an entitiy to re-attach to
						// update the fire entity
						m_regdEntity = static_cast<CEntity*>(brokenFragInfo.pChildEntity.Get());

						// tell any particle effects to re-attach as well
						ptfxAttach::UpdateBrokenFrag(brokenFragInfo.pParentEntity, brokenFragInfo.pChildEntity, m_boneIndex);

						fireEntityRelocated = true;
					}
				}

				// if we couldn't re-attach - shutdown the fire
				if (!fireEntityRelocated)
				{
					Shutdown();
					return;
				}
			}
		}
	}

	// update entity fires
	bool isEntityInWater = false;
	if (m_regdEntity)
	{
		AssertEntityPointerValid(m_regdEntity);

		// set this entity as being on fire (another fire may have cleared the flag when it was shutdown)
		m_regdEntity->m_nFlags.bIsOnFire = true;

		// if this fire is connected to a ped we'll inflict some damage to him.
		if (m_regdEntity->GetType() == ENTITY_TYPE_PED)
		{
			CPed *pPedOnFire = static_cast<CPed*>(m_regdEntity.Get());

			// check if the ped is in a vehicle
			if (pPedOnFire->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPedOnFire->GetMyVehicle())
			{	
				// set vehicle on fire
				AssertEntityPointerValid(pPedOnFire->GetMyVehicle());

				// don't set fire trucks on fire 
				if (pPedOnFire->GetMyVehicle()->GetModelIndex() != MI_CAR_FIRETRUCK)
				{
					// make car go on fire as well
					if (pPedOnFire->GetMyVehicle()->InheritsFromAutomobile())
					{
                        if (!pPedOnFire->GetMyVehicle()->IsNetworkClone())
                        {
							if (static_cast<CAutomobile*>(pPedOnFire->GetMyVehicle())->GetHealth() > 75.0f)
							{
								// set low health on the vehicle so an engine fire starts
								static_cast<CAutomobile*>(pPedOnFire->GetMyVehicle())->SetHealth(75.0f);
							}
						}
					}
				}
			}
			else
			{
				if (!pPedOnFire->IsPlayer() && pPedOnFire->IsDead() && GetLifeRatio() > 0.9f ) 
				{
					pPedOnFire->m_nPhysicalFlags.bRenderScorched = true;
				}
			}

			isEntityInWater = pPedOnFire->GetIsInWater();
		}
		else if (m_regdEntity->GetType() == ENTITY_TYPE_VEHICLE)
		{
			CVehicle* pVehOnFire = static_cast<CVehicle*>(m_regdEntity.Get());
			isEntityInWater = pVehOnFire->GetIsInWater();
		}

		if(m_regdEntity->GetIsTypePed() && GetPathServerObjectIndex()!=DYNAMIC_OBJECT_INDEX_NONE)
		{
			CPathServerGta::UpdateFireObject(this);
		}
	
		if (CVfxHelper::IsInInterior(m_regdEntity))
		{
			SetFlag(FIRE_FLAG_IS_INTERIOR_FIRE);
		}
		else
		{
			ClearFlag(FIRE_FLAG_IS_INTERIOR_FIRE);
		}
	}

	// remove any fires that are underwater
	if (GetFlag(FIRE_FLAG_IS_REGISTERED)==false)
	{
		Vec3V vFirePos = GetPositionWorld();
		if (m_regdEntity==NULL || isEntityInWater)
		{
			float waterDepth;
			CVfxHelper::GetWaterDepth(vFirePos, waterDepth);
			if (waterDepth>0.0f)
			{
				if(m_regdEntity && m_regdEntity->GetType() == ENTITY_TYPE_PED)
				{
					audSpeechAudioEntity* speechEnt = static_cast<CPed*>(m_regdEntity.Get())->GetSpeechAudioEntity();
					if(speechEnt)
					{
						audDamageStats damageStats;
						damageStats.Fatal = true;
						damageStats.PlayGargle = true;
						speechEnt->InflictPain(damageStats);
					}
				}
				Shutdown();
				return;
			}
		}
	}
	else
	{
		// clear the  we have dealt with this - clear the flag
		ClearFlag(FIRE_FLAG_REGISTERED_THIS_FRAME);
	}

	// update the fire's fuel
	if (m_fuelLevel>0.0f)
	{
		m_fuelLevel = Max(0.0f, m_fuelLevel-(m_fuelBurnRate*deltaTime));
	}

	// continually add the shocking event in - it will be removed automatically if not polled for 1 sec
	if (GetFlag(FIRE_FLAG_IS_CONTAINED)==false)
	{
		s32 offset = ((s32)(uptr)this)%11;
		if (bFirstUpdate || !((fwTimer::GetSystemFrameCount()+offset)&31))
		{
			CEntity* pFireEntity = m_regdEntity;
			if (pFireEntity && pFireEntity->GetIsTypeBuilding())
			{
				pFireEntity = NULL;
			}
			Vec3V vFirePos = GetPositionWorld();
			CEventShockingFire ev(vFirePos, pFireEntity);
			CShockingEventsManager::Add(ev);
		}
	}

	// add in a dispatch incident for this fire 
	if (!m_reportedIncident REPLAY_ONLY(&& !CReplayMgr::IsEditModeActive()) && !m_networkIdentifier.IsClone())
	{
		s32 offset = ((s32)(uptr)this)%7;
		//try the first time, and then so often
		if (bFirstUpdate || !((fwTimer::GetSystemFrameCount()+offset)&31))
		{
			if (CanDispatchFireTruckTo())
			{
				m_reportedIncident = CIncidentManager::GetInstance().AddFireIncident(this);
			}
		}
	}

	// update fire strength 
	float burnOutStartTime = m_totalBurnTime*FIRE_BURNOUT_RATIO;
	if (GetFlag(FIRE_FLAG_IS_SCRIPTED))
	{
		// keep script fires at max strength all the time
		m_currStrength = m_maxStrength;
	}
	else if (m_currBurnTime>burnOutStartTime)
	{
		// fade out the fire strength over the burn out period
		m_currStrength = m_maxStrength * (1.0f-((m_currBurnTime-burnOutStartTime)/(m_totalBurnTime-burnOutStartTime)));
	}
	else if (m_currBurnTime<m_maxStrengthTime)
	{
		// fade in the fire strength at the start of the fire
		m_currStrength = m_maxStrength * (m_currBurnTime/m_maxStrengthTime);
	}
	else
	{
		// otherwise we are at max strength
		m_currStrength = m_maxStrength;
	}

	// update fx system
	ProcessVfx();
	ProcessAsynchProbes();

	// should the fire spread and cause damage?
#if __BANK | GTA_REPLAY
	if (!g_fireMan.m_disableAllSpreading)
#endif
	{
		// we don't want to spread if we are a network clone (only the host controls this)
		if (!m_networkIdentifier.IsClone())
		{
			if (m_fireType==FIRETYPE_TIMED_PETROL_TRAIL || m_fireType==FIRETYPE_TIMED_PETROL_POOL)
			{
				if (m_numChildren<3 && GetLifeRatio()>=FIRE_PETROL_SPREAD_LIFE_RATIO)
				{
					Spread(true);
				}
			}
			else if (GetFlag(FIRE_FLAG_IS_CONTAINED))
			{
				// contained fires from particle effects shouldn't try to spread around the map
				// if they do they can cause issues - e.g. beach fires setting towels alight
				// we only want them to spread if there is petrol beside them
				if (m_currStrength==m_maxStrength && g_DrawRand.GetRanged(0.0f, 100.0f)<3.0f)
				{
					Spread(true);
				}
			}
			else
			{
				// get the spread chance based on the flammability of the fire
				VfxFireInfo_s* pVfxFireInfo = g_vfxFire.GetInfo(PGTAMATERIALMGR->GetMtlVfxGroup(m_mtlId));
				float mtlFlammability = pVfxFireInfo->flammability;
				mtlFlammability *= g_fireMan.GetScriptFlammabilityMultiplier();
#if __BANK
				if (g_fireMan.m_overrideMtlFlammability>=0.0f)
				{
					mtlFlammability = g_fireMan.m_overrideMtlFlammability;
				}
#endif

				float spreadChance = FIRE_SPREAD_CHANCE_LO_FLAM + (mtlFlammability*(FIRE_SPREAD_CHANCE_HI_FLAM-FIRE_SPREAD_CHANCE_LO_FLAM));
					
				// increase the spread chance if there is fuel in the fire
				spreadChance += m_fuelLevel*FIRE_FUEL_SPREAD_CHANCE;

				m_spreadChanceAccumTime += fwTimer::GetTimeStep();
				while (m_spreadChanceAccumTime >= FIRE_SPREAD_CHANCE_SECS_PER_RAND_TRY)
				{
					if (g_DrawRand.GetRanged(0.0f, 100.0f)<spreadChance)
					{
						bool canSpreadToNonPetrol = m_currStrength==m_maxStrength && m_numGenerations>0 && m_numChildren<FIRE_MAX_CHILDREN;
						Spread(!canSpreadToNonPetrol);
					}

					m_spreadChanceAccumTime -= FIRE_SPREAD_CHANCE_SECS_PER_RAND_TRY;
				}
			}
		}
		else if(NetworkInterface::IsGameInProgress())
		{
			//Clone fires look for any petrol trails within their range and start them fading
			Vec3V vFirePos = GetPositionWorld();
			u32 foundDecalTypeFlag = 0;
			Vec3V vPetrolDecalPos;
			fwEntity* pPetrolDecalEntity = NULL;
			if (g_decalMan.GetDisablePetrolDecalsIgniting()==false)
			{
				g_decalMan.FindClosest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, vFirePos, FIRE_PETROL_SEARCH_DIST, 0.0f, false, true, FIRE_PETROL_BURN_TIME, vPetrolDecalPos, &pPetrolDecalEntity, foundDecalTypeFlag);
			}
		}

		if (fwTimer::GetTimeInMilliseconds() >= m_entityTestTime)
		{
			m_entityTestTime = fwTimer::GetTimeInMilliseconds() + (GetFlag(FIRE_FLAG_IS_FLAME_THROWER) ? FIRE_ENTITY_TEST_INTERVAL_FLAMETHROWER : FIRE_ENTITY_TEST_INTERVAL);
			
			//Request searching the object around us
			RequestAsyncFireSearch();
		}
	}

	// update the audio, but not for vehicles
	if (GetEntity() && GetEntity()->GetIsTypeVehicle())
		return;

	if (m_pAudioEntity)
	{
		m_pAudioEntity->Update(this, deltaTime);
	}
}



///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////
#if __BANK
void			CFire::RenderDebug						() const
{
	float colVal = Min(1.0f,  0.2f*(m_numGenerations+1));
	Color32 fireCol(colVal, colVal, colVal, 1.0f);

	if (GetFlag(FIRE_FLAG_IS_DORMANT))
	{
		fireCol = Color32(0.0f, 0.0f, 0.0f, 0.2f);
	}
	else if (GetFlag(FIRE_FLAG_IS_ACTIVE))
	{
		if (GetFlag(FIRE_FLAG_IS_SCRIPTED))
		{
			fireCol = Color32(colVal, colVal*0.5f, 0.0f, 0.5f);
		}
		else if (GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED))
		{
			fireCol = Color32(colVal, colVal*0.25f, 0.0f, 0.5f);
		}
		else if (m_fireType==FIRETYPE_TIMED_MAP)
		{
			fireCol = Color32(0.0f, colVal, 0.0f, 0.5f);
		}
		else if (m_fireType==FIRETYPE_TIMED_PETROL_TRAIL || m_fireType==FIRETYPE_TIMED_PETROL_POOL)
		{
			fireCol = Color32(colVal, colVal, colVal*0.5f, 0.5f);
		}
		else if (m_fireType==FIRETYPE_TIMED_PED)
		{
			fireCol = Color32(colVal, 0.0f, 0.0f, 0.5f);
		}
		else if (m_fireType>=FIRETYPE_TIMED_VEH_WRECKED && m_fireType<=FIRETYPE_TIMED_VEH_WRECKED_3)
		{
			fireCol = Color32(colVal, colVal, 0.0f, 0.5f);
		}
		else if (m_fireType==FIRETYPE_TIMED_VEH_GENERIC)
		{
			fireCol = Color32(0.0f, colVal, colVal, 0.5f);
		}
		else if (m_fireType==FIRETYPE_TIMED_OBJ_GENERIC)
		{
			fireCol = Color32(0.0f, 0.0f, colVal, 0.5f);
		}
		else if (m_fireType==FIRETYPE_REGD_VEH_WHEEL)
		{
			fireCol = Color32(colVal, 0.0f, colVal, 0.5f);
		}
		else if (m_fireType==FIRETYPE_REGD_VEH_PETROL_TANK)
		{
			fireCol = Color32(colVal, 0.0f, colVal*0.5f, 0.5f);
		}
		else if (m_fireType==FIRETYPE_REGD_FROM_PTFX)
		{
			fireCol = Color32(colVal, colVal, colVal, 0.5f);
		}
		else
		{
			vfxAssertf(0, "unrecognised fire type");
		}
	}
	else
	{
		return;
	}

	Vec3V vFirePos = GetPositionWorld();

	if (g_fireMan.m_renderDebugSpheres)
	{
		grcDebugDraw::Sphere(RCC_VECTOR3(vFirePos), CalcCurrRadius(), fireCol);
	}

	// draw a line from this fire to its parent
	if (g_fireMan.m_renderDebugParents)
	{
		if (IsZeroAll(m_vParentPosWld)==false)
		{
			Color32 lineCol(colVal, colVal, 1.0f, 1.0f);
			Vec3V vPosFire = vFirePos;
			Vec3V vPosParent = m_vParentPosWld;
		
			// move up to top of sphere
			vPosFire.SetZf(vPosFire.GetZf()+0.25f);
			vPosParent.SetZf(vPosParent.GetZf()+0.25f);

			grcDebugDraw::Line(RCC_VECTOR3(vPosFire), RCC_VECTOR3(vPosParent), lineCol);
		}
	}

	// draw a line from this fire to its root
	if (g_fireMan.m_renderDebugRoots)
	{
		if (IsZeroAll(m_vParentPosWld)==false)
		{
			Color32 lineCol(colVal, 1.0f, colVal, 1.0f);
			Vec3V vPosFire = vFirePos;
			Vec3V vPosRoot = m_vRootPosWld;

			// move up to top of sphere
			vPosFire.SetZf(vPosFire.GetZf()+0.5f);
			vPosRoot.SetZf(vPosRoot.GetZf()+0.5f);

			grcDebugDraw::Line(RCC_VECTOR3(vPosFire), RCC_VECTOR3(vPosRoot), lineCol);
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  GetMatrixOffset
///////////////////////////////////////////////////////////////////////////////

Mat34V_Out			CFire::GetMatrixOffset						() const
{
	Mat34V mtxOffset;
	if (m_fireType==FIRETYPE_TIMED_PETROL_TRAIL)
	{
		bool shouldAlignToParentPos = false;
		if (m_parentFireType==FIRETYPE_TIMED_PETROL_TRAIL || 
			m_parentFireType==FIRETYPE_TIMED_PETROL_POOL || 
			m_parentFireType==FIRETYPE_TIMED_MAP)
		{
			shouldAlignToParentPos = true;
		}

		Vec3V vDir = Vec3V(V_Y_AXIS_WZERO);
		if (shouldAlignToParentPos)
		{
			// point the y axis back to the parent fire
			vDir = m_vParentPosWld - m_vPosLcl;

			vfxAssertf(IsEqualAll(m_vPosLcl, m_vPosLcl), "fire local position is not a valid number");
			vfxAssertf(IsEqualAll(m_vParentPosWld, m_vParentPosWld), "fire parent world position is not a valid number");
			vfxAssertf(IsEqualAll(vDir, vDir), "fire direction is not a valid number");

			const Vec3V vZero(V_ZERO);
			const Vec3V vEpsilon(0.001f, 0.001f, 0.001f);
			if (IsCloseAll(vDir, vZero, vEpsilon))
			{
				vDir = Vec3V(V_Y_AXIS_WZERO);
			}
		}

		CVfxHelper::CreateMatFromVecYAlign(mtxOffset, m_vPosLcl, vDir, m_vNormLcl);
	}
	else
	{
		CVfxHelper::CreateMatFromVecZAlign(mtxOffset, m_vPosLcl, m_vNormLcl, Vec3V(V_Y_AXIS_WZERO));
	}

	return mtxOffset;
}


///////////////////////////////////////////////////////////////////////////////
//  GetPosition
///////////////////////////////////////////////////////////////////////////////

Vec3V_Out			CFire::GetPositionWorld						() const
{
	Vec3V vPos;
	if (m_regdEntity)
	{
		Mat34V mtxEntityBone;
		if (m_boneIndex>0)
		{
			CVfxHelper::GetMatrixFromBoneIndex(mtxEntityBone, m_regdEntity, m_boneIndex);
		}
		else
		{
			mtxEntityBone = m_regdEntity->GetMatrix();
		}

		Mat34V mtxFinal;
		Transform(mtxFinal, mtxEntityBone, GetMatrixOffset());
		vPos = mtxFinal.GetCol3();
	}
	else
	{
		vPos = GetMatrixOffset().GetCol3();
	}

	return vPos;
}


///////////////////////////////////////////////////////////////////////////////
//  GetMaxRadius
///////////////////////////////////////////////////////////////////////////////

float			CFire::GetMaxRadius					() const
{
	// calc the maximum radius
	if (m_fireType==FIRETYPE_TIMED_MAP) 
	{
		return FIRE_MAX_RADIUS_MAP;
	}
	else if (m_fireType==FIRETYPE_TIMED_PETROL_TRAIL || 
			 m_fireType==FIRETYPE_TIMED_PETROL_POOL) 
	{
		return FIRE_MAX_RADIUS_PETROL;
	}
	else if (m_fireType==FIRETYPE_TIMED_PED)
	{
		return FIRE_MAX_RADIUS_PED;
	}
	else if (m_fireType==FIRETYPE_TIMED_OBJ_GENERIC)
	{
		return FIRE_MAX_RADIUS_OBJ_GENERIC;
	}
	else if (m_fireType==FIRETYPE_TIMED_VEH_GENERIC)
	{
		return FIRE_MAX_RADIUS_VEH_GENERIC;
	}
	else if ((m_fireType>=FIRETYPE_TIMED_VEH_WRECKED && m_fireType<=FIRETYPE_TIMED_VEH_WRECKED_3) ||
			 m_fireType==FIRETYPE_REGD_VEH_WHEEL ||
			 m_fireType==FIRETYPE_REGD_VEH_PETROL_TANK) 
	{
		// these fires all specify their max radius when they are created
		return m_maxRadius;
	}
	else if (m_fireType==FIRETYPE_REGD_FROM_PTFX)
	{
		return FIRE_MAX_RADIUS_PTFX;
	}

	return FIRE_MAX_RADIUS_DEFAULT;
}


///////////////////////////////////////////////////////////////////////////////
//  CalcCurrRadius
///////////////////////////////////////////////////////////////////////////////

float			CFire::CalcCurrRadius				() const
{
	return m_currStrength * GetMaxRadius();
}


///////////////////////////////////////////////////////////////////////////////
//  CalcStrengthGenScalar
///////////////////////////////////////////////////////////////////////////////

float			CFire::CalcStrengthGenScalar		() const
{
	// calc the generation scalar
	float genScale = CVfxHelper::GetInterpValue(m_numGenerations, 0.0f, 5.0f);
	return 0.2f + (genScale*0.8f); 
}


///////////////////////////////////////////////////////////////////////////////
//  FindMtlBurnInfo
///////////////////////////////////////////////////////////////////////////////

bool			CFire::FindMtlBurnInfo			(float& burnTime, float& burnStrength, float& peakTime, phMaterialMgr::Id groundMtl, bool isPetrol) const
{
	if (isPetrol)
	{
		burnTime = FIRE_PETROL_BURN_TIME;
		burnStrength = FIRE_PETROL_BURN_STRENGTH;
		peakTime = FIRE_PETROL_TRAIL_PEAK_TIME;

		return true;
	}
	else
	{
		// get the flammability of the material 
		VfxFireInfo_s* pVfxFireInfo = g_vfxFire.GetInfo(PGTAMATERIALMGR->GetMtlVfxGroup(groundMtl));
		float mtlFlammability = pVfxFireInfo->flammability;
		mtlFlammability *= g_fireMan.GetScriptFlammabilityMultiplier();
#if __BANK
		if (g_fireMan.m_overrideMtlFlammability>=0.0f)
		{
			mtlFlammability = g_fireMan.m_overrideMtlFlammability;
		}
#endif

		// add on the existing fuel left in this fire
		float flam = Min(1.0f, mtlFlammability + (m_fuelLevel*FIRE_FUEL_FLAMMABILITY_MULT));

		if (flam>0.0f)
		{
			float mtlBurnTime = pVfxFireInfo->burnTime;	
			float mtlMaxStrength = pVfxFireInfo->burnStrength;	
#if __BANK
			if (g_fireMan.m_overrideMtlBurnTime>=0.0f)
			{
				mtlBurnTime = g_fireMan.m_overrideMtlBurnTime;
			}

			if (g_fireMan.m_overrideMtlBurnStrength>=0.0f)
			{
				mtlMaxStrength = g_fireMan.m_overrideMtlBurnStrength;
			}
#endif

			burnTime = mtlBurnTime + (m_fuelLevel*FIRE_FUEL_BURN_TIME_MULT); 
			burnStrength = Min(1.0f, mtlMaxStrength + (m_fuelLevel*FIRE_FUEL_BURN_STRENGTH_MULT));
			peakTime = Max(0.5f, (1.0f-flam)*burnTime*FIRE_BURNOUT_RATIO);

			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  Spread
///////////////////////////////////////////////////////////////////////////////

void			CFire::Spread							(bool onlySpreadToPetrol) 
{
	if (g_fireMan.PlentyFiresAvailable()==false)
	{
		return;
	}

	Vec3V vFirePos = GetPositionWorld();

#if __BANK
	Vec3V vRootPos;
	// set the new fire's root
	if (IsZeroAll(m_vRootPosWld))
	{
		vRootPos = vFirePos;
	}
	else
	{
		vRootPos = m_vRootPosWld;
	}
#endif

	// check for petrol pools around the fire 
	u32 foundDecalTypeFlag = 0;
	Vec3V vPetrolDecalPos;
	fwEntity* pPetrolDecalEntity = NULL;
	bool foundPetrol = false;
	bool foundPetrolPool = false;
	if (g_decalMan.GetDisablePetrolDecalsIgniting()==false)
	{
		foundPetrol = g_decalMan.FindClosest((1<<DECALTYPE_POOL_LIQUID | 1<<DECALTYPE_TRAIL_LIQUID), VFXLIQUID_TYPE_PETROL, vFirePos, FIRE_PETROL_SEARCH_DIST, 0.0f, false, true, FIRE_PETROL_BURN_TIME, vPetrolDecalPos, &pPetrolDecalEntity, foundDecalTypeFlag);
		if (foundPetrol)
		{
			foundPetrolPool = foundDecalTypeFlag==(1<<DECALTYPE_POOL_LIQUID);
		}
	}

	CEntity* pPetrolDecalEntityActual = NULL;
	if (pPetrolDecalEntity)
	{
		pPetrolDecalEntityActual = static_cast<CEntity*>(pPetrolDecalEntity);
	}

	if (onlySpreadToPetrol)
	{
		// petrol fires can only spread to other petrol
		if (foundPetrol)
		{		
			Vec3V vNormal = Vec3V(V_Z_AXIS_WZERO);
			CFire* pFire = g_fireMan.StartPetrolFire(pPetrolDecalEntityActual, vPetrolDecalPos, vNormal, GetCulprit(), foundPetrolPool, vFirePos, BANK_ONLY(vRootPos,) GetFlag(FIRE_FLAG_IS_SCRIPTED) || GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED), GetFireType(), GetFlag(FIRE_FLAG_CULPRIT_CLEARED));

			if (pFire)
			{
#if __BANK
				if (g_fireMan.m_ignoreMaxChildren==false)
#endif
				{
					m_numChildren++;
				}
			}
		}
	}
	else
	{
		// GROUND SPREADING
		Vec3V vTestPos;
		if (foundPetrol)
		{		
			vTestPos = vPetrolDecalPos;
		}
		else
		{
			// get a random position beside this fire
			vTestPos = vFirePos;
			if (m_regdEntity.Get()==NULL)
			{
				Vec3V vOutwardDir = Vec3V(V_ZERO);
				if (IsZeroAll(m_vParentPosWld)==false)
				{
					vOutwardDir = vFirePos - m_vParentPosWld;
					vOutwardDir.SetZ(ScalarV(V_ZERO));
					vOutwardDir = NormalizeSafe(vOutwardDir, Vec3V(V_X_AXIS_WZERO));
				}

				vTestPos.SetXf(g_DrawRand.GetRanged(-1.0f, 1.0f));
				vTestPos.SetYf(g_DrawRand.GetRanged(-1.0f, 1.0f));
				vTestPos.SetZ(ScalarV(V_ZERO));
				vTestPos += vOutwardDir * ScalarVFromF32(FIRE_SPREAD_GROUND_OUTWARD_DIR_MULT);
				vTestPos = NormalizeSafe(vTestPos, Vec3V(V_X_AXIS_WZERO));

				float currFireRadius = GetMaxRadius();//CalcCurrRadius();
				vTestPos *= ScalarVFromF32(g_DrawRand.GetRanged(currFireRadius*FIRE_SPREAD_GROUND_DIST_RADIUS_MULT_MIN, currFireRadius*FIRE_SPREAD_GROUND_DIST_RADIUS_MULT_MAX));
				vTestPos += vFirePos;
			}
		}

		// calc the grid indices for this position
		s32 gridX, gridY;
		g_fireMan.GetMapStatusGridIndices(vTestPos, gridX, gridY);

		// check if we can start a fire here
#if __BANK
		if (g_fireMan.m_disableGroundSpreading==false)
#endif
		{
			fireMapStatusGridData& gridData = g_fireMan.GetMapStatusGrid(gridX, gridY);
			if (gridData.m_timer==0.0f || vTestPos.GetZf()>gridData.m_height)
			{
				TriggerGroundInfoProbe(vTestPos, pPetrolDecalEntityActual, foundPetrol, foundPetrolPool);
			}
		}

		// UPWARD SPREADING
#if __BANK
		if (g_fireMan.m_disableUpwardSpreading==false)
#endif
		{
			float currFireRadius = GetMaxRadius();//CalcCurrRadius();
			float offsetZ = vFirePos.GetZf()+g_DrawRand.GetRanged(currFireRadius*FIRE_SPREAD_UP_DIST_RADIUS_UP_MULT_MIN, currFireRadius*FIRE_SPREAD_UP_DIST_RADIUS_UP_MULT_MAX);
			ScalarV vRadiusA = ScalarVFromF32(g_DrawRand.GetRanged(currFireRadius*FIRE_SPREAD_UP_DIST_RADIUS_OUT_MULT_MIN, currFireRadius*FIRE_SPREAD_UP_DIST_RADIUS_OUT_MULT_MAX));
			ScalarV vRadiusB = ScalarVFromF32(g_DrawRand.GetRanged(0.0f, currFireRadius*FIRE_SPREAD_UP_DIST_RADIUS_OUT_MULT_MIN));
			ScalarV vRadiusADouble = vRadiusA * ScalarV(V_TWO);

			Vec3V vTestPosA;
			{
				// get a random vector
				vTestPosA.SetXf(g_DrawRand.GetRanged(-1.0f, 1.0f));
				vTestPosA.SetYf(g_DrawRand.GetRanged(-1.0f, 1.0f));
				vTestPosA.SetZ(ScalarV(V_ZERO));
				vTestPosA = NormalizeSafe(vTestPosA, Vec3V(V_X_AXIS_WZERO));

				// set the length between the min and max radius
				vTestPosA *= vRadiusA;
				vTestPosA += vFirePos;

				// set the height
				vTestPosA.SetZf(offsetZ);
			}

			Vec3V vTestPosB;
			{
				// get a random vector
				vTestPosB.SetXf(g_DrawRand.GetRanged(-1.0f, 1.0f));
				vTestPosB.SetYf(g_DrawRand.GetRanged(-1.0f, 1.0f));
				vTestPosB.SetZ(ScalarV(V_ZERO));
				vTestPosB = NormalizeSafe(vTestPosB, Vec3V(V_X_AXIS_WZERO));

				// set the length between zero and the min radius
				vTestPosB *= vRadiusB;
				vTestPosB += vFirePos;

				// set the height
				vTestPosB.SetZf(offsetZ);
			}

			// extend B along the A->B vector
			Vec3V vAtoB = vTestPosB - vTestPosA;
			vAtoB = NormalizeSafe(vAtoB, Vec3V(V_X_AXIS_WZERO));
			vTestPosB = vTestPosA + (vAtoB*vRadiusADouble);

			TriggerUpwardProbe(vTestPosA, vTestPosB);
		}

		// OBJECT SPREADING
#if __BANK
		if (g_fireMan.m_disableObjectSpreading==false)
#endif
		{
			if (m_fireType==FIRETYPE_TIMED_OBJ_GENERIC)
			{
				if (vfxVerifyf(m_regdEntity->GetIsTypeObject(), "object fire type is not attached to an object"))
				{
					CObject* pObject = static_cast<CObject*>(m_regdEntity.Get());

					// create a fire on the object in a random place
					Vec3V vRandom(g_DrawRand.GetRanged(-1.0f, 1.0f), g_DrawRand.GetRanged(-1.0f, 1.0f), g_DrawRand.GetRanged(-1.0f, 1.0f));
					vRandom = NormalizeSafe(vRandom, Vec3V(V_X_AXIS_WZERO));
					vRandom *= ScalarVFromF32(pObject->GetBoundRadius()*2.0f);
					Vec3V vEndPos = GetPositionWorld();
					Vec3V vStartPos = vEndPos + vRandom;

#if __BANK
					if (g_fireMan.m_renderDebugSpreadProbes)
					{
						grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 0.0f, 1.0f, 1.0f), -30);
					}
#endif

					WorldProbe::CShapeTestHitPoint probeResult;
					WorldProbe::CShapeTestResults probeResults(probeResult);
					WorldProbe::CShapeTestProbeDesc probeDesc;
					probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
					probeDesc.SetResultsStructure(&probeResults);
					probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
					probeDesc.SetIncludeEntity(pObject);

					if (pObject && pObject->GetCurrentPhysicsInst() && WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
					{
						if (vfxVerifyf(probeResults[0].GetHitInst()==pObject->GetCurrentPhysicsInst(), "shape test against a specific object has returned a hit against a different object"))
						{
							// check that we're not too close to an existing fire on this object
							if (g_fireMan.GetClosestEntityFireDist(pObject, RCC_VEC3V(probeResults[0].GetHitPosition()))>FIRE_SPREAD_OBJECT_REJECT_DIST)
							{
								float burnTime, burnStrength, peakTime;
								if (FindMtlBurnInfo(burnTime, burnStrength, peakTime, probeResults[0].GetHitMaterialId(), false))
								{
									s16 newNumGenerations = GetNumGenerations()-1;
									if (newNumGenerations>=0)
									{
										burnStrength *= CalcStrengthGenScalar();
										
										s32 boneIndex = -1;
										fragInst* pFragInst = pObject->GetFragInst();
										if (pFragInst)
										{
											fragPhysicsLOD* pPhysicsLod = pFragInst->GetTypePhysics();
											if (pPhysicsLod)
											{
												fragTypeChild* pFragTypeChild = pPhysicsLod->GetChild(probeResults[0].GetHitComponent());
												if (pFragTypeChild)
												{
													u16 boneId = pFragTypeChild->GetBoneID();

													CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo(); 
													if (pModelInfo)
													{
														gtaFragType* pFragType = pModelInfo->GetFragType();
														if (pFragType)
														{
															boneIndex = pFragType->GetBoneIndexFromID(boneId);
														}
													}
												}
											}
										}

										Vec3V vPosLcl;
										Vec3V vNormLcl;
										if (CVfxHelper::GetLocalEntityBonePosDir(*pObject, boneIndex, probeResults[0].GetHitPositionV(), probeResults[0].GetHitNormalV(), vPosLcl, vNormLcl))
										{
											g_fireMan.StartObjectFire(pObject, boneIndex, vPosLcl, vNormLcl, GetCulprit(), burnTime, burnStrength, peakTime, newNumGenerations, vFirePos, BANK_ONLY(vRootPos,) GetFlag(FIRE_FLAG_IS_SCRIPTED) || GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED), 0, GetFlag(FIRE_FLAG_CULPRIT_CLEARED));
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


///////////////////////////////////////////////////////////////////////////////
//  ProcessVfx
///////////////////////////////////////////////////////////////////////////////

void			CFire::ProcessVfx						() 
{
#if __BANK
	if (g_fireMan.m_disablePtFx)
	{
		return;
	}
#endif
	
	// triggered fires
	if (m_fireType==FIRETYPE_TIMED_PED)
	{
		if (CLocalisation::PedsOnFire())
		{
			if (m_regdEntity && m_regdEntity->GetType()==ENTITY_TYPE_PED)
			{
				CPed* pPedOnFire = static_cast<CPed*>(m_regdEntity.Get());
				g_vfxFire.ProcessPedFire(this, pPedOnFire);
			}
		}
	}
	else if (m_fireType>=FIRETYPE_TIMED_VEH_WRECKED && m_fireType<=FIRETYPE_TIMED_VEH_WRECKED_3)
	{
		if (m_regdEntity && m_regdEntity->GetType()==ENTITY_TYPE_VEHICLE)
		{
			g_vfxFire.UpdatePtFxFireVehWrecked(this);
		}
	}
	else if (m_fireType==FIRETYPE_TIMED_VEH_GENERIC || m_fireType==FIRETYPE_TIMED_OBJ_GENERIC)
	{
		if (m_regdEntity && (m_regdEntity->GetType()==ENTITY_TYPE_VEHICLE || m_regdEntity->GetType()==ENTITY_TYPE_OBJECT))
		{
			g_vfxFire.UpdatePtFxFireEntity(this);
		}
	}
	else if (m_fireType==FIRETYPE_TIMED_MAP)
	{
		vfxAssertf(m_regdEntity==NULL, "map fires shouldn't be attached to an entity");
		g_vfxFire.UpdatePtFxFireMap(this);
	}
	else if (m_fireType==FIRETYPE_TIMED_PETROL_TRAIL)
	{
		g_vfxFire.UpdatePtFxFirePetrolTrail(this);
	}
	else if (m_fireType==FIRETYPE_TIMED_PETROL_POOL)
	{
		vfxAssertf(m_regdEntity==NULL, "petrol fires shouldn't be attached to an entity");
		g_vfxFire.UpdatePtFxFirePetrolPool(this);
	}
	// registered fires
	else if (m_fireType==FIRETYPE_REGD_VEH_WHEEL)
	{
		g_vfxFire.UpdatePtFxFireVehWheel(this);
	}
	else if (m_fireType==FIRETYPE_REGD_VEH_PETROL_TANK)
	{
		g_vfxFire.UpdatePtFxFirePetrolTank(this);
	}
	else if (m_fireType==FIRETYPE_REGD_FROM_PTFX)
	{
		// registered from a particle effect - no need to do any more particle effects
	}
	else
	{
		vfxAssertf(0, "particle effects not supported for this fire type (%d)", m_fireType);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessAsynchProbes
///////////////////////////////////////////////////////////////////////////////

void			CFire::ProcessAsynchProbes			()
{
#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
	{
		return;
	}
#endif

	// scorch mark probes
	if (m_scorchMarkVfxProbeId>-1)
	{
		VfxAsyncProbeResults_s vfxProbeResults;
		if (CVfxAsyncProbeManager::QueryProbe(m_scorchMarkVfxProbeId, vfxProbeResults))
		{
			m_scorchMarkVfxProbeId = -1;

			if (vfxProbeResults.hasIntersected)
			{
				CEntity* pEntity = CVfxAsyncProbeManager::GetEntity(vfxProbeResults);
				if (pEntity)
				{
					VfxCollInfo_s vfxCollInfo;
					vfxCollInfo.regdEnt = pEntity;
					vfxCollInfo.vPositionWld = vfxProbeResults.vPos;
					vfxCollInfo.vNormalWld = vfxProbeResults.vNormal;
					vfxCollInfo.vDirectionWld = -vfxCollInfo.vNormalWld;
					vfxCollInfo.materialId = PGTAMATERIALMGR->UnpackMtlId(vfxProbeResults.mtlId);
					vfxCollInfo.roomId = PGTAMATERIALMGR->UnpackRoomId(vfxProbeResults.mtlId);
					vfxCollInfo.componentId = vfxProbeResults.componentId;
					vfxCollInfo.weaponGroup = WEAPON_EFFECT_GROUP_FIRE;
					vfxCollInfo.force = VEHICLEGLASSFORCE_NOT_USED;
					vfxCollInfo.isBloody = false;
					vfxCollInfo.isWater = false;
					vfxCollInfo.isExitFx = false;
					vfxCollInfo.noPtFx = false;
					vfxCollInfo.noPedDamage = false;
					vfxCollInfo.noDecal = false;
					vfxCollInfo.isSnowball = false;
					vfxCollInfo.isFMJAmmo = false;

					// play any weapon impact effects
					g_vfxWeapon.DoWeaponImpactVfx(vfxCollInfo, DAMAGE_TYPE_FIRE, m_regdCulprit);
				}
			}
		}
	}

	// ground info probes
	if (m_groundInfoVfxProbeId>-1)
	{
		VfxAsyncProbeResults_s vfxProbeResults;
		if (CVfxAsyncProbeManager::QueryProbe(m_groundInfoVfxProbeId, vfxProbeResults))
		{
			m_groundInfoVfxProbeId = -1;

			if (vfxProbeResults.hasIntersected)
			{
				Vec3V vGroundPos = vfxProbeResults.vPos;

				// check if this is underwater
				bool isUnderwater = false;
				float waterZ;
				if (Water::GetWaterLevelNoWaves(RCC_VECTOR3(vGroundPos), &waterZ, POOL_DEPTH, REJECTIONABOVEWATER, NULL))
				{
					if (waterZ > vGroundPos.GetZf())
					{
						// Position is underwater - test upwards in case we're in a tunnel.
						Vec3V vStartPos(vGroundPos.GetXf(), vGroundPos.GetYf(), vGroundPos.GetZf()+0.05f);
						float zDist = waterZ-vGroundPos.GetZf()-0.1f;
						Vec3V vEndPos = vStartPos + Vec3V(0.0f, 0.0f, zDist);
						WorldProbe::CShapeTestProbeDesc probeDesc;
						probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
						probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
						probeDesc.SetExcludeEntity(NULL);
						probeDesc.SetResultsStructure(NULL);
						if (!WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
						{
							isUnderwater = true;
						}
					}
				}

				if (!isUnderwater)
				{
					// calc the grid indices for this position
					s32 gridX, gridY;
					g_fireMan.GetMapStatusGridIndices(vGroundPos, gridX, gridY);

					// check if the status grid is still clear 
					// the probe is now asynchronous so the grid entry may have become active since we triggered the probe
					fireMapStatusGridData& gridData = g_fireMan.GetMapStatusGrid(gridX, gridY);
					if (gridData.m_timer==0.0f || vGroundPos.GetZf()>gridData.m_height)
					{
						Vec3V vGroundNormal = vfxProbeResults.vNormal;
						vfxAssertf(IsEqualAll(vGroundNormal, vGroundNormal), "fire probe normal is not a valid number");

						phMaterialMgr::Id groundMtl = vfxProbeResults.mtlId;

						float burnTime, burnStrength, peakTime;
						if (FindMtlBurnInfo(burnTime, burnStrength, peakTime, groundMtl, m_groundInfoVfxProbeFoundPetrol))
						{
							// update the burn strength based on the max allowed for this generation
							burnStrength *= CalcStrengthGenScalar();

							Vec3V vFirePos = GetPositionWorld();
#if __BANK
							Vec3V vRootPos;
							// set the new fire's root
							if (IsZeroAll(m_vRootPosWld))
							{
								vRootPos = vFirePos;
							}
							else
							{
								vRootPos = m_vRootPosWld;
							}
#endif

							// try to create a new fire here
							CFire* pNewFire = NULL;
							if (m_groundInfoVfxProbeFoundPetrol)
							{
								pNewFire = g_fireMan.StartPetrolFire(m_pGroundInfoVfxProbePetrolEntity, vGroundPos, vGroundNormal, m_regdCulprit, m_groundInfoVfxProbeFoundPetrolPool, vFirePos, BANK_ONLY(vRootPos,) GetFlag(FIRE_FLAG_IS_SCRIPTED) || GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED), GetFireType(), GetFlag(FIRE_FLAG_CULPRIT_CLEARED));
							}
							else
							{
								pNewFire = g_fireMan.StartMapFire(vGroundPos, vGroundNormal, groundMtl, m_regdCulprit, burnTime, burnStrength, peakTime, m_fuelLevel, m_fuelBurnRate, m_numGenerations-1, vFirePos, BANK_ONLY(vRootPos,) GetFlag(FIRE_FLAG_IS_SCRIPTED) || GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED), GetWeaponHash(), GetFlag(FIRE_FLAG_CULPRIT_CLEARED));
							}

							if (pNewFire)
							{
#if __BANK
								if (g_fireMan.m_ignoreMaxChildren==false)
#endif
								{
									m_numChildren++;
								}
							}
						}
					}
				}
			}
		}
	}

	// upward probes
	if (m_upwardVfxProbeId>-1)
	{
		VfxAsyncProbeResults_s vfxProbeResults;
		if (CVfxAsyncProbeManager::QueryProbe(m_upwardVfxProbeId, vfxProbeResults))
		{
			m_upwardVfxProbeId = -1;

			if (vfxProbeResults.hasIntersected)
			{
				Vec3V vWallPos = vfxProbeResults.vPos;

				// calc the grid indices for this position
				s32 gridX, gridY;
				g_fireMan.GetMapStatusGridIndices(vWallPos, gridX, gridY);

				// check if the status grid is still clear 
				// the probe is now asynchronous so the grid entry may have become active since we triggered the probe
				fireMapStatusGridData& gridData = g_fireMan.GetMapStatusGrid(gridX, gridY);
				if (gridData.m_timer==0.0f || vWallPos.GetZf()>gridData.m_height)
				{
					Vec3V vWallNormal = vfxProbeResults.vNormal;
					vfxAssertf(IsEqualAll(vWallNormal, vWallNormal), "fire probe normal is not a valid number");

					phMaterialMgr::Id wallMtl = vfxProbeResults.mtlId;

					float burnTime, burnStrength, peakTime;
					if (FindMtlBurnInfo(burnTime, burnStrength, peakTime, wallMtl, false))
					{
						// update the burn strength based on the max allowed for this generation
						burnStrength *= CalcStrengthGenScalar();

						Vec3V vFirePos = GetPositionWorld();
#if __BANK
						Vec3V vRootPos;
						// set the new fire's root
						if (IsZeroAll(m_vRootPosWld))
						{
							vRootPos = vFirePos;
						}
						else
						{
							vRootPos = m_vRootPosWld;
						}
#endif

						// try to create a new fire here
						CFire* pNewFire = g_fireMan.StartMapFire(vWallPos, vWallNormal, wallMtl, NULL, burnTime, burnStrength, peakTime, m_fuelLevel, m_fuelBurnRate, m_numGenerations-1, vFirePos, BANK_ONLY(vRootPos,) GetFlag(FIRE_FLAG_IS_SCRIPTED) || GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED), GetWeaponHash(), GetFlag(FIRE_FLAG_CULPRIT_CLEARED));
						if (pNewFire)
						{
#if __BANK
							if (g_fireMan.m_ignoreMaxChildren==false)
#endif
							{
								m_numChildren++;
							}
						}
					}
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerScorchMarkProbe
///////////////////////////////////////////////////////////////////////////////

void			CFire::TriggerScorchMarkProbe			()
{
	if (m_scorchMarkVfxProbeId==-1)
	{
		Vec3V_In vPos = GetPositionWorld();
		Vec3V vStartPos = vPos + (m_vNormLcl*ScalarVFromF32(0.05f));
		Vec3V vEndPos = vPos - (m_vNormLcl*ScalarVFromF32(0.05f));

		m_scorchMarkVfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerGroundInfoProbe
///////////////////////////////////////////////////////////////////////////////

void			CFire::TriggerGroundInfoProbe			(Vec3V_In vTestPos, CEntity* pPetrolEntity, bool foundPetrol, bool foundPetrolPool)
{
	if (m_groundInfoVfxProbeId==-1)
	{
		float fireRadius = CalcCurrRadius();

		// test this position for ground
		Vec3V vStartPos = vTestPos;
		vStartPos.SetZf(vStartPos.GetZf()+(fireRadius*FIRE_SPREAD_HEIGHT_RADIUS_OFFSET));
		Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, (fireRadius*FIRE_SPREAD_HEIGHT_RADIUS_MULT));

		m_groundInfoVfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER);
		m_pGroundInfoVfxProbePetrolEntity = pPetrolEntity;
		m_groundInfoVfxProbeFoundPetrol = foundPetrol;
		m_groundInfoVfxProbeFoundPetrolPool = foundPetrolPool;

#if __BANK
		if (g_fireMan.m_renderDebugSpreadProbes)
		{
			grcDebugDraw::Line(vStartPos, vEndPos, Color32(0.0f, 1.0f, 0.0f, 1.0f), -30);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  TriggerUpwardProbe
///////////////////////////////////////////////////////////////////////////////

void			CFire::TriggerUpwardProbe				(Vec3V_In vStartPos, Vec3V_In vEndPos)
{
	if (m_upwardVfxProbeId==-1)
	{
		m_upwardVfxProbeId = CVfxAsyncProbeManager::SubmitWorldProbe(vStartPos, vEndPos, ArchetypeFlags::GTA_MAP_TYPE_MOVER);

#if __BANK
		if (g_fireMan.m_renderDebugSpreadProbes)
		{
			grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 0.0f, 0.0f, 1.0f), -30);
		}
#endif
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CanDispatchFireTruckTo
///////////////////////////////////////////////////////////////////////////////

bool			CFire::CanDispatchFireTruckTo			()
{
	bool validFlags = GetFlag(FIRE_FLAG_IS_ACTIVE) && !GetFlag(FIRE_FLAG_IS_SCRIPTED) && !GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED);
	bool validType = m_fireType==FIRETYPE_TIMED_MAP || (m_fireType>=FIRETYPE_TIMED_VEH_WRECKED && m_fireType<=FIRETYPE_TIMED_VEH_WRECKED_3) || m_fireType==FIRETYPE_TIMED_VEH_GENERIC || m_fireType==FIRETYPE_TIMED_OBJ_GENERIC || m_fireType == FIRETYPE_REGD_VEH_PETROL_TANK;
	bool isNotFadingOut = m_currBurnTime<m_totalBurnTime*FIRE_BURNOUT_RATIO;

	return (validFlags && validType && isNotFadingOut);
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterFireTruckDispatched
///////////////////////////////////////////////////////////////////////////////

void			CFire::RegisterInUseByDispatch		()
{
	SetFlag(FIRE_FLAG_DONT_UPDATE_TIME_THIS_FRAME);
}


///////////////////////////////////////////////////////////////////////////////
//  ActivateDormantFire
///////////////////////////////////////////////////////////////////////////////

void			CFire::ActivateDormantFire		()
{
	if (vfxVerifyf(GetFlag(FIRE_FLAG_IS_DORMANT), "trying to activate a dormant fire that isn't dormant"))
	{
		ClearFlag(FIRE_FLAG_IS_DORMANT);
		SetFlag(FIRE_FLAG_IS_ACTIVE);

		g_fireMan.ActivateFireCB(this);
	}
}



///////////////////////////////////////////////////////////////////////////////
//  CLASS CFireManager
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

bool			CFireManager::Init						(unsigned initMode)
{
	if (initMode == INIT_CORE || initMode == INIT_AFTER_MAP_LOADED)
	{
		// initialise the fire array
		for (s32 i=0; i<MAX_FIRES; i++)
		{
			m_fires[i].m_flags = 0;
		}

		// map status grid
		m_mapStatusGridActive = false;
		for (s32 i=0; i<FIRE_MAP_GRID_SIZE; i++)
		{
			for (s32 j=0; j<FIRE_MAP_GRID_SIZE; j++)
			{
				m_mapStatusGrid[i][j].m_timer = 0.0f;
				m_mapStatusGrid[i][j].m_height = -9999.0f;
			}
		}

		// object status
		m_objectStatusActive = false;
		for (s32 i=0; i<MAX_OBJECTS_ON_FIRE; i++)
		{
			m_objectStatus[i].m_pObject = NULL;
			m_objectStatus[i].m_canBurnTimer = 0.0f;
			m_objectStatus[i].m_dontBurnTimer = 0.0f;
		}

		m_numFiresActive = 0;

		// Initially reserve enough room for max entities from one search. 
		// Array can grow larger if needed. This is called on startup and 
		// on map/level load, so make sure to reset before reserving.
		m_EntitiesNextToFires.Reset();
		m_EntitiesNextToFires.Reserve(SPREAD_FIRE_SEARCH_MAX_ENTITY);

		m_scriptFlammabilityMultiplierScriptThreadId = THREAD_INVALID;
		m_scriptFlammabilityMultiplier = 1.0f;

#if __BANK | GTA_REPLAY
		m_disableAllSpreading = false;
#endif

#if __BANK
		m_overrideMtlFlammability = -1.0f;
		m_overrideMtlBurnTime = -1.0f;
		m_overrideMtlBurnStrength = -1.0f;

		m_debugBurnTime = 10.0f;
		m_debugBurnStrength = 1.0f;
		m_debugPeakTime = 1.0f;
		m_debugFuelLevel = 0.0f;
		m_debugFuelBurnRate = 0.0f;
		m_debugNumGenerations = FIRE_DEFAULT_NUM_GENERATIONS;

		m_disablePtFx = false;
		m_disableDecals = false;
		m_disableGroundSpreading = false;
		m_disableUpwardSpreading = false;
		m_disableObjectSpreading = false;
		m_ignoreGenerations = false;
		m_ignoreMaxChildren = false;
		m_ignoreStatusGrid = false;
		m_renderDebugSpheres = false;
		m_renderDebugParents = false;
		m_renderDebugRoots = false;
		m_renderDebugSpreadProbes = false;
		m_renderStartFireProbes = false;
		m_renderMapStatusGrid = false;
		m_renderMapStatusGridHeight = 1.0f;
		m_renderMapStatusGridAlpha = 0.25f;
		m_renderMapStatusGridMinZ = -9999.0f;
		m_renderObjectStatus = false;
		m_renderPedNextToFireTests = false;
		m_renderVehicleNextToFireTests = false;
#endif

#if GTA_REPLAY
		m_replayIsScriptedFire = false;
		m_replayIsPetrolFire = false;
#endif //GTA_REPLAY

	}

	return true;
}

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::Shutdown					(unsigned shutdownMode)
{
	if (shutdownMode==SHUTDOWN_CORE || shutdownMode==SHUTDOWN_WITH_MAP_LOADED || shutdownMode==SHUTDOWN_SESSION)
	{
		// cache the active fire array info as we'll potentially alter it as we go through shutting down fires
		s32 numFiresActive = m_numFiresActive;
		s32	activeFireIndices[MAX_FIRES];
		sysMemCpy(&activeFireIndices[0], &m_activeFireIndices[0], numFiresActive*sizeof(s32));

		for (s32 i=0; i<numFiresActive; i++)
		{
			CFire* pFire = &m_fires[activeFireIndices[i]];
			vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

			pFire->Shutdown();
		}
	}
}

///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void CFireManager::Update (float deltaTime)
{
	UpdateTask(deltaTime);
}

void CFireManager::ManageWreckedFires()
{
	// check how many wrecked fires there currently are that aren't fading out
	// and store the one with the largest life ratio that's at full strength
	u32 numWreckedFiresNotFadingOut = 0;
	float maxWreckedFireLifeRatio = 0.0f;
	CFire* pWreckedFireWithMaxLifeRatio = NULL;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED ||
			pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED_2 ||
			pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED_3)
		{
			// check if this fire isn't fading out
			float burnOutStartTime = pFire->m_totalBurnTime*FIRE_BURNOUT_RATIO;
			if (pFire->m_currBurnTime<burnOutStartTime)
			{
				// check if this fire isn't fading in
				if (pFire->m_currBurnTime>pFire->m_maxStrengthTime)
				{
					float currLifeRatio = pFire->GetLifeRatio();
					if (currLifeRatio>=maxWreckedFireLifeRatio)
					{
						maxWreckedFireLifeRatio = currLifeRatio;
						pWreckedFireWithMaxLifeRatio = pFire;
					}
				}

				numWreckedFiresNotFadingOut++;
			}
		}
	}

	// if we have too many wrecked fires then start fading the one that's closest to expiring
	if (numWreckedFiresNotFadingOut>VFXVEHICLE_NUM_WRECKED_FIRES_FADE_THRESH && pWreckedFireWithMaxLifeRatio!=NULL)
	{
		float burnOutStartTime = pWreckedFireWithMaxLifeRatio->m_totalBurnTime*FIRE_BURNOUT_RATIO;
		pWreckedFireWithMaxLifeRatio->m_currBurnTime = burnOutStartTime;
	}
}

bool CFireManager::ManageAddedPetrol(Vec3V_In vPos, float dist)
{
	bool foundFires = false;

	for (int i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (pFire->GetFlag(FIRE_FLAG_IS_REGISTERED)==false)
		{
			Vec3V vDecalToFire = pFire->GetPositionWorld()-vPos;
			if (MagSquared(vDecalToFire).Getf()<dist*dist)
			{
				// set to full strength (as if just faded in)
				if (pFire->m_currBurnTime>pFire->m_maxStrengthTime)
				{
					pFire->m_currBurnTime = pFire->m_maxStrengthTime;
				}
				foundFires = true;
			}
		}

	}

	return foundFires;
}

void CFireManager::UpdateTask(float deltaTime)
{
	PIXBegin(1, "Fire update");

	ManageWreckedFires();

	// cache the active fire array info as we'll alter it as we go through shutting down fires
	s32 numFiresActive = m_numFiresActive;
	s32	activeFireIndices[MAX_FIRES];
	sysMemCpy(&activeFireIndices[0], &m_activeFireIndices[0], numFiresActive*sizeof(s32));

	for (s32 i=0; i<numFiresActive; i++)
	{
		CFire* pFire = &m_fires[activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		pFire->Update(deltaTime);
	}

	// update the map status grid
	if (m_mapStatusGridActive)
	{
		bool isStillActive = false;
		for (s32 i=0; i<FIRE_MAP_GRID_SIZE; i++)
		{
			for (s32 j=0; j<FIRE_MAP_GRID_SIZE; j++)
			{
				if (m_mapStatusGrid[i][j].m_timer>0.0f)
				{
					m_mapStatusGrid[i][j].m_timer -= deltaTime;
					if (m_mapStatusGrid[i][j].m_timer<=0.0f)
					{
						m_mapStatusGrid[i][j].m_timer = 0.0f;
						m_mapStatusGrid[i][j].m_height = -9999.0f;
					}

					if (isStillActive==false)
					{
						isStillActive = true;
					}
				}
			}
		}

		m_mapStatusGridActive = isStillActive;
	}

	// update the object status
	if (m_objectStatusActive)
	{
		bool isStillActive = false;
		for (s32 i=0; i<MAX_OBJECTS_ON_FIRE; i++)
		{
			if (m_objectStatus[i].m_pObject)
			{
				if (m_objectStatus[i].m_canBurnTimer>0.0f)
				{
					m_objectStatus[i].m_canBurnTimer = Max(0.0f, m_objectStatus[i].m_canBurnTimer-deltaTime);
				}
				else if (m_objectStatus[i].m_dontBurnTimer>0.0f)
				{
					m_objectStatus[i].m_dontBurnTimer = Max(0.0f, m_objectStatus[i].m_dontBurnTimer-deltaTime);
				}
				else
				{
					m_objectStatus[i].m_pObject = NULL;
				}

				if (isStillActive==false && m_objectStatus[i].m_pObject)
				{
					isStillActive = true;
				}
			}
		}

		m_objectStatusActive = isStillActive;
	}

#if __BANK
	m_numScriptedFires = 0;
	m_numScriptedFiresActive = 0;
	for (s32 i=0; i<MAX_FIRES; i++)
	{
		if (m_fires[i].GetFlag(FIRE_FLAG_IS_SCRIPTED))
		{
			m_numScriptedFires++;

			if (m_fires[i].GetFlag(FIRE_FLAG_IS_ACTIVE))
			{
				m_numScriptedFiresActive++;
			}
		}
	}
#endif

	PIXEnd();
}

void CFireManager::UnregisterPathserverID(const TDynamicObjectIndex dynObjId)
{
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		if(pFire->GetPathServerObjectIndex()==dynObjId)
		{
			pFire->SetPathServerObjectIndex(DYNAMIC_OBJECT_INDEX_NONE);
		}
	}
}

bool CFireManager::CanSetPedOnFire(CPed* pPed)
{
	if (pPed->m_nPhysicalFlags.bRenderScorched)
	{
		return false;
	}

	// only try to set fire if the ped isn't in a vehicle (unless it's a motorbike or bicycle)
	if (!pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) || 
		(pPed->GetMyVehicle() && (pPed->GetMyVehicle()->InheritsFromBike() || pPed->GetMyVehicle()->InheritsFromQuadBike() || pPed->GetMyVehicle()->InheritsFromAmphibiousQuadBike())))
	{
		return true;
	}
	else
	{
		if (NetworkInterface::IsGameInProgress() && pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetVehicleModelInfo())
		{
			// Open top vehicles
			bool bCanSetPedOnFire = pPed->GetMyVehicle()->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_PEDS_INSIDE_CAN_BE_SET_ON_FIRE_MP);
			if (!bCanSetPedOnFire)
			{
				// Hanging seats
				bCanSetPedOnFire = pPed->IsCollisionEnabled(); 
			}
			return bCanSetPedOnFire;
		}
		return false;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////
#if __BANK
void			CFireManager::InitWidgets						()
{
	char txt[128];

	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Fire", false);
	{
		pVfxBank->AddTitle("INFO");
		sprintf(txt, "Num Fires Active(%d)", MAX_FIRES);
		pVfxBank->AddSlider(txt, &m_numFiresActive, 0, MAX_FIRES, 0);
		pVfxBank->AddSlider("Num Active Scripted Fires", &m_numScriptedFiresActive, 0, MAX_FIRES, 0);
		pVfxBank->AddSlider("Num Scripted Fires", &m_numScriptedFires, 0, MAX_FIRES, 0);

#if __DEV
		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("SETTINGS");
		pVfxBank->AddSlider("Fire Radius Default ", &FIRE_MAX_RADIUS_DEFAULT, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Fire Radius Map ", &FIRE_MAX_RADIUS_MAP, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Fire Radius Petrol", &FIRE_MAX_RADIUS_PETROL, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Fire Radius Ped ", &FIRE_MAX_RADIUS_PED, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Fire Radius Object Generic ", &FIRE_MAX_RADIUS_OBJ_GENERIC, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Fire Radius Vehicle Generic", &FIRE_MAX_RADIUS_VEH_GENERIC, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Fire Radius Particle", &FIRE_MAX_RADIUS_PTFX, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Max Children", &FIRE_MAX_CHILDREN, 0, 10, 1);
		pVfxBank->AddSlider("Status Grid Cell Size", &FIRE_STATUS_GRID_CELL_SIZE, 1, 5, 1);
		pVfxBank->AddSlider("Spread Flammability Multiplier (script)", &m_scriptFlammabilityMultiplier, 0.0f, 2.0f, 0.01f);
		pVfxBank->AddSlider("Spread Chance - Seconds Per Random Try", &FIRE_SPREAD_CHANCE_SECS_PER_RAND_TRY, 0.0f, 1.0f, 0.0001f);
		pVfxBank->AddSlider("Spread Chance - Low Flammability", &FIRE_SPREAD_CHANCE_LO_FLAM, 0.0f, 100.0f, 0.01f);
		pVfxBank->AddSlider("Spread Chance - High Flammability", &FIRE_SPREAD_CHANCE_HI_FLAM, 0.0f, 100.0f, 0.01f);
		pVfxBank->AddSlider("Spread Ground Dist Radius Mult Min", &FIRE_SPREAD_GROUND_DIST_RADIUS_MULT_MIN, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Spread Ground Dist Radius Mult Max", &FIRE_SPREAD_GROUND_DIST_RADIUS_MULT_MAX, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Spread Ground Outward Dir Mult", &FIRE_SPREAD_GROUND_OUTWARD_DIR_MULT, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Spread Up Dist Radius Up Mult Min", &FIRE_SPREAD_UP_DIST_RADIUS_UP_MULT_MIN, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Spread Up Dist Radius Up Mult Max", &FIRE_SPREAD_UP_DIST_RADIUS_UP_MULT_MAX, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Spread Up Dist Radius Out Mult Min", &FIRE_SPREAD_UP_DIST_RADIUS_OUT_MULT_MIN, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Spread Up Dist Radius Out Mult Max", &FIRE_SPREAD_UP_DIST_RADIUS_OUT_MULT_MAX, 0.0f, 10.0f, 0.05f);
		pVfxBank->AddSlider("Spread Height Radius Offset", &FIRE_SPREAD_HEIGHT_RADIUS_OFFSET, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Spread Height Radius Mult", &FIRE_SPREAD_HEIGHT_RADIUS_MULT, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Spread Object Reject Dist", &FIRE_SPREAD_OBJECT_REJECT_DIST, 0.0f, 5.0f, 0.05f);
		pVfxBank->AddSlider("Fuel Spread Chance", &FIRE_FUEL_SPREAD_CHANCE, 0.0f, 100.0f, 0.01f);
		pVfxBank->AddSlider("Fuel Flammability Mult", &FIRE_FUEL_FLAMMABILITY_MULT, 0.0f, 100.0f, 0.1f);
		pVfxBank->AddSlider("Fuel Burn Time Mult", &FIRE_FUEL_BURN_TIME_MULT, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Fuel Burn Strength Mult", &FIRE_FUEL_BURN_STRENGTH_MULT, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Ped Burn Time", &FIRE_PED_BURN_TIME, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Player Burn Time", &FIRE_PLAYER_BURN_TIME, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Player Burn Time (Explosion)", &FIRE_PLAYER_BURN_TIME_EXPLOSION, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Petrol Burn Time", &FIRE_PETROL_BURN_TIME, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Petrol Burn Strength", &FIRE_PETROL_BURN_STRENGTH, 0.0f, 2.0f, 0.1f);
		pVfxBank->AddSlider("Petrol Pool Peak Time", &FIRE_PETROL_POOL_PEAK_TIME, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Petrol Trail Peak Time", &FIRE_PETROL_TRAIL_PEAK_TIME, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Petrol Spread Life Ratio", &FIRE_PETROL_SPREAD_LIFE_RATIO, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Petrol Search Dist", &FIRE_PETROL_SEARCH_DIST, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Extinguish Time Mult", &FIRE_EXTINGUISH_TIME_MULT, 0.0f, 25.0f, 0.1f);
		pVfxBank->AddSlider("Entity Test Interval", &FIRE_ENTITY_TEST_INTERVAL, 0, 5000, 10);
#endif

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG MAP FIRE");
		pVfxBank->AddSlider("Burn Time", &m_debugBurnTime, 0.0f, 50.0f, 0.5f);
		pVfxBank->AddSlider("Burn Strength", &m_debugBurnStrength, 0.0f, 1.0f, 0.05f);
		pVfxBank->AddSlider("Peak Time", &m_debugPeakTime, 0.0f, 10.0f, 0.5f);
		pVfxBank->AddSlider("Fuel Level", &m_debugFuelLevel, 0.0f, 1.0f, 0.05f);
		pVfxBank->AddSlider("Fuel Burn Rate", &m_debugFuelBurnRate, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Num Generations", &m_debugNumGenerations, 0, 25, 1);
		pVfxBank->AddButton("Start Map Fire", StartDebugMapFire);
		pVfxBank->AddButton("Start Script Fire", StartDebugScriptFire);
		pVfxBank->AddButton("Start Focus Ped Fire", StartDebugFocusPedFire);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DEBUG MISC");
		pVfxBank->AddSlider("Override Mtl Flammability", &m_overrideMtlFlammability, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddSlider("Override Mtl Burn Time", &m_overrideMtlBurnTime, -1.0f, 30.0f, 0.5f);
		pVfxBank->AddSlider("Override Mtl Burn Strength", &m_overrideMtlBurnStrength, -1.0f, 1.0f, 0.01f);
		pVfxBank->AddToggle("Disable PtFx", &m_disablePtFx);
		pVfxBank->AddToggle("Disable Decals", &m_disableDecals);
		pVfxBank->AddToggle("Disable All Spreading", &m_disableAllSpreading);
		pVfxBank->AddToggle("Disable Ground Spreading", &m_disableGroundSpreading);
		pVfxBank->AddToggle("Disable Upward Spreading", &m_disableUpwardSpreading);
		pVfxBank->AddToggle("Disable Object Spreading", &m_disableObjectSpreading);
		pVfxBank->AddToggle("Ignore Generations", &m_ignoreGenerations);
		pVfxBank->AddToggle("Ignore Max Children", &m_ignoreMaxChildren);
		pVfxBank->AddToggle("Ignore Status Grid", &m_ignoreStatusGrid);
		pVfxBank->AddButton("Extinguish Area Around Player", ExtinguishAreaAroundPlayer);
		pVfxBank->AddButton("Remove From Area Around Player", RemoveFromAreaAroundPlayer);
		pVfxBank->AddToggle("Render Debug Spheres", &m_renderDebugSpheres);
		pVfxBank->AddToggle("Render Debug Parents", &m_renderDebugParents);
		pVfxBank->AddToggle("Render Debug Roots", &m_renderDebugRoots);
		pVfxBank->AddToggle("Render Spread Probes", &m_renderDebugSpreadProbes);
		pVfxBank->AddToggle("Render Start Fire Probes", &m_renderStartFireProbes);
		pVfxBank->AddToggle("Render Map Status Grid", &m_renderMapStatusGrid);
		pVfxBank->AddSlider("Render Map Status Grid Height", &m_renderMapStatusGridHeight, -10.0f, 10.0f, 0.1f);
		pVfxBank->AddSlider("Render Map Status Grid Alpha", &m_renderMapStatusGridAlpha, 0.0f, 1.0f, 0.01f);
		pVfxBank->AddButton("Output Map Status Grid", OutputStatusGrid);
		pVfxBank->AddButton("Reset Map Status Grid", ResetStatusGrid);
		pVfxBank->AddToggle("Render Object Status Grid", &m_renderObjectStatus);
		pVfxBank->AddToggle("Render Ped Next To Fire Tests", &m_renderPedNextToFireTests);
		pVfxBank->AddToggle("Render Vehicle Next To Fire Tests", &m_renderVehicleNextToFireTests);
		pVfxBank->AddButton("Reset Object Status", ResetObjectStatus);
		pVfxBank->AddButton("Reset", Reset);

		pVfxBank->AddTitle("");
		pVfxBank->AddTitle("DATA FILE");
		pVfxBank->AddButton("Reload", Reset);
	}
	pVfxBank->PopGroup();
}

///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::RenderDebug				()
{
	if (m_renderDebugSpheres || m_renderDebugParents || m_renderDebugRoots)
	{
		for (s32 i=0; i<MAX_FIRES; i++)
		{
			m_fires[i].RenderDebug();
		}
	}

	if (m_renderMapStatusGrid)
	{
		RenderStatusGrid();
	}

	if (m_renderObjectStatus)
	{
		RenderObjectStatus();
	}
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  ActivateFireCB
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ActivateFireCB		(CFire* pFire)
{
	// update our active fire index array

	// get the fire index
	s32 fireIndex = ptrdiff_t_to_int(pFire - &m_fires[0]);

	// check this index isn't in the list already
#if __ASSERT
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		vfxAssertf(m_activeFireIndices[i]!=fireIndex, "fire index is in the active list already");
	}
#endif
	// add this index to the list
	m_activeFireIndices[m_numFiresActive] = fireIndex;
	m_numFiresActive++;
}

///////////////////////////////////////////////////////////////////////////////
//  DeactivateFireCB
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::DeactivateFireCB			(CFire* ASSERT_ONLY(pFire))
{
	// check that this fire was in the active fire index array
#if __ASSERT
	// get the fire index
	s32 fireIndex = ptrdiff_t_to_int(pFire - &m_fires[0]);

	// go through the active fire index array to find this fire
	bool foundFire = false;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		if (m_activeFireIndices[i]==fireIndex)
		{
			foundFire = true;
			break;
		}
	}

	vfxAssertf(foundFire, "didn't find the fire in the active fire index array");
#endif

	// rebuild the active fire index array
#if __ASSERT
	s32 prevNumFiresActive = m_numFiresActive;
#endif

	m_numFiresActive = 0;
	for (s32 i=0; i<MAX_FIRES; i++)
	{
		if (m_fires[i].GetFlag(FIRE_FLAG_IS_ACTIVE))
		{
			m_activeFireIndices[m_numFiresActive] = i;
			m_numFiresActive++;
		}
	}

#if __ASSERT
	vfxAssertf(m_numFiresActive == prevNumFiresActive-1, "error rebuilding active fire index array (prev %d, now %d)", prevNumFiresActive, m_numFiresActive);
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  StartMapFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::StartMapFire				(Vec3V_In vFirePos, 
														 Vec3V_In vFireNormal,
														 phMaterialMgr::Id mtlId,
														 CPed* pCulprit, 
														 float burnTime,
														 float burnStrength,
														 float peakTime,
														 float fuelLevel,
														 float fuelBurnRate,
														 s32 numGenerations,
														 Vec3V_In vParentPos,
														 BANK_ONLY(Vec3V_In vRootPos,)
														 bool hasSpreadFromScriptedFire,
														 u32 weaponHash,
														 bool hasCulpritBeenCleared)
{
	// clones can't generate fires: the machine controlling the culprit is responsible for that
	if (pCulprit && pCulprit->IsNetworkClone())
	{
		return NULL;
	}

	CFireSettings fireSettings;
	fireSettings.vPosLcl = vFirePos;
	fireSettings.vNormLcl = vFireNormal;
	fireSettings.fireType = FIRETYPE_TIMED_MAP;
	fireSettings.mtlId = mtlId;
	fireSettings.burnTime = burnTime;
	fireSettings.burnStrength = burnStrength;
	fireSettings.peakTime = peakTime;
	fireSettings.fuelLevel = fuelLevel;
	fireSettings.fuelBurnRate = fuelBurnRate;
	fireSettings.numGenerations = numGenerations;
	fireSettings.pCulprit = pCulprit;
	fireSettings.vParentPos = vParentPos;
	fireSettings.weaponHash = weaponHash;
#if __BANK
	fireSettings.vRootPos = vRootPos;
#endif
	fireSettings.hasSpreadFromScriptedFire = hasSpreadFromScriptedFire;
	fireSettings.hasCulpritBeenCleared = hasCulpritBeenCleared;

	CFire* pFire = StartFire(fireSettings);
	if (pFire)
	{
#if __BANK
		if (!g_fireMan.m_ignoreStatusGrid)
#endif
		{
			s32 gridX, gridY;
			g_fireMan.GetMapStatusGridIndices(vFirePos, gridX, gridY);

			g_fireMan.SetMapStatusGrid(gridX, gridY, pFire->GetBurnTime()*5.0f, vFirePos.GetZf()+pFire->GetMaxRadius());
		}
	}

	return pFire;
} 


///////////////////////////////////////////////////////////////////////////////
//  StartPetrolFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::StartPetrolFire			(CEntity* pEntity,
														 Vec3V_In vPosWld, 
														 Vec3V_In vNormWld,
														 CPed* pCulprit,
														 bool isPool,
														 Vec3V_In vParentPos,
														 BANK_ONLY(Vec3V_In vRootPos,)
														 bool hasSpreadFromScriptedFire,
														 FireType_e parentFireType,
														 bool hasCulpritBeenCleared)
{
	// clones can't generate fires: the machine controlling the culprit is responsible for that
	if (pCulprit && pCulprit->IsNetworkClone())
	{
		return NULL;
	}

	// if a physical entity is passed we need to attach to it
	// only do this for petrol trails - pools shouldn't be able to be created on physical entities
	Vec3V vPosLcl = vPosWld;
	Vec3V vNormLcl = vNormWld;
	if (!isPool && pEntity && pEntity->GetIsPhysical())
	{
		if (!CVfxHelper::GetLocalEntityBonePosDir(*pEntity, -1, vPosWld, vNormWld, vPosLcl, vNormLcl))
		{
			return NULL;
		}
	}
	else
	{
		pEntity = NULL;
	}

	CFireSettings fireSettings;
	fireSettings.pEntity = pEntity;
	fireSettings.vPosLcl = vPosLcl;
	fireSettings.vNormLcl = vNormLcl;
	if (isPool)
	{
		fireSettings.fireType = FIRETYPE_TIMED_PETROL_POOL;
		fireSettings.peakTime = FIRE_PETROL_POOL_PEAK_TIME;
	}
	else
	{
		fireSettings.fireType = FIRETYPE_TIMED_PETROL_TRAIL;
		fireSettings.peakTime = FIRE_PETROL_TRAIL_PEAK_TIME;
	}
	fireSettings.parentFireType = parentFireType;
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnTime = FIRE_PETROL_BURN_TIME;
	fireSettings.burnStrength = FIRE_PETROL_BURN_STRENGTH;
	fireSettings.pCulprit = pCulprit;
	fireSettings.vParentPos = vParentPos;
#if __BANK
	fireSettings.vRootPos = vRootPos;
#endif
	fireSettings.hasSpreadFromScriptedFire = hasSpreadFromScriptedFire;
	fireSettings.hasCulpritBeenCleared = hasCulpritBeenCleared;

	return StartFire(fireSettings);
} 


///////////////////////////////////////////////////////////////////////////////
//  StartPedFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::StartPedFire				(CPed* pPed, 
														 CPed* pCulprit,
														 s32 numGenerations,
														 Vec3V_In vParentPos,
														 BANK_ONLY(Vec3V_In vRootPos,)
														 bool hasSpreadFromScriptedFire,
														 bool hasSpreadFromExplosion,
														 u32 weaponHash,
														 float currBurnTime,
														 bool startAtMaxStrength,
														 bool bReportCrimes,
														 bool hasCulpritBeenCleared)
{	
	// hard wire the burn time for the player ped
	float burnTime = FIRE_PED_BURN_TIME;
	if (pPed->IsPlayer())
	{
#if !__FINAL
		// invincible players don't get set on fire
		if (pPed->IsDebugInvincible())
		{
			return NULL;
		}
#endif 

		if (hasSpreadFromExplosion)
		{
			burnTime = FIRE_PLAYER_BURN_TIME_EXPLOSION;	
		}
		else
		{
			burnTime = FIRE_PLAYER_BURN_TIME;	
		}
	}

	// return if this ped is already on fire, cannot be damaged by flames or is not in control
	if (IsEntityOnFire(pPed) || pPed->m_nPhysicalFlags.bNotDamagedByFlames 
		|| (pPed->m_nPhysicalFlags.bNotDamagedByAnything BANK_ONLY(&& !CPedVariationDebug::ms_bLetInvunerablePedsBeAffectedByFire)) 
		|| pPed->IsNetworkClone())
	{
		return NULL;
	}

	//@@: range CFIREMANAGER_STARTPEDFIRE {
	#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_INVINCIBLECOP
		if(TamperActions::IsInvincibleCop(pPed))
		{
			return NULL;
		}
	#endif
	//@@: } CFIREMANAGER_STARTPEDFIRE

	// try to create the fire
	CFireSettings fireSettings;
	fireSettings.pEntity = pPed;
	fireSettings.fireType = FIRETYPE_TIMED_PED;
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnTime = burnTime;
	fireSettings.numGenerations = numGenerations;
	fireSettings.pCulprit = pCulprit;
	fireSettings.vParentPos = vParentPos;
	fireSettings.weaponHash = weaponHash;
#if __BANK
	fireSettings.vRootPos = vRootPos;
#endif
	fireSettings.hasSpreadFromScriptedFire = hasSpreadFromScriptedFire;
	fireSettings.hasCulpritBeenCleared = hasCulpritBeenCleared;

	CFire* pFire = StartFire(fireSettings);

	if (pFire)
	{
		pFire->m_currBurnTime = currBurnTime;

		if (startAtMaxStrength)
		{
			pFire->m_maxStrengthTime = 0.0f;
		}

		// report the crime
		if (pCulprit && bReportCrimes)
		{
			if (pPed->IsLawEnforcementPed())
			{
				CCrime::ReportCrime(CRIME_COP_SET_ON_FIRE, pPed, pCulprit);
			}
			else
			{
				CCrime::ReportCrime(CRIME_PED_SET_ON_FIRE, pPed, pCulprit);
			}
		}
	}

	return pFire;
}


///////////////////////////////////////////////////////////////////////////////
//  StartVehicleWreckedFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::StartVehicleWreckedFire	(CVehicle* pVehicle, 
														 Vec3V_In vPosLcl,
														 s32 wreckedFireId,
														 CPed* pCulprit,
														 float burnTime,
														 float maxRadius,
														 s32 numGenerations)
{	
	CFireSettings fireSettings;
	fireSettings.pEntity = pVehicle;
	fireSettings.vPosLcl = vPosLcl;
	fireSettings.fireType = (FireType_e)(FIRETYPE_TIMED_VEH_WRECKED+wreckedFireId);
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnTime = burnTime;
	fireSettings.numGenerations = numGenerations;
	fireSettings.pCulprit = pCulprit;

	CFire* pFire = StartFire(fireSettings);
	if (pFire)
	{
		// wrecked fires need to come in at full strength
		pFire->m_maxStrengthTime = 0.0f;

		// set the max radius
		pFire->m_maxRadius = maxRadius;

		// report the crime
		if (pCulprit)
		{
			// ignore the crime if the culprit set fire to their own car
			if (!pVehicle->ContainsPed(pCulprit))
			{
				CCrime::ReportCrime(CRIME_CAR_SET_ON_FIRE, pVehicle, pCulprit);
			}
		}
		// Register the audio
		if(!pFire->IsAudioRegistered())
		{
			pVehicle->GetVehicleAudioEntity()->GetVehicleFireAudio().RegisterVehWreckedFire(pFire);
			pFire->SetAudioRegistered(true); 
		}
	}

	return pFire;
}


///////////////////////////////////////////////////////////////////////////////
//  StartVehicleFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::StartVehicleFire			(CVehicle* pVehicle, 
														 s32 boneIndex,
														 Vec3V_In vPosLcl,
														 Vec3V_In vNormLcl,
														 CPed* pCulprit,
														 float burnTime,
														 float burnStrength,
														 float peakTime,
														 s32 numGenerations,
														 Vec3V_In vParentPos,
														 BANK_ONLY(Vec3V_In vRootPos,)
														 bool hasSpreadFromScriptedFire,
														 u32 weaponHash)
{	
	// return if this vehicle is cannot be damaged by flames
	// note that we are now allowing vehicle fires even if the vehicle cannot be damaged by flames
	//      this allows the fire particles to be rendered but the vehicle doesn't take any damage
	//		see bug #547495 for details
//	if (pVehicle->m_nPhysicalFlags.bNotDamagedByFlames || pVehicle->m_nPhysicalFlags.bNotDamagedByAnything || pVehicle->IsNetworkClone())
	if (pVehicle->IsNetworkClone())
	{
		return NULL;
	}

	if (pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType()==VEHICLE_TYPE_BICYCLE)
	{
		return NULL;
	}

	CFireSettings fireSettings;
	fireSettings.pEntity = pVehicle;
	fireSettings.boneIndex = boneIndex;
	fireSettings.vPosLcl = vPosLcl;
	fireSettings.vNormLcl = vNormLcl;
	fireSettings.fireType = FIRETYPE_TIMED_VEH_GENERIC;
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnTime = burnTime;
	fireSettings.burnStrength = burnStrength;
	fireSettings.peakTime = peakTime;
	fireSettings.numGenerations = numGenerations;
	fireSettings.pCulprit = pCulprit;
	fireSettings.vParentPos = vParentPos;
	fireSettings.weaponHash = weaponHash;
#if __BANK
	fireSettings.vRootPos = vRootPos;
#endif
	fireSettings.hasSpreadFromScriptedFire = hasSpreadFromScriptedFire;

	CFire* pFire = StartFire(fireSettings);

	if (pFire)
	{
		//Register the audio
		if(!pFire->IsAudioRegistered())
		{	
			pVehicle->GetVehicleAudioEntity()->GetVehicleFireAudio().RegisterVehGenericFire(pFire);
			pFire->SetAudioRegistered(true); 
		}
		// report the crime
		if(pCulprit)
		{
			// ignore the crime if the culprit set fire to their own car
			if (!pVehicle->ContainsPed(pCulprit))
			{
				CCrime::ReportCrime(CRIME_CAR_SET_ON_FIRE, pVehicle, pCulprit);
			}
		}
	}

	return pFire;
}


///////////////////////////////////////////////////////////////////////////////
//  StartObjectFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::StartObjectFire			(CObject* pObject, 
														 s32 boneIndex,
														 Vec3V_In vPosLcl,
														 Vec3V_In vNormLcl,
														 CPed* pCulprit,
														 float burnTime,
														 float burnStrength,
														 float peakTime,
														 s32 numGenerations,
														 Vec3V_In vParentPos,
														 BANK_ONLY(Vec3V_In vRootPos,)
														 bool hasSpreadFromScriptedFire,
														 u32 weaponHash,
														 bool hasCulpritBeenCleared)
{
	// we can't start fires locally on clones
	if (pObject->IsNetworkClone())
	{
		return NULL;
	}

	// don't set weapon objects on fire
	if (pObject->GetWeapon())
	{
		return NULL;
	}

	// don't set objects attached to weapons on fire
	fwEntity* pAttachedEntity = pObject->GetAttachParent();
	if (pAttachedEntity && pAttachedEntity->GetType()==ENTITY_TYPE_OBJECT)
	{
		CObject* pAttachedObject = static_cast<CObject*>(pAttachedEntity);
		if (pAttachedObject && pAttachedObject->GetWeapon())
		{
			return NULL;
		}
	}

	// don't set pickup objects on fire
	if (pObject->m_nObjectFlags.bIsPickUp)
	{
		return NULL;
	}

	// check if this object's fire status is being monitored
	fireObjectStatusData* pObjectFireStatus = g_fireMan.GetObjectStatus(pObject);
	if (pObjectFireStatus)
	{
		// it is - return if this object isn't allowed to create any fires at the moment
		if (pObjectFireStatus->m_canBurnTimer==0.0f)
		{
			return NULL;
		}
	}
	else
	{
		// it isn't - register the object fire and return if we can't register it
		if (g_fireMan.SetObjectStatus(pObject, burnTime*4.0f, burnTime*5.0f)==false)
		{
			return NULL;
		}
	}

	CFireSettings fireSettings;
	fireSettings.pEntity = pObject;
	fireSettings.boneIndex = boneIndex;
	fireSettings.vPosLcl = vPosLcl;
	fireSettings.vNormLcl = vNormLcl;
	fireSettings.fireType = FIRETYPE_TIMED_OBJ_GENERIC;
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnTime = burnTime;
	fireSettings.burnStrength = burnStrength;
	fireSettings.peakTime = peakTime;
	fireSettings.numGenerations = numGenerations;
	fireSettings.pCulprit = pCulprit;
	fireSettings.vParentPos = vParentPos;
	fireSettings.weaponHash = weaponHash;
#if __BANK
	fireSettings.vRootPos = vRootPos;
#endif
	fireSettings.hasSpreadFromScriptedFire = hasSpreadFromScriptedFire;
	fireSettings.hasCulpritBeenCleared = hasCulpritBeenCleared;

	CFire* pFire = StartFire(fireSettings);

	return pFire;
}


///////////////////////////////////////////////////////////////////////////////
//  StartFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::StartFire					(const CFireSettings& fireSettings)
{	
	// check we're being called from the main update thread
	vfxAssertf((sysThreadType::GetCurrentThreadType()&sysThreadType::THREAD_TYPE_UPDATE), "a fire is being added by a thread other than the update thread");

	// check that the number of generations passed in is valid
	vfxAssertf(fireSettings.numGenerations>=0, "invalid number of generations passed in");

	if (IsZeroAll(fireSettings.vParentPos)==false)
	{
		Vec3V vStartPos = fireSettings.vParentPos;
		Vec3V vEndPos = fireSettings.vPosLcl;

		if (fireSettings.pEntity)
		{
			Mat34V mtxEntityBone;
			if (fireSettings.boneIndex>0)
			{
				CVfxHelper::GetMatrixFromBoneIndex(mtxEntityBone, fireSettings.pEntity, fireSettings.boneIndex);
			}
			else
			{
				mtxEntityBone = fireSettings.pEntity->GetMatrix();
			}

			vEndPos = Transform(mtxEntityBone, fireSettings.vPosLcl);
		}

		static float PROBE_OFFSET_LENGTH = -0.1f;
		Vec3V vDir = vEndPos - vStartPos;
		vDir = NormalizeSafe(vDir, Vec3V(V_X_AXIS_WZERO));
		vStartPos += vDir*ScalarVFromF32(PROBE_OFFSET_LENGTH);

		static ScalarV vOffsetZ = ScalarV(V_HALF);
		vStartPos.SetZ(vStartPos.GetZ()+vOffsetZ);
		vEndPos.SetZ(vEndPos.GetZ()+vOffsetZ);

		WorldProbe::CShapeTestHitPoint probeResult;
		WorldProbe::CShapeTestResults probeResults(probeResult);
		WorldProbe::CShapeTestProbeDesc probeDesc;
		probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
		probeDesc.SetResultsStructure(&probeResults);
		probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER | ArchetypeFlags::GTA_OBJECT_TYPE | ArchetypeFlags::GTA_GLASS_TYPE);
		probeDesc.SetExcludeEntity(fireSettings.pEntity);
		probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
		if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
		{
#if __BANK
			if (m_renderStartFireProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 1.0f, 1.0f, 1.0f), Color32(1.0f, 0.0f, 0.0f, 1.0f), -30);
			}
#endif
			return NULL;
		}
#if __BANK
		else
		{
			if (m_renderStartFireProbes)
			{
				grcDebugDraw::Line(vStartPos, vEndPos, Color32(1.0f, 1.0f, 1.0f, 1.0f), Color32(0.0f, 1.0f, 0.0f, 1.0f), -30);
			}
		}
#endif
	}

	if(fireSettings.pCulprit && fireSettings.pEntity)
	{
		CPed* pBurningPed = (fireSettings.pEntity->GetIsTypePed()) ? static_cast<CPed*>(fireSettings.pEntity) : NULL;
		CPed* pCulprit = (fireSettings.pCulprit->GetIsTypePed()) ? static_cast<CPed*>(fireSettings.pCulprit) : NULL;

		if(pBurningPed && pCulprit && pBurningPed->IsNetworkPlayer() && !IsEntityOnFire(pBurningPed) && pCulprit->IsLocalPlayer())
		{
			CStatsMgr::RegisterPlayerSetOnFire();
		}
	}

	// don't start any entity fires set by team-mates when friendly fire is
	// disabled - this leads to a few exploits where you can get a friend to
	// set you on fire and then be immune from enemy fires
	if (NetworkInterface::IsGameInProgress())
	{
		// Now, we have non-networked peds in MP. We want them still to be able to catch fire.
		//// don't start fires on entities that are not synchronised if we are in a network game
        //if(fireSettings.pEntity && !NetworkUtils::GetNetworkObjectFromEntity(fireSettings.pEntity))
        //{
        //    return NULL;
        //}

		if(fireSettings.pCulprit)
		{
			CPed* pBurningPed = (fireSettings.pEntity && fireSettings.pEntity->GetIsTypePed()) ? static_cast<CPed*>(fireSettings.pEntity) : NULL;
			if(!NetworkInterface::IsFriendlyFireAllowed(pBurningPed,fireSettings.pCulprit))
				return NULL;
		}
	}

	// fires not attached to a moving entity should check they aren't underwater
	if (fireSettings.pEntity==NULL)
	{
		float waterDepth;
		CVfxHelper::GetWaterDepth(fireSettings.vPosLcl, waterDepth);
		if (waterDepth>0.0f)
		{
			return NULL;
		}
	}

	// get a free fire from the pool
	CFire* pFire = GetNextFreeFire(false);
	if (pFire)
	{
		// setup the fire
		bool fireActivated = pFire->Init(fireSettings);
		
#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord() && fireActivated)
		{
			s32 fireIndex = GetFireIndex(pFire);
			CReplayMgr::RecordPersistantFx<CPacketFireFx>(CPacketFireFx(fireSettings, fireIndex, pFire->GetPositionWorld()), CTrackedEventInfo<ptxFireRef>(ptxFireRef((size_t)pFire)), fireSettings.pEntity, true);
		}
#endif // GTA_REPLAY
	}

	return pFire;
}

s32 CFireManager::GetFireIndex(CFire* pFire)
{
	for (s32 i=0; i<MAX_FIRES; i++)
	{
		if (&m_fires[i] == pFire)
		{
			return i;
		}
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
//  RegisterVehicleTyreFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::RegisterVehicleTyreFire	(CVehicle* pVehicle, s32 boneIndex, Vec3V_In vPosLcl, void* pRegOwner, u32 regOffset, float currStrength, bool hasCulpritBeenCleared)
{
	CFireSettings fireSettings;
	fireSettings.pEntity = pVehicle;
	fireSettings.boneIndex = boneIndex;
	fireSettings.vPosLcl = vPosLcl;
	fireSettings.fireType = FIRETYPE_REGD_VEH_WHEEL;
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnStrength = currStrength;
	fireSettings.peakTime = 0.0f;
	fireSettings.regOwner = pRegOwner;
	fireSettings.regOffset = regOffset;
	fireSettings.numGenerations = FIRE_DEFAULT_NUM_GENERATIONS;
	fireSettings.hasCulpritBeenCleared = hasCulpritBeenCleared;

	if (pVehicle && !hasCulpritBeenCleared)
	{
		CEntity* pEntity = pVehicle->GetWeaponDamageEntity();
		if (pEntity)
		{
			if (pEntity->GetIsTypePed())
			{
				fireSettings.pCulprit = static_cast<CPed*>(pEntity);
			}
			else if (pEntity->GetIsTypeVehicle())
			{
				fireSettings.pCulprit = static_cast<CVehicle*>(pEntity)->GetDriver();
				if (!fireSettings.pCulprit && static_cast<CVehicle*>(pEntity)->GetSeatManager())
				{
					fireSettings.pCulprit = static_cast<CVehicle*>(pEntity)->GetSeatManager()->GetLastPedInSeat(0);
				}
			}
		}
	}

	CFire* pFire = RegisterFire(fireSettings);

	if (pFire)
	{
		// set the max radius
		CWheel* pWheel = static_cast<CWheel*>(pRegOwner);
		if (ptfxVerifyf(pWheel, "wheel is not valid"))
		{
			pFire->m_maxRadius = pWheel->GetWheelRadius();
		}
		//Register audio
		if(!pFire->IsAudioRegistered())
		{
			if(Verifyf(pFire->GetEntity(), "CFireManager - Trying to register a vehicle fire without a valid entity"))
			{
				if(Verifyf(pFire->GetEntity() == pVehicle, "CFireManager - Trying to register a fire in the wrong vehicle"))
				{
					pVehicle->GetVehicleAudioEntity()->GetVehicleFireAudio().RegisterVehTyreFire(pFire);
					pFire->SetAudioRegistered(true); 
				}
			}
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			int wheelIndex = 0;
			for(; wheelIndex < pVehicle->GetNumWheels(); ++wheelIndex)
			{
				if(pVehicle->GetWheel(wheelIndex) == pWheel)
					break;
			}

			CReplayMgr::RecordFx(CPacketRegisterVehicleFire(boneIndex, vPosLcl, (u8)wheelIndex, currStrength, CPacketRegisterVehicleFire::TYRE_FIRE), pVehicle);
		}
#endif // GTA_REPLAY
	}

	return pFire;
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterVehicleTankFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::RegisterVehicleTankFire	(CVehicle* pVehicle, s32 boneIndex, Vec3V_In vPosLcl, void* pRegOwner, u32 regOffset, float currStrength, CPed* pCulprit)
{
	CFireSettings fireSettings;
	fireSettings.pEntity = pVehicle;
	fireSettings.boneIndex = boneIndex;
	fireSettings.vPosLcl = vPosLcl;
	fireSettings.fireType = FIRETYPE_REGD_VEH_PETROL_TANK;
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnStrength = currStrength;
	fireSettings.peakTime = 0.0f;
	fireSettings.regOwner = pRegOwner;
	fireSettings.regOffset = regOffset;
	fireSettings.numGenerations = FIRE_DEFAULT_NUM_GENERATIONS;
	fireSettings.pCulprit = pCulprit;

	if (pVehicle && !pCulprit)
	{
		CEntity* entity = pVehicle->GetWeaponDamageEntity();
		if (entity)
		{
			if (entity->GetIsTypePed())
			{
				fireSettings.pCulprit = static_cast<CPed*>(entity);
			}
			else if (entity->GetIsTypeVehicle())
			{
				fireSettings.pCulprit = static_cast<CVehicle*>(entity)->GetDriver();
				if (!fireSettings.pCulprit && static_cast<CVehicle*>(entity)->GetSeatManager())
				{
					fireSettings.pCulprit = static_cast<CVehicle*>(entity)->GetSeatManager()->GetLastPedInSeat(0);
				}
			}
		}
	}

	CFire* pFire = RegisterFire(fireSettings);

	if (pFire)
	{
		// set the max radius
		CVfxVehicleInfo* pVfxVehicleInfo = g_vfxVehicleInfoMgr.GetInfo(pVehicle);
		if (pVfxVehicleInfo)
		{
			pFire->m_maxRadius = pVfxVehicleInfo->GetPetrolTankFirePtFxRadius();
		}

		//Register audio
		if(!pFire->IsAudioRegistered())
		{
			pVehicle->GetVehicleAudioEntity()->GetVehicleFireAudio().RegisterVehTankFire(pFire);
			pFire->SetAudioRegistered(true); 
		}

#if GTA_REPLAY
		if(CReplayMgr::ShouldRecord())
		{
			u8 fireType = 0;
			Assign(fireType, regOffset - PTFXOFFSET_VEHICLE_FIRE_PETROL_TANK); 
			CReplayMgr::RecordFx(CPacketRegisterVehicleFire(boneIndex, vPosLcl, 0, currStrength, fireType), pVehicle);
		}
#endif // GTA_REPLAY
	}

	return pFire;
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterPtFxFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::RegisterPtFxFire		(CEntity* pEntity, Vec3V_In vPosLcl, void* pRegOwner, u32 regOffset, float currStrength, bool isContained, bool isFlameThrower, CPed* pCulprit)
{
	CFireSettings fireSettings;
	fireSettings.pEntity = pEntity;
	fireSettings.vPosLcl = vPosLcl;
	fireSettings.fireType = FIRETYPE_REGD_FROM_PTFX;
	fireSettings.mtlId = PGTAMATERIALMGR->g_idDefault;
	fireSettings.burnStrength = currStrength;
	fireSettings.peakTime = 0.0f;
	fireSettings.regOwner = pRegOwner;
	fireSettings.regOffset = regOffset;
	fireSettings.numGenerations = FIRE_DEFAULT_NUM_GENERATIONS;
	fireSettings.isContained = isContained;
	fireSettings.isFlameThrower = isFlameThrower;
	fireSettings.pCulprit = pCulprit;

	return RegisterFire(fireSettings);
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::RegisterFire				(const CFireSettings& fireSettings)
{	
	// fires not attached to a moving entity should check they aren't underwater
	if (fireSettings.pEntity==NULL)
	{
		float waterDepth;
		CVfxHelper::GetWaterDepth(fireSettings.vPosLcl, waterDepth);
		if (waterDepth>0.0f)
		{
			return NULL;
		}
	}

	// try to find this fire 
	fwUniqueObjId regId = fwIdKeyGenerator::Get(fireSettings.regOwner, fireSettings.regOffset);
	CFire* pFire = FindRegisteredFire(regId);
	if (pFire && pFire->GetEntity()==fireSettings.pEntity)
	{
		// set as registered this frame
		pFire->SetFlag(FIRE_FLAG_REGISTERED_THIS_FRAME);

		// update position
		pFire->SetPositionLocal(fireSettings.vPosLcl);
	}
	else
	{
		// couldn't find it - get a free fire from the pool
		pFire = GetNextFreeFire(false);
		if (pFire)
		{
			pFire->Init(fireSettings);
		}
	}

	if (pFire)
	{
		pFire->m_currStrength = fireSettings.burnStrength;
		pFire->m_maxStrength = fireSettings.burnStrength;
	}

	return pFire;
}


///////////////////////////////////////////////////////////////////////////////
//  ClearCulpritFromAllFires
///////////////////////////////////////////////////////////////////////////////
void CFireManager::ClearCulpritFromAllFires(const CEntity* pCulprit)
{
	// cache the active fire array info as we'll alter it as we go through shutting down fires
	s32 numFiresActive = m_numFiresActive;
	s32	activeFireIndices[MAX_FIRES];
	sysMemCpy(&activeFireIndices[0], &m_activeFireIndices[0], numFiresActive*sizeof(s32));

	for (s32 i=0; i<numFiresActive; i++)
	{
		CFire* pFire = &m_fires[activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if(pFire->GetCulprit() == pCulprit)
		{
			pFire->SetFlag(FIRE_FLAG_CULPRIT_CLEARED);
			pFire->SetCulprit(NULL);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetScriptFlammabilityMultiplier
///////////////////////////////////////////////////////////////////////////////

void CFireManager::SetScriptFlammabilityMultiplier(float value, scrThreadId threadId)
{
	if (vfxVerifyf(m_scriptFlammabilityMultiplierScriptThreadId == THREAD_INVALID ||
		m_scriptFlammabilityMultiplierScriptThreadId == threadId,
		"Trying to set the script flammability multiplier when this is already in use by another script")) 
	{
		m_scriptFlammabilityMultiplier = value;
		m_scriptFlammabilityMultiplierScriptThreadId = threadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetScriptFlammabilityMultiplier
///////////////////////////////////////////////////////////////////////////////

float CFireManager::GetScriptFlammabilityMultiplier()
{
	return m_scriptFlammabilityMultiplier;
}


///////////////////////////////////////////////////////////////////////////////
//  ExtinguishAll
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ExtinguishAll				(bool finishPtFx)
{
	// cache the active fire array info as we'll alter it as we go through shutting down fires
	s32 numFiresActive = m_numFiresActive;
	s32	activeFireIndices[MAX_FIRES];
	sysMemCpy(&activeFireIndices[0], &m_activeFireIndices[0], numFiresActive*sizeof(s32));

	for (s32 i=0; i<numFiresActive; i++)
	{
		CFire* pFire = &m_fires[activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		pFire->Shutdown(finishPtFx);
	}

	m_EntitiesNextToFires.ResetCount();
}

///////////////////////////////////////////////////////////////////////////////
//  ExtinguishArea
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ExtinguishArea			(Vec3V_In vPosition, float radius, bool finishPtFx)
{
	// cache the active fire array info as we'll potentially alter it as we go through shutting down fires
	s32 numFiresActive = m_numFiresActive;
	s32	activeFireIndices[MAX_FIRES];
	sysMemCpy(&activeFireIndices[0], &m_activeFireIndices[0], numFiresActive*sizeof(s32));

	for (s32 i=0; i<numFiresActive; i++)
	{
		CFire* pFire = &m_fires[activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (DistSquared(vPosition, pFire->GetPositionWorld()).Getf() < radius*radius)
		{
			pFire->Shutdown(finishPtFx);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ExtinguishAreaSlowly
///////////////////////////////////////////////////////////////////////////////

bool			CFireManager::ExtinguishAreaSlowly		(Vec3V_In vPosition, float radius)
{
	bool foundFireToExtinguish = false;

	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (DistSquared(vPosition, pFire->GetPositionWorld()).Getf() < radius*radius)
		{
			g_vfxFire.UpdatePtFxFireSteam(pFire);

			pFire->SetFlag(FIRE_FLAG_IS_BEING_EXTINGUISHED);
			foundFireToExtinguish = true;		
		}
	}

	return foundFireToExtinguish;
}


///////////////////////////////////////////////////////////////////////////////
//  ExtinguishAreaSlowly
///////////////////////////////////////////////////////////////////////////////

bool			CFireManager::ExtinguishAreaSlowly		(ptxDataVolume& dataVolume)
{
	bool foundFireToExtinguish = false;

	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (dataVolume.IsPointInside(pFire->GetPositionWorld()))
		{
			g_vfxFire.UpdatePtFxFireSteam(pFire);

			pFire->SetFlag(FIRE_FLAG_IS_BEING_EXTINGUISHED);
			foundFireToExtinguish = true;	
		}
	}

	return foundFireToExtinguish;
}


///////////////////////////////////////////////////////////////////////////////
//  ExtinguishEntityFires
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ExtinguishEntityFires		(CEntity* pEntity, bool finishPtFx)
{
	// cache the active fire array info as we'll potentially alter it as we go through shutting down fires
	s32 numFiresActive = m_numFiresActive;
	s32	activeFireIndices[MAX_FIRES];
	sysMemCpy(&activeFireIndices[0], &m_activeFireIndices[0], numFiresActive*sizeof(s32));

	for (s32 i=0; i<numFiresActive; i++)
	{
		CFire* pFire = &m_fires[activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (pFire->m_regdEntity==pEntity)
		{	
			pFire->Shutdown(finishPtFx);
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetFire
///////////////////////////////////////////////////////////////////////////////

CFire&			CFireManager::GetFire					(s32 index)	
{
	vfxAssertf(index>=0, "called with index less than zero"); 
	vfxAssertf(index<MAX_FIRES, "called with out of range index"); 
	return m_fires[index];
}

CFire*			CFireManager::GetActiveFire				(s32 activeFireIndex)	
{
	vfxAssertf(activeFireIndex>=0, "called with index less than zero"); 
	vfxAssertf(activeFireIndex<m_numFiresActive, "called with out of range index"); 
	vfxAssertf(m_fires[m_activeFireIndices[activeFireIndex]].GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");
	return &m_fires[m_activeFireIndices[activeFireIndex]];
}

CFire*			CFireManager::GetFire					(const CNetFXIdentifier& netIdentifier)
{
	for (s32 i=0; i<MAX_FIRES; i++)
	{
		if (m_fires[i].GetFlag(FIRE_FLAG_IS_DORMANT))
		{
			if (m_fires[i].m_networkIdentifier == netIdentifier)
			{
				return &m_fires[i];
			}
		}
	}	

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetMapStatusGridIndices
///////////////////////////////////////////////////////////////////////////////

void 			CFireManager::GetMapStatusGridIndices		(Vec3V_In vQueryPos, s32& gridX, s32& gridY)
{
	gridX = static_cast<s32>(vQueryPos.GetXf())/FIRE_STATUS_GRID_CELL_SIZE & (FIRE_MAP_GRID_SIZE-1); 
	gridY = static_cast<s32>(vQueryPos.GetYf())/FIRE_STATUS_GRID_CELL_SIZE & (FIRE_MAP_GRID_SIZE-1);
}


///////////////////////////////////////////////////////////////////////////////
//  GetObjectStatus
///////////////////////////////////////////////////////////////////////////////

fireObjectStatusData*	CFireManager::GetObjectStatus	(CObject* pObject)
{
	for (int i=0; i<MAX_OBJECTS_ON_FIRE; i++)
	{
		if (m_objectStatus[i].m_pObject==pObject)
		{
			return &m_objectStatus[i];
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  SetObjectStatus
///////////////////////////////////////////////////////////////////////////////

bool			CFireManager::SetObjectStatus			(CObject* pObject, float canBurnTime, float dontBurnTime)
{
	for (int i=0; i<MAX_OBJECTS_ON_FIRE; i++)
	{
		if (m_objectStatus[i].m_pObject==NULL)
		{
			m_objectStatusActive = true;

			m_objectStatus[i].m_pObject = pObject;
			m_objectStatus[i].m_canBurnTimer = canBurnTime;
			m_objectStatus[i].m_dontBurnTimer = dontBurnTime;

			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  PlentyFiresAvailable
///////////////////////////////////////////////////////////////////////////////

bool 			CFireManager::PlentyFiresAvailable		()
{
	s32 numFires = 0;
	for (s32 i=0; i<MAX_FIRES; i++)
	{
		if (!m_fires[i].GetFlag(FIRE_FLAG_IS_ACTIVE) && !m_fires[i].GetFlag(FIRE_FLAG_IS_DORMANT))
		{
			numFires++;
			if (numFires >= SAFETY_FIRES) 
			{
				return true;
			}
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetNumFiresInRange
///////////////////////////////////////////////////////////////////////////////

s32			CFireManager::GetNumFiresInRange		(Vec3V_In vPos, float range)
{
	int numFires = 0;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		Vec3V vFirePos = pFire->GetPositionWorld();
		Vec2V vVec = (vFirePos - vPos).GetXY();

		if (Mag(vVec).Getf() <= range)
		{
			numFires++;
		}
	}

	return numFires;
}


///////////////////////////////////////////////////////////////////////////////
//  GetNumFiresInArea
///////////////////////////////////////////////////////////////////////////////

s32			CFireManager::GetNumFiresInArea			(float minX, float minY, 
													 float minZ, float maxX, 
													 float maxY, float maxZ)
{
	int numFires = 0;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (!pFire->GetFlag(FIRE_FLAG_IS_SCRIPTED))
		{
			Vec3V vFirePos = pFire->GetPositionWorld();

			if (vFirePos.GetXf()>=minX && vFirePos.GetXf()<=maxX &&
				vFirePos.GetYf()>=minY && vFirePos.GetYf()<=maxY &&
				vFirePos.GetZf()>=minZ && vFirePos.GetZf()<=maxZ)
			{
				numFires++;
			}
		}
	}

	return numFires;
}


///////////////////////////////////////////////////////////////////////////////
//  GetNumNonScriptFires
///////////////////////////////////////////////////////////////////////////////

u32			CFireManager::GetNumNonScriptFires			()
{
	s32 numFires = 0;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (!pFire->GetFlag(FIRE_FLAG_IS_SCRIPTED))
		{
			numFires++;
		}
	}

	return numFires;
}


///////////////////////////////////////////////////////////////////////////////
//  GetNumWreckedFires
///////////////////////////////////////////////////////////////////////////////

u32			CFireManager::GetNumWreckedFires			()
{
	s32 numFires = 0;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED ||
			pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED_2 ||
			pFire->GetFireType()==FIRETYPE_TIMED_VEH_WRECKED_3)
		{
			numFires++;
		}
	}

	return numFires;
}


///////////////////////////////////////////////////////////////////////////////
//  FindNearestFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::FindNearestFire			(Vec3V_In vSearchPos, float& nearestDistSqr)
{
	CFire* pNearestFire = NULL;

	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		Vec3V vVec = pFire->GetPositionWorld()-vSearchPos;
		vVec.SetZ(ScalarV(V_ZERO));

		float distSqr = MagSquared(vVec).Getf();
		if (distSqr<nearestDistSqr)
		{
			nearestDistSqr = distSqr;
			pNearestFire = pFire;
		}
	}

	return pNearestFire;
}


///////////////////////////////////////////////////////////////////////////////
//  GetEntityFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::GetEntityFire				(CEntity* pEntity)
{
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");
		if (pFire->m_regdEntity==pEntity)
		{	
			return pFire;
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  GetClosestEntityFireDist
///////////////////////////////////////////////////////////////////////////////

float			CFireManager::GetClosestEntityFireDist	(CEntity* pEntity, Vec3V_In vTestPos)
{
	float closestDistSqr = FLT_MAX;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		if (pFire->m_regdEntity==pEntity)
		{	
			// calc distance to test pos
			Vec3V vVec = vTestPos - pFire->GetPositionWorld();
			float distSqr = MagSquared(vVec).Getf();
			if (distSqr<closestDistSqr)
			{
				closestDistSqr = distSqr;
			}
		}
	}

	return rage::Sqrtf(closestDistSqr);
}


///////////////////////////////////////////////////////////////////////////////
//  GetClosestFirePos
///////////////////////////////////////////////////////////////////////////////

bool			CFireManager::GetClosestFirePos			(Vec3V_InOut vClosestFirePos, Vec3V_In vTestPos)
{
	CFire* pClosestFire = NULL;
	float closestDistSqr = FLT_MAX;
	for (s32 i=0; i<m_numFiresActive; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		// calc distance to test pos
		Vec3V vVec = vTestPos - pFire->GetPositionWorld();
		float distSqr = MagSquared(vVec).Getf();
		if (distSqr<closestDistSqr)
		{
			closestDistSqr = distSqr;
			pClosestFire = pFire;
		}
	}

	if (pClosestFire)
	{
		vClosestFirePos = pClosestFire->GetPositionWorld();
		return true;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void CFireManager::RemoveScript(scrThreadId threadId)
{
	if (threadId == m_scriptFlammabilityMultiplierScriptThreadId)
	{
		m_scriptFlammabilityMultiplier = 1.0f;
		m_scriptFlammabilityMultiplierScriptThreadId = THREAD_INVALID;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  StartScriptFire
///////////////////////////////////////////////////////////////////////////////

s32 			CFireManager::StartScriptFire			(Vec3V_In vPos, 
														 CEntity* pEntity, 
														 float burnTime,
														 float burnStrength,
														 s32 numGenerations,
														 bool isPetrolFire)
{
	// if this is an entity fire then firstly shutdown any current fires on the entity
	if (pEntity)
	{
		if (pEntity->GetIsTypePed())
		{
			// extinguish any fires already on this ped 
			CPed* pBurningPed = static_cast<CPed*>(pEntity);
			ExtinguishEntityFires(pBurningPed);
		}
		else
		{
			vfxAssertf(0, "command only supports ped entity fires");
			return -1;
		}
	}
	
#if GTA_REPLAY
	SetReplayIsScriptedFire(true);
	SetReplayIsPetrolFire(isPetrolFire);
#endif
	// create the fire
	CFire* pFire = NULL;
	if (pEntity)
	{
		if (pEntity->GetIsTypePed())
		{
			CPed* pBurningPed = static_cast<CPed*>(pEntity);
			pFire = StartPedFire(pBurningPed, NULL, numGenerations, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false, false, 0);
		}
	}
	else
	{
		Vec3V vFireNormal(V_Z_AXIS_WZERO);
		pFire = StartMapFire(vPos, vFireNormal, 0, NULL, burnTime, burnStrength, 1.0f, 0.0f, 0.0f, numGenerations, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);
	}

#if GTA_REPLAY
	SetReplayIsScriptedFire(false);
	SetReplayIsPetrolFire(false);
#endif

	if (pFire==NULL)
	{	
		vfxWarningf("Cannot create script fire");
		return -1;
	}

	// set the scripted flag
	pFire->SetFlag(FIRE_FLAG_IS_SCRIPTED);

	if (isPetrolFire)
	{
		pFire->SetFlag(FIRE_FLAG_IS_SCRIPTED_PETROL_FIRE);
	}

	// return the fire index
	return ptrdiff_t_to_int(pFire - m_fires);
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScriptFire
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::RemoveScriptFire			(s32 fireIndex)
{
	vfxAssertf(fireIndex>=0, "called with index less than zero");
	vfxAssertf(fireIndex<MAX_FIRES, "called with out of range index");
	vfxAssertf(m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_SCRIPTED), "fire at index doesn't have scripted flag set");

	if (m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_SCRIPTED))
	{	
		m_fires[fireIndex].ClearFlag(FIRE_FLAG_IS_SCRIPTED);

		// the fire may not longer be active (may have been extinguished perhaps) so only shutdown if necessary
		if (m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_ACTIVE))
		{
			m_fires[fireIndex].Shutdown();
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ClearScriptFireFlag
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ClearScriptFireFlag		(s32 fireIndex)
{
	vfxAssertf(fireIndex>=0, "called with index less than zero");
	vfxAssertf(fireIndex<MAX_FIRES, "called with out of range index");
	vfxAssertf(m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_SCRIPTED), "fire at index doesn't have scripted flag set");

	if (m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_SCRIPTED))
	{	
		m_fires[fireIndex].ClearFlag(FIRE_FLAG_IS_SCRIPTED);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetScriptFireCoords
///////////////////////////////////////////////////////////////////////////////

 Vec3V_Out		CFireManager::GetScriptFireCoords		(s32 fireIndex)
 {
	 vfxAssertf(fireIndex>=0, "called with index less than zero");
	 vfxAssertf(fireIndex<MAX_FIRES, "called with out of range index");
	 vfxAssertf(m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_SCRIPTED), "fire at index doesn't have scripted flag set");

	return m_fires[fireIndex].GetPositionWorld();
}


///////////////////////////////////////////////////////////////////////////////
//  IsScriptFireExtinguished
///////////////////////////////////////////////////////////////////////////////

// bool			CFireManager::IsScriptFireExtinguished	(s32 fireIndex)
// {
// 	vfxAssertf(fireIndex>=0, "called with index less than zero");
// 	vfxAssertf(fireIndex<MAX_FIRES, "called with out of range index");
// 	vfxAssertf(m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_SCRIPTED), "fire at index doesn't have scripted flag set");
// 
// 	return !m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_ACTIVE);
// }


///////////////////////////////////////////////////////////////////////////////
//  ClearReplayFireFlag
///////////////////////////////////////////////////////////////////////////////
#if GTA_REPLAY
void			CFireManager::ClearReplayFireFlag		(s32 fireIndex)
{
	vfxAssertf(fireIndex>=0, "called with index less than zero");
	vfxAssertf(fireIndex<MAX_FIRES, "called with out of range index");
	if(m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_REPLAY) == false)
	{
		return;
	}

	m_fires[fireIndex].ClearFlag(FIRE_FLAG_IS_REPLAY);
	if (m_fires[fireIndex].GetFlag(FIRE_FLAG_IS_SCRIPTED))
	{	
		m_fires[fireIndex].ClearFlag(FIRE_FLAG_IS_SCRIPTED);
	}
}
#endif // GTA_REPLAY

///////////////////////////////////////////////////////////////////////////////
//  GetNextFreeFire
///////////////////////////////////////////////////////////////////////////////

CFire* 			CFireManager::GetNextFreeFire			(bool isScriptRequest)
{
	for (s32 i=0; i<MAX_FIRES; i++)
	{
		if (!m_fires[i].GetFlag(FIRE_FLAG_IS_ACTIVE) && !m_fires[i].GetFlag(FIRE_FLAG_IS_SCRIPTED) && !m_fires[i].GetFlag(FIRE_FLAG_IS_DORMANT))
		{
			return &m_fires[i];
		}
	}

	if (isScriptRequest)
	{	
		for (s32 i=0; i<MAX_FIRES; i++)
		{
			if (!m_fires[i].GetFlag(FIRE_FLAG_IS_SCRIPTED))
			{
				// only search for existing fires that aren't attached to an entity
				if (m_fires[i].GetEntity()==NULL)
				{
					m_fires[i].Shutdown();
					return &m_fires[i];
				}
			}
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  FindRegisteredFire
///////////////////////////////////////////////////////////////////////////////

CFire*			CFireManager::FindRegisteredFire		(fwUniqueObjId regdId)
{
	for (s32 i=0; i<MAX_FIRES; i++)
	{
		if (m_fires[i].GetFlag(FIRE_FLAG_IS_REGISTERED) && m_fires[i].GetRegId()==regdId)
		{
			return &m_fires[i];
		}
	}

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
//  FireNextToEntityCB
///////////////////////////////////////////////////////////////////////////////

bool			CFireManager::FireNextToEntityCB		(fwEntity* entity, void* pData)
{
	// cast the input data 
	CEntity* pEntity = (CEntity*) entity;
	CFire* pFire = static_cast<CFire*>(pData);

	AddEntityNextToFireResult(pEntity, pFire);
	return true;
}

void			CFireManager::AddEntityNextToFireResult	(CEntity* pEntity, CFire* pFire)
{ 
	EntityNextToFire entityNextToFire(pEntity, pFire);
	m_EntitiesNextToFires.PushAndGrow(entityNextToFire);
}


void			CFireManager::ProcessAllEntitiesNextToFires()
{
	const int numEntitiesNextToFires = m_EntitiesNextToFires.GetCount();
	for(int i = 0; i < numEntitiesNextToFires; i++ )
	{
		CEntity* pEntity = m_EntitiesNextToFires[i].m_pEntity.Get();
		CFire* pFire = m_EntitiesNextToFires[i].m_pFire;
		
		// Make sure entity is still valid, then check the type and 
		// call appropriate function based on type.
		if(pEntity)
		{
			//Make sure we use a valid damage entity.
			if (CNetwork::IsGameInProgress())
			{
				if (pFire && pEntity->GetIsPhysical())
				{
					CPed* pCulprit = pFire->GetCulprit( );
					if (!pCulprit && !pFire->GetFlag(FIRE_FLAG_CULPRIT_CLEARED))
					{
						CPhysical* pPhysical = static_cast< CPed* >( pEntity );
						if(pPhysical->GetNetworkObject() && pPhysical->GetWeaponDamageEntity())
						{
							CEntity* damager = pPhysical->GetWeaponDamageEntity();
							if (damager->GetIsTypePed())
							{
								pCulprit = static_cast<CPed*>(pPhysical->GetWeaponDamageEntity());
							}
							else if (damager->GetIsTypeVehicle())
							{
								CVehicle* vehicle = static_cast<CVehicle*>(pPhysical->GetWeaponDamageEntity());
								pCulprit = vehicle->GetDriver();

								if (!pCulprit)
									pCulprit = vehicle->GetSeatManager()->GetLastPedInSeat(0);
							}

							if (pCulprit && pCulprit->GetNetworkObject())
							{
								gnetDebug1("Found Fire '%d', will damage '%s' with no culprit - reset culprit to '%s'", pFire->GetFireType(), pPhysical->GetNetworkObject()->GetLogName(), pCulprit->GetNetworkObject()->GetLogName());
								pFire->SetCulprit( pCulprit );
							}
						}
					}
				}
			}

			if (pEntity->GetIsTypePed())
			{
				g_fireMan.ProcessPedNextToFire(static_cast<CPed*>(pEntity), pFire);
			}
			else if (pEntity->GetIsTypeVehicle())
			{
				g_fireMan.ProcessVehicleNextToFire(static_cast<CVehicle*>(pEntity), pFire);
			}
			else if (pEntity->GetIsTypeObject())
			{
				g_fireMan.ProcessObjectNextToFire(static_cast<CObject*>(pEntity), pFire);
			}
		}

		// Make sure to free the reference-count on the entity now that we're done with it.
		m_EntitiesNextToFires[i].m_pEntity = NULL;
	}

	m_EntitiesNextToFires.ResetCount();
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessPedNextToFire
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ProcessPedNextToFire		(CPed* pPed, CFire* pFire)
{    
	// contained fires should only cause certain non-player peds to catch alight
	if (pFire->GetFlag(FIRE_FLAG_IS_CONTAINED) && !pPed->IsPlayer())
	{
		// mission peds and non-ragdolling peds shouldn't catch alight
		if (pPed->PopTypeIsMission() || !pPed->GetUsingRagdoll())
		{
			return;
		}
	}

	if (pFire->GetCulprit() && NetworkInterface::IsDamageDisabledInMP(*pPed, *pFire->GetCulprit()))
		return;

	// check if the ped exists, return if we're not allowed to be set on fire
	if (pPed==NULL || pPed->m_nPhysicalFlags.bNotDamagedByFlames || (pPed->m_nPhysicalFlags.bNotDamagedByAnything 
		BANK_ONLY(&& !CPedVariationDebug::ms_bLetInvunerablePedsBeAffectedByFire)))
	{
		return;
	}

	//@@: range CFIREMANAGER_PROCESSPEDNEXTTOFIRE {
	#if TAMPERACTIONS_ENABLED && TAMPERACTIONS_INVINCIBLECOP
		if(TamperActions::IsInvincibleCop(pPed))
		{
			return;
		}
	#endif
	//@@: } CFIREMANAGER_PROCESSPEDNEXTTOFIRE

	// don't set clones on fire from particle effects as the effect may not be
	// active on remote machines
	if (pPed->IsNetworkClone())
	{
		return;
	}

	//Avoid fire reaction and damage if in a network game and ped is a passive player or the fire owner is passive
	if (NetworkInterface::IsGameInProgress())
	{
		CPed* pFireCulprit = pFire->GetCulprit();
		if (pFireCulprit)
		{
			if(!NetworkInterface::IsFriendlyFireAllowed(pPed,pFireCulprit))
			{
				weaponDebugf3("CFireManager::ProcessPedNextToFire--!IsFriendlyFireAllowed(pPed,pFireCulprit)-->return");
				return;
			}
		}
	}

	if(CanSetPedOnFire(pPed))
	{
		if (!g_fireMan.IsEntityOnFire(pPed) && ( !pPed->GetIsAttached() || pPed->GetAttachFlag(ATTACH_FLAG_AUTODETACH_ON_RAGDOLL) ) )
		{
			Vec3V vFirePos = pFire->GetPositionWorld();
			float fireRadius = pFire->CalcCurrRadius();

			// get the fire sphere data
			spdSphere fireSphereWld(vFirePos, ScalarVFromF32(fireRadius));

			// get the axis aligned bounding box of the ped (and shrink it down a little)
			spdAABB aabbPed;
			aabbPed = pPed->GetBoundBox(aabbPed);
			Vec3V vBBMin = aabbPed.GetMin();
			Vec3V vBBMax = aabbPed.GetMax();
			Vec3V vBBDiff = vBBMax - vBBMin;
			ScalarV vShrink = ScalarV(V_THIRD);
			Vec3V vBBDiffScaled = vBBDiff*vShrink;
			aabbPed.SetMin(Vec3V(vBBMin.GetX()+vBBDiffScaled.GetX(), vBBMin.GetY()+vBBDiffScaled.GetY(), vBBMin.GetZ()));
			aabbPed.SetMax(Vec3V(vBBMax.GetX()-vBBDiffScaled.GetX(), vBBMax.GetY()-vBBDiffScaled.GetY(), vBBMax.GetZ()));

#if __BANK
			if (m_renderPedNextToFireTests)
			{
				grcDebugDraw::Sphere(vFirePos, fireRadius, Color32(1.0f, 0.5f, 0.0f, 1.0f), false);
			}
#endif

			// check if the fire intersects the ped 
			if (aabbPed.IntersectsSphere(fireSphereWld))
			{
				// get the vector from fire to ped on the xy plane
				Vec3V vFireToPedVec = pPed->GetTransform().GetPosition()-vFirePos;
				vFireToPedVec.SetZ(ScalarV(V_ZERO));
	
				const CBaseCapsuleInfo* pCapsuleInfo = pPed->GetCapsuleInfo();
				const float pedCapsuleRadius = pCapsuleInfo->GetHalfWidth();

				// calc penetration depth 
				// if we aren't fire-resistant, do the cheap checks
				// we only care about penetration depth for fire resistant peds
				float fPenetrationDepth = 0.0f;
				if (!pPed->IsFireResistant())
				{
					fPenetrationDepth = pedCapsuleRadius;
				}
				else
				{
					const float fFireToPedMag = Mag(vFireToPedVec).Getf();
					const float fFirePlusCapsule = (fireRadius+pedCapsuleRadius);
					fPenetrationDepth = Max(0.001f, fFirePlusCapsule-fFireToPedMag);
				}

				if (pPed->ProcessFireResistance(fPenetrationDepth, (float) FIRE_ENTITY_TEST_INTERVAL))
				{
					// allow peds to catch fire even if the number of generations is 0
					s16 newNumGenerations = MAX(0, pFire->GetNumGenerations()-1);
					g_fireMan.StartPedFire(pPed, pFire->GetCulprit(), newNumGenerations, vFirePos, BANK_ONLY(vFirePos,) pFire->GetFlag(FIRE_FLAG_IS_SCRIPTED) || pFire->GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED), false, pFire->GetWeaponHash(), 0.0f, false, true, pFire->GetFlag(FIRE_FLAG_CULPRIT_CLEARED));
				}
#if __BANK
				if (m_renderPedNextToFireTests)
				{
					grcDebugDraw::BoxAxisAligned(aabbPed.GetMin(), aabbPed.GetMax(), Color32(1.0f, 0.0f, 0.0f, 1.0f), true);
				}
#endif
			}
#if __BANK
			else
			{
				if (m_renderPedNextToFireTests)
				{
					grcDebugDraw::BoxAxisAligned(aabbPed.GetMin(), aabbPed.GetMax(), Color32(0.0f, 1.0f, 0.0f, 1.0f), false);
				}
			}
#endif
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessVehicleNextToFire
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ProcessVehicleNextToFire	(CVehicle* pVehicle, CFire* pFire)
{
	if (pVehicle->m_nPhysicalFlags.bNotDamagedByFlames || pVehicle->m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (!NetworkInterface::IsFriendlyFireAllowed(pVehicle,pFire->GetCulprit()))
			return;

		if (pFire->GetCulprit() && NetworkInterface::IsDamageDisabledInMP(*pVehicle, *pFire->GetCulprit()))
			return;
	}

	Vec3V vFirePos = pFire->GetPositionWorld();
	float fireRadius = pFire->CalcCurrRadius();

	// get the oriented bounding box of the vehicle
	spdAABB aabbVehicleLcl;
	aabbVehicleLcl = pVehicle->GetLocalSpaceBoundBox(aabbVehicleLcl);

	// check if the water point intersects the vehicle 
	spdSphere sphereWld(vFirePos, ScalarVFromF32(fireRadius));
	spdSphere sphereLcl = UnTransformOrthoSphere(pVehicle->GetMatrix(), sphereWld);

#if __BANK
	if (m_renderVehicleNextToFireTests)
	{
		grcDebugDraw::Sphere(vFirePos, fireRadius, Color32(1.0f, 1.0f, 0.0f, 1.0f), false);
	}
#endif

	if (aabbVehicleLcl.IntersectsSphere(sphereLcl))
	{
		bool isFlameThrowerFire = pFire->GetFlag(FIRE_FLAG_IS_FLAME_THROWER);

		// apply damage to the vehicle
		// Constant damage for flamethrower to deal with the flame getting bigger with time
		float damageMult = isFlameThrowerFire ? FLAMETHROWER_DAMAGE_MULTIPLIER : fireRadius;
		if (pFire->GetFireType()==FIRETYPE_TIMED_PETROL_TRAIL || pFire->GetFireType()==FIRETYPE_TIMED_PETROL_POOL)
		{	
			damageMult *= FIRE_DAMAGE_MULT_PETROL;
		}
		else
		{
			damageMult *= FIRE_DAMAGE_MULT_DEFAULT;
		}

		bool avoidExplosions = false;
		
		if(CVehicleDamage::AvoidVehicleExplosionChainReactions())
		{
			// If we're trying to avoid chain reaction vehicle explosions don't let vehicle fire damage
			//   create explosions on other vehicles
			const CEntity* pFireEntity = pFire->GetEntity();
			if(pFireEntity && pFireEntity->GetIsTypeVehicle() && pFireEntity != pVehicle)
			{
				avoidExplosions = true;

				if( CPhysics::ms_bInArenaMode )
				{
					damageMult = 0.0f;
				}
			}
		}

		// Don't allow flamethrower damage to a vehicle if the culprit is an occupant
		if (isFlameThrowerFire && pFire->GetCulprit() && pVehicle->ContainsPed(pFire->GetCulprit()))
		{
			damageMult = 0.0f;
		}

		pVehicle->GetVehicleDamage()->ApplyDamage(pFire->GetCulprit(), DAMAGE_TYPE_FIRE, WEAPONTYPE_FIRE, fwTimer::GetTimeStep()*VEH_DAMAGE_HEALTH_STD*damageMult, RCC_VECTOR3(vFirePos), VEC3_ZERO, VEC3_ZERO, 0,0,-1,false,true,0.0f,avoidExplosions, false, false, isFlameThrowerFire );

		// check if the vehicle is leaking petrol
		if (pVehicle && pVehicle->IsLeakingPetrol())
		{
			pVehicle->GetVehicleDamage()->SetPetrolTankOnFire(pFire->GetCulprit());
		}

		// check if the vehicle's wheels should be set on fire
		bool isPetrolTankFireOnSameVehicle = pFire->GetFireType()==FIRETYPE_REGD_VEH_PETROL_TANK && pFire->GetEntity()==pVehicle;
		if (isPetrolTankFireOnSameVehicle==false && pVehicle->GetHealth() <= 0.0f)
		{
			for (s32 i=0; i<pVehicle->GetNumWheels(); i++)
			{
				// get this wheel
				CWheel* pWheel = pVehicle->GetWheel(i);
				if (pWheel)
				{
					// get the wheel position and radius
					Vec3V vWheelPos;
					float wheelRadius = pWheel->GetWheelPosAndRadius(RC_VECTOR3(vWheelPos));

					// check if the fire and the wheel collide
					Vec3V vFireToWheelVec = vWheelPos-vFirePos;
					float testRadius = wheelRadius+fireRadius;
					if (MagSquared(vFireToWheelVec).Getf()<=testRadius*testRadius)
					{
						// do a tighter test - that the wheel centre point is within the fire radius when z is ignored
						vFireToWheelVec.SetZf(0.0f);
						if (MagSquared(vFireToWheelVec).Getf()<=fireRadius*fireRadius)
						{
							// wheel on fire
							pWheel->SetWheelOnFire();
						}
					}
				}
			}
		}

#if __BANK
		if (m_renderVehicleNextToFireTests)
		{
			grcDebugDraw::BoxOriented(aabbVehicleLcl.GetMin(), aabbVehicleLcl.GetMax(), pVehicle->GetMatrix(), Color32(0.0f, 1.0f, 0.0f, 1.0f), true);
		}
#endif
	}
#if __BANK
	else
	{
		if (m_renderVehicleNextToFireTests)
		{
			grcDebugDraw::BoxOriented(aabbVehicleLcl.GetMin(), aabbVehicleLcl.GetMax(), pVehicle->GetMatrix(), Color32(0.0f, 1.0f, 0.0f, 1.0f), false);
		}
	}
#endif
}


///////////////////////////////////////////////////////////////////////////////
//  ProcessObjectNextToFire
///////////////////////////////////////////////////////////////////////////////

void			CFireManager::ProcessObjectNextToFire	(CObject* pObject, CFire* pFire)
{
	// don't do anything if fireproof or invincible
	if (pObject->m_nPhysicalFlags.bNotDamagedByFlames || pObject->m_nPhysicalFlags.bNotDamagedByAnything)
	{
		return;
	}

	// contained fires shouldn't cause object to catch alight
	if (pFire->GetFlag(FIRE_FLAG_IS_CONTAINED))
	{
		return;
	}

	// don't process clones
	if (NetworkUtils::IsNetworkCloneOrMigrating(pObject))
	{
		return;
	}

	if (pFire->GetCulprit() && NetworkInterface::IsDamageDisabledInMP(*pObject, *pFire->GetCulprit()))
		return;

	Vec3V vFirePos = pFire->GetPositionWorld();
	float fireRadius = pFire->CalcCurrRadius();

	// Some pickup can be damaged by fire now, e.g, petrol can.
	bool bIsPickup = pObject->m_nObjectFlags.bIsPickUp;
	bool bPickupCanBeDamagedByFire = bIsPickup ? ((CPickup*)pObject)->CanBeDamagedByFire(pFire) : false; 

	// check if this is a weapon
	bool isWeapon = pObject->GetWeapon() != NULL;

	// check if this is attached to a weapon
	if (!isWeapon)
	{
		fwEntity* pAttachedEntity = pObject->GetAttachParent();
		if (pAttachedEntity && pAttachedEntity->GetType()==ENTITY_TYPE_OBJECT)
		{
			CObject* pAttachedObject = static_cast<CObject*>(pAttachedEntity);
			if (pAttachedObject && pAttachedObject->GetWeapon())
			{
				isWeapon = true;
			}
		}
	}

	// don't set weapon objects on fire
	if (isWeapon && !bPickupCanBeDamagedByFire)
	{
		return;
	}

	// don't set pickup objects on fire
	if (bIsPickup && !bPickupCanBeDamagedByFire)
	{
		return;
	}

	if (CExplosionManager::IsTimedExplosionInProgress(pObject))
	{
		return;
	}

	bool bRemoteFire = pFire->GetNetworkIdentifier().IsClone() || (pFire->GetEntity() && NetworkUtils::IsNetworkClone(pFire->GetEntity()));

	CEntity* pCulprit = pFire->GetCulprit();

	// If the Fire does not have a culprit like the PtFxFire the get the object damager
	if (!pCulprit && !pFire->GetFlag(FIRE_FLAG_CULPRIT_CLEARED))
		pCulprit = pObject->GetWeaponDamageEntity();

	if (!bRemoteFire && pCulprit)
	{
		netObject* pNetObj = NetworkUtils::GetNetworkObjectFromEntity(pCulprit);

		if (pNetObj && pNetObj->IsClone())
		{
			bRemoteFire = true;
		}
	}

	// remotely controlled fires, or fires on burning remote entities are not to explode non-local entities
	if (bRemoteFire)
	{
		if (!pObject->GetNetworkObject() || pObject->IsNetworkClone())
		{
			return;
		}
	}

	// apply damage to the object 
	if (pObject->GetHealth()>0.0f)
	{
		float damageMult = fireRadius;
//		if (pFire->GetFireType()==FIRETYPE_TIMED_PETROL_TRAIL || pFire->GetFireType()==FIRETYPE_TIMED_PETROL_POOL)
//		{	
//			damageMult *= FIRE_DAMAGE_MULT_PETROL;
//		}
//		else
		{
			damageMult *= FIRE_DAMAGE_MULT_DEFAULT;
		}

		Vec3V vDamageNormal(V_Z_AXIS_WZERO);
		pObject->ObjectDamage(fwTimer::GetTimeStep()*2000.0f*damageMult, &RCC_VECTOR3(vFirePos), &RC_VECTOR3(vDamageNormal), pFire->GetCulprit(), WEAPONTYPE_FIRE);

		if (pObject->GetHealth()<=0.0f)
		{
			// this object should blow up if it has any explosion properties
			pObject->TryToExplode(pCulprit);
		}
	}

	// try to start a fire on the object
#if __BANK
	if (g_fireMan.m_disableObjectSpreading==false)
#endif
	{
		if (g_fireMan.IsEntityOnFire(pObject)==false)
		{
			// probe from the fire to the centre of the object and create a fire at the collision position
			Vec3V vStartPos = vFirePos;
			Vec3V vEndPos = pObject->GetTransform().GetPosition();

			WorldProbe::CShapeTestHitPoint probeResult;
			WorldProbe::CShapeTestResults probeResults(probeResult);
			WorldProbe::CShapeTestProbeDesc probeDesc;
			probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
			probeDesc.SetResultsStructure(&probeResults);
			probeDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
			probeDesc.SetIncludeEntity(pObject);

			if (pObject && pObject->GetCurrentPhysicsInst() && WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
			{
				if (vfxVerifyf(probeResults[0].GetHitInst()==pObject->GetCurrentPhysicsInst(), "shape test against a specific object has returned a hit against a different object"))
				{
					float burnTime, burnStrength, peakTime;
					if (pFire->FindMtlBurnInfo(burnTime, burnStrength, peakTime, probeResults[0].GetHitMaterialId(), false))
					{
						s16 newNumGenerations = pFire->GetNumGenerations()-1;
						if (newNumGenerations>=0)
						{
							burnStrength *= pFire->CalcStrengthGenScalar();

							s32 boneIndex = -1;
							fragInst* pFragInst = pObject->GetFragInst();
							if (pFragInst)
							{
								fragPhysicsLOD* pPhysicsLod = pFragInst->GetTypePhysics();
								if (pPhysicsLod)
								{
									fragTypeChild* pFragTypeChild = pPhysicsLod->GetChild(probeResults[0].GetHitComponent());
									if (pFragTypeChild)
									{
										u16 boneId = pFragTypeChild->GetBoneID();

										CBaseModelInfo* pModelInfo = pObject->GetBaseModelInfo(); 
										if (pModelInfo)
										{
											gtaFragType* pFragType = pModelInfo->GetFragType();
											if (pFragType)
											{
												boneIndex = pFragType->GetBoneIndexFromID(boneId);
											}
										}
									}
								}
							}

							// check that the new fire location is close enough to the fire it's spreading from
							float distToFire = Dist(probeResults[0].GetHitPositionV(), vFirePos).Getf();
							if (distToFire<=pFire->CalcCurrRadius()+1.0f)
							{
								Vec3V vPosLcl;
								Vec3V vNormLcl;
								if (CVfxHelper::GetLocalEntityBonePosDir(*pObject, boneIndex, probeResults[0].GetHitPositionV(), probeResults[0].GetHitNormalV(), vPosLcl, vNormLcl))
								{
									g_fireMan.StartObjectFire(pObject, boneIndex, vPosLcl, vNormLcl, pFire->GetCulprit(), burnTime, burnStrength, peakTime, newNumGenerations, vFirePos, BANK_ONLY(vFirePos,) pFire->GetFlag(FIRE_FLAG_IS_SCRIPTED) || pFire->GetFlag(FIRE_FLAG_HAS_SPREAD_FROM_SCRIPTED), 0, pFire->GetFlag(FIRE_FLAG_CULPRIT_CLEARED));
								}
							}

						}
					}
				}
			}
		}
	}
}

//To avoid waiting on our searches, we must call those two function (start and finalize) with enough cpu interval inbetween
// in order to hide the search.
// Start the fire searches for spreading
void CFireManager::StartFireSearches()
{
	PF_AUTO_PUSH_TIMEBAR("StartFireSearches");

	if( fwTimer::IsGamePaused() )
	{
		// Don't find stuff to catch on fire while game is paused!
		return;
	}

	// All previous results from fire searches should have already been processed by now. If not, 
	// just clear out these entities, as we're going to kick off a new round of searches momentarily.
	m_EntitiesNextToFires.ResetCount();

	int cap = m_numFiresActive;
	//if (cap > 1)
	//	cap = 1;
	for (s32 i=0; i<cap; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		pFire->StartAsyncFireSearch();
	}
}
// End the fire searches for spreading
void CFireManager::FinalizeFireSearches()
{
	PF_AUTO_PUSH_TIMEBAR("FinalizeFireSearches");

	int cap = m_numFiresActive;
	//if (cap > 1)
	//	cap = 1;
	for (s32 i=0; i<cap; i++)
	{
		CFire* pFire = &m_fires[m_activeFireIndices[i]];
		vfxAssertf(pFire->GetFlag(FIRE_FLAG_IS_ACTIVE), "fire in active indices is now inactive");

		pFire->EndAsyncFireSearch();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  StartDebugMapFire
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::StartDebugMapFire			()
{
	// probe downwards looking for ground down from the camera

	Vec3V vStartPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, 10.0f);

	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		Vec3V vGroundPos = RCC_VEC3V(probeResults[0].GetHitPosition());
		g_fireMan.StartMapFire(vGroundPos, Vec3V(V_Z_AXIS_WZERO), probeResults[0].GetMaterialId(), NULL, g_fireMan.m_debugBurnTime, g_fireMan.m_debugBurnStrength, g_fireMan.m_debugPeakTime, g_fireMan.m_debugFuelLevel, g_fireMan.m_debugFuelBurnRate, g_fireMan.m_debugNumGenerations, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  StartDebugScriptFire
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::StartDebugScriptFire		()
{
	// probe downwards looking for ground down from the camera

	Vec3V vStartPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V vEndPos = vStartPos - Vec3V(0.0f, 0.0f, 10.0f);

	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vStartPos), RCC_VECTOR3(vEndPos));
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_MAP_TYPE_MOVER);
	probeDesc.SetExcludeEntity(NULL);
	probeDesc.SetContext(WorldProbe::LOS_GeneralAI);
	if(WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		Vec3V vGroundPos = RCC_VEC3V(probeResults[0].GetHitPosition());
		g_fireMan.StartScriptFire(vGroundPos, NULL, g_fireMan.m_debugBurnTime, g_fireMan.m_debugBurnStrength, g_fireMan.m_debugNumGenerations);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  StartDebugFocusPedFire
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::StartDebugFocusPedFire		()
{
	for (int i=0; i<CDebugScene::FOCUS_ENTITIES_MAX; i++)
	{
		CEntity* pFocusEntity = CDebugScene::FocusEntities_Get(i);
		if (pFocusEntity != NULL && pFocusEntity->GetIsTypePed())
		{
			g_fireMan.ExtinguishEntityFires(pFocusEntity);
			g_fireMan.StartPedFire(static_cast<CPed*>(pFocusEntity), NULL, g_fireMan.m_debugNumGenerations, Vec3V(V_ZERO), BANK_ONLY(Vec3V(V_ZERO),) false, false, 0);
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ExtinguishAreaAroundPlayer
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::ExtinguishAreaAroundPlayer	()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		g_fireMan.ExtinguishArea(pPed->GetTransform().GetPosition(), 50.0f);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  RemoveFromAreaAroundPlayer
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::RemoveFromAreaAroundPlayer	()
{
	CPed* pPed = CGameWorld::FindLocalPlayer();
	if (pPed)
	{
		g_fireMan.ExtinguishArea(pPed->GetTransform().GetPosition(), 50.0f, true);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ResetStatusGrid
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::ResetStatusGrid			()
{
	g_fireMan.m_mapStatusGridActive = false;
	for (s32 i=0; i<FIRE_MAP_GRID_SIZE; i++)
	{
		for (s32 j=0; j<FIRE_MAP_GRID_SIZE; j++)
		{	
			g_fireMan.m_mapStatusGrid[i][j].m_timer = 0.0f;
			g_fireMan.m_mapStatusGrid[i][j].m_height = -9999.0f;
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  OutputStatusGrid
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::OutputStatusGrid			()
{
	vfxDebugf1("FIRE STATUS GRID\n");
	vfxDebugf1("================\n");

	char txtFireStatusRow[FIRE_MAP_GRID_SIZE+1];
	for (s32 i=0; i<FIRE_MAP_GRID_SIZE; i++)
	{
		txtFireStatusRow[0] = '\0';
		for (s32 j=0; j<FIRE_MAP_GRID_SIZE; j++)
		{	
			if (g_fireMan.m_mapStatusGrid[i][j].m_timer>0.0f)
			{
				strcat(txtFireStatusRow, "#");
			}
			else
			{
				strcat(txtFireStatusRow, "-");
			}
		}

		vfxDebugf1("%s", txtFireStatusRow);
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  RenderStatusGrid
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::RenderStatusGrid			()
{
	float minActiveZ = 9999.0f;

	Vec3V_ConstRef vCamPos = RCC_VEC3V(camInterface::GetPos());
	Vec3V_ConstRef vCamForward = RCC_VEC3V(camInterface::GetFront());
	Vec3V vPosCentre = vCamPos + (ScalarVFromF32(FIRE_MAP_GRID_SIZE*FIRE_STATUS_GRID_CELL_SIZE/2.0f) * vCamForward);

	Vec3V vPosStart = vPosCentre;
	vPosStart.SetZ(vCamPos.GetZ());
	Vec3V vPosEnd = vPosStart;
	vPosEnd.SetZ(vCamPos.GetZ()-ScalarVFromF32(500.0f));

	u32 probeFlags = ArchetypeFlags::GTA_MAP_TYPE_WEAPON;
	WorldProbe::CShapeTestHitPoint probeResult;
	WorldProbe::CShapeTestResults probeResults(probeResult);
	WorldProbe::CShapeTestProbeDesc probeDesc;
	probeDesc.SetResultsStructure(&probeResults);
	probeDesc.SetIncludeFlags(probeFlags);
	probeDesc.SetStartAndEnd(RCC_VECTOR3(vPosStart), RCC_VECTOR3(vPosEnd));
	if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
	{
		Vec3V vHitPos = RCC_VEC3V(probeResults[0].GetHitPosition());

		float basePosX = (float)((int)(vHitPos.GetXf()/FIRE_STATUS_GRID_CELL_SIZE)*FIRE_STATUS_GRID_CELL_SIZE);
		float basePosY = (float)((int)(vHitPos.GetYf()/FIRE_STATUS_GRID_CELL_SIZE)*FIRE_STATUS_GRID_CELL_SIZE);
		basePosX -= FIRE_MAP_GRID_SIZE*FIRE_STATUS_GRID_CELL_SIZE/2;
		basePosY -= FIRE_MAP_GRID_SIZE*FIRE_STATUS_GRID_CELL_SIZE/2;

		Vec3V vPosBase;
		vPosBase.SetXf(basePosX);
		vPosBase.SetYf(basePosY);

		float baseZ;
		if (m_renderMapStatusGridMinZ<9999.0f)
		{
			baseZ = m_renderMapStatusGridMinZ;
		}
		else
		{
			baseZ = vHitPos.GetZf()+m_renderMapStatusGridHeight;
		}

		vPosBase.SetZf(baseZ);

		for (s32 i=0; i<FIRE_MAP_GRID_SIZE; i++)
		{
			for (s32 j=0; j<FIRE_MAP_GRID_SIZE; j++)
			{	
				Vec3V vVtx1 = vPosBase;
				Vec3V vVtx2 = vPosBase;
				Vec3V vVtx3 = vPosBase;
				Vec3V vVtx4 = vPosBase;
				Vec3V vVtxCentre = vPosBase;

				vVtx1.SetXf(vPosBase.GetXf()+(float)(i*FIRE_STATUS_GRID_CELL_SIZE));
				vVtx1.SetYf(vPosBase.GetYf()+(float)(j*FIRE_STATUS_GRID_CELL_SIZE));

				vVtx2.SetXf(vPosBase.GetXf()+(float)(i*FIRE_STATUS_GRID_CELL_SIZE) + FIRE_STATUS_GRID_CELL_SIZE);
				vVtx2.SetYf(vPosBase.GetYf()+(float)(j*FIRE_STATUS_GRID_CELL_SIZE));

				vVtx3.SetXf(vPosBase.GetXf()+(float)(i*FIRE_STATUS_GRID_CELL_SIZE) + FIRE_STATUS_GRID_CELL_SIZE);
				vVtx3.SetYf(vPosBase.GetYf()+(float)(j*FIRE_STATUS_GRID_CELL_SIZE) + FIRE_STATUS_GRID_CELL_SIZE);

				vVtx4.SetXf(vPosBase.GetXf()+(float)(i*FIRE_STATUS_GRID_CELL_SIZE));
				vVtx4.SetYf(vPosBase.GetYf()+(float)(j*FIRE_STATUS_GRID_CELL_SIZE) + FIRE_STATUS_GRID_CELL_SIZE);

				vVtxCentre.SetXf(vPosBase.GetXf()+(float)(i*FIRE_STATUS_GRID_CELL_SIZE) + FIRE_STATUS_GRID_CELL_SIZE/2.0f);
				vVtxCentre.SetYf(vPosBase.GetYf()+(float)(j*FIRE_STATUS_GRID_CELL_SIZE) + FIRE_STATUS_GRID_CELL_SIZE/2.0f);

				// calc the grid indices for this position
				s32 gridX, gridY;
				g_fireMan.GetMapStatusGridIndices(vVtxCentre, gridX, gridY);

				if (g_fireMan.m_mapStatusGrid[gridX][gridY].m_timer>0.0f)
				{
					vVtx1.SetZf(g_fireMan.m_mapStatusGrid[gridX][gridY].m_height);
					vVtx2.SetZf(g_fireMan.m_mapStatusGrid[gridX][gridY].m_height);
					vVtx3.SetZf(g_fireMan.m_mapStatusGrid[gridX][gridY].m_height);
					vVtx4.SetZf(g_fireMan.m_mapStatusGrid[gridX][gridY].m_height);

					Vec3V vVtx1Bottom = vVtx1;
					vVtx1Bottom.SetZf(baseZ);
					Vec3V vVtx2Bottom = vVtx2;
					vVtx2Bottom.SetZf(baseZ);
					Vec3V vVtx3Bottom = vVtx3;
					vVtx3Bottom.SetZf(baseZ);
					Vec3V vVtx4Bottom = vVtx4;
					vVtx4Bottom.SetZf(baseZ);

					if (g_fireMan.m_mapStatusGrid[gridX][gridY].m_height<minActiveZ)
					{
						minActiveZ = g_fireMan.m_mapStatusGrid[gridX][gridY].m_height;
					}

					grcDebugDraw::Quad(vVtx1, vVtx2, vVtx3, vVtx4, Color32(1.0f, 0.0f, 0.0f, m_renderMapStatusGridAlpha), false, true);
					grcDebugDraw::Line(vVtx1, vVtx1Bottom, Color32(1.0f, 0.0f, 0.0f, m_renderMapStatusGridAlpha));
					grcDebugDraw::Line(vVtx2, vVtx2Bottom, Color32(1.0f, 0.0f, 0.0f, m_renderMapStatusGridAlpha));
					grcDebugDraw::Line(vVtx3, vVtx3Bottom, Color32(1.0f, 0.0f, 0.0f, m_renderMapStatusGridAlpha));
					grcDebugDraw::Line(vVtx4, vVtx4Bottom, Color32(1.0f, 0.0f, 0.0f, m_renderMapStatusGridAlpha));
				}
// 				else
// 				{
// 					grcDebugDraw::Quad(vVtx1, vVtx2, vVtx3, vVtx4, Color32(0.0f, 1.0f, 0.0f, m_renderMapStatusGridAlpha), false, false);
// 				}
			}
		}
	}

	m_renderMapStatusGridMinZ = minActiveZ;
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ResetObjectStatus
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::ResetObjectStatus			()
{
	g_fireMan.m_objectStatusActive = false;
	for (s32 i=0; i<MAX_OBJECTS_ON_FIRE; i++)
	{
		g_fireMan.m_objectStatus[i].m_pObject = NULL;
		g_fireMan.m_objectStatus[i].m_canBurnTimer = 0.0f;
		g_fireMan.m_objectStatus[i].m_dontBurnTimer = 0.0f;
	}
}
#endif



///////////////////////////////////////////////////////////////////////////////
//  RenderObjectStatus
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::RenderObjectStatus			()
{
	for (s32 i=0; i<MAX_OBJECTS_ON_FIRE; i++)
	{
		if (g_fireMan.m_objectStatus[i].m_pObject)
		{
			Vec3V vObjectPos = g_fireMan.m_objectStatus[i].m_pObject->GetTransform().GetPosition();
			float objectCullRadius = g_fireMan.m_objectStatus[i].m_pObject->GetBoundRadius();

			float colR = 1.0f;
			float colG = 1.0f;
			float colB = 1.0f;
			float colA = 0.3f;

			if (g_fireMan.m_objectStatus[i].m_canBurnTimer>0.0f)
			{
				float ratio = 1.0f - Min(g_fireMan.m_objectStatus[i].m_canBurnTimer/25.0f, 1.0f);
				colR = ratio;
				colG = 1.0f;
				colB = ratio;
			}
			else if (g_fireMan.m_objectStatus[i].m_dontBurnTimer>0.0f)
			{
				float ratio = Min(g_fireMan.m_objectStatus[i].m_dontBurnTimer/25.0f, 1.0f);
				colR = ratio;
				colG = 0.0f;
				colB = 0.0f;
			}

			grcDebugDraw::Sphere(vObjectPos, objectCullRadius, Color32(colR, colG, colB, colA));
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  Reset
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void			CFireManager::Reset						()
{
	g_vfxFire.Shutdown(SHUTDOWN_WITH_MAP_LOADED);

	g_fireMan.Shutdown(SHUTDOWN_CORE);
	g_fireMan.Init(INIT_CORE);

	g_vfxFire.Init(INIT_AFTER_MAP_LOADED);
}
#endif






