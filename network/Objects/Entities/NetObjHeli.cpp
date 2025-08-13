//
// name:        NetObjHeli.cpp
// description: Derived from netObject, this class handles all helicopter-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjHeli.h"

// framework headers
#include "fwscene/stores/staticboundsstore.h"

// game headers
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "network/Debug/NetworkDebug.h"
#include "network/NetworkInterface.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "network/objects/prediction/NetBlenderHeli.h"
#include "network/players/NetworkPlayerMgr.h"
#include "peds/Ped.h"
#include "physics/gtaInst.h"
#include "renderer/PostScan.h"
#include "vehicles/Heli.h"
#include "vehicles/VehicleGadgets.h"
#include "vehicleAi/VehicleIntelligence.h"

NETWORK_OPTIMISATIONS()
NETWORK_OBJECT_OPTIMISATIONS()

#define NETOBJHELI_ANCHOR_TIME_DELAY 1000

CHeliBlenderData				s_heliBlenderData;
CHeliBlenderData*				CNetObjHeli::ms_heliBlenderData = &s_heliBlenderData;
CNetObjHeli::CHeliScopeData		CNetObjHeli::ms_heliScopeData;
CHeliSyncTree*					CNetObjHeli::ms_heliSyncTree;

CNetObjHeli::CNetObjHeli(CHeli						*heli,
                         const NetworkObjectType	type,
                         const ObjectId				objectID,
                         const PhysicalPlayerIndex  playerIndex,
                         const NetObjFlags			localFlags,
                         const NetObjFlags			globalFlags) :
CNetObjAutomobile(heli, type, objectID, playerIndex, localFlags, globalFlags)
, m_LastHeliThrottleReceived(0.0f)
, m_IsCustomHealthHeli(false)
{
}

