// Title	:	QuadTree.h
// Author	:	Greg Smith
// Started	:	18/11/03

#ifndef _QUADTREE_H_
#define _QUADTREE_H_

// Rage headers
#include "vector\vector3.h"

// Framework headers
#include "fwtl/pool.h"
#include "fwmaths/rect.h"

// Game headers
#include "utils\ptrList.h"

class CQuadTreeNode
{
public:
	FW_REGISTER_CLASS_POOL(CQuadTreeNode);

	enum
	{
		TOP_LEFT,
		TOP_RIGHT,
		BOTTOM_LEFT,
		BOTTOM_RIGHT,
		NUM_CHILDREN
	};

	CQuadTreeNode(const fwRect& bb,s32 Depth = 0);
	~CQuadTreeNode();

	void AddItem(void* p_vData, const fwRect& bb);
	void DeleteItem(void* p_vData);
	void DeleteItem(void* p_vData, const fwRect& bb);
	void DeleteAllItems(void);
	void GetAllMatching(const fwRect& bb,CPtrListSingleLink& matches);
	void GetAllMatching(const Vector3& position,CPtrListSingleLink& matches);
	void GetAll(CPtrListSingleLink& Matches);
	void GetAllNoDupes(CPtrListSingleLink& Matches);

	// class used in for all matching functions
	typedef void (*RectCB)(const fwRect& bb, void* data);
	typedef void (*PosnCB)(const Vector3& bb, void* data);

	class QuadTreeFn {
	public:
		virtual ~QuadTreeFn() {};
		virtual void operator()(const fwRect&, void*) {}
		virtual void operator()(const Vector3&, void*) {}
	};
	void ForAllMatching(const fwRect& bb, QuadTreeFn& fn);
	void ForAllMatching(const Vector3& posn, QuadTreeFn& fn);
	// old style functions 
	void ForAllMatching(const fwRect& bb, RectCB fn);
	void ForAllMatching(const Vector3& posn, PosnCB fn);

	fwRect m_rect;
	CPtrListSingleLink m_DataItems;
	CQuadTreeNode* mp_Children[NUM_CHILDREN];
	s32 m_Depth;
	
protected:
	bool InSector(const fwRect& bb, s32 sectorIndex);
	s32 FindSector(const fwRect& bb);
	s32 FindSector(const Vector3& Position);
};
	
#endif //_QUADTREE_H_
