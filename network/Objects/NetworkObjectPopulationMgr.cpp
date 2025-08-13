//
// name:        NetworkObjectPopulationMgr.h
// description: Manages levels of network objects of the different types in network games
// written by:  Daniel Yelland
//

// --- Include Files ------------------------------------------------------------
#include "NetworkObjectPopulationMgr.h"

#include "grcore/debugdraw.h"
#include "fwnet/netchannel.h"
#include "fwnet/netlogutils.h"
#include "fwnet/netutils.h"
#include "fwscript/scriptinterface.h"

#include "Peds/Ped.h"
#include "Network/Network.h"
#include "network/Bandwidth/NetworkBandwidthManager.h"
#include "Network/Objects/Entities/NetObjPlayer.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "Network/Objects/NetworkObjectMgr.h"
#include "Network/Players/NetworkPlayerMgr.h"
#include "Network/General/NetworkUtil.h"
#include "Network/Sessions/NetworkSession.h"

#include "Scene/World/GameWorld.h"
#include "script/handlers/GameScriptHandler.h"
#include "script/handlers/GameScriptIDs.h"
#include "Script/Script.h"
#include "Vehicles/VehiclePopulation.h"

NETWORK_OPTIMISATIONS()

CNetworkObjectPopulationMgr::TargetLevels CNetworkObjectPopulationMgr::ms_PopulationTargetLevels;
CNetworkObjectPopulationMgr::TargetLevels CNetworkObjectPopulationMgr::ms_CurrentBandwidthTargetLevels;
CNetworkObjectPopulationMgr::TargetLevels CNetworkObjectPopulationMgr::ms_MaxBandwidthTargetLevels;
CNetworkObjectPopulationMgr::TargetLevels CNetworkObjectPopulationMgr::ms_BalancingTargetLevels;
u32      CNetworkObjectPopulationMgr::ms_LastTimeBandwidthLevelsIncreased = 0;
bool     CNetworkObjectPopulationMgr::ms_ProcessLocalPopulationLimits     = true;
unsigned CNetworkObjectPopulationMgr::ms_CurrentExternalReservation       = CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID;
CNetworkObjectPopulationMgr::ExternalReservation CNetworkObjectPopulationMgr::ms_ExternalReservations[CNetworkObjectPopulationMgr::MAX_EXTERNAL_RESERVATIONS];
ObjectId  CNetworkObjectPopulationMgr::ms_TransitionalPopulation[MAX_NUM_TRANSITIONAL_OBJECTS];
unsigned  CNetworkObjectPopulationMgr::ms_TransitionalPopulationTimer = 0;
unsigned  CNetworkObjectPopulationMgr::ms_NumTransitionalObjects = 0;
int CNetworkObjectPopulationMgr::ms_TotalNumVehicles = 0;
CNetworkObjectPopulationMgr::eVehicleGenerationContext CNetworkObjectPopulationMgr::ms_eVehicleGenerationContext = CNetworkObjectPopulationMgr::VGT_AmbientPopulation;
CNetworkObjectPopulationMgr::ePedGenerationContext CNetworkObjectPopulationMgr::ms_ePedGenerationContext = CNetworkObjectPopulationMgr::PGT_Default;

#if !__NO_OUTPUT
CNetworkObjectPopulationMgr::PopulationFailTypes CNetworkObjectPopulationMgr::ms_LastFailType = CNetworkObjectPopulationMgr::PFT_SUCCESS;
#endif // !__NO_OUTPUT

int CNetworkObjectPopulationMgr::ms_CachedNumLocalVehicles = 0;

bank_float OBJECT_SCALED_PERCENTAGE_DISTANCE   = 200.0f;
bank_float PED_SCALED_PERCENTAGE_DISTANCE      = 250.0f;
bank_float VEHICLE_SCALED_PERCENTAGE_DISTANCE  = 400.0f;
bank_float OBJECT_FULL_PERCENTAGE_DISTANCE     = 80.0f;
bank_float PED_FULL_PERCENTAGE_DISTANCE        = 120.0f;
bank_float VEHICLE_FULL_PERCENTAGE_DISTANCE    = 250.0f;

// time interval to wait before allowing ambient objects to be created after a local object has been dumped
bank_u32 DUMPED_AMBIENT_ENTITY_TIMER             = 30000;
// time interval to allow ambient objects to be created after the dumped timer has completed
bank_u32 DUMPED_AMBIENT_ENTITY_INCREASE_INTERVAL = 2500;

#if __BANK
CNetworkObjectPopulationMgr::ObjectManagementDebugSettings CNetworkObjectPopulationMgr::ms_ObjectManagementDebugSettings;
CNetworkObjectPopulationMgr::PopulationFailureReasons CNetworkObjectPopulationMgr::ms_LastPopulationFailureReasons;
CNetworkObjectPopulationMgr::PopulationFailureReasons CNetworkObjectPopulationMgr::ms_PopulationFailureReasons;
static bool s_displayObjectPopulationFailureReasons = false;
static bool s_displayObjectTargetLevelsDebugDisplay = false;

static CNetworkObjectPopulationMgr::PercentageShare s_LastPlayerPercentageShares[MAX_NUM_PHYSICAL_PLAYERS];

//
// CNetworkObjectPopulationMgr::ObjectManagementDebugSettings
//
CNetworkObjectPopulationMgr::ObjectManagementDebugSettings::ObjectManagementDebugSettings() :
m_UseDebugSettings        (false),
m_MaxLocalObjects         (MAX_NUM_NETOBJOBJECTS   / MAX_NUM_PHYSICAL_PLAYERS),
m_MaxLocalPeds            (MAX_NUM_NETOBJPEDS      / MAX_NUM_PHYSICAL_PLAYERS),
m_MaxLocalVehicles        (MAX_NUM_NETOBJVEHICLES  / MAX_NUM_PHYSICAL_PLAYERS),
m_MaxTotalObjects         (MAX_NUM_NETOBJOBJECTS),
m_MaxTotalPeds            (MAX_NUM_NETOBJPEDS),
m_MaxTotalVehicles        (MAX_NUM_NETOBJVEHICLES)
{
}

//
// CNetworkObjectPopulationMgr::ObjectManagementDebugSettings
//
CNetworkObjectPopulationMgr::PopulationFailureReasons::PopulationFailureReasons() :
m_NumCanRegisterPedFails(0)
, m_NumCanRegisterLocalPedFails(0)
, m_NumTooManyTotalPedFails(0)
, m_NumTooManyLocalPedFails(0)
, m_NumCanRegisterVehicleFails(0)
, m_NumCanRegisterLocalVehicleFails(0)
, m_NumTooManyTotalVehicleFails(0)
, m_NumTooManyLocalVehicleFails(0)
, m_NumCanRegisterObjectsFails(0)
, m_NumCanRegisterObjectsPedFails(0)
, m_NumCanRegisterObjectsVehicleFails(0)
, m_NumCanRegisterObjectsObjectFails(0)
, m_NumNoIDFails(0)
, m_NumTooManyObjectsFails(0)
, m_NumDumpTimerFails(0)
{
}

void CNetworkObjectPopulationMgr::PopulationFailureReasons::Reset()
{
    m_NumCanRegisterPedFails            = 0;
    m_NumCanRegisterLocalPedFails       = 0;
    m_NumTooManyTotalPedFails           = 0;
    m_NumTooManyLocalPedFails           = 0;
    m_NumCanRegisterVehicleFails        = 0;
    m_NumCanRegisterLocalVehicleFails   = 0;
    m_NumTooManyTotalVehicleFails       = 0;
    m_NumTooManyLocalVehicleFails       = 0;
    m_NumCanRegisterObjectsFails        = 0;
    m_NumCanRegisterObjectsPedFails     = 0;
    m_NumCanRegisterObjectsVehicleFails = 0;
    m_NumCanRegisterObjectsObjectFails  = 0;
    m_NumNoIDFails                      = 0;
    m_NumTooManyObjectsFails            = 0;
    m_NumDumpTimerFails                 = 0;
}

//
// CNetworkObjectPopulationMgr
//

void CNetworkObjectPopulationMgr::AddDebugWidgets()
{
    bkBank *bank = BANKMGR.FindBank("Network");

    if(gnetVerifyf(bank, "Unable to find network bank!"))
    {
        bank->AddToggle("Display object population failure reasons",  &s_displayObjectPopulationFailureReasons);
        bank->AddToggle("Display object target levels debug display", &s_displayObjectTargetLevelsDebugDisplay);
#if __DEV
		bank->AddSlider("Object   Scaled Percentage Distance",   &OBJECT_FULL_PERCENTAGE_DISTANCE,     0.0f, 200.0f,  1.0f);
        bank->AddSlider("Object   Full   Percentage Distance",   &OBJECT_SCALED_PERCENTAGE_DISTANCE,   0.0f, 200.0f,  1.0f);
        bank->AddSlider("Ped      Scaled Percentage Distance",   &PED_SCALED_PERCENTAGE_DISTANCE,      0.0f, 300.0f,  1.0f);
        bank->AddSlider("Ped      Full   Percentage Distance",   &PED_FULL_PERCENTAGE_DISTANCE,        0.0f, 200.0f,  1.0f);
        bank->AddSlider("Vehicle  Scaled Percentage Distance",   &VEHICLE_SCALED_PERCENTAGE_DISTANCE,  0.0f, 500.0f,  1.0f);
        bank->AddSlider("Vehicle  Full   Percentage Distance",   &VEHICLE_FULL_PERCENTAGE_DISTANCE,    0.0f, 300.0f,  1.0f);
#endif

        bank->AddToggle("Use Debug Settings",   &ms_ObjectManagementDebugSettings.m_UseDebugSettings);
        bank->AddSlider("Max Local Objects",    &ms_ObjectManagementDebugSettings.m_MaxLocalObjects,   0, MAX_NUM_NETOBJOBJECTS,   1);
        bank->AddSlider("Max Local Peds",       &ms_ObjectManagementDebugSettings.m_MaxLocalPeds,      0, MAX_NUM_NETOBJPEDS,      1);
        bank->AddSlider("Max Local Vehicles",   &ms_ObjectManagementDebugSettings.m_MaxLocalVehicles,  0, MAX_NUM_NETOBJVEHICLES,  1);
        bank->AddSlider("Max Total Objects",    &ms_ObjectManagementDebugSettings.m_MaxTotalObjects,   0, MAX_NUM_NETOBJOBJECTS,   1);
        bank->AddSlider("Max Total Peds",       &ms_ObjectManagementDebugSettings.m_MaxTotalPeds,      0, MAX_NUM_NETOBJPEDS,      1);
        bank->AddSlider("Max Total Vehicles",   &ms_ObjectManagementDebugSettings.m_MaxTotalVehicles,  0, MAX_NUM_NETOBJVEHICLES,  1);
    }
}

