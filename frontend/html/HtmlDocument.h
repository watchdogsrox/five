//
// filename:	HtmlDocument.h
// description:	
//

#ifndef INC_HTMLDOCUMENT_H_
#define INC_HTMLDOCUMENT_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "data/struct.h"
#include "grcore/texture.h"
#include "paging/dictionary.h"
#include "atl/functor.h"
#include "vector/vector2.h"
// Game headers
#include "css.h"
//#include "HtmlLanguage.h"
//#include "HtmlNode.h"
//#include "HtmlRenderState.h"
#include "text/text.h"
#include "paging/base.h"
// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

class CHtmlNode;
class CHtmlDataNode;
class CHtmlElementNode;
class CHtmlTableNode;
struct HtmlRenderState;

namespace rage {
	class parElement;
};

//
// name:		CHtmlDocument
// description:	Class containing root for HTML node tree
class CHtmlDocument : public pgBase
{
	friend class CHtmlElementNode;
public:
	typedef Functor1Ret<grcTexture*, const char*> LoadTextureFn;

	CHtmlDocument();
	CHtmlDocument(datResource& rsc);

	DECLARE_PLACE(CHtmlDocument);

	~CHtmlDocument();

	static void Init(unsigned initMode);

#if __DECLARESTRUCT
	void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL
	void Release() {delete this;}

	bool LoadDocument(const char* pFilename);
	void Delete();

	// access functions
	CHtmlElementNode* GetRootNode() {return m_pRootNode;}
	CHtmlElementNode* GetBodyNode() {return m_pBodyNode;}
	
	void Process();
	void PostProcess();
	void FormatDocument(s32 width, s32 height);
	void GetFormattedDimensions(float& x, float& y);
	float GetFormattedWidth();

	void SetTitle(GxtChar* pTitle);
	GxtChar* GetTitle() {return m_pTitle;}
	pgDictionary<grcTexture>* GetTextureDictionary() {return m_pTxd;}

	// inline object list 
	static s32 GetInlineObjectCount() {return sm_inlineObjs.GetCount();}
	static CHtmlNode* GetInlineObject(s32 i) {return sm_inlineObjs[i];}
	static void AddInlineObject(CHtmlNode* pNode);
	static void ResetInlineObjectList();

	static void EnableParseScriptObjects(bool bProcess) {sm_parseScriptObjects = bProcess;}
	static void SetTextureLoadingFn(LoadTextureFn fn) {sm_loadTextureFn = fn;}

	grcTexture* LoadTexture(const char* pName) {return sm_loadTextureFn(pName);}

	static CHtmlDocument* GetCurrentDocument() {return sm_pCurrentDocument;}
	static void PushHtmlFolder(const char* pFilename);
	static atArray<s32>& GetTxdIndexArray() {return sm_txdIndicesUsed;}

#ifndef HTML_RESOURCE
	static CTextLayout sm_HtmlTextLayout;
#endif

#if __DEV
	void DebugPrint();
#endif
	// link access
	s32 GetNumLinks() {return m_links.GetCount();}
	CHtmlElementNode* GetLink(s32 link) {return m_links[link];}
	void GetLinkPosition(s32 link, float& x, float& y);
	const char* GetLinkRef(s32 link);
	s32 GetLinkAtPosition(float x, float y);

	void SetActive(CHtmlNode* pNode, bool active);
	void ParseScriptObject(CHtmlElementNode* pNode);


private:
	void SetupDefaultStyleSheet();
	void ApplyDefaultStyleSheet(CHtmlElementNode* pNode);
	void ApplyStyleSheet(CHtmlElementNode* pNode, const char* pClassName);
	void ApplyPseudoClassStyleSheet(CHtmlElementNode* pNode, const char* pClassName, const char* pPseudoClassName);

	void ReadBeginElement(parElement& elt, bool leaf);
	void ReadEndElement(bool leaf);
	void ReadData(char* data, int , bool );

	static void ResetElementArray();

	// extract data from nodes
	void ProcessNode(CHtmlNode* pNode);
	void ProcessElement(CHtmlElementNode* pNode);
	void ProcessData(CHtmlDataNode* pNode);
	void PostProcessNode(CHtmlNode* pNode);
	void PostProcessElement(CHtmlElementNode* pNode);

