//
// name:        CNetObjPlane.cpp
// description: Derived from netObject, this class handles all plane-related network object
//              calls. See NetworkObject.h for a full description of all the methods.
// written by:  John Gurney
//

#include "network/objects/entities/NetObjPlane.h"

// framework headers
#include "fwscene/stores/staticboundsstore.h"

// game headers
#include "event/EventShocking.h"
#include "event/ShockingEvents.h"
#include "network/network.h"
#include "network/Objects/Prediction/NetBlenderHeli.h"
#include "Peds/Ped.h"
#include "Peds/PopCycle.h"
#include "vehicles/Planes.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "audio/planeaudioentity.h"

NETWORK_OPTIMISATIONS()

CHeliBlenderData	s_planeBlenderData;
CHeliBlenderData*	CNetObjPlane::ms_planeBlenderData = &s_planeBlenderData;
CPlaneSyncTree      *CNetObjPlane::ms_planeSyncTree;


CNetObjPlane::CPlaneScopeData CNetObjPlane::ms_planeScopeData;

CNetObjPlane::CNetObjPlane(CPlane					*plane,
                         const NetworkObjectType	type,
                         const ObjectId				objectID,
                         const PhysicalPlayerIndex  playerIndex,
                         const NetObjFlags			localFlags,
                         const NetObjFlags			globalFlags) :
CNetObjAutomobile(plane, type, objectID, playerIndex, localFlags, globalFlags)
, m_LockOnTargetID(NETWORK_INVALID_OBJECT_ID)
, m_LockOnState(0)
, m_LastPlaneThrottleReceived(0.0f)
, m_CloneDoorsBroken(0)
, m_planeBroken(0)
, m_brake(0.0f)
{
}

void CNetObjPlane::CreateSyncTree()
{
    ms_planeSyncTree = rage_new CPlaneSyncTree(VEHICLE_SYNC_DATA_BUFFER_SIZE);
}

void CNetObjPlane::DestroySyncTree()
{
	ms_planeSyncTree->ShutdownTree();
    delete ms_planeSyncTree;
    ms_planeSyncTree = 0;
}

netINodeDataAccessor *CNetObjPlane::GetDataAccessor(u32 dataAccessorType)
{
    netINodeDataAccessor *dataAccessor = 0;

    if(dataAccessorType == IPlaneNodeDataAccessor::DATA_ACCESSOR_ID())
    {
        dataAccessor = (IPlaneNodeDataAccessor *)this;
    }
    else
    {
        dataAccessor = CNetObjAutomobile::GetDataAccessor(dataAccessorType);
    }

    return dataAccessor;
}

bool CNetObjPlane::Update()
{
    CPlane *plane = GetPlane();
	gnetAssert(plane);

    if (IsClone())
    {
        CNetBlenderVehicle *netBlenderVehicle = SafeCast(CNetBlenderVehicle, GetNetBlender());

        if(netBlenderVehicle->HasStoppedPredictingPosition())
        {
            // if the blender for this vehicle has stopped predicting position (due to not receiving a
            // position update for too long) stop the vehicle from driving off
			vehicleDebugf3("CNetObjPlane::Update--plane[%p][%s][%s]--IsClone--HasStoppedPredictingPosition--SetThrottleControl(0.0f)",plane,plane->GetModelName(),plane->GetNetworkObject() ? plane->GetNetworkObject()->GetLogName() : "");
            plane->SetThrottleControl(0.0f);
        }
        else
        {
            // ensure the vehicle is always using the last received value for the throttle
			vehicleDebugf3("CNetObjPlane::Update--plane[%p][%s][%s]--IsClone--!HasStoppedPredictingPosition--SetThrottleControl(m_LastPlaneThrottleReceived[%f])",plane,plane->GetModelName(),plane->GetNetworkObject() ? plane->GetNetworkObject()->GetLogName() : "",m_LastPlaneThrottleReceived);
			plane->SetThrottleControl(m_LastPlaneThrottleReceived);
        }
    }
	else
	{
		if (plane->GetLandingGear().GetPublicState() != CLandingGear::STATE_BROKEN)
		{
			if (plane->GetLandingGearDamage().GetSectionHealth(0) <= 0.f)
			{
				plane->GetLandingGear().SetFromPublicState(plane,CLandingGear::STATE_BROKEN);
			}
		}
	}

	// force broken planes with player drivers into high lod, so that we see the damage at a distance
	if (m_planeBroken)
	{
		CPed* pDriver = plane->GetDriver();

		if (!pDriver)
		{
			pDriver = plane->GetSeatManager()->GetLastDriver();
		}

		if (pDriver && pDriver->IsAPlayerPed())
		{
			plane->m_nVehicleFlags.bRequestDrawHD = true;
		}
	}
	
	return CNetObjAutomobile::Update();
}

