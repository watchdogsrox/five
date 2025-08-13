/************************************************************************************************************************/
/* Terrain tessellation open edge info builder.																			*/
/************************************************************************************************************************/

#include <string.h>
#include "TerrainEdgeInfo.h"
#include "grmodel/geometry.h"

#if TEMP_RUNTIME_CALCULATE_TERRAIN_OPEN_EDGE_INFO

/****************************************************************************************************/
/* TerrainOpenEdgeInfoBuilder.																		*/
/****************************************************************************************************/

TerrainOpenEdgeInfoBuilder::TerrainOpenEdgeInfoBuilder(TriangleListInterface &triInterface, VertexInterface &vertInterface)
{
	int tri;

	// Allocate memory for vertices and all potential edges.
	m_pVertices = rage_new Vertex[vertInterface.GetNoOfVertices()];
	m_NextFreeEdge = 0;
	m_pEdges = rage_new Edge[triInterface.GetNoOfTriangles()*3];
	m_NextFreeVertexToEdgeLink = 0;
	m_pVertexToEdgeLinks = rage_new VertexToEdgeLink[triInterface.GetNoOfTriangles()*3*2];

	// Visit each triangle.
	for(tri=0; tri<triInterface.GetNoOfTriangles(); tri++)
	{
		int triEdge;
		int indices[4];

		// Collect the tri-th triangle.
		triInterface.GetTriangle(tri, indices[0], indices[1], indices[2]);
		// Make the info "wrap" round.
		indices[3] = indices[0];

		// Visit each edge of the triangle.
		for(triEdge=0; triEdge<3; triEdge++)
		{
			int v0 = indices[triEdge];
			int v1 = indices[triEdge + 1];
			int connectingEdge = m_pVertices[v0].IsConnectedToVertexByEdge(v1);

			if(connectingEdge == -1)
			{
				// Allocate a new edge.
				Edge *pNewEdge = &m_pEdges[m_NextFreeEdge];
				pNewEdge->m_Index = m_NextFreeEdge;
				pNewEdge->m_VertexIndices[0] = v0;
				pNewEdge->m_VertexIndices[1] = v1;
				pNewEdge->m_Count = 1;
				m_NextFreeEdge++;

				// Add links from each vertex to this new edge.
				VertexToEdgeLink *pLink1 = &m_pVertexToEdgeLinks[m_NextFreeVertexToEdgeLink++];
				pLink1->m_pEdge = pNewEdge;
				m_pVertices[v0].m_EdgeList.push_back(pLink1);

				VertexToEdgeLink *pLink2 = &m_pVertexToEdgeLinks[m_NextFreeVertexToEdgeLink++];
				pLink2->m_pEdge = pNewEdge;
				m_pVertices[v1].m_EdgeList.push_back(pLink2);
			}
			else
			{
				// Increment the count.
				m_pEdges[connectingEdge].m_Count++;
			}
		}
	}

	//--------------------------------------------------------------------------------------------------//

	int vrt;
	vertInterface.Begin();

	// Visit each vertex.
	for(vrt=0; vrt<vertInterface.GetNoOfVertices(); vrt++)
	{
		bool isConnectedToOpenEdge = false;
		// Visit each edge hanging off the vertex.
		EdgeList::iterator it = m_pVertices[vrt].m_EdgeList.begin();
		EdgeList::iterator end = m_pVertices[vrt].m_EdgeList.end();

		// Visit each edge used by the vertex.
		while (it != end) 
		{
			Edge *pEdge = (*it)->m_pEdge;

			if(pEdge->m_Count == 0x1)
			{
				isConnectedToOpenEdge = true;
				break;
			}
			it++;
		}
	
		if(isConnectedToOpenEdge)
		{
			vertInterface.MarkVertexAsUsingOpenEdge(vrt);
		}
		else
		{
			vertInterface.MarkVertexAsUsingOnlyInternalEdges(vrt);
		}
		// Dismantle the list of edges hanging from this vertex.
		m_pVertices[vrt].m_EdgeList.clear();
	}
	vertInterface.End();
}

