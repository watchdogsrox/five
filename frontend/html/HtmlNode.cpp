//
// filename:	HtmlNode.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

#include "htmlnode.h"

// C headers
// Rage headers
#include "grcore/texture.h"
#include "parsercore/element.h"
// Game headers
#include "HtmlDocument.h"
#include "HtmlLanguage.h"
#ifndef HTML_RESOURCE
#include "scene/stores/txdstore.h"
#endif


// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

// --- CHtmlNode ----------------------------------------------------------------

CHtmlNode::CHtmlNode() : m_pParent(NULL)
{
	m_type = INVALID_NODE;
}

CHtmlNode::~CHtmlNode()
{
	for(s32 i=0; i<m_children.GetCount(); i++)
	{
		delete m_children[i];
		m_children[i] = NULL;
	}
}

#if __DECLARESTRUCT
void CHtmlNode::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(CHtmlNode);
	STRUCT_FIELD(m_type);
	STRUCT_FIELD(m_pParent);
	STRUCT_FIELD(m_children);
	STRUCT_FIELD(m_renderState);
	STRUCT_END();
}
#endif


void CHtmlNode::Place(CHtmlNode* that,datResource& rsc)
{
	switch(that->m_type)
	{
	case ELEMENT_NODE:
		::new (that) CHtmlElementNode(rsc);
		break;
	case DATA_NODE:
		::new (that) CHtmlDataNode(rsc);
		break;
	case TABLE_NODE:
		::new (that) CHtmlTableNode(rsc);
		break;
	case TABLEENTRY_NODE:
		::new (that) CHtmlTableElementNode(rsc);
		break;
		//	case FLASH_NODE:
		//		::new (that) CHtmlFlashNode(rsc);
		//		break;
	default:
		Assertf(false, "Found invalid html node type");
		break;
	}
}

void CHtmlNode::AddChild(CHtmlNode* pNode)
{
	m_children.PushAndGrow(pNode);
	pNode->m_pParent = this;
}

#if __DEV
void CHtmlNode::DebugPrint()
{
	sm_debugPrintTab++;
	for(s32 i=0; i<m_children.GetCount(); i++)
		m_children[i]->DebugPrint();
	sm_debugPrintTab--;
}
#endif

//
// name:		CHtmlNode::GetBlockParent
// description:	return nearest ancestor to this node that doesn't have has display properties "inline"
CHtmlNode* CHtmlNode::GetNonInlineNode()
{
	CHtmlNode* pNode = GetParent();
	while(pNode)
	{
		if(pNode->GetRenderState().m_display != HTML_INLINE)
			break;
		pNode = pNode->m_pParent;
	}
	Assert(pNode);
	return pNode;
}

//
// name:		CHtmlNode::GetDimensions
// description:	return the width and height that current node is using, return node that contains
//				the width and height as well
void CHtmlNode::GetDimensions(float& x, float& y, float& width, float& height)
{
	CHtmlNode* pNode = this;
	while(pNode)
	{
		if(pNode->GetRenderState().m_width != -1 && pNode->GetRenderState().m_display != HTML_INLINE)
			break;
		pNode = pNode->m_pParent;
	}
	Assert(pNode);
	x = pNode->GetRenderState().m_currentX + pNode->GetRenderState().m_paddingLeft;
	width = pNode->GetRenderState().m_width - (pNode->GetRenderState().m_paddingLeft + pNode->GetRenderState().m_paddingRight);

	pNode = this;
	while(pNode)
	{
		if(pNode->GetRenderState().m_height != -1 && pNode->GetRenderState().m_display != HTML_INLINE)
			break;
		pNode = pNode->m_pParent;
	}
	Assert(pNode);
	y = pNode->GetRenderState().m_currentY + pNode->GetRenderState().m_paddingTop;
	height = pNode->GetRenderState().m_height - (pNode->GetRenderState().m_paddingTop + pNode->GetRenderState().m_paddingBottom);
}

