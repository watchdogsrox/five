//
// name:		WeaponModelInfo.cpp
// description:	File containing class describing weapon model info objects
// Written by:	Adam Fowler

#include "modelinfo/WeaponModelInfo.h"

// Rage headers
#include "crskeleton/skeletondata.h"
#include "fwanimation/animmanager.h"
#include "rmcore/drawable.h"
#include "phbound/BoundSphere.h"
#include "phbound/BoundComposite.h"
#include "phcore/MaterialMgr.h"
#include "phcore/phmath.h"
#include "physics/archetype.h"
#include "physics/tunable.h"

// Framework headers
#include "fwanimation\expressionsets.h"
#include "vfx/ptfx/ptfxmanager.h"

// game headers
#include "physics/gtaArchetype.h"
#include "physics/GtaMaterial.h"
#include "physics/floater.h"
#include "modelinfo/weaponmodelinfodata_parser.h"
#include "fwscene/stores/drawablestore.h"
#include "rmcore/drawable.h"
#include "shaders/CustomShaderEffectBase.h"
#include "shaders/CustomShaderEffectWeapon.h"
#include "scene/texLod.h"
#include "weapons/weaponchannel.h"

WEAPON_OPTIMISATIONS()

fwPtrListSingleLink CWeaponModelInfo::ms_HDStreamRequests;

CWeaponModelInfo::InitData::InitData()
{
	Init();
}

void CWeaponModelInfo::InitData::Init()
{
	m_lodDist = 0.0f;
}

//
// name:		CWeaponModelInfo::Init
// description:	Initialise class
void CWeaponModelInfo::Init()
{
	CBaseModelInfo::Init();

	 m_type = MI_TYPE_WEAPON;

	for(int i=0; i<MAX_WEAPON_NODES; i++)
		m_nBoneIndices[i] = -1;

	m_HDDrwbIdx = -1;
	m_HDtxdIdx  = -1;
	m_HDDistance = 0.0f;

	m_bRequestLoadHDFiles            = false;
	m_bAreHDFilesLoaded	             = false;	
	m_bRequestHDFilesRemoval         = false;
	m_bDampingTunedViaTunableObjects = false;

	m_numHDUpdateRefs = 0;
	m_numHDRenderRefs = 0;
}


void CWeaponModelInfo::Init(const InitData & rInitData)
{
	m_bRequestLoadHDFiles            = false;
	m_bAreHDFilesLoaded	             = false;	
	m_bRequestHDFilesRemoval         = false;
	m_bDampingTunedViaTunableObjects = false;

	m_pHDShaderEffectType = NULL;

	s32 parentTxdSlot = FindTxdSlotIndex(rInitData.m_txdName.c_str()).Get();
	this->SetDrawableOrFragmentFile(rInitData.m_modelName.c_str(), parentTxdSlot, false);

	// setup the particle effect asset
	if (stricmp(rInitData.m_ptfxAssetName.c_str(), "null"))
	{
		strLocalIndex slot = ptfxManager::GetAssetStore().FindSlotFromHashKey(atStringHash(rInitData.m_ptfxAssetName.c_str()));
		if (Verifyf(slot.IsValid(), "%s is trying to load a PTFX asset (%s) which doesn't exist", GetModelName(), rInitData.m_ptfxAssetName.c_str()))
		{
			SetPtFxAssetSlot(slot.Get());
		}
	}

	this->SetLodDistance(rInitData.m_lodDist);
	m_HDDistance = rInitData.m_HDDistance;

	if (m_HDDistance > 0.0f)
	{
		SetupHDFiles(rInitData.m_modelName);
	}
	else
	{
		m_HDDrwbIdx = -1;
		m_HDtxdIdx  = -1;
		m_HDDistance = 0.0f;
	}

	SetHasBoundInDrawable();	// assume weapon bounds are in drawable

	const u32 s_HashOfNullString = ATSTRINGHASH("null", 0x03adb3357);

	// Expression set checking.  If you've set an expression set to use, then the specific:
	// rInitData.m_ExpressionDictionaryName and
	// rInitData.m_ExpressionName
	// should both be empty/null.  Expression sets are for peds only at the moment.
	if(rInitData.m_ExpressionSetName && rInitData.m_ExpressionSetName != s_HashOfNullString)
	{
		Assertf(!rInitData.m_ExpressionDictionaryName || rInitData.m_ExpressionDictionaryName == s_HashOfNullString, "Expression Set (%s) should not be specified if expression dictionary is also specified", rInitData.m_ExpressionSetName.GetCStr());
		Assertf(!rInitData.m_ExpressionName || rInitData.m_ExpressionName == s_HashOfNullString, "Expression Set (%s) should not be specified if expression name is also specified", rInitData.m_ExpressionSetName.GetCStr());
	}

	if(rInitData.m_ExpressionSetName && rInitData.m_ExpressionSetName != s_HashOfNullString)
	{
		fwMvExpressionSetId expressionSetId = fwExpressionSetManager::GetExpressionSetId(rInitData.m_ExpressionSetName);
		Assertf(expressionSetId != EXPRESSION_SET_ID_INVALID, "%s:Unrecognized expression set", rInitData.m_ExpressionSetName.GetCStr());
		this->SetExpressionSet(expressionSetId);
	}

	this->SetExpressionDictionaryIndex(rInitData.m_ExpressionDictionaryName);
	this->SetExpressionHash(rInitData.m_ExpressionName);
}


