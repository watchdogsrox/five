/////////////////////////////////////////////////////////////////////////////////
// FILE :		VolumeManager.h
// PURPOSE :	The factory class for volumes
// AUTHOR :		Jason Jurecka.
// CREATED :	3/5/2011
/////////////////////////////////////////////////////////////////////////////////

#include "Scene\Volume\VolumeManager.h"

#if VOLUME_SUPPORT
///////////////////////////////////////////////////////////////////////////////
//  INCLUDES
///////////////////////////////////////////////////////////////////////////////

//rage
#include "bank\bkmgr.h"
#include "bank\bank.h"
#include "grcore\debugdraw.h"
#include "grcore\im.h"

//framework
#include "fwsys\gameskeleton.h"

//Game
#include "Scene\Volume\VolumeBox.h"
#include "Scene\Volume\VolumeCylinder.h"
#include "Scene\Volume\VolumeSphere.h"
#include "Scene\Volume\VolumeAggregate.h"
#include "Scene\Volume\VolumeTool.h"

///////////////////////////////////////////////////////////////////////////////
//  GLOBAL VARIABLES
///////////////////////////////////////////////////////////////////////////////
CVolumeManager g_VolumeManager;

///////////////////////////////////////////////////////////////////////////////
//  STATIC CODE
///////////////////////////////////////////////////////////////////////////////
void CVolumeManager::Init (unsigned initMode)
{
    //INIT_CORE
    //INIT_BEFORE_MAP_LOADED
    //INIT_AFTER_MAP_LOADED
    //INIT_SESSION

    if (initMode == INIT_SESSION)
    {
        CVolumeBox::InitPool( MEMBUCKET_GAMEPLAY );
        CVolumeCylinder::InitPool( MEMBUCKET_GAMEPLAY );
        CVolumeSphere::InitPool( MEMBUCKET_GAMEPLAY );
        CVolumeAggregate::InitPool( MEMBUCKET_GAMEPLAY );
    }
}

void CVolumeManager::Shutdown (unsigned shutdownMode)
{
    //SHUTDOWN_CORE
    //SHUTDOWN_WITH_MAP_LOADED
    //SHUTDOWN_SESSION

    if (shutdownMode == SHUTDOWN_SESSION)
    {
        CVolumeBox::ShutdownPool();
        CVolumeCylinder::ShutdownPool();
        CVolumeSphere::ShutdownPool();
        CVolumeAggregate::ShutdownPool();
    }
}

void CVolumeManager::Update ()
{

}

