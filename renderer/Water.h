//
// Title	:	Water.h
// Author	:	Obbe
//
//
//
//
//
#ifndef _WATER_H_
#define _WATER_H_

#if !__SPU
	#include "vector/vector3.h"

	#include "Renderer/RenderThread.h"
	#include "Renderer/DrawLists/drawList.h"
#if __PPU
	#include "grcore/device.h"
#endif // __PPU	
#endif //!__SPU...

#include "parser/macros.h"
#include "data/base.h"
#include "Renderer/RenderPhases/RenderPhase.h"

#include "../shader_source/Water/water_common.fxh"
#include "../../rage/framework/src/timecycle/tccycle.h"

class CCalmingQuad
{
public:
	s16	minX, minY, maxX, maxY;
	float	m_fDampening;
};

class CWaveQuad
{
public:

	float	GetAmplitude(){ return amp/255.0f; }
	void	SetAmplitude(float amplitude){ amp = (u16)(amplitude*255.0f); }

	float	GetXDirection(){ return x/127.0f; }
	void	SetXDirection(float dir){ x = (s8)(dir*127.0f); }

	float	GetYDirection(){ return y/127.0f; }
	void	SetYDirection(float dir){ y = (s8)(dir*127.0f); }

	s16	minX, minY, maxX, maxY;
	u16 amp;
	s8 x, y;
};

namespace rage {
	class spdkDOP2D_8;
	class grcTexture;
	class grcRenderTarget;
	class bkBank;
}

class CPhysical;

#define USE_WATER_REGIONS (__DEV || (__WIN32PC && !__FINAL)) // required for heightmap tool

#define REJECTIONABOVEWATER (20.0f)
#define POOL_DEPTH			(6.0f)

enum
{
	QUADSTRIS_EMPTY=0,
	QUADSTRIS_QUAD,
	QUADSTRIS_INDEX
};

class CSecondaryWaterTune
{
public:
	void interpolate(const CSecondaryWaterTune &src, const CSecondaryWaterTune &dst, float interp);
#if __BANK
	void InitWidgets(bkBank *bank);
#endif	
	float m_RippleBumpiness;
	float m_RippleMinBumpiness;
	float m_RippleMaxBumpiness;
	float m_RippleBumpinessWindScale;
	float m_RippleSpeed;
	float m_RippleDisturb;
	float m_RippleVelocityTransfer;
	float m_OceanBumpiness;
	float m_DeepOceanScale;
	float m_OceanNoiseMinAmplitude;
	float m_OceanWaveAmplitude;
	float m_ShoreWaveAmplitude;
	float m_OceanWaveWindScale;
	float m_ShoreWaveWindScale;
	float m_OceanWaveMinAmplitude;
	float m_ShoreWaveMinAmplitude;
	float m_OceanWaveMaxAmplitude;
	float m_ShoreWaveMaxAmplitude;
	float m_OceanFoamIntensity;
	
	PAR_SIMPLE_PARSABLE;
};


class CMainWaterTune
{
public:
	Color32 m_WaterColor;
	float m_RippleScale;
	float m_OceanFoamScale;
	float m_SpecularFalloff;
	float m_FogPierceIntensity;
	float m_RefractionBlend;
	float m_RefractionExponent;
	float m_WaterCycleDepth;
	float m_WaterCycleFade;
	float m_WaterLightningDepth;
	float m_WaterLightningFade;
	float m_DeepWaterModDepth;
	float m_DeepWaterModFade;
	float m_GodRaysLerpStart;
	float m_GodRaysLerpEnd;
	float m_DisturbFoamScale;
	Vector2 m_FogMin;
	Vector2 m_FogMax;
	
	float GetSpecularIntensity(const tcKeyframeUncompressed& currKeyframe) const;

	static bool m_OverrideTimeCycle;
	static float m_SpecularIntensity;

	PAR_SIMPLE_PARSABLE;
};


#define MAXDISTURB	32
#define MAXFOAM		32
struct WaterDisturb{
	s16 m_x;
	s16 m_y;
	float m_amount[2];
	float m_change[2];
};
struct WaterFoam{
	s16 m_x;
	s16 m_y;
	float m_amount;
};

