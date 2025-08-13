// Framework headers
#include "ai/aichannel.h"
#include "fwanimation/clipsets.h"

// Game Headers
#include "animation/Move.h"
#include "Game/Clock.h"
#include "game/weather.h"
#include "Game/ModelIndices.h"
#include "Peds/Ped.h"
#include "Peds/PedIntelligence.h"
#include "Peds/Ped.h"
#include "Scene/World/GameWorld.h"
#include "scene/EntityIterator.h"
#include "streaming/streaming.h"		// For CStreaming::RequestObject(), etc.
#include "Task/Default/AmbientAnimationManager.h"
#include "AI/Ambient/AmbientModelSetManager.h"
#include "Task/Default/TaskAmbient.h"
#include "Task/General/TaskBasic.h"
#include "Task/Scenario/ScenarioManager.h"
#include "Task/Scenario/info/ScenarioInfoManager.h"
#include "Task/Scenario/TaskScenario.h"
#include "Task/Scenario/Types/TaskUseScenario.h"
#include "Weapons/Weapon.h"

AI_OPTIMISATIONS()

void CAmbientAnimationManager::Init(unsigned /*initMode*/)
{
//	ms_bInitialised = true;
}

void CAmbientAnimationManager::Shutdown(unsigned /*shutdownMode*/)
{
//	ms_bInitialised = false;
}

/*
//-------------------------------------------------------------------------
// Adds an ambient group in and returns a pointer so conditions can be set
//-------------------------------------------------------------------------
CAmbientSet* CAmbientAnimationManager::AddAmbientSet( const fwMvClipSetId &ambientSetId )
{
	CAmbientSet* pNewGroup = rage_new CAmbientSet();
	pNewGroup->SetClipSet(ambientSetId);
	ms_aAmbientGroups.PushAndGrow(pNewGroup);
	return pNewGroup;
}

//-------------------------------------------------------------------------
// Returns the ambient group
//-------------------------------------------------------------------------
CAmbientSet* CAmbientAnimationManager::GetAmbientSet( const fwMvClipSetId &clipSetId )
{
	for (s32 i = 0; i < ms_aAmbientGroups.GetCount(); i++)
	{
		if (ms_aAmbientGroups[i]->GetClipSet() == clipSetId)
		{
			return ms_aAmbientGroups[i];
		}
	}

	return NULL;
}
*/

/*
//-------------------------------------------------------------------------
// Returns the ambient group set with the given name
//-------------------------------------------------------------------------
s32 CAmbientAnimationManager::GetAmbientGroupSetIndex	(const char* szName )
{
	for( s32 i = 0; i < ms_aAmbientGroupSets.GetCount(); i++ )
	{
		if( ms_aAmbientGroupSets[i]->IsActive() )
		{
			if( ms_aAmbientGroupSets[i]->GetHash() == atStringHash(szName) ) 
			{
				return i;
			}
		}
	}
	return -1;
}


//-------------------------------------------------------------------------
// Returns the ambient group set from the index
//-------------------------------------------------------------------------
CAmbientSetList* CAmbientAnimationManager::GetAmbientSetList( s32 iSet )
{
	aiAssert( iSet >= 0 && iSet < ms_aAmbientGroupSets.GetCount() );
	aiAssert( ms_aAmbientGroupSets[iSet]->IsActive() );
	return ms_aAmbientGroupSets[iSet];
}

//-------------------------------------------------------------------------
// Adds an ambient group in and returns a pointer so conditions can be set
//-------------------------------------------------------------------------
CAmbientContext* CAmbientAnimationManager::AddAmbientContext( const char* szContext )
{
	CAmbientContext* pNewGroup = rage_new CAmbientContext();
	pNewGroup->SetContext(szContext);
	ms_aContexts.PushAndGrow(pNewGroup);
	return pNewGroup;
}


//-------------------------------------------------------------------------
// Adds a propset and returns a pointer
//-------------------------------------------------------------------------
CAmbientSetList* CAmbientAnimationManager::AddAmbientSetList( char* szName )
{
	CAmbientSetList* pSet = rage_new CAmbientSetList();
	pSet->SetHash(szName);
	ms_aAmbientGroupSets.PushAndGrow(pSet);
	return pSet;
}


//-------------------------------------------------------------------------
// Returns the set of ambients available for this movement group
//-------------------------------------------------------------------------
CAmbientContext* CAmbientAnimationManager::GetAmbientContext( const char* szContext )
{
	u32 iContextHash = atStringHash(szContext);
	return GetAmbientContext(iContextHash);
}

CAmbientContext* CAmbientAnimationManager::GetAmbientContext( u32 iContextHash )
{
	for( s32 i = 0; i < ms_aContexts.GetCount(); i++ )
	{
		if( ms_aContexts[i]->GetContext().GetHash() == iContextHash )
		{
			return ms_aContexts[i];
		}
	}

	return NULL;
}
*/
//-------------------------------------------------------------------------
//Adjusts the priority of this particular group for the ped:
// o Are they a mission ped?
// o How close are they?
//-------------------------------------------------------------------------
#define MAX_DISTANCE_PRIORITY	(5.0f)
#define DISTANCE_PRIORITY_SCALE (7.5f)
#define MISSION_PED_PRIORITY	(2.0f)

