//  
// audio/audiogeometry.cpp
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 


#include "audiogeometry.h" 
#include "audio/environment/environment.h"
#include "fwaudio/audmesh.h"
#include "fwscene\world\WorldLimits.h"

// Rage includes
#include "bank\bkmgr.h"
#include "diag\output.h"
#include "fwsys\fileExts.h"
#include "pathserver\PathServer.h"
#include "pathserver\ExportCollision.h"
#include "phbound\BoundGeom.h"
#include "phbound\boundbvh.h"
#include "phbound\optimizedbvh.h"
#include "physics\iterator.h"
#include "peds\PlayerInfo.h"
#include "peds/Ped.h"
#include "system\param.h"
#include "audioengine\engine.h"

#if USE_AUDMESH_SECTORS
audSector* g_AudioWorldSectors = NULL;

f32 g_fAudNumWorldSectorsX = 200.f;
f32 g_fAudNumWorldSectorsY = 200.f;

u32 g_audNumWorldSectorsX = 200;
u32 g_audNumWorldSectorsY = 200;

u32 g_audNumWorldSectors = g_audNumWorldSectorsX*g_audNumWorldSectorsY;
f32 g_audWidthOfSector = (WORLDLIMITS_XMAX - WORLDLIMITS_XMIN) / g_fAudNumWorldSectorsX;
f32 g_audDepthOfSector = (WORLDLIMITS_YMAX - WORLDLIMITS_YMIN) / g_fAudNumWorldSectorsY;
#endif

#if __BANK

static bool s_ShowDebugMesh = false;
static bool s_DisplayLoadedMeshes = false;

char audGeometry::ms_DebugMeshName[256] = { 0 };

#endif

PARAM(audmesh, "turns on audmesh streaming and streaming module registration");

float audGeometry::sm_AudMeshLoadProximity = 100.f;
aiMeshLoadRegion audGeometry::sm_AudMeshRequiredRegion;
bool audGeometry::sm_RunningRequestAndEvictMeshes = false;
bool audGeometry::sm_StreamingDisabled = true;
audMeshStore * audGeometry::sm_pAudMeshStore;
Vector3 audGeometry::m_Origin(0.f, 0.f, 0.f);
Vector3 audGeometry::m_LastOrigin(0.f, 0.f, 0.f);

audMeshStore::audMeshStore(
	const char* pModuleName, 
	int moduleTypeIndex, 
	s32 size,
	bool requiresTempMemory,
	bool canDefragment,
	s32 rscVersion)
	: aiMeshStore<agPolyMesh, audMeshAssetDef>(pModuleName, moduleTypeIndex, size, requiresTempMemory, canDefragment, rscVersion)
{
	aiMeshStore<agPolyMesh, audMeshAssetDef>::m_iMeshIndexNone = AUDMESH_INDEX_NONE;

}
audMeshStore::~audMeshStore()
{
#if USE_AUDMESH_SECTORS
	if(g_AudioWorldSectors)
	{
		delete g_AudioWorldSectors;
		g_AudioWorldSectors = NULL;
	}
#endif
}

void audMeshStore::Init()
{
	aiMeshStore::Init();
}

void audMeshStore::Shutdown()
{
	aiMeshStore::Shutdown();
}
void * audMeshStore::GetPtr(strLocalIndex index)
{
	return fwAssetStore::GetPtr(index);
}

#if !__FINAL
const char* audMeshStore::GetName(strLocalIndex iStreamingIndex) const
{
	Assert(iStreamingIndex.Get() <= (s32)m_iMaxMeshIndex);
	Assert(m_pIndexMappingArray);
	static char s_szName[64] = "\0";

	// Find the streaming index from the remapping table.
	// Super-inefficient.. this is only done when Asserting, right?
	s32 iMesh = m_iMeshIndexNone;
	for(s32 i=0; i<m_iMaxMeshIndex; i++)
	{
		if(m_pIndexMappingArray[i]==iStreamingIndex.Get())
		{
			iMesh = i;
			break;
		}
	}
	if(iMesh != m_iMaxMeshIndex)
	{
		// Work out the original filename from the navmesh index.
		s32 iY = iMesh / GetNumMeshesInX();
		s32 iX = (iMesh - (iY * GetNumMeshesInY()));
		iX *= GetNumSectorsPerMesh();
		iY *= GetNumSectorsPerMesh();

		sprintf( s_szName, "audmesh[%i][%i]", iX, iY );
		return s_szName;
	}
	return NULL;
}
#endif

