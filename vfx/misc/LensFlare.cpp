//
// vfx/misc/LensFlare.cpp
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved.
//

// INCLUDES
#include "Vfx/misc/LensFlare.h"
#include "Vfx/misc/LensFlare_parser.h"

#if __BANK
#include "bank/widget.h"
#include "bank/bkmgr.h"
#endif

// framework
#include "camera/CamInterface.h"
#include "grcore/viewport.h"
#include "grcore/debugdraw.h"
#include "grcore/quads.h"
#include "grcore/stateblock.h"
#include "vector/colors.h"
#include "vfx/vfxwidget.h"
#include "fwscene/stores/txdstore.h"
#include "fwsys/gameskeleton.h"
#include "fwsys/timer.h"
#include "fwmaths/vectorutil.h"
#include "vfx/channel.h"
#include "vfx/vfxhelper.h"
#include "shaders/ShaderLib.h"
#include "renderer/sprite2d.h"
#include "renderer/PostProcessFX.h"
#include "timecycle/TimeCycle.h"
#include "timecycle/TimeCycleConfig.h"

#include "debug/LightProbe.h"

VFX_MISC_OPTIMISATIONS()

#define CALC_TOTAL_VERTS (VFX_SINCOS_LOOKUP_TOTAL_ENTRIES + 2)
#define NUM_VERTS_SUN_FAN ((VFX_SINCOS_LOOKUP_TOTAL_ENTRIES/8) + 2)

// GLOBAL VARIABLES
CLensFlare g_LensFlare;

static atArray<Vector4> g_arrUV;

#if __BANK
Color32 g_SunColor;
bool g_bShowVecToEdge = false;
bkGroup* g_pFXEditGroup = NULL;
#endif // __BANK

bool CLensFlare::ms_QuerySubmited[MAX_NUM_OCCLUSION_QUERIES];
u32 CLensFlare::ms_QueryResult[MAX_NUM_OCCLUSION_QUERIES];
u32 CLensFlare::ms_QueryIdx = 0;

grcOcclusionQuery  CLensFlare::m_SunOcclusionQuery[MAX_NUM_OCCLUSION_QUERIES];

CLensFlare::CLensFlare() : m_bEnabled(true),
	DSS_SunVisibility(grcStateBlock::DSS_Invalid), BS_SunVisibility(grcStateBlock::BS_Invalid), m_pSunVisibilityShader(NULL), 
	m_fSunVisibility(0.0f) BANK_ONLY(, m_bIgnoreExposure(false), m_bWidgetsCreated(false))
{
	for(int iCurTex = 0; iCurTex < FT_COUNT; iCurTex++)
	{
		m_arrFlareTex[iCurTex] = NULL;
	}
}

CLensFlare::~CLensFlare()
{

}

void CLensFlare::Init(unsigned initMode)
{
	if (initMode == INIT_CORE)
	{
		// Prepare the sun visibility shader
		ASSET.PushFolder("common:/shaders");
		m_pSunVisibilityShader = grmShaderFactory::GetInstance().Create();
		m_pSunVisibilityShader->Load("sky_system");
		ASSET.PopFolder();
		m_SunVisibilityDrawTech = m_pSunVisibilityShader->LookupTechnique("sun_visibility", true);
		m_SunPositionVar = m_pSunVisibilityShader->LookupVar("sunPosition");
		m_SunAlphaRefVar = m_pSunVisibilityShader->LookupVar("noiseThreshold");
		m_SunFogFactorVar = m_pSunVisibilityShader->LookupVar("noiseSoftness");
		m_fSunVisibility = 0.0f;
		m_fCachedSunVisibility = 0.0f;

		// Load the textures from the dictionary
		strLocalIndex txdSlot = strLocalIndex(CShaderLib::GetTxdId());
		m_arrFlareTex[FT_ANIMORPHIC] = g_TxdStore.Get(txdSlot)->Lookup(ATSTRINGHASH("flare_animorphic", 0x0448eef02));
		vfxAssertf(m_arrFlareTex[FT_ANIMORPHIC], "Missing lens flare texture flare_animorphic");
		m_arrFlareTex[FT_ARTEFACT] = g_TxdStore.Get(txdSlot)->Lookup(ATSTRINGHASH("flare_artefactx2", 0x4e9ef01a));
		vfxAssertf(m_arrFlareTex[FT_ARTEFACT], "Missing lens flare texture flare_artefact");
		m_arrFlareTex[FT_CHROMATIC] = g_TxdStore.Get(txdSlot)->Lookup(ATSTRINGHASH("flare_chromatic", 0x0266e84f5));
		vfxAssertf(m_arrFlareTex[FT_CHROMATIC], "Missing lens flare texture flare_chromatic");
		m_arrFlareTex[FT_CORONA] = g_TxdStore.Get(txdSlot)->Lookup(ATSTRINGHASH("flare_corona", 0x04eb94d5b));
		vfxAssertf(m_arrFlareTex[FT_CORONA], "Missing lens flare texture flare_corona");

		//generate UV's
		//note if the textures width or size of each element changes then old index's will be incorrectly
		//mapped, best to to extend the texture downward to maintain compatibility with current flare setup
		s32 textureWidth = m_arrFlareTex[FT_ARTEFACT]->GetWidth();
		s32 textureHeight = m_arrFlareTex[FT_ARTEFACT]->GetHeight();

		//specified as relative to the current size, because it can be resize on PC
		s32 elementSize =  m_arrFlareTex[FT_ARTEFACT]->GetWidth() / 2;

		float elementWidthInUVs = elementSize / (float)textureWidth;
		float elementHeightInUVs = elementSize / (float)textureHeight;

		s32 numberOfElements = (textureWidth / elementSize) * (textureHeight / elementSize) + 1;
		g_arrUV.Reserve(numberOfElements);

		//we push a set that cover the whole texture as the non-artifact elements use this
		g_arrUV.Push(Vector4(0.0f,0.0f,1.0f,1.0f));

		for(float v = 0.0f; v < 1.0f; v += elementHeightInUVs)
		{	
			for(float u = 0.0f; u < 1.0f; u += elementWidthInUVs)
			{
				Vector4 texCoords = Vector4(u, v, u + elementWidthInUVs, v + elementHeightInUVs);
				g_arrUV.Push(texCoords);
			}
		}

		// Reserve the max size for the flares
		const u32 nMaxFlares = 1;
		m_arrLightFlares.Reserve(nMaxFlares);

		// Initialize the flare FX
		InitFlareFX();

		for(int i=0;i<MAX_NUM_OCCLUSION_QUERIES;i++)
		{
			m_SunOcclusionQuery[i] = GRCDEVICE.CreateOcclusionQuery();
			ms_QuerySubmited[i] = false;
			ms_QueryResult[i] = 0;
		}

		grcDepthStencilStateDesc sunDepthStencilStateBlockDesc;
		sunDepthStencilStateBlockDesc.DepthEnable = true;
		sunDepthStencilStateBlockDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		sunDepthStencilStateBlockDesc.DepthWriteMask = false;
		sunDepthStencilStateBlockDesc.StencilEnable = true;
		sunDepthStencilStateBlockDesc.StencilReadMask = DEFERRED_MATERIAL_CLEAR;
		sunDepthStencilStateBlockDesc.StencilWriteMask = 0;
		sunDepthStencilStateBlockDesc.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		sunDepthStencilStateBlockDesc.BackFace.StencilFunc = grcRSV::CMP_EQUAL;
		DSS_SunVisibility = grcStateBlock::CreateDepthStencilState(sunDepthStencilStateBlockDesc);

		grcBlendStateDesc sunBlendStateBlockDesc;
#if __XENON
		sunBlendStateBlockDesc.AlphaToCoverageEnable = true;
		sunBlendStateBlockDesc.AlphaToMaskOffsets = grcRSV::ALPHATOMASKOFFSETS_DITHERED;
#endif // __XENON
		sunBlendStateBlockDesc.BlendRTDesc[0].RenderTargetWriteMask = grcRSV::COLORWRITEENABLE_NONE;
		BS_SunVisibility = grcStateBlock::CreateBlendState(sunBlendStateBlockDesc);

		m_CurrentSettingsIndex = 0;
			
		m_Transition = FT_NONE;
		m_StartTimeMS = 0u;
		m_LengthMS = 0u;
		m_CurrentFade = 0.0f;

		if (g_visualSettings.GetIsLoaded())
		{
			m_SwitchLengthMS = (u32)g_visualSettings.Get("lensflare.switch.length", 1000);
			m_HideLengthMS = (u32)g_visualSettings.Get("lensflare.hide.length", 1000);
			m_ShowLengthMS = (u32)g_visualSettings.Get("lensflare.show.length", 500);
			m_DesaturationBoost = g_visualSettings.Get("lensflare.desaturation.boost", 1.0f);
		}

		m_frameHold = 0;

#if __BANK
		g_SunColor = Color_black;
		
		m_enablePicking = false;
		m_flareFxPickerValue = 0.2f;
		m_previousPickerValue = FLT_MAX;
		m_pSelectedGroup = NULL;
		m_currentlySelected = -1;
#endif // __BANK
	}
}

