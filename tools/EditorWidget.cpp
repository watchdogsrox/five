/////////////////////////////////////////////////////////////////////////////////
// FILE :		EditorWidget.h
// PURPOSE :	An widget to send manipulation messages to __BANK editors
// AUTHOR :		Jason Jurecka.
// CREATED :	6/6/2012
/////////////////////////////////////////////////////////////////////////////////
#include "tools/EditorWidget.h"

#if __BANK
///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

//rage
#include "atl/binmap.h"
#include "grcore/debugdraw.h"
#include "input/mouse.h"
#include "vector/geometry.h"

//framework

//game
#include "camera/CamInterface.h"
#include "camera/viewports/ViewportManager.h"

///////////////////////////////////////////////////////////////////////////////
//  STATIC CODE
///////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////
CEditorWidget::CEditorWidget()
: m_NeedOptionsDisplay(true)
, m_isTranslation(true)
, m_isScale(false)
, m_isRotation(false)
, m_HoverT(false)
, m_HoverS(false)
, m_HoverR(false)
, m_HoveredAxis(0xff)
, m_prevMouse(V_ZERO)
, m_Matrix(V_IDENTITY)
{
	m_SupportFlags.operationFlags = ALL;
}

void CEditorWidget::Init (u32 supportFlags)
{
	m_SupportFlags.operationFlags = supportFlags;

	//Find valid default mode ...
	if (m_SupportFlags.IsTranslate())
	{
		m_isTranslation = true;
	}
	else if (m_SupportFlags.IsRotate())
	{
		m_isRotation = true;
	}
	else if (m_SupportFlags.IsScale())
	{
		m_isScale = true;
	}
    
    int numSupported = 0;
    if (m_SupportFlags.IsTranslate())
	{
		numSupported++;
	}
	if (m_SupportFlags.IsRotate())
	{
		numSupported++;
	}
	if (m_SupportFlags.IsScale())
	{
		numSupported++;
	}

	//If only one thing is supported then dont worry about drawing the options
	m_NeedOptionsDisplay = (numSupported <= 1)?  false : true;
}

