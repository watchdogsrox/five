//
// Filename:	CustomShaderEffectTree.cpp
// Description:	Class for controlling tree shaders variables
// Written by:	Andrzej
//
//	18/06/2011 -	Andrzej:	- initial;
//
//
//
//

// Rage headers
#include "math\amath.h"
#include "grmodel\ShaderFx.h"
#include "grmodel\Geometry.h"
#include "grcore\texturereference.h"
#include "rmcore/drawable.h"
#include "pheffects\wind.h"

#include "fwdrawlist/drawlistmgr.h"
#include "fwdebug/picker.h"

// Game headers
#include "debug\debug.h"
#include "renderer\PlantsGrassRenderer.h"
#include "Peds\ped.h"
#include "objects\Object.h"
#include "objects\ProceduralInfo.h"
#include "scene\Entity.h"
#include "scene\world\GameWorld.h"
#include "scene\world\GameWorldWaterHeight.h"
#include "vehicles\Vehicle.h"
#include "CustomShaderEffectTree.h"
#include "shaders\ShaderLib.h"
#include "renderer\DrawLists\drawlist.h"
#include "renderer\RenderPhases\RenderPhaseCascadeShadows.h"

#include "../shader_source/Lighting/Shadows/cascadeshadows_common.fxh"

#if RSG_PC && __D3D11
#include "grcore/texture_d3d11.h"
#endif // RSG_PC && __D3D11

#include "CustomShaderEffectTree_Parser.h"

//#include "system/findsize.h"
//FindSize(CCustomShaderEffectTree);

#define TREE_DEFAULT_WIND_RESPONSE_AMOUNT					(ScalarV(V_FIVE))
#define TREE_DEFAULT_WIND_RESPONSE_VEL_MIN					(ScalarV(V_TWO))
#define TREE_DEFAULT_WIND_RESPONSE_VEL_MAX					(ScalarVConstant<FLOAT_TO_INT(30.0f)>())
#define TREE_DEFAULT_WIND_RESPONSE_VEL_EXP					(ScalarV(V_ZERO))
#define TREE_DEFAULT_WIND_STRENGTH_INFLUENCE				(ScalarVConstant<FLOAT_TO_INT(0.9f)>())

#define TREE_WIND_MAX_DISTANCE_LOCAL_DISTURBANCE			(ScalarVConstant<FLOAT_TO_INT(50.0f)>())
#define TREE_WIND_MAX_DISTANCE_GLOBAL_DISTURBANCE			(ScalarVConstant<FLOAT_TO_INT(40.0f)>())
#define TREE_WIND_MIN_DISTANCE_IGNORE_NON_DISTURBANCE		(ScalarVConstant<FLOAT_TO_INT(40.0f)>())
#define TREE_WIND_MAX_DISTANCE_LOCAL_VELOCITY				(ScalarVConstant<FLOAT_TO_INT(60.0f)>())

#define TREE_WIND_MAX_DISTANCE_LOCAL_DISTURBANCE_SQR		(TREE_WIND_MAX_DISTANCE_LOCAL_DISTURBANCE * TREE_WIND_MAX_DISTANCE_LOCAL_DISTURBANCE)
#define TREE_WIND_MAX_DISTANCE_GLOBAL_DISTURBANCE_SQR		(TREE_WIND_MAX_DISTANCE_GLOBAL_DISTURBANCE * TREE_WIND_MAX_DISTANCE_GLOBAL_DISTURBANCE)
#define TREE_WIND_MIN_DISTANCE_IGNORE_NON_DISTURBANCE_SQR	(TREE_WIND_MIN_DISTANCE_IGNORE_NON_DISTURBANCE * TREE_WIND_MIN_DISTANCE_IGNORE_NON_DISTURBANCE)
#define TREE_WIND_MAX_DISTANCE_LOCAL_VELOCITY_SQR			(TREE_WIND_MAX_DISTANCE_LOCAL_VELOCITY * TREE_WIND_MAX_DISTANCE_LOCAL_VELOCITY)


#define TREE_WIND_MAX_DISTANCE_FOR_SFX_WIND_FIELD			(ScalarVConstant<FLOAT_TO_INT(20.0f)>())
#define TREE_WIND_MAX_DISTANCE_FOR_SFX_WIND_FIELD_SQR		(TREE_WIND_MAX_DISTANCE_FOR_SFX_WIND_FIELD * TREE_WIND_MAX_DISTANCE_FOR_SFX_WIND_FIELD)


#if CSE_TREE_EDITABLEVALUES
	static	ScalarV g_AvgTreeWindStrengthInfluence						= TREE_DEFAULT_WIND_STRENGTH_INFLUENCE;
	bool	CCustomShaderEffectTree::ms_bEVEnabled						= false;
	Vector4	CCustomShaderEffectTree::ms_fEVumGlobalParams				= Vector4(0.025f, 0.020f, 1.000f, 0.500f);  //ScaleH, ScaleV, ArgFreqH, ArgFreqV
	bool	CCustomShaderEffectTree::ms_bEVUseUmGlobalOverrideValues	= false;	
	Vector4	CCustomShaderEffectTree::ms_fEVUmGlobalOverrideParams		= Vector4(1.0f, 1.0f, 1.0f, 1.0f);
	Vector4	CCustomShaderEffectTree::ms_fEVWindGlobalParams				= Vector4(1.0f, 5.0f, 5.0f, 1.0f);			//Scale, playerCollR, vehicleCollR, n/a

	bool	CCustomShaderEffectTree::ms_bEVWindResponseEnabled			= true;
	ScalarV	CCustomShaderEffectTree::ms_fEVWindResponseAmount			= TREE_DEFAULT_WIND_RESPONSE_AMOUNT;
	ScalarV	CCustomShaderEffectTree::ms_fEVWindResponseVelMin			= TREE_DEFAULT_WIND_RESPONSE_VEL_MIN;
	ScalarV	CCustomShaderEffectTree::ms_fEVWindResponseVelMax			= TREE_DEFAULT_WIND_RESPONSE_VEL_MAX;
	ScalarV	CCustomShaderEffectTree::ms_fEVWindResponseVelExp			= TREE_DEFAULT_WIND_RESPONSE_VEL_EXP;

	static ScalarV g_MaxTreeDistForLocalWindDisturbance					= TREE_WIND_MAX_DISTANCE_LOCAL_DISTURBANCE;
	static ScalarV g_MinTreeDistForIgnoreNonDisturbance					= TREE_WIND_MIN_DISTANCE_IGNORE_NON_DISTURBANCE;
	static ScalarV g_MaxTreeDistForGlobalDisturbance					= TREE_WIND_MAX_DISTANCE_GLOBAL_DISTURBANCE;
	static ScalarV g_MaxTreeDistForLocalVelocity						= TREE_WIND_MAX_DISTANCE_LOCAL_VELOCITY;
#endif

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	u32 CCustomShaderEffectTree::ms_BufferIndex = 0;
	mthRandom CCustomShaderEffectTree::ms_WindRandomizer;
	CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_GLOBALS CCustomShaderEffectTree::ms_branchBendEtc_Globals;
#if TREES_INCLUDE_SFX_WIND
	CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_SFX_GLOBALS CCustomShaderEffectTree::ms_branchBendEtc_SfxGlobals;
#endif // TREES_INCLUDE_SFX_WIND

#if CSE_TREE_EDITABLEVALUES
	CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_LOCALS CCustomShaderEffectTree::ms_branchBendEtc_Locals;
	CCustomShaderEffectTree::BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS_DEBUG_CONTROL CCustomShaderEffectTree::ms_branchBendEtc_DebugControl;
#endif // CSE_TREE_EDITABLEVALUES

#if TREES_INCLUDE_SFX_WIND
	u32 CCustomShaderEffectTree::ms_UpdateTickLabel = 0;
	CCustomShaderEffectTree::SfxWindFieldCacheEntry *CCustomShaderEffectTree::ms_pSfxWindFieldCache = NULL;
#if CSE_TREE_EDITABLEVALUES
	static ScalarV g_MaxTreeDistForSfxWindField							= TREE_WIND_MAX_DISTANCE_FOR_SFX_WIND_FIELD;
#endif

#define TREES_SFX_WIND_CACHE_ENTRY_NONE						0
#define TREES_SFX_WIND_CACHE_ENTRY_OK						1
#define TREES_SFX_WIND_CACHE_ENTRY_COOLING_DOWN				2
#define TREES_SFX_WIND_CACHE_ENTRY_NEED_ONE					3

#endif // TREES_INCLUDE_SFX_WIND

#if CSE_TREE_EDITABLEVALUES

unsigned char CCustomShaderEffectTree::ms_DebugRenderMode = 0;
const char *CCustomShaderEffectTree::ms_DebugRenderModes[TREES_DEBUG_RENDER_MODES_MAX] =	{ 
																"None",
																"Trunk stiffness",
																"Phase stiffness",
																"Phase",
																"Foliage Phase",
																"umAmplitude H",
																"umAmplitude V",
																"Sfx cache"
															};
#endif

DECLARE_MTR_THREAD bool CCustomShaderEffectTree::ms_DontSetSetShaderVariables = false;

#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

static const u32 sNumAdditionalRegsPerInst = 1;
const u32 CCustomShaderEffectTree::sNumRegsPerInst = CCustomShaderEffectBase::sNumRegsPerInst + sNumAdditionalRegsPerInst;

static BankBool										g_UseTreeShaderVarCaching											= true;
bool												CCustomShaderEffectTree::ms_EnableShaderVarCaching[NUMBER_OF_RENDER_THREADS] = {0};
CCustomShaderEffectTree::CCachedCSETreeShaderVars	CCustomShaderEffectTree::ms_CachedCSETreeShaderVars[NUMBER_OF_RENDER_THREADS];

