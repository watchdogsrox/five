//
// Peds\CombatBehaviour.h
//
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_COMBAT_BEHAVIOUR_H
#define INC_COMBAT_BEHAVIOUR_H

////////////////////////////////////////////////////////////////////////////////

// Rage Headers
#include "ai/task/taskchannel.h"
#include "bank/combo.h"
#include "fwtl/pool.h"
#include "fwutil/Flags.h"
#include "parser/macros.h"

// Game Headers
#include "Peds/CombatData.h"
#include "Weapons/FiringPattern.h"

#if __WIN32
	#pragma warning(push)
	#pragma warning(disable:4201)	// nonstandard extension used : nameless struct/union
#endif

////////////////////////////////////////////////////////////////////////////////
// This list must match the list in commands_ped.sch
// for parsing keep this file updated as well: //depot/gta5/src/dev/game/Peds/CombatInfo.psc

#define BH_CC_FLOAT_ATTRIB_LIST					\
	ITEM(BlindFireChance)						\
	ITEM(BurstDurationInCover)					\
	ITEM(MaxShootingDistance)					\
	ITEM(TimeBetweenBurstsInCover)				\
	ITEM(TimeBetweenPeeks)						\
	ITEM(StrafeWhenMovingChance)				\
	ITEM(WeaponAccuracy)						\
	ITEM(FightProficiency)						\
	ITEM(WalkWhenStrafingChance)				\
	ITEM(HeliSpeedModifier)						\
	ITEM(HeliSensesRange)						\
	ITEM(AttackWindowDistanceForCover)			\
	ITEM(TimeToInvalidateInjuredTarget)			\
	ITEM(MinimumDistanceToTarget)				\
	ITEM(BulletImpactDetectionRange)			\
	ITEM(AimTurnThreshold)						\
	ITEM(OptimalCoverDistance)					\
	ITEM(AutomobileSpeedModifier)				\
	ITEM(SpeedToFleeInVehicle)					\
	ITEM(TriggerChargeTime_Far)					\
	ITEM(TriggerChargeTime_Near)				\
	ITEM(MaxDistanceToHearEvents)				\
	ITEM(MaxDistanceToHearEventsUsingLOS) 		\
	ITEM(HomingRocketBreakLockAngle)			\
	ITEM(HomingRocketBreakLockAngleClose)		\
	ITEM(HomingRocketBreakLockCloseDistance)	\
	ITEM(HomingRocketTurnRateModifier)			\
	ITEM(TimeBetweenAggressiveMovesDuringVehicleChase)\
	ITEM(MaxVehicleTurretFiringRange)			\
	ITEM(WeaponDamageModifier)

// Use the above list to make our combat float list
enum CombatFloats
{
	kAttribFloatInvalid = -1,

#define ITEM(a) kAttribFloat##a,
	BH_CC_FLOAT_ATTRIB_LIST
#undef ITEM

	kMaxCombatFloats
};

////////////////////////////////////////////////////////////////////////////////

// Forward Declarations
class CPed;
class CPedWeaponManager;

// Typedefs
typedef CCombatData::BehaviourFlagsBitSet CombatBehaviourFlags;

// Constants
const float MAX_SHOOT_RATE_MODIFIER = 10.0f;

////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Defines an individual peds combat style
////////////////////////////////////////////////////////////////////////////////

class CCombatBehaviour
{
public:

	// PURPOSE: Constructor
	CCombatBehaviour();

	// PURPOSE: Destructor
	~CCombatBehaviour() {}

	// PURPOSE: What type of combat ped is performing (determines which combat transition tables to use)
	typedef enum
	{
		CT_OnFootArmed,
		CT_OnFootArmedSeekCoverOnly,
		CT_OnFootUnarmed,
		CT_Underwater,
		CT_NumTypes,
	} CombatType;

	// PURPOSE: Initialises a peds combat behaviour from the combat data
	void InitFromCombatData(atHashWithStringNotFinal iCombatInfoHash);

	// PURPOSE: Gets/Sets the combat CCombatData::Movement style
	CCombatData::Movement GetCombatMovement() const { return m_CombatMovement; }
	void SetCombatMovement(CCombatData::Movement combatMovement) { m_CombatMovement = combatMovement; }

	// PURPOSE: Gets/Sets the peds combat proficiency, the higher the more proficient
	CCombatData::Ability GetCombatAbility() const { return m_CombatAbility; }
	void SetCombatAbility(CCombatData::Ability val) { m_CombatAbility = val; }

