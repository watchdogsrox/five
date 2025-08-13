#include "atl/slist.h"

typedef struct {
  double x, y;
} point_t, vector_t;


/* Segment attributes */

typedef struct {	
  point_t v0, v1;		/* two endpoints */
  int is_inserted;		/* inserted in trapezoidation yet ? */
  int root0, root1;		/* root nodes in Q */
  int next;			/* Next logical segment */
  int prev;			/* Previous segment */
} segment_t;


/* Trapezoid attributes */

typedef struct {
  int lseg, rseg;		/* two adjoining segments */
  point_t hi, lo;		/* max/min y-values */
  int u0, u1;
  int d0, d1;
  int sink;			/* pointer to corresponding in Q */
  int usave, uside;		/* I forgot what this means */
  int state;
} trap_t;


/* Node attributes for every node in the query structure */

typedef struct {
  int nodetype;			/* Y-node or S-node */
  int segnum;
  point_t yval;
  int trnum;
  int parent;			/* doubly linked DAG */
  int left, right;		/* children */
} node_t;


typedef struct {
  int vnum;
  int next;			/* Circularly linked list  */
  int prev;			/* describing the monotone */
  int marked;			/* polygon */
} monchain_t;			


typedef struct {
  point_t pt;
  int vnext[4];			/* next vertices for the 4 chains */
  int vpos[4];			/* position of v in the 4 chains */
  int nextfree;
} vertexchain_t;


/* Node types */

#define T_X     1
#define T_Y     2
#define T_SINK  3


#define SEGSIZE 200		/* max# of segments. Determines how */
				/* many points can be specified as */
				/* input. If your datasets have large */
				/* number of points, increase this */
				/* value accordingly. */

#define QSIZE   8*SEGSIZE	/* maximum table sizes */
#define TRSIZE  4*SEGSIZE	/* max# trapezoids */


#define TRUE  1
#define FALSE 0


#define FIRSTPT 1		/* checking whether pt. is inserted */ 
#define LASTPT  2


#define C_EPS 1.0e-7		/* tolerance value: Used for making */
				/* all decisions about collinearity or */
				/* left/right of segment. Decrease */
				/* this value if the input points are */
				/* spaced very close together */


#define S_LEFT 1		/* for merge-direction */
#define S_RIGHT 2


#define ST_VALID 1		/* for trapezium state */
#define ST_INVALID 2


#define SP_SIMPLE_LRUP 1	/* for splitting trapezoids */
#define SP_SIMPLE_LRDN 2
#define SP_2UP_2DN     3
#define SP_2UP_LEFT    4
#define SP_2UP_RIGHT   5
#define SP_2DN_LEFT    6
#define SP_2DN_RIGHT   7
#define SP_NOSPLIT    -1	

#define TR_FROM_UP 1		/* for traverse-direction */
#define TR_FROM_DN 2

#define TRI_LHS 1
#define TRI_RHS 2


#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define CROSS(v0, v1, v2) (((v1).x - (v0).x)*((v2).y - (v0).y) - ((v1).y - (v0).y)*((v2).x - (v0).x))

#define DOT(v0, v1) ((v0).x * (v1).x + (v0).y * (v1).y)

#define FP_EQUAL(s, t) (fabs(s - t) <= C_EPS)



#define CROSS_SINE(v0, v1) ((v0).x * (v1).y - (v1).x * (v0).y)
#define LENGTH(v0) (sqrt((v0).x * (v0).x + (v0).y * (v0).y))

static monchain_t mchain[TRSIZE]; /* Table to hold all the monotone */
				  /* polygons . Each monotone polygon */
				  /* is a circularly linked list */

static vertexchain_t vert[SEGSIZE]; /* chain init. information. This */
				    /* is used to decide which */
				    /* monotone polygon to split if */
				    /* there are several other */
				    /* polygons touching at the same */
				    /* vertex  */

static int mon[SEGSIZE];	/* contains position of any vertex in */
				/* the monotone chain for the polygon */
static int visited[TRSIZE];
static int chain_idx, op_idx, mon_idx;


static int triangulate_single_polygon(int, int, int, int (*)[3]);
static int traverse_polygon(int, int, int, int);


node_t qs[QSIZE];		/* Query structure */
trap_t tr[TRSIZE];		/* Trapezoid structure */
segment_t seg[SEGSIZE];		/* Segment table */

static int q_idx;
static int tr_idx;

//#ifdef __STDC__
//extern double log2(double);
//#else
//extern double log2();
//#endif

static int choose_idx;
static int permute[SEGSIZE];


/* Generate a random permutation of the segments 1..n */
int generate_random_ordering(int n)
{
	register int i;
	int m, st[SEGSIZE], *p;

	choose_idx = 1;

	for (i = 0; i <= n; i++)
		st[i] = i;

	p = st;
	for (i = 1; i <= n; i++, p++)
	{
		m = fwRandom::GetRandomNumber() % (n + 1 - i) + 1;
		permute[i] = p[m];
		if (m != 1)
			p[m] = p[1];
	}
	return 0;
}

  
/* Return the next segment in the generated random ordering of all the */
/* segments in S */
int choose_segment()
{
  //int i;

#ifdef DEBUG
  fprintf(stderr, "choose_segment: %d\n", permute[choose_idx]);
#endif 
  return permute[choose_idx++];
}


#ifdef STANDALONE

/* Read in the list of vertices from infile */
int read_segments(filename, genus)
     char *filename;
     int *genus;
{
  FILE *infile;
  int ccount;
  register int i;
  int ncontours, npoints, first, last;

  if ((infile = fopen(filename, "r")) == NULL)
    {
      perror(filename);
      return -1;
    }

  fscanf(infile, "%d", &ncontours);
  if (ncontours <= 0)
    return -1;
  
  /* For every contour, read in all the points for the contour. The */
  /* outer-most contour is read in first (points specified in */
  /* anti-clockwise order). Next, the inner contours are input in */
  /* clockwise order */

  ccount = 0;
  i = 1;
  
  while (ccount < ncontours)
    {
      int j;

      fscanf(infile, "%d", &npoints);
      first = i;
      last = first + npoints - 1;
      for (j = 0; j < npoints; j++, i++)
	{
	  fscanf(infile, "%lf%lf", &seg[i].v0.x, &seg[i].v0.y);
	  if (i == last)
	    {
	      seg[i].next = first;
	      seg[i].prev = i-1;
	      seg[i-1].v1 = seg[i].v0;
	    }
	  else if (i == first)
	    {
	      seg[i].next = i+1;
	      seg[i].prev = last;
	      seg[last].v1 = seg[i].v0;
	    }
	  else
	    {
	      seg[i].prev = i-1;
	      seg[i].next = i+1;
	      seg[i-1].v1 = seg[i].v0;
	    }
	  
	  seg[i].is_inserted = FALSE;
	}

      ccount++;
    }

  *genus = ncontours - 1;
  return i-1;
}

#endif


/* Get log*n for given n */
int math_logstar_n(int n)
{
  register int i;
  double v;
  
  for (i = 0, v = (double) n; v >= 1; i++)
    v = log2(v);
  
  return (i - 1);
}
  

int math_N(int n,int h)
{
  register int i;
  double v;

  for (i = 0, v = (int) n; i < h; i++)
    v = log2(v);
  
  return (int) ceil((double) 1.0*n/v);
}

/* Return a new node to be added into the query tree */
static int newnode()
{
  if (q_idx < QSIZE)
    return q_idx++;
  else
    {
      fprintf(stderr, "newnode: Query-table overflow\n");
      return -1;
    }
}

/* Return a free trapezoid */
static int newtrap()
{
  if (tr_idx < TRSIZE)
    {
      tr[tr_idx].lseg = -1;
      tr[tr_idx].rseg = -1;
      tr[tr_idx].state = ST_VALID;
      return tr_idx++;
    }
  else
    {
      fprintf(stderr, "newtrap: Trapezoid-table overflow\n");
      return -1;
    }
}


/* Return the maximum of the two points into the yval structure */
static int _max(
     point_t *yval,
     point_t *v0,
     point_t *v1)
{
  if (v0->y > v1->y + C_EPS)
    *yval = *v0;
  else if (FP_EQUAL(v0->y, v1->y))
    {
      if (v0->x > v1->x + C_EPS)
	*yval = *v0;
      else
	*yval = *v1;
    }
  else
    *yval = *v1;
  
  return 0;
}


/* Return the minimum of the two points into the yval structure */
static int _min(
     point_t *yval,
     point_t *v0,
     point_t *v1)
{
  if (v0->y < v1->y - C_EPS)
    *yval = *v0;
  else if (FP_EQUAL(v0->y, v1->y))
    {
      if (v0->x < v1->x)
	*yval = *v0;
      else
	*yval = *v1;
    }
  else
    *yval = *v1;
  
  return 0;
}


int _greater_than(
     point_t *v0,
     point_t *v1)
{
  if (v0->y > v1->y + C_EPS)
    return TRUE;
  else if (v0->y < v1->y - C_EPS)
    return FALSE;
  else
    return (v0->x > v1->x);
}


int _equal_to(
     point_t *v0,
     point_t *v1)
{
  return (FP_EQUAL(v0->y, v1->y) && FP_EQUAL(v0->x, v1->x));
}

int _greater_than_equal_to(
     point_t *v0,
     point_t *v1)
{
  if (v0->y > v1->y + C_EPS)
    return TRUE;
  else if (v0->y < v1->y - C_EPS)
    return FALSE;
  else
    return (v0->x >= v1->x);
}

int _less_than(
     point_t *v0,
     point_t *v1)
{
  if (v0->y < v1->y - C_EPS)
    return TRUE;
  else if (v0->y > v1->y + C_EPS)
    return FALSE;
  else
    return (v0->x < v1->x);
}


/* Initilialise the query structure (Q) and the trapezoid table (T) 
 * when the first segment is added to start the trapezoidation. The
 * query-tree starts out with 4 trapezoids, one S-node and 2 Y-nodes
 *    
 *                4
 *   -----------------------------------
 *  		  \
 *  	1	   \        2
 *  		    \
 *   -----------------------------------
 *                3
 */

static int init_query_structure(int segnum)
{
  int i1, i2, i3, i4, i5, i6, i7, root;
  int t1, t2, t3, t4;
  segment_t *s = &seg[segnum];

  q_idx = tr_idx = 1;
  memset((void *)tr, 0, sizeof(tr));
  memset((void *)qs, 0, sizeof(qs));

  i1 = newnode();
  qs[i1].nodetype = T_Y;
  _max(&qs[i1].yval, &s->v0, &s->v1); /* root */
  root = i1;

  qs[i1].right = i2 = newnode();
  qs[i2].nodetype = T_SINK;
  qs[i2].parent = i1;

  qs[i1].left = i3 = newnode();
  qs[i3].nodetype = T_Y;
  _min(&qs[i3].yval, &s->v0, &s->v1); /* root */
  qs[i3].parent = i1;
  
  qs[i3].left = i4 = newnode();
  qs[i4].nodetype = T_SINK;
  qs[i4].parent = i3;
  
  qs[i3].right = i5 = newnode();
  qs[i5].nodetype = T_X;
  qs[i5].segnum = segnum;
  qs[i5].parent = i3;
  
  qs[i5].left = i6 = newnode();
  qs[i6].nodetype = T_SINK;
  qs[i6].parent = i5;

  qs[i5].right = i7 = newnode();
  qs[i7].nodetype = T_SINK;
  qs[i7].parent = i5;

  t1 = newtrap();		/* middle left */
  t2 = newtrap();		/* middle right */
  t3 = newtrap();		/* bottom-most */
  t4 = newtrap();		/* topmost */

  tr[t1].hi = tr[t2].hi = tr[t4].lo = qs[i1].yval;
  tr[t1].lo = tr[t2].lo = tr[t3].hi = qs[i3].yval;
  tr[t4].hi.y = (double) (INFINITY);
  tr[t4].hi.x = (double) (INFINITY);
  tr[t3].lo.y = (double) -1* (INFINITY);
  tr[t3].lo.x = (double) -1* (INFINITY);
  tr[t1].rseg = tr[t2].lseg = segnum;
  tr[t1].u0 = tr[t2].u0 = t4;
  tr[t1].d0 = tr[t2].d0 = t3;
  tr[t4].d0 = tr[t3].u0 = t1;
  tr[t4].d1 = tr[t3].u1 = t2;
  
  tr[t1].sink = i6;
  tr[t2].sink = i7;
  tr[t3].sink = i4;
  tr[t4].sink = i2;

  tr[t1].state = tr[t2].state = ST_VALID;
  tr[t3].state = tr[t4].state = ST_VALID;

  qs[i2].trnum = t4;
  qs[i4].trnum = t3;
  qs[i6].trnum = t1;
  qs[i7].trnum = t2;

  s->is_inserted = TRUE;
  return root;
}


