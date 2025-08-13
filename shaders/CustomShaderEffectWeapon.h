//
// Filename:	CustomShaderEffectWeapon.h
// Description:	Class for controlling weapon shader variables
// Written by:	Andrzej
//
//	16/02/2012 -	Andrzej:	- initial;
//
//
//
//
#ifndef __CCUSTOMSHADEREFFECTWEAPON_H__
#define __CCUSTOMSHADEREFFECTWEAPON_H__

#include "rmcore\drawable.h"
#include "shaders\CustomShaderEffectBase.h"
#include "shaders\CustomShaderEffectTint.h"


#define	CSE_WEAPON_EDITABLEVALUES					(__BANK)
#define CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM		(32)	// no of palette rows in one palette texture 
#define CSE_WEAPON_MAX_CAMO_TEXTURES				(8)		// amount of swappable textures


#define CSE_WEAPON_CAMO_SHADER1_NAME				"weapon_normal_spec_cutout_palette"
#define CSE_WEAPON_CAMO_SHADER1_NAMEHASH			((u32)0x2098AD32) // "weapon_normal_spec_cutout_palette"
#define CSE_WEAPON_CAMO_SHADER2_NAME				"weapon_normal_spec_palette"
#define CSE_WEAPON_CAMO_SHADER2_NAMEHASH			((u32)0x9905A1ED) // "weapon_normal_spec_palette"

//
//
//
//
class CCustomShaderEffectWeaponType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectWeapon;
public:
	CCustomShaderEffectWeaponType()	 : CCustomShaderEffectBaseType(CSE_WEAPON), m_pTintType(NULL){}

	virtual bool							Initialise(rmcDrawable *)						{ Assert(0); return(false); }
	bool									Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bTint, bool bPalette);

	virtual CCustomShaderEffectBase*		CreateInstance(CEntity *pEntity);
	virtual bool							RestoreModelInfoDrawable(rmcDrawable *pDrawable);

	void									SetIsHighDetail(bool flag);
	bool									GetIsHighDetail() const							{ return m_bIsHighDetail; }

	bool									GetHasTint() const								{ return m_bHasTint;	}
	bool									GetHasPalette() const							{ return m_bHasPalette;	}

protected:
	virtual ~CCustomShaderEffectWeaponType();

private:
	CCustomShaderEffectTintType				*m_pTintType;	// tint type cascade

	pgDictionary<grcTexture>				*m_pTexDict;
	atArray<s8>								*m_pCamoDiffTexIdxTab;

	struct shaderDiffuseTex
	{
		u8		m_ShaderIdx;		// shader idx in shaderGroup
		u8		m_idVarDiffuseTex;	// idx to diffuse tex variable
	};
	atArray<shaderDiffuseTex>				*m_pCamoShaderDiffTexRemap;


	u8										m_idVarSpecFresnel;
	u8										m_idVarSpecFalloffMult;
	u8										m_idVarSpecIntMult;
	u8										m_idVarSpec2Factor;
	u8										m_idVarSpec2ColorInt;
	u8										m_idVarSpec2Color;
	u8										m_idVarDiffuseTexPalSelector;

	u8										m_bIsHighDetail		:1;
	u8										m_bHasTint			:1;
	u8										m_bHasPalette		:1;
	u8										m_bSwappableCamoTex	:1;
	ATTR_UNUSED u8							m_idVarPad00		:4;
};


//
//
//
//
class CCustomShaderEffectWeapon : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectWeaponType;

public:
	CCustomShaderEffectWeapon();
	~CCustomShaderEffectWeapon();

public:
	virtual void						SetShaderVariables(rmcDrawable* pDrawable);
	virtual void						AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void						AddToDrawListAfterDraw();
	virtual u32							AddDataForPrototype(void * address);

	virtual void						Update(fwEntity *pEntity);

	void								CopySettings(CCustomShaderEffectWeapon *src);
#if CSE_WEAPON_EDITABLEVALUES
	static bool							InitWidgets(bkBank& bank);
	static void							SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable, CCustomShaderEffectWeaponType *pType);
