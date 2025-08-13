// 
// sample_parser/datadicts.cpp 
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#include "savegame_scriptdatafile.h"
#include "savegame_scriptdatafile_parser.h"

// Rage headers
#include "data/rson.h"
#include "diag/output.h"
#include "file/asset.h"
#include "fwnet/netchannel.h"
#include "parsercore/streamxml.h"
#include "system/nelem.h"
#include "system/param.h"

SAVEGAME_OPTIMISATIONS()
NETWORK_OPTIMISATIONS()

#if __BANK
	PARAM(outputCloudFiles, "Write cloud files to PC harddisk for debugging");
#endif

static void PutNewline(fiStream& out, bool prettyPrint)
{
	if (prettyPrint)
	{
		out.FastPutCh('\n');
	}
}

static void PutIndent(fiStream& out, bool prettyPrint, int indent)
{
	if (!prettyPrint)
		return;

	for (int i = 0; i < indent; i++)
		out.FastPutCh('\t');
}

const char* const sveNode::sm_TypeNames[] = {
	"none",
	"bool",
	"int",
	"float",
	"string",
	"vec3",
	"dict",
	"array",
};

atFinalHashString finalHashStringOfX("x");
atFinalHashString finalHashStringOfY("y");
atFinalHashString finalHashStringOfZ("z");

sveNode* sveNode::ReadJsonValue(RsonReader& reader)
{
	// look at the first characters in the value to decide what kind of node to create

	char valueBuf[32];
	reader.GetRawValueString(valueBuf, NELEM(valueBuf));

	char firstChar = valueBuf[0];

	switch(firstChar)
	{
	case '\0':
		Errorf("No value found!");
		return rage_new sveString();
	case '\"':
		return sveString::ReadJsonStringValue(reader);
	case 't':
	case 'f':
		return sveBool::ReadJsonBoolValue(reader);
	case '{':
		{
			sveDict* dict = sveDict::ReadJsonDictValue(reader);
			// check to see if its really a vec3
			if (dict->GetCount() != 3)
			{
				return dict;
			}
			sveNode* x = (*dict)[finalHashStringOfX];
			if (!x || x->GetType() != SVE_FLOAT) 
			{ 
				return dict; 
			}
			sveNode* y = (*dict)[finalHashStringOfY];
			if (!y || y->GetType() != SVE_FLOAT) 
			{ 
				return dict; 
			}
			sveNode* z = (*dict)[finalHashStringOfZ];
			if (!z || z->GetType() != SVE_FLOAT) 
			{ 
				return dict; 
			}
			sveNode* vec = rage_new sveVec3(Vec3V(x->Downcast<sveFloat>()->GetValue(), y->Downcast<sveFloat>()->GetValue(), z->Downcast<sveFloat>()->GetValue()));
			delete dict;
			return vec;
		}
	case '[':
		return sveArray::ReadJsonArrayValue(reader);
	default:
		if (stricmp(valueBuf, "null") == 0)
		{	//	Graeme - I need this check when retrieving the contents of the photo directory from the cloud. If the directory is empty, the value is null
			return rage_new sveInt(0);
		}

		if (!Verifyf(firstChar == '-' || (firstChar >= '0' && firstChar <= '9'), "Unexpected start of value string: %.10s", valueBuf))
		{
			return rage_new sveInt(0);
		}
		// scan as an int
		// then check the next char, if it's a '.', 'E' or 'e', go back and scan as a float
		char* endInt = NULL;
		strtol(valueBuf, &endInt, 0);
		if (!Verifyf(endInt != NULL, "Couldn't convert valueBuf: %s", valueBuf))
		{
			Errorf("Couldn't convert valueBuf: %s", valueBuf);
			return rage_new sveInt(0);
		}

		if (*endInt == '.' || *endInt == 'E' || *endInt == 'e')
		{
			return sveFloat::ReadJsonFloatValue(reader);
		}
		else
		{
			return sveInt::ReadJsonIntValue(reader);
		}
	}
}

