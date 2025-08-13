//
// Filename:	CustomShaderEffectPed.h
// Description:	Class for controlling damage display in ped shaders;
// Written by:	Andrzej
//
//	10/08/2005	-	Andrzej:	- initial;
//	30/09/2005	-	Andrzej:	- update to Rage WR146;
//
//
//
//
#ifndef __CCUSTOMSHADEREFFECTPED_H__
#define __CCUSTOMSHADEREFFECTPED_H__

#include "shaders\CustomShaderEffectBase.h"

#include "grblendshapes/blendshapes_config.h"
#include "grblendshapes/manager.h"
#include "rmcore/drawable.h"
#include "../shader_source/Peds/ped_common_values.h"
#include "vector/color32.h"
#include "paging/ref.h"
#include "Peds/rendering/PedVariationDS.h"	// PV_MAX_COMP
#include "grcore/effect.h"

#define PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM		    (32)	// no of palette rows in one palette texture 
#define PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM_LARGE    (64)	// no of palette rows in one palette texture 


#define	CSE_PED_EDITABLEVALUES						(__BANK)


#define MAX_PED_COMPONENTS							(PV_MAX_COMP)

namespace rage {
class crCreature; 
}

//
//
//
//
class CCustomShaderEffectPedType: public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectPed;

public:
	CCustomShaderEffectPedType() : CCustomShaderEffectBaseType(CSE_PED) {}
	virtual bool						Initialise(rmcDrawable *pDrawable);
	virtual CCustomShaderEffectBase*	CreateInstance(CEntity *pEntity);
#if __DEV
	virtual bool						AreShadersValid(rmcDrawable* pDrawable);
#endif
protected:
	virtual ~CCustomShaderEffectPedType() {}
private:
	u8									m_idVarDiffuseTex;
	u8									m_idVarDiffuseTexPal;
	u8									m_idVarDiffuseTexPalSelector;
	u8									m_idVarMaterialColorScale;

	u8									m_idVarWrinkleMaskStrengths0;
	u8									m_idVarWrinkleMaskStrengths1;
	u8									m_idVarWrinkleMaskStrengths2;
	u8									m_idVarWrinkleMaskStrengths3;

	u8									m_idVarWrinkleMaskStrengths4;
	u8									m_idVarWrinkleMaskStrengths5;
	u8									m_idVarEnvEffFatSweatScale;
	u8									m_idVarEnvEffColorModCpvAdd;

	u8									m_idVarPedBloodTarget;
	u8									m_idVarPedTattooTarget;
	u8									m_idVarPedDamageData;
	u8									m_idVarPedDamageColors;

	u8									m_idVarWetClothesData;
	u8									m_idVarWetnessAdjust;
	u8									m_idVarStubbleGrowthDetailScale;
	u8									m_idVarUmGlobalOverrideParams;
	u8									m_idVarAlphaToCoverageScale;
#if __XENON || RSG_PC || RSG_DURANGO || RSG_ORBIS
	u8									m_idVarClothParentMatrix;
#endif // __XENON || RSG_PC || RSG_DURANGO || RSG_ORBIS
#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	u8									m_idVarClothVertices;
#endif // RSG_PC || RSG_DURANGO || RSG_ORBIS
	u8									m_idVarNormalTex;
	u8									m_idVarSpecularTex;

	u8									m_bIsPaletteShader	:1;
	u8									m_bIsFurShader		:1;
	u8									m_bPad0				:6;
};

//
//
//
//
class CCustomShaderEffectPed : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectPedType;
public:
	CCustomShaderEffectPed(u32 size);
	~CCustomShaderEffectPed();

public:
	static void						InitClass();
	static void						EnableShaderVarCaching(bool value);
private:
	static void						InvalidateShaderVarCache();
