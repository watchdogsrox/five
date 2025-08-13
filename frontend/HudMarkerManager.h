/////////////////////////////////////////////////////////////////////////////////
//
// FILE    : HudMarkerManager.h
// PURPOSE : Manages 2D world markers (instancing, layout, etc)
// AUTHOR  : Aaron Baumbach (Ruffian)
// STARTED : 06/02/2020
//
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_HUD_MARKER_MANAGER_H_
#define INC_HUD_MARKER_MANAGER_H_

#include "frontend/HudMarkerTypes.h"

#include "atl/array.h"
#include "fwmaths/Rect.h"
#include "grcore/viewport.h"

class CPed;
class CHudMarkerRenderer;

class CHudMarkerManager
{
public:

	inline static CHudMarkerManager* Get() { return sm_instance; }

	static void Init(unsigned Mode);
	static void Shutdown(unsigned Mode);
	static void DeviceLost();
	static void DeviceReset();
	static void InitWidgets();
	static void ShutdownWidgets();

	bool IsInitialised() const;

	bool GetIsEnabled() { return m_isEnabled; }
	void SetIsEnabled(bool IsEnabled);

	bool ShouldRender() const;

	void SetHideThisFrame() { m_hideThisFrame = true; }

	void UpdateMovieConfig();

	void Update();
	void AddDrawListCommands();

	bool IsMarkerValid(HudMarkerID ID) const;

	HudMarkerID CreateMarker(const SHudMarkerState& InitState);
	HudMarkerID CreateMarkerForBlip(s32 BlipID, SHudMarkerState* outState = nullptr);

	const SHudMarkerState* GetMarkerState(HudMarkerID ID) const;
	SHudMarkerState* GetMarkerState(HudMarkerID ID);

	const SHudMarkerGroup* GetMarkerGroup(HudMarkerGroupID ID) const;
	SHudMarkerGroup* GetMarkerGroup(HudMarkerGroupID ID);

	bool RemoveMarker(HudMarkerID& ID);

	const HudMarkerStateArray& GetMarkerStateArray() const { return m_markerStates; }
	const HudMarkerIDArray& GetActiveIDArray() const { return m_activeIds; }

	CHudMarkerRenderer& GetRenderer() const { return *m_renderer; }

private:

	CHudMarkerManager();
	~CHudMarkerManager();

	void Init_Internal(unsigned Mode);
	void Shutdown_Internal(unsigned Mode);
	void DeviceReset_Internal();

	void RemoveMarker_Internal(int ActiveIndex);

	bool UpdateMarker(SHudMarkerState& State, SHudMarkerGroup* Group, const fwRect& ClampBoundary, CPed* FollowPed, const grcViewport& Viewport, bool CanPerformOcclusionTest) const;
	bool UpdateMarkerFromBlip(s32 BlipID, SHudMarkerState& State) const;

	void UpdateWaypoint();

	fwRect CalculateClampBoundary() const;
	fwRect CalculateMinimapArea() const;

	//Projects State.WorldPosition to HUD(0-1), clamping to boundary if required
	//Results written into State
	void ProjectMarker(SHudMarkerState& State, bool ShouldClamp, const fwRect& ClampBoundary, const grcViewport& Viewport) const;

private:

	static CHudMarkerManager* sm_instance;

	bool m_isEnabled;

	CHudMarkerRenderer* m_renderer;
	bool m_hideThisFrame;
	bool m_didRenderLastFrame;

	HudMarkerStateArray m_markerStates;
	HudMarkerIDArray m_activeIds;
	HudMarkerIDArray m_freeIds;

	HudMarkerGroupArray m_markerGroups;

	atMap<s32, HudMarkerID> m_blipMarkerMap;

	atArray<HudMarkerID> m_markersWithDisplayText;

	int m_clampMode;
	float m_clampInsetX;
	float m_clampInsetY;
	float m_clampCloseProximityMinDistance;
	float m_clampCloseProximityMaxDistance;
	float m_clampCloseProximityInset;

	float m_markerScreenHalfSize; //Used for screen space culling

	u8 m_fadeOverDistanceAlpha;
	float m_fadeOverDistanceMin;
	float m_fadeOverDistanceMax;

	float m_scaleMin;
	float m_scaleMax;
	float m_scaleDistanceMin;
	float m_scaleDistanceMax;

	u8 m_occludedAlpha;
	bool m_hideDistanceTextWhenClamped;

	int m_occlusionTestFrame;
	int m_occlusionTestFrameSkip;

	fwRect m_minimapArea;
	fwRect m_minimapAreaBig;
	
	Vector3 m_cachedPlayerPedPosition;
	float m_cachedAspectRatio;
	float m_cachedAspectMultiplier;
	fwRect m_cachedMinimapArea;

	bool m_isAnyGroupSolo;

	float m_focusRadius;
	HudMarkerID m_focusedMarker;
	u32 m_lastFocusTime;
	bool m_pendingFocusInput;
	u32 m_focusInputPressedTime;
	HudMarkerID m_pendingFocusSelection;

	HudMarkerID m_activeWaypointMarker;
	HudMarkerID m_transientWaypointMarker;

//START_DEBUG===========================================
#if __BANK
public: 
	void CreateBankWidgets();
	void PushDebugMarker();
	void PopDebugMarker();
	void RenderDebug(const fwRect& BoundaryRect);
private:
	//Debug for tracking marker allocations
	u32 m_waterMark;
	u32 m_highWaterMark;

	bool m_renderDebugMarkers;
	bool m_renderDebugText;
	bool m_renderClampBoundary;
	bool m_renderMinimapArea;
	bool m_renderFocusRadius;

	HudMarkerIDArray m_debugMarkers;
	u16 m_pushPopNum;
	u8 m_pushGroup;
	float m_pushRange;

	int m_numActive;
	int m_numDebug;
#endif
//END_DEBUG=============================================
};

#endif //INC_HUD_MARKER_MANAGER_H_