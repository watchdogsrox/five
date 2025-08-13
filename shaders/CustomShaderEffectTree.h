//
// Filename:	CustomShaderEffectTree.h
// Description:	Class for controlling tree shaders variables
// Written by:	Andrzej
//
//	18/06/2011 -	Andrzej:	- initial;
//
//
//
//
#ifndef __CCUSTOMSHADEREFFECTTREE_H__
#define __CCUSTOMSHADEREFFECTTREE_H__

#include "rmcore\drawable.h"
#include "shaders\CustomShaderEffectBase.h"
#include "shaders\CustomShaderEffectTint.h"
#include "shader_source\Vegetation\Trees\trees_shared.h"
#include "math\random.h"

#define	CSE_TREE_EDITABLEVALUES				(__BANK)

#if RSG_PC
#define CSE_TREE_MAX_TEXTURES	(MAX_GPUS + 1)
#else
#define CSE_TREE_MAX_TEXTURES	(2)
#endif
//
//
//
//
class CCustomShaderEffectTreeType : public CCustomShaderEffectBaseType
{
	friend class CCustomShaderEffectTree;

public:
	enum { NUM_COL_VEH = 4	};	// support up to 4 vehicles deforming bushes

public:
	CCustomShaderEffectTreeType() : CCustomShaderEffectBaseType(CSE_TREE) {}

	virtual bool							Initialise(rmcDrawable *)						{ Assert(0); return(false); }
	bool									Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bTint, bool bLod2d, bool bVehicleDeform);
	virtual CCustomShaderEffectBase*		CreateInstance(CEntity *pEntity);
	virtual bool							RestoreModelInfoDrawable(rmcDrawable *pDrawable);

protected:
	virtual ~CCustomShaderEffectTreeType();

	inline float							GetMaxPlayerCollisionDist2()					{ return m_fMaxPlayerCollisionDist2;	}
	inline float							GetMaxVehicleCollisionDist()					{ return m_fMaxVehicleCollisionDist;	}
	inline bool								IsVehicleDeformable()							{ return m_bVehDeformable;				}
	inline bool								ContainsLod2d()									{ return m_bLod2d;						}

private:
	CCustomShaderEffectTintType				*m_pTintType;	// tint type cascade

	u8										m_idVarVehCollEnabled[NUM_COL_VEH];
	u8										m_idVarVehColl[NUM_COL_VEH];

	u8										m_idVarEntityScale;
	u8										m_idVarPlayerCollEnabled;
	u8										m_idVarWorldPlayerPos_umGlobalPhaseShift;

	u8										m_idVarUmGlobalOverrideParams;

	u8										m_bVehDeformable	: 1;
	u8										m_bLod2d			: 1;
	u8										m_bHasNewWindShaders : 1;
	ATTR_UNUSED u8							m_nUnused			: 5;
	ATTR_UNUSED u8							m_pad01;
	ATTR_UNUSED u8							m_pad02;

	float									m_fMaxPlayerCollisionDist2;
	float									m_fMaxVehicleCollisionDist;

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	grcEffectGlobalVar						ms_idVar_branchBendEtc_WindVector;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_WindSpeedSoftClamp;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_UnderWaterTransition;
#if TREES_INCLUDE_SFX_WIND
	grcEffectGlobalVar						ms_idVar_branchBendEtc_SfxWindEvalModulation;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_SfxWindValueModulation;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_AABBInfo;
	// Wind field texture.
	grmShaderGroupVar						m_idLocalVar_branchBendEtc_SfxWindField;
#endif // TREES_INCLUDE_SFX_WIND

	grcEffectGlobalVar						ms_idVar_branchBendEtc_Control1;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_Control2;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_DebugRenderControl1;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_DebugRenderControl2;
	grcEffectGlobalVar						ms_idVar_branchBendEtc_DebugRenderControl3;