namespace Water
{
#define DEFAULT_WATERCOLOR					8,22,21,40
#define DEFAULT_RIPPLEBUMPINESS				(0.5f)
#define DEFAULT_RIPPLEMINBUMPINESS			(0.4f)
#define DEFAULT_RIPPLEMAXBUMPINESS			(0.7f)
#define DEFAULT_RIPPLEBUMPINESSWINDSCALE	(0.5f)
#define DEFAULT_RIPPLESCALE					(0.1f)
#define DEFAULT_RIPPLESPEED					(0.0f)
#define DEFAULT_SPECULARINTENSITY			(3.0f)
#define DEFAULT_SPECULARFALLOFF				(256)
#define DEFAULT_FOGLIGHTINTENSITY			(1.0f)
#define DEFAULT_FOGPIERCEINTENSITY			(1.0f)
#define DEFAULT_REFRACTIONBLEND				(0.5f)
#define DEFAULT_REFRACTIONEXPONENT			(0.25f)
#define DEFAULT_PARALLAXINTENSITY			(0.3f)
#define DEFAULT_OCEANBUMPINESS				(0.25f)
#define DEFAULT_OCEANWAVEAMPLITUDE			(0.5f)
#define DEFAULT_SHOREWAVEAMPLITUDE			(2.0f)
#define DEFAULT_OCEANWAVEWINDSCALE			(0.5f)
#define DEFAULT_SHOREWAVEWINDSCALE			(0.5f)
#define DEFAULT_OCEANWAVEMINAMPLITUDE		(0.2f)
#define DEFAULT_SHOREWAVEMINAMPLITUDE		(0.5f)
#define DEFAULT_OCEANWAVEMAXAMPLITUDE		(0.5f)
#define DEFAULT_SHOREWAVEMAXAMPLITUDE		(2.0f)
#define DEFAULT_OCEANFOAMINTENSITY			(2.0f)
#define DEFAULT_OCEANFOAMSCALE				(1.0f)
#define DEFAULT_WATERCYCLEDEPTH				(100.0f)
#define DEFAULT_WATERCYCLEFADE				(25.0f)
#define DEFAULT_DEEPWATERMODDEPTH			(150.0f)
#define DEFAULT_DEEPWATERMODFADE			(25.0f)
#define DEFAULT_GODRAYSLERPSTART			(150.0f)
#define DEFAULT_GODRAYSLERPEND				(200.0f)
#define DEFAULT_REFRACTIONINDEX				(1.22f)

	// Variables
#if !__SPU

	extern	u32	ms_bufIdx;

	extern grcTexture* m_noiseTexture;
	extern grcTexture* m_CausticTexture;
	//extern grcTexture* m_NormalTexture;

#if RSG_PC
	extern bool	m_bDepthUpdated;
#endif

#if __BANK
	extern bool m_bForceWaterPixelsRenderedON; // overrides GetWaterPixelsRendered
	extern bool m_bForceWaterPixelsRenderedOFF;
	extern bool m_bForceWaterReflectionON; // overrides TCVAR_WATER_REFLECTION to 1
	extern bool m_bForceWaterReflectionOFF; // overrides TCVAR_WATER_REFLECTION to 0
	extern bool m_bForceMirrorReflectionOn;
	extern bool m_bForceMirrorReflectionOff;
	extern bool m_bUnderwaterPlanarReflection;
	enum
	{
		WATER_REFLECTION_SOURCE_DEFAULT,
		WATER_REFLECTION_SOURCE_BLACK,
		WATER_REFLECTION_SOURCE_WHITE,
		WATER_REFLECTION_SOURCE_SKY_ONLY,
		WATER_REFLECTION_SOURCE_COUNT
	};
	extern int   m_debugReflectionSource;
#if WATER_SHADER_DEBUG
	extern float m_debugAlphaBias;
	extern float m_debugRefractionOverride;
	extern float m_debugReflectionOverride;
	extern float m_debugReflectionOverrideParab;
	extern float m_debugReflectionBrightnessScale;
	extern bool  m_debugWaterFogBiasEnabled;
	extern float m_debugWaterFogDepthBias;
	extern float m_debugWaterFogRefractBias;
	extern float m_debugWaterFogColourBias;
	extern float m_debugWaterFogShadowBias;
	extern bool  m_debugDistortionEnabled;
	extern float m_debugDistortionScale;
	extern float m_debugHighlightAmount;
#endif // WATER_SHADER_DEBUG
	extern bool	m_bRunScanCutBlocksEarly;
#endif // __BANK

