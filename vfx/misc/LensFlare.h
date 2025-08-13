//
// vfx/misc/LensFlare.h
// 
// Copyright (C) 1999-2012 Rockstar Games.  All Rights Reserved.
//

#ifndef INC_LENS_FLARE
#define INC_LENS_FLARE

// rage
#include "vectormath/vec3v.h"
#include "vector/color32.h"
#include "vector/colors.h"
#include "atl/array.h"
#include "data/callback.h"
#include "parser/macros.h"
#include "grcore/device.h"
#include "grmodel/shader.h"
#include "vector/colors.h"

namespace rage
{
	class grcTexture;

#if __BANK
	class bkGroup;
#endif // __BANK
};

#if RSG_PC
	#define MAX_NUM_OCCLUSION_QUERIES	(/*MAX_FRAMES_RENDER_THREAD_AHEAD_OF_GPU*/ 2 + MAX_GPUS + 1)
	#define ACTIVE_GPUS					(MAX_FRAMES_RENDER_THREAD_AHEAD_OF_GPU + 1)
#else
	#define MAX_NUM_OCCLUSION_QUERIES	(2)
	#define ACTIVE_GPUS					(1)
#endif // RSG_PC

typedef enum
{
	LF_SUN,
	LF_COUNT
} E_LIGHT_TYPE;


class CFlareFX
{
public:
	CFlareFX() 
	: fDistFromLight(0.0f)
	, fSize(0.1f)
	, fWidthRotate(0.01f)
	, fScrollSpeed(0.15f)
	, fCurrentScroll(0.0f)
	, fDistanceInnerOffset(0.01f)
	, fIntensityScale(100.f)
	, fIntensityFade(1.0f)
	, m_fAnimorphicScaleFactorU(0.0f)
	, m_fAnimorphicScaleFactorV(0.0f)
	, color(Color_white)
	, m_fTextureColorDesaturate(0.0f)
	, m_fGradientMultiplier(1.0f)
	, nTexture(0)
	, nSubGroup(1)
	, m_bAnimorphicBehavesLikeArtefact(false)
	, m_fPositionOffsetU(0.0f)
	{};

	~CFlareFX() {};

	/**
		* Distance from the light along the vector to the screen edge.
		*/
	float fDistFromLight;

	/**
		* Size of the flare.
		*/
	float fSize;

	/**
		* Width if of type chromatic.
		* Rotation if of type corona.
		*/
	float fWidthRotate;

	/**
		* Chromatic scrolling
		*/
	float fScrollSpeed;
	float fCurrentScroll;

	/**
		* Offset applied only to the inner ring scaled by the distance from the screen center.
		* For coronas this is used as additional distance based scale.
		*/
	float fDistanceInnerOffset;

	/**
		* Additional scale to the intensity.
		*/
	float fIntensityScale;

	/**
		* Chromatic intensity fade factor.
		*/
	float fIntensityFade;

	/**
		* Additional horizontal scale.
		*/
	float m_fAnimorphicScaleFactorU;

	/**
		* Additional vertical scale.
		*/
	float m_fAnimorphicScaleFactorV;

	/**
		* Color multiplier for the flare.
		*/
	Color32 color;

	/**
		* Texture Color Desaturation.
		*/
	float m_fTextureColorDesaturate;
		
	/**
		* Gradient Multiplier
		*/
	float m_fGradientMultiplier;

	/**
		* Index of the texture to use.
		*/
	u8 nTexture;

	/**
		* Subgroup inside the texture.
		*/
	u8 nSubGroup;

	/**
		* Behave like an artefact type
		*/
	bool m_bAnimorphicBehavesLikeArtefact;

	/**
		* Align the artifact to the sun vector
		*/
	bool m_bAlignRotationToSun;

	/**
		* U Position offset
		*/
	float m_fPositionOffsetU;


#if __BANK
	/**
		* Pointer to the RAG FX group.
		*/
	bkGroup* pFXGroup;

	/**
		* Position in world space.
		*/	
	float worldSpacePos;

	/**
		* Is this the selected fx?
		*/	
	bool isSelectedByPicker;
#endif // __BANk

	PAR_SIMPLE_PARSABLE;
};

class CFlareTypeFX
{
public:
	CFlareTypeFX() {}
	~CFlareTypeFX() {}

	atArray<CFlareFX> arrFlareFX;

	PAR_SIMPLE_PARSABLE;
};

class CLensFlareSettings
{
public:
	CLensFlareSettings() : 
		m_fSunVisibilityFactor(1.0f), m_iSunVisibilityAlphaClip(10),  m_fSunFogFactor(1.0f), m_fExposureScaleFactor(1.0f), m_fExposureIntensityFactor(1.0f), 
		m_fExposureRotationFactor(1.0f), m_fChromaticFadeFactor(1.0f), m_fArtefactDistanceFadeFactor(1.0f), m_fArtefactRotationFactor(1.0f), 
		m_fArtefactScaleFactor(1.0f), m_fArtefactGradientFactor(0.0f), m_fCoronaDistanceAdditionalSize(1.0f), m_fMinExposureScale(0.0f), m_fMaxExposureScale(20.0f), 
		m_fMinExposureIntensity(0.0f), m_fMaxExposureIntensity(20.0f)
	{};