#endif

	inline void							SetSpecFresnel(float f);
	inline void							SetSpecFalloffMult(float f);
	inline void							SetSpecIntMult(float f);
	inline void							SetSpec2Factor(float f);
	inline void							SetSpec2ColorInt(float f);
	inline void							SetSpec2Color(Color32 c);

	inline float						GetSpecFresnel() const		{	return m_fSpecFresnel;		}
	inline float						GetSpecFalloffMult() const	{	return m_fSpecFalloffMult;	}
	inline float						GetSpecIntMult() const		{	return m_fSpecIntMult;		}
	inline float						GetSpec2Factor() const		{	return m_fSpec2Factor;		}
	inline float						GetSpec2ColorInt() const	{	return m_fSpec2ColorInt;	}
	inline Color32						GetSpec2Color() const		{	return m_fSpec2Color;		}


	// tint wrappers:
	inline void							SelectTintPalette(u8 pal, fwEntity* pEntity);
	inline u32							GetMaxTintPalette();

	// palette:
	inline void							SetDiffusePalette(u32 pal);
	inline u32							GetDiffusePalette() const;
	
	// camo diffuse tex:
	inline void							SetCamoDiffuseTexIdx(u32 diff);
	inline u32							GetCamoDiffuseTexIdx() const;


	const CCustomShaderEffectWeaponType*	GetCseType() const						{ return m_pType; }

private:
	CCustomShaderEffectWeaponType*		m_pType;
	CCustomShaderEffectTint*			m_pTint;	// tint instance cascade

private:
	float								m_fSpecFresnel;
	float								m_fSpecFalloffMult;
	float								m_fSpecIntMult;
	float								m_fSpec2Factor;
	float								m_fSpec2ColorInt;
	Color32								m_fSpec2Color;

	u8									m_varDiffuseTexPalSelector;
	u8									m_varCamoDiffuseTexIdx;			// selected diffuse texture in m_pCamoDiffTexIdxTab

#if CSE_WEAPON_EDITABLEVALUES
	static bool							ms_bEVEnabled;
	static float						ms_fEVSpecFalloff;
	static float						ms_fEVSpecIntensity;
	static float						ms_fEVSpecFresnel;
	static float						ms_fEVSpec2Factor;
	static float						ms_fEVSpec2ColorIntensity;
	static Color32						ms_fEVSpec2Color;
	static u32							ms_fEVDiffuseTexPalSelector;
	static u32							ms_fEVCamoDiffuseTexIdx;
#endif
};



void CCustomShaderEffectWeapon::SetSpecFresnel(float f)
{
	m_fSpecFresnel = f;
}

void CCustomShaderEffectWeapon::SetSpecFalloffMult(float f)
{
	m_fSpecFalloffMult = f;
}

void CCustomShaderEffectWeapon::SetSpecIntMult(float f)
{
	m_fSpecIntMult = f;
}

void CCustomShaderEffectWeapon::SetSpec2Factor(float f)
{
	m_fSpec2Factor = f;
}

void CCustomShaderEffectWeapon::SetSpec2ColorInt(float f)
{
	m_fSpec2ColorInt = f;
}

void CCustomShaderEffectWeapon::SetSpec2Color(Color32 c)
{
	m_fSpec2Color = c;
}

void CCustomShaderEffectWeapon::SelectTintPalette(u8 pal, fwEntity* pEntity)
{
	if(m_pTint)
	{
		m_pTint->SelectTintPalette(pal, pEntity);
	}
}

u32	CCustomShaderEffectWeapon::GetMaxTintPalette()
{
	if(m_pTint)
	{
		return m_pTint->GetMaxTintPalette();
	}

	return 0;
}

void CCustomShaderEffectWeapon::SetDiffusePalette(u32 pal)
{
	Assert(pal < CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM);
	if(pal >= CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM)
	{
		pal = CSE_WEAPON_COLOR_VAR_TEXTURE_PAL_ROWNUM-1;
	}

	m_varDiffuseTexPalSelector = (u8)pal;
}

u32 CCustomShaderEffectWeapon::GetDiffusePalette() const
{
	return (u32)m_varDiffuseTexPalSelector;
}

void CCustomShaderEffectWeapon::SetCamoDiffuseTexIdx(u32 diff)
{
	if(m_pType->m_bSwappableCamoTex)
	{
		Assert(diff < CSE_WEAPON_MAX_CAMO_TEXTURES);
		if(diff >= CSE_WEAPON_MAX_CAMO_TEXTURES)
		{
			diff = CSE_WEAPON_MAX_CAMO_TEXTURES-1;
		}

		m_varCamoDiffuseTexIdx = (u8)diff;
	}
}

u32 CCustomShaderEffectWeapon::GetCamoDiffuseTexIdx() const
{
	return (u32)m_varCamoDiffuseTexIdx;
}

#endif //__CCUSTOMSHADEREFFECTWEAPON_H__...