/* Retun TRUE if the vertex v is to the left of line segment no.
 * segnum. Takes care of the degenerate cases when both the vertices
 * have the same y--cood, etc.
 */

static int is_left_of(
     int segnum,
     point_t *v)
{
  segment_t *s = &seg[segnum];
  double area;
  
  if (_greater_than(&s->v1, &s->v0)) /* seg. going upwards */
    {
      if (FP_EQUAL(s->v1.y, v->y))
	{
	  if (v->x < s->v1.x)
	    area = 1.0;
	  else
	    area = -1.0;
	}
      else if (FP_EQUAL(s->v0.y, v->y))
	{
	  if (v->x < s->v0.x)
	    area = 1.0;
	  else
	    area = -1.0;
	}
      else
	area = CROSS(s->v0, s->v1, (*v));
    }
  else				/* v0 > v1 */
    {
      if (FP_EQUAL(s->v1.y, v->y))
	{
	  if (v->x < s->v1.x)
	    area = 1.0;
	  else
	    area = -1.0;
	}
      else if (FP_EQUAL(s->v0.y, v->y))
	{
	  if (v->x < s->v0.x)
	    area = 1.0;
	  else
	    area = -1.0;
	}
      else
	area = CROSS(s->v1, s->v0, (*v));
    }
  
  if (area > 0.0)
    return TRUE;
  else 
    return FALSE;
}



/* Returns true if the corresponding endpoint of the given segment is */
/* already inserted into the segment tree. Use the simple test of */
/* whether the segment which shares this endpoint is already inserted */

static int inserted(int segnum,int whichpt)
{
  if (whichpt == FIRSTPT)
    return seg[seg[segnum].prev].is_inserted;
  else
    return seg[seg[segnum].next].is_inserted;
}

/* This is query routine which determines which trapezoid does the 
 * point v lie in. The return value is the trapezoid number. 
 */

int locate_endpoint(
     point_t *v,
     point_t *vo,
     int r)
{
  node_t *rptr = &qs[r];
  
  switch (rptr->nodetype)
    {
    case T_SINK:
      return rptr->trnum;
      
    case T_Y:
      if (_greater_than(v, &rptr->yval)) /* above */
	return locate_endpoint(v, vo, rptr->right);
      else if (_equal_to(v, &rptr->yval)) /* the point is already */
	{			          /* inserted. */
	  if (_greater_than(vo, &rptr->yval)) /* above */
	    return locate_endpoint(v, vo, rptr->right);
	  else 
	    return locate_endpoint(v, vo, rptr->left); /* below */	    
	}
      else
	return locate_endpoint(v, vo, rptr->left); /* below */

    case T_X:
      if (_equal_to(v, &seg[rptr->segnum].v0) || 
	       _equal_to(v, &seg[rptr->segnum].v1))
	{
	  if (FP_EQUAL(v->y, vo->y)) /* horizontal segment */
	    {
	      if (vo->x < v->x)
		return locate_endpoint(v, vo, rptr->left); /* left */
	      else
		return locate_endpoint(v, vo, rptr->right); /* right */
	    }

	  else if (is_left_of(rptr->segnum, vo))
	    return locate_endpoint(v, vo, rptr->left); /* left */
	  else
	    return locate_endpoint(v, vo, rptr->right); /* right */
	}
      else if (is_left_of(rptr->segnum, v))
	return locate_endpoint(v, vo, rptr->left); /* left */
      else
	return locate_endpoint(v, vo, rptr->right); /* right */	

    default:
      fprintf(stderr, "Haggu !!!!!\n");
      break;
    }
    
    return FALSE;
}


/* Thread in the segment into the existing trapezoidation. The 
 * limiting trapezoids are given by tfirst and tlast (which are the
 * trapezoids containing the two endpoints of the segment. Merges all
 * possible trapezoids which flank this segment and have been recently
 * divided because of its insertion
 */

static int merge_trapezoids(
     int segnum,
     int tfirst,
     int tlast,
     int side)
{
  int t, tnext, cond;
  int ptnext;

  /* First merge polys on the LHS */
  t = tfirst;
  while ((t > 0) && _greater_than_equal_to(&tr[t].lo, &tr[tlast].lo))
    {
      if (side == S_LEFT)
	cond = ((((tnext = tr[t].d0) > 0) && (tr[tnext].rseg == segnum)) ||
		(((tnext = tr[t].d1) > 0) && (tr[tnext].rseg == segnum)));
      else
	cond = ((((tnext = tr[t].d0) > 0) && (tr[tnext].lseg == segnum)) ||
		(((tnext = tr[t].d1) > 0) && (tr[tnext].lseg == segnum)));
      
      if (cond)
	{
	  if ((tr[t].lseg == tr[tnext].lseg) &&
	      (tr[t].rseg == tr[tnext].rseg)) /* good neighbours */
	    {			              /* merge them */
	      /* Use the upper node as the new node i.e. t */
	      
	      ptnext = qs[tr[tnext].sink].parent;
	      
	      if (qs[ptnext].left == tr[tnext].sink)
		qs[ptnext].left = tr[t].sink;
	      else
		qs[ptnext].right = tr[t].sink;	/* redirect parent */
	      
	      
	      /* Change the upper neighbours of the lower trapezoids */
	      
	      if ((tr[t].d0 = tr[tnext].d0) > 0)
		if (tr[tr[t].d0].u0 == tnext)
		  tr[tr[t].d0].u0 = t;
		else if (tr[tr[t].d0].u1 == tnext)
		  tr[tr[t].d0].u1 = t;
	      
	      if ((tr[t].d1 = tr[tnext].d1) > 0)
		if (tr[tr[t].d1].u0 == tnext)
		  tr[tr[t].d1].u0 = t;
		else if (tr[tr[t].d1].u1 == tnext)
		  tr[tr[t].d1].u1 = t;
	      
	      tr[t].lo = tr[tnext].lo;
	      tr[tnext].state = ST_INVALID; /* invalidate the lower */
				            /* trapezium */
	    }
	  else		    /* not good neighbours */
	    t = tnext;
	}
      else		    /* do not satisfy the outer if */
	t = tnext;
      
    } /* end-while */
       
  return 0;
}


/* Add in the new segment into the trapezoidation and update Q and T
 * structures. First locate the two endpoints of the segment in the
 * Q-structure. Then start from the topmost trapezoid and go down to
 * the  lower trapezoid dividing all the trapezoids in between .
 */

