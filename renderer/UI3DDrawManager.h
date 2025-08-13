//
// Filename:	UI3DDrawManager.h
// Description:	Handles requests to render peds as part of the UI.
//				Peds are forward-rendered after UI elements.
//				The system interface works with "scene presets" loaded from metadata (uiscenes.meta) 
//				that define the positioning and lighting setup for the different peds to be rendered.
//				Users can select a scene preset that will then be patched with CPed instances.
//				Presets can be edited in BANK builds via RAG > Renderer > UI 3D Draw Manager.
//
//				Presets can contain up to UI_MAX_SCENE_PRESET_ELEMENTS (i.e.: peds with a specific position, 
//				scissor rectangle, etc.) and only one preset can be rendered within a single frame.
//				The CPed instances are *not* managed by this system.

#ifndef __UI_3D_DRAW_MANAGER_H__
#define __UI_3D_DRAW_MANAGER_H__

#include "parser/macros.h"
#include "atl/array.h"
#include "atl/hashstring.h"
#include "bank/combo.h"
#include "scene/RegdRefTypes.h"
#include "renderer/Deferred/DeferredConfig.h"

#define DEFERRED_RENDER_UI	(1)

class CLightSource;

namespace rage
{
	class bkBank;
	class grcViewport;
}

class CPed;

//////////////////////////////////////////////////////////////////////////
// Helper structures

#define UI_MAX_SCENE_PRESET_ELEMENTS (5)

enum UILightRigType
{
	UI_LIGHT_RIG_0 = 0,
	UI_LIGHT_RIG_1,
	UI_LIGHT_RIG_2,
	UI_LIGHT_RIG_3,
	UI_LIGHT_RIG_COUNT,
};

struct UIGlobalRenderData
{
	UIGlobalRenderData() : m_dynamicBakeBoostMin(0.0f), m_dynamicBakeBoostMax(2.0f), m_exposure(0.5f), m_gamma(1.0f/2.2f) {}

	float m_exposure;
	float m_gamma;
	float m_dynamicBakeBoostMin;
	float m_dynamicBakeBoostMax;


	PAR_SIMPLE_PARSABLE;
};

struct UILightData
{
	UILightData() : m_intensity(0.0f), m_radius(0.0f), m_falloff(0.0f) {}

	Vector3		m_position;
	float		m_intensity;
	float		m_radius;
	float		m_falloff;
	Color32		m_color;

	PAR_SIMPLE_PARSABLE;
};

struct UILightRig
{
	UILightRig() : m_id(0U) {}

	atFixedArray<UILightData, 4>	m_lights;
	Color32 						m_ambientBaseCol;
	float							m_ambientBaseMult;
	Color32							m_ambientDownCol;
	float							m_ambientDownMult;
	float							m_ambientDownWrap;

	u32								m_id;

	PAR_SIMPLE_PARSABLE;
};

struct UIPostFXFilterData
{
	UIPostFXFilterData() : m_blend(0.0f) { m_blackWhiteWeights.Set(0U); m_tintColor.Set(0U); }

	float	m_blend;
	Color32 m_blackWhiteWeights;
	Color32 m_tintColor;

	PAR_SIMPLE_PARSABLE;
};

struct UISceneObjectData 
{
	UISceneObjectData () { Reset(); }

	void Reset();

	RegdPed	m_pPed;
	Vector3 m_position;
	Vector3 m_position_43;
	Vector3 m_rotationXYZ;
	Vector4 m_bgRectXYWH;
	Vector4 m_bgRectXYWH_43;
	Color32	m_bgRectColor;
	Color32 m_blendColor;
	bool	m_enabled;
	Vector2 m_perspectiveShear;
	UIPostFXFilterData m_postfxData;

	PAR_SIMPLE_PARSABLE;
};

struct UIScenePreset 
{
	UIScenePreset() { Reset(); }

	atHashString 													m_name;
	atFixedArray<UISceneObjectData, UI_MAX_SCENE_PRESET_ELEMENTS>	m_elements;

	void Reset();

	PAR_SIMPLE_PARSABLE;
};

struct UISceneObjectPatchedData 
{
	UISceneObjectPatchedData() : m_pPed(NULL), m_globalLightIntensity(1.0f) {}
	void Reset() { m_pPed = NULL; m_offset.Zero(); m_rotationXYZ.Zero(); m_globalLightIntensity = 1.0f; m_bgRectXYWH.Zero(); m_bgRectXYWH_43.Zero(); m_alpha = 1.0f; }

	RegdPed	m_pPed;
	Vector3	m_offset;
	Vector3 m_rotationXYZ;
	Vector4 m_bgRectXYWH;
	Vector4 m_bgRectXYWH_43;
	Color32	m_bgRectColor;
	float	m_alpha;
	float	m_globalLightIntensity;
};

//////////////////////////////////////////////////////////////////////////
//
#if __BANK
namespace rage 
{
	class bkCombo;
};

class UI3DDrawEditor 
{

public:

