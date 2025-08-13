//
// filename:	ShaderLib.h
// description:	Class controlling loading of shaders
// written by:	Adam Fowler
//
#ifndef INC_SHADER_LIB_H_
#define	INC_SHADER_LIB_H_

#include "grcore/stateblock.h"
#include "grmodel/ShaderFx.h"
#include "renderer/DrawLists/drawListMgr.h"
#include "renderer/Deferred/GBuffer.h"
#include "vectormath/threadvector.h"
#include "diag/trap.h"
#include "vfx/vfx_shared.h"

#define GTASHADERVARBOOL(X)				(bool(X))


// materialID: bits 0-2:
#define DEFERRED_MATERIAL_DEFAULT		(0)
#define DEFERRED_MATERIAL_PED			(1)
#define DEFERRED_MATERIAL_VEHICLE		(2)	
#define DEFERRED_MATERIAL_TREE			(3)
#define DEFERRED_MATERIAL_TERRAIN		(4)

#if __BANK
#define DEFERRED_MATERIAL_SELECTED		(5) // Selected in picker
#define DEFERRED_MATERIAL_WATER_DEBUG	(6) // Water mask
#endif // __BANK

#define DEFERRED_MATERIAL_CLEAR			(7) 

#define DEFERRED_MATERIAL_TYPE_MASK		(0x7)

// materialID: bits 3-7:
#define DEFERRED_MATERIAL_INTERIOR		(8)		// anything can be OR'd with interior material
#define DEFERRED_MATERIAL_SPECIALBIT	(16)	// general purpose bit for marking pre-pass areas
#define DEFERRED_MATERIAL_SPAREOR1		(32)	// used for edge flag up until the directional pass, then for puddles and scene lights
#define DEFERRED_MATERIAL_SPAREOR2		(64)
#define DEFERRED_MATERIAL_SPAREMASK		(96)	// DEFERRED_MATERIAL_SPAREOR1 | DEFERRED_MATERIAL_SPAREOR2
#define DEFERRED_MATERIAL_MOTIONBLUR	(128)	// anything can be OR'd with motion blur flag

// materialID bits
#define DEFERRED_MATERIAL_ALL			(0xff)

#define SHADERLIB_USE_MSAA_PARAMS		( 1 && (__D3D11 || RSG_ORBIS) )

#define DEFERRED_MATERIAL_VEHICLE_BADGE_LOCAL_PLAYER		(DEFERRED_MATERIAL_SPAREOR1)

// MSAA_EDGE_PASS
enum { EDGE_FLAG = DEFERRED_MATERIAL_SPAREOR1 };

//
//
//
//
class CShaderLib
{
public:
	class CFogParams
	{
	public:
		CFogParams()
		{
			vFogParams[0] = VEC4V_TO_VECTOR4(Vec4V(V_Y_AXIS_WZERO));
			vFogParams[1] = VEC4V_TO_VECTOR4(Vec4V(V_ZERO));
			vFogParams[2] = VEC4V_TO_VECTOR4(Vec4V(1.0f, 0.0f, 1.0f, 0.0f));
			vSunDirAndPower = VEC4V_TO_VECTOR4(Vec4V(V_ZERO_WONE));
			vMoonDirAndPower = VEC4V_TO_VECTOR4(Vec4V(V_ZERO_WONE));
		}
		ThreadVector4 vFogParams[3];
		ThreadVector4 vSunDirAndPower;
		ThreadVector4 vMoonDirAndPower;
	};
public:
	// Perform the bare minimum of setup that's needed to print some text and render the intro movie.
	// This is done to avoid a long pause before rendering when starting the game
	// Init() should be called after we start rendering.
	static void PreInit();
	static void Init();

	static void Shutdown();	

	static void LoadInitialShaders();
	static void LoadAllShaders();

	static void InitTxd(unsigned initmode);
	static void ShutdownTxd(unsigned shutdowmode);

#if __BANK
	static void InitWidgets(Static0 shaderReloadCB = grmShader::ReloadAll);
#endif

