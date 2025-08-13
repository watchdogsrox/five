// FILE :    PedMotivation.cpp
// PURPOSE : Used to the emotional motivation for a peds response to a given event or situation...
// CREATED : 15-09-2010


// class header
#include "Peds/PedIntelligence/PedMotivation.h"

// Framework headers
#include "ai/aichannel.h"
#include "fwmaths/random.h"

// Game headers
#include "Scene/DataFileMgr.h"

// Parser headers
#include "PedMotivation_parser.h"

// rage headers
#include "math/amath.h"
#include "parser/manager.h"


ANIM_OPTIMISATIONS()

dev_float CPedMotivation::sm_fFearThreshold = 0.3f;

CPedMotivation::CPedMotivation()
{
	m_aDefaultStates[HAPPY_STATE] = 0.5f; 
	m_aCurrentStates[HAPPY_STATE] = m_aDefaultStates[HAPPY_STATE];  

	m_aDefaultStates[FEAR_STATE] = 0.0f; 
	m_aCurrentStates[FEAR_STATE] = m_aDefaultStates[FEAR_STATE]; 
				
	m_aDefaultStates[ANGRY_STATE] = 0.0f; 
	m_aCurrentStates[ANGRY_STATE] = m_aDefaultStates[ANGRY_STATE]; 
			
	m_aDefaultStates[HUNGRY_STATE] = 0.0f; 
	m_aCurrentStates[HUNGRY_STATE] = m_aDefaultStates[HUNGRY_STATE]; 
			
	m_aDefaultStates[TIRED_STATE] = 0.0f; 
	m_aCurrentStates[TIRED_STATE] = m_aDefaultStates[TIRED_STATE]; 

	for (u32 index = 0; index < NUMBER_OF_MOTIVATION_STATES; index++)
	{
		m_aMotivationRateModifiers[index] = 1.0f; 
	}
}

CPedMotivation::~CPedMotivation()
{

}

void CPedMotivation::SetMotivationProfile(u32 NameHash)
{
	const CPedMotivationInfo* PedVarInfo = CPedMotivationInfoManager::GetInfo(NameHash); 

	if(PedVarInfo)	
	{
		for(u32 i=0; i < NUMBER_OF_MOTIVATION_STATES; i++ )
		{
			switch(i)
			{
			case HAPPY_STATE:
				{
					m_aDefaultStates[HAPPY_STATE] = PedVarInfo->GetDefaultHappyValue(); 
					m_aCurrentStates[HAPPY_STATE] = m_aDefaultStates[HAPPY_STATE]; 
					m_aMotivationRateModifiers[HAPPY_STATE] = RandomiseMotivationRateModifiers(PedVarInfo->GetHappyRate());
				}
				break;

			case FEAR_STATE:
				{
					m_aDefaultStates[FEAR_STATE] = PedVarInfo->GetDefaultFearValue(); 
					m_aCurrentStates[FEAR_STATE] = m_aDefaultStates[FEAR_STATE]; 
					m_aMotivationRateModifiers[FEAR_STATE] = RandomiseMotivationRateModifiers(PedVarInfo->GetFearRate());
				}
				break;

			case ANGRY_STATE:
				{
					m_aDefaultStates[ANGRY_STATE] = PedVarInfo->GetDefaultAngerValue(); 
					m_aCurrentStates[ANGRY_STATE] = m_aDefaultStates[ANGRY_STATE]; 
					m_aMotivationRateModifiers[ANGRY_STATE] = RandomiseMotivationRateModifiers(PedVarInfo->GetAngerRate());
				}
				break;

			case HUNGRY_STATE:
				{
					m_aDefaultStates[HUNGRY_STATE] = PedVarInfo->GetDefaultHungerValue(); 
					m_aCurrentStates[HUNGRY_STATE] = m_aDefaultStates[HUNGRY_STATE]; 
					m_aMotivationRateModifiers[HUNGRY_STATE] = RandomiseMotivationRateModifiers(PedVarInfo->GetHungerRate());
				}
				break;


			case TIRED_STATE:
				{
					m_aDefaultStates[TIRED_STATE] = PedVarInfo->GetDefaultTiredValue(); 
					m_aCurrentStates[TIRED_STATE] = m_aDefaultStates[TIRED_STATE]; 
					m_aMotivationRateModifiers[TIRED_STATE] = RandomiseMotivationRateModifiers(PedVarInfo->GetTiredRate());
				}
				break;

			}
		}	
	}
}

float CPedMotivation::RandomiseMotivationRateModifiers(float fMotivationRate)
{
	Assertf(fMotivationRate,"Motivation rate must be greater than 0 defaulting to 1.0f"); 
	
	float NewMotivationRate = 1.0f; 

	if(fMotivationRate == 0.0f)
	{
		fMotivationRate = 1.0f; 
	}
	
	if (fMotivationRate > 1.0f)
	{
		NewMotivationRate = fwRandom::GetRandomNumberInRange(DEFAULT_MOTIVATION_RATE, fMotivationRate); 
	}

	return NewMotivationRate; 
}

