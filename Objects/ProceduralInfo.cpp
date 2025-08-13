///////////////////////////////////////////////////////////////////////////////
//  
//	FILE: 	ProceduralInfo.h
//	BY	: 	Mark Nicholson
//	FOR	:	Rockstar North
//	ON	:	10 October 2006
//	WHAT:	Routines to handle loading in and storing of procedural data
//
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

#include "ProceduralInfo.h"
#include "ProceduralInfo_parser.h"
// Rage headers
#include "fwsys/fileExts.h"
#include "math/amath.h"
#include "math/float16.h"
#include "parser/restparserservices.h"
#include "diag/art_channel.h"

// Framework headers
//#include	"fwmaths/Maths.h"
#include	"vfx/channel.h"

// Game headers
#include "Debug/Debug.h"
#include "ModelInfo/ModelInfo.h"
#include "ModelInfo/ModelInfo_Factories.h"
#include "Physics/GtaMaterialManager.h"
#include "Renderer/PlantsMgr.h"		// CPLANT_SLOT_NUM_TEXTURES...
#include "Scene/Entity.h"
#include "Scene/DataFileMgr.h"
#include "Objects/ProcObjects.h"

///////////////////////////////////////////////////////////////////////////////
//  OPTIMISATIONS - TURN ON IN OPTIMISATIONS.H
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////

CProceduralInfo	g_procInfo;

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CLASS ProcObjData
///////////////////////////////////////////////////////////////////////////////

void CProcObjInfo::OnPostLoad()
{
#if __BANK
	if(	(m_MinDistance.GetFloat32_FromFloat16() > CPLANT_TRILOC_SHORT_FAR_DIST_INITVAL)	||
		(m_MaxDistance.GetFloat32_FromFloat16() > CPLANT_TRILOC_SHORT_FAR_DIST_INITVAL)	)
	{
		m_Flags.Set(PROCOBJ_FARTRI);
	}
#else
	if(	(m_MinDistance.GetFloat32_FromFloat16() > CPLANT_TRILOC_SHORT_FAR_DIST)	||
		(m_MaxDistance.GetFloat32_FromFloat16() > CPLANT_TRILOC_SHORT_FAR_DIST)	)
	{
		m_Flags.Set(PROCOBJ_FARTRI);
	}
#endif

	m_ModelIndex = fwArchetypeManager::GetArchetypeIndex(m_ModelName);
	Assertf(m_ModelIndex != fwModelId::MI_INVALID, "Model %s in procedural.meta doesn't exist", m_ModelName.GetCStr());
}

void CProcObjInfo::OnPreSave()
{
#if !__FINAL
	if(m_ModelIndex != fwModelId::MI_INVALID)
	{
		m_ModelName = fwArchetypeManager::GetArchetype(m_ModelIndex.Get())->GetModelName();
	}
#endif //!__FINAL
}

///////////////////////////////////////////////////////////////////////////////
//  CLASS CPlantInfo
///////////////////////////////////////////////////////////////////////////////

void CPlantInfo::OnPostLoad()
{
//	Assertf(m_Density >= m_DensityRange.GetFloat32_FromFloat16(), "%s: Density needs to be greater than the density range", m_Tag.GetCStr());

	const float collisionRadius		= m_CollisionRadius.GetFloat32_FromFloat16();
	m_CollisionRadius.SetFloat16_FromFloat32(collisionRadius*collisionRadius);

#if __BANK
	CPlantMgr::MarkReloadConfig();	// let PlantsMgr know that config just changed
#endif
}

///////////////////////////////////////////////////////////////////////////////
//  CLASS ProcTagLookup
///////////////////////////////////////////////////////////////////////////////

