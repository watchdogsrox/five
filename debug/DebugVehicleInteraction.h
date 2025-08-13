#ifndef DEBUG_VEHICLE_INTERACTION_H 
#define DEBUG_VEHICLE_INTERACTION_H 

#include "physics/debugPlayback.h"
#if DR_ENABLED
#include "peds/ped.h"
#include "vehicles/vehicle.h"

class CComponentReservation;

namespace rage
{
	namespace debugPlayback
	{	
		void RecordPedSetIntoVehicle(const CPed& rPed, const CVehicle& rVeh, s32 iFlags);
		void RecordPedSetOutOfVehicle(const CPed& rPed, const CVehicle* pVeh, s32 iFlags);
		void RecordPedDetachFromVehicle(const CPed& rPed, const CVehicle* pVeh, s32 iFlags);
		void RecordComponentReservation(const CComponentReservation* pComponentReservation, bool bPreUpdate);
		void RecordReserveUseOfComponent(const CComponentReservation* pComponentReservation, const CPed& rPed);
		void RecordKeepUseOfComponent(const CComponentReservation* pComponentReservation, const CPed& rPed);
		void RecordUnReserveUseOfComponent(const CComponentReservation* pComponentReservation, const CPed& rPed);
		void RecordUpdatePedUsingComponentFromNetwork(const CComponentReservation* pComponentReservation, const CPed* pPed);
	}
}

#endif //DR_ENABLED
#endif //DEBUG_VEHICLE_INTERACTION_H
