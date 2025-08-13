#ifndef PATHSERVER_DATATYPES_H
#define PATHSERVER_DATATYPES_H
//************************************************************************************
//	Filename :	PathServer_DataTypes.h
//	Author   :	James Broad
//	Purpose  :	Contains various data-types used by the pathserver
//************************************************************************************

// Framework headers
#include "ai/navmesh/datatypes.h"

// TODO: Remove this file. All its useful contents have been moved, all or most of it
// to 'ai/navmesh/datatypes.h'.

// This TVector3Unpadded struct appears to be completely unused. /FF
#if 0

struct TVector3Unpadded
{
public:
	inline void Get(Vector3 & vVec) { vVec.x=fPts[0]; vVec.y=fPts[1]; vVec.z=fPts[2]; }
	inline void Set(const Vector3 & vVec) { fPts[0]=vVec.x; fPts[1]=vVec.y; fPts[2]=vVec.z; }
private:
	float fPts[3];
};

#endif

#endif //	PATHSERVER_DATATYPES_H
