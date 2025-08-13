///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	Coronas.h
//	BY	: 	Mark Nicholson (Adapted from original by Obbe)
//	FOR	:	Rockstar North
//	ON	:	06 Aug 2008
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

#ifndef CORONAS_H
#define CORONAS_H


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////													

#if !__SPU
// rage
#include "vector/color32.h"

// game
#include "renderer/rendertargets.h"
#include "shaders/ShaderLib.h"
#include "vfx/VisualEffects.h"
#include "vfx/vfx_shared.h"
#endif

#include "math/float16.h"

///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////													

// ====================== Platform Dependent =========================
//	Set the maximum number of coronas
#if RSG_PC || RSG_ORBIS || RSG_DURANGO
	#define CORONAS_MAX							(1088)
#else
	#define CORONAS_MAX							(900)
#endif
// ====================== Platform Dependent =========================
#define CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_INNER	(0.0f)
#define CORONAS_DEF_DIR_LIGHT_CONE_ANGLE_OUTER	(90.0f)
#define CORONAS_MAX_DIR_LIGHT_CONE_ANGLE_OUTER	(90.0f)
#define CORONAS_MAX_DIR_LIGHT_CONE_ANGLE_INNER	(90.0f)
#define LATE_CORONAS_MAX						(8)

#define CORONAS_DIR_CONE_ANGLE_OUTER_MASK		0x00FF0000
#define CORONAS_DIR_CONE_ANGLE_INNER_MASK		0xFF000000
#define CORONAS_DIR_CONE_ANGLE_OUTER_SHIFT		16
#define CORONAS_DIR_CONE_ANGLE_INNER_SHIFT		24

///////////////////////////////////////////////////////////////////////////////
//  FORWARD DECLARATIONS
///////////////////////////////////////////////////////////////////////////////	

namespace rage
{
	class grcTexture;
};

class CEntity;
class C2dEffect;


///////////////////////////////////////////////////////////////////////////////
//  STRUCTURES
///////////////////////////////////////////////////////////////////////////////	

enum eCoronaFlags
{
	CORONA_DONT_REFLECT				= (1 << 0),
	CORONA_HAS_DIRECTION			= (1 << 1),
	CORONA_DONT_RENDER_UNDERWATER	= (1 << 2),
	CORONA_ONLY_IN_REFLECTION		= (1 << 3),
	CORONA_DONT_RENDER_FLARE		= (1 << 4),
	CORONA_FADEOFF_DIR_MULT			= (1 << 5),
	CORONA_FROM_LOD_LIGHT			= (1 << 6),
	CORONA_DONT_ROTATE				= (1 << 7), // Don`t apply double rotating corona rendering.
	CORONA_DONT_RAMP_UP				= (1 << 8), // Don`t apply ramping up when looking directly at light source.
	CORONA_FLAGS_MASK				= 0xffff
};

struct Corona_t
{
private:
	Vec3V		vPos; // .w: 16 bits reserved for flags, 8 bits for light cone angle inner, 8 bits for light cone angle outer
	Vec4V		vDirAndViewThreshold; // Direction in .xyz, View Threshold in .w
	float		size;
	Color32		col;
	float		intensity;
	float		zBias;
public:
	
	void SetPosition						(Vec3V_In pos)						{ vPos = pos; }
	void SetDirectionViewThreshold			(Vec4V_In vDirThreshold)			{ vDirAndViewThreshold = vDirThreshold; }	
	void SetSize							(float t_size)						{ size = t_size; }
	void SetIntensity						(float t_intensity)					{ intensity = t_intensity; }
	void SetZBias							(float t_zBias)						{ zBias = t_zBias; }
	void SetColor							(Color32 t_col)						{ col = t_col; }


