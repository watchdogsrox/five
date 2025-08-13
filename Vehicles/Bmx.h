// Title	:	Bmx.h
// Author	:	Ben Lyons
// Started	:	08/10/2001 (adapted from bike.h)
//
//This will be used for all pedal bikes but calling it bmx causes less confusion
//
//
#ifndef _BMX_H_
#define _BMX_H_

// Rage headers
#include "phCore/segment.h"
#include "physics/intersection.h"

// Game headers
#include "renderer\hierarchyIds.h"
#include "vehicles\handlingMgr.h"
#include "vehicles\vehicle.h"
#include "vehicles\wheel.h"

#include "network\objects\entities\netObjBike.h"

namespace rage { class CNodeAddress; }

class CBmx : public CBike
{
public:

	virtual void BlowUpCar(CEntity* pCulprit, bool bInACutscene=false, bool bAddExplosion=true, bool bNetCall=false, u32 weaponHash=WEAPONTYPE_EXPLOSION, bool bDelayedExplosion = false);

protected:
	// This is PROTECTED as only CVEHICLEFACTORY is allowed to call this - make sure you use CVehicleFactory!
	// DONT make this public!
	CBmx(const eEntityOwnedBy ownedBy, u32 popType);
	friend class CVehicleFactory;

public:
	~CBmx();

};

#endif//_BMX_H_
