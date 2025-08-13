//
// Filename:	CustomShaderEffectInterior.h
// Description:	Class for controlling DLC interior shaders variables
// Written by:	Andrzej
//
//	30/10/2013 -	Andrzej:	- initial;
//
//
//
//
#ifndef __CCUSTOMSHADEREFFECTINTERIOR_H__
#define __CCUSTOMSHADEREFFECTINTERIOR_H__

#include "rmcore\drawable.h"
#include "shaders\CustomShaderEffectBase.h"
#include "shaders\CustomShaderEffectTint.h"


#define	CSE_INTERIOR_EDITABLEVALUES				(__BANK)

//
//
//
//
class CCustomShaderEffectInteriorType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectInterior;
public:
	CCustomShaderEffectInteriorType() : CCustomShaderEffectBaseType(CSE_INTERIOR), m_pTintType(NULL) {}

	virtual bool						Initialise(rmcDrawable *)								{ Assert(0); return(false); }
	bool								Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bIsTint);

	virtual CCustomShaderEffectBase*	CreateInstance(CEntity *pEntity);
	virtual bool						RestoreModelInfoDrawable(rmcDrawable *pDrawable);

protected:
	virtual ~CCustomShaderEffectInteriorType();

private:
	CCustomShaderEffectTintType			*m_pTintType;	// tint type cascade

	u8									m_idVarDiffuseTex;
};


//
//
//
//
class CCustomShaderEffectInterior : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectInteriorType;
public:
	CCustomShaderEffectInterior();
	~CCustomShaderEffectInterior();

public:
	virtual void						SetShaderVariables(rmcDrawable* pDrawable);
	virtual void						AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void						AddToDrawListAfterDraw();

	virtual void						Update(fwEntity *pEntity);

	inline void							SetDiffuseTexture(grcTexture *pNewDiffuseTex);
	inline grcTexture*					GetDiffuseTexture() const;

	inline void							SelectTintPalette(u8 pal, fwEntity* pEntity);
	inline u32							GetMaxTintPalette() const;
	u32									GetNumTintPalettes();

#if CSE_INTERIOR_EDITABLEVALUES
	static bool							InitWidgets(bkBank& bank);
	static void							SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable);
#endif

private:
	// These could be changed to u8's but due to alignment of Vector4 it won't actually save any space.
	CCustomShaderEffectInteriorType		*m_pType;
	CCustomShaderEffectTint				*m_pTint;	// tint instance cascade

	grcTextureIndex						m_varhDiffuseTex;

#if CSE_INTERIOR_EDITABLEVALUES
	static bool							ms_bEVEnabled;
#endif
};

CompileTimeAssertSize(CCustomShaderEffectBase,16,24);

//
//
//
//
inline void CCustomShaderEffectInterior::SetDiffuseTexture(grcTexture *pNewDiffuseTex)
{
	this->m_varhDiffuseTex = pNewDiffuseTex;
}

inline grcTexture* CCustomShaderEffectInterior::GetDiffuseTexture() const
{
	return (grcTexture*)this->m_varhDiffuseTex;
}

//
//
//
//
inline void CCustomShaderEffectInterior::SelectTintPalette(u8 pal, fwEntity* pEntity)
{
	if(m_pTint)
	{
		m_pTint->SelectTintPalette(pal, pEntity);
	}
}

inline u32	CCustomShaderEffectInterior::GetMaxTintPalette() const
{
	if(m_pTint)
	{
		return m_pTint->GetMaxTintPalette();
	}

	return 0;
}

#endif //__CCUSTOMSHADEREFFECTINTERIOR_H__...