void CNetworkObjectPopulationMgr::DisplayDebugText()
{
    if(s_displayObjectPopulationFailureReasons)
    {
        unsigned numCanRegisterPedFails            = ms_LastPopulationFailureReasons.m_NumCanRegisterPedFails;
        unsigned numCanRegisterLocalPedFails       = ms_LastPopulationFailureReasons.m_NumCanRegisterLocalPedFails;
        unsigned numTooManyTotalPedFails           = ms_LastPopulationFailureReasons.m_NumTooManyTotalPedFails;
        unsigned numTooManyLocalPedFails           = ms_LastPopulationFailureReasons.m_NumTooManyLocalPedFails;

        unsigned numCanRegisterVehicleFails        = ms_LastPopulationFailureReasons.m_NumCanRegisterVehicleFails;
        unsigned numCanRegisterLocalVehicleFails   = ms_LastPopulationFailureReasons.m_NumCanRegisterLocalVehicleFails;
        unsigned numTooManyTotalVehicleFails       = ms_LastPopulationFailureReasons.m_NumTooManyTotalVehicleFails;
        unsigned numTooManyLocalVehicleFails       = ms_LastPopulationFailureReasons.m_NumTooManyLocalVehicleFails;

        unsigned numCanRegisterObjectsFails        = ms_LastPopulationFailureReasons.m_NumCanRegisterObjectsFails;
        unsigned numCanRegisterObjectsPedFails     = ms_LastPopulationFailureReasons.m_NumCanRegisterObjectsPedFails;
        unsigned numCanRegisterObjectsVehicleFails = ms_LastPopulationFailureReasons.m_NumCanRegisterObjectsVehicleFails;
        unsigned numCanRegisterObjectsObjectFails  = ms_LastPopulationFailureReasons.m_NumCanRegisterObjectsObjectFails;

        unsigned numNoIDFails                      = ms_LastPopulationFailureReasons.m_NumNoIDFails;
        unsigned numTooManyObjectsFails            = ms_LastPopulationFailureReasons.m_NumTooManyObjectsFails;
        unsigned numDumpTimerFails                 = ms_LastPopulationFailureReasons.m_NumDumpTimerFails;

        int	numAmbientVehicles    = CVehiclePopulation::ms_numRandomCars;
        int ambientVehicleDeficit = static_cast<int>(CVehiclePopulation::GetDesiredNumberAmbientVehicles()) - numAmbientVehicles;

        grcDebugDraw::AddDebugOutput("CanRegisterObject(PED)         %d", numCanRegisterPedFails);
        grcDebugDraw::AddDebugOutput("CanRegisterLocalObjectsOfType: %d -> (TooManyTotal:%d, TooManyLocal:%d)", numCanRegisterLocalPedFails, numTooManyTotalPedFails, numTooManyLocalPedFails);
        grcDebugDraw::AddDebugOutput("CanRegisterObject(VEHICLE):    %d", numCanRegisterVehicleFails);
        grcDebugDraw::AddDebugOutput("CanRegisterLocalObjectsOfType: %d -> (TooManyTotal:%d, TooManyLocal:%d)", numCanRegisterLocalVehicleFails, numTooManyTotalVehicleFails, numTooManyLocalVehicleFails);        
        grcDebugDraw::AddDebugOutput("CanRegisterObjects:            %d -> (P:%d, V:%d, O:%d)", numCanRegisterObjectsFails, numCanRegisterObjectsPedFails, numCanRegisterObjectsVehicleFails, numCanRegisterObjectsObjectFails);
        grcDebugDraw::AddDebugOutput("No ID fails:                   %d", numNoIDFails);
        grcDebugDraw::AddDebugOutput("Too many object fails:         %d", numTooManyObjectsFails);
        grcDebugDraw::AddDebugOutput("Dump timer fails:              %d", numDumpTimerFails);
        grcDebugDraw::AddDebugOutput("Ambient vehicle deficit:       %d", ambientVehicleDeficit);
    }

    if(s_displayObjectTargetLevelsDebugDisplay)
    {
        for(PhysicalPlayerIndex index = 0; index < (int) MAX_NUM_PHYSICAL_PLAYERS; index++)
        {
            CNetGamePlayer *player = NetworkInterface::GetPhysicalPlayerFromIndex(index);

            if(player)
            {
                if(player->IsMyPlayer())
                {
                    grcDebugDraw::AddDebugOutput("Local Player Percentage Share O:%.2f, P:%.2f, V:%.2f", s_LastPlayerPercentageShares[index].m_ObjectShare, s_LastPlayerPercentageShares[index].m_PedShare, s_LastPlayerPercentageShares[index].m_VehicleShare);
                }
                else if(!player->IsInDifferentTutorialSession())
                {
                    grcDebugDraw::AddDebugOutput("%s Percentage Share O:%.2f, P:%.2f, V:%.2f", player->GetLogName(), s_LastPlayerPercentageShares[index].m_ObjectShare, s_LastPlayerPercentageShares[index].m_PedShare, s_LastPlayerPercentageShares[index].m_VehicleShare);
                }
            }

        }

        grcDebugDraw::AddDebugOutput("");

        grcDebugDraw::AddDebugOutput("Population Target Objects:  %d", ms_PopulationTargetLevels.m_TargetNumObjects);
        grcDebugDraw::AddDebugOutput("Population Target Peds:     %d", ms_PopulationTargetLevels.m_TargetNumPeds);
        grcDebugDraw::AddDebugOutput("Population Target Vehicles: %d", ms_PopulationTargetLevels.m_TargetNumVehicles);
    }
}

#endif // __BANK

void CNetworkObjectPopulationMgr::Init()
{
    NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "INITIALISING_OBJECT_POPULATION_MANAGER","");

    for(unsigned index = 0; index < MAX_EXTERNAL_RESERVATIONS; index++)
    {
        ms_ExternalReservations[index].Reset();
    }

	ms_TransitionalPopulationTimer = 0;
	ms_NumTransitionalObjects      = 0;
	for(unsigned idx=0; idx<MAX_NUM_TRANSITIONAL_OBJECTS; idx++)
	{
		ms_TransitionalPopulation[idx] = NETWORK_INVALID_OBJECT_ID;
	}

    ms_CurrentExternalReservation   = UNUSED_RESERVATION_ID;
    ms_ProcessLocalPopulationLimits = true;

	ms_eVehicleGenerationContext = VGT_AmbientPopulation;
	ms_ePedGenerationContext = PGT_Default;

    ms_PopulationTargetLevels.Reset();
    ms_CurrentBandwidthTargetLevels.Reset();
    ms_MaxBandwidthTargetLevels.Reset();
    ms_BalancingTargetLevels.Reset();
    ms_LastTimeBandwidthLevelsIncreased = 0;

    OUTPUT_ONLY(ms_LastFailType = PFT_SUCCESS);

	ms_CachedNumLocalVehicles = 0;
}

void CNetworkObjectPopulationMgr::Shutdown()
{
    NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "SHUTTING_DOWN_OBJECT_POPULATION_MANAGER","");

	ms_TransitionalPopulationTimer = 0;
	ms_NumTransitionalObjects      = 0;
	for(unsigned idx=0; idx<MAX_NUM_TRANSITIONAL_OBJECTS; idx++)
	{
		ms_TransitionalPopulation[idx] = NETWORK_INVALID_OBJECT_ID;
	}
}

void CNetworkObjectPopulationMgr::Update()
{
    UpdateObjectTargetLevels();
	UpdatePendingTransitionalObjects();

	ms_TotalNumVehicles = CNetwork::GetObjectManager().GetTotalNumObjects(IsVehiclePredicate);

#if __BANK
    const unsigned RESET_FAIL_REASONS_FRAME = 32;
    if((fwTimer::GetSystemFrameCount() & (RESET_FAIL_REASONS_FRAME - 1)) == 0)
    {
        ms_LastPopulationFailureReasons = ms_PopulationFailureReasons;
        ms_PopulationFailureReasons.Reset();
    }
#endif // __BANK;

	ms_CachedNumLocalVehicles  = GetNumLocalVehicles();
}

void CNetworkObjectPopulationMgr::OnNetworkObjectCreated(netObject& object)
{
    if(ms_CurrentExternalReservation != UNUSED_RESERVATION_ID &&
       gnetVerifyf(ms_CurrentExternalReservation < MAX_EXTERNAL_RESERVATIONS, "Current External Reservation is Invalid!") &&
       gnetVerifyf(ms_ExternalReservations[ms_CurrentExternalReservation].m_ReservationID == ms_CurrentExternalReservation, "Current External Reservation is not in use!"))
	{
		object.SetGlobalFlag(CNetObjGame::GLOBALFLAG_RESERVEDOBJECT, true);
	}
}

void CNetworkObjectPopulationMgr::OnNetworkObjectRemoved(netObject&)
{
}

unsigned CNetworkObjectPopulationMgr::CreateExternalReservation(u32 numPedsToReserve,
                                                                u32 numVehiclesToReserve,
																u32 numObjectsToReserve,
																bool bAllowExcessAllocations)
{
    unsigned reservationID = UNUSED_RESERVATION_ID;

    if(gnetVerifyf(numObjectsToReserve  <= MAX_EXTERNAL_RESERVED_OBJECTS,  "Trying to reserve too many objects for an external system!") &&
       gnetVerifyf(numPedsToReserve     <= MAX_EXTERNAL_RESERVED_PEDS,     "Trying to reserve too many peds for an external system!")    &&
       gnetVerifyf(numVehiclesToReserve <= MAX_EXTERNAL_RESERVED_VEHICLES, "Trying to reserve too many vehicles for an external system!"))
    {
        for(unsigned index = 0; index < MAX_EXTERNAL_RESERVATIONS && (reservationID == UNUSED_RESERVATION_ID); index++)
        {
            if(ms_ExternalReservations[index].m_ReservationID == UNUSED_RESERVATION_ID)
            {
                reservationID = index;
            }
        }

        if(gnetVerifyf(reservationID != UNUSED_RESERVATION_ID, "Ran out of external reservations! Possibly need to increase MAX_EXTERNAL_RESERVATIONS."))
        {
            ExternalReservation &reservation = ms_ExternalReservations[reservationID];

            reservation.m_ReservationID     = reservationID;
            reservation.m_ReservedObjects   = numObjectsToReserve;
            reservation.m_ReservedPeds      = numPedsToReserve;
            reservation.m_ReservedVehicles  = numVehiclesToReserve;
            reservation.m_AllocatedObjects  = 0;
            reservation.m_AllocatedPeds     = 0;
            reservation.m_AllocatedVehicles = 0;
			reservation.m_AllowExcessAllocations = bAllowExcessAllocations;

			FindAllocatedExternalReservations(reservationID);
        }
    }

    return reservationID;
}

void CNetworkObjectPopulationMgr::UpdateExternalReservation(unsigned reservationID, u32 numPedsToReserve, u32 numVehiclesToReserve, u32 numObjectsToReserve)
{
	if(gnetVerifyf(reservationID < MAX_EXTERNAL_RESERVATIONS, "Trying to update an invalid external reservation!"))
	{
		if(gnetVerifyf(numObjectsToReserve  <= MAX_EXTERNAL_RESERVED_OBJECTS,  "Trying to reserve too many objects for an external system!") &&
			gnetVerifyf(numPedsToReserve     <= MAX_EXTERNAL_RESERVED_PEDS,     "Trying to reserve too many peds for an external system!")    &&
			gnetVerifyf(numVehiclesToReserve <= MAX_EXTERNAL_RESERVED_VEHICLES, "Trying to reserve too many vehicles for an external system!"))
		{
			ExternalReservation &reservation = ms_ExternalReservations[reservationID];

			reservation.m_ReservedObjects   = numObjectsToReserve;
			reservation.m_ReservedPeds      = numPedsToReserve;
			reservation.m_ReservedVehicles  = numVehiclesToReserve;

			FindAllocatedExternalReservations(reservationID);
		}
	}
}

