#include "AnimSceneScrollbar.h"

//-------- Constructors ------
CAnimSceneScrollbar::CAnimSceneScrollbar()
{
	Init(0.f,100.f,0.f,Color32(0.2f,0.2f,0.2f,1.f), Color32(0.8f,0.8f,0.8f,1.0f), Orientation::ORIENTATION_HORIZONTAL);
}

CAnimSceneScrollbar::CAnimSceneScrollbar(float minValue, float maxValue, float startValue, Color32 backgroundColour, Color32 textColour)
{
	Init(minValue,maxValue,startValue,backgroundColour,textColour,Orientation::ORIENTATION_HORIZONTAL);
}


//--------- Rendering ------
void CAnimSceneScrollbar::Render()
{
	if(m_hidden)
	{
		return;
	}
	animAssertf(m_maxValue >= m_minValue, "Max value (%f) is less than min value (%f) in scrollbar range",m_maxValue, m_minValue);

	//Make sure to update the bar properties in case something other than the mouse input changed its location or values.
	ComputeBarProperties();

	if(m_barOrientation == ORIENTATION_HORIZONTAL )
	{
		RenderHorizontal();
	}
	else
	{
		RenderVertical();
	}	
}


void CAnimSceneScrollbar::RenderHorizontal()
{
	//-----Draw a line where the slider/scroll bar will go
	Color32 scrollbarLineColour = m_darkBackground;
	scrollbarLineColour.SetAlpha(128);
	grcColor(scrollbarLineColour);
	float yMin = m_screenTopLeftExtents.y;
	float yMax = m_screenBottomLeftExtents.y;
	float xMin = m_screenBottomLeftExtents.x;
	float xMax = m_screenBottomRightExtents.x;

	grcBegin(drawLines,4);
	grcVertex2f(xMin,(yMax + yMin) / 2.f);
	grcVertex2f(m_barMinX,(yMax + yMin) / 2.f);

	grcVertex2f(xMax,(yMax + yMin) / 2.f);
	grcVertex2f(m_barMaxX,(yMax + yMin) / 2.f);
	grcEnd();

	//If we are dragging, render the bar highlighted
	if(m_dragging)
	{
		grcColor(m_darkBackground);
	}
	else if(m_highlighted)
	{
		grcColor(m_highlightBackground);
	}
	else
	{
		grcColor(m_defaultBackground);
	}

	//-----Draw the background of the main bar
	//Draw triangles as quads aren't supported on pc
	grcBegin(drawTris,6);
	grcVertex2f(m_barMinX, m_screenTopLeftExtents.y);
	grcVertex2f(m_barMinX, m_screenBottomLeftExtents.y);
	grcVertex2f(m_barMaxX, m_screenBottomRightExtents.y);

	grcVertex2f(m_barMinX, m_screenTopLeftExtents.y);
	grcVertex2f(m_barMaxX, m_screenBottomRightExtents.y);
	grcVertex2f(m_barMaxX, m_screenTopRightExtents.y);
	grcEnd();

	//====Draw arrows at the scrollbar ends
	if(m_hoveringMinArrow)
	{
		grcColor(m_highlightBackground);
	}
	else
	{
		grcColor(m_defaultBackground);
	}

	//Render the min arrow box
	grcBegin(drawTris,6);
	grcVertex2f(m_minArrowMinX, m_screenTopLeftExtents.y);
	grcVertex2f(m_minArrowMinX, m_screenBottomLeftExtents.y);
	grcVertex2f(m_minArrowMaxX, m_screenBottomLeftExtents.y);

	grcVertex2f(m_minArrowMinX, m_screenTopLeftExtents.y);
	grcVertex2f(m_minArrowMaxX, m_screenBottomLeftExtents.y);
	grcVertex2f(m_minArrowMaxX, m_screenTopLeftExtents.y);
	grcEnd();

	//Handle clicking on max arrow
	if(m_hoveringMaxArrow)
	{
		grcColor(m_highlightBackground);
	}
	else
	{
		grcColor(m_defaultBackground);
	}

	//Render the max arrow
	grcBegin(drawTris,6);
	grcVertex2f(m_maxArrowMinX,  m_screenTopLeftExtents.y);
	grcVertex2f(m_maxArrowMinX,  m_screenBottomLeftExtents.y);
	grcVertex2f(m_maxArrowMaxX,  m_screenBottomLeftExtents.y);

	grcVertex2f(m_maxArrowMinX,  m_screenTopLeftExtents.y);
	grcVertex2f(m_maxArrowMaxX,  m_screenBottomLeftExtents.y);
	grcVertex2f(m_maxArrowMaxX,  m_screenTopLeftExtents.y);
	grcEnd();

	//Draw an arrow on each end
	grcColor(m_defaultForeground);
	grcBegin(drawTris,6);
	grcVertex2f(m_maxArrowMinX + 1.0f, yMin + 1.0f);
	grcVertex2f(m_maxArrowMinX + 1.0f, yMax - 1.0f);
	grcVertex2f(m_maxArrowMaxX, (yMax + yMin) / 2.0f);

	grcVertex2f(m_minArrowMinX, (yMax + yMin) / 2.0f);
	grcVertex2f(m_minArrowMaxX - 1.0f, yMax - 1.f);
	grcVertex2f(m_minArrowMaxX - 1.0f, yMin + 1.f);
	grcEnd();		

	//Draw some 'grab' lines in the centre of the control
	grcColor(m_defaultForeground);
	float xMid = (m_barMaxX + m_barMinX) / 2.0f;
	for(float i = -5.f; i <= 5.1f; i+=5.0f)
	{
		grcBegin(drawTris,6);
		grcVertex2f(xMid + i - 1.f, yMin + 2.f);
		grcVertex2f(xMid + i - 1.f, yMax - 2.f);
		grcVertex2f(xMid + i + 1.f, yMax - 2.f);

		grcVertex2f(xMid + i - 1.f, yMin + 2.f);
		grcVertex2f(xMid + i + 1.f, yMax - 2.f);
		grcVertex2f(xMid + i + 1.f, yMin + 2.f);
		grcEnd();
	}		
}


