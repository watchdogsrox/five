///////////////////////////////////////////////////////////////////////////////
//  
//	FILE:	GtaMaterialManager.cpp
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	04 October 2006
// 
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "GtaMaterialManager.h"
#include "gtaMaterialManager_parser.h"

// game
#include "Debug/Debug.h"

#include "parser/manager.h"

#if __PS3
#include "phBullet/CollisionWorkUnit.h"
#endif



///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

VFX_SYSTEM_OPTIMISATIONS()


///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// CLASS phMaterialMgrGta
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
// Create
///////////////////////////////////////////////////////////////////////////////


phMaterialMgrGta* phMaterialMgrGta::Create()
{
	phMaterialMgrGta* instance = rage_new phMaterialMgrGta;
	SetInstance(instance);
	return instance;
}


///////////////////////////////////////////////////////////////////////////////
// PreLoadGtaMtlFile
///////////////////////////////////////////////////////////////////////////////

s32 phMaterialMgrGta::PreLoadGtaMtlFile(const char* name)
{
	s32 reservedSlots = 0;

	ASSET.PushFolder("commoncrc:/data/materials");
	fiStream* stream = ASSET.Open(name, "dat", true);
	Assertf(stream, "Could not load common:/data/materials/%s.dat", name);
	ASSET.PopFolder();

	if (stream)
	{
		fiAsciiTokenizer token;
		token.Init(name, stream);

		// read in the verison number line
		char lineBuff[1024];
		token.GetLine(lineBuff, 1024);

		// count the number of materials
		while (token.GetLine(lineBuff, 1024) > 0)
		{
			if (lineBuff[0] != '#')
			{
				++reservedSlots;
			}
		}

		stream->Close();
	}

	return reservedSlots;
}


///////////////////////////////////////////////////////////////////////////////
// LoadGtaMtlFile
///////////////////////////////////////////////////////////////////////////////

void phMaterialMgrGta::LoadGtaMtlFile		(const char* name)
{
	LoadRumbleProfiles();

	ASSET.PushFolder("common:/data/materials");
	fiStream* stream = ASSET.Open(name, "dat", true);
	Assertf(stream, "Could not load common:/data/materials/%s.dat", name);
	ASSET.PopFolder();

	if (stream)
	{
		fiAsciiTokenizer token;
		token.Init(name, stream);

		// read in the verison number line
		char lineBuff[1024];
		token.GetLine(lineBuff, 1024);
		sscanf(lineBuff, "%f", &m_version);

#if __ASSERT
		int numMaterials = 0;
#endif
		while (token.GetLine(lineBuff, 1024) > 0)
		{
			if (lineBuff[0] != '#')
			{
				char materialName[phMaterial::MAX_NAME_LENGTH];
				sscanf(lineBuff, "%s", materialName);

				phMaterial& material = AllocateMaterial(materialName);
				Assert(material.GetClassType() == GTA_MATERIAL);
				phMaterialGta& gtaMaterial = (phMaterialGta&)material;
				gtaMaterial.LoadGtaMtlData(&lineBuff[0], m_version);

#if __ASSERT
				numMaterials++;
				Assertf(numMaterials<256, "trying to load more than 256 materials");
#endif
			}
		}

		stream->Close();
	}
}

///////////////////////////////////////////////////////////////////////////////
// LoadGtaMtlOverridePairFile
///////////////////////////////////////////////////////////////////////////////

