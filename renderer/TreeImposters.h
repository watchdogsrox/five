////////////////////////////////////////////////////////////////////////////////////
// Title	:	TreeImposters.h
// Author	:	
// Started	:	2008/02/19
//			:	
////////////////////////////////////////////////////////////////////////////////////
 
#ifndef _TREEIMPOSTERS_H_
#define _TREEIMPOSTERS_H_

#include "grcore/effect_typedefs.h"
#include "UseTreeImposters.h"

#if USE_TREE_IMPOSTERS

namespace rage {
class Vector3;
class Matrix34;
class grcRenderTarget;
class grmShader;
class grcTexture;
class rmcDrawable;
}

// Game headers
class CRenderPhaseTreeImposters;

class CImposterCacheInfo
{
public:
	u32 m_modelIndex;
	int m_age;
	float m_fade;
	int m_index;
	// [SPHERE-OPTIMISE] m_centre,m_radius should be spdSphere
	float m_radius;
	Vector3 m_centre;

	grcRenderTarget* m_renderTarget;
};

//
//
//
//
class CTreeImposters
{
public:
	static void Init();
	static void Shutdown();

	static void Update();

	static grcRenderTarget* GetCacheRenderTarget(int index);
	static grcRenderTarget* GetSharedDepthTarget();
	static grcRenderTarget* GetPaletteTarget();

	static Vector3 GetImposterViewDir(int i);
	static Vector3 GetImposterViewUp(int i);

	static CImposterCacheInfo* GetCacheEntryToRender();

	static int GetImposterCacheIndex(int modelIndex);

	static void RenderLeafColour(int index);

	static void RenderTreeImposter(int index, const Matrix34& mat, u8 alpha, u8 ambient);
	static void RenderTreeImposterShadow(int index, const Matrix34& mat, u8 alpha);

	static s32 GetImposterTechniqueGroupId(){return ms_imposterTechniqueGroupId;}
	static s32 GetImposterDeferredTechniqueGroupId(){return ms_imposterDeferredTechniqueGroupId;}

	static float GetImposterFade(int index, float dist);
	static float GetImposterFadeShadow(int index, float dist);

#if __BANK
	static void InitWidgets();
#endif //__BANK..

private:

	static grcRenderTarget*		ms_sharedDepthRT;
#if __PPU
	static grcRenderTarget*		ms_paletteRT;
#endif

	static grmShader*			ms_imposterShader;
	static grcEffectTechnique	ms_imposterTechnique;
	static grcEffectTechnique	ms_imposterShadowTechnique;
	static grcEffectTechnique	ms_blitAverageTechnique;
	static grcEffectVar			ms_imposterTexture_ID;
	static grcEffectVar			ms_imposterWorldX_ID;
	static grcEffectVar			ms_imposterWorldY_ID;
	static grcEffectVar			ms_imposterWorldZ_ID;
	static grcEffectVar			ms_blitTexture_ID;

	static s32					ms_imposterTechniqueGroupId;
	static s32					ms_imposterDeferredTechniqueGroupId;

	static CImposterCacheInfo	ms_imposterCache[16]; 

public:
	static float				ms_viewport_fov;
	static Vector3				ms_windDir;
	static float				ms_windAng;
 
#if __BANK
	static bool		ms_Enable;
	static bool		ms_EnableShadow;
	static bool		ms_FlushCache;
	static int		ms_ForcedUpdateIndex;

	static bool		ms_debugNoLeaves;
	static bool		ms_debugNoLeafShadows;
	static bool		ms_debugNoTrunks;
	static bool		ms_debugNoTrunkShadows;
	static bool		ms_debugOverride;
	static bool		ms_debugDisableAllTrees;
#endif //__BANK...

	static float ms_FadeStart;
	static float ms_FadeEnd;
	static float ms_FadeStartShadow;
	static float ms_FadeEndShadow;
#if 1//__PPU
	static grcTexture* ms_normTextureLUT;
#endif

	static int ms_RTUsedFlags[2];
	static int ms_RTBuffer[2][16];
	static int ms_genModelIndex;

//#if __XENON
	static int ms_bufferID;
//#endif
};

#endif // USE_TREE_IMPOSTERS

#endif //TREEIMPOSTERS