void sveBool::WriteJson( fiStream& out, int /*indent*/, bool /*prettyPrint */) const
{
	fprintf(&out, m_Value ? "true" : "false");
}

sveBool* sveBool::ReadJsonBoolValue(RsonReader& reader) 
{
	bool val;
	if (reader.AsBool(val))
	{
		return rage_new sveBool(val);
	}		   
	Errorf("Couldn't read bool value from JSON");
	return rage_new sveBool(false);
}

void sveInt::WriteJson( fiStream& out, int /*indent*/, bool /*prettyPrint*/ ) const
{
	fprintf(&out, "%d", m_Value);
}

sveInt* sveInt::ReadJsonIntValue(RsonReader& reader)
{
	int val;
	if (reader.AsInt(val))
	{
		return rage_new sveInt(val);
	}
	Errorf("Couldn't read int value from JSON");
	return rage_new sveInt(0);
}

void sveFloat::WriteJson( fiStream& out, int /*indent*/, bool /*prettyPrint*/ ) const
{
    if(Abs(m_Value) < SMALL_FLOAT)
    {
        fprintf(&out, "0.0");
    }
    else
    {
        fprintf(&out, "%.3f", m_Value);
    }
}

sveFloat* sveFloat::ReadJsonFloatValue(RsonReader& reader)
{
	float val;
	if (reader.AsFloat(val))
	{
		return rage_new sveFloat(val);
	}
	Errorf("Couldn't read float value from JSON");
	return rage_new sveFloat(0.0f);
}

void sveString::WriteJson( fiStream& out, int /*indent*/, bool /*prettyPrint*/ ) const
{
	out.FastPutCh('"');

	for(const char* c = m_Value; *c; ++c)
	{
		switch(*c)
		{
		case '"':
			out.FastPutCh('\\');
			out.FastPutCh('"');
			break;
		case '\\':
			out.FastPutCh('\\');
			out.FastPutCh('\\');
			break;
		case '\n':
			out.FastPutCh('\\');
			out.FastPutCh('n');
			break;
		case '\r':
			out.FastPutCh('\\');
			out.FastPutCh('r');
			break;
		case '\t':
			out.FastPutCh('\\');
			out.FastPutCh('t');
			break;
		case '\f':
			out.FastPutCh('\\');
			out.FastPutCh('f');
			break;
		default:
			out.FastPutCh(*c);
		}
	}

	out.FastPutCh('"');
}

sveString* sveString::ReadJsonStringValue(RsonReader& reader)
{
	int length = reader.GetValueLength();
	if (length)
	{
		char* buf = rage_new char[length+1];
		sveString* strNode = NULL;
		if (reader.GetValue(buf, length+1))
		{
			strNode = rage_new sveString(buf);
		}
		delete buf;
		return strNode;
	}
	else
	{
		return rage_new sveString();
	}
}

void sveVec3::WriteJson( fiStream& out, int /*indent*/, bool /*prettyPrint*/ ) const
{
    fprintf(&out, "{");

    if(Abs(m_Value.GetXf()) < SMALL_FLOAT)
    {
        fprintf(&out, "\"x\":0.0");
    }
    else
    {
        fprintf(&out, "\"x\":%.3f", m_Value.GetXf());
    }

    if(Abs(m_Value.GetYf()) < SMALL_FLOAT)
    {
        fprintf(&out, ",\"y\":0.0");
    }
    else
    {
        fprintf(&out, ",\"y\":%.3f", m_Value.GetYf());
    }

    if(Abs(m_Value.GetZf()) < SMALL_FLOAT)
    {
        fprintf(&out, ",\"z\":0.0");
    }
    else
    {
        fprintf(&out, ",\"z\":%.3f", m_Value.GetZf());
    }

    fprintf(&out, "}");
}

sveDict::~sveDict()
{
	for(ValueMap::Iterator iter = m_Values.CreateIterator(); !iter.AtEnd(); iter.Next())
	{
		delete (*iter);
	}
}

void sveDict::Insert( rage::atFinalHashString name, bool val )
{
	sveNode* newNode = rage_new sveBool(val);
	Insert(name, newNode);
}