bool CVolumeManager::FindClosestPointInEllipsoid(float prea, float preb, float prec,
                                                 float preu, float prev, float prew, float &xout, float &yout, float &zout)
{
    // This algorithm is based on the equation for an ellipsoid combined by
    // the condition that the closest point on the ellipsoid will have a normal
    // parallel to the vector from it to the input point. By introducing a
    // parameter t, this turns into a sixth degree polynomial equation,
    // which is solved using Newton-Raphson's method. This approach is described
    // in Eberly, D. H., "3D Game Engine Design: A Practical Approach to Real-Time Computer Graphics",
    // Morgan Kaufmann, 2001, though most of that is supposedly originally based
    // on an article in "Graphics Gems IV". /FF

    // Find the largest axis. /FF
    const float rescale = Max(prea, preb, prec);

    // Check size of largest axis. /FF
    if(rescale < SMALL_FLOAT)
    {
        // The volume is tiny in all dimensions. I suppose we may as well
        // just return the center point and consider it a success. /FF
        xout = yout = zout = 0.0f;
        return true;
    }

    // Compute a factor for uniform pre-scaling of the coordinates. /FF
    const float prescale = 1.0f/rescale;

    // Pre-scale the input point. /FF
    const float u = preu*prescale;
    const float v = prev*prescale;
    const float w = prew*prescale;

    // Pre-scale the ellipsoid axes. /FF
    const float a = prea*prescale;
    const float b = preb*prescale;
    const float c = prec*prescale;

    // Check to see if any ellipsoid axis is now close to 0. /FF
    if(Min(a, b, c) <= SMALL_FLOAT)
    {
        // This would indicate a flattened or otherwise degenerate ellipsoid,
        // for which the numerical solution would be poorly conditioned.
        // It would be possibe to correctly treat these as special cases here
        // (finding the closest point on a 2D ellipse, 1D line, or "0D" point,
        // depending on how degenerate it is), but for now we just treat them
        // as failures. /FF
        return false;
    }

    // Compute some squares we'll need. /FF
    const float a2 = square(a);
    const float b2 = square(b);
    const float c2 = square(c);
    const float u2 = square(u);
    const float v2 = square(v);
    const float w2 = square(w);

    // Check if the point is inside the ellipsoid. /FF
    const float r0 = u2/a2 + v2/b2 + w2/c2;
    if(r0 <= 1.0f)
    {
        xout = preu;
        yout = prev;
        zout = prew;
        return true;
    }

    // Compute the initial point for Newton-Raphson iteration. This initial value
    // was recommended by the book mentioned earlier. /FF
    const float t0 = sqrtf(r0);

    // Compute some more constants. /FF
    const float nega2u2 = -a2*u2;
    const float negb2v2 = -b2*v2;
    const float negc2w2 = -c2*w2;
    const float twoa2 = 2.0f*a2;
    const float twob2 = 2.0f*b2;
    const float twoc2 = 2.0f*c2;

    // MAGIC! This is a threshold for when to stop. I believe it is interpreted
    // as a squared distance (in the scaled space), so it's roughly 1% of the
    // largest axis. /FF
    const float distSqThreshold = square(0.01f);

    bool success = false;

    // MAGIC! Often, in practice I've seen solutions after about 20 iterations or so,
    // so if we reach 100 it's probably not going to converge. /FF
    static const int kMaxIter = 100;

    // Perform Newton-Raphson iteration. /FF
    float t = t0;
    for(int i = 0; i < kMaxIter; i++)
    {
        // Compute the value of the polynomial. /FF
        const float ta = square(t + a2);
        const float tb = square(t + b2);
        const float tc = square(t + c2);
        const float tatb = ta*tb;
        const float tbtc = tb*tc;
        const float tatc = ta*tc;
        const float ft1 = tatb*tc;
        const float ft2 = nega2u2*tbtc;
        const float ft3 = negb2v2*tatc;
        const float ft4 = negc2w2*tatb;
        const float ft = ft1 + ft2 + ft3 + ft4;

        // I believe that by dividing by ft1, we end up with
        // something proportional to the squared distance between the surface
        // of the volume and the point corresponding to the current parameter
        // t. We now check to see if this is close enough to stop iterating. /FF
        if(fabsf(ft/ft1) <= distSqThreshold)
        {
            success = true;
            break;
        }

        // Compute the derivative of the polyonomial. /FF
        const float twot = 2.0f*t;
        const float dta = twot + twoa2;
        const float dtb = twot + twob2;
        const float dtc = twot + twoc2;
        const float dtatb = ta*dtb + dta*tb;
        const float dtbtc = tb*dtc + dtb*tc;
        const float dtcta = tc*dta + dtc*ta;
        const float dtatc = dtcta;
        const float dft1 = dtatb*tc + tatb*dtc;
        const float dft2 = nega2u2*dtbtc;
        const float dft3 = negb2v2*dtatc;
        const float dft4 = negc2w2*dtatb;
        const float dft = dft1 + dft2 + dft3 + dft4;

        if(fabsf(dft) <= VERY_SMALL_FLOAT)
        {
            // I guess this would indicate a major loss of precision,
            // so consider it a failure instead of dividing by a tiny
            // value. /FF
            break;
        }

        // Apply the Newton-Raphson iteration on the parameter value. /FF
        t = t - ft/dft;
    }

    if(success)
    {
        // We got close enough, compute the coordinates of the point
        // and scale it back to the input coordinate system. /FF
        xout = rescale*a2*u/(t + a2);
        yout = rescale*b2*v/(t + b2);
        zout = rescale*c2*w/(t + c2);
        return true;
    }

    // Failed to find a point. /FF
    return false;
}

