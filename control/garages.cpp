
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    garages.cpp
// PURPOSE : Logic for garages, bomb shops, respray places etc
// AUTHOR :  Obbe.
// CREATED : 5/10/00
//
/////////////////////////////////////////////////////////////////////////////////

// Framework Headers
#include "grcore/debugdraw.h"
#include "fwmaths/random.h"
#include "fwmaths/vector.h"
#include "fwmaths/angle.h"

// Game headers
#include "control/garages.h"

#include "camera/CamInterface.h"
#include "camera/viewports/Viewport.h"
#include "control/gamelogic.h"
#include "control/GarageEditor.h"
#include "control/replay/replay.h"
#include "core/game.h"
#include "cutscene/CutSceneManagerNew.h"
#include "frontend/hud_colour.h"
#include "Frontend/MobilePhone.h"
#include "game/clock.h"
#include "game/modelindices.h"
#include "Stats/StatsMgr.h"
#include "modelInfo/vehiclemodelinfo.h"
#include "network/Events/NetworkEventTypes.h"
#include "network/NetworkInterface.h"
#include "network/players/NetworkPlayerMgr.h"
#include "objects/door.h"
#include "objects/DummyObject.h"
#include "objects/object.h"
#include "peds/ped_channel.h"
#include "peds/pedIntelligence.h"
#include "peds/ped.h"
#include "peds/popcycle.h"
#include "scene/world/gameWorld.h"
#include "streaming/streaming.h"
#include "fwsys/timer.h"
#include "system/pad.h"
#include "text/messages.h"
#include "vehicles/automobile.h"
#include "vehicles/bike.h"
#include "vehicles/vehicleFactory.h"
#include "vehicles/heli.h"
#include "vfx/Systems/VfxVehicle.h"
#include "audioengine\controller.h"

#include "frontend\MobilePhone.h"
#include "Stats\StatsInterface.h"

#define DOORSPEEDOPEN  (0.011f)
#define DOORSPEEDCLOSE (0.013f)

#define MARGIN_FOR_CAMERA 			(0.5f)	// Distance for the camera to stay out of the actual zone
#define MARGIN_FOR_CAMERA_NARROW 	(0.5f)	// Same for import export garages. Smaller range since player can go all the wat round

AI_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

atArray<CGarage> CGarages::aGarages;
bool			CGarages::RespraysAreFree;				// resprays can be made free during missions
bool			CGarages::NoResprays;					// resprays are disabled
bool			CGarages::ResprayHappened;				// A respray (in any garage) has happened. For the script to test
CGarage*		CGarages::pGarageCamShouldBeOutside;	// Camera should be outside since stuff is going on
s32 			CGarages::LastGaragePlayerWasIn;		// the last garage any part of the player's vehicle was in
s32				CGarages::NumSafeHousesRegistered;		// How many safehouses have been registered on the map
u32				CGarages::timeForNextCollisionTest;		// when to trigger a new collision test for spawning vehicles

#if __BANK
bool			CGarages::bDebugDisplayGarages = false;	// Render a cube for the garage activation area
bool			CGarages::bAlternateOccupantSearch = true; // Loop over vehicles and peds directly when checking garage occupancy
#endif // __BANK


CStoredVehicle	CGarages::aCarsInSafeHouse[NUM_SAFE_HOUSES][MAX_NUM_STORED_CARS_IN_SAFEHOUSE];


#define RESPRAYCOST 	(100)
#define BOMBSCOST 		(500)

