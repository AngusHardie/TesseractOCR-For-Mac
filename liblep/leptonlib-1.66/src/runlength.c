/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -  This software is distributed in the hope that it will be
 -  useful, but with NO WARRANTY OF ANY KIND.
 -  No author or distributor accepts responsibility to anyone for the
 -  consequences of using this software, or for whether it serves any
 -  particular purpose or works at all, unless he or she says so in
 -  writing.  Everyone is granted permission to copy, modify and
 -  redistribute this source code, for commercial or non-commercial
 -  purposes, with the following restrictions: (1) the origin of this
 -  source code must not be misrepresented; (2) modified versions must
 -  be plainly marked as such; and (3) this notice may not be removed
 -  or altered from any source or modified source distribution.
 *====================================================================*/

/*
 *  runlength.c
 *
 *     Label pixels by membership in runs
 *           PIX        *pixRunlengthTransform()
 *
 *     Find runs along horizontal and vertical lines
 *           l_int32     pixFindHorizontalRuns()
 *           l_int32     pixFindVerticalRuns()
 *
 *     Compute runlength-to-membership transform on a line
 *           l_int32     runlengthMembershipOnLine()
 *
 *     Make byte position LUT
 *           l_int32     makeMSBitLocTab()
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "allheaders.h"


/*-----------------------------------------------------------------------*
 *                   Label pixels by membership in runs                  *
 *-----------------------------------------------------------------------*/
/*!
 *  pixRunlengthTransform()
 *
 *      Input:   pixs (1 bpp)
 *               color (0 for white runs, 1 for black runs)
 *               direction (L_HORIZONTAL_RUNS, L_VERTICAL_RUNS)
 *               depth (8 or 16 bpp)
 *      Return:  pixd (8 or 16 bpp), or null on error
 *
 *  Notes:
 *      (1) The dest Pix is 8 or 16 bpp, with the pixel values
 *          equal to the runlength in which it is a member.
 *          The length is clipped to the max pixel value if necessary.
 *      (2) The color determines if we're labelling white or black runs.
 *      (3) A pixel that is not a member of the chosen color gets
 *          value 0; it belongs to a run of length 0 of the
 *          chosen color.
 *      (4) To convert for maximum dynamic range, either linear or
 *          log, use pixMaxDynamicRange().
 */
PIX *
pixRunlengthTransform(PIX     *pixs,
                      l_int32  color,
                      l_int32  direction,
                      l_int32  depth)
{
l_int32    i, j, w, h, wpld, bufsize, maxsize, n;
l_int32   *start, *end, *buffer;
l_uint32  *datad, *lined;
PIX       *pixt, *pixd;

    PROCNAME("pixRunlengthTransform");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    if (pixGetDepth(pixs) != 1)
        return (PIX *)ERROR_PTR("pixs not 1 bpp", procName, NULL);
    if (depth != 8 && depth != 16)
        return (PIX *)ERROR_PTR("depth must be 8 or 16 bpp", procName, NULL);

    pixGetDimensions(pixs, &w, &h, NULL);
    if (direction == L_HORIZONTAL_RUNS)
        maxsize = 1 + w / 2;
    else if (direction == L_VERTICAL_RUNS)
        maxsize = 1 + h / 2;
    else
        return (PIX *)ERROR_PTR("invalid direction", procName, NULL);
    bufsize = L_MAX(w, h);

    if ((pixd = pixCreate(w, h, depth)) == NULL)
        return (PIX *)ERROR_PTR("pixd not made", procName, NULL);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);

    if ((start = (l_int32 *)CALLOC(maxsize, sizeof(l_int32))) == NULL)
        return (PIX *)ERROR_PTR("start not made", procName, NULL);
    if ((end = (l_int32 *)CALLOC(maxsize, sizeof(l_int32))) == NULL)
        return (PIX *)ERROR_PTR("end not made", procName, NULL);
    if ((buffer = (l_int32 *)CALLOC(bufsize, sizeof(l_int32))) == NULL)
        return (PIX *)ERROR_PTR("buffer not made", procName, NULL);

        /* Use fg runs for evaluation */
    if (color == 0)
        pixt = pixInvert(NULL, pixs);
    else
        pixt = pixClone(pixs);

    if (direction == L_HORIZONTAL_RUNS) {
        for (i = 0; i < h; i++) {
            pixFindHorizontalRuns(pixt, i, start, end, &n);
            runlengthMembershipOnLine(buffer, w, depth, start, end, n);
            lined = datad + i * wpld;
            if (depth == 8) {
                for (j = 0; j < w; j++)
                    SET_DATA_BYTE(lined, j, buffer[j]);
            } else {  /* depth == 16 */
                for (j = 0; j < w; j++)
                    SET_DATA_TWO_BYTES(lined, j, buffer[j]);
            }
        }
    } else {  /* L_VERTICAL_RUNS */
        for (j = 0; j < w; j++) {
            pixFindVerticalRuns(pixt, j, start, end, &n);
            runlengthMembershipOnLine(buffer, h, depth, start, end, n);
            if (depth == 8) {
                for (i = 0; i < h; i++) {
                    lined = datad + i * wpld;
                    SET_DATA_BYTE(lined, j, buffer[i]);
                }
            } else {  /* depth == 16 */
                for (i = 0; i < h; i++) {
                    lined = datad + i * wpld;
                    SET_DATA_TWO_BYTES(lined, j, buffer[i]);
                }
            }
        }
    }

    pixDestroy(&pixt);
    FREE(start);
    FREE(end);
    FREE(buffer);
    return pixd;
}


