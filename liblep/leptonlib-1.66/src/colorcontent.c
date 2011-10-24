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
 *  colorcontent.c
 *                     
 *      Builds an image of the color content, on a per-pixel basis,
 *      as a measure of the amount of divergence of each color
 *      component (R,G,B) from gray.
 *         l_int32    pixColorContent()
 *
 *      Finds the 'amount' of color in an image, on a per-pixel basis,
 *      as a measure of the difference of the pixel color from gray.
 *         PIX       *pixColorMagnitude()
 *
 *      Generates a mask over pixels that have sufficient color and
 *      are not too close to gray pixels.
 *         PIX       *pixMaskOverColorPixels()
 *
 *      Finds the fraction of pixels with "color" that are not close to black
 *         l_int32    pixColorFraction()
 *
 *      Finds the number of perceptually significant gray intensities
 *      in a grayscale image.
 *         l_int32    pixNumSignificantGrayColors()
 *
 *      Identifies images where color quantization will cause posterization
 *      due to the existence of many colors in low-gradient regions.
 *         l_int32    pixColorsForQuantization()
 *
 *      Finds the number of unique colors in an image
 *         l_int32    pixNumColors()
 *
 *  Color is tricky.  If we consider gray (r = g = b) to have no color
 *  content, how should we define the color content in each component
 *  of an arbitrary pixel, as well as the overall color magnitude?
 *
 *  I can think of three ways to define the color content in each component:
 *
 *  (1) Linear.  For each component, take the difference from the average
 *      of all three.
 *  (2) Linear.  For each component, take the difference from the average
 *      of the other two.
 *  (3) Nonlinear.  For each component, take the minimum of the differences
 *      from the other two.
 *
 *  How might one choose from among these?  Consider two different situations:
 *  (a) r = g = 0, b = 255            {255}   /255/
 *  (b) r = 0, g = 127, b = 255       {191}   /128/
 *  How much g is in each of these?  The three methods above give:
 *  (a)  1: 85   2: 127   3: 0        [85]
 *  (b)  1: 0    2: 0     3: 127      [0]
 *  How much b is in each of these?
 *  (a)  1: 170  2: 255   3: 255      [255]
 *  (b)  1: 127  2: 191   3: 127      [191]
 *  The number I'd "like" to give is in [].  (Please don't ask why, it's
 *  just a feeling.
 *
 *  So my preferences seem to be somewhere between (1) and (2).
 *  (3) is just too "decisive!"  Let's pick (2).
 *
 *  We also allow compensation for white imbalance.  For each
 *  component, we do a linear TRC (gamma = 1.0), where the black
 *  point remains at 0 and the white point is given by the input
 *  parameter.  This is equivalent to doing a global remapping,
 *  as with pixGlobalNormRGB(), followed by color content (or magnitude)
 *  computation, but without the overhead of first creating the
 *  white point normalized image.
 *
 *  Another useful property is the overall color magnitude in the pixel.
 *  For this there are again several choices, such as:
 *      (a) rms deviation from the mean
 *      (b) the average L1 deviation from the mean
 *      (c) the maximum (over components) of one of the color
 *          content measures given above.
 *
 *  For now, we will choose two of the methods in (c):
 *     L_MAX_DIFF_FROM_AVERAGE_2
 *        Define the color magnitude as the maximum over components
 *        of the difference between the component value and the
 *        average of the other two.  It is easy to show that
 *        this is equivalent to selecting the two component values
 *        that are closest to each other, averaging them, and
 *        using the distance from that average to the third component.
 *        For (a) and (b) above, this value is in {..}.
 *    L_MAX_MIN_DIFF_FROM_2
 *        Define the color magnitude as the maximum over components
 *        of the minimum difference between the component value and the
 *        other two values.  It is easy to show that this is equivalent
 *        to selecting the intermediate value of the three differences
 *        between the three components.  For (a) and (b) above,
 *        this value is in /../.
 */

#include <stdio.h>
#include <stdlib.h>
#include "allheaders.h"


/*!
 *  pixColorContent()
 *
 *      Input:  pixs  (32 bpp rgb or 8 bpp colormapped)
 *              rwhite, gwhite, bwhite (color value associated with white point)
 *              mingray (min gray value for which color is measured)
 *              &pixr (<optional return> 8 bpp red 'content')
 *              &pixg (<optional return> 8 bpp green 'content')
 *              &pixb (<optional return> 8 bpp blue 'content')
 *      Return: 0 if OK, 1 on error
 *
 *  Notes:
 *      (1) This returns the color content in each component, which is
 *          a measure of the deviation from gray, and is defined
 *          as the difference between the component and the average of
 *          the other two components.  See the discussion at the
 *          top of this file.
 *      (2) The three numbers (rwhite, gwhite and bwhite) can be thought
 *          of as the values in the image corresponding to white.
 *          They are used to compensate for an unbalanced color white point.
 *          They must either be all 0 or all non-zero.  To turn this
 *          off, set them all to 0.
 *      (3) If the maximum component after white point correction,
 *          max(r,g,b), is less than mingray, all color components
 *          for that pixel are set to zero.
 *          Use mingray = 0 to turn off this filtering of dark pixels.
 *      (4) Therefore, use 0 for all four input parameters if the color
 *          magnitude is to be calculated without either white balance
 *          correction or dark filtering.
 */