	bool IsUsingMirrorWaterSurface();

	bool		SynchRecordingWithWater();

	extern grcRenderTarget* sm_pWaterSurface;

	extern float m_waterSurfaceTextureSimParams[8];
	
	void				FlipBuffers();

	inline grcTexture*	GetCausticTexture()				{ return m_CausticTexture; }
	inline grcTexture*	GetNoiseTexture()				{ return m_noiseTexture; }

#if RSG_PC
	void                UpdatePreviousWaterBumps(bool currentFrameDrawn);
#endif
	void				UpdateWaterBumpTexture(u32 pixels);
	grcTexture*			GetWaterHeightMap();
	grcRenderTarget*	GetRefractionRT();
	grcRenderTarget*	GetWaterBumpHeightRT();
	grcTexture*			GetBumpTexture();
	grcRenderTarget*	GetWaterFogTexture();
#if __PS3
	grcRenderTarget*	GetFogDepthRT();
#endif

	void				SetReflectionRT(grcRenderTarget* rt);
	grcRenderTarget*	GetReflectionRT();
#if RSG_PC
	void				SetMSAAReflectionRT(grcRenderTarget* rt);
	grcRenderTarget*	GetMSAAReflectionRT();
#endif
	const grcTexture*	GetWetMap();
	void				SetReflectionMatrix(const Mat44V& matrix);

	void				SetupRefractionTexture();

	void		ModifyDynamicWaterSpeed(float WorldX, float WorldY, float NewSpeed, float ChangePercentage);
	void		AddFoam(float worldX, float worldY, float amount);

	int			AddExtraCalmingQuad(int minX, int minY, int maxX, int maxY, float dampening);
	void		RemoveExtraCalmingQuad(int idx);
	void		RemoveAllExtraCalmingQuads();
	
	void		SetScriptDeepOceanScaler(float s);
	float		GetScriptDeepOceanScaler();

	void		SetScriptCalmedWaveHeightScaler(float s);
	
	void		LoadWater(const char* path = NULL);

#if RSG_PC || RSG_DURANGO || RSG_ORBIS
	void		CreateScreenResolutionBasedRenderTargets();
	void		DeleteScreenResolutionBasedRenderTargets();
#endif
#if RSG_PC
	void		DeviceLost();
	void		DeviceReset();
#endif
	void		ScanCutBlocksWrapper(); 
	void		Init(unsigned initMode);
//	void		LoadWaterFiles(const char *pFileName1);
	void		InitDLCCommands();
	void		InitFogTextureTxdSlots();

	void		Shutdown(unsigned shutdownMode);

	void		AddWaveToResult(float x, float y, float *pWaterZ, Vector3 *pNormal, float *pSpeedUp, bool bHighDetail=false);

	// Helper functions for AddWaveToResult().
	void		SetWaterCellVertexPositions(const float fBaseX, const float fBaseY, Vec2V_InOut v0, Vec2V_InOut v1, Vec2V_InOut v2, Vec2V_InOut v3);
	void		TransformWaterVertexXY(Vec2V_InOut vVert);
	Vec2V_Out	ComputeRightNormalToLineSegmentXY(Vec2V_ConstRef v0, Vec2V_ConstRef v1);
	bool		IsPointInteriorToLineSegmentXY(Vec2V_In vPoint, Vec2V_In v0, Vec2V_In v1);
	float		InterpTriangle(s32 Base0X, s32 Base0Y, s32 Base1X, s32 Base1Y, s32 Base2X, s32 Base2Y, Vec2V_In vTransformed0, Vec2V_In vTransformed1, Vec2V_In vTransformed2, Vec2V_In r, Vector3* pNormal=NULL, float* pVelZ=NULL);