void phMaterialMgrGta::LoadGtaMtlOverridePairFile(const char* name)
{
	// Count the number of material pair overrides
	m_MaterialOverridePairs.Reserve(PreLoadGtaMtlFile(name));

	ASSET.PushFolder("common:/data/materials");
	fiStream* stream = ASSET.Open(name, "dat", true);
	Assertf(stream, "Could not load common:/data/materials/%s.dat", name);
	ASSET.PopFolder();

	if (stream)
	{
		fiAsciiTokenizer token;
		token.Init(name, stream);

		char lineBuff[1024];
		while (token.GetLine(lineBuff, 1024) > 0)
		{
			if (lineBuff[0] != '#')
			{
				char materialNameA[phMaterial::MAX_NAME_LENGTH];
				char materialNameB[phMaterial::MAX_NAME_LENGTH];
				float combinedFriction;
				float combinedElasticity;

				// Load the information about the material override pair
				sscanf(lineBuff, "%s %s %f %f", materialNameA, materialNameB, &combinedFriction, &combinedElasticity);

				Id materialIdA = FindMaterialId(materialNameA);
				Id materialIdB = FindMaterialId(materialNameB);

				// Don't allow overriding of the default material
				if(	Verifyf(materialIdA != DEFAULT_MATERIAL_ID, "Could not locate material type %s.", materialNameA) &&
					Verifyf(materialIdB != DEFAULT_MATERIAL_ID, "Could not locate material type %s.", materialNameB) )
				{
					phMaterialPair& materialPair = m_MaterialOverridePairs.Append();
					materialPair.m_CombinedFriction = combinedFriction;
					materialPair.m_CombinedElasticity = combinedElasticity;
					materialPair.m_MaterialIndexA = materialIdA & m_MaterialIndexMask;
					materialPair.m_MaterialIndexB = materialIdB & m_MaterialIndexMask;

					// Material index A needs to be less than material index B to save comparisons
					if(materialPair.m_MaterialIndexA > materialPair.m_MaterialIndexB)
						SwapEm(materialPair.m_MaterialIndexA, materialPair.m_MaterialIndexB);

					// Values of exactly -1 means that the default combination of the 2 materials should be used
					if(materialPair.m_CombinedFriction == -1.0f)
					{
						materialPair.m_CombinedFriction = CombineFriction(GetMaterial(materialIdA).GetFriction(), GetMaterial(materialIdB).GetFriction());
					}
					else
					{
						Assertf(materialPair.m_CombinedFriction >= 0, "Material pair override friction (%f) should be positive. (%s - %s)", materialPair.m_CombinedFriction, materialNameA, materialNameB);
					}

					if(materialPair.m_CombinedElasticity == -1.0f)
					{
						materialPair.m_CombinedElasticity = CombineElasticity(GetMaterial(materialIdA).GetElasticity(), GetMaterial(materialIdB).GetElasticity());
					}
					else
					{
						Assertf(materialPair.m_CombinedElasticity >= PHMATERIALMGR_MIN_ELASTICITY, "Material pair override elasticity (%f) is less than the minimum elasticity (%f). (%s - %s)", materialPair.m_CombinedElasticity, PHMATERIALMGR_MIN_ELASTICITY, materialNameA, materialNameB);
						Assertf(materialPair.m_CombinedElasticity <= PHMATERIALMGR_MAX_ELASTICITY, "Material pair override elasticity (%f) is greater than the maximum elasticity (%f). (%s - %s)", materialPair.m_CombinedElasticity, PHMATERIALMGR_MAX_ELASTICITY, materialNameA, materialNameB);
					}
				}
			}
		}

		stream->Close();
	}
}


///////////////////////////////////////////////////////////////////////////////
// Load
///////////////////////////////////////////////////////////////////////////////

