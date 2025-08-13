///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	DecalManager.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	12 May 2010
//	WHAT:	
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "DecalManager.h"

// rage
#include "math/random.h"
#include "phbound/boundcomposite.h"
#include "system/nelem.h"
#include "system/threadtype.h"

// framework
#include "grcore/debugdraw.h"
#include "fwsys/glue.h"
#include "vfx/channel.h"
#include "vfx/vfxutil.h"
#include "vfx/decal/decalmanager.h"
#include "vfx/decal/decalsettings.h"

// game
#include "camera/viewports/ViewportManager.h"
#include "control/replay/Effects/ProjectedTexturePacket.h"
#include "control/replay/Effects/ParticleVehicleFxPacket.h"
#include "Core/Game.h"
#include "Game/Weather.h"
#include "ModelInfo/PedModelInfo.h"
#include "ModelInfo/VehicleModelInfo.h"
#include "Network/Live/livemanager.h"
#include "Network/Live/NetworkClan.h"
#include "Network/Objects/Entities/NetObjVehicle.h"
#include "Peds/PlayerInfo.h"
#include "Peds/Ped.h"
#include "Peds/Rendering/PedVariationDS.h"
#include "Peds/Rendering/PedVariationStream.h"
#include "Peds/rendering/PedVariationPack.h"
#include "Physics/GtaInst.h"
#include "Physics/Physics.h"
#include "Renderer/Deferred/GBuffer.h"
#include "renderer/Entities/PedDrawHandler.h"
#include "renderer/lights/lights.h"
#include "Renderer/RenderTargets.h"
#include "renderer/PostProcessFX.h"
#include "Scene/DataFileMgr.h"
#include "Scene/World/GameWorld.h"
#include "Shaders/ShaderLib.h"
#include "Vehicles/Vehicle.h"
#include "Vfx/Decals/DecalAsyncTaskDesc.h"
#include "Vfx/Decals/DecalCallbacks.h"
#include "vfx/vehicleglass/VehicleGlassManager.h"
#include "Vfx/VfxHelper.h"
#include "Vfx/Misc/Fire.h"
#include "Vfx/Particles/PtfxManager.h"
#include "Vfx/Systems/VfxBlood.h"
#include "Vfx/Systems/VfxLiquid.h"
#include "Vfx/Systems/VfxMaterial.h"
#include "Vfx/Systems/VfxPed.h"
#include "Vfx/Systems/VfxWeapon.h"
#include "Vfx/Systems/VfxWheel.h"
#include "control/replay/effects/Particlebloodfxpacket.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_DECAL_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  DEFINES
///////////////////////////////////////////////////////////////////////////////

#define DECAL_COLN_FLAG_MAP				    (1<<PH_INST_MAPCOL)
#define DECAL_COLN_FLAG_BUILDINGS		    (1<<PH_INST_BUILDING)
#define DECAL_COLN_FLAG_PEDS			    ((1<<PH_INST_PED) | (1<<PH_INST_FRAG_PED))
#define DECAL_COLN_FLAG_VEHICLES		    ((1<<PH_INST_VEH) | (1<<PH_INST_FRAG_VEH))
#define DECAL_COLN_FLAG_OBJECT			    ((1<<PH_INST_OBJECT) | (1<<PH_INST_FRAG_OBJECT) | (1<<PH_INST_FRAG_CACHE_OBJECT))
#define DECAL_COLN_FLAG_ALL				    (DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS | DECAL_COLN_FLAG_VEHICLES | DECAL_COLN_FLAG_OBJECT | DECAL_COLN_FLAG_PEDS)
#define DECAL_COLN_FLAG_ALL_BAR_PEDS	    (DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS | DECAL_COLN_FLAG_VEHICLES | DECAL_COLN_FLAG_OBJECT)

#define DECAL_NUM_GLASS_SHADERS			    (9)
#define DECAL_MAX_SHADER_STRING_LENGTH	    (64)


///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

#if __PS3
	DECLARE_TASK_INTERFACE(DecalProcessBound);
	DECLARE_TASK_INTERFACE(DecalProcessDrawablesNonSkinned);
	DECLARE_TASK_INTERFACE(DecalProcessDrawablesSkinned);
	DECLARE_TASK_INTERFACE(DecalProcessSmashGroup);
#endif

CDecalManager g_decalMan;