void CPedMotivation::IncreasePedMotivation(PedMotivationState MotivationState, const float fResponse)
{
	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		m_aCurrentStates[MotivationState] = m_aCurrentStates[MotivationState]+fResponse * m_aMotivationRateModifiers[MotivationState]; 
		m_aCurrentStates[MotivationState] = Clamp(m_aCurrentStates[MotivationState],MIN_MOTIVATION_STATE_VALUE,MAX_MOTIVATION_STATE_VALUE);
	}
}

void CPedMotivation::DecreasePedMotivation(PedMotivationState MotivationState, const float fResponse)
{
	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		m_aCurrentStates[MotivationState] = m_aCurrentStates[MotivationState] - fResponse * m_aMotivationRateModifiers[MotivationState]; 
		m_aCurrentStates[MotivationState] = Clamp(m_aCurrentStates[MotivationState],MIN_MOTIVATION_STATE_VALUE,MAX_MOTIVATION_STATE_VALUE);
	}
}

void CPedMotivation::SetMotivationToMax(PedMotivationState MotivationState)
{
	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		m_aCurrentStates[MotivationState] = MAX_MOTIVATION_STATE_VALUE; 
	}
}

void CPedMotivation::SetMotivationToMin(PedMotivationState MotivationState)
{
	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		m_aCurrentStates[MotivationState] = MIN_MOTIVATION_STATE_VALUE; 
	}
}

float CPedMotivation::GetMotivation(PedMotivationState MotivationState) const
{
	//These are currently unused, and need to be re-enabled if desired.
	aiAssert(MotivationState != HAPPY_STATE);
	aiAssert(MotivationState != HUNGRY_STATE);
	aiAssert(MotivationState != TIRED_STATE);

	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		return m_aCurrentStates[MotivationState]; 
	}
	
	//return 0;
}

void CPedMotivation::SetMotivation(PedMotivationState MotivationState, float fValue)
{
	Assert(fValue >= MIN_MOTIVATION_STATE_VALUE && fValue <= MAX_MOTIVATION_STATE_VALUE);

	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		m_aCurrentStates[MotivationState] = fValue;
	}
}

float CPedMotivation::GetMotivationRateModifier(PedMotivationState MotivationState) const
{
	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		return m_aMotivationRateModifiers[MotivationState]; 
	}

	//return 0;
}

void  CPedMotivation::SetMotivationRateModifier(PedMotivationState MotivationState, float fMotivationRate)
{
	Assertf((fMotivationRate >= 0.0f && fMotivationRate <= 100.0f),"Motivation rate modifiers can be set between 0 and 100" );
	
	//if(MotivationState > -1 && MotivationState < NUMBER_OF_MOTIVATION_STATES )
	{
		m_aMotivationRateModifiers[MotivationState] = fMotivationRate; 
	}
}

void CPedMotivation::Update()
{
	static float s_fFastDecay = DEFAULT_MOTIVATION_RESPONSE / 100.0f;
	//static float s_fSlowDecay = DEFAULT_MOTIVATION_RESPONSE / 1000.0f;

	UpdateMotivationState(m_aCurrentStates[ANGRY_STATE],	m_aDefaultStates[ANGRY_STATE],	s_fFastDecay);
	UpdateMotivationState(m_aCurrentStates[FEAR_STATE],		m_aDefaultStates[FEAR_STATE],	s_fFastDecay);

	//These are currently unused, and need to be re-enabled if desired.
	//UpdateMotivationState(m_aCurrentStates[HAPPY_STATE],	m_aDefaultStates[HAPPY_STATE],	s_fFastDecay);
	//UpdateMotivationState(m_aCurrentStates[HUNGRY_STATE],	m_aDefaultStates[HUNGRY_STATE], s_fSlowDecay);
	//UpdateMotivationState(m_aCurrentStates[TIRED_STATE],	m_aDefaultStates[TIRED_STATE],	s_fSlowDecay);
}


////////////////////////////////////////////////////////////////////////////////
// CPedMotivationInfo
////////////////////////////////////////////////////////////////////////////////



////////////////////////////////////////////////////////////////////////////////
// CPedMotivationInfoManager
////////////////////////////////////////////////////////////////////////////////

// Static manager object
CPedMotivationInfoManager CPedMotivationInfoManager::m_MotivationManagerInstance;
//atArray<CPedMotivationInfo> CPedMotivationInfoManager::m_aPedMotivation;  



void CPedMotivationInfoManager::Init()
{
	Load(); 
}

void CPedMotivationInfoManager::Shutdown()
{
	// Delete
	Reset();
}

void CPedMotivationInfoManager::Load()
{
	// Iterate through the files backwards, so episodic data can override any old data
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetLastFile(CDataFileMgr::MOTIVATIONS_FILE);
	while(DATAFILEMGR.IsValid(pData))
	{
		// Load
		PARSER.LoadObject(pData->m_filename, "meta", m_MotivationManagerInstance);

		// Get next file
		pData = DATAFILEMGR.GetPrevFile(pData);
	}
}

void CPedMotivationInfoManager::Reset()
{
	m_MotivationManagerInstance.m_aPedMotivation.Reset(); 
}