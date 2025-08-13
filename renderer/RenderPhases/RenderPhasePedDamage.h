//
// filename:	RenderPhasePedDamage.h
// description: renderphase classes
//

#ifndef INC_RENDERPHASEPEDDAMAGE_H_
#define INC_RENDERPHASEPEDDAMAGE_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "RenderPhase.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CRenderPhasePedDamageUpdate : public CRenderPhase
{
public:
	CRenderPhasePedDamageUpdate(CViewport *pViewport);

	virtual void BuildRenderList();

	virtual void BuildDrawList();

	virtual void UpdateViewport();

private:
};

#endif // !INC_RENDERPHASEPEDDAMAGE_H_
