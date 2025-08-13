// File header
#include "AI/Ambient/AmbientModelSet.h"

// Rage headers
#include "ai/aichannel.h"
#include "fwmaths/random.h"

// Game headers
#include "ModelInfo/ModelInfo.h"

// Parser
#include "AmbientModelSet_parser.h"

AI_OPTIMISATIONS()

INSTANTIATE_RTTI_CLASS(CAmbientModelVariations,0x3975CD70)
INSTANTIATE_RTTI_CLASS(CAmbientPedModelVariations,0xDC4C43FA)
INSTANTIATE_RTTI_CLASS(CAmbientVehicleModelVariations,0xF6787C97)

////////////////////////////////////////////////////////////////////////////////
// CAmbientModel
////////////////////////////////////////////////////////////////////////////////

CAmbientModel::CAmbientModel()
		: m_Variations(NULL)
{
}

////////////////////////////////////////////////////////////////////////////////

CAmbientModel::CAmbientModel(const CAmbientModel& other)
		: m_Variations(NULL)
{
	*this = other;
}

////////////////////////////////////////////////////////////////////////////////

CAmbientModel::~CAmbientModel()
{
	delete m_Variations;
}

////////////////////////////////////////////////////////////////////////////////

const CAmbientModel& CAmbientModel::operator=(const CAmbientModel& other)
{
	if(&other != this)
	{
		if(m_Variations)
		{
			delete m_Variations;
			m_Variations = NULL;
		}
		if(other.m_Variations)
		{
			m_Variations = other.m_Variations->Clone();
		}
	}
	return *this;
}

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelSet
////////////////////////////////////////////////////////////////////////////////

bool CAmbientModelSet::GetContainsModel(u32 uModelHash, const CAmbientModelVariations** variationsOut) const
{
	float fProbability = 0.0f;
	int firstFoundIndex = -1;
	int numFound = 0;
	for(s32 i = 0; i < m_Models.GetCount(); i++)
	{
		if(m_Models[i].GetHash() == uModelHash)
		{
			if(variationsOut)
			{
				// Sum up the probability of the entries using this model.
				fProbability += m_Models[i].GetProbability();
				if(firstFoundIndex < 0)
				{
					firstFoundIndex = i;
				}
				numFound++;
			}
			else
			{
				// If the user is not interested in knowing about the variations,
				// we're done.
				return true;
			}
		}
	}

	// If we didn't find anything, return.
	if(!numFound)
	{
		if(variationsOut)
		{
			*variationsOut = NULL;
		}
		return false;
	}

	// Deal with the common case of a single matching entry.
	if(numFound == 1)
	{
		aiAssert(variationsOut);	// Shouldn't be possible to get here otherwise.
		*variationsOut = m_Models[firstFoundIndex].GetVariations();
		return true;
	}
	
	// In this case, there were multiple entries using this model, which could
	// potentially have different variations, so we pick one of them randomly.
	float fRandom = fwRandom::GetRandomNumberInRange(0.0f, fProbability);
	for(s32 i = 0; i < m_Models.GetCount(); i++)
	{
		if(m_Models[i].GetHash() != uModelHash)
		{
			continue;
		}
		if(fRandom <= m_Models[i].GetProbability())
		{
			if(variationsOut)
			{
				*variationsOut = m_Models[i].GetVariations();
			}
			return true;
		}

		fRandom -= m_Models[i].GetProbability();
	}

	aiAssertf(0, "Unexpectedly failed to find variations.");
	return false;
}

////////////////////////////////////////////////////////////////////////////////

u32 CAmbientModelSet::GetRandomModelHash(const CAmbientModelVariations** variationsOut, const CAmbientModelSetFilter* filter, const char* debugContext) const
{
	return GetRandomModelHash(fwRandom::GetRandomNumberInRange(0.0f, 1.0f), variationsOut, filter, debugContext);
}

////////////////////////////////////////////////////////////////////////////////

