// File header
#include "game/witness.h"

// Game headers
#include "game/CrimeInformation.h"
#include "game/Wanted.h"
#include "game/WitnessInformation.h"
#include "Peds/ped.h"
#include "Peds/PedCapsule.h"				// IsBiped
#include "Peds/PedGeometryAnalyser.h"
#include "Peds/PedIntelligence.h"
#include "scene/world/GameWorld.h"
#include "task/General/Phone/TaskCallPolice.h"
#include "task/Service/Witness/TaskWitness.h"

// Parser headers
#include "game/witness_parser.h"

AI_OPTIMISATIONS()

#include "data/aes_init.h"
AES_INIT_6;

//-----------------------------------------------------------------------------

CReportingWitness::CReportingWitness()
		: m_Active(false)
{
}


bool CReportingWitness::Init(CPed& witness, CPed* criminal, Vec3V_ConstRef crimePos)
{
	Assert(!m_Active);

	Assert(!m_Witness);
	m_Witness = &witness;

	m_Criminal = criminal;

	m_CrimePos = crimePos;

	if(!GiveTask())
	{
		m_Criminal = NULL;
		m_Witness = NULL;
		return false;
	}

	m_Active = true;

	return true;
}


void CReportingWitness::Update()
{
	Assert(m_Active);

	CPed* pPed = m_Witness;

	if(!pPed || pPed->IsInjured())
	{
		// Our chosen witness seems to no longer exist (maybe despawned), or is dead. Let the
		// user pick another or something.
		Shutdown();
		return;
	}

	//Check if the task is not running.
	if(!IsTaskRunning())
	{
		//Give the task again.
		GiveTask();

		// TODO: If this condition persists for several seconds, that we can't get the task to run,
		// we should probably give up on this witness and pick another.
	}
}


void CReportingWitness::Shutdown()
{
	// TODO: Clear tasks of the witness here, somehow?

	Assert(m_Active);
	m_Active = false;
	m_Criminal.Reset(NULL);
	m_Witness.Reset(NULL);
}

#if DEBUG_DRAW

void CReportingWitness::DebugRender() const
{
	//Ensure the report is active.
	if(!m_Active)
	{
		return;
	}

	//Ensure the witness is valid.
	CPed* pWitness = m_Witness;
	if(!pWitness)
	{
		return;
	}

	grcDebugDraw::Sphere(VEC3V_TO_VECTOR3(pWitness->GetTransform().GetPosition()) + (ZAXIS * 1.5f), 0.125f, Color_blue);
}

#endif	// DEBUG_DRAW

bool CReportingWitness::GiveTask()
{
	//Ensure the witness is valid.
	CPed* pPed = m_Witness;
	if(!pPed)
	{
		return false;
	}

	//Ensure the criminal is valid.
	if(!m_Criminal)
	{
		return false;
	}

	//Call the police, if we can.
	if(CTaskCallPolice::CanGiveToWitness(*pPed, *m_Criminal) && CTaskCallPolice::GiveToWitness(*pPed, *m_Criminal))
	{
		return true;
	}

	//Witness, if we can.
	if(CTaskWitness::CanGiveToWitness(*pPed) && CTaskWitness::GiveToWitness(*pPed, *m_Criminal, m_CrimePos))
	{
		return true;
	}

	return false;
}

bool CReportingWitness::IsTaskRunning() const
{
	//Ensure the witness is valid.
	CPed* pWitness = m_Witness;
	if(!pWitness)
	{
		return false;
	}

	//Check if the call police task is active.
	if(pWitness->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_CALL_POLICE))
	{
		return true;
	}

	//Check if the witness task is active.
	const CTask* pTaskActive = pWitness->GetPedIntelligence()->GetTaskActive();
	if(pTaskActive && (pTaskActive->GetTaskType() == CTaskTypes::TASK_WITNESS))
	{
		return true;
	}

	return false;
}

//-----------------------------------------------------------------------------

static bool sMayNeedFutureObservations(const CCrimeBeingQd& rCrime)
{
	//Assert that the crime is valid.
	Assert(rCrime.IsValid());

	//Ensure the crime has not already been reported.
	if(rCrime.WasAlreadyReported())
	{
		return false;
	}

	//Ensure the crime has not been witnessed by law.
	if(rCrime.GetWitnessType() == eWitnessedByLaw)
	{
		return false;
	}

	//Ensure the crime is in progress, or it can't decay.
	if(!rCrime.IsInProgress() && rCrime.HasLifetimeExpired())
	{
		return false;
	}

	return true;
}

static bool sMayCrimeBePerceived(const CCrimeBeingQd& rCrime)
{
	// First, do some checks on things that should not change in the future.
	// For example, if the crime has already been reported.
	if(!sMayNeedFutureObservations(rCrime))
	{
		return false;
	}

	//Ensure the crime does not have the max witnesses.
	if(rCrime.HasMaxWitnesses())
	{
		return false;
	}

	return true;
}

static bool sMayCrimeBeHeard(const CCrimeBeingQd& rCrime, float& fMaxDistance)
{
	//Ensure the crime can be perceived.
	if(!sMayCrimeBePerceived(rCrime))
	{
		return false;
	}

	//Ensure the crime happened less than a second ago.
	u32 uCurrentTime = fwTimer::GetTimeInMilliseconds();
	u32 uTimeSinceCrimeOccurred = uCurrentTime - rCrime.GetTimeOfQing();
	if(uTimeSinceCrimeOccurred > 1000)
	{
		return false;
	}

	//Ensure the crime can be heard.
	if(!CCrimeInformationManager::GetInstance().CanBeHeard(rCrime.GetCrimeType(), fMaxDistance))
	{
		return false;
	}

	return true;
}

