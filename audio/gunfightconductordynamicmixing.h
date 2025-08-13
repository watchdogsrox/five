// 
// audio/gunfightconductorfakescenes.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_GUNFIGHTCONDUCTORDYNAMICMIXING_H
#define AUD_GUNFIGHTCONDUCTORDYNAMICMIXING_H

#include "fwsys/fsm.h"

#include "conductorsutil.h"
#include "dynamicmixer.h"
#include "scene/RegdRefTypes.h"

class audGunFightConductorDynamicMixing : public fwFsm
{
public:
	void						Init();
	void						ProcessUpdate();
	void						Reset();

	void						DeemphasizeBulletImpacts();
	void						EmphasizeBulletImpacts();

	BANK_ONLY(static void		AddWidgets(bkBank &bank);)
private:
	virtual fwFsm::Return		UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);
	// Handlers
	fwFsm::Return				Idle();
	fwFsm::Return				ProcessLowIntensityDM();
	fwFsm::Return				ProcessMediumIntensityDM();
	fwFsm::Return				ProcessHighIntensityDM();

	//Helpers
	void						StartGunfightScene();
	void						StopGunfightScene();
	void						UpdateGunfightMixing(f32 volumeOffset, f32 targetVolOffset);
#if __BANK
	void						GunfightConductorDMDebugInfo();
	const char*					GetStateName(s32 iState) const ;
#endif
	audScene *					m_GunfightScene;
	audScene *					m_BulletImpactsScene;

	audSimpleSmoother			m_GunfightApplySmoother;
	static f32					sm_NonTargetsLowVolOffset;
	static f32					sm_NonTargetsMedVolOffset;
	static f32					sm_NonTargetsHighVolOffset;
	static f32					sm_TargetsLowVolOffset;
	static f32					sm_TargetsMedVolOffset;
	static f32					sm_TargetsHighVolOffset;
};
#endif
