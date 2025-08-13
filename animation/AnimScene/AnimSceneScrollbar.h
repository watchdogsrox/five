#ifndef _ANIM_SCENE_SCROLLBAR_H_
#define _ANIM_SCENE_SCROLLBAR_H_

#include "animation/debug/AnimDebug.h"
#include "input/mouse.h"
#include "vector/color32.h"

/************************************************************************/
/* This is a class that encapsulates the scrollbar used in anim scenes, so they can be reused.

It handles the state, the mouse input and the rendering, but focus must be handled
externally (e.g. you must check if 'IsDragging()' here to ensure you don't select in your UI while we are
dragging the scroll bar).

Example Usage:

//Setup
CAnimSceneScrollbar scrollbar();
scrollbar.SetBackgroundColour(animSceneEditor->GetColor(CAnimSceneEditor::kScrollBarBackgroundColour));
scrollbar.SetForegroundColour(animSceneEditor->GetColor(CAnimSceneEditor::kMainTextColor));
scrollbar.SetOrientation(CAnimSceneScrollbar::ORIENTATION_HORIZONTAL);
//Set the position on screen by the top left point of the scrollbar
scrollbar.SetTopLeft(Vector2(m_screenExtents.GetMinX(),m_screenExtents.GetMaxY()));
scrollbar.SetLength(200.0f);
scrollbar.SetMinValue(0.f);
scrollbar.SetMaxValue(100.f);
scrollbar.SetValue(20.f);
scrollbar.SetBarSize(0.4f);//The scroll bar itself is 40% of the total width of the scroll bar region

//Handle input
//if(canFocusOnScrollbar)
scrollbar.ProcessMouseInput();

//Render
scrollbar.Render();

//Get current scrollbar value
float val = scrollbar.GetValue();

//val can range between 0 and (the maximum value - (bar size * range))
//which in this case is 0-60 as the bar is 40% of the size of the area.

//This needs to be taken into consideration if used as a value selector instead of a scrollbar for a region
//For a region, it can just take the maximum and minimum extents of the region and will scroll between them
//by setting the return value as an offset (as is used in AnimSceneEditor.cpp).
*/
/************************************************************************/
class CAnimSceneScrollbar{

public:
	enum Orientation{
		ORIENTATION_HORIZONTAL,
		ORIENTATION_VERTICAL
	};

	CAnimSceneScrollbar();

	CAnimSceneScrollbar(float minValue, float maxValue, float startValue, Color32 backgroundColour, Color32 textColour);

	void Init(float minValue, 
		float maxValue, 
		float startValue, 
		Color32 backgroundColour, 
		Color32 textColour,
		CAnimSceneScrollbar::Orientation orientation);

	void Render();

	//This function detects the mouse and handles any input.
	//It assumes that we have focus, if this isn't the case then
	//ProcessMouseInput should not be called.
	void ProcessMouseInput();

	void SetValue(float val);

	//Get the current (non-normalised) value of the bar location (in the range of the min value to the max value)
	float GetValue() const;	

	//Sets the length of the whole bar extents
	void SetLength(float pixels)
	{
		m_pixelLength = pixels;
	}

	//Sets the actual draggable bar size between 0 and 1
	void SetBarSize(float barSize)
	{
		m_barSize = Clamp(barSize, 0.f,1.f);
	}

	//Sets the scroll increment when clicking on the arrows.
	//In non-normalised units, based on the current range
	void SetDiscreteScrollIncrement(float val)
	{
		m_increment = (val - m_minValue) / m_maxValue;
		if(m_increment < 0.f || m_increment > 1.f)
		{
			animAssertf(0,"Incorrect increment passed (resulted in %f percentage change per click).",m_increment);
			m_increment = 0.05f;
		}
	}

	//Gets the range the bar covers in the overall range of the scroll area
	void GetBarRange(float &low, float &high) const;

	//The actual bar size can be set from the amount of area that is visible compared to the total area of the
	//object the scrollbar controls
	void SetBarSizeFromVisibleArea(float visibleArea, float totalArea);

	void SetTopLeft(const Vector2 &topLeft);

	//Scroll (discrete) towards the negative
	void ScrollNegative();

	//Scroll (discrete) towards the positive
	void ScrollPositive();

	//Scroll (amount) towards the negative
	void ScrollNegativeAmount(float val);

	//Scroll (amount) towards the positive
	void ScrollPositiveAmount(float val);

	void SetOrientation(Orientation orient)
	{
		m_barOrientation = orient;
	}

	void SetBackgroundColour(const Color32 colour);
	void SetForegroundColour(const Color32 colour);

	void SetBarHidden(bool val);
	bool IsBarHidden() const;
	bool IsDragging() const;

	void SetMinValue(float val);
	void SetMaxValue(float val);


	float GetMinValue()
	{
		return m_minValue;
	}

	float GetMaxValue()
	{
		return m_maxValue;
	}

protected:
	void BrightenColour(Color32 &col, int amount = 50);

	bool MouseXInRange(float minX, float maxX);
	bool MouseYInRange(float minY, float maxY);

private:
	void RenderHorizontal();
	void RenderVertical();

	void ComputeBarProperties();

	//The colours of the bar
	Color32 m_defaultBackground, m_highlightBackground, m_darkBackground;
	Color32 m_defaultForeground, m_highlightForeground;

	Orientation m_barOrientation;

	//The base size that affects the size of the components of the bar (width). Does not affect the length aside from the arrow boxes.
	float m_baseSize;

	//Set by the user
	Vector2 m_screenTopLeftExtents;
	//Calculated by us
	Vector2 m_screenTopRightExtents, m_screenBottomLeftExtents, m_screenBottomRightExtents;

	//PURPOSE: Stores the value from 0.0 to 1.0 of where the scrollbar is in its range.
	//At zero, it is fully 'left' (horizontal) or 'up' (vertical).
	//At one, it is fully right or down
	float m_normalisedValue;

	//The absolute values that the scrollbar varies between
	float m_maxValue;
	float m_minValue;

	//The normalised size of the bar in the axis the bar moves (how much of the scrollbar area the actual bar takes up)
	float m_barSize;

	//The fixed length of the scroll bar main section (long axis), without arrow bars
	float m_pixelLength;

	//Are we dragging the bar around?
	bool m_dragging;

	//Is the bar highlighted?
	bool m_highlighted;

	//Is the mouse hovering over the bar? the minimum arrow (left or top)? the maximum arrow (right or bottom)?
	bool m_hovering, m_hoveringMinArrow, m_hoveringMaxArrow;

	//The clickable bar extents
	float m_barMinX, m_barMaxX, m_barMinY, m_barMaxY;

	//The previous mouse location
	float m_mousePrevX,m_mousePrevY;

	//Hide the bar
	bool m_hidden;

	//Arrows
	//min end
	float m_minArrowMinX;
	float m_minArrowMaxX;
	float m_minArrowMinY; 
	float m_minArrowMaxY;
	//max end
	float m_maxArrowMinX;
	float m_maxArrowMaxX;
	float m_maxArrowMinY; 
	float m_maxArrowMaxY;

	//Discrete increment for scrolling
	float m_increment;
};

#endif //_ANIM_SCENE_SCROLLBAR_H_