void phMaterialMgrGta::Load(int reservedSlots)
{
	// preload the material data
	reservedSlots += PreLoadGtaMtlFile("materials");

	// make room for the materials in the containers
	Assert(reservedSlots < 0xffff);
	m_map.Recompute(atHashNextSize(static_cast<u16>(reservedSlots)));
	m_array.Reserve(reservedSlots);

	// load in the material data
	LoadGtaMtlFile("materials");

	// cache some material ids

	// materials used in the heli code to override high up default material as concrete
	g_idDefault						= PGTAMATERIALMGR->FindMaterialId("DEFAULT");
	g_idConcrete					= PGTAMATERIALMGR->FindMaterialId("CONCRETE");

	// road materials - other road materials that don't have a tarmac vfx group
	g_idTarmac						= PGTAMATERIALMGR->FindMaterialId("TARMAC");

	// vehicle materials - used in the probe code when processing vehicle wheels
	g_idRubber						= PGTAMATERIALMGR->FindMaterialId("RUBBER");
	g_idCarVoid						= PGTAMATERIALMGR->FindMaterialId("PHYS_CAR_VOID");
	g_idCarMetal					= PGTAMATERIALMGR->FindMaterialId("CAR_METAL");
	g_idCarEngine					= PGTAMATERIALMGR->FindMaterialId("CAR_ENGINE");
	g_idTrainTrack					= PGTAMATERIALMGR->FindMaterialId("metal_solid_small");

	// clear materials - need to have decals applied in the forward render pass
	g_idCarSoftTopClear				= PGTAMATERIALMGR->FindMaterialId("CAR_SOFTTOP_CLEAR");
	g_idPlasticClear				= PGTAMATERIALMGR->FindMaterialId("PLASTIC_CLEAR");
	g_idPlasticHollowClear			= PGTAMATERIALMGR->FindMaterialId("PLASTIC_HOLLOW_CLEAR");
	g_idPlasticHighDensityClear		= PGTAMATERIALMGR->FindMaterialId("PLASTIC_HIGH_DENSITY_CLEAR");

	// low friction material for the ped's physics capsule
	g_idPhysPedCapsule				= PGTAMATERIALMGR->FindMaterialId("PHYS_PED_CAPSULE");

	// NM materials
	g_idButtocks					= PGTAMATERIALMGR->FindMaterialId("BUTTOCKS");
	g_idThighLeft					= PGTAMATERIALMGR->FindMaterialId("THIGH_LEFT");
	g_idShinLeft					= PGTAMATERIALMGR->FindMaterialId("SHIN_LEFT");
	g_idFootLeft					= PGTAMATERIALMGR->FindMaterialId("FOOT_LEFT");
	g_idThighRight					= PGTAMATERIALMGR->FindMaterialId("THIGH_RIGHT");
	g_idShinRight					= PGTAMATERIALMGR->FindMaterialId("SHIN_RIGHT");
	g_idFootRight					= PGTAMATERIALMGR->FindMaterialId("FOOT_RIGHT");
	g_idSpine0						= PGTAMATERIALMGR->FindMaterialId("SPINE0");
	g_idSpine1						= PGTAMATERIALMGR->FindMaterialId("SPINE1");
	g_idSpine2						= PGTAMATERIALMGR->FindMaterialId("SPINE2");
	g_idSpine3						= PGTAMATERIALMGR->FindMaterialId("SPINE3");
	g_idClavicleLeft				= PGTAMATERIALMGR->FindMaterialId("CLAVICLE_LEFT");
	g_idUpperArmLeft				= PGTAMATERIALMGR->FindMaterialId("UPPER_ARM_LEFT");
	g_idLowerArmLeft				= PGTAMATERIALMGR->FindMaterialId("LOWER_ARM_LEFT");
	g_idHandLeft					= PGTAMATERIALMGR->FindMaterialId("HAND_LEFT");
	g_idClavicleRight				= PGTAMATERIALMGR->FindMaterialId("CLAVICLE_RIGHT");
	g_idUpperArmRight				= PGTAMATERIALMGR->FindMaterialId("UPPER_ARM_RIGHT");
	g_idLowerArmRight				= PGTAMATERIALMGR->FindMaterialId("LOWER_ARM_RIGHT");
	g_idHandRight					= PGTAMATERIALMGR->FindMaterialId("HAND_RIGHT");
	g_idNeck						= PGTAMATERIALMGR->FindMaterialId("NECK");
	g_idHead						= PGTAMATERIALMGR->FindMaterialId("HEAD");
	g_idAnimalDefault				= PGTAMATERIALMGR->FindMaterialId("ANIMAL_DEFAULT");

	// new materials
	g_idConcretePavement			= PGTAMATERIALMGR->FindMaterialId("CONCRETE_PAVEMENT");
	g_idBrickPavement				= PGTAMATERIALMGR->FindMaterialId("BRICK_PAVEMENT");

	// Special material used to identify frag bounds used only for cover shape tests and disabled when the frag breaks.
	g_idPhysDynamicCoverBound		= PGTAMATERIALMGR->FindMaterialId("PHYS_DYNAMIC_COVER_BOUND");

	// glass materials - used in the smashable code
	g_idGlassShootThrough			= PGTAMATERIALMGR->FindMaterialId("GLASS_SHOOT_THROUGH");
	g_idGlassBulletproof			= PGTAMATERIALMGR->FindMaterialId("GLASS_BULLETPROOF");
	g_idGlassOpaque					= PGTAMATERIALMGR->FindMaterialId("GLASS_OPAQUE");

	g_idCarGlassWeak				= PGTAMATERIALMGR->FindMaterialId("CAR_GLASS_WEAK");
	g_idCarGlassMedium				= PGTAMATERIALMGR->FindMaterialId("CAR_GLASS_MEDIUM");
	g_idCarGlassStrong				= PGTAMATERIALMGR->FindMaterialId("CAR_GLASS_STRONG");
	g_idCarGlassBulletproof			= PGTAMATERIALMGR->FindMaterialId("CAR_GLASS_BULLETPROOF");
	g_idCarGlassOpaque				= PGTAMATERIALMGR->FindMaterialId("CAR_GLASS_OPAQUE");

	// emissive materials - used in the decal code (can we have an is emissive flag instead?)
	g_idEmissiveGlass				= PGTAMATERIALMGR->FindMaterialId("EMISSIVE_GLASS");
	g_idEmissivePlastic				= PGTAMATERIALMGR->FindMaterialId("EMISSIVE_PLASTIC");
	g_idTreeBark					= PGTAMATERIALMGR->FindMaterialId("TREE_BARK");

	// deep surface materials
	g_idSnowDeep					= PGTAMATERIALMGR->FindMaterialId("SNOW_DEEP");
	g_idMarshDeep					= PGTAMATERIALMGR->FindMaterialId("MARSH_DEEP");
	g_idSandDryDeep					= PGTAMATERIALMGR->FindMaterialId("SAND_DRY_DEEP");
	g_idSandWetDeep					= PGTAMATERIALMGR->FindMaterialId("SAND_WET_DEEP");

	// cached material ids - special online vehicle materials
	g_idPhysVehicleSpeedUp			= PGTAMATERIALMGR->FindMaterialId("TEMP_01");
	g_idPhysVehicleSlowDown			= PGTAMATERIALMGR->FindMaterialId("TEMP_02");
	g_idPhysVehicleRefill			= PGTAMATERIALMGR->FindMaterialId("TEMP_03");
	g_idPhysVehicleBoostCancel		= PGTAMATERIALMGR->FindMaterialId( "TEMP_07" );
	g_idPhysVehicleTyreBurst		= PGTAMATERIALMGR->FindMaterialId( "TEMP_08" );
	g_idPhysPropPlacement			= PGTAMATERIALMGR->FindMaterialId( "TEMP_09" );

	// cached material ids - stunt races
	g_idStuntRampSurface			= PGTAMATERIALMGR->FindMaterialId("STUNT_RAMP_SURFACE");
	g_idStuntTargetA				= PGTAMATERIALMGR->FindMaterialId("TEMP_04");
	g_idStuntTargetB				= PGTAMATERIALMGR->FindMaterialId("TEMP_05");
	g_idStuntTargetC				= PGTAMATERIALMGR->FindMaterialId("TEMP_06");

	SetMaterialInformation(m_array.GetElements(), GetMaterialMask());

	LoadGtaMtlOverridePairFile("material_override_pairs");	

	Assert(g_idEmissiveGlass>0);
	Assert(g_idEmissivePlastic>0);
}