void CNetworkObjectPopulationMgr::FindAllocatedExternalReservations(unsigned reservationID)
{
	if(gnetVerify(reservationID < MAX_EXTERNAL_RESERVATIONS))
	{
		ExternalReservation &reservation = ms_ExternalReservations[reservationID];

		reservation.m_AllocatedPeds			= 0;
		reservation.m_AllocatedVehicles		= 0;
		reservation.m_AllocatedObjects		= 0;

		netObject* objects[MAX_NUM_NETOBJECTS];
		unsigned numObjects = netInterface::GetObjectManager().GetAllObjects(objects, MAX_NUM_NETOBJECTS, true, false);

		for (u32 i=0; i<numObjects; i++)
		{
			netObject* pNetObj = objects[i];

			if (IsVehicleObjectType(pNetObj->GetObjectType()))
			{
				CNetObjVehicle *pVehicleObj = SafeCast(CNetObjVehicle, pNetObj);

				pVehicleObj->SetCountedByDispatch(false);
			}
		}

		for (u32 i=0; i<numObjects; i++)
		{
			netObject* pNetObj = objects[i];

			if (pNetObj && pNetObj->IsGlobalFlagSet(CNetObjGame::GLOBALFLAG_RESERVEDOBJECT))
			{
				if (pNetObj->GetObjectType() == NET_OBJ_TYPE_PED)
				{
					CPed* pPed = SafeCast(CPed, pNetObj->GetEntity());

					if (pPed && !pPed->GetIsDeadOrDying())
					{
						reservation.m_AllocatedPeds++;

						if (pPed->GetMyVehicle() && pPed->GetMyVehicle()->GetDriver() == NULL)
						{
							CNetObjVehicle *pVehicleObj = SafeCast(CNetObjVehicle, pPed->GetMyVehicle()->GetNetworkObject());
							
							if (pVehicleObj && !pVehicleObj->HasBeenCountedByDispatch())
							{
								reservation.m_AllocatedVehicles++;
								pVehicleObj->SetCountedByDispatch(true);
							}
						}
					}
				}
				else if (IsVehicleObjectType(pNetObj->GetObjectType()))
				{
					CVehicle* pVehicle = SafeCast(CVehicle, pNetObj->GetEntity());
					CNetObjVehicle *pVehicleObj = SafeCast(CNetObjVehicle, pNetObj);

					if (pVehicle && pVehicle->GetDriver() != NULL && pVehicle->GetStatus() != STATUS_WRECKED && !pVehicleObj->HasBeenCountedByDispatch())
					{
						reservation.m_AllocatedVehicles++;
						pVehicleObj->SetCountedByDispatch(true);
					}
				}
				else if (pNetObj->GetObjectType() == NET_OBJ_TYPE_OBJECT)
				{
					reservation.m_AllocatedObjects++;
				}
			}
		}
	}
}

bool CNetworkObjectPopulationMgr::IsExternalReservationInUse(unsigned reservationID)
{
	if(gnetVerifyf(reservationID < MAX_EXTERNAL_RESERVATIONS, "Trying to release an invalid external reservation!"))
	{
		ExternalReservation &reservation = ms_ExternalReservations[reservationID];

		return reservation.IsInUse();
	}

	return false;
}

void CNetworkObjectPopulationMgr::ReleaseExternalReservation(unsigned reservationID)
{
    if(gnetVerifyf(reservationID < MAX_EXTERNAL_RESERVATIONS, "Trying to release an invalid external reservation!"))
    {
        ExternalReservation &reservation = ms_ExternalReservations[reservationID];

        if(gnetVerifyf(reservation.m_ReservationID == reservationID, "Trying to release an external reservation not in use!"))
        {
            reservation.Reset();
        }
    }
}

void CNetworkObjectPopulationMgr::SetCurrentExternalReservationIndex(unsigned reservationID)
{
    if(gnetVerifyf(reservationID < MAX_EXTERNAL_RESERVATIONS, "Trying to set current external reservation to an invalid value!"))
    {
        ms_CurrentExternalReservation = reservationID;
    }
}

void CNetworkObjectPopulationMgr::ClearCurrentReservationIndex()
{
    ms_CurrentExternalReservation = UNUSED_RESERVATION_ID;
}

void CNetworkObjectPopulationMgr::GetNumReservedObjects(unsigned reservationID, u32& numPeds, u32& numVehicles, u32& numObjects)
{
	if(gnetVerifyf(reservationID < MAX_EXTERNAL_RESERVATIONS, "Trying to set current external reservation to an invalid value!"))
	{
		ExternalReservation &reservation = ms_ExternalReservations[reservationID];

		numPeds		= reservation.m_ReservedPeds;
		numVehicles = reservation.m_ReservedVehicles;
		numObjects	= reservation.m_ReservedObjects;
	}
}

void CNetworkObjectPopulationMgr::GetNumCreatedObjects(unsigned reservationID, u32& numPeds, u32& numVehicles, u32& numObjects)
{
	if(gnetVerifyf(reservationID < MAX_EXTERNAL_RESERVATIONS, "Trying to set current external reservation to an invalid value!"))
	{
		ExternalReservation &reservation = ms_ExternalReservations[reservationID];

		numPeds		= reservation.m_AllocatedPeds;
		numVehicles = reservation.m_AllocatedVehicles;
		numObjects	= reservation.m_AllocatedObjects;
	}
}

void CNetworkObjectPopulationMgr::SetNumCreatedObjects(unsigned reservationID, u32 numPeds, u32 numVehicles, u32 numObjects)
{
	if(gnetVerifyf(reservationID < MAX_EXTERNAL_RESERVATIONS, "Trying to set current external reservation to an invalid value!"))
	{
		ExternalReservation &reservation = ms_ExternalReservations[reservationID];

		if (gnetVerifyf(numVehicles <= reservation.m_ReservedVehicles || reservation.m_AllowExcessAllocations, "External system creating more vehicles than it has reserved!"))
		{
			reservation.m_AllocatedVehicles = numVehicles;
		}

		if (gnetVerifyf(numPeds <= reservation.m_ReservedPeds || reservation.m_AllowExcessAllocations, "External system creating more peds than it has reserved!"))
		{
			reservation.m_AllocatedPeds = numPeds;
		}

		if (gnetVerifyf(numObjects <= reservation.m_ReservedObjects || reservation.m_AllowExcessAllocations, "External system creating more objects than it has reserved!"))
		{
			reservation.m_AllocatedObjects = numObjects;
		}
	}
}

int CNetworkObjectPopulationMgr::GetTotalNumObjects()
{
    return CNetwork::GetObjectManager().GetTotalNumObjects();
}

void CNetworkObjectPopulationMgr::SetBandwidthTargetLevels(u32 targetNumObjects, u32 targetNumPeds, u32 targetNumVehicles)
{
    ms_CurrentBandwidthTargetLevels.m_TargetNumObjects   = targetNumObjects;
    ms_CurrentBandwidthTargetLevels.m_TargetNumPeds      = targetNumPeds;
    ms_CurrentBandwidthTargetLevels.m_TargetNumVehicles  = targetNumVehicles;
}

void CNetworkObjectPopulationMgr::SetMaxBandwidthTargetLevels(u32 targetBandwidth)
{
    static const u32 MAX_TARGET_LEVELS_INDEX = 6;

    static const u32 targetNumPeds[MAX_TARGET_LEVELS_INDEX] =
    {
        MAX_NUM_LOCAL_NETOBJPEDS,
        MAX_NUM_LOCAL_NETOBJPEDS,
        MAX_NUM_LOCAL_NETOBJPEDS,
        MAX_NUM_LOCAL_NETOBJPEDS,
        MAX_NUM_LOCAL_NETOBJPEDS,
        MAX_NUM_LOCAL_NETOBJPEDS
    };

    static const u32 targetNumVehicles[MAX_TARGET_LEVELS_INDEX] =
    {
        MAX_NUM_LOCAL_NETOBJVEHICLES,
        MAX_NUM_LOCAL_NETOBJVEHICLES,
        MAX_NUM_LOCAL_NETOBJVEHICLES,
        MAX_NUM_LOCAL_NETOBJVEHICLES,
        MAX_NUM_LOCAL_NETOBJVEHICLES,
        MAX_NUM_LOCAL_NETOBJVEHICLES
    };

    // calculate the target level index based on the bandwidth
    u32 targetLevelsIndex = 0;

    if(targetBandwidth >= 1024)
    {
        targetLevelsIndex = 0;
    }
    else if(targetBandwidth >= 512)
    {
        targetLevelsIndex = 1;
    }
    else if(targetBandwidth >= 256)
    {
        targetLevelsIndex = 2;
    }
    else if(targetBandwidth >= 128)
    {
        targetLevelsIndex = 3;
    }
    else if(targetBandwidth > 64)
    {
        targetLevelsIndex = 4;
    }
    else
    {
        targetLevelsIndex = 5;
    }

    gnetAssert(targetLevelsIndex < MAX_TARGET_LEVELS_INDEX);

    if(targetLevelsIndex < MAX_TARGET_LEVELS_INDEX)
    {
        ms_MaxBandwidthTargetLevels.m_TargetNumObjects   = MAX_NUM_NETOBJOBJECTS; // objects don't affect bandwidth so much so are not capped
        ms_MaxBandwidthTargetLevels.m_TargetNumPeds      = targetNumPeds[targetLevelsIndex];
        ms_MaxBandwidthTargetLevels.m_TargetNumVehicles  = targetNumVehicles[targetLevelsIndex];
    }
}

bool CNetworkObjectPopulationMgr::CanRegisterObject(NetworkObjectType objectType, bool isMissionObject)
{
    bool canRegister = true;

    if(!NetworkInterface::GetObjectManager().GetObjectIDManager().HasFreeObjectIdsAvailable(1, isMissionObject))
    {
        canRegister = false;
        BANK_ONLY(ms_PopulationFailureReasons.m_NumNoIDFails++;);
        OUTPUT_ONLY(ms_LastFailType = PFT_NO_ID_FAILS);
    }
    else if(!CanRegisterLocalObjectsOfType(objectType, 1, isMissionObject))
    {
        canRegister = false;

        if(IsVehicleObjectType(objectType))
        {
            BANK_ONLY(ms_PopulationFailureReasons.m_NumCanRegisterLocalVehicleFails++);
        }
        else if(objectType == NET_OBJ_TYPE_PED) 
        { 
            BANK_ONLY(ms_PopulationFailureReasons.m_NumCanRegisterLocalPedFails++);
        }

    }
    else if(!isMissionObject && ms_ProcessLocalPopulationLimits && NetworkInterface::GetObjectManager().IsOwningTooManyObjects())
    {
        canRegister = false;
        BANK_ONLY(ms_PopulationFailureReasons.m_NumTooManyObjectsFails++;);
        OUTPUT_ONLY(ms_LastFailType = PFT_TOO_MANY_OBJECTS);
    }

    BANK_ONLY(if(IsVehicleObjectType(objectType) && !canRegister) { ms_PopulationFailureReasons.m_NumCanRegisterVehicleFails++; } );
    BANK_ONLY(if(objectType == NET_OBJ_TYPE_PED  && !canRegister) { ms_PopulationFailureReasons.m_NumCanRegisterPedFails++; } );

    return canRegister;
}

