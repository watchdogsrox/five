//
// VehicleModelInfoVariation.h
// Data file for vehicle customization
//
// Rockstar Games (c) 2011
#include "modelinfo/VehicleModelInfoVariation.h"
#include "modelinfo/VehicleModelInfoVariation_parser.h"

#include "bank/bank.h"
#include "bank/bkmgr.h"
#include "bank/combo.h"

#include "fragment/type.h"
#include "fwscene/stores/drawablestore.h"
#include "fwscene/stores/fragmentstore.h"
#include "control/Gen9ExclusiveAssets.h"
#include "modelinfo/VehicleModelInfoColors.h"
#include "modelinfo/VehicleModelInfoPlates.h"
#include "modelinfo/VehicleModelInfo.h"
#include "parser/manager.h"
#include "scene/Entity.h"
#include "scene/EntityIterator.h"
#include "shaders/CustomShaderEffectVehicle.h"
#include "streaming/populationstreaming.h"
#include "streaming/ScriptPreload.h"
#include "streaming/streaming.h"
#include "vehicles/vehicle.h"
#include "vehicles/vehicleDamage.h"
#include "vehicles/Automobile.h"
#include "vehicles/Bike.h"
#include "vehicles/Planes.h"
#include "Vehicles/VehicleFactory.h"
#include "Vehicles/VehicleGadgets.h"


PARAM(disableVehMods, "[vehicles] Disable vehicle mods (temp disabled as patch for BS#1017717, BS#1022990, etc.)");

VEHICLE_OPTIMISATIONS()

const char* exhaustBoneNames[NUM_EXHAUST_BONES] = {"exhaust","exhaust_2","exhaust_3","exhaust_4","exhaust_5","exhaust_6","exhaust_7","exhaust_8","exhaust_9","exhaust_10","exhaust_11","exhaust_12","exhaust_13","exhaust_14","exhaust_15","exhaust_16","exhaust_17","exhaust_18","exhaust_19","exhaust_20","exhaust_21","exhaust_22","exhaust_23","exhaust_24","exhaust_25","exhaust_26","exhaust_27","exhaust_28","exhaust_29","exhaust_30","exhaust_31","exhaust_32" };

FW_INSTANTIATE_CLASS_POOL(CVehicleStreamRequestGfx, CONFIGURED_FROM_FILE, atHashString("CVehicleStreamRequestGfx",0x54753c86));
FW_INSTANTIATE_CLASS_POOL(CVehicleStreamRenderGfx, CONFIGURED_FROM_FILE, atHashString("CVehicleStreamRenderGfx",0xc2b5f089));

AUTOID_IMPL(CVehicleStreamRequestGfx);
AUTOID_IMPL(CVehicleStreamRenderGfx);

atArray<CVehicleStreamRenderGfx*> CVehicleStreamRenderGfx::ms_renderRefs[FENCE_COUNT];
s32 CVehicleStreamRenderGfx::ms_fenceUpdateIdx = 0;


int CVehicleVariationInstance::ms_MaxVehicleStreamRequest = 0;
int CVehicleVariationInstance::ms_MaxVehicleStreamRender = 0;

CVehicleVariationInstance::CVehicleVariationInstance() : 
m_color1(0),
m_color2(0),
m_color3(0),
m_color4(0),
m_colorType1(VCT_NONE),
m_colorType2(VCT_NONE),
m_baseColorIndex(-1),
m_specColorIndex(-1),
m_baseColorIndex2(-1),
m_smokeColR(255),
m_smokeColG(255),
m_smokeColB(255),
m_windowTint(0xff),
m_numMods(0),
m_kitIdx(INVALID_VEHICLE_KIT_INDEX),
m_renderGfx(NULL),
m_modMatricesStoredThisFrame(0),
m_canHaveRearWheels(false),
m_neonLOn(false),
m_neonROn(false),
m_neonFOn(false),
m_neonBOn(false),
m_xenonLightColor(0xff),
m_preloadData(NULL),
m_wheelType(VWT_INVALID),
m_modVariations(0)
{
	for (s32 i = 0; i < VMT_MAX; ++i)
	{
		m_mods[i] = INVALID_MOD;
	}

	for (s32 i = 0; i < MAX_LINKED_MODS; ++i)
	{
		m_linkedMods[i] = INVALID_MOD;
	}

	ClearBonePositions();
	ShowAllMods();
}

CVehicleVariationInstance::~CVehicleVariationInstance()
{
	if (m_renderGfx)
	{
		gPopStreaming.RemoveStreamedVehicleVariation(m_renderGfx);
		m_renderGfx->SetUsed(false);
	}

	CleanUpPreloadMods(NULL);
}

void CVehicleVariationInstance::CleanUpPreloadMods(CVehicle* pVeh)
{
	// try to apply the requested mods on the vehicle, usually these are the preloaded mods and there's a slight
	// chance they'll get freed here a frame before they get used again
	if (pVeh && m_preloadData)
	{
		CVehicleStreamRequestGfx* req = pVeh->GetExtensionList().GetExtension<CVehicleStreamRequestGfx>();
		if (req && req->HaveAllLoaded())
		{
			Assert(req->GetTargetEntity() == pVeh);
			req->RequestsComplete();
			pVeh->GetExtensionList().Destroy(req);
		}
	}

	delete m_preloadData;
	m_preloadData = NULL;
}

void CVehicleVariationInstance::ClearBonePositions()
{
	for (s32 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		m_bonePositions[i] = VEC3_ZERO;
	}
}

void CVehicleVariationInstance::UpdateBonePositions(CVehicle* pVehicle)
{
	for (u8 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		s32 boneIdx = -1;
		if (i < VMT_RENDERABLE)
			boneIdx = GetBone(i);
		else
			boneIdx = GetBoneForLinkedMod(i - VMT_RENDERABLE);

		if (boneIdx < 0)
			continue;

		s32 modBone = pVehicle->GetBoneIndex((eHierarchyId)boneIdx);

		if ((GetVehicleRenderGfx() == NULL) || (boneIdx < 0) || (modBone < 0))
		{
			continue;
		}

		Vector3 bonePosition = VEC3_ZERO;
		pVehicle->GetDefaultBonePosition(modBone, bonePosition);

		SetBonePosition(i, bonePosition);
	}
}

void CVehicleVariationInstance::ApplyDeformation(CVehicle* pParentVehicle, const void* basePtr)
{
	if ((pParentVehicle == NULL) || (basePtr == NULL))
	{
		return;
	}

	UpdateBonePositions(pParentVehicle);
}

bool CVehicleVariationInstance::HasPreloadModsFinished()
{
    if (!m_preloadData)
        return true;
    return m_preloadData->HasPreloadFinished();
}

void CVehicleVariationInstance::SetVehicleRenderGfx(CVehicleStreamRenderGfx* newRenderGfx) 
{ 
	if (m_renderGfx && newRenderGfx)
	{
		newRenderGfx->CopyMatrices(*m_renderGfx);
	}

	if (m_renderGfx)
	{
		gPopStreaming.RemoveStreamedVehicleVariation(m_renderGfx);
		m_renderGfx->SetUsed(false);
	}

	m_renderGfx = newRenderGfx; 
	if (m_renderGfx)
	{
		gPopStreaming.AddStreamedVehicleVariation(m_renderGfx);
	}
}


void CVehicleVariationInstance::InitSystem()
{
	ms_MaxVehicleStreamRequest = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("VehicleStreamRequest", 0x90d4bff4), CONFIGURED_FROM_FILE);
	ms_MaxVehicleStreamRender = fwConfigManager::GetInstance().GetSizeOfPool(ATSTRINGHASH("VehicleStreamRender", 0xe11d5267), CONFIGURED_FROM_FILE);

	CVehicleStreamRequestGfx::InitPool(ms_MaxVehicleStreamRequest, MEMBUCKET_STREAMING);
	CVehicleStreamRenderGfx::InitPool(ms_MaxVehicleStreamRender, MEMBUCKET_STREAMING);
	CVehicleStreamRenderGfx::Init();

#if __DEV
	CVehicleStreamRenderGfx::GetPool()->RegisterPoolCallback(CVehicleStreamRenderGfx::PoolFullCallback);
#endif // __DEV
}

void CVehicleVariationInstance::ShutdownSystem()
{
	CVehicleStreamRenderGfx::Shutdown();
	CVehicleStreamRequestGfx::ShutdownPool();
	CVehicleStreamRenderGfx::ShutdownPool();
}

void CVehicleVariationInstance::ProcessStreamRequests()
{
	CVehicleStreamRequestGfx::Pool* pStreamReqPool = CVehicleStreamRequestGfx::GetPool();
	if (pStreamReqPool->GetNoOfUsedSpaces() == 0)
		return;												// early out if no requests outstanding

	CVehicleStreamRequestGfx* pStreamReq = NULL;
	CVehicleStreamRequestGfx* pStreamReqNext = NULL;
	s32 poolSize = pStreamReqPool->GetSize();

	pStreamReqNext = pStreamReqPool->GetSlot(0);

	s32 i = 0;
	while(i < poolSize)
	{
		pStreamReq = pStreamReqNext;

		// spin to next valid entry
		while((++i < poolSize) && (pStreamReqPool->GetSlot(i) == NULL));

		if (i != poolSize)
			pStreamReqNext = pStreamReqPool->GetSlot(i);

		// do any required processing on each interior
		if (pStreamReq && pStreamReq->HaveAllLoaded())
		{
			// apply the files we requested and which have now all loaded to the target vehicle
			CVehicle* pVehicle =  pStreamReq->GetTargetEntity();
			pStreamReq->RequestsComplete();
			pVehicle->GetExtensionList().Destroy(pStreamReq);		// will remove from pool & from extension list
		}
	}
}

void CVehicleVariationInstance::RequestVehicleModFiles(CVehicle* pVehicle, s32 streamFlags)
{
	pVehicle->GetExtensionList().Destroy(CVehicleStreamRequestGfx::GetAutoId()); // strip all mod stream requests for this entity

	CVehicleStreamRequestGfx* pNewStreamGfx = rage_new CVehicleStreamRequestGfx;
	Assert(pNewStreamGfx);
	pNewStreamGfx->SetTargetEntity(pVehicle);

	s32 parentTxd = CVehicleModelInfo::GetCommonTxd();
	s32 modelParentTxd = pVehicle->GetVehicleModelInfo()->GetAssetParentTxdIndex();
	if (modelParentTxd != -1)
		parentTxd = modelParentTxd;

	const CVehicleVariationInstance& var = pVehicle->GetVariationInstance();
	const CVehicleKit* kit = var.GetKit();
    Assert(kit);
	for (s32 i = 0; i < VMT_RENDERABLE; ++i)
	{
		u32 modEntry = var.GetMods()[i];
		if (modEntry == INVALID_MOD)
			continue;
		
		const CVehicleModVisible& mod = kit->GetVisibleMods()[(eVehicleModType)modEntry];
		pNewStreamGfx->AddFrag((u8)i, mod.GetNameHashString(), streamFlags, parentTxd);
	}

	for (s32 i = 0; i < MAX_LINKED_MODS; ++i)
	{
		u32 modEntry = var.GetLinkedMods()[i];
		if (modEntry == INVALID_MOD)
			continue;

		const CVehicleModLink& mod = kit->GetLinkMods()[modEntry];
		pNewStreamGfx->AddFrag((u8)(VMT_RENDERABLE + i), mod.GetNameHashString(), streamFlags, parentTxd);
	}

	eVehicleWheelType wheelType = var.GetWheelType(pVehicle);

    u32 modEntry = var.GetMods()[VMT_WHEELS];
    if (modEntry != INVALID_MOD && modEntry < CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType].GetCount())
    {
		bool variation = var.HasVariation(0);
        const CVehicleWheel& wheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][modEntry];
        pNewStreamGfx->AddDrawable(VMT_WHEELS, variation ? wheel.GetVariationHashString() : wheel.GetNameHashString(), streamFlags, CVehicleModelInfo::GetCommonTxd());
    }

	const u32 modelNameHash = pVehicle->GetModelNameHash();

	if ((pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE) || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))
	{
		u32 modEntry = var.GetMods()[VMT_WHEELS_REAR_OR_HYDRAULICS];
		if (modEntry != INVALID_MOD && modEntry < CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType].GetCount())
		{
			bool variation = var.HasVariation(1);
			const CVehicleWheel& wheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][modEntry];
			pNewStreamGfx->AddDrawable(VMT_WHEELS_REAR_OR_HYDRAULICS, variation ? wheel.GetVariationHashString() : wheel.GetNameHashString(), streamFlags, CVehicleModelInfo::GetCommonTxd());
		}
	}

	pNewStreamGfx->PushRequest(); // activate this set of requests
}

const CVehicleKit* CVehicleVariationInstance::GetKit() const
{
#if !__FINAL
	XPARAM(disableVehMods);
	if(PARAM_disableVehMods.Get())
	{
		return NULL;
	}
#endif

	if (m_kitIdx == INVALID_VEHICLE_KIT_ID)
		return NULL;

	return &CVehicleModelInfo::GetVehicleColours()->m_Kits[m_kitIdx];
}

