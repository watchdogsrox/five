
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    remote.cpp
// PURPOSE : Deals with remote controlled cars.
// AUTHOR :  Obbe.
// CREATED : 22/11/00
//
/////////////////////////////////////////////////////////////////////////////////
//Game headers
#include "control/remote.h"
#include "game/modelIndices.h"
#include "modelInfo/vehiclemodelinfo.h"
#include "peds/ped.h"
#include "physics/WorldProbe/worldprobe.h"
#include "scene/world/gameWorld.h"
#include "script/script.h"
#include "vehicles/heli.h"
#include "vehicles/planes.h"
#include "vehicles/vehicleFactory.h"
#include "debug/DebugScene.h"

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GivePlayerRemoteControlledCar
// PURPOSE :  Sets everything up for the player to have a remote controlled car
/////////////////////////////////////////////////////////////////////////////////

void CRemote::GivePlayerRemoteControlledCar(float UNUSED_PARAM(CoorX), float UNUSED_PARAM(CoorY), float UNUSED_PARAM(CoorZ), float UNUSED_PARAM(Orientation), s32 UNUSED_PARAM(nModelIndex))
{
	Assertf(0, "Remote controlled cars don't work at the moment. Talk to Obbe");


}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : TakeRemoteControlledCarFromPlayer
// PURPOSE :  Remote controlled car is about to explode. Do necessary stuff.
/////////////////////////////////////////////////////////////////////////////////

void CRemote::TakeRemoteControlledCarFromPlayer(bool UNUSED_PARAM(bCreateExplosion))
{
/*
	s32 VehiclePoolIndex;
	CPlayerInfo* pPlayerInfo = FindPlayerPed()->GetPlayerInfo();

	Assert(pPlayerInfo->GetRemoteVehicle());


	// Turn remote controlled vehicle into a random car so that it can be
	// taken away by the code.
	if (pPlayerInfo->GetRemoteVehicle()->GetVehPopType() == MISSION_VEHICLE)
	{
		pPlayerInfo->GetRemoteVehicle()->SetVehicleCreatedBy(RANDOM_VEHICLE);
		VehiclePoolIndex = CTheScripts::GetGUIDFromEntity(pPlayerInfo->GetRemoteVehicle());
		CMissionCleanup::GetInstance()->RemoveEntityFromList(VehiclePoolIndex, CLEANUP_CAR, NULL, CLEANUP_SOURCE_CODE);
	}
	pPlayerInfo->GetRemoteVehicle()->m_nVehicleFlags.bCannotRemoveFromWorld = false;

	pPlayerInfo->TimeOfRemoteVehicleExplosion = fwTimer::GetTimeInMilliseconds();
	pPlayerInfo->bAfterRemoteVehicleExplosion = true;
	pPlayerInfo->bCreateRemoteVehicleExplosion = bCreateExplosion;
	pPlayerInfo->bFadeAfterRemoteVehicleExplosion = true;


*/
}


void CRemote::TakeRemoteControlOfCar(CVehicle *UNUSED_PARAM(pVehicle))
{
Assertf(0, "Remote control stuff doesn't work at the moment (Obbe)");

}