bool CNetworkObjectPopulationMgr::CanRegisterObjects(u32 numPeds, u32 numVehicles, u32 numObjects, u32 numPickups, u32 numDoors, bool areMissionObjects)
{
	bool canRegister = false;

	u32 totalNumObjects = numPeds + numVehicles + numObjects + numPickups + numDoors;

	if(!NetworkInterface::GetObjectManager().GetObjectIDManager().HasFreeObjectIdsAvailable(totalNumObjects, areMissionObjects))
    {
        BANK_ONLY(ms_PopulationFailureReasons.m_NumNoIDFails++;);
        OUTPUT_ONLY(ms_LastFailType = PFT_NO_ID_FAILS);
    }
    else
	{
        canRegister = (areMissionObjects || !NetworkInterface::GetObjectManager().IsOwningTooManyObjects());

        if(!canRegister)
        {
            OUTPUT_ONLY(ms_LastFailType = PFT_TOO_MANY_OBJECTS);
        }

        if(numPeds > 0 && canRegister)
        {
            canRegister = CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PED, numPeds, areMissionObjects);

            if(!canRegister) 
            { 
                BANK_ONLY(ms_PopulationFailureReasons.m_NumCanRegisterObjectsPedFails++);
            }
        }

        if(numVehicles > 0 && canRegister)
        {
            // the population management code doesn't distinguish between different vehicle types - so it's safe to check for automobiles
            canRegister = CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_AUTOMOBILE, numVehicles, areMissionObjects);

            if(!canRegister) 
            {
                BANK_ONLY(ms_PopulationFailureReasons.m_NumCanRegisterObjectsVehicleFails++);
            }
        }

        if(numObjects > 0 && canRegister)
        {
            canRegister = CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_OBJECT, numObjects, areMissionObjects);

            if(!canRegister)
            {
                BANK_ONLY(ms_PopulationFailureReasons.m_NumCanRegisterObjectsObjectFails++);
            }
        }

        if(numPickups > 0 && canRegister)
        {
            canRegister = CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_PICKUP_PLACEMENT, numPickups, areMissionObjects);
        }

		if(numDoors > 0 && canRegister)
		{
			canRegister = CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_DOOR, numDoors, areMissionObjects);
		}

    }

    BANK_ONLY(if(!canRegister) { ms_PopulationFailureReasons.m_NumCanRegisterObjectsFails++; } );

    return canRegister;
}

bool CNetworkObjectPopulationMgr::CanRegisterLocalObjectOfType(NetworkObjectType objectType, bool isMissionObject)
{
    return CanRegisterLocalObjectsOfType(objectType, 1, isMissionObject);
}

bool CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NetworkObjectType objectType, u32 count, bool areMissionObjects)
{
	bool canRegister = false;

	if(NetworkInterface::IsInSpectatorMode() && (objectType == NET_OBJ_TYPE_PED || IsVehicleObjectType(objectType)))
	{
		// if the local spectator player joined the session less than 50 seconds ago don't allow object creation
		const static u32 SPECTATOR_WAIT_TIME_AFTER_JOIN_SESSION = 50 * 1000;
		if(CNetwork::GetNetworkSession().GetTimeInSession() < SPECTATOR_WAIT_TIME_AFTER_JOIN_SESSION)
		{
			OUTPUT_ONLY(ms_LastFailType = PFT_SPECTATOR_JOIN_DELAY);
			return false;
		}
	}

	u32 timeSinceLastObjectDumped = fwTimer::GetSystemTimeInMilliseconds() - CNetwork::GetObjectManager().m_lastTimeObjectDumped;

	if((timeSinceLastObjectDumped > DUMPED_AMBIENT_ENTITY_TIMER) || areMissionObjects || !ms_ProcessLocalPopulationLimits)
	{
		// first check whether any more objects of this type can be registered
		if(CanRegisterObjectsOfType(objectType, count, areMissionObjects, ms_CurrentExternalReservation != UNUSED_RESERVATION_ID))
		{
			// then check whether we are allowed to own any more objects of this type
			u32 numObjectsOwned        = GetNumLocalObjects(objectType);
			u32 maxObjectsAllowedToOwn = GetMaxObjectsAllowedToOwnOfType(objectType);

			// check whether we are using an external reservation
			bool externalReservationValid = CanAllocateFromExternalReservation(objectType, count);

			if(((numObjectsOwned + count) <= maxObjectsAllowedToOwn) || areMissionObjects || externalReservationValid)
			{
				canRegister = true;
                OUTPUT_ONLY(ms_LastFailType = PFT_SUCCESS);
			}
            else
            {
                if(IsVehicleObjectType(objectType))
                {
                    BANK_ONLY(ms_PopulationFailureReasons.m_NumTooManyLocalVehicleFails++);
                    OUTPUT_ONLY(ms_LastFailType = PFT_TOO_MANY_LOCAL_VEHICLES);
                }
                else if(objectType == NET_OBJ_TYPE_PED) 
                { 
                    BANK_ONLY(ms_PopulationFailureReasons.m_NumTooManyLocalPedFails++);
                    OUTPUT_ONLY(ms_LastFailType = PFT_TOO_MANY_LOCAL_PEDS);
                }
            }
		}
        else
        {
            if(IsVehicleObjectType(objectType))
            {
                BANK_ONLY(ms_PopulationFailureReasons.m_NumTooManyTotalVehicleFails++);
                OUTPUT_ONLY(ms_LastFailType = PFT_TOO_MANY_TOTAL_VEHICLES);
            }
            else if(objectType == NET_OBJ_TYPE_PED) 
            { 
                BANK_ONLY(ms_PopulationFailureReasons.m_NumTooManyTotalPedFails++);
                OUTPUT_ONLY(ms_LastFailType = PFT_TOO_MANY_TOTAL_PEDS);
            }
        }
	}
    else
    {
        BANK_ONLY(ms_PopulationFailureReasons.m_NumDumpTimerFails++);
        OUTPUT_ONLY(ms_LastFailType = PFT_DUMP_TIMER_ACTIVE);
    }  

	return canRegister;
}

bool CNetworkObjectPopulationMgr::CanRegisterRemoteObjectOfType(NetworkObjectType objectType, bool isMissionObject, bool isReservedObject)
{
    return CanRegisterObjectOfType(objectType, isMissionObject, isReservedObject);
}

bool CNetworkObjectPopulationMgr::CanTakeOwnershipOfObjectOfType(NetworkObjectType objectType, bool isMissionObject)
{
    bool   canTakeOwnership       = false;
    u32 numObjectsOwned        = GetNumLocalObjects(objectType);
    u32 maxObjectsAllowedToOwn = GetMaxObjectsAllowedToOwnOfType(objectType);

    if((numObjectsOwned < maxObjectsAllowedToOwn) || isMissionObject)
    {
        canTakeOwnership = true;
    }

    return canTakeOwnership;
}

#if !__NO_OUTPUT

const char *CNetworkObjectPopulationMgr::GetLastFailTypeString()
{
    switch(ms_LastFailType)
    {
    case PFT_SUCCESS:
        return "Success";
    case PFT_TOO_MANY_TOTAL_PEDS:
        return "Too many total peds";
    case PFT_TOO_MANY_LOCAL_PEDS:
        return "Too many local peds";
    case PFT_TOO_MANY_TOTAL_VEHICLES:
        return "Too many total vehicles";
    case  PFT_TOO_MANY_LOCAL_VEHICLES:
        return "Too many local vehicles";
    case PFT_NO_ID_FAILS:
        return "No object IDs";
    case PFT_TOO_MANY_OBJECTS:
        return "Owning too many objects";
    case PFT_DUMP_TIMER_ACTIVE:
        return "Dump timer active";
	case PFT_SPECTATOR_JOIN_DELAY:
		return "Spectator just joined so wait";
    default:
        gnetAssertf(0, "Unknown fail type!");
        return "Unknown fail type!";
    }
}

#endif // !__NO_OUTPUT

bool  CNetworkObjectPopulationMgr::FlagObjAsTransitional(const ObjectId& id)
{
	bool added = false;

	//Object already exists;
	for(unsigned idx=0; idx<MAX_NUM_TRANSITIONAL_OBJECTS && !added; idx++)
	{
		if (id == ms_TransitionalPopulation[idx])
		{
			added = true;
		}
	}

	if (0 == ms_NumTransitionalObjects)
	{
		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tSTART_CONTROL_TRANSITIONAL_POPULATION\t\t", "");
		BANK_ONLY(NetworkDebug::ToggleLogExtraDebugNetworkPopulation();)
		ms_TransitionalPopulationTimer = sysTimer::GetSystemMsTime();
	}

	netObject* obj = NetworkInterface::GetNetworkObject(id);
	if (!added && gnetVerify(obj) && gnetVerifyf(ms_NumTransitionalObjects < MAX_NUM_TRANSITIONAL_OBJECTS, "ms_NumTransitionalObjects=%d", ms_NumTransitionalObjects))
	{
		gnetAssert(NETWORK_INVALID_OBJECT_ID == ms_TransitionalPopulation[ms_NumTransitionalObjects]);

		NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tREGISTER_TRANSITIONAL_OBJECT\t\t", obj->GetLogName());

		ms_TransitionalPopulation[ms_NumTransitionalObjects] = id;

		ms_NumTransitionalObjects++;

		added = true;
	}

	return added;
}

bool  CNetworkObjectPopulationMgr::UnFlagObjAsTransitional(const ObjectId& id)
{
	bool found = false;

	for(unsigned idx=0; idx<MAX_NUM_TRANSITIONAL_OBJECTS && !found; idx++)
	{
		found = (id == ms_TransitionalPopulation[idx]);

		if (found)
		{
			netObject* obj = NetworkInterface::GetNetworkObject(id);
			if (obj)
			{
				NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tREMOVE_TRANSITIONAL_OBJECT\t\t", obj->GetLogName());
			}
			else
			{
				NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tREMOVE_TRANSITIONAL_OBJECT\t\t", "%d", ms_TransitionalPopulation[idx]);
			}

			ms_TransitionalPopulation[idx] = NETWORK_INVALID_OBJECT_ID;

			//shift left array
			for(; idx+1<MAX_NUM_TRANSITIONAL_OBJECTS && NETWORK_INVALID_OBJECT_ID != ms_TransitionalPopulation[idx+1]; idx++)
			{
				ms_TransitionalPopulation[idx]   = ms_TransitionalPopulation[idx+1];
				ms_TransitionalPopulation[idx+1] = NETWORK_INVALID_OBJECT_ID;
			}
		}
	}

	if (found)
	{
		ms_NumTransitionalObjects--;
	}

	return found;
}