void CEditorWidget::Update (Mat34V &matrix)
{
	if (ioMouse::GetPressedButtons() & ioMouse::MOUSE_LEFT )
	{
		if (m_HoverR)
		{
			m_isRotation = !m_isRotation;
			m_isScale = false;
			m_isTranslation = false;
		}
		else if (m_HoverS)
		{
			m_isScale = !m_isScale;
			m_isRotation = false;
			m_isTranslation = false;
		}
		else if (m_HoverT)
		{
			m_isTranslation = !m_isTranslation;
			m_isScale = false;
			m_isRotation = false;
		}
	}

	m_Matrix = matrix;

	//Update Axis volumes
	UpdateAxis();

	Vec2V currMouse((float)ioMouse::GetX(), (float)ioMouse::GetY());
	if (ioMouse::GetButtons() & ioMouse::MOUSE_LEFT)
	{
		if (!m_HoverR && !m_HoverS && !m_HoverT)
		{
			ScalarV delta = Dist(m_prevMouse, currMouse);
			if (delta.Getf() != 0.0f)
			{
				grcViewport& viewport = gVpMan.GetCurrentViewport()->GetGrcViewport();
				Vec3V startC, endC, startP, endP;
				viewport.ReverseTransform(currMouse.GetXf(), currMouse.GetYf(), startC, endC);
				viewport.ReverseTransform(m_prevMouse.GetXf(), m_prevMouse.GetYf(), startP, endP);

				Vec4V plane(V_ZERO);
				Vector3 planecheckNormal;
				ScalarV distTo0 = Dist(m_Matrix.GetCol3(), Vec3V(V_ZERO));
				Vec3V camPos = VECTOR3_TO_VEC3V(camInterface::GetPos());
				Vec3V dir = camPos - m_Matrix.GetCol3();
				dir = Normalize(dir);
				plane.SetXYZ(dir);
				plane.SetW(distTo0);

				switch(m_HoveredAxis)
				{
				case 0://X
					{
						planecheckNormal.Set(VEC3V_TO_VECTOR3(m_Matrix.GetCol0()));
					}
					break;
				case 1://Y
					{
						planecheckNormal.Set(VEC3V_TO_VECTOR3(m_Matrix.GetCol1()));
					}
					break;
				case 2://Z
					{
						planecheckNormal.Set(VEC3V_TO_VECTOR3(m_Matrix.GetCol2()));
					}
					break;
				default:
					break;
				};

				Vec3V dirC = Normalize(endC - startC);
				ScalarV tC;
				geomSegments::CollideRayPlaneNoBackfaceCullingV(startC, dirC, plane, tC);

				Vec3V dirP = Normalize(endP - startP);
				ScalarV tP;
				geomSegments::CollideRayPlaneNoBackfaceCullingV(startP, dirP, plane, tP);

				Vec3V hitC = startC + (dirC * ScalarV(tC));
				Vec3V hitP = startP + (dirP * ScalarV(tP));

				//const Vector3& point, const Vector3& planePoint, const Vector3& planeNormal
				if (geomPoints::IsPointBehindPlane(VEC3V_TO_VECTOR3(hitP), VEC3V_TO_VECTOR3(hitC), planecheckNormal))
				{
					delta = delta * ScalarV(V_NEGONE);
				}

				//send out operation events ... 
				EditorWidgetEvent event;
				switch(m_HoveredAxis)
				{
				case 0://X
					if (m_isTranslation) event.operationFlags |= TRANSLATE_X;
					if (m_isScale) event.operationFlags |= SCALE_X;
					if (m_isRotation) event.operationFlags |= ROTATE_X;
					break;
				case 1://Y
					if (m_isTranslation) event.operationFlags |= TRANSLATE_Y;
					if (m_isScale) event.operationFlags |= SCALE_Y;
					if (m_isRotation) event.operationFlags |= ROTATE_Y;
					break;
				case 2://Z
					if (m_isTranslation) event.operationFlags |= TRANSLATE_Z;
					if (m_isScale) event.operationFlags |= SCALE_Z;
					if (m_isRotation) event.operationFlags |= ROTATE_Z;
					break;
				default:
					break;
				};

				if (event.operationFlags)
				{
					event.delta = delta;
					m_Delegator.Dispatch(event);
				}
			}
		}
	}
	else
	{
		//Update hovering ... if we are not performing an operation ... 
		grcViewport& viewport = gVpMan.GetCurrentViewport()->GetGrcViewport();
		Vec3V startPoint;
		Vec3V endPoint;            
		viewport.ReverseTransform((float)ioMouse::GetX(), (float)ioMouse::GetY(), startPoint, endPoint);

		m_HoveredAxis = 0xff;
		Vec3V hitPoint(V_ZERO);
		atBinaryMap<u8, float> hoverObjects;
		if (AxisActive(0) && DoesRayIntersectAxis(0, startPoint, endPoint, hitPoint))
		{
			ScalarV dist = DistSquared(startPoint, hitPoint);
			hoverObjects.SafeInsert(dist.Getf(), 0);
		}

		if (AxisActive(1) && DoesRayIntersectAxis(1, startPoint, endPoint, hitPoint))
		{
			ScalarV dist = DistSquared(startPoint, hitPoint);
			hoverObjects.SafeInsert(dist.Getf(), 1);
		}

		if (AxisActive(2) && DoesRayIntersectAxis(2, startPoint, endPoint, hitPoint))
		{
			ScalarV dist = DistSquared(startPoint, hitPoint);
			hoverObjects.SafeInsert(dist.Getf(), 2);
		}
		hoverObjects.FinishInsertion();

		//Figure out what is hovered on ... 
		if (hoverObjects.GetCount())
		{
			//the First one should be the closest ... 
			m_HoveredAxis = *hoverObjects.GetItem(0);
		}
	}

	m_prevMouse = currMouse;
}