const CVehicle* pDebugVehicle = NULL;
const CVehicleKit* pDebugKit = NULL;
u32 CVehicleVariationInstance::GetNumModsForType(eVehicleModType modSlot, const CVehicle* pVehicle) const
{
	pDebugVehicle = pVehicle;

	const u32 modelNameHash = pVehicle->GetModelNameHash();

	if (modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
    {
		eVehicleWheelType wheelType = GetWheelType(pVehicle);
		s32 numWheels = 0;
		const s32 totalNumWheels = CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType].GetCount();
		for (s32 i = 0; i < totalNumWheels; ++i)
		{
			if(	CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][i].IsRearWheel()	||
				(modSlot==VMT_WHEELS_REAR_OR_HYDRAULICS && ((modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))	// tornado6: all front wheels can be used as rear wheel
				numWheels = (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS) ? numWheels + 1 : numWheels;
			else
				numWheels = (modSlot == VMT_WHEELS) ? numWheels + 1 : numWheels;
		}

		return numWheels;
    }

    const CVehicleKit* kit = GetKit();
	pDebugKit = kit;
    if (!kit)
        return 0;

    if (modSlot < VMT_RENDERABLE)
    {
        u32 count = 0;
        for (s32 i = 0; i < kit->GetVisibleMods().GetCount(); ++i)
        {
            if (kit->GetVisibleMods()[i].GetType() == modSlot)        
                count++;
        }
        return count;
    }

    if (modSlot < VMT_TOGGLE_MODS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS)
    {
        u32 count = 0;
        for (s32 i = 0; i < kit->GetStatMods().GetCount(); ++i)
        {
            if (kit->GetStatMods()[i].GetType() == modSlot)        
                count++;
        }
        return count;
    }

    // the rest are toggle mods
    return 1;
}

bool CVehicleVariationInstance::IsGen9ExclusiveVehicleMod(eVehicleModType modSlot, u8 modIndex, const CVehicle* pVehicle) const
{
	const CVehicleKit *pVehicleModKit = GetKit();
	if (!Verifyf(pVehicleModKit, "CVehicleVariationInstance::IsGen9ExclusiveVehicleMod - this vehicle has no mod kit: %s", pVehicle->GetModelName()))
		return false;

	u8 globalModIndex = GetGlobalModIndex(modSlot, modIndex, pVehicle);
	return CGen9ExclusiveAssets::IsGen9ExclusiveVehicleMod(pVehicleModKit, modSlot, globalModIndex);
}

void CVehicleVariationInstance::SetModIndexForType(eVehicleModType modSlot, u8 modIndex, CVehicle* pVehicle, bool variation, u32 streamingFlags)
{
	if (!Verifyf(GetKit(), "Trying to set a mod on vehicle with no mod kit: %s", pVehicle->GetModelName()))
		return;

    u8 globalModIndex = GetGlobalModIndex(modSlot, modIndex, pVehicle);
    SetModIndex(modSlot, globalModIndex, pVehicle, variation, streamingFlags);
}

u8 CVehicleVariationInstance::GetGlobalModIndex(eVehicleModType modSlot, u8 modIndex, const CVehicle* pVehicle) const
{
	if (!Verifyf(GetKit(), "Trying to set a mod on vehicle with no mod kit: %s", pVehicle->GetModelName()))
		return INVALID_MOD;

    u8 globalModIndex = INVALID_MOD;
    
	if (modIndex != INVALID_MOD)
	{
		const u32 modelNameHash = pVehicle->GetModelNameHash();
        if (modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
        {
            u8 count = (u8)-1;

			eVehicleWheelType wheelType = GetWheelType(pVehicle);
            const s32 totalNumWheels = CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType].GetCount();
            for (s32 i = 0; i < totalNumWheels; ++i)
            {
                if (CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][i].IsRearWheel()	||
					(modSlot==VMT_WHEELS_REAR_OR_HYDRAULICS && ((modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
                    count = (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS) ? count + 1 : count;
                else
                    count = (modSlot == VMT_WHEELS) ? count + 1 : count;

                if (count == modIndex)
                {
                    globalModIndex = (u8)i;
                    break;
                }
            }
        }
        else
        {
            const CVehicleKit* kit = GetKit();
            if (!kit)
                return INVALID_MOD;

            // modIndex is per type, so count the mods of this type and find out the index into the mod array of the kit
            if (modSlot < VMT_RENDERABLE)
            {
                u8 count = (u8)-1;
                for (s32 i = 0; i < kit->GetVisibleMods().GetCount(); ++i)
                {
                    if (kit->GetVisibleMods()[i].GetType() == modSlot)        
                        count++;

                    if (count == modIndex)
                    {
                        globalModIndex = (u8)i;
                        break;
                    }
                }
            }
            else if (modSlot < VMT_TOGGLE_MODS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS)
            {
                u8 count = (u8)-1;
                for (s32 i = 0; i < kit->GetStatMods().GetCount(); ++i)
                {
                    if (kit->GetStatMods()[i].GetType() == modSlot)        
                        count++;

                    if (count == modIndex)
                    {
                        globalModIndex = (u8)i;
                        break;
                    }
                }
            }
            else
            {
//                Assertf(false, "Use ToggleMod() for other types of mods");
                return modIndex;
            }
        }
	}

	return globalModIndex;
}

void CVehicleVariationInstance::SetPreloadModForType(eVehicleModType modSlot, u8 modIndex, CVehicle* pVehicle)
{
	if (!Verifyf(GetKit(), "Trying to preload a mod on vehicle with no mod kit: %s", pVehicle->GetModelName()))
		return;

	u8 globalModIndex = GetGlobalModIndex(modSlot, modIndex, pVehicle);
	if (globalModIndex == INVALID_MOD)
		return;

	const u32 modelNameHash = pVehicle->GetModelNameHash();

	if (modSlot < VMT_RENDERABLE)
		Assert(globalModIndex < CVehicleModelInfo::GetVehicleColours()->m_Kits[m_kitIdx].GetVisibleMods().GetCount());
	else if (modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
		Assert(globalModIndex < CVehicleModelInfo::GetVehicleColours()->m_Wheels[GetWheelType(pVehicle)].GetCount());
	else
	{
		Assertf(false, "Trying to preload non renderable mod! slot(%d), index(%d) on vehicle <%s>", modSlot, modIndex, pVehicle->GetModelName());
		return;
	}

	const CVehicleVariationInstance& var = pVehicle->GetVariationInstance();
	const CVehicleKit* kit = var.GetKit();
	Assert(kit);

	s32 parentTxd = CVehicleModelInfo::GetCommonTxd();
	s32 modelParentTxd = pVehicle->GetVehicleModelInfo()->GetAssetParentTxdIndex();
	if (modelParentTxd != -1)
		parentTxd = modelParentTxd;

	if (!m_preloadData)
		m_preloadData = rage_new CScriptPreloadData(0, 0, 0, VMT_RENDERABLE + MAX_LINKED_MODS, 32, true);

	if (modSlot < VMT_RENDERABLE)
	{
		const CVehicleModVisible& mod = kit->GetVisibleMods()[globalModIndex];
        s32 fragSlot = -1;
        bool slotFound = true;

		strLocalIndex idxFragStoreSlot = g_FragmentStore.FindSlot(mod.GetNameHash());

		if(mod.GetNameHash() != MID_DUMMY_MOD_FRAG_MODEL)
		{
			Assertf(idxFragStoreSlot.IsValid(), "Cannot find frag slot for missing mod '%s' on vehicle '%s' - is it definitely in the .rpf?", mod.GetNameHashString().TryGetCStr(), pVehicle->GetModelName());
		}

		slotFound &= m_preloadData->AddFrag(idxFragStoreSlot, parentTxd, fragSlot);

		// preload any linked mods too
		for (s32 i = 0; i < mod.GetNumLinkedModels(); ++i)
		{
			if (!Verifyf(i < MAX_LINKED_MODS, "Too many linked mods in mod '%s' on vehicle '%s'", mod.GetNameHashString().TryGetCStr(), pVehicle->GetModelName()))
				break;

			u8 linkedModIndex = 0;
			const CVehicleModLink* linkedMod = kit->FindLinkMod(mod.GetLinkedModelHash(i), linkedModIndex);
			if (Verifyf(linkedMod && linkedModIndex != INVALID_MOD, "Couldn't find linked mod '%s' for mod '%s'", mod.GetLinkedModelHashString(i).GetCStr(), mod.GetNameHashString().GetCStr()))
			{
				slotFound &= m_preloadData->AddFrag(g_FragmentStore.FindSlot(mod.GetLinkedModelHash(i)), parentTxd, fragSlot);
			}
		}

        Assertf(slotFound || idxFragStoreSlot.IsInvalid(), "Out of frag slots in vehicle preload data!");
	}
	else if (modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
	{
        s32 slot = -1;
        bool slotFound = true;

		const CVehicleWheel& wheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[GetWheelType(pVehicle)][globalModIndex];
        slotFound &= m_preloadData->AddDrawable(g_DrawableStore.FindSlot(wheel.GetNameHash()), CVehicleModelInfo::GetCommonTxd(), slot);
		if (wheel.HasVariation())
			slotFound &= m_preloadData->AddDrawable(g_DrawableStore.FindSlot(wheel.GetVariationHash()), CVehicleModelInfo::GetCommonTxd(), slot);

        Assertf(slotFound, "Out of drawable slots in vehicle preload data!");
	}
}

u8 CVehicleVariationInstance::GetModIndexForType(eVehicleModType modSlot, const CVehicle* pVehicle, bool& variation) const
{
	variation = false;

    u8 modIndex = INVALID_MOD;
	if (m_mods[modSlot] == INVALID_MOD)
		return INVALID_MOD;

	const u32 modelNameHash = pVehicle->GetModelNameHash();

    if (modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
    {
		u8 count = (u8)-1;
		for (s32 i = 0; i <= m_mods[modSlot]; ++i)
		{
			int numWheelMods = CVehicleModelInfo::GetVehicleColours()->m_Wheels[ GetWheelType(pVehicle) ].GetCount();

			if( numWheelMods > i )
			{
				if(CVehicleModelInfo::GetVehicleColours()->m_Wheels[ GetWheelType(pVehicle)][i].IsRearWheel())
				{
					count = (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS) ? count + 1 : count;
				}
				else
				{
					count = (modSlot == VMT_WHEELS) ? count + 1 : count;
				}
			}
		}

        modIndex = count;
		variation = HasVariation(modSlot - VMT_WHEELS);
    }
    else if (m_mods[modSlot] != INVALID_MOD)
    {
        const CVehicleKit* kit = GetKit();
        if (!kit)
            return INVALID_MOD;

        if (modSlot < VMT_RENDERABLE)
        {
            u8 count = (u8)-1;
            for (s32 i = 0; i <= m_mods[modSlot]; ++i)
            {
                if (kit->GetVisibleMods()[i].GetType() == modSlot)        
                    count++;
            }
            modIndex = count;
        }
        else if (modSlot < VMT_TOGGLE_MODS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS)
        {
            u8 count = (u8)-1;
            for (s32 i = 0; i <= m_mods[modSlot]; ++i)
            {
                if (kit->GetStatMods()[i].GetType() == modSlot)        
                    count++;
            }
            modIndex = count;
        }
        else
        {
#if __BANK 
			// these are toggle mods so there are 2 options on and off
			for( s32 i = 0; i <= m_mods[ modSlot ]; ++i )
			{
				if( kit->GetStatMods()[ i ].GetType() == modSlot )
				{
					modIndex = 2;
				}
			}
#else
            Assertf(false, "Use ToggleMod() for other types of mods");
            return INVALID_MOD;
#endif
        }
    }

    return modIndex;
}

void CVehicleVariationInstance::SetModIndex(eVehicleModType modSlot, u8 modIndex, CVehicle* pVehicle, bool variation, u32 streamingFlags)
{
	const CVehicleKit* kit = GetKit();
	if (!Verifyf(kit, "Trying to set a mod on vehicle with no mod kit: %s", pVehicle->GetModelName()))
		return;

	if (!CGen9ExclusiveAssets::CanCreateVehicleMod(kit, modSlot, modIndex, true))
	{
		return;
	}

	const u32 modelNameHash = pVehicle->GetModelNameHash();

#if __ASSERT
	if (modSlot < VMT_RENDERABLE)
		Assertf(modIndex == INVALID_MOD || modIndex < kit->GetVisibleMods().GetCount(), "modSlot=%d modIndex=%u GetVisibleMods().GetCount()=%d", modSlot, modIndex, kit->GetVisibleMods().GetCount());
	else if (modSlot < VMT_TOGGLE_MODS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && !(pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
		Assertf(modIndex == INVALID_MOD || modIndex < kit->GetStatMods().GetCount(), "modSlot=%d modIndex=%u GetStatMods().GetCount()=%d", modSlot, modIndex, kit->GetStatMods().GetCount());
    else if (modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
		Assertf(modIndex == INVALID_MOD || modIndex < CVehicleModelInfo::GetVehicleColours()->m_Wheels[GetWheelType(pVehicle)].GetCount(), "modSlot=%d modIndex=%u wheels.GetCount()=%d", modSlot, modIndex, CVehicleModelInfo::GetVehicleColours()->m_Wheels[GetWheelType(pVehicle)].GetCount());
	else
	{
		ToggleMod( modSlot, modIndex != INVALID_MOD );

		//Assertf(false, "Use ToggleMod() for other types of mods");
		return;
	}
#endif

	if (m_mods[modSlot] == INVALID_MOD && modIndex != INVALID_MOD)
	{
		ENABLE_FRAG_OPTIMIZATION_ONLY(pVehicle->GiveFragCacheEntry();)
		m_numMods++;
	}

	if (m_mods[modSlot] != INVALID_MOD && modIndex == INVALID_MOD)
		m_numMods--;

    // need to get bone index of old mod
    s32 boneIndexOld = GetModBoneIndex((u8)modSlot);
    u8 oldMod = m_mods[modSlot];
	m_mods[modSlot] = modIndex;
    s32 boneIndexNew = GetModBoneIndex((u8)modSlot);

	// turn extras on for old mod and off for new mod
	if (modSlot < VMT_RENDERABLE)
	{
		if (boneIndexOld >= VEH_EXTRA_1 && boneIndexOld <= VEH_LAST_EXTRA)
		{
			if (oldMod != INVALID_MOD && kit->GetVisibleMods()[oldMod].GetTurnOffExtra())
			{
                // if we just removed a mod that had turned off an extra, restore it now
				pVehicle->TurnOffExtra((eHierarchyId)boneIndexOld, false);
			}
		}

		if (boneIndexNew >= VEH_EXTRA_1 && boneIndexNew <= VEH_LAST_EXTRA)
		{
			if (modIndex != INVALID_MOD && kit->GetVisibleMods()[modIndex].GetTurnOffExtra())
			{
                // if we add a mod that turns off an extra, do it now
				pVehicle->TurnOffExtra((eHierarchyId)boneIndexNew, true);
			}
		}


		if (oldMod != INVALID_MOD)
		{
			int iCount = kit->GetVisibleMods()[oldMod].GetNumBonesToTurnOff();
			for (s32 i = 0; i < iCount; ++i)
			{
				s32 bone = kit->GetVisibleMods()[oldMod].GetBoneToTurnOff(i);
				if (bone == -1)
					continue;

				if (bone >= VEH_EXTRA_1 && bone <= VEH_LAST_EXTRA)
				{
					// if we just removed a mod that had turned off an extra, restore it now
					pVehicle->TurnOffExtra((eHierarchyId)bone, false);
				}
			}
		}


		if (modIndex != INVALID_MOD)
		{
			int iCount = kit->GetVisibleMods()[modIndex].GetNumBonesToTurnOff();
			for (s32 i = 0; i < iCount; ++i)
			{
				s32 bone = kit->GetVisibleMods()[modIndex].GetBoneToTurnOff(i);
				if (bone == -1)
					continue;

				if (bone >= VEH_EXTRA_1 && bone <= VEH_LAST_EXTRA)
				{
					// if we add a mod that turns off an extra, do it now
					pVehicle->TurnOffExtra((eHierarchyId)bone, true);
				}
			}
		}


        // turn off any collision bones the old mod might have turned on
        if (oldMod != INVALID_MOD)
        {
			bool hasExtendedModCollision = pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_EXTENDED_COLLISION_MODS );
            CVehicleModVisible::eVehicleModBone collisionBone = kit->GetVisibleMods()[oldMod].GetCollisionBone();

            if( ( hasExtendedModCollision && collisionBone != -1 ) ||
	            ( collisionBone >= CVehicleModVisible::mod_col_1 && collisionBone <= CVehicleModVisible::mod_col_16 ) )
            {
                SetModCollision(pVehicle, collisionBone, false);
            }
        }
        // turn on collision the new mod might need
        if (modIndex != INVALID_MOD)
        {
			bool hasExtendedModCollision = pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_EXTENDED_COLLISION_MODS );
            CVehicleModVisible::eVehicleModBone collisionBone = kit->GetVisibleMods()[modIndex].GetCollisionBone();

            if( ( hasExtendedModCollision && collisionBone != -1 ) ||
	            ( collisionBone >= CVehicleModVisible::mod_col_1 && collisionBone <= CVehicleModVisible::mod_col_16 ) )

            {
                SetModCollision(pVehicle, collisionBone, true);
            }
        }
    }

    if(modSlot == VMT_ENGINE)
    {
		float fDriveForce = pVehicle->pHandling->m_fInitialDriveForce;
		//HACK
		if(pVehicle->GetModelIndex() == MI_BIKE_SANCHEZ || pVehicle->GetModelIndex() == MI_BIKE_SANCHEZ2 )
		{
			if(NetworkInterface::IsGameInProgress())
			{
				float adjustmentMultiplier = 1.0f + (SANCHEZ_MP_DRIVE_FORCE_ADJUSTMENT/100.0f);
				fDriveForce = pVehicle->pHandling->m_fInitialDriveForce * adjustmentMultiplier;
			}
		}

#if __BANK
		//static u32 s_maxEngineModifier = 100;
		//if( pVehicle->GetVariationInstance().GetEngineModifier() > s_maxEngineModifier )
		//{
		//	atHashString dlcPackName = pVehicle->GetVehicleModelInfo()->GetVehicleDLCPack();
		//	atHashString s_currentDLCPackName( "dlc_mpChristmas2018CRC", 0x99443AE5 );

		//	if( dlcPackName == s_currentDLCPackName )
		//	{
		//		Assertf( false, "CVehicleVariationInstance::SetModIndex Engine mod for vehicle '%s' is too high %d", pVehicle->GetDebugName(), pVehicle->GetVariationInstance().GetEngineModifier() );
		//	}
		//}
#endif // #if __BANK

        pVehicle->m_Transmission.SetDriveForce( fDriveForce * (1.0f + (CVehicle::ms_fEngineVarianceMaxModifier * (pVehicle->GetVariationInstance().GetEngineModifier()/100.0f))) );

		if( MI_CAR_BANSHEE2.IsValid() && pVehicle->GetModelIndex() == MI_CAR_BANSHEE2.GetModelIndex() ) 
		{
			bool dummy = false;
			if( pVehicle->GetVariationInstance().GetModIndexForType(VMT_ENGINE, pVehicle, dummy) != INVALID_MOD )
			{	
				float percentageIncrease = 4.0f + ( 4.0f * ( pVehicle->GetVariationInstance().GetModIndexForType(VMT_ENGINE, pVehicle, dummy) + 1 ) );
				CVehicleFactory::ModifyVehicleTopSpeed( pVehicle, percentageIncrease );
			}
		}

		if( CFlyingHandlingData* pFlyingHandling = pVehicle->pHandling->GetFlyingHandlingData() )
		{
			if( pFlyingHandling->m_fInitialThrustFallOff == 0.0f )
			{
				pFlyingHandling->m_fInitialThrustFallOff = pFlyingHandling->m_fThrustFallOff;
			}
			if( pFlyingHandling->m_fInitialThrust == 0.0f )
			{
				pFlyingHandling->m_fInitialThrust = pFlyingHandling->m_fThrust;
			}
			float increase = pVehicle->GetModelIndex() == MI_PLANE_MICROLIGHT && pVehicle->GetVariationInstance().GetModIndex( VMT_EXHAUST ) != INVALID_MOD ? 1.0f : 0.0f;
			increase += ( pVehicle->GetVariationInstance().GetEngineModifier() / 100.0f );
			pFlyingHandling->m_fThrust = pFlyingHandling->m_fInitialThrust * (1.0f + ( CVehicle::ms_fEngineVarianceMaxModifier * increase ) );
			pFlyingHandling->m_fThrustFallOff = pFlyingHandling->m_fInitialThrustFallOff * (1.0f - ( CVehicle::ms_fThrustDropOffVarianceMaxModifier * increase ) );
		}
    }

	static bool sbEnableMicrolightVisiblePropellerMods = false;

	if( sbEnableMicrolightVisiblePropellerMods &&
		modSlot == VMT_EXHAUST &&
		pVehicle->GetModelIndex() == MI_PLANE_MICROLIGHT )
	{
		if( CFlyingHandlingData* pFlyingHandling = pVehicle->pHandling->GetFlyingHandlingData() )
		{
			if( pFlyingHandling->m_fInitialThrustFallOff == 0.0f )
			{
				pFlyingHandling->m_fInitialThrustFallOff = pFlyingHandling->m_fThrustFallOff;
			}
			if( pFlyingHandling->m_fInitialThrust == 0.0f )
			{
				pFlyingHandling->m_fInitialThrust = pFlyingHandling->m_fThrust;
			}
			float increase = pVehicle->GetVariationInstance().GetModIndex( VMT_EXHAUST ) != INVALID_MOD ? 1.0f : 0.0f;
			increase += ( pVehicle->GetVariationInstance().GetEngineModifier() / 100.0f );
			pFlyingHandling->m_fThrust = pFlyingHandling->m_fInitialThrust * ( 1.0f + ( CVehicle::ms_fEngineVarianceMaxModifier * increase ) );
			pFlyingHandling->m_fThrustFallOff = pFlyingHandling->m_fInitialThrustFallOff * (1.0f - ( CVehicle::ms_fThrustDropOffVarianceMaxModifier * increase ) );

			if( sbEnableMicrolightVisiblePropellerMods )
			{
				u8 modIndex = pVehicle->GetVariationInstance().GetModIndex( VMT_EXHAUST );

				for( int i = 0; i < PLANE_NUM_PROPELLERS; i++ )
				{
					bool enablePropeller = false;

					if( ( modIndex == INVALID_MOD && i == 0 ) ||
						( modIndex == i ) )
					{
						enablePropeller = true;
					}
					static_cast< CPlane* >( pVehicle )->EnableIndividualPropeller( i, enablePropeller );
				}

				static_cast< CPlane* >( pVehicle )->UpdatePropellerCollision();
			}
		}
	}

	if(modSlot == VMT_ICE)
	{
		//Gen9 HSW Top Speed upgrades when ICE mod is equipped
		if(pVehicle->HasExpandedMods() && pVehicle->pHandling && pVehicle->pHandling->GetCarHandlingData())
		{
			// NOTE: The modIndex passed into this function is the globalModIndex, but we want the modIndex for type here.
			bool variation = false;
			int appliedModIndex = (int)pVehicle->GetVariationInstance().GetModIndexForType(VMT_ICE, pVehicle, variation);

			for (int i = 0; i < pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData.GetCount(); i++)
			{
				int advancedDataModSlot = pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Slot;

				// NOTE: ICE is the mod slot for the HSW engine upgrades
				if (advancedDataModSlot == VMT_ICE)
				{
					if (appliedModIndex == pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Index)
					{
						float percentageIncrease = pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[i].m_Value;
						CVehicleFactory::ModifyVehicleTopSpeed(pVehicle, percentageIncrease);
					}
				}
			}
		}
	}

	if(modSlot == VMT_BRAKES)
	{
		if( CFlyingHandlingData* pFlyingHandling = pVehicle->pHandling->GetFlyingHandlingData() )
		{
			float increase = ( pVehicle->GetVariationInstance().GetBrakesModifier() / 100.0f );
			if( pFlyingHandling->m_fInitialYawMult == 0.0f )
			{
				pFlyingHandling->m_fInitialYawMult = pFlyingHandling->m_fYawMult;
			}
			if( pFlyingHandling->m_fInitialRollMult == 0.0f )
			{
				pFlyingHandling->m_fInitialRollMult = pFlyingHandling->m_fRollMult;
			}
			if( pFlyingHandling->m_fInitialPitchMult == 0.0f )
			{
				pFlyingHandling->m_fInitialPitchMult = pFlyingHandling->m_fPitchMult;
			}
			pFlyingHandling->m_fYawMult = pFlyingHandling->m_fInitialYawMult * (1.0f + ( CVehicle::ms_fPlaneHandlingVarianceMaxModifier * increase ) );
			pFlyingHandling->m_fRollMult = pFlyingHandling->m_fInitialRollMult * (1.0f + ( CVehicle::ms_fPlaneHandlingVarianceMaxModifier * increase ) );
			pFlyingHandling->m_fPitchMult = pFlyingHandling->m_fInitialPitchMult * (1.0f + ( CVehicle::ms_fPlaneHandlingVarianceMaxModifier * increase ) );
		}
	}

    if(modSlot == VMT_GEARBOX)
	{
		// NOTE: dont mod gerbox on CVT vehicles
		bool bDontModGearbox = (MI_CAR_CYCLONE2.IsValid() && (pVehicle->GetModelIndex() == MI_CAR_CYCLONE2.GetModelIndex())) ? true : false;
        if (m_mods[modSlot] != INVALID_MOD && !bDontModGearbox )
        {
            pVehicle->m_Transmission.SetupTransmission(pVehicle->m_Transmission.GetDriveForce(), pVehicle->m_Transmission.GetDriveMaxFlatVelocity(), pVehicle->m_Transmission.GetDriveMaxVelocity(), pVehicle->pHandling->m_nInitialDriveGears + 1, pVehicle);
        }
        else
        {
            pVehicle->m_Transmission.SetupTransmission(pVehicle->m_Transmission.GetDriveForce(), pVehicle->m_Transmission.GetDriveMaxFlatVelocity(), pVehicle->m_Transmission.GetDriveMaxVelocity(), pVehicle->pHandling->m_nInitialDriveGears, pVehicle);
        }

		if( MI_CAR_BANSHEE2.IsValid() && pVehicle->GetModelIndex() == MI_CAR_BANSHEE2.GetModelIndex() ) 
		{
			bool dummy = false;
			if( pVehicle->GetVariationInstance().GetModIndexForType(VMT_ENGINE, pVehicle, dummy) != INVALID_MOD )
			{
				float percentageIncrease = 4.0f + ( 4.0f * ( pVehicle->GetVariationInstance().GetModIndexForType(VMT_ENGINE, pVehicle, dummy) + 1 ) );
				CVehicleFactory::ModifyVehicleTopSpeed( pVehicle, percentageIncrease );
			}
		}
    }

    if(modSlot == VMT_SUSPENSION)
    {
        if(pVehicle->InheritsFromAutomobile())
        {
            CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
			float suspensionModifier = Clamp( (float)pVehicle->GetVariationInstance().GetSuspensionModifier(), 0.0f, 100.0f );
            pAutomobile->SetFakeSuspensionLoweringAmount((CVehicle::ms_fSuspensionVarianceMaxModifier * (suspensionModifier/100.0f)));
        }
    }

	if(modSlot == VMT_HYDRAULICS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS/*hack, reuse VMT_WHEELS_REAR_OR_HYDRAULICS for VMT_HYDRAULICS*/)
	{
		// tornado6 uses WHEELS_REAR and not HYDRAULICS mod:
		if((pVehicle->GetVehicleType()==VEHICLE_TYPE_CAR) && (modelNameHash!=MID_TORNADO6) && (modelNameHash!=MID_IMPALER2) && (modelNameHash!=MID_IMPALER4) && (modelNameHash!=MID_PEYOTE2))
		{
			CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
			float hydraulicsModifier = (pVehicle->GetVariationInstance().GetHydraulicsModifier()/100.0f);
			if(hydraulicsModifier > 0.0f)
			{
				float upperBound = CVehicle::ms_fHydraulicVarianceUpperBoundMinModifier + ( ( CVehicle::ms_fHydraulicVarianceUpperBoundMaxModifier - CVehicle::ms_fHydraulicVarianceUpperBoundMinModifier ) * hydraulicsModifier );
				float lowerBound = CVehicle::ms_fHydraulicVarianceLowerBoundMinModifier + ( ( CVehicle::ms_fHydraulicVarianceLowerBoundMaxModifier - CVehicle::ms_fHydraulicVarianceLowerBoundMinModifier ) * hydraulicsModifier );
				float rate = CVehicle::ms_fHydraulicRateMin + ( ( CVehicle::ms_fHydraulicRateMax - CVehicle::ms_fHydraulicRateMin ) * hydraulicsModifier );

				pAutomobile->SetHydraulicsBounds( upperBound, -lowerBound );
				pAutomobile->SetHydraulicsRate( rate );
			}
		}
	}

	if(modSlot == VMT_SPOILER)
	{
		if(pVehicle->InheritsFromAutomobile())
		{
			// Don't apply downforce values for VMT_SPOILER modifications linked to a weapon (not actually spoilers)
			bool bSpoilerModLinkedToWeapon = modIndex != INVALID_MOD ? kit->GetVisibleMods()[modIndex].GetWeaponSlot() != -1 : false;
			
			//Make sure we reset any dynamic spoilers to prevent bits disappearing as mods are changed.
			for(int i = 0; i < pVehicle->GetNumberOfVehicleGadgets(); i++)
			{
				if(pVehicle->GetVehicleGadget(i)->GetType() == VGT_DYNAMIC_SPOILER)
				{
					((CVehicleGadgetDynamicSpoiler*)pVehicle->GetVehicleGadget(i))->ResetSpoilerBones();
				}
			}

			if (!bSpoilerModLinkedToWeapon &&
                 !pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_SPOILER_MOD_DOESNT_INCREASE_GRIP ) &&
                 ( !pVehicle->pHandling->GetCarHandlingData() ||
                   !( pVehicle->pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS ) ) )
			{
				if(oldMod != INVALID_MOD && modIndex == INVALID_MOD) // Remove Spoiler
				{
					CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
					for(int wheelIndex = 0; wheelIndex < pAutomobile->GetNumWheels(); ++wheelIndex)
					{
						CWheel* pWheel = pAutomobile->GetWheel(wheelIndex);					
						pWheel->m_fDownforceMult = CWheel::ms_fDownforceMult;
					}
				}
				else // Add spoiler
				{
					CAutomobile *pAutomobile = static_cast<CAutomobile*>(pVehicle);
					for(int wheelIndex = 0; wheelIndex < pAutomobile->GetNumWheels(); ++wheelIndex)
					{
						CWheel* pWheel = pAutomobile->GetWheel(wheelIndex);
						// Tell the wheels we have a spoiler so we can increase down force					
						pWheel->m_fDownforceMult = CWheel::ms_fDownforceMultSpoiler;
					}
				}
			}
            else
            {
                if( pVehicle->pHandling->GetCarHandlingData() &&
                    pVehicle->pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
                {
                    float fDownforceModifier = 1.0f;
                    for( int i = 0; i < pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData.GetCount(); i++ )
                    {
						int advancedDataModSlot = pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Slot;

						if( ( advancedDataModSlot == -1 || advancedDataModSlot == (int)modSlot ) &&
							modIndex == pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Index )
                        {
                            fDownforceModifier = pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Value;
                            break;
                        }
                    }

                    pVehicle->m_fDownforceModifierRear = fDownforceModifier;
                }
            }
		}
	}

    if( modSlot == VMT_BUMPER_F )
    {
        if( pVehicle->pHandling->GetCarHandlingData() &&
            pVehicle->pHandling->GetCarHandlingData()->aFlags & CF_USE_DOWNFORCE_BIAS )
        {
            float fDownforceModifier = 1.0f;
            for( int i = 0; i < pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData.GetCount(); i++ )
            {
				int advancedDataModSlot = pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Slot;

                if( ( advancedDataModSlot == -1 || advancedDataModSlot == (int)modSlot ) &&
					modIndex == pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Index )
                {
                    fDownforceModifier = pVehicle->pHandling->GetCarHandlingData()->m_AdvancedData[ i ].m_Value;
                    break;
                }
            }

            pVehicle->m_fDownforceModifierFront = fDownforceModifier;
        }
    }

	if (modSlot < VMT_RENDERABLE)
	{
		//First get the bone indices from the vehicle model info
		int lightBoneIds[VEHICLE_LIGHT_COUNT];
		int totallighBoneIds = VEHICLE_LIGHT_COUNT;

		for(int i=0; i<VEHICLE_LIGHT_COUNT; i++)
		{
			lightBoneIds[i] = pVehicle->GetBoneIndex((eHierarchyId)(VEH_HEADLIGHT_L+i));
		}

		// remove old linked mods
        if (oldMod != INVALID_MOD)
        {
            const CVehicleModVisible& mod = kit->GetVisibleMods()[oldMod];
            for (s32 i = 0; i < mod.GetNumLinkedModels(); ++i)
            {
                u8 linkedModIndex = 0;
                const CVehicleModLink* linkedMod = kit->FindLinkMod(mod.GetLinkedModelHash(i), linkedModIndex);
                if (Verifyf(linkedMod && linkedModIndex != INVALID_MOD, "Couldn't find linked mod '%s' for mod '%s'", mod.GetNameHashString().GetCStr(), mod.GetLinkedModelHashString(i).GetCStr()))
                {
					for (s32 f = 0; f < MAX_LINKED_MODS; ++f)
					{
						if (m_linkedMods[f] == linkedModIndex)
						{
							// turn extra on if we turned it off when this mod was set
							s32 boneIndex = GetModBoneIndex((u8)(VMT_RENDERABLE + f));
							m_linkedMods[f] = INVALID_MOD;

							if (boneIndex >= VEH_EXTRA_1 && boneIndex <= VEH_LAST_EXTRA && linkedMod->GetTurnOffExtra())
							{
								pVehicle->TurnOffExtra((eHierarchyId)boneIndex, false);
							}
						}
					}
                }
            }
			
			//Reset all the lights broken by the existing mod
			int iCount = mod.GetNumBonesToTurnOff();
			for (s32 f = 0; f < iCount; ++f)
			{
				s32 bone = mod.GetBoneToTurnOff(f);
				if (bone == -1)
					continue;

				int boneIdx = pVehicle->GetBoneIndex((eHierarchyId)bone);
				if (Verifyf(boneIdx != -1, "Invalid bone index in vehicle variation instance on slot %i, bone %d (%s)", modSlot, boneIdx, pVehicle->GetModelName()))
				{					
					for(int lightBoneIdx = 0; lightBoneIdx<totallighBoneIds; lightBoneIdx++)
					{
						if(boneIdx == lightBoneIds[lightBoneIdx])
						{
							//reset the light set by this mod
							pVehicle->GetVehicleDamage()->SetLightState(lightBoneIdx+VEH_HEADLIGHT_L, false);
						}
					}
				}
			}
        }

		// NOTE: B*7594679 hack. dont allow linked mod on granger2 VMT_CHASSIS
		bool bAllowMod = true;
#if RSG_GEN9
		if (pVehicle->GetModelIndex() == MI_CAR_GRANGER2 && modSlot == VMT_CHASSIS)
		{
			bAllowMod = false;
		}		
#endif
		// add new linked mods
		if (modIndex != INVALID_MOD && bAllowMod )
		{
			const CVehicleModVisible& mod = kit->GetVisibleMods()[modIndex];
			for (s32 i = 0; i < mod.GetNumLinkedModels(); ++i)
			{
				u8 linkedModIndex = 0;
				const CVehicleModLink* linkedMod = kit->FindLinkMod(mod.GetLinkedModelHash(i), linkedModIndex);
				if (Verifyf(linkedMod && linkedModIndex != INVALID_MOD, "Couldn't find linked mod '%s' for mod '%s'", mod.GetLinkedModelHashString(i).GetCStr(), mod.GetNameHashString().GetCStr()))
				{
					u8 index;
					if (SetLinkedModIndex(linkedModIndex, index))
					{
						// turn off extra if required
						s32 boneIndex = GetModBoneIndex(VMT_RENDERABLE + index);
						if (boneIndex >= VEH_EXTRA_1 && boneIndex <= VEH_LAST_EXTRA && linkedMod->GetTurnOffExtra())
						{
							pVehicle->TurnOffExtra((eHierarchyId)boneIndex, true);
						}
					}
				}
			}

			//Now break all the light invalidated by the new mod
			int iCount = mod.GetNumBonesToTurnOff();
			for (s32 f = 0; f < iCount; ++f)
			{
				s32 bone = mod.GetBoneToTurnOff(f);
				if (bone == -1)
					continue;

				int boneIdx = pVehicle->GetBoneIndex((eHierarchyId)bone);
				if (Verifyf(boneIdx != -1, "Invalid bone index in vehicle variation instance on slot %i, bone %d (%s)", modSlot, boneIdx, pVehicle->GetModelName()))
				{					
					for(int lightBoneIdx = 0; lightBoneIdx<totallighBoneIds; lightBoneIdx++)
					{
						if(boneIdx == lightBoneIds[lightBoneIdx])
						{
							//break light if mod does not support it
							pVehicle->GetVehicleDamage()->SetLightState(lightBoneIdx+VEH_HEADLIGHT_L, true);
						}
					}
				}
			}
		}
	}

	if (modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
	{
		u32 bitIndex = modSlot - VMT_WHEELS;
        if (!variation || modIndex == INVALID_MOD || (modIndex >= CVehicleModelInfo::GetVehicleColours()->m_Wheels[GetWheelType(pVehicle)].GetCount()))
            DisableVariation(bitIndex);
        else
        {
            bool hasVariation = CVehicleModelInfo::GetVehicleColours()->m_Wheels[GetWheelType(pVehicle)][modIndex].HasVariation();
            if (hasVariation)
                EnableVariation(bitIndex);
            else
                DisableVariation(bitIndex);
        }
	}

	if ((modSlot == VMT_LIVERY_MOD) && (modIndex == INVALID_MOD))
	{
		pVehicle->SetLiveryModFrag(strLocalIndex(strLocalIndex::INVALID_INDEX));
	}

	if( modSlot == VMT_CHASSIS2 )
	{
		if( pVehicle->GetVehicleModelInfo()->GetVehicleFlag( CVehicleModelInfoFlags::FLAG_HAS_RAMMING_SCOOP_MOD ) )
		{
			for( u32 i = VEH_FIRST_SCOOP_MOD; i <= VEH_LAST_SCOOP_MOD; i++ )
			{
				u32 collisionBoneIndex = modIndex != INVALID_MOD ? kit->GetVisibleMods()[ modIndex ].GetCollisionBone() : (u32)-1;
				bool enableCollision = i == collisionBoneIndex;
				pVehicle->GetVariationInstance().SetModCollision( pVehicle, i, enableCollision );
			}
		}
	}

    // request new assets
	if (modSlot < VMT_RENDERABLE || modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))))
		RequestVehicleModFiles(pVehicle, streamingFlags);

    if ((modSlot == VMT_WHEELS || (modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS && (pVehicle->InheritsFromBike() || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2)))) && m_renderGfx)
		m_renderGfx->RestoreWheelRimRadius(pVehicle);

	if ( pVehicle->GetVehicleType() == VEHICLE_TYPE_BIKE				|| 
		(pVehicle->IsReverseTrike() && (modelNameHash!=MID_STRYDER))	|| // B*6144657 - Stryder: Oversized back tire when custom wheels are applied to trike
		(pVehicle->IsTrike())											||
		(modelNameHash==MID_TORNADO6)									||
		(modelNameHash==MID_IMPALER2)									||
		(modelNameHash==MID_IMPALER4) 									||
		(modelNameHash==MID_PEYOTE2)									)
	{
		m_canHaveRearWheels = true;
	}

	if(oldMod != m_mods[modSlot])
	{
		pVehicle->GetVehicleAudioEntity()->OnModChanged(modSlot, modIndex);

		bool bShouldEquipBestWeapon = false;

		// Recreate the vehicle weapon manager when certain types of mods are applied.
		if (CVehicleWeaponMgr::IsValidModTypeForVehicleWeapon(modSlot))
		{
			bool bVehicleWeaponManagerExisted = pVehicle->GetVehicleWeaponMgr() != NULL;
			s32 iNumVehicleWeapons = bVehicleWeaponManagerExisted ? pVehicle->GetVehicleWeaponMgr()->GetNumVehicleWeapons() : 0;

			pVehicle->RecreateVehicleWeaponMgr();

			if (!pVehicle->GetVehicleModelInfo()->GetVehicleFlag(CVehicleModelInfoFlags::FLAG_EQUIP_UNARMED_ON_ENTER))
			{
				// If we new have a vehicle weapon manager, equip one of the new vehicle weapons.
				if (!bVehicleWeaponManagerExisted && pVehicle->GetVehicleWeaponMgr())
				{
					bShouldEquipBestWeapon = true;
				}
				// If we no longer have a vehicle weapon manager, equip best weapon (which clears any selected vehicle weapon index)
				else if (bVehicleWeaponManagerExisted && !pVehicle->GetVehicleWeaponMgr())
				{
					bShouldEquipBestWeapon = true;
				}
				// If we now have less vehicle weapons than we had available before we recreated the manager, equip best weapon (which clears any invalid selected vehicle weapon index)
				else if (pVehicle->GetVehicleWeaponMgr() && iNumVehicleWeapons > pVehicle->GetVehicleWeaponMgr()->GetNumVehicleWeapons())
				{
					bShouldEquipBestWeapon = true;
				}
			}
		}

		pVehicle->InitialiseWeaponBlades();
		pVehicle->InitialiseSpikeCollision();
		pVehicle->InitialiseRammingScoops();
		pVehicle->InitialiseRamps();
		pVehicle->InitialiseRammingBars();

		// If a chassis mod will disable driveby, reset the weapon selection of all players in vehicle (as it may no longer be valid)
		if ((modSlot == VMT_CHASSIS || modSlot == VMT_ROOF || modSlot == VMT_WING_L) && modIndex != INVALID_MOD && pVehicle->ContainsPlayer())
		{
			const CVehicleModVisible& mod = kit->GetVisibleMods()[modIndex];
			if (mod.IsDriveByDisabled() || mod.IsProjectileDriveByDisabled())
			{
				bShouldEquipBestWeapon = true;
			}
		}

		if (bShouldEquipBestWeapon)
		{
			for (s32 i = 0; i < pVehicle->GetSeatManager()->GetMaxSeats(); i++)
			{
				CPed* pPed = pVehicle->GetSeatManager()->GetPedInSeat(i);
				if (pPed && pPed->IsPlayer() && pPed->GetWeaponManager())
				{
					pPed->GetWeaponManager()->EquipBestWeapon();
				}
			}
		}
	}

	UpdateBonePositions(pVehicle);
}

bool CVehicleVariationInstance::SetLinkedModIndex(u8 modIndex, u8& index)
{
	for (s32 i = 0; i < MAX_LINKED_MODS; ++i)
	{
		if (m_linkedMods[i] == INVALID_MOD)
		{
			index = (u8)i;
			m_linkedMods[i] = modIndex;
			return true;
		}
	}

	Assertf(false, "Tried to set more than %d linked mods!", MAX_LINKED_MODS);
	return false;
}

s32 CVehicleVariationInstance::GetModBoneIndex(u8 modSlot) const
{
	const CVehicleKit* kit = GetKit();
	Assert(kit);
	if (!Verifyf(kit, "No kit found on vehicle but trying to render mods!"))
		return -1;

	if (modSlot < VMT_RENDERABLE)
	{
		u32 modEntry = GetMods()[modSlot];
		if (modEntry == INVALID_MOD)
			return -1;

		const CVehicleModVisible& mod = kit->GetVisibleMods()[modEntry];
		return mod.GetBone();
	}
	else
	{
		modSlot -= VMT_RENDERABLE;
		if (modSlot < MAX_LINKED_MODS)
		{
			u32 modEntry = GetLinkedMods()[modSlot];
			if (modEntry == INVALID_MOD)
				return -1;

			const CVehicleModLink& mod = kit->GetLinkMods()[modEntry];
			return mod.GetBone();
		}
	}

	return -1;
}

void CVehicleVariationInstance::ToggleMod(eVehicleModType modSlot, bool toggleOn)
{
	if (Verifyf(modSlot >= VMT_TOGGLE_MODS && modSlot < VMT_MISC_MODS, "Can't toggle mods of type %d", modSlot))
	{
		m_mods[modSlot] = toggleOn ? 1 : INVALID_MOD;
	}
}

bool CVehicleVariationInstance::IsToggleModOn(eVehicleModType modSlot) const
{
	if (Verifyf(modSlot >= VMT_TOGGLE_MODS && modSlot < VMT_MISC_MODS, "Mod type %d is not a toggle mod", modSlot))
	{
		return m_mods[modSlot] != INVALID_MOD;
	}

	return false;
}

void CVehicleVariationInstance::ClearMods()
{
	for (s32 i = 0; i < VMT_MAX; ++i)
	{
		m_mods[i] = INVALID_MOD;
	}
	m_numMods = 0;
}

#if GTA_REPLAY
//When we jump around on the timeline we change the vehicle variation but this doesnt reset
//the linked mods so end up filling the array.
void CVehicleVariationInstance::ClearLinkedModsForReplay()
{
	for (s32 i = 0; i < MAX_LINKED_MODS; ++i)
	{
		m_linkedMods[i] = INVALID_MOD;
	}
}
#endif //GTA_REPLAY


u32 CVehicleVariationInstance::GetTotalWeightModifier() const
{
	u32 totalWeight = 0;

    Assert(GetKit());
	for (s32 i = 0; i < VMT_MAX; ++i)
	{
		if (m_mods[i] == INVALID_MOD)
			continue;

		if (i < VMT_RENDERABLE)
		{
			const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[i]];
			totalWeight += mod.GetWeightModifier();
		}
		else if (i < VMT_TOGGLE_MODS || i == VMT_WHEELS_REAR_OR_HYDRAULICS)
		{
			const CVehicleModStat& mod = GetKit()->GetStatMods()[m_mods[i]];
			totalWeight += mod.GetWeightModifier();
		}
		else
		{
			// toggle mods have no weight
		}
	}

	return totalWeight;
}

float CVehicleVariationInstance::GetArmourDamageMultiplier() const
{
	u32 currentModifier = GetArmourModifier();

	if (currentModifier == 0)
	{
		return 1.0f;
	}

	float armorPct = (float)currentModifier / 100.0f;
	float multiplier = 1.0f - (CVehicleDamage::ms_fArmorModMagnitude * armorPct);
	return multiplier;
}

bool CVehicleVariationInstance::HideModOnBone(const CVehicle* veh, s32 boneIndex, s8& slotTurnedOff)
{
	slotTurnedOff = -1;

    if (!GetKit() || !veh)
        return false;

	bool ret = false;
	for (s32 i = 0; i < VMT_RENDERABLE; ++i)
	{
		if (m_mods[i] == INVALID_MOD)
			continue;

		const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[i]];
		s32 modBone = veh->GetBoneIndex((eHierarchyId)mod.GetBone());

		if (modBone != boneIndex)
			continue;

		// turn off this mod
		m_renderableIsVisible &= ~(BIT_64(i));

#if __BANK
		if( slotTurnedOff != -1 )
		{
			Warningf("Vehicle: %s\tMod: %d is on the same bone as Mod: %d", veh->GetModelName(), i, slotTurnedOff );
		}
#endif

		Assign(slotTurnedOff, i);
		ret = true;
	}

	for (s32 i = 0; i < MAX_LINKED_MODS; ++i)
	{
		if (m_linkedMods[i] == INVALID_MOD)
			continue;

		const CVehicleModLink& mod = GetKit()->GetLinkMods()[m_linkedMods[i]];
		s32 modBone = veh->GetBoneIndex((eHierarchyId)mod.GetBone());

		if (modBone != boneIndex)
			continue;

		// turn off this mod
		m_renderableIsVisible &= ~(BIT_64(VMT_RENDERABLE + i));
		ret = true;
	}

	return ret;
}

void CVehicleVariationInstance::ShowModOnBone(const CVehicle* veh, s32 boneIndex)
{
    if (!GetKit() || !veh)
        return;

	for (s32 i = 0; i < VMT_RENDERABLE; ++i)
	{
		if (m_mods[i] == INVALID_MOD)
			continue;

		const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[i]];
		s32 modBone = veh->GetBoneIndex((eHierarchyId)mod.GetBone());

		if (modBone != boneIndex)
			continue;

		// turn off this mod
		m_renderableIsVisible |= BIT_64(i);
	}

	for (s32 i = 0; i < MAX_LINKED_MODS; ++i)
	{
		if (m_linkedMods[i] == INVALID_MOD)
			continue;

		const CVehicleModLink& mod = GetKit()->GetLinkMods()[m_linkedMods[i]];
		s32 modBone = veh->GetBoneIndex((eHierarchyId)mod.GetBone());

		if (modBone != boneIndex)
			continue;

		// turn off this mod
		m_renderableIsVisible |= BIT_64(VMT_RENDERABLE + i);
	}
}


bool CVehicleVariationInstance::IsBoneModed(eHierarchyId hierarchID)
{
	for(s32 i = 0; i < VMT_RENDERABLE; ++i)
	{
		if(m_mods[i] == INVALID_MOD)
			continue;

		const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[i]];
		
		if((eHierarchyId)mod.GetBone() == hierarchID)
		{
			return (m_renderableIsVisible & BIT_64(i)) != 0;
		}

		for(int b = 0; b < mod.GetNumBonesToTurnOff(); ++b)
		{
			if((eHierarchyId)mod.GetBoneToTurnOff(b) == hierarchID)
			{
				return true;
			}
		}
	}

	return false;
}

u8 CVehicleVariationInstance::GetModSlotFromBone(eHierarchyId hierarchID)
{
	for(u8 i = 0; i < VMT_RENDERABLE; ++i)
	{
		if(m_mods[i] == INVALID_MOD)
			continue;

		const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[i]];

		if((eHierarchyId)mod.GetBone() == hierarchID)
		{
			return i;
		}
	}

	for (u8 i = 0; i < MAX_LINKED_MODS; ++i)
	{
		if (m_linkedMods[i] == INVALID_MOD)
			continue;

		const CVehicleModLink& mod = GetKit()->GetLinkMods()[m_linkedMods[i]];

		if((eHierarchyId)mod.GetBone() == hierarchID)
		{
			return VMT_RENDERABLE + i;
		}
	}

	return INVALID_MOD;
}


