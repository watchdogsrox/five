// =============================================================================================== //
// INCLUDES
// =============================================================================================== //

// rage includes
#include "grmodel/shader.h"
#include "grcore/device.h"
#include "grprofile/pix.h"

// game includes
#include "debug/Rendering/DebugLighting.h"
#include "renderer/Deferred/DeferredLighting.h"
#include "renderer/Util/ShaderUtil.h"
#include "renderer/render_channel.h"

#include "system/xtl.h"

#define UPSAMPLE_BILATERAL_EDGE_THRESHOLD	(0.96f)
#define UPSAMPLE_BILATERAL_COEFFICIENT		(0.0016f)

#if __BANK
enum eUpsampleType
{
	UPSAMPLE_BILINEAR = 0,
	UPSAMPLE_BILATERAL,
	UPSAMPLE_BILINEAR_BILATERAL_COMBO,
	UPSAMPLE_TYPE_TOTAL

};

float UpsampleHelper::ms_bilateralCoefficientValue = UPSAMPLE_BILATERAL_COEFFICIENT;
float UpsampleHelper::ms_bilateralEdgeThresholdValue = UPSAMPLE_BILATERAL_EDGE_THRESHOLD;
int UpsampleHelper::ms_CompositeTechniqueID = UPSAMPLE_BILINEAR_BILATERAL_COMBO;
#endif

// ----------------------------------------------------------------------------------------------- //
static DECLARE_MTR_THREAD const grmShader* s_CurrentShader=NULL;

bool ShaderUtil::StartShader(
	const char *PIX_TAGGING_ONLY(name), 
	const grmShader *shader, 
	const grcEffectTechnique tech, 
	const u32 pass, 
	bool PIX_TAGGING_ONLY(addPIXAnnotation),
	int GENSHMOO_ONLY(shmooIdx)
	)
{
	Assert(s_CurrentShader==NULL);
	FatalAssert(shader != NULL);

	// Bind and enable vertex & fragment programs.
	if (Verifyf(shader->BeginDraw(grmShader::RMC_DRAW, true, tech), "ShaderUtil::StartShader() - Cannot start shader '%s', technique %d", shader->GetName(), tech))
	{
		if (Verifyf(pass < shader->GetPassCount(tech), "ShaderUtil::StartShader() - Invalid pass (%d) for shader '%s', technique %d",pass, shader->GetName(), tech))
		{
#if GENERATE_SHMOO
			ShmooHandling::BeginShmoo(shmooIdx, shader, pass);
#endif // GENERATE_SHMOO
			PIX_TAGGING_ONLY( if (addPIXAnnotation)	PF_PUSH_MARKER(name); );
			shader->Bind(pass);
			s_CurrentShader = shader;
			return true;
		}
		else
		{
			// make sure we unbind it if we failed.
			shader->EndDraw();
		}
	}

	return false;
}

// ----------------------------------------------------------------------------------------------- //

void ShaderUtil::EndShader(const grmShader *shader, bool PIX_TAGGING_ONLY(addPIXAnnotation), int GENSHMOO_ONLY(shmooIdx))
{
	if (s_CurrentShader)
	{
		Assert(shader==s_CurrentShader);
		s_CurrentShader = NULL;
		shader->UnBind(); 
#if GENERATE_SHMOO
		ShmooHandling::EndShmoo(shmooIdx);
#endif // GENERATE_SHMOO
		shader->EndDraw();
		PIX_TAGGING_ONLY( if (addPIXAnnotation)	PF_POP_MARKER(); );
	}
}

// ----------------------------------------------------------------------------------------------- //
#if __DEV
// ----------------------------------------------------------------------------------------------- //
void ShaderUtil::DumpShaderState(char* label)
{
	renderDebugf1("-= Start (%s) =-", label);
	renderDebugf1("-= End (%s) =-\n", label);
}

// ----------------------------------------------------------------------------------------------- //
#endif
// ----------------------------------------------------------------------------------------------- //
grcEffectVar		UpsampleHelper::ms_texelSizeId = grcevNONE;
grcEffectVar		UpsampleHelper::ms_bilateralCoefficientId = grcevNONE;
grcEffectVar		UpsampleHelper::ms_bilateralEdgeThresholdId = grcevNONE;
grcEffectVar		UpsampleHelper::ms_lowResDepthTextureId = grcevNONE;
grcEffectVar		UpsampleHelper::ms_hiResDepthTexrcEffectVar = grcevNONE;   
grcEffectVar		UpsampleHelper::ms_hiResDepthTextureId = grcevNONE; 
grcEffectVar		UpsampleHelper::ms_DepthTextureId = grcevNONE;

