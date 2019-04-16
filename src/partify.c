/*====================================================================*
 -  Copyright (C) 2001 Leptonica.  All rights reserved.
 -
 -  Redistribution and use in source and binary forms, with or without
 -  modification, are permitted provided that the following conditions
 -  are met:
 -  1. Redistributions of source code must retain the above copyright
 -     notice, this list of conditions and the following disclaimer.
 -  2. Redistributions in binary form must reproduce the above
 -     copyright notice, this list of conditions and the following
 -     disclaimer in the documentation and/or other materials
 -     provided with the distribution.
 -
 -  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 -  ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 -  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 -  A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL ANY
 -  CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 -  EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 -  PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 -  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 -  OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 -  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 -  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *====================================================================*/

/*!
 * \file  partify.c
 * <pre>
 *
 *     Top level
 *         l_int32          partifyFiles()
 *         l_int32          partifyPixac()
 *
 *     Helper
 *         static l_int32   boxaRemoveVGaps()
 * </pre>
 */

#include "allheaders.h"

    /* Static helpler */
static l_ok boxaRemoveVGaps(BOXA *boxa);


/*---------------------------------------------------------------------*
 *                              Top level                              *
 *---------------------------------------------------------------------*/
/*!
 * \brief   partifyFiles()
 *
 * \param[in]    dirname    directory of files
 * \param[in]    substr     required filename substring; use NULL for all files
 * \param[in]    nparts     number of parts to generate (counting from top)
 * \param[in]    outroot    root name of output pdf files
 * \param[in]    debugfile  [optional] set to NULL for no debug output
 * \return  0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) All page images are compressed in png format into a pixacomp.
 *      (2) Each page image is deskewed, binarized, partified into %nparts,
 *          and saved in a set of pixacomps in tiff-g4 format.
 *      (3) Each partified pixacomp is rendered into a set of page images,
 *          and output as a pdf.
 * </pre>
 */
l_ok
partifyFiles(const char  *dirname,
             const char  *substr,
             l_int32      nparts,
             const char  *outroot,
             const char  *debugfile)
{
PIXA   *pixadb;
PIXAC  *pixac;

    PROCNAME("partifyFiles");

    if (!dirname)
        return ERROR_INT("dirname not defined", procName, 1);
    if (nparts < 0 || nparts > 10)
        return ERROR_INT("nparts not in [1 ... 10]", procName, 1);
    if (!outroot || outroot[0] == '\n')
        return ERROR_INT("outroot undefined or empty", procName, 1);

    pixadb = (debugfile) ? pixaCreate(0) : NULL;
    pixac = pixacompCreateFromFiles(dirname, substr, IFF_PNG);
    partifyPixac(pixac, nparts, outroot, pixadb);
    if (pixadb) {
        L_INFO("writing debug output to %s\n", procName, debugfile);
        pixaConvertToPdf(pixadb, 300, 1.0, L_FLATE_ENCODE, 0,
                         "Partify Debug", debugfile);
    }
    pixacompDestroy(&pixac);
    pixaDestroy(&pixadb);
    return 0;
}


/*!
 * \brief   partifyPixac()
 *
 * \param[in]    pixac      with at least one image
 * \param[in]    nparts     number of parts to generate (counting from top)
 * \param[in]    outroot    root name of output pdf files
 * \param[in]    pixadb     [optional] debug pixa; can be NULL
 * \return  0 if OK, 1 on error
 */
