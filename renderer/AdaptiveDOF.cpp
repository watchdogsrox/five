

#include "grcore/config.h"
#include "renderer/PostFX_shared.h"

#if ADAPTIVE_DOF
#include "grcore/effect.h"
#include "grcore/fastquad.h"
#include "grcore/quads.h"
#if __D3D11
#include "grcore/rendertarget_d3d11.h"
#endif // __D3D11
#include "grcore/texture.h"
#include "grcore/texturedefault.h"
#include "grcore/buffer_gnm.h"

#include "profile/group.h"
#include "profile/page.h"

#include "AdaptiveDOF.h"
#include "peds/PedIntelligence.h"
#include "cutscene/CutSceneManagerNew.h"
#include "debug/DebugScene.h"
#include "frontend/Credits.h"
#include "renderer/Deferred/GBuffer.h"
#include "renderer/Util/ShaderUtil.h"
#include "timecycle/TimeCycleConfig.h"

PF_PAGE(adaptiveDofPage, "Adaptive DOF");

PF_GROUP(adaptiveDof);
PF_LINK(adaptiveDofPage, adaptiveDof);

PF_VALUE_FLOAT(desiredFocusDistance, adaptiveDof);
PF_VALUE_FLOAT(dampedFocusDistance, adaptiveDof);

namespace PostFX
{

#if ADAPTIVE_DOF_GPU
enum adaptiveDOFPass
{
	pp_depthParallelReductionCS = 0,
	pp_depthDownsample,
	pp_adaptiveDOFcalc,
#if __BANK
	pp_DEBUG_drawFocalGrid,
#endif
};
#if POSTFX_UNIT_QUAD
namespace adaptiveDOFQuad
{
	static grcEffectVar			Position;
	static grcEffectVar			TexCoords;
	static grcEffectVar			Scale;
}
#endif //POSTFX_UNIT_QUAD
#endif

PARAM(noAdaptivedof,"Disable adaptive dof");
PARAM(AdaptivedofReadback,"Disable adaptive dof");

#if __BANK
bool gAdaptiveDOFReadback = false;
#endif 

const float g_FovForUnityPixelDepthPowerFactor	= 10.0f;
const float g_FovForFullPixelDepthPowerFactor	= 50.0f;

const u32 uBufferStride = 5 * sizeof(float);

AdaptiveDOF::AdaptiveDOF() :
 AdaptiveDofEnabled(true)
#if ADAPTIVE_DOF_GPU
,AdaptiveDofProjVar(grcevNONE)
,AdaptiveDofDepthDownSampleParamsVar(grcevNONE)
,AdaptiveDofParams0Var(grcevNONE)
,AdaptiveDofParams1Var(grcevNONE)
,AdaptiveDofParams2Var(grcevNONE)
,AdaptiveDofParams3Var(grcevNONE)
,AdaptiveDofFocusDistanceDampingParams1Var(grcevNONE)
,AdaptiveDofFocusDistanceDampingParams2Var(grcevNONE)
,reductionComputeInputTex(grcevNONE)
,reductionComputeOutputTex(grcevNONE)
,AdaptiveDOFShader(NULL)
,depthRedution0(NULL)
,depthRedution1(NULL)
,depthRedution2(NULL)
,adaptiveDofParams(NULL)
,adaptiveDofParamsTexture(NULL)
#if __BANK
,depthReduction2Texture(NULL)
#endif //__BANK
#endif //ADAPTIVE_DOF_GPU
#if __BANK
,AdaptiveDofFocusDistanceGridScaling(1.0f, 1.0f)
,AdaptiveDofDrawFocusGrid(false)
,AdaptiveDofOverrideFocalDistance(false)
,AdaptiveDofOverriddenFocalDistanceVal(0.0f)
,ShouldMeasurePostAlphaPixelDepth(false)
#endif //__BANK
,AdaptiveDofHistoryIndex(0)
#if ADAPTIVE_DOF_CPU
,AdaptiveDofRayMissedValue(25000.0f)
,AdaptiveDofFocalGridSize(8)
,AdaptiveDofBlurDiskRadiusToApply(15.0f)
,AdaptiveDofHistoryTime(1.0)
#endif
{	
#if ADAPTIVE_DOF_CPU
	for( u32 i = 0; i < ADAPTIVE_DOF_HISTORY_SIZE; i++ )
	{
		AdaptiveDofParamHistory[i].time = 0;
		AdaptiveDofParamHistory[i].DOFParams = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
	}
#endif
}

AdaptiveDOF::~AdaptiveDOF()
{
	DeleteRenderTargets();

#if ADAPTIVE_DOF_GPU
	delete AdaptiveDOFShader;
	AdaptiveDOFShader = NULL;
#endif
}

void AdaptiveDOF::CreateRenderTargets()
{
#if ADAPTIVE_DOF_GPU
	grcTextureFactory::CreateParams params;

	params.Format = grctfR32F;
	params.Lockable = false;
	// a bit of wasting space here (16x16 and 4x4 buffer), but want everything tiled:

#if __WIN32PC
	params.StereoRTMode = grcDevice::AUTO;
#endif

	grcTextureFactory::TextureCreateParams TextureParams(grcTextureFactory::TextureCreateParams::SYSTEM, 
		grcTextureFactory::TextureCreateParams::LINEAR,	grcsRead|grcsWrite, NULL, 
		grcTextureFactory::TextureCreateParams::RENDERTARGET, 
		grcTextureFactory::TextureCreateParams::MSAA_NONE);

#if __BANK

	if( gAdaptiveDOFReadback )
	{
		depthReduction2Texture	= grcTextureFactory::GetInstance().Create( 1, 1, params.Format, NULL, 1, &TextureParams );
		depthRedution2ReadBack = grcTextureFactory::GetInstance().CreateRenderTarget( "depth reduc 2 readback", depthReduction2Texture->GetTexturePtr());
	}
#endif

	depthRedution0 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "depth reduc 0", grcrtPermanent, 1024, 1024, 32, &params, 3 WIN32PC_ONLY(, depthRedution0));
	params.UseAsUAV = true;
	depthRedution1 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "depth reduc 1", grcrtPermanent, 32, 32, 32, &params, 3 WIN32PC_ONLY(, depthRedution1));
	depthRedution2 = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, "depth reduc 2", grcrtPermanent, 1, 1, 32, &params, 3 WIN32PC_ONLY(, depthRedution2));

	
	
