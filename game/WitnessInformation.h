#ifndef INC_WITNESS_INFORMATION_H
#define INC_WITNESS_INFORMATION_H

// Rage headers
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "system/bit.h"

// Game forward declarations
class CPed;

////////////////////////////////////////////////////////////////////////////////
// CWitnessPersonality
////////////////////////////////////////////////////////////////////////////////

struct CWitnessPersonality
{
	enum Flags
	{
		CanWitnessCrimes			= BIT0,
		WillCallLawEnforcement		= BIT1,
		WillMoveToLawEnforcement	= BIT2,
	};

	CWitnessPersonality()
	{}

	fwFlags8	m_Flags;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CWitnessInformations
////////////////////////////////////////////////////////////////////////////////

struct CWitnessInformations
{
	CWitnessInformations()
	{}

	atBinaryMap<CWitnessPersonality, atHashWithStringNotFinal> m_Personalities;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CWitnessInformationManager
////////////////////////////////////////////////////////////////////////////////

class CWitnessInformationManager : public datBase
{

public:

	static CWitnessInformationManager& GetInstance()
	{	FastAssert(sm_Instance); return *sm_Instance;	}

private:

	explicit CWitnessInformationManager();
	~CWitnessInformationManager();

public:

	bool CanWitnessCrimes(const CPed& rPed) const;
	bool WillCallLawEnforcement(const CPed& rPed) const;
	bool WillMoveToLawEnforcement(const CPed& rPed) const;

public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

private:

	const CWitnessPersonality* GetPersonality(atHashWithStringNotFinal hName) const
	{
		return m_WitnessInformations.m_Personalities.SafeGet(hName);
	}

private:

	const CWitnessPersonality* GetPersonality(const CPed& rPed) const;

private:

	CWitnessInformations m_WitnessInformations;

private:

	static CWitnessInformationManager* sm_Instance;

};

#endif // INC_WITNESS_INFORMATION_H