	Vec3V_Out GetPosition					() const							{ return vPos; }
	Vec3V_Out GetDirection					() const							{ return vDirAndViewThreshold.GetXYZ(); }
	float GetDirectionThreshold				() const							{ return vDirAndViewThreshold.GetWf(); }
	void SetCoronaFlagsAndDirLightConeAngles(u16 flag, 
											 u8 dirLightConeAngleIn, u8 dirLightConeAngleOut)	
																				{ vPos.SetWi( ((dirLightConeAngleOut<<CORONAS_DIR_CONE_ANGLE_OUTER_SHIFT) | (dirLightConeAngleIn<<CORONAS_DIR_CONE_ANGLE_INNER_SHIFT) |  flag) ); }
	float GetDirLightConeAngleOuter			() const							{ return (float)((vPos.GetWi()>>CORONAS_DIR_CONE_ANGLE_OUTER_SHIFT)&0xff); }
	float GetDirLightConeAngleInner			() const							{ return (float)((vPos.GetWi()>>CORONAS_DIR_CONE_ANGLE_INNER_SHIFT)&0xff); }

	bool IsFlagSet							(u16 flag) const					{ return ((u16)(vPos.GetWi()&CORONA_FLAGS_MASK) & flag) != 0; }
	u16 GetFlags							() const							{ return (u16)(vPos.GetWi()&CORONA_FLAGS_MASK);}

	float GetSize							() const							{ return size; }
	float GetIntensity						() const							{ return intensity; }
	float GetZBias							() const							{ return zBias; }
	Color32 GetColor						() const							{ return col; }
	
} ;


struct CoronaRenderProperties_t
{
	Vec3V vPos;
	float size;
	float intensity;
	u8 bRenderFlare;
	u8 bDontRotate;
	u16 registerIndex;
	float rotationBlend;
};

struct CoronaRenderInfos_t
{
	Corona_t *coronas;
	int numCoronas;
	int lateCoronas;
	float rotation[2];
};
#if !__SPU

///////////////////////////////////////////////////////////////////////////////
//  CLASSES
///////////////////////////////////////////////////////////////////////////////	

class CCoronas
{	
	public: ///////////////////////////

	///////////////////////////////////
	// FUNCTIONS 
	///////////////////////////////////


		void 				Init						(unsigned initMode);
		void 				Shutdown					(unsigned shutdownMode);

		void 				Render						(CVisualEffects::eRenderMode mode, float sizeScale, float intensityScale);
		void				UpdateAfterRender			();

		static void			InitDLCCommands				();
		void				SetupRenderthreadFrameInfo	();
		void				ClearRenderthreadFrameInfo	();
		// dirViewThreshold controls broadness of the directional falloff 
		// (0=tight angle falloff. 1=broad 180 degree falloff which is equivalent to the 'old' directional coronas)
		void 				Register					(Vec3V_In vPos,
														 const float size,
														 const Color32 col,
														 const float intensity,
														 const float zBias,
														 Vec3V_In   vDir,
														 const float dirViewThreshold,
														 const float dirLightConeAngleInner,
														 const float dirLightConeAngleOuter,
														 const u16 coronaFlags);
		bool 				LateRegister				(Vec3V_In vPos,
														 const float size,
														 const Color32 col,
														 const float intensity,
														 const float zBias,
														 Vec3V_In   vDir,
														 const float dirViewThreshold,
														 const float dirLightConeAngleInner,
														 const float dirLightConeAngleOuter,
														 const u16 coronaFlags);

		Corona_t*			GetCoronasUpdateBuffer		()						{ return &m_CoronasUpdateBuffer[0]; }
		int					GetNumCoronasUpdate			();
		int					GetNumCoronasRender			();
		void				GetDataForAsyncJob			(Corona_t*& pCoronaArray,
														 u32*& pCount
														 BANK_ONLY(, u32*& pOverflown, float& zBiasScale));

		float				GetOcclusionSizeScale		()						{ return m_occlusionSizeScale; }
		float				GetOcclusionSizeMax			()						{ return m_occlusionSizeMax; }
		float				GetRotationSlow				()						{ return m_rotation[0]; }
		float				GetRotationFast				()						{ return m_rotation[1]; }

		float				GetBaseValueR				()						{ return m_baseValueR; }
		float				GetBaseValueG				()						{ return m_baseValueG; }
		float				GetBaseValueB				()						{ return m_baseValueB; }

