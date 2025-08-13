
//**********************************
// RenderSettings.h
// (C) Rockstar North Ltd.
// description:	Hi level renderer - abstracts rendering into a platform independent set of phases.
// Started - 11/2005 - Derek Ward
//**********************************
// History
// Created 11/2005 - Derek Ward
//**********************************

#ifndef INC_RENDERSETTINGS_H_
#define INC_RENDERSETTINGS_H_

#include "fwrenderer/RenderSettingsBase.h"

//============================================================================
// Bit flags for m_flags member of CRenderSettings.
// SEARCH CODE FOR @RENDERSETTING_UPDATE_TOKEN when updating this list and make appropriate changes.
enum eRenderSettingFlags
{
	RENDER_SETTING_RENDER_PARTICLES				= (1<< 0),
	RENDER_SETTING_RENDER_WATER					= (1<< 1),
	RENDER_SETTING_STORE_WATER_OCCLUDERS		= (1<< 2),
	RENDER_SETTING_RENDER_ALPHA_POLYS			= (1<< 3),
	RENDER_SETTING_RENDER_SHADOWS				= (1<< 4),
	RENDER_SETTING_RENDER_DECAL_POLYS			= (1<< 5),
	RENDER_SETTING_RENDER_CUTOUT_POLYS			= (1<< 6),
	RENDER_SETTING_RENDER_UI_EFFECTS			= (1<< 7),
	RENDER_SETTING_ALLOW_PRERENDER				= (1<< 8),
	RENDER_SETTING_RENDER_DISTANT_LIGHTS		= (1<< 9),
	RENDER_SETTING_RENDER_DISTANT_LOD_LIGHTS	= (1<<10),
	RENDER_SETTING_NOT_USED11					= (1<<11),
	RENDER_SETTING_NOT_USED12					= (1<<12),
	RENDER_SETTING_NOT_USED13					= (1<<13),
	RENDER_SETTING_NOT_USED14					= (1<<14),
	RENDER_SETTING_NOT_USED15					= (1<<15),
	RENDER_SETTING_NOT_USED16					= (1<<16),
	RENDER_SETTING_NOT_USED17					= (1<<17),
	RENDER_SETTING_NOT_USED18					= (1<<18),
	RENDER_SETTING_NOT_USED19					= (1<<19),
	RENDER_SETTING_NOT_USED20					= (1<<20),
	RENDER_SETTING_NOT_USED21					= (1<<21),
	RENDER_SETTING_NOT_USED22					= (1<<22),
	RENDER_SETTING_NOT_USED23					= (1<<23),
	RENDER_SETTING_NOT_USED24					= (1<<24),
	RENDER_SETTING_NOT_USED25					= (1<<25),
	RENDER_SETTING_NOT_USED26					= (1<<26),
	RENDER_SETTING_NOT_USED27					= (1<<27),
	RENDER_SETTING_NOT_USED28					= (1<<28),
	RENDER_SETTING_NOT_USED29					= (1<<29),
	RENDER_SETTING_NOT_USED30					= (1<<30),
	RENDER_SETTING_NOT_USED31					= (1<<31),
	RENDER_SETTINGS_MAX							=     32
};
//-------------------------------------------------------------
// Settings describing what/how is desired to be rendered
// Rendersettings presently operate on a multi tier basis where 
// CViewportManager settings can override CViewport settings
// which in turn can override RenderPhase settings.
class CRenderSettings : public CRenderSettingsBase
{
public:
	CRenderSettings() { Reset(); }
	CRenderSettings(CRenderSettingsBase base) : CRenderSettingsBase(base) {}
	 ~CRenderSettings() {}
	
	void Reset();

#if __DEV && !__SPU
	static const char *GetSettingsFlagString(s32 i);
#endif // __DEV && !__SPU
};

#endif //INC_RENDERPHASE_H_