	// Process special nodes
	void ProcessBody(CHtmlElementNode* pNode);
	void ProcessLink(CHtmlElementNode* pNode);
	void ProcessFileLink(CHtmlElementNode* pNode);
	void ProcessMetaData(CHtmlElementNode* pNode);
	void ProcessImage(CHtmlElementNode* pNode);
	void ProcessEmbed(CHtmlElementNode* pNode);
	void ProcessObject(CHtmlElementNode* pNode);
	void ProcessScriptObject(CHtmlElementNode* pNode);
	void ProcessText(CHtmlElementNode* pNode);
	void PostProcessLink(CHtmlElementNode* pNode);
	void PostProcessTable(CHtmlElementNode* pNode);

	// position nodes
	void FormatNode(CHtmlNode* pNode, Vector2& posn);
	void FormatElement(CHtmlElementNode* pNode, Vector2&);
	void FormatData(CHtmlDataNode* pNode, Vector2&);
	void FormatTable(CHtmlElementNode* pNode, Vector2&);
	void FormatTableEntries(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pNode);
	void FormatInlineObjectList(Vector2& posn);

	// get size info for nodes, that is forced (so far only by an image)
	void GetForcedNodeDimensions(CHtmlNode* pNode, Vector2& size);
	void GetMinimumNodeDimensions(CHtmlNode* pNode, Vector2& size);
	// Get min max sizes for table dimension calculations
	void GetNodeMinMaxWidth(CHtmlNode* pNode, float& minWidth, float& maxWidth);
	bool GetElementMinMaxWidth(CHtmlElementNode* pNode, float& minWidth, float& maxWidth);
	void GetDataMinMaxWidth(CHtmlDataNode* pNode, float& minWidth, float& maxWidth);

	// calculate height info for nodes
	float CalculateNodeHeight(CHtmlNode* pNode);
	float CalculateElementHeight(CHtmlElementNode* pNode);
	float CalculateDataHeight(CHtmlDataNode* pNode);

	void UpdatePosition(CHtmlNode* pNode, Vector2& posn, const Vector2& newPosn);

	void AlignNode(CHtmlElementNode* pNode);
	void VerticalAlignNode(CHtmlElementNode* pNode);
	void OffsetPositions(CHtmlNode* pNode, const Vector2& posn);

	// store node sizes
	void StoreNodeSize(CHtmlNode* pNode);
	void StoreElementSize(CHtmlNode* pNode);
	
	void ProcessAttributes(CHtmlElementNode* pNode);
	void ParseStyle(const char* pStyle, HtmlRenderState& renderState);

	// table stuff
	void GetTableNodes(CHtmlNode* pNode, CHtmlElementNode** ppNodes, s32& index);
	void CountTableRowsAndColumns(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode);
	void CheckTableRowsAndColumns(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode);
	void GetTableCellCoords(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode);
	void GetTableWidthsAndHeights(CHtmlElementNode** pNodes, s32 num, CHtmlTableNode* pTableNode);
	void CalculateTableWidths(CHtmlElementNode* pTableNode);
	void FilloutTableColumnWidths(CHtmlElementNode** pNodes, s32 numNodes, CHtmlTableNode* pTableNode);

	grcTexture* LoadTextureInternal(const char* pName);

	CHtmlElementNode* m_pRootNode;
	CHtmlElementNode* m_pBodyNode;
	datRef<GxtChar> m_pTitle;
	datOwner<pgDictionary<grcTexture> > m_pTxd;
	atArray<datRef<int> > m_padArray;
	atArray<datRef<CHtmlElementNode> > m_links;
	CCssStyleSheet m_styleSheet;

	// temporaries used in html document loading and formatting
	static CHtmlNode* sm_pCurrent;
	static atArray<parElement*> sm_elements;
	static bool sm_parseScriptObjects;
	static bool sm_loadTextures;
	static bool sm_inBody;
	static CHtmlDocument* sm_pCurrentDocument;
	static LoadTextureFn sm_loadTextureFn;
	static atArray<int> sm_txdIndicesUsed;
	// temporaries used in formatting
	static atArray<CHtmlNode* > sm_inlineObjs;
};

// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLDOCUMENT_H_
