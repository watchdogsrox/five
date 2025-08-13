/****************************************
 * cargen.cpp                           *
 * ------------                         *
 *                                      *
 * car generator control for GTA3		*
 *                                      * 
 * GSW 07/06/00                         *
 *                                      *
 * (c)2000 Rockstar North               *
 ****************************************/

// Framework headers
#include "grcore/debugdraw.h"
#include "fwmaths/random.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwsys/timer.h"
#include "fwscene/stores/mapdatastore.h"
#include "fwscene/stores/staticboundsstore.h"
#include "math/vecrand.h"
#include "streaming/streamingvisualize.h"

// Game headers
#include "ai/ambient/AmbientModelSetManager.h"
#include "camera/debug/DebugDirector.h"
#include "camera/helpers/Frame.h"
#include "camera/CamInterface.h"
#include "control/replay/replay.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/debugglobals.h"
#include "debug/vectormap.h"
#include "game/clock.h"
#include "audio/vehicleaudioentity.h"
#include "game/modelIndices.h"
#include "game/zones.h"
#include "modelInfo/modelInfo.h"
#include "modelInfo/vehicleModelInfo.h"
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "pathserver/ExportCollision.h"
#include "peds/PlayerInfo.h"
#include "peds/ped.h"
#include "peds/pedintelligence.h"
#include "peds/Ped.h"
#include "peds/pedpopulation.h"
#include "peds/popcycle.h"
#include "physics/gtaArchetype.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/lod/LodMgr.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "scene/world/gameWorld.h"
#include "script/script_cars_and_peds.h"
#include "script/script_channel.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "system/controlMgr.h"
#include "Task/Vehicle/TaskCar.h"
#include "Task/Vehicle/TaskEnterVehicle.h"
#include "Task/Vehicle/TaskExitVehicle.h"
#include "Task/Default/TaskWander.h"
#include "task/Default/TaskUnalerted.h"
#include "Task/General/TaskBasic.h"
#include "Task/Scenario/info/ScenarioInfoManager.h"
#include "task/Scenario/ScenarioChaining.h"
#include "task/Scenario/ScenarioChainingTests.h"
#include "task/Scenario/ScenarioPointManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "vehicles/automobile.h"
#include "vehicles/bike.h"
#include "vehicles/boat.h"
#include "vehicles/cargen.h"
#include "vehicles/cargens_parser.h"
#include "vehicles/heli.h"
#include "vehicles/planes.h"
#include "vehicles/train.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/vehiclepopulation.h"
#include "vehicles/vehpopulation_channel.h"
#include "vehicleAi/task/TaskVehicleCruise.h"
#include "vehicleai/pathfind.h"

// network includes 
#include "network/NetworkInterface.h"
#include "network/objects/entities/NetObjPlayer.h"
#include "network/players/NetworkPlayerMgr.h"

#if __DEV
#include "Network/Objects/NetworkObjectPopulationMgr.h"
#include "Network/Objects/NetworkObjectTypes.h"
#include "diag/output.h"
#endif // __DEV

#if !__FINAL
#include "task/Scenario/ScenarioPointManager.h"		// For DumpCargenInfo().
#include "task/Scenario/ScenarioPointRegion.h"		// For DumpCargenInfo().
#endif

#define HACK_GTAV_REMOVEME	(1)
#if __STATS
EXT_PF_COUNTER(CargenVehiclesCreated);
#endif // __STATS

AI_OPTIMISATIONS()
AI_VEHICLE_OPTIMISATIONS()

atIteratablePool<CCarGenerator> CTheCarGenerators::CarGeneratorPool;
u16* CTheCarGenerators::ms_CarGeneratorIPLs;
s32	CTheCarGenerators::NumOfCarGenerators;
u16 CTheCarGenerators::m_MinAllowedScenarioCarGenerators = 0;
u16 CTheCarGenerators::m_NumScenarioCarGenerators = 0;
u8 CTheCarGenerators::m_NumScriptGeneratedCarGenerators(0);
bool	CTheCarGenerators::NoGenerate = false;
bool	CTheCarGenerators::bScriptDisabledCarGenerators = false;
bool	CTheCarGenerators::bScriptDisabledCarGeneratorsWithHeli = false;
bool	CTheCarGenerators::bScriptForcingAllCarsLockedThisFrame = false;
s32		CTheCarGenerators::m_iTryToWakeUpScenarioPoints = -1;
s32		CTheCarGenerators::m_iTryToWakeUpPriorityAreaScenarioPoints = -1;
Vector3	CTheCarGenerators::m_CarGenSwitchedOffMin[MAX_NUM_CAR_GENERATOR_SWITCHES];
Vector3	CTheCarGenerators::m_CarGenSwitchedOffMax[MAX_NUM_CAR_GENERATOR_SWITCHES];
Vector3 CTheCarGenerators::m_CarGenAreaOfInterestPosition;
float CTheCarGenerators::m_CarGenAreaOfInterestRadius = -1.f;
float CTheCarGenerators::m_CarGenAreaOfInterestPercentage = 0.8f; // 20% chance to spawn cars outside of the AOI
float CTheCarGenerators::m_CarGenScriptLockedPercentage = 0.33f;	// 2/3 unlocked
u32 CTheCarGenerators::m_ActivationTime = 0;
dev_u32 CTheCarGenerators::m_TimeBetweenScenarioActivationBoosts = 3000;
dev_float CTheCarGenerators::m_MinDistanceToPlayerForScenarioActivation  = 150.0f;
dev_float CTheCarGenerators::m_MinDistanceToPlayerForHighPriorityScenarioActivation = 90.0f;
s32	CTheCarGenerators::m_numCarGenSwitches = 0;
u32 CTheCarGenerators::m_NumScheduledVehicles = 0;
u16 CTheCarGenerators::m_scheduledVehiclesNextIndex = 0;
atFixedArray<ScheduledVehicle, MAX_SCHEDULED_CARS_AT_ONCE> CTheCarGenerators::m_scheduledVehicles;

CargenPriorityAreas* CTheCarGenerators::ms_prioAreas = NULL;
atArray<u32> CTheCarGenerators::ms_preassignedModels;
atArray<u32> CTheCarGenerators::ms_scenarioModels;

#if !__FINAL
bool	CTheCarGenerators::gs_bDisplayCarGenerators=false;
bool	CTheCarGenerators::gs_bDisplayCarGeneratorsOnVMap=false;
bool	CTheCarGenerators::gs_bDisplayCarGeneratorsScenariosOnly=false;
bool	CTheCarGenerators::gs_bDisplayCarGeneratorsPreassignedOnly=false;
float	CTheCarGenerators::gs_fDisplayCarGeneratorsDebugRange=100.0f;
bool	CTheCarGenerators::gs_bDisplayAreaOfInterest = false;

#if __BANK
// prio area editor
static char g_Pos1[256];
static char g_Pos2[256];
Vector3	g_vClickedPos[2];
bool	g_bHasPosition[2];
bool    g_bShowAreas = false;
bool    g_bEnableEditor = false;
s32     g_iAreaToShow = -1;

float	CTheCarGenerators::gs_fForceSpawnCarRadius=100.0f;
bool	CTheCarGenerators::gs_bCheckPhysicProbeWhenForceSpawn=true;
#endif // __BANK
#endif // !__FINAL


void ScheduledVehicle::Invalidate()
{
	//If we have a chain user setting here we need to release it cause teh scheduled vehicle never got created
	if (chainUser)
	{
		CScenarioChainingGraph::UnregisterPointChainUser(*chainUser);
		chainUser = NULL;
	}

	if(valid)
	{
		Assert(CTheCarGenerators::m_NumScheduledVehicles > 0);
		--CTheCarGenerators::m_NumScheduledVehicles;
	}

	valid = false;
}

CSpecialPlateHandler	CTheCarGenerators::m_SpecialPlateHandler;

void CSpecialPlateHandler::Init()
{
	m_nSpecialPlates = 0;		
	for(s32 i=0; i<MAX_SPECIAL_PLATES; i++)
	{
		m_aSpecialPlates[i].nCarIndex = -1;
		m_aSpecialPlates[i].szNumPlate[0] = 0;
	}
}

s32 CSpecialPlateHandler::Find(s32 nIndex, char* str)
{
	str[0] = 0;
	if(IsEmpty())
		return -1;
		
	for(s32 i=0; i<MAX_SPECIAL_PLATES; i++)
	{
		if(m_aSpecialPlates[i].nCarIndex == nIndex)
		{
			strcpy(str, m_aSpecialPlates[i].szNumPlate);
			return i;
		}
	}
	return -1;
}

void CSpecialPlateHandler::Add(s32 nCarIndex, char* szNumPlate)
{
	if(m_nSpecialPlates == MAX_SPECIAL_PLATES)
	{
		Assertf(FALSE, "CSpecialPlateHandler - Ran out of Special Plate Slots");
		return;
	}
	m_aSpecialPlates[m_nSpecialPlates].nCarIndex = nCarIndex;
	Assert(strlen(szNumPlate) <= CUSTOM_PLATE_NUM_CHARS);
	strcpy(m_aSpecialPlates[m_nSpecialPlates].szNumPlate, szNumPlate);
	m_nSpecialPlates++;
}

void CSpecialPlateHandler::Remove(s32 nIndex)
{
	if(nIndex < 0)
		return;

	if(m_nSpecialPlates == 0)
	{
		Assert(FALSE);
		return;
	}

	m_aSpecialPlates[nIndex].nCarIndex = -1;
	m_aSpecialPlates[nIndex].szNumPlate[0] = 0;
	if(nIndex < m_nSpecialPlates - 1)
	{
		m_aSpecialPlates[nIndex] = m_aSpecialPlates[m_nSpecialPlates - 1];
	}
	m_nSpecialPlates--;
}

void CCarGenerator::SwitchOff(void)
{
	GenerateCount = 0;
}

void CCarGenerator::SwitchOn(void)
{
	GenerateCount = INFINITE_CAR_GENERATE;
}

bool CCarGenerator::IsSwitchedOff(void) const
{
	return (GenerateCount == 0);
}


///////////////////////////////////////////////////
// CCarGenerator::PickSuitableCar
// This function picks a random car for this generator.
// The car has to physically fit in the area that the generator specifies.
///////////////////////////////////////////////////

u32 CCarGenerator::PickSuitableCar(fwModelId& trailer) const
{
	CLoadedModelGroup	CarsToChooseFrom;

	float	lengthVec1 = rage::Sqrtf(m_vec1X*m_vec1X + m_vec1Y*m_vec1Y);
	float	lengthVec2 = rage::Sqrtf(m_vec2X*m_vec2X + m_vec2Y*m_vec2Y);

	// We go through the appropriate loaded cars and put the ones that would fit within the car
	// generator in a special list.
	CarsToChooseFrom.Clear();

	if (m_CreationRule == CREATION_RULE_BOATS)
	{
		for (s32 i = 0; i < gPopStreaming.GetLoadedBoats().CountMembers(); i++)
		{
			u32 TestBoat = gPopStreaming.GetLoadedBoats().GetMember(i);
			Assert(CModelInfo::IsValidModelInfo(TestBoat));

			if (CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(TestBoat))
				continue;

			CVehicleModelInfo *pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(TestBoat)));

			if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_SPAWN_IN_CARGEN))
				continue;

			float Length = pModelInfo->GetBoundingBoxMax().y - pModelInfo->GetBoundingBoxMin().y;
			if (Length < lengthVec1)
			{
				float Width = pModelInfo->GetBoundingBoxMax().x - pModelInfo->GetBoundingBoxMin().x;
				if (Width < lengthVec2)
				{
					CarsToChooseFrom.AddMember(TestBoat);
				}
			}
		}
	}
	else
	{
		CLoadedModelGroup& loadedCars = gPopStreaming.GetAppropriateLoadedCars();
#if __BANK
		CLoadedModelGroup debugCar;
		if (gPopStreaming.GetVehicleOverrideIndex() != fwModelId::MI_INVALID)
		{
			debugCar.AddMember(gPopStreaming.GetVehicleOverrideIndex());
			loadedCars = debugCar;
		}
#endif
		s32 numCars = loadedCars.CountMembers();
		for (s32 i = 0; i < numCars; i++)
		{
			u32 testCarMI = loadedCars.GetMember(i);
			Assert(CModelInfo::IsValidModelInfo(testCarMI));

            if (CNetwork::IsGameInProgress() && !gPopStreaming.CanSpawnInMp(testCarMI))
                continue;

			if (CScriptCars::GetSuppressedCarModels().HasModelBeenSuppressed(testCarMI))
				continue;

			CVehicleModelInfo *pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(testCarMI)));

			if (pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DONT_SPAWN_IN_CARGEN))
				continue;

			float Length = pModelInfo->GetBoundingBoxMax().y - pModelInfo->GetBoundingBoxMin().y;
			if (Length < lengthVec1)
			{
				float Width = pModelInfo->GetBoundingBoxMax().x - pModelInfo->GetBoundingBoxMin().x;
				if (Width < lengthVec2)
				{
					// Don't create taxis and cop cars in random car generators
					if(	!pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EXTRAS_TAXI) &&
						!CVehicle::IsLawEnforcementVehicleModelId(fwModelId(strLocalIndex(testCarMI))) &&
						(pModelInfo->GetVehicleType() != VEHICLE_TYPE_BOAT) &&
						(pModelInfo->GetVehicleType() != VEHICLE_TYPE_HELI) )
					{
						vehPopDebugf2("[CARGEN]: Picked suitable vehicle %s (%.2f, %.2f) fits in (%.2f, %.2f)", pModelInfo->GetModelName(), Length, Width, lengthVec1, lengthVec2);

						bool addCar = false;
						switch (m_CreationRule)
						{
							case CREATION_RULE_ALL:
								addCar = true;

								// Disallow bike generation at scenarios unless specifically requested [5/15/2013 mdawe]
								if (IsScenarioCarGen())
								{
									bool isBikeOrQuadBike = pModelInfo->GetIsBike() || pModelInfo->GetIsQuadBike() || pModelInfo->GetIsAmphibiousQuad();
									addCar = !isBikeOrQuadBike;
								}
								// dissallow bicycle creation unless specifically requested by the pop group
								else if(m_popGroup.GetHash() == 0)
								{
									bool isBicycle = pModelInfo->GetIsBicycle();
									addCar = !isBicycle;									
								}
								break;
							case CREATION_RULE_ONLY_SPORTS:
								addCar = pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS);
								break;
							case CREATION_RULE_NO_SPORTS:
								addCar = !pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS);
								break;
							case CREATION_RULE_ONLY_BIG:
								addCar = pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG);
								break;
							case CREATION_RULE_NO_BIG:
								addCar = !pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_BIG);
								break;
							case CREATION_RULE_ONLY_BIKES:
								addCar = pModelInfo->GetIsBike();
								break;
							case CREATION_RULE_NO_BIKES:
								addCar = !pModelInfo->GetIsBike();
								break;
							case CREATION_RULE_ONLY_DELIVERY:
								addCar = pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DELIVERY);
								break;
							case CREATION_RULE_NO_DELIVERY:
								addCar = !pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DELIVERY);
								break;
							case CREATION_RULE_ONLY_POOR:
								addCar = pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_POOR_CAR);
								break;
							case CREATION_RULE_NO_POOR:
								addCar = !pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_POOR_CAR);
								break;
							case CREATION_RULE_CAN_BE_BROKEN_DOWN:
								addCar = CScenarioManager::CanVehicleModelBeUsedForBrokenDownScenario(*pModelInfo);
								break;
							default:
								Assert(0);
								break;
						}

						if (addCar)
						{
							CarsToChooseFrom.AddMember(testCarMI);
						}
					}
				}
			}
		}
	}
	CarsToChooseFrom.RemoveModelsThatHaveHitMaximumNumber();

	bool useWantedGroup = false;
	CLoadedModelGroup wantedGroup = CarsToChooseFrom;
	if (m_popGroup.GetHash() != 0)
	{
		u32 wantedGroupIndex = 0;
		bool foundGroup = CPopCycle::GetPopGroups().FindVehGroupFromNameHash(m_popGroup.GetHash(), wantedGroupIndex);
		if (foundGroup)
		{
			wantedGroup.RemoveModelsNotInGroup(wantedGroupIndex, false);
			useWantedGroup = wantedGroup.CountMembers() > 0;
		}
		else
		{
			const int modelSetIndex = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(CAmbientModelSetManager::kVehicleModelSets, m_popGroup.GetHash());
			if(modelSetIndex >= 0)
			{
				wantedGroup.RemoveModelsNotInModelSet(CAmbientModelSetManager::kVehicleModelSets, modelSetIndex);

				// In the case of a population group above, we do this
				//	useWantedGroup = wantedGroup.CountMembers() > 0;
				// but I'm not sure that that makes sense here - if they specify a vehicle model set, we probably
				// only want to spawn from that set, right?
				useWantedGroup = true;
			}
		}
	}

	u32 returnVal = fwModelId::MI_INVALID;

#if HACK_GTAV_REMOVEME
	const Vec3V vCentrePos(m_centreX, m_centreY, m_centreZ);
	static spdSphere veniceBeachCarPark( Vec3V(-1654.3f, -895.9f, 9.0f), ScalarV(75.0f) );
	static spdSphere airportCarPark( Vec3V(-574.7f, -2158.5f, 9.0f), ScalarV(75.0f) );
	const bool bCarParkHackUseRandomModels = (veniceBeachCarPark.ContainsPoint(vCentrePos) || airportCarPark.ContainsPoint(vCentrePos));
#else
	const bool bCarParkHackUseRandomModels = false;
#endif

	const Vector3 cargenPos(m_centreX, m_centreY, m_centreZ);
	CPopCycle::FindNewCarModelIndex(&returnVal, useWantedGroup ? wantedGroup : CarsToChooseFrom, &cargenPos, true, bCarParkHackUseRandomModels);

	// check if trailer can be attached
	fwModelId chosenCar((strLocalIndex(returnVal)));
	if (chosenCar.IsValid())
	{
		CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(chosenCar);
		if (vmi)
		{
			fwModelId trailerId = vmi->GetRandomLoadedTrailer();
			if (trailerId.IsValid() && (!CNetwork::IsGameInProgress() || gPopStreaming.CanSpawnInMp(trailerId.GetModelIndex())))
			{
				CVehicleModelInfo* tmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(trailerId);
				if (tmi)
				{
					float totalLength = (vmi->GetBoundingBoxMax().y - vmi->GetBoundingBoxMin().y) + (tmi->GetBoundingBoxMax().y - tmi->GetBoundingBoxMin().y);
					float vehicleWidth = vmi->GetBoundingBoxMax().x - vmi->GetBoundingBoxMin().x;
					float trailerWidth = tmi->GetBoundingBoxMax().x - tmi->GetBoundingBoxMin().x;
					float largestWidth = vehicleWidth > trailerWidth ? vehicleWidth : trailerWidth;

					if (totalLength <= lengthVec1 && largestWidth <= lengthVec2)
						trailer = trailerId;
				}
			}
		}
	}

	return (returnVal);
}

bool CCarGenerator::IsUsed(u16 ipl) const
{ 
	return (IsUsed() && (ipl == m_ipl) && !IsScenarioCarGen()); 
}

bool CCarGenerator::IsUsed() const
{
	u32 index = CTheCarGenerators::CarGeneratorPool.GetIndex((void*)this);
	return index < CTheCarGenerators::CarGeneratorPool.GetSize() // test if the pointer returns a valid index into the pool
		? CTheCarGenerators::CarGeneratorPool.GetSlot(index) != NULL : false; // will return null if this car gen isn't actually used
}

bool CCarGenerator::CheckIfReadyForSchedule(bool bIgnorePopulation, bool& carModelShouldBeRequested, fwModelId& carToUse, fwModelId& trailerToUse)
{
	if (!CheckHasSpaceToSchedule())
	{
		//failure registered in function.
		return false;
	}

	if (!CheckIsActiveForGen())
	{
		//failure registered in function.
		return false;
	}

	if (!bIgnorePopulation)
	{
		if (!CheckPopulationHasSpace())
		{
			//failure registered in function.
			return false;
		}
	}
	
	// If this is a car generator for a scenario, give the scenario code a chance to reject spawning.
	bool bScenarioCarGen = IsScenarioCarGen();

	if(bScenarioCarGen)
	{
		if(!CheckScenarioConditionsA())
		{
			//failure registered in function.
			return false;
		}
	}

	if (!PickValidModels(carToUse, trailerToUse, carModelShouldBeRequested))
	{
		//failure registered in function.
		return false;
	}

	if(bScenarioCarGen)
	{
		if(!CheckScenarioConditionsB(carToUse, trailerToUse))
		{
			//failure registered in function.
			return false;
		}
	}

	if(NetworkInterface::IsGameInProgress())
	{
		if( !CheckNetworkObjectLimits(carToUse) || !CVehicle::IsTypeAllowedInMP(carToUse.GetModelIndex()) )
		{
			//failure registered in function.
			return false;
		}
	}

	return true;
}

///////////////////////////////////////////////////
// CTheCarGenerators::ScheduleVehsForGenerationIfInRange
//
// RETURNS true if car was successfully scheduled for generation
///////////////////////////////////////////////////

bool CCarGenerator::ScheduleVehsForGenerationIfInRange(bool bIgnorePopulation, bool bVeryClose, bool& bCarModelPicked)
{
	fwModelId carToUse;
	fwModelId trailerToUse;
	bool carModelShouldBeRequested = false;

	const bool readyForSchedule = CheckIfReadyForSchedule(bIgnorePopulation, carModelShouldBeRequested, carToUse, trailerToUse);

	bCarModelPicked = carToUse.IsValid();

	if (!readyForSchedule)
		return false;

	// If this is set, we have already determined that the correct model is not available and would potentially have
	// to be streamed in.
	if(carModelShouldBeRequested)
	{
		// Check if it's already counted either as a preassigned model or a scenario vehicle, if so,
		// we don't want to add it anywhere again.
		bool alreadyCounted = (CTheCarGenerators::GetScenarioModels().Find(carToUse.GetModelIndex()) >= 0)
				|| (CTheCarGenerators::GetPreassignedModels().Find(carToUse.GetModelIndex()) >= 0);

		if(!alreadyCounted)
		{
			if (m_bPreAssignedModel && IsOwnedByMapData())
			{
				Assert(CTheCarGenerators::GetPreassignedModels().GetCount() < CPopCycle::GetMaxCargenModels());
				CTheCarGenerators::GetPreassignedModels().PushAndGrow(carToUse.GetModelIndex(), 1);
			}
			else if(IsScenarioCarGen())
			{
				Assert(IsHighPri() || CTheCarGenerators::GetScenarioModels().GetCount() < CPopCycle::GetMaxScenarioVehicleModels());
				CTheCarGenerators::GetScenarioModels().PushAndGrow(carToUse.GetModelIndex());
			}
		}

        CModelInfo::RequestAssets(carToUse, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);

		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Model not streamed in index=%d", carToUse.GetModelIndex());
		BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehModel));

		return false;
	}

	// do bbox test with dynamic objects to make sure we're not creating on top of another car
	bool	bAllowedToSwitchOffCarGenIfSomethingOnTop = !m_bHighPriority;
	if (!m_bOwnedByScript && !IsScenarioCarGen())	// Don't let high priority vehicle scenarios deactivate if something is temporarily in the way.
	{
		if (NetworkInterface::IsGameInProgress())
		{
			bAllowedToSwitchOffCarGenIfSomethingOnTop = true;
		}
		else
		{
			if (bVeryClose)
			{
				bAllowedToSwitchOffCarGenIfSomethingOnTop = true;
			}
		}
	}
	
	Matrix34 CreationMatrix;
	BuildCreationMatrix(&CreationMatrix, carToUse.GetModelIndex());

	// we always probe later before placing the car, so we can safely skip testing here.
	/*if (!DoSyncProbeForBlockingObjects(carToUse, CreationMatrix))
	{
		// There is something on top of the generator.
		// Keep trying to create vehicle next frame unless we're in a network game. In network games we ended up with piles of boats.
		if(bAllowedToSwitchOffCarGenIfSomethingOnTop)
		{
			m_bReadyToGenerateCar = false;
		}

		//failure registered in function.
		return false;
	}
	*/
	return InsertVehForGen(carToUse, trailerToUse, CreationMatrix, bAllowedToSwitchOffCarGenIfSomethingOnTop) != NULL;
}