void sveDict::Insert( rage::atFinalHashString name, int val )
{
	sveNode* newNode = rage_new sveInt(val);
	Insert(name, newNode);
}

void sveDict::Insert( rage::atFinalHashString name, float val )
{
	sveNode* newNode = rage_new sveFloat(val);
	Insert(name, newNode);
}

void sveDict::Insert( rage::atFinalHashString name, rage::Vec3V_In val )
{
	sveNode* newNode = rage_new sveVec3(val);
	Insert(name, newNode);
}

void sveDict::Insert( rage::atFinalHashString name, const char* val )
{
	sveNode* newNode = rage_new sveString(val);
	Insert(name, newNode);
}

void sveDict::Remove( rage::atFinalHashString name )
{
	sveNode** oldNode = m_Values.Access(name);
	if (oldNode)
	{
		delete *oldNode;
		m_Values.Delete(name);
	}
}

struct sveDictPair {
	const char* name;
	sveNode* data;
};

bool SveNodeSort(const sveDictPair& a, const sveDictPair& b)
{
	if (AssertVerify(a.name) && AssertVerify(b.name))
	{
		return strcmp(a.name, b.name) < 0;
	}

	return false;
}

void sveDict::WriteJson( fiStream& out, int indent, bool prettyPrint ) const
{
	int numKids = m_Values.GetNumUsed();

	if (numKids > 0)
	{
		if (indent > 0) 
			PutNewline(out, prettyPrint);

		PutIndent(out, prettyPrint, indent);
	}

	fprintf(&out, "{");
	indent++;

	if (numKids > 0)
		PutNewline(out, prettyPrint);

	sveDictPair* pairs = Alloca(sveDictPair, numKids);
	int currKid = 0;
	for(ValueMap::ConstIterator iter = m_Values.CreateIterator(); !iter.AtEnd(); iter.Next())
	{
		pairs[currKid].name = iter.GetKey().GetCStr();
		pairs[currKid].data = iter.GetData();
		currKid++;
	}

	std::sort(pairs, pairs + numKids, SveNodeSort);

	for(int i = 0; i < numKids; i++)
	{
		PutIndent(out, prettyPrint, indent);
		fprintf(&out, "\"%s\":", pairs[i].name);
		pairs[i].data->WriteJson(out, indent, prettyPrint);

		if (i != numKids-1)
		{
			// not the last element
			out.FastPutCh(',');
			PutNewline(out, prettyPrint);
		}
	}

	indent--;

	if (numKids > 0)
	{
		PutNewline(out, prettyPrint);
		PutIndent(out, prettyPrint, indent);
	}

	fprintf(&out, "}");
}

sveDict* sveDict::ReadJsonDictValue(RsonReader& reader)
{
	sveDict* newDict = rage_new sveDict;

	RsonReader child;

	if (!reader.GetFirstMember(&child))
	{
		return newDict;
	}

	do
	{
		char nameBuf[128];
		if (child.GetName(nameBuf, 128))
		{
			sveNode* childNode = sveNode::ReadJsonValue(child);
			newDict->Insert(atFinalHashString(nameBuf), childNode);
		}
	}
	while(child.GetNextSibling(&child));

	return newDict;
}

////////////////////////// sCloudFile /////////////////////////////////////////////

std::unique_ptr<sCloudFile> sCloudFile::LoadFile(const void* pData, unsigned nDataSize, const char* OUTPUT_ONLY(szName))
{
	// log out 
	DIAG_CONTEXT_MESSAGE("Reading JSON document from %s", szName);

#if __BANK
	if (PARAM_outputCloudFiles.Get())
		OutputCloudFile(pData, nDataSize, szName);
#endif

	// validate the JSON file
	if (!gnetVerifyf(RsonReader::ValidateJson(static_cast<const char*>(pData), nDataSize), "sCloudFile::LoadFile :: Error parsing %s. Include cloud_ValidateError.log in the logs.", szName))
	{
		OUTPUT_ONLY(OutputCloudFile(pData, nDataSize, "ValidateError");)
			gnetError("sCloudFile::LoadFile :: Error parsing %s. Include cloud_ValidateError.log in the logs.", szName);
		return nullptr;
	}

	RsonReader reader;
	if (!reader.Init(static_cast<const char*>(pData), 0, nDataSize))
	{
		gnetError("sCloudFile::LoadFile :: Failed to initialised RsonReader for %s.", szName);
		return nullptr;
	}

	// create our save file
	return sCloudFile::ReadJsonFileValue(reader);
}