void CLensFlare::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
		if(m_pSunVisibilityShader)
		{
			delete m_pSunVisibilityShader;
			m_pSunVisibilityShader = NULL;
		}

		for(int i=0;i<MAX_NUM_OCCLUSION_QUERIES;i++)
		{
			GRCDEVICE.DestroyOcclusionQuery(m_SunOcclusionQuery[i]);
			m_SunOcclusionQuery[i] = NULL;
			ms_QuerySubmited[i] = false;
			ms_QueryResult[i] = 0;
		}
		if(DSS_SunVisibility != grcStateBlock::DSS_Invalid)
		{
			DestroyDepthStencilState(DSS_SunVisibility);
			DSS_SunVisibility = grcStateBlock::DSS_Invalid;
		}

		if(BS_SunVisibility != grcStateBlock::BS_Invalid)
		{
			DestroyBlendState(BS_SunVisibility);
			BS_SunVisibility = grcStateBlock::BS_Invalid;
		}
	}
}

void CLensFlare::Register(E_LIGHT_TYPE eFlareType, const Color32& color, float fIntensity, Vec3V_In vPos, float fRadius)
{
	if(m_arrLightFlares.GetCount() < m_arrLightFlares.GetCapacity())
	{
		// We have room - just add the flare
		TLightFlare& tLightFlare = m_arrLightFlares.Append();
		tLightFlare.eFlareType = eFlareType;
		tLightFlare.color = color;
		tLightFlare.fIntensity = fIntensity;
		tLightFlare.vPos = vPos;
		tLightFlare.fRadius = fRadius;
	}
	else
	{
		// No more room - check if we want to replace one of the flares
		vfxAssertf(false, "Not implemented yet!");
	}
}

void CLensFlare::IssueOcclusionQuerries()
{
	for(int iCurFlare = 0; iCurFlare < m_arrLightFlares.GetCount(); iCurFlare++)
	{
		TLightFlare& curLight = m_arrLightFlares[iCurFlare];
		CalcSunVisibility(curLight.vPos, m_Settings[m_CurrentSettingsIndex].m_fSunVisibilityFactor * curLight.fRadius);
	}
}

void CLensFlare::Render(bool cameraCut)
{
	// Check if the flare is disabled
	if(!m_bEnabled)
	{
		return;
	}

#if __BANK
	if (LightProbe::IsCapturingPanorama())
		return;
#endif

	PF_PUSH_TIMEBAR_DETAIL("Lens Flare - Render");

	PF_PUSH_TIMEBAR_DETAIL("Preparation");
	// Calculate the exposure size scale
	const int rb = gRenderThreadInterface.GetRenderBuffer();
	float fCurrentExposure = m_fExposure[rb];
#if __BANK
	if(m_Settings[m_CurrentSettingsIndex].m_fMinExposureScale >= m_Settings[m_CurrentSettingsIndex].m_fMaxExposureScale)
	{
		m_Settings[m_CurrentSettingsIndex].m_fMinExposureScale = m_Settings[m_CurrentSettingsIndex].m_fMaxExposureScale - 0.01f;
	}
	if(m_Settings[m_CurrentSettingsIndex].m_fMinExposureIntensity >= m_Settings[m_CurrentSettingsIndex].m_fMaxExposureIntensity)
	{
		m_Settings[m_CurrentSettingsIndex].m_fMinExposureIntensity = m_Settings[m_CurrentSettingsIndex].m_fMaxExposureIntensity - 0.01f;
	}
#endif // __BANK

	float fNormExposure = fCurrentExposure;
	m_fExposureScale = Clamp(m_Settings[m_CurrentSettingsIndex].m_fExposureScaleFactor * fNormExposure, m_Settings[m_CurrentSettingsIndex].m_fMinExposureScale, m_Settings[m_CurrentSettingsIndex].m_fMaxExposureScale);
	m_fExposureIntensity = Clamp(m_Settings[m_CurrentSettingsIndex].m_fExposureIntensityFactor * fNormExposure, m_Settings[m_CurrentSettingsIndex].m_fMinExposureIntensity, m_Settings[m_CurrentSettingsIndex].m_fMaxExposureIntensity);

	// Currently we only rotate artifacts and we want those to stop rotating when the scale is clamped
	if(m_fExposureScale <= m_Settings[m_CurrentSettingsIndex].m_fMinExposureScale || m_fExposureScale >= m_Settings[m_CurrentSettingsIndex].m_fMaxExposureIntensity)
	{
		// Extrapolate the clamped exposure value to keep the rotation at the same value
		float fClampedExposure = m_fExposureScale / m_Settings[m_CurrentSettingsIndex].m_fExposureScaleFactor;
		m_fExposureRotation = m_Settings[m_CurrentSettingsIndex].m_fExposureRotationFactor * fClampedExposure;
	}
	else
	{
		m_fExposureRotation = m_Settings[m_CurrentSettingsIndex].m_fExposureRotationFactor * fNormExposure;
	}
	
#if __BANK
	m_fExposureSquared = fCurrentExposure * fCurrentExposure;
	if(m_bIgnoreExposure)
	{
		m_fExposureScale = 1.0f;
		m_fExposureIntensity = 1.0f;
		m_fExposureRotation = 1.0f;
	}
#endif // __BANK

	//update transitions
	PF_PUSH_TIMEBAR_DETAIL("ProcessTransition");
	ProcessTransition();
	PF_POP_TIMEBAR_DETAIL(); // ProcessTransition

	//trigger the load and fade in, if we have one pending
	if(m_CurrentFade == 1.0f && m_CurrentSettingsIndex != m_DestinationSettingsIndex)
	{
		ChangeSettings(m_DestinationSettingsIndex);
		TriggerTransition(FT_IN, m_SwitchLengthMS / 2);
	}

	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Add);
	PF_POP_TIMEBAR_DETAIL(); // Preparation

	// Go over all the flares and render them
	PF_PUSH_TIMEBAR_DETAIL("RenderSunFlare");
	for(int iCurFlare = 0; iCurFlare < m_arrLightFlares.GetCount(); iCurFlare++)
	{
		TLightFlare& curLight = m_arrLightFlares[iCurFlare];
		vfxAssertf(curLight.fIntensity > 0.0f, "Why did we add a flare that has zero intensity?");
		if(curLight.eFlareType == LF_SUN)
		{
			RenderSunFlare(curLight.color, curLight.vPos,cameraCut);
		}
	}
	PF_POP_TIMEBAR_DETAIL(); // RenderSunFlare

	PF_POP_TIMEBAR_DETAIL(); // Lens Flare - Render
}

void CLensFlare::UpdateAfterRender()
{
	// Clean the array of the currently rendered buffer
	m_arrLightFlares.clear();

	// Update the exposure value
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	const int ub=gRenderThreadInterface.GetUpdateBuffer();
	m_fExposure[ub] = PostFX::GetAverageLinearExposure();
}

void CLensFlare::ChangeSettings(const u8 settingsIndex)
{
	Assertf(settingsIndex < NUMBER_OF_SETTINGS_FILES, "No lens flare settings in this slot!");

	m_CurrentSettingsIndex = settingsIndex;
	m_DestinationSettingsIndex = settingsIndex;
}

void CLensFlare::TransitionSettings(const u8 settingsIndex)
{
	Assertf(settingsIndex < NUMBER_OF_SETTINGS_FILES, "No lens flare settings in this slot!");

	//if the flare isn't visible then we can just switch instantly
	if(m_fSunVisibility == 0.0f && m_CurrentFade == 1.0f)
	{
		ChangeSettings(settingsIndex);
	}
	else
	{
		m_DestinationSettingsIndex = settingsIndex;
		TriggerTransition(FT_OUT, m_SwitchLengthMS / 2);
	}
}

void CLensFlare::TransitionSettingsRandom()
{
	u8 randomSettingsIndex = (u8)fwRandom::GetRandomNumberInRange(0, NUMBER_OF_SETTINGS_FILES);

	if(randomSettingsIndex == m_CurrentSettingsIndex)
	{
		randomSettingsIndex = (randomSettingsIndex + 1) % NUMBER_OF_SETTINGS_FILES;
	}

	TransitionSettings(randomSettingsIndex);
}