static int add_segment(int segnum)
{
  segment_t s;
  int tu, tl, sk, tfirst, tlast;
  int tfirstr, tlastr, tfirstl, tlastl;
  int i1, i2, t, tn;
  point_t tpt;
  int /*tritop = 0, */tribot = 0, is_swapped = 0;
  int tmptriseg;

  s = seg[segnum];
  if (_greater_than(&s.v1, &s.v0)) /* Get higher vertex in v0 */
    {
      int tmp;
      tpt = s.v0;
      s.v0 = s.v1;
      s.v1 = tpt;
      tmp = s.root0;
      s.root0 = s.root1;
      s.root1 = tmp;
      is_swapped = TRUE;
    }

  if ((is_swapped) ? !inserted(segnum, LASTPT) :
       !inserted(segnum, FIRSTPT))     /* insert v0 in the tree */
    {
      int tmp_d;

      tu = locate_endpoint(&s.v0, &s.v1, s.root0);
      tl = newtrap();		/* tl is the new lower trapezoid */
      tr[tl].state = ST_VALID;
      tr[tl] = tr[tu];
      tr[tu].lo.y = tr[tl].hi.y = s.v0.y;
      tr[tu].lo.x = tr[tl].hi.x = s.v0.x;
      tr[tu].d0 = tl;      
      tr[tu].d1 = 0;
      tr[tl].u0 = tu;
      tr[tl].u1 = 0;

      if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u0 == tu))
	tr[tmp_d].u0 = tl;
      if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u1 == tu))
	tr[tmp_d].u1 = tl;

      if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u0 == tu))
	tr[tmp_d].u0 = tl;
      if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u1 == tu))
	tr[tmp_d].u1 = tl;

      /* Now update the query structure and obtain the sinks for the */
      /* two trapezoids */ 
      
      i1 = newnode();		/* Upper trapezoid sink */
      i2 = newnode();		/* Lower trapezoid sink */
      sk = tr[tu].sink;
      
      qs[sk].nodetype = T_Y;
      qs[sk].yval = s.v0;
      qs[sk].segnum = segnum;	/* not really reqd ... maybe later */
      qs[sk].left = i2;
      qs[sk].right = i1;

      qs[i1].nodetype = T_SINK;
      qs[i1].trnum = tu;
      qs[i1].parent = sk;

      qs[i2].nodetype = T_SINK;
      qs[i2].trnum = tl;
      qs[i2].parent = sk;

      tr[tu].sink = i1;
      tr[tl].sink = i2;
      tfirst = tl;
    }
  else				/* v0 already present */
    {       /* Get the topmost intersecting trapezoid */
      tfirst = locate_endpoint(&s.v0, &s.v1, s.root0);
      //tritop = 1;
    }


  if ((is_swapped) ? !inserted(segnum, FIRSTPT) :
       !inserted(segnum, LASTPT))     /* insert v1 in the tree */
    {
      int tmp_d;

      tu = locate_endpoint(&s.v1, &s.v0, s.root1);

      tl = newtrap();		/* tl is the new lower trapezoid */
      tr[tl].state = ST_VALID;
      tr[tl] = tr[tu];
      tr[tu].lo.y = tr[tl].hi.y = s.v1.y;
      tr[tu].lo.x = tr[tl].hi.x = s.v1.x;
      tr[tu].d0 = tl;      
      tr[tu].d1 = 0;
      tr[tl].u0 = tu;
      tr[tl].u1 = 0;

      if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u0 == tu))
	tr[tmp_d].u0 = tl;
      if (((tmp_d = tr[tl].d0) > 0) && (tr[tmp_d].u1 == tu))
	tr[tmp_d].u1 = tl;

      if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u0 == tu))
	tr[tmp_d].u0 = tl;
      if (((tmp_d = tr[tl].d1) > 0) && (tr[tmp_d].u1 == tu))
	tr[tmp_d].u1 = tl;
      
      /* Now update the query structure and obtain the sinks for the */
      /* two trapezoids */ 
      
      i1 = newnode();		/* Upper trapezoid sink */
      i2 = newnode();		/* Lower trapezoid sink */
      sk = tr[tu].sink;
      
      qs[sk].nodetype = T_Y;
      qs[sk].yval = s.v1;
      qs[sk].segnum = segnum;	/* not really reqd ... maybe later */
      qs[sk].left = i2;
      qs[sk].right = i1;

      qs[i1].nodetype = T_SINK;
      qs[i1].trnum = tu;
      qs[i1].parent = sk;

      qs[i2].nodetype = T_SINK;
      qs[i2].trnum = tl;
      qs[i2].parent = sk;

      tr[tu].sink = i1;
      tr[tl].sink = i2;
      tlast = tu;
    }
  else				/* v1 already present */
    {       /* Get the lowermost intersecting trapezoid */
      tlast = locate_endpoint(&s.v1, &s.v0, s.root1);
      tribot = 1;
    }
  
  /* Thread the segment into the query tree creating a new X-node */
  /* First, split all the trapezoids which are intersected by s into */
  /* two */

  t = tfirst;			/* topmost trapezoid */
  
  while ((t > 0) && 
	 _greater_than_equal_to(&tr[t].lo, &tr[tlast].lo))
				/* traverse from top to bot */
    {
      int t_sav, tn_sav;
      sk = tr[t].sink;
      i1 = newnode();		/* left trapezoid sink */
      i2 = newnode();		/* right trapezoid sink */
      
      qs[sk].nodetype = T_X;
      qs[sk].segnum = segnum;
      qs[sk].left = i1;
      qs[sk].right = i2;

      qs[i1].nodetype = T_SINK;	/* left trapezoid (use existing one) */
      qs[i1].trnum = t;
      qs[i1].parent = sk;

      qs[i2].nodetype = T_SINK;	/* right trapezoid (allocate new) */
      qs[i2].trnum = tn = newtrap();
      tr[tn].state = ST_VALID;
      qs[i2].parent = sk;

      if (t == tfirst)
	tfirstr = tn;
      if (_equal_to(&tr[t].lo, &tr[tlast].lo))
	tlastr = tn;

      tr[tn] = tr[t];
      tr[t].sink = i1;
      tr[tn].sink = i2;
      t_sav = t;
      tn_sav = tn;

      /* error */

      if ((tr[t].d0 <= 0) && (tr[t].d1 <= 0)) /* case cannot arise */
	{
	  fprintf(stderr, "add_segment: error\n");
	  break;
	}
      
      /* only one trapezoid below. partition t into two and make the */
      /* two resulting trapezoids t and tn as the upper neighbours of */
      /* the sole lower trapezoid */
      
      else if ((tr[t].d0 > 0) && (tr[t].d1 <= 0))
	{			/* Only one trapezoid below */
	  if ((tr[t].u0 > 0) && (tr[t].u1 > 0))
	    {			/* continuation of a chain from abv. */
	      if (tr[t].usave > 0) /* three upper neighbours */
		{
		  if (tr[t].uside == S_LEFT)
		    {
		      tr[tn].u0 = tr[t].u1;
		      tr[t].u1 = -1;
		      tr[tn].u1 = tr[t].usave;
		      
		      tr[tr[t].u0].d0 = t;
		      tr[tr[tn].u0].d0 = tn;
		      tr[tr[tn].u1].d0 = tn;
		    }
		  else		/* intersects in the right */
		    {
		      tr[tn].u1 = -1;
		      tr[tn].u0 = tr[t].u1;
		      tr[t].u1 = tr[t].u0;
		      tr[t].u0 = tr[t].usave;

		      tr[tr[t].u0].d0 = t;
		      tr[tr[t].u1].d0 = t;
		      tr[tr[tn].u0].d0 = tn;		      
		    }
		  
		  tr[t].usave = tr[tn].usave = 0;
		}
	      else		/* No usave.... simple case */
		{
		  tr[tn].u0 = tr[t].u1;
		  tr[t].u1 = tr[tn].u1 = -1;
		  tr[tr[tn].u0].d0 = tn;
		}
	    }
	  else 
	    {			/* fresh seg. or upward cusp */
	      int tmp_u = tr[t].u0;
	      int td0;
	      if (((td0 = tr[tmp_u].d0) > 0) && 
		  ((tr[tmp_u].d1) > 0))
		{		/* upward cusp */
		  if ((tr[td0].rseg > 0) &&
		      !is_left_of(tr[td0].rseg, &s.v1))
		    {
		      tr[t].u0 = tr[t].u1 = tr[tn].u1 = -1;
		      tr[tr[tn].u0].d1 = tn;
		    }
		  else		/* cusp going leftwards */
		    { 
		      tr[tn].u0 = tr[tn].u1 = tr[t].u1 = -1;
		      tr[tr[t].u0].d0 = t;
		    }
		}
	      else		/* fresh segment */
		{
		  tr[tr[t].u0].d0 = t;
		  tr[tr[t].u0].d1 = tn;
		}	      
	    }
	  
	  if (FP_EQUAL(tr[t].lo.y, tr[tlast].lo.y) && 
	      FP_EQUAL(tr[t].lo.x, tr[tlast].lo.x) && tribot)
	    {		/* bottom forms a triangle */

	      if (is_swapped)	
		tmptriseg = seg[segnum].prev;
	      else
		tmptriseg = seg[segnum].next;
	      
	      if ((tmptriseg > 0) && is_left_of(tmptriseg, &s.v0))
		{
				/* L-R downward cusp */
		  tr[tr[t].d0].u0 = t;
		  tr[tn].d0 = tr[tn].d1 = -1;
		}
	      else
		{
				/* R-L downward cusp */
		  tr[tr[tn].d0].u1 = tn;
		  tr[t].d0 = tr[t].d1 = -1;
		}
	    }
	  else
	    {
	      if ((tr[tr[t].d0].u0 > 0) && (tr[tr[t].d0].u1 > 0))
		{
		  if (tr[tr[t].d0].u0 == t) /* passes thru LHS */
		    {
		      tr[tr[t].d0].usave = tr[tr[t].d0].u1;
		      tr[tr[t].d0].uside = S_LEFT;
		    }
		  else
		    {
		      tr[tr[t].d0].usave = tr[tr[t].d0].u0;
		      tr[tr[t].d0].uside = S_RIGHT;
		    }		    
		}
	      tr[tr[t].d0].u0 = t;
	      tr[tr[t].d0].u1 = tn;
	    }
	  
	  t = tr[t].d0;
	}


      else if ((tr[t].d0 <= 0) && (tr[t].d1 > 0))
	{			/* Only one trapezoid below */
	  if ((tr[t].u0 > 0) && (tr[t].u1 > 0))
	    {			/* continuation of a chain from abv. */
	      if (tr[t].usave > 0) /* three upper neighbours */
		{
		  if (tr[t].uside == S_LEFT)
		    {
		      tr[tn].u0 = tr[t].u1;
		      tr[t].u1 = -1;
		      tr[tn].u1 = tr[t].usave;
		      
		      tr[tr[t].u0].d0 = t;
		      tr[tr[tn].u0].d0 = tn;
		      tr[tr[tn].u1].d0 = tn;
		    }
		  else		/* intersects in the right */
		    {
		      tr[tn].u1 = -1;
		      tr[tn].u0 = tr[t].u1;
		      tr[t].u1 = tr[t].u0;
		      tr[t].u0 = tr[t].usave;

		      tr[tr[t].u0].d0 = t;
		      tr[tr[t].u1].d0 = t;
		      tr[tr[tn].u0].d0 = tn;		      
		    }
		  
		  tr[t].usave = tr[tn].usave = 0;
		}
	      else		/* No usave.... simple case */
		{
		  tr[tn].u0 = tr[t].u1;
		  tr[t].u1 = tr[tn].u1 = -1;
		  tr[tr[tn].u0].d0 = tn;
		}
	    }
	  else 
	    {			/* fresh seg. or upward cusp */
	      int tmp_u = tr[t].u0;
	      int td0;
	      if (((td0 = tr[tmp_u].d0) > 0) && 
		  ((tr[tmp_u].d1) > 0))
		{		/* upward cusp */
		  if ((tr[td0].rseg > 0) &&
		      !is_left_of(tr[td0].rseg, &s.v1))
		    {
		      tr[t].u0 = tr[t].u1 = tr[tn].u1 = -1;
		      tr[tr[tn].u0].d1 = tn;
		    }
		  else 
		    {
		      tr[tn].u0 = tr[tn].u1 = tr[t].u1 = -1;
		      tr[tr[t].u0].d0 = t;
		    }
		}
	      else		/* fresh segment */
		{
		  tr[tr[t].u0].d0 = t;
		  tr[tr[t].u0].d1 = tn;
		}
	    }
	  
	  if (FP_EQUAL(tr[t].lo.y, tr[tlast].lo.y) && 
	      FP_EQUAL(tr[t].lo.x, tr[tlast].lo.x) && tribot)
	    {		/* bottom forms a triangle */
	      int tmptriseg;

	      if (is_swapped)	
		tmptriseg = seg[segnum].prev;
	      else
		tmptriseg = seg[segnum].next;

	      if ((tmptriseg > 0) && is_left_of(tmptriseg, &s.v0))
		{
		  /* L-R downward cusp */
		  tr[tr[t].d1].u0 = t;
		  tr[tn].d0 = tr[tn].d1 = -1;
		}
	      else
		{
		  /* R-L downward cusp */
		  tr[tr[tn].d1].u1 = tn;
		  tr[t].d0 = tr[t].d1 = -1;
		}
	    }		
	  else
	    {
	      if ((tr[tr[t].d1].u0 > 0) && (tr[tr[t].d1].u1 > 0))
		{
		  if (tr[tr[t].d1].u0 == t) /* passes thru LHS */
		    {
		      tr[tr[t].d1].usave = tr[tr[t].d1].u1;
		      tr[tr[t].d1].uside = S_LEFT;
		    }
		  else
		    {
		      tr[tr[t].d1].usave = tr[tr[t].d1].u0;
		      tr[tr[t].d1].uside = S_RIGHT;
		    }		    
		}
	      tr[tr[t].d1].u0 = t;
	      tr[tr[t].d1].u1 = tn;
	    }
	  
	  t = tr[t].d1;
	}

      /* two trapezoids below. Find out which one is intersected by */
      /* this segment and proceed down that one */
      
      else
	{
	  double y0, yt;
	  point_t tmppt;
	  int tnext, i_d0;

	  i_d0 = FALSE;
	  if (FP_EQUAL(tr[t].lo.y, s.v0.y))
	    {
	      if (tr[t].lo.x > s.v0.x)
		i_d0 = TRUE;
	    }
	  else
	    {
	      tmppt.y = y0 = tr[t].lo.y;
	      yt = (y0 - s.v0.y)/(s.v1.y - s.v0.y);
	      tmppt.x = s.v0.x + yt * (s.v1.x - s.v0.x);
	      
	      if (_less_than(&tmppt, &tr[t].lo))
		i_d0 = TRUE;
	    }
	  
	  /* check continuity from the top so that the lower-neighbour */
	  /* values are properly filled for the upper trapezoid */

	  if ((tr[t].u0 > 0) && (tr[t].u1 > 0))
	    {			/* continuation of a chain from abv. */
	      if (tr[t].usave > 0) /* three upper neighbours */
		{
		  if (tr[t].uside == S_LEFT)
		    {
		      tr[tn].u0 = tr[t].u1;
		      tr[t].u1 = -1;
		      tr[tn].u1 = tr[t].usave;
		      
		      tr[tr[t].u0].d0 = t;
		      tr[tr[tn].u0].d0 = tn;
		      tr[tr[tn].u1].d0 = tn;
		    }
		  else		/* intersects in the right */
		    {
		      tr[tn].u1 = -1;
		      tr[tn].u0 = tr[t].u1;
		      tr[t].u1 = tr[t].u0;
		      tr[t].u0 = tr[t].usave;

		      tr[tr[t].u0].d0 = t;
		      tr[tr[t].u1].d0 = t;
		      tr[tr[tn].u0].d0 = tn;		      
		    }
		  
		  tr[t].usave = tr[tn].usave = 0;
		}
	      else		/* No usave.... simple case */
		{
		  tr[tn].u0 = tr[t].u1;
		  tr[tn].u1 = -1;
		  tr[t].u1 = -1;
		  tr[tr[tn].u0].d0 = tn;
		}
	    }
	  else 
	    {			/* fresh seg. or upward cusp */
	      int tmp_u = tr[t].u0;
	      int td0;
	      if (((td0 = tr[tmp_u].d0) > 0) && 
		  ((tr[tmp_u].d1) > 0))
		{		/* upward cusp */
		  if ((tr[td0].rseg > 0) &&
		      !is_left_of(tr[td0].rseg, &s.v1))
		    {
		      tr[t].u0 = tr[t].u1 = tr[tn].u1 = -1;
		      tr[tr[tn].u0].d1 = tn;
		    }
		  else 
		    {
		      tr[tn].u0 = tr[tn].u1 = tr[t].u1 = -1;
		      tr[tr[t].u0].d0 = t;
		    }
		}
	      else		/* fresh segment */
		{
		  tr[tr[t].u0].d0 = t;
		  tr[tr[t].u0].d1 = tn;
		}
	    }
	  
	  if (FP_EQUAL(tr[t].lo.y, tr[tlast].lo.y) && 
	      FP_EQUAL(tr[t].lo.x, tr[tlast].lo.x) && tribot)
	    {
	      /* this case arises only at the lowest trapezoid.. i.e.
		 tlast, if the lower endpoint of the segment is
		 already inserted in the structure */
	      
	      tr[tr[t].d0].u0 = t;
	      tr[tr[t].d0].u1 = -1;
	      tr[tr[t].d1].u0 = tn;
	      tr[tr[t].d1].u1 = -1;

	      tr[tn].d0 = tr[t].d1;
	      tr[t].d1 = tr[tn].d1 = -1;
	      
	      tnext = tr[t].d1;	      
	    }
	  else if (i_d0)
				/* intersecting d0 */
	    {
	      tr[tr[t].d0].u0 = t;
	      tr[tr[t].d0].u1 = tn;
	      tr[tr[t].d1].u0 = tn;
	      tr[tr[t].d1].u1 = -1;
	      
	      /* new code to determine the bottom neighbours of the */
	      /* newly partitioned trapezoid */
	      
	      tr[t].d1 = -1;

	      tnext = tr[t].d0;
	    }
	  else			/* intersecting d1 */
	    {
	      tr[tr[t].d0].u0 = t;
	      tr[tr[t].d0].u1 = -1;
	      tr[tr[t].d1].u0 = t;
	      tr[tr[t].d1].u1 = tn;

	      /* new code to determine the bottom neighbours of the */
	      /* newly partitioned trapezoid */
	      
	      tr[tn].d0 = tr[t].d1;
	      tr[tn].d1 = -1;
	      
	      tnext = tr[t].d1;
	    }	    
	  
	  t = tnext;
	}
      
      tr[t_sav].rseg = tr[tn_sav].lseg  = segnum;
    } /* end-while */
  
  /* Now combine those trapezoids which share common segments. We can */
  /* use the pointers to the parent to connect these together. This */
  /* works only because all these new trapezoids have been formed */
  /* due to splitting by the segment, and hence have only one parent */

  tfirstl = tfirst; 
  tlastl = tlast;
  merge_trapezoids(segnum, tfirstl, tlastl, S_LEFT);
  merge_trapezoids(segnum, tfirstr, tlastr, S_RIGHT);

  seg[segnum].is_inserted = TRUE;
  return 0;
}


