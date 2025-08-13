//
// filename:	HtmlRenderer.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "HtmlRenderer.h"

// C headers
// Rage headers
#include "file/asset.h"
#include "grcore/im.h"
#include "grcore/state.h"
#include "grcore/viewport.h"
#include "parsercore/attribute.h"
#include "parsercore/element.h"
// Game headers
#include "camera/viewports/Viewport.h"
#include "HtmlDocument.h"
#include "HtmlNode.h"
#include "HtmlRenderer.h"
#include "HtmlRenderState.h"
#include "HtmlTextFormat.h"
#include "htmlviewport.h"
#include "renderer/DrawLists/DrawListMgr.h"
#include "Text/Text.h"
#include "Text/TextDraw.h"
#include "Text/TextFormat.h"
#include "renderer/sprite2d.h"
#include "system/controlmgr.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// Declare an input object; the player watches this
// object for pad events.
//swfINPUT gInput(1);

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

static bool gInBody;

// --- CHtmlRenderer ------------------------------------------------------------

void CHtmlRenderer::SetDocument(CHtmlDocument* pDocument)
{
	m_pDocument = pDocument;
}

void CHtmlRenderer::Static_PreRender(float screenWidth){
	CSprite2d::OverrideBlitMatrix(*grcViewport::GetCurrent());//-screenWidth*1.0f/6.0f, screenWidth*7.0f/6.0f, 0.0f, screenHeight);//screenWidth * 4.0f/3.0f, screenHeight * 4.0f/3.0f);

#ifndef HTML_RESOURCE
	// default font renderer
	CHtmlDocument::sm_HtmlTextLayout.SetBackground(false);
	CHtmlDocument::sm_HtmlTextLayout.SetEdge(0);
	CHtmlDocument::sm_HtmlTextLayout.SetProportional(true);
	CHtmlDocument::sm_HtmlTextLayout.SetWrap(Vector2(-1.0f/screenWidth, 2.0f/screenWidth));
#else
	screenWidth = 0.0f;
#endif
	// default render state
	grcState::Default();
	grcState::SetAlphaTest(false);
	grcState::SetDepthTest(false);
	grcState::SetCullMode(grccmNone);
	grcState::SetAlphaBlend(true);
	grcState::SetBlendSet(grcbsNormal);
}

void CHtmlRenderer::Static_PostRender(void){
	// stop sprite2d referencing textures
	CSprite2d::ClearRenderState();
	CSprite2d::ResetBlitMatrix();
}

void CHtmlRenderer::Render()
{
	gInBody = false;

	if (gDrawListMgr.IsBuildingDrawList()){
		DLC_Add ( Static_PreRender, m_screenWidth );
	} else {
		Static_PreRender(m_screenWidth);
	}

	if(m_pDocument)
		RenderNode(m_pDocument->GetRootNode());

	if (gDrawListMgr.IsBuildingDrawList()){
		DLC_Add ( Static_PostRender );

		// reference all the txd's used
		for(s32 i=0; i<CHtmlDocument::GetTxdIndexArray().GetCount(); i++)
		{
			gDrawListMgr.AddTxdReference(CHtmlDocument::GetTxdIndexArray()[i]);
		}
	} else {
		Static_PostRender();
	}
}

void CHtmlRenderer::RenderNode(CHtmlNode* pNode)
{
	if(pNode == NULL)
		return;

	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);

	if(pNode->GetRenderState().m_display != HTML_INLINE)
		RenderInlineObjectList();

	if(pNode->GetNumChildren() > 0 || pNode->GetRenderState().m_display != HTML_INLINE)
	{
		Assert(pElement);
		RenderElement(pElement);

		// process children nodes
		for(s32 i=0; i<pNode->GetNumChildren(); i++)
			RenderNode(pNode->GetChild(i));

		if(pNode->GetRenderState().m_display != HTML_INLINE)
			RenderInlineObjectList();

		// function to call once an element and its children have been drawn
		PostRenderElement(pElement);
	}
	// otherwise this is an inline object, add it to the list
	else
		m_pDocument->AddInlineObject(pNode);
}