static bool sMayCrimeBeObserved(const CCrimeBeingQd& rCrime)
{
	//Ensure the crime can be perceived.
	if(!sMayCrimeBePerceived(rCrime))
	{
		return false;
	}

	return true;
}

static bool sCanPedWitnessCrimes(const CPed& rPed)
{
	//Ensure the ped is not injured.
	if(rPed.IsInjured())
	{
		return false;
	}

	//Ensure the ped is not a player.
	if(rPed.IsPlayer())
	{
		return false;
	}

	//Ensure the ped type is law or civilian.
	if(!rPed.IsLawEnforcementPed() && !rPed.IsCivilianPed())
	{
		return false;
	}

	//Ensure the ped can witness crimes.
	if(!CWitnessInformationManager::GetInstance().CanWitnessCrimes(rPed))
	{
		return false;
	}

	return true;
}

static bool sCanVehicleWitnessCrimes(const CVehicle& rVehicle)
{
	//Ensure the vehicle has pretend occupants.
	if(!rVehicle.IsUsingPretendOccupants())
	{
		return false;
	}

	return true;
}

static bool sCanPedNotifyLawEnforcement(const CPed& rPed, const CPed& rCriminal)
{
	//Ensure the ped is not a clone.
	if(rPed.IsNetworkClone())
	{
		return false;
	}

	//Ensure the ped can witness crimes.
	if(!sCanPedWitnessCrimes(rPed))
	{
		return false;
	}

	//Ensure the ped can be given a witness task.
	if(!CTaskCallPolice::CanGiveToWitness(rPed, rCriminal) && !CTaskWitness::CanGiveToWitness(rPed))
	{
		return false;
	}

	return true;
}

static bool sCanVehicleNotifyLawEnforcement(const CVehicle& rVehicle)
{
	//Ensure the vehicle is not a clone.
	if(rVehicle.IsNetworkClone())
	{
		return false;
	}

	//Ensure the vehicle can witness crimes.
	if(!sCanVehicleWitnessCrimes(rVehicle))
	{
		return false;
	}

	return true;
}

static bool sCanNotifyLawEnforcement(const CEntity& rEntity, const CPed& rCriminal)
{
	//Check if the entity is a ped.
	if(rEntity.GetIsTypePed())
	{
		return sCanPedNotifyLawEnforcement(static_cast<const CPed &>(rEntity), rCriminal);
	}
	//Check if the entity is a vehicle.
	else if(rEntity.GetIsTypeVehicle())
	{
		return sCanVehicleNotifyLawEnforcement(static_cast<const CVehicle &>(rEntity));
	}

	return false;
}

static bool sMayPedPerceiveAnyCrimes(const CPed& rPed)
{
	//Ensure the ped can witness crimes.
	if(!sCanPedWitnessCrimes(rPed))
	{
		return false;
	}

	return true;
}

static bool sMayVehiclePerceiveAnyCrimes(const CVehicle& rVehicle)
{
	//Ensure the vehicle can witness crimes.
	if(!sCanVehicleWitnessCrimes(rVehicle))
	{
		return false;
	}

	return true;
}

static bool sMayPedPerceiveCrime(const CPed& rPed, const CCrimeBeingQd& rCrime)
{
	//Assert that the ped can perceive crimes.
	Assert(sMayPedPerceiveAnyCrimes(rPed));

	//Ensure that the crime may be perceived.
	if(!sMayCrimeBePerceived(rCrime))
	{
		return false;
	}

	//Ensure the crime has not been witnessed.
	if(rCrime.HasBeenWitnessedBy(rPed))
	{
		return false;
	}

	return true;
}

static bool sMayVehiclePerceiveCrime(const CVehicle& rVehicle, const CCrimeBeingQd& rCrime)
{
	//Assert that the vehicle can perceive crimes.
	Assert(sMayVehiclePerceiveAnyCrimes(rVehicle));

	//Ensure that the crime may be perceived.
	if(!sMayCrimeBePerceived(rCrime))
	{
		return false;
	}

	//Ensure the crime has not been witnessed.
	if(rCrime.HasBeenWitnessedBy(rVehicle))
	{
		return false;
	}

	return true;
}

static bool sMayPedHearCrime(const CPed& rPed, const CCrimeBeingQd& rCrime)
{
	//Ensure the ped can perceive the crime.
	if(!sMayPedPerceiveCrime(rPed, rCrime))
	{
		return false;
	}

	return true;
}

static bool sMayVehicleHearCrime(const CVehicle& rVehicle, const CCrimeBeingQd& rCrime)
{
	//Ensure the vehicle can perceive the crime.
	if(!sMayVehiclePerceiveCrime(rVehicle, rCrime))
	{
		return false;
	}

	return true;
}

static bool sMayPedObserveCrime(const CPed& witness, const CCrimeBeingQd& crime)
{
	//Ensure the ped can perceive the crime.
	if(!sMayPedPerceiveCrime(witness, crime))
	{
		return false;
	}

	return true;
}

static ECanTargetResult sCheckWitnessPed(CPed& witness, CPed& tgt)
{
	// Not entirely sure about what flags to use. We may want to adapt more of
	// the code from CPedAcquaintanceScanner::CanPedSeePed() here, using CPedTargetting
	// if possible, etc. /FF
	const s32 iTargetFlags = TargetOption_UseFOVPerception;

	const ECanTargetResult losRet = CPedGeometryAnalyser::CanPedTargetPedAsync(witness, tgt, iTargetFlags);

#if __BANK && DEBUG_DRAW
	if(CCrimeWitnesses::ms_BankDrawVisibilityTests)
	{
		Color32 col;
		if(losRet == ECanTarget)
		{
			col = Color_green;
		}
		else if(losRet == ECanNotTarget)
		{
			col = Color_red;
		}
		else
		{
			col = Color_grey;
		}
		grcDebugDraw::Line(VEC3V_TO_VECTOR3(witness.GetTransform().GetPosition()), VEC3V_TO_VECTOR3(tgt.GetTransform().GetPosition()), col);
	}
#endif	// BANK && DEBUG_DRAW

	return losRet;
}