l_int32
pixColorContent(PIX     *pixs,
                l_int32  rwhite,
                l_int32  gwhite,
                l_int32  bwhite,
                l_int32  mingray,
                PIX    **ppixr,
                PIX    **ppixg,
                PIX    **ppixb)
{
l_int32    w, h, d, i, j, wplc, wplr, wplg, wplb;
l_int32    rval, gval, bval, rgdiff, rbdiff, gbdiff, maxval, colorval;
l_int32   *rtab, *gtab, *btab;
l_uint32   pixel;
l_uint32  *datac, *datar, *datag, *datab, *linec, *liner, *lineg, *lineb;
NUMA      *nar, *nag, *nab;
PIX       *pixc;   /* rgb */
PIX       *pixr, *pixg, *pixb;   /* 8 bpp grayscale */
PIXCMAP   *cmap;

    PROCNAME("pixColorContent");

    if (!pixs)
        return ERROR_INT("pixs not defined", procName, 1);
    if (!ppixr && !ppixg && !ppixb)
        return ERROR_INT("nothing to compute", procName, 1);
    if (mingray < 0) mingray = 0;
    pixGetDimensions(pixs, &w, &h, &d);
    if (mingray > 255)
        return ERROR_INT("mingray > 255", procName, 1);
    if (rwhite < 0 || gwhite < 0 || bwhite < 0)
        return ERROR_INT("some white vals are negative", procName, 1);
    if ((rwhite || gwhite || bwhite) && (rwhite * gwhite * bwhite == 0))
        return ERROR_INT("white vals not all zero or all nonzero", procName, 1);

    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return ERROR_INT("pixs neither cmapped nor 32 bpp", procName, 1);
    if (cmap)
        pixc = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc = pixClone(pixs);

    pixr = pixg = pixb = NULL;
    w = pixGetWidth(pixc);
    h = pixGetHeight(pixc);
    if (ppixr) {
        pixr = pixCreate(w, h, 8);
        datar = pixGetData(pixr);
        wplr = pixGetWpl(pixr);
        *ppixr = pixr;
    }
    if (ppixg) {
        pixg = pixCreate(w, h, 8);
        datag = pixGetData(pixg);
        wplg = pixGetWpl(pixg);
        *ppixg = pixg;
    }
    if (ppixb) {
        pixb = pixCreate(w, h, 8);
        datab = pixGetData(pixb);
        wplb = pixGetWpl(pixb);
        *ppixb = pixb;
    }

    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);
    if (rwhite) {  /* all white pt vals are nonzero */
        nar = numaGammaTRC(1.0, 0, rwhite);
        rtab = numaGetIArray(nar);
        nag = numaGammaTRC(1.0, 0, gwhite);
        gtab = numaGetIArray(nag);
        nab = numaGammaTRC(1.0, 0, bwhite);
        btab = numaGetIArray(nab);
    }
    for (i = 0; i < h; i++) {
        linec = datac + i * wplc;
        if (pixr)
            liner = datar + i * wplr;
        if (pixg)
            lineg = datag + i * wplg;
        if (pixb)
            lineb = datab + i * wplb;
        for (j = 0; j < w; j++) {
            pixel = linec[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            if (rwhite) {  /* color correct for white point */
                rval = rtab[rval];
                gval = gtab[gval];
                bval = btab[bval];
            }
            if (mingray > 0) {  /* dark pixels have no color value */
                maxval = L_MAX(rval, gval);
                maxval = L_MAX(maxval, bval);
                if (maxval < mingray)
                    continue;  /* colorval = 0 for each component */
            }
            rgdiff = L_ABS(rval - gval);
            rbdiff = L_ABS(rval - bval);
            gbdiff = L_ABS(gval - bval);
            if (pixr) {
                colorval = (rgdiff + rbdiff) / 2;
                SET_DATA_BYTE(liner, j, colorval);
            }
            if (pixg) {
                colorval = (rgdiff + gbdiff) / 2;
                SET_DATA_BYTE(lineg, j, colorval);
            }
            if (pixb) {
                colorval = (rbdiff + gbdiff) / 2;
                SET_DATA_BYTE(lineb, j, colorval);
            }
        }
    }

    if (rwhite) {
        numaDestroy(&nar);
        numaDestroy(&nag);
        numaDestroy(&nab);
        FREE(rtab);
        FREE(gtab);
        FREE(btab);
    }
    pixDestroy(&pixc);
    return 0;
}


