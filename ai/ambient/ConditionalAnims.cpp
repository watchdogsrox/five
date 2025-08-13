// File header
#include "AI/Ambient/ConditionalAnims.h"
#include "AI/Ambient/ConditionalAnims_Parser.h"

// Rage headers
#include "ai/aichannel.h"
#include "fwanimation/clipsets.h"
#include "scene/dynamicentity.h"
#include "task/Scenario/info/scenarioinfoconditions.h"

// Framework headers
#include "fwmaths/random.h"

// Game headers
#include "peds/Ped.h"
#include "Streaming/streaming.h"

AI_OPTIMISATIONS()

bool CConditionalHelpers::CheckConditionsHelper( const ConditionsArray & conditions , const CScenarioCondition::sScenarioConditionData& conditionData, s32 * firstFailureCondition )
{
	for ( u32 i=0; i<conditions.size(); i++)
	{
#if !__FINAL
		if (!conditions[i])
		{
			Quitf("Either conditionalanims.meta/scenarios.meta is newer than the code being run, or parser dependencies didn't build correctly. Please check these.");
		}
#endif

		bool checkResult = conditions[i]->Check(conditionData);
		if ( !checkResult )
		{
			if ( firstFailureCondition )
			{
				*firstFailureCondition = i;
			}

			return false;
		}
	}

	// All conditions have returned true
	return true;
}


int CConditionalHelpers::GetImmediateConditionsHelper(const ConditionsArray& conditions, const CScenarioCondition** pArrayOut, int maxNumber)
{
	int numFound = 0;

	if(numFound >= maxNumber)
	{
		// The array is already full, no use looking for more.
		return numFound;
	}

	const int numConditions = conditions.GetCount();
	for(int i = 0; i < numConditions; i++)
	{
		// CScenarioConditionSpeed needs to be checked on every frame for the base animations
		// in CTaskAmbientClips, in case the speed changes while the task is running.
		// See B* 212191: "CODE_HUMAN_WANDER scenarios need 'static' idles".
		// Note: a cleaner solution may be to add a virtual function in CScenarioCondition,
		// and make it return true for the classes that need it.
		if(conditions[i]->GetIsClass<CScenarioConditionSpeed>() || conditions[i]->GetIsClass<CScenarioConditionFullyInIdle>() || conditions[i]->GetIsClass<CScenarioPhoneConversationInProgress>() )
		{
			pArrayOut[numFound++] = conditions[i];
			if(numFound >= maxNumber)
			{
				// Filled up, nothing more to do.
				break;
			}
		}
	}
	return numFound;
}

CConditionalClipSet::~CConditionalClipSet()
{
	// Conditions that we use these anims
	for(int i = 0; i < m_Conditions.GetCount(); i++)
	{
		if (m_Conditions[i])
		{
			delete m_Conditions[i];
			m_Conditions[i] = NULL;
		}
	}
	m_Conditions.ResetCount();
}

bool CConditionalClipSet::CheckConditions( const CScenarioCondition::sScenarioConditionData& conditionData ) const
{
	if(GetIsSynchronized())
	{
		if(!conditionData.m_AllowSynchronized)
		{
			return false;
		}
	}
	else
	{
		if(!conditionData.m_AllowNotSynchronized)
		{
			return false;
		}
	}

	return CConditionalHelpers::CheckConditionsHelper( m_Conditions, conditionData, NULL );
}

int CConditionalClipSet::GetImmediateConditions(const CScenarioCondition** pArrayOut, int maxNumber) const
{
	return CConditionalHelpers::GetImmediateConditionsHelper(m_Conditions, pArrayOut, maxNumber);
}

CConditionalAnims::CConditionalAnims()
		: m_SpecialCondition(NULL)
		, m_TransitionInfo(NULL)
{
}

void ClearConditionalClipSetArray(CConditionalAnims::ConditionalClipSetArray& _InOutArray)
{
	for(int i = 0; i < _InOutArray.GetCount(); i++)
	{
		if (_InOutArray[i])
		{
			delete _InOutArray[i];
			_InOutArray[i] = NULL;
		}
	}
	_InOutArray.ResetCount();
}

