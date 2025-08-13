// 
// sagworld/CloudListController.cpp 
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#include "CloudListController.h"

// Parser generated files
#include "CloudListController_parser.h"

#include "CloudHat.h"
#include "math\random.h"
#include "bank\bank.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_MISC_OPTIMISATIONS()


namespace rage
{

namespace
{
	static const CloudListController::probability_array_type::value_type sDefaultProbability = 1;
};

CloudListController::CloudListController()
: mProbability()
, mBits()
#if __BANK
, mBank(NULL)
, mGroup(NULL)
, mList(NULL)
#endif
{
	//Fixup bitset if needed.
	FixupBitset();
}

CloudListController::CloudListController(const bitset_type &bits)
: mProbability(bits.GetNumBits(), bits.GetNumBits())//count and capacity
, mBits(bits)
#if __BANK
, mBank(NULL)
, mGroup(NULL)
, mList(NULL)
#endif
{
	std::fill(mProbability.begin(), mProbability.end(), sDefaultProbability);

	//Fixup bitset if needed.
	FixupBitset();
}

void CloudListController::UpdateUI()
{
#if __BANK
	//Reset the list while we still know how many entries are in it..(in case FixupBistset changes this)
	ClearList();

	//See if we need to update the bitset. (due to CloudHat number change possibly)
	FixupBitset();

	//Setup the list with our new data.
	SetupList();

	//Update the Probability controls.
	UpdateProbabilityControls();
#endif
}

//This decides on an appropriate CloudHat from the active list for the associated weather type.
int CloudListController::SelectNewActiveCloudHat() const
{
	probability_array_type::value_type sumProbability = 0;
	int newCloudHat = 0;
	if(Verifyf(CLOUDHATMGR, "CloudHatManager has not yet been initialized! Current call may not succeed..."))
		newCloudHat = CLOUDHATMGR->GetNumCloudHats(); //No CloudHat
	int numActive = mBits.CountOnBits();

	//Sum all active probabilities
	for(int i = 0; i < numActive; ++i)
	{
		sumProbability += mProbability[mBits.GetNthBitIndex(i)];
	}

	//Roll the dice...
	if(sumProbability > 0)
	{
		int randSelector = g_DrawRand.GetInt() % sumProbability;

		//Find out which CloudHat won!
		probability_array_type::value_type currSum = 0;
		for(int i = 0; i < numActive; ++i)
		{
			int currCloudHat = mBits.GetNthBitIndex(i);
			currSum += mProbability[currCloudHat];
			
			if(randSelector < currSum)
			{
				//We found our winner!!
				newCloudHat = currCloudHat;
				break;
			}
		}
	}

	return newCloudHat;
}

bool CloudListController::IsCloudHatInList(int index) const
{
	bool isInList = false;
	int numBits = mBits.GetNumBits();
	if(index >= 0 && index < numBits)
		isInList = mBits.IsSet(static_cast<u32>(index));

	return isInList;
}

void CloudListController::PreLoad(parTreeNode * /*node*/)
{
	//Need to reset arrays before loading until parser code is fixed. (Otherwise, loading may crash if arrays are already set)
	mProbability.Reset();
	mBits.Reset();
}

void CloudListController::PostLoad()
{
	//After loading, we need to fixup the bitset. If we don't, it may remain uninitialized and eventually crash.
	FixupBitset();
}

void CloudListController::FixupBitset()
{
	//Make sure bit set represents the correct number of bits as there are CloudHats.
	//If it does not, something is out of date, and we need to try to compensate.
	int prevNumBits = mBits.GetNumBits();
	CloudHatManager::size_type numCloudHats = 0;
	if(Verifyf(CLOUDHATMGR, "CloudHatManager has not yet been initialized! Current call may not succeed..."))
		numCloudHats = CLOUDHATMGR->GetNumCloudHats();
	CloudHatManager::size_type numCloudHatsWithNoCloudHat = numCloudHats + 1;
	if(prevNumBits != numCloudHatsWithNoCloudHat)
	{
		//Since noCloudHat is always at the end of the array, we need to handle it separately to try and preserve it's value.
		int noCloudHatActiveIndex = prevNumBits - 1;
		bool isNoCloudHatActive = false;
		if(noCloudHatActiveIndex > 0 && noCloudHatActiveIndex < prevNumBits)
		{
			isNoCloudHatActive = mBits.GetAndClear(prevNumBits - 1);
		}

		//Union doesn't respect new bitset's size completely. (Respects per-word size, but not individual bit size.)
		//So reset trailing bits if the new bitset has been reduced in size.
		int diff = prevNumBits - numCloudHatsWithNoCloudHat;
		for(int i = prevNumBits - diff; i < prevNumBits; ++i)
		{
			mBits.Set(i, false);
		}

		//Fix Bitst
		bitset_type newBits(numCloudHatsWithNoCloudHat);
		newBits.Union(mBits);
		mBits = newBits;

		//Set noCloudHat value
		mBits.Set(numCloudHats, isNoCloudHatActive);
	}

	probability_array_type::size_type numProb = mProbability.size();
	probability_array_type::size_type probCapacity = mProbability.GetCapacity();
	if(numProb != numCloudHatsWithNoCloudHat)
	{
		//Again, noCloudHat case needs to be handled separately to try and preserve it's value.
		probability_array_type::size_type noCloudHatProbabilityIndex = numProb - 1;
		probability_array_type::value_type noCloudHatProbability = sDefaultProbability;
		if(noCloudHatProbabilityIndex > 0 && noCloudHatProbabilityIndex < numProb)
		{
			noCloudHatProbability = mProbability[noCloudHatProbabilityIndex];
			mProbability[noCloudHatProbabilityIndex] = sDefaultProbability;
		}

		//Fix Probability array
		if(numCloudHatsWithNoCloudHat < probCapacity || probCapacity == 0)
		{
			mProbability.Resize(numCloudHatsWithNoCloudHat);

			//Initialize any new members
			probability_array_type::iterator begin = mProbability.begin();
			probability_array_type::iterator end = mProbability.end();
			probability_array_type::iterator iter = (begin + numProb) < end ? begin + numProb : end;
			std::fill(iter, end, sDefaultProbability);
		}
		else
		{
			//Need to reserve more memory.
			for(int i = numProb; i < numCloudHatsWithNoCloudHat; ++i)
			{
				mProbability.PushAndGrow(sDefaultProbability);
			}
		}

		//Now that array has been resized, set previous noCloudHat probability
		mProbability[numCloudHats] = noCloudHatProbability;
	}
}

#if __BANK
namespace
{
	enum ListColumns
	{
		COL_CLOUDHAT_NAME,
		COL_ON,
		COL_OFF,
		COL_PROBABILITY,

