
#ifndef __LENS_ARTEFACTS_H__
#define __LENS_ARTEFACTS_H__

#include "atl/hashstring.h"
#include "atl/array.h"
#include "atl/functor.h"
#include "bank/bank.h"
#include "bank/combo.h"
#include "data/callback.h"
#include "fwtl/LinkList.h"
#include "grcore/effect_typedefs.h"
#include "grcore/stateblock.h"
#include "grmodel/shader.h"
#include "parser/macros.h"
#include "vector/color32.h"
#include "vector/vector4.h"

namespace rage
{
class grcRenderTarget;
}

//////////////////////////////////////////////////////////////////////////
//
enum eLensArtefactBlurType
{
	LENSARTEFACT_BLUR_2X = 0,
	LENSARTEFACT_BLUR_4X,
	LENSARTEFACT_BLUR_8X,
	LENSARTEFACT_BLUR_16X,
	LENSARTEFACT_BLUR_0X,
	LENSARTEFACT_BLUR_NONE
};

//////////////////////////////////////////////////////////////////////////
//
enum eLensArtefactStreakDir
{
	LENSARTEFACT_STREAK_HORIZONTAL = 0,
	LENSARTEFACT_STREAK_VERTICAL,
	LENSARTEFACT_STREAK_DIAGONAL_1,
	LENSARTEFACT_STREAK_DIAGONAL_2,
	LENSARTEFACT_STREAK_NONE
};

//////////////////////////////////////////////////////////////////////////
//
enum eLensArtefactMaskType
{
	LENSARTEFACT_MASK_HORIZONTAL,
	LENSARTEFACT_MASK_VERTICAL,
	LENSARTEFACT_MASK_NONE,
};

//////////////////////////////////////////////////////////////////////////
//
enum LensArtefactBufferSize
{
	LENSARTEFACT_HALF = 0,
	LENSARTEFACT_QUARTER,
	LENSARTEFACT_EIGHTH,
	LENSARTEFACT_SIXTEENTH,
	LENSARTEFACT_BUFFER_COUNT
};

enum LensArtefactShaderPass
{
	LENSARTEFACT_LENS_PASS = 0,
	LENSARTEFACT_UPSAMPLE_PASS,
	LENSARTEFACT_DIRECTIONAL_BLUR_LOW_PASS,
	LENSARTEFACT_DIRECTIONAL_BLUR_MED_PASS,
	LENSARTEFACT_DIRECTIONAL_BLUR_HIGH_PASS,
	LENSARTEFACT_LENS_COMBINED_PASS,
	LENSARTEFACT_SHADER_PASS_COUNT
};

//////////////////////////////////////////////////////////////////////////
//
class LensArtefact
{
public:
	
	LensArtefact() { Reset(); }
	
	void Reset()
	{
		m_sortIndex = 0;
		m_scale.Zero();
		m_offset.Zero();
		m_blurType = LENSARTEFACT_BLUR_2X;
		m_opacity = 0.0f;
		m_enabled = false;
		m_streakDirection = LENSARTEFACT_STREAK_NONE;
		m_fadeMaskType = LENSARTEFACT_MASK_NONE;
		m_fadeMaskMinMax.x = 0.0f;
		m_fadeMaskMinMax.y = 1.0f;
	}

	bool IsEnabled() const { return m_enabled; }
	void GetOffset(Vector2& offset) const { offset = m_offset; }
	void GetScale(Vector2& scale) const { scale = m_scale; }
	float GetOpacity() const { return m_opacity; }
	int GetSortIndex() const { return m_sortIndex; }
	eLensArtefactBlurType GetBlurType() const { return m_blurType; }
	eLensArtefactStreakDir GetStreakDirection() const { return m_streakDirection; }
	eLensArtefactMaskType GetFadeMaskType() const { return m_fadeMaskType; }
	void GetFadeMaskMinMax(Vector2& fadeMaskMinMax) const { fadeMaskMinMax = m_fadeMaskMinMax; }