#if __BANK
	if( gAdaptiveDOFReadback )
	{
		//Use 32 bits per pixel to allow easier reading of values.
		params.Format = grctfA32B32G32R32F;
	}
	else
#endif //__BANK
	params.Format = grctfA16B16G16R16F;


	params.UseAsUAV = false;

	char szName[] = { "adaptiveDofParams" };
# if RSG_ORBIS
	// Orbis doesn't like rendertargets created from 1D textures
	adaptiveDofParamsTexture = NULL;
	adaptiveDofParams = CRenderTargets::CreateRenderTarget(RTMEMPOOL_NONE, szName, grcrtPermanent, ADAPTIVE_DOF_GPU_HISTORY_SIZE, 1, 32, &params, 3);
# else
	if (!adaptiveDofParamsTexture)
	{
		BANK_ONLY(grcTexture::SetCustomLoadName(szName);)
		adaptiveDofParamsTexture	= grcTextureFactory::GetInstance().Create( ADAPTIVE_DOF_GPU_HISTORY_SIZE, 1, params.Format, NULL, 1, &TextureParams );
		BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
	}
	if (!adaptiveDofParams)
	{
		adaptiveDofParams = grcTextureFactory::GetInstance().CreateRenderTarget( szName, adaptiveDofParamsTexture->GetTexturePtr());
	}
# endif // RSG_ORBIS

#endif // ADAPTIVE_DOF_GPU
}

void AdaptiveDOF::DeleteRenderTargets()
{
#if ADAPTIVE_DOF_GPU
	if(depthRedution0)
	{
		delete depthRedution0;
		depthRedution0 = NULL;
	}
	if(depthRedution1)
	{
		delete depthRedution1;
		depthRedution1 = NULL;
	}
	if(depthRedution2)
	{
		delete depthRedution2;
		depthRedution2 = NULL;
	}
	if(adaptiveDofParams)
	{
		delete adaptiveDofParams;
		adaptiveDofParams = NULL;
	}

#if ADAPTIVE_DOF_OUTPUT_UAV
	if( adaptiveDofParamsBuffer )
	{
		delete adaptiveDofParamsBuffer;
		adaptiveDofParamsBuffer = NULL;
	}
#endif
#endif
}

void AdaptiveDOF::Init()
{
	if( PARAM_noAdaptivedof.Get() )
	{
		AdaptiveDofEnabled = false;
	}
#if RSG_PC
	if (/*!GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50) ||*/ GRCDEVICE.IsStereoEnabled())
		AdaptiveDofEnabled = false;
#endif // RSG_PC
	LockFocusDistance = false;
#if __BANK
	gAdaptiveDOFReadback = PARAM_AdaptivedofReadback.Get();
#endif //__BANK

#if ADAPTIVE_DOF_GPU
	ASSET.PushFolder("common:/shaders");
	AdaptiveDOFShader = grmShaderFactory::GetInstance().Create();
	AdaptiveDOFShader->Load("adaptiveDof");
	AdaptiveDOFTechnique = AdaptiveDOFShader->LookupTechnique("AdaptiveDOF");	
	ASSET.PopFolder();

	AdaptiveDofProjVar = AdaptiveDOFShader->LookupVar("adapDOFProj");
	AdaptiveDofDepthDownSampleParamsVar = AdaptiveDOFShader->LookupVar("AdaptiveDofDepthDownSampleParams");
	AdaptiveDofParams0Var = AdaptiveDOFShader->LookupVar("AdaptiveDofParams0");
	AdaptiveDofParams1Var = AdaptiveDOFShader->LookupVar("AdaptiveDofParams1");
	AdaptiveDofParams2Var = AdaptiveDOFShader->LookupVar("AdaptiveDofParams2");
	AdaptiveDofParams3Var = AdaptiveDOFShader->LookupVar("AdaptiveDofParams3");
	AdaptiveDofFocusDistanceDampingParams1Var = AdaptiveDOFShader->LookupVar("AdaptiveDofFocusDistanceDampingParams1");
	AdaptiveDofFocusDistanceDampingParams2Var = AdaptiveDOFShader->LookupVar("AdaptiveDofFocusDistanceDampingParams2");

	reductionComputeInputTex = AdaptiveDOFShader->LookupVar("reductionDepthTexture");
	reductionComputeOutputTex = AdaptiveDOFShader->LookupVar("ReductionOutputTexture");

	AdaptiveDofDepth = AdaptiveDOFShader->LookupVar("adaptiveDOFTextureDepth");
	AdaptiveDofTexture = AdaptiveDOFShader->LookupVar("adaptiveDOFTexture");
	AdaptiveDofExposureTex = AdaptiveDOFShader->LookupVar("adaptiveDOFExposureTexture");

# if ADAPTIVE_DOF_OUTPUT_UAV && (RSG_DURANGO || RSG_ORBIS)
	AdaptiveDOFOutputBufferVar = AdaptiveDOFShader->LookupVar("AdaptiveDOFOutputBuffer");
# endif

#if POSTFX_UNIT_QUAD
	adaptiveDOFQuad::Position	= AdaptiveDOFShader->LookupVar("QuadPosition");
	adaptiveDOFQuad::TexCoords	= AdaptiveDOFShader->LookupVar("QuadTexCoords");
	adaptiveDOFQuad::Scale = AdaptiveDOFShader->LookupVar("QuadScale");
#endif
#endif
}

