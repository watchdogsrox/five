//
//
//
//
#ifndef NAVMESH_GTA_H
#define NAVMESH_GTA_H

#include "ai\navmesh\NavMesh.h"

//*********************************************************************************
//
//	Filename : NavMeshGta.h
//	Author : James Broad - split by Greg Smith
//	RockstarNorth (c) 2005
//
//	------------------------------------------------------------------------------
//	
//	Overview:
//	---------
//
//	This implements a navigation mesh which defines all the areas upon which
//	characters can walk.  Polygon adjacency info is used to allow us to perform
//	an approximate path search across the mesh, and then optimised 2d line-of-sight
//	tests are performed across the mesh to refine the path to a minimal set of
//	waypoints.
//	Each CNavMesh occupies the space of a small block of sectors (4x4, 8x8) and
//	they are managed by the CPathServer class.
//	CNavMesh classes may be streamed in/out at any time between path requests,
//	but never whilst a search is active.
//
//******************************************************************************

class CNavMeshGTA : public CNavMesh
{
public:

#if !__FINAL
	void VisualiseQuadTree(bool bDrawCoverpoints);
	void VisualiseQuadTree(CNavMeshQuadTree * pTree, bool bDrawCoverpoints);
#endif
};

#endif	// NAVMESH_GTA_H