//
// name:		CWeaponModelInfo::Shutdown
// description:	Shutdown class
void CWeaponModelInfo::Shutdown()
{
	CBaseModelInfo::Shutdown();
}

//
// name:		SetAtomic
// description:	Set atomic for weapon modelinfo
void CWeaponModelInfo::InitMasterDrawableData(u32 modelIdx)
{
	CBaseModelInfo::InitMasterDrawableData(modelIdx);
	ConfirmHDFiles();
	SetBoneIndicies();
}

void CWeaponModelInfo::DeleteMasterDrawableData()
{
	modelinfoAssert(GetNumHdRefs() == 0);

	if(m_pHDShaderEffectType)
	{
		rmcDrawable *pDrawable = (m_HDDrwbIdx != -1) ? g_DrawableStore.Get(strLocalIndex(m_HDDrwbIdx)) : NULL; 
		if(pDrawable)
		{
			// check txds are set as expected
			Assertf(g_TxdStore.HasObjectLoaded(strLocalIndex(GetAssetParentTxdIndex())), "archetype %s : unexpected txd state", GetModelName());
			m_pHDShaderEffectType->RestoreModelInfoDrawable(pDrawable);
		}

		m_pHDShaderEffectType->RemoveRef();
		m_pHDShaderEffectType = NULL;
	}

	CBaseModelInfo::DeleteMasterDrawableData();
}

void CWeaponModelInfo::DestroyCustomShaderEffects()
{
	modelinfoAssert(GetNumHdRefs() == 0);

	if(m_pHDShaderEffectType)
	{
		rmcDrawable *pDrawable = (m_HDDrwbIdx != -1) ? g_DrawableStore.Get(strLocalIndex(m_HDDrwbIdx)) : NULL; 
		if(pDrawable)
		{
			// check txds are set as expected
			Assertf(g_TxdStore.HasObjectLoaded(strLocalIndex(GetAssetParentTxdIndex())), "archetype %s : unexpected txd state", GetModelName());
			m_pHDShaderEffectType->RestoreModelInfoDrawable(pDrawable);
		}

		m_pHDShaderEffectType->RemoveRef();
		m_pHDShaderEffectType = NULL;
	}

	CBaseModelInfo::DestroyCustomShaderEffects();
}

// Use a larger than normal angular velocity damping to force curved weapons (RPG launcher) to settle faster 
#define DEFAULT_WEAPON_LINEAR_C_DRAG_COEFF (0.0f)
#define DEFAULT_WEAPON_LINEAR_V_DRAG_COEFF (0.0f)
#define DEFAULT_WEAPON_LINEAR_V2_DRAG_COEFF (0.05f)
#define DEFAULT_WEAPON_ANGULAR_C_DRAG_COEFF (0.02f)
#define DEFAULT_WEAPON_ANGULAR_V_DRAG_COEFF (0.1f)
#define DEFAULT_WEAPON_ANGULAR_V2_DRAG_COEFF (0.1f)