		float				GetFlareColShiftR			()						{ return m_flareColShiftR; }
		float				GetFlareColShiftG			()						{ return m_flareColShiftG; }
		float				GetFlareColShiftB			()						{ return m_flareColShiftB; }

		float				GetFlareCoronaSizeRatio		()						{ return m_flareCoronaSizeRatio; }
		float				GetFlareMinAngleDegToFadeIn	()						{ return m_flareMinAngleDegToFadeIn; }
		float				GetFlareScreenCenterOffsetSizeMult()				{ return m_flareScreenCenterOffsetSizeMult; }
		float				GetFlareScreenCenterOffsetVerticalSizeMult()		{ return m_flareScreenCenterOffsetVerticalSizeMult; }

		grcTexture*			GetCoronaDiffuseTexture			()					{ return m_pTexCoronaDiffuse; }
		grcTexture*			GetAnimorphicFlareDiffuseTexture()					{ return m_pTexAnimorphicFlareDiffuse; }

inline	float				ZBiasClamp					(float zBias)			{	return Clamp(zBias, m_zBiasMin, m_zBiasMax); }

inline	float				GetZBiasFromFinalSizeMultiplier() const				{ return m_zBiasFromFinalSizeMult; }

static	void				UpdateVisualDataSettings	();

inline	float				GetDirMultFadeOffStart		()						{ return m_dirMultFadeOffStart; }
inline	float				GetDirMultFadeOffDist		()						{ return m_dirMultFadeOffDist; }

#if __BANK
		void				InitWidgets					();
#endif

		static CoronaRenderInfos_t				m_CoronasRenderBuffer;

	private: //////////////////////////

		struct CoronaRenderInfo
		{
			Vec3V						vCamPos;
			Vec3V						vCamRight;
			Vec3V						vCamUp;
			Vec3V						vCamDir;
			float						screenspaceExpansion;
			float						screenspaceExpansionWater;
			float						zBiasMultiplier;
			float						zBiasDistanceNear;
			float						zBiasDistanceFar;
			float						camScreenToWorld;
			float						tanHFov;
			float						tanVFov;
			CVisualEffects::eRenderMode	mode;
		};

		void		 		LoadTextures				(s32 txdSlot);

		void				ComputeFinalProperties		(const CoronaRenderInfo& renderInfo, CoronaRenderProperties_t* coronaRenderPropeties, float sizeScale, float intensityScale, s32& numToRender, s32& numFlaresToRender, s32 numCoronas);
		float				ComputeAdditionalZBias		(const CoronaRenderInfo& renderInfo);

		bool				RenderCorona				(int coronaIndex, CVisualEffects::eRenderMode mode, CoronaRenderInfo &renderInfo, CoronaRenderProperties_t *coronaRenderProperties, float sign, float layerMultipier);
		void				AddCoronaVerts				(Vec3V_In vPos, Vec3V_In vRight_In, Vec3V_In vUp_In, Color32	col, float intensity);
		void				RenderFlares				(const CoronaRenderInfo& renderInfo, CoronaRenderProperties_t* coronaIntensities, s32 numCoronas, s32 numToRender);
#if GLINTS_ENABLED
		void				SetupGlints();
		void				RenderGlints();
#endif 

#if __BANK
static	void				DebugAdd					();
static	void				DebugRemove					();

// Start/end corona overflow test
static void DebugOverflowStart();
static void DebugOverflowEnd();
#endif


	///////////////////////////////////
	// VARIABLES
	///////////////////////////////////

	private: //////////////////////////

		Corona_t			m_CoronasUpdateBuffer		[CORONAS_MAX];
		u32					m_numCoronas;

		grcTexture*			m_pTexCoronaDiffuse;	
		grcTexture*			m_pTexAnimorphicFlareDiffuse;