bool CVehicleVariationInstance::IsBoneTurnedOff(const CVehicle* veh, s32 boneIndex) const
{
	if (!GetKit())
		return false;

	for (s32 i = 0; i < VMT_RENDERABLE; ++i)
	{
		if (m_mods[i] == INVALID_MOD)
			continue;

		const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[i]];

		for (u32 f = 0; f < mod.GetNumBonesToTurnOff(); ++f)
		{
			if (veh->GetBoneIndex((eHierarchyId)mod.GetBoneToTurnOff(f)) == boneIndex)
				return true;
		}
	}

	return false;
}

s32 CVehicleVariationInstance::GetBone(u8 slot) const
{
	if (!GetKit() || slot >= VMT_RENDERABLE || m_mods[slot] == INVALID_MOD)
		return -1;

	const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[slot]];
	return (s32)mod.GetBone();
}

s32 CVehicleVariationInstance::GetBoneForLinkedMod(u8 slot) const
{
	if (!GetKit() || slot >= MAX_LINKED_MODS || m_linkedMods[slot] == INVALID_MOD)
		return -1;

	const CVehicleModLink& mod = GetKit()->GetLinkMods()[m_linkedMods[slot]];
	return (s32)mod.GetBone();
}

s32 CVehicleVariationInstance::GetBoneToTurnOffForSlot(u8 slot, u32 index) const
{
	if (!GetKit() || slot >= VMT_RENDERABLE || m_mods[slot] == INVALID_MOD)
		return -1;

	const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[slot]];
	if (index >= mod.GetNumBonesToTurnOff())
		return -1;

	return (s32)mod.GetBoneToTurnOff(index);
}