#if ADAPTIVE_DOF_CPU
void AdaptiveDOF::CalculateCPUParams(Vector4 &AdaptiveDofParams, float& blurDiskRadiusToApply)
{
	u32 numSamples = 0;
	u32 currentTime = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
	for( u32 i = 0; i < ADAPTIVE_DOF_HISTORY_SIZE; i++ )
	{
		if( currentTime - AdaptiveDofParamHistory[i].time < AdaptiveDofHistoryTime * 1000 )
		{
			AdaptiveDofParams += AdaptiveDofParamHistory[i].DOFParams;
			numSamples++;
		}
	}
	if (numSamples)
		AdaptiveDofParams /= (float)numSamples;

	blurDiskRadiusToApply = AdaptiveDofBlurDiskRadiusToApply;
};
#endif

#if ADAPTIVE_DOF_CPU
void AdaptiveDOF::ProcessAdaptiveDofCPU()
{
	bool bCutsceneRunning = (CutSceneManager::GetInstance() && CutSceneManager::GetInstance()->IsRunning());

	if(IsEnabled() && !bCutsceneRunning)
	{
		const camFrame& frame					= gVpMan.GetCurrentGameViewportFrame();
		const Vector2& focusDistanceGridScaling	= frame.GetDofFocusDistanceGridScaling();

#if __BANK
		AdaptiveDofFocusDistanceGridScaling.Set(focusDistanceGridScaling);
#endif // __BANK

		float distanceToSubject = 0.0f;
		static float prevDistanceToSubject = 0.0f;

		const bool shouldLockAdaptiveDofFocusDistance	= frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldLockAdaptiveDofFocusDistance);

		if (!shouldLockAdaptiveDofFocusDistance)
		{
			PF_PUSH_TIMEBAR("AdaptiveDofFocusDistance");
			u32 screenWidth = grcViewport::GetDefaultScreen()->GetWidth();
			u32 screenHeight = grcViewport::GetDefaultScreen()->GetHeight();
			float screenCenterX = (float)(screenWidth/2);
			float screenCenterY = (float)(screenHeight/2);

			static float gridScale = 1.0f;
			float gridWidth = (screenWidth / (AdaptiveDofFocalGridSize-1)) * focusDistanceGridScaling.x;
			float gridHeight = (screenHeight / (AdaptiveDofFocalGridSize-1)) * focusDistanceGridScaling.y;

			float screenX = screenCenterX - ((screenWidth / 2.0f) * focusDistanceGridScaling.x);
			float screenY = screenCenterY - ((screenHeight / 2.0f) * focusDistanceGridScaling.y);

			//Fire a gid of rays to find the current focal distance by averaging the distance from the camera to the hitpoint for all the rays.
			//TODO: This isnt ideal due to the cost of the raytests and also the distance at which the collision spawns in results in some rays missing geometry.
			//We should hopefully be able to move this over to the gpu and somehow use the depth buffer to calculate it which will be quicker and give better result.
			for(u32 x = 0; x < AdaptiveDofFocalGridSize; x++)
			{
				for(u32 y = 0; y < AdaptiveDofFocalGridSize; y++)
				{
					Vector3	probeStartPos, probEndPos;
					gVpMan.GetGameViewport()->GetGrcViewport().ReverseTransform( screenX, screenY, (Vec3V&)probeStartPos, (Vec3V&)probEndPos );

					int nIncludeFlags = ArchetypeFlags::GTA_ALL_TYPES_MOVER;
					WorldProbe::CShapeTestProbeDesc probeData;
					WorldProbe::CShapeTestFixedResults<1> ShapeTestResults; // We're only looking at the first result, so requesting more is wasteful
					probeData.SetResultsStructure(&ShapeTestResults);
					probeData.SetStartAndEnd(probeStartPos, probEndPos);
					//probeData.SetExcludeInstance(pHitPed->GetAnimatedInst());
					probeData.SetIncludeFlags(nIncludeFlags);
					probeData.SetStateIncludeFlags(phLevelBase::STATE_FLAGS_ALL);
					probeData.SetIsDirected(true);
					if(WorldProbe::GetShapeTestManager()->SubmitTest(probeData))
					{
						if( ShapeTestResults[0].GetHitDetected() )
						{
#if __BANK
							if( AdaptiveDofDrawFocusGrid )
								grcDebugDraw::Sphere(ShapeTestResults[0].GetHitPosition(), 0.1f, Color32(255,0,0));
#endif

							distanceToSubject += ShapeTestResults[0].GetHitPosition().Dist(probeStartPos);

						}
						else
						{
							distanceToSubject += AdaptiveDofRayMissedValue;
						}
					}
					else
					{
						distanceToSubject += AdaptiveDofRayMissedValue;
					}

					screenY += gridHeight;
				}
				screenX += gridWidth;
				screenY = screenCenterY - ((screenHeight / 2.0f) * focusDistanceGridScaling.y);
			}

			distanceToSubject /= (AdaptiveDofFocalGridSize*AdaptiveDofFocalGridSize);

			const float overriddenFocusDistanceBlendLevel	= frame.GetDofOverriddenFocusDistanceBlendLevel();
			const float overriddenFocusDistance				= frame.GetDofOverriddenFocusDistance();
			distanceToSubject								= Lerp(overriddenFocusDistanceBlendLevel, distanceToSubject, overriddenFocusDistance);

	#if __BANK
			//Allow the focus distance to be overridden from RAG.
			if(AdaptiveDofOverrideFocalDistance)
			{
				distanceToSubject = AdaptiveDofOverriddenFocalDistanceVal;
			}
	#endif // __BANK
			prevDistanceToSubject = distanceToSubject;
			PF_POP_TIMEBAR();
		}
		else
		{
			distanceToSubject = prevDistanceToSubject;
		}

		Vector4 adaptiveDofPlanes;
		frame.ComputeDofPlanesAndBlurRadiusUsingLensModel(distanceToSubject, adaptiveDofPlanes, AdaptiveDofBlurDiskRadiusToApply);

		//Store the dof planes in the history to allow us to blend between them over time.
		AdaptiveDofParamHistory[AdaptiveDofHistoryIndex].DOFParams = adaptiveDofPlanes;
		AdaptiveDofParamHistory[AdaptiveDofHistoryIndex].time = fwTimer::GetTimeInMilliseconds_NonScaledClipped();
		AdaptiveDofHistoryIndex++;

		if( AdaptiveDofHistoryIndex >= ADAPTIVE_DOF_HISTORY_SIZE )
			AdaptiveDofHistoryIndex = 0;

#if __BANK
		sprintf(AdaptiveDofDesiredFocusDistanceString, "%f", distanceToSubject);
		sprintf(AdaptiveDofDampedFocusDistanceString, "%f", distanceToSubject);
#endif
	}
}
#endif //ADAPTIVE_DOF_CPU

