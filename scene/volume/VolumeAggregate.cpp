// Title	:	"scene/volume/VolumeAggregate.h"
// Author	:	Jason Jurecka
// Started	:	2/5/11
//

#include "scene/volume/Volume.h"

#if VOLUME_SUPPORT

//rage includes
#include "math\amath.h"
#include "vector\geometry.h"
#include "vector\color32.h"
#include "grcore\im.h"
#include "file\stream.h"
#include "bank\bkmgr.h"

//framework includes

//game includes
#include "scene\volume\VolumeAggregate.h"
#include "scene\volume\VolumeManager.h"
#include "scene\volume\VolumeTool.h"

FW_INSTANTIATE_CLASS_POOL(CVolumeAggregate, 256, atHashString("VolumeAggregate",0xf4197375));
INSTANTIATE_RTTI_CLASS(CVolumeAggregate,0x14DCF76D)

// NOTE: DELETES ANY VOLUMES STILL IN THE LINKED LIST
CVolumeAggregate::~CVolumeAggregate()
{
    LinkedListNode* pNode = m_Head;
    while (pNode)
    {
        LinkedListNode* pNext = pNode->m_Next;
        pNode->m_Data->SetOwner(NULL); //this done so it does not loop back and try to "remove" 
                                       //   from the list that is getting deleted here.
        delete pNode->m_Data;
        delete pNode;

        pNode = pNext;
    }
    m_Head = NULL;
}

void CVolumeAggregate::Add(CVolume* pVolume)
{
    CVolumeAggregate* owner = pVolume->GetOwner();

    bool add = false;
    if (!owner)
    {
        //no owner yet so will add it
        add = true;
        pVolume->SetOwner(this);
    }
    else if (owner == this)
    {
        //already in my aggregate dont add twice but throw an assert
        Assertf(0, "Adding volume to Aggregate that already contains the volume");
    }
    else if (owner != this)
    {
        // switch which aggregate volume i am in ... Display info indicating switch
        add = true;
        owner->Remove(pVolume);
        pVolume->SetOwner(this);
        Warningf("Switching aggregate owners for volume.");
    }

    if (add)
    {
        LinkedListNode* node = rage_new LinkedListNode;
        node->m_Data = pVolume;
        node->m_Next = m_Head;
        m_Head = node;
        m_OwnedCount++;
    }
}

// NOTE: DOES NOT DELETE THE VOLUME ONLY REMOVES FROM LINKED LIST
void CVolumeAggregate::Remove (CVolume* pVolume)
{
    LinkedListNode* pCur = m_Head;
    LinkedListNode* pPrev = NULL;
    while(pCur) {
        if (pCur->m_Data == pVolume)
        {
            if (pPrev)
            {
                pPrev->m_Next = pCur->m_Next;
            }
            else
            {
                //must be the head
                m_Head = pCur->m_Next;
            }

            pCur->m_Data->SetOwner(NULL);//I am no longer the owner of this Volume
            delete pCur; //this should be the isolated link list node
            m_OwnedCount--;
            break;
        }
        pPrev = pCur;
        pCur = pCur->m_Next;
    }
}

void CVolumeAggregate::RemoveAll ()
{
	LinkedListNode* pNode = m_Head;
	while (pNode)
	{
		LinkedListNode* pNext = pNode->m_Next;
		pNode->m_Data->SetOwner(NULL); //this done so it does not loop back and try to "remove" 
		delete pNode;

		pNode = pNext;
	}
	m_Head = NULL;
}

void CVolumeAggregate::Scale (float fScale)
{
    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->Scale( fScale );
        pCur = pCur->m_Next;
    }
}

bool CVolumeAggregate::GetPosition (Vec3V_InOut rPoint) const
{
    //Just get the center point of all the aggregate volumes...
    Vec3V maxBox(V_NEG_FLT_MAX);
    Vec3V minBox(V_FLT_MAX);
    LinkedListNode* pCur = m_Head;
    while(pCur) {
        Vec3V mxBox;
        Vec3V mnBox;
        pCur->m_Data->GetBounds(mxBox, mnBox);

        maxBox = Max(maxBox, mxBox);
        minBox = Min(minBox, mnBox);
        pCur = pCur->m_Next;
    }

    rPoint = (maxBox + minBox) * ScalarV(V_HALF);
    return true;
}

bool CVolumeAggregate::GetTransform (Mat34V_InOut rMatrix) const
{
    //Just get the center point of all the aggregate volumes...
    Vec3V center(V_ZERO);
    GetPosition(center);
    rMatrix.SetIdentity3x3();
    rMatrix.Setd(center);
    return true;
}

void CVolumeAggregate::GetBounds(Vec3V_InOut rMax, Vec3V_InOut rMin) const
{
    rMax = Vec3V(V_NEG_FLT_MAX);
    rMin = Vec3V(V_FLT_MAX);

    LinkedListNode* pCur = m_Head;
    while(pCur) {
        Vec3V lMax;
        Vec3V lMin;
        pCur->m_Data->GetBounds(lMax, lMin);

        rMax = Max(rMax,lMax);
        rMin = Min(rMin,lMin);
        
        pCur = pCur->m_Next;
    }
}

bool CVolumeAggregate::IsPointInside(Vec3V_In rPoint) const
{
    if (GetEnabled())
    {
        // TODO:  Compare against our own volume first!
        const LinkedListNode* pCur = m_Head;
        while(pCur) {
            if (pCur->m_Data->IsPointInside(rPoint))
            {
                return true;
            }
            pCur = pCur->m_Next;
        }
    }

    return false;
}

