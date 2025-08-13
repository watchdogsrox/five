//
// filename:	HtmlNode.h
// description:	
//

#ifndef INC_HTMLNODE_H_
#define INC_HTMLNODE_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/array.h"
#include "atl/string.h"
#include "data/struct.h"
// Game headers
#include "HtmlRenderState.h"
#ifndef HTML_RESOURCE
#include "text/text.h"
#endif //!HTML_RESOURCE

// --- Defines ------------------------------------------------------------------

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

#ifdef HTML_RESOURCE
typedef u8 GxtChar;
#endif

namespace rage {
	class parElement;
};

//
// name:		CHtmlNode
// description:	Base class for an HTML node
class CHtmlNode
{
public:
	enum eNodeType
	{
		ELEMENT_NODE,
		DATA_NODE,
		TABLE_NODE,
		TABLEENTRY_NODE,
		//		FLASH_NODE,
		INVALID_NODE
	};

	CHtmlNode();
	CHtmlNode( datResource& rsc ) : m_pParent(rsc), m_children(rsc, true), m_renderState(rsc)	{}

	DECLARE_PLACE(CHtmlNode);

	virtual ~CHtmlNode();

#if __DECLARESTRUCT
	virtual void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	void AddChild(CHtmlNode* pNode);
	s32 GetNumChildren() {return m_children.GetCount();}
	CHtmlNode* GetChild(s32 i) {return m_children[i];}
	CHtmlNode* GetParent() {return m_pParent;}
	CHtmlNode* GetNonInlineNode();

	HtmlRenderState& GetRenderState() {return m_renderState;}
	void GetDimensions(float& x, float& y, float& width, float& height);

#if __DEV
	virtual void DebugPrint();
	static s32 sm_debugPrintTab;
#endif
protected:
	s32 m_type;

private:
	datRef<CHtmlNode> m_pParent;
	atArray<datOwner<CHtmlNode> > m_children;
	HtmlRenderState m_renderState;
};

//
// name:		CHtmlElementNode
// description:	Class for an HTML node containing an HTML element
class CHtmlElementNode : public CHtmlNode
{
	friend class CHtmlDocument;
	friend class CHtmlRenderer;

public:
	CHtmlElementNode(parElement* pElement);
	CHtmlElementNode(datResource& rsc);

	//	IMPLEMENT_PLACE_INLINE(CHtmlElementNode);

#if __DECLARESTRUCT
	virtual void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	const parElement& GetElement() const {Assert(m_pElement); return *m_pElement;}
	parElement& GetElement() {Assert(m_pElement); return *m_pElement;}
	void ResetParserElement() {Assert(m_pElement); m_pElement = NULL;}
	s32 GetElementId() {return m_elementId;}
	const char* GetAttributeData() {return m_data;}

	// special case functions for returning image and txd names
	const char* GetTextureName();
	void GetTxdName(char* pBuffer, int bufferLen);
	grcTexture* GetTexture(class CHtmlDocument* pDocument);

#if __DEV
	virtual void DebugPrint();
#endif

private:
	s32 m_elementId;
	parElement* m_pElement;
	atString m_data;
};

//
// name:		CHtmlDataNode
// description:	Class for an HTML node containing ASCII data
class CHtmlDataNode : public CHtmlNode
{
public:
	CHtmlDataNode(const char* pData, s32 size);
	CHtmlDataNode(datResource& rsc);
	~CHtmlDataNode();

	//	IMPLEMENT_PLACE_INLINE(CHtmlDataNode);

#if __DECLARESTRUCT
	virtual void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	void SetData(const char* pData, s32 size);
	GxtChar* GetData() {return m_pData;}

#if __DEV
	virtual void DebugPrint();
#endif

private:
	GxtChar* m_pData;
};

class CHtmlTableNode : public CHtmlElementNode
{
public:
	CHtmlTableNode(parElement* pElement);
	CHtmlTableNode(datResource& rsc);

	//	IMPLEMENT_PLACE_INLINE(CHtmlTableNode);

	~CHtmlTableNode();

#if __DECLARESTRUCT
	virtual void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

	void AllocateArrays();
	void DeleteArrays();
	void ResetCalculatedWidthAndHeights();
	void CalculateColumnWidths();

	float GetColumnWidth(s32 i) {Assert(i<m_numColumns); return m_pCalculatedWidths[i];}
	float GetRowHeight(s32 i) {Assert(i<m_numRows); return m_pCalculatedHeights[i];}
	float GetMinWidth();
	float GetMaxWidth();

	void SetNumColumns(s32 columns) {m_numColumns = columns;}
	void SetNumRows(s32 rows) {m_numRows = rows;}
	s32 GetNumColumns() {return m_numColumns;}
	s32 GetNumRows() {return m_numRows;}
	void SetColumnMinWidth(s32 i, float width) {Assert(i<m_numColumns); m_pMinWidths[i] = width;}
	void SetColumnMaxWidth(s32 i, float width) {Assert(i<m_numColumns); m_pMaxWidths[i] = width;}
	void SetRowHeight(s32 i, float height) {Assert(i<m_numRows); m_pHeights[i] = height;}
	void SetCalculatedRowHeight(s32 i, float height) {Assert(i<m_numRows); m_pCalculatedHeights[i] = height;}
	float GetColumnMinWidth(s32 i) {Assert(i<m_numColumns); return m_pMinWidths[i];}
	float GetColumnMaxWidth(s32 i) {Assert(i<m_numColumns); return m_pMaxWidths[i];}

private:
	float *m_pHeights;
	float *m_pMinWidths;
	float *m_pMaxWidths;
	float *m_pCalculatedWidths;
	float *m_pCalculatedHeights;
	s32 m_numColumns;
	s32 m_numRows;
};

class CHtmlTableElementNode : public CHtmlElementNode
{
	friend class CHtmlDocument;
public:
	CHtmlTableElementNode(parElement* pElement);
	CHtmlTableElementNode(datResource& rsc) : CHtmlElementNode(rsc) {}

	//	IMPLEMENT_PLACE_INLINE(CHtmlTableElementNode);

#if __DECLARESTRUCT
	virtual void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

private:
	s32 m_row;
	s32 m_column;
};

/*class CHtmlFlashNode : public CHtmlElementNode
{
friend class CHtmlDocument;
public:
CHtmlFlashNode(parElement* pElement);
CHtmlFlashNode(datResource& rsc) : CHtmlElementNode(rsc), m_pFile(rsc) {}
~CHtmlFlashNode();

//	IMPLEMENT_PLACE_INLINE(CHtmlFlashNode);

#if !__FINAL
virtual void DeclareStruct(datTypeStruct &s);
#endif // !__FINAL

void Load();
swfFILE* GetFile() {return m_pFile;}
swfCONTEXT* GetContext() {return m_pContext;}

private:
static void FSHandler(const char* command, const swfVARIANT& param);

datOwner<swfFILE> m_pFile;
swfCONTEXT* m_pContext;
};*/


// --- Globals ------------------------------------------------------------------

#endif // !INC_HTMLNODE_H_