s32 CVehicleVariationInstance::GetNumBonesToTurnOffForSlot(u8 slot) const
{
	if (!GetKit() || slot >= VMT_RENDERABLE || m_mods[slot] == INVALID_MOD)
		return -1;

	if( m_mods[slot] < GetKit()->GetVisibleMods().GetCount() )
	{
		if( const CVehicleModVisible* mod = &GetKit()->GetVisibleMods()[m_mods[slot]] )
		{
			return (s32)mod->GetNumBonesToTurnOff();
		}
	}
	return 0;
}

bool CVehicleVariationInstance::ShouldDrawOriginalForSlot(CVehicle* veh, u8 slot) const
{
	if (!GetKit() || slot >= VMT_RENDERABLE || m_mods[slot] == INVALID_MOD)
		return false;

	const CVehicleModVisible& mod = GetKit()->GetVisibleMods()[m_mods[slot]];

	s32 modBone = mod.GetBone();

	s32 boneIdx = veh->GetBoneIndex((eHierarchyId)modBone);
	return !IsBoneTurnedOff(veh, boneIdx);
}

const atArray<CVehicleModColor>* GetColArray(eVehicleColorType type, bool base)
{
    if (!CVehicleModelInfo::GetVehicleModColors())
        return NULL;

    const atArray<CVehicleModColor>* colArray = NULL;
    switch (type)
    {
        case VCT_METALLIC:
            colArray = &CVehicleModelInfo::GetVehicleModColors()->m_metallic;
            break;
        case VCT_CLASSIC:
            colArray = &CVehicleModelInfo::GetVehicleModColors()->m_classic;
            break;
        case VCT_PEARLESCENT:
            if (base)
                colArray = &CVehicleModelInfo::GetVehicleModColors()->m_pearlescent.m_baseCols;
            else
                colArray = &CVehicleModelInfo::GetVehicleModColors()->m_pearlescent.m_specCols;
            break;
        case VCT_MATTE:
            colArray = &CVehicleModelInfo::GetVehicleModColors()->m_matte;
            break;
        case VCT_METALS:
            colArray = &CVehicleModelInfo::GetVehicleModColors()->m_metals;
            break;
        case VCT_CHROME:
            colArray = &CVehicleModelInfo::GetVehicleModColors()->m_chrome;
            break;
		case VCT_CHAMELEON:
			colArray = CVehicleModelInfo::GetVehicleModColorsGen9() ? &CVehicleModelInfo::GetVehicleModColorsGen9()->m_chameleon : NULL;
			break;
        case VCT_NONE:
        default:
            colArray = NULL;
    };

    return colArray;
}

void CVehicleVariationInstance::SetModColor1(eVehicleColorType type, int baseColIndex, int specColIndex)
{
    m_colorType1 = type;
	m_baseColorIndex = baseColIndex;
	m_specColorIndex = specColIndex;

    // set base and spec color
    const atArray<CVehicleModColor>* colArray = GetColArray(m_colorType1, true);
    if (colArray)
    {
        if (baseColIndex >= 0 && baseColIndex < colArray->GetCount())
        {
            m_color1 = (*colArray)[baseColIndex].col;
            m_color3 = (*colArray)[baseColIndex].spec;
        }
    }

    // if current type is pearlescent, set a custom spec color
    if (m_colorType1 == VCT_PEARLESCENT)
    {
        const atArray<CVehicleModColor>* colArray = GetColArray(m_colorType1, false);
        if (colArray)
        {
            if (specColIndex >= 0 && specColIndex < colArray->GetCount())
            {
                m_color3 = (*colArray)[specColIndex].col;
            }
        }
    }
}

void CVehicleVariationInstance::SetModColor2(eVehicleColorType type, int baseColIndex)
{
    m_colorType2 = type;
	m_baseColorIndex2 = baseColIndex;

    // color 2 doesn't support pearlescent with custom spec color because we only support one spec in the shader, and this comes from color 1
    const atArray<CVehicleModColor>* colArray = GetColArray(m_colorType2, true);
    if (colArray)
    {
        if (baseColIndex >= 0 && baseColIndex < colArray->GetCount())
        {
            m_color2 = (*colArray)[baseColIndex].col;
            // ignore spec on color 2, we don't support that in the shader so use spec from color 1
        }
    }
}

void CVehicleVariationInstance::GetModColor1(eVehicleColorType& type, int& baseColIndex, int& specColIndex)
{
    type = m_colorType1;
    baseColIndex = m_baseColorIndex;
	specColIndex = m_specColorIndex;
}

void CVehicleVariationInstance::GetModColor2(eVehicleColorType& type, int& baseColIndex)
{
    type = m_colorType2;
    baseColIndex = m_baseColorIndex2;
}

const char* CVehicleVariationInstance::GetModColor1Name(bool spec) const
{
    const atArray<CVehicleModColor>* colArray = GetColArray(m_colorType1, !spec);
    if (colArray)
    {
		if (m_colorType1 == VCT_PEARLESCENT && spec && m_specColorIndex > -1 && m_specColorIndex < colArray->GetCount())
			return (*colArray)[m_specColorIndex].name;
		else if (m_baseColorIndex > -1 && m_baseColorIndex < colArray->GetCount())
			return (*colArray)[m_baseColorIndex].name;
	}

    return NULL;
}

const char* CVehicleVariationInstance::GetModColor2Name() const
{
    const atArray<CVehicleModColor>* colArray = GetColArray(m_colorType2, true);
    if (colArray)
    {
		if (m_baseColorIndex2 > -1 && m_baseColorIndex2 < colArray->GetCount())
			return (*colArray)[m_baseColorIndex2].name;
    }

    return NULL;
}

eVehicleWheelType CVehicleVariationInstance::GetWheelType(const CVehicle* pVeh) const
{
	if (!pVeh)
		return VWT_SPORT;

	if (m_wheelType >= 0 && m_wheelType < VWT_MAX)
		return (eVehicleWheelType)m_wheelType;

	return pVeh->GetVehicleModelInfo()->GetWheelType();
}

void CVehicleVariationInstance::SetWheelType(eVehicleWheelType type)
{
    m_wheelType = (s8)type;

	if (m_mods[VMT_WHEELS] != INVALID_MOD && m_mods[VMT_WHEELS] >= CVehicleModelInfo::GetVehicleColours()->m_Wheels[m_wheelType].GetCount())
		m_mods[VMT_WHEELS] = 0;

	if (m_mods[VMT_WHEELS_REAR_OR_HYDRAULICS] != INVALID_MOD && m_mods[VMT_WHEELS_REAR_OR_HYDRAULICS] >= CVehicleModelInfo::GetVehicleColours()->m_Wheels[m_wheelType].GetCount())
		m_mods[VMT_WHEELS_REAR_OR_HYDRAULICS] = 0;
}

