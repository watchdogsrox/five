//
// filename:	RenderPhaseWater.h
// description: renderphase classes
//

#ifndef INC_RENDERPHASEWATER_H_
#define INC_RENDERPHASEWATER_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "RenderPhase.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CRenderPhaseWaterSurface : public CRenderPhase
{
public:
	CRenderPhaseWaterSurface(CViewport* pGameViewport);
	virtual void BuildDrawList();
};

class CRenderPhaseCloudGeneration : public CRenderPhase
{
public:
	CRenderPhaseCloudGeneration(CViewport* pGameViewport);
	virtual void BuildDrawList();
};

class CRenderPhaseRainUpdate : public CRenderPhase
{
public:
	CRenderPhaseRainUpdate(CViewport* pGameViewport);
	virtual void BuildDrawList();
};

#endif // !INC_RENDERPHASEWATER_H_