	static void SetGlobals(u8 natAmb, u8 artAmb, bool setAlpha, u32 alpha, bool setStipple, bool bInvertStipple, u32 stippleAlpha, bool bDontWriteDepth, bool bDontReadDepth, bool bDontWriteColor);
	static void SetScreenDoorAlpha(bool setAlpha, u32 alpha, bool setStipple, bool bInvertStipple, u32 stippleAlpha, bool bDontWriteDepth);
	static void ResetAlpha(bool resetAlpha = true, bool resetStipple = true);

	static u16 GetStippleMask(u32 stippleAlpha, bool bInvertStipple);	//Gets the stipple mask. Used as a sort parameter by the instanced renderer.
	static void SetStippleAlpha(u32 stippleAlpha, bool bInvertStipple);	//Just sets stipple alpha state... used by instancing since globals are already set.
	static bool UsesStippleFades();

	static float DivideBy255(u32 idx) { TrapGE(idx,256U); return ms_divideBy255[idx]; }
	static u64	ComputeSampleMask(u32 stippleAlpha);

	static void SetGlobalAlpha(float alpha);

	static void SetAlphaTestRef(float alphaTestRef);
	static float GetAlphaTestRef();

	static void SetGlobalEmissiveScale(float hdrIntensity, bool ignoreDisable = false);
	static float GetGlobalEmissiveScale();

	static void SetGlobalArtificialAmbientScale(float ambScale);
	static float GetGlobalArtificialAmbientScale();

	static void SetGlobalNaturalAmbientScale(float ambScale);
	static float GetGlobalNaturalAmbientScale();

	static void SetGlobalAmbientOcclusionEffect(const Vector4 &ambEffect);
	static void SetGlobalPedRimLightScale(float rimScale);

	static void SetGlobalTimer(float time);
	
	static void SetGlobalInInterior(bool inInterior);
	static bool GetGlobalInInterior();

	static void SetGlobalToneMapScalers(const Vector4& scalers);
	static Vector4 GetGlobalToneMapScalers();

	static void SetGlobalScreenSize(const Vector2& screenSize);
#if SHADERLIB_USE_MSAA_PARAMS
	static void SetMSAAParams(const grcDevice::MSAAMode &mode, bool colorDecoding);
#endif // USE_MSAA_PARAMSF
	static void SetTreesParams();
	static void SetUseFogRay(bool bUse);

	static void UpdateGlobalDevScreenSize();
	static void UpdateGlobalGameScreenSize();

	static void SetGlobalFogParams(const CFogParams &fogParams);
	static void	ResendCurrentFogSettings();
	static void SetGlobalFogColors(Vec3V_In vFogColorSun, Vec3V_In vFogColorAtmosphere, Vec3V_In vFogColorGround, Vec3V_In vFogColorHaze, Vec3V_In vFogColorMoon);
#if LINEAR_PIECEWISE_FOG
	static void SetLinearPiecewiseFogParams(const class LinearPiecewiseFog_ShaderParams &fogParams);
#endif // LINEAR_PIECEWISE_FOG

	// Sets a constant to whether to flip the normals to face forward
	static void SetFacingBackwards( bool facingBackwards);
	static void SetGlobalReflectionCamDirAndEmissiveMult(const Vector3 &dir, const float emissiveMult);
	static void ApplyGlobalReflectionTweaks();

	static void SetGlobalDynamicBakeBoostAndWetness(Vec4V_In bakeParams);

	static void DisableGlobalFogAndDepthFx();
	static void EnableGlobalFogAndDepthFx(); //using current fog and depthfx params

	static void	ApplyDayNightBalance();
	static float GetDayNightBalance();

	static void SetGlobalPedAOFixEnable(bool b);
	static bool GetGlobalPedAOFixEnable();

	static void SetMotionBlurMask(bool motion);
	static bool GetMotionBlurMask();

	static void BeginMotionBlurMask();
	static void EndMotionBlurMask();

