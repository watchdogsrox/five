#include "renderer/computeshader.h"
#if __D3D11
#include "system/system.h"
#include "camera/viewports/Viewport.h"
#include "profile/timebars.h"
#include "file/asset.h"
#include "grprofile/pix.h"
#include "grmodel/shader.h"
#include "grcore/texturedefault.h"
#include "grcore/rendertarget_d3d11.h"
#include "grcore/rendertarget_durango.h"
//#include "timecycle/timecyclecolourset.h"
//#include "timecycle/TimeCycle.h"

RENDER_OPTIMISATIONS();
	
CSGameManager gCSGameManager;

#if __D3D11
extern ID3D11SamplerState **g_SamplerStates11;
#endif

//
//WaterCS::WaterCS(rage::grmShader* pShader) : rage::BaseCS(pShader)
//{
//
//}
//WaterCS::~WaterCS()
//{
//
//}
//void WaterCS::Init() 
//{
//
//}
//void WaterCS::Shutdown()
//{
//	;
//}
//void WaterCS::SetupResources(int pass) const
//{
//	(void) pass;
//}
//void WaterCS::CleanUpResources() const
//{
//	BaseCS::CleanUpResources();
//}
//void WaterCS::Run(u16 xthread, u16 ythread, u16 zthread) const
//{
//	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	
//	BaseCS::Run(xthread, ythread, zthread, 0);
//}
///********************************************************************************/
//HDAOCS::HDAOParams HDAOCS::sm_Params;
//HDAOCS::HDAOCS(rage::grmShader* pShader) : rage::BaseCS(pShader)
//	, m_RejectRadiusVarId(grcevNONE)
//	, m_AcceptRadiusVarId(grcevNONE)
//	, m_RecipFadeOutDistVarId(grcevNONE)
//	, m_IntensityVarId(grcevNONE)
//	, m_NormalScaleVarId(grcevNONE)
//	, m_AcceptAngleVarId(grcevNONE)
//	, m_RTSizeVarId(grcevNONE)
//	, m_fQVarId(grcevNONE)
//	, m_fQTimesZNearVarId(grcevNONE) 
//	, m_fTanHalfHVVarId(grcevNONE)
//	, m_AmbientLightParamsVarId(grcevNONE)
//	, m_DepthTexVarId(grcevNONE)
//	, m_NormalTexVarId(grcevNONE)
//	, m_LightShaftTexVarId(grcevNONE)
//	, m_BaseTexVarId(grcevNONE)
//	, m_StencilBufferTexVarId(grcevNONE)
//
//{
//}
//HDAOCS::~HDAOCS()
//{
//	Shutdown();
//}
//void HDAOCS::Init()
//{
//	m_RejectRadiusVarId = m_Shader->LookupVar("g_fHDAORejectRadius");
//	m_AcceptRadiusVarId = m_Shader->LookupVar("g_fHDAOAcceptRadius");
//	m_RecipFadeOutDistVarId = m_Shader->LookupVar("g_fHDAORecipFadeOutDist");
//	m_IntensityVarId = m_Shader->LookupVar("g_fHDAOIntensity");
//	m_NormalScaleVarId = m_Shader->LookupVar("g_fHDAONormalScale");
//	m_AcceptAngleVarId = m_Shader->LookupVar("g_fAcceptAngle");
//	
//	m_RTSizeVarId = m_Shader->LookupVar("g_f4RTSize");
//	m_fQVarId = m_Shader->LookupVar("g_fQ");
//	m_fQTimesZNearVarId = m_Shader->LookupVar("g_fQTimesZNear");
//	m_fTanHalfHVVarId = m_Shader->LookupVar("g_f4TanHalfHV");
//
//	m_AmbientLightParamsVarId = m_Shader->LookupVar("AmbientLightingParams");
//
//	m_DepthTexVarId = m_Shader->LookupVar("GBufferTextureSamplerDepth");
//	m_NormalTexVarId = m_Shader->LookupVar("GBufferTextureSampler1");
//	m_LightShaftTexVarId = m_Shader->LookupVar("g_LightShaftSampler");
//#if USE_STENCIL_TARGET
//	m_StencilBufferTexVarId = m_Shader->LookupVar("GBufferStencilTextureSampler");
//#endif
//	m_BaseTexVarId = m_Shader->LookupVar("BaseSampler");
//	
//	/*
//		// Samplers
//		SamplerState g_SamplePoint  : register( s0 );
//	*/
//}
//void HDAOCS::Shutdown()
//{
//
//}
//
//#if __BANK
//void HDAOCS::AddBankWidgets(bkBank& bank)
//{
//	bank.PushGroup("HDAO", false);
//	bank.AddToggle("Use Compute Shader Blur", &sm_Params.bUseCSBlur);
//	bank.AddSlider("Reject Radius", &sm_Params.fHDAORejectRadius, 0.0f, 1.0f, 0.01f);
//	bank.AddSlider("Accept Radius", &sm_Params.fHDAOAcceptRadius, 0.0f, 1.0f, 0.0001f);
//	bank.AddSlider("Fade out distance", &sm_Params.fHDAORecipFadeOutDist, 0.0f, 10.0f, 0.01f);
//	bank.AddSlider("Intensity", &sm_Params.fHDAOIntensity, 0.0f, 2.0f, 0.05f);
//	bank.AddSlider("Normal Scale", &sm_Params.fHDAONormalScale, 0.0f, 1.0f, 0.01f);
//	bank.AddSlider("Accept Angle", &sm_Params.fAcceptAngle, 0.0f, 1.0f, 0.01f);
//	bank.PopGroup();
//	//fHDAORejectRadius    = 0.43f;		// Camera Z values must fall within the reject and accept radius to be 
//	//fHDAOAcceptRadius    = 0.00312f;	// considered as a valley
//	//fHDAORecipFadeOutDist	= 1.0f / 0.6f;
//	//fHDAOIntensity       = 0.85f;		// Simple scaling factor to control the intensity of the occlusion
//	//fHDAONormalScale     = 0.2f;		// Scaling factor to control the effect the normals have 
//	//fAcceptAngle         = 0.98f;		// Used by the ValleyAngle function to determine shallow valleys
//}
//#endif
//void HDAOCS::SetupResources(int pass) const
//{
//	CColourSet& currColourSet = g_timeCycle.GetCurrRenderColourSet();
//
//	m_Shader->SetVar(m_RejectRadiusVarId, sm_Params.fHDAORejectRadius);
//	m_Shader->SetVar(m_AcceptRadiusVarId, sm_Params.fHDAOAcceptRadius);
//	m_Shader->SetVar(m_RecipFadeOutDistVarId, sm_Params.fHDAORecipFadeOutDist);
//	m_Shader->SetVar(m_IntensityVarId, sm_Params.fHDAOIntensity);
//	m_Shader->SetVar(m_NormalScaleVarId, sm_Params.fHDAONormalScale);
//
//	m_Shader->SetVar(m_AcceptAngleVarId, sm_Params.fAcceptAngle);
//	Vector3 ambParams(currColourSet.GetVar(CSVAR_LIGHT_RAY_MULT), currColourSet.GetVar(CSVAR_LIGHT_AMB_MINIMUM), currColourSet.GetVar(CSVAR_LIGHT_AMB_PED_WRAP));
//	m_Shader->SetVar(m_AmbientLightParamsVarId, ambParams);
//	
//
//	float nearClip = grcViewport::GetCurrent()->GetNearClip();
//	float farClip = grcViewport::GetCurrent()->GetFarClip();
//	sm_Params.fQ = farClip / (farClip - nearClip);
//	sm_Params.fQTimesZNear = sm_Params.fQ * nearClip;
//	grcRenderTarget* pBackbuffer = grcTextureFactory::GetInstance().GetBackBuffer(true);
//	grcRenderTarget* pSSAOBuffer = CDeferredLightingHelper::GetGBufferSSAO_1();
//	sm_Params.f4RTSize.Set(static_cast<float>(pSSAOBuffer->GetWidth()), static_cast<float>(pSSAOBuffer->GetHeight()), 
//						   1.0f/static_cast<float>(pBackbuffer->GetWidth()), 1.0f/static_cast<float>(pBackbuffer->GetHeight()));
//	/*
//		sm_Params.f4RTSize.Set(static_cast<float>(pBackbuffer->GetWidth()), static_cast<float>(pBackbuffer->GetHeight()), 
//								static_cast<float>(pSSAOBuffer->GetWidth()), static_cast<float>(pSSAOBuffer->GetHeight()));
//
//	*/
//	
//	float fAspectRatio = static_cast<float>(pSSAOBuffer->GetWidth()) / static_cast<float>(pSSAOBuffer->GetHeight());
//	sm_Params.fTanHalfHV.Set(tanf( PI / 8 * fAspectRatio ), tanf( PI / 8 ));
//
//	m_Shader->SetVar(m_RTSizeVarId, sm_Params.f4RTSize);
//	m_Shader->SetVar(m_fQVarId, sm_Params.fQ);
//	m_Shader->SetVar(m_fQTimesZNearVarId, sm_Params.fQTimesZNear);
//	m_Shader->SetVar(m_fTanHalfHVVarId, sm_Params.fTanHalfHV);
//
//	m_Shader->SetVar(m_DepthTexVarId, CDeferredLightingHelper::GetGBufferDepthTarget());
//	m_Shader->SetVar(m_NormalTexVarId, CDeferredLightingHelper::GetGBufferTarget(1));
//	m_Shader->SetVar(m_LightShaftTexVarId, CDeferredLightingHelper::GetVolumetricLightTarget());
//	m_Shader->SetVar(m_BaseTexVarId, CDeferredLightingHelper::GetGBufferSSAO() );
//#if USE_STENCIL_TARGET
//	m_Shader->SetVar(m_StencilBufferTexVarId, (grcRenderTarget*)CDeferredLightingHelper::GetGBufferStencilTexture());
//#endif
//	ID3D11DeviceContext* pd3dImmediateContext = GRCDEVICE.GetCurrentContextEx();
//	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { static_cast<ID3D11UnorderedAccessView*>(static_cast<grcRenderTargetDX10*>(pSSAOBuffer)->GetUnorderedAccessView()) };
//	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );
//
//	BaseCS::SetupResources(pass);
//}
//void HDAOCS::CleanUpResources() const
//{
//	ID3D11DeviceContext* pd3dImmediateContext = GRCDEVICE.GetCurrentContextEx();
//	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
//	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );
//	BaseCS::CleanUpResources();
//}
//void HDAOCS::Run(u16 xthread, u16 ythread, u16 zthread) const
//{
//	(void)(xthread);
//	(void)(ythread);
//
//	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	
//	PIXBegin(0, "HDAOCS::Run");
//	// CS params
//	static float s_fGroupTexelDimension = 56.0f;
//	static float s_fGroupTexelOverlap = 12.0f;
//	static float s_fGroupTexelDimensionAfterOverlap = s_fGroupTexelDimension - 2 * s_fGroupTexelOverlap;
//
//	u16 iGroupsX = static_cast<u16>(ceil( sm_Params.f4RTSize.GetX() / s_fGroupTexelDimensionAfterOverlap ));
//	u16 iGroupsY = static_cast<u16>(ceil( sm_Params.f4RTSize.GetY() / s_fGroupTexelDimensionAfterOverlap ));
//
//	BaseCS::Run(iGroupsX, iGroupsY, zthread, 0);
//
//	PIXEnd();
//}
/********************************************************************************/
BlurCS::BlurParams BlurCS::sm_Params;
BlurCS::BlurCS(rage::grmShader* pShader) : rage::BaseCS(pShader)
	, m_ResultVarId(grcevNONE)
	, m_RTSizeVarId(grcevNONE)
	, m_SampleWeightsVarId(grcevNONE)
	, m_InputTexVarId(grcevNONE)
{
}
BlurCS::~BlurCS()
{
	Shutdown();
}
float BlurCS::GaussianDistribution( float x, float y, float rho )
{
	static const float TWO_PI = (PI * 2.0f);
	float g = 1.0f / sqrtf( TWO_PI * rho * rho );
	g *= expf( -( x * x + y * y ) / ( 2 * rho * rho ) );

	return g;
}
void BlurCS::GetSampleWeights( float fDeviation, float fMultiplier )
{
	//float totalWeight = 0;
    // Fill the center texel
    float weight = 1.0f * GaussianDistribution( 0, 0, fDeviation );
	int iCenter = (eKernelSize + 1) / 2;
    sm_Params.f4SampleWeights[iCenter].Set( weight, weight, weight, 1.0f );
	//totalWeight += weight;
    // Fill the right side
    for( int i = 1; i < iCenter+1 ; i++ )
    {
        weight = fMultiplier * GaussianDistribution( ( float )i, 0, fDeviation );
        sm_Params.f4SampleWeights[iCenter-i].Set( weight, weight, weight, 1.0f );
	//	totalWeight += weight;
    }

    // Copy to the left side
    for( int i = iCenter+1; i < eKernelSize; i++ )
    {
        sm_Params.f4SampleWeights[i] = sm_Params.f4SampleWeights[(eKernelSize-1) - i];
	//	totalWeight +=  weight;
    }
	//totalWeight = 1.0f/ totalWeight;
	//for(int i = 0 ;i < eKernelSize; ++i)
	//{
	//	sm_Params.f4SampleWeights[i] *= totalWeight;
	//}
}
void BlurCS::Init()
{
	m_ResultVarId = m_Shader->LookupVar("Result");
	m_RTSizeVarId = m_Shader->LookupVar("g_f4RTSize");
	m_SampleWeightsVarId = m_Shader->LookupVar("g_f4SampleWeights");
	m_InputTexVarId = m_Shader->LookupVar("InputTex");
	m_PointSamplerVarId = m_Shader->LookupVar("g_PointSampler");
	m_LinearSamplerVarId = m_Shader->LookupVar("g_LinearClampSampler");
	GetSampleWeights(3.0f, 1.25f);

	grcSamplerStateDesc desc;
	desc.Filter = grcSSV::FILTER_MIN_MAG_MIP_POINT;
	m_PointSampler = grcStateBlock::CreateSamplerState(desc);

	desc.AddressU = grcSSV::TADDRESS_CLAMP;
	desc.AddressV = grcSSV::TADDRESS_CLAMP;
	desc.Filter = grcSSV::FILTER_MIN_MAG_MIP_LINEAR;
	m_LinearSampler = grcStateBlock::CreateSamplerState(desc);
}

