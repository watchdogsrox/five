//
// filename:	HtmlDocument.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------
#include "HtmlDocument.h"

// C headers
// Rage headers
#include "file/asset.h"
#include "file/device.h"
#include "file/device_relative.h"
#include "flash/err.h"
#include "parsercore/attribute.h"
#include "parsercore/element.h"
#include "system/param.h"
// Game headerso
#include "Css.h"
#include "HtmlLanguage.h"
#include "HtmlNode.h"
#include "HtmlParser.h"

// Framework Headers
//#include "fwmaths/Maths.h"

#ifndef HTML_RESOURCE
#include "HtmlScriptObject.h"
#include "HtmlTextFormat.h"
#include "HtmlViewport.h"
#include "camera/viewports/Viewport.h"
#include "debug/debug.h"
#include "Text/TextFormat.h"
#include "renderer/RenderThread.h"
#include "scene/stores/txdstore.h"
#endif

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

#if __DEV
PARAM(htmlpath, "Path to find non-resourced HTML files");
#endif

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------
CHtmlNode* CHtmlDocument::sm_pCurrent;
atArray<parElement*> CHtmlDocument::sm_elements;
bool CHtmlDocument::sm_parseScriptObjects = true;
bool CHtmlDocument::sm_loadTextures = true;
bool CHtmlDocument::sm_inBody = false;
atArray<CHtmlNode* > CHtmlDocument::sm_inlineObjs;
CHtmlDocument* CHtmlDocument::sm_pCurrentDocument = NULL;
CHtmlDocument::LoadTextureFn CHtmlDocument::sm_loadTextureFn = NULL;
atArray<int> CHtmlDocument::sm_txdIndicesUsed;

#ifndef HTML_RESOURCE
CTextLayout CHtmlDocument::sm_HtmlTextLayout;
#endif

#if __DEV
s32 CHtmlNode::sm_debugPrintTab = 0;
#ifndef HTML_RESOURCE
PARAM(debugscripthtml, "Debug html script objects");
#endif
#endif
// --- Code ---------------------------------------------------------------------

// --- CHtmlDocument ------------------------------------------------------------

CHtmlDocument::CHtmlDocument() : m_pRootNode(NULL), 
m_pBodyNode(NULL),
m_pTitle(NULL),
m_pTxd(NULL)
{
}

CHtmlDocument::CHtmlDocument(datResource& rsc) : 
m_pTitle(rsc),
m_pTxd(rsc),
//m_swfArray(rsc, true),
m_links(rsc, true),
m_styleSheet(rsc)
{
	// do them in the function because we want to control the order they get done in
	rsc.PointerFixup(m_pRootNode);
	::new ((void*)m_pRootNode) CHtmlElementNode(rsc);
	rsc.PointerFixup(m_pBodyNode);
}

CHtmlDocument::~CHtmlDocument()
{
	Delete();
}

void CHtmlDocument::Init(unsigned /*initMode*/)
{
	static fiDeviceRelative gHttpDevice;
	// create html mount
#if __DEV
	const char* pHtmlPath;
	if(PARAM_htmlpath.Get(pHtmlPath))
		gHttpDevice.Init(pHtmlPath, false);
	else
#endif
	gHttpDevice.Init("platform:/html/", true);
	gHttpDevice.MountAs("http:/");
}

void CHtmlDocument::Place(CHtmlDocument* that, datResource& rsc)
{
	sm_pCurrentDocument = that;
	::new (that) CHtmlDocument(rsc);
	sm_pCurrentDocument = NULL;
}

#if __DECLARESTRUCT
void CHtmlDocument::DeclareStruct(datTypeStruct &s)
{
	STRUCT_BEGIN(CHtmlDocument);
	if(m_pRootNode)
		datSwapper(*m_pRootNode);
	STRUCT_FIELD_VP(m_pRootNode);
	STRUCT_FIELD_VP(m_pBodyNode);
	STRUCT_FIELD(m_pTitle);
	STRUCT_FIELD(m_pTxd);
	STRUCT_FIELD(m_padArray);
	STRUCT_FIELD(m_links);
	STRUCT_FIELD(m_styleSheet);
	STRUCT_END();
}
#endif

void CHtmlDocument::Delete()
{
	if(m_pRootNode)
	{
		delete m_pRootNode;
		m_pRootNode = NULL;
	}
	// remove title string
	if(m_pTitle)
	{
		delete[] m_pTitle;
		m_pTitle = NULL;
	}

	// remove texture dictionary
	if(m_pTxd)
	{
#ifndef HTML_RESOURCE
		// flush render thread so it releases the current visible objects
		gRenderThreadInterface.Flush();
#endif //HTML_RESOURCE

		delete m_pTxd;
		m_pTxd = NULL;
	}

	m_links.Reset();
//	m_swfArray.Reset();
	m_styleSheet.Shutdown();

	// reset the txds used
	sm_txdIndicesUsed.Reset();
}

//
// name:		CHtmlDocument::GetLinkRef
// description:	Get web page this link references
const char* CHtmlDocument::GetLinkRef(s32 link)
{
	CHtmlElementNode* pNode = m_links[link];
	return pNode->GetAttributeData();
}

//
// name:		CHtmlDocument::GetLinkAtPosition
// description:	Return the link at this position. If there is no link then return -1
s32 CHtmlDocument::GetLinkAtPosition(float x, float y)
{
#define TEMP_LINK_WIDTH		(48.0f)
#define TEMP_LINK_HEIGHT	(16.0f)
	// temporary until we get proper bounding boxes for the links
	for(s32 i=0; i<m_links.GetCount(); i++)
	{
		CHtmlElementNode* pNode = m_links[i];
		float linkX = pNode->GetRenderState().m_currentX;
		float linkY = pNode->GetRenderState().m_currentY;
		float linkWidth = pNode->GetRenderState().m_width;
		float linkHeight = pNode->GetRenderState().m_height;
		if(x >= linkX && x < linkX+linkWidth && y >= linkY && y < linkY+linkHeight)
			return i;
	}
	return -1;
}

//
// name:		CHtmlDocument::AddInlineObject
// description:	Add to inline object list
void CHtmlDocument::AddInlineObject(CHtmlNode* pNode) 
{
	sysMemStartTemp();
	sm_inlineObjs.PushAndGrow(pNode);
	sysMemEndTemp();
}

//
// name:		CHtmlDocument::ResetInlineObjectList
// description:	Reset inline object list
void CHtmlDocument::ResetInlineObjectList() 
{
	sysMemStartTemp();
	sm_inlineObjs.Reset();
	sysMemEndTemp();
}

//
// name:		CHtmlDocument::SetupDefaultStyleSheet
// description:	Setup default style sheet 
void CHtmlDocument::SetupDefaultStyleSheet()
{
	m_styleSheet.Init();
	CCssStyleClass* pClass = m_styleSheet.GetDefaultStyleClass();
	CCssStyleGroup* pGroup = pClass->AddStyleGroup(HTML_HTML);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_BODY);
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 8));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINLEFT, 8));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINRIGHT, 8));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 8));
	pGroup->AddStyle(CCssStyle(CCssStyle::COLOR, Color32(0,0,0)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BACKGROUNDCOLOR, Color32(255,255,255)));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_HEADING1);
	pGroup->AddStyle(CCssStyle(CCssStyle::FONTSIZE, 24));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 16));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 16));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_HEADING2);
	pGroup->AddStyle(CCssStyle(CCssStyle::FONTSIZE, 18));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 13.5f));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 13.5f));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_HEADING3);
	pGroup->AddStyle(CCssStyle(CCssStyle::FONTSIZE, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 12));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 12));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_HEADING4);
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 12));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 15));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_HEADING5);
	pGroup->AddStyle(CCssStyle(CCssStyle::FONTSIZE, 10));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 15));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_HEADING6);
	pGroup->AddStyle(CCssStyle(CCssStyle::FONTSIZE, 9));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 15));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_DIV);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

//	pGroup = sm_styleSheet.AddStyleGroup(HTML_IMAGE);
//	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_TABLE);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERWIDTH, 1.0f));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERSTYLE, HTML_SOLID));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERLEFTCOLOR, Color32(0xcc,0xcc,0xcc)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERTOPCOLOR, Color32(0xcc,0xcc,0xcc)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERRIGHTCOLOR, Color32(0x66,0x66,0x66)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERBOTTOMCOLOR, Color32(0x66,0x66,0x66)));

	pGroup = pClass->AddStyleGroup(HTML_TABLEROW);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_TABLESECTION));

	pGroup = pClass->AddStyleGroup(HTML_TABLEENTRY);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_TABLECELL));
	pGroup->AddStyle(CCssStyle(CCssStyle::VERTICALALIGN, HTML_ALIGN_MIDDLE));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERWIDTH, 1.0f));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERSTYLE, HTML_SOLID));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERTOPCOLOR, Color32(0x66,0x66,0x66)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERLEFTCOLOR, Color32(0x66,0x66,0x66)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERBOTTOMCOLOR, Color32(0xcc,0xcc,0xcc)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERRIGHTCOLOR, Color32(0xcc,0xcc,0xcc)));

	pGroup = pClass->AddStyleGroup(HTML_TABLEHEADER);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_TABLECELL));
	pGroup->AddStyle(CCssStyle(CCssStyle::VERTICALALIGN, HTML_ALIGN_MIDDLE));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERWIDTH, 1.0f));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERSTYLE, HTML_SOLID));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERTOPCOLOR, Color32(0x66,0x66,0x66)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERLEFTCOLOR, Color32(0x66,0x66,0x66)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERBOTTOMCOLOR, Color32(0xcc,0xcc,0xcc)));
	pGroup->AddStyle(CCssStyle(CCssStyle::BORDERRIGHTCOLOR, Color32(0xcc,0xcc,0xcc)));

	pGroup = pClass->AddStyleGroup(HTML_DEFLIST);
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_DEFTERM);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_DEFDESC);
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINLEFT, 40));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_UNORDERED_LIST);
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINLEFT, 40));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_ORDERED_LIST);
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINLEFT, 40));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_PARAGRAPH);
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINTOP, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::MARGINBOTTOM, 14));
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));

	pGroup = pClass->AddStyleGroup(HTML_A);
	pGroup->AddStyle(CCssStyle(CCssStyle::TEXTDECORATION, HTML_UNDERLINE));
	pGroup->AddStyle(CCssStyle(CCssStyle::COLOR, Color32(0x80,0x80,0x80)));

	pGroup = pClass->AddStyleGroup(HTML_CENTER);
	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));
	pGroup->AddStyle(CCssStyle(CCssStyle::TEXTALIGN, HTML_ALIGN_CENTER));

