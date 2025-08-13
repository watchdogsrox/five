//
// Filename: CustomShaderEffectMirror.h
//

#ifndef __CUSTOMSHADEREFFECTMIRROR_H__
#define __CUSTOMSHADEREFFECTMIRROR_H__

#include "rmcore/drawable.h"
#include "shaders/CustomShaderEffectBase.h"
#include "scene/RegdRefTypes.h"

#define CSE_MIRROR_STORE_ENTITY_LIST (0)

#if CSE_MIRROR_STORE_ENTITY_LIST
	#define CSE_MIRROR_STORE_ENTITY_LIST_ONLY(x) x
#else
	#define CSE_MIRROR_STORE_ENTITY_LIST_ONLY(x)
#endif

class CCustomShaderEffectMirrorType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectMirror;

public:
	CCustomShaderEffectMirrorType() : CCustomShaderEffectBaseType(CSE_MIRROR) {}

	virtual bool Initialise(rmcDrawable* pDrawable);
	virtual CCustomShaderEffectBase* CreateInstance(CEntity* pEntity);
protected:
	virtual ~CCustomShaderEffectMirrorType() {}

private:
	grmShaderGroupVar m_idVarMirrorBounds;
#if __PS3
	grmShaderGroupVar m_idVarMirrorReflection;
#endif // __PS3
#if !__FINAL
	grmShaderGroupVar m_idVarMirrorDebugParams;
#endif // !__FINAL
};

class CCustomShaderEffectMirror : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectMirrorType;

public:
	static void Initialise();

	CCustomShaderEffectMirror(CEntity* pEntity, CCustomShaderEffectMirrorType* pType);
	~CCustomShaderEffectMirror() {}

	virtual void SetShaderVariables(rmcDrawable* pDrawable);
	virtual void AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void AddToDrawListAfterDraw();
#if __BANK
	static void InitWidgets(bkBank& bank);
#endif // __BANK

private:
	CCustomShaderEffectMirrorType* m_pType;
	bool m_bIsLowPriorityMirrorFloor;
#if !__FINAL
	static bool  ms_debugMirrorUseWhiteTexture;
	static float ms_debugMirrorSuperReflective;
	static float ms_debugMirrorBrightnessScale;
#endif // !__FINAL

#if CSE_MIRROR_STORE_ENTITY_LIST
public:
	static int GetNumMirrorEntities();
	static CEntity* GetMirrorEntity(int index);
private:
	static atFixedArray<RegdEnt,16> sm_mirrorEntities;
#endif // CSE_MIRROR_STORE_ENTITY_LIST
};

#endif // __CUSTOMSHADEREFFECTMIRROR_H__