#if ADAPTIVE_DOF_GPU

#if ADAPTIVE_DOF_OUTPUT_UAV
static bool sCreatedUAVs = false;
void AdaptiveDOF::InitiliaseOnRenderThread()
{
	// UAVS need to be created on the render thread.
	if( AdaptiveDofEnabled && !sCreatedUAVs )
	{			
#if RSG_ORBIS
		const u32 uBufferSize = ADAPTIVE_DOF_GPU_HISTORY_SIZE * uBufferStride;
		adaptiveDofParamsBuffer = rage_new grcBufferUAV( grcBuffer_Structured, true );
		const sce::Gnm::SizeAlign accumSizeAlign = { uBufferSize, 16 };
		adaptiveDofParamsBuffer->Allocate( accumSizeAlign, true, NULL, uBufferStride );
#else // RSG_ORBIS
		adaptiveDofParamsBuffer = rage_new grcBufferUAV();
#if RSG_DURANGO
		adaptiveDofParamsBuffer->Initialise( ADAPTIVE_DOF_GPU_HISTORY_SIZE, uBufferStride, grcBindShaderResource|grcBindUnorderedAccess, NULL, grcBufferMisc_BufferStructured);
#else
		if (GRCDEVICE.SupportsFeature(COMPUTE_SHADER_50))
		{
			ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(true);))
			adaptiveDofParamsBuffer->Initialise( ADAPTIVE_DOF_GPU_HISTORY_SIZE, uBufferStride, grcBindShaderResource|grcBindUnorderedAccess, grcsBufferCreate_ReadWrite, grcsBufferSync_None, NULL, false, grcBufferMisc_BufferStructured );
			ASSERT_ONLY(RESOURCE_CACHE_ONLY(grcResourceCache::GetInstance().SetSafeResourceCreate(false);))
		}
#endif // RSG_DURANGO
#endif // RSG_ORBIS

		sCreatedUAVs = true;
	}
}
#endif //ADAPTIVE_DOF_OUTPUT_UAV

int DepthCompare(const float* a, const float* b)
{
	return (int)(*a - *b);
}