int CNetworkObjectPopulationMgr::GetNumLocalObjects(NetworkObjectType objectType)
{
    netObjectTypePredicate::SetObjectType(objectType);
    return CNetwork::GetObjectManager().GetNumLocalObjects(netObjectTypePredicate::ObjectInclusionTest);
}

int CNetworkObjectPopulationMgr::GetNumLocalVehicles()
{
    return CNetwork::GetObjectManager().GetNumLocalObjects(IsVehiclePredicate);
}

int CNetworkObjectPopulationMgr::GetNumRemoteObjects(NetworkObjectType objectType)
{
    netObjectTypePredicate::SetObjectType(objectType);
    return CNetwork::GetObjectManager().GetNumRemoteObjects(netObjectTypePredicate::ObjectInclusionTest);
}

int CNetworkObjectPopulationMgr::GetNumRemoteVehicles()
{
    return CNetwork::GetObjectManager().GetNumRemoteObjects(IsVehiclePredicate);
}

int CNetworkObjectPopulationMgr::GetTotalNumObjectsOfType(NetworkObjectType objectType)
{
    netObjectTypePredicate::SetObjectType(objectType);
    int numObjectsOfType = CNetwork::GetObjectManager().GetTotalNumObjects(netObjectTypePredicate::ObjectInclusionTest);

    return numObjectsOfType;
}

int CNetworkObjectPopulationMgr::GetTotalNumVehicles()
{
    int numObjectsOfType = CNetwork::GetObjectManager().GetTotalNumObjects(IsVehiclePredicate);

    return numObjectsOfType;
}

int CNetworkObjectPopulationMgr::GetMaxNumObjectsOfType(NetworkObjectType objectType)
{
    u32 maxObjectsAllowed = 0;

    if(IsVehicleObjectType(objectType))
    {
        maxObjectsAllowed = MAX_NUM_NETOBJVEHICLES;
    }
    else
    {
        switch(objectType)
        {
        case NET_OBJ_TYPE_PLAYER:
            {
                maxObjectsAllowed = MAX_NUM_NETOBJPLAYERS;
            }
            break;
        case NET_OBJ_TYPE_PED:
            {
                maxObjectsAllowed = MAX_NUM_NETOBJPEDS;
            }
            break;
        case NET_OBJ_TYPE_OBJECT:
            {
                maxObjectsAllowed = MAX_NUM_NETOBJOBJECTS;
            }
            break;
		case NET_OBJ_TYPE_DOOR:
        case NET_OBJ_TYPE_PICKUP:
        case NET_OBJ_TYPE_PICKUP_PLACEMENT:
            {
                // Pickups & doors should always be allowed
                maxObjectsAllowed = ~0u;
            }
            break;
        default:
            {
                gnetAssertf(0, "Unexpected network object type encountered!");
            }
            break;
        }
    }

#if __BANK
    // check whether we are overriding these limits for debugging purposes
    if(ms_ObjectManagementDebugSettings.m_UseDebugSettings)
    {
        if(IsVehicleObjectType(objectType))
        {
            maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxTotalVehicles;
        }
        else
        {
            switch(objectType)
            {
            case NET_OBJ_TYPE_PLAYER:
            case NET_OBJ_TYPE_PED:
                {
                    maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxTotalPeds;
                }
                break;
            case NET_OBJ_TYPE_OBJECT:
                {
                    maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxTotalObjects;
                }
                break;
			case NET_OBJ_TYPE_DOOR: 
            case NET_OBJ_TYPE_PICKUP:
            case NET_OBJ_TYPE_PICKUP_PLACEMENT:
                break;
            default:
                break;
            }
        }
    }
#endif

    return maxObjectsAllowed;
}

void CNetworkObjectPopulationMgr::GetNumExternallyRequiredEntities(u32 &numUnusedReservedPeds,
                                                                   u32 &numUnusedReservedVehicles,
																   u32 &numUnusedReservedObjects)
{
    numUnusedReservedPeds     = 0;
    numUnusedReservedVehicles = 0;
	numUnusedReservedObjects  = 0;

    for(unsigned index = 0; index < MAX_EXTERNAL_RESERVATIONS; index++)
    {
        ExternalReservation &reservation = ms_ExternalReservations[index];

        if(reservation.IsInUse() && (ms_CurrentExternalReservation != reservation.m_ReservationID))
        {
			if (!reservation.m_AllowExcessAllocations)
			{
				gnetAssertf(reservation.m_ReservedPeds >= reservation.m_AllocatedPeds, "Num allocated peds is greater than num reserved peds!");
				gnetAssertf(reservation.m_ReservedVehicles >= reservation.m_AllocatedVehicles, "Num allocated vehicles is greater than num reserved vehicles!");
				gnetAssertf(reservation.m_ReservedObjects >= reservation.m_AllocatedObjects, "Num allocated objects is greater than num reserved objects!");
			}

			if (reservation.m_ReservedPeds >= reservation.m_AllocatedPeds)
			{
				u32 unusedPeds = reservation.m_ReservedPeds - reservation.m_AllocatedPeds;
				numUnusedReservedPeds += unusedPeds;
			}

			if(reservation.m_ReservedVehicles >= reservation.m_AllocatedVehicles)
			{
				u32 unusedVehicles = reservation.m_ReservedVehicles - reservation.m_AllocatedVehicles;
				numUnusedReservedVehicles += unusedVehicles;
			}

			if(reservation.m_ReservedObjects >= reservation.m_AllocatedObjects)
			{
				u32 unusedObjects = reservation.m_ReservedObjects - reservation.m_AllocatedObjects;
				numUnusedReservedObjects += unusedObjects;
			}
 		}
    }
}

bool CNetworkObjectPopulationMgr::CanAllocateFromExternalReservation(NetworkObjectType objectType, u32 count)
{
    bool canAllocate = false;
            
    if(ms_CurrentExternalReservation != CNetworkObjectPopulationMgr::UNUSED_RESERVATION_ID)
    {
        ExternalReservation &reservation = ms_ExternalReservations[ms_CurrentExternalReservation];

		if (reservation.m_AllowExcessAllocations)
		{
			canAllocate = true;
		}
		else
		{
			if(IsVehicleObjectType(objectType))
			{
				if((reservation.m_AllocatedVehicles + count) <= reservation.m_ReservedVehicles)
				{
					canAllocate = true;
				}
			}
			else
			{
				switch(objectType)
				{
				case NET_OBJ_TYPE_PED:
					{
						if((reservation.m_AllocatedPeds + count) <= reservation.m_ReservedPeds)
						{
							canAllocate = true;
						}
					}
					break;
				case NET_OBJ_TYPE_OBJECT:
					{
						if((reservation.m_AllocatedObjects + count) <= reservation.m_ReservedObjects)
						{
							canAllocate = true;
						}
					}
					break;
				case NET_OBJ_TYPE_DOOR:
				case NET_OBJ_TYPE_PICKUP:
				case NET_OBJ_TYPE_PICKUP_PLACEMENT:
					// these types cannot be externally reserved at present
					break;
				default:
					{
						gnetAssertf(0, "Unexpected network object type encountered!");
					}
					break;
				}
			}
		}
    }

    return canAllocate;
}

bool CNetworkObjectPopulationMgr::CanRegisterObjectOfType(NetworkObjectType objectType, bool isMissionObject, bool isReservedObject)
{
    return CanRegisterObjectsOfType(objectType, 1, isMissionObject, isReservedObject);
}

bool CNetworkObjectPopulationMgr::CanRegisterObjectsOfType(NetworkObjectType objectType, u32 count, bool areMissionObjects, bool areReservedObjects)
{
	bool canRegister = false;

	u32 maxObjectsAllowed = 0;
	int numObjectsOfType  = 0;

	if(IsVehicleObjectType(objectType))
	{
		numObjectsOfType  = GetTotalNumVehicles();
		maxObjectsAllowed = MAX_NUM_NETOBJVEHICLES;
	}
	else
	{
		numObjectsOfType = GetTotalNumObjectsOfType(objectType);

		switch(objectType)
		{
		case NET_OBJ_TYPE_PLAYER:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJPLAYERS;
			}
			break;
		case NET_OBJ_TYPE_PED:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJPEDS;
			}
			break;
		case NET_OBJ_TYPE_OBJECT:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJOBJECTS;
			}
			break;
		case NET_OBJ_TYPE_DOOR:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJDOORS;
			}
			break;
		case NET_OBJ_TYPE_GLASS_PANE:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJGLASSPANE;
			}
			break;
		case NET_OBJ_TYPE_PICKUP:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJPICKUPS;
			}
			break;
		case NET_OBJ_TYPE_PICKUP_PLACEMENT:
			{
				maxObjectsAllowed =  MAX_NUM_NETOBJPICKUPPLACEMENTS;
			}
			break;
		default:
			{
				gnetAssertf(0, "Unexpected network object type encountered!");
			}
			break;
		}
	}

#if __BANK
	// check whether we are overriding these limits for debugging purposes
	if(ms_ObjectManagementDebugSettings.m_UseDebugSettings)
	{
		if(IsVehicleObjectType(objectType))
		{
			maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxTotalVehicles;
		}
		else
		{
			switch(objectType)
			{
			case NET_OBJ_TYPE_PLAYER:
			case NET_OBJ_TYPE_PED:
				{
					maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxTotalPeds;
				}
				break;
			case NET_OBJ_TYPE_OBJECT:
				{
					maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxTotalObjects;
				}
				break;
			case NET_OBJ_TYPE_PICKUP:
			case NET_OBJ_TYPE_PICKUP_PLACEMENT:
			case NET_OBJ_TYPE_DOOR:  
			case NET_OBJ_TYPE_GLASS_PANE:
				break;
			default:
				break;
			}
		}
	}
#endif

	// adjust object allowance based on reservations from external systems
	u32 numUnusedReservedScriptPeds			= 0;
	u32 numUnusedReservedScriptVehicles		= 0;
	u32 numUnusedReservedScriptObjects		= 0;
	u32 numUnusedReservedDispatchPeds		= 0;
	u32 numUnusedReservedDispatchVehicles	= 0;
	u32 numUnusedReservedDispatchObjects	= 0;

	// adjust object allowance based on the reserved mission object counts
	if(!areMissionObjects)
	{
		if (!areReservedObjects)
		{
			GetNumExternallyRequiredEntities(numUnusedReservedDispatchPeds, numUnusedReservedDispatchVehicles, numUnusedReservedDispatchObjects);
		}

		CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numUnusedReservedScriptPeds, numUnusedReservedScriptVehicles, numUnusedReservedScriptObjects);
	}

	u32 totalUnusedReservedPeds = numUnusedReservedScriptPeds + numUnusedReservedDispatchPeds;
	u32 totalUnusedReservedVehicles = numUnusedReservedScriptVehicles + numUnusedReservedDispatchVehicles;
	u32 totalUnusedReservedObjects = numUnusedReservedScriptObjects + numUnusedReservedDispatchObjects;

	if(IsVehicleObjectType(objectType))
	{
		if(maxObjectsAllowed >= totalUnusedReservedVehicles)
		{
			maxObjectsAllowed -= totalUnusedReservedVehicles;
		}
		else
		{
			maxObjectsAllowed = 0;
		}
	}
	else
	{
		switch(objectType)
		{
		case NET_OBJ_TYPE_PED:
			{
				if(maxObjectsAllowed >= totalUnusedReservedPeds)
				{
					maxObjectsAllowed -= totalUnusedReservedPeds;
				}
				else
				{
					maxObjectsAllowed = 0;
				}
			}
			break;
		case NET_OBJ_TYPE_OBJECT:
			{
				if(maxObjectsAllowed >= totalUnusedReservedObjects)
				{
					maxObjectsAllowed -= totalUnusedReservedObjects;
				}
				else
				{
					maxObjectsAllowed = 0;
				}
			}
			break;
		case NET_OBJ_TYPE_DOOR:
		case NET_OBJ_TYPE_PICKUP:
		case NET_OBJ_TYPE_PICKUP_PLACEMENT:
		case NET_OBJ_TYPE_PLAYER:
		case NET_OBJ_TYPE_GLASS_PANE:
			break;
		default:
			gnetAssertf(0, "Unexpected network object type encountered!");
			break;
		}
	}

	if((numObjectsOfType + count) <= maxObjectsAllowed)
	{
		canRegister = true;
	}