void CEditorWidget::RenderOptions() const
{
	if (!m_NeedOptionsDisplay)
		return;

	//////////////////////////////////////////////////////////////////////////
	// Text Drawing for mode options
#if DR_ENABLED
	int mousex = ioMouse::GetX();
	int mouseY = ioMouse::GetY();

	debugPlayback::OnScreenOutput screenSpew(10.0f);
	screenSpew.m_fMouseX = (float)mousex;
	screenSpew.m_fMouseY = (float)mouseY;
	screenSpew.m_Color.Set(200,200,200,255);
	screenSpew.m_HighlightColor.Set(255,0,0,255);

	m_HoverT = false;
	if (m_SupportFlags.IsTranslate())
	{
		if (m_isTranslation)
		{
			screenSpew.m_bDrawBackground = true;
		}
		if (m_HoverT)
		{
			screenSpew.m_bDrawBackground = true;
			screenSpew.m_bForceColor = true;
			screenSpew.m_Color = Color_yellow;
		}
		screenSpew.m_fXPosition = 300.0f;
		screenSpew.m_fYPosition = 50.0f;
		m_HoverT |= screenSpew.AddLine("-----------");
		m_HoverT |= screenSpew.AddLine("|Translate|");
		m_HoverT |= screenSpew.AddLine("-----------");

	}
	
	screenSpew.m_bDrawBackground = false;
	screenSpew.m_bForceColor = false;
	screenSpew.m_Color.Set(200,200,200,255);

	m_HoverR = false;
	if (m_SupportFlags.IsRotate())
	{
		if (m_isRotation)
		{
			screenSpew.m_bDrawBackground = true;
		}
		if (m_HoverR)
		{
			screenSpew.m_bDrawBackground = true;
			screenSpew.m_bForceColor = true;
			screenSpew.m_Color = Color_yellow;
		}
		screenSpew.m_fXPosition = 400.0f;
		screenSpew.m_fYPosition = 50.0f;
		m_HoverR |= screenSpew.AddLine("--------");
		m_HoverR |= screenSpew.AddLine("|Rotate|");
		m_HoverR |= screenSpew.AddLine("--------");
	}
	
	screenSpew.m_bDrawBackground = false;
	screenSpew.m_bForceColor = false;
	screenSpew.m_Color.Set(200,200,200,255);

	m_HoverS = false;
	if (m_SupportFlags.IsScale())
	{
		if (m_isScale)
		{
			screenSpew.m_bDrawBackground = true;
		}
		if (m_HoverS)
		{
			screenSpew.m_bDrawBackground = true;
			screenSpew.m_bForceColor = true;
			screenSpew.m_Color = Color_yellow;
		}
		screenSpew.m_fXPosition = 475.0f;
		screenSpew.m_fYPosition = 50.0f;
		m_HoverS |= screenSpew.AddLine("-------");
		m_HoverS |= screenSpew.AddLine("|Scale|");
		m_HoverS |= screenSpew.AddLine("-------");
	}
	
	screenSpew.m_bDrawBackground = false;
	screenSpew.m_bForceColor = false;
	screenSpew.m_Color.Set(200,200,200,255);

	screenSpew.m_Color.Set(100,100,100,255);
	screenSpew.m_fXPosition = 375.0f;
	screenSpew.m_fYPosition = 35.0f;
	screenSpew.AddLine("Widget Edit Mode");
#endif
	//////////////////////////////////////////////////////////////////////////
}

void CEditorWidget::Render() const
{
	//Translation
	if (m_isTranslation)
	{
		Mat34V temp(V_IDENTITY);
		Vec3V pos(V_ZERO);

		//X
		if (AxisActive(0))
		{
			pos = Multiply(m_Matrix.GetMat33(), Vec3V(V_X_AXIS_WZERO));
			temp = m_Matrix;
			temp.Setd(temp.GetCol3() + pos);
			Color32 color = (m_HoveredAxis == 0) ? Color_yellow : Color_red;
			Vec3V pos2 = temp.GetCol3() - Multiply(m_Matrix.GetMat33(), Vec3V(0.2f,0.0f,0.0f));
			LineTranslate(m_Matrix, temp.GetCol3(), pos2, color);
		}

		//Y
		if (AxisActive(1))
		{
			pos = Multiply(m_Matrix.GetMat33(), Vec3V(V_Y_AXIS_WZERO));
			temp = m_Matrix;
			temp.Setd(temp.GetCol3() + pos);
			Color32 color = (m_HoveredAxis == 1) ? Color_yellow : Color_green;
			Vec3V pos2 = temp.GetCol3() - Multiply(m_Matrix.GetMat33(), Vec3V(0.0f,0.2f,0.0f));
			LineTranslate(m_Matrix, temp.GetCol3(), pos2, color);
		}

		//Z
		if (AxisActive(2))
		{
			pos = Multiply(m_Matrix.GetMat33(), Vec3V(V_Z_AXIS_WZERO));
			temp = m_Matrix;
			temp.Setd(temp.GetCol3() + pos);
			Color32 color = (m_HoveredAxis == 2) ? Color_yellow : Color_blue;
			Vec3V pos2 = temp.GetCol3() - Multiply(m_Matrix.GetMat33(), Vec3V(0.0f,0.0f,0.2f));
			LineTranslate(m_Matrix, temp.GetCol3(), pos2, color);
		}
	}

	//Scale display
	if (m_isScale)
	{
		Mat34V temp(V_IDENTITY);
		Vec3V pos(V_ZERO);

		//X
		if (AxisActive(0))
		{
			pos = Multiply(m_Matrix.GetMat33(), Vec3V(V_X_AXIS_WZERO));
			temp = m_Matrix;
			temp.Setd(temp.GetCol3() + pos);
			Color32 color = (m_HoveredAxis == 0) ? Color_yellow : Color_red;
			LineScale(m_Matrix.GetCol3(), temp, color);
		}

		//Y
		if (AxisActive(1))
		{
			pos = Multiply(m_Matrix.GetMat33(), Vec3V(V_Y_AXIS_WZERO));
			temp = m_Matrix;
			temp.Setd(temp.GetCol3() + pos);
			Color32 color = (m_HoveredAxis == 1) ? Color_yellow : Color_green;
			LineScale(m_Matrix.GetCol3(), temp, color);
		}

		//Z
		if (AxisActive(2))
		{
			pos = Multiply(m_Matrix.GetMat33(), Vec3V(V_Z_AXIS_WZERO));
			temp = m_Matrix;
			temp.Setd(temp.GetCol3() + pos);
			Color32 color = (m_HoveredAxis == 2) ? Color_yellow : Color_blue;
			LineScale(m_Matrix.GetCol3(), temp, color);
		}
	}

	//Rotation display
	if (m_isRotation)
	{
		Vec3V pos = m_Matrix.GetCol3();
		grcDebugDraw::Axis(m_Matrix, 0.25f, false);

		// Draw the local X rotation axis red.
		Vec3V tx;
		Vec3V tz;
		if (AxisActive(0))
		{
			tx = m_Axis[0].GetCol0();
			tz = m_Axis[0].GetCol2();
			Color32 color = (m_HoveredAxis == 0) ? Color_yellow : Color_red;
			Circle(pos, color, tx, tz);
		}

		// Draw the local Y rotation axis green.
		if (AxisActive(1))
		{
			tx = m_Axis[1].GetCol0();
			tz = m_Axis[1].GetCol2();
			Color32 color = (m_HoveredAxis == 1) ? Color_yellow : Color_green;
			Circle(pos, color, tx, tz);
		}

		// Draw the local Z rotation axis blue.
		if (AxisActive(2))
		{
			tx = m_Axis[2].GetCol0();
			tz = m_Axis[2].GetCol2();
			Color32 color = (m_HoveredAxis == 2) ? Color_yellow : Color_blue;
			Circle(pos, color, tx, tz);
		}
	}
}