bool CVolumeManager::FindClosestPointInEllipse(float prea, float preb,
                                               float preu, float prev, float &xout, float &yout)
{
    // This algorithm is based on the equation for an ellipse combined by
    // the condition that the closest point on the ellipse will have a normal
    // parallel to the vector from it to the input point. By introducing a
    // parameter t, this turns into a fourth degree polynomial equation,
    // which is solved using Newton-Raphson's method. This approach is described
    // in Eberly, D. H., "3D Game Engine Design: A Practical Approach to Real-Time Computer Graphics",
    // Morgan Kaufmann, 2001, though most of that is supposedly originally based
    // on an article in "Graphics Gems IV". /FF

    // Find the largest axis. /FF
    const float rescale = Max(prea, preb);

    // Check size of largest axis. /FF
    if(rescale < SMALL_FLOAT)
    {
        // The volume is tiny in all dimensions. I suppose we may as well
        // just return the center point and consider it a success. /FF
        xout = yout = 0.0f;
        return true;
    }

    // Compute a factor for uniform pre-scaling of the coordinates.
    // This is done for increased numerical robustness - in fact,
    // without it I had trouble getting the NR solver to converge,
    // even for FF
    const float prescale = 1.0f/rescale;

    // Pre-scale the input point. /FF
    const float u = preu*prescale;
    const float v = prev*prescale;

    // Pre-scale the ellipse axes. /FF
    const float a = prea*prescale;
    const float b = preb*prescale;

    // Check to see if any ellipse axis is now close to 0. /FF
    if(Min(a, b) <= SMALL_FLOAT)
    {
        // This would indicate a near-degenerate ellipse,
        // for which the numerical solution would be poorly conditioned.
        // It would be possibe to correctly treat these as special cases here
        // (finding the closest point on a 1D line, or "0D" point,
        // depending on how degenerate it is), but for now we just treat them
        // as failures. /FF
        return false;
    }

    // Compute some squares we'll need. /FF
    const float a2 = square(a);
    const float b2 = square(b);
    const float u2 = square(u);
    const float v2 = square(v);

    // Check if the point is inside the ellipse. /FF
    const float r0 = u2/a2 + v2/b2;
    if(r0 <= 1.0f)
    {
        xout = preu;
        yout = prev;
        return true;
    }

    // Compute the initial point for Newton-Raphson iteration. This initial value
    // was recommended by the book mentioned earlier. /FF
    const float t0 = sqrtf(r0);

    // Compute some more constants. /FF
    const float nega2u2 = -a2*u2;
    const float negb2v2 = -b2*v2;
    const float twoa2 = 2.0f*a2;
    const float twob2 = 2.0f*b2;

    // MAGIC! This is a threshold for when to stop. I believe it is interpreted
    // as a squared distance (in the scaled space), so it's roughly 1% of the
    // largest axis. /FF
    const float distSqThreshold = square(0.01f);

    bool success = false;

    // MAGIC! Often, in practice I've seen solutions after about 20 iterations or so,
    // so if we reach 100 it's probably not going to converge. /FF
    static const int kMaxIter = 100;

    // Perform Newton-Raphson iteration. /FF
    float t = t0;
    for(int i = 0; i < kMaxIter; i++)
    {
        // Compute the value of the polynomial. /FF
        const float ta = square(t + a2);
        const float tb = square(t + b2);
        const float tatb = ta*tb;
        const float ft1 = tatb;
        const float ft2 = nega2u2*tb;
        const float ft3 = negb2v2*ta;
        const float ft = ft1 + ft2 + ft3;

        // I believe that by dividing by ft1, we end up with
        // something proportional to the squared distance between the surface
        // of the volume and the point corresponding to the current parameter
        // t. We now check to see if this is close enough to stop iterating. /FF
        if(fabsf(ft/ft1) <= distSqThreshold)
        {
            success = true;
            break;
        }

        // Compute the derivative of the polyonomial. /FF
        const float twot = 2.0f*t;
        const float dta = twot + twoa2;
        const float dtb = twot + twob2;
        const float dtatb = ta*dtb + dta*tb;
        const float dft1 = dtatb;
        const float dft2 = nega2u2*dtb;
        const float dft3 = negb2v2*dta;
        const float dft = dft1 + dft2 + dft3;

        if(fabsf(dft) <= VERY_SMALL_FLOAT)
        {
            // I guess this would indicate a major loss of precision,
            // so consider it a failure instead of dividing by a tiny
            // value. /FF
            break;
        }

        // Apply the Newton-Raphson iteration on the parameter value. /FF
        t = t - ft/dft;
    }

    if(success)
    {
        // We got close enough, compute the coordinates of the point
        // and scale it back to the input coordinate system. /FF
        xout = rescale*a2*u/(t + a2);
        yout = rescale*b2*v/(t + b2);
        return true;
    }

    // Failed to find a point. /FF
    return false;
}

