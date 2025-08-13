#ifndef PICKUP_ACTIONS_H
#define PICKUP_ACTIONS_H

// Rage Headers
#include "atl/hashstring.h"

// Game headers
#include "Pickups/Data/ActionData.h"

class CPed;

//////////////////////////////////////////////////////////////////////////
// CPickupActionAudio
//////////////////////////////////////////////////////////////////////////

class CPickupActionAudio : public CPickupActionData
{
public:
	CPickupActionAudio( ) : CPickupActionData() {}

	void Apply(CPickup* pPickup, CPed* pPed) const;

protected:
	rage::atHashString AudioRef;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupActionPadShake
//////////////////////////////////////////////////////////////////////////

class CPickupActionPadShake : public CPickupActionData
{
public:

	CPickupActionPadShake( ) : CPickupActionData() {}

	void Apply(CPickup* pPickup, CPed* pPed) const;

protected:
	rage::f32 Intensity;
	rage::s32 Duration;

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupActionVfx
//////////////////////////////////////////////////////////////////////////

class CPickupActionVfx : public CPickupActionData
{
public:

	CPickupActionVfx( ) : CPickupActionData() {}

	void Apply(CPickup* pPickup, CPed* pPed) const;

protected:
	char Vfx[32];

	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
// CPickupActionGroup
//////////////////////////////////////////////////////////////////////////

class CPickupActionGroup : public CPickupActionData
{
public:
	CPickupActionGroup( ) : CPickupActionData() {}

	void Apply(CPickup* pPickup, CPed* pPed) const;

protected:
	static const rage::u8 MAX_ACTIONSS = 10;
	atFixedArray<rage::atHashString, MAX_ACTIONSS> Actions;

	PAR_PARSABLE;
};

#endif // PICKUP_ACTIONS_H