#if CSE_TREE_EDITABLEVALUES
	grcEffectGlobalVar						ms_idVar_triuMovement_FreqAndAmp;
	grcEffectGlobalVar						ms_idVar_branchBend_Stiffness;
	grcEffectGlobalVar						ms_idVar_branchBend_FoliageStiffness;

	grmShaderGroupVar						m_idLocalVar_umTriWave1Params;
	grmShaderGroupVar						m_idLocalVar_umTriWave2Params;
	grmShaderGroupVar						m_idLocalVar_umTriWave3Params;
	grmShaderGroupVar						m_idLocalVar_branchBendPivot;
	grmShaderGroupVar						m_idLocalVar_branchBendStiffnessAdjust;
	grmShaderGroupVar						m_idLocalVar_foliageBranchBendPivot;
	grmShaderGroupVar						m_idLocalVar_foliageBranchBendStiffnessAdjust;
	grmShaderGroupVar						m_idLocalVar_stiffnessMultiplier;
#endif // CSE_TREE_EDITABLEVALUES

#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
};


//
//
//
//
class CCustomShaderEffectTree : public CCustomShaderEffectBase
{
	friend class CCustomShaderEffectTreeType;

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
#if TREES_INCLUDE_SFX_WIND

public:
	class SfxWindFieldValues
	{
	public:
		SfxWindFieldValues()
		{
			Reset();
		}
		~SfxWindFieldValues() 
		{
		}
	public:
		void Reset()
		{
			for(int i=0; i<TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE; i++)
				m_WindField[i] = Vec4V(V_ZERO);
		}
	public:
		Vec4V m_WindField[TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE]; 
	};

	class SfxWindFieldCacheEntry
	{
	public:
		SfxWindFieldCacheEntry();
		~SfxWindFieldCacheEntry();

	public:
		void Reset();
		void SetWindField(SfxWindFieldValues &values);
		bool CoolDownWindField();
		void UpdateTexture();
	public:
		u32 m_TickLabelWhenLastUsed; // Update tick label when this entry was last used.
		fwEntity *m_pEntity; // Which entity this entry is bound with.
		SfxWindFieldValues m_Values; // CPU copy of current wind field (it`s updated then copied into a texture for rendering).
		grcTexture *m_pWindFieldTextures[CSE_TREE_MAX_TEXTURES]; // Double buffered.
	};

#endif // TREES_INCLUDE_SFX_WIND
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

public:
	CCustomShaderEffectTree();
	~CCustomShaderEffectTree();

public:
	static void							InitClass();
	static void							EnableShaderVarCaching(bool value);
private:
	static void							InvalidateShaderVarCache();
public:

	static void							DontSetShaderVariables(bool dontSet);
	virtual void						SetShaderVariables(rmcDrawable* pDrawable);
	virtual void						AddToDrawList(u32 modelIndex, bool bExecute);
	virtual void						AddToDrawListAfterDraw();
	virtual u32							AddDataForPrototype(void * address);

	virtual void						Update(fwEntity *pEntity);
	Vec3V								EvaluateWind(fwEntity *pEntity, const Vec3V &entityPos);
	Vec3V								ApplyRandomOffset(Vec3V &baseWind, int randOffset);
	void								AddWindToSmoother(Vec3V &wind);
	Vec3V								EvaluateWindWithRandomOffset(fwEntity *pEntity, const Vec3V &entityPos, int randOffset);
	u32									GetNumTintPalettes();

	void								SetUmGlobalOverrideParams(float scaleH, float scaleV, float argFreqH, float argFreqV);
	void								SetUmGlobalOverrideParams(Vec4V_In scaleArgFreq);
	void								SetUmGlobalOverrideParams(float s);
	void								GetUmGlobalOverrideParams(float *pScaleH, float *pScaleV, float *pArgFreqH, float *pArgFreqV);

#if CSE_TREE_EDITABLEVALUES
	static bool							InitWidgets(bkBank& bank);
	static void							SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable, CCustomShaderEffectTreeType* pType);
#endif

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	static void							Init();
private:
	static void							LoadParserFiles();
#if CSE_TREE_EDITABLEVALUES
	static void							SaveParserFiles();
	static void							OnButtonLoad();
	static void							OnButtonSave();
#endif // CSE_TREE_EDITABLEVALUES