std::unique_ptr<sCloudFile> sCloudFile::ReadJsonFileValue(RsonReader& reader)
{
	std::unique_ptr<sCloudFile> pFile(rage_new sCloudFile);

	RsonReader child;
	if (!reader.GetFirstMember(&child))
		return pFile;

	do
	{
		char nameBuf[128];
		if (child.GetName(nameBuf, 128))
		{
			sveNode* pChildNode = sveNode::ReadJsonValue(child);
			if (pChildNode)
				pFile->Insert(atFinalHashString(nameBuf), pChildNode);
			else
			{
				return nullptr;
			}
		}
	} while (child.GetNextSibling(&child));

	// finished, return our file
	return pFile;
}

void sCloudFile::OutputCloudFile(const void* pData, unsigned nDataSize, const char* szName)
{
	// build an output name
	static const unsigned kMaxName = 128;
	char szOutputName[kMaxName];
	snprintf(szOutputName, kMaxName, "cloud_%s.log", szName);

	// log to file
	fiStream* pStream(ASSET.Create(szOutputName, ""));
	if (pStream)
	{
		pStream->Write(pData, nDataSize);
		pStream->Close();
	}
}

////////////////////////// sveArray /////////////////////////////////////////////

sveArray::~sveArray()
{
	for(int i = 0; i < m_Values.GetCount(); i++)
	{
		delete m_Values[i];
	}
}

void sveArray::Append( bool val )
{
	sveNode* newNode = rage_new sveBool(val);
	Append(newNode);
}

void sveArray::Append( int val )
{
	sveNode* newNode = rage_new sveInt(val);
	Append(newNode);
}

void sveArray::Append( float val )
{
	sveNode* newNode = rage_new sveFloat(val);
	Append(newNode);
}

void sveArray::Append( rage::Vec3V_In val )
{
	sveNode* newNode = rage_new sveVec3(val);
	Append(newNode);
}

void sveArray::Append( const char* val )
{
	sveNode* newNode = rage_new sveString(val);
	Append(newNode);
}

void sveArray::Remove( int index )
{
	delete m_Values[index];
	m_Values.Delete(index);
}

void sveArray::WriteJson( fiStream& out, int indent, bool prettyPrint ) const
{
	int numKids = m_Values.GetCount();

	if (numKids == 0)
	{
		PutIndent(out, prettyPrint, indent);
		fprintf(&out, "[]");
		return;
	}

	PutNewline(out, prettyPrint);
	PutIndent(out, prettyPrint, indent);
	fprintf(&out, "[");
	PutNewline(out, prettyPrint);
	indent++;

	for(int i = 0; i < numKids; i++)
	{
		PutIndent(out, prettyPrint, indent);
		m_Values[i]->WriteJson(out, indent, prettyPrint);

		if (i != numKids-1)
		{
			// not the last element
			out.FastPutCh(',');
			PutNewline(out, prettyPrint);
		}
	}

	indent--;
	PutNewline(out, prettyPrint);
	PutIndent(out, prettyPrint, indent);
	fprintf(&out, "]");
}

sveArray* sveArray::ReadJsonArrayValue(RsonReader& reader)
{
	sveArray* newArr = rage_new sveArray;

	RsonReader child;

	if (!reader.GetFirstMember(&child))
	{
		return newArr;
	}

	do
	{
		sveNode* childNode = sveNode::ReadJsonValue(child);
		if(Verifyf(childNode, "Invalid child node"))
		{
			newArr->Append(childNode);
		}
	}
	while(child.GetNextSibling(&child));

	return newArr;
}