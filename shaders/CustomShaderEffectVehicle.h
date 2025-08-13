//
// Filename:	CustomShaderEffectVehicle.h
// Description:	Takes care of all gta_vehicle shader variables;
// Written by:	Andrzej
//
//	07/10/2005	-	Andrzej:	- initial;
//
//
//
//
#ifndef __CCUSTOMSHADEREFFECTVEHICLE_H__
#define __CCUSTOMSHADEREFFECTVEHICLE_H__

#include "grcore/effect.h"

#include "CustomShaderEffectBase.h"
#include "vehicles/VehicleDefines.h"
#include "vehicles/vehicleLightSwitch.h"
#include "vfx/vehicleglass/VehicleGlass.h"
#include "vfx/vfx_shared.h"
#if __ASSERT
#include "phbound/surfacegrid.h"
#endif

#define	CSE_VEHICLE_EDITABLEVALUES					(__BANK)
#define USE_DISKBRAKEGLOW							(0)
#define USE_GPU_VEHICLEDEFORMATION					(!USE_EDGE)

class CCustomShaderEffectVehicle;
//
//
//
//
class CCustomShaderEffectVehicleType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectVehicle;
	friend class CVehicleGlassShaderData;
public:
	CCustomShaderEffectVehicleType() : CCustomShaderEffectBaseType(CSE_VEHICLE) {}
	static CCustomShaderEffectVehicleType*	Create(rmcDrawable *pDrawable, CVehicleModelInfo *pVehicleMI, grmShaderGroup* pClonedShaderGroup);
	bool									Recreate(rmcDrawable *pDrawable, CVehicleModelInfo *pVehicleMI);

	virtual bool							Initialise(rmcDrawable*);
			bool							Initialise(rmcDrawable *pDrawable, grmShaderGroup *pClonedShaderGroup);
	virtual CCustomShaderEffectBase*		CreateInstance(CEntity *pEntity);
	virtual bool							RestoreModelInfoDrawable(rmcDrawable *pDrawable);
	rmcDrawable*							InitialiseClothDrawable(CEntity *pEntity) const;

	void									SetIsHighDetail(bool flag)		{ m_bIsHighDetail = flag; }
	bool									GetIsHighDetail() const			{ return m_bIsHighDetail; }
	Color32									GetDefaultDirtColor() const		{ return m_DefDirtColor;  }

	grcTexture*								GetDirtTexture(u32 shaderIdx) const;

	bool									HasBodyColor1() const			{ return m_bHasBodyColor1;}
	bool									HasBodyColor2() const			{ return m_bHasBodyColor2;}
	bool									HasBodyColor3() const			{ return m_bHasBodyColor3;}
	bool									HasBodyColor4() const			{ return m_bHasBodyColor4;}
	bool									HasBodyColor5() const			{ return m_bHasBodyColor5;}

	inline CCustomShaderEffectVehicleType*	SwapType(CCustomShaderEffectVehicle* pCse);

protected:
	virtual ~CCustomShaderEffectVehicleType();

private:
	Color32							m_DefDirtColor;			// default dirt color
	float							m_DefEmissiveMult;		// default emissive multiplier

private:
	u8								m_idVarSpec2Color;
	u8								m_idVarDiffuse2Color;
	u8								m_idVarDirtLevelMod;
	u8								m_idVarDirtColor;

	u8								m_idVarDiffuseColorTint;
	u8								m_idVarDiffuseTex2;			// controls liveries texture
	u8								m_idVarDiffuseTex3;			// controls liveries2 texture
	u8								m_idVarTyreDeformSwitchOn;	// controls tyre deformation:

	u8								m_idVarTrackAnimUV;			// tracks uv animation:
	u8								m_idVarTrack2AnimUV;		// tracks uv animation:
	u8								m_idVarTrackAmmoAnimUV;		// tracks uv animation
	u8								m_idVarEnvEffScale;

	u8								m_idLicensePlateBgTex;		//License Plate background diffuse texture var
	u8								m_idLicensePlateBgNormTex;	//License Plate background diffuse texture var
	u8								m_idLicensePlateFontColor;	//License Plate font color var
	u8								m_idLicensePlateText1;		//License Plate Text Var: Letters 0-3

	u8								m_idLicensePlateText2;		//License Plate Text Var: Letters 4-7
	u8								m_idLicensePlateFontOutlineColor;				//License Plate font outline color var
	u8								m_idLicensePlateFontExtents;					//License Plate font extents var
	u8								m_idLicensePlateFontOutlineMinMaxDepth_Enabled;	//License Plate font outline min/max depth and outline enabled var

	u8								m_idLicensePlateMaxLettersOnPlate;				//License Plate font max letters on plate.
	u8								m_idVarDimmer;				// controls headlights etc.
	u8								m_idVarEmissiveMult;
	// controls vehicle deformation