void CHtmlRenderer::RenderElement(CHtmlElementNode* pElementNode)
{
	// push render state
//	HtmlRenderState state(m_renderStateStack.Top());
//	m_renderStateStack.PushAndGrow(state);

	s32 elementId = pElementNode->m_elementId;

//	ProcessAttributes(pElementNode->m_element, m_renderStateStack.Top());
	switch(elementId)
	{
	case HTML_BODY:
		RenderBody(pElementNode);
		break;

	case HTML_TABLE:
		RenderTable(pElementNode);
		break;

	case HTML_IMAGE:
		RenderImage(pElementNode);
		break;

	default:
		if(pElementNode->GetRenderState().m_display != HTML_INLINE)
		{
			RenderBackground(pElementNode);
			RenderBorder(pElementNode);
		}
		break;
	}
}

void CHtmlRenderer::PostRenderElement(CHtmlElementNode* pElementNode)
{
	switch(pElementNode->m_elementId)
	{
	case HTML_BODY:
		gInBody = false;
		break;

	default:
		break;
	}
}

// code to render background using given params (can be used safely as a callback for RT)
void CHtmlRenderer::Static_RenderBgnd(float width, float height, fwRect& drawRect, HtmlRenderState& rs, bool bHTMLBody){

	//if(pElement->GetElementId() == HTML_BODY){
	if (bHTMLBody){
		CSprite2d::ResetBlitMatrix();
	}

	grcState::SetAlphaBlend(false);
	// if background image
	if(rs.m_pBackgroundImage)
	{
		CSprite2d backGrdSprite(rs.m_pBackgroundImage);
		float imgWidth = static_cast<float>(rs.m_pBackgroundImage->GetWidth());
		float imgHeight = static_cast<float>(rs.m_pBackgroundImage->GetHeight());
		fwRect uvRect(0.0f,0.0f, width/imgWidth, height/imgHeight);

		backGrdSprite.SetRenderState();
		backGrdSprite.Draw(drawRect, uvRect, Color32(255,255,255));
	}
	// else use bg colour
	else
	{
		CSprite2d::DrawRect(drawRect, rs.m_backgroundColor);
	}
	grcState::SetAlphaBlend(true);

	//if(pElement->GetElementId() == HTML_BODY)
	if (bHTMLBody){
		CSprite2d::OverrideBlitMatrix(*grcViewport::GetCurrent());
	}
}

void CHtmlRenderer::RenderBackground(CHtmlNode* pNode)
{
	HtmlRenderState& rs = pNode->GetRenderState();
	if(rs.m_renderBackground)
	{
		float x = rs.m_currentX;
		float y = rs.m_currentY;
		float width = rs.m_width;
		float height = rs.m_height;

		Assert(rs.m_width >= 0.0f);
		Assert(rs.m_height >= 0.0f);

		CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
		// BODY is a special case as the margins are used when rendering background
		if(pElement->GetElementId() == HTML_BODY)
		{
			pElement->GetParent()->GetDimensions(x,y, width, height);
		}
		else
		{
			// if the width is set on this node
			if(rs.m_width >= 0.0f)
			{
				x -= rs.m_marginLeft;
				width += rs.m_marginLeft + rs.m_marginRight;
			}
			// if the height is set on this node
			if(rs.m_height >= 0.0f)
			{
				y -= rs.m_marginTop;
				height += rs.m_marginTop + rs.m_marginBottom;
			}
			y -= m_scroll;
		}

		fwRect drawRect((x/m_screenWidth), (y/m_screenHeight), ((x+width)/m_screenWidth), ((y+height)/m_screenHeight));

		bool bHTMLBody = (pElement->GetElementId() == HTML_BODY);

		if (gDrawListMgr.IsBuildingDrawList()){
			DLC_Add( Static_RenderBgnd, width, height, drawRect, rs, bHTMLBody);
		} else {
			Static_RenderBgnd(width, height, drawRect, rs, bHTMLBody);
		}
	}
}

void CHtmlRenderer::Static_RenderBorder(HtmlRenderState& rs, float top, float bottom, float left, float right, u32 numBorders){

	grcBindTexture(NULL);
	grcWorldIdentity();

	grcBegin(drawLines, numBorders * 2);

	if(rs.m_borderTopStyle == HTML_SOLID)
	{
		grcColor(rs.m_borderTopColor);
		grcVertex2f(left, top);
		grcVertex2f(right, top);
	}
	if(rs.m_borderRightStyle == HTML_SOLID)
	{
		grcColor(rs.m_borderRightColor);
		grcVertex2f(right, top);
		grcVertex2f(right, bottom);
	}
	if(rs.m_borderBottomStyle == HTML_SOLID)
	{
		grcColor(rs.m_borderBottomColor);
		grcVertex2f(right, bottom);
		grcVertex2f(left, bottom);
	}
	if(rs.m_borderTopStyle == HTML_SOLID)
	{
		grcColor(rs.m_borderLeftColor);
		grcVertex2f(left, bottom);
		grcVertex2f(left, top);
	}
	grcEnd();

}

