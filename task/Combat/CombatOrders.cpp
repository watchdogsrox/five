// File header
#include "Task/Combat/CombatOrders.h"

AI_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////
// CCombatOrders
//////////////////////////////////////////////////////////////////////////

CCombatOrders::CCombatOrders()
{
	Clear();
}

CCombatOrders::~CCombatOrders()
{

}

void CCombatOrders::Clear()
{
	//Clear the attack range.
	ClearAttackRange();

	//Clear the modifiers.
	m_fShootRateModifier = 1.0f;
	m_fTimeBetweenBurstsModifier = 1.0f;
		
	//Clear the flags.
	GetActiveFlags().ClearAllFlags();
	GetPassiveFlags().ClearAllFlags();
}

bool CCombatOrders::HasActiveOrders() const
{
	return (GetActiveFlags() != 0);
}

bool CCombatOrders::HasPassiveOrders() const
{
	return (GetPassiveFlags() != 0);
}