/*!
 *  pixColorMagnitude()
 *
 *      Input:  pixs  (32 bpp rgb or 8 bpp colormapped)
 *              rwhite, gwhite, bwhite (color value associated with white point)
 *              type (chooses the method for calculating the color magnitude:
 *                    L_MAX_DIFF_FROM_AVERAGE_2, L_MAX_MIN_DIFF_FROM_2,
 *                    L_MAX_DIFF)
 *      Return: pixd (8 bpp, amount of color in each source pixel),
 *                    or NULL on error
 *
 *  Notes:
 *      (1) For an RGB image, a gray pixel is one where all three components
 *          are equal.  We define the amount of color in an RGB pixel by
 *          considering the absolute value of the differences between the
 *          three color components.  Consider the two largest
 *          of these differences.  The pixel component in common to these
 *          two differences is the color farthest from the other two.
 *          The color magnitude in an RGB pixel can be taken as:
 *              * the average of these two differences; i.e., the
 *                average distance from the two components that are
 *                nearest to each other to the third component, or
 *              * the minimum value of these two differences; i.e., the
 *                maximum over all components of the minimum distance
 *                from that component to the other two components.
 *          Even more simply, the color magnitude can be taken as
 *              * the maximum difference between component values
 *      (2) As an example, suppose that R and G are the closest in
 *          magnitude.  Then the color is determined as:
 *              * the average distance of B from these two; namely,
 *                (|B - R| + |B - G|) / 2, which can also be found
 *                from |B - (R + G) / 2|, or
 *              * the minimum distance of B from these two; namely,
 *                min(|B - R|, |B - G|).
 *              * the max(|B - R|, |B - G|)
 *      (3) The three numbers (rwhite, gwhite and bwhite) can be thought
 *          of as the values in the image corresponding to white.
 *          They are used to compensate for an unbalanced color white point.
 *          They must either be all 0 or all non-zero.  To turn this
 *          off, set them all to 0.
 *      (4) We allow the following methods for choosing the color
 *          magnitude from the three components:
 *              * L_MAX_DIFF_FROM_AVERAGE_2
 *              * L_MAX_MIN_DIFF_FROM_2
 *              * L_MAX_DIFF
 *          These are described above in (1) and (2), as well as at
 *          the top of this file.
 */
PIX *
pixColorMagnitude(PIX     *pixs,
                  l_int32  rwhite,
                  l_int32  gwhite,
                  l_int32  bwhite,
                  l_int32  type)
{
l_int32    w, h, d, i, j, wplc, wpld;
l_int32    rval, gval, bval, rdist, gdist, bdist, colorval;
l_int32    rgdist, rbdist, gbdist, mindist, maxdist, minval, maxval;
l_int32   *rtab, *gtab, *btab;
l_uint32   pixel;
l_uint32  *datac, *datad, *linec, *lined;
NUMA      *nar, *nag, *nab;
PIX       *pixc, *pixd;
PIXCMAP   *cmap;

    PROCNAME("pixColorMagnitude");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    pixGetDimensions(pixs, &w, &h, &d);
    if (type != L_MAX_DIFF_FROM_AVERAGE_2 && type != L_MAX_MIN_DIFF_FROM_2 &&
        type != L_MAX_DIFF)
        return (PIX *)ERROR_PTR("invalid type", procName, NULL);
    if (rwhite < 0 || gwhite < 0 || bwhite < 0)
        return (PIX *)ERROR_PTR("some white vals are negative", procName, NULL);
    if ((rwhite || gwhite || bwhite) && (rwhite * gwhite * bwhite == 0))
        return (PIX *)ERROR_PTR("white vals not all zero or all nonzero",
                                procName, NULL);

    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("pixs not cmapped or 32 bpp", procName, NULL);
    if (cmap)
        pixc = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc = pixClone(pixs);

    pixd = pixCreate(w, h, 8);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    datac = pixGetData(pixc);
    wplc = pixGetWpl(pixc);
    if (rwhite) {  /* all white pt vals are nonzero */
        nar = numaGammaTRC(1.0, 0, rwhite);
        rtab = numaGetIArray(nar);
        nag = numaGammaTRC(1.0, 0, gwhite);
        gtab = numaGetIArray(nag);
        nab = numaGammaTRC(1.0, 0, bwhite);
        btab = numaGetIArray(nab);
    }
    for (i = 0; i < h; i++) {
        linec = datac + i * wplc;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            pixel = linec[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            if (rwhite) {  /* color correct for white point */
                rval = rtab[rval];
                gval = gtab[gval];
                bval = btab[bval];
            }
            if (type == L_MAX_DIFF_FROM_AVERAGE_2) {
                rdist = ((gval + bval ) / 2 - rval);
                rdist = L_ABS(rdist);
                gdist = ((rval + bval ) / 2 - gval);
                gdist = L_ABS(gdist);
                bdist = ((rval + gval ) / 2 - bval);
                bdist = L_ABS(bdist);
                colorval = L_MAX(rdist, gdist);
                colorval = L_MAX(colorval, bdist);
            }
            else if (type == L_MAX_MIN_DIFF_FROM_2) { /* choose intermed dist */
                rgdist = L_ABS(rval - gval);
                rbdist = L_ABS(rval - bval);
                gbdist = L_ABS(gval - bval);
                maxdist = L_MAX(rgdist, rbdist);
                if (gbdist >= maxdist)
                    colorval = maxdist;
                else {  /* gbdist is smallest or intermediate */
                    mindist = L_MIN(rgdist, rbdist);
                    colorval = L_MAX(mindist, gbdist);
                }
            }
            else {  /* type == L_MAX_DIFF */
                minval = L_MIN(rval, gval);
                minval = L_MIN(minval, bval);
                maxval = L_MAX(rval, gval);
                maxval = L_MAX(maxval, bval);
                colorval = maxval - minval;
            }
            SET_DATA_BYTE(lined, j, colorval);
        }
    }

    if (rwhite) {
        numaDestroy(&nar);
        numaDestroy(&nag);
        numaDestroy(&nab);
        FREE(rtab);
        FREE(gtab);
        FREE(btab);
    }
    pixDestroy(&pixc);
    return pixd;
}