#if __BANK
void CVolumeManager::OpenVolumeTool()
{
    g_VolumeManager.OpenTool();
}

void CVolumeManager::CloseVolumeTool()
{
    g_VolumeManager.CloseTool();
}

bool CVolumeManager::RenderDebugCB (void* volume, void* /*data*/)
{
    Assert(volume);
	CVolume *pVolume = static_cast<CVolume *>(volume);
	CVolume::RenderMode renderMode = (g_VolumeManager.ShouldRenderAsSolid()) ? CVolume::RENDER_MODE_SOLID : CVolume::RENDER_MODE_WIREFRAME;
	pVolume->RenderDebug(g_VolumeManager.GetDrawColor(pVolume), renderMode);
    return true;
}
#endif

///////////////////////////////////////////////////////////////////////////////
//  CODE
///////////////////////////////////////////////////////////////////////////////
CVolumeManager::CVolumeManager(void)
#if __BANK
: m_renderAllDebug(false)
, m_renderBoxDebug(false)
, m_renderCylinderDebug(false)
, m_renderSphereDebug(false)
, m_renderAggregateDebug(false)
, m_renderSolidObjects(false)
, m_renderAxis(false)
, m_DrawColor(0, 200, 0, 255)
, m_AggregateColor(0, 128, 128, 255)
, m_SelectedColor(250, 250, 250, 255)
, m_HighlightColor(222, 222, 0, 255)
, m_InActiveColor(200, 200, 200, 255)
, m_AxisSize(1.0f)
#endif
{
}

CVolume* CVolumeManager::CreateVolumeBox(Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale)
{
    CVolumeBox *result = rage_new CVolumeBox();
    Assertf(result, "Unable to create volume box");
    result->Init(rPosition,rOrientation,rScale);
    return result;
}

CVolume* CVolumeManager::CreateVolumeCylinder(Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale)
{
    CVolumeCylinder *result = rage_new CVolumeCylinder();
    Assertf(result, "Unable to create volume cylinder");
    result->Init(rPosition,rOrientation,rScale);
    return result;
}

CVolume* CVolumeManager::CreateVolumeSphere(Vec3V_In rPosition, Mat33V_In rOrientation, Vec3V_In rScale)
{
    CVolumeSphere *result = rage_new CVolumeSphere();
    Assertf(result, "Unable to create volume sphere");
    result->Init(rPosition,rOrientation,rScale);
    return result;
}

CVolume* CVolumeManager::CreateVolumeAggregate()
{
    CVolumeAggregate *result = rage_new CVolumeAggregate();
    Assertf(result, "Unable to create volume Aggregate");
    return result;
}

void CVolumeManager::DeleteVolume (CVolume* toDelete)
{
    delete toDelete;
}

#if __BANK
void CVolumeManager::DrawVolumeAxis(Mat34V_In mtrx) const
{
    if (m_renderAxis)
    {
		grcDebugDraw::Axis(mtrx, m_AxisSize);
    }
}

void CVolumeManager::InitWidgets()
{
    BANKMGR.CreateBank("Volumes Script", 0, 0, false);
    CloseTool();
}