	bool		GetWaterLevel(const Vector3& vPos, float* pWaterZ, bool bForceResult, float PoolDepth, float RejectionAboveWater, bool *pShouldPlaySeaSounds, const CPhysical* pContextEntity=NULL, bool bHighDetail=false);
	bool		GetWaterLevelNoWaves(const Vector3& vPos, float* pWaterZ, float PoolDepth, float RejectionAboveWater, bool *pShouldPlaySeaSounds, const CPhysical* pContextEntity=NULL);
	bool		TestLineAgainstWater(const Vector3& StartCoors, const Vector3& EndCoors, Vector3 *pPenetrationPoint);

	void		SetUnderwaterRefractionIndex(float index = DEFAULT_REFRACTIONINDEX);
	float		GetUnderwaterRefractionIndex();
	float		GetWaterTimeStepScale();

	void		Update();
	void		UpdateHeightSimulation();
#if GTA_REPLAY
	void		SetReplayPlayBackForNextFrame(s32 bufferIndex);
	bool		ShouldUpdateDuringReplay();
#endif // GTA_REPLAY
	bool		IsHeightSimulationActive();
	void		BlockTillUpdateComplete();
	void		PreRender(const grcViewport &viewport);
	void		ClearRender(s32 list);
	void		Render(u32 pixelsRendered, u32 renderFlags);
#if __BANK
	void		RenderDebugOverlay();
	void		ResetBankVars();
#endif // __BANK

	void		UpdateCachedWorldBaseCoords();

	void		SetupWaterParams();
	void		ProcessWaterPass();
	void		ProcessDepthOccluderPass();
	void		ProcessHeightMapPass();

	void		BeginWaterPass(bool bIsUsingMirrorWaterSurface);
	void		EndWaterPass();

	void		BeginWaterRender();
	void		EndWaterRender();

	void		BeginRiverRender(s32 techniqueGroup, s32 underwaterLowTechniqueGroup, s32 underwaterHighTechniqueGroup, s32 singlePassTechniqueGroup, s32 queryIndex);
	void		EndRiverRender(s32 queryIndex);

	void		BeginFogPrepass(u32 pixels);
	void		EndFogPrepass(float elapsedTime);

	void		RenderForwardIntoRefraction();
	void		RenderRefractionAndFoam();

	grcRenderTarget* GetWaterRefractionColorTarget();
	grcRenderTarget* GetWaterRefractionDepthTarget();

	s32			GetWaterRenderIndex();
	s32			GetWaterUpdateIndex();
	s32			GetWaterCurrentIndex();

	void		GetWaterOcclusionQueryData();
	void		UnflipWaterUpdateIndex();

	void		RenderHeightMap(s32 list, u32 renderFlags);
	void		CausticsPreRender();
	void		RenderWaterCaustics(s32 list);
#if GS_INSTANCED_CUBEMAP
	void		RenderWaterCubeInst();
#endif
	void		RenderWaterCube();
	void		RenderWaterCubeUnderwater();
	void		RenderWaterFLOD();
	void		RenderWaterFLODDisc(s32 pass, float z);
	void		BeginRenderDepthOccluders(s32 list);
	void		EndRenderDepthOccluders();

	void		UpdateDynamicGPU(bool heightSimulationActive);

	void		ResetSimulation();

	struct FoamShaderParameters
	{
		int m_wetmapIndex;
		Vector4 _updateOffset;
		Vector4 _updateParams0;
		const grcTexture* _heightMap;
		s32 cameraMinX;
		s32 cameraMaxX;
		s32 cameraMinY;
		s32 cameraMaxY;
		float m_disturbFoamScale;
		bool isValid;
		atFixedArray<WaterFoam,		MAXFOAM>	m_foamBuffer;
		atFixedArray<WaterDisturb,	MAXDISTURB> m_disturbBuffer;

		FoamShaderParameters()
		{
			Clear();
		};

		void Clear()
		{
			m_wetmapIndex = 0;
			_updateOffset.Zero();
			_updateParams0.Zero();
			_heightMap = grcTexture::NoneBlack;
			cameraMinX = 0;
			cameraMaxX = 0;
			cameraMinY = 0;
			cameraMaxY = 0;
			m_disturbFoamScale = 0.0f;
			isValid = false;
			m_foamBuffer.Reset();
			m_disturbBuffer.Reset();
		}
	};

