//
// filename:	RenderPhaseEntitySelect.h
// description: renderphase classes
//

#ifndef INC_RENDERPHASEENTITYSELECT_H_
#define INC_RENDERPHASEENTITYSELECT_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "RenderPhase.h"

// --- Defines ------------------------------------------------------------------
#define ENTITYSELECT_ENABLED_BUILD (__BANK)

#if ENTITYSELECT_ENABLED_BUILD
#	define ENTITYSELECT_ONLY(expr) expr
#else
#	define ENTITYSELECT_ONLY(expr)
#endif

#if ENTITYSELECT_ENABLED_BUILD
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// name:		CRenderPhaseEntitySelect
// description:	Debug only render pass that renders entity IDs to render target used to select the exact entity selected by the cursor.
class CRenderPhaseEntitySelect : public CRenderPhaseScanned
{
public:
	CRenderPhaseEntitySelect(CViewport* pGameViewport);
	~CRenderPhaseEntitySelect();

	virtual void BuildRenderList();
	
	virtual void BuildDrawList();

	virtual void UpdateViewport();

	virtual u32 GetCullFlags() const;

	virtual void AddWidgets(bkGroup & group);
	
	//Custom Interface:
	static void InitializeRenderingResources();
	static void ReleaseRenderingResources();

	//For Debug Drawing. Please call in appropriate render phase.
	static void BuildDebugDrawList();

	static bool GetIsBuildingDrawList() {return sm_inBuildDrawList;}

#if __D3D11 || RSG_ORBIS
private:
	bool m_wasActiveLastFrame;
#endif // __D3D11 || RSG_ORBIS
	static bool sm_inBuildDrawList;
};

#endif //ENTITYSELECT_ENABLED_BUILD

#endif // !INC_RENDERPHASEENTITYSELECT_H_
