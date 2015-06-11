#include "quant.h"
#include <QColor>
#include <QByteArray>
#include <QBuffer>
#include <QFile>
#include "ppm.h"
#include <iostream>

//#include "ppmcmap.h"
//#define MAXCOLORS 32767
#define MAXCOLORS 65536
//#define MAXCOLORS 262144

QuantizeMethod toQuantizeMethod( const QString& s )
{
    if ( s == "DIFFUSE" )
    {
        return QuantizeMethod_DIFFUSE;
    }
    if ( s == "THRESHOLD" )
    {
        return QuantizeMethod_THRESHOLD;
    }
    if ( s == "ORDERED" )
    {
        return QuantizeMethod_ORDERED;
    }
    if ( s == "MEDIANCUT" )
    {
        return QuantizeMethod_MEDIANCUT;
    }
    if ( s == "MEDIANCUT_FLOYD" )
    {
        return QuantizeMethod_MEDIANCUT_FLOYD;
    }
    std::cerr << "warning: unknown quantize method:" << s.toLocal8Bit().constData() << ", using MEDIANCUT" << std::endl;
    return QuantizeMethod_MEDIANCUT;    
}


typedef struct box* box_vector;
struct box
{
    int ind;
    int colors;
    int sum;
};

static int
redcompare( colorhist_vector ch1, colorhist_vector ch2 )
{
    return (int) PPM_GETR( ch1->color ) - (int) PPM_GETR( ch2->color );
}

static int
greencompare( colorhist_vector ch1, colorhist_vector ch2 )
{
    return (int) PPM_GETG( ch1->color ) - (int) PPM_GETG( ch2->color );
}

static int
bluecompare( colorhist_vector ch1, colorhist_vector ch2 )
{
    return (int) PPM_GETB( ch1->color ) - (int) PPM_GETB( ch2->color );
}

static int
sumcompare( box_vector b1, box_vector b2 )
{
    return b2->sum - b1->sum;
}


/*
** Here is the fun part, the median-cut colormap generator.  This is based
** on Paul Heckbert's paper "Color Image Quantization for Frame Buffer
** Display", SIGGRAPH '82 Proceedings, page 297.
*/