//
void CWeaponModelInfo::SetPhysics(phArchetypeDamp* pPhysics)
{
	SetPhysicsOnly(pPhysics); 
	if(pPhysics != NULL)
	{
		if(pPhysics->GetBound())
		{
			UpdateBoundingVolumes(*pPhysics->GetBound());
		}

		pPhysics->SetTypeFlags(ArchetypeFlags::GTA_PROJECTILE_TYPE);
		pPhysics->SetIncludeFlags(ArchetypeFlags::GTA_PROJECTILE_INCLUDE_TYPES);

		static dev_float s_fMass = 3.0f;
		float fMass = s_fMass;
		pPhysics->SetMass(fMass);
		pPhysics->SetAngInertia(pPhysics->GetBound()->GetComputeAngularInertia(fMass));

		phBound* pBound = pPhysics->GetBound();
		Assert(pBound);
		if(pBound->GetType() == phBound::COMPOSITE)
		{
			phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pBound);
			if(pBoundComp->GetTypeAndIncludeFlags())
			{
				for(int i = 0; i < pBoundComp->GetNumBounds(); ++i)
				{
					// Set the type and include flags for this component.
					pBoundComp->SetTypeFlags(i, ArchetypeFlags::GTA_PROJECTILE_TYPE);
					pBoundComp->SetIncludeFlags(i, INCLUDE_FLAGS_ALL);
				}
			}
		}

		// Set up weapon damping values. Projectiles override this in CProjectile::CProjectile(). 
		pPhysics->ActivateDamping(phArchetypeDamp::LINEAR_C,  Vector3(DEFAULT_WEAPON_LINEAR_C_DRAG_COEFF, DEFAULT_WEAPON_LINEAR_C_DRAG_COEFF, DEFAULT_WEAPON_LINEAR_C_DRAG_COEFF));
		pPhysics->ActivateDamping(phArchetypeDamp::LINEAR_V,  Vector3(DEFAULT_WEAPON_LINEAR_V_DRAG_COEFF, DEFAULT_WEAPON_LINEAR_V_DRAG_COEFF, DEFAULT_WEAPON_LINEAR_V_DRAG_COEFF));
		pPhysics->ActivateDamping(phArchetypeDamp::LINEAR_V2, Vector3(DEFAULT_WEAPON_LINEAR_V2_DRAG_COEFF,DEFAULT_WEAPON_LINEAR_V2_DRAG_COEFF,DEFAULT_WEAPON_LINEAR_V2_DRAG_COEFF));
		pPhysics->ActivateDamping(phArchetypeDamp::ANGULAR_C,  Vector3(DEFAULT_WEAPON_ANGULAR_C_DRAG_COEFF, DEFAULT_WEAPON_ANGULAR_C_DRAG_COEFF, DEFAULT_WEAPON_ANGULAR_C_DRAG_COEFF));
		pPhysics->ActivateDamping(phArchetypeDamp::ANGULAR_V,  Vector3(DEFAULT_WEAPON_ANGULAR_V_DRAG_COEFF, DEFAULT_WEAPON_ANGULAR_V_DRAG_COEFF, DEFAULT_WEAPON_ANGULAR_V_DRAG_COEFF));
		pPhysics->ActivateDamping(phArchetypeDamp::ANGULAR_V2, Vector3(DEFAULT_WEAPON_ANGULAR_V2_DRAG_COEFF,DEFAULT_WEAPON_ANGULAR_V2_DRAG_COEFF,DEFAULT_WEAPON_ANGULAR_V2_DRAG_COEFF));

		if (GetIsTypeObject() && !GetIsFixed())
		{
			if (const CTunableObjectEntry* pTuning = CTunableObjectManager::GetInstance().GetTuningEntry(GetModelNameHash()))
			{
				//ComputeMass(pPhysics, pTuning); TODO: ought to call ComputeMass here, but skipping it for now to reduce the risk of a late-project submit
				if (ComputeDamping(pPhysics, pTuning))
				{
					m_bDampingTunedViaTunableObjects = true;
				}
			}
		}
		if(m_pBuoyancyInfo == NULL)
		{
			InitWaterSamples();
		}

		static dev_float MAX_SPEED = 600.0f;
		pPhysics->SetMaxSpeed(MAX_SPEED);
	}
}