public:
	static void							UpdateWindStuff();
	static void							Synchronise() { ms_BufferIndex = (ms_BufferIndex + 1)%CSE_TREE_MAX_TEXTURES; }
	static u32							GetUpdateBuffer() { return ms_BufferIndex; }
	static u32							GetRenderBuffer() { return (ms_BufferIndex - 1)%CSE_TREE_MAX_TEXTURES; }
	void								SetWindVector(Vec3V windVector);
	static Vec4V						CreateUnderWaterBlend(CEntity *pEntity);
#if TREES_INCLUDE_SFX_WIND
	static void							InitialiseSfxWindFieldCache();
	static void							UpdateSfxWindCache();
	SfxWindFieldCacheEntry				*GetCurrentSfxWindFieldCacheEntry(fwEntity *pEntity);
	SfxWindFieldCacheEntry				*GetNewSfxWindFieldCacheEntry(fwEntity *pEntity);
	void								ReleaseSfxWindFieldCacheEntry(class SfxWindFieldCacheEntry *pCacheEntry);
#endif // TREES_INCLUDE_SFX_WIND

	void								SetBranchBendAndTriWaveMicromovementParams(rmcDrawable* pDrawable);
#if CSE_TREE_EDITABLEVALUES
	static void							SetEditableBranchBendAndTriWaveMicromovementParams(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable, CCustomShaderEffectTreeType* pType, bool isSelectedEntity);
	static void							OnMAXShaderVarsLoad();
	static void							OnMAXShaderVarsSave();
#endif
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

	//Instancing support!
protected:
	//Tree instancing Constants: base_class + 1
	static const u32 sNumRegsPerInst;

public:
	virtual bool HasPerBatchShaderVars() const;
	virtual u32 GetNumRegistersPerInstance() const		{ return sNumRegsPerInst; }
	virtual Vector4 *WriteInstanceData(grcVec4BufferInstanceBufferList &ibList, Vector4 *pDest, const InstancedEntityCache &entityCache, u32 alpha) const;
	virtual void AddBatchRenderStateToDrawList() const;
	virtual void AddBatchRenderStateToDrawListAfterDraw() const;

private:
	void								SetGlobalVehCollisionParams(u32 n, bool bEnable, Vec3V_In vecB, Vec3V_In vecM, ScalarV_In radius, ScalarV_In groundZ);