//	pGroup = pClass->AddStyleGroup(HTML_SPAN);
//	pGroup->AddStyle(CCssStyle(CCssStyle::DISPLAY, HTML_BLOCK));
}

void CHtmlDocument::PushHtmlFolder(const char* pFilename)
{
	char pathname[RAGE_MAX_PATH];
	ASSET.RemoveNameFromPath(pathname, RAGE_MAX_PATH, pFilename);
	ASSET.PushFolder(pathname);
}

bool CHtmlDocument::LoadDocument(const char* pFilename)
{
	sysMemStartTemp();

	// set flash warning level
#if __DEV
//	swfSetFlashDebugLevel(eFEL_Fatal, eFWL_Critical);
#endif

	Assert(m_pRootNode == NULL);

	CHtmlDocument::PushHtmlFolder(pFilename);

	fiStream* pStream = ASSET.Open(ASSET.FileName(pFilename), NULL);
	if(pStream == NULL)
	{
		return false;
	}

	sm_pCurrentDocument = this;
	CHtmlLanguage::GetCurrent();
	sm_pCurrent = NULL;
	sysMemEndTemp();

	SetupDefaultStyleSheet();

	sysMemStartTemp();

	// If loading texture function isn't setup then set it
	if(sm_loadTextureFn == 0)
		sm_loadTextureFn = MakeFunctorRet(*this, &CHtmlDocument::LoadTextureInternal);

	CHtmlParser parser(pStream);

	parser.SetBeginElementCallback(parStreamIn::BeginElementCB(this, &CHtmlDocument::ReadBeginElement));
	parser.SetEndElementCallback(parStreamIn::EndElementCB(this, &CHtmlDocument::ReadEndElement));
	parser.SetDataCallback(parStreamIn::DataCB(this, &CHtmlDocument::ReadData));
	parser.ReadWithCallbacks();

	pStream->Close();

	sysMemEndTemp();

	Process();

	sysMemStartTemp();

	ResetElementArray();
	//sm_styleSheet.Shutdown();

	sysMemEndTemp();

	ASSET.PopFolder();

	// reset loading texture function
	sm_loadTextureFn = NULL;

	sm_pCurrentDocument = NULL;
	return true;
}

void CHtmlDocument::ReadBeginElement(parElement& element, bool leaf)
{
	CHtmlElementNode* pNode;

	parElement* pElement = rage_new parElement;
	// ensure element owns it own name
	pElement->SetName("temp", true);
	sm_elements.PushAndGrow(pElement);
	pElement->CopyFrom(element);

	sysMemEndTemp();

	// If this is a table element create a special class because table formatting is a bitch and needs loads
	// of special code
	if(!stricmp(element.GetName(), "table"))
		pNode = rage_new CHtmlTableNode(pElement);
	else if(!stricmp(element.GetName(), "td") || !stricmp(element.GetName(), "th") || !stricmp(element.GetName(), "tr"))
		pNode = rage_new CHtmlTableElementNode(pElement);
//	else if(!stricmp(element.GetName(), "embed"))	// assume all embed tags are flash objects
//		pNode = new CHtmlFlashNode(pElement);
	else
		pNode = rage_new CHtmlElementNode(pElement);

	// if there is no current node then this must be the root node
	if(sm_pCurrent == NULL)
	{
		Assert(m_pRootNode == NULL);
		m_pRootNode = pNode;
	}
	else
		sm_pCurrent->AddChild(pNode);

	if(leaf == false)
	{
		sm_pCurrent = pNode;
	}

	sysMemStartTemp();
}

void CHtmlDocument::ReadEndElement(bool leaf)
{
	if(!leaf)
		sm_pCurrent = sm_pCurrent->GetParent();
}

void CHtmlDocument::ReadData(char* pData, int size, bool incomplete)
{
	sysMemEndTemp();

	if(incomplete)
		Quitf("Need to increase datachunk size. Data is too large");

	Assert(sm_pCurrent);

	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(sm_pCurrent);
	// if parent element is a style tag then read style sheet
	if(pElement && !stricmp(pElement->GetElement().GetName(), "style"))
	{
		char filename[RAGE_MAX_PATH];

		fiDeviceMemory::MakeMemoryFileName(filename, sizeof(filename), pData, size, false, NULL);
		m_styleSheet.ReadFile(filename);
	}
	else
	{
		CHtmlDataNode* pNode = rage_new CHtmlDataNode(pData, size);
		sm_pCurrent->AddChild(pNode);
	}
	sysMemStartTemp();
}

//
// name:		CHtmlDocument::SetActive
// description:	
void CHtmlDocument::SetActive(CHtmlNode* pNode, bool active)
{
	pNode->GetRenderState().m_active = active;
	for(s32 i=0; i<pNode->GetNumChildren(); i++)
		SetActive(pNode->GetChild(i), active);
}

//
// name:		CHtmlDocument::Process
// description:	Extract data from elements
void CHtmlDocument::Process()
{
	sm_inBody = false;
	ProcessNode(m_pRootNode);
	Assertf(m_pRootNode->GetElementId() == HTML_HTML, "There is no HTML tag in this file");
}

//
// name:		CHtmlDocument::Process
// description:	Extract data from elements
void CHtmlDocument::PostProcess()
{
	PostProcessNode(m_pRootNode);
}

void CHtmlDocument::ProcessNode(CHtmlNode* pNode)
{
	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
	CHtmlDataNode* pData = dynamic_cast<CHtmlDataNode*>(pNode);

	if(pElement)
	{
		ProcessElement(pElement);

		// process children nodes
		for(s32 i=0; i<pNode->GetNumChildren(); i++)
		{
			pNode->GetChild(i)->GetRenderState().Inherit(pNode->GetRenderState());
			ProcessNode(pNode->GetChild(i));
		}

		// this is called from PostProcessNode now
		//PostProcessElement(pElement);

		pElement->ResetParserElement();
	}
	else if(pData)
		ProcessData(pData);

}

void CHtmlDocument::ProcessElement(CHtmlElementNode* pElementNode)
{
	s32 elementId = CHtmlLanguage::GetCurrent().GetElementId(pElementNode->GetElement().GetName());
	pElementNode->m_elementId = elementId;

#if __DEV
	if(elementId == HTML_ELEMENT_INVALID)
		Warningf("Unrecognised tag: %s", pElementNode->GetElement().GetName());
#endif

	// Apply style sheet
	ApplyDefaultStyleSheet(pElementNode);

	// inherit table values
	if(pElementNode->GetRenderState().m_display == HTML_TABLECELL ||
		pElementNode->GetRenderState().m_display == HTML_TABLESECTION)
	{
		pElementNode->GetRenderState().InheritTableState(pElementNode->GetParent()->GetRenderState());
	}

	ProcessAttributes(pElementNode);

	switch(elementId)
	{
	case HTML_BODY:
		ProcessBody(pElementNode);
		break;

	case HTML_META:
		ProcessMetaData(pElementNode);
		break;

	case HTML_A:
		ProcessLink(pElementNode);
		break;

	case HTML_LINK:
		ProcessFileLink(pElementNode);
		break;

	case HTML_IMAGE:
		ProcessImage(pElementNode);
		break;

	case HTML_EMBED:
		ProcessEmbed(pElementNode);
		break;

	case HTML_OBJECT:
		ProcessObject(pElementNode);
		break;

	case HTML_SCRIPTOBJ:
		ProcessScriptObject(pElementNode);
		break;

	case HTML_TEXT:
		ProcessText(pElementNode);
		break;

	default:
		break;
	}
}

//
// name:		CHtmlDocument::ResetElementArray
// description:	Reset all the parser elements and empty array
void CHtmlDocument::ResetElementArray()
{
	for(s32 i=0; i<sm_elements.GetCount(); i++)
	{
		delete sm_elements[i];
	}
	sm_elements.Reset();
}

//
// name:		ApplyDefaultStyleSheet
// description:	Apply default style sheet for this node
void CHtmlDocument::ApplyDefaultStyleSheet(CHtmlElementNode* pNode)
{
	CCssStyleGroup* pGroup = m_styleSheet.GetDefaultStyleClass()->GetStyleGroup(pNode->GetElementId());
	if(pGroup)
		pGroup->Apply(pNode->GetRenderState());
	// check for pseudo classes
	ApplyPseudoClassStyleSheet(pNode, "", ":hover");
}