	static void SetDeferredWetBlendEnable(bool b);
	static bool GetDeferredWetBlendEnable();

	static void		SetGlobalDeferredMaterial(u32 material, bool forceSetMaterial = false MSAA_EDGE_PROCESS_FADING_ONLY(, bool reset = false));

	static u32		GetGlobalDeferredMaterial();
	static void		SetGlobalDeferredMaterialBit(u32 bit, bool forceSetMaterial = false);
	static void		ClearGlobalDeferredMaterialBit(u32 bit, bool forceSetMaterial = false);

	static void		ResetStippleAlpha(void) 
	{
		// State block path, set the current BS block with a new reference.
		grcStateBlock::SetBlendState(grcStateBlock::BS_Active, grcStateBlock::ActiveBlendFactors, ~0ULL);
	}

	static void ForceScalarGlobals2(const Vector4& vec);
	static void ResetScalarGlobals2();
	static void SetAlphaRefConsts();
	static void ResetAllVar();

	static void Update();
	static void SetGlobalShaderVariables();

#if RSG_PC
	static void SetReticuleDistTexture(bool bDist = true);
	static void SetStereoTexture();
	static void SetStereoParams(const Vector4& vec);
	static void SetStereoParams1(const Vector4& vec);
#endif	

	static void SetFogRayTexture();
	static void SetFogRayIntensity(float fIntensity);

//#if MSAA_EDGE_PASS
	static void SetForcedEdge(bool bEdge);
//#endif

	template<class T_arg1> static void SetGlobalVarInContext(grcEffectGlobalVar globalVar, const T_arg1& arg1){

		//if (gDrawListMgr->ms_drawListSwitch == 0){
		if( !gDrawListMgr->IsBuildingDrawList() )
		{
			//execute immediately if called from a render thread
			grcEffect::SetGlobalVar(globalVar, arg1);
		} 
		else {
			//place in current drawlist otherwise
			DLC_SET_GLOBAL_VAR(globalVar, arg1);
		}
	}

	template<class T_arg1> static void SetGlobalVarInContext(grcEffectGlobalVar globalVar, const T_arg1* arg1, s32 count){

		//if (gDrawListMgr->ms_drawListSwitch == 0){
		if( !gDrawListMgr->IsBuildingDrawList() )
		{
			//execute immediately if called from a render thread
			grcEffect::SetGlobalVar(globalVar, arg1, count);
		} 
		else {
			//place in current drawlist otherwise
			DLC_SET_GLOBAL_VAR_ARRAY(globalVar, arg1, count);
		}
	}
	
	// Global texture dictionary
	static int GetTxdId() { return ms_txdId.Get(); }
	static grcTexture *LookupTexture(const char* name) { return LookupTexture(fwTxd::ComputeHash(name)); }
	static grcTexture *LookupTexture(u32 hash) { return g_TxdStore.Get(ms_txdId)->Lookup(hash); }
	
	// state blocks
	static void SetDefaultDepthStencilBlock() { grcStateBlock::SetDepthStencilState(ms_gDefaultState_DS, ms_gDeferredMaterial[g_RenderThreadIndex]); };
	static void SetFrontFaceCulling() { grcStateBlock::SetRasterizerState(ms_frontFaceCulling_RS); };
	static void SetDisableFaceCulling() { grcStateBlock::SetRasterizerState(ms_disableFaceCulling_RS); };
	static void ResetBlendState();
	static float GetOverridesEmissiveScale() { return ms_overridesEmissiveScale; }
	static float GetOverridesEmissiveMult() { return ms_overridesEmissiveMult; }
	static float GetOverridesArtIntAmbDown() { return ms_overridesArtIntAmbDown; }
	static float GetOverridesArtIntAmbBase() { return ms_overridesArtIntAmbBase; }
	static float GetOverridesArtExtAmbDown() { return ms_overridesArtExtAmbDown; }
	static float GetOverridesArtExtAmbBase() { return ms_overridesArtExtAmbBase; }
	static float GetOverridesArtAmbScale() { return ms_overridesArtAmbScale ; }

