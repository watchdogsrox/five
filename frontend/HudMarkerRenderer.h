/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HudMarkerManager.h
// PURPOSE : Manages 2D world markers (instancing, layout, etc)
// AUTHOR  : Aaron Baumbach (Ruffian)
// STARTED : 06/02/2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_HUD_MARKER_RENDERER_H_
#define INC_HUD_MARKER_RENDERER_H_

#include "frontend/HudMarkerTypes.h"
#include "system/criticalsection.h"

#include "frontend/Scaleform/ScaleFormMgr.h"
#include "Scaleform/Include/GFxPlayer.h"

class CHudMarkerRenderer
{
public:

	CHudMarkerRenderer();

	void Init(unsigned Mode);
	void Shutdown(unsigned Mode);
	bool IsInitialised() const;

	void DeviceReset();
	void InvalidateAllMarkers();

	void UpdateMovieConfig();

	void AddDrawListCommands(const HudMarkerIDArray& ActiveIDs, HudMarkerStateArray& StateUT);

private:

	bool InitScaleformUT();
	void ShutdownScaleformUT();

	void SyncStateUT(const HudMarkerIDArray& ActiveIDs, HudMarkerStateArray& StateUT);

	void Render();

	void UpdateRT();

	const char* IconIndexToName(s32 Index) const;

	bool UpdateDistanceMode();
	bool DistanceToText(u16 DistanceMeters, char(&out_Text)[8]) const;

private:

	HudMarkerRenderStateArray m_markerRenderStates;
	HudMarkerIDArray m_activeIds;

	CScaleformMovieWrapper m_movie;
	GFxValue m_asMarkerContainer;
	atFixedArray<GFxValue, MAX_NUM_MARKERS> m_asMarkers;

	int m_maxNewMarkersPerFrame;
	int m_maxVisibleMarkers;
	
	bool m_renderScaleformMarkers;

	HudMarkerDistanceMode m_distanceMode;

//START_DEBUG===========================================
#if __BANK
public:
	void CreateBankWidgets(bkBank *pBank);
private:
	int m_numVisible;
	bool m_hideArrows;
	bool m_refreshMarkers;
	int m_debugDistanceMode;
#endif
//END_DEBUG=============================================

};

#endif //INC_HUD_MARKER_RENDERER_H_