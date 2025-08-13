//
// Filename:	CustomShaderEffectProp.h
// Description:	Class for controlling prop shaders variables
// Written by:	Andrzej
//
//	12/08/2010 -	Andrzej:	- initial;
//
//
//
//
#ifndef __CCUSTOMSHADEREFFECTPROP_H__
#define __CCUSTOMSHADEREFFECTPROP_H__

#include "rmcore\drawable.h"
#include "shaders\CustomShaderEffectBase.h"
#include "shaders\CustomShaderEffectTint.h"


#define	CSE_PROP_EDITABLEVALUES				(__BANK)
#define CSE_PROP_MAX_DIFF_TEXTURES			(26)	// amount of swappable textures

//
//
//
//
class CCustomShaderEffectPropType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectProp;
public:
	CCustomShaderEffectPropType() : CCustomShaderEffectBaseType(CSE_PROP), m_pTintType(NULL) {}

	virtual bool						Initialise(rmcDrawable *)								{ Assert(0); return(false); }
	bool								Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bIsParachute, bool bSwapableDiffTex, bool bIsTint);

	virtual CCustomShaderEffectBase*	CreateInstance(CEntity *pEntity);
	virtual bool						RestoreModelInfoDrawable(rmcDrawable *pDrawable);

protected:
	virtual ~CCustomShaderEffectPropType();

	inline pgDictionary<grcTexture>*	GetTexDict(rmcDrawable *drwbl);

private:
	CCustomShaderEffectTintType			*m_pTintType;	// tint type cascade
	atArray<s8>							*m_pDiffTexIdxTab;

	u8									m_idVarWrinkleMaskStrengths0;
	u8									m_idVarWrinkleMaskStrengths1;
	u8									m_idVarWrinkleMaskStrengths2;
	u8									m_idVarWrinkleMaskStrengths3;

	u8									m_idVarUmGlobalOverrideParams;
	u8									m_idVarDiffuseTex;

	u8									m_bIsParachute		:1;
	u8									m_bSwapableDiffTex	:1;
	ATTR_UNUSED u8						m_pad_u8			:6;
};


//
//
//
//
pgDictionary<grcTexture>* CCustomShaderEffectPropType::GetTexDict(rmcDrawable *drwbl)
{
	if(drwbl)
	{
		return drwbl->GetShaderGroup().GetTexDict();
	}
	return(NULL);
}

//
//
//
//
class CCustomShaderEffectProp : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectPropType;
public:
	CCustomShaderEffectProp();
	~CCustomShaderEffectProp();

public:

	virtual void						SetShaderVariables(rmcDrawable* pDrawable);
	virtual void						AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void						AddToDrawListAfterDraw();

	virtual void						Update(fwEntity *pEntity);

	Vector4*							GetWrinkleMaskStrengths()		{return m_wrinkleMaskStrengths;}

	u32									GetNumTintPalettes();

	// um global override params:
	void								SetUmGlobalOverrideParams(float scaleH, float scaleV, float argFreqH, float argFreqV);
	void								SetUmGlobalOverrideParams(float s);
	void								GetUmGlobalOverrideParams(float *pScaleH, float *pScaleV, float *pArgFreqH, float *pArgFreqV);

	inline void							SelectTintPalette(u8 pal, fwEntity* pEntity);
	inline u32							GetMaxTintPalette() const;

	inline void							SetDiffuseTexIdx(u8 diff);
	inline u8							GetDiffuseTexIdx() const;

#if CSE_PROP_EDITABLEVALUES
	static bool							InitWidgets(bkBank& bank);
	static void							SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable, CCustomShaderEffectPropType *pType);
#endif

private:
	// These could be changed to u8's but due to alignment of Vector4 it won't actually save any space.
	CCustomShaderEffectPropType			*m_pType;
	CCustomShaderEffectTint				*m_pTint;	// tint instance cascade
private:
	Vector4								m_wrinkleMaskStrengths[4];

	Float16Vec4							m_umGlobalOverrideParams;
	u8									m_nDiffuseTexIdx;			// selected diffuse texture in m_pDiffTexIdxTab
	ATTR_UNUSED u16						m_pad_u8[1];
	ATTR_UNUSED u16						m_pad_u16[3];

#if CSE_PROP_EDITABLEVALUES
	static bool							ms_bEVEnabled;
	static Vector4						ms_vEVWrinkleMaskStrengths[4];
	static Vector4						ms_fEVUmGlobalParams;
	static bool							ms_bEVUseUmGlobalOverrideValues;
	static Vector4						ms_fEVUmGlobalOverrideParams;
	static u32							ms_nEVDiffuseTexId;
	static bool							ms_bEVWindMotionEnabled;
	static float						ms_fEVWindMotionFreqScale;
	static float						ms_fEVWindMotionFreqX;
	static float						ms_fEVWindMotionFreqY;
	static float						ms_fEVWindMotionAmpScale;
	static float						ms_fEVWindMotionAmpX;
	static float						ms_fEVWindMotionAmpY;
	static bool							ms_bEVWindResponseEnabled;
	static float						ms_fEVWindResponseAmount;
	static float						ms_fEVWindResponseVelMin;
	static float						ms_fEVWindResponseVelMax;
	static float						ms_fEVWindResponseVelExp;
#endif
};

CompileTimeAssertSize(CCustomShaderEffectBase,16,24);


//
//
//
//
void CCustomShaderEffectProp::SelectTintPalette(u8 pal, fwEntity* pEntity)
{
	if(m_pTint)
	{
		m_pTint->SelectTintPalette(pal, pEntity);
	}
}

u32	CCustomShaderEffectProp::GetMaxTintPalette() const
{
	if(m_pTint)
	{
		return m_pTint->GetMaxTintPalette();
	}

	return 0;
}

void CCustomShaderEffectProp::SetDiffuseTexIdx(u8 diff)
{
	if(m_pType->m_bSwapableDiffTex)
	{
		Assert(diff < CSE_PROP_MAX_DIFF_TEXTURES);
		if(diff >= CSE_PROP_MAX_DIFF_TEXTURES)
		{
			diff = CSE_PROP_MAX_DIFF_TEXTURES-1;
		}

		m_nDiffuseTexIdx = diff;
	}
}

u8 CCustomShaderEffectProp::GetDiffuseTexIdx() const
{
	return m_nDiffuseTexIdx;
}

#endif //__CCUSTOMSHADEREFFECTPROP_H__...

