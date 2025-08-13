// filename:	RenderPhaseLensDistortion.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "RenderPhaseLensDistortion.h"

#if 0 && RSG_PC && ENABLE_OVR
#include "../../../rage/3rdparty/OculusSDK/LibOVR/Include/OVR.h"
#include "../../../rage/3rdparty/OculusSDK/LibOVR/Src/OVR_Device.h"
#include "../../../rage/3rdparty/OculusSDK/LibOVR/Src/Util/Util_Render_Stereo.h"
#endif 

#include "grcore/quads.h"
#include "input/headtracking.h"
#include "Renderer/DrawLists/DrawListMgr.h"
#include "Renderer/RenderTargets.h"
#include "System/ControlMgr.h"

grcRenderTarget* CRenderPhaseLensDistortion::sm_Target = NULL;
grcRenderTarget* CRenderPhaseLensDistortion::sm_SrcLeft = NULL;
grcRenderTarget* CRenderPhaseLensDistortion::sm_SrcRight = NULL;

grmShader* CRenderPhaseLensDistortion::sm_pShader = NULL;
grcEffectTechnique CRenderPhaseLensDistortion::sm_LensDistortionTechnique = grcetNONE;

grcEffectVar CRenderPhaseLensDistortion::sm_TexTransformId;
grcEffectVar CRenderPhaseLensDistortion::sm_LensCenterId;
grcEffectVar CRenderPhaseLensDistortion::sm_ScreenCenterId;
grcEffectVar CRenderPhaseLensDistortion::sm_ScreenSizeHalfScaleId;
grcEffectVar CRenderPhaseLensDistortion::sm_ScaleId;
grcEffectVar CRenderPhaseLensDistortion::sm_ScaleInId;
grcEffectVar CRenderPhaseLensDistortion::sm_HwdWarpParamId;
grcEffectVar CRenderPhaseLensDistortion::sm_ChromAbParamId;
grcEffectVar CRenderPhaseLensDistortion::sm_SourceTexId;

Matrix44	CRenderPhaseLensDistortion::sm_matTexTransform;
Vector2		CRenderPhaseLensDistortion::sm_vLenCenter;
Vector2		CRenderPhaseLensDistortion::sm_vScreenCenter;
Vector2		CRenderPhaseLensDistortion::sm_vHalfScreenSizeScale;
Vector2		CRenderPhaseLensDistortion::sm_vScale;
Vector2		CRenderPhaseLensDistortion::sm_vScaleIn;
Vector4		CRenderPhaseLensDistortion::sm_vHwdWarpParam;
Vector4		CRenderPhaseLensDistortion::sm_vChromAbParam;

bool CRenderPhaseLensDistortion::sm_ChromaticAbberration = false;

float CRenderPhaseLensDistortion::sm_fAspectMod = 0.5f;
float CRenderPhaseLensDistortion::sm_fFOV = 111.0f;


#if 0 && RSG_PC && ENABLE_OVR
extern OVR::Util::Render::StereoConfig	sm_SConfig;
extern OVR::HMDInfo						sm_TheHMDInfo;
#endif // RSG_PC

#if __BANK
bool CRenderPhaseLensDistortion::sm_EnablePhase = true;
#endif // __BANK

PARAM(FOV, "Oculus FOV");

CRenderPhaseLensDistortion::CRenderPhaseLensDistortion(CViewport* pViewport) :
	CRenderPhase(pViewport, "LensDistortion", DL_RENDERPHASE_LENSDISTORTION, 0.0f, false)
{
#if 0 && RSG_PC && ENABLE_OVR
	sm_SConfig.SetFullViewport(OVR::Util::Render::Viewport(0, 0, GRCDEVICE.GetWidth(), GRCDEVICE.GetHeight()));
	sm_SConfig.SetStereoMode(OVR::Util::Render::Stereo_None); // Stereo_LeftRight_Multipass);

    // Configure proper Distortion Fit.
    // For 7" screen, fit to touch left side of the view, leaving a bit of
    // invisible screen on the top (saves on rendering cost).
    // For smaller screens (5.5"), fit to the top.
    if (sm_TheHMDInfo.HScreenSize > 0.0f)
    {
        if (sm_TheHMDInfo.HScreenSize > 0.140f)  // 7"
            sm_SConfig.SetDistortionFitPointVP(-1.0f, 0.0f);
        else        
            sm_SConfig.SetDistortionFitPointVP(0.0f, 1.0f);
    }

	PARAM_FOV.Get(sm_fFOV);

	sm_SConfig.Set2DAreaFov(DtoR * sm_fFOV);

    switch(sm_SConfig.GetStereoMode())
    {
	case OVR::Util::Render::Stereo_None:
        //Render(sm_SConfig.GetEyeRenderParams(StereoEye_Center));
        break;

	case OVR::Util::Render::Stereo_LeftRight_Multipass:
        //case Stereo_LeftDouble_Multipass:
        //Render(sm_SConfig.GetEyeRenderParams(StereoEye_Left));
        //Render(sm_SConfig.GetEyeRenderParams(StereoEye_Right));
        break;
	}
#endif // RSG_PC

	InitClass();
}