#if !__FINAL
bool	bPrintNearestObject = false;
#endif

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ClearAll
// PURPOSE :  Removes all cars from safehouses
/////////////////////////////////////////////////////////////////////////////////
void CGarages::ClearAll()
{
	for (s32 C = 0; C < MAX_NUM_STORED_CARS_IN_SAFEHOUSE; C++)
	{
		for (s32 C2 = 0; C2 < NUM_SAFE_HOUSES; C2++)
		{
			aCarsInSafeHouse[C2][C].Clear();
		}
	}
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitLevel
// PURPOSE :  Puts the garages back to their original state.
/////////////////////////////////////////////////////////////////////////////////
void CGarages::Init(unsigned initMode)
{
	if (initMode == INIT_BEFORE_MAP_LOADED)
	{
	}
	else if (initMode == INIT_SESSION)
	{
		RespraysAreFree = false;
		NoResprays = false;
		ResprayHappened = false;
		LastGaragePlayerWasIn = -1;

		NumSafeHousesRegistered = 0;
		timeForNextCollisionTest = 0;

		// Clear the cars in the safehouses
		ClearAll();

		Load();
	}
}

//
// name:		ShutdownLevel
// description:	Shut's down the garages system
void CGarages::Shutdown(unsigned shutdownMode)
{
	if( shutdownMode == SHUTDOWN_SESSION )
	{
		ClearAll();
	}

	CGarages::aGarages.Reset();
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Load
// PURPOSE :  Loads garage data from meta data
/////////////////////////////////////////////////////////////////////////////////

void CGarages::Load()
{
	CGarageInitDataList garageData;
	PARSER.LoadObject(GARAGE_META_FILE, GARAGE_META_FILE_EXT, garageData);

	for (s32 i = 0; i < garageData.garages.GetCount(); ++i)
	{
		const CGarageInitData& g = garageData.garages[i];
		AddOne(g.m_boxes, g.type, g.name, g.owner, g.permittedVehicleType, g.startedWithVehicleSavingEnabled, g.isMPGarage, g.isEnclosed, g.InteriorBoxIDX, g.ExteriorBoxIDX);
	}
}

#if __BANK
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Save
// PURPOSE :  Saves garage data to meta data
/////////////////////////////////////////////////////////////////////////////////

void CGarages::Save()
{
	CGarageInitDataList garageData;
	garageData.garages.Reserve(aGarages.GetCount());
	for (s32 i = 0; i < aGarages.GetCount(); ++i)
	{
		CGarageInitData& g = garageData.garages.Append();
		CGarage &garage = aGarages[i];
		for (int j = 0; j < garage.m_boxes.GetCount(); ++j)
		{
			g.m_boxes.PushAndGrow(garage.m_boxes[j]);
		}
		g.name = aGarages[i].name;
		g.owner = aGarages[i].owner;
		g.type = aGarages[i].type;
		g.permittedVehicleType = aGarages[i].m_permittedVehicleType;
		g.startedWithVehicleSavingEnabled = aGarages[i].Flags.bSavingVehiclesWasEnabledBeforeSave;

		// Error check box IDX's for MP garages
		Assertf( !(garage.m_IsMPGarage == true && garage.m_ExteriorBoxIDX == -1), "ERROR: MP flagged garage %s doesn't have and exterior box defined", garage.name.GetCStr() );
		Assertf( !(garage.m_IsMPGarage == true && garage.m_InteriorBoxIDX == -1), "ERROR: MP flagged garage %s doesn't have and interior box defined", garage.name.GetCStr() );

		g.isEnclosed = garage.m_IsEnclosed;
		g.isMPGarage = garage.m_IsMPGarage;
		g.ExteriorBoxIDX = garage.m_ExteriorBoxIDX;
		g.InteriorBoxIDX = garage.m_InteriorBoxIDX;
	}

	PARSER.SaveObject(GARAGE_META_FILE, GARAGE_META_FILE_EXT, &garageData);
}
#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Updates the garages.
/////////////////////////////////////////////////////////////////////////////////

void CGarages::Update(void)
{
#if __BANK
	CGarageEditor::UpdateDebug();
#endif // __BANK

	s32	Index;
	static	u32	GarageToBeTidied = 0;
	static	u32 GarageToBeOccupancyChecked = 0;

#if GTA_REPLAY
	if(CReplayMgr::IsEditModeActive())
		return;
#endif
	if (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning())
		return;

	if (!aGarages.GetCount())
		return;

	pGarageCamShouldBeOutside = NULL;
	CPhoneMgr::ClearRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_GARAGES);

#define GARAGES_TO_BE_OCCUPANCY_CHECKED (1)
	for(u32 i = 0; i < GARAGES_TO_BE_OCCUPANCY_CHECKED; i++)
	{
		GarageToBeOccupancyChecked++;
		if(GarageToBeOccupancyChecked >= aGarages.GetCount())
			GarageToBeOccupancyChecked = 0;

		if(aGarages[GarageToBeOccupancyChecked].type != GARAGE_NONE)
		{
#if __BANK
			aGarages[GarageToBeOccupancyChecked].UpdateOccupiedBoxes(bAlternateOccupantSearch);
#else
			aGarages[GarageToBeOccupancyChecked].UpdateOccupiedBoxes(true);
#endif
		}
	}

	for (Index = 0; Index < aGarages.GetCount(); Index++)
	{
		if (aGarages[Index].type != GARAGE_NONE)
		{
			aGarages[Index].Update(Index);
		}
	}

	// Once in a while we tidy up a garage. (Remove wreck etc)
	if (!aGarages[GarageToBeTidied].Flags.bDisableTidyingUp && (fwTimer::GetSystemFrameCount() & 15) == 12)
	{
		GarageToBeTidied++;
		if (GarageToBeTidied >= aGarages.GetCount()) GarageToBeTidied = 0;
		
		if (aGarages[GarageToBeTidied].type != GARAGE_NONE)
		{
			// Make sure the camera is not too close
			if ( ABS(camInterface::GetPos().x - aGarages[GarageToBeTidied].MinX) > 40.0f ||
				 ABS(camInterface::GetPos().y - aGarages[GarageToBeTidied].MinY) > 40.0f)
			{
				aGarages[GarageToBeTidied].TidyUpGarage();
			}
			else
			{
				aGarages[GarageToBeTidied].TidyUpGarageClose();
			}
		}
	}

#if __BANK
	{
		// Debug thing:
		// If this flag is set we will go through the objects and print the one closest to the cam.
		// Handy for finding doors.
		if (bPrintNearestObject)
		{
			CEntity	*pResult[128], *pClosest = NULL;
			s32	Num, Loop;
			float	SmallestDist = 99999.9f, Dist;
		
			CGameWorld::FindObjectsInRange(camInterface::GetPos(), 20.0f, false, &Num, 128, pResult, true, false, false, true, true);

			for (Loop = 0; Loop < Num; Loop++)
			{
				Dist = (camInterface::GetPos() - VEC3V_TO_VECTOR3(pResult[Loop]->GetTransform().GetPosition())).Mag();
				if (Dist < SmallestDist)
				{
					SmallestDist = Dist;
					pClosest = pResult[Loop];
				}
			}
			if (pClosest)
			{
				Displayf("MI:%d %s Coors:%f %f\n", pClosest->GetModelIndex(), pClosest->GetModelName(), pClosest->GetTransform().GetPosition().GetXf(), pClosest->GetTransform().GetPosition().GetYf());

				// A little test thing for the algorithm that works out the projection on z of the bb.
				Vector3	P1, P2, P3, P4;
				pClosest->CalculateBBProjection(&P1, &P2, &P3, &P4);
				
				grcDebugDraw::Line(P1, P2, Color32(0xff, 0x00, 0x00, 0xff), Color32(0xff, 0xff, 0xff, 0xff));
				grcDebugDraw::Line(P2, P3, Color32(0x00, 0xff, 0x00, 0xff), Color32(0xff, 0xff, 0xff, 0xff));
				grcDebugDraw::Line(P3, P4, Color32(0x00, 0x00, 0xff, 0xff), Color32(0xff, 0xff, 0xff, 0xff));
				grcDebugDraw::Line(P4, P1, Color32(0xff, 0xff, 0x00, 0xff), Color32(0xff, 0xff, 0xff, 0xff));

			}
		}
	}
#endif
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddOne
// PURPOSE :  Finds a slot for and generates a new garage.
/////////////////////////////////////////////////////////////////////////////////
s32 CGarages::AddOne(float BaseX, float BaseY, float BaseZ, float Delta1X, float Delta1Y, float Delta2X, float Delta2Y, float CeilingZ, GarageType type, atHashString name, atHashString owner, bool isMpGarage, bool isEnclosed, s8 interiorBoxIDX, s8 exteriorBoxIDX)
{
	atArray<CGarage::Box> box;
	CGarage::Box &b = box.Grow();

	Assertf(box.GetCount() <= (sizeof(BoxFlags)<<3), "Increase size of BoxFlags");

	b.BaseX = BaseX;
	b.BaseY = BaseY;
	b.BaseZ = BaseZ;

	b.Delta1X = Delta1X;
	b.Delta1Y = Delta1Y;

	b.Delta2X = Delta2X;
	b.Delta2Y = Delta2Y;

	b.CeilingZ = CeilingZ;
	b.useLineIntersection = false;

	return AddOne(box, type, name, owner, VEHICLE_TYPE_NONE, true, isMpGarage, isEnclosed, interiorBoxIDX, exteriorBoxIDX);
}

s32 CGarages::AddOne(const atArray<CGarage::Box> &boxes, GarageType type, atHashString name, atHashString owner, int permittedVehicleType, bool vehicleSavingEnabled, bool isMpGarage, bool isEnclosed, s8 interiorBoxIDX, s8 exteriorBoxIDX)
{
	CGarage &g = aGarages.Grow();
	
	for (int i = 0; i < boxes.GetCount(); ++i)
	{
		g.m_boxes.Grow();
		g.m_boxesOccupiedForPlayers.Grow();
		g.UpdateSize(i, boxes[i]);
	}

	g.TimeOfNextEvent = ~(u32)0;		// Really large number so the garages won't skip past stuff.

	g.pCarToCollect = NULL;
	g.name = name;
	g.owner = owner;
	g.m_permittedVehicleType = VehicleType(permittedVehicleType);
	g.Flags.bSavingVehiclesWasEnabledBeforeSave = vehicleSavingEnabled;
	g.Flags.bSavingVehiclesEnabled = vehicleSavingEnabled;

	g.m_IsEnclosed = isEnclosed;
	g.m_IsMPGarage = isMpGarage;
	g.m_InteriorBoxIDX = interiorBoxIDX;
	g.m_ExteriorBoxIDX = exteriorBoxIDX;

#if __ASSERT
	for (u32 garage_loop = 0; garage_loop < aGarages.GetCount() - 1; garage_loop++)
	{
		Assertf(g.name.GetHash() != aGarages[garage_loop].name.GetHash(), "CGarages::AddOne - more than one garage called %s exists", name.GetCStr());
	}
#endif // __ASSERT


	g.m_DoorSlideTimeOutTimer = 0;

	g.type = type;
	g.originalType = type;

	g.state = GSTATE_CLOSED;

	// Some are open to begin with. Some are closed.
	switch (g.type)
	{
		// These ones are closed to begin with
	case GARAGE_NONE:
	case GARAGE_MISSION:
	case GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE:
	case GARAGE_AMBIENT_PARKING:
	case GARAGE_SAFEHOUSE_PARKING_ZONE:
		break;
	case GARAGE_RESPRAY:
		// These ones are open to begin with
		g.state = GSTATE_OPEN;
		break;
	default:
		Assertf(0, "CGarages::AddOne - unknown garage type");
		break;
	}


	// The hideouts are now assigned by the game. This way the artists don't have to worry about type
	// clashes as they set the type for hideout garages.
	if (type == GARAGE_SAFEHOUSE_PARKING_ZONE)
	{
		g.safeHouseIndex = (s8)NumSafeHousesRegistered;
		NumSafeHousesRegistered++;
		Assert(NumSafeHousesRegistered <= NUM_SAFE_HOUSES);
	}
	else
	{
		g.safeHouseIndex = -1;
	}

	g.Flags.bClosingEmpty = true;
	g.Flags.bLeaveCameraAlone = false;
	g.Flags.bDoorSlidingPreviousFrame = false;
	g.Flags.bDoorSlidingThisFrame = false;
	g.Flags.bSafehouseGarageWasOpenBeforeSave = false;
	g.Flags.bDisableTidyingUp = false;

	g.m_pDoorObject = NULL;
	g.m_oldDoorOpenness = 0.0f;
	g.m_oldDoorFrameCount = 0;

	return aGarages.GetCount();
}

inline bool DoorIsInUseByScript(CDoor *pDoor)
{
	if (!Verifyf(pDoor, "pDoor is NULL"))
	{
		return false;
	}

	CDoorSystemData *pData = pDoor->GetDoorSystemData();

	// If script has locked the door or it has an open ratio thats not 0 its in used by script
	if (pData && (pData->GetLocked() || pData->GetTargetRatio() != 0.0f))
	{
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SlideDoorOpen
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////


bool CGarage::SlideDoorOpen()
{
	// If this door is controlled by script its safe to move it
	if (DoorIsInUseByScript(m_pDoorObject))
	{
		return true;
	}

	// This deals with a timeout. After 5 seconds of trying to open we return true so that the player doesn't get stuck.
	bool	bForceDone = false;
	Flags.bDoorSlidingThisFrame = true;
	if (!Flags.bDoorSlidingPreviousFrame)
	{
		m_DoorSlideTimeOutTimer = fwTimer::GetTimeInMilliseconds();
	}
	if (fwTimer::GetTimeInMilliseconds() > m_DoorSlideTimeOutTimer + 5000)
	{
		bForceDone = true;
		Flags.bDoorSlidingThisFrame = false;
	}

	if (m_pDoorObject && m_pDoorObject->GetCurrentPhysicsInst())
	{
		m_pDoorObject->SetFixedPhysics(false, false);
		m_pDoorObject->SetTargetDoorRatio(1.0f, false);
		float	doorOpenness = FindDoorOpenness();
		if (bForceDone || doorOpenness > 0.98f || (doorOpenness > 0.5f && doorOpenness <= m_oldDoorOpenness && m_oldDoorFrameCount == (fwTimer::GetSystemFrameCount()-1) ))
		{
			m_pDoorObject->SetTargetDoorRatio(1.0f, true);
			m_pDoorObject->SetFixedPhysics(true, false);
			return true;
		}
		m_oldDoorOpenness = doorOpenness;
		m_oldDoorFrameCount = fwTimer::GetSystemFrameCount();
	}

//	if (bForceDone)
//	{
//		if (m_pDoorObject && m_pDoorObject->GetCurrentPhysicsInst())	// Need to try and force door open as player can get stuck inside.
//		{
//			m_pDoorObject->SetTargetDoorRatio(1.0f, true);
//			m_pDoorObject->SetFixedPhysics(true, false);
//		}
//
//		return true;
//	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SlideDoorClosed
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::SlideDoorClosed()
{
	// If this door is controlled by script its safe to move it
	if (DoorIsInUseByScript(m_pDoorObject))
	{
		return true;
	}

	// This deals with a timeout. After 5 seconds of trying to close we return true so that the player doesn't get stuck.
	bool	bForceDone = false;
	Flags.bDoorSlidingThisFrame = true;
	if (!Flags.bDoorSlidingPreviousFrame)
	{
		m_DoorSlideTimeOutTimer = fwTimer::GetTimeInMilliseconds();
	}
	if (fwTimer::GetTimeInMilliseconds() > m_DoorSlideTimeOutTimer + 5000)
	{
		bForceDone = true;
		Flags.bDoorSlidingThisFrame = false;
	}

	if (m_pDoorObject && m_pDoorObject->GetCurrentPhysicsInst())
	{
		m_pDoorObject->SetFixedPhysics(false, false);
		m_pDoorObject->SetTargetDoorRatio(0.0001f, false);

		float	doorOpenness = FindDoorOpenness();

			// Done if door shut or doesn't progress anymore.
		if (bForceDone || doorOpenness < 0.02f || (doorOpenness < 0.5f && doorOpenness >= m_oldDoorOpenness && m_oldDoorFrameCount == (fwTimer::GetSystemFrameCount()-1)))
		{
			m_pDoorObject->SetFixedPhysics(true, false);
			return true;
		}
		m_oldDoorOpenness = doorOpenness;
		m_oldDoorFrameCount = fwTimer::GetSystemFrameCount();
	}

	if (bForceDone)
	{
		return true;
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindDoorOpenness
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

float CGarage::FindDoorOpenness()
{
	Assert(m_pDoorObject);
	Vector3 vecDoorForward(VEC3V_TO_VECTOR3(m_pDoorObject->GetTransform().GetB()));
	float fCurrentHeading = rage::Atan2f(vecDoorForward.z, vecDoorForward.XYMag()) / HALF_PI;
	return fabs(fCurrentHeading);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindGarageIndex
// PURPOSE :  This function finds the garage index for this string.
/////////////////////////////////////////////////////////////////////////////////

s32 CGarages::FindGarageIndexFromNameHash(u32 NameHash)
{
	for (u32 C = 0; C < aGarages.GetCount(); C++)
	{
		if (NameHash == aGarages[C].name.GetHash())
		{
			return C;
		}

	}
	Assertf(0, "Garage wasn't found");
	return -1;
}

s32 CGarages::GetNumGarages() 
{
	return aGarages.GetCount();
}

s32 CGarages::FindGarageIndex(char *pName)
{
	u32	garageHash = atStringHash(pName);
	return FindGarageIndexFromNameHash(garageHash);
}

s32 CGarages::FindGarageWithSafeHouseIndex(s32 safeHouseIdx)
{
	for (u32 C = 0; C < aGarages.GetCount(); C++)
	{
		if (aGarages[C].type == GARAGE_SAFEHOUSE_PARKING_ZONE)
		{
			if (aGarages[C].safeHouseIndex == safeHouseIdx)
			{
				return C;
			}
		}
	}

	Assertf(0, "CGarages::FindGarageWithSafeHouseIndex - Garage with index %d wasn't found", safeHouseIdx);
	return -1;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ChangeGarageType
// PURPOSE :  Change the type of this garage. Mostly used for bomb shops. They
//			  can change depending on the mission.
/////////////////////////////////////////////////////////////////////////////////

void CGarages::ChangeGarageType(s32 NumGarage, s32 NewType)
{
	CGarage	*pGarage;
	
	pGarage = &aGarages[NumGarage];

	if (Verifyf(pGarage->type != GARAGE_SAFEHOUSE_PARKING_ZONE, "CGarages::ChangeGarageType - it's not safe to change the type of a garage from GARAGE_SAFEHOUSE_PARKING_ZONE. This will cause problems when saving the game"))
	{
		if (Verifyf(NewType != GARAGE_SAFEHOUSE_PARKING_ZONE, "CGarages::ChangeGarageType - it's probably not safe to change the type of a garage to GARAGE_SAFEHOUSE_PARKING_ZONE. This could cause problems when saving the game"))
		{
			pGarage->type = (GarageType)(NewType);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PrintMessage
// PURPOSE :  Prints a garage related message
/////////////////////////////////////////////////////////////////////////////////

void CGarage::PrintMessage(const char *pTextLabel)
{
	char *pText = TheText.Get(pTextLabel);
	s32 TextBlock = TheText.GetBlockContainingLastReturnedString();
	CMessages::AddMessage(pText, TextBlock, 
		4000, true, true, PREVIOUS_BRIEF_NO_OVERRIDE, NULL, 0, NULL, 0, false);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Update
// PURPOSE :  Updates one garage
/////////////////////////////////////////////////////////////////////////////////
void CGarage::Update(s32 MyIndex)
{
	u8	NewCol1, NewCol2, NewCol3, NewCol4, NewCol5, NewCol6;
	bool	bChargeMoney;
	bool	bChangedColour = false;
	bool	bPlayerWasWanted;
	float	DistanceSqr;
	CPed * pPlayerPed = FindPlayerPed();
	if (!AssertVerify(pPlayerPed))
		return;
	CPlayerInfo* pPlayerInfo = pPlayerPed->GetPlayerInfo();

	Flags.bDoorSlidingThisFrame = false;

#if !__FINAL
	if (pCarToCollect)
	{
		AssertEntityPointerValid(pCarToCollect);
	}
#endif	
	//do garage camera thingy first
 	switch (state)
	{
		case GSTATE_CLOSED: 
		case GSTATE_OPEN:
		case GSTATE_CLOSING:
		case GSTATE_OPENING: 
		case GSTATE_OPENEDWAITINGTOBEREACTIVATED:
		case GSTATE_MISSIONCOMPLETED:
		if (FindPlayerPed())
		{
			//sometimes the level designers don't want the camera to go out
			//so check this first
			if  (Flags.bLeaveCameraAlone==false)
			{
				if (type != GARAGE_SAFEHOUSE_PARKING_ZONE)	// This shouldn't be needed as the parking zones should have been set up by the artists with bLeaveCameraAlone but it's too late to get this fixed properly.
				{

					CVehicle* PlayerVehicle=NULL;
					PlayerVehicle=CGameWorld::FindLocalPlayerVehicle();
				
					if (PlayerVehicle)
					{
						// If we've already been in this garage we set the margin a bit bigger. This way we avoid the first person glitch whilst walking into a garage backwards.
						CEntity *pEntityToTest = FindPlayerPed();

						if (IsEntityEntirelyInside3D(pEntityToTest, 1.0f)==true)
						{
							CGarages::pGarageCamShouldBeOutside = this;
							CPhoneMgr::SetRemoveFlags(CPhoneMgr::WANTS_PHONE_REMOVED_GARAGES);	// Make player put phone away
						}
					}
				}
			}	
		}
		break;
	default:
		break;
	}


	static u32 policeBackoffTime;

	switch(type)
	{
			///////////////////////// GARAGE_RESPRAY ////////////////////////
		case GARAGE_RESPRAY:
			switch (GetState())
			{
				case GSTATE_OPEN:

					if (!CGarages::NoResprays)
					{
						if (IsStaticPlayerCarEntirelyInside() && !AnyNetworkPlayersOnFootInGarage(1.0f) &&
							!pPlayerPed->GetPedIntelligence()->GetQueriableInterface()->IsTaskCurrentlyRunning(CTaskTypes::TASK_COMPLEX_USE_MOBILE_PHONE, true) &&
							!pPlayerInfo->AreControlsDisabled())
						{		// Activate the dude
							if (CGarages::IsCarSprayable((CAutomobile *)CGameWorld::FindLocalPlayerVehicle()))	// Is car the right type ?
							{
								StatId stat;
								if(NetworkInterface::IsInFreeMode())
								{
									// This is dealt by script in mp
								}
								else
								{
									stat = STAT_TOTAL_CASH.GetStatId();
								}

								if ((StatsInterface::IsKeyValid(stat) && StatsInterface::GetIntStat(stat) >= RESPRAYCOST) || CGarages::RespraysAreFree || NetworkInterface::IsGameInProgress())
								{
									Vector3 Crs;
									CalcCentrePointOfGarage(Crs);
									if( pPlayerInfo->GetWanted().m_WantedLevel >= WANTED_LEVEL1 && CWanted::WorkOutPolicePresence(VECTOR3_TO_VEC3V(Crs), 35.0f) > 0)
									{
										PrintMessage("GA_POL");	//	// The cops saw you enter.

										SetState(GSTATE_OPENEDWAITINGTOBEREACTIVATED);
										policeBackoffTime = fwTimer::GetTimeInMilliseconds() + 8000;
									}
									else
									{
										SetState(GSTATE_CLOSING);
											// Disable controls
										pPlayerInfo->DisableControlsGarages();
									}
								}
								else
								{		// We don't have enough money
									PrintMessage("GA_3");		// Come back when you've got the cash.

									SetState(GSTATE_OPENEDWAITINGTOBEREACTIVATED);
									policeBackoffTime = fwTimer::GetTimeInMilliseconds() + 10000;
								}
							}
							else
							{
								PrintMessage("GA_1");		// They're not going to respray THAT.

								SetState(GSTATE_OPENEDWAITINGTOBEREACTIVATED);
								policeBackoffTime = fwTimer::GetTimeInMilliseconds() + 10000;

							}
							
							CGameWorld::FindLocalPlayerWanted()->m_PoliceBackOffGarage = true;
							CGarages::LastGaragePlayerWasIn = MyIndex;
						}
						else if (!IsPlayerOutsideGarage(FindPlayerPed()))
						{
							CGameWorld::FindLocalPlayerWanted()->m_PoliceBackOffGarage = true;
							CGarages::LastGaragePlayerWasIn = MyIndex;
						}
						else if (MyIndex == CGarages::LastGaragePlayerWasIn)
						{
							CGameWorld::FindLocalPlayerWanted()->m_PoliceBackOffGarage = false;
						}
							
					}
					break;
				case GSTATE_CLOSING:
					if (CGameWorld::FindLocalPlayerVehicle()) ThrowCarsNearDoorOutOfGarage(CGameWorld::FindLocalPlayerVehicle());
					if (SlideDoorClosed())
					{
						SetState(GSTATE_CLOSED);

						if (NetworkInterface::IsGameInProgress())
						{
							TimeOfNextEvent = fwTimer::GetTimeInMilliseconds() + 400;
							camInterface::FadeOut(400);
						}
						else
						{
							TimeOfNextEvent = fwTimer::GetTimeInMilliseconds() + 2000;
							camInterface::FadeOut(2000);
						}

						StatId stat = StatsInterface::GetStatsModelHashId("KILLS_SINCE_LAST_CHECKPOINT");
						if (StatsInterface::IsKeyValid(stat))
						{
							StatsInterface::IncrementStat(StatsInterface::GetStatsModelHashId("TOTAL_LEGITIMATE_KILLS"), (float)StatsInterface::GetIntStat(stat));
							StatsInterface::SetStatData(stat, 0);
						}
					}
					// postpone explosion
					if (CGameWorld::FindLocalPlayerVehicle())
					{
						CGameWorld::FindLocalPlayerVehicle()->m_nVehicleFlags.bDisableParticles = true;
					}
					break;
				case GSTATE_CLOSED:
					if (!CGarages::NoResprays)
					{
						{
							if (fwTimer::GetTimeInMilliseconds() > TimeOfNextEvent && !camInterface::IsFading())
							{
								if (!NetworkInterface::IsGameInProgress())
								{
									CClock::PassTime(3 * 60, false);
									CHeli::RemovePoliceHelisUponDeathArrest();
									CGameWorld::ClearExcitingStuffFromArea(pPlayerInfo->GetPos(), 4000.0f, TRUE, true, true, false, m_pDoorObject, true, true);
								}

								if (NetworkInterface::IsGameInProgress())
								{
									camInterface::FadeIn(400);
								}
								else
								{
									camInterface::FadeIn(400);
								}

								SetState(GSTATE_OPENING);

								bChargeMoney = false;		// Should we charge money ?
								
								// Reset wanted state
								bPlayerWasWanted = CGameWorld::FindLocalPlayerWanted()->m_WantedLevel != WANTED_CLEAN;
								
								if (bPlayerWasWanted)
								{
									bChargeMoney = true;

									CPed * pPlayerPed = FindPlayerPed();
									pPlayerPed->GetPlayerWanted()->SetWantedLevel(VEC3V_TO_VECTOR3(pPlayerPed->GetTransform().GetPosition()), WANTED_CLEAN, 0, WL_REMOVING);
								}
								
                                CVehicle *pPlayerVehicle = CGameWorld::FindLocalPlayerVehicle();

                                if(pPlayerVehicle && !pPlayerVehicle->IsNetworkClone())
                                {
								    // Fix car
								    if (pPlayerVehicle && (pPlayerVehicle->InheritsFromAutomobile() || pPlayerVehicle->InheritsFromBike()) &&
									    pPlayerVehicle->GetStatus() != STATUS_WRECKED)		// Don't fix vehicles that are completely wrecked.
								    {
									    // If the car has full health and there is no wanted level we will
									    // just change the colour and not charge any money
									    if (pPlayerVehicle->GetHealth() < 970)
									    {
										    bChargeMoney = true;
									    }
    								
									    pPlayerVehicle->SetHealth(rage::Max(CREATED_VEHICLE_HEALTH, pPlayerVehicle->GetHealth()));
										    // Also make sure the car won't explode
									    if (pPlayerVehicle->InheritsFromAutomobile())
									    {
										    ((CAutomobile *)pPlayerVehicle)->Fix();
										    // AF: this is not needed anymore
										    //CGameWorld::FindLocalPlayerVehicle()->SetCarDoorLocks(CARLOCK_UNLOCKED);	// Without this the player could knock the doors of a car and fix it->unjackable
									    }
									    else
									    {
										    Assert(pPlayerVehicle->InheritsFromBike());
										    ((CBike *)pPlayerVehicle)->Fix();
										    // AF: this is not needed anymore
										    //CGameWorld::FindLocalPlayerVehicle()->SetCarDoorLocks(CARLOCK_UNLOCKED);	// Without this the player could knock the doors of a car and fix it->unjackable
									    }

									    // If the car is upside down we put it right side up
									    if (pPlayerVehicle->GetTransform().GetC().GetZf() < 0.0f)
									    {	// Simply invert the side and up vectors
											Matrix34 mat = MAT34V_TO_MATRIX34(pPlayerVehicle->GetMatrix());

											mat.c = -mat.c;
										    //mat.c.x = -mat.c.x;	// Up vector
										    //mat.c.y = -mat.c.y;
										    //mat.c.z = -mat.c.z;
											
											mat.a = -mat.a;
										    //mat.a.x = -mat.a.x;	// Right vector
										    //mat.a.y = -mat.a.y;
										    //mat.a.z = -mat.a.z;
										    // This should work since the centerpoint is below the middle. (Car shouldn't get stuck in ground)		

											pPlayerVehicle->SetMatrix(mat, false, false, false);
									    }
    									
									    // Change colour of the car too
    									
									    bChangedColour = false;
									    if( !pPlayerVehicle->m_nVehicleFlags.bShouldNotChangeColour )
									    {
										    CVehicleModelInfo* pVehicleModelInfo = pPlayerVehicle->GetVehicleModelInfo();
    //										pVehicleModelInfo->ChooseVehicleColourFancy(pPlayerVehicle, NewCol1, NewCol2, NewCol3, NewCol4);
											    // Instead of the fancy function we always pick the next colour so that the colour always changes. (if more than 1 colour set up)
										    pVehicleModelInfo->GetNextVehicleColour(pPlayerVehicle, NewCol1, NewCol2, NewCol3, NewCol4, NewCol5, NewCol6);

										    if( pPlayerVehicle->GetBodyColour1() != NewCol1 ||
											    pPlayerVehicle->GetBodyColour2() != NewCol2 ||
											    pPlayerVehicle->GetBodyColour3() != NewCol3 ||
											    pPlayerVehicle->GetBodyColour4() != NewCol4 ||
												pPlayerVehicle->GetBodyColour5() != NewCol5 ||
												pPlayerVehicle->GetBodyColour6() != NewCol6 )
											    bChangedColour = true;
    											
										    pPlayerVehicle->SetBodyColour1(NewCol1);
										    pPlayerVehicle->SetBodyColour2(NewCol2);
										    pPlayerVehicle->SetBodyColour3(NewCol3);
										    pPlayerVehicle->SetBodyColour4(NewCol4);
										    pPlayerVehicle->SetBodyColour5(NewCol5);
										    pPlayerVehicle->SetBodyColour6(NewCol6);
										    pPlayerVehicle->UpdateBodyColourRemapping();	// let shaders know, that body colours changed

										    // trigger the particle effect
										    if (m_pDoorObject)
										    {
											    Vector3 fxPos = VEC3V_TO_VECTOR3(m_pDoorObject->GetTransform().GetPosition());
											    fxPos.z = m_boxes[0].BaseZ; // Ignoring other boxes for now

											    Vector3 fxDir = VEC3V_TO_VECTOR3(m_pDoorObject->GetTransform().GetB());
											    fxDir.z = 0.0f;
											    fxDir.Normalize();

											    g_vfxVehicle.TriggerPtFxRespray(pPlayerVehicle, RCC_VEC3V(fxPos), RCC_VEC3V(fxDir));
										    }

									    }
									    pPlayerVehicle->m_fBodyDirtLevel = 0.0f;	// Give the car a wash.
									    pPlayerVehicle->m_nVehicleFlags.bDisableParticles = false;
								    }
									    // Take money away from player
								    if (CGarages::RespraysAreFree)
								    {
										PrintMessage("GA_22");

										CGarages::RespraysAreFree = false;		// Only one freebie
    //									CMessages::AddBigMessage( TheText.Get("GA_22"), -1, 4000, BIG_MESSAGE_4, true);
								    }
								    else
								    {
									    if (bChargeMoney)
									    {
											if(NetworkInterface::IsInFreeMode())
											{
												// this is dealt by script.
											}
											else
											{
												StatsInterface::DecrementStat(STAT_TOTAL_CASH.GetStatId(), RESPRAYCOST);
											}


											bool isTaxi = pPlayerVehicle && CVehicle::IsTaxiModelId(pPlayerVehicle->GetModelId());

											if (bPlayerWasWanted)
										    {
											    if (pPlayerVehicle && isTaxi)
											    {
													PrintMessage("GA_2T");
											    }
											    else
											    {
													PrintMessage("GA_2");
											    }
    //											CMessages::AddBigMessage( TheText.Get("GA_2"), -1, 4000, BIG_MESSAGE_4, true);
										    }
										    else
										    {
											    if (pPlayerVehicle && (isTaxi || pPlayerVehicle->GetModelIndex() == MI_CAR_FIRETRUCK) )
											    {
													PrintMessage("GA_XT"); // As good as new.
											    }
											    else
											    {
												    {
													    switch (fwRandom::GetRandomNumberInRange(0, 3))
													    {
														    case 0:
															    {
																	PrintMessage("GA_X1");
															    }
															    // CMessages::AddBigMessage( TheText.Get("GA_X1"), -1, 4000, BIG_MESSAGE_4, true);
															    break;
														    case 1:
															    {
																	PrintMessage("GA_X2");
															    }
															    // CMessages::AddBigMessage( TheText.Get("GA_X2"), -1, 4000, BIG_MESSAGE_4, true);
															    break;
														    case 2:
															    {
																	PrintMessage("GA_X3");
															    }
															    // CMessages::AddBigMessage( TheText.Get("GA_X3"), -1, 4000, BIG_MESSAGE_4, true);
															    break;
													    }
												    }
											    }
										    }
									    }
									    else
									    {
										    if (bChangedColour)
										    {
											    {
												    if (fwRandom::GetRandomNumber() & 1)
												    {
														PrintMessage("GA_15");

	    //												CMessages::AddBigMessage( TheText.Get("GA_15"), -1, 4000, BIG_MESSAGE_4, true);
												    }
												    else
												    {
														PrintMessage("GA_16");

	    //												CMessages::AddBigMessage( TheText.Get("GA_16"), -1, 4000, BIG_MESSAGE_4, true);
												    }
											    }
										    }
									    }
								    }
									CGarages::ResprayHappened = true;
    								
								    if (CGameWorld::FindLocalPlayerVehicle())
								    {
									    CGameWorld::FindLocalPlayerVehicle()->m_nVehicleFlags.bHasBeenResprayed = true;
								    }
                                }

							}
						}
					}
					break;
				case GSTATE_OPENING:
					if (SlideDoorOpen())
					{
						SetState(GSTATE_OPENEDWAITINGTOBEREACTIVATED);
						policeBackoffTime = fwTimer::GetTimeInMilliseconds() + 5000;
					}

					// Enable controls
					{
						{
							pPlayerInfo->EnableControlsGarages();

							// Also stop the cops chasing player. Unfair to get arrested now.
							CGameWorld::FindLocalPlayerWanted()->m_PoliceBackOffGarage = false;
						}
					}
							
					break;
				case GSTATE_OPENEDWAITINGTOBEREACTIVATED:

					if (fwTimer::GetTimeInMilliseconds() > policeBackoffTime)
					{
						CGameWorld::FindLocalPlayerWanted()->m_PoliceBackOffGarage = false;
					}

					if (IsPlayerOutsideGarage(FindPlayerPed()))
					{
						SetState(GSTATE_OPEN);
					}
					break;
				default:
					break;
			}
			break;

		// Mission garages only open if the player brings a specific car
		case GARAGE_MISSION:
			switch (state)
			{
				case GSTATE_OPEN:

					DistanceSqr = ( (CGameWorld::FindLocalPlayerCoors().x - (MinX + MaxX) * 0.5f) * (CGameWorld::FindLocalPlayerCoors().x - (MinX + MaxX) * 0.5f) +
									(CGameWorld::FindLocalPlayerCoors().y - (MinY + MaxY) * 0.5f) * (CGameWorld::FindLocalPlayerCoors().y - (MinY + MaxY) * 0.5f) );

					if (DistanceSqr > 30.0f * 30.0f)
					{
						if (!(fwTimer::GetSystemFrameCount() & 31))
						{
							if ( (!pCarToCollect) || (!IsEntityTouching3D(pCarToCollect)))	// Added just before Miami-PC
							{
								// Close the door now please.
								SetState(GSTATE_CLOSING);
								Flags.bClosingEmpty = true;
							}
						}
					}
					else
					{
						// Wait till the car is INside and the player is OUTside.
						// At this point we close the door.
						if (NetworkInterface::IsGameInProgress() || CGameWorld::FindLocalPlayerVehicle() != pCarToCollect)
						{
							if(pCarToCollect && IsEntityEntirelyInside3D(pCarToCollect))
							{		// Car IS inside
			
								if (AllPlayerEntitiesEntirelyOutside(), 2.0f)	// THIS IS WORNG AND HAS TO BE FIXED (Not now as gtaIV was submitted yesterday)
								{		// Player IS outside
										// Excellent. Take car.
										// Freeze player to stop him from going back.
									if (!NetworkInterface::IsGameInProgress())
									{
										pPlayerInfo->DisableControlsGarages();
										CGameWorld::FindLocalPlayerWanted()->m_PoliceBackOffGarage = true;
									}
								
									// Close the door now please.
									SetState(GSTATE_CLOSING);
									Flags.bClosingEmpty = false;
								}						
							}
						}
					}
					break;
				case GSTATE_CLOSING:
					if (pCarToCollect) ThrowCarsNearDoorOutOfGarage(pCarToCollect);
					if (SlideDoorClosed())
					{
						if (!Flags.bClosingEmpty)
						{
							if (pCarToCollect)
							{
								SetState(GSTATE_MISSIONCOMPLETED);
								// Destroy the car. (Not needed anymore)
								CVehicleFactory::GetFactory()->Destroy(pCarToCollect);
								pCarToCollect = NULL;
							}
							else
							{
								SetState(GSTATE_CLOSED);
							}
							// release player
							if (!NetworkInterface::IsGameInProgress())
							{
								pPlayerInfo->EnableControlsGarages();
								CGameWorld::FindLocalPlayerWanted()->m_PoliceBackOffGarage = false;
							}
						}
						else
						{
							SetState(GSTATE_CLOSED);
						}
					}
					break;
				case GSTATE_CLOSED:
						// If the player drives the right car and is nearby
						// we open the door.
				
					if (CGameWorld::FindLocalPlayerVehicle() == pCarToCollect && pCarToCollect)
					{
						// Work out the distance to the garage
						const Vector3 vPlayerVehiclePos = VEC3V_TO_VECTOR3(CGameWorld::FindLocalPlayerVehicle()->GetTransform().GetPosition());
						if (CalcDistToGarageRectangleSquared(vPlayerVehiclePos.x, vPlayerVehiclePos.y) < DOOROPENDISTSQR)
						{
							SetState(GSTATE_OPENING);
						}
					}
					break;
				case GSTATE_OPENING:
					if (SlideDoorOpen())
					{
						SetState(GSTATE_OPEN);
					}
					break;
				default:
					break;
			}
			break;

			// This garage can be opened by the script. Once opened it can be closed again
		case GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE:
			switch (state)
			{
				case GSTATE_OPEN:
					
					break;
				case GSTATE_CLOSING:
					if (SlideDoorClosed())
					{
						SetState(GSTATE_CLOSED);
					}
					break;
				case GSTATE_CLOSED:
					break;
				case GSTATE_OPENING:
					if (SlideDoorOpen())
					{
						SetState(GSTATE_OPEN);
					}
					break;
				default:
					break;
			}
			break;
		case GARAGE_SAFEHOUSE_PARKING_ZONE:
			if (!NetworkInterface::IsGameInProgress() && Flags.bSavingVehiclesEnabled)		// Parking spots outside safehouse only work in single player.
			{
				switch (state)
				{
					case GSTATE_OPEN:
						// Do we need to think about removing the cars?
						if( m_IsEnclosed )
						{
							// An enclosed garage
							if( IsBeyondStoreDistance2(SAFE_HOUSE_ENCLOSED_GARAGES_REMOVE_CARS_DIST*SAFE_HOUSE_ENCLOSED_GARAGES_REMOVE_CARS_DIST) )
							{
								// Yes, but we must await the door to be closed
								if( !m_pDoorObject || SlideDoorClosed() )
								{	// Save the cars; remove them from the map and close the garage.
									StoreAndRemoveCarsForThisHideOut(&CGarages::aCarsInSafeHouse[safeHouseIndex][0]);
									state = GSTATE_CLOSED;
									// Tell door it can start moving again
									SafeSetDoorFixedPhysics(false, false);
								}
							}
						}
						else
						{
							// An open Garage
							if( IsBeyondStoreDistance2(SAFE_HOUSE_GARAGES_REMOVE_CARS_DIST*SAFE_HOUSE_GARAGES_REMOVE_CARS_DIST) )
							{	// Save the cars; remove them from the map and close the garage.
								StoreAndRemoveCarsForThisHideOut(&CGarages::aCarsInSafeHouse[safeHouseIndex][0]);
								state = GSTATE_CLOSED;
							}
						}
						break;
					case GSTATE_CLOSED:
						// Do we need to think about spawning the cars in this garage?
						if( m_IsEnclosed )
						{
							if( IsWithinRestoreDistance2(SAFE_HOUSE_ENCLOSED_GARAGES_CREATE_CARS_DIST*SAFE_HOUSE_ENCLOSED_GARAGES_CREATE_CARS_DIST) )
							{
								// Yes
								if( !m_pDoorObject || SlideDoorClosed() )
								{
									if (RestoreCarsForThisHideOut(&CGarages::aCarsInSafeHouse[safeHouseIndex][0]))
									{
										state = GSTATE_OPEN;
										// Tell door it can start moving again
										SafeSetDoorFixedPhysics(false, false);
									}
								}
							}
						}
						else
						{
							if( IsWithinRestoreDistance2(SAFE_HOUSE_GARAGES_CREATE_CARS_DIST*SAFE_HOUSE_GARAGES_CREATE_CARS_DIST) )
							{	// Revive the cars in the garage
								if (RestoreCarsForThisHideOut(&CGarages::aCarsInSafeHouse[safeHouseIndex][0]))
								{
									state = GSTATE_OPEN;
								}
							}
						}
						break;

					default:
						Assert(0);
						break;
				}
			}
			break;

		case GARAGE_AMBIENT_PARKING:
			switch (state)
			{
				case GSTATE_OPEN:
					if (!m_pDoorObject)		// Need a proper condition to close here eventually
					{
						SetState(GSTATE_CLOSED);
					}
					break;
				case GSTATE_CLOSING:
					if (SlideDoorClosed())
					{
						SetState(GSTATE_CLOSED);
					}
					break;
				case GSTATE_CLOSED:
					if (m_pDoorObject)		// We only have a door object when reasonably close
					{
						if ( ( (MyIndex + fwTimer::GetSystemFrameCount()) & 15) == 0)
						{
							CVehicle::Pool *pool = CVehicle::GetPool();
							CVehicle* pVehicle;
							s32 i = (s32) pool->GetSize();

							while(i--)
							{
								pVehicle = pool->GetSlot(i);
								if (pVehicle)
								{
									if (IsLessThanAll(MagXY(m_pDoorObject->GetTransform().GetPosition() - pVehicle->GetTransform().GetPosition()), ScalarV(V_FIVE)))
									{
										SetState(GSTATE_OPENING);
									}
								}
							}
						}
					}
					break;
				case GSTATE_OPENING:
					if (SlideDoorOpen())
					{
						SetState(GSTATE_OPEN);
					}
					break;
				default:
					break;
				}
			break;
		default:
			break;
	}

	Flags.bDoorSlidingPreviousFrame = Flags.bDoorSlidingThisFrame;

	NetworkUpdate(MyIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : UpdateSize
// PURPOSE :  Updates garage size values
/////////////////////////////////////////////////////////////////////////////////

void CGarage::UpdateSize(int boxIndex, const Box &aBox)
{
	Box &box = m_boxes[boxIndex];
	MinX = MIN(MIN(MIN(MIN(aBox.BaseX, aBox.Delta1X + aBox.BaseX), aBox.Delta2X + aBox.BaseX), aBox.Delta1X + aBox.BaseX + aBox.Delta2X), MinX);
	MaxX = MAX(MAX(MAX(MAX(aBox.BaseX, aBox.Delta1X + aBox.BaseX), aBox.Delta2X + aBox.BaseX), aBox.Delta1X + aBox.BaseX + aBox.Delta2X), MaxX);

	MinY = MIN(MIN(MIN(MIN(aBox.BaseY, aBox.Delta1Y + aBox.BaseY), aBox.Delta2Y + aBox.BaseY), aBox.Delta1Y + aBox.BaseY + aBox.Delta2Y), MinY);
	MaxY = MAX(MAX(MAX(MAX(aBox.BaseY, aBox.Delta1Y + aBox.BaseY), aBox.Delta2Y + aBox.BaseY), aBox.Delta1Y + aBox.BaseY + aBox.Delta2Y), MaxY);

	MinZ = MIN(MinZ, aBox.BaseZ);
	MaxZ = MAX(MaxZ, aBox.CeilingZ);

	box.BaseX = aBox.BaseX;
	box.BaseY = aBox.BaseY;
	box.BaseZ = aBox.BaseZ;
	box.Delta1X = aBox.Delta1X;
	box.Delta1Y = aBox.Delta1Y;
	box.Delta2X = aBox.Delta2X;
	box.Delta2Y = aBox.Delta2Y;
	box.CeilingZ = aBox.CeilingZ;
	box.useLineIntersection = aBox.useLineIntersection;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : 
// PURPOSE : 
//
/////////////////////////////////////////////////////////////////////////////////

bool	CGarage::IsBeyondStoreDistance2(float distToCheck2)
{
	float distSqr = (CGameWorld::FindLocalPlayerCoors() - Vector3(m_boxes[0].BaseX, m_boxes[0].BaseY, m_boxes[0].BaseZ)).Mag2(); // Ignoring other boxes for now
	if( distSqr > distToCheck2 )
	{
		return true;
	}
	return false;
}

bool	CGarage::IsWithinRestoreDistance2(float distToCheck2)
{
	float distSqr = (CGameWorld::FindLocalPlayerCoors() - Vector3(m_boxes[0].BaseX, m_boxes[0].BaseY, m_boxes[0].BaseZ)).Mag2(); // Ignoring other boxes for now
	if( distSqr < distToCheck2 )
	{
		return true;
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : 
// PURPOSE : 
//
/////////////////////////////////////////////////////////////////////////////////

void	CGarage::SafeSetDoorFixedPhysics(bool bFixed, bool bNetwork)
{
	// If this door is not controlled by script its safe to set its physics
	if (m_pDoorObject && !DoorIsInUseByScript(m_pDoorObject))
	{
		m_pDoorObject->SetFixedPhysics(bFixed, bNetwork);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsStaticPlayerCarInside
// PURPOSE :  returns true if the player is in his car inside the garage
//			  and not moving.
/////////////////////////////////////////////////////////////////////////////////

#define CARISSTATICSPEED (0.01f)

bool CGarage::IsStaticPlayerCarEntirelyInside(void)
{
	float		SpeedSqr, SpeedX, SpeedY, SpeedZ;
	CAutomobile	*pCar;

	if (!CGameWorld::FindLocalPlayerVehicle()) return (false);
	if (!CGameWorld::FindLocalPlayerVehicle()->InheritsFromAutomobile() && !CGameWorld::FindLocalPlayerVehicle()->InheritsFromBike()) return (false);

	if(FindPlayerPed()->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_EXIT_VEHICLE)) return false;

	pCar = (CAutomobile	*)CGameWorld::FindLocalPlayerVehicle();

	const Vector3 vCarPos = VEC3V_TO_VECTOR3(pCar->GetTransform().GetPosition());
	if (vCarPos.x < MinX) return false;
	if (vCarPos.x > MaxX) return false;
	if (vCarPos.y < MinY) return false;
	if (vCarPos.y > MaxY) return false;

	if ( (SpeedX = ABS(pCar->GetVelocity().x)) > CARISSTATICSPEED) return false;
	if ( (SpeedY = ABS(pCar->GetVelocity().y)) > CARISSTATICSPEED) return false;
	if ( (SpeedZ = ABS(pCar->GetVelocity().z)) > CARISSTATICSPEED) return false;

	SpeedSqr = SpeedX * SpeedX + SpeedY * SpeedY + SpeedZ * SpeedZ;

	if (SpeedSqr > CARISSTATICSPEED*CARISSTATICSPEED) return false;

	return IsEntityEntirelyInside3D(pCar, 0.5f);
}

void CGarage::BuildQuadTestPoints(const CEntity *pEntity,  Vector3 &p0, Vector3 &p1, Vector3 &p2, Vector3 &p3)
{
	if (pEntity->GetIsTypeVehicle())
	{
		Vector3 BBox0 = pEntity->GetBoundingBoxMin(); 
		Vector3 BBox3 = pEntity->GetBoundingBoxMax(); 

		Vector3 BBox1(BBox0.x, BBox3.y, BBox0.z); 
		Vector3 BBox2(BBox3.x, BBox0.y, BBox3.z); 

		BBox0 = pEntity->TransformIntoWorldSpace(BBox0);
		BBox1 = pEntity->TransformIntoWorldSpace(BBox1);
		BBox2 = pEntity->TransformIntoWorldSpace(BBox2);
		BBox3 = pEntity->TransformIntoWorldSpace(BBox3);

		float averageZ = (BBox0.z + BBox3.z) * 0.5f;

		BBox0.z = averageZ;
		BBox1.z = averageZ;
		BBox2.z = averageZ;
		BBox3.z = averageZ;

		p0 = BBox0;
		p1 = BBox1;
		p2 = BBox2;
		p3 = BBox3;
	}
	else
	{
		Vector3 BBoxMin = pEntity->TransformIntoWorldSpace(pEntity->GetBoundingBoxMin());
		Vector3 BBoxMax = pEntity->TransformIntoWorldSpace(pEntity->GetBoundingBoxMax());

		float averageZ = (BBoxMin.z + BBoxMax.z) * 0.5f;

		p0 = Vector3(BBoxMin.x, BBoxMin.y, averageZ);
		p1 = Vector3(BBoxMin.x, BBoxMax.y, averageZ);
		p2 = Vector3(BBoxMax.x, BBoxMin.y, averageZ);
		p3 = Vector3(BBoxMax.x, BBoxMax.y, averageZ);
	}
}

//		   2------------5
//		  /|		   /|
//		 / |		  / |
//      /  |		 /  |
//      1 ----------6   |
// 		|  |		|   |
// 		|  3--------|--4
// 		| /			| /
// 		|/			|/
//		0 --------- 5
inline void CGarage::BuildBoxTestPoints(const CEntity *pEntity, Vector3 &_p0, Vector3 &_p1, Vector3 &_p2, Vector3 &_p3,
															    Vector3 &_p4, Vector3 &_p5, Vector3 &_p6, Vector3 &_p7)
{
	if (pEntity->GetIsTypeVehicle())
	{
		Vector3 p0 = pEntity->GetBoundingBoxMin();
		Vector3 p7 = pEntity->GetBoundingBoxMax();

		Vector3 p1(p0.x, p0.y, p7.z);
		Vector3 p2(p0.x, p7.y, p7.z);
		Vector3 p3(p0.x, p7.y, p0.z);
		Vector3 p4(p7.x, p7.y, p0.z);
		Vector3 p5(p7.x, p0.y, p0.z);
		Vector3 p6(p7.x, p0.y, p7.z);

		_p0 = pEntity->TransformIntoWorldSpace(p0);
		_p1 = pEntity->TransformIntoWorldSpace(p1);
		_p2 = pEntity->TransformIntoWorldSpace(p2);
		_p3 = pEntity->TransformIntoWorldSpace(p3);

		_p4 = pEntity->TransformIntoWorldSpace(p4);
		_p5 = pEntity->TransformIntoWorldSpace(p5);
		_p6 = pEntity->TransformIntoWorldSpace(p6);
		_p7 = pEntity->TransformIntoWorldSpace(p7);
	}
	else
	{
		_p0 = pEntity->TransformIntoWorldSpace(pEntity->GetBoundingBoxMin());
		_p7 = pEntity->TransformIntoWorldSpace(pEntity->GetBoundingBoxMax());

		_p1 = Vector3(_p0.x, _p0.y, _p7.z);
		_p2 = Vector3(_p0.x, _p7.y, _p7.z);
		_p3 = Vector3(_p0.x, _p7.y, _p0.z);
		_p4 = Vector3(_p7.x, _p7.y, _p0.z);
		_p5 = Vector3(_p7.x, _p0.y, _p0.z);
		_p6 = Vector3(_p7.x, _p0.y, _p7.z);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsEntityEntirelyInside3D
// PURPOSE :  returns true if this object and all its spheres are entirely inside
//			  the garage.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsEntityEntirelyInside3D(const CEntity *pEntity, float Margin, int boxIndex)
{
	const Vector3 vEntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	if (vEntityPos.x < MinX-Margin) return false;
	if (vEntityPos.x > MaxX+Margin) return false;
	if (vEntityPos.y < MinY-Margin) return false;
	if (vEntityPos.y > MaxY+Margin) return false;
	if (vEntityPos.z < MinZ-Margin) return false;
	if (vEntityPos.z > MaxZ+Margin) return false;


	Vector3 p0, p1, p2, p3;
	BuildQuadTestPoints(pEntity, p0, p1, p2, p3);

	if (!IsPointInsideGarage(p0, Margin, boxIndex)) return false;
	if (!IsPointInsideGarage(p1, Margin, boxIndex)) return false;
	if (!IsPointInsideGarage(p2, Margin, boxIndex)) return false;
	if (!IsPointInsideGarage(p3, Margin, boxIndex)) return false;

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsEntityEntirelyOutside
// PURPOSE :  returns true if this object and all its spheres are entirely outside
//			  the garage.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsEntityEntirelyOutside(const CEntity *pEntity, float Margin, int boxIndex)
{
	const Vector3 vEntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	if (vEntityPos.x > (MinX-Margin) &&
		vEntityPos.x < (MaxX+Margin) &&
		vEntityPos.y > (MinY-Margin) &&
		vEntityPos.y < (MaxY+Margin)) 
	{
		return false;
	}
	Vector3 p0, p1, p2, p3, p4, p5, p6, p7;
	CGarage::BuildBoxTestPoints(pEntity, p0, p1, p2, p3, p4, p5, p6, p7);
	if (IsPointInsideGarage(p0, Margin)) return false;
	if (IsPointInsideGarage(p7, Margin)) return false;

	if (IsPointInsideGarage(p1, Margin, boxIndex)) return false;
	if (IsPointInsideGarage(p2, Margin, boxIndex)) return false;
	if (IsPointInsideGarage(p3, Margin, boxIndex)) return false;
	if (IsPointInsideGarage(p4, Margin, boxIndex)) return false;
	if (IsPointInsideGarage(p5, Margin, boxIndex)) return false;
	if (IsPointInsideGarage(p6, Margin, boxIndex)) return false;

	// This is not really an accurate solution. It is possible for the car to be partially
	// within the garage even though all the corner points are outside.
	// I don't think this will be a problem as the cars will never approach a garage from a
	// corner but if it is; I'll have to re-write this function.

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AllPlayerEntitiesEntirelyOutside
// PURPOSE :  returns true if this object and all its spheres are entirely outside
//			  the garage.
//			  This tests for all the players (in a network game)
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::AllPlayerEntitiesEntirelyOutside(float Margin)
{
	if (AssertVerify(NetworkInterface::IsGameInProgress()))
	{
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
        const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

        for(unsigned index = 0; index < numPhysicalPlayers; index++)
        {
            const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

            if(pPlayer->GetPlayerPed())
            {
			    if (pPlayer->GetPlayerPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ) && pPlayer->GetPlayerPed()->GetMyVehicle())
			    {
				    if (!IsEntityEntirelyOutside( pPlayer->GetPlayerPed()->GetMyVehicle(), Margin))
				    {
					    return false;
				    }
			    }
			    else
			    {
				    if (!IsEntityEntirelyOutside( pPlayer->GetPlayerPed(), Margin))
				    {
					    return false;
				    }
			    }
            }
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AnyNetworkPlayersOnFootInGarage
// PURPOSE :  returns true if any of the network players are within the garage.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::AnyNetworkPlayersOnFootInGarage(float Margin)
{
	if (NetworkInterface::IsGameInProgress())
	{
		unsigned                 numPhysicalPlayers = netInterface::GetNumPhysicalPlayers();
        const netPlayer * const *allPhysicalPlayers = netInterface::GetAllPhysicalPlayers();

        for(unsigned index = 0; index < numPhysicalPlayers; index++)
        {
            const CNetGamePlayer *pPlayer = SafeCast(const CNetGamePlayer, allPhysicalPlayers[index]);

            if(pPlayer->GetPlayerPed())
            {
			    if (!pPlayer->GetPlayerPed()->GetPedConfigFlag( CPED_CONFIG_FLAG_InVehicle ))
			    {
				    if (IsPointInsideGarage(VEC3V_TO_VECTOR3(pPlayer->GetPlayerPed()->GetTransform().GetPosition()), Margin))
				    {
					    return true;
				    }
			    }
            }
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsGarageEmpty
// PURPOSE :  Tests whether this garage is empty.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsGarageEmpty(int boxIndex, bool alternateSearch)
{	
	if(alternateSearch)
	{
		spdAABB boxBound;		
		boxBound.Set(VECTOR3_TO_VEC3V(Vector3(MinX, MinY, MinZ)), VECTOR3_TO_VEC3V(Vector3(MaxX, MaxY, MaxZ)));
		// check vehicles
		CVehicle::Pool* vehiclePool = CVehicle::GetPool();
		for(s32 i = 0; i < vehiclePool->GetSize(); i++)
		{
			CVehicle* pVeh = vehiclePool->GetSlot(i);
			if(pVeh)
			{
				spdAABB tempBox;
				spdAABB vehBound = pVeh->GetBoundBox(tempBox);
				if(boxBound.IntersectsAABB(vehBound))
				{
					if (IsEntityTouching3D(pVeh, boxIndex))
					{						
						return false;
					}
				}
			}
		}
		// check peds		
		CPed::Pool* pedPool = CPed::GetPool();
		for(s32 i = 0; i < pedPool->GetSize(); i++)
		{
			CPed* pPed = pedPool->GetSlot(i);
			if(pPed)
			{
				spdAABB tempBox;
				spdAABB pedBound = pPed->GetBoundBox(tempBox);
				if(boxBound.IntersectsAABB(pedBound))
				{
					if (IsEntityTouching3D(pPed, boxIndex))
					{						
						return false;
					}
				}
			}
		}
	}
	else
	{
		s32	Num = 0;
		s32 C;
		CEntity	*apEnts[16];

		CGameWorld::FindObjectsIntersectingCube(Vector3(MinX, MinY, MinZ), Vector3(MaxX, MaxY, MaxZ), &Num, 16, apEnts, false, true, true, false, false);		

		// Go through the entities found and test whether any of them really are touching the garage 
		for (C = 0; C < Num; C++)
		{
			if (IsEntityTouching3D(apEnts[C], boxIndex))
			{
				return false;
			}
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPlayerOutsideGarage
// PURPOSE :  Tests whether this garage is empty.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsPlayerOutsideGarage(const CEntity *pEntity, float margin, int boxIndex)
{
	Assertf(pEntity->GetBaseModelInfo()->GetModelType() == MI_TYPE_PED, "Player Model Type is not MI_TYPE_PED, %d", pEntity->GetBaseModelInfo()->GetModelType());
	const CPed *pPlayer = static_cast<const CPed*>(pEntity);

	if (pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		CVehicle *pVehicle = pPlayer->GetMyVehicle();
		return IsEntityEntirelyOutside(pVehicle, margin, boxIndex);
	}

	return IsEntityEntirelyOutside(pPlayer, margin, boxIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPlayerEntirelyInsideGarage
// PURPOSE :  Tests whether this garage is empty.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsPlayerEntirelyInsideGarage(const CEntity *pEntity, float margin, int boxIndex)
{
	Assertf(pEntity, "CGarage::IsPlayerEntirelyInsideGarage Invalid Entity");
	Assertf(pEntity->GetIsTypePed(), "CGarage::IsPlayerEntirelyInsideGarage Passed in Entity %s is not a ped",pEntity->GetModelName());
	Assertf(pEntity->GetBaseModelInfo()->GetModelType() == MI_TYPE_PED, "Player Model Type is not MI_TYPE_PED, %d", pEntity->GetBaseModelInfo()->GetModelType());
	const CPed *pPlayer = static_cast<const CPed*>(pEntity);

	if(m_IsMPGarage && boxIndex == ALL_BOXES_INDEX )
	{
		// MP Garage stuff (B*782985)
		Assertf(m_InteriorBoxIDX!=-1, "ERROR: MP Garage has no interior box IDX" );
		Assertf(m_ExteriorBoxIDX!=-1, "ERROR: MP Garage has no exterior box IDX" );

		// MP Garage functionality
		if (pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
		{
			CVehicle *pVehicle = pPlayer->GetMyVehicle();
			return IsEntityEntirelyInside3D(pVehicle, margin, m_ExteriorBoxIDX) && 
				IsPointInsideGarage(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()), margin, m_InteriorBoxIDX);
		}
		
		return IsEntityEntirelyInside3D(pPlayer, margin, m_ExteriorBoxIDX) && 
			IsPointInsideGarage(VEC3V_TO_VECTOR3(pPlayer->GetTransform().GetPosition()), margin, m_InteriorBoxIDX);
	}

	// Normal non MP Garage functionality
	if (pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		CVehicle *pVehicle = pPlayer->GetMyVehicle();
		if (Verifyf(pVehicle, "IsPlayerEntirelyInsideGarage ... Ped [%s] has no vehicle but has config flag CPED_CONFIG_FLAG_InVehicle", pPlayer->GetModelName()))
		{
			return IsEntityEntirelyInside3D(pVehicle, margin, boxIndex);
		}
	}

	return IsEntityEntirelyInside3D(pPlayer, margin, boxIndex);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsObjectEntirelyInsideGarage
// PURPOSE :  Tests whether this garage is empty.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsObjectEntirelyInsideGarage(const CEntity *pEntity, float margin, int boxIndex)
{
	if(m_IsMPGarage && boxIndex == CGarage::ALL_BOXES_INDEX )
	{
		// MP Garage stuff (B*782985)
		Assertf(m_InteriorBoxIDX!=-1, "ERROR: MP Garage has no interior box IDX" );
		Assertf(m_ExteriorBoxIDX!=-1, "ERROR: MP Garage has no exterior box IDX" );

		// MP Garage functionality
		return IsEntityEntirelyInside3D(pEntity, margin, m_ExteriorBoxIDX) && 
			   IsPointInsideGarage(VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition()), margin, m_InteriorBoxIDX);
	}

	return IsEntityEntirelyInside3D(pEntity, margin, boxIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AreEntitiesEntirelyInsideGarage
// PURPOSE :  returns true if all entities of the specified type(s) are fully inside the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::AreEntitiesEntirelyInsideGarage(bool peds, bool vehs, bool objs, float margin, int boxIndex)
{
	s32	Num;
	CEntity	*apEnts[32];

	CGameWorld::FindObjectsIntersectingCube(Vector3(MinX, MinY, MinZ), Vector3(MaxX, MaxY, MaxZ), &Num, 16, apEnts, false, vehs, peds, objs, false);

	// Go through the entities found and test whether any of them really are touching the garage
	for (s32 i = 0; i < Num; ++i)
	{
		if (!IsObjectEntirelyInsideGarage(apEnts[i], margin, boxIndex))
		{
			return false;
		}
	}

	return true;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsAnyEntitiesEntirelyInsideGarage
// PURPOSE :  returns true if any entities of the specified type(s) are fully inside the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsAnyEntityEntirelyInsideGarage(bool peds, bool vehs, bool objs, float margin, int boxIndex)
{
	s32	Num;
	CEntity	*apEnts[32];

	CGameWorld::FindObjectsIntersectingCube(Vector3(MinX, MinY, MinZ), Vector3(MaxX, MaxY, MaxZ), &Num, 16, apEnts, false, vehs, peds, objs, false);

	// Go through the entities found and test whether any of them really are touching the garage
	for (s32 i = 0; i < Num; ++i)
	{
		if (IsEntityEntirelyInside3D(apEnts[i], margin, boxIndex))
		{
			return true;
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsEntityTouching3D
// PURPOSE :  returns true if at least one of the spheres of this object touch the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsEntityTouching3D(const CEntity *pEntity, int boxIndex)
{
	float	Radius = pEntity->GetBoundRadius();

	const Vector3 vEntityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
	if (vEntityPos.x < MinX - Radius) return false;
	if (vEntityPos.x > MaxX + Radius) return false;
	if (vEntityPos.y < MinY - Radius) return false;
	if (vEntityPos.y > MaxY + Radius) return false;
	if (vEntityPos.z < MinZ - Radius) return false;
	if (vEntityPos.z > MaxZ + Radius) return false;

	// We go through the 8 corner points of the bounding box and check if any point is inside
	Vector3 p0, p1, p2, p3, p4, p5, p6, p7;
	CGarage::BuildBoxTestPoints(pEntity, p0, p1, p2, p3, p4, p5, p6, p7);

	if (IsPointInsideGarage(p0, boxIndex)) return true;
	if (IsPointInsideGarage(p7, boxIndex)) return true;
	if (IsPointInsideGarage(p1, boxIndex)) return true;
	if (IsPointInsideGarage(p2, boxIndex)) return true;
	if (IsPointInsideGarage(p3, boxIndex)) return true;
	if (IsPointInsideGarage(p4, boxIndex)) return true;
	if (IsPointInsideGarage(p5, boxIndex)) return true;
	if (IsPointInsideGarage(p6, boxIndex)) return true;

	int startIndex;
	int count;
	if (boxIndex == ALL_BOXES_INDEX)
	{
		startIndex = 0;
		count = m_boxes.GetCount();
	}
	else
	{
		startIndex = boxIndex;
		count = boxIndex + 1;
	}

	//		   2------------7
	//		  /|		   /|
	//		 / |		  / |
	//      /  |		 /  |
	//      1 ----------6   |
	// 		|  |		|   |
	// 		|  3--------|-- 4
	// 		| /			|  /
	// 		|/			| /
	//		0 --------- 5
	
	Vector3 pA(p0.x, p0.y, (p0.z + p1.z)* 0.5f);
	Vector3 pB(p5.x, p5.y, (p5.z + p6.z)* 0.5f);
	Vector3 pC(p3.x, p3.y, (p3.z + p2.z)* 0.5f);
	Vector3 pD(p4.x, p4.y, (p4.z + p7.z)* 0.5f);

	for (int i = startIndex; i < count; ++i)
	{
		if (!m_boxes[i].useLineIntersection)
		{
			continue;
		}
		if (IsBoxIntersectingGarage(pA, pB, pC, pD, i))
		{
			return true;
		}
	}

	return false;
}



inline bool CalcLineParams(const Vector2& p0, const Vector2& p1, float &m, float &c)
{
	float xDiff = p0.x - p1.x;
	if ((fabs(xDiff) < 0.01f))
	{
		m = 0.0f;
		c = 0.0f;
		return true;
	}

	m = (p0.y - p1.y)/ xDiff;
	c = -p1.x * m + p1.y;
	return false;
}
inline bool LinesIntersect(const Vector2& pA0, const Vector2& pA1, const Vector2& pB0, const Vector2& pB1)
{
	float mA, cA;
	bool verticalA = CalcLineParams(pA0, pA1, mA, cA);

	float mB, cB;
	bool verticalB = CalcLineParams(pB0, pB1, mB, cB);

	if (verticalA && verticalB)
	{
		return false;
	}

	Vector2 p;
	if (verticalA)
	{
		p.x = pA0.x;
		p.y = mB*p.x + cB;
	}
	else if (verticalB)
	{
		p.x = pB0.x;
		p.y = mA*p.x + cA;
	}
	else if (fabs(mA - mB) < 0.005f)
	{
		return false;
	}
	else
	{
		float invGradientDiff = 1.0f / (mA - mB);  

		p.x = (cB - cA)		   * invGradientDiff; 
		p.y = (cB* mA - mB * cA) * invGradientDiff; 
	}

	const float epsilon = 0.001f;

	float minXA = rage::Min(pA0.x, pA1.x);
	float minXB = rage::Min(pB0.x, pB1.x);
	float testMinX = rage::Max(minXA, minXB) - epsilon;

	float minYA	= rage::Min(pA0.y, pA1.y);
	float minYB = rage::Min(pB0.y, pB1.y);
	float testMinY = rage::Max(minYA, minYB) - epsilon;

	float maxXA = rage::Max(pA0.x, pA1.x);
	float maxXB = rage::Max(pB0.x, pB1.x);
	float testMaxX = rage::Min(maxXA, maxXB) + epsilon;

	float maxYA = rage::Max(pA0.y, pA1.y);
	float maxYB = rage::Max(pB0.y, pB1.y);
	float testMaxY = rage::Min(maxYA, maxYB) + epsilon;

	return (p.x >= testMinX && p.x <= testMaxX && p.y >= testMinY && p.y <= testMaxY);
}

bool CGarage::IsBoxIntersectingGarage(const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, int boxIndex)
{
	//  p2 ------------ p3
	//	|				|
	//	|				|
	//	|				|
	//	|				|
	//	|				|
	//	p0 ------------ p1

	if (IsLineIntersectingGarage(p0, p1, boxIndex))
	{
		return true;
	}

	if (IsLineIntersectingGarage(p2, p3, boxIndex))
	{
		return true;
	}

	if (IsLineIntersectingGarage(p0, p2, boxIndex))
	{
		return true;
	}

	if (IsLineIntersectingGarage(p1, p3, boxIndex))
	{
		return true;
	}

	return false;
}


bool CGarage::IsLineIntersectingGarage(const Vector3 &start3, const Vector3 &end3, int boxIndex)
{
	Vector2 start(start3.x, start3.y);
	Vector2 end(end3.x, end3.y);

	int startIndex;
	int count;
	if (boxIndex == ALL_BOXES_INDEX)
	{
		startIndex = 0;
		count = m_boxes.GetCount();
	}
	else
	{
		startIndex = boxIndex;
		count = boxIndex + 1;
	}

	float middleZ = (start3.z + end3.z) * 0.5f;

	for (int i = startIndex; i < count; ++i)
	{
		Box &box = m_boxes[i];
		 
		if (middleZ >= box.BaseZ && middleZ <=  box.CeilingZ)
		{
			//  p2 ------------ p3
			//	|				|
			//	|				|
			//	|				|
			//	|				|
			//	|				|
			//	p0 ------------ p1

			Vector2 p0(box.BaseX, box.BaseY);
			Vector2 p1(p0.x + box.Delta1X, p0.y + box.Delta1Y);
			Vector2 p2(p0.x + box.Delta2X, p0.y + box.Delta2Y);
			Vector2 p3(p1.x + box.Delta2X, p1.y + box.Delta2Y);
		
			// front
			if (LinesIntersect(start, end, p0, p1))
			{
				return true;
			}

			// back
			if (LinesIntersect(start, end, p2, p3))
			{
				return true;
			}
			// left
			if (LinesIntersect(start, end, p0, p2))
			{
				return true;
			}
			// right
			if (LinesIntersect(start, end, p1, p3))
			{
				return true;
			}

		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : EntityHasASphereWayOutsideGarage
// PURPOSE :  returns true if this entity has at least one sphere way outside the
//			  garage. We are talking about the radius of the sphere + a margin.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::EntityHasASphereWayOutsideGarage(CEntity *pEntity, float Margin)
{

	// We go through the 8 corner points of the bounding box and check inf any sphere is 
	// outside the garage.
	Vector3 p0, p1, p2, p3, p4, p5, p6, p7;
	CGarage::BuildBoxTestPoints(pEntity, p0, p1, p2, p3, p4, p5, p6, p7);
	
	if (!IsPointInsideGarage(p0, Margin)) return true;
	if (!IsPointInsideGarage(p7, Margin)) return true;
	if (!IsPointInsideGarage(p1, Margin)) return true;
	if (!IsPointInsideGarage(p2, Margin)) return true;
	if (!IsPointInsideGarage(p3, Margin)) return true;
	if (!IsPointInsideGarage(p4, Margin)) return true;
	if (!IsPointInsideGarage(p5, Margin)) return true;
	if (!IsPointInsideGarage(p6, Margin)) return true;

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsAnyOtherCarTouchingGarage
// PURPOSE :  returns true if a car that is not the player touches the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsAnyOtherCarTouchingGarage(CVehicle *pException)
{

	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();
	
	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle && pVehicle != pException && pVehicle->GetStatus() != STATUS_WRECKED)
		{
			if (IsEntityTouching3D(pVehicle))
			{
				return true;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ThrowCarsNearDoorOutOfGarage
// PURPOSE :  Any car touching a door of this garage will slowly be pushed out
/////////////////////////////////////////////////////////////////////////////////

void CGarage::ThrowCarsNearDoorOutOfGarage(const CVehicle *pException)
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();
	
	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle && pVehicle != pException)
		{
			if (IsEntityTouching3D(pVehicle) && EntityHasASphereWayOutsideGarage(pVehicle))
			{		// Slowly push this guy away from the center of the garage.
				Vector3	Diff;
				const Vector3 vVehiclePos = VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition());
				Diff.x = vVehiclePos.x - (MinX + MaxX) * 0.5f;
				Diff.y = vVehiclePos.y - (MinY + MaxY) * 0.5f;
				Diff.z = 0.0f;
				Diff.Normalize();
				pVehicle->SetVelocity(pVehicle->GetVelocity() + Diff * 0.02f * 50.0f*fwTimer::GetTimeStep());
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsAnyOtherPedTouchingGarage
// PURPOSE :  returns true if a ped that is not the exception touches the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsAnyOtherPedTouchingGarage(CPed *pException)
{
	CPed::Pool *pool = CPed::GetPool();
	CPed* pPed;
	s32 i=pool->GetSize();
	
	while(i--)
	{
		pPed = pool->GetSlot(i);
		if(pPed && pPed != pException)
		{
			if (IsEntityTouching3D(pPed))
			{
				return true;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsAnyCarBlockingDoor
// PURPOSE :  returns true if a car has at least 1 sphere entirely inside and
//			  one sphere entirely outside garage. This should stop certain garages
//			  from closing.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsAnyCarBlockingDoor()
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();
	
	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle)
		{
			if (IsEntityTouching3D(pVehicle) && EntityHasASphereWayOutsideGarage(pVehicle))
			{
				return true;
			}
		}
	}

	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CountCarsWithCenterPointWithinGarage
// PURPOSE :  Returns the number of them
/////////////////////////////////////////////////////////////////////////////////

s32 CGarage::CountCarsWithCenterPointWithinGarage(CEntity *pException)
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* 	pVehicle;
	s32 		i = (s32) pool->GetSize();
	s32		Result = 0;
	
	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle && pVehicle != pException)
		{
			if (IsPointInsideGarage(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())))
			{
				Result++;
			}
		}
	}
	return Result;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RemoveCarsBlockingDoorNotInside
// PURPOSE :  Removes cars touching the door but not inside
/////////////////////////////////////////////////////////////////////////////////

void CGarage::RemoveCarsBlockingDoorNotInside()
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();
	
	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if(pVehicle)
		{
			if (IsEntityTouching3D(pVehicle))
			{
				if (!IsPointInsideGarage(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())))
				{	// Remove this guy (if we are allowed)
					if ((!pVehicle->m_nVehicleFlags.bCannotRemoveFromWorld) && (pVehicle->CanBeDeleted()))
					{
						CVehicleFactory::GetFactory()->Destroy(pVehicle);
						return;
					}
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsCarSprayable
// PURPOSE :  Works out whether the spray dude will take this car.
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsCarSprayable(CVehicle *pCar)
{
    CVehicleModelInfo* vmi = pCar->GetVehicleModelInfo();
    Assert(vmi);
    if (vmi->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_NO_RESPRAY))
        return false;

    return true;
}
		
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : SetTargetCarForMissionGarage
// PURPOSE :  Tells a specific garage to open for a specific car only.
/////////////////////////////////////////////////////////////////////////////////

void CGarages::SetTargetCarForMissionGarage(s32 NumGarage, CAutomobile *pCar)
{

	if (!pCar)
	{
		aGarages[NumGarage].pCarToCollect = NULL;
		return;
	}

	// We found the garage. Make sure it's a mission one
	Assert(aGarages[NumGarage].type == GARAGE_MISSION);
	// Make sure the car is kosher
	
	if (pCar)
	{
		AssertVehiclePointerValid(pCar);
	}

	aGarages[NumGarage].pCarToCollect = pCar;

	// If the guy has been used for a previous mission we re-activate him.
	if (aGarages[NumGarage].GetState() == GSTATE_MISSIONCOMPLETED) aGarages[NumGarage].SetState(GSTATE_CLOSED);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : HasCarBeenDroppedOffYet
// PURPOSE :  Tells the script whether a car has been dropped of by the player yet.
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::HasCarBeenDroppedOffYet(s32 NumGarage)
{
	return (aGarages[NumGarage].GetState() == GSTATE_MISSIONCOMPLETED);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsGarageOpen
// PURPOSE :  Returns true if this garage is open.
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsGarageOpen(s32 NumGarage)
{
	CGarage	*pGarage;
	
	pGarage = &aGarages[NumGarage];

	if (pGarage->GetState() == GSTATE_OPEN || pGarage->GetState() == GSTATE_OPENEDWAITINGTOBEREACTIVATED)
	{
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsGarageClosed
// PURPOSE :  Returns true if this garage is closed.
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsGarageClosed(s32 NumGarage)
{
	CGarage	*pGarage;
	
	pGarage = &aGarages[NumGarage];

	if (pGarage->GetState() == GSTATE_CLOSED)
	{
		return true;
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : OpenThisGarage
// PURPOSE :  If this is the right type of garage it will be opened.
/////////////////////////////////////////////////////////////////////////////////

void CGarage::OpenThisGarage()
{
	Assert(type == GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE);

	if (state == GSTATE_CLOSED || state == GSTATE_CLOSING || state == GSTATE_MISSIONCOMPLETED)
	{
		//@@: location CGARAGE_OPENTHISGARAGE
		SetState(GSTATE_OPENING);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CloseThisGarage
// PURPOSE :  If this is the right type of garage it will be closed.
/////////////////////////////////////////////////////////////////////////////////

void CGarage::CloseThisGarage()
{
	Assert(type == GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE);

	if (state == GSTATE_OPEN || state == GSTATE_OPENING)
	{
		SetState(GSTATE_CLOSING);
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalcDistToGarageRectangleSquared
// PURPOSE :  Works out the distance from a point to the area that is defined for
//			  the garage. This is better and more accurate than simply taking the
//			  distance to the center point.
/////////////////////////////////////////////////////////////////////////////////

float CGarage::CalcDistToGarageRectangleSquared(float X, float Y)
{
	float	DistX, DistY;
	
	if (X < MinX)
	{
		DistX = X - MinX;
	}
	else if (X > MaxX)
	{
		DistX = X - MaxX;
	}
	else
	{
		DistX = 0.0f;
	}
	
	if (Y < MinY)
	{
		DistY = Y - MinY;
	}
	else if (Y > MaxY)
	{
		DistY = Y - MaxY;
	}
	else
	{
		DistY = 0.0f;
	}
	
	return (DistX*DistX + DistY*DistY);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsThisCarWithinGarageArea
// PURPOSE :  Returns true if the car is in the catchment area of the garage.
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsThisCarWithinGarageArea(s32 NumGarage, const CEntity *pEntity)
{
	return (aGarages[NumGarage].IsEntityEntirelyInside3D(pEntity));
}


bool CGarage::CanThisVehicleBeStoredInThisGarage(const CVehicle *pVehicle)
{
	bool storeVehicle = false;

	if (pVehicle && pVehicle->CanSaveInGarage())
	{
		if (!pVehicle->PopTypeIsMission())	// Don't want to remove mission cars. That would fail the mission
		{
			if (!pVehicle->HasPedsInIt() )
			{
				if( IsPointInsideGarage(VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition())))							// Make sure there is no-one in it. Don't want to save players vehicles or random ambient cars
				{
					bool isAcceptableGarage = true;

					// Does this garage have an owner?
					if(owner.GetHash() != 0)
					{
						// Yes
						CPed *pPed = pVehicle->GetSeatManager()->GetLastPedInSeat(0);
						if(pPed)
						{
							// If the last person to drive this car (the person who parked it), isn't the owner of this garage, then it is not allowed to be saved.
							if( pPed->GetPedModelInfo()->GetModelNameHash() != owner.GetHash() )
							{
								isAcceptableGarage = false;
							}
						}
					}

					if(isAcceptableGarage)
					{
						if (m_permittedVehicleType == VEHICLE_TYPE_NONE) 
						{
							storeVehicle = (pVehicle->GetVehicleType() == VEHICLE_TYPE_CAR || pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE || pVehicle->GetVehicleType() == VEHICLE_TYPE_BICYCLE || pVehicle->GetVehicleType() == VEHICLE_TYPE_HELI || pVehicle->GetVehicleType() == VEHICLE_TYPE_QUADBIKE);
						}
						else 
						{
							storeVehicle = m_permittedVehicleType == pVehicle->GetVehicleType();
						}
					}
				}
			}
		}
	}

	return storeVehicle;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StoreAndRemoveCarsForThisHideOut
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

void CGarage::StoreAndRemoveCarsForThisHideOut(CStoredVehicle *aStoredCars, bool bRemoveCars)
{
	s32	C;
	Vector3	GarageCentre;
	CalcCentrePointOfGarage(GarageCentre);

	// Clear the cars first
	for (C = 0; C < MAX_NUM_STORED_CARS_IN_SAFEHOUSE; C++)
	{
		aStoredCars[C].Clear();
	}
	
	// Find the first car in the area of the garage.
	// Go through the cars in the vehicle list
		
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVehicle;
	s32 i = (s32) pool->GetSize();
	C = 0;
	
	CVehicle	*pVehToStore[MAX_NUM_STORED_CARS_IN_SAFEHOUSE];
	float		distVehToStore[MAX_NUM_STORED_CARS_IN_SAFEHOUSE];

	for (s32 j = 0; j < MAX_NUM_STORED_CARS_IN_SAFEHOUSE; j++)
	{
		pVehToStore[j] = NULL;
		distVehToStore[j] = 99999.9f; 
	}

	const u32 MaxCarsWeExpectToEverFitInAGarage = 10;
	CVehicle *pCarsToRemove[MaxCarsWeExpectToEverFitInAGarage];

	u32 numberOfCarsFoundInGarage = 0;

	while(i--)
	{
		pVehicle = pool->GetSlot(i);
		if (pVehicle)
		{
			if (CanThisVehicleBeStoredInThisGarage(pVehicle))
			{
				float distToCentre = (VEC3V_TO_VECTOR3(pVehicle->GetTransform().GetPosition()) - GarageCentre).Mag();

				if (distToCentre < distVehToStore[0])
				{
					pVehToStore[1] = pVehToStore[0];
					distVehToStore[1] = distVehToStore[0];
					pVehToStore[0] = pVehicle;
					distVehToStore[0] = distToCentre;
				}
				else if (distToCentre < distVehToStore[1])
				{
					pVehToStore[1] = pVehicle;
					distVehToStore[1] = distToCentre;
				}

				if (numberOfCarsFoundInGarage < MaxCarsWeExpectToEverFitInAGarage)
				{
					pCarsToRemove[numberOfCarsFoundInGarage++] = pVehicle;
				}
			}
		}
	}
	for (s32 j = 0; j < MAX_NUM_STORED_CARS_IN_SAFEHOUSE; j++)
	{
		if (pVehToStore[j])
		{
            if(Verifyf(!pVehToStore[j]->IsNetworkClone(), "CGarage::StoreAndRemoveCarsForThisHideOut - Graeme - just checking if the game can ever reach here for a network clone"))
            {
			    aStoredCars[C].StoreVehicle(pVehToStore[j]);
			    C++;
            }
		}
	}

	// Fill in the rest with zeros
	while (C < MAX_NUM_STORED_CARS_IN_SAFEHOUSE)
	{
		aStoredCars[C].Clear();
		C++;
	}

	if (bRemoveCars)
	{	//	Remove the cars we stored (up to 2) and the excess cars in the garage that we didn't store
		for (u32 removeLoop = 0; removeLoop < numberOfCarsFoundInGarage; removeLoop++)
		{
			CVehicleFactory::GetFactory()->Destroy(pCarsToRemove[removeLoop]);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : RestoreCarsForThisHideOut
// PURPOSE :  
// RETURNS :  true if all the cars have been generated.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::RestoreCarsForThisHideOut(CStoredVehicle *aStoredCars)
{
	s32		C;
	CVehicle	*pVehicle;

	bool physicsLoaded = true;
	bool modelsLoaded = true;
	for (C = 0; C < MAX_NUM_STORED_CARS_IN_SAFEHOUSE; C++)
	{
		// We have to request the appropriate cars and wait for them to be loaded first
		if (aStoredCars[C].IsUsed())
		{
			fwModelId vehicleModelId;
			u32 vehicleHash = aStoredCars[C].GetModelHashKey();

			if(!CModelInfo::GetBaseModelInfoFromHashKey(vehicleHash, &vehicleModelId)) // we need to check this for DLC vehicles
			{
				Warningf("CGarage::RestoreCarsForThisHideOut vehicle with model hash 0x%X doesn't exist! - > Removing reference to it in savegame", vehicleHash);
				aStoredCars[C].Clear();
				continue;
			}

			bool assetsLoaded =  CModelInfo::HaveAssetsLoaded(vehicleModelId);
			if (!assetsLoaded)
			{
				CModelInfo::RequestAssets(vehicleModelId, STRFLAG_FORCE_LOAD | STRFLAG_INTERIOR_REQUIRED);
				modelsLoaded = false;
				aStoredCars[C].m_bInteriorRequired = true;				
			}

			bool testPending = false;
			switch (aStoredCars[C].m_collisionTestResult.GetResultsStatus())
			{
			case WorldProbe::WAITING_ON_RESULTS:
				physicsLoaded = false;
				testPending = true;
				break;
			case WorldProbe::RESULTS_READY:
				physicsLoaded = aStoredCars[C].m_collisionTestResult.GetNumHits() > 0;
				if (!physicsLoaded)
				{
					aStoredCars[C].m_collisionTestResult.Reset();
				}
				testPending = true;					// Vital, or the next vehicle will never be tested, which will cause the spawn to fail
				break;
			case WorldProbe::TEST_NOT_SUBMITTED: // fall through, don't do anything and re-launch a test each interval..
			case WorldProbe::SUBMISSION_FAILED:
			case WorldProbe::TEST_ABORTED:
			default:
				physicsLoaded = false;
				break;
			}

			if (CGarages::timeForNextCollisionTest < fwTimer::GetTimeInMilliseconds() && !testPending)
			{
				const Vector3 carPos(aStoredCars[C].m_CoorX, aStoredCars[C].m_CoorY, aStoredCars[C].m_CoorZ);
				WorldProbe::CShapeTestProbeDesc probeDesc;
				probeDesc.SetResultsStructure(&aStoredCars[C].m_collisionTestResult);
				const float extent = 3.0f;
				probeDesc.SetStartAndEnd(carPos + Vector3(extent, extent, extent), carPos + Vector3(-extent, -extent, -extent));
				probeDesc.SetExcludeEntity(NULL);
				probeDesc.SetIncludeFlags(ArchetypeFlags::GTA_ALL_MAP_TYPES);

				WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_ASYNCHRONOUS_TEST);

				CGarages::timeForNextCollisionTest = fwTimer::GetTimeInMilliseconds() + 2000;
			}
		}
	}

	if (physicsLoaded && modelsLoaded)
	{
		for (C = 0; C < MAX_NUM_STORED_CARS_IN_SAFEHOUSE; C++)
		{
			if (aStoredCars[C].IsUsed())
			{
				pVehicle = aStoredCars[C].RestoreVehicle();

				if(aStoredCars[C].m_bInteriorRequired)
				{
					fwModelId vehicleModelId;
					u32 vehicleHash = aStoredCars[C].GetModelHashKey();

					CModelInfo::GetBaseModelInfoFromHashKey(vehicleHash, &vehicleModelId);		//	ignores return value
					CModelInfo::ClearAssetRequiredFlag(vehicleModelId, STRFLAG_INTERIOR_REQUIRED);					
					aStoredCars[C].m_bInteriorRequired = false;
				}

				Assert(pVehicle);
				if (pVehicle)
				{
					aStoredCars[C].Clear();
				}
			}
		}
		return true;		// done reviving this garage
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CameraShouldBeOutside
// PURPOSE :  If any garage is doing stuff (opening, closing etc) the camera should
//			  be outside of the garage.
/////////////////////////////////////////////////////////////////////////////////

CGarage *CGarages::GarageCameraShouldBeOutside()
{
	return pGarageCamShouldBeOutside;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TidyUpGarage
// PURPOSE :  Removes stuff that can potentially block the car. (Wrecks etc)
//			  Should only be called if the player cannot see the garage.
/////////////////////////////////////////////////////////////////////////////////

void CGarage::TidyUpGarage()
{
	CVehicle::Pool *pool = CVehicle::GetPool();
	CVehicle* pVeh;
	s32 i = (s32) pool->GetSize()-1;
	
	while (i)
	{
		pVeh = pool->GetSlot(i);
		if (pVeh && (pVeh->InheritsFromAutomobile() || pVeh->InheritsFromBike()))
		{
			if (IsPointInsideGarage(VEC3V_TO_VECTOR3(pVeh->GetTransform().GetPosition())))
			{
				// If this is a wreck we'll take it away
				if (pVeh->GetStatus() == STATUS_WRECKED || pVeh->GetTransform().GetC().GetZf() < 0.5f)
				{
					if (pVeh->CanBeDeleted())
					{
	                    if (pVeh->GetNetworkObject()==0 || (pVeh->GetNetworkObject()->CanDelete() && !pVeh->IsNetworkClone()))
		                {
						    CGameWorld::Remove(pVeh);
						    CVehicleFactory::GetFactory()->Destroy(pVeh);
						}
                    }
				}
			}
		}
		i--;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TidyUpGarageClose
// PURPOSE :  Removes stuff that can potentially block the car. (Wrecks etc)
//			  This version will be called even when player is close.
//            Emergency stuff.
/////////////////////////////////////////////////////////////////////////////////

void CGarage::TidyUpGarageClose()
{
	CVehicle::Pool	*pool = CVehicle::GetPool();
	CVehicle*		pVeh;
	s32			i = (s32) pool->GetSize()-1;
	Vector3			Center;
	bool			bRemove;
	static			u32	missionVehCount = 0;		// Fudgy counter to make sure mission vehicles don't get removed the second they explode.

	while (i)
	{
		pVeh = pool->GetSlot(i);
			// Wrecks should be removed asap since the player cannot always move them himself.
		if (pVeh && (pVeh->InheritsFromAutomobile() || pVeh->InheritsFromBike()) && (pVeh->GetStatus() == STATUS_WRECKED) )
		{
			if (IsEntityTouching3D(pVeh))
			{
				// Make sure we have at least one sphere outside garage
				bRemove = false;
				
				if (state == GSTATE_CLOSED)
				{
					bRemove = true;
				}
				else if (EntityHasASphereWayOutsideGarage(pVeh))
				{				
					bRemove = true;
				}
				
				if (pVeh->GetNetworkObject() && (!pVeh->GetNetworkObject()->CanDelete() || pVeh->IsNetworkClone()))
				{
					bRemove = false;
				}

                if (bRemove)
				{
					if (pVeh->CanBeDeleted())
					{
						CGameWorld::Remove(pVeh);
						CVehicleFactory::GetFactory()->Destroy(pVeh);
					}
					else
					{
						missionVehCount++;		// Mission vehicles don't get removed straight away.
						if (missionVehCount > 100)
						{
							CGameWorld::Remove(pVeh);
							CVehicleFactory::GetFactory()->Destroy(pVeh);
							missionVehCount = 0;
						}
					}
				}
			}
		}
		i--;
	}
}



/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlayerArrestedOrDied
// PURPOSE :  Takes appropriate steps if the player is arrested or has died.
/////////////////////////////////////////////////////////////////////////////////

void CGarages::PlayerArrestedOrDied(void)
{
	s32	Index;

	for (Index = 0; Index < aGarages.GetCount(); Index++)
	{
		if (aGarages[Index].type != GARAGE_NONE)
		{
			aGarages[Index].PlayerArrestedOrDied();
		}
	}
	
	CloseHideOutGaragesBeforeSave(true);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlayerArrestedOrDied
// PURPOSE :  Takes appropriate steps if the player is arrested or has died for
//			  this garage.
/////////////////////////////////////////////////////////////////////////////////

void CGarage::PlayerArrestedOrDied(void)
{
	switch(type)
	{
			// Garages that should be open
		case GARAGE_RESPRAY:
			switch(state)
			{
				case GSTATE_OPEN:
					break;
				case GSTATE_OPENING:
				case GSTATE_CLOSING:
				case GSTATE_CLOSED:
					SetState(GSTATE_OPENING);
					break;
				default:
					break;
			}
			break;
			// Garages that should be closed
		case GARAGE_MISSION:
		case GARAGE_FOR_SCRIPT_TO_OPEN_AND_CLOSE:
		case GARAGE_AMBIENT_PARKING:
	
			switch(state)
			{
				case GSTATE_CLOSED:
					break;
				case GSTATE_OPENING:
				case GSTATE_CLOSING:
				case GSTATE_OPEN:
					SetState(GSTATE_CLOSING);
					break;
				default:
					break;
			}
			break;
		case GARAGE_SAFEHOUSE_PARKING_ZONE:
			// Do nothing
			break;
		default:
			Assert(0);
			break;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPointInsideGarage
// PURPOSE :  Tests whether this point is inside a garage.
/////////////////////////////////////////////////////////////////////////////////

bool CGarage::IsPointInsideGarage(const Vector3& Point, int boxIndex)
{
	return IsPointInsideGarage(Point, 0.f, boxIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPointInsideGarage
// PURPOSE :  Tests whether this point is inside a garage.
//			  A positive margin makes it easier for the point to fix (garage bigger)
//			  A negative margin makes it harder (smaller garage or radius)
/////////////////////////////////////////////////////////////////////////////////
bool CGarage::IsPointInsideGarage(const Vector3& Point, float Margin, int boxIndex)
{	
	int startIndex;
	int count;
	if (boxIndex == ALL_BOXES_INDEX)
	{
		startIndex = 0;
		count = m_boxes.GetCount();
	}
	else
	{
		startIndex = boxIndex;
		count = boxIndex + 1;
	}

	for (int i = startIndex; i < count; ++i)
	{
		Box &box = m_boxes[i];
		if ((Point.z < box.BaseZ - Margin) || (Point.z > box.CeilingZ + Margin))
		{	
			continue;
		}

		float Delta1Length = rage::Sqrtf(box.Delta1X * box.Delta1X + box.Delta1Y * box.Delta1Y);

		Assertf(Delta1Length > 0.1f, "Garage corner points of Box %d are identical", i);

		float invDelta1Length = 1.0f / Delta1Length;
		float normDelta1X = box.Delta1X * invDelta1Length;
		float normDelta1Y = box.Delta1Y * invDelta1Length;

		float DotProd = (Point.x - box.BaseX) * normDelta1X + (Point.y - box.BaseY) * normDelta1Y;
		if ((DotProd < -Margin) || (DotProd > Delta1Length + Margin))
		{
			continue;	
		}

		float Delta2Length = rage::Sqrtf(box.Delta2X * box.Delta2X + box.Delta2Y * box.Delta2Y);

		Assertf(Delta1Length > 0.1f, "Garage corner points of Box %d are identical", i);

		float invDelta2Length = 1.0f / Delta2Length;
		float normDelta2X = box.Delta2X * invDelta2Length;
		float normDelta2Y = box.Delta2Y * invDelta2Length;

		DotProd = (Point.x - box.BaseX) * normDelta2X + (Point.y - box.BaseY) * normDelta2Y;
		if ((DotProd < -Margin) || (DotProd > Delta2Length + Margin))
		{
			continue;
		}

		return true;
	}

	return false;
}
////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CalcCentrePointOfGarage
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

void CGarage::CalcCentrePointOfGarage(Vector3& result)
{
	// For the moment just use box 0 as the Garage and log a warning out if theres more than 1
	Box &box0 = m_boxes[0]; 
	result = Vector3(box0.BaseX, box0.BaseY, box0.BaseZ);
	result.x += 0.5f * box0.Delta1X + 0.5f * box0.Delta2X;
	result.y += 0.5f * box0.Delta1Y + 0.5f * box0.Delta2Y;
	result.z = (box0.BaseZ + box0.CeilingZ) * 0.5f;
#if __DEV
	if (m_boxes.GetCount() > 1)
	{
		Warningf("CalcCentrePointOfGarage only uses box 0 to calculate the centre point");
	}
#endif
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CloseHideOutGaragesBeforeSave
// PURPOSE :  Closes hideout garages that are open so that cars will be saved.
/////////////////////////////////////////////////////////////////////////////////

void CGarages::CloseHideOutGaragesBeforeSave(bool bByGameLogic)
{
	if (!NetworkInterface::IsGameInProgress())		// Parking spots outside safehouse only work in single player.
	{
		// Go through the garages
		for (s32 Index = 0; Index < aGarages.GetCount(); Index++)
		{
			if ( (aGarages[Index].type == GARAGE_SAFEHOUSE_PARKING_ZONE) && (aGarages[Index].Flags.bSavingVehiclesEnabled) )
			{		// If this is one of the hideout garages
				if (aGarages[Index].GetState() != GSTATE_CLOSED)
				{
					aGarages[Index].StoreAndRemoveCarsForThisHideOut(&CGarages::aCarsInSafeHouse[aGarages[Index].safeHouseIndex][0], bByGameLogic);
					aGarages[Index].state = GSTATE_CLOSED;
					aGarages[Index].Flags.bSafehouseGarageWasOpenBeforeSave = true;
				}
				else
				{
					aGarages[Index].Flags.bSafehouseGarageWasOpenBeforeSave = false;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : OpenHideOutGaragesAfterSave
// PURPOSE :  Open hideout garages that were open before. Cars should still sit on top.
/////////////////////////////////////////////////////////////////////////////////

void CGarages::OpenHideOutGaragesAfterSave()
{

	s32	Index;

	// Go through the garages
	for (Index = 0; Index < aGarages.GetCount(); Index++)
	{
		if (aGarages[Index].type == GARAGE_SAFEHOUSE_PARKING_ZONE)
		{		// If this is one of the hideout garages
			if (aGarages[Index].Flags.bSafehouseGarageWasOpenBeforeSave)
			{
				aGarages[Index].state = GSTATE_OPEN;
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CloseSafeHouseGarages
// PURPOSE :  Closes hideout garages that are open so that cars will be saved.
/////////////////////////////////////////////////////////////////////////////////
void CGarages::CloseSafeHouseGarages()
{
	// Go through the garages
	for (s32 Index = 0; Index < aGarages.GetCount(); Index++)
	{
		if ( (aGarages[Index].type == GARAGE_SAFEHOUSE_PARKING_ZONE) && (aGarages[Index].Flags.bSavingVehiclesEnabled) )
		{		// If this is one of the hideout garages
			if (aGarages[Index].GetState() != GSTATE_CLOSED)
			{
				aGarages[Index].StoreAndRemoveCarsForThisHideOut(&CGarages::aCarsInSafeHouse[aGarages[Index].safeHouseIndex][0], true);
				aGarages[Index].state = GSTATE_CLOSED;
			}
		}
	}
}
/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindMaxNumStoredCarsForGarage
// PURPOSE :  Works out how many cars could possible 
/////////////////////////////////////////////////////////////////////////////////

s32 CGarage::FindMaxNumStoredCarsForGarage()
{
	return (MAX_NUM_STORED_CARS_IN_SAFEHOUSE);
}





/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : StopCarFromBlowingUp
// PURPOSE :  Make sure the car is not about to blow up
/////////////////////////////////////////////////////////////////////////////////

void CGarages::StopCarFromBlowingUp(CAutomobile *pCar)
{
	pCar->SetHealth(MAX(pCar->GetHealth(), CAR_ON_FIRE_HEALTH+50));
	pCar->GetVehicleDamage()->ResetEngineHealth();
	pCar->GetVehicleDamage()->ResetPetrolTankHealth();
	//pCar->Damage.SetEngineStatus(MAX(DT_ENGINE_ON_FIRE+50, pCar->Damage.GetEngineStatus()));
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Returns true if this vehicle is within a save garage. (Car should not be deleted)
// PURPOSE :  Does the same checks as CGarage::Update() and StoreAndRemoveCarsForThisHideOut()
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsVehicleWithinHideOutGarageThatCanSaveIt(const CVehicle *pVehicle)
{
	if (NetworkInterface::IsGameInProgress() REPLAY_ONLY(|| CReplayMgr::IsClearingWorldForReplay()))
	{	// Parking spots outside safehouse only work in single player.
		return false;
	}

	if (pVehicle)
	{
		for (s32 Garage = 0; Garage < aGarages.GetCount(); Garage++)
		{
			CGarage &thisGarage = aGarages[Garage];
			if ( (thisGarage.type == GARAGE_SAFEHOUSE_PARKING_ZONE) && (thisGarage.Flags.bSavingVehiclesEnabled) )
			{
				if (thisGarage.CanThisVehicleBeStoredInThisGarage(pVehicle))
				{
					return true;
				}
			}
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : Returns true if this coordinate is within a garage.
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsPointWithinAnyGarage(const Vector3 &Point)
{
	s32	Garage;
	
	for (Garage = 0; Garage < aGarages.GetCount(); Garage++)
	{
		switch(aGarages[Garage].type)
		{
			default:
				if (aGarages[Garage].IsPointInsideGarage(Point))
				{
					return true;
				}
				break;
			case GARAGE_NONE:
				break;
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AllRespraysCloseOrOpen
// PURPOSE :  Goes through all resprays and closes or opens them.
/////////////////////////////////////////////////////////////////////////////////

void CGarages::AllRespraysCloseOrOpen(bool bOpen)
{
	if (bOpen)
	{
		FindPlayerPed()->GetPlayerInfo()->EnableControlsGarages();
	}

	for (u32 Garage = 0; Garage < aGarages.GetCount(); Garage++)
	{
		if (aGarages[Garage].type == GARAGE_RESPRAY)
		{
			if (!aGarages[Garage].m_pDoorObject)
			{
				if (bOpen)
				{
					aGarages[Garage].state = GSTATE_OPEN;
				}
				else
				{
					aGarages[Garage].state = GSTATE_CLOSED;			
				}
			}
			else
			{
				if (aGarages[Garage].state == GSTATE_CLOSED)	// We have to do this here or the garage will repair the players car when SET_NO_RESPRAYS FALSE happens
				{
					aGarages[Garage].state = GSTATE_OPENING;
				}
			}
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AttachDoorToGarage
// PURPOSE :  Gets called when an door object is given physics.
//			  If there is a garage that could use this door we attach the object
//			  to that garage and the garage can control the door.
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::AttachDoorToGarage(CDoor* pObj)
{
	for (s32 g = 0; g < aGarages.GetCount(); g++)
	{
		CGarage &theGarage = aGarages[g];

		// Only connect doors to enclosed garages, everything else remains as before
		if( theGarage.m_IsEnclosed )
		{
			if (theGarage.IsPointInsideGarage(VEC3V_TO_VECTOR3(pObj->GetTransform().GetPosition()), 3.0f))
			{
				Assert(!theGarage.m_pDoorObject);

				theGarage.m_pDoorObject = pObj;

/*				// Tell the door to be open or closed depending on the state of the garage. This will also cancel their
				// default door behaviour.
				switch (theGarage.GetState())
				{
					case GSTATE_OPEN:
					case GSTATE_OPENING:
					case GSTATE_OPENEDWAITINGTOBEREACTIVATED:
						theGarage.SlideDoorOpen();
						break;
					case GSTATE_CLOSED:
					case GSTATE_CLOSING:
					case GSTATE_MISSIONCOMPLETED:
					default:
						theGarage.SlideDoorClosed();
						break;
				}
*/
				return true;
			}
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPaynSprayActive
// PURPOSE :  Returns true if any of the respray shops is opening/closing or repairing the players car.
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsPaynSprayActive()
{
	if (NoResprays)
	{
		return false;
	}

	for (s32 i = 0; i < aGarages.GetCount(); i++)
	{
		if (aGarages[i].type == GARAGE_RESPRAY)
		{
			if (aGarages[i].m_pDoorObject)
			{
				if (aGarages[i].state != GSTATE_OPEN)
				{
					return true;
				}
			}
		}
	}
	return false;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : PlayerIsInteractingWithGarage
// PURPOSE :  For the script to use to determine whether the player is interacting with a garage.
//			  If so certain cut scenes should not kick off.
/////////////////////////////////////////////////////////////////////////////////

bool	CGarages::PlayerIsInteractingWithGarage()
{
	if (IsPaynSprayActive())
	{
		return true;
	}

	if (CGarages::GarageCameraShouldBeOutside())
	{
		return true;
	}
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : InitWidgets
// PURPOSE :  
/////////////////////////////////////////////////////////////////////////////////
#if __BANK
void CGarages::InitWidgets()
{
	bkBank *bank = CGameLogic::GetGameLogicBank();
	bank->PushGroup("Garages");
		bank->AddToggle("Render debug", &bDebugDisplayGarages);
		bank->AddToggle("Alternate garage occupancy check", &bAlternateOccupantSearch);

		CGarageEditor::InitWidgets(*bank);
	bank->PopGroup();
}
#endif // __BANK


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsPlayerPartialyInsideGarage
// PURPOSE :  For script to check if player is partially inside garage
/////////////////////////////////////////////////////////////////////////////////
bool CGarages::IsPlayerPartiallyInsideGarage(s32 garageIndex, const CEntity *pEntity, int boxIndex)
{
	Assertf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex);
	Assertf(boxIndex >= CGarage::ALL_BOXES_INDEX && boxIndex < aGarages[garageIndex].m_boxes.GetCount(), "Invalid Box Index %d for Garage %d", boxIndex, garageIndex);

	CGarage *pGarage = &aGarages[garageIndex];

	Assertf(pEntity->GetBaseModelInfo()->GetModelType() == MI_TYPE_PED, "Player Model Type is not MI_TYPE_PED, %d", pEntity->GetBaseModelInfo()->GetModelType());
	const CPed *pPlayer = static_cast<const CPed*>(pEntity);

	if (pPlayer->GetPedConfigFlag(CPED_CONFIG_FLAG_InVehicle))
	{
		CVehicle *pVehicle = pPlayer->GetMyVehicle();
		if (Verifyf(pVehicle, "IsPlayerPartiallyInsideGarage ... Ped [%s] has no vehicle but has config flag CPED_CONFIG_FLAG_InVehicle", pPlayer->GetModelName()))
		{
			return pGarage->IsEntityTouching3D(pVehicle, boxIndex) && !pGarage->IsEntityEntirelyInside3D(pVehicle, 0.0f, boxIndex);
		}
	}

	return pGarage->IsEntityTouching3D(pPlayer, boxIndex) && !pGarage->IsEntityEntirelyInside3D(pPlayer, 0.0f, boxIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AreEntitiesEntirelyInsideGarage
// PURPOSE :  returns true if all entities of the specified type(s) are fully inside the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::AreEntitiesEntirelyInsideGarage(s32 garageIndex, bool peds, bool vehs, bool objs, float margin, int boxIndex)
{
	if (!Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
		return true;

	CGarage *pGarage = &aGarages[garageIndex];
	return pGarage->AreEntitiesEntirelyInsideGarage(peds, vehs, objs, margin, boxIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsAnyEntitiesEntirelyInsideGarage
// PURPOSE :  returns true if any entities of the specified type(s) are fully inside the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsAnyEntityEntirelyInsideGarage(s32 garageIndex, bool peds, bool vehs, bool objs, float margin, int boxIndex)
{
	if (!Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
		return true;

	CGarage *pGarage = &aGarages[garageIndex];
	return pGarage->IsAnyEntityEntirelyInsideGarage(peds, vehs, objs, margin, boxIndex);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsLineIntersectGarage
// PURPOSE :  returns true the line intersects the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsLineIntersectingGarage(s32 garageIndex, const Vector3& start, const Vector3 &end, int boxIndex )
{
	if (!Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
		return true;

	CGarage *pGarage = &aGarages[garageIndex];
	return pGarage->IsLineIntersectingGarage(start, end, boxIndex);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsLineIntersectGarage
// PURPOSE :  returns true the line intersects the garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsBoxIntersectingGarage(s32 garageIndex, const Vector3 &p0, const Vector3 &p1, const Vector3 &p2, const Vector3 &p3, int boxIndex )
{
	if (!Verifyf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex))
		return true;

	CGarage *pGarage = &aGarages[garageIndex];
	return pGarage->IsBoxIntersectingGarage(p0, p1, p2, p3, boxIndex);
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : IsObjectPartialyInsideGarage
// PURPOSE :  For script to check if object is partially inside garage
/////////////////////////////////////////////////////////////////////////////////

bool CGarages::IsObjectPartiallyInsideGarage(s32 garageIndex, const CEntity *pEntity, int boxIndex)
{
	Assertf(garageIndex >= 0 && garageIndex < aGarages.GetCount(), "Invalid Garage Index %d", garageIndex);
	Assertf(boxIndex >= CGarage::ALL_BOXES_INDEX && boxIndex < aGarages[garageIndex].m_boxes.GetCount(), "Invalid Box Index %d for Garage %d", boxIndex, garageIndex);

	CGarage *pGarage = &aGarages[garageIndex];
	return pGarage->IsEntityTouching3D(pEntity, boxIndex) && !pGarage->IsEntityEntirelyInside3D(pEntity, 0.0f, boxIndex);
}

void CGarage::FindAllObjectsInGarage(atArray<CEntity*> &objects, bool checkPeds, bool checkVehicles, bool checkObjects)
{
	s32	Num, C;
	const int maxEntites = 32;
	CEntity	*apEnts[maxEntites];

 	CGameWorld::FindObjectsIntersectingCube(Vector3(MinX, MinY, MinZ), Vector3(MaxX, MaxY, MaxZ), &Num, maxEntites, apEnts, 
											false, checkVehicles, checkPeds, checkObjects, false);
 
 	// Go through the entities found and test whether any of them really are touching the garage
 	for (C = 0; C < Num; C++)
 	{
 		if (IsObjectEntirelyInsideGarage(apEnts[C]))
 		{
 			objects.PushAndGrow(apEnts[C]);
 		}
 	}
}

/////////////////////////////////////////////////////////////////////////////////
// NETWORK FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////

void CGarage::NetworkInit(unsigned UNUSED_PARAM(garageIndex))
{
	for (u32 i=0; i<m_boxesOccupiedForPlayers.GetCount(); i++)
	{
		m_boxesOccupiedForPlayers[i] = 0;
	}

	m_lastBoxesOccupied = 0;
	m_bBoxesOccupiedHasChanged = false;
}

void CGarage::NetworkUpdate(unsigned garageIndex)
{
	// broadcast changed occupied state if necessary
	if (NetworkInterface::IsGameInProgress())
	{
		if (m_bBoxesOccupiedHasChanged)
		{
			CGarageOccupiedStatusEvent::Trigger(garageIndex, m_lastBoxesOccupied);
			
			m_bBoxesOccupiedHasChanged = false;
		}
	}
}

void CGarage::UpdateOccupiedBoxes(bool alternateSearch)
{
	// check for changes in the occupied status of the garage - network only
	if (NetworkInterface::IsGameInProgress())
	{
		BoxFlags currBoxesOccupied = 0;		
		for (u32 i=0; i<m_boxes.GetCount(); i++)
		{
			if (!IsGarageEmpty(i, alternateSearch))
			{
				currBoxesOccupied |= (1<<i);
			}
		}		

		if (currBoxesOccupied != m_lastBoxesOccupied)
		{
			m_bBoxesOccupiedHasChanged = true; // we need to send out this status to players
			m_lastBoxesOccupied = currBoxesOccupied;
		}
	}
}

bool CGarage::IsGarageEmptyOnAllMachines(int boxIndex)
{
	if (NetworkInterface::IsGameInProgress())
	{
		if (boxIndex == ALL_BOXES_INDEX)
		{
			for (u32 i=0; i<m_boxes.GetCount(); i++)
			{
				if (m_boxesOccupiedForPlayers[i] != 0)
				{
					return false;
				}
			}
		}
		else if (Verifyf(boxIndex >= 0 && boxIndex < m_boxesOccupiedForPlayers.GetCount(), "Invalid Box Index %d", boxIndex))
		{
			if (m_boxesOccupiedForPlayers[boxIndex] != 0)
			{
				return false;
			}
		}
	}

	return IsGarageEmpty(boxIndex);
}


void CGarage::ReceiveOccupiedStateFromPlayer(PhysicalPlayerIndex playerIndex, BoxFlags boxesOccupied)
{
	for (u32 i=0; i<m_boxes.GetCount(); i++)
	{
		if (boxesOccupied & (1<<i))
		{
			m_boxesOccupiedForPlayers[i] |= (1<<playerIndex);
		}
		else
		{
			m_boxesOccupiedForPlayers[i] &= ~(1<<playerIndex);
		}
	}
}

void CGarage::PlayerHasLeft(PhysicalPlayerIndex playerIndex)
{
	for (u32 i=0; i<m_boxes.GetCount(); i++)
	{
		m_boxesOccupiedForPlayers[i] &= ~(1<<playerIndex);
	}
}

void CGarage::PlayerHasJoined(PhysicalPlayerIndex playerIndex, unsigned garageIndex)
{
	if (m_lastBoxesOccupied)
	{
		CGarageOccupiedStatusEvent::Trigger(garageIndex, m_lastBoxesOccupied, playerIndex);
	}
}

void CGarages::NetworkInit()
{
	for (u32 i=0; i<aGarages.GetCount(); i++)
	{
		aGarages[i].NetworkInit(i);
	}
}

void CGarages::PlayerHasLeft(PhysicalPlayerIndex playerIndex)
{
	for (u32 i=0; i<aGarages.GetCount(); i++)
	{
		aGarages[i].PlayerHasLeft(playerIndex);
	}
}

void CGarages::PlayerHasJoined(PhysicalPlayerIndex playerIndex)
{
	for (u32 i=0; i<aGarages.GetCount(); i++)
	{
		aGarages[i].PlayerHasJoined(playerIndex, i);
	}
}