bool CNetObjPlane::ProcessControl()
{
	CPlane	*pPlane = GetPlane();
    
	gnetAssert(pPlane);

	if( pPlane->GetDriver() && pPlane->GetDriver()->IsPlayer() && pPlane->IsEngineOn() && pPlane->IsInAir() && pPlane->ShouldGenerateEngineShockingEvents() )
	{
		if ( !CPopCycle::IsCurrentZoneAirport() )
		{
			CEventShockingPlaneFlyby ev(*pPlane);
			CShockingEventsManager::Add(ev);
		}
	}
	else if( pPlane->IsPlayerDrivingOnGround() )
	{
		CEventShockingMadDriver madDriverEvent( *(CEntity*)pPlane->GetDriver() );
		CShockingEventsManager::Add(madDriverEvent);
	}

	CNetObjAutomobile::ProcessControl();

	if (IsClone())
    {
        UpdateLockOnTarget();

		pPlane->SetBrake(m_brake);
	}

	return true;
}

void CNetObjPlane::CreateNetBlender()
{
	CPlane	*pPlane = GetPlane();
	gnetAssert(pPlane);
	gnetAssert(!GetNetBlender());

	netBlender *blender = rage_new CNetBlenderHeli(pPlane, ms_planeBlenderData);
	gnetAssert(blender);
	blender->Reset();
	SetNetBlender(blender);
}

void CNetObjPlane::ChangeOwner(const netPlayer& player, eMigrationType migrationType)
{
    CPlane *plane = GetPlane();
	gnetAssert(plane);

    bool bWasClone = IsClone();

	CNetObjAutomobile::ChangeOwner(player, migrationType);

    if(plane)
    {
        if (bWasClone && !IsClone())
        {
            // ensure the vehicle is always using the last received value for the throttle
            plane->SetThrottleControl(m_LastPlaneThrottleReceived);
			m_CloneDoorsBroken =0;
        }
    }
}