//
// name:		ApplyDefaultStyleSheet
// description:	Apply default style sheet for this node
void CHtmlDocument::ApplyStyleSheet(CHtmlElementNode* pNode, const char* pClassName)
{
	CCssStyleClass* pClass = m_styleSheet.GetStyleClass(pClassName);
	if(pClass)
	{
		// apply default style group first
		CCssStyleGroup* pGroup = pClass->GetDefaultStyleGroup();
		if(pGroup)
			pGroup->Apply(pNode->GetRenderState());
		// apply element style group
		pGroup = pClass->GetStyleGroup(pNode->GetElementId());
		if(pGroup)
			pGroup->Apply(pNode->GetRenderState());

		// check for pseudo classes
		ApplyPseudoClassStyleSheet(pNode, pClassName, ":hover");
	}
}

//
// name:		CHtmlDocument::ApplyPseudoClassStyleSheet
// description:	Apply style sheet pseudo classes to additional renderstates
void CHtmlDocument::ApplyPseudoClassStyleSheet(CHtmlElementNode* pNode, const char* pClassName, const char* pPseudoClassName)
{
	CCssStyleClass* pClass;
	CCssStyleGroup* pGroup;

	// check for pseudo classes
	char pseudoClassName[128];
	strcpy(pseudoClassName, pClassName);
	strcat(pseudoClassName, pPseudoClassName);
	pClass = m_styleSheet.GetStyleClass(pseudoClassName);
	if(pClass)
	{
		// apply element style group
		pGroup = pClass->GetStyleGroup(pNode->GetElementId());
		if(pGroup)
		{
			pGroup->ApplyHoverPseudoClass(pNode->GetRenderState());
		}
	}
}

void CHtmlDocument::ProcessAttributes(CHtmlElementNode* pElementNode)
{
	parElement& element = pElementNode->GetElement();
	HtmlRenderState& renderState = pElementNode->GetRenderState();

	sysMemStartTemp();

	for(s32 i=0; i<element.GetAttributes().GetCount(); i++)
	{
		parAttribute& attr = element.GetAttributes()[i];
		HtmlAttributeTag tag = CHtmlLanguage::GetCurrent().GetAttributeId(attr.GetName());
		switch(tag)
		{
		case HTML_ATTR_ALIGN:
			renderState.m_textAlign = CHtmlLanguage::GetCurrent().GetValueId(attr.GetStringValue());
			break;
		case HTML_ATTR_ALINK:
			renderState.m_activeColor = CHtmlLanguage::GetCurrent().GetColor(attr.GetStringValue());
			break;
		case HTML_ATTR_LINK:
			{
				Color32 col = CHtmlLanguage::GetCurrent().GetColor(attr.GetStringValue());
				m_styleSheet.GetDefaultStyleClass()->GetStyleGroup(HTML_A)->AddStyle(CCssStyle(CCssStyle::COLOR, col));
			}
			break;
		case HTML_ATTR_BGCOLOR:
			renderState.m_renderBackground = true;
			renderState.m_backgroundColor = CHtmlLanguage::GetCurrent().GetColor(attr.GetStringValue());
			break;
		case HTML_ATTR_BACKGROUND:
			{
				sysMemEndTemp();
				renderState.m_pBackgroundImage = sm_loadTextureFn(attr.GetStringValue());
				renderState.m_renderBackground = true;
				sysMemStartTemp();
			}
			break;
		case HTML_ATTR_BORDER:
			attr.ConvertToInt();
			{
				float border = (float)attr.GetIntValue();
				if(border == 0.0f)
				{
					renderState.m_borderBottomStyle = HTML_NONE;
					renderState.m_borderLeftStyle = HTML_NONE;
					renderState.m_borderRightStyle = HTML_NONE;
					renderState.m_borderTopStyle = HTML_NONE;
				}
				else
				{
					renderState.m_borderBottomStyle = HTML_SOLID;
					renderState.m_borderLeftStyle = HTML_SOLID;
					renderState.m_borderRightStyle = HTML_SOLID;
					renderState.m_borderTopStyle = HTML_SOLID;
				}
				renderState.m_borderBottomWidth = border;
				renderState.m_borderLeftWidth = border;
				renderState.m_borderRightWidth = border;
				renderState.m_borderTopWidth = border;
			}
			break;
		case HTML_ATTR_BORDERCOLOR:
			renderState.m_borderBottomColor = CHtmlLanguage::GetCurrent().GetColor(attr.GetStringValue());
			renderState.m_borderLeftColor = renderState.m_borderBottomColor;
			renderState.m_borderRightColor = renderState.m_borderBottomColor;
			renderState.m_borderTopColor = renderState.m_borderBottomColor;
			break;
		case HTML_ATTR_CELLSPACING:
			attr.ConvertToInt();
			renderState.m_cellspacing = (float)attr.GetIntValue();
			break;
		case HTML_ATTR_CELLPADDING:
			attr.ConvertToInt();
			renderState.m_cellpadding = (float)attr.GetIntValue();
			break;
		case HTML_ATTR_CLASS:
			// do some style sheet 
			ApplyStyleSheet(pElementNode, attr.GetStringValue());
			break;
		case HTML_ATTR_COLOR:
			renderState.m_color = CHtmlLanguage::GetCurrent().GetColor(attr.GetStringValue());
			break;
		case HTML_ATTR_HEIGHT:
			attr.ConvertToInt();
			renderState.m_forcedHeight = (float)attr.GetIntValue();
			break;
		case HTML_ATTR_MARGINHEIGHT:
			attr.ConvertToInt();
			renderState.m_marginBottom = (float)attr.GetIntValue();
			renderState.m_marginTop = (float)attr.GetIntValue();
			break;
		case HTML_ATTR_MARGINWIDTH:
			attr.ConvertToInt();
			renderState.m_marginLeft = (float)attr.GetIntValue();
			renderState.m_marginRight = (float)attr.GetIntValue();
			break;
		case HTML_ATTR_VALIGN:
			renderState.m_verticalAlign = CHtmlLanguage::GetCurrent().GetValueId(attr.GetStringValue());
			break;
		case HTML_ATTR_WIDTH:
			attr.ConvertToInt();
			renderState.m_forcedWidth = (float)attr.GetIntValue();
			break;
		case HTML_ATTR_STYLE:
			ParseStyle(attr.GetStringValue(), renderState);
			break;
		case HTML_ATTR_COLSPAN:
			attr.ConvertToInt();
			renderState.m_colSpan = attr.GetIntValue();
			break;
		case HTML_ATTR_ROWSPAN:
			attr.ConvertToInt();
			renderState.m_rowSpan = attr.GetIntValue();
			break;
		case HTML_ATTR_SRC:
		case HTML_ATTR_ALT:
		case HTML_ATTR_NAME:
		case HTML_ATTR_CONTENT:
		case HTML_ATTR_HREF:
			//ignore
			break;
		default:
			Warningf("Unrecognised attribute %s", attr.GetName());
			break;
		}
	}

	sysMemEndTemp();
}

void CHtmlDocument::ParseStyle(const char* pStyle, HtmlRenderState& renderState)
{
	sysMemStartTemp();

	char filename[RAGE_MAX_PATH];

	fiDeviceMemory::MakeMemoryFileName(filename, sizeof(filename), pStyle, strlen(pStyle), false, NULL);

	CCssStyleGroup styleGroup;
	styleGroup.ReadFile(filename);
	styleGroup.Apply(renderState);

	sysMemEndTemp();
}

void CHtmlDocument::ProcessBody(CHtmlElementNode* pElementNode)
{
	//	parElement& element = pElementNode->m_element;

	sm_inBody = true;
	//	pElementNode->GetRenderState().m_renderBackground = true;
	m_pBodyNode = pElementNode;
}

void CHtmlDocument::ProcessLink(CHtmlElementNode* pElementNode)
{
	const parElement& element = pElementNode->GetElement();

	// get image texture
	const parAttribute* pHRef = element.FindAttribute("href");
	//	parElement& element = pElementNode->m_element;
	// add to link array, if link has valid href value
	if(pHRef)
	{
		pElementNode->m_data = pHRef->GetStringValue();
		// This is now done in the PostProcess stage
		//m_links.PushAndGrow(pElementNode);
	}
}

void CHtmlDocument::ProcessFileLink(CHtmlElementNode* pElementNode)
{
	const parElement& element = pElementNode->GetElement();

	// get image texture
	const parAttribute* pRel = element.FindAttribute("rel");
	const parAttribute* pHRef = element.FindAttribute("href");
	//	parElement& element = pElementNode->m_element;
	// add to link array, if link has valid href value
	if(pRel && !stricmp(pRel->GetStringValue(), "stylesheet"))
	{
		// if there is a href attribute it must be the stylesheet filename
		if(pHRef)
		{
			m_styleSheet.ReadFile(pHRef->GetStringValue());
		}
	}
}

