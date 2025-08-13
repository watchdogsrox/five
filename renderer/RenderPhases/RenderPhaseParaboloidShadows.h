// ====================================================
// renderer/renderphases/renderphaseparaboloidshadows.h
// (c) 2010 RockstarNorth
// ====================================================

#ifndef INC_RENDERPHASEPARABOLOIDSHADOW_H_
#define INC_RENDERPHASEPARABOLOIDSHADOW_H_

// Game headers
#include "renderer/renderphases/renderphase.h"
class CShadowInfo;

class CRenderPhaseParaboloidShadow : public CRenderPhaseScanned
{
public:
	CRenderPhaseParaboloidShadow(CViewport* pGameViewport, int s);

	virtual void BuildRenderList();
	virtual void BuildDrawList();
	virtual void UpdateViewport();

	virtual void SetCullShape(fwRenderPhaseCullShape & cullShape);

	virtual  u32 GetCullFlags() const;

	fwInteriorLocation GetInteriorLocation() const;

	static void	 AddToDrawListParaboloidShadow(s32 list, int s, int side, bool drawDynamic, bool drawStatic, bool isFaceted);
	static bool  ShadowEntityCullCheck     (fwRenderPassId id, fwRenderBucket bucket, fwRenderMode renderMode, int subphasePass, float sortVal, fwEntity *entity);

private:
	void	AddCubeMapFaces(int s, int destIndex, bool renderDynamic, bool renderStatic, const CShadowInfo& info, bool clearTarget);
	void	BuildDrawListCM(const CParaboloidShadowActiveEntry& entry, int s);

	int m_paraboloidShadowIndex;
	
	static Mat44V	sm_CurrentFacetFrustumLRTB;
};

#endif // !INC_RENDERPHASEPARABOLOIDSHADOW_H_
