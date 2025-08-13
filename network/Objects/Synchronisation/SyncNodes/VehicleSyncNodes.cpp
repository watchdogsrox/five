//
// name:        VehicleSyncNodes.cpp
// description: Network sync nodes used by CNetObjVehicles
// written by:    John Gurney
//

#include "network/network.h"
#include "network/general/NetworkStreamingRequestManager.h"
#include "network/objects/entities/NetObjVehicle.h"
#include "network/objects/synchronisation/syncnodes/VehicleSyncNodes.h"
#include "streaming/streaming.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vehicles/VehicleDefines.h"
#include "vfx/vehicleglass/VehicleGlassComponent.h"

NETWORK_OPTIMISATIONS()

static const unsigned SIZEOF_HEALTH = 19;
DATA_ACCESSOR_ID_IMPL(IVehicleNodeDataAccessor);

bool CVehicleCreationDataNode::CanApplyData(netSyncTreeTargetObject* UNUSED_PARAM(pObj)) const
{
    // ensure that the model is loaded and ready for drawing for this ped
	fwModelId modelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_modelHash, &modelId);

	if (!gnetVerifyf(modelId.IsValid(), "Unrecognised custom model hash %u for vehicle clone", m_modelHash))
	{
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, model not valid\r\n");
		return false;
	}

	if(!CNetworkStreamingRequestManager::RequestModel(modelId))
    {
		CNetwork::GetMessageLog().WriteDataValue("FAIL", "Can't apply data, model not loaded\r\n");
        return false;
    }

	return true;
}

void CVehicleCreationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_SEED             = 16;
	static const unsigned SIZEOF_STATUS           = 3;
    static const unsigned SIZEOF_LAST_DRIVER_TIME = 32;

	SERIALISE_MODELHASH(serialiser, m_modelHash, "Model");
	SERIALISE_VEHICLE_POP_TYPE(serialiser, m_popType,  "Pop type");
	SERIALISE_UNSIGNED(serialiser, m_randomSeed, SIZEOF_SEED, "Random Seed");

	if (m_popType == POPTYPE_MISSION || m_popType == POPTYPE_PERMANENT)
	{
		SERIALISE_BOOL(serialiser, m_takeOutOfParkedCarBudget, "Take Out Of Parked Car Budget");
	}

	SERIALISE_UNSIGNED(serialiser, m_maxHealth, SIZEOF_HEALTH, "Max Health");

	SERIALISE_UNSIGNED(serialiser, m_status,         SIZEOF_STATUS,           "Vehicle Status");
    SERIALISE_UNSIGNED(serialiser, m_lastDriverTime, SIZEOF_LAST_DRIVER_TIME, "Last driver time");
	SERIALISE_BOOL(serialiser, m_needsToBeHotwired,         "Needs To Be Hotwired");
	SERIALISE_BOOL(serialiser, m_tyresDontBurst,            "Tyres Don't Burst");

	SERIALISE_BOOL(serialiser, m_usesVerticalFlightMode, "Uses Special Flight Mode");
}

void CVehicleAngVelocityDataNode::Serialise(CSyncDataBase& serialiser)
{
    bool velocityIsZero = m_IsSuperDummyAngVel || (m_packedAngVelocityX==0 && m_packedAngVelocityY==0 && m_packedAngVelocityZ==0);

    SERIALISE_BOOL(serialiser, velocityIsZero);

    if(!velocityIsZero || serialiser.GetIsMaximumSizeSerialiser())
    {
        SERIALISE_VELOCITY(serialiser, m_packedAngVelocityX, m_packedAngVelocityY, m_packedAngVelocityZ, SIZEOF_ANGVELOCITY, "Angular Velocity");
        m_IsSuperDummyAngVel = false;
    }
    else
    {
        SERIALISE_BOOL(serialiser, m_IsSuperDummyAngVel, "Is Superdummy Ang Velocity");
        m_packedAngVelocityX = 0;
        m_packedAngVelocityY = 0;
        m_packedAngVelocityZ = 0;
    }
}

bool CVehicleAngVelocityDataNode::HasDefaultState() const 
{ 
    return (m_packedAngVelocityX == 0 && m_packedAngVelocityY == 0 && m_packedAngVelocityZ == 0) && (m_IsSuperDummyAngVel== false);
}

void CVehicleAngVelocityDataNode::SetDefaultState() 
{
    m_packedAngVelocityX = m_packedAngVelocityY = m_packedAngVelocityZ = 0;
    m_IsSuperDummyAngVel = false;
}


CVehicleTaskDataNode::fnTaskDataLogger CVehicleTaskDataNode::m_taskDataLogFunction = 0;
CVehicleTaskDataNode::fnTaskDataLogger CVehicleProximityMigrationDataNode::m_taskDataLogFunction = 0;

void CVehicleGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_RADIO_STATION = 7;
	static const unsigned int SIZEOF_DOORLOCKS     = datBitsNeeded<CARLOCK_NUM_STATES>::COUNT;
	static const unsigned int SIZEOF_NUM_WINDOWS   = 6;
	static const unsigned int SIZEOF_CUSTOM_PATHNODE_STREAMING_RADIUS = 10;
	static const unsigned int SIZEOF_EXPLOSION_TIME	= 32; 
	static const unsigned int SIZEOF_OVERRIDELIGHTS = 3;
	static const unsigned int SIZEOF_IND_DOORFILTER = 8;
	static const unsigned int SIZEOF_JUNCTION_ARRIVAL_TIME = 32;
	static const unsigned int SIZEOF_JUNCTION_COMMAND = datBitsNeeded<JUNCTION_COMMAND_LAST>::COUNT;
	static const unsigned int SIZEOF_DOOR_STATE	= 4;
	static const unsigned int SIZEOF_XENON_LIGHT_COLOR = 8;
	static const unsigned int SIZEOF_TIMER = 32;
	CompileTimeAssert(SIZEOF_JUNCTION_COMMAND <= 7);	// s8 m_junctionCommand

	bool bHasWindowState = (m_windowsDown != 0);
	bool bDefaultXenonLight = (m_xenonLightColor == XENON_LIGHT_DEFAULT_COLOR);
	bool bHasDoorState = (m_doorLockState != 1) || (m_doorsBroken != 0) || (m_doorsOpen != 0) || (m_doorsNotAllowedToBeBrokenOff != 0);
	bool bHasIndividualDoorState = (m_doorIndividualLockedStateFilter > 0);
	bHasDoorState |= bHasIndividualDoorState;
	bool bHasNonStandardCustomPathNodeStreamingRadius = m_customPathNodeStreamingRadius > 0.0f;
	bool bDefaultData = !bHasWindowState && bDefaultXenonLight && !bHasDoorState && m_isDriveable &&
						!m_sirenOn && !m_runningRespotTimer &&	!m_alarmSet && 
						!m_alarmActivated && !m_isStationary && !m_isParked && !m_forceOtherVehsToStop  &&
						!bHasNonStandardCustomPathNodeStreamingRadius && !m_DontTryToEnterThisVehicleIfLockedForPlayer;

	SERIALISE_UNSIGNED(serialiser, m_radioStation, SIZEOF_RADIO_STATION, "Radio Station");
	SERIALISE_BOOL(serialiser, m_pretendOccupants, "Using Pretend Occupants");
	SERIALISE_BOOL(serialiser, m_engineOn, "Engine On");
	SERIALISE_BOOL(serialiser, m_engineStarting, "Engine Starting");
	SERIALISE_BOOL(serialiser, m_engineSkipEngineStartup, "Engine SkipStartup");
	SERIALISE_BOOL(serialiser, m_handBrakeOn, "Handbrake On");
    SERIALISE_BOOL(serialiser, m_scriptSetHandbrakeOn, "Script Set Handbrake On");
	SERIALISE_BOOL(serialiser, m_moveAwayFromPlayer, "Move Away From Player");

	SERIALISE_BOOL(serialiser, bDefaultData);

	if (!bDefaultData || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, bDefaultXenonLight, "Is Default Xenon Light Color");
		if (!bDefaultXenonLight || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_xenonLightColor, SIZEOF_XENON_LIGHT_COLOR, "Xenon Light Color");
		}
		else
		{
			m_xenonLightColor = XENON_LIGHT_DEFAULT_COLOR;
		}

		SERIALISE_BOOL(serialiser, m_sirenOn, "Siren On");
		SERIALISE_BOOL(serialiser, m_runningRespotTimer, "Running Respot Timer");
		if(m_runningRespotTimer || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_BOOL(serialiser, m_useRespotEffect, "Use Respot Effect");
		}
		else
		{
			m_useRespotEffect = true;
		}

		SERIALISE_BOOL(serialiser, m_isDriveable, "Is Drivable");
		SERIALISE_BOOL(serialiser, bHasDoorState);

		if (bHasDoorState || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_doorLockState, SIZEOF_DOORLOCKS, "Door Lock State");
			SERIALISE_UNSIGNED(serialiser, m_doorsBroken,   SIZEOF_NUM_DOORS, "Doors Broken");
			SERIALISE_UNSIGNED(serialiser, m_doorsNotAllowedToBeBrokenOff,   SIZEOF_NUM_DOORS, "Doors Not Allowed To Be Broken Off");

			SERIALISE_UNSIGNED( serialiser, m_doorsOpen, SIZEOF_NUM_DOORS, "Num Doors Open" );

			for( int i = 0; i < SIZEOF_NUM_DOORS; i++ )
			{
				if( ( m_doorsOpen & ( 1 << i ) ) || serialiser.GetIsMaximumSizeSerialiser() )
				{
					SERIALISE_UNSIGNED( serialiser, m_doorsOpenRatio[ i ], SIZEOF_DOOR_STATE, "Doors Open" );
				}
				else
				{
					m_doorsOpenRatio[ i ] = 0;
				}
			}

			SERIALISE_UNSIGNED(serialiser, m_doorIndividualLockedStateFilter, SIZEOF_IND_DOORFILTER, "Door Individual Filter");

			for (int i=0; i < SIZEOF_NUM_DOORS; ++i)
			{
				if ((m_doorIndividualLockedStateFilter & (1<<i)) || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_UNSIGNED(serialiser, m_doorIndividualLockedState[i], SIZEOF_DOORLOCKS, "Door Individual Lock State");
				}
				else
				{
					m_doorIndividualLockedState[i] = CARLOCK_NONE;
				}
			}
		}
		else
		{
			m_doorLockState = 1;
			m_doorsBroken = 0;
			m_doorsOpen = 0;
			m_doorsNotAllowedToBeBrokenOff = 0;

			m_doorIndividualLockedStateFilter = 0;
			for (int i=0; i < SIZEOF_NUM_DOORS; ++i)
			{
				m_doorIndividualLockedState[i] = CARLOCK_NONE;
				m_doorsOpenRatio[i] = 0;
			}
		}

		SERIALISE_BOOL(serialiser, bHasWindowState);

		if (bHasWindowState || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_windowsDown, SIZEOF_NUM_WINDOWS, "Windows Down");
		}
		else
		{
			m_windowsDown = 0;
		}

		SERIALISE_BOOL(serialiser, m_alarmSet, "Alarm Set");
		SERIALISE_BOOL(serialiser, m_alarmActivated, "Alarm Activated");
		SERIALISE_BOOL(serialiser, m_isStationary, "Is Stationary");
		SERIALISE_BOOL(serialiser, m_isParked, "Is Parked");
		SERIALISE_BOOL(serialiser, m_forceOtherVehsToStop, "Force Other Vehs to Stop");
        SERIALISE_BOOL(serialiser, m_DontTryToEnterThisVehicleIfLockedForPlayer, "Player Doesn't Try Enter A Vehicle Locked To Them");

		SERIALISE_BOOL(serialiser, bHasNonStandardCustomPathNodeStreamingRadius);
		if (bHasNonStandardCustomPathNodeStreamingRadius || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_customPathNodeStreamingRadius, 3000.0f, SIZEOF_CUSTOM_PATHNODE_STREAMING_RADIUS, "Custom Pathnode Streaming Radius");
		}
	}
	else
	{
		m_xenonLightColor = XENON_LIGHT_DEFAULT_COLOR;
		m_isDriveable = true; 
		m_moveAwayFromPlayer = m_sirenOn = m_runningRespotTimer = m_alarmSet = m_alarmActivated = m_isStationary = m_isParked = m_forceOtherVehsToStop = m_DontTryToEnterThisVehicleIfLockedForPlayer = false;
		m_doorLockState = 1;
		m_doorsBroken = 0;
		m_doorsOpen = 0;
		m_doorsNotAllowedToBeBrokenOff = 0;
		m_windowsDown = 0;
		m_customPathNodeStreamingRadius = -1.0f;

		m_doorIndividualLockedStateFilter = 0;
		for (int i=0; i < SIZEOF_NUM_DOORS; ++i)
		{
			m_doorIndividualLockedState[i] = CARLOCK_NONE;
			m_doorsOpenRatio[i] = 0;
		}
	}

	// serialise timed explosion
	SERIALISE_BOOL(serialiser, m_hasTimedExplosion, "Has Timed Explosion");
	if(m_hasTimedExplosion || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_timedExplosionTime, SIZEOF_EXPLOSION_TIME, "Timed Explosion Time");
		SERIALISE_OBJECTID(serialiser, m_timedExplosionCulprit, "Timed Explosion Culprit");
	}
	else
	{
		m_timedExplosionTime = 0;
		m_timedExplosionCulprit = NETWORK_INVALID_OBJECT_ID;
	}

	// serialise exclusive drivers
	bool hasExclusiveDrivers = false;

	for (u32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
	{
		if (m_exclusiveDriverPedID[i] != NETWORK_INVALID_OBJECT_ID)
		{
			hasExclusiveDrivers = true;
		}
	}
	SERIALISE_BOOL(serialiser, hasExclusiveDrivers, "Has Exclusive Driver ID");
	if(hasExclusiveDrivers || serialiser.GetIsMaximumSizeSerialiser())
	{
		for (u32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
		{
			SERIALISE_OBJECTID(serialiser, m_exclusiveDriverPedID[i], "Exclusive Driver ID");
		}
	}
    else
    {
        for (u32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
        {
            m_exclusiveDriverPedID[i] = NETWORK_INVALID_OBJECT_ID;
        }
    }

	SERIALISE_BOOL(serialiser, m_hasLastDriver, "Has Last Driver ID");
	if(m_hasLastDriver || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_lastDriverPedID, "Last Driver ID");
	}

	// vehicle lights
	SERIALISE_BOOL(serialiser, m_lightsOn, "Lights On");
	SERIALISE_BOOL(serialiser, m_headlightsFullBeamOn, "Headlights FullBeam On");
	SERIALISE_UNSIGNED(serialiser, m_overridelights, SIZEOF_OVERRIDELIGHTS, "Override Lights");

	// roof
	SERIALISE_BOOL(serialiser, m_roofLowered, "Roof Lowered");

	// removal
	SERIALISE_BOOL(serialiser, m_removeWithEmptyCopOrWreckedVehs, "Remove With Empty Cop Or Wrecked Veh");

	SERIALISE_BOOL(serialiser, m_radioStationChangedByDriver, "Radio Station Changed By Driver");
	SERIALISE_BOOL(serialiser, m_noDamageFromExplosionsOwnedByDriver, "No Damage From Explosions Owned By Driver");
	SERIALISE_BOOL(serialiser, m_flaggedForCleanup, "Flagged For Cleanup");
	SERIALISE_BOOL(serialiser, m_ghost, "Is Ghost");

	SERIALISE_UNSIGNED(serialiser, m_junctionArrivalTime, SIZEOF_JUNCTION_ARRIVAL_TIME, "Junction Arrival Time");
	SERIALISE_UNSIGNED(serialiser, m_junctionCommand, SIZEOF_JUNCTION_COMMAND, "Junction Command"); 

	SERIALISE_BOOL(serialiser, m_influenceWantedLevel, "Influence Wanted Level");
	SERIALISE_BOOL(serialiser, m_hasBeenOwnedByPlayer, "Has Been Owned By Player");
	SERIALISE_BOOL(serialiser, m_AICanUseExclusiveSeats, "AI can use exclusive seats");

	SERIALISE_BOOL(serialiser, m_placeOnRoadQueued,					  "Place on road queued");
	SERIALISE_BOOL(serialiser, m_planeResistToExplosion,			  "Plane resist to explosion");
	SERIALISE_BOOL(serialiser, m_mercVeh,                             "Mercenary Vehicle");
	SERIALISE_BOOL(serialiser, m_vehicleOccupantsTakeExplosiveDamage, "Veh occupants take explosive damage");
	SERIALISE_BOOL(serialiser, m_canEjectPassengersIfLocked,		  "Can Eject players if they have been locked out");
	SERIALISE_BOOL(serialiser, m_disableSuperDummy,		              "Disable super dummy");
	SERIALISE_BOOL(serialiser, m_checkForEnoughRoomToFitPed,		  "Check whether there's enough room for a ped in the seat before entry");

	// serialise player locks
	bool bHasPlayerLocks = m_PlayerLocks != 0;

	SERIALISE_BOOL(serialiser, bHasPlayerLocks);

	if (bHasPlayerLocks || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_PlayerLocks, MAX_NUM_PHYSICAL_PLAYERS,  "Player Locks");
	}
	else
	{
		m_PlayerLocks = 0;
	}

	SERIALISE_BOOL(serialiser, m_RemoveAggressivelyForCarjackingMission, "Allows the vehicle to be removed aggressively during the car jacking missions");

	SERIALISE_BOOL(serialiser, m_OverridingVehHorn, "Is Vehicle Horn Sound Overriden");
	if(m_OverridingVehHorn || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_HASH(serialiser, m_OverridenVehHornHash, "Overriden Horn Sound Hash");
	}
	else
	{
		m_OverridenVehHornHash = 0;
	}

	SERIALISE_BOOL(serialiser, m_UnFreezeWhenCleaningUp, "UnfreezeWhenCleeningUp");

	SERIALISE_BOOL(serialiser, m_usePlayerLightSettings, "Use Player Light Settings");

	bool hasHeadlightMultiplier = !IsClose(m_HeadlightMultiplier, 1.0f, 0.001f);
	SERIALISE_BOOL(serialiser, hasHeadlightMultiplier, "Has Headlight Multiplier");
	if(hasHeadlightMultiplier || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const float MAX_VALUE_HEADLIGHT_MULTIPLIER = 16.0f;
		static const unsigned NUM_BITS_HEADLIGHT_MULTIPLIER = 8;
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_HeadlightMultiplier, MAX_VALUE_HEADLIGHT_MULTIPLIER, NUM_BITS_HEADLIGHT_MULTIPLIER, "Headlight Multiplier");
	}
	else
	{
		m_HeadlightMultiplier = 1.0f;
	}

	SERIALISE_BOOL(serialiser, m_isTrailerAttachmentEnabled, "Is Trailer Attachment Enabled");

	SERIALISE_BOOL(serialiser, m_detachedTombStone, "Detached TombStone");
	
	bool syncExtraBrokenFlags = m_ExtraBrokenFlags != 0;
	SERIALISE_BOOL(serialiser, syncExtraBrokenFlags, "Sync Extra Broken Flags");
	if(syncExtraBrokenFlags || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_ExtraBrokenFlags, MAX_EXTRA_BROKEN_FLAGS, "Extra Broken Flags");
	}
	else
	{
		m_ExtraBrokenFlags = 0;
	}

	SERIALISE_BOOL(serialiser, m_fullThrottleActive, "Full Throttle Active");
	if (m_fullThrottleActive || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_fullThrottleEndTime, SIZEOF_TIMER, "Full Throttle End Time");
	}
	else
	{
		m_fullThrottleEndTime = 0;
	}

	SERIALISE_BOOL(serialiser, m_driftTyres, "Drift Tyres");

	static const unsigned SIZEOF_DOWNFORCE_MODIFIER = 10;
	static const float MAX_DAMAGE_MODIFIER_VALUE = 2.0f;

	bool hasDownforceModifier = !IsClose(m_downforceModifierRear, 1.0f, SMALL_FLOAT) || !IsClose(m_downforceModifierFront, 1.0f, SMALL_FLOAT);

	SERIALISE_BOOL(serialiser, hasDownforceModifier, "Has Down Force Modifier");
	if(hasDownforceModifier || serialiser.GetIsMaximumSizeSerialiser())
	{		
		SERIALISE_PACKED_FLOAT(serialiser, m_downforceModifierRear, MAX_DAMAGE_MODIFIER_VALUE, SIZEOF_DOWNFORCE_MODIFIER, "Downforce Modifier Rear");
		SERIALISE_PACKED_FLOAT(serialiser, m_downforceModifierFront, MAX_DAMAGE_MODIFIER_VALUE, SIZEOF_DOWNFORCE_MODIFIER, "Downforce Modifier Front");
	}
	else
	{
		m_downforceModifierRear = 1.0f;
		m_downforceModifierFront = 1.0f;
	}
}

bool CVehicleGameStateDataNode::HasDefaultState() const
{
	return (m_radioStation == 0 && !m_engineOn                    &&
			!m_moveAwayFromPlayer								  &&
            !m_sirenOn && !m_pretendOccupants                     &&
            !m_runningRespotTimer && m_useRespotEffect			  &&
			m_isDriveable                &&
		    m_doorLockState                              == 1     &&
		    m_doorsBroken                                == 0     &&
            m_doorsOpen                                  == 0     &&
		    m_windowsDown                                == 0     &&
            !m_isStationary                                       &&
			!m_isParked											  &&
			!m_forceOtherVehsToStop								  &&
            m_customPathNodeStreamingRadius              <= 0.0f  &&
			m_xenonLightColor							 == XENON_LIGHT_DEFAULT_COLOR  &&
            m_hasTimedExplosion                          == false &&
            m_DontTryToEnterThisVehicleIfLockedForPlayer == false &&
			m_lightsOn									 == false &&
			m_overridelights							 == NO_CAR_LIGHT_OVERRIDE &&
			m_roofLowered								 == false &&
			m_removeWithEmptyCopOrWreckedVehs            == false &&
			m_radioStationChangedByDriver 				 == false &&
			m_hasLastDriver								 == false &&
			m_junctionArrivalTime						 == UINT_MAX &&
			m_junctionCommand							 == JUNCTION_COMMAND_NOT_ON_JUNCTION &&
			m_influenceWantedLevel						 == false &&
			m_hasBeenOwnedByPlayer						 == false &&
			m_AICanUseExclusiveSeats					 == false &&
			m_doorIndividualLockedStateFilter			 == 0     &&
			m_placeOnRoadQueued							 == false &&
			m_planeResistToExplosion					 == false &&
			m_disableSuperDummy                          == false &&
			m_checkForEnoughRoomToFitPed				 == false &&
			m_mercVeh									 == false &&
			m_vehicleOccupantsTakeExplosiveDamage        == false &&
			m_canEjectPassengersIfLocked				 == false &&
			m_PlayerLocks								 == 0	  &&
			m_RemoveAggressivelyForCarjackingMission     == false &&
			m_OverridingVehHorn							 == false &&
			m_UnFreezeWhenCleaningUp					 == true  &&
			m_usePlayerLightSettings					 == false &&
			m_isTrailerAttachmentEnabled                 == true  &&
			m_detachedTombStone							 == false &&
			m_ExtraBrokenFlags							 == 0 &&
			IsClose(m_HeadlightMultiplier, 1.0f, 0.001f) &&
			IsClose(m_downforceModifierFront, 1.0f, 0.001f) &&
			IsClose(m_downforceModifierRear, 1.0f, 0.01f) &&
			m_driftTyres == false && 
			m_handBrakeOn == false &&
			m_scriptSetHandbrakeOn == false &&
			m_alarmSet == false &&  m_alarmActivated == false &&
			m_headlightsFullBeamOn == false &&
			m_noDamageFromExplosionsOwnedByDriver == false &&
			m_flaggedForCleanup == false &&
			m_ghost == false &&
			m_fullThrottleActive == false
			);
}

