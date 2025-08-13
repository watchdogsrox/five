// 
// animation/debug/ClipTagMetadata.cpp 
// 
// Copyright (C) 1999-2010 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#include "ClipTagMetadata.h"

// system includes

// rage includes

// game includes

#include "animation/debug/ClipEditor.h"

#include "ClipTagMetadata_parser.h"

ANIM_OPTIMISATIONS()

//////////////////////////////////////////////////////////////////////////

ClipTagMetadataManager g_ClipTagMetadataManager;
 
const char *g_clipTagMetadataDirectory = "X:/gta5/tools_ng/etc/config/anim";
const char *g_clipTagMetadataFilename = "clip_tag_metadata";
const char *g_clipTagMetadataExtension = "xml";

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagAttribute::~ClipTagAttribute()
{
}

/* Constructors */

ClipTagAttribute::ClipTagAttribute()
: m_Type(kTypeFloat), m_Editable(false)
{
}

ClipTagAttribute::ClipTagAttribute(const char *name, eClipTagAttributeType type, bool editable)
: m_Name(name), m_Type(type), m_Editable(editable)
{
	Assert(name);
}

/* Properties */

const char *ClipTagAttribute::GetName() const
{
	return m_Name.c_str();
}

eClipTagAttributeType ClipTagAttribute::GetType() const
{
	return m_Type;
}

bool ClipTagAttribute::GetEditable() const
{
	return m_Editable;
}

/* Operations */

void ClipTagAttribute::AddWidgets(bkBank * /*bank*/)
{
}