	UI3DDrawEditor	() : m_bUsePedsFromPicker(false), m_postfxOverrideObjectData(false), m_postfxBlendFilter(0.0f)  { m_postfxBWWeights.Set(0U); m_postfxTintCol.Set(0U); m_presetNames.Reset(); m_presetNames.Reserve(4); };
	~UI3DDrawEditor	() { m_presetNames.Reset(); }

	void	Init();
	void	Update();
	void	AddWidgets(rage::bkBank& bank );

	void	OnPresetNameSelected();
	void	OnSaveCurrentPreset();
	void	OnDeleteCurrentPreset();
	void	OnSavePresets();
	void	OnLoadPresets();

	bool	IsPostFXDataOverrideEnabled() const { return m_postfxOverrideObjectData; }
	void	GetPostFXDataOverride(Vector4& colFilter0, Vector4& colFilter1);

private:

	void	UpdatePresetNamesList(const atArray<UIScenePreset>& presets);

	bkCombo*				m_pPresetNamesCombo;
	int						m_presetNamesComboIdx;
	bool					m_presetChanged;
	bool					m_bUsePedsFromPicker;

	Color32					m_postfxBWWeights;
	Color32					m_postfxTintCol;
	float					m_postfxBlendFilter;
	bool					m_postfxOverrideObjectData;

	atArray<const char*>	m_presetNames;
	UIScenePreset			m_editablePreset;
	atHashString			m_newPresetName;

};
#endif // __BANK

//////////////////////////////////////////////////////////////////////////
//
class UI3DDrawManager
{

#if __BANK
	friend class UI3DDrawEditor;
#endif

public:
	UI3DDrawManager();
	~UI3DDrawManager();

	// script interface (external systems should use this too)
	bool							PushCurrentPreset(atHashString presetName);
	bool							AssignPedToSlot(atHashString presetName, u32 slot, CPed* pPed, float alpha);
	bool							AssignPedToSlot(atHashString presetName, u32 slot, CPed* pPed, Vector3::Vector3Param offset, float alpha);
	bool							AssignPedToSlot(atHashString presetName, u32 slot, CPed* pPed, Vector3::Vector3Param offset, Vector3::Vector3Param eulersOffset, float alpha);
	void							AssignGlobalLightIntensityToSlot(atHashString presetName, u32 slot, float intensity);

	void							MakeCurrentPresetPersistent(bool bEnable) { m_bKeepCurrentPreset = bEnable; };

	bool							IsAvailable() const { return (m_pCurrentPreset == NULL); }
	bool							IsUsingPreset(atHashString presetName) const;
	bool							HasPedsToDraw() const;

	void 							Init();
	void 							Shutdown();
	void 							InitRenderTargets();
#if RSG_PC
	static void						DeviceLost();
	static void						DeviceReset();
#endif
	void 							Update();
	void							PreRender();
	void 							AddToDrawList();

	void							ClearPatchedData() { m_bClearPatchedData = true; };

	static void 					ClassInit(unsigned initMode);
	static void 					ClassShutdown(unsigned shutdownMode);
	static UI3DDrawManager&			GetInstance() { return *smp_Instance; }

#if __BANK
	void							AddWidgets(bkBank *bank);
	void							UpdateDebug() { m_editTool.Update(); }
#endif // __BANK

#if DEFERRED_RENDER_UI
	grcRenderTarget*				GetTarget(u32 index) { return m_pGBufferRTs[index]; }
	grcRenderTarget*				GetDepth() { return m_pDepthRT; }
	grcRenderTarget*				GetDepthCopy();
	grcRenderTarget*				GetLightingTarget() { return m_pLightRT; }
	grcRenderTarget*				GetLightingTargetCopy() { return m_pLightRTCopy; }
	grcRenderTarget*				GetDepthAlias() { return m_pDepthAliasRT; }
	grcRenderTarget*				GetCompositeTarget0() { return m_pCompositeRT0; }
	grcRenderTarget*				GetCompositeTarget1() { return m_pCompositeRT1; }
	grcRenderTarget*				GetCompositeTargetCopy() { return (m_pCompositeRTCopy != NULL) ? m_pCompositeRTCopy : m_pCompositeRT0; }
#endif

private:

	void							GetPostFXDataForObject(const UISceneObjectData* pObject, Vector4& colFilter0, Vector4& colFilter1);

	bool							PushCurrentPreset(UIScenePreset* pPreset);
	bool							AssignPedToSlot(u32 slot, CPed* pPed, float alpha);
	bool							AssignPedToSlot(u32 slot, CPed* pPed, Vector3::Vector3Param offset, float alpha);
	bool							AssignPedToSlot(u32 slot, CPed* pPed, Vector3::Vector3Param offset, Vector3::Vector3Param eulersOffset, float alpha);

	UIScenePreset*					GetPreset(atHashString presetName);
	const UIScenePreset*			GetPreset(atHashString presetName) const;
	const atArray<UIScenePreset>&	GetPresetsArray() const { return m_scenePresets; };
	bool							LoadPresets();

#if __BANK
	bool							IsRenderingEnabled() const { return m_bEnableRendering; }
#endif

#if __BANK && !__FINAL
	bool							SavePresets() const;
#endif

	void							AddPreset(UIScenePreset& preset);
	void							RemovePreset(atHashString presetName);

