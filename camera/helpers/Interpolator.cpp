//
// camera/helpers/Interpolator.h
// 
// Copyright (C) 1999-2015 Rockstar Games.  All Rights Reserved.
//
#include "Interpolator.h"
#include "math/amath.h"

camInterpolator::camInterpolator()
    : m_T1(0.0f)
    , m_T2(0.0f)
    , m_T3(0.0f)
    , m_X1(0.0f)
    , m_X2(0.0f)
    , m_Start(0.0f)
    , m_End(0.0f)
    , m_Duration(0.0f)
    , m_ExitTangent(0.0f)
    , m_EnterTangent(0.0f)
    , m_T1Valid(false)
    , m_T2Valid(false)
    , m_T3Valid(false)
{
}

camInterpolator::camInterpolator(float duration, float easeIn, float easeOut, float start, float end)
{
    UpdateParameters(duration, easeIn, easeOut, start, end);
}

float camInterpolator::Interpolate(float t) const
{
    //make sure the t is inside the valid function range
    t = Clamp( t, 0.0f, m_Duration );

    float returnValue = m_Start;

    if ( m_T1Valid && m_T2Valid && t <= m_T1 )
    {
        //accelerating
        const float t_norm = t / m_T1;
        returnValue = CubicHermite( t_norm, m_Start, m_X1, 0.0f, m_ExitTangent );
    }
    else if ( m_T2Valid && t > m_T1 && t <= ( m_T1 + m_T2 ) )
    {
        //constant speed
        const float t_norm = ( t - m_T1 ) / m_T2;
        returnValue = ( m_X1 + m_X2 * t_norm );
    }
    else if ( m_T2Valid && m_T3Valid )
    {
        //decelerating
        const float t_norm = ( t - m_T1 - m_T2 ) / m_T3;
        returnValue = CubicHermite( t_norm, m_X1 + m_X2, m_End, m_EnterTangent, 0.0f );
    }
    else
    {
        //failsafe case, simple linear interpolation
        float t_ratio = m_Duration != 0.0f ? t / m_Duration : 1.0f;
        returnValue = Lerp( t_ratio, m_Start, m_End );
    }

    return Clamp( returnValue, m_Start, m_End );
}

void camInterpolator::UpdateParameters(float duration, float easeIn, float easeOut, float start, float end)
{
    m_Duration = duration;
    m_Start = start;
    m_End = end;

    //timers (accelerating, constant speed, decelerating)
    const float midTime = duration - easeIn - easeOut;

    //ensure that all the values are not zero
    m_T1 = easeIn;
    m_T2 = midTime;
    m_T3 = easeOut;

    m_T1Valid = easeIn  != 0.0f;
    m_T2Valid = midTime != 0.0f;
    m_T3Valid = easeOut != 0.0f;

    //average velocity of the interpolation
    const float velocity = ( m_T1Valid || m_T2Valid || m_T3Valid ) ? ( end - start ) / ( ( m_T1 * 0.5f ) + m_T2 + ( m_T3 * 0.5f ) ) : 0.0f;

    //steps (accelerating, constant speed, decelerating)
    m_X1 = velocity * m_T1 * 0.5f;
    m_X2 = velocity * m_T2;

    //tangents
    m_ExitTangent  = ( m_T1Valid && m_T2Valid ) ? (m_X2 / m_T2) * m_T1 : 0.0f;
    m_EnterTangent = ( m_T2Valid && m_T3Valid ) ? (m_X2 / m_T2) * m_T3 : 0.0f;
}

void camInterpolator::UpdateParameters(const camInterpolatorMetadata& metadata)
{
    UpdateParameters(metadata.Duration, metadata.EaseIn, metadata.EaseOut, metadata.Start, metadata.End);
}

//EOF