void ProcTagLookup::PreLoad(parTreeNode* pTreeNode)
{
	procObjIndex = -1;
	plantIndex = -1;

	parTreeNode* pProcObj = pTreeNode->FindChildWithName("procObjTag");
	if(pProcObj)
	{
		procObjIndex = (s16)g_procInfo.FindProcObjInfo(pProcObj->GetData());
	}

	parTreeNode* pPlantObj = pTreeNode->FindChildWithName("plantTag");
	if(pPlantObj)
	{
		plantIndex = (s16)g_procInfo.FindPlantInfo(pPlantObj->GetData());
	}
#if !__FINAL
	// If tag lookup already has a name use that otherwise work out one from the procObj and plants
	parTreeNode* pName = pTreeNode->FindChildWithName("name");
	if(pName)
	{
		m_name = pName->GetData();
	}
	else
	{
		// Get name
		if(procObjIndex != -1)
			m_name = g_procInfo.GetProcObjInfo(procObjIndex).GetTag().GetCStr();
		else if(plantIndex != -1)
			m_name = g_procInfo.GetPlantInfo(plantIndex).GetTag().GetCStr();
	}
#endif // !__FINAL

	m_Flags.Reset();
	parTreeNode* pFlags = pTreeNode->FindChildWithName("Flags");
	if(pFlags)
	{
		char *flagsStr = pFlags->GetData();
		if(flagsStr)
		{
			for(int i=0; i<parser_ProcTagLookupFlags_Count; i++)
			{
				// flag present?
				if( strstr(flagsStr, parser_ProcTagLookupFlags_Strings[i]) )
				{
					m_Flags.Set(parser_ProcTagLookupFlags_Values[i].m_Value);
				}
			}
		}
	}

	pTreeNode->ClearChildrenAndData();
}


void ProcTagLookup::PostSave(parTreeNode* NOTFINAL_ONLY(pTreeNode))
{
#if !__FINAL
	pTreeNode->ClearChildrenAndData();

	if(procObjIndex != -1)
	{
		pTreeNode->InsertStdLeafChild("procObjTag", g_procInfo.GetProcObjInfo(procObjIndex).GetTag().GetCStr());
	}

	if(plantIndex != -1)
	{
		pTreeNode->InsertStdLeafChild("plantTag", g_procInfo.GetPlantInfo(plantIndex).GetTag().GetCStr());
	}

	pTreeNode->InsertStdLeafChild("name", GetName());
#endif //!__FINAL...
}


///////////////////////////////////////////////////////////////////////////////
//  CLASS CProceduralInfo
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  Init
///////////////////////////////////////////////////////////////////////////////

bool				CProceduralInfo::Init			()
{
	const CDataFileMgr::DataFile* pData = DATAFILEMGR.GetFirstFile(CDataFileMgr::PROC_META_FILE);
    if (DATAFILEMGR.IsValid(pData))
    {
#if 1
		// load procedural.meta:
		char file[256] = {0};
		ASSET.RemoveExtensionFromPath(file, sizeof(file), pData->m_filename);
		const char* ext = ASSET.FindExtensionInPath(pData->m_filename);
		PARSER.LoadObject(file, ++ext, *this);
#else
		// load procedural.pso.meta:
		fwPsoStoreLoader loader = fwPsoStoreLoader::MakeSimpleFileLoader<CProceduralInfo>();
		fwPsoStoreLoadInstance inst(this);
		loader.GetFlagsRef().Set(fwPsoStoreLoader::RUN_POSTLOAD_CALLBACKS);
		char fullName[RAGE_MAX_PATH];
		fullName[0] = '\0';
		ASSET.AddExtension(fullName, RAGE_MAX_PATH, pData->m_filename, META_FILE_EXT);
		loader.Load(fullName, inst);
#endif
		parRestRegisterSingleton("scene/procedural", *this, pData->m_filename);
	}

	if(!gPlantMgr.CheckProceduralMeta())
	{
		return false;
	}

	return true;
}

