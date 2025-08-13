/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetImage.h
// PURPOSE : A UI Gadget to draw a texture/image to the screen
// AUTHOR :  James Strain
// CREATED : 13/12/2013
//
/////////////////////////////////////////////////////////////////////////////////

#ifndef _DEBUG_UIGADGET_UIGADGETIMAGE_H_
#define _DEBUG_UIGADGET_UIGADGETIMAGE_H_

#if __BANK

#include "debug/UiGadget/UiGadgetBox.h"
#include "vector/color32.h"

namespace rage
{
	class grcTexture;
}

class CUiGadgetImage : public CUiGadgetBox
{
public:
	CUiGadgetImage() 
		: CUiGadgetBox()
		, m_texture( NULL )
	{}

	CUiGadgetImage(float fX, float fY, float fWidth, float fHeight, const Color32& colour )
		: CUiGadgetBox( fX, fY, fWidth, fHeight, colour )
		, m_texture( NULL )
	{

	}

	CUiGadgetImage( grcTexture const* texture, float fX, float fY, float fWidth, float fHeight, const Color32& colour )
		: CUiGadgetBox( fX, fY, fWidth, fHeight, colour )
		, m_texture( texture )
	{

	}

	inline void setTexture( grcTexture const * texture ) { m_texture = texture; }
	inline grcTexture const * getTexture() const { return m_texture; }

protected:
	virtual void ImmediateDraw();

private:
	grcTexture const* m_texture;
};

#endif	//__BANK

#endif	//_DEBUG_UIGADGET_UIGADGETIMAGE_H_
