//
// asmOS: R_RenderSegLoop, the walls' loop over screen columns, as r_segs.c had it but faster in an
// emulator that charges by the instruction. r_segs.c includes this in place of its own.
//
// The loop's state is in locals rather than the globals r_segs.c keeps it in: R_GetColumn and the
// column drawer are called in the loop, so the compiler had to load and store each global again
// around every call. The globals get their final values after the loop, as the original left them.
// And a column of a wall goes straight to doom_column (doom_simd.asm), with R_DrawColumn's own
// arithmetic, rather than through colfunc and R_DrawColumn's globals, while the drawer is the
// ordinary one and Game > Fast Drawing is on (or to doom_column2 for R_DrawColumnLow, in low
// detail); otherwise through colfunc, as ever.
//
// R_GetColumn of a column drawn from a patch calls W_CacheLumpNum, which finds the lump and sets
// its zone tag again: the same lump for every column of a wall the patch covers. R_SegGetColumn
// keeps the last lump of each tier, as long as nothing has been allocated since, which only
// R_GenerateComposite does here.
//

// One tier of a wall's column, yl to yh, as colfunc () would draw it. direct is 1 for doom_column,
// 2 for doom_column2, 0 for colfunc.
static void R_SegColumn (int direct, int x, int yl, int yh, fixed_t texturemid,
                         fixed_t iscale, lighttable_t *colormap, byte *source)
{
    if (direct)
    {
        int count = yh - yl;
        if (count < 0)
            return;
        if (direct == 1)
            doom_column(ylookup[yl] + columnofs[x], count + 1,
                        texturemid + (yl - centery) * iscale, iscale, source, colormap);
        else
            doom_column2(ylookup[yl] + columnofs[x << 1], count + 1,
                         texturemid + (yl - centery) * iscale, iscale, source, colormap);
        return;
    }
    dc_x = x;
    dc_yl = yl;
    dc_yh = yh;
    dc_texturemid = texturemid;
    dc_iscale = iscale;
    dc_colormap = colormap;
    dc_source = source;
    colfunc ();
}

// The lump a tier's columns last came from, and where W_CacheLumpNum had it.
typedef struct { int lump; byte *data; } SegLump;

// R_GetColumn, with the tier's last lump remembered in *last; all three forgotten when a
// composite is made, as its Z_Malloc may purge cached lumps.
static inline byte *R_SegGetColumn (int tex, int col, SegLump *last, SegLump *tiers)
{
    int lump, ofs;

    col &= texturewidthmask[tex];
    lump = texturecolumnlump[tex][col];
    ofs = texturecolumnofs[tex][col];

    if (lump > 0)
    {
        if (lump != last->lump)
        {
            last->data = W_CacheLumpNum(lump, PU_CACHE);
            last->lump = lump;
        }
        return last->data + ofs;
    }

    if (!texturecomposite[tex])
    {
        R_GenerateComposite (tex);
        tiers[0].lump = tiers[1].lump = tiers[2].lump = 0;
    }

    return texturecomposite[tex] + ofs;
}