public:
	virtual void					SetShaderVariables(rmcDrawable* pDrawable);
	virtual void					AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void					AddToDrawListAfterDraw()		{/*do nothing*/}
	virtual u32						AddDataForPrototype(void * address);

	virtual void					Update(fwEntity *pEntity); 
	// update componenets of the creature
	void							UpdateExpression(crCreature* pCreature, grbTargetManager* pManager);
 
	// called from CPedVariationMgr:
#if __XENON || __WIN32PC || RSG_DURANGO || RSG_ORBIS
	void							PedVariationMgr_SetShaderVariables(rmcDrawable *pDrawable, s32 component, const Matrix34 *pClothParentMatrix = NULL);
#else
	void							PedVariationMgr_SetShaderVariables(rmcDrawable *pDrawable, s32 component);
#endif	
#if __WIN32PC || RSG_DURANGO || RSG_ORBIS
	void							PedVariationMgr_SetShaderClothVertices(rmcDrawable *pDrawable, u32 numVertices, const Vec3V *pClothVertices );
#endif
	void PedVariationMgr_SetWetnessAdjust(int wetness);

	// external access to internal variables (called from CPedVariationMgr::SetModelShaderTex()):
	void							ClearTextures(s32 index);
	void							ClearShaderVars(rmcDrawable *pDrawable, s32 component);
	void							ClearGlobalShaderVars();
	void							SetDiffuseTexture(grcTexture *pNewDiffuseTex, s32 component);
	grcTexture*						GetDiffuseTexture(s32 component);

	bool							GetRenderBurnout() const								{ return((this->m_bRenderBurnout) || (this->m_bRenderVehBurnout));	}
	void							SetBurnoutLevel(float b)								{ m_varBurnoutLevel = (u8)MIN(u32(b*255.0f),255);	}
	float							GetBurnoutLevel() const									{ return(m_varBurnoutLevel/255.0f);				}

	void							SetEmissiveScale(float e)								{ m_varEmissiveScale = (u8)MIN(u32(e*255.0f),255);	}
	float							GetEmissiveScale() const								{ return(m_varEmissiveScale/255.0f);				}

	inline void						SetDiffuseTexturePal(grcTexture *pNewDiffuseTexPal, s32 component);
	inline grcTexture*				GetDiffuseTexturePal(s32 component);
	inline void						SetDiffuseTexturePalSelector(u8 palsel, s32 component, bool largePal);
	inline u8						GetDiffuseTexturePalSelector(s32 component);
    inline bool                     GetDiffuseTexturePalLarge(s32 component);

	void							SetNormalTexture(grcTexture *pNewNormalTex, s32 component);
	grcTexture*						GetNormalTexture(s32 component);

	void							SetSpecularTexture(grcTexture *pNewSpecularTex, s32 component);
	grcTexture*						GetSpecularTexture(s32 component);

	// enveff scale:
	inline void						SetEnvEffScale(float scl);
	inline float 					GetEnvEffScale() const;
	inline void						SetEnableEnvEffScale(bool enable);
	inline bool						GetEnableEnvEffScale() const;

	// fat scale:
	inline void						SetFatScale(float scl);
	inline float 					GetFatScale();

	// normal map blend:
	inline void						SetNormalMapBlend(float scl);
	inline float 					GetNormalMapBlend();

	// stubble growth:
	inline void						SetStubbleGrowth(float stubble);
	inline float 					GetStubbleGrowth();

	// sweat scale:
	inline void						SetSweatScale(float sweat);
	inline float					GetSweatScale();

	// enveff color modulator:
	inline void						SetEnvEffColorModulator(Color32 c);
	inline void						SetEnvEffColorModulator(u8 r, u8 g, u8 b);				// <0; 255>
	inline void						SetEnvEffColorModulatorf(float rf, float gf, float bf); // <0; 1>
	inline Color32					GetEnvEffColorModulator();

	// enveff cpv add:
	inline void						SetEnvEffCpvAdd(float cpvAdd);
	inline float					GetEnvEffCpvAdd();

	inline u8						GetHeat() const;
	inline bool						GetRenderDead() const;
	inline bool						GetRenderVehBurnout() const;

	// um global override params:
	void							SetUmGlobalOverrideParams(float scaleH, float scaleV, float argFreqH, float argFreqV);
	void							SetUmGlobalOverrideParams(float s);
	void							GetUmGlobalOverrideParams(float *pScaleH, float *pScaleV, float *pArgFreqH, float *pArgFreqV);

	// x = lower wet level, y = higher wet level, z = lower wet amount, w = higher wet amount
	void							SetWetClothesData(Vector4::Vector4Param wet)					{ m_varWetClothesData=wet;}
	const Vector4 &					GetWetClothesData() const 										{ return m_varWetClothesData;}
	void							UpdatePedDamage(const CPed* pPed);

	// local wind vector:
	inline void						GetLocalWindVec(Vector3 &vOut) const;

	void							SetSecondHairPalSelector(u8 selector) { m_secondHairPalSelector = selector; }