void CHtmlRenderer::RenderBorder(CHtmlNode* pNode)
{
	HtmlRenderState& rs = pNode->GetRenderState();
	s32 numBorders = 0;

	// count borders to be rendered
	if(rs.m_borderLeftStyle == HTML_SOLID)
		numBorders++;
	if(rs.m_borderRightStyle == HTML_SOLID)
		numBorders++;
	if(rs.m_borderTopStyle == HTML_SOLID)
		numBorders++;
	if(rs.m_borderBottomStyle == HTML_SOLID)
		numBorders++;

	// If we have some borders
	if(numBorders > 0)
	{
		Assert(rs.m_width >= 0.0f);
		Assert(rs.m_height >= 0.0f);

		float bottom = ((rs.m_currentY + rs.m_height + rs.m_borderBottomWidth - 1.0f - m_scroll)/m_screenHeight);
		float left = ((rs.m_currentX - rs.m_borderLeftWidth)/m_screenWidth);
		float right = ((rs.m_currentX + rs.m_width + rs.m_borderRightWidth - 1.0f)/m_screenWidth);
		float top = ((rs.m_currentY - rs.m_borderTopWidth - m_scroll)/m_screenHeight);

		if (gDrawListMgr.IsBuildingDrawList()){
			DLC_Add( Static_RenderBorder, rs, top, bottom, left, right, numBorders);
		} else {
			Static_RenderBorder(rs, top, bottom, left, right, numBorders);
		}
	}
}

void CHtmlRenderer::RenderBody(CHtmlElementNode* pNode)
{
	gInBody = true;

	RenderBackground(pNode);
}

void CHtmlRenderer::Static_RenderTextDecoration(Color32& color, float x1, float x2, float y)
{
	grcBindTexture(NULL);
	grcBegin(drawLines, 2);

	grcColor(color);

	grcVertex2f(x1, y);
	grcVertex2f(x2, y);

	grcEnd();
}

void CHtmlRenderer::RenderText(CHtmlNode* pNode, float x, float y, GxtChar* pTextStart, GxtChar* pTextEnd, float length)
{
	if(pTextStart == pTextEnd)
		return;

	HtmlRenderState& rs = pNode->GetRenderState();
	// cache font state

	rs.SetFontProperties();

#ifndef HTML_RESOURCE
	CTextDraw::AddToRenderBuffer(&CHtmlDocument::sm_HtmlTextLayout, floorf(x)/m_screenWidth, floorf(y)/m_screenHeight, pTextStart, pTextEnd);
	CText::Flush();
#endif

	Color32 color = rs.m_color;
	s32 textDecoration = rs.m_textDecoration;

	// if node is active then get active render state
	if(rs.m_active)
	{
		color = rs.m_activeColor;
		textDecoration = rs.m_activeTextDecoration;
	}
	// draw text decoration (underline etc)
	if(textDecoration != HTML_NONE)
	{
//		float vpWidth = (float)grcViewport::GetCurrent()->GetWidth();
//		float vpHeight = (float)grcViewport::GetCurrent()->GetHeight();
		float yOffset=0.0f;

#ifndef HTML_RESOURCE
		if(textDecoration == HTML_UNDERLINE)
			yOffset += ((CTextFormat::GetCharacterHeight(CHtmlDocument::sm_HtmlTextLayout)*m_screenHeight)) * 0.9f;
		else if(textDecoration == HTML_LINETHROUGH)
			yOffset += ((CTextFormat::GetCharacterHeight(CHtmlDocument::sm_HtmlTextLayout)*m_screenHeight)) / 2.0f;
#endif		

		if(gDrawListMgr.IsBuildingDrawList()){
			DLC_Add( Static_RenderTextDecoration, color, (x/m_screenWidth), ((x+length)/m_screenWidth), ((y+yOffset)/m_screenHeight));
		}
		else
		{
			Static_RenderTextDecoration(color, (x/m_screenWidth), ((x+length)/m_screenWidth), ((y+yOffset)/m_screenHeight));
/*			grcBindTexture(NULL);
			grcBegin(drawLines, 2);

			grcColor(color);

			grcVertex2f((x/m_screenWidth), ((y+yOffset)/m_screenHeight));
			grcVertex2f(((x+length)/m_screenWidth), ((y+yOffset)/m_screenHeight));

			grcEnd();*/
		}
	}
}