// --- CHtmlElementNode ---------------------------------------------------------

CHtmlElementNode::CHtmlElementNode(parElement* pElement)
{
	m_type = ELEMENT_NODE;
	m_pElement = pElement;
}

CHtmlElementNode::CHtmlElementNode(datResource& rsc) : CHtmlNode(rsc), m_data(rsc) 
{
#ifndef HTML_RESOURCE
	if(m_elementId == HTML_SCRIPTOBJ)
	{
		CHtmlDocument* pDocument = CHtmlDocument::GetCurrentDocument();
		//pDocument->SetupDefaultStyleSheet();

		// If loading texture function isn't setup then set it
		//if(sm_loadTextureFn == NULL)
		//	sm_loadTextureFn = MakeFunctorRet(*pDocument, &CHtmlDocument::LoadTexture);
		pDocument->SetTextureLoadingFn(MakeFunctorRet(*pDocument, &CHtmlDocument::LoadTextureInternal));
		pDocument->ParseScriptObject(this);

		pDocument->sm_inBody = true;
		// process children nodes
		for(s32 i=0; i<GetNumChildren(); i++)
		{
			GetChild(i)->GetRenderState().Inherit(GetRenderState());
			pDocument->ProcessNode(GetChild(i));
		}
		pDocument->sm_inBody = false;

		pDocument->ResetElementArray();
		//pDocument->m_styleSheet.Shutdown();
	}
	else if(m_elementId == HTML_IMAGE)
	{
		// If image is coming from a texture dictionary outside of the html page resource then go find it
		// and get the width and height from it
		char txdName[64];
		GetTxdName(txdName, 64);

		if(txdName[0] != '\0')
		{
			grcTexture* pImage = GetTexture(CHtmlDocument::GetCurrentDocument());
			if(pImage)
			{
				HtmlRenderState& rs = GetRenderState();
				if(rs.m_forcedWidth == -1)
					rs.m_forcedWidth = (float)pImage->GetWidth();
				if(rs.m_forcedHeight == -1)
					rs.m_forcedHeight = (float)pImage->GetHeight();
				rs.m_width = rs.m_forcedWidth;
				rs.m_height = rs.m_forcedHeight;
			}
		}

	}
#endif
}

#if __DECLARESTRUCT
void CHtmlElementNode::DeclareStruct(datTypeStruct &s)
{
	CHtmlNode::DeclareStruct(s);
	STRUCT_BEGIN(CHtmlElementNode);
	STRUCT_FIELD(m_elementId);
	STRUCT_IGNORE(m_pElement);
	STRUCT_FIELD(m_data);
	STRUCT_END();
}
#endif

const char* CHtmlElementNode::GetTextureName()
{
	const char* pColon = strchr(m_data, ':');
	if(pColon)
		return pColon+1;
	return m_data;
}

void CHtmlElementNode::GetTxdName(char* pBuffer, int bufferLen)
{
	const char* pColon = strchr(m_data, ':');
	if(pColon)
	{
		int len = pColon - m_data;
		bufferLen--;
		if(len > bufferLen)
			len = bufferLen;
		strncpy(pBuffer, m_data, len);
		pBuffer[len]= '\0';
	}
	else
	{
		pBuffer[0] = '\0';
	}
}

grcTexture* CHtmlElementNode::GetTexture(class CHtmlDocument* pDocument)
{
	const char* pImageName = GetTextureName();
	
#ifndef HTML_RESOURCE
	char txdName[64];
	GetTxdName(txdName, 64);
	if(txdName[0] == '*')
	{
		return grcTextureFactory::GetInstance().LookupTextureReference(pImageName);
	}
	else if(txdName[0] != '\0')
	{
		int slot = g_TxdStore.FindSlot(txdName);
		Assertf(slot != -1, "%s:Txd does not exist", txdName);
		Txd* pTxd = g_TxdStore.Get(slot);
		Assertf(pTxd, "%s:Txd is not in memory", txdName);
		return pTxd->Lookup(pImageName);
	}
	else
#endif
		return pDocument->GetTextureDictionary()->Lookup(pImageName);

}

