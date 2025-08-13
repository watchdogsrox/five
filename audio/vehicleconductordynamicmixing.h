// 
// audio/vehicledynamicmixing.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLECONDUCTORDYNAMICMIXING_H
#define AUD_VEHICLECONDUCTORDYNAMICMIXING_H

#include "dynamicmixer.h"
#include "fwsys/fsm.h"

class audVehicleConductorDynamicMixing  : public fwFsm
{
public:

	void					Init();
	void					ProcessUpdate();
	void					Reset();

	void					StartStuntJumpScene();
	void					StopStuntJumpScene();
	void					StartBadLandingScene();
	void					StopBadLandingScene();
	void					StartGoodLandingScene();
	void					StopGoodLandingScene();
	void					ClearDynamicMixJobForVehicle(CVehicle *pVeh);

	BANK_ONLY(static void	AddWidgets(bkBank &bank);)
private:
	virtual fwFsm::Return	UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);
	// Handlers
	fwFsm::Return			Idle();
	fwFsm::Return			ProcessLowIntensity();
	fwFsm::Return			ProcessMediumIntensity();
	fwFsm::Return			ProcessHighIntensity();

	//Helpers
	void					StartLowIntensityScene();
	void					StartMediumIntensityScene();
	void					StartHighIntensityScene();

	void					StopLowIntensityScene();
	void					StopMediumIntensityScene();
	void					StopHighIntensityScene();

	void					UpdateLowIntensityMixGroup();
	void					UpdateMediumIntensityMixGroup();
	void					UpdateHighIntensityMixGroup();


#if __BANK
	void					VehicleConductorDMDebugInfo();
	const char*				GetStateName(s32 iState) const ;
#endif
	audScene*				m_StuntJumpScene;
	audScene*				m_BadLandingScene;
	audScene*				m_GoodLandingScene;
	audScene*				m_LowIntensityScene;
	audScene *				m_MediumIntensityScene;
	audScene *				m_HighIntensityScene;
};
#endif 