/* Update the roots stored for each of the endpoints of the segment.
 * This is done to speed up the location-query for the endpoint when
 * the segment is inserted into the trapezoidation subsequently
 */
static int find_new_roots(int segnum)
{
  segment_t *s = &seg[segnum];
  
  if (s->is_inserted)
    return 0;

  s->root0 = locate_endpoint(&s->v0, &s->v1, s->root0);
  s->root0 = tr[s->root0].sink;

  s->root1 = locate_endpoint(&s->v1, &s->v0, s->root1);
  s->root1 = tr[s->root1].sink;  
  return 0;
}


/* Main routine to perform trapezoidation */
int construct_trapezoids(int nseg)
{
  register int i;
  int root, h;
  
  /* Add the first segment and get the query structure and trapezoid */
  /* list initialised */

  root = init_query_structure(choose_segment());

  for (i = 1; i <= nseg; i++)
    seg[i].root0 = seg[i].root1 = root;
  
  for (h = 1; h <= math_logstar_n(nseg); h++)
    {
      for (i = math_N(nseg, h -1) + 1; i <= math_N(nseg, h); i++)
	add_segment(choose_segment());
      
      /* Find a new root for each of the segment endpoints */
      for (i = 1; i <= nseg; i++)
	find_new_roots(i);
    }
  
  for (i = math_N(nseg, math_logstar_n(nseg)) + 1; i <= nseg; i++)
    add_segment(choose_segment());

  return 0;
}
/* Function returns TRUE if the trapezoid lies inside the polygon */
static int inside_polygon(trap_t *t)
{
  int rseg = t->rseg;

  if (t->state == ST_INVALID)
    return 0;

  if ((t->lseg <= 0) || (t->rseg <= 0))
    return 0;
  
  if (((t->u0 <= 0) && (t->u1 <= 0)) || 
      ((t->d0 <= 0) && (t->d1 <= 0))) /* triangle */
    return (_greater_than(&seg[rseg].v1, &seg[rseg].v0));
  
  return 0;
}


/* return a new mon structure from the table */
static int newmon()
{
  return ++mon_idx;
}


/* return a new chain element from the table */
static int new_chain_element()
{
  return ++chain_idx;
}


static double get_angle(
     point_t *vp0,
     point_t *vpnext,
     point_t *vp1)
{
  point_t v0, v1;
  
  v0.x = vpnext->x - vp0->x;
  v0.y = vpnext->y - vp0->y;

  v1.x = vp1->x - vp0->x;
  v1.y = vp1->y - vp0->y;

  if (CROSS_SINE(v0, v1) >= 0)	/* sine is positive */
    return DOT(v0, v1)/LENGTH(v0)/LENGTH(v1);
  else
    return (-1.0 * DOT(v0, v1)/LENGTH(v0)/LENGTH(v1) - 2);
}


/* (v0, v1) is the new diagonal to be added to the polygon. Find which */
/* chain to use and return the positions of v0 and v1 in p and q */ 
static int get_vertex_positions(
     int v0,
     int v1,
     int *ip,
     int *iq)
{
  vertexchain_t *vp0, *vp1;
  register int i;
  double angle, temp;
  int tp, tq;

  vp0 = &vert[v0];
  vp1 = &vert[v1];
  
  /* p is identified as follows. Scan from (v0, v1) rightwards till */
  /* you hit the first segment starting from v0. That chain is the */
  /* chain of our interest */
  
  angle = -4.0;
  for (i = 0; i < 4; i++)
    {
      if (vp0->vnext[i] <= 0)
	continue;
      if ((temp = get_angle(&vp0->pt, &(vert[vp0->vnext[i]].pt), 
			    &vp1->pt)) > angle)
	{
	  angle = temp;
	  tp = i;
	}
    }

  *ip = tp;

  /* Do similar actions for q */

  angle = -4.0;
  for (i = 0; i < 4; i++)
    {
      if (vp1->vnext[i] <= 0)
	continue;      
      if ((temp = get_angle(&vp1->pt, &(vert[vp1->vnext[i]].pt), 
			    &vp0->pt)) > angle)
	{
	  angle = temp;
	  tq = i;
	}
    }

  *iq = tq;

  return 0;
}

  
/* v0 and v1 are specified in anti-clockwise order with respect to 
 * the current monotone polygon mcur. Split the current polygon into 
 * two polygons using the diagonal (v0, v1) 
 */
static int make_new_monotone_poly(
     int mcur,
     int v0,
     int v1)
{
  int p, q, ip, iq;
  int mnew = newmon();
  int i, j, nf0, nf1;
  vertexchain_t *vp0, *vp1;
  
  vp0 = &vert[v0];
  vp1 = &vert[v1];

  get_vertex_positions(v0, v1, &ip, &iq);

  p = vp0->vpos[ip];
  q = vp1->vpos[iq];

  /* At this stage, we have got the positions of v0 and v1 in the */
  /* desired chain. Now modify the linked lists */

  i = new_chain_element();	/* for the new list */
  j = new_chain_element();

  mchain[i].vnum = v0;
  mchain[j].vnum = v1;

  mchain[i].next = mchain[p].next;
  mchain[mchain[p].next].prev = i;
  mchain[i].prev = j;
  mchain[j].next = i;
  mchain[j].prev = mchain[q].prev;
  mchain[mchain[q].prev].next = j;

  mchain[p].next = q;
  mchain[q].prev = p;

  nf0 = vp0->nextfree;
  nf1 = vp1->nextfree;

  vp0->vnext[ip] = v1;

  vp0->vpos[nf0] = i;
  vp0->vnext[nf0] = mchain[mchain[i].next].vnum;
  vp1->vpos[nf1] = j;
  vp1->vnext[nf1] = v0;

  vp0->nextfree++;
  vp1->nextfree++;

#ifdef DEBUG
  fprintf(stderr, "make_poly: mcur = %d, (v0, v1) = (%d, %d)\n", 
	  mcur, v0, v1);
  fprintf(stderr, "next posns = (p, q) = (%d, %d)\n", p, q);
#endif

  mon[mcur] = p;
  mon[mnew] = i;
  return mnew;
}

/* Main routine to get monotone polygons from the trapezoidation of 
 * the polygon.
 */

int monotonate_trapezoids(
     int n)
{
  register int i;
  int tr_start;

  memset((void *)vert, 0, sizeof(vert));
  memset((void *)visited, 0, sizeof(visited));
  memset((void *)mchain, 0, sizeof(mchain));
  memset((void *)mon, 0, sizeof(mon));
  
  /* First locate a trapezoid which lies inside the polygon */
  /* and which is triangular */
  for (i = 0; i < TRSIZE; i++)
    if (inside_polygon(&tr[i]))
      break;
  tr_start = i;
  
  /* Initialise the mon data-structure and start spanning all the */
  /* trapezoids within the polygon */

#if 0
  for (i = 1; i <= n; i++)
    {
      mchain[i].prev = i - 1;
      mchain[i].next = i + 1;
      mchain[i].vnum = i;
      vert[i].pt = seg[i].v0;
      vert[i].vnext[0] = i + 1;	/* next vertex */
      vert[i].vpos[0] = i;	/* locn. of next vertex */
      vert[i].nextfree = 1;
    }
  mchain[1].prev = n;
  mchain[n].next = 1;
  vert[n].vnext[0] = 1;
  vert[n].vpos[0] = n;
  chain_idx = n;
  mon_idx = 0;
  mon[0] = 1;			/* position of any vertex in the first */
				/* chain  */

#else

  for (i = 1; i <= n; i++)
    {
      mchain[i].prev = seg[i].prev;
      mchain[i].next = seg[i].next;
      mchain[i].vnum = i;
      vert[i].pt = seg[i].v0;
      vert[i].vnext[0] = seg[i].next; /* next vertex */
      vert[i].vpos[0] = i;	/* locn. of next vertex */
      vert[i].nextfree = 1;
    }

  chain_idx = n;
  mon_idx = 0;
  mon[0] = 1;			/* position of any vertex in the first */
				/* chain  */

#endif
  
  /* traverse the polygon */
  if (tr[tr_start].u0 > 0)
    traverse_polygon(0, tr_start, tr[tr_start].u0, TR_FROM_UP);
  else if (tr[tr_start].d0 > 0)
    traverse_polygon(0, tr_start, tr[tr_start].d0, TR_FROM_DN);
  
  /* return the number of polygons created */
  return newmon();
}