void CAnimSceneScrollbar::RenderVertical()
{
	//-----Draw a line where the slider/scroll bar will go
	Color32 scrollbarLineColour = m_darkBackground;
	scrollbarLineColour.SetAlpha(128);
	grcColor(scrollbarLineColour);
	float yMin = m_screenTopLeftExtents.y;
	float yMax = m_screenBottomLeftExtents.y;
	float xMin = m_screenBottomLeftExtents.x;
	float xMax = m_screenBottomRightExtents.x;

	float xCentre = (m_barMinX + m_barMaxX )/ 2.f;

	grcBegin(drawLines,4);
	grcVertex2f(xCentre,yMin);
	grcVertex2f(xCentre,m_barMinY);

	grcVertex2f(xCentre,m_barMaxY);
	grcVertex2f(xCentre, yMax );
	grcEnd();

	//Handle hover and dragging special colours
	if(m_dragging)
	{
		grcColor(m_darkBackground);
	}
	else if(m_highlighted)
	{
		grcColor(m_highlightBackground);
	}
	else
	{
		grcColor(m_defaultBackground);
	}

	//-----Draw the background of the main bar
	//Draw triangles as quads aren't supported on pc
	grcBegin(drawTris,6);
	grcVertex2f(m_barMinX, m_barMinY);
	grcVertex2f(m_barMinX, m_barMaxY);
	grcVertex2f(m_barMaxX, m_barMaxY);

	grcVertex2f(m_barMinX, m_barMinY);
	grcVertex2f(m_barMaxX, m_barMaxY);
	grcVertex2f(m_barMaxX, m_barMinY);
	grcEnd();

	//====Draw arrows at the scrollbar ends
	if(m_hoveringMinArrow)
	{
		grcColor(m_highlightBackground);
	}
	else
	{
		grcColor(m_defaultBackground);
	}

	//Render the min arrow box
	grcBegin(drawTris,6);
	grcVertex2f(m_barMinX, m_minArrowMinY);
	grcVertex2f(m_barMinX, m_minArrowMaxY);
	grcVertex2f(m_barMaxX, m_minArrowMaxY);

	grcVertex2f(m_barMinX, m_minArrowMinY);
	grcVertex2f(m_barMaxX, m_minArrowMaxY);
	grcVertex2f(m_barMaxX, m_minArrowMinY);
	grcEnd();

	//Handle clicking on max arrow
	if(m_hoveringMaxArrow)
	{
		grcColor(m_highlightBackground);
	}
	else
	{
		grcColor(m_defaultBackground);
	}

	//Render the max arrow
	grcBegin(drawTris,6);
	grcVertex2f(m_barMinX,  m_maxArrowMinY);
	grcVertex2f(m_barMinX,  m_maxArrowMaxY);
	grcVertex2f(m_barMaxX,  m_maxArrowMaxY);

	grcVertex2f(m_barMinX,  m_maxArrowMinY);
	grcVertex2f(m_barMaxX,  m_maxArrowMaxY);
	grcVertex2f(m_barMaxX,  m_maxArrowMinY);
	grcEnd();

	//Draw an arrow on each end
	grcColor(m_defaultForeground);
	grcBegin(drawTris,6);
	grcVertex2f(xMin + 1.0f, m_maxArrowMinY  + 1.0f);
	grcVertex2f(xMax - 1.0f, m_maxArrowMinY  + 1.0f);
	grcVertex2f((xMax + xMin) / 2.0f, m_maxArrowMaxY);

	grcVertex2f((xMax + xMin) / 2.0f, m_minArrowMinY);
	grcVertex2f(xMax - 1.f, m_minArrowMaxY - 1.0f);
	grcVertex2f(xMin + 1.f, m_minArrowMaxY - 1.0f);
	grcEnd();		

	//Draw some 'grab' lines in the centre of the control
	grcColor(m_defaultForeground);
	float yMid = (m_barMaxY + m_barMinY) / 2.0f;
	for(float i = -5.f; i <= 5.1f; i+=5.0f)
	{
		grcBegin(drawTris,6);
		grcVertex2f( xMin + 2.f,yMid + i - 1.f);
		grcVertex2f( xMax - 2.f,yMid + i - 1.f);
		grcVertex2f( xMax - 2.f,yMid + i + 1.f);

		grcVertex2f( xMin + 2.f,yMid + i - 1.f);
		grcVertex2f( xMax - 2.f,yMid + i + 1.f);
		grcVertex2f( xMin + 2.f,yMid + i + 1.f);
		grcEnd();
	}		
}