grcEffectTechnique	UpsampleHelper::ms_BilinearUpSampleTechnique = grcetNONE;
grcEffectTechnique	UpsampleHelper::ms_BilateralUpSampleTechnique = grcetNONE;
grcEffectTechnique	UpsampleHelper::ms_BilinearBilateralUpSampleTechniqueWithAlphaDiscard = grcetNONE;
grcEffectTechnique	UpsampleHelper::ms_BilinearBilateralUpSampleTechnique = grcetNONE;

bool			UpsampleHelper::ms_ShaderInitialized = false;

UpsampleHelper::UpsampleParams::UpsampleParams() :
	lockTarget(true),
	unlockTarget(true),
	lowResUpsampleSource(NULL), 
	highResUpsampleDestination(NULL), 
	lowResDepthTarget(NULL), 
	highResDepthTarget(NULL), 
	blendType(grcCompositeRenderTargetHelper::COMPOSITE_RT_BLEND_COMPOSITE_ALPHA), 
	maskUpsampleByAlpha(false)
{
}

void UpsampleHelper::Init()
{
	// Customise the shaders/vars used by grcCompositeHelper
	grcEffect& defaultEffect = GRCDEVICE.GetDefaultEffect();
	ms_texelSizeId = defaultEffect.LookupVar("TexelSize");
	ms_bilateralCoefficientId = defaultEffect.LookupVar("BilateralCoefficient");
	ms_bilateralEdgeThresholdId = defaultEffect.LookupVar("BilateralEdgeThreshold");
	
	ms_lowResDepthTextureId = defaultEffect.LookupVar( "LowResDepthPointMap" );
	ms_hiResDepthTextureId = defaultEffect.LookupVar("HiResDepthPointMap");
	ms_DepthTextureId = defaultEffect.LookupVar("DepthPointMap");
	
	grcEffectTechnique compositeTechnique = defaultEffect.LookupTechnique("BlitTransparentBilateralBlur");
	ms_BilinearUpSampleTechnique = defaultEffect.LookupTechnique("BlitTransparentBilinearBlur");
	ms_BilinearBilateralUpSampleTechniqueWithAlphaDiscard = defaultEffect.LookupTechnique("BlitTransparentBilinearBilateralWithAlphaDiscard");
	ms_BilinearBilateralUpSampleTechnique = defaultEffect.LookupTechnique("BlitTransparentBilinearBilateral");
	
	ms_BilateralUpSampleTechnique = compositeTechnique;

	ms_ShaderInitialized = true;
}

void UpsampleHelper::SetupBilateralShader(const UpsampleParams &params)
{
	if ( ms_lowResDepthTextureId )
	{
		GRCDEVICE.GetDefaultEffect().SetVar(ms_lowResDepthTextureId, params.lowResDepthTarget);
	}

	if ( ms_DepthTextureId )
	{		
		GRCDEVICE.GetDefaultEffect().SetVar(ms_DepthTextureId, params.highResDepthTarget);
	}

	if ( ms_texelSizeId )
	{
		float floatWidth = static_cast<float>(params.lowResUpsampleSource->GetWidth());
		float floatHeight = static_cast<float>(params.lowResUpsampleSource->GetHeight());
		Vector4 texelSize(1.0f / floatWidth, 1.0f / floatHeight, floatWidth, floatHeight);
		GRCDEVICE.GetDefaultEffect().SetVar(ms_texelSizeId, texelSize);
	}
	
	#if RSG_BANK
		if( ms_bilateralEdgeThresholdId )
		{
			GRCDEVICE.GetDefaultEffect().SetVar(ms_bilateralEdgeThresholdId, ms_bilateralEdgeThresholdValue);
		}
	#endif
}

void UpsampleHelper::SetupBilateralCoefficientInShader()
{
	if( ms_bilateralCoefficientId )
	{
		GRCDEVICE.GetDefaultEffect().SetVar(ms_bilateralCoefficientId, BANK_SWITCH(ms_bilateralCoefficientValue, UPSAMPLE_BILATERAL_COEFFICIENT));
	}
}

void UpsampleHelper::PerformUpsample(const UpsampleParams &params)
{
	if ( !Verifyf(ms_ShaderInitialized, "UpsampleHelper wasn't initialized before use!") )
	{
		return;
	}

	#if RSG_BANK
		if(ms_CompositeTechniqueID == UPSAMPLE_BILINEAR)
		{
			PerformBilinearUpsample(params);
		}
		else if(ms_CompositeTechniqueID == UPSAMPLE_BILATERAL)
		{
			PerformBilateralUpsample(params);
		}
		else
	#endif
		{
			PerformBilinearBilateralComboUpsample(params);
		}	
}

