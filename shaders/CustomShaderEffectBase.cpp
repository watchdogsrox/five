//
// Filename:	CustomShaderEffect.cpp
// Description:	Class for customizable updating local shader variables during run-time at CEntity level
// Written by:	Andrzej
//
//	10/06/2005	-	Andrzej:	- initial;
//	30/09/2005	-	Andrzej:	- update to Rage WR146;
//
//
//

// Rage:
#include "rmcore/drawable.h"

// Game headers:
#include "debug/debug.h"
#include "CustomShaderEffectBase.h"
#include "CustomShaderEffectPed.h"
#include "CustomShaderEffectVehicle.h"
#include "CustomShaderEffectAnimUV.h"
#include "CustomShaderEffectTint.h"
#include "CustomShaderEffectTree.h"
#include "CustomShaderEffectGrass.h"
#include "CustomShaderEffectProp.h"
#include "CustomShaderEffectCable.h"
#include "CustomShaderEffectMirror.h"
#include "CustomShaderEffectWeapon.h"
#include "CustomShaderEffectInterior.h"
#include "modelinfo/vehicleModelInfo.h"
#include "physics/gtaArchetype.h"
#include "renderer/DrawLists/drawlist.h"
#include "renderer/Entities/InstancedEntityRenderer.h"

#if !__FINAL
	// for debug purposes:
	char* CCustomShaderEffectBase_EntityName = NULL;
#endif //!__FINAL...

//Statics
const u32 CCustomShaderEffectBase::sNumRegsPerInst = 4;

//
//
//
//
static u32 s_DlcInteriorMIDs[] =
{
	(u32)MID_INT_MP_H_01, (u32)MID_INT_MP_M_01, (u32)MID_INT_MP_L_01,
	(u32)MID_INT_MP_H_02, (u32)MID_INT_MP_M_02, (u32)MID_INT_MP_L_02,
	(u32)MID_INT_MP_H_03, (u32)MID_INT_MP_M_03, (u32)MID_INT_MP_L_03,
	(u32)MID_INT_MP_H_04, (u32)MID_INT_MP_M_04, (u32)MID_INT_MP_L_04,
	(u32)MID_INT_MP_H_05, (u32)MID_INT_MP_M_05, (u32)MID_INT_MP_L_05
};
static const int s_DlcInteriorMIDsSize = sizeof(s_DlcInteriorMIDs) / sizeof(s_DlcInteriorMIDs[0]);


//
//
//
//
CCustomShaderEffectBase::CCustomShaderEffectBase(u32 size, eCSE_TYPE type)
{
	m_size = size;
	FastAssert(m_size == size);	// Assign() doesn't work for bitfields
	
	m_type = type;
	FastAssert(m_type == (u8)type); // Assign() doesn't work for bitfields
}


//
//
//
//
CCustomShaderEffectBase::~CCustomShaderEffectBase()
{
	// do nothing
}

//
//
//
//
Vector4 *CCustomShaderEffectBase::WriteInstanceData(grcVec4BufferInstanceBufferList &ibList, Vector4 *pDest, const InstancedEntityCache &entityCache, u32 alpha) const
{
	if(	Verifyf(	ibList.GetNumRegsPerInstance() >= GetNumRegistersPerInstance(), "WARNING! Supplied grcVecArrayInstanceBufferList was not sized to contain enough instance registers! "
					"CCustomShaderEffectBase needs at least %d registers; grcVecArrayInstanceBufferList has %d registers. Make sure you are drawing the same model for this instanced batch!",
					GetNumRegistersPerInstance(), ibList.GetNumRegsPerInstance()) && 
		entityCache.IsValid())
	{
		Vector4 *ib = pDest;
		if(Verifyf(ib != NULL, "WARNING! Unable to write instance buffer data - instance buffer allocation failed! There will be missing instances."))
		{
			CEntity *entity = entityCache.GetEntity();
			const fwTransform &transform = entity->GetTransform();

			//Write out world transformation
			//Not sure if it's better to do a full transpose (ie. Matrix43) or pack in col_major and fix-up the matrix in the shader?
			//Sticking with col_major for now, just because it matches other matrices and makes for less shader changes. Should check at
			//some point how many shader instructions are used to unpack this col_major method. It's simple to change this if we decide
			//one is preferable to the other.
			Vector3 position = VEC3V_TO_VECTOR3(transform.GetPosition());
			ib[0].SetVector3(VEC3V_TO_VECTOR3(transform.GetNonOrthoA()));
			ib[1].SetVector3(VEC3V_TO_VECTOR3(transform.GetNonOrthoB()));
			ib[2].SetVector3(VEC3V_TO_VECTOR3(transform.GetNonOrthoC()));
			ib[0].w = position.x; ib[1].w = position.y; ib[2].w = position.z;

			//Write out globals
			u32 naturalAmb = entity->GetNaturalAmbientScale();
			u32 artificialAmb = entity->GetArtificialAmbientScale();
			ib[3].Set(CShaderLib::DivideBy255(alpha), CShaderLib::DivideBy255(naturalAmb), CShaderLib::DivideBy255(artificialAmb), 0.0f);

			return ib + sNumRegsPerInst;
		}
	}

	return NULL;
}