TerrainOpenEdgeInfoBuilder::~TerrainOpenEdgeInfoBuilder()
{
	if(m_pVertexToEdgeLinks)
	{
		delete [] m_pVertexToEdgeLinks;
		m_pVertexToEdgeLinks = NULL;
	}
	if(m_pEdges)
	{
		delete [] m_pEdges;
		m_pEdges = NULL;
	}
	if(m_pVertices)
	{
		delete [] m_pVertices;
		m_pVertices = NULL;
	}
}

/****************************************************************************************************/
/* VertexWelder_NaiveONSquared.																		*/
/****************************************************************************************************/

VertexWelder_NaiveONSquared::VertexWelder_NaiveONSquared(VertexInterface *pVertInterface) : VertexWelder(pVertInterface)
{
	int i;
	m_NextUniqueVertex = 0;
	m_pIndexOfWeldVertices = rage_new int[m_pVertexInterface->GetNoOfVertices()];
	m_pRealToWeldVertices = rage_new int[m_pVertexInterface->GetNoOfVertices()];

	m_pVertexInterface->Begin();

	for(i=0; i<m_pVertexInterface->GetNoOfVertices(); i++)
	{
		// Check against our list of weld vertices for a match.
		int weldedIdx = FindIndexForVertex(i);

		if(weldedIdx == -1)
		{
			// It`s the 1st time we`re encountered this vertex...Record it.
			m_pIndexOfWeldVertices[m_NextUniqueVertex] = i;
			weldedIdx = m_NextUniqueVertex++;
		}
		m_pRealToWeldVertices[i] = weldedIdx;
	}

	m_pVertexInterface->End();
}


VertexWelder_NaiveONSquared::~VertexWelder_NaiveONSquared()
{
	if(m_pRealToWeldVertices)
	{
		delete [] m_pRealToWeldVertices;
		m_pRealToWeldVertices = NULL;
	}
	if(m_pIndexOfWeldVertices)
	{
		delete [] m_pIndexOfWeldVertices;
		m_pIndexOfWeldVertices = NULL;
	}
}

int VertexWelder_NaiveONSquared::FindIndexForVertex(int vertIdx)
{
	int i;

	for(i=0; i<m_NextUniqueVertex; i++)
	{
		if(m_pVertexInterface->ShouldVertsBeConsideredTheSame(vertIdx, m_pIndexOfWeldVertices[i]))
		{
			return i;
		}
	}
	return -1;
}


int VertexWelder_NaiveONSquared::NoOfWeldedVertices()
{
	return m_NextUniqueVertex;
}


int VertexWelder_NaiveONSquared::RealIndexOfWeldedVert(int idx)
{
	return m_pIndexOfWeldVertices[idx];
}


int VertexWelder_NaiveONSquared::RealIndexToWeldedIndex(int realIdx)
{
	return m_pRealToWeldVertices[realIdx];
}

//--------------------------------------------------------------------------------------------------//

class VertexInterfaceForWelder : public VertexWelder::VertexInterface
{
public:
	VertexInterfaceForWelder(grcVertexBuffer *pvertexBuffer) 
	{ 
		m_pVertexBuffer = pvertexBuffer;
	}
	~VertexInterfaceForWelder()
	{
	}
	int GetNoOfVertices()
	{
		return m_pVertexBuffer->GetVertexCount();
	}
	void Begin() 
	{
		m_pVertexData = (char *)m_pVertexBuffer->LockRO();
	}
	bool ShouldVertsBeConsideredTheSame(int a, int b)
	{
		static float normalThreshold = cosf((1.0f/180.0f)*3.141592654f);

		// Position.
		Vector3 v3Diff = v3Get(a, 0) - v3Get(b, 0);
		if(v3Diff.Dot(v3Diff) > (0.05f*0.05f))
		{
			return false;
		}

		// Normal.
		Vector3 nA = v3Get(a, 3);
		Vector3 nB = v3Get(b, 3);
		if(IsInnerProductBetweenUnitVectorLessThan(nA, nB, normalThreshold))
		{
			return false;
		}

		return true;
	}
	void End()
	{
		m_pVertexBuffer->UnlockRO();
	}

public:
	Vector4 v4Get(int idx, int offsetInfloats)
	{
		float *pV = &((float *)(m_pVertexData + idx*m_pVertexBuffer->GetVertexStride()))[offsetInfloats];
		return Vector3(pV[0], pV[1], pV[2], pV[3]);
	}
	Vector3 v3Get(int idx, int offsetInfloats)
	{
		float *pV = &((float *)(m_pVertexData + idx*m_pVertexBuffer->GetVertexStride()))[offsetInfloats];
		return Vector3(pV[0], pV[1], pV[2]);
	}
	int iGet(int idx, int offsetInfloats)
	{
		int *pV = &((int *)(m_pVertexData + idx*m_pVertexBuffer->GetVertexStride()))[offsetInfloats];
		return pV[0];
	}
	float fGet(int idx, int offsetInfloats)
	{
		float *pV = &((float *)(m_pVertexData + idx*m_pVertexBuffer->GetVertexStride()))[offsetInfloats];
		return pV[0];
	}