/*!
 *  pixMaskOverColorPixels()
 *
 *      Input:  pixs  (32 bpp rgb or 8 bpp colormapped)
 *              threshdiff (threshold for minimum of the max difference
 *                          between components)
 *              mindist (minimum allowed distance from nearest non-color pixel)
 *      Return: pixd (1 bpp, mask over color pixels), or null on error
 *
 *  Notes:
 *      (1) The generated mask identifies each pixel as either color or
 *          non-color.  For a pixel to be color, it must satisfy two
 *          constraints:
 *            (a) The max difference between the r,g and b components must
 *                equal or exceed a threshold @threshdiff.
 *            (b) It must be at least @mindist (in an 8-connected way)
 *                from the nearest non-color pixel.
 *      (2) The distance constraint (b) is only applied if @mindist > 1.
 *          For example, if @mindist == 2, the color pixels identified
 *          by (a) are eroded by a 3x3 Sel.  In general, the Sel size
 *          for erosion is 2 * (@mindist - 1) + 1.
 *          Why have this constraint?  In scanned images that are
 *          essentially gray, color artifacts are typically introduced
 *          in transition regions near sharp edges that go from dark
 *          to light, so this allows these transition regions to be removed.
 */
PIX *
pixMaskOverColorPixels(PIX     *pixs,
                       l_int32  threshdiff,
                       l_int32  mindist)
{
l_int32    w, h, d, i, j, wpls, wpld, size;
l_int32    rval, gval, bval, minval, maxval;
l_uint32  *datas, *datad, *lines, *lined;
PIX       *pixc, *pixd;
PIXCMAP   *cmap;

    PROCNAME("pixMaskOverColorPixels");

    if (!pixs)
        return (PIX *)ERROR_PTR("pixs not defined", procName, NULL);
    pixGetDimensions(pixs, &w, &h, &d);

    cmap = pixGetColormap(pixs);
    if (!cmap && d != 32)
        return (PIX *)ERROR_PTR("pixs not cmapped or 32 bpp", procName, NULL);
    if (cmap)
        pixc = pixRemoveColormap(pixs, REMOVE_CMAP_TO_FULL_COLOR);
    else
        pixc = pixClone(pixs);

    pixd = pixCreate(w, h, 1);
    datad = pixGetData(pixd);
    wpld = pixGetWpl(pixd);
    datas = pixGetData(pixc);
    wpls = pixGetWpl(pixc);
    for (i = 0; i < h; i++) {
        lines = datas + i * wpls;
        lined = datad + i * wpld;
        for (j = 0; j < w; j++) {
            extractRGBValues(lines[j], &rval, &gval, &bval);
            minval = L_MIN(rval, gval);
            minval = L_MIN(minval, bval);
            maxval = L_MAX(rval, gval);
            maxval = L_MAX(maxval, bval);
            if (maxval - minval >= threshdiff)
                SET_DATA_BIT(lined, j);
        }
    }

    if (mindist > 1) {
        size = 2 * (mindist - 1) + 1;
        pixErodeBrick(pixd, pixd, size, size);
    }

    pixDestroy(&pixc);
    return pixd;
}