		// visual settings
		float				m_sizeScaleGlobal;
		float				m_intensityScaleGlobal;
		float				m_intensityScaleWater;
		float				m_sizeScaleWater;
		float				m_sizeScaleParaboloid;
		float				m_screenspaceExpansion;
		float				m_screenspaceExpansionWater;
		float				m_zBiasMultiplier;
		float				m_zBiasDistanceNear;
		float				m_zBiasDistanceFar;
		float				m_zBiasMin;
		float				m_zBiasMax;
		float				m_zBiasAdditionalBiasMirrorReflection;
		float				m_zBiasAdditionalBiasEnvReflection;
		float				m_zBiasAdditionalBiasWaterReflection;
		float				m_occlusionSizeScale;
		float				m_occlusionSizeMax;
		float				m_doNGCoronas;
		float				m_rotation[2];
		float				m_rotationSpeed[2];
		float				m_layer1Muliplier;
		float				m_layer2Muliplier;
		float				m_scaleRampUp;
		float				m_scaleRampUpAngle;

		float				m_flareCoronaSizeRatio;
		float				m_flareMinAngleDegToFadeIn;
		float				m_flareScreenCenterOffsetSizeMult;
		float				m_flareScreenCenterOffsetVerticalSizeMult;
		float				m_flareColShiftR;
		float				m_flareColShiftG;
		float				m_flareColShiftB;
		float				m_baseValueR;
		float				m_baseValueG;
		float				m_baseValueB;

		float				m_dirMultFadeOffStart;
		float				m_dirMultFadeOffDist;

		float				m_screenEdgeMinDistForFade;

		float				m_underwaterFadeDist;

		float				m_zBiasFromFinalSizeMult;
#if GLINTS_ENABLED
		grcBufferUAV			m_GlintAccumBuffer;
		grcBufferBasic			m_GlintRenderBuffer;
		grcEffectVar			m_GlintBackbufferPointVar;
		grcEffectVar			m_GlintBackbufferLinearVar;
		grcEffectVar			m_GlintPointBufferVar;
		grcEffectVar			m_GlintParamsVar0;
		grcEffectVar			m_GlintParamsVar1;
		grcEffectVar			m_GlintDepthVar;
		grcEffectTechnique		m_GlintTechnique;
		float					m_GlintBrightnessThreshold;
		float					m_GlintSizeX;
		float					m_GlintSizeY;
		float					m_GlintAlphaScale;
		float					m_GlintDepthScaleRange;
		bool					m_GlintsEnabled;
#endif //GLINTS_ENABLED

#if __BANK
		s32					m_numRegistered;
		s32					m_numRendered;
		s32					m_numFlaresRendered;
		u32					m_numOverflown;
		bool				m_hasOverflown;

		bool				m_disableRender;
		bool				m_renderDebug;
		bool				m_debugCoronaActive;
		bool				m_disableRenderFlares;
		bool				m_disableLateRegistration;
		bool				m_overflowCoronas;

		Vec3V				m_vDebugCoronaPos;
		float				m_debugCoronaSize;
		s32					m_debugCoronaColR;
		s32					m_debugCoronaColG;
		s32					m_debugCoronaColB;
		float				m_debugCoronaIntensity;
		bool				m_debugCoronaDir;

		bool				m_debugCoronaForceRenderSimple; // force simple rendering (no depth buffer read)
		bool				m_debugCoronaForceReflection; // force coronas in reflection phases even if dontReflect is true
		bool				m_debugCoronaForceOmni; // force coronas to ignore direction when computing final brightness
		bool				m_debugCoronaForceExterior;
		float				m_debugCoronaZBiasScale;

		float				m_debugDirLightConeAngleInner;
		float				m_debugDirLightConeAngleOuter;
		bool				m_debugOverrideDirLightConeAngle;
		bool				m_debugApplyNewColourShift;

	public:
		bool GetForceExterior() const { return m_debugCoronaForceExterior; }
		float& GetSizeScaleGlobalRef() { return m_sizeScaleGlobal; } 
#endif
};


///////////////////////////////////////////////////////////////////////////////
//  EXTERNS
///////////////////////////////////////////////////////////////////////////////

extern CCoronas g_coronas;

#endif // !__SPU

#endif // CORONAS_H