//
//
//
//
inline u8 TreeShaderGroupLookupVar(grmShaderGroup *pShaderGroup, const char *name, bool mustExist)
{
	grmShaderGroupVar varID = pShaderGroup->LookupVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

inline u8 TreeEffectLookupGlobalVar(const char *name, bool mustExist)
{
	grcEffectGlobalVar  varID = grcEffect::LookupGlobalVar(name, mustExist);
	Assert((int)varID < 256);	// must fit into u8
	return (u8)varID;
}

//
//
// initializes all instances of the effect class;
// to be called once, when "master" class is created;
//
bool CCustomShaderEffectTreeType::Initialise(rmcDrawable *pDrawable, CBaseModelInfo *pMI, bool bTint, bool bLod2d, bool bVehicleDeformation)
{
	Assert(pDrawable);

	// create cascaded tint type:
	if(bTint)
	{
		m_pTintType = CCustomShaderEffectTintType::Create(pDrawable, pMI);
		if(!m_pTintType->Initialise(pDrawable))
		{
			m_pTintType->RemoveRef();
			m_pTintType = NULL;
			Assertf(0, "Error initialising CustomShaderEffectTint!");
			return(FALSE);
		}
	}
	else
	{
		m_pTintType = NULL;
	}

	grmShaderGroup *pShaderGroup = &pDrawable->GetShaderGroup();

	m_idVarPlayerCollEnabled					= TreeEffectLookupGlobalVar("bPlayerCollEnabled0", TRUE);
	m_idVarEntityScale							= TreeEffectLookupGlobalVar("globalEntityScale0", TRUE);
	m_idVarWorldPlayerPos_umGlobalPhaseShift	= TreeEffectLookupGlobalVar("worldPlayerPos_umGlobalPhaseShift0", TRUE);
#if !TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	m_idVarUmGlobalOverrideParams				= TreeEffectLookupGlobalVar("umGlobalOverrideParams0", TRUE);
#endif //!TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	m_bHasNewWindShaders						= 0;

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	ms_idVar_branchBendEtc_WindVector			= grcEffect::LookupGlobalVar("branchBendEtc_WindVector0", RSG_PC ? FALSE : TRUE);
	ms_idVar_branchBendEtc_WindSpeedSoftClamp	= grcEffect::LookupGlobalVar("branchBendEtc_WindSpeedSoftClamp0", RSG_PC ? FALSE : TRUE);
	ms_idVar_branchBendEtc_UnderWaterTransition = grcEffect::LookupGlobalVar("branchBendEtc_UnderWaterTransition0", RSG_PC ? FALSE : TRUE);

	if(pShaderGroup->LookupVar("branchBendPivot0", false) != grmsgvNONE)
		m_bHasNewWindShaders = true;

#if TREES_INCLUDE_SFX_WIND
	ms_idVar_branchBendEtc_AABBInfo					= grcEffect::LookupGlobalVar("branchBendEtc_AABBInfo0", RSG_PC ? FALSE : TRUE);
	ms_idVar_branchBendEtc_SfxWindEvalModulation	= grcEffect::LookupGlobalVar("branchBendEtc_SfxWindEvalModulation0", RSG_PC ? FALSE : TRUE);
	ms_idVar_branchBendEtc_SfxWindValueModulation	= grcEffect::LookupGlobalVar("branchBendEtc_SfxWindValueModulation0", RSG_PC ? FALSE : TRUE);
	m_idLocalVar_branchBendEtc_SfxWindField			= pShaderGroup->LookupVar("SfxWindTexture3D", FALSE);
#endif // TREES_INCLUDE_SFX_WIND

	ms_idVar_branchBendEtc_Control1				= grcEffect::LookupGlobalVar("branchBendEtc_Control10", FALSE);
	ms_idVar_branchBendEtc_Control2				= grcEffect::LookupGlobalVar("branchBendEtc_Control20", FALSE);
	ms_idVar_branchBendEtc_DebugRenderControl1	= grcEffect::LookupGlobalVar("branchBendEtc_DebugRenderControl10", FALSE);
	ms_idVar_branchBendEtc_DebugRenderControl2	= grcEffect::LookupGlobalVar("branchBendEtc_DebugRenderControl20", FALSE);
	ms_idVar_branchBendEtc_DebugRenderControl3	= grcEffect::LookupGlobalVar("branchBendEtc_DebugRenderControl30", FALSE);

#if CSE_TREE_EDITABLEVALUES
	// Cone and fall off globals.
	ms_idVar_triuMovement_FreqAndAmp			= grcEffect::LookupGlobalVar("trium_FreqAndAmp0", FALSE);
	ms_idVar_branchBend_Stiffness				= grcEffect::LookupGlobalVar("branchBend_Stiffness0", FALSE);
	ms_idVar_branchBend_FoliageStiffness		= grcEffect::LookupGlobalVar("branchBend_FoliageStiffness0", FALSE);

	// Proper locals.
	m_idLocalVar_umTriWave1Params	= pShaderGroup->LookupVar("umTriWave1Params0", FALSE);
	m_idLocalVar_umTriWave2Params	= pShaderGroup->LookupVar("umTriWave2Params0", FALSE);
	m_idLocalVar_umTriWave3Params	= pShaderGroup->LookupVar("umTriWave3Params0", FALSE);
	m_idLocalVar_branchBendPivot					= pShaderGroup->LookupVar("branchBendPivot0", FALSE);
	m_idLocalVar_branchBendStiffnessAdjust			= pShaderGroup->LookupVar("branchBendStiffnessAdjust0", FALSE);
	m_idLocalVar_foliageBranchBendPivot				= pShaderGroup->LookupVar("foliageBranchBendPivot0", FALSE);
	m_idLocalVar_foliageBranchBendStiffnessAdjust	= pShaderGroup->LookupVar("foliageBranchBendStiffnessAdjust0", FALSE);
	
	// Cut-down version locals.
	m_idLocalVar_stiffnessMultiplier = pShaderGroup->LookupVar("umGlobalParams0", FALSE);
#endif // CSE_TREE_EDITABLEVALUES

#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

	m_bVehDeformable				= bVehicleDeformation;
	m_bLod2d						= bLod2d;

	for(u32 i=0; i<NUM_COL_VEH; i++)
	{
		char var_name[128];
		
		sprintf(var_name, "bVehTreeColl%dEnabled0", i);	// bVehTreeColl0Enabled0
		m_idVarVehCollEnabled[i]= TreeEffectLookupGlobalVar(var_name, TRUE);

		sprintf(var_name, "vecVehColl%d", i); // vecVehColl0
		m_idVarVehColl[i]		= TreeEffectLookupGlobalVar(var_name, TRUE);		
		
	}

	static dev_float maxPlayerCollisionDist	= 5.0f;	// 5m distance
	static dev_float maxVehCollisionDist	= 5.0f;	// 5m distance

	m_fMaxPlayerCollisionDist2  = maxPlayerCollisionDist*maxPlayerCollisionDist;
	m_fMaxVehicleCollisionDist	= maxVehCollisionDist;

	// find highest collision radius for player and vehicle collision detection:
	if(1)
	{
		float highestDistY = -1.0f;
		float highestDistZ = -1.0f;

		const u32 varNameHash = ATSTRINGHASH("windGlobalParams0", 0x026123458);
		const u32 count = pShaderGroup->GetCount();
		for(u32 i=0; i<count; i++)
		{
			grmShader *pShader = pShaderGroup->GetShaderPtr(i);
			if(pShader)
			{
				grcEffectVar varID = pShader->LookupVarByHash(varNameHash);
				if(varID)
				{
					Vector4 globalParams;
					pShader->GetVar(varID, globalParams);

					if(globalParams.y > highestDistY)
					{
						highestDistY = globalParams.y;
					}
					
					if(globalParams.z > highestDistZ)
					{
						highestDistZ = globalParams.z;
					}
				}
			}
		}//for(u32 i=0; i<count; i++)...

		if(highestDistY > 0.0f)
		{
			m_fMaxPlayerCollisionDist2 = highestDistY*highestDistY;
		}

		if(highestDistZ > 0.0f)
		{
			m_fMaxVehicleCollisionDist = highestDistZ;
		}
	}

	return(TRUE);
}// end of CCustomShaderEffectTree::Initialise()...

CCustomShaderEffectTreeType::~CCustomShaderEffectTreeType()
{
	if(m_pTintType)
	{
		m_pTintType->RemoveRef();
		m_pTintType = NULL;
	}
}

//
//
//
//
CCustomShaderEffectBase*	CCustomShaderEffectTreeType::CreateInstance(CEntity* pEntity)
{
	CCustomShaderEffectTree *pNewShaderEffect = rage_new CCustomShaderEffectTree;

	pNewShaderEffect->m_pType = this;

	if(this->m_pTintType)
	{
		pNewShaderEffect->m_pTint = (CCustomShaderEffectTint*)(m_pTintType->CreateInstance(pEntity));	// cascaded CSE tint instance
	}
	else
	{
		pNewShaderEffect->m_pTint = NULL;
	}

	float phaseShift = 0.0f;
	if(pEntity)
	{
		// calculate localized micro-movement phase shift based on xy coords of the tree:
		const Vector3 entityPos = VEC3V_TO_VECTOR3(pEntity->GetTransform().GetPosition());
		const u32 phaseShiftX = u32( Abs(entityPos.x*10.0f) )&0x7f;
		const u32 phaseShiftY = u32( Abs(entityPos.y*10.0f) )&0x7f;
		phaseShift = float(phaseShiftX+phaseShiftY)/255.0f - 1.0f;
	}
	pNewShaderEffect->m_umPhaseShift = phaseShift;

	pNewShaderEffect->m_bPlayerCollEnabled = false;

	for(u32 i=0; i<CCustomShaderEffectTreeType::NUM_COL_VEH; i++)
	{
		pNewShaderEffect->m_bVehCollisionEnabled[i] = false;
	}

	pNewShaderEffect->m_avgWindStrength = ScalarV(V_ZERO);
	pNewShaderEffect->SetUmGlobalOverrideParams(1.0f);

	// parse entity XY & Z scaling:
	float entScaleXY=1.0f, entScaleZ=1.0f;
	if(1)
	{
		fwTransform *tr = (fwTransform*)pEntity->GetTransformPtr();

		if(tr->IsSimpleScaledTransform())
		{
			fwSimpleScaledTransform *simpleScaledTr	= static_cast<fwSimpleScaledTransform*>(tr);
			entScaleXY	= simpleScaledTr->GetScaleXY();
			entScaleZ	= simpleScaledTr->GetScaleZ();
		}
		else if(tr->IsMatrixScaledTransform())
		{
			fwMatrixScaledTransform *mtxScaledTr	= static_cast<fwMatrixScaledTransform*>(tr);
			entScaleXY	= mtxScaledTr->GetScaleXY();
			entScaleZ	= mtxScaledTr->GetScaleZ();
		}
		else if(tr->IsQuaternionScaledTransform())
		{
			fwQuaternionScaledTransform *mtxScaledTr	= static_cast<fwQuaternionScaledTransform*>(tr);
			entScaleXY	= mtxScaledTr->GetScaleXY();
			entScaleZ	= mtxScaledTr->GetScaleZ();
		}
		else
		{	// just transform:
			entScaleXY	= 1.0f;
			entScaleZ	= 1.0f;
		}
	}
	pNewShaderEffect->m_scaleXY = entScaleXY;
	pNewShaderEffect->m_scaleZ	= entScaleZ;
	pNewShaderEffect->m_bHasNewWindShaders	= m_bHasNewWindShaders;
	pNewShaderEffect->m_CacheDebug			= 0;
	pNewShaderEffect->m_SfxWindCacheEntry	= -1;

	for (u32 uTexIndex = 0; uTexIndex < CSE_TREE_MAX_TEXTURES; uTexIndex++)
	{
		pNewShaderEffect->m_SfxWindTextures[uTexIndex]	= -1;
	}
#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	rage::spdAABB boundingBox = pEntity->GetBaseModelInfo()->GetBoundingBox();
	Vec3V min = boundingBox.GetMin();
	Vec3V max = boundingBox.GetMax();
	Vec3V diagonal = max - min;

	pNewShaderEffect->m_WindShaderVars.m_AABBMin = Vec4V(min, ScalarV(0.0f));
	float lastTexel = 1.0f;
	//float lastTexel = (TREES_SFX_WIND_FIELD_TEXTURE_SIZE - 1)/(TREES_SFX_WIND_FIELD_TEXTURE_SIZE);
	pNewShaderEffect->m_WindShaderVars.m_OneOverAABBDiagonal = Vec4V(lastTexel/diagonal.GetXf(), lastTexel/diagonal.GetYf(), lastTexel/diagonal.GetZf(), 0.0f);
	pNewShaderEffect->m_WindShaderVars.m_UnderwaterBlend = CCustomShaderEffectTree::CreateUnderWaterBlend(pEntity);
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	
#if __ASSERT
	// BS#961202: verify against proc objects using PROCOBJ_ALIGN_OBJ flag and non-uniform scaling at the same time with tree shaders (which support players collision):
	const CProcObjInfo *pProcObjInfo=NULL;

	if(pEntity->m_nFlags.bIsProcObject)
	{
		if(CModelInfo::GetBaseModelInfo(pEntity->GetModelId())->GetIsTypeObject())
		{
			CObject *pObject = (CObject*)pEntity;
			if(pObject->m_procObjInfoIdx != 0xffff)
			{
				pProcObjInfo = &g_procInfo.GetProcObjInfo(pObject->m_procObjInfoIdx);
			}
		}
		else
		{
			CBuilding *pBuilding = (CBuilding*)pEntity;

			const u32 count = g_procInfo.GetProcObjCount();
			for(u32 i=0; i<count; i++)
			{
				const CProcObjInfo *procInfo = &g_procInfo.GetProcObjInfo(i);
				if(procInfo->m_ModelIndex.Get() == (s32)pBuilding->GetModelIndex())
				{
					pProcObjInfo = procInfo;
					break;
				}
			}
		}
	}
	
	if(pProcObjInfo)
	{
		if(pProcObjInfo->m_Flags.IsSet(PROCOBJ_ALIGN_OBJ))
		{
			if( (pProcObjInfo->m_MinScale.GetFloat32_FromFloat16() != 1.0f)	||
				(pProcObjInfo->m_MaxScale.GetFloat32_FromFloat16() != 1.0f)	)
			{
				Assertf(false,	"Procedural object '%s' from tag '%s' is using variable scaling XY and PROCOBJ_ALIGN_OBJ flag at the same time with tree shaders! "
								"This is not going to work.",
						pProcObjInfo->m_ModelName.GetCStr(), pProcObjInfo->m_Tag.GetCStr());
			}
			
			if(pProcObjInfo->m_MinScaleZ.GetFloat32_FromFloat16() > 0.0f)
			{
				if( (pProcObjInfo->m_MinScaleZ.GetFloat32_FromFloat16() != 1.0f)	||
					(pProcObjInfo->m_MaxScaleZ.GetFloat32_FromFloat16() != 1.0f)	)
				{
					Assertf(false,	"Procedural object '%s' from tag '%s' is using variable scaling Z and PROCOBJ_ALIGN_OBJ flag at the same time with tree shaders! "
									"This is not going to work.",
							pProcObjInfo->m_ModelName.GetCStr(), pProcObjInfo->m_Tag.GetCStr());
				}
			}
		}
	}
#endif //__ASSERT...

	return(pNewShaderEffect);
}

//
//
//
//
bool CCustomShaderEffectTreeType::RestoreModelInfoDrawable(rmcDrawable *pDrawable)
{
	if (m_pTintType)
	{
		return m_pTintType->RestoreModelInfoDrawable(pDrawable);
	}
	else
	{
		return true;
	}
}

//
//
//
//
CCustomShaderEffectTree::CCustomShaderEffectTree() : CCustomShaderEffectBase(sizeof(*this), CSE_TREE)
{
	// do nothing
}

//
//
//
//
CCustomShaderEffectTree::~CCustomShaderEffectTree()
{
	if(m_pTint)
	{
		delete m_pTint;
		m_pTint = NULL;
	}
}

//
//
//
//
void CCustomShaderEffectTree::InitClass()
{
	InvalidateShaderVarCache();
}

//
//
//
//
void CCustomShaderEffectTree::InvalidateShaderVarCache()
{
	int threadIdx = g_RenderThreadIndex;
	ms_CachedCSETreeShaderVars[threadIdx].bPlayerCollEnabled	= 0xff;	// invalid: neither true(1) nor false(0)
	ms_CachedCSETreeShaderVars[threadIdx].scaleXY				= 0.0f;
	ms_CachedCSETreeShaderVars[threadIdx].scaleZ				= 0.0f;
	ms_CachedCSETreeShaderVars[threadIdx].WorldPlayerPos.Set(0.0f);
	ms_CachedCSETreeShaderVars[threadIdx].umPhaseShift			= 0.0f;

	for(int i=0; i<CCustomShaderEffectTreeType::NUM_COL_VEH; i++)
	{
		ms_CachedCSETreeShaderVars[threadIdx].VehCollEnabled[i]= 0xff;	// 	invalid: neither true(1) nor false(0)
		ms_CachedCSETreeShaderVars[threadIdx].VehColl[i].B		= Vec4V(V_ZERO);
		ms_CachedCSETreeShaderVars[threadIdx].VehColl[i].M		= Vec4V(V_ZERO);
		ms_CachedCSETreeShaderVars[threadIdx].VehColl[i].R		= Vec4V(V_ZERO);
	}

	ms_CachedCSETreeShaderVars[threadIdx].umGlobalOverrideParams.x.SetFloat16_Zero();
	ms_CachedCSETreeShaderVars[threadIdx].umGlobalOverrideParams.y.SetFloat16_Zero();
	ms_CachedCSETreeShaderVars[threadIdx].umGlobalOverrideParams.z.SetFloat16_Zero();
	ms_CachedCSETreeShaderVars[threadIdx].umGlobalOverrideParams.w.SetFloat16_Zero();
}

//
//
//
//
void CCustomShaderEffectTree::EnableShaderVarCaching(bool value)
{
	ms_EnableShaderVarCaching[g_RenderThreadIndex] = value && g_UseTreeShaderVarCaching;

	if(ms_EnableShaderVarCaching[g_RenderThreadIndex])
	{
		InvalidateShaderVarCache();
	}
}

//
//
//
//
void CCustomShaderEffectTree::AddToDrawList(u32 modelIndex, bool bExecute)
{
	CCustomShaderEffectTint *tmpTint = m_pTint; // save cascaded tint ptr
	m_pTint = NULL;		// remove cascaded link for DLC
	DLC(CCustomShaderEffectDC, (*this, modelIndex, bExecute, m_pType));
	m_pTint = tmpTint;	// restore ptr

	if(m_pTint)
	{
		m_pTint->AddToDrawList(modelIndex, bExecute);
	}
}

void CCustomShaderEffectTree::AddToDrawListAfterDraw()
{
	if(m_pTint)
	{
		m_pTint->AddToDrawListAfterDraw();
	}
}

u32 CCustomShaderEffectTree::AddDataForPrototype(void * address)
{
	u32 size = 0;
	DrawListAddress drawListOffset;
	CCustomShaderEffectTint *tmpTint = m_pTint; // save cascaded tint ptr
	m_pTint = NULL;		// remove cascaded link for DLC
	CopyOffShader(this, this->Size(), drawListOffset, false, m_pType);
	drawListOffset = gDCBuffer->LookupSharedData(DL_MEMTYPE_SHADER, GetSharedDataOffset());
	m_pTint = tmpTint;	// restore ptr
	*static_cast<DrawListAddress*>(address) = drawListOffset;
	size += sizeof(DrawListAddress);
	if(m_pTint)
	{
		size += m_pTint->AddDataForPrototype((char*)address + size);
	}
	else
	{
		DrawListAddress nullAddress;
		*reinterpret_cast<DrawListAddress*>((char*)address + size) = nullAddress;
		size += sizeof(DrawListAddress);
	}
	return size;
}

//
//
//
//
void CCustomShaderEffectTree::Update(fwEntity* pEntity)
{
	Assert(pEntity);

	//Doing a prefetch to avoid L2 Cache miss during write operation
	PrefetchDC(&m_umGlobalOverrideParams);

	const Vec3V entityPos = pEntity->GetTransform().GetPosition();
	const ScalarV distFromCamera = CVfxHelper::GetDistSqrToCameraV(entityPos);
	if(m_pTint)
	{
		m_pTint->Update(pEntity);
	}

	// enable players collision only if he is close enough to the tree:
	ScalarV maxPlayerCollisionDist2(m_pType->GetMaxPlayerCollisionDist2());
#if CSE_TREE_EDITABLEVALUES
	if(ms_bEVEnabled)
	{
		maxPlayerCollisionDist2 = ScalarVFromF32(ms_fEVWindGlobalParams.y*ms_fEVWindGlobalParams.y);
	}
#endif

	Vec3V playerPosV3(V_ZERO);
	CPed* pPlayer = CGameWorld::FindLocalPlayer();
	if( pPlayer )
	{
		playerPosV3 = pPlayer->GetTransform().GetPosition();
	}

	const Vec3V playerDist = playerPosV3 - entityPos;
	const ScalarV playerDist2 = MagSquared(playerDist);

	//initial wind amount is 1.0f as we need to have local micromovement even if there is no wind
	ScalarV windAmount(V_ONE);
#if CSE_TREE_EDITABLEVALUES
	if (ms_bEVWindResponseEnabled && IsGreaterThanAll(ms_fEVWindResponseAmount, ScalarV(V_ZERO)))
#else
	if (true)
#endif
	{
		Vec3V wind;

		const ScalarV treeWindMaxDistanceLocalVelSqr = BANK_SWITCH(g_MaxTreeDistForLocalVelocity * g_MaxTreeDistForLocalVelocity, TREE_WIND_MAX_DISTANCE_LOCAL_VELOCITY_SQR);
		const ScalarV treeWindMaxDistanceLocalDisturbanceSqr = BANK_SWITCH(g_MaxTreeDistForLocalWindDisturbance * g_MaxTreeDistForLocalWindDisturbance, TREE_WIND_MAX_DISTANCE_LOCAL_DISTURBANCE_SQR);
		const ScalarV treeWindMaxDistanceGlobalDisturbanceSqr = BANK_SWITCH(g_MaxTreeDistForGlobalDisturbance * g_MaxTreeDistForGlobalDisturbance, TREE_WIND_MAX_DISTANCE_GLOBAL_DISTURBANCE_SQR);
		const ScalarV treeWindMinDistanceIgnoreNonDisturbancesSqr = BANK_SWITCH(g_MinTreeDistForIgnoreNonDisturbance * g_MinTreeDistForIgnoreNonDisturbance, TREE_WIND_MIN_DISTANCE_IGNORE_NON_DISTURBANCE_SQR);


	#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

	#if TREES_USE_ALPHA_CARD_ONLY_BENDING
		if(1)
	#else // TREES_USE_ALPHA_CARD_ONLY_BENDING
		if(m_bHasNewWindShaders)
	#endif // TREES_USE_ALPHA_CARD_ONLY_BENDING
		{
		#if 0 && __BANK
			// LDS DMC TEMP:-
			bool MyTestTree = false;

			if(g_PickerManager.GetSelectedEntity() == pEntity)
			{
				MyTestTree = true;
				Displayf("Got tree....\n");
			}
		#endif //__BANK

			// Don`t update if game is paused.
			if(!fwTimer::IsGamePaused())
			{
				u32 lastUpdated = (u32)m_WindShaderVars.m_OneOverAABBDiagonal.GetWf();

				// Ensure the wind values in the smoother are not stale.
				if(lastUpdated < ms_branchBendEtc_Globals.m_WindSmoothUpdateCount)
				{
					wind = EvaluateWind(pEntity, entityPos);
					u32 count = ms_branchBendEtc_Globals.m_WindSmoothUpdateCount - lastUpdated;

					if(count == 1)
					{
						// Just add a new control point.
						AddWindToSmoother(wind);
					}
					else if(count == 2)
					{
						// "Synthesize" a new previous control point + add the new real one.
						Vec3V randWind = ApplyRandomOffset(wind, 1);
						AddWindToSmoother(randWind);
						AddWindToSmoother(wind);
					}
					else
					{
						// "Synthesize" two previous points +  + add the new real one.
						Vec3V randWind1 = ApplyRandomOffset(wind, 1);
						Vec3V randWind2 = ApplyRandomOffset(wind, 2);
						AddWindToSmoother(randWind2);
						AddWindToSmoother(randWind1);
						AddWindToSmoother(wind);
					}

					// Record last time tree was updated.
					m_WindShaderVars.m_OneOverAABBDiagonal.SetWf((float)ms_branchBendEtc_Globals.m_WindSmoothUpdateCount);
				}

				// Compute smoothed wind.
				Vec3V windToUse = m_WindSmoother.CalculateSmoothedWind(ms_branchBendEtc_Globals.m_WindSmoothNormalisedTime);

				// Pass down to shader variables.
				SetWindVector(windToUse);

				// Entities which are objects can move under physics. We need to re-calculate the underwater blend as it`s based upon the transform.
				if(static_cast < CEntity * >(pEntity)->GetIsTypeObject())
					m_WindShaderVars.m_UnderwaterBlend = CCustomShaderEffectTree::CreateUnderWaterBlend(static_cast < CEntity * >(pEntity));

			#if TREES_INCLUDE_SFX_WIND

				const ScalarV treeSfxWindSqr = BANK_SWITCH(g_MaxTreeDistForSfxWindField * g_MaxTreeDistForSfxWindField, TREE_WIND_MAX_DISTANCE_FOR_SFX_WIND_FIELD_SQR);
				Vec3V playerDistXY = playerDist*Vec3V(1.0f, 1.0f, 0.0f);
				const ScalarV playerDistXYSqr = MagSquared(playerDistXY);

				//if(IsLessThanAll(distFromCamera, treeSfxWindSqr))
				if(IsLessThanAll(playerDistXYSqr, treeSfxWindSqr))
				{
					SfxWindFieldCacheEntry *pCacheEntry = GetCurrentSfxWindFieldCacheEntry(pEntity);

					if(pCacheEntry == NULL)
						pCacheEntry = GetNewSfxWindFieldCacheEntry(pEntity);

					if(pCacheEntry)
					{
						SfxWindFieldValues windFieldValues;
						Vec4V *pDest = &windFieldValues.m_WindField[0];
						Mat34V objectMtx = pEntity->GetMatrix();

						rage::spdAABB boundingBox = pEntity->GetArchetype()->GetBoundingBox();
						Vec3V min = boundingBox.GetMin();
						Vec3V max = boundingBox.GetMax();
						Vec3V diagonal = (max - min)/ScalarV((float)(TREES_SFX_WIND_FIELD_TEXTURE_SIZE - 1));

						Vec3V basis[4];
						basis[0] = SplatX(diagonal)*objectMtx.GetCol0();
						basis[1] = SplatY(diagonal)*objectMtx.GetCol1();
						basis[2] = SplatZ(diagonal)*objectMtx.GetCol2();
						basis[3] = Transform(objectMtx, min);

						Vec3V pz = basis[3];

						// Form a localized wind field round the tree.
						for(u32 k=0; k<TREES_SFX_WIND_FIELD_TEXTURE_SIZE; k++)
						{
							Vec3V py = pz;

							for(u32 j=0; j<TREES_SFX_WIND_FIELD_TEXTURE_SIZE; j++)
							{
								Vec3V px = py;

								for(u32 i=0; i<TREES_SFX_WIND_FIELD_TEXTURE_SIZE; i++)
								{
									Vec3V windAtGridPoint;
									//WIND.GetLocalVelocity(px, windAtGridPoint, true, true, true, false);
									WIND.GetLocalVelocity_DownwashOnly(px, windAtGridPoint);
									*pDest++ = Vec4V(windAtGridPoint);
									px += basis[0];
								}
								py += basis[1];
							}
							pz += basis[2];
						}
						// Update the cache with the values.
						pCacheEntry->SetWindField(windFieldValues);
						// Say use the texture in the cache entry.
						m_SfxWindTextures[GetUpdateBuffer()] = m_SfxWindCacheEntry;
						m_CacheDebug = TREES_SFX_WIND_CACHE_ENTRY_OK;
					}
					else
					{
						// Don`t use any sfx wind.
						m_SfxWindTextures[GetUpdateBuffer()] = -1;
						m_CacheDebug = TREES_SFX_WIND_CACHE_ENTRY_NEED_ONE;
					}
				}
				else
				{
					SfxWindFieldCacheEntry *pCacheEntry = GetCurrentSfxWindFieldCacheEntry(pEntity);

					if(pCacheEntry)
					{
						// Apply zero sfx wind until values are close to zero.
						if(pCacheEntry->CoolDownWindField() == false)
						{
							// Use the texture.
							m_SfxWindTextures[GetUpdateBuffer()] = m_SfxWindCacheEntry;
							m_CacheDebug = TREES_SFX_WIND_CACHE_ENTRY_COOLING_DOWN;
						}
						else
						{
							// Don`t use any sfx wind + release the cache entry.
							ReleaseSfxWindFieldCacheEntry(pCacheEntry);
							m_SfxWindTextures[GetUpdateBuffer()] = -1;
							m_CacheDebug = TREES_SFX_WIND_CACHE_ENTRY_NONE;
						}
					}
					else
					{
						// Don`t use any sfx wind.
						m_SfxWindTextures[GetUpdateBuffer()] = -1;
						m_CacheDebug = TREES_SFX_WIND_CACHE_ENTRY_NONE;
					}
				}
			#endif // TREES_INCLUDE_SFX_WIND
			}
			#if TREES_INCLUDE_SFX_WIND
			else
			{
				SfxWindFieldCacheEntry *pCacheEntry = GetCurrentSfxWindFieldCacheEntry(pEntity);

				if(pCacheEntry)
				{
					// Use current cache entry.
					m_SfxWindTextures[GetUpdateBuffer()] = m_SfxWindCacheEntry;
					// Update the texture.
					pCacheEntry->UpdateTexture();
				}
				else
					m_SfxWindTextures[GetUpdateBuffer()] = -1;
			}
			#endif // TREES_INCLUDE_SFX_WIND
		}
		else if(IsLessThanAll(distFromCamera, treeWindMaxDistanceLocalVelSqr))
		{
			WIND.GetLocalVelocity(entityPos, wind, 
				(IsLessThanAll(distFromCamera, treeWindMaxDistanceLocalDisturbanceSqr)?true:false),
				(IsLessThanAll(distFromCamera, treeWindMaxDistanceGlobalDisturbanceSqr)?true:false),
				(IsLessThanAll(distFromCamera, treeWindMinDistanceIgnoreNonDisturbancesSqr)?true:false)
				);
		}
	#else // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
		//Use cheaper wind functions if trees are far away
		if( IsLessThanAll(distFromCamera, treeWindMaxDistanceLocalVelSqr) )
		{
			WIND.GetLocalVelocity(entityPos, wind, 
				(IsLessThanAll(distFromCamera, treeWindMaxDistanceLocalDisturbanceSqr)?true:false),
				(IsLessThanAll(distFromCamera, treeWindMaxDistanceGlobalDisturbanceSqr)?true:false),
				(IsLessThanAll(distFromCamera, treeWindMinDistanceIgnoreNonDisturbancesSqr)?true:false)
				);
		}
	#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
		else
		{
			WIND.GetGlobalVelocity(WIND_TYPE_AIR, wind);
		}

		//Using local velocity as we have access to each entity's position
		const ScalarV windVel = MagFast(wind);
		const ScalarV windVelMin = BANK_SWITCH(ms_fEVWindResponseVelMin, TREE_DEFAULT_WIND_RESPONSE_VEL_MIN);
		const ScalarV windVelMax = BANK_SWITCH(ms_fEVWindResponseVelMax, TREE_DEFAULT_WIND_RESPONSE_VEL_MAX);
#if __BANK
		const ScalarV windVelExp = BANK_SWITCH(ms_fEVWindResponseVelExp, TREE_DEFAULT_WIND_RESPONSE_VEL_EXP);
#endif

		//Calculate wind response in [0.0, 1.0f]		
		ScalarV windResponse =  Clamp((windVel - windVelMin) / (windVelMax - windVelMin), ScalarV(V_ZERO), ScalarV(V_ONE));
		Assertf(IsEqualAll(TREE_DEFAULT_WIND_RESPONSE_VEL_EXP, ScalarV(V_ZERO)), " TREE_DEFAULT_WIND_RESPONSE_VEL_EXP is not Zero, please remove __BANK macro for !IsZeroAll(windVelExp) condition in CCustomShaderEffectTree::Update");
#if __BANK //Remove __BANK macro if TREE_DEFAULT_WIND_RESPONSE_VEL_EXP != 0.0f
		if (!IsZeroAll(windVelExp))
		{
			windResponse = Pow(windResponse,Pow(ScalarV(V_TWO),	windVelExp));
		}
#endif

		//Scale wind response
		windAmount += windResponse * BANK_SWITCH(ms_fEVWindResponseAmount, TREE_DEFAULT_WIND_RESPONSE_AMOUNT);

		//smoothening strength to avoid jerks in movements
		m_avgWindStrength = m_avgWindStrength * BANK_SWITCH(g_AvgTreeWindStrengthInfluence, TREE_DEFAULT_WIND_STRENGTH_INFLUENCE); 
		m_avgWindStrength += windAmount * (ScalarV(V_ONE) - BANK_SWITCH(g_AvgTreeWindStrengthInfluence, TREE_DEFAULT_WIND_STRENGTH_INFLUENCE));

		// Higher winds causes only higher amplitude
		// Frequency remains the same
		this->SetUmGlobalOverrideParams(Vec4V(m_avgWindStrength, m_avgWindStrength, Vec2V(V_ONE)));
	#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

	#if CSE_TREE_EDITABLEVALUES
		if(g_PickerManager.GetSelectedEntity() == pEntity)
			m_bIsSelectedEntity = true;
		else
	#endif 
			m_bIsSelectedEntity = false;
			

	#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	}
	else
	{
		this->SetUmGlobalOverrideParams(1.0f);
	#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
		this->SetWindVector(Vec3V(V_ZERO));
	#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	}

	m_bPlayerCollEnabled = (u32)(IsLessThanAll(playerDist2,maxPlayerCollisionDist2));

	if(m_pType->IsVehicleDeformable())
	{
		// find 4 nearest vehicles to camera:

		// enable players collision only if he is close enough to the tree:
		float maxVehCollisionDist = m_pType->GetMaxVehicleCollisionDist();
#if CSE_TREE_EDITABLEVALUES
		if(ms_bEVEnabled)
		{
			maxVehCollisionDist = ms_fEVWindGlobalParams.z;
		}
#endif

		CVehicle*	pVehicles[CCustomShaderEffectTreeType::NUM_COL_VEH]		= {NULL,		NULL,		NULL,		NULL		};
		float		fVehicleDist[CCustomShaderEffectTreeType::NUM_COL_VEH]	= {999999.0f,	999999.0f,	999999.0f,	999999.0f	};	// vehicle dist sqr to camera

		//Grab the spatial array.
		const CSpatialArray& rSpatialArray = CVehicle::GetSpatialArray();
		static const int sMaxResults = 20;
		CSpatialArray::FindResult result[sMaxResults];
		int iFound = rSpatialArray.FindInSphere(entityPos, maxVehCollisionDist, &result[0], sMaxResults);

		int iEmptySlot = 0;
		//Iterate over the results.
		for(int iVeh = 0; iVeh < iFound; ++iVeh)
		{
			CVehicle* pVehicle = CVehicle::GetVehicleFromSpatialArrayNode(result[iVeh].m_Node);
			const VehicleType vehType	= pVehicle->GetVehicleType();
			const u32 modelNameHash		= pVehicle? pVehicle->GetBaseModelInfo()->GetModelNameHash() : 0;

			enum { MID_DELUXO	= 0x586765FB };	// "deluxo"

			bool bIsHooveringDeluxo = false;
			if(pVehicle && modelNameHash==MID_DELUXO)
			{
				bIsHooveringDeluxo = pVehicle->HoveringCloseToGround();
			}


			if( (vehType==VEHICLE_TYPE_HELI)		||
				(vehType==VEHICLE_TYPE_PLANE)		||
				(vehType==VEHICLE_TYPE_BOAT && ( pVehicle->GetModelId() == MI_BOAT_SEASHARK || pVehicle->GetModelId() == MI_BOAT_SEASHARK2 || pVehicle->GetModelId() == MI_BOAT_SEASHARK3))
				)
			{
				continue;
			}

			// BS#4081671 - Deluxo - Foliage does not react to the Deluxo while it is in Hover mode
			if(bIsHooveringDeluxo)
			{
				// do nothing - deluxo is hoovering close to the ground
			}
			// BS#2199995 - allow for boats (they have no wheels):
			else if((vehType!=VEHICLE_TYPE_BOAT) && (pVehicle->GetNumContactWheels() < 3))
			{
				continue;
			}

			// allow for vehicle collision when bush and vehicle are (almost) at the same ground level (BS#1312415):
			const Vec3V vehiclePos = pVehicle->GetTransform().GetPosition();
			if( rage::Abs(entityPos.GetZf()-vehiclePos.GetZf()) > 2.0f)
			{
				continue;
			}

			// B*2491123: disable collision for attached vehicles:
			if(pVehicle && pVehicle->GetIsAttached())
			{
				CPhysical* pAttachParent = (CPhysical*)pVehicle->GetAttachParent();
				if(pAttachParent)
				{
					continue;	// no tree collision for attached vehicles
				}
			}

			const float distSqr = result[iVeh].m_DistanceSq;

			if(iEmptySlot < CCustomShaderEffectTreeType::NUM_COL_VEH)
			{
				fVehicleDist[iEmptySlot] = distSqr;
				pVehicles[iEmptySlot] = pVehicle;
				iEmptySlot++;
				continue;
			}

			// find vehicle's index which is most far away from the camera:
			u32		fari	= 0;
			float	farDist	= fVehicleDist[0];
			for(u32 i=1; i<CCustomShaderEffectTreeType::NUM_COL_VEH; i++)
			{
				if( fVehicleDist[i] > farDist )
				{
					farDist = fVehicleDist[i];
					fari	= i;
				}
			}
			Assert(fari < CCustomShaderEffectTreeType::NUM_COL_VEH);

			

			// dist2 smaller than furthest found?
			if( distSqr < farDist )
			{
				fVehicleDist[fari]	= distSqr;
				pVehicles[fari]		= pVehicle;	
			}
		}

		for(u32 v=0; v<CCustomShaderEffectTreeType::NUM_COL_VEH; v++)
		{
			CVehicle *pVehicle = pVehicles[v];
			if( pVehicle )
			{
				const VehicleType vehType = pVehicle->GetVehicleType();

				// grab unmodified (e.g. by opened cardoors, etc.) vehicle's bbox:
				const Vector3 bboxmin = pVehicle->GetBaseModelInfo()->GetBoundingBoxMin(); //pVehicle->GetBoundingBoxMin();
				const Vector3 bboxmax = pVehicle->GetBaseModelInfo()->GetBoundingBoxMax(); //pVehicle->GetBoundingBoxMax();

				// two points: on the front and back of the car:
				float middleX = bboxmin.x + (bboxmax.x - bboxmin.x)*0.5f;
				float middleZ = bboxmin.z + (bboxmax.z - bboxmin.z)*0.5f;

				static dev_float radiusPerc = 1.05f;	// how much make bounding shape "shorter" to fit better around vehicle

				float Radius = (bboxmax.x - bboxmin.x)*0.5f;	// radius
				ScalarV vRadius = ScalarVFromF32(Radius);
				Vector3 ptb(middleX, bboxmax.y - Radius*radiusPerc, middleZ);
				Vector3 pta(middleX, bboxmin.y + Radius*radiusPerc, middleZ);

				// first point on segment:
				Vec3V ptB = VECTOR3_TO_VEC3V(pVehicle->TransformIntoWorldSpace(ptb));
				// last point on segment:
				Vec3V ptA = VECTOR3_TO_VEC3V(pVehicle->TransformIntoWorldSpace(pta));
				// segment vector:
				Vec3V vecM = ptA - ptB;

				float minHitPos = 100000.0f;
				if(vehType == VEHICLE_TYPE_BOAT)
				{
					minHitPos = MIN(bboxmax.z, bboxmin.z);	// boats: work out something as they have no wheels
				}

				// detect groundZ pos (from wheel hit pos):	
				Vector3 wheelHitPos(0,0,minHitPos);
				const s32 numWheels = pVehicle->GetNumWheels();
				for(s32 i=0; i<numWheels; i++)
				{
					if(pVehicle->GetWheel(i))
					{
						// select wheelHitPos with lowest Z (to allow for steep slopes - see BS#1759035):
						Vector3 wheelPos = pVehicle->GetWheel(i)->GetHitPos();
						if(wheelPos.z < wheelHitPos.z)
						{
							wheelHitPos = wheelPos;
						}
					}
				}
				
				static dev_bool bOverrideZ = FALSE;
				DEV_SWITCH(static, static const) ScalarV overrideZ(398.44f); // test for testbed
				ScalarV groundZ = bOverrideZ? overrideZ : ScalarV(wheelHitPos.GetZ());

				DEV_SWITCH(static, static const) ScalarV tweakRadiusScale(V_ONE);		// v1 collision
				SetGlobalVehCollisionParams(v, TRUE, ptB, vecM, vRadius*tweakRadiusScale, groundZ);
			}
			else
			{
				SetGlobalVehCollisionParams(v, FALSE, Vec3V(V_ZERO), Vec3V(V_ZERO), ScalarV(V_ZERO), ScalarV(V_ZERO));
			}
		}//	for(u32 v=0; v<CCustomShaderEffectTreeType::NUM_COL_VEH; v++)...
	}//m_pType->IsVehicleDeformable()...

}// end of Update()...

//
//
//
//
Vec3V CCustomShaderEffectTree::EvaluateWind(fwEntity *pEntity, const Vec3V &entityPos)
{
	Vec3V wind;

	rage::spdAABB boundingBox = static_cast<CEntity *>(pEntity)->GetBaseModelInfo()->GetBoundingBox();
	Vec3V windEvalPos = entityPos + boundingBox.GetMax().GetZ()*Vec3V(V_Z_AXIS_WZERO);

	// Calculate basic wind (grid position variance + gusts).
	WIND.GetLocalVelocity(windEvalPos, wind, true, false, false, true);

#if CSE_TREE_EDITABLEVALUES
	if(ms_branchBendEtc_DebugControl.m_ApplyOverrideWindSpeed)
		wind *= ScalarV(ms_branchBendEtc_DebugControl.m_WindSpeedOverride)*InvMag(wind);
#endif // CSE_TREE_EDITABLEVALUES

	return wind;
}

//
//
//
//
Vec3V CCustomShaderEffectTree::ApplyRandomOffset(Vec3V &baseWind, int randOffset)
{
	ScalarV modBaseWind = Mag(baseWind);
	Vec3V normalizedBaseWind = Vec3V(baseWind/modBaseWind);

	if(modBaseWind.Getf() > 0.01f)
	{
		// Reset the randomizer for this tree.
		ms_WindRandomizer.Reset(*((int *)&m_umPhaseShift) + randOffset);

		// Offset the direction slightly.
		Vec3V windDir = Vec3V(ms_WindRandomizer.GetRanged(-ms_branchBendEtc_Globals.m_WindVariationRange, ms_branchBendEtc_Globals.m_WindVariationRange),
			ms_WindRandomizer.GetRanged(-ms_branchBendEtc_Globals.m_WindVariationRange, ms_branchBendEtc_Globals.m_WindVariationRange),
			ms_WindRandomizer.GetRanged(-ms_branchBendEtc_Globals.m_WindVariationRange, ms_branchBendEtc_Globals.m_WindVariationRange))
			+ normalizedBaseWind;
		windDir = Normalize(windDir);
		return modBaseWind*windDir;
	}
	return baseWind;
}

//
//
//
//
void CCustomShaderEffectTree::AddWindToSmoother(Vec3V &wind)
{
	// When player is inside wind always seems to be zero.
	if(MagSquared(wind).Getf() == 0.0f)
		m_WindSmoother.RecycleLeastRecentControlPoint();
	else
		m_WindSmoother.AcceptNewControlPoint(wind);
}

//
//
//
//
void CCustomShaderEffectTree::SetGlobalVehCollisionParams(u32 n, bool bEnable, Vec3V_In vecB, Vec3V_In vecM, ScalarV_In radius, ScalarV_In groundZ)
{
	Assert(CSystem::IsThisThreadId(SYS_THREAD_UPDATE));
	Assert(n<CCustomShaderEffectTreeType::NUM_COL_VEH);

	m_bVehCollisionEnabled[n] = bEnable;

	Vec4V &vehCollisionB = m_vecVehCollision[n].B; 
	vehCollisionB = Vec4V(vecB, ScalarV(V_ZERO));

	ScalarV dotMM = MagSquared(vecM.GetXY());
	Vec4V &vehCollisionM = m_vecVehCollision[n].M;
	vehCollisionM = Vec4V(vecM, IsGreaterThanAll(dotMM, ScalarV(V_ZERO))? InvertFast(dotMM) : ScalarV(V_ZERO));

	Vec4V &vehCollisionR = m_vecVehCollision[n].R;
	vehCollisionR = Vec4V(radius, radius*radius, IsGreaterThanAll(radius, ScalarV(V_ZERO))?InvertFast(radius * radius):ScalarV(V_ZERO), groundZ);	
}

//
//
//
//
void CCustomShaderEffectTree::DontSetShaderVariables(bool dontSet)
{
#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	ms_DontSetSetShaderVariables = dontSet;
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
}

//
//
//
//
void CCustomShaderEffectTree::SetShaderVariables(rmcDrawable* pDrawable)
{
	if(!pDrawable)
		return;

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	if(ms_DontSetSetShaderVariables)
		return;
#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

#if CSE_TREE_EDITABLEVALUES
	grmShaderGroup *shaderGroup = &pDrawable->GetShaderGroup();
#endif


	// NOTE: we need to set shader vars for debug overlay so that micromovements are correct
	const int threadIdx = g_RenderThreadIndex;

	// need to set these for lod2 rendering into shadow maps
	if(	m_bPlayerCollEnabled || m_bVehCollisionEnabled[0] || m_bVehCollisionEnabled[1] || m_bVehCollisionEnabled[2] || m_bVehCollisionEnabled[3]
		|| m_pType->ContainsLod2d())
	{
		if((!ms_EnableShaderVarCaching[threadIdx]) || ((ms_CachedCSETreeShaderVars[threadIdx].scaleXY != m_scaleXY) || (ms_CachedCSETreeShaderVars[threadIdx].scaleZ != m_scaleZ)))
		{
			if(m_pType->m_idVarEntityScale)
			{
				ms_CachedCSETreeShaderVars[threadIdx].scaleXY	= m_scaleXY;
				ms_CachedCSETreeShaderVars[threadIdx].scaleZ	= m_scaleZ;
				grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarEntityScale, Vec4V(m_scaleXY, m_scaleZ, 1.0f/m_scaleXY, 1.0f/m_scaleZ));
			}
		}
	}

#if !CASCADE_SHADOWS_TREE_MICROMOVEMENTS
	if(DRAWLISTMGR->IsExecutingShadowDrawList() || grmModel::GetForceShader())
	{
		return;
	}
#endif


	if((!ms_EnableShaderVarCaching[threadIdx]) || (ms_CachedCSETreeShaderVars[threadIdx].bPlayerCollEnabled != u8(m_bPlayerCollEnabled)))
	{
		if(m_pType->m_idVarPlayerCollEnabled)
		{
			ms_CachedCSETreeShaderVars[threadIdx].bPlayerCollEnabled = m_bPlayerCollEnabled;
			grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarPlayerCollEnabled, bool(m_bPlayerCollEnabled));
		}
	}

	const Vector3 &playerPos			= CGrassRenderer::GetGlobalPlayerPos();
	const bool bSetUmGlobalPhaseShift	= (!ms_EnableShaderVarCaching[threadIdx]) || (ms_CachedCSETreeShaderVars[threadIdx].umPhaseShift != m_umPhaseShift);
	const bool bSetWorldPlayerPos		= m_bPlayerCollEnabled && ((!ms_EnableShaderVarCaching[threadIdx]) || (ms_CachedCSETreeShaderVars[threadIdx].WorldPlayerPos != playerPos));
	if(bSetUmGlobalPhaseShift || bSetWorldPlayerPos)
	{
		if(m_pType->m_idVarWorldPlayerPos_umGlobalPhaseShift)
		{
			ms_CachedCSETreeShaderVars[threadIdx].WorldPlayerPos	= playerPos;
			ms_CachedCSETreeShaderVars[threadIdx].umPhaseShift		= m_umPhaseShift;
			const Vec4V value = Vec4V(playerPos.x, playerPos.y, playerPos.z, m_umPhaseShift);
			grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarWorldPlayerPos_umGlobalPhaseShift, value);
		}
	}

	
	// vehicle collision:
	if(m_pType->IsVehicleDeformable())
	{
		for(u32 i=0; i<CCustomShaderEffectTreeType::NUM_COL_VEH; i++)
		{
			if((!ms_EnableShaderVarCaching[threadIdx]) || (ms_CachedCSETreeShaderVars[threadIdx].VehCollEnabled[i] != u8(m_bVehCollisionEnabled[i])))
			{
				if(m_pType->m_idVarVehCollEnabled[i])
				{
					ms_CachedCSETreeShaderVars[threadIdx].VehCollEnabled[i] = m_bVehCollisionEnabled[i];
					grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarVehCollEnabled[i], m_bVehCollisionEnabled[i]);
				}
			}

			if(m_bVehCollisionEnabled[i])
			{
				if(	(!ms_EnableShaderVarCaching[threadIdx])													||
					(!IsEqualAll(ms_CachedCSETreeShaderVars[threadIdx].VehColl[i].B, m_vecVehCollision[i].B))	||
					(!IsEqualAll(ms_CachedCSETreeShaderVars[threadIdx].VehColl[i].M, m_vecVehCollision[i].M))	||
					(!IsEqualAll(ms_CachedCSETreeShaderVars[threadIdx].VehColl[i].R, m_vecVehCollision[i].R))	)
				{
					if(m_pType->m_idVarVehColl[i])
					{
						ms_CachedCSETreeShaderVars[threadIdx].VehColl[i] = m_vecVehCollision[i];
						grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarVehColl[i], (Vec4V*)(&(m_vecVehCollision[i])), 3);
					}
				}			
			}
		}
	}
	else
	{
		for(u32 i=0; i<CCustomShaderEffectTreeType::NUM_COL_VEH; i++)
		{
			if((!ms_EnableShaderVarCaching[threadIdx]) || (ms_CachedCSETreeShaderVars[threadIdx].VehCollEnabled[i] != u8(false)))
			{
				if(m_pType->m_idVarVehCollEnabled[i])
				{
					ms_CachedCSETreeShaderVars[threadIdx].VehCollEnabled[i] = false;
					grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarVehCollEnabled[i], false);
				}
			}
		}
	}

	if((!ms_EnableShaderVarCaching[threadIdx]) || (ms_CachedCSETreeShaderVars[threadIdx].umGlobalOverrideParams != m_umGlobalOverrideParams))
	{
		if(m_pType->m_idVarUmGlobalOverrideParams)
		{
			ms_CachedCSETreeShaderVars[threadIdx].umGlobalOverrideParams = m_umGlobalOverrideParams;

		#if !TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
			const Vec4V umOverrideParamsV4 = Vec4V(
						m_umGlobalOverrideParams.x.GetFloat32_FromFloat16(),
						m_umGlobalOverrideParams.y.GetFloat32_FromFloat16(),
						m_umGlobalOverrideParams.z.GetFloat32_FromFloat16(),
						m_umGlobalOverrideParams.w.GetFloat32_FromFloat16()	);
			grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->m_idVarUmGlobalOverrideParams, umOverrideParamsV4);
		#endif // !TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
		}
	}
	