private:
	CCustomShaderEffectTreeType*		m_pType;
	CCustomShaderEffectTint*			m_pTint;	// tint instance cascade

	float								m_umPhaseShift;
	float								m_scaleXY;
	float								m_scaleZ;
	ScalarV								m_avgWindStrength;

	struct CVehCollisionData
	{
		Vec4V B;
		Vec4V M;
		Vec4V R;
	};
	CVehCollisionData					m_vecVehCollision		[CCustomShaderEffectTreeType::NUM_COL_VEH];
	Float16Vec4							m_umGlobalOverrideParams;

	bool								m_bVehCollisionEnabled	[CCustomShaderEffectTreeType::NUM_COL_VEH];
	u8									m_bPlayerCollEnabled: 1;
	u8									m_bHasNewWindShaders: 1;
	u8									m_bIsSelectedEntity	: 1;
	u8									m_CacheDebug		: 2;	
	ATTR_UNUSED u8						m_pad_u32			: 3;
	s8									m_SfxWindCacheEntry;
	s8									m_SfxWindTextures[CSE_TREE_MAX_TEXTURES];

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

	class BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SHADER_VARS
	{
	public:
		BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SHADER_VARS()
		{
			for(int i=0; i<4; i++)
				m_WindVectors[i] = Vec4V(V_ZERO);

			m_AABBMin = Vec4V(V_ZERO);
			m_OneOverAABBDiagonal = Vec4V(V_ZERO);
		}

	public:
		// These should probably be double buffered!
		Vec4V m_WindVectors[4];
		// These not.
		Vec4V m_AABBMin;
		Vec4V m_OneOverAABBDiagonal;
		Vec4V m_UnderwaterBlend;
	};

	class WindSmoother
	{
	public:
		WindSmoother()
		{
			for(int i=0; i<3; i++)
				m_ControlPoints[i] = Vec3V(0.1f, 0.1f, 0.1f); // Some small initial wind value.
		}
		~WindSmoother()
		{

		}
	public:
		void AcceptNewControlPoint(Vec3V windControlPoint)
		{
			m_ControlPoints[2] = m_ControlPoints[1];
			m_ControlPoints[1] = m_ControlPoints[0];
			m_ControlPoints[0] = windControlPoint;
		}
		void RecycleLeastRecentControlPoint()
		{
			Vec3V v = m_ControlPoints[2];
			AcceptNewControlPoint(v);
		}
		Vec3V CalculateSmoothedWind(float normalizedTime)
		{
			return ComputeBezierCurve((m_ControlPoints[2] + m_ControlPoints[1])*ScalarV(0.5f), m_ControlPoints[1], (m_ControlPoints[1] + m_ControlPoints[0])*ScalarV(0.5f), normalizedTime);
		}
	private:
		Vec3V ComputeBezierCurve(Vec3V A, Vec3V B, Vec3V C, float t)
		{
			float coeffA = (1.0f-t)*(1.0f-t);
			float coeffB = 2.0f*t*(1.0f - t);
			float coeffC = t*t;
			return ScalarV(coeffA)*A + ScalarV(coeffB)*B + ScalarV(coeffC)*C;
		}
	private:
		Vec3V m_ControlPoints[3];
	};

	BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SHADER_VARS m_WindShaderVars;
	// TODO:- Probably have a cache of these for near the viewpoint.
	WindSmoother m_WindSmoother;
	
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	
private:
	struct CCachedCSETreeShaderVars
	{
		float				scaleXY;
		float				scaleZ;
		float				umPhaseShift;
		u8					bPlayerCollEnabled;											// note: byte, not bool
		Vector3				WorldPlayerPos;
		u8					VehCollEnabled[CCustomShaderEffectTreeType::NUM_COL_VEH];	// note: byte, not bool
		CVehCollisionData	VehColl[CCustomShaderEffectTreeType::NUM_COL_VEH];
		Float16Vec4			umGlobalOverrideParams;

	};
	static CCachedCSETreeShaderVars		ms_CachedCSETreeShaderVars[NUMBER_OF_RENDER_THREADS];
	static bool							ms_EnableShaderVarCaching[NUMBER_OF_RENDER_THREADS];

#if CSE_TREE_EDITABLEVALUES
	static bool							ms_bEVEnabled;
	static Vector4						ms_fEVumGlobalParams;	
	static bool							ms_bEVUseUmGlobalOverrideValues;
	static Vector4						ms_fEVUmGlobalOverrideParams;
	static Vector4						ms_fEVWindGlobalParams;
	static bool							ms_bEVWindResponseEnabled;
	static ScalarV						ms_fEVWindResponseAmount;
	static ScalarV						ms_fEVWindResponseVelMin;
	static ScalarV						ms_fEVWindResponseVelMax;
	static ScalarV						ms_fEVWindResponseVelExp;
#endif

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