void CNetObjHeli::CreateSyncTree()
{
    ms_heliSyncTree = rage_new CHeliSyncTree(VEHICLE_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjHeli::DestroySyncTree()
{
	ms_heliSyncTree->ShutdownTree();
    delete ms_heliSyncTree;
    ms_heliSyncTree = 0;
}

netINodeDataAccessor *CNetObjHeli::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IHeliNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IHeliNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjAutomobile::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

bool CNetObjHeli::Update()
{
    CHeli *heli = GetHeli();
	gnetAssert(heli);

    if (IsClone())
    {
        CNetBlenderVehicle *netBlenderVehicle = SafeCast(CNetBlenderVehicle, GetNetBlender());

        if(netBlenderVehicle->HasStoppedPredictingPosition())
        {
            // if the blender for this vehicle has stopped predicting position (due to not receiving a
            // position update for too long) stop the vehicle from driving off
            heli->SetThrottleControl(0.0f);
        }
        else
        {
            // ensure the vehicle is always using the last received value for the throttle
            heli->SetThrottleControl(m_LastHeliThrottleReceived);
        }
    }

    return CNetObjAutomobile::Update();
}

// name:        ProcessControl
// description: Called from CGameWorld::Process, called in the same place as the local entity process controls
bool CNetObjHeli::ProcessControl()
{
    CHeli* pHeli = GetHeli();
    gnetAssert(pHeli);

    CNetObjAutomobile::ProcessControl();

	CSeaPlaneExtension* pSeaPlaneExtension = pHeli->GetExtension<CSeaPlaneExtension>();

    if (IsClone())
    {
        //pHeli->UpdateRotorSpeed(); //UpdateRotorSpeed is called above through CNetObjAutomobile::ProcessControl -> CNetObjVehicle::ProcessControl -> CVehicle::ProcessControl -> CHeli::DoProcessControl -> CHeli::UpdateRotorSpeed
        pHeli->UpdateRopes();
		//pHeli->ProcessFiringGun(m_firingGun);

        if(pHeli->GetSearchLight())
        {
		    if(pHeli->GetSearchLightOn())
		    {
			    if(!pHeli->GetAlwaysPreRender())
			    {
				    gPostScan.AddToAlwaysPreRenderList(pHeli);
			    }
		    }
		    else
		    {
			    if(pHeli->GetAlwaysPreRender())
			    {
				    gPostScan.RemoveFromAlwaysPreRenderList(pHeli);
			    }
		    }
        	//B* 1558073 Move ProcessSearchLight update to CVehicle::ProcessPostCamera
            //pHeli->GetSearchLight()->ProcessSearchLight(pHeli);
        }

        // Add in a shocking event if the helicopter is flying
        if( pHeli->GetMainRotorSpeed() > 0.1f && pHeli->ShouldGenerateEngineShockingEvents() )
		{
			CEventShockingHelicopterOverhead ev(*pHeli);
			CShockingEventsManager::Add(ev);
		}

		// clone seaplanes should never be anchored as it causes problems with the prediction code		
		if(pSeaPlaneExtension)
		{
			pSeaPlaneExtension->GetAnchorHelper().Anchor(false);
		}		
    }
	else
	{
		//Establish an anchor because one couldn't be established on ChangeOwner - continue to try at a throttled rate; If the boat has a driver - it shouldn't be anchored
		if (pHeli->GetDriver())
		{
			m_bLockedToXY = false;
		}

		if (pHeli->GetIsInWater() && m_bLockedToXY && !pSeaPlaneExtension->GetAnchorHelper().IsAnchored())
		{
			//Throttle this check
			if (m_attemptedAnchorTime < fwTimer::GetSystemTimeInMilliseconds())
			{
				m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + NETOBJHELI_ANCHOR_TIME_DELAY;

				//Try to anchor because it should be anchored
				if (pSeaPlaneExtension->GetAnchorHelper().CanAnchorHere(true))
				{
					pSeaPlaneExtension->GetAnchorHelper().Anchor(m_bLockedToXY);
				}
			}
		}
	}

    return true;
}

// Name         :   CreateNetBlender
// Purpose      :
void CNetObjHeli::CreateNetBlender()
{
    CHeli *pHeli = GetHeli();
    gnetAssert(pHeli);
    gnetAssert(!GetNetBlender());

    netBlender *blender = rage_new CNetBlenderHeli(pHeli, ms_heliBlenderData);
    gnetAssert(blender);
    blender->Reset();
    SetNetBlender(blender);
}

// sync parser getter functions
void CNetObjHeli::GetHeliHealthData(CHeliHealthDataNode& data)
{
    CHeli* pHeli = GetHeli();
    gnetAssert(pHeli);

    float health = rage::Max(0.0f, pHeli->GetMainRotorHealth());
    data.m_mainRotorHealth = static_cast<u32>(health);
    health = rage::Max(0.0f, pHeli->GetRearRotorHealth());
    data.m_rearRotorHealth = static_cast<u32>(health);

	data.m_canBoomBreak = pHeli->GetCanBreakOffTailBoom();
    data.m_boomBroken   = pHeli->GetIsTailBoomBroken();

	data.m_mainRotorDamageScale = pHeli->GetMainRotorDamageScale();
	data.m_rearRotorDamageScale = pHeli->GetRearRotorDamageScale();
	data.m_tailBoomDamageScale  = pHeli->GetTailBoomDamageScale();

	data.m_hasCustomHealth = m_IsCustomHealthHeli;
	if (data.m_hasCustomHealth)
	{
		s32 bodyHealth    = (s32)pHeli->GetVehicleDamage()->GetBodyHealth();
		s32 gasTankHealth = (s32)pHeli->GetVehicleDamage()->GetPetrolTankHealth();
		s32 engineHealth  = (s32)pHeli->GetVehicleDamage()->GetEngineHealth();

		if (bodyHealth < 0)
		{
			bodyHealth = 0;
		}
		if (gasTankHealth < 0)
		{
			gasTankHealth = 0;
		}
		if (engineHealth < 0)
		{
			engineHealth = 0;
		}

		data.m_bodyHealth = bodyHealth;
		data.m_gasTankHealth = gasTankHealth;
		data.m_engineHealth = engineHealth;
	}

	data.m_disableExpFromBodyDamage = pHeli->GetDisableExplodeFromBodyDamage();
}

void CNetObjHeli::GetHeliControlData(CHeliControlDataNode& data)
{
    CHeli* pHeli = GetHeli();
    gnetAssert(pHeli);

    data.m_yawControl		= pHeli->GetYawControl();
    data.m_pitchControl		= pHeli->GetPitchControl();
    data.m_rollControl		= pHeli->GetRollControl();
    data.m_throttleControl	= pHeli->GetThrottleControl();	
	data.m_mainRotorStopped = pHeli->GetMainRotorSpeed() == 0.0f;

    // clamp
    data.m_yawControl		= Clamp(data.m_yawControl, -1.0f, 1.0f);
    data.m_pitchControl		= Clamp(data.m_pitchControl, -1.0f, 1.0f);
    data.m_rollControl		= Clamp(data.m_rollControl, -1.0f, 1.0f);
    data.m_throttleControl	= Clamp(data.m_throttleControl, 0.0f, 2.0f);

	data.m_bHasLandingGear  = pHeli->HasLandingGear();
	if (data.m_bHasLandingGear)
	{
		data.m_landingGearState = pHeli->GetLandingGear().GetPublicState();
	}

	data.m_hasJetpackStrafeForceScale = pHeli->GetIsJetPack();
	if (data.m_hasJetpackStrafeForceScale)
	{
		data.m_jetPackStrafeForceScale = pHeli->GetJetpackStrafeForceScale();
		data.m_jetPackThrusterThrottle = pHeli->GetJetPackThrusterThrotle();
	}

	data.m_hasActiveAITask = pHeli->GetHeliIntelligence()->GetTaskManager()->GetActiveTask(PED_TASK_TREE_PRIMARY) != 0;
	
	data.m_lockedToXY = false;

	CSeaPlaneExtension* pSeaPlaneExtension = pHeli->GetExtension<CSeaPlaneExtension>();
	if(pSeaPlaneExtension)
	{
		data.m_lockedToXY = pSeaPlaneExtension->GetAnchorHelper().IsAnchored();
	}
}

// sync parser setter functions
void CNetObjHeli::SetHeliHealthData(const CHeliHealthDataNode& data)
{
    CHeli* pHeli = GetHeli();
    gnetAssert(pHeli);

    if (pHeli->GetStatus() != STATUS_WRECKED && !data.m_boomBroken &&
        (data.m_mainRotorHealth > pHeli->GetMainRotorHealth() ||
        data.m_rearRotorHealth > pHeli->GetRearRotorHealth()))
    {
        pHeli->Fix();
    }

    pHeli->SetMainRotorHealth(static_cast<float>(data.m_mainRotorHealth));
    pHeli->SetRearRotorHealth(static_cast<float>(data.m_rearRotorHealth));
	pHeli->SetCanBreakOffTailBoom(data.m_canBoomBreak);

	pHeli->SetMainRotorDamageScale(data.m_mainRotorDamageScale);
	pHeli->SetRearRotorDamageScale(data.m_rearRotorDamageScale);
	pHeli->SetTailBoomDamageScale(data.m_tailBoomDamageScale);

    if (pHeli->GetMainRotorHealth() == 0.0f)
        pHeli->BreakOffMainRotor();
    if (pHeli->GetRearRotorHealth() == 0.0f)
        pHeli->BreakOffRearRotor();
    if (data.m_boomBroken)
        pHeli->BreakOffTailBoom();

	m_IsCustomHealthHeli = data.m_hasCustomHealth;

	if (data.m_hasCustomHealth)
	{
		pHeli->GetVehicleDamage()->SetBodyHealth((float)data.m_bodyHealth);
		pHeli->GetVehicleDamage()->SetPetrolTankHealth((float)data.m_gasTankHealth);
		pHeli->m_Transmission.SetEngineHealth((float)data.m_engineHealth);
	}

	pHeli->SetDisableExplodeFromBodyDamage(data.m_disableExpFromBodyDamage);
}

void CNetObjHeli::SetHeliControlData(const CHeliControlDataNode& data)
{
    CHeli* pHeli = GetHeli();
    gnetAssert(pHeli);

    pHeli->SetYawControl(data.m_yawControl);
    pHeli->SetPitchControl(data.m_pitchControl);
    pHeli->SetRollControl(data.m_rollControl);
    pHeli->SetThrottleControl(data.m_throttleControl);

    m_LastHeliThrottleReceived = data.m_throttleControl;

	if (data.m_bHasLandingGear)
	{
		pHeli->GetLandingGear().SetFromPublicState(pHeli,data.m_landingGearState);
	}

	if (data.m_hasJetpackStrafeForceScale)
	{
		pHeli->SetJetpackStrafeForceScale(data.m_jetPackStrafeForceScale);
		pHeli->SetJetPackThrusterThrotle(data.m_jetPackThrusterThrottle);
	}		

	if (data.m_mainRotorStopped)
	{
		pHeli->SetMainRotorSpeed(0.0f);
	}

	m_HasActiveAITask = data.m_hasActiveAITask;

	m_bLockedToXY = false;
	CSeaPlaneExtension* pSeaPlaneExtension = pHeli->GetExtension<CSeaPlaneExtension>();
	if(pSeaPlaneExtension)
	{
		m_bLockedToXY = data.m_lockedToXY;
	}
}

float CNetObjHeli::GetPlayerDriverScopeDistanceMultiplier() const
{
    static const float HELI_SCOPE_PLAYER_MODIFIER = 11.0f;
	return HELI_SCOPE_PLAYER_MODIFIER;
}

bool CNetObjHeli::FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const
{
	CHeli* pHeli = GetHeli();
	gnetAssert(pHeli);

	// flying planes are never fixed
	bool bFix = CNetObjAutomobile::FixPhysicsWhenNoMapOrInterior(npfbnReason);

    // flying/moving planes are never fixed
    Vector3 targetVelocity(VEC3_ZERO);
    CNetBlenderPhysical *physicalBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());

    if(physicalBlender)
    {
        targetVelocity = physicalBlender->GetLastVelocityReceived();
    }

	static const float MIN_VELOCITY_TO_FIX_SQR = 1.0f;

	if (pHeli)
	{
		bool bInAir = IsClone() ? IsRemoteInAir() : pHeli->IsInAir();

		if (bInAir || (IsClone() && targetVelocity.Mag2() > MIN_VELOCITY_TO_FIX_SQR))
		{
			if(npfbnReason)
			{	
				*npfbnReason = NPFBN_IN_AIR;
			}
			bFix = false;
		}

		if(IsClone() && !m_HasActiveAITask && !pHeli->ShouldFixIfNoCollisionLoadedAroundPosition())
		{
			bFix = true;
		}
	}

	return bFix;
}

// Name         :   ChangeOwner
// Purpose      :   change ownership from one player to another
// Parameters   :   playerIndex          - playerIndex of new owner
//                  migrationType - how the vehicle is migrating
// Returns      :   void
void CNetObjHeli::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
	CHeli* pHeli = GetHeli();
	gnetAssert(pHeli);

    bool bWasClone      = IsClone();
	bool bSearchLightOn = pHeli && pHeli->GetSearchLight() && pHeli->GetSearchLightOn();

	CSearchLight cacheSearchLight(0,VEH_INVALID_ID);

	if(bSearchLightOn)
	{
		CSearchLight* pSearchLight = pHeli->GetSearchLight();
		cacheSearchLight.CopyTargetInfo(*pSearchLight);
	}

	CNetObjAutomobile::ChangeOwner(player,  migrationType);

	if(bSearchLightOn && pHeli->GetSearchLight())
	{
		CSearchLight* pSearchLight = pHeli->GetSearchLight();
		pSearchLight->CopyTargetInfo(cacheSearchLight);

		//When migrating to local make sure the turret is updated immediately to prevent orientation snapping to identity for a frame
		if(!IsClone()  && taskVerifyf(pHeli->GetVehicleWeaponMgr(),"%s Expect a weapon manager",GetLogName()) )
		{
			// Get all weapons and turrets
			atArray<CVehicleWeapon*> weaponArray;
			atArray<CTurret*> turretArray;
			pHeli->GetVehicleWeaponMgr()->GetWeaponsAndTurretsForSeat(0,weaponArray,turretArray);

			int numTurrets = turretArray.size(); 
			for(int iTurretIndex = 0; iTurretIndex < numTurrets; iTurretIndex++)
			{
				turretArray[iTurretIndex]->AimAtWorldPos(pSearchLight->GetCloneTargetPos(), pHeli, false, true);
			}
		}
	}

    if(pHeli)
    {
        if (bWasClone && !IsClone())
        {
            // ensure the vehicle is always using the last received value for the throttle
            pHeli->SetThrottleControl(m_LastHeliThrottleReceived);

			// handle anchor (if any)
			CSeaPlaneExtension* pSeaPlaneExtension = pHeli->GetExtension<CSeaPlaneExtension>();
			if(pSeaPlaneExtension)
			{
				if (m_bLockedToXY && pSeaPlaneExtension->GetAnchorHelper().CanAnchorHere(true))
				{
					pSeaPlaneExtension->GetAnchorHelper().Anchor(m_bLockedToXY);
				}
				else
				{
					m_attemptedAnchorTime = fwTimer::GetSystemTimeInMilliseconds() + NETOBJHELI_ANCHOR_TIME_DELAY;
				}
			}			
        }		
    }
}

bool CNetObjHeli::ShouldOverrideBlenderWhenStoppedPredicting() const
{
    if(m_LastHeliThrottleReceived > 0.01f)
    {
        return true;
    }

    return CNetObjAutomobile::ShouldOverrideBlenderWhenStoppedPredicting();
}

float CNetObjHeli::GetForcedMigrationRangeWhenPlayerTransitioning()
{
	CHeli* pHeli = GetHeli();
	gnetAssert(pHeli);

	// Special case: prevent the heli forcibly migrating to players far away when the player has been killed in it. 
	// If it migrates to a player far away it may fall through the map as it is never fixed by network. The dying player will see this locally
	if (pHeli && pHeli->GetStatus() == STATUS_WRECKED && pHeli->GetDriver() && pHeli->GetDriver()->IsLocalPlayer() && pHeli->GetDriver()->GetIsDeadOrDying())
	{
		return MOVER_BOUNDS_PRESTREAM*MOVER_BOUNDS_PRESTREAM;
	}

	return CNetObjAutomobile::GetForcedMigrationRangeWhenPlayerTransitioning();
}