		NUM_LIST_COLS
	};

	static const char *sColTitles[NUM_LIST_COLS] =
	{
		"CloudHat Name",	//COL_CLOUDHAT_NAME
		"On",				//COL_ON
		"Off",				//COL_OFF
		"Probability (%)",	//COL_PROBABILITY
	};

	static const bkList::ItemType sColItemType[NUM_LIST_COLS] =
	{
		bkList::STRING,	//COL_CLOUDHAT_NAME
		bkList::STRING,	//COL_ON
		bkList::STRING,	//COL_OFF
		bkList::FLOAT,	//COL_PROBABILITY
	};

	static const char *sOnString[2] =
	{
		"",
		"On"
	};

	static const char *sOffString[2] =
	{
		"Off",
		""
	};

	static const char *sNoCloudHat = "No CloudHat";
	static const char *sInvlaid = "INVALID!!";

	const char *GetColumnString(CloudHatManager::size_type row, u32 col, bool isOn = true)
	{
		const char *strVal = NULL;
		isOn = !!isOn;	//Make sure isOn is between [0,1];
		switch(col)
		{
		case COL_CLOUDHAT_NAME:
			if(Verifyf(CLOUDHATMGR, "CloudHatManager has not yet been initialized! Current call may not succeed..."))
				strVal = row < CLOUDHATMGR->GetNumCloudHats() ? CLOUDHATMGR->GetCloudHat(row).GetName() : sNoCloudHat;
			else
				strVal = sNoCloudHat;
			break;

		case COL_ON:
			strVal = sOnString[isOn];
			break;

		case COL_OFF:
			strVal = sOffString[isOn];
			break;

		default:
			strVal = sInvlaid;
			break;
		}

		return strVal;
	}