#if USE_GPU_VEHICLEDEFORMATION
	u8								m_idVarDamageSwitchOn;
	u8								m_idVarBoundRadius;
	u8								m_idVarDamageMult;

	u8								m_idVarDamageTex;
	u8								m_idVarOffset;
	u8								m_idVarDamagedFrontWheelOffsets;
#if __BANK
	u8								m_idVarDebugDamageMap;
	u8								m_idVarDebugDamageMult;
#endif
#endif
#if USE_DISKBRAKEGLOW
	u8								m_idVarDiskBrakeGlow;
#endif

	u8								m_bHasTruckTex	:1;
	u8								m_bIsHighDetail	:1;
	u8								m_bHasBodyColor1:1;	// true if BODYCOLOR1 is mapped onto this vehicle
	u8								m_bHasBodyColor2:1;
	u8								m_bHasBodyColor3:1;
	u8								m_bHasBodyColor4:1;
	u8								m_bHasBodyColor5:1;
	u8								m_bHasBodyColor6:1;
	u8								m_bHasBodyColor7:1; 

	// remapping of diffuse color:
	// simple structure to tie together Effect with DiffuseColor variable;
	struct structDiffColEffect
	{
		u8			idvarDiffColor;
		u8			colorIdx;			// color mapping (col1, ..., col4)
		u8			shaderIdx;
#if VEHICLE_SUPPORT_PAINT_RAMP
		u8			idvarDiffuseRampTexture;
		u8			idvarSpecularRampTexture;
		u8			idvarDiffuseSpecularRampEnabled;
		u8			m_pad[2];
#else
		u8			m_pad;
#endif
	};
	atArray<structDiffColEffect> m_DiffColEffect;

	// storing original specularColor for vehicles (used to restore specular after burnout vehicles):
	struct structSpecColEffect
	{
		float		origSpecInt;		// original values (as exported)
		float		origSpecFalloff;
		float		origSpecFresnel;

		u8			idvarSpecInt;
		u8			idvarSpecFalloff;
		u8			idvarSpecFresnel;
		u8			colorIdx;			// color mapping (metallic settings)
		u8			shaderIdx;
		u8			m_pad[3];
	};
	atArray<structSpecColEffect> m_SpecColEffect;

	// storing original dirtTexture for vehicles (used to restore dirt after burnout vehicles):
	struct structDirtTexEffect
	{
		grcTextureIndex pOrigDirtTex;		// original value (as exported)
		u8				idvarDirtTex;
		u8				shaderIdx;
		u8				bIsInterior	:1;
		u8				m_flagpad	:7;
		u8				m_pad[3];
	};
	atArray<structDirtTexEffect> m_DirtTexEffect;

	// storing original diffTexture2 for vehicles (used to restore original overlay texture):
	struct structTex2Effect
	{
		grcTextureIndex pOrigTex2;		// original value (as exported)
		u8				idvarTex2;
		u8				shaderIdx;
	};
	atArray<structTex2Effect> m_Tex2Effect;

private:
	// these are shared among every instance of the class:
	static grcTextureIndex			ms_pBurnoutTexture;		// standard burnout texture
	static grcTextureIndex			ms_pBurnoutIntTexture;	// interior burnout texture
	static grcTextureIndex			ms_pBurnoutTruckTexture;// interior burnout texture
	static s32						ms_nBurnoutTruckTextureRef;
};

//
//
//
//
class CCustomShaderEffectVehicle : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectVehicleType;
	friend class CVehicleGlassShaderData;
private:
	CCustomShaderEffectVehicle();
public:
	~CCustomShaderEffectVehicle();

public:
	virtual void				SetShaderVariables(rmcDrawable* pDrawable);
	void						SetShaderVariables(rmcDrawable* pDrawable, Vector3 boneOffset, bool isBrokenOffPart = false);
	virtual void				AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void				AddToDrawListAfterDraw()		{/*do nothing*/}
	virtual void				Update(fwEntity *pEntity);