/* recursively visit all the trapezoids */
static int traverse_polygon(
     int mcur,
     int trnum,
     int from,
     int dir)
{
  trap_t *t = &tr[trnum];
  int /*howsplit, */mnew;
  int v0, v1/*, v0next*//*, v1next*/;
  int retval/*, tmp*/;
  //int do_switch = FALSE;

  if ((trnum <= 0) || visited[trnum])
    return 0;

  visited[trnum] = TRUE;
  
  /* We have much more information available here. */
  /* rseg: goes upwards   */
  /* lseg: goes downwards */

  /* Initially assume that dir = TR_FROM_DN (from the left) */
  /* Switch v0 and v1 if necessary afterwards */


  /* special cases for triangles with cusps at the opposite ends. */
  /* take care of this first */
  if ((t->u0 <= 0) && (t->u1 <= 0))
    {
      if ((t->d0 > 0) && (t->d1 > 0)) /* downward opening triangle */
	{
	  v0 = tr[t->d1].lseg;
	  v1 = t->lseg;
	  if (from == t->d1)
	    {
	      //do_switch = TRUE;
	      mnew = make_new_monotone_poly(mcur, v1, v0);
	      traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
	      traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);	    
	    }
	  else
	    {
	      mnew = make_new_monotone_poly(mcur, v0, v1);
	      traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
	      traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
	    }
	}
      else
	{
	  retval = SP_NOSPLIT;	/* Just traverse all neighbours */
	  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
	  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
	  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
	  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
	}
    }
  
  else if ((t->d0 <= 0) && (t->d1 <= 0))
    {
      if ((t->u0 > 0) && (t->u1 > 0)) /* upward opening triangle */
	{
	  v0 = t->rseg;
	  v1 = tr[t->u0].rseg;
	  if (from == t->u1)
	    {
	      //do_switch = TRUE;
	      mnew = make_new_monotone_poly(mcur, v1, v0);
	      traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
	      traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);	    
	    }
	  else
	    {
	      mnew = make_new_monotone_poly(mcur, v0, v1);
	      traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
	      traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
	    }
	}
      else
	{
	  retval = SP_NOSPLIT;	/* Just traverse all neighbours */
	  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
	  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
	  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
	  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
	}
    }
  
  else if ((t->u0 > 0) && (t->u1 > 0)) 
    {
      if ((t->d0 > 0) && (t->d1 > 0)) /* downward + upward cusps */
	{
	  v0 = tr[t->d1].lseg;
	  v1 = tr[t->u0].rseg;
	  retval = SP_2UP_2DN;
	  if (((dir == TR_FROM_DN) && (t->d1 == from)) ||
	      ((dir == TR_FROM_UP) && (t->u1 == from)))
	    {
	      //do_switch = TRUE;
	      mnew = make_new_monotone_poly(mcur, v1, v0);
	      traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
	      traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
	      traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
	      traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
	    }
	  else
	    {
	      mnew = make_new_monotone_poly(mcur, v0, v1);
	      traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
	      traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
	      traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
	      traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);	      
	    }
	}
      else			/* only downward cusp */
	{
	  if (_equal_to(&t->lo, &seg[t->lseg].v1))
	    {
	      v0 = tr[t->u0].rseg;
	      v1 = seg[t->lseg].next;

	      retval = SP_2UP_LEFT;
	      if ((dir == TR_FROM_UP) && (t->u0 == from))
		{
		  //do_switch = TRUE;
		  mnew = make_new_monotone_poly(mcur, v1, v0);
		  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
		}
	      else
		{
		  mnew = make_new_monotone_poly(mcur, v0, v1);
		  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
		}
	    }
	  else
	    {
	      v0 = t->rseg;
	      v1 = tr[t->u0].rseg;	
	      retval = SP_2UP_RIGHT;
	      if ((dir == TR_FROM_UP) && (t->u1 == from))
		{
		  //do_switch = TRUE;
		  mnew = make_new_monotone_poly(mcur, v1, v0);
		  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
		}
	      else
		{
		  mnew = make_new_monotone_poly(mcur, v0, v1);
		  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
		}
	    }
	}
    }
  else if ((t->u0 > 0) || (t->u1 > 0)) /* no downward cusp */
    {
      if ((t->d0 > 0) && (t->d1 > 0)) /* only upward cusp */
	{
	  if (_equal_to(&t->hi, &seg[t->lseg].v0))
	    {
	      v0 = tr[t->d1].lseg;
	      v1 = t->lseg;
	      retval = SP_2DN_LEFT;
	      if (!((dir == TR_FROM_DN) && (t->d0 == from)))
		{
		  //do_switch = TRUE;
		  mnew = make_new_monotone_poly(mcur, v1, v0);
		  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
		}
	      else
		{
		  mnew = make_new_monotone_poly(mcur, v0, v1);
		  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);	      
		}
	    }
	  else
	    {
	      v0 = tr[t->d1].lseg;
	      v1 = seg[t->rseg].next;

	      retval = SP_2DN_RIGHT;	    
	      if ((dir == TR_FROM_DN) && (t->d1 == from))
		{
		  //do_switch = TRUE;
		  mnew = make_new_monotone_poly(mcur, v1, v0);
		  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
		}
	      else
		{
		  mnew = make_new_monotone_poly(mcur, v0, v1);
		  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
		}
	    }
	}
      else			/* no cusp */
	{
	  if (_equal_to(&t->hi, &seg[t->lseg].v0) &&
	      _equal_to(&t->lo, &seg[t->rseg].v0))
	    {
	      v0 = t->rseg;
	      v1 = t->lseg;
	      retval = SP_SIMPLE_LRDN;
	      if (dir == TR_FROM_UP)
		{
		  //do_switch = TRUE;
		  mnew = make_new_monotone_poly(mcur, v1, v0);
		  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
		}
	      else
		{
		  mnew = make_new_monotone_poly(mcur, v0, v1);
		  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
		}
	    }
	  else if (_equal_to(&t->hi, &seg[t->rseg].v1) &&
		   _equal_to(&t->lo, &seg[t->lseg].v1))
	    {
	      v0 = seg[t->rseg].next;
	      v1 = seg[t->lseg].next;

	      retval = SP_SIMPLE_LRUP;
	      if (dir == TR_FROM_UP)
		{
		  //do_switch = TRUE;
		  mnew = make_new_monotone_poly(mcur, v1, v0);
		  traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->d0, trnum, TR_FROM_UP);
		}
	      else
		{
		  mnew = make_new_monotone_poly(mcur, v0, v1);
		  traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);
		  traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
		  traverse_polygon(mnew, t->u0, trnum, TR_FROM_DN);
		  traverse_polygon(mnew, t->u1, trnum, TR_FROM_DN);
		}
	    }
	  else			/* no split possible */
	    {
	      retval = SP_NOSPLIT;
	      traverse_polygon(mcur, t->u0, trnum, TR_FROM_DN);
	      traverse_polygon(mcur, t->d0, trnum, TR_FROM_UP);
	      traverse_polygon(mcur, t->u1, trnum, TR_FROM_DN);
	      traverse_polygon(mcur, t->d1, trnum, TR_FROM_UP);	      	      
	    }
	}
    }

  return retval;
}


/* For each monotone polygon, find the ymax and ymin (to determine the */
/* two y-monotone chains) and pass on this monotone polygon for greedy */
/* triangulation. */
/* Take care not to triangulate duplicate monotone polygons */

int triangulate_monotone_polygons(
     int nvert,
     int nmonpoly,
     int op[][3])
{
  register int i;
  point_t ymax, ymin;
  int p, vfirst, posmax/*, posmin*/, v;
  int vcount, processed;

#ifdef DEBUG
  for (i = 0; i < nmonpoly; i++)
    {
      fprintf(stderr, "\n\nPolygon %d: ", i);
      vfirst = mchain[mon[i]].vnum;
      p = mchain[mon[i]].next;
      fprintf (stderr, "%d ", mchain[mon[i]].vnum);
      while (mchain[p].vnum != vfirst)
	{
	  fprintf(stderr, "%d ", mchain[p].vnum);
	  p = mchain[p].next;
	}
    }
  fprintf(stderr, "\n");
#endif

  op_idx = 0;
  for (i = 0; i < nmonpoly; i++)
    {
      vcount = 1;
      processed = FALSE;
      vfirst = mchain[mon[i]].vnum;
      ymax = ymin = vert[vfirst].pt;
      posmax = /*posmin = */mon[i];
      mchain[mon[i]].marked = TRUE;
      p = mchain[mon[i]].next;
      while ((v = mchain[p].vnum) != vfirst)
	{
	 if (mchain[p].marked)
	   {
	     processed = TRUE;
	     break;		/* break from while */
	   }
	 else
	   mchain[p].marked = TRUE;

	  if (_greater_than(&vert[v].pt, &ymax))
	    {
	      ymax = vert[v].pt;
	      posmax = p;
	    }
	  if (_less_than(&vert[v].pt, &ymin))
	    {
	      ymin = vert[v].pt;
	      //posmin = p;
	    }
	  p = mchain[p].next;
	  vcount++;
       }

      if (processed)		/* Go to next polygon */
	continue;
      
      if (vcount == 3)		/* already a triangle */
	{
	  op[op_idx][0] = mchain[p].vnum;
	  op[op_idx][1] = mchain[mchain[p].next].vnum;
	  op[op_idx][2] = mchain[mchain[p].prev].vnum;
	  op_idx++;
	}
      else			/* triangulate the polygon */
	{
	  v = mchain[mchain[posmax].next].vnum;
	  if (_equal_to(&vert[v].pt, &ymin))
	    {			/* LHS is a single line */
	      triangulate_single_polygon(nvert, posmax, TRI_LHS, op);
	    }
	  else
	    triangulate_single_polygon(nvert, posmax, TRI_RHS, op);
	}
    }
  
#ifdef DEBUG
  for (i = 0; i < op_idx; i++)
    fprintf(stderr, "tri #%d: (%d, %d, %d)\n", i, op[i][0], op[i][1],
	   op[i][2]);
#endif
  return op_idx;
}


/* A greedy corner-cutting algorithm to triangulate a y-monotone 
 * polygon in O(n) time.
 * Joseph O-Rourke, Computational Geometry in C.
 */
static int triangulate_single_polygon(
     int nvert,
     int posmax,
     int side,
     int op[][3])
{
  register int v;
  int rc[SEGSIZE], ri = 0;	/* reflex chain */
  int endv, tmp, vpos;
  
  if (side == TRI_RHS)		/* RHS segment is a single segment */
    {
      rc[0] = mchain[posmax].vnum;
      tmp = mchain[posmax].next;
      rc[1] = mchain[tmp].vnum;
      ri = 1;
      
      vpos = mchain[tmp].next;
      v = mchain[vpos].vnum;
      
      if ((endv = mchain[mchain[posmax].prev].vnum) == 0)
	endv = nvert;
    }
  else				/* LHS is a single segment */
    {
      tmp = mchain[posmax].next;
      rc[0] = mchain[tmp].vnum;
      tmp = mchain[tmp].next;
      rc[1] = mchain[tmp].vnum;
      ri = 1;

      vpos = mchain[tmp].next;
      v = mchain[vpos].vnum;

      endv = mchain[posmax].vnum;
    }
  
  while ((v != endv) || (ri > 1))
    {
      if (ri > 0)		/* reflex chain is non-empty */
	{
	  if (CROSS(vert[v].pt, vert[rc[ri - 1]].pt, 
		    vert[rc[ri]].pt) > 0)
	    {			/* convex corner: cut if off */
	      op[op_idx][0] = rc[ri - 1];
	      op[op_idx][1] = rc[ri];
	      op[op_idx][2] = v;
	      op_idx++;	     
	      ri--;
	    }
	  else		/* non-convex */
	    {		/* add v to the chain */
	      ri++;
	      rc[ri] = v;
	      vpos = mchain[vpos].next;
	      v = mchain[vpos].vnum;
	    }
	}
      else			/* reflex-chain empty: add v to the */
	{			/* reflex chain and advance it  */
	  rc[++ri] = v;
	  vpos = mchain[vpos].next;
	  v = mchain[vpos].vnum;
	}
    } /* end-while */
  
  /* reached the bottom vertex. Add in the triangle formed */
  op[op_idx][0] = rc[ri - 1];
  op[op_idx][1] = rc[ri];
  op[op_idx][2] = v;
  op_idx++;	     
  ri--;
  
  return 0;
}


static int initialise(int n)
{
  register int i;

  for (i = 1; i <= n; i++)
    seg[i].is_inserted = FALSE;

  generate_random_ordering(n);
  
  return 0;
}

/* Input specified as contours.
 * Outer contour must be anti-clockwise.
 * All inner contours must be clockwise.
 *  
 * Every contour is specified by giving all its points in order. No
 * point shoud be repeated. i.e. if the outer contour is a square,
 * only the four distinct endpoints shopudl be specified in order.
 *  
 * ncontours: #contours
 * cntr: An array describing the number of points in each
 *	 contour. Thus, cntr[i] = #points in the i'th contour.
 * vertices: Input array of vertices. Vertices for each contour
 *           immediately follow those for previous one. Array location
 *           vertices[0] must NOT be used (i.e. i/p starts from
 *           vertices[1] instead. The output triangles are
 *	     specified  w.r.t. the indices of these vertices.
 * triangles: Output array to hold triangles.
 *  
 * Enough space must be allocated for all the arrays before calling
 * this routine
 */


int triangulate_polygon( int ncontours, int cntr[], double (*vertices)[2], int (*triangles)[3])
{
  register int i;
  int nmonpoly, ccount, npoints/*, genus*/;
  int n;

  memset((void *)seg, 0, sizeof(seg));
  ccount = 0;
  i = 1;
  
  while (ccount < ncontours)
    {
      int j;
      int first, last;

      npoints = cntr[ccount];
      first = i;
      last = first + npoints - 1;
      for (j = 0; j < npoints; j++, i++)
	{
	  seg[i].v0.x = vertices[i][0];
	  seg[i].v0.y = vertices[i][1];

	  if (i == last)
	    {
	      seg[i].next = first;
	      seg[i].prev = i-1;
	      seg[i-1].v1 = seg[i].v0;
	    }
	  else if (i == first)
	    {
	      seg[i].next = i+1;
	      seg[i].prev = last;
	      seg[last].v1 = seg[i].v0;
	    }
	  else
	    {
	      seg[i].prev = i-1;
	      seg[i].next = i+1;
	      seg[i-1].v1 = seg[i].v0;
	    }
	  
	  seg[i].is_inserted = FALSE;
	}
      
      ccount++;
    }
  
  //*genus = ncontours - 1;
  n = i-1;

  initialise(n);
  construct_trapezoids(n);
  nmonpoly = monotonate_trapezoids(n);
  return triangulate_monotone_polygons(n, nmonpoly, triangles);
}