#if __DEV
void CHtmlElementNode::DebugPrint()
{
	for(s32 i=0; i<sm_debugPrintTab; i++)
		Printf("  ");
	Printf("%s x:%f y:%f\n", m_pElement->GetName(), GetRenderState().m_currentX, GetRenderState().m_currentY);
	CHtmlNode::DebugPrint();
}
#endif

// --- CHtmlDataNode ------------------------------------------------------------

CHtmlDataNode::CHtmlDataNode(const char* pData, s32 size) : CHtmlNode(), 
m_pData(NULL)
{
	m_type = DATA_NODE;
	SetData(pData, size);
}

CHtmlDataNode::CHtmlDataNode(datResource& rsc) : CHtmlNode(rsc)
{
	rsc.PointerFixup(m_pData);
}

CHtmlDataNode::~CHtmlDataNode()
{
	if(m_pData)
		delete[] m_pData;
}

#if __DECLARESTRUCT
void CHtmlDataNode::DeclareStruct(datTypeStruct &s)
{
	CHtmlNode::DeclareStruct(s);
	STRUCT_BEGIN(CHtmlDataNode);
	STRUCT_FIELD_VP(m_pData);
	STRUCT_END();
}
#endif

void CHtmlDataNode::SetData(const char* pData, s32 size)
{
	Assert(m_pData == NULL);
	if(size < 0)
		size = strlen(pData) + 1;
	m_pData = rage_new GxtChar[size+1];
	// safecpy wants the actual size of the data not size-1
	safecpy(m_pData, pData, size+1);
}

#if __DEV
void CHtmlDataNode::DebugPrint()
{
	CHtmlNode::DebugPrint();
}
#endif

// --- CHtmlTableNode ------------------------------------------------------------

CHtmlTableNode::CHtmlTableNode(parElement* pElement) : CHtmlElementNode(pElement),
m_pHeights(NULL),
m_pMinWidths(NULL),
m_pMaxWidths(NULL),
m_pCalculatedWidths(NULL),
m_pCalculatedHeights(NULL),
m_numColumns(0),
m_numRows(0)
{
	m_type = TABLE_NODE;
}

CHtmlTableNode::CHtmlTableNode(datResource& rsc) : CHtmlElementNode(rsc) 
{
	rsc.PointerFixup(m_pHeights);
	rsc.PointerFixup(m_pMinWidths);
	rsc.PointerFixup(m_pMaxWidths);
	rsc.PointerFixup(m_pCalculatedHeights);
	rsc.PointerFixup(m_pCalculatedWidths);
}

CHtmlTableNode::~CHtmlTableNode()
{
	DeleteArrays();
}

#if __DECLARESTRUCT
void CHtmlTableNode::DeclareStruct(datTypeStruct &s)
{
	CHtmlElementNode::DeclareStruct(s);
	STRUCT_BEGIN(CHtmlTableNode);
	STRUCT_DYNAMIC_ARRAY(m_pHeights, m_numRows);
	STRUCT_DYNAMIC_ARRAY(m_pMinWidths, m_numColumns);
	STRUCT_DYNAMIC_ARRAY(m_pMaxWidths, m_numColumns);
	STRUCT_DYNAMIC_ARRAY(m_pCalculatedWidths, m_numColumns);
	STRUCT_DYNAMIC_ARRAY(m_pCalculatedHeights, m_numRows);
	STRUCT_FIELD(m_numColumns);
	STRUCT_FIELD(m_numRows);
	STRUCT_END();
}
#endif

