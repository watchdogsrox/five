// ==================================================
// renderer/RenderPhases/RenderPhaseWaterReflection.h
// (c) 2012 RockstarNorth
// ==================================================

#ifndef _RENDERER_RENDERPHASES_RENDERPHASEWATERREFLECTION_H_
#define _RENDERER_RENDERPHASES_RENDERPHASEWATERREFLECTION_H_

#include "renderer/RenderPhases/RenderPhase.h"

#define WATER_REFLECTION_OFFSET_TRICK (0 && __DEV) // for per-object offset in water reflection based on camera or something
#define WATER_REFLECTION_PRE_REFLECTED (1) // another possible solution to sloped/curved/multilevel water surfaces

namespace rage { class fwRenderPhaseCullShape; }
namespace rage { class grcRenderTarget; }
namespace rage { class bkGroup; }

class CViewport;
class CPhysical;

class CRenderPhaseWaterReflection : public CRenderPhaseScanned
{
public:
	static void InitClass();

	CRenderPhaseWaterReflection(CViewport* pGameViewport);
	~CRenderPhaseWaterReflection();

	virtual  u32 GetCullFlags() const;
	virtual void SetCullShape(fwRenderPhaseCullShape& cullShape);
	virtual void UpdateViewport();
	virtual void BuildRenderList();
	virtual void BuildDrawList();
#if __BANK
	virtual void AddWidgets(bkGroup& group);
#endif // __BANK

	static void SetRenderTargetColor(grcRenderTarget *target) { m_color = target; }
	static grcRenderTarget* GetRenderTargetColor() { return m_color; } 
#if RSG_PC
	static void SetMSAARenderTargetColor(grcRenderTarget *target) { m_MSAAcolor = target; }
	static grcRenderTarget* GetMSAARenderTargetColor() { return m_MSAAcolor; }
#endif
#if __PS3
	static void SetRenderTargetMSAAColor(grcRenderTarget *target) { m_msaacolor = target; }
#endif // __PS3
	static void SetRenderTargetDepth(grcRenderTarget *target) { m_depth = target; }

#if __D3D11
	static void SetRenderTargetDepthReadOnly(grcRenderTarget *target) { m_depthReadOnly = target; }
	static void SetRenderTargetDepthCopy(grcRenderTarget *target) { m_depthCopy = target; }

#if RSG_PC
	static grcRenderTarget* GetDepthReadOnly();
	static grcRenderTarget* GetDepthCopy();
#else
	static grcRenderTarget* GetDepthReadOnly() { return m_depthReadOnly; }
	static grcRenderTarget* GetDepthCopy() { return m_depthCopy; }
#endif

	static grcRenderTarget* GetDepth() { return m_depth; }
#endif // __D3D11

#if RSG_PC
	static void ResolveMSAA();
#endif

#if RSG_ORBIS
	static void ResolveRenderTarget() { m_color->GenerateMipmaps(); }
#endif

private:

	static grcRenderTarget* m_color;
	static grcRenderTarget* m_depth;
#if RSG_PC
	static grcRenderTarget* m_MSAAcolor;
#endif

#if __D3D11
	static grcRenderTarget* m_depthCopy;
	static grcRenderTarget* m_depthReadOnly;
#endif
};

class CRenderPhaseWaterReflectionInterface
{
public:
	static bool IsWaterReflectionEnabled();
	static void SetCullShapeFromStencil(bool bRunOnSPU = false);
	static const grcViewport* GetViewport();
	static void ResetViewport();
	static Vec4V_Out GetCurrentWaterPlane();
	static const CRenderPhaseScanned* GetRenderPhase();
	static int GetWaterReflectionTechniqueGroupId();
#if __BANK
	static bool DrawListPrototypeEnabled();
	static bool IsUsingMirrorWaterSurface();
	static bool IsSkyEnabled();
	static u32 GetWaterReflectionClearColour();
#if WATER_REFLECTION_OFFSET_TRICK
	static void ApplyWaterReflectionOffset(Vec4V_InOut pos, u32 modelIndex);
#endif // WATER_REFLECTION_OFFSET_TRICK
#endif // __BANK
	static void UpdateDistantLights(Vec3V_In camPos);
	static void AdjustDistantLights(float& intensityScale);
	static bool IsOcclusionDisabledForOceanWaves();
	static void UpdateInWater(CPhysical* pPhysical, bool bInWater);
	static bool ShouldEntityForceUpAlpha(const CEntity* pEntity);
	static bool CanAlphaCullInPostScan(const CEntity* pEntity);
	static bool CanAlphaCullInPostScan();
	static void SetScriptWaterHeight(bool bEnable, float height = 0.0f); // temporary workaround until we have a proper water height system
	static void SetScriptObjectVisibility(bool bForceVisible);
};

#endif // _RENDERER_RENDERPHASES_RENDERPHASEWATERREFLECTION_H_
