
#include "script/script_cars_and_peds.h"

// Framework headers
#include "fwsys/metadatastore.h"
#include "fwvehicleai/pathfindtypes.h"

// Game headers
#include "animation/MovePed.h"
#include "camera/CamInterface.h"
#include "event/Events.h"
#include "event/ShockingEvents.h"
#include "MissionCreatorData_parser.h"
#include "Peds/ped.h"
#include "Peds/PedCapsule.h"
#include "Peds/PedFactory.h"
#include "Peds/PedIntelligence.h"
#include "Peds/pedpopulation.h"
#include "Peds/PedTaskRecord.h"
#include "Peds/WildlifeManager.h"
#include "physics/physics.h"
#include "scene/world/GameWorld.h"
#include "scene/LoadScene.h"
#include "scene/playerswitch/PlayerSwitchInterface.h"
#include "script/script.h"
#include "script/script_brains.h"
#include "script/script_channel.h"
#include "streaming/populationstreaming.h"
#include "streaming/streaming.h"
#include "task/General/TaskBasic.h"
#include "task/Response/Info/AgitatedManager.h"
#include "task/Scenario/ScenarioManager.h"
#include "Vehicles/vehicle.h"
#include "Vehicles/VehicleFactory.h"
#include "vehicles/VehiclePopulation.h"
#include "vehicleAi/VehicleIntelligence.h"
#include "vfx/particles/PtFxManager.h"



#define AS_GOOD_AS_STOPPED_SPEED		(0.02f)

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

#define UPSIDEDOWN_CAR_TIMER_THRESHOLD	(7000)

CUpsideDownCarCheck CScriptCars::UpsideDownCars;

CStuckCarCheck CScriptCars::StuckCars;

CSuppressedModels CScriptPeds::SuppressedPedModels;
CRestrictedModels CScriptPeds::RestrictedPedModels;
CSuppressedModels CScriptCars::SuppressedCarModels;


PARAM(missioncreatordebug, "prints debug related to the missionc reator asset budget");