public:
	bool						UpdateVehicleColors(CEntity *pEntity);
	void						CopySettings(CCustomShaderEffectVehicle *src, CEntity *pEntity);

	const CCustomShaderEffectVehicleType* GetCseType() const { return (g_IsSubRenderThread ? m_pType[g_RenderThreadIndex] : m_pType[0]); }
private:
	CCustomShaderEffectVehicleType* GetCseTypeInternal() const { return (g_IsSubRenderThread ? m_pType[g_RenderThreadIndex] : m_pType[0]); }
public:

#if CSE_VEHICLE_EDITABLEVALUES
	static bool					InitWidgets(bkBank& bank);
	void						SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable);

	// editable inner wheel radius stuff:
	static bool					GetEWREnabled()				{ return(ms_bEWREnabled);			}
	static bool					GetEWRTyreEnabled()			{ return(ms_bEWRTyreEnabled);		}
	static float				GetEWRInnerRadius()			{ return(ms_fEWRInnerRadius*0.1f);	}	// widgets can only display in 1.3f format (we need 4 digits after dot)

	// Editable diffuse tint
	static bool					GetEVOverrideDiffuseColorTint() { return ms_bEVEnabled && ms_fEVOverrideDiffuseColorTint; }
	static void					GetEVDiffuseColorTint(Vector4& vDiffuseColorTint)	{ vDiffuseColorTint.Set(ms_fEVDiffuseColorTint.x, ms_fEVDiffuseColorTint.y, ms_fEVDiffuseColorTint.z, ms_fEVDiffuseColorTintAlpha); }
#endif //CSE_VEHICLE_EDITABLEVALUES...

	// light stuff access functions
	inline float				GetLightValue(const CVehicleLightSwitch::LightId light) const					{ return m_fLightDimmer[light]; }
	inline void					SetLightValue(const CVehicleLightSwitch::LightId light, const float value)		{ m_fLightDimmer[light] = value; }
	inline void					SetLightValueMax(const CVehicleLightSwitch::LightId light, const float value)	{ m_fLightDimmer[light] = rage::Max(m_fLightDimmer[light], value); }
	inline void					ClearAllLights(void)															{ sysMemSet(m_fLightDimmer,0,sizeof(m_fLightDimmer)); }
	inline void					SetAllLights(const float *valueTable)											{ sysMemCpy(m_fLightDimmer,valueTable,sizeof(m_fLightDimmer)); }
	inline void					SetEnableDamage(bool bEnable)													{ m_bDamageSwitchOn = bEnable;}
	inline bool					GetEnableDamage() const															{ return(m_bDamageSwitchOn); }
	inline void					SetDamageTex(grcTexture* pTex)													{ m_pDamageTexture = pTex; }
	inline grcTexture*			GetDamageTex() const															{ return m_pDamageTexture; }
	inline void					SetBoundRadius(float boundRadius)												{ m_fBoundRadius = boundRadius; }
	inline float				GetBoundRadius() const															{ return m_fBoundRadius; }
#if USE_GPU_VEHICLEDEFORMATION
	inline void					SetDamageMultiplier(float multiplier)											{ m_fDamageMultiplier = multiplier; }
	inline float				GetDamageMultiplier() const														{ return m_fDamageMultiplier; }
	inline void					SetDamagedFrontWheelOffsets(const Vector4& vFLWheelOffset, const Vector4& vFRWheelOffset) {
		Assertf(!IsNanInMemory(&vFLWheelOffset.x) && !IsNanInMemory(&vFLWheelOffset.y) && !IsNanInMemory(&vFLWheelOffset.z) &&
			!IsNanInMemory(&vFRWheelOffset.x) && !IsNanInMemory(&vFRWheelOffset.y) && !IsNanInMemory(&vFRWheelOffset.z),
			"NaN detected in SetDamagedFrontWheelOffsets, [%3.2f,%3.2f,%3.2f], [%3.2f,%3.2f,%3.2f]", 
			vFLWheelOffset.x, vFLWheelOffset.y, vFLWheelOffset.z, vFRWheelOffset.x, vFRWheelOffset.y, vFRWheelOffset.z);
		m_vDamagedFrontWheelOffsets[0] = vFLWheelOffset; m_vDamagedFrontWheelOffsets[1] = vFRWheelOffset;
	}
	inline const Vector4 *		GetDamagedFrontWheelOffsets() const												{return m_vDamagedFrontWheelOffsets;}
