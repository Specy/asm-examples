//
// asmOS: the line-of-sight trace, P_DivlineSide to P_CrossBSPNode, as p_sight.c had it but in
// fewer instructions, which is what the emulator charges by. p_sight.c includes this in place of
// its own. The answers, and the slopes and validcounts left behind, are the original's.
//
// What changed: which side of a line a point is on is worked out in line (P_SideOf), with the
// trace's start, end and steps in locals rather than read from strace through a pointer at every
// test; and P_CrossBSPNode walks the tree with a stack of its own rather than by calling itself.
//

// P_DivlineSide's sum, for a divline given by its parts. The vertical case compares x with node's
// y, as DOOM always has: an answer the demos were recorded with, so it stays.
static inline int P_SideOf (fixed_t x, fixed_t y, fixed_t nx, fixed_t ny, fixed_t ndx, fixed_t ndy)
{
    fixed_t	left;
    fixed_t	right;

    if (!ndx)
    {
	if (x==nx)
	    return 2;

	if (x <= nx)
	    return ndy > 0;

	return ndy < 0;
    }

    if (!ndy)
    {
	if (x==ny)
	    return 2;

	if (y <= ny)
	    return ndx < 0;

	return ndx > 0;
    }

    left =  (ndy>>FRACBITS) * ((x - nx)>>FRACBITS);
    right = ((y - ny)>>FRACBITS) * (ndx>>FRACBITS);

    if (right < left)
	return 0;	// front side

    if (left == right)
	return 2;
    return 1;		// back side
}

//
// P_DivlineSide
// Returns side 0 (front), 1 (back), or 2 (on).
//
int
P_DivlineSide
( fixed_t	x,
  fixed_t	y,
  divline_t*	node )
{
    return P_SideOf (x, y, node->x, node->y, node->dx, node->dy);
}


//
// P_InterceptVector2
// Returns the fractional intercept point
// along the first divline.
// This is only called by the addthings and addlines traversers.
//
fixed_t
P_InterceptVector2
( divline_t*	v2,
  divline_t*	v1 )
{
    fixed_t	frac;
    fixed_t	num;
    fixed_t	den;

    den = FixedMul (v1->dy>>8,v2->dx) - FixedMul(v1->dx>>8,v2->dy);

    if (den == 0)
	return 0;
    //	I_Error ("P_InterceptVector: parallel");

    num = FixedMul ( (v1->x - v2->x)>>8 ,v1->dy) +
	FixedMul ( (v2->y - v1->y)>>8 , v1->dx);
    frac = FixedDiv (num , den);

    return frac;
}

//
// P_CrossSubsector
// Returns true
//  if strace crosses the given subsector successfully.
//
boolean P_CrossSubsector (int num)
{
    seg_t*		seg;
    line_t*		line;
    int			s1;
    int			s2;
    int			count;
    subsector_t*	sub;
    sector_t*		front;
    sector_t*		back;
    fixed_t		opentop;
    fixed_t		openbottom;
    divline_t		divl;
    vertex_t*		v1;
    vertex_t*		v2;
    fixed_t		frac;
    fixed_t		slope;

    // the trace, which nothing here changes
    const fixed_t	sx = strace.x, sy = strace.y, sdx = strace.dx, sdy = strace.dy;
    const fixed_t	ex = t2x, ey = t2y;
    const int		valid = validcount;

#ifdef RANGECHECK
    if (num>=numsubsectors)
	I_Error ("P_CrossSubsector: ss %i with numss = %i",
		 num,
		 numsubsectors);
#endif

    sub = &subsectors[num];

    // check lines
    count = sub->numlines;
    seg = &segs[sub->firstline];

    for ( ; count ; seg++, count--)
    {
	line = seg->linedef;

	// allready checked other side?
	if (line->validcount == valid)
	    continue;

	line->validcount = valid;

	v1 = line->v1;
	v2 = line->v2;
	s1 = P_SideOf (v1->x, v1->y, sx, sy, sdx, sdy);
	s2 = P_SideOf (v2->x, v2->y, sx, sy, sdx, sdy);

	// line isn't crossed?
	if (s1 == s2)
	    continue;

	divl.x = v1->x;
	divl.y = v1->y;
	divl.dx = v2->x - v1->x;
	divl.dy = v2->y - v1->y;
	s1 = P_SideOf (sx, sy, divl.x, divl.y, divl.dx, divl.dy);
	s2 = P_SideOf (ex, ey, divl.x, divl.y, divl.dx, divl.dy);

	// line isn't crossed?
	if (s1 == s2)
	    continue;

        // Backsector may be NULL if this is an "impassible
        // glass" hack line.

        if (line->backsector == NULL)
        {
            return false;
        }

	// stop because it is not two sided anyway
	// might do this after updating validcount?
	if ( !(line->flags & ML_TWOSIDED) )
	    return false;

	// crosses a two sided line
	front = seg->frontsector;
	back = seg->backsector;

	// no wall to block sight with?
	if (front->floorheight == back->floorheight
	    && front->ceilingheight == back->ceilingheight)
	    continue;

	// possible occluder
	// because of ceiling height differences
	if (front->ceilingheight < back->ceilingheight)
	    opentop = front->ceilingheight;
	else
	    opentop = back->ceilingheight;

	// because of ceiling height differences
	if (front->floorheight > back->floorheight)
	    openbottom = front->floorheight;
	else
	    openbottom = back->floorheight;

	// quick test for totally closed doors
	if (openbottom >= opentop)
	    return false;		// stop

	frac = P_InterceptVector2 (&strace, &divl);

	if (front->floorheight != back->floorheight)
	{
	    slope = FixedDiv (openbottom - sightzstart , frac);
	    if (slope > bottomslope)
		bottomslope = slope;
	}

	if (front->ceilingheight != back->ceilingheight)
	{
	    slope = FixedDiv (opentop - sightzstart , frac);
	    if (slope < topslope)
		topslope = slope;
	}

	if (topslope <= bottomslope)
	    return false;		// stop
    }
    // passed the subsector ok
    return true;
}