	float GetMax(float *pV, int count)
	{
		float ret = pV[0];

		for(int i=1; i<count; i++)
		{
			if(pV[i] > ret)
			{
				ret = pV[i];
			}
		}
		return ret;
	}

	float IsInnerProductBetweenUnitVectorLessThan(Vector3 &A, Vector3 &B, float threshold)
	{
		float ADotB = A.Dot(B);

		// Assume threshold is > 0.
		if(ADotB < 0.0f)
		{
			return true;
		}
		float ModASqr = A.Dot(A);
		float ModBSqr = B.Dot(B);

		if(ADotB*ADotB < threshold*threshold*ModASqr*ModBSqr)
		{
			return true;
		}
		return false;
	}
private:
	char *m_pVertexData;
	grcVertexBuffer *m_pVertexBuffer;
};

/****************************************************************************************************/
/* Build open edge info.																			*/
/****************************************************************************************************/

void BuildTerrainOpenEdgeInfo(grmGeometry &geometry)
{
	class grcVertexBuffer_EIBVertexInterface : public TerrainOpenEdgeInfoBuilder::VertexInterface
	{
	public:
		grcVertexBuffer_EIBVertexInterface(grcVertexBuffer *pvertexBuffer) : TerrainOpenEdgeInfoBuilder::VertexInterface()
		{
			m_pVertexBuffer = pvertexBuffer;
		}
		~grcVertexBuffer_EIBVertexInterface()
		{
		}
	public:
		int GetNoOfVertices()
		{
			return m_pVertexBuffer->GetVertexCount();
		}

		void Begin()
		{
			m_pVertData = (char *)m_pVertexBuffer->LockRO();
		}
		void End()
		{
			m_pVertexBuffer->UnlockRO();
		#if __D3D11
			static_cast<grcVertexBufferD3D11*>(m_pVertexBuffer)->RecreateInternal();
		#else
			// Need to implement RecreateInternal() on other platforms.
			CompileTimeAssert(0);
		#endif
		}

		void MarkVertexAsUsingOpenEdge(int idx)
		{
			float *pV = (float *)(m_pVertData + idx*m_pVertexBuffer->GetVertexStride());
			pV[2] -= TERRAIN_OPEN_EDGE_INFO_COMMON_FLOOR;
			pV[2] *= -1.0f;
		}
		void MarkVertexAsUsingOnlyInternalEdges(int idx)
		{
			float *pV = (float *)(m_pVertData + idx*m_pVertexBuffer->GetVertexStride());
			pV[2] -= TERRAIN_OPEN_EDGE_INFO_COMMON_FLOOR;
		}
	public:
		char *m_pVertData;
		grcVertexBuffer *m_pVertexBuffer;
	};

	//--------------------------------------------------------------------------------------------------//
	
	class grcIndexBuffer_EIBTriangleListInterface : public TerrainOpenEdgeInfoBuilder::TriangleListInterface
	{
	public:
		grcIndexBuffer_EIBTriangleListInterface(grcIndexBuffer *pindexBuffer) : TerrainOpenEdgeInfoBuilder::TriangleListInterface()
		{
			m_pIndexBuffer = pindexBuffer;
			m_pTriIndices = (u16 *)m_pIndexBuffer->LockRO();
		}
		~grcIndexBuffer_EIBTriangleListInterface()
		{
			m_pIndexBuffer->UnlockRO();
		}
	public:
		int GetNoOfTriangles()
		{
			// Assume triangle index buffer.
			return m_pIndexBuffer->GetIndexCount()/3;
		}
		void GetTriangle(int idx, int &a, int &b, int &c)
		{
			u16 *pTri = &m_pTriIndices[idx*3];
			a = (int)pTri[0];
			b = (int)pTri[1];
			c = (int)pTri[2];
		}
	public:
		u16 *m_pTriIndices;
		grcIndexBuffer *m_pIndexBuffer;
	};

	//--------------------------------------------------------------------------------------------------//

	class EIBVertexInterface_ThroughWelder : public TerrainOpenEdgeInfoBuilder::VertexInterface
	{
	public:
		EIBVertexInterface_ThroughWelder(grcVertexBuffer_EIBVertexInterface *pBaseInterface, VertexWelder *pWelder)
		{
			m_pBaseInterface = pBaseInterface;
			m_pWelder = pWelder;
		}
		~EIBVertexInterface_ThroughWelder()
		{
		}

	public:
		int GetNoOfVertices()
		{
			return m_pWelder->NoOfWeldedVertices();
		}
		void Begin()
		{
			// Make space for the edge info, one per welded vertex.
			m_pVertexOpenEdgeInfo = rage_new bool[GetNoOfVertices()];
		}
		void MarkVertexAsUsingOpenEdge(int idx)
		{
			m_pVertexOpenEdgeInfo[idx] = true;
		}
		void MarkVertexAsUsingOnlyInternalEdges(int idx)
		{
			m_pVertexOpenEdgeInfo[idx] = false;
		}
		void End()
		{
			int realVtx;
			m_pBaseInterface->Begin();

			// Visit each of the real vertices...
			for(realVtx=0; realVtx<m_pBaseInterface->GetNoOfVertices(); realVtx++)
			{
				//...And read the edge info for the welded vertex it resolves to.
				int idx = m_pWelder->RealIndexToWeldedIndex(realVtx);

				if(m_pVertexOpenEdgeInfo[idx] == true)
				{
					m_pBaseInterface->MarkVertexAsUsingOpenEdge(realVtx);
				}
				else
				{
					m_pBaseInterface->MarkVertexAsUsingOnlyInternalEdges(realVtx);
				}
			}
			m_pBaseInterface->End();

			delete [] m_pVertexOpenEdgeInfo;
			m_pVertexOpenEdgeInfo = NULL;
		}
	private:
		VertexWelder *m_pWelder;
		grcVertexBuffer_EIBVertexInterface *m_pBaseInterface;
		bool *m_pVertexOpenEdgeInfo;
	};

	class EIBTriangleListInterface_ThroughWelder : public TerrainOpenEdgeInfoBuilder::TriangleListInterface
	{
	public:
		EIBTriangleListInterface_ThroughWelder(grcIndexBuffer_EIBTriangleListInterface *pBaseInterface, VertexWelder *pWelder)
		{
			m_pBaseInterface = pBaseInterface;
			m_pWelder = pWelder;
		}
		~EIBTriangleListInterface_ThroughWelder()
		{
		}
	public:
		int GetNoOfTriangles()
		{
			return m_pBaseInterface->GetNoOfTriangles();
		}
		void GetTriangle(int idx, int &a, int &b, int &c)
		{
			int realIndices[3];
			int weldedIndices[3];

			// Get real indices...
			m_pBaseInterface->GetTriangle(idx, realIndices[0], realIndices[1], realIndices[2]);
			//..Convert to welded ones.
			weldedIndices[0] = m_pWelder->RealIndexToWeldedIndex(realIndices[0]);
			weldedIndices[1] = m_pWelder->RealIndexToWeldedIndex(realIndices[1]);
			weldedIndices[2] = m_pWelder->RealIndexToWeldedIndex(realIndices[2]);

			a = weldedIndices[0];
			b = weldedIndices[1];
			c = weldedIndices[2];
		}

	private:
		grcIndexBuffer_EIBTriangleListInterface *m_pBaseInterface;
		VertexWelder *m_pWelder;
	};

	//--------------------------------------------------------------------------------------------------//

	AssertMsg(geometry.GetIndexBuffer(), "BuildTerrainOpenEdgeInfo()...Expects indexed geometry.");

	VertexInterfaceForWelder vertexInferfaceForWelder(geometry.GetVertexBuffer());
	//DummyVertexWelder dummyVertexWelder(&vertexInferfaceForWelder);
	VertexWelder_NaiveONSquared dummyVertexWelder(&vertexInferfaceForWelder);
	Printf("BuildTerrainOpenEdgeInfo()...Before weld = %d, after = %d\n", vertexInferfaceForWelder.GetNoOfVertices(), dummyVertexWelder.NoOfWeldedVertices());

	grcVertexBuffer_EIBVertexInterface baseVertInterface(geometry.GetVertexBuffer());
	grcIndexBuffer_EIBTriangleListInterface baseTriInterface(geometry.GetIndexBuffer());
	
	{
		EIBTriangleListInterface_ThroughWelder triInterface(&baseTriInterface, &dummyVertexWelder);
		{
			EIBVertexInterface_ThroughWelder vertInterface(&baseVertInterface, &dummyVertexWelder);
			{
				TerrainOpenEdgeInfoBuilder builder(triInterface, vertInterface);
			}
		}
	}
}