//-----------------------------------------------------------------------------

#if __BANK
bool CCrimeWitnesses::ms_BankDebugBlockReport = false;
bool CCrimeWitnesses::ms_BankDebugDoStepVisibilityTests = false;
bool CCrimeWitnesses::ms_BankDebugStepVisibilityTests = false;
bool CCrimeWitnesses::ms_BankDrawVisibilityTests = false;
#endif	// __BANK

Vec3V CCrimeWitnesses::ms_WitnessReportPosition(V_ZERO);
bool CCrimeWitnesses::ms_WitnessesNeeded = true;

CCrimeWitnesses::Tunables CCrimeWitnesses::sm_Tunables;
IMPLEMENT_TUNABLES(CCrimeWitnesses, "CCrimeWitnesses", 0x7ef4058b, "Witness Tuning", "Game Logic");

CCrimeWitnesses::CCrimeWitnesses(CWanted& wanted)
		: m_pWanted(&wanted)
		, m_LineOfSightCrimeIndexToCheck(0)
		, m_LineOfSightNumAttempts(0)
		, m_LineOfSightPedIndexToCheck(0)
		, m_LineOfSightCriminalDone(false)
		, m_LineOfSightVictimDone(false)
		, m_QueueMayContainCrimesThatCanBeWitnessedVictims(false)
		, m_QueueMayContainUnheardCrimes(false)
		, m_QueueMayContainUnobservedCrimes(false)
{


// TEMP, can be enabled for testing.
#if 0
	ms_WitnessesNeeded = true;
	ms_WitnessReportPosition.SetXf(103.0f);
	ms_WitnessReportPosition.SetYf(-745.6f);
	ms_WitnessReportPosition.SetZf(45.76f);
#endif
}

void CCrimeWitnesses::Update()
{
	//Update the reports.
	UpdateReports();

	//Update the observations.
	UpdateObservations();
}

void CCrimeWitnesses::UpdateReports()
{
	//Iterate over the reports.
	for(int i = 0; i < m_Reports.GetMaxCount(); ++i)
	{
		//Check if the report is active.
		if(m_Reports[i].IsActive())
		{
			//Update the report.
			m_Reports[i].Update();
		}
	}
}

int CCrimeWitnesses::GetMaxActiveReports(const CCrimeBeingQd& rCrime) const
{
	//Calculate the max active reports.
	float fValue = (float)rCrime.m_Witnesses.GetCount() / (float)rCrime.m_Witnesses.GetMaxCount();
	return Lerp(fValue, 1, sMaxReports);
}

int CCrimeWitnesses::GetNumActiveReports() const
{
	int iNumActiveReports = 0;
	for(int i = 0; i < m_Reports.GetMaxCount(); ++i)
	{
		if(m_Reports[i].IsActive())
		{
			++iNumActiveReports;
		}
	}

	return iNumActiveReports;
}

int CCrimeWitnesses::GetNumAvailableReports(const CCrimeBeingQd& rCrime) const
{
	//Get the number of active reports.
	int iNumActiveReports = GetNumActiveReports();
	aiAssert(iNumActiveReports >= 0);

	//Get the max active reports.
	int iMaxActiveReports = GetMaxActiveReports(rCrime);
	aiAssert(iMaxActiveReports > 0);

	return (iMaxActiveReports - iNumActiveReports);
}

bool CCrimeWitnesses::HasActiveReport() const
{
	return (const_cast<CCrimeWitnesses *>(this)->FindActiveReport() != NULL);
}

bool CCrimeWitnesses::HasAvailableReport(const CCrimeBeingQd& rCrime) const
{
	return (GetNumAvailableReports(rCrime) > 0);
}

bool CCrimeWitnesses::IsCrimeAutomaticallyReportedToLaw(const CCrimeBeingQd& UNUSED_PARAM(rCrime)) const
{
	return (m_pWanted->GetWantedLevel() > WANTED_CLEAN);
}

bool CCrimeWitnesses::UpdateIsWitnessed(CCrimeBeingQd& crime)
{
	eWitnessType witnessType = crime.GetWitnessType();
	if(witnessType != eWitnessedByLaw)
	{
		bool bMustNotifyLawEnforcement = crime.bMustNotifyLawEnforcement || (crime.GetVictim() && crime.HasOnlyBeenWitnessedBy(*crime.GetVictim()));

		if(
#if __BANK
				!ms_BankDebugBlockReport &&
#endif
				witnessType == eWitnessedByPeds
				&& crime.HasWitnesses()
				&& crime.IsSerious()
				&& !crime.HasLifetimeExpired()
				&& bMustNotifyLawEnforcement
				&& CanLaunchReportCrimeResponses(crime)
				)
		{
			LaunchReportCrimeResponses(crime);
		}
		else if(witnessType != eWitnessedByPeds || HasActiveReport())
		{

		}
		else if(IsCrimeAutomaticallyReportedToLaw(crime) ||
			((witnessType == eWitnessedByPeds) && !bMustNotifyLawEnforcement))
		{
			crime.SetWitnessType(eWitnessedByLaw);
			m_pWanted->SetAllPendingCrimesWitnessedByLaw();
		}
	}
	else
	{
		return true;
	}

	if(crime.GetWitnessType() == eWitnessedByLaw)
	{
		return true;
	}

	return false;
}