/* This function returns TRUE or FALSE depending upon whether the 
 * vertex is inside the polygon or not. The polygon must already have
 * been triangulated before this routine is called.
 * This routine will always detect all the points belonging to the 
 * set (polygon-area - polygon-boundary). The return value for points 
 * on the boundary is not consistent!!!
 */

int is_point_inside_polygon( double vertex[2] )
{
  point_t v;
  int trnum, rseg;
  trap_t *t;

  v.x = vertex[0];
  v.y = vertex[1];
  
  trnum = locate_endpoint(&v, &v, 1);
  t = &tr[trnum];
  
  if (t->state == ST_INVALID)
    return FALSE;
  
  if ((t->lseg <= 0) || (t->rseg <= 0))
    return FALSE;
  rseg = t->rseg;
  return _greater_than_equal_to(&seg[rseg].v1, &seg[rseg].v0);
}

#define USE_FAT_VTXDCL 0

// Water Tesselation test (v5, some WiP are checked in //depot/user/alex.hadjadj
// Current limitation: 
// - It's rather slow
// - It doesn't deal with certain bad cases and long triangles, creating holes and nasty looking edge, this is why the default grid value is 10 
//   (Expected normal value should be around 20) and that the frustum is pushed by 10 meters (expected final requirement at around 1m).
// - it's quite liberal in it's memory usage.
// - there's a lot of testing/compare/special case due to some slopiness on my part with maintaining polygones 
// - It relies on the fact that the simulation area is close to the camera (if not centered around it) : It should be able to with more variable positioning
//   by adding a second splitplane during the first lowres/highres split.
// - The file is currently included as a CPP file straight into water.cpp, this is disgusting.
// - Move to SPU. See particle renderer for edge buffer re-use and plants renderer for various other bits and pieces.
// - There's various ready to use tesselator to play with:
//   barycentric.h : add a verts in the middle, create 3 trees based on it.
//   linear.h : simple linear tesselation.
//   linearDualEdge.h : simple linear tesselation with 2 edge: 1 edge get tesselated to one level, the two other's to the second level.
//   progressiveLinear.h : progressive linear tesselation
//   progressiveLinear2.h : progressive linar tesselation with a different edge selection
//   progressiveLinearDualEdge.h : progressive linear tesselation with two level, based on Linear2



// All splitting/clipping operation operate on the following structure.
// To add extra components, just add and make sure that the add/sub/div/mul functions are updated to 
// operate as expected on them.
struct ALIGNAS(16) vtxDcl {
	Vector3 pos;
#if USE_FAT_VTXDCL	
	Vector3 col;
#endif // USE_FAT_VTXDCL
	
	inline void add(const vtxDcl &vtx)
	{
		pos += vtx.pos;
#if USE_FAT_VTXDCL		
		col += vtx.col;
#endif // USE_FAT_VTXDCL		
	}
	
	inline void sub(const vtxDcl &vtx)
	{
		pos -= vtx.pos;
#if USE_FAT_VTXDCL		
		col -= vtx.col;
#endif // USE_FAT_VTXDCL		
	}
	
	inline void div(const float d)
	{
		pos /= d;
#if USE_FAT_VTXDCL		
		col /= d;
#endif // USE_FAT_VTXDCL		
	}

	inline void mul(const float d)
	{
		pos *= d;
#if USE_FAT_VTXDCL		
		col *= d;
#endif // USE_FAT_VTXDCL		
	}
	
}
#if (__PS3) && VECTORIZED

#endif
;

// This is how we store triangles over the process.
struct triangle
{
	// Verts
	vtxDcl v[3];

#if __DEV
		void VectorMapDraw()
		{
			CVectorMap::DrawLine(v[0].pos,v[1].pos,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
			CVectorMap::DrawLine(v[2].pos,v[0].pos,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
			CVectorMap::DrawLine(v[1].pos,v[2].pos,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
		}
#endif // __DEV		
};

struct quadDcl
{
	// Verts
	vtxDcl v[4];

#if __DEV
		void VectorMapDraw()
		{
			CVectorMap::DrawLine(v[0].pos,v[1].pos,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
			CVectorMap::DrawLine(v[1].pos,v[2].pos,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
			CVectorMap::DrawLine(v[2].pos,v[3].pos,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
			CVectorMap::DrawLine(v[3].pos,v[0].pos,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
		}
#endif // __DEV		
};

spdPlane clipPlanes[6];

static atFixedArray<quadDcl,200> levelQuads;

// Store for triangles being treated,
// Todo: - make sure the code can deal when running out of those, by flushing the current list/rendering and then keep going
//       - Reduce size of working sets
typedef atSNode<triangle> linkedTriangle;
static atFixedArray<linkedTriangle,1000> linkedTrianglePool;

// Linked list
static atSList<triangle> lowResTriangle;
static atSList<triangle> highResTriangle;


static float FindDynamicWaterHeight_RT(s32 WorldX, s32 WorldY, Vector3 *pNormal, Color32* pCol, float *pSpeedUp);


//
// Triangle splitting Utilities
//
static bool isQuadVisible(const quadDcl &q, const grcViewport *vp)
{
	const int kNumPoints=4;
	// Go through each of the points and determine, for each axis individually, whether that point is to
	//   the outside of the frustum in the positive direction of that axis, whether it is outside of the
	//   frustum in the negative direction of that axis, or whether it is within the frustum with respect
	//   to that axis.
	int ClipStatus = 0;
	Vector3 CameraToVertex;
	Vector3 TanVFOV(vp->GetTanVFOV(), vp->GetTanVFOV(), vp->GetTanVFOV());
	Vector3 TanHFOV(vp->GetTanHFOV(), vp->GetTanHFOV(), vp->GetTanHFOV());
	for(int VertexIndex = 0; VertexIndex < kNumPoints && ClipStatus != 15; ++VertexIndex)
	{
		CameraToVertex.Subtract(q.v[VertexIndex].pos, vp->GetCameraMtx().d);
		Vector3 FrontDistV = -CameraToVertex.DotV(vp->GetCameraMtx().c);

		if((ClipStatus & 3) != 3)
		{
			Vector3 UpDistV = CameraToVertex.DotV(vp->GetCameraMtx().b);
			Vector3 AbsUpDistV(UpDistV);
			AbsUpDistV.Abs();
			if(AbsUpDistV.IsLessThan(TanVFOV * FrontDistV))
			{
				ClipStatus |= 3;
			}
			else
			{
				ClipStatus |= UpDistV.IsGreaterThan(ORIGIN) ? 2 : 1;
			}
		}

		if((ClipStatus & 12) != 12)
		{
			Vector3 RightDistV = CameraToVertex.DotV(vp->GetCameraMtx().a);
			Vector3 AbsRightDistV(RightDistV);
			AbsRightDistV.Abs();
			if(AbsRightDistV.IsLessThan(TanHFOV * FrontDistV))
			{
				ClipStatus |= 12;
			}
			else
			{
				ClipStatus |= RightDistV.IsGreaterThan(ORIGIN) ? 8 : 4;
			}
		}
	}

	return ClipStatus == 15;
}


static __forceinline Vector3 worldTogrid(const Vector3 &world, const float gridStep)
{
	Vector3 result;
	result.x = (float)((int)((world.x)/gridStep))*gridStep;
	result.y = (float)((int)((world.y)/gridStep))*gridStep;
	result.z = world.z;
	
	return result;
}


static __forceinline Vector3 worldTogridX(const Vector3 &world, const float gridStep)
{
	Vector3 result;
	result.x = (float)((int)((world.x)/gridStep))*gridStep;
	result.y = world.y;
	result.z = world.z;
	
	return result;
}


static __forceinline Vector3 worldTogridY(const Vector3 &world, const float gridStep)
{
	Vector3 result;
	result.x = world.x;
	result.y = (float)((int)((world.y)/gridStep))*gridStep;
	result.z = world.z;
	
	return result;
}


static __forceinline void OutputTriangle(const vtxDcl &p1, const vtxDcl &p2, const vtxDcl &p3)
{
	linkedTriangle &newTri = linkedTrianglePool.Append();
	newTri.Data.v[0] = p1;
	newTri.Data.v[1] = p2;
	newTri.Data.v[2] = p3;
	highResTriangle.Append(newTri);
}


// Look up the values from the rendering loop and push the verts.
// The filtering is a bit stupid and could be more clever, depending on how much tesselation we end up going for.
static void pushVtx(const vtxDcl &vtx)
{
#if 1
	float flooredBaseX = Floorf( (vtx.pos.x)*WATERGRIDSIZE_INVF);
	float flooredBaseY = Floorf( (vtx.pos.y)*WATERGRIDSIZE_INVF);
	
	s32 baseX = (s32)(WATERGRIDSIZE * flooredBaseX);
	s32 baseY = (s32)(WATERGRIDSIZE * flooredBaseY);
	Vector3 normal(0.0f,0.0f,1.0f);
	Color32 color;
	float height = FindDynamicWaterHeight_RT(baseX,baseY,&normal,&color,NULL);
	grcVertex(vtx.pos.x,vtx.pos.y,vtx.pos.z + height,normal.x,normal.y,normal.z,color,0.0f,0.0f);
#else
	float flooredBaseX1 = Floorf( (vtx.pos.x + 1.0f )*WATERGRIDSIZE_INVF);
	float flooredBaseY1 = Floorf( (vtx.pos.y + 0.0f )*WATERGRIDSIZE_INVF);
	
	float flooredBaseX2 = Floorf( (vtx.pos.x - 1.0f )*WATERGRIDSIZE_INVF);
	float flooredBaseY2 = Floorf( (vtx.pos.y + 0.0f )*WATERGRIDSIZE_INVF);

	float flooredBaseX3 = Floorf( (vtx.pos.x + 0.0f )*WATERGRIDSIZE_INVF);
	float flooredBaseY3 = Floorf( (vtx.pos.y + 1.0f )*WATERGRIDSIZE_INVF);

	float flooredBaseX4 = Floorf( (vtx.pos.x + 0.0f )*WATERGRIDSIZE_INVF);
	float flooredBaseY4 = Floorf( (vtx.pos.y - 1.0f )*WATERGRIDSIZE_INVF);
	
	s32 baseX1 = (s32)(WATERGRIDSIZE * flooredBaseX1);
	s32 baseY1 = (s32)(WATERGRIDSIZE * flooredBaseY1);
	Vector3 normal1(0.0f,0.0f,1.0f);
	Color32 color1;
	float height1 = FindDynamicWaterHeight_RT(baseX1,baseY1,&normal1,&color1,NULL);

	s32 baseX2 = (s32)(WATERGRIDSIZE * flooredBaseX2);
	s32 baseY2 = (s32)(WATERGRIDSIZE * flooredBaseY2);
	Vector3 normal2(0.0f,0.0f,1.0f);
	Color32 color2;
	float height2 = FindDynamicWaterHeight_RT(baseX2,baseY2,&normal2,&color2,NULL);

	s32 baseX3 = (s32)(WATERGRIDSIZE * flooredBaseX3);
	s32 baseY3 = (s32)(WATERGRIDSIZE * flooredBaseY3);
	Vector3 normal3(0.0f,0.0f,1.0f);
	Color32 color3;
	float height3 = FindDynamicWaterHeight_RT(baseX3,baseY3,&normal3,&color3,NULL);

	s32 baseX4 = (s32)(WATERGRIDSIZE * flooredBaseX4);
	s32 baseY4 = (s32)(WATERGRIDSIZE * flooredBaseY4);
	Vector3 normal4(0.0f,0.0f,1.0f);
	Color32 color4;
	float height4 = FindDynamicWaterHeight_RT(baseX4,baseY4,&normal4,&color4,NULL);
	
	float height = (height1 + height2 + height3 + height4)/4.0f;
	Vector3 normal = (normal1 + normal2 + normal3 + normal4)/4.0f;
	Color32 color;
	color.SetRed	((color1.GetRed()	+ color2.GetRed()	+ color3.GetRed()	+ color4.GetRed()	)/4);
	color.SetGreen	((color1.GetGreen()	+ color2.GetGreen()	+ color3.GetGreen()	+ color4.GetGreen()	)/4);
	color.SetBlue	((color1.GetBlue()	+ color2.GetBlue()	+ color3.GetBlue()	+ color4.GetBlue()	)/4);
	color.SetAlpha	((color1.GetAlpha()	+ color2.GetAlpha()	+ color3.GetAlpha()	+ color4.GetAlpha()	)/4);
	grcVertex(vtx.pos.x,vtx.pos.y,vtx.pos.z + height,normal.x,normal.y,normal.z,color,0.0f,0.0f);
#endif	
}


//
// Debug Rendering
// 
#if __DEV
static void newWaterVectorMapDraw()
{
	int triCount = levelQuads.GetCount();

	for(int i=0;i<triCount;i++)
	{
		levelQuads[i].VectorMapDraw();
	}
}
#endif // __DEV


// Water initialisation:
//Take the water quad and feed them as triangle down to the levelQuads list.
//
//The triangle orientation/winding order is important, if passed in wrong, the tesselation won't work
//
// Todo:
// Make the levelQuads array dynamically allocated rather than static : we could save space here
// Find a way of importing a drawable/free form mesh layed out by an artist
static void newWaterInit()
{
	for (int i = 0; i < m_nNumOfWaterQuads; i++)
	{
		CWaterQuad &quad = m_aQuads[i];
		Vector3 p1(m_aVertices[quad.Index1].m_X,m_aVertices[quad.Index1].m_Y,m_aVertices[quad.Index1].m_Z);
		Vector3 p2(m_aVertices[quad.Index1].m_X,m_aVertices[quad.Index3].m_Y,m_aVertices[quad.Index2].m_Z);
		Vector3 p3(m_aVertices[quad.Index2].m_X,m_aVertices[quad.Index3].m_Y,m_aVertices[quad.Index4].m_Z);
		Vector3 p4(m_aVertices[quad.Index2].m_X,m_aVertices[quad.Index1].m_Y,m_aVertices[quad.Index3].m_Z);

		// Tri1 = p1,p2,p3
		if( p1 != p2 && p1 != p3 && p2 != p3 && p3 != p4 && p1 != p4)
		{
			vtxDcl vtx1;
			vtx1.pos = p1;
			vtxDcl vtx2;
			vtx2.pos = p2;
			vtxDcl vtx3;
			vtx3.pos = p3;
			vtxDcl vtx4;
			vtx4.pos = p4;
			
			quadDcl &q = levelQuads.Append();
			q.v[0] = vtx1;
			q.v[1] = vtx2;
			q.v[2] = vtx3;
			q.v[3] = vtx4;
		}
	}
	
	for(int i=0;i<levelQuads.GetCount();i++)
	{
		for(int j=0;j<4;j++)
		{
			Displayf("%d %d %f %f %f\n",i,j,levelQuads[i].v[j].pos.x,levelQuads[i].v[j].pos.y,levelQuads[i].v[j].pos.z);
		}
	}
	__debugbreak();
}


static bool Intersect(const vtxDcl &ap1, const vtxDcl &ap2, const Vector3 &bp1, const Vector3 &bp2, vtxDcl& intersection)
{
    const Vector3 &vap1 = ap1.pos;
    const Vector3 &vap2 = ap2.pos;
    
    float denom = ((bp2.y - bp1.y)*(vap2.x - vap1.x)) - ((bp2.x - bp1.x)*(vap2.y - vap1.y));

    if(denom == 0.0f)
    {
		return false;
    }

    float nume_a = ((bp2.x - bp1.x)*(vap1.y - bp1.y)) - ((bp2.y - bp1.y)*(vap1.x - bp1.x));

    float ua = nume_a / denom;

    intersection = ap2;
	intersection.sub(ap1);
	intersection.mul(ua);
	intersection.add(ap1);

	return true;
}


static void ClipTriangle(const vtxDcl *in, vtxDcl *out, int &vtxCount, float xMin, float xMax, float yMin, float yMax)
{
	vtxDcl screenClippedA[8];
	int screenClippedCountA = 0;

	vtxDcl screenClippedB[8];
	int screenClippedCountB = 0;
	
	Vector3 clipLineA;
	Vector3 clipLineB;
	
	const vtxDcl *src;
	vtxDcl *dst;
	s32 srcCount;
	s32 dstCount;
	s32 i;
	
	// xMin Clipping
	clipLineA.Set(xMin,yMin-10.0f,0.0f);
	clipLineB.Set(xMin,yMax+10.0f,0.0f);

	src = in;
	dst = screenClippedA;
	srcCount = vtxCount;
	dstCount = 0;
	
	for(i = 0;i<srcCount-1;i++) 
	{
		if( src[i].pos.x < xMin )
		{
			if( src[i+1].pos.x > xMin )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}
		}
		else 
		{
			Assert(dstCount<8);
			dst[dstCount] = src[i];
			dstCount++;
			
			if( src[i+1].pos.x < xMin )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}		
		}
	}

	if( src[i].pos.x < xMin )
	{
		if( src[0].pos.x > xMin )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}
	}
	else 
	{
		Assert(dstCount<8);
		dst[dstCount] = src[i];
		dstCount++;
		
		if( src[0].pos.x < xMin )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}		
	}
	screenClippedCountA = dstCount;

	// yMin Clipping
	clipLineA.Set(xMin-10.0f,yMin,0.0f);
	clipLineB.Set(xMax+10.0f,yMin,0.0f);

	src = screenClippedA;
	dst = screenClippedB;
	srcCount = screenClippedCountA;
	dstCount = 0;
	
	for(i = 0;i<srcCount-1;i++)
	{
		if( src[i].pos.y < yMin )
		{
			if( src[i+1].pos.y > yMin )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}
		}
		else 
		{
			Assert(dstCount<8);
			dst[dstCount] = src[i];
			dstCount++;
			
			if( src[i+1].pos.y < yMin )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}		
		}
	}

	if( src[i].pos.y < yMin )
	{
		if( src[0].pos.y > yMin )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}
	}
	else 
	{
		Assert(dstCount<8);
		dst[dstCount] = src[i];
		dstCount++;
		
		if( src[0].pos.y < yMin )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}		
	}
	screenClippedCountB = dstCount;

	// xMax Clipping
	clipLineA.Set(xMax,yMin-10.0f,0.0f);
	clipLineB.Set(xMax,yMax+10.0f,0.0f);

	src = screenClippedB;
	dst = screenClippedA;
	srcCount = screenClippedCountB;
	dstCount = 0;
	
	for(i = 0;i<srcCount-1;i++)
	{
		if( src[i].pos.x > xMax )
		{
			if( src[i+1].pos.x < xMax )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}
		}
		else 
		{
			Assert(dstCount<8);
			dst[dstCount] = src[i];
			dstCount++;
			
			if( src[i+1].pos.x > xMax )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}		
		}
	}

	if( src[i].pos.x > xMax )
	{
		if( src[0].pos.x < xMax )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}
	}
	else 
	{
		Assert(dstCount<8);
		dst[dstCount] = src[i];
		dstCount++;
		
		if( src[0].pos.x > xMax )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}		
	}
	screenClippedCountA = dstCount;	

	// yMax Clipping
	clipLineA.Set(xMin-10.0f,yMax,0.0f);
	clipLineB.Set(xMax+10.0f,yMax,0.0f);

	src = screenClippedA;
	dst = out;
	srcCount = screenClippedCountA;
	dstCount = 0;
	
	for(i = 0;i<srcCount-1;i++)
	{
		if( src[i].pos.y > yMax )
		{
			if( src[i+1].pos.y < yMax )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}
		}
		else 
		{
			Assert(dstCount<8);
			dst[dstCount] = src[i];
			dstCount++;
			
			if( src[i+1].pos.y > yMax )
			{
				Assert(dstCount<8);
				bool res = Intersect(src[i],src[i+1],clipLineA,clipLineB,dst[dstCount]);
				dstCount = res ? (dstCount + 1) : dstCount;
			}		
		}
	}

	if( src[i].pos.y > yMax )
	{
		if( src[0].pos.y < yMax )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}
	}
	else 
	{
		Assert(dstCount<8);
		dst[dstCount] = src[i];
		dstCount++;
		
		if( src[0].pos.y > yMax )
		{
			Assert(dstCount<8);
			bool res = Intersect(src[i],src[0],clipLineA,clipLineB,dst[dstCount]);
			dstCount = res ? (dstCount + 1) : dstCount;
		}		
	}

	vtxCount = dstCount;
}


