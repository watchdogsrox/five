//
// scene/loader/mapFileMgr.h
//
// Copyright (C) 1999-2011 Rockstar Games. All Rights Reserved.
//

#ifndef SCENE_LOADER_MAPFILEMGR_H
#define SCENE_LOADER_MAPFILEMGR_H

#include "parser/macros.h"
#include "data/base.h"
#include "atl/array.h"
#include "string/stringhash.h"


#include "fwscene/mapdata/maptypes.h"
#include "fwscene/mapdata/mapdata.h"


//pargen includes
#include "scene/loader/PackFileMetaData.h"

class CTxdRelationship;  //FWD declare

//
// Map data loader manager class.
//
class CMapFileMgr
{
public:
	static void Init(unsigned initMode);
	static void Shutdown(unsigned shutdown);
	
	static CMapFileMgr& GetInstance() { return ms_instance; }

	void Reset(void);

	// handle the per .rpf meta data file
	void LoadPackFileMetaData(const fiPackfile* pDevice, const char* pPackFileName);
	void LoadPackFileMetaDataCommon(CPackFileMetaData &packFileMetaData, const char* pPackFileName);

	void LoadGlobalTxdParents(void);
	void LoadGlobalTxdParents(const char* slotName);

	void ProcessGlobalTxdParentsFile(const char* pFileName);

	void SetMapFileDependencies_old(void);
	void SetMapFileDependencies(void);

	atHashString	LookupInteriorWeaponBound(atHashString name) { atHashString* pBound = m_interiorBounds.Access(name); return((pBound!=NULL) ? (*pBound) : atHashString((u32)0));}
	void Cleanup(void); 

private:
	
	void SetupTxdParent(const CTxdRelationship& txdRelationShip);

	static CMapFileMgr ms_instance;
	static bool			ms_bDependencyArraysAllocated;

	atArray<CImapDependency> m_MapDependencies;

	atArray<CImapDependencies>	m_MapDeps;
	atArray<atHashString>		m_packFileNames;

	atArray<CItypDependencies>	m_TypDeps;
	atArray<atHashString>		m_ItypPackFileNames;
	
	atMap<atHashString, atHashString>	m_interiorBounds;
};
 
#endif // SCENE_LOADER_MAPFILEMGR_H