void CRenderPhaseLensDistortion::InitClass()
{
	USE_MEMBUCKET(MEMBUCKET_RENDER);

	ASSET.PushFolder("common:/shaders");
	
	sm_pShader = grmShaderFactory::GetInstance().Create();
	if ( !sm_pShader->Load("DistortionWarp") )
	{
		AssertMsg( sm_pShader, "Unable to load the DistortionWarp shader!" );
		ASSET.PopFolder();
		return;
	}
	ASSET.PopFolder();

	sm_LensDistortionTechnique = sm_pShader->LookupTechnique("DistortionWarp", true);

	sm_TexTransformId = sm_pShader->LookupVar( "Texm", true );
	sm_LensCenterId = sm_pShader->LookupVar( "LensCenter", true );
	sm_ScreenCenterId = sm_pShader->LookupVar( "ScreenCenter", true );
	sm_ScreenSizeHalfScaleId = sm_pShader->LookupVar( "ScreenSizeHalfScale", true );
	sm_ScaleId = sm_pShader->LookupVar( "Scale", true );
	sm_ScaleInId = sm_pShader->LookupVar( "ScaleIn", true );
	sm_HwdWarpParamId = sm_pShader->LookupVar( "HmdWarpParam", true );
	sm_ChromAbParamId = sm_pShader->LookupVar( "ChromAbParam", true );

	sm_SourceTexId = sm_pShader->LookupVar("SourceTexture", true );
}

void CRenderPhaseLensDistortion::BuildDrawList()
{
	SetActive(true BANK_ONLY( && sm_EnablePhase));
	BANK_ONLY(rage::ioHeadTracking::EnableMotionTracking(sm_EnablePhase));

	if (!rage::ioHeadTracking::IsMotionTrackingEnabled())
	{
		SetActive(false);
		return;
	}

	if (CControlMgr::GetKeyboard().GetKeyJustDown(KEY_F1, KEYBOARD_MODE_DEBUG_ALT))
	{
		rage::ioHeadTracking::Reset();
	}

	DrawListPrologue();
	DLC(dlCmdSetCurrentViewport, (&GetGrcViewport()));

#if 0 && RSG_PC && ENABLE_OVR
	// Set up viewport for rendering
	const OVR::Util::Render::Viewport& Viewport = sm_SConfig.GetFullViewport();
	int WindowWidth = GRCDEVICE.GetWidth();
	int WindowHeight = GRCDEVICE.GetHeight();

    float w = float(Viewport.w) / float(WindowWidth),
          h = float(Viewport.h) / float(WindowHeight),
          x = float(Viewport.x) / float(WindowWidth),
          y = float(Viewport.y) / float(WindowHeight);

    sm_matTexTransform.Set( w, 0, 0, x,
							0, h, 0, y,
							0, 0, 0, 0,
							0, 0, 0, 1);

	float aspectRatio = sm_fAspectMod * float(Viewport.w) / float(Viewport.h);

	const OVR::Util::Render::DistortionConfig& oDistortion = sm_SConfig.GetDistortionConfig();

	float fXOffset = (sm_SConfig.GetStereoMode() == OVR::Util::Render::Stereo_None) ? 0.0f : oDistortion.XCenterOffset;
	sm_vLenCenter = Vector2(x + (w + fXOffset * 0.5f)*0.5f, y + h*0.5f);
	sm_vScreenCenter = Vector2( x + w*0.5f, y + h*0.5f);
	sm_vHalfScreenSizeScale = (sm_SConfig.GetStereoMode() == OVR::Util::Render::Stereo_None) ?
								Vector2(0.5f, 0.5f) : Vector2(0.25f, 0.25f);

	float scaleFactor = 1.0f / oDistortion.Scale;
	sm_vScale   = Vector2((w/2) * scaleFactor, (h/2) * scaleFactor * aspectRatio);
	sm_vScaleIn = Vector2((2/w),               (2/h) / aspectRatio);
	sm_vHwdWarpParam = Vector4(oDistortion.K[0], oDistortion.K[1], oDistortion.K[2], oDistortion.K[3]);
	sm_vChromAbParam = Vector4(oDistortion.ChromaticAberration[0], oDistortion.ChromaticAberration[1], oDistortion.ChromaticAberration[2], oDistortion.ChromaticAberration[3]);
#endif // RSG_PC
	DLC_Add(CRenderPhaseLensDistortion::Render);
	DLC(dlCmdSetCurrentViewportToNULL, ());
	DrawListEpilogue();
}

void CRenderPhaseLensDistortion::UpdateViewport()
{
}

