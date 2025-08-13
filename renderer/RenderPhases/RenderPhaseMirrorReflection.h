// ===================================================
// renderer/RenderPhases/RenderPhaseMirrorReflection.h
// (c) 2011 RockstarNorth
// ===================================================

#ifndef INC_RENDERPHASEMIRRORREFLECTION_H_
#define INC_RENDERPHASEMIRRORREFLECTION_H_

#include "renderer/RenderPhases/RenderPhase.h"

class CRenderPhaseMirrorReflection : public CRenderPhaseScanned
{
public:
	static void InitClass() {}

	CRenderPhaseMirrorReflection(CViewport* pGameViewport);
	virtual ~CRenderPhaseMirrorReflection();

	virtual  u32 GetCullFlags() const;
	virtual void BuildRenderList();
	virtual void BuildDrawList();
	virtual void UpdateViewport();
	virtual void SetCullShape(fwRenderPhaseCullShape& cullShape);
#if __BANK
	virtual void AddWidgets(bkGroup& group);
#endif // __BANK

	static fwRoomSceneGraphNode* GetMirrorRoomSceneNode();
	static void                  SetMirrorScanCameraIsInInterior(bool b); // would be cleaner to just store this in ScanResults?
	static bool                  GetMirrorScanFromGBufStartingNode();

private:
#if __BANK
	bool  m_debugDrawMirror;
	float m_debugDrawMirrorOffset;
#endif // __BANK
};

class CRenderPhaseMirrorReflectionInterface
{
public:
	static const grcViewport* GetViewport();
	static const grcViewport* GetViewportNoObliqueProjection();
	static CRenderPhaseScanned* GetRenderPhase();
	static void SetIsMirrorUsingWaterReflectionTechniqueAndSurface_Render(bool bIsMirrorUsingWaterWaterReflectionTechnique, bool bIsMirrorUsingWaterSurface);
	static bool GetIsMirrorUsingWaterReflectionTechnique_Render();
	static bool GetIsMirrorUsingWaterSurface_Render();
#if __XENON
	static bool GetIsMirrorUsingWaterReflectionTechnique_Update();
#endif // __XENON
};

#endif // !INC_RENDERPHASEMIRRORREFLECTION_H_