/*!
 *  pixColorFraction()
 *
 *      Input:  pixs  (32 bpp rgb)
 *              darkthresh (threshold near black; if the lightest component
 *                          is below this, the pixel is not considered in
 *                          the statistics; typ. 20)
 *              lightthresh (threshold near white; if the darkest component
 *                           is above this, the pixel is not considered in
 *                           the statistics; typ. 244)
 *              diffthresh (thresh for the maximum difference between
 *                          component value; below this the pixel is not
 *                          considered to have sufficient color)
 *              factor (subsampling factor)
 *              &pixfract (<return> fraction of pixels in intermediate
 *                         brightness range that were considered
 *                         for color content)
 *              &colorfract (<return> fraction of pixels that meet the
 *                           criterion for sufficient color; 0.0 on error)
 *      Return: 0 if OK, 1 on error
 *
 *  Notes:
 *      (1) This function is asking the question: to what extent does the
 *          image appear to have color?   The amount of color a pixel
 *          appears to have depends on both the deviation of the
 *          individual components from their average and on the average
 *          intensity itself.  For example, the color will be much more
 *          obvious with a small deviation from white than the same
 *          deviation from black.
 *      (2) Any pixel that meets these three tests is considered a
 *          colorful pixel:
 *            (a) the lightest component must equal or exceed @darkthresh
 *            (b) the darkest component must not exceed @lightthresh
 *            (c) the max difference between components must equal or
 *                exceed @diffthresh.
 *      (3) The dark pixels are removed from consideration because
 *          they don't appear to have color.
 *      (4) The very lightest pixels are removed because if an image
 *          has a lot of "white", the color fraction will be artificially
 *          low, even if all the other pixels are colorful.
 *      (5) If pixfract is very small, there are few pixels that are neither
 *          black nor white.  If colorfract is very small, the pixels
 *          that are neither black nor white have very little color
 *          content.  The product 'pixfract * colorfract' gives the
 *          fraction of pixels with significant color content.
 *      (6) One use of this function is as a preprocessing step for median
 *          cut quantization (colorquant2.c), which does a very poor job
 *          splitting the color space into rectangular volume elements when
 *          all the pixels are near the diagonal of the color cube.  For
 *          octree quantization of an image with only gray values, the
 *          2^(level) octcubes on the diagonal are the only ones
 *          that can be occupied.
 */
l_int32
pixColorFraction(PIX        *pixs,
                 l_int32     darkthresh,
                 l_int32     lightthresh,
                 l_int32     diffthresh,
                 l_int32     factor,
                 l_float32  *ppixfract,
                 l_float32  *pcolorfract)
{
l_int32    i, j, w, h, wpl, rval, gval, bval, minval, maxval;
l_int32    total, npix, ncolor;
l_uint32   pixel;
l_uint32  *data, *line;

    PROCNAME("pixColorFraction");

    if (!ppixfract || !pcolorfract)
        return ERROR_INT("&pixfract and &colorfract not both defined",
                         procName, 1);
    *ppixfract = 0.0;
    *pcolorfract = 0.0;
    if (!pixs || pixGetDepth(pixs) != 32)
        return ERROR_INT("pixs not defined or not 32 bpp", procName, 1);

    pixGetDimensions(pixs, &w, &h, NULL);
    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    npix = ncolor = total = 0;
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            total++;
            pixel = line[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            minval = L_MIN(rval, gval);
            minval = L_MIN(minval, bval);
            if (minval > lightthresh)  /* near white */
                continue;
            maxval = L_MAX(rval, gval);
            maxval = L_MAX(maxval, bval);
            if (maxval < darkthresh)  /* near black */
                continue;

            npix++;
            if (maxval - minval >= diffthresh)
                ncolor++;
        }
    }

    if (npix == 0) {
        L_WARNING("No pixels found for consideration", procName);
        return 0;
    }
    *ppixfract = (l_float32)npix / (l_float32)total;
    *pcolorfract = (l_float32)ncolor / (l_float32)npix;
    return 0;
}