void CCrimeWitnesses::UpdateObservations()
{
	//Ensure witnesses are needed.
	if(!ms_WitnessesNeeded)
	{
		return;
	}

	//Update the observations from the victim.
	UpdateObservationsFromVictim();

	//Update the observations from hearing.
	UpdateObservationsFromHearing();

	//Update the observations from sight.
	UpdateObservationsFromSight();
}

void CCrimeWitnesses::UpdateObservationsFromHearing()
{
	//Ensure the queue may contain unheard crimes.
	if(!m_QueueMayContainUnheardCrimes)
	{
		return;
	}

	//Iterate over the crimes.
	for(int i = 0; i < m_pWanted->m_MaxCrimesBeingQd; ++i)
	{
		//Ensure the crime is valid.
		CCrimeBeingQd& rCrime = m_pWanted->CrimesBeingQd[i];
		if(!rCrime.IsValid())
		{
			continue;
		}

		//Ensure the crime may be heard.
		float fMaxDistance;
		if(!sMayCrimeBeHeard(rCrime, fMaxDistance))
		{
			continue;
		}

		//Update the observations from hearing for the crime.
		UpdateObservationsFromHearingForCrime(rCrime, fMaxDistance);
	}

	//Note that the queue has been checked.
	m_QueueMayContainUnheardCrimes = false;
}

void CCrimeWitnesses::UpdateObservationsFromHearingForCrime(CCrimeBeingQd& rCrime, float fMaxDistance)
{
	//Update the observations from hearing for the crime from peds.
	UpdateObservationsFromHearingForCrimeFromPeds(rCrime, fMaxDistance);

	//Update the observations from hearing for the crime from vehicles.
	UpdateObservationsFromHearingForCrimeFromVehicles(rCrime, fMaxDistance);
}

void CCrimeWitnesses::UpdateObservationsFromHearingForCrimeFromPeds(CCrimeBeingQd& rCrime, float fMaxDistance)
{
	//Grab the spatial array.
	const CSpatialArray& rSpatialArray = CPed::GetSpatialArray();

	//Search for peds near the crime.
	CSpatialArray::FindResult result[sMaxPerCrime];
	int iFound = rSpatialArray.FindInSphere(rCrime.GetPosition(), fMaxDistance, &result[0], sMaxPerCrime);

	//Iterate over the results.
	for(int i = 0; i < iFound; ++i)
	{
		//Grab the ped.
		CPed* pPed = CPed::GetPedFromSpatialArrayNode(result[i].m_Node);

		//Ensure the ped may perceive crimes.
		if(!sMayPedPerceiveAnyCrimes(*pPed))
		{
			continue;
		}

		//Ensure the ped may hear the crime.
		if(!sMayPedHearCrime(*pPed, rCrime))
		{
			continue;
		}

		//Try to hear the crime.
		TryToHearCrime(*pPed, rCrime);
	}
}

void CCrimeWitnesses::UpdateObservationsFromHearingForCrimeFromVehicles(CCrimeBeingQd& rCrime, float fMaxDistance)
{
	//Grab the spatial array.
	const CSpatialArray& rSpatialArray = CVehicle::GetSpatialArray();

	//Search for vehicles near the crime.
	CSpatialArray::FindResult result[sMaxPerCrime];
	int iFound = rSpatialArray.FindInSphere(rCrime.GetPosition(), fMaxDistance, &result[0], sMaxPerCrime);

	//Iterate over the results.
	for(int i = 0; i < iFound; ++i)
	{
		//Grab the vehicle.
		CVehicle* pVehicle = CVehicle::GetVehicleFromSpatialArrayNode(result[i].m_Node);

		//Ensure the vehicle may perceive crimes.
		if(!sMayVehiclePerceiveAnyCrimes(*pVehicle))
		{
			continue;
		}

		//Ensure the vehicle may hear the crime.
		if(!sMayVehicleHearCrime(*pVehicle, rCrime))
		{
			continue;
		}

		//Try to hear the crime.
		TryToHearCrime(*pVehicle, rCrime);
	}
}

