//
// filename:	HtmlParser.cpp
// description:	
//

// --- Include Files ------------------------------------------------------------

// C headers
// Rage headers
#include "atl/string.h"
#include "parsercore/element.h"
// Game headers
#include "htmlparser.h"

// --- Defines ------------------------------------------------------------------

#define DEBUG_HTML_PARSER	1

// --- Constants ----------------------------------------------------------------

// --- Structure/Class Definitions ----------------------------------------------

// --- Globals ------------------------------------------------------------------

// --- Static Globals -----------------------------------------------------------

// --- Static class members -----------------------------------------------------

// --- Code ---------------------------------------------------------------------

void CHtmlParser::ReadWithCallbacks()
{
	Assert(m_Stream);
	char wsBuffer[1024];
	int wsBufLen = 0;
#if DEBUG_HTML_PARSER
	atArray<atString> elementNameStack;
#endif

	// Check that this isn't a UTF-16 file
	int firstChar;
	firstChar = ReadRawCharacter();
	Assertf(firstChar != 0xFF && firstChar != 0xFE, "Error: Can't read UTF-16 files. Reencode as UTF-8");
	if (firstChar == 0xEF) 
	{
		// could be a UTF-8 byte order mark (0xEFBBBF = 0xFEFF in UTF-16). If not, assert 
		// (since whatever character it is, the first character's not whitespace and its not a <)
		Match("\xBB\xBF");
	}
	else
	{
		PushBack((char)firstChar);
	}

	// Should this be alloca'ed instead?
	//	char* dataBuf = new char[m_MaxDataSize];
	char* dataBuf = (char*)alloca(sizeof(char) * m_DataChunkSize);
	int depth = 0;

	bool eof = false;
	while(!eof) {
		wsBufLen = SkipWhitespaceAndComments(wsBuffer, 1024);
		int c = ReadRawCharacter();
		if (c == -1) {
			break;
		}
		else if (c == '<') {
			wsBufLen = 0; // clear the whitespace buffer
			int nextC = ReadRawCharacter();
			if (nextC == '/') {
#if DEBUG_HTML_PARSER
				char elementName[MAX_ELEMENT_NAME_LENGTH];
				Match("sws>", elementName);
#if __ASSERT
				Assertf(!stricmp(elementName, elementNameStack.Top()), "%s Element wasn't closed", (const char*)elementNameStack.Top());
#else
				if(stricmp(elementName, elementNameStack.Top()))
					Errorf("%s Element wasn't closed", (const char*)elementNameStack.Top());
#endif
				elementNameStack.Pop();
#else
				Match("sws>"); // not skipping whitespace here because we don't want to consume past eof, other than that
				// it would be good because data can't follow an end tag, and not filling the wsBuffer is more efficient.
#endif
				--depth;
				if (depth == 0)
				{
					eof = true;
				}
				m_EndElementCB(false);
				m_JustBeganElement = false;
				m_JustReadData = false;
			}
			else {
				//Assert(!m_JustReadData);
				PushBack((char)nextC);
				PushBack((char)c);
				parElement elem;
				bool leaf = ReadElementInternal(elem);
				if (leaf)
				{
					SkipWhitespaceAndComments(); // skipping whitespace here because data can't follow an end tag, and not filling the wsBuffer is a little more efficient.
					m_BeginElementCB(elem, true);
					m_EndElementCB(true);
					m_JustBeganElement = false;
				}
				else
				{
					++depth;
					m_BeginElementCB(elem, false);

					m_JustBeganElement = true;
#if DEBUG_HTML_PARSER
					elementNameStack.PushAndGrow(elem.GetName());
#endif
				}
				m_JustReadData = false;
			}
		}
		else
		{
			//Assert(m_JustBeganElement && "XML file format error");
			PushBack((char)c);
			PushBack(wsBuffer, wsBufLen-1); // send len-1 because len includes the terminating 0
			wsBufLen = 0;
			m_DataIncomplete = true;
			m_JustBeganElement = false;
			while(m_DataIncomplete)
			{
				int len = ReadDataInternal(dataBuf, m_DataChunkSize, m_LastEncoding);
				m_DataCB(dataBuf, len, m_DataIncomplete);
			}
			m_JustReadData = true;
		}
	}
}