strLocalIndex audMeshStore::Register(const char* name)
{
	if(!PARAM_audmesh.Get())
	{
		return strLocalIndex(-1);
	}

	Assert(name);
	// TODO: Check up on this. GetNavMeshIndexFromFilename() may actually be using
	// parameters (m_iNumSectorsPerMesh and m_iNumMeshesInX) from the aiNavMeshStore
	// for the regular navmeshes, instead of this audMeshStore.
	u32 iMeshIndex = CPathServer::GetNavMeshIndexFromFilename(kNavDomainRegular, name);
	Assert(iMeshIndex != AUDMESH_INDEX_NONE);

	strLocalIndex iStreamingIndex = GetStreamingIndex(iMeshIndex);
	Assert(iStreamingIndex.Get() != -1);

	return iStreamingIndex;
}

void audMeshStore::Remove(strLocalIndex index)
{
	// NB: If you are processing audMeshes in a thread other that the main one, then you will want a critical section
	// here to ensure to ensure that you don't stream things out which you're in the middle of using them!

	fwAssetStore::Remove(index);
}
bool audMeshStore::LoadOrPlace(s32 iStreamingIndex, void * pMemForRead, s32 iLengthForRead, datResourceMap* & pMapForPlacement, datResourceInfo* & pHeaderForPlacement)
{
	USE_MEMBUCKET(MEMBUCKET_AUDIO);

	Assert(iStreamingIndex < (s32)GetNumMeshesInAnyLevel());
	ASSERT_ONLY( agPolyMesh * pAudMesh = GetMeshByStreamingIndex(iStreamingIndex); )
	Assert(!pAudMesh);

	// NB: If you are processing audMeshes in a thread other that the main one, then you will want a critical section
	// here to ensure to ensure that you don't stream things out which you're in the middle of using them!

	agPolyMesh * pMesh = NULL;

	if(pMemForRead)
	{
		char filename[256];
		fiDeviceMemory::MakeMemoryFileName(filename, 256, pMemForRead, iLengthForRead, false, "agPolyMesh");
		pMesh = agPolyMesh::Load(filename);
	}
	else if(pMapForPlacement)
	{
		pgRscBuilder::PlaceStream(pMesh, *pHeaderForPlacement, *pMapForPlacement, "agPolyMesh");
	}
	else
	{
		Assertf(false, "audMeshStore::LoadOrPlace() - we've gotta be either reading or placing the audio-mesh.");
		return false;
	}

	return true;
}

#if __BANK
void audMeshStore::DrawMesh() 
{
	int x,y;
	for(y=0; y< GetNumMeshesInX()*GetNumSectorsPerMesh(); y+=GetNumSectorsPerMesh())
	{
		for(x=0; x<GetNumMeshesInY()*GetNumSectorsPerMesh()
			; x+=GetNumSectorsPerMesh())
		{
			const int iMeshIndex = GetMeshIndexFromSectorCoords(x, y);
			
			if(iMeshIndex != m_iMeshIndexNone)
			{
				const int iStreamingIndex = GetStreamingIndex(iMeshIndex);

				if(iStreamingIndex>=0)
				{
					agPolyMesh * mesh = GetMeshByIndex(iMeshIndex);
					if(mesh)
					{
						mesh->DebugDraw();
					}
				}
			}
		}
	}

	
}
#endif //__BANK



//*********************************************************************************************************


namespace rage
{
	extern audEngine g_AudioEngine;
}

audGeometry::~audGeometry()
{
	if(sm_pAudMeshStore)
	{
		delete sm_pAudMeshStore;
		sm_pAudMeshStore = NULL;
	}
}

atArray<aiMeshLoadRegion> audMeshLoadRegions(0,4);