	// PURPOSE: Gets/sets combat style flags
	bool IsFlagSet(CCombatData::BehaviourFlags eFlag) const { return m_BehaviourFlags.BitSet().IsSet(eFlag); }
	void SetFlag(CCombatData::BehaviourFlags eFlag) { m_BehaviourFlags.BitSet().Set(eFlag); }
	void ChangeFlag(CCombatData::BehaviourFlags eFlag, bool bVal) { m_BehaviourFlags.BitSet().Set(eFlag, bVal); }
	void ClearFlag(CCombatData::BehaviourFlags eFlag) { m_BehaviourFlags.BitSet().Clear(eFlag); }

	const CombatBehaviourFlags GetCombatBehaviorFlags() const { return m_BehaviourFlags; }
	void SetCombatBehaviorFlags(const CCombatData::BehaviourFlagsBitSet& flags) { m_BehaviourFlags = flags; }

	// PURPOSE: Gets sets the attack window ranges
	CCombatData::Range GetAttackRange() { return m_AttackRanges; }
	void SetAttackRange(CCombatData::Range range) { m_AttackRanges = range; }

	// PURPOSE: Gets sets the target loss response
	CCombatData::TargetLossResponse GetTargetLossResponse() const { return m_TargetLossResponse; }
	void SetTargetLossResponse(CCombatData::TargetLossResponse val) { m_TargetLossResponse = val; }

	// PURPOSE: Gets/sets the target injured reaction
	CCombatData::TargetInjuredReaction GetTargetInjuredReaction() const { return m_TargetInjuredReaction; }
	void SetTargetInjuredReaction(CCombatData::TargetInjuredReaction val) { m_TargetInjuredReaction = val; }

	// PURPOSE: Gets/sets the shoot rate modifier - this changes the fire rate
	float GetShootRateModifier() const { return m_fShootRateModifierThisFrame; }
	void SetShootRateModifier(float fShootRateModifier) 
	{ 
		if (taskVerifyf(fShootRateModifier > 0.0f, "Shoot rate modifier should be greater than zero"))
		{
			m_fShootRateModifier = Min(fShootRateModifier, MAX_SHOOT_RATE_MODIFIER);
			float fTimeTillNextModifier = 1.0f / m_fShootRateModifier;
			m_FiringPattern.SetTimeTillNextBurstModifier(fTimeTillNextModifier); 
			m_FiringPattern.SetTimeTillNextShotModifier(fTimeTillNextModifier); 
		}
	}

	// PURPOSE: If our single frame shoot rate modifier doesn't match our overall modifier then reset it
	void ResetSingleFrameShootRateModifier()
	{
		if(m_fShootRateModifierThisFrame != m_fShootRateModifier)
		{
			// This will also make sure the firing pattern rates match our shoot rate modifier
			SetShootRateModifierThisFrame(m_fShootRateModifier);
		}
	}

	// PURPOSE: Sets the single frame shoot rate modifier plus the firing patterns modifiers
	void SetShootRateModifierThisFrame(float fSingleFrameShootRateModifier)
	{
		if (taskVerifyf(fSingleFrameShootRateModifier > 0.0f, "Shoot rate modifier should be greater than zero"))
		{
			m_fShootRateModifierThisFrame = Min(fSingleFrameShootRateModifier, MAX_SHOOT_RATE_MODIFIER);
			float fTimeTillNextModifier = 1.0f / m_fShootRateModifierThisFrame;
			m_FiringPattern.SetTimeTillNextBurstModifier(fTimeTillNextModifier); 
			m_FiringPattern.SetTimeTillNextShotModifier(fTimeTillNextModifier); 
		}
	}

	// PURPOSE: Gets the peds firing pattern
	const CFiringPattern& GetFiringPattern() const { return m_FiringPattern; }
	CFiringPattern& GetFiringPattern() { return m_FiringPattern; }

	// PURPOSE: Sets this ped's firing pattern hash and also sets the firing pattern info
	void SetFiringPattern(u32 firingPatternHash)
	{
		m_FiringPatternHash = atHashWithStringNotFinal(firingPatternHash);
		m_FiringPattern.SetFiringPattern(m_FiringPatternHash, true);
	}

	// PURPOSE: Gets this ped's firing pattern hash
	const atHashWithStringNotFinal GetFiringPatternHash() const { return m_FiringPatternHash; }