	void GetScaleAndOffset(Vector4& scaleOffset) const { scaleOffset.x = m_scale.x; scaleOffset.y = m_scale.y; scaleOffset.z = m_offset.x; scaleOffset.w = m_offset.y;  }
	Color32 GetColorShift() const { return m_colorShift; }

	bool IsValid() { return true; };

	atHashString GetName() const { return m_name; }

	bool operator > (const LensArtefact& b) const { return m_sortIndex > b.m_sortIndex; }
	bool operator >=(const LensArtefact& b) const	{ return m_sortIndex >= b.m_sortIndex; }

private:

	atHashString			m_name;
	Vector2					m_scale;
	Vector2					m_offset;
	Color32					m_colorShift;
	eLensArtefactBlurType	m_blurType;
	eLensArtefactStreakDir	m_streakDirection;
	eLensArtefactMaskType	m_fadeMaskType;
	Vector2					m_fadeMaskMinMax;
	float					m_opacity;
	int						m_sortIndex;
	bool					m_enabled;
	
	PAR_SIMPLE_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
//
class LensArtefacts
{
public:

	LensArtefacts();
	~LensArtefacts();

	void Init();
	void Shutdown();
	
	// Render target memory is managed in PostProcessFX
	void SetRenderTargets(	grcRenderTarget* pDstBuffer, grcRenderTarget* pSrcBufferUnfiltered, grcRenderTarget** pSrcBuffers,
							grcRenderTarget** pScratchBuffers0, grcRenderTarget** pScratchBuffers1);

	// Callback to PostFX::Blit wrapper
	typedef Functor3<grcRenderTarget*, const grcTexture*, u32> RenderFunctor;
	void SetRenderCallback( RenderFunctor cb) { m_pBlitCallback = cb; }

 	//  Expects the same order and number of passes as the LensArtefactShaderPass enumeration
	void SetShaderData(grmShader* pShader, u32* pPasses);

	const grcRenderTarget* GetRenderTarget() const { return m_pFinalBuffer; }

	LensArtefact* Get(atHashString name);
	const LensArtefact* Get(atHashString name) const;

	void SetActive(bool bActive) { m_bIsActive = bActive; }
	bool IsActive() const;

	void Add(LensArtefact& stack);
	void Remove(atHashString name);
	void Copy(LensArtefact* pLayerDst, const LensArtefact* pLayerSrc);

	int GetLayerCount() const { return m_sortedLayers.GetNumUsed(); }

	float GetGlobalIntensity() const { return m_globalIntensity; }
	void SetGlobalIntensity(float intensity) { m_globalIntensity = intensity; }
	void SetExposureMinMaxFadeMultipliers(float expMin, float expMax, float fadeMin, float fadeMax);
	float GetExposureMinForFade() const { return m_minExposureForFade; }
	float GetExposureMaxForFade() const { return m_maxExposureForFade; }
	float GetExposureFadeMinMult() const { return m_exposureFadeMinMult; }
	float GetExposureFadeMaxMult() const { return m_exposureFadeMaxMult; }


	void GetExposureBasedOpacityMultipliers(Vector4& LensArtefactsParams0, Vector4& LensArtefactsParams1) const;

	void Render();

	bool LoadMetaData();
#if __BANK && !__FINAL
	bool SaveMetaData();
#endif

	void Update();

	static void	ClassInit();
	static void	ClassShutdown();
	static LensArtefacts&	GetInstance() { return *sm_pInstance; }

#if __BANK
	void AddWidgets(rage::bkBank& bank);
#endif


private:

	void RenderLayer(const LensArtefact* pLayer);
	void RenderLayersCombined(const LensArtefact* pLayer0, const LensArtefact* pLayer1);

	void ProcessDirectionalBlur(const LensArtefact* pLayer, grcRenderTarget* pSrcBuffer);
	void RunDirectionalBlurPass(const LensArtefact* pLayer, grcRenderTarget* pSrcBuffer, Vector4& dir);
	void RunDirectionalBlurPassesInterleaved(const LensArtefact* pLayer, grcRenderTarget* pSrcBuffer, Vector4& dir0, Vector4& dir1);