void CLensFlare::ProcessQuery()
{
	int readIdx = GetReadQueryIdx();
	if(ms_QuerySubmited[readIdx])
	{
		GRCDEVICE.GetOcclusionQueryData(m_SunOcclusionQuery[readIdx], ms_QueryResult[readIdx]);
		ms_QuerySubmited[readIdx] = false;
	}
	FlipQueryIdx();
}

void CLensFlare::RenderSunFlare(const Color32& color, Vec3V_In vSunPos, bool cameraCut)
{
#if __BANK
	g_SunColor = color;
#endif // __BANK
	// Find the suns 2D position
	grcViewport vp = *grcViewport::GetCurrent();
	vp.SetNearFarClip(1.0f, 25000.0f);
	vp.SetWorldIdentity();

	Vec4V vSunPosMult= Multiply(vp.GetFullCompositeMtx(),Vec4V(vSunPos,ScalarV(V_ONE)));

	if( IsZeroAll(vSunPosMult.GetW()) )
		return; // Someone gone and fucked the sun up

	Vec3V vSunPosP = vSunPosMult.GetXYZ() / vSunPosMult.GetW();
	float fHorPos = vSunPosP.GetXf()/(float)vp.GetWidth();
	float fVerPos = vSunPosP.GetYf() /(float)vp.GetHeight();
	Vec2V vSunPos2D(fHorPos, fVerPos);
	
	bool onCameraCut = cameraCut && PostFX::GetUseExposureTweak();
	bool isOnScreen = vSunPos2D.GetXf() > 0.0f && vSunPos2D.GetXf() < 1.0f && vSunPos2D.GetYf() > 0.0f && vSunPos2D.GetYf() < 1.0f;

	if(onCameraCut)
	{
		if( isOnScreen == false || vSunPosP.GetZf() < 0.0f )
		{
			//if we detect a camera cut then instantly transition out to prevent the lens flare from fading out
			m_Transition = FT_NONE;
			m_CurrentFade = 1.0f;
			m_fCachedSunVisibility = 0.0f;

			//we also reset the occlusion query that can be a couple of frames behind and cause the lens flare to reappear
			for(int i=0;i<MAX_NUM_OCCLUSION_QUERIES;i++)
			{
				ms_QuerySubmited[i] = false;
				ms_QueryResult[i] = 0;
			}
		}
		else
		{
			m_Transition = FT_NONE;
			m_CurrentFade = 1.0f;
			m_fCachedSunVisibility = 0.0f;

			m_frameHold = MAX_NUM_OCCLUSION_QUERIES+1;
		}
	}
	else if( m_frameHold )
	{
		m_frameHold--;
		if( m_frameHold == 0 )
		{
			TriggerTransition(FT_IN, m_ShowLengthMS, true);
		}
	}
	else
	{
		if(isOnScreen)
		{
			if(m_fSunVisibility < 0.001f)
			{
				//hide the lens flare, the sun is not visible
				m_Transition = FT_NONE;
				m_CurrentFade = 1.0f;
				m_fCachedSunVisibility = 0.0f;
			}
			else if(m_CurrentFade != 0.0f && m_Transition != FT_IN && m_CurrentSettingsIndex == m_DestinationSettingsIndex)
			{
				//snap the lens flare back in, unless we're switching lens flares
				m_Transition = FT_NONE;
				m_CurrentFade = 0.0f;
			}
		}
		else
		{
			//otherwise we've just gone off screen so we'll just transition out
			if(m_CurrentFade < 1.0f && m_Transition == FT_NONE)
			{
				TriggerTransition(FT_OUT, m_HideLengthMS);
			}
		}
	}

	if(m_CurrentFade < 1.0f)
	{
		//use the cached visibility value for the fadeout
		RenderFlareFX(LF_SUN, color, m_fCachedSunVisibility, vSunPos2D);
	}
}