#if CSE_TREE_EDITABLEVALUES
	CCustomShaderEffectTree::SetEditableShaderValues(shaderGroup, pDrawable, m_pType);
#endif

#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
	CCustomShaderEffectTree::SetBranchBendAndTriWaveMicromovementParams(pDrawable);
#endif
}// end of SetShaderVariables()...


// um global override params:
void CCustomShaderEffectTree::SetUmGlobalOverrideParams(float scaleH, float scaleV, float argFreqH, float argFreqV)
{
	m_umGlobalOverrideParams.x.SetFloat16_FromFloat32(scaleH);
	m_umGlobalOverrideParams.y.SetFloat16_FromFloat32(scaleV);
	m_umGlobalOverrideParams.z.SetFloat16_FromFloat32(argFreqH);
	m_umGlobalOverrideParams.w.SetFloat16_FromFloat32(argFreqV);
}

void CCustomShaderEffectTree::SetUmGlobalOverrideParams(Vec4V_In scaleArgFreq)
{
	m_umGlobalOverrideParams.x.SetFloat16_FromFloat32(scaleArgFreq.GetX());
	m_umGlobalOverrideParams.y.SetFloat16_FromFloat32(scaleArgFreq.GetY());
	m_umGlobalOverrideParams.z.SetFloat16_FromFloat32(scaleArgFreq.GetZ());
	m_umGlobalOverrideParams.w.SetFloat16_FromFloat32(scaleArgFreq.GetW());
}

