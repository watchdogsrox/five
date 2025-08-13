//
// filename:	RenderPhaseStd.h
// description:	List of standard renderphase classes
//

#ifndef INC_RENDERPHASESTD_H_
#define INC_RENDERPHASESTD_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "renderer/sprite2d.h"
#include "RenderPhase.h"

// --- Defines ------------------------------------------------------------------
 
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
class  CPortalVisTracker;
struct NamedRenderTargetBufferedData;
struct MovieMeshRenderTargetBufferedData;

enum eRenderAlphaState
{
	RENDER_ALPHA_CURRENT_INTERIOR_ONLY = BIT(0),
	RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR = BIT(1),
	RENDER_VEHICLE_ALPHA_INTERIOR_ONLY = BIT(2),
	RENDER_VEHICLE_ALPHA_EXCLUDE_INTERIOR = BIT(3),
	RENDER_ALPHA_ALL = RENDER_ALPHA_CURRENT_INTERIOR_ONLY | RENDER_ALPHA_EXCLUDE_CURRENT_INTERIOR | RENDER_VEHICLE_ALPHA_INTERIOR_ONLY | RENDER_VEHICLE_ALPHA_EXCLUDE_INTERIOR
};

class CRenderPhasePreRenderViewport : public CRenderPhase
{
public:
	CRenderPhasePreRenderViewport(CViewport *pViewport);

	virtual void BuildDrawList();
	virtual void UpdateViewport()  {};
};

class CRenderPhaseTimeBars : public CRenderPhase
{
public:
	CRenderPhaseTimeBars(CViewport *pViewport);

	virtual void BuildDrawList();

	static void StaticRenderCall(void);
};



class CRenderPhaseHud : public CRenderPhase
{
public:
	CRenderPhaseHud(CViewport *pViewport);

	virtual void BuildDrawList();

private:
	void AddItemsToDrawList();
};

class CRenderPhaseScript2d : public CRenderPhase
{
public:
	CRenderPhaseScript2d(CViewport *pViewport);

	virtual void BuildDrawList();
	virtual void UpdateViewport()  {};
	
private:
	bool RenderScriptedRT();
	static void RenderScriptedRTCallBack(unsigned count, const NamedRenderTargetBufferedData *rts);
	
	void MovieMeshBlitToRenderTarget();
	static void MovieMeshBlitToRenderTargetCallback(unsigned count, const MovieMeshRenderTargetBufferedData* pRts);


};


class CRenderPhasePhoneScreen : public CRenderPhase
{
public:
	CRenderPhasePhoneScreen(CViewport *pViewport);

	virtual void BuildDrawList();
	virtual void UpdateViewport();

#if GTA_REPLAY
	void EnableForReplay() { Enable(); m_bInReplay = true; }
#endif

private:
	grcBlendStateHandle m_alphaClearBS;
#if GTA_REPLAY
	bool m_bInReplay;
#endif
#if SUPPORT_MULTI_MONITOR
	u32 m_uLastDeviceSyncTimestamp;
#endif
};

class CRenderPhaseDrawScene : public CRenderPhaseScanned
{
public:

	CRenderPhaseDrawScene(CViewport *pViewport, CRenderPhase* pGBufferPhase);

	virtual ~CRenderPhaseDrawScene();

	virtual void BuildRenderList();

	virtual void BuildDrawList();

	virtual void UpdateViewport();

#if __BANK
	virtual void AddWidgets(bkGroup& group);
#endif // __BANK

	void SetGBufferRenderPhase(CRenderPhase* pPhase)					{ m_pGBufferPhase = pPhase; }
	void SetWaterSurfaceRenderPhase(CRenderPhase* pPhase)				{ m_pWaterSurfacePhase = pPhase; }

	void BuildDrawListAlpha(int numEntityPasses = 1);

	u32 GetRenderAlphaState()											{ return m_renderAlphaState; }
	fwInteriorLocation GetCameraInteriorLocation()						{ return m_cameraInteriorLocation; }	

private:

	static void LockAlphaRenderTarget();
	static void UnLockAlphaRenderTarget(bool finalize);

	void SetupDrawListAlphaState(bool forForcedAlpha = false);
	void CleanupDrawListAlphaState();

	u32 m_renderAlphaState;
	bool m_lateVehicleInteriorAlphaState;
	fwInteriorLocation m_cameraInteriorLocation;
	CRenderPhase*	m_pGBufferPhase;

	CRenderPhase*	m_pWaterSurfacePhase;

};


class CRenderPhaseDrawSceneInterface
{
public:
	static const grcViewport* GetViewport();
	static CRenderPhaseDrawScene* GetRenderPhase();
		
	static bool			GetLateVehicleInterAlphaState_Render();				
	static void			SetLateInteriorStateDC(fwInteriorLocation cameralocation, u32 renderAlphaState);
	static void			SetLateVehicleInteriorAlphaStateDC(bool value);

	static void			Static_BlitRT(grcRenderTarget* dst, grcRenderTarget* src, CSprite2d::CustomShaderType type);
	static void			Static_PostFxDepthPass();

	static void			Init(unsigned initMode);
	static void			InitDLCCommands();

	static bool			RenderAlphaEntityInteriorLocationCheck(fwInteriorLocation interiorLocation);
	static bool			RenderAlphaEntityInteriorHashCheck(u64 interiorHash);
	static bool			RenderAlphaInteriorCheck(fwInteriorLocation interiorLocation, bool bIsVehicleAlphaInterior);
	static bool			AlphaEntityInteriorCheck(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, float sortVal, fwEntity *entity);
	static bool			AlphaEntityHybridCheck	(fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, float sortVal, fwEntity *entity);
	
	static bool			IsRenderLateVehicleAlpha();

private:
	static bool			RenderAlphaEntityInteriorLocationCheck_Common(bool bIsInSameInteriorAsCam);


};




// --- Globals ------------------------------------------------------------------


#endif // !INC_RENDERPHASESTD_H_