void CLensFlare::RenderFlareFX(E_LIGHT_TYPE eFlareType, const Color32& color, float fIntensity, Vec2V_In vPos)
{
	// Construct the sized direction to the screen edge
	Vec2V vFlareDirNorm = Vec2V(V_HALF) - vPos;
	ScalarV fLen = Mag(vFlareDirNorm);
	ScalarV fDistScale = ScalarV(V_ZERO);
	Vec2V vFlareDir;
	if(IsLessThanAll(fLen, ScalarV(V_FLT_SMALL_3)))
	{
		// The light is in the center of the screen - no vector to edge
		vFlareDirNorm = Vec2V(V_ZERO);
		vFlareDir = Vec2V(V_ZERO);
	}
	else
	{
		Vec2V vDist = Abs(vFlareDirNorm) + Vec2V(V_HALF);
		vFlareDirNorm /= fLen; // Normalize
		ScalarV fDistToEdge = Mag(vDist);
		vFlareDir = vFlareDirNorm * fDistToEdge; // Scale by the distance to the edge of the screen

		// Use the scale to push the flares towards the light as it gets closer to the center of the screen
		fDistScale = Min(ScalarV(V_ONE), fLen * ScalarV(V_TWO));
		fDistScale *= fDistScale; 
	}

#if __BANK
	if(g_bShowVecToEdge)
	{
		grcDebugDraw::Line(vPos, vPos + vFlareDir, Color_red, Color_red);
	}

	ScalarV pickerDistFromLight = ScalarV(m_flareFxPickerValue); 

	if(m_enablePicking)
	{
		//render picker
		Vec2V pickerPos = AddScaled(vPos, vFlareDir, pickerDistFromLight);
		grcDebugDraw::Circle(pickerPos, 0.06f, Color_LightGray, false, 30);
		grcDebugDraw::Cross(pickerPos, 0.02f, Color_LightGray);

		//work out the transform from the world position of the picker into the space 
		//the vaious types of fx work in, it's the reverse of the position calc bellow
		for (int t = 0; t < FT_COUNT; ++t)
		{
			if(t == FT_ARTEFACT)
			{
				float fDistFromCenterNorm = 2.0f * fLen.Getf();
				m_fxInsertPoints[t] =  (m_flareFxPickerValue - (2.0f * fDistFromCenterNorm)) / fDistFromCenterNorm;
			}
			else
			{
				m_fxInsertPoints[t] = (pickerDistFromLight / fDistScale).Getf();
			}
		}
	}
	
	//setup some values so we can find the currently selected item as we go
	float shortestDistanceToPicker = FLT_MAX;
	Vec2V positionOfSelected;
	int indexOfSelected = -1;
#endif // __BANK

	CSprite2d flareSprite;

	// Cache the previous viewport and make a local copy that will get modified
	GRC_VIEWPORT_AUTO_PUSH_POP();
	grcViewport customViewport = *grcViewport::GetCurrent();
	
	// Clear all transformation - all rendering is done in clip space
	customViewport.SetWorldIdentity();
	customViewport.SetCameraIdentity();
	customViewport.SetProjection(Mat44V(V_IDENTITY));

	// Helpers for the transformation to clip space
	const Vec2V vNegPosOne = Vec2V(ScalarV(V_NEGONE), ScalarV(V_ONE));
	const Vec2V vOneNegOne = Vec2V(ScalarV(V_ONE), ScalarV(V_NEGONE));
	const Vec2V vPosNegTwo = Vec2V(ScalarV(V_TWO), ScalarV(V_NEGTWO));

	float fAspectRatio = customViewport.GetAspectRatio();
	Vec2V vAspectRatio = Vec2V(ScalarV(V_ONE), ScalarV(fAspectRatio));

	// Set our custom viewport
	grcViewport::SetCurrent(&customViewport);

	atArray<CFlareFX>& arrFX = m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[eFlareType].arrFlareFX;
	for(int iCurFx = 0; iCurFx < arrFX.GetCount(); iCurFx++)
	{
#if __BANK
		// Skip dead FX
		if(m_bWidgetsCreated && arrFX[iCurFx].pFXGroup == NULL)
		{
			continue;
		}
#endif // __BANK

		float fGammaFactor = fIntensity * arrFX[iCurFx].fIntensityScale;
		if(arrFX[iCurFx].nTexture == FT_CHROMATIC)
		{
			// For chromatic just fade based on the distance from the center
			fGammaFactor -= fIntensity * arrFX[iCurFx].fIntensityFade * Max(0.0f, 1.0f - fDistScale.Getf());
		}
		else
		{
			fGammaFactor *= m_fExposureIntensity;
		}

		ScalarV fDistFromLightFactor;

		bool bCurAnamorphicLikeArtefact = (arrFX[iCurFx].nTexture == FT_ANIMORPHIC && arrFX[iCurFx].m_bAnimorphicBehavesLikeArtefact);

		if(arrFX[iCurFx].nTexture == FT_ARTEFACT || bCurAnamorphicLikeArtefact)
		{
			float fDistFromCenterNorm = 2.0f * fLen.Getf();
			fDistFromLightFactor = ScalarV(2.0f * fDistFromCenterNorm + arrFX[iCurFx].fDistFromLight * fDistFromCenterNorm);
		}
		else
		{
			fDistFromLightFactor = ScalarV(arrFX[iCurFx].fDistFromLight) * fDistScale;
		}

		// 2D center position
		Vec2V vFlarePos = AddScaled(vPos, vFlareDir, fDistFromLightFactor);

		if(arrFX[iCurFx].nTexture == FT_ARTEFACT)
		{
			vFlarePos.SetXf(vFlarePos.GetXf() + arrFX[iCurFx].m_fPositionOffsetU);
		}

#if __BANK
		//work out if we're the nearest to the picker so far
		float distance = abs(pickerDistFromLight.Getf() - fDistFromLightFactor.Getf());

		arrFX[iCurFx].isSelectedByPicker = false;

		if(distance < shortestDistanceToPicker)
		{
			shortestDistanceToPicker = distance;
			indexOfSelected = iCurFx;
		}

		//draw a debug shape that represents this fx, it's easier to see what is selected this way
		if(m_enablePicking)
		{
			grcDebugDraw::Circle(vFlarePos, arrFX[iCurFx].fSize, arrFX[iCurFx].color * Color32(0.6f, 0.6f, 0.6f, 0.6f), false);
		}

		//cache the world space, so we can use it for sorting and drawing later
		arrFX[iCurFx].worldSpacePos = fDistFromLightFactor.Getf();
#endif

		float fHalfSize;
		if(arrFX[iCurFx].nTexture == FT_CHROMATIC) // Special case for the Chromatic flare
		{
			fHalfSize = arrFX[iCurFx].fSize;
		}
		else
		{
			fHalfSize = m_fExposureScale * arrFX[iCurFx].fSize;
			if(arrFX[iCurFx].nTexture == FT_ARTEFACT)
			{
				fHalfSize *= m_Settings[m_CurrentSettingsIndex].m_fArtefactScaleFactor;
			}
			else if(arrFX[iCurFx].nTexture == FT_CORONA)
			{
				fHalfSize += fLen.Getf() * m_Settings[m_CurrentSettingsIndex].m_fCoronaDistanceAdditionalSize;
			}
		}

		Vec2V vDirFromLight = NormalizeSafe(vPos - vFlarePos,Vec2V(ScalarV(V_ZERO)));
		if(arrFX[iCurFx].nTexture == FT_ARTEFACT)
		{
			// Set the gradient parameters with the final 2D position
			const Vec2V vHalfSize = Vec2V(ScalarV(fHalfSize), ScalarV(fHalfSize * fAspectRatio));
			Vec2V vFlarePixelDir = vFlarePos - vDirFromLight * vHalfSize * ScalarV(V_HALF) * ScalarV(arrFX[iCurFx].m_fGradientMultiplier);

			Vector4 vFirstParam(vDirFromLight.GetXf(), vDirFromLight.GetYf(), vFlarePixelDir.GetXf(), vFlarePixelDir.GetYf());
			Vector4 vSecondParam(fHalfSize, fHalfSize * fAspectRatio, 0.0f, 0.0f);
			flareSprite.SetGeneralParams(vFirstParam, vSecondParam);
		}
		else if(arrFX[iCurFx].nTexture == FT_CHROMATIC)
		{
			flareSprite.SetGeneralParams(Vector4(1.0f, 1.0, 1.0, 0.0f), Vector4(0.0f, 0.0f, arrFX[iCurFx].m_fTextureColorDesaturate, m_DesaturationBoost));
		}

		// To clip space
		vFlarePos = AddScaled(vNegPosOne, vPosNegTwo, vFlarePos);
		
		CShaderLib::SetGlobalEmissiveScale( Max(fGammaFactor, 0.0f) );
		flareSprite.BeginCustomList(arrFX[iCurFx].nTexture == FT_ARTEFACT ? CSprite2d::CS_COLORCORRECTED_GRADIENT : CSprite2d::CS_COLORCORRECTED,
#if __BANK
			arrFX[iCurFx].nTexture >= FT_COUNT ? m_arrFlareTex[FT_ANIMORPHIC] : // Avoid crash when editing the FX
#endif // __BANK
			m_arrFlareTex[arrFX[iCurFx].nTexture]
		);

		if(arrFX[iCurFx].nTexture == FT_CHROMATIC) // Special case for the Chromatic flare
		{
			arrFX[iCurFx].fCurrentScroll += static_cast<float>(fwTimer::GetTimeStep()) * arrFX[iCurFx].fScrollSpeed * 0.5f;

			//wrap around so we're always 0 - 1
			if(arrFX[iCurFx].fCurrentScroll >= 1.0f)
			{
				arrFX[iCurFx].fCurrentScroll -= 1.0f;
			}

			//draw two elements, but the second one has flipped texture offset
			for(u32 cp = 0; cp < 2; ++cp)
			{
 				int iTotalVerts = CALC_TOTAL_VERTS;
				int iTotalEntries = (iTotalVerts>>1)-1;
				DEV_ONLY(if(Verifyf(iTotalEntries <= VFX_SINCOS_LOOKUP_TOTAL_ENTRIES, "Chromatic Tessellation does not have Same number of entries (%d) as the lookup Table(%d)", iTotalEntries, VFX_SINCOS_LOOKUP_TOTAL_ENTRIES)))
				{
					grcBegin(drawTriStrip, iTotalVerts);

					ScalarV fScaleFactor = Min(ScalarV(V_ONE), fLen);
					ScalarV fInnerRad = ScalarV(fHalfSize);
					Vec2V ScaleFactors(arrFX[iCurFx].m_fAnimorphicScaleFactorU,arrFX[iCurFx].m_fAnimorphicScaleFactorV);
					Vec2V vInnerRadAspect = Vec2V(fInnerRad) * vAspectRatio + fLen * ScaleFactors;
					ScalarV fOuterRad = fInnerRad + ScalarV(arrFX[iCurFx].fWidthRotate);
					Vec2V vOuterRadAspect = Vec2V(fOuterRad) * vAspectRatio + fLen * ScaleFactors;
					ScalarV fDistanceOffset = ScalarV(arrFX[iCurFx].fDistanceInnerOffset) * fScaleFactor;
					Vec2V vDistOffset = vFlareDirNorm * Vec2V(-fDistanceOffset, fDistanceOffset);

					// Calculate the current U value for the texture
					// and increment by fUstep each iteration
					float fU  = cp == 0 ? arrFX[iCurFx].fCurrentScroll : -arrFX[iCurFx].fCurrentScroll;
					float fUstep = 2.0f * PI * arrFX[iCurFx].fSize / (m_Settings[m_CurrentSettingsIndex].m_fChromaticTexUSize * static_cast<float>(iTotalVerts));

					for(int iCurEntry=0; iCurEntry <= iTotalEntries; iCurEntry++)
					{
						const Vec2V& vSinCos = CVfxHelper::GetSinCosValue(iTotalEntries, iCurEntry % iTotalEntries);

						Vec2V vInnerPos = AddScaled(vFlarePos, vInnerRadAspect, vSinCos) + vDistOffset;
						Vec2V vOuterPos = AddScaled(vFlarePos, vOuterRadAspect, vSinCos);

						float fFadeFactor = Clamp((ScalarV(V_HALF) * Dot(vFlareDirNorm * vOneNegOne, vSinCos) + ScalarV(V_HALF)) * 
												ScalarV(m_Settings[m_CurrentSettingsIndex].m_fChromaticFadeFactor), ScalarV(V_ZERO), ScalarV(V_ONE)).Getf();
						Color32 finalColor = Lerp(fFadeFactor * m_CurrentFade, arrFX[iCurFx].color, Color_black);

						finalColor = Lerp(m_CurrentFade, finalColor, Color_black);

						grcColor(finalColor);

						grcTexCoord2f(fU, 1.0f);
						grcVertex2f(vOuterPos);

						grcTexCoord2f(fU, 0.0f);
						grcVertex2f(vInnerPos);

						fU += fUstep;
					}

					grcEnd();
				}
			}
		}
		else
		{
			// Vertex positions			
			float x1 = -fHalfSize;
			float y1 = fHalfSize;
			float x2 = fHalfSize;
			float y2 = -fHalfSize;

			if(arrFX[iCurFx].nTexture != FT_ARTEFACT)
			{
				x1 -= fLen.Getf() * arrFX[iCurFx].m_fAnimorphicScaleFactorU;
				y1 += fLen.Getf() * arrFX[iCurFx].m_fAnimorphicScaleFactorV;
				x2 += fLen.Getf() * arrFX[iCurFx].m_fAnimorphicScaleFactorU;
				y2 -= fLen.Getf() * arrFX[iCurFx].m_fAnimorphicScaleFactorV;
			}
	
			// UV coords
			const Vector4& vUV = g_arrUV[arrFX[iCurFx].nSubGroup];

			Color32 finalColor;
			finalColor.Setf(
				Min(1.0f, color.GetRedf() * arrFX[iCurFx].color.GetRedf()),
				Min(1.0f, color.GetGreenf() * arrFX[iCurFx].color.GetGreenf()),
				Min(1.0f, color.GetBluef() * arrFX[iCurFx].color.GetBluef()),
				Min(1.0f, color.GetAlphaf() * arrFX[iCurFx].color.GetAlphaf()) );

			if(arrFX[iCurFx].nTexture == FT_ARTEFACT)
			{
				float fFadeFactor = Clamp((0.707f - fLen.Getf()) * m_Settings[m_CurrentSettingsIndex].m_fArtefactDistanceFadeFactor, 0.0f, 1.0f);
				finalColor = Lerp(fFadeFactor * fFadeFactor, finalColor, Color_black);
			}

			finalColor = Lerp(m_CurrentFade, finalColor, Color_black);

			// Render everything with rotation other than coronas that are set not to rotate
			if(arrFX[iCurFx].nTexture != FT_CORONA || arrFX[iCurFx].fWidthRotate != 0.0f)
			{
				ScalarV fDT;
				if(arrFX[iCurFx].nTexture == FT_ARTEFACT)
				{
					if(arrFX[iCurFx].m_bAlignRotationToSun)
					{
						Vector2 up(0.0f, -1.0f);
						Vector2 dirToLight(vDirFromLight.GetXf(), vDirFromLight.GetYf());
						float angle = up.Angle(dirToLight);

						fDT = ScalarV(dirToLight.x > 0.0f ? -angle : angle);
					}
					else
					{
						fDT = ScalarV(m_fExposureRotation * m_Settings[m_CurrentSettingsIndex].m_fArtefactRotationFactor);
					}
				}
				else if(arrFX[iCurFx].nTexture == FT_CORONA)
				{
					fDT = ScalarV(static_cast<float>(fwTimer::GetTimeInMilliseconds()) * 0.001f * arrFX[iCurFx].fWidthRotate);
				}
				else // arrFX[iCurFx].nTexture == FT_ANIMORPHIC
				{
					fDT = ScalarV(arrFX[iCurFx].fWidthRotate * PI);
				}

				Vec2V vSinCos = Sin(Vec2V(fDT, fDT + ScalarV(V_PI_OVER_TWO)));
				Vec2V vPos1 = Vec2V(ScalarV(x2), ScalarV(y1));
				Vec2V vPos2 = Vec2V(ScalarV(x1), ScalarV(y1));
				Vec2V vPos3 = Vec2V(ScalarV(x2), ScalarV(y2));
				Vec2V vPos4 = Vec2V(ScalarV(x1), ScalarV(y2));
				Vec2V Rot1 = Vec2V(vSinCos.GetY(), vSinCos.GetX());
				Vec2V Rot2 = Vec2V(-vSinCos.GetX(), vSinCos.GetY());

				Vec2V finalAspect = vAspectRatio;

				if(arrFX[iCurFx].nTexture == FT_ARTEFACT)
				{
					finalAspect.SetXf(finalAspect.GetXf() + fLen.Getf() * arrFX[iCurFx].m_fAnimorphicScaleFactorU);
					finalAspect.SetYf(finalAspect.GetYf() + fLen.Getf() * arrFX[iCurFx].m_fAnimorphicScaleFactorV);	
				}

				grcBegin(drawTriStrip, 4);
				grcColor(finalColor);
				grcTexCoord2f(vUV.z, vUV.y);
				grcVertex2f(AddScaled(vFlarePos ,AddScaled(Rot1 * vPos1.GetX(), Rot2, vPos1.GetY()), finalAspect));
				grcTexCoord2f(vUV.x, vUV.y);
				grcVertex2f(AddScaled(vFlarePos ,AddScaled(Rot1 * vPos2.GetX(), Rot2, vPos2.GetY()), finalAspect));
				grcTexCoord2f(vUV.z, vUV.w);
				grcVertex2f(AddScaled(vFlarePos ,AddScaled(Rot1 * vPos3.GetX(), Rot2, vPos3.GetY()), finalAspect));
				grcTexCoord2f(vUV.x, vUV.w);
				grcVertex2f(AddScaled(vFlarePos ,AddScaled(Rot1 * vPos4.GetX(), Rot2, vPos4.GetY()), finalAspect));
				grcEnd();
			}
			else
			{
				x1 = vFlarePos.GetXf() + x1;
				y1 = vFlarePos.GetYf() + y1 * fAspectRatio;
				x2 = vFlarePos.GetXf() + x2;
				y2 = vFlarePos.GetYf() + y2 * fAspectRatio;

				grcDrawSingleQuadf(x1, y1, x2, y2, 0.1f, vUV.x, vUV.y, vUV.z, vUV.w, finalColor);
			}
		}

		flareSprite.EndCustomList();
	}

#if __BANK
	if(m_enablePicking)
	{
		if(indexOfSelected >= 0)
		{
			//we only want to update we move the picker, otherwise it's weird if you move the position of the current selection
			if(m_flareFxPickerValue != m_previousPickerValue || m_pSelectedGroup->GetChild() == NULL)
			{	
				if(m_pSelectedGroup->GetChild())
				{
					m_pSelectedGroup->GetChild()->Destroy();
				}

				//add a copy of the edit widget for easy reference
				AddSingleFXBank(LF_SUN, indexOfSelected, m_pSelectedGroup, false);
				m_currentlySelected = indexOfSelected;
				m_previousPickerValue = m_flareFxPickerValue;
			}

			arrFX[m_currentlySelected].isSelectedByPicker = true;

			//draw a brigher version of the previous circle to indicate it's selected
			Vec2V selectedPos = AddScaled(vPos, vFlareDir, ScalarV(arrFX[m_currentlySelected].worldSpacePos));
			grcDebugDraw::Circle(selectedPos, arrFX[m_currentlySelected].fSize, arrFX[m_currentlySelected].color, false);
		}
		else if(m_pSelectedGroup && m_pSelectedGroup->GetChild())
		{
			m_pSelectedGroup->GetChild()->Destroy();
		}
	}
#endif

	// Restore globals
	CShaderLib::SetGlobalEmissiveScale(1.0f);
}

