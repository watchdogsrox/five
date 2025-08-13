// 
// debug/BankDebug.cpp 
// 
// Copyright (C) 1999-2014 Rockstar Games.  All Rights Reserved. 
// 

#if __BANK

#include "bank/bank.h"
#include "bank/button.h"
#include "bank/color.h"
#include "bank/combo.h"
#include "bank/data.h"
#include "bank/slider.h"
#include "bank/toggle.h"
#include "bank/bkmatrix.h"
#include "bank/bkvector.h"


#include "BankDebug.h"

const char* g_ComboItems[10] = {
	"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"
};

CBankDebug::CBankDebug() :
	m_vec2Angle(0.0f, 1.0f),
	m_vec3Color(0.0f, 0.0f, 0.0f), m_vec4Color(0.0f, 0.0f, 0.0f, 1.0f), m_color32(0.0f, 0.0f, 0.0f, 1.0f),
	m_matrix33(Matrix33::IdentityType), m_matrix34(Matrix34::IdentityType), m_matrix44(Matrix44::IdentityType),
	m_bitSetToggle(16),
	m_vector2(0.0f, 0.0f), m_vector3(0.0f, 0.0f, 0.0f), m_vector4(0.0f, 0.0f, 0.0f, 0.0f)
{
	// Angles
	m_angleDegrees = 0.0f;
	m_angleFractions = 0.0f;
	m_angleRadians = 0.0f;
	m_anglesEnabled = true;
	m_pDegreesAngleWidget = NULL;
	m_pFractionAngleWidget = NULL;
	m_pRadiansAngleWidget = NULL;
	m_pVec2AngleWidget = NULL;

	// Buttons
	m_buttonPressCount = 0;
	m_buttonEnabled = true;
	m_pButtonWidget = NULL;

	// Colors
	m_floatColorArray[0] = 0.0f;
	m_floatColorArray[1] = 0.0f;
	m_floatColorArray[2] = 0.0f;
	m_floatColorArray[3] = 0.0f;
	m_colorEnabled = true;
	m_colorChangeCount = 0;
	m_pColor3Widget = NULL;
	m_pColor4Widget = NULL;
	m_pColor32Widget = NULL;
	m_pColorArrayWidget = NULL;

	// Combo
	m_comboSelection = 0;
	m_comboEnabled = true;
	m_pComboWidget = NULL;

	// Data
	m_dataEnabled = true;
	m_dataChangeCount = 0;
	m_pDataWidget = NULL;

	// Matrices
	m_matricesEnabled = true;
	m_matrixChangeCount = 0;
	m_pMatrix33Widget = NULL;
	m_pMatrix34Widget = NULL;
	m_pMatrix44Widget = NULL;

	// Sliders
	m_floatSlider = 0.0f;
	m_u8Slider = 0;
	m_s8Slider = 0;
	m_u16Slider = 0;
	m_s16Slider = 0;
	m_u32Slider = 0;
	m_s32Slider = 0;
	m_slidersEnabled = true;
	m_pFloatSliderWidget = NULL;
	m_pU8SliderWidget = NULL;
	m_pS8SliderWidget = NULL;
	m_pU16SliderWidget = NULL;
	m_pS16SliderWidget = NULL;
	m_pU32SliderWidget = NULL;
	m_pS32SliderWidget = NULL;

	// Toggles
	m_boolToggle = true;
	m_intToggle = 0;
	m_floatToggle = 0.0f;
	m_u32Toggle = 0;
	m_u16Toggle = 0;
	m_u8Toggle = 0;
	m_togglesEnabled = true;
	m_pBoolToggleWidget = NULL;
	m_pIntegerToggleWidget = NULL;
	m_pFloatToggleWidget = NULL;
	m_pU32ToggleWidget = NULL;
	m_pU16ToggleWidget = NULL;
	m_pU8ToggleWidget = NULL;
	m_pBitsetToggleWidget = NULL;

	// Vectors
	m_pVector2Widget = NULL;
	m_pVector3Widget = NULL;
	m_pVector4Widget = NULL;
	m_vectorsEnabled = true;
	m_vectorChangeCount = 0;
}

CBankDebug::~CBankDebug()
{
}