CConditionalAnims::~CConditionalAnims()
{
	if (m_SpecialCondition)
	{
		delete m_SpecialCondition;
		m_SpecialCondition = NULL;
	}

	if (m_TransitionInfo)
	{
		delete m_TransitionInfo;
		m_TransitionInfo = NULL;
	}

	// VFX used by this conditional clip set
	for(int i = 0; i < m_VFXData.GetCount(); i++)
	{
		if (m_VFXData[i])
		{
			delete m_VFXData[i];
			m_VFXData[i] = NULL;
		}
	}
	m_VFXData.ResetCount();

	// Conditions that we use these anims
	for(int i = 0; i < m_Conditions.GetCount(); i++)
	{
		if (m_Conditions[i])
		{
			delete m_Conditions[i];
			m_Conditions[i] = NULL;
		}
	}
	m_Conditions.ResetCount();

	// This will play under the variation anims
	ClearConditionalClipSetArray(m_BaseAnims);

	// Intro animations - to transition into the scenario
	ClearConditionalClipSetArray(m_Enters);

	// Outro animations - to transition out of the scenario
	ClearConditionalClipSetArray(m_Exits);

	// Variations - idle variations
	ClearConditionalClipSetArray(m_Variations);

	// Reactions - idle variations
	ClearConditionalClipSetArray(m_Reactions);

	// Basic idle for panic
	ClearConditionalClipSetArray(m_PanicBaseAnims);

	// Intro into the panic base
	ClearConditionalClipSetArray(m_PanicIntros);

	// Outro from the panic base (back to just a normal idle)
	ClearConditionalClipSetArray(m_PanicOutros);

	// Variations - panic idle variations
	ClearConditionalClipSetArray(m_PanicVariations);

	// Exit directly into a flee 
	ClearConditionalClipSetArray(m_PanicExits);
}


u32 CConditionalAnims::GetPropSetHash() const
{
	return m_PropSet.GetHash();
}


float CConditionalAnims::GetConditionalProbability(const CScenarioCondition::sScenarioConditionData& conditionData) const
{
	if(m_SpecialCondition && m_SpecialCondition->Check(conditionData))
	{
		return m_SpecialConditionProbability;
	}
	return m_Probability;
}


bool CConditionalAnims::CheckConditions( const CScenarioCondition::sScenarioConditionData& conditionData, s32 * firstFailureCondition ) const
{
	return CConditionalHelpers::CheckConditionsHelper( m_Conditions, conditionData, firstFailureCondition );
}

// Check only those conditions that can be checked before a ped is actually created - i.e. ones that deal with a ped model index not an actual ped.
// Return false if any of the conditions fail, true otherwise.
bool CConditionalAnims::CheckPopulationConditions( const CScenarioCondition::sScenarioConditionData& conditionData ) const
{
	for(int i=0; i < m_Conditions.GetCount(); i++)
	{
		CScenarioCondition* pCondition = m_Conditions[i];
		if (pCondition->GetIsClass<CScenarioConditionPopulation>() && !pCondition->Check(conditionData))
		{
			return false;
		}
	}
	return true;
}

bool CConditionalAnims::CheckPropConditions( const CScenarioCondition::sScenarioConditionData& conditionData ) const
{
	for(int i=0; i < m_Conditions.GetCount(); i++)
	{
		CScenarioCondition* pCondition = m_Conditions[i];
		if ( (pCondition->GetIsClass<CScenarioConditionHasProp>() || pCondition->GetIsClass<CScenarioConditionHasNoProp>())
			 && !pCondition->Check(conditionData))
		{
			return false;
		}
	}
	return true;
}

int CConditionalAnims::GetImmediateConditions(u32 clipSetHash, const CScenarioCondition** pArrayOut, int maxNumber, eAnimType animType) const
{
	// First, check the conditions that apply to the whole CConditionalAnims object.
	int numFound = CConditionalHelpers::GetImmediateConditionsHelper(m_Conditions, pArrayOut, maxNumber);

	// Next, look for the clip that we are playing, and add the conditions from there.
	if(numFound < maxNumber)
	{
		const ConditionalClipSetArray* clipSets = GetClipSetArray(animType);
		if(clipSets)
		{
			const int numClipSets = clipSets->GetCount();
			for(int j = 0; j < numClipSets; j++)
			{
				const CConditionalClipSet* pClipSet = (*clipSets)[j];
				if(pClipSet->GetClipSetHash() == clipSetHash)
				{
					numFound += pClipSet->GetImmediateConditions(pArrayOut + numFound, maxNumber - numFound);
					break;
				}
			}
		}
	}

	return numFound;
}


// Check that at least one set in this list is usable
bool CConditionalAnims::CheckClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData ) const
{
	for(int i=0; i<anims.size(); i++)
	{
		if(anims[i]->CheckConditions(conditionData))
		{
			// Check that the animation dictionary for this exists?
			strLocalIndex iDictIndex = anims[i]->GetAnimDictIndex();
			aiAssertf(iDictIndex.Get()>=0,"Failed to find animation dictionary from set '%s'\n",anims[i]->GetClipSetName());
			if (iDictIndex.Get()>=0 && CStreaming::IsObjectInImage(iDictIndex, fwAnimManager::GetStreamingModuleId()))
			{
				// As long as at least one set in this list is OK then we're fine
				return true;
			}
		}
	}
	return false;
}