void CHtmlDocument::ProcessImage(CHtmlElementNode* pElementNode)
{
	char tempBuffer[512];
	HtmlRenderState& rs = pElementNode->GetRenderState();
	const parElement& element = pElementNode->GetElement();
	grcTexture* pImage = NULL;

	// get image texture
	const parAttribute* pSrc = element.FindAttribute("src");
	Assertf(pSrc, "\"img\" tag doesn't have \"src\" attribute");
	const parAttribute* pTxd = element.FindAttribute("txd");

	if(pTxd)
	{
		strcpy(tempBuffer, pTxd->GetStringValue());
		strcat(tempBuffer, ":");
		strcat(tempBuffer, pSrc->GetStringValue());
		pElementNode->m_data = tempBuffer;
#ifndef HTML_RESOURCE
		if(pTxd->GetStringValue()[0] == '*')
		{
			pImage = grcTextureFactory::GetInstance().LookupTextureReference(pElementNode->GetTextureName());
		}
		else
		{
			int slot = g_TxdStore.FindSlot(pTxd->GetStringValue());
			Assertf(slot != -1, "%s:Txd file does not exist", pTxd->GetStringValue());
			g_TxdStore.PushCurrentTxd();
			g_TxdStore.SetCurrentTxd(slot);
			pImage = sm_loadTextureFn(pElementNode->GetTextureName());
			g_TxdStore.PopCurrentTxd();

			// add slot to txd array if it isn't already there
			s32 i;
			for(i=0; i<sm_txdIndicesUsed.GetCount(); i++)
				if(sm_txdIndicesUsed[i] == slot)
					break;
			if(i == sm_txdIndicesUsed.GetCount())
				sm_txdIndicesUsed.PushAndGrow(slot);
		}
#endif
	}
	else
	{
		pgDictionary<grcTexture>::SetCurrent(NULL);
		pElementNode->m_data = pSrc->GetStringValue();
		pImage = sm_loadTextureFn(pElementNode->m_data);
	}

	// work out width and height of image
	float width=-1;
	float height=-1;
	if(pImage)
	{
		width = (float)pImage->GetWidth();
		height = (float)pImage->GetHeight();
	}

	const parAttribute* pWidth = element.FindAttribute("width");
	const parAttribute* pHeight = element.FindAttribute("height");
	if(pWidth && pHeight)
	{
		rs.m_forcedWidth = (float)pWidth->GetIntValue();
		rs.m_forcedHeight = (float)pHeight->GetIntValue();
	}
	else if(pWidth)
	{
		rs.m_forcedWidth = (float)pWidth->GetIntValue();
		rs.m_forcedHeight = height * rs.m_width / width;
	}
	else if(pHeight)
	{
		rs.m_forcedHeight = (float)pHeight->GetIntValue();
		rs.m_forcedWidth = width * rs.m_height / height;
	}
	else
	{
		rs.m_forcedHeight = height;
		rs.m_forcedWidth = width;
	}
	rs.m_width = rs.m_forcedWidth;
	rs.m_height = rs.m_forcedHeight;
}

void CHtmlDocument::ProcessEmbed(CHtmlElementNode* UNUSED_PARAM(pElementNode))
{
	// assume all embed tags are flash movies
/*	CHtmlFlashNode* pFlashNode = dynamic_cast<CHtmlFlashNode*>(pElementNode);
	HtmlRenderState& rs = pElementNode->GetRenderState();
	Assert(pFlashNode);
	pFlashNode->Load();
	m_swfArray.PushAndGrow(pFlashNode->GetContext());

	rs.m_width = rs.m_forcedWidth;
	rs.m_height = rs.m_forcedHeight;*/
}

void CHtmlDocument::ProcessObject(CHtmlElementNode* pElementNode)
{
	// we are ignoring this object so we need to reset its renderstate so it doesn't affect its children
	HtmlRenderState rs;
	pElementNode->GetRenderState() = rs;
}

//
// name:		CHtmlDocument::ProcessScriptObject
// description:	Process related script object 
void CHtmlDocument::ProcessScriptObject(CHtmlElementNode* pElementNode)
{
	const parElement& element = pElementNode->GetElement();
	const parAttribute* pAttr = element.FindAttribute("name");
	Assertf(pAttr, "scriptobj tag doesn't have name attribute");

	pElementNode->m_data = pAttr->GetStringValue();

#ifndef HTML_RESOURCE
	if(sm_parseScriptObjects)
	{
		ParseScriptObject(pElementNode);
	}
#endif
}

#ifndef HTML_RESOURCE
//
// name:		CHtmlDocument::ParseScriptObject
// description:	
void CHtmlDocument::ParseScriptObject(CHtmlElementNode* pElementNode)
{
	sysMemStartTemp();

	s32 index = gHtmlScriptObjectMgr.Get(pElementNode->m_data);
	Assertf(index != -1, "%s:Script object doesn't exists", (const char*)pElementNode->m_data);

	// create memory stream and load html from memory
	char filename[64];

#if __DEV
	if(PARAM_debugscripthtml.Get())
	{
		Displayf("\n%s\n", gHtmlScriptObjectMgr.GetHtml(index));
	}
#endif
	fiDeviceMemory::MakeMemoryFileName(filename, 64, gHtmlScriptObjectMgr.GetHtml(index), gHtmlScriptObjectMgr.GetHtmlSize(index), false, NULL);

	// create file handle and load
	fiStream* pStream = ASSET.Open(filename, NULL);
	Assert(pStream);

	sm_pCurrent = pElementNode;

	CHtmlParser parser(pStream);

	parser.SetBeginElementCallback(parStreamIn::BeginElementCB(this, &CHtmlDocument::ReadBeginElement));
	parser.SetEndElementCallback(parStreamIn::EndElementCB(this, &CHtmlDocument::ReadEndElement));
	parser.SetDataCallback(parStreamIn::DataCB(this, &CHtmlDocument::ReadData));
	parser.ReadWithCallbacks();

	pStream->Close();

	sm_pCurrent = NULL;

	sysMemEndTemp();
}
#endif
//
// name:		CHtmlDocument::ProcessText
// description:	
void CHtmlDocument::ProcessText(CHtmlElementNode* pElementNode)
{
	// get name
	const parElement& element = pElementNode->GetElement();
	const parAttribute* pSrc = element.FindAttribute("name");
	Assertf(pSrc, "\"text\" tag doesn't have \"name\" attribute");

	pElementNode->m_data = pSrc->GetStringValue();
}

void CHtmlDocument::ProcessMetaData(CHtmlElementNode* UNUSED_PARAM(pElementNode))
{
}

void CHtmlDocument::PostProcessNode(CHtmlNode* pNode)
{
	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);

	if(pElement)
	{
		// process children nodes
		for(s32 i=0; i<pNode->GetNumChildren(); i++)
		{
			//pNode->GetChild(i)->GetRenderState().Inherit(pNode->GetRenderState());
			PostProcessNode(pNode->GetChild(i));
		}

		PostProcessElement(pElement);
	}
}

void CHtmlDocument::PostProcessElement(CHtmlElementNode* pElementNode)
{
	switch(pElementNode->m_elementId)
	{
	case HTML_TABLE:
		PostProcessTable(pElementNode);
		break;

	case HTML_A:
		PostProcessLink(pElementNode);
		break;

	default:
		break;
	}
}

void CHtmlDocument::PostProcessLink(CHtmlElementNode* pElementNode)
{
	m_links.PushAndGrow(pElementNode);
}

#define TABLE_NODE_ARRAY_SIZE 1024
//
// name:		GetTableNodes
// description:	Fillout array with all the table related nodes
void CHtmlDocument::GetTableNodes(CHtmlNode* pNode, CHtmlElementNode** ppNodes, s32& index)
{
	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
	CHtmlTableElementNode* pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNode);
	if(pElement)
	{
		if(pTableElement)
		{
			Assertf(index < TABLE_NODE_ARRAY_SIZE, "Too many table tags");
			ppNodes[index] = pElement;
			index++;
		}

		// don't process children for table entry as it may contain another table
		if(pElement->GetRenderState().m_display != HTML_TABLECELL)
		{
			// process children nodes
			for(s32 i=0; i<pNode->GetNumChildren(); i++)
			{
				GetTableNodes(pNode->GetChild(i), ppNodes, index);
			}
		}
	}
}

//
// name:		CountTableRowsAndColumns
// description:	Count the number of rows and columns a table has
void CHtmlDocument::CountTableRowsAndColumns(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode)
{
	s32 numColumns = 0;
	s32 numRows = 0;

	s32 numColumns2=0;
	for(s32 i=0; i<num; i++)
	{
		if(pNodes[i]->GetElementId() == HTML_TABLEROW)
		{
			if(numColumns2 > numColumns)
				numColumns = numColumns2;
			numRows++;
			numColumns2 = 0;
		}
		if(pNodes[i]->GetRenderState().m_display == HTML_TABLECELL)
		{
			s32 colspan = pNodes[i]->GetRenderState().m_colSpan;
			numColumns2 += colspan;	
		}
	}
	if(numColumns2 > numColumns)
		numColumns = numColumns2;
	// if found entry tags but no row tags
	if(numColumns > 0 && numRows == 0)
		numRows = 1;

	pTableNode->SetNumColumns(numColumns);
	pTableNode->SetNumRows(numRows);
}

//
// name:		CHtmlDocument::CheckTableRowsAndColumns
// description:	Check that table rows and columns haven't changed number and if so re-allocate 
void CHtmlDocument::CheckTableRowsAndColumns(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode)
{
	s32 numRows = pTableNode->GetNumRows();
	s32 numColumns = pTableNode->GetNumColumns();

	CountTableRowsAndColumns(pNodes, num, pTableNode);

	if(numRows != pTableNode->GetNumRows() || numColumns != pTableNode->GetNumColumns())
	{
		pTableNode->DeleteArrays();
		pTableNode->AllocateArrays();
	}
}