u32 CAmbientModelSet::GetRandomModelHash(float randZeroToOne,const CAmbientModelVariations** variationsOut,
		const CAmbientModelSetFilter* filter, const char* ASSERT_ONLY(debugContext)) const
{
	f32 fProbability = 0.f;
	for(s32 i = 0; i < m_Models.GetCount(); i++)
	{
		if(filter && !filter->Match(m_Models[i].GetHash()))
		{
			continue;
		}
		fProbability += m_Models[i].GetProbability();
	}

	f32 fRandom = randZeroToOne*fProbability;

	for(s32 i = 0; i < m_Models.GetCount(); i++)
	{
		// Note: filter function is actually called twice now, for simplicity.
		if(filter && !filter->Match(m_Models[i].GetHash()))
		{
			continue;
		}

		if(fRandom <= m_Models[i].GetProbability())
		{
			if(variationsOut)
			{
				*variationsOut = m_Models[i].GetVariations();
			}
			return m_Models[i].GetHash();
		}

		fRandom -= m_Models[i].GetProbability();
	}

#if __ASSERT
	const bool triggerAssert = ((!filter) || (filter && filter->FailureShouldTriggerAsserts()));
	if (triggerAssert)
	{
		const char* setName = GetName();
		char filterInfo[256];
#if __BANK
		if(filter)
		{
			filter->GetFilterDebugInfoString(filterInfo, sizeof(filterInfo));
		}
		else
		{
			formatf(filterInfo, "None");
		}
#else
		formatf(filterInfo, "?");
#endif
		aiAssertf(0, "No valid model found - model set: %s context: %s filter: %s", setName, debugContext ? debugContext : "?", filterInfo);
	}
#endif	// __ASSERT

	if(variationsOut)
	{
		*variationsOut = NULL;
	}
	return 0;
}

////////////////////////////////////////////////////////////////////////////////

strLocalIndex CAmbientModelSet::GetRandomModelIndex(const CAmbientModelVariations** variationsOut, const CAmbientModelSetFilter* filter, const char* debugContext) const
{
	u32 hash = GetRandomModelHash(variationsOut, filter, debugContext);
	if(hash != 0)
	{
		fwModelId uModelId;
		CModelInfo::GetBaseModelInfoFromHashKey(hash, &uModelId);
		return strLocalIndex(uModelId.GetModelIndex());
	}
	return strLocalIndex(fwModelId::MI_INVALID);
}

////////////////////////////////////////////////////////////////////////////////