void audGeometry::RegisterStreamingModule(const char* pPathForDatFile)
{
	if(!pPathForDatFile)
	{
		pPathForDatFile = "common:/data/"; 
	}

	if(!PARAM_audmesh.Get())
	{
		return;
	}

	// Read the "nav.dat" file
	CNavDatInfo navMeshesInfo, navNodesInfo;
	CPathServer::ReadDatFile(pPathForDatFile, navMeshesInfo, navNodesInfo);

	if(navMeshesInfo.iMaxMeshIndex == 0)
	{
		Assertf(false, "PathServer::Init() - didn't initialise properly. (in audGeometryRegisterStreamingModule)");
		return;
	}


#ifdef GTA_ENGINE
	sm_pAudMeshStore = rage_new audMeshStore("AudioMeshes", AUDMESH_FILE_ID, 10000 /*navMeshesInfo.iNumMeshesInAnyLevel*/, false);
#else
	sm_pAudMeshStore = rage_new audMeshStore("AudioMeshes", NAVMESH_FILE_ID, 10000 /*navMeshesInfo.iNumMeshesInAnyLevel*/, false);
#endif


	// At this point, fwConfigManager should have been initialized already, so
	// we can just force the pools to be allocated right here, unlike what we do for
	// the statically constructed fwAssetStore objects (g_IplStore, etc). It needs
	// to be done before the aiNavMeshStore::Init() call.
#ifdef GTA_ENGINE
	sm_pAudMeshStore->FinalizeSize();
#endif

	sm_pAudMeshStore->SetMaxMeshIndex(10000); //navMeshesInfo.iMaxMeshIndex);
	sm_pAudMeshStore->SetMeshSize(navMeshesInfo.fMeshSize);
	sm_pAudMeshStore->SetNumMeshesInX(navMeshesInfo.iNumMeshesInX);
	sm_pAudMeshStore->SetNumMeshesInY(navMeshesInfo.iNumMeshesInY);
	sm_pAudMeshStore->SetNumMeshesInAnyLevel(10000); //navMeshesInfo.iNumMeshesInAnyLevel);
	sm_pAudMeshStore->SetNumSectorsPerMesh(navMeshesInfo.iSectorsPerMesh);
	sm_pAudMeshStore->Init();


#ifdef GTA_ENGINE
#if __BANK
	if(PARAM_audmesh.Get())
		sm_StreamingDisabled = false;
#endif
#if NAVMESH_EXPORT
	if(CNavMeshDataExporter::WillExportCollision())
		sm_StreamingDisabled = true;
#endif

	if(!sm_StreamingDisabled)
	{

			if(sm_pAudMeshStore)
			{
				strStreamingEngine::GetInfo().GetModuleMgr().AddModule(sm_pAudMeshStore);
			}
	}
#endif

#if USE_AUDMESH_SECTORS
	g_fAudNumWorldSectorsX = (f32)(sm_pAudMeshStore->GetNumMeshesInX() * sm_pAudMeshStore->GetNumSectorsPerMesh());
	g_fAudNumWorldSectorsY = (f32)(sm_pAudMeshStore->GetNumMeshesInY() * sm_pAudMeshStore->GetNumSectorsPerMesh());

	g_audNumWorldSectorsX = sm_pAudMeshStore->GetNumMeshesInX() * sm_pAudMeshStore->GetNumSectorsPerMesh();
	g_audNumWorldSectorsY = sm_pAudMeshStore->GetNumMeshesInY() * sm_pAudMeshStore->GetNumSectorsPerMesh();

	g_audNumWorldSectors = (g_audNumWorldSectorsX * g_audNumWorldSectorsY);
	g_audWidthOfSector = sm_pAudMeshStore->GetMeshSize()/sm_pAudMeshStore->GetNumSectorsPerMesh();
	g_audDepthOfSector = sm_pAudMeshStore->GetMeshSize()/sm_pAudMeshStore->GetNumSectorsPerMesh();

	g_AudioWorldSectors = rage_new audSector[g_audNumWorldSectors];
#endif
}

void audGeometry::Init()
{
}

void audGeometry::ReadIndexMappingFile()
{
	if(!sm_StreamingDisabled)
	{
		Assert(sm_pAudMeshStore);
		sm_pAudMeshStore->AllocateMapping(sm_pAudMeshStore->GetMaxMeshIndex());
		for(s32 m=0; m<sm_pAudMeshStore->GetMaxMeshIndex(); m++)
		{
			sm_pAudMeshStore->SetMapping(m, (s16)m);
		}
	}
}

void audGeometry::Update()
{
	
	if(!sm_StreamingDisabled)
	{
#if __BANK
		if(s_DisplayLoadedMeshes)
		{
			CPed * player = FindPlayerPed();
			if(player)
			{
				const Vector3 & playerPos = VEC3V_TO_VECTOR3(player->GetTransform().GetPosition());
				sm_pAudMeshStore->Visualise(&playerPos);
			} 
		}
		if(s_ShowDebugMesh)
		{
			sm_pAudMeshStore->DrawMesh();
		}
#endif
		ProcessAudmesh();
	}
}

//****************************************************************
//audGeometry::ProcessAudmesh
//This function must be called once a frame from the main game thread.
//Here we request and evict audmeshes surrounding the player
//**************************************************************** 
void audGeometry::ProcessAudmesh()
{
	static const u32 maxNullTime = 10000;
	static u32 lastTimePedWasNoneNull = 0;

	CPed * player = FindPlayerPed();
	if(player)
	{
		lastTimePedWasNoneNull = fwTimer::GetTimeInMilliseconds();
		ProcessAudmesh(VEC3V_TO_VECTOR3(player->GetTransform().GetPosition()));
	} 

	if((fwTimer::GetTimeInMilliseconds() - lastTimePedWasNoneNull) > maxNullTime)
	{
		naAssertf(0, "FindPlayerPed() has been returning NULL for over 10 seconds...something's wrong");
	}
}

