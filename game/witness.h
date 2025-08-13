#ifndef GAME_WITNESS_H
#define GAME_WITNESS_H

#include "grcore/debugdraw.h"		// Only for DEBUG_DRAW.
#include "scene/RegdRefTypes.h"		// RegdPed
#include "Task/System/Tuning.h"

//-----------------------------------------------------------------------------

class CCrimeBeingQd;
class CWanted;

//-----------------------------------------------------------------------------

enum eWitnessType
{
	eWitnessedNone,
	eWitnessedByPeds,
	eWitnessedByLaw
};

//-----------------------------------------------------------------------------

class CReportingWitness
{
public:
	CReportingWitness();

	bool Init(CPed& witness, CPed* criminal, Vec3V_ConstRef crimePos);
	void Update();
	void Shutdown();

	CPed* GetWitness() const { return m_Witness; }

	bool IsActive() const
	{	return m_Active;	}

#if DEBUG_DRAW
	void DebugRender() const;
#endif

protected:
	bool GiveTask();
	bool IsTaskRunning() const;

	RegdPed		m_Criminal;
	RegdPed		m_Witness;
	Vec3V		m_CrimePos;
	bool		m_Active;
};

//-----------------------------------------------------------------------------

class CCrimeWitnesses
{

public:

	static const int sMaxPerCrime = 8;

public:

	struct Tunables : CTuning
	{
		struct Scoring
		{
			Scoring()
			{}

			float m_BonusForOnScreen;
			float m_DistanceForMax;
			float m_DistanceForMin;
			float m_BonusForDistance;
			float m_PenaltyForVehicle;

			PAR_SIMPLE_PARSABLE;
		};

		Tunables();

		Scoring m_Scoring;

		PAR_PARSABLE;
	};

public:

	explicit CCrimeWitnesses(CWanted& wanted);

public:

	int		GetMaxActiveReports(const CCrimeBeingQd& rCrime) const;
	int		GetNumActiveReports() const;
	int		GetNumAvailableReports(const CCrimeBeingQd& rCrime) const;
	bool	HasActiveReport() const;
	bool	HasAvailableReport(const CCrimeBeingQd& rCrime) const;
	bool	IsCrimeAutomaticallyReportedToLaw(const CCrimeBeingQd& rCrime) const;

	void Update();
	void UpdateReports();
	bool UpdateIsWitnessed(CCrimeBeingQd& crime);
	void UpdateObservations();
	void UpdateObservationsFromHearing();
	void UpdateObservationsFromHearingForCrime(CCrimeBeingQd& rCrime, float fMaxDistance);
	void UpdateObservationsFromHearingForCrimeFromPeds(CCrimeBeingQd& rCrime, float fMaxDistance);
	void UpdateObservationsFromHearingForCrimeFromVehicles(CCrimeBeingQd& rCrime, float fMaxDistance);
	void UpdateObservationsFromSight();
	void UpdateObservationsFromVictim();

	void NotifyQueueMayContainUnperceivedCrimes()
	{	m_QueueMayContainCrimesThatCanBeWitnessedVictims = true; m_QueueMayContainUnheardCrimes = true; m_QueueMayContainUnobservedCrimes = true;	}

	void LaunchReportCrimeResponse(CEntity& rEntity, const CCrimeBeingQd& rCrime);
	void LaunchReportCrimeResponses(const CCrimeBeingQd& crime);

	void ReportWitnessSuccessful(CEntity* pEntity);
	void ReportWitnessUnsuccessful(CEntity* pEntity);

	static bool GetWitnessesNeeded() { return ms_WitnessesNeeded; }
	static const Vec3V &GetWitnessReportPosition() { return ms_WitnessReportPosition; }
	static void SetWitnessesNeeded(bool b) { ms_WitnessesNeeded = b; }
	static void SetWitnessReportPosition(Vec3V_In pos) { ms_WitnessReportPosition = pos; }

#if DEBUG_DRAW
	void DebugRender() const;
	static void GetWitnessInfoText(char* strOut, int maxLen, const CCrimeBeingQd& crime);
	static const char* GetWitnessTypeName(eWitnessType type);
#endif

#if __BANK
	static void AddWidgets(bkBank& bank);

	static void BankStepButtonCB()
	{	ms_BankDebugDoStepVisibilityTests = true;	}

	static bool ms_BankDebugBlockReport;
	static bool ms_BankDebugDoStepVisibilityTests;
	static bool ms_BankDebugStepVisibilityTests;
	static bool ms_BankDrawVisibilityTests;
#endif

protected:

	bool	CanLaunchReportCrimeResponses(const CCrimeBeingQd& rCrime) const;
	void	GetWitnessesToNotifyLawEnforcement(const CCrimeBeingQd& rCrime, atFixedArray<CEntity *, sMaxPerCrime>& rEntities) const;
	float	ScoreWitness(const CEntity& rEntity, const CCrimeBeingQd& rCrime) const;
	void	TrimWitnesses(const CCrimeBeingQd& rCrime, atFixedArray<CEntity *, sMaxPerCrime>& rWitnesses, int iMaxWitnesses) const;
	bool	TryObserveCrime(CPed& witness, CCrimeBeingQd& crime);
	bool	TryToHearCrime(CEntity& rEntity, CCrimeBeingQd& rCrime);
	bool	CheckLineOfSightAndProcessResult(CPed& witness, CPed& tgt, CCrimeBeingQd& crime, bool& seenOut);
	void	ProcessResultCrimeWitnessed(CEntity& rEntity, CCrimeBeingQd& rCrime);

	CReportingWitness* FindActiveReport();
	CReportingWitness* FindInactiveReport();
	CReportingWitness* FindReportForEntity(const CEntity& rEntity);

	CPed& GetCriminal(const CCrimeBeingQd& crime) const;

private:

	static CPed* GetVictimPed(const CCrimeBeingQd& rCrime);

private:

	// PURPOSE:	These objects manages witnesses trying to report the crimes.
	static const int sMaxReports = 3;
	atRangeArray<CReportingWitness, sMaxReports> m_Reports;

	CWanted*			m_pWanted;

	int					m_LineOfSightCrimeIndexToCheck;
	int					m_LineOfSightNumAttempts;
	int					m_LineOfSightPedIndexToCheck;

	bool				m_LineOfSightCriminalDone;
	bool				m_LineOfSightVictimDone;
	bool				m_QueueMayContainCrimesThatCanBeWitnessedVictims;
	bool				m_QueueMayContainUnheardCrimes;
	bool				m_QueueMayContainUnobservedCrimes;

private:

	// PURPOSE:	If ms_WitnessesNeeded is set, this is the position where witnesses try to go to report crimes.
	// NOTES:	Definitely room for improvement here - perhaps this should be a list, or perhaps these positions
	//			should be some sort of objects in the world instead (like point objects with a decorator).
	static Vec3V		ms_WitnessReportPosition;

	// PURPOSE:	True if crimes need to be witnessed and reported to law enforcement in order to be registered.
	static bool			ms_WitnessesNeeded;

	static Tunables sm_Tunables;

};

//-----------------------------------------------------------------------------

#endif	// GAME_WITNESS_H
