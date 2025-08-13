//
// Task/Default/TaskArrest.h
//
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_ARREST_HELPERS_H
#define INC_ARREST_HELPERS_H

class CPed;
namespace rage { class bkBank; }

#if __BANK
#define ARREST_DISPLAYF(Arrester, Arrestee, fmt,...)	taskDisplayf(fmt,##__VA_ARGS__);\
	taskDisplayf("[%d] Arrester: %s, Arrestee: %s\n",\
	CNetwork::GetNetworkTime(),\
	((Arrester) && (Arrester)->GetNetworkObject()) ? (Arrester)->GetNetworkObject()->GetLogName() : "None",\
	((Arrestee) && (Arrestee)->GetNetworkObject()) ? (Arrestee)->GetNetworkObject()->GetLogName() : "None");
#else
#define ARREST_DISPLAYF(Arrester, Arrestee, fmt,...)
#endif

class CArrestHelpers
{
public:

	//! Purpose: Script use these to identify between different arrest types.
	enum eArrestTypes
	{
		ARREST_FALLBACK = -1,
		ARREST_CUFFING = 0,
		ARREST_UNCUFFING,
		ARREST_COUNT,
	};

	//! Purpose: Script use these to identify between different arrest event types.
	enum eArrestEvent
	{
		EVENT_CUFF_SUCCESSFUL = 0,
		EVENT_CUFF_FAILED,
		EVENT_UNCUFF_SUCCESSFUL,
		EVENT_UNCUFF_FAILED,
		EVENT_TAKENCUSTODY,
		EVENT_CUSTODY_EXIT_CUSTODIAN_DEAD,
		EVENT_CUSTODY_EXIT_ABANDONED,
		EVENT_CUSTODY_EXIT_CUSTODIAN_OUTOFRANGE,
		EVENT_CUSTODY_EXIT_UNCUFFED,
		EVENT_TAKENCUSTODY_FAILED,
		EVENT_COUNT,
	};

	static float GetArrestNetworkTimeout();
	static bool IsArrestTypeValid(const CPed *pCopPed, const CPed *pCrookPed, int ArrestType);

	static bool UsingNewArrestAnims();
	static bool CanArresterStepAroundToCuff();
};

#if __BANK

class CArrestDebug
{
public:

	// Setup the RAG widgets for climbing
	static void SetupWidgets(bkBank& bank);

	static float GetPairedAnimOverride();

private:

	static void ToggleHandcuffed();
	static void ToggleCanBeArrested();
	static void ToggleCanArrest();
	static void ToggleCanUnCuff();
	static void RemoveFromCustody();
	static void LocalPedTakeCustody();
	static void CreateArrestablePed();
	static void CreateUncuffablePed();
	static void IncapacitatedFocusPed();
	static void SetRelationshipGroupToCop();
	static void SetRelationshipGroupToGang();

	static void ToggleConfigFlag(int nFlag);
};

#endif // __BANK

#endif // INC_ARREST_HELPERS_H