#endif
	inline void                 SetLiveryId(s32 liveryIdx)														{ m_nLiveryIdx = liveryIdx; }
    inline s32					GetLiveryId() const																{ return m_nLiveryIdx; }
	inline void					SetLiveryFrag(strLocalIndex fragSlotIdx)										{ m_liveryFragSlotIdx = fragSlotIdx;}
	inline void                 SetLivery2Id(s32 liveryIdx2)													{ m_nLivery2Idx = liveryIdx2; }
    inline s32					GetLivery2Id() const															{ return m_nLivery2Idx; }
	inline bool					GetIsPrimaryColorCustom() const													{ return m_bCustomPrimaryColor; }
	void						SetCustomPrimaryColor(Color32 col);
	Color32						GetCustomPrimaryColor();
	void						ClearCustomPrimaryColor()														{ m_bCustomPrimaryColor = false; }
	inline bool					GetIsSecondaryColorCustom() const												{ return m_bCustomSecondaryColor; }
	void						SetCustomSecondaryColor(Color32 col);
	Color32						GetCustomSecondaryColor();
	void						ClearCustomSecondaryColor()														{ m_bCustomSecondaryColor = false; }
	void						SetIsBrokenOffPart(bool val)													{ m_bIsBrokenOffPart = val; }
	bool						GetIsBrokenOffPart() const												        { return m_bIsBrokenOffPart; }
	

	inline void					SetEnableTyreDeform(bool bEnable)			{ m_bTyreDeformSwitchOn = bEnable;}
	inline bool					GetEnableTyreDeform() const					{ return(m_bTyreDeformSwitchOn); }

	void						SetRed();
	void						SetGreen();
	void						SetBlue();

	inline bool					HasBodyColor1() const						{ return GetCseType()->HasBodyColor1();		}
	inline bool					HasBodyColor2() const						{ return GetCseType()->HasBodyColor2();		}
	inline bool					HasBodyColor3() const						{ return GetCseType()->HasBodyColor3();		}
	inline bool					HasBodyColor4() const						{ return GetCseType()->HasBodyColor4();		}
	inline bool					HasBodyColor5() const						{ return GetCseType()->HasBodyColor5();		}
	inline bool					GetIsHighDetail() const						{ return GetCseType()->GetIsHighDetail();	}

	void						SelectLicensePlateByProbability(CEntity *pEntity);
	void						GenerateLicensePlateText(u32 seed);
	void						SetLicensePlateText(const char *str);
	const char*					GetLicensePlateText() const;
	void						SetLicensePlateTexIndex(s32 index);
	inline s32					GetLicensePlateTexIndex() const				{ return m_nLicensePlateTexIdx; }

	void						TrackUVAnimSet(const Vector2& a)			{ this->m_uvTrackAnim=a;		}
	void						TrackUVAnimAdd(const Vector2& a);
	Vector2						TrackUVAnimGet() const						{ return this->m_uvTrackAnim;	}

	void						Track2UVAnimSet(const Vector2& a)			{ this->m_uvTrack2Anim=a;		}
	void						Track2UVAnimAdd(const Vector2& a);
	Vector2						Track2UVAnimGet() const						{ return this->m_uvTrack2Anim;	}

	void						TrackAmmoUVAnimSet(const Vector2& a)		{ this->m_uvTrackAmmoAnim=a;	}
	void						TrackAmmoUVAnimAdd(const Vector2& a);
	Vector2						TrackAmmoUVAnimGet() const					{ return this->m_uvTrackAmmoAnim;}

	bool						GetIsBurstWheel() const						{ return this->m_bBurstWheel;											}
	const u8*					GetBurstAndSideRatios(u8 w) const			{ FastAssert(w<NUM_VEH_CWHEELS_MAX); return m_wheelBurstSideRatios[w];	}