///////////////////////////////////////////////////////////////////////////////
// Destroy
///////////////////////////////////////////////////////////////////////////////

void phMaterialMgrGta::Destroy()
{
	delete this;																// remember, you can't access members after this point!
	SetInstance(NULL);
}


///////////////////////////////////////////////////////////////////////////////
// AllocateMaterial
///////////////////////////////////////////////////////////////////////////////

phMaterial& phMaterialMgrGta::AllocateMaterial(const char* name)
{
	// the material itself is stored in the array
	phMaterialGta& material = m_array.Append();
	m_NumMaterials = m_array.GetCount();

	// the map element is added for quick searching
	char normalized[phMaterial::MAX_NAME_LENGTH];
	StringNormalize(normalized, name, sizeof(normalized));

	// the name stored in the material is actually owned by the map
	material.SetName(m_map.Insert(ConstString(normalized), &material).key.m_String);

	return material;
}

///////////////////////////////////////////////////////////////////////////////
// Constructor
///////////////////////////////////////////////////////////////////////////////

phMaterialMgrGta::phMaterialMgrGta()
: phMaterialMgr()
{
}

///////////////////////////////////////////////////////////////////////////////
// Destructor
///////////////////////////////////////////////////////////////////////////////

phMaterialMgrGta::~phMaterialMgrGta()
{
	// both the containers are automatically Killed in their destructors
}

///////////////////////////////////////////////////////////////////////////////
// FindMaterial
///////////////////////////////////////////////////////////////////////////////