public:
	// special version for PedVariationMgr:
	static CCustomShaderEffectBaseType* SetupForDrawableComponent(rmcDrawable* pDrawable, s32 componentID, CCustomShaderEffectBaseType *pShaderEffectType);

#if CSE_PED_EDITABLEVALUES
	static bool						InitWidgets(bkBank& bank);
	static void						SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable, CCustomShaderEffectPedType* pType = NULL, float pedDetailScale = 0.0f);
	static bool						SaveCurrentShaderSettingsToFile();
#endif //CSE_PED_EDITABLEVALUES...

	bool							IsPaletteShader(void)		{return(m_pType->m_bIsPaletteShader);}

	bool							IsFurShader(void)			{return(m_pType && m_pType->m_bIsFurShader);}
	// Utility function: Calculate the heat of a given ped.
	static float					GetPedHeat(CPed *ped, u32 thermalBehaviour);
	
private:
#if ENABLE_BLENDSHAPES
	void							SetupBlendshapeWeights(rmcDrawable *pDrawable, s32 componentID);
#endif // ENABLE_BLENDSHAPES

	Vector4							m_varWetClothesData;		// used to set wet peds data 
	Vector4							m_PedDamageData;

	Float16Vec4						m_umGlobalOverrideParams;
	s16								m_vLocalWind[3];
	u8								m_secondHairPalSelector;
	u8								m_pad_u8				:7;
	grcTextureHandle				m_PedTattooTargetOverride;
	
	// local instances of variables:
	grcTextureIndex					m_varhDiffuseTex[MAX_PED_COMPONENTS];
	grcTextureIndex					m_varhDiffuseTexPal[MAX_PED_COMPONENTS];
	u8								m_varDiffuseTexPalSelector[MAX_PED_COMPONENTS];

	grcTextureIndex					m_varhNormalTex[MAX_PED_COMPONENTS];
	grcTextureIndex					m_varhSpecularTex[MAX_PED_COMPONENTS];

	CCustomShaderEffectPedType		*m_pType;
	datRef<grbTargetManager>		m_Manager;
	grcTextureHandle				m_PedBloodTarget;	 // could be a streaming texture for compressed decorations	
	grcTextureHandle				m_PedTattooTarget;

	// r = m_varEnvEffScale:	<0.0f; 1.0f>
	// g = m_varFatScale:		<0.0f; 1.0f>
	// b = m_varStubbleGrowth:	<0.0f; 1.0f>
	// a = sweat scale:			<0.0f; 1.0f>
	Color32							m_varScaleCombo0;

	// rgb	= envEffColorMod: envEff texture color modulator
	// a	= envEffCpvAdd
	Color32							m_varEnvEffColorModCpvAdd;

	u8								m_wrinkleMaskStrengths[4][6][4];

	u8								m_PedDetailScale;
	u8								m_varHeat;
	u8								m_varBurnoutLevel;
	u8								m_varEmissiveScale;
	u8								m_bRenderBurnout		:1;	// if true, render ped as burnout (from fire)
	u8								m_bRenderVehBurnout		:1;	// if true, render ped as burnout (from exploded vehicle)
	u8								m_bRenderDead			:1; // if true, render ped as dead crispy critter
	u8								m_bTriggeredSmokeFx		:1;
	u8								m_bHasBlendshapes		:1;
	u8								m_bCustomEnvEffScale	:1;	// if true, then EnvEffScale is set directly from script (and not from PopCycle)
	u8								m_useCutsceneTech		:1;
	u8								m_usePedDamageInMirror	:1; // only the player ped damage is enable in the mirror pass, and only under certain conditions.

	typedef struct _BlendWeight
	{
		u16 m_FixedWeight;
		u16 m_Id;
	} BlendWeight;

	static const unsigned MAX_PED_BLEND_WEIGHTS = 100;
	typedef struct _ComponentBlendWeights
	{
		//atArray<BlendWeight> m_BlendWeights;
		atFixedArray<BlendWeight,MAX_PED_BLEND_WEIGHTS> m_BlendWeights;
		bool m_Exists;
		bool m_Processed;
	} ComponentBlendWeights;

