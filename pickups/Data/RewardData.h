#ifndef REWARD_DATA_H
#define REWARD_DATA_H

// Rage headers
#include "atl/hashstring.h"
#include "data/base.h"
#include "fwtl/pool.h"
#include "parser/macros.h"

// Game includes
#include "pickups/Data/PickupIds.h"

class CPickup;
class CPed;

//////////////////////////////////////////////////////////////////////////
// CPickupRewardData
//////////////////////////////////////////////////////////////////////////

class CPickupRewardData : public rage::datBase
{
public:
	CPickupRewardData( )							{}

	u32 GetHash() const								{ return m_Name.GetHash(); }
#if !__FINAL
	const char * GetName() const					{ return m_Name.GetCStr(); }
#endif	//	!__FINAL

	// returns true if the reward can be given to the ped
	virtual bool CanGive(const CPickup*, const CPed*) const { return true; }

	// give the reward to the ped
	virtual void Give(const CPickup*, CPed*) const { Assertf(0, "No Give() defined for %s", GetName()); }

	// if this returns true the pickup will not be collected when CanGive() returns false
	virtual bool PreventCollectionIfCantGive(const CPickup*, const CPed*) const { return false; }

	// Get the type
	virtual PickupRewardType GetType() const { Assertf(0, "No GetType() defined for %s", GetName()); return PICKUP_REWARD_TYPE_NONE; }

	// Some vehicle pickup rewards can be passed on to other passengers in the vehicle
	virtual bool CanGiveToPassengers(const CPickup*) const { return false; }

protected:

	rage::atHashString	m_Name;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CPickupRewardData(const CPickupRewardData& other);
	CPickupRewardData& operator=(const CPickupRewardData& other);
};

#endif // REWARD_DATA_H