static void SplitTriangle(const vtxDcl *in, const int inCount, float gridStep, const grcViewport *vp)
{
	Vector3 minPos = in[0].pos;
	Vector3 maxPos = in[0].pos;
	Vector3 center = in[0].pos;
	
	for(int i=0;i<inCount;i++)
	{
		minPos.Min(minPos,in[i].pos);
		maxPos.Max(maxPos,in[i].pos);
		center += in[i].pos;
	}
	
	center *= 1.0f/(float)inCount;
	minPos = worldTogrid(minPos,gridStep);
	
	minPos -= Vector3(gridStep,gridStep,0.0f);

	maxPos = worldTogrid(maxPos,gridStep);
	
	maxPos += Vector3(gridStep,gridStep,0.0f);

	float x = minPos.x;
	int countX = (int)((maxPos.x - minPos.x)/gridStep);
	int countY = (int)((maxPos.y - minPos.y)/gridStep);
	for(int i=0;i<countX;i++,x+=gridStep)
	{
		float y = minPos.y;
		for(int j=0;j<countY;j++,y+=gridStep)
		{
			Vector4 sphere(x,y,center.z,gridStep*2.0f);
			if( !vp->IsSphereVisible(sphere) )
				continue;
				
			Vector3 a = Vector3(x,y,0);
			Vector3 b = Vector3(x+gridStep,y+gridStep,0);

			vtxDcl out[8];
			int vtxCount=inCount;
			ClipTriangle(in, out, vtxCount, x, x+gridStep, y, y+gridStep);

			if( vtxCount > 2)
			{
				int k=0;
				Color32 col1;
				Color32 col2;
				int startIdx = 0;
				Vector3 startPos = out[0].pos;
				for(k=1;k<vtxCount;k++)
				{
					if( out[k].pos.x < startPos.x)
					{
						startPos = out[k].pos;
						startIdx = k;
					}
					else if( out[k].pos.x == startPos.x && out[k].pos.y < startPos.y )
					{
						startPos = out[k].pos;
						startIdx = k;
					}
				}
				
				int idx = startIdx;
				vtxDcl aa,bb,cc;
				aa = out[idx];
				idx = (idx + 1 == vtxCount) ? 0 : (idx+1);
				bb = out[idx];
				idx = (idx + 1 == vtxCount) ? 0 : (idx+1);

				for(k=2;k<vtxCount;k++)
				{
					cc = out[idx];
					idx = (idx + 1 == vtxCount) ? 0 : (idx+1);
					OutputTriangle(aa, bb, cc);
					bb = cc;
				}
			}			
		}
	}
}

