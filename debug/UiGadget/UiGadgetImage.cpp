/////////////////////////////////////////////////////////////////////////////////
//
// FILE :    debug/UiGadget/UiGadgetImage.cpp
// PURPOSE : A UI Gadget to draw a texture/image to the screen
// AUTHOR :  James Strain
// CREATED : 13/12/2013
//
/////////////////////////////////////////////////////////////////////////////////

#include "debug/UiGadget/UiGadgetImage.h"

#if __BANK

#include "grcore/debugdraw.h"
#include "grcore/quads.h"

void CUiGadgetImage::ImmediateDraw()
{
	if( m_texture )
	{
		fwBox2D const rect = GetBounds();

		grcBindTexture( m_texture );
		grcDrawSingleQuadf( rect.x0, rect.y0, rect.x1, rect.y1, 0.f, 0.f, 0.f, 1.f, 1.f, Color32(255,255,255,255) );
		grcBindTexture(NULL);
	}
}

#endif	//__BANK