	static char sLastCloudHatWinText[CloudHatFragContainer::kMaxNameLength] = {0};

	static void SelectNewCloudHatBtn(const CloudListController *cloudList)
	{
		if(Verifyf(CLOUDHATMGR, "CloudHatManager has not yet been initialized! Current call may not succeed..."))
		{
			int cloudHatWinIndex = cloudList->SelectNewActiveCloudHat();
			Assertf(cloudHatWinIndex >= 0 && cloudHatWinIndex <= CLOUDHATMGR->GetNumCloudHats(),
					"Debug CloudHat selection test selected a new CloudHat index of %d. That is not in the valid CloudHat range of [0, %d]! " 
					"Please check selection algorithm to make sure it works as expected.", cloudHatWinIndex, CLOUDHATMGR->GetNumCloudHats() + 1);

			safecpy(sLastCloudHatWinText, GetColumnString(cloudHatWinIndex, COL_CLOUDHAT_NAME), CloudHatFragContainer::kMaxNameLength);
		}
	}
}

void CloudListController::AddWidgets(bkBank &bank)
{
	mBank = &bank;
	mGroup = bank.PushGroup("Associated Cloud List", false);
	{
		bank.AddTitle("Associated Clouds:\n");
		bank.AddTitle("Select which CloudHats should be associated with this weather setting. To select");
		bank.AddTitle("a specific CloudHat, double-click on it until it's state is set to \"On\".");

		mList = bank.AddList("CloudHats Listbox");
		bank.AddButton("Update List", datCallback(MFA(CloudListController::UpdateUI), this));
		InitList();
		SetupProbabilityControls(bank);
	}
	bank.PopGroup();
}

//Initializes list
void CloudListController::InitList()
{
	if(mList)
	{
		//Bind Functors
		mUpdateFunc.Reset<CloudListController, &CloudListController::ListUpdateCallback>(this);
		mDoubleClickFunc.Reset<CloudListController, &CloudListController::ListDoubleClickCallback>(this);

		//Attach functors to our listbox
		mList->SetUpdateItemFunc(mUpdateFunc);
		mList->SetDoubleClickItemFunc(mDoubleClickFunc);

		//Setup list headers
		for(int i = 0; i < NUM_LIST_COLS; ++i)
		{
			mList->AddColumnHeader(i, sColTitles[i], sColItemType[i]);
		}

		//Setup the list data
		SetupList();
		RecomputeListProbabilityCol();
	}
}

//Deletes all the data in the list then sets it up the list again.
void CloudListController::ClearList()
{
	if(mList)
	{
		//Remove all entries in list
		int numBits = mBits.GetNumBits();
		for(int i = 0; i < numBits; ++i)
		{
			mList->RemoveItem(i);
		}
	}
}

//Sets up the initially blank list.
void CloudListController::SetupList()
{
	if(mList)
	{
		//Add all CloudHats to list
		CloudHatManager::size_type numCloudHats = 0;
		if(Verifyf(CLOUDHATMGR, "CloudHatManager has not yet been initialized! Current call may not succeed..."))
			numCloudHats = CLOUDHATMGR->GetNumCloudHats();
		CloudHatManager::size_type numCloudHatsWithNoCloudHat = numCloudHats + 1;
		for(int i = 0; i < numCloudHatsWithNoCloudHat; ++i)
		{
			for(int col = 0; col < NUM_LIST_COLS; ++col)
			{
				if(col != COL_PROBABILITY)
				{
					mList->AddItem(i, col, GetColumnString(i, col, mBits.IsSet(i)));
				}
			}
		}
	}
}

void CloudListController::RecomputeListProbabilityCol()
{
	if(mList)
	{
		//Sum all active probabilities
		probability_array_type::value_type sumProbability = 0;
		int numActive = mBits.CountOnBits();
		for(int i = 0; i < numActive; ++i)
		{
			sumProbability += mProbability[mBits.GetNthBitIndex(i)];
		}

		//Add all probabilities to list
		static const float sNoProbability = 0.0f;
		CloudHatManager::size_type numCloudHats = 0;
		if(Verifyf(CLOUDHATMGR, "CloudHatManager has not yet been initialized! Current call may not succeed..."))
			numCloudHats = CLOUDHATMGR->GetNumCloudHats();
		CloudHatManager::size_type numCloudHatsWithNoCloudHat = numCloudHats + 1;
		for(int i = 0; i < numCloudHatsWithNoCloudHat; ++i)
		{
			float probability = mBits.IsSet(i) ? (static_cast<float>(mProbability[i]) / static_cast<float>(sumProbability)) * 100.0f : sNoProbability;
			mList->AddItem(i, COL_PROBABILITY, probability);
		}
	}
}

void CloudListController::UpdateProbabilityControls()
{
	ClearProbabilityControls();

	//We need to set the correct bank context for SetProbabilityControls to work.
	if(mBank)
	{
		bkBank &bank = *mBank;
		bank.SetCurrentGroup(*mGroup);
		SetupProbabilityControls(*mBank);
		bank.UnSetCurrentGroup(*mGroup);
	}

	//Lastly, update the list with new probability percentages.
	RecomputeListProbabilityCol();
}

void CloudListController::ClearProbabilityControls()
{
	if(mList)
	{
		//Should be Update List button widget
		bkWidget *next = mList->GetNext();

		if(next)
		{
			next = next->GetNext(); //Should finally be 1st probability control.
			while(next)
			{
				bkWidget *currWidget = next;
				next = currWidget->GetNext();
				currWidget->Destroy();
			}
		}
	}
}

void CloudListController::SetupProbabilityControls(bkBank &bank)
{
	static const int sMinVal = 1;
	static const int sMaxVal = 500;
	static const int sDeltaVal = 1;
	int numActive = mBits.CountOnBits();

	bank.AddSeparator();
	bank.AddTitle("Probability for each active CloudHat:");
	bank.AddTitle("NOTE: The way these values work are as if each value represents a number of pieces of paper in a hat.");
	bank.AddTitle("The more pieces, the more likely it will be selected. See the list for probability percentage calculations.");

	for(int i = 0; i < numActive; ++i)
	{
		int cloudHatIndex = mBits.GetNthBitIndex(i);
		bank.AddSlider(GetColumnString(cloudHatIndex, COL_CLOUDHAT_NAME), &mProbability[cloudHatIndex], sMinVal, sMaxVal, sDeltaVal, datCallback(MFA(CloudListController::RecomputeListProbabilityCol), this));
	}

	//For debugging and to get a feeling of the probabilities
	bank.AddSeparator();
	bank.AddTitle("Test/Sample CloudHat probability-based selection:");
	bank.AddTitle("NOTE: This button is meant as debug only to test/get a feeling for how the probabilities will affect Skyaht selection.");

	bank.AddText("Last selected CloudHat:", sLastCloudHatWinText, CloudHatFragContainer::kMaxNameLength, true);
	bank.AddButton("Select another CloudHat!", datCallback(CFA1(SelectNewCloudHatBtn), this));
}

void CloudListController::ListUpdateCallback(s32 /*row*/, s32 /*column*/, const char * /*newVal*/)
{
	//List is read only.
}

void CloudListController::ListDoubleClickCallback(s32 row)
{
	//Update bit for this row.
	mBits.Toggle(row);

	//Update list to reflect new state!
	Assertf(mList, "How did the List callback get called if list is NULL?!");

	for(int col = 0; col < NUM_LIST_COLS; ++col)
	{
		if(col != COL_PROBABILITY)
		{
			mList->AddItem(row, col, GetColumnString(row, col, mBits.IsSet(row)));
		}
	}

	UpdateProbabilityControls();
}
#endif //__BANK

} //namespace rage
