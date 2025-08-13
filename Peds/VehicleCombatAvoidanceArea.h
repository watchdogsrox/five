#ifndef VEHICLE_COMBAT_AVOIDANCE_AREA_H
#define VEHICLE_COMBAT_AVOIDANCE_AREA_H

// Rage headers
#include "fwtl/pool.h"
#include "fwutil/Flags.h"

// Game headers
#include "Peds/Area.h"

//-------------------------------------------------------------------------
// Defines a vehicle avoidance area used during combat
//-------------------------------------------------------------------------
class CVehicleCombatAvoidanceArea : public CArea
{

public:

	CVehicleCombatAvoidanceArea();
	virtual ~CVehicleCombatAvoidanceArea();

	FW_REGISTER_CLASS_POOL(CVehicleCombatAvoidanceArea);

public:

	static s32	Add(Vec3V_In vStart, Vec3V_In vEnd, const float fWidth);
	static s32	AddSphere(Vec3V_In vCenter, const float fRadius);
	static void	Clear();
	static void	Init(unsigned initMode);
	static bool	IsPointInAreas(Vec3V_In vPosition);
	static void	Remove(const s32 iIndex);
	static void	Shutdown(unsigned shutdownMode);

#if __BANK
	static void Debug();
#endif

private:

#if __BANK
	static bool ms_bRender;
#endif

};

#endif //VEHICLE_COMBAT_AVOIDANCE_AREA_H