	// PURPOSE: Forced injured on ground interface
	void ForceInjuredOnGroundBehaviour( float duration ) { m_BehaviourFlags.BitSet().Set(CCombatData::BF_ForceInjuredOnGround, true); m_InjuredOnGroundDuration = (s32)(duration*1000.0f);}
	void ClearForcedInjuredOnGroundBehaviour(){ m_BehaviourFlags.BitSet().Clear(CCombatData::BF_ForceInjuredOnGround); m_InjuredOnGroundDuration = 15000;}
	s32 GetForcedInjuredOnGroundDuration() { return m_InjuredOnGroundDuration; }
	void DisableInjuredOnGroundBehaviour() { m_BehaviourFlags.BitSet().Set(CCombatData::BF_DisableInjuredOnGround, true); }
	void ClearDisableInjuredOnGroundBehaviour(){ m_BehaviourFlags.BitSet().Clear(CCombatData::BF_DisableInjuredOnGround); }

	// PURPOSE: Gets/sets a peds preferred cover point list
	s32 GetPreferredCoverGuid() const { return m_iPreferredCoverGuid; }
	void SetPreferredCoverGuid(s32 iPreferredCoverGuid) { m_iPreferredCoverGuid = iPreferredCoverGuid; }

	void SetCombatFloat(CombatFloats eCombatFloat, float fNewValue) { m_aCombatFloats[eCombatFloat] = fNewValue; }
	float GetCombatFloat(CombatFloats eCombatFloat) const { return m_aCombatFloats[eCombatFloat]; }

	// Returns the current combat type, on foot, skiing, underwater etc...
	static CCombatBehaviour::CombatType GetCombatType( const CPed* pPed );

	static bool GetDefaultCombatFloat(const CPed& rPed, CombatFloats eCombatFloat, float& fValue);

	static void SetDefaultCombatMovement(CPed& rPed);
	static void SetDefaultTargetLossResponse(CPed& rPed);

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Defines how the AI can navigate and move during combat
	CCombatData::Movement m_CombatMovement;

	// PURPOSE: A set of flags that alters the way an AI behaves during combat
	CombatBehaviourFlags m_BehaviourFlags;

	// PURPOSE: How proficient this AI is during combat 0 to 1 the higher the more proficient
	CCombatData::Ability m_CombatAbility;

	// PURPOSE: Defines the perfect attack range for this ped
	CCombatData::Range m_AttackRanges;

	// PURPOSE: Defines how a ped will react to losing the target
	CCombatData::TargetLossResponse	m_TargetLossResponse;

	// PURPOSE: Defines how a ped will react to an injured target, e.g. stop attacking or carry on
	CCombatData::TargetInjuredReaction m_TargetInjuredReaction;

	// PURPOSE: If the forced injured on ground flag is set, this stores the required injured on ground duration
	s32 m_InjuredOnGroundDuration;

	// PURPOSE: A modifier applied to the firing rate
	float m_fShootRateModifier;

	// PURPOSE: A single frame modifier applied to the firing rate
	float m_fShootRateModifierThisFrame;

	atRangeArray<float, kMaxCombatFloats>	m_aCombatFloats;

	// PURPOSE: Determines the peds firing style
	CFiringPattern m_FiringPattern;

	// PURPOSE: This is the firing pattern we generall want to use in tasks
	atHashWithStringNotFinal m_FiringPatternHash;

	// PURPOSE: Script guid for a peds preferred cover point list
	s32 m_iPreferredCoverGuid;

	////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Default combat information that script can use as a basis
////////////////////////////////////////////////////////////////////////////////

class CCombatInfo
{
public:

	// PURPOSE: Pool declaration
	FW_REGISTER_CLASS_POOL(CCombatInfo);

	// PURPOSE: Constructor
	CCombatInfo() {};

	// PURPOSE: Destructor
	~CCombatInfo() {};

	// PURPOSE: Get the name of this combat info
	atHashWithStringNotFinal GetName() const { return m_Name; }

	// PURPOSE: Accessors to combat attributes
	CCombatData::Movement GetCombatMovement() const { return m_CombatMovement; }
	CombatBehaviourFlags GetBehaviourFlags() const { return m_BehaviourFlags; }
	CCombatData::Ability GetCombatAbility() const { return m_CombatAbility; }
	CCombatData::Range GetAttackRanges() const { return m_AttackRanges; }
	CCombatData::TargetLossResponse GetTargetLossResponse() const { return m_TargetLossResponse; }
	CCombatData::TargetInjuredReaction GetTargetInjuredReaction() const { return m_TargetInjuredReaction; }
	float GetWeaponShootRateModifier() const { return m_WeaponShootRateModifier; }
	float GetCombatFloat(CombatFloats eCombatFloat) const { return m_CombatFloatArray[eCombatFloat]; }
	u32 GetFiringPatternHash() const { return m_FiringPatternHash.GetHash(); }

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Name of the combat info
	atHashWithStringNotFinal m_Name;