void CRenderPhaseLensDistortion::Render()
{
	sm_Target = CRenderTargets::GetUIBackBuffer();
#if RSG_DURANGO
	sm_SrcLeft = sm_SrcRight = GRCDEVICE.GetMSAA() ? CRenderTargets::GetOffscreenBuffer_Resolved(3) : CRenderTargets::GetOffscreenBuffer2();
#else
	sm_SrcLeft = sm_SrcRight = GRCDEVICE.GetMSAA() ? CRenderTargets::GetOffscreenBuffer_Resolved(2) : CRenderTargets::GetOffscreenBuffer2();
#endif
	// sm_SrcRight = CRenderTargets::GetOffscreenBuffer2();

#if RSG_PC
	// TODO - Don't copy.  Make destination source
	CRenderTargets::CopyTarget(sm_Target, sm_SrcLeft);
	//CRenderTargets::CopyTarget(sm_Target, sm_SrcRight);
#endif // RSG_PC
	
	Color32 color(0.0f,0.0f,0.0f,1.0f);
	grcTextureFactory::GetInstance().LockRenderTarget(0, sm_Target, NULL ); 
	GRCDEVICE.Clear( true, color, false, 0, false, 0);

	grcStateBlock::SetBlendState(grcStateBlock::BS_Default);
	grcStateBlock::SetDepthStencilState(grcStateBlock::DSS_IgnoreDepth);
	grcStateBlock::SetRasterizerState(grcStateBlock::RS_NoBackfaceCull);

	// TODO - Double Buffer all parameters
	sm_pShader->SetVar(sm_TexTransformId, sm_matTexTransform);
	sm_pShader->SetVar(sm_LensCenterId, sm_vLenCenter);
	sm_pShader->SetVar(sm_ScreenCenterId, sm_vScreenCenter);
	sm_pShader->SetVar(sm_ScreenSizeHalfScaleId, sm_vHalfScreenSizeScale);
	sm_pShader->SetVar(sm_ScaleId, sm_vScale);
	sm_pShader->SetVar(sm_ScaleInId, sm_vScaleIn);
	sm_pShader->SetVar(sm_HwdWarpParamId, sm_vHwdWarpParam);
	sm_pShader->SetVar(sm_ChromAbParamId, sm_vChromAbParam);
	sm_pShader->SetVar(sm_SourceTexId, sm_SrcLeft);

	DrawEye(false);
	DrawEye(true);

	grcResolveFlags resolveFlags;
	resolveFlags.NeedResolve = false;
	grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);	
}

void CRenderPhaseLensDistortion::DrawEye(bool bRightEye)
{
	// Offset to align texels with pixels
	float fOffsetX = 0.f;
	float fOffsetY = 0.f;
	grmShader* pShader = sm_pShader;

#if __XENON
	fOffsetX = 0.5f/pColorTarget->GetWidth();
	fOffsetY = 0.5f/pColorTarget->GetHeight();
#elif (RSG_PC && !__D3D11)
	//if (GRCDEVICE.GetDxVersion() < 1000)
	{
		fOffsetX = 0.5f/pColorTarget->GetWidth();
		fOffsetY = 0.5f/pColorTarget->GetHeight();
	}
#endif

	static Vector4 avQuad[2] = { Vector4(-1.f, 1.f, 0.f, -1.f), Vector4(0.f, 1.f, 1.f, -1.f) };
	u32 uEyeIndex = bRightEye ? 1 : 0;

	if ( pShader->BeginDraw( grmShader::RMC_DRAW, true, sm_LensDistortionTechnique ) )
	{
		pShader->Bind(sm_ChromaticAbberration ? 1 : 0);

		grcBeginQuads(1);
		grcDrawQuadf( avQuad[uEyeIndex].GetX(), avQuad[uEyeIndex].GetY(), avQuad[uEyeIndex].GetZ(), avQuad[uEyeIndex].GetW(), 0.f, fOffsetX, fOffsetY, 1.f+fOffsetX, 1.f+fOffsetY, Color32(1.0f,1.0f,1.0f,1.0f) );
		grcEndQuads();

		pShader->UnBind();
	}
	pShader->EndDraw();
}

#if __BANK
void CRenderPhaseLensDistortion::InitWidgets()
{
	bkBank* pBank = BANKMGR.FindBank("Renderer");
	if (pBank)
	{
		pBank->PushGroup("Lens Distortion", false);
		pBank->AddToggle("Chromatic Abberration Correction", &sm_ChromaticAbberration);
		pBank->AddToggle("Toggle Phase", &sm_EnablePhase);
		pBank->AddSlider("Aspect Modifier", &sm_fAspectMod, 0.0001f, 16.0f, 0.01f);
		pBank->AddSlider("FOV", &sm_fFOV, 15.0f, 180.0f, 1.0f);
		pBank->PopGroup();
	}
}
#endif // __BANK

void CRenderPhaseLensDistortion::SetRenderTargets(grcRenderTarget* pFinalTarget, grcRenderTarget* pSrcLeft, grcRenderTarget* pSrcRight)
{
	sm_Target = pFinalTarget;
	sm_SrcLeft = pSrcLeft;
	sm_SrcRight = pSrcRight;
}