colorhist_vector mediancut( colorhist_vector chv, int colors, int sum, pixval maxval, int newcolors )
{

#define LARGE_LUM
#define REP_AVERAGE_PIXELS

    colorhist_vector colormap;
    box_vector bv;
    register int bi, i;
    int boxes;

    bv = (box_vector) malloc( sizeof(struct box) * newcolors );
    colormap =
	(colorhist_vector) malloc( sizeof(struct colorhist_item) * newcolors );
    if ( bv == (box_vector) 0 || colormap == (colorhist_vector) 0 )
        std::cerr << "out of memory" << std::endl;
    for ( i = 0; i < newcolors; ++i )
	PPM_ASSIGN( colormap[i].color, 0, 0, 0 );

    /*
    ** Set up the initial box.
    */
    bv[0].ind = 0;
    bv[0].colors = colors;
    bv[0].sum = sum;
    boxes = 1;

    /*
    ** Main loop: split boxes until we have enough.
    */
    while ( boxes < newcolors )
    {
	register int indx, clrs;
	int sm;
	register int minr, maxr, ming, maxg, minb, maxb, v;
	int halfsum, lowersum;

	/*
	** Find the first splittable box.
	*/
	for ( bi = 0; bi < boxes; ++bi )
	    if ( bv[bi].colors >= 2 )
		break;
	if ( bi == boxes )
	    break;	/* ran out of colors! */
	indx = bv[bi].ind;
	clrs = bv[bi].colors;
	sm = bv[bi].sum;

	/*
	** Go through the box finding the minimum and maximum of each
	** component - the boundaries of the box.
	*/
	minr = maxr = PPM_GETR( chv[indx].color );
	ming = maxg = PPM_GETG( chv[indx].color );
	minb = maxb = PPM_GETB( chv[indx].color );
	for ( i = 1; i < clrs; ++i )
        {
	    v = PPM_GETR( chv[indx + i].color );
	    if ( v < minr ) minr = v;
	    if ( v > maxr ) maxr = v;
	    v = PPM_GETG( chv[indx + i].color );
	    if ( v < ming ) ming = v;
	    if ( v > maxg ) maxg = v;
	    v = PPM_GETB( chv[indx + i].color );
	    if ( v < minb ) minb = v;
	    if ( v > maxb ) maxb = v;
        }

	/*
	** Find the largest dimension, and sort by that component.  I have
	** included two methods for determining the "largest" dimension;
	** first by simply comparing the range in RGB space, and second
	** by transforming into luminosities before the comparison.  You
	** can switch which method is used by switching the commenting on
	** the LARGE_ defines at the beginning of this source file.
	*/
#ifdef LARGE_NORM
	if ( maxr - minr >= maxg - ming && maxr - minr >= maxb - minb )
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                (int (*)(const void*, const void*))
		redcompare );
	else if ( maxg - ming >= maxb - minb )
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                (int (*)(const void*, const void*))
		greencompare );
	else
	    qsort(
		(char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                (int (*)(const void*, const void*))
		bluecompare );
#endif /*LARGE_NORM*/
#ifdef LARGE_LUM
	{
            pixel p;
            float rl, gl, bl;

            PPM_ASSIGN(p, maxr - minr, 0, 0);
            rl = PPM_LUMIN(p);
            PPM_ASSIGN(p, 0, maxg - ming, 0);
            gl = PPM_LUMIN(p);
            PPM_ASSIGN(p, 0, 0, maxb - minb);
            bl = PPM_LUMIN(p);

            if ( rl >= gl && rl >= bl )
                qsort(
                    (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                    (int (*)(const void*, const void*))
                    redcompare );
            else if ( gl >= bl )
                qsort(
                    (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                    (int (*)(const void*, const void*))
                    greencompare );
            else
                qsort(
                    (char*) &(chv[indx]), clrs, sizeof(struct colorhist_item),
                    (int (*)(const void*, const void*))
                    bluecompare );
	}
#endif /*LARGE_LUM*/
	
	/*
	** Now find the median based on the counts, so that about half the
	** pixels (not colors, pixels) are in each subdivision.
	*/
	lowersum = chv[indx].value;
	halfsum = sm / 2;
	for ( i = 1; i < clrs - 1; ++i )
        {
	    if ( lowersum >= halfsum )
		break;
	    lowersum += chv[indx + i].value;
        }

	/*
	** Split the box, and sort to bring the biggest boxes to the top.
	*/
	bv[bi].colors = i;
	bv[bi].sum = lowersum;
	bv[boxes].ind = indx + i;
	bv[boxes].colors = clrs - i;
	bv[boxes].sum = sm - lowersum;
	++boxes;
	qsort( (char*) bv, boxes, sizeof(struct box), (int (*)(const void*, const void*))sumcompare );
    }

    /*
    ** Ok, we've got enough boxes.  Now choose a representative color for
    ** each box.  There are a number of possible ways to make this choice.
    ** One would be to choose the center of the box; this ignores any structure
    ** within the boxes.  Another method would be to average all the colors in
    ** the box - this is the method specified in Heckbert's paper.  A third
    ** method is to average all the pixels in the box.  You can switch which
    ** method is used by switching the commenting on the REP_ defines at
    ** the beginning of this source file.
    */
    for ( bi = 0; bi < boxes; ++bi )
    {
#ifdef REP_CENTER_BOX
	register int indx = bv[bi].ind;
	register int clrs = bv[bi].colors;
	register int minr, maxr, ming, maxg, minb, maxb, v;

	minr = maxr = PPM_GETR( chv[indx].color );
	ming = maxg = PPM_GETG( chv[indx].color );
	minb = maxb = PPM_GETB( chv[indx].color );
	for ( i = 1; i < clrs; ++i )
        {
	    v = PPM_GETR( chv[indx + i].color );
	    minr = min( minr, v );
	    maxr = max( maxr, v );
	    v = PPM_GETG( chv[indx + i].color );
	    ming = min( ming, v );
	    maxg = max( maxg, v );
	    v = PPM_GETB( chv[indx + i].color );
	    minb = min( minb, v );
	    maxb = max( maxb, v );
        }
	PPM_ASSIGN(
	    colormap[bi].color, ( minr + maxr ) / 2, ( ming + maxg ) / 2,
	    ( minb + maxb ) / 2 );
#endif /*REP_CENTER_BOX*/
#ifdef REP_AVERAGE_COLORS
	register int indx = bv[bi].ind;
	register int clrs = bv[bi].colors;
	register long r = 0, g = 0, b = 0;

	for ( i = 0; i < clrs; ++i )
        {
	    r += PPM_GETR( chv[indx + i].color );
	    g += PPM_GETG( chv[indx + i].color );
	    b += PPM_GETB( chv[indx + i].color );
        }
	r = r / clrs;
	g = g / clrs;
	b = b / clrs;
	PPM_ASSIGN( colormap[bi].color, r, g, b );
#endif /*REP_AVERAGE_COLORS*/
#ifdef REP_AVERAGE_PIXELS
	register int indx = bv[bi].ind;
	register int clrs = bv[bi].colors;
	register long r = 0, g = 0, b = 0, sum = 0;

	for ( i = 0; i < clrs; ++i )
        {
	    r += PPM_GETR( chv[indx + i].color ) * chv[indx + i].value;
	    g += PPM_GETG( chv[indx + i].color ) * chv[indx + i].value;
	    b += PPM_GETB( chv[indx + i].color ) * chv[indx + i].value;
	    sum += chv[indx + i].value;
        }
	r = r / sum;
	if ( r > maxval ) r = maxval;	/* avoid math errors */
	g = g / sum;
	if ( g > maxval ) g = maxval;
	b = b / sum;
	if ( b > maxval ) b = maxval;
	PPM_ASSIGN( colormap[bi].color, r, g, b );
#endif /*REP_AVERAGE_PIXELS*/
    }

    /*
    ** All done.
    */
    free(bv);
    return colormap;
}

QImage quantize_mediancut( const QImage& src, bool use_floyd )
{
    //std::cout << "quantize_mediancut" << std::endl;
    QByteArray bar;
    QBuffer buffer(&bar);
    buffer.open(QIODevice::WriteOnly);
    src.save(&buffer, "PPM");

    FILE* ppmfile = fmemopen( bar.data(), bar.size(), "r" );

    int floyd = use_floyd;
    long* thisrerr=0;
    long* nextrerr=0;
    long* thisgerr=0;
    long* nextgerr=0;
    long* thisberr=0;
    long* nextberr=0;
    long* temperr=0;
    register long sr=0, sg=0, sb=0, err=0;
    register int col = 0, limitcol = 0;

#define FS_SCALE 1024
    int fs_direction = 0;
    register pixel* pP = 0;
    int ind = 0;
    int argn=0, rows=0, cols=0, maprows=0, mapcols=0, row=0;
    pixval maxval=0, newmaxval=0, mapmaxval=0;
    int newcolors=0, colors=0;
    colorhist_vector chv, colormap;
    colorhash_table cht;

    pixel** pixels = ppm_readppm( ppmfile, &cols, &rows, &maxval );
    pm_close( ppmfile );

    //std::cout << "ppm_readppm cols:" << cols << " rows:" << rows << " maxval:" << maxval << std::endl;
    newcolors = 256;

    /*
    ** Step 2: attempt to make a histogram of the colors, unclustered.
    ** If at first we don't succeed, lower maxval to increase color
    ** coherence and try again.  This will eventually terminate, with
    ** maxval at worst 15, since 32^3 is approximately MAXCOLORS.
    */
    for ( ; ; )
    {
        chv = ppm_computecolorhist(
            pixels, cols, rows, MAXCOLORS, &colors );
        if ( chv != (colorhist_vector) 0 )
            break;
        newmaxval = maxval - 1;
        //std::cout << "scaling colors (computed " << colors << ") from maxval=" << maxval << " to maxval=" << newmaxval << " to improve clustering" << std::endl;
        for ( row = 0; row < rows; ++row ) {
            pixel* pP = pixels[row];
            for ( col = 0 ; col < cols; ++col, ++pP )
                PPM_DEPTH( *pP, *pP, maxval, newmaxval );
        }
        maxval = newmaxval;
    }
    /*
    ** Step 3: apply median-cut to histogram, making the new colormap.
    */
    //std::cout <<  "choosing " << newcolors << " colors..." << " maxval:" << maxval << " computed colors:" << colors << std::endl;    
    maxval = 255;
    colormap = mediancut( chv, colors, rows * cols, maxval, newcolors );
    ppm_freecolorhist( chv );

    /*
    register int i, r1, g1, b1;
    for ( i = 0; i < newcolors; ++i )
    {
        r1 = PPM_GETR( colormap[i].color );
        g1 = PPM_GETG( colormap[i].color );
        b1 = PPM_GETB( colormap[i].color );
        //std::cout << "colormap ind:" << i << " has color:" << r1 << "," << g1 << "," << b1 << std::endl;
    }
    */
        
    /*
    ** Step 4: map the colors in the image to their closest match in the
    ** new colormap, and write 'em out.
    */

    FILE* ofd = fmemopen( bar.data(), bar.size() + 1, "w" );

    cht = ppm_alloccolorhash( );
    int usehash = 1;
    ppm_writeppminit( ofd, cols, rows, maxval, 0 );
    if ( floyd )
    {
	/* Initialize Floyd-Steinberg error vectors. */
	thisrerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	nextrerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	thisgerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	nextgerr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	thisberr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	nextberr = (long*) pm_allocrow( cols + 2, sizeof(long) );
	srandom( (int) ( time( 0 ) ^ getpid( ) ) );
	for ( col = 0; col < cols + 2; ++col )
        {
	    thisrerr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
	    thisgerr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
	    thisberr[col] = random( ) % ( FS_SCALE * 2 ) - FS_SCALE;
	    /* (random errors in [-1 .. 1]) */
        }
	fs_direction = 1;
    }
    for ( row = 0; row < rows; ++row )
    {
	if ( floyd )
	    for ( col = 0; col < cols + 2; ++col )
		nextrerr[col] = nextgerr[col] = nextberr[col] = 0;
	if ( ( ! floyd ) || fs_direction )
        {
	    col = 0;
	    limitcol = cols;
	    pP = pixels[row];
        }
	else
        {
	    col = cols - 1;
	    limitcol = -1;
	    pP = &(pixels[row][col]);
        }
	do
        {
	    if ( floyd )
            {
		/* Use Floyd-Steinberg errors to adjust actual color. */
		sr = PPM_GETR(*pP) + thisrerr[col + 1] / FS_SCALE;
		sg = PPM_GETG(*pP) + thisgerr[col + 1] / FS_SCALE;
		sb = PPM_GETB(*pP) + thisberr[col + 1] / FS_SCALE;
		if ( sr < 0 ) sr = 0;
		else if ( sr > maxval ) sr = maxval;
		if ( sg < 0 ) sg = 0;
		else if ( sg > maxval ) sg = maxval;
		if ( sb < 0 ) sb = 0;
		else if ( sb > maxval ) sb = maxval;
		PPM_ASSIGN( *pP, sr, sg, sb );
            }

	    /* Check hash table to see if we have already matched this color. */
	    ind = ppm_lookupcolor( cht, pP );
	    if ( ind == -1 )
            { /* No; search colormap for closest match. */
		register int i, r1, g1, b1, r2, g2, b2;
		register long dist, newdist;
		r1 = PPM_GETR( *pP );
		g1 = PPM_GETG( *pP );
		b1 = PPM_GETB( *pP );
		dist = 2000000000;
		for ( i = 0; i < newcolors; ++i )
                {
		    r2 = PPM_GETR( colormap[i].color );
		    g2 = PPM_GETG( colormap[i].color );
		    b2 = PPM_GETB( colormap[i].color );
		    newdist = ( r1 - r2 ) * ( r1 - r2 ) +
                        ( g1 - g2 ) * ( g1 - g2 ) +
                        ( b1 - b2 ) * ( b1 - b2 );
		    if ( newdist < dist )
                    {
			ind = i;
			dist = newdist;
                    }
                }
		if ( usehash )
                {
		    if ( ppm_addtocolorhash( cht, pP, ind ) < 0 )
                    {
                        std::cerr << "out of memory adding to hash table, proceeding without it" << std::endl;
			usehash = 0;
                    }
                }
            }

	    if ( floyd )
            {
		/* Propagate Floyd-Steinberg error terms. */
		if ( fs_direction )
                {
		    err = ( sr - (long) PPM_GETR( colormap[ind].color ) ) * FS_SCALE;
		    thisrerr[col + 2] += ( err * 7 ) / 16;
		    nextrerr[col    ] += ( err * 3 ) / 16;
		    nextrerr[col + 1] += ( err * 5 ) / 16;
		    nextrerr[col + 2] += ( err     ) / 16;
		    err = ( sg - (long) PPM_GETG( colormap[ind].color ) ) * FS_SCALE;
		    thisgerr[col + 2] += ( err * 7 ) / 16;
		    nextgerr[col    ] += ( err * 3 ) / 16;
		    nextgerr[col + 1] += ( err * 5 ) / 16;
		    nextgerr[col + 2] += ( err     ) / 16;
		    err = ( sb - (long) PPM_GETB( colormap[ind].color ) ) * FS_SCALE;
		    thisberr[col + 2] += ( err * 7 ) / 16;
		    nextberr[col    ] += ( err * 3 ) / 16;
		    nextberr[col + 1] += ( err * 5 ) / 16;
		    nextberr[col + 2] += ( err     ) / 16;
                }
		else
                {
		    err = ( sr - (long) PPM_GETR( colormap[ind].color ) ) * FS_SCALE;
		    thisrerr[col    ] += ( err * 7 ) / 16;
		    nextrerr[col + 2] += ( err * 3 ) / 16;
		    nextrerr[col + 1] += ( err * 5 ) / 16;
		    nextrerr[col    ] += ( err     ) / 16;
		    err = ( sg - (long) PPM_GETG( colormap[ind].color ) ) * FS_SCALE;
		    thisgerr[col    ] += ( err * 7 ) / 16;
		    nextgerr[col + 2] += ( err * 3 ) / 16;
		    nextgerr[col + 1] += ( err * 5 ) / 16;
		    nextgerr[col    ] += ( err     ) / 16;
		    err = ( sb - (long) PPM_GETB( colormap[ind].color ) ) * FS_SCALE;
		    thisberr[col    ] += ( err * 7 ) / 16;
		    nextberr[col + 2] += ( err * 3 ) / 16;
		    nextberr[col + 1] += ( err * 5 ) / 16;
		    nextberr[col    ] += ( err     ) / 16;
                }
            }

            //std::cout << "using ind:" << ind << " with color:" << colormap[ind].color.r << "," << colormap[ind].color.g << "," << colormap[ind].color.b << std::endl;
	    *pP = colormap[ind].color;

	    if ( ( ! floyd ) || fs_direction )
            {
		++col;
		++pP;
            }
	    else
            {
		--col;
		--pP;
            }
        }
	while ( col != limitcol );

	if ( floyd )
        {
	    temperr = thisrerr;
	    thisrerr = nextrerr;
	    nextrerr = temperr;
	    temperr = thisgerr;
	    thisgerr = nextgerr;
	    nextgerr = temperr;
	    temperr = thisberr;
	    thisberr = nextberr;
	    nextberr = temperr;
	    fs_direction = ! fs_direction;
        }

	ppm_writeppmrow( ofd, pixels[row], cols, maxval, 0 );
    }

    pm_close( ofd );
    ppm_freearray( pixels, rows );
    ppm_freecolorhash( cht );
    free(colormap);
    

    return QImage::fromData( bar, "ppm" );
}