bool CCarGenerator::CheckPopulationHasSpace()
{
	if (!m_bHighPriority || m_bTempHighPrio)
	{
		// Make sure we stick to the limit of parked cars.
		s32	MaxNumParkedCars = CPopCycle::GetCurrentMaxNumParkedCars();

		if (CVehiclePopulation::ms_overrideNumberOfParkedCars >= 0)
		{
			MaxNumParkedCars = CVehiclePopulation::ms_overrideNumberOfParkedCars;
		}

		s32	TotalCarsOnMap = CVehiclePopulation::GetTotalVehsOnMap();

		Assertf(CVehiclePopulation::ms_maxNumberOfCarsInUse, "CVehiclePopulation::ms_maxNumberOfCarsInUse not set as expected.");
		if (TotalCarsOnMap >= CVehiclePopulation::ms_maxNumberOfCarsInUse)		// Have we hit absolute max number?
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "We have hit the absolute max number %d : %d.", TotalCarsOnMap, CVehiclePopulation::ms_maxNumberOfCarsInUse);
			BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehPopLimits));
			BANK_ONLY(m_failCode = FC_TOO_MANY_CARS);
			m_bReadyToGenerateCar = false;
			return false;
		}

		// Will the population allow for another vehicle?
		if (CVehiclePopulation::ms_numParkedCars >= MaxNumParkedCars && (!m_bHighPriority || MaxNumParkedCars == 0) && !IsScenarioCarGen())
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "We have hit the absolute parked max number %d : %d.", CVehiclePopulation::ms_numParkedCars, MaxNumParkedCars);
			BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehPopLimits));
			BANK_ONLY(m_failCode = FC_TOO_MANY_PARKED_CARS);
			m_bReadyToGenerateCar = false;
			return false;
		}

		// check low prio cargen and low prio budget.
		if (m_bLowPriority)
		{
			s32 maxNumLowPrioCars = CPopCycle::GetCurrentMaxNumLowPrioParkedCars();
			if (CVehiclePopulation::ms_numLowPrioParkedCars >= maxNumLowPrioCars)
			{
				NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "We have hit the max low prio car number %d : %d.", CVehiclePopulation::ms_numLowPrioParkedCars, maxNumLowPrioCars);
				BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehPopLimits));
				BANK_ONLY(m_failCode = FC_TOO_MANY_LOWPRIO_PARKED_CARS);
				m_bReadyToGenerateCar = false;
				return false;
			}
		}

		//check if we're approaching the (reduced) number of local vehicles we can spawn and we want (and are able) to spawn more ambient
		//we'd prefer if ambient vehicles used the last spaces rather than parked vehicles
		if(NetworkInterface::IsGameInProgress())
		{
			static u32 iNumVehiclesBeforeLimit = 40;
			static u32 iSpacesReservedForAmbientVehicles = 10;
			u32 TargetNumLocalVehicles  = CNetworkObjectPopulationMgr::GetLocalTargetNumVehicles();
			u32 TotalNumLocalVehicles   = CNetworkObjectPopulationMgr::GetNumCachedLocalVehicles();
			if(TargetNumLocalVehicles < iNumVehiclesBeforeLimit && CVehiclePopulation::GetTotalVehsOnMap() < iNumVehiclesBeforeLimit &&
			   TargetNumLocalVehicles - TotalNumLocalVehicles < iSpacesReservedForAmbientVehicles && CVehiclePopulation::GetAverageLinkDisparity() > 5.0f )
			{
				NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Low desired local vehicle limits %d / %d.", TotalNumLocalVehicles, TargetNumLocalVehicles);
				BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehPopLimits));
				BANK_ONLY(m_failCode = FC_CARGEN_LOW_LOCAL_POP_LIMIT);
				m_bReadyToGenerateCar = false;
				return false;
			}
		}
	}
	else
	{
		//check if we want no parked cars at all
		s32	MaxNumParkedCars = CPopCycle::GetCurrentMaxNumParkedCars();
		if (CVehiclePopulation::ms_overrideNumberOfParkedCars >= 0)
		{
			MaxNumParkedCars = CVehiclePopulation::ms_overrideNumberOfParkedCars;
		}

		if(MaxNumParkedCars == 0 && !IsScenarioCarGen())
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "We have hit the absolute parked max number %d : %d.", CVehiclePopulation::ms_numParkedCars, MaxNumParkedCars);
			BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehPopLimits));
			BANK_ONLY(m_failCode = FC_TOO_MANY_PARKED_CARS);
			m_bReadyToGenerateCar = false;
			return false;
		}
	}

	return true;
}

bool CCarGenerator::CheckIsActiveForGen()
{
	// Low priority car generators can be set to be active during day or night only
	if ((!m_bHighPriority) && (!m_bReservedForPoliceCar) && (!m_bReservedForFiretruck) && (!m_bReservedForAmbulance) )
	{
		if ((!m_bActiveDuringDay) && CTheCarGenerators::ItIsDay())
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Not Active during the Day.");
			BANK_ONLY(m_failCode = FC_NOT_ACTIVE_DURING_DAY);
			m_bReadyToGenerateCar = false;
			return false;
		}
		if ((!m_bActiveAtNight) && !CTheCarGenerators::ItIsDay())
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Not Active during the Night.");
			BANK_ONLY(m_failCode = FC_NOT_ACTIVE_DURING_NIGHT);
			m_bReadyToGenerateCar = false;
			return false;
		}
	}

	// The script can disable the car generators. High priority ones and emergency services still work.
	if (CTheCarGenerators::bScriptDisabledCarGenerators)
	{
		if (!m_bHighPriority || m_bTempHighPrio)
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Script Disabled Car Generators.");
			BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioCarGenScriptDisabled));
			BANK_ONLY(m_failCode = FC_SCRIPT_DISABLED);
			m_bReadyToGenerateCar = false;
			return false;
		}
	}

	// The script can disable generators with helis. This will only work during network games.
	if (CTheCarGenerators::bScriptDisabledCarGeneratorsWithHeli && NetworkInterface::IsGameInProgress())
	{
		if (CModelInfo::IsValidModelInfo(VisibleModel) && ((CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(VisibleModel))))->GetVehicleType() == VEHICLE_TYPE_HELI)
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Script Disabled Car Generators with heli.");
			BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioCarGenScriptDisabled));
			BANK_ONLY(m_failCode = FC_SCRIPT_DISABLED);
			m_bReadyToGenerateCar = false;
			return false;
		}
	}

	return true;
}

bool CCarGenerator::PickValidModels(fwModelId& car_Out, fwModelId& trailer_Out, bool& carModelShouldBeRequested_Out)
{
	car_Out.SetModelIndex(VisibleModel);

	if (m_bStoredVisibleModel || !CModelInfo::IsValidModelInfo(car_Out.GetModelIndex()))
	{
		// if visible model isnt -1 then a previous model has been generated for
		// this car generator. Check if that model is in memory and reuse
		if(car_Out.IsValid() && CModelInfo::HaveAssetsLoaded(car_Out) && !gPopStreaming.IsCarDiscarded(car_Out.GetModelIndex()))
		{
			// All good ... move along ... 
		}
		else
		{
			car_Out.SetModelIndex(PickSuitableCar(trailer_Out));

			if (!CModelInfo::IsValidModelInfo(car_Out.GetModelIndex()))
			{
				NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Its not a valid Model info index=%d", car_Out.GetModelIndex());

				BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehModel));
				BANK_ONLY(m_failCode = FC_NO_VALID_MODEL;)
					return false;
			}

			if (!NetworkInterface::IsGameInProgress())	// Don't do this in network games as it causes problems when that car is not wanted anymore.
			{
				VisibleModel = car_Out.GetModelIndex();

                // FA: this flag is used to indicate the cargen has been assigned a vehicle model and even if the vehicle
                // is removed, it should always spawn this vehicle type to appear persistent. this can cause issues
                // with the "closest spawn distance" however, and cause several identical vehicles to spawn very close
                // to each other. i've chosen to only use this behavior for scenario cargens (mostly cause i don't
                // know the side effects of removing it). the persistent appearence doesn't really work anyway. if the cargen
                // gets streamed out the flag is lost and a new car will be assigned.
				m_bStoredVisibleModel = !IsOwnedByMapData();
			}
			car_remap1 = 0xff;
			car_remap2 = 0xff;
			car_remap3 = 0xff;
			car_remap4 = 0xff;
			car_remap5 = 0xff;
			car_remap6 = 0xff;
		}
	}
	else
	{
		Assertf( ((CBaseModelInfo*)CModelInfo::GetBaseModelInfo(car_Out))->GetModelType() == MI_TYPE_VEHICLE, "CCarGenerator::ScheduleVehsForGenerationIfInRange - %s is not a car model %f %f %f", CModelInfo::GetBaseModelInfoName(car_Out), 
			m_centreX, m_centreY, m_centreZ);

		if	(!CModelInfo::HaveAssetsLoaded(car_Out))
		{
			// vehicle models streamed are strictly controlled in network games, we don't want to force load
			// models for car generators
			// - Exception: high priority scenarios are allowed to do it.

			bool canRequestNewModel = false;
			if(!NetworkInterface::IsGameInProgress())
			{
				if(IsScenarioCarGen())
				{
					canRequestNewModel = (IsHighPri() || CTheCarGenerators::GetScenarioModels().GetCount() < CPopCycle::GetMaxScenarioVehicleModels());
				}
				else if(m_bPreAssignedModel && IsOwnedByMapData())
				{
					canRequestNewModel = (CTheCarGenerators::GetPreassignedModels().GetCount() < CPopCycle::GetMaxCargenModels());
				}
				else
				{
					canRequestNewModel = true;
				}
			}
			else
			{
				// If in island heist, allow as long as we have car gen models available.
				if(ThePaths.bStreamHeistIslandNodes)
				{
					canRequestNewModel = (CTheCarGenerators::GetPreassignedModels().GetCount() < CPopCycle::GetMaxCargenModels());
				}
			}

			if(canRequestNewModel || (m_bHighPriority && IsScenarioCarGen()))
			{
				// Now, we just make a note that we don't have the vehicle model streamed in, but that
				// we should request it. It's too early to do here though, because we don't yet know
				// if all other scenario conditions are fulfilled.
				carModelShouldBeRequested_Out = true;
			}
			else
			{
				NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Model not streamed in index=%d", car_Out.GetModelIndex());
				BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehModel));
				BANK_ONLY(m_failCode = FC_CANT_REQUEST_NEW_MODEL;)

				return false;
			}
		}
	}

	if (CVehicle::IsLawEnforcementCarModelId(car_Out) && !CPedPopulation::GetAllowCreateRandomCops())
	{
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Don't spawn cops");
		BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_dontSpawnCops));
		BANK_ONLY(m_failCode = FC_DONT_SPAWN_COPS);
		return false;
	}

	// If this is a car generator for a scenario, give the scenario code a chance to reject the trailer.
	if(IsScenarioCarGen())
	{
		// Let CScenarioManager fill in TrailerModelToUse, or reset it if it doesn't want a trailer.
		if(!CScenarioManager::GetCarGenTrailerToUse(*this, trailer_Out))
		{
			trailer_Out.Invalidate();
		}
	}
	else
	{
		trailer_Out.Invalidate();
	}

	return true;
}

bool CCarGenerator::CheckScenarioConditionsA()
{
	Assert(IsScenarioCarGen());

	CScenarioManager::CarGenConditionStatus status = CScenarioManager::CheckCarGenScenarioConditionsA(*this);
	if(status != CScenarioManager::kCarGenCondStatus_Ready)
	{
		// Note: intentionally no call to RegisterSpawnFailure() here - CheckCarGenScenarioConditionsA()
		// should report failures internally.

		BANK_ONLY(m_failCode = FC_REJECTED_BY_SCENARIO);
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Rejected by scenario code.");
		if(status == CScenarioManager::kCarGenCondStatus_Failed)
		{
			// If we failed completely, stop trying until leaving and getting back in range.
			// In the kCarGenCondStatus_NotReady case, we may have already requested peds to stream
			// in and are waiting for those to be ready.
			if(!m_bHighPriority)
			{
				m_bReadyToGenerateCar = false;
			}
		}
		return false;
	}

	return true;
}

bool CCarGenerator::CheckScenarioConditionsB(const fwModelId& carToUse, const fwModelId& trailerToUse )
{
	Assert(IsScenarioCarGen());

	CScenarioManager::CarGenConditionStatus status = CScenarioManager::CheckCarGenScenarioConditionsB(*this, carToUse, trailerToUse);
	if(status != CScenarioManager::kCarGenCondStatus_Ready)
	{
		// Note: intentionally no call to RegisterSpawnFailure() here - CheckCarGenScenarioConditionsB()
		// should report failures internally.

		BANK_ONLY(m_failCode = FC_REJECTED_BY_SCENARIO);
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Rejected by scenario code.");
		if(status == CScenarioManager::kCarGenCondStatus_Failed)
		{
			// If we failed completely, stop trying until leaving and getting back in range.
			// In the kCarGenCondStatus_NotReady case, we may have already requested peds to stream
			// in and are waiting for those to be ready.
			if(!m_bHighPriority)
			{
				m_bReadyToGenerateCar = false;
			}
		}
		return false;
	}

	return true;
}

bool CCarGenerator::CheckNetworkObjectLimits(const fwModelId &carToUse)
{
	u32 numRequiredPeds    = 0;
	u32 numRequiredCars    = 1;
	u32 numRequiredObjects = 0;

	bool restorePopulationLimits = false;

	// Don't do this thing requiring additional people in police cars if we're spawning
	// at a scenario. Technically, we should probably ask CScenarioManager how many guys
	// we're planning on putting in/around the car, but at least this way, spawning
	// police vehicle scenarios shouldn't be harder than spawning non-police scenarios.
	if(!IsScenarioCarGen())
	{
		CVehicleModelInfo* vmi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(carToUse);
		if ( vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_LAW_ENFORCEMENT) )
		{
			numRequiredPeds = CVehiclePopulation::GetNumRequiredPoliceVehOccupants(carToUse.GetModelIndex());
		}
	}
	else
	{
		int numRequiredPedsForScenario = 0;
		int numRequiredVehiclesForScenario = 0;
		int numRequiredObjectsForScenario = 0;
		CScenarioManager::GetCarGenRequiredObjects(*this, numRequiredPedsForScenario, numRequiredVehiclesForScenario, numRequiredObjectsForScenario);
		numRequiredPeds += numRequiredPedsForScenario;
		numRequiredCars += numRequiredVehiclesForScenario;
		numRequiredObjects += numRequiredObjectsForScenario;

		if(m_bHighPriority)
		{
			// Note: this must be restored further down.
			CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(false);
			restorePopulationLimits = true;
		}
	}

	CNetworkObjectPopulationMgr::eVehicleGenerationContext eOldGenContext = CNetworkObjectPopulationMgr::GetVehicleGenerationContext();
	CNetworkObjectPopulationMgr::SetVehicleGenerationContext(m_bHighPriority ? CNetworkObjectPopulationMgr::VGT_HighPriorityCarGen : CNetworkObjectPopulationMgr::VGT_CarGen);

	bool canCreateCar = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredCars, numRequiredObjects, 0, 0, false);
	if(!canCreateCar && IsScenarioCarGen() && m_bHighPriority)
	{
		bool failed = false;

		// Try to free up peds as needed, one at a time. Would be nice to know how many we don't have
		// space for and tell TryToMakeSpaceForObject() to free up that many in one go, but
		// that would require some interface changes.
		for(int numPeds = 1; numPeds <= numRequiredPeds; numPeds++)
		{
			if(!NetworkInterface::CanRegisterObjects(numPeds, 0, 0, 0, 0, false))
			{
				failed = true;
				break;
			}
		}

		// If we succeeded with the peds, now try to free up vehicles.
		if(!failed)
		{
			// Note: we seem to have different types of network objects for different types of vehicles,
			// so this code might need to be modified to properly support vehicles other than automobiles.
			for(int numVehicles = 1; numVehicles <= numRequiredCars; numVehicles++)
			{
				if(!NetworkInterface::CanRegisterObjects(numRequiredPeds, numVehicles, 0, 0, 0, false))
				{
					failed = true;
					break;
				}
			}
		}

		// We might never need objects, so for now, I haven't bothered to implement the support for freeing them,
		// but if needed, it can be done like for peds and vehicles above.
		Assertf(numRequiredObjects == 0, "Object spawning through vehicle scenarios needs support for freeing up network objects.");

		if(!failed)
		{
			// Do a final check and see if we were able to recover.
			canCreateCar = NetworkInterface::CanRegisterObjects(numRequiredPeds, numRequiredCars, numRequiredObjects, 0, 0, false);
		}
	}

	if(!canCreateCar)
	{
#if __DEV
		if(!CNetworkObjectPopulationMgr::CanRegisterLocalObjectsOfType(NET_OBJ_TYPE_AUTOMOBILE, numRequiredCars, false))
		{
			NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR","PossiblyGenerateVeh", "Failed:", "Not enough vehicles.");
		}
#endif // __DEV
		BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_notAllowdByNetwork));
	}

	if(restorePopulationLimits)
	{
		CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(true);
	}	

	CNetworkObjectPopulationMgr::SetVehicleGenerationContext(eOldGenContext);

	return canCreateCar;
}

bool CCarGenerator::CheckHasSpaceToSchedule()
{
	// if queue is full we ignore the request since we have cars coming
	if (CTheCarGenerators::GetNumQueuedVehs() >= CTheCarGenerators::m_scheduledVehicles.GetMaxCount())
	{
		return false;
	}

	if(CVehicle::GetPool()->GetNoOfFreeSpaces() < 7 || CPed::GetPool()->GetNoOfFreeSpaces() < 3)
	{
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Too few slots left in vehicle or ped pool");
		BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_poolOutOfSpace));
		return false;
	}

	return true;
}

bool CCarGenerator::DoSyncProbeForBlockingObjects(const fwModelId &carToUse, const Matrix34& creationMatrix, const CVehicle* excludeVeh)
{
	CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(carToUse);
	Assert(pModel);
	Assertf(pModel->GetFragType(), "Trying to create vehicle of type '%s' with no frag type", pModel->GetModelName());
	if (!pModel->GetFragType())
	{
		NOTFINAL_ONLY(NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Trying to create vehicle of type '%s' with no frag type", pModel->GetModelName());)
		BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehModel));
		return false;
	}

	phBound *bound = (phBound *)pModel->GetFragType()->GetPhysics(0)->GetCompositeBounds();
	int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;
	Vector3 vecExtendBoxMin(0.0f,0.0f,-2.0f); // extend bbox check 2m down (and 2m up)

	WorldProbe::CShapeTestBoundingBoxDesc bboxDesc;
	bboxDesc.SetBound(bound);
	bboxDesc.SetTransform(&creationMatrix);
	bboxDesc.SetIncludeFlags(nTestTypeFlags);
	bboxDesc.SetExtensionToBoundingBoxMin(vecExtendBoxMin);
	bboxDesc.SetExtensionToBoundingBoxMax(VEC3_ZERO);

	// Pickups come with a large sphere around them, for detecting if the player is close enough
	// to pick them up. If we are not careful, they would be included in this check and possibly
	// prevent spawning. A more proper fix would be to iterate over the results (which is now
	// supported by the underlying test, see IsParkingSpaceFreeFromObstructions()), and disregard
	// only the pickup sphere without disregarding the whole object.
	bboxDesc.SetExcludeTypeFlags(ArchetypeFlags::GTA_PICKUP_TYPE);

    if (excludeVeh)
        bboxDesc.SetExcludeEntity(excludeVeh);

	if(WorldProbe::GetShapeTestManager()->SubmitTest(bboxDesc))
	{
#if __BANK
		m_failCode = FC_CARGEN_PROBE_FAILED;
#endif // __BANK

		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "There is something on top of the generator");
		BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_spawnCollisionFail));
		return false;
	}

	return true;
}

ScheduledVehicle* CCarGenerator::InsertVehForGen(const fwModelId& carToUse, const fwModelId& trailerToUse, const Matrix34& CreationMatrix, const bool bAllowedToSwitchOffCarGenIfSomethingOnTop, const bool forceSpawn/* = false*/)
{
	Assertf(CTheCarGenerators::GetNumQueuedVehs() < CTheCarGenerators::m_scheduledVehicles.GetMaxCount(), "Scheduled Vehicles queue is full %d", CTheCarGenerators::m_scheduledVehicles.GetCount());

	s32 firstSlot = -1;
	for (s32 i = 0; i < CTheCarGenerators::m_scheduledVehicles.GetCount(); ++i)
	{
		if (!CTheCarGenerators::m_scheduledVehicles[i].valid)
		{
			firstSlot = i;
			break;
		}
	}
	if (firstSlot == -1)
	{
		if (CTheCarGenerators::m_scheduledVehicles.GetCount() >= CTheCarGenerators::m_scheduledVehicles.GetMaxCount())
		{
            Assertf(false, "Trying to generate vehicle with no room in the schedule array!");
            return NULL;
		}

		CTheCarGenerators::m_scheduledVehicles.Append();
		firstSlot = CTheCarGenerators::m_scheduledVehicles.GetCount() - 1;
	}
	ScheduledVehicle& newVeh = CTheCarGenerators::m_scheduledVehicles[firstSlot];
	newVeh.Invalidate();
	newVeh.valid = true;

	++CTheCarGenerators::m_NumScheduledVehicles;
	Assert(CTheCarGenerators::m_NumScheduledVehicles <= CTheCarGenerators::m_scheduledVehicles.GetMaxCount());

#if 0 // async sphere test
	if (bAsyncProbForBlockingObjects)
	{
		// NOTE: this test has not been enabled for a very long time ... this will need to be reworked a bit to enable this functionality if it is desired...
		//	the code here has been left as a reference, but could just as easily be removed to be implemented later if the functionality is desired.
		CBaseModelInfo* pModel = CModelInfo::GetBaseModelInfo(carToUse);
		Assert(pModel);
		Assertf(pModel->GetFragType(), "Trying to create vehicle of type '%s' with no frag type", pModel->GetModelName());
		if (!pModel->GetFragType())
		{
			NOTFINAL_ONLY(NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Failed:", "Trying to create vehicle of type '%s' with no frag type", pModel->GetModelName());)
				BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehModel));
			return NULL;
		}

		phBound *bound = (phBound *)pModel->GetFragType()->GetPhysics(0)->GetCompositeBounds();
		int nTestTypeFlags = ArchetypeFlags::GTA_BOX_VEHICLE_TYPE|ArchetypeFlags::GTA_VEHICLE_TYPE|ArchetypeFlags::GTA_PED_TYPE|ArchetypeFlags::GTA_OBJECT_TYPE;

		WorldProbe::CShapeTestSphereDesc sphereDesc;
		sphereDesc.SetResultsStructure(&newVeh.collisionTestResult);
		sphereDesc.SetSphere(VEC3V_TO_VECTOR3(bound->GetWorldCentroid(RCC_MAT34V(CreationMatrix))), bound->GetRadiusAroundCentroid());
		sphereDesc.SetIncludeFlags(nTestTypeFlags);
		WorldProbe::GetShapeTestManager()->SubmitTest(sphereDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

		if (newVeh.collisionTestResult.GetResultsStatus() == WorldProbe::SUBMISSION_FAILED)
		{
			Assertf(false, "Failed to submit world probe for cargen");
			return NULL;
		}

		newVeh.isAsync = true;
	}
	else
#endif
	{
		newVeh.isAsync = false;
	}

	newVeh.isAsync = false;
	newVeh.carGen = this;
	newVeh.carModelId = carToUse;
	newVeh.trailerModelId = trailerToUse;
	newVeh.creationMatrix = CreationMatrix;
	newVeh.allowedToSwitchOffCarGenIfSomethingOnTop = bAllowedToSwitchOffCarGenIfSomethingOnTop;
	newVeh.asyncHandle = INVALID_ASYNC_ENTRY;
	newVeh.vehicle = NULL;
	newVeh.pDestInteriorProxy = NULL;
	newVeh.destRoomIdx = -1;
	newVeh.preventEntryIfNotQualified = m_bPreventEntryIfNotQualified;
	newVeh.forceSpawn = forceSpawn; //allows spawn to be considered as though they should succeed in almost all cases ... (ie debug spawning)
	newVeh.needsStaticBoundsStreamingTest = (!IsDistantGroundScenarioCarGen() && !m_bSkipStaticBoundsTest); //distant ground scenario cargens can be created without collision being loaded around them
	newVeh.addToGameWorld = true;

	// make sure these models don't get streamed out while we have the cars scheduled
	if (newVeh.carModelId.IsValid())
	{
		CBaseModelInfo* carMi = CModelInfo::GetBaseModelInfo(newVeh.carModelId);
		carMi->AddRef();
	}
	if (newVeh.trailerModelId.IsValid())
	{
		CBaseModelInfo* trailerMi = CModelInfo::GetBaseModelInfo(newVeh.trailerModelId);
		trailerMi->AddRef();
	}

	// At this point we have scheduled a vehicle to be created ...
	//  if this is a scenario cargen then we need to attempt to hold the chain
	//  if the point we are spawning at is in a chain ... 
	Assert(!newVeh.chainUser);//should be null
	if(IsScenarioCarGen())
	{
		CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*this);
		if (Verifyf(pt, "No point found for car generator") && pt->IsChained())
		{
			// Only register a chain user if this is not a clustered point, otherwise it's
			// the cluser code's responsibility to do so.
			if(!pt->IsInCluster())
			{
				bool allowOverflow = CScenarioManager::AllowExtraChainedUser(*pt);
				newVeh.chainUser = CScenarioChainingGraph::RegisterChainUserDummy(*pt, allowOverflow);
			}
		}
	}

	// Make sure we're not somehow already scheduled, and remember that we will be now.
	Assert(!m_bScheduled);
	m_bScheduled = true;

	// don't try to generate vehicles here while we have one scheduled
	m_bReadyToGenerateCar = false;

#if __BANK
	m_failCode = FC_SCHEDULED;
#endif // __BANK

	return &newVeh;
}

/////////////////////////////////////////////////////////
// CCarGenerator::BuildCreationMatrix
/////////////////////////////////////////////////////////

void CCarGenerator::BuildCreationMatrix(Matrix34 *pCreationMatrix, u32 MI) const
{
	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(MI)));

	pCreationMatrix->a = Vector3(m_vec2X, m_vec2Y, 0.0f);
	pCreationMatrix->b = Vector3(m_vec1X, m_vec1Y, 0.0f);
	pCreationMatrix->c = Vector3(0.0f, 0.0f, 1.0f);
	pCreationMatrix->d = Vector3(m_centreX, m_centreY, m_centreZ);

	bool bDoRandomization = true;
	// B* 1490127, skip randomizing the generation point for scenario bicycles. [6/22/2013 mdawe]
	if (IsScenarioCarGen() && pModelInfo->GetIsBicycle())
	{
		bDoRandomization = false;
	}
	
	if (bDoRandomization)
	{
		// If the car generator wants the car to be aligned we move the generation point depending on the room we have.
		if (m_bAlignedToLeft || m_bAlignedToRight)
		{
			float	lengthVec2 = rage::Sqrtf(m_vec2X*m_vec2X + m_vec2Y*m_vec2Y);			// This is the width of the car generator
			float	carWidth = (pModelInfo->GetBoundingBoxMax().x - pModelInfo->GetBoundingBoxMin().x);
			float	room = lengthVec2 - carWidth;

			// We will move the car closer to the alligned side (pavement)
			float	shift = room * 0.5f;
#define RANDOM_SPACE (0.3f)		// Randomize within this.
			shift = rage::Max(0.0f, (shift - fwRandom::GetRandomNumberInRange(0.0f, RANDOM_SPACE)));
			Vector3 shiftVec = Vector3(m_vec2X, m_vec2Y, 0.0f);
			shiftVec.Normalize();
			if (m_bAlignedToRight)
			{
				pCreationMatrix->d += shiftVec * shift;
			}
			else
			{
				pCreationMatrix->d -= shiftVec * shift;
			}
		}
		else 
		{
			// Move centerpoint a small amount so that things don't look too regular
			pCreationMatrix->d.x += fwRandom::GetRandomNumberInRange(-0.4f, 0.4f);
			pCreationMatrix->d.y += fwRandom::GetRandomNumberInRange(-0.4f, 0.4f);
		}

		// Rotate matrix a marginal amount so that things don't look too regular.
		if ( (pModelInfo->GetBoundingBoxMax().y - pModelInfo->GetBoundingBoxMin().y) < 8.0f)
		{
			mthRandom rnd(fwRandom::GetRandomNumber());
			static dev_float s_fVariance = 0.11f;
			const float fRotation = rnd.GetGaussian(0.0f, s_fVariance);
			pCreationMatrix->RotateLocalZ(fRotation);
		}
	}
	pCreationMatrix->Normalize();
}

