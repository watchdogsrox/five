#ifndef COMBAT_ORDERS_H
#define COMBAT_ORDERS_H

// Rage headers
#include "ai/aichannel.h"
#include "fwutil/Flags.h"

// Game headers
#include "Peds/CombatData.h"
#include "scene/RegdRefTypes.h"

//////////////////////////////////////////////////////////////////////////
// CCombatOrders
//////////////////////////////////////////////////////////////////////////

class CCombatOrders
{

public:

	enum ActiveFlags
	{
		AF_CoverFire	= BIT0,
	};
	
	enum PassiveFlags
	{
		PF_BlockCover	= BIT0,
		PF_HoldFire		= BIT1,
	};

private:

	enum RunningFlags
	{
		RF_HasAttackRange	= BIT0,
	};

public:
	
	CCombatOrders();
	~CCombatOrders();

public:
	
	const float GetShootRateModifier() const { return m_fShootRateModifier; }
	void SetShootRateModifier(const float fModifier) { m_fShootRateModifier = fModifier; }
	
	const float GetTimeBetweenBurstsModifier() const { return m_fTimeBetweenBurstsModifier; }
	void SetTimeBetweenBurstsModifier(const float fModifier) { m_fTimeBetweenBurstsModifier = fModifier; }
		
	fwFlags8& GetActiveFlags() { return m_uActiveFlags; }
	const fwFlags8& GetActiveFlags() const { return m_uActiveFlags; }
		
	fwFlags8& GetPassiveFlags() { return m_uPassiveFlags; }
	const fwFlags8& GetPassiveFlags() const { return m_uPassiveFlags; }

public:

	void ClearAttackRange()
	{
		m_nAttackRange = CCombatData::CR_Medium;

		m_uRunningFlags.ClearFlag(RF_HasAttackRange);
	}

	CCombatData::Range GetAttackRange() const
	{
		aiAssert(HasAttackRange());

		return m_nAttackRange;
	}

	bool HasAttackRange() const
	{
		return (m_uRunningFlags.IsFlagSet(RF_HasAttackRange));
	}

	void SetAttackRange(CCombatData::Range nRange)
	{
		m_uRunningFlags.SetFlag(RF_HasAttackRange);

		m_nAttackRange = nRange;
	}

public:
	
	void Clear();
	bool HasActiveOrders() const;
	bool HasPassiveOrders() const;
	
private:

	CCombatData::Range	m_nAttackRange;
	float				m_fShootRateModifier;
	float				m_fTimeBetweenBurstsModifier;
	fwFlags8			m_uActiveFlags;
	fwFlags8			m_uPassiveFlags;
	fwFlags8			m_uRunningFlags;
	
};

#endif // COMBAT_ORDERS_H