	void GetLayerRenderData(const LensArtefact* pLayer, Vector4& scaleoffset, Vector4& misc, Vector4& color) const;
	void GetRenderTargets(grcRenderTarget*& pDstBuffer, grcRenderTarget*& pSrcBuffer, eLensArtefactBlurType blurType, eLensArtefactStreakDir layerType) const;

	void UpdateSortedList();

	LensArtefactBufferSize GetBufferSize(const grcRenderTarget* pBuffer) const;

#if __BANK
	class EditTool
	{
	public:

		EditTool() : m_pLayerNamesCombo(NULL) {}
		void Init();
		void Shutdown();

		void Update();

		void AddWidgets(rage::bkBank& bank );

		void OnSaveCurrentLayer();
		void OnDeleteCurrentLayer();
		void OnSaveData();
		void OnLoadData();
		void OnLayerNameSelected();
		
		void UpdateLayerNamesList(const fwLinkList<LensArtefact*>& layers);

	private:
		
		atArray<const char*>			m_layerNames;
		int								m_layerNamesComboIdx;
		bkCombo*						m_pLayerNamesCombo;
		LensArtefact					m_editableLayer;
		static atHashString				ms_newLayerName;
	};
#endif // __BANK
	
	float					m_minExposureForFade;
	float					m_maxExposureForFade;
	float					m_exposureFadeMinMult;
	float					m_exposureFadeMaxMult;
	float					m_exposureFadeContribution;
	float					m_globalIntensity;
	atArray<LensArtefact>	m_layers;

	fwLinkList<LensArtefact*>		m_sortedLayers;
	typedef fwLink<LensArtefact*>	SortedLayerkNode;
	
#if __BANK
	EditTool				m_editor;
#endif

	//////////////////////////////////////////////////////////////////////////
	// Rendering related data
	grcRenderTarget*		m_pSrcBufferUnfiltered;
	grcRenderTarget*		m_pSrcBuffers[LENSARTEFACT_BUFFER_COUNT];
	grcRenderTarget*		m_pScratchBuffers[2][LENSARTEFACT_BUFFER_COUNT];
	grcRenderTarget*		m_pFinalBuffer;

	grmShader*				m_pShader;
	grcEffectTechnique		m_shaderTechnique;
	u32						m_shaderPasses[LENSARTEFACT_SHADER_PASS_COUNT];

	grcEffectVar			m_shaderVarBlurDirectionID;
	grcEffectVar			m_shaderVarDirBlurColWeightsID;
	grcEffectVar			m_shaderVarColorShiftID;
	grcEffectVar			m_shaderVarLensArtefactParams0ID;
	grcEffectVar			m_shaderVarLensArtefactParams1ID;
	grcEffectVar			m_shaderVarLensArtefactParams2ID;
	grcEffectVar			m_shaderVarLensArtefactParams3ID;
	grcEffectVar			m_shaderVarLensArtefactParams4ID;
	grcEffectVar			m_shaderVarLensArtefactParams5ID;
	grcEffectVar			m_shaderVarUpsampleParamsID;
	grcEffectVar			m_shaderVarTexelSizeID;
	grcEffectVar			m_shaderVarSamplerID;
	grcEffectVar			m_shaderVarSamplerAltID;

	grcSamplerStateHandle	m_samplerStateBorder;

	RenderFunctor			m_pBlitCallback;

	//////////////////////////////////////////////////////////////////////////
	u32						m_blurWidth[3];
	float					m_blurFalloffExponent;

	bool					m_bIsActive;
	bool					m_bUseInterleavedBlur;
	bool					m_bCombineLayerRendering;
#if __BANK
	bool					m_bUseSolidBorderSampler;
	bool					m_bOverrideTimecycle;
#endif

	static LensArtefacts* sm_pInstance;

	PAR_SIMPLE_PARSABLE;

};
#define LENSARTEFACTSMGR LensArtefacts::GetInstance()

#endif // __LENS_ARTEFACTS_H__
