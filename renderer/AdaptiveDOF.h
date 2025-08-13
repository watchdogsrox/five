
#ifndef _ADAPTIVE_DOF_H_
#define _ADAPTIVE_DOF_H_

#if ADAPTIVE_DOF

#include "camera/helpers/Frame.h"
#include "renderer/PostProcessFX.h"
#include "input/headtracking.h"

namespace PostFX
{
#if ADAPTIVE_DOF_CPU
	struct sAdaptiveDofHistory
	{
		u32 time;
		Vector4 DOFParams;
	};

#define ADAPTIVE_DOF_HISTORY_SIZE	256
#endif

	class AdaptiveDOF
	{
	public:

		AdaptiveDOF();
		~AdaptiveDOF();

		void Init();
#if ADAPTIVE_DOF_OUTPUT_UAV
		void InitiliaseOnRenderThread();
#endif //ADAPTIVE_DOF_OUTPUT_UAV

		void DeleteRenderTargets();

		void CreateRenderTargets();		
#if ADAPTIVE_DOF_CPU
		void ProcessAdaptiveDofCPU();
		void CalculateCPUParams(Vector4 &AdaptiveDofParams, float& blurDiskRadiusToApply);
#endif
#if ADAPTIVE_DOF_GPU
		void ProcessAdaptiveDofGPU(const PostFXParamBlock::paramBlock& settings, grcRenderTarget*& prevExposureRT, bool wasAdaptiveDofProcessedOnPreviousUpdate);
#if ADAPTIVE_DOF_OUTPUT_UAV
		grcBufferUAV* GetParamsRT() { return adaptiveDofParamsBuffer;}
#else
		grcRenderTarget* GetParamsRT() { return adaptiveDofParams;}
#endif 
#endif
		void SetEnabled(bool val) { AdaptiveDofEnabled = val;}
		bool IsEnabled() { return AdaptiveDofEnabled && !rage::ioHeadTracking::IsMotionTrackingEnabled(); }

		void SetLockFocusDistance(bool val) { LockFocusDistance = val; }

		void UpdateVisualDataSettings();

#if __BANK
		void AddWidgets(bkBank &bk, PostFX::PostFXParamBlock::paramBlock &edittedSettings);
		void DrawFocusGrid();
#endif

	private:

		bool AdaptiveDofEnabled;
		bool LockFocusDistance;

#if ADAPTIVE_DOF_GPU
		grmShader*				AdaptiveDOFShader;
		grcEffectTechnique		AdaptiveDOFTechnique;

		grcEffectVar			AdaptiveDofProjVar;
		grcEffectVar			AdaptiveDofDepthDownSampleParamsVar;
		grcEffectVar			AdaptiveDofParams0Var;
		grcEffectVar			AdaptiveDofParams1Var;
		grcEffectVar			AdaptiveDofParams2Var;
		grcEffectVar			AdaptiveDofParams3Var;
		grcEffectVar			AdaptiveDofFocusDistanceDampingParams1Var;
		grcEffectVar			AdaptiveDofFocusDistanceDampingParams2Var;
		grcEffectVar			AdaptiveDofDepth;
		grcEffectVar			AdaptiveDofTexture;
		grcEffectVar			AdaptiveDofExposureTex;

		grcEffectVar			reductionComputeInputTex;
		grcEffectVar			reductionComputeOutputTex;

# if ADAPTIVE_DOF_OUTPUT_UAV && (RSG_DURANGO || RSG_ORBIS)
		grcEffectVar			AdaptiveDOFOutputBufferVar;
# endif

		grcRenderTarget*		depthRedution0;
		grcRenderTarget*		depthRedution1;
		grcRenderTarget*		depthRedution2;
		grcRenderTarget*		adaptiveDofParams;
#if ADAPTIVE_DOF_OUTPUT_UAV
		grcBufferUAV*			adaptiveDofParamsBuffer;
#endif //ADAPTIVE_DOF_OUTPUT_UAV

		//To allow the values to be read back for debugging.
		grcTexture*				adaptiveDofParamsTexture;
#if __BANK
		grcTexture*				depthReduction2Texture;
		grcRenderTarget*		depthRedution2ReadBack;
#endif
		
#endif

#if __BANK
		char AdaptiveDofDesiredFocusDistanceString[64];
		char AdaptiveDofDampedFocusDistanceString[64];
		char AdaptiveDofFinalFocusDistanceString[64];
		char AdaptiveDofMaxBlurDiskRadiusNearString[64];
		char AdaptiveDofMaxBlurDiskRadiusFarString[64];
		Vector2 AdaptiveDofFocusDistanceGridScaling;
		bool AdaptiveDofDrawFocusGrid;

		bool AdaptiveDofOverrideFocalDistance;
		float AdaptiveDofOverriddenFocalDistanceVal;

		bool ShouldMeasurePostAlphaPixelDepth;
#endif //__BANK

		Vector2 AdaptiveDofExposureLimits;
		u32 AdaptiveDofHistoryIndex;
#if ADAPTIVE_DOF_CPU
		float AdaptiveDofRayMissedValue;
		u32 AdaptiveDofFocalGridSize;

		sAdaptiveDofHistory AdaptiveDofParamHistory[ADAPTIVE_DOF_HISTORY_SIZE];
		float AdaptiveDofBlurDiskRadiusToApply;
		float AdaptiveDofHistoryTime;
#endif
	};
}

#endif //ADAPTIVE_DOF

#endif //_ADAPTIVE_DOF_H_