void R_RenderSegLoop (void)
{
    angle_t		angle;
    unsigned		index;
    int			yl;
    int			yh;
    int			mid;
    fixed_t		texturecolumn;
    int			top;
    int			bottom;
    fixed_t		iscale = 0;
    lighttable_t*	colormap = dc_colormap;

    // what the loop steps through, in locals until the end
    int			x = rw_x;
    const int		startx = rw_x;
    fixed_t		scale = rw_scale, tfrac = topfrac, bfrac = bottomfrac;
    fixed_t		phigh = pixhigh, plow = pixlow;

    // and what it only reads, which nothing it calls changes
    const int		stopx = rw_stopx;
    const boolean	mark_ceiling = markceiling, mark_floor = markfloor;
    const boolean	textured = segtextured, masked = maskedtexture;
    const int		midtex = midtexture, toptex = toptexture, bottomtex = bottomtexture;
    const fixed_t	scalestep = rw_scalestep, tstep = topstep, bstep = bottomstep;
    const fixed_t	phighstep = pixhighstep, plowstep = pixlowstep;
    const angle_t	centerangle = rw_centerangle;
    const fixed_t	offset = rw_offset, distance = rw_distance;
    lighttable_t**	const lights = walllights;
    visplane_t*		const ceiling = ceilingplane;
    visplane_t*		const floorpl = floorplane;
    const int		direct = !doom_simd ? 0 : colfunc == R_DrawColumn ? 1
			       : colfunc == R_DrawColumnLow ? 2 : 0;
    SegLump		tiers[3] = { { 0, NULL }, { 0, NULL }, { 0, NULL } };

    for ( ; x < stopx ; x++)
    {
	// mark floor / ceiling areas
	yl = (tfrac+HEIGHTUNIT-1)>>HEIGHTBITS;

	// no space above wall?
	if (yl < ceilingclip[x]+1)
	    yl = ceilingclip[x]+1;

	if (mark_ceiling)
	{
	    top = ceilingclip[x]+1;
	    bottom = yl-1;

	    if (bottom >= floorclip[x])
		bottom = floorclip[x]-1;

	    if (top <= bottom)
	    {
		ceiling->top[x] = top;
		ceiling->bottom[x] = bottom;
	    }
	}

	yh = bfrac>>HEIGHTBITS;

	if (yh >= floorclip[x])
	    yh = floorclip[x]-1;

	if (mark_floor)
	{
	    top = yh+1;
	    bottom = floorclip[x]-1;
	    if (top <= ceilingclip[x])
		top = ceilingclip[x]+1;
	    if (top <= bottom)
	    {
		floorpl->top[x] = top;
		floorpl->bottom[x] = bottom;
	    }
	}

	// texturecolumn and lighting are independent of wall tiers
	if (textured)
	{
	    // calculate texture offset
	    angle = (centerangle + xtoviewangle[x])>>ANGLETOFINESHIFT;
	    texturecolumn = offset-FixedMul(finetangent[angle],distance);
	    texturecolumn >>= FRACBITS;
	    // calculate lighting
	    index = scale>>LIGHTSCALESHIFT;

	    if (index >=  MAXLIGHTSCALE )
		index = MAXLIGHTSCALE-1;

	    colormap = lights[index];
	    iscale = 0xffffffffu / (unsigned)scale;
	}
	else
	{
	    texturecolumn = 0;
	}

	// draw the wall tiers
	if (midtex)
	{
	    // single sided line
	    R_SegColumn(direct, x, yl, yh, rw_midtexturemid, iscale, colormap,
			R_SegGetColumn(midtex,texturecolumn,&tiers[0],tiers));
	    ceilingclip[x] = viewheight;
	    floorclip[x] = -1;
	}
	else
	{
	    // two sided line
	    if (toptex)
	    {
		// top wall
		mid = phigh>>HEIGHTBITS;
		phigh += phighstep;

		if (mid >= floorclip[x])
		    mid = floorclip[x]-1;

		if (mid >= yl)
		{
		    R_SegColumn(direct, x, yl, mid, rw_toptexturemid, iscale, colormap,
				R_SegGetColumn(toptex,texturecolumn,&tiers[1],tiers));
		    ceilingclip[x] = mid;
		}
		else
		    ceilingclip[x] = yl-1;
	    }
	    else
	    {
		// no top wall
		if (mark_ceiling)
		    ceilingclip[x] = yl-1;
	    }

	    if (bottomtex)
	    {
		// bottom wall
		mid = (plow+HEIGHTUNIT-1)>>HEIGHTBITS;
		plow += plowstep;

		// no space above wall?
		if (mid <= ceilingclip[x])
		    mid = ceilingclip[x]+1;

		if (mid <= yh)
		{
		    R_SegColumn(direct, x, mid, yh, rw_bottomtexturemid, iscale, colormap,
				R_SegGetColumn(bottomtex,texturecolumn,&tiers[2],tiers));
		    floorclip[x] = mid;
		}
		else
		    floorclip[x] = yh+1;
	    }
	    else
	    {
		// no bottom wall
		if (mark_floor)
		    floorclip[x] = yh+1;
	    }

	    if (masked)
	    {
		// save texturecol
		//  for backdrawing of masked mid texture
		maskedtexturecol[x] = texturecolumn;
	    }
	}

	scale += scalestep;
	tfrac += tstep;
	bfrac += bstep;
    }

    // the globals as the original loop left them
    rw_x = x;
    rw_scale = scale;
    topfrac = tfrac;
    bottomfrac = bfrac;
    pixhigh = phigh;
    pixlow = plow;
    if (textured && x > startx)
    {
	dc_x = x - 1;
	dc_colormap = colormap;
	dc_iscale = iscale;
    }
}
