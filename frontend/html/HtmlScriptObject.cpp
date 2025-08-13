//
// filename:	HtmlScriptObject.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "HtmlScriptObject.h"

// --- Defines ------------------------------------------------------------------

#define HTML_SCRIPT_OBJECT_POOL_SIZE	16
// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

CHtmlScriptObjectMgr gHtmlScriptObjectMgr;

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

//
// name:		CHtmlScriptObjectMgr::CHtmlScriptObjectMgr
// description:	Constructor, set initial size of array
CHtmlScriptObjectMgr::CHtmlScriptObjectMgr() : m_htmlScriptPool(HTML_SCRIPT_OBJECT_POOL_SIZE, "HtmlScriptObjects")
{
}

//
// name:		CHtmlScriptObjectMgr::Create
// description:	Create and initialise an html script object
s32 CHtmlScriptObjectMgr::Create(const char* pName)
{
	HtmlScriptObject* pObj = m_htmlScriptPool.New();

	// initialise object
	strncpy(pObj->m_name, pName, HTML_SCRIPTOBJECT_NAME_SIZE);
	pObj->m_posn = 0;
	pObj->m_text[0] = '\0';
	pObj->m_bFinished = false;

	s32 index = m_htmlScriptPool.GetJustIndex(pObj);
	AddHtml(index, "<span>");
	return index;
}

//
// name:		CHtmlScriptObjectMgr::Delete
// description:	Delete html script object
void CHtmlScriptObjectMgr::Delete(s32 index)
{
	Assertf(m_htmlScriptPool.GetSlot(index) != NULL, "Delete: this is not a valid slot id");
	m_htmlScriptPool.Delete(m_htmlScriptPool.GetSlot(index));
}

//
// name:		CHtmlScriptObjectMgr::Delete
// description:	Delete html script object
void CHtmlScriptObjectMgr::DeleteAll()
{
	s32 size = m_htmlScriptPool.GetSize();
	while(size--)
	{
		HtmlScriptObject* pItem = m_htmlScriptPool.GetSlot(size);
		if(pItem)
		{
			m_htmlScriptPool.Delete(pItem);
		}
	}
}

//
// name:		CHtmlScriptObjectMgr::GetHtml
// description:	Return pointer to HTML
const char* CHtmlScriptObjectMgr::GetHtml(s32 index)
{
	HtmlScriptObject* pObj = m_htmlScriptPool.GetSlot(index);
	Assertf(pObj != NULL, "GetHtml: this is not a valid slot id");

	FinishHtml(index);
	return pObj->m_text;
}

//
// name:		CHtmlScriptObjectMgr::GetHtml
// description:	Return size of HTML data
s32 CHtmlScriptObjectMgr::GetHtmlSize(s32 index)
{
	HtmlScriptObject* pObj = m_htmlScriptPool.GetSlot(index);
	Assertf(pObj != NULL, "GetHtmlSize: this is not a valid slot id");
	FinishHtml(index);
	return pObj->m_posn;
}

void CHtmlScriptObjectMgr::FinishHtml(s32 index)
{
	HtmlScriptObject* pObj = m_htmlScriptPool.GetSlot(index);

	// if html isn't finished then finish it. ie close span tag opened when it was created
	if(pObj->m_bFinished == false)
	{
		AddHtml(index, "</span>");
		pObj->m_bFinished = true;
	}
}

//
// name:		CHtmlScriptObjectMgr::Get
// description:	Return the html script object with the given name
s32 CHtmlScriptObjectMgr::Get(const char* pName)
{
	s32 i = m_htmlScriptPool.GetSize();

	while(i--)
	{
		HtmlScriptObject* pObj = m_htmlScriptPool.GetSlot(i);
		if(pObj && !strncmp(pObj->m_name, pName, 32))
			return i;
	}
	Assertf(0, "%s:Couldn't find the Html script object", pName);
	return 0;
}

//
// name:		CHtmlScriptObjectMgr::AddHtml
// description:	Add html to html script object
void CHtmlScriptObjectMgr::AddHtml(s32 index, const char* pHtml)
{
	HtmlScriptObject* pObj = m_htmlScriptPool.GetSlot(index);
	s32 len = strlen(pHtml);

	Assertf(pObj != NULL, "AddHtml: this is not a valid slot id");
	Assertf(pObj->m_posn + len < HTML_BUFFER_SIZE-1, "%s:There is too much data in this Html script object", pObj->m_name);
	Assertf(pObj->m_bFinished == false, "%s:Cannot add html to script object that has already been displayed", pObj->m_name);

	strcpy(pObj->m_text + pObj->m_posn, pHtml);
	pObj->m_posn += len;
}