void CVehicleGameStateDataNode::SetDefaultState()
{
	m_radioStation                               = 0;
	m_engineOn = m_engineStarting = m_sirenOn    = false; 
	m_engineSkipEngineStartup					 = false;
	m_moveAwayFromPlayer						 = false;
    m_isStationary                               = false;
	m_isParked									 = false;
	m_forceOtherVehsToStop						 = false;
	m_pretendOccupants							 = false;
	m_useRespotEffect							 = true;
	m_runningRespotTimer						 = false; 
	m_isDriveable                                = true;
	m_doorLockState                              = 1;
	m_doorsBroken                                = 0;
    m_doorsOpen                                  = 0;
	m_windowsDown                                = 0;
	m_xenonLightColor							 = XENON_LIGHT_DEFAULT_COLOR;
	m_customPathNodeStreamingRadius              = -1.0f;
	m_hasTimedExplosion                          = false;
    m_DontTryToEnterThisVehicleIfLockedForPlayer = false;
	m_removeWithEmptyCopOrWreckedVehs            = false;
	m_radioStationChangedByDriver 				 = false;
	m_hasLastDriver								 = false;
	m_influenceWantedLevel						 = false;
	m_hasBeenOwnedByPlayer						 = false;
	m_AICanUseExclusiveSeats					 = false;
	m_placeOnRoadQueued							 = false;
	m_disableSuperDummy                          = false;
	m_checkForEnoughRoomToFitPed				 = false;
	m_planeResistToExplosion					 = false;
	m_mercVeh									 = false;
	m_vehicleOccupantsTakeExplosiveDamage		 = false;
	m_canEjectPassengersIfLocked				 = false;
	m_PlayerLocks								 = 0;
	m_RemoveAggressivelyForCarjackingMission	 = false;
	m_OverridingVehHorn							 = false;
	m_OverridenVehHornHash						 = 0;
	m_UnFreezeWhenCleaningUp					 = true;
	m_usePlayerLightSettings					 = false;
	m_HeadlightMultiplier						 = 1.0f;
	m_isTrailerAttachmentEnabled                 = true;
	m_detachedTombStone							 = false;
	m_ExtraBrokenFlags							 = 0;
	m_downforceModifierFront                     = 1.0f;
	m_downforceModifierRear                      = 1.0f;
	m_driftTyres                                 = false;
	m_handBrakeOn								 = false;
	m_scriptSetHandbrakeOn						 = false;
	m_alarmSet									 = false;
	m_alarmActivated							 = false;
	m_headlightsFullBeamOn						 = false;
	m_noDamageFromExplosionsOwnedByDriver		 = false;
	m_flaggedForCleanup							 = false;
	m_ghost										 = false;
	m_fullThrottleActive						 = false;

	for (u32 i=0; i<CVehicle::MAX_NUM_EXCLUSIVE_DRIVERS; ++i)
	{
		m_exclusiveDriverPedID[i] = NETWORK_INVALID_OBJECT_ID;
	}
	
	m_lastDriverPedID = NETWORK_INVALID_OBJECT_ID;

	m_doorIndividualLockedStateFilter			 = 0;
	for (u32 i=0; i<SIZEOF_NUM_DOORS; ++i)
	{
		m_doorIndividualLockedState[i] = CARLOCK_NONE;
	}
	m_junctionArrivalTime						 = UINT_MAX;
	m_junctionCommand							 = JUNCTION_COMMAND_NOT_ON_JUNCTION;
}

void CVehicleScriptGameStateDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_STICK = 8;

	SERIALISE_BOOL(serialiser, m_VehicleFlags.freebies,					    	     "Has Freebies");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.canBeVisiblyDamaged,		    	     "Can Be Visibly Damaged");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.isDrowning,					         "Is Drowning");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.partOfConvoy,				    	     "Part Of Convoy");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.vehicleCanBeTargetted,		         "Vehicle Can Be Targetted");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.takeLessDamage,				         "Take Less Damage");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.lockedForNonScriptPlayers,      	     "Locked For Non Script Players");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.respectLocksWhenHasDriver,      	     "Respect Locks When Has Driver");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.lockDoorsOnCleanup,			         "Lock doors on cleanup");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.shouldFixIfNoCollision,		         "Should Fix If No Collision");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.automaticallyAttaches,		         "Automatically attaches");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.scansWithNonPlayerDriver,	    	     "Scans with non-player driver");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.disableExplodeOnContact,        	     "Disable explode on contact");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.canEngineDegrade,               	     "Can Engine Degrade");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.canPlayerAircraftEngineDegrade, 	     "Can Plane Aircraft Engine Degrade");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.nonAmbientVehicleMayBeUsedByGoToPointAnyMeans, "None Ambient May Be Used By GoToPoint anymeans");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.canBeUsedByFleeingPeds,         	     "Can be used by fleeing peds");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.allowNoPassengersLockOn,       	     "Allow no passengers lock-on");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.allowHomingMissleLockOn,               "Allow homing missile lock-on");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.disablePetrolTankDamage,        	     "Disable petrol tank damage");
    SERIALISE_BOOL(serialiser, m_VehicleFlags.isStolen,                       	     "Is Stolen");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.explodesOnHighExplosionDamage,  	     "Explodes on high explosion damage");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.rearDoorsHaveBeenBlownOffByStickybomb, "Rear Doors Have Been Blown Off By Stickybomb");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.isInCarModShop,                        "Is In Car Mod Shop");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.specialEnterExit,                      "Is special Boss/Goon vehicle");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.onlyStartVehicleEngineOnThrottle,      "Only allow ped to start engine if player accelerates");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.dontOpenRearDoorsOnExplosion,          "Prevents rear doors from opening from sticky explosion");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.disableTowing,        	             "Disable towing");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.dontProcessVehicleGlass,				 "Stop vehicle glass processing");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.useDesiredZCruiseSpeedForLanding,		 "Use desired z cruise speed for landing");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.dontResetTurretHeadingInScriptedCamera,"Will not reset turret heading in scripted cameras");
	SERIALISE_BOOL(serialiser, m_VehicleFlags.disableWantedConesResponse,			 "Disables responses from the wanted cones");

	SERIALISE_BOOL(serialiser, m_isinair,									     	 "Is Vehicle In Air");

	SERIALISE_BOOL(serialiser, m_IsCarParachuting, "Is Parachuting");

	if (m_IsCarParachuting || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_parachuteStickX, 1.0f, SIZEOF_STICK, "Parachute Stick X");
		SERIALISE_PACKED_FLOAT(serialiser, m_parachuteStickY, 1.0f, SIZEOF_STICK, "Parachute Stick Y");
	}	
	else
	{
		m_parachuteStickX = 0;
		m_parachuteStickY = 0;
	}

	SERIALISE_VEHICLE_POP_TYPE(serialiser, m_PopType, "Pop type");

	// serialise team locks
	bool bHasTeamLocks = m_TeamLocks != 0;

	SERIALISE_BOOL(serialiser, bHasTeamLocks);

	if (bHasTeamLocks || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BITFIELD(serialiser, m_TeamLocks, MAX_NUM_TEAMS, "Team Locks");

		bool bHasTeamLockOverrides = m_TeamLockOverrides != 0;

		SERIALISE_BOOL(serialiser, bHasTeamLockOverrides);

		if (bHasTeamLockOverrides || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_BITFIELD(serialiser, m_TeamLockOverrides, MAX_NUM_PHYSICAL_PLAYERS,  "Team Lock Overrides");
		}
		else
		{
			m_TeamLockOverrides = 0;
		}
	}
	else
	{
		m_TeamLocks         = 0;
		m_TeamLockOverrides = 0;
	}

    // serialise garage instance index
    bool hasGarageInstanceIndex = m_GarageInstanceIndex != INVALID_GARAGE_INDEX;

    SERIALISE_BOOL(serialiser, hasGarageInstanceIndex);

    if(hasGarageInstanceIndex || serialiser.GetIsMaximumSizeSerialiser())
    {
        static const unsigned SIZEOF_GARAGE_INDEX = 5;

        SERIALISE_UNSIGNED(serialiser, m_GarageInstanceIndex, SIZEOF_GARAGE_INDEX, "Garage Instance Index");
    }
    else
    {
        m_GarageInstanceIndex = INVALID_GARAGE_INDEX;
    }

	bool hasDamageTreshold = m_DamageThreshold != 0;
	SERIALISE_BOOL(serialiser, hasDamageTreshold, "Has Damage Treshold");
	if(hasDamageTreshold || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned SIZEOF_DAMAGE_TRESHOLD = 13;
		SERIALISE_UNSIGNED(serialiser, m_DamageThreshold, SIZEOF_DAMAGE_TRESHOLD, "Damage Treshold Value");
	}
	else
	{
		m_DamageThreshold = 0;
	}

	bool hasDamageScale = !IsClose(m_fScriptDamageScale, 1.0f);
	SERIALISE_BOOL(serialiser, hasDamageScale, "Has Damage Scale");
	if(hasDamageScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned SIZEOF_DAMAGE_SCALE = 10;
		static const float MAX_SCALE_VALUE = 2.0f;
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fScriptDamageScale, MAX_SCALE_VALUE, SIZEOF_DAMAGE_SCALE, "Script Damage Scale");
	}
	else
	{
		m_fScriptDamageScale = 1.0f;
	}

	bool hasWeaponDamageScale = !IsClose(m_fScriptWeaponDamageScale, 1.0f);
	SERIALISE_BOOL(serialiser, hasWeaponDamageScale, "Has Weapon Damage Scale");
	if(hasWeaponDamageScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned SIZEOF_WEAPON_DAMAGE_SCALE = 6;
		static const float MAX_WEAPON_SCALE_VALUE = 5.0f;
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_fScriptWeaponDamageScale, MAX_WEAPON_SCALE_VALUE, SIZEOF_WEAPON_DAMAGE_SCALE, "Weapon Damage Scale");
	}
	else
	{
		m_fScriptWeaponDamageScale = 1.0f;
	}

	SERIALISE_BOOL(serialiser, m_bBlockWeaponSelection, "Block Weapon Selection");

	SERIALISE_BOOL(serialiser, m_isBeastVehicle, "Is Beast Vehicle");

	SERIALISE_OBJECTID(serialiser, m_VehicleProducingSlipstream, "Slipstreamed Vehicle ID");
	
	SERIALISE_BOOL(serialiser, m_hasParachuteObject, "Has Parachute Object");
	if (m_hasParachuteObject || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_parachuteObjectId, "ID of the parachute object attached to this vehicle");		
	}
	else
	{
		m_parachuteObjectId = NETWORK_INVALID_OBJECT_ID;
	}

	bool hasParachuteTintIndex = m_vehicleParachuteTintIndex != 0;
	SERIALISE_BOOL(serialiser, hasParachuteTintIndex, "Has Parachute Tint Index");
	if(hasParachuteTintIndex || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned SIZEOF_PARACHUTE_TINT_INDEX = 8;
		SERIALISE_UNSIGNED(serialiser, m_vehicleParachuteTintIndex, SIZEOF_PARACHUTE_TINT_INDEX, "Vehicle Parachute Tint Index");
	}
	else
	{
		m_vehicleParachuteTintIndex = 0;
	}

	bool changedScriptRammingImpulseScale = !IsClose(m_fRampImpulseScale, 1.0f); 
	SERIALISE_BOOL(serialiser, changedScriptRammingImpulseScale, "Has Ramming Impulse Scale Changed");
	if(changedScriptRammingImpulseScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned NUMOF_BITS_RAMP_IMPULSE_SCALE = 10;
		unsigned rammingInpulseScale = (unsigned)m_fRampImpulseScale;
		SERIALISE_UNSIGNED(serialiser, rammingInpulseScale, NUMOF_BITS_RAMP_IMPULSE_SCALE, "Ramming Impulse Scale");
		m_fRampImpulseScale = (float)rammingInpulseScale;
	}
	else
	{
		m_fRampImpulseScale = 1.0f;
	}

	bool changedOverrideArriveDistForVehPersuitAttack = !IsClose(m_fOverrideArriveDistForVehPersuitAttack, -1.0f); 
	SERIALISE_BOOL(serialiser, changedOverrideArriveDistForVehPersuitAttack, "Has Arrive Dist For Vehicle Persuit Override");
	if(changedOverrideArriveDistForVehPersuitAttack || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const float MAX_SCRIPT_ARRIVE_DIST_OVERRIDE = 500.0f;
		static const int   NUMOF_BITS_SCRIPT_MAX_ARRIVE_DIST_OVERRIDE = 10;
		SERIALISE_PACKED_FLOAT(serialiser, m_fOverrideArriveDistForVehPersuitAttack, MAX_SCRIPT_ARRIVE_DIST_OVERRIDE, NUMOF_BITS_SCRIPT_MAX_ARRIVE_DIST_OVERRIDE, "Arrive Dist Override For Vehicle Persuit Attack");
	}
	else
	{
		m_fOverrideArriveDistForVehPersuitAttack = -1.0f;
	}

	static const unsigned int SIZEOF_BUOYANCY_FORCE_MULTIPLIER = 14;
	SERIALISE_BOOL(serialiser, m_lockedToXY, "Locked to XY (Anchored)");
	SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_BuoyancyForceMultiplier, 1.0f, SIZEOF_BUOYANCY_FORCE_MULTIPLIER, "Amphibious Buoyancy Force Multiplier");
	
	SERIALISE_BOOL(serialiser, m_disableRampCarImpactDamage, "Disable Ramp Car Impact Damage");
	SERIALISE_BOOL(serialiser, m_bBoatIgnoreLandProbes, "Boat Ignores Land Probes");

	SERIALISE_BOOL(serialiser, m_disableCollisionUponCreation, "Disable collision upon creation");

	const unsigned MAX_DEBUG_TEXT = 128;
	char str[MAX_DEBUG_TEXT];
	for(int i = 0; i < MAX_NUM_VEHICLE_WEAPONS; i++)
	{
		bool hasWeapon = m_restrictedAmmoCount[i] != -1;
		sprintf(str, "Has Vehicle Weapon %d", i);
		SERIALISE_BOOL(serialiser, hasWeapon, str);
		if(hasWeapon || serialiser.GetIsMaximumSizeSerialiser())
		{
			sprintf(str, "Vehicle Weapon %d", i);
			static const unsigned NUM_BITS_VEHICLE_WEAPON = 11;
			SERIALISE_UNSIGNED(serialiser, m_restrictedAmmoCount[i], NUM_BITS_VEHICLE_WEAPON, str);
		}
		else
		{
			m_restrictedAmmoCount[i] = -1;
		}
	}

	SERIALISE_BOOL(serialiser, m_tuckInWheelsForQuadBike, "Tuck In Wheels For Quad Bike");
	
	static const float MAX_ROCKET_BOOST_ERROR_VALUE = 0.0001f;
	bool hasRocketBoostChanged = !IsClose(m_rocketBoostRechargeRate, CVehicle::ROCKET_BOOST_RECHARGE_RATE, MAX_ROCKET_BOOST_ERROR_VALUE);
	SERIALISE_BOOL(serialiser, hasRocketBoostChanged, "Has Rocket Booster Recharge");
	if(hasRocketBoostChanged || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const float MAX_VALUE_ROCKET_RECHARGE_RATE = 2.5f;
		static const unsigned NUMOF_BITS_ROCKET_RECHARGE_RATE = 10;
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_rocketBoostRechargeRate, MAX_VALUE_ROCKET_RECHARGE_RATE, NUMOF_BITS_ROCKET_RECHARGE_RATE, "Rocket Booster Recharge Rate");
	}
	else
	{
		m_rocketBoostRechargeRate = CVehicle::ROCKET_BOOST_RECHARGE_RATE;
	}

	SERIALISE_BOOL(serialiser, m_hasHeliRopeLengthSet, "Has Heli Rope Length Set");
	if(m_hasHeliRopeLengthSet || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_HeliRopeLength, 15.0f, 8, "Heli Rope Length");
	}
	else
	{
		m_HeliRopeLength = 0.0f;
	}

    bool hasExtraBoundAttachAllowance = !IsClose(m_ExtraBoundAttachAllowance, 0.0f, 0.01f);

    SERIALISE_BOOL(serialiser, hasExtraBoundAttachAllowance);
    if(hasExtraBoundAttachAllowance || serialiser.GetIsMaximumSizeSerialiser())
    {
        const float    MAX_EXTRA_BOUND_ATTACH_ALLOWANCE    = 1.0f;
        const unsigned SIZEOF_EXTRA_BOUND_ATTACH_ALLOWANCE = 4;
        SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_ExtraBoundAttachAllowance, MAX_EXTRA_BOUND_ATTACH_ALLOWANCE, SIZEOF_EXTRA_BOUND_ATTACH_ALLOWANCE, "Extra bound attach allowance");
    }
    else
    {
        m_ExtraBoundAttachAllowance = 0.0f;
    }

	SERIALISE_BOOL(serialiser, m_ScriptForceHd, "Script Force Hd");

	bool hasGliderState = m_gliderState != 0;
	SERIALISE_BOOL(serialiser, hasGliderState, "Has Glider State");
	if(hasGliderState || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned SIZEOF_GLIDER_STATE = datBitsNeeded<CVehicle::GLIDER_STATE_MAX-1>::COUNT;
		SERIALISE_UNSIGNED(serialiser, m_gliderState, SIZEOF_GLIDER_STATE, "Vehicle Glider State");
	}
	else
	{
		m_gliderState = 0;
	}

	SERIALISE_BOOL(serialiser, m_CanEngineMissFire, "Can Engine Miss Fire");
	SERIALISE_BOOL(serialiser, m_DisableBreaking, "Disable Breaking");
	SERIALISE_BOOL(serialiser, m_disablePlayerCanStandOnTop, "Disable Player Can Stand On Top");

	static const unsigned NUM_BITS_VEHICLE_MISC_AMMO = 10;
	SERIALISE_UNSIGNED(serialiser, m_BombAmmoCount, NUM_BITS_VEHICLE_MISC_AMMO, "Bomb Ammo Count");
	SERIALISE_UNSIGNED(serialiser, m_CountermeasureAmmoCount, NUM_BITS_VEHICLE_MISC_AMMO, "Countermeasure Ammo Count");

	SERIALISE_BOOL(serialiser, m_canPickupEntitiesThatHavePickupDisabled, "Can Pickup Entities That Have Pickup Disabled");

	bool hasMaxSpeedSet = !IsClose(m_ScriptMaxSpeed, -1.0f, SMALL_FLOAT);
	SERIALISE_BOOL(serialiser, hasMaxSpeedSet, "Has Script Max Speed Set");
	if(hasMaxSpeedSet || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const float MAX_SCRIPT_MAX_SPEED = 320.0f;
		static const int   NUMOF_BITS_SCRIPT_MAX_SPEED = 10;

		SERIALISE_PACKED_FLOAT(serialiser, m_ScriptMaxSpeed, MAX_SCRIPT_MAX_SPEED, NUMOF_BITS_SCRIPT_MAX_SPEED, "Script Max Speed");
	}
	else
	{
		m_ScriptMaxSpeed = -1.0f;
	}

	bool hasCollisionWithMapDamageScale = !IsClose(m_CollisionWithMapDamageScale, 1.0f, SMALL_FLOAT);
	SERIALISE_BOOL(serialiser, hasCollisionWithMapDamageScale, "Has Collision With Map Damage Scale");
	if(hasCollisionWithMapDamageScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const float MAX_COLLISION_WITH_MAP_DAMAGE_SCALE = 10.0f;
		static const int   NUMOF_BITS_COLLISION_WITH_MAP_DAMAGE_SCALE = 8;
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_CollisionWithMapDamageScale, MAX_COLLISION_WITH_MAP_DAMAGE_SCALE, NUMOF_BITS_COLLISION_WITH_MAP_DAMAGE_SCALE, "Collision With Map Damage Scale");
	}
	else
	{
		m_CollisionWithMapDamageScale = 1.0f;
	}

	SERIALISE_BOOL(serialiser, m_InSubmarineMode, "Is In Submarine Mode");
	SERIALISE_BOOL(serialiser, m_TransformInstantly, "Transform Instantly");

	SERIALISE_BOOL(serialiser, m_AllowSpecialFlightMode, "Allow Special Flight Mode");
	if(m_AllowSpecialFlightMode || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_BOOL(serialiser, m_SpecialFlightModeUsed, "Using special flight mode");
	}
	else
	{
		m_SpecialFlightModeUsed = false;
	}

	SERIALISE_BOOL(serialiser, m_homingCanLockOnToObjects, "Homing can lock onto objects");
	SERIALISE_BOOL(serialiser, m_DisableVericalFlightModeTransition, "Disable Verical Flight Mode");
	SERIALISE_BOOL(serialiser, m_HasOutriggerDeployed, "Has Outrigger Deployed");
	SERIALISE_BOOL(serialiser, m_UsingAutoPilot, "Using Autopilot");
    SERIALISE_BOOL(serialiser, m_DisableHoverModeFlight, "Disable HoverMode Flight");

	SERIALISE_BOOL(serialiser, m_RadioEnabledByScript, "Radio Enabled by Script");

	SERIALISE_BOOL(serialiser, m_bIncreaseWheelCrushDamage, "Increase Wheel Crush Damage");

}