void CEditorWidget::Reset ()
{
	m_HoverT = false;
	m_HoverS = false;
	m_HoverR = false;
	m_HoveredAxis = 0xff;
}

bool CEditorWidget::WantHoverFocus () const
{
	return (m_HoverT || m_HoverR || m_HoverS || (m_HoveredAxis != 0xff));
}

Vec3V_Out CEditorWidget::GetPosition () const
{
	return m_Matrix.GetCol3();
}

void CEditorWidget::LineTranslate(Mat34V_In startMat, Vec3V_In lineEnd, Vec3V_In coneStart, Color32 color) const
{
	Color32 halfA = color;
	halfA.SetAlpha(128);
	grcDebugDraw::Line(startMat.GetCol3(), lineEnd, color);
	grcDebugDraw::Cone(lineEnd, coneStart, 0.05f, halfA, false);
	grcDebugDraw::Cone(lineEnd, coneStart, 0.05f, color, false, false);
}

void CEditorWidget::LineScale(Vec3V_In position, Mat34V_In matrix, Color32 color) const
{
	Vec3V bxMin(-0.05f,-0.05f,-0.05f);
	Vec3V bxMax(0.05f,0.05f,0.05f);

	Color32 halfA = color;
	halfA.SetAlpha(128);

	grcDebugDraw::Line(position, matrix.GetCol3(), color);
	grcDebugDraw::BoxOriented(bxMin, bxMax, matrix, halfA);
	grcDebugDraw::BoxOriented(bxMin, bxMax, matrix, color, false);
}

void CEditorWidget::Circle(Vec3V_In position, Color32 color, Vec3V_In tx, Vec3V_In tz) const
{
	grcDebugDraw::Circle(position,0.5f,color,tx, tz);
}

void CEditorWidget::UpdateAxis()
{
	//Cylinders point down the Y axis by default so need rotate for x & z ... 
	ScalarV halfpi(V_PI_OVER_TWO);
	Mat33V rot(V_IDENTITY);
	Mat33V final(V_IDENTITY);
	Vec3V off(V_ZERO);

	//X
	Mat33VFromZAxisAngle(rot, halfpi);
	Multiply(final, m_Matrix.GetMat33(), rot);
	m_Axis[0].Set3x3(final);
	if (m_isTranslation || m_isScale) //rotations dont need the offset
		off = Multiply(m_Matrix.GetMat33(), Vec3V(0.5f,0.0f,0.0f));
	m_Axis[0].SetCol3(m_Matrix.GetCol3() + off);

	//Y
	m_Axis[1] = m_Matrix;
	if (m_isTranslation || m_isScale) //rotations dont need the offset
		off = Multiply(m_Matrix.GetMat33(), Vec3V(0.0f,0.5f,0.0f));
	m_Axis[1].SetCol3(m_Matrix.GetCol3() + off);

	//Z
	Mat33VFromXAxisAngle(rot, halfpi);
	Multiply(final, m_Matrix.GetMat33(), rot);
	m_Axis[2].Set3x3(final);
	if (m_isTranslation || m_isScale) //rotations dont need the offset
		off = Multiply(m_Matrix.GetMat33(), Vec3V(0.0f,0.0f,0.5f));
	m_Axis[2].SetCol3(m_Matrix.GetCol3() + off);
}