#endif //TEMP_RUNTIME_CALCULATE_TERRAIN_OPEN_EDGE_INFO

/****************************************************************************************************/
/* Applies welding to the passed geometry.															*/
/****************************************************************************************************/

#if 0

void Applywelding(grmGeometry &geometry, VertexWelder *pWelder)
{
#if __D3D11
	int vrt;
	grcVertexBufferD3D11 *pVB = static_cast <grcVertexBufferD3D11 *>(geometry.GetVertexBuffer());
	grcIndexBufferD3D11 *pIB = static_cast <grcIndexBufferD3D11 *>(geometry.GetIndexBuffer());

	char *pSourceVertData = (char *)pVB->LockRO();
	char *pUniqueVertices = rage_new char[pVB->GetVertexStride()*pWelder->NoOfWeldedVertices()];
	char *pDest = pUniqueVertices;

	// Form a linear list of the welded vertices.
	for(vrt=0; vrt<pWelder->NoOfWeldedVertices(); vrt++)
	{
		int idx = pWelder->RealIndexOfWeldedVert(vrt);
		char *pSrc = &pSourceVertData[idx*pVB->GetVertexStride()];
		memcpy((void *)pDest, (void *)pSrc, sizeof(char)*pVB->GetVertexStride());
		pDest += pVB->GetVertexStride();
	}

	// Over-write the initial portion of the vertex data with the unique/welded ones.
	memcpy((void *)pSourceVertData, (void *)pUniqueVertices, pWelder->NoOfWeldedVertices()*pVB->GetVertexStride());

	pVB->UnlockRO();
	pVB->RecreateInternal();
	delete [] pUniqueVertices;

	//--------------------------------------------------------------------------------------------------//

	int idx;
	u16 *pIndexData = (u16 *)pIB->LockRO();

	// Replace each index with it`s welded version.
	for(idx=0; idx<pIB->GetIndexCount(); idx++)
	{
		int newIdx = pWelder->RealIndexToWeldedIndex((int)pIndexData[idx]);
		pIndexData[idx] = (u16)newIdx;
	}

	pIB->UnlockRO();
	pIB->RecreateInternal();
#endif //__D3D11
}