void AdaptiveDOF::ProcessAdaptiveDofGPU(const PostFX::PostFXParamBlock::paramBlock& settings, grcRenderTarget*& prevExposureRT, bool wasAdaptiveDofProcessedOnPreviousUpdate)
{
	if(IsEnabled())
	{
		PIXBegin(0, "Adaptive DOF GPU");

#if __BANK

		if( gAdaptiveDOFReadback )
		{
#if ADAPTIVE_DOF_OUTPUT_UAV

#if RSG_DURANGO || RSG_ORBIS
#if RSG_DURANGO
			float* pBuff = (float*) adaptiveDofParamsBuffer->LockAll();
#elif RSG_ORBIS
			float* pBuff = (float*) adaptiveDofParamsBuffer->GetAddress();
#endif
			const float finalFocusDistance		= *pBuff;
			pBuff++;
 			const float maxBlurDiskRadiusNear	= *pBuff;
			pBuff++;
 			const float maxBlurDiskRadiusFar	= *pBuff;
			pBuff++;
			const float springResult			= *pBuff;

			sprintf(AdaptiveDofFinalFocusDistanceString, "%f", finalFocusDistance);
			sprintf(AdaptiveDofMaxBlurDiskRadiusNearString, "%f", maxBlurDiskRadiusNear);
			sprintf(AdaptiveDofMaxBlurDiskRadiusFarString, "%f", maxBlurDiskRadiusFar);

			float localDampedFocusDistance = Powf(10.0f, springResult);
			sprintf(AdaptiveDofDampedFocusDistanceString, "%f", localDampedFocusDistance);
			PF_SET(dampedFocusDistance, localDampedFocusDistance);

#if RSG_DURANGO
			adaptiveDofParamsBuffer->UnlockAll();
#endif //RSG_DURANGO

#endif //RSG_DURANGO || RSG_ORBIS

#endif //ADAPTIVE_DOF_OUTPUT_UAV

			float localDesiredFocusDistance = 0.0f;

#if RSG_ORBIS
			grcTextureLock lock;
			if(depthRedution2 && depthRedution2->LockRect( 0, 0, lock, grcsRead ))
			{
				localDesiredFocusDistance = *((float*)lock.Base);
				depthRedution2->UnlockRect(lock);
			}
#else // RSG_ORBIS
			if(depthReduction2Texture)
			{
#if RSG_PC	
				grcTextureD3D11* depthReduction2TextureTarget = (grcTextureD3D11*)depthReduction2Texture;
				if( depthReduction2TextureTarget->MapStagingBufferToBackingStore(true) )
#else //RSG_PC
				grcTexture* depthReduction2TextureTarget = (grcTexture*)depthReduction2Texture;
#endif //RSG_PC
				{
					grcTextureLock lock;
					if(depthReduction2TextureTarget->LockRect( 0, 0, lock, grcsRead ))
					{
						localDesiredFocusDistance = *((float*)lock.Base);
						depthReduction2TextureTarget->UnlockRect(lock);
					}
				}
			}
#endif //RSG_ORBIS

			sprintf(AdaptiveDofDesiredFocusDistanceString, "%f", localDesiredFocusDistance);
			PF_SET(desiredFocusDistance, localDesiredFocusDistance);
		}
#endif //__BANK

		const Vector4 projParams = ShaderUtil::CalculateProjectionParams();
		AdaptiveDOFShader->SetVar(AdaptiveDofProjVar, projParams);

		//Find the average depth of the scene to use as the focal depth
		PIXBegin(0, "Depth Reduction");

		//Copy the depth buffer to a 1024 target to allow optimal parallel reduction.
		//Blit with different UVs to scale to correct ratio of the focusGridSize
		grcTextureFactory::GetInstance().LockRenderTarget(0, depthRedution0, NULL);

		const camFrame& frame					= gVpMan.GetCurrentGameViewportFrame();
		const Vector2& focusDistanceGridScaling	= frame.GetDofFocusDistanceGridScaling();

#if __BANK
		AdaptiveDofFocusDistanceGridScaling.Set(focusDistanceGridScaling);
#endif // __BANK

		float u1 = 0.5f - (0.5f*focusDistanceGridScaling.x);
		float u2 = 0.5f + (0.5f*focusDistanceGridScaling.x);
		float v1 = 0.5f - (0.5f*focusDistanceGridScaling.y);
		float v2 = 0.5f + (0.5f*focusDistanceGridScaling.y);
#if POSTFX_UNIT_QUAD
		AdaptiveDOFShader->SetVar(adaptiveDOFQuad::Position,	FastQuad::Pack(-1.f,1.f,1.f,-1.f));
		AdaptiveDOFShader->SetVar(adaptiveDOFQuad::TexCoords,	FastQuad::Pack(u1, v1, u2, v2));
		AdaptiveDOFShader->SetVar(adaptiveDOFQuad::Scale,		Vector4(1.0f,1.0f,1.0f,1.0f));
#endif

		const bool shouldMeasurePostAlphaPixelDepthForCamera = frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldMeasurePostAlphaPixelDepth);

		grcRenderTarget* pDepth = CRenderTargets::GetDepthBuffer_PreAlpha();
		if( BANK_ONLY(ShouldMeasurePostAlphaPixelDepth || )shouldMeasurePostAlphaPixelDepthForCamera )
		{
			pDepth = CRenderTargets::GetDepthResolved();
		}

		AdaptiveDOFShader->SetVar(AdaptiveDofDepth, pDepth);

		//NOTE: The maximum pixel depth limit can interact undesirably with other DOF parameters, particularly the focus distance grid scaling. So we blend this limit and
		//apply a non-linear function to encourage a more aggressive start to any blend in. This is pretty hacky, but it reduces the scope for overshoot in the focus distance
		//when transitioning out of aim cameras, as an example.
		const float maxPixelDepth					= frame.GetDofMaxPixelDepth();
		float maxPixelDepthBlendLevel				= frame.GetDofMaxPixelDepthBlendLevel();
		maxPixelDepthBlendLevel						= SlowOut(SlowOut(maxPixelDepthBlendLevel));

		//NOTE: We blend from the desired pixel depth power factor towards unity (linear depth) as the field of view decreases (and the zoom factor increases.) This is pretty unscientific,
		//but it provides a means to artificially pull in the measured focus depth in wider shots whilst maintaining reasonable focus on the subject(s) of a close-up.
		const float desiredPixelDepthPowerFactor	= frame.GetDofPixelDepthPowerFactor();
		const float fov								= frame.GetFov();
		const float pixelDepthPowerFactorToApply	= RampValueSafe(fov, g_FovForUnityPixelDepthPowerFactor, g_FovForFullPixelDepthPowerFactor, 1.0f, desiredPixelDepthPowerFactor);

		Vector4 AdaptiveDofDepthDownSampleParamsConstant(maxPixelDepth, pixelDepthPowerFactorToApply, maxPixelDepthBlendLevel, 0.0f);
		AdaptiveDOFShader->SetVar(AdaptiveDofDepthDownSampleParamsVar, AdaptiveDofDepthDownSampleParamsConstant);

		AssertVerify(AdaptiveDOFShader->BeginDraw(grmShader::RMC_DRAW,true,AdaptiveDOFTechnique)); // MIGRATE_FIXME
		AdaptiveDOFShader->Bind((int)pp_depthDownsample);
		Color32 color(1.0f,1.0f,1.0f,1.0f);
#if POSTFX_UNIT_QUAD
		FastQuad::Draw(true);
#else
		grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, u1, v1, u2, v2,color);
#endif
		AdaptiveDOFShader->UnBind();
		AdaptiveDOFShader->EndDraw();

		grcResolveFlags resolveFlags;
		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

		//Compute shaders for parallel reduction
		AdaptiveDOFShader->SetVar(reductionComputeInputTex, depthRedution0);

		AdaptiveDOFShader->SetVarUAV( reductionComputeOutputTex, static_cast<grcTextureUAV*>( depthRedution1 ) );

		u32 TGSize = 16;
		u32 inputSize = 1024;
		u32 outputSize = MAX(inputSize / (TGSize * 2), 1u);

		GRCDEVICE.RunComputation( "Depth reduction 1", *AdaptiveDOFShader, pp_depthParallelReductionCS, outputSize, outputSize, 1);

		AdaptiveDOFShader->SetVar(reductionComputeInputTex, depthRedution1);
		AdaptiveDOFShader->SetVarUAV( reductionComputeOutputTex, static_cast<grcTextureUAV*>( depthRedution2 ) );

		inputSize = 32;
		outputSize = MAX(inputSize / (TGSize * 2), 1u);
		GRCDEVICE.RunComputation( "Depth reduction 2", *AdaptiveDOFShader, pp_depthParallelReductionCS, outputSize, outputSize, 1);