void CHtmlTableNode::AllocateArrays()
{
	m_pMinWidths = rage_new float[m_numColumns];
	m_pMaxWidths = rage_new float[m_numColumns];
	m_pHeights = rage_new float[m_numRows];
	m_pCalculatedWidths = rage_new float[m_numColumns];
	m_pCalculatedHeights = rage_new float[m_numRows];

	for(s32 i=0; i<m_numColumns; i++)
	{
		m_pMinWidths[i] = -1.0f;
		m_pMaxWidths[i] = -1.0f;
		m_pCalculatedWidths[i] = -1.0f;
	}
	for(s32 i=0; i<m_numRows; i++)
	{
		m_pHeights[i] = -1.0f;
		m_pCalculatedHeights[i] = -1.0f;
	}
}

void CHtmlTableNode::DeleteArrays()
{
	if(m_pMinWidths)
		delete[] m_pMinWidths;
	if(m_pMaxWidths)
		delete[] m_pMaxWidths;
	if(m_pHeights)
		delete[] m_pHeights;
	if(m_pCalculatedWidths)
		delete[] m_pCalculatedWidths;
	if(m_pCalculatedHeights)
		delete[] m_pCalculatedHeights;
}


void CHtmlTableNode::ResetCalculatedWidthAndHeights()
{
	/*	for(s32 i=0; i<m_numColumns; i++)
	{
	m_pCalculatedWidths[i] = m_pWidths[i];
	}*/

}

float CHtmlTableNode::GetMinWidth()
{
	HtmlRenderState& rs = GetRenderState();
	float width = 0.0f;
	for(s32 i=0; i<m_numColumns; i++)
		width += m_pMinWidths[i];

	width += rs.m_cellspacing * (m_numColumns + 1);
	width += (rs.m_borderLeftWidth + rs.m_borderRightWidth) * m_numColumns;
	width += rs.m_cellpadding * m_numColumns * 2.0f;
	return width;
}

float CHtmlTableNode::GetMaxWidth()
{
	HtmlRenderState& rs = GetRenderState();
	float width = 0.0f;
	for(s32 i=0; i<m_numColumns; i++)
		width += m_pMaxWidths[i];

	width += rs.m_cellspacing * (m_numColumns + 1);
	width += (rs.m_borderLeftWidth + rs.m_borderRightWidth) * m_numColumns;
	width += rs.m_cellpadding * m_numColumns * 2.0f;
	return width;
}

void CHtmlTableNode::CalculateColumnWidths()
{
	HtmlRenderState& rs = GetRenderState();

	//ResetCalculatedWidthAndHeights();
	float x, y, width, height;
	float minWidth = 0.0f;
	float maxWidth = 0.0f;

	for(s32 i=0; i<m_numColumns; i++)
	{
		minWidth += m_pMinWidths[i];
		maxWidth += m_pMaxWidths[i];
	}

	GetDimensions(x,y,width,height);

	// subtract cell spacing
	width -= rs.m_cellspacing * (m_numColumns + 1);
	width -= (rs.m_borderLeftWidth + rs.m_borderRightWidth) * m_numColumns;
	width -= rs.m_cellpadding * m_numColumns * 2.0f;

	// three cases for setting widths
	// 1. The minimum table width is equal to or wider than the available space
	if(minWidth >= width)
	{
		//		rs.m_width = minWidth;
		for(s32 i=0; i<m_numColumns; i++)
			m_pCalculatedWidths[i] = m_pMinWidths[i] + rs.m_cellpadding* 2.0f;
	}
	// 2. The maximum table width fits within the space available
	else if(maxWidth <= width)
	{
		float mult=1.0f;
		// If table width has been forced then scale up cell widths to reach this width
		if(rs.m_width > 0.0f) 
		{// what if maxwidth is 0.0f
			if(maxWidth == 0.0f)
			{
				for(s32 i=0; i<m_numColumns; i++)
					m_pCalculatedWidths[i] = (width / m_numColumns) + rs.m_cellpadding* 2.0f;
			}
			else
			{
				mult = width / maxWidth;
				for(s32 i=0; i<m_numColumns; i++)
					m_pCalculatedWidths[i] = m_pMaxWidths[i]*mult + rs.m_cellpadding* 2.0f;
			}
		}
		else
		{
			for(s32 i=0; i<m_numColumns; i++)
				m_pCalculatedWidths[i] = m_pMaxWidths[i] + rs.m_cellpadding* 2.0f;
		}
	}
	// 3. The maximum width of the table is greater than the available space, but the minimum table width is
	// smaller
	else
	{
		float W = width - minWidth;
		float D = maxWidth - minWidth;
		for(s32 i=0; i<m_numColumns; i++)
		{
			float d = m_pMaxWidths[i] - m_pMinWidths[i];
			m_pCalculatedWidths[i] = m_pMinWidths[i] + d * (W/D) + rs.m_cellpadding* 2.0f;
		}
	}

	// set total width
	rs.m_width = rs.m_cellspacing;
	for(s32 i=0; i<m_numColumns; i++)
	{
		// add border widths on here as they will be removed in the FormatElement function
		m_pCalculatedWidths[i] += rs.m_borderLeftWidth + rs.m_borderRightWidth;
		rs.m_width += m_pCalculatedWidths[i] + rs.m_cellspacing;
	}

	// set heights
	for(s32 i=0; i<m_numRows; i++)
	{
		m_pCalculatedHeights[i] = m_pHeights[i];
	}

}