public:
	class BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_GLOBALS
	{
	public:
		BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_GLOBALS()
		{
			m_umLowWind = 0.0f;
			m_umHighWind = 3.5f;

			m_WindVariationRange = 0.268f;
			m_WindVariationScales[0] = 1.0f;
			m_WindVariationScales[1] = 0.862f;
			m_WindVariationScales[2] = 0.754f;
			m_WindVariationBlendWaveFreq[0] = 0.123f;
			m_WindVariationBlendWaveFreq[1] = 0.074f;

			m_WindSmoothChangeControlPointInterval = 0.5f;
			m_WindSmoothNormalisedTime = 0.0f;
			m_WindSmoothTime = 0.0f;
			m_WindSmoothUpdateCount = 0;

			m_WindSpeedSoftClamp = 20.0f;
			m_WindSpeedSoftClampUnrestrictedProportion = 0.99f;

			m_UnderWaterTransitionRange = 1.0f;

			m_AlphaCardOnlyGlobalStiffness = 0.7f;
		}
		float	m_umLowWind;	// Wind speed for low wind micro-movement (see m_BasisWaves[n].lowWind).
		float	m_umHighWind;	// Wind speed for high wind micro-movement (see m_BasisWaves[n].highWind).

		// Wind variation range (branch bending uses 4 wind vectors - 3 are offset from the main wind vector).
		float	m_WindVariationRange;
		float	m_WindVariationScales[3];
		float	m_WindVariationBlendWaveFreq[2];

		// Time interval when wind vector control points change (in seconds).
		float	m_WindSmoothChangeControlPointInterval;
		// In interval (0, m_WindSmoothChangeControlPointInterval).
		float	m_WindSmoothTime;
		// Current time (in range 0, 1 - for Bezier curve calculations.).
		float	m_WindSmoothNormalisedTime;
		// Wind update count. Used to flag up when we need to change control points (i.e. calculate new wind vectors).
		u32		m_WindSmoothUpdateCount;

		// Max wind speed soft clamp.
		float	m_WindSpeedSoftClamp;
		float	m_WindSpeedSoftClampUnrestrictedProportion;

		// Height above water over which we blend form original uMovements to new wind.
		float	m_UnderWaterTransitionRange;

		// Stiffness for alpha card only cut-down version.
		float	m_AlphaCardOnlyGlobalStiffness;
		PAR_SIMPLE_PARSABLE;
	};

	class BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SFX_GLOBALS
	{
	public:
		BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SFX_GLOBALS()
		{
		#if TREES_INCLUDE_SFX_WIND
				m_SfxWindEvalModulationFreq.x = 0.1f;
				m_SfxWindEvalModulationFreq.y = 0.2f;
				m_SfxWindEvalModulationFreq.z = 0.3f;
				m_SfxWindEvalModulationDisplacementAmp = 2.0f;
				m_SfxWindValueModulationProportion = 0.5f;
				m_SfxMaxAccrn = 10.0f;
		#endif // TREES_INCLUDE_SFX_WIND
		}

		Vector3	m_SfxWindEvalModulationFreq;
		float	m_SfxWindEvalModulationDisplacementAmp;
		float	m_SfxWindValueModulationProportion;
		float	m_SfxMaxAccrn;
		PAR_SIMPLE_PARSABLE;
	};

public:

	class BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS
	{
	public:
		class TRIANGLE_WAVE
		{
		public:
			TRIANGLE_WAVE()
			{
				freqAndAmp.x = 0.0f;
				freqAndAmp.y = 0.0f;
			}
			Vector2 freqAndAmp;
			PAR_SIMPLE_PARSABLE;
		};
		struct BASIS_WAVE
		{
			TRIANGLE_WAVE lowWind;
			TRIANGLE_WAVE highWind;
			PAR_SIMPLE_PARSABLE;
		};
	public:
		BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS()
		{
		#if CSE_TREE_EDITABLEVALUES
			m_PivotHeight = 3.150f;
			m_TrunkStiffnessAdjustLow = 2.0f;
			m_TrunkStiffnessAdjustHigh = 9.06f;
			m_PhaseStiffnessAdjustLow = 3.0f;
			m_PhaseStiffnessAdjustHigh = 15.0f;
			m_GlobalStiffnessMultiplier = 1.0f;

			// Some initial values suitable for the oak tree near -playercoords=-944.0,2762.0,25.4.
			m_Stiffness = 0.9f; 
			m_FoliageStiffness = 0.85f;
			m_ConeAngle = 1.36f;
			m_FallOffRadius = 60.0f;

			m_BasisWaves[0].lowWind.freqAndAmp = Vector2(0.053f, 0.020f);
			m_BasisWaves[1].lowWind.freqAndAmp = Vector2(0.046f, 0.019f);
			m_BasisWaves[2].lowWind.freqAndAmp = Vector2(0.079f, 0.012f);

			m_BasisWaves[0].highWind.freqAndAmp = Vector2(0.043f, 0.110f);
			m_BasisWaves[1].highWind.freqAndAmp = Vector2(0.028f, 0.048f);
			m_BasisWaves[2].highWind.freqAndAmp = Vector2(0.010f, 0.029f);
		#endif // CSE_TREE_EDITABLEVALUES
		}
	public:

		float m_PivotHeight;
		float m_TrunkStiffnessAdjustLow;
		float m_TrunkStiffnessAdjustHigh;
		float m_PhaseStiffnessAdjustLow;
		float m_PhaseStiffnessAdjustHigh;
		float m_GlobalStiffnessMultiplier;
		// TODO:- Remove old cone and fall off.
		float m_Stiffness; 
		float m_ConeAngle;
		float m_FallOffRadius;
		float m_FoliageStiffness;
		BASIS_WAVE m_BasisWaves[TREES_NUMBER_OF_BASIS_WAVES]; // Sum the 3 triangle waves to form a wobbly wave which is applied during micro-movement.
		PAR_SIMPLE_PARSABLE;
	};