/****************************************************************************************************/
/* General Fvf vertex welder.																		*/
/****************************************************************************************************/

class FvfVertexInterfaceForWelder : public VertexWelder::VertexInterface
{
public:
	enum CompareOp
	{
		LengthOp = 0,
		InnerProductOp,
		InnerProductPlusWOp,
		MaxComponentOp,
		StraightCompareOp
	};

	FvfVertexInterfaceForWelder(grcVertexBuffer *pvertexBuffer) 
	{ 
		m_BadVertexTypeEncountered = false;
		m_pVertexBuffer = pvertexBuffer;
		m_pFvf = m_pVertexBuffer->GetFvf();
	}
	~FvfVertexInterfaceForWelder()
	{
	}
	int GetNoOfVertices()
	{
		return m_pVertexBuffer->GetVertexCount();
	}
	void Begin() 
	{
		m_pVertexData = (char *)m_pVertexBuffer->LockRO();
	}

	void GetSizeOfIntOrFloat(int a, int b, rage::grcFvf::grcFvfChannels channel, int count, float *pDestA, float *pDestB)
	{
		int i;
		float *pVA = (float *)(m_pVertexData + a*m_pVertexBuffer->GetVertexStride() + m_pFvf->GetOffset(channel));
		float *pVB = (float *)(m_pVertexData + b*m_pVertexBuffer->GetVertexStride() + m_pFvf->GetOffset(channel));

		for(i=0; i<count; i++)
		{
			pDestA[i] = pVA[i];
			pDestB[i] = pVB[i];
		}
	}
	int GetChannelData(int a, int b, rage::grcFvf::grcFvfChannels channel, bool &isInteger, void *pDestA, void *pDestB)
	{
		int dataCount = 0;
		isInteger = false;

		switch(m_pFvf->GetDataSizeType(channel))
		{
		case rage::grcFvf::grcdsHalf:
		case rage::grcFvf::grcdsHalf2:
		case rage::grcFvf::grcdsHalf3:
		case rage::grcFvf::grcdsHalf4:
			{
				return -1;
			}
		case rage::grcFvf::grcdsFloat4:
			{
				dataCount++;
			}
		case rage::grcFvf::grcdsFloat3:
			{
				dataCount++;
			}
		case rage::grcFvf::grcdsFloat2:
			{
				dataCount++;
			}
		case rage::grcFvf::grcdsFloat:
			{
				dataCount++;
				GetSizeOfIntOrFloat(a, b, channel, dataCount, (float *)pDestA,  (float *)pDestB);
				break;
			}
		case rage::grcFvf::grcdsUBYTE4:
		case rage::grcFvf::grcdsColor:
			{
				dataCount = 1;
				isInteger = true;
				GetSizeOfIntOrFloat(a, b, channel, 1, (float *)pDestA,  (float *)pDestB);
				break;
			}
		case rage::grcFvf::grcdsPackedNormal:
		case rage::grcFvf::grcdsEDGE0:
		case rage::grcFvf::grcdsEDGE1:
		case rage::grcFvf::grcdsEDGE2:
		case rage::grcFvf::grcdsShort2:
		case rage::grcFvf::grcdsShort4:
		case rage::grcFvf::grcdsCount:
			{
				return -1;
			}
		}
		return dataCount;
	}

