#ifndef INC_FLEE_BEHAVIOUR_H
#define INC_FLEE_BEHAVIOUR_H

#include "fwutil/Flags.h"
#include "system/bit.h"

#include "fwanimation/animdefines.h"

// Defines an individual peds flee style
class CFleeBehaviour
{
public:
	CFleeBehaviour()
		: m_fleeBehaviourFlags()
		, m_sCowerHash("CODE_HUMAN_COWER",0xD58CB510)
	{
	}
	~CFleeBehaviour() {}

	typedef enum
	{
		FT_OnFoot = 0,
		FT_Skiing,
		FT_InVehicle,
		FT_NumTypes,
	} FleeType;

	typedef enum 
	{
		BF_CanUseCover								= BIT0,
		BF_CanUseVehicles							= BIT1,
		BF_CanScream								= BIT2,
		BF_PreferPavements							= BIT3,
		BF_WanderAtEnd								= BIT4,
		BF_LookForCrowds							= BIT5,
		BF_WillReturnToOriginalPositionAfterFlee	= BIT6, // Respond to new events
		BF_DisableHandsUp							= BIT7,
		BF_UpdateToNearestHatedPed					= BIT8,
		BF_NeverFlee								= BIT9,
		BF_DisableCower								= BIT10,
		BF_DisableExitVehicle						= BIT11,
		BF_DisableReverseInVehicle					= BIT12,
		BF_DisableAccelerateInVehicle				= BIT13,
		BF_DisableFleeFromIndirectThreats			= BIT14,
		BF_CowerInsteadOfFlee						= BIT15,
		BF_ForceExitVehicle							= BIT16,
		BF_DisableHesitateInVehicle					= BIT17,
		BF_DisableAmbientClips						= BIT18,
	} BehaviourFlags;

	// Gets/sets flee style flags
	fwFlags32& GetFleeFlags() { return m_fleeBehaviourFlags; }
	const fwFlags32& GetFleeFlags() const { return m_fleeBehaviourFlags; }

	void SetFleeFlags(u32 flags) { m_fleeBehaviourFlags = flags; }

	// Setter / Getter for which scenario hash to use when cowering
	void SetStationaryReactHash(atHashWithStringNotFinal sReactHash)	{	m_sCowerHash = sReactHash;	}
	atHashWithStringNotFinal GetStationaryReactHash() const				{	return m_sCowerHash;	}

private:

	// A set of flags that alters the way an AI behaves during fleeing
	fwFlags32	m_fleeBehaviourFlags;

	atHashWithStringNotFinal m_sCowerHash;
};


#endif // INC_FLEE_BEHAVIOUR_H