const phMaterial& phMaterialMgrGta::FindMaterial(const char* packedName) const
{
	char mtlName[64];
	UnpackMtlName(packedName, mtlName);

	char normalized[phMaterial::MAX_NAME_LENGTH];
	StringNormalize(normalized, mtlName, sizeof(normalized));

	phMaterialGta* const* pMaterial = m_map.Access(normalized);

	if(pMaterial==NULL || *pMaterial==NULL)
		return GetDefaultMaterial();

	//	Assert(pMaterial);
	//	Assert(*pMaterial);
	return **pMaterial;
}


///////////////////////////////////////////////////////////////////////////////
// GetMaterialId
///////////////////////////////////////////////////////////////////////////////

phMaterialMgr::Id phMaterialMgrGta::GetMaterialId(const phMaterial& material) const
{
	int id = ptrdiff_t_to_int(&static_cast<const phMaterialGta&>(material) - &m_array[0]);

	Assert(id >= 0 && id < m_array.GetCount());
	return id;
}


///////////////////////////////////////////////////////////////////////////////
// GetMaterialId
///////////////////////////////////////////////////////////////////////////////
#if __PS3
void phMaterialMgrGta::FillInSpuWorkUnit(PairListWorkUnitInput& wuInput)
{
	wuInput.m_materialArray = &m_array.GetElements()[0];
	wuInput.m_numMaterials = m_array.GetCount();
	wuInput.m_materialStride = u32(sizeof(phMaterialGta));
	wuInput.m_materialMask = 0xff;
	phMaterialMgr::FillInSpuWorkUnit(wuInput);
}
#endif



///////////////////////////////////////////////////////////////////////////////
// UnpackMtlName
///////////////////////////////////////////////////////////////////////////////

void phMaterialMgrGta::UnpackMtlName(const char* packedName, char* mtlName) const
{
	s32 currTargetIndex = 0;
	for (u32 i=0; i<strlen(packedName); i++)
	{
		if (packedName[i]=='|')
		{
			break;
		}

		mtlName[currTargetIndex++] = packedName[i];
	}

	mtlName[currTargetIndex] = '\0';
}


///////////////////////////////////////////////////////////////////////////////
// UnpackProcName
///////////////////////////////////////////////////////////////////////////////

void phMaterialMgrGta::UnpackProcName(const char* packedName, char* procName) const
{
	s32 currTargetIndex = 0;
	s32 numDelimitersFound = 0;
	for (u32 i=0; i<strlen(packedName); i++)
	{
		if (packedName[i]=='|')
		{
			numDelimitersFound++;
			continue;
		}

		if (numDelimitersFound==1)
		{
			procName[currTargetIndex++] = packedName[i];
		}
	}

	Assert(numDelimitersFound>=1);

	procName[currTargetIndex] = '\0';
}


///////////////////////////////////////////////////////////////////////////////
// UnpackRoomId
///////////////////////////////////////////////////////////////////////////////

s32 phMaterialMgrGta::UnpackRoomId(const char* packedName) const
{
	char tempBuff[64];
	s32 currTargetIndex = 0;
	s32 numDelimitersFound = 0;
	for (u32 i=0; i<strlen(packedName); i++)
	{
		if (packedName[i]=='|')
		{
			numDelimitersFound++;
			continue;
		}

		if (numDelimitersFound==2)
		{
			tempBuff[currTargetIndex++] = packedName[i];
		}
	}

	Assert(numDelimitersFound>=2);

	tempBuff[currTargetIndex] = '\0';

	return atoi(tempBuff);
}


///////////////////////////////////////////////////////////////////////////////
// UnpackPedDensity
///////////////////////////////////////////////////////////////////////////////

s32 phMaterialMgrGta::UnpackPedDensity(const char* packedName) const
{
	char tempBuff[64];
	s32 currTargetIndex = 0;
	s32 numDelimitersFound = 0;
	for (u32 i=0; i<strlen(packedName); i++)
	{
		if (packedName[i]=='|')
		{
			numDelimitersFound++;
			continue;
		}

		if (numDelimitersFound==3)
		{
			tempBuff[currTargetIndex++] = packedName[i];
		}
	}

	Assert(numDelimitersFound>=3);

	tempBuff[currTargetIndex] = '\0';

	return atoi(tempBuff);
}