void CLensFlare::InitFlareFX()
{
	ASSERT_ONLY(bool bFileLoaded =) PARSER.LoadObject("common:/data/lensflare_m", "xml", m_Settings[0]);
	vfxAssertf(bFileLoaded, "Failed to load common:/data/lensflare_m.xml");

	ASSERT_ONLY(bFileLoaded =) PARSER.LoadObject("common:/data/lensflare_f", "xml", m_Settings[1]);
	vfxAssertf(bFileLoaded, "Failed to load common:/data/lensflare_f.xml");

	ASSERT_ONLY(bFileLoaded =) PARSER.LoadObject("common:/data/lensflare_t", "xml", m_Settings[2]);
	vfxAssertf(bFileLoaded, "Failed to load common:/data/lensflare_t.xml");
}

void CLensFlare::PrepareSunVisibility(grcViewport* pViewport)
{
	// Prepare the viewport
	pViewport->SetNearFarClip(1.0f, 25000.0f);
	pViewport->SetWorldIdentity();

	// Infinite projection matrix
	Mat44V proj = pViewport->GetProjection();

	// Take projection matrix far-plane to infinity and 
	// adjust appropriately
	float epsilon = 0.000001f;
	proj.GetCol2Ref().SetZf(epsilon - 1.0f);
	proj.GetCol2Ref().SetW(ScalarV(V_NEGONE));
	proj.GetCol3Ref().SetZ(ScalarV(V_ZERO));

	// This ensures that before the perspective divide 
	// we that [x, y, z, z] -> [x/z, y/z, 1.0, 1.0]
	pViewport->SetProjection(proj);

	// Clip the depth range so we can use depth test
	const int viewX = pViewport->GetUnclippedWindow().GetX();
	const int viewY = pViewport->GetUnclippedWindow().GetY();
	const int viewW = pViewport->GetUnclippedWindow().GetWidth();
	const int viewH = pViewport->GetUnclippedWindow().GetHeight();
	pViewport->SetWindow(viewX, viewY, viewW, viewH, 1.0f, 1.0f);

	grcStateBlock::SetDepthStencilState(DSS_SunVisibility, DEFERRED_MATERIAL_CLEAR);
	grcStateBlock::SetBlendState(BS_SunVisibility, ~0U,~0ULL);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);
}

