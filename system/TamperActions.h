//
// system/TamperActions.h
//
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//

#ifndef TAMPERACTIONS_H
#define TAMPERACTIONS_H

#define TAMPERACTIONS_SIXSTARWANTEDLEVEL	0
#define TAMPERACTIONS_ROTATEMINIMAP			0
#define TAMPERACTIONS_DISABLEPHONE      	0
#define TAMPERACTIONS_SNOW              	0
#define TAMPERACTIONS_LEAKPETROL        	0
#define TAMPERACTIONS_INVINCIBLECOP     	0
#define TAMPERACTIONS_DMT					0
#define TAMPERACTIONS_SENDTELEMETRY			1
#define TAMPERACTIONS_P2PREPORT				1
#define TAMPERACTIONS_SET_VIDEOCLIP_MOD		1


#define TAMPERACTIONS_ENABLED RSG_PC 

class CPed;

namespace TamperActions {
#if TAMPERACTIONS_ENABLED
	#if __BANK
		void InitWidgets();
	#endif
	void Init(unsigned initMode);

	void Update();
	void PerformSafeModeOperations();

	void OnPlayerSwitched(CPed& oldPed, CPed& newPed);

	// Accessors
	bool IsSixStarWantedLevel();
	bool ShouldRotateMiniMap();
	bool IsPhoneDisabled(int phoneId);
	bool AreCopsInvincible();
	bool IsSnowing();
	bool DoVehiclesLeakPetrol();
	bool IsInvincibleCop(CPed* pPed);
#endif // TAMPERACTIONS_ENABLED

	// Save / load
	void SetTriggeredActionsFromSaveGame(u32 data);
	u32 GetTriggeredActions();
}

#endif // TAMPERACTIONS_H
