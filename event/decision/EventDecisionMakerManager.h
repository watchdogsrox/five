#ifndef INC_EVENT_DECISION_MAKER_MANAGER_H
#define INC_EVENT_DECISION_MAKER_MANAGER_H

// Game headers
#include "Event/Decision/EventDecisionMaker.h"
#include "Event/Decision/EventDecisionMakerResponse.h"
#include "Event/System/EventDataManager.h"

//////////////////////////////////////////////////////////////////////////
// CEventDecisionMakerManager
//////////////////////////////////////////////////////////////////////////

class CEventDecisionMakerManager
{
public:

	// Static initialisation
	static void Init();

	// Static shutdown
	static void Shutdown();

	// 
	static bool GetIsDecisionMakerValid(u32 uDecisionMakerId);

	//
	static CEventDecisionMaker* GetDecisionMaker(u32 uDecisionMakerId);

	// 
	static CEventDecisionMaker* CloneDecisionMaker(u32 uDecisionMakerId, const char* pNewName);

	//
	static void DeleteDecisionMaker(u32 uDecisionMakerId);

	//
	// 
	//

private:

	//
	// Searching
	//

	// Search for a decision maker with the specified id using a binary search
	static s32 GetDecisionMakerIndexById(u32 uId);

	// Function to sort the array so we can use a binary search
	static s32 PairSort(const CEventDecisionMaker** a, const CEventDecisionMaker** b) { return (*a)->GetId() < (*b)->GetId() ? -1 : 1; }

	// Sort the decision makers for binary searching
	static void SortDecisionMakers() { qsort(&ms_decisionMakers[0], ms_decisionMakers.GetCount(), sizeof(CEventDecisionMaker*), (int (/*__cdecl*/ *)(const void*, const void*))PairSort); }

	//
	// Members
	//

	// Decision maker storage
	typedef atFixedArray<CEventDecisionMaker*, CEventDecisionMaker::MAX_STORAGE> DecisionMakers;
	static DecisionMakers ms_decisionMakers;
};

#endif // INC_EVENT_DECISION_MAKER_MANAGER_H