void UpsampleHelper::PerformBilinearUpsample(const UpsampleParams &params)
{
	Assert(params.highResUpsampleDestination);
	Assert(params.lowResUpsampleSource);

	PF_PUSH_MARKER("Bilinear Upsample");
	
	grcCompositeRenderTargetHelper::CompositeParams compositeParams;
	compositeParams.compositeTechnique = ms_BilinearUpSampleTechnique;
	compositeParams.srcColor = params.lowResUpsampleSource;
	compositeParams.dstColor = params.highResUpsampleDestination;
	compositeParams.resolveFlags = NULL;
	compositeParams.lockTarget = params.lockTarget;
	compositeParams.unlockTarget = params.unlockTarget;
	compositeParams.compositeRTblendType = params.blendType;

	grcCompositeRenderTargetHelper::CompositeDstToSrc(compositeParams);

	PF_POP_MARKER();
}

void UpsampleHelper::PerformBilateralUpsample(const UpsampleParams &params)
{
	Assert(params.highResUpsampleDestination);
	Assert(params.lowResUpsampleSource);
	Assert(params.highResDepthTarget);
	Assert(params.lowResDepthTarget);

	//Setup 
	SetupBilateralCoefficientInShader();
	SetupBilateralShader(params);
	PF_PUSH_MARKER("Bilateral Upsample");

	grcCompositeRenderTargetHelper::CompositeParams compositeParams;
	compositeParams.compositeTechnique = ms_BilateralUpSampleTechnique;
	compositeParams.srcColor = params.lowResUpsampleSource;
	compositeParams.dstColor = params.highResUpsampleDestination;
	compositeParams.srcDepth = params.lowResDepthTarget;
	compositeParams.dstDepth = params.highResDepthTarget;
	compositeParams.resolveFlags = NULL;
	compositeParams.lockTarget = params.lockTarget;
	compositeParams.unlockTarget = params.unlockTarget;
	compositeParams.compositeRTblendType = params.blendType;

	grcCompositeRenderTargetHelper::CompositeDstToSrc(compositeParams);

	PF_POP_MARKER();
}

void UpsampleHelper::PerformBilinearBilateralComboUpsample(const UpsampleParams &params)
{	
	Assert(params.highResUpsampleDestination);
	Assert(params.lowResUpsampleSource);
	Assert(params.highResDepthTarget);
	Assert(params.lowResDepthTarget);

	grcCompositeRenderTargetHelper::CompositeParams compositeParams;
	compositeParams.srcColor = params.lowResUpsampleSource;
	compositeParams.srcDepth = params.lowResDepthTarget;
	compositeParams.dstColor = params.highResUpsampleDestination;
	compositeParams.dstDepth = params.highResDepthTarget;
	compositeParams.compositeRTblendType = params.blendType;
	compositeParams.lockTarget = params.lockTarget;
	compositeParams.unlockTarget = params.unlockTarget;
	compositeParams.resolveFlags = NULL;
	compositeParams.depth = 0.0f;
	compositeParams.compositeTechnique = params.maskUpsampleByAlpha ? ms_BilinearBilateralUpSampleTechniqueWithAlphaDiscard : ms_BilinearBilateralUpSampleTechnique;

	PF_PUSH_MARKER("Bilinear/Bilateral Upsample");
		SetupBilateralCoefficientInShader();
		SetupBilateralShader(params);
		grcCompositeRenderTargetHelper::CompositeDstToSrc(compositeParams);
	PF_POP_MARKER();
}


#if __BANK

void UpsampleHelper::AddWidgets(bkBank *bk)
{

	bk->PushGroup("Upsample Helper");

	const char* compositeTechniqueTypes[UPSAMPLE_TYPE_TOTAL] = {"Bilinear", "Bilateral","Hybrid Bilinear/Bilateral"};

	bk->AddCombo("Composite Technique Type", &ms_CompositeTechniqueID, 3, compositeTechniqueTypes); 

	bk->AddSlider("Bilateral coefficient", &ms_bilateralCoefficientValue, 0.0001f, 0.1f, 0.0001f);
	bk->AddSlider("Bilateral Edge Threshold", &ms_bilateralEdgeThresholdValue, 0.0001f, 1.0f, 0.0001f);
	bk->PopGroup();
}
#endif