bool CVehicleScriptGameStateDataNode::ResetScriptStateInternal()
{
	m_VehicleFlags.freebies						                 = false;
    m_VehicleFlags.canBeVisiblyDamaged			                 = true;
    m_VehicleFlags.isDrowning					                 = false;
    m_VehicleFlags.partOfConvoy					                 = false;
    m_VehicleFlags.takeLessDamage				                 = false;
	m_VehicleFlags.vehicleCanBeTargetted		                 = false;
	m_VehicleFlags.lockedForNonScriptPlayers	                 = false;
	m_VehicleFlags.respectLocksWhenHasDriver                     = false;
    m_VehicleFlags.lockDoorsOnCleanup                            = false;
    m_VehicleFlags.shouldFixIfNoCollision		                 = false;
    m_VehicleFlags.automaticallyAttaches                         = true;
    m_VehicleFlags.scansWithNonPlayerDriver                      = false;
    m_VehicleFlags.disableExplodeOnContact                       = false;
    m_VehicleFlags.canEngineDegrade                              = false;
    m_VehicleFlags.canPlayerAircraftEngineDegrade                = false;
    m_VehicleFlags.nonAmbientVehicleMayBeUsedByGoToPointAnyMeans = false;
    m_VehicleFlags.canBeUsedByFleeingPeds                        = true;
    m_VehicleFlags.allowNoPassengersLockOn                       = false;
	m_VehicleFlags.allowHomingMissleLockOn						 = true;
    m_VehicleFlags.disablePetrolTankDamage                       = false;
    m_VehicleFlags.isStolen                                      = false;
	m_VehicleFlags.explodesOnHighExplosionDamage                 = true;
	m_VehicleFlags.rearDoorsHaveBeenBlownOffByStickybomb		 = false;
	m_VehicleFlags.specialEnterExit								 = false;
	m_VehicleFlags.onlyStartVehicleEngineOnThrottle				 = false;
	m_VehicleFlags.dontOpenRearDoorsOnExplosion					 = false;
	m_VehicleFlags.disableTowing                                 = false;
	m_VehicleFlags.dontProcessVehicleGlass						 = false;
	m_VehicleFlags.useDesiredZCruiseSpeedForLanding				 = false;
	m_VehicleFlags.dontResetTurretHeadingInScriptedCamera		 = false;
	m_VehicleFlags.disableWantedConesResponse					 = false;

	m_PopType									= POPTYPE_RANDOM_AMBIENT;
	m_TeamLocks                                 = 0;
	m_TeamLockOverrides                         = 0;
	m_isinair									= false;
    m_GarageInstanceIndex                       = INVALID_GARAGE_INDEX;
	m_DamageThreshold							= 0;
	m_parachuteObjectId                         = NETWORK_INVALID_OBJECT_ID;
	m_vehicleParachuteTintIndex                 = 0;

	m_ScriptForceHd								= false;
	m_CanEngineMissFire							= true;
	m_DisableBreaking							= false;
	m_fScriptDamageScale                        = 1.0f;
	m_disablePlayerCanStandOnTop				= false;
	m_BombAmmoCount								= 0;
	m_CountermeasureAmmoCount					= 0;
	m_canPickupEntitiesThatHavePickupDisabled	= false;
	m_ScriptMaxSpeed							= -1.0f;
	m_CollisionWithMapDamageScale				= 1.0f;
	m_InSubmarineMode							= false;
	m_TransformInstantly						= false;
	m_AllowSpecialFlightMode					= false;
	m_SpecialFlightModeUsed						= false;
	m_homingCanLockOnToObjects                  = false;
	m_DisableVericalFlightModeTransition		= false;
	m_HasOutriggerDeployed                      = false;
	m_UsingAutoPilot							= false;
	m_DisableHoverModeFlight                    = false;
	m_RadioEnabledByScript						= true;
	m_bIncreaseWheelCrushDamage					= false;

	return true;
}