private:
	struct CCachedCSEPedShaderVars
	{
		Vec4V				MaterialColorScale[2];
		Vec4V				WetClothesData;
		Vec4V				WetnessAdjust;
		Vec4V				PedDamageData;
		Vec4V				PedDamageColors[3];
		Vec4V				EnvEffFatSweatScale;
		Vec4V				EnvEffColorMod_CpvAdd;
		Vec4V				WrinkleMaskStrenghts[6];
		Float16Vec4			umGlobalOverrideParams;
		Vec2V				StubbleGrowthDetScale;
		grcTextureHandle	PedBloodTarget;
		grcTextureHandle	PedTattooTarget;
		grcTextureIndex		DiffuseTexPal;
		u8					DiffuseTexPalSelector;
	};
	static CCachedCSEPedShaderVars	ms_CachedCSEPedShaderVars[NUMBER_OF_RENDER_THREADS];
	static bool						ms_EnableShaderVarCaching[NUMBER_OF_RENDER_THREADS];


private:
#if CSE_PED_EDITABLEVALUES
	static bool						ms_bEVEnabled;
	static float					ms_fEVSpecFalloff;
	static float					ms_fEVSpecIntensity;
	static float					ms_fEVSpecFresnel;
	static float					ms_fEVReflectivity;
	static float					ms_fEVBumpiness;
	static float					ms_fEVBurnoutLevelSpeed;
	static float					ms_fEVDetailIntensity;
	static float					ms_fEVDetailBumpIntensity;
	static float					ms_fEVDetailScale;
	static float					ms_fEVEnvEffScale;
	static float					ms_fEVEnvEffCpvAdd;
	static float					ms_fEVEnvEffThickness;
	static Color32					ms_fEVEnvEffColorMod;
	static float					ms_fEVEnvEffZoneScale;
	static u32						ms_fEVEnvEffZoneColorModR;
	static u32						ms_fEVEnvEffZoneColorModG;
	static u32						ms_fEVEnvEffZoneColorModB;
	static float					ms_fEVFatScale;
	static float					ms_fEVFatThickness;
	static float					ms_fEVSweatScale;
	static float					ms_fEVNormalBlend;
	static float					ms_fEVStubbleStrength;
	static float					ms_fEVStubbleIndex;
	static float					ms_fEVStubbleTile;
	static Vector4					ms_fEVUmGlobalParams;
	static bool						ms_bEVUseUmGlobalOverrideValues;
	static Vector4					ms_fEVUmGlobalOverrideParams;
	
	static bool						ms_bEVWindMotionEnabled;
	static float					ms_fEVWindMotionAmpX;
	static float					ms_fEVWindMotionAmpY;
	static float					ms_fEVWindMotionFreq;
	static bool						ms_bEVWindResponseEnabled;
	static float					ms_fEVWindResponseVelMin;
	static float					ms_fEVWindResponseVelMax;
	static float					ms_fEVWindFlutterIntensity;

	static Vector2					ms_fEVAnisotropicSpecularIntensity;
	static Vector2					ms_fEVAnisotropicSpecularExponent;
	static float					ms_fEVAnisotropicAlphaBias;
	static Color32					ms_fEVAnisotropicSpecularColour;
	static Vector4					ms_fEVSpecularNoiseMapUVScaleFactor;