// um global override params:
void CCustomShaderEffectTree::SetUmGlobalOverrideParams(float s)
{
Float16 s16(s);
	m_umGlobalOverrideParams.x =
	m_umGlobalOverrideParams.y =
	m_umGlobalOverrideParams.z =
	m_umGlobalOverrideParams.w = s16;
}

void CCustomShaderEffectTree::GetUmGlobalOverrideParams(float *pScaleH, float *pScaleV, float *pArgFreqH, float *pArgFreqV)
{
	if(pScaleH)
	{
		*pScaleH = m_umGlobalOverrideParams.x.GetFloat32_FromFloat16(); 
	}

	if(pScaleV)
	{
		*pScaleV = m_umGlobalOverrideParams.y.GetFloat32_FromFloat16();
	}

	if(pArgFreqH)
	{
		*pArgFreqH = m_umGlobalOverrideParams.z.GetFloat32_FromFloat16();
	}

	if(pArgFreqV)
	{
		*pArgFreqV = m_umGlobalOverrideParams.w.GetFloat32_FromFloat16();
	}
}

//
//
//
//
u32 CCustomShaderEffectTree::GetNumTintPalettes()
{
	if(m_pTint)
	{
		return m_pTint->GetNumTintPalettes();
	}

	return(0);
}

//
//
//
//
bool CCustomShaderEffectTree::HasPerBatchShaderVars() const
{
	return CCustomShaderEffectBase::HasPerBatchShaderVars() || (m_pTint ? m_pTint->HasPerBatchShaderVars() : false);
}