//-------- Input ------

void CAnimSceneScrollbar::ProcessMouseInput()
{
	if(m_hidden)
	{
		return;
	}
	animAssertf(m_maxValue >= m_minValue, "Max value (%f) is less than min value (%f) in scrollbar range",m_maxValue, m_minValue);

	float mouseX = static_cast<float>(ioMouse::GetX());
	float mouseY = static_cast<float>(ioMouse::GetY());

	float mouseDX = mouseX - m_mousePrevX;
	float mouseDY = mouseY - m_mousePrevY;
	//MousePrev is only used for deltas
	m_mousePrevX = mouseX;
	m_mousePrevY = mouseY;
	//true whenever the left mouse is down
	bool leftMouseDown = ioMouse::GetButtons() & ioMouse::MOUSE_LEFT;
	//The initial click of the mouse (mousedown)
	bool leftMouseClicked = ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT;
	float maxBarValue = Clamp(1.0f - m_barSize , 0.0f, 1.0f);

	//early out if bar cannot be moved
	if(m_barSize == 1.0f)
	{
		return;
	}

	//compute bounds of bar so we can ignore anything outside
	ComputeBarProperties();
	m_highlighted = false;
	m_hovering = false;
	m_hoveringMinArrow = false;
	m_hoveringMaxArrow = false;

	if(m_dragging)
	{
		m_highlighted = true;
		m_hovering = false;

		if(!leftMouseDown)
		{
			m_dragging = false;
		}
		else //left mouse still down
		{
			if(m_barOrientation == ORIENTATION_HORIZONTAL)
			{
				//---move by dx
				//change in normalised value
				float dx = (mouseDX / m_pixelLength );

				m_normalisedValue = Clamp( m_normalisedValue + dx, 0.f, maxBarValue);
			}
			else //ORIENTATION_VERTICAL
			{
				//----move by dy
				//change in normalised value
				float dy = (mouseDY / m_pixelLength );;

				m_normalisedValue = Clamp( m_normalisedValue + dy, 0.f, maxBarValue);
			}
		}
	}
	else if ( MouseXInRange(m_screenBottomLeftExtents.x, m_screenBottomRightExtents.x) && 
		MouseYInRange(m_screenTopRightExtents.y, m_screenBottomRightExtents.y) )
	{
		//The mouse is in the overall bar range, but may not be in the range of the actual movement bar you can drag
		bool mouseOnBar = false;
		if(m_barOrientation == ORIENTATION_HORIZONTAL)
		{
			//check whether we are in bounds of actual bar
			mouseOnBar = MouseXInRange(m_barMinX,m_barMaxX);
		}
		else //VERTICAL
		{
			//check whether we are in bounds of actual bar
			mouseOnBar = MouseYInRange(m_barMinY,m_barMaxY);
		}

		//handle setting up dragging if we are not currently
		if(mouseOnBar)
		{
			m_highlighted = true;
			m_hovering = true;
			if(leftMouseClicked)
			{
				m_dragging = true;
			}
			//handle scroll wheel
			else if(ioMouse::GetDZ() > 0)
			{
				ScrollPositive();
			}
			else if(ioMouse::GetDZ() < 0)
			{
				ScrollNegative();
			}

		}
	}
	else if (MouseXInRange(m_minArrowMinX, m_minArrowMaxX) && 
		MouseYInRange(m_minArrowMinY, m_minArrowMaxY) ) //Over the minimum arrow
	{
		m_hoveringMinArrow = true;
		if(leftMouseDown)//as we have already checked for dragging behaviour, can allow for continued scrolling if the mouse is down
		{
			ScrollNegative();
		}
	}
	else if (MouseXInRange(m_maxArrowMinX, m_maxArrowMaxX) && 
		MouseYInRange(m_maxArrowMinY, m_maxArrowMaxY) ) //Over the maximum arrow
	{
		m_hoveringMaxArrow = true;
		if(leftMouseDown)//as we have already checked for dragging behaviour, can allow for continued scrolling if the mouse is down
		{
			ScrollPositive();
		}
	}
	else
	{
		//In this case we aren't dragging and aren't in the area of the bar
	}
}