void CCrimeWitnesses::UpdateObservationsFromSight()
{
	if(!m_QueueMayContainUnobservedCrimes)
	{
		return;
	}

	// We will use a temporary array to keep track of crime status. First, make sure
	// it's large enough. For the rest of this function, maxCrimes should be used instead
	// of m_MaxCrimesBeingQd.
	static const int kCrimeIsObservableArrayMaxSize = 64;
	int maxCrimes = m_pWanted->m_MaxCrimesBeingQd;
	if(!aiVerifyf(maxCrimes <= kCrimeIsObservableArrayMaxSize, "Too many max crimes, increase kCrimeIsObservableArrayMaxSize."))
	{
		// Shrink to prevent overrun.
		maxCrimes = kCrimeIsObservableArrayMaxSize;
	}

	// Next, we look through the crime queue and check if there is anything in there
	// that we currently should try to observe. crimeIsObservableArray[] gets filled
	// in for later use.
	bool foundObservableCrimes = false;
	bool crimeIsObservableArray[kCrimeIsObservableArrayMaxSize];
	for(int crimeIndex = 0; crimeIndex < maxCrimes; crimeIndex++)
	{
		Assert(crimeIndex >= 0 && crimeIndex < m_pWanted->m_MaxCrimesBeingQd);
		const CCrimeBeingQd& crime = m_pWanted->CrimesBeingQd[crimeIndex];
		const bool crimeIsObservable = (crime.IsValid() && sMayCrimeBeObserved(crime));
		foundObservableCrimes |= crimeIsObservable;
		crimeIsObservableArray[crimeIndex] = crimeIsObservable;
	}

	// If we didn't find anything, we don't have to look at the peds.
	if(!foundObservableCrimes)
	{
		// We are not going to do any tests this frame because there are no crimes
		// we can currently observer. Probably best to reset these variables since they
		// shouldn't be relevant when new crimes become available.
		m_LineOfSightPedIndexToCheck = 0;
		m_LineOfSightCrimeIndexToCheck = 0;
		m_LineOfSightNumAttempts = 0;
		m_LineOfSightCriminalDone = m_LineOfSightVictimDone = false;

		// Have another look in the crime array. It's conceivable that even though
		// we didn't want to consider observing the current crimes, there could still
		// be something in there that we may want to observe in the future. The primary
		// case I can think of is if we filled up the witness array for the crime, and
		// then some of those witnesses get eliminated (in practice, we may not actually
		// clean them out from the array, but anyway).
		// While we do some extra work here to account for this possibility, it should
		// still be cheap since most likely we will find that we can set
		// m_QueueMayContainUnobservedCrimes back to false, i.e. we only need to do it once.
		bool gotCrimes = false;
		for(int crimeIndex = 0; crimeIndex < maxCrimes; crimeIndex++)
		{
			const CCrimeBeingQd& crime = m_pWanted->CrimesBeingQd[crimeIndex];
			if(crime.IsValid() && sMayNeedFutureObservations(crime))
			{
				gotCrimes = true;
				break;
			}
		}

		// Update m_QueueMayContainUnobservedCrimes, making it go false if we don't
		// have any crimes that need more observation work. It will get set back to true
		// if new crimes get added, etc.
		m_QueueMayContainUnobservedCrimes = gotCrimes;
		return;
	}

	int numTests = 0;
	static const int maxTests = 1;

	// We use a local copy of this member variable to avoid LHS penalties, since we need
	// to both read and write it inside the loop.
	// Note: we need to be careful to not do any early returns without writing this back.
	int lineOfSightPedIndexToCheck = m_LineOfSightPedIndexToCheck;

	int numPedSlotsChecked = 0;
	while(numTests < maxTests)
	{
		const int maxPeds = CPed::GetPool()->GetSize();
		int foundPedIndex = -1;
		for(int pedIndex = lineOfSightPedIndexToCheck; numPedSlotsChecked < maxPeds;)
		{
			if(pedIndex >= maxPeds)
			{
				pedIndex = 0;
			}

			// Regardless of the outcome, we have checked this slot, so count it as such.
			numPedSlotsChecked++;

			const CPed* pPed = CPed::GetPool()->GetSlot(pedIndex);
			if(pPed && sMayPedPerceiveAnyCrimes(*pPed))
			{
				foundPedIndex = pedIndex;
				break;
			}
			pedIndex++;
		}

		if(foundPedIndex < 0)
		{
			break;
		}

		lineOfSightPedIndexToCheck = foundPedIndex;

		CPed& witness = *CPed::GetPool()->GetSlot(foundPedIndex);

		int foundCrimeIndex = -1;
		for(int crimeIndex = m_LineOfSightCrimeIndexToCheck; crimeIndex < maxCrimes; crimeIndex++)
		{
			Assert(crimeIndex >= 0 && crimeIndex < m_pWanted->m_MaxCrimesBeingQd);
			// Check if marked as observable in the temporary array, and if so, check if this
			// particular ped may observe it.
			if(crimeIsObservableArray[crimeIndex] && sMayPedObserveCrime(witness, m_pWanted->CrimesBeingQd[crimeIndex]))
			{
				foundCrimeIndex = crimeIndex;
				break;
			}
		}

		if(foundCrimeIndex < 0)
		{
			lineOfSightPedIndexToCheck++;
			m_LineOfSightCrimeIndexToCheck = 0;
			m_LineOfSightNumAttempts = 0;
			m_LineOfSightCriminalDone = m_LineOfSightVictimDone = false;
			continue;
		}

		bool done = TryObserveCrime(witness, m_pWanted->CrimesBeingQd[foundCrimeIndex]);

		if(!done)
		{
			static const int kMaxAttempts = 10;

			if(m_LineOfSightNumAttempts < kMaxAttempts)
			{
				m_LineOfSightNumAttempts++;
				break;
			}
			aiWarningf("Testing line of sight to crime didn't succeed in %d attempts, something may be wrong.", kMaxAttempts);
		}

		numTests++;

#if __BANK
		if(ms_BankDebugStepVisibilityTests && !ms_BankDebugDoStepVisibilityTests)
		{
			break;
		}
		ms_BankDebugDoStepVisibilityTests = false;
#endif

		m_LineOfSightCrimeIndexToCheck = foundCrimeIndex + 1;
		m_LineOfSightNumAttempts = 0;
		m_LineOfSightCriminalDone = m_LineOfSightVictimDone = false;
	}

	m_LineOfSightPedIndexToCheck = lineOfSightPedIndexToCheck;
}

void CCrimeWitnesses::UpdateObservationsFromVictim()
{
	//Ensure the queue may contain crimes that can be witnessed by their victims.
	if(!m_QueueMayContainCrimesThatCanBeWitnessedVictims)
	{
		return;
	}

	//Iterate over the crimes.
	for(int i = 0; i < m_pWanted->m_MaxCrimesBeingQd; ++i)
	{
		//Ensure the crime is valid.
		CCrimeBeingQd& rCrime = m_pWanted->CrimesBeingQd[i];
		if(!rCrime.IsValid())
		{
			continue;
		}

		//Ensure the crime may be perceived.
		if(!sMayCrimeBePerceived(rCrime))
		{
			continue;
		}

		//Ensure the victim is valid.
		CPed* pVictim = GetVictimPed(rCrime);
		if(!pVictim)
		{
			continue;
		}

		//Ensure the victim may perceive crimes.
		if(!sMayPedPerceiveAnyCrimes(*pVictim))
		{
			continue;
		}

		//Ensure the victim may perceive the crime.
		if(!sMayPedPerceiveCrime(*pVictim, rCrime))
		{
			continue;
		}

		//Note that the crime was witnessed.
		ProcessResultCrimeWitnessed(*pVictim, rCrime);
	}

	//Note that we have processed the queue.
	m_QueueMayContainCrimesThatCanBeWitnessedVictims = false;
}

