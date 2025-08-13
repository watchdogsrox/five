#ifndef ACTION_DATA_H
#define ACTION_DATA_H

//Rage headers
#include "atl/hashstring.h"
#include "data/base.h"
#include "fwtl/pool.h"
#include "parser/macros.h"


class CPickup;
class CPickupDataManager;
class CPed;

//////////////////////////////////////////////////////////////////////////
// CPickupActionData
//////////////////////////////////////////////////////////////////////////

class CPickupActionData : public rage::datBase
{
public:
	// Construction
	CPickupActionData()							{ }

	// Destruction
	virtual ~CPickupActionData()				{ }

	// applies the action to the ped
	virtual void Apply(CPickup*, CPed*) const	{ Assertf(0, "No Apply() defined for %s", GetName()); }

	//
	// Accessors
	//

	// Get the item hash
	u32 GetHash() const							{ return m_Name.GetHash(); }

#if !__FINAL
	// Get the info name from the metadata manager
	const char* GetName() const					{ return m_Name.GetCStr(); }
#endif // !__FINAL

protected:
	rage::atHashString	m_Name;

	PAR_PARSABLE;

private:
	// Not allowed to copy construct or assign
	CPickupActionData(const CPickupActionData& other);
	CPickupActionData& operator=(const CPickupActionData& other);
};




#endif // PICKUP_DATA_H
