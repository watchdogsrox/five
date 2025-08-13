// 
// animation/debug/ClipTagMetadata.h 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#ifndef CLIP_TAG_METADATA_H
#define CLIP_TAG_METADATA_H

// rage includes

#include "atl/hashstring.h"
#include "atl/string.h"
#include "bank/bank.h"
#include "bank/bkmatrix.h"
#include "bank/bkvector.h"
#include "bank/slider.h"
#include "bank/text.h"
#include "bank/title.h"
#include "data/base.h"
#include "parser/macros.h"
#include "parser/manager.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "vector/quaternion.h"

// game includes

// forward declarations

///////////////////////////////////////////////////////////////////////////////

enum eClipTagAttributeType
{
	kTypeFloat,
	kTypeInt,
	kTypeBool,
	kTypeString,
	kTypeVector3,
	kTypeVector4,
	kTypeQuaternion,
	kTypeMatrix34,
	kTypeData,
	kTypeHashString,
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagAttribute : public datBase
{
public:

	/* Destructor */

	virtual ~ClipTagAttribute();

	/* Constructors */

	ClipTagAttribute();
	ClipTagAttribute(const char *name, eClipTagAttributeType type, bool editable);

	/* Properties */

	const char *GetName() const;
	eClipTagAttributeType GetType() const;
	bool GetEditable() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	atString m_Name;
	eClipTagAttributeType m_Type;
	bool m_Editable;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagFloatAttribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagFloatAttribute();

	/* Constructors */

	ClipTagFloatAttribute();
	ClipTagFloatAttribute(const char *name, float value, bool editable, float minValue, float maxValue, float deltaValue);

	/* Properties */

	void SetValue(float value);
	float GetValue() const;

	void SetMinValue(float minValue);
	float GetMinValue() const;
	void SetMaxValue(float maxValue);
	float GetMaxValue() const;
	void SetDeltaValue(float deltaValue);
	float GetDeltaValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	float m_Value;

	float m_MinValue;
	float m_MaxValue;
	float m_DeltaValue;
	bkSlider *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagIntAttribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagIntAttribute();

	/* Constructors */

	ClipTagIntAttribute();
	ClipTagIntAttribute(const char *name, int value, bool editable, int minValue, int maxValue, int deltaValue);

	/* Properties */

	void SetValue(int value);
	int GetValue() const;

	void SetMinValue(int minValue);
	int GetMinValue() const;
	void SetMaxValue(int maxValue);
	int GetMaxValue() const;
	void SetDeltaValue(int deltaValue);
	int GetDeltaValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	int m_Value;

	int m_MinValue;
	int m_MaxValue;
	int m_DeltaValue;
	bkSlider *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagBoolAttribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagBoolAttribute();

	/* Constructors */

	ClipTagBoolAttribute();
	ClipTagBoolAttribute(const char *name, bool value, bool editable);

	/* Properties */

	void SetValue(bool value);
	bool GetValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	bool m_Value;

	bkToggle *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagStringAttribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagStringAttribute();

	/* Constructors */

	ClipTagStringAttribute();
	ClipTagStringAttribute(const char *name, const atString &value, bool editable);

	/* Properties */

	void SetValue(const atString &value);
	const atString &GetValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Events */

	void OnWidgetChanged();

	/* Data */

	atString m_Value;

	static const int m_BufferLength = 256;
	char m_Buffer[m_BufferLength];
	bkText *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagVector3Attribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagVector3Attribute();

	/* Constructors */

	ClipTagVector3Attribute();
	ClipTagVector3Attribute(const char *name, const Vector3 &value, bool editable, float minValue, float maxValue, float deltaValue);

	/* Properties */

	void SetValue(const Vector3 &value);
	const Vector3 &GetValue() const;

	void SetMinValue(float minValue);
	float GetMinValue() const;
	void SetMaxValue(float maxValue);
	float GetMaxValue() const;
	void SetDeltaValue(float deltaValue);
	float GetDeltaValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	Vector3 m_Value;

	float m_MinValue;
	float m_MaxValue;
	float m_DeltaValue;
	bkVector3 *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagVector4Attribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagVector4Attribute();

	/* Constructors */

	ClipTagVector4Attribute();
	ClipTagVector4Attribute(const char *name, const Vector4 &value, bool editable, float minValue, float maxValue, float deltaValue);

	/* Properties */

	void SetValue(const Vector4 &value);
	const Vector4 &GetValue() const;

	void SetMinValue(float minValue);
	float GetMinValue() const;
	void SetMaxValue(float maxValue);
	float GetMaxValue() const;
	void SetDeltaValue(float deltaValue);
	float GetDeltaValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	Vector4 m_Value;

	float m_MinValue;
	float m_MaxValue;
	float m_DeltaValue;
	bkVector4 *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagQuaternionAttribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagQuaternionAttribute();

	/* Constructors */

	ClipTagQuaternionAttribute();
	ClipTagQuaternionAttribute(const char *name, const Quaternion &value, bool editable);

	/* Properties */

	void SetValue(const Quaternion &value);
	const Quaternion &GetValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Events */

	void OnWidgetChanged();

	/* Data */

	Vector4 m_Value;

	Vector3 m_Euler;
	bkGroup *m_GroupWidget;
	bkAngle *m_EulerXWidget;
	bkAngle *m_EulerYWidget;
	bkAngle *m_EulerZWidget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagMatrix34Attribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagMatrix34Attribute();

	/* Constructors */

	ClipTagMatrix34Attribute();
	ClipTagMatrix34Attribute(const char *name, const Matrix34 &value, bool editable, float minValue, float maxValue, float deltaValue);

	/* Properties */

	void SetValue(const Matrix34 &value);
	const Matrix34 &GetValue() const;

	void SetMinValue(float minValue);
	float GetMinValue() const;
	void SetMaxValue(float maxValue);
	float GetMaxValue() const;
	void SetDeltaValue(float deltaValue);
	float GetDeltaValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	Matrix34 m_Value;

	float m_MinValue;
	float m_MaxValue;
	float m_DeltaValue;
	bkMatrix34 *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagDataAttribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagDataAttribute();

	/* Constructors */

	ClipTagDataAttribute();
	ClipTagDataAttribute(const char *name, const atArray< u8 > &value, bool editable);

	/* Properties */

	void SetValue(const atArray< u8 > &value);
	const atArray< u8 > &GetValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Data */

	atArray< u8 > m_Value;

	bkTitle *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagHashStringAttribute : public ClipTagAttribute
{
public:

	/* Destructor */

	virtual ~ClipTagHashStringAttribute();

	/* Constructors */

	ClipTagHashStringAttribute();
	ClipTagHashStringAttribute(const char *name, const atHashString &value, bool editable);

	/* Properties */

	void SetValue(const atHashString &value);
	const atHashString &GetValue() const;

	/* Operations */

	virtual void AddWidgets(bkBank *bank);
	virtual void RemoveWidgets();

protected:

	/* Events */

	void OnWidgetChanged();
	void OnWidgetChangedTrim();

	/* Data */

	atHashString m_Value;

	static const int m_BufferLength = 256;
	char m_Buffer[m_BufferLength];
	bkText *m_Widget;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagProperty
{
public:

	/* Destructor */

	virtual ~ClipTagProperty();

	/* Constructors */

	ClipTagProperty();
	ClipTagProperty(const char *name);

	/* Properties */

	const char *GetName() const;
	int GetAttributeCount() const;
	ClipTagAttribute *GetAttribute(int index);
	const ClipTagAttribute *GetAttribute(int index) const;

	/* Operations */

	ClipTagFloatAttribute *CreateFloatAttribute(const char *name, float value, bool editable = false, float minValue = bkSlider::FLOAT_MIN_VALUE, float maxValue = bkSlider::FLOAT_MAX_VALUE, float deltaValue = 1.0f);
	ClipTagIntAttribute *CreateIntAttribute(const char *name, int value, bool editable = false, int minValue = INT_MIN, int maxValue = INT_MAX, int deltaValue = 1);
	ClipTagBoolAttribute *CreateBoolAttribute(const char *name, bool value, bool editable = false);
	ClipTagStringAttribute *CreateStringAttribute(const char *name, const atString &value, bool editable = false);
	ClipTagVector3Attribute *CreateVector3Attribute(const char *name, const Vector3 &value, bool editable = false, float minValue = bkSlider::FLOAT_MIN_VALUE, float maxValue = bkSlider::FLOAT_MAX_VALUE, float deltaValue = 1.0f);
	ClipTagVector4Attribute *CreateVector4Attribute(const char *name, const Vector4 &value, bool editable = false, float minValue = bkSlider::FLOAT_MIN_VALUE, float maxValue = bkSlider::FLOAT_MAX_VALUE, float deltaValue = 1.0f);
	ClipTagQuaternionAttribute *CreateQuaternionAttribute(const char *name, const Quaternion &value, bool editable = false);
	ClipTagMatrix34Attribute *CreateMatrix34Attribute(const char *name, const Matrix34 &value, bool editable = false, float minValue = bkSlider::FLOAT_MIN_VALUE, float maxValue = bkSlider::FLOAT_MAX_VALUE, float deltaValue = 1.0f);
	ClipTagDataAttribute *CreateDataAttribute(const char *name, const atArray< u8 > &value, bool editable = false);
	ClipTagHashStringAttribute *CreateHashStringAttribute(const char *name, const atHashString &value, bool editable = false);
	bool DeleteAttribute(int index);
	void DeleteAllAttributes();

protected:

	/* Data */

	atString m_Name;
	atArray< ClipTagAttribute * > m_Attributes;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagAction
{
public:

	/* Destructor */

	virtual ~ClipTagAction();

	/* Constructors */

	ClipTagAction();
	ClipTagAction(const char *name, const char *propertyName);

	/* Properties */

	const char *GetName() const;
	ClipTagProperty &GetProperty();
	const ClipTagProperty &GetProperty() const;

	/* Operations */

	void AddWidgets(bkBank *bank);
	void RemoveWidgets();

protected:

	/* Data */

	atString m_Name;
	ClipTagProperty m_Property;

	bkButton *m_ActionButton;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagActionGroup
{
public:

	/* Destructor */

	virtual ~ClipTagActionGroup();

	/* Constructors */

	ClipTagActionGroup();
	ClipTagActionGroup(const char *name);

	/* Properties */

	const char *GetName() const;
	int GetActionCount() const;
	ClipTagAction *GetAction(int index);
	const ClipTagAction *GetAction(int index) const;

	/* Operations */

	ClipTagAction *CreateAction(const char *name, const char *propertyName);
	bool DeleteAction(int index);
	void DeleteAllActions();

	void AddWidgets(bkBank *bank);
	void RemoveWidgets();

protected:

	/* Data */

	atString m_Name;
	atArray< ClipTagAction > m_Actions;

	bkGroup *m_ActionGroupGroup;

	PAR_PARSABLE;
};

///////////////////////////////////////////////////////////////////////////////

class ClipTagMetadataManager
{
public:

	/* Destructor */

	virtual ~ClipTagMetadataManager();

	/* Constructor */

	ClipTagMetadataManager();

	/* Properties */

	int GetActionGroupCount() const;
	ClipTagActionGroup *GetActionGroup(int index);
	const ClipTagActionGroup *GetActionGroup(int index) const;

	/* Operations */

	ClipTagActionGroup *CreateActionGroup(const char *name);
	bool DeleteActionGroup(int index);
	void DeleteAllActionGroups();

	bool Load();
	bool Save();

	void AddWidgets(bkBank *bank);
	void RemoveWidgets();

protected:

	/* Data */

	atArray< ClipTagActionGroup > m_ActionGroups;

	bkGroup *m_Group;

	PAR_PARSABLE;
};

extern ClipTagMetadataManager g_ClipTagMetadataManager;

///////////////////////////////////////////////////////////////////////////////

#endif // CLIP_TAG_METADATA_H

#endif // __BANK