bool CCarGenerator::PlaceOnRoadProperly(CVehicle* pVehicle, Matrix34 *pCreationMatrix, CInteriorInst*& pDestInteriorInst, s32& destRoomIdx, u32& asyncHandle, bool forceSynchronous)
{
	asyncHandle = INVALID_ASYNC_ENTRY;
	if (IsDistantGroundScenarioCarGen() && !pVehicle->IsCollisionLoadedAroundPosition())
	{
		// We aren't requiring collision to be loaded, so we are unable to place this vehicle on the ground properly.
		// However, this was spawned by a scenario so we know the designer put it in a reasonable location.
		// The only thing that isn't known is the height above the ground.  We can compute where the transform should be in the Z 
		// by taking the maximum of the two heights above the ground for this model and adding it to the scenario position.
		float frontHeightAboveGround, rearHeightAboveGround;
		CVehicle::CalculateHeightsAboveRoad(pVehicle->GetModelId(), &frontHeightAboveGround, &rearHeightAboveGround);
		pCreationMatrix->d.z += Max(frontHeightAboveGround, rearHeightAboveGround);
		pVehicle->m_nVehicleFlags.bPlaceOnRoadQueued = true;
		pVehicle->SetIsFixedWaitingForCollision(true);

		return true;
	}
	else if(pVehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
	{
		if(pVehicle->PlaceOnRoadAdjust())
		{
			Matrix34 boatMat = MAT34V_TO_MATRIX34(pVehicle->GetMatrix());
			*pCreationMatrix = boatMat;
			return true;
		}
		else
		{
			return false;
		}
	}
	else if (pVehicle->InheritsFromHeli() || pVehicle->InheritsFromPlane())
	{
		// Scenario cargens allow helicopters and planes to spawn in the air.
		// Scenario points that desire alignment will not be flagged with "AerialVehiclePoint."
		if (IsScenarioCarGen())
		{
			CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*this);
			if (Verifyf(pt, "No point found for car generator."))
			{
				if (pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint))
				{
					return true;
				}
			}
		}

		// Align as if it was a normal vehicle.
		if (forceSynchronous)
		{
			bool setWaitForAllCollisionsBeforeProbe = false;
			return CAutomobile::PlaceOnRoadProperly((CAutomobile*)pVehicle, pCreationMatrix, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, pVehicle->GetModelIndex(), NULL, false, NULL, NULL, 0);
		}

		asyncHandle = CAutomobile::PlaceOnRoadProperlyAsync((CAutomobile*)pVehicle, pCreationMatrix, pVehicle->GetModelIndex());
		if (asyncHandle == INVALID_ASYNC_ENTRY)
		{
			return false;
		}
		else
		{
			return true;
		}
	}
	else if (pVehicle->InheritsFromAutomobile())
	{
		if (forceSynchronous)
		{
			bool setWaitForAllCollisionsBeforeProbe = false;
			return CAutomobile::PlaceOnRoadProperly((CAutomobile*)pVehicle, pCreationMatrix, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, pVehicle->GetModelIndex(), NULL, false, NULL, NULL, 0);
		}

		asyncHandle = CAutomobile::PlaceOnRoadProperlyAsync((CAutomobile*)pVehicle, pCreationMatrix, pVehicle->GetModelIndex());
		if (asyncHandle == INVALID_ASYNC_ENTRY)
			return false;

		return true;
	}
	else if(pVehicle->InheritsFromBike())
	{
		bool setWaitForAllCollisionsBeforeProbe = false;
		return CBike::PlaceOnRoadProperly(static_cast<CBike*>(pVehicle), pCreationMatrix, pDestInteriorInst, destRoomIdx, setWaitForAllCollisionsBeforeProbe, pVehicle->GetModelIndex(), NULL, false, NULL, NULL, 0);
	}

	return true;
}

/////////////////////////////////////////////////////////
// CCarGenerator::Process
//
/////////////////////////////////////////////////////////

bool CCarGenerator::Process(bool bWakeUpVehicleScenarios)
{
#if !__FINAL
	if(!gbAllowVehGenerationOrRemoval && ((!m_bHighPriority) /*|| bHasBeenHijacked*/) )
	{
		m_bReadyToGenerateCar = false;
	}
#endif

#if GTA_REPLAY
	if (CReplayMgr::IsReplayInControlOfWorld()) return false;	
#endif // GTA_REPLAY

	bool bCarModelPicked = false;

	if (m_pLatestCar == NULL && !m_bScheduled)
	{
		if (GenerateCount > 0)
		{
            bool allowedToGenerate = true;

            if(NetworkInterface::IsGameInProgress())
            {
                Vector3 pos(m_centreX, m_centreY, m_centreZ);
                allowedToGenerate = NetworkInterface::IsPosLocallyControlled(pos);
				if(!allowedToGenerate)
				{
					BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioPosNotLocallyControlledCarGen));

					// In SP, or if we were in control of the position, we would have set m_bReadyToGenerateCar back to
					// true whenever we had reached a certain distance away. In MP, we wouldn't do this if we lost control
					// of the position, and once we gain control of the position back, we may already be too close to
					// reactivate the car generator. To fix this, if the car generator is deactivated, we measure the
					// distance here and reactivate if far enough away.
					// Note: we could also try always just setting m_bReadyToGenerateCar back to true here, since
					// we perhaps shouldn't lose control over the position unless we get some distance away anyway.
					if(!m_bReadyToGenerateCar)
					{
						bool bTooClose = false;
						if(!CheckIfWithinRangeOfAnyPlayers(bTooClose, true) && !bTooClose)
						{
							m_bReadyToGenerateCar = true;
						}
					}
				}
            }
			//Activate vehicle scenario points if not near any player and it has been ~3 seconds.
			if (!m_bReadyToGenerateCar  && IsScenarioCarGen())
			{
				if  (CScenarioManager::ShouldWakeUpVehicleScenario(*this, bWakeUpVehicleScenarios))
				{
					if (CScenarioManager::ShouldDoOutsideRangeOfAllPlayersCheck(*this))
					{
						float fRange = m_bHighPriority ? CTheCarGenerators::m_MinDistanceToPlayerForHighPriorityScenarioActivation :
							CTheCarGenerators::m_MinDistanceToPlayerForScenarioActivation;

						if (CheckIfOutsideRangeOfAllPlayers(fRange))
						{
							m_bReadyToGenerateCar = true;
						}	
					}
					else
					{
						// should not do the check so just try to generate something ... 
						m_bReadyToGenerateCar = true;
					}
				}
			}

            if(allowedToGenerate)
            {
			    bool bTooClose = false;
			    if (CheckIfWithinRangeOfAnyPlayers(bTooClose, true))
			    {	//	Shouldn't generate a car on any player's screen
					// Also, regardless of m_bReadyToGenerateCar, we probably shouldn't let it be scheduled again
					// if m_bScheduled is already set.
				    if (m_bReadyToGenerateCar)
				    {
						PF_PUSH_TIMEBAR("ScheduleVehsForGenerationIfInRange");
					    ScheduleVehsForGenerationIfInRange(false, bTooClose, bCarModelPicked);
						PF_POP_TIMEBAR();
				    }
					else
					{
						// Bug? On Island heist, don't wait for the car gen to get out of range or to get too close before letting it generate.
						if(!bTooClose && ThePaths.bStreamHeistIslandNodes)
						{
							m_bReadyToGenerateCar = true;
						}

						BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioCarGenNotReady));
					}
			    }
			    else
			    {

				    if (!bTooClose)
				    {
					    m_bReadyToGenerateCar = true;		// Player was far away from this car generator. We can try and generate a new car here.

						BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioPointTooFarAway));
				    }
					else
					{
						m_bReadyToGenerateCar = m_bHighPriority;

						BANK_ONLY(RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_spawnPointOutsideCreationZone));
					}
			    }
            }
		}
	}
	
	if (m_pLatestCar)
	{
		if (m_pLatestCar->GetStatus() == STATUS_PLAYER)
		{	//	The car counts as being stolen if the player gets in it
			m_pLatestCar->m_nVehicleFlags.bNeverUseSmallerRemovalRange = false;
			// if car generator was a random generator then reset last model 
			if (m_bStoredVisibleModel){
				m_bStoredVisibleModel = false;
				VisibleModel = fwModelId::MI_INVALID;
			}
		}
	}
	else
	{	//	Car no longer exists so stop checking it
		if (m_bHasGeneratedACar)
		{
			m_bHasGeneratedACar = false;
			if (m_bHighPriority)
			{
				m_bReadyToGenerateCar = true;
			}
		}
	}

	return bCarModelPicked;
}

void CCarGenerator::InstantProcess()
{
#if !__FINAL
	if (!gbAllowVehGenerationOrRemoval && !m_bHighPriority)
		return;
#endif

#if GTA_REPLAY
	if (CReplayMgr::IsReplayInControlOfWorld()) return;	
#endif // GTA_REPLAY

	if (m_pLatestCar || m_bScheduled || !m_bReadyToGenerateCar || IsSwitchedOff() || IsScenarioCarGen())
		return;
		
	if (GenerateCount == 0)
		return;

	if(NetworkInterface::IsGameInProgress())
	{
		Vector3 pos(m_centreX, m_centreY, m_centreZ);
		if (!NetworkInterface::IsPosLocallyControlled(pos))
			return;
	}

	bool bTooClose = false;
	if (CheckIfWithinRangeOfAnyPlayers(bTooClose, true) || bTooClose) // too close doesn't matter, we expect a faded screen
	{
#if __BANK
		utimer_t startTime = sysTimer::GetTicks();
#endif
		fwModelId car, trailer;
		bool carModelShouldBeRequested = false;
		if (!CheckIfReadyForSchedule(false, carModelShouldBeRequested, car, trailer) || carModelShouldBeRequested)
			return;

		Matrix34 CreationMatrix;
		BuildCreationMatrix(&CreationMatrix, car.GetModelIndex());

		if (!DoSyncProbeForBlockingObjects(car, CreationMatrix, NULL))
			return;

		Assert(CModelInfo::HaveAssetsLoaded(car));

		const spdSphere testSphere(RC_VEC3V(CreationMatrix.d), ScalarV(V_HALF));
		if (!g_StaticBoundsStore.GetBoxStreamer().HasLoadedWithinAABB(spdAABB(testSphere), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
			return;

		// actually create the vehicle
		CVehicle* pNewVehicle = CVehicleFactory::GetFactory()->Create(car, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, &CreationMatrix, true, true);
		if (!pNewVehicle)
			return;
#if __BANK
		pNewVehicle->m_CreationMethod = CVehicle::VEHICLE_CREATION_CARGEN;
#endif // __BANK

		// do a synchronous probe test when placing the vehicle on the ground
		u32 asyncHandle = INVALID_ASYNC_ENTRY;
		CInteriorInst* interior = NULL;
		s32 interiorId = -1;
		if (!PlaceOnRoadProperly(pNewVehicle, &CreationMatrix, interior, interiorId, asyncHandle, true))
		{
			CVehicleFactory::GetFactory()->Destroy(pNewVehicle, true);
			return;
		}

		pNewVehicle->SetAllowFreezeWaitingOnCollision(true);
		pNewVehicle->m_nVehicleFlags.bShouldFixIfNoCollision = true;

#if __ASSERT
		if(pNewVehicle->InheritsFromPlane())
		{			
			Displayf("---=== [B*1890538] Plane getting set to fix on no collision (%s) ===--", pNewVehicle->GetDebugName());
			sysStack::PrintStackTrace();
		}
#endif		

#if __BANK
		pNewVehicle->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (sysTimer::GetTicks() - startTime));
#endif // __BANK

		ScheduledVehicle veh;
		veh.creationMatrix = CreationMatrix;
		veh.carModelId = car;
		veh.trailerModelId = trailer;
		veh.carGen = this;
		veh.vehicle = pNewVehicle;
		CTheCarGenerators::PostCreationSetup(veh, false);
	}
}


////////////////////////////////////////////////////////
// CCarGenerator::Setup
// ----------------------
//
// input		: (x,y,z) position of centre
//					
//                visible car model to be generated, 
//					remap 1 and 2 of generated car, 
//					flag to determine whether generator is high priority or not
//					%age chance of each car having a car alarm
//					%age chance of each car being locked
//					min & max delay between generations
// function		: setup new car generator
//
////////////////////////////////////////////////////////


void CCarGenerator::Setup(const float centreX, const float centreY, const float centreZ,
						  const float vec1X, const float vec1Y,
						  const float lengthVec2,
						  const u32 visible_model_hash_key, 
						  const s32 remap1_for_car, const s32 remap2_for_car, const s32 remap3_for_car, const s32 remap4_for_car, const s32 remap5_for_car, const s32 remap6_for_car,
						  u32 flags,
						  const u32 ipl,
						  const u32 creationRule, const u32 scenario, const bool bOwnedByScript,
						  atHashString popGroup, s8 livery, s8 livery2, bool unlocked, bool canBeStolen)
{
	Assert(!m_bScheduled);

	// For now, RemoveCarGenerators() never deletes car generators with scenarios, because we would never
	// want to do that with the car generators created and owned by the scenarios themselves.
	// If we knew that 0 could never be a valid ipl number for car generators in map data,
	// we would be able to relax that restriction (or, we could sacrifice a bit for another member
	// variable, if needed), but for now, we'll just make sure that car generators from map data don't
	// have scenarios.
	Assert(ipl == 0 || scenario == CARGEN_SCENARIO_NONE);

	Vec3V cargenPos(centreX, centreY, centreZ);

	m_centreX = centreX;
	m_centreY = centreY;
	m_centreZ = centreZ;

	m_vec1X = vec1X;
	m_vec1Y = vec1Y;

	m_vec2X = vec1Y;
	m_vec2Y = -vec1X;
	float length2 = rage::Sqrtf(m_vec2X*m_vec2X + m_vec2Y*m_vec2Y);
	m_vec2X /= length2;
	m_vec2Y /= length2;
	m_vec2X *= lengthVec2;
	m_vec2Y *= lengthVec2;

	m_bExtendedRange = false;
	// Make sure vector1 is always the longest vector. (rest of the code relies on this)
//	float length1 = rage::Sqrtf(m_vec1X*m_vec1X + m_vec1Y*m_vec1Y);
//	if (length1 < length2)
//	{
//		float	tempX = m_vec1X;
//		float	tempY = m_vec1Y;
//		m_vec1X = m_vec2X;
//		m_vec1Y = m_vec2Y;
//		m_vec2X = tempX;
//		m_vec2Y = tempY;
//	}

	m_bStoredVisibleModel = false;

	fwModelId modelId;
	CBaseModelInfo *pBaseModelInfo = CModelInfo::GetBaseModelInfoFromHashKey(visible_model_hash_key, &modelId);
	if (pBaseModelInfo)
	{
		Assertf(pBaseModelInfo->GetModelType() == MI_TYPE_VEHICLE, "CCarGenerator::Setup - %s is not a car model %f %f %f", pBaseModelInfo->GetModelName(), 
			m_centreX, m_centreY, m_centreZ);

		VisibleModel = modelId.GetModelIndex();
		m_bPreAssignedModel = true;

		if (((CVehicleModelInfo *)pBaseModelInfo)->GetVehicleType() == VEHICLE_TYPE_HELI)
		{
			m_bExtendedRange = true;
		}
	}
	else
	{
#if __ASSERT
		if(visible_model_hash_key)
		{
			atHashString str(visible_model_hash_key);
			char buff[256];
			const char* pName = str.TryGetCStr();
			if(pName)
			{
				formatf(buff, "'%s'", pName);
			}
			else
			{
				formatf(buff, "0x%08x", visible_model_hash_key);
			}
			const char* pTypeString;
			if(scenario != CARGEN_SCENARIO_NONE)
			{
				pTypeString = "vehicle scenario";
			}
			else if(bOwnedByScript)
			{
				pTypeString = "script-created car generator";
			}
			else
			{
				pTypeString = "art-placed car generator";
			}
			Assertf(false, "Failed to find model %s for %s at %f %f %f, a random vehicle may get spawned instead.",
					buff, pTypeString,
					m_centreX, m_centreY, m_centreZ);

		}
#endif

		VisibleModel = fwModelId::MI_INVALID;
	}

	if(remap1_for_car != -1)
		car_remap1 = (u8)remap1_for_car;
	else
		car_remap1 = 0xff;	

	if(remap2_for_car != -1)
		car_remap2 = (u8)remap2_for_car;
	else
		car_remap2 = 0xff;	

	if(remap3_for_car != -1)
		car_remap3 = (u8)remap3_for_car;
	else
		car_remap3 = 0xff;

	if(remap4_for_car != -1)
		car_remap4 = (u8)remap4_for_car;
	else
		car_remap4 = 0xff;	

	if(remap5_for_car != -1)
		car_remap5 = (u8)remap5_for_car;
	else
		car_remap5 = 0xff;	

	if(remap6_for_car != -1)
		car_remap6 = (u8)remap6_for_car;
	else
		car_remap6 = 0xff;	

	m_bHighPriority = (flags & CARGEN_FORCESPAWN) != 0;
	m_bLowPriority = (flags & CARGEN_LOWPRIORITY) != 0;
	m_bPlayerHasAlreadyOwnedCar = false;
	m_bOwnedByScript = bOwnedByScript;

	Assert(creationRule < 16);		// Only 4 bits for storage
	m_CreationRule = creationRule;
	Assert(scenario < MAX_SCENARIO_TYPES);
	m_Scenario = scenario;

	m_pLatestCar = NULL;

    if (m_bHighPriority)
    {
        Vector3 pos(m_centreX, m_centreY, m_centreZ);

        CVehicle::Pool* pool = CVehicle::GetPool();
        s32 count = (s32) pool->GetSize();

        for (s32 i = 0; i < count; ++i)
        {
            CVehicle* veh = pool->GetSlot(i);
            if (!veh)
                continue;

            const Vector3 parentPos = veh->GetParentCargenPos();
            if ((pos - parentPos).Mag2() < 1.f)
            {
                m_pLatestCar = veh;
                break;
            }
        }
    }

	GenerateCount = INFINITE_CAR_GENERATE;

	m_bReadyToGenerateCar = true;		// false her would force the generator to be far away from the player before we can start generating cars.
	m_bHasGeneratedACar = false;

	m_bReservedForPoliceCar = (flags & CARGEN_POLICE) != 0;
	m_bReservedForFiretruck = (flags & CARGEN_FIRETRUCK) != 0;
	m_bReservedForAmbulance = (flags & CARGEN_AMBULANCE) != 0;
	m_bAlignedToLeft = (flags & CARGEN_ALIGN_LEFT) != 0;
	m_bAlignedToRight = (flags & CARGEN_ALIGN_RIGHT) != 0;
	m_bPreventEntryIfNotQualified = (flags & CARGEN_PREVENT_ENTRY) != 0;
	m_bCreateCarUnlocked = unlocked;

	if (m_bReservedForPoliceCar)
	{
		VisibleModel = MI_CAR_POLICE;
	}

	if (m_bReservedForFiretruck)
	{
		VisibleModel = MI_CAR_FIRETRUCK;
	}

	if (m_bReservedForAmbulance)
	{
		VisibleModel = MI_CAR_AMBULANCE;
	}

	Assertf( (!m_bAlignedToLeft) || (!m_bAlignedToRight), "Car generator can not be aligned to left AND aligned to Right" );

	m_bActiveDuringDay = (flags & CARGEN_DURING_DAY) != 0;
	m_bActiveAtNight = (flags & CARGEN_AT_NIGHT) != 0;

	// If day and night are both false the artists have probaly not set this correctly and
	// we'll create cars during the day and night instead.
	if (!m_bActiveDuringDay && !m_bActiveAtNight)
	{
		m_bActiveDuringDay = m_bActiveAtNight = true;
	}

	m_ipl = (u16)ipl;
	m_bDestroyAfterSchedule = false;

#if __BANK
	m_failCode = FC_NONE;
	m_failPos.Set(0.f, 0.f, 0.f);
	m_hitEntities[0] = m_hitEntities[1] = m_hitEntities[2] = m_hitEntities[3] = NULL;

	m_hitPosZ[0] = 0.f;
	m_hitPosZ[1] = 0.f;
	m_hitPosZ[2] = 0.f;
	m_hitPosZ[3] = 0.f;
	m_hitPosZ[4] = 0.f;

	placementFail = 0xffffffff;
#endif // __BANK

#if STORE_CARGEN_PLACEMENT_FILE
	char msgBuf[128];
	const char* msg = diagContextMessage::GetTopMessage(msgBuf,sizeof(msgBuf));
	if(msg)
	{
		while(*msg != '(' && *msg != 0)
			msg++;
		formatf(m_placedFrom, "%s", msg); // store name of file this cargen is from
	}
	else
	{
		formatf(m_placedFrom, "unknown_file"); // we don't know the filename
	}
#endif

	m_popGroup = popGroup;
    m_livery = livery;
    m_livery2 = livery2;

	m_bCanBeStolen = canBeStolen;

	SwitchOffIfWithinSwitchArea();

	// tag cargens in prio areas so we don't have to do it every frame
	m_bInPrioArea = false;
	if (!m_bHighPriority)
	{
		CargenPriorityAreas* prioAreas = CTheCarGenerators::GetPrioAreas();
		if (prioAreas)
		{
			for (s32 i = 0; i < prioAreas->areas.GetCount(); ++i)
			{
				Vec3V areaMin = RCC_VEC3V(prioAreas->areas[i].minPos);
				Vec3V areaMax = RCC_VEC3V(prioAreas->areas[i].maxPos);
				spdAABB area(areaMin, areaMax);

				if (area.ContainsPoint(cargenPos))
				{
					m_bInPrioArea = true;
					break;
				}
			}
		}
	}

#if HACK_GTAV_REMOVEME
	//////////////////////////////////////////////////////////////////////////
	static spdSphere interestSphere( Vec3V(-1542.0f, 186.3f, 58.2f), ScalarV(30.0f) );
	m_bSkipStaticBoundsTest = interestSphere.ContainsPoint(cargenPos);
	//////////////////////////////////////////////////////////////////////////
#else
	m_bSkipStaticBoundsTest = false;
#endif
}


//////////////////////////////////////////////////////////
// CCarGenerator::ShouldCheckRangesIn3D
//-----------------------------------------
//
// function		: returns true if range checks need to be performed in three dimentions
//					(typically for aerial vehicles)
//////////////////////////////////////////////////////////
bool CCarGenerator::ShouldCheckRangesIn3D() const
{
	// Note: the !m_bDestroyAfterSchedule check here was added because when a scenario point
	// gets destroyed, we need to keep the car generator around for a moment if it's already
	// scheduled to spawn, so it can clean up properly.	At that point, the destruction of the
	// CCarGenerator object is imminent and it won't be allowed to actually spawn a vehicle,
	// so 2D vs. 3D doesn't matter, and we don't even want to spend time looking for the scenario.
	if (IsScenarioCarGen() && !m_bDestroyAfterSchedule)
	{
		CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*this);
		if (Verifyf(pt, "No point found for car generator."))
		{
			if (pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint))
			{
				return true;
			}
		}
	}
	return false;
}


////////////////////////////////////////////////////////
// CCarGenerator::CheckIfWithinRangeOfPlayer
// ---------------------------------------------
//
// function		: returns TRUE if the given player is within the given 
//					range of the generator
////////////////////////////////////////////////////////

bool CCarGenerator::CheckIfWithinRangeOfPlayer(CPlayerInfo* pPlayer, float rangeSqr)
{
	// is our player close enough?
	float XDiff, YDiff, ZDiff;
	float DistanceSqr;

	if(!pPlayer || !AssertVerify(pPlayer->GetPlayerPed()))
	{
		return false;
	}

	Vector3 PlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition());

    if(pPlayer->GetPlayerPed()->IsNetworkClone())
    {
        CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer  *>(pPlayer->GetPlayerPed()->GetNetworkObject());
        netObjPlayer->GetAccurateRemotePlayerPosition(PlayerPos);
    }

	if (ShouldCheckRangesIn3D())
	{
		if (IsLessThanAll(DistSquared(VECTOR3_TO_VEC3V(PlayerPos), Vec3V(m_centreX, m_centreY, m_centreZ)), ScalarV(rangeSqr)))
		{
			return true;
		}
		else
		{
			return false;
		}
	}
	else
	{
		XDiff = PlayerPos.x - m_centreX;
		YDiff = PlayerPos.y - m_centreY;

		DistanceSqr = XDiff * XDiff + YDiff * YDiff;

		if (DistanceSqr < rangeSqr)
		{
			if(IsScenarioCarGen())
			{
				// Determine Z coordinate limits up and down (from the player).
				float limitOffsZUp = 50.0f;
				float limitOffsZDown = -50.0f;
				const CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*this);
				if(pt && pt->HasExtendedRange())
				{
					limitOffsZUp = 100.0f;
					if(CScenarioManager::ShouldExtendExtendedRangeFurther())
					{
						limitOffsZDown = -300.0f;
					}
					else
					{
						limitOffsZDown = -100.0f;
					}
				}

				const float rangeMinZ = PlayerPos.z + limitOffsZDown;
				const float rangeMaxZ = PlayerPos.z + limitOffsZUp;
				if(m_centreZ < rangeMinZ || m_centreZ > rangeMaxZ)
				{
					return false;
				}
			}
			else
			{
				ZDiff = PlayerPos.z - m_centreZ;
				// not in range if you are 50 meters above or below
				if(rage::Abs(ZDiff) > 50.0f)
					return false;
			}

			return true;
		}

		return false;
	}
}

float CCarGenerator::GetGenerationRange() const
{
    float carGenRange = CVehiclePopulation::GetKeyholeShape().GetInnerBandRadiusMax();
	if (m_bExtendedRange)
	{
		carGenRange = CVehiclePopulation::GetKeyholeShape().GetOuterBandRadiusMax();
	}
	carGenRange *= CVehiclePopulation::GetVehicleLodScale();

    if (NetworkInterface::IsGameInProgress())
    {
        float maxCarGenRangeForNetworkGame = CNetObjVehicle::GetStaticScopeData().m_scopeDistance * 0.9f;

        if(carGenRange > maxCarGenRangeForNetworkGame)
        {
            carGenRange = maxCarGenRangeForNetworkGame;
        }
    }

    return carGenRange;
}

