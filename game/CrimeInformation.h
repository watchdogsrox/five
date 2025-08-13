#ifndef INC_CRIME_INFORMATION_H
#define INC_CRIME_INFORMATION_H

// Rage headers
#include "atl/binmap.h"
#include "atl/hashstring.h"
#include "data/base.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"
#include "system/bit.h"

// Game headers
#include "game/Crime.h"

////////////////////////////////////////////////////////////////////////////////
// CWitnessInformation
////////////////////////////////////////////////////////////////////////////////

class CWitnessInformation
{

public:

	enum Flags
	{
		MustBeWitnessed						= BIT0,
		MustNotifyLawEnforcement			= BIT1,
		OnlyVictimCanNotifyLawEnforcement	= BIT2,
	};

public:

	CWitnessInformation();
	~CWitnessInformation();

public:

	float GetMaxDistanceToHear() const { return m_MaxDistanceToHear; }

public:

	bool IsFlagSet(u8 uFlag) const;

private:

	float		m_MaxDistanceToHear;
	fwFlags8	m_Flags;
	fwFlags8	m_AdditionalFlagsForSP;
	fwFlags8	m_AdditionalFlagsForMP;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CCrimeInformation
////////////////////////////////////////////////////////////////////////////////

struct CCrimeInformation
{
	CCrimeInformation()
	{}

	CWitnessInformation	m_WitnessInformation;
	float				m_ImmediateDetectionRange;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CCrimeInformations
////////////////////////////////////////////////////////////////////////////////

struct CCrimeInformations
{
	CCrimeInformations()
	{}

	atBinaryMap<CCrimeInformation, atHashWithStringNotFinal> m_CrimeInformations;

	PAR_SIMPLE_PARSABLE;
};

////////////////////////////////////////////////////////////////////////////////
// CCrimeInformationManager
////////////////////////////////////////////////////////////////////////////////

class CCrimeInformationManager : public datBase
{

public:

	static CCrimeInformationManager& GetInstance()
	{	FastAssert(sm_Instance); return *sm_Instance;	}

private:

	explicit CCrimeInformationManager();
	~CCrimeInformationManager();

public:

	const CCrimeInformation* GetInformation(atHashWithStringNotFinal hName) const
	{
		return m_CrimeInformations.m_CrimeInformations.SafeGet(hName);
	}

	const CCrimeInformation* GetInformation(eCrimeType nCrimeType) const
	{
		return GetInformation(atHashWithStringNotFinal(g_CrimeNameHashes[nCrimeType]));
	}

public:

	bool	CanBeHeard(eCrimeType nCrimeType, float& fMaxDistance) const;
	float	GetImmediateDetectionRange(eCrimeType nCrimeType) const;
	bool	MustBeWitnessed(eCrimeType nCrimeType) const;
	bool	MustNotifyLawEnforcement(eCrimeType nCrimeType) const;
	bool	OnlyVictimCanNotifyLawEnforcement(eCrimeType nCrimeType) const;

public:

	static void	Init(unsigned initMode);
	static void	Shutdown(unsigned shutdownMode);

private:

	CCrimeInformations m_CrimeInformations;

private:

	static CCrimeInformationManager* sm_Instance;

};

#endif // INC_CRIME_INFORMATION_H
