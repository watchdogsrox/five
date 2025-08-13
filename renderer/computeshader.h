#ifndef __COMPUTESHADER_H_
#define __COMPUTESHADER_H_

#include "grcore/computeshader.h"

#if __D3D11

class BlurCS : public BaseCS
{
public:
	enum 
	{
		eBlurGaussian,
		eBlurBox,
		eBlurNone,

		eKernelSize = 15 //should be same in the shader.... 
	};
	struct BlurParams
	{
		Vector4 f4SampleWeights[eKernelSize];
		Vector4 f4RTSize;
	};
	BlurCS(rage::grmShader* pShader);
	~BlurCS();


	void Init();
	void Shutdown();
	void SetupResources(u8 programId = 0) const;
	void Run(u16 xthread, u16 ythread, u16 zthread) const;
	void CleanUpResources() const;
	
	void SetBuffers(grcRenderTarget* inout, grcRenderTarget* temp);
#if __BANK
	static void AddBankWidgets(bkBank& bank);
#endif
	void GetSampleWeights( float fDeviation, float fMultiplier );
protected:
	//void GetSampleWeights( float fDeviation, float fMultiplier );
	float GaussianDistribution( float x, float y, float rho );

	grcSamplerStateHandle		m_PointSampler;
	grcSamplerStateHandle		m_LinearSampler;

	grcEffectVar		m_ResultVarId;
	grcEffectVar		m_RTSizeVarId;
	grcEffectVar		m_SampleWeightsVarId;
	grcEffectVar		m_InputTexVarId;
	grcEffectVar		m_PointSamplerVarId;
	grcEffectVar		m_LinearSamplerVarId;
	grcRenderTarget*	m_pInputBuffer[2];	// 0 -> Output for Vertical pass (inout), 1 -> for Horizontal Pass (temp)
	grcRenderTarget*	m_pOutputBuffer[2]; // 0 -> Output for Vertical pass (temp), 1 -> for Horizontal Pass(inout)

	static BlurParams sm_Params;
};
//class HDAOCS : public BaseCS
//{
//public:
//
//	struct HDAOParams
//	{
//		Vector4	f4RTSize;			// Used by HDAO shaders for scaling texture coords
//		float	fQ;					// far / (far - near)
//		float	fQTimesZNear;			// Q * near
//		Vector2	fTanHalfHV;			// Tan( Half Horiz FOV )
//
//		float 	fHDAORejectRadius;	// Camera Z values must fall within the reject and accept radius to be  
//		float 	fHDAOAcceptRadius;	// considered as a valley  
//		float 	fHDAORecipFadeOutDist; //1.0f / 0.6f;
//		float 	fHDAOIntensity;   	// Simple scaling factor to control the intensity of the occlusion  
//		float 	fHDAONormalScale; 	// Scaling factor to control the effect the normals have   
//		float 	fAcceptAngle;     	// Used by the ValleyAngle function to determine shallow valleys  
//		
//		bool	bUseCSBlur;
//		HDAOParams()
//		{
//			f4RTSize.Set(1280.0f, 720.0f, 1.0f/1280.0f, 1.0f/720.f);
//			fQ					= 1.001001001f; //(1000 / (1000 - 1))
//			fQTimesZNear		= 1001.001f;
//			fTanHalfHV.Set(0.0f, 0.0f);			
//			fHDAORejectRadius	= 0.8f;		
//			fHDAOAcceptRadius	= 0.003f;	
//			fHDAORecipFadeOutDist = 1.0f/0.6f;
//			fHDAOIntensity		= 0.5f;		
//			fHDAONormalScale	= 0.05f;		
//			fAcceptAngle		= 0.98f;	
//			bUseCSBlur			= true;
//		}
//	};
//
//	HDAOCS(rage::grmShader* pShader);
//	~HDAOCS();
//
//	void Init();
//	void Shutdown();
//	void SetupResources(int pass = 0) const;
//	void Run( u16 xthread, u16 ythread, u16 zthread) const;
//	void CleanUpResources() const;
//	void ChangeResolution();
//
//	static bool UseCSBlur()		{ return sm_Params.bUseCSBlur; }
//#if __BANK
//	static void AddBankWidgets(bkBank& bank);
//#endif
//protected:
//	grcEffectVar m_RejectRadiusVarId;
//	grcEffectVar m_AcceptRadiusVarId;
//	grcEffectVar m_RecipFadeOutDistVarId;
//	grcEffectVar m_IntensityVarId;
//	grcEffectVar m_NormalScaleVarId;
//	grcEffectVar m_AcceptAngleVarId; 
//	grcEffectVar m_RTSizeVarId;
//	grcEffectVar m_fQVarId;
//	grcEffectVar m_fQTimesZNearVarId;
//	grcEffectVar m_fTanHalfHVVarId;
//	grcEffectVar m_AmbientLightParamsVarId;
//
//	grcEffectVar m_DepthTexVarId;
//	grcEffectVar m_NormalTexVarId;
//	grcEffectVar m_LightShaftTexVarId;
//	grcEffectVar m_BaseTexVarId;
//	grcEffectVar m_StencilBufferTexVarId;
//
//	static HDAOParams sm_Params;
//};
//
//class WaterCS : public BaseCS
//{
//public:
//	WaterCS(rage::grmShader* pShader);
//	~WaterCS();
//
//	void Init();
//	void Shutdown();
//	void SetupResources(int pass = 0) const;
//	void Run( u16 xthread, u16 ythread, u16 zthread) const;
//	void CleanUpResources() const;
//};

#if 0
//these have to be same as teh shader source...
#define USE_STRUCTURED_BUFFERS
//#define TEST_DOUBLE
class TempCS : public BaseCS
{
public:
	// If defined, then the hardware/driver must report support for double-precision CS 5.0 shaders or the sample fails to run
	//#define TEST_DOUBLE

	// The number of elements in a buffer to be tested
	enum { 
		kNumBuffers = 2,
		kNumElements = 1024
	};

	struct BufType
	{
		int i;
		float f;
#ifdef TEST_DOUBLE
		double d;
#endif
	};
	
	TempCS(rage::grmShader* pShader);
	~TempCS();

	void Init();
	void Shutdown();
	void SetupResources(u8 programId = 0) const;
	void Run( u16 xthread, u16 ythread, u16 zthread) const;
	void CleanUpResources() const;
protected:
	void DebugVerify( ID3D11Buffer* pBuffer ) const;

	BufType						m_InputData[kNumBuffers][kNumElements];

	ID3DnnBuffer*				m_pBuffers[kNumBuffers];
	ID3DnnBuffer*				m_pBufResult;
	ID3D11UnorderedAccessView*	m_UAView;
	ID3DnnShaderResourceView*	m_SRViews[kNumBuffers];
};

#endif //USER_SSEN

class CSGameManager : public rage::CSManagerBase
{
public:
	CSGameManager();
	~CSGameManager();

	enum ECSType
	{
		//CS_HDAO = 0,
		CS_BLUR = 0,
		//CS_WATER,
		CS_COUNT
	};
	void Init();
	void Shutdown();
	
	static bool IsEnabled(u32 index)				{ return sm_EnableCS[index]; }
#if __BANK
	static void AddBankWidgets(bkBank& bank);
#endif
protected:
	static bool sm_EnableCS[CS_COUNT];
};

extern CSGameManager gCSGameManager;
#endif 

#endif