void CProceduralInfo::ConstructTagTable()
{
	m_numProcTags = MAX_PROCEDURAL_TAGS-1;

	for(int i=MAX_PROCEDURAL_TAGS-1; i>=0; i--)
	{
		if( (m_procTagTable[i].procObjIndex != -1)	||
			(m_procTagTable[i].plantIndex	!= -1)	)
		{
			m_numProcTags = i;
			break;
		}
	}

/*
	for (s32 i=0; i<MAX_PROCEDURAL_TAGS; i++)
	{
		m_procTagTable[i].procObjIndex = -1;
		m_procTagTable[i].plantIndex = -1;
	}

	atHashValue lastHash;
	int procNum = 1;
	// go through procedural objects finding all the starting indices for each tag
	for(int i=0; i<m_procObjInfos.GetCount(); i++)
	{
		if(m_procObjInfos[i].m_Tag != lastHash)
		{
			//Assert(m_procTagTable[procNum].procObjIndex == i);
		#if !__FINAL
			m_procTagTable[procNum].m_name = m_procObjInfos[i].m_Tag.GetCStr();
		#endif
			Assign(m_procTagTable[procNum++].procObjIndex, i);
			lastHash = m_procObjInfos[i].m_Tag;
		}
	}
	lastHash = atHashValue::Null();
	// go through plants finding all the starting indices for each tag
	int plantNum = PROCEDURAL_TAG_PLANTS_BEGIN;
	for(int i=0; i<m_plantInfos.GetCount(); i++)
	{
		atHashValue plantHash = m_plantInfos[i].m_Tag;
		if(plantHash != lastHash)
		{
			bool bAdded = false;

			// go through all the proc obj hashes already setup to see if they need a plant index setup
			for(int j=1; j<procNum; j++)
			{
				const int procObjIndex = m_procTagTable[j].procObjIndex;
				
				// if plant name is same as procobj name setup index and flag that this name has already been added
				if(m_procObjInfos[procObjIndex].m_Tag == plantHash)
				{
					//Assert(m_procTagTable[j].plantIndex == i);
					Assign(m_procTagTable[j].plantIndex, i);
					bAdded = true;
				}
				
				if(m_procObjInfos[procObjIndex].m_PlantTag == plantHash)
				{
					//Assert(m_procTagTable[j].plantIndex == i);
					Assign(m_procTagTable[j].plantIndex, i);
				}
			}

			if(!bAdded)
			{
				//Assert(m_procTagTable[plantNum].plantIndex == i);
			#if !__FINAL
				m_procTagTable[plantNum].m_name = m_plantInfos[i].m_Tag.GetCStr();
			#endif
				Assign(m_procTagTable[plantNum++].plantIndex, i);
			}
			lastHash = plantHash;
		}
	}
	m_numProcTags = plantNum;
*/
}

///////////////////////////////////////////////////////////////////////////////
//  ShutdownLevel
///////////////////////////////////////////////////////////////////////////////

void CProceduralInfo::Shutdown ()
{
}

int	CProceduralInfo::FindProcObjInfo(const char* pName)
{
	for(int i=0; i<m_procObjInfos.GetCount(); i++)
	{
		if(m_procObjInfos[i].GetTag() == atHashValue(pName))
			return i;
	}
	return -1;
}

int	CProceduralInfo::FindPlantInfo(const char* pName)
{
	for(int i=0; i<m_plantInfos.GetCount(); i++)
	{
		if(m_plantInfos[i].GetTag() == atHashValue(pName))
			return i;
	}
	return -1;
}

///////////////////////////////////////////////////////////////////////////////
//  ProcessProcTag
///////////////////////////////////////////////////////////////////////////////