	bool AreChannelsTheSame(int a, int b, rage::grcFvf::grcFvfChannels channel, float epsilon, CompareOp compareOp)
	{
		if(m_pFvf->IsChannelActive(channel) == false)
		{
			return true;
		}
		float vFloatsA[4];
		float vFloatsB[4];
		bool isInteger = false;
		int noOfComponents = GetChannelData(a, b, channel, isInteger, (void *)vFloatsA, (void *)vFloatsB);

		if(noOfComponents == -1)
		{
			m_BadVertexTypeEncountered = true;
			return false;
		}

		switch(compareOp)
		{
		case LengthOp:
			{
				int i;
				float sum = 0.0f;

				if(isInteger == true)
				{
					m_BadVertexTypeEncountered = true;
					return false;
				}

				for(i=0; i<noOfComponents; i++)
				{
					float diff = vFloatsA[i] - vFloatsB[i];
					sum += diff*diff;
				}
				if(sum < epsilon*epsilon)
				{
					return true;
				}
				return false;
			}
		case InnerProductOp:
			{
				if((isInteger == true) || (noOfComponents == 1) || (noOfComponents == 4))
				{
					m_BadVertexTypeEncountered = true;
					return false;
				}
				if(noOfComponents == 2)
				{
					Vector2 A(vFloatsA[0], vFloatsA[1]);
					Vector2 B(vFloatsB[0], vFloatsB[1]);
					return !IsInnerProductBetweenUnitVectorLessThan(A, B, epsilon);
				}
				else
				{
					Vector3 A(vFloatsA[0], vFloatsA[1], vFloatsA[2]);
					Vector3 B(vFloatsB[0], vFloatsB[1], vFloatsB[2]);
					return !IsInnerProductBetweenUnitVectorLessThan(A, B, epsilon);
				}
			}
		case InnerProductPlusWOp:
			{
				if((isInteger == true) || (noOfComponents != 4))
				{
					m_BadVertexTypeEncountered = true;
					return false;
				}

				if(vFloatsA[3] != vFloatsB[3])
				{
					return false;
				}
				Vector3 A(vFloatsA[0], vFloatsA[1], vFloatsA[2]);
				Vector3 B(vFloatsB[0], vFloatsB[1], vFloatsB[2]);
				return !IsInnerProductBetweenUnitVectorLessThan(A, B, epsilon);
			}
		case MaxComponentOp:
			{
				int i;
				float max = -FLT_MAX;

				if(isInteger == true)
				{
					m_BadVertexTypeEncountered = true;
					return false;
				}

				for(i=0; i<noOfComponents; i++)
				{
					float diff = fabs(vFloatsA[i] - vFloatsB[i]);

					if(diff > max)
					{
						max = diff;
					}
				}
				if(max > epsilon)
				{
					return false;
				}
				return true;
			}
		case StraightCompareOp:
			{
				if(noOfComponents != 1)
				{
					m_BadVertexTypeEncountered = true;
					return false;
				}
				if(isInteger)
				{
					int *piA = (int *)vFloatsA;
					int *piB = (int *)vFloatsB;
					return (piA[0] == piB[0]);
				}
				else
				{
					return (vFloatsA[0] == vFloatsB[0]);
				}
			}
		}
		return true;
	}

