/////////////////////////////////////////////////////////////////////////////////
// FILE :		EditorWidget.h
// PURPOSE :	An widget to send manipulation messages to __BANK editors
// AUTHOR :		Jason Jurecka.
// CREATED :	6/6/2012
/////////////////////////////////////////////////////////////////////////////////
#ifndef INC_EDITORWIDGET_H_
#define INC_EDITORWIDGET_H_

#if __BANK
///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////
//rage
#include "atl/delegate.h"
#include "system/bit.h"
#include "vector/color32.h"
#include "vectormath/mat34v.h"
#include "vectormath/vec3v.h"

//framework

//game

///////////////////////////////////////////////////////////////////////////////
class CEditorWidget
{
public:
	CEditorWidget  ();
	virtual	~CEditorWidget (){}

	enum eOperationFlags
	{
		TRANSLATE_X	= BIT1,
		TRANSLATE_Y	= BIT2,
		TRANSLATE_Z	= BIT3,
		ROTATE_X	= BIT4,
		ROTATE_Y	= BIT5,
		ROTATE_Z	= BIT6,
		SCALE_X		= BIT7,
		SCALE_Y		= BIT8,
		SCALE_Z		= BIT9,

		TRANSLATE_ALL = TRANSLATE_X | TRANSLATE_Y | TRANSLATE_Z,
		ROTATE_ALL = ROTATE_X | ROTATE_Y | ROTATE_Z,
		SCALE_ALL = SCALE_X | SCALE_Y | SCALE_Z,

		ALL = TRANSLATE_ALL | ROTATE_ALL | SCALE_ALL,
	};

	//This is the event passed to any delegate functions ... 
	struct EditorWidgetEvent
	{
		EditorWidgetEvent() : operationFlags(0), delta(V_ZERO) {}

		friend class CEditorWidget;
		
		//Translate
		inline bool TranslateX() const { return (operationFlags & TRANSLATE_X) ? true : false; }
		inline bool TranslateY() const { return (operationFlags & TRANSLATE_Y) ? true : false; }
		inline bool TranslateZ() const { return (operationFlags & TRANSLATE_Z) ? true : false; }
		inline bool IsTranslate() const { return (TranslateX() || TranslateY() || TranslateZ()) ? true : false; }

		//Rotate
		inline bool RotateX() const { return (operationFlags & ROTATE_X) ? true : false; }
		inline bool RotateY() const { return (operationFlags & ROTATE_Y) ? true : false; }
		inline bool RotateZ() const { return (operationFlags & ROTATE_Z) ? true : false; }
		inline bool IsRotate() const { return (RotateX() || RotateY() || RotateZ()) ? true : false; }

		//Scale
		inline bool ScaleX() const { return (operationFlags & SCALE_X) ? true : false; }
		inline bool ScaleY() const { return (operationFlags & SCALE_Y) ? true : false; }
		inline bool ScaleZ() const { return (operationFlags & SCALE_Z) ? true : false; }
		inline bool IsScale() const { return (ScaleX() || ScaleY() || ScaleZ()) ? true : false; }
		
		inline ScalarV_Out GetDelta () const { return delta; }
	private:
		u32 operationFlags;
		ScalarV delta;
	};

	typedef atDelegator<void (const EditorWidgetEvent&)> Delegator;
	typedef Delegator::Delegate Delegate;

	void		Init			(u32 supportFlags);
	void        Update          (Mat34V &matrix);
	void        Render          () const;
	void        RenderOptions   () const;
	void        Reset           ();
	bool        WantHoverFocus  () const;
	Vec3V_Out   GetPosition     () const;
	bool        InTranslateMode () const { return m_isTranslation; }
	bool        InScaleMode     () const { return m_isScale; }
	bool        InRotateMode    () const { return m_isRotation; }

	Delegator m_Delegator; //Add delegates here for messages
private:
	
	void LineTranslate(Mat34V_In startMat, Vec3V_In lineEnd, Vec3V_In coneStart, Color32 color) const;
	void LineScale(Vec3V_In position, Mat34V_In matrix, Color32 color) const;
	void Circle(Vec3V_In position, Color32 color, Vec3V_In tx, Vec3V_In tz) const;
	
	void UpdateAxis();
	bool DoesRayIntersectAxis(u8 axisIndex, Vec3V_In startPoint, Vec3V_In endPoint, Vec3V_InOut hitPoint, bool infinateCyl=false) const;
	bool AxisActive(u8 axisIndex) const;

	bool m_NeedOptionsDisplay;
	bool m_isTranslation;
	bool m_isScale;
	bool m_isRotation;
	mutable bool m_HoverT; //make this mutable because the hover test is happening in OnScreenOutput::AddLine
	mutable bool m_HoverR; //make this mutable because the hover test is happening in OnScreenOutput::AddLine
	mutable bool m_HoverS; //make this mutable because the hover test is happening in OnScreenOutput::AddLine
	Vec2V m_prevMouse;
	Mat34V m_Matrix;
	Mat34V m_Axis[3];// X=0, Y=1, Z=2
	u8 m_HoveredAxis;// the axis that mouse is hovered on
	EditorWidgetEvent m_SupportFlags;//Default is eOperationFlags::ALL
};

#endif // __BANK
#endif // INC_EDITORWIDGET_H_