//
// P_CrossBSPNode
// Returns true
//  if strace crosses the given node successfully.
//
// The original, which calls itself: kept for a tree deeper than P_CrossBSPNode's stack.
static boolean P_CrossBSPNodeDeep (int bspnum)
{
    node_t*	bsp;
    int		side;

    for (;;)
    {
	if (bspnum & NF_SUBSECTOR)
	{
	    if (bspnum == -1)
		return P_CrossSubsector (0);
	    else
		return P_CrossSubsector (bspnum&(~NF_SUBSECTOR));
	}

	bsp = &nodes[bspnum];

	// decide which side the start point is on
	side = P_SideOf (strace.x, strace.y, bsp->x, bsp->y, bsp->dx, bsp->dy);
	if (side == 2)
	    side = 0;	// an "on" should cross both sides

	// cross the starting side
	if (!P_CrossBSPNodeDeep (bsp->children[side]) )
	    return false;

	// the partition plane is crossed here
	if (side == P_SideOf (t2x, t2y, bsp->x, bsp->y, bsp->dx, bsp->dy))
	{
	    // the line doesn't touch the other side
	    return true;
	}

	// cross the ending side
	bspnum = bsp->children[side^1];
    }
}

//
// The same walk without calling itself: the nodes whose starting side is being crossed wait on a
// stack. A subsector that blocks the trace ends it all, as false returned up every call did; one
// that lets it through answers the node waiting last, which then either lets the trace through
// too (the end is on the same side) or crosses its other side in its place, that side's answer
// being its own, as the original's last call was.
//
enum { SIGHT_STACK = 128 };

boolean P_CrossBSPNode (int bspnum)
{
    node_t*		waiting[SIGHT_STACK];
    unsigned char	sides[SIGHT_STACK];
    int			count = 0;
    node_t*		bsp;
    int			side;
    const fixed_t	sx = strace.x, sy = strace.y, ex = t2x, ey = t2y;

    for (;;)
    {
	// down the starting sides to a subsector, or to a node too deep for the stack, whose whole
	// subtree P_CrossBSPNodeDeep answers for
	while (!(bspnum & NF_SUBSECTOR) && count < SIGHT_STACK)
	{
	    bsp = &nodes[bspnum];
	    side = P_SideOf (sx, sy, bsp->x, bsp->y, bsp->dx, bsp->dy);
	    if (side == 2)
		side = 0;	// an "on" should cross both sides
	    waiting[count] = bsp;
	    sides[count++] = side;
	    bspnum = bsp->children[side];
	}

	if (!(bspnum & NF_SUBSECTOR) ? !P_CrossBSPNodeDeep (bspnum)
	    : !P_CrossSubsector (bspnum == -1 ? 0 : bspnum&(~NF_SUBSECTOR)))
	    return false;

	// up to the first node whose ending side is still to cross
	for (;;)
	{
	    if (!count)
		return true;
	    bsp = waiting[--count];
	    side = sides[count];
	    if (side != P_SideOf (ex, ey, bsp->x, bsp->y, bsp->dx, bsp->dy))
		break;
	}
	bspnum = bsp->children[side^1];
    }
}