bool CAmbientModelSet::HasAnyValidModel(const CAmbientModelSetFilter* filter) const
{
	const int numModels = m_Models.GetCount();
	if(!filter)
	{
		return numModels > 0;
	}

	for(int i = 0; i < numModels; i++)
	{
		if(filter->Match(m_Models[i].GetHash()))
		{
			return true;
		}
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////

strLocalIndex CAmbientModelSet::GetModelIndex( u32 iModel ) const
{
	fwModelId uModelId;
	CModelInfo::GetBaseModelInfoFromHashKey(m_Models[iModel].GetHash(), &uModelId);
	return strLocalIndex(uModelId.GetModelIndex());
}

////////////////////////////////////////////////////////////////////////////////

const CAmbientModelVariations* CAmbientModelSet::GetModelVariations(u32 indexInSet) const
{
	return m_Models[indexInSet].GetVariations();
}

////////////////////////////////////////////////////////////////////////////////
// CAmbientModelSets
////////////////////////////////////////////////////////////////////////////////

void CAmbientModelSets::ParserPostLoad()
{
	// In case this is re-loaded, clear out the cache map
	if( m_ModelSetIndexCache.GetCount() > 0 )
	{
		m_ModelSetIndexCache.Reset();
	}

	// Reserve the map memory that will be filled in
	m_ModelSetIndexCache.Reserve(m_ModelSets.GetCount());

	// Fill out the hash to index cache map
	for(int i=0; i < m_ModelSets.GetCount(); i++)
	{
		const u32 hashKey = m_ModelSets[i]->GetHash();
		const int indexToStore = i;
		m_ModelSetIndexCache.Insert(hashKey, indexToStore);
	}

	// Prep the map for use
	// NOTE: It is safe to call this if nothing was inserted
	m_ModelSetIndexCache.FinishInsertion();
}

void CAmbientModelSets::Append(CAmbientModelSets &sets)
{
	for(int i=0; i < sets.m_ModelSets.GetCount(); i++)
	{
		if(AssertVerify(m_ModelSets.Find((sets.m_ModelSets[i])) == -1))
		{
			m_ModelSets.PushAndGrow(sets.m_ModelSets[i]);
			sets.m_ModelSets[i]->ReleaseVariations();
		}
	}

	ParserPostLoad();
}

void CAmbientModelSets::Remove(const CAmbientModelSets &sets)
{
	for(int i=0; i < sets.m_ModelSets.GetCount(); i++)
	{
		int index = m_ModelSets.Find(sets.m_ModelSets[i]);

		if(AssertVerify(index != -1))
		{
			m_ModelSets[index]->Clear();
			m_ModelSets.DeleteFast(index);
		}
	}

	ParserPostLoad();
}

////////////////////////////////////////////////////////////////////////////////
// CAmbientPedModelVariations
////////////////////////////////////////////////////////////////////////////////

CAmbientModelVariations* CAmbientPedModelVariations::Clone() const
{
	CAmbientPedModelVariations* clone = rage_new CAmbientPedModelVariations;
	clone->m_CompRestrictions = m_CompRestrictions;
	clone->m_PropRestrictions = m_PropRestrictions;
	clone->m_LoadOut = m_LoadOut;
	return clone;
}

////////////////////////////////////////////////////////////////////////////////
// CAmbientVehicleModelVariations
////////////////////////////////////////////////////////////////////////////////

CAmbientVehicleModelVariations::CAmbientVehicleModelVariations()
		: m_BodyColour1(-1)
		, m_BodyColour2(-1)
		, m_BodyColour3(-1)
		, m_BodyColour4(-1)
		, m_BodyColour5(-1)
		, m_BodyColour6(-1)
		, m_WindowTint(-1)
		, m_ColourCombination(-1)
		, m_Livery(-1)
		, m_Livery2(-1)
		, m_ModKit(-1)
		, m_Extra1(Either)
		, m_Extra2(Either)
		, m_Extra3(Either)
		, m_Extra4(Either)
		, m_Extra5(Either)
		, m_Extra6(Either)
		, m_Extra7(Either)
		, m_Extra8(Either)
		, m_Extra9(Either)
		, m_Extra10(Either)
{
}

////////////////////////////////////////////////////////////////////////////////

CAmbientModelVariations* CAmbientVehicleModelVariations::Clone() const
{
	CAmbientVehicleModelVariations* clone = rage_new CAmbientVehicleModelVariations;
	clone->m_BodyColour1 = m_BodyColour1;
	clone->m_BodyColour2 = m_BodyColour2;
	clone->m_BodyColour3 = m_BodyColour3;
	clone->m_BodyColour4 = m_BodyColour4;
	clone->m_BodyColour5 = m_BodyColour5;
	clone->m_BodyColour6 = m_BodyColour6;
	clone->m_WindowTint = m_WindowTint;
	clone->m_ColourCombination = m_ColourCombination;
	clone->m_Livery = m_Livery;
	clone->m_Livery2= m_Livery2;
	clone->m_ModKit = m_ModKit;
	clone->m_Mods = m_Mods;
	clone->m_Extra1 = m_Extra1;
	clone->m_Extra2 = m_Extra2;
	clone->m_Extra3 = m_Extra3;
	clone->m_Extra4 = m_Extra4;
	clone->m_Extra5 = m_Extra5;
	clone->m_Extra6 = m_Extra6;
	clone->m_Extra7 = m_Extra7;
	clone->m_Extra8 = m_Extra8;
	clone->m_Extra9 = m_Extra9;
	clone->m_Extra10 = m_Extra10;
	return clone;
}

////////////////////////////////////////////////////////////////////////////////

CAmbientVehicleModelVariations::UseExtra CAmbientVehicleModelVariations::GetExtra(int index) const
{
	// This function isn't nice, but having these as separate variables rather than
	// an array is probably the most user friendly, in terms of how it shows up in RAG
	// and in the data files.

	CAmbientVehicleModelVariations::UseExtra e = Either;

	switch(index)
	{
		case 0:
			e = m_Extra1;
			break;
		case 1:
			e = m_Extra2;
			break;
		case 2:
			e = m_Extra3;
			break;
		case 3:
			e = m_Extra4;
			break;
		case 4:
			e = m_Extra5;
			break;
		case 5:
			e = m_Extra6;
			break;
		case 6:
			e = m_Extra7;
			break;
		case 7:
			e = m_Extra8;
			break;
		case 8:
			e = m_Extra9;
			break;
		case 9:
			e = m_Extra10;
			break;
		default:
			aiAssertf(0, "Unknown extra index %d.", index);
	}
	return e;
}

////////////////////////////////////////////////////////////////////////////////