/*!
 *  pixNumSignificantGrayColors()
 *
 *      Input:  pixs  (8 bpp gray)
 *              darkthresh (dark threshold for minimum intensity to be
 *                          considered; typ. 20)
 *              lightthresh (threshold near white, for maximum intensity
 *                           to be considered; typ. 236)
 *              minfract (minimum fraction of all pixels to include a level
 *                        as significant; typ. 0.0001; should be < 0.001)
 *              factor (subsample factor; integer >= 1)
 *              &ncolors (<return> number of significant colors; 0 on error)
 *      Return: 0 if OK, 1 on error
 *
 *  Notes:
 *      (1) This function is asking the question: how many perceptually
 *          significant gray color levels is in this pix?
 *          A color level must meet 3 criteria to be significant:
 *            - it can't be too close to black
 *            - it can't be too close to white
 *            - it must have at least some minimum fractional population
 *      (2) Use -1 for default values for darkthresh, lightthresh and minfract.
 *      (3) Choose default of darkthresh = 20, because variations in very
 *          dark pixels are not visually significant.
 *      (4) Choose default of lightthresh = 236, because document images
 *          that have been jpeg'd typically have near-white pixels in the
 *          8x8 jpeg blocks, and these should not be counted.  It is desirable
 *          to obtain a clean image by quantizing this noise away.
 */
l_int32
pixNumSignificantGrayColors(PIX       *pixs,
                            l_int32    darkthresh,
                            l_int32    lightthresh,
                            l_float32  minfract,
                            l_int32    factor,
                            l_int32   *pncolors)
{
l_int32  i, w, h, count, mincount, ncolors;
NUMA    *na;

    PROCNAME("pixNumSignificantGrayColors");

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", procName, 1);
    *pncolors = 0;
    if (!pixs || pixGetDepth(pixs) != 8)
        return ERROR_INT("pixs not defined or not 8 bpp", procName, 1);
    if (darkthresh < 0) darkthresh = 20;  /* defaults */
    if (lightthresh < 0) lightthresh = 236;
    if (minfract < 0.0) minfract = 0.0001;
    if (minfract > 1.0)
        return ERROR_INT("minfract > 1.0", procName, 1);
    if (minfract >= 0.001)
        L_WARNING("minfract too big; likely to underestimate ncolors",
                  procName);
    if (lightthresh > 255 || darkthresh >= lightthresh)
        return ERROR_INT("invalid thresholds", procName, 1);
    if (factor < 1) factor = 1;

    pixGetDimensions(pixs, &w, &h, NULL);
    mincount = (l_int32)(minfract * w * h);
    if ((na = pixGetGrayHistogram(pixs, factor)) == NULL)
        return ERROR_INT("na not made", procName, 1);
    ncolors = 2;  /* add in black and white */
    for (i = darkthresh; i <= lightthresh; i++) {
        numaGetIValue(na, i, &count);
        if (count >= mincount)
            ncolors++;
    }

    *pncolors = ncolors;
    numaDestroy(&na);
    return 0;
}


/*!
 *  pixColorsForQuantization()
 *      Input:  pixs (8 bpp gray or 32 bpp rgb; with or without colormap)
 *              thresh (binary threshold on edge gradient; 0 for default)
 *              &ncolors (<return> the number of colors found)
 *              &iscolor (<optional return> 1 if significant color is found;
 *                        0 otherwise.  If pixs is 8 bpp, and does not have
 *                        a colormap with color entries, this is 0)
 *              debug (1 to output masked image that is tested for colors;
 *                     0 otherwise)
 *      Return: 0 if OK, 1 on error.
 *
 *  Notes:
 *      (1) This function finds a measure of the number of colors that are
 *          found in low-gradient regions of an image.  By its
 *          magnitude relative to some threshold (not specified in
 *          this function), it gives a good indication of whether
 *          quantization will generate posterization.   This number
 *          is larger for images with regions of slowly varying
 *          intensity (if 8 bpp) or color (if rgb). Such images, if
 *          quantized, may require dithering to avoid posterization,
 *          and lossless compression is then expected to be poor.
 *      (2) If pixs has a colormap, the number of colors returned is
 *          the number in the colormap.
 *      (3) It is recommended that document images be reduced to a width
 *          of 800 pixels before applying this function.  Then it can
 *          be expected that color detection will be fairly accurate
 *          and the number of colors will reflect both the content and
 *          the type of compression to be used.  For less than 15 colors,
 *          there is unlikely to be a halftone image, and lossless
 *          quantization should give both a good visual result and
 *          better compression.
 *      (4) When using the default threshold on the gradient (15),
 *          images (both gray and rgb) where ncolors is greater than
 *          about 15 will compress poorly with either lossless
 *          compression or dithered quantization, and they may be
 *          posterized with non-dithered quantization.
 *      (5) For grayscale images, or images without significant color,
 *          this returns the number of significant gray levels in
 *          the low-gradient regions.  The actual number of gray levels
 *          can be large due to jpeg compression noise in the background.
 *      (6) Similarly, for color images, the actual number of different
 *          (r,g,b) colors in the low-gradient regions (rather than the
 *          number of occupied level 4 octcubes) can be quite large, e.g.,
 *          due to jpeg compression noise, even for regions that appear
 *          to be of a single color.  By quantizing to level 4 octcubes,
 *          most of these superfluous colors are removed from the counting.
 *      (7) The image is tested for color.  If there is very little color,
 *          it is thresholded to gray and the number of gray levels in
 *          the low gradient regions is found.  If the image has color,
 *          the number of occupied level 4 octcubes is found.
 *      (8) The number of colors in the low-gradient regions increases
 *          monotonically with the threshold @thresh on the edge gradient.
 *      (9) Background: grayscale and color quantization is often useful
 *          to achieve highly compressed images with little visible
 *          distortion.  However, gray or color washes (regions of
 *          low gradient) can defeat this approach to high compression.
 *          How can one determine if an image is expected to compress
 *          well using gray or color quantization?  We use the fact that
 *            * gray washes, when quantized with less than 50 intensities,
 *              have posterization (visible boundaries between regions
 *              of uniform 'color') and poor lossless compression
 *            * color washes, when quantized with level 4 octcubes,
 *              typically result in both posterization and the occupancy
 *              of many level 4 octcubes.
 *          Images can have colors either intrinsically or as jpeg
 *          compression artifacts.  This function reduces but does not
 *          completely eliminate measurement of jpeg quantization noise
 *          in the white background of grayscale or color images.
 */