///////////////////////////////////////////////////////////////////////////////
// UnpackPolyFlags
///////////////////////////////////////////////////////////////////////////////

s32 phMaterialMgrGta::UnpackPolyFlags(const char* packedName) const
{
	char tempBuff[64];
	s32 currTargetIndex = 0;
	s32 numDelimitersFound = 0;
	for (u32 i=0; i<strlen(packedName); i++)
	{
		if (packedName[i]=='|')
		{
			numDelimitersFound++;
			continue;
		}

		if (numDelimitersFound==4)
		{
			tempBuff[currTargetIndex++] = packedName[i];
		}
	}

	Assert(numDelimitersFound>=4);

	tempBuff[currTargetIndex] = '\0';

	return atoi(tempBuff);
}


///////////////////////////////////////////////////////////////////////////////
// FindMaterialIdImpl
///////////////////////////////////////////////////////////////////////////////

phMaterialMgr::Id phMaterialMgrGta::FindMaterialIdImpl(const char* mtlName) const
{
	char normalized[phMaterial::MAX_NAME_LENGTH];
	StringNormalize(normalized, mtlName, sizeof(normalized));

	phMaterialGta* const* pMaterial = m_map.Access(normalized);

#if __XENON	|| __PPU // wierd ass fix for now... why are Xenon materials shagged?
	if (pMaterial == NULL || *pMaterial == NULL)
	{
		StringNormalize(normalized, "default", sizeof(normalized));
		pMaterial = m_map.Access(normalized);
	}
#endif

	//	Assert(pMaterial);
	if(pMaterial == NULL)
		return DEFAULT_MATERIAL_ID;
	Assert(*pMaterial);
	return GetMaterialId(**pMaterial);
}


///////////////////////////////////////////////////////////////////////////////
// GetDefaultMaterial
///////////////////////////////////////////////////////////////////////////////

const phMaterial& phMaterialMgrGta::GetDefaultMaterial() const
{
	return GetMaterial(FindMaterialId("default"));
}


///////////////////////////////////////////////////////////////////////////////
// GetMtlRumbleProfile
///////////////////////////////////////////////////////////////////////////////

phRumbleProfile* phMaterialMgrGta::GetMtlRumbleProfile(Id mtlId)
{
	s32 profileIndex = ((phMaterialGta*)&GetMaterial(mtlId))->GetRumbleProfileIndex();

	if (profileIndex >= 0 && profileIndex < m_rumbleProfileList.m_rumbleProfiles.GetCount())
		return &m_rumbleProfileList.m_rumbleProfiles[profileIndex];

	return NULL;
}


///////////////////////////////////////////////////////////////////////////////
// GetPackedPolyFlagValue
///////////////////////////////////////////////////////////////////////////////

// NOTE: This must line up with /framework/tools/src/cli/ragebuilder/gtamaterialmgr.h GtaMaterialMgr::GetPackedPolyFlagValue
phMaterialMgrGta::Id phMaterialMgrGta::GetPackedPolyFlagValue(u32 flag) const
{
#if __BANK
	if (m_mtlDebug.GetDisableShootThrough() && flag == POLYFLAG_SHOOT_THROUGH)
	{
		return 0;
	}
	if (m_mtlDebug.GetDisableSeeThrough() && flag == POLYFLAG_SEE_THROUGH)
	{
		return 0;
	}
#endif
	return (static_cast<Id>(BIT(flag)) << 24);
}


///////////////////////////////////////////////////////////////////////////////
// LoadRumbleProfiles
///////////////////////////////////////////////////////////////////////////////

void phMaterialMgrGta::LoadRumbleProfiles()
{
    PARSER.LoadObject("common:/data/materials/rumbleprofiles", "meta", m_rumbleProfileList);
}


///////////////////////////////////////////////////////////////////////////////
// GetRumbleProfileIndex
///////////////////////////////////////////////////////////////////////////////

s32 phMaterialMgrGta::GetRumbleProfileIndex(const char* pName) const
{
	u32 hash = atStringHash(pName);

	for (s32 i = 0; i < m_rumbleProfileList.m_rumbleProfiles.GetCount(); ++i)
	{
		if (m_rumbleProfileList.m_rumbleProfiles[i].m_profileName.GetHash() == hash)
			return i;
	}

	return -1;
}