// static rendering call so it can be used as a callback safely from RT
void CHtmlRenderer::Static_RenderSprite(grcTexture* pImage, fwRect& rect, fwRect& uvRect, Color32& col){

	Assert(pImage);

	CSprite2d imageSprite(pImage);
	imageSprite.SetRenderState();
	imageSprite.Draw(rect, uvRect, col);
}

void CHtmlRenderer::RenderImage(CHtmlElementNode* pElementNode)
{
	HtmlRenderState& rs = pElementNode->GetRenderState();
	//parElement& element = pElementNode->GetElement();
	grcTexture* pImage = NULL;

	// get image texture
	//parAttribute* pSrc = element.FindAttribute("src");
	pImage =pElementNode->GetTexture(m_pDocument);

	char txdName[64];
	pElementNode->GetTxdName(txdName, 64);
	if(txdName[0] != '\0' && txdName[0] != '*')
	{
		int slot = g_TxdStore.FindSlot(txdName);
		Assertf(slot != -1, "%s:Txd does not exist", txdName);
		if (gDrawListMgr.IsBuildingDrawList()){
			DLC ( CAddTxdReferenceDC(slot));
		}
	}

	if(pImage)
	{
		float left = rs.m_currentX;
		float right = rs.m_currentX + rs.m_width;
		float top = rs.m_currentY - m_scroll;
		float bottom = rs.m_currentY + rs.m_height - m_scroll;

		// draw image
		fwRect	rect((left/m_screenWidth), (top/m_screenHeight), (right/m_screenWidth), (bottom/m_screenHeight));
		fwRect	uvRect(0.0f, 0.0f, 1.0f, 1.0f);
		Color32	col(255,255,255);

		if (gDrawListMgr.IsBuildingDrawList()){
			DLC_Add( Static_RenderSprite,pImage, rect, uvRect, col);
		} else {
			Static_RenderSprite(pImage, rect, uvRect, col);
		}
	}
}

void CHtmlRenderer::RenderFlash(CHtmlElementNode* UNUSED_PARAM(pElementNode))
{
/*	CHtmlFlashNode* pFlashNode = dynamic_cast<CHtmlFlashNode*>(pElementNode);
	//HtmlRenderState& rs = pElementNode->GetRenderState();
	Assert(pFlashNode);
	Assertf(pFlashNode->GetContext(), "Failed to load flash movie");
	HtmlRenderState& rs = pElementNode->GetRenderState();
	// cache old viewport
	grcViewport* pOldVp = grcViewport::GetCurrent();

	RenderBackground(pElementNode);

	float widthRatio = pOldVp->GetWidth() / m_screenWidth;
	float heightRatio = pOldVp->GetHeight() / m_screenHeight;
	float left = rs.m_currentX * widthRatio;
	float top = (rs.m_currentY - m_scroll) * heightRatio;
	float bottom = (rs.m_currentY + rs.m_height - m_scroll) * heightRatio;
	float width = rs.m_width * widthRatio;
	float height = rs.m_height * heightRatio;
	float vpY = 0.0f;
	float vpHeight = height;

	const grcWindow& win = pOldVp->GetWindow();

	if(top < 0)
	{
		vpY -= top;
		height += top;
		top = 0.0f;
	}
	if(bottom > win.GetHeight())
	{
		vpHeight += bottom - win.GetHeight();
		height += bottom - win.GetHeight();
		bottom = (float)win.GetHeight();
	}
	// If height is less than zero then don't render
	if(height <= 0)
		return;

//	left += win.GetX();
//	top += win.GetY();

	grcViewport flashVp;
	//	flashVp.SetWindow((int)left, (int)right, (int)bottom, (int)top);
	//	flashVp.Ortho(left, right, bottom, top, __D3D? 0.0f : -1.0f, 1.0f);
	flashVp.SetWindow((int)(left/m_screenWidth)*SCREEN_WIDTH, (int)(top/m_screenHeight)*SCREEN_HEIGHT, (int)(width/m_screenWidth)*SCREEN_WIDTH, (int)(height/m_screenHeight)*SCREEN_HEIGHT);
	flashVp.Ortho(0, width, vpHeight, vpY, __D3D? 0.0f : -1.0f, 1.0f);
	grcViewport::SetCurrent(&flashVp);


	pFlashNode->GetContext()->Update(fwTimer::GetTimeStep(), false, false);
	pFlashNode->GetContext()->Draw();
	pFlashNode->GetContext()->GarbageCollect();

	grcViewport::SetCurrent(pOldVp);*/
}	
	
