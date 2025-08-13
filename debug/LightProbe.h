// ===========================
// debug/LightProbe.h
// (c) 2013 Rockstar San Diego
// ===========================

#ifndef _DEBUG_LIGHTPROBE_H_
#define _DEBUG_LIGHTPROBE_H_

#include "control/replay/replay.h"

#if __BANK

namespace rage { class grcTexture; }

namespace LightProbe
{
	bool IsEnabled();
	bool IsCapturingPanorama();
	void CreateRenderTargetsAndInitShader();
	void AddWidgets();
	void Capture(grcTexture* reflection);
	void Update();
	void Render();
	float GetSkyHDRMultiplier();

#if GTA_REPLAY
	s32  GetFixedPanoramaTimestepMS();
	void OnFinishedReplayTick();
#endif
}

#endif // __BANK
#endif // _DEBUG_LIGHTPROBE_H_
