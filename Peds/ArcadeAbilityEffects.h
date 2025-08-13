// Title	:	ArcadeAbilityeffects.h
// Author	:	Ross McDonald (Ruffian Games)
// Started	:	07/02/20

#pragma once

#include "atl/array.h"
#include "atl/bitset.h"

enum eArcadeAbilityEffectType
{
	AAE_NONE = -1,
	AAE_PARKOUR = 0,
	AAE_SNITCH,
	AAE_MARATHON_MAN,
	AAE_SECOND_WIND,
	AAE_HACK_THE_SYSTEM,
	AAE_FREIGHT_TRAIN,
	AAE_NUM_TYPES
};

class CArcadePassiveAbilityEffectsConfigInfo
{
public:
	const float GetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType) const;
	void SetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType, float modifierValue);

	CArcadePassiveAbilityEffectsConfigInfo();
	virtual ~CArcadePassiveAbilityEffectsConfigInfo();

private:
	atFixedArray<float, AAE_NUM_TYPES> m_values;
};

typedef u8 ArcadePassiveAbilityEffectsFlags;

class CArcadePassiveAbilityEffects
{

public:

	enum Flag
	{
		AAE_NULL				= 0,
		AAE_PARKOUR				= BIT(eArcadeAbilityEffectType::AAE_PARKOUR),
		AAE_SNITCH				= BIT(eArcadeAbilityEffectType::AAE_SNITCH),
		AAE_MARATHON_MAN		= BIT(eArcadeAbilityEffectType::AAE_MARATHON_MAN),
		AAE_SECOND_WIND			= BIT(eArcadeAbilityEffectType::AAE_SECOND_WIND),
		AAE_HACK_THE_SYSTEM		= BIT(eArcadeAbilityEffectType::AAE_HACK_THE_SYSTEM),
		AAE_FREIGHT_TRAIN		= BIT(eArcadeAbilityEffectType::AAE_FREIGHT_TRAIN),
	};

	CArcadePassiveAbilityEffects();
	virtual ~CArcadePassiveAbilityEffects();

	inline void Initialise() { Reset(); }
	inline void Reset() { m_arcadePassiveAbilityFlags = Flag::AAE_NULL;  }
	inline void Cleanup() { Reset(); }	
	inline u8 GetArcadePassiveAbilityEffectsFlags() const { return static_cast<u8>(m_arcadePassiveAbilityFlags); }
	inline void SetArcadePassiveAbilityEffectsFlags(u8 flags) { m_arcadePassiveAbilityFlags = (ArcadePassiveAbilityEffectsFlags)flags; }
	
	const bool GetIsActive(eArcadeAbilityEffectType arcadeAbilityEffectType) const;
	void SetIsActive(eArcadeAbilityEffectType arcadeAbilityEffectType, bool isActiveValue);

	const float GetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType) const;
	void SetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType, float modifierValue);

private:	
	ArcadePassiveAbilityEffectsFlags m_arcadePassiveAbilityFlags;
	static CArcadePassiveAbilityEffectsConfigInfo ms_abilityValues;

	inline void SetFlag(Flag flag) { m_arcadePassiveAbilityFlags |= flag; }
	inline bool GetFlag(Flag flag) const { return (m_arcadePassiveAbilityFlags & flag) != 0; }
	inline void ClearFlag(Flag flag) { m_arcadePassiveAbilityFlags &= ~flag; }
};