bool CVehicleVariationInstance::HaveModsStreamedIn(CVehicle* pVeh)
{
	if (!pVeh)
		return true;
	
	CVehicleStreamRequestGfx* req = pVeh->GetExtensionList().GetExtension<CVehicleStreamRequestGfx>();
	if (req)
		return false;

	return true;
}

void CVehicleVariationInstance::SetModCollision(CVehicle* pVeh, u32 slot, bool on)
{
	if (!pVeh)
		return;

	fragInst* pFragInst = pVeh->GetVehicleFragInst();
	Assertf(pFragInst, "Vehicle '%s' has no fraginst!", pVeh->GetModelName());
	phBoundComposite* pBoundComp = static_cast<phBoundComposite*>(pFragInst->GetArchetype()->GetBound());

	const s32 boneIdx = pVeh->GetBoneIndex((eHierarchyId)slot);
	if (boneIdx != -1)
	{
		int group = pFragInst->GetGroupFromBoneIndex(boneIdx);
		if (group > -1)
		{
			fragPhysicsLOD* physicsLOD = pFragInst->GetTypePhysics();
			fragTypeGroup* pGroup = physicsLOD->GetGroup(group);
			int child = pGroup->GetChildFragmentIndex();
			for (s32 f = 0; f < pGroup->GetNumChildren(); ++f)
			{
				pBoundComp->SetIncludeFlags(child + f, on ? ArchetypeFlags::GTA_VEHICLE_INCLUDE_TYPES : 0);
			}
		}
	}
}

void CVehicleVariationInstance::PreRender2()
{
	m_modMatricesStoredThisFrame.ClearAllFlags();
}

bool CVehicleVariationInstance::HasModMatrixBeenStoredThisFrame(u8 i) const
{
	vehicleAssert(i < 64);
	return (m_modMatricesStoredThisFrame.IsFlagSet(BIT_64(i)));
}

void CVehicleVariationInstance::SetModMatrixStoredThisFrame(u8 i)
{
	vehicleAssert(i < 64);
	m_modMatricesStoredThisFrame.SetFlag(BIT_64(i));
}

u32 CVehicleVariationInstance::GetModifier(eVehicleModType modSlot) const
{
    if (!GetKit())
        return 0;

	if ((modSlot < VMT_STAT_MODS || modSlot >= VMT_TOGGLE_MODS) && modSlot != VMT_WHEELS_REAR_OR_HYDRAULICS)
	{
		Assertf(false, "Mod type %d has no modifier value", modSlot);
		return 0;
	}
	
	if (m_mods[modSlot] == INVALID_MOD)
		return 0;

	const CVehicleModStat& mod = GetKit()->GetStatMods()[m_mods[modSlot]];
	return mod.GetModifier();
}

u32	CVehicleVariationInstance::GetWeightModifer(eVehicleModType modSlot) const
{
	if ((modSlot < VMT_STAT_MODS || modSlot >= VMT_TOGGLE_MODS) && modSlot != VMT_WHEELS_REAR_OR_HYDRAULICS)
	{
		Assertf(false, "Mod type %d has no weight modifier value", modSlot);
		return 0;
	}

	if (m_mods[modSlot] == INVALID_MOD)
		return 0;

	const CVehicleModStat& mod = GetKit()->GetStatMods()[m_mods[modSlot]];
	return mod.GetWeightModifier();
}

u32 CVehicleVariationInstance::GetIdentifierHashForType(eVehicleModType modSlot, u32 index) const
{
	const CVehicleKit* kit = GetKit();
	if (!kit)
	{
		Assertf(false, "Vehicle has no mod kit assigned!");
		return 0;
	}

	if ((modSlot < VMT_STAT_MODS || modSlot >= VMT_TOGGLE_MODS) && modSlot != VMT_WHEELS_REAR_OR_HYDRAULICS)
	{
		Assertf(false, "Mod type %d has no modifier value", modSlot);
		return 0;
	}

	u32 modifier = 0;
	s32 count = -1;
	for (s32 i = 0; i < kit->GetStatMods().GetCount(); ++i)
	{
		if (kit->GetStatMods()[i].GetType() == modSlot)        
			count++;

		if (count == (s32)index)
		{
			modifier = kit->GetStatMods()[i].GetIdentifierHash();
			break;
		}
	}

	return modifier;
}

u32 CVehicleVariationInstance::GetModifierForType(eVehicleModType modSlot, u32 index) const
{
	const CVehicleKit* kit = GetKit();
	if (!kit)
	{
		Assertf(false, "Vehicle has no mod kit assigned!");
		return 0;
	}

	if ((modSlot < VMT_STAT_MODS || modSlot >= VMT_TOGGLE_MODS) && modSlot != VMT_WHEELS_REAR_OR_HYDRAULICS)
	{
		Assertf(false, "Mod type %d has no modifier value", modSlot);
		return 0;
	}

	u32 modifier = 0;
	s32 count = -1;
	for (s32 i = 0; i < kit->GetStatMods().GetCount(); ++i)
	{
		if (kit->GetStatMods()[i].GetType() == modSlot)        
			count++;

		if (count == (s32)index)
		{
			modifier = kit->GetStatMods()[i].GetModifier();
			break;
		}
	}

	return modifier;
}

const char* CVehicleVariationInstance::GetModShopLabelForType(eVehicleModType modSlot, u32 modIndex, CVehicle* pVehicle) const
{
	u8 globalModIndex = GetGlobalModIndex(modSlot, (u8)modIndex, pVehicle);

	const CVehicleKit* kit = GetKit();
	if (!kit)
	{
		Assertf(false, "Vehicle has no mod kit assigned!");
		return NULL;
	}

	if (modSlot >= VMT_RENDERABLE && modSlot != VMT_WHEELS && modSlot != VMT_WHEELS_REAR_OR_HYDRAULICS)
	{
		Assertf(false, "Mod type %d has no text label", modSlot);
		return NULL;
	}

	const char* label = NULL;
	if (modSlot < VMT_RENDERABLE)
	{
		int	maxModIndex = kit->GetVisibleMods().size();
		if( globalModIndex >= maxModIndex )
		{
			Assertf(false, "globalModIndex >= kit->GetVisibleMods().size().. globalModIndex = %d, kit->GetVisibleMods().size() = %d", globalModIndex, maxModIndex);
			return NULL;
		}

		label = kit->GetVisibleMods()[globalModIndex].GetModShopLabel();

		//         s32 count = -1;
		//         for (s32 i = 0; i < kit->GetVisibleMods().GetCount(); ++i)
		//         {
		//             if (kit->GetVisibleMods()[i].GetType() == modSlot)        
		//                 count++;
		// 
		//             if (count == (s32)index)
		//             {
		//                 label = kit->GetVisibleMods()[i].GetModShopLabel();
		//                 break;
		//             }
		//         }
	}
	else if (modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS)
	{
		eVehicleWheelType wheelType = GetWheelType(pVehicle);
		if (globalModIndex < CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType].GetCount())
		{
			const CVehicleWheel& modWheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][globalModIndex];
			label = modWheel.GetModShopLabel();
		}
	}

	return label;
}

float CVehicleVariationInstance::GetAudioApply(eVehicleModType modSlot) const
{
    if (!GetKit())
        return 1.f;

	if (modSlot >= VMT_TOGGLE_MODS)
	{
		Assertf(false, "Mod type %d has no audio apply modifier value", modSlot);
		return 1.f;
	}

	if (m_mods[modSlot] == INVALID_MOD)
		return 1.f;

    if (modSlot < VMT_RENDERABLE)
        return GetKit()->GetVisibleMods()[m_mods[modSlot]].GetAudioApply();

    const CVehicleModStat& mod = GetKit()->GetStatMods()[m_mods[modSlot]];
    return mod.GetAudioApply();
}

void CVehicleVariationInstance::SetXenonLightColor(u8 col)
{
	Assert((col < CVehicleModelInfo::GetVehicleColours()->m_XenonLightColors.GetCount()) || (col==0xFF));

	m_xenonLightColor = (col < CVehicleModelInfo::GetVehicleColours()->m_XenonLightColors.GetCount()) ? col : /*invalid*/0xff;
}


CVehicleModelInfoVariation::CVehicleModelInfoVariation()
{
#if __BANK
	m_vehicleModelEditingIndex = 0;
	m_vehicleCopyFromIndex = 0;
	m_vehicleModelBankGroup = NULL;
	m_vehicleCurrentModelBankGroup = NULL;
#endif
}

CVehicleModelInfoVariation::~CVehicleModelInfoVariation()
{
#if __BANK
	DeleteWidgetNames();

	bkBank* rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_vehicleModelBankGroup)
		rootBank->DeleteGroup(*m_vehicleModelBankGroup);
	if (rootBank && m_vehicleCurrentModelBankGroup)
		rootBank->DeleteGroup(*m_vehicleCurrentModelBankGroup);
#endif
}

CVehicleVariationData::CVehicleVariationData() :
	m_modelName("NOT SET"), 
	m_lightSettingsIndex(0), 
	m_sirenSettingsIndex(0), 
	m_lightSettings(INVALID_VEHICLE_LIGHT_SETTINGS_ID), 
	m_sirenSettings(INVALID_SIREN_SETTINGS_ID)
#if __BANK
, m_selectedColorBit(0)
#endif
{
}

void CVehicleModelInfoVariation::OnPostLoad()
{
	for (s32 i = 0; i < m_variationData.GetCount(); ++i)
	{
		m_variationData[i].OnPostLoad();
	}
}

void CVehicleModelInfoVariation::OnPreSave()
{
	for (s32 i = 0; i < m_variationData.GetCount(); ++i)
	{
		m_variationData[i].OnPreSave();
	}
}

#if __BANK
s32 QSortVehicleNames(const char* const* lhs, const char* const* rhs)
{
	return stricmp(*lhs, *rhs);
}

void CVehicleModelInfoVariation::AddWidgets(bkBank& bank)
{
	// copy out the model names for widgets
	DeleteWidgetNames();
	m_vehicleNames.Reserve(m_variationData.GetCount());
	for (s32 i = 0; i < m_variationData.GetCount(); ++i)
	{
		m_vehicleNames.Push(StringDuplicate(m_variationData[i].GetModelName()));
	}
	m_vehicleNames.QSort(0, -1, QSortVehicleNames);

	// build array of vehicle name indices into the unsorted list
	// not very nice but needed to show the vehicles in alphabetical order in rag
	m_vehicleIndices.Resize(m_vehicleNames.GetCount());
	for (s32 i = 0; i < m_vehicleNames.GetCount(); ++i)
	{
		for (s32 f = 0; f < m_variationData.GetCount(); ++f)
		{
			if (!strcmp(m_vehicleNames[i], m_variationData[f].GetModelName()))
			{
				m_vehicleIndices[i] = (u16)f;
				break;
			}
		}
	}

	m_vehicleModelBankGroup = bank.PushGroup("Vehicles", false);
	bank.AddCombo("Current Vehicle Model", &m_vehicleModelEditingIndex, m_vehicleNames.GetCount(), m_vehicleNames.GetElements(), datCallback(MFA(CVehicleModelInfoVariation::RefreshVehicleWidgets), this));
	bank.PopGroup();
}

void CVehicleModelInfoVariation::DeleteWidgetNames()
{
	for (s32 i = 0; i < m_vehicleNames.GetCount(); ++i)
	{
		if (m_vehicleNames[i])
		{
			delete[] m_vehicleNames[i];
			m_vehicleNames[i] = NULL;
		}
	}
	m_vehicleNames.Reset();
	m_vehicleIndices.Reset();
}

void CVehicleModelInfoVariation::RefreshVehicleWidgets()
{
	s32 actualEditingIndex = m_vehicleIndices[m_vehicleModelEditingIndex];

	if (actualEditingIndex >= m_variationData.GetCount())
		actualEditingIndex = m_variationData.GetCount()-1;

	// delete the old widgets
	bkBank* rootBank = BANKMGR.FindBank("Vehicles");
	if (rootBank && m_vehicleCurrentModelBankGroup)
		rootBank->DeleteGroup(*m_vehicleCurrentModelBankGroup);
	m_vehicleCurrentModelBankGroup = NULL;

	if (m_variationData.GetCount() > 0)
	{
		if (rootBank && m_vehicleModelBankGroup)
		{
			m_vehicleCurrentModelBankGroup = m_vehicleModelBankGroup->AddGroup(m_variationData[actualEditingIndex].GetModelName(), true);
			rootBank->SetCurrentGroup(*m_vehicleCurrentModelBankGroup);

			rootBank->AddButton("Add Color", datCallback(MFA(CVehicleModelInfoVariation::WidgetAddVehicleColor), this));
			m_variationData[actualEditingIndex].AddWidgets(*rootBank,
																	CVehicleModelInfo::GetVehicleColours()->GetColorNames(),
																	CVehicleModelInfo::GetVehicleColours()->GetLightNames(),
																	CVehicleModelInfo::GetVehicleColours()->GetSirenNames(),
																	CVehicleModelInfo::GetVehicleColours()->GetLicensePlateData());

			rootBank->AddCombo("Vehicle to copy colors from", &m_vehicleCopyFromIndex, m_vehicleNames.GetCount(), m_vehicleNames.GetElements());
			rootBank->AddButton("Copy Colors", datCallback(MFA(CVehicleModelInfoVariation::WidgetCopyColorsFromVehicle), this));

			rootBank->UnSetCurrentGroup(*m_vehicleCurrentModelBankGroup);
		}
	}
	else
	{
		m_vehicleModelEditingIndex = 0;
		m_vehicleCopyFromIndex = 0;
	}

}

void CVehicleModelInfoVariation::ReshuffleColors(s32 editingIndex)
{
	for(s32 i = 0; i < m_variationData.GetCount(); ++i)
	{
		for (s32 j = 0; j < m_variationData[i].GetNumColors(); ++j)
		{
			CVehicleModelColorIndices& c = m_variationData[i].GetColor(j);
			if (c.m_indices[0] >= editingIndex && editingIndex > 0)
				c.m_indices[0]--;
			if (c.m_indices[1] >= editingIndex && editingIndex > 0)
				c.m_indices[1]--;
			if (c.m_indices[2] >= editingIndex && editingIndex > 0)
				c.m_indices[2]--;
			if (c.m_indices[3] >= editingIndex && editingIndex > 0)
				c.m_indices[3]--;
		}
	}
}

void CVehicleModelInfoVariation::ReshuffleLights(s32 editingIndex)
{
	for(s32 i = 0; i < m_variationData.GetCount(); ++i)
	{
		u32 setting = m_variationData[i].GetLightSettings();
		if (setting >= editingIndex && editingIndex > 0)
		{
			setting--;
			m_variationData[i].SetLightSettings(setting);
		}
	}
}

void CVehicleModelInfoVariation::ReshuffleSirens(s32 editingIndex)
{
	for(s32 i = 0; i < m_variationData.GetCount(); ++i)
	{
		u32 setting = m_variationData[i].GetSirenSettings();
		if (setting >= editingIndex && editingIndex > 0)
		{
			setting--;
			m_variationData[i].SetSirenSettings(setting);
		}
	}
}

void CVehicleModelInfoVariation::WidgetAddVehicleColor()
{
	u8 currentColor = (u8)CVehicleModelInfo::GetVehicleColours()->GetColorEditingIndex();
	s32 actualEditingIndex = (s32)m_vehicleIndices[m_vehicleModelEditingIndex];
	CVehicleModelColorIndices& c = m_variationData[actualEditingIndex].AddColor();

	// set to the current selected color
	c.m_indices[0] = currentColor;
	c.m_indices[1] = currentColor;
	c.m_indices[2] = 0;
	c.m_indices[3] = 0;

	// refresh the widgets
	RefreshVehicleWidgets();
}

void CVehicleModelInfoVariation::WidgetCopyColorsFromVehicle()
{
	s32 actualCopyFromIndex = (s32)m_vehicleIndices[m_vehicleCopyFromIndex];
	s32 actualEditingIndex = (s32)m_vehicleIndices[m_vehicleModelEditingIndex];
	if (m_variationData.GetCount() > 0 && m_variationData.GetCount() > actualCopyFromIndex && m_variationData.GetCount() > actualEditingIndex)
	{
		while(m_variationData[actualEditingIndex].GetNumColors() > 0)
		{
			m_variationData[actualEditingIndex].WidgetRemoveVehicleColor(m_variationData[actualEditingIndex].GetNumColors() - 1);
		}

		if (m_variationData[actualCopyFromIndex].GetNumColors() > 0)
		{
			for(int i=0;i<m_variationData[actualCopyFromIndex].GetNumColors();i++)
			{
				m_variationData[actualEditingIndex].AddColor() = m_variationData[actualCopyFromIndex].GetColor(i);
			}
		}
	}

	// refresh the widgets
	RefreshVehicleWidgets();
}

