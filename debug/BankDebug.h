//
// filename:	BankDebug.h
// description: Class for debugging bank widgets via RAG.
// 
// Used for ensuring that all widget types are working as expected.
//
//

#ifndef INC_BANK_DEBUG_H_
#define INC_BANK_DEBUG_H_

// --- Include Files ------------------------------------------------------------

// C headers

// Rage headers
#include "bank/bank.h"
#include "vector/color32.h"
#include "vector/vector2.h"
#include "vector/vector3.h"
#include "vector/vector4.h"
#include "vector/matrix33.h"
#include "vector/matrix34.h"
#include "vector/matrix44.h"

// --- Defines ------------------------------------------------------------------


// --- Forward Declarations -----------------------------------------------------

namespace rage
{
	class bkAngle;
	class bkButton;
	class bkColor;
	class bkCombo;
	class bkData;
	class bkImageViewer;
	class bkList;
	class bkMatrix33;
	class bkMatrix34;
	class bkMatrix44;
	class bkSlider;
	class bkText;
	class bkTitle;
	class bkToggle;
	class bkVector2;
	class bkVector3;
	class bkVector4;
}


#if __BANK

class CBankDebug
{
public:
	CBankDebug(); 
	~CBankDebug();

	void InitWidgets(bkBank& bank);

	// Angle
	void OnAngleReadOnlyChanged();

	// Button
	void OnButtonPress();
	void OnButtonReadOnlyChanged();

	// Color
	void OnColorValueChanged();
	void OnColorReadOnlyChanged();

	// Combo
	void OnComboReadOnlyChanged();

	// Data
	void OnDataBufferChanged();
	void OnDataReadOnlyChanged();

	// Matrix
	void OnMatrix33ValueChanged();
	void OnMatrix34ValueChanged();
	void OnMatrix44ValueChanged();
	void OnMatrixReadOnlyChanged();

	// Slider
	void OnSliderReadOnlyChanged();

	// Toggle
	void OnToggleReadOnlyChanged();

	// Vector
	void OnVector2ValueChanged();
	void OnVector3ValueChanged();
	void OnVector4ValueChanged();
	void OnVectorReadOnlyChanged();

private:
	void AddAngleWidgets(bkBank& bank);
	void AddButtonWidgets(bkBank& bank);
	void AddColorWidgets(bkBank& bank);
	void AddComboWidgets(bkBank& bank);
	void AddDataWidgets(bkBank& bank);
	void AddImageViewerWidgets(bkBank& bank);
	void AddListWidgets(bkBank& bank);
	void AddMatrixWidgets(bkBank& bank);
	void AddSliderWidgets(bkBank& bank);
	void AddTextWidgets(bkBank& bank);
	void AddTitleWidgets(bkBank& bank);
	void AddToggleWidgets(bkBank& bank);
	void AddVectorWidgets(bkBank& bank);

	// Angle variables
	float m_angleDegrees;
	float m_angleFractions;
	float m_angleRadians;
	Vector2 m_vec2Angle;
	bkAngle* m_pDegreesAngleWidget;
	bkAngle* m_pFractionAngleWidget;
	bkAngle* m_pRadiansAngleWidget;
	bkAngle* m_pVec2AngleWidget;
	bool m_anglesEnabled;

	// Button variables
	int m_buttonPressCount;
	bool m_buttonEnabled;
	bkButton* m_pButtonWidget;

	// Color variables
	Vector3 m_vec3Color;
	Vector4 m_vec4Color;
	Color32 m_color32;
	float m_floatColorArray[4];
	bkColor* m_pColor3Widget;
	bkColor* m_pColor4Widget;
	bkColor* m_pColor32Widget;
	bkColor* m_pColorArrayWidget;
	bool m_colorEnabled;
	int m_colorChangeCount;

	// Combo variables
	int m_comboSelection;
	bkCombo* m_pComboWidget;
	bool m_comboEnabled;

	// Data variables
	#define DATA_WIDGET_SIZE 256
	u8 m_dataWidgetBuffer[DATA_WIDGET_SIZE]; 
	bkData* m_pDataWidget;
	bool m_dataEnabled;
	int m_dataChangeCount;

	// Matrix variables
	Matrix33 m_matrix33;
	Matrix34 m_matrix34;
	Matrix44 m_matrix44;
	bkMatrix33* m_pMatrix33Widget;
	bkMatrix34* m_pMatrix34Widget;
	bkMatrix44* m_pMatrix44Widget;
	bool m_matricesEnabled;
	int m_matrixChangeCount;

	// Slider variables
	float m_floatSlider;
	unsigned char m_u8Slider;
	signed char m_s8Slider;
	unsigned short m_u16Slider;
	short m_s16Slider;
	unsigned int m_u32Slider;
	int m_s32Slider;
	bkSlider* m_pFloatSliderWidget;
	bkSlider* m_pU8SliderWidget;
	bkSlider* m_pS8SliderWidget;
	bkSlider* m_pU16SliderWidget;
	bkSlider* m_pS16SliderWidget;
	bkSlider* m_pU32SliderWidget;
	bkSlider* m_pS32SliderWidget;
	bool m_slidersEnabled;

	// Toggle variables
	bool m_boolToggle;
	int m_intToggle;
	float m_floatToggle;
	unsigned m_u32Toggle;
	unsigned short m_u16Toggle;
	unsigned char m_u8Toggle;
	atBitSet m_bitSetToggle;

	bkToggle* m_pBoolToggleWidget;
	bkToggle* m_pIntegerToggleWidget;
	bkToggle* m_pFloatToggleWidget;
	bkToggle* m_pU32ToggleWidget;
	bkToggle* m_pU16ToggleWidget;
	bkToggle* m_pU8ToggleWidget;
	bkToggle* m_pBitsetToggleWidget;
	bool m_togglesEnabled;

	// Vector variables
	Vector2 m_vector2;
	Vector3 m_vector3;
	Vector4 m_vector4;
	bkVector2* m_pVector2Widget;
	bkVector3* m_pVector3Widget;
	bkVector4* m_pVector4Widget;
	bool m_vectorsEnabled;
	int m_vectorChangeCount;
};

// --- Globals ------------------------------------------------------------------

#endif // __BANK

#endif //INC_BANK_DEBUG_H_