bool CCarGenerator::IsDistantGroundScenarioCarGen() const
{
	if (IsScenarioCarGen())
	{
		const CScenarioVehicleInfo* pInfo = CScenarioManager::GetScenarioVehicleInfoForCarGen(*this);
		if (pInfo && pInfo->GetIsFlagSet(CScenarioInfoFlags::DistantGroundVehicleSpawn))
		{
			return true;
		}
	}
	return false;
}

bool CCarGenerator::GetCarGenCullRange(float &fOutRange, bool bUseReducedNetworkScopeDist) const
{
	// First discard all cargens too far away.
	// Creating parked vehicles outside of the cull range is daft as they will be removed the next frame
	fOutRange = CVehiclePopulation::GetCullRange()-1.0f;

	TUNE_GROUP_BOOL(VEHICLE_POPULATION, bUseMPCarGenCullRange, true);
	if (NetworkInterface::IsGameInProgress() && bUseMPCarGenCullRange)
	{
		// In network games, use scope distance to spawn parked cars. This prevents us creating the vehicle & immediately passing ownership to another player (who
		// also may not have it in scope).
		// Note: we assume this is less than the normal cull range, but prevent making it much larger.
		static dev_float s_ReductionScale = 0.9f;
		float fScopeScale = bUseReducedNetworkScopeDist ? s_ReductionScale : 1.0f;
		fOutRange = Min(CNetObjVehicle::GetStaticScopeData().m_scopeDistance * fScopeScale, fOutRange); 
	}

	if (IsScenarioCarGen() && !m_bDestroyAfterSchedule)	// If m_bDestroyAfterSchedule is set, the scenario system has already detached from this car generator, which is about to be destroyed ASAP.
	{
		fOutRange *= CScenarioManager::GetCarGenSpawnRangeMultiplyer(*this);

		// If we are clustered, skip the max range check, as we need to try hard to get all the
		// vehicles in, and some may be much further away than others.
		const CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*this);
		if(pt && pt->IsInCluster())
		{
			return false;
		}
	}

	return true;
}

////////////////////////////////////////////////////////
// CCarGenerator::CheckIfWithinRangeOfAnyPlayers
// ---------------------------------------------
//
// function		: returns TRUE if at least one player is close to this car generator
//					returns FALSE if no player is close or any player is too close
//
////////////////////////////////////////////////////////
bool CCarGenerator::CheckIfWithinRangeOfAnyPlayers(bool &bTooClose, bool bUseReducedNetworkScopeDist)
{
	bTooClose = false;

	// First discard all cargens too far away.
	float maxRange = 0.0f;
	bool doMaxRangeCheck = GetCarGenCullRange(maxRange, bUseReducedNetworkScopeDist);
    
    float maxRangeSqr = maxRange * maxRange;
	bool inRangeOfAnyPlayer = !doMaxRangeCheck || CheckIfWithinRangeOfPlayer(CGameWorld::GetMainPlayerInfo(), maxRangeSqr);

	// temp fix for cargens repeatedly spawning cars in MP. This happens when a player in charge of a network world grid creates a vehicle
	// that is out of scope of that player. The car then migrates and is immediately removed on the player's machine by the new owner. The 
	// player then sees that the cargen is empty and creates another one. 
	if (!inRangeOfAnyPlayer)
	{
		return false;
	}

	Vector3 cargenPos(m_centreX, m_centreY, m_centreZ);

	// if area of interest is in rage of player and this cargen isn't inside that area, discard
	if (CTheCarGenerators::m_CarGenAreaOfInterestRadius > 0.f)
	{
		if (fwRandom::GetRandomNumberInRange(0.f, 1.f) <= CTheCarGenerators::m_CarGenAreaOfInterestPercentage)
		{
			const float aoiRadiusSqr = CTheCarGenerators::m_CarGenAreaOfInterestRadius * CTheCarGenerators::m_CarGenAreaOfInterestRadius;

			if ((CTheCarGenerators::m_CarGenAreaOfInterestPosition - cargenPos).Mag2() > aoiRadiusSqr)
			{
				if(CGameWorld::GetMainPlayerInfo() && CGameWorld::GetMainPlayerInfo()->GetPlayerPed())
				{
					Vector3 PlayerPos = VEC3V_TO_VECTOR3(CGameWorld::GetMainPlayerInfo()->GetPlayerPed()->GetTransform().GetPosition());
					if(CGameWorld::GetMainPlayerInfo()->GetPlayerPed()->IsNetworkClone())
					{
						CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer  *>(CGameWorld::GetMainPlayerInfo()->GetPlayerPed()->GetNetworkObject());
						netObjPlayer->GetAccurateRemotePlayerPosition(PlayerPos);
					}

					if (((CTheCarGenerators::m_CarGenAreaOfInterestPosition - PlayerPos).Mag() - CTheCarGenerators::m_CarGenAreaOfInterestRadius) <= maxRange)
					{
						return false;
					}
				}
			}
			else if (!m_bHighPriority && fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f) // don't set all cargens to high prio or we'll spam the population
			{
				m_bHighPriority = true;
				m_bTempHighPrio = true;
			}
		}
	}
	else if (!m_bHighPriority && m_bInPrioArea && fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f)
	{
		m_bHighPriority = true;
		m_bTempHighPrio = true;
	}
	else if (m_bTempHighPrio)
	{
		m_bTempHighPrio = false;
		m_bHighPriority = false;
	}

	float onScreenMinSpawnDist = GetCreationDistanceOnScreen();
	float offScreenMinSpawnDist = GetCreationDistanceOffScreen();

	if(IsScenarioCarGen())
	{
		// If this is a cluster point, make sure we don't end up with a minimum on-screen spawn
		// distance that's too restrictive - there are many things that have to come together
		// for a cluster to spawn successfully, and without this, onScreenMinSpawnDist could
		// be up towards 250 m or so (at least when flying).
		const CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*this);
		if(pt && pt->IsInCluster())
		{
			onScreenMinSpawnDist = Min(onScreenMinSpawnDist, CScenarioManager::GetMaxOnScreenMinSpawnDistForCluster());

			// Note: we might want to clamp offScreenMinSpawnDist to the same value, if it's not already
			// is less - it tends to be, though.
			// Assert(onScreenMinSpawnDist >= offScreenMinSpawnDist);
		}

		// For scenario car generators, if we already know which vehicle we are about to spawn,
		// factor in the spawn distance scale in the distance threshold.
		if(VisibleModel != fwModelId::MI_INVALID)
		{
			const bool isAerialPoint = pt && pt->IsFlagSet(CScenarioPointFlags::AerialVehiclePoint);
			const float scaleFactor = CScenarioManager::GetVehicleScenarioDistScaleFactorForModel(fwModelId(strLocalIndex(VisibleModel)), isAerialPoint);

			onScreenMinSpawnDist *= scaleFactor;

			// After bumping up the distance scale factors for aircraft, I had some difficulty getting planes
			// to spawn in air to land at the airport. We now don't include the distance scale factor
			// in the off-screen distance for aerial points. Possibly we shouldn't do it on-ground points
			// either, but to achieve that, we would also have to adjust the checks against the keyhole shape
			// (which is bypassed for aerial ones).
			if(!isAerialPoint)
			{
				offScreenMinSpawnDist *= scaleFactor;
			}
		}
	}

	// discard cargens too close
	//float radius = (m_vec1X + m_vec1Y) * 0.5f;
	// if we're doing a player switch, we need to be more judicious about checking vehicle view frustums
	float radius = Sqrtf((m_vec1X*m_vec1X) + (m_vec1Y*m_vec1Y));
	bool inView = CPedPopulation::IsCandidateInViewFrustum(cargenPos, radius, (g_PlayerSwitch.IsActive() ? PLAYER_SWITCH_ON_SCREEN_SPAWN_DISTANCE : onScreenMinSpawnDist));

	float minDistanceSqr = square(offScreenMinSpawnDist);
	if (inView)
	{
		if(g_PlayerSwitch.IsActive())
		{
			return false; // don't spawn on-screen inside PLAYER_SWITCH_ON_SCREEN_SPAWN_DISTANCE during player switch
		}

		minDistanceSqr = square(onScreenMinSpawnDist);
	}

    if (!m_bIgnoreTooClose)
    {
        if (CheckIfWithinRangeOfPlayer(CGameWorld::GetMainPlayerInfo(), minDistanceSqr))
        {
            bTooClose = true;
            return false;
        }
    }

    // in network games we need to generate cars within range of the other players in this area, as
    // well as avoiding creating cars too close to these players (unless they are high priority)
    if (NetworkInterface::IsGameInProgress())
    {
        unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
        const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

        for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
        {
	        const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

            if(pPlayer->GetPlayerPed())
            {
                inRangeOfAnyPlayer |= CheckIfWithinRangeOfPlayer(pPlayer->GetPlayerPed()->GetPlayerInfo(), maxRange*maxRange);

				bool inView = NetworkInterface::IsVisibleToPlayer(pPlayer, cargenPos, radius, onScreenMinSpawnDist);

				minDistanceSqr = square(offScreenMinSpawnDist);
				if (inView)
				{
					minDistanceSqr = square(onScreenMinSpawnDist);
				}

                if (CheckIfWithinRangeOfPlayer(pPlayer->GetPlayerPed()->GetPlayerInfo(), minDistanceSqr))
			    {
                    bTooClose = true;
				    return false;
			    }
            }
        }
    }

    return inRangeOfAnyPlayer;
}

bool CCarGenerator::CheckIfOutsideRangeOfAllPlayers(float fRange)
{
	Vector3 vPos(m_centreX, m_centreY, m_centreZ);

	if (NetworkInterface::IsGameInProgress())
	{
		// Only wake up the point if the position is locally controlled.
		const netPlayer* pClosest = NULL;
		return NetworkInterface::IsPosLocallyControlled(vPos) && !NetworkInterface::IsCloseToAnyPlayer(vPos, fRange, pClosest);
	}
	else
	{
		Vector3 vPlayer = CGameWorld::FindLocalPlayerCoors();
		if (vPlayer.Dist2(vPos) >= fRange * fRange)
		{
			return true;
		}
		else
		{
			return false;
		}
	}
}

float CCarGenerator::GetCreationDistanceOnScreen() const
{
	if (m_bHighPriority)
	{
		return CVehiclePopulation::GetKeyholeShapeOuterBandRadiusHighPriority();
	}
	else
	{
		return CVehiclePopulation::GetPopulationSpawnFrustumNearDist();
	}

}

float CCarGenerator::GetCreationDistanceOffScreen() const
{
	if (m_bHighPriority)
	{
		return CVehiclePopulation::GetKeyholeShapeInnerBandRadiusHighPriority();
	}
	else
	{
		return CVehiclePopulation::GetCreationDistanceOffScreen();
	}

}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#if __BANK

void CCarGenerator::RegisterSpawnFailure(int failureType) const
{
	if(IsScenarioCarGen())
	{
		Assert(failureType >= 0 && failureType < CPedPopulation::PedPopulationFailureData::FR_Max);
		CPedPopulation::PedPopulationFailureData::FailureType type = (CPedPopulation::PedPopulationFailureData::FailureType)failureType;
		Vec3V pos(m_centreX, m_centreY, m_centreZ);
		CPedPopulation::ms_populationFailureData.RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::ST_VehicleScenario, type, VEC3V_TO_VECTOR3(pos));
	}
}

#endif	// __BANK

#if !__FINAL

void CCarGenerator::DebugRender(bool DEBUG_DRAW_ONLY(forVectorMap))
{
#if DEBUG_DRAW
	Vector3 vCurrPos = camInterface::GetPos();
	Vector3 vPos(m_centreX, m_centreY, m_centreZ+0.5f);
	Vector3 Diff = vCurrPos - vPos;
	if(!forVectorMap && Diff.Mag() > CTheCarGenerators::gs_fDisplayCarGeneratorsDebugRange)
	{
		return;
	}

	// Determine the color to draw the generator.
	Color32 color(80, 80, 80, 255);				// Switched off->grey
	if (GenerateCount)
	{
		if (m_bReadyToGenerateCar)
		{
			color = Color32(255, 255, 0, 255);	// Ready to create a car->yellow
		}
		else
		{
			color = Color32(255, 128, 0, 255);	// Active but not ready->orange
		}
	}

	if (m_bStoredVisibleModel && (fwTimer::GetSystemFrameCount() & 1))
	{
		if (m_bPreAssignedModel)
			color = Color32(255, 0, 255, 255);
		else
			color = Color32(255, 0, 0, 255);
	}


	Vector3	Forward = 0.5f * Vector3(m_vec1X, m_vec1Y, 0.0f);
	Vector3	Side = 0.5f * Vector3(m_vec2X, m_vec2Y, 0.0f);

	Vector3 p1 = vPos + Forward + Side;
	Vector3 p2 = vPos + Forward - Side;
	Vector3 p3 = vPos - Forward - Side;
	Vector3 p4 = vPos - Forward + Side;

	// Renders a rectangle
	if(forVectorMap)
	{
		CVectorMap::DrawLine(p1, p2, color, color);
		CVectorMap::DrawLine(p2, p3, color, color);
		CVectorMap::DrawLine(p3, p4, color, color);
		CVectorMap::DrawLine(p4, p1, color, color);
	}
	else
	{
		grcDebugDraw::Line(p1, p2, color, color);
		grcDebugDraw::Line(p2, p3, color, color);
		grcDebugDraw::Line(p3, p4, color, color);
		grcDebugDraw::Line(p4, p1, color, color);
	}

	// Renders the arrow bit to indicate the direction of the car generator.
	if(forVectorMap)
	{
		CVectorMap::DrawLine(vPos+Forward, vPos+ (Forward * 0.5f) -Side, color, color);
		CVectorMap::DrawLine(vPos+Forward, vPos+ (Forward * 0.5f) +Side, color, color);
		CVectorMap::DrawLine(vPos+(Forward*0.5f)-Side, vPos+(Forward*0.5f)+Side, color, color);
	}
	else
	{
		grcDebugDraw::Line(vPos+Forward, vPos+ (Forward * 0.5f) -Side, color);
		grcDebugDraw::Line(vPos+Forward, vPos+ (Forward * 0.5f) +Side, color);
		grcDebugDraw::Line(vPos+(Forward*0.5f)-Side, vPos+(Forward*0.5f)+Side, color);
	}

	// Also render an extra line to the side if this car generator is aligned.
	if (m_bAlignedToLeft)
	{
		if(forVectorMap)
		{
			CVectorMap::DrawLine(vPos+Forward-(Side*0.92f), vPos-Forward-(Side*0.92f), color, color);
		}
		else
		{
			grcDebugDraw::Line(vPos+Forward-(Side*0.92f), vPos-Forward-(Side*0.92f), color);
		}
	}
	else if (m_bAlignedToRight)
	{
		if(forVectorMap)
		{
			CVectorMap::DrawLine(vPos+Forward+(Side*0.92f), vPos-Forward+(Side*0.92f), color, color);
		}
		else
		{
			grcDebugDraw::Line(vPos+Forward+(Side*0.92f), vPos-Forward+(Side*0.92f), color);
		}
		
	}
	
    s32 prioArea = -1;
    CargenPriorityAreas* prioAreas = CTheCarGenerators::GetPrioAreas();
    if (prioAreas)
    {
        for (s32 i = 0; i < prioAreas->areas.GetCount(); ++i)
        {
            if (geomPoints::IsPointInBox(vPos, prioAreas->areas[i].minPos, prioAreas->areas[i].maxPos))
            {
                prioArea = i;
                break;
            }
        }
    }

	char debugText[128];
	u32 line = 1;
	Color32 iTextCol = Color_NavyBlue;

#if STORE_CARGEN_PLACEMENT_FILE
	sprintf(debugText, "0x%p (%s) prio area: %d (idx: %d)", this, m_placedFrom, prioArea, CTheCarGenerators::GetIndexForCarGenerator(this));
#else
	sprintf(debugText, "0x%p prio area: %d", this, prioArea);
#endif
	grcDebugDraw::Text(vPos, iTextCol, debugText);
	debugText[0] = 0;

	if (m_bPreAssignedModel && CModelInfo::IsValidModelInfo(VisibleModel))
	{
		sprintf(debugText, "%s", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(VisibleModel))));
	}
	if (m_bHighPriority > 0)
	{
		sprintf(debugText, "%s (high priority)", debugText);
	}
	if (m_bPreAssignedModel)
	{
		sprintf(debugText, "%s (preassigned)", debugText);
	}
	if (!GenerateCount)
	{
		sprintf(debugText, "%s (switched off)", debugText);
	}
	if (m_bReservedForAmbulance)
	{
		sprintf(debugText, "%s (reserved for ambulance)", debugText);
	}
	else if (m_bReservedForFiretruck)
	{
		sprintf(debugText, "%s (reserved for firetruck)", debugText);
	}
	else if (m_bReservedForPoliceCar)
	{
		sprintf(debugText, "%s (reserved for police car)", debugText);
	}

	if (debugText[0] != 0)
	{
		if(!forVectorMap)
		{
			grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);
		}
	}

	if (m_popGroup.GetHash() != 0)
	{
		if (!forVectorMap)
		{
			formatf(debugText, "pop group: %s", m_popGroup.TryGetCStr());
			grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);
		}
	}

	const char * pGenRule = GetCreationRuleDesc(m_CreationRule);
	formatf(debugText, "Generation Rule: %s %s", pGenRule, m_bExtendedRange ? "(extended)" : "");
	if(!forVectorMap)
	{
		grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);
	}

	if(m_bOwnedByScript)
	{
		formatf(debugText, "Created by script (%8x)", placementFail);
	}
	else if(IsScenarioCarGen())
	{
		const int scenarioType = GetScenarioType();
		formatf(debugText, "Scenario: %s", SCENARIOINFOMGR.GetNameForScenario(scenarioType));
	}
	else
	{
		formatf(debugText, "Created by map data %s (%8x)", INSTANCE_STORE.GetName(strLocalIndex(m_ipl)), placementFail);
	}
	grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);

	if (m_failCode != FC_NONE && !forVectorMap)
	{
		switch (m_failCode)
		{
		case FC_SCHEDULED:
			safecpy(debugText, "Status: Car scheduled");
			break;
		case FC_NO_VALID_MODEL:
			safecpy(debugText, "Status: No valid model");
			break;
		case FC_CARGEN_PROBE_FAILED:
			safecpy(debugText, "Status: Cargen probe failed");
			break;
		case FC_CARGEN_PROBE_2_FAILED:
			safecpy(debugText, "Status: Second cargen probe failed");
			break;
		case FC_PLACE_ON_ROAD_FAILED:
			safecpy(debugText, "Status: Place on road failed");
			break;
		case FC_CARGEN_VEH_STREAMED_OUT:
			safecpy(debugText, "Status: Vehicle has been streamed out");
			break;
		case FC_CARGEN_FACTORY_FAIL:
			safecpy(debugText, "Status: Factory failed to create vehicle");
			break;
		case FC_CARGEN_PROBE_SCHEDULE_FAILED:
			safecpy(debugText, "Status: Failed to schedule collision test probe");
			break;
		case FC_VEHICLE_REMOVED:
			safecpy(debugText, "Status: Vehicle entity was removed");
			break;
		case FC_SCENARIO_POP_FAILED:
			safecpy(debugText, "Status: Scenario population failed");
			break;
		case FC_COLLISIONS_NOT_LOADED:
			safecpy(debugText, "Status: Collisions not loaded");
			break;
		case FC_NOT_ACTIVE_DURING_DAY:
			safecpy(debugText, "Status: Not active during day");
			break;
		case FC_NOT_ACTIVE_DURING_NIGHT:
			safecpy(debugText, "Status: Not active during night");
			break;
		case FC_SCRIPT_DISABLED:
			safecpy(debugText, "Status: Disabled by script");
			break;
		case FC_TOO_MANY_CARS:
			safecpy(debugText, "Status: Too many cars");
			break;
		case FC_TOO_MANY_PARKED_CARS:
			safecpy(debugText, "Status: Too many parked cars");
			break;
		case FC_TOO_MANY_LOWPRIO_PARKED_CARS:
			safecpy(debugText, "Status: Too many lowprio cars");
			break;
		case FC_REJECTED_BY_SCENARIO:
			safecpy(debugText, "Status: Rejected by scenario");
			break;
		case FC_IDENTICAL_MODEL_CLOSE:
			safecpy(debugText, "Status: Identical model close");
			break;
		case FC_NOT_IN_RANGE:
			safecpy(debugText, "Status: Not in range");
			break;
		case FC_TOO_CLOSE_AT_GEN_TIME:
			safecpy(debugText, "Status: Too close at generation time");
			break;
		case FC_DONT_SPAWN_COPS:
			safecpy(debugText, "Status: Don't spawn cops");
			break;
		case FC_CANT_REQUEST_NEW_MODEL:
			safecpy(debugText, "Status: Can't request new model");
			break;
		case FC_CARGEN_BLOCKED_SCENARIO_CHAIN:
			safecpy(debugText, "Status: Blocked scenario chain");
			break;
		case FC_CARGEN_NOT_ALLOWED_IN_MP:
			safecpy(debugText, "Status: Cargen not allowed in MP");
			break;
		case FC_CARGEN_DESTROYED:
			safecpy(debugText, "Status: Cargen destroyed");
			break;
		default:
			safecpy(debugText, "Status: unknown");
			break;
		}

		grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);
	}

	safecpy(debugText, m_bReadyToGenerateCar ? "Ready" : "Not ready");
	grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);

#if __BANK
	formatf(debugText, "Placement: %.2f %.2f %.2f", m_failPos.x, m_failPos.y, m_failPos.z);
	grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);
#endif

#if __BANK && __ASSERT
	CEntity* e0 = m_hitEntities[0].Get();
	CEntity* e1 = m_hitEntities[1].Get();
	CEntity* e2 = m_hitEntities[2].Get();
	CEntity* e3 = m_hitEntities[3].Get();

	formatf(debugText, "Placement hits: 0x%8x 0x%8x 0x%8x 0x%8x", e0, e1, e2, e3);
	grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);

	formatf(debugText, "Wheel hits: %.2f, %.2f, %.2f, %.2f, %.2f", m_hitPosZ[0], m_hitPosZ[1], m_hitPosZ[2], m_hitPosZ[3], m_hitPosZ[4]);
	grcDebugDraw::Text(vPos, iTextCol, 0, grcDebugDraw::GetScreenSpaceTextHeight()*(line++), debugText);
#endif
#endif // DEBUG_DRAW
}

const char * CCarGenerator::GetCreationRuleDesc(int i)
{
	switch(i)
	{
		case CREATION_RULE_ALL:
			return "CREATION_RULE_ALL";
		case CREATION_RULE_ONLY_SPORTS:
			return "CREATION_RULE_ONLY_SPORTS";
		case CREATION_RULE_NO_SPORTS:
			return "CREATION_RULE_NO_SPORTS";
		case CREATION_RULE_ONLY_BIG:
			return "CREATION_RULE_ONLY_BIG";
		case CREATION_RULE_NO_BIG:
			return "CREATION_RULE_NO_BIG";
		case CREATION_RULE_ONLY_BIKES:
			return "CREATION_RULE_ONLY_BIKES";
		case CREATION_RULE_NO_BIKES:
			return "CREATION_RULE_NO_BIKES";
		case CREATION_RULE_ONLY_DELIVERY:
			return "CREATION_RULE_ONLY_DELIVERY";
		case CREATION_RULE_NO_DELIVERY:
			return "CREATION_RULE_NO_DELIVERY";
		case CREATION_RULE_BOATS:
			return "CREATION_RULE_BOATS";
		case CREATION_RULE_ONLY_POOR:
			return "CREATION_RULE_ONLY_POOR";
		case CREATION_RULE_NO_POOR:
			return "CREATION_RULE_NO_POOR";
		case CREATION_RULE_CAN_BE_BROKEN_DOWN:
			return "CREATION_RULE_CAN_BE_BROKEN_DOWN";
		default:
			Assertf(false, "Creation rule enum val not found..");
			return "UNKNOWN";
	}
}
#endif // !FINAL


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

void CTheCarGenerators::Init() // Static constructor initializes pool
{
	int poolSize = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("CCargen", 0xA987CB1B), CONFIGURED_FROM_FILE);
	
#if COMMERCE_CONTAINER
	CarGeneratorPool.Init(poolSize, true);
#else
	CarGeneratorPool.Init(poolSize);
#endif

	ms_CarGeneratorIPLs = rage_new u16[poolSize];

	const int minAllowedScenarioCarGenerators = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("CCargenForScenarios", 0x4c3a446e), CONFIGURED_FROM_FILE);
	Assert(minAllowedScenarioCarGenerators <= 0xffff);
	m_MinAllowedScenarioCarGenerators = (u16)minAllowedScenarioCarGenerators;

	delete ms_prioAreas;
	ms_prioAreas = NULL;

    PARSER.LoadObjectPtr("common:/data/cargens", "meta", ms_prioAreas);

#if __BANK
	g_bHasPosition[0] = false;
	g_bHasPosition[1] = false;
#endif // __BANK

	ms_preassignedModels.Reset();
	ms_scenarioModels.Reset();
}

////////////////////////////////////////////////////////
// CTheCarGenerators::Process
// -----------------------------------
//
// function		: do all car generator processing for one game cycle
//
////////////////////////////////////////////////////////

bool CTheCarGenerators::ShouldGenerateVehicles()
{
#if !__FINAL
	if(!gbAllowVehGenerationOrRemoval)
		return false;
#endif //__DEV...

	if (NoGenerate)
	{
		return false;
	}

    if(NetworkInterface::IsGameInProgress() && NetworkInterface::AreCarGeneratorsDisabled())
    {
        return false;
    }

	if (CGameWorld::FindLocalPlayerTrain() || (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning() && !CutSceneManager::GetInstance()->CanCarGenertorsUpdateDuringCutscene()))
	{
		return false;
	}

#if GTA_REPLAY
	if (CReplayMgr::IsReplayInControlOfWorld())
	{
		return false;		// No car generators when replays are playing back.
	}
#endif

	return true;
}