static const char* weaponBoneNames[MAX_WEAPON_NODES + 1] = 
{
	"gun_root",
	"gun_gripr",
	"gun_gripl",
	"gun_muzzle",
	"gun_vfx_eject",
	"gun_magazine",
	"gun_ammo",
	"gun_vfx_projtrail",
	"gun_barrels",
	"WAPClip",
	"WAPClip_2",
	"WAPClip_3",
	"WAPScop",
	"WAPScop_2",
	"WAPScop_3",
	"WAPGrip",
	"WAPGrip_2",
	"WAPGrip_3",
	"WAPSupp",
	"WAPSupp_2",
	"WAPSupp_3",
	"WAPFlsh",
	"WAPFlsh_2",
	"WAPFlsh_3",
	"WAPStck",
	"WAPSeWp",
	"WAPLasr",
	"WAPLasr_2",
	"WAPLasr_3",
	"WAPFlshLasr",
	"WAPFlshLasr_2",
	"WAPFlshLasr_3",
	"NM_Butt_Marker",
	"gun_drum",
	"gun_cylinder",
	"WAPBarrel",
	0
};


void CWeaponModelInfo::SetBoneIndicies()
{
	crSkeletonData *pSkeletonData = NULL;
	if(GetFragType())
		pSkeletonData = GetFragType()->GetCommonDrawable()->GetSkeletonData();
	else
		pSkeletonData = GetDrawable()->GetSkeletonData();

	if (pSkeletonData)
	{
		s32 i=0;
		const crBoneData *pBone = NULL;

		while(weaponBoneNames[i] != NULL)
		{
			pBone = pSkeletonData->FindBoneData(weaponBoneNames[i]);
			if(pBone)
			{
				m_nBoneIndices[i] = pBone->GetIndex();
			}
			i++;
		}

		// force root bone to be bone 0 since it's got a different name for each weapon
		m_nBoneIndices[0] = 0;
	}
}

eHierarchyId CWeaponModelInfo::GetBoneHierarchyId(u32 boneHash) const
{
	s32 i=0;
	while(weaponBoneNames[i] != NULL)
	{
		if(atStringHash(weaponBoneNames[i]) == boneHash)
		{
			return static_cast<eHierarchyId>(i);
		}
		i++;
	}

	return WEAPON_ROOT;
}

static dev_float WEAPON_SINK_MULTIPLIER = 0.6f;

void CWeaponModelInfo::InitWaterSamples()
{
	CBaseModelInfo::InitWaterSamples();
	modelinfoAssert(m_pBuoyancyInfo);
	if(m_pBuoyancyInfo)
	{
		m_pBuoyancyInfo->m_fBuoyancyConstant *= WEAPON_SINK_MULTIPLIER;
	}
}

// ---- HD streaming stuff ----
//

// PURPOSE: Add a reference to this archetype and add ref to any HD assets loaded
void CWeaponModelInfo::AddHDRef(bool bRenderRef)
{
	Assert(GetHasLoaded());
	AddRef();

	if (bRenderRef)
	{
		Assert(m_numHDRenderRefs < 1500);
		m_numHDRenderRefs++;
	} 
	else
	{
		Assert(m_numHDUpdateRefs < 1500);
		m_numHDUpdateRefs++;
	}
}

void CWeaponModelInfo::RemoveHDRef(bool bRenderRef)
{
	RemoveRef();
	if (bRenderRef)
	{
		Assert(m_numHDRenderRefs > 0);
		if (m_numHDRenderRefs > 0)
		{
			m_numHDRenderRefs--;
		}
	} 
	else 
	{
		Assert(m_numHDUpdateRefs > 0);
		if (m_numHDUpdateRefs > 0)
		{
			m_numHDUpdateRefs--;
		}
	}

	if (GetNumHdRefs() == 0)
	{
		modelinfoDisplayf("+++ weapon cleanup : %s", GetModelName());
		ReleaseHDFiles();
	} 
}

#if __DEV
bool CWeaponModelInfo_bDisableHD = false;
#endif
void CWeaponModelInfo::AddToHDInstanceList(size_t id)
{
	modelinfoAssert(id);
	modelinfoAssert(!m_HDInstanceList.IsMemberOfList((void*)id));

	if (GetHasHDFiles())
	{
		// set this to true to disable HD weapon switching
#if __DEV
		if (CWeaponModelInfo_bDisableHD)
		{
			return;
		}
#endif

		// if this is the first one then issue the HD requests for this type
		if (m_HDInstanceList.GetHeadPtr() == NULL)
		{
			RequestAndRefHDFiles();
		}

		m_HDInstanceList.Add((void*)id);
	}
}