bool CEditorWidget::DoesRayIntersectAxis(u8 axisIndex, Vec3V_In startPoint, Vec3V_In endPoint, Vec3V_InOut hitPoint, bool infinateCyl/*=false*/) const
{
	Mat34V inverse;
	InvertTransformFull(inverse, m_Axis[axisIndex]);

	Vec3V localStart = Transform(inverse, startPoint);
	Vec3V localEnd = Transform(inverse, endPoint);
	Vec3V dir = localEnd - localStart;

	if (m_isTranslation || m_isScale)
	{
		float radius = 0.05f;
		if (infinateCyl)
		{
			radius = 0.09f;
		}

		float totalLength = 1.0f;
		float segmentT1, segmentT2, cylinderT1, cylinderT2;
		//The cylinder is considered to extend from y=-length/2 to y=+length/2
		// for us that is -0.5 to 0.5.
		if (geomSegments::SegmentToUprightCylIsects(
			VEC3V_TO_VECTOR3(localStart),
			VEC3V_TO_VECTOR3(dir),
			totalLength, 
			(radius * radius),
			&segmentT1,
			&segmentT2,
			&cylinderT1,
			&cylinderT2)
			)
		{

			if (infinateCyl)
			{
				Vec3V hitLocal = localStart + (dir * ScalarV(segmentT1));
				hitPoint = Transform(m_Axis[axisIndex], hitLocal);
				return true;
			}
			else
			{
				//Are we within the limits of the cylinders height
				//	- The cylinder t values are from 0 (meaning y=-length/2) to 1 (meaning y=+length/2).  They indicate the
				//	t value of the point on the cylinder's axis closest to the intersection point.
				if(cylinderT1 >= 0.0f && cylinderT1 <= 1.0f)
				{
					Vec3V hitLocal = localStart + (dir * ScalarV(segmentT1));
					hitPoint = Transform(m_Axis[axisIndex], hitLocal);
					return true;
				}

				if (cylinderT2 >= 0.0f && cylinderT2 <= 1.0f)
				{
					Vec3V hitLocal = localStart + (dir * ScalarV(segmentT2));
					hitPoint = Transform(m_Axis[axisIndex], hitLocal);
					return true;
				}
			}
		}
	}
	else if (m_isRotation)
	{

		float radius = 0.5f;
		//The disk is assumed to be in a plane with constant y (i.e. aligned with the XZ plane), and the disk is assumed to
		//	be centered at (x=0,z=0).
		float segmentT1;
		if (geomSegments::SegmentToDiskIntersection(
			VEC3V_TO_VECTOR3(localStart),
			VEC3V_TO_VECTOR3(dir),
			(radius * radius),
			&segmentT1)
			)
		{
			Vec3V hitLocal = localStart + (dir * ScalarV(segmentT1));
			hitPoint = Transform(m_Axis[axisIndex], hitLocal);
			return true;
		}
	}

	return false;
}

bool CEditorWidget::AxisActive(u8 axisIndex) const
{
	switch(axisIndex)
	{
	case 0://X
		if (m_isTranslation && m_SupportFlags.TranslateX()) return true;
		if (m_isScale && m_SupportFlags.ScaleX()) return true;
		if (m_isRotation && m_SupportFlags.RotateX()) return true;
		break;
	case 1://Y
		if (m_isTranslation && m_SupportFlags.TranslateY()) return true;
		if (m_isScale && m_SupportFlags.ScaleY()) return true;
		if (m_isRotation && m_SupportFlags.RotateY()) return true;
		break;
	case 2://Z
		if (m_isTranslation && m_SupportFlags.TranslateZ()) return true;
		if (m_isScale && m_SupportFlags.ScaleZ()) return true;
		if (m_isRotation && m_SupportFlags.RotateZ()) return true;
		break;
	default:
		break;
	};

	return false;
}

#endif // __BANK