void CAnimSceneScrollbar::Init(float minValue, 
		  float maxValue, 
		  float startValue, 
		  Color32 backgroundColour, 
		  Color32 foregroundColour,
		  Orientation orientation)
{
	animAssertf(minValue < maxValue,"Min value (%f) must be less than max value (%f)",minValue,maxValue);
	m_minValue = minValue;
	m_maxValue = maxValue;
	if(m_minValue == m_maxValue)
	{
		++m_maxValue;
	}

	//Set the colours for use internally
	m_defaultBackground = backgroundColour;
	m_defaultForeground = foregroundColour;

	m_highlightBackground = m_defaultBackground;
	BrightenColour(m_highlightBackground);
	m_highlightForeground = m_defaultForeground;
	BrightenColour(m_highlightForeground);
	m_darkBackground = m_defaultBackground;
	BrightenColour(m_darkBackground,-50);

	//Normalise start value
	SetValue(startValue);

	m_barOrientation = orientation;
	m_barSize = 1.f;
	m_hovering = false;
	m_pixelLength = 100.f;
	m_baseSize = 10.f;
	m_dragging = false;
	m_highlighted = false;
	m_mousePrevX = 0.f;
	m_mousePrevY = 0.f;

	m_hidden = false;

	m_increment = 0.05f;
}

