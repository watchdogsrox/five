//
// CustomShaderAnimUV - class for managing UV animation for entities and their shaders;
//
//	20/06/2006	-	Andrzej:	- intial;
//
//
//
//
#ifndef __CUSTOMSHADEREFFECT_ANIMUV__
#define __CUSTOMSHADEREFFECT_ANIMUV__

#include "shaders\CustomShaderEffectBase.h"


#define	CSE_ANIM_EDITABLEVALUES				(__BANK)


class CCustomShaderEffectAnimUVType: public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectAnimUV;
public:
	CCustomShaderEffectAnimUVType() : CCustomShaderEffectBaseType(CSE_ANIMUV) {}
	static CCustomShaderEffectAnimUVType* Create(rmcDrawable *pDrawable, u32 nObjectHashKey, s32 animDictFileIndex);
	static CCustomShaderEffectAnimUVType* Create(rmcDrawable *pDrawable) { return Create(pDrawable,0,-1);}
	virtual CCustomShaderEffectBase*	CreateInstance(CEntity *pEntity);														// instance create in CEntity::CreateDrawable()
protected:
	virtual ~CCustomShaderEffectAnimUVType();
private:
	struct structAnimUVinfo
	{
		pgRef<const crClip>		m_pClip;
		u8					m_varUV0, m_varUV1, m_shaderIdx, pad0;
	};
	atArray<structAnimUVinfo> m_tabAnimUVInfo;
	s32						m_wadAnimIndex;
};

//
//
//
//
class CCustomShaderEffectAnimUV : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectAnimUVType;
public:
	~CCustomShaderEffectAnimUV();

#if __DEV
	static void AreClipsValid(CCustomShaderEffectAnimUV* pShader, const char* pObjectName = NULL);
#endif //__DEV
	static bool						CreateExtraAnimationUVvariables(rmcDrawable *pDrawable);

public:
	virtual void					Update(fwEntity *pEntity);																// instance update in CEntity::PreRender()

	virtual void					SetShaderVariables(rmcDrawable* pDrawable);													// instance setting variables in CEntity/DynamicEntity::Render()
	virtual void					AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void					AddToDrawListAfterDraw()		{/*do nothing*/}

	bool							GetLightAnimUV(Vector3 *uv0, Vector3 *uv1);
	strLocalIndex					GetClipDictIndex() { return strLocalIndex(m_pType->m_wadAnimIndex); }

#if CSE_ANIM_EDITABLEVALUES
	static bool						InitWidgets(bkBank& bank);
#endif

private:
	CCustomShaderEffectAnimUV(u32 size);

	CCustomShaderEffectAnimUVType *m_pType;
	ATTR_UNUSED int					pad[3];

	struct ALIGNAS(16) structUVs { float m_uv0[3]; unsigned m_timeAnimStart; float m_uv1[3]; unsigned pad; } ;
	structUVs m_uvs[0];
};


#endif //__CUSTOMSHADEREFFECT_ANIMUV__....