#if __DEV
fwClipSet* CAmbientAnimationManager::AddClipsToClipSet( const char* szClipSetName, const char* szDictName, char* pToken, char* seps )
{
	fwClipSet* pClipSet = NULL;

	// Find the dictionary entry
	strLocalIndex iClipDictIndex = fwAnimManager::FindSlotFromHashKey(atStringHash(szDictName));

	if (iClipDictIndex != -1)
	{
		s32 iNumClips = 0;
		s32 iNumClipsFound = 0;

		bool bClipSetCreated = false;

		// Stream in the clip dictionary
		CStreaming::RequestObject(iClipDictIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
		CStreaming::LoadAllRequestedObjects();

		char szClipName[64];

		// Read in each clip name and try to find it in the dictionary before adding it to the clip set
		while((pToken = strtok(NULL, seps)) != NULL && iNumClips < 8)
		{
			sprintf( szClipName, "%s", pToken );

			bool bAddedClip = false;

			const s32 iNumClipsInDict = fwAnimManager::CountAnimsInDict(iClipDictIndex.Get());
			for (s32 i=0; i<iNumClipsInDict; i++)
			{
				u32 iClipHash = fwAnimManager::GetAnimHashInDict(iClipDictIndex.Get(), i);
				if (iClipHash == atStringHash(szClipName))
				{
					if (!bClipSetCreated)
					{
						pClipSet = fwClipSetManager::CreateClipSet(fwMvClipSetId(szClipSetName), CLIP_SET_ID_INVALID, szDictName);
						taskAssert(pClipSet);
						bClipSetCreated = true;
					}
					bAddedClip = pClipSet->CreateClipItemWithProps(fwMvClipId(szClipName), APF_ISLOOPED | APF_ISBLENDAUTOREMOVE) != NULL;
					break;
				}
			}

			++iNumClipsFound;

			if (!bAddedClip)
			{
				aiAssertf(0, "Couldn't find clip %s in dictionary %s", szClipName, szDictName);
			}

			aiAssertf(iNumClipsFound < 8,  "Clip set %s nearly has too many clips, max 8", szClipSetName);
		}

		CStreaming::RemoveObject(iClipDictIndex, fwAnimManager::GetStreamingModuleId());
	}

	return pClipSet;
}

fwClipSet* CAmbientAnimationManager::AddFullDictionaryClipSet( const char* szDictName )
{
	const strLocalIndex iDictIndex = fwAnimManager::FindSlotFromHashKey(atStringHash(szDictName));

	fwClipSet* pClipSet = NULL;

	if (taskVerifyf(iDictIndex != -1, "Couldn't find dictionary %s in CAmbientAnimationManager::AddFullDictionaryClipSet", szDictName))
	{		
		// Stream in the clip dictionary
		CStreaming::RequestObject(iDictIndex, fwAnimManager::GetStreamingModuleId(), STRFLAG_FORCE_LOAD);
		CStreaming::LoadAllRequestedObjects();

		const s32 iNumClipsInDict = fwAnimManager::CountAnimsInDict(iDictIndex.Get());

		bool bDictionaryLoaded = false;
		while (!bDictionaryLoaded)
		{
			// Don't reorder the dictionaries when loading them in as it messes up the dictionary indices
			if (CDebugClipDictionary::StreamDictionary(iDictIndex.Get(), false))
			{
				// Create a new clipset with the same name as the dictionary
				pClipSet = fwClipSetManager::CreateClipSet(fwMvClipSetId(szDictName), CLIP_SET_ID_INVALID, szDictName);

				bDictionaryLoaded = true;

				CDebugClipDictionary& clipDict = CDebugClipDictionary::GetClipDictionary(iDictIndex.Get());

				// Go through the dictionary
				for (s32 i=0; i<iNumClipsInDict; i++)
				{							
					u32 iClipHash = fwAnimManager::GetAnimHashInDict(iDictIndex.Get(), i);

					// Ensure the clip exists before adding
					if (taskVerifyf(iClipHash == atStringHash(clipDict.GetClipName(i, false)), "Couldn't find clip %s in dictionary %s (check ordering)", clipDict.GetClipName(i, false), szDictName))
					{
						pClipSet->CreateClipItemWithProps(fwMvClipId(clipDict.GetClipName(i, false)), APF_ISLOOPED | APF_ISBLENDAUTOREMOVE);
					}
				}
			}
		}

		CStreaming::RemoveObject(iDictIndex, fwAnimManager::GetStreamingModuleId());
	}

	return pClipSet;
}
#endif

float CAmbientAnimationManager::AdjustPriorityForPed(const CDynamicEntity& dynamicEntity, float iPriority)
{
	const CPed*	pPed = dynamicEntity.GetIsTypePed() ? static_cast<const CPed*>(&dynamicEntity) : NULL;

	float fAdjustedPriority = iPriority;
	// o Are they a mission ped?
	if(pPed && pPed->PopTypeIsMission())
	{
		fAdjustedPriority *= MISSION_PED_PRIORITY;
	}

	// o How close are they?
	float fDistanceToPlayer = CGameWorld::FindLocalPlayerCoors().Dist(VEC3V_TO_VECTOR3(dynamicEntity.GetTransform().GetPosition()));
	fDistanceToPlayer /= DISTANCE_PRIORITY_SCALE;
	float iDistancePriority = MIN(fDistanceToPlayer, MAX_DISTANCE_PRIORITY );
	iDistancePriority = MAX_DISTANCE_PRIORITY - iDistancePriority;
	fAdjustedPriority *= iDistancePriority;

	return fAdjustedPriority;
}

#if !__FINAL

/*
//-------------------------------------------------------------------------
// Verifies the ambient dictionaries
//-------------------------------------------------------------------------
void CAmbientAnimationManager::TestAllAmbientDictionaries()
{
m
 	ASSERT_ONLY(bool bFoundErrors = false;)

	for( s32 i = 0; i < ms_aAmbientGroups.GetCount(); i++ )
	{
		CAmbientSet* pAmbientSet = ms_aAmbientGroups[i];
		if( pAmbientSet->m_clipSet.GetHash() != 0)
		{
			fwClipSet* pClipSet = fwClipSetManager::GetClipSet(pAmbientSet->m_clipSet);
			if (aiVerifyf(pClipSet, "Clipset (%s) doesn't exist", pAmbientSet->m_clipSet.GetCStr()))
			{
				s32 iClipDictIndex = fwAnimManager::FindSlotFromHashKey(pClipSet->GetClipDictionaryName().GetHash());

				if( iClipDictIndex == -1 )
				{
					ASSERT_ONLY(bFoundErrors = true;)
						Printf("Animation dictionary (%s) missing!\n", pAmbientSet->m_clipSet.GetCStr());
				}
			}
		}
	}

	aiAssertf(!bFoundErrors, "Found errors in ambient clips - See output for details\n");
}
*/
#endif // !__FINAL



//-------------------------------------------------------------------------
// Randomly chooses a suitable prop to spawn this ped with, also return the
// probability that the ped will spawn with this particular prop
//-------------------------------------------------------------------------
bool CAmbientAnimationManager::RandomlyChoosePropForSpawningPed( CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalAnimsGroup * pClips, u32& uiPropHashOut, int& animIndexWithinGroupOut, int conditionalAnimsChosen )
{	
	sPropSearchInfo searchInfo;
	memset(&searchInfo,0,sizeof(sPropSearchInfo));
	
	// TODO - NeilD
	// initialise a cached state to be passed through the system to save on condition checking
	//CCachedState cachedState;

	// Go through and get all clips that require a propSet
	AddClipsToPropSearch(conditionData,searchInfo,*pClips, conditionalAnimsChosen);

	// No clips with props found?
	if ( searchInfo.numSearchItems == 0 )
	{
		return false;
	}

	// Re-probablise depending on the number nearby with similar objects
	if( searchInfo.foundAnySimilarProps )
	{
		s32 iHighestCount = 0;
		s32 iLowestCount = 999999;
		for( s32 i = 0; i < searchInfo.numSearchItems; i++ )
		{
			sPropSearchItem & searchItem = searchInfo.searchItems[i];

			const CConditionalAnims * pAmbientSet = searchItem.pAmbientSet;

			if( pAmbientSet->GetPropSetHash() != 0 )
			{
				const CAmbientPropModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(pAmbientSet->GetPropSetHash());
				s32 iNumProps = pPropSet->GetNumModels();
				searchItem.iNumPedsNearbyWithProp = 99999;
				for( s32 j = 0; j < iNumProps; j++ )
				{
					searchItem.iNumPedsNearbyWithProp = MIN(searchItem.iNumPedsNearbyWithProp, searchItem.iPropSetsUses[j]);
				}
				iHighestCount = MAX( iHighestCount, searchItem.iNumPedsNearbyWithProp );
				iLowestCount = MIN( iLowestCount, searchItem.iNumPedsNearbyWithProp );
			}
		}
		for( s32 i = 0; i < searchInfo.numSearchItems; i++ )
		{
			sPropSearchItem & searchItem = searchInfo.searchItems[i];
			if( searchItem.iNumPedsNearbyWithProp > iLowestCount )
			{
				searchInfo.totalProbability -= searchItem.fProbabilities;
				searchItem.fProbabilities = 0.0f;
			}
		}
	}

	// Pick a random group
	const float fRandom = fwRandom::GetRandomNumberInRange(0.0f, searchInfo.totalProbability);

	float fProbSum = 0.0f;
	for( u32 i = 0; i < searchInfo.numSearchItems; i++ )
	{
		sPropSearchItem & searchItem = searchInfo.searchItems[i];

		fProbSum += searchItem.fProbabilities;
		if( fRandom < fProbSum || ( i == ( searchInfo.numSearchItems - 1 ) ) )
		{
			const CConditionalAnims* pAmbientSet = searchItem.pAmbientSet;
			if( pAmbientSet->GetPropSetHash() != 0 )
			{
				const CAmbientPropModelSet* pPropSet = CAmbientAnimationManager::GetPropSetFromHash(pAmbientSet->GetPropSetHash());
#if !__FINAL
                if (aiVerifyf(pPropSet, "Unable to Get PropSet {%d}%s", pAmbientSet->GetPropSetHash(), pAmbientSet->GetPropSetName()))
#else
                if (pPropSet)
#endif
                {
				    float fTotalProbability = 0.0f;
				    s32 iNumProps = pPropSet->GetNumModels();
				    float fProbabilities[MAX_SIZE_PROP_SET];
				    Assert(iNumProps<=MAX_SIZE_PROP_SET);
				    for( s32 k = 0; k < iNumProps; k++ )
				    {
					    if( searchItem.iPropSetsUses[k] == searchItem.iNumPedsNearbyWithProp )
					    {
						    fTotalProbability += pPropSet->GetModelProbability(k);
					    }
					    fProbabilities[k] = fTotalProbability;
				    }
				    float fRandom = fwRandom::GetRandomNumberInRange(0.0f, fTotalProbability);
				    for( s32 k = 0; k < iNumProps; k++ )
				    {
					    if( ( fRandom < fProbabilities[k] ) || ( k == ( iNumProps - 1 ) ) )
					    {
						    uiPropHashOut = pPropSet->GetModelHash(k);
						    if (aiVerifyf(CModelInfo::GetBaseModelInfoFromHashKey(uiPropHashOut, NULL), "Model [%s] doesn't exist", pPropSet->GetModelName(k)))
							{
								animIndexWithinGroupOut = searchItem.iIndex;
								return true;
							}
					    }
				    }
                }

			}
			return false;
		}
	}
	return false;
}

void CAmbientAnimationManager::AddClipsToPropSearch( CScenarioCondition::sScenarioConditionData& conditionData, sPropSearchInfo & searchInfo, const CConditionalAnimsGroup & ConditionalAnims, int conditionalAnimsChosen )
{
	for( s32 i = 0; i < ConditionalAnims.GetNumAnims(); i++ )
	{
		if(conditionalAnimsChosen >= 0 && conditionalAnimsChosen != i)
		{
			continue;
		}

		//float fPriority = 0;
		const CConditionalAnims * pAmbientSet = ConditionalAnims.GetAnims(i);

		const float prob = pAmbientSet->GetConditionalProbability(conditionData);

		// only happens if we turn off the probability in data during development.
		if (prob == 0.0f)
		{
			continue;
		}

		s32 failedCondition = -1;
		conditionData.iScenarioFlags |= SF_IgnoreHasPropCheck;
		if (pAmbientSet->GetPropSetHash()!=0 && pAmbientSet->CheckConditions( conditionData, &failedCondition ) && searchInfo.numSearchItems < NELEM(searchInfo.searchItems) )
		{
			sPropSearchItem & searchItem = searchInfo.searchItems[searchInfo.numSearchItems];

			searchItem.pAmbientSet				= pAmbientSet;
			searchItem.fProbabilities			= prob;
			searchItem.iNumPedsNearbyWithProp	= 0;
			searchItem.iIndex					= i;

			searchInfo.totalProbability += searchItem.fProbabilities;

			++searchInfo.numSearchItems;
		}
	}

	// No clips with props found?
	if( searchInfo.numSearchItems == 0 )
	{
		return;
	}

	// Loop through all nearby peds and dummies and count the number with similar objects

	const float MAX_RANGE_TO_CHECK_FOR_IDENTICAL_PROPS = 10.0f;
	Vec3V p = conditionData.pPed->GetTransform().GetPosition();
	CEntityIterator entityIterator( IteratePeds, NULL, &p, MAX_RANGE_TO_CHECK_FOR_IDENTICAL_PROPS );
	CPed* pDynamicEntity = static_cast<CPed*>( entityIterator.GetNext() );

	searchInfo.foundAnySimilarProps = false;
	while( pDynamicEntity )
	{
		Assert(pDynamicEntity->GetIsTypePed());
		if( pDynamicEntity != conditionData.pPed)
		{
			u32 uiPedsProp = 0;
			if( pDynamicEntity->GetHeldObject(*pDynamicEntity) )
			{
				uiPedsProp = pDynamicEntity->GetHeldObject(*pDynamicEntity)->GetBaseModelInfo()->GetHashKey();
			}
			else
			{
				strLocalIndex iIndex = strLocalIndex(pDynamicEntity->GetLoadingObjectIndex( *pDynamicEntity ));
				if( iIndex != fwModelId::MI_INVALID )
				{

					uiPedsProp = CModelInfo::GetBaseModelInfo(fwModelId(iIndex))->GetHashKey();;
				}
			}

			for( s32 i = 0; i < searchInfo.numSearchItems; i++ )
			{
				const CConditionalAnims * pAmbientSet = searchInfo.searchItems[i].pAmbientSet;
				aiAssert(pAmbientSet);
				
				const CAmbientPropModelSet * pPropSet = CAmbientAnimationManager::GetPropSetFromHash(pAmbientSet->GetPropSetHash());
				if ( pPropSet )
				{
					s32 iNumProps = pPropSet->GetNumModels();
					for( s32 j = 0; j < iNumProps; j++ )
					{
						if( uiPedsProp == pPropSet->GetModelHash(j) )
						{
							++searchInfo.searchItems[i].iPropSetsUses[j];
							searchInfo.foundAnySimilarProps = true;
						}
					}
				}
			}
		}
		pDynamicEntity = static_cast<CPed*>( entityIterator.GetNext() );
	}
}

void CAmbientAnimationManager::FillSearchInfoForConditionalAnimsGroup(CScenarioCondition::sScenarioConditionData& conditionData, const CConditionalAnimsGroup* pConditionalAnimsGroup, CConditionalAnims::eAnimType clipType, sSearchInfo& searchInfoOut)
{
	aiAssert(pConditionalAnimsGroup);

	searchInfoOut.totalProbability = 0.0f;
	searchInfoOut.numSearchItems = 0;

	// The pConditionalAnimsGroup is a set of all anims for a scenario or situation, e.g. PROP_HUMAN_SEAT_BENCH group, including all male/female variations
	for(int j = 0; j < pConditionalAnimsGroup->GetNumAnims(); j++)    
	{
		// The specific animation set, e.g. PROP_HUMAN_SEAT_BENCH_MALE_ELBOWS_ON_KNEES, inluding enters, exits, variations, etc.
		const CConditionalAnims * pClips = pConditionalAnimsGroup->GetAnims(j); 
		
		if (pClips->ShouldNotPickOnEnter() && ((clipType == CConditionalAnims::AT_ENTER) || conditionData.m_bNeedsValidEnterClip))
		{
			continue;
		}
		// Check the conditions on the animation set.
		if(pClips->CheckConditions(conditionData,NULL))
		{
			// The specified clips list must have at least one valid clip set for this CCondtionalClips 
			// to be added to the list of possibles. 

			// The ConditionalClipSetArray are the animations by type (ENTER, EXIT, VARIATION, etc) within the clip set.
			const CConditionalAnims::ConditionalClipSetArray * pConditionalAnimsArray = pClips->GetClipSetArray(clipType);

			// Check that the ClipSetArray exists (e.g., it has ENTERS) and that the conditions on it pass
			if (pConditionalAnimsArray && pClips->CheckClipSet(*pConditionalAnimsArray,conditionData))
			{
				// Make sure we never pick anything with the probability set to exactly zero.
				const float prob = pClips->GetConditionalProbability(conditionData);
				if(prob > 0.0f)	
				{
					sAmbientManagerSearchResult & searchItemToFill = searchInfoOut.searchItems[searchInfoOut.numSearchItems];

					searchItemToFill.pAmbientSet = pClips;
					searchItemToFill.iIndex = j;
					searchItemToFill.fProbabilities = prob;

					searchInfoOut.totalProbability += prob;
					searchInfoOut.numSearchItems++;
				}
			}
		}
	}
}

//-------------------------------------------------------------------------
// Randomly chooses one of CCondtitionalClips for the ped
//-------------------------------------------------------------------------
bool CAmbientAnimationManager::ChooseConditionalAnimations( CScenarioCondition::sScenarioConditionData& conditionData, s32& iChosenPriority, const CConditionalAnimsGroup * pConditionalAnimsGroup, s32 & ConditionalAnimsChosen, CConditionalAnims::eAnimType clipType )
{
	sSearchInfo	searchInfo;

	iChosenPriority = 0;

	// NeilD - Todo
	// initialise a cached state to be passed through the system to save on condition checking
	//	CCachedState cachedState;

	if( pConditionalAnimsGroup == NULL || pConditionalAnimsGroup->GetNumAnims() == 0 ) 
	{
		return false;
	}

	FillSearchInfoForConditionalAnimsGroup(conditionData, pConditionalAnimsGroup, clipType, searchInfo);

	// Pick a random group
	const float fRandom = fwRandom::GetRandomNumberInRange(0.0f, searchInfo.totalProbability);

	float fProbSum = 0.0f;
	for( u32 i = 0; i < searchInfo.numSearchItems; i++ )
	{
		fProbSum += searchInfo.searchItems[i].fProbabilities;
		if( fRandom < fProbSum || ( i == ( searchInfo.numSearchItems - 1 ) ) )
		{
			// this is needed later by whatever task is using it, to get hold of the clip sets
			// Could just return a pointer to the CConditionalAnims instead?
			ConditionalAnimsChosen = searchInfo.searchItems[i].iIndex;
			return true;
		}
	}

	return false;
}

//-------------------------------------------------------------------------
// Randomly chooses one of CCondtitionalClips for the ped
//-------------------------------------------------------------------------
bool CAmbientAnimationManager::ChooseConditionalAnimationsConsiderNearbyPeds( CScenarioCondition::sScenarioConditionData& conditionData, s32& iChosenPriority, const CConditionalAnimsGroup * pConditionalAnimsGroup, s32 & iConditionalAnimsChosenOut, CConditionalAnims::eAnimType clipType )
{
	sSearchInfo	searchInfo;

	iChosenPriority = 0;

	if( pConditionalAnimsGroup == NULL || pConditionalAnimsGroup->GetNumAnims() == 0 ) 
	{
		return false;
	}

	aiAssertf(!IsZeroAll(conditionData.vAmbientEventPosition), "Zero position in ChooseConditionalAnimationConsiderNearbyPeds, ped = %p", conditionData.pPed);

	FillSearchInfoForConditionalAnimsGroup(conditionData, pConditionalAnimsGroup, clipType, searchInfo);

	// Initialize the SearchInfo we'll pick from to this original set.
	sSearchInfo* pSearchInfoToUse = &searchInfo;
	sSearchInfo copiedSearchInfo;

	// If there's only one conditional anim we can play, don't bother trying to filter any out for nearby peds. [4/26/2013 mdawe]
	if (searchInfo.numSearchItems > 1)
	{
		// Try to pick an animation set from the group that's not being used by anybody too close.
		//  Start by making a copy of the animation sets we could pick from. We'll need the original list in case
		//  they're all being used by nearby peds.
		sysMemCpy(&copiedSearchInfo, pSearchInfoToUse, sizeof(sSearchInfo));

		// Next find any nearby peds.
		const CPed* pPed = conditionData.pPed;
		if (pPed)
		{
			static const int   sMaxPedsPerSearch  = 4;    // Up to 4 peds
			static const ScalarV svMaxDistance(2.5f);     // Up to 2.5 meters away

			// Get nearby peds using the spatial array
			const CSpatialArray& rSpatialArray = CPed::GetSpatialArray();
			CSpatialArrayNode* result[sMaxPedsPerSearch];


			u32 flag = CPed::kSpatialArrayTypeFlagUsingScenario;
			int iFound = rSpatialArray.FindInSphereOfType(conditionData.vAmbientEventPosition, svMaxDistance, &result[0], sMaxPedsPerSearch, flag, flag);

			//Iterate over the nearby peds, if any.
			for(int i = 0; i < iFound; ++i)
			{
				//Grab the ped.
				const CPed* pNearbyPed = CPed::GetPedFromSpatialArrayNode(result[i]); 
				if (pNearbyPed)
				{
					//Is this ped using a scenario?
					const CTaskUseScenario* pNearbyUseScenario = static_cast<const CTaskUseScenario*>(pNearbyPed->GetPedIntelligence()->FindTaskActiveByType(CTaskTypes::TASK_USE_SCENARIO));
					{
						if (pNearbyUseScenario)
						{
							// If the nearby ped's ConditionalAnimsGroup is the same that we're using...
							const CConditionalAnimsGroup* pNearbyConditionalAnimsGroup = pNearbyUseScenario->GetConditionalAnimsGroup();
							if (pNearbyConditionalAnimsGroup == pConditionalAnimsGroup)
							{
								// ...check our list to see if it includes the conditional anim set chosen by this other ped
								s32 iNearbyAnimIndex = pNearbyUseScenario->GetConditionalAnimIndex();
								for (s32 iSearchItemIndex = copiedSearchInfo.numSearchItems - 1; iSearchItemIndex >= 0; --iSearchItemIndex)
								{
									if ( copiedSearchInfo.searchItems[iSearchItemIndex].iIndex	== static_cast<u32>(iNearbyAnimIndex) )
									{
										copiedSearchInfo.RemoveFast(iSearchItemIndex);
									}
								}
							}
						}
					}
				}
			}
		}

		// If there's anything left in our pared-down list, we'll use that to pick randomly from.
		if (copiedSearchInfo.numSearchItems > 0)
		{
			pSearchInfoToUse = &copiedSearchInfo;
		}
	}

	// Pick a random group
	const float fRandom = fwRandom::GetRandomNumberInRange(0.0f, pSearchInfoToUse->totalProbability);

	float fProbSum = 0.0f;
	for( u32 i = 0; i < pSearchInfoToUse->numSearchItems; i++ )
	{
		fProbSum += pSearchInfoToUse->searchItems[i].fProbabilities;
		if( fRandom < fProbSum || ( i == ( pSearchInfoToUse->numSearchItems - 1 ) ) )
		{
			// this is needed later by whatever task is using it, to get hold of the clip sets
			// Could just return a pointer to the CConditionalAnims instead?
			iConditionalAnimsChosenOut = pSearchInfoToUse->searchItems[i].iIndex;
			return true;
		}
	}

	return false;
}


const CAmbientPropModelSet* CAmbientAnimationManager::GetPropSetFromHash(u32 hash)	
{
	s32 index = CAmbientModelSetManager::GetInstance().GetModelSetIndexFromHash(CAmbientModelSetManager::kPropModelSets, hash);
	if ( index>=0 )
	{
		return CAmbientModelSetManager::GetInstance().GetPropSet(index);
	}

    Assertf(false,"Unknown Prop Set {%d}\n", hash);
	return NULL;
}
