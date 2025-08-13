/************************************************************************************************************************/
/* Terrain tessellation open edge info builder.																			*/
/************************************************************************************************************************/

#ifndef __TERRAIN_EDGE_INFO_H
#define __TERRAIN_EDGE_INFO_H

#include "grcore/config_switches.h"

#if TEMP_RUNTIME_CALCULATE_TERRAIN_OPEN_EDGE_INFO

#include "shader_source/Terrain/terrain_cb_switches.fxh"

/************************************************************************************************************************/
/* Vertex welding classes.																								*/
/************************************************************************************************************************/

class VertexWelder
{
public:
	class VertexInterface
	{
	public:
		VertexInterface() {};
		virtual ~VertexInterface(){};
		virtual int GetNoOfVertices() = 0;
		virtual void Begin() = 0;
		virtual bool ShouldVertsBeConsideredTheSame(int a, int b) = 0;
		virtual void End() = 0;
	};

/*--------------------------------------------------------------------------------------------------*/
/* Constructors/Destructor.																			*/
/*--------------------------------------------------------------------------------------------------*/

public:
	VertexWelder(VertexInterface *pVertInterface) { m_pVertexInterface = pVertInterface; }
	virtual ~VertexWelder() {}

public:
	virtual int NoOfWeldedVertices() = 0;
	virtual int RealIndexOfWeldedVert(int idx) = 0;
	virtual int RealIndexToWeldedIndex(int realIdx) = 0;

/*--------------------------------------------------------------------------------------------------*/
/* Data members.																					*/
/*--------------------------------------------------------------------------------------------------*/

protected:
	VertexInterface *m_pVertexInterface;
};

class DummyVertexWelder : public VertexWelder
{
public:
	DummyVertexWelder(VertexInterface *pVertInterface) : VertexWelder(pVertInterface) {}
	virtual ~DummyVertexWelder() {};
public:
	int NoOfWeldedVertices() { return m_pVertexInterface->GetNoOfVertices(); }
	int RealIndexOfWeldedVert(int idx) { return idx; }
	int RealIndexToWeldedIndex(int realIdx) { return realIdx; }
};


class VertexWelder_NaiveONSquared : public VertexWelder
{
/*--------------------------------------------------------------------------------------------------*/
/* Constructors/Destructor.																			*/
/*--------------------------------------------------------------------------------------------------*/

public:
	VertexWelder_NaiveONSquared(VertexInterface *pVertInterface);
	~VertexWelder_NaiveONSquared();
	int FindIndexForVertex(int vertIdx);
public:
	int NoOfWeldedVertices();
	int RealIndexOfWeldedVert(int idx);
	int RealIndexToWeldedIndex(int realIdx);

/*--------------------------------------------------------------------------------------------------*/
/* Data members.																					*/
/*--------------------------------------------------------------------------------------------------*/

protected:
	int m_NextUniqueVertex;
	int *m_pIndexOfWeldVertices;
	int *m_pRealToWeldVertices;
};


/************************************************************************************************************************/
/* Open edge info building class.																						*/
/************************************************************************************************************************/

class TerrainOpenEdgeInfoBuilder
{
private:
	class Edge
	{
	public:
		Edge()
		{
			m_Index = -1;
			m_VertexIndices[0] = -1;
			m_VertexIndices[1] = -1;
			m_Count = 0;
		}
		~Edge()
		{
		}
	public:
		bool DoesUseVertex(int v)
		{
			if((m_VertexIndices[0] == v) || (m_VertexIndices[1] == v))
			{
				return true;
			}
			return false;
		}
	public:
		// Index of the edge.
		int m_Index;
		// The two vertices of the edge.
		int m_VertexIndices[2];
		// Edge multiplicity.
		int m_Count;
	};

	//--------------------------------------------------------------------------------------------------//

	class VertexToEdgeLink
	{
	public:
		VertexToEdgeLink(){}
		~VertexToEdgeLink(){};
	public:
		// Pointer to the edge the vertex uses.
		Edge *m_pEdge;
		// List node.
		inlist_node<VertexToEdgeLink> m_ListLink;
	};

	typedef inlist<VertexToEdgeLink, &VertexToEdgeLink::m_ListLink> EdgeList;

	class Vertex
	{
	public:
		Vertex() {}
		~Vertex() {}
	public:
		int IsConnectedToVertexByEdge(int idx)
		{
			EdgeList::iterator it = m_EdgeList.begin();
			EdgeList::iterator end = m_EdgeList.end();

			// Visit each edge used by the vertex.
			while (it != end) 
			{
				Edge *pEdge = (*it)->m_pEdge;

				if(pEdge->DoesUseVertex(idx))
				{
					return pEdge->m_Index;
				}
				it++;
			}
			return -1;
		}
	public:
		// List of edges using this vertex.
		EdgeList m_EdgeList;
	};

	//--------------------------------------------------------------------------------------------------//

public:
	class VertexInterface
	{
	public:
		VertexInterface() {};
		virtual ~VertexInterface(){};
		virtual int GetNoOfVertices() = 0;
		virtual void Begin() = 0;
		virtual void MarkVertexAsUsingOpenEdge(int idx) = 0;
		virtual void MarkVertexAsUsingOnlyInternalEdges(int idx) = 0;
		virtual void End() = 0;
	};

	class TriangleListInterface
	{
	public:
		TriangleListInterface() {};
		virtual ~TriangleListInterface(){};
		virtual int GetNoOfTriangles() = 0;
		virtual void GetTriangle(int idx, int &a, int &b, int &c) = 0;
	};

/*--------------------------------------------------------------------------------------------------*/
/* Constructors/Destructor.																			*/
/*--------------------------------------------------------------------------------------------------*/

public:
	TerrainOpenEdgeInfoBuilder(TriangleListInterface &triInterface, VertexInterface &vertInterface);
	~TerrainOpenEdgeInfoBuilder();

/*--------------------------------------------------------------------------------------------------*/
/* Data members.																					*/
/*--------------------------------------------------------------------------------------------------*/

private:
	Vertex *m_pVertices;
	int m_NextFreeEdge;
	Edge *m_pEdges;
	int m_NextFreeVertexToEdgeLink;
	VertexToEdgeLink *m_pVertexToEdgeLinks;
};

#endif // TEMP_RUNTIME_CALCULATE_TERRAIN_OPEN_EDGE_INFO
#endif //__TERRAIN_EDGE_INFO_H
