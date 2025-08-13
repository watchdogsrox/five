// 
// shadercontrollers/CloudListController.h 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef SHADERCONTROLLERS_CLOUDLISTCONTROLLER_H
#define SHADERCONTROLLERS_CLOUDLISTCONTROLLER_H 

#include "data\base.h"
#include "parser\manager.h"

#include "atl\bitset.h"
#include "atl\atfunctor.h"
#include "bank\list.h"

namespace rage
{

//
//PURPOSE:
//	CloudListController handles tracking the on/off states of per-weather CloudHat selections
//	and updates the Rag list widget associated with it.

class CloudListController : public datBase
{
public:
	typedef atBitSet bitset_type;
	typedef atArray<int> probability_array_type;

	CloudListController();
	CloudListController(const bitset_type &bits);

	void UpdateUI();

	//If no CloudHat is in the active list, active CloudHat will be NumCloudHats, which is an invalid CloudHat index.
	int SelectNewActiveCloudHat() const;

	bool IsCloudHatInList(int index) const;

#if __BANK
	void AddWidgets(bkBank &bank);
#endif

private:
	probability_array_type mProbability;
	bitset_type mBits;

	void PreLoad(parTreeNode *node);
	void PostLoad();

	void FixupBitset();	//Called when there might be a mismatch between bitset size and num CloudHats

#if __BANK
	typedef bkList::UpdateItemFuncType ListUpdateFunctor;
	typedef bkList::ClickItemFuncType DoubleClickFunctor;

	//Helpers
	void InitList(); //Initializes list
	void ClearList(); //Deletes all the data in the list.
	void SetupList(); //Sets up the initially blank list.
	void RecomputeListProbabilityCol();
	void UpdateProbabilityControls();
	void ClearProbabilityControls();
	void SetupProbabilityControls(bkBank &bank);

	//Callbacks
	void ListUpdateCallback(s32 row, s32 column, const char *newVal);
	void ListDoubleClickCallback(s32 row);

	bkBank *mBank;
	bkGroup *mGroup;

	bkList *mList;
	ListUpdateFunctor mUpdateFunc;
	DoubleClickFunctor mDoubleClickFunc;
#endif

	PAR_PARSABLE;
};

}	//namespace rage

#endif //SHADERCONTROLLERS_CLOUDLISTCONTROLLER_H