// Check all random sets and choose one that has satisfied its conditions
const CConditionalClipSet * CConditionalAnims::ChooseClipSet( const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData )
{
	#define		MAX_SETS 32
	u32			sets[MAX_SETS];
	
	s32			iNoValidSets = 0;

	if ( anims.size() == 0 )
	{
		return NULL;
	}

	for(int i=0; i<anims.size(); i++)
	{
		if(anims[i]->CheckConditions(conditionData) && iNoValidSets<MAX_SETS)
		{
			sets[iNoValidSets] = i;
			iNoValidSets++;
		}
	}

	if ( iNoValidSets == 0 )
	{
		return NULL;
	}

	// Pick a random group (note: these do not have individual probabilities at the moment, might be needed though?)
	u32 randomSet = sets[fwRandom::GetRandomNumberInRange(0, iNoValidSets)];
	return anims[randomSet];
}

#if __BANK
// Of the valid sets get the next one in line from the one we used already
const CConditionalClipSet * CConditionalAnims::ChooseNextClipSet(const fwMvClipSetId& lastClipSet, const ConditionalClipSetArray & anims, const CScenarioCondition::sScenarioConditionData& conditionData ) const
{
#define		MAX_SETS 32
	u32			sets[MAX_SETS];

	s32			iNoValidSets = 0;

	if ( anims.size() == 0 )
	{
		return NULL;
	}

	for(int i=0; i<anims.size(); i++)
	{
		if(anims[i]->CheckConditions(conditionData) && iNoValidSets<MAX_SETS)
		{
			sets[iNoValidSets] = i;
			iNoValidSets++;
		}
	}

	if ( iNoValidSets == 0 )
	{
		return NULL;
	}

	//To find the next clip we just need to search the valid array and find the next
	// clip past the one referenced to by iClipHash
	int found = 0; //assume using first
	for( s32 i = 0; i < iNoValidSets; i++ )
	{
		//Found the previously used clip ... 
		if (anims[sets[i]]->GetClipSetHash() == lastClipSet.GetHash())
		{
			if (i+1 < iNoValidSets)
			{
				//Use next
				found = i+1;
			}
			break;
		}
	}

	return anims[found];
}
#endif

#if !__FINAL

bool CConditionalAnims::HasAnimationData() const
{
	if (m_BaseAnims.size() || m_Enters.size() || m_Exits.size() || m_Variations.size() || m_Reactions.size())
	{
		return true;
	}

	return false;
}

bool CConditionalAnimsGroup::HasAnimationData() const
{
	for ( u32 i=0; i<m_ConditionalAnims.size(); i++)
	{
		if ( m_ConditionalAnims[i].HasAnimationData() )
		{
			return true;
		}
	}

	return false;
}

bool CConditionalAnimsGroup::HasProp() const
{
	for ( u32 index = 0; index < m_ConditionalAnims.size(); ++index)
	{
		if (m_ConditionalAnims[index].GetPropSetHash() != 0)
		{
			return true;
		}
	}

	return false;
}
#endif

const CConditionalClipSetVFX* CConditionalAnims::GetVFXByName(atHashWithStringNotFinal name) const
{
	for (int fx=0; fx < m_VFXData.GetCount(); fx++)
	{
		if (m_VFXData[fx]->m_Name == name)
		{
			return m_VFXData[fx];
		}
	}

	return NULL;
}

// Return true if there is at least one conditional anim set that qualifies.
bool CConditionalAnimsGroup::CheckForMatchingConditionalAnims(const CScenarioCondition::sScenarioConditionData& rConditionData) const
{
	int iNumberConditionalAnims = m_ConditionalAnims.GetCount();
	for(int i=0; i < iNumberConditionalAnims; i++)
	{
		const CConditionalAnims& pAnims = m_ConditionalAnims[i];
		if (pAnims.GetConditionalProbability(rConditionData) > 0.0f && pAnims.CheckConditions(rConditionData, NULL))
		{
			return true;
		}
	}

	// Return success if there are no conditional animations.
	return iNumberConditionalAnims == 0;
}

// Return true if there is at least one conditional anim that qualifies (but only check conditions that only require a model index)
bool CConditionalAnimsGroup::CheckForMatchingPopulationConditionalAnims(const CScenarioCondition::sScenarioConditionData& rConditionData) const
{
	int iNumberConditionalAnims = m_ConditionalAnims.GetCount();
	for(int i=0; i < iNumberConditionalAnims; i++)
	{
		const CConditionalAnims& pAnims = m_ConditionalAnims[i];
		if (pAnims.GetConditionalProbability(rConditionData) > 0.0f && pAnims.CheckPopulationConditions(rConditionData))
		{
			return true;
		}
	}
	
	// Return success if there are no conditional animations.
	return iNumberConditionalAnims == 0;
}