void CTheCarGenerators::Process()
{
	if (!ShouldGenerateVehicles())
	{
		return;
	}

#if __BANK
	if (gs_bDisplayAreaOfInterest && m_CarGenAreaOfInterestRadius > 0.f)
		grcDebugDraw::Sphere(m_CarGenAreaOfInterestPosition, m_CarGenAreaOfInterestRadius, Color32(255, 0, 0), false, 1, 64);
#endif // __BANK

	const s32 initialNumScheduled = CTheCarGenerators::GetNumQueuedVehs();
	const s32 poolSize = static_cast<s32>(CarGeneratorPool.GetSize());

	const CargenPriorityAreas* priorityAreas = CTheCarGenerators::GetPrioAreas();

	const bool processPriorityAreaCargens = priorityAreas && priorityAreas->areas.GetCount() != 0;

	// To make spawning with scenario points more reliable, vehicle scenario points can occasionally be woken up
	// earlier than other cargens.
	// If starting the first of a set of batches and have exceeded the timer, then try to wake up the points.
	if (m_iTryToWakeUpScenarioPoints == -1 && m_iTryToWakeUpPriorityAreaScenarioPoints == -1 && fwTimer::GetTimeInMilliseconds() >= m_ActivationTime)
	{
		m_iTryToWakeUpScenarioPoints = poolSize;
		m_iTryToWakeUpPriorityAreaScenarioPoints = processPriorityAreaCargens ? poolSize : 0;
	}

	if(processPriorityAreaCargens)
	{
		bool carModelPicked = false;

		s32 priorityAreaCargensToProcess = poolSize;
		static s32 priorityAreaCargenIndex = 0;

		while(priorityAreaCargensToProcess != 0 && !carModelPicked)
		{
			if(priorityAreaCargenIndex >= poolSize)
			{
				priorityAreaCargenIndex = 0;
			}

			CCarGenerator* pCargen = CarGeneratorPool.GetSlot(priorityAreaCargenIndex);

			if(pCargen && pCargen->m_bInPrioArea)
			{
				carModelPicked = pCargen->Process(m_iTryToWakeUpPriorityAreaScenarioPoints > 0);
			}

			if(m_iTryToWakeUpPriorityAreaScenarioPoints > 0)
			{
				--m_iTryToWakeUpPriorityAreaScenarioPoints;
			}

			--priorityAreaCargensToProcess;
			++priorityAreaCargenIndex;
		}
	}

	if(CTheCarGenerators::GetNumQueuedVehs() == initialNumScheduled)
	{
		bool carModelPicked = false;

		s32 cargensToProcess = static_cast<s32>(ceil(static_cast<float>(poolSize) / (CVehiclePopulation::GetPopulationFrameRate() * CVehiclePopulation::GetPopulationCyclesPerFrame())));
		static s32 cargenIndex = 0;

		while(cargensToProcess != 0 && !carModelPicked)
		{
			if(cargenIndex >= poolSize)
			{
				cargenIndex = 0;
			}

			CCarGenerator* pCargen = CarGeneratorPool.GetSlot(cargenIndex);

			if(pCargen && !pCargen->m_bInPrioArea)
			{
				carModelPicked = pCargen->Process(m_iTryToWakeUpScenarioPoints > 0);
			}

			if(m_iTryToWakeUpScenarioPoints > 0)
			{
				--m_iTryToWakeUpScenarioPoints;
			}

			--cargensToProcess;
			++cargenIndex;
		}
	}

	// After the last batch is through, reset the timer.
	if(m_iTryToWakeUpScenarioPoints == 0 && m_iTryToWakeUpPriorityAreaScenarioPoints == 0)
	{
		m_ActivationTime = fwTimer::GetTimeInMilliseconds() + m_TimeBetweenScenarioActivationBoosts;
		m_iTryToWakeUpScenarioPoints = -1;
		m_iTryToWakeUpPriorityAreaScenarioPoints = -1;
	}
}

//
// name:		CTheCarGenerators::ProcessAll
// description:	Process all the car generators to ensure they are inplace before teleporting to an area
void CTheCarGenerators::ProcessAll()
{
	s32 i;
	// Process all the car generators
	for (i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
		if(pCargen)
		{
			pCargen->Process();	
		}	
	}

	// Load all vehicle streaming requests
	CStreaming::LoadAllRequestedObjects();

	GenerateScheduledVehiclesIfReady();

	// Process all the car generators again
	for (i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
		if(pCargen)
		{
			pCargen->Process();	
		}
	}

	bScriptForcingAllCarsLockedThisFrame = false;
}

//
// name:		CTheCarGenerators::GenerateScheduledVehiclesIfReady
// description:	Pops one scheduled vehicle from the queue and tries to spawn it if it's ready.
//				The function needs to be called every frame. It will advance through several stages for
//				each entry in the queue. It first waits for the collision probes to finish.
//				Second, it tries to place the vehicle properly on the road and will wait (early out)
//				until that work has finished. Finally it will set up the vehicle and return true for that
//				queue entry, unless something went wrong earlier at which point it should have dropped the entry from the queue
#define IGNORE_ASYNC_HANDLE ((u32)-2)
bool CTheCarGenerators::GenerateScheduledVehiclesIfReady()
{
	if(!ShouldGenerateVehicles())
	{
		return true;
	}

	s32 nextSlot = -1;
	int slotIndex = m_scheduledVehiclesNextIndex;
	const int numScheduledVehicles = m_scheduledVehicles.GetCount();
	for (s32 i = 0; i < numScheduledVehicles; ++i)
	{
		if(slotIndex >= numScheduledVehicles)
		{
			slotIndex = 0;
		}

		// try to spawn the first vehicle that passes all the tests
		if (m_scheduledVehicles[slotIndex].valid && PerformFinalReadyChecks(m_scheduledVehicles[slotIndex]))
		{
			nextSlot = slotIndex;
			break;
		}

		slotIndex++;
	}

	if (nextSlot == -1)
		return true;

	ScheduledVehicle& veh = m_scheduledVehicles[nextSlot];
	
	bool ignoreLocalPopulationLimits = false;
	if(NetworkInterface::IsGameInProgress() && veh.carGen->IsScenarioCarGen() && veh.carGen->m_bHighPriority)
	{
		ignoreLocalPopulationLimits = true;
	}

	bool success = false;
	// if we've scheduled the probes for placing the vehicle on road, it should have a valid handle
	if (veh.asyncHandle == INVALID_ASYNC_ENTRY)
	{
		PF_PUSH_TIMEBAR("GenerateScheduledVehicle");
		success = GenerateScheduledVehicle(veh, ignoreLocalPopulationLimits);
		PF_POP_TIMEBAR();
	}
	else
	{
		PF_PUSH_TIMEBAR("FinishScheduledVehicleCreation");
		success = FinishScheduledVehicleCreation(veh, ignoreLocalPopulationLimits);
		PF_POP_TIMEBAR();
	}
	
	if(!success)
	{
		// If FinishScheduledVehicleCreation() returned false and didn't drop the entry
		// from the scheduled queue, make sure that on the next frame, we give some other
		// car generator a chance.
		if(veh.valid)
		{
			Assert(nextSlot < 0xffff);
			m_scheduledVehiclesNextIndex = (u16)(nextSlot + 1);
			if(m_scheduledVehiclesNextIndex >= m_scheduledVehicles.GetCount())
			{
				m_scheduledVehiclesNextIndex = 0;
			}
		}		
	}

	return success;
}

bool CTheCarGenerators::IsClusteredScenarioCarGen( ScheduledVehicle &veh )
{
	if (!veh.carGen->IsScenarioCarGen())
		return false;

	CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*veh.carGen);
	if (!pt)
		return false;

	return pt->IsInCluster();
}

bool CTheCarGenerators::FinishScheduledVehicleCreation( ScheduledVehicle &veh, bool ignoreLocalPopulationLimits )
{
	PF_AUTO_PUSH_TIMEBAR("CTheCarGenerators");
	CInteriorInst* pIntInst = veh.pDestInteriorProxy ? veh.pDestInteriorProxy->GetInteriorInst() : NULL;

	u32 failed = 0;
    BANK_ONLY(veh.carGen->m_failCode = CCarGenerator::FC_NONE;)
	if (veh.asyncHandle != IGNORE_ASYNC_HANDLE && veh.vehicle)
	{
#if __BANK
		if(!CAutomobile::HasPlaceOnRoadProperlyFinished(veh.asyncHandle, pIntInst, veh.destRoomIdx, failed, veh.carGen->m_hitEntities, veh.carGen->m_hitPosZ))
#else
		if(!CAutomobile::HasPlaceOnRoadProperlyFinished(veh.asyncHandle, pIntInst, veh.destRoomIdx, failed))
#endif
		{
			Assert(!failed);
			return false;
		}

		if (pIntInst)
		{
			veh.pDestInteriorProxy = pIntInst->GetProxy();
		}
		BANK_ONLY(veh.carGen->placementFail = failed;)
			failed &= 0x000000ff;

		if (failed)
		{
			BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_spawnCollisionFail));
		}

		BANK_ONLY(veh.carGen->m_failPos = veh.creationMatrix.d;)
	}

	// If the car generator no longer exists, go through the same code path as if
	// the place-on-road failed, to clean up the vehicle, etc.
	if(!failed && veh.carGen->m_bDestroyAfterSchedule)
	{
		BANK_ONLY(veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_DESTROYED;)
		failed = true;
	}

	// some times the probes to place on road can take a long time and the player can get very close to the cargen
	// effectively having a vehicle spawn right in front or even on top of him.
	// a check and early out here fixes this but we've wasted the work :(
	bool bTooClose = false;
	if (!failed && !veh.forceSpawn && !veh.carGen->IsHighPri() && !veh.carGen->CheckIfWithinRangeOfAnyPlayers(bTooClose) && bTooClose)
	{
		BANK_ONLY(veh.carGen->m_failCode = CCarGenerator::FC_NOT_IN_RANGE;)
		failed = true;
	}

	// If we created a vehicle and we intend to keep it, and this is a scenario, we may need to
	// report it to the scenario cluster here. There are some cases where the scenario clusters may
	// no longer want the vehicle (for example, if the scenario points got streamed out), and in that
	// case, we want to make sure the vehicle gets destroyed here, instead of getting left in a
	// semi-spawned unmanaged state.
	if (!failed && veh.vehicle && veh.carGen->IsScenarioCarGen())
	{
		if(!CScenarioManager::ReportCarGenVehicleSpawning(*veh.vehicle, *veh.carGen))
		{
            BANK_ONLY(veh.carGen->m_failCode = CCarGenerator::FC_SCENARIO_POP_FAILED;)
			failed = true;
		}
	}

    if (!failed)
	{
		// do second BB test here, in some cases script might have placed a vehicle on a cargen, usually during/after mission cutscenes
        // or enough time has passed since the PlaceOnRoad test that an ambient vehicle has driven up on the cargen.
		if(!veh.carGen->DoSyncProbeForBlockingObjects(veh.carModelId, veh.creationMatrix, veh.vehicle))
		{
#if __BANK //override fail code ... 
			veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_PROBE_2_FAILED;
#endif // __BANK
            failed = true;
		}
	}

	if(failed && veh.vehicle)
	{
		PF_AUTO_PUSH_TIMEBAR("Destroy");
		veh.vehicle->SetIsScheduled(false);
		CVehicleFactory::GetFactory()->Destroy(veh.vehicle, true);
	}

	//dont release the chain user here ... 
	DropScheduledVehicle(veh, false);
	if (failed || !veh.vehicle)
	{
		if(veh.carGen)
		{
#if __BANK
            if (veh.carGen->m_failCode == CCarGenerator::FC_NONE)
				veh.carGen->m_failCode = failed ? (u8)CCarGenerator::FC_PLACE_ON_ROAD_FAILED : (u8)CCarGenerator::FC_VEHICLE_REMOVED;
#endif // __BANK

			// re-enable cargen since it might be an interior and collision just hasn't loaded yet
			veh.carGen->m_bReadyToGenerateCar = true;
		}

		if (veh.chainUser)
		{
			CScenarioPointChainUseInfo* info = veh.chainUser;
			veh.chainUser = NULL; //dont need this any longer but dont delete the user info ... the chaining system will handle that ... 

			CScenarioChainingGraph::UnregisterPointChainUser(*info);
		}

		return false;
	}

	if (!PostCreationSetup(veh, ignoreLocalPopulationLimits))
		return false;

	NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "PossiblyGenerateVeh", "Vehicle Created:", "%s", veh.vehicle->GetNetworkObject() ? veh.vehicle->GetNetworkObject()->GetLogName() : "Network Object Null");

	return true;
}

bool CTheCarGenerators::PostCreationSetup(ScheduledVehicle& veh, bool ignoreLocalPopulationLimits)
{
	// track created lowprio cars correctly
	CVehiclePopulation::UpdateVehCount(veh.vehicle, CVehiclePopulation::SUB);
	veh.vehicle->m_nVehicleFlags.bCreatedByLowPrioCargen = veh.carGen->m_bLowPriority;
	CVehiclePopulation::UpdateVehCount(veh.vehicle, CVehiclePopulation::ADD);

	veh.vehicle->m_CarGenThatCreatedUs = (s16)(veh.carGen - CTheCarGenerators::GetCarGenerator(0));

	// handle trail attachment
	const bool bUseBiggerBoxesForCollisionTest = false;
	CVehicle* trailer = CVehicleFactory::GetFactory()->CreateAndAttachTrailer(veh.trailerModelId, veh.vehicle, veh.creationMatrix, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, bUseBiggerBoxesForCollisionTest);

	if(veh.vehicle->InheritsFromBoat() || veh.vehicle->InheritsFromSubmarine() )
	{
		veh.vehicle->m_ExtendedRemovalRange = s16(1.5f * CVehiclePopulation::GetCullRange(1.0f, 1.0f));
	}
	else
	{
#if __ASSERT
		const Vector3 vNewVehiclePosition = VEC3V_TO_VECTOR3(veh.vehicle->GetTransform().GetPosition());
		Assertf(veh.vehicle->InheritsFromAutomobile() || veh.vehicle->InheritsFromBike(), "Non supported vehicle type - %s %x (%f %f %f)", veh.vehicle->GetModelName(), veh.carModelId.GetModelIndex(), vNewVehiclePosition.x, vNewVehiclePosition.y, vNewVehiclePosition.z);
#endif	//	__ASSERT

		veh.vehicle->m_OverrideLights = NO_CAR_LIGHT_OVERRIDE;			
		veh.vehicle->m_nVehicleFlags.bLightsOn = FALSE;
	}

	Assertf(veh.vehicle->GetStatus() != STATUS_WRECKED, "Created vehicle '%s' is already wrecked! %s", veh.vehicle->GetModelName(), veh.vehicle->GetIsInPopulationCache() ? "(from cache)" : "(not from cache)");
	veh.vehicle->SwitchEngineOff();
	veh.vehicle->SetIsAbandoned();

	bool forceAlarm = false;
	if (veh.vehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPORTS))
	{
		forceAlarm = true;
		veh.vehicle->SetCarAlarm(true);
		veh.vehicle->SetCarDoorLocks(CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED);
	}

	if (NetworkInterface::IsGameInProgress())
	{
		if (!bScriptForcingAllCarsLockedThisFrame && (veh.carGen->m_bCreateCarUnlocked || fwRandom::GetRandomNumberInRange(0.0f, 1.0f) > m_CarGenScriptLockedPercentage))
		{
			if (!forceAlarm)
			{
				veh.vehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
				veh.vehicle->SetCarAlarm(false);
			}
		}
		else
		{
			veh.vehicle->SetCarDoorLocks(CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED);
		}
		veh.vehicle->m_nVehicleFlags.bDontTryToEnterThisVehicleIfLockedForPlayer = veh.carGen->m_bPreventEntryIfNotQualified;
	}
	{
		if (veh.carGen->m_bCreateCarUnlocked || veh.vehicle->InheritsFromBoat()) // Never lock boats.
		{
			if (!forceAlarm)
			{
				veh.vehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
				veh.vehicle->SetCarAlarm(false);
			}
		}
		else
		{
			veh.vehicle->SetCarDoorLocks(CARLOCK_LOCKED_BUT_CAN_BE_DAMAGED);
		}

	}

	veh.vehicle->m_nVehicleFlags.bHasBeenOwnedByPlayer = veh.carGen->m_bPlayerHasAlreadyOwnedCar;		// If this is true player won't get wanted level for taking car
	veh.vehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = !veh.carGen->m_bPlayerHasAlreadyOwnedCar;

	// CNC: Unlock all ambient cars and disable hotwiring.
	TUNE_GROUP_BOOL(CNC_RESPONSIVENESS, bSpawnCarsUnlockedAndNoHotwiring, true);
	TUNE_GROUP_BOOL(CNC_RESPONSIVENESS, bForceSpawnCarsUnlockedAndNoHotwiringOutsideOfCNC, false);
	if ((bSpawnCarsUnlockedAndNoHotwiring && NetworkInterface::IsInCopsAndCrooks()) || bForceSpawnCarsUnlockedAndNoHotwiringOutsideOfCNC)
	{
		veh.vehicle->m_nVehicleFlags.bCarNeedsToBeHotwired = false;
		veh.vehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
		veh.vehicle->SetCarAlarm(false);
	}

	veh.vehicle->m_nVehicleFlags.bCanBeStolen = veh.carGen->m_bCanBeStolen;

	veh.vehicle->SetMatrix(veh.creationMatrix);

	CScenarioPointChainUseInfo* pChainUseInfoPtr = NULL;

	const CAmbientVehicleModelVariations* pVehVar = NULL;
	if(veh.carGen->IsScenarioCarGen())
	{
		if(ignoreLocalPopulationLimits)
		{
			CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(false);
		}
		// Let the scenario code add passengers, etc, and do other setup of the vehicle as needed.
		// Note: if this vehicle is a part of a cluster, we won't actually do anything here.
		bool populated = CScenarioManager::PopulateCarGenVehicle(*veh.vehicle, *veh.carGen, false, &pVehVar);
		if(ignoreLocalPopulationLimits)
		{
			CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(true);
		}

		if(!populated)
		{
#if __BANK
			veh.carGen->m_failCode = (u8)CCarGenerator::FC_SCENARIO_POP_FAILED;
#endif // __BANK

			BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioPopCarGenFailed));

			// Not sure about this, but it may make sense to try again in this case.
			veh.carGen->m_bReadyToGenerateCar = true;

			// Destroy the vehicle again - it appears that this reliably destroys any occupants
			// and cleans up properly.
			veh.vehicle->SetIsScheduled(false);
			CVehicleFactory::GetFactory()->Destroy(veh.vehicle, true);

			if (veh.chainUser)
			{
				CScenarioPointChainUseInfo* info = veh.chainUser;
				veh.chainUser = NULL; //dont need this any longer but dont delete the user info ... the chaining system will handle that ... 

				CScenarioChainingGraph::UnregisterPointChainUser(*info);
			}
			return false;
		}

		// If we have a chain user dummy
		if (veh.chainUser)
		{
			pChainUseInfoPtr = veh.chainUser;

			//Need to transfer ownership of this to the real user.
			CPed* driver = veh.vehicle->GetDriver();
			if (driver)
			{
				//get the task we were setup with ... "this should be task_unalerted"
				CTask* task = driver->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_UNALERTED);
				if (task)
				{
					CTaskUnalerted* tua = (CTaskUnalerted*)task;
					CScenarioChainingGraph::UpdateDummyChainUser(*veh.chainUser, driver, veh.vehicle);
					tua->SetScenarioChainUserInfo(veh.chainUser);						
				}
				else
				{
					CScenarioPointChainUseInfo* user = veh.chainUser;
					veh.chainUser = NULL;
					CScenarioChainingGraph::UnregisterPointChainUser(*user);
				}
			}
			else
			{
				CScenarioChainingGraph::UpdateDummyChainUser(*veh.chainUser, NULL, veh.vehicle);
			}				
			veh.chainUser = NULL; //dont need this any longer but dont delete the user info ... the chaining system will handle that ... 
		}
	}
	else
	{
		if (veh.vehicle->GetVehicleType() == VEHICLE_TYPE_BOAT)
		{
			((CBoat*)veh.vehicle.Get())->GetAnchorHelper().Anchor(true);
		}
	}

	fwInteriorLocation loc = CGameWorld::OUTSIDE;
	CInteriorInst* pIntInst = veh.pDestInteriorProxy ? veh.pDestInteriorProxy->GetInteriorInst() : NULL;
	if (pIntInst && pIntInst->CanReceiveObjects()){
		loc = CInteriorInst::CreateLocation(pIntInst, veh.destRoomIdx);
	}

#if __BANK
	CVehiclePopulation::AddVehicleToSpawnHistory(veh.vehicle, NULL, "CarGen");
	if(trailer)
		CVehiclePopulation::AddVehicleToSpawnHistory(trailer, NULL, "CarGen");
#endif

	// Add to the game world unless the user requested that we don't, or if the vehicle
	// has a trailer or is in an interior, for simplicity.
	if(veh.addToGameWorld || trailer || !loc.IsSameLocation(CGameWorld::OUTSIDE))
	{
		AddToGameWorld(*veh.vehicle, trailer, loc);
	}

	if (veh.carGen->GetLivery() > -1 && veh.carGen->GetLivery() < veh.vehicle->GetVehicleModelInfo()->GetLiveriesCount())
		veh.vehicle->SetLiveryId(veh.carGen->GetLivery());

	if (veh.carGen->GetLivery2() > -1 && veh.carGen->GetLivery2() < veh.vehicle->GetVehicleModelInfo()->GetLiveries2Count())
		veh.vehicle->SetLivery2Id(veh.carGen->GetLivery2());

	CVehicleModelInfo *pModelInfo = (CVehicleModelInfo *)CModelInfo::GetBaseModelInfo(veh.carModelId);
	pModelInfo->IncreaseNumTimesUsed();

	if (trailer)
	{
		CVehicleModelInfo* trailerMi = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfo(veh.trailerModelId);
		trailerMi->IncreaseNumTimesUsed();
	}

	if (trailer && trailer->GetVehicleModelInfo() && fwRandom::GetRandomNumberInRange(0.f, 1.f) > 0.5f)
	{
		if (trailer->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_ON_TRAILER))
		{
			CVehicleFactory::GetFactory()->AddCarsOnTrailer((CTrailer*)trailer, loc, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, NULL);
		}
		else if (trailer->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_SPAWN_BOAT_ON_TRAILER))
		{
			if (!CVehicleFactory::GetFactory()->AddBoatOnTrailer((CTrailer*)trailer, loc, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, NULL))
			{
				// We failed to create a boat. Check to see if we're still allowed to have this trailer
				CVehicleFactory::DestroyBoatTrailerIfNecessary((CTrailer**)&trailer);
			}
		}
	}

	if (!veh.vehicle->IsAlarmActivated())
	{
		// Use the car swankineess to determine if the vehicle should have an alarm
		// NOTE: PH: The cargen values aren't setup in maps so we ignore this/
		// If we need to use it we'll need to make sure all values are set to 1 in general.
		static float safSwankinessAlarmPercentages[SWANKNESS_MAX] = {0.0f, 0.15f, 0.25f, 0.35f, 0.5f, 1.0f};
		Assert( veh.vehicle->m_Swankness >= 0 && veh.vehicle->m_Swankness < SWANKNESS_MAX);
		float fChanceFromCarType = safSwankinessAlarmPercentages[veh.vehicle->m_Swankness];
		if (veh.vehicle->GetVehicleType() == VEHICLE_TYPE_CAR && 
			(fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < (fChanceFromCarType/**fChanceFromCargen*/)))
		{
			veh.vehicle->SetCarAlarm(true);
		}
	}

	//cars should slightly randomize their wheel angle
	if (veh.vehicle->InheritsFromAutomobile())
	{
		const float fMaxRandomSteerAngle = pModelInfo->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_DELIVERY)
			? 30.0f * DtoR
			: 8.0f * DtoR;

		mthRandom rnd(veh.vehicle->GetRandomSeed());
		const float fRandomSteerAngle = rnd.GetGaussian(0.0f, fMaxRandomSteerAngle);
		veh.vehicle->SetSteerAngle(fRandomSteerAngle);
	}

	// The idea here is that the car generators that Dave Bruce uses for his car collection mission always look the same.
	// This way the snapshots the player gets send to his phone don't match up.
	// This code only deals with the textures and not the attributes though.
	if (veh.carGen->m_bOwnedByScript && veh.carGen->m_bHighPriority)
	{	
		s32 newID = (pModelInfo->GetLiveriesCount() > 0) ? 0 : -1;
		veh.vehicle->SetLiveryId(newID);

		s32 new2ID = (pModelInfo->GetLiveries2Count() > 0) ? 0 : -1;
		veh.vehicle->SetLivery2Id(new2ID);
	}

	// car colours
	if ((veh.carGen->car_remap1 != 0xff) && (veh.carGen->car_remap2 != 0xff) && (veh.carGen->car_remap3 != 0xff))
	{
		veh.vehicle->SetBodyColour1(veh.carGen->car_remap1);
		veh.vehicle->SetBodyColour2(veh.carGen->car_remap2);
		veh.vehicle->SetBodyColour3(veh.carGen->car_remap3);
		veh.vehicle->SetBodyColour4(veh.carGen->car_remap4);
		veh.vehicle->SetBodyColour5(veh.carGen->car_remap5);
		veh.vehicle->SetBodyColour6(veh.carGen->car_remap6);
	}
	else
	{
		u8	Col1, Col2, Col3, Col4, Col5, Col6;
		((CVehicleModelInfo *)veh.vehicle->GetBaseModelInfo())->ChooseVehicleColourFancy(veh.vehicle, Col1, Col2, Col3, Col4, Col5, Col6);
		veh.vehicle->SetBodyColour1(Col1);
		veh.vehicle->SetBodyColour2(Col2);
		veh.vehicle->SetBodyColour3(Col3);
		veh.vehicle->SetBodyColour4(Col4);
		veh.vehicle->SetBodyColour5(Col5);
		veh.vehicle->SetBodyColour6(Col6);
	}

	if(pVehVar)
	{
		CScenarioManager::ApplyVehicleColors(*veh.vehicle, *pVehVar);
	}

	// if visible model is random store previous colours
	if (!NetworkInterface::IsGameInProgress())		// Car generators have no 'memory' in network games.
	{
		//if(VisibleModel < -1)
		if (veh.carGen->m_bStoredVisibleModel)
		{
			veh.carGen->car_remap1 = veh.vehicle->GetBodyColour1();
			veh.carGen->car_remap2 = veh.vehicle->GetBodyColour2();
			veh.carGen->car_remap3 = veh.vehicle->GetBodyColour3();
			veh.carGen->car_remap4 = veh.vehicle->GetBodyColour4();
			veh.carGen->car_remap5 = veh.vehicle->GetBodyColour5();
			veh.carGen->car_remap6 = veh.vehicle->GetBodyColour6();
		}
	}
	veh.vehicle->UpdateBodyColourRemapping();	// let shaders know, that body colours changed

	if(veh.carGen->IsScenarioCarGen())
	{
		// The scenario code has already had a chance to populate the vehicle - it's done earlier
		// because it might fail.
	}
	// Random vehicles occasionally have some people in it.
	else if ( (!veh.carGen->m_bHighPriority) && (!veh.carGen->m_bOwnedByScript) )
	{
#if 0 // turn this off, we don't allow the drivers to drive off anymore so it was an unnecessary extra cost for an ambient ped (that would let the engine run even! bastard!)
		static float perc = 0.03f;
		if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < perc && gPopStreaming.IsFallbackPedAvailable() && !NetworkInterface::IsGameInProgress())
		{
			if (veh.vehicle->GetSeatManager()->GetMaxSeats() > 0)
			{
				CVehiclePopulation::AddDriverAndPassengersForVeh(veh.vehicle, 0, 0);

				CPed *pDriver = veh.vehicle->GetDriver();
				if (pDriver)
				{
					veh.vehicle->SetCarDoorLocks(CARLOCK_UNLOCKED);
					veh.vehicle->SetCarAlarm(false);
					veh.vehicle->SwitchEngineOn(true);
					//float	random = fwRandom::GetRandomNumberInRange(0.0f, 1.0f);
					//if (random < 0.5f)
					if (0)
					{			// Driver waits for a random time and drives off
						CTaskSequenceList* pSequence=rage_new CTaskSequenceList;
						pSequence->AddTask(rage_new CTaskCarSetTempAction(veh.vehicle, MISSION_STOP, fwRandom::GetRandomNumberInRange(5000, 40000) ) );

						CTask* pVehicleTask = NULL;

						sVehicleMissionParams params;
						params.m_iDrivingFlags = DMode_AvoidCars_StopForPeds_ObeyLights;
						params.m_fCruiseSpeed = 12.0f;
						CTask* pTask = CVehicleIntelligence::CreateCruiseTask(*veh.vehicle, params);
						pVehicleTask = rage_new CTaskControlVehicle(veh.vehicle, pTask);

						pSequence->AddTask(pVehicleTask);
						pDriver->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskUseSequence(*pSequence), PED_TASK_PRIORITY_PRIMARY,true);
					}
					else
					{			// Driver waits for a random time and gets out off car
						CTaskSequenceList* pSequence=rage_new CTaskSequenceList;
						VehicleEnterExitFlags vehicleFlags;
						vehicleFlags.BitSet().Set(CVehicleEnterExitFlags::DelayForTime);
						pSequence->AddTask( rage_new CTaskExitVehicle(veh.vehicle,vehicleFlags, fwRandom::GetRandomNumberInRange(5.0f, 40.0f)) );
						pSequence->AddTask( rage_new CTaskWander(MOVEBLENDRATIO_WALK, fwRandom::GetRandomNumberInRange(-PI, PI) ));
						pDriver->GetPedIntelligence()->AddTaskAtPriority(rage_new CTaskUseSequence(*pSequence), PED_TASK_PRIORITY_PRIMARY,true);
					}
				}
			}
		}
