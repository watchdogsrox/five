/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetBase.h
// PURPOSE : base class for debug ui gadgets, to be attached in a tree structure
// AUTHOR :  Ian Kiigan
// CREATED : 22/11/10
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIGADGETBASE_H_
#define _DEBUG_UIGADGET_UIGADGETBASE_H_

#if __BANK

#include "fwmaths/Rect.h"
#include "fwutil/PtrList.h"
#include "grcore/viewport.h"
#include "vector/vector2.h"

#include "debug/UiGadget/UiEvent.h"

#define UIGADGET_SCREEN_COORDS(x,y)	Vector2((float)(x)/grcViewport::GetDefaultScreen()->GetWidth(), (float)(y)/grcViewport::GetDefaultScreen()->GetHeight())

// base class for all gadgets
class CUiGadgetBase
{
public:
	CUiGadgetBase();
	virtual ~CUiGadgetBase()									{ DetachAllChildren(); }

	void ImmediateDrawAll();
	void DrawAll();
	void UpdateAll();

	inline bool IsActive()										{ return m_bActive; }
	virtual void SetActive(bool bActive)						{ m_bActive = bActive; }
	void SetChildrenActive( bool bActive );
    u32 GetChildCount() const { return m_childList.CountElements(); }

	// parent and children links
	void DetachAllChildren();
    void DetachFromParent();
    void AttachToParent(CUiGadgetBase* pParent);
    bool IsAttached( CUiGadgetBase const& child ) const;
    void AttachChild(CUiGadgetBase* pChild);
	inline CUiGadgetBase* GetParent() const						{ return m_pParent; }
	inline void DetachChild(CUiGadgetBase* pChild)				{ m_childList.Remove(pChild); pChild->SetParent(NULL); }
    CUiGadgetBase* TryGetChildByIndex( int const index );

	// position, bounds etc
    inline Vector2 GetPos() const								{ return m_vPos; }
    virtual fwBox2D GetBounds() const = 0;
	void AddTreeBounds(fwBox2D& bb);
	void SetPos(const Vector2& vNewPos, bool bMoveChildren=false);

	// event handling
	void PropagateEvent(const CUiEvent& uiEvent);
	bool ContainsMouse();
	virtual void HandleEvent(const CUiEvent& UNUSED_PARAM(uiEvent))	{ }
	inline void SetCanHandleEventsOfType(eUiEventType eEventType) { m_handledEventsMask |= CUiEvent::GetTypeMask(eEventType); }
	inline bool HasFocus() { return m_bHasFocus; }
	inline void SetHasFocus(bool bFocus) { m_bHasFocus = bFocus; }
    void SetNewChildFocus(CUiGadgetBase* pNewFocus);

	// tbd
	enum
	{
		ALIGN_TOP_LEFT = 0,
		ALIGN_TOP,
		ALIGN_TOP_RIGHT,
		ALIGN_LEFT,
		ALIGN_CENTRE,
		ALIGN_RIGHT,
		ALIGN_BOTTOM_LEFT,
		ALIGN_BOTTOM,
		ALIGN_BOTTOM_RIGHT
	};
	
protected:
	virtual void ImmediateDraw() {};

	// must be implemented by subclass
	virtual void Draw() = 0;
	virtual void Update() = 0;

	Vector2 m_vPos;
	u32 m_alignment;

private:
	inline void SetParent(CUiGadgetBase* pParent)				{ m_pParent = pParent; }
	void ApplyMovement(const Vector2& vDelta);

	bool m_bActive;
	bool m_bHasFocus;
	CUiGadgetBase* m_pParent;
	fwPtrListSingleLink m_childList;
	u32 m_handledEventsMask;
};

// dummy widget that does no drawing
class CUiGadgetDummy : public CUiGadgetBase
{
	virtual void Draw() {}
	virtual void Update() {}
	virtual fwBox2D GetBounds() const { return fwBox2D( GetPos() ); }
};

extern CUiGadgetDummy g_UiGadgetRoot;

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIGADGETBASE_H_