Vector4 *CCustomShaderEffectTree::WriteInstanceData(grcVec4BufferInstanceBufferList &ibList, Vector4 *pDest, const InstancedEntityCache &entityCache, u32 alpha) const
{
	//Write base data
	if(Vector4 *ib = CCustomShaderEffectBase::WriteInstanceData(ibList, pDest, entityCache, alpha))
	{
		//LOD2D shaders also need inverse scaleXY and scaleZ. Rather than adding a constant, we will pack these in existing instance data.
		(ib-1)->SetW(1.0f / m_scaleXY);
		ib[0] = Vector4(1.0f,1.0f,1.0f, 1.0f / m_scaleZ);
		return ib + sNumAdditionalRegsPerInst;
	}

	return NULL;
}

namespace CSETreeStatic
{
	DECLARE_MTR_THREAD grcRasterizerStateHandle sPrevRSBlock = grcStateBlock::RS_Invalid;
	void CacheRSBlock()
	{
		Assert(sysThreadType::IsRenderThread());
		Assertf(sPrevRSBlock == grcStateBlock::RS_Invalid, "WARNING! CSETreeStatic::CacheRSBlock() is trying to push a state block before popping the previously pushed block.");
		sPrevRSBlock = grcStateBlock::RS_Active;
	}

	void RestoreCachedRSBlock()
	{
		Assert(sysThreadType::IsRenderThread());
		Assertf(sPrevRSBlock != grcStateBlock::RS_Invalid,	"WARNING! CSETreeStatic::RestoreCachedRSBlock() is trying to restore an invalid state block. Make sure a matching CacheRSBlock " 
															"was called 1st and that RestoreCachedRSBlock was not called multiple times in a row without a call to CacheRSBlock in between.");
		grcStateBlock::SetRasterizerState(sPrevRSBlock);
		sPrevRSBlock = grcStateBlock::RS_Invalid;
	}
}

void CCustomShaderEffectTree::AddBatchRenderStateToDrawList() const
{
	//Setup tree render states
	const bool isShadowDrawList = static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingShadowDrawList();

	if(isShadowDrawList)
		DLC_Add(&CRenderPhaseCascadeShadowsInterface::SetRasterizerState, CSM_RS_TWO_SIDED);
	else
	{
		DLC_Add(&CSETreeStatic::CacheRSBlock);
		DLC_Add(&CShaderLib::SetFacingBackwards, false);
		DLC_Add(&CShaderLib::SetDisableFaceCulling);
	}
}

void CCustomShaderEffectTree::AddBatchRenderStateToDrawListAfterDraw() const
{
	//Restore render states
	const bool isShadowDrawList = static_cast<CDrawListMgr *>(gDrawListMgr)->IsBuildingShadowDrawList();

	if(isShadowDrawList)
		DLC_Add(&CRenderPhaseCascadeShadowsInterface::RestoreRasterizerState);
	else
		DLC_Add(&CSETreeStatic::RestoreCachedRSBlock);
}

#if CSE_TREE_EDITABLEVALUES
//
//
//
//
void CCustomShaderEffectTree::SetEditableShaderValues(grmShaderGroup *pShaderGroup, rmcDrawable* UNUSED_PARAM(pDrawable), CCustomShaderEffectTreeType* pType)
{
	Assert(pShaderGroup);
	Assert(pType);

	if(!ms_bEVEnabled)
		return;

	SetVector4ShaderGroupVar(pShaderGroup, "umGlobalParams0",			ms_fEVumGlobalParams);

	SetVector4ShaderGroupVar(pShaderGroup, "windGlobalParams0",			ms_fEVWindGlobalParams);

	if(ms_bEVUseUmGlobalOverrideValues)
	{
		if(pType->m_idVarUmGlobalOverrideParams)
		{
			grcEffect::SetGlobalVar((grcEffectGlobalVar)pType->m_idVarUmGlobalOverrideParams, RCC_VEC3V(ms_fEVUmGlobalOverrideParams));
		}
	}

}// end of SetEditableShaderValues()...



