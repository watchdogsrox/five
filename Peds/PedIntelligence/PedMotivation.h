// FILE :    PedMotivation.h
// PURPOSE : Used to the emotional motivation for a peds response to a given event or situation...
// CREATED : 15-09-2010

#ifndef PED_MOTIVATION_H
#define PED_MOTIVATION_H

// Rage headers
#include "atl/hashstring.h"
#include "atl/array.h"
#include "math/amath.h"
#include "parser/macros.h"

// Framework headers

// Game headers

#define DEFAULT_MOTIVATION_RESPONSE (0.01f)
#define MAX_MOTIVATION_STATE_VALUE	(1.0f)
#define MIN_MOTIVATION_STATE_VALUE	(0.0f)
#define DEFAULT_MOTIVATION_RATE		(1.0f)

#define REALLY_ANGRY				(0.9f)

class CPedMotivationInfo; 

class CPedMotivation
{
public:	

	
	CPedMotivation();
	~CPedMotivation(); 
	
	enum PedMotivationState
	{
		HAPPY_STATE,	//moods
		FEAR_STATE,
		ANGRY_STATE,
		HUNGRY_STATE,	//desires
		TIRED_STATE,
		NUMBER_OF_MOTIVATION_STATES

	};

	//Change the motivation state based on events happening to peds. 
	void IncreasePedMotivation(PedMotivationState MotivationState, const float fResponse = DEFAULT_MOTIVATION_RESPONSE);
	void DecreasePedMotivation(PedMotivationState MotivationState, const float fResponse = DEFAULT_MOTIVATION_RESPONSE); 
	
	//Get the current motivation value
	float GetMotivation(PedMotivationState MotivationState) const; 

	//Set the motivation
	void SetMotivation(PedMotivationState MotivationState, float fValue);

	//Set the motivation value to max or min value
	void SetMotivationToMax(PedMotivationState MotivationState); 
	void SetMotivationToMin(PedMotivationState MotivationState); 

	//Alter the rate at which motivations change.
	float GetMotivationRateModifier(PedMotivationState MotivationState) const;	
	void SetMotivationRateModifier(PedMotivationState MotivationState, float fMotivation);

	//Set the motivation profile for the ped as defined in pedpersonality.dat
	void SetMotivationProfile(u32 NameHash); 
	
	//Converge motivations to default.
	void Update();

	// Accessors for motivations.
	bool IsAfraid() const { return GetMotivation(FEAR_STATE) >= sm_fFearThreshold; }

private:
	//Given the max value rate modifier peds responses.
	float RandomiseMotivationRateModifiers(float fMotivationRate); 

	//Update each of the motivation states 
	void UpdateMotivationState(float& fCurrentState, const float fDefaultState, float fDecay)
	{
		if(fCurrentState > fDefaultState)
		{
			fCurrentState = Clamp(fCurrentState - fDecay, fDefaultState, MAX_MOTIVATION_STATE_VALUE);
		}
		else if(fCurrentState < fDefaultState)
		{
			fCurrentState = Clamp(fCurrentState + fDecay, MIN_MOTIVATION_STATE_VALUE, fDefaultState);
		}
	}

	static dev_float sm_fFearThreshold;
	
	float m_aDefaultStates[NUMBER_OF_MOTIVATION_STATES];
		
	float m_aCurrentStates[NUMBER_OF_MOTIVATION_STATES];

	float m_aMotivationRateModifiers[NUMBER_OF_MOTIVATION_STATES]; 
	
};


class CPedMotivationInfo
{
public:
	// Get the hash
	u32 GetHash() const { return m_Name.GetHash(); }

	float GetDefaultHappyValue() const { return m_Happy; }

	float GetDefaultAngerValue() const { return m_Anger; } 

	float GetDefaultFearValue() const { return m_Fear; }
	
	float GetDefaultHungerValue() const { return m_Hunger; }

	float GetDefaultTiredValue() const { return m_Tired; }
	
	float GetHappyRate() const { return m_HappyRate; }

	float GetAngerRate() const { return m_AngerRate; } 

	float GetFearRate() const { return m_FearRate; }

	float GetHungerRate() const { return m_HungerRate; }

	float GetTiredRate() const { return m_TiredRate; }
	

private:
	atHashWithStringNotFinal m_Name;
	float m_Happy; 
	float m_Anger;
	float m_Fear;
	float m_Hunger; 
	float m_Tired; 
	float m_HappyRate; 
	float m_AngerRate; 
	float m_FearRate; 
	float m_HungerRate; 
	float m_TiredRate; 

	PAR_SIMPLE_PARSABLE;

};

class CPedMotivationInfoManager
{
public:
	// Initialise
	static void Init();

	// Shutdown
	static void Shutdown();

	// Access to the firing pattern data
	static const CPedMotivationInfo* GetInfo(u32 uNameHash)
	{
		if(uNameHash != 0)
		{
			for (s32 i = 0; i < m_MotivationManagerInstance.m_aPedMotivation.GetCount(); i++ ) 
			{
				if ( m_MotivationManagerInstance.m_aPedMotivation[i].GetHash() == uNameHash)
				{
					return &m_MotivationManagerInstance.m_aPedMotivation[i]; 
				}
			}
		}
		return NULL;
	}		
	
private:
	
	// Delete the data
	static void Reset();

	// Load the data
	static void Load(); 
	
	atArray<CPedMotivationInfo> m_aPedMotivation;  
	
	static CPedMotivationInfoManager m_MotivationManagerInstance; 
	
	PAR_SIMPLE_PARSABLE;
};





#endif

