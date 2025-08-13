// Title	:	ArcadeAbilityeffects.cpp
// Author	:	Ross McDonald (Ruffian Games)
// Started	:	07/02/20

#include "ArcadeAbilityEffects.h"
#include "ped_channel.h"

CArcadePassiveAbilityEffectsConfigInfo::CArcadePassiveAbilityEffectsConfigInfo()
{
	m_values = atFixedArray<float, AAE_NUM_TYPES>();
	
	const float fDefaultModifier = 1.0f;
	for (int i = 0; i < eArcadeAbilityEffectType::AAE_NUM_TYPES; ++i)
	{
		m_values.Push(fDefaultModifier);
	}
}

CArcadePassiveAbilityEffectsConfigInfo::~CArcadePassiveAbilityEffectsConfigInfo()
{
	m_values.Reset();
}

const float CArcadePassiveAbilityEffectsConfigInfo::GetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType) const
{
	if (m_values.IsEmpty()) return 0.0f;
	if (arcadeAbilityEffectType >= 0 && arcadeAbilityEffectType < m_values.GetCount())
	{
		return m_values[arcadeAbilityEffectType];
	}
	return 0.0f;
}

void CArcadePassiveAbilityEffectsConfigInfo::SetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType, float modifierValue)
{
	if (m_values.IsEmpty()) return;
	if (arcadeAbilityEffectType >= 0 && arcadeAbilityEffectType < m_values.GetCount())
	{
		m_values[arcadeAbilityEffectType] = modifierValue;
	}
}

CArcadePassiveAbilityEffectsConfigInfo CArcadePassiveAbilityEffects::ms_abilityValues = CArcadePassiveAbilityEffectsConfigInfo();

CArcadePassiveAbilityEffects::CArcadePassiveAbilityEffects()
	: m_arcadePassiveAbilityFlags(Flag::AAE_NULL)
{
}

CArcadePassiveAbilityEffects::~CArcadePassiveAbilityEffects()
{
}

const bool CArcadePassiveAbilityEffects::GetIsActive(eArcadeAbilityEffectType arcadeAbilityEffectType) const
{	
	Flag flag = (Flag)(1 << ((int)arcadeAbilityEffectType));
	return GetFlag(flag);
}

void CArcadePassiveAbilityEffects::SetIsActive(eArcadeAbilityEffectType arcadeAbilityEffectType, bool isActiveValue)
{
	if (eArcadeAbilityEffectType::AAE_NONE == arcadeAbilityEffectType)
	{
		return Reset();
	}

	Flag flag = (Flag)(1 << ((int)arcadeAbilityEffectType));
	return isActiveValue ? SetFlag(flag) : ClearFlag(flag);
}

const float CArcadePassiveAbilityEffects::GetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType) const
{	
	return ms_abilityValues.GetModifier(arcadeAbilityEffectType);
}

void CArcadePassiveAbilityEffects::SetModifier(eArcadeAbilityEffectType arcadeAbilityEffectType, float modifierValue)
{	
	ms_abilityValues.SetModifier(arcadeAbilityEffectType, modifierValue);
}