/*-----------------------------------------------------------------------*
 *               Find runs along horizontal and vertical lines           *
 *-----------------------------------------------------------------------*/
/*!
 *  pixFindHorizontalRuns()
 *
 *      Input:  pix (1 bpp)
 *              y (line to traverse)
 *              xstart (returns array of start positions for fg runs)
 *              xend (returns array of end positions for fg runs)
 *              &n  (<return> the number of runs found)
 *      Return: 0 if OK; 1 on error
 *
 *  Notes:
 *      (1) This finds foreground horizontal runs on a single scanline.
 *      (2) To find background runs, use pixInvert() before applying
 *          this function.
 *      (3) The xstart and xend arrays are input.  They should be
 *          of size w/2 + 1 to insure that they can hold
 *          the maximum number of runs in the raster line.
 */
l_int32
pixFindHorizontalRuns(PIX      *pix,
                      l_int32   y,
                      l_int32  *xstart,
                      l_int32  *xend,
                      l_int32  *pn)
{
l_int32    inrun;  /* boolean */
l_int32    index, w, h, d, j, wpl, val;
l_uint32  *line;

    PROCNAME("pixFindHorizontalRuns");

    if (!pn)
        return ERROR_INT("&n not defined", procName, 1);
    *pn = 0;
    if (!pix)
        return ERROR_INT("pix not defined", procName, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 1)
        return ERROR_INT("pix not 1 bpp", procName, 1);
    if (y < 0 || y >= h)
        return ERROR_INT("y not in [0 ... h - 1]", procName, 1);
    if (!xstart)
        return ERROR_INT("xstart not defined", procName, 1);
    if (!xend)
        return ERROR_INT("xend not defined", procName, 1);

    wpl = pixGetWpl(pix);
    line = pixGetData(pix) + y * wpl;
    
    inrun = FALSE;
    index = 0;
    for (j = 0; j < w; j++) {
        val = GET_DATA_BIT(line, j);
        if (!inrun) {
            if (val) {
                xstart[index] = j;
                inrun = TRUE;
            }
        }
        else {
            if (!val) {
                xend[index++] = j - 1;
                inrun = FALSE;
            }
        }
    }

        /* Finish last run if necessary */
    if (inrun)
        xend[index++] = w - 1;

    *pn = index;
    return 0;
}


/*!
 *  pixFindVerticalRuns()
 *
 *      Input:  pix (1 bpp)
 *              x (line to traverse)
 *              ystart (returns array of start positions for fg runs)
 *              yend (returns array of end positions for fg runs)
 *              &n   (<return> the number of runs found)
 *      Return: 0 if OK; 1 on error
 *
 *  Notes:
 *      (1) This finds foreground vertical runs on a single scanline.
 *      (2) To find background runs, use pixInvert() before applying
 *          this function.
 *      (3) The ystart and yend arrays are input.  They should be
 *          of size h/2 + 1 to insure that they can hold
 *          the maximum number of runs in the raster line.
 */