#ifndef HTML_RESOURCE
//
// name:		GetTableCellCoords
// description:	Calculate the cell coords for each table node
void CHtmlDocument::GetTableCellCoords(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode)
{
	s32* pRowSpan = Alloca(s32, pTableNode->GetNumColumns());
	// set array to 1
	for(s32 i=0; i<pTableNode->GetNumColumns(); i++)
		pRowSpan[i] = 1;

	s32 column = 0;
	s32 rows = 0;
	for(s32 i=0; i<num; i++)
	{

		if(pNodes[i]->GetElementId() == HTML_TABLEROW)
		{
			CHtmlTableElementNode* pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNodes[i]);
			Assert(pTableElement);
			pTableElement->m_row = rows;

			rows++;
			column = 0;
		}
		if(pNodes[i]->GetRenderState().m_display == HTML_TABLECELL)
		{
			//If we have hit an entry and not hit a row increment row counter
			if(rows == 0)
				rows = 1;

			while(pRowSpan[column] > rows && column < pTableNode->GetNumColumns())
				column++;
			Assert(column != pTableNode->GetNumColumns());

			CHtmlTableElementNode* pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNodes[i]);
			Assert(pTableElement);
			pTableElement->m_row = rows-1;
			pTableElement->m_column = column;
			s32 colSpan = pTableElement->GetRenderState().m_colSpan;
			while(colSpan--)
			{
				pRowSpan[column] = rows + pTableElement->GetRenderState().m_rowSpan;
				column++;
			}
//			pRowSpan[column] = rows + pTableElement->GetRenderState().m_rowSpan;
//			column += pTableElement->GetRenderState().m_colSpan;

		}
	}

}

//
// name:		GetTableWidthsAndHeights
// description:	Get the column width and row heights for a table
void CHtmlDocument::GetTableWidthsAndHeights(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode)
{
	for(s32 i=0; i<num; i++)
	{
		CHtmlTableElementNode* pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNodes[i]);
		Assert(pTableElement);
		if(pTableElement->GetElementId() == HTML_TABLEROW)
		{
			if(pTableElement->GetRenderState().m_height > 0.0f)
				pTableNode->SetRowHeight(pTableElement->m_row, pNodes[i]->GetRenderState().m_height);
		}
		if(pTableElement->GetRenderState().m_display == HTML_TABLECELL)
		{
			s32 colspan = pTableElement->GetRenderState().m_colSpan;

			float minWidth, maxWidth;
			GetNodeMinMaxWidth(pTableElement, minWidth, maxWidth);
			//GetForcedNodeDimensions(pNodes[i], childSize);
			// can't force width for entries that span more than one column at the moment
			if(colspan == 1)
			{
				pTableNode->SetColumnMinWidth(pTableElement->m_column, 
					Max<float>(minWidth, pTableNode->GetColumnMinWidth(pTableElement->m_column)));
				pTableNode->SetColumnMaxWidth(pTableElement->m_column, 
					Max<float>(maxWidth, pTableNode->GetColumnMaxWidth(pTableElement->m_column)));
			}
		}
	}
}

#endif
//
// name:		CHtmlDocument::PostProcessTable
// description:	Calculate table size and sizes that are forced
void CHtmlDocument::PostProcessTable(CHtmlElementNode* pElementNode)
{
	CHtmlTableNode* pTable = dynamic_cast<CHtmlTableNode*>(pElementNode);
	CHtmlElementNode* pNodes[TABLE_NODE_ARRAY_SIZE];
	s32 numNodes=0;
	Assert(pTable);

	GetTableNodes(pTable, pNodes, numNodes);
	CountTableRowsAndColumns(pNodes, numNodes, pTable);
	// allocate arrays for row and column sizes
	pTable->AllocateArrays();
//	GetTableCellCoords(pNodes, numNodes, pTable);
//	GetTableWidthsAndHeights(pNodes, numNodes, pTable);
}

void CHtmlDocument::SetTitle(GxtChar* pTitle)
{
	s32 length = StringLength(pTitle);
	// delete old string if it existed
	if(m_pTitle)
		delete[] m_pTitle;
	// allocate space for new one and copy
	m_pTitle = rage_new GxtChar[length + 1];
	safecpy(m_pTitle.ptr, pTitle, length+1);
}

//
// name:		CHtmlDocument::ProcessData
// description:	Process data
void CHtmlDocument::ProcessData(CHtmlDataNode* pDataNode)
{
	Assert(pDataNode->GetParent());
	CHtmlElementNode* pParentNode = dynamic_cast<CHtmlElementNode*>(pDataNode->GetParent());
	Assert(pParentNode);

	switch(pParentNode->m_elementId)
	{
	case HTML_TITLE:
		SetTitle(pDataNode->GetData());
		break;

	default:
		break;
	}

/*	if(sm_inBody)
	{
		p
	}*/
}
#ifndef HTML_RESOURCE
//
// name:		CHtmlDocument::GetNodeMinMaxWidth/GetElementMinMaxWidth/GetDataMinMaxWidth
// description:	Get minimum and maximum width of node for table dimension calculations
void CHtmlDocument::GetNodeMinMaxWidth(CHtmlNode* pNode, float& minWidth, float& maxWidth)
{
	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
	CHtmlDataNode* pData = dynamic_cast<CHtmlDataNode*>(pNode);
	
	minWidth = 0.0f;
	maxWidth = 0.0f;
	if(pNode->GetRenderState().m_forcedWidth > 0.0f)
	{
		minWidth = pElement->GetRenderState().m_forcedWidth;
		maxWidth = pElement->GetRenderState().m_forcedWidth;
	}

	if(pElement)
	{
		float localMin = 0.0f;
		float localMax = 0.0f;

		if(GetElementMinMaxWidth(pElement, minWidth, maxWidth) == false)
		{
			// process children nodes
			for(s32 i=0; i<pNode->GetNumChildren(); i++)
			{
				GetNodeMinMaxWidth(pNode->GetChild(i), localMin, localMax);
				minWidth = rage::Max(localMin, minWidth);
				maxWidth = rage::Max(localMax, maxWidth);
			}
		}
	}
	else if(pData)
		GetDataMinMaxWidth(pData, minWidth, maxWidth);

}

bool CHtmlDocument::GetElementMinMaxWidth(CHtmlElementNode* pElement, float& minWidth, float& maxWidth)
{
	if(pElement->GetElementId() == HTML_IMAGE)
	{
		minWidth = pElement->GetRenderState().m_width;
		maxWidth = pElement->GetRenderState().m_width;
		return true;
	}
	// don't parse into tables
	else if(pElement->GetElementId() == HTML_TABLE)
	{
		CHtmlTableNode* pTable = dynamic_cast<CHtmlTableNode*>(pElement);
		Assert(pTable);
		minWidth = rage::Max(pTable->GetMinWidth(), minWidth);
		maxWidth = rage::Max(pTable->GetMaxWidth(), maxWidth);
		return true;
	}
	else if(pElement->GetElementId() == HTML_TEXT)
	{
		// get text
		GxtChar* pString = TheText.Get(pElement->GetAttributeData());
		pElement->GetRenderState().SetFontProperties();
#ifndef HTML_RESOURCE
		minWidth = (float)CTextFormat::GetMaxWordLength(&sm_HtmlTextLayout, pString);
		maxWidth = (float)(sm_HtmlTextLayout.GetStringWidthOnScreen(pString, true)*GetFormattedWidth());  // 1st screen done
#else
		pString;
		minWidth = maxWidth = 0.0f;
#endif
	}
	return false;
}

void CHtmlDocument::GetDataMinMaxWidth(CHtmlDataNode* pData, float& minWidth, float& maxWidth)
{
	pData->GetRenderState().SetFontProperties();

#ifndef HTML_RESOURCE
	minWidth = (float)CTextFormat::GetMaxWordLength(&sm_HtmlTextLayout, pData->GetData());
	maxWidth = (float)(sm_HtmlTextLayout.GetStringWidthOnScreen(pData->GetData(), true)*GetFormattedWidth());  // 1st screen done
#else
	minWidth = maxWidth = 0.0f;
#endif
}

//
// name:		CHtmlDocument::GetForcedNodeDimensions
// description:	Get dimensions of node if it is forced
void CHtmlDocument::GetForcedNodeDimensions(CHtmlNode* pNode, Vector2& size)
{
	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
	size.x = -1.0f;
	size.y = -1.0f;

	// if this is an image node return width of the image
	if(pElement)
	{
		HtmlRenderState& rs = pElement->GetRenderState();
		if(pElement->GetElementId() == HTML_IMAGE)
		{
			size.x = rs.m_width;
			size.y = rs.m_height;
			return;
		}
		// don't parse into tables
		if(pElement->GetElementId() == HTML_TABLE)
			return;
	}
	// if this isn't an element node then return invalid width
	else
	{
		return;
	}

	// process children nodes. If one returns an invalid width then return as we cannot force the width 
	// of this node
	for(s32 i=0; i<pElement->GetNumChildren(); i++)
	{
		GetForcedNodeDimensions(pElement->GetChild(i), size);
		if(size.x == -1.0f)
			return;
	}
}