#endif
	}

	if (veh.carGen->m_bHighPriority && veh.carGen->m_bOwnedByScript)
	{
		veh.vehicle->m_nVehicleFlags.bNeverUseSmallerRemovalRange = true;
	}

	veh.carGen->m_pLatestCar = veh.vehicle;

	Vector3 pos(veh.carGen->m_centreX, veh.carGen->m_centreY, veh.carGen->m_centreZ);
	veh.vehicle->SetParentCargenPos(pos);

	veh.carGen->m_bReadyToGenerateCar = false;				// Make sure we don't keep trying to generate cars here.
	veh.carGen->m_bHasGeneratedACar = true;

	//NOTE: Make sure we have a generate count before we subtract anything...
	// clusters that generate vehicles will not have the generate count as they 
	// schedule vehicles on their own and dont want to use the process code to 
	// schdeule vehicles for generation.
	if (veh.carGen->GenerateCount && veh.carGen->GenerateCount < CCarGenerator::INFINITE_CAR_GENERATE)
	{
		--veh.carGen->GenerateCount;
	}

#if __BANK
	CVehiclePopulation::DebugEventCreateVehicle(veh.vehicle, pModelInfo);
	veh.carGen->m_failCode = CCarGenerator::FC_NONE;
#endif

	// vehicle created - no longer scheduled
	veh.vehicle->SetIsScheduled(false);

	CSpatialArrayNode& spatialArrayNode = veh.vehicle->GetSpatialArrayNode();
	if(!spatialArrayNode.IsInserted())
	{
		CVehicle::GetSpatialArray().Insert(spatialArrayNode);
		if(spatialArrayNode.IsInserted())
		{
			veh.vehicle->UpdateInSpatialArray();
		}
	}

	if(veh.carGen->IsScenarioCarGen())
	{
		// Give final notification to the scenario manager that we have passed all tests and decided to keep
		// this vehicle. This sets the last spawn time in the scenario info.
		CScenarioManager::ReportCarGenVehicleSpawnFinished(*veh.vehicle, *veh.carGen, pChainUseInfoPtr);
		Assert(veh.chainUser == NULL);
		veh.chainUser = NULL; //just to be safe... 
	}

	//update the ped and vehicle AI once
	CPed* pDriver = veh.vehicle->GetDriver();
	if (pDriver)
	{
		// Normally, a call to CAutomobile::DoProcessControl() would convert to POPTYPE_RANDOM_AMBIENT
		// if we have a driver, but we need it to happen earlier, so we don't do the dummy conversion
		// as if this vehicle is parked.
		if(veh.vehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
		{
			veh.vehicle->PopTypeSet(POPTYPE_RANDOM_AMBIENT);
		}

		pDriver->InstantAIUpdate();
		pDriver->SetPedResetFlag(CPED_RESET_FLAG_SkipAiUpdateProcessControl, true);
	}

	veh.carGen->m_bIgnoreTooClose = false;

	bool bDummyConversionFailed = false;
	CVehiclePopulation::MakeVehicleIntoDummyBasedOnDistance(*veh.vehicle, veh.vehicle->GetVehiclePosition(), bDummyConversionFailed);

	return true;
}

void CTheCarGenerators::AddToGameWorld(CVehicle& vehicle, CVehicle* pTrailer, const fwInteriorLocation& loc)
{
	vehicle.EnableCollision();

	CGameWorld::Add(&vehicle, loc);	
	if (pTrailer)
		CGameWorld::Add(pTrailer, loc);

	static const float fMinFadeDist = 20.0f;

	// Determine whether to alpha-fade in these vehicles
	// I think we can safely assume that the bounding sphere is centerd around the vehicle's origin.
	if( CPedPopulation::IsCandidateInViewFrustum( VEC3V_TO_VECTOR3(vehicle.GetTransform().GetPosition()), vehicle.GetBaseModelInfo()->GetBoundingSphereRadius(), 9999.0f, fMinFadeDist) )
	{
		vehicle.GetLodData().SetResetDisabled(false);
		vehicle.GetLodData().SetResetAlpha(true);
	}
	if( pTrailer && CPedPopulation::IsCandidateInViewFrustum( VEC3V_TO_VECTOR3(pTrailer->GetTransform().GetPosition()), pTrailer->GetBaseModelInfo()->GetBoundingSphereRadius(), 9999.0f, fMinFadeDist) )
	{
		pTrailer->GetLodData().SetResetDisabled(false);
		pTrailer->GetLodData().SetResetAlpha(true);
	}
}

bool CTheCarGenerators::GenerateScheduledVehicle( ScheduledVehicle &veh, bool ignoreLocalPopulationLimits )
{
#if __BANK
	utimer_t startTime = sysTimer::GetTicks();
#endif

	if (veh.isAsync && veh.collisionTestResult.GetNumHits() > 0)
	{
		// There is something on top of the generator.
		// Keep trying to create vehicle next frame unless we're in a network game. In network games we ended up with piles of boats.
		if(veh.allowedToSwitchOffCarGenIfSomethingOnTop)
		{
			veh.carGen->m_bReadyToGenerateCar = false;
		}

#if __BANK
		veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_PROBE_FAILED;
#endif // __BANK

		DropScheduledVehicle(veh);
		BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_spawnCollisionFail));
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "GenerateScheduledVehiclesIfReady", "Failed:", "There is something on top of the generator");
		return false;
	}

	// when crossing popzone boundaries an already scheduled vehicle model can become inappropriate and flagged as being streamed out
	if (!CModelInfo::HaveAssetsLoaded(veh.carModelId))
	{
#if __BANK
		veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_VEH_STREAMED_OUT;
#endif // __BANK
		DropScheduledVehicle(veh);
		BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioVehModel));
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "GenerateScheduledVehiclesIfReady", "Failed:", "Scheduled vehicle model is now inappropriate");
		return false;
	}

	if (veh.needsStaticBoundsStreamingTest)
	{
        if(!g_StaticBoundsStore.GetBoxStreamer().HasLoadedAboutPos(VECTOR3_TO_VEC3V(veh.creationMatrix.d), fwBoxStreamerAsset::FLAG_STATICBOUNDS_MOVER))
		{
            //veh.carGen->m_bReadyToGenerateCar = true;
            BANK_ONLY(veh.carGen->m_failCode = CCarGenerator::FC_COLLISIONS_NOT_LOADED;)
            //DropScheduledVehicle(veh);
            //BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioCollisionNotLoaded));
            return false;
		}
	}

	// If this is a vehicle scenario, we may have to test the scenario chain to see if it leads
	// to a blocked parking spot or something like that.
	if(veh.carGen->IsScenarioCarGen())
	{
		CScenarioPoint* pt = CScenarioManager::GetScenarioPointForCarGen(*veh.carGen);
		if(pt && pt->IsChained())
		{
			// Note: not sure if this should really be an assert, but so far it seems to hold up.
			if(Verifyf(veh.carModelId.IsValid(), "Expected valid vehicle model."))
			{
				CParkingSpaceFreeTestVehInfo vehInfo;
				vehInfo.m_ModelInfo = CModelInfo::GetBaseModelInfo(veh.carModelId);

				Assert(vehInfo.m_ModelInfo);

				bool requestDone = false;
				bool usable = false;
				CScenarioManager::GetChainTests().RequestTest(*pt, vehInfo, requestDone, usable);
				if(!requestDone)
				{
					// Return false, but don't drop the vehicle, so we should try again soon
					// (probably next frame if no other scheduled vehicle gets in the way).
					return false;
				}

				if(!usable)
				{
					// In this case, we can't spawn here because there is something blocking the scenario
					// chain. Drop the scheduled vehicle and deactivate for a while.

					veh.carGen->m_bReadyToGenerateCar = false;
#if __BANK
					veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_BLOCKED_SCENARIO_CHAIN;
#endif // __BANK

					DropScheduledVehicle(veh);

					BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_scenarioCarGenBlockedChain));
					NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "GenerateScheduledVehiclesIfReady", "Failed:", "There is something blocking a part of the scenario chain");
					return false;
				}
			}
		}
	}

	const Vector3& vBoundBoxMin = (CModelInfo::GetBaseModelInfo(veh.carModelId))->GetBoundingBoxMin();
	const Vector3& vBoundBoxMax = (CModelInfo::GetBaseModelInfo(veh.carModelId))->GetBoundingBoxMax();

	if (CVehiclePopulation::WillVehicleCollideWithOtherCarUsingRestrictiveChecks(veh.creationMatrix, vBoundBoxMin, vBoundBoxMax))
	{
#if __BANK
		veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_RESTRICTIVE_CHECKS_FAILED;
#endif // __BANK
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR","GenerateScheduledVehiclesIfReady", "Failed:", "Skip vehicle spawn, WillVehicleCollideWithOtherCarUsingRestrictiveChecks");

		DropScheduledVehicle(veh);
		veh.carGen->m_bReadyToGenerateCar = veh.carGen->m_bHighPriority;
		return false;
	}

	// bug 2144284 : I think that invalid types get queued up outwith of MP game, so trap them here and remove them
	if (NetworkInterface::IsGameInProgress() && !CVehicle::IsTypeAllowedInMP(veh.carModelId.GetModelIndex()))
	{
		BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_notAllowdByNetwork));
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "GenerateScheduledVehiclesIfReady", "Failed:", "Vehicle type not allowed in MP");
#if __BANK
		veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_NOT_ALLOWED_IN_MP;
#endif // __BANK
		DropScheduledVehicle(veh);
		return false;
	}

	if(ignoreLocalPopulationLimits)
	{
		CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(false);
	}
	CVehicle *pNewVehicle = CVehicleFactory::GetFactory()->Create(veh.carModelId, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, &veh.creationMatrix, true, true);

	// track cargen vehicles created this frame	
	PF_INCREMENT(CargenVehiclesCreated);
#if __BANK
	if(pNewVehicle)
		pNewVehicle->m_CreationMethod = CVehicle::VEHICLE_CREATION_CARGEN;

	//! Debug to check we haven't spawned a cargen outside scope distance
	if (NetworkInterface::IsGameInProgress() && CGameWorld::GetMainPlayerInfo())
	{
		Vector3 PlayerPos = VEC3V_TO_VECTOR3(CGameWorld::GetMainPlayerInfo()->GetPlayerPed()->GetTransform().GetPosition());
		Vector3 vGenPos(veh.carGen->m_centreX, veh.carGen->m_centreY, veh.carGen->m_centreZ);

		Vector3 vDistance = vGenPos - PlayerPos;
		float fDist = vDistance.Mag();
		//Displayf("Car Gen Spawn Distance: %f", fDist);

		if(fDist > CNetObjVehicle::GetStaticScopeData().m_scopeDistance)
		{
			Warningf("Vehicle spawned outside network scope distance, this could cause vehicle pileups! %f/%f", fDist, CNetObjVehicle::GetStaticScopeData().m_scopeDistance);
		}
	}

#endif // __BANK

	if(ignoreLocalPopulationLimits)
	{
		CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(true);
	}

	if (!pNewVehicle)
	{
		BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_factoryFailedToCreateVehicle));
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "GenerateScheduledVehiclesIfReady", "Failed:", "Vehicle Factory can't create a vehicle");
#if __BANK
		veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_FACTORY_FAIL;
#endif // __BANK
		DropScheduledVehicle(veh);

		// re-enable generation on this one
		veh.carGen->m_bReadyToGenerateCar = true;
		return false;
	}

	pNewVehicle->m_nVehicleFlags.bCreatedByCarGen = true;

	Assertf(pNewVehicle->GetStatus() != STATUS_WRECKED, "Created vehicle '%s' is already wrecked! %s", pNewVehicle->GetModelName(), pNewVehicle->GetIsInPopulationCache() ? "(from cache)" : "(not from cache)");

	// DAN TEMP - we've created a car - log where we think each Player is to help catch the multiple car gen bug
#if __DEV
	if(pNewVehicle->GetNetworkObject())
	{
		//NetworkInterface::GetObjectManagerLog().WriteMessageHeader(CNetworkLog::LOG_LEVEL_MEDIUM, pNewVehicle->GetNetworkObject()->GetPlayerIndex(), "CREATED_CAR_ON_CAR_GEN", pNewVehicle->GetNetworkObject()->GetLogName());

		if (NetworkInterface::IsGameInProgress())
		{
			// Players with a higher Player id have priority when generating cars: if a player with a higher Player id is near the
			// generating range then we are not allowed to generate a parked car. We also cannot generate a parked car if another
			// player with a lower priority is too close
			unsigned                 numRemotePhysicalPlayers = netInterface::GetNumRemotePhysicalPlayers();
			const netPlayer * const *remotePhysicalPlayers    = netInterface::GetRemotePhysicalPlayers();

			for(unsigned index = 0; index < numRemotePhysicalPlayers; index++)
			{
				const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, remotePhysicalPlayers[index]);

				if(pPlayer->GetPlayerPed())
				{
					Vector3 PlayerPos = VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition());

					if(pPlayer->GetPlayerPed()->IsNetworkClone())
					{
						CNetObjPlayer *netObjPlayer = static_cast<CNetObjPlayer  *>(pPlayer->GetPlayerPed()->GetNetworkObject());
						netObjPlayer->GetAccurateRemotePlayerPosition(PlayerPos);
					}

					//const float XDiff = PlayerPos.x - m_centreX;
					//const float YDiff = PlayerPos.y - m_centreY;
					//const float DistanceSqr = XDiff * XDiff + YDiff * YDiff;
					//float distance = rage::Sqrtf(DistanceSqr);

					//netObject *playerObj = pPlayer->GetPlayerPed()->GetNetworkObject();
					//NetworkInterface::GetObjectManagerLog().WriteDataHeader(CNetworkLog::LOG_LEVEL_MEDIUM, playerObj ? playerObj->GetLogName() : "Unknown");
					//NetworkInterface::GetObjectManagerLog().WriteData(CNetworkLog::LOG_LEVEL_MEDIUM, false, "%.2f, %.2f, %.2f (%.2fm)", PlayerPos.x, PlayerPos.y, PlayerPos.z, distance);
					//NetworkInterface::GetObjectManagerLog().LineBreak(CNetworkLog::LOG_LEVEL_MEDIUM);
				}
			}
		}
	}
#endif // __DEV
	// END DAN TEMP

	// place newly created on the road properly
	u32 asyncHandle = INVALID_ASYNC_ENTRY;
	CInteriorInst* pIntInst = veh.pDestInteriorProxy ? veh.pDestInteriorProxy->GetInteriorInst() : NULL;
	if (!veh.carGen->PlaceOnRoadProperly(pNewVehicle, &veh.creationMatrix, pIntInst, veh.destRoomIdx, asyncHandle, true))
	{
		if (!veh.carGen->m_bHighPriority)
		{
			veh.carGen->m_bReadyToGenerateCar = false;
		}
		else
		{
			// re-enable generation on this cargen since we've disabled it when we first scheduled the vehicle
			veh.carGen->m_bReadyToGenerateCar = true;
		}

#if __BANK
		veh.carGen->m_failCode = CCarGenerator::FC_PLACE_ON_ROAD_FAILED;
#endif // __BANK

		BANK_ONLY(veh.carGen->RegisterSpawnFailure(CPedPopulation::PedPopulationFailureData::FR_spawnCollisionFail));
		NetworkInterface::WriteDataPopulationLog("CAR_GENERATOR", "GenerateScheduledVehiclesIfReady", "Failed:", "Not Placed On Road Properly");

		Assert(asyncHandle == INVALID_ASYNC_ENTRY);
		veh.asyncHandle = INVALID_ASYNC_ENTRY; // for debugging
		DropScheduledVehicle(veh);
		CVehicleFactory::GetFactory()->Destroy(pNewVehicle, true);
		return false;
	}
	else
	{
		Assertf(pNewVehicle->GetStatus() != STATUS_WRECKED, "Created vehicle '%s' is already wrecked! %s", pNewVehicle->GetModelName(), pNewVehicle->GetIsInPopulationCache() ? "(from cache)" : "(not from cache)");

		veh.pDestInteriorProxy = pIntInst ? pIntInst->GetProxy() : NULL;

		// mark vehicle as scheduled
		pNewVehicle->SetIsScheduled(true);
		pNewVehicle->GetVehicleAudioEntity()->SetWasScheduledCreation(true);
		pNewVehicle->SetAllowFreezeWaitingOnCollision(true);
		pNewVehicle->m_nVehicleFlags.bShouldFixIfNoCollision = true;
		//pNewVehicle->m_nPhysicalFlags.bAllowFreezeIfNoCollision = true;
		//pNewVehicle->PlaceOnRoadAdjust();

		// Remove it from the spatial array while it's being placed, so it doesn't get found by other
		// systems looking for vehicles.
		CSpatialArrayNode& spatialArrayNode = pNewVehicle->GetSpatialArrayNode();
		if(spatialArrayNode.IsInserted())
		{
			CVehicle::GetSpatialArray().Remove(spatialArrayNode);
		}

#if __BANK
		if(pNewVehicle)
		{
			pNewVehicle->m_iCreationCost = (u32)(sysTimer::GetTicksToMicroseconds() * (sysTimer::GetTicks() - startTime));
		}
#endif // __BANK

		pNewVehicle->DisableCollision();

		if (asyncHandle != INVALID_ASYNC_ENTRY)
		{
			veh.asyncHandle = asyncHandle;
			veh.vehicle = pNewVehicle;
			return false;	
		}
		else
		{
			// this is used for synchronous version of PlaceOnRoadProperly, currently used for boats and bikes
			veh.asyncHandle = IGNORE_ASYNC_HANDLE;
			veh.vehicle = pNewVehicle;
			return false;
		}
	}
}

bool CTheCarGenerators::PerformFinalReadyChecks( ScheduledVehicle &veh )
{
	if (veh.isAsync && !veh.collisionTestResult.GetResultsReady())
	{
		// if something bad happened to the test we get rid of the entry
		if (!veh.collisionTestResult.GetWaitingOnResults() || !Verifyf(veh.carGen, "Cargen pointer missing from scheduled vehicle entry!"))
		{
#if __BANK
			if (!veh.collisionTestResult.GetWaitingOnResults())
				veh.carGen->m_failCode = CCarGenerator::FC_CARGEN_PROBE_SCHEDULE_FAILED;
#endif // __BANK
			DropScheduledVehicle(veh);
		}

		return false;
	}

	if (veh.carGen->IsSwitchedOff() && !IsClusteredScenarioCarGen(veh))
	{
		DropScheduledVehicle(veh);

		if(veh.vehicle)
		{
			veh.vehicle->SetIsScheduled(false);
			CVehicleFactory::GetFactory()->Destroy(veh.vehicle, true);
		}

		return false;
	}

	// if we are not force spawning then do some more checks for validity ... 
	if (!veh.forceSpawn)
	{
		// If cargen is too far from any player then we shouldn't generate (will be removed straight away) - so drop scheduled vehicle
		// If cargen is too close to any player then we cannot generate, just wait for a while
		// What may have happened is the camera swung around since the car was scheduled, and it is now onscreen
		// cargens can ignore being too close, e.g. during a mission replay when cargens are spawning while screen is faded

		bool bTooClose = false;
        bool bDiscarded = gPopStreaming.IsCarDiscarded(veh.carModelId.GetModelIndex());
		if((!veh.carGen->m_bIgnoreTooClose && !veh.carGen->CheckIfWithinRangeOfAnyPlayers(bTooClose)) || bDiscarded)
		{	
			DropScheduledVehicle(veh);

			// re-enable generation on this cargen since we've disabled it when we first scheduled the vehicle
            if (veh.carGen)
			{
				veh.carGen->m_bReadyToGenerateCar = veh.carGen->m_bHighPriority;
			}

			if(veh.vehicle)
			{
				veh.vehicle->SetIsScheduled(false);
				CVehicleFactory::GetFactory()->Destroy(veh.vehicle, true);
			}

#if __BANK
            if (veh.carGen)
			{
                veh.carGen->m_failCode = (u8)(bDiscarded ? CCarGenerator::FC_VEHICLE_REMOVED : CCarGenerator::FC_NOT_IN_RANGE);
			}
#endif
            return false;
		}
		if(bTooClose)
		{
			BANK_ONLY(if (veh.carGen) veh.carGen->m_failCode = CCarGenerator::FC_TOO_CLOSE_AT_GEN_TIME;)
            return false;
		}
	}

	// If we get here and the car generator has been destroyed, and we don't have an asynchronous
	// query in progress, and we haven't created a vehicle yet, it should be safe to stop processing.
	if(veh.carGen->m_bDestroyAfterSchedule
		&& (veh.asyncHandle == INVALID_ASYNC_ENTRY || veh.asyncHandle == IGNORE_ASYNC_HANDLE)
		&& !veh.vehicle)
	{
		DropScheduledVehicle(veh);
		return false;
	}

	return true;//we passed all the checks ... 
}

////////////////////////////////////////////////////////
// CTheCarGenerators::CreateCarGenerator
//
// input		: (x,y,z) position of centre
//					rotation of the cars which will be created, 
//					visible car model to be generated, 
//					remap 1 and 2 for the generated cars, 
//					is high priority generator flag,
//					%age chance of car alarm, %age chance of being locked, 
//					min & max delay between generations
// return value	: index of the new car generator in the CarGeneratorArray
//
////////////////////////////////////////////////////////

