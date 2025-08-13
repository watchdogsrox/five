#ifndef TASK_CAR_UTILS_H
#define TASK_CAR_UTILS_H

class CPed;
class CVehicle;
class CClipPlayer;
class CTask;
class CCarDoor;

//Game headers.
#include "Renderer/HierarchyIds.h"
#include "Task/System/TaskHelpers.h"
#include "Modelinfo/VehicleModelInfo.h"

//Rage headers.
#include "Vector/Vector3.h"

class CCarEnterExit
{

public:

    CCarEnterExit(){}
    ~CCarEnterExit(){}
    
    //Compute nearest door when getting in as driver.
    static bool GetNearestCarDoor(const CPed& ped, CVehicle& vehicle, Vector3& vDoorPos, int& iEntryPointIndex);
	//Compute nearest seat when getting in as driver.
	static bool GetNearestCarSeat(const CPed& ped, CVehicle& vehicle, Vector3& vSeatPos, int& iSeatIndex);
    //Compute if the door exists and isn't fully open.
    static bool CarHasDoorToOpen(CVehicle& vehicle, int iEntryPointIndex);
    //Compute if the door exists and isn't closed.
    static bool CarHasDoorToClose(CVehicle& vehicle, int iDoorBoneIndex);
    //Compute if a ped has permision to open a car door.
    static bool CarHasOpenableDoor(CVehicle& vehicle, int iDoorBoneIndex, const CPed& ped);
    //Compute if a car can be stolen bya ped.
    static bool IsVehicleStealable(const CVehicle& vehicle, const CPed& ped, const bool bIgnoreLawEnforcementVehicles = true, const bool bIgnoreVehicleHealth = false);
    //Compute if a car has enough space around it to drive.
    static bool IsClearToDriveAway(const CVehicle& vehicle);
    
    //Make all undragged peds leave the car
    static void MakeUndraggedPassengerPedsLeaveCar(const CVehicle& vehicle, const CPed* pDraggedPed, const CPed* pDraggingPed);
	//Make the driver leave the car
	static void MakeUndraggedDriverPedLeaveCar(const CVehicle& vehicle, const CPed& pedGettingIn);

	// Helper function to see which door we have collided with from the component index returned by the physics.
	// This includes the possibility that we've collided with the window set in the door & any other parts.
	static const CCarDoor * GetCollidedDoorFromCollisionComponent(const CVehicle * pVehicle, const int iHitComponent, const float fMinOpenThreshold=0.0f);

	static const CCarDoor * GetCollidedDoorBonnetBootFromCollisionComponent(const CVehicle * pVehicle, const int iHitComponent, const float fMinOpenThreshold=0.0f);

	static const CWheel *GetCollidedWheelFromCollisionComponent(const CVehicle * pVehicle, const int iHitComponent);

	// This returns the component indices within the vehicle's composite bound of all the parts of the]
	// given door.  This is the door itself, the window, and any other parts.
	// pComponentIndices must point to an array of at least MAX_DOOR_COMPONENTS in size
	static int GetDoorComponents(const CVehicle * pVehicle, const eHierarchyId iDoorId, int * pComponentIndices, const s32 iMaxNum=-1);

	// Given a bone index for a door, this will return the hierarchy ID for that door
	static eHierarchyId DoorBoneIndexToHierarchyID(const CVehicle * pVehicle, const s32 iBoneIndex);

	// This returns the component indices within the vehicle's composite bound of all the awkward
	// sticky-out parts of the heli.
	// pComponentIndices must point to an array of at least MAX_HELI_COMPONENTS in size
	static int GetHeliComponents(CVehicle * pVehicle, int * pComponentIndices, const eHierarchyId iDoorId=VEH_INVALID_ID);

	static bool CanDoorBePushedClosed(CVehicle * pVehicle, const CCarDoor * pDoor, const Vector3 & vPedPos, const Vector3 & vTargetPos);
   
private:

};

#endif
