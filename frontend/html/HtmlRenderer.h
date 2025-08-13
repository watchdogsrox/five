//
// filename:	HtmlRenderer.h
// description:	
//

#ifndef INC_HTMLRENDERER_H_
#define INC_HTMLRENDERER_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "vector/color32.h"
// Game headers
#include "HtmlLanguage.h"
#include "HtmlRenderState.h"
#include "scene/stores/TxdStore.h"
#include "text/text.h"

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

namespace rage {
	class parElement;
	class fwRect;
}

class CHtmlDocument;
class CHtmlNode;
class CHtmlElementNode;
class CHtmlDataNode;
struct HtmlRenderState;

class CHtmlRenderer
{
public:
	CHtmlRenderer() : m_pDocument(NULL), m_scroll(0.0f) {};
	~CHtmlRenderer() {}

	void SetDocument(CHtmlDocument* pDocument);
	CHtmlDocument* GetDocument() { return m_pDocument;}
	void Render();

	void RenderImage(CHtmlElementNode* pNode);
	void RenderFlash(CHtmlElementNode* pNode);
	void RenderText(CHtmlNode* pNode, float x, float y, GxtChar* pTextStart, GxtChar* pTextEnd, float length);

	void SetScroll(float scroll) {m_scroll = scroll;}
	float GetScroll() {return m_scroll;}

	void SetScreenSize(float width, float height) {m_screenWidth = width; m_screenHeight = height;}

private:
	static void Static_PreRender(float screenWidth);
	static void Static_PostRender(void);

	static void Static_RenderBgnd(float width, float height, fwRect& drawRect, HtmlRenderState& rs, bool bHTMLBody);
	static void Static_RenderSprite(grcTexture* pImage, fwRect& rect, fwRect& uvRect, Color32& col);
	static void Static_RenderBorder(HtmlRenderState& rs, float top, float bottom, float left, float right, u32 numBorders);
	static void Static_RenderTextDecoration(Color32& color, float x1, float x2, float y);

	void RenderNode(CHtmlNode* pNode);
	void RenderElement(CHtmlElementNode* pNode);
	void PostRenderElement(CHtmlElementNode* pNode);
	void RenderData(CHtmlDataNode* pNode);
	void RenderInlineObjectList();

	// Render special elements
	void RenderBackground(CHtmlNode* pNode);
	void RenderBorder(CHtmlNode* pNode);
	void RenderBody(CHtmlElementNode* pNode);
	void RenderTable(CHtmlElementNode* pNode);
	void RenderTableEntry(CHtmlElementNode* pNode);

	void ProcessAttributes(parElement& element, HtmlRenderState& renderState);

	atArray<HtmlRenderState> m_renderStateStack;
	atArray<HtmlElementTag> m_elementStack;
	CHtmlDocument* m_pDocument;
	float m_scroll;
	float m_screenWidth;
	float m_screenHeight;
};

// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLRENDERER_H_