void CVolumeManager::OpenTool()
{
    //Delete all child widgets
    bkBank* pBank = BANKMGR.FindBank("Volumes Script");
    Assertf(pBank, "Unable to find bank (Volumes Script)");
    bkWidget* child = pBank->GetChild();
    while(child)
    {
        bkWidget* toDestroy = child;
        child = child->GetNext();
        toDestroy->Destroy();
    }

    g_VolumeTool.Init();
    pBank->AddButton("Close Volume Tool", &CVolumeManager::CloseVolumeTool);
    pBank->AddToggle("Render Solid", &m_renderSolidObjects);
    pBank->AddToggle("Render Volume Axis", &m_renderAxis);
    pBank->AddSlider("Axis Size", &m_AxisSize, 0.01f, 10.0f, 0.01f);
    pBank->AddColor("Volume Color", &m_DrawColor);
    pBank->AddColor("Aggregate Volume Color", &m_AggregateColor);
    pBank->AddColor("Selected Volume Color", &m_SelectedColor);
    pBank->AddColor("Highlighted Volume Color", &m_HighlightColor);
    pBank->AddColor("InActive Volume Color", &m_InActiveColor);    
    g_VolumeTool.CreateWidgets(pBank);
}

void CVolumeManager::CloseTool()
{
    bkBank* pBank = BANKMGR.FindBank("Volumes Script");
    Assertf(pBank, "Unable to find bank (Volumes Script)");
    bkWidget* child = pBank->GetChild();
    while(child)
    {
        bkWidget* toDestroy = child;
        child = child->GetNext();
        toDestroy->Destroy();
    }

    g_VolumeTool.Shutdown();
    pBank->AddButton("Open Volume Tool", &CVolumeManager::OpenVolumeTool);
    pBank->AddToggle("Render All Volumes", &m_renderAllDebug);
    pBank->AddToggle("Render Box Volumes", &m_renderBoxDebug);
    pBank->AddToggle("Render Cylinder Volumes", &m_renderCylinderDebug);
    pBank->AddToggle("Render Sphere Volumes", &m_renderSphereDebug);
    pBank->AddToggle("Render Aggregate Volumes", &m_renderAggregateDebug);
    pBank->AddToggle("Render Solid", &m_renderSolidObjects);
    pBank->AddToggle("Render Volume Axis", &m_renderAxis);
    pBank->AddSlider("Axis Size", &m_AxisSize, 0.01f, 10.0f, 0.01f);
    pBank->AddColor("Volume Color", &m_DrawColor);
    pBank->AddColor("Aggregate Volume Color", &m_AggregateColor);    
}

void CVolumeManager::RenderDebug()
{
    if (g_VolumeTool.IsOpen())
    {
        g_VolumeTool.Render();
        return; //drawing other things would probably be confusing
    }

    if (m_renderAllDebug || m_renderBoxDebug)
    {
        CVolumeBox::Pool* p = CVolumeBox::GetPool();
        p->ForAll(&CVolumeManager::RenderDebugCB, NULL);
    }

    if (m_renderAllDebug || m_renderCylinderDebug)
    {
        CVolumeCylinder::Pool* p = CVolumeCylinder::GetPool();
        p->ForAll(&CVolumeManager::RenderDebugCB, NULL);
    }        

    if (m_renderAllDebug || m_renderSphereDebug)
    {
        CVolumeSphere::Pool* p = CVolumeSphere::GetPool();
        p->ForAll(&CVolumeManager::RenderDebugCB, NULL);
    }

    if (m_renderAllDebug || m_renderAggregateDebug)
    {
        CVolumeAggregate::Pool* p = CVolumeAggregate::GetPool();
        p->ForAll(&CVolumeManager::RenderDebugCB, NULL);
    }
}

Color32 CVolumeManager::GetDrawColor (const CVolume* volume) const
{
    Color32 retval = m_DrawColor;
    if (volume->GetOwner())
    {
        retval = m_AggregateColor;
    }

    if (g_VolumeTool.IsOpen())
    {
        CVolumeTool::eRenderStyle rs = g_VolumeTool.GetRenderStyle();
        if (rs == CVolumeTool::ersHighlight)
        {
            retval = m_HighlightColor;
        }
        else if (rs == CVolumeTool::ersSelected)
        {
            retval = m_SelectedColor;
        }
        else if (rs == CVolumeTool::ersInActive)
        {
            retval = m_InActiveColor;
        }
        else //if (rs == CVolumeTool::ersNormal)
        {/*nothing*/}
    }

    return retval;
}

#endif 
#endif // VOLUME_SUPPORT