	struct WaterUpdateShaderParameters
	{
		u32 _bufferIndex;
		float _minX;
		float _maxX;
		float _minY;
		float _maxY;
		Vector3 _centerCoors;
		float _timeStep;
		float _deepScale;
		float _oceanWaveMult;
		float _shoreWaveMult;
		float _timeFactorScale;
		float _timeFactorOffset;
		float _disturbScale;
		float _fTimeMS;
		float _waterRandScaled;
		float _waveLengthScale;
		Vector4 _vUpdateOffset;
		bool _isPaused;
		atFixedArray<WaterDisturb,	MAXDISTURB> _WaterDisturbBuffer;
		bool _resetThisFrame;

		WaterUpdateShaderParameters()
		{
			Clear();
		};

		void Clear()
		{
			_bufferIndex = 0;
			_minX = 0.0f;
			_maxX = 0.0f;
			_minY = 0.0f;
			_maxY = 0.0f;
			_centerCoors = VEC3_ZERO;
			_timeStep = 0.0f;
			_deepScale = 0.0f;
			_oceanWaveMult = 0.0f;
			_shoreWaveMult = 0.0f;
			_timeFactorScale = 0.0f;
			_timeFactorOffset = 0.0f;
			_disturbScale = 0.0f;
			_fTimeMS = 0.0f;
			_waterRandScaled = 0.0f;
			_waveLengthScale = 0.0f;
			_vUpdateOffset = Vector4(0.0f, 0.0f, 0.0f, 0.0f);
			_isPaused = true;
			_WaterDisturbBuffer.Reset();
			_resetThisFrame = false;
		}
	};

	struct WaterBumpShaderParameters
	{
		Vector4 _params[2];
		float _offx;
		float _offy;
		u32 _pixels;
		bool _doubleBufferID, _inUse;

		WaterBumpShaderParameters()
		{
			Clear();
		};

		void Clear()
		{
			_params[0].Zero();
			_params[1].Zero();
			_offx = 0.0f;
			_offy = 0.0f;
			_pixels = 0;
			_doubleBufferID = false;
			_inUse = false;
		}
	};

	void		UpdateGPUVelocityAndHeight(WaterUpdateShaderParameters& params, u32 index);
	void		UpdateWetMapRender(FoamShaderParameters &foamParameters);
	void		UpdateWetMap();

	void		AddToDrawListBoatInteriorsNoSplashes(int drawListType);
	void		AddToDrawListCarInteriorsNoSplashes(int drawListType);

	inline void		SetWaterSurface(grcRenderTarget* pTarget) {sm_pWaterSurface = pTarget;}

	float GetWaterLevel();//This is actually the reflection height
	bool  IsWaterHeightDefault();

	float GetAverageWaterSimHeight();
	float GetCurrentDefaultWaterHeight();

	const CMainWaterTune& GetMainWaterTunings();
	const CSecondaryWaterTune& GetSecondaryWaterTunings();
#if GTA_REPLAY
	const Vector3& GetCurrentWaterPos();
	const float& GetWaterRand();
	float *GetWaterHeightBufferForRecording();
#endif

	void SetSecondaryWaterTunings(const CSecondaryWaterTune& newOne);
	void SetRippleWindValue(float wind);
#if GTA_REPLAY
	void SetCurrentWaterPos(const Vector3& pos);
	void SetCurrentWaterRand(float rand);
#endif

	bool		GetWaterDepth(const Vector3& pos, float *pWaterDepth, float *pWaterZ=NULL, float *pGroundZ=NULL);

	////Water Shader Pass Enums
	enum{
		//fog tile-using techniques
		pass_fogflat,
		pass_fogtess,
		pass_fogtessvsrt,
		pass_flat,
		pass_tess,
		pass_tessvsrt,
		pass_singlepassflat,
		pass_singlepasstess,
		pass_singlepasstessvsrt,
		//non fog tile using techniques start here
		pass_underwaterflat,
		pass_underwatertess,
		pass_underwaterflatlow,
		pass_underwatertesslow,
		pass_clear,
		pass_cleartess,
		pass_height,
		pass_cube,
		pass_cubeunderwater,
#if GS_INSTANCED_CUBEMAP
		pass_cubeinst,
#endif
		pass_flod,
		pass_flodblack,
		pass_invalid,
		pass_count
	};