bool CVehicleScriptGameStateDataNode::HasDefaultState() const
{
	return (!m_VehicleFlags.takeLessDamage                 &&
		    !m_VehicleFlags.vehicleCanBeTargetted          &&
		    !m_VehicleFlags.partOfConvoy                   &&
		    !m_VehicleFlags.isDrowning                     &&
		    !m_VehicleFlags.lockedForNonScriptPlayers      &&
		    !m_VehicleFlags.respectLocksWhenHasDriver      &&
		     m_VehicleFlags.canBeVisiblyDamaged            &&
		     m_VehicleFlags.freebies                       &&
		    !m_VehicleFlags.shouldFixIfNoCollision         &&
		     m_PopType             == POPTYPE_MISSION      &&
		     m_TeamLocks           == 0                    &&
		     m_TeamLockOverrides   == 0				       &&
		     m_isinair             == false                &&
             m_GarageInstanceIndex == INVALID_GARAGE_INDEX &&
			 m_DamageThreshold	   == 0                    &&
			 m_fScriptDamageScale  == 1.0f			       &&
			 m_fScriptWeaponDamageScale == 1.0f			   &&
			 m_lockedToXY          == false				   &&
			 m_ScriptForceHd	   == false				   &&
			 m_CanEngineMissFire   == true				   &&
			 m_DisableBreaking	   == false                &&
			 m_disableCollisionUponCreation == false	   &&
			 m_disablePlayerCanStandOnTop == false		   &&
			 m_BombAmmoCount		   == 0				   &&
			 m_CountermeasureAmmoCount == 0				   &&
			 m_canPickupEntitiesThatHavePickupDisabled == false &&
			 IsClose(m_ScriptMaxSpeed, 1.0f, SMALL_FLOAT)  &&
			 IsClose(m_CollisionWithMapDamageScale, 1.0f, SMALL_FLOAT) &&
			 m_InSubmarineMode		== false			   &&
			 m_TransformInstantly	== false			   &&
			 m_AllowSpecialFlightMode == false			   &&
			 m_SpecialFlightModeUsed == false              &&
			 m_homingCanLockOnToObjects == false		   &&
			 m_DisableVericalFlightModeTransition == false &&
			 m_HasOutriggerDeployed == false			   &&
			 m_UsingAutoPilot == false                     &&
			 m_DisableHoverModeFlight == false			   &&
			 m_RadioEnabledByScript == true				   &&
			 m_bIncreaseWheelCrushDamage == false);
}

void CVehicleScriptGameStateDataNode::SetDefaultState()
{
	m_VehicleFlags.takeLessDamage            = m_VehicleFlags.vehicleCanBeTargetted     = m_VehicleFlags.partOfConvoy       = false;
	m_VehicleFlags.isDrowning                = m_VehicleFlags.lockedForNonScriptPlayers = false;
	m_VehicleFlags.canBeVisiblyDamaged       = m_VehicleFlags.freebies                  = true;
	m_VehicleFlags.respectLocksWhenHasDriver = false;
	m_VehicleFlags.shouldFixIfNoCollision	 = false;
	m_PopType                                = POPTYPE_MISSION;
	m_TeamLocks                              = 0;
	m_TeamLockOverrides                      = 0;
    m_GarageInstanceIndex                    = INVALID_GARAGE_INDEX;
	m_isinair                                = false;
	m_DamageThreshold						 = 0;
	m_fScriptDamageScale					 = 1.0f;
	m_fScriptWeaponDamageScale				 = 1.0f;
	m_lockedToXY                             = false;
	m_BuoyancyForceMultiplier                = 1.0f;
	m_disableCollisionUponCreation           = false;
	m_BombAmmoCount							 = 0;
	m_CountermeasureAmmoCount				 = 0;
	m_ScriptMaxSpeed						 = -1.0f;
	m_CollisionWithMapDamageScale			 = 1.0f;
	m_InSubmarineMode						 = false;
	m_TransformInstantly					 = false;
	m_AllowSpecialFlightMode				 = false;
	m_SpecialFlightModeUsed					 = false;
	m_homingCanLockOnToObjects               = false;
	m_DisableVericalFlightModeTransition	 = false;
	m_HasOutriggerDeployed                   = false;
	m_UsingAutoPilot						 = false;
    m_DisableHoverModeFlight                 = false;
	m_RadioEnabledByScript					 = true;
	m_bIncreaseWheelCrushDamage				 = false;
}

void CVehicleHealthDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int NUMBITS_TYRE_WEAR_RATE = 8;
	static const unsigned int SIZEOF_NUM_WHEELS        = 4;
	static const unsigned int SIZEOF_SUSPENSION_HEALTH = 10;
	static const unsigned int SIZEOF_WEAPON_HASH = 32;//6; this is a hash now!
#if PH_MATERIAL_ID_64BIT
	static const unsigned int SIZEOF_MATERIAL_ID = 64;
#else
	static const unsigned int SIZEOF_MATERIAL_ID = 32;
#endif // PH_MATERIAL_ID_64BIT

	bool bDamagedEngine     = m_packedEngineHealth != ENGINE_HEALTH_MAX;
	bool bDamagedPetrolTank = m_packedPetrolTankHealth != VEH_DAMAGE_HEALTH_STD;

	SERIALISE_BOOL(serialiser, m_isWrecked, "Is Wrecked");
	SERIALISE_BOOL(serialiser, m_isBlownUp, "Is Wrecked By Explosion");

	SERIALISE_BOOL(serialiser, bDamagedEngine);
	SERIALISE_BOOL(serialiser, bDamagedPetrolTank);

	if (bDamagedEngine || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_INTEGER(serialiser, m_packedEngineHealth,     SIZEOF_HEALTH, "Packed Engine Health");
	}
	else
	{
		m_packedEngineHealth = static_cast<s32>(ENGINE_HEALTH_MAX);
	}

	if (bDamagedPetrolTank || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_INTEGER(serialiser, m_packedPetrolTankHealth, SIZEOF_HEALTH, "Packed Petrol Tank Health");	
	}
	else
	{
		m_packedPetrolTankHealth = static_cast<s32>(VEH_DAMAGE_HEALTH_STD);
	}

	SERIALISE_BOOL(serialiser, m_tyreHealthDefault, "Tyres Have Default Health");
	SERIALISE_BOOL(serialiser, m_suspensionHealthDefault);

	if (!m_tyreHealthDefault || !m_suspensionHealthDefault || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_numWheels, SIZEOF_NUM_WHEELS, "Number Of Wheels");

		// this is the maximum allowable value for the number of wheels
		if(serialiser.GetIsMaximumSizeSerialiser())
		{
			m_numWheels = NUM_VEH_CWHEELS_MAX;
		}

		if(!m_tyreHealthDefault || serialiser.GetIsMaximumSizeSerialiser())
		{
			for(u32 index = 0; index < m_numWheels; index++)
			{
				bool hasWearRate = m_tyreWearRate[index] >= 0.0f && m_tyreWearRate[index] <= 1.0f;
				SERIALISE_BOOL(serialiser, hasWearRate, "Has Tyre Wear Rate");
				if (hasWearRate || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_tyreWearRate[index], 1.0f, NUMBITS_TYRE_WEAR_RATE, "Tyre Wear Rate");
				}
				else
				{
					m_tyreWearRate[index] = 0.0f;
				}

				SERIALISE_BOOL(serialiser, m_tyreDamaged[index],   "Tyre Damaged");
				SERIALISE_BOOL(serialiser, m_tyreDestroyed[index], "Tyre Destroyed");
				SERIALISE_BOOL(serialiser, m_tyreBrokenOff[index], "Tyre BrokenOff");
				SERIALISE_BOOL(serialiser, m_tyreFire[index], "Tyre Fire");
			}
		}

		if(!m_suspensionHealthDefault || serialiser.GetIsMaximumSizeSerialiser())
		{
			for(u32 index = 0; index < m_numWheels; index++)
			{
				bool bSuspensionDamaged = m_suspensionHealth[index] < SUSPENSION_HEALTH_DEFAULT;

				SERIALISE_BOOL(serialiser, bSuspensionDamaged);

				if (bSuspensionDamaged || serialiser.GetIsMaximumSizeSerialiser())
				{
					SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_suspensionHealth[index], SUSPENSION_HEALTH_DEFAULT, SIZEOF_SUSPENSION_HEALTH, "Suspension Health");
				}
				else
				{
					m_suspensionHealth[index] = SUSPENSION_HEALTH_DEFAULT;
				}
			}
		}
	}

	SERIALISE_BOOL(serialiser, m_hasMaxHealth, "Has max health");
	if(!m_hasMaxHealth || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_health, SIZEOF_HEALTH, "Health");		
	}
	SERIALISE_BOOL(serialiser, m_healthsame, "Health Same As Body Health");
	if (!m_healthsame || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_bodyhealth, SIZEOF_HEALTH, "Body Health");
	}

	bool bHasDamageEntity = m_weaponDamageEntity != NETWORK_INVALID_OBJECT_ID;

	SERIALISE_BOOL(serialiser, bHasDamageEntity);

	if (bHasDamageEntity || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_OBJECTID(serialiser, m_weaponDamageEntity, "Weapon Damage Entity");
		SERIALISE_UNSIGNED(serialiser, m_weaponDamageHash, SIZEOF_WEAPON_HASH, "Weapon Damage Hash");
	}
	else
	{
		m_weaponDamageEntity = NETWORK_INVALID_OBJECT_ID;
		m_weaponDamageHash   = 0;
	}

	SERIALISE_UNSIGNED(serialiser, m_extinguishedFireCount, SIZEOF_ALTERED_COUNT, "Extinguish Fire Count");
	SERIALISE_UNSIGNED(serialiser, m_fixedCount, SIZEOF_ALTERED_COUNT, "Fixed Count");

	bool hasLastDamageMaterial = m_lastDamagedMaterialId != 0;
	SERIALISE_BOOL(serialiser, hasLastDamageMaterial, "Has Last Damaged Material");
	if(hasLastDamageMaterial || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_lastDamagedMaterialId, SIZEOF_MATERIAL_ID, "Last Damaged Material Id");
	}
	else
	{
		m_lastDamagedMaterialId = 0;
	}
}

bool CVehicleHealthDataNode::HasDefaultState() const
{
	return (!m_isWrecked && 
		m_packedEngineHealth == static_cast<s32>(ENGINE_HEALTH_MAX) &&
		m_packedPetrolTankHealth == static_cast<s32>(VEH_DAMAGE_HEALTH_STD) &&
		m_tyreHealthDefault && m_suspensionHealthDefault);
}

void CVehicleHealthDataNode::SetDefaultState()
{
	m_fixedCount = 0;
	m_extinguishedFireCount = 0;
	m_isWrecked = false;
	m_isBlownUp = false;
	m_packedEngineHealth = static_cast<s32>(ENGINE_HEALTH_MAX);
	m_packedPetrolTankHealth = static_cast<s32>(VEH_DAMAGE_HEALTH_STD);
	m_tyreHealthDefault = m_suspensionHealthDefault = true;
	m_hasMaxHealth = true;
	m_healthsame = true;
	m_lastDamagedMaterialId = 0;
}

void CVehicleSteeringDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_STEERING = 10; // this needs to be quite precise as it impacts how well the network blending code works

	SERIALISE_PACKED_FLOAT(serialiser, m_steeringAngle, 1.0f, SIZEOF_STEERING, "Steering Angle");
}

bool CVehicleSteeringDataNode::HasDefaultState() const 
{ 
	return m_steeringAngle > -0.01f && m_steeringAngle < 0.01f; 
}

void CVehicleSteeringDataNode::SetDefaultState() 
{
	m_steeringAngle = 0.0f; 
}

void CVehicleControlDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_BRAKE		      = 3;
	static const unsigned int SIZEOF_THROTTLE		  = 6;
	static const float MAX_STOPPING_DIST			  = 40.0f;
	static const unsigned SIZEOF_STOPPING_DIST		  = 13;
	static const unsigned SIZEOF_SUSPENSION			  = 9;
	static const unsigned int SIZEOF_NUM_WHEELS       = datBitsNeeded<NUM_VEH_CWHEELS_MAX>::COUNT;
	static const unsigned SIZEOF_TOP_SPEED_PERCENT    = 10;
	static const unsigned SIZEOF_TARGET_GRAVITY_SCALE = 9;
	static const unsigned int SIZEOF_ANGCONTROL       = 8;

	bool braking      = (m_brakePedal > 0.0f);
	bool throttleOpen = !IsClose(m_throttle, 0.0f, 0.01f);
	bool roadNode = m_roadNodeAddress != 0;

	SERIALISE_BOOL(serialiser, braking);

    SERIALISE_BOOL(serialiser, m_bNitrousActive, "Is Nitrous Active");
	SERIALISE_BOOL(serialiser, m_bIsNitrousOverrideActive, "Is Nitrous Override Active");	

	if (braking || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_brakePedal, 1.0f, SIZEOF_BRAKE, "Brake Control");
	}
	else
	{
		m_brakePedal = 0.0f;
	}

	SERIALISE_BOOL(serialiser, throttleOpen);

	if (throttleOpen || serialiser.GetIsMaximumSizeSerialiser())
	{
		gnetAssert(SIZEOF_THROTTLE >= SIZEOF_BRAKE + 1);
		SERIALISE_PACKED_FLOAT(serialiser, m_throttle, 1.0f, SIZEOF_THROTTLE, "Throttle Control");
	}
	else
	{
		m_throttle = 0.0f;
	}

	SERIALISE_BOOL(serialiser, roadNode);

	if (roadNode || serialiser.GetIsMaximumSizeSerialiser())
	{
		// possibly split this into 2 later and sync the node region with the sector node
		SERIALISE_ROAD_NODE_ADDRESS(serialiser, m_roadNodeAddress, "Road node address");
	}
	else
	{
		m_roadNodeAddress = 0;
	}

	SERIALISE_BOOL(serialiser, m_kersActive, "KERS Active");

	SERIALISE_BOOL(serialiser, m_bringVehicleToHalt, "Bring Vehicle To Halt");

	if (m_bringVehicleToHalt || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_BVTHStoppingDist, MAX_STOPPING_DIST, SIZEOF_STOPPING_DIST, "BVTH Stopping Dist");
		SERIALISE_BOOL(serialiser, m_BVTHControlVertVel, "BVTH Control Vertical Velocity");
	}

	SERIALISE_BOOL(serialiser, m_bModifiedLowriderSuspension, "Modified Lowrider Suspension");
	SERIALISE_BOOL(serialiser, m_bAllLowriderHydraulicsRaised, "All Lowrider Suspension Raised");

	SERIALISE_UNSIGNED(serialiser, m_numWheels, SIZEOF_NUM_WHEELS, "Number Of Wheels");

	// This is the maximum allowable value for the number of wheels
	if(serialiser.GetIsMaximumSizeSerialiser())
	{
		m_numWheels = NUM_VEH_CWHEELS_MAX;
	}

	if (m_bModifiedLowriderSuspension || m_bAllLowriderHydraulicsRaised || serialiser.GetIsMaximumSizeSerialiser())
	{
		for (int i = 0; i < m_numWheels; i++)
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_fLowriderSuspension[i], 1.0f, SIZEOF_SUSPENSION, "Lowrider Wheel Suspension");
		}
	}
	else
	{
		for (int i = 0; i < m_numWheels; i++)
		{
			m_fLowriderSuspension[i] = 0;
		}
	}

	SERIALISE_BOOL(serialiser, m_reducedSuspensionForce, "Reduced suspension force");
	SERIALISE_BOOL(serialiser, m_bPlayHydraulicsBounceSound, "Hydraulics Bounce Sound Effect");
	SERIALISE_BOOL(serialiser, m_bPlayHydraulicsActivationSound, "Hydraulics Activation Sound Effect");
	SERIALISE_BOOL(serialiser, m_bPlayHydraulicsDeactivationSound, "Hydraulics Deactivation Sound Effect");

	SERIALISE_BOOL(serialiser, m_bIsClosingAnyDoor, "Is Closing Any Of The Doors");

	SERIALISE_BOOL(serialiser, m_HasTopSpeedPercentage, "Has Top Speed Percentage Set");
	if(m_HasTopSpeedPercentage || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_topSpeedPercent, 150.0f, SIZEOF_TOP_SPEED_PERCENT, "Top Speed Percentage");
	}
	else
	{
		m_topSpeedPercent = -1.0f;
	}

	SERIALISE_BOOL(serialiser, m_HasTargetGravityScale, "Has Target Gravity Scale");
	if(m_HasTargetGravityScale || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_TargetGravityScale, 1.0f, SIZEOF_TARGET_GRAVITY_SCALE, "Target Gravity Scale");
		SERIALISE_PACKED_FLOAT(serialiser, m_StickY, 1.0f, SIZEOF_TARGET_GRAVITY_SCALE, "Cached StickY");
	}
	else
	{
		m_TargetGravityScale = 0.0f;
		m_StickY = 0.0f;
	}	

	SERIALISE_BOOL(serialiser, m_isSubCar);
	if(m_isSubCar || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, m_subCarYaw,   1.0f, SIZEOF_ANGCONTROL, "Yaw Control");
		SERIALISE_PACKED_FLOAT(serialiser, m_SubCarPitch, 1.0f, SIZEOF_ANGCONTROL, "Pitch Control");
		SERIALISE_PACKED_FLOAT(serialiser, m_SubCarDive,  1.0f, SIZEOF_ANGCONTROL, "Dive Control");
	}
	else
	{
		m_subCarYaw = m_SubCarPitch = m_SubCarDive = 0.0f;
	}

	SERIALISE_BOOL(serialiser, m_isInBurnout, "Is In Burnout");
}

bool CVehicleControlDataNode::HasDefaultState() const 
{ 
	return m_brakePedal > -0.01f && m_brakePedal < 0.01f && m_throttle > -0.01f && m_throttle < 0.01f && m_roadNodeAddress == 0 && m_kersActive == false && m_reducedSuspensionForce == false
		&& m_bringVehicleToHalt == false && m_BVTHControlVertVel == false && m_bModifiedLowriderSuspension == false && m_bAllLowriderHydraulicsRaised == false
		&& m_bPlayHydraulicsBounceSound == false && m_bPlayHydraulicsDeactivationSound == false && m_bIsClosingAnyDoor == false && m_HasTopSpeedPercentage == false && m_HasTargetGravityScale == false
		&& m_isInBurnout == false && m_bNitrousActive == false && m_bIsNitrousOverrideActive == false;
}

void CVehicleControlDataNode::SetDefaultState()
{ 
	m_brakePedal = m_throttle = 0.0f; m_roadNodeAddress = 0; m_kersActive = false; m_reducedSuspensionForce = false;
	m_bringVehicleToHalt = false; m_BVTHControlVertVel = false; m_bModifiedLowriderSuspension = false; m_bAllLowriderHydraulicsRaised = false;
	m_bPlayHydraulicsBounceSound = false; m_bPlayHydraulicsDeactivationSound = false;
	m_bIsClosingAnyDoor = false; m_HasTopSpeedPercentage = false;
	m_HasTargetGravityScale = false; m_isInBurnout = false;
	m_bNitrousActive = false; m_bIsNitrousOverrideActive = false;
}

FW_INSTANTIATE_CLASS_POOL(CVehicleAppearanceDataNode::CVehicleBadgeData, NetworkCrewEmblemMgr::MAX_NUM_EMBLEMS * CVehicleAppearanceDataNode::MAX_VEHICLE_BADGES, atHashString("CVehicleBadgeData",0xed007b64)); //the badge data is the emplacement of emblems * number of emblems per vehicle (lavalley)

void  CVehicleAppearanceDataNode::ExtractDataForSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
{
	vehicleNodeData.GetVehicleAppearanceData(*this);

	// m_nDisableExtras is in bits 1-10 but we pack it in bits 0-9
	m_disableExtras = static_cast<u32>(m_disableExtras>>1);
	m_liveryID      = static_cast<u32>(m_liveryID)+1;
	m_livery2ID     = static_cast<u32>(m_livery2ID)+1;

	m_bWindowTint = (m_windowTint != 0);

	m_bSmokeColor = ((m_smokeColorR != 255) || (m_smokeColorG != 255) || (m_smokeColorB != 255));
}

void CVehicleAppearanceDataNode::ApplyDataFromSerialising(IVehicleNodeDataAccessor &vehicleNodeData)
{
	// m_nDisableExtras is in bits 1-10 but we pack it in bits 0-9
	m_disableExtras = static_cast<u32>(m_disableExtras<<1);
	m_liveryID      = (static_cast<s32>(m_liveryID)-1);
	m_livery2ID     = (static_cast<s32>(m_livery2ID)-1);

	vehicleNodeData.SetVehicleAppearanceData(*this);
}

void SerialiseOptimizedSmallFloat(CSyncDataBase& serialiser, float& fValue, const float maxValue, const int numBits)
{
	//Most values are between -2.5 and 2.5 -- this will capture those values and allow for precise replication without funky quantization

	bool bOptimizedSize = false;

	//If the value is less than 2.5 when multiplied by 100 it will only be 250 which is less than 255 and will fit into a u8
	u32 uValue = 0;
	float fAbsValue = fabs(fValue);
	if (fAbsValue >= 0.f && fAbsValue <= 2.5f)
	{
		uValue = (u32) (fAbsValue * 100.f);
		bOptimizedSize = true;
	}

	SERIALISE_BOOL(serialiser, bOptimizedSize);
	if (!bOptimizedSize  || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_PACKED_FLOAT(serialiser, fValue, maxValue, numBits);
	}
	else
	{
		//Serialise the sign for use below
		bool bNegative = fValue < 0.f;
		SERIALISE_BOOL(serialiser, bNegative);

		SERIALISE_UNSIGNED(serialiser, uValue, 8);

		fValue = (float) uValue;
		fValue /= 100.f;

		if (bNegative)
			fValue *= -1.f;
	}
}

void SerialiseOptimized101Float(CSyncDataBase& serialiser, float& fFloat, const float maxValue, const int numBits)
{
	float fSerializeValue = fFloat;

	u32 uSerializedValue = 0;
	bool bOptimizedElement = false;
	if (fSerializeValue == -1.f)
	{
		uSerializedValue = 0;
		bOptimizedElement = true;
	}
	else if (fSerializeValue == 0.f)
	{
		uSerializedValue = 1;
		bOptimizedElement = true;
	}
	else if (fSerializeValue == 1.f)
	{
		uSerializedValue = 2;
		bOptimizedElement = true;
	}

	SERIALISE_BOOL(serialiser, bOptimizedElement);

	if (!bOptimizedElement || serialiser.GetIsMaximumSizeSerialiser())
	{
		SerialiseOptimizedSmallFloat(serialiser,fSerializeValue,maxValue,numBits);
	}
	else
	{
		//serialise 2bits for -1 0 1... then ...
		SERIALISE_UNSIGNED(serialiser, uSerializedValue, 2);
		switch(uSerializedValue)
		{
			case 0:
				fSerializeValue = -1.f;
				break;
			case 1:
				fSerializeValue = 0.f;
				break;
			case 2:
				fSerializeValue = 1.f;
				break;
			default:
				Warningf("SerialiseOptimized101Float should recieve value uSerializedValue[%u] setting fSerializedValue = 0.f",uSerializedValue);
				fSerializeValue = 0.f;
				break;
		}
	}

	fFloat = fSerializeValue;
}

void SerialiseOptimizedVector(CSyncDataBase& serialiser, Vector3& vVector, const float maxValue, const int numBits)
{
	SerialiseOptimized101Float(serialiser, vVector.x, maxValue, numBits);
	SerialiseOptimized101Float(serialiser, vVector.y, maxValue, numBits);
	SerialiseOptimized101Float(serialiser, vVector.z, maxValue, numBits);
}

void CVehicleAppearanceDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_COLOUR            = 8;
	static const unsigned int SIZEOF_ENVEFFSCALE	   = 8;
	static const unsigned int SIZEOF_EXTRAS            = VEH_LAST_EXTRA - VEH_EXTRA_1 + 1;
	static const unsigned int SIZEOF_LIVERY            = datBitsNeeded<MAX_NUM_LIVERIES>::COUNT;
	static const unsigned int SIZEOF_KIT_INDEX		    = 2;
	static const unsigned int SIZEOF_TOGGLE_MODS       = VMT_TOGGLE_NUM;
	static const unsigned int SIZEOF_PLATE_CHAR        = 7;
	static const unsigned int SIZEOF_PLATE_TEXIDX      = 32;
	//static const unsigned int SIZEOF_PHYSICAL_PLAYER_INDEX = 5;
	static const unsigned int SIZEOF_DIRTLEVEL		   = 5;
	static const unsigned int SIZEOF_VEHICLEBADGEBONEINDEX = 10;
	static const unsigned int SIZEOF_VEHICLEBADGEOFFSET = 10;
	static const unsigned int SIZEOF_VEHICLEBADGESIZE   = 16;
	static const unsigned int SIZEOF_BADGE_ALPHA   = 8;

	// this information comes from platform:/data/carcols.#mt (there are VWT_MAX different wheel types)
	static const unsigned int SIZEOF_WHEEL_TYPE_INDEX  = 4;

	static const unsigned int SIZEOF_HORNTYPE		   = 32;

	bool bHasExtras = m_bodyDirtLevel != 0 || m_disableExtras != 0 || m_liveryID != 0 || m_livery2ID != 0;

	// basic appearance data
	SERIALISE_UNSIGNED(serialiser, m_bodyColour1,   SIZEOF_COLOUR,    "Body Colour 1");
	SERIALISE_UNSIGNED(serialiser, m_bodyColour2,   SIZEOF_COLOUR,    "Body Colour 2");
	SERIALISE_UNSIGNED(serialiser, m_bodyColour3,   SIZEOF_COLOUR,    "Body Colour 3");
	SERIALISE_UNSIGNED(serialiser, m_bodyColour4,   SIZEOF_COLOUR,    "Body Colour 4");
	SERIALISE_UNSIGNED(serialiser, m_bodyColour5,   SIZEOF_COLOUR,    "Body Colour 5");
	SERIALISE_UNSIGNED(serialiser, m_bodyColour6,   SIZEOF_COLOUR,    "Body Colour 6");

	SERIALISE_BOOL(serialiser, m_customPrimaryColor);
	if (m_customPrimaryColor || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_customPrimaryR,   SIZEOF_COLOUR,    "Custom Primary Color R");
		SERIALISE_UNSIGNED(serialiser, m_customPrimaryG,   SIZEOF_COLOUR,    "Custom Primary Color G");
		SERIALISE_UNSIGNED(serialiser, m_customPrimaryB,   SIZEOF_COLOUR,    "Custom Primary Color B");
	}

	SERIALISE_BOOL(serialiser, m_customSecondaryColor);
	if (m_customSecondaryColor || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_customSecondaryR,   SIZEOF_COLOUR,    "Custom Secondary Color R");
		SERIALISE_UNSIGNED(serialiser, m_customSecondaryG,   SIZEOF_COLOUR,    "Custom Secondary Color G");
		SERIALISE_UNSIGNED(serialiser, m_customSecondaryB,   SIZEOF_COLOUR,    "Custom Secondary Color B");
	}

	SERIALISE_UNSIGNED(serialiser, m_envEffScale,   SIZEOF_ENVEFFSCALE,    "Env Eff Scale");

	SERIALISE_BOOL(serialiser, bHasExtras);

	if (bHasExtras || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_bodyDirtLevel, SIZEOF_DIRTLEVEL, "Body Dirt Level");
		SERIALISE_UNSIGNED(serialiser, m_disableExtras, SIZEOF_EXTRAS,    "Extras");

		SERIALISE_BOOL(serialiser, m_hasLiveryID);
		if (m_hasLiveryID || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_liveryID,      SIZEOF_LIVERY,    "Livery");
		}
		else
		{
			m_liveryID = 0;
		}

		SERIALISE_BOOL(serialiser, m_hasLivery2ID);
		if (m_hasLivery2ID || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_livery2ID,      SIZEOF_LIVERY,    "Livery2");
		}
		else
		{
			m_livery2ID = 0;
		}
	}
	else
	{
		m_bodyDirtLevel = 0;
		m_disableExtras = 0;
		m_liveryID = 0;
		m_hasLiveryID = false;
		m_livery2ID = 0;
		m_hasLivery2ID = false;
	}

	// mods
	SERIALISE_UNSIGNED(serialiser, m_kitIndex, SIZEOF_KIT_INDEX, "Mod Kit Index");

	if(m_kitIndex != 0 || serialiser.GetIsMaximumSizeSerialiser())
	{
		for(unsigned i = 0; i < NUM_KIT_MOD_SYNC_VARIABLES; i++)
		{
			bool bHasKitMod = m_allKitMods[i] != 0;

			SERIALISE_BOOL(serialiser, bHasKitMod, "Has Kit Mods");
			if(bHasKitMod || serialiser.GetIsMaximumSizeSerialiser())
			{
				char stringIndex[11];
				formatf(stringIndex, "Kit Mods %d", i);
				SERIALISE_UNSIGNED(serialiser, m_allKitMods[i], SIZEOF_KIT_MODS_U32, stringIndex);
			}
			else
			{
				m_allKitMods[i] = 0;
			}
		}

		bool bHasToggleMods = (m_toggleMods != 0);
		
		SERIALISE_BOOL(serialiser, bHasToggleMods, "Has Toggle Mods");
		if(bHasToggleMods || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_toggleMods, SIZEOF_TOGGLE_MODS, "Toggle Mods");
		}
		else
		{
			m_toggleMods = 0;
		}

		SERIALISE_UNSIGNED(serialiser, m_wheelMod, SIZEOF_WHEEL_MOD_INDEX, "Wheel Mod");
		SERIALISE_UNSIGNED(serialiser, m_wheelType, SIZEOF_WHEEL_TYPE_INDEX, "Wheel Type");
		
		SERIALISE_BOOL(serialiser, m_hasDifferentRearWheel, "Has Different Rear Wheel");
		if(m_hasDifferentRearWheel || serialiser.GetIsMaximumSizeSerialiser())
		{
			SERIALISE_UNSIGNED(serialiser, m_rearWheelMod, SIZEOF_WHEEL_MOD_INDEX, "Rear Wheel Mod");
		}
		else
		{
			m_rearWheelMod = 0;
		}

		SERIALISE_BOOL(serialiser, m_wheelVariation0, "Wheel Variation0");
		SERIALISE_BOOL(serialiser, m_wheelVariation1, "Wheel Variation1");
	}
	else
	{
		for(int i = 0; i < NUM_KIT_MOD_SYNC_VARIABLES; i++)
		{
			m_allKitMods[i] = 0;
		}

		m_toggleMods		= 0;
		m_wheelMod			= 0;
		m_rearWheelMod      = 0;
		m_wheelType			= 255;
		m_wheelVariation0	= false;
		m_wheelVariation1	= false;
	}

	SERIALISE_BOOL(serialiser, m_bWindowTint);
	if (m_bWindowTint || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_windowTint,   SIZEOF_COLOUR,    "Window Tint");
	}
	else
	{
		m_windowTint = 0;
	}

	SERIALISE_BOOL(serialiser, m_bSmokeColor);
	if (m_bSmokeColor || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_smokeColorR,   SIZEOF_COLOUR,    "Smoke Color R");
		SERIALISE_UNSIGNED(serialiser, m_smokeColorG,   SIZEOF_COLOUR,    "Smoke Color G");
		SERIALISE_UNSIGNED(serialiser, m_smokeColorB,   SIZEOF_COLOUR,    "Smoke Color B");
	}

	// licence plate
    bool hasLicensePlate = m_licencePlate[0] != '\0';

    SERIALISE_BOOL(serialiser, hasLicensePlate);

	if(hasLicensePlate || serialiser.GetIsMaximumSizeSerialiser())
	{
		for(u32 plate = 0; plate < SIZEOF_LICENCE_PLATE; plate++)
		{
			SERIALISE_UNSIGNED(serialiser, m_licencePlate[plate], SIZEOF_PLATE_CHAR, "Licence plate char");
		}
	}
	SERIALISE_UNSIGNED(serialiser, m_LicencePlateTexIndex, SIZEOF_PLATE_TEXIDX, "Licence plate texture index");

	SERIALISE_UNSIGNED(serialiser, m_horntype, SIZEOF_HORNTYPE, "Horn Type Hash");

	SERIALISE_BOOL(serialiser, m_VehicleBadge, "Has Vehicle Badge");
	if (m_VehicleBadge || serialiser.GetIsMaximumSizeSerialiser())
	{
		m_VehicleBadgeDesc.Serialise(serialiser);

		for(int i = 0; i < MAX_VEHICLE_BADGES; ++i)
		{
			SERIALISE_BOOL(serialiser, m_bVehicleBadgeData[i], "Has Vehicle Badge i");
			if (m_bVehicleBadgeData[i] || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_INTEGER(serialiser, m_VehicleBadgeData[i].m_boneIndex, SIZEOF_VEHICLEBADGEBONEINDEX, "Vehicle Badge Bone Index");
				
				u32 alpha = m_VehicleBadgeData[i].m_alpha;
				SERIALISE_UNSIGNED(serialiser, alpha, SIZEOF_BADGE_ALPHA, "Vehicle Badge Alpha Value");
				m_VehicleBadgeData[i].m_alpha = (u8)alpha;

				SerialiseOptimizedVector(serialiser, m_VehicleBadgeData[i].m_offsetPos, 10.0f, SIZEOF_VEHICLEBADGEOFFSET); //most of the values here are -1 0 1 - optimize for send - but also to replicate exactly to remote

				SerialiseOptimizedVector(serialiser, m_VehicleBadgeData[i].m_dir, 1.01f, SIZEOF_VEHICLEBADGEOFFSET); //most of the values here are -1 0 1 - optimize for send - but also to replicate exactly to remote

				SerialiseOptimizedVector(serialiser, m_VehicleBadgeData[i].m_side, 1.01f, SIZEOF_VEHICLEBADGEOFFSET); //most of the values here are -1 0 1 - optimize for send - but also to replicate exactly to remote
				
				SerialiseOptimizedSmallFloat(serialiser, m_VehicleBadgeData[i].m_size, 10.0f, SIZEOF_VEHICLEBADGESIZE); //most of the values here are tenths 0.1 0.5 etc..., optimized to send - but also to replicate exactly the size to remote so it looks right

				m_VehicleBadgeData[i].m_badgeIndex = i;
			}
		}
	}
	else
	{
		for(int i = 0; i < MAX_VEHICLE_BADGES; ++i)
		{
			m_bVehicleBadgeData[i] = false;
			m_VehicleBadgeData[i].m_badgeIndex = i;
		}
	}

	SERIALISE_BOOL(serialiser, m_neonOn, "Neon On");
	if (m_neonOn || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_neonColorR, SIZEOF_COLOUR, "Neon Color R");
		SERIALISE_UNSIGNED(serialiser, m_neonColorG, SIZEOF_COLOUR, "Neon Color G");
		SERIALISE_UNSIGNED(serialiser, m_neonColorB, SIZEOF_COLOUR, "Neon Color B");

		SERIALISE_BOOL(serialiser, m_neonLOn, "Neon Left On");
		SERIALISE_BOOL(serialiser, m_neonROn, "Neon Right On");
		SERIALISE_BOOL(serialiser, m_neonFOn, "Neon Front On");
		SERIALISE_BOOL(serialiser, m_neonBOn, "Neon Back On");
		SERIALISE_BOOL(serialiser, m_neonSuppressed, "Neon Suppressed");
	}
}

//-----

void  CVehicleDamageStatusDataNode::ExtractDataForSerialising(IVehicleNodeDataAccessor &data)
{
	data.GetVehicleDamageStatusData(*this);

	m_hasLightsBroken = false;

	for(u32 light = 0; light < SIZEOF_LIGHT_DAMAGE; light++)
	{
		if (m_lightsBroken[light])
		{
			m_hasLightsBroken = true;
			break;
		}
	}

	m_hasWindowsBroken = false;

	for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
	{
		if (m_windowsBroken[window])
		{
			m_hasWindowsBroken = true;
			break;
		}
	}

	m_hasSirensBroken = false;

	for(u32 siren = 0; siren < SIZEOF_SIREN_DAMAGE; siren++)
	{
		if (m_sirensBroken[siren])
		{
			m_hasSirensBroken = true;
			break;
		}
	}
}

void CVehicleDamageStatusDataNode::ApplyDataFromSerialising(IVehicleNodeDataAccessor &data)
{
	if (!m_hasLightsBroken)
	{
		for(u32 light = 0; light < SIZEOF_LIGHT_DAMAGE; light++)
		{
			m_lightsBroken[light] = false;
		}
	}

	if (!m_hasWindowsBroken)
	{
		for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
		{
			m_windowsBroken[window] = false;
		}
	}

	if (!m_hasSirensBroken)
	{
		for(u32 siren = 0; siren < SIZEOF_SIREN_DAMAGE; siren++)
		{
			m_sirensBroken[siren] = false;
		}
	}

	data.SetVehicleDamageStatusData(*this);
}

void CVehicleDamageStatusDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_DEFORMATION       = 2;
	static const unsigned int SIZEOF_BB_STATE		   = 2;
	static const unsigned int SIZEOF_SINGLE_WEAPON_IMPACT_POINT = 8;

	// vehicle deformation
	SERIALISE_BOOL(serialiser, m_hasDeformationDamage);

	if (m_hasDeformationDamage || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_frontLeftDamageLevel, SIZEOF_DEFORMATION, "Front Left Damage Level");
		SERIALISE_UNSIGNED(serialiser, m_frontRightDamageLevel, SIZEOF_DEFORMATION, "Front Right Damage Level");
		SERIALISE_UNSIGNED(serialiser, m_middleLeftDamageLevel, SIZEOF_DEFORMATION, "Middle Left Damage Level");
		SERIALISE_UNSIGNED(serialiser, m_middleRightDamageLevel, SIZEOF_DEFORMATION, "Middle Right Damage Level");
		SERIALISE_UNSIGNED(serialiser, m_rearLeftDamageLevel, SIZEOF_DEFORMATION, "Rear Left Damage Level");
		SERIALISE_UNSIGNED(serialiser, m_rearRightDamageLevel, SIZEOF_DEFORMATION, "Rear RightDamage Level");
	}

	SERIALISE_BOOL(serialiser, m_weaponImpactPointLocationSet, "m_weaponImpactPointLocationSet");
	if (m_weaponImpactPointLocationSet || serialiser.GetIsMaximumSizeSerialiser())
	{
		for(int i=0; i < NUM_NETWORK_DAMAGE_DIRECTIONS; i++)
		{
			SERIALISE_UNSIGNED(serialiser, m_weaponImpactPointLocationCounts[i], SIZEOF_SINGLE_WEAPON_IMPACT_POINT);
		}
	}

	SERIALISE_BOOL(serialiser, m_hasBrokenBouncing);
	if (m_hasBrokenBouncing || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_frontBumperState, SIZEOF_BB_STATE, "Front Bumper State");
		SERIALISE_UNSIGNED(serialiser, m_rearBumperState, SIZEOF_BB_STATE, "Rear Bumper State");
	}
	else
	{
		m_frontBumperState = 0;
		m_rearBumperState = 0;
	}

	//vehicle lights - broken status
	SERIALISE_BOOL(serialiser, m_hasLightsBroken);

	if (m_hasLightsBroken || serialiser.GetIsMaximumSizeSerialiser())
	{
		for(u32 light = 0; light < SIZEOF_LIGHT_DAMAGE; light++)
		{
			SERIALISE_BOOL(serialiser, m_lightsBroken[light], "Light Broken");
		}
	}

	// vehicle windows
	SERIALISE_BOOL(serialiser, m_hasWindowsBroken);

	if (m_hasWindowsBroken || serialiser.GetIsMaximumSizeSerialiser())
	{
		for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
		{
			SERIALISE_BOOL(serialiser, m_windowsBroken[window], "Window Broken");
		}
	}

	SERIALISE_BOOL(serialiser, m_hasArmouredGlass);
	
	if (m_hasArmouredGlass || serialiser.GetIsMaximumSizeSerialiser())
	{
		static const unsigned SIZEOF_MAX_ARMOURED_GLASS_HEALTH = 10;

		for(u32 window = 0; window < SIZEOF_WINDOW_DAMAGE; window++)
		{
			SERIALISE_PACKED_FLOAT(serialiser, m_armouredWindowsHealth[window], CVehicleGlassComponent::MAX_ARMOURED_GLASS_HEALTH, SIZEOF_MAX_ARMOURED_GLASS_HEALTH, "Armoured window health");

			if (m_armouredWindowsHealth[window] != CVehicleGlassComponent::MAX_ARMOURED_GLASS_HEALTH || serialiser.GetIsMaximumSizeSerialiser())
			{
				SERIALISE_INTEGER(serialiser, m_armouredPenetrationDecalsCount[window], 8, "Armoured window decal count");
			}			
			else
			{
				m_armouredPenetrationDecalsCount[window] = 0;
			}
		}
	}

	// siren
	SERIALISE_BOOL(serialiser, m_hasSirensBroken);

	if(m_hasSirensBroken || serialiser.GetIsMaximumSizeSerialiser())
	{
		for(u32 siren = 0; siren < SIZEOF_SIREN_DAMAGE; siren++)
		{
			SERIALISE_BOOL(serialiser, m_sirensBroken[siren], "Siren Broken");
		}
	}
}

//-----

void CVehicleTaskDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_TASK_DATA = datBitsNeeded<MAX_VEHICLE_TASK_DATA_SIZE>::COUNT;

	SERIALISE_VEHICLE_TASKTYPE(serialiser, m_taskType, "Task Type");
	SERIALISE_UNSIGNED(serialiser, m_taskDataSize, SIZEOF_TASK_DATA);

	if (serialiser.GetIsMaximumSizeSerialiser())
	{
		m_taskDataSize = MAX_VEHICLE_TASK_DATA_SIZE;
	}

	if(m_taskDataSize > 0)
	{
		SERIALISE_DATABLOCK(serialiser, m_taskData, m_taskDataSize);

		if (m_taskDataLogFunction)
		{
			(*m_taskDataLogFunction)(serialiser.GetLog(), m_taskType, m_taskData, m_taskDataSize);
		}
	}	
}

void CVehicleComponentReservationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_COMPONENTS = 5;

	SERIALISE_UNSIGNED(serialiser, m_numVehicleComponents, SIZEOF_COMPONENTS);
	SERIALISE_BOOL(serialiser, m_hasReservations);

	// this is the maximum allowable value for the number of wheels
	if(serialiser.GetIsMaximumSizeSerialiser())
	{
		m_numVehicleComponents = CComponentReservation::MAX_NUM_COMPONENTS;
		m_hasReservations      = true;
	}

	if(m_hasReservations)
	{
		for(u32 componentIndex = 0; componentIndex < m_numVehicleComponents; componentIndex++)
		{
			SERIALISE_OBJECTID(serialiser, m_componentReservations[componentIndex], "Ped Using Component");
		}
	}
}

bool CVehicleComponentReservationDataNode::HasDefaultState() const 
{ 
	return m_hasReservations == false; 
}

void CVehicleComponentReservationDataNode::SetDefaultState() 
{ 
	m_hasReservations = false;    

	for(u32 componentIndex = 0; componentIndex < m_numVehicleComponents; componentIndex++)
	{
		m_componentReservations[componentIndex] = NETWORK_INVALID_OBJECT_ID;
	}
}

void CVehicleProximityMigrationDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned SIZEOF_MAX_VEHICLE_SEATS = datBitsNeeded<MAX_VEHICLE_SEATS>::COUNT;
	//static const unsigned int SIZEOF_TASK_TYPE		   = 7;
	static const unsigned SIZEOF_STATUS            = 3;
    static const unsigned SIZEOF_LAST_DRIVER_TIME  = 32;
	static const unsigned SIZEOF_MIGRATIONDATASIZE = datBitsNeeded<MAX_TASK_MIGRATION_DATA_SIZE>::COUNT;
	static const unsigned SIZEOF_RESPOT_COUNTER = 16;

	SERIALISE_UNSIGNED(serialiser, m_maxOccupants, SIZEOF_MAX_VEHICLE_SEATS, "Max Occupants");

	for(u32 index = 0; index < m_maxOccupants; index++)
	{
		SERIALISE_BOOL(serialiser, m_hasOccupant[index]);

		if(m_hasOccupant[index])
		{
			SERIALISE_OBJECTID(serialiser, m_occupantID[index], "Occupant");
		}
	}

	SERIALISE_BOOL(serialiser, m_hasPopType);
	if(m_hasPopType || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_VEHICLE_POP_TYPE(serialiser, m_PopType,  "Pop type");
	}

	SERIALISE_UNSIGNED(serialiser, m_status,         SIZEOF_STATUS,           "Status");
    SERIALISE_UNSIGNED(serialiser, m_lastDriverTime, SIZEOF_LAST_DRIVER_TIME, "Last Driver Time");

	SERIALISE_BOOL(serialiser, m_isMoving);

	if(m_isMoving || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_POSITION(serialiser, m_position, "Position");
		SERIALISE_VELOCITY(serialiser, m_packedVelocityX, m_packedVelocityY, m_packedVelocityZ, SIZEOF_VELOCITY, "Velocity");
	}

    static const float    MAX_SPEED_MULTIPLIER    = 3.0f;
    static const unsigned SIZEOF_SPEED_MULTIPLIER = 10;

    SERIALISE_PACKED_UNSIGNED_FLOAT(serialiser, m_SpeedMultiplier, MAX_SPEED_MULTIPLIER, SIZEOF_SPEED_MULTIPLIER, "Speed Multiplier");
	SERIALISE_BOOL(serialiser, m_hasTaskData);

	if (m_hasTaskData || serialiser.GetIsMaximumSizeSerialiser())
	{
		if (serialiser.GetIsMaximumSizeSerialiser())
		{
			m_taskMigrationDataSize = MAX_TASK_MIGRATION_DATA_SIZE;
		}

		SERIALISE_VEHICLE_TASKTYPE(serialiser, m_taskType, "Task type");
		SERIALISE_UNSIGNED(serialiser, m_taskMigrationDataSize, SIZEOF_MIGRATIONDATASIZE);

		if(m_taskMigrationDataSize > 0)
		{
			SERIALISE_DATABLOCK(serialiser, m_taskMigrationData, m_taskMigrationDataSize);

			if (m_taskDataLogFunction)
			{
				(*m_taskDataLogFunction)(serialiser.GetLog(), m_taskType, m_taskMigrationData, m_taskMigrationDataSize);
			}
		}	
	}

	bool hasRespotCounter = m_RespotCounter > 0;
	SERIALISE_BOOL(serialiser, hasRespotCounter, "Has Respot Counter");
	if(hasRespotCounter || serialiser.GetIsMaximumSizeSerialiser())
	{
		SERIALISE_UNSIGNED(serialiser, m_RespotCounter, SIZEOF_RESPOT_COUNTER, "Respot Counter");
	}
	else
	{
		m_RespotCounter = 0;
	}
}

CVehicleGadgetDataNode::fnGadgetDataLogger CVehicleGadgetDataNode::m_gadgetLogFunction = 0;

void CVehicleGadgetDataNode::Serialise(CSyncDataBase& serialiser)
{
	static const unsigned int SIZEOF_NUM_GADGETS = datBitsNeeded<MAX_NUM_GADGETS>::COUNT;
	static const unsigned int SIZEOF_GADGET_TYPE = datBitsNeeded<VGT_NUM_NETWORKED_TYPES-1>::COUNT;

    SERIALISE_BOOL(serialiser, m_IsAttachedTrailer);

    if(m_IsAttachedTrailer || serialiser.GetIsMaximumSizeSerialiser())
    {
        static const unsigned SIZEOF_PARENT_VEHICLE_OFFSET = 14;
		static const float MAXSIZE_PARENT_VEHICLE_OFFSET = 24.0f;
        SERIALISE_PACKED_FLOAT(serialiser, m_OffsetFromParentVehicle.x, MAXSIZE_PARENT_VEHICLE_OFFSET, SIZEOF_PARENT_VEHICLE_OFFSET, "Parent Vehicle Offset X");
		SERIALISE_PACKED_FLOAT(serialiser, m_OffsetFromParentVehicle.y, MAXSIZE_PARENT_VEHICLE_OFFSET, SIZEOF_PARENT_VEHICLE_OFFSET, "Parent Vehicle Offset Y");
		SERIALISE_PACKED_FLOAT(serialiser, m_OffsetFromParentVehicle.z, MAXSIZE_PARENT_VEHICLE_OFFSET, SIZEOF_PARENT_VEHICLE_OFFSET, "Parent Vehicle Offset Z");
    }
    else
    {
        m_IsAttachedTrailer       = false;
        m_OffsetFromParentVehicle = VEC3_ZERO;
    }
	
    if (serialiser.GetIsMaximumSizeSerialiser())
	{
		m_NumGadgets = MAX_NUM_GADGETS;
	}

	SERIALISE_UNSIGNED(serialiser, m_NumGadgets, SIZEOF_NUM_GADGETS);

	for (u32 i=0; i<m_NumGadgets; i++)
	{
		SERIALISE_UNSIGNED(serialiser, m_GadgetData[i].Type, SIZEOF_GADGET_TYPE);

		u32 dataSize = serialiser.GetIsMaximumSizeSerialiser() ? IVehicleNodeDataAccessor::MAX_VEHICLE_GADGET_DATA_SIZE : CNetObjVehicle::GetSizeOfVehicleGadgetData((eVehicleGadgetType)m_GadgetData[i].Type);

		if (dataSize > 0)
		{
			SERIALISE_DATABLOCK(serialiser, m_GadgetData[i].Data, dataSize);
		}

		if (m_gadgetLogFunction)
		{
			(*m_gadgetLogFunction)(serialiser.GetLog(), m_GadgetData[i]);
		}
	}
}