// sync parser get functions
void  CNetObjPlane::GetPlaneGameState(CPlaneGameStateDataNode& data)
{
	CPlane* pPlane = GetPlane();
    gnetAssert(pPlane);

	data.m_LandingGearPublicState = pPlane->GetLandingGear().GetPublicState();

	data.m_DamagedSections = data.m_BrokenSections = data.m_RotorBroken = data.m_IndividualPropellerFlags = 0;

	m_planeBroken = false;

	for (int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
	{
		CAircraftDamage::eAircraftSection section = static_cast<CAircraftDamage::eAircraftSection>(i);

		data.m_SectionDamage[i] = pPlane->GetAircraftDamage().GetSectionHealthAsFractionActual(section);
		data.m_SectionDamage[i] = Clamp(data.m_SectionDamage[i], -1.0f, 1.0f);

		if (data.m_SectionDamage[i] < 1.0f)
		{
			data.m_DamagedSections |= (1<<i);
		}

		if (pPlane->GetAircraftDamage().HasSectionBrokenOff(pPlane, section))
		{
			data.m_BrokenSections |= (1<<i);
			m_planeBroken = true;
		}	
	}

	data.m_EngineDamageScale = pPlane->m_Transmission.GetEngineDamageScale();

	// Sections damage scale
	data.m_HasCustomSectionDamageScale = false;
	if (pPlane->GetAircraftDamage().GetHasCustomSectionDamageScale())
	{
		data.m_HasCustomSectionDamageScale = true;
		for (int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
		{
			data.m_SectionDamageScale[i] = pPlane->GetAircraftDamage().GetSectionDamageScale(i);
		}
	}

	// Landing gear sections damage scale
	data.m_HasCustomLandingGearSectionDamageScale = false;
	if (pPlane->GetLandingGearDamage().GetHasCustomSectionDamageScale())
	{
		data.m_HasCustomLandingGearSectionDamageScale = true;
		for (int i=0; i<NUM_PLANE_LANDING_GEAR_DAMAGE_SECTIONS; i++)
		{
			data.m_LandingGearSectionDamageScale[i] = pPlane->GetLandingGearDamage().GetSectionDamageScale(i);
		}
	}

	int numPropellers = pPlane->GetNumPropellors();
	gnetAssertf(numPropellers <= MAX_NUM_SYNCED_PROPELLERS, "Plane: %s (%s) has more propellers than allowed (%d)", GetLogName(), pPlane->GetModelName(), numPropellers);
	for(int i=0; i < numPropellers; i++)
	{
		if (pPlane->IsRotorBroken(i))
		{
			data.m_RotorBroken |= (1<<i);
		}

		if (pPlane->IsIndividualPropellerOn(i))
		{
			data.m_IndividualPropellerFlags |= (1<<i);
		}
	}

    if(pPlane->GetLockOnTarget() && pPlane->GetLockOnTarget()->GetNetworkObject())
    {
        data.m_LockOnTarget = pPlane->GetLockOnTarget()->GetNetworkObject()->GetObjectID();
        data.m_LockOnState = static_cast<unsigned>(pPlane->GetHomingLockOnState());
    }
    else
    {
        data.m_LockOnTarget = NETWORK_INVALID_OBJECT_ID;
        data.m_LockOnState = static_cast<unsigned>(CEntity::HLOnS_NONE);
    }

	data.m_AIIgnoresBrokenPartsForHandling = pPlane->GetAircraftDamage().GetIgnoreBrokenOffPartsForAIHandling(); 
	data.m_ControlSectionsBreakOffFromExplosions = pPlane->GetAircraftDamage().GetControlSectionsBreakOffFromExplosions();

	data.m_LODdistance = pPlane->GetLodDistance();
	data.m_disableExpFromBodyDamage = pPlane->GetDisableExplodeFromBodyDamage();
	data.m_disableExlodeFromBodyDamageOnCollision = pPlane->GetDisableExplodeFromBodyDamageOnCollision();

	data.m_AllowRollAndYawWhenCrashing = pPlane->GetAllowRollAndYawWhenCrashing();
	data.m_dipStraightDownWhenCrashing = pPlane->GetDipStraightDownWhenCrashing();
}

void  CNetObjPlane::GetPlaneControlData(CPlaneControlDataNode& data)
{

	CPlane* pPlane = GetPlane();
	gnetAssert(pPlane);

	data.m_verticalFlightMode = pPlane->GetDesiredVerticalFlightModeRatio();

	data.m_yawControl		= pPlane->GetYawControl();
	data.m_pitchControl		= pPlane->GetPitchControl();
	data.m_rollControl		= pPlane->GetRollControl();
	data.m_throttleControl	= pPlane->GetThrottleControl();
	data.m_brake			= pPlane->GetBrake();

	// clamp
	data.m_yawControl		= Clamp(data.m_yawControl, -1.0f, 1.0f);
	data.m_pitchControl		= Clamp(data.m_pitchControl, -1.0f, 1.0f);
	data.m_rollControl		= Clamp(data.m_rollControl, -1.0f, 1.0f);
	data.m_throttleControl	= Clamp(data.m_throttleControl, 0.0f, 2.0f);
	data.m_brake			= Clamp(data.m_brake, -1.0f, 1.0f);

	data.m_hasActiveAITask = pPlane->GetPlaneIntelligence()->GetTaskManager()->GetActiveTask(PED_TASK_TREE_PRIMARY) != 0;
}

// sync parser setter functions
void CNetObjPlane::SetPlaneGameState(const CPlaneGameStateDataNode& data)
{
	CPlane* pPlane = GetPlane();
    gnetAssert(pPlane);

	if ((pPlane->GetLandingGear().GetPublicState() == CLandingGear::STATE_BROKEN) && (data.m_LandingGearPublicState != CLandingGear::STATE_BROKEN))
	{
		vehicleDebugf3("CNetObjPlane::SetPlaneGameState -- fix remote landing gear as authority is fine");
		pPlane->GetLandingGear().Fix(pPlane);
	}
	else if ((pPlane->GetLandingGear().GetPublicState() != CLandingGear::STATE_BROKEN) && (data.m_LandingGearPublicState == CLandingGear::STATE_BROKEN))
	{
		vehicleDebugf3("CNetObjPlane::SetPlaneGameState -- break remote landing gear as authority is broken");
		pPlane->GetLandingGearDamage().BreakAll(pPlane);
	}
	pPlane->GetLandingGear().SetFromPublicState(pPlane, data.m_LandingGearPublicState);

	int numPropellers = pPlane->GetNumPropellors();
	for(int i=0; i < numPropellers; i++)
	{
		if (data.m_RotorBroken & (1<<i))
		{
			pPlane->BreakOffRotor(i);
		}

		// these can be enabled and disabled - set based on value
		pPlane->EnableIndividualPropeller(i, (data.m_IndividualPropellerFlags & (1<<i)) != 0);
	}

	for (int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
	{
		CAircraftDamage::eAircraftSection section = static_cast<CAircraftDamage::eAircraftSection>(i);

#if !__FINAL
		if(data.m_DamagedSections==0)
		{
			float sectionDamage = MAX(pPlane->GetAircraftDamage().GetSectionHealthAsFraction(section), 0.0f);

			if (sectionDamage < 1.0f)
			{   //B* 162443 - Some part of the plane has got better therefore the whole plane must have been debug healed on the remote - fix local one too
				Displayf("CNetObjPlane::SetPlaneGameState--DEBUGONLY--m_DamagedSections==0 && section[%d] sectionDamage[%f] < 1.0f-->Fix",section,sectionDamage);
				pPlane->Fix();
				break;
			}
		}

		if(data.m_BrokenSections==0 && pPlane->GetAircraftDamage().HasSectionBrokenOff(pPlane, section))
		{   //B* 162443 - Some part of the plane has got better therefore the whole plane must have been debug healed on the remote - fix local one too
			Displayf("CNetObjPlane::SetPlaneGameState--DEBUGONLY--m_BrokenSections==0 && section[%d] HasSectionBrokenOff-->Fix",section);
			pPlane->Fix();
			m_planeBroken = false;
			break;
		}
#endif
		if (data.m_DamagedSections & (1<<i))
		{
			pPlane->GetAircraftDamage().SetSectionHealthAsFraction(section, data.m_SectionDamage[i]);
		}

		if ((data.m_BrokenSections & (1<<i)) && !pPlane->GetAircraftDamage().HasSectionBrokenOff(pPlane, section))
		{
			pPlane->GetAircraftDamage().SetSectionHealth(section, 0.0f); //ensure the health is set to 0 before breakoff as is done in BreakOffComponent
			pPlane->GetAircraftDamage().BreakOffSection(pPlane, section, false, true);
			m_planeBroken = true;
		}
	}

	pPlane->m_Transmission.SetEngineDamageScale(data.m_EngineDamageScale);

	if (data.m_HasCustomSectionDamageScale)
	{
		for(int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
		{
			pPlane->GetAircraftDamage().SetSectionDamageScale(i, data.m_SectionDamageScale[i]);
		}
	}
	else
	{
		pPlane->GetAircraftDamage().SetHasCustomSectionDamageScale(false);
		for(int i=0; i<NUM_PLANE_DAMAGE_SECTIONS; i++)
		{
			pPlane->GetAircraftDamage().SetSectionDamageScale(i, 1.0f);
		}
	}

	if (data.m_HasCustomLandingGearSectionDamageScale)
	{
		for(int i=0; i<NUM_PLANE_LANDING_GEAR_DAMAGE_SECTIONS; i++)
		{
			pPlane->GetLandingGearDamage().SetSectionDamageScale(i, data.m_LandingGearSectionDamageScale[i]);
		}
	}
	else
	{
		pPlane->GetLandingGearDamage().SetHasCustomSectionDamageScale(false);
		for(int i=0; i<NUM_PLANE_LANDING_GEAR_DAMAGE_SECTIONS; i++)
		{
			pPlane->GetLandingGearDamage().SetSectionDamageScale(i, 1.0f);
		}
	}

    m_LockOnTargetID = data.m_LockOnTarget;
    m_LockOnState    = data.m_LockOnState;
	
	pPlane->GetAircraftDamage().SetIgnoreBrokenOffPartsForAIHandling(data.m_AIIgnoresBrokenPartsForHandling);
	pPlane->GetAircraftDamage().SetControlSectionsBreakOffFromExplosions(data.m_ControlSectionsBreakOffFromExplosions);

    UpdateLockOnTarget();

	pPlane->SetLodDistance(data.m_LODdistance);
	pPlane->SetDisableExplodeFromBodyDamage(data.m_disableExpFromBodyDamage);
	pPlane->SetDisableExplodeFromBodyDamageOnCollision(data.m_disableExlodeFromBodyDamageOnCollision);

	pPlane->SetAllowRollAndYawWhenCrashing(data.m_AllowRollAndYawWhenCrashing);
	pPlane->SetDipStraightDownWhenCrashing(data.m_dipStraightDownWhenCrashing);
}

void CNetObjPlane::SetPlaneControlData(const CPlaneControlDataNode& data)
{
	CPlane* pPlane = GetPlane();
	gnetAssert(pPlane);

	pPlane->SetYawControl(data.m_yawControl);
	pPlane->SetPitchControl(data.m_pitchControl);
	pPlane->SetRollControl(data.m_rollControl);
	pPlane->SetThrottleControl(data.m_throttleControl);

	m_brake = data.m_brake;
	pPlane->SetBrake(m_brake);

    m_LastPlaneThrottleReceived = data.m_throttleControl;

	vehicleDebugf3("CNetObjPlane::SetPlaneControlData--plane[%p][%s][%s]--data.m_yawControl[%f] data.m_pitchControl[%f] data.m_rollControl[%f] data.m_throttleControl[%f]",pPlane,pPlane->GetModelName(),pPlane->GetNetworkObject() ? pPlane->GetNetworkObject()->GetLogName() : "",data.m_yawControl,data.m_pitchControl,data.m_rollControl,data.m_throttleControl);

	if (pPlane->GetVerticalFlightModeAvaliable())
	{
		if(data.m_verticalFlightMode < 1.0f && IsClose(pPlane->GetDesiredVerticalFlightModeRatio(), 1.0f, SMALL_FLOAT))
		{
			((audPlaneAudioEntity*)pPlane->GetVehicleAudioEntity())->OnVerticalFlightModeChanged(0.0f);
		}
		else if(data.m_verticalFlightMode > 0.0f && IsClose(pPlane->GetDesiredVerticalFlightModeRatio(), 0.0f, SMALL_FLOAT))
		{
			((audPlaneAudioEntity*)pPlane->GetVehicleAudioEntity())->OnVerticalFlightModeChanged(1.0f);
		}
		pPlane->SetDesiredVerticalFlightModeRatio(data.m_verticalFlightMode);
		m_lastReceivedVerticalFlightModeRatio = data.m_verticalFlightMode;
	}
	else
	{
		m_lastReceivedVerticalFlightModeRatio = 0.0f;
	}

	m_HasActiveAITask = data.m_hasActiveAITask;
}

float CNetObjPlane::GetPlayerDriverScopeDistanceMultiplier() const
{
    static const float PLANE_SCOPE_PLAYER_MODIFIER = 11.0f;
	return PLANE_SCOPE_PLAYER_MODIFIER;
}

bool CNetObjPlane::FixPhysicsWhenNoMapOrInterior(unsigned* npfbnReason) const
{
	CPlane* pPlane = GetPlane();
	gnetAssert(pPlane);

	bool bFix = CNetObjAutomobile::FixPhysicsWhenNoMapOrInterior(npfbnReason);

	// flying/moving planes are never fixed
    Vector3 targetVelocity(VEC3_ZERO);
    CNetBlenderPhysical *physicalBlender = static_cast<CNetBlenderPhysical *>(GetNetBlender());

    if(physicalBlender)
    {
        targetVelocity = physicalBlender->GetLastVelocityReceived();
    }

	static const float MIN_VELOCITY_TO_FIX_SQR = 1.0f;

	if (pPlane)
	{
		bool bInAir = IsClone() ? IsRemoteInAir() : pPlane->IsInAir();

		if (bInAir || (IsClone() && targetVelocity.Mag2() > MIN_VELOCITY_TO_FIX_SQR))
		{	
			if(npfbnReason)
			{
				*npfbnReason = NPFBN_IN_AIR;
			}
			bFix = false;
		}

		if(IsClone() && !m_HasActiveAITask && !pPlane->ShouldFixIfNoCollisionLoadedAroundPosition())
		{
			bFix = true;
		}
	}

	return bFix;
}

void CNetObjPlane::UpdateLockOnTarget()
{
    CPlane *pPlane = GetPlane();
	gnetAssert(pPlane);

    if(pPlane)
    {
        if(m_LockOnTargetID == NETWORK_INVALID_OBJECT_ID)
        {
            pPlane->SetHomingLockOnState(CEntity::HLOnS_NONE);
            pPlane->SetLockOnTarget(0);
        }
        else
        {
            netObject *targetObject  = NetworkInterface::GetNetworkObject(m_LockOnTargetID);
            CVehicle  *targetVehicle = targetObject ? NetworkUtils::GetVehicleFromNetworkObject(targetObject) : 0;

            if(targetVehicle)
            {
                CEntity::eHomingLockOnState lockOnState = static_cast<CEntity::eHomingLockOnState>(m_LockOnState);

                pPlane->SetHomingLockOnState(lockOnState);
                pPlane->SetLockOnTarget(targetVehicle);

                targetVehicle->UpdateHomingLockedOntoState(lockOnState);
            }
        }
    }
}

bool CNetObjPlane::ShouldOverrideBlenderWhenStoppedPredicting() const
{
    if(m_LastPlaneThrottleReceived > 0.01f)
    {
        return true;
    }

    return CNetObjAutomobile::ShouldOverrideBlenderWhenStoppedPredicting();
}

float CNetObjPlane::GetForcedMigrationRangeWhenPlayerTransitioning()
{
	CPlane *plane = GetPlane();
	gnetAssert(plane);

	// Special case: prevent the heli forcibly migrating to players far away when the player has been killed in it. 
	// If it migrates to a player far away it may fall through the map as it is never fixed by network. The dying player will see this locally
	if (plane && plane->GetStatus() == STATUS_WRECKED && plane->GetDriver() && plane->GetDriver()->IsLocalPlayer() && plane->GetDriver()->GetIsDeadOrDying())
	{
		return MOVER_BOUNDS_PRESTREAM*MOVER_BOUNDS_PRESTREAM;
	}

	return CNetObjAutomobile::GetForcedMigrationRangeWhenPlayerTransitioning();
}

void CNetObjPlane::UpdateCloneDoorsBroken()
{
	CPlane *plane = GetPlane();
	gnetAssertf(plane,"%s, null plane",GetLogName());

	if (plane && taskVerifyf(IsClone(),"%s only expected to be used on clones",GetLogName())  )
	{
		if(m_CloneDoorsBroken != 0)
		{
			for(int i=0; i<plane->GetNumDoors(); i++)
			{
				if (m_CloneDoorsBroken & (1<<i))
				{
					if (plane->GetDoor(i)->GetIsIntact(plane))
					{
						plane->GetDoor(i)->BreakOff(plane, false);
					}
				}
			}
		}
	}
}