void CBankDebug::InitWidgets(bkBank& bank)
{
	bank.PushGroup("RAG Debug");

	AddAngleWidgets(bank);
	AddButtonWidgets(bank);
	AddColorWidgets(bank);
	AddComboWidgets(bank);
	AddDataWidgets(bank);
	//AddImageViewerWidgets(bank);
	//AddListWidgets(bank);
	AddMatrixWidgets(bank);
	AddSliderWidgets(bank);
	//AddTextWidgets(bank);
	//AddTitleWidgets(bank);
	AddToggleWidgets(bank);
	AddVectorWidgets(bank);

	bank.PopGroup();
}

void CBankDebug::AddAngleWidgets(bkBank& bank)
{
	bank.PushGroup("Angle");

	m_pDegreesAngleWidget = bank.AddAngle("Angle Degrees", &m_angleDegrees, bkAngleType::DEGREES);
	m_pFractionAngleWidget = bank.AddAngle("Angle Fractions", &m_angleFractions, bkAngleType::FRACTION);
	m_pRadiansAngleWidget = bank.AddAngle("Angle Radians", &m_angleRadians, bkAngleType::RADIANS);
	m_pVec2AngleWidget = bank.AddAngle("Angle Vec2", &m_vec2Angle, 0.0f, 1.0f);
	bank.AddSeparator();
	bank.AddToggle("Angle enabled", &m_anglesEnabled, datCallback(MFA(CBankDebug::OnAngleReadOnlyChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddAngle("Angle Degrees (always read-only)", &m_angleDegrees, bkAngleType::DEGREES, NullCB, NULL, NULL, true);
	bank.AddAngle("Angle Fractions (always read-only)", &m_angleFractions, bkAngleType::FRACTION, NullCB, NULL, NULL, true);
	bank.AddAngle("Angle Radians (always read-only)", &m_angleRadians, bkAngleType::RADIANS, NullCB, NULL, NULL, true);
	bank.AddAngle("Angle Vec2 (always read-only)", &m_vec2Angle, 0.0f, 1.0f, NullCB, NULL, NULL, true);

	bank.PopGroup();
}

void CBankDebug::OnAngleReadOnlyChanged()
{
	m_pDegreesAngleWidget->SetReadOnly(!m_anglesEnabled);
	m_pFractionAngleWidget->SetReadOnly(!m_anglesEnabled);
	m_pRadiansAngleWidget->SetReadOnly(!m_anglesEnabled);
	m_pVec2AngleWidget->SetReadOnly(!m_anglesEnabled);
}

void CBankDebug::AddButtonWidgets(bkBank& bank)
{
	bank.PushGroup("Button");
	m_pButtonWidget = bank.AddButton("Press Me", datCallback(MFA(CBankDebug::OnButtonPress), (datBase*)this));
	bank.AddText("Press Count", &m_buttonPressCount, true);
	bank.AddToggle("'Press Me' button enabled", &m_buttonEnabled, datCallback(MFA(CBankDebug::OnButtonReadOnlyChanged), (datBase*)this));
	bank.AddButton("Disabled button", NullCB, NULL, NULL, true);
	bank.PopGroup();
}

void CBankDebug::OnButtonPress()
{
	m_buttonPressCount++;
}

void CBankDebug::OnButtonReadOnlyChanged()
{
	m_pButtonWidget->SetReadOnly(!m_buttonEnabled);
}

void CBankDebug::AddColorWidgets(bkBank& bank)
{
	bank.PushGroup("Color");
	
	m_pColor3Widget = bank.AddColor("Color Vec3", &m_vec3Color, datCallback(MFA(CBankDebug::OnColorValueChanged), (datBase*)this));
	m_pColor4Widget = bank.AddColor("Color Vec4", &m_vec4Color, datCallback(MFA(CBankDebug::OnColorValueChanged), (datBase*)this));
	m_pColor32Widget = bank.AddColor("Color 32", &m_color32, datCallback(MFA(CBankDebug::OnColorValueChanged), (datBase*)this));
	m_pColorArrayWidget = bank.AddColor("Color Array", &m_floatColorArray[0], 4, datCallback(MFA(CBankDebug::OnColorValueChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddText("Change count", &m_colorChangeCount, true);
	bank.AddToggle("Colors enabled", &m_colorEnabled, datCallback(MFA(CBankDebug::OnColorReadOnlyChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddColor("Color Vec3 (always read-only)", &m_vec3Color, NullCB, NULL, NULL, false, true);
	bank.AddColor("Color Vec4 (always read-only)", &m_vec4Color, NullCB, NULL, NULL, false, true);
	bank.AddColor("Color 32 (always read-only)", &m_color32, NullCB, NULL, NULL, false, true);
	bank.AddColor("Color Array (always read-only)", &m_floatColorArray[0], 4, NullCB, NULL, NULL, false, true);

	bank.PopGroup();
}

void CBankDebug::OnColorValueChanged()
{
	++m_colorChangeCount;
}

void CBankDebug::OnColorReadOnlyChanged()
{
	m_pColor3Widget->SetReadOnly(!m_colorEnabled);
	m_pColor4Widget->SetReadOnly(!m_colorEnabled);
	m_pColor32Widget->SetReadOnly(!m_colorEnabled);
	m_pColorArrayWidget->SetReadOnly(!m_colorEnabled);
}

void CBankDebug::AddComboWidgets(bkBank& bank)
{
	bank.PushGroup("Combo");

	m_pComboWidget = bank.AddCombo("Combo", &m_comboSelection, 10, g_ComboItems);
	bank.AddSeparator();
	bank.AddToggle("Combo enabled", &m_comboEnabled, datCallback(MFA(CBankDebug::OnComboReadOnlyChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddCombo("Combo", &m_comboSelection, 10, g_ComboItems, NullCB, NULL, NULL, true);

	bank.PopGroup();
}

void CBankDebug::OnComboReadOnlyChanged()
{
	m_pComboWidget->SetReadOnly(!m_comboEnabled);
}

void CBankDebug::AddDataWidgets(bkBank& bank)
{
	bank.PushGroup("Data");
	bank.AddTitle("Todo");

	m_pDataWidget = bank.AddDataWidget("Data", &m_dataWidgetBuffer[0], DATA_WIDGET_SIZE, datCallback(MFA(CBankDebug::OnDataBufferChanged), (datBase*)this), NULL, true);
	bank.AddSeparator();
	bank.AddText("Change count", &m_dataChangeCount, true);
	bank.AddToggle("Data enabled", &m_dataEnabled, datCallback(MFA(CBankDebug::OnDataReadOnlyChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddDataWidget("Data (always read-only)", &m_dataWidgetBuffer[0], DATA_WIDGET_SIZE, NullCB, NULL, true, NULL, true);

	bank.PopGroup();
}

void CBankDebug::OnDataBufferChanged()
{
	++m_dataChangeCount;
}

void CBankDebug::OnDataReadOnlyChanged()
{
	m_pDataWidget->SetReadOnly(!m_dataEnabled);
}

void CBankDebug::AddImageViewerWidgets(bkBank& bank)
{
	bank.PushGroup("Image Viewer");
	bank.AddTitle("Todo");
	bank.PopGroup();
}

void CBankDebug::AddListWidgets(bkBank& bank)
{
	bank.PushGroup("List");
	bank.AddTitle("Todo");
	bank.PopGroup();
}

void CBankDebug::AddMatrixWidgets(bkBank& bank)
{
	bank.PushGroup("Matrix");

	m_pMatrix33Widget = bank.AddMatrix("Matrix33", &m_matrix33, -1000.0f, 1000.0f, 1.0f, datCallback(MFA(CBankDebug::OnMatrix33ValueChanged), (datBase*)this));
	m_pMatrix34Widget = bank.AddMatrix("Matrix34", &m_matrix34, -1000.0f, 1000.0f, 1.0f, datCallback(MFA(CBankDebug::OnMatrix34ValueChanged), (datBase*)this));
	m_pMatrix44Widget = bank.AddMatrix("Matrix44", &m_matrix44, -1000.0f, 1000.0f, 1.0f, datCallback(MFA(CBankDebug::OnMatrix44ValueChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddText("Change count", &m_matrixChangeCount, true);
	bank.AddToggle("Matrices enabled", &m_matricesEnabled, datCallback(MFA(CBankDebug::OnMatrixReadOnlyChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddMatrix("Matrix33 (always read-only)", &m_matrix33, -1000.0f, 1000.0f, 1.0f, NullCB, NULL, NULL, true);
	bank.AddMatrix("Matrix34 (always read-only)", &m_matrix34, -1000.0f, 1000.0f, 1.0f, NullCB, NULL, NULL, true);
	bank.AddMatrix("Matrix44 (always read-only)", &m_matrix44, -1000.0f, 1000.0f, 1.0f, NullCB, NULL, NULL, true);

	bank.PopGroup();
}

void CBankDebug::OnMatrix33ValueChanged()
{
	++m_matrixChangeCount;
}

void CBankDebug::OnMatrix34ValueChanged()
{
	++m_matrixChangeCount;
}

void CBankDebug::OnMatrix44ValueChanged()
{
	++m_matrixChangeCount;
}

void CBankDebug::OnMatrixReadOnlyChanged()
{
	m_pMatrix33Widget->SetReadOnly(!m_matricesEnabled);
	m_pMatrix34Widget->SetReadOnly(!m_matricesEnabled);
	m_pMatrix44Widget->SetReadOnly(!m_matricesEnabled);
}

void CBankDebug::AddSliderWidgets(bkBank& bank)
{
	bank.PushGroup("Slider");

	m_pFloatSliderWidget = bank.AddSlider("Float", &m_floatSlider, -100.0f, 100.0f, 1.0f);
	m_pU8SliderWidget = bank.AddSlider("U8", &m_u8Slider, 0, 100, 1);;
	m_pS8SliderWidget = bank.AddSlider("S8", &m_s8Slider, -100, 100, 1);
	m_pU16SliderWidget = bank.AddSlider("U16", &m_u16Slider, 0, 1000, 10);
	m_pS16SliderWidget = bank.AddSlider("S16", &m_s16Slider, -1000, 1000, 10);
	m_pU32SliderWidget = bank.AddSlider("U32", &m_u32Slider, 0, 100000, 100);
	m_pS32SliderWidget = bank.AddSlider("S32", &m_s32Slider, -100000, 100000, 100);
	bank.AddSeparator();
	bank.AddToggle("Sliders enabled", &m_slidersEnabled, datCallback(MFA(CBankDebug::OnSliderReadOnlyChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddSlider("Float", &m_floatSlider, -100.0f, 100.0f, 1.0f, NullCB, NULL, NULL, false, true);
	bank.AddSlider("U8", &m_u8Slider, 0, 100, 1, NullCB, NULL, NULL, false, true);
	bank.AddSlider("S8", &m_s8Slider, -100, 100, 1, NullCB, NULL, NULL, false, true);
	bank.AddSlider("U16", &m_u16Slider, 0, 1000, 10, NullCB, NULL, NULL, false, true);
	bank.AddSlider("S16", &m_s16Slider, -1000, 1000, 10, NullCB, NULL, NULL, false, true);
	bank.AddSlider("U32", &m_u32Slider, 0, 100000, 100, NullCB, NULL, NULL, false, true);
	bank.AddSlider("S32", &m_s32Slider, -100000, 100000, 100, NullCB, NULL, NULL, false, true);

	bank.PopGroup();
}

void CBankDebug::OnSliderReadOnlyChanged()
{
	m_pFloatSliderWidget->SetReadOnly(!m_slidersEnabled);
	m_pU8SliderWidget->SetReadOnly(!m_slidersEnabled);
	m_pS8SliderWidget->SetReadOnly(!m_slidersEnabled);
	m_pU16SliderWidget->SetReadOnly(!m_slidersEnabled);
	m_pS16SliderWidget->SetReadOnly(!m_slidersEnabled);
	m_pU32SliderWidget->SetReadOnly(!m_slidersEnabled);
	m_pS32SliderWidget->SetReadOnly(!m_slidersEnabled);
}

void CBankDebug::AddTextWidgets(bkBank& bank)
{
	bank.PushGroup("Text");
	bank.AddTitle("Todo");
	bank.PopGroup();
}

void CBankDebug::AddTitleWidgets(bkBank& bank)
{
	bank.PushGroup("Title");
	bank.AddTitle("Todo");
	bank.PopGroup();
}

void CBankDebug::AddToggleWidgets(bkBank& bank)
{
	bank.PushGroup("Toggle");

	m_pBoolToggleWidget = bank.AddToggle("Bool", &m_boolToggle);
	m_pIntegerToggleWidget = bank.AddToggle("Int", &m_intToggle);
	m_pFloatToggleWidget = bank.AddToggle("Float", &m_floatToggle);
	m_pU32ToggleWidget = bank.AddToggle("U32", &m_u32Toggle, (u32)BIT(0));
	m_pU16ToggleWidget = bank.AddToggle("U16", &m_u16Toggle, (u16)BIT(0));
	m_pU8ToggleWidget = bank.AddToggle("U8", &m_u8Toggle, (u8)BIT(0));
	m_pBitsetToggleWidget = bank.AddToggle("BitSet", &m_bitSetToggle, (u8)BIT(0));
	bank.AddSeparator();
	bank.AddToggle("Toggles enabled", &m_togglesEnabled, datCallback(MFA(CBankDebug::OnToggleReadOnlyChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddToggle("Bool (always read-only)", &m_boolToggle, NullCB, NULL, NULL, true);
	bank.AddToggle("Int (always read-only)", &m_intToggle, NullCB, NULL, NULL, true);
	bank.AddToggle("Float (always read-only)", &m_floatToggle, NullCB, NULL, NULL, true);
	bank.AddToggle("U32 (always read-only)", &m_u32Toggle, (u32)BIT(0), NullCB, NULL, NULL, true);
	bank.AddToggle("U16 (always read-only)", &m_u16Toggle, (u16)BIT(0), NullCB, NULL, NULL, true);
	bank.AddToggle("U8 (always read-only)", &m_u8Toggle, (u8)BIT(0), NullCB, NULL, NULL, true);
	bank.AddToggle("BitSet (always read-only)", &m_bitSetToggle, (u8)BIT(0), NullCB, NULL, NULL, true);

	bank.PopGroup();
}

void CBankDebug::OnToggleReadOnlyChanged()
{
	m_pBoolToggleWidget->SetReadOnly(!m_togglesEnabled);
	m_pIntegerToggleWidget->SetReadOnly(!m_togglesEnabled);
	m_pFloatToggleWidget->SetReadOnly(!m_togglesEnabled);
	m_pU32ToggleWidget->SetReadOnly(!m_togglesEnabled);
	m_pU16ToggleWidget->SetReadOnly(!m_togglesEnabled);
	m_pU8ToggleWidget->SetReadOnly(!m_togglesEnabled);
	m_pBitsetToggleWidget->SetReadOnly(!m_togglesEnabled);
}

void CBankDebug::AddVectorWidgets(bkBank& bank)
{
	bank.PushGroup("Vector");

	m_pVector2Widget = bank.AddVector("Vector2", &m_vector2, -1000.0f, 1000.0f, 1.0f, datCallback(MFA(CBankDebug::OnVector2ValueChanged), (datBase*)this));
	m_pVector3Widget = bank.AddVector("Vector3", &m_vector3, -1000.0f, 1000.0f, 1.0f, datCallback(MFA(CBankDebug::OnVector3ValueChanged), (datBase*)this));
	m_pVector4Widget = bank.AddVector("Vector4", &m_vector4, -1000.0f, 1000.0f, 1.0f, datCallback(MFA(CBankDebug::OnVector4ValueChanged), (datBase*)this));
	bank.AddSeparator();
	bank.AddText("Change count", &m_vectorChangeCount, true);
	bank.AddToggle("Vectors enabled", &m_vectorsEnabled, datCallback(MFA(CBankDebug::OnVectorReadOnlyChanged), (datBase*)this));	
	bank.AddSeparator();
	bank.AddVector("Vector2 (always read-only)", &m_vector2, -1000.0f, 1000.0f, 1.0f, NullCB, NULL, NULL, true);
	bank.AddVector("Vector3 (always read-only)", &m_vector3, -1000.0f, 1000.0f, 1.0f, NullCB, NULL, NULL, true);
	bank.AddVector("Vector4 (always read-only)", &m_vector4, -1000.0f, 1000.0f, 1.0f, NullCB, NULL, NULL, true);

	bank.PopGroup();
}

void CBankDebug::OnVector2ValueChanged()
{
	++m_vectorChangeCount;
}

void CBankDebug::OnVector3ValueChanged()
{
	++m_vectorChangeCount;
}

void CBankDebug::OnVector4ValueChanged()
{
	++m_vectorChangeCount;
}

void CBankDebug::OnVectorReadOnlyChanged()
{
	m_pVector2Widget->SetReadOnly(!m_vectorsEnabled);
	m_pVector3Widget->SetReadOnly(!m_vectorsEnabled);
	m_pVector4Widget->SetReadOnly(!m_vectorsEnabled);
}

#endif // __BANK

