// 
// Clouds.h
// 
// Copyright (C) 1999-2008 Rockstar Games.  All Rights Reserved. 
// 

#ifndef CLOUDS_H
#define CLOUDS_H

// Game includes
#include "Vfx/clouds/CloudHatSettings.h"
#include "vfx/VisualEffects.h"

// Rage includes
#include "atl/hashstring.h"


//Simplified interface class for CloudHat functionality.
class CClouds
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdownMode);

	static void Update();
	static void Render(CVisualEffects::eRenderMode mode, int scissorX, int scissorY, int scissorW, int scissorH, float cloudAlpha);
	static void Synchronise();

	static void OverrideCloudSettingName(const char * settingName); // if non NULL, force this cloud setting name to be used, ignoring the current weather value 	
	static void RequestCloudTransition(bool instant=false, bool assumedMaxLength=false);
	static int  GetNumTimeSamples();
	static float GetTimeSampleTime(int index);
	static int   GetUpdateBuffer() {return m_UpdateBufferIndex;}
	static const CloudFrame & GetCurrentRenderCloudFrame() {return m_CurrentFrame[1-m_UpdateBufferIndex];}
#if __BANK
	static void InitWidgets();
	static void UpdateWidgets();

	//Copy Paste Settings:
	static void CopyDebugSettingsFromKeyframes(float time, const atHashString &name);
	static void PasteDebugSettingsInToKeyframes(float time, const atHashString &name);
	static void CopyTimeCycleColorsToDebugSettings(float time);
#endif

	static float GetCloudAlpha() { return m_CloudAlpha; }
	static void SetCloudAlpha(float f) { m_CloudAlpha = f; }

private:
	static CloudFrame	m_CurrentFrame[2];	
	static int			m_UpdateBufferIndex;
	static float		m_CloudAlpha;
};

#endif //CLOUDS_H 