void CLensFlare::CalcSunVisibility(Vec3V_In vSunPos, float fSunRadius)
{
	PF_PUSH_TIMEBAR_DETAIL("CalcSunVisibility");

	// Cache the previous viewport and make a local copy that will get modified
	grcViewport* pPrevViewport = grcViewport::GetCurrent();
	grcViewport customViewport = *pPrevViewport;

	UpdateSunVisibilityFromQuery(ms_QueryResult[GetReadQueryIdx()], customViewport, vSunPos, fSunRadius);
	GRCDEVICE.BeginOcclusionQuery(m_SunOcclusionQuery[GetWriteQueryIdx()]);

	PrepareSunVisibility(&customViewport);

	// Set the modified viewport
	grcViewport::SetCurrent(&customViewport);

	m_pSunVisibilityShader->SetVar(m_SunPositionVar, vSunPos);
	m_pSunVisibilityShader->SetVar(m_SunFogFactorVar,m_Settings[m_CurrentSettingsIndex]. m_fSunFogFactor);

	if (m_pSunVisibilityShader->BeginDraw(grmShader::RMC_DRAW, true, m_SunVisibilityDrawTech))
	{
		PF_PUSH_TIMEBAR_DETAIL("DrawTriangleFan");
		m_pSunVisibilityShader->Bind(0);
		CVfxHelper::DrawTriangleFan(NUM_VERTS_SUN_FAN, fSunRadius, 1.0f);
		m_pSunVisibilityShader->UnBind();
		m_pSunVisibilityShader->EndDraw();
		PF_POP_TIMEBAR_DETAIL();
	}

	// End the occlusion query for the sun visible pixels
	GRCDEVICE.EndOcclusionQuery(m_SunOcclusionQuery[GetWriteQueryIdx()]);
	ms_QuerySubmited[GetWriteQueryIdx()] = true;

	// Restore states
	grcStateBlock::SetStates(grcStateBlock::RS_NoBackfaceCull, grcStateBlock::DSS_IgnoreDepth, grcStateBlock::BS_Add);

	// Restore the viewport
	grcViewport::SetCurrent(pPrevViewport);

	PF_POP_TIMEBAR_DETAIL();
}

void CLensFlare::UpdateSunVisibilityFromQuery(u32 queryResult, grcViewport &customViewport, Vec3V_In vSunPos, float fSunRadius)
{
	float fNewSunVisibility = static_cast<float>(queryResult); // Get the amount of pixels that where visible

	// To normalize the query result we need to come up with some estimate to how many pixels the sun should have covered if it was fully visible
	// For that we are going to find the perspective W at the sun position, calculate the radius size in pixels and find the circle area in pixels
	// Last thing is a small hack to take into account the fact that the sun is facing the camera position, not the plane (this means the sun area
	//  is bigger when its close to the edge of the view)				
	Vec4V vSunPosView = Multiply(customViewport.GetViewMtx(), Vec4V(vSunPos, ScalarV(V_ONE))); // Transform to view space
	float fTanFOV = customViewport.GetTanHFOV();
	float fViewWidthAtSunZ = fTanFOV * vSunPosView.GetZf(); // Find the perspective devision value at the suns center distance from the cam
	float fViewPixelHalfWidth = static_cast<float>(customViewport.GetWidth() >> 1);
	float fSunPixelRad = fSunRadius / fViewWidthAtSunZ * fViewPixelHalfWidth; // Sun radius in pixels
	float fEstimateSunPixelCount = PI * fSunPixelRad * fSunPixelRad;
	float fDot = Abs(Dot(customViewport.GetCameraMtx().c(), Normalize(vSunPos))).Getf();
	fEstimateSunPixelCount /= fDot; // Take into account the angle of the suns direction to the camera

	fNewSunVisibility /= fEstimateSunPixelCount; // Normalize the query result by the estimate

	// Take the timecycle visibility multiplayer value into account
	float tcVisMul = g_timeCycle.GetCurrRenderKeyframe().GetVar(TCVAR_LENSFLARE_VISIBILITY);
	fNewSunVisibility *= tcVisMul;

	m_fSunVisibility = Min(1.0f, fNewSunVisibility); // Clamp to one to avoid very high values

	//cache the smallest none zero value of the sun visibility, otherwise we can't do a fade out when the sun disappears
	//as it immediately multiplies out the flare intensity causing it to disappear instantly
	if(m_fSunVisibility > 0.0f)
	{
		m_fCachedSunVisibility = m_fSunVisibility;
	}
}

void CLensFlare::TriggerTransition(E_FLARE_TRANSITIONS type, u32 lengthMS, bool forceTransition)
{
	//only transition in when the sun is visible and we're not already transitioning
	if( (forceTransition == false) && (m_Transition != FT_NONE || (type == FT_IN && m_fSunVisibility == 0.0f)) )
	{
		return;
	}

	m_Transition = type;
	m_LengthMS = lengthMS;

	m_StartTimeMS = fwTimer::GetTimeInMilliseconds();
}

void CLensFlare::ProcessTransition()
{
	if(m_Transition != FT_NONE)
	{
		float transitionProgress = (float)(fwTimer::GetTimeInMilliseconds()  - m_StartTimeMS) / (float)m_LengthMS;
		
		if(transitionProgress >= 1.0f)
		{
			m_CurrentFade = m_Transition == FT_OUT ?  1.0f : 0.0f;
			m_Transition = FT_NONE;
		}
		else
		{
			m_CurrentFade = m_Transition == FT_OUT ? transitionProgress : 1.0f - transitionProgress;
		}
	}
}

#if __BANK
void CLensFlare::AddFX(CallbackData pCallbackData)
{
	// Decode the FX to be deleted and delete it
	int iEncoded = static_cast<int>((size_t)pCallbackData);
	int iType = iEncoded & 0xFF;
	u8 nFx = static_cast<u8>(iEncoded >> 8);
	
	// Add an empty FX
	CFlareFX fx;
	fx.fDistFromLight = m_fxInsertPoints[nFx];
	fx.fSize = 0.1f;
	fx.fWidthRotate = 0.0f;
	fx.fDistanceInnerOffset = 0.0f;
	fx.fIntensityScale = 0.1f;
	fx.nTexture = nFx;
	fx.nSubGroup = nFx == FT_ARTEFACT ? 1 : 0;
	fx.pFXGroup = NULL;

	// Try to reuse
	bool bReused = false;
	for(int iCurType = 0; iCurType < LF_COUNT; iCurType++)
	{
		for(int iCurFX = 0; iCurFX < m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.GetCount(); iCurFX++)
		{
			if(m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX].pFXGroup == NULL)
			{
				// There is still room so just add another one
				m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX] = fx;

				// Update the bank group
				AddFXBank(iType, iCurFX);

				bReused = true;

				// Kill the loop
				iCurType = LF_COUNT;
				break;
			}
		}
	}

	if(!bReused)
	{
		if(m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iType].arrFlareFX.GetCount() < m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iType].arrFlareFX.GetCapacity())
		{
			// There is still room so just add another one
			m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iType].arrFlareFX.Push(fx);

			// Update the bank group
			AddFXBank(iType, m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iType].arrFlareFX.GetCount()-1);
		}
		else
		{
			// No more room - we have to re-create the whole thing
			// We do this because push and grow will re-allocate the array members which breaks the connection with the RAG widgets
			m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iType].arrFlareFX.PushAndGrow(fx);

			// Update the bank group
			bkBank* pVfxBank = vfxWidget::GetBank();
			InitFXWidgets(pVfxBank, true);
		}
	}
}