void CCrimeWitnesses::LaunchReportCrimeResponse(CEntity& rEntity, const CCrimeBeingQd& rCrime)
{
	//Check if we should use a simulated report.
	bool bUseSimulatedReport = false;

	//Check if the entity is not a ped.
	if(!rEntity.GetIsTypePed())
	{
		//Use a simulated report.
		bUseSimulatedReport = true;
	}
	else
	{
		//Grab the ped.
		CPed& rPed = static_cast<CPed &>(rEntity);

		//Check if the witness is on foot.
		if(rPed.GetIsOnFoot())
		{
			//Ensure the report is valid.
			CReportingWitness* pReport = FindInactiveReport();
			if(!pReport)
			{
				return;
			}

			//Initialize the report.
			pReport->Init(rPed, &GetCriminal(rCrime), rCrime.GetPosition());
		}
		else
		{
			//Use a simulated report.
			bUseSimulatedReport = true;
		}
	}

	//Check if we should use a simulated report.
	if(bUseSimulatedReport)
	{
		//Wait a few seconds, then report the crime.
		static float s_fTimeBeforeReport = 5.0f;
		if((rCrime.GetTimeOfQing() + (s_fTimeBeforeReport * 1000.0f)) < fwTimer::GetTimeInMilliseconds())
		{
			//Report the witness as successful.
			ReportWitnessSuccessful(&rEntity);
		}
	}
}

void CCrimeWitnesses::LaunchReportCrimeResponses(const CCrimeBeingQd& rCrime)
{
	//Assert that we can launch report crime responses.
	aiAssert(CanLaunchReportCrimeResponses(rCrime));

	//Get the valid witnesses.
	atFixedArray<CEntity *, sMaxPerCrime> aWitnesses;
	GetWitnessesToNotifyLawEnforcement(rCrime, aWitnesses);
	if(aWitnesses.IsEmpty())
	{
		return;
	}

	//Calculate the available reports.
	int iNumAvailableReports = GetNumAvailableReports(rCrime);
	aiAssert(iNumAvailableReports > 0);

	//Check if the witnesses exceed the number of available reports.
	int iNumWitnesses = aWitnesses.GetCount();
	if(iNumWitnesses > iNumAvailableReports)
	{
		//Trim the witnesses.
		TrimWitnesses(rCrime, aWitnesses, iNumAvailableReports);
		iNumWitnesses = aWitnesses.GetCount();
		aiAssert(iNumWitnesses == iNumAvailableReports);
	}

	//Iterate over the witnesses.
	for(int i = 0; i < iNumWitnesses; ++i)
	{
		//Launch a report crime response.
		LaunchReportCrimeResponse(*aWitnesses[i], rCrime);
	}
}

void CCrimeWitnesses::ReportWitnessSuccessful(CEntity* pEntity)
{
	//Shut down the report.
	CReportingWitness* pReport = pEntity ? FindReportForEntity(*pEntity) : NULL;
	if(pReport)
	{
		pReport->Shutdown();
	}

	//Set all pending crimes as witnessed.
	m_pWanted->SetAllPendingCrimesWitnessedByLaw();
}

void CCrimeWitnesses::ReportWitnessUnsuccessful(CEntity* pEntity)
{
	//Shut down the report.
	CReportingWitness* pReport = pEntity ? FindReportForEntity(*pEntity) : NULL;
	if(pReport)
	{
		pReport->Shutdown();
	}
}

#if DEBUG_DRAW

void CCrimeWitnesses::DebugRender() const
{
	//Iterate over the reports.
	for(int i = 0; i < m_Reports.GetMaxCount(); ++i)
	{
		//Ensure the report is active.
		if(!m_Reports[i].IsActive())
		{
			continue;
		}

		//Debug render.
		m_Reports[i].DebugRender();
	}

	if(ms_WitnessesNeeded)
	{
		grcDebugDraw::Text(ms_WitnessReportPosition, Color_yellow, 0, 0, "Witness report position");
	}
}


void CCrimeWitnesses::GetWitnessInfoText(char* strOut, int maxLen, const CCrimeBeingQd& crime)
{
	if(ms_WitnessesNeeded)
	{
		formatf(strOut, maxLen, "Witnessed by %s", GetWitnessTypeName(crime.GetWitnessType()));
	}
	else
	{
		safecpy(strOut, "No witness needed", maxLen);
	}
}


const char* CCrimeWitnesses::GetWitnessTypeName(eWitnessType type)
{
	switch(type)
	{
		case eWitnessedNone:
			return "nobody";
		case eWitnessedByPeds:
			return "peds";
		case eWitnessedByLaw:
			return "law";
		default:
			return "unknown";
	}
}

#endif	// DEBUG_DRAW

#if __BANK