	void							PopCurrentPreset();
	void 							AddPresetElementsToDrawList(const UIScenePreset* pPreset, const u32 slotIndex, u32 renderBucket, int renderMode);

	static void DrawBackgroundSprite(float x, float y, float w, float h, Color32 col);
	static void DrawListPrologue();
	static void DrawListEpilogue();

	static void RenderToneMapPass(grcRenderTarget* pDstRT, grcTexture* pSrcRT, const Vector4& scissorRect, const Vector4& colFilter0, const Vector4& colFilter1, bool bClearBackground);
	static void SetToneMapScalars();

	static void LockRenderTarget(u32 idx, bool bLockDepth);
	static void UnlockRenderTarget(bool bResolveCol, bool bResolveDepth);

#if DEFERRED_RENDER_UI

	static void SetUIProjectionParams(eDeferredShaders currentShader, const grcRenderTarget *pDstRT);
	static void SetUILightingGlobals();
	static void RestoreLightingGlobals();

	static void Initialize();
	static void Finalize();

	static void PerSlotSetup(float shearX, float shearY);

	static void	DeferredRenderStart();
	static void	DeferredRenderEnd(const Vector4& scissorRect);

	static void ForwardRenderStart();
	static void ForwardRenderEnd();
	
	static void	DeferredLight(const Vector4 &scissorRect, const Vector3 &objPos, Color32 clearCol, float lightGlobalIntensity);
	
	static void	LockRenderTargets(bool bLockDepth);
	static void	UnlockRenderTargets(bool bResolveCol, bool bResolveDepth, const Vector4& scissorRect);
	
	static void LockSingleTarget(u32 index, bool bLockDepth);
	static void LockLightingTarget(bool bLockDepth);
	static void UnlockSingleTarget(bool bResolveColor, bool bClearColor, bool bResolveDepth, bool bClearDepth, const Vector4& scissorRect);

	static void SetGbufferTargets();

	static void CompositeLuminanceToAlpha(grcRenderTarget* pDstRT, grcRenderTarget* pSrcRT, const Vector4& scissorRect);
	static void BlitBackground(grcRenderTarget* pDstRT, const grcRenderTarget* pSrcRT, const Vector4& rect, const Vector4& scissorRect);
	static void DrawBackground(grcRenderTarget* pDstRT, Color32 col, const Vector4& scissorRect);
	static void CompositeToBackbuffer(grcRenderTarget* pSrcRT, const Vector4& rect, float alpha);
#endif

	static void ClearTarget(bool bSkinPass);

	void LoadData();

	static void SetAmbients();
	static void SetLightsForObject(const Vector3& objectPos, float globalLightIntensity);

	void GenerateLights(CLightSource* pLights, const Vector3& objectPos, float globalLightIntensity);

#if DEFERRED_RENDER_UI
	grcRenderTarget*								m_pCompositeRT0;
	grcRenderTarget*								m_pCompositeRT1;
	grcRenderTarget*								m_pCompositeRTCopy;
	grcRenderTarget*								m_pGBufferRTs[4];
	grcRenderTarget*								m_pDepthRT;
	grcRenderTarget*								m_pDepthRTCopy;
	grcRenderTarget*								m_pDepthAliasRT;
	grcRenderTarget*								m_pLightRT;
	grcRenderTarget*								m_pLightRTCopy;
	
	grcDepthStencilStateHandle						m_insideLightVolumeDSS;
	grcDepthStencilStateHandle						m_outsideLightVolumeDSS;
#endif

	grcViewport*									m_pLastViewport;
	grcViewport										m_Viewport;
	Vec3V											m_vCamPos;
	Vec3V											m_vCamAnglesDeg;
	float											m_CamFieldOfView;

	grcDepthStencilStateHandle						m_DSSWriteStencilHandle;
	grcDepthStencilStateHandle						m_DSSTestStencilHandle;

	// light data
	atFixedArray<UILightRig, UI_LIGHT_RIG_COUNT>	m_lightRigs;
	u32												m_currentRigIdx;

	// scene presets
	atArray<UIScenePreset>							m_scenePresets;
	UISceneObjectPatchedData						m_scenePresetPatchedData[UI_MAX_SCENE_PRESET_ELEMENTS];

	UIScenePreset*									m_pCurrentPreset;

#if __BANK
	bool											m_bRunSkinPass;
	bool											m_bRunSSAPass;
	bool											m_bEnableRendering;
	bool											m_bRunForwardPass;
	bool											m_bDrawDebug;
	bool											m_bDisableScissor;
	bool											m_bRunFXAAPass;
	bool											m_bRunCompositePasses;

	UI3DDrawEditor									m_editTool;
#endif

	UIGlobalRenderData								m_globalRenderData;

	static UI3DDrawManager*							smp_Instance;

	static s32										sm_prevForcedTechniqueGroup;

	bool											m_prevInInterior;
	bool											m_bClearPatchedData;
	bool											m_bKeepCurrentPreset;

	PAR_SIMPLE_PARSABLE;

};

#define UI3DDRAWMANAGER UI3DDrawManager::GetInstance()

#endif // __UI_3D_DRAW_MANAGER_H__