decalTypeSettings g_typeSettings[DECALTYPE_NUM] = 
{
//	depth		zShiftOffst	polyRejectThreshold	lodDistance	fadeDistance	cullDistance,	canMergeInsts,	wrapUV,			useAniso,	isPersistent,	collisionFlags,

// DECALTYPE_BANG
	{0.5f,		0.000f,		0.5f,				20.0f,		30.0f,			40.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_BLOOD_SPLAT
	{0.5f,		0.005f,		0.3f,				30.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_FOOTPRINT
	{0.3f,		0.003f,		0.3f,				10.0f,		20.0f,			25.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS},

// DECALTYPE_GENERIC_SIMPLE_COLN
	{0.3f,		0.000f,		0.3f,				20.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS},

// DECALTYPE_GENERIC_COMPLEX_COLN
	{0.3f,		0.000f,		0.3f,				20.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_GENERIC_SIMPLE_COLN_LONG_RANGE
	{0.3f,		0.000f,		0.3f,				50.0f,		450.0f,			500.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS},

// DECALTYPE_GENERIC_COMPLEX_COLN_LONG_RANGE
	{0.3f,		0.000f,		0.3f,				50.0f,		450.0f,			500.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_POOL_LIQUID
	{0.3f,		0.009f,		0.3f,				30.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS},
	
// DECALTYPE_SCUFF
	{0.5f,		0.000f,		0.5f,				20.0f,		30.0f,			40.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_TRAIL_SCRAPE
	{0.3f,		0.000f,		0.5f,				15.0f,		25.0f,			30.0f,			true,			true,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_TRAIL_LIQUID
	{0.3f,		0.008f,		0.3f,				20.0f,		50.0f,			60.0f,			true,			true,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_TRAIL_SKID
	{0.3f,		0.003f,		0.3f,				20.0f,		50.0f,			60.0f,			true,			true,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_OBJECT},

// DECALTYPE_TRAIL_GENERIC
	{0.3f,		0.000f,		0.3f,				20.0f,		50.0f,			60.0f,			true,			true,			false,		false,			DECAL_COLN_FLAG_MAP},

// DECALTYPE_VEHICLE_BADGE
	{0.3f,		0.000f,		0.3f,				0.0f,		50.0f,			60.0f,			false,			false,			true,		true,			DECAL_COLN_FLAG_VEHICLES},

// DECALTYPE_VEHICLE_BADGE+1
	{0.3f,		0.000f,		0.3f,				0.0f,		50.0f,			60.0f,			false,			false,			true,		true,			DECAL_COLN_FLAG_VEHICLES},

// DECALTYPE_VEHICLE_BADGE+2
	{0.3f,		0.000f,		0.3f,				0.0f,		50.0f,			60.0f,			false,			false,			true,		true,			DECAL_COLN_FLAG_VEHICLES},

// DECALTYPE_VEHICLE_BADGE+3
	{0.3f,		0.000f,		0.3f,				0.0f,		50.0f,			60.0f,			false,			false,			true,		true,			DECAL_COLN_FLAG_VEHICLES},

// DECALTYPE_WEAPON_IMPACT
	{0.3f,		0.003f,		0.3f,				20.0f,		30.0f,			40.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_WEAPON_IMPACT_SHOTGUN
	{0.3f,		0.003f,		0.3f,				20.0f,		30.0f,			40.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_WEAPON_IMPACT_SMASHGROUP
	{1.0f,		0.003f,		0.3f,				30.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_WEAPON_IMPACT_SMASHGROUP_SHOTGUN
	{1.0f,		0.003f,		0.3f,				30.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND
	{2.0f,		0.003f,		0.3f,				125.0f,		100.0f,			125.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS},
	
// DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE (currently planes only - make depth massive so decals appear fine on jumbo jet)
	{10.0f,		0.003f,		-1.0f,				125.0f,		100.0f,			125.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_VEHICLES},

// DECALTYPE_WEAPON_IMPACT_VEHICLE
	{0.3f,		0.003f,		0.3f,				20.0f,		100.0f,			125.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL_BAR_PEDS},

// DECALTYPE_WEAPON_IMPACT_PED
//	{0.1f,		0.003f,		0.3f,				20.0f,		30.0f,			40.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_PEDS},

// DECALTYPE_LASER_SIGHT
//	{0.3f,		0.010f,		0.0f,				0.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_ALL},

// DECALTYPE_MARKER
	{1.0f,		0.001f,		0.3f,				50.0f,		50.0f,			60.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS},

// DECALTYPE_MARKER_LONG_RANGE
	{1.0f,		0.001f,		0.3f,				50.0f,		450.0f,			500.0f,			false,			false,			false,		false,			DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_BUILDINGS},
};

dev_u8 DECAL_VEHICLE_BADGE_TINT_R = 255;
dev_u8 DECAL_VEHICLE_BADGE_TINT_G = 255;
dev_u8 DECAL_VEHICLE_BADGE_TINT_B = 255;

dev_float DECAL_BLOOD_SPLAT_DRD_WIDTH_MULT = 0.5f;


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::Init()
{
	m_emblemDesc.Invalidate();
	m_txdHashName.Clear();
	m_texHashName.Clear();
	m_pTexture = nullptr;
}


///////////////////////////////////////////////////////////////////////////////
//  Set
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::Set(EmblemDescriptor& emblemDesc)
{
	m_emblemDesc = emblemDesc;
	m_txdHashName.Clear();
	m_texHashName.Clear();
	m_pTexture = nullptr;
}


///////////////////////////////////////////////////////////////////////////////
//  Set
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::Set(atHashWithStringNotFinal txdHashName, atHashWithStringNotFinal texHashName)
{
	m_emblemDesc.Invalidate();
	m_txdHashName = txdHashName;
	m_texHashName = texHashName;
	m_pTexture = nullptr;
}


///////////////////////////////////////////////////////////////////////////////
//  Set
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::Set(EmblemDescriptor& emblemDesc, atHashWithStringNotFinal txdHashName, atHashWithStringNotFinal texHashName)
{
	m_emblemDesc = emblemDesc;
	m_txdHashName = txdHashName;
	m_texHashName = texHashName;
	m_pTexture = nullptr;
	decalAssertf(IsValid(), "invalid vehicle badge desc settings");
}


///////////////////////////////////////////////////////////////////////////////
//  Request
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeDesc::Request() const
{
	if (IsUsingEmblem())
	{
		return NETWORK_CLAN_INST.GetCrewEmblemMgr().RequestEmblem(m_emblemDesc ASSERT_ONLY(, "CVehicleBadgeManager"));
	}
	else
	{
		return RequestTxd();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetTexture
///////////////////////////////////////////////////////////////////////////////

grcTexture* CVehicleBadgeDesc::GetTexture()
{
#if __BANK
	if (g_decalMan.GetDebugInterface().GetVehicleBadgeSimulateCloudIssues())
	{
		return nullptr;
	}
#endif

	if (IsUsingEmblem())
	{
		const char* pEmblemName = NETWORK_CLAN_INST.GetCrewEmblemMgr().GetEmblemName(m_emblemDesc);
		if (pEmblemName)
		{
			fwTxd* pTxd = g_TxdStore.GetSafeFromName(pEmblemName);
			if (pTxd)
			{
				m_pTexture = pTxd->Lookup(pEmblemName);
			}
		}
	}
	else
	{
		if (HasTxdLoaded())
		{
			fwTxd* pTxd = g_TxdStore.GetSafeFromName(m_txdHashName);
			if (pTxd)
			{
				m_pTexture = pTxd->Lookup(m_texHashName);
			}
		}
	}

	return m_pTexture;
}


///////////////////////////////////////////////////////////////////////////////
//  Release
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::Release() const
{
	if (IsUsingEmblem())
	{
		NETWORK_CLAN_INST.GetCrewEmblemMgr().ReleaseEmblem(m_emblemDesc ASSERT_ONLY(, "CVehicleBadgeManager"));
	}
}


///////////////////////////////////////////////////////////////////////////////
//  IsFree
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeDesc::IsFree() const
{
	return m_emblemDesc.IsValid()==false && m_txdHashName.IsNull() && m_texHashName.IsNull();
}


///////////////////////////////////////////////////////////////////////////////
//  IsValid
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeDesc::IsValid() const
{
	return (m_emblemDesc.IsValid()==false && m_txdHashName.IsNotNull() && m_texHashName.IsNotNull()) || 
		   (m_emblemDesc.IsValid() && m_txdHashName.IsNull() && m_texHashName.IsNull());
}


///////////////////////////////////////////////////////////////////////////////
//  IsEqual
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeDesc::IsEqual(const CVehicleBadgeDesc& other) const
{
	return m_emblemDesc==other.m_emblemDesc && m_txdHashName==other.m_txdHashName && m_texHashName==other.m_texHashName;
}


///////////////////////////////////////////////////////////////////////////////
//  Serialise
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::Serialise(CSyncDataBase& serialiser)
{
	bool usingEmblemDesc = IsUsingEmblem();

	SERIALISE_BOOL(serialiser, usingEmblemDesc);

	if (usingEmblemDesc && !serialiser.GetIsMaximumSizeSerialiser())
	{
		m_emblemDesc.Serialise(serialiser);

		m_txdHashName.Clear();
		m_texHashName.Clear();

		m_pTexture = nullptr;
	}
	else
	{
		u32 txdHash = m_txdHashName.GetHash();
		SERIALISE_HASH(serialiser, txdHash, "txd name");
		m_txdHashName = txdHash;

		u32 texHash = m_texHashName.GetHash();
		SERIALISE_HASH(serialiser, texHash, "texture name");
		m_texHashName = texHash;

		m_emblemDesc.Invalidate();

		m_pTexture = nullptr;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RequestTxd
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeDesc::RequestTxd() const
{
	strLocalIndex txdSlot = g_TxdStore.FindSlotFromHashKey(m_txdHashName);
	if (vfxVerifyf(txdSlot.IsValid(), "Texture dictionary %s not present or not registered with CStreaming", m_txdHashName.TryGetCStr()))
	{
		if (g_TxdStore.Get(txdSlot))
		{
			return true;
		}
		else
		{
			strIndex index = g_TxdStore.GetStreamingIndex(txdSlot);
			return strStreamingEngine::GetInfo().RequestObject(index, STRFLAG_DONTDELETE|STRFLAG_FORCE_LOAD|STRFLAG_PRIORITY_LOAD);
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  HasTxdLoaded
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeDesc::HasTxdLoaded() const
{
	strLocalIndex txdSlot = g_TxdStore.FindSlotFromHashKey(m_txdHashName);
	if (vfxVerifyf(txdSlot.IsValid(), "Texture dictionary %s not present or not registered with CStreaming", m_txdHashName.TryGetCStr()))
	{
		strIndex index = g_TxdStore.GetStreamingIndex(txdSlot);
		if (strStreamingEngine::GetInfo().GetStreamingInfoRef(index).GetStatus()==STRINFO_LOADED)
		{
			strStreamingEngine::GetInfo().ClearRequiredFlag(index, STRFLAG_DONTDELETE);
			return true;
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  AddTextureRef
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::AddTextureRef()
{
	decalAssertf(m_pTexture, "trying to add a reference to an invalid texture");
	if (m_pTexture)
	{
		m_pTexture->AddRef();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveTextureRef
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeDesc::RemoveTextureRef()
{
	decalAssertf(m_pTexture, "trying to add a reference to an invalid texture");
	if (m_pTexture)
	{
		m_pTexture->DecRef();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeData::Init()
{
	m_badgeDesc.Init();
	m_refCount = 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddRef
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeData::AddRef()
{
	m_badgeDesc.AddTextureRef();
	m_refCount++;
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveRef
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeData::RemoveRef(s32 decalId)
{
	m_badgeDesc.RemoveTextureRef();
	m_refCount--;
	if (m_refCount==0)
	{
		grcTexture* pDiffuseMap = const_cast<grcTexture*>(grcTexture::None);
		g_decalMan.PatchDiffuseMap(decalId, pDiffuseMap);

		Init();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeManager::Init()
{
	m_requests.Resize(MAX_VEHICLE_BADGE_REQUESTS);
	for (int i=0; i<MAX_VEHICLE_BADGE_REQUESTS; i++)
	{
		m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
	}

	m_vehicleBadgeData.Resize(DECALID_VEHICLE_BADGE_NUM);
	for (int i=0; i<DECALID_VEHICLE_BADGE_NUM; i++)
	{
		m_vehicleBadgeData[i].Init();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeManager::Shutdown()
{
	for (int i=0; i<MAX_VEHICLE_BADGE_REQUESTS; i++)
	{
		if (m_requests[i].m_currState != DECALREQUESTSTATE_NOT_ACTIVE)
		{
			m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
		}
	}

	for (int i=0; i<DECALID_VEHICLE_BADGE_NUM; i++)
	{
		for (int j=0; j<m_vehicleBadgeData[i].GetRefCount(); j++)
		{
			m_requests[i].m_badgeDesc.Release();
		}

		m_vehicleBadgeData[i].Init();

		g_decalMan.Remove(DECALID_VEHICLE_BADGE_FIRST+i);
		grcTexture* pDiffuseMap = const_cast<grcTexture*>(grcTexture::None);
		g_decalMan.PatchDiffuseMap(DECALID_VEHICLE_BADGE_FIRST+i, pDiffuseMap);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  CleanUpAfterFailure
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeManager::CleanUpAfterFailure(CVehicleBadgeRequestData* pRequest)
{
	if (pRequest->m_currState>=DECALREQUESTSTATE_FAILED)
	{
		if (pRequest->m_currState>=DECALREQUESTSTATE_FAILED_CANT_GET_CLAN_TEXTURE)
		{
			pRequest->m_badgeDesc.Release();
		}

		if (pRequest->m_currState>=DECALREQUESTSTATE_FAILED_VEHICLE_NO_LONGER_VALID)
		{
			for (int j=0; j<DECALID_VEHICLE_BADGE_NUM; j++)
			{
				if (m_vehicleBadgeData[j].GetVehicleBadgeDesc().IsEqual(pRequest->m_badgeDesc))
				{
					m_vehicleBadgeData[j].RemoveRef(DECALID_VEHICLE_BADGE_FIRST+j);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Update
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeManager::Update()
{
	for (int i=0; i<m_requests.GetCount(); i++)
	{
		if (m_requests[i].m_currState!=DECALREQUESTSTATE_NOT_ACTIVE)
		{
			// increment the count since the request was made
			m_requests[i].m_numFramesSinceRequest++;

			// check for requests that have timed out
			if (m_requests[i].m_numFramesSinceRequest>1000)
			{
				if (m_requests[i].m_currState>=DECALREQUESTSTATE_SUCCEEDED)
				{
					// the request is complete (succeeded or failed) - just deactivate the request
					m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
				}
				else if (m_requests[i].m_currState==DECALREQUESTSTATE_REQUESTING_BADGE)
				{
					// allow a little longer to get this data from the cloud
					if (m_requests[i].m_numFramesSinceRequest>1500)
					{
						// the request is still trying to get the emblem from the cloud - time it out, it's taken too long
						m_requests[i].m_badgeDesc.Release();
						
						m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
					}
				}
#if __ASSERT
				else
				{
					decalAssertf(0, "vehicle badge request hasn't been queried in 1000 frames and hasn't finished (currState: %d)", m_requests[i].m_currState);
				}
#endif
			}

			// check for the vehicle being deleted
			if (m_requests[i].m_currState==DECALREQUESTSTATE_REQUESTING_BADGE)
			{
				grcTexture* pDiffuseMap = m_requests[i].m_badgeDesc.GetTexture();
				if (pDiffuseMap)
				{
					m_requests[i].m_decalId = 0;
					m_requests[i].m_currState = DECALREQUESTSTATE_FAILED_CANT_ADD_DATA_SLOT;

					s32 idx = AddData(m_requests[i].m_badgeDesc);
					if (idx>-1)
					{
						g_decalMan.PatchDiffuseMap(DECALID_VEHICLE_BADGE_FIRST+idx, pDiffuseMap);

						m_requests[i].m_currState = DECALREQUESTSTATE_FAILED_VEHICLE_NO_LONGER_VALID;
						if (m_requests[i].m_regdVeh)
						{
							const CVehicle* pVehicle = m_requests[i].m_regdVeh;

							Mat34V vVehicleMtx = pVehicle->GetTransform().GetMatrix();

							s32 validComponentIds[3] = {-1, -1, -1};

							s32 boneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_BONNET));
							if (boneIndex>-1)
							{
								validComponentIds[0] = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(boneIndex);
							}

							boneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_DOOR_DSIDE_F));
							if (boneIndex>-1)
							{
								validComponentIds[1] = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(boneIndex);
							}

							boneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_DOOR_PSIDE_F));
							if (boneIndex>-1)
							{
								validComponentIds[2] = pVehicle->GetVehicleFragInst()->GetComponentFromBoneIndex(boneIndex);
							}

							m_requests[i].m_currState = DECALREQUESTSTATE_FAILED_VEHICLE_BONE_NOT_VALID;
							s32 badgeBoneIndex = m_requests[i].m_boneIndex;
							if (badgeBoneIndex>-1)
							{
								Mat34V vBoneMtxWld;
								CVfxHelper::GetMatrixFromBoneIndex(vBoneMtxWld, pVehicle, badgeBoneIndex);

								m_requests[i].m_currState = DECALREQUESTSTATE_FAILED_VEHICLE_BONE_IS_ZERO;
								if (IsZeroAll(vBoneMtxWld.GetCol0())==false)
								{
									Vec3V vPosWld = vBoneMtxWld.GetCol3();
									vPosWld = vPosWld + (vVehicleMtx.GetCol0()*ScalarVFromF32(m_requests[i].m_vOffsetPos.GetXf()));
									vPosWld = vPosWld + (vVehicleMtx.GetCol1()*ScalarVFromF32(m_requests[i].m_vOffsetPos.GetYf()));
									vPosWld = vPosWld + (vVehicleMtx.GetCol2()*ScalarVFromF32(m_requests[i].m_vOffsetPos.GetZf()));

									Vec3V vDirWld = Transform3x3(vVehicleMtx, m_requests[i].m_vDir);
									Vec3V vSideWld = Transform3x3(vVehicleMtx, m_requests[i].m_vSide);

									u32 probeFlags = ArchetypeFlags::GTA_VEHICLE_TYPE;
									Vec3V vPosStart = vPosWld;
									Vec3V vPosEnd = vPosStart + vDirWld;
									const s32 MAX_PROBE_RESULTS = 4;
									WorldProbe::CShapeTestHitPoint probeResult[MAX_PROBE_RESULTS];
									WorldProbe::CShapeTestResults probeResults(probeResult, MAX_PROBE_RESULTS);
									WorldProbe::CShapeTestProbeDesc probeDesc;
									probeDesc.SetResultsStructure(&probeResults);
									probeDesc.SetIncludeFlags(probeFlags);
									probeDesc.SetIncludeEntity(pVehicle);
									probeDesc.SetStartAndEnd(RCC_VECTOR3(vPosStart), RCC_VECTOR3(vPosEnd));

#if __BANK
									if (g_decalMan.GetDebugInterface().GetVehicleBadgeProbeDebug())
									{
										grcDebugDraw::Line(vPosStart, vPosEnd, Color32(1.0f, 1.0f, 0.0f, 1.0), -30);
									}
#endif

									m_requests[i].m_currState = DECALREQUESTSTATE_FAILED_VEHICLE_PROBE_FAILED;
									if (WorldProbe::GetShapeTestManager()->SubmitTest(probeDesc, WorldProbe::PERFORM_SYNCHRONOUS_TEST))
									{
										for (int j=0; j<MAX_PROBE_RESULTS; j++)
										{
											CEntity* pHitEntity = CPhysics::GetEntityFromInst(probeResults[j].GetHitInst());
											if (pHitEntity && pHitEntity==pVehicle)
											{
												if (pVehicle->GetVehicleType()==VEHICLE_TYPE_BIKE ||
													probeResults[j].GetHitComponent()==validComponentIds[0] || 
													probeResults[j].GetHitComponent()==validComponentIds[1]	||
													probeResults[j].GetHitComponent()==validComponentIds[2])
												{
													u32 renderSettingIndex, renderSettingCount;
													if (decalSettingsManager::FindRenderSettingInfo(DECALID_VEHICLE_BADGE_FIRST+idx, renderSettingIndex, renderSettingCount))
													{
														m_requests[i].m_decalId = g_decalMan.AddVehicleBadge(pVehicle, probeResults[j].GetHitComponent(), renderSettingIndex, renderSettingCount, probeResults[j].GetHitPositionV(), vDirWld, vSideWld, m_requests[i].m_size, m_requests[i].m_isForLocalPlayer, m_requests[i].m_badgeIndex, m_requests[i].m_alpha);
														pVehicle->SetEmblemMaterialGroup(m_requests[i].m_isForLocalPlayer);
														m_requests[i].m_currState = DECALREQUESTSTATE_APPLYING_DECAL;
													}

													break;
												}
											}
										}
									}
								}
							}
						}
					}
				}

				// tidy up if anything fails
				CleanUpAfterFailure(&m_requests[i]);
			}
			else if (m_requests[i].m_currState==DECALREQUESTSTATE_APPLYING_DECAL)
			{
				// check if the decal is no longer pending
				if (g_decalMan.IsDecalPending(m_requests[i].m_decalId)==false)
				{
					// it isn't - check that it has been applied
					if (g_decalMan.DoesVehicleHaveBadge(m_requests[i].m_regdVeh, m_requests[i].m_badgeIndex))
					{
						m_requests[i].m_currState = DECALREQUESTSTATE_SUCCEEDED;
					}
					else
					{
						m_requests[i].m_currState = DECALREQUESTSTATE_FAILED_DECAL_DIDNT_APPLY;
					}
				}

				// tidy up if anything fails
				CleanUpAfterFailure(&m_requests[i]);
			}
		}

		// remove any completed requests that are flagged
		if (m_requests[i].m_removeOnceComplete)
		{
			if (m_requests[i].m_currState==DECALREQUESTSTATE_SUCCEEDED)
			{
				m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
				Remove(m_requests[i].m_regdVeh, m_requests[i].m_badgeIndex);
			}
			else if (m_requests[i].m_currState>=DECALREQUESTSTATE_FAILED)
			{
				m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Request
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeManager::Request(const CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha)
{
	decalAssertf(badgeIndex < DECAL_NUM_VEHICLE_BADGES, "Vehicle Badge Index out of range");

	// check that this vehicle doesn't already have a badge
	decalBucket* pBucket = g_decalMan.FindFirstBucket(1<<(DECALTYPE_VEHICLE_BADGE+badgeIndex), pVehicle);
	if (pBucket)
	{
		decalAssertf(0, "requesting a badge on a vehicle that already has one");
		return false;
	}

	// check that a request isn't already in progress for this vehicle
	for (int i=0; i<m_requests.GetCount(); i++)
	{
		if (m_requests[i].m_currState!=DECALREQUESTSTATE_NOT_ACTIVE && m_requests[i].m_regdVeh==pVehicle && m_requests[i].m_badgeIndex==badgeIndex)
		{
			decalAssertf(0, "requesting a badge on a vehicle when one has already been requested");
			return false;
		}
	}

	bool hasRequested = false;
	// check the validity of the request
	if (pVehicle)
	{
		if (decalVerifyf(boneIndex>=0 && boneIndex<pVehicle->GetSkeletonData().GetNumBones(), "requesting a vehicle badge on an invalid bone index (%s %d)", pVehicle->GetDebugName(), boneIndex))
		{
			// need to request it
			for (int i=0; i<m_requests.GetCount(); i++)
			{
				if (m_requests[i].m_currState==DECALREQUESTSTATE_NOT_ACTIVE)
				{
					hasRequested = badgeDesc.Request();
					if (hasRequested)
					{
						m_requests[i].m_currState = DECALREQUESTSTATE_REQUESTING_BADGE;
						m_requests[i].m_badgeDesc = badgeDesc;
						m_requests[i].m_regdVeh = pVehicle;
						m_requests[i].m_boneIndex = boneIndex;
						m_requests[i].m_vOffsetPos = vOffsetPos;
						m_requests[i].m_vDir = vDir;
						m_requests[i].m_vSide = vSide;
						m_requests[i].m_size = size;
						m_requests[i].m_numFramesSinceRequest = 0;
						m_requests[i].m_removeOnceComplete = false;
						m_requests[i].m_isForLocalPlayer = isForLocalPlayer;
						m_requests[i].m_badgeIndex = badgeIndex;
						m_requests[i].m_alpha = alpha;
					}

					break;
				}
			}
		}
	}

#if GTA_REPLAY
	if( CReplayMgr::ShouldRecord() )
	{
		CReplayMgr::RecordFx<CPacketVehicleBadgeRequest>(
			CPacketVehicleBadgeRequest(badgeDesc, boneIndex, vOffsetPos, vDir, vSide, size, isForLocalPlayer, badgeIndex, alpha),
			pVehicle);
	}
#endif

	return hasRequested;
}


///////////////////////////////////////////////////////////////////////////////
//  Abort
///////////////////////////////////////////////////////////////////////////////

bool CVehicleBadgeManager::Abort(const CVehicle* pVehicle)
{
	for (int i=0; i<m_requests.GetCount(); i++)
	{
		if (m_requests[i].m_regdVeh==pVehicle)
		{
			if (m_requests[i].m_currState==DECALREQUESTSTATE_REQUESTING_BADGE)
			{
				m_requests[i].m_badgeDesc.Release();

				m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
				return true;
			}
		}
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetRequestState
///////////////////////////////////////////////////////////////////////////////

DecalRequestState_e CVehicleBadgeManager::GetRequestState(const CVehicle* pVehicle, const u32 badgeIndex)
{
	for (int i=0; i<m_requests.GetCount(); i++)
	{
		if (m_requests[i].m_currState!=DECALREQUESTSTATE_NOT_ACTIVE && m_requests[i].m_regdVeh==pVehicle && m_requests[i].m_badgeIndex == badgeIndex)
		{
			if (m_requests[i].m_currState==DECALREQUESTSTATE_REQUESTING_BADGE)
			{
				return DECALREQUESTSTATE_REQUESTING_BADGE;
			}
			else if (m_requests[i].m_currState==DECALREQUESTSTATE_APPLYING_DECAL)
			{
				return DECALREQUESTSTATE_APPLYING_DECAL;
			}
			else if (m_requests[i].m_currState==DECALREQUESTSTATE_SUCCEEDED)
			{
				m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
				return DECALREQUESTSTATE_SUCCEEDED;
			}
			else if (m_requests[i].m_currState>=DECALREQUESTSTATE_FAILED)
			{
				DecalRequestState_e failedState = m_requests[i].m_currState;
				m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
				return failedState;
			}
		}
	}

	//decalAssertf(0, "querying the state of an invalid request");
	return DECALREQUESTSTATE_NOT_ACTIVE;
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveCompletedRequests
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void CVehicleBadgeManager::RemoveCompletedRequests()
{
	for (int i=0; i<m_requests.GetCount(); i++)
	{
		if (m_requests[i].m_currState==DECALREQUESTSTATE_SUCCEEDED ||
			m_requests[i].m_currState>=DECALREQUESTSTATE_FAILED)
		{
			m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
		}
	}
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  AddData
///////////////////////////////////////////////////////////////////////////////

s32 CVehicleBadgeManager::AddData(CVehicleBadgeDesc& badgeDesc)
{
	// check if data already exists for this clan
	s32 firstFreeIdx = -1;
	for (int i=0; i<DECALID_VEHICLE_BADGE_NUM; i++)
	{
		if (m_vehicleBadgeData[i].GetVehicleBadgeDesc().IsEqual(badgeDesc))
		{
			// it does, add a ref and return the data
			m_vehicleBadgeData[i].AddRef();
			return i;
		}
		else if (firstFreeIdx==-1 && m_vehicleBadgeData[i].GetVehicleBadgeDesc().IsFree())
		{
			firstFreeIdx = i;
		}
	}

	// no data exists - add it
	if (decalVerifyf(firstFreeIdx>-1, "vehicle badge data is full"))
	{
		CVehicleBadgeData* pNewData = &m_vehicleBadgeData[firstFreeIdx];
		pNewData->SetVehicleBadgeDesc(badgeDesc);
		decalAssertf(pNewData->GetRefCount()==0, "vehicle badge data refcount isn't zero");
		pNewData->AddRef();

		return firstFreeIdx;
	}

	return -1;
}


///////////////////////////////////////////////////////////////////////////////
//  Remove
///////////////////////////////////////////////////////////////////////////////

void CVehicleBadgeManager::Remove(const CVehicle* pVehicle, const u32 badgeIndex)
{
	// check that a request isn't already in progress for this vehicle
	for (int i=0; i<m_requests.GetCount(); i++)
	{
		if (m_requests[i].m_currState!=DECALREQUESTSTATE_NOT_ACTIVE && m_requests[i].m_regdVeh==pVehicle && m_requests[i].m_badgeIndex == badgeIndex)
		{
			if (m_requests[i].m_currState==DECALREQUESTSTATE_SUCCEEDED)
			{
				// the request has succeeded but the result hasn't been queried yet
				// deactivate the request and remove the decal from the vehicle
				m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
				break;
			}
			else if (m_requests[i].m_currState>=DECALREQUESTSTATE_FAILED)
			{
				// the request has failed but the result hasn't been queried yet
				// just deactivate the request
				m_requests[i].m_currState = DECALREQUESTSTATE_NOT_ACTIVE;
				return;
			}
			else
			{
				// the request is still in progress
				// let it continue and flag it to remove once completed
				m_requests[i].m_removeOnceComplete = true;
				return;
			}
		}
	}

	decalBucket* pBucket = g_decalMan.FindFirstBucket(1<<(DECALTYPE_VEHICLE_BADGE+badgeIndex), pVehicle);
	if (pBucket)
	{
		u16 renderSettingsIndex = pBucket->GetRenderSettingsIndex();
		const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
		if (pRenderSettings)
		{
			u32 decalId = pRenderSettings->id;
			if (decalVerifyf(decalId>=DECALID_VEHICLE_BADGE_FIRST && decalId<DECALID_VEHICLE_BADGE_FIRST+DECALID_VEHICLE_BADGE_NUM, "vehicle badge decal id out of range"))
			{
				CVehicleBadgeData* pData = &m_vehicleBadgeData[decalId-DECALID_VEHICLE_BADGE_FIRST];

				if (decalVerifyf(pData->GetRefCount()>0, "invalid ref count on decal vehicle badge data"))
				{
					pData->GetVehicleBadgeDesc().Release();
					pData->RemoveRef(decalId);

					pVehicle->ClearEmblemMaterialGroup();
					DECALMGR.Remove(pVehicle, -1, NULL, ~(1U<<(DECALTYPE_VEHICLE_BADGE+badgeIndex)));
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::Init(unsigned initMode)
{    
	if (initMode == INIT_CORE)
	{
		m_asyncTaskDescPool.Init(DECAL_MAX_ASYNC_PROCESS_INST_TASKS);

		m_numNonPlayerVehicleBangsScuffsThisFrame = 0;

		m_disablePetrolDecalsIgniting = false;
		m_disablePetrolDecalsIgnitingThisFrame = false;
		m_disablePetrolDecalsRecyclingThisFrame = false;
		m_disableRenderingThisFrameUT = false;
		m_disableRenderingThisFrameRT = false;

		m_hasRunCompleteAsync = false;

		m_disableFootprintsScriptThreadId = THREAD_INVALID;
		m_disableFootprintsFromScript = false;

		m_disableCompositeShotgunImpactsScriptThreadId = THREAD_INVALID;
		m_disableCompositeShotgunImpactsFromScript = false;

		m_disableScuffsScriptThreadId = THREAD_INVALID;
		m_disableScuffsFromScript = false;

		m_scriptedTrailData.m_isActive = false;

		rage_new decalManager;

		// Decal manager renderstate overiddes.
		grcDepthStencilStateDesc depthStencilBlockIgnoreDesc;
		depthStencilBlockIgnoreDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		depthStencilBlockIgnoreDesc.DepthWriteMask = 0;
		depthStencilBlockIgnoreDesc.StencilEnable = true;
		depthStencilBlockIgnoreDesc.StencilReadMask = (DEFERRED_MATERIAL_TYPE_MASK ^ 0x2) | DEFERRED_MATERIAL_VEHICLE_BADGE_LOCAL_PLAYER;
		depthStencilBlockIgnoreDesc.StencilWriteMask = 0x0;
		depthStencilBlockIgnoreDesc.FrontFace.StencilFunc = grcRSV::CMP_NOTEQUAL;
		depthStencilBlockIgnoreDesc.BackFace.StencilFunc = grcRSV::CMP_NOTEQUAL;

		grcDepthStencilStateDesc depthStencilBlockExclusiveDesc;
		depthStencilBlockExclusiveDesc.DepthFunc = rage::FixupDepthDirection(grcRSV::CMP_LESSEQUAL);
		depthStencilBlockExclusiveDesc.DepthWriteMask = 0;
		depthStencilBlockExclusiveDesc.StencilEnable = true;
		depthStencilBlockExclusiveDesc.StencilReadMask = DEFERRED_MATERIAL_TYPE_MASK | DEFERRED_MATERIAL_VEHICLE_BADGE_LOCAL_PLAYER;
		depthStencilBlockExclusiveDesc.StencilWriteMask = 0x0;
		depthStencilBlockExclusiveDesc.FrontFace.StencilFunc = grcRSV::CMP_EQUAL;
		depthStencilBlockExclusiveDesc.BackFace.StencilFunc = grcRSV::CMP_EQUAL;

		grcDepthStencilStateHandle handleIgnore = grcStateBlock::CreateDepthStencilState(depthStencilBlockIgnoreDesc);
		grcDepthStencilStateHandle handleExclusive = grcStateBlock::CreateDepthStencilState(depthStencilBlockExclusiveDesc);
		
		DECALMGR.SetDynamicDecalDepthStencilState(DECAL_RENDER_PASS_VEHICLE_BADGE_LOCAL_PLAYER, handleExclusive, DEFERRED_MATERIAL_VEHICLE | DEFERRED_MATERIAL_VEHICLE_BADGE_LOCAL_PLAYER);
		DECALMGR.SetStaticDecalDepthStencilState(DECAL_RENDER_PASS_VEHICLE_BADGE_LOCAL_PLAYER, handleExclusive, DEFERRED_MATERIAL_VEHICLE | DEFERRED_MATERIAL_VEHICLE_BADGE_LOCAL_PLAYER);

		DECALMGR.SetDynamicDecalDepthStencilState(DECAL_RENDER_PASS_VEHICLE_BADGE_NOT_LOCAL_PLAYER, handleExclusive, DEFERRED_MATERIAL_VEHICLE);
		DECALMGR.SetStaticDecalDepthStencilState(DECAL_RENDER_PASS_VEHICLE_BADGE_NOT_LOCAL_PLAYER, handleExclusive, DEFERRED_MATERIAL_VEHICLE);

		// Only stencil test in deferred, forward decals still need to show up on peds (like broken windows and whatnot)
		DECALMGR.SetDynamicDecalDepthStencilState(DECAL_RENDER_PASS_MISC, handleIgnore, DEFERRED_MATERIAL_PED);
		DECALMGR.SetStaticDecalDepthStencilState(DECAL_RENDER_PASS_MISC, handleIgnore, DEFERRED_MATERIAL_PED);

#if GTA_REPLAY
		m_ReplayMergedDecalEvents.Reset();
		m_ReplayUVMultChanges.Reset();
#endif //GTA_REPLAY

#		if __BANK
			m_decalDebug.Init();
#		endif
	}
	else if (initMode == INIT_AFTER_MAP_LOADED)
	{
	}
	else if (initMode == INIT_SESSION)
	{
		m_vehicleBadgeMgr.Init();
	}

	decalManager *const dm = &DECALMGR;
	dm->Init(initMode);
#	if __PS3
		dm->RegisterSpuTask(DECAL_DO_PROCESSBOUND,               TASK_INTERFACE(DecalProcessBound));
		dm->RegisterSpuTask(DECAL_DO_PROCESSDRAWABLESNONSKINNED, TASK_INTERFACE(DecalProcessDrawablesNonSkinned));
		dm->RegisterSpuTask(DECAL_DO_PROCESSDRAWABLESSKINNED,    TASK_INTERFACE(DecalProcessDrawablesSkinned));
		dm->RegisterSpuTask(DECAL_DO_PROCESSSMASHGROUP,          TASK_INTERFACE(DecalProcessSmashGroup));
#	endif

	static_cast<CDecalCallbacks*>(g_pDecalCallbacks)->GlobalInit(PGTAMATERIALMGR->GetMaterialArray());

	//Create static blend state override after rage decal manager init
	if (initMode == INIT_CORE)
	{
#if APPLY_DOF_TO_ALPHA_DECALS
		if (GRCDEVICE.GetDxFeatureLevel() > 1000)
		{
			grcBlendStateDesc blendDesc = dm->GetStaticBlendStateDesc();
			PostFX::SetupBlendForDOF(blendDesc, false);
			m_StaticForwardBlendState = grcStateBlock::CreateBlendState(blendDesc);

			DECALMGR.SetOverrideBlendStateFunctor(MakeFunctorRet(OverrideStaticBlendStateFunctor));
		}
#endif //APPLY_DOF_TO_ALPHA_DECALS
	}
}


///////////////////////////////////////////////////////////////////////////////
//  InitDefinitions
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::InitDefinitions	()
{
	vfxUtil::InitTxd("fxdecal");
	InitSettings();
}


///////////////////////////////////////////////////////////////////////////////
//  InitSettings
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::InitSettings()
{
	// read in the data
	int numRenderSettingsFiles = 0;
	decalRenderSettingsInfo renderSettingsInfos[DECAL_MAX_RENDER_SETTINGS_FILES];
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::DECALS_FILE);
	while (DATAFILEMGR.IsValid(pData))
	{
		renderSettingsInfos[numRenderSettingsFiles].pFilename = pData->m_filename;
		numRenderSettingsFiles++;

		pData = DATAFILEMGR.GetNextFile(pData);
	}

	DECALMGR.InitSettings(DECALTYPE_NUM, g_typeSettings, numRenderSettingsFiles, &renderSettingsInfos[0]);
}

#if RSG_PC
void CDecalManager::ResetShaders()
{
	decalManager::GetInstance().ResetShaders();
	static_cast<CDecalCallbacks*>(g_pDecalCallbacks)->ResetShaders();
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  Shutdown
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::Shutdown(unsigned shutdownMode)
{
	if (shutdownMode == SHUTDOWN_CORE)
	{
	}
	else if (shutdownMode == SHUTDOWN_WITH_MAP_LOADED)
	{
		vfxUtil::ShutdownTxd("fxdecal");
	}	
	else if (shutdownMode == SHUTDOWN_SESSION)
	{
		m_vehicleBadgeMgr.Shutdown();
	}

	DECALMGR.Shutdown(shutdownMode);

	if (shutdownMode == SHUTDOWN_CORE)
	{
		delete &DECALMGR;
		m_asyncTaskDescPool.Shutdown();
	}
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateCompleteAsync
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::UpdateCompleteAsync()
{

	PF_PUSH_TIMEBAR("Decal Manager UpdateCompleteAsync");
	decalAssert(!m_hasRunCompleteAsync);
	DECALMGR.UpdateCompleteAsync();
	m_hasRunCompleteAsync = true;
	m_disableRenderingThisFrameRT = m_disableRenderingThisFrameUT;
	m_disableRenderingThisFrameUT = false;
	PF_POP_TIMEBAR();
}


///////////////////////////////////////////////////////////////////////////////
//  UpdateStartAsync
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::UpdateStartAsync(float dt)
{
	if (Unlikely(!m_hasRunCompleteAsync))
	{
		return;
	}
	m_hasRunCompleteAsync = false;

	PF_PUSH_TIMEBAR("Decal Manager UpdateStartAsync");

	m_numNonPlayerVehicleBangsScuffsThisFrame = 0;

	const bool isGamePaused = fwTimer::IsGamePaused();
	if (!isGamePaused REPLAY_ONLY( || CReplayMgr::IsEditModeActive() ) )
	{
#		if __BANK
			m_decalDebug.Update();
#		endif
		m_vehicleBadgeMgr.Update();
	}

	decalManager *const dm = &DECALMGR;
	dm->UpdateStartAsync(dt);

	if (!isGamePaused)
	{
		// wash textures slightly if it's raining
		if (g_DrawRand.GetRanged(0.0f, 2.0f)<g_weather.GetRain())
		{
			dm->Wash(0.004f, NULL, true);
		}

		// reset update flags
		m_disablePetrolDecalsIgnitingThisFrame = false;
		m_disablePetrolDecalsRecyclingThisFrame = false;

		// debug
#		if DECAL_SHOW_BROKEN_FRAG_MTX_PROBLEM
			m_decalDebug.OutputPlayerVehicleWindscreenMatrix();
#		endif
	}

	PF_POP_TIMEBAR();
}


///////////////////////////////////////////////////////////////////////////////
//  Render
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::Render(decalRenderPass firstRenderPass, decalRenderPass lastRenderPass, bool bIsForwardPass, bool bResetRenderCtxIndex, bool bDynamicOnly)
{
	u32 exceptionTypeFlags = 0;

	if (m_disableRenderingThisFrameRT)
	{
		exceptionTypeFlags |= 1U<<DECALTYPE_MARKER | 1U<<DECALTYPE_MARKER_LONG_RANGE;
		for (int i=0; i<DECAL_NUM_VEHICLE_BADGES; i++)
		{
			exceptionTypeFlags |= 1U<<(DECALTYPE_VEHICLE_BADGE+i);
		}

		exceptionTypeFlags = ~exceptionTypeFlags;
	}

	if (!CVfxHelper::ShouldRenderInGameUI())
	{
		exceptionTypeFlags |= 1U<<DECALTYPE_MARKER | 1U<<DECALTYPE_MARKER_LONG_RANGE;
	}

	if (exceptionTypeFlags!=0xffffffff)
	{
		DECALMGR.Render(firstRenderPass, lastRenderPass, exceptionTypeFlags, bIsForwardPass, bResetRenderCtxIndex, bDynamicOnly);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  AllocAsyncTaskDesk
///////////////////////////////////////////////////////////////////////////////

decalAsyncTaskDescBase* CDecalManager::AllocAsyncTaskDesk()
{
	return m_asyncTaskDescPool.AllocAsyncTaskDesk();
}


///////////////////////////////////////////////////////////////////////////////
//  FreeAsyncTaskDesc
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::FreeAsyncTaskDesc(decalAsyncTaskDescBase* desc)
{
	m_asyncTaskDescPool.FreeAsyncTaskDesc(desc);
}


///////////////////////////////////////////////////////////////////////////////
//  RenderDebug
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void CDecalManager::RenderDebug()
{
	DECALMGR.RenderDebug();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  InitWidgets
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void CDecalManager::InitWidgets()
{
	// init the framework widgets
	DECALMGR.InitWidgets();

	// init the game widgets
	m_decalDebug.InitWidgets();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  ReloadSettings
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void CDecalManager::ReloadSettings()
{
	InitSettings();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  AddBang
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddBang(const VfxMaterialInfo_s& vfxMaterialInfo, const VfxCollInfo_s& vfxCollInfo)
{
#if __BANK
	if (m_decalDebug.GetDisableDecalType(DECALTYPE_BANG))
	{
		return 0;
	}
#endif

	if (PGTAMATERIALMGR->GetMtlFlagNoDecal(vfxCollInfo.materialId))
	{
		return 0;
	}

	if (vfxCollInfo.regdEnt->GetType()==ENTITY_TYPE_PED)
	{
		return 0;
	}

	if (vfxMaterialInfo.decalRenderSettingIndex>-1)
	{
		s32 renderSettingsIndex = GetRenderSettingsIndex(vfxMaterialInfo.decalRenderSettingIndex, vfxMaterialInfo.decalRenderSettingCount);
		if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
		{
			const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
			if (pRenderSettings->pDiffuseMap)
			{
				// calc the width
				float width = vfxMaterialInfo.texWidthMin + vfxCollInfo.force*(vfxMaterialInfo.texWidthMax-vfxMaterialInfo.texWidthMin);

				float length = width * g_DrawRand.GetRanged(vfxMaterialInfo.texLengthMultMin, vfxMaterialInfo.texLengthMultMax);

				// calc normal
				Vec3V vNewNormal = Normalize(vfxCollInfo.vNormalWld);

				// calc side
				Vec3V vSide;
				if (CVfxHelper::GetRandomTangent(vSide, vNewNormal)==false)
				{
					return 0;
				}

				// set up the decal settings
				decalUserSettings decalSettings;

				decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_BANG;
				decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
				decalSettings.bucketSettings.roomId = (s16)vfxCollInfo.roomId;
				
				decalSettings.instSettings.vPosition = vfxCollInfo.vPositionWld;		
				decalSettings.instSettings.vDirection = -vNewNormal;			
				decalSettings.instSettings.vSide = vSide;				
				decalSettings.instSettings.vDimensions = Vec3V(width, length, decalSettingsManager::GetTypeSettings(DECALTYPE_BANG)->depth);			
				decalSettings.instSettings.colR = vfxMaterialInfo.decalColR;		
				decalSettings.instSettings.colG = vfxMaterialInfo.decalColG;		
				decalSettings.instSettings.colB = vfxMaterialInfo.decalColB;			
				decalSettings.instSettings.flipU = fwRandom::GetRandomTrueFalse();			
				decalSettings.instSettings.flipV = fwRandom::GetRandomTrueFalse();		

				decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
				decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
				decalSettings.pipelineSettings.duplicateRejectDist = vfxMaterialInfo.duplicateRejectDist;
				decalSettings.pipelineSettings.maxOverlayRadius = vfxMaterialInfo.maxOverlayRadius;

				// in MP only allow a single non player vehicle bang/scuff per frame
				if (NetworkInterface::IsGameInProgress() && vfxCollInfo.regdEnt->GetIsTypeVehicle())
				{
					const CVehicle* pVehicle = static_cast<const CVehicle*>(vfxCollInfo.regdEnt.Get());
					if (pVehicle && !pVehicle->ContainsLocalPlayer())
					{
						if (m_numNonPlayerVehicleBangsScuffsThisFrame == 1 BANK_ONLY(-1+m_decalDebug.GetMaxNonPlayerVehicleBangsScuffsPerFrame()))
						{
							return 0;
						}

						m_numNonPlayerVehicleBangsScuffsThisFrame++;
					}
				}

				return AddSettings(vfxCollInfo.regdEnt, decalSettings);
			}
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddBloodSplat
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddBloodSplat(const VfxBloodInfo_s& vfxBloodInfo, const VfxCollInfo_s& vfxCollInfo, float width, bool doSprayDecal, bool doMistDecal)
{
#if __BANK
	if (m_decalDebug.GetDisableDecalType(DECALTYPE_BLOOD_SPLAT))
	{
		return 0;
	}
#endif

	if (vfxCollInfo.regdEnt->GetType()==ENTITY_TYPE_PED)
	{
		return 0;
	}

#if __ASSERT
	// we're not projecting onto a smash group - make sure the object is not smashable if we've got a glass collision
	if (PGTAMATERIALMGR->GetIsSmashableGlass(vfxCollInfo.materialId) && IsEntitySmashable(vfxCollInfo.regdEnt))
	{
		decalAssertf(0, "trying to project onto a smashable object without using a smash group");
	}
#endif

	s32 renderSettingIndex;
	s32 renderSettingCount;

	if (doMistDecal)
	{
		if (PGTAMATERIALMGR->GetMtlFlagIsPorous(vfxCollInfo.materialId))
		{
			renderSettingIndex = vfxBloodInfo.mistSoakRenderSettingIndex;
			renderSettingCount = vfxBloodInfo.mistSoakRenderSettingCount;
		}
		else
		{
			renderSettingIndex = vfxBloodInfo.mistNormRenderSettingIndex;
			renderSettingCount = vfxBloodInfo.mistNormRenderSettingCount;
		}
	}
	else if (doSprayDecal)
	{
		if (PGTAMATERIALMGR->GetMtlFlagIsPorous(vfxCollInfo.materialId))
		{
			renderSettingIndex = vfxBloodInfo.spraySoakRenderSettingIndex;
			renderSettingCount = vfxBloodInfo.spraySoakRenderSettingCount;
		}
		else
		{
			renderSettingIndex = vfxBloodInfo.sprayNormRenderSettingIndex;
			renderSettingCount = vfxBloodInfo.sprayNormRenderSettingCount;
		}
	}
	else
	{
		if (PGTAMATERIALMGR->GetMtlFlagIsPorous(vfxCollInfo.materialId))
		{
			renderSettingIndex = vfxBloodInfo.splatSoakRenderSettingIndex;
			renderSettingCount = vfxBloodInfo.splatSoakRenderSettingCount;
		}
		else
		{
			renderSettingIndex = vfxBloodInfo.splatNormRenderSettingIndex;
			renderSettingCount = vfxBloodInfo.splatNormRenderSettingCount;
		}
	}

	if (renderSettingIndex>-1)
	{
		s32 renderSettingsIndex = GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
		if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
		{
			const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
			if (pRenderSettings->pDiffuseMap)
			{
				// calc side
				Vec3V vSide;
				if (doSprayDecal)
				{
					if (CVfxHelper::GetRandomTangentAlign(vSide, vfxCollInfo.vNormalWld, vfxCollInfo.vDirectionWld)==false)
					{
						return 0;
					}
				}
				else
				{
					if (CVfxHelper::GetRandomTangent(vSide, vfxCollInfo.vNormalWld)==false)
					{
						return 0;
					}
				}

				// set up the decal settings
				decalUserSettings decalSettings;

				decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_BLOOD_SPLAT;
				decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
				decalSettings.bucketSettings.roomId = (s16)vfxCollInfo.roomId;

				decalSettings.instSettings.vPosition = vfxCollInfo.vPositionWld;			
				decalSettings.instSettings.vDirection = -vfxCollInfo.vNormalWld;			
				decalSettings.instSettings.vSide = vSide;				
				decalSettings.instSettings.vDimensions = Vec3V(width, width, decalSettingsManager::GetTypeSettings(DECALTYPE_BLOOD_SPLAT)->depth);			
				decalSettings.instSettings.totalLife = g_DrawRand.GetRanged(vfxBloodInfo.lifeMin, vfxBloodInfo.lifeMax);			
				decalSettings.instSettings.fadeInTime = vfxBloodInfo.fadeInTime;		
				decalSettings.instSettings.uvMultStart = g_DrawRand.GetRanged(vfxBloodInfo.growthMultMin, vfxBloodInfo.growthMultMax);		
				decalSettings.instSettings.uvMultEnd = 1.0f;			
				decalSettings.instSettings.uvMultTime = vfxBloodInfo.growthTime;			
				decalSettings.instSettings.colR = vfxBloodInfo.decalColR;				
				decalSettings.instSettings.colG = vfxBloodInfo.decalColG;				
				decalSettings.instSettings.colB = vfxBloodInfo.decalColB;					
	
				decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
				decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
				decalSettings.pipelineSettings.duplicateRejectDist = width*DECAL_BLOOD_SPLAT_DRD_WIDTH_MULT;
				decalSettings.pipelineSettings.colnComponentId = (s16)vfxCollInfo.componentId;
#if DECAL_PROCESS_BREAKABLES
				decalSettings.pipelineSettings.applyToBreakables = true;		// blood should get applied to breakable glass 
#endif

				// override settings for glass collisions
				if (PGTAMATERIALMGR->GetIsGlass(vfxCollInfo.materialId))
				{
					decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_FORWARD;
				}

				int id = AddSettings(vfxCollInfo.regdEnt, decalSettings);

#if GTA_REPLAY
				CReplayMgr::RecordFx<CPacketBloodDecal>(CPacketBloodDecal(decalSettings, id), vfxCollInfo.regdEnt);
#endif // GTA_REPLAY
				
				return id;
			}
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddFootprint
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddFootprint(int decalRenderSettingIndex, int decalRenderSettingCount, float width, float length, Color32 col, phMaterialMgr::Id mtlId, Vec3V_In vPos, Vec3V_In vNormal, Vec3V_In vSide, bool isLeftFoot, float life)
{
#if __BANK
	if (m_decalDebug.GetDisableDecalType(DECALTYPE_FOOTPRINT))
	{
		return 0;
	}
#endif

	if (m_disableFootprintsFromScript)
	{
		return 0;
	}

	if (decalRenderSettingIndex>-1)
	{
		s32 renderSettingsIndex = GetRenderSettingsIndex(decalRenderSettingIndex, decalRenderSettingCount);
		if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
		{
			const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
			if (pRenderSettings->pDiffuseMap)
			{
				// calc the width
				if (width>0.0f && length>0.0f)
				{
					decalAssertf(mtlId==-1 || (mtlId&0xffffffffffffff00)==0, "decal exclusive mtl id contains other packed info");

					// set up the decal settings
					decalUserSettings decalSettings;

					decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_FOOTPRINT;
					decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
					decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;

					decalSettings.instSettings.vPosition = vPos;			
					decalSettings.instSettings.vDirection = -vNormal;			
					decalSettings.instSettings.vSide = vSide;				
					decalSettings.instSettings.vDimensions = Vec3V(width, length, decalSettingsManager::GetTypeSettings(DECALTYPE_FOOTPRINT)->depth);			
					decalSettings.instSettings.totalLife = life;			
					decalSettings.instSettings.colR = col.GetRed();		
					decalSettings.instSettings.colG = col.GetGreen();		
					decalSettings.instSettings.colB = col.GetBlue();		
					decalSettings.instSettings.alphaFront = col.GetAlpha();			
					decalSettings.instSettings.alphaBack = col.GetAlpha();
					decalSettings.instSettings.flipU = isLeftFoot;		

					decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
					decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
					decalSettings.pipelineSettings.duplicateRejectDist = 0.025f;
					decalSettings.pipelineSettings.exclusiveMtlId = mtlId;

					// override the depth for 'deep' materials
					if (PGTAMATERIALMGR->GetIsDeepNonWading(mtlId))
					{
						decalSettings.instSettings.vDimensions.SetZ(ScalarV(V_HALF));
					}
	
					return AddSettings(NULL, decalSettings);
				}
			}
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddGeneric
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddGeneric(DecalType_e decalType, s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vSide, float width, float height, float depth, float totalLife, float fadeInTime, float uvMultStart, float uvMultEnd, float uvMultTime, Color32 col, bool flipU, bool flipV, float duplicateRejectDist, bool isScripted, bool isDynamic REPLAY_ONLY(, float startLife))
{
#if __BANK
	if ((decalType==DECALTYPE_GENERIC_SIMPLE_COLN && m_decalDebug.GetDisableDecalType(DECALTYPE_GENERIC_SIMPLE_COLN)) ||
		(decalType==DECALTYPE_GENERIC_COMPLEX_COLN && m_decalDebug.GetDisableDecalType(DECALTYPE_GENERIC_COMPLEX_COLN)) ||
		(decalType==DECALTYPE_GENERIC_SIMPLE_COLN_LONG_RANGE && m_decalDebug.GetDisableDecalType(DECALTYPE_GENERIC_SIMPLE_COLN_LONG_RANGE)) ||
		(decalType==DECALTYPE_GENERIC_COMPLEX_COLN_LONG_RANGE && m_decalDebug.GetDisableDecalType(DECALTYPE_GENERIC_COMPLEX_COLN_LONG_RANGE)))
	{
		return 0;
	}
#endif

	// if the passed in depth is zero use the default
	if (depth==0.0f)
	{
		depth = decalSettingsManager::GetTypeSettings(decalType)->depth;
	}

	if (renderSettingIndex>-1)
	{
		s32 renderSettingsIndex = GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
		if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
		{
			const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
			if (pRenderSettings->pDiffuseMap)
			{	
				// set up the decal settings
				decalUserSettings decalSettings;

				decalSettings.bucketSettings.typeSettingsIndex = (u16)decalType;
				decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
				decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;
				decalSettings.bucketSettings.isScripted = isScripted;

				decalSettings.instSettings.vPosition = vPos;			
				decalSettings.instSettings.vDirection = vDir;			
				decalSettings.instSettings.vSide = vSide;				
				decalSettings.instSettings.vDimensions = Vec3V(width, height, depth);		
				decalSettings.instSettings.totalLife = totalLife;			
				decalSettings.instSettings.fadeInTime = fadeInTime;			
				decalSettings.instSettings.uvMultStart = uvMultStart;
				decalSettings.instSettings.uvMultEnd = uvMultEnd;	
				decalSettings.instSettings.uvMultTime = uvMultTime;	
				decalSettings.instSettings.colR = col.GetRed();
				decalSettings.instSettings.colG = col.GetGreen();
				decalSettings.instSettings.colB = col.GetBlue();
				decalSettings.instSettings.alphaFront = col.GetAlpha();	
				decalSettings.instSettings.alphaBack = col.GetAlpha();		
				decalSettings.instSettings.flipU = flipU;			
				decalSettings.instSettings.flipV = flipV;			

				decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
				decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
				decalSettings.pipelineSettings.duplicateRejectDist = duplicateRejectDist;

#if GTA_REPLAY
				//On replay if we add decals at the start of the clip that were added before recording started we need them to be visible
				//and not faded out. If the life is under DECAL_INST_INV_FADE_TIME_UNITS_HZ they'll be faded even if the fadeInTime is 0.
				decalSettings.lodSettings.currLife = startLife;
#endif //GTA_REPLAY

				if (isDynamic)
				{
					decalSettings.pipelineSettings.method = DECAL_METHOD_DYNAMIC;
				}

				return AddSettings(NULL, decalSettings);
			}
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddPool
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddPool(VfxLiquidType_e liquidType, s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPosition, Vec3V_In vNormal, float startSize, float endSize, float growRate, u8 colnMtlId, Color32 col, bool isScripted)
{
#if __BANK
	if (m_decalDebug.GetDisableDecalType(DECALTYPE_POOL_LIQUID))
	{
		return 0;
	}
#endif

	if (liquidType!=VFXLIQUID_TYPE_NONE)
	{
		if (liquidType==VFXLIQUID_TYPE_PETROL)
		{
			if (g_fireMan.ManageAddedPetrol(vPosition, 1.0f))
			{
				return 0;
			}
		}
		
		VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(liquidType);
		if (PGTAMATERIALMGR->GetMtlFlagIsPorous((phMaterialMgr::Id)colnMtlId))
		{
			renderSettingIndex = liquidInfo.soakDecalPoolRenderSettingIndex;
			renderSettingCount = liquidInfo.soakDecalPoolRenderSettingCount;
		}
		else
		{
			renderSettingIndex = liquidInfo.normDecalPoolRenderSettingIndex;
			renderSettingCount = liquidInfo.normDecalPoolRenderSettingCount;
		}

		// multiply the liquid colour by the particle colour
		u8 colR = static_cast<u8>(liquidInfo.decalColR * col.GetRedf());
		u8 colG = static_cast<u8>(liquidInfo.decalColG * col.GetGreenf());
		u8 colB = static_cast<u8>(liquidInfo.decalColB * col.GetBluef());
		u8 colA = static_cast<u8>(liquidInfo.decalColA * col.GetAlphaf());
		col = Color32(colR, colG, colB, colA);
	}

	if (renderSettingIndex>-1)
	{
		s32 renderSettingsIndex = GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
		if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
		{
			const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
			if (pRenderSettings->pDiffuseMap)
			{
				// calc the width
				float width = endSize;

				// calc tangent
				Vec3V vSide;
				if (CVfxHelper::GetRandomTangent(vSide, vNormal)==false)
				{
					return 0;
				}

				// check that the start and end sizes aren't zero
				startSize = Max(startSize, 0.001f);
				endSize = Max(endSize, 0.001f);

				// set up the decal settings
				decalUserSettings decalSettings;

				decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_POOL_LIQUID;
				decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
				decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;
				decalSettings.bucketSettings.isScripted = isScripted;

				decalSettings.instSettings.vPosition = vPosition;			
				decalSettings.instSettings.vDirection = -vNormal;			
				decalSettings.instSettings.vSide = vSide;				
				decalSettings.instSettings.vDimensions = Vec3V(width, width, decalSettingsManager::GetTypeSettings(DECALTYPE_POOL_LIQUID)->depth);				
				decalSettings.instSettings.uvMultStart = startSize/endSize;
				decalSettings.instSettings.uvMultEnd = growRate;	
				decalSettings.instSettings.colR = col.GetRed();
				decalSettings.instSettings.colG = col.GetGreen();
				decalSettings.instSettings.colB = col.GetBlue();
				decalSettings.instSettings.alphaFront = col.GetAlpha();	
				decalSettings.instSettings.alphaBack = col.GetAlpha();	
				decalSettings.instSettings.liquidType = (s8)liquidType;			
				decalSettings.instSettings.isLiquidPool = true;			

				decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
				decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;

				int id = AddSettings(NULL, decalSettings);

#if GTA_REPLAY
				CReplayMgr::RecordFx<CPacketBloodPool>(CPacketBloodPool(decalSettings, id));
#endif // GTA_REPLAY

				return id;
			}
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddScuff
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddScuff(CEntity* pEntity, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, float life, u8 mtlId, float maxOverlayRadius, float duplicateRejectDist, bool REPLAY_ONLY(disableFadeInForReplay))
{
#if __BANK
	if (m_decalDebug.GetDisableDecalType(DECALTYPE_SCUFF))
	{
		return 0;
	}
#endif

	if (m_disableScuffsFromScript)
	{
		return 0;
	}

	// test for early rejection
	if (IsEqualAll(vPtA, vPtB))
	{
		return 0;
	}

	// 
	if (renderSettingsIndex>-1 && renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
	{
		const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
		if (pRenderSettings->pDiffuseMap)
		{
			// calc the mid point
			Vec3V vAtoB = vPtB-vPtA;
			Vec3V vMidPt = vPtA + (vAtoB*ScalarV(V_HALF));

			// calc the forward vector
			Vec3V vForward = Normalize(vAtoB);

			// calc the side vector
			Vec3V vSide = Cross(vForward, vNormal);
			vSide = Normalize(vSide);

			// return if the forward and normal are equal (i.e. the cross product is zero)
			if (MagSquared(vSide).Getf()==0.0f)
			{
				return 0;
			}

			// calc the new normal
			Vec3V vNewNormal = Cross(vSide, vForward);
			vNewNormal = Normalize(vNewNormal);

			// calc the distance between the 2 points
			float distAB = Mag(vAtoB).Getf();

			// set up the decal settings
			decalUserSettings decalSettings;

			decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_SCUFF;
			decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
			decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;

			decalSettings.instSettings.vPosition = vMidPt;			
			decalSettings.instSettings.vDirection = -vNewNormal;			
			decalSettings.instSettings.vSide = vSide;				
			decalSettings.instSettings.vDimensions = Vec3V(width, distAB, decalSettingsManager::GetTypeSettings(DECALTYPE_SCUFF)->depth);			
			decalSettings.instSettings.totalLife = life;			
			decalSettings.instSettings.colR = col.GetRed();
			decalSettings.instSettings.colG = col.GetGreen();
			decalSettings.instSettings.colB = col.GetBlue();
			decalSettings.instSettings.alphaFront = col.GetAlpha();
			decalSettings.instSettings.alphaBack = col.GetAlpha();		
			decalSettings.instSettings.flipU = fwRandom::GetRandomTrueFalse();	
			decalSettings.instSettings.flipV = fwRandom::GetRandomTrueFalse();		

#if GTA_REPLAY
			//on the first frame of replay we dont want these to fade in
			if( disableFadeInForReplay )
				decalSettings.instSettings.fadeInTime = 0.0f;
#endif //GTA_REPLAY

			decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
			decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
			decalSettings.pipelineSettings.duplicateRejectDist = duplicateRejectDist;
			decalSettings.pipelineSettings.maxOverlayRadius = maxOverlayRadius;

			// override the depth for 'deep' materials
			if (PGTAMATERIALMGR->GetIsDeepNonWading(mtlId))
			{
				decalSettings.instSettings.vDimensions.SetZ(ScalarV(V_HALF));
			}

			// in MP only allow a single non player vehicle bang/scuff per frame
			if (NetworkInterface::IsGameInProgress() && pEntity->GetIsTypeVehicle())
			{
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
				if (pVehicle && !pVehicle->ContainsLocalPlayer())
				{
					if (m_numNonPlayerVehicleBangsScuffsThisFrame == 1 BANK_ONLY(-1+m_decalDebug.GetMaxNonPlayerVehicleBangsScuffsPerFrame()))
					{
						return 0;
					}

					m_numNonPlayerVehicleBangsScuffsThisFrame++;
				}
			}

			int decalId = AddSettings(pEntity, decalSettings);

			return decalId;
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddTrail
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddTrail(DecalType_e decalType, VfxLiquidType_e liquidType, s32 renderSettingsIndex, Vec3V_In vPtA, Vec3V_InOut vPtB, Vec3V_In vNormal, float width, Color32 col, u8 alphaBack, float life, u8 mtlId, float& currLength, Vec3V_Ptr pvJoinVerts, bool isScripted, DecalTrailPiece_e trailPiece)
{
#if __BANK
	if ((decalType==DECALTYPE_TRAIL_SKID && m_decalDebug.GetDisableDecalType(DECALTYPE_TRAIL_SKID)) ||
		(decalType==DECALTYPE_TRAIL_SCRAPE && m_decalDebug.GetDisableDecalType(DECALTYPE_TRAIL_SCRAPE)) ||
		(decalType==DECALTYPE_TRAIL_LIQUID && m_decalDebug.GetDisableDecalType(DECALTYPE_TRAIL_LIQUID)) ||
		(decalType==DECALTYPE_TRAIL_GENERIC && m_decalDebug.GetDisableDecalType(DECALTYPE_TRAIL_GENERIC)))
	{
		return 0;
	}
#endif

	// test for early rejection
	if (IsEqualAll(vPtA, vPtB))
	{
		return 0;
	}

	// override liquid decal render settings
	if (liquidType!=VFXLIQUID_TYPE_NONE)
	{
		if (liquidType==VFXLIQUID_TYPE_PETROL)
		{
			g_fireMan.ManageAddedPetrol(vPtA, 0.25f);
		}

		VfxLiquidInfo_s& liquidInfo = g_vfxLiquid.GetLiquidInfo(liquidType);
		if (PGTAMATERIALMGR->GetMtlFlagIsPorous((phMaterialMgr::Id)mtlId))
		{
			if (trailPiece==DECALTRAIL_IN)
			{
				renderSettingsIndex = GetRenderSettingsIndex(liquidInfo.soakDecalTrailInRenderSettingIndex, liquidInfo.soakDecalTrailInRenderSettingCount);
			}
			else if (trailPiece==DECALTRAIL_MID)
			{
				renderSettingsIndex = GetRenderSettingsIndex(liquidInfo.soakDecalTrailMidRenderSettingIndex, liquidInfo.soakDecalTrailMidRenderSettingCount);
			}
			else if (trailPiece==DECALTRAIL_OUT)
			{
				renderSettingsIndex = GetRenderSettingsIndex(liquidInfo.soakDecalTrailOutRenderSettingIndex, liquidInfo.soakDecalTrailOutRenderSettingCount);
			}
		}
		else
		{
			if (trailPiece==DECALTRAIL_IN)
			{
				renderSettingsIndex = GetRenderSettingsIndex(liquidInfo.normDecalTrailInRenderSettingIndex, liquidInfo.normDecalTrailInRenderSettingCount);
			}
			else if (trailPiece==DECALTRAIL_MID)
			{
				renderSettingsIndex = GetRenderSettingsIndex(liquidInfo.normDecalTrailMidRenderSettingIndex, liquidInfo.normDecalTrailMidRenderSettingCount);
			}
			else if (trailPiece==DECALTRAIL_OUT)
			{
				renderSettingsIndex = GetRenderSettingsIndex(liquidInfo.normDecalTrailOutRenderSettingIndex, liquidInfo.normDecalTrailOutRenderSettingCount);
			}
		}
	}

	// 
	if (renderSettingsIndex>-1 && renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
	{
		const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
		if (pRenderSettings->pDiffuseMap)
		{
			// calc the mid point
			Vec3V vAtoB = vPtB-vPtA;
			Vec3V vMidPt = vPtA + (vAtoB*ScalarV(V_HALF));

			// calc the forward vector
			Vec3V vForward = Normalize(vAtoB);

			// calc the side vector
			Vec3V vSide = Cross(vForward, vNormal);
			vSide = Normalize(vSide);

			// return if the forward and normal are equal (i.e. the cross product is zero)
			if (MagSquared(vSide).Getf()==0.0f)
			{
				return 0;
			}

			// calc the new normal
			Vec3V vNewNormal = Cross(vSide, vForward);
			vNewNormal = Normalize(vNewNormal);

// 			grcDebugDraw::Line(vPtA, vPtB, Color32(1.0f, 0.0f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 1.0f, 0.5f), -200);
// 			grcDebugDraw::Line(vMidPt, vMidPt+(vNormal*ScalarVFromF32(0.2f)), Color32(0.0f, 1.0f, 0.0f, 1.0f), Color32(0.0f, 1.0f, 0.0f, 1.0f), -200);
// 			grcDebugDraw::Line(vMidPt, vMidPt+(vNewNormal*ScalarVFromF32(0.2f)), Color32(1.0f, 1.0f, 0.0f, 1.0f), Color32(1.0f, 1.0f, 0.0f, 1.0f), -200);

			// calc the distance between the 2 points
			float distAB = Mag(vAtoB).Getf();

			// set up the decal settings
			decalUserSettings decalSettings;

			decalSettings.bucketSettings.typeSettingsIndex = (u16)decalType;
			decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
			decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;
			decalSettings.bucketSettings.isScripted = isScripted;

			decalSettings.instSettings.vPosition = vMidPt;			
			decalSettings.instSettings.vDirection = -vNewNormal;			
			decalSettings.instSettings.vSide = vSide;				
			decalSettings.instSettings.vDimensions = Vec3V(width, distAB, decalSettingsManager::GetTypeSettings(decalType)->depth);		
			decalSettings.instSettings.currWrapLength = currLength;		
			decalSettings.instSettings.totalLife = life;				
			decalSettings.instSettings.colR = col.GetRed();
			decalSettings.instSettings.colG = col.GetGreen();
			decalSettings.instSettings.colB = col.GetBlue();
			decalSettings.instSettings.alphaFront = col.GetAlpha();	
			decalSettings.instSettings.alphaBack = alphaBack;	
			decalSettings.instSettings.liquidType = (s8)liquidType;				

			decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
			decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
			decalSettings.pipelineSettings.hasJoinVerts = true;

			SetJoinVerts(decalSettings.pipelineSettings, pvJoinVerts);

			// override the depth for 'deep' materials
			if (PGTAMATERIALMGR->GetIsDeepNonWading(mtlId))
			{
				decalSettings.instSettings.vDimensions.SetZ(ScalarV(V_HALF));
			}

			int decalId = AddSettings(NULL, decalSettings);

			// update the current length of the mark
			GetJoinVerts(decalSettings.pipelineSettings, pvJoinVerts);
			currLength += distAB;

#if GTA_REPLAY
			CReplayMgr::RecordFx<CPacketTrailDecal>(CPacketTrailDecal(decalSettings, mtlId, decalId));
#endif // GTA_REPLAY

			return decalId;
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  AddVehicleBadge
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddVehicleBadge(const CVehicle* pVehicle, s32 componentId, s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha)
{
	decalAssertf(badgeIndex < DECAL_NUM_VEHICLE_BADGES, "Vehicle Badge Index out of range");

#if __BANK
	if (m_decalDebug.GetDisableDecalType(DecalType_e(DECALTYPE_VEHICLE_BADGE+badgeIndex)))
	{
		return 0;
	}
#endif

	if (renderSettingIndex>-1)
	{
		s32 renderSettingsIndex = GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
		if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
		{
			const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
			if (pRenderSettings->pDiffuseMap)
			{
				// set up the decal settings
				decalUserSettings decalSettings;

				decalSettings.bucketSettings.typeSettingsIndex = (u16)(DECALTYPE_VEHICLE_BADGE+badgeIndex);
				decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
				decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;

				decalSettings.instSettings.vPosition = vPos;			
				decalSettings.instSettings.vDirection = vDir;			
				decalSettings.instSettings.vSide = vSide;				
				decalSettings.instSettings.vDimensions = Vec3V(size, size, decalSettingsManager::GetTypeSettings(DECALTYPE_VEHICLE_BADGE+badgeIndex)->depth);				
				decalSettings.instSettings.colR = DECAL_VEHICLE_BADGE_TINT_R;	
				decalSettings.instSettings.colG = DECAL_VEHICLE_BADGE_TINT_G;	
				decalSettings.instSettings.colB = DECAL_VEHICLE_BADGE_TINT_B;	
				decalSettings.instSettings.alphaFront = alpha;		
				decalSettings.instSettings.alphaBack = alpha;		

#if GTA_REPLAY
				//force the decal to be on rather than fade in if we're adding as part of a clip.
				if( CReplayMgr::IsEditModeActive() )
				{
					decalSettings.lodSettings.currLife = DECAL_INST_INV_FADE_TIME_UNITS_HZ;
				}
#endif

				decalSettings.pipelineSettings.renderPass = isForLocalPlayer ? DECAL_RENDER_PASS_VEHICLE_BADGE_LOCAL_PLAYER : DECAL_RENDER_PASS_VEHICLE_BADGE_NOT_LOCAL_PLAYER;
				decalSettings.pipelineSettings.method = DECAL_METHOD_DYNAMIC;
				decalSettings.pipelineSettings.colnComponentId = (s16)componentId;
				decalSettings.pipelineSettings.useColnEntityOnly = true;

				return AddSettings(const_cast<CVehicle*>(pVehicle), decalSettings);
			}
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
//  AddWeaponImpact
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddWeaponImpact(const VfxWeaponInfo_s& vfxWeaponInfo, const VfxCollInfo_s& vfxCollInfo, void* pSmashGroup, CEntity* pSmashEntity, s32 smashMatrixId, bool applyBulletproofGlassTextureSwap, bool lookupCentreOfVehicleFromBone)
{
	if (vfxCollInfo.regdEnt->GetType()==ENTITY_TYPE_PED)
	{
		return 0;
	}

#if __ASSERT
	if (pSmashGroup)
	{
		// we're projecting onto a smash group - check that the object is smashable
		decalAssertf(IsEntitySmashable(vfxCollInfo.regdEnt), "trying to project a smash texture onto a non smashable object");
		decalAssertf(PGTAMATERIALMGR->GetIsSmashableGlass(vfxCollInfo.materialId), "trying to project onto non smashable glass material");
	}
	else
	{
		// we're not projecting onto a smash group - make sure the object is not smashable if we've got a glass collision
		if (PGTAMATERIALMGR->GetIsSmashableGlass(vfxCollInfo.materialId) && IsEntitySmashable(vfxCollInfo.regdEnt))
		{
			decalAssertf(0, "trying to project onto a smashable object without using a smash group");
		}
	}
#endif

	if (PGTAMATERIALMGR->GetMtlFlagNoDecal(vfxCollInfo.materialId))
	{
		return 0;
	}

	// check if this is a normal or stretch weapon impact
	float dot = Max(0.0f, Dot(-vfxCollInfo.vDirectionWld, vfxCollInfo.vNormalWld).Getf());
	float width = g_DrawRand.GetRanged(vfxWeaponInfo.minTexSize, vfxWeaponInfo.maxTexSize);
	float height = width;
	s32 decalRenderSettingIndex = vfxWeaponInfo.decalRenderSettingIndex;
	s32 decalRenderSettingCount = vfxWeaponInfo.decalRenderSettingCount;

	Vec3V vSide;
	if (vfxWeaponInfo.stretchDot>0.0f && dot<vfxWeaponInfo.stretchDot)
	{
		if (CVfxHelper::GetRandomTangentAlign(vSide, vfxCollInfo.vNormalWld, vfxCollInfo.vDirectionWld)==false)
		{
			return 0;
		}

		height = width + (width*(vfxWeaponInfo.stretchMult*((vfxWeaponInfo.stretchDot-dot)/vfxWeaponInfo.stretchDot)));

		decalRenderSettingIndex = vfxWeaponInfo.stretchRenderSettingIndex;
		decalRenderSettingCount = vfxWeaponInfo.stretchRenderSettingCount;
	}
	else
	{
		if (CVfxHelper::GetRandomTangent(vSide, vfxCollInfo.vNormalWld)==false)
		{
			return 0;
		}
	}

	if (applyBulletproofGlassTextureSwap)
	{
		g_decalMan.FindRenderSettingInfo(14501, decalRenderSettingIndex, decalRenderSettingCount);
	}

	if (vfxCollInfo.isSnowball)
	{
		// snowball specific hack
		g_decalMan.FindRenderSettingInfo(DECALID_SNOWBALL, decalRenderSettingIndex, decalRenderSettingCount);
	}

	if (vfxCollInfo.isFMJAmmo && vfxCollInfo.regdEnt->GetIsTypeVehicle() && !PGTAMATERIALMGR->GetIsGlass(vfxCollInfo.materialId))
	{
		// full metal jacket hack
		g_decalMan.FindRenderSettingInfo(14505, decalRenderSettingIndex, decalRenderSettingCount);
	}

	if (decalRenderSettingIndex>-1)
	{
		s32 renderSettingsIndex = GetRenderSettingsIndex(decalRenderSettingIndex, decalRenderSettingCount);
		if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
		{
			const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
			if (pRenderSettings->pDiffuseMap)
			{
				// set up the decal settings
				decalUserSettings decalSettings;

				decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_WEAPON_IMPACT;
				decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
				decalSettings.bucketSettings.subType = (u32)vfxCollInfo.isExitFx;
				decalSettings.bucketSettings.roomId = (s16)vfxCollInfo.roomId;

				decalSettings.instSettings.vPosition = vfxCollInfo.vPositionWld;			
				decalSettings.instSettings.vDirection = -vfxCollInfo.vNormalWld;			
				decalSettings.instSettings.vSide = vSide;				
				decalSettings.instSettings.vDimensions = Vec3V(width, height, decalSettingsManager::GetTypeSettings(DECALTYPE_WEAPON_IMPACT)->depth);			
				decalSettings.instSettings.totalLife = g_DrawRand.GetRanged(vfxWeaponInfo.minLifeTime, vfxWeaponInfo.maxLifeTime);			
				decalSettings.instSettings.fadeInTime = vfxWeaponInfo.fadeInTime;			
				decalSettings.instSettings.colR = vfxWeaponInfo.decalColR;				
				decalSettings.instSettings.colG = vfxWeaponInfo.decalColG;				
				decalSettings.instSettings.colB = vfxWeaponInfo.decalColB;				
				decalSettings.instSettings.alphaFront = vfxWeaponInfo.decalColA;		
				decalSettings.instSettings.alphaBack = vfxWeaponInfo.decalColA;	
				decalSettings.instSettings.flipU = fwRandom::GetRandomTrueFalse();			

				decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
				decalSettings.pipelineSettings.method = DECAL_METHOD_STATIC;
				decalSettings.pipelineSettings.duplicateRejectDist = vfxWeaponInfo.duplicateRejectDist;
				decalSettings.pipelineSettings.maxOverlayRadius = vfxWeaponInfo.maxOverlayRadius;
				decalSettings.pipelineSettings.dontApplyToGlass = true;

				if (vfxWeaponInfo.useExclusiveMtl)
				{
					decalAssertf(vfxCollInfo.materialId==-1 || (vfxCollInfo.materialId&0xffffffffffffff00)==0, "decal exclusive mtl id contains other packed info");
					decalSettings.pipelineSettings.exclusiveMtlId = vfxCollInfo.materialId;
				}

				if (applyBulletproofGlassTextureSwap)
				{
					decalSettings.pipelineSettings.duplicateRejectDist *= 0.25f;
				}

#if __BANK
				if (g_vfxWeapon.GetBulletImpactDecalOverridesActive())
				{
					decalSettings.pipelineSettings.duplicateRejectDist = g_vfxWeapon.GetBulletImpactDecalDuplicateRejectDistOverride();
					decalSettings.pipelineSettings.maxOverlayRadius = g_vfxWeapon.GetBulletImpactDecalMaxOverlayRadiusOverride();
				}
#endif

				CEntity* pEntity = vfxCollInfo.regdEnt;

				// override some settings for a smash group
				if (pSmashGroup)
				{
					pEntity = pSmashEntity;
					decalSettings.bucketSettings.pSmashGroup = pSmashGroup;
					decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_WEAPON_IMPACT_SMASHGROUP;
					decalSettings.instSettings.vDimensions = Vec3V(width, height, decalSettingsManager::GetTypeSettings(DECALTYPE_WEAPON_IMPACT_SMASHGROUP)->depth);
					decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_FORWARD;
					decalSettings.pipelineSettings.colnComponentId = (s16)smashMatrixId;
					decalSettings.pipelineSettings.useColnEntityOnly = true;

					if (pEntity->GetIsTypeVehicle())
					{
						decalSettings.pipelineSettings.isVehicleGlass = true;
					}

					// make sure that any smash groups get a collision going into the vehicle
					// as the smash group is single sided 
					if (pSmashEntity && pSmashEntity->GetIsTypeVehicle())
					{
						Vec3V vCentreOfVehicle = pSmashEntity->GetMatrix().GetCol3();
						if (lookupCentreOfVehicleFromBone)
						{
							// we could look into always taking this branch for vehicles since it's likely to be
							// more accurate to consider the "inside" of the vehicle the place where the steering wheel
							// is that just the local origin of the vehicle model itself
							const CVehicle* pVehicle = static_cast<const CVehicle*>(pSmashEntity);
							s32 boneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_STEERING_WHEEL));
							if (boneIndex <= -1)
							{
								boneIndex = pVehicle->GetBoneIndex((eHierarchyId)(VEH_CAR_STEERING_WHEEL));
							}
							if (boneIndex > -1)
							{
								Mat34V vBoneMtxWld;
								CVfxHelper::GetMatrixFromBoneIndex(vBoneMtxWld, pVehicle, boneIndex);
								vCentreOfVehicle = vBoneMtxWld.GetCol3();
							}
						}

						// get vector from smash position to centre of vehicle
						Vec3V vSmashToCentre = vCentreOfVehicle - vfxCollInfo.vPositionWld;
						vSmashToCentre = Normalize(vSmashToCentre);

						if (Dot(vSmashToCentre, vfxCollInfo.vNormalWld).Getf()>0.0f)
						{		
							decalSettings.instSettings.vDirection = vfxCollInfo.vNormalWld;
						}
					}
				}
				// override some settings for glass collisions
				else if (PGTAMATERIALMGR->GetIsGlass(vfxCollInfo.materialId))
				{
					decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_FORWARD;
					decalSettings.pipelineSettings.useColnEntityOnly = true;
					decalSettings.pipelineSettings.onlyApplyToGlass = true;
					decalSettings.pipelineSettings.dontApplyToGlass = false;
				}
				// override some settings for emissive collisions
				else if (PGTAMATERIALMGR->GetIsEmissive(vfxCollInfo.materialId))
				{
					decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_FORWARD;
				}
				// override some settings for car soft top windows
				else if (PGTAMATERIALMGR->UnpackMtlId(vfxCollInfo.materialId)==PGTAMATERIALMGR->g_idCarSoftTopClear)
				{
					decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_FORWARD;
					decalSettings.pipelineSettings.useColnEntityOnly = true;
					decalSettings.pipelineSettings.onlyApplyToGlass = true;
					decalSettings.pipelineSettings.dontApplyToGlass = false;
				}
				// override some settings for other clear materials
				else if (PGTAMATERIALMGR->UnpackMtlId(vfxCollInfo.materialId)==PGTAMATERIALMGR->g_idPlasticClear || 
							PGTAMATERIALMGR->UnpackMtlId(vfxCollInfo.materialId)==PGTAMATERIALMGR->g_idPlasticHollowClear ||
							PGTAMATERIALMGR->UnpackMtlId(vfxCollInfo.materialId)==PGTAMATERIALMGR->g_idPlasticHighDensityClear)
				{
					decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_FORWARD;
					decalSettings.pipelineSettings.useColnEntityOnly = true;
				}

				// override some settings for a shotgun 
				if (vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_SHOTGUN)
				{
					if (m_disableCompositeShotgunImpactsFromScript==false)
					{
						// change the type
						decalSettings.bucketSettings.typeSettingsIndex++;
						decalSettings.instSettings.vDimensions = Vec3V(width, height, decalSettingsManager::GetTypeSettings(decalSettings.bucketSettings.typeSettingsIndex)->depth);

						// check if it's against a vehicle/object - reject if one has already been applied to the vehicle/object this frame
						if (vfxCollInfo.regdEnt->GetIsTypeVehicle() || vfxCollInfo.regdEnt->GetIsTypeObject())
						{
							if (DECALMGR.IsWaitingToAdd(1<<decalSettings.bucketSettings.typeSettingsIndex, vfxCollInfo.regdEnt))
							{
								return 0;
							}

							if (vfxWeaponInfo.shotgunDecalRenderSettingIndex==-1)
							{
								return 0;
							}

							// set the new render settings index
							s32 shotgunRenderSettingIndex = vfxWeaponInfo.shotgunDecalRenderSettingIndex;
							s32 shotgunRenderSettingCount = vfxWeaponInfo.shotgunDecalRenderSettingCount;
							if (applyBulletproofGlassTextureSwap)
							{
								g_decalMan.FindRenderSettingInfo(14502, shotgunRenderSettingIndex, shotgunRenderSettingCount);
							}

							s32 renderSettingsIndex = GetRenderSettingsIndex(shotgunRenderSettingIndex, shotgunRenderSettingCount);
							if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
							{
								const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
								if (pRenderSettings->pDiffuseMap)
								{
									// override the render settings
									decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
								}
							}

							// scale the dimensions
							decalSettings.instSettings.vDimensions *= Vec3V(vfxWeaponInfo.shotgunTexScale, vfxWeaponInfo.shotgunTexScale, 1.0f);
						}
					}
				}
				// override some settings for explosions
				else if (vfxCollInfo.weaponGroup>=WEAPON_EFFECT_GROUP_ROCKET && vfxCollInfo.weaponGroup<=WEAPON_EFFECT_GROUP_EXPLOSION)
				{
					if (vfxCollInfo.regdEnt->GetIsTypeVehicle())
					{
						const CVehicle* pVehicle = static_cast<const CVehicle*>(vfxCollInfo.regdEnt.Get());
						if (pVehicle && pVehicle->GetStatus() == STATUS_WRECKED)
						{
							return 0;
						}

						decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE;
						decalSettings.instSettings.vDimensions = Vec3V(width, height, decalSettingsManager::GetTypeSettings(DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE)->depth);
					}
					else
					{
						decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND;
						decalSettings.instSettings.vDimensions = Vec3V(width, height, decalSettingsManager::GetTypeSettings(DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND)->depth);
					}
				}
				else if (vfxCollInfo.weaponGroup==WEAPON_EFFECT_GROUP_VEHICLE_MG)
				{
					decalSettings.bucketSettings.typeSettingsIndex = DECALTYPE_WEAPON_IMPACT_VEHICLE;
					decalSettings.instSettings.vDimensions = Vec3V(width, height, decalSettingsManager::GetTypeSettings(DECALTYPE_WEAPON_IMPACT_VEHICLE)->depth);
				}

#if __BANK
				if ((decalSettings.bucketSettings.typeSettingsIndex==DECALTYPE_WEAPON_IMPACT && m_decalDebug.GetDisableDecalType(DECALTYPE_WEAPON_IMPACT)) ||
					(decalSettings.bucketSettings.typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_SHOTGUN && m_decalDebug.GetDisableDecalType(DECALTYPE_WEAPON_IMPACT_SHOTGUN)) ||
					(decalSettings.bucketSettings.typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_SMASHGROUP && m_decalDebug.GetDisableDecalType(DECALTYPE_WEAPON_IMPACT_SMASHGROUP)) ||
					(decalSettings.bucketSettings.typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_SMASHGROUP_SHOTGUN && m_decalDebug.GetDisableDecalType(DECALTYPE_WEAPON_IMPACT_SMASHGROUP_SHOTGUN)) ||
					(decalSettings.bucketSettings.typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND && m_decalDebug.GetDisableDecalType(DECALTYPE_WEAPON_IMPACT_EXPLOSION_GROUND)) ||
					(decalSettings.bucketSettings.typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE && m_decalDebug.GetDisableDecalType(DECALTYPE_WEAPON_IMPACT_EXPLOSION_VEHICLE)) ||
					(decalSettings.bucketSettings.typeSettingsIndex==DECALTYPE_WEAPON_IMPACT_VEHICLE && m_decalDebug.GetDisableDecalType(DECALTYPE_WEAPON_IMPACT_VEHICLE)))
				{
					return 0;
				}
#endif

				return AddSettings(pEntity, decalSettings);
			}
		}
	}

	return 0;
}


///////////////////////////////////////////////////////////////////////////////
//  RegisterMarker
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::RegisterMarker(s32 renderSettingIndex, s32 renderSettingCount, Vec3V_In vPos, Vec3V_In vDir, Vec3V_In vSide, float width, float height, Color32 col, bool isLongRange, bool isDynamic, bool alignToCamRot)
{
	DecalType_e decalType = DECALTYPE_MARKER;
	if (isLongRange)
	{
		decalType = DECALTYPE_MARKER_LONG_RANGE;
	}

#if __BANK
	if ((decalType==DECALTYPE_MARKER && m_decalDebug.GetDisableDecalType(DECALTYPE_MARKER)) ||
		(decalType==DECALTYPE_MARKER_LONG_RANGE && m_decalDebug.GetDisableDecalType(DECALTYPE_MARKER_LONG_RANGE)))
	{
		return 0;
	}
#endif

	s32 renderSettingsIndex = GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
	if (renderSettingsIndex<DECAL_MAX_RENDER_SETTINGS)
	{
		const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(renderSettingsIndex);
		if (pRenderSettings->pDiffuseMap)
		{
			// set up the decal settings
			decalUserSettings decalSettings;

			decalSettings.bucketSettings.typeSettingsIndex = (u16)decalType;
			decalSettings.bucketSettings.renderSettingsIndex = (u16)renderSettingsIndex;
			decalSettings.bucketSettings.roomId = INTLOC_INVALID_INDEX;
			decalSettings.bucketSettings.subType = alignToCamRot ? 1 : 0;

			decalSettings.instSettings.vPosition = vPos;			
			decalSettings.instSettings.vDirection = vDir;			
			decalSettings.instSettings.vSide = vSide;				
			decalSettings.instSettings.vDimensions = Vec3V(width, height, decalSettingsManager::GetTypeSettings(decalType)->depth);				
			decalSettings.instSettings.colR = col.GetRed();
			decalSettings.instSettings.colG = col.GetGreen();
			decalSettings.instSettings.colB = col.GetBlue();
			decalSettings.instSettings.alphaFront = col.GetAlpha();
			decalSettings.instSettings.alphaBack = col.GetAlpha();		
			decalSettings.instSettings.singleFrame = true;		

			decalSettings.pipelineSettings.renderPass = DECAL_RENDER_PASS_MISC;
			decalSettings.pipelineSettings.method = isDynamic ? DECAL_METHOD_DYNAMIC : DECAL_METHOD_STATIC;
#if DECAL_PROCESS_BREAKABLES
			decalSettings.pipelineSettings.applyToBreakables = true;
#endif

			return AddSettings(NULL, decalSettings);
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
//  AddReplaySettings
///////////////////////////////////////////////////////////////////////////////

#if GTA_REPLAY
int	CDecalManager::AddSettingsReplay(CEntity* pEntity, decalUserSettings& settings)
{
	return AddSettings(pEntity, settings);
}
#endif // GTA_REPLAY

///////////////////////////////////////////////////////////////////////////////
//  AddSettings
///////////////////////////////////////////////////////////////////////////////

int CDecalManager::AddSettings(CEntity* pEntity, decalUserSettings& settings)
{
	// verify the dimensions are valid (url:bugstar:1734342)
	decalAssertf(IsFiniteAll(settings.instSettings.vDimensions), "non finite decal dimensions found");

	// don't let any decals be applied to a physics inst that isn't a frag inst (where a frag inst exists)
	if (settings.pipelineSettings.useColnEntityOnly)
	{
		if (pEntity==NULL)
		{
			return 0;
		}

		phInst* pInst = static_cast<CDecalCallbacks*>(g_pDecalCallbacks)->GetPhysInstFromEntity(pEntity);
		if (pEntity->m_nFlags.bIsFrag && !IsFragInst(pInst))
		{
			return 0;
		}
	}

	// don't let any decals be applied to mutliplayer vehicles that are being 'respotted'
	if (NetworkInterface::IsGameInProgress() && pEntity && pEntity->GetIsTypeVehicle())
	{
		CVehicle* pVehicle = static_cast<CVehicle*>(pEntity);
		if (pVehicle && pVehicle->IsBeingRespotted())
		{
			return 0;
		}
	}

	// check if the decal is underwater
	const decalRenderSettings* pRenderSettings = decalSettingsManager::GetRenderSettings(settings.bucketSettings.renderSettingsIndex);
	if (pRenderSettings && (pRenderSettings->createUnderwater==false || pRenderSettings->fadeUnderwater==true))
	{
		float waterDepth;
		CVfxHelper::GetWaterDepth(settings.instSettings.vPosition, waterDepth);
		if (waterDepth>0.0f)
		{
			if (pRenderSettings->createUnderwater==false)
			{
				return 0;
			}
			else if (pRenderSettings->fadeUnderwater)
			{
				settings.instSettings.totalLife = DECAL_FADE_OUT_TIME;
			}
		}
	}

#if __BANK
	settings.instSettings.vDimensions.SetXf(Max(0.01f, settings.instSettings.vDimensions.GetXf() * m_decalDebug.GetSizeMult()));
	settings.instSettings.vDimensions.SetYf(Max(0.01f, settings.instSettings.vDimensions.GetYf() * m_decalDebug.GetSizeMult()));

	if (m_decalDebug.GetDisableDuplicateRejectDistances())
	{
		settings.pipelineSettings.duplicateRejectDist = 0.0f;
	}

	if (m_decalDebug.GetDisableOverlays())
	{
		settings.pipelineSettings.maxOverlayRadius = 0.0f;
	}
#endif

	// verify the dimensions are valid (url:bugstar:1734342)
	decalAssertf(IsFiniteAll(settings.instSettings.vDimensions), "non finite decal dimensions found");

	bool overridePauseCheck = false;	// Apply decal regardless of the game being paused
	// If the replay is playing back then override the pause check as we need
	// decals to be added when the replay playback state is paused.
	REPLAY_ONLY(overridePauseCheck = CReplayMgr::IsEditModeActive();)
	return DECALMGR.AddSettings(pEntity, settings, overridePauseCheck);
}


///////////////////////////////////////////////////////////////////////////////
//  FindRenderSettingInfo
///////////////////////////////////////////////////////////////////////////////

bool CDecalManager::FindRenderSettingInfo(u32 renderSettingId, s32& renderSettingIndex, s32& renderSettingCount)
{
	if (decalVerifyf(decalSettingsManager::FindRenderSettingInfo(renderSettingId, (u32&)renderSettingIndex, (u32&)renderSettingCount), "cannot find render setting information"))
	{
		return true;
	}

	renderSettingIndex = -1;
	renderSettingCount = 0;

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  GetRenderSettingsIndex
///////////////////////////////////////////////////////////////////////////////

s32 CDecalManager::GetRenderSettingsIndex(u32 renderSettingIndex, u32 renderSettingCount)
{
	s32 index = decalSettingsManager::GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
#if __BANK
	if (m_decalDebug.GetOverrideDiffuseMap())
	{		
		if (decalSettingsManager::FindRenderSettingInfo(m_decalDebug.GetOverrideDiffuseMapId(), renderSettingIndex, renderSettingCount))
		{
			index = decalSettingsManager::GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
		}
	}
	else if (m_decalDebug.GetUseTestDiffuseMap())
	{
		if (decalSettingsManager::FindRenderSettingInfo(DECALID_TEST_WRAP, renderSettingIndex, renderSettingCount))
		{
			index = decalSettingsManager::GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
		}
	}
	else if (m_decalDebug.GetUseTestDiffuseMap2())
	{
		if (decalSettingsManager::FindRenderSettingInfo(DECALID_TEST_ROUND, renderSettingIndex, renderSettingCount))
		{
			index = decalSettingsManager::GetRenderSettingsIndex(renderSettingIndex, renderSettingCount);
		}
	}
#endif

	return index;
}

///////////////////////////////////////////////////////////////////////////////
//  Remove
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::Remove(const fwEntity* pEntity, const int boneIndex, const void* pSmashGroup, u32 exceptionTypeFlags)
{
	// check if we're trying to remove a vehicle badge
	// we need to make sure the ref counting updates when this happens
	for (int i=0; i<DECAL_NUM_VEHICLE_BADGES; i++)
	{
		if (pEntity && pEntity->GetType()==ENTITY_TYPE_VEHICLE && pSmashGroup==NULL)
		{
			if ((exceptionTypeFlags & (1<<(DECALTYPE_VEHICLE_BADGE+i)))==0)
			{
				const CVehicle* pVehicle = static_cast<const CVehicle*>(pEntity);
				if (pVehicle)
				{
					if (DoesVehicleHaveBadge(pVehicle, i))
					{
						RemoveVehicleBadge(pVehicle, i);
					}
				}
			}
		}
	}

	DECALMGR.Remove(pEntity, boneIndex, pSmashGroup, exceptionTypeFlags);

#if GTA_REPLAY
	if(CReplayMgr::ShouldRecord())
	{
		CReplayMgr::RecordFx(CPacketDecalRemove(boneIndex), (const CEntity*)pEntity);
	}	
#endif // GTA_REPLAY
}


///////////////////////////////////////////////////////////////////////////////
//  SetJoinVerts
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::SetJoinVerts(decalPipelineSettings& pipelineSettings, Vec3V_Ptr pvJoinVerts)
{
	if (!IsZeroAll(pvJoinVerts[0]))
	{
		pipelineSettings.vPassedJoinVerts[0] = pvJoinVerts[0];
		pipelineSettings.vPassedJoinVerts[1] = pvJoinVerts[1];
		pipelineSettings.vPassedJoinVerts[2] = pvJoinVerts[2];
		pipelineSettings.vPassedJoinVerts[3] = pvJoinVerts[3];
	}
	else
	{
		pipelineSettings.vPassedJoinVerts[0] = Vec3V(V_ZERO);
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetJoinVerts
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::GetJoinVerts(decalPipelineSettings& pipelineSettings, Vec3V_Ptr pvJoinVerts)
{
	pvJoinVerts[0] = pipelineSettings.vReturnedJoinVerts[0];
	pvJoinVerts[1] = pipelineSettings.vReturnedJoinVerts[1];
	pvJoinVerts[2] = pipelineSettings.vReturnedJoinVerts[2];
	pvJoinVerts[3] = pipelineSettings.vReturnedJoinVerts[3];
}


///////////////////////////////////////////////////////////////////////////////
//  RequestReplayVehicleBadge
///////////////////////////////////////////////////////////////////////////////

#if GTA_REPLAY
bool CDecalManager::RequestReplayVehicleBadge(const CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha) 
{
	if (boneIndex>-1)
	{
		bool retc = m_vehicleBadgeMgr.Request(pVehicle, badgeDesc, boneIndex, vOffsetPos, vDir, vSide, size, isForLocalPlayer, badgeIndex, alpha);

		return retc;
	}

	return false;
}
#endif // GTA_REPLAY


///////////////////////////////////////////////////////////////////////////////
//  ProcessVehicleBadge
///////////////////////////////////////////////////////////////////////////////

bool CDecalManager::ProcessVehicleBadge(const CVehicle* pVehicle, const CVehicleBadgeDesc& badgeDesc, s32 boneIndex, Vec3V_In vOffsetPos, Vec3V_In vDir, Vec3V_In vSide, float size, bool isForLocalPlayer, u32 badgeIndex, u8 alpha) 
{
	decalAssertf(badgeDesc.IsValid(), "invalid badgeDesc passed to ProcessVehicleBadge - an emblem desc or a texture must be specified and not both");

	if (boneIndex>-1)
	{
#if !__FINAL
		if (NetworkInterface::IsGameInProgress() && pVehicle && pVehicle->GetNetworkObject() && pVehicle->IsNetworkClone())
		{
			vehicleDebugf3("CDecalManager::ProcessVehicleBadge RECEIVE model[%s][%s] boneIndex[%d] vOffsetPos[%f %f %f] vDir[%f %f %f] vSide[%f %f %f] size[%f]",pVehicle->GetModelName(),pVehicle->GetNetworkObject()->GetLogName(),boneIndex,vOffsetPos.GetXf(),vOffsetPos.GetYf(),vOffsetPos.GetZf(),vDir.GetXf(),vDir.GetYf(),vDir.GetZf(),vSide.GetXf(),vSide.GetYf(),vSide.GetZf(),size);
		}
#endif

		bool retc = m_vehicleBadgeMgr.Request(pVehicle, badgeDesc, boneIndex, vOffsetPos, vDir, vSide, size, isForLocalPlayer, badgeIndex, alpha);	

		if (NetworkInterface::IsGameInProgress() && retc && pVehicle && pVehicle->GetNetworkObject() && !pVehicle->IsNetworkClone())
		{
			CNetObjVehicle* pNetObjVehicle = (CNetObjVehicle*) pVehicle->GetNetworkObject();
			
			if (pNetObjVehicle)
			{
#if !__FINAL
				vehicleDebugf3("CDecalManager::ProcessVehicleBadge   LOCAL model[%s][%s] boneIndex[%d] vOffsetPos[%f %f %f] vDir[%f %f %f] vSide[%f %f %f] size[%f]",pVehicle->GetModelName(),pVehicle->GetNetworkObject()->GetLogName(),boneIndex,vOffsetPos.GetXf(),vOffsetPos.GetYf(),vOffsetPos.GetZf(),vDir.GetXf(),vDir.GetYf(),vDir.GetZf(),vSide.GetXf(),vSide.GetYf(),vSide.GetZf(),size);
#endif
				pNetObjVehicle->SetVehicleBadge(badgeDesc, boneIndex, vOffsetPos, vDir, vSide, size, badgeIndex, alpha);	
			}
		}
#if !__FINAL
		else if (NetworkInterface::IsGameInProgress() && pVehicle && pVehicle->GetNetworkObject() && !pVehicle->IsNetworkClone())
		{
			vehicleDebugf3("CDecalManager::ProcessVehicleBadge (FAILED m_vehicleBadgeMgr.Request)   LOCAL model[%s][%s] boneIndex[%d] vOffsetPos[%f %f %f] vDir[%f %f %f] vSide[%f %f %f] size[%f]",pVehicle->GetModelName(),pVehicle->GetNetworkObject()->GetLogName(),boneIndex,vOffsetPos.GetXf(),vOffsetPos.GetYf(),vOffsetPos.GetZf(),vDir.GetXf(),vDir.GetYf(),vDir.GetZf(),vSide.GetXf(),vSide.GetYf(),vSide.GetZf(),size);
		}
#endif

		return retc;
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveVehicleBadge
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::RemoveVehicleBadge(const CVehicle* pVehicle, const u32 badgeIndex) 
{
	if (pVehicle)
	{
		m_vehicleBadgeMgr.Remove(pVehicle, badgeIndex);

		if (NetworkInterface::IsGameInProgress() && !pVehicle->IsNetworkClone())
		{
			CNetObjVehicle* pNetObjVehicle = (CNetObjVehicle*) pVehicle->GetNetworkObject();
			if (pNetObjVehicle)
			{
				pNetObjVehicle->ResetVehicleBadge(badgeIndex);
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveAllVehicleBadges
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::RemoveAllVehicleBadges(const CVehicle* pVehicle) 
{
	if (pVehicle)
	{
		for (int i=0; i<DECAL_NUM_VEHICLE_BADGES; i++)
		{
			m_vehicleBadgeMgr.Remove(pVehicle, i);

			if (NetworkInterface::IsGameInProgress() && !pVehicle->IsNetworkClone())
			{
				CNetObjVehicle* pNetObjVehicle = (CNetObjVehicle*) pVehicle->GetNetworkObject();
				if (pNetObjVehicle)
				{
					pNetObjVehicle->ResetVehicleBadge(i);
				}
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////
//  GetVehicleBadgeRequestState
///////////////////////////////////////////////////////////////////////////////

DecalRequestState_e CDecalManager::GetVehicleBadgeRequestState(const CVehicle* pVehicle, const u32 badgeIndex) 
{
	return m_vehicleBadgeMgr.GetRequestState(pVehicle,badgeIndex);
}


///////////////////////////////////////////////////////////////////////////////
//  GetVehicleBadgeRequestState
///////////////////////////////////////////////////////////////////////////////

#if __BANK
void CDecalManager::RemoveCompletedVehicleBadgeRequests() 
{
	return m_vehicleBadgeMgr.RemoveCompletedRequests();
}
#endif


///////////////////////////////////////////////////////////////////////////////
//  DoesVehicleHaveBadge
///////////////////////////////////////////////////////////////////////////////

bool CDecalManager::DoesVehicleHaveBadge(const CVehicle* pVehicle, u32 badgeIndex) 
{
	if (pVehicle)
	{
		return (FindFirstBucket( 1<<(DECALTYPE_VEHICLE_BADGE+badgeIndex), pVehicle )!=NULL);
	}

	return false;
}


///////////////////////////////////////////////////////////////////////////////
//  AbortVehicleBadgeRequests
///////////////////////////////////////////////////////////////////////////////

bool CDecalManager::AbortVehicleBadgeRequests(const CVehicle* pVehicle) 
{
	return m_vehicleBadgeMgr.Abort(pVehicle);
}


///////////////////////////////////////////////////////////////////////////////
//  StartScriptedTrail
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::StartScriptedTrail(DecalType_e decalType, VfxLiquidType_e liquidType, s32 renderSettingsIndex, float width, Color32 col) 
{
	decalAssertf(m_scriptedTrailData.m_isActive==false, "trying to start a new scripted trail before a previous one has ended");
	m_scriptedTrailData.m_isActive = true;

	m_scriptedTrailData.m_decalType = decalType;
	m_scriptedTrailData.m_liquidType = liquidType;
	m_scriptedTrailData.m_renderSettingsIndex = renderSettingsIndex;
	m_scriptedTrailData.m_width = width;
	m_scriptedTrailData.m_colour = col;
	m_scriptedTrailData.m_currLength = 0.0f;
	m_scriptedTrailData.m_prevAlphaMult = 0.0f;

	m_scriptedTrailData.m_prevPos = Vec3V(V_ZERO);

	m_scriptedTrailData.m_joinVerts[0] = Vec3V(V_ZERO);
	m_scriptedTrailData.m_joinVerts[1] = Vec3V(V_ZERO);
	m_scriptedTrailData.m_joinVerts[2] = Vec3V(V_ZERO);
	m_scriptedTrailData.m_joinVerts[3] = Vec3V(V_ZERO);
}


///////////////////////////////////////////////////////////////////////////////
//  AddScriptedTrailInfo
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::AddScriptedTrailInfo(Vec3V_In vPos_In, float alphaMult, bool isScripted) 
{
	decalAssertf(m_scriptedTrailData.m_isActive, "trying to add info to a scripted trail that hasn't been started");

	Vec3V vPos = vPos_In;
	Vec3V vNormal = Vec3V(V_Z_AXIS_WZERO);

	if (IsZeroAll(m_scriptedTrailData.m_prevPos)==false)
	{
		Color32 col = m_scriptedTrailData.m_colour;
		col.SetAlpha(static_cast<u8>(col.GetAlpha()*alphaMult));
		u8 alphaBack = static_cast<u8>(m_scriptedTrailData.m_prevAlphaMult*255);

		g_decalMan.AddTrail(m_scriptedTrailData.m_decalType, m_scriptedTrailData.m_liquidType, m_scriptedTrailData.m_renderSettingsIndex, m_scriptedTrailData.m_prevPos, vPos, vNormal, m_scriptedTrailData.m_width, col, alphaBack, -1.0f, 0, m_scriptedTrailData.m_currLength, &m_scriptedTrailData.m_joinVerts[0], isScripted);
	}

	m_scriptedTrailData.m_prevPos = vPos;
	m_scriptedTrailData.m_prevAlphaMult = alphaMult;
}


///////////////////////////////////////////////////////////////////////////////
//  EndScriptedTrail
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::EndScriptedTrail()
{
	decalAssertf(m_scriptedTrailData.m_isActive, "trying to stop a scripted trail that hasn't been started");
	m_scriptedTrailData.m_isActive = false;

	m_scriptedTrailData.m_width = 0.0f;
	m_scriptedTrailData.m_currLength = 0.0f;
	m_scriptedTrailData.m_prevAlphaMult = 0.0f;

	m_scriptedTrailData.m_prevPos = Vec3V(V_ZERO);

	m_scriptedTrailData.m_joinVerts[0] = Vec3V(V_ZERO);
	m_scriptedTrailData.m_joinVerts[1] = Vec3V(V_ZERO);
	m_scriptedTrailData.m_joinVerts[2] = Vec3V(V_ZERO);
	m_scriptedTrailData.m_joinVerts[3] = Vec3V(V_ZERO);
}


///////////////////////////////////////////////////////////////////////////////
//  SetDisableFootprints
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::SetDisableFootprints(bool val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_disableFootprintsScriptThreadId==THREAD_INVALID || m_disableFootprintsScriptThreadId==scriptThreadId, "trying to disable footprint decals when this is already in use by another script")) 
	{
		m_disableFootprintsFromScript = val; 
		m_disableFootprintsScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetDisableCompositeShotgunImpacts
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::SetDisableCompositeShotgunImpacts(bool val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_disableCompositeShotgunImpactsScriptThreadId==THREAD_INVALID || m_disableCompositeShotgunImpactsScriptThreadId==scriptThreadId, "trying to disable footprint decals when this is already in use by another script")) 
	{
		m_disableCompositeShotgunImpactsFromScript = val; 
		m_disableCompositeShotgunImpactsScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  SetDisableScuffs
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::SetDisableScuffs(bool val, scrThreadId scriptThreadId) 
{
	if (vfxVerifyf(m_disableScuffsScriptThreadId==THREAD_INVALID || m_disableScuffsScriptThreadId==scriptThreadId, "trying to disable scuff decals when this is already in use by another script")) 
	{
		m_disableScuffsFromScript = val; 
		m_disableScuffsScriptThreadId = scriptThreadId;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  ApplyWheelTrailHackForArenaMode
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::ApplyWheelTrailHackForArenaMode(bool enable)
{
	if (enable)
	{
		g_typeSettings[DECALTYPE_TRAIL_SKID].collisionFlags = DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_OBJECT | DECAL_COLN_FLAG_BUILDINGS;
	}
	else
	{
		g_typeSettings[DECALTYPE_TRAIL_SKID].collisionFlags = DECAL_COLN_FLAG_MAP | DECAL_COLN_FLAG_OBJECT;
	}
}


///////////////////////////////////////////////////////////////////////////////
//  RemoveScript
///////////////////////////////////////////////////////////////////////////////

void CDecalManager::RemoveScript(scrThreadId scriptThreadId)
{
	if (scriptThreadId==m_disableFootprintsScriptThreadId)
	{
		m_disableFootprintsFromScript = false;
		m_disableFootprintsScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_disableCompositeShotgunImpactsScriptThreadId)
	{
		m_disableCompositeShotgunImpactsFromScript = false;
		m_disableCompositeShotgunImpactsScriptThreadId = THREAD_INVALID;
	}

	if (scriptThreadId==m_disableScuffsScriptThreadId)
	{
		m_disableScuffsFromScript = false;
		m_disableScuffsScriptThreadId = THREAD_INVALID;
	}
}

grcBlendStateHandle	CDecalManager::OverrideStaticBlendStateFunctor(decalRenderPass pass)
{
#if APPLY_DOF_TO_ALPHA_DECALS
	if (CPtFxManager::UseParticleDOF() && (pass == DECAL_RENDER_PASS_FORWARD))
	{
		return g_decalMan.GetStaticForwardBlendState();

	}
#endif //APPLY_DOF_TO_ALPHA_DECALS

	return grcStateBlock::BS_Invalid;
}

void CDecalManager::DecalMerged(int REPLAY_ONLY(oldDecalId), int REPLAY_ONLY(newDecalId))
{
#if GTA_REPLAY
	//Only do this in game and not on replay playback.
	if( !CReplayMgr::IsReplayInControlOfWorld() )
	{
		if( g_decalMan.m_ReplayMergedDecalEvents.size() >= MAX_REPLAY_MERGED_DECAL_EVENTS )
		{
			AssertMsg(false, "m_ReplayMergedDecalEvents ran out of space");
			return;
		}

		replayMergedDecalEvent &eventData = g_decalMan.m_ReplayMergedDecalEvents.Append();
		eventData.newDecalId = newDecalId;
		eventData.oldDecalId = oldDecalId;
	}
#endif //GTA_REPLAY
}

void CDecalManager::LiquidUVMultChanged(int REPLAY_ONLY(decalId), float REPLAY_ONLY(uvMult), Vec3V_In REPLAY_ONLY(pos))
{
#if GTA_REPLAY
	//Only do this in game and not on replay playback.
	if( !CReplayMgr::IsReplayInControlOfWorld() )
	{
		if( g_decalMan.m_ReplayUVMultChanges.size() >= MAX_REPLAY_UV_MULT_CHANGES )
		{
			Warningf("m_ReplayUVMultChanges ran out of space");
			return;
		}

		replayUVMultChanges &eventData = g_decalMan.m_ReplayUVMultChanges.Append();
		eventData.decalId = decalId;
		eventData.uvMult = uvMult;
		eventData.pos = pos;
	}
#endif //GTA_REPLAY
}

#if GTA_REPLAY
void CDecalManager::ResetReplayDecalEvents()
{
	m_ReplayMergedDecalEvents.Reset();
	m_ReplayUVMultChanges.Reset();
}

int CDecalManager::ProcessMergedDecals(int decalId)
{
	for( u32 i = 0; i < m_ReplayMergedDecalEvents.size(); i++)
	{
		if( m_ReplayMergedDecalEvents[i].oldDecalId == decalId )
			return m_ReplayMergedDecalEvents[i].newDecalId;
	}

	return decalId;
}

float CDecalManager::ProcessUVMultChanges(int decalId, Vec3V vPos)
{
	float uvMult = -1.0f;
	for( u32 i = 0; i < m_ReplayUVMultChanges.size(); i++)
	{
		float dist = VEC3V_TO_VECTOR3(vPos - m_ReplayUVMultChanges[i].pos).Mag2();
		if( m_ReplayUVMultChanges[i].decalId == decalId || dist < 0.5f )
			uvMult = Max(uvMult, m_ReplayUVMultChanges[i].uvMult);
	}

	return uvMult;
}
#endif //GTA_REPLAY