void CCrimeWitnesses::AddWidgets(bkBank& bank)
{
	bank.AddToggle("Need Witnesses", &ms_WitnessesNeeded);
	bank.AddVector("Witness Report Position", &ms_WitnessReportPosition, -10000.0f, 10000.0f, 0.1f);
	bank.AddToggle("Debug Step Witness Visibility Tests", &ms_BankDebugStepVisibilityTests);
	bank.AddButton("Debug Step", datCallback(CFA(CCrimeWitnesses::BankStepButtonCB)));
	bank.AddToggle("Debug Block Report", &ms_BankDebugBlockReport);
	bank.AddToggle("Draw Visibility Tests", &ms_BankDrawVisibilityTests);
}

#endif	// __BANK

bool CCrimeWitnesses::CanLaunchReportCrimeResponses(const CCrimeBeingQd& rCrime) const
{
	//Ensure the crime is not automatically witnessed by law.
	if(IsCrimeAutomaticallyReportedToLaw(rCrime))
	{
		return false;
	}

	//Ensure the crime has an available report.
	if(!HasAvailableReport(rCrime))
	{
		return false;
	}

	//Ensure the crime has been in the queue for a bit of time.  This allows us to collect witnesses.
	static float s_fMinTimeInQueue = 1.0f;
	if((rCrime.GetTimeOfQing() + (u32)(s_fMinTimeInQueue * 1000.0f)) > fwTimer::GetTimeInMilliseconds())
	{
		return false;
	}

	return true;
}

void CCrimeWitnesses::GetWitnessesToNotifyLawEnforcement(const CCrimeBeingQd& rCrime, atFixedArray<CEntity *, sMaxPerCrime>& rEntities) const
{
	//Check if the crime can only be reported by the victim.
	if(rCrime.bOnlyVictimCanNotifyLawEnforcement)
	{
		//Ensure the victim can notify law enforcement about the crime.
		CPed* pVictim = GetVictimPed(rCrime);
		if(pVictim && sCanPedNotifyLawEnforcement(*pVictim, GetCriminal(rCrime)))
		{
			rEntities.Push(pVictim);
		}
	}
	else
	{
		//Iterate over the witnesses.
		for(int i = 0; i < rCrime.m_Witnesses.GetCount(); ++i)
		{
			//Check if the entity can notify law enforcement.
			CEntity* pEntity = rCrime.m_Witnesses[i];
			if(pEntity && sCanNotifyLawEnforcement(*pEntity, GetCriminal(rCrime)))
			{
				rEntities.Push(pEntity);
			}
		}
	}
}

float CCrimeWitnesses::ScoreWitness(const CEntity& rEntity, const CCrimeBeingQd& UNUSED_PARAM(rCrime)) const
{
	//Calculate the score.
	float fScore = 1.0f;

	//Check if the entity is on-screen.
	if(rEntity.GetIsOnScreen())
	{
		fScore *= sm_Tunables.m_Scoring.m_BonusForOnScreen;
	}

	//Calculate the distance.
	//Use the local player position (instead of the crime position).
	//We use this so we get more interesting things happening on screen.
	Vec3V vPlayerPosition = VECTOR3_TO_VEC3V(CGameWorld::FindLocalPlayerCoors());
	Vec3V vEntityPosition = rEntity.GetTransform().GetPosition();
	float fDistSq = DistSquared(vPlayerPosition, vEntityPosition).Getf();

	//Calculate the score.
	float fDistanceForMaxSq = square(sm_Tunables.m_Scoring.m_DistanceForMax);
	float fDistanceForMinSq = square(sm_Tunables.m_Scoring.m_DistanceForMin);
	fDistSq = Clamp(fDistSq, fDistanceForMaxSq, fDistanceForMinSq);
	float fLerp = (fDistSq - fDistanceForMaxSq) / (fDistanceForMinSq - fDistanceForMaxSq);
	fScore *= Lerp(fLerp, sm_Tunables.m_Scoring.m_BonusForDistance, 1.0f);

	//Check if the entity is a vehicle.
	if(rEntity.GetIsTypeVehicle())
	{
		fScore *= sm_Tunables.m_Scoring.m_PenaltyForVehicle;
	}

	return 1.0f;
}

namespace
{
	struct ScoredWitness
	{
		ScoredWitness()
		: m_pEntity(NULL)
		, m_fScore(FLT_MIN)
		{}

		CEntity*	m_pEntity;
		float		m_fScore;
	};

	bool SortScoredWitnessesByScore(const ScoredWitness& rA, const ScoredWitness& rB)
	{
		float fScoreA = rA.m_fScore;
		float fScoreB = rB.m_fScore;
		if(fScoreA != fScoreB)
		{
			return (fScoreA > fScoreB);
		}

		return (rA.m_pEntity < rB.m_pEntity);
	}
}

void CCrimeWitnesses::TrimWitnesses(const CCrimeBeingQd& rCrime, atFixedArray<CEntity *, sMaxPerCrime>& rWitnesses, int iMaxWitnesses) const
{
	//Ensure we have too many witnesses.
	if(!aiVerify(rWitnesses.GetCount() > iMaxWitnesses))
	{
		return;
	}

	ScoredWitness aScoredWitnesses[sMaxPerCrime];
	int iNumScoredWitnesses = 0;

	//Score the witnesses.
	for(int i = 0; i < rWitnesses.GetCount(); ++i)
	{
		//Score the witness.
		CEntity* pEntity = rWitnesses[i];
		float fScore = ScoreWitness(*pEntity, rCrime);

		//Add the scored witness.
		ScoredWitness& rScoredEntity = aScoredWitnesses[iNumScoredWitnesses++];
		rScoredEntity.m_pEntity	= pEntity;
		rScoredEntity.m_fScore	= fScore;
	}

	//Sort the witnesses.
	std::sort(&aScoredWitnesses[0], &aScoredWitnesses[0] + iNumScoredWitnesses, SortScoredWitnessesByScore);

	//Add the witnesses with the top scores.
	rWitnesses.Reset();
	for(int i = 0; i < iMaxWitnesses; ++i)
	{
		rWitnesses.Push(aScoredWitnesses[i].m_pEntity);
	}
}