void CWeaponModelInfo::RemoveFromHDInstanceList(size_t id)
{
	modelinfoAssert(id);

	if (m_HDInstanceList.IsMemberOfList((void*)id))
	{
		m_HDInstanceList.Remove((void*)id);

		// if this was the last one then free up the HD requests for this type
		if (m_HDInstanceList.GetHeadPtr() == NULL)
		{
			ReleaseRequestOrRemoveRefHD();
		}
	}
}

void CWeaponModelInfo::SetupHDFiles(const char* pName)
{
	if ( pName == NULL || stricmp(pName, "NULL")== 0)
	{
		return;
	}

	if (m_HDDistance <= 0.0f)
	{
		return;
	}

	char HDName[256];
	size_t size = strlen(pName);

	if (size > 250)
	{
		return;
	}

	strncpy(HDName, pName, size);

	HDName[size] = '_';
	HDName[size+1] = 'h';
	HDName[size+2] = 'i';
	HDName[size+3] = '\0';

	m_HDDrwbIdx = g_DrawableStore.FindSlot(HDName).Get();

	HDName[size] = '+';		// HD txd name is different because it is machine generated

	m_HDtxdIdx = g_TxdStore.FindSlot(HDName).Get();

	if (m_HDtxdIdx == -1 && m_HDDrwbIdx == -1)
	{
		m_HDDistance = 0.0f;
		return;
	}

	if (m_HDtxdIdx != -1)
	{
		CTexLod::StoreHDMapping(STORE_ASSET_TXD, GetAssetParentTxdIndex(), m_HDtxdIdx);
		SetIsHDTxdCapable(true);
		SetHDTextureDistance(m_HDDistance);
	}

	if (m_HDDrwbIdx != -1)
	{
		g_DrawableStore.SetParentTxdForSlot(strLocalIndex(m_HDDrwbIdx), strLocalIndex(GetAssetParentTxdIndex()));
	}
}

void CWeaponModelInfo::ConfirmHDFiles()
{
	if (GetHasHDFiles())
	{
		// verify existence of HD files
		if (m_HDDrwbIdx != -1 && g_DrawableStore.IsObjectInImage(strLocalIndex(m_HDDrwbIdx)))
		{
			return;
		}
	}
	// verification failed. Set HD dist to invalid
	SetHDDist(-1.0f);
}

// issue the HD request for this weapon type (if it has one set up)
void CWeaponModelInfo::RequestAndRefHDFiles()
{
	if (GetAreHDFilesLoaded())
	{
		AddHDRef(false);
		return;
	}

	if (!GetRequestLoadHDFiles())
	{
		WeaponHDStreamReq* pNewReq = rage_new(WeaponHDStreamReq);
		Assertf(pNewReq, "Failed to allocate WeaponHDStreamReq");
		if (pNewReq)
		{
			pNewReq->m_Request.Request(strLocalIndex(m_HDDrwbIdx), g_DrawableStore.GetStreamingModuleId());
			pNewReq->m_pTargetWeaponMI = this;

			ms_HDStreamRequests.Add(pNewReq);

			SetRequestLoadHDFiles(true);
			return;
		}
	}

	modelinfoAssertf(false, "Error in CWeaponModelInfo::RequestAndRefHDFiles");
}

// remove the request if it still outstanding, otherwise remove the ref
void CWeaponModelInfo::ReleaseRequestOrRemoveRefHD()
{
	if (GetAreHDFilesLoaded())
	{
		Assert(m_numHDUpdateRefs > 0);
		RemoveHDRef(false);
		return;
	}

	if (GetRequestLoadHDFiles())
	{
		// remove requested files
		fwPtrNode* pLinkNode = ms_HDStreamRequests.GetHeadPtr();
		while(pLinkNode) {
			WeaponHDStreamReq*	pStreamRequest = static_cast<WeaponHDStreamReq*>(pLinkNode->GetPtr());
			pLinkNode = pLinkNode->GetNextPtr();

			if (pStreamRequest && pStreamRequest->m_pTargetWeaponMI == this)
			{
				SetRequestLoadHDFiles(false);
				ms_HDStreamRequests.Remove(pStreamRequest);
				delete pStreamRequest;
			}
		}
		return;
	} 

	modelinfoAssertf(false, "Error in CWeaponModelInfo::ReleaseRequestOrRemoveRefHD");
}