void CHtmlRenderer::RenderTable(CHtmlElementNode* pNode)
{
	RenderBackground(pNode);
	RenderBorder(pNode);
}

void CHtmlRenderer::RenderTableEntry(CHtmlElementNode* pNode)
{
	RenderBackground(pNode);
	RenderBorder(pNode);
}

void CHtmlRenderer::RenderInlineObjectList()
{
	// get inline object list
	//atArray<CHtmlNode*>& inlineObjs = m_pDocument->GetInlineObjectList();
	if(gInBody)
	{
		float formattedWidth, formattedHeight;
		m_pDocument->GetFormattedDimensions(formattedWidth, formattedHeight);

		if(m_pDocument->GetInlineObjectCount() > 0)
		{
			float x,y;
			float width, height;
			CHtmlNode* pNonInlineNode = m_pDocument->GetInlineObject(0)->GetNonInlineNode();

			// get dimensions from first objects parent
			pNonInlineNode->GetDimensions(x,y,width,height);

			CHtmlTextFormat fn(0, (-m_scroll), width, pNonInlineNode->GetRenderState().m_textAlign, formattedWidth, formattedHeight, this);

#ifndef HTML_RESOURCE
			CHtmlDocument::sm_HtmlTextLayout.SetWrap(Vector2(0.0f, width/m_screenWidth));
#endif

			for(s32 i=0; i<m_pDocument->GetInlineObjectCount(); i++)
			{
				CHtmlNode* pNode = m_pDocument->GetInlineObject(i);
				CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
				CHtmlDataNode* pData = dynamic_cast<CHtmlDataNode*>(pNode);

				if(pData)
				{
					HtmlRenderState& rs = pData->GetRenderState();
					rs.SetFontProperties();
					fn.SetHtmlNode(pNode);
#ifndef HTML_RESOURCE
					CHtmlDocument::sm_HtmlTextLayout.SetOrientation(FONT_LEFT);

					CTextFormat::ProcessCurrentString(&CHtmlDocument::sm_HtmlTextLayout, fn.GetCurrentX()/m_screenWidth, 0, pData->GetData(), fn);
#endif
				}
				else if(pElement)
				{
					// only know how to format images
					if(pElement->GetElementId() == HTML_IMAGE ||
						pElement->GetElementId() == HTML_EMBED)
					{
						fn.SetHtmlNode(pNode);
						fn.AddNonTextBlock(pElement->GetRenderState().m_width, pElement->GetRenderState().m_height);
					}
					else if(pElement->GetElementId() == HTML_TEXT)
					{
						HtmlRenderState& rs = pElement->GetRenderState();
						rs.SetFontProperties();
						fn.SetHtmlNode(pNode);

#ifndef HTML_RESOURCE
						CHtmlDocument::sm_HtmlTextLayout.SetOrientation(FONT_LEFT);

						CTextFormat::ProcessCurrentString(&CHtmlDocument::sm_HtmlTextLayout, fn.GetCurrentX()/m_screenWidth, 0, TheText.Get(pElement->GetAttributeData()), fn);
#endif
					}
					else if(pElement->GetElementId() == HTML_BREAK)
					{
						fn.SetHtmlNode(pNode);
#ifndef HTML_RESOURCE
						fn.AddNonTextBlock(1, (CTextFormat::GetLineHeight(CHtmlDocument::sm_HtmlTextLayout)*m_screenHeight));
#endif
						fn.NewLine();
					}
				}
			}
			fn.NewLine();
		}
	}
	// empty the inline object list at the end of this block
	m_pDocument->ResetInlineObjectList();
}