//-----------------Setters, getters and helpers ---------------------------

void CAnimSceneScrollbar::SetValue(float val)
{
	animAssertf((val - m_minValue) / (m_maxValue - m_minValue) >= -SMALL_FLOAT &&  (val - m_minValue) / (m_maxValue - m_minValue) <= 1.f+SMALL_FLOAT,
		"Value was out of scrollbar range");
	m_normalisedValue = Clamp( (val - m_minValue) / (m_maxValue - m_minValue), 0.f, 1.0f );
}

void CAnimSceneScrollbar::BrightenColour(Color32 &col, int amount /*= 50*/)
{
	col.SetRed( Clamp(col.GetRed() + amount, 0, 255) );
	col.SetGreen( Clamp(col.GetGreen() + amount, 0, 255) );
	col.SetBlue( Clamp(col.GetBlue() + amount, 0, 255) );
}

bool CAnimSceneScrollbar::MouseXInRange(float minX, float maxX)
{
	float mouseX = static_cast<float>(ioMouse::GetX());
	return ( mouseX >= minX && mouseX <= maxX );
}

bool CAnimSceneScrollbar::MouseYInRange(float minY, float maxY)
{
	float mouseY = static_cast<float>(ioMouse::GetY());
	return ( mouseY >= minY && mouseY <= maxY );
}

void CAnimSceneScrollbar::ScrollNegative()
{
	m_normalisedValue = Clamp(m_normalisedValue - m_increment, 0.f,1.f - m_barSize);
}

void CAnimSceneScrollbar::ScrollPositive()
{
	m_normalisedValue = Clamp(m_normalisedValue + m_increment, 0.f,1.f - m_barSize);
}

void CAnimSceneScrollbar::ScrollNegativeAmount(float val)
{
	float percChange = ( val / (m_maxValue-m_minValue) );
	m_normalisedValue = Clamp(m_normalisedValue - percChange, 0.f,1.f - m_barSize);
}

void CAnimSceneScrollbar::ScrollPositiveAmount(float val)
{
	float percChange = ( val / (m_maxValue-m_minValue) );
	m_normalisedValue = Clamp(m_normalisedValue + percChange, 0.f,1.f - m_barSize);
}


void CAnimSceneScrollbar::SetForegroundColour(const Color32 colour)
{
	m_defaultForeground = colour;
	m_highlightForeground = m_defaultForeground;
	BrightenColour(m_highlightForeground);
}

void CAnimSceneScrollbar::SetBackgroundColour(const Color32 colour)
{
	m_defaultBackground = colour;
	m_highlightBackground = m_defaultBackground;
	BrightenColour(m_highlightBackground);
	m_darkBackground = m_defaultBackground;
	BrightenColour(m_darkBackground,-50);
}

bool CAnimSceneScrollbar::IsBarHidden() const
{
	return m_hidden;
}

void CAnimSceneScrollbar::SetMinValue(float val)
{
	m_minValue = val;
}

void CAnimSceneScrollbar::SetMaxValue(float val)
{
	m_maxValue = val;
}

bool CAnimSceneScrollbar::IsDragging() const
{
	return m_dragging;
}