// Class to handle checking for mission cars being stuck on their roofs

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CUpsideDownCarCheck::Init
// PURPOSE : Clears the array used to check for upside-down cars
/////////////////////////////////////////////////////////////////////////////////
void CUpsideDownCarCheck::Init(void)
{
	for (u32 loop = 0; loop < MAX_UPSIDEDOWN_CAR_CHECKS; loop++)
	{
		Cars[loop].CarID = NULL_IN_SCRIPTING_LANGUAGE;
		Cars[loop].UpsideDownTimer = 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CUpsideDownCarCheck::IsCarUpsideDown
// PURPOSE : Returns TRUE if the specified car is upside-down and moving slowly
// INPUTS :	ID of car to check
// RETURNS : TRUE if the car is stuck on its roof, FALSE if the car is not on its
//				roof or is moving at a reasonable speed
/////////////////////////////////////////////////////////////////////////////////
bool CUpsideDownCarCheck::IsCarUpsideDown(s32 CarToCheckID)
{
	const CVehicle *pVehicleToCheck = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(CarToCheckID);
	if(scriptVerifyf(pVehicleToCheck, "CUpsideDownCarCheck::IsCarUpsideDown - Car doesn't exist"))
	{
		return IsCarUpsideDown(pVehicleToCheck);
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : As above, but takes pointer to vehicle
/////////////////////////////////////////////////////////////////////////////////

bool CUpsideDownCarCheck::IsCarUpsideDown(const CVehicle* pVehicleToCheck)
{
	if(scriptVerifyf(pVehicleToCheck, "CUpsideDownCarCheck::IsCarUpsideDown - car doesn't exist"))
	{
		const float fCz = pVehicleToCheck->GetTransform().GetC().GetZf();
        int iNumberOfWheels = pVehicleToCheck->GetNumWheels();
        if(iNumberOfWheels <= 0)
        {
            iNumberOfWheels = 1;//pretend we have at least one wheel, so this will vaguely work for boats
        }

		static dev_float sfMaxVelMagSq = 4.0f;
		if(pVehicleToCheck->GetVelocity().Mag2() < sfMaxVelMagSq && ((fCz < UPSIDEDOWN_CAR_UPZ_THRESHOLD && pVehicleToCheck->GetNumContactWheels() < iNumberOfWheels) || fCz < 0.0f))
		{
			if(pVehicleToCheck->CanPedStepOutCar(NULL, false))
				return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CUpsideDownCarCheck::IsCarInUpsideDownCarArray 
// PURPOSE : Checks whether the provided car ID is contained in the CUpsideDownCarCheck array
/////////////////////////////////////////////////////////////////////////////////
bool CUpsideDownCarCheck::IsCarInUpsideDownCarArray(s32 CarToCheckID)
{
	for (u32 loop = 0; loop < MAX_UPSIDEDOWN_CAR_CHECKS; loop++)
	{
		if (Cars[loop].CarID == CarToCheckID)
		{
			return true;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CUpsideDownCarCheck::UpdateTimers
// PURPOSE : Increases the timer of all cars in the array which are currently upside down
/////////////////////////////////////////////////////////////////////////////////
void CUpsideDownCarCheck::UpdateTimers(void)
{
	u32 TimeStep = fwTimer::GetTimeStepInMilliseconds();

	for (u32 loop = 0; loop < MAX_UPSIDEDOWN_CAR_CHECKS; loop++)
	{
		if (Cars[loop].CarID != NULL_IN_SCRIPTING_LANGUAGE)
		{
			const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(Cars[loop].CarID, 0);
			if (pVehicle)
			{
				if (IsCarUpsideDown(pVehicle))
				{
					Cars[loop].UpsideDownTimer += TimeStep;
				}
				else
				{
					Cars[loop].UpsideDownTimer = 0;
				}
			}
			else
			{	// Car is no longer on the map - remove it from the array of cars to check
				Cars[loop].CarID = NULL_IN_SCRIPTING_LANGUAGE;
				Cars[loop].UpsideDownTimer = 0;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CUpsideDownCarCheck::AddCarToCheck
// PURPOSE : Adds a car to the array of cars to be checked for being upside-down
// INPUTS : ID of car to add
/////////////////////////////////////////////////////////////////////////////////
void CUpsideDownCarCheck::AddCarToCheck(s32 NewCarID)
{
	u32 loop = 0;
// Check car is not already in the array
	while (loop < MAX_UPSIDEDOWN_CAR_CHECKS)
	{
		scriptAssertf((Cars[loop].CarID != NewCarID), "CUpsideDownCarCheck::AddCarToCheck - Trying to add car to upsidedown check again");
		loop++;
	}

	loop = 0;
// Find a space in the array for the car
	while ((loop < MAX_UPSIDEDOWN_CAR_CHECKS) && (Cars[loop].CarID != NULL_IN_SCRIPTING_LANGUAGE))
	{
		loop++;
	}

	scriptAssertf((loop < MAX_UPSIDEDOWN_CAR_CHECKS), "CUpsideDownCarCheck::AddCarToCheck - Too many upsidedown car checks");

// Add the car to the array
	if (loop < MAX_UPSIDEDOWN_CAR_CHECKS)
	{
		Cars[loop].CarID = NewCarID;
		Cars[loop].UpsideDownTimer = 0;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CUpsideDownCarCheck::RemoveCarFromCheck
// PURPOSE : Removes a car from the array of cars to be checked (if it is there)
// INPUTS : ID of car to remove
/////////////////////////////////////////////////////////////////////////////////
void CUpsideDownCarCheck::RemoveCarFromCheck(s32 RemoveCarID)
{
	for (u32 loop = 0; loop < MAX_UPSIDEDOWN_CAR_CHECKS; loop++)
	{
		if (Cars[loop].CarID == RemoveCarID)
		{
			Cars[loop].CarID = NULL_IN_SCRIPTING_LANGUAGE;
			Cars[loop].UpsideDownTimer = 0;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CUpsideDownCarCheck::HasCarBeenUpsideDownForAWhile
// PURPOSE : Returns TRUE if the specified car (which must be in the array)
//				has been upside-down for the required length of time
// INPUTS : ID of car to check (must have already been added to the array)
// RETURNS : TRUE if the car has been upside-down for a reasonable time
/////////////////////////////////////////////////////////////////////////////////
bool CUpsideDownCarCheck::HasCarBeenUpsideDownForAWhile(s32 CheckCarID)
{
	for (u32 loop = 0; loop < MAX_UPSIDEDOWN_CAR_CHECKS; loop++)
	{
		if (Cars[loop].CarID == CheckCarID)
		{
			if (Cars[loop].UpsideDownTimer > UPSIDEDOWN_CAR_TIMER_THRESHOLD)
			{
				return TRUE;
			}
			else
			{
				return FALSE;
			}
		}
	}

	scriptAssertf(0, "CUpsideDownCarCheck::HasCarBeenUpsideDownForAWhile - Vehicle not in upsidedown check array");
	return FALSE;
}
// End of code for checking for upsidedown cars

// Class to handle checking for mission cars being stuck
void CStuckCarCheck::ResetArrayElement(u32 Index)
{
	Cars[Index].m_CarID = NULL_IN_SCRIPTING_LANGUAGE;
	Cars[Index].LastPos = Vector3(-5000.0f, -5000.0f, -5000.0f);
	Cars[Index].LastChecked = -1;
	Cars[Index].StuckRadius = 0.0f;
	Cars[Index].CheckTime = 0;
	Cars[Index].bStuck = FALSE;

	Cars[Index].bWarpCar = false;
	Cars[Index].bWarpIfStuckFlag = false;
	Cars[Index].bWarpIfUpsideDownFlag = false;
	Cars[Index].bWarpIfInWaterFlag = false;
	Cars[Index].NumberOfNodesToCheck = 0;
}

void CStuckCarCheck::Init(void)
{
	for (u32 loop = 0; loop < MAX_STUCK_CAR_CHECKS; loop++)
	{
		ResetArrayElement(loop);
	}
}

bool CStuckCarCheck::AttemptToWarpVehicle(CVehicle *pVehicle, Vector3 &NewCoords, float NewHeading)
{
	float fRadiusForOffScreenChecks = 4.0f;	//	should this be available to the level designers too?
	if (camInterface::IsSphereVisibleInGameViewport(NewCoords, fRadiusForOffScreenChecks) == false)
	{
//		NewCoords.z += 1.0f;
		float MinX = NewCoords.x - fRadiusForOffScreenChecks;
		float MaxX = NewCoords.x + fRadiusForOffScreenChecks;

		float MinY = NewCoords.y - fRadiusForOffScreenChecks;
		float MaxY = NewCoords.y + fRadiusForOffScreenChecks;

		float MinZ = NewCoords.z - fRadiusForOffScreenChecks;
		float MaxZ = NewCoords.z + fRadiusForOffScreenChecks;

		Vec3V vBoxMin(MinX, MinY, MinZ);
		Vec3V vBoxMax(MaxX, MaxY, MaxZ);
		spdAABB testBox;
		testBox.Set(vBoxMin, vBoxMax);

		int NumberOfMissionEntitiesInArea = 0;

		fwIsBoxIntersectingApprox searchBox(testBox);
		CGameWorld::ForAllEntitiesIntersecting(&searchBox, 
			CTheScripts::CountMissionEntitiesInAreaCB, static_cast<void*>(&NumberOfMissionEntitiesInArea),
							ENTITY_TYPE_MASK_VEHICLE|ENTITY_TYPE_MASK_PED|ENTITY_TYPE_MASK_OBJECT, SEARCH_LOCATION_EXTERIORS|SEARCH_LOCATION_INTERIORS,
							SEARCH_LODTYPE_HIGHDETAIL, SEARCH_OPTION_NONE, WORLDREP_SEARCHMODULE_SCRIPT);

		if (NumberOfMissionEntitiesInArea == 0)
		{
			CVehiclePopulation::SetCoordsOfScriptCar(pVehicle, NewCoords.x, NewCoords.y, NewCoords.z, true, true);	//	clear orientation to ensure the vehicle isn't upside-down
			pVehicle->SetHeading(( DtoR * NewHeading));
			return true;
		}
	}

	return false;
}

void CStuckCarCheck::Process(void)
{
	Vector3 DiffVec;

	Vector3 NewCoords;
	float NewHeading;

	CNodeAddress ResultNode;

	u32 TimeStep = fwTimer::GetTimeInMilliseconds();

	for (u32 loop = 0; loop < MAX_STUCK_CAR_CHECKS; loop++)
	{
		if (Cars[loop].m_CarID != NULL_IN_SCRIPTING_LANGUAGE)
		{
			CVehicle *pVehicle = CTheScripts::GetEntityToModifyFromGUID<CVehicle>(Cars[loop].m_CarID, 0);
			if (pVehicle)
			{
				if (pVehicle->GetDriver())
				{
					if (TimeStep > u32(Cars[loop].LastChecked + Cars[loop].CheckTime))
					{
						const Vector3 NewPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());

						DiffVec = NewPos - Cars[loop].LastPos;
						float DistanceMoved = DiffVec.Mag();

						if (DistanceMoved < Cars[loop].StuckRadius)
						{
							Cars[loop].bStuck = TRUE;
						}
						else
						{
							Cars[loop].bStuck = FALSE;
						}

						Cars[loop].LastPos = NewPos;
						Cars[loop].LastChecked = TimeStep;
					}

					if (Cars[loop].bWarpCar)
					{
						bool bAttemptToWarpThisCarNow = false;

						if (Cars[loop].bWarpIfStuckFlag)
						{
							if (Cars[loop].bStuck)
							{
								bAttemptToWarpThisCarNow = true;
							}
						}

						if (Cars[loop].bWarpIfUpsideDownFlag)
						{
							if(CScriptCars::GetUpsideDownCars().IsCarUpsideDown(pVehicle))
								bAttemptToWarpThisCarNow = true;
						}

						if (Cars[loop].bWarpIfInWaterFlag)
						{
							if (pVehicle->GetIsInWater())
							{
								bAttemptToWarpThisCarNow = true;
							}
						}

						if (bAttemptToWarpThisCarNow)
						{
							if (camInterface::IsSphereVisibleInGameViewport(pVehicle->GetBoundCentre(), pVehicle->GetBoundRadius()) == false)
							{	//	car is offscreen
								if (Cars[loop].NumberOfNodesToCheck < 0)
								{
									CVehicleNodeList * pNodeList = pVehicle->GetIntelligence()->GetNodeList();
									Assertf(pNodeList, "CStuckCarCheck::Process - vehicle has no node list");
									if(pNodeList)
									{									
										//	find the vehicle's last route coords
										CNodeAddress OldNode, NewNode;

										s32 iTargetNode = pNodeList->GetTargetNodeIndex();
										s32 iOldNode = pNodeList->GetTargetNodeIndex() - 1;

										OldNode = pNodeList->GetPathNodeAddr(iOldNode);
										NewNode = pNodeList->GetPathNodeAddr(iTargetNode);

										bool bSuccess;
										NewCoords = ThePaths.FindNodeCoorsForScript(OldNode, NewNode, &NewHeading, &bSuccess);
		//								Assert(bSuccess);
										if (bSuccess)
										{
											AttemptToWarpVehicle(pVehicle, NewCoords, NewHeading);
										}
									}
								}
								else
								{	//	closest car nodes - check up to NumberOfNodesToCheck closest car nodes from its current position
									u8 ClosestNodeIndex = 0;
									bool bFound = false;
									const Vector3 vVehiclePosition = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
									while ( (ClosestNodeIndex < Cars[loop].NumberOfNodesToCheck) && !bFound)
									{
										ResultNode = ThePaths.FindNthNodeClosestToCoors(vVehiclePosition, 999999.9f, false, ClosestNodeIndex);

										bool	bSuccess;
										NewCoords = ThePaths.FindNodeCoorsForScript(ResultNode, &bSuccess);
	//									Assert(bSuccess);	// Code needed to deal with this.

										if (bSuccess)
										{
											NewHeading = ThePaths.FindNodeOrientationForCarPlacement(ResultNode);	//	in degrees

											if (AttemptToWarpVehicle(pVehicle, NewCoords, NewHeading))
											{
												bFound = true;
											}
											else
											{
												ClosestNodeIndex++;
											}
										}
										else
										{
											ClosestNodeIndex++;
										}
									}
								}
							}
						}
					}	//	end of if (Cars[loop].bWarpCar)
				}	//	end of if (pVehicle->pDriver)
				else
				{	//	vehicle doesn't have a driver
					Cars[loop].LastPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
					Cars[loop].LastChecked = TimeStep;
				}
			}
			else
			{	//	Car is no longer on the map - remove it from the array of cars to check
				ResetArrayElement(loop);
			}
		}	//	end of if (Cars[loop].m_CarID != NULL_IN_SCRIPTING_LANGUAGE)
	}	//	end of for
}

void CStuckCarCheck::AddCarToCheck(s32 NewCarID, float StuckRad, u32 CheckEvery, bool bWarpCar, bool bStuckFlag, bool bUpsideDownFlag, bool bInWaterFlag, s8 NumberOfNodesToCheck)
{
	const CVehicle *pVehicle = CTheScripts::GetEntityToQueryFromGUID<CVehicle>(NewCarID);
	if (pVehicle)
	{	//	Only add the car if it is valid
		u32 loop = 0;

	// Check car is not already in the array
		while (loop < MAX_STUCK_CAR_CHECKS)
		{
			scriptAssertf( (Cars[loop].m_CarID != NewCarID), "CStuckCarCheck::AddCarToCheck - Trying to add car to stuck car check again");
			loop++;
		}

		loop = 0;

	// Find a space in the array for the car
		while ((loop < MAX_STUCK_CAR_CHECKS) && (Cars[loop].m_CarID != NULL_IN_SCRIPTING_LANGUAGE))
		{
			loop++;
		}

		scriptAssertf( (loop < MAX_STUCK_CAR_CHECKS), "CStuckCarCheck::AddCarToCheck - Too many stuck car checks");

	// Add the car to the array
		if (loop < MAX_STUCK_CAR_CHECKS)
		{
			Cars[loop].m_CarID = NewCarID;
			Cars[loop].LastPos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
			Cars[loop].LastChecked = fwTimer::GetTimeInMilliseconds();
			Cars[loop].StuckRadius = StuckRad;
			Cars[loop].CheckTime = CheckEvery;
			Cars[loop].bStuck = FALSE;

			Cars[loop].bWarpCar = bWarpCar;
			Cars[loop].bWarpIfStuckFlag = bStuckFlag;
			Cars[loop].bWarpIfUpsideDownFlag = bUpsideDownFlag;
			Cars[loop].bWarpIfInWaterFlag = bInWaterFlag;
			Cars[loop].NumberOfNodesToCheck = NumberOfNodesToCheck;
		}
	}
}

void CStuckCarCheck::RemoveCarFromCheck(s32 RemoveCarID)
{
	for (u32 loop = 0; loop < MAX_STUCK_CAR_CHECKS; loop++)
	{
		if (Cars[loop].m_CarID == RemoveCarID)
		{
			ResetArrayElement(loop);
		}
	}
}

//	Clears the stuck flag if the car is present in the stuck car check array
void CStuckCarCheck::ClearStuckFlagForCar(s32 CheckCarID)
{
	for (u32 loop = 0; loop < MAX_STUCK_CAR_CHECKS; loop++)
	{
		if (Cars[loop].m_CarID == CheckCarID)
		{
			Cars[loop].bStuck = false;
		}
	}
}


bool CStuckCarCheck::IsCarInStuckCarArray(s32 CheckCarID)
{
	for (u32 loop = 0; loop < MAX_STUCK_CAR_CHECKS; loop++)
	{
		if (Cars[loop].m_CarID == CheckCarID)
		{
			return true;
		}
	}

	return false;
}

// End of code for checking for mission cars being stuck

bool CBoatIsBeachedCheck::IsBoatBeached(const CVehicle& in_VehicleToCheck)
{
	if ( in_VehicleToCheck.InheritsFromBoat() )
	{
		if ( in_VehicleToCheck.GetIntelligence() )
		{
			if ( in_VehicleToCheck.GetIntelligence()->GetBoatAvoidanceHelper() )
			{
				if ( in_VehicleToCheck.GetIntelligence()->GetBoatAvoidanceHelper()->IsBeached() )
				{
					return true;
				}
			}
		}
	}
	return false;
}

// end of code for checking boats beached

void CScriptCars::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
    {
	    UpsideDownCars.Init();

	    StuckCars.Init();

	    SuppressedCarModels.ClearAllSuppressedModels();
	}
}

void CScriptPeds::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
    {
		SuppressedPedModels.ClearAllSuppressedModels();

	    CTaskSequences::Init();
	    CPedGroups::Init();

	    CInformFriendsEventQueue::Init();
	    CInformGroupEventQueue::Init();
	}
}

void CScriptEntities::Init(unsigned initMode)
{
	if(initMode == INIT_SESSION)
    {
	}
}

void CScriptCars::Process(void)
{
	UpsideDownCars.UpdateTimers();

	StuckCars.Process();
}

void CScriptPeds::Process(void)
{
	// Certain events can be suppressed per frame through script.
	// To support this, reset the suppression at the last possible moment (i.e., now).
	// That way we are certain that all events had a chance to be blocked as they can be
	// created at various times in the update cycle.
	CShockingEventsManager::ProcessPreScripts();

	// Likewise, certain aspects of scenario usage can be suppressed per frame through script.
	CScenarioManager::ProcessPreScripts();

	// Clear the suppression of shocking events this frame before scripts get a chance to run.
	CAgitatedManager::GetInstance().SetSuppressAgitationEvents(false);

	// Clear reset variables in the wildlife manager.
	CWildlifeManager::GetInstance().ProcessPreScripts();

	// Clear ped reset variables.
	CPed::SetRandomPedsBlockingNonTempEventsThisFrame(false);

	// Reset cop perception override parameters if set by script using SET_COP_PERCEPTION_OVERRIDES (get set in CPed::ProcessPreRender).
	bool bResetCopPerceptionOverrides = CPedPerception::ms_bCopOverridesSet;
	if (bResetCopPerceptionOverrides)
	{
		CPedPerception::ResetCopOverrideParameters();
	}

	CPedAccuracy::fMPAmbientLawPedAccuracyModifier = 1.0f;

	CPed::Pool* pPedPool = CPed::GetPool();
	s32 i=pPedPool->GetSize();
	while(i--)
	{
		CPed *pPed = pPedPool->GetSlot(i);
		if(pPed)
		{
			//Clear all the recorded events for every mission ped.
			if(pPed->PopTypeIsMission())
			{
				pPed->GetPedIntelligence()->RecordEventForScript(EVENT_NONE,0);
			}

			// Reset anything on players that needs to be reset pre-script
			if(pPed->IsPlayer() && pPed->GetPlayerInfo())
			{
				pPed->GetPlayerInfo()->ResetPreScript();
			}

			// Reset the perception data on cops if overrides were previously set.
			if (bResetCopPerceptionOverrides && pPed->IsCopPed())
			{
				pPed->GetPedIntelligence()->SetPedPerception(pPed);
			}
		}
	}
}


void CSuppressedModels::ClearAllSuppressedModels(void)
{
	u32 loop = 0;
	while (loop < SuppressedModels.GetCount())
	{
		gPopStreaming.RequestVehicleDriver(SuppressedModels[loop].SuppressedModel);
		loop++;
	}
	SuppressedModels.Reset();
}

bool CSuppressedModels::AddToSuppressedModelArray(u32 NewModel, const strStreamingObjectName pScriptName)
{
	if (HasModelBeenSuppressed(NewModel))
	{	//	This model is already in the array so just return
		return false;
	}

	if (!scriptVerifyf(!SuppressedModels.IsFull(), "AddToSuppressedModelArray - no space left to add another model, max is %i", SuppressedModels.GetMaxCount()))
	{
		return false;
	}

	suppressed_model_struct st;
	st.SuppressedModel = NewModel;
	st.NameOfScriptThatHasSuppressedModel = pScriptName;

	SuppressedModels.Push(st);

	gPopStreaming.UnrequestVehicleDriver(NewModel);

	return true;
}

bool CSuppressedModels::RemoveFromSuppressedModelArray(u32 ModelToRemove)
{
	u32 loop = 0;
	while (loop < SuppressedModels.GetCount())
	{
		if (SuppressedModels[loop].SuppressedModel == ModelToRemove)
		{
			SuppressedModels.DeleteFast(loop);

			gPopStreaming.RequestVehicleDriver(ModelToRemove);

			return true; // we're already testing to make sure we don't have duplicates in AddToSuppressedModelArray()
		}
		loop++;
	}
	return false;
}

bool CSuppressedModels::HasModelBeenSuppressed(u32 Model)
{
	u32 loop = 0;
	while (loop < SuppressedModels.GetCount())
	{
		if (SuppressedModels[loop].SuppressedModel == Model)
		{
			return true;
		}
		loop++;
	}

	return false;
}

#if __BANK
void CSuppressedModels::PrintSuppressedModels()
{
	for (int loop = 0; loop < SuppressedModels.GetCount(); loop++)
	{
		if (SuppressedModels[loop].SuppressedModel != fwModelId::MI_INVALID)
		{
			scriptDisplayf("%s suppressed by %s", 
				CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(SuppressedModels[loop].SuppressedModel))), 
				SuppressedModels[loop].NameOfScriptThatHasSuppressedModel.GetCStr());
		}
	}
}
#endif	//	__BANK

#if DEBUG_DRAW
void CSuppressedModels::DisplaySuppressedModels()
{
	char debugText[255];

	bool bAnySuppressedModels = false;
	formatf(debugText, "Suppressed models:");
	char* debugTextPtr = debugText;
	debugTextPtr += strlen(debugTextPtr);
	for (s32 i = 0; i < SuppressedModels.GetCount(); i++)
	{
		if (SuppressedModels[i].SuppressedModel != fwModelId::MI_INVALID)
		{
#if !__FINAL
			formatf(debugTextPtr, debugText + sizeof(debugText) - debugTextPtr, " %s", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(SuppressedModels[i].SuppressedModel))));
#else
			formatf(debugTextPtr, debugText + sizeof(debugText) - debugTextPtr, " %d", SuppressedModels[i].SuppressedModel);
#endif
			debugTextPtr += strlen(debugTextPtr);
			bAnySuppressedModels = true;
		}
	}
	if (bAnySuppressedModels)
	{
		grcDebugDraw::AddDebugOutput(debugText);
	}
}
#else	//	DEBUG_DRAW
void CSuppressedModels::DisplaySuppressedModels()
{
}
#endif	//	DEBUG_DRAW

CRestrictedModels::CRestrictedModels()
{
	ClearAllRestrictedModels();
}

void CRestrictedModels::ClearAllRestrictedModels()
{
	RestrictedModels.Reset();
}

void CRestrictedModels::AddToRestrictedModelArray(u32 model, const strStreamingObjectName scriptName)
{  
	if (HasModelBeenRestricted(model))
	{	//	This model is already in the array so just return
		return;
	}

	scriptAssertf(!RestrictedModels.IsFull(), "AddToRestrictedModelArray - no space left to add another model");

	RestrictedModelStruct st;
	st.m_model = model;
	st.m_scriptName = scriptName;

	RestrictedModels.Push(st);
}

void CRestrictedModels::RemoveFromRestrictedModelArray(u32 model)
{
	u32 loop = 0;
	while (loop < RestrictedModels.GetCount())
	{
		if (RestrictedModels[loop].m_model == model)
		{
			RestrictedModels.DeleteFast(loop);
			break; // we're already testing to make sure we don't have duplicates in AddToSuppressedModelArray()
		}
		loop++;
	}
}

bool CRestrictedModels::HasModelBeenRestricted(u32 model) const
{
	u32 loop = 0;
	while (loop < RestrictedModels.GetCount())
	{
		if (RestrictedModels[loop].m_model == model)
		{
			return true;
		}
		loop++;
	}
	return false;
}


void CRestrictedModels::DisplayRestrictedModels()
{
#if DEBUG_DRAW
	char debugText[255];
	bool bAnyRestrictedModels = false;
	formatf(debugText, "Restricted Models:");
	char* debugTextPtr = debugText;
	debugTextPtr += strlen(debugTextPtr);
	for (s32 i = 0; i < RestrictedModels.GetCount(); i++)
	{
		if (RestrictedModels[i].m_model != fwModelId::MI_INVALID)
		{
#if !__FINAL
			formatf(debugTextPtr, debugText + sizeof(debugText) - debugTextPtr, " %s", CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(RestrictedModels[i].m_model))));
#else
			formatf(debugTextPtr, debugText + sizeof(debugText) - debugTextPtr, " %d", RestrictedModels[i].m_model);
#endif
			debugTextPtr += strlen(debugTextPtr);
			bAnyRestrictedModels = true;
		}
	}
	if (bAnyRestrictedModels)
	{
		grcDebugDraw::AddDebugOutput(debugText);
	}
#endif // DEBUG_DRAW
}

#if __BANK
void CRestrictedModels::PrintRestrictedModels()
{
	for (int loop = 0; loop < RestrictedModels.GetCount(); loop++)
	{
		if (RestrictedModels[loop].m_model != fwModelId::MI_INVALID)
		{
			scriptDisplayf("%s restricted by %s", 
				CModelInfo::GetBaseModelInfoName(fwModelId(strLocalIndex(RestrictedModels[loop].m_model))), 
				RestrictedModels[loop].m_scriptName.GetCStr());
		}
	}
}
#endif	//	__BANK

bool CScriptPeds::IsPedStopped(const CPed *pPedToCheck)
{
	if (scriptVerifyf(pPedToCheck, "CScriptPeds::IsPedStopped - Ped doesn't exist"))
	{
		if (pPedToCheck->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPedToCheck->GetMyVehicle())
		{
			scriptAssertf(pPedToCheck->GetMyVehicle(), "CScriptPeds::IsPedStopped - Ped is not in a vehicle");

			if (pPedToCheck->GetMyVehicle()->GetDistanceTravelled() <= (AS_GOOD_AS_STOPPED_SPEED * 50.0f*fwTimer::GetTimeStep()))
			{
				return TRUE;
			}
		}
		else
		{
			if (pPedToCheck->IsPlayer())
			{
				if (pPedToCheck->GetPedIntelligence()->FindTaskByType(CTaskTypes::TASK_JUMPVAULT)) 
					return FALSE;
			}

			if (pPedToCheck->GetMotionData()->GetIsStill())
			{
				if ( (pPedToCheck->GetPedConfigFlag( CPED_CONFIG_FLAG_IsInTheAir ) == false) && (pPedToCheck->GetPedResetFlag( CPED_RESET_FLAG_IsLanding ) == false) && pPedToCheck->GetIsStanding() )
				{
					fwDynamicEntityComponent *pedDynComp = pPedToCheck->GetDynamicComponent();

					if (!pedDynComp || pedDynComp->GetAnimatedVelocity().XYMag2() == 0.0f)
					{
						return TRUE;
					}
				}
			}
		}
	}

	return FALSE;
}

bool CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(u32 modelIndex)
{
	CPedModelInfo *pPedModelInfo = static_cast<CPedModelInfo *>(CModelInfo::GetBaseModelInfo(fwModelId(strLocalIndex(modelIndex))));
	return HasPedModelBeenRestrictedOrSuppressed(modelIndex, pPedModelInfo);
}

bool CScriptPeds::HasPedModelBeenRestrictedOrSuppressed(u32 modelIndex, const CPedModelInfo *pPedModelInfo)
{
	return (pPedModelInfo && (GetSuppressedPedModels().HasModelBeenSuppressed(modelIndex) || (pPedModelInfo->GetNumTimesOnFootAndInCar() > 0 && GetRestrictedPedModels().HasModelBeenRestricted(modelIndex))));
}

bool CScriptCars::IsVehicleStopped(const CVehicle *pVehicleToCheck)
{
	if (scriptVerifyf(pVehicleToCheck, "CScriptCars::IsVehicleStopped - Vehicle doesn't exist"))
	{
		//When a ped exits a vehicle it moves, so this tries to compensate for this amount of movement. 
		if (pVehicleToCheck->GetHandBrake())
		{
			if (pVehicleToCheck->GetVelocity().Mag2() <= 0.05*0.05 )//, fwTimer::GetOldTimeStep()))) GetOldTimeStep doesn't exist anymore (Obbe)
			{
				return TRUE;
			}
		}
		else
		{
			if (pVehicleToCheck->GetVelocity().Mag2() <= AS_GOOD_AS_STOPPED_SPEED*AS_GOOD_AS_STOPPED_SPEED )//, fwTimer::GetOldTimeStep()))) GetOldTimeStep doesn't exist anymore (Obbe)
			{
				return TRUE;
			}
		}
	}

	return FALSE;
}


void CScriptPeds::SetPedCoordinates(CPed* pPed, float NewX,float NewY, float NewZ, int Flags, bool bWarp, bool bResetPlants)
{
#if !__FINAL && !__NO_OUTPUT && __BANK
	if (pPed && pPed->IsPlayer() && g_PlayerSwitch.IsActive())
	{
		Displayf("[Playerswitch] Script %s is moving player ped %s to (%4.2f, %4.2f, %4.2f) during switch. Call stack follows", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetModelName(), NewX, NewY, NewZ);
		scrThread::PrePrintStackTrace();
	}

	if (pPed && pPed->IsPlayer() && g_LoadScene.IsActive())
	{
		Displayf("[Playerswitch] Script %s is moving player ped %s to (%4.2f, %4.2f, %4.2f) during load scene. Call stack follows", CTheScripts::GetCurrentScriptNameAndProgramCounter(), pPed->GetModelName(), NewX, NewY, NewZ);
		scrThread::PrePrintStackTrace();
	}
#endif

	bool bWarpGang = false;
	if (Flags & SetPedCoordFlag_WarpGang)
	{
		bWarpGang = true;
	}

	bool bOffset = false;
	if (Flags & SetPedCoordFlag_OffsetZ)
	{
		bOffset = true;
	}

	bool bWarpCar = false;
	if (Flags & SetPedCoordFlag_WarpCar)
	{
		bWarpCar = true;
	}

	CVehicle* pVehicle = NULL;
	if (pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
	{
		pVehicle = pPed->GetMyVehicle();
	}

	bool bOnGround = false;
	if (NewZ <= INVALID_MINIMUM_WORLD_Z)
	{
		NewZ = WorldProbe::FindGroundZForCoord(BOTTOM_SURFACE, NewX, NewY);
		bOnGround = true;
	}

	// Don't clear tasks if the ped is injured, and deactivate the ragdoll in a safe way.
	bool bKeepTasks = pPed->IsInjured() || (Flags & SetPedCoordFlag_KeepTasks);
	if( bKeepTasks ||
		( pPed->GetUsingRagdoll() && bWarp ) )
	{
		pPed->DeactivatePhysicsAndDealWithRagdoll();

		// We have seen a number of crashes when going into spectator mode.
		// These seem to be caused by the ragdoll inst still having active contacts but they are now a long way from where the ped is
		// to try and fix that we will just make sure we clear all the ragdoll inst contacts
		if( pPed->GetRagdollInst() )
		{
			CPhysics::ClearManifoldsForInst( pPed->GetRagdollInst() );
		}
	}

	if(pVehicle && pVehicle->GetVehicleType()==VEHICLE_TYPE_TRAIN)
	{
		pPed->SetPedOutOfVehicle(CPed::PVF_Warp|CPed::PVF_IgnoreSafetyPositionCheck);
		pVehicle = NULL;
	}

	if (pVehicle && !bWarpCar)
	{
		CNetObjPed *pNetObjPed = (CNetObjPed *)pPed->GetNetworkObject();
		if(pNetObjPed)
		{
			pNetObjPed->SetPedOutOfVehicle(CPed::PVF_IgnoreSafetyPositionCheck);
		}
		else
		{
//			pPed->GetPedIntelligence()->FlushImmediately(true);
			pPed->SetPedOutOfVehicle(CPed::PVF_Warp|CPed::PVF_IgnoreSafetyPositionCheck);
		}

		pVehicle = NULL;
	}

	if (pVehicle && bWarpCar)
	{
		scriptAssertf(!pVehicle->IsNetworkClone(), "%s:Attempting to set the coordinates of a ped in a vehicle controlled on another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter());

		CVehiclePopulation::SetCoordsOfScriptCar(pVehicle, NewX, NewY, NewZ, false, true);
	}
	else
	{
#if __ASSERT
		if ((Flags & SetPedCoordFlag_AllowClones)==0)
		{
			scriptAssertf(!pPed->IsNetworkClone(), "%s:Attempting to set the coordinates of a ped controlled on another machine", CTheScripts::GetCurrentScriptNameAndProgramCounter());
		}
#endif
		if(bOffset)
		{
			NewZ += pPed->GetCapsuleInfo()->GetGroundToRootOffset();	//	pPed->GetDistanceFromCentreOfMassToBaseOfModel();
		}

        if(NetworkInterface::IsGameInProgress())
        {
            // we can't warp the entire group in the network game as the different group members
            // may be owned by other machines - this isn't the nicest way to fix this, but it's
            // what the scripters want!
            bWarpGang = false;
        }

		bool bKeepIK = (Flags & SetPedCoordFlag_KeepIK) != 0;

		CPedGroup* pPedGroup=pPed->GetPedsGroup();
		if(pPedGroup && pPedGroup->GetGroupMembership()->IsLeader(pPed) && bWarpGang)
		{
			//Ped is leader of the group so teleport the whole group.
			pPedGroup->Teleport(Vector3(NewX, NewY, NewZ), bKeepTasks, bKeepIK, bResetPlants);
		}
		else
		{
			//Ped isn't leader of any group so just teleport the ped.
			pPed->Teleport(Vector3(NewX, NewY, NewZ), pPed->GetCurrentHeading(), bKeepTasks, true, bKeepIK, bWarp, bResetPlants);
		}

		if(bOnGround)
		{
			pPed->SetIsStanding(true);
			pPed->SetWasStanding(true);
			pPed->SetGroundOffsetForPhysics(-pPed->GetCapsuleInfo()->GetGroundToRootOffset());
		}

		//Clear some space for pPed.
		if(bWarp)
		{
			CScriptEntities::ClearSpaceForMissionEntity(pPed);
		}
	}

	CStreaming::SetIsPlayerPositioned(true);
}


// Name			:	CScriptEntities::ClearSpaceForMissionEntity
// Purpose		:	Removes any unimportant peds and vehicles from the space
//					which is occupied by a newly-created mission entity
// Parameters	:	pointer to the entity
// Returns		:	nothing
void CScriptEntities::ClearSpaceForMissionEntity(CEntity *pEnt)
{
	CEntity *pCurrentEnt;
	CPed *pPed;
	CVehicle *pVehicle;
	u16 loop;

	float Range = pEnt->GetBoundRadius();
	s32 NumberOfCollisions = 0;

//	Check for collisions with vehicles, peds, objects and dummies
	CEntity *pCollidingEnts[16];
	CGameWorld::FindObjectsKindaColliding(VEC3V_TO_VECTOR3(pEnt->GetTransform().GetPosition()), Range, FALSE, FALSE, &NumberOfCollisions, 16, pCollidingEnts,
				FALSE, TRUE, TRUE, FALSE, FALSE);	//	TRUE, TRUE);
//				FALSE, TRUE, TRUE, TRUE, TRUE);

	
	Matrix34 m = MAT34V_TO_MATRIX34(pEnt->GetMatrix());

	if (NumberOfCollisions > 0)
	{
		for (loop = 0; loop < NumberOfCollisions; loop++)
		{
			pCurrentEnt = pCollidingEnts[loop];
			if (pCurrentEnt != pEnt)
			{
				switch (pCurrentEnt->GetType())
				{
					case ENTITY_TYPE_PED :
						{
							//	Peds in vehicles will be deleted when the vehicle is deleted
							//	so don't delete them seperately
							pPed = (CPed *) pCurrentEnt;
							if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
							{
								pCollidingEnts[loop] = NULL;
							}
							if(pPed->IsPlayer() || !pPed->CanBeDeleted())
							{
								pCollidingEnts[loop] = NULL;
							}
						}
						break;
					case ENTITY_TYPE_VEHICLE :
						{
							pVehicle = (CVehicle *)pCurrentEnt;
							// The false here (at the end of CanBeDeleted) means that being the player's last car is not enough to stop it being removed
							if(pVehicle->m_nVehicleFlags.bCannotRemoveFromWorld || !pVehicle->CanBeDeletedSpecial(true, true, true, true, false))
							{
								pCollidingEnts[loop] = NULL;
							}
						}
						break;
				}
			}
		}

		for(loop = 0; loop < NumberOfCollisions; loop++)
		{
			pCurrentEnt = pCollidingEnts[loop];
			if( (pCurrentEnt != pEnt) && (pCurrentEnt != NULL))
			{
				if(pEnt->GetCurrentPhysicsInst() && pCurrentEnt->GetCurrentPhysicsInst() &&
					Verifyf(pCurrentEnt->GetCurrentPhysicsInst() != reinterpret_cast<phInst*>(0xDDDDDDDD) && pCurrentEnt->GetCurrentPhysicsInst() != reinterpret_cast<phInst*>(0xCDCDCDCD), "Entity with no valid physics instance caught!"))
				{
					WorldProbe::CShapeTestBoundDesc boundTestDesc;
					boundTestDesc.SetDomainForTest(WorldProbe::TEST_AGAINST_INDIVIDUAL_OBJECTS);
					boundTestDesc.SetBoundFromEntity(pEnt);
					boundTestDesc.SetIncludeEntity(pCurrentEnt);
					boundTestDesc.SetTransform(&m);

					if(WorldProbe::GetShapeTestManager()->SubmitTest(boundTestDesc))
					{
						switch (pCurrentEnt->GetType())
						{
						case ENTITY_TYPE_VEHICLE :

							Displayf("Will try to delete a vehicle where a mission entity should be\n");
							pVehicle = (CVehicle *)pCurrentEnt;

							// this test is now done at an earlier stage to avoid wasted work in TestEntityAgainstEntity 
							// just assert to check that change worked ok
							Assert(!pVehicle->m_nVehicleFlags.bCannotRemoveFromWorld && pVehicle->CanBeDeletedSpecial(true, true, true, true, false));
							{
								//skip any cargo cars, their parents will clean them up
								if (CVehicle::IsEntityAttachedToTrailer(pVehicle))
								{
									continue;
								}

								pVehicle->RemoveDeleteWithParentProgenyFromList(&pCollidingEnts[loop+1],NumberOfCollisions-loop-1);
								if (pVehicle->HasDummyAttachmentChildren())
								{
									for (int i = 0; i < pVehicle->GetMaxNumDummyAttachmentChildren(); i++)
									{
										if (pVehicle->GetDummyAttachmentChild(i))
										{
											std::replace( &pCollidingEnts[loop+1], pCollidingEnts + NumberOfCollisions, (CEntity*) pVehicle->GetDummyAttachmentChild(i), (CEntity*) NULL );
											pVehicle->GetDummyAttachmentChild(i)->RemoveDeleteWithParentProgenyFromList(&pCollidingEnts[loop+1],NumberOfCollisions-loop-1);
										}
									}
								}

								CVehicleFactory::GetFactory()->Destroy(pVehicle);

							}

							// Is it safe to just assume that the vehicle has been deleted?

							break;

						case ENTITY_TYPE_PED :

							pPed = (CPed *) pCurrentEnt;

							// this test is now done at an earlier stage to avoid wasted work in TestEntityAgainstEntity 
							Assert(!pPed->IsPlayer() && pPed->CanBeDeleted());
							{
								// Is it safe to just delete any ped (including cops/emergency)?
								pPed->RemoveDeleteWithParentProgenyFromList(&pCollidingEnts[loop+1],NumberOfCollisions-loop-1);
								CPedFactory::GetFactory()->DestroyPed(pPed);
								Displayf("Deleted a ped where a mission entity should be\n");
							}

							break;

						default :
							scriptAssertf(0, "ClearSpaceForMissionEntity - Colliding Entity Type not handled");
							break;
						}
					}
				}
			}
		}
	}
}


//
// name:		GivePedScriptedTask
// description:	Give ped a new task. Instead of just giving a task the ped gets given
//				a scripted task event. This ensures the ped will get the task and that
//				it doesn't break the current task
void CScriptPeds::GivePedScriptedTask(s32 pedId, CTaskSequenceList* pTaskList, s32 CurrCommand,const  char* commandName)
{
	CTask* pTask = rage_new CTaskUseSequence(*pTaskList);
	GivePedScriptedTask(pedId, pTask, CurrCommand, commandName);
}

void CScriptPeds::GivePedScriptedTask(s32 iPedID, aiTask* pTask, s32 CurrCommand, const char* commandName)
{
#if __DEV
	char tmp[512];
	sprintf(tmp, "%s, %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName);
#endif

	if (NetworkInterface::IsGameInProgress() && !CQueriableInterface::IsTaskHandledByNetwork(pTask->GetTaskType()) && !CQueriableInterface::CanTaskBeGivenToNonLocalPeds(pTask->GetTaskType()))
	{
		scriptAssertf(0, "%s : %s (%s) - This script command is not supported in network game scripts", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, pTask->GetName().c_str());
	}

	if (NULL_IN_SCRIPTING_LANGUAGE != iPedID)	//	New Rage script compiler uses NULL instead of -1 for sequence tasks
    {
		// Give task to ped
		CPed* pPed = CTheScripts::GetEntityToModifyFromGUID<CPed>(iPedID, CTheScripts::GUID_ASSERT_FLAGS_INCLUDE_CLONES);

		if (!pPed)
		{
			delete pTask;
			return;
		}

        // if we are giving a task to a ped we are not currently in control of we need the network code to handle it
        if(pPed->IsNetworkClone() || (pPed->GetNetworkObject() && pPed->GetNetworkObject()->IsPendingOwnerChange()))
        {
            gnetDebug2("%s : Calling %s on remotely owned %s", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, pPed->GetNetworkObject()->GetLogName());
            NetworkInterface::GivePedScriptedTask(pPed, pTask, commandName);
   			return;
        }

    	scriptAssertf(-1==CTaskSequences::ms_iActiveSequence, "%s:Sequence opened unexpectedly", tmp);

		//	A ped with a brain can only be given a task by its own ped brain script
		CPed* pPedForBrain=0;
		s8 ScriptBrainType = CTheScripts::GetCurrentGtaScriptThread()->ScriptBrainType;
		if ( (CScriptsForBrains::PED_STREAMED==ScriptBrainType) || (CScriptsForBrains::SCENARIO_PED==ScriptBrainType) )
		{
			pPedForBrain = CTheScripts::GetEntityToModifyFromGUID<CPed>(CTheScripts::GetCurrentGtaScriptThread()->EntityIndexForScriptBrain);
		}

		if(pPed->GetPedConfigFlag( CPED_CONFIG_FLAG_HasAScriptBrain ) && pPedForBrain!=pPed)
		{
			delete pTask;
			return;
		}

		// check that a sequence being used by this ped is not too large
		if (pPed->GetNetworkObject() && !pPed->IsPlayer() && pTask->GetTaskType() == CTaskTypes::TASK_USE_SEQUENCE)
		{
			const CTaskSequenceList* pSeqList = static_cast<CTaskUseSequence*>(pTask)->GetTaskSequenceList();

			if (pSeqList)
			{
				u32 numTasks = pSeqList->GetTaskList().GetNumTasks();

				if (numTasks > IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS)
				{
					scriptAssertf(0, "%s : %s  - The sequence used for this task is too big (%d), the max size currently supported is %d. Speak to a network coder", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, numTasks, IPedNodeDataAccessor::MAX_SYNCED_SEQUENCE_TASKS);
				}
			}
		}

		// create event and give to ped
		CEventScriptCommand event(PED_TASK_PRIORITY_PRIMARY,pTask);
#if __DEV
		event.SetScriptNameAndCounter(CTheScripts::GetCurrentScriptName(), CTheScripts::GetCurrentGtaScriptThread()->GetThreadPC());
#endif
		pPed->GetPedIntelligence()->RemoveEventsOfType( EVENT_SCRIPT_COMMAND );
		pPed->GetPedAiLod().SetForceNoTimesliceIntelligenceUpdate(true);// ensure event is processed next frame
		CEvent* pEvent=pPed->GetPedIntelligence()->AddEvent(event,true);
		if(pEvent)
		{
			const int iVacantSlot=CPedScriptedTaskRecord::GetVacantSlot();
			Assert(iVacantSlot>=0);
			if( iVacantSlot >= 0 )
			{
#if __DEV
				CPedScriptedTaskRecord::ms_scriptedTasks[iVacantSlot].Set(pPed,CurrCommand,(const CEventScriptCommand*)pEvent,tmp);
#else
				CPedScriptedTaskRecord::ms_scriptedTasks[iVacantSlot].Set(pPed,CurrCommand,(const CEventScriptCommand*)pEvent);
#endif
			}
		}
    }
    else
    {
		// Add task to sequence
		scriptAssertf(CTaskSequences::ms_bIsOpened[CTaskSequences::ms_iActiveSequence], "%s:Sequence task closed", tmp);
		if (scriptVerifyf(CTaskSequences::ms_iActiveSequence >=0 && CTaskSequences::ms_iActiveSequence<CTaskSequences::MAX_NUM_SEQUENCE_TASKS, "%s:Sequence task closed", tmp))
		{
#if __ASSERT
			if (NetworkInterface::IsGameInProgress() && !CTaskSequences::ms_TaskSequenceLists[CTaskSequences::ms_iActiveSequence].IsMigrationPrevented())
			{
				aiTask* pTaskToCheck = pTask;

				// the subtask of a control movement task is what is synced and recreated when the ped migrates
				if (pTask && pTask->GetTaskType() == CTaskTypes::TASK_COMPLEX_CONTROL_MOVEMENT)
				{
					CTaskComplexControlMovement *pMoveTask = SafeCast(CTaskComplexControlMovement, pTask);

					pTaskToCheck = pMoveTask->GetBackupCopyOfMovementSubtask();

					// sub task not currently synced
					scriptAssertf(pMoveTask->GetMainSubTaskType() == CTaskTypes::TASK_NONE, "%s : %s (%s) - CTaskComplexControlMovement - sub task not currently supported in MP", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, pTaskToCheck->GetName().c_str());
				}

				if (pTaskToCheck && !CQueriableInterface::CanTaskBeGivenToNonLocalPeds(pTaskToCheck->GetTaskType()))
				{
					scriptAssertf(0, "%s : %s (%s) - Adding this task to a sequence is not currently supported in MP (it may not run when the ped migrates)", CTheScripts::GetCurrentScriptNameAndProgramCounter(), commandName, pTaskToCheck->GetName().c_str());
					return;
				}
			}
#endif	// __ASSERT

			CTaskSequences::ms_TaskSequenceLists[CTaskSequences::ms_iActiveSequence].AddTask(pTask); 
		}
		else
		{
			delete pTask;
			return;
		}
    }
}

/*
CRaceTimes is a helper class for script to use to quickly get split times in races.
It's based around three arrays:
* ms_players stores all participating players where the index into the array is the player index used in the other arrays too.
* ms_splitTimes is used to cache the calculated split times and each player's position in the race. this will only need updating once any player crosses a new checkpoint.
* ms_records stores the absolute race time for each player, for each checkpoint, for each lap

ms_records is a flat array but it really stores a 3d array like this:

        |      | c0
        | lap0 | c1
player0 |      | c2
        |      | c0
        | lap1 | c1
        |      | c2

        |      | c0
        | lap0 | c1
player1 |      | c2
        |      | c0
        | lap1 | c1
        |      | c2
        
		  ...

It only stores the race time at each checkpoint as an s32 (c0 - cn).
*/

atArray<s32> CRaceTimes::ms_records;
atArray<CRaceTimes::sSplitTime> CRaceTimes::ms_splitTimes;
atArray<CRaceTimes::sPlayer> CRaceTimes::ms_players;
s32 CRaceTimes::ms_numCheckPoints = 0;
s32 CRaceTimes::ms_numLaps = 0;
s32 CRaceTimes::ms_numPlayers = 0;
s32 CRaceTimes::ms_lastLocalCheckpoint = -1;
s32 CRaceTimes::ms_localPlayerIndex = -1;
bool CRaceTimes::ms_modified = true;

#define MAX_PLAYERS_ALLOWED (32)

void CRaceTimes::Init(s32 numCheckpoints, s32 numLaps, s32 numPlayers, s32 localPlayerIndex)
{
    if (!Verifyf(numCheckpoints > 0, "CRaceTimes::Init: Invalid checkpoint count!"))
        return;
    if (!Verifyf(numPlayers > 0, "CRaceTimes::Init: Invalid player count!"))
        return;

    ms_records.Reset();
    ms_records.Resize(numCheckpoints * numLaps * numPlayers);

    ms_splitTimes.Reset();
    ms_splitTimes.Resize(numPlayers);

    for (s32 i = 0; i < numPlayers; ++i)
	{
        ms_splitTimes[i].time = 0;
        ms_splitTimes[i].position = i;
	}

    ms_players.Reset();
    ms_players.Resize(numPlayers);

    ms_localPlayerIndex = localPlayerIndex;
    ms_numCheckPoints = numCheckpoints;
    ms_numLaps = numLaps;
    ms_numPlayers = numPlayers;

    ms_lastLocalCheckpoint = -1;

    Displayf("[RACETIMES]: RaceTimes Init checkpoints: %d laps: %d players: %d", numCheckpoints, numLaps, numPlayers);

    if (!Verifyf(ms_numPlayers <= MAX_PLAYERS_ALLOWED, "Too many players (%d) passed to CRaceTime::Init! Max allowed is %d", numPlayers, MAX_PLAYERS_ALLOWED))
        ms_numPlayers = MAX_PLAYERS_ALLOWED;
}

void CRaceTimes::Shutdown()
{
    Displayf("[RACETIMES]: RaceTimes shutdown!");
    ms_records.Reset();
    ms_splitTimes.Reset();
    ms_players.Reset();

    ms_numPlayers = 0;
    ms_numCheckPoints = 0;
    ms_numLaps = 0;
    ms_lastLocalCheckpoint = -1;
    ms_localPlayerIndex = -1;
}

void CRaceTimes::CheckpointHit(s32 pedIndex, s32 checkpoint, s32 lap, s32 time)
{
    if (pedIndex < 0)
        return;

    if (!Verifyf(checkpoint >= 0 && checkpoint < ms_numCheckPoints, "Checkpoint %d out of %d passed in to CRaceTimes::CheckpointHit!", checkpoint, ms_numCheckPoints))
        return;

    if (!Verifyf(lap >= 0 && lap < ms_numLaps, "Lap %d out of %d passed in to CRaceTimes::CheckpointHit!", lap, ms_numLaps))
        return;

    s32 firstEmpty = ms_numPlayers;
    s32 playerIndex = ms_numPlayers;
    for (s32 i = 0; i < ms_numPlayers; ++i)
	{
        // store first empty index in case this ped isn't yet registered
        if (firstEmpty == ms_numPlayers && ms_players[i].pedIndex == -1)
            firstEmpty = i;

        if (ms_players[i].pedIndex == pedIndex)
		{
            playerIndex = i;
            break;
		}
	}

    if (playerIndex == ms_numPlayers && firstEmpty != ms_numPlayers)
    {
        playerIndex = firstEmpty;
        ms_players[playerIndex].pedIndex = pedIndex;
    }

    if (!Verifyf(playerIndex >= 0 && playerIndex < ms_numPlayers, "CRaceTimes::CheckpointHit: No empty slot found for player %d! index: %d", pedIndex, playerIndex))
		return;

    s32 arrayIndex = playerIndex * (ms_numCheckPoints * ms_numLaps) + lap * ms_numCheckPoints + checkpoint;
    Assert(arrayIndex >= 0 && arrayIndex < ms_numCheckPoints * ms_numLaps * ms_numPlayers);

    ms_players[playerIndex].lastCheckpoint = lap * ms_numCheckPoints + checkpoint;

    // store local player last checkpoint in static
    if (pedIndex == ms_localPlayerIndex)
        ms_lastLocalCheckpoint = ms_players[playerIndex].lastCheckpoint;

    Displayf("[RACETIMES]: Checkpoint Hit: player index: %d (%d)(local player: %p) checkpoint: %d lap: %d time: %d lastCheckPoint: %d", playerIndex, pedIndex, CPedFactory::GetFactory()->GetLocalPlayer(), checkpoint, lap, time, ms_players[playerIndex].lastCheckpoint);
    ms_records[arrayIndex] = time;
    ms_modified = true;
}

int SplitTimeCompare(const CRaceTimes::sSplitTime* a, const CRaceTimes::sSplitTime* b)
{
	return a->time - b->time;
}

bool CRaceTimes::UpdateTimes()
{
	// find local player's index into the record array
	s32 localPlayerIndex = ms_numPlayers;
	for (s32 i = 0; i < ms_numPlayers; ++i)
	{
		if (ms_players[i].pedIndex == ms_localPlayerIndex)
		{
			localPlayerIndex = i;
			break;
		}
	}

	if (!Verifyf(localPlayerIndex >= 0 && localPlayerIndex < ms_numPlayers, "CRaceTimes::GetPlayerTime: Local player %d not found in list!", ms_localPlayerIndex))
		return false;

#if 1 // position isn't currently used by script so skip sorting
	Assert(ms_lastLocalCheckpoint >= 0 && ms_lastLocalCheckpoint < ms_numCheckPoints * ms_numLaps);
	s32 arrayStep = ms_numCheckPoints * ms_numLaps;

	for (s32 i = 0; i < ms_numPlayers; ++i)
	{
		ms_splitTimes[i].position = i;

        s32 playerCheckpoint = ms_players[i].lastCheckpoint;

        // if this player is ahead of the local player, calculate the split time for the local player's last checkpoint
        if (playerCheckpoint > ms_lastLocalCheckpoint)
            playerCheckpoint = ms_lastLocalCheckpoint;

        // if no checkpoint has been passed set the time to 0, script deals with this
        if (playerCheckpoint < 0)
            continue;

		s32 localPlayerArrayIndex = playerCheckpoint + arrayStep * localPlayerIndex;

        Assertf(playerCheckpoint < ms_numCheckPoints * ms_numLaps, "Invalid checkpoint %d for player %d (%d)", playerCheckpoint, i, ms_players[i].pedIndex);
		ms_splitTimes[i].time = ms_records[playerCheckpoint + arrayStep * i] - ms_records[localPlayerArrayIndex];
	}
#else
	// calculate split times relative to the local player
	sSplitTime backingStore[MAX_PLAYERS_ALLOWED];
	atUserArray<sSplitTime> splitTimes(backingStore, MAX_PLAYERS_ALLOWED);

	Assert(ms_lastLocalCheckpoint >= 0 && ms_lastLocalCheckpoint < ms_numCheckPoints * ms_numLaps);
	s32 arrayStep = ms_numCheckPoints * ms_numLaps;
    s32 localPlayerArrayIndex = ms_lastLocalCheckpoint + arrayStep * localPlayerIndex;
	for (s32 i = 0; i < ms_numPlayers; ++i)
	{
        sSplitTime& newSplit = splitTimes.Append();
		newSplit.time = ms_records[ms_lastLocalCheckpoint + arrayStep * i] - ms_records[localPlayerArrayIndex];
		newSplit.position = i;
	}

	splitTimes.QSort(0, -1, SplitTimeCompare);

	// store the sorted times for later use
	for (s32 i = 0; i < ms_numPlayers; ++i)
	{
		s32 index = splitTimes[i].position;
		ms_splitTimes[index].time = splitTimes[i].time;
		ms_splitTimes[index].position = i;
	}
#endif

	ms_modified = false;
    return true;
}

bool CRaceTimes::GetPlayerTime(s32 pedIndex, s32& time, s32& position)
{
    time = 0;

    if (pedIndex < 0)
        return false;

    // local player hasn't crossed the first checkpoint yet
    if (ms_lastLocalCheckpoint < 0)
        return true;

    // find the given player's index into the record array
	s32 playerIndex = ms_numPlayers;
	for (s32 i = 0; i < ms_numPlayers; ++i)
	{
		if (ms_players[i].pedIndex == pedIndex)
		{
			playerIndex = i;
			break;
		}
	}

    // GetPlayerTime might be called before all players have hit the first checkpoint and we won't find them in the array
    if (playerIndex < 0 || playerIndex >= ms_numPlayers)
        return false;

    
#if 0 // we don't care about this now, we just calculate the last valid split time instead
    // current player hasn't crossed the same checkpoint as the player's last one, report no time
    if (ms_players[playerIndex].lastCheckpoint < ms_lastLocalCheckpoint)
        return false;
#endif

    // update times if needed
    if (ms_modified)
        if (!UpdateTimes())
            return false;
	
    time = ms_splitTimes[playerIndex].time;
    position = ms_splitTimes[playerIndex].position;
    Displayf("[RACETIMES]: Get Player Time: player index %d (%d) time: %d position: %d", playerIndex, pedIndex, time, position);

    return true;
}

const char* MISSION_CREATOR_DATA_FILE = "common:/data/missioncreatordata";
MissionCreatorAssetData* MissionCreatorAssets::ms_data = NULL;
atArray<MissionCreatorAssets::sAssetEntry> MissionCreatorAssets::ms_assets;
s32 MissionCreatorAssets::ms_freeMemory = 0;
void MissionCreatorAssets::Init()
{
    Shutdown();

	PARSER.LoadObjectPtr(MISSION_CREATOR_DATA_FILE, "meta", ms_data);
    Assertf(ms_data, "MissionCreatorAssets::Init: Failed to load %s", MISSION_CREATOR_DATA_FILE);

    if (ms_data)
	{
        ms_data->memory <<= 20;
        ms_freeMemory = ms_data->memory;
	}

#if !__FINAL
    if (PARAM_missioncreatordebug.Get())
        Displayf("[MISSION_CREATOR_ASSETS]: Init %d memory available", ms_freeMemory);
#endif
}

void MissionCreatorAssets::Shutdown()
{
    ms_assets.Reset();

    delete ms_data;
    ms_data = NULL;

    ms_freeMemory = 0;

#if !__FINAL
    if (PARAM_missioncreatordebug.Get())
        Displayf("[MISSION_CREATOR_ASSETS]: Shutdown");
#endif
}

bool MissionCreatorAssets::HasRoomForModel(fwModelId modelId)
{
	if (!ms_data)
		return false;

	if (modelId.IsValid())
	{
		// get all dependencies for this asset
		strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
		atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);
		CModelInfo::GetObjectAndDependencies(modelId, deps, gPopStreaming.GetResidentObjectList().GetElements(), gPopStreaming.GetResidentObjectList().GetCount());

		// sum up the additional memory cost this new model would incur
		u32 main = 0;
		u32 vram = 0;
		GetAdditionalModelCost(deps, main, vram);

#if !__FINAL
		if (PARAM_missioncreatordebug.Get())
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
			if (pModelInfo)
			{
				Displayf("[MISSION_CREATOR_ASSETS]: RequestingHasRoomForModel %s (cost %d) with free memory %d", pModelInfo->GetModelName(), main + vram, ms_freeMemory);
			}
		}
#endif

		// not enough space in the budget
		if ((s32)(main + vram) <= ms_freeMemory)
			return true;
	}

	return false;
}

bool MissionCreatorAssets::AddModel(fwModelId modelId)
{
    if (!ms_data)
        return false;

	if (modelId.IsValid())
	{
		// get all dependencies for this asset
		strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
		atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);
		CModelInfo::GetObjectAndDependencies(modelId, deps, gPopStreaming.GetResidentObjectList().GetElements(), gPopStreaming.GetResidentObjectList().GetCount());

		// sum up the additional memory cost this new model would incur
		u32 main = 0;
		u32 vram = 0;
		GetAdditionalModelCost(deps, main, vram);

#if !__FINAL
		if (PARAM_missioncreatordebug.Get())
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
			if (pModelInfo)
			{
				Displayf("[MISSION_CREATOR_ASSETS]: Adding %s (cost %d) with free memory %d", pModelInfo->GetModelName(), main + vram, ms_freeMemory);
			}
		}
#endif

#if 0 // allow script to over-allocate
		// not enough space in the budget
		if ((s32)(main + vram) > ms_freeMemory)
			return false;
#endif

		// add cost to our tracking
		AddAdditionalModelCost(deps);

		return true;
	}

    return false;
}

void MissionCreatorAssets::RemoveModel(fwModelId modelId)
{
	if (!ms_data)
		return;

	if (modelId.IsValid())
	{
		// get all dependencies for this asset
		strIndex backingStore[STREAMING_MAX_DEPENDENCIES];
		atUserArray<strIndex> deps(backingStore, STREAMING_MAX_DEPENDENCIES);
		CModelInfo::GetObjectAndDependencies(modelId, deps, gPopStreaming.GetResidentObjectList().GetElements(), gPopStreaming.GetResidentObjectList().GetCount());

        // remove cost from our tracking
        RemoveAdditionalModelCost(deps);

#if !__FINAL
		if (PARAM_missioncreatordebug.Get())
		{
			CBaseModelInfo* pModelInfo = CModelInfo::GetBaseModelInfo(modelId);
			if (pModelInfo)
			{
				Displayf("[MISSION_CREATOR_ASSETS]: Removing %s", pModelInfo->GetModelName());
			}
		}
#endif
	}
}

float MissionCreatorAssets::GetUsedBudget()
{
	if (!ms_data)
		return 0.f;

	float budget = (ms_data->memory > 0) ? (1.0f - ((float)ms_freeMemory / ms_data->memory)) : 0.0f;

#if !__FINAL
	if (PARAM_missioncreatordebug.Get())
		Displayf("[MISSION_CREATOR_ASSETS]: Budget requested %f", budget);
#endif

    return budget;
}

void MissionCreatorAssets::GetAdditionalModelCost(const atUserArray<strIndex>& deps, u32& main, u32& vram)
{
	for (s32 i = 0; i < deps.GetCount(); ++i)
	{
		bool found = false;
		for (s32 f = 0; f < ms_assets.GetCount(); ++f)
		{
			if (ms_assets[f].refCount > 0 && ms_assets[f].streamIdx == deps[i])
			{
				found = true;
				break;
			}
		}

		// if asset is already in our tracking list, skip it
		if (found)
			continue;

		main += (s32)strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
		vram += (s32)strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);
	}
}

void MissionCreatorAssets::AddAdditionalModelCost(const atUserArray<strIndex>& deps)
{
	for (s32 i = 0; i < deps.GetCount(); ++i)
	{
		bool found = false;
        s32 firstEmpty = -1;
		for (s32 f = 0; f < ms_assets.GetCount(); ++f)
		{
            if (firstEmpty == -1 && ms_assets[f].refCount == 0)
                firstEmpty = f;

			if (ms_assets[f].refCount > 0 && ms_assets[f].streamIdx == deps[i])
			{
                ms_assets[f].refCount++;
				found = true;
				break;
			}
		}

		// if asset is already in our tracking list, skip it
		if (found)
			continue;

        if (firstEmpty != -1)
		{
            ms_assets[firstEmpty].refCount = 1;
            ms_assets[firstEmpty].streamIdx = deps[i];
		}
		else
		{
            sAssetEntry newEntry;
            newEntry.refCount = 1;
            newEntry.streamIdx = deps[i];
            ms_assets.PushAndGrow(newEntry);
		}

		s32 virtSize = (s32)strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
		s32 physSize = (s32)strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);

#if __ASSERT
		bool bIsMetaData = (strStreamingEngine::GetInfo().GetModuleId(deps[i]) == g_fwMetaDataStore.GetStreamingModuleId());
		bool bIsDummy = strStreamingEngine::GetInfo().GetStreamingInfoRef(deps[i]).IsFlagSet(STRFLAG_INTERNAL_DUMMY);

		scriptAssertf(bIsDummy || bIsMetaData || ( (virtSize + physSize) > 0), "%s, requested resource size is 0, are we getting resource size from unloaded rpf? Please load the model '%s' first.", 
			CTheScripts::GetCurrentScriptNameAndProgramCounter(), 
			strStreamingEngine::GetInfo().GetObjectName(deps[i]));
#endif	//	__ASSERT
		
		ms_freeMemory -= virtSize;
		ms_freeMemory -= physSize;

#if 0 // allow script to over-allocate
		Assertf(ms_freeMemory >= 0, "MissionCreatorAssets::AddAdditionalModelCost: Memory tracking went wrong! memory %d - %d", ms_freeMemory, strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]));
#endif
	}
}

void MissionCreatorAssets::RemoveAdditionalModelCost(const atUserArray<strIndex>& deps)
{
	for (s32 i = 0; i < deps.GetCount(); ++i)
	{
		for (s32 f = 0; f < ms_assets.GetCount(); ++f)
		{
            if (ms_assets[f].streamIdx == deps[i])
			{
				if (ms_assets[f].refCount > 0)
				{
					ms_assets[f].refCount--;

					if (ms_assets[f].refCount == 0)
					{
						ms_freeMemory += (s32)strStreamingEngine::GetInfo().GetObjectVirtualSize(deps[i]);
						ms_freeMemory += (s32)strStreamingEngine::GetInfo().GetObjectPhysicalSize(deps[i]);
					}
				}
			}
		}
	}

    Assert(ms_data);
	Assertf(ms_freeMemory <= (s32)ms_data->memory, "MissionCreatorAssets::RemoveAdditionalModelCost: Memory tracking went wrong! memory %d", ms_freeMemory);
}