#if CSE_TREE_EDITABLEVALUES
	class BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_DEBUG_CONTROL
	{
	public:
		BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_DEBUG_CONTROL()
		{
			m_enabled = false;
			m_OverrideumLowHighBlend = false;
			m_umLowHighBlendOverride = 0.0f;
			m_NoBranchBending = false;
			m_NoBranchBendingPhaseVariations = false;
			m_NoumMovements = false;
			m_NoRegularWind = false;
			m_NoSfxWind = false;
			m_NoAlphaCardVariation = false;
			m_RedirectToSelectedEntity = false;
			m_ApplyToAllEntities = false;
			m_DebugRenderStiffnessRange = 0.8f;
			m_ApplyOverrideWindSpeed = false;
			m_WindSpeedOverride = 2.0f;
			m_ForceUnderWater = false;
			m_ForceAboveWater = false;
		}
		bool	m_enabled;	// Run-time on/off.
		bool	m_OverrideumLowHighBlend; // Overrides blend wind strength for blending low->high wind micro-movements.
		float	m_umLowHighBlendOverride; // Set by RAG.

		bool	m_RedirectToSelectedEntity;
		float	m_DebugRenderStiffnessRange;
		bool	m_ApplyToAllEntities;

		bool	m_NoBranchBending; // Disable ALL branch bending.
		bool	m_NoBranchBendingPhaseVariations; // Disable phase variations.
		bool	m_NoumMovements; // Disable micro-movements.
		bool	m_NoRegularWind; // No normal wind.
		bool	m_NoSfxWind; // No Sfx wind.
		bool	m_NoAlphaCardVariation; // No phase variation on alpha cards.

		// Wind speed override.
		bool	m_ApplyOverrideWindSpeed;
		float	m_WindSpeedOverride;

		bool	m_ForceUnderWater; // Forces under water type movement.
		bool	m_ForceAboveWater; // Forces under water type movement.
	};
#endif // CSE_TREE_EDITABLEVALUES

	static u32 ms_BufferIndex;
	static mthRandom ms_WindRandomizer;
	static BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_GLOBALS ms_branchBendEtc_Globals;
#if TREES_INCLUDE_SFX_WIND
		static BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SFX_GLOBALS ms_branchBendEtc_SfxGlobals;

		static u32 ms_UpdateTickLabel;
		static SfxWindFieldCacheEntry *ms_pSfxWindFieldCache;
#endif // TREES_INCLUDE_SFX_WIND

#if CSE_TREE_EDITABLEVALUES
	static BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS ms_branchBendEtc_Locals;
	static BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_DEBUG_CONTROL ms_branchBendEtc_DebugControl;

	enum TreeDebugRenderMode
	{
		NONE = 0,
		TRUNK_STIFFNESS,
		PHASE_STIFFNESS,
		PHASE,
		FOLIAGE_PHASE,
		UM_AMPLITUDE_H,
		UM_AMPLITUDE_V,
		SFX_CACHE,
		TREES_DEBUG_RENDER_MODES_MAX,
	};

	static unsigned char ms_DebugRenderMode;
	static const char *ms_DebugRenderModes[TREES_DEBUG_RENDER_MODES_MAX];
	static void NullFunction(void) {}
#endif // CSE_TREE_EDITABLEVALUES
	
	static DECLARE_MTR_THREAD bool ms_DontSetSetShaderVariables;

	friend class CCustomShaderEffectTreeType;
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
};

#endif //__CCUSTOMSHADEREFFECTTREE_H__...

