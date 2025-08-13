//
// camera/debug/ShakeDebugger.h
// 
// Copyright (C) 1999-2020 Rockstar Games.  All Rights Reserved.
//
#ifndef SHAKE_DEBUGGER_H
#define SHAKE_DEBUGGER_H

#if __BANK
#include "atl/array.h"
#include "atl/hashstring.h"
#include "scene/RegdRefTypes.h"

class camMetadataStore;

class camShakeDebugger
{
public:
	camShakeDebugger();
	~camShakeDebugger();

	void Initialise(const camMetadataStore& metadataStore);
	void Shutdown();
	void AddWidgets(bkBank& bank);
	void OnShakeSelected();
	void OnStartShake();
	void OnStopShake();
	void OnAmplitudeChanged();
	void OnVehiclePartBrokenShake();

private:
	atArray<const char*> m_ShakesNamesRaw;
	RegdCamBaseCamera m_BaseCamera;
	RegdCamBaseFrameShaker m_FrameShaker;
	bkGroup* m_ShakeDebuggerWidget;
	bkGroup* m_SelectedShakeWidget;
	int m_SelectedShake;
	int m_SelectedVehiclePart;
	float m_Amplitude;
};
#endif //__BANK

#endif //SHAKE_DEBUGGER_H