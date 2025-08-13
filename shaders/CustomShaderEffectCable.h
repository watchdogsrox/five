//
// Filename: CustomShaderEffectCable.h
//

#ifndef __CUSTOMSHADEREFFECTCABLE_H__
#define __CUSTOMSHADEREFFECTCABLE_H__

#include "rmcore/drawable.h"
#include "shaders/CustomShaderEffectBase.h"

#define CABLE_AUTO_CHECK (1 && __DEV)

#if CABLE_AUTO_CHECK
	#define CABLE_AUTO_CHECK_ONLY(x) x
	#include "atl/map.h"
#else
	#define CABLE_AUTO_CHECK_ONLY(x)
#endif

class CCustomShaderEffectCableType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectCable;

public:
	CCustomShaderEffectCableType() : CCustomShaderEffectBaseType(CSE_CABLE) {}

	virtual bool Initialise(rmcDrawable* pDrawable);
	virtual CCustomShaderEffectBase* CreateInstance(CEntity* pEntity);

protected:
	virtual ~CCustomShaderEffectCableType() {}

private:
	grmShaderGroupVar m_idVarViewProjection;
	grmShaderGroupVar m_idVarCableParams;
	grmShaderGroupVar m_idVarCableTexture;
	grmShaderGroupVar m_idVarAlphaTest;
};

class CCustomShaderEffectCable : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectCableType;

public:
	CCustomShaderEffectCable(CEntity* pEntity);
	~CCustomShaderEffectCable() {}

public:
	virtual void SetShaderVariables(rmcDrawable* pDrawable);
	virtual void AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void AddToDrawListAfterDraw();
	virtual void Update(fwEntity* pEntity);

	inline bool IsLitCable() { return m_bDepthWrite; }

	static void Init();
#if __BANK
	static void InitWidgets(bkBank& bank);
	static void InitOptimisationWidgets(bkBank& bank);
	static void SetEnable(bool bEnable) { ms_debugCableEnable = bEnable; }
	static void SetEditableShaderValues(grmShaderGroup* pShaderGroup, rmcDrawable* pDrawable);
#if __DEV
	static bool CheckDrawable(const Drawable* pDrawable, const char* path, bool bSimple = false, atMap<u16,u32>* indices = NULL, Mat34V_In matrix = Mat34V(V_IDENTITY), float* distToCamera = NULL);
#endif // __DEV
#endif // __BANK

	static void UpdateWindParams();
	static void SwitchDrawCore(bool enable);
#if RSG_PC
	static void SwitchDepthWrite(bool enable);
#endif
	static bool IsCableVisible(CEntity* pEntity);

private:
	CCustomShaderEffectCableType* m_pType;
//	bool                          m_bInterior;
	bool                          m_bDepthWrite; // used for lit cables
#if CABLE_AUTO_CHECK
	atMap<u16,u32> m_indices;
#endif // CABLE_AUTO_CHECK
#if __BANK
	static bool        ms_debugCableEnable;
	static bool        ms_debugCableForceInteriorAmbientOn;
	static bool        ms_debugCableForceInteriorAmbientOff;
	static bool        ms_debugCableForceExteriorAmbientOn;
	static bool        ms_debugCableForceExteriorAmbientOff;
	static bool        ms_debugCableForceDepthWritesOn;
	static bool        ms_debugCableForceDepthWritesOff;
	static bool        ms_debugCableDrawNormals;
	static float       ms_debugCableDrawNormalsLength;
	static bool        ms_debugCableDrawWindTestGlobal;
	static bool        ms_debugCableDrawWindTestLocal;
	static bool        ms_debugCableDrawWindTestLocalCamera;
	static float       ms_debugCableDrawWindTestLength;
	static int         ms_debugCableTextureOverride;
	static grcTexture* ms_debugCableTextures[3];
	static float       ms_debugCableRadiusScale;
	static float       ms_debugCablePixelThickness;
	static float	   ms_debugCableShadowThickness;
	static float	   ms_debugCableShadowWindModifier;
	static float       ms_debugCableFadeExponentAdj;
	static bool        ms_debugCableDiffuseOverride;
	static Color32     ms_debugCableDiffuse;
	static bool        ms_debugCableAmbientOverride;
	static float       ms_debugCableAmbientNatural;
	static float       ms_debugCableAmbientArtificial;
	static float       ms_debugCableAmbientEmissive;
	static float       ms_debugCableOpacity;
	static bool        ms_debugCableWindMotionEnabled;
	static float       ms_debugCableWindMotionFreqScale;
	static float       ms_debugCableWindMotionFreqX;
	static float       ms_debugCableWindMotionFreqY;
	static float       ms_debugCableWindMotionAmpScale;
	static float       ms_debugCableWindMotionAmpX;
	static float       ms_debugCableWindMotionAmpY;
	static bool        ms_debugCableWindResponseEnabled;
	static float       ms_debugCableWindResponseAmount;
	static float       ms_debugCableWindResponseVelMin;
	static float       ms_debugCableWindResponseVelMax;
	static float       ms_debugCableWindResponseVelExp;
#if CABLE_AUTO_CHECK
	static float       ms_debugCableCheckSphereRadius;
	static u32         ms_debugCableCheckFlags;
#endif // CABLE_AUTO_CHECK
#endif // __BANK
};

#endif // __CUSTOMSHADEREFFECTCABLE_H__