private:
	void						SetSpec2Color(u8 r, u8 g, u8 b)				{ m_varSpec2Color_DirLerp.SetBytes(r,g,b,m_varSpec2Color_DirLerp.GetAlpha());	}
	Color32						GetSpec2Color()	const						{ return m_varSpec2Color_DirLerp;												}
	void						SetSpec2DirLerp(float l)					{ m_varSpec2Color_DirLerp.SetAlpha((u8)MIN(u32(l*255.0f), 255));				}
	u8							GetSpec2DirLerp() const						{ return m_varSpec2Color_DirLerp.GetAlpha();									}
	float						GetSpec2DirLerpf() const					{ return m_varSpec2Color_DirLerp.GetAlphaf();									}

	void						SetDirtColor(float r, float g, float b)		{ m_varDirtColor_DirtLevel.Setf(r,g,b,m_varDirtColor_DirtLevel.GetAlphaf());	}
	void						SetDirtColor(u8 r, u8 g, u8 b)				{ m_varDirtColor_DirtLevel.SetBytes(r,g,b,m_varDirtColor_DirtLevel.GetAlpha()	);				}
	void						SetDirtColor(Color32 c)						{ m_varDirtColor_DirtLevel.SetBytes(c.GetRed(),c.GetGreen(),c.GetBlue(),m_varDirtColor_DirtLevel.GetAlpha());								}
	Color32						GetDirtColor() const						{ return m_varDirtColor_DirtLevel;												}
	void						SetDirtLevel(float d)						{ m_varDirtColor_DirtLevel.SetAlpha(int(d*255.0f));}
	float						GetDirtLevel() const						{ return m_varDirtColor_DirtLevel.GetAlphaf();		}

	void						SetDiffuseTint(Color32 tint)						{ m_varDiffColorTint = tint;				}
	void						SetDiffuseTint(u8 r, u8 g, u8 b, u8 a)				{ m_varDiffColorTint.SetBytes(r,g,b,a);		}
	void						SetDiffuseTintf(float r, float g, float b, float a)	{ m_varDiffColorTint.Setf(r,g,b,a);			}	
	Color32						GetDiffuseTint() const						{ return m_varDiffColorTint; }
public:
	void						SetEnvEffScale(float s)						{ u32 is=u32(s*255.0f); m_varEnvEffScale=(u8)MIN(is, 255);	}
	float						GetEnvEffScale() const						{ return float(m_varEnvEffScale)/255.0f;	}

	void						SetEnvEffScaleU8(u8 s)						{ m_varEnvEffScale = s; }
	u8							GetEnvEffScaleU8() const					{ return m_varEnvEffScale; }

	void						SetBurnoutLevel(float b)					{ u32 ib=u32(b*255.0f); m_varBurnoutLevel=(u8)MIN(ib, 255);	}
	float						GetBurnoutLevel() const						{ return float(m_varBurnoutLevel)/255.0f;	}

	void						SetUserEmissiveMultiplier(float m)			{ m_fUserEmissiveMultiplier=m;		}
	float						GetUserEmissiveMultiplier() const			{ return m_fUserEmissiveMultiplier; }

public:
	static void					SetupBurnoutTexture(s32 txdSlot);

#if VEHICLE_SUPPORT_PAINT_RAMP
	static void					LoadRampTxd();
	static void					UnloadRampTxd();
	static fwTxd*				GetRampTxd();
#endif

	strLocalIndex				GetLiveryFragSlotIdx()	const				{ return m_liveryFragSlotIdx; }

private:
	// these are private values of the instance:
	// m_fLightDimmer must be first because it needs 16-byte alignment.  We treat it as a four-element Vector4 array.
	ALIGNAS(16) float				m_fLightDimmer[4*CVehicleLightSwitch::LW_LIGHTVECTORS+1];	// +1 extra last element for invalid boneIDs converted with BONEID_TO_LIGHTID()
	float							m_fBoundRadius;
#if USE_GPU_VEHICLEDEFORMATION
	float							m_fDamageMultiplier;
	Vector4							m_vDamagedFrontWheelOffsets[2];
#endif
	float							m_fUserEmissiveMultiplier;			// <0; 1>

	Color32							m_varDifCol[7];
	Color32							m_varSpec2Color_DirLerp;			// rgb=spec2Color, a=spec2DirLerp
	Color32							m_varDiffColorTint;
	Color32							m_varDirtColor_DirtLevel;			// rgb=dirt color, a=dirt level
	u8								m_varMetallicID[7];

	CCustomShaderEffectVehicleType	*m_pType[NUMBER_OF_RENDER_THREADS];
	grcTextureIndex					m_pDamageTexture;

#if VEHICLE_SUPPORT_PAINT_RAMP
	grcTextureIndex					m_pRampTextures[4];