l_int32
pixColorsForQuantization(PIX      *pixs,
                         l_int32   thresh,
                         l_int32  *pncolors,
                         l_int32  *piscolor,
                         l_int32   debug)
{
l_int32    w, h, d, minside, factor;
l_float32  pixfract, colorfract;
PIX       *pixt, *pixsc, *pixg, *pixe, *pixb, *pixm;
PIXCMAP   *cmap;

    PROCNAME("pixColorsForQuantization");

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", procName, 1);
    *pncolors = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", procName, 1);
    if ((cmap = pixGetColormap(pixs)) != NULL) {
        *pncolors = pixcmapGetCount(cmap);
        if (piscolor)
            pixcmapHasColor(cmap, piscolor);
        return 0;
    }

    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 8 && d != 32)
        return ERROR_INT("pixs not 8 or 32 bpp", procName, 1);
    if (thresh <= 0) 
        thresh = 15;
    if (piscolor)
        *piscolor = 0;

        /* First test if 32 bpp has any significant color; if not,
         * convert it to gray.  Colors whose average values are within
         * 20 of black or 8 of white are ignored because they're not
         * very 'colorful'.  If less than 2.5/10000 of the pixels have
         * significant color, consider the image to be gray. */
    minside = L_MIN(w, h);
    if (d == 8)
        pixt = pixClone(pixs);
    else {  /* d == 32 */
        factor = L_MAX(1, minside / 400);
        pixColorFraction(pixs, 20, 248, 30, factor, &pixfract, &colorfract);
        if (pixfract * colorfract < 0.00025) {
            pixt = pixGetRGBComponent(pixs, COLOR_RED);
            d = 8;
        }
        else {  /* d == 32 */
            pixt = pixClone(pixs);
            if (piscolor)
                *piscolor = 1;
        }
    }

        /* If the smallest side is less than 1000, do not downscale.
         * If it is in [1000 ... 2000), downscale by 2x.  If it is >= 2000,
         * downscale by 4x.  Factors of 2 are chosen for speed.  The
         * actual resolution at which subsequent calculations take place
         * is not strongly dependent on downscaling.  */
    factor = L_MAX(1, minside / 500);
    if (factor == 1)
        pixsc = pixCopy(NULL, pixt);  /* to be sure pixs is unchanged */
    else if (factor == 2 || factor == 3)
        pixsc = pixScaleAreaMap2(pixt);
    else
        pixsc = pixScaleAreaMap(pixt, 0.25, 0.25);

        /* Basic edge mask generation procedure:
         *   - work on a grayscale image
         *   - get a 1 bpp edge mask by using an edge filter and
         *     thresholding to get fg pixels at the edges
         *   - for gray, dilate with a 3x3 brick Sel to get mask over
         *     all pixels within a distance of 1 pixel from the nearest
         *     edge pixel
         *   - for color, dilate with a 7x7 brick Sel to get mask over
         *     all pixels within a distance of 3 pixels from the nearest
         *     edge pixel  */
    if (d == 8)
        pixg = pixClone(pixsc);
    else  /* d == 32 */
        pixg = pixConvertRGBToLuminance(pixsc);
    pixe = pixSobelEdgeFilter(pixg, L_ALL_EDGES);
    pixb = pixThresholdToBinary(pixe, thresh);
    pixInvert(pixb, pixb);
    if (d == 8)
        pixm = pixMorphSequence(pixb, "d3.3", 0);
    else
        pixm = pixMorphSequence(pixb, "d7.7", 0);

        /* Mask the near-edge pixels to white, and count the colors.
         * If grayscale, don't count colors within 20 levels of
         * black or white, and only count colors with a fraction
         * of at least 1/10000 of the image pixels.
         * If color, count the number of level 4 octcubes that
         * contain at least 20 pixels.  These magic numbers are guesses
         * as to what might work, based on a small data set.  Results
         * should not be overly sensitive to their actual values. */
    if (d == 8) {
        pixSetMasked(pixg, pixm, 0xff);
        if (debug) pixWrite("junkpix8.png", pixg, IFF_PNG);
        pixNumSignificantGrayColors(pixg, 20, 236, 0.0001, 1, pncolors);
    }
    else {  /* d == 32 */
        pixSetMasked(pixsc, pixm, 0xffffffff);
        if (debug) pixWrite("junkpix32.png", pixsc, IFF_PNG);
        pixNumberOccupiedOctcubes(pixsc, 4, 20, -1, pncolors);
    }

    pixDestroy(&pixt);
    pixDestroy(&pixsc);
    pixDestroy(&pixg);
    pixDestroy(&pixe);
    pixDestroy(&pixb);
    pixDestroy(&pixm);
    return 0;
}