void ClipTagAttribute::RemoveWidgets()
{
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagFloatAttribute::~ClipTagFloatAttribute()
{
}

/* Constructors */

ClipTagFloatAttribute::ClipTagFloatAttribute()
: m_Value(0.0f), m_MinValue(bkSlider::FLOAT_MIN_VALUE), m_MaxValue(bkSlider::FLOAT_MAX_VALUE), m_DeltaValue(1.0f), m_Widget(NULL)
{
}

ClipTagFloatAttribute::ClipTagFloatAttribute(const char *name, float value, bool editable, float minValue, float maxValue, float deltaValue)
: ClipTagAttribute(name, kTypeFloat, editable), m_Value(value), m_MinValue(minValue), m_MaxValue(maxValue), m_DeltaValue(deltaValue), m_Widget(NULL)
{
}

/* Properties */

void ClipTagFloatAttribute::SetValue(float value)
{
	m_Value = value;
}

float ClipTagFloatAttribute::GetValue() const
{
	return m_Value;
}

void ClipTagFloatAttribute::SetMinValue(float minValue)
{
	m_MinValue = minValue;
}

float ClipTagFloatAttribute::GetMinValue() const
{
	return m_MinValue;
}

void ClipTagFloatAttribute::SetMaxValue(float maxValue)
{
	m_MaxValue = maxValue;
}

float ClipTagFloatAttribute::GetMaxValue() const
{
	return m_MaxValue;
}

void ClipTagFloatAttribute::SetDeltaValue(float deltaValue)
{
	m_DeltaValue = deltaValue;
}

float ClipTagFloatAttribute::GetDeltaValue() const
{
	return m_DeltaValue;
}

/* Operations */

void ClipTagFloatAttribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		m_Widget = bank->AddSlider(m_Name.c_str(), &m_Value, m_MinValue, m_MaxValue, m_DeltaValue);
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagFloatAttribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagIntAttribute::~ClipTagIntAttribute()
{
}

/* Constructors */

ClipTagIntAttribute::ClipTagIntAttribute()
: m_Value(0), m_MinValue(INT_MIN), m_MaxValue(INT_MAX), m_DeltaValue(1), m_Widget(NULL)
{
}

ClipTagIntAttribute::ClipTagIntAttribute(const char *name, int value, bool editable, int minValue, int maxValue, int deltaValue)
: ClipTagAttribute(name, kTypeInt, editable), m_Value(value), m_MinValue(minValue), m_MaxValue(maxValue), m_DeltaValue(deltaValue), m_Widget(NULL)
{
}

/* Properties */

void ClipTagIntAttribute::SetValue(int value)
{
	m_Value = value;
}

int ClipTagIntAttribute::GetValue() const
{
	return m_Value;
}

void ClipTagIntAttribute::SetMinValue(int minValue)
{
	m_MinValue = minValue;
}

int ClipTagIntAttribute::GetMinValue() const
{
	return m_MinValue;
}

void ClipTagIntAttribute::SetMaxValue(int maxValue)
{
	m_MaxValue = maxValue;
}

int ClipTagIntAttribute::GetMaxValue() const
{
	return m_MaxValue;
}

void ClipTagIntAttribute::SetDeltaValue(int deltaValue)
{
	m_DeltaValue = deltaValue;
}

int ClipTagIntAttribute::GetDeltaValue() const
{
	return m_DeltaValue;
}

/* Operations */

void ClipTagIntAttribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		m_Widget = bank->AddSlider(m_Name.c_str(), &m_Value, m_MinValue, m_MaxValue, m_DeltaValue);
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagIntAttribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagBoolAttribute::~ClipTagBoolAttribute()
{
}

/* Constructors */

ClipTagBoolAttribute::ClipTagBoolAttribute()
: m_Value(false), m_Widget(NULL)
{
}

ClipTagBoolAttribute::ClipTagBoolAttribute(const char *name, bool value, bool editable)
: ClipTagAttribute(name, kTypeBool, editable), m_Value(value), m_Widget(NULL)
{
}

/* Properties */

void ClipTagBoolAttribute::SetValue(bool value)
{
	m_Value = value;
}

bool ClipTagBoolAttribute::GetValue() const
{
	return m_Value;
}

/* Operations */

void ClipTagBoolAttribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		m_Widget = bank->AddToggle(m_Name.c_str(), &m_Value);
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagBoolAttribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagStringAttribute::~ClipTagStringAttribute()
{
}

/* Constructors */

ClipTagStringAttribute::ClipTagStringAttribute()
: m_Widget(NULL)
{
	memset(m_Buffer, '\0', m_BufferLength);
}

ClipTagStringAttribute::ClipTagStringAttribute(const char *name, const atString &value, bool editable)
: ClipTagAttribute(name, kTypeString, editable), m_Value(value), m_Widget(NULL)
{
	memset(m_Buffer, '\0', m_BufferLength);
}

/* Properties */

void ClipTagStringAttribute::SetValue(const atString &value)
{
	m_Value = value;
}

const atString &ClipTagStringAttribute::GetValue() const
{
	return m_Value;
}

/* Operations */

void ClipTagStringAttribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		if(m_Value.GetLength() > 0)
		{
			strcpy(m_Buffer, m_Value.c_str());
		}
		else
		{
			memset(m_Buffer, '\0', m_BufferLength);
		}

		m_Widget = bank->AddText(m_Name.c_str(), m_Buffer, m_BufferLength, datCallback(MFA(ClipTagStringAttribute::OnWidgetChanged), this));
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagStringAttribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

/* Events */

void ClipTagStringAttribute::OnWidgetChanged()
{
	m_Value = m_Buffer;
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagVector3Attribute::~ClipTagVector3Attribute()
{
}

/* Constructors */

ClipTagVector3Attribute::ClipTagVector3Attribute()
: m_Value(Vector3::ZeroType), m_MinValue(bkSlider::FLOAT_MIN_VALUE), m_MaxValue(bkSlider::FLOAT_MAX_VALUE), m_DeltaValue(1.0f), m_Widget(NULL)
{
}

ClipTagVector3Attribute::ClipTagVector3Attribute(const char *name, const Vector3 &value, bool editable, float minValue, float maxValue, float deltaValue)
: ClipTagAttribute(name, kTypeVector3, editable), m_Value(value), m_MinValue(minValue), m_MaxValue(maxValue), m_DeltaValue(deltaValue), m_Widget(NULL)
{
}

/* Properties */

void ClipTagVector3Attribute::SetValue(const Vector3 &value)
{
	m_Value = value;
}

const Vector3 &ClipTagVector3Attribute::GetValue() const
{
	return m_Value;
}

void ClipTagVector3Attribute::SetMinValue(float minValue)
{
	m_MinValue = minValue;
}

float ClipTagVector3Attribute::GetMinValue() const
{
	return m_MinValue;
}

void ClipTagVector3Attribute::SetMaxValue(float maxValue)
{
	m_MaxValue = maxValue;
}

float ClipTagVector3Attribute::GetMaxValue() const
{
	return m_MaxValue;
}

void ClipTagVector3Attribute::SetDeltaValue(float deltaValue)
{
	m_DeltaValue = deltaValue;
}

float ClipTagVector3Attribute::GetDeltaValue() const
{
	return m_DeltaValue;
}

/* Operations */

void ClipTagVector3Attribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		m_Widget = bank->AddVector(m_Name.c_str(), &m_Value, m_MinValue, m_MaxValue, m_DeltaValue);
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagVector3Attribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagVector4Attribute::~ClipTagVector4Attribute()
{
}

/* Constructors */

ClipTagVector4Attribute::ClipTagVector4Attribute()
: m_Value(Vector4::ZeroType), m_MinValue(bkSlider::FLOAT_MIN_VALUE), m_MaxValue(bkSlider::FLOAT_MAX_VALUE), m_DeltaValue(1.0f), m_Widget(NULL)
{
}

ClipTagVector4Attribute::ClipTagVector4Attribute(const char *name, const Vector4 &value, bool editable, float minValue, float maxValue, float deltaValue)
: ClipTagAttribute(name, kTypeVector4, editable), m_Value(value), m_MinValue(minValue), m_MaxValue(maxValue), m_DeltaValue(deltaValue), m_Widget(NULL)
{
}

/* Properties */

void ClipTagVector4Attribute::SetValue(const Vector4 &value)
{
	m_Value = value;
}

const Vector4 &ClipTagVector4Attribute::GetValue() const
{
	return m_Value;
}

void ClipTagVector4Attribute::SetMinValue(float minValue)
{
	m_MinValue = minValue;
}

float ClipTagVector4Attribute::GetMinValue() const
{
	return m_MinValue;
}

void ClipTagVector4Attribute::SetMaxValue(float maxValue)
{
	m_MaxValue = maxValue;
}

float ClipTagVector4Attribute::GetMaxValue() const
{
	return m_MaxValue;
}

void ClipTagVector4Attribute::SetDeltaValue(float deltaValue)
{
	m_DeltaValue = deltaValue;
}

float ClipTagVector4Attribute::GetDeltaValue() const
{
	return m_DeltaValue;
}

/* Operations */

void ClipTagVector4Attribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		m_Widget = bank->AddVector(m_Name.c_str(), &m_Value, m_MinValue, m_MaxValue, m_DeltaValue);
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagVector4Attribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagQuaternionAttribute::~ClipTagQuaternionAttribute()
{
}

/* Constructors */

ClipTagQuaternionAttribute::ClipTagQuaternionAttribute()
: m_Value(Quaternion::IdentityType), m_GroupWidget(NULL), m_EulerXWidget(NULL), m_EulerYWidget(NULL), m_EulerZWidget(NULL)
{
}

ClipTagQuaternionAttribute::ClipTagQuaternionAttribute(const char *name, const Quaternion &value, bool editable)
: ClipTagAttribute(name, kTypeQuaternion, editable), m_Value(*reinterpret_cast< const Vector4 * >(&value)), m_GroupWidget(NULL), m_EulerXWidget(NULL), m_EulerYWidget(NULL), m_EulerZWidget(NULL)
{
}

/* Properties */

void ClipTagQuaternionAttribute::SetValue(const Quaternion &value)
{
	m_Value = *reinterpret_cast< const Vector4 * >(&value);
}

const Quaternion &ClipTagQuaternionAttribute::GetValue() const
{
	return *reinterpret_cast< const Quaternion * >(&m_Value);
}

/* Operations */

void ClipTagQuaternionAttribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		reinterpret_cast< Quaternion * >(&m_Value)->ToEulers(m_Euler);
		m_GroupWidget = bank->PushGroup(m_Name.c_str());
		{
			m_EulerXWidget = bank->AddAngle("x", &m_Euler.x, bkAngleType::RADIANS, datCallback(MFA(ClipTagQuaternionAttribute::OnWidgetChanged), this));
			m_EulerYWidget = bank->AddAngle("y", &m_Euler.y, bkAngleType::RADIANS, datCallback(MFA(ClipTagQuaternionAttribute::OnWidgetChanged), this));
			m_EulerZWidget = bank->AddAngle("z", &m_Euler.z, bkAngleType::RADIANS, datCallback(MFA(ClipTagQuaternionAttribute::OnWidgetChanged), this));
		}
		bank->PopGroup();
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagQuaternionAttribute::RemoveWidgets()
{
	if(m_EulerXWidget)
	{
		m_EulerXWidget->Destroy(); m_EulerXWidget = NULL;
	}
	if(m_EulerYWidget)
	{
		m_EulerYWidget->Destroy(); m_EulerYWidget = NULL;
	}
	if(m_EulerZWidget)
	{
		m_EulerZWidget->Destroy(); m_EulerZWidget = NULL;
	}
	if(m_GroupWidget)
	{
		m_GroupWidget->Destroy(); m_GroupWidget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

/* Events */

void ClipTagQuaternionAttribute::OnWidgetChanged()
{
	reinterpret_cast< Quaternion * >(&m_Value)->FromEulers(m_Euler);
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagMatrix34Attribute::~ClipTagMatrix34Attribute()
{
}

/* Constructors */

ClipTagMatrix34Attribute::ClipTagMatrix34Attribute()
: m_Value(Matrix34::IdentityType), m_MinValue(bkSlider::FLOAT_MIN_VALUE), m_MaxValue(bkSlider::FLOAT_MAX_VALUE), m_DeltaValue(1.0f), m_Widget(NULL)
{
}

ClipTagMatrix34Attribute::ClipTagMatrix34Attribute(const char *name, const Matrix34 &value, bool editable, float minValue, float maxValue, float deltaValue)
: ClipTagAttribute(name, kTypeMatrix34, editable), m_Value(value), m_MinValue(minValue), m_MaxValue(maxValue), m_DeltaValue(deltaValue), m_Widget(NULL)
{
}

/* Properties */

void ClipTagMatrix34Attribute::SetValue(const Matrix34 &value)
{
	m_Value = value;
}

const Matrix34 &ClipTagMatrix34Attribute::GetValue() const
{
	return m_Value;
}

void ClipTagMatrix34Attribute::SetMinValue(float minValue)
{
	m_MinValue = minValue;
}

float ClipTagMatrix34Attribute::GetMinValue() const
{
	return m_MinValue;
}

void ClipTagMatrix34Attribute::SetMaxValue(float maxValue)
{
	m_MaxValue = maxValue;
}

float ClipTagMatrix34Attribute::GetMaxValue() const
{
	return m_MaxValue;
}

void ClipTagMatrix34Attribute::SetDeltaValue(float deltaValue)
{
	m_DeltaValue = deltaValue;
}

float ClipTagMatrix34Attribute::GetDeltaValue() const
{
	return m_DeltaValue;
}

/* Operations */

void ClipTagMatrix34Attribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		m_Widget = bank->AddMatrix(m_Name.c_str(), &m_Value, m_MinValue, m_MaxValue, m_DeltaValue);
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagMatrix34Attribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagDataAttribute::~ClipTagDataAttribute()
{
}

/* Constructors */

ClipTagDataAttribute::ClipTagDataAttribute()
: m_Widget(NULL)
{
}

ClipTagDataAttribute::ClipTagDataAttribute(const char *name, const atArray< u8 > &value, bool editable)
: ClipTagAttribute(name, kTypeData, editable), m_Value(value), m_Widget(NULL)
{
}

/* Properties */

void ClipTagDataAttribute::SetValue(const atArray< u8 > &value)
{
	m_Value = value;
}

const atArray< u8 > &ClipTagDataAttribute::GetValue() const
{
	return m_Value;
}

/* Operations */

void ClipTagDataAttribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		m_Widget = bank->AddTitle("%s%s - Attribute type not supported", m_Name.c_str(), "");
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagDataAttribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagHashStringAttribute::~ClipTagHashStringAttribute()
{
}

/* Constructors */

ClipTagHashStringAttribute::ClipTagHashStringAttribute()
: m_Widget(NULL)
{
	memset(m_Buffer, '\0', m_BufferLength);
}

ClipTagHashStringAttribute::ClipTagHashStringAttribute(const char *name, const atHashString &value, bool editable)
: ClipTagAttribute(name, kTypeHashString, editable), m_Value(value), m_Widget(NULL)
{
	memset(m_Buffer, '\0', m_BufferLength);
}

/* Properties */

void ClipTagHashStringAttribute::SetValue(const atHashString &value)
{
	m_Value = value;
}

const atHashString &ClipTagHashStringAttribute::GetValue() const
{
	return m_Value;
}

/* Operations */

void ClipTagHashStringAttribute::AddWidgets(bkBank *bank)
{
	if(m_Editable)
	{
		if(m_Value.GetLength() > 0)
		{
			strcpy(m_Buffer, m_Value.GetCStr());
		}
		else
		{
			memset(m_Buffer, '\0', m_BufferLength);
		}

		// Make the removal of whitespace at the start and end of all ClipTagHashStringAttribute's the default behaviour.
		m_Widget = bank->AddText(m_Name.c_str(), m_Buffer, m_BufferLength, datCallback(MFA(ClipTagHashStringAttribute::OnWidgetChangedTrim), this));
	}

	ClipTagAttribute::AddWidgets(bank);
}

void ClipTagHashStringAttribute::RemoveWidgets()
{
	if(m_Widget)
	{
		m_Widget->Destroy(); m_Widget = NULL;
	}

	ClipTagAttribute::RemoveWidgets();
}

/* Events */

void ClipTagHashStringAttribute::OnWidgetChanged()
{
	m_Value = m_Buffer;
}

void ClipTagHashStringAttribute::OnWidgetChangedTrim()
{
	atString textEntered(m_Buffer);
	textEntered.Trim();

	strcpy(m_Buffer, textEntered.c_str());

	m_Value = textEntered.c_str();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagProperty::~ClipTagProperty()
{
	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		delete m_Attributes[index]; m_Attributes[index] = NULL;
	}
	m_Attributes.Reset();
}

/* Constructors */

ClipTagProperty::ClipTagProperty()
{
}

ClipTagProperty::ClipTagProperty(const char *name)
: m_Name(name)
{
	Assert(name);
}

/* Properties */

const char *ClipTagProperty::GetName() const
{
	return m_Name.c_str();
}

int ClipTagProperty::GetAttributeCount() const
{
	return m_Attributes.GetCount();
}

ClipTagAttribute *ClipTagProperty::GetAttribute(int index)
{
	Assert(index >= 0 && index < m_Attributes.GetCount());
	if(index >= 0 && index < m_Attributes.GetCount())
	{
		return m_Attributes[index];
	}

	return NULL;
}

const ClipTagAttribute *ClipTagProperty::GetAttribute(int index) const
{
	Assert(index >= 0 && index < m_Attributes.GetCount());
	if(index >= 0 && index < m_Attributes.GetCount())
	{
		return m_Attributes[index];
	}

	return NULL;
}

/* Operations */

ClipTagFloatAttribute *ClipTagProperty::CreateFloatAttribute(const char *name, float value, bool editable /*= false*/, float minValue /*= bkSlider::FLOAT_MIN_VALUE*/, float maxValue /*= bkSlider::FLOAT_MAX_VALUE*/, float deltaValue /*= 1.0f*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagFloatAttribute *clipTagFloatAttribute = rage_new ClipTagFloatAttribute(name, value, editable, minValue, maxValue, deltaValue);

	m_Attributes.PushAndGrow(clipTagFloatAttribute);

	return clipTagFloatAttribute;
}

ClipTagIntAttribute *ClipTagProperty::CreateIntAttribute(const char *name, int value, bool editable /*= false*/, int minValue /*= INT_MIN*/, int maxValue /*= INT_MAX*/, int deltaValue /*= 1*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagIntAttribute *clipTagIntAttribute = rage_new ClipTagIntAttribute(name, value, editable, minValue, maxValue, deltaValue);

	m_Attributes.PushAndGrow(clipTagIntAttribute);

	return clipTagIntAttribute;
}

ClipTagBoolAttribute *ClipTagProperty::CreateBoolAttribute(const char *name, bool value, bool editable /*= false*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagBoolAttribute *clipTagBoolAttribute = rage_new ClipTagBoolAttribute(name, value, editable);

	m_Attributes.PushAndGrow(clipTagBoolAttribute);

	return clipTagBoolAttribute;
}

ClipTagStringAttribute *ClipTagProperty::CreateStringAttribute(const char *name, const atString &value, bool editable /*= false*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagStringAttribute *clipTagStringAttribute = rage_new ClipTagStringAttribute(name, value, editable);

	m_Attributes.PushAndGrow(clipTagStringAttribute);

	return clipTagStringAttribute;
}

ClipTagVector3Attribute *ClipTagProperty::CreateVector3Attribute(const char *name, const Vector3 &value, bool editable /*= false*/, float minValue /*= bkSlider::FLOAT_MIN_VALUE*/, float maxValue /*= bkSlider::FLOAT_MAX_VALUE*/, float deltaValue /*= 1.0f*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagVector3Attribute *clipTagVector3Attribute = rage_new ClipTagVector3Attribute(name, value, editable, minValue, maxValue, deltaValue);

	m_Attributes.PushAndGrow(clipTagVector3Attribute);

	return clipTagVector3Attribute;
}

ClipTagVector4Attribute *ClipTagProperty::CreateVector4Attribute(const char *name, const Vector4 &value, bool editable /*= false*/, float minValue /*= bkSlider::FLOAT_MIN_VALUE*/, float maxValue /*= bkSlider::FLOAT_MAX_VALUE*/, float deltaValue /*= 1.0f*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagVector4Attribute *clipTagVector4Attribute = rage_new ClipTagVector4Attribute(name, value, editable, minValue, maxValue, deltaValue);

	m_Attributes.PushAndGrow(clipTagVector4Attribute);

	return clipTagVector4Attribute;
}

ClipTagQuaternionAttribute *ClipTagProperty::CreateQuaternionAttribute(const char *name, const Quaternion &value, bool editable /*= false*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagQuaternionAttribute *clipTagQuaternionAttribute = rage_new ClipTagQuaternionAttribute(name, value, editable);

	m_Attributes.PushAndGrow(clipTagQuaternionAttribute);

	return clipTagQuaternionAttribute;
}

ClipTagMatrix34Attribute *ClipTagProperty::CreateMatrix34Attribute(const char *name, const Matrix34 &value, bool editable /*= false*/, float minValue /*= bkSlider::FLOAT_MIN_VALUE*/, float maxValue /*= bkSlider::FLOAT_MAX_VALUE*/, float deltaValue /*= 1.0f*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagMatrix34Attribute *clipTagMatrix34Attribute = rage_new ClipTagMatrix34Attribute(name, value, editable, minValue, maxValue, deltaValue);

	m_Attributes.PushAndGrow(clipTagMatrix34Attribute);

	return clipTagMatrix34Attribute;
}

ClipTagDataAttribute *ClipTagProperty::CreateDataAttribute(const char *name, const atArray< u8 > &value, bool editable /*= false*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagDataAttribute *clipTagDataAttribute = rage_new ClipTagDataAttribute(name, value, editable);

	m_Attributes.PushAndGrow(clipTagDataAttribute);

	return clipTagDataAttribute;
}

ClipTagHashStringAttribute *ClipTagProperty::CreateHashStringAttribute(const char *name, const atHashString &value, bool editable /*= false*/)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		if(stricmp(m_Attributes[index]->GetName(), name) == 0)
		{
			return NULL;
		}
	}

	ClipTagHashStringAttribute *clipTagHashStringAttribute = rage_new ClipTagHashStringAttribute(name, value, editable);

	m_Attributes.PushAndGrow(clipTagHashStringAttribute);

	return clipTagHashStringAttribute;
}

bool ClipTagProperty::DeleteAttribute(int index)
{
	Assert(index >= 0 && index < m_Attributes.GetCount());
	if(index >= 0 && index < m_Attributes.GetCount())
	{
		delete m_Attributes[index]; m_Attributes[index] = NULL;

		m_Attributes.Delete(index);

		return true;
	}

	return false;
}

void ClipTagProperty::DeleteAllAttributes()
{
	for(int index = 0; index < m_Attributes.GetCount(); index ++)
	{
		delete m_Attributes[index]; m_Attributes[index] = NULL;
	}
	m_Attributes.Reset();
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagAction::~ClipTagAction()
{
}

/* Constructors */

ClipTagAction::ClipTagAction()
: m_ActionButton(NULL)
{
}

ClipTagAction::ClipTagAction(const char *name, const char *propertyName)
: m_Name(name), m_Property(propertyName), m_ActionButton(NULL)
{
	Assert(name);
}

/* Properties */

const char *ClipTagAction::GetName() const
{
	return m_Name.c_str();
}

ClipTagProperty &ClipTagAction::GetProperty()
{
	return m_Property;
}

const ClipTagProperty &ClipTagAction::GetProperty() const
{
	return m_Property;
}

/* Operations */

void ClipTagAction::AddWidgets(bkBank *bank)
{
	for(int clipTagAttributeIndex = 0; clipTagAttributeIndex < m_Property.GetAttributeCount(); clipTagAttributeIndex ++)
	{
		ClipTagAttribute *clipTagAttribute = m_Property.GetAttribute(clipTagAttributeIndex);

		clipTagAttribute->AddWidgets(bank);
	}

	m_ActionButton = bank->AddButton(m_Name.c_str(), datCallback(CFA1(CAnimClipEditor::ActionButton), this));
}

void ClipTagAction::RemoveWidgets()
{
	for(int clipTagAttributeIndex = 0; clipTagAttributeIndex < m_Property.GetAttributeCount(); clipTagAttributeIndex ++)
	{
		ClipTagAttribute *clipTagAttribute = m_Property.GetAttribute(clipTagAttributeIndex);

		clipTagAttribute->RemoveWidgets();
	}

	if(m_ActionButton)
	{
		m_ActionButton->Destroy(); m_ActionButton = NULL;
	}
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagActionGroup::~ClipTagActionGroup()
{
	m_Actions.Reset();
}

/* Constructors */

ClipTagActionGroup::ClipTagActionGroup()
{
}

ClipTagActionGroup::ClipTagActionGroup(const char *name)
: m_Name(name), m_ActionGroupGroup(NULL)
{
	Assert(name);
}

/* Properties */

const char *ClipTagActionGroup::GetName() const
{
	return m_Name.c_str();
}

int ClipTagActionGroup::GetActionCount() const
{
	return m_Actions.GetCount();
}

ClipTagAction *ClipTagActionGroup::GetAction(int index)
{
	Assert(index >= 0 && index < m_Actions.GetCount());
	if(index >= 0 && index < m_Actions.GetCount())
	{
		return &m_Actions[index];
	}

	return NULL;
}

const ClipTagAction *ClipTagActionGroup::GetAction(int index) const
{
	Assert(index >= 0 && index < m_Actions.GetCount());
	if(index >= 0 && index < m_Actions.GetCount())
	{
		return &m_Actions[index];
	}

	return NULL;
}

/* Operations */

ClipTagAction *ClipTagActionGroup::CreateAction(const char *name, const char *propertyName)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	Assert(propertyName);
	if(!propertyName)
	{
		return NULL;
	}

	for(int index = 0; index < m_Actions.GetCount(); index ++)
	{
		if(stricmp(m_Actions[index].GetName(), name) == 0)
		{
			return NULL;
		}
	}

	m_Actions.PushAndGrow(ClipTagAction(name, propertyName));

	return &m_Actions[m_Actions.GetCount() - 1];
}

bool ClipTagActionGroup::DeleteAction(int index)
{
	Assert(index >= 0 && index < m_Actions.GetCount());
	if(index >= 0 && index < m_Actions.GetCount())
	{
		m_Actions.Delete(index);

		return true;
	}

	return false;
}

void ClipTagActionGroup::DeleteAllActions()
{
	m_Actions.Reset();
}

void ClipTagActionGroup::AddWidgets(bkBank *bank)
{
	m_ActionGroupGroup = bank->PushGroup(m_Name.c_str());
	{
		for(int actionIndex = 0; actionIndex < m_Actions.GetCount(); actionIndex ++)
		{
			ClipTagAction *clipTagAction = &m_Actions[actionIndex];

			clipTagAction->AddWidgets(bank);
		}
	}
	bank->PopGroup();
}

void ClipTagActionGroup::RemoveWidgets()
{
	for(int actionIndex = 0; actionIndex < m_Actions.GetCount(); actionIndex ++)
	{
		ClipTagAction *clipTagAction = &m_Actions[actionIndex];

		clipTagAction->RemoveWidgets();
	}
}

///////////////////////////////////////////////////////////////////////////////

/* Destructor */

ClipTagMetadataManager::~ClipTagMetadataManager()
{
	m_ActionGroups.Reset();
}

/* Constructor */

ClipTagMetadataManager::ClipTagMetadataManager()
: m_Group(NULL)
{
}

/* Properties */

int ClipTagMetadataManager::GetActionGroupCount() const
{
	return m_ActionGroups.GetCount();
}

ClipTagActionGroup *ClipTagMetadataManager::GetActionGroup(int index)
{
	Assert(index >= 0 && index < m_ActionGroups.GetCount());
	if(index >= 0 && index < m_ActionGroups.GetCount())
	{
		return &m_ActionGroups[index];
	}

	return NULL;
}

const ClipTagActionGroup *ClipTagMetadataManager::GetActionGroup(int index) const
{
	Assert(index >= 0 && index < m_ActionGroups.GetCount());
	if(index >= 0 && index < m_ActionGroups.GetCount())
	{
		return &m_ActionGroups[index];
	}

	return NULL;
}

/* Operations */

ClipTagActionGroup *ClipTagMetadataManager::CreateActionGroup(const char *name)
{
	Assert(name);
	if(!name)
	{
		return NULL;
	}

	for(int index = 0; index < m_ActionGroups.GetCount(); index ++)
	{
		if(stricmp(m_ActionGroups[index].GetName(), name) == 0)
		{
			return NULL;
		}
	}

	m_ActionGroups.PushAndGrow(ClipTagActionGroup(name));

	return &m_ActionGroups[m_ActionGroups.GetCount() - 1];
}

bool ClipTagMetadataManager::DeleteActionGroup(int index)
{
	Assert(index >= 0 && index < m_ActionGroups.GetCount());
	if(index >= 0 && index < m_ActionGroups.GetCount())
	{
		m_ActionGroups.Delete(index);

		return true;
	}

	return false;
}

void ClipTagMetadataManager::DeleteAllActionGroups()
{
	m_ActionGroups.Reset();
}

bool ClipTagMetadataManager::Load()
{
	bool loaded = false;

	DeleteAllActionGroups();

	ASSET.PushFolder(g_clipTagMetadataDirectory);

	if(Verifyf(ASSET.Exists(g_clipTagMetadataFilename, g_clipTagMetadataExtension), "Could not load %s.%s, file does not exist!", g_clipTagMetadataFilename, g_clipTagMetadataExtension))
	{
		loaded = PARSER.LoadObject(g_clipTagMetadataFilename, g_clipTagMetadataExtension, *this);
	}

	ASSET.PopFolder();

	return loaded;
}

bool ClipTagMetadataManager::Save()
{
	ASSET.PushFolder(g_clipTagMetadataDirectory);

	bool saved = PARSER.SaveObject(g_clipTagMetadataFilename, g_clipTagMetadataExtension, this);

	ASSET.PopFolder();

	return saved;
}

void ClipTagMetadataManager::AddWidgets(bkBank *bank)
{
	m_Group = bank->PushGroup("Add tags");
	{
		for(int actionGroupIndex = 0; actionGroupIndex < g_ClipTagMetadataManager.GetActionGroupCount(); actionGroupIndex ++)
		{
			ClipTagActionGroup *clipTagActionGroup = g_ClipTagMetadataManager.GetActionGroup(actionGroupIndex);

			clipTagActionGroup->AddWidgets(bank);
		}
	}
	bank->PopGroup();
}

void ClipTagMetadataManager::RemoveWidgets()
{
	for(int actionGroupIndex = 0; actionGroupIndex < g_ClipTagMetadataManager.GetActionGroupCount(); actionGroupIndex ++)
	{
		ClipTagActionGroup *clipTagActionGroup = g_ClipTagMetadataManager.GetActionGroup(actionGroupIndex);

		clipTagActionGroup->RemoveWidgets();
	}
}

///////////////////////////////////////////////////////////////////////////////

#endif //__BANK