#endif

	grcTextureIndex					m_pLiveryTexture;
	grcTextureIndex					m_pLivery2Texture;
    s32								m_nLiveryIdx;
    s32								m_nLivery2Idx;

	strLocalIndex					m_liveryFragSlotIdx;

	Vector2							m_uvTrackAnim;		//U track shift: x=right, y=left
	Vector2							m_uvTrack2Anim;		//U track shift: x=right, y=left
	Vector2							m_uvTrackAmmoAnim;	//U track shift: x=right, y=left
	
	//License plate text
public:
	enum {	LICENCE_PLATE_LETTERS_MAX = 8 };
private:
	s32								m_nLicensePlateTexIdx;
	grcTextureIndex					m_pLicensePlateDiffuseTex;
	grcTextureIndex					m_pLicensePlateNormalTex;
	u8								m_LicensePlateText[LICENCE_PLATE_LETTERS_MAX];

	u8								m_wheelBurstSideRatios[NUM_VEH_CWHEELS_MAX][2];	// 0=burst ratio, 1=side ratio
#if USE_DISKBRAKEGLOW
	float							m_varDiskBrakeGlow;
#endif
	u8								m_varEnvEffScale;
	u8								m_varBurnoutLevel;

	u8								m_bRenderScorched		:1;
	u8								m_bTyreDeformSwitchOn	:1;
	u8								m_bBurstWheel			:1;
	u8								m_bDamageSwitchOn		:1;
	u8								m_bCustomPrimaryColor	:1;
	u8								m_bCustomSecondaryColor :1;
	u8								m_bIsBrokenOffPart		:1;
	u8								m_bHasLiveryMod			:1;

/////////////////////////////////////////////////////////////////////////////////////////////////////////

private:
#if CSE_VEHICLE_EDITABLEVALUES
	static bool						ms_bEVEnabled;
	static float					ms_fEVSpecFalloff;
	static float					ms_fEVSpecIntensity;
	static float					ms_fEVSpecTexTileUV;
	static float					ms_fEVSpec2Falloff;
	static float					ms_fEVSpec2Intensity;
	static bool						ms_bEVSpec2DirLerpOverride;
	static float					ms_fEVSpec2DirLerp;
	static float					ms_fEVReflectivity;
	static float					ms_fEVBumpiness;
	static float					ms_fEVSpecFresnel;
	static float					ms_fEVDiffuseTexTileUV;
	static float					ms_fEVDirtLevel;
	static float					ms_fEVDirtModDry;
	static float					ms_fEVDirtModWet;
	static s32						ms_fEVDirtOrBurnout;
	static Vector3					ms_fEVDirtColor;
	static float					ms_fEVEnvEffThickness;
	static float					ms_fEVEnvEffScale;
	static float					ms_fEVEnvEffTexTileUV;
	static bool						ms_fEVOverrideBodyColor;
	static Vector3					ms_fEVBodyColor;
	static bool						ms_fEVOverrideDiffuseColorTint;
	static Vector3					ms_fEVDiffuseColorTint;
	static float					ms_fEVDiffuseColorTintAlpha;

	// editable inner wheel radius stuff:
	static bool						ms_bEWREnabled;
	static bool						ms_bEWRTyreEnabled;	// on/off tire
	static float					ms_fEWRInnerRadius;	// actual inner radius to adjust
	
	// editable sun glare stuff:
	static bool						ms_bEVSunGlareEnabled;	// on/off
	static float					ms_fEVSunGlareDistMin;
	static float					ms_fEVSunGlareDistMax;
	static u32						ms_nEVSunGlarePow;		// pow strength
	static float					ms_fEVSunGlareSpriteSize;
	static float					ms_fEVSunGlareSpriteZShiftScale;
	static float					ms_fEVSunGlareHdrMult;
	
	// editable License plate
	static Vector4					ms_fEVLetterIndex1;
	static Vector4					ms_fEVLetterIndex2;
	static Vector2					ms_fEVLetterSize;
	static Vector2					ms_fEVNumLetters;
	static Vector4					ms_fEVLicensePlateFontExtents;
	static Vector4					ms_fEVLicensePlateFontTint;
	static float					ms_fEVFontNormalScale;
	static float					ms_fEVDistMapCenterVal;
	static Vector4					ms_fEVDistEpsilonScaleMin;
	static Vector3					ms_fEVFontOutlineMinMaxDepthEnabled;
	static Vector3					ms_fEVFontOutlineColor;

	static float					ms_fEVBurnoutSpeed;