l_ok
partifyPixac(PIXAC       *pixac,
             l_int32      nparts,
             const char  *outroot,
             PIXA        *pixadb)
{
char     buf[512];
l_int32  i, j, pageno, npage, nbox, icount;
BOX     *box1, *box2;
BOXA    *boxa1, *boxa2, *boxa3;
PIX     *pix1, *pix2, *pix3, *pix4, *pix5;
PIXAC  **pixaca;

    PROCNAME("partifyPixac");

    if (!pixac)
        return ERROR_INT("pixac not defined", procName, 1);
    if ((npage = pixacompGetCount(pixac)) == 0)
        return ERROR_INT("pixac is empty", procName, 1);
    if (nparts < 1 || nparts > 10)
        return ERROR_INT("nparts not in [1 ... 10]", procName, 1);
    if (!outroot || outroot[0] == '\n')
        return ERROR_INT("outroot undefined or empty", procName, 1);

        /* Initialize the output array for each of the nparts */
    pixaca = (PIXAC **)LEPT_CALLOC(nparts, sizeof(PIXAC *));
    for (i = 0; i < nparts; i++)
        pixaca[i] = pixacompCreate(0);

        /* Process each page */
    for (pageno = 0; pageno < npage; pageno++) {
        if ((pix1 = pixacompGetPix(pixac, pageno)) == NULL) {
            L_ERROR("pix for page %d not found\n", procName, pageno);
            continue;
        }

            /* Binarize and deskew */
        pix2 = pixConvertTo1Adaptive(pix1);
        pix3 = pixDeskew(pix2, 0);
        pixDestroy(&pix1);
        pixDestroy(&pix2);
        if (!pix3) {
            L_ERROR("pix for page %d not deskewed\n", procName, pageno);
            continue;
        }

            /* Find the stave sets at 4x reduction */
        pix4 = pixMorphSequence(pix3, "r11", 0);
        boxa1 = pixConnCompBB(pix4, 8);
        boxa2 = boxaSelectByArea(boxa1, 15000, L_SELECT_IF_GT, NULL);
        boxa3 = boxaSort(boxa2, L_SORT_BY_Y, L_SORT_INCREASING, NULL);
        if (pixadb) {
            pix5 = pixConvertTo32(pix4);
            pixRenderBoxaArb(pix5, boxa3, 2, 255, 0, 0);
            pixaAddPix(pixadb, pix5, L_INSERT);
            pixDisplay(pix5, 100 * pageno, 100);
        }
        boxaDestroy(&boxa1);
        boxaDestroy(&boxa2);

        boxaRemoveVGaps(boxa3);
        if (pixadb) {
            pix5 = pixConvertTo32(pix4);
            pixRenderBoxaArb(pix5, boxa3, 2, 0, 255, 0);
            pixaAddPix(pixadb, pix5, L_INSERT);
            pixDisplay(pix5, 100 * pageno, 600);
        }
        pixDestroy(&pix4);

            /* Locate the stave sets at full resolution, and break
             * each one into the separate staves (parts).  A typical
             * set will have more than one part, but if one of the
             * parts is a keyboard, it will usually have two staves
             * (also called a Grand Staff), composed of treble and
             * bass staves.  For example, a classical violin sonata
             * could have a staff for the violin and two staves for 
             * the piano.  We would set nparts == 2, and extract both
             * of the piano staves as the piano part.  */
        boxa1 = boxaTransform(boxa3, 0, 0, 4.0, 4.0);
        boxaDestroy(&boxa3);
        nbox = boxaGetCount(boxa1);
        fprintf(stderr, "number of boxes in page %d: %d\n", pageno, nbox);
        for (i = 0; i < nbox; i++) {
            box1 = boxaGetBox(boxa1, i, L_COPY);
            pix1 = pixClipRectangle(pix3, box1, NULL);
            pix2 = pixMorphSequence(pix1, "d1.20 + o50.1 + o1.30", 0);
            boxa2 = pixConnCompBB(pix2, 8);
            boxa3 = boxaSort(boxa2, L_SORT_BY_Y, L_SORT_INCREASING, NULL);
            boxaRemoveVGaps(boxa3);
            icount = boxaGetCount(boxa3);
            if (icount < nparts)
                L_WARNING("nparts requested = %d, but only found %d\n",
                          procName, nparts, icount);
            for (j = 0; j < icount && j < nparts; j++) {
                box2 = boxaGetBox(boxa3, j, L_COPY);
                if (j == nparts - 1)  /* extend the box to the bottom */
                    boxSetSideLocations(box2, -1, -1, -1,
                                        pixGetHeight(pix1) - 1);
                pix4 = pixClipRectangle(pix1, box2, NULL);
                pixacompAddPix(pixaca[j], pix4, IFF_TIFF_G4);
                boxDestroy(&box2);
                pixDestroy(&pix4);
            }
            boxaDestroy(&boxa2);
            boxaDestroy(&boxa3);
            boxDestroy(&box1);
            pixDestroy(&pix1);
            pixDestroy(&pix2);
        }
        boxaDestroy(&boxa1);
        pixDestroy(&pix3);
    }

    for (i = 0; i < nparts; i++) {
        snprintf(buf, sizeof(buf), "%s-%d.pdf", outroot, i);
        L_INFO("writing part %d: %s\n", procName, i, buf);
        pixacompConvertToPdf(pixaca[i], 300, 1.0, L_G4_ENCODE, 0, NULL, buf);
        pixacompDestroy(&pixaca[i]);
    }
    LEPT_FREE(pixaca);
    return 0;
}


/*
 * \brief   boxaRemoveVGaps()
 *
 * \param[in]    boxa
 * \return   0 if OK, 1 on error
 *
 * <pre>
 * Notes:
 *      (1) The boxes in %boxa are aligned vertically.  Move the horizontal
 *          edges vertically to remove the gaps between boxes.
 * </pre>
 */
static  l_ok
boxaRemoveVGaps(BOXA  *boxa)
{
l_int32  nbox, i, y1, h1, y2, h2, delta;

    nbox = boxaGetCount(boxa);
    for (i = 0; i < nbox - 1; i++) {
        boxaGetBoxGeometry(boxa, i, NULL, &y1, NULL, &h1);
        boxaGetBoxGeometry(boxa, i + 1, NULL, &y2, NULL, &h2);
        delta = (y2 - y1 - h1) / 2;
        boxaAdjustBoxSides(boxa, i, 0, 0, 0, delta);
        boxaAdjustBoxSides(boxa, i + 1, 0, 0, -delta, 0);
    }
    boxaAdjustBoxSides(boxa, nbox - 1, 0, 0, 0, delta);  /* bot of last */
    return 0;
}