	// Occlusion Query results
	enum
	{
		query_oceanstart,
		query_ocean0 = query_oceanstart,
		query_ocean1,
		query_ocean2,
		query_ocean3,
		query_oceanend = query_ocean3,
		query_oceanfar,
		query_riverplanar,
		query_riverenv,
		query_count
	};

	u32 GetWaterPixelsRendered(int queryType = -1, bool bIgnoreCameraCuts = false);
	u32 GetWaterPixelsRenderedOcean(); // all oceans
	u32 GetWaterPixelsRenderedPlanarReflective(bool bIgnoreCameraCuts = false); // all oceans plus riverplanar
	float GetOceanHeight(int queryType);
	bool ShouldDrawCaustics();

	inline float getWaterSurfaceSimParam(int i){return m_waterSurfaceTextureSimParams[i];}

#if __BANK || (__WIN32PC && !__FINAL)
	class CWaterPoly
	{
	public:
		Vec3V	m_points[5];
		int		m_numPoints;
	};

	void GetWaterPolys(atArray<CWaterPoly>& polys);
#endif // __BANK || (__WIN32PC && !__FINAL)

#if __BANK
	void UpdateShaderEditVars();
	void MakeWaterFlat();

	void InitOptimizationWidgets();
	void InitWidgets();
	bool& GetRasterizeWaterQuadsIntoStencilRef();

	float GetWaterTestOffset();

	float FindNearestWaterHeightFromQuads(float TestX, float TestY);

	void RenderDebug(Color32 waterColor, Color32 calmingColor, Color32 waveColor, bool bRenderSolid, bool bRenderCutTriangles, bool bRenderVectorMap);
	void RenderDebugToEPS(const char* name);
	void PreRenderVisibility_Debug(spdkDOP2D_8& bounds, const grcViewport& viewport, bool bUseOcclusion, bool BANK_ONLY(bDrawPoints));
#endif // __BANK
#endif //!__SPU...

	void SetWorldBase();

	void SetWaterRenderPhase(const CRenderPhaseScanned* renderPhase);
	const CRenderPhaseScanned* GetWaterRenderPhase();
	void GetCameraRangeForRender();

#if RSG_PC
	void	DepthUpdated(bool flag);
	inline bool	IsDepthUpdated()			{ return m_bDepthUpdated; }
#endif

#if __WIN32PC && __D3D11
	static WaterUpdateShaderParameters* s_GPUWaterShaderParams;
#endif

	float GetCameraWaterHeight();
	float GetCameraWaterDepth();
	float GetWaterOpacityUnderwaterVfx();
	bool IsCameraUnderwater();

	bool WaterEnabled();
	bool UseFogPrepass();
	bool UseHQWaterRendering();
	bool UsePlanarReflection();

	float*	GetHeightMapData();
	float*	GetVelocityMapData();
	void	DoReplayWaterSimulate(bool b);

	enum
	{
		WATERXML_V_DEFAULT		= 0,	// default V main map
		WATERXML_ISLANDHEIST	= 1,	// Cayo Perico DLC
		WATERXML_INVALID		= -1	// invalid
	};

	void	RequestGlobalWaterXmlFile(u32 file);
	void	LoadGlobalWaterXmlFile(u32 file);
	u32		GetGlobalWaterXmlFile();
};

// Custom draw list commands
#if !__SPU
class dlCmdWaterRender : public dlCmdBase {
public:
	enum {
		INSTRUCTION_ID = DC_WaterRender,
	};

	dlCmdWaterRender(u32 pixelsRendered, u32 renderFlags)
		: m_PixelsRendered(pixelsRendered)
		, m_RenderFlags(renderFlags)
	{
	}

	s32 GetCommandSize()						{ return(sizeof(*this)); }
	void Execute()								{ Water::Render(m_PixelsRendered, m_RenderFlags); }
	static void ExecuteStatic(dlCmdBase & cmd)	{ ((dlCmdWaterRender &) cmd).Execute(); }

private:
	u32 m_PixelsRendered;
	u32 m_RenderFlags;
};
#endif // !__SPU



#if __PS3
	#define DMA_HEIGHT_BUFFER_WITH_JOB 1
#else
	#define DMA_HEIGHT_BUFFER_WITH_JOB 0
#endif


#endif //_WATER_H_...
