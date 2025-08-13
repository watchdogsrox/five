// 
// audio/gunfightconductor.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_GUNFIGHTCONDUCTOR_H
#define AUD_GUNFIGHTCONDUCTOR_H


#include "conductorsutil.h"
#include "gunfightconductordynamicmixing.h"
#include "gunfightconductorfakescenes.h"
#include "scene/RegdRefTypes.h"

#include "fwsys/fsm.h"

#define AUD_MAX_NUM_PEDS_IN_COMBAT 64
class audGunFightConductor : public fwFsm
{
private:
	struct combatPedData{
		RegdPed pPed;
		f32 fitness;
	};
	typedef atRangeArray<combatPedData,MaxNumConductorsTargets> Targets;
	typedef atRangeArray<combatPedData,AUD_MAX_NUM_PEDS_IN_COMBAT> PedsInCombat;

public: 

	void						Init();
	void						ProcessMessage(const ConductorMessageData &messageData);
	void						ProcessSuperConductorOrder(ConductorsOrders order);
	void						ProcessUpdate();
	void						Reset();
	bool						ComputeAndValidatePeds();
	bool						UseQSSoftVersion(){return m_UseQSSoftVersion;};
	audGunFightConductorDynamicMixing  &GetDynamicMixingConductor()
	{
		return m_GunFightConductorDM;
	}
	audGunFightConductorFakeScenes  &GetFakeScenesConductor()
	{
		return m_GunFightConductorFS;
	}
	CPed*						GetTarget(ConductorsTargets target) const;
	CPed*						GetPedInCombat(s32 pedIndex) const;
	bool						IsTarget(const CPed *pPed);
	u32							GetNumberOfPedsInCombat() {return m_NumberOfPedsInCombat;}
	u32							GetNumberOfTargets(){return m_NumberOfTargets;}
	ConductorsInfo				UpdateInfo();
	EmphasizeIntensity			GetIntensity();
 
#if __BANK
	static void					AddWidgets(bkBank &bank);
	static bool					GetShowInfo() {return sm_ShowInfo;}
#endif

private:
	virtual fwFsm::Return		UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);
	//Handlers
	fwFsm::Return 				Idle();
	fwFsm::Return 				EmphasizeLowIntensity();
	fwFsm::Return 				EmphasizeMediumIntensity();
	fwFsm::Return 				EmphasizeHighIntensity();

	//Helpers
	void 						ComputeGunFightTargets();
	BANK_ONLY(void 				ComputeGunFightTargetsDebug(););
	void 						ComputeNearTargets(const s32 pedIndex,s32 &nearPrimaryIndex,s32 &nearSecondaryIndex,f32 &distanceToPrimaryTarget);
	void 						ComputeFarTargets(const s32 pedIndex,s32 nearPrimaryIndex,s32 nearSecondaryIndex, f32 distanceToPrimaryTarget);
	void						ForceTarget(const CPed *pPed);
	void 						UpdatePedsInCombat();
	BANK_ONLY(void 				UpdateGunFightTargetDebug(););
	void						UpdateTargets();

	f32							ComputeActionFitness(CPed *pPed);
	f32							ComputeFitness(CPed *pPlayer,CPed *pPed);
#if __BANK
	const char*					GetStateName(s32 iState) const ;
	void						GunfightConductorDebugInfo();
#endif

	audGunFightConductorDynamicMixing	m_GunFightConductorDM;
	audGunFightConductorFakeScenes		m_GunFightConductorFS;

	// Gun fights curves
	audCurve							m_CombatMovementToFitness;
	audCurve							m_CombatAbilityToFitness;
	audCurve							m_CombatRangeToFitness;
	audCurve							m_PositionToFitness;
	audCurve							m_DistanceToFitness;
	audCurve							m_DamageTypeToFitness;
	audCurve							m_EffectGroupToFitness;
	audCurve							m_ExplosionTagToFitness;
	audCurve							m_FireTypeToFitness;
	audCurve 							m_DistanceAffectsWeaponType;
	audCurve 							m_NumberOfPedsToConfidence;

	PedsInCombat						m_PedsInCombat;  
	Targets								m_CombatTargetPeds;  
	ConductorsOrders					m_LastOrder;

	bool								m_UseQSSoftVersion;
	bool								m_NeedToRecalculate;
	bool								m_HasToTriggerPlayerIntoCover;
	bool								m_HasToTriggerPlayerExitCover;
	u32									m_NumberOfPedsInCombat;
	u32									m_NumberOfTargets;
	static f32							sm_DistanceThreshold;
#if __BANK
	static CPed 						*sm_LastFocusedEntity;
	static bool 						sm_DebugMode;
	static bool 						sm_ShowTargetPeds;
	static bool 						sm_ShowPedsInCombat;
	static bool 						sm_ShowGunfightVolOffset;
	static bool 						sm_ShowInfo;
	static bool 						sm_ForceLowIntensityScene;
	static bool 						sm_ForceMediumIntensityScene;
	static bool 						sm_ForceHighIntensityScene;
#endif
};
#endif // AUD_GUNFIGHTCONDUCTOR_H
