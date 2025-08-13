//
// camera/helpers/Interpolator.h
// 
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
#ifndef CAMERA_HELPER_INTERPOLATOR_H
#define CAMERA_HELPER_INTERPOLATOR_H

#include "camera/system/CameraMetadata.h"

//-------------------------------------------------------------------------
// camInterpolator helper: Non linear interpolator class.	  
//-------------------------------------------------------------------------
class camInterpolator
{
public:
    camInterpolator();
    camInterpolator(float duration, float easeIn, float easeOut, float start, float end);

    //-------------------------------------------------------------------------	  
    // Unidimensional non linear interpolate function that use cubic Hermite 
    // curves to smooth the resulting value.
    //
    // Param[in] t - current time of interpolation
    //
    // Return    The interpolated value in range [start, end]
    //-------------------------------------------------------------------------
    float Interpolate(float t) const;

    //-------------------------------------------------------------------------
    // Update the parameters and recompute the constant tangent values.
    //
    // Param[in] duration - duration of the interpolation
    // Param[in] easeIn   - duration of the acceleration. easeIn + easeOut <= duration.
    // Param[in] easeOut  - duration of the deceleration. easeIn + easeOut <= duration.
    // Param[in] start    - interpolation start value
    // Param[in] end      - interpolation end value
    //-------------------------------------------------------------------------
    void UpdateParameters(float duration, float easeIn, float easeOut, float start, float end);
    void UpdateParameters(const camInterpolatorMetadata& metadata);

private:
    //-------------------------------------------------------------------------
    // Cubic Hermite Spline function
    // See http://en.wikipedia.org/wiki/Cubic_Hermite_spline
    //
    // Param[in] t  - ratio in interval [0.0f,1.0f]
    // Param[in] p0 - start position
    // Param[in] p1 - end position
    // Param[in] m0 - start tangent
    // Param[in] m1 - end tangent
    //
    // Return    The value on the curve at the specified ratio.
    //-------------------------------------------------------------------------
    float CubicHermite(float t, float p0, float p1, float m0, float m1) const
    {
        const float t_2 = t * t;
        const float t_3 = t_2 * t;

        return ( 2.0f * t_3 - 3.0f * t_2 + 1.0f ) * p0 + 
               ( t_3 - 2.0f * t_2 + t )           * m0 + 
               ( -2.0f * t_3 + 3.0f * t_2 )       * p1 + 
               ( t_3 - t_2 )                      * m1;
    }

private:
    float m_T1;
    float m_T2;
    float m_T3;
    float m_X1;
    float m_X2;
    float m_Start;
    float m_End;
    float m_Duration;
    float m_ExitTangent;
    float m_EnterTangent;

    bool m_T1Valid;
    bool m_T2Valid;
    bool m_T3Valid;
};

#endif //CAMERA_HELPER_INTERPOLATOR_H