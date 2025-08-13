// 
// audio/vehicleconductorfakescenes.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_VEHICLECONDUCTORFAKESCENES_H
#define AUD_VEHICLECONDUCTORFAKESCENES_H

#include "audioengine/entity.h"
#include "audioengine/soundset.h"

#include "audioentity.h"
#include "dynamicmixer.h"
#include "fwsys/fsm.h"
#include "scene/RegdRefTypes.h"


class audVehicleConductorFakeScenes : public fwFsm,naAudioEntity
{
public:
	// FSM States
	enum 
	{
		State_Idle  = 0,
	};
	AUDIO_ENTITY_NAME(audVehicleConductorFakeScenes);

	void					Init();
	void					ProcessUpdate();
	void					Reset();
	void					PlayFakeDistanceSirens();
	void					StopFakeDistanceSirens();
	void					ResetFakeSirenPosApply() { m_DistantSirenPosApply = 0.f;};
	BANK_ONLY(static void	AddWidgets(bkBank &bank););

private:
	virtual fwFsm::Return	UpdateState(const s32 state, const s32 iMessage, const fwFsm::Event event);
	void					GetFakeSirenPosition(Vec3V_InOut sirenPosition);

	// Handlers
	fwFsm::Return			Idle();

	Vec3V					m_DistantSirenPos;

	audCurve				m_OWOToFakeSirnVol;
	audSimpleSmoother		m_FakeDistSirenVolSmoother;
	f32						m_FakeDistSirenLinVol;
	f32						m_DistantSirenPosApply;
	//Helpers
	static f32				sm_DistanceFakedSirens;
	static f32				sm_FakeSirenDiscWidth;
	static f32				sm_MinUpCountryDistThresholdForFakedSirens;
	static f32				sm_MaxUpCountryDistThresholdForFakedSirens;
	static f32				sm_MinUpCityDistThresholdForFakedSirens;
	static f32				sm_MaxUpCityDistThresholdForFakedSirens;
	static f32				sm_TimeToMoveDistantSiren;
#if __BANK
	void					VehicleConductorFSDebugInfo();
	const char*				GetStateName(s32 iState) const ;
	static bool				sm_ShowDistantSirenInfo;
#endif
	audSound				*m_DistanceSiren;
};

#endif