#endif //CSE_VEHICLE_EDITABLEVALUES...

#if __DEV
public:
	static s32						ms_nForceColorCars;
#endif
};

//
//
// Use with care:
// sets this CSEType to given CSE instance, returns old type;
//
CCustomShaderEffectVehicleType*	CCustomShaderEffectVehicleType::SwapType(CCustomShaderEffectVehicle* pCse)
{
	Assert(pCse);
	CCustomShaderEffectVehicleType *oldType = NULL;

	if (g_IsSubRenderThread) 
	{
		oldType = pCse->m_pType[g_RenderThreadIndex];
		pCse->m_pType[g_RenderThreadIndex] = this;

	}
	else
	{
		oldType = pCse->m_pType[0];

		for (int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
		{
			pCse->m_pType[i] = this;
		}
	}

	return oldType;
}

class CVehicleGlassShaderData
{
public:
	void GetStandardShaderVariables(const grmShader* pShader);
	void GetCustomShaderVariables(const CCustomShaderEffectVehicle* pCSE);
	void GetDirtTexture(const CCustomShaderEffectVehicle* pCSE, int shaderIndex ASSERT_ONLY(, const char* vehicleModelName));

#if USE_GPU_VEHICLEDEFORMATION
	void GetDamageShaderVariables(const CCustomShaderEffectVehicle* pCSE);
#endif // USE_GPU_VEHICLEDEFORMATION

	// shader vars from vehicle_vehglass.fx
	grcTextureHandle m_DiffuseTex;
	grcTextureHandle m_DirtTex;
	grcTextureHandle m_SpecularTex;

	Vector3 m_SpecularMapIntensityMask;
	float   m_EnvEffTexTileUV;
	float   m_EnvEffThickness;
//	Vector3 m_DiffuseColor; // -- removed

	// shader vars from vehicle CSE
	Vector4 m_DirtLevelMod;
	Vector4 m_DirtColor;
	Vector4 m_DiffuseColorTint;
	Vector2 m_EnvEffScale0;

#if USE_GPU_VEHICLEDEFORMATION
	bool m_bDamageOn;
	float m_fBoundRadius;
	float m_fDamageMultiplier;
	grcTextureHandle m_pDamageTex;
#endif // USE_GPU_VEHICLEDEFORMATION
};

class CVehicleGlassShader
{
public:
	CVehicleGlassShader() : m_shader(NULL) {}

	void Create(grmShader* pShader);
	void Shutdown();

	void SetShaderVariables(const CVehicleGlassShaderData& data) const;
	void SetTintVariable(const Vector4& diffuseColorTint) const;
	void ClearShaderVariables() const;
	void SetCrackTextureParams(grcTexture* crackTexture, float amount, float scale, float bumpAmount, float bumpiness, Vec4V_In tint) const;

	grmShader* m_shader;

	// shader vars from vehicle_vehglass.fx
	grcEffectVar m_idVarDiffuseTex;
	grcEffectVar m_idVarDirtTex;
	grcEffectVar m_idVarSpecularTex;
	grcEffectVar m_idVarSpecularMapIntensityMask;
	grcEffectVar m_idVarEnvEffTexTileUV;
	grcEffectVar m_idVarEnvEffThickness;
//	grcEffectVar m_idVarDiffuseColor; // -- removed

	// shader vars from vehicle CSE
	grcEffectVar m_idVarDirtLevelMod;
	grcEffectVar m_idVarDirtColor;
	grcEffectVar m_idVarDiffuseColorTint;
	grcEffectVar m_idVarEnvEffScale0;

	// cracked vehicle glass extensions
	grcEffectVar m_idVarCrackTexture;
	grcEffectVar m_idVarCrackTextureParams;

#if USE_GPU_VEHICLEDEFORMATION
	grcEffectGlobalVar m_idVarDamageSwitchOn;
	grcEffectVar m_idVarDamageTex;
	grcEffectVar m_idVarBoundRadius;
	grcEffectVar m_idVarDamageMult;
	grcEffectVar m_idVarOffset;
#endif // USE_GPU_VEHICLEDEFORMATION
};

#endif //__CCUSTOMSHADEREFFECTVEHICLE_H__...