CCustomShaderEffectBaseType::~CCustomShaderEffectBaseType()
{
	// do nothing
}

//
//
//
//

void CCustomShaderEffectBaseType::AddRef()
{
	Assert(sysThreadType::IsUpdateThread());
	++m_refCount;
}


void CCustomShaderEffectBaseType::RemoveRef()
{
	Assert(sysThreadType::IsUpdateThread());
	--m_refCount;
	if (m_refCount == 0)
	{
		delete this;
	}
}

//
//
//
//

CCustomShaderEffectBaseType* CCustomShaderEffectBaseType::SetupMasterForModelInfo(CBaseModelInfo* pMI)
{
	Assert(pMI);
	CCustomShaderEffectBaseType* ret = NULL;

	if(pMI)
	{
		rmcDrawable* pDrawable = NULL;
		pDrawable = pMI->GetDrawable();

		s32 clipDictionaryIndex = pMI->GetClipDictionaryIndex().Get();
		if(!pMI->GetHasUvAnimation())
		{
			clipDictionaryIndex = -1;
		}

		Assert(pDrawable);
		if(pDrawable)
		{
			ret = SetupForDrawable(pDrawable, (ModelInfoType)pMI->GetModelType(), pMI, pMI->GetHashKey(), clipDictionaryIndex);
		}
	}

	return(ret);
}

CCustomShaderEffectBaseType* CCustomShaderEffectBaseType::SetupMasterVehicleHDInfo(CVehicleModelInfo* pVMI)
{
	Assert(pVMI);
	CCustomShaderEffectBaseType* ret = NULL;

	if(pVMI)
	{
		rmcDrawable* pDrawable = NULL;
		if(pVMI->GetHDFragType())
		{
			pDrawable = pVMI->GetHDFragType()->GetCommonDrawable();
		}

		Assert(pDrawable);
		if(pDrawable)
		{
			ret = SetupForDrawable(pDrawable, (ModelInfoType)pVMI->GetModelType(), pVMI, pVMI->GetHashKey(), -1);
			Assert(dynamic_cast<CCustomShaderEffectVehicleType*>(ret));
		}
	}

	return(ret);
}