#if ENABLE_NETWORK_LOGGING
	if (!canRegister)
	{
		char reservationType[50];

		if (areMissionObjects)
		{
			formatf(reservationType, "SCRIPT");
		}
		else if (areReservedObjects)
		{
			formatf(reservationType, "DISPATCH");
		}
		else
		{
			formatf(reservationType, "AMBIENT");
		}

		CNetwork::GetObjectManager().GetLog().WriteDataValue("CREATION FAIL", "Can't create %d %s objects of this type %s (curr: %d, max: %d)", count, reservationType, NetworkUtils::GetObjectTypeName(objectType, areMissionObjects), numObjectsOfType, maxObjectsAllowed);
		CNetwork::GetObjectManager().GetLog().WriteDataValue("SCRIPT RESERVATIONS", "Ped %d, Veh %d, Obj %d", numUnusedReservedScriptPeds, numUnusedReservedScriptVehicles, numUnusedReservedScriptObjects);
		CNetwork::GetObjectManager().GetLog().WriteDataValue("DISPATCH RESERVATIONS", "Ped %d, Veh %d, Obj %d", numUnusedReservedDispatchPeds, numUnusedReservedDispatchVehicles, numUnusedReservedDispatchObjects);
	}
#endif  // ENABLE_NETWORK_LOGGING

	return canRegister;
}

bool CanUseLocalVehicleBuffer()
{
	if(!CVehiclePopulation::ms_bNetworkPopMaxIfCanUseLocalBufferToggle)
	{
		return false;
	}

	// We want to avoid empty streets in multiplayer games - so we allocate additional network objects
	// to be used when the local target levels have been reached but the global target level has not.
	// The idea here is that the number of network vehicles may temporarily rise into this buffer as
	// remotely owned vehicles are created on the local machine, but this will drop as vehicles are removed naturally
	// via the local vehicle management code
	const int LOCAL_AMBIENT_VEHICLE_BUFFER_SIZE = (MAX_NUM_NETOBJVEHICLES - MAX_NUM_LOCAL_NETOBJVEHICLES);
	int localAmbientVehicleBufferSize = static_cast<int>(static_cast<float>(LOCAL_AMBIENT_VEHICLE_BUFFER_SIZE * CVehiclePopulation::ms_fLocalAmbientVehicleBufferMult));

	int	numAmbientVehicles = CVehiclePopulation::ms_numRandomCars;

	if(numAmbientVehicles < localAmbientVehicleBufferSize)
	{
		return true;
	}

	return false;
}

u32 CNetworkObjectPopulationMgr::GetMaxObjectsAllowedToOwnOfType(NetworkObjectType objectType)
{
    u32 maxObjectsAllowed = 0;

    if(IsVehicleObjectType(objectType))
    {
        maxObjectsAllowed = MIN(ms_PopulationTargetLevels.m_TargetNumVehicles, ms_CurrentBandwidthTargetLevels.m_TargetNumVehicles);
        
		bool bRejectedParkedCars = false;
		if(ms_eVehicleGenerationContext==VGT_CarGen && CVehiclePopulation::ms_bNetworkPopCheckParkedCarsToggle)
		{
			// when we are trying to register a cargen vehicle this is a parked car. We want to prioritise 
			// ambient driving vehicles so only allow this if we have space
			float percentage           = static_cast<float>(maxObjectsAllowed) / static_cast<float>(MAX_NUM_LOCAL_NETOBJVEHICLES);
			int   maxParkedCarsAllowed = MAX_NUM_LOCAL_NETOBJVEHICLES - static_cast<int>(CVehiclePopulation::GetDesiredNumberAmbientVehicles());
			int   maxLocalParkedCars   = static_cast<int>(maxParkedCarsAllowed * percentage * CVehiclePopulation::ms_fMaxLocalParkedCarsMult);
			int   numLocalParkedCars   = CNetwork::GetObjectManager().GetNumLocalObjects(IsParkedVehiclePredicate);

			if(numLocalParkedCars >= maxLocalParkedCars)
			{
				maxObjectsAllowed = 0;
				bRejectedParkedCars = true;
			}
		}

		if(!bRejectedParkedCars)
		{
			if(ms_ProcessLocalPopulationLimits)
			{
				// we need to keep some vehicles back for player vehicles
				u32 numPlayerVehiclesNotCloned = CNetwork::GetObjectManager().GetNumClosePlayerVehiclesNotOnOurMachine();

				if(numPlayerVehiclesNotCloned < maxObjectsAllowed)
				{
					maxObjectsAllowed -= numPlayerVehiclesNotCloned;
				}
				else
				{
					maxObjectsAllowed = 0;
				}

				if(CanUseLocalVehicleBuffer() && (ms_eVehicleGenerationContext!=VGT_CarGen))
				{
					maxObjectsAllowed  = ~0u;
				}
			}
			else
			{
				 maxObjectsAllowed = ms_PopulationTargetLevels.m_TargetNumVehicles;
			}
		}
    }
    else
    {
        switch(objectType)
        {
        case NET_OBJ_TYPE_PLAYER:
            {
                maxObjectsAllowed = 1;
            }
            break;
        case NET_OBJ_TYPE_PED:
            {
                if(ms_ProcessLocalPopulationLimits)
                {
                    maxObjectsAllowed = MIN(ms_PopulationTargetLevels.m_TargetNumPeds, ms_CurrentBandwidthTargetLevels.m_TargetNumPeds);

                    // we need to keep some peds for drivers of cars with pretend occupants
                    u32 numCarsUsingPretendOccupants = CNetwork::GetObjectManager().GetNumLocalVehiclesUsingPretendOccupants();

                    if(numCarsUsingPretendOccupants < maxObjectsAllowed)
                    {
                        maxObjectsAllowed -= numCarsUsingPretendOccupants;
                    }
                    else
                    {
                        maxObjectsAllowed = 0;
                    }

					// If trying to spawn an ambient driver & we have ambient vehicle buffer space, then allow this.
					if(ms_ePedGenerationContext == PGT_AmbientDriver && CanUseLocalVehicleBuffer())
					{
						maxObjectsAllowed  = ~0u;
					}
                }
                else
                {
                    // when we are trying to register a ped and not processing local population limits this
                    // is for something important, like a pretend occupant for a vehicle or a high priority scenario ped
                    maxObjectsAllowed = ms_PopulationTargetLevels.m_TargetNumPeds;
                }
            }
            break;
        case NET_OBJ_TYPE_OBJECT:
            {
                maxObjectsAllowed = MIN(ms_PopulationTargetLevels.m_TargetNumObjects, ms_CurrentBandwidthTargetLevels.m_TargetNumObjects);
            }
            break;
        case NET_OBJ_TYPE_PICKUP:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJPICKUPS;
			}
			break;
        case NET_OBJ_TYPE_PICKUP_PLACEMENT:
			{
				maxObjectsAllowed = MAX_NUM_NETOBJPICKUPPLACEMENTS;
			}
			break;
		case NET_OBJ_TYPE_DOOR:  
			{
				maxObjectsAllowed = MAX_NUM_NETOBJDOORS;
			}
			break;
		case NET_OBJ_TYPE_GLASS_PANE:
           {
                 maxObjectsAllowed = MAX_NUM_NETOBJGLASSPANE;
            }
           break;
        default:
            {
                gnetAssertf(0, "Unexpected network object type encountered!");
            }
            break;
        }
    }

#if __BANK
    // check whether we are overriding these limits for debugging purposes
    if(ms_ObjectManagementDebugSettings.m_UseDebugSettings)
    {
        if(IsVehicleObjectType(objectType))
        {
            maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxLocalVehicles;
        }
        else
        {
            switch(objectType)
            {
            case NET_OBJ_TYPE_PLAYER:
            case NET_OBJ_TYPE_PED:
                {
                    maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxLocalPeds;
                }
                break;
            case NET_OBJ_TYPE_OBJECT:
                {
                    maxObjectsAllowed = ms_ObjectManagementDebugSettings.m_MaxLocalObjects;
                }
                break;
			case NET_OBJ_TYPE_DOOR: 
            case NET_OBJ_TYPE_PICKUP:
            case NET_OBJ_TYPE_PICKUP_PLACEMENT:
			case NET_OBJ_TYPE_GLASS_PANE:
                break;
            default:
                break;
            }
        }
    }
#endif

    return maxObjectsAllowed;
}

void CNetworkObjectPopulationMgr::ReduceBandwidthTargetLevels()
{
    const unsigned MIN_BANDWIDTH_LEVEL_PEDS     = 20;
    const unsigned MIN_BANDWIDTH_LEVEL_VEHICLES = 20;
    const unsigned REDUCTION_AMOUNT             = 5;

    // Note: We don't modify bandwidth levels for CObjects as they have very low bandwidth requirements
    ms_CurrentBandwidthTargetLevels.m_TargetNumPeds     = MAX(ms_CurrentBandwidthTargetLevels.m_TargetNumPeds     - REDUCTION_AMOUNT, MIN_BANDWIDTH_LEVEL_PEDS);
    ms_CurrentBandwidthTargetLevels.m_TargetNumVehicles = MAX(ms_CurrentBandwidthTargetLevels.m_TargetNumVehicles - REDUCTION_AMOUNT, MIN_BANDWIDTH_LEVEL_VEHICLES);
}

void CNetworkObjectPopulationMgr::IncreaseBandwidthTargetLevels()
{
    const unsigned INCREMENT_AMOUNT = 5;

    // Note: We don't modify bandwidth levels for CObjects as they have very low bandwidth requirements
    ms_CurrentBandwidthTargetLevels.m_TargetNumPeds     = MIN(ms_CurrentBandwidthTargetLevels.m_TargetNumPeds     + INCREMENT_AMOUNT, ms_MaxBandwidthTargetLevels.m_TargetNumPeds);
    ms_CurrentBandwidthTargetLevels.m_TargetNumVehicles = MIN(ms_CurrentBandwidthTargetLevels.m_TargetNumVehicles + INCREMENT_AMOUNT, ms_MaxBandwidthTargetLevels.m_TargetNumVehicles);
}