	bool ShouldVertsBeConsideredTheSame(int a, int b)
	{
		static float lengthThreshold = 0.001f;
		static float normalThreshold = cosf((1.0f/180.0f)*3.141592654f);
		static float UVThreshold = 1.0f/2048.0f;
		static float tangentThreshold = cosf((1.0f/180.0f)*3.141592654f);

		// grcfcPosition.
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcPosition, lengthThreshold, LengthOp))
		{
			return false;
		}

		// grcfcWeight & grcfcBinding.
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcWeight, 0.0f, StraightCompareOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcBinding, 0.0f, StraightCompareOp))
		{
			return false;
		}

		// grcfcNormal. 
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcNormal, normalThreshold, InnerProductOp))
		{
			return false;
		}

		// grcfcDiffuse & grcfcSpecular.
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcDiffuse, 0.0f, StraightCompareOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcSpecular, 0.0f, StraightCompareOp))
		{
			return false;
		}

		// grcfcTexture0 - grcfcTexture7
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture0, UVThreshold, MaxComponentOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture1, UVThreshold, MaxComponentOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture2, UVThreshold, MaxComponentOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture3, UVThreshold, MaxComponentOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture4, UVThreshold, MaxComponentOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture5, UVThreshold, MaxComponentOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture6, UVThreshold, MaxComponentOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTexture7, UVThreshold, MaxComponentOp))
		{
			return false;
		}

		// grcfcTangent0 - grcfcTangent1
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTangent0, normalThreshold, InnerProductPlusWOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcTangent1, normalThreshold, InnerProductPlusWOp))
		{
			return false;
		}

		// grcfcBinormal0 - grcfcBinormal1
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcBinormal0, normalThreshold, InnerProductOp))
		{
			return false;
		}
		if(!AreChannelsTheSame(a, b, rage::grcFvf::grcfcBinormal0, normalThreshold, InnerProductOp))
		{
			return false;
		}
		return true;
	}
	void End()
	{
		m_pVertexBuffer->UnlockRO();
	}