void CAnimSceneScrollbar::SetBarHidden(bool val)
{
	m_hidden = val;
	if(m_hidden)
	{
		m_dragging = false;
	}
}

float CAnimSceneScrollbar::GetValue() const
{
	return ( m_normalisedValue * (m_maxValue - m_minValue) ) + m_minValue;
}

void CAnimSceneScrollbar::SetBarSizeFromVisibleArea(float visibleArea, float totalArea)
{
	SetBarSize(visibleArea/totalArea);
}

void CAnimSceneScrollbar::SetTopLeft(const Vector2 &topLeft)
{
	m_screenTopLeftExtents = topLeft;
}

void CAnimSceneScrollbar::GetBarRange(float &low, float &high) const
{
	low = GetValue();
	high =  ( (m_normalisedValue + m_barSize) * (m_maxValue - m_minValue) ) + m_minValue;
}

void CAnimSceneScrollbar::ComputeBarProperties()
{
	if(m_barOrientation == ORIENTATION_HORIZONTAL)
	{
		//Bar extremes
		m_screenTopRightExtents = m_screenTopLeftExtents;
		m_screenTopRightExtents.Add(Vector2(m_pixelLength,0.f));

		m_screenBottomLeftExtents = m_screenTopLeftExtents;
		m_screenBottomLeftExtents.Add(Vector2( 0.f, m_baseSize ) );

		m_screenBottomRightExtents = m_screenTopRightExtents;
		m_screenBottomRightExtents.Add( Vector2(0.f, m_baseSize) );

		//Actual bar
		m_barMinX = m_normalisedValue * m_pixelLength + m_screenTopLeftExtents.x;
		m_barMaxX = m_barMinX +  (m_barSize * m_pixelLength );

		m_barMinY = m_screenTopRightExtents.y;
		m_barMaxY = m_screenBottomRightExtents.y;

		//End Arrows
		m_minArrowMinX = m_screenTopLeftExtents.x - m_baseSize - 1.0f;
		m_minArrowMaxX = m_minArrowMinX + m_baseSize;
		m_maxArrowMinX = m_screenBottomRightExtents.x  + 1.0f;
		m_maxArrowMaxX = m_maxArrowMinX + m_baseSize;

		m_minArrowMaxY = m_barMaxY;
		m_minArrowMinY = m_barMinY;
		m_maxArrowMaxY = m_barMaxY;
		m_maxArrowMinY = m_barMinY;
	}
	else if(m_barOrientation == ORIENTATION_VERTICAL)
	{
		//Bar extremes
		m_screenTopRightExtents = m_screenTopLeftExtents;
		m_screenTopRightExtents.Add( Vector2( m_baseSize, 0.f ) );

		m_screenBottomLeftExtents = m_screenTopLeftExtents;
		m_screenBottomLeftExtents.Add( Vector2( 0.f, m_pixelLength  ) );

		m_screenBottomRightExtents = m_screenTopRightExtents;
		m_screenBottomRightExtents.Add( Vector2( 0.f, m_pixelLength ) );

		//Actual bar
		m_barMinX = m_screenBottomLeftExtents.x;
		m_barMaxX = m_screenBottomRightExtents.x;

		m_barMinY = m_normalisedValue * m_pixelLength + m_screenTopLeftExtents.y;
		m_barMaxY = m_barMinY +  ( m_barSize * m_pixelLength );

		//End Arrows
		m_minArrowMinY = m_screenTopLeftExtents.y - m_baseSize - 1.0f;
		m_minArrowMaxY = m_minArrowMinY + m_baseSize;
		m_maxArrowMinY = m_screenBottomRightExtents.y  + 1.0f;
		m_maxArrowMaxY = m_maxArrowMinY + m_baseSize;

		m_minArrowMaxX = m_barMaxX;
		m_minArrowMinX = m_barMinX;
		m_maxArrowMaxX = m_barMaxX;
		m_maxArrowMinX = m_barMinX;
	}

}