void CNetworkObjectPopulationMgr::UpdateObjectTargetLevels()
{
    if(!FindPlayerPed() || !FindPlayerPed()->GetNetworkObject())
        return;

#if ENABLE_NETWORK_LOGGING
    u32 oldPopulationTargetNumObjects  = ms_PopulationTargetLevels.m_TargetNumObjects;
    u32 oldPopulationTargetNumPeds     = ms_PopulationTargetLevels.m_TargetNumPeds;
    u32 oldPopulationTargetNumVehicles = ms_PopulationTargetLevels.m_TargetNumVehicles;
#endif // ENABLE_NETWORK_LOGGING

    // check the distance between our ped and the other players peds to
    // find overlapping object clone ranges
	Vector3 myPos = NetworkInterface::GetLocalPlayerFocusPosition();

    u32 numPlayersInObjectRange  = 1;
    u32 numPlayersInPedRange     = 1;
    u32 numPlayersInVehicleRange = 1;

    PercentageShare playerPercentageShares[MAX_NUM_PHYSICAL_PLAYERS];

    u32 playersSharingObjectsBeforeLocalPlayer  = 0;
    u32 playersSharingPedsBeforeLocalPlayer     = 0;
    u32 playersSharingVehiclesBeforeLocalPlayer = 0;
	float myVehicleShareMultiplier = CVehiclePopulation::CalculateVehicleShareMultiplier();

    BANK_ONLY(PhysicalPlayerIndex localPlayerIndex = 0);

    float localShare = 1.0f;

    // track sum of all percentages (initialised to our player object share - full share)
    float objectSharePercentagesSum  = localShare;
    float pedSharePercentagesSum     = localShare;
    float vehicleSharePercentagesSum = localShare;

    for(PhysicalPlayerIndex index = 0; index < (int) MAX_NUM_PHYSICAL_PLAYERS; index++)
    {
        playerPercentageShares[index].Reset();

		//don't include players that are in different sessions or in different garages
		CNetGamePlayer* pPlayer = NetworkInterface::GetPhysicalPlayerFromIndex(index);
		bool bInSameLocation = pPlayer && !pPlayer->IsMyPlayer() && !pPlayer->IsInDifferentTutorialSession() && 
								CNetObjProximityMigrateable::IsInSameGarageInterior(myPos, NetworkInterface::GetPlayerFocusPosition(*pPlayer), NULL);
        if (bInSameLocation)
        {
            Vector3 delta    = NetworkInterface::GetPlayerFocusPosition(*pPlayer) - myPos;
            float   distance = delta.Mag();

            // update the object share
            if(distance <= OBJECT_FULL_PERCENTAGE_DISTANCE)
            {
                playerPercentageShares[index].m_ObjectShare = 1.0f;
            }
            else
            {
                float scaledDistance = distance - OBJECT_FULL_PERCENTAGE_DISTANCE;
                float scaleRange     = OBJECT_SCALED_PERCENTAGE_DISTANCE - OBJECT_FULL_PERCENTAGE_DISTANCE;
                playerPercentageShares[index].m_ObjectShare = MAX(1.0f - (scaledDistance / scaleRange), 0.0f);
            }

            // update the ped share
            if(distance <= PED_FULL_PERCENTAGE_DISTANCE)
            {
                playerPercentageShares[index].m_PedShare = 1.0f;
            }
            else
            {
                float scaledDistance = distance - PED_FULL_PERCENTAGE_DISTANCE;
                float scaleRange     = PED_SCALED_PERCENTAGE_DISTANCE - PED_FULL_PERCENTAGE_DISTANCE;
                playerPercentageShares[index].m_PedShare = MAX(1.0f - (scaledDistance / scaleRange), 0.0f);
            }

			TUNE_GROUP_BOOL(NET_POP_MGR, USE_SHARED_MULTIPLIERS, true);
			if(USE_SHARED_MULTIPLIERS)
			{
				//if we're not happy with our share ratio then if we find a player that has the opposite
				//share ratio to us (so one of us has excess local vehicles and the other is limited in their local vehicles)
				//then scale their perceived distance by their share ratio which will make that player affect us more/less.
				//They do the same which has the effect of "moving" local vehicles from the player who has excess to the player who wants more 
				if(myVehicleShareMultiplier != 1.0f)
				{
					// update the vehicle share
					if (pPlayer->GetPlayerPed() && pPlayer->GetPlayerPed()->GetNetworkObject())
					{
						CNetObjPlayer* netObjPlayer = SafeCast(CNetObjPlayer, pPlayer->GetPlayerPed()->GetNetworkObject());
						if(netObjPlayer)
						{
							float fVehicleShareMultiplier = netObjPlayer->GetVehicleShareMultiplier();
							if(fVehicleShareMultiplier != 1.0f && (fVehicleShareMultiplier > 1.0f) ^ (myVehicleShareMultiplier > 1.0f))
							{		
								//adjust distance by up to PED_FULL_PERCENTAGE_DISTANCE
								float fDistanceModifier = 0.0f;
								if(fVehicleShareMultiplier > 1.0f)
								{
									//We want more vehicles. In this instance we are likely to find many other players with excess spaces
									//As we could have many vehicles affecting us only use full modifier when the other vehicle has 1/4 local vehicles
									fDistanceModifier += RampValue(fVehicleShareMultiplier, 1.0f, 4.0f, 0.0f, PED_FULL_PERCENTAGE_DISTANCE);

									//adjust distance modifier by our multiplier so we don't take too much when we're near the limit
									fDistanceModifier = Lerp(myVehicleShareMultiplier, fDistanceModifier, 0.0f); 
								}
								else
								{
									//We have excess vehicles. In this instance we are unlikely to have many people who want more vehicles
									//so we can give them a larger proportion of our spaces, so use full modifier when at 1/2 pool
									fDistanceModifier -= RampValue(fVehicleShareMultiplier, 0.5f, 1.0f, PED_FULL_PERCENTAGE_DISTANCE, 0.0f);

									//adjust distance modifier by our multiplier so we don't give away too much when we're near the limit
									fDistanceModifier = Lerp(1.0f / myVehicleShareMultiplier, fDistanceModifier, 0.0f); 
								}

								distance += fDistanceModifier;
							}					
						}
					}
				}

				float scaledDistance = distance - VEHICLE_FULL_PERCENTAGE_DISTANCE;
				float scaleRange     = VEHICLE_SCALED_PERCENTAGE_DISTANCE - VEHICLE_FULL_PERCENTAGE_DISTANCE;
				float share = Clamp(1.0f - (scaledDistance / scaleRange), 0.0f, 1.0f);
				playerPercentageShares[index].m_VehicleShare = share;
			}
			else
			{
				// update the vehicle share
				if(distance <= VEHICLE_FULL_PERCENTAGE_DISTANCE)
				{
					playerPercentageShares[index].m_VehicleShare = 1.0f;
				}
				else
				{
					float scaledDistance = distance - VEHICLE_FULL_PERCENTAGE_DISTANCE;
					float scaleRange     = VEHICLE_SCALED_PERCENTAGE_DISTANCE - VEHICLE_FULL_PERCENTAGE_DISTANCE;
					playerPercentageShares[index].m_VehicleShare = MAX(1.0f - (scaledDistance / scaleRange), 0.0f);
				}
			}

            objectSharePercentagesSum  += playerPercentageShares[index].m_ObjectShare;
            pedSharePercentagesSum     += playerPercentageShares[index].m_PedShare;
            vehicleSharePercentagesSum += playerPercentageShares[index].m_VehicleShare;
        }
        else if(pPlayer && pPlayer->IsMyPlayer())
        {
            // the players in range counts start at 1 to include the local player,
            // so we need to take this into account when doing this calculation
            playersSharingObjectsBeforeLocalPlayer  = numPlayersInObjectRange  - 1;
            playersSharingPedsBeforeLocalPlayer     = numPlayersInPedRange     - 1;
            playersSharingVehiclesBeforeLocalPlayer = numPlayersInVehicleRange - 1;

            BANK_ONLY(localPlayerIndex = index);
        }

        // update the number of players in range
        if(playerPercentageShares[index].m_ObjectShare > 0.0f)
            numPlayersInObjectRange++;
        if(playerPercentageShares[index].m_PedShare > 0.0f)
            numPlayersInPedRange++;
        if(playerPercentageShares[index].m_VehicleShare > 0.0f)
            numPlayersInVehicleRange++;

        BANK_ONLY(s_LastPlayerPercentageShares[index] = playerPercentageShares[index]);
    }

    // calculate the maximum ambient game objects allowed
    u32 numUnusedReservedObjects  = 0;
    u32 numUnusedReservedPeds     = 0;
    u32 numUnusedReservedVehicles = 0;

    CTheScripts::GetScriptHandlerMgr().GetNumRequiredScriptEntities(numUnusedReservedPeds, numUnusedReservedVehicles, numUnusedReservedObjects);

    u32 maxAmbientObjects  = MAX_NUM_NETOBJOBJECTS         - MIN(numUnusedReservedObjects,  MAX_NUM_NETOBJOBJECTS);
    u32 maxAmbientPeds     = MAX_NUM_LOCAL_NETOBJPEDS      - MIN(numUnusedReservedPeds,     MAX_NUM_LOCAL_NETOBJPEDS);
    u32 maxAmbientVehicles = MAX_NUM_LOCAL_NETOBJVEHICLES  - MIN(numUnusedReservedVehicles, MAX_NUM_LOCAL_NETOBJVEHICLES);

    // now calculate the local target levels for each object type based on the percentage levels
    const u32 objectsPerFullPlayerShare   = static_cast<u32>(maxAmbientObjects   / objectSharePercentagesSum);
    const u32 pedsPerFullPlayerShare      = static_cast<u32>(maxAmbientPeds      / pedSharePercentagesSum);
    const u32 vehiclesPerFullPlayerShare  = static_cast<u32>(maxAmbientVehicles  / vehicleSharePercentagesSum);

    // scale down our object share if we don't have a full share
    if(localShare < 1.0f)
    {
        ms_PopulationTargetLevels.m_TargetNumObjects   = static_cast<u32>(objectsPerFullPlayerShare  * localShare);
        ms_PopulationTargetLevels.m_TargetNumPeds      = static_cast<u32>(pedsPerFullPlayerShare     * localShare);
        ms_PopulationTargetLevels.m_TargetNumVehicles  = static_cast<u32>(vehiclesPerFullPlayerShare * localShare);
    }
    else
    {
        ms_PopulationTargetLevels.m_TargetNumObjects   = objectsPerFullPlayerShare;
        ms_PopulationTargetLevels.m_TargetNumPeds      = pedsPerFullPlayerShare;
        ms_PopulationTargetLevels.m_TargetNumVehicles  = vehiclesPerFullPlayerShare;
    }

    BANK_ONLY(s_LastPlayerPercentageShares[localPlayerIndex].m_ObjectShare  = localShare);
    BANK_ONLY(s_LastPlayerPercentageShares[localPlayerIndex].m_PedShare     = localShare);
    BANK_ONLY(s_LastPlayerPercentageShares[localPlayerIndex].m_VehicleShare = localShare);

    u32 remoteObjectAllocation    = 0;
    u32 remotePedAllocation       = 0;
    u32 remoteVehicleAllocation   = 0;

    for(PhysicalPlayerIndex index = 0; index < MAX_NUM_PHYSICAL_PLAYERS; index++)
    {
        remoteObjectAllocation   += static_cast<u32>(playerPercentageShares[index].m_ObjectShare   * objectsPerFullPlayerShare);
        remotePedAllocation      += static_cast<u32>(playerPercentageShares[index].m_PedShare      * pedsPerFullPlayerShare);
        remoteVehicleAllocation  += static_cast<u32>(playerPercentageShares[index].m_VehicleShare  * vehiclesPerFullPlayerShare);
    }

    gnetAssert(remoteObjectAllocation   < MAX_NUM_NETOBJOBJECTS);
    gnetAssert(remotePedAllocation      < MAX_NUM_LOCAL_NETOBJPEDS);
    gnetAssert(remoteVehicleAllocation  < MAX_NUM_LOCAL_NETOBJVEHICLES);

    // calculate any remainders, and allocate the objects out
    u32 objectsRemainder  = maxAmbientObjects  - (remoteObjectAllocation  + ms_PopulationTargetLevels.m_TargetNumObjects);
    u32 pedsRemainder     = maxAmbientPeds     - (remotePedAllocation     + ms_PopulationTargetLevels.m_TargetNumPeds);
    u32 vehiclesRemainder = maxAmbientVehicles - (remoteVehicleAllocation + ms_PopulationTargetLevels.m_TargetNumVehicles);

    const u32 objectIncreasePerPlayer  = (objectsRemainder  / numPlayersInObjectRange);
    const u32 pedIncreasePerPlayer     = (pedsRemainder     / numPlayersInObjectRange);
    const u32 vehicleIncreasePerPlayer = (vehiclesRemainder / numPlayersInObjectRange);

    ms_PopulationTargetLevels.m_TargetNumObjects  += objectIncreasePerPlayer;
    ms_PopulationTargetLevels.m_TargetNumPeds     += pedIncreasePerPlayer;
    ms_PopulationTargetLevels.m_TargetNumVehicles += vehicleIncreasePerPlayer;

    objectsRemainder  -= (objectIncreasePerPlayer  * numPlayersInObjectRange);
    pedsRemainder     -= (pedIncreasePerPlayer     * numPlayersInObjectRange);
    vehiclesRemainder -= (vehicleIncreasePerPlayer * numPlayersInObjectRange);

    if(gnetVerifyf(objectsRemainder < numPlayersInObjectRange, "Unexpected object remainder!") &&
        playersSharingObjectsBeforeLocalPlayer < objectsRemainder)
    {
        ms_PopulationTargetLevels.m_TargetNumObjects++;
    }

    if(gnetVerifyf(pedsRemainder < numPlayersInPedRange, "Unexpected ped remainder!") &&
        playersSharingPedsBeforeLocalPlayer < pedsRemainder)
    {
        ms_PopulationTargetLevels.m_TargetNumPeds++;
    }

    if(gnetVerifyf(vehiclesRemainder < numPlayersInVehicleRange, "Unexpected vehicle remainder!") &&
        playersSharingVehiclesBeforeLocalPlayer < vehiclesRemainder)
    {
        ms_PopulationTargetLevels.m_TargetNumVehicles++;
    }

    // update balancing target levels
    const u32 numTotalObjects           = GetTotalNumObjectsOfType(NET_OBJ_TYPE_OBJECT);
    const u32 numTotalPeds              = GetTotalNumObjectsOfType(NET_OBJ_TYPE_PED);
    const u32 numTotalVehicles          = GetTotalNumVehicles();
    const u32 balancingObjectShare      = static_cast<u32>(numTotalObjects  / objectSharePercentagesSum);
    const u32 balancingPedShare         = static_cast<u32>(numTotalPeds     / pedSharePercentagesSum);
    const u32 balancingVehicleShare     = static_cast<u32>(numTotalVehicles / vehicleSharePercentagesSum);
    const u32 balancingObjectRemainder  = numTotalObjects  - static_cast<u32>(balancingObjectShare  * objectSharePercentagesSum);
    const u32 balancingPedRemainder     = numTotalPeds     - static_cast<u32>(balancingPedShare     * pedSharePercentagesSum);
    const u32 balancingVehicleRemainder = numTotalVehicles - static_cast<u32>(balancingVehicleShare * vehicleSharePercentagesSum);

    ms_BalancingTargetLevels.m_TargetNumObjects  = static_cast<u32>(balancingObjectShare  * localShare) + balancingObjectRemainder;
    ms_BalancingTargetLevels.m_TargetNumPeds     = static_cast<u32>(balancingPedShare     * localShare) + balancingPedRemainder;
    ms_BalancingTargetLevels.m_TargetNumVehicles = static_cast<u32>(balancingVehicleShare * localShare) + balancingVehicleRemainder;

#if ENABLE_NETWORK_BOTS

    // if we have local network bots in the session we need to multiply our target numbers
    unsigned numLocalPlayers = netInterface::GetPlayerMgr().GetNumLocalPhysicalPlayers();

    if(numLocalPlayers > 1)
    {
        ms_PopulationTargetLevels.m_TargetNumObjects  *= numLocalPlayers;
        ms_PopulationTargetLevels.m_TargetNumPeds     *= numLocalPlayers;
        ms_PopulationTargetLevels.m_TargetNumVehicles *= numLocalPlayers;
        ms_PopulationTargetLevels.m_TargetNumObjects  = rage::Min(ms_PopulationTargetLevels.m_TargetNumObjects,  (u32)MAX_NUM_NETOBJOBJECTS);
        ms_PopulationTargetLevels.m_TargetNumPeds     = rage::Min(ms_PopulationTargetLevels.m_TargetNumPeds,     (u32)MAX_NUM_LOCAL_NETOBJPEDS);
        ms_PopulationTargetLevels.m_TargetNumVehicles = rage::Min(ms_PopulationTargetLevels.m_TargetNumVehicles, (u32)MAX_NUM_LOCAL_NETOBJVEHICLES);

        ms_BalancingTargetLevels.m_TargetNumObjects  *= numLocalPlayers;
        ms_BalancingTargetLevels.m_TargetNumPeds     *= numLocalPlayers;
        ms_BalancingTargetLevels.m_TargetNumVehicles *= numLocalPlayers;
        ms_BalancingTargetLevels.m_TargetNumObjects  = rage::Min(ms_BalancingTargetLevels.m_TargetNumObjects,  numTotalObjects);
        ms_BalancingTargetLevels.m_TargetNumPeds     = rage::Min(ms_BalancingTargetLevels.m_TargetNumPeds,     numTotalPeds);
        ms_BalancingTargetLevels.m_TargetNumVehicles = rage::Min(ms_BalancingTargetLevels.m_TargetNumVehicles, numTotalVehicles);
    }

#endif // ENABLE_NETWORK_BOTS

    // update the bandwidth target levels
    u32 timeSinceLastObjectDumped = fwTimer::GetSystemTimeInMilliseconds() - CNetwork::GetObjectManager().m_lastTimeObjectDumped;

    if(timeSinceLastObjectDumped > DUMPED_AMBIENT_ENTITY_TIMER)
    {
        u32 timeSinceLastBandwidthLevelIncrease = fwTimer::GetSystemTimeInMilliseconds() - ms_LastTimeBandwidthLevelsIncreased;

        if(timeSinceLastBandwidthLevelIncrease > DUMPED_AMBIENT_ENTITY_INCREASE_INTERVAL)
        {
            if(CNetwork::GetBandwidthManager().GetTimeSinceAverageFailuresNonZero() > 5000 && !CNetwork::GetObjectManager().IsOwningTooManyObjects())
            {
                IncreaseBandwidthTargetLevels();

                ms_LastTimeBandwidthLevelsIncreased = fwTimer::GetSystemTimeInMilliseconds();
            }
        }
    }

#if ENABLE_NETWORK_LOGGING

    if(oldPopulationTargetNumObjects  != ms_PopulationTargetLevels.m_TargetNumObjects ||
       oldPopulationTargetNumPeds     != ms_PopulationTargetLevels.m_TargetNumPeds    ||
       oldPopulationTargetNumVehicles != ms_PopulationTargetLevels.m_TargetNumVehicles)
    {
        NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "POPULATION_TARGET_CHANGE", "");

        CNetwork::GetObjectManager().GetLog().WriteDataValue("Population Target Objects",  "%d", ms_PopulationTargetLevels.m_TargetNumObjects);
        CNetwork::GetObjectManager().GetLog().WriteDataValue("Population Target Peds",     "%d", ms_PopulationTargetLevels.m_TargetNumPeds);
        CNetwork::GetObjectManager().GetLog().WriteDataValue("Population Target Vehicles", "%d", ms_PopulationTargetLevels.m_TargetNumVehicles);
    }