// --- CHtmlTableElementNode ----------------------------------------------------

CHtmlTableElementNode::CHtmlTableElementNode(parElement* pElement) : CHtmlElementNode(pElement),
m_row(-1),
m_column(-1)
{
	m_type = TABLEENTRY_NODE;
}

#if __DECLARESTRUCT
void CHtmlTableElementNode::DeclareStruct(datTypeStruct &s)
{
	CHtmlElementNode::DeclareStruct(s);
	STRUCT_BEGIN(CHtmlTableElementNode);
	STRUCT_FIELD(m_row);
	STRUCT_FIELD(m_column);
	STRUCT_END();
}
#endif

// --- CHtmlFlashNode ----------------------------------------------------

/*CHtmlFlashNode::CHtmlFlashNode(parElement* pElement) : CHtmlElementNode(pElement), 
m_pFile(NULL),
m_pContext(NULL)
{
m_type = FLASH_NODE;
}

CHtmlFlashNode::~CHtmlFlashNode()
{
if(m_pContext)
delete m_pContext;
if(m_pFile)
delete m_pFile;
}

#if !__FINAL
void CHtmlFlashNode::DeclareStruct(datTypeStruct &s)
{
CHtmlElementNode::DeclareStruct(s);
STRUCT_BEGIN(CHtmlFlashNode);
STRUCT_FIELD(m_pFile);
STRUCT_IGNORE(m_pContext);
STRUCT_END();
}
#endif

void CHtmlFlashNode::Load()
{
const parAttribute* pSrc = GetElement().FindAttribute("src");
Assertf(pSrc, "\"embed\" tag doesn't have \"src\" attribute");
m_pFile = rage_new swfFILE;
if(m_pFile->Load(pSrc->GetStringValue()))
{
m_pContext = rage_new swfCONTEXT(*m_pFile, 5000, 5000, 5000, 5000);
m_pContext->SetFSCommandHandler(&FSHandler);
}
}

//Example of how to handle fscommand and MeasureString fscommand:
void CHtmlFlashNode::FSHandler(const char* command, const swfVARIANT& UNUSED_PARAM(param))
{
if(!strcmp("FSCommand:MeasureString",command))
{
swfFILE *file = (swfFILE*)swfFILE::GetCurrentFile();
swfCONTEXT *context = swfFILE::GetCurrentContext();

Assert(file && context);

char szFontName[64];
context->GetGlobal("fs_measure_font",szFontName,sizeof(szFontName));

char szText[255];

float fPointSize = 0;
context->GetGlobal("fs_measure_size", fPointSize);

context->GetGlobal("fs_measure_string",szText,sizeof(szText));

context->SetGlobal("fs_measure_result", file->MeasureString(szFontName,fPointSize,szText));
}
}
*/