	float m_fSunVisibilityFactor;
	int m_iSunVisibilityAlphaClip;

	/**
	 * Normalized value that modulate the fogs affect on the sun.
	 */
	float m_fSunFogFactor;

	/**
	 * Controls how the chromatic texture is repeated on the ring.
	 */
	float m_fChromaticTexUSize;

	/**
	 * Factor used for scaling FX size based on exposure value.
	 */
	float m_fExposureScaleFactor;
	float m_fExposureIntensityFactor;
	float m_fExposureRotationFactor;

	/**
	 * Global fade factor for the chromatic FX.
	 */
	float m_fChromaticFadeFactor;

	/**
	 * Global distance fade factor for the artefact FX.
	 */
	float m_fArtefactDistanceFadeFactor;

	/**
	 * Global rotation factor for the artefact FX.
	 */
	float m_fArtefactRotationFactor;

	/**
	 * Global scale factor for the artefact FX.
	 */
	float m_fArtefactScaleFactor;

	/**
	 * Global gradient factor for artefects.
	 * The larger the value is, the stronger the affect is going to be.
	 */
	float m_fArtefactGradientFactor;

	/**
	 * Global distance additional size for the corona FX.
	 */
	float m_fCoronaDistanceAdditionalSize;

	/**
	 * Minimum exposure scale multiplier.
	 */
	float m_fMinExposureScale;

	/**
	 * Maximum exposure scale multiplier.
	 */
	float m_fMaxExposureScale;

	/**
	 * Minimum exposure intensity multiplier.
	 */
	float m_fMinExposureIntensity;
	
	/**
	 * Maximum exposure intensity multiplier.
	 */
	float m_fMaxExposureIntensity;

	/**
	 * Array with each flare type FX list.
	 */
	CFlareTypeFX m_arrFlareTypeFX[LF_COUNT];

	PAR_SIMPLE_PARSABLE;
};


class CLensFlare
{
public:

	CLensFlare();
	~CLensFlare();

	typedef enum
	{
		FT_ANIMORPHIC,
		FT_ARTEFACT,
		FT_CHROMATIC,
		FT_CORONA,
		FT_COUNT
	} E_FLARE_TEXTURE;

	typedef enum
	{
		FT_NONE = 0,
		FT_IN,
		FT_OUT,
	} E_FLARE_TRANSITIONS;

	/**
	 * Initialize the flare textures, array and occlusion query.
	 */
	void Init(unsigned initMode);

	/**
	 * Destroy the occlusion query.
	 */
	void Shutdown(unsigned shutdownMode);

	/**
	 * Add a light sources flare.
	 */
	void Register(E_LIGHT_TYPE eFlareType, const Color32& color, float fIntensity, Vec3V_In vPos, float fRadius);

	/**
	 * Render the accumulated flares. Do this earlier than render to give the conditional query time to get the results before we render the flares
	 */
	void IssueOcclusionQuerries();
		
	/**
	 * Render the accumulated flares.
	 */
	void Render(bool cameraCut);

	/**
	 * Clean the array from the lights added during this frame.
	 */
	void UpdateAfterRender();

	/**
	 * Get the current sun visibility value.
	 */
	float GetSunVisibility() const { return m_fSunVisibility; }

	/**
	 * Switch the lens flare type
	 */
	void ChangeSettings(const u8 settingsIndex);
	void TransitionSettings(const u8 settingsIndex);
	void TransitionSettingsRandom();

	/**
	 * Process the occlution query
	 */
	static void ProcessQuery();

	static inline int GetReadQueryIdx()
	{ 
#if RSG_PC
		FatalAssertf((ACTIVE_GPUS + 1) <= MAX_NUM_OCCLUSION_QUERIES, "Max Occluders %d must exceed read queue %d", ACTIVE_GPUS + 1, MAX_NUM_OCCLUSION_QUERIES); 
#endif
		return (ms_QueryIdx+1) % (ACTIVE_GPUS + 1);
	};

	static inline int GetWriteQueryIdx() { return ms_QueryIdx;};
	static inline void FlipQueryIdx() { ms_QueryIdx = (ms_QueryIdx+1) % (ACTIVE_GPUS + 1); };

	static bool ms_QuerySubmited[MAX_NUM_OCCLUSION_QUERIES];
	static u32 ms_QueryResult[MAX_NUM_OCCLUSION_QUERIES];
	static u32 ms_QueryIdx;

#if __BANK
	void InitWidgets();
#endif // __BANK

private:
	/**
	 * Render the suns flare.
	 */
	void RenderSunFlare(const Color32& color, Vec3V_In vSunPos, bool cameraCut);