l_int32
pixFindVerticalRuns(PIX      *pix,
                    l_int32   x,
                    l_int32  *ystart,
                    l_int32  *yend,
                    l_int32  *pn)
{
l_int32    inrun;  /* boolean */
l_int32    index, w, h, d, i, wpl, val;
l_uint32  *data, *line;

    PROCNAME("pixFindVerticalRuns");

    if (!pn)
        return ERROR_INT("&n not defined", procName, 1);
    *pn = 0;
    if (!pix)
        return ERROR_INT("pix not defined", procName, 1);
    pixGetDimensions(pix, &w, &h, &d);
    if (d != 1)
        return ERROR_INT("pix not 1 bpp", procName, 1);
    if (x < 0 || x >= w)
        return ERROR_INT("x not in [0 ... w - 1]", procName, 1);
    if (!ystart)
        return ERROR_INT("ystart not defined", procName, 1);
    if (!yend)
        return ERROR_INT("yend not defined", procName, 1);

    wpl = pixGetWpl(pix);
    data = pixGetData(pix);
    
    inrun = FALSE;
    index = 0;
    for (i = 0; i < h; i++) {
        line = data + i * wpl;
        val = GET_DATA_BIT(line, x);
        if (!inrun) {
            if (val) {
                ystart[index] = i;
                inrun = TRUE;
            }
        }
        else {
            if (!val) {
                yend[index++] = i - 1;
                inrun = FALSE;
            }
        }
    }

        /* Finish last run if necessary */
    if (inrun)
        yend[index++] = h - 1;

    *pn = index;
    return 0;
}


/*-----------------------------------------------------------------------*
 *            Compute runlength-to-membership transform on a line        *
 *-----------------------------------------------------------------------*/
/*!
 *  runlengthMembershipOnLine()
 *
 *      Input:   buffer (into which full line of data is placed)
 *               size (full size of line; w or h)
 *               depth (8 or 16 bpp)
 *               start (array of start positions for fg runs)
 *               end (array of end positions for fg runs)
 *               n   (the number of runs)
 *      Return:  0 if OK; 1 on error
 *
 *  Notes:
 *      (1) Converts a set of runlengths into a buffer of
 *          runlength membership values.
 *      (2) Initialization of the array gives pixels that are
 *          not within a run the value 0.
 */
l_int32
runlengthMembershipOnLine(l_int32  *buffer,
                          l_int32   size,
                          l_int32   depth,
                          l_int32  *start,
                          l_int32  *end,
                          l_int32   n)
{
l_int32  i, j, first, last, diff, max;

    PROCNAME("runlengthMembershipOnLine");

    if (!buffer)
        return ERROR_INT("buffer not defined", procName, 1);
    if (!start)
        return ERROR_INT("start not defined", procName, 1);
    if (!end)
        return ERROR_INT("end not defined", procName, 1);

    if (depth == 8)
        max = 0xff;
    else  /* depth == 16 */
        max = 0xffff;

    memset(buffer, 0, 4 * size);
    for (i = 0; i < n; i++) {
        first = start[i];
        last = end[i];
        diff = last - first + 1;
        diff = L_MIN(diff, max);
        for (j = first; j <= last; j++)
            buffer[j] = diff;
    }

    return 0;
}



/*-----------------------------------------------------------------------*
 *                       Make byte position LUT                          *
 *-----------------------------------------------------------------------*/
/*!
 *  makeMSBitLocTab()
 *
 *      Input:  bitval (either 0 or 1)
 *      Return: table (giving, for an input byte, the MS bit location,
 *                     starting at 0 with the MSBit in the byte),
 *                     or null on error.
 *
 *  Notes:
 *      (1) If bitval == 1, it finds the leftmost ON pixel in a byte;
 *          otherwise if bitval == 0, it finds the leftmost OFF pixel.
 *      (2) If there are no pixels of the indicated color in the byte,
 *          this returns 8. 
 */
l_int32 *
makeMSBitLocTab(l_int32  bitval)
{
l_int32   i, j;
l_int32  *tab;
l_uint8   byte, mask;

    PROCNAME("makeMSBitLocTab");

    if ((tab = (l_int32 *)CALLOC(256, sizeof(l_int32))) == NULL)
        return (l_int32 *)ERROR_PTR("tab not made", procName, NULL);

    for (i = 0; i < 256; i++) {
        byte = (l_uint8)i;
        if (bitval == 0)
            byte = ~byte;
        tab[i] = 8;
        mask = 0x80;
        for (j = 0; j < 8; j++) {
            if (byte & mask) {
                tab[i] = j;
                break;
            }
            mask >>= 1;
        }
    }

    return tab;
}