// cleanup HD state back to normal state (whether HD requested or loaded)
void CWeaponModelInfo::ReleaseHDFiles()
{
	modelinfoAssert(GetAreHDFilesLoaded());
	modelinfoAssert(GetHasHDFiles());
	modelinfoAssert(GetNumHdRefs() == 0);

	if (GetHasHDFiles())
	{
		if(m_pHDShaderEffectType)
		{
			rmcDrawable *pDrawable = g_DrawableStore.Get(strLocalIndex(m_HDDrwbIdx)); 
			if(pDrawable)
			{
				// check txds are set as expected
				weaponAssertf(g_TxdStore.HasObjectLoaded(strLocalIndex(GetAssetParentTxdIndex())), "archetype %s : unexpected txd state", GetModelName());
				m_pHDShaderEffectType->RestoreModelInfoDrawable(pDrawable);
			}

			m_pHDShaderEffectType->RemoveRef();
			m_pHDShaderEffectType = NULL;
		}
		
		if (GetAreHDFilesLoaded())
		{
			// remove loaded files
			modelinfoAssert(m_HDDrwbIdx != -1);
			g_DrawableStore.RemoveRef(strLocalIndex(m_HDDrwbIdx), REF_OTHER);

			SetAreHDFilesLoaded(false);
		}
	}
}

// update the state of any requests, start triggering requests if desired
void CWeaponModelInfo::UpdateHDRequests()
{
	fwPtrNode* pLinkNode = ms_HDStreamRequests.GetHeadPtr();

	while(pLinkNode)
	{
		WeaponHDStreamReq*	pStreamRequest = static_cast<WeaponHDStreamReq*>(pLinkNode->GetPtr());
		pLinkNode = pLinkNode->GetNextPtr();

		if (pStreamRequest)
		{
			if (pStreamRequest->m_Request.HasLoaded()){
				// add refs 
				CWeaponModelInfo* pWMI = pStreamRequest->m_pTargetWeaponMI;
				modelinfoAssert(pWMI);

				if (!pWMI->GetHasLoaded())
				{
					// handle what happens if SD assets are cleaned up whilst HD request is outstanding
					Assert(!pWMI->GetAreHDFilesLoaded());

					pWMI->SetRequestLoadHDFiles(false);
					ms_HDStreamRequests.Remove(pStreamRequest);
					delete pStreamRequest;

					pWMI->FlushHDInstanceList();
				} 
				else
				{
					g_DrawableStore.AddRef(strLocalIndex(pWMI->m_HDDrwbIdx), REF_OTHER);

					pWMI->SetAreHDFilesLoaded(true);
					pWMI->SetRequestLoadHDFiles(false);
					ms_HDStreamRequests.Remove(pStreamRequest);
					delete pStreamRequest;

					pWMI->SetupHDCustomShaderEffects();

					pWMI->AddHDRef(false);
				}
			}
		}
	}
}

bool CWeaponModelInfo::SetupHDCustomShaderEffects()
{
	rmcDrawable *pDrawable = (m_HDDrwbIdx != -1) ? g_DrawableStore.Get(strLocalIndex(m_HDDrwbIdx)) : NULL; 

	if (pDrawable)
	{
#if __ASSERT
		extern char* CCustomShaderEffectBase_EntityName;
		CCustomShaderEffectBase_EntityName = (char*)this->GetModelName(); // debug only: defined in CCustomShaderEffectBase.cpp
#endif //__ASSERT...

		if(m_pHDShaderEffectType)
		{	// re-initialise CSE HD (if created):
 			m_pHDShaderEffectType->Initialise(pDrawable, this, m_pHDShaderEffectType->GetHasTint(), m_pHDShaderEffectType->GetHasPalette());
		}
		else
		{	// create CSE HD (if not created already):
			m_pHDShaderEffectType = static_cast<CCustomShaderEffectWeaponType*>(CCustomShaderEffectBaseType::SetupForDrawable(pDrawable, (ModelInfoType)m_type, this, GetHashKey(), -1)); 
		}

		if(m_pHDShaderEffectType)
		{
 			modelinfoAssertf(m_pHDShaderEffectType->AreShadersValid(pDrawable), "%s: Model has wrong shaders applied", GetModelName());
 			m_pHDShaderEffectType->SetIsHighDetail(true);
		}

		return(TRUE);
	}
	return(FALSE);
}

rmcDrawable *CWeaponModelInfo::GetHDDrawable()
{
	return (m_HDDrwbIdx != -1) ? g_DrawableStore.Get(strLocalIndex(m_HDDrwbIdx)) : NULL; 
}