static void CCWSort(Vector3 *vecIn, int numVrt)
{
	Vector3 center;
	
	for(int i=0;i<numVrt;i++)
	{
		center += vecIn[i];
	}
	center /= (float)numVrt;
	
	float angles[16];
	
	Vector3 a = center - vecIn[0];
	angles[0] = a.AngleZ(a);

	for(int i=1;i<numVrt;i++)
	{	
		Vector3 b = center - vecIn[i];
		angles[i] = a.AngleZ(b);
	}
	
	int i,j;
	i=1;
	j=2;

	while (i<=numVrt-1)
	{
		if( angles[i-1] <= angles[i] )
		{
			i=j;
			j=j+1;
		}
		else
		{
			Vector3 tmp = vecIn[i-1];
			float tmpA = angles[i-1];
			vecIn[i-1] = vecIn[i];
			vecIn[i] = tmp;
			angles[i-1] = angles[i];
			angles[i] = tmpA;

			i=i-1;
			if (i==0)
				i=1;
		}
	}
}
// Split the level triangles for rendering and clear all lists...
// It outputs two list of triangles : lowResTriangle and highResTriangle.
// lowRes are rendered as is, highRes are tesselated further.
// Todo:
// - There's a few cases where long triangles are returned by the tesselator, those should be split further along the sim grid, probably 
// by radding them to workList1Triangle
// - There's a lot of copying of verts and a couple of special cases because the vert order is not maintained in a quad throughout the process, this should be fixed.
// - Potential memory optimization: generate the verts/idx buffer straight out of the function, rather than building two lists to be rendered later on.
// - Inject the end of world/looping triangles as if part of the level set, if done cleverly, we can probably push them straight to the highres triangle list.
static void splitWaterTriangle(const grcViewport *vp, float highLowsplitDistance, float nearClipFactor,float gridStep, bool DEV_ONLY(debugRender))
{
	// Reset various lists
	linkedTrianglePool.Reset();
	lowResTriangle.Reset();
	//workList1Triangle.Reset();
	highResTriangle.Reset();

	int triCount = levelQuads.GetCount();

	// Set plane clip out of frutrum
	Vector4 scale(-1.0f,-1.0f,-1.0f,1.0f);

	for(int i=0;i<6;i++)
	{	
		Vector4 plane = vp->GetFrustumClipPlane(i);
		plane *= scale;
		clipPlanes[i].SetCoefficientVector(plane);
	}

	Vector3 planePos;
	Vector3 planeNormal;
	
	// Move the following clip plane away.
	// Todo : Maybe try using a wider frustum rather than just moving the planes away. 
	const s32 clipPlanesCode[5] = {	grcViewport::CLIP_PLANE_NEAR, 
									grcViewport::CLIP_PLANE_LEFT, 
									grcViewport::CLIP_PLANE_RIGHT, 
									grcViewport::CLIP_PLANE_TOP, 
									grcViewport::CLIP_PLANE_BOTTOM };

	for(int planeId = 0;planeId < 5;planeId++)
	{
		int planeIndex = clipPlanesCode[planeId];
		planePos = clipPlanes[planeIndex].GetPos();
		planeNormal = clipPlanes[planeIndex].GetNormal();
		planePos += planeNormal*nearClipFactor;
		clipPlanes[planeIndex].SetPos(planePos);
	}

	Vector3 pos = vp->GetCameraMtx().d;
#if __DEV
	if( debugRender )
	{
		CVectorMap::DrawLine(pos,pos + vp->GetCameraMtx().a * highLowsplitDistance,Color32(0xff,0x00,0x00),Color32(0xff,0x00,0x00));
		CVectorMap::DrawLine(pos,pos + vp->GetCameraMtx().b * highLowsplitDistance,Color32(0x00,0xff,0x00),Color32(0x00,0xff,0x00));
		CVectorMap::DrawLine(pos,pos + vp->GetCameraMtx().c * highLowsplitDistance,Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
	}
#endif // __DEV
	
	// First pass over triangles:
	// Clip every triangle allong the highlevel distance Plane, putting the one behind in the lowres list and the other one 
	// in the worklist to be tesselated further
	// Todo:
	// - Use a single split function to output two polys per triangle fed, rasther than doing two separate splits.

	spdPlane clipHighLevelPlane = clipPlanes[grcViewport::CLIP_PLANE_NEAR];
	clipHighLevelPlane.SetPos(pos-vp->GetCameraMtx().c * highLowsplitDistance);

	spdPlane clipInvHighLevelPlane(clipHighLevelPlane);
	clipInvHighLevelPlane.NegateNormal();

	for(int i=0;i<triCount;i++)
	{
		if( isQuadVisible(levelQuads[i],vp) )
		{
			Vector3 vertsIn[4];
			Vector3 vertsOut[8];
			
			vertsIn[0] = levelQuads[i].v[0].pos;
			vertsIn[1] = levelQuads[i].v[1].pos;
			vertsIn[2] = levelQuads[i].v[2].pos;
			vertsIn[3] = levelQuads[i].v[3].pos;
			
			// Split visible triangles : low res are behind the clip plane
			int lowResVtxCount = clipHighLevelPlane.Clip(vertsIn, 4, vertsOut, 8);

			if( lowResVtxCount )
			{
				//Vector3 a,b,c;
				//
				//a = vertsOut[0];
				//b = vertsOut[1];
				//
				//for(int j=2;j<lowResVtxCount;j++)
				//{
				//	c = vertsOut[2];
				//	
				//	linkedTriangle &newTri = linkedTrianglePool.Append();
				//	newTri.Data.v[0].pos = a;
				//	newTri.Data.v[1].pos = b;
				//	newTri.Data.v[2].pos = c;
				//	lowResTriangle.Append(newTri);
				//	
				//	b = c;
				//}

				if( sortQuads )
				{
					CCWSort(vertsOut,lowResVtxCount);
				}

				for(int j=0;j<lowResVtxCount-1;j++)
				{
					CVectorMap::DrawLine(vertsOut[j],vertsOut[j+1],Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
				}
				CVectorMap::DrawLine(vertsOut[lowResVtxCount-1],vertsOut[0],Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));

				Vector3 center = vertsOut[0];
				
				for(int i=1;i<lowResVtxCount;i++)
				{
					center += vertsOut[i];
				}
				center /= (float)lowResVtxCount;
				
				for(int j=0;j<lowResVtxCount;j++)
				{
					int color = ((float)j/(float)lowResVtxCount) * 255.0f;
					Color32 col(color,color,color);
					CVectorMap::DrawLine(center,vertsOut[j],col,col);
				}
				
				if( sortQuads && lowResVtxCount > 3)
				{
					int cntr[1];
					int ncontours = 1;
					cntr[0] = 4;
					
					double verts[50][2];
					int tris[50][3];
					
					for(int j=0;j<lowResVtxCount;j++)
					{
						verts[j+1][0] = vertsOut[j].x;
						verts[j+1][1] = vertsOut[j].y;
					}
					
					int triCountRes = triangulate_polygon(ncontours, cntr, verts, tris);
					
					for(int j=0;j<triCountRes;j++)
					{
						Vector3 a,b,c;
						a.x = vert[tris[j][0]].pt.x;
						a.y = vert[tris[j][0]].pt.y;
						a.z = 0.0f;
						b.x = vert[tris[j][1]].pt.x;
						b.y = vert[tris[j][1]].pt.y;
						b.z = 0.0f;
						c.x = vert[tris[j][2]].pt.x;
						c.y = vert[tris[j][2]].pt.y;
						c.z = 0.0f;
						
						CVectorMap::DrawLine(a,b,Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
						CVectorMap::DrawLine(b,c,Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
						CVectorMap::DrawLine(c,a,Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
					}
				}
				else
				{
					CVectorMap::DrawLine(vertsOut[0],vertsOut[1],Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
					CVectorMap::DrawLine(vertsOut[1],vertsOut[2],Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
					CVectorMap::DrawLine(vertsOut[2],vertsOut[0],Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
				}
				//if( lowResVtxCount > 3 )
				//{
				//	// The curlies are here to avoid some gcc weirdness where the two tris
				//	// end up with the same value, it could be me, though.
				//	{
				//	}
				//	
				//	{
				//		linkedTriangle &newTri = linkedTrianglePool.Append();
				//		newTri.Data.v[0].pos = vertsOut[3];
				//		newTri.Data.v[1].pos = vertsOut[1];
				//		newTri.Data.v[2].pos = vertsOut[2];
				//		lowResTriangle.Append(newTri);
				//	}
				//}
				//else 
				//{
				//	linkedTriangle &newTri = linkedTrianglePool.Append();
				//	newTri.Data.v[0].pos = vertsOut[0];
				//	newTri.Data.v[1].pos = vertsOut[1];
				//	newTri.Data.v[2].pos = vertsOut[2];
				//	lowResTriangle.Append(newTri);
				//}
			}

			int highResVtxCount = clipInvHighLevelPlane.Clip(vertsIn, 4, vertsOut, 8);

			if( highResVtxCount )
			{
				vtxDcl verts[8];
				for(int i=0;i<highResVtxCount;i++)
				{
#if USE_FAT_VTXDCL
					float color=((float)i/highResVtxCount)*255.0f;
					verts[i].col = Vector3(color,color,color);
#endif // USE_FAT_VTXDCL					
					verts[i].pos = vertsOut[i];
				}

				SplitTriangle(verts,highResVtxCount,gridStep,vp);
			}
		}
	}
}


// Render...
// Maybe use a separate larger, fixed size vertex buffer and a generated index stream, to reduce the numbers of drawcalls
// 
static void newWaterRender()
{
	int lowResVtxCount = lowResTriangle.GetNumItems()*3;
	if( lowResVtxCount )
	{
		int batchSize = Min(grcBeginMax3,lowResVtxCount);
		int currentBatch = batchSize;
		
		linkedTriangle *tri = lowResTriangle.PopHead();
		
		// Go through lowres triangle and render them as is
		// The batching logic is retarded, and can probably be streamlined.
#if __BANK
		grcBegin(m_bWireFrame ? drawLineStrip : drawTris,batchSize);
#else // __BANK
		grcBegin(drawTris,batchSize);
#endif // __BANK		
		while(tri)
		{
			if( 0 == currentBatch )
			{
				grcEnd();
				lowResVtxCount -= batchSize;
				batchSize = Min(grcBeginMax3,lowResVtxCount);
				currentBatch = batchSize;
#if __BANK
				grcBegin(m_bWireFrame ? drawLineStrip : drawTris,batchSize);
#else // __BANK
				grcBegin(drawTris,batchSize);
#endif // __BANK		
			}
			
#if __DEV
			if( debugRender )
			{
				CVectorMap::DrawLine(tri->Data.v[0].pos,tri->Data.v[1].pos,Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
				CVectorMap::DrawLine(tri->Data.v[1].pos,tri->Data.v[2].pos,Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
				CVectorMap::DrawLine(tri->Data.v[2].pos,tri->Data.v[0].pos,Color32(0x00,0x00,0xff),Color32(0x00,0x00,0xff));
			}
#endif // __DEV

			grcVertex(tri->Data.v[0].pos.x,tri->Data.v[0].pos.y,tri->Data.v[0].pos.z,0.0f,0.0f,1.0f,Color32(0.0f, 0.0f, 0.0f, 1.0f),0.0f,0.0f);
			grcVertex(tri->Data.v[1].pos.x,tri->Data.v[1].pos.y,tri->Data.v[1].pos.z,0.0f,0.0f,1.0f,Color32(0.0f, 0.0f, 0.0f, 1.0f),0.0f,0.0f);
			grcVertex(tri->Data.v[2].pos.x,tri->Data.v[2].pos.y,tri->Data.v[2].pos.z,0.0f,0.0f,1.0f,Color32(0.0f, 0.0f, 0.0f, 1.0f),0.0f,0.0f);
			
			currentBatch -= 3;
			tri = lowResTriangle.PopHead();
		}

		grcEnd();
	}

	
	// Render highres tris the same way the lowres are
	int highResVtxCount = highResTriangle.GetNumItems()*3;
	if( highResVtxCount )
	{
		int batchSize = Min(grcBeginMax3,highResVtxCount);
		int currentBatch = batchSize;

		linkedTriangle *tri = highResTriangle.PopHead();
#if __BANK
		grcBegin(m_bWireFrame ? drawLineStrip : drawTris,batchSize);
#else // __BANK
		grcBegin(drawTris,batchSize);
#endif // __BANK		
		
		while(tri)
		{
			if( 0 == currentBatch )
			{
				grcEnd();
				highResVtxCount -= batchSize;
				batchSize = Min(grcBeginMax3,highResVtxCount);
				currentBatch = batchSize;
#if __BANK
				grcBegin(m_bWireFrame ? drawLineStrip : drawTris,batchSize);
#else // __BANK
				grcBegin(drawTris,batchSize);
#endif // __BANK		
			}

#if __DEV
			if( debugRender )
			{
				CVectorMap::DrawLine(tri->Data.v[0].pos,tri->Data.v[1].pos,Color32(0xff,0x00,0x00),Color32(0x00,0xff,0x00));
				CVectorMap::DrawLine(tri->Data.v[1].pos,tri->Data.v[2].pos,Color32(0x00,0xff,0x00),Color32(0x00,0x00,0xff));
				CVectorMap::DrawLine(tri->Data.v[2].pos,tri->Data.v[0].pos,Color32(0x00,0x00,0xff),Color32(0xff,0x00,0x00));
			}
#endif //__DEv
	
			pushVtx(tri->Data.v[0]);
			pushVtx(tri->Data.v[1]);
			pushVtx(tri->Data.v[2]);

			currentBatch -= 3;
			tri = highResTriangle.PopHead();
		}

		grcEnd();
	}
}