//
//
// checks what shaders are attached to drawable to find out what ShaderEffect should be used
// for them (called from CBaseModelInfo::SetDrawable())
//
CCustomShaderEffectBaseType* CCustomShaderEffectBaseType::SetupForDrawable(rmcDrawable* pDrawable, ModelInfoType miType, CBaseModelInfo* pMI,
							u32 nObjectHashkey, s32 animDictFileIndex, grmShaderGroup *pClonedShaderGroup)
{
	Assert(pDrawable);

	grmShaderGroup *pShaderGroup = pClonedShaderGroup? pClonedShaderGroup : &pDrawable->GetShaderGroup();
	Assert(pShaderGroup);
	if(!pShaderGroup)
		return(NULL);

	CCustomShaderEffectBaseType* pShaderEffect=NULL;

	// fix missing texture variables in drawables:
/*	if(CCustomShaderEffectBase::FixShaderTextureVariables(pDrawable))
	{
		// something was fixed, print warning:
#if !__FINAL
		char msg[256];
		::sprintf(msg, "Texture variables were fixed in object '%s'.",CCustomShaderEffectBase_EntityName);
//		Assertf(FALSE, msg);
#endif //!__FINAL...
	}*/

	// AnimUV:
	if(CCustomShaderEffectAnimUV::CreateExtraAnimationUVvariables(pDrawable))
	{
		if((nObjectHashkey) && (animDictFileIndex!=-1))	// uv anim present?
		{
		#if __ASSERT
			// neither ped nor vehicle objects not allowed here:
			Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses anim UV shader(s)!",CCustomShaderEffectBase_EntityName);
		#endif//!__FINAL...
			return CCustomShaderEffectAnimUVType::Create(pDrawable, nObjectHashkey, animDictFileIndex);
		}
	}

	// Weapons:
	if(	pShaderGroup->LookupShader("weapon_emissive_tnt")				|| 
		pShaderGroup->LookupShader("weapon_normal_spec_tnt")			||
		pShaderGroup->LookupShader("weapon_normal_spec_detail_tnt")		||
		pShaderGroup->LookupShader("weapon_normal_spec_detail_palette")	||
		pShaderGroup->LookupShader("weapon_normal_spec_palette")		||
		pShaderGroup->LookupShader("weapon_normal_spec_cutout_palette")	||
		false
		)
	{
	#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses weapon shader(s)!",CCustomShaderEffectBase_EntityName);
	#endif//!__FINAL...
		const bool bTint	= pShaderGroup->CheckForShadersWithSuffix("_tnt");
		const bool bPalette	= pShaderGroup->CheckForShadersWithSuffix("_palette");

		CCustomShaderEffectWeaponType* pShaderEffect = rage_new CCustomShaderEffectWeaponType;
		if(!pShaderEffect->Initialise(pDrawable, pMI, bTint, bPalette))
		{
			pShaderEffect->RemoveRef();
			Assertf(FALSE, "Error initialising CustomShaderEffectWeapon!");
			return(NULL);
		}
		return(pShaderEffect);
	}

	//
	// Grass:
	//
	if(	pShaderGroup->LookupShader("grass_batch")	||
		pShaderGroup->LookupShader("normal_spec_batch")	||
		false
		)
	{
	#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses batch shader(s)!",CCustomShaderEffectBase_EntityName);
	#endif
		pShaderEffect = rage_new CCustomShaderEffectGrassType;
		if(!pShaderEffect->Initialise(pDrawable))
		{
			Assertf(FALSE, "Error initialising CustomShaderEffectGrass!");
			return(NULL);
		}
		return(pShaderEffect);
	}

	//
	// Props:
	//
	const u32 uModelNameHash = pMI? pMI->GetModelNameHash() : 0;
	if(	 pShaderGroup->LookupShader("normal_spec_wrinkle")	||
		(pShaderGroup->LookupShader("normal_um")		&& ((uModelNameHash==MID_P_PARACHUTE1_S) || (uModelNameHash==MID_P_PARACHUTE1_SP_S) || (uModelNameHash==MID_P_PARACHUTE1_MP_S) || (uModelNameHash==MID_PIL_P_PARA_PILOT_SP_S) || (uModelNameHash==MID_LTS_P_PARA_PILOT2_SP_S) || (uModelNameHash==MID_SR_PROP_SPECRACES_PARA_S_01) || (uModelNameHash==MID_GR_PROP_GR_PARA_S_01) || (uModelNameHash==MID_XM_PROP_X17_PARA_SP_S) || (uModelNameHash==MID_TR_PROP_TR_PARA_SP_S_01A) || (uModelNameHash==MID_REH_PROP_REH_PARA_SP_S_01A)))	||
		(pShaderGroup->LookupShader("normal_um_tnt")	&& ((uModelNameHash==MID_P_PARACHUTE1_S) || (uModelNameHash==MID_P_PARACHUTE1_SP_S) || (uModelNameHash==MID_P_PARACHUTE1_MP_S) || (uModelNameHash==MID_PIL_P_PARA_PILOT_SP_S) || (uModelNameHash==MID_LTS_P_PARA_PILOT2_SP_S) || (uModelNameHash==MID_SR_PROP_SPECRACES_PARA_S_01) || (uModelNameHash==MID_GR_PROP_GR_PARA_S_01) || (uModelNameHash==MID_XM_PROP_X17_PARA_SP_S) || (uModelNameHash==MID_TR_PROP_TR_PARA_SP_S_01A) || (uModelNameHash==MID_REH_PROP_REH_PARA_SP_S_01A)))	||
		(uModelNameHash==MID_PIL_P_PARA_BAG_PILOT_S)																																																																																																																							||
		(uModelNameHash==MID_LTS_P_PARA_BAG_LTS_S)																																																																																																																								||
		(uModelNameHash==MID_LTS_P_PARA_BAG_PILOT2_S)																																																																																																																							||
		(uModelNameHash==MID_P_PARA_BAG_XMAS_S)																																																																																																																									||
		(uModelNameHash==MID_VW_P_PARA_BAG_VINE_S)																																																																																																																								||
		(uModelNameHash==MID_P_PARA_BAG_TR_S_01A)																																																																																																																							||
		(uModelNameHash==MID_TR_P_PARA_BAG_TR_S_01A)																																																																																																																							||
		(uModelNameHash==MID_REH_P_PARA_BAG_REH_S_01A)																																																																																																																							||
		false
		)
	{
		bool bIsParachute = false;
		bool bIsTint = false;
		bool bUseSwapableTex = false;
		if((uModelNameHash==MID_P_PARACHUTE1_S) || (uModelNameHash==MID_P_PARACHUTE1_SP_S) || (uModelNameHash==MID_P_PARACHUTE1_MP_S) || (uModelNameHash==MID_PIL_P_PARA_PILOT_SP_S) || (uModelNameHash==MID_LTS_P_PARA_PILOT2_SP_S) || (uModelNameHash==MID_SR_PROP_SPECRACES_PARA_S_01) || (uModelNameHash==MID_GR_PROP_GR_PARA_S_01) || (uModelNameHash==MID_XM_PROP_X17_PARA_SP_S) || (uModelNameHash==MID_TR_PROP_TR_PARA_SP_S_01A) || (uModelNameHash==MID_REH_PROP_REH_PARA_SP_S_01A))
		{
			bIsParachute = 	pShaderGroup->LookupShader("normal_um")		|| 
							pShaderGroup->LookupShader("normal_um_tnt");
			bIsTint = 		pShaderGroup->LookupShader("normal_um_tnt")?true:false;
		}
		else if((uModelNameHash==MID_PIL_P_PARA_BAG_PILOT_S)	||
				(uModelNameHash==MID_LTS_P_PARA_BAG_LTS_S)		||
				(uModelNameHash==MID_LTS_P_PARA_BAG_PILOT2_S)	||
				(uModelNameHash==MID_P_PARA_BAG_XMAS_S)			||
				(uModelNameHash==MID_VW_P_PARA_BAG_VINE_S)		||
				(uModelNameHash==MID_P_PARA_BAG_TR_S_01A)		||
				(uModelNameHash==MID_TR_P_PARA_BAG_TR_S_01A)	||
				(uModelNameHash==MID_REH_P_PARA_BAG_REH_S_01A))
		{
			bUseSwapableTex = true;
		}

	#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses prop shader(s)!",CCustomShaderEffectBase_EntityName);
	#endif
		CCustomShaderEffectPropType* pShaderEffect = rage_new CCustomShaderEffectPropType;
		if(!pShaderEffect->Initialise(pDrawable, pMI, bIsParachute, bUseSwapableTex, bIsTint))
		{
			Assertf(FALSE, "Error initialising CustomShaderEffectProp!");
			return(NULL);
		}
		return(pShaderEffect);
	}

	//
	// DLC Interiors:
	//
	bool bIsDlcInterior = false;
	if(pMI)
	{
		for(s32 i=0; i<s_DlcInteriorMIDsSize; i++)
		{
			if(pMI->GetModelNameHash() == s_DlcInteriorMIDs[i])
			{
				bIsDlcInterior = true;
				break;
			}
		}
	}

	if(bIsDlcInterior)
	{
		const bool bIsTint = pShaderGroup->CheckForShadersWithSuffix( "_tnt" );
	#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses interior shader(s)!",CCustomShaderEffectBase_EntityName);
	#endif
		CCustomShaderEffectInteriorType* pShaderEffect = rage_new CCustomShaderEffectInteriorType;
		if(!pShaderEffect->Initialise(pDrawable, pMI, bIsTint))
		{
			Assertf(FALSE, "Error initialising CustomShaderEffectInterior!");
			return(NULL);
		}
		return(pShaderEffect);
	}

	// Trees:
	if(	pShaderGroup->LookupShader("trees")									||
		pShaderGroup->LookupShader("trees_camera_aligned")					||
		pShaderGroup->LookupShader("trees_camera_facing")					||
		pShaderGroup->LookupShader("trees_lod")								||
		pShaderGroup->LookupShader("trees_lod_tnt")							||
		pShaderGroup->LookupShader("trees_lod2")							||
		pShaderGroup->LookupShader("trees_lod2d")							||
		pShaderGroup->LookupShader("trees_normal")							||
		pShaderGroup->LookupShader("trees_normal_diffspec")					||
		pShaderGroup->LookupShader("trees_normal_diffspec_tnt")				||
		pShaderGroup->LookupShader("trees_normal_spec")						||
		pShaderGroup->LookupShader("trees_normal_spec_wind")				||
		pShaderGroup->LookupShader("trees_normal_spec_camera_aligned")		||
		pShaderGroup->LookupShader("trees_normal_spec_camera_aligned_tnt")	||
		pShaderGroup->LookupShader("trees_normal_spec_camera_facing")		||
		pShaderGroup->LookupShader("trees_normal_spec_camera_facing_tnt")	||
		pShaderGroup->LookupShader("trees_normal_spec_tnt")					||
		pShaderGroup->LookupShader("trees_tnt")								||
		false
		)
	{
	#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses tree shader(s)!",CCustomShaderEffectBase_EntityName);
	#endif//!__FINAL...

		// does it contain at least 1 tinted shader (maybe any _tnt, not necesarrily trees_tnt)?
		const bool bTint = pShaderGroup->CheckForShadersWithSuffix( "_tnt" );
		const bool bLod2d= pShaderGroup->LookupShader("trees_lod2d")?true:false;

static dev_bool bForceVehicleDeformation = false;
		bool bVehicleDeformation = (pMI->GetAttributes()==MODEL_ATTRIBUTE_NOISY_AND_DEFORMABLE_BUSH) || bForceVehicleDeformation;

		CCustomShaderEffectTreeType* pShaderEffect = rage_new CCustomShaderEffectTreeType;
		if(!pShaderEffect->Initialise(pDrawable, pMI, bTint, bLod2d, bVehicleDeformation))
		{
			pShaderEffect->RemoveRef();
			Assertf(FALSE, "Error initialising CustomShaderEffectTree!");
			return(NULL);
		}
		return(pShaderEffect);
	}

	//
	// Cable:
	//
	if(	pShaderGroup->LookupShader("cable")	||
		false
		)
	{
	#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses cable shader(s)!",CCustomShaderEffectBase_EntityName);
	#endif
		pShaderEffect = rage_new CCustomShaderEffectCableType;
		if(!pShaderEffect->Initialise(pDrawable))
		{
			Assertf(FALSE, "Error initialising CustomShaderEffectCable!");
			return(NULL);
		}
		return(pShaderEffect);
	}

	//
	// Mirror:
	//
	if(	pShaderGroup->LookupShader("mirror_default")	||
		pShaderGroup->LookupShader("mirror_normal")		||
		pShaderGroup->LookupShader("mirror_crack")		||
		pShaderGroup->LookupShader("mirror_decal")		||
		false
		)
	{
#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses mirror shader(s)!",CCustomShaderEffectBase_EntityName);
#endif
		pShaderEffect = rage_new CCustomShaderEffectMirrorType;
		if(!pShaderEffect->Initialise(pDrawable))
		{
			Assertf(FALSE, "Error initialising CustomShaderEffectMirror!");
			return(NULL);
		}
		return(pShaderEffect);
	}

	//
	// CSE Ped:
	//
#if !__ASSERT
	if( miType==MI_TYPE_PED )
#endif
	{
		if(	pShaderGroup->LookupShader("ped")							||
			pShaderGroup->LookupShader("ped_alpha")						||
			pShaderGroup->LookupShader("ped_cloth")						||
			pShaderGroup->LookupShader("ped_cloth_enveff")				||
			pShaderGroup->LookupShader("ped_decal")						||
			pShaderGroup->LookupShader("ped_decal_decoration")			||
			pShaderGroup->LookupShader("ped_decal_exp")					||
			pShaderGroup->LookupShader("ped_decal_medals")				||
			pShaderGroup->LookupShader("ped_decal_nodiff")				||
			pShaderGroup->LookupShader("ped_default")					||
			pShaderGroup->LookupShader("ped_default_cloth")				||
			pShaderGroup->LookupShader("ped_default_enveff")			||
			pShaderGroup->LookupShader("ped_default_mp")				||
			pShaderGroup->LookupShader("ped_default_palette")			||
			pShaderGroup->LookupShader("ped_emissive")					||
			pShaderGroup->LookupShader("ped_enveff")					||
			pShaderGroup->LookupShader("ped_fur")						||
			pShaderGroup->LookupShader("ped_hair_cutout_alpha")			||
			pShaderGroup->LookupShader("ped_hair_cutout_alpha_cloth")	||
			pShaderGroup->LookupShader("ped_hair_spiked")				||
			pShaderGroup->LookupShader("ped_hair_spiked_enveff")		||
			pShaderGroup->LookupShader("ped_hair_spiked_mask")			||
			pShaderGroup->LookupShader("ped_nopeddamagedecals")			||
			pShaderGroup->LookupShader("ped_palette")					||
			pShaderGroup->LookupShader("ped_wrinkle")					||
			pShaderGroup->LookupShader("ped_wrinkle_cloth_enveff")		||
			pShaderGroup->LookupShader("ped_wrinkle_cloth")				||
			pShaderGroup->LookupShader("ped_wrinkle_cs")				||
			pShaderGroup->LookupShader("ped_wrinkle_enveff")			||
			false
			)
		{
		#if __ASSERT
			// ped shader used, but not a ped?
			Assertf(miType==MI_TYPE_PED, "ArtError: Object '%s' is not PED, but uses ped shader(s)!",CCustomShaderEffectBase_EntityName);
		#endif//!__FINAL...

			pShaderEffect = rage_new CCustomShaderEffectPedType();
			if(!pShaderEffect->Initialise(pDrawable))
			{
				Assertf(FALSE, "Error initialising CustomShaderEffect!");
				return(NULL);
			}

			return(pShaderEffect);
		}
	}


	//
	// CSE Vehicle:
	//
#if !__ASSERT
	if(	miType == MI_TYPE_VEHICLE )
#endif
	{
		if( pShaderGroup->LookupShader("vehicle_apply_damage")			||
			pShaderGroup->LookupShader("vehicle_badges")				||
			pShaderGroup->LookupShader("vehicle_basic")					||
			pShaderGroup->LookupShader("vehicle_blurredrotor")			||
			pShaderGroup->LookupShader("vehicle_blurredrotor_emissive")	||
			pShaderGroup->LookupShader("vehicle_cloth")					||
			pShaderGroup->LookupShader("vehicle_cloth2")				||
			pShaderGroup->LookupShader("vehicle_cutout")				||
			pShaderGroup->LookupShader("vehicle_dash_emissive")			||
			pShaderGroup->LookupShader("vehicle_dash_emissive_opaque")	||
			pShaderGroup->LookupShader("vehicle_decal")					||
			pShaderGroup->LookupShader("vehicle_decal2")				||
			pShaderGroup->LookupShader("vehicle_detail")				||
			pShaderGroup->LookupShader("vehicle_detail2")				||
			pShaderGroup->LookupShader("vehicle_emissive_opaque")		||
			pShaderGroup->LookupShader("vehicle_generic")				||
			pShaderGroup->LookupShader("vehicle_interior")				||
			pShaderGroup->LookupShader("vehicle_interior2")				||
			pShaderGroup->LookupShader("vehicle_licenseplate")			||
			pShaderGroup->LookupShader("vehicle_lightsemissive")		||
			pShaderGroup->LookupShader("vehicle_mesh")					||
			pShaderGroup->LookupShader("vehicle_mesh_enveff")			||
			pShaderGroup->LookupShader("vehicle_mesh2_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint1")				||
			pShaderGroup->LookupShader("vehicle_paint1_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint2")				||
			pShaderGroup->LookupShader("vehicle_paint2_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint3")				||
			pShaderGroup->LookupShader("vehicle_paint3_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint4")				||
			pShaderGroup->LookupShader("vehicle_paint4_emissive")		||
			pShaderGroup->LookupShader("vehicle_paint4_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint5_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint6")				||
			pShaderGroup->LookupShader("vehicle_paint6_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint7")				||
			pShaderGroup->LookupShader("vehicle_paint7_enveff")			||
			pShaderGroup->LookupShader("vehicle_paint8")				||
			pShaderGroup->LookupShader("vehicle_paint9")				||
			pShaderGroup->LookupShader("vehicle_shuts")					||
			pShaderGroup->LookupShader("vehicle_tire")					||
			pShaderGroup->LookupShader("vehicle_tire_emissive")			||
			pShaderGroup->LookupShader("vehicle_track")					||
			pShaderGroup->LookupShader("vehicle_track_ammo")			||
			pShaderGroup->LookupShader("vehicle_track_emissive")		||
			pShaderGroup->LookupShader("vehicle_track_siren")			||
			pShaderGroup->LookupShader("vehicle_track2")				||
			pShaderGroup->LookupShader("vehicle_track2_emissive")		||
			pShaderGroup->LookupShader("vehicle_vehglass")				||	
			pShaderGroup->LookupShader("vehicle_vehglass_crack")		||	
			pShaderGroup->LookupShader("vehicle_vehglass_inner")		||
			false
			)
		{
		#if __ASSERT
			// vehicle shader used, but not a vehicle?
			Assertf(miType==MI_TYPE_VEHICLE,"ArtError: Object '%s' is not VEHICLE, but uses vehicle shader(s)!",CCustomShaderEffectBase_EntityName);
		#endif//!__FINAL...

			CCustomShaderEffectVehicleType *pVehShaderEffect = NULL;
			pShaderEffect = pVehShaderEffect = CCustomShaderEffectVehicleType::Create(pDrawable, (CVehicleModelInfo*)pMI, pClonedShaderGroup);
			if(!pVehShaderEffect->Initialise(pDrawable, pClonedShaderGroup))
			{
				Assertf(FALSE, "Error initialising CustomShaderEffect!");
				return(NULL);
			}
			return(pVehShaderEffect);
		}
	}
	
	// Palette tint:
	if( pShaderGroup->CheckForShadersWithSuffix( "_tnt" ) )
	{
	#if __ASSERT
		// neither ped nor vehicle objects not allowed here:
		Assertf((miType!=MI_TYPE_PED) && (miType!=MI_TYPE_VEHICLE), "ArtError: Object '%s' is ped or vehicle, but uses tint shader(s)!",CCustomShaderEffectBase_EntityName);
	#endif//!__FINAL...

		pShaderEffect = CCustomShaderEffectTintType::Create(pDrawable, pMI);
		if(!pShaderEffect->Initialise(pDrawable))
		{
			delete pShaderEffect;
			Assertf(FALSE, "Error initialising CustomShaderEffectTint!");
			return(NULL);
		}
		if(pShaderGroup->LookupShader("spec_twiddle_tnt") || pShaderGroup->LookupShader("normal_spec_twiddle_tnt"))
		{
			((CCustomShaderEffectTintType*)pShaderEffect)->SetForceVehicleStencil(true);
		}
		else
		{
			((CCustomShaderEffectTintType*)pShaderEffect)->SetForceVehicleStencil(false);
		}
		return(pShaderEffect);
	}

	return(NULL);

}// end of SetupForDrawable()...