s32 CTheCarGenerators::CreateCarGenerator(const float (centreX), const float (centreY), const float (centreZ),
											const float (vec1X), const float (vec1Y),
											const float (lengthVec2),
											const u32 (VisModHashKey), 
											const s32 (car_rmap1), const s32 (car_rmap2), const s32 (car_rmap3), const s32 (car_rmap4), const s32 (car_rmap5), const s32 (car_rmap6),
											u32 flags,
											const u32 (ipl),
											const bool bCreatedByScript, const u32 creationRule, const u32 scenario,
                                            atHashString popGroup, s8 livery, s8 livery2, bool unlocked, bool canBeStolen)
{
#if (__DEV) && (__ASSERT)
	s32 i;
	// Here we make sure the car generator that is being added doesn't already exist.
	for (i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		const CCarGenerator* pCarGen = CarGeneratorPool.GetSlot(i);
#if STORE_CARGEN_PLACEMENT_FILE // additional debug info for this assert
		Assertf(!pCarGen
				|| pCarGen->m_bDestroyAfterSchedule	// Don't fail the assert if the existing car generator is scheduled to be destroyed, that's not considered a true duplicate.
				|| !(centreX == pCarGen->m_centreX && centreY == pCarGen->m_centreY && centreZ == pCarGen->m_centreZ),
				"Double generators at %i: X:%f Y:%f Z:%f Ipls: %s(%d),%s(%d) - was placed by: %s", i, centreX, centreY, centreZ, INSTANCE_STORE.GetName(strLocalIndex(ipl)), ipl, pCarGen->IsScenarioCarGen() ? "SCENARIO" : INSTANCE_STORE.GetName(strLocalIndex(pCarGen->m_ipl)), pCarGen->m_ipl, pCarGen->m_placedFrom);
#else
		Assertf(!pCarGen
			|| pCarGen->m_bDestroyAfterSchedule	// Don't fail the assert if the existing car generator is scheduled to be destroyed, that's not considered a true duplicate.
			|| !(centreX == pCarGen->m_centreX && centreY == pCarGen->m_centreY && centreZ == pCarGen->m_centreZ),
			"Double generators at %i: X:%f Y:%f Z:%f Ipls: %s(%d),%s(%d)", i, centreX, centreY, centreZ, INSTANCE_STORE.GetName(ipl), ipl, pCarGen->IsScenarioCarGen() ? "SCENARIO" : INSTANCE_STORE.GetName(pCarGen->m_ipl), pCarGen->m_ipl);
#endif
	}
#endif

	const bool bTooThin = (lengthVec2 < 0.1f) || ((vec1X*vec1X + vec1Y*vec1Y) < 0.02f);
	Assertf(!bTooThin, "The car generator at (%.1f, %.1f, %.1f) is very thin and could cause problems - it will not be added.\n", centreX, centreY, centreZ);

	if(bTooThin)
		return -1;

	bool bSinglePlayerGame = (flags & CARGEN_SINGLE_PLAYER) != 0;
	bool bNetworkGame = (flags & CARGEN_NETWORK_PLAYER) != 0;

	// Ignore the ones that aren't needed (network/non network)
	if (bSinglePlayerGame || bNetworkGame)		// Only consider jumping out if the artist has set at least one of the flags (The flags have probably not been set up correctly yet)
	{
		if (NetworkInterface::IsGameInProgress())
		{
			if (!bNetworkGame)
			{
				return -1;
			}
		}
		else
		{
			if (!bSinglePlayerGame)
			{
				return -1;
			}
		}
	}

    bool destroyOneCargen = false;

	// If we are creating a car generator for a scenario, and the pool is full, we may want
	// to try to free up some space.
	const bool isScenarioCarGen = (scenario != CARGEN_SCENARIO_NONE);
	if (isScenarioCarGen && CarGeneratorPool.IsFull())
	{
		// If it's a high priority scenario, or if the number of car generators we are using
		// for scenarios isn't high already, try to remove one of the art-placed car generators.
		if ((flags & CARGEN_FORCESPAWN) != 0 || (m_NumScenarioCarGenerators < m_MinAllowedScenarioCarGenerators))
		{
            destroyOneCargen = true;
		}
	}

    if (bCreatedByScript && CarGeneratorPool.IsFull())
        destroyOneCargen = true;

    if (destroyOneCargen)
		TryToDestroyUnimportantCarGen();

	if(!CarGeneratorPool.IsFull() && !(bCreatedByScript && m_NumScriptGeneratedCarGenerators >= MAX_CAR_GENERATORS_CREATED_BY_SCRIPT))
	{
		CCarGenerator* pCargen = CarGeneratorPool.Construct();
		pCargen->Setup(centreX,centreY,centreZ,vec1X,vec1Y,lengthVec2,VisModHashKey,car_rmap1,car_rmap2,car_rmap3,car_rmap4,car_rmap5,car_rmap6, flags, ipl, creationRule, scenario, bCreatedByScript, popGroup, livery, livery2, unlocked, canBeStolen);
		NumOfCarGenerators++;

		if(isScenarioCarGen)
		{
			m_NumScenarioCarGenerators++;
		}
		
		if(bCreatedByScript)
			m_NumScriptGeneratedCarGenerators++;

		u32 index = CarGeneratorPool.GetIndex(pCargen);
		vehPopDebugf2("[CARGEN]: Creating %s %s cargen (pool slot %d, used %" SIZETFMT "u) at %.2f %.2f %.2f\n", pCargen->m_bOwnedByScript ? "(script)" : "", pCargen->IsScenarioCarGen() ? "(scenario)" : "", index, CarGeneratorPool.GetCount(), pCargen->GetCentre().GetXf(), pCargen->GetCentre().GetYf(), pCargen->GetCentre().GetZf());

		Assert(ipl < 0xffff);
		ms_CarGeneratorIPLs[index] = (u16)ipl;
		return CarGeneratorPool.GetIndex(pCargen);
	}

	if (bCreatedByScript)
	{
#if !__FINAL
		DumpCargenInfo();
#endif // !__FINAL

		Assertf(0, "CTheCarGenerators::CreateCarGenerator - ran out of script-created car generators or room for car generators. Total Cargens: %" SIZETFMT "u / %" SIZETFMT "u Script Cargens: %u / %u",
			CarGeneratorPool.GetSize() - CarGeneratorPool.GetFreeCount(), CarGeneratorPool.GetSize(), m_NumScriptGeneratedCarGenerators, MAX_CAR_GENERATORS_CREATED_BY_SCRIPT);
	}

	return -1;
}

void CTheCarGenerators::DestroyCarGeneratorByIndex(s32 index, bool shouldBeScenarioCarGen)
{
	Assert(index >= 0 && index < CarGeneratorPool.GetSize());
	CCarGenerator* pCargen = CarGeneratorPool.GetSlot(index);
	if(Verifyf(pCargen, "Expected car generator %d to be in use.", index))
	{
		if(pCargen->IsScenarioCarGen() != shouldBeScenarioCarGen)
		{
#if __ASSERT
			if(shouldBeScenarioCarGen)
			{
				Assertf(0, "Trying to destroy car generator %d, but it doesn't seem to be a vehicle scenario as was expected.", index);
			}
			else
			{
				char buff[256];
				CScenarioManager::GetInfoStringForScenarioUsingCarGen(buff, sizeof(buff), index);
				Assertf(0, "Trying to destroy car generator %d, but it belongs to a vehicle scenario! "
						"If this is coming from a script, make sure that you really own the car generator "
						"you are trying to destroy, and don't try to destroy it more than once. The car generator is at %.1f, %.1f, %.1f. Scenario point is %s.", index,
						pCargen->m_centreX, pCargen->m_centreY, pCargen->m_centreZ, buff);
			}
#endif	// __ASSERT
			return;
		}

		if(pCargen->m_bScheduled)
		{
			// trying to delete a cargen that's scheduled. Mark it for deletion later
			pCargen->m_bDestroyAfterSchedule = true;
			return;
		}
		
		if(pCargen->m_bOwnedByScript)
			m_NumScriptGeneratedCarGenerators--;

		if(pCargen->IsScenarioCarGen())
		{
			Assert(m_NumScenarioCarGenerators > 0);
			m_NumScenarioCarGenerators--;
		}

		ms_CarGeneratorIPLs[index] = 0; // invalid ipl
#if __BANK // Clean up RegdEntities in BANK - otherwise we get an assert about ref pointers to this Cargen
		pCargen->RemoveRegdEntities();
#endif
		CarGeneratorPool.Destruct(pCargen);
		NumOfCarGenerators--;
		Assertf(NumOfCarGenerators >= 0, "CTheCarGenerators::DestroyCarGeneratorByIndex - number of car generators is negative");

		vehPopDebugf2("[CARGEN]: Destroying %s %s cargen (pool slot %d, used %" SIZETFMT "u) at %.2f %.2f %.2f\n", pCargen->m_bOwnedByScript ? "(script)" : "", pCargen->IsScenarioCarGen() ? "(scenario)" : "", index, CarGeneratorPool.GetCount(), pCargen->GetCentre().GetXf(), pCargen->GetCentre().GetYf(), pCargen->GetCentre().GetZf());
	}
}

void CTheCarGenerators::TryToDestroyUnimportantCarGen()
{
	const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	if(!pLocalPlayer)
	{
		return;
	}
	const Vec3V centerPosV = pLocalPlayer->GetTransform().GetPosition();

	ScalarV bestScoreV(V_ZERO);
	int bestCarGeneratorIndex = -1;

	const ScalarV notReadyScoreAddV(square(100.0f));	// MAGIC!
	const ScalarV randomAddV(square(100.0f));			// MAGIC!

	// We don't really need to loop through the whole array if we can avoid it,
	// so we'll stop once we've found a certain number of car generators that are
	// good enough. Note that it's not a requirement for the best candidate to beat
	// this, it's just a way to stop early for performance reasons.
	const ScalarV scoreConsideredGoodEnoughV(square(300.0f));	// MAGIC!
	const int kNumGoodEnoughRequiredBeforeStop = 6;				// MAGIC!
	int numGoodEnoughFound = 0;

	// Generate a random index that we will start at, to increase the amount of randomness
	// we get without having to look at too many candidates.
	const int sz = (int)CarGeneratorPool.GetSize();
	int index = fwRandom::GetRandomNumberInRange(0, sz);
	Assert(index < sz);

	// This random number generator is used to add some randomness to the scoring.
	// This class currently doesn't seem to support setting a seed, so the numbers are not
	// going to be all that random, but since there are other variables changing between
	// the calls to this function, it probably won't matter much.
	mthVecRand rnd;

	// Loop over all the car generators, starting at the random index.
	for(int j = 0; j < sz; j++)
	{
		// This is the index we are using for this iteration.
		const int currentIndex = index;

		// Move on to the next index, for the next iteration.
		index++;
		if(index >= sz)
		{
			index = 0;
		}

		const CCarGenerator* pCarGenerator = CarGeneratorPool.GetSlot(currentIndex);
		if(pCarGenerator)
		{
			Assert(pCarGenerator->IsUsed());

			// If it's currently scheduled, it's best to let it continue.
			if(pCarGenerator->m_bScheduled)
			{
				continue;
			}

			// Don't remove anything created by script or belonging to a scenario.
			if(pCarGenerator->IsOwnedByScript() || pCarGenerator->IsScenarioCarGen())
			{
				continue;
			}

			// Don't remove any high priority generators.
			if(pCarGenerator->IsHighPri())
			{
				continue;
			}

			const Vec3V posV = pCarGenerator->GetCentre();
			const ScalarV distSqV = DistSquared(posV, centerPosV);

			// Start off with the squared distance as the score (a higher number is a better candidate for removal).
			ScalarV scoreV = distSqV;

			// If we are not ready to generate a car, increase the score. This could be a car generator
			// that already failed for some reason, and is probably less likely to be useful.
			if(!pCarGenerator->m_bReadyToGenerateCar)
			{
				scoreV = Add(scoreV, notReadyScoreAddV);
			}

			// Add some randomness to the score. This is done so we don't end up stripping a whole
			// parking lot clean or something like that, if we have to free up multiple car generators.
			ScalarV rndV = rnd.GetScalarV();
			scoreV = AddScaled(scoreV, randomAddV, rndV);

			if(IsGreaterThanAll(scoreV, bestScoreV))
			{
				bestScoreV = scoreV;
				bestCarGeneratorIndex = currentIndex;
			}

			// Check to see if this car generator should be considered good enough,
			// and if so, increase the count of such objects. Note that this is done independently
			// of whether it's the best we've found so far or not.
			if(IsGreaterThanAll(scoreV, scoreConsideredGoodEnoughV))
			{
				numGoodEnoughFound++;

				// Check to see if we have found enough candidates considered good enough,
				// and if so, break, and we will use the best we found.
				if(numGoodEnoughFound >= kNumGoodEnoughRequiredBeforeStop)
				{
					break;
				}
			}
		}
	}

	if(bestCarGeneratorIndex >= 0)
	{
#if !__NO_OUTPUT
		const CCarGenerator* pCarGenToRemove = CarGeneratorPool.GetSlot(bestCarGeneratorIndex);
		const Vec3V carGenPosV = pCarGenToRemove->GetCentre();
		vehPopDebugf1("Removing art-placed car generator at (%.1f, %.1f, %.1f) to free pool space for vehicle scenarios.",
				carGenPosV.GetXf(), carGenPosV.GetYf(), carGenPosV.GetZf());
#endif
		DestroyCarGeneratorByIndex(bestCarGeneratorIndex, false);
	}
}

//
// name:		RemoveCarGenerators
// description:	Remove all the car generators with certain ipl index
void CTheCarGenerators::RemoveCarGenerators(u32 ipl)
{
	for(s32 i=0; i<CarGeneratorPool.GetSize(); i++)
	{
		if (ms_CarGeneratorIPLs[i] == (u16)ipl)
		{
			CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
			if(pCargen)
			{
				Assertf(pCargen->m_ipl == (u16)ipl, "Mapdata removing cargen %d with bad ipl index: %d - %d", i, pCargen->m_ipl, ipl);
				Assertf(!pCargen->m_bOwnedByScript, "Mapdata removing cargen %d owned by script (ipl %d)", i, ipl);
				Assertf(!pCargen->IsScenarioCarGen(), "Mapdata removing scenario cargen %d (ipl %d)", i, ipl);

				if(pCargen->m_bScheduled)
				{
					// trying to delete a cargen that's scheduled. Mark it for deletion later
					pCargen->m_bDestroyAfterSchedule = true;
					continue;
				}

				ms_CarGeneratorIPLs[i] = 0;
				BANK_ONLY(pCargen->RemoveRegdEntities();)

				CarGeneratorPool.Destruct(pCargen);
				NumOfCarGenerators--;
				Assertf(NumOfCarGenerators >= 0, "CTheCarGenerators::DestroyCarGeneratorByIndex - number of car generators is negative");
			}
		}
	}
}

//
// name:		SetCarGeneratorsActiveInArea
// description:	Sets the active flag for the car generators in a specific area

void CTheCarGenerators::SetCarGeneratorsActiveInArea(const Vector3 &vecMin, const Vector3 &vecMax, bool bActive)
{
	const float fEpsilon = 0.001f;

		// If the car generator is switched to in-active we also store this data so that when car generators get streamed in we can
		// switch them off also.
	if (!bActive)
	{
		bool bAlreadyExistsInList = false;
		int t = 0;
		while (t < m_numCarGenSwitches)
		{
			if ( (m_CarGenSwitchedOffMin[t].IsClose(vecMin, fEpsilon)) && (m_CarGenSwitchedOffMax[t].IsClose(vecMax, fEpsilon)) )
			{
				bAlreadyExistsInList = true;
				break;
			}
			t++;
		}

		if (!bAlreadyExistsInList)
		{
			if (m_numCarGenSwitches<MAX_NUM_CAR_GENERATOR_SWITCHES)
			{
				m_CarGenSwitchedOffMin[m_numCarGenSwitches] = vecMin;
				m_CarGenSwitchedOffMax[m_numCarGenSwitches] = vecMax;
				m_numCarGenSwitches++;
			}
			else
			{
#if __BANK	//	I'd like to get this info in BankRelease builds too
				scriptDebugf3("CTheCarGenerators::SetCarGeneratorsActiveInArea - too many areas have been switched off\n");
				scriptDebugf3("Attempting to switch off <<%f,%f,%f>>  <<%f,%f,%f>>\n", vecMin.GetX(), vecMin.GetY(), vecMin.GetZ(), vecMax.GetX(), vecMax.GetY(), vecMax.GetZ());
				scriptDebugf3("The following areas are already switched off\n");
				for (s32 DebugLoop = 0; DebugLoop < MAX_NUM_CAR_GENERATOR_SWITCHES; DebugLoop++)
				{
					const Vector3 &vecMinToPrint = m_CarGenSwitchedOffMin[DebugLoop];
					const Vector3 &vecMaxToPrint = m_CarGenSwitchedOffMax[DebugLoop];

					scriptDebugf3("Array entry %d <<%f,%f,%f>>  <<%f,%f,%f>>\n", DebugLoop, 
						vecMinToPrint.GetX(), vecMinToPrint.GetY(), vecMinToPrint.GetZ(), 
						vecMaxToPrint.GetX(), vecMaxToPrint.GetY(), vecMaxToPrint.GetZ());
				}
#endif	//	__BANK
				scriptAssertf(0, "SET_CAR_GENERATORS_ACTIVE_IN_AREA: Too many car generator switches");
			}
		}
	}
	else
	{
		// Nodes are getting switched back on. Check whether this switch existed and remove it.
		int t = 0;
		while (t < m_numCarGenSwitches)
		{
			if ( (m_CarGenSwitchedOffMin[t].IsClose(vecMin, fEpsilon)) && (m_CarGenSwitchedOffMax[t].IsClose(vecMax, fEpsilon)) )
			{
				for (s32 l = t; l < m_numCarGenSwitches-1; l++)
				{
					m_CarGenSwitchedOffMin[l] = m_CarGenSwitchedOffMin[l+1];
					m_CarGenSwitchedOffMax[l] = m_CarGenSwitchedOffMax[l+1];
				}
				m_numCarGenSwitches--;
			}
			else
			{
				t++;
			}
		}
	}

	for (s32 i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
		if (pCargen) // && !CarGeneratorArray[i].m_bHighPriority)
		{
			Vector3 vecCarGeneratorPosition(pCargen->m_centreX, pCargen->m_centreY, pCargen->m_centreZ);

			if ( (vecMax.IsGreaterThan(vecCarGeneratorPosition))
				&& (vecCarGeneratorPosition.IsGreaterThan(vecMin)) )
			{
				if (bActive)
				{
					if (pCargen->IsSwitchedOff() && !pCargen->m_bHighPriority)
						pCargen->m_bReadyToGenerateCar = false;		// Don't want cars to be created the second switched back on.

					pCargen->SwitchOn();
				}
				else
				{
					pCargen->SwitchOff();

					for (s32 f = 0; f < m_scheduledVehicles.GetCount(); ++f)
					{
						if (m_scheduledVehicles[f].valid && m_scheduledVehicles[f].carGen == pCargen)
						{
							DropScheduledVehicle(m_scheduledVehicles[f]);
						}
					}
				}
			}
		}
	}
}


void CTheCarGenerators::SetAllCarGeneratorsBackToActive()
{
		// Go through the car generator switches and switch on all the generators that were affected.
	for (s32 i = m_numCarGenSwitches-1; i >= 0; i--)
	{
		SetCarGeneratorsActiveInArea(m_CarGenSwitchedOffMin[i], m_CarGenSwitchedOffMax[i], true);
	}
	m_numCarGenSwitches = 0;
}

void CCarGenerator::SwitchOffIfWithinSwitchArea()
{
//	if (!m_bHighPriority)
	{
		for (s32 i = 0; i < CTheCarGenerators::m_numCarGenSwitches; i++)
		{
			if (m_centreX > CTheCarGenerators::m_CarGenSwitchedOffMin[i].x && m_centreX < CTheCarGenerators::m_CarGenSwitchedOffMax[i].x &&
				m_centreY > CTheCarGenerators::m_CarGenSwitchedOffMin[i].y && m_centreY < CTheCarGenerators::m_CarGenSwitchedOffMax[i].y &&
				m_centreZ > CTheCarGenerators::m_CarGenSwitchedOffMin[i].z && m_centreZ < CTheCarGenerators::m_CarGenSwitchedOffMax[i].z)
			{
				SwitchOff();
			}
		}
	}
}

//
// name:		CreateCarsOnGeneratorsInArea
// description:	For all car generators in this area a vehicle is created (if the space is clear).
//				This function will ignore the limit of parked cars. Use with care.

void CTheCarGenerators::CreateCarsOnGeneratorsInArea(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ, float Percentage, s32 maxNumCarsInArea)
{
	CVehicle::Pool *VehiclePool = CVehicle::GetPool();
	CVehicle* pVeh;
	s32 i = (s32) VehiclePool->GetSize();

	while(i--)
	{
		pVeh = VehiclePool->GetSlot(i);
		if(pVeh)
		{
			Vector3 pos = VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition());
			if (pos.x > MinX && pos.x < MaxX && pos.y > MinY && pos.y < MaxY && pos.z > MinZ && pos.z < MaxZ)
			{
				maxNumCarsInArea--;
			}
		}
	}

	s32 maxWeCanCreate = (s32) VehiclePool->GetNoOfFreeSpaces() - 15;	// Make sure we leave 15 spaces in the vehicle pool
	maxNumCarsInArea = MIN(maxNumCarsInArea, maxWeCanCreate);

	for (s32 i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
		if (pCargen)
		{
			if (pCargen->m_centreX > MinX && pCargen->m_centreX < MaxX &&
				pCargen->m_centreY > MinY && pCargen->m_centreY < MaxY &&
				pCargen->m_centreZ > MinZ && pCargen->m_centreZ < MaxZ)
			{
				if (fwRandom::GetRandomNumberInRange(0.0f, 1.0f) < Percentage)
				{
					pCargen->m_bReadyToGenerateCar = true;
					bool bCarModelPicked = false;
					if (pCargen->ScheduleVehsForGenerationIfInRange(true, false, bCarModelPicked))
					{
						maxNumCarsInArea--;
						if (maxNumCarsInArea <= 0)
						{
							return;
						}
					}
				}
			}
		}
	}
}

//
// name:		RemoveCarsFromGeneratorsInArea
// description:	For all car generators in this area parked vehicles that are on it are removed.

void CTheCarGenerators::RemoveCarsFromGeneratorsInArea(float MinX, float MinY, float MinZ, float MaxX, float MaxY, float MaxZ)
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();
	
	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle && pVehicle->PopTypeGet() == POPTYPE_RANDOM_PARKED)
		{
			// Make sure there is nobody in it.
			if (pVehicle->GetSeatManager()->CountPedsInSeats()==0 && pVehicle->CanBeDeleted())
			{
				const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
#define MARGIN (0.5f)
				if ( (vVehiclePosition.x > MinX-MARGIN) && (vVehiclePosition.x < MaxX+MARGIN) &&
					 (vVehiclePosition.y > MinY-MARGIN) && (vVehiclePosition.y < MaxY+MARGIN) &&
					 (vVehiclePosition.z > MinZ-MARGIN) && (vVehiclePosition.z < MaxZ+MARGIN) )
				{
					if(NetworkUtils::IsNetworkCloneOrMigrating(pVehicle))
					{
						CNetObjVehicle* netVehicle = static_cast<CNetObjVehicle*>(pVehicle->GetNetworkObject());
						netVehicle->FlagForDeletion();
					}
					else
					{
						CVehicleFactory::GetFactory()->Destroy(pVehicle, true);
					}
				}
			}
		}
	}
}


//
// name:		HijackCarGenerator
// description:	Scheduled peds and perhaps other stuff can grab a cargenerator and force a specific car to
//				be generated on it.
// returns:		If -1 is returned the Hijack was unsuccessful (no empty generators nearby) Otherwise the
//				index of the car generator is returned.
/*
s32 CTheCarGenerators::HijackCarGenerator(Vector3 &Coors, s32 ModelIndex, u8 Remap1, u8 Remap2, u8 Remap3, u8 Remap4, float bestDist)
{
	// Find best candidate for hijacking
	s32	BestCandidate = -1;

	for (s32 c = 0; c < MAX_CAR_GENERATORS; c++)
	{
		if (CarGeneratorArray[c].m_bUsed == true &&
			CarGeneratorArray[c].bHasBeenHijacked == false &&
			CarGeneratorArray[c].m_bHighPriority == false)
		{
			float	Dist = (Coors - Vector3(CarGeneratorArray[c].m_centreX, CarGeneratorArray[c].m_centreY, CarGeneratorArray[c].m_centreZ)).Mag();

			if (Dist < bestDist)
			{
				// Make sure there aren't any cars sitting on top of this generator.
				// If this is the case this generator would never work.
				s32	Num;
				CGameWorld::FindObjectsKindaColliding(Vector3(CarGeneratorArray[c].m_centreX, CarGeneratorArray[c].m_centreY, CarGeneratorArray[c].m_centreZ), 5.0f, false, true, &Num, 2, NULL, 0, 1, 0, 0, 0);	// Just Cars we're interested in
				if (Num == 0)
				{
					BestCandidate = c;
					bestDist = Dist;
				}
			}
		}
	}

	if (BestCandidate >= 0)
	{
		HijackCarGenerator(BestCandidate, ModelIndex, Remap1, Remap2, Remap3, Remap4);
	}

	return BestCandidate;
}

//
// name:		HijackCarGenerator
// description:	Scheduled peds and perhaps other stuff can grab a cargenerator and force a specific car to
//				be generated on it.
// returns:		If -1 is returned the Hijack was unsuccessful (no empty generators nearby) Otherwise the
//				index of the car generator is returned.

void CTheCarGenerators::HijackCarGenerator(s32 index, s32 ModelIndex, u8 Remap1, u8 Remap2, u8 Remap3, u8 Remap4, u8 Remap5, u8 Remap6)
{
	CarGeneratorArray[index].m_bReadyToGenerateCar = true;
	CarGeneratorArray[index].m_bHighPriority = true;
	CarGeneratorArray[index].m_bCarCreatedBefore = false;
	CarGeneratorArray[index].bHasBeenHijacked = true;

	Assert(ModelIndex >= 0 && ModelIndex < 32767);
	Assertf( ((CBaseModelInfo*)CModelInfo::GetModelInfo (ModelIndex))->GetModelType() == MI_TYPE_VEHICLE, "CTheCarGenerators::HijackCarGenerator - %s is not a car model %f %f %f", CModelInfo::GetBaseModelInfoName(ModelIndex), 
		CarGeneratorArray[index].m_centreX, CarGeneratorArray[index].m_centreY, CarGeneratorArray[index].m_centreZ);

	CarGeneratorArray[index].VisibleModel = (s16)ModelIndex;
	CarGeneratorArray[index].car_remap1 = Remap1;
	CarGeneratorArray[index].car_remap2 = Remap2;
	CarGeneratorArray[index].car_remap3 = Remap3;
	CarGeneratorArray[index].car_remap4 = Remap4;
	CarGeneratorArray[index].car_remap5 = Remap5;
	CarGeneratorArray[index].car_remap6 = Remap6;
}

//
// name:		ReleaseHijackedCarGenerator
// description:	Scheduled peds and perhaps other stuff can grab a cargenerator and force a specific car to
//				be generated on it.
//				This function releases a car generator if it had been hijacked.

void CTheCarGenerators::ReleaseHijackedCarGenerator(s32 CarGenerator)
{
	// Only release it if it looks like it is still the same car generator
	// (There is the possibility this slot has been streamed out and a new one
	// was streamed in in its place)
	if (CarGeneratorArray[CarGenerator].m_bUsed &&
		CarGeneratorArray[CarGenerator].bHasBeenHijacked)
	{
		CarGeneratorArray[CarGenerator].m_bHighPriority = false;
		CarGeneratorArray[CarGenerator].bHasBeenHijacked = false;
		CarGeneratorArray[CarGenerator].VisibleModel = -1;
		CarGeneratorArray[CarGenerator].car_remap1 = 0xff;
		CarGeneratorArray[CarGenerator].car_remap2 = 0xff;
		CarGeneratorArray[CarGenerator].car_remap3 = 0xff;
		CarGeneratorArray[CarGenerator].car_remap4 = 0xff;
		CarGeneratorArray[CarGenerator].car_remap5 = 0xff;
		CarGeneratorArray[CarGenerator].car_remap6 = 0xff;
	}
}
*/