# if __D3D11 || RSG_ORBIS
		GRCDEVICE.SynchronizeComputeToGraphics();
# endif

		PIXEnd();

		const float fNumberOfLens						= frame.GetFNumberOfLens();
		const float focalLengthMultiplier				= frame.GetFocalLengthMultiplier();
		const Vector2& subjectMagPowerNearFar			= frame.GetDofSubjectMagPowerNearFar();
		const float fStopsAtMaxExposure					= frame.GetDofFStopsAtMaxExposure();
		const float focusDistanceBias					= frame.GetDofFocusDistanceBias();
		const float maxNearInFocusDistance				= frame.GetDofMaxNearInFocusDistance();
		const float maxNearInFocusDistanceBlendLevel	= frame.GetDofMaxNearInFocusDistanceBlendLevel();
		const float maxBlurRadiusAtNearInFocusLimit		= frame.GetDofMaxBlurRadiusAtNearInFocusLimit();
		float overriddenFocusDistance					= frame.GetDofOverriddenFocusDistance();
		float overriddenFocusDistanceBlendLevel			= frame.GetDofOverriddenFocusDistanceBlendLevel();
		const float focusDistanceIncreaseSpringConstant	= frame.GetDofFocusDistanceIncreaseSpringConstant();
		const float focusDistanceDecreaseSpringConstant = frame.GetDofFocusDistanceDecreaseSpringConstant();
		const bool shouldLockAdaptiveDofFocusDistance	= frame.GetFlags().IsFlagSet(camFrame::Flag_ShouldLockAdaptiveDofFocusDistance);

		const float focalLengthOfLens = focalLengthMultiplier * g_HeightOf35mmFullFrame / (2.0f * Tanf(0.5f * fov * DtoR));

		TUNE_BOOL(adaptiveDofShouldComputeDepthOnCpu, false)
		TUNE_BOOL(adaptiveDofShouldUseMedianDepth, false)

		if(adaptiveDofShouldComputeDepthOnCpu)
		{
			const grcViewport* renderGameGrcViewport = gVpMan.GetRenderGameGrcViewport();
			if(renderGameGrcViewport)
			{
				grcRenderTarget* depth = GBuffer::GetDepthTarget();
				if(depth)
				{
					const int fullHeight	= depth->GetHeight();
					const int fullWidth		= depth->GetWidth();

					const int startWidth		= static_cast<int>((0.5f - (0.5f * focusDistanceGridScaling.x)) * static_cast<float>(fullWidth));
					const int widthToConsider	= Max(static_cast<int>(focusDistanceGridScaling.x * static_cast<float>(fullWidth)), 1);
					const int endWidth			= startWidth + widthToConsider;

					const int startHeight		= static_cast<int>((0.5f - (0.5f * focusDistanceGridScaling.y)) * static_cast<float>(fullHeight));
					const int heightToConsider	= Max(static_cast<int>(focusDistanceGridScaling.y * static_cast<float>(fullHeight)), 1);
					const int endHeight			= startHeight + heightToConsider;

					const int numSamplesToConsider = widthToConsider * heightToConsider;

					grcTextureLock depthLock;
					if(depth->LockRect(0, 0, depthLock, grcsRead | grcsAllowVRAMLock))
					{
						//NOTE: These values appear to have a stride of two on x64 PC.
						const float* depthValues = (const float*)depthLock.Base;
						if(depthValues)
						{
							float measuredFocusDistance = 0.0f;

							atArray<float, 0, unsigned int> tempDepths;

							if(adaptiveDofShouldUseMedianDepth)
							{
								tempDepths.Reserve(numSamplesToConsider);
							}

							const float invNumSampleToConsider = 1.0f / (float)numSamplesToConsider;

							for(int y=startHeight; y<endHeight; y++)
							{
								for(int x=startWidth; x<endWidth; x++)
								{
									int sampleIndex	= (y * fullWidth) + x;
#if RSG_PC
									sampleIndex *= 2; //There's a stride of two in the depth samples on x64.
#endif // RSG_PC
									const float sampleDepth	= depthValues[sampleIndex];
									float depth				= ShaderUtil::GetLinearDepth(renderGameGrcViewport, ScalarV(sampleDepth)).Getf();
									depth					= Min(depth, maxPixelDepth);
									depth					= Powf(depth, pixelDepthPowerFactorToApply);

									if(adaptiveDofShouldUseMedianDepth)
									{
										tempDepths.Push(depth);
									}
									else
									{
										measuredFocusDistance	+= (depth * invNumSampleToConsider);
									}
								}
							}

							if(adaptiveDofShouldUseMedianDepth)
							{
								tempDepths.QSort(0, -1, DepthCompare);

								measuredFocusDistance = tempDepths[numSamplesToConsider / 2];
							}

							cameraDisplayf("measuredFocusDistance = %f", measuredFocusDistance);

							overriddenFocusDistance				= Lerp(overriddenFocusDistanceBlendLevel, measuredFocusDistance, overriddenFocusDistance);
							overriddenFocusDistanceBlendLevel	= 1.0f;
						}

						depth->UnlockRect(depthLock);
					}
				}
			}
		}