//
// name:		CHtmlDocument::GetMinimumNodeDimensions
// description:	Get mnimum dimensions of node if it is forced
/*void CHtmlDocument::GetMinimumNodeDimensions(CHtmlNode* pNode, Vector2& size)
{
	CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);

	// if this is an image node return width of the image
	if(pElement)
	{
		HtmlRenderState& rs = pElement->GetRenderState();
		if(rs.m_width > size.x)
			size.x = rs.m_width;
		if(rs.m_height > size.y)
			size.y = rs.m_height;

		// process children nodes. If one returns an invalid width then return as we cannot force the width 
		// of this node
		for(s32 i=0; i<pElement->GetNumChildren(); i++)
		{
			GetMinimumNodeDimensions(pElement->GetChild(i), size);
		}
	}
}*/

//
// name:		CHtmlDocument::GetFormattedDimensions
// description:	Get dimensions document has been formatted to
void CHtmlDocument::GetFormattedDimensions(float& x, float& y)
{
	x = m_pRootNode->GetRenderState().m_forcedWidth;
	y = m_pRootNode->GetRenderState().m_forcedHeight;
}

//
// name:		CHtmlDocument::GetFormattedWidth
// description:	Get width document has been formatted to
float CHtmlDocument::GetFormattedWidth()
{
	return m_pRootNode->GetRenderState().m_forcedWidth;
}

//
// name:		CHtmlDocument::FormatDocument
// description:	Setups renderstate and positional info for each node based on window size
void CHtmlDocument::FormatDocument(s32 width, s32 height)
{
	Vector2 posn(0.0f, 0.0f);
	Vector2 size(0.0f, 0.0f);

	m_pRootNode->GetRenderState().m_forcedWidth = (float)width;
	m_pRootNode->GetRenderState().m_forcedHeight = (float)height;
	m_pRootNode->GetRenderState().m_currentX = 0.0f;
	m_pRootNode->GetRenderState().m_currentY = 0.0f;

	sm_inBody = false;

	//GetNodeSize(m_pRootNode, size);
	FormatNode(m_pRootNode, posn);
}

//
// name:		CHtmlDocument::FormatNode
// description:	Format positions of node
void CHtmlDocument::FormatNode(CHtmlNode* pNode, Vector2& posn)
{
	Vector2 posn2 = posn;
	
	//CHtmlNode* pParent = pNode->GetParent();
	// If not the root node
	//if(pParent)
	{
		CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);

		if(pNode->GetRenderState().m_display != HTML_INLINE)
			FormatInlineObjectList(posn2);

		if(pNode->GetNumChildren() > 0 || pNode->GetRenderState().m_display != HTML_INLINE)
		{
			posn = posn2;

			if(pNode->GetRenderState().m_display == HTML_INLINE)
				AddInlineObject(pNode);//sm_inlineObjs.PushAndGrow(pNode);

			Assert(pElement);
			FormatElement(pElement, posn2);

			UpdatePosition(pNode, posn, posn2);

		}
		else
			AddInlineObject(pNode);//sm_inlineObjs.PushAndGrow(pNode);
	}

}

void CHtmlDocument::UpdatePosition(CHtmlNode* pNode, Vector2& posn, const Vector2& newPosn)
{
	CHtmlNode* pParentNode = pNode->GetParent();
	if(pParentNode == NULL)
		return;

	HtmlRenderState& rs = pNode->GetRenderState();
	float x, y;
	float width, height;

	pParentNode->GetDimensions(x, y, width, height);

	// If render state width is valid then use that as a delta on the position
	// otherwise just use the returned position from FormatNode()
	Vector2 delta;

	if(rs.m_width != -1)
		delta.x = (float)rs.m_width;
	else
		delta.x = newPosn.x - posn.x;
	if(rs.m_height != -1)
		delta.y = (float)rs.m_height;
	else
		delta.y = newPosn.y - posn.y;

	/*if(rs.m_currentX + delta.x + rs.m_marginRight <= x + width && rs.m_display == HTML_INLINE)
		posn.x = rs.m_currentX + delta.x;
	else*/
	{
		posn.x = x;
		posn.y = rs.m_currentY + delta.y;// + rs.m_marginBottom + rs.m_borderBottomWidth;
		posn.y += rs.m_borderBottomWidth;
		// if not last child add margin bottom
		CHtmlNode* pLastChild = NULL;
		if(pNode->GetParent())
		{
			pLastChild = pNode->GetParent()->GetChild(pNode->GetParent()->GetNumChildren()-1);
		}
		if(pLastChild != pNode)
			posn.y += rs.m_marginBottom;
	}
}

//
// name:		CHtmlDocument::FormatElement
// description:	
void CHtmlDocument::FormatElement(CHtmlElementNode* pElementNode, Vector2& posn)
{
	HtmlRenderState& rs = pElementNode->GetRenderState();
	float x, y, width, height;

	rs.m_width = rs.m_forcedWidth;
	rs.m_height = rs.m_forcedHeight;

	pElementNode->GetDimensions(x,y,width,height);

	// force width and height on all block display nodes
	if(rs.m_display == HTML_BLOCK)
	{
		if(pElementNode->GetElementId() != HTML_TABLE)
		{
			if(rs.m_width < 0.0f)
				rs.m_width = width;
		}

		// the first child of block nodes can't set left and top margins
		if(pElementNode->GetParent() && 
			pElementNode->GetParent()->GetRenderState().m_display != HTML_INLINE &&
			pElementNode->GetParent()->GetChild(0) == pElementNode)
		{
			rs.m_marginLeft = 0.0f;
			rs.m_marginTop = 0.0f;
		}
	}
	// non-block nodes can't set the margins or have borders
	else if(rs.m_display == HTML_INLINE)
	{
		rs.m_marginLeft = 0.0f;
		rs.m_marginRight = 0.0f;
		rs.m_paddingLeft = 0.0f;
		rs.m_paddingRight = 0.0f;

		rs.m_borderLeftStyle = HTML_NONE;
		rs.m_borderRightStyle = HTML_NONE;
		rs.m_borderTopStyle = HTML_NONE;
		rs.m_borderBottomStyle = HTML_NONE;
	}

	// set border widths
	if(rs.m_borderLeftStyle == HTML_NONE)
		rs.m_borderLeftWidth = 0.0f;
	if(rs.m_borderRightStyle == HTML_NONE)
		rs.m_borderRightWidth = 0.0f;
	if(rs.m_borderTopStyle == HTML_NONE)
		rs.m_borderTopWidth = 0.0f;
	if(rs.m_borderBottomStyle == HTML_NONE)
		rs.m_borderBottomWidth = 0.0f;
	// set border style
	if(rs.m_borderLeftWidth == 0.0f)
		rs.m_borderLeftStyle = HTML_NONE;
	if(rs.m_borderRightWidth == 0.0f)
		rs.m_borderRightStyle = HTML_NONE;
	if(rs.m_borderTopWidth == 0.0f)
		rs.m_borderTopStyle = HTML_NONE;
	if(rs.m_borderBottomWidth == 0.0f)
		rs.m_borderBottomStyle = HTML_NONE;

	// Add margin to position. Padding doesn't affect position of this node only nodes that are 
	// children of this node
	posn.x += rs.m_marginLeft + rs.m_borderLeftWidth;
	posn.y += rs.m_marginTop + rs.m_borderTopWidth;

	// margins affect width and height
	if(rs.m_width > 0.0f)
	{
		rs.m_width -= rs.m_marginLeft + rs.m_marginRight + rs.m_borderLeftWidth + rs.m_borderRightWidth;
		if(rs.m_width < 0.0f)
			rs.m_width = 0.0f;
	}
/*	if(rs.m_height > 0.0f)
	{
		rs.m_height -= rs.m_marginTop + rs.m_marginBottom;
		if(rs.m_height < 0.0f)
			rs.m_height = 0.0f;
	}*/

	// set current position
	rs.m_currentX = posn.x;
	rs.m_currentY = posn.y;

	switch(pElementNode->m_elementId)
	{
	case HTML_BODY:
		sm_inBody = true;
		break;

	case HTML_TABLE:
		FormatTable(pElementNode, posn);
		AlignNode(pElementNode);
		return;

	default:
		break;
	}

	posn.x += rs.m_paddingLeft;
	posn.y += rs.m_paddingTop;

	// process children nodes
	for(s32 i=0; i<pElementNode->GetNumChildren(); i++)
	{
		FormatNode(pElementNode->GetChild(i), posn);
	}

	if(rs.m_display != HTML_INLINE)
		FormatInlineObjectList(posn);

	if(sm_inBody)
	{
		// Set block size
		if(rs.m_display == HTML_BLOCK)
		{
			posn.y += rs.m_paddingBottom;
			rs.m_height = posn.y - rs.m_currentY;
/*			posn.y += rs.m_borderBottomWidth;
			// if not last child
			CHtmlNode* pLastChild = NULL;
			if(pElementNode->GetParent())
			{
				pLastChild = pElementNode->GetParent()->GetChild(pElementNode->GetParent()->GetNumChildren()-1);
			}
			if(pLastChild != pElementNode)
				posn.y += rs.m_marginBottom;*/
		}

		if(pElementNode->m_elementId == HTML_BODY)
		{
			//		rs.m_height = height;
			sm_inBody = false;
		}

		AlignNode(pElementNode);
	}
}

