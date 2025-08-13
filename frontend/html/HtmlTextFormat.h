//
// filename:	HtmlTextFormat.h
// description:	
//

#ifndef INC_HTMLTEXTFORMAT_H_
#define INC_HTMLTEXTFORMAT_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
//#include "htmlviewport.h"
#include "Text/TextFormat.h"
#include "HtmlDocument.h"

// --- Defines ------------------------------------------------------------------

#define NUM_NODES_PER_LINE	32
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CHtmlRenderer;
class CHtmlNode;
class CHtmlElementNode;

//
// name:		CFontFormatHtml
// description:	Class used to work out how much space text and images will take up
class CHtmlTextFormat : public CTextFormatStringProcess
{
public:
	CHtmlTextFormat(float x, float y, float width, s32 align, float viewportWidth, float viewportHeight, CHtmlRenderer* pRenderer=NULL) :
		m_baseX(x), 
		m_baseY(y),
		m_width(width), 
		m_viewportWidth(viewportWidth),
		m_viewportHeight(viewportHeight),
		m_align(align), 
		m_pRenderer(pRenderer),
		m_currentLineHeight(0.0f), 
		m_x(0.0f), 
		m_y(0.0f), 
		m_pCurrentNode(NULL),
		m_numNodes(0)
	{}

#ifndef HTML_RESOURCE
	bool operator()(CTextLayout *, float , float , GxtChar* pStart, GxtChar* pEnd, float length)
	{
		AddTextBlock(pStart, pEnd, length*m_viewportWidth, CHtmlDocument::sm_HtmlTextLayout.GetLineHeight() * m_viewportHeight);
		return true;
	}
#else
	bool operator()(CTextLayout *, float , float , GxtChar* pStart, GxtChar* pEnd, float length)
	{
		AddTextBlock(pStart, pEnd, length*m_viewportWidth, 1.0f * m_viewportHeight);
		return true;
	}
#endif

	void SetHtmlNode(CHtmlNode* pNode);
	void AddTextBlock(GxtChar* pStart, GxtChar* pEnd, float length, float height);
	void AddNonTextBlock(float length, float height);
	void NewLine();

	float GetCurrentX() {return m_x;}
	float GetCurrentY() {return m_y;}
	float GetCurrentLineHeight() {return m_currentLineHeight;}

	bool DontSkipLineOnFirstWord() { return (m_numNodes == 0);}

private:
	void AddBlock(float width, float height);
	void RenderElement(s32 i, float x, float y);
	void RenderImage(CHtmlElementNode* pElementNode);

	float m_baseX;
	float m_baseY;
	float m_width;
	float m_viewportWidth;
	float m_viewportHeight;
	s32 m_align;
	float m_xOffset;
	CHtmlRenderer* m_pRenderer;

	float m_currentLineHeight;
	float m_x;
	float m_y;
	CHtmlNode* m_pNodes[NUM_NODES_PER_LINE];
	CHtmlNode* m_pCurrentNode;
	CHtmlNode* m_pPreviousNode;
	GxtChar* m_pTextStart[NUM_NODES_PER_LINE];
	GxtChar* m_pTextEnd[NUM_NODES_PER_LINE];
	float m_lengths[NUM_NODES_PER_LINE];
	s32 m_numNodes;
};


// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLTEXTFORMAT_H_