	static grcDepthStencilStateHandle DSS_Default_Invert;
	static grcDepthStencilStateHandle DSS_LessEqual_Invert;
	static grcDepthStencilStateHandle DSS_TestOnly_Invert;
	static grcDepthStencilStateHandle DSS_TestOnly_LessEqual_Invert;

	static grcBlendStateHandle BS_NoColorWrite;

	#if !RSG_ORBIS && !RSG_DURANGO
		static u16	GetFadeMask16() 			{ return ms_FadeMask16; }
	#endif

private:
	//This function is used for keeping the integer and float global scalars in sync
	static inline void SetScalarGlobalConstant(int index, int iValue, float fValue)
	{
		ms_giCurrentScalarGlobals[g_RenderThreadIndex][index] = iValue;
		ms_gCurrentScalarGlobals[g_RenderThreadIndex][index] = fValue; 
	
		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_giCurrentScalarGlobals[i][index] = iValue;
				ms_gCurrentScalarGlobals[i][index] = fValue;
			}
		}
	}

	static inline void SetScalarGlobalConstant2(int index, float fValue)
	{
		ms_gCurrentScalarGlobals2[g_RenderThreadIndex][index] = fValue; 
	
		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gCurrentScalarGlobals2[i][index] = fValue;
			}
		}
	}

	static inline void SetScalarGlobalConstant3(const Vector4& v)
	{
		ms_gCurrentScalarGlobals3[g_RenderThreadIndex] = v; 
	
		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gCurrentScalarGlobals3[i] = v;
			}
		}
	}

	static inline void SetCurrentDirectionalAmbient(const Vector4& dirAmb)
	{
		ms_gCurrentDirectionalAmbient[g_RenderThreadIndex] = dirAmb; 
	
		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
					ms_gCurrentDirectionalAmbient[i] = dirAmb;
			}
		}
	}

	static inline void SetCurrentScreenSize(const Vector2& screenSize)
	{
		const float W = screenSize.x;
		const float H = screenSize.y;
		const float invW = 1.0f/screenSize.x;
		const float invH = 1.0f/screenSize.y;
		ms_gCurrentScreenSize[g_RenderThreadIndex].x = W;;
		ms_gCurrentScreenSize[g_RenderThreadIndex].y = H;
		ms_gCurrentScreenSize[g_RenderThreadIndex].z = invW;
		ms_gCurrentScreenSize[g_RenderThreadIndex].w = invH;

		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gCurrentScreenSize[i].x = W;
				ms_gCurrentScreenSize[i].y = H;
				ms_gCurrentScreenSize[i].z = invW;
				ms_gCurrentScreenSize[i].w = invH;
			}
		}
	}

	static inline void SetCurrentAOEffectScale(const Vector4& ambEffect)
	{
		ms_gCurrentAOEffectScale[g_RenderThreadIndex] = ambEffect;

		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gCurrentAOEffectScale[i] = ambEffect;
			}
		}
	}

	static inline void SetCurrentPlayerLFootPos(const Vector4& LFootPos)
	{
		ms_gCurrentPlayerLFootPos[g_RenderThreadIndex] = LFootPos;
		
		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gCurrentPlayerLFootPos[i] = LFootPos;
			}
		}
	}

	static inline void SetCurrentPlayerRFootPos(const Vector4& RFootPos)
	{
		ms_gCurrentPlayerRFootPos[g_RenderThreadIndex] = RFootPos;
		
		if(!g_IsSubRenderThread)
		{
			for(int i=0; i<NUMBER_OF_RENDER_THREADS; i++)
			{
				ms_gCurrentPlayerRFootPos[i] = RFootPos;
			}
		}
	}

	static void SetPlayerFeetPos(const Vector4& LFootPos, const Vector4& RFootPos);