//
//
//
//
bool CCustomShaderEffectTree::InitWidgets(bkBank& bank)
{
	bank.PushGroup("Trees", true);
		bank.AddSeparator("CSETree:");
		bank.AddToggle("Use tree shaders var caching", &g_UseTreeShaderVarCaching);

		// debug widgets:
		bank.PushGroup("Tree Editable Shaders Values", false);
			bank.AddToggle("Enable",	&ms_bEVEnabled);
			bank.PushGroup("Micro-movements", true);
				bank.AddSlider("x: Global Scale H    (default: 0.025)",			&ms_fEVumGlobalParams.x, 0.0f, 0.5f, 0.001f, NullCB, "x: global horizontal scale: how big amplitude of movement is (lower=smaller, higher=bigger)");
				bank.AddSlider("y: Global Scale V    (default: 0.020)",			&ms_fEVumGlobalParams.y, 0.0f, 0.5f, 0.001f, NullCB, "y: global vertical scale: how big amplitude of movement is (lower=smaller, higher=bigger)");
				bank.AddSlider("z: Global Arg Freq H    (default: 1.000)",		&ms_fEVumGlobalParams.z, 0.0f, 2.0f, 0.001f, NullCB, "z: global horizontal frequency: how quickly plants move (lower=slower, higher=quicker)");
				bank.AddSlider("w: Global Arg Freq V    (default: 0.500)",		&ms_fEVumGlobalParams.w, 0.0f, 2.0f, 0.001f, NullCB, "w: global vertical frequency: how quickly plants move (lower=slower, higher=quicker)");
				bank.AddToggle("Use Global uMovement Override Values", &ms_bEVUseUmGlobalOverrideValues);
				bank.AddSlider("Global Override ScaleH    (default: 1.000)",	&ms_fEVUmGlobalOverrideParams.x,	0.0f, 10.0f, 0.001f);
				bank.AddSlider("Global Override ScaleV    (default: 1.000)",	&ms_fEVUmGlobalOverrideParams.y,	0.0f, 10.0f, 0.001f);
				bank.AddSlider("Global Override ArgFreqH  (default: 1.000)",	&ms_fEVUmGlobalOverrideParams.z,	0.0f, 10.0f, 0.001f);
				bank.AddSlider("Global Override ArgFreqV  (default: 1.000)",	&ms_fEVUmGlobalOverrideParams.w,	0.0f, 10.0f, 0.001f);
			bank.PopGroup();
			bank.AddSlider("Global player collision radius (default: 5.0)",		&ms_fEVWindGlobalParams.y,0.0f,250.0f, 0.1f, NullCB, "global player collision radius");
			bank.AddSlider("Global vehicle collision radius (default: 5.0)",	&ms_fEVWindGlobalParams.z,0.0f,250.0f, 0.1f, NullCB, "global vehicle collision radius");
		bank.PopGroup();

		bank.PushGroup("Tree Wind Movement", true);
			bank.AddToggle("Response Enabled",					&ms_bEVWindResponseEnabled);
			bank.AddSlider("Response Amount",					&ms_fEVWindResponseAmount,		0.0f, 50.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Min",					&ms_fEVWindResponseVelMin,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Max",					&ms_fEVWindResponseVelMax,		0.0f, 64.0f, 1.0f/256.0f);
			bank.AddSlider("Response Vel Exp",					&ms_fEVWindResponseVelExp,		-4.0f, 4.0f, 1.0f/256.0f);
			bank.AddSlider("Average Wind Strength Influence",	&g_AvgTreeWindStrengthInfluence,0.0f,  1.0f, 0.001f);
			bank.AddSeparator();
			bank.AddSlider("Max Distance for Local Wind Disturbances",	&g_MaxTreeDistForLocalWindDisturbance,	0.0f, 1000.0f, 0.01f);
			bank.AddSlider("Max Distance for Global Wind Disturbances",	&g_MaxTreeDistForGlobalDisturbance,		0.0f, 1000.0f, 0.01f);
			bank.AddSlider("Min Distance for Ignoring Non Disturbances",&g_MinTreeDistForIgnoreNonDisturbance,	0.0f, 1000.0f, 0.01f);
			bank.AddSlider("Max Distance for Local Wind Velocity",		&g_MaxTreeDistForLocalVelocity,			0.0f, 1000.0f, 0.01f);
		bank.PopGroup();

	#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS
		bank.PushGroup("New Wind stuff", true);

			bank.AddButton("Load global settings", CCustomShaderEffectTree::OnButtonLoad);
			bank.AddButton("Save global settings", CCustomShaderEffectTree::OnButtonSave);

			bank.AddTitle("------------------------ Alpha card only cut-down version ------------------------");
			bank.AddSlider("Alpha card global stiffness",				&ms_branchBendEtc_Globals.m_AlphaCardOnlyGlobalStiffness, 0.0f, 1.0f, 0.01f);

			bank.AddTitle("------------------------ Wind smoothing & Speed ------------------------");
			bank.AddSlider("Wind control point interval",				&ms_branchBendEtc_Globals.m_WindSmoothChangeControlPointInterval, 1.0f/60.0f, 2.0f, 0.01f);
			bank.AddToggle("Wind speed override",						&ms_branchBendEtc_DebugControl.m_ApplyOverrideWindSpeed);
			bank.AddSlider("Wind speed",								&ms_branchBendEtc_DebugControl.m_WindSpeedOverride, 0.0f, 200.0f, 0.01f);
			bank.AddSlider("Soft clamp max",							&ms_branchBendEtc_Globals.m_WindSpeedSoftClamp, 0.0f, 200.0f, 0.01f);
			bank.AddSlider("Unrestricted proportion",					&ms_branchBendEtc_Globals.m_WindSpeedSoftClampUnrestrictedProportion, 0.0f, 0.99f, 0.01f);

			bank.AddTitle("------------------------ Water transition ------------------------");
			bank.AddSlider("Water transition range (applied on tree creation)", &ms_branchBendEtc_Globals.m_UnderWaterTransitionRange, 0.0f, 10.0f, 0.01f);

			bank.AddTitle("------------------------ Debug control ------------------------");
			bank.AddToggle("New wind Enabled",							&ms_branchBendEtc_DebugControl.m_enabled);
			bank.AddToggle("Disable ALL branch bending",				&ms_branchBendEtc_DebugControl.m_NoBranchBending);
			bank.AddToggle("Disable normal wind",						&ms_branchBendEtc_DebugControl.m_NoRegularWind);
			bank.AddToggle("Disable branch phase variations",			&ms_branchBendEtc_DebugControl.m_NoBranchBendingPhaseVariations);
			bank.AddToggle("Disable foliage phase variations",			&ms_branchBendEtc_DebugControl.m_NoAlphaCardVariation);
			bank.AddToggle("Disable micro-movements",					&ms_branchBendEtc_DebugControl.m_NoumMovements);
			bank.AddToggle("Disable sfx wind",							&ms_branchBendEtc_DebugControl.m_NoSfxWind);
			bank.AddToggle("Force under water movement",				&ms_branchBendEtc_DebugControl.m_ForceUnderWater);
			bank.AddToggle("Force above water movement",				&ms_branchBendEtc_DebugControl.m_ForceAboveWater);

			bank.AddTitle("------------------------ Debug render ------------------------");
			bank.AddCombo("Debug render type", &CCustomShaderEffectTree::ms_DebugRenderMode, TREES_DEBUG_RENDER_MODES_MAX, &ms_DebugRenderModes[0], 0, datCallback(CFA(CCustomShaderEffectTree::NullFunction)));
			bank.AddSlider("Stiffness render range",					&ms_branchBendEtc_DebugControl.m_DebugRenderStiffnessRange,	0.0f, 1.0f, 0.01f);

			// Shader would be locals.
			bank.AddTitle("------------------------ MAX Shader variables ------------------------");

			bank.AddToggle("Redirect To Selected Entity",		&ms_branchBendEtc_DebugControl.m_RedirectToSelectedEntity);
			bank.AddToggle("Apply to all entities",				&ms_branchBendEtc_DebugControl.m_ApplyToAllEntities);

			bank.AddSlider("pivot height",						&ms_branchBendEtc_Locals.m_PivotHeight,	0.0f, 50.0f, 0.01f);
			bank.AddSlider("trunk stiffness adjust low",		&ms_branchBendEtc_Locals.m_TrunkStiffnessAdjustLow, 0.0f, 30.0f, 0.01f);
			bank.AddSlider("trunk stiffness adjust high",		&ms_branchBendEtc_Locals.m_TrunkStiffnessAdjustHigh, 0.0f, 30.0f, 0.01f);
			bank.AddSlider("phase stiffness adjust low",		&ms_branchBendEtc_Locals.m_PhaseStiffnessAdjustLow, 0.0f, 30.0f, 0.01f);
			bank.AddSlider("phase stiffness adjust high",		&ms_branchBendEtc_Locals.m_PhaseStiffnessAdjustHigh, 0.0f, 30.0f, 0.01f);

			for(int i=0; i<TREES_NUMBER_OF_BASIS_WAVES; i++)
			{
				char lowWidgetName[256];
				char highWidgetName[256];
				sprintf(lowWidgetName, "low wind %d basis wave", i);
				sprintf(highWidgetName, "high wind %d basis wave", i);
				bank.AddVector(lowWidgetName, &ms_branchBendEtc_Locals.m_BasisWaves[i].lowWind.freqAndAmp, 0.0f, 0.5f, 0.001f);
				bank.AddVector(highWidgetName, &ms_branchBendEtc_Locals.m_BasisWaves[i].highWind.freqAndAmp, 0.0f, 0.5f, 0.001f);
			}

			bank.AddTitle("----Alpha card only version----");
			bank.AddSlider("global stiffness multiplier",		&ms_branchBendEtc_Locals.m_GlobalStiffnessMultiplier, 0.0f, 2.0f, 0.01f);

			bank.AddButton("Load MAX varibles", CCustomShaderEffectTree::OnMAXShaderVarsLoad);
			bank.AddButton("Save MAX varibles", CCustomShaderEffectTree::OnMAXShaderVarsSave);

			//  Shader globals.
			bank.AddTitle("------------------------ Shader globals ------------------------");

			// um low/high control.
			bank.AddToggle("Override um high/low blend",		&ms_branchBendEtc_DebugControl.m_OverrideumLowHighBlend);
			bank.AddSlider("um high/low blend override",		&ms_branchBendEtc_DebugControl.m_umLowHighBlendOverride,	0.0f, 1.0f, 0.01f);
			bank.AddSlider("um low wind speed",					&ms_branchBendEtc_Globals.m_umLowWind,	0.0f, 30.0f, 0.01f);
			bank.AddSlider("um high wind speed",				&ms_branchBendEtc_Globals.m_umHighWind,	0.0f, 30.0f, 0.01f);

			// Wind variation.
			bank.AddSlider("wind variation range (offsets from a unit vector)",	&ms_branchBendEtc_Globals.m_WindVariationRange,	0.0f, 1.0f, 0.01f);
			bank.AddSlider("wind variation 1 scale",			&ms_branchBendEtc_Globals.m_WindVariationScales[0],	0.0f, 1.0f, 0.01f);
			bank.AddSlider("wind variation 2 scale",			&ms_branchBendEtc_Globals.m_WindVariationScales[1],	0.0f, 1.0f, 0.01f);
			bank.AddSlider("wind variation 3 scale",			&ms_branchBendEtc_Globals.m_WindVariationScales[2],	0.0f, 1.0f, 0.01f);

			bank.AddSlider("wind variation blend freq 1",		&ms_branchBendEtc_Globals.m_WindVariationBlendWaveFreq[0], 0.0f, 0.5f, 0.01f);
			bank.AddSlider("wind variation blend freq 2",		&ms_branchBendEtc_Globals.m_WindVariationBlendWaveFreq[1], 0.0f, 0.5f, 0.01f);

		#if TREES_INCLUDE_SFX_WIND
			bank.AddTitle("------------------------ Sfx wind ------------------------");
			bank.AddSlider("Radius",							&g_MaxTreeDistForSfxWindField, 0.0f, 100.0f, 0.1f);
			bank.AddSlider("Evaluation Displacement Amp.",		&ms_branchBendEtc_SfxGlobals.m_SfxWindEvalModulationDisplacementAmp, 0.0f, 5.0f, 0.1f);
			bank.AddVector("Frequencies",						&ms_branchBendEtc_SfxGlobals.m_SfxWindEvalModulationFreq, 0.0f, 2.5f, 0.001f);
			bank.AddSlider("Value modulation proportion.",		&ms_branchBendEtc_SfxGlobals.m_SfxWindValueModulationProportion, 0.0f, 1.0f, 0.1f);
			bank.AddSlider("Cache max accrn.",					&ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, 0.0f, 30.0f, 0.1f);
		#endif // TREES_INCLUDE_SFX_WIND

			bank.AddTitle("------------------------ Old Cone and fall off ------------------------");
			bank.AddSlider("stiffness",							&ms_branchBendEtc_Locals.m_Stiffness, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("foliage stiffness",					&ms_branchBendEtc_Locals.m_FoliageStiffness, 0.0f, 1.0f, 0.01f);
			bank.AddSlider("cone angle",						&ms_branchBendEtc_Locals.m_ConeAngle, 0.0f, PI, 0.01f);
			bank.AddSlider("fall off radius",					&ms_branchBendEtc_Locals.m_FallOffRadius, 0.0f, 100.0f, 0.01f);

		bank.PopGroup();
	#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

	bank.PopGroup();
	return(TRUE);
}// end of InitWidgets()...

#endif //CSE_TREE__EDITABLEVALUES...


#if TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

void CCustomShaderEffectTree::Init()
{
	LoadParserFiles();

#if TREES_INCLUDE_SFX_WIND
	InitialiseSfxWindFieldCache();
#endif // TREES_INCLUDE_SFX_WIND
}

void CCustomShaderEffectTree::LoadParserFiles()
{
	PARSER.LoadObject("common:/data/BranchBend_WindSettings", "xml", ms_branchBendEtc_Globals);
#if TREES_INCLUDE_SFX_WIND
	PARSER.LoadObject("common:/data/BranchBend_SfxWindSettings", "xml", ms_branchBendEtc_SfxGlobals);
#endif // TREES_INCLUDE_SFX_WIND
}

#if CSE_TREE_EDITABLEVALUES

void CCustomShaderEffectTree::SaveParserFiles()
{
	PARSER.SaveObject("common:/data/BranchBend_WindSettings", "xml", &ms_branchBendEtc_Globals, parManager::XML);
#if TREES_INCLUDE_SFX_WIND
	PARSER.SaveObject("common:/data/BranchBend_SfxWindSettings", "xml", &ms_branchBendEtc_SfxGlobals, parManager::XML);
#endif // TREES_INCLUDE_SFX_WIND
}

void CCustomShaderEffectTree::OnButtonLoad()
{
	LoadParserFiles();
}

void CCustomShaderEffectTree::OnButtonSave()
{
	SaveParserFiles();
}

#endif // CSE_TREE_EDITABLEVALUES

void CCustomShaderEffectTree::UpdateWindStuff()
{
	float deltaT = TIME.GetSeconds();

	ms_branchBendEtc_Globals.m_WindSmoothTime += deltaT;

	while(ms_branchBendEtc_Globals.m_WindSmoothTime >= ms_branchBendEtc_Globals.m_WindSmoothChangeControlPointInterval)
	{
		ms_branchBendEtc_Globals.m_WindSmoothTime -= ms_branchBendEtc_Globals.m_WindSmoothChangeControlPointInterval;
		ms_branchBendEtc_Globals.m_WindSmoothUpdateCount++;
	}
	ms_branchBendEtc_Globals.m_WindSmoothNormalisedTime = ms_branchBendEtc_Globals.m_WindSmoothTime/ms_branchBendEtc_Globals.m_WindSmoothChangeControlPointInterval;

#if TREES_INCLUDE_SFX_WIND
	UpdateSfxWindCache();
#endif // TREES_INCLUDE_SFX_WIND
}


void CCustomShaderEffectTree::SetWindVector(Vec3V windVector)
{
	Vec4V windVectors[4];
	ScalarV modWind = Mag(windVector);

	if(modWind.Getf() > 0.01f)
	{
		float umBlend;

	#if CSE_TREE_EDITABLEVALUES
		if(ms_branchBendEtc_DebugControl.m_OverrideumLowHighBlend)
		{
			// Use value set by RAG widget.
			umBlend = ms_branchBendEtc_DebugControl.m_umLowHighBlendOverride; 
		}
		else
	#endif // CSE_TREE_EDITABLEVALUES
		{
			// Calculate um high low blend using wind speed.
			float windSpeed = modWind.Getf();
			umBlend = Min(1.0f, Max(0.0f, (windSpeed - ms_branchBendEtc_Globals.m_umLowWind)/(ms_branchBendEtc_Globals.m_umHighWind - ms_branchBendEtc_Globals.m_umLowWind)));
		}

		windVectors[0] = Vec4V(windVector.GetXf(), windVector.GetYf(), windVector.GetZf(), 0.0f);
		Vec4V normalizedWind = Vec4V(windVectors[0]/modWind);

		// Reset the randomizer for this tree.
		ms_WindRandomizer.Reset(*((int *)&m_umPhaseShift));

		// Create 3 further wind vectors slightly offset from the main wind vector.
		for(int i=1; i<4; i++)
		{
			Vec4V windDir = Vec4V(ms_WindRandomizer.GetRanged(-ms_branchBendEtc_Globals.m_WindVariationRange, ms_branchBendEtc_Globals.m_WindVariationRange),
				ms_WindRandomizer.GetRanged(-ms_branchBendEtc_Globals.m_WindVariationRange, ms_branchBendEtc_Globals.m_WindVariationRange),
				ms_WindRandomizer.GetRanged(-ms_branchBendEtc_Globals.m_WindVariationRange, ms_branchBendEtc_Globals.m_WindVariationRange),
				0.0f) + normalizedWind;
			windDir = Normalize(windDir);
			windVectors[i] = modWind*ScalarV(ms_branchBendEtc_Globals.m_WindVariationScales[i-1])*windDir;
		}

		windVectors[0].SetWf(umBlend);
		windVectors[1].SetWf(ms_branchBendEtc_Globals.m_WindVariationBlendWaveFreq[0]);
		windVectors[2].SetWf(ms_branchBendEtc_Globals.m_WindVariationBlendWaveFreq[1]);
		windVectors[3].SetWf(m_umPhaseShift);
	}
	else
	{
		for(int i=0; i<4; i++)
			windVectors[i] = Vec4V(V_ZERO);

		windVectors[0].SetWf(0.0f);
		windVectors[1].SetWf(ms_branchBendEtc_Globals.m_WindVariationBlendWaveFreq[0]);
		windVectors[2].SetWf(ms_branchBendEtc_Globals.m_WindVariationBlendWaveFreq[1]);
		windVectors[3].SetWf(m_umPhaseShift);
	}

	// These should be double buffered, but a bit of "tearing" won`t make much difference (wind vectors only change slightly from frame to frame)
	m_WindShaderVars.m_WindVectors[0] = windVectors[0];
	m_WindShaderVars.m_WindVectors[1] = windVectors[1];
	m_WindShaderVars.m_WindVectors[2] = windVectors[2];
	m_WindShaderVars.m_WindVectors[3] = windVectors[3];
}


Vec4V CCustomShaderEffectTree::CreateUnderWaterBlend(CEntity *pEntity)
{
	float waterHeight = CGameWorldWaterHeight::GetWaterHeightAtPos(pEntity->GetTransform().GetPosition());
	const fwTransform &trans = pEntity->GetTransform();

	Vec4V toWorldSpaceZ = trans.GetA().GetZ()*Vec4V(V_X_AXIS_WZERO) 
		+ trans.GetB().GetZ()*Vec4V(V_Y_AXIS_WZERO) 
		+ trans.GetC().GetZ()*Vec4V(V_Z_AXIS_WZERO) 
		+ trans.GetPosition().GetZ()*Vec4V(V_ZERO_WONE);
	toWorldSpaceZ -= Vec4V(0.0f, 0.0f, 0.0f, waterHeight);
	toWorldSpaceZ *= ScalarV(1.0f/CCustomShaderEffectTree::ms_branchBendEtc_Globals.m_UnderWaterTransitionRange);
	return toWorldSpaceZ;
}


#if TREES_INCLUDE_SFX_WIND

void CCustomShaderEffectTree::InitialiseSfxWindFieldCache()
{
	if(ms_pSfxWindFieldCache == NULL)
		ms_pSfxWindFieldCache = rage_new SfxWindFieldCacheEntry[TREES_SFX_WIND_FIELD_CACHE_SIZE];
}


void CCustomShaderEffectTree::UpdateSfxWindCache()
{
	if(ms_pSfxWindFieldCache)
	{
		// Any cache entries not used this frame mark as free (it`s only small).
		for(int i=0; i<TREES_SFX_WIND_FIELD_CACHE_SIZE; i++)
			if(ms_pSfxWindFieldCache[i].m_TickLabelWhenLastUsed != ms_UpdateTickLabel)
				ms_pSfxWindFieldCache[i].m_pEntity = NULL;
	}
	ms_UpdateTickLabel++;
}


CCustomShaderEffectTree::SfxWindFieldCacheEntry *CCustomShaderEffectTree::GetCurrentSfxWindFieldCacheEntry(fwEntity *pEntity)
{
	if(m_SfxWindCacheEntry == -1)
		return NULL;

	SfxWindFieldCacheEntry *pCacheEntry = &ms_pSfxWindFieldCache[m_SfxWindCacheEntry];

	// Is the entry associated with the entity ?
	if(pCacheEntry->m_pEntity == pEntity)
	{
		// Say it`s been used this frame.
		pCacheEntry->m_TickLabelWhenLastUsed = ms_UpdateTickLabel;
		// We`re ok to use this as is.
		return pCacheEntry;
	}
	return NULL;
}


CCustomShaderEffectTree::SfxWindFieldCacheEntry *CCustomShaderEffectTree::GetNewSfxWindFieldCacheEntry(fwEntity *pEntity)
{
	for(int i=0; i<TREES_SFX_WIND_FIELD_CACHE_SIZE; i++)
	{
		SfxWindFieldCacheEntry *pCacheEntry = &ms_pSfxWindFieldCache[i];

		if(pCacheEntry->m_pEntity == NULL)
		{
			// Say it`s been used this frame.
			pCacheEntry->m_TickLabelWhenLastUsed = ms_UpdateTickLabel;
			// Bind it to the entity.
			pCacheEntry->m_pEntity = pEntity;
			m_SfxWindCacheEntry = (s8)i;
			pCacheEntry->Reset();
			return pCacheEntry;
		}
	}
	// No free slots.
	return NULL;
}


void CCustomShaderEffectTree::ReleaseSfxWindFieldCacheEntry(SfxWindFieldCacheEntry *pCacheEntry)
{
	pCacheEntry->m_TickLabelWhenLastUsed = 0;
	pCacheEntry->m_pEntity = NULL;
}

#endif // TREES_INCLUDE_SFX_WIND

void CCustomShaderEffectTree::SetBranchBendAndTriWaveMicromovementParams(rmcDrawable* pDrawable)
{
	grmShaderGroup *shaderGroup = &pDrawable->GetShaderGroup();
	(void)shaderGroup;

#if TREES_INCLUDE_SFX_WIND
	// Set the eval and value modulation.
	Vec4V sfxWindEvalModulation = Vec4V(ms_branchBendEtc_SfxGlobals.m_SfxWindEvalModulationFreq.x, ms_branchBendEtc_SfxGlobals.m_SfxWindEvalModulationFreq.y, ms_branchBendEtc_SfxGlobals.m_SfxWindEvalModulationFreq.z, ms_branchBendEtc_SfxGlobals.m_SfxWindEvalModulationDisplacementAmp);
	Vec4V sfxWindValueModulation = Vec4V(ms_branchBendEtc_SfxGlobals.m_SfxWindValueModulationProportion, 0.0f, 0.0f, 0.0f);

	if(m_pType->ms_idVar_branchBendEtc_SfxWindEvalModulation != grcegvNONE)
		grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->ms_idVar_branchBendEtc_SfxWindEvalModulation, sfxWindEvalModulation);

	if(m_pType->ms_idVar_branchBendEtc_SfxWindValueModulation != grcegvNONE)
		grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->ms_idVar_branchBendEtc_SfxWindValueModulation, sfxWindValueModulation);

	s8 textureIndex = m_SfxWindTextures[GetRenderBuffer()];

	float UseTexture = 0.0f;
	grcTexture *pTexture = NULL;

	// Set the wind field texture.
	if(textureIndex >= 0)
	{
		pTexture = ms_pSfxWindFieldCache[textureIndex].m_pWindFieldTextures[GetRenderBuffer()];
		UseTexture = 1.0f;
	}
	else
		pTexture = ms_pSfxWindFieldCache[0].m_pWindFieldTextures[GetRenderBuffer()]; // Just bind the 1st 3d texture (fixed some d3d debug run-time errors)...It won`t get read from.

	if(m_pType->m_idLocalVar_branchBendEtc_SfxWindField != grmsgvNONE)
		shaderGroup->SetVar(m_pType->m_idLocalVar_branchBendEtc_SfxWindField, pTexture);

	m_WindShaderVars.m_AABBMin.SetWf(UseTexture);

	if(m_pType->ms_idVar_branchBendEtc_AABBInfo != grcegvNONE)
		grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->ms_idVar_branchBendEtc_AABBInfo, &m_WindShaderVars.m_AABBMin, 2);