//
// name:		InteriorHasBeenPopulated
// description:	This is called whenever a interior is deployed. The car generators that 

void CTheCarGenerators::InteriorHasBeenPopulated(const spdAABB &bbox)
{
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::IsExportingCollision())
		return;
#endif

	CTheCarGenerators::CreateCarsOnGeneratorsInArea( bbox.GetMinVector3().x, bbox.GetMinVector3().y, bbox.GetMinVector3().z, bbox.GetMaxVector3().x, bbox.GetMaxVector3().y, bbox.GetMaxVector3().z, 1.0f, 10 );
}


//
// name:		ItIsDay
// description:	Some car generators work at night and some work during the day. This function decides
//				which ones should be active.

bool CTheCarGenerators::ItIsDay()
{
	return (CClock::GetHour() < 22 && CClock::GetHour() > 6);
}

////////////////////////////////////////////////////////
// CTheCarGenerators::InitLevelWithMapLoaded
//
////////////////////////////////////////////////////////

void CTheCarGenerators::InitLevelWithMapLoaded(void)
{
	if(CarGeneratorPool.GetSize() == 0) // if the Car generator pool hasn't been set up
		Init();
	bScriptDisabledCarGenerators = false;
	bScriptDisabledCarGeneratorsWithHeli = false;

	NoGenerate = false;

	Assertf(CScenarioManager::IsSafeToDeleteCarGens(), "Shutting down car generators without first shutting down the scenario manager properly.");
	Assertf(m_NumScenarioCarGenerators == 0, "Shutting down car generators with %d scenario car generators still in use.", m_NumScenarioCarGenerators);
	Assertf(!GetNumQueuedVehs(), "Shutting down car generators with %d vehicles still queued up.", GetNumQueuedVehs());

	// set all car generators as not used
	for(int i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
		if(pCargen)
			DestroyCarGeneratorByIndex(i, false);
	}

	m_numCarGenSwitches = 0;		// No switches set up by script yet.

	// Note: it's tempting to set m_bScheduled to false here (and in InitLevelWithMapUnloaded())
	// for all car generators, but if we do that, we would also need to make sure that
	// m_scheduledVehicles has been safely emptied out, with asynchronous queries cancelled, etc.
	// When the game starts, m_bScheduled should already be false from construction,
	// and when restarting, the queue should get cleaned up properly from m_bUsed having been cleared out.
}

////////////////////////////////////////////////////////
// CTheCarGenerators::InitLevelWithMapUnloaded
//
////////////////////////////////////////////////////////

void CTheCarGenerators::InitLevelWithMapUnloaded(void)	//	GSW_ - should NumOfCarGenerators only be cleared here? Maybe InitLevelWithMapLoaded isn't needed at all
{
	if(CarGeneratorPool.GetSize() == 0) // if the Car generator pool hasn't been set up
		Init();
#if !__FINAL
	gs_bDisplayCarGenerators = false;
#endif
	
	NoGenerate = false;


	m_SpecialPlateHandler.Init();
	
	Assertf(CScenarioManager::IsSafeToDeleteCarGens(), "Shutting down car generators without first shutting down the scenario manager properly.");
	Assertf(m_NumScenarioCarGenerators == 0, "Shutting down car generators with %d scenario car generators still in use.", m_NumScenarioCarGenerators);
	Assertf(!GetNumQueuedVehs(), "Shutting down car generators with %d vehicles still queued up.", GetNumQueuedVehs());

	// set all car generators as not used
	for(int i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
		if(pCargen)
			DestroyCarGeneratorByIndex(i, false);
	}
}

#if !__FINAL

void CTheCarGenerators::DebugRender()
{
	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_S, KEYBOARD_MODE_DEBUG_SHIFT_ALT))
	{
		gs_bDisplayCarGenerators = !gs_bDisplayCarGenerators;
	}

	const Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;

	if(gs_bDisplayCarGenerators || gs_bDisplayCarGeneratorsOnVMap || gs_bDisplayCarGeneratorsScenariosOnly || gs_bDisplayCarGeneratorsPreassignedOnly)
	{
        float minGenerationRange = FLT_MAX;
        float maxGenerationRange = 0.0f;

		CCarGenerator* pCarGenerator;
		for (u32 i = 0; i < CarGeneratorPool.GetSize(); i++)
		{
			pCarGenerator = CarGeneratorPool.GetSlot(i);
			if(pCarGenerator)
			{
				if( (vOrigin - VEC3V_TO_VECTOR3(pCarGenerator->GetCentre())).Mag2() > gs_fDisplayCarGeneratorsDebugRange*gs_fDisplayCarGeneratorsDebugRange)
					continue;

				if(gs_bDisplayCarGeneratorsScenariosOnly && !pCarGenerator->IsScenarioCarGen())
				{
					continue;
				}

				if (gs_bDisplayCarGeneratorsPreassignedOnly && !pCarGenerator->HasPreassignedModel())
				{
					continue;
				}

				if(gs_bDisplayCarGenerators || gs_bDisplayCarGeneratorsScenariosOnly || gs_bDisplayCarGeneratorsPreassignedOnly)
				{
					pCarGenerator->DebugRender(false);
				}
				if(gs_bDisplayCarGeneratorsOnVMap)
				{
					pCarGenerator->DebugRender(true);
				}

                if(pCarGenerator->GetGenerationRange() < minGenerationRange)
                {
                    minGenerationRange = pCarGenerator->GetGenerationRange();
                }

                if(pCarGenerator->GetGenerationRange() > maxGenerationRange)
                {
                    maxGenerationRange = pCarGenerator->GetGenerationRange();
                }
			}
		}

        // render the areas that will be prevented from spawning car generators based on remote network player positions
        if(NetworkInterface::IsGameInProgress() && gs_bDisplayCarGeneratorsOnVMap)
        {
            unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
            const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

            for(unsigned index = 0; index < numPhysicalPlayers; index++)
            {
                const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

                if(pPlayer->GetPlayerPed())
                {
                    // draw a transparent filled circle in the regions that are too close to generate
                    Vector3 playerPosition = VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition());
                    float radius = CVehiclePopulation::GetCreationDistanceOffScreen();
                    Color32 playerColour = NetworkColours::GetNetworkColour(NetworkColours::GetDefaultPlayerColour(pPlayer->GetPhysicalPlayerIndex()));
                    playerColour.SetAlpha(128);

                    CVectorMap::DrawCircle(playerPosition, radius, playerColour, true);

                    // draw a solid outlined circle around the regions that are close enough for a car generator to generate
                    radius = minGenerationRange;
                    playerColour.SetAlpha(255);
                    CVectorMap::DrawCircle(playerPosition, radius, playerColour, false);

                    if(maxGenerationRange != minGenerationRange)
                    {
                        radius = maxGenerationRange;
                        playerColour.SetAlpha(255);
                        CVectorMap::DrawCircle(playerPosition, radius, playerColour, false);
                    }
                }
            }
        }
	}

#if __BANK
	// deal with the prio area editor
    if (g_bEnableEditor)
    {
        Vector3 vPos;
        bool bHasPosition = CDebugScene::GetWorldPositionUnderMouse(vPos, ArchetypeFlags::GTA_ALL_TYPES_MOVER);
        static bool sbLeftButtonHeld = false;
        static bool sbRightButtonHeld = false;

        if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT)
            sbLeftButtonHeld = true;
        if (sbLeftButtonHeld && ioMouse::GetReleasedButtons() & ioMouse::MOUSE_LEFT)
            sbLeftButtonHeld = false;
        if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_RIGHT)
            sbRightButtonHeld = true;
        if (sbRightButtonHeld && ioMouse::GetReleasedButtons() & ioMouse::MOUSE_RIGHT)
            sbRightButtonHeld = false;

        if (bHasPosition)
        {
            if (sbLeftButtonHeld)
            {
                g_vClickedPos[0] = vPos;
                g_bHasPosition[0] = true;
                sprintf(g_Pos1, "%.5f, %.5f, %.5f", g_vClickedPos[0].x, g_vClickedPos[0].y, g_vClickedPos[0].z);
            }
            else if( sbRightButtonHeld )
            {
                g_vClickedPos[1] = vPos;
                g_bHasPosition[1] = true;
                sprintf(g_Pos2, "%.5f, %.5f, %.5f", g_vClickedPos[1].x, g_vClickedPos[1].y, g_vClickedPos[1].z);
            }
        }

        if (g_bHasPosition[0])
            grcDebugDraw::Sphere(g_vClickedPos[0], 0.1f, Color32(255, 0, 0), false, 1, 1);
        if (g_bHasPosition[1])
            grcDebugDraw::Sphere(g_vClickedPos[1], 0.1f, Color32(0, 0, 255), false, 1, 1);
        if (g_bHasPosition[0] && g_bHasPosition[1])
            grcDebugDraw::BoxAxisAligned(RCC_VEC3V(g_vClickedPos[0]), RCC_VEC3V(g_vClickedPos[1]), Color32(0, 255, 0), false, 1);
        if (g_bShowAreas)
        {
            CargenPriorityAreas* prioAreas = CTheCarGenerators::GetPrioAreas();
            if (prioAreas)
                for (s32 i = 0; i < prioAreas->areas.GetCount(); ++i)
                    grcDebugDraw::BoxAxisAligned(RCC_VEC3V(prioAreas->areas[i].minPos), RCC_VEC3V(prioAreas->areas[i].maxPos), Color32(255, 255, 0), false, 1);
        }
    }
#endif // __BANK
}

void CTheCarGenerators::DumpCargenInfo()
{
#if !__NO_OUTPUT
	const CPed* pLocalPlayer = CGameWorld::FindLocalPlayer();
	Vec3V centerPosV(V_ZERO);
	if(pLocalPlayer)
	{
		centerPosV = pLocalPlayer->GetTransform().GetPosition();
	}

	Displayf("slot origin                                   model           properties               x        y        z  dist");

	int numUsedScript = 0;
	int numUsedScenarios = 0;
	int numUsedMap = 0;
	for(int i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		const CCarGenerator* pCarGenerator = CarGeneratorPool.GetSlot(i);
		if(pCarGenerator)
		{
			Assert(pCarGenerator->IsUsed());

			const Vec3V posV = pCarGenerator->GetCentre();
			float dist = Dist(posV, centerPosV).Getf();

			char origin[256];
			char origin2[256];
			char timeRange[256];
			origin2[0] = '\0';
			timeRange[0] = '\0';
			if(pCarGenerator->IsOwnedByScript())
			{
				formatf(origin, "script");
				numUsedScript++;
			}
			else if(pCarGenerator->IsScenarioCarGen())
			{
				const int scenarioType = pCarGenerator->GetScenarioType();
				formatf(origin, "%s", SCENARIOINFOMGR.GetNameForScenario(scenarioType));

				bool foundPt = false;
				const int numReg = SCENARIOPOINTMGR.GetNumRegions();
				for(int j = 0; j < numReg; j++)
				{
					CScenarioPointRegion* reg = SCENARIOPOINTMGR.GetRegion(j);
					if(!reg)
					{
						continue;
					}
					const int numPts = reg->GetNumPoints();
					for(int k = 0; k < numPts; k++)
					{
						const CScenarioPoint& pt = reg->GetPoint(k);
						if(pt.GetCarGen() == pCarGenerator)
						{
#if SCENARIO_DEBUG
							const char* regionNamePtr = SCENARIOPOINTMGR.GetRegionName(j);
							const char* strippedRegionNamePtr = strrchr(regionNamePtr, '/');
							if(strippedRegionNamePtr)
							{
								strippedRegionNamePtr++;
							}
							else
							{
								strippedRegionNamePtr = regionNamePtr;
							}
							formatf(origin2, " %s", strippedRegionNamePtr);
#endif	// SCENARIO_DEBUG
							foundPt = true;

							if(pt.HasTimeOverride())
							{
								formatf(timeRange, " %d-%d", pt.GetTimeStartOverride(), pt.GetTimeEndOverride());
							}

							break;
						}
					}
					if(foundPt)
					{
						break;
					}
				}

				numUsedScenarios++;
			}
			else
			{
				formatf(origin, "map (%s)", INSTANCE_STORE.GetName(strLocalIndex(pCarGenerator->GetIpl())));
				numUsedMap++;
			}

			char model[256];
			u32 visibleModel = pCarGenerator->GetVisibleModel();
			if(CModelInfo::IsValidModelInfo(visibleModel))
			{
				formatf(model, "%s", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(visibleModel))));
			}

			char properties[256];
			formatf(properties, "%s%s", pCarGenerator->IsHighPri() ? "HighPri " : "", pCarGenerator->IsExtendedRange() ? "ExtRange " : "");

			Displayf("%3d: %-40s %-15s %-18s %7.1f %7.1f %7.1f %5.1f%s%s", i, origin, model, properties, posV.GetXf(), posV.GetYf(), posV.GetZf(), dist, origin2, timeRange);
		}
		else
		{
			//Displayf("%3d: empty", i);
		}
	}

	const int numUsedTotal = numUsedScenarios + numUsedScript + numUsedMap;
	Displayf("%d car generators in use: %d for script, %d for scenarios, and %d from map data.",
			numUsedTotal, numUsedScript, numUsedScenarios, numUsedMap);
	Displayf("Player pos: %.1f %.1f %.1f", centerPosV.GetXf(), centerPosV.GetYf(), centerPosV.GetZf());
	Displayf("Time: %02d:%02d", CClock::GetHour(), CClock::GetMinute());
	Assert(CarGeneratorPool.GetNoOfFreeSpaces() == CarGeneratorPool.GetSize() - numUsedTotal);
	Assert(m_NumScenarioCarGenerators == numUsedScenarios);
#endif	// !__NO_OUTPUT
}

#endif // !__FINAL

#if __BANK
void UpdatePrioAreaPositionOne()
{
	Vector3 vPosFromText(Vector3::ZeroType);
	if (sscanf(g_Pos1, "%f, %f, %f", &vPosFromText.x, &vPosFromText.y, &vPosFromText.z) == 3)
	{
		g_vClickedPos[0] = vPosFromText;
		g_bHasPosition[0] = true;
	}
}

void UpdatePrioAreaPositionTwo()
{
	Vector3 vPosFromText(Vector3::ZeroType);
	if (sscanf(g_Pos2, "%f, %f, %f", &vPosFromText.x, &vPosFromText.y, &vPosFromText.z) == 3) {
		g_vClickedPos[1] = vPosFromText;
		g_bHasPosition[1] = true;
	}
}

void AddPriorityArea()
{
	CargenPriorityAreas* prioAreas = CTheCarGenerators::GetPrioAreas();
	if (g_bHasPosition[0] && g_bHasPosition[1] && prioAreas)
	{
		CargenPriorityAreas::Area newArea;
		newArea.minPos.x = rage::Min(g_vClickedPos[0].x, g_vClickedPos[1].x);
		newArea.minPos.y = rage::Min(g_vClickedPos[0].y, g_vClickedPos[1].y);
		newArea.minPos.z = rage::Min(g_vClickedPos[0].z, g_vClickedPos[1].z);
		newArea.maxPos.x = rage::Max(g_vClickedPos[0].x, g_vClickedPos[1].x);
		newArea.maxPos.y = rage::Max(g_vClickedPos[0].y, g_vClickedPos[1].y);
		newArea.maxPos.z = rage::Max(g_vClickedPos[0].z, g_vClickedPos[1].z);
		prioAreas->areas.PushAndGrow(newArea, 1);
	}

	g_bHasPosition[0] = false;
	g_bHasPosition[1] = false;
}

void ClearPriorityArea()
{
	g_bHasPosition[0] = false;
	g_bHasPosition[1] = false;
}

void SavePriorityAreas()
{
	CargenPriorityAreas* prioAreas = CTheCarGenerators::GetPrioAreas();
	if (prioAreas)
	{
		for (s32 i = 0; i < prioAreas->areas.GetCount(); ++i)
		{
			Vector3 pos0 = prioAreas->areas[i].minPos;
			Vector3 pos1 = prioAreas->areas[i].maxPos;
			prioAreas->areas[i].minPos.x = rage::Min(pos0.x, pos1.x);
			prioAreas->areas[i].minPos.y = rage::Min(pos0.y, pos1.y);
			prioAreas->areas[i].minPos.z = rage::Min(pos0.z, pos1.z);
			prioAreas->areas[i].maxPos.x = rage::Max(pos0.x, pos1.x);
			prioAreas->areas[i].maxPos.y = rage::Max(pos0.y, pos1.y);
			prioAreas->areas[i].maxPos.z = rage::Max(pos0.z, pos1.z);
		}
	}

	AssertVerify(PARSER.SaveObject("common:/data/cargens", "meta", CTheCarGenerators::GetPrioAreas(), parManager::XML));
}

void DeleteAllPriorityAreas()
{
	CargenPriorityAreas* prioAreas = CTheCarGenerators::GetPrioAreas();
    if (prioAreas)
        prioAreas->areas.Reset();
}

void CTheCarGenerators::InitWidgets(bkBank* bank)
{
	bank->PushGroup("Priority Area Editor", false);
        bank->AddToggle("Enable editor", &g_bEnableEditor);
        bank->AddText("Min pos", g_Pos1, sizeof(g_Pos1), false, datCallback(CFA1(UpdatePrioAreaPositionOne)));
        bank->AddText("Max pos", g_Pos2, sizeof(g_Pos2), false, datCallback(CFA1(UpdatePrioAreaPositionTwo)));
        bank->AddButton("Clear area", datCallback(CFA(ClearPriorityArea)));
        bank->AddButton("Add area", datCallback(CFA(AddPriorityArea)));
        bank->AddButton("Delete all areas", datCallback(CFA(DeleteAllPriorityAreas)));
        bank->AddButton("Save areas", datCallback(CFA(SavePriorityAreas)));
        bank->AddToggle("Show areas", &g_bShowAreas);
	bank->PopGroup();
}

//
// name:		ForceSpawnCarsWithinRadius
// description:	For all car generators in this radius we force the spawn of the next scheduled car
// notes:		This function will ignore the limit of parked cars. Use with care.
//				Untick gs_bCheckPhysicProbeWhenForceSpawn in Rag if you want to force spawn vehicles on top of an existing vehicle. Use with extra care

void CTheCarGenerators::ForceSpawnCarsWithinRadius()
{
	const float sqRadius = square(gs_fForceSpawnCarRadius);
	for (s32 i = 0; i < CarGeneratorPool.GetSize(); i++)
	{
		CCarGenerator* pCargen = CarGeneratorPool.GetSlot(i);
		if (!pCargen)
			continue;

		const Vector3 vOrigin = camInterface::GetDebugDirector().IsFreeCamActive() ? camInterface::GetDebugDirector().GetFreeCamFrame().GetPosition() : CPlayerInfo::ms_cachedMainPlayerPos;
		const Vector3 vCargen(pCargen->m_centreX, pCargen->m_centreY, pCargen->m_centreZ);
		// skip cargen if we're not in user defined radius
		if( (vOrigin - vCargen).Mag2() > sqRadius )
			continue;

		fwModelId car;
		fwModelId trailer;
		bool carModelShouldBeRequested;
		Matrix34 creationMatrix;
		pCargen->PickValidModels(car, trailer, carModelShouldBeRequested);

		if (!car.IsValid())
			continue;

		pCargen->BuildCreationMatrix(&creationMatrix, car.GetModelIndex());

		if (gs_bCheckPhysicProbeWhenForceSpawn && !pCargen->DoSyncProbeForBlockingObjects(car, creationMatrix, NULL))
			continue;

		ScheduledVehicle newVeh;
		newVeh.valid = true;
		newVeh.isAsync = false;
		newVeh.carGen = pCargen;
		newVeh.carModelId = car;
		newVeh.trailerModelId = trailer;
		newVeh.creationMatrix = creationMatrix;
		newVeh.allowedToSwitchOffCarGenIfSomethingOnTop = false;
		newVeh.asyncHandle = INVALID_ASYNC_ENTRY;
		newVeh.pDestInteriorProxy = NULL;
		newVeh.destRoomIdx = -1;
		newVeh.preventEntryIfNotQualified = false;
		newVeh.forceSpawn = true;
		newVeh.needsStaticBoundsStreamingTest = false;
		newVeh.addToGameWorld = true;

		// We're forcing a car to spawn so it's not technically a scheduled one but if we don't do this we mess with the other scheduled cars.
		++m_NumScheduledVehicles;

		CBaseModelInfo* carMi = CModelInfo::GetBaseModelInfo(newVeh.carModelId);
		carMi->AddRef();

		if (newVeh.trailerModelId.IsValid())
		{
			CBaseModelInfo* trailerMi = CModelInfo::GetBaseModelInfo(newVeh.trailerModelId);
			trailerMi->AddRef();
		}

		CNetworkObjectPopulationMgr::SetProcessLocalPopulationLimits(false);
		CVehicle *pNewVehicle = CVehicleFactory::GetFactory()->Create(newVeh.carModelId, ENTITY_OWNEDBY_POPULATION, POPTYPE_RANDOM_PARKED, &newVeh.creationMatrix, true, true);			

		pNewVehicle->SetAllowFreezeWaitingOnCollision(true);
		pNewVehicle->m_nVehicleFlags.bShouldFixIfNoCollision = true;

		pNewVehicle->ActivatePhysics();

		newVeh.vehicle = pNewVehicle;

		PostCreationSetup(newVeh, false);
	}
}
#endif // __BANK

// PURPOSE:	Clears the scheduled queue of vehicles.
void CTheCarGenerators::ClearScheduledQueue()
{
	for (s32 i = 0; i < m_scheduledVehicles.GetCount(); ++i)
	{
		if (m_scheduledVehicles[i].valid)
		{
			DropScheduledVehicle(m_scheduledVehicles[i]);
			if(m_scheduledVehicles[i].vehicle)
			{
				CVehicleFactory::GetFactory()->Destroy(m_scheduledVehicles[i].vehicle, true);
			}
		}
	}
}

// PURPOSE:	Remove the top-most entry from m_scheduledVehicles, and mark the associated car generator
//			as unscheduled.
void CTheCarGenerators::DropScheduledVehicle(ScheduledVehicle& veh, bool releaseChainUser /*= true*/)
{
	if(veh.valid)
	{
		Assert(CTheCarGenerators::m_NumScheduledVehicles > 0);
		--CTheCarGenerators::m_NumScheduledVehicles;
	}

	veh.valid = false;

	if (veh.carModelId.IsValid())
	{
		CBaseModelInfo* carMi = CModelInfo::GetBaseModelInfo(veh.carModelId);
		carMi->RemoveRef();
	}
	if (veh.trailerModelId.IsValid())
	{
		CBaseModelInfo* trailerMi = CModelInfo::GetBaseModelInfo(veh.trailerModelId);
		trailerMi->RemoveRef();
	}

	Assert(veh.carGen->m_bScheduled);
	veh.carGen->m_bScheduled = false;
	// If they cargen was set to be destroyed, destroy it now
	if(veh.carGen->m_bDestroyAfterSchedule)
	{
		DestroyCarGeneratorByIndex(CarGeneratorPool.GetIndex(veh.carGen), veh.carGen->IsScenarioCarGen());
		veh.carGen = NULL;
	}

	if (releaseChainUser && veh.chainUser)
	{
		CScenarioPointChainUseInfo* info = veh.chainUser;
		veh.chainUser = NULL; //dont need this any longer but dont delete the user info ... the chaining system will handle that ... 
		CScenarioChainingGraph::UnregisterPointChainUser(*info);
	}
}

void CTheCarGenerators::VehicleHasBeenStreamedOut(u32 modelIndex)
{
	atArray<u32> newModelArray;
	newModelArray.Reserve(ms_preassignedModels.GetCount());
	for (s32 i = 0; i < ms_preassignedModels.GetCount(); ++i)
	{
		if (ms_preassignedModels[i] != modelIndex)
			newModelArray.Push(ms_preassignedModels[i]);
	}
	ms_preassignedModels = newModelArray;

	int indexInScenarioArray = ms_scenarioModels.Find(modelIndex);
	if(indexInScenarioArray >= 0)
	{
		ms_scenarioModels.DeleteFast(indexInScenarioArray);
	}
}


bool CTheCarGenerators::RequestVehicleModelForScenario(fwModelId modelId, bool highPri)
{
	const u32 modelIndex = modelId.GetModelIndex();
	bool alreadyCounted = (GetScenarioModels().Find(modelIndex) >= 0)
			|| (GetPreassignedModels().Find(modelIndex) >= 0);

	bool allowRequest = true;
	if(!alreadyCounted)
	{
		if(highPri || GetScenarioModels().GetCount() < CPopCycle::GetMaxScenarioVehicleModels())
		{
			GetScenarioModels().PushAndGrow(modelIndex);
		}
		else
		{
			allowRequest = false;
		}
	}

	if(allowRequest)
	{
		STRVIS_AUTO_CONTEXT(strStreamingVisualize::CARGEN);
		CModelInfo::RequestAssets(modelId, STRFLAG_FORCE_LOAD|STRFLAG_DONTDELETE);
	}

	return allowRequest;
}


// aggressively spawn as many cars as we can to fill the budget. This should only be called
// when the screen is faded out and a major spike is acceptable
void CTheCarGenerators::InstantFill()
{
	ClearScheduledQueue();

	// abort if we've filled our budget
	s32	maxCars = CPopCycle::GetCurrentMaxNumParkedCars();
	if (CVehiclePopulation::ms_overrideNumberOfParkedCars >= 0)
		maxCars = CVehiclePopulation::ms_overrideNumberOfParkedCars;

	for (size_t i = 0; i < CarGeneratorPool.GetSize(); ++i)
	{
		if (CVehiclePopulation::ms_numParkedCars >= maxCars)
			break;

		CCarGenerator* pCargen = CarGeneratorPool.GetSlot((s32)i);
		if(pCargen)
		{
			pCargen->InstantProcess();	
		}	
	}
}