	// PURPOSE: Type of CCombatData::Movement
	CCombatData::Movement m_CombatMovement;

	// PURPOSE: A set of flags that alters the way an AI behaves during combat
	CombatBehaviourFlags m_BehaviourFlags;

	// PURPOSE: How proficient this AI is during combat 0 to 1 the higher the more proficient
	CCombatData::Ability m_CombatAbility;

	// PURPOSE: Defines the perfect attack range for this ped
	CCombatData::Range m_AttackRanges;

	// PURPOSE: Defines how a ped will react to losing the target
	CCombatData::TargetLossResponse m_TargetLossResponse;

	// PURPOSE: Defines how a ped will react to an injured target, e.g. stop attacking or carry on
	CCombatData::TargetInjuredReaction m_TargetInjuredReaction;

	// PURPOSE: Modifier to affect firing pattern time between bursts
	float m_WeaponShootRateModifier;

	// PURPOSE: Peds default firing pattern
	atHashWithStringNotFinal m_FiringPatternHash;

	union {
		struct {
			#define ITEM(a) float m_##a;
			BH_CC_FLOAT_ATTRIB_LIST
			#undef ITEM
		};
		float m_CombatFloatArray[kMaxCombatFloats];
	};

	PAR_SIMPLE_PARSABLE

	////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// PURPOSE: Combat info manager, handles combat infos
////////////////////////////////////////////////////////////////////////////////

class CCombatInfoMgr
{
public:

	// PURPOSE: Initialise combat info pool and load file
	static void Init(unsigned initMode);

	// PURPOSE: Clear combat infos and shutdown pool
	static void Shutdown(unsigned shutdownMode);

	// PURPOSE: Get a combat info by hash
	static const CCombatInfo* GetCombatInfoByHash(u32 uHash);

	// PURPOSE: Get a combat info by index
	static const CCombatInfo* GetCombatInfoByIndex(u32 uIndex);

	// PURPOSE: Get the number of combat infos
	static u32 GetNumberOfCombatInfos() { return ms_Instance.m_CombatInfos.GetCount(); }

	// PURPOSE: Constructor
	CCombatInfoMgr() {};

	// PURPOSE: Destructor
	~CCombatInfoMgr() {};

private:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Clear all infos in memory
	static void Reset();

	// PURPOSE: Load all infos from file
	static void Load();

	// PURPOSE: Static instance of metadata 
	static CCombatInfoMgr ms_Instance;

	// PURPOSE: The array of combat infos defined in data
	atArray<CCombatInfo*> m_CombatInfos;

	PAR_SIMPLE_PARSABLE

public:

	////////////////////////////////////////////////////////////////////////////////

	// PURPOSE: Overrides combat behaviour using a specified file.
	static void Override(const char* fileName);

	// PURPOSE: Reverts to original combat behaviour.
	static void Revert();

#if __BANK
	// PURPOSE: Save metadata to file
	static void Save();

	// PURPOSE: Add widgets to RAG
	static void AddWidgets(bkBank& bank);

	// PURPOSE: Gives the focus ped the selected combat info
	static void GiveFocusPedSelectedCombatInfo();

	// PURPOSE: For combat info selection
	static bkCombo*				ms_pCombatInfoCombo;
	static s32					ms_iSelectedCombatInfo;
	static atArray<const char*> ms_CombatInfoNames;		
#endif

	////////////////////////////////////////////////////////////////////////////////
};

/////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////
// CCombatBehaviourDebug
//////////////////////////////////////////////////////////////////////////

#if __BANK
class CCombatBehaviourDebug
{
public:

	static void InitGroup();
	static void AddWidgets(bkBank& bank);
	static void ShutdownGroup(); 

private:

	// PURPOSE: Pointer to the main RAG bank
	static bkBank* ms_pBank;
};
#endif // __BANK

/////////////////////////////////////////////////////////////////////////////////

#if __WIN32
	#pragma warning(pop)
#endif

#endif // INC_COMBAT_BEHAVIOUR_H