#if __BANK
		//Allow the focus distance to be overridden from RAG.
		if(AdaptiveDofOverrideFocalDistance)
		{
			overriddenFocusDistance				= AdaptiveDofOverriddenFocalDistanceVal;
			overriddenFocusDistanceBlendLevel	= 1.0f;
		}
#endif // __BANK

		static Vector4 paramsCache[4];
		static Vector4 dampingParamsCache[2];

		bool useCached = LockFocusDistance;

		const bool shouldCutAdaptiveDofDistanceDamping	= !wasAdaptiveDofProcessedOnPreviousUpdate || frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutPosition) ||
			frame.GetFlags().IsFlagSet(camFrame::Flag_HasCutOrientation);

		const float increaseSpringConstant = shouldCutAdaptiveDofDistanceDamping ? 0.0f : focusDistanceIncreaseSpringConstant;
		const float decreaseSpringConstant = shouldCutAdaptiveDofDistanceDamping ? 0.0f : focusDistanceDecreaseSpringConstant;

		TUNE_FLOAT(adaptiveDofFocusDistanceIncreaseDampingRatio, 0.666f, 0.0f, 10.0f, 0.01f)
		TUNE_FLOAT(adaptiveDofFocusDistanceDecreaseDampingRatio, 1.0f, 0.0f, 10.0f, 0.01f)

		const float timeStepToApply = shouldLockAdaptiveDofFocusDistance ? 0.0f : settings.m_AdaptiveDofTimeStep;

		Vector4 dofParams[4] = {
			Vector4(fNumberOfLens, focalLengthOfLens, overriddenFocusDistance, overriddenFocusDistanceBlendLevel),
			Vector4(focusDistanceBias, subjectMagPowerNearFar.x, subjectMagPowerNearFar.y, 0.0f),
			Vector4(maxNearInFocusDistance, 0.0f, maxBlurRadiusAtNearInFocusLimit, maxNearInFocusDistanceBlendLevel),
			Vector4(AdaptiveDofExposureLimits.x, AdaptiveDofExposureLimits.y, fStopsAtMaxExposure, 0.0f)
		};

		Vector4 dofDampingParams[2] = {
			Vector4(increaseSpringConstant, adaptiveDofFocusDistanceIncreaseDampingRatio, decreaseSpringConstant, adaptiveDofFocusDistanceDecreaseDampingRatio),
			Vector4(timeStepToApply, 0.0f, 0.0f, 0.0f)
		};

		if (useCached) {
			sysMemCpy(dofParams, paramsCache, sizeof(paramsCache));
			sysMemCpy(dofDampingParams, dampingParamsCache, sizeof(dampingParamsCache));
			dofDampingParams[1] = Vector4(0.0f, 0.0f, 0.0f, 0.0f); //zeroed time step
		}

		AdaptiveDOFShader->SetVar(AdaptiveDofParams0Var, dofParams[0]);
		AdaptiveDOFShader->SetVar(AdaptiveDofParams1Var, dofParams[1]);
		AdaptiveDOFShader->SetVar(AdaptiveDofParams2Var, dofParams[2]);
		AdaptiveDOFShader->SetVar(AdaptiveDofParams3Var, dofParams[3]);
		AdaptiveDOFShader->SetVar(AdaptiveDofFocusDistanceDampingParams1Var, dofDampingParams[0]);
		AdaptiveDOFShader->SetVar(AdaptiveDofFocusDistanceDampingParams2Var, dofDampingParams[1]);

		if(!useCached) {
			sysMemCpy(paramsCache, dofParams, sizeof(paramsCache));
			sysMemCpy(dampingParamsCache, dofDampingParams, sizeof(dampingParamsCache));
		}

		AdaptiveDOFShader->SetVar(AdaptiveDofTexture, depthRedution2);

		AdaptiveDOFShader->SetVar(AdaptiveDofExposureTex, prevExposureRT);
#if ADAPTIVE_DOF_OUTPUT_UAV
#if __D3D11
		grcTextureFactoryDX11& textureFactory11 = static_cast<grcTextureFactoryDX11&>(grcTextureFactoryDX11::GetInstance()); 
		const grcRenderTarget* RenderTargets[grcmrtColorCount] = {
			adaptiveDofParams
		};
		const grcBufferUAV* UAVs[grcMaxUAVViews] = {
			NULL, adaptiveDofParamsBuffer
		};

		UINT initialCounts[grcMaxUAVViews] = { 0 };
		textureFactory11.LockMRTWithUAV(RenderTargets, NULL, NULL, NULL, UAVs, initialCounts);
#else
		grcTextureFactory::GetInstance().LockRenderTarget(0, adaptiveDofParams, NULL);
#endif
#if RSG_DURANGO || RSG_ORBIS
		AdaptiveDOFShader->SetVarUAV(AdaptiveDOFOutputBufferVar, adaptiveDofParamsBuffer, 0);
#endif
#else
		grcTextureFactory::GetInstance().LockRenderTarget(0, adaptiveDofParams, NULL);
#endif //ADAPTIVE_DOF_OUTPUT_UAV

#if POSTFX_UNIT_QUAD
		AdaptiveDOFShader->SetVar(adaptiveDOFQuad::Position,	FastQuad::Pack(-1.f,1.f,1.f,-1.f));
		AdaptiveDOFShader->SetVar(adaptiveDOFQuad::TexCoords,	FastQuad::Pack(0.0f, 0.0f, 1.0f, 1.0f));
		AdaptiveDOFShader->SetVar(adaptiveDOFQuad::Scale,		Vector4(1.0f,1.0f,1.0f,1.0f));
#endif

		grcViewport::GetCurrent()->ResetWindow();
		grcViewport::GetCurrent()->SetWindow(AdaptiveDofHistoryIndex,0,1,1);

#if ADAPTIVE_DOF_OUTPUT_UAV
		grcStateBlock::SetBlendState(grcStateBlock::BS_Default_WriteMaskNone);