void CVehicleVariationData::AddWidgets(bkBank& bank, const atArray<const char *>& colorNames, const atArray<const char *>& lightNames, const atArray<const char *>& sirenNames, const CVehicleModelInfoPlates &plateInfo)
{
	char colIdxName[64];

	if (m_colors.GetCount() > m_selectedColorBit.GetNumBits() )
	{
		// grow the selected color bitset (and keep the existing selected bit if there was one)
		int selectedBit = m_selectedColorBit.GetFirstBitIndex();
		m_selectedColorBit.Init(m_colors.GetCount());
		if (selectedBit >= 0)
			m_selectedColorBit.Set(selectedBit, true);
	}

	bank.PushGroup("Colors");
	for (s32 i = 0; i < m_colors.GetCount(); ++i)
	{
		CVehicleModelColorIndices & c = m_colors[i];
		formatf(colIdxName, "Colour %d", i);
		bank.AddTitle(colIdxName);
		bank.AddToggle("Current Selection", &m_selectedColorBit, (u8) i, datCallback(MFA1(CVehicleVariationData::WidgetChangedColorSelection),this,(CallbackData)(size_t)i));
		formatf(colIdxName, "Remove %d", i);
		bank.AddButton(colIdxName, datCallback(CFA2(CVehicleVariationData::WidgetRemoveVehicleColorCB),this));
		bank.AddCombo("Slot0",&c.m_indices[0],colorNames.GetCount(), const_cast<const char**>(colorNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)(size_t)i));
		bank.AddCombo("Slot1",&c.m_indices[1],colorNames.GetCount(), const_cast<const char**>(colorNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)(size_t)i));
		bank.AddCombo("Slot2",&c.m_indices[2],colorNames.GetCount(), const_cast<const char**>(colorNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)(size_t)i));
		bank.AddCombo("Slot3",&c.m_indices[3],colorNames.GetCount(), const_cast<const char**>(colorNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)(size_t)i));
		bank.AddCombo("Slot4",&c.m_indices[4],colorNames.GetCount(), const_cast<const char**>(colorNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)(size_t)i));
		bank.AddCombo("Slot5",&c.m_indices[5],colorNames.GetCount(), const_cast<const char**>(colorNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)(size_t)i));
		bank.AddToggle("Enabled for livery 1", &c.m_liveries[0], datCallback(MFA1(CVehicleVariationData::WidgetChangedColorSelection), this, 0));
		bank.AddToggle("Enabled for livery 2", &c.m_liveries[1], datCallback(MFA1(CVehicleVariationData::WidgetChangedColorSelection), this, 0));
		bank.AddToggle("Enabled for livery 3", &c.m_liveries[2], datCallback(MFA1(CVehicleVariationData::WidgetChangedColorSelection), this, 0));
		bank.AddToggle("Enabled for livery 4", &c.m_liveries[3], datCallback(MFA1(CVehicleVariationData::WidgetChangedColorSelection), this, 0));
	}
	bank.PopGroup();

	bank.PushGroup("License Plate Probabilities");
	m_plateProbabilities.AddWidgets(bank, plateInfo);
	bank.PopGroup();

	bank.PushGroup("Light Settings");
	bank.AddCombo("Settings",&m_lightSettingsIndex,lightNames.GetCount(), const_cast<const char**>(lightNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)-1));
	bank.PopGroup();

	bank.PushGroup("Siren Settings");
	bank.AddCombo("Settings",&m_sirenSettingsIndex,sirenNames.GetCount(), const_cast<const char**>(sirenNames.GetElements()), datCallback(MFA1(CVehicleVariationData::WidgetChangedVehicleColor),this,(CallbackData)-1));
	bank.PopGroup();
}

void CVehicleVariationData::WidgetRemoveVehicleColorCB(CallbackData obj, CallbackData clientData)
{
	bkWidget * button = (bkWidget*)clientData;
	int colorIndex = 0;
	if (button)
	{
		Assert(strncmp(button->GetTitle(),"Remove ",7)==0);
		colorIndex = atoi(button->GetTitle()+7);
	}
	((CVehicleVariationData*)obj)->WidgetRemoveVehicleColor(colorIndex);
}
void CVehicleVariationData::WidgetRemoveVehicleColor(const int colorIndex)
{
	if (colorIndex>=0 && colorIndex < m_colors.GetCount() && m_colors.GetCount()>0)
	{
		m_colors.Delete(colorIndex);
		for(int i=colorIndex;i<m_selectedColorBit.GetNumBits()-1;i++)
			m_selectedColorBit.Set(i,m_selectedColorBit.GetAndClear(i+1));

		CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName( m_modelName, NULL );
		modelinfoAssertf(pModelInfo, "ModelInfo doesn't exist");
		if( pModelInfo )
		{
			pModelInfo->UpdateModelColors(*this, 256);
		}

		CVehicleModelInfo::RefreshVehicleWidgets();
	}
}

void CVehicleVariationData::WidgetChangedVehicleColor(CallbackData variationIndexCD)
{
	s32 variationIndex = (s32)(size_t)variationIndexCD;
	CVehicleModelInfo* pModelInfo = (CVehicleModelInfo*)CModelInfo::GetBaseModelInfoFromName( m_modelName , NULL);
	modelinfoAssertf(pModelInfo, "ModelInfo doesn't exist");
	if( pModelInfo )
	{
		pModelInfo->UpdateModelColors(*this, 256);
	}

	if (variationIndex >= 0 /*&& m_selectedColorBit.IsSet(variationIndex)*/)
	{
		CEntityIterator entityIterator( IterateVehicles );
		CEntity* pEntity = entityIterator.GetNext();
		while(pEntity)
		{
			CVehicle * pVehicle = static_cast<CVehicle*>(pEntity);
			if (pVehicle->GetVehicleModelInfo() == pModelInfo)
			{
				pVehicle->SetBodyColour1( pModelInfo->GetPossibleColours(0,variationIndex) );
				pVehicle->SetBodyColour2( pModelInfo->GetPossibleColours(1,variationIndex) );
				pVehicle->SetBodyColour3( pModelInfo->GetPossibleColours(2,variationIndex) );
				pVehicle->SetBodyColour4( pModelInfo->GetPossibleColours(3,variationIndex) );
				pVehicle->SetBodyColour5( pModelInfo->GetPossibleColours(4,variationIndex) );
				pVehicle->SetBodyColour6( pModelInfo->GetPossibleColours(5,variationIndex) );
				// let shaders know, that body colours changed
				pVehicle->UpdateBodyColourRemapping();
			}
			pEntity = entityIterator.GetNext();
		}
	}
}

void CVehicleVariationData::WidgetChangedColorSelection(CallbackData variationIndex)
{
	u32 selectedIndex = (u32)(size_t)variationIndex;

	// turn off all the other bits and turn this bit on (if it was set and not reset)
	const bool turningBitOn = m_selectedColorBit.IsSet(selectedIndex);
	m_selectedColorBit.Reset();
	m_selectedColorBit.Set(selectedIndex, turningBitOn);

	// set all vehicles of this type to this color (or reset the possible color array in the model info if no bits are set)
	WidgetChangedVehicleColor(variationIndex);
}

CVehicleModelColorIndices& CVehicleVariationData::AddColor()
{
	CVehicleModelColorIndices& c = m_colors.Grow();

	// grow the selected color bitset
	int selectedBit = m_selectedColorBit.GetFirstBitIndex();
	if (m_selectedColorBit.GetNumBits() < m_colors.GetCount())
		m_selectedColorBit.Init(m_colors.GetCount());
	if (selectedBit == -1)
		// if nothing selected then make this new color the selected one
		selectedBit = m_colors.GetCount()-1;
	m_selectedColorBit.Set(selectedBit, true);

	return c;
}
#endif // __BANK

void CVehicleVariationData::OnPostLoad()
{
	Assert(m_lightSettings != INVALID_VEHICLE_LIGHT_SETTINGS_ID);

	if(m_lightSettings == INVALID_VEHICLE_LIGHT_SETTINGS_ID)
	{
		if(m_lightSettingsIndex < CVehicleModelInfo::GetVehicleColours()->m_Lights.GetCount())
			m_lightSettings = CVehicleModelInfo::GetVehicleColours()->m_Lights[m_lightSettingsIndex].id;
	}
	else
	{
		m_lightSettingsIndex = CVehicleModelInfo::GetVehicleColours()->GetLightIndexById(m_lightSettings);
	}

	Assert(m_sirenSettings != INVALID_SIREN_SETTINGS_ID);

	if(m_sirenSettings == INVALID_SIREN_SETTINGS_ID)
	{
		if(m_sirenSettingsIndex < CVehicleModelInfo::GetVehicleColours()->m_Sirens.GetCount())
			m_sirenSettings = CVehicleModelInfo::GetVehicleColours()->m_Sirens[m_sirenSettingsIndex].id;
	}
	else
	{
		m_sirenSettingsIndex = CVehicleModelInfo::GetVehicleColours()->GetSirenIndexById(m_sirenSettings);
	}
}

void CVehicleVariationData::OnPreSave()
{
	if(m_sirenSettingsIndex < CVehicleModelInfo::GetVehicleColours()->m_Sirens.GetCount())
		m_sirenSettings = CVehicleModelInfo::GetVehicleColours()->m_Sirens[m_sirenSettingsIndex].id;
	else
		m_sirenSettings = INVALID_VEHICLE_LIGHT_SETTINGS_ID;

	if(m_lightSettingsIndex < CVehicleModelInfo::GetVehicleColours()->m_Lights.GetCount())
		m_lightSettings = CVehicleModelInfo::GetVehicleColours()->m_Lights[m_lightSettingsIndex].id;
	else
		m_lightSettings = INVALID_SIREN_SETTINGS_ID;
}

CVehicleStreamRequestGfx::CVehicleStreamRequestGfx() : m_pTargetEntity(NULL), m_state(VS_INIT), m_bDelayCompletion(false)
{ 
	m_reqFrags.UseAsArray();
	m_reqDrawables.UseAsArray();
}

CVehicleStreamRequestGfx::~CVehicleStreamRequestGfx()
{
	Release();
	Assert(m_state == VS_FREED);
}

s32 CVehicleStreamRequestGfx::AddFrag(u8 modSlot, const strStreamingObjectName fragName, s32 streamFlags, s32 parentTxd)
{
	strLocalIndex fragSlotId = strLocalIndex(g_FragmentStore.FindSlot(fragName));
	
	if (fragSlotId != -1)
	{
		// this check is pretty nasty, but the vehicle artists share mods between vehicles and the parent id WILL change
		// causing streaming refcounting issues. this way we just don't reparent the mod asset and they'll have to make sure
		// the parenting hierarchy matches for the shared mods.
		if (g_FragmentStore.GetParentTxdForSlot(fragSlotId) == strLocalIndex(STRLOCALINDEX_INVALID))
		{
			g_FragmentStore.SetParentTxdForSlot(fragSlotId, strLocalIndex(parentTxd));
		}
		m_reqFrags.SetRequest(modSlot, fragSlotId.Get(), g_FragmentStore.GetStreamingModuleId(), streamFlags);
	} 
	else 
	{
		if(fragName.GetHash() != MID_DUMMY_MOD_FRAG_MODEL)
		{
			vehicleAssertf(false, "Streamed frag request for vehicle is not found : %s", fragName.GetCStr());
		}
	}

	return fragSlotId.Get();
}

s32 CVehicleStreamRequestGfx::AddDrawable(eVehicleModType modSlot, const strStreamingObjectName drawableName, s32 streamFlags, s32 parentTxd)
{
	strLocalIndex drawableSlotId = strLocalIndex(g_DrawableStore.FindSlot(drawableName));
	
	if (drawableSlotId != -1)
	{
		g_DrawableStore.SetParentTxdForSlot(drawableSlotId, strLocalIndex(parentTxd));

		// this will only be called for wheels
		m_reqDrawables.SetRequest(modSlot - VMT_WHEELS, drawableSlotId.Get(), g_DrawableStore.GetStreamingModuleId(), streamFlags);
	} 
	else 
	{
		vehicleAssertf(false, "Streamed drawable request for vehicle is not found : %s", drawableName.GetCStr());
	}

	return drawableSlotId.Get();
}

void CVehicleStreamRequestGfx::Release()
{
	m_reqFrags.ReleaseAll();
	m_reqDrawables.ReleaseAll();

	m_state = VS_FREED;
}

// requests have completed - add refs to the slots we wanted and free up the initial requests
void CVehicleStreamRequestGfx::RequestsComplete()
{ 
	Assert(m_state == VS_REQ);

	CVehicle* pTarget = GetTargetEntity();

	if (Verifyf(CVehicleStreamRenderGfx::GetPool()->GetNoOfFreeSpaces() > 0, "No space left in CVehicleStreamRenderGfx pool. Too many modded cars created!"))
	{
		pTarget->GetVariationInstance().SetVehicleRenderGfx(rage_new CVehicleStreamRenderGfx(this, pTarget));		
	}
	else
	{
#if !__NO_OUTPUT
		Displayf("Ran out of pool entries for CVehicleStreamRenderGfx frame %d", fwTimer::GetFrameCount());
		for (s32 i = 0; i < CVehicleStreamRenderGfx::GetPool()->GetSize(); ++i)
		{
			CVehicleStreamRenderGfx* gfx = CVehicleStreamRenderGfx::GetPool()->GetSlot(i);
			if (gfx)
			{
				Displayf("CVehicleStreamRenderGfx %d / %d: %s (%p) (%s) (%d refs) (frame %d)\n", i, CVehicleStreamRenderGfx::GetPool()->GetSize(), gfx->m_targetEntity ? gfx->m_targetEntity->GetModelName() : "NULL", gfx->m_targetEntity.Get(), gfx->m_used ? "used" : "not used", gfx->m_refCount, gfx->m_frameCreated);
				if (gfx->m_targetEntity)
				{
					CVehicle* pCurrVehicle = gfx->m_targetEntity;
					Displayf("Vehicle owned by %d (%s) \n",pCurrVehicle->GetOwnedBy(), (pCurrVehicle->GetOwnedBy()==ENTITY_OWNEDBY_SCRIPT) ? "SCRIPT" : "OTHER");
				}
			}
			else
			{
				Displayf("CVehicleStreamRenderGfx %d / %d: null slot\n", i, CVehicleStreamRenderGfx::GetPool()->GetSize());
			}
		}
#endif // __NO_OUTPUT

		if (pTarget->GetVariationInstance().GetVehicleRenderGfx())
		{
			pTarget->GetVariationInstance().SetVehicleRenderGfx(NULL);
		}

		// remove any mods to render the original vehicle (instead of missing geometry)
		pTarget->GetVariationInstance().ClearMods();
	}

	// detect loading of livery mods & use to set livery override ptr
	if (pTarget)
	{
		u32 modIndex = pTarget->GetVariationInstance().GetModIndex(VMT_LIVERY_MOD);
		if (modIndex != INVALID_MOD)
		{
			CVehicleStreamRenderGfx* pGfx = pTarget->GetVariationInstance().GetVehicleRenderGfx();

			if (AssertVerify(pGfx))
			{
				strLocalIndex fragSlotIdx = pGfx->GetFragIndex(VMT_LIVERY_MOD);
				if (fragSlotIdx != strIndex::INVALID_INDEX)
				{
					grmShaderGroup *pShaderGroup = g_FragmentStore.Get(fragSlotIdx)->GetCommonDrawable()->GetShaderGroupPtr();
					fwTxd const* pTxd = pShaderGroup ? pShaderGroup->GetTexDict() : NULL; // frag's internal txd
						
					Assertf(pTxd,"vehicle %s, livery mod %d, does not have a .txd embedded in the mod frag", pTarget->GetModelName(),modIndex); 
					if (pTxd && pTxd->GetCount() > 0)
					{
						pTarget->SetLiveryModFrag(fragSlotIdx);
					}
				}
			}
		}
	}

	m_state = VS_USE;
}

void CVehicleStreamRequestGfx::SetTargetEntity(CVehicle* pEntity) { 
	Assert(pEntity);

	// add this request extension to the extension list
	pEntity->GetExtensionList().Add(*this);
	m_pTargetEntity = pEntity; 
}

CVehicleStreamRenderGfx::CVehicleStreamRenderGfx() 
{
	for(s32 i=0; i<VMT_RENDERABLE; i++)
	{
		m_varModVisible[i] = NULL;
	}

	for(s32 i=0; i<MAX_LINKED_MODS; i++)
	{
		m_varModLinked[i] = NULL;
	}

	m_varWheel[0] = m_varWheel[1] = NULL;

	m_usingWheelVariation[0] = m_usingWheelVariation[1] = false;

	for (s32 i = 0; i < NUM_EXHAUST_BONES; ++i)
	{
		m_exhaustBones[i].m_boneIndex = -1;
		m_exhaustBones[i].m_modIndex = INVALID_MOD;
	}

	for (s32 i = 0; i < NUM_EXTRALIGHT_BONES; ++i)
	{
		m_extraLightBones[i].m_boneIndex = -1;
		m_extraLightBones[i].m_modIndex = INVALID_MOD;
	}

	m_refCount = 0;
	m_used = true;

	m_wheelTyreRadiusScale[0] = 1.f;
	m_wheelTyreWidthScale[0] = 1.f;
	m_wheelTyreRadiusScale[1] = 1.f;
	m_wheelTyreWidthScale[1] = 1.f;

#if !__NO_OUTPUT
	m_targetEntity = NULL;
	m_frameCreated = fwTimer::GetFrameCount();
#endif
}

const CVehicleVariationInstance* pDebugVar  = NULL;