void CVolumeAggregate::GenerateRandomPointInVolume(Vec3V_InOut point, mthRandom &randGen) const
{
    static const int kMaxChildren = 256;
    float childVol[kMaxChildren];
    const CVolume *childPtr[kMaxChildren];

    int numChildren = 0;
    const LinkedListNode* pCur;
    float volSum = 0.0f;
    for(pCur = m_Head; pCur; pCur = pCur->m_Next)
    {
        if(Verifyf(numChildren < kMaxChildren, "Max %d child volumes supported.", kMaxChildren))
        {
            const float vol = pCur->m_Data->ComputeVolume();
            volSum += vol;

            childVol[numChildren] = vol;
            childPtr[numChildren] = pCur->m_Data;
            numChildren++;
        }
    }

    if(Verifyf(numChildren > 0, "Trying to generate random point in empty volume set."))
    {
        const int n = numChildren - 1;
        float p = randGen.GetFloat()*volSum;
        int i;
        for(i = 0; i < n; i++)
        {
            const float vol = childVol[i];
            if(p < vol)
            {
                break;
            }
            p -= vol;
        }
        childPtr[i]->GenerateRandomPointInVolume(point, randGen);
    }
    else
    {
        // Better than leaving it uninitialized, I suppose. /FF
        point.ZeroComponents();
    }
}

float CVolumeAggregate::ComputeVolume() const
{
    const LinkedListNode* pCur = m_Head;
    float totalVol = 0.0f;

    while(pCur) {
        totalVol += pCur->m_Data->ComputeVolume();
        pCur = pCur->m_Next;
    }

    return totalVol;
}

bool CVolumeAggregate::FindClosestPointInVolume(Vec3V_In closestToPoint, Vec3V_InOut pointInVolumeOut) const
{
    const LinkedListNode* pCur = m_Head;
    ScalarV closestDistSq(V_FLT_LARGE_8);
    bool gotClosest= false;
    Vec3V closestTemp;

    while(pCur)
    {
        Vec3V childClosestPoint;
        if(pCur->m_Data->FindClosestPointInVolume(closestToPoint, childClosestPoint))
        {
            ScalarV distSq = DistSquared(childClosestPoint,closestToPoint);
            if(!gotClosest || IsLessThanOrEqualAll(distSq, closestDistSq))
            {
                closestTemp = childClosestPoint;
                gotClosest = true;
                closestDistSq = distSq;
            }
        }
        pCur = pCur->m_Next;
    }

    if(gotClosest)
    {
        pointInVolumeOut = closestTemp;
        return true;
    }
    return false;
}

#if __BANK
void CVolumeAggregate::RenderDebug(Color32 color, CVolume::RenderMode renderMode)
{
    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->RenderDebug(color, renderMode);
        pCur = pCur->m_Next;
    }
}

void CVolumeAggregate::ExportToFile () const
{
    //CVolumeTool::ms_ExportToFile is assumed to be a valid open fistream pointer at this point
    //CVolumeTool::ms_ExportCurVolIndex is used to keep variable names unique as we export
    Assert(CVolumeTool::ms_ExportToFile);

    int myIndex = CVolumeTool::ms_ExportCurVolIndex;
    fprintf(CVolumeTool::ms_ExportToFile, "VOLUME scrVol%d = CREATE_VOLUME_AGGREGATE ()\n", myIndex);
    CVolumeTool::ms_ExportCurVolIndex++;

    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->ExportToFile();
        fprintf(CVolumeTool::ms_ExportToFile, "ADD_TO_VOLUME_AGGREGATE (scrVol%d, scrVol%d)\n", myIndex, CVolumeTool::ms_ExportCurVolIndex-1);
        pCur = pCur->m_Next;
    }

}

void CVolumeAggregate::CopyToClipboard () const
{
    //CVolumeTool::ms_ExportCurVolIndex is used to keep variable names unique as we export
    char temp[RAGE_MAX_PATH];

    int myIndex = CVolumeTool::ms_ExportCurVolIndex;
    formatf(temp, "VOLUME scrVol%d = CREATE_VOLUME_AGGREGATE ()\n", myIndex);
    BANKMGR.CopyTextToClipboard(temp);
    CVolumeTool::ms_ExportCurVolIndex++;

    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->CopyToClipboard();
        formatf(temp, "ADD_TO_VOLUME_AGGREGATE (scrVol%d, scrVol%d)\n", myIndex, CVolumeTool::ms_ExportCurVolIndex-1);
        BANKMGR.CopyTextToClipboard(temp);
        pCur = pCur->m_Next;
    }
}

void CVolumeAggregate::Translate (Vec3V_In rTrans)
{
    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->Translate(rTrans);
        pCur = pCur->m_Next;
    }
}

void CVolumeAggregate::Scale (Vec3V_In rScale)
{
    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->Scale(rScale);
        pCur = pCur->m_Next;
    }
}

void CVolumeAggregate::Rotate (Mat33V_In rRotation)
{
    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->Rotate(rRotation);
        pCur = pCur->m_Next;
    }
}

void CVolumeAggregate::RotateAboutPoint (Mat33V_In rRotation, Vec3V_In rPoint)
{
    LinkedListNode* pCur = m_Head;
    while(pCur) {
        pCur->m_Data->RotateAboutPoint(rRotation, rPoint);
        pCur = pCur->m_Next;
    }
}

#endif

#endif // VOLUME_SUPPORT