//
// name:		FilloutTableColumnWidths
// description:	Fillout table element widths
void CHtmlDocument::FilloutTableColumnWidths(CHtmlElementNode** pNodes, s32 numNodes, CHtmlTableNode* pTableNode)
{
	HtmlRenderState& trs = pTableNode->GetRenderState();

	for(s32 i=0; i<numNodes; i++)
	{
		if(pNodes[i]->GetRenderState().m_display == HTML_TABLECELL)
		{
			CHtmlTableElementNode* pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNodes[i]);
			Assert(pTableElement);
			HtmlRenderState& rs = pTableElement->GetRenderState();

			// get column span
			s32 colspan = pTableElement->GetRenderState().m_colSpan;

			// add column widths onto table entry width
			rs.m_forcedWidth = 0;
			for(s32 j=0; j<colspan; j++)
				rs.m_forcedWidth += pTableNode->GetColumnWidth(pTableElement->m_column+j) + trs.m_cellspacing;
			// take off last spacing values
			rs.m_forcedWidth -= trs.m_cellspacing;
		}
	}
}

//
// name:		FormatTableEntries
// description:	Calculate table entry positions and format their children
void CHtmlDocument::FormatTableEntries(CHtmlElementNode** pNodes, s32 numNodes, CHtmlTableNode* pTable)
{
	HtmlRenderState& trs = pTable->GetRenderState();
	//int rows = 0;
	int columns = 0;
	Vector2 tableRootPosn = Vector2(trs.m_currentX, trs.m_currentY);
	Vector2 currRowPosn = tableRootPosn;
	float maxHeight = tableRootPosn.y;
	CHtmlTableElementNode* pTableElement = NULL;

	currRowPosn.x += trs.m_cellspacing;
	currRowPosn.y += trs.m_cellspacing;

	for(s32 i=0; i<numNodes; i++)
	{
		pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNodes[i]);
		Assert(pTableElement);
		if(pTableElement->GetElementId() == HTML_TABLEROW)
		{
			// store previous row height
			if(pTableElement->m_row > 0)
			{
				maxHeight += trs.m_cellpadding;
				pTable->SetCalculatedRowHeight(pTableElement->m_row-1, maxHeight - currRowPosn.y - trs.m_borderTopWidth - trs.m_borderBottomWidth);
			}

			// set current row position
			currRowPosn.x = tableRootPosn.x + trs.m_cellspacing;
			currRowPosn.y = maxHeight + trs.m_cellspacing;

			// set new max height
			// if row height is set then set max height
			HtmlRenderState& rs = pTableElement->GetRenderState();
			if(rs.m_height > 0.0f)
				maxHeight = currRowPosn.y + rs.m_height;

			columns = 0;
		}
		else if(pTableElement->GetRenderState().m_display == HTML_TABLECELL)
		{
			HtmlRenderState& rs = pTableElement->GetRenderState();

			// get position of cell
			for(s32 j=columns; j<pTableElement->m_column; j++)
				currRowPosn.x += pTable->GetColumnWidth(j) + trs.m_cellspacing;

			Vector2 posn(currRowPosn);
			Vector2 posn2(currRowPosn);

			rs.m_paddingLeft = trs.m_cellpadding;
			rs.m_paddingRight = trs.m_cellpadding;
			rs.m_paddingTop = trs.m_cellpadding;
			rs.m_paddingBottom = trs.m_cellpadding;

			// format table entry and its children
			//FormatNode(pTableElement, posn);
			FormatElement(pTableElement, posn2);

			rs.m_cellHeight = posn2.y - currRowPosn.y;
			rs.m_height = rage::Max(rs.m_cellHeight, rs.m_height);

			UpdatePosition(pTableElement, posn, posn2);

			//if(rs.m_height + currRowPosn.y + trs.m_borderTopWidth + trs.m_borderBottomWidth > posn.y)
			//	posn.y = rs.m_height + currRowPosn.y + trs.m_borderTopWidth + trs.m_borderBottomWidth;

			// has it increased the height of this row?
			if(rs.m_rowSpan < 2)
				maxHeight = rage::Max(posn.y, maxHeight);
			
			columns = pTableElement->m_column;
		}
	}

	// store previous row height
	maxHeight += trs.m_cellpadding;
	if(pTableElement)
		pTable->SetCalculatedRowHeight(pTableElement->m_row, maxHeight - currRowPosn.y - trs.m_borderTopWidth - trs.m_borderBottomWidth);

	// set table height
	float height = maxHeight - pTable->GetRenderState().m_currentY + trs.m_cellspacing;
	float cellscaling = 1.0f;
	// if calculated height is greater than current set height then set table height. 
	if(height >= pTable->GetRenderState().m_height)
		pTable->GetRenderState().m_height = height;
	// otherwise scale up all 
	else if(pTable->GetNumRows() > 0)
	{
		float forcedHeight = pTable->GetRenderState().m_height;
		float currentHeight = 0.0f;
		
		forcedHeight -= trs.m_cellspacing * (pTable->GetNumRows() + 1);
		forcedHeight -= (trs.m_borderTopWidth + trs.m_borderBottomWidth) * (pTable->GetNumRows() + 1);
		//forcedHeight -= trs.m_cellpadding * pTable->GetNumRows() * 2.0f;

		for(s32 i=0; i<pTable->GetNumRows(); i++)
			currentHeight += pTable->GetRowHeight(i);

		cellscaling = forcedHeight / currentHeight;
		Assert(cellscaling >= 1.0f);
	}

	Vector2 offset(0.0f, 0.0f);
	float nextYOffset = 0.0f;
	// set table cell positions based on new row heights
	for(s32 i=0; i<numNodes; i++)
	{
		pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNodes[i]);
		HtmlRenderState& rs = pTableElement->GetRenderState();
		if(pTableElement->GetElementId() == HTML_TABLEROW)
		{
			offset.y = nextYOffset;
			// calculate how much we have to offset the table cells by
			nextYOffset += pTable->GetRowHeight(pTableElement->m_row) * (cellscaling - 1.0f);
			pTable->SetCalculatedRowHeight(pTableElement->m_row, pTable->GetRowHeight(pTableElement->m_row) * cellscaling);
		}
		else if(rs.m_display == HTML_TABLECELL)
		{
			// If cell is to be offset, offset the cell and then all its children
			if(offset.y > 0.0f)
			{
				pTableElement->GetRenderState().m_currentY += offset.y;
				for(s32 i=0; i<pTableElement->GetNumChildren(); i++)
					OffsetPositions(pTableElement->GetChild(i), offset);

			}
		}
	}

	// set table entry heights, and vertically align contents
	for(s32 i=0; i<numNodes; i++)
	{
		pTableElement = dynamic_cast<CHtmlTableElementNode*>(pNodes[i]);
		HtmlRenderState& rs = pTableElement->GetRenderState();
		if(rs.m_display == HTML_TABLECELL)
		{
			// vertical align
			pTableElement->GetRenderState().m_height = 0.0f;
			for(s32 j=0; j<rs.m_rowSpan; j++)
			{
				if(pTableElement->m_row+j < pTable->GetNumRows())
				{
					rs.m_height += (pTable->GetRowHeight(pTableElement->m_row+j));
					rs.m_height += trs.m_cellspacing + trs.m_borderTopWidth + trs.m_borderBottomWidth;
				}
			}
			rs.m_height -= trs.m_cellspacing + trs.m_borderTopWidth + trs.m_borderBottomWidth;

			VerticalAlignNode(pTableElement);
		}
	}
}


void CHtmlDocument::FormatTable(CHtmlElementNode* pNode, Vector2& posn)
{
	CHtmlTableNode* pTable = dynamic_cast<CHtmlTableNode*>(pNode);
	CHtmlElementNode* pNodes[TABLE_NODE_ARRAY_SIZE];
	s32 numNodes=0;
	Assert(pTable);

	// get list of table rows and entries
	GetTableNodes(pTable, pNodes, numNodes);

	// count table rows and columns and if they are different re-allocate arrays
	CheckTableRowsAndColumns(pNodes, numNodes, pTable);
	GetTableCellCoords(pNodes, numNodes, pTable);
	GetTableWidthsAndHeights(pNodes, numNodes, pTable);

	// calculate the size of the columns that don't have any size info
	pTable->CalculateColumnWidths();

	// fillout their width values
	FilloutTableColumnWidths(pNodes, numNodes, pTable);
	// format contents of the table entries and set table height
	FormatTableEntries(pNodes, numNodes, pTable);

	posn.y += pTable->GetRenderState().m_height;
}

//
// name:		CHtmlDocument::AlignNode
// description:	Align node according to alignment render state
void CHtmlDocument::AlignNode(CHtmlElementNode* pNode)
{
	CHtmlNode* pBlockNode = pNode->GetNonInlineNode();
	HtmlRenderState& rs = pNode->GetRenderState();
	HtmlRenderState& brs = pBlockNode->GetRenderState();
	Vector2 offset(0.0f, 0.0f);

	// cannot align table entries or rows
	if(pNode->GetRenderState().m_display == HTML_TABLECELL || pNode->GetRenderState().m_display == HTML_TABLESECTION)
		return;

	if(rs.m_display == HTML_BLOCK)
	{
		s32 textAlign = rs.m_textAlign;
		if(textAlign == HTML_UNDEFINED)
			textAlign = brs.m_textAlign;
//		Assert(brs.m_width >= rs.m_width);

		float diffWidth = brs.m_width - rs.m_width;
		if(brs.m_width < rs.m_width)
			diffWidth = 0.0f;

		if(textAlign == HTML_ALIGN_RIGHT)
			offset.x = diffWidth;
		else if(textAlign == HTML_ALIGN_CENTER)
			offset.x = diffWidth / 2.0f;

		if(offset.x != 0.0f)
			OffsetPositions(pNode, offset);
	}
}