bool CHtmlParser::ReadElementInternal(parElement& outElement)
{
	outElement.Reset();

	if (Peek("</", false)) 
		return true;

	Match("S<");
	char elemName[MAX_ELEMENT_NAME_LENGTH];
	ReadToken(elemName);
	outElement.SetName(elemName);
	SkipWhitespace();

	m_Base64Buffer = 0;
	m_Base64BufferSize = 0;
	m_LastEncoding = UNSPECIFIED;

	// read attribute list;
	while (!Peek('>', false) && !Peek("/>", false)) {
		char attrName[MAX_ELEMENT_NAME_LENGTH];
		ReadToken(attrName);

		Match("s=s");

		char attrValue[MAX_ATTRIBUTE_VALUE_LENGTH];
		ReadAttributeValue(attrValue);

		if (!strcmp(attrName, "content")) {
			m_LastEncoding = FindEncoding(attrValue);
		}

		strlwr(attrName);
		outElement.AddAttribute(attrName, attrValue);

		SkipWhitespace();
	}

	if (!m_NeverSortAttrList && outElement.GetAttributes().GetCount() > 4) {
		outElement.SortAttributes();
	}

	if (Peek('>')) {
		return false;
	}
	else if (Peek("/>")) {
		return true;
	}
	else {
		Assertf(0, "Error: Tag isn't closed properly");
	}
	return false;
}

s32 CHtmlParser::ReadDataInternal(char* str, int size, DataEncoding enc)
{
	/* Different rules for different encodings.

	1) ASCII
	Assert on any markup characters (<, &). These must be escaped.
	2) UTF-16
	Assert on any markup characters, convert the UTF-8 data into UTF-16
	3) Binary
	Assert on any invalid base64 character, convert from base64 to binary
	4) Raw XML
	The tricky one - Read all data up to the _matching_ end tag.
	Stream needs to support multicharacter push-back for this, so we can hit the end tag and back up.
	5) Int array
	Assert on any non-integer character
	6) Float array
	Assert on any non-float character
	7) Vector array
	Assert on any non-float character
	*/
	int c;

	SkipWhitespace();
	// The functions here set this to false if the data read was completed.
	m_DataIncomplete = true;

	switch (enc) {
		case UNSPECIFIED:
		case ASCII:
			{
				s32 len = 0;
				c = ReadEscapedCharacter();
				while(c != -1 && c != '<' && len < size-1) {
					Assert(c != '&');
					// don't store any control characters
					if(c < ' ')
					{
						c = ' ';
					}
					str[len] = (char)c;
					len++;
					c = ReadEscapedCharacter();
				}
				str[len] = '\0';
				if (c == '<') {
					// Normally it's bogus to call PushBack on an escaped character, but in this case
					// we know that we're talking about a character that started as "<" not "&quot;"
					// so pushing and poping it won't change its value.
					PushBack((char)c);
					m_DataIncomplete = false;
				}
				else if (len == size-1) {
					m_DataIncomplete = true;
				}
				return len;
			}
		default:
			Assert(0);
			break;
	}
	return -1;
}

//
// name:		CHtmlParser::ReadEscapedCharacter
// description:	
s32 CHtmlParser::ReadEscapedCharacter()
{
	int ch = ReadRawCharacter();
	if (ch == '&') {
		char buf[8];
		s32 stored = 0;
		while ((ch = ReadRawCharacter()) >= 'a' && ch <= 'z')
		{
			if (stored < (int)sizeof(buf)-1)
			{
				buf[stored++] = (char)ch;
			}
		}
		buf[stored] = 0;

		// add 256 to metacharacters so we can tell them apart from real ones
		// BUT casting the result to a (char) will "just work"
		if (!strcmp(buf,"lt")) 
		{
			return ESC_LT;
		}
		else if (!strcmp(buf,"gt")) 
		{
			return ESC_GT;
		}
		else if (!strcmp(buf,"apos")) 
		{
			return ESC_APOS;
		}
		else if (!strcmp(buf,"quot")) 
		{
			return ESC_QUOT;
		}
		else if (!strcmp(buf,"amp")) 
		{
			return ESC_AMP;
		}
		else if (!strcmp(buf,"nbsp")) 
		{
			return ESC_SPACE;
		}
		else if (!strcmp(buf,"rsquo")) 
		{
			return ESC_RSQUO;
		}
		else if (!strcmp(buf,"copy")) 
		{
			return ESC_COPYRIGHT;
		}
		else 
		{
			return ESC_UNKNOWN;
		}
	}
	else {
		return ch;
	}
}