void CLensFlare::AddFXBank(int iType, int iFX)
{
	// Find the group this FX goes in
	bkWidget *pTypeGroupWidget = g_pFXEditGroup->GetChild();
	for(int iCurType = 0; pTypeGroupWidget != NULL; pTypeGroupWidget = pTypeGroupWidget->GetNext(), iCurType++)
	{
		if(iCurType == iType)
		{
			break;
		}
	}
	if(!Verifyf(pTypeGroupWidget != NULL, "Lens flare add FX couldn't find the type %d", iType))
	{
		return;
	}
	bkGroup* pTypeGroup = dynamic_cast<bkGroup*>(pTypeGroupWidget);
	if(!Verifyf(pTypeGroup != NULL, "Lens flare add FX couldn't bad cast?"))
	{
		return;
	}

	// Add the FX widgets
	AddSingleFXBank(iType, iFX, pTypeGroup, true);
}

void CLensFlare::AddSingleFXBank(int iType, int iFX, bkGroup* pTypeGroup, bool linkWidget)
{
	CFlareFX& newFX = m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iType].arrFlareFX[iFX];
	bkGroup* pFXGroup = NULL;
	if(newFX.nTexture == FT_ANIMORPHIC)
	{
		pFXGroup = pTypeGroup->AddGroup("Animorphic FX", false);
		pFXGroup->AddToggle("Selected", &newFX.isSelectedByPicker);
		pFXGroup->AddToggle("Behave Like Artefact", &newFX.m_bAnimorphicBehavesLikeArtefact);
		pFXGroup->AddSlider("Distance from light", &newFX.fDistFromLight, -2.5f, 2.5f, 0.1f);
		pFXGroup->AddSlider("Size", &newFX.fSize, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor U", &newFX.m_fAnimorphicScaleFactorU, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor V", &newFX.m_fAnimorphicScaleFactorV, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Rotation", &newFX.fWidthRotate, -0.5f, 0.5f, 0.01f);
		pFXGroup->AddSlider("Intensity Scale", &newFX.fIntensityScale, 0.0f, 200.0f, 0.1f);
		pFXGroup->AddColor("Color", &newFX.color);
	}
	else if(newFX.nTexture == FT_ARTEFACT)
	{
		pFXGroup = pTypeGroup->AddGroup("Artefact FX", false);
		pFXGroup->AddToggle("Selected", &newFX.isSelectedByPicker);
		pFXGroup->AddSlider("Distance from light", &newFX.fDistFromLight, -2.5f, 2.5f, 0.1f);
		pFXGroup->AddSlider("Size", &newFX.fSize, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor U", &newFX.m_fAnimorphicScaleFactorU, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor V", &newFX.m_fAnimorphicScaleFactorV, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Intensity Scale", &newFX.fIntensityScale, 0.0f, 200.0f, 0.1f);
		pFXGroup->AddToggle("Align Rotation To Sun", &newFX.m_bAlignRotationToSun);
		pFXGroup->AddColor("Color", &newFX.color);
		pFXGroup->AddSlider("Gradient Multiplier", &newFX.m_fGradientMultiplier, 0.0f, 2.0f, 0.01f);
		Assertf(g_arrUV.GetCount() > 1, "Error, possibly incorrect UV count");
		pFXGroup->AddSlider("UV Group", &newFX.nSubGroup, 1, u8(g_arrUV.GetCount() - 1), 1);
		pFXGroup->AddSlider("U position offset", &newFX.m_fPositionOffsetU, -0.5f, 0.5f, 0.01f);
	}
	else if(newFX.nTexture == FT_CHROMATIC)
	{
		pFXGroup = pTypeGroup->AddGroup("Chromatic FX", false);
		pFXGroup->AddToggle("Selected", &newFX.isSelectedByPicker);
		pFXGroup->AddSlider("Distance from light", &newFX.fDistFromLight, 0.0f, 2.0f, 0.1f);
		pFXGroup->AddSlider("Size", &newFX.fSize, 0.0f, 2.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor U", &newFX.m_fAnimorphicScaleFactorU, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor V", &newFX.m_fAnimorphicScaleFactorV, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Width", &newFX.fWidthRotate, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scroll Speed", &newFX.fScrollSpeed, 0.0f, 1.0f, 0.01f);
		pFXGroup->AddSlider("Distance Inner Offset", &newFX.fDistanceInnerOffset, 0.0f, 2.0f, 0.01f);
		pFXGroup->AddSlider("Intensity Scale", &newFX.fIntensityScale, 0.0f, 200.0f, 0.1f);
		pFXGroup->AddSlider("Intensity Center Fade", &newFX.fIntensityFade, 0.0f, 100.0f, 0.1f);
		pFXGroup->AddColor("Color", &newFX.color);
		pFXGroup->AddSlider(" Texture color desaturation", &newFX.m_fTextureColorDesaturate, 0.0f, 1.0f, 0.01f);
		
	}
	else if(newFX.nTexture == FT_CORONA)
	{
		pFXGroup = pTypeGroup->AddGroup("Corona FX", false);
		pFXGroup->AddToggle("Selected", &newFX.isSelectedByPicker);
		pFXGroup->AddSlider("Distance from light", &newFX.fDistFromLight, 0.0f, 1.5f, 0.1f);
		pFXGroup->AddSlider("Size", &newFX.fSize, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor U", &newFX.m_fAnimorphicScaleFactorU, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Scale Factor V", &newFX.m_fAnimorphicScaleFactorV, 0.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Rotation", &newFX.fWidthRotate, -5.0f, 5.0f, 0.01f);
		pFXGroup->AddSlider("Intensity Scale", &newFX.fIntensityScale, 0.0f, 200.0f, 0.1f);
		pFXGroup->AddColor("Color", &newFX.color);
	}
	else
	{
		Assertf(0, "unknown flare texture %d", newFX.nTexture);
		return;
	}
	pFXGroup->AddButton("Remove", datCallback(MFA2(CLensFlare::RemoveFX), (datBase*)this, (CallbackData)(void*)pFXGroup));
	if(linkWidget)
	{
		newFX.pFXGroup = pFXGroup;
	}
}

void CLensFlare::RemoveFX(CallbackData pCallbackData)
{
	bkWidget* pFXDeleteGroup = static_cast<bkWidget*>(pCallbackData);

	for(int iCurType = 0; iCurType < LF_COUNT; iCurType++)
	{
		for(int iCurFX = 0; iCurFX < m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.GetCount(); iCurFX++)
		{
			if(m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX].pFXGroup == pFXDeleteGroup)
			{
				Printf("Removing lens flare FX %s\n", pFXDeleteGroup->GetTitle());
				m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX].pFXGroup = NULL; // Indicate this slot is not in use
				pFXDeleteGroup->Destroy();
				pFXDeleteGroup = NULL;
				iCurType = LF_COUNT; // Kill the main loop
				break;
			}
		}
	}
	Assertf(pFXDeleteGroup == NULL, "Failed to remove lens flare %s FX", pFXDeleteGroup->GetTitle());
}

void CLensFlare::ClearAll()
{
	for(int iCurType = 0; iCurType < LF_COUNT; iCurType++)
	{
		for(int iCurFX = 0; iCurFX < m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.GetCount(); iCurFX++)
		{
			if(m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX].pFXGroup != NULL)
			{
				bkWidget* pFXDeleteGroup = m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX].pFXGroup;
				Printf("Removing lens flare FX %s\n", pFXDeleteGroup->GetTitle());
				m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX].pFXGroup = NULL; // Indicate this slot is not in use
				pFXDeleteGroup->Destroy();
			}
		}
	}
}

void CLensFlare::Load()
{
	static const int iFileNameSize = 512;
	char filename[iFileNameSize] = { 0 };
	if(BANKMGR.OpenFile( filename, iFileNameSize, "*.xml", false, "Load Lens Flare XML" ))
	{
		ASSERT_ONLY(bool bFileLoaded =) PARSER.LoadObject(filename, "", m_Settings[m_CurrentSettingsIndex]);
		vfxAssertf(bFileLoaded, "Failed to load %s", filename);
	}

	// Update the bank group
	bkBank* pVfxBank = vfxWidget::GetBank();
	InitFXWidgets(pVfxBank, true);
}

void CLensFlare::Save()
{
	bool bAnyDeleted = false;
	for(int iCurType = 0; iCurType < LF_COUNT; iCurType++)
	{
		for(int iCurFX = 0; iCurFX < m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.GetCount(); iCurFX++)
		{
			if(m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX[iCurFX].pFXGroup == NULL)
			{
				m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.Delete(iCurFX);
				bAnyDeleted = true;
			}
		}
	}

	// Rebuild the widgets if anything was deleted
	if(bAnyDeleted)
	{
		// Update the bank group
		bkBank* pVfxBank = vfxWidget::GetBank();
		InitFXWidgets(pVfxBank, true);
	}

	static const int iFileNameSize = 512;
	char filename[iFileNameSize] = { 0 };
	if(BANKMGR.OpenFile( filename, iFileNameSize, "*.xml", true, "Save Lens Flare XML" ))
	{
		ASSERT_ONLY(bool bFileSaved =) PARSER.SaveObject(filename, "", &m_Settings[m_CurrentSettingsIndex]);
		vfxAssertf(bFileSaved, "Failed to save %s", filename);
	}
}

