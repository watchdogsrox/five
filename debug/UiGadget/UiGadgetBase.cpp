/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetBase.cpp
// PURPOSE : base class for debug ui gadgets, to be attached in a tree structure
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiGadgetBase.h"

#if __BANK

CUiGadgetDummy g_UiGadgetRoot;

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	CUiGadgetBase
// PURPOSE:		ctor
//////////////////////////////////////////////////////////////////////////
CUiGadgetBase::CUiGadgetBase()
: m_alignment(ALIGN_TOP_LEFT), m_bActive(true), m_pParent(NULL), m_handledEventsMask(0)
{
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ImmediateDrawAll
// PURPOSE:		if active, recursively draw self supporting immediate draw (not batched)
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::ImmediateDrawAll()
{
    if (IsActive())
    {
        ImmediateDraw();
    }
    fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
    while (pNode)
    {
        CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
        if (pChild && pChild->IsActive())
        {
            pChild->ImmediateDrawAll();
        }
        pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
    }
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DrawAll
// PURPOSE:		if active, recursively draw self and all attached gadgets (batched)
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::DrawAll()
{
	if (IsActive())
	{
		Draw();
	}
	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild && pChild->IsActive())
		{
			pChild->DrawAll();
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	UpdateAll
// PURPOSE:		if active, recursively update self and all attached gadgets
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::UpdateAll()
{
	if (IsActive())
	{
		Update();
	}
	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild && pChild->IsActive())
		{
			pChild->UpdateAll();
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
}

void CUiGadgetBase::SetChildrenActive( bool bActive )
{
	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild )
		{
			pChild->SetActive( bActive );
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DetachFromParent
// PURPOSE:		detach self from parent gadget
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::DetachFromParent()
{
	Assertf(GetParent()!=NULL, "Not currently attached to parent");
	if (m_pParent)
	{
		m_pParent->DetachChild(this);
		m_pParent = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	DetachAllChildren
// PURPOSE:		detaches all attached children
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::DetachAllChildren()
{
	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild)
		{
			pChild->SetParent(NULL);
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
	m_childList.Flush();
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AttachToParent
// PURPOSE:		attach to parent gadget
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::AttachToParent(CUiGadgetBase* pParent)
{
	Assertf(GetParent()==NULL, "Already attached to parent");
	if (pParent)
	{
		pParent->AttachChild(this);
		m_pParent = pParent;
	}
}

bool CUiGadgetBase::IsAttached( CUiGadgetBase const& child ) const
{
    bool attached = false;

    fwPtrNodeSingleLink const * pNode = m_childList.GetHeadPtr();
    while( attached == false && pNode )
    {
        CUiGadgetBase const * const c_currentChild = (CUiGadgetBase*) pNode->GetPtr();
        attached = ( c_currentChild == &child );
        pNode = (fwPtrNodeSingleLink const*) pNode->GetNextPtr();
    }

    return attached;
}

void CUiGadgetBase::AttachChild( CUiGadgetBase* pChild )
{
    if( pChild )
    {
        if( !IsAttached( *pChild ) )
        {
            m_childList.Add( pChild );
            pChild->SetParent(this);
        }
    }
}

CUiGadgetBase* CUiGadgetBase::TryGetChildByIndex( int const index )
{
    CUiGadgetBase* result = nullptr;
    int currentIndex = 0;

    fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
    while (pNode)
    {
        CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
        if( pChild && currentIndex == index )
        {
            result = pChild;
            break;
        }
        pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
        ++currentIndex;
    }

    return result;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetPos
// PURPOSE:		sets new position for self, and optionally all attached children (and so on)
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::SetPos(const Vector2& vNewPos, bool bMoveChildren/*=false*/)
{
	if (bMoveChildren)
	{
		ApplyMovement(vNewPos - GetPos());
	}

    m_vPos = vNewPos;
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ApplyMovement
// PURPOSE:		translation by specified delta, and recursively applies same to children
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::ApplyMovement(const Vector2& vDelta)
{
	m_vPos += vDelta;

	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild)
		{
			pChild->ApplyMovement(vDelta);
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	AddTreeBounds
// PURPOSE:		recursively adds bounds to specified box, useful for finding overall dimensions of gadget tree
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::AddTreeBounds(fwBox2D& bb)
{
	bb.Grow(GetBounds());

	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild)
		{
			pChild->AddTreeBounds(bb);
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	ContainsMouse
// PURPOSE:		returns true if current position of mouse is within bounds
//////////////////////////////////////////////////////////////////////////
bool CUiGadgetBase::ContainsMouse()
{
	Vector2 vMousePos = g_UiEventMgr.GetCurrentMousePos();
	fwBox2D bounds = GetBounds();
	if (bounds.IsInside(vMousePos))
	{
		return true;
	}

	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild->ContainsMouse())
		{
			return true;
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
	return false;	
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	PropagateEvent
// PURPOSE:		propagate event down hierarchy to be handled by all interested gadgets
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::PropagateEvent(const CUiEvent& uiEvent)
{
	if (uiEvent.IsInteresting(m_handledEventsMask))
	{
		HandleEvent(uiEvent);
	}

	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild)
		{
			pChild->PropagateEvent(uiEvent);
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
}

//////////////////////////////////////////////////////////////////////////
// FUNCTION:	SetNewChildFocus
// PURPOSE:		sets all children to not have focus, except the one specified
//////////////////////////////////////////////////////////////////////////
void CUiGadgetBase::SetNewChildFocus(CUiGadgetBase* pNewFocus)
{
	fwPtrNodeSingleLink* pNode = m_childList.GetHeadPtr();
	while (pNode)
	{
		CUiGadgetBase* pChild = (CUiGadgetBase*) pNode->GetPtr();
		if (pChild)
		{
			pChild->SetHasFocus(pChild == pNewFocus);
		}
		pNode = (fwPtrNodeSingleLink*) pNode->GetNextPtr();
	}
}

#endif	//__BANK