#endif //CSE_PED_EDITABLEVALUES...

	static Vector4					ms_fEVWetClothesAdjust;
	static Vector4					ms_fEVWetClothesAdjustMoreWet;
	static Vector4					ms_fEVWetClothesAdjustLessWet;

	static Vector2					ms_DetailFadeOut[2];
#if __BANK
public:
	// various external debug stuff:
	static bool						ms_bBlendshapeForceAll;
	static bool						ms_bBlendshapeForceSpecific;
	static int						ms_nBlendshapeForceSpecificIdx;
	static float					ms_fBlendshapeForceSpecificBlend;

	static bool						ms_wrinkleEnableOverride;
	static bool						ms_wrinkleEnableSliders;
	static Vector4					ms_wrinkleMaskStrengths[6];
	static atArray<const char*>		ms_wrinkleDebugComponentStrings;
	static int						ms_wrinkleDebugComponent;
#endif //__BANK...
};


//
//
//
//
inline
void CCustomShaderEffectPed::ClearTextures(s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	this->m_varhDiffuseTex[index] = NULL;
	this->m_varhDiffuseTexPal[index] = NULL;
	this->m_varhNormalTex[index] = NULL;
	this->m_varhSpecularTex[index] = NULL;
}

inline
void CCustomShaderEffectPed::SetDiffuseTexture(grcTexture *pNewDiffuseTex, s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	this->m_varhDiffuseTex[index] = pNewDiffuseTex;
}

inline
grcTexture* CCustomShaderEffectPed::GetDiffuseTexture(s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	return m_varhDiffuseTex[index];
}

inline
void CCustomShaderEffectPed::SetDiffuseTexturePal(grcTexture *pNewDiffuseTexPal, s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	this->m_varhDiffuseTexPal[index] = pNewDiffuseTexPal;
}

inline
grcTexture* CCustomShaderEffectPed::GetDiffuseTexturePal(s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	return this->m_varhDiffuseTexPal[index];
}

inline
void CCustomShaderEffectPed::SetDiffuseTexturePalSelector(u8 palsel, s32 index, bool largePal)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	Assert(palsel < PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM || (largePal && palsel < PED_COLOR_VARIATION_TEXTURE_PAL_ROWNUM_LARGE));
    Assert((palsel & (1 << 7)) == 0);
	this->m_varDiffuseTexPalSelector[index] = palsel | ((u8)largePal << 7);
}

inline
u8 CCustomShaderEffectPed::GetDiffuseTexturePalSelector(s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	return m_varDiffuseTexPalSelector[index] & 0x7f;
}

inline
bool CCustomShaderEffectPed::GetDiffuseTexturePalLarge(s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	return (m_varDiffuseTexPalSelector[index] >> 7) != 0;
}

inline
void CCustomShaderEffectPed::SetNormalTexture(grcTexture *pNewNormalTex, s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	this->m_varhNormalTex[index] = pNewNormalTex;
}

inline
grcTexture* CCustomShaderEffectPed::GetNormalTexture(s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	return m_varhNormalTex[index];
}

inline
void CCustomShaderEffectPed::SetSpecularTexture(grcTexture *pNewSpecularTex, s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	this->m_varhSpecularTex[index] = pNewSpecularTex;
}

inline
grcTexture* CCustomShaderEffectPed::GetSpecularTexture(s32 index)
{
	Assert((index >= 0) && (index < MAX_PED_COMPONENTS)); 
	return m_varhSpecularTex[index];
}