public:
	float IsInnerProductBetweenUnitVectorLessThan(Vector3 &A, Vector3 &B, float threshold)
	{
		float ADotB = A.Dot(B);

		// Assume threshold is > 0.
		if(ADotB < 0.0f)
		{
			return true;
		}
		float ModASqr = A.Dot(A);
		float ModBSqr = B.Dot(B);

		if(ADotB*ADotB < threshold*threshold*ModASqr*ModBSqr)
		{
			return true;
		}
		return false;
	}

	float IsInnerProductBetweenUnitVectorLessThan(Vector2 &A, Vector2 &B, float threshold)
	{
		float ADotB = A.Dot(B);

		// Assume threshold is > 0.
		if(ADotB < 0.0f)
		{
			return true;
		}
		float ModASqr = A.Dot(A);
		float ModBSqr = B.Dot(B);

		if(ADotB*ADotB < threshold*threshold*ModASqr*ModBSqr)
		{
			return true;
		}
		return false;
	}

	bool IsGood()
	{
		return !m_BadVertexTypeEncountered;
	}
private:
	char *m_pVertexData;
	grcVertexBuffer *m_pVertexBuffer;
	const rage::grcFvf *m_pFvf;
	bool m_BadVertexTypeEncountered;
};


void TestWeldLandscape(grmGeometry &geometry)
{
	/*
	{
		VertexInterfaceForWelder vertexInferfaceForWelder(geometry.GetVertexBuffer());
		VertexWelder_NaiveONSquared dummyVertexWelder(&vertexInferfaceForWelder);
		Printf("BuildTerrainOpenEdgeInfo()...Before weld = %d, after = %d\n", vertexInferfaceForWelder.GetNoOfVertices(), dummyVertexWelder.NoOfWeldedVertices());
	}
	*/
	{
		FvfVertexInterfaceForWelder vertexInferfaceForWelder(geometry.GetVertexBuffer());
		VertexWelder_NaiveONSquared dummyVertexWelder(&vertexInferfaceForWelder);

		if(vertexInferfaceForWelder.IsGood())
		{
			Printf("TestWeldLandscape()...Before weld = %d, after = %d\n", vertexInferfaceForWelder.GetNoOfVertices(), dummyVertexWelder.NoOfWeldedVertices());
		}
	}
}


bool TestWeld(grmGeometry &geometry)
{
	bool ret = false;
	/*
	{
		VertexInterfaceForWelder vertexInferfaceForWelder(geometry.GetVertexBuffer());
		VertexWelder_NaiveONSquared dummyVertexWelder(&vertexInferfaceForWelder);
		Printf("BuildTerrainOpenEdgeInfo()...Before weld = %d, after = %d\n", vertexInferfaceForWelder.GetNoOfVertices(), dummyVertexWelder.NoOfWeldedVertices());
	}
	*/
	//if(geometry.GetVertexBuffer()->GetFvf()->IsChannelActive(rage::grcFvf::grcfcTangent0) || geometry.GetVertexBuffer()->GetFvf()->IsChannelActive(rage::grcFvf::grcfcTangent1))
	{
		FvfVertexInterfaceForWelder vertexInferfaceForWelder(geometry.GetVertexBuffer());
		VertexWelder_NaiveONSquared dummyVertexWelder(&vertexInferfaceForWelder);

		if(vertexInferfaceForWelder.IsGood())
		{
			if((float)dummyVertexWelder.NoOfWeldedVertices()/(float)vertexInferfaceForWelder.GetNoOfVertices() < 0.9f)
			{
				Printf("TestWeld()...Before weld = %d, after = %d\n", vertexInferfaceForWelder.GetNoOfVertices(), dummyVertexWelder.NoOfWeldedVertices());
				ret = true;
			}
		}
	}
	return ret;
}

#endif // 0