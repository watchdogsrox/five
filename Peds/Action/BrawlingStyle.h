// FILE		: BrawlingStyle.h
// PURPOSE	: Load in brawling type information and manage the styles
// CREATED	: 06/10/11

#ifndef INC_BRAWLING_STYLE_H
#define INC_BRAWLING_STYLE_H

// Rage headers
#include "atl/array.h"
#include "atl/hashstring.h"
#include "parser/macros.h"

class CBrawlingStyleData
{
public:
	CBrawlingStyleData() :m_TargetRadius(1.5f) {}

	const atHashWithStringNotFinal	GetName								(void) const { return m_Name; }
	float							GetTargetRadius						(void) const { return m_TargetRadius; }
	float							GetKeepMovingWhilePathingDistance	(void) const { return m_KeepMovingWhilePathingDistance; }
	float							GetMaxDistanceMayAdjustPathEndPosition(void) const { return m_MaxDistanceMayAdjustPathEndPosition; }
	float							GetMeleeMovementMBR					(void) const { return m_MeleeMovementMBR; }
	float							GetAttackRangeMax					(void) const { return m_AttackRangeMax; }
	float							GetAttackProbabilityToComboMin		(void) const { return m_AttackProbabilityToComboMin; }
	float							GetAttackProbabilityToComboMax		(void) const { return m_AttackProbabilityToComboMax; }
	float							GetProbabilityToBeDazedMin			(void) const { return m_ProbabilityToBeDazedMin; }
	float							GetProbabilityToBeDazedMax			(void) const { return m_ProbabilityToBeDazedMax; }
	float							GetBlockProbabilityMin				(void) const { return m_BlockProbabilityMin; }
	float							GetBlockProbabilityMax				(void) const { return m_BlockProbabilityMax; }
	float							GetCounterProbabilityMin			(void) const { return m_CounterProbabilityMin; }
	float							GetCounterProbabilityMax			(void) const { return m_CounterProbabilityMax; }
	float							GetTauntProbability					(void) const { return m_TauntProbability; }
	float							GetTauntProbabilityQueued			(void) const { return m_TauntProbabilityQueued; }
	s32								GetSeekModeScanTimeMin				(void) const { return m_SeekModeScanTimeMin; }
	s32								GetSeekModeScanTimeMax				(void) const { return m_SeekModeScanTimeMax; }
	s32								GetAttackFrequencyWorstFighterMin	(void) const { return m_AttackFrequencyWorstFighterMinInMs; } 
	s32								GetAttackFrequencyWorstFighterMax	(void) const { return m_AttackFrequencyWorstFighterMaxInMs; }
	s32								GetAttackFrequencyBestFighterMin	(void) const { return m_AttackFrequencyBestFighterMinInMs; } 
	s32								GetAttackFrequencyBestFighterMax	(void) const { return m_AttackFrequencyBestFighterMaxInMs; }
	s32								GetTauntFrequencyMin				(void) const { return m_TauntFrequencyMinInMs; } 
	s32								GetTauntFrequencyMax				(void) const { return m_TauntFrequencyMaxInMs; }
	s32								GetTauntFrequencyQueuedMin			(void) const { return m_TauntFrequencyQueuedMinInMs; } 
	s32								GetTauntFrequencyQueuedMax			(void) const { return m_TauntFrequencyQueuedMaxInMs; }
	bool							ShouldPlayTauntBeforeAttack			(void) const { return m_PlayTauntBeforeAttacking; }

private:
	atHashWithStringNotFinal	m_Name;
	float						m_TargetRadius;
	float						m_KeepMovingWhilePathingDistance;
	float						m_MaxDistanceMayAdjustPathEndPosition;
	float						m_MeleeMovementMBR;
	float						m_AttackRangeMax;
	float						m_AttackProbabilityToComboMin;
	float						m_AttackProbabilityToComboMax;
	float						m_ProbabilityToBeDazedMin;
	float						m_ProbabilityToBeDazedMax;
	float						m_TauntProbability;
	float						m_TauntProbabilityQueued;
	float						m_BlockProbabilityMin;
	float						m_BlockProbabilityMax;
	float						m_CounterProbabilityMin;
	float						m_CounterProbabilityMax;
	s32							m_SeekModeScanTimeMin;
	s32							m_SeekModeScanTimeMax;
	s32							m_AttackFrequencyWorstFighterMinInMs;
	s32							m_AttackFrequencyWorstFighterMaxInMs;
	s32							m_AttackFrequencyBestFighterMinInMs;
	s32							m_AttackFrequencyBestFighterMaxInMs;	
	s32							m_TauntFrequencyMinInMs;
	s32							m_TauntFrequencyMaxInMs;
	s32							m_TauntFrequencyQueuedMinInMs;
	s32							m_TauntFrequencyQueuedMaxInMs;
	bool						m_PlayTauntBeforeAttacking;

	PAR_SIMPLE_PARSABLE;
};

class CBrawlingStyleManager
{
public:
	static void							Init(const char* fileName);
	static void							Shutdown();
	static const CBrawlingStyleData*	GetBrawlingData( u32 uStringHash );

#if __BANK
	static void	AddWidgets(bkBank& bank);
	static void	LoadCallback();
	static void	SaveCallback();
#endif

private:

	static void	Reset();
	static void	Load(const char* fileName);
	void		PostLoad();

	atArray<CBrawlingStyleData>		m_aBrawlingData;
	static CBrawlingStyleManager	sm_Instance;

	PAR_SIMPLE_PARSABLE;
};

#endif // INC_COMBAT_BRAWLING_STYLE_H