CVehicleStreamRenderGfx::CVehicleStreamRenderGfx(CVehicleStreamRequestGfx* pReq, CVehicle* targetEntity)  
{
	Assert(pReq);

	m_wheelTyreRadiusScale[0] = 1.f;
	m_wheelTyreWidthScale[0] = 1.f;
	m_wheelTyreRadiusScale[1] = 1.f;
	m_wheelTyreWidthScale[1] = 1.f;

	m_usingWheelVariation[0] = m_usingWheelVariation[1] = false;

	for (s32 i = 0; i < NUM_EXHAUST_BONES; ++i)
	{
		m_exhaustBones[i].m_boneIndex = -1;
		m_exhaustBones[i].m_modIndex = INVALID_MOD;
	}

	for (s32 i = 0; i < NUM_EXTRALIGHT_BONES; ++i)
	{
		m_extraLightBones[i].m_boneIndex = -1;
		m_extraLightBones[i].m_modIndex = INVALID_MOD;
	}

	if (pReq)
	{
		CVehicleModelInfo *pVMI = targetEntity->GetVehicleModelInfo();

		const CVehicleVariationInstance& var = targetEntity->GetVariationInstance();
		pDebugVar = &var;

		const CVehicleKit* kit = var.GetKit();
		Assert(kit);

		for (s32 i = 0; i < VMT_RENDERABLE; ++i)
		{
			m_varModVisible[i] = NULL;

			u32 modEntry = var.GetMods()[i];
			if (modEntry == INVALID_MOD)
				continue;

			m_varModVisible[i] = (CVehicleModVisible*) &(kit->GetVisibleMods()[modEntry]);
		}

		for (s32 i = 0; i < MAX_LINKED_MODS; ++i)
		{
			m_varModLinked[i] = NULL;

			u32 modEntry = var.GetLinkedMods()[i];
			if (modEntry == INVALID_MOD)
				continue;

			m_varModLinked[i] = (CVehicleModLink*) &(kit->GetLinkMods()[modEntry]);
		}


		for(s32 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
		{
			strLocalIndex fragSlotId = strLocalIndex(pReq->GetFragId((u8)i));
			if (fragSlotId.IsValid())
			{
				g_FragmentStore.AddRef(fragSlotId, REF_OTHER);

				strLocalIndex*								pVarFragIdx=NULL;
				CCustomShaderEffectVehicleType**	ppVarFragCseType=NULL;

				if(i < VMT_RENDERABLE)
				{	// renderable mod:
					Assert(m_varModVisible[i]);
					pVarFragIdx			= &(m_varModVisible[i]->m_VariationFragIdx);
					ppVarFragCseType	= &(m_varModVisible[i]->m_VariationFragCseType);
				}
				else
				{	// linked mod:
					Assert(m_varModLinked[i-VMT_RENDERABLE]);
					pVarFragIdx			= &(m_varModLinked[i-VMT_RENDERABLE]->m_VariationFragIdx);
					ppVarFragCseType	= &(m_varModLinked[i-VMT_RENDERABLE]->m_VariationFragCseType);

				}
				Assert(pVarFragIdx);
				Assert(ppVarFragCseType);
				CCustomShaderEffectVehicleType*	pVarFragCseType = *ppVarFragCseType;

			#if __ASSERT
				if(pVarFragCseType)
				{
					Assert(*pVarFragIdx == fragSlotId);
				}
			#endif
				*pVarFragIdx = fragSlotId;

				fragType* type = this->GetFrag((u8)i);
				Assert(type);
				if(type)
				{
					if(pVarFragCseType)
					{
						pVarFragCseType->AddRef();
					}
					else
					{
						fragDrawable* drawable = type->GetCommonDrawable();
						if(drawable)
						{
						#if __ASSERT
							extern char* CCustomShaderEffectBase_EntityName;
							CCustomShaderEffectBase_EntityName = (char*)g_FragmentStore.GetName(fragSlotId);
						#endif

							CCustomShaderEffectVehicleType *pVehicleCSE = NULL;
							if(drawable->GetClonedShaderGroupPtr())
							{
								pVehicleCSE = (CCustomShaderEffectVehicleType*)CCustomShaderEffectBaseType::SetupForDrawable(drawable, MI_TYPE_VEHICLE, NULL,
																													0, -1, drawable->GetClonedShaderGroupPtr());
							}
							else
							{
								pVehicleCSE = (CCustomShaderEffectVehicleType*)CCustomShaderEffectBaseType::SetupForDrawable(drawable, MI_TYPE_VEHICLE, NULL,
																													0, -1, NULL);
								drawable->CloneShaderGroup();	// BS#2551385: clone drawable's shaderGroup for next use
							}
							Assert(pVehicleCSE);
							*ppVarFragCseType = pVehicleCSE;
						}
					}

					// check all frags for any additional exhaust bones they might add (should only check exhaust frags really)
					const crSkeletonData& skelData = type->GetSkeletonData();
					for (s32 f = 0; f < skelData.GetNumBones(); ++f)
					{
						const crBoneData* boneData = skelData.GetBoneData(f);
						if (!boneData)
							continue;

						// exhaust bones
						for (s32 g = 0 ; g < NUM_EXHAUST_BONES; ++g)
						{
							if (m_exhaustBones[g].m_boneIndex == -1 && !stricmp(boneData->GetName(), exhaustBoneNames[g]))
							{
								m_exhaustBones[g].m_boneIndex = (s8)boneData->GetIndex();
								m_exhaustBones[g].m_modIndex = (u8)i;
								break;
							}
						}

						// extra light bones
						CompileTimeAssert(NUM_EXTRALIGHT_BONES == 4);
						const char *postfix = strstr(boneData->GetName(), "_ng");

						if (m_extraLightBones[0].m_boneIndex == -1 && strstr(boneData->GetName(), "extralight_1") != NULL && postfix == NULL)
						{
							m_extraLightBones[0].m_boneIndex = (s8)boneData->GetIndex();
							m_extraLightBones[0].m_modIndex = (u8)i;
						}
						else if (m_extraLightBones[1].m_boneIndex == -1 && strstr(boneData->GetName(), "extralight_2") != NULL && postfix == NULL)
						{
							m_extraLightBones[1].m_boneIndex = (s8)boneData->GetIndex();
							m_extraLightBones[1].m_modIndex = (u8)i;
						}
						else if (m_extraLightBones[2].m_boneIndex == -1 && strstr(boneData->GetName(), "extralight_3") != NULL && postfix == NULL)
						{
							m_extraLightBones[2].m_boneIndex = (s8)boneData->GetIndex();
							m_extraLightBones[2].m_modIndex = (u8)i;
						}
						else if (m_extraLightBones[3].m_boneIndex == -1 && strstr(boneData->GetName(), "extralight_4") != NULL && postfix == NULL)
						{
							m_extraLightBones[3].m_boneIndex = (s8)boneData->GetIndex();
							m_extraLightBones[3].m_modIndex = (u8)i;
						}
					}

					rmcDrawable* drawable = type->GetCommonDrawable();
					if (drawable)
					{
						drawable->GetLodGroup().SetLodThresh(0, pVMI->GetVehicleLodDistance(VLT_LOD0));
						drawable->GetLodGroup().SetLodThresh(1, pVMI->GetVehicleLodDistance(VLT_LOD1));
					}
				}
			}
		}	

		eVehicleWheelType wheelType = var.GetWheelType(targetEntity);

		m_varWheel[0] = NULL;
		u8 wheelEntry0 = var.GetMods()[VMT_WHEELS];
		if(wheelEntry0 != INVALID_MOD)
		{
			m_varWheel[0] = &CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][ wheelEntry0 ];
		}
	
		const u32 modelNameHash = targetEntity->GetModelNameHash();

		m_varWheel[1] = NULL;
		if((targetEntity->GetVehicleType() == VEHICLE_TYPE_BIKE) || (modelNameHash==MID_TORNADO6) || (modelNameHash==MID_IMPALER2) || (modelNameHash==MID_IMPALER4) || (modelNameHash==MID_PEYOTE2))
		{
			u8 wheelEntry1 = var.GetMods()[VMT_WHEELS_REAR_OR_HYDRAULICS];
			if(wheelEntry1 != INVALID_MOD)
			{
				m_varWheel[1] = &CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][ wheelEntry1 ];
			}
		}


		bool overrideRimRadius = false;

		if(m_varWheel[0])
		{
			m_usingWheelVariation[0] = var.HasVariation(0);
			s32* varWheelDrawableIdx = m_usingWheelVariation[0] ? &m_varWheel[0]->m_VariationDrawableVarIdx : &m_varWheel[0]->m_VariationDrawableIdx;
			CCustomShaderEffectVehicleType** varWheelCseType = m_usingWheelVariation[0] ? &m_varWheel[0]->m_VariationDrawableVarCseType : &m_varWheel[0]->m_VariationDrawableCseType;

			strLocalIndex drawableSlotId = strLocalIndex(pReq->GetDrawableId(VMT_WHEELS));
	        if(drawableSlotId.IsValid())
	        {
	            g_DrawableStore.AddRef(drawableSlotId, REF_OTHER);

			#if __ASSERT
				if(*varWheelCseType)
				{
					Assert(*varWheelDrawableIdx == drawableSlotId.Get());	// must be the same drawable
				}
			#endif
				*varWheelDrawableIdx = drawableSlotId.Get();

				rmcDrawable* drawable = this->GetDrawable(VMT_WHEELS);
				Assertf(drawable, "%s: Wheel drawable is missing! drawblIdx: %d cseType: %p", targetEntity->GetModelName(), *varWheelDrawableIdx, *varWheelCseType);
				if(drawable)
				{
                    drawable->GetLodGroup().SetLodThresh(0, pVMI->GetVehicleLodDistance(VLT_LOD0));
                    drawable->GetLodGroup().SetLodThresh(1, pVMI->GetVehicleLodDistance(VLT_LOD1));

                    if(*varWheelCseType)
					{
						(*varWheelCseType)->AddRef();
					}
					else
					{
						#if __ASSERT
							extern char* CCustomShaderEffectBase_EntityName;
							CCustomShaderEffectBase_EntityName = (char*)g_DrawableStore.GetName(drawableSlotId);
						#endif
						CCustomShaderEffectVehicleType *pVehicleCSE = (CCustomShaderEffectVehicleType*)CCustomShaderEffectBaseType::SetupForDrawable(drawable, MI_TYPE_VEHICLE, NULL);
						Assert(pVehicleCSE);
						(*varWheelCseType) = pVehicleCSE;
					}
					
					// compute and store wheel scale needed for this drawable
					// first wheel is enough, all matrices are set up to account for front/rear differences
					Vector3 bbMin, bbMax;
					drawable->GetLodGroup().GetBoundingBox(bbMin, bbMax);
					const CVehicleStructure* vehStruct = targetEntity->GetVehicleModelInfo()->GetStructure();
					m_wheelTyreRadiusScale[0] = vehStruct->m_fWheelTyreRadius[0] / bbMax.z;
					m_wheelTyreWidthScale[0] = vehStruct->m_fWheelTyreWidth[0] / (bbMax.x * 2.f);

					// infernus2 has custom scale on rear wheel:
					if(pVMI->GetModelNameHash() == MID_INFERNUS2)
					{
						u32 rearIndex = 0;
						for (rearIndex = 0; rearIndex < NUM_VEH_CWHEELS_MAX; ++rearIndex)
						{
							if (vehStruct->m_isRearWheel & (1 << rearIndex))
								break;
						}

						if (rearIndex >= NUM_VEH_CWHEELS_MAX)
							rearIndex = 0;

						m_wheelTyreRadiusScale[1]	= vehStruct->m_fWheelTyreRadius[rearIndex] / bbMax.z;
						m_wheelTyreWidthScale[1]	= vehStruct->m_fWheelTyreWidth[rearIndex] / (bbMax.x * 2.f);
					}

					overrideRimRadius = true;
				}
	        }
			else
			{
				Assert(*varWheelDrawableIdx == -1);
				Assert(*varWheelCseType == NULL);

				*varWheelDrawableIdx = -1;
				*varWheelCseType = NULL;
			}
		}

		if(m_varWheel[1])
		{
			m_usingWheelVariation[1] = var.HasVariation(1);
			s32* varWheelDrawableIdx = m_usingWheelVariation[1] ? &m_varWheel[1]->m_VariationDrawableVarIdx : &m_varWheel[1]->m_VariationDrawableIdx;
			CCustomShaderEffectVehicleType** varWheelCseType = m_usingWheelVariation[1] ? &m_varWheel[1]->m_VariationDrawableVarCseType : &m_varWheel[1]->m_VariationDrawableCseType;

			strLocalIndex drawableSlotId = strLocalIndex(pReq->GetDrawableId(VMT_WHEELS_REAR_OR_HYDRAULICS));
			if (drawableSlotId.IsValid())
			{
				g_DrawableStore.AddRef(drawableSlotId, REF_OTHER);
			#if __ASSERT
				if(*varWheelCseType)
				{
					Assert(*varWheelDrawableIdx == drawableSlotId.Get());
				}
			#endif
				*varWheelDrawableIdx = drawableSlotId.Get();

				rmcDrawable* drawable = this->GetDrawable(VMT_WHEELS_REAR_OR_HYDRAULICS);
				Assertf(drawable, "%s: Rear wheel drawable is missing! drawblIdx: %d cseType: %p", targetEntity->GetModelName(), *varWheelDrawableIdx, *varWheelCseType);
				if(drawable)
				{
                    drawable->GetLodGroup().SetLodThresh(0, pVMI->GetVehicleLodDistance(VLT_LOD0));
                    drawable->GetLodGroup().SetLodThresh(1, pVMI->GetVehicleLodDistance(VLT_LOD1));

					if(*varWheelCseType)
					{
						(*varWheelCseType)->AddRef();
					}
					else
					{
						CCustomShaderEffectVehicleType *pVehicleCSE = (CCustomShaderEffectVehicleType*)CCustomShaderEffectBaseType::SetupForDrawable(drawable, MI_TYPE_VEHICLE, NULL);
						Assert(pVehicleCSE);
						*varWheelCseType = pVehicleCSE;
					}

					// compute and store wheel scale needed for this drawable
					// we have an explicit rear wheel so store that value as well
					Vector3 bbMin, bbMax;
					drawable->GetLodGroup().GetBoundingBox(bbMin, bbMax);
					const CVehicleStructure* vehStruct = targetEntity->GetVehicleModelInfo()->GetStructure();

					// find a rear wheel so we get correct scaling, and hopefully we don't have moddable rear wheels of different sizes
					u32 rearIndex = 0;
					for (rearIndex = 0; rearIndex < NUM_VEH_CWHEELS_MAX; ++rearIndex)
						if (vehStruct->m_isRearWheel & (1 << rearIndex))
							break;

					if (rearIndex >= NUM_VEH_CWHEELS_MAX)
						rearIndex = 0;

					m_wheelTyreRadiusScale[1] = vehStruct->m_fWheelTyreRadius[rearIndex] / bbMax.z;
					m_wheelTyreWidthScale[1] = vehStruct->m_fWheelTyreWidth[rearIndex] / (bbMax.x * 2.f);

					overrideRimRadius = true;
				}
			}
			else
			{
				Assert(*varWheelDrawableIdx	== -1);
				Assert(*varWheelCseType == NULL);
				*varWheelDrawableIdx = -1;
				*varWheelCseType = NULL;
			}
		}

		if (overrideRimRadius)
			OverrideWheelRimRadius(targetEntity);
	}

	m_refCount = 0;
	m_used = true;

	StoreModMatrices(targetEntity);

#if !__NO_OUTPUT
	m_targetEntity = targetEntity;
	m_frameCreated = fwTimer::GetFrameCount();
#endif

	pDebugVar = NULL;
}

CVehicleStreamRenderGfx::~CVehicleStreamRenderGfx()
{
	ReleaseGfx();
}

void CVehicleStreamRenderGfx::ReleaseGfx()
{
	for(s32 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		strLocalIndex*						pVarFragIdx=NULL;
		CCustomShaderEffectVehicleType**	ppVarFragCseType=NULL;

		if(i < VMT_RENDERABLE)
		{	// renderable mod:
			if(!m_varModVisible[i])
				continue;

			pVarFragIdx			= &(m_varModVisible[i]->m_VariationFragIdx);
			ppVarFragCseType	= &(m_varModVisible[i]->m_VariationFragCseType);
		}
		else
		{	// linked mod:
			if(!m_varModLinked[i-VMT_RENDERABLE])
				continue;

			pVarFragIdx			= &(m_varModLinked[i-VMT_RENDERABLE]->m_VariationFragIdx);
			ppVarFragCseType	= &(m_varModLinked[i-VMT_RENDERABLE]->m_VariationFragCseType);
		}
		Assert(pVarFragIdx);
		Assert(ppVarFragCseType);
		CCustomShaderEffectVehicleType*	pVarFragCseType = *ppVarFragCseType;


		if(pVarFragCseType)
		{
			if(pVarFragCseType->GetRefCount()==1)
			{
				fragType* type = this->GetFrag((u8)i);
				if(type)
				{
					rmcDrawable* drawable = type->GetCommonDrawable();
					if(drawable)
					{
						pVarFragCseType->RestoreModelInfoDrawable(drawable);
					}
				}
				
				pVarFragCseType->RemoveRef();
				(*ppVarFragCseType) = NULL;
				pVarFragCseType = NULL;
			}
			else
			{
				pVarFragCseType->RemoveRef();
			}
		}

		if((*pVarFragIdx) != -1)
		{
			g_FragmentStore.RemoveRef(*pVarFragIdx, REF_OTHER);
			
			if(!pVarFragCseType)
			{
				(*pVarFragIdx)	= -1;
			}
		}
	}

	if(m_varWheel[0])
	{
		s32* varWheelDrawableIdx = m_usingWheelVariation[0] ? &m_varWheel[0]->m_VariationDrawableVarIdx : &m_varWheel[0]->m_VariationDrawableIdx;
		CCustomShaderEffectVehicleType** varWheelCseType = m_usingWheelVariation[0] ? &m_varWheel[0]->m_VariationDrawableVarCseType : &m_varWheel[0]->m_VariationDrawableCseType;

		if (*varWheelCseType)
		{
			if((*varWheelCseType)->GetRefCount()==1)
			{
				rmcDrawable *drawable = this->GetDrawable(VMT_WHEELS);
				if(drawable)
				{
					(*varWheelCseType)->RestoreModelInfoDrawable(drawable);
				}

				(*varWheelCseType)->RemoveRef();
				*varWheelCseType = NULL;
			}
			else
			{
				(*varWheelCseType)->RemoveRef();
			}
		}

		if (*varWheelDrawableIdx != -1)
		{
			g_DrawableStore.RemoveRef(strLocalIndex(*varWheelDrawableIdx), REF_OTHER);

			if(!(*varWheelCseType))
			{
				*varWheelDrawableIdx = -1;
			}
		}
	}


	if(m_varWheel[1])
	{
		s32* varWheelDrawableIdx = m_usingWheelVariation[1] ? &m_varWheel[1]->m_VariationDrawableVarIdx : &m_varWheel[1]->m_VariationDrawableIdx;
		CCustomShaderEffectVehicleType** varWheelCseType = m_usingWheelVariation[1] ? &m_varWheel[1]->m_VariationDrawableVarCseType : &m_varWheel[1]->m_VariationDrawableCseType;

		if(*varWheelCseType)
		{
			if((*varWheelCseType)->GetRefCount()==1)
			{
				rmcDrawable *drawable = this->GetDrawable(VMT_WHEELS_REAR_OR_HYDRAULICS);
				if(drawable)
				{
					(*varWheelCseType)->RestoreModelInfoDrawable(drawable);
				}
				(*varWheelCseType)->RemoveRef();
				*varWheelCseType = NULL;
			}
			else
			{
				(*varWheelCseType)->RemoveRef();
			}
		}

		if(*varWheelDrawableIdx != -1)
		{
			g_DrawableStore.RemoveRef(strLocalIndex(*varWheelDrawableIdx), REF_OTHER);
			if(!(*varWheelCseType))
			{
				*varWheelDrawableIdx= -1;
			}
		}
	}
}