/*!
 *  pixNumColors()
 *      Input:  pixs (2, 4, 8, 32 bpp)
 *              factor (subsampling factor; integer)
 *              &ncolors (<return> the number of colors found, or 0 if
 *                        there are more than 256)
 *      Return: 0 if OK, 1 on error.
 *
 *  Notes:
 *      (1) This returns the actual number of colors found in the image,
 *          even if there is a colormap.  If @factor == 1 and the
 *          number of colors differs from the number of entries
 *          in the colormap, a warning is issued.
 *      (2) Use @factor == 1 to find the actual number of colors.
 *          Use @factor > 1 to quickly find the approximate number of colors.
 *      (3) For d = 2, 4 or 8 bpp grayscale, this returns the number
 *          of colors found in the image in 'ncolors'.
 *      (4) For d = 32 bpp (rgb), if the number of colors is
 *          greater than 256, this returns 0 in 'ncolors'.
 */
l_int32
pixNumColors(PIX      *pixs,
             l_int32   factor,
             l_int32  *pncolors)
{
l_int32    w, h, d, i, j, wpl, hashsize, sum, count;
l_int32    rval, gval, bval, val;
l_int32   *inta;
l_uint32   pixel;
l_uint32  *data, *line;
PIXCMAP   *cmap;

    PROCNAME("pixNumColors");

    if (!pncolors)
        return ERROR_INT("&ncolors not defined", procName, 1);
    *pncolors = 0;
    if (!pixs)
        return ERROR_INT("pixs not defined", procName, 1);
    pixGetDimensions(pixs, &w, &h, &d);
    if (d != 2 && d != 4 && d != 8 && d != 32)
        return ERROR_INT("d not in {2, 4, 8, 32}", procName, 1);
    if (factor < 1) factor = 1;

    data = pixGetData(pixs);
    wpl = pixGetWpl(pixs);
    sum = 0;
    if (d != 32) {  /* grayscale */
        inta = (l_int32 *)CALLOC(256, sizeof(l_int32));
        for (i = 0; i < h; i += factor) {
            line = data + i * wpl;
            for (j = 0; j < w; j += factor) {
                if (d == 8)
                    val = GET_DATA_BYTE(line, j);
                else if (d == 4)
                    val = GET_DATA_QBIT(line, j);
                else  /* d == 2 */
                    val = GET_DATA_DIBIT(line, j);
                inta[val] = 1;
            }
        }
        for (i = 0; i < 256; i++)
            if (inta[i]) sum++;
        *pncolors = sum;
        FREE(inta);

        if (factor == 1 && ((cmap = pixGetColormap(pixs)) != NULL)) {
            count = pixcmapGetCount(cmap);
            if (sum != count) 
                L_WARNING_INT("colormap size %d differs from actual colors",
                              procName, count);
        }
        return 0;
    }

        /* 32 bpp rgb; quit if we get above 256 colors */
    hashsize = 5507;  /* big and prime; collisions are not likely */
    inta = (l_int32 *)CALLOC(hashsize, sizeof(l_int32));
    for (i = 0; i < h; i += factor) {
        line = data + i * wpl;
        for (j = 0; j < w; j += factor) {
            pixel = line[j];
            extractRGBValues(pixel, &rval, &gval, &bval);
            val = (137 * rval + 269 * gval + 353 * bval) % hashsize;
            if (inta[val] == 0) {
                inta[val] = 1;
                sum++;
                if (sum > 256) {
                    FREE(inta);
                    return 0;
                }
            }
        }
    }

    *pncolors = sum;
    FREE(inta);
    return 0;
}