void CLensFlare::SortElementsByDistance()
{
	for(int iCurType = 0; iCurType < LF_COUNT; iCurType++)
	{
		atArray<CFlareFX>& arrFX = m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX;
		std::sort(arrFX.begin(), arrFX.end(), &CLensFlare::CompareDistance);
	}

	bkBank* pVfxBank = vfxWidget::GetBank();
	InitFXWidgets(pVfxBank, true);
}


bool CLensFlare::CompareDistance(const CFlareFX& right, const CFlareFX& left)
{
	return right.worldSpacePos < left.worldSpacePos;
}

void CLensFlare::InitFXWidgets(bkBank* pBank, bool bOpen)
{
	// Grow all the arrays so we can add stuff to them with out growing
	for(int iCurType = 0; iCurType < LF_COUNT; iCurType++)
	{
		int iOldCount = m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.GetCount();
		m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.ResizeGrow(iOldCount + 16);
		m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX.Resize(iOldCount);
	}

	// Remove the old edit stuff and create everything from scratch
	if(g_pFXEditGroup != NULL)
	{
		pBank->DeleteGroup(*g_pFXEditGroup);
	}
	g_pFXEditGroup = pBank->PushGroup("Lens FX Editor", bOpen);

		for(int iCurType = 0; iCurType < LF_COUNT; iCurType++)
		{
			bkGroup* pCurTypeGroup = pBank->PushGroup("Sun FX", false);
				atArray<CFlareFX>& arrFX = m_Settings[m_CurrentSettingsIndex].m_arrFlareTypeFX[iCurType].arrFlareFX;
				for(int iCurFx = 0; iCurFx < arrFX.GetCount(); iCurFx++)
				{
					AddSingleFXBank(iCurType, iCurFx, pCurTypeGroup, true);
				}
			pBank->PopGroup();
		}

		pBank->PushGroup("Lens Editor");

			pBank->AddToggle("Enable picker", &m_enablePicking);
			pBank->AddSlider("Fx Picker", &m_flareFxPickerValue, -0.2f, 1.0f, 0.001f);

			m_pSelectedGroup = pBank->PushGroup("Current Selection");
			pBank->PopGroup();

			int type = LF_SUN;

			pBank->AddButton("Sort Elements By Distance", datCallback(MFA2(CLensFlare::SortElementsByDistance), (datBase*)this));
			pBank->AddButton("Add Animorphic FX", datCallback(MFA2(CLensFlare::AddFX), (datBase*)this, (CallbackData)(void*)(size_t)((type & 0xFF) | (0 << 8))));
			pBank->AddButton("Add Artefact FX", datCallback(MFA2(CLensFlare::AddFX), (datBase*)this, (CallbackData)(void*)(size_t)((type & 0xFF) | (1 << 8))));
			pBank->AddButton("Add Chromatic FX", datCallback(MFA2(CLensFlare::AddFX), (datBase*)this, (CallbackData)(void*)(size_t)((type & 0xFF) | (2 << 8))));
			pBank->AddButton("Add Corona FX", datCallback(MFA2(CLensFlare::AddFX), (datBase*)this, (CallbackData)(void*)(size_t)((type & 0xFF) | (3 << 8))));

			pBank->AddButton("Clear All", datCallback(MFA2(CLensFlare::ClearAll), (datBase*)this));
			pBank->AddButton("Load", datCallback(MFA2(CLensFlare::Load), (datBase*)this));
			pBank->AddButton("Save", datCallback(MFA2(CLensFlare::Save), (datBase*)this));

			pBank->PushGroup("Transition");

				pBank->AddButton("Test Transition", datCallback(MFA2(CLensFlare::TransitionSettingsRandom), (datBase*)this));
				pBank->AddSlider("Transition Length MS", &m_SwitchLengthMS, 0, 4000, 1);

			pBank->PopGroup();

		pBank->PopGroup();

	pBank->PopGroup();
}

void CLensFlare::InitWidgets()
{
	bkBank* pVfxBank = vfxWidget::GetBank();
	pVfxBank->PushGroup("Lens Flare", false);
	pVfxBank->AddToggle("Enabled", &m_bEnabled);
	pVfxBank->PushGroup("Sun Debug", false);
	pVfxBank->AddSlider("Sun Visibility", &m_fSunVisibility, 0.0f, 1000000.0f, -1);
	pVfxBank->AddSlider("Sun Visibility Fog Block", &m_Settings[m_CurrentSettingsIndex].m_iSunVisibilityAlphaClip, 0, 255, 1);
	pVfxBank->AddSlider("Sun Fog Factor", &m_Settings[m_CurrentSettingsIndex].m_fSunFogFactor, 0.0f, 1.0f, 0.01f);
	pVfxBank->AddColor("Sun Color", &g_SunColor);
	pVfxBank->AddSlider("Sun Visibility Factor", &m_Settings[m_CurrentSettingsIndex].m_fSunVisibilityFactor, 0.01f, 1.0f, 0.1f);
	pVfxBank->AddSlider("Current Exposure Squared", &m_fExposureSquared, 0.0f, 20.0f, -1);
	pVfxBank->AddSlider("Exposure Scale Factor", &m_Settings[m_CurrentSettingsIndex].m_fExposureScaleFactor, 0.01f, 50.0f, 0.01f);
	pVfxBank->AddSlider("Exposure Intensity Factor", &m_Settings[m_CurrentSettingsIndex].m_fExposureIntensityFactor, 0.01f, 50.0f, 0.01f);
	pVfxBank->AddSlider("Exposure Rotation Factor", &m_Settings[m_CurrentSettingsIndex].m_fExposureRotationFactor, 0.01f, 50.0f, 0.01f);
	pVfxBank->AddToggle("Ignore Exposure", &m_bIgnoreExposure);
	pVfxBank->AddSlider("Exposure scale clamp min", &m_Settings[m_CurrentSettingsIndex].m_fMinExposureScale, 0.0f, 20.0f, 0.1f);
	pVfxBank->AddSlider("Exposure scale clamp max", &m_Settings[m_CurrentSettingsIndex].m_fMaxExposureScale, 0.01f, 20.0, 0.1f);
	pVfxBank->AddSlider("Exposure intensity clamp min", &m_Settings[m_CurrentSettingsIndex].m_fMinExposureIntensity, 0.0f, 20.0f, 0.1f);
	pVfxBank->AddSlider("Exposure intensity clamp max", &m_Settings[m_CurrentSettingsIndex].m_fMaxExposureIntensity, 0.01f, 20.0f, 0.1f);
	pVfxBank->PopGroup();
	pVfxBank->AddToggle("Show light vectors", &g_bShowVecToEdge);
	pVfxBank->AddSlider("Chromatic UV scale", &m_Settings[m_CurrentSettingsIndex].m_fChromaticTexUSize, 0.01f, 2.0f, 0.01f);
	pVfxBank->AddSlider("Ring Fade Factor", &m_Settings[m_CurrentSettingsIndex].m_fChromaticFadeFactor, 0.0f, 15.0f, 0.1f);
	pVfxBank->AddSlider("Artefact Distance Fade Factor", &m_Settings[m_CurrentSettingsIndex].m_fArtefactDistanceFadeFactor, 0.0f, 15.0f, 0.1f);
	pVfxBank->AddSlider("Artefact Rotation Factor", &m_Settings[m_CurrentSettingsIndex].m_fArtefactRotationFactor, 0.0f, 50.0f, 0.1f);
	pVfxBank->AddSlider("Artefact Scale Factor", &m_Settings[m_CurrentSettingsIndex].m_fArtefactScaleFactor, 0.01f, 50.0f, 0.01f);
	pVfxBank->AddSlider("Artefact Gradient Factor", &m_Settings[m_CurrentSettingsIndex].m_fArtefactGradientFactor, 0.0f, 5.0f, 0.01f);
	pVfxBank->AddSlider("Corona Distance Additional Size", &m_Settings[m_CurrentSettingsIndex].m_fCoronaDistanceAdditionalSize, 0.0f, 5.0f, 0.01f);
	InitFXWidgets(pVfxBank);
	pVfxBank->PopGroup();

	m_bWidgetsCreated = true;
}
#endif // __BANK