/*s32				CProceduralInfo::ProcessProcTag			(char procTagNames[MAX_PROCEDURAL_TAGS][MAX_PROCEDURAL_TAG_LENGTH],
														 char procPlantNames[MAX_PROCEDURAL_TAGS][MAX_PROCEDURAL_TAG_LENGTH],
														 char* newProcTag, s32 type)
{
	// search for this proc tag in the table
	for (s32 i=1; i<=m_numProcTags; i++)
	{
		if (strcmp(procTagNames[i], newProcTag)==0)
		{
			// found in list already - make sure it hasn't been set already for this type
			if (type==0)
			{
				Assert(m_procTagTable[i].procObjIndex==-1);
				m_procTagTable[i].procObjIndex = m_numProcObjInfos;
			}	
			else if (type==1) 
			{
				Assert(m_procTagTable[i].plantIndex==-1);
				m_procTagTable[i].plantIndex = m_numPlantInfos;
			}
			else
			{
				Assert(0);
			}
				
			return i;
		}
		
		// procgrass: add procgrass to parent procobject tag:
		if(type==1)
		{
			if( strcmp(procPlantNames[i], newProcTag)==0 )
			{
				Assert(m_procTagTable[i].plantIndex==-1);
				m_procTagTable[i].plantIndex = m_numPlantInfos;
				break;	// still add plant type below
			}
		}
	}

	// proc tag hasn't been found in the table - create a new one
	m_numProcTags++;
	if(m_numProcTags > MAX_PROCEDURAL_TAGS)
	{
		Assertf(m_numProcTags <= MAX_PROCEDURAL_TAGS, "Num proc tags exceeded! (max: %d)",MAX_PROCEDURAL_TAGS);
		Quitf("Num proc tags exceeded! (max: %d)",MAX_PROCEDURAL_TAGS);
	}

	strcpy(procTagNames[m_numProcTags], newProcTag);
#if __BANK
	m_procTagTable[m_numProcTags].name = newProcTag;
#endif
	if (type==0)
	{
		Assert(m_procTagTable[m_numProcTags].procObjIndex==-1);
		m_procTagTable[m_numProcTags].procObjIndex = m_numProcObjInfos;
	}	
	else if (type==1)
	{
		Assert(m_procTagTable[m_numProcTags].plantIndex==-1);
		m_procTagTable[m_numProcTags].plantIndex = m_numPlantInfos;
	}
	else
	{
		Assert(0);
	}

	return m_numProcTags;
}*/


///////////////////////////////////////////////////////////////////////////////
//  CreatesProcObjects
///////////////////////////////////////////////////////////////////////////////

bool				CProceduralInfo::CreatesProcObjects		(s32 procTagId)
{
	Assert(procTagId<=m_numProcTags); 
	bool bCreatesObjects = m_procTagTable[procTagId].procObjIndex>-1;

#if __PS3 || __XENON
	// if nextgen flag set on procTagRable, then skip it for current gen:
	if(bCreatesObjects)
	{
		bCreatesObjects &= !m_procTagTable[procTagId].m_Flags.IsSet(PROCTAGLOOKUP_NEXTGENONLY);
	}
#endif

	return bCreatesObjects;
}


///////////////////////////////////////////////////////////////////////////////
//  CreatesPlants
///////////////////////////////////////////////////////////////////////////////

bool				CProceduralInfo::CreatesPlants			(s32 procTagId)
{
	Assert(procTagId<=m_numProcTags); 

	bool bCreatesPlants = m_procTagTable[procTagId].plantIndex>-1;

#if __PS3 || __XENON
	// if nextgen flag set on procTagRable, then skip it for current gen:
	if(bCreatesPlants)
	{
		bCreatesPlants &= !m_procTagTable[procTagId].m_Flags.IsSet(PROCTAGLOOKUP_NEXTGENONLY);
	}
#endif

	return bCreatesPlants;
}

//
//
// returns index to procObjInfos[] table:
//
u32 CProceduralInfo::GetProcObjInfoIdx(CProcObjInfo *pInfo)
{
	return ptrdiff_t_to_int(pInfo - &m_procObjInfos[0]);
}

void CProceduralInfoWrapper::Init(unsigned /*initMode*/)
{
    g_procInfo.Init();
}

void CProceduralInfoWrapper::Shutdown(unsigned /*shutdownMode*/)
{
    g_procInfo.Shutdown();
}