#endif // TREES_INCLUDE_SFX_WIND

	Vec4V windSpeedSoftClamp = Vec4V(ms_branchBendEtc_Globals.m_WindSpeedSoftClampUnrestrictedProportion*ms_branchBendEtc_Globals.m_WindSpeedSoftClamp, (1.0f - ms_branchBendEtc_Globals.m_WindSpeedSoftClampUnrestrictedProportion)*ms_branchBendEtc_Globals.m_WindSpeedSoftClamp, ms_branchBendEtc_Globals.m_AlphaCardOnlyGlobalStiffness, 0.0f);
	Vec4V underwaterTransition = m_WindShaderVars.m_UnderwaterBlend;
	grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->ms_idVar_branchBendEtc_WindVector, m_WindShaderVars.m_WindVectors, 4);
	grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->ms_idVar_branchBendEtc_WindSpeedSoftClamp, windSpeedSoftClamp);
			
#if CSE_TREE_EDITABLEVALUES
	if(ms_branchBendEtc_DebugControl.m_ForceUnderWater)
		underwaterTransition = Vec4V(V_ZERO);
	if(ms_branchBendEtc_DebugControl.m_ForceAboveWater)
		underwaterTransition = Vec4V(V_ZERO_WONE);
#endif // CSE_TREE_EDITABLEVALUES

	grcEffect::SetGlobalVar((grcEffectGlobalVar)m_pType->ms_idVar_branchBendEtc_UnderWaterTransition, underwaterTransition);

	Vec4V debugRender_PixelControl1 =  Vec4V(V_ZERO);
	Vec4V debugRender_PixelControl2 =  Vec4V(V_ZERO);
	Vec4V debugRender_PixelControl3 =  Vec4V(V_ZERO);

	Vector4 control1(1.0f, 1.0f, 1.0f, 1.0f);
	Vector4 control2(1.0f, 1.0f, 1.0f, 0.0f);

