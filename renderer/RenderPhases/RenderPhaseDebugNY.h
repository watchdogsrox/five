//
// filename:	RenderPhaseDebugNY.h
// description:	List of standard renderphase classes
//

#ifndef INC_RENDERPHASEDEBUGNY_H_
#define INC_RENDERPHASEDEBUGNY_H_

// --- Include Files ------------------------------------------------------------
#include "renderer/RenderPhases/renderphase.h"

#if DEBUG_DISPLAY_BAR

// C headers
// Rage headers
// Game headers


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CRenderPhaseDebug2d : public CRenderPhase
{
public:
	CRenderPhaseDebug2d(CViewport* pGameViewport);

	virtual void UpdateViewport();
	virtual void BuildDrawList();

	static void StaticRender(u32 numBatchers);
	
private:
	static grcDepthStencilStateHandle sm_depthState;
	static grcRasterizerStateHandle sm_rasterizerState;
	static grcBlendStateHandle sm_blendState;
	
};

class CRenderPhaseDebug3d : public CRenderPhase
{
public:
	CRenderPhaseDebug3d(CViewport* pGameViewport);

	virtual void BuildDrawList();

	static void StaticRender(u32 numBatchers);
private:
	static grcDepthStencilStateHandle sm_depthState;
	static grcRasterizerStateHandle sm_rasterizerState;
	static grcBlendStateHandle sm_blendState;
};

class CRenderPhaseScreenShot : public CRenderPhase
{
public:
	CRenderPhaseScreenShot(CViewport* pGameViewport);

	virtual void BuildDrawList();
};

namespace CRenderPhaseDebug
{
	void InitDLCCommands();
} // namespace CRenderPhaseDebug

#endif //DEBUG_DISPLAY_BAR

// --- Globals ------------------------------------------------------------------

#endif // !INC_RENDERPHASEDEBUGNY_H_