void audGeometry::ProcessAudmesh(const Vector3 &origin)
{
	m_Origin = origin;

	// Update player's origin
	sm_AudMeshRequiredRegion.m_vOrigin = Vector2(origin.x, origin.y);
	sm_AudMeshRequiredRegion.m_fRadius = sm_AudMeshLoadProximity;

	audMeshLoadRegions.clear();
	audMeshLoadRegions.Append() = sm_AudMeshRequiredRegion;

	sm_pAudMeshStore->RequestAndEvict(audMeshLoadRegions, NULL);

	m_LastOrigin = m_Origin;
}


void audGeometry::GetAudMeshExtentsFromExtents(const TNavMeshIndex meshIndex, Vector2 &min, Vector2 &max)
{
	const s32 y = meshIndex / sm_pAudMeshStore->GetNumMeshesInY();
	const s32 x = meshIndex - (y*sm_pAudMeshStore->GetNumMeshesInX());
	const f32 meshSize = sm_pAudMeshStore->GetMeshSize();

	min.x = ((((f32)x) * meshSize) + -5000.0f);	// NB please review these numbers, probably not valid with current world size!
	min.y = ((((f32)y) * meshSize) + -5000.0f);	// NB please review these numbers, probably not valid with current world size!
	max.x = min.x + meshSize;
	max.y = min.y + meshSize;
}

#if __BANK

void audGeometry::TestAudMeshStreaming()
{
	//if(audGeometry::GetDebugMeshName()[0])
		//agPolyMesh::RequestMesh(audGeometry::GetDebugMeshName());
}

void audGeometry::AddWidgets(bkBank &bank)
{
	bank.PushGroup("Geometry",false);
		bank.AddText("Debug Mesh Name", audGeometry::ms_DebugMeshName, 256, false);
		bank.AddButton("Test : Stream route", TestAudMeshStreaming);
		bank.AddToggle("Show debug mesh", &s_ShowDebugMesh);
		bank.AddToggle("Display loaded meshes", &s_DisplayLoadedMeshes);
	bank.PopGroup();
}

void audGeometry::DrawMesh(u32 UNUSED_PARAM(Hash))
{
	/*
	u32 slot = CAudMesh::FindExistingMeshSlot(iHash);
	
	//if(slot >= 0)
	{	
		agPolyMesh * mesh = CAudMesh::ms_LoadedMeshes[slot].m_PolyMesh;
		Assertf(mesh, "Loaded slot has null mesh");
		mesh->fwDebugDraw();
	}
	*/
}

void audGeometry::DrawMesh(const char * UNUSED_PARAM(pMeshName))
{
	//const u32 iHash = atStringHash(pMeshName);
	//CAudMesh::DrawMesh(iHash);
}

#endif

//================audBeamPyramid====================//

audBeamPyramid::audBeamPyramid(const Vec3f &apex, Vec3f &direction, const Vec3f &axis, float faceAngle)
	:m_apex(apex),
	m_direction(direction)
{
	Init(NULL, NULL, axis, faceAngle);
}

void audBeamPyramid::Init(const Vec3f *apex, Vec3f *direction, const Vec3f &axis, float faceAngle)
{
	if(apex)
	{
		m_apex = *apex;
	}
	if(direction)
	{
		m_direction = *direction;
	}

	//Set up the normal for the first face
	Vec3f normal(m_direction), faceNormal;
	normal = Cross(normal, axis);
	RotateVectorAboutCustomAxis(faceNormal, normal, axis, faceAngle);

	//d is the shortest distance between the origin and the plane
	float d = Dot(m_apex*(-1.f), faceNormal);

	
	m_Faces[0].SetXYZ(faceNormal);
	m_Faces[0].SetW(d);
	
}

void audBeamPyramid::RotateVectorAboutCustomAxis(Vec3f &outVec, const Vec3f &inVec, const Vec3f &axis, float angle)
{
	Normalize(axis);
	f32 x = inVec.GetX(), y = inVec.GetY(), z = inVec.GetZ();
	f32 u = axis.GetX(), v = axis.GetY(), w = axis.GetZ();
	f32 xu = x*u;
	f32 xv = x*v;
	f32 xw = x*w;
	f32 yu = y*u;
	f32 yv = y*v;
	f32 yw = y*w;
	f32 zu = z*u;
	f32 zv = z*v;
	f32 zw = z*w;
	
	f32 sina = Sinf(angle);
	f32 cosa = Cosf(angle);

	f32 newX = u*(xu+yv+zw) + (x*(v*v+w+w) - u*(yv+zw))*cosa + (zv-yw)*sina;
	f32 newY = v*(xu+yv+zw) + (y*(u*u+w*w) - v*(xu+zw))*cosa + (xw-zu)*sina;
	f32 newZ = w*(xu+yv+zw) + (z*(u*u+v*v) - w*(xu+yv))*cosa + (yu-xv)*sina;

	outVec.SetX(newX);
	outVec.SetY(newY);
	outVec.SetZ(newZ);
}

