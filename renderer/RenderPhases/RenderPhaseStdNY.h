//
// filename:	RenderPhaseStdNY.h
// description:	List of standard renderphase classes
//

#ifndef INC_RENDERPHASESTDNY_H_
#define INC_RENDERPHASESTDNY_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "RenderPhase.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------
namespace rage {
class grcViewport;
};

//
// name:		CRenderPhaseFrontEnd
// description:	Draw frontend render phase
class CRenderPhaseFrontEnd : public CRenderPhase
{
public:
	CRenderPhaseFrontEnd(CViewport* pGameViewport);

	virtual void BuildDrawList();

	virtual void UpdateViewport();
};



//
// name:		CRenderPhasePhoneModel
// description:	Draw phone model render phase
class CRenderPhasePhoneModel : public CRenderPhase
{
public:
	CRenderPhasePhoneModel(CViewport* pGameViewport);

	virtual void BuildDrawList();

	static void PhoneAAPrologue(const Vector3& phonePos, float phoneScale, float phoneRadius);
	static void PhoneAAEpilogue();

	static void SetupPhoneViewport(grcViewport* pViewport, Vector3& phonePos);

#if __BANK
	static bool ms_bDrawScissorRect;
#endif

private:

	static int ms_scissorRect[4];
	static int ms_prevScissorRect[4];

};



// --- Globals ------------------------------------------------------------------

#endif // !INC_RENDERPHASESTDNY_H_