#endif

		AssertVerify(AdaptiveDOFShader->BeginDraw(grmShader::RMC_DRAW,true,AdaptiveDOFTechnique)); // MIGRATE_FIXME
		AdaptiveDOFShader->Bind((int)pp_adaptiveDOFcalc);
#if POSTFX_UNIT_QUAD
		FastQuad::Draw(true);
#else
		grcDrawSingleQuadf(-1.0, 1.0f, 1.0f, -1.0f, 0.0f, u1, v1, u2, v2,color);
#endif
		AdaptiveDOFShader->UnBind();
		AdaptiveDOFShader->EndDraw();

		grcTextureFactory::GetInstance().UnlockRenderTarget(0, &resolveFlags);

		grcViewport::GetCurrent()->ResetWindow();
		grcViewport::GetCurrent()->SetWindow(0,0,1,1);

		AdaptiveDofHistoryIndex++;
		if( AdaptiveDofHistoryIndex >= ADAPTIVE_DOF_GPU_HISTORY_SIZE)
			AdaptiveDofHistoryIndex = 0;

		PIXEnd();

#if __BANK && __D3D11
		if( gAdaptiveDOFReadback )
		{
			((grcTextureD3D11*)adaptiveDofParamsTexture)->CopyFromGPUToStagingBuffer();

			depthRedution2ReadBack->Copy(depthRedution2);
			((grcTextureD3D11*)depthReduction2Texture)->CopyFromGPUToStagingBuffer();
		}
#endif
	}
}

#if __BANK
void AdaptiveDOF::DrawFocusGrid()
{
	if( AdaptiveDofDrawFocusGrid )
	{
		float x1 = - AdaptiveDofFocusDistanceGridScaling.x;
		float x2 = AdaptiveDofFocusDistanceGridScaling.x;
		float y1 = - AdaptiveDofFocusDistanceGridScaling.y;
		float y2 = AdaptiveDofFocusDistanceGridScaling.y;

		grcStateBlock::SetBlendState(grcStateBlock::BS_Normal);

		AssertVerify(AdaptiveDOFShader->BeginDraw(grmShader::RMC_DRAW,true,AdaptiveDOFTechnique)); // MIGRATE_FIXME
		AdaptiveDOFShader->Bind((int)pp_DEBUG_drawFocalGrid);

		Color32 color(1.0f,1.0f,1.0f,1.0f);
		grcDrawSingleQuadf(x1, y1, x2, y2, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,color);

		AdaptiveDOFShader->UnBind();
		AdaptiveDOFShader->EndDraw();

	}
}
#endif //__BANK


#endif //ADAPTIVE_DOF_GPU

void AdaptiveDOF::UpdateVisualDataSettings()
{
#if ADAPTIVE_DOF
	AdaptiveDofExposureLimits.x =																	g_visualSettings.Get("adaptivedof.exposureMin",				-2.5f);
	AdaptiveDofExposureLimits.y =																	g_visualSettings.Get("adaptivedof.exposureMax",				2.5f);

	//Other settings
#if ADAPTIVE_DOF_CPU
	AdaptiveDofRayMissedValue =																		g_visualSettings.Get("adaptivedof.missedrayvalue",			6000);
	AdaptiveDofFocalGridSize =																		(u32) g_visualSettings.Get("adaptivedof.gridsize",			8);
	AdaptiveDofHistoryTime =																		g_visualSettings.Get("adaptivedof.smoothtime",				1.0f);
#endif
#endif //ADAPTIVE_DOF
}

#if __BANK

void AdaptiveDOF::AddWidgets(bkBank &bk, PostFX::PostFXParamBlock::paramBlock &/*edittedSettings*/)
{
	// In Game DOF
#if ADAPTIVE_DOF
	bk.PushGroup("Adaptive DOF");
	bk.AddToggle("Enabled", &AdaptiveDofEnabled );
	bk.AddTitle("use -AdaptivedofReadback to get debug values (Durango and PS4 for best results)");
	bk.AddText("Desired focus distance", AdaptiveDofDesiredFocusDistanceString, 64);
	bk.AddText("Damped focus distance", AdaptiveDofDampedFocusDistanceString, 64);
	bk.AddText("Final focus distance", AdaptiveDofFinalFocusDistanceString, 64);
	bk.AddText("Max blur radius (near)", AdaptiveDofMaxBlurDiskRadiusNearString, 64);
	bk.AddText("Max blur radius (far)", AdaptiveDofMaxBlurDiskRadiusFarString, 64);
	bk.AddSeparator();

#if ADAPTIVE_DOF_CPU
	bk.AddSlider("Focus grid size", &AdaptiveDofFocalGridSize, 2, 15, 1);	
#endif	
	bk.AddToggle("Lock focus distance", &LockFocusDistance);
	bk.AddToggle("Draw focus grid", &AdaptiveDofDrawFocusGrid);

	bk.AddSeparator();
#if ADAPTIVE_DOF_CPU
	bk.AddSlider("Missed Ray test value", &AdaptiveDofRayMissedValue, 0.0f, 25000.0f, 1.0f);
	bk.AddSlider("Num frames averaged", &AdaptiveDofHistoryTime, 0.25f, 1.0f, 0.01f);
#endif

	bk.AddSeparator();
	bk.AddToggle("Override Focal Distance", &AdaptiveDofOverrideFocalDistance);
	bk.AddSlider("Overridden Focal Distance", &AdaptiveDofOverriddenFocalDistanceVal, 0.0f, 25000.0f, 1.0f);
	
	bk.AddSeparator();
	bk.AddToggle("Force measurement of post-alpha pixel depth", &ShouldMeasurePostAlphaPixelDepth);

	bk.PopGroup();
#endif //ADAPTIVE_DOF
}

#endif //#if __BANK

} //namespace postfx


#endif //ADAPTIVE_DOF
