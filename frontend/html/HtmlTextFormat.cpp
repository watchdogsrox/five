//
// filename:	HtmlTextFormat.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "HtmlTextFormat.h"

// C headers
// Rage headers
#include "parsercore/attribute.h"
// Game headers
#include "HtmlDocument.h"
#include "HtmlNode.h"
#include "HtmlRenderer.h"
#include "renderer/sprite2d.h"


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

//
// name:		CHtmlTextFormat::AddHtmlNode
// description:	Add node to list
void CHtmlTextFormat::SetHtmlNode(CHtmlNode* pNode)
{
	m_pCurrentNode = pNode;
//	m_pPreviousNode = pNode;
}

void CHtmlTextFormat::AddBlock(float width, float height)
{
	// if the width is too wide and this isn't the first block then add height and reset current line width
	if(m_x + width > m_width && m_x != 0.0f)
		NewLine();
	m_x += width;
	if(height > m_currentLineHeight && width > 0.0f)
		m_currentLineHeight = height;
}

//
// name:		CHtmlTextFormat::AddTextBlock
// description:	Add node to list
void CHtmlTextFormat::AddTextBlock(GxtChar* pStart, GxtChar* pEnd, float length, float height)
{
	Assertf(m_numNodes < NUM_NODES_PER_LINE, "Too many html elements on one line");

	AddBlock(length, height);

	m_pTextStart[m_numNodes] = pStart;
	m_pTextEnd[m_numNodes] = pEnd;
	m_lengths[m_numNodes] = length;
	if(pStart != pEnd)
	{
		m_pNodes[m_numNodes] = m_pCurrentNode;
		m_pCurrentNode = NULL;
	}
	else
		m_pNodes[m_numNodes] = NULL;
	m_numNodes++;


}

//
// name:		CHtmlTextFormat::AddTextBlock
// description:	Add node to list
void CHtmlTextFormat::AddNonTextBlock(float length, float height)
{
	Assertf(m_numNodes < NUM_NODES_PER_LINE, "Too many html elements on one line");

	AddBlock(length, height);

	m_pTextStart[m_numNodes] = NULL;
	m_pTextEnd[m_numNodes] = NULL;
	m_lengths[m_numNodes] = length;
	m_pNodes[m_numNodes] = m_pCurrentNode;
	m_pCurrentNode = NULL;
	m_numNodes++;


}

//
// name:		CHtmlTextFormat::NewLine
// description:	We have filled out our line, format/render elements 
void CHtmlTextFormat::NewLine()
{
	// format current line entries
	//float diff = m_width - m_x;
	float start = 0.0f;
	if(m_align == HTML_ALIGN_CENTER)
		start = (m_width - m_x) / 2.0f;
	else if(m_align == HTML_ALIGN_RIGHT)
		start = (m_width - m_x);

	for(s32 i=0; i<m_numNodes; i++)
	{
		if(m_pRenderer)
		{
			if(m_pNodes[i])
			{
				m_y = m_pNodes[i]->GetRenderState().m_currentY;
				m_xOffset = m_pNodes[i]->GetRenderState().m_currentX - (m_baseX + start);
			}
			RenderElement(i, m_baseX + start + m_xOffset, m_baseY + m_y);
		}
		else 
		{
			if(m_pNodes[i])
			{
				m_pNodes[i]->GetRenderState().m_currentX = m_baseX + start;
				m_pNodes[i]->GetRenderState().m_currentY = m_baseY + m_y;
			}
		}
		start += m_lengths[i];
	}

	m_y += m_currentLineHeight;
	m_x = 0.0f;
	m_currentLineHeight = 0.0f;
	m_numNodes = 0;
}

//
// name:		CHtmlTextFormat::RenderElement
// description:	
void CHtmlTextFormat::RenderElement(s32 i, float x, float y)
{
	if(m_pTextStart[i])
	{
		Assert(m_pTextEnd);
		if(m_pNodes[i])
			m_pPreviousNode = m_pNodes[i];

		m_pRenderer->RenderText(m_pPreviousNode, x, y, m_pTextStart[i], m_pTextEnd[i], m_lengths[i]);
	}
	else
	{
		// if we have no text info then we need an html node
		Assert(m_pNodes[i]);
		CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(m_pNodes[i]);
		Assert(pElement);
		if(pElement->GetElementId() == HTML_IMAGE)
			m_pRenderer->RenderImage(pElement);
		else if(pElement->GetElementId() == HTML_EMBED)
			m_pRenderer->RenderFlash(pElement);
	}
}

