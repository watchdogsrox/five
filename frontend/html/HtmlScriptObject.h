//
// filename:	HtmlScriptObject.h
// description:	
//

#ifndef INC_HTMLSCRIPTOBJECT_H_
#define INC_HTMLSCRIPTOBJECT_H_

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
// Game headers
#include "fwtl/pool.h"

// --- Defines ------------------------------------------------------------------

#define HTML_SCRIPTOBJECT_NAME_SIZE		32
#define HTML_BUFFER_SIZE	1300

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

struct HtmlScriptObject
{
	char m_name[HTML_SCRIPTOBJECT_NAME_SIZE];
	char m_text[HTML_BUFFER_SIZE];
	s32 m_posn;
	bool m_bFinished;
};

class CHtmlScriptObjectMgr
{
public:
	CHtmlScriptObjectMgr();

	s32 Create(const char* pName);
	void Delete(s32 index);
	void DeleteAll();
	s32 Get(const char* pName);
	const char* GetHtml(s32 index);
	s32 GetHtmlSize(s32 index);

	void AddHtml(s32 index, const char* pHtml);

private:
	void FinishHtml(s32 index);
	fwPool<HtmlScriptObject> m_htmlScriptPool;
};

// --- Globals ------------------------------------------------------------------

extern CHtmlScriptObjectMgr gHtmlScriptObjectMgr;

#endif // !INC_HTMLSCRIPTOBJECT_H_