void CVehicleStreamRenderGfx::AddRefsToGfx()
{
    for (s32 i = 0; i < ms_renderRefs[ms_fenceUpdateIdx].GetCount(); ++i)
	{
        if (ms_renderRefs[ms_fenceUpdateIdx][i] == this)
            return;
	}

	for (int i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		strLocalIndex fragIdx = strLocalIndex(-1);
		if(i < VMT_RENDERABLE)
		{	// renderable mod:
			if(m_varModVisible[i])
			{
				fragIdx	= m_varModVisible[i]->m_VariationFragIdx;
			}
		}
		else
		{	// linked mod:
			if(m_varModLinked[i-VMT_RENDERABLE])
			{
				fragIdx = m_varModLinked[i-VMT_RENDERABLE]->m_VariationFragIdx;
			}
		}
		
		if (fragIdx != -1)
		{
			g_FragmentStore.AddRef(fragIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(fragIdx.Get(), &g_FragmentStore);
		}
	}

	if(m_varWheel[0])
	{
		strLocalIndex dwbIdx = m_usingWheelVariation[0] ? strLocalIndex(m_varWheel[0]->m_VariationDrawableVarIdx) : strLocalIndex(m_varWheel[0]->m_VariationDrawableIdx);
		if (dwbIdx != -1)
		{
			g_DrawableStore.AddRef(dwbIdx, REF_RENDER);
			gDrawListMgr->AddRefCountedModuleIndex(dwbIdx.Get(), &g_DrawableStore);
		}
	}

	AddRef();
	ms_renderRefs[ms_fenceUpdateIdx].Push(this);
}
#if __DEV
bool CVehicleStreamRenderGfx::ValidateStreamingIndexHasBeenCached() 
{
	bool indexInCache = true;

	if(m_varWheel[0])
	{
		s32 dwbIdx = m_usingWheelVariation[0] ? m_varWheel[0]->m_VariationDrawableVarIdx : m_varWheel[0]->m_VariationDrawableIdx;
		if (dwbIdx != -1)
		{
			//strIndex dwbStrIndex = g_DrawableStore.GetStreamingIndex(dwbIdx);
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(dwbIdx, &g_DrawableStore);
		}
	}

	for (int i = 0; indexInCache && i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		s32 fragIdx = -1;
		if(i < VMT_RENDERABLE)
		{	// renderable mod:
			if(m_varModVisible[i])
			{
				fragIdx	= m_varModVisible[i]->m_VariationFragIdx.Get();
			}
		}
		else
		{	// linked mod:
			if(m_varModLinked[i-VMT_RENDERABLE])
			{
				fragIdx = m_varModLinked[i-VMT_RENDERABLE]->m_VariationFragIdx.Get();
			}
		}

		if (fragIdx != -1)
		{
			//strIndex fragStrIndex = g_FragmentStore.GetStreamingIndex(fragIdx);
			indexInCache = gDrawListMgr->ValidateModuleIndexHasBeenCached(fragIdx, &g_FragmentStore);
		}
	}

	return indexInCache;
}
#endif

void CVehicleStreamRenderGfx::Init()
{
	for (int i = 0; i < FENCE_COUNT; ++i)
	{
		ms_renderRefs[i].Reserve(CVehicleVariationInstance::ms_MaxVehicleStreamRender);
	}
}

void CVehicleStreamRenderGfx::Shutdown()
{
	for (int i = 0; i < FENCE_COUNT; ++i)
	{
		ms_renderRefs[i].Reset();
	}
}

void CVehicleStreamRenderGfx::SetUsed(bool val)
{
	m_used = val;

	if (!m_used && !m_refCount)
		delete this;
}

void CVehicleStreamRenderGfx::RemoveRefs()
{
	s32 newUpdateIdx = (ms_fenceUpdateIdx + 1) % FENCE_COUNT;

	for (s32 i = 0; i < ms_renderRefs[newUpdateIdx].GetCount(); ++i)
	{
		CVehicleStreamRenderGfx* gfx = ms_renderRefs[newUpdateIdx][i];
		if (!gfx)
			continue;

		gfx->m_refCount--;
	}

	ms_renderRefs[newUpdateIdx].Resize(0);
	ms_fenceUpdateIdx = newUpdateIdx;

	// free unused entries
	CVehicleStreamRenderGfx::Pool* pool = CVehicleStreamRenderGfx::GetPool();
	if (pool)
	{
		for (s32 i = 0; i < pool->GetSize(); ++i)
		{
			CVehicleStreamRenderGfx* gfx = pool->GetSlot(i);
			if (!gfx)
				continue;

			if (!gfx->m_used && !gfx->m_refCount)
				delete gfx;
		}
	}
}

void CVehicleStreamRenderGfx::CopyMatrices(const CVehicleStreamRenderGfx& rOther)
{
	for (u8 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		m_boneMatrices[i] = rOther.m_boneMatrices[i];
	}
}

u8 CVehicleStreamRenderGfx::GetExhaustModIndex(eHierarchyId id) const
{
	if (id < VEH_FIRST_EXHAUST || id > VEH_LAST_EXHAUST)
		return INVALID_MOD;

	Assert(id - VEH_FIRST_EXHAUST < NUM_EXHAUST_BONES);
	return m_exhaustBones[id - VEH_FIRST_EXHAUST].m_modIndex;
}

s8 CVehicleStreamRenderGfx::GetExhaustBoneIndex(eHierarchyId id) const
{
	if (id < VEH_FIRST_EXHAUST || id > VEH_LAST_EXHAUST)
		return -1;

	Assert(id - VEH_FIRST_EXHAUST < NUM_EXHAUST_BONES);
	return m_exhaustBones[id - VEH_FIRST_EXHAUST].m_boneIndex;
}

u8 CVehicleStreamRenderGfx::GetExtraLightModIndex(eHierarchyId id) const
{
	if (id < VEH_EXTRALIGHT_1 || id > VEH_EXTRALIGHT_4)
		return INVALID_MOD;

	Assert(id - VEH_EXTRALIGHT_1 < NUM_EXTRALIGHT_BONES);
	return m_extraLightBones[id - VEH_EXTRALIGHT_1].m_modIndex;
}

s8 CVehicleStreamRenderGfx::GetExtraLightBoneIndex(eHierarchyId id) const
{
	if (id < VEH_EXTRALIGHT_1 || id > VEH_EXTRALIGHT_4)
		return -1;

	Assert(id - VEH_EXTRALIGHT_1 < NUM_EXTRALIGHT_BONES);
	return m_extraLightBones[id - VEH_EXTRALIGHT_1].m_boneIndex;
}

u8 CVehicleStreamRenderGfx::GetExtraLightCount() const
{
    u8 count = 0;
    for (s32 i = 0; i < NUM_EXTRALIGHT_BONES; ++i)
        if (m_extraLightBones[i].m_boneIndex != -1)
            count++;

    return count;
}

fragType* CVehicleStreamRenderGfx::GetFrag(u8 modSlot) const
{ 
	if(!Verifyf(modSlot < VMT_RENDERABLE + MAX_LINKED_MODS, "Invalid modSlot (%u)", modSlot))
	{
		return NULL;
	}

	strLocalIndex fragidx = strLocalIndex(-1);
	if(modSlot < VMT_RENDERABLE)
	{
		if(m_varModVisible[modSlot])
		{
			fragidx = m_varModVisible[modSlot]->m_VariationFragIdx;
		}
	}
	else
	{
		if(m_varModLinked[modSlot-VMT_RENDERABLE])
		{
			fragidx = m_varModLinked[modSlot-VMT_RENDERABLE]->m_VariationFragIdx;
		}
	}

	if (fragidx.IsValid())
	{
		return g_FragmentStore.Get(fragidx); 
	}

	return NULL;
}

CCustomShaderEffectVehicleType*	CVehicleStreamRenderGfx::GetFragCSEType(u8 modSlot) const
{
	if(!Verifyf(modSlot < VMT_RENDERABLE + MAX_LINKED_MODS, "Invalid modSlot (%u)", modSlot))
	{
		return NULL;
	}

	CCustomShaderEffectVehicleType *type=NULL;
	if(modSlot < VMT_RENDERABLE)
	{
		if(m_varModVisible[modSlot])
		{
			type = m_varModVisible[modSlot]->m_VariationFragCseType;
		}
	}
	else
	{
		if(m_varModLinked[modSlot-VMT_RENDERABLE])
		{
			type = m_varModLinked[modSlot-VMT_RENDERABLE]->m_VariationFragCseType;
		}
	}

	return type;
}

strLocalIndex CVehicleStreamRenderGfx::GetFragIndex(u8 modSlot) const
{
	if(!Verifyf(modSlot < VMT_RENDERABLE + MAX_LINKED_MODS, "Invalid modSlot (%u)", modSlot))
	{
		return strLocalIndex(-1);
	}

	s32 fragidx =-1;
	if(modSlot < VMT_RENDERABLE)
	{
		if(m_varModVisible[modSlot])
		{
			fragidx = m_varModVisible[modSlot]->m_VariationFragIdx.Get();
		}
	}
	else
	{
		if(m_varModLinked[modSlot-VMT_RENDERABLE])
		{
			fragidx = m_varModLinked[modSlot-VMT_RENDERABLE]->m_VariationFragIdx.Get();
		}
	}

	return strLocalIndex(fragidx);
}

rmcDrawable* CVehicleStreamRenderGfx::GetDrawable(eVehicleModType modSlot) const
{ 
	Assert(modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS); 

	CVehicleWheel *wheel = m_varWheel[modSlot-VMT_WHEELS];
	if(wheel)
	{
		strLocalIndex drawableIdx = m_usingWheelVariation[modSlot-VMT_WHEELS] ? strLocalIndex(wheel->m_VariationDrawableVarIdx) : strLocalIndex(wheel->m_VariationDrawableIdx);
		if (drawableIdx.IsValid())
		{
			return g_DrawableStore.Get(drawableIdx); 
		}
	}

	return NULL;
}

CCustomShaderEffectVehicleType*	CVehicleStreamRenderGfx::GetDrawableCSEType(eVehicleModType modSlot) const
{
	Assert(modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS); 

	CVehicleWheel *wheel = m_varWheel[modSlot-VMT_WHEELS];
	if(wheel)
	{
		return m_usingWheelVariation[modSlot-VMT_WHEELS] ? wheel->m_VariationDrawableVarCseType : wheel->m_VariationDrawableCseType;
	}

	return NULL;
}

strLocalIndex CVehicleStreamRenderGfx::GetDrawableIndex(eVehicleModType modSlot) const
{
	Assert(modSlot == VMT_WHEELS || modSlot == VMT_WHEELS_REAR_OR_HYDRAULICS); 

	CVehicleWheel *wheel = m_varWheel[modSlot-VMT_WHEELS];
	if (wheel)
	{
		return m_usingWheelVariation[modSlot-VMT_WHEELS] ? strLocalIndex(wheel->m_VariationDrawableVarIdx) : strLocalIndex(wheel->m_VariationDrawableIdx);
	}

	return strLocalIndex(-1);
}

static float sfMinTyreThickness = 0.05f;
// overrides the rim radius on each wheel with the custom one from our mod wheel
void CVehicleStreamRenderGfx::OverrideWheelRimRadius(CVehicle* targetEntity) const
{
	CVehicleStructure* vehStruct = targetEntity->GetVehicleModelInfo()->GetStructure();

	CVehicleModelInfo* vmi = targetEntity->GetVehicleModelInfo();
	const CVehicleVariationInstance& var = targetEntity->GetVariationInstance();
	eVehicleWheelType wheelType = var.GetWheelType(targetEntity);
	float frontRimRadius = 0.f;
	float rearRimRadius = 0.f;
	bool overrideFrontRadius = false;
	bool overrideRearRadius = false;

	if (var.HasCustomWheels())
    {
        u32 modEntry = var.GetMods()[VMT_WHEELS];
        const CVehicleWheel& modWheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][modEntry];
		frontRimRadius = modWheel.GetRimRadius() * m_wheelTyreRadiusScale[0];
		frontRimRadius = Min(frontRimRadius, vmi->GetTyreRadius(true) - sfMinTyreThickness);
		overrideFrontRadius = true;

		// infernus2 has custom scale on rear wheel:
		if(targetEntity->GetModelNameHash() == MID_INFERNUS2)
		{
			rearRimRadius = modWheel.GetRimRadius() * m_wheelTyreRadiusScale[1];
			overrideRearRadius = true;
		}
		else
		{
			rearRimRadius = frontRimRadius * vmi->GetTyreRadius(false) / vmi->GetTyreRadius(true);
		}
		
		rearRimRadius = Min(rearRimRadius, vmi->GetTyreRadius(false) - sfMinTyreThickness);
    }

	// override calculated rear wheel radius with a specific one, if we have custom rear wheels
	if (var.HasCustomRearWheels())
	{
		u32 modEntry = var.GetMods()[VMT_WHEELS_REAR_OR_HYDRAULICS];
		const CVehicleWheel& modWheel = CVehicleModelInfo::GetVehicleColours()->m_Wheels[wheelType][modEntry];
		rearRimRadius = modWheel.GetRimRadius();

		rearRimRadius *= m_wheelTyreRadiusScale[1]; // scale to	vehicle space

		rearRimRadius = Min(rearRimRadius, vmi->GetTyreRadius(false) - sfMinTyreThickness);
		overrideRearRadius = true;
	}

	// override the rim radius for each wheel
	s32 numWheels = targetEntity->GetNumWheels();
	for (s32 i = 0; i < numWheels; ++i)
	{
		CWheel* wheel = targetEntity->GetWheel(i);
		if (wheel)
		{
			if (wheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL))
			{
				if ( ( !var.CanHaveRearWheels() && overrideFrontRadius ) || overrideRearRadius)
					wheel->SetWheelRimRadius(rearRimRadius);
				else
					rearRimRadius = wheel->GetWheelRimRadius();
			}
			else 
			{
				if (overrideFrontRadius)
					wheel->SetWheelRimRadius(frontRimRadius);
				else
					frontRimRadius = wheel->GetWheelRimRadius();
			}
		}
	}

	// override the rim radius for each wheel instance too
	for (s32 i = 0; i < NUM_VEH_CWHEELS_MAX; ++i)
	{
		const bool isFrontWheel = CVehicleModelInfo::ms_aSetupWheelIds[i] == VEH_WHEEL_LF || CVehicleModelInfo::ms_aSetupWheelIds[i] == VEH_WHEEL_RF;
		vehStruct->m_fWheelRimRadius[i] = isFrontWheel ? frontRimRadius : rearRimRadius;
	}
}

// restores the wheel rim radius to the one initially stored in the model info (done when mod wheel is removed and replaced with original one)
void CVehicleStreamRenderGfx::RestoreWheelRimRadius(CVehicle* targetEntity) const
{
	CVehicleStructure* vehStruct = targetEntity->GetVehicleModelInfo()->GetStructure();
	CVehicleModelInfo* vmi = targetEntity->GetVehicleModelInfo();

	s32 numWheels = targetEntity->GetNumWheels();
	for (s32 i = 0; i < numWheels; ++i)
	{
		CWheel* wheel = targetEntity->GetWheel(i);
		wheel->SetWheelRimRadius(wheel->GetConfigFlags().IsFlagSet(WCF_REARWHEEL) ? vmi->GetRimRadius(false) : vmi->GetRimRadius(true));
	}

	for (s32 i = 0; i < NUM_VEH_CWHEELS_MAX; ++i)
	{
		const bool isFrontWheel = CVehicleModelInfo::ms_aSetupWheelIds[i] == VEH_WHEEL_LF || CVehicleModelInfo::ms_aSetupWheelIds[i] == VEH_WHEEL_RF;
		vehStruct->m_fWheelRimRadius[i] = isFrontWheel ? vmi->GetRimRadius(true) : vmi->GetRimRadius(false);
	}
}

void CVehicleStreamRenderGfx::StoreModMatrices(CVehicle* pVeh)
{
    CVehicleVariationInstance& var = pVeh->GetVariationInstance();
	for (u8 i = 0; i < VMT_RENDERABLE + MAX_LINKED_MODS; ++i)
	{
		if (var.HasModMatrixBeenStoredThisFrame(i))
			continue;

		s32 bone = -1;
		if (i < VMT_RENDERABLE)
		{
			if (var.GetMods()[i] == INVALID_MOD)
				continue;

			bone = var.GetBone(i);
		}
		else
		{
			u8 linkSlot = i - VMT_RENDERABLE;
			if (linkSlot < MAX_LINKED_MODS)
			{
				bone = var.GetBoneForLinkedMod(linkSlot);
			}
		}

		if (bone == -1)
			continue;

		int boneIdx = pVeh->GetBoneIndex((eHierarchyId)bone);
		if (Verifyf(boneIdx != -1, "Invalid bone index in vehicle variation instance on slot %i, bone %d (%s)", i, bone, pVeh->GetModelName()))
		{
			if (Verifyf(pVeh->GetSkeleton(), "Vehicle instance %p of type %s has no skeleton!", pVeh, pVeh->GetModelName()))
			{
				Matrix34 boneMat;
				Matrix33 rotMat;
				pVeh->GetSkeleton()->GetGlobalMtx(boneIdx, RC_MAT34V(boneMat));
				rotMat = boneMat;
				SetMatrix(i, rotMat);

				var.SetModMatrixStoredThisFrame(i);
			}
		}
	}
}

#if __DEV
void CVehicleStreamRenderGfx::PoolFullCallback(void* pItem)
{
	if (!pItem)
	{
		Printf("ERROR - CVehicleStreamRenderGfx Pool FULL!\n");
	}
	else
	{
		CVehicleStreamRenderGfx* pVehRenderGfx =  static_cast<CVehicleStreamRenderGfx*>(pItem);
		Displayf("CVehicleStreamRenderGfx* : %p", pVehRenderGfx);

		CVehicle* pVehicle = pVehRenderGfx->m_targetEntity;
		if (pVehicle){
			CVehicle::PoolFullCallback(pVehicle);
		}

		Displayf("---");
	}
}
#endif
