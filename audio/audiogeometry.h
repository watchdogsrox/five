//  
// audio/audiogeometry.h
// 
// Copyright (C) 1999-2006 Rockstar Games.  All Rights Reserved.
// 

#ifndef AUD_GEOMETRY_H
#define AUD_GEOMETRY_H

#include <vector>
using namespace std;

#include "fwaudio/audmesh.h"
#include "pathserver\PathServer_Store.h"
#include "Scene\Entity.h"
#include "scene\portals\interiorInst.h"

#include "phbound\BoundGeom.h"
#include "phbound\BoundBox.h"
#include "phbound\BoundBvh.h"
#include "phbound\BoundComposite.h"

#define USE_AUDMESH_SECTORS 0

#if USE_AUDMESH_SECTORS
#define DSECTOR_HYPOTENUSE 452.55f // WARNING : 2 * SECTOR_HYPOTENUSE, if we change the values above we have to recalculate this value.
// f32 to save a few LSH - these are set after the audmesh is initialised, may need refactoring as functionality is moved to the audmesh
extern f32 g_fAudNumWorldSectorsX;
extern f32 g_fAudNumWorldSectorsY;

extern u32 g_audNumWorldSectorsX;
extern u32 g_audNumWorldSectorsY;

extern u32 g_audNumWorldSectors;

extern f32 g_audWidthOfSector;
extern f32 g_audDepthOfSector;

struct audSector;
extern audSector* g_AudioWorldSectors;


const u32 g_audMaxCachedSectorOps = 64;
#endif


typedef aiMeshAssetDef<agPolyMesh> audMeshAssetDef;


class audMeshStore : public aiMeshStore<agPolyMesh, audMeshAssetDef>
{
public:
	audMeshStore(
		const char* pModuleName, 
		int moduleTypeIndex,
		s32 size,
		bool requiresTempMemory,
		bool canDefragment = false,
		s32 rscVersion = 0);
	virtual ~audMeshStore();

	void Init();
	virtual void Shutdown();

	virtual void* GetPtr(strLocalIndex index);
	virtual strLocalIndex Register(const char* name);
	virtual void Remove(strLocalIndex index);

	virtual bool Load(strLocalIndex index, void* pData, int size)
	{
		datResourceMap * pResMap = NULL;
		datResourceInfo * pResHdr = NULL;
		return LoadOrPlace(index.Get(), pData, size, pResMap, pResHdr);
	}
	virtual void PlaceResource(strLocalIndex index, datResourceMap & map, datResourceInfo & header)
	{
		datResourceMap * pResMap = &map;
		datResourceInfo * pResHdr = &header;
		LoadOrPlace(index.Get(), NULL, 0, pResMap, pResHdr);
	}

	// TODO: perhaps remove the Load() here and just turn LoadOrPlace() into PlaceResource(), just like
	// we did for aiNavMeshStore. I don't know much about the audio meshes though, so I'm not sure if they
	// are properly resourced or not right now. /FF

	bool LoadOrPlace(s32 iStreamingIndex, void * pMemForRead, s32 iLengthForRead, datResourceMap* & pMapForPlacement, datResourceInfo* & pHeaderForPlacement);

#if !__FINAL
	virtual const char* GetName(strLocalIndex index) const;
#endif
#if __BANK
	void DrawMesh();
#endif
};


//Class used for beam tracing
class audBeamPyramid
{
public:
	//Default constructor
	audBeamPyramid()
	{};
	//Constructor that sets up the pyramid too
	//apex: the origin of the beam and apex of the pyramid
	//direction: a vector describing the direction of the beam (i.e the line from the apex to the center of the base of the pyramid
	//axis: a vector used to set the orientation of the pyramid
	//faceAngle: the angle in radians between direction and the faces of the beam: must divide cleanly into 2PI
	audBeamPyramid(const Vec3f &apex, Vec3f &direction, const Vec3f &axis, float faceAngle);
	//Sets up the pyramid
	//apex: the origin of the beam and apex of the pyramid
	//direction: a vector describing the direction of the beam (i.e the line from the apex to the center of the base of the pyramid
	//faceAngle: the angle in radians between direction and the faces of the beam
	void Init(const Vec3f *apex, Vec3f *direction,const Vec3f &axis, float faceAngle);	
	static void RotateVectorAboutCustomAxis(Vec3f &outVec, const Vec3f &inVec, const Vec3f &axis, float angle);

private:
	//The beam consists of four intersecting planes, an apex and a direction that form a pyramid
	Vec4f m_Faces[4];
	Vec3f m_apex;
	Vec3f m_direction; //this decribes the direction running from the apex of the pyramid to the center of its base
};

class audGeometry
{
	friend audMeshStore;
public:

	~audGeometry();

	static void Update();
	void Init();

	static void RegisterStreamingModule(const char* pPathForDatFile = NULL);

	void ReadIndexMappingFile();

	void RequestAndEvictMeshes();
	void GetAudMeshExtentsFromExtents(const TNavMeshIndex meshIndex, Vector2 &min, Vector2 &max);

	audMeshStore * GetAudioMeshStore() { return sm_pAudMeshStore; }

	static bool sm_RunningRequestAndEvictMeshes;
	static bool sm_StreamingDisabled;

#if __BANK
	static char * GetDebugMeshName() { return ms_DebugMeshName; } 
	static void AddWidgets(bkBank &bank);
	static void DrawMesh(u32 iHash);
	static void DrawMesh(const char * pMeshName);
#endif

private:	

	static void ProcessAudmesh(const Vector3 &origin);
	static void ProcessAudmesh();

	static void TestAudMeshStreaming();

#if __BANK
	static char ms_DebugMeshName[256];
#endif

	static aiMeshLoadRegion sm_AudMeshRequiredRegion;

	//The distance at which audmeshes are loaded
	static f32 sm_AudMeshLoadProximity;

	//AudMeshes to cover the entire game map
	static audMeshStore * sm_pAudMeshStore;

	static Vector3 m_Origin;
	static Vector3 m_LastOrigin;

};


#endif