bool CCrimeWitnesses::TryObserveCrime(CPed& witness, CCrimeBeingQd& crime)
{
	bool victimDone = true;
	if(!m_LineOfSightVictimDone
#if __BANK
			|| ms_BankDebugStepVisibilityTests
#endif
			)
	{
		CEntity* pVictim = crime.GetVictim();

		if(pVictim == &witness)
		{
			ProcessResultCrimeWitnessed(witness, crime);
			return true;
		}

		if(pVictim && pVictim->GetIsTypePed())
		{
			bool seen = false;
			victimDone = CheckLineOfSightAndProcessResult(witness, static_cast<CPed&>(*pVictim), crime, seen);
			if(victimDone && seen)
			{
				return true;
			}
		}
	}

	bool criminalDone = true;
	if(!m_LineOfSightCriminalDone
#if __BANK
			|| ms_BankDebugStepVisibilityTests
#endif
			)
	{
		CPed& criminal = GetCriminal(crime);
		if(&witness != &criminal)
		{
			bool seen = false;
			criminalDone = CheckLineOfSightAndProcessResult(witness, criminal, crime, seen);
			if(criminalDone && seen)
			{
				return true;
			}
		}
	}

	m_LineOfSightCriminalDone = criminalDone;
	m_LineOfSightVictimDone = victimDone;

	return victimDone && criminalDone;
}

bool CCrimeWitnesses::TryToHearCrime(CEntity& rEntity, CCrimeBeingQd& rCrime)
{
	//Note that the crime was witnessed.
	ProcessResultCrimeWitnessed(rEntity, rCrime);

	return true;
}

bool CCrimeWitnesses::CheckLineOfSightAndProcessResult(CPed& witness, CPed& tgt, CCrimeBeingQd& crime, bool& seenOut)
{
	seenOut = false;
	const ECanTargetResult los = sCheckWitnessPed(witness, tgt);
	bool done = false;

	if(los == ECanTarget)
	{
		done = true;

		ProcessResultCrimeWitnessed(witness, crime);
		seenOut = true;
	}
	else if(los == ECanNotTarget)
	{
		done = true;
	}
	return done;

}


void CCrimeWitnesses::ProcessResultCrimeWitnessed(CEntity& rEntity, CCrimeBeingQd& rCrime)
{
	//Check if the crime has not reached the maximum witnesses.
	if(!rCrime.HasMaxWitnesses())
	{
		//Add the witness.
		rCrime.AddWitness(rEntity);
	}

	//Generate the witness type.
	eWitnessType nWitnessType = eWitnessedByPeds;

	//Check if the entity is a ped.
	if(rEntity.GetIsTypePed())
	{
		//Check if the ped is law enforcement.
		const CPed& rPed = static_cast<const CPed &>(rEntity);
		if(rPed.IsLawEnforcementPed())
		{
			nWitnessType = eWitnessedByLaw;
		}
	}
	//Check if the entity is a vehicle.
	else if(rEntity.GetIsTypeVehicle())
	{
		//Check if the vehicle is law enforcement.
		//@@: location CCRIMEWITNESSES_PROCESSRESULTCRIMEWITNESSED
		const CVehicle& rVehicle = static_cast<const CVehicle &>(rEntity);
		if(rVehicle.IsLawEnforcementVehicle())
		{
			nWitnessType = eWitnessedByLaw;
		}
	}

	//Set the witness type.
	rCrime.SetWitnessType(nWitnessType);
	if(nWitnessType == eWitnessedByLaw)
	{
		//Report the crimes.
		m_pWanted->SetAllPendingCrimesWitnessedByLaw();
	}
}

CReportingWitness* CCrimeWitnesses::FindActiveReport()
{
	//Iterate over the reports.
	for(int i = 0; i < m_Reports.GetMaxCount(); ++i)
	{
		//Check if the report is active.
		if(m_Reports[i].IsActive())
		{
			return &m_Reports[i];
		}
	}

	return NULL;
}

CReportingWitness* CCrimeWitnesses::FindInactiveReport()
{
	//Iterate over the reports.
	for(int i = 0; i < m_Reports.GetMaxCount(); ++i)
	{
		//Check if the report is inactive.
		if(!m_Reports[i].IsActive())
		{
			return &m_Reports[i];
		}
	}

	return NULL;
}

CReportingWitness* CCrimeWitnesses::FindReportForEntity(const CEntity& rEntity)
{
	//Iterate over the reports.
	for(int i = 0; i < m_Reports.GetMaxCount(); ++i)
	{
		//Check if the entity matches.
		if(m_Reports[i].IsActive() && (&rEntity == m_Reports[i].GetWitness()))
		{
			return &m_Reports[i];
		}
	}

	return NULL;
}

CPed& CCrimeWitnesses::GetCriminal(const CCrimeBeingQd& /*crime*/) const
{
	CPed* pPed = m_pWanted->GetPed();
	Assert(pPed);
	return *pPed;
}

CPed* CCrimeWitnesses::GetVictimPed(const CCrimeBeingQd& rCrime)
{
	//Ensure the entity is valid.
	CEntity* pEntity = rCrime.GetVictim();
	if(!pEntity)
	{
		return NULL;
	}

	//Ensure the entity is a ped.
	if(!pEntity->GetIsTypePed())
	{
		return NULL;
	}

	CPed* pPed = static_cast<CPed *>(pEntity);
	return pPed;
}

//-----------------------------------------------------------------------------
