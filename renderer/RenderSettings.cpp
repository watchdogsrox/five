//**********************************
// RenderSettings.cpp
// (C) Rockstar North Ltd.
// Started - 11/2005 - Derek Ward
//**********************************
// History
// Created 11/2005 - Derek Ward
//**********************************

#if !__SPU
#include "Renderer/RenderSettings.h"

// C++ hdrs
#include "stdio.h"
#include "string.h"

// Rage hdrs
// Core hdrs
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/RenderPhases/renderPhase.h" // CIRCULAR REFERENCE
#include "renderer/renderThread.h"
#include "system/system.h"

#else // !__SPU
#include "RenderSettings.h"
#endif

// GTA hdrs

//------------------------------------------------------------------
//
void CRenderSettings::Reset()
{
	s32 flags = 0xffffffff;	// don't update LOD values by default (only scene render)
	SetFlags(flags);
	SetLODScale(1.0f);
}

#if __DEV && !__SPU
//-------------------------------------------------------
// Get a string representing the meaning of this setting
// @RENDERSETTING_UPDATE_TOKEN
const char* CRenderSettings::GetSettingsFlagString(s32 bitIdx)
{
	switch (bitIdx)
	{
		case 0:		return	"RENDER_PARTICLES";
		case 1:		return	"RENDER_WATER";
		case 2:		return	"STORE_WATER_OCCLUDERS";
		case 3:		return	"RENDER_ALPHA_POLYS";
		case 4:		return	"RENDER_SHADOWS";
		case 5:		return	"RENDER_DECAL_POLYS";
		case 6:		return	"RENDER_CUTOUT_POLYS";
		case 7:		return	"RENDER_UI_EFFECTS";
		case 8:		return	"ALLOW_PRERENDER";
		case 9:		return	"TONE_MAPPED_EFFECTS";
		case 10:	return  "RENDER_LATEVEHICLEALPHA";
		case 11:	return  "NOT_USED3";
		case 12:	return  "NOT_USED4";
		case 13:	return  "NOT_USED5";
		case 14:	return  "NOT_USED6";
		case 15:	return  "NOT_USED7";
		case 16:	return	"NOT_USED8";
		case 17:	return  "NOT_USED9";
		case 18:	return  "NOT_USED10";
		case 19:	return  "NOT_USED11";
		case 20:	return  "NOT_USED12";
		case 21:	return  "NOT_USED13";
		case 22:	return  "NOT_USED14";
		case 23:	return  "NOT_USED15";
		case 24:	return  "NOT_USED16";
		case 25:	return  "NOT_USED17";
		case 26:	return  "NOT_USED18";
		case 27:	return  "NOT_USED19";
		case 28:	return  "NOT_USED20";
		case 29:	return  "NOT_USED21";
		case 30:	return  "NOT_USED22";
		case 31:	return  "NOT_USED23";
		case 32:	
		default:	return	NULL;
	}
}
#endif // __DEV && !__SPU
