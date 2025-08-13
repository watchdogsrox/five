// FILE :    AnimBlackboard.cpp
// PURPOSE : Provides a place to track ambient animations as they play to prevent 
//						playing the same animation too often.
// AUTHOR :  Michael Dawe
// CREATED : 20 Dec 2012

#include "AnimBlackboard.h"

#include "Peds/PedIntelligence.h"

AI_OPTIMISATIONS()
//OPTIMISATIONS_OFF()

CAnimBlackboard CAnimBlackboard::m_instance;
const u32 CAnimBlackboard::sm_MinTimeBetweenAnims[MAX_BLACKBOARD_TIME_DELAY_TYPES] = { 2000, 350 };
const ScalarV CAnimBlackboard::vMinSpecialTypeDistanceSq = ScalarV(V_FOUR);

CAnimBlackboard::CAnimBlackboard()
: m_blackboard(BLACKBOARD_SIZE)
, m_uNewestIndex(0)
{

}

CAnimBlackboard::~CAnimBlackboard()
{

}

void CAnimBlackboard::Init(unsigned UNUSED_PARAM(initMode))
{
	if(!CAnimBlackboardSingleton::IsInstantiated()) 
	{
		CAnimBlackboardSingleton::Instantiate();
	}
}

void CAnimBlackboard::Shutdown(unsigned UNUSED_PARAM(shutdownMode))
{

}

// Checks to see if the clip in the supplied BlackboardItem can be played. If it can, adds the item to the blackboard and supplied
// ped intelligence and returns true. If the item cannot be played, returns false.
bool CAnimBlackboard::AddItemToBlackboardAndPedIntelligenceIfAllowedToPlay(const AnimBlackboardItem& item, CPedIntelligence* pPedIntelligence)
{
	if (CanPlayAnim(item))
	{
		AddItemToBlackboardAndPedIntelligence(item, pPedIntelligence);
		return true;
	}
	return false;
}


void CAnimBlackboard::AddItemToBlackboardAndPedIntelligence(const AnimBlackboardItem& item, CPedIntelligence* pPedIntelligence)
{
	// Set the pPedIntelligence item first.
	if (pPedIntelligence)
	{
		pPedIntelligence->SetLastBlackboardAnimPlayed(item);
	}

	// Check to see if we have an existing instance of this item in the list [12/20/2012 mdawe]
	const int iExistingIndex = FindAnimItemIndex(item);
	if (iExistingIndex != -1)
	{
		m_blackboard[iExistingIndex] = item;
		return;
	}

	//Add the item to the list with the current time
	m_blackboard[m_uNewestIndex++] = item;
	
	// Check to see if our index wrapped around [12/20/2012 mdawe]
	if (m_uNewestIndex == BLACKBOARD_SIZE)
	{
		m_uNewestIndex -= BLACKBOARD_SIZE;
	}
}


u32 CAnimBlackboard::TimeSinceLastPlayedAnim(const AnimBlackboardItem& item) const
{
	const int iItemIndex = FindAnimItemIndex(item);
	if (iItemIndex > -1)
	{
		const u32 timeSince = fwTimer::GetTimeInMilliseconds() - m_blackboard[iItemIndex].GetTimePlayed();
		return timeSince;
	}
	return 0xFFFF;
}

// Checks the blackboard for another anim matching the given item's special type within our minimum reaction distance and time.
bool CAnimBlackboard::IsPlayingSpecialTypeWithinTimeAndDistance(const AnimBlackboardItem& item) const
{
	for (unsigned scan = 0; scan < BLACKBOARD_SIZE; ++scan)
	{
		// Does this item have the same SpecialType?
		if ( m_blackboard[scan].GetSpecialType() == item.GetSpecialType())
		{
			// Is it within our time requirement?
			const u32 timeSince = fwTimer::GetTimeInMilliseconds() - m_blackboard[scan].GetTimePlayed();
			if ( timeSince < CAnimBlackboard::sm_MinTimeBetweenAnims[item.GetTimeType()])
			{
				// Is it within our distance requirement?
				if ( !IsZeroAll(m_blackboard[scan].GetLocation()) && IsLessThanAll(DistSquared(m_blackboard[scan].GetLocation(), item.GetLocation()), CAnimBlackboard::vMinSpecialTypeDistanceSq) )
				{
					return true;
				} 
			}
		}
	}

	return false;
}


bool CAnimBlackboard::CanPlayAnim(const AnimBlackboardItem& item) const
{ 
	// If the item we're checking against has a special type defined, check for any other anim in the blackboard of that type.
	if (item.GetSpecialType() != CAnimBlackboard::NO_SPECIAL_ANIM)
	{
		return !IsPlayingSpecialTypeWithinTimeAndDistance(item);
	}

	// Otherwise, just check the time on this specific animation.
	return TimeSinceLastPlayedAnim(item) > CAnimBlackboard::sm_MinTimeBetweenAnims[item.GetTimeType()]; 
}


// Returns the existing index of a blackboard item in the list, or -1 if not found
int CAnimBlackboard::FindAnimItemIndex(const AnimBlackboardItem& item) const
{
	for (unsigned scan = 0; scan < BLACKBOARD_SIZE; ++scan)
	{
		if ( m_blackboard[scan] == item )
		{
			return scan;
		}
	}
	return -1;
}