void BlurCS::Shutdown()
{

}

#if __BANK
void BlurCS::AddBankWidgets(bkBank& bank)
{
	float m_FilterType = 0.0f;
	bank.PushGroup("Blur", false);
	bank.AddSlider("Filter", &m_FilterType, 0.0f, 1.0f, 0.01f);
	bank.PopGroup();
}
#endif
void BlurCS::SetBuffers(grcRenderTarget* inout, grcRenderTarget* temp)
{
	m_pInputBuffer[0] = inout;
	m_pInputBuffer[1] = temp;
	m_pOutputBuffer[0]  = temp;
	m_pOutputBuffer[1]  = inout;
}
void BlurCS::SetupResources(u8 programId) const
{
	sm_Params.f4RTSize.Set(static_cast<float>(m_pInputBuffer[programId]->GetWidth()), static_cast<float>(m_pInputBuffer[programId]->GetHeight()), 
						   1.0f/static_cast<float>(m_pOutputBuffer[programId]->GetWidth()), 1.0f/static_cast<float>(m_pOutputBuffer[programId]->GetHeight()));
	
	m_Shader->SetVar(m_RTSizeVarId, sm_Params.f4RTSize);
	m_Shader->SetVar(m_SampleWeightsVarId, sm_Params.f4SampleWeights, eKernelSize);

	m_Shader->SetVarUAV(m_ResultVarId, static_cast<grcRenderTargetD3D11*>(m_pOutputBuffer[programId]) );
	m_Shader->SetVar(m_InputTexVarId, m_pInputBuffer[programId]);

	/*ID3D11DeviceContext* pd3dImmediateContext = GRCDEVICE.GetCurrentContextEx();
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { static_cast<ID3D11UnorderedAccessView*>(static_cast<grcRenderTargetDX11*>(m_pOutputBuffer[programId])->GetUnorderedAccessView()) };
	g_grcCurrentContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );*/

	// force sampler state to do multiple sampler state sampling per texture
	/*grcComputeProgram* pComputeProgram = m_Shader->GetComputeProgram(pass);
	pComputeProgram->SetTexture(1,NULL,static_cast<u16>(m_PointSampler),true);
	pComputeProgram->SetTexture(2,NULL,static_cast<u16>(m_LinearSampler),true);*/

	BaseCS::SetupResources(programId);
}
void BlurCS::CleanUpResources() const
{
	/*ID3D11DeviceContext* pd3dImmediateContext = GRCDEVICE.GetCurrentContextEx();
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
	g_grcCurrentContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );*/
	BaseCS::CleanUpResources();
}
void BlurCS::Run(u16 xthread, u16 ythread, u16 zthread) const
{
	static unsigned int RUN_SIZE = 128;
	static unsigned int RUN_LINES = 2;

	m_Shader->SetSamplerState(m_PointSamplerVarId, m_PointSampler);
	m_Shader->SetSamplerState(m_LinearSamplerVarId, m_LinearSampler);

	zthread = 1;
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	// CS Horizontal
	xthread = (u16)ceil((float)m_pOutputBuffer[0]->GetWidth() / RUN_SIZE);
	ythread = (u16)ceil((float)m_pOutputBuffer[0]->GetHeight() / RUN_LINES);
	BaseCS::Run( "BlurCS::Run-1", xthread, ythread, zthread, 0);

	// CS Vertical
	xthread = (u16)ceil((float)m_pOutputBuffer[1]->GetWidth() / RUN_LINES );
	ythread = (u16)ceil((float)m_pOutputBuffer[1]->GetHeight() / RUN_SIZE );
	BaseCS::Run( "BlurCS::Run-2", xthread, ythread, zthread, 1);
}
/********************************************************************************/
#if 0 //this is a sample CS of doing non-graphical computations using Compute Shaders
TempCS::TempCS(rage::grmShader* pShader) : rage::BaseCS(pShader)
{
	for(int j = 0; j < kNumBuffers; ++j)
	{
		m_pBuffers[j] = NULL;
		m_SRViews[j] = NULL;
	}
	
	m_pBufResult = NULL;
	m_UAView = NULL;
}
TempCS::~TempCS()
{
	Shutdown();
}
void TempCS::Init()
{
	for(int j = 0; j < kNumBuffers; ++j)
	{
		for ( int i = 0; i < kNumElements; ++i ) 
		{
			m_InputData[j][i].i = i;
			m_InputData[j][i].f = (float)i;
	#ifdef TEST_DOUBLE
			m_InputData[j][i].d = (double)i;
	#endif
		}
	}

#ifdef USE_STRUCTURED_BUFFERS
	CreateStructuredBuffer( sizeof(BufType), kNumElements, &m_InputData[0][0], &m_pBuffers[0]);
	CreateStructuredBuffer( sizeof(BufType), kNumElements, &m_InputData[1][0], &m_pBuffers[1]);
	CreateStructuredBuffer( sizeof(BufType), kNumElements, NULL, &m_pBufResult );
#else
	CreateRawBuffer( kNumElements * sizeof(BufType), &m_InputData[0][0], &m_pBuffers[0] );
	CreateRawBuffer( kNumElements * sizeof(BufType), &m_InputData[1][0], &m_pBuffers[1] );
	CreateRawBuffer( kNumElements * sizeof(BufType), NULL, &m_pBufResult );
#endif


	for(int i = 0; i < kNumBuffers; ++i)
	{
		CreateBufferSRV( m_pBuffers[i], &m_SRViews[i] );
	}
	
	CreateBufferUAV( m_pBufResult, &m_UAView );
}
void TempCS::Shutdown()
{
	
	if (m_UAView) 
	{
		m_UAView->Release();
		m_UAView = NULL;
	}

	if (m_pBufResult)
	{
		m_pBufResult->Release();
		m_pBufResult = NULL;
	}

	for(int i = 0; i < kNumBuffers; ++i)
	{
		if (m_pBuffers[i])
		{
			m_pBuffers[i]->Release();
			m_pBuffers[i] = NULL;
		}

		if (m_SRViews[i]) 
		{
			m_SRViews[i]->Release();
			m_SRViews[i] = NULL;
		}
	}
}
//--------------------------------------------------------------------------------------
// Create a CPU accessible buffer and download the content of a GPU buffer into it
// This function is for verifying the CS
//-------------------------------------------------------------------------------------- 
void TempCS::DebugVerify( ID3D11Buffer* pBuffer ) const
{
	PF_PUSH_TIMEBAR("TempCS::DebugVerify ");
	ID3D11Buffer* debugbuf = NULL;

	D3D11_BUFFER_DESC desc;
	ZeroMemory( &desc, sizeof(desc) );
	pBuffer->GetDesc( &desc );
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	desc.Usage = D3D11_USAGE_STAGING;
	desc.BindFlags = 0;
	desc.MiscFlags = 0;
	HRESULT hr = GRCDEVICE.GetCurrentEx()->CreateBuffer(&desc, NULL, &debugbuf);
	if (hr == S_OK)
	{
		ID3D11DeviceContext* pd3dImmediateContext = GRCDEVICE.GetCurrentContextEx();
		pd3dImmediateContext->CopyResource( debugbuf, pBuffer );
		D3D11_MAPPED_SUBRESOURCE oMappedResource; 
		HRESULT hr = pd3dImmediateContext->Map( debugbuf, 0, (D3D11_MAP) D3D11_MAP_READ, 0,(D3D11_MAPPED_SUBRESOURCE*)&oMappedResource );
		switch (hr)
		{
		case D3D11_ERROR_FILE_NOT_FOUND: 
			break; // //The file was not found. 
		case D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS: 
			break; // There are too many unique instances of a particular type of state object. 
		case D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS: 
			break; // There are too many unique instance of a particular type of view object. 
		case D3D11_ERROR_DEFERRED_CONTEXT_MAP_WITHOUT_INITIAL_DISCARD: 
			break; // The first call to ID3D11DeviceContext::Map after either ID3D11Device::CreateDeferredContext or ID3D11DeviceContext::FinishCommandList per Resource was not D3D11_MAP_WRITE_DISCARD. 
		case D3DERR_INVALIDCALL: 
			break; // The method call is invalid. For example, a method's parameter may not be a valid pointer. 
		case D3DERR_WASSTILLDRAWING: 
			break; // The previous blit operation that is transferring information to or from this surface is incomplete. 
		case E_FAIL: 
			break; // Attempted to create a device with the debug layer enabled and the layer is not installed. 
		case E_INVALIDARG: 
			break; // An invalid parameter was passed to the returning function. 
		case E_OUTOFMEMORY: 
			break; // Direct3D could not allocate sufficient memory to complete the call. 
		case S_FALSE: 
			break; // Alternate success value, indicating a successful but nonstandard completion (the precise meaning depends on context). 
		case S_OK: 
			break; // No error occurred. 
		}
		// Set a break point here and put down the expression "p, 1024" in your watch window to see what has been written out by our CS
		// This is also a common trick to debug CS programs.
		BufType *p = NULL;
		p = (BufType*)oMappedResource.pData;

		// Verify that if Compute Shader has done right
		if (p != NULL)
		{
			BOOL bSuccess = TRUE;
			for ( int i = 0; i < kNumElements; ++i )
			{
				BufType r;
				memset(&r, 0, sizeof(BufType));
				for(int j = 0; j < kNumBuffers; ++j)
				{
					r.i += m_InputData[j][i].i;
					r.f += m_InputData[j][i].f;
#ifdef TEST_DOUBLE
					r.d += m_InputData[j][i].d;
#endif
				}
				if ( (p[i].i != r.i ) || (p[i].f != r.f) 
#ifdef TEST_DOUBLE
					|| (p[i].d != r.d)
#endif
					)
				{
					bSuccess = FALSE;
					break;
				}
			}
			Assert( bSuccess );
			pd3dImmediateContext->Unmap( debugbuf, 0 );
		}

		debugbuf->Release();

	}
	PF_POP_TIMEBAR();
}
void TempCS::SetupResources() const
{
	ID3D11DeviceContext* pd3dImmediateContext = GRCDEVICE.GetCurrentContextEx();
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, &m_UAView, NULL );
	//SSEN - ToDo : this should go into effect_d3d setvar
	//pd3dImmediateContext->CSSetShaderResources( 0, kNumBuffers, m_SRViews);

	BaseCS::SetupResources();
}
void TempCS::CleanUpResources() const
{
	ID3D11DeviceContext* pd3dImmediateContext = GRCDEVICE.GetCurrentContextEx();
	ID3D11UnorderedAccessView* ppUAViewNULL[1] = { NULL };
	pd3dImmediateContext->CSSetUnorderedAccessViews( 0, 1, ppUAViewNULL, NULL );

	BaseCS::CleanUpResources();
}
void TempCS::Run(u16 xthread, u16 ythread, u16 zthread) const
{
	(void)(xthread);
	Assert(CSystem::IsThisThreadId(SYS_THREAD_RENDER));	

	//
	BaseCS::Run( "TempCS::Run", kNumElements, ythread, zthread, 0 );

	// Read back the result from GPU, verify its correctness against result computed by CPU
	DebugVerify( m_pBufResult );
}