#endif // ENABLE_NETWORK_LOGGING
}

void CNetworkObjectPopulationMgr::UpdatePendingTransitionalObjects()
{
	if (PendingTransitionalObjects())
	{
		//Try to UnFlag objects previously flag
		for(unsigned idx=0; idx<MAX_NUM_TRANSITIONAL_OBJECTS; idx++)
		{
			if (NETWORK_INVALID_OBJECT_ID != ms_TransitionalPopulation[idx])
			{
				netObject* obj = NetworkInterface::GetNetworkObject(ms_TransitionalPopulation[idx]);
				if (!obj || !obj->IsPendingOwnerChange())
				{
					UnFlagObjAsTransitional(ms_TransitionalPopulation[idx]);
				}
			}
		}

		//Check for the timeout
		const unsigned curTime = sysTimer::GetSystemMsTime() | 0x01;
		if ((curTime - ms_TransitionalPopulationTimer) > WAIT_TIMER_FOR_POPULATION_TRANSITION)
		{
			NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tFORCE_REMOVE_TRANSITIONAL_OBJECT\t\t\t\t", "");

			for(unsigned idx=0; idx<MAX_NUM_TRANSITIONAL_OBJECTS; idx++)
			{
				if (NETWORK_INVALID_OBJECT_ID != ms_TransitionalPopulation[idx])
				{
					netObject* obj = NetworkInterface::GetNetworkObject(ms_TransitionalPopulation[idx]);
					if (obj)
					{
						NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tREMOVE_TRANSITIONAL_OBJECT\t\t", obj->GetLogName());
					}
					else
					{
						NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tREMOVE_TRANSITIONAL_OBJECT\t\t", "%d", ms_TransitionalPopulation[idx]);
					}

					ms_TransitionalPopulation[idx] = NETWORK_INVALID_OBJECT_ID;
				}
			}

			ms_NumTransitionalObjects = 0;
		}

		//Log the end 
		if (!PendingTransitionalObjects())
		{
			ms_NumTransitionalObjects = 0;
			NetworkLogUtils::WriteLogEvent(CNetwork::GetObjectManager().GetLog(), "\tEND_CONTROL_TRANSITIONAL_POPULATION\t\t\t\t", "");
			BANK_ONLY(NetworkDebug::UnToggleLogExtraDebugNetworkPopulation();)
		}
	}
}
