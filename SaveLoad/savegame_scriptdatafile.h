// 
// datadicts.h
// 
// Copyright (C) 1999-2011 Rockstar Games.  All Rights Reserved. 
// 

#ifndef SAVEGAME_DATADICT_H
#define SAVEGAME_DATADICT_H

#include <memory>

#include "atl/array.h"
#include "atl/map.h"
#include "atl/hashstring.h"
#include "atl/string.h"
#include "data/base.h"
#include "parser/macros.h"
#include "vectormath/vec3v.h"

namespace rage
{
	class RsonReader;
}

//////////////////////////////////////////////////////////////////////////
class sveNode : public rage::datBase
{
public:
	virtual ~sveNode() {}

	// Needs to stay in sync with sm_TypeNames in the .cpp 
	enum Type {
		SVE_NONE,
		SVE_BOOL,
		SVE_INT,
		SVE_FLOAT,
		SVE_STRING,
		SVE_VEC3,
		SVE_DICT,
		SVE_ARRAY,
	};

	virtual Type GetType() const { return SVE_NONE; }

	static const char* GetTypeName(Type t) {return sm_TypeNames[(int)t];}

	template<typename _Type>
	_Type* Downcast() {
		const Type destType = _Type::GetStaticType();
		if (!Verifyf(this->GetType() == destType, "Found a node of type %s but expected %s", sveNode::GetTypeName(this->GetType()), sveNode::GetTypeName(destType)))
		{
			return NULL;
		}
		return smart_cast<_Type*>(this);
	}

	static Type GetStaticType() { return SVE_NONE; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const = 0;

	static sveNode* ReadJsonValue(RsonReader& reader);

protected:
	static const char* const sm_TypeNames[];

	sveNode() {}; // Protected because sveNode is basically abstract
	PAR_PARSABLE;

};

//////////////////////////////////////////////////////////////////////////
class sveBool : public sveNode
{
public:
	sveBool() : m_Value(false) {}
	explicit sveBool(bool val) : m_Value(val) {}

	virtual ~sveBool() {}

	bool GetValue() const { return m_Value; }
	void SetValue(bool val) { m_Value = val; }

	virtual Type GetType() const { return SVE_BOOL; }
	static Type GetStaticType() { return SVE_BOOL; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const;

	static sveBool* ReadJsonBoolValue(RsonReader& reader);

protected:
	bool			m_Value;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
class sveInt : public sveNode
{
public:
	sveInt() : m_Value(0) {}
	explicit sveInt(int val) : m_Value(val) {}

	virtual ~sveInt() {}

	int GetValue() const { return m_Value; }
	void SetValue(int val) { m_Value = val; }

	virtual Type GetType() const { return SVE_INT; }
	static Type GetStaticType() { return SVE_INT; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const;

	static sveInt* ReadJsonIntValue(RsonReader& reader);

protected:
	int			m_Value;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
class sveFloat : public sveNode
{
public:
	sveFloat() : m_Value(0.0f) {}
	explicit sveFloat(float val) : m_Value(val) {}

	virtual ~sveFloat() {}

	float GetValue() const { return m_Value; }
	void SetValue(float val) { m_Value = val; }

	virtual Type GetType() const { return SVE_FLOAT; }
	static Type GetStaticType() { return SVE_FLOAT; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const;

	static sveFloat* ReadJsonFloatValue(RsonReader& reader);

protected:
	float		m_Value;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
class sveString : public sveNode
{
public:
	sveString() {}
	explicit sveString(const char* val) : m_Value(val) {}

	virtual ~sveString() {}

	const char* GetValue() const { return m_Value.c_str(); }
	void SetValue(const char* val) { m_Value = val; }

	virtual Type GetType() const { return SVE_STRING; }
	static Type GetStaticType() { return SVE_STRING; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const;

	static sveString* ReadJsonStringValue(RsonReader& reader);

protected:
	rage::atString	m_Value;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
class sveVec3 : public sveNode
{
public:
	sveVec3() : m_Value(rage::V_ZERO) {}
	explicit sveVec3(rage::Vec3V_In val) : m_Value(val) {}

	virtual ~sveVec3() {}

	rage::Vec3V_Out GetValue() const { return m_Value; }
	void SetValue(rage::Vec3V_In val) { m_Value = val; }

	virtual Type GetType() const { return SVE_VEC3; }
	static Type GetStaticType() { return SVE_VEC3; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const;

protected:
	rage::Vec3V	m_Value;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
class sveDict : public sveNode
{
public:
	sveDict() {}

	virtual ~sveDict();

	sveNode* operator[](rage::atFinalHashString h) { sveNode** ret = m_Values.Access(h); return ret ? *ret : NULL; }

	void Insert(rage::atFinalHashString name, sveNode* node) { Assert(name.GetCStr()); Assert(node); m_Values.Insert(name, node); }
	void Insert(rage::atFinalHashString name, bool val);
	void Insert(rage::atFinalHashString name, int val);
	void Insert(rage::atFinalHashString name, float val);
	void Insert(rage::atFinalHashString name, rage::Vec3V_In val);
	void Insert(rage::atFinalHashString name, const char* val);

	void Remove(rage::atFinalHashString name);

	virtual Type GetType() const { return SVE_DICT; }
	static Type GetStaticType() { return SVE_DICT; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const;

	static sveDict* ReadJsonDictValue(RsonReader& reader);

	int GetCount() const { return m_Values.GetNumUsed(); }
protected:
	typedef rage::atMap<rage::atFinalHashString, sveNode*> ValueMap;

	ValueMap m_Values;
	PAR_PARSABLE;
};

//////////////////////////////////////////////////////////////////////////
class sCloudFile : public sveDict 
{
public:
	static std::unique_ptr<sCloudFile> LoadFile(const void* pData, unsigned nDataSize, const char* BANK_ONLY(szName));
	static std::unique_ptr<sCloudFile> ReadJsonFileValue(RsonReader& reader);
	static void OutputCloudFile(const void* pData, unsigned nDataSize, const char* szName);
};

//////////////////////////////////////////////////////////////////////////
class sveArray : public sveNode
{
public:
	sveArray() {}

	virtual ~sveArray();

	sveNode*& operator[](int i) { return m_Values[i]; }

	int GetCount() const { return m_Values.GetCount(); }
	void Append(sveNode* node) { m_Values.PushAndGrow(node); }
	void Append(bool val);
	void Append(int val);
	void Append(float val);
	void Append(rage::Vec3V_In val);
	void Append(const char* val);

	void Remove(int index);

	virtual Type GetType() const { return SVE_ARRAY; }
	static Type GetStaticType() { return SVE_ARRAY; }

	virtual void WriteJson(fiStream& out, int indent, bool prettyPrint) const;

	static sveArray* ReadJsonArrayValue(RsonReader& reader);

protected:
	rage::atArray<sveNode*>	m_Values;
	PAR_PARSABLE;
};

#endif // SAVEGAME_DATADICT_H