//
// name:		CHtmlDocument::VerticalAlignNode
// description:	Vertical align children nodes according to alignment render state and the height passed in
void CHtmlDocument::VerticalAlignNode(CHtmlElementNode* pNode)
{
	HtmlRenderState& rs = pNode->GetRenderState();
	Vector2 offset(0.0f, 0.0f);

//	Assert(rs.m_cellHeight <= rs.m_height);
	if(rs.m_cellHeight == 0.0f)
	{
		return;
	}

	float diffHeight = rage::Max(rs.m_height - rs.m_cellHeight, 0.0f);
	
	if(rs.m_verticalAlign == HTML_ALIGN_BOTTOM)
		offset.y = diffHeight;
	else if(rs.m_verticalAlign == HTML_ALIGN_MIDDLE)
		offset.y = diffHeight / 2.0f;

	// offset children
	if(offset.y != 0.0f)
	{
		for(s32 i=0; i<pNode->GetNumChildren(); i++)
			OffsetPositions(pNode->GetChild(i), offset);
	}
}
//
// name:		CHtmlDocument::FormatData
// description:	
void CHtmlDocument::FormatData(CHtmlDataNode* pDataNode, Vector2& posn)
{
	HtmlRenderState& rs = pDataNode->GetRenderState();

	// set current position
	rs.m_currentX = posn.x;
	rs.m_currentY = posn.y;

	Vector2 vPos;
	float width, height;
	float formattedWidth, formattedHeight;

	pDataNode->GetDimensions(vPos.x,vPos.y,width,height);
	GetFormattedDimensions(formattedWidth, formattedHeight);

	rs.SetFontProperties();

	vPos.x = rs.m_currentX;
	vPos.y = rs.m_currentY;

#ifndef HTML_RESOURCE
	sm_HtmlTextLayout.SetWrap(Vector2(vPos.x/formattedWidth, (vPos.x+width)/formattedWidth));
#endif

	if(rs.m_textAlign == HTML_ALIGN_RIGHT)
		vPos.x += width;
	else if(rs.m_textAlign == HTML_ALIGN_CENTER)
		vPos.x += width / 2.0f;

	// temporary stuff to get size of lines
#ifndef HTML_RESOURCE
	s32 lines = sm_HtmlTextLayout.GetNumberOfLines(&vPos, pDataNode->GetData());
	posn.y += lines * ((CTextFormat::GetLineHeight(sm_HtmlTextLayout)*formattedHeight));
#endif
}

//
// name:		CHtmlDocument::FormatInlineObjectList
// description:	Format the list of data objec
void CHtmlDocument::FormatInlineObjectList(Vector2& posn)
{
	Vector2 posn2 = posn;
	if(sm_inBody)
	{
		float formattedWidth, formattedHeight;
		GetFormattedDimensions(formattedWidth, formattedHeight);

		if(GetInlineObjectCount() > 0)
		{
			float x,y;
			float width, height;
			CHtmlNode* pNonInlineNode = GetInlineObject(0)->GetNonInlineNode();//sm_inlineObjs[0]->GetNonInlineNode();
			CHtmlNode* pLastLinkNode = NULL;

			// get dimensions from first objects parent
			pNonInlineNode->GetDimensions(x,y,width,height);

			CHtmlTextFormat fn(x, posn.y, width, pNonInlineNode->GetRenderState().m_textAlign, formattedWidth, formattedHeight);

			for(s32 i=0; i<GetInlineObjectCount(); i++)
			{
				CHtmlNode* pNode = GetInlineObject(i);
				CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
				CHtmlDataNode* pData = dynamic_cast<CHtmlDataNode*>(pNode);

				if(pData)
				{
					HtmlRenderState& rs = pData->GetRenderState();
					rs.SetFontProperties();
					fn.SetHtmlNode(pNode);

#ifndef HTML_RESOURCE
					sm_HtmlTextLayout.SetWrap(Vector2(x/formattedWidth, (x+width)/formattedWidth));  // 1st screen done maybe
					sm_HtmlTextLayout.SetOrientation(FONT_LEFT);

					CTextFormat::ProcessCurrentString(&sm_HtmlTextLayout, (posn.x + fn.GetCurrentX())/formattedWidth, posn.y/formattedHeight, pData->GetData(), fn);
#endif
				}
				else if(pElement)
				{
					// only know how to format images, flash movies
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
						sm_HtmlTextLayout.SetWrap(Vector2(x/formattedWidth, (x+width)/formattedWidth));
						sm_HtmlTextLayout.SetOrientation(FONT_LEFT);

						CTextFormat::ProcessCurrentString(&sm_HtmlTextLayout, (posn.x + fn.GetCurrentX())/formattedWidth, posn.y/formattedHeight, TheText.Get(pElement->GetAttributeData()), fn);
#endif
					}
					else if(pElement->GetElementId() == HTML_BREAK)
					{
						pElement->GetRenderState().SetFontProperties();
						fn.SetHtmlNode(pNode);
#ifndef HTML_RESOURCE
						fn.AddNonTextBlock(1, (CTextFormat::GetLineHeight(sm_HtmlTextLayout)*formattedHeight));
#endif
						fn.NewLine();
					}
					else
					{
						pElement->GetRenderState().m_currentX = posn.x + fn.GetCurrentX();
						pElement->GetRenderState().m_currentY = posn.y + fn.GetCurrentY();
					}
				}

				// If we are working out the size of a link and the current node's parent was this link then calculate size of link
				if(pLastLinkNode && pNode->GetParent() == pLastLinkNode)
				{
					if(fn.GetCurrentY() != pLastLinkNode->GetRenderState().m_currentY - posn.y)
					{
						pLastLinkNode->GetRenderState().m_width = fn.GetCurrentX();
						pLastLinkNode->GetRenderState().m_height = posn.y + fn.GetCurrentY() + fn.GetCurrentLineHeight() - pLastLinkNode->GetRenderState().m_currentY;
					}
					else
					{
						pLastLinkNode->GetRenderState().m_width = posn.x + fn.GetCurrentX() - pLastLinkNode->GetRenderState().m_currentX;
						pLastLinkNode->GetRenderState().m_height = fn.GetCurrentLineHeight();
					}
				}
				else
					pLastLinkNode = NULL;
				// Have to do this after getting size of link from child
				if(pElement && pElement->GetElementId() == HTML_A)
					pLastLinkNode = pElement;
			}
			fn.NewLine();
			posn.y += fn.GetCurrentY();

			// work out link positions
			for(s32 i=0; i<GetInlineObjectCount(); i++)
			{
				CHtmlNode* pNode = GetInlineObject(i);
				CHtmlElementNode* pElement = dynamic_cast<CHtmlElementNode*>(pNode);
				// Position of link is defined by the position of the first entry
				if(pLastLinkNode && pNode->GetParent() == pLastLinkNode)
				{
					pLastLinkNode->GetRenderState().m_currentX = pNode->GetRenderState().m_currentX;
					pLastLinkNode->GetRenderState().m_currentY = pNode->GetRenderState().m_currentY;
				}
				pLastLinkNode = NULL;
				// Have to do this after getting size of link from child
				if(pElement && pElement->GetElementId() == HTML_A)
					pLastLinkNode = pElement;
			}
		}
	}

	ResetInlineObjectList();//sm_inlineObjs.Reset();
}

#endif  //HTML_FORMAT

//
// name:		OffsetPositions
// description:	Offset the position of children nodes
void CHtmlDocument::OffsetPositions(CHtmlNode* pNode, const Vector2& posn)
{
	pNode->GetRenderState().m_currentX += posn.x;
	pNode->GetRenderState().m_currentY += posn.y;
	for(s32 i=0; i<pNode->GetNumChildren(); i++)
	{
		CHtmlNode* pChild = pNode->GetChild(i);
		OffsetPositions(pChild, posn);
	}
}

//
// name:		CHtmlDocument::GetLinkPosition
// description:	Return position of link
void CHtmlDocument::GetLinkPosition(s32 link, float& x, float& y) 
{
	x = m_links[link]->GetRenderState().m_currentX; 
	y= m_links[link]->GetRenderState().m_currentY;
}

//
// name:		CHtmlDocument::LoadTextureInternal
// description:	Load a texture and add it to the texture dictionary
grcTexture* CHtmlDocument::LoadTextureInternal(const char* pName)
{
	grcTexture* pImage;

	// If we have a current txd then check inside that for the texture
	if(pgDictionary<grcTexture>::GetCurrent())
	{
		pImage = pgDictionary<grcTexture>::GetCurrent()->Lookup(pName);
		if(pImage)
			return pImage;
	}
	// If texture dictionary isn't constructed build it and reserve 256 slots
	if(m_pTxd == NULL)
		m_pTxd = rage_new pgDictionary<grcTexture>(256);

	// Is image already in texture dictionary
	pImage = m_pTxd->Lookup(pName);
	if(pImage)
		return pImage;

	char filename[RAGE_MAX_PATH];
	ASSET.RemoveExtensionFromPath(filename, RAGE_MAX_PATH, pName);
	pImage = grcTextureFactory::GetInstance().Create(filename);
	if(pImage)
	{
		// Add entry to texture dictionary
		m_pTxd->AddEntry(pName, pImage);
	}
	else
	{
		Errorf("Cannot load image %s", pName);
	}
	return pImage;
}

#if __DEV
void CHtmlDocument::DebugPrint()
{
	Printf("Title: %s\n", (const char*)(GxtChar*)m_pTitle);
	m_pRootNode->DebugPrint();
}
#endif