	void RenderFlareFX(E_LIGHT_TYPE eFlareType, const Color32& color, float fIntensity, Vec2V_In vPos);

	void InitFlareFX();
	
	void PrepareSunVisibility(grcViewport* pViewport);

	/**
	 * Get a normalized sun visibility.
	 */
	void CalcSunVisibility(Vec3V_In vSunPos, float fSunRadius);
	
	/**
	 * Update the sun visibility based on the occlusion query result.
	 */
	void UpdateSunVisibilityFromQuery(u32 queryResult, grcViewport &customViewport, Vec3V_In vSunPos, float fSunRadius);

	void TriggerTransition(E_FLARE_TRANSITIONS type, u32 lengthMS, bool forceTransition = false);
	void ProcessTransition();

	/**
	 * Turn flares on/off.
	 */
	bool m_bEnabled;

	/**
	 * Array with all the textures used for the flares.
	 */
	grcTexture* m_arrFlareTex[FT_COUNT];

	/**
	 * Light source information.
	 */
	typedef struct
	{
		/**
		 * Light type.
		 */
		E_LIGHT_TYPE eFlareType;

		/**
		 * Light color.
		 */
		Color32 color;

		/**
		 * Light intensity.
		 */
		float fIntensity;

		/**
		 * Light position.
		 */
		Vec3V vPos;

		/**
		 * Light radius.
		 */
		float fRadius;
	} TLightFlare;

	/**
	 * Array with all the lights that will add flares.
	 */
	atArray<TLightFlare> m_arrLightFlares;

	/**
	 * Double buffered average exposure value.
	 */
	float m_fExposure[2];

	/**
	 * Size scale calculated based on the current exposure and m_fExposureScaleFactor values.
	 */
	float m_fExposureScale;

	float m_fExposureIntensity;

	float m_fExposureRotation;

	/**
	 * Occlusion query for the sun visibility.
	 */
	static grcOcclusionQuery m_SunOcclusionQuery[MAX_NUM_OCCLUSION_QUERIES];

	/**
	 * Stencil state for the sun visibility.
	 */
	grcDepthStencilStateHandle DSS_SunVisibility;

	/**
	 * Blend state with alpha test to cull of pixels covered by fog.
	 * This is only used on 360 - for other systems we clip in the shader.
	 */
	grcBlendStateHandle BS_SunVisibility;

	/**
	 * Shader used for the sun visibility occlusion query.
	 */
	grmShader* m_pSunVisibilityShader;

	/**
	 * Technique to render the sun visibility occlusion query.
	 */
	grcEffectTechnique m_SunVisibilityDrawTech;

	grcEffectVar m_SunPositionVar;

	grcEffectVar m_SunAlphaRefVar;

	/**
	 * Current sun visibility.
	 */
	float m_fSunVisibility;
	float m_fCachedSunVisibility;

	grcEffectVar m_SunFogFactorVar;

	/**
	 * Lens Flare Settings
	 */
	static const int NUMBER_OF_SETTINGS_FILES = 3;
	CLensFlareSettings m_Settings[NUMBER_OF_SETTINGS_FILES];
	u8 m_CurrentSettingsIndex;
	
	/**
	 * Transition
	 */
	E_FLARE_TRANSITIONS m_Transition;
	s32 m_StartTimeMS;
	s32 m_LengthMS;
	float m_CurrentFade;

	s32 m_SwitchLengthMS;
	s32 m_ShowLengthMS;
	s32 m_HideLengthMS;

	u8 m_DestinationSettingsIndex;

	s32 m_frameHold;

	/**
	 * Desaturation Boost
	 */
	 float m_DesaturationBoost;

#if __BANK
	/**
	 * Current squared exposure value.
	 */
	float m_fExposureSquared;

	/**
	 * Turn on to remove exposure from the calculations.
	 */
	bool m_bIgnoreExposure;

	/**
	 * Indicate if the widgets where created.
	 */
	bool m_bWidgetsCreated;

	void InitFXWidgets(bkBank* pBank, bool bOpen = false);
	void AddFX(CallbackData pCallbackData);
	void AddFXBank(int iType, int iFX);
	void AddSingleFXBank( int iType, int iFX, bkGroup* pTypeGroup, bool linkWidget);
	void RemoveFX(CallbackData pCallbackData);
	void ClearAll();
	void Load();
	void Save();

	void SortElementsByDistance();
	static bool CompareDistance(const CFlareFX& right,  const CFlareFX& left);

	bool m_enablePicking;
	float m_flareFxPickerValue;
	float m_previousPickerValue;

	bkGroup* m_pSelectedGroup;
	int m_currentlySelected;

	float m_fxInsertPoints[FT_COUNT];
#endif // __BANK
};

extern CLensFlare g_LensFlare;

#endif // INC_SKY
