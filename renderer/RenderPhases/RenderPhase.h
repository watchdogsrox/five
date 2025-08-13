
//**********************************
// RenderPhase.h
// (C) Rockstar North Ltd.
// description: Hi level renderer - abstracts rendering into a platform independent set of phases.
// Started - 11/2005 - Derek Ward
//**********************************
// History
// Created 11/2005 - Derek Ward
//**********************************

#ifndef INC_RENDERPHASE_H_
#define INC_RENDERPHASE_H_

#if !defined(PPUVIRTUAL)
 #if __SPU
 #define PPUVIRTUAL
#else // __SPU
 #define PPUVIRTUAL virtual
#endif // __SPU
#endif
// Rage hdrs
#include "grcore/viewport.h"

#include "fwrenderer/renderPhaseBase.h"

// Game hdrs
#include "scene/world/VisibilityMasks.h"
#include "Renderer/clip_stat.h"
#include "Renderer/RenderSettings.h"
#include "Renderer/Shadows/ShadowTypes.h"

#if !__SPU
#include "Renderer/DrawLists/drawList.h"
#endif

// GTA hdrs

// Frefs
class CViewport;
class CRenderPhase;
class CPortalVisTracker;

namespace rage {
class fwRenderPhaseCullShape;
class fwPvsEntry;
}

#define	MAX_PHASE_FLAGS	24

class CRenderPhase : public fwRenderPhase
{
public:
	CRenderPhase(CViewport *viewport, const char *name, int drawListType, float updateBudget, bool disableOnPause);
#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
	CRenderPhase(CViewport *viewport, const char *name, int drawListType, float updateBudget, bool disableOnPause, int startDrawListType, int endDrawListType);
#endif // RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS

public:
	virtual  u32 GetCullFlags() const				{ return 0; }

	virtual bool IsScanRenderPhase() const			{ return false; }

	CViewport *GetViewport() const					{ return m_Viewport; }

	virtual void UpdateViewport();

	bool GetDisableOnPause() const { return m_disableOnPause; }
	
protected:

	bool m_disableOnPause;
	
	CViewport *m_Viewport;
};

class CRenderPhaseScanned : public CRenderPhase
{
public:
	CRenderPhaseScanned(CViewport *viewport, const char *name, int drawListType, float updateBudget);
#if RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS
	CRenderPhaseScanned(CViewport *viewport, const char *name, int masterDrawListType, float updateBudget, int startDrawListType, int endDrawListType);
#endif //RENDER_PHASE_SUPPORT_MULIPLE_DRAW_LISTS

	virtual bool IsScanRenderPhase() const				{ return true; }

	virtual void SetCullShape(fwRenderPhaseCullShape & cullShape);

	u32 GetRenderFlags() const							{ return m_Scanner.GetRenderFlags(); }
	void SetRenderFlags(u32 renderFlags)				{ m_Scanner.SetRenderFlags(renderFlags); }

	virtual fwRenderPhaseScanner *GetScanner()			{ return &m_Scanner; }

	virtual void UpdateViewport();


	//
	// Portal
	//
	CPortalVisTracker* GetPortalVisTracker(void) const	{ return m_pPortalVisTracker; } 
	bool IsPrimaryPortalVisTrackerPhase(void) const		{ return m_isPrimaryTrackerPhase; } 
	bool IsUpdatePortalVisTrackerPhase(void) const		{ return m_isUpdateTrackerPhase; } 

	const Vector3& GetCameraPositionForFade() const		{ return m_Scanner.GetCameraPositionForFade(); } 
	const Vector3& GetCameraDirection() const			{ return m_Scanner.GetCameraDirection(); } 

	virtual void ConstructRenderList();

	void SetCameraForFadeCalcs(const Vector3 &pos,const Vector3 &dir,float farclip) { m_Scanner.SetCameraForFadeCalcs(pos, dir, farclip); } 

protected:

	const grcViewport* GetLodViewport() const;

	fwRenderPhaseScanner m_Scanner;

	CPortalVisTracker *m_pPortalVisTracker;

	bool m_isPrimaryTrackerPhase;
	bool m_isUpdateTrackerPhase;
};

#if __PPU
class CBubbleFences
{
public:
	typedef enum {
		BFENCE_GBUFFER,			// waits for lighting_end
		BFENCE_SHADOW,			// waits for alpha
		BFENCE_WATERREFLECT,	// waits for gbuffer
		BFENCE_REFLECT,			// waits for shadows
		BFENCE_LIGHTING_START,	
		BFENCE_LIGHTING_END,
		BFENCE_ALPHA,			// waits for lighting_start
		BFENCE_COUNT
	} kBubbleFence;

	static void InsertBubbleFenceDLC(kBubbleFence fence);
	static void WaitForBubbleFenceDLC(kBubbleFence fence);
};
#endif // __PPU

#endif //INC_RENDERPHASE_H_

