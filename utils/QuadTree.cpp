// Title	:	QuadTree.h
// Author	:	Greg Smith
// Started	:	18/11/03

#include "quadtree.h"
#include "debug/vectormap.h"
#if __DEV
//	#pragma optimize ("", off)
#endif

FW_INSTANTIATE_CLASS_POOL(CQuadTreeNode, 800, "QuadTreeNodes");

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : CQuadTreeNode
// PURPOSE :  sets up the dimensions for this node and create any child nodes
/////////////////////////////////////////////////////////////////////////////////
CQuadTreeNode::CQuadTreeNode(const fwRect& bb,s32 Depth):
	m_rect(bb),
	m_Depth(Depth)
{
	for(s32 i = 0;i<NUM_CHILDREN;i++)
	{
		mp_Children[i] = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ~CQuadTreeNode
// PURPOSE :  deletes the child nodes
/////////////////////////////////////////////////////////////////////////////////
CQuadTreeNode::~CQuadTreeNode()
{
	Assert((m_DataItems.GetHeadPtr() == NULL));
	
	for(s32 i = 0;i<NUM_CHILDREN;i++)
	{
		if(mp_Children[i])
		{
			delete mp_Children[i];
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetAll
// PURPOSE :  gets every pointer stored in this and any child nodes
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::GetAll(CPtrListSingleLink& Matches)
{
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();
	
	//pointers in this node
	
	while(p_LinkNode)
	{
		Matches.Add(p_LinkNode->GetPtr());
		
		p_LinkNode = p_LinkNode->GetNextPtr();
	}
	
	//all the children
	
	for(s32 i=0;i<NUM_CHILDREN;i++)
	{
		if(mp_Children[i])
		{
			mp_Children[i]->GetAll(Matches);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetAll
// PURPOSE :  gets every pointer stored in this and any child nodes
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::GetAllNoDupes(CPtrListSingleLink& Matches)
{
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();
	
	//pointers in this node
	
	while(p_LinkNode)
	{
		if (!Matches.IsMemberOfList(p_LinkNode->GetPtr())){
			Matches.Add(p_LinkNode->GetPtr());
		}
		
		p_LinkNode = p_LinkNode->GetNextPtr();
	}
	
	//all the children
	
	for(s32 i=0;i<NUM_CHILDREN;i++)
	{
		if(mp_Children[i])
		{
			mp_Children[i]->GetAllNoDupes(Matches);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetAllMatching
// PURPOSE :  gets every pointer that matches the position passed in
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::GetAllMatching(const fwRect& bb,CPtrListSingleLink& Matches)
{
	//add each of the items that are in this node
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();
	
	while(p_LinkNode)
	{
		Matches.Add(p_LinkNode->GetPtr());
		
		p_LinkNode = p_LinkNode->GetNextPtr();
	}

	for(s32 i=0; i<4; i++)
	{
		if(mp_Children[i] && InSector(bb, i))
		{
			mp_Children[i]->GetAllMatching(bb, Matches);
		}
	}
/*	s32 ChildIndex = FindSector(bb);
	
	if(ChildIndex == -1)
	{
		//if there is no matching child node then we will
		//need to add all the children because they could be possible
		//matches
		for(s32 i=0;i<NUM_CHILDREN;i++)
		{
			if(mp_Children[i])
			{
				mp_Children[i]->GetAll(Matches);
			}
		}
	}
	else
	{
		//and if there is a matching child node then add the items from this node
		if(mp_Children[ChildIndex])
		{
			mp_Children[ChildIndex]->GetAllMatching(bb,Matches);
		}
	}*/
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : GetAllMatching
// PURPOSE :  gets every pointer that matches the position passed in
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::GetAllMatching(const Vector3& position, CPtrListSingleLink& matches)
{
	//add each of the items that are in this node
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();
	
	while(p_LinkNode)
	{
		matches.Add(p_LinkNode->GetPtr());
		
		p_LinkNode = p_LinkNode->GetNextPtr();
	}

	s32 ChildIndex = FindSector(position);
	
	if(ChildIndex != -1)
	{
		//and if there is a matching child node then add the items from this node
		if(mp_Children[ChildIndex])
		{
/*
			CVectorMap::DrawLine(Vector3(mp_Children[ChildIndex]->m_rect.left,mp_Children[ChildIndex]->m_rect.top,0.0f),Vector3(mp_Children[ChildIndex]->m_rect.right,mp_Children[ChildIndex]->m_rect.top,0.0f),Color_red,Color_red);
			CVectorMap::DrawLine(Vector3(mp_Children[ChildIndex]->m_rect.left,mp_Children[ChildIndex]->m_rect.bottom,0.0f),Vector3(mp_Children[ChildIndex]->m_rect.right,mp_Children[ChildIndex]->m_rect.bottom,0.0f),Color_red,Color_red);
			CVectorMap::DrawLine(Vector3(mp_Children[ChildIndex]->m_rect.right,mp_Children[ChildIndex]->m_rect.top,0.0f),Vector3(mp_Children[ChildIndex]->m_rect.right,mp_Children[ChildIndex]->m_rect.bottom,0.0f),Color_red,Color_red);
			CVectorMap::DrawLine(Vector3(mp_Children[ChildIndex]->m_rect.left,mp_Children[ChildIndex]->m_rect.top,0.0f),Vector3(mp_Children[ChildIndex]->m_rect.left,mp_Children[ChildIndex]->m_rect.bottom,0.0f),Color_red,Color_red);
*/
			mp_Children[ChildIndex]->GetAllMatching(position, matches);
		}
	}
}


//
// name:		CQuadTreeNode::ForAllMatching
// description:	Run function for all entries in the quadtree that are in sectors that intersect with the supplied box
void CQuadTreeNode::ForAllMatching(const fwRect& bb, QuadTreeFn& fn)
{
	//add each of the items that are in this node
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();

	// call function for all link nodes in this sector
	while(p_LinkNode)
	{
		fn(bb, p_LinkNode->GetPtr());
		p_LinkNode = p_LinkNode->GetNextPtr();
	}

	// check all the children nodes
	for(s32 i=0; i<4; i++)
	{
		if(mp_Children[i] && InSector(bb, i))
		{
			mp_Children[i]->ForAllMatching(bb, fn);
		}
	}
}

//
// name:		CQuadTreeNode::ForAllMatching
// description:	Run function for all entries in the quadtree that are in sectors that intersect with the supplied point
void CQuadTreeNode::ForAllMatching(const Vector3& posn, QuadTreeFn& fn)
{
	//add each of the items that are in this node
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();

	while(p_LinkNode)
	{
		fn(posn, p_LinkNode->GetPtr());
		p_LinkNode = p_LinkNode->GetNextPtr();
	}

	s32 ChildIndex = FindSector(posn);

	if(ChildIndex != -1)
	{
		//and if there is a matching child node then add the items from this node
		if(mp_Children[ChildIndex])
		{
			mp_Children[ChildIndex]->ForAllMatching(posn, fn);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ForAllMatching
// PURPOSE :  runs a function for every pointer that matches the bounding box passed in.
//			this is the old skool version of the function. Everyone should be using the 
//			version which takes a function class
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::ForAllMatching(const fwRect& bb, CQuadTreeNode::RectCB fn)
{
	class CForAllMatchingFnPointer : public QuadTreeFn
	{
	public:
		CForAllMatchingFnPointer(RectCB fn) : m_fnPointer(fn) {}
		void operator()(const fwRect& bb, void* data) {m_fnPointer(bb, data);}

		RectCB m_fnPointer;
	};

	CForAllMatchingFnPointer callback(fn);
	ForAllMatching(bb, callback);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : ForAllMatching
// PURPOSE :  runs a function for every pointer that matches the bounding box passed in
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::ForAllMatching(const Vector3& posn, CQuadTreeNode::PosnCB fn)
{
	class CForAllMatchingFnPointer : public QuadTreeFn
	{
	public:
		CForAllMatchingFnPointer(PosnCB fn) : m_fnPointer(fn) {}
		void operator()(const Vector3& posn, void* data) {m_fnPointer(posn, data);}

		PosnCB m_fnPointer;
	};

	CForAllMatchingFnPointer callback(fn);
	ForAllMatching(posn, callback);
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindSector
// PURPOSE :  finds a matching child quad sector for a position and size
//			  (-1 if overlaps more than one sector)
/////////////////////////////////////////////////////////////////////////////////
s32 CQuadTreeNode::FindSector(const fwRect& bb)
{
	float centreX = (m_rect.left + m_rect.right) / 2.0f;
	float centreY = (m_rect.bottom + m_rect.top) / 2.0f;
	bool bTop;
//	s32 ChildIndex = -1;
	
	//if the node shouldnt have any children then add it anyway
	if(m_Depth == 0)
	{
		return -1;
	}
		
	//test the y axis
	if(bb.top < centreY)
	{
		bTop = false;
	}
	else if(bb.bottom > centreY)
	{
		bTop = true;
	}
	else
	{
		//we are straddling this axis so just add it to this node
		return -1;
	}

	//test the x axis	
	if(bb.right < centreX)
	{
		if(bTop)
		{
			return 0;
		}
		else
		{
			return 2;
		}
	}
	else if(bb.left > centreX)
	{
		if(bTop)
		{
			return 1;
		}
		else
		{
			return 3;
		}
	}

	//we are straddling this axis so just add it to this node
	return -1;
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindSector
// PURPOSE :  finds a matching child quad sector for a position and size
//			  (-1 if overlaps more than one sector)
/////////////////////////////////////////////////////////////////////////////////
s32 CQuadTreeNode::FindSector(const Vector3& Position)
{
	float centreX = (m_rect.left + m_rect.right) / 2.0f;
	float centreY = (m_rect.bottom + m_rect.top) / 2.0f;
	bool bTop;
//	s32 ChildIndex = -1;
	
	//if the node shouldnt have any children then add it anyway
	if(m_Depth == 0)
	{
		return -1;
	}
		
	//test the y axis
	if((Position.y < centreY))
		bTop = false;
	else
		bTop = true;

	//test the x axis	
	if(Position.x < centreX)
	{
		if(bTop)
		{
			return 0;
		}
		else
		{
			return 2;
		}
	}
	else
	{
		if(bTop)
		{
			return 1;
		}
		else
		{
			return 3;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : FindSector
// PURPOSE :  finds a matching child quad sector for a position and size
//			  (-1 if overlaps more than one sector)
/////////////////////////////////////////////////////////////////////////////////
bool CQuadTreeNode::InSector(const fwRect& bb, s32 sectorIndex)
{
	fwRect newBB = m_rect;
	
	if(m_Depth == 0)
	{
		return false;
	}
	
	switch(sectorIndex)
	{
	case TOP_LEFT:
		newBB.right = (newBB.right + newBB.left) / 2.0f;
		newBB.bottom = (newBB.top + newBB.bottom) / 2.0f;
		break;
	case TOP_RIGHT:
		newBB.left = (newBB.right + newBB.left) / 2.0f;
		newBB.bottom = (newBB.top + newBB.bottom) / 2.0f;
		break;
	case BOTTOM_LEFT:
		newBB.right = (newBB.right + newBB.left) / 2.0f;
		newBB.top = (newBB.top + newBB.bottom) / 2.0f;
		break;
	case BOTTOM_RIGHT:
		newBB.left = (newBB.right + newBB.left) / 2.0f;
		newBB.top = (newBB.top + newBB.bottom) / 2.0f;
		break;
	}
	
	if(newBB.DoesIntersect(bb))
		return true;
		
	return false;
}


/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : AddItem
// PURPOSE :  stores a new pointer in the quadtree at the optimal position
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::AddItem(void* p_vData, const fwRect& bb)
{
	if(m_Depth == 0)
	{
		m_DataItems.Add(p_vData);
		return;
	}
	
	for(s32 i=0; i<4; i++)
	{
		if(InSector(bb, i))
		{
			if(!mp_Children[i])
			{
				fwRect newBB = m_rect;
			
				switch(i)
				{
				case TOP_LEFT:
					newBB.right = (newBB.right + newBB.left) / 2.0f;
					newBB.bottom = (newBB.top + newBB.bottom) / 2.0f;
					break;
				case TOP_RIGHT:
					newBB.left = (newBB.right + newBB.left) / 2.0f;
					newBB.bottom = (newBB.top + newBB.bottom) / 2.0f;
					break;
				case BOTTOM_LEFT:
					newBB.right = (newBB.right + newBB.left) / 2.0f;
					newBB.top = (newBB.top + newBB.bottom) / 2.0f;
					break;
				case BOTTOM_RIGHT:
					newBB.left = (newBB.right + newBB.left) / 2.0f;
					newBB.top = (newBB.top + newBB.bottom) / 2.0f;
					break;
				}
				
				mp_Children[i] = rage_new CQuadTreeNode(newBB, m_Depth - 1);			
			}
			
			mp_Children[i]->AddItem(p_vData, bb);
		}
	}	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DeleteItem
// PURPOSE :  removes an item from the quadtree
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::DeleteItem(void* p_vData)
{
	//if its in this node then delete it
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();
	bool bFound = false;
	
	while(p_LinkNode)
	{
		if(p_LinkNode->GetPtr() == p_vData)
		{
			Assert(!bFound);
			bFound = true;
			break;
		}
		
		p_LinkNode = p_LinkNode->GetNextPtr();
	}
	
	if(bFound)
	{
		m_DataItems.Remove(p_vData);
	}
	
	//then look in all the children
	for(s32 i = 0;i < NUM_CHILDREN;i++)
	{
		if(mp_Children[i])
		{	
			mp_Children[i]->DeleteItem(p_vData);
		}
	}
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DeleteItem
// PURPOSE :  removes an item from the quadtree
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::DeleteItem(void* p_vData, const fwRect& bb)
{
	//if its in this node then delete it
	CPtrNode* p_LinkNode = m_DataItems.GetHeadPtr();
	bool bFound = false;
	
	while(p_LinkNode)
	{
		if(p_LinkNode->GetPtr() == p_vData)
		{
			Assert(!bFound);
			bFound = true;
			break;
		}
		
		p_LinkNode = p_LinkNode->GetNextPtr();
	}
	
	if(bFound)
	{
		m_DataItems.Remove(p_vData);
	}
	
	for(s32 i=0; i<4; i++)
	{
		if(InSector(bb, i))
		{
			mp_Children[i]->DeleteItem(p_vData, bb);
		}
	}	
}

/////////////////////////////////////////////////////////////////////////////////
// FUNCTION : DeleteAllItems
// PURPOSE :  removes all items from the quadtree
/////////////////////////////////////////////////////////////////////////////////
void CQuadTreeNode::DeleteAllItems(void)
{
	m_DataItems.Flush();

	for(s32 i=0; i<4; i++)
	{
		if(mp_Children[i])
			mp_Children[i]->DeleteAllItems();
	}	
}