private:
	static u16 ms_aaMaskLut[32];
	static u16 ms_aaMaskFadeLut[32];
	static float ms_divideBy255[256];

	static ThreadVector4	ms_gCurrentScalarGlobals[NUMBER_OF_RENDER_THREADS];
	static u32				ms_giCurrentScalarGlobals[NUMBER_OF_RENDER_THREADS][4];			//Used for avoiding integer->float conversions for condition checking. Both should remain in sync
	static ThreadVector4	ms_gCurrentScalarGlobals2[NUMBER_OF_RENDER_THREADS];
	static ThreadVector4	ms_gCurrentScalarGlobals3[NUMBER_OF_RENDER_THREADS];
	static ThreadVector4	ms_gCurrentDirectionalAmbient[NUMBER_OF_RENDER_THREADS];

	static u32				ms_gDeferredMaterial[NUMBER_OF_RENDER_THREADS];
	static u32				ms_gDeferredMaterialORMask[NUMBER_OF_RENDER_THREADS];

	static bool				ms_fogDepthFxEnabled[NUMBER_OF_RENDER_THREADS];
	static CFogParams		ms_globalFogParams[NUMBER_OF_RENDER_THREADS];

#if LINEAR_PIECEWISE_FOG
	static ThreadVector4	ms_linearPiecewiseFog[NUMBER_OF_RENDER_THREADS][LINEAR_PIECEWISE_FOG_NUM_SHADER_VARIABLES];
#endif // LINEAR_PIECEWISE_FOG
	static bool				ms_facingBackwards[NUMBER_OF_RENDER_THREADS];
	static ThreadVector4	ms_gReflectionTweaks;	

	static ThreadVector4	ms_gCurrentScreenSize[NUMBER_OF_RENDER_THREADS];
	static ThreadVector4	ms_gCurrentAOEffectScale[NUMBER_OF_RENDER_THREADS];
	static bool				ms_gDeferredWetBlend[NUMBER_OF_RENDER_THREADS];
	static bool				ms_gMotionBlurMask;			// used as global switch flag only

	static float			ms_alphaTestRef[NUMBER_OF_RENDER_THREADS];

	static Vector4			ms_PlayerLFootPos[2];
	static Vector4			ms_PlayerRFootPos[2];
	static ThreadVector4	ms_gCurrentPlayerLFootPos[NUMBER_OF_RENDER_THREADS];
	static ThreadVector4	ms_gCurrentPlayerRFootPos[NUMBER_OF_RENDER_THREADS];
#if RSG_PC
	static float			ms_ShaderQuality[2];
#endif

	static grcEffectGlobalVar ms_globalScalarsID;
	static grcEffectGlobalVar ms_globalScalars2ID;
	static grcEffectGlobalVar ms_globalScalars3ID;
#if !RSG_ORBIS && !RSG_DURANGO
	static grcEffectGlobalVar ms_globalFadeID;
	static u16				  ms_FadeMask16;	
#endif // !RSG_ORBIS && !RSG_DURANGO
	static strLocalIndex ms_txdId;

	static grcDepthStencilStateHandle ms_gDefaultState_DS;
	static grcDepthStencilStateHandle ms_gNoDepthWrite_DS;
	static grcDepthStencilStateHandle ms_gNoDepthRead_DS;

	static grcRasterizerStateHandle ms_frontFaceCulling_RS;
	static grcRasterizerStateHandle ms_disableFaceCulling_RS;

	static bool ms_ActivatedNV;
#if __BANK
public:
#endif
	static float ms_overridesEmissiveScale;
	static float ms_overridesEmissiveMult;
	static float ms_overridesArtIntAmbDown;
	static float ms_overridesArtIntAmbBase;
	static float ms_overridesArtExtAmbDown;
	static float ms_overridesArtExtAmbBase;
	static float ms_overridesArtAmbScale;

#if __DEV
public:
	static bool  ms_gReflectTweakEnabled;
	static float ms_gReflectTweak;
	static bool  ms_gDisableShaderVarCaching;
#endif // __DEV
	
};

#endif //INC_SHADER_LIB_H_