//
// enveff scale:
//
inline void CCustomShaderEffectPed::SetEnvEffScale(float scl)
{
	this->m_varScaleCombo0.SetRed(int(scl*255.0f));
}

inline float CCustomShaderEffectPed::GetEnvEffScale() const
{
	return this->m_varScaleCombo0.GetRedf();
}

inline void CCustomShaderEffectPed::SetEnableEnvEffScale(bool enable)
{
	this->m_bCustomEnvEffScale = enable;
}

inline bool CCustomShaderEffectPed::GetEnableEnvEffScale() const
{
	return this->m_bCustomEnvEffScale;
}

//
// fat scale:
//
inline void CCustomShaderEffectPed::SetFatScale(float scl)
{
	this->m_varScaleCombo0.SetGreen(int(scl*255.0f));
}

inline float CCustomShaderEffectPed::GetFatScale()
{
	return this->m_varScaleCombo0.GetGreenf();
}

// local wind vector:
#define PED_WIND_FIXED_POINT (10)
inline void CCustomShaderEffectPed::GetLocalWindVec(Vector3 &vOut) const
{
	vOut[0] = float(m_vLocalWind[0])/float(1<<PED_WIND_FIXED_POINT);
	vOut[1] = float(m_vLocalWind[1])/float(1<<PED_WIND_FIXED_POINT);
	vOut[2] = float(m_vLocalWind[2])/float(1<<PED_WIND_FIXED_POINT); 
}

//
// stubble growth:
//
inline void CCustomShaderEffectPed::SetStubbleGrowth(float stubble)
{
	this->m_varScaleCombo0.SetBlue(int(stubble*255.0f));
}

inline float CCustomShaderEffectPed::GetStubbleGrowth()
{
	return this->m_varScaleCombo0.GetBluef();
}

//
// sweat scale:
//
inline void CCustomShaderEffectPed::SetSweatScale(float sweat)
{
	this->m_varScaleCombo0.SetAlpha(int(sweat*255.0f));
}

inline float CCustomShaderEffectPed::GetSweatScale()
{
	return this->m_varScaleCombo0.GetAlphaf();
}

//
// enveff color modulator:
//
inline void CCustomShaderEffectPed::SetEnvEffColorModulator(Color32 c)
{
	this->m_varEnvEffColorModCpvAdd.Set(c.GetRed(), c.GetGreen(), c.GetBlue(), m_varEnvEffColorModCpvAdd.GetAlpha());
}

inline void CCustomShaderEffectPed::SetEnvEffColorModulator(u8 r, u8 g, u8 b)
{
	this->m_varEnvEffColorModCpvAdd.Set(r, g, b, m_varEnvEffColorModCpvAdd.GetAlpha());
}

inline void CCustomShaderEffectPed::SetEnvEffColorModulatorf(float rf, float gf, float bf)
{
	this->m_varEnvEffColorModCpvAdd.Setf(rf, gf, bf, m_varEnvEffColorModCpvAdd.GetAlphaf());
}

inline Color32 CCustomShaderEffectPed::GetEnvEffColorModulator()
{
	Color32 c(m_varEnvEffColorModCpvAdd);
	c.SetAlpha(255);
	return c;
}

inline void CCustomShaderEffectPed::SetEnvEffCpvAdd(float cpvAdd)
{
	this->m_varEnvEffColorModCpvAdd.SetAlpha(int(cpvAdd*255.0f));
}

inline float CCustomShaderEffectPed::GetEnvEffCpvAdd()
{
	return this->m_varEnvEffColorModCpvAdd.GetAlphaf();
}

inline u8 CCustomShaderEffectPed::GetHeat() const
{
	return this->m_varHeat;
}

inline bool CCustomShaderEffectPed::GetRenderDead() const
{
	return this->m_bRenderDead;
}

inline bool CCustomShaderEffectPed::GetRenderVehBurnout() const
{
	return this->m_bRenderVehBurnout;
}

#endif //__CCUSTOMSHADEREFFECTPED_H__...