#if CSE_TREE_EDITABLEVALUES
	control1 = Vector4(ms_branchBendEtc_DebugControl.m_enabled ? 1.0f : 0.0f, ms_branchBendEtc_DebugControl.m_NoBranchBending ? 0.0f : 1.0f, ms_branchBendEtc_DebugControl.m_NoumMovements ? 0.0f : 1.0f, ms_branchBendEtc_DebugControl.m_NoBranchBendingPhaseVariations ? 0.0f : 1.0f);
	control2 = Vector4(ms_branchBendEtc_DebugControl.m_NoRegularWind ? 0.0f : 1.0f, ms_branchBendEtc_DebugControl.m_NoSfxWind ? 0.0f : 1.0f, ms_branchBendEtc_DebugControl.m_NoAlphaCardVariation ? 0.0f : 1.0f, 0.0f);

	switch ((TreeDebugRenderMode)ms_DebugRenderMode)
	{
	case NONE:
		{
			debugRender_PixelControl1 = Vec4V(V_ZERO);
			debugRender_PixelControl2 = Vec4V(V_ZERO);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	case TRUNK_STIFFNESS:
		{
			debugRender_PixelControl1 = Vec4V(0.0f, 0.0f, 0.0f, 1.0f);
			debugRender_PixelControl2 = Vec4V(1.0f, 0.0f, ms_branchBendEtc_DebugControl.m_DebugRenderStiffnessRange, 0.0f);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	case PHASE_STIFFNESS:
		{
			debugRender_PixelControl1 = Vec4V(0.0f, 0.0f, 0.0f, 1.0f);
			debugRender_PixelControl2 = Vec4V(0.0f, 0.0f, ms_branchBendEtc_DebugControl.m_DebugRenderStiffnessRange, 1.0f);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	case PHASE:	
		{
			debugRender_PixelControl1 = Vec4V(0.0f, 1.0f, 0.0f, 1.0f);
			debugRender_PixelControl2 = Vec4V(0.0f, 0.0f, 0.0f, 0.0f);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	case FOLIAGE_PHASE:
		{
			debugRender_PixelControl1 = Vec4V(0.0f, 0.0f, 0.0f, 1.0f);
			debugRender_PixelControl2 = Vec4V(0.0f, 1.0f, 0.0f, 0.0f);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	case UM_AMPLITUDE_H:
		{
			debugRender_PixelControl1 = Vec4V(1.0f, 0.0f, 0.0f, 1.0f);
			debugRender_PixelControl2 = Vec4V(0.0f, 0.0f, 0.0f, 0.0f);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	case UM_AMPLITUDE_V:
		{
			debugRender_PixelControl1 = Vec4V(0.0f, 0.0f, 1.0f, 1.0f);
			debugRender_PixelControl2 = Vec4V(0.0f, 0.0f, 0.0f, 0.0f);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	case SFX_CACHE:
		{
			debugRender_PixelControl1 = Vec4V(V_ZERO_WONE);
			debugRender_PixelControl2 = Vec4V(V_ZERO);
			debugRender_PixelControl3 = Vec4V(V_ZERO);

		#if TREES_INCLUDE_SFX_WIND
			switch(m_CacheDebug)
			{
			case TREES_SFX_WIND_CACHE_ENTRY_OK:
				{
					debugRender_PixelControl3 = Vec4V(0.0f, 1.0f, 0.0f, 1.0f);
					break;
				}
			case TREES_SFX_WIND_CACHE_ENTRY_COOLING_DOWN:
				{
					debugRender_PixelControl3 = Vec4V(1.0f, 1.0f, 0.0f, 1.0f);
					break;
				}
			case TREES_SFX_WIND_CACHE_ENTRY_NEED_ONE:
				{
					debugRender_PixelControl3 = Vec4V(1.0f, 0.0f, 0.0f, 1.0f);
					break;
				}
			case TREES_SFX_WIND_CACHE_ENTRY_NONE:
			default:
				{
					debugRender_PixelControl3 = Vec4V(0.25f, 0.25f, 0.25f, 1.0f);
					break;
				}
			}
		#endif // TREES_INCLUDE_SFX_WIND

			break;
		}
	default:
		{
			debugRender_PixelControl1 = Vec4V(V_ZERO);
			debugRender_PixelControl2 = Vec4V(V_ZERO);
			debugRender_PixelControl3 = Vec4V(V_ZERO);
			break;
		}
	}
#endif // CSE_TREE_EDITABLEVALUES

	if(m_pType->ms_idVar_branchBendEtc_Control1 != grcegvNONE)
		grcEffect::SetGlobalVar(m_pType->ms_idVar_branchBendEtc_Control1, RCC_VEC4V(control1));
	if(m_pType->ms_idVar_branchBendEtc_Control2 != grcegvNONE)
		grcEffect::SetGlobalVar(m_pType->ms_idVar_branchBendEtc_Control2, RCC_VEC4V(control2));

	if(m_pType->ms_idVar_branchBendEtc_DebugRenderControl1 != grcegvNONE)
		grcEffect::SetGlobalVar(m_pType->ms_idVar_branchBendEtc_DebugRenderControl1, debugRender_PixelControl1);
	if(m_pType->ms_idVar_branchBendEtc_DebugRenderControl2 != grcegvNONE)
		grcEffect::SetGlobalVar(m_pType->ms_idVar_branchBendEtc_DebugRenderControl2, debugRender_PixelControl2);
	if(m_pType->ms_idVar_branchBendEtc_DebugRenderControl3 != grcegvNONE)
		grcEffect::SetGlobalVar(m_pType->ms_idVar_branchBendEtc_DebugRenderControl3, debugRender_PixelControl3);

#if CSE_TREE_EDITABLEVALUES
	// Override shader locals.
	SetEditableBranchBendAndTriWaveMicromovementParams(shaderGroup, pDrawable, m_pType, m_bIsSelectedEntity);
#endif // CSE_TREE_EDITABLEVALUES
}

#if CSE_TREE_EDITABLEVALUES

void CCustomShaderEffectTree::SetEditableBranchBendAndTriWaveMicromovementParams(grmShaderGroup *pShaderGroup, rmcDrawable* pDrawable, CCustomShaderEffectTreeType* pType, bool isSelectedEntity)
{
	(void)pDrawable;
	(void)pShaderGroup;

	Vec4V branchBendStiffness;
	Vec4V branchBendFoliageStiffness;
	Vec4V freqAndAmplitude[TREES_NUMBER_OF_BASIS_WAVES];

	// Set triangle wave params.
	for(int i=0; i<TREES_NUMBER_OF_BASIS_WAVES; i++)
		freqAndAmplitude[i] = Vec4V(ms_branchBendEtc_Locals.m_BasisWaves[i].lowWind.freqAndAmp.x, ms_branchBendEtc_Locals.m_BasisWaves[i].lowWind.freqAndAmp.y, ms_branchBendEtc_Locals.m_BasisWaves[i].highWind.freqAndAmp.x, ms_branchBendEtc_Locals.m_BasisWaves[i].highWind.freqAndAmp.y);

	// Set "stiffness" variables.
	branchBendStiffness = Vec4V(1.0f - ms_branchBendEtc_Locals.m_Stiffness, ms_branchBendEtc_Locals.m_PivotHeight, cosf(ms_branchBendEtc_Locals.m_ConeAngle), 1.0f/Max(0.01f, ms_branchBendEtc_Locals.m_FallOffRadius));
	branchBendFoliageStiffness = Vec4V(1.0f - ms_branchBendEtc_Locals.m_FoliageStiffness, 0.0f, 0.0f, 0.0f);

	if((ms_branchBendEtc_DebugControl.m_RedirectToSelectedEntity && isSelectedEntity) || ms_branchBendEtc_DebugControl.m_ApplyToAllEntities)
	{
		if(pType->m_idLocalVar_umTriWave1Params != grmsgvNONE)
			pShaderGroup->SetVar(pType->m_idLocalVar_umTriWave1Params, freqAndAmplitude[0]);
		if(pType->m_idLocalVar_umTriWave2Params != grmsgvNONE)
			pShaderGroup->SetVar(pType->m_idLocalVar_umTriWave2Params, freqAndAmplitude[1]);
		if(pType->m_idLocalVar_umTriWave3Params != grmsgvNONE)
			pShaderGroup->SetVar(pType->m_idLocalVar_umTriWave3Params, freqAndAmplitude[2]);

		if(pType->m_idLocalVar_branchBendPivot != grmsgvNONE)
		{
			Vec4V bendPivot = Vec4V(ms_branchBendEtc_Locals.m_PivotHeight, 0.0f, 0.0f, 0.0f);
			pShaderGroup->SetVar(pType->m_idLocalVar_branchBendPivot, bendPivot);
		}
		if(pType->m_idLocalVar_branchBendStiffnessAdjust != grmsgvNONE)
		{
			Vec4V stiffnessAdjust = Vec4V(ms_branchBendEtc_Locals.m_TrunkStiffnessAdjustLow, ms_branchBendEtc_Locals.m_TrunkStiffnessAdjustHigh,  ms_branchBendEtc_Locals.m_PhaseStiffnessAdjustLow, ms_branchBendEtc_Locals.m_PhaseStiffnessAdjustHigh);
			pShaderGroup->SetVar(pType->m_idLocalVar_branchBendStiffnessAdjust, stiffnessAdjust);
		}

		if(pType->m_idLocalVar_foliageBranchBendPivot != grmsgvNONE)
		{
			Vec4V bendPivot = Vec4V(ms_branchBendEtc_Locals.m_PivotHeight, 0.0f, 0.0f, 0.0f);
			pShaderGroup->SetVar(pType->m_idLocalVar_foliageBranchBendPivot, bendPivot);
		}
		if(pType->m_idLocalVar_foliageBranchBendStiffnessAdjust != grmsgvNONE)
		{
			Vec4V stiffnessAdjust = Vec4V(ms_branchBendEtc_Locals.m_TrunkStiffnessAdjustLow, ms_branchBendEtc_Locals.m_TrunkStiffnessAdjustHigh,  ms_branchBendEtc_Locals.m_PhaseStiffnessAdjustLow, ms_branchBendEtc_Locals.m_PhaseStiffnessAdjustHigh);
			pShaderGroup->SetVar(pType->m_idLocalVar_foliageBranchBendStiffnessAdjust, stiffnessAdjust);
		}

		if(pType->m_idLocalVar_stiffnessMultiplier != grmsgvNONE)
		{
			Vec4V stiffnessMult = Vec4V(0.0f, 0.0f, ms_branchBendEtc_Locals.m_GlobalStiffnessMultiplier, 0.0f);
			pShaderGroup->SetVar(pType->m_idLocalVar_stiffnessMultiplier, stiffnessMult);
		}
	}
	else
	{
		if(pType->ms_idVar_branchBend_Stiffness != grcegvNONE)
			grcEffect::SetGlobalVar(pType->ms_idVar_branchBend_Stiffness, branchBendStiffness);
		if(pType->ms_idVar_branchBend_FoliageStiffness != grcegvNONE)
			grcEffect::SetGlobalVar(pType->ms_idVar_branchBend_FoliageStiffness, branchBendFoliageStiffness);
		if(pType->ms_idVar_triuMovement_FreqAndAmp != grcegvNONE)
			grcEffect::SetGlobalVar(pType->ms_idVar_triuMovement_FreqAndAmp, &freqAndAmplitude[0], TREES_NUMBER_OF_BASIS_WAVES);
	}
} // end of SetTriMicroMovementParams()...


void CCustomShaderEffectTree::OnMAXShaderVarsLoad()
{
	PARSER.LoadObject("common:/data/BranchBend_MAXShadersetting", "xml", ms_branchBendEtc_Locals);
}


void CCustomShaderEffectTree::OnMAXShaderVarsSave()
{
	PARSER.SaveObject("common:/data/BranchBend_MAXShadersetting", "xml", &ms_branchBendEtc_Locals, parManager::XML);
}


#endif // CSE_TREE_EDITABLEVALUES

#endif // TREES_ENABLE_BRANCH_BEND_AND_TRI_WAVE_MICRO_MOVEMENTS

//----------------------------------------------------------------------------------------

#if TREES_INCLUDE_SFX_WIND

CCustomShaderEffectTree::SfxWindFieldCacheEntry::SfxWindFieldCacheEntry()
{
	m_TickLabelWhenLastUsed = 0;
	m_pEntity = (fwEntity *)NULL;


	//TREES_USE_UPDATE_SUBRESOURCE_FOR_SFX_WIND_TEXTURE:
	grcTextureFactory::TextureCreateParams param(grcTextureFactory::TextureCreateParams::VIDEO,
		grcTextureFactory::TextureCreateParams::LINEAR,	grcsWrite, NULL,
		grcTextureFactory::TextureCreateParams::NORMAL, grcTextureFactory::TextureCreateParams::MSAA_NONE);

	if(RSG_PC && __D3D11)
	{
		param.LockFlags = grcsWrite | grcsDiscard; // Interprets grcsWrite | grcsDiscard as use dynamic texture on PC.

		#if RSG_PC && __D3D11
			param.ThreadUseHint = grcTextureFactory::TextureCreateParams::THREAD_USE_HINT_CAN_BE_UPDATE_THREAD;
		#endif // RSG_PC && __D3D11
	}


	for(int i=0; i<CSE_TREE_MAX_TEXTURES; i++)
	{
		BANK_ONLY(char szName[255];)
		BANK_ONLY(formatf(szName, "Sfx Wind Field %d", i);)
		BANK_ONLY(grcTexture::SetCustomLoadName(szName);)
		m_pWindFieldTextures[i] = grcTextureFactory::GetInstance().Create(
			(u32)TREES_SFX_WIND_FIELD_TEXTURE_SIZE, (u32)TREES_SFX_WIND_FIELD_TEXTURE_SIZE, (u32)TREES_SFX_WIND_FIELD_TEXTURE_SIZE, 
			grctfA32B32G32R32F, (void *)NULL, &param);
		BANK_ONLY(grcTexture::SetCustomLoadName(NULL);)
	}
}


CCustomShaderEffectTree::SfxWindFieldCacheEntry::~SfxWindFieldCacheEntry()
{
	for(int i=0; i<CSE_TREE_MAX_TEXTURES; i++)
	{
		if(m_pWindFieldTextures[i])
		{
			m_pWindFieldTextures[i]->Release();
			m_pWindFieldTextures[i] = NULL;
		}
	}
}


void CCustomShaderEffectTree::SfxWindFieldCacheEntry::Reset()
{
	m_Values.Reset();
}


void CCustomShaderEffectTree::SfxWindFieldCacheEntry::SetWindField(SfxWindFieldValues &values)
{
	Vec4V *pToAttain = &values.m_WindField[0];
	Vec4V *pCurrent = &m_Values.m_WindField[0];

	Vec4V maxAccrn = Vec4V(ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, 0.0f);
	Vec4V negMaxAccrn = Vec4V(-ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, -ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, -ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, 0.0f);

	for(int i=0; i<TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE; i++)
	{
		Vec4V accrn = (pToAttain[0] - pCurrent[0])*TIME.GetInvSecondsV();
		accrn = Max(Min(accrn, maxAccrn), negMaxAccrn);
		Vec4V newValue = pCurrent[0] + accrn*TIME.GetSecondsV();
		*pCurrent++ = newValue;
		pToAttain++;
	}

	// Update the texture.
	UpdateTexture();
}


bool CCustomShaderEffectTree::SfxWindFieldCacheEntry::CoolDownWindField()
{
	const float epsilon = 0.01f;

	bool ret = false;
	Vec4V toAttain = Vec4V(V_ZERO);
	Vec4V epsilonV = Vec4V(epsilon, epsilon, epsilon, epsilon);
	Vec4V *pCurrent = &m_Values.m_WindField[0];
	VecBoolV andLessthan(V_TRUE);

	Vec4V maxAccrn = Vec4V(ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, 0.0f);
	Vec4V negMaxAccrn = Vec4V(-ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, -ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, -ms_branchBendEtc_SfxGlobals.m_SfxMaxAccrn, 0.0f);

	for(int i=0; i<TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE*TREES_SFX_WIND_FIELD_TEXTURE_SIZE; i++)
	{
		Vec4V accrn = (toAttain - pCurrent[0])*TIME.GetInvSecondsV();
		accrn = Max(Min(accrn, maxAccrn), negMaxAccrn);
		Vec4V newValue = pCurrent[0] + accrn*TIME.GetSecondsV();
		*pCurrent++ = newValue;

		newValue = Abs(newValue);
		andLessthan = And(andLessthan, IsLessThan(newValue, epsilonV));
	}

	if(IsTrueAll(andLessthan))
		ret = true;

	// Update the texture.
	UpdateTexture();
	return ret;
}


void CCustomShaderEffectTree::SfxWindFieldCacheEntry::UpdateTexture()
{
	grcTextureLock textureLock;

	// Lock the texture.
	if(m_pWindFieldTextures[GetUpdateBuffer()]->LockRect(0, 0, textureLock, grcsWrite))
	{
		Vec4V *pSrc = &m_Values.m_WindField[0];
		char *pZ = (char *)textureLock.Base;

		// Copy in the wind field data.
		for(int k=0; k<TREES_SFX_WIND_FIELD_TEXTURE_SIZE; k++)
		{
			char *pY = pZ;

			for(int j=0; j<TREES_SFX_WIND_FIELD_TEXTURE_SIZE; j++)
			{
				memcpy(pY, pSrc, sizeof(Vec4V)*TREES_SFX_WIND_FIELD_TEXTURE_SIZE);
				pSrc += TREES_SFX_WIND_FIELD_TEXTURE_SIZE;
				pY += textureLock.Pitch;
			}
			pZ += textureLock.Pitch*textureLock.Height;
		}
		// Unlock the texture.
		m_pWindFieldTextures[GetUpdateBuffer()]->UnlockRect(textureLock);
	}
}

#endif // TREES_INCLUDE_SFX_WIND


