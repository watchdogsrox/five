
/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    remote.h
// PURPOSE : Deals with remote controlled cars.
// AUTHOR :  Obbe.
// CREATED : 22/11/00
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _REMOTE_H_
	#define _REMOTE_H_

#include "game/modelIndices.h"
#include "vehicles/automobile.h"
#include "vehicles/vehicle.h"


/////////////////////////////////////////////////////////////////////////////////
// A class to contain the functions.
/////////////////////////////////////////////////////////////////////////////////

class CRemote
{

public:
	static	void GivePlayerRemoteControlledCar(float CoorsX, float CoorsY, float CoorsZ, float Orientation, s32 nModelIndex=-1); //MODELID_CAR_REMOTE);

	static	void TakeRemoteControlledCarFromPlayer(bool bCreateExplosion = true);

	static	void TakeRemoteControlOfCar(CVehicle *pVehicle);
};



#endif
