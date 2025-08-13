#ifndef INC_AGITATED_TRIGGER_H
#define INC_AGITATED_TRIGGER_H

// Rage headers
#include "ai/aichannel.h"
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "string/string.h"
#include "system/bit.h"

// Game headers
#include "Peds/PedType.h"
#include "task/Response/Info/AgitatedEnums.h"

// Game forward declarations
class CPed;

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTriggerReaction
////////////////////////////////////////////////////////////////////////////////

class CAgitatedTriggerReaction
{

public:

	enum Flags
	{
		DisableWhenAgitated							= BIT0,
		DisableWhenFriendlyIsAgitated				= BIT1,
		MustBeUsingScenario							= BIT2,
		MustBeUsingScenarioBeforeInitialReaction	= BIT3,
		MustBeWandering								= BIT4,
		MustBeWanderingBeforeInitialReaction		= BIT5,
		NoLineOfSightNeeded							= BIT6,
		UseDistanceFromScenario						= BIT7,
		MustBeUsingATerritorialScenario				= BIT8,
		DisableWhenWandering						= BIT9,
		DisableWhenStill							= BIT10,
		DisableIfNeitherPedIsOnFoot					= BIT11,
		MustBeOnFoot								= BIT12,
		TargetMustBeOnFoot							= BIT13,
		MustBeInVehicle								= BIT14,
		MustBeStationary							= BIT15,
	};

public:

	struct Time
	{
		Time()
		: m_Min(0.0f)
		, m_Max(0.0f)
		{}

		float m_Min;
		float m_Max;

		PAR_SIMPLE_PARSABLE;
	};

public:

	CAgitatedTriggerReaction();
	~CAgitatedTriggerReaction();
	
public:

	fwFlags16		GetFlags()								const { return m_Flags; }
	float			GetMaxDotToTarget()						const { return m_MaxDotToTarget; }
	u8				GetMaxReactions()						const { return m_MaxReactions; }
	float			GetMaxTargetSpeed()						const { return m_MaxTargetSpeed; }
	float			GetMinDotToTarget()						const { return m_MinDotToTarget; }
	float			GetMinTargetSpeed()						const { return m_MinTargetSpeed; }
	const Time&		GetTimeAfterInitialReactionFailure()	const { return m_TimeAfterInitialReactionFailure; }
	const Time&		GetTimeAfterLastSuccessfulReaction()	const { return m_TimeAfterLastSuccessfulReaction; }
	const Time&		GetTimeBeforeInitialReaction()			const { return m_TimeBeforeInitialReaction; }
	const Time&		GetTimeBetweenEscalatingReactions()		const { return m_TimeBetweenEscalatingReactions; }
	AgitatedType	GetType()								const { return m_Type; }

public:

	void AddReaction()
	{
		++m_Reactions;
		aiAssert(m_Reactions > 0);
	}

	bool HasReachedMaxReactions() const
	{
		return (m_Reactions >= m_MaxReactions);
	}

	void RemoveReaction()
	{
		aiAssert(m_Reactions > 0);
		--m_Reactions;
	}

private:

	AgitatedType			m_Type;
	Time					m_TimeBeforeInitialReaction;
	Time					m_TimeBetweenEscalatingReactions;
	Time					m_TimeAfterLastSuccessfulReaction;
	Time					m_TimeAfterInitialReactionFailure;
	float					m_MinDotToTarget;
	float					m_MaxDotToTarget;
	float					m_MinTargetSpeed;
	float					m_MaxTargetSpeed;
	u8						m_MaxReactions;
	fwFlags16				m_Flags;

private:

	u8	m_Reactions;

private:

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTrigger
////////////////////////////////////////////////////////////////////////////////

class CAgitatedTrigger
{

public:
	
	CAgitatedTrigger();
	~CAgitatedTrigger();

public:

	float						GetChances()	const { return m_Chances; }
	float						GetDistance()	const { return m_Distance; }
	atHashWithStringNotFinal	GetReaction()	const { return m_Reaction; }

public:

	bool HasPedType(ePedType nPedType) const;
	void OnPostLoad();

private:

	atArray<ConstString>		m_PedTypes;
	float						m_Chances;
	float						m_Distance;
	atHashWithStringNotFinal	m_Reaction;

private:

	atArray<ePedType> m_nPedTypes;

private:

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTriggerSet
////////////////////////////////////////////////////////////////////////////////

class CAgitatedTriggerSet
{

public:

	enum Flags
	{
		IgnoreHonkedAt = BIT0,
	};

public:

	CAgitatedTriggerSet()
	{}

public:

			fwFlags32					GetFlags()		const	{ return m_Flags; }
			atHashWithStringNotFinal	GetParent()		const	{ return m_Parent; }
			atArray<CAgitatedTrigger>&	GetTriggers()			{ return m_Triggers; }
	const	atArray<CAgitatedTrigger>&	GetTriggers()	const	{ return m_Triggers; }

private:

	atHashWithStringNotFinal	m_Parent;
	fwFlags32					m_Flags;
	atArray<CAgitatedTrigger>	m_Triggers;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CAgitatedTriggers
////////////////////////////////////////////////////////////////////////////////

class CAgitatedTriggers
{

public:

	CAgitatedTriggers();
	~CAgitatedTriggers();

public:

	CAgitatedTriggerReaction* GetReaction(atHashWithStringNotFinal hName)
	{
		return m_Reactions.SafeGet(hName);
	}

	const CAgitatedTriggerReaction* GetReaction(atHashWithStringNotFinal hName) const
	{
		return m_Reactions.SafeGet(hName);
	}

	CAgitatedTriggerSet* GetSet(atHashWithStringNotFinal hName)
	{
		return m_Sets.SafeGet(hName);
	}

	const CAgitatedTriggerSet* GetSet(atHashWithStringNotFinal hName) const
	{
		return m_Sets.SafeGet(hName);
	}

public:

			atHashWithStringNotFinal	GetSet(const CPed& rPed) const;
	const	CAgitatedTrigger*			GetTrigger(const CPed& rPed, atHashWithStringNotFinal hReaction) const;
			bool						HasReactionWithType(const CPed& rPed, const CPed& rTarget, AgitatedType nType) const;
			bool						IsFlagSet(const CPed& rPed, CAgitatedTriggerSet::Flags nFlag) const;
			int							LoadReactions(atHashWithStringNotFinal* aReactions, int iMaxReactions, AgitatedType nFilter = AT_None) const;

private:

	atBinaryMap<CAgitatedTriggerReaction, atHashWithStringNotFinal>	m_Reactions;
	atBinaryMap<CAgitatedTriggerSet, atHashWithStringNotFinal>		m_Sets;

	PAR_SIMPLE_PARSABLE;

};

#endif // INC_AGITATED_TRIGGER_H
