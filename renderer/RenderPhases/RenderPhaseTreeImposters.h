//
// filename:	RenderPhaseTreeImposters.h
// description: renderphase classes
//

#ifndef INC_RENDERPHASETREEIMPOSTER_H_
#define INC_RENDERPHASETREEIMPOSTER_H_

#include "renderer/TreeImposters.h"

#if USE_TREE_IMPOSTERS

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "RenderPhase.h"
// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
//
// name:		CRenderPhaseTreeImpostersReflection
// description:	TreeImposters Reflection render phase
class CRenderPhaseTreeImposters : public CRenderPhase
{
public:
	CRenderPhaseTreeImposters(CViewport* pViewport);

	virtual void UpdateViewport();

	virtual void BuildDrawList();


	static void InitDLCCommands();
};

#endif // USE_TREE_IMPOSTERS

// --- Globals ------------------------------------------------------------------

#endif // !INC_RENDERPHASETREEIMPOSTER_H_