#endif 
/********************************************************************************/
bool CSGameManager::sm_EnableCS[CSGameManager::CS_COUNT];

CSGameManager::CSGameManager() : rage::CSManagerBase()
{
	for(int  i = 0; i < CS_COUNT; ++i)
	{
		sm_EnableCS[i] = true;
	}
}
CSGameManager::~CSGameManager()
{
}

void CSGameManager::Init()
{
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		m_ComputeShaders.Reserve(CS_COUNT);
		for(int  i =0; i < CS_COUNT; ++i)
			m_ComputeShaders.Push(NULL);


		//Create HDAO Compute Shader
//		grmShader* pShader = grmShaderFactory::GetInstance().Create();
//		if (pShader->Load("common:/shaders/hdaoCS"))
//		{
//			HDAOCS* pHDAOCS = rage_new HDAOCS(pShader);
//			pHDAOCS->Init();
//			m_ComputeShaders[CS_HDAO] = pHDAOCS;
//		}
//		else
//		{
//			AssertMsg(false, "Cannot Create Compute Shader for HDAO");
//#if !__RESOURCECOMPILER
//			grmShaderFactory::GetInstance().RemoveShaderReference(pShader);
//#endif
//		}
		//Create Blur Compute Shader
		ASSET.PushFolder("common:/shaders");

		grmShader* pShader = grmShaderFactory::GetInstance().Create();
		if (pShader->Load("BlurCS"))
		{
			BlurCS* pBlurCS = rage_new  BlurCS(pShader);
			pBlurCS->Init();
			m_ComputeShaders[CS_BLUR] = pBlurCS;
		}
		else
		{
			AssertMsg(false, "Cannot Create Compute Shader for Blur");
//#if !__RESOURCECOMPILER
//			grmShaderFactory::GetInstance().RemoveShaderReference(pShader);
//#endif
		}
		
		ASSET.PopFolder();

		////Create Water Compute Shader
		//pShader = grmShaderFactory::GetInstance().Create();
		//if (pShader->Load("common:/shaders/waterCS"))
		//{
		//	WaterCS* pWaterCS = rage_new WaterCS(pShader);
		//	pWaterCS->Init();
		//	m_ComputeShaders[CS_WATER] = pWaterCS;
		//}
		//else
		//{
		//	grmShaderFactory::GetInstance().RemoveShaderReference(pShader);
		//}
	}

}
void CSGameManager::Shutdown()
{
	if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	{
		for(int i = 0; i < m_ComputeShaders.GetCount(); ++i)
		{
			if (m_ComputeShaders[i])
			{
				m_ComputeShaders[i]->Shutdown();
				delete m_ComputeShaders[i];
				m_ComputeShaders[i] = NULL;
			}
		}

		m_ComputeShaders.Reset();
	}
}

#if __BANK
void CSGameManager::AddBankWidgets(bkBank& /*bank*/)
{
	//if (GRCDEVICE.GetDxFeatureLevel() >= 1100)
	//{
	//	bank.PushGroup("Compute Shader", false);
	//	bank.AddToggle("Enable HDAO", &sm_EnableCS[CSGameManager::CS_HDAO]);
	//	bank.AddToggle("Enable Water", &sm_EnableCS[CSGameManager::CS_WATER]);
	//	HDAOCS::AddBankWidgets(bank);
	//	bank.PopGroup();
	//}
}
#endif

#endif
