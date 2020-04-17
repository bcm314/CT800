/* SPDX-License-Identifier: BSD-2-Clause */

/*----------------------------------------------------------------------+
 |                                                                      |
 |      kpk.c -- pretty fast KPK endgame table generator                |
 |                                                                      |
 +----------------------------------------------------------------------*/

/*
 *  Copyright (C) 2015, Marcel van Kervinck
 *  Copyright (portions) (C) 2016, Rasmus Althoff (added saving of the
 *                                 binary table to a file)
 *  Copyright (portions) (C) 2020, Rasmus Althoff (reduced table from 32k
 *                                 to 24k)
 *
 *  All rights reserved
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *
 *  1. Redistributions of source code must retain the above copyright
 *  notice, this list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright
 *  notice, this list of conditions and the following disclaimer in the
 *  documentation and/or other materials provided with the distribution.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 *  FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 *  COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 *  BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 *  LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 *  ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

/*----------------------------------------------------------------------+
 |      Includes                                                        |
 +----------------------------------------------------------------------*/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "kpk.h"

/*----------------------------------------------------------------------+
 |      Definitions                                                     |
 +----------------------------------------------------------------------*/

enum { N = a2-a1, S = -N, E = b1-a1, W = -E }; // Derived geometry

#define inPawnZone(square) (rank(square)!=rank1 && rank(square)!=rank8)
#define conflict(wKing,wPawn,bKing) (wKing==wPawn || wPawn==bKing || wKing==bKing)

#define dist(a,b) (abs(file(a)-file(b)) | abs(rank(a)-rank(b))) // logical-or as max()
#define wInCheck(wKing,wPawn,bKing) (dist(wKing,bKing)==1)
#define bInCheck(wKing,wPawn,bKing) (dist(wKing,bKing)==1                     \
    || (file(wPawn)!=fileA && wPawn+N+W==bKing) \
    || (file(wPawn)!=fileH && wPawn+N+E==bKing))

// Square set macros (no need to adapt these to the specific geometry)
#define bit(i) (1ULL << (i))
#define mask 0x0101010101010101ULL
#define allW(set) ((set) >> 8)
#define allE(set) ((set) << 8)
#define allS(set) (((set) & ~mask) >> 1)
#define allN(set) (((set) << 1) & ~mask)
#define allKing(set) (allW(allN(set)) | allN(set) | allE(allN(set)) \
    | allW(set)                   | allE(set)       \
    | allW(allS(set)) | allS(set) | allE(allS(set)))

#define arrayLen(a) (sizeof(a) / sizeof((a)[0]))
enum { white, black };

#define kpIndex(wKing,wPawn) ((rank(wPawn) << 8) + (file(wPawn) << 6) + (wKing))
#define wKingSquare(ix) ((ix)&0x3F)
#define wPawnSquare(ix) square(((ix)>>6)&3, (ix)>>8)

/*----------------------------------------------------------------------+
 |      Data                                                            |
 +----------------------------------------------------------------------*/

static uint64_t kpkTable[2][32*64];
static uint64_t kpkLookupTable[2][24*64];
static const int kingSteps[] = { N+W, N, N+E, W, E, S+W, S, S+E };

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

int kpkProbe(int side, int wKing, int wPawn, int bKing)
{
    if (!kpkTable[0][1]) kpkGenerate();

    if (file(wPawn) >= 4) {
        wPawn ^= square(7, 0);
        wKing ^= square(7, 0);
        bKing ^= square(7, 0);
    }

    /*the lowest 4*64 positions would be for the pawn on the first rank,
      which has been left out from the table.*/
    int ix = kpIndex(wKing, wPawn) - 4*64;
    int bit = (kpkLookupTable[side][ix] >> bKing) & 1;
    return (side == white) ? bit : -bit;
}

int kpkGenerate(void)
{
    uint64_t valid[ arrayLen(kpkTable[0]) ];

    for (int ix=0; ix<(int)arrayLen(kpkTable[0]); ix++) {
        int wKing = wKingSquare(ix), wPawn = wPawnSquare(ix);

        // Positions after winning pawn promotion (we can ignore stalemate here)
        if (rank(wPawn) == rank8 && wKing != wPawn) {
            uint64_t lost = ~allKing(bit(wKing)) & ~bit(wKing) & ~bit(wPawn);
            if (dist(wKing, wPawn) > 1)
            lost &= ~allKing(bit(wPawn));
            kpkTable[black][ix] = lost;
        }

        // Valid positions after black move, pawn capture allowed
        valid[ix] = ~allKing(bit(wKing));
        if (rank(wPawn) != rank8 && file(wPawn) != fileA) valid[ix] &= ~bit(wPawn+N+W);
        if (rank(wPawn) != rank8 && file(wPawn) != fileH) valid[ix] &= ~bit(wPawn+N+E);
    }

    int changed;
    do {
        for (int ix=0; ix<(int)arrayLen(kpkTable[0]); ix++) {
            int wKing = wKingSquare(ix), wPawn = wPawnSquare(ix);
            if (!inPawnZone(wPawn))
                continue;

            // White king moves
            uint64_t won = 0;
            for (int i=0; i<(int)arrayLen(kingSteps); i++) {
                int to = wKing + kingSteps[i];
                int jx = ix + kpIndex(kingSteps[i], 0);
                if (dist(wKing, to & 63) == 1 && to != wPawn)
                won |= kpkTable[black][jx] & ~allKing(bit(to));
            }
            // White pawn moves
            if (wPawn+N != wKing) {
                won |= kpkTable[black][ix+kpIndex(0,N)] & ~bit(wPawn+N);
                if (rank(wPawn) == rank2 && wPawn+N+N != wKing)
                won |= kpkTable[black][ix+kpIndex(0,N+N)]
                & ~bit(wPawn+N) & ~bit(wPawn+N+N);
            }
            kpkTable[white][ix] = won & ~bit(wPawn);
        }

        changed = 0;
        for (int ix=0; ix<(int)arrayLen(kpkTable[0]); ix++) {
            if (!inPawnZone(wPawnSquare(ix)))
                continue;

            // Black king moves
            uint64_t isBad = kpkTable[white][ix] | ~valid[ix];
            uint64_t canDraw = allKing(~isBad);
            uint64_t hasMoves = allKing(valid[ix]);
            uint64_t lost = hasMoves & ~canDraw;

            changed += (kpkTable[black][ix] != lost);
            kpkTable[black][ix] = lost;
        }
    } while (changed);

    for (int ix=0; ix<24*64; ix++)
    {
        /*the first rank cannot hold a white pawn, which is the first 4*64
          positions. the last rank can't either, that's not being copied.*/
        kpkLookupTable[0][ix] = kpkTable[0][ix + 4*64];
        kpkLookupTable[1][ix] = kpkTable[1][ix + 4*64];
    }

    return sizeof kpkLookupTable;
}

int kpkSelfCheck(void)
{
    int counts[] = {            // As given by Steven J. Edwards (1996):
        163328 / 2, 168024 / 2, // - Legal positions per side
        124960 / 2, 97604  / 2  // - Non-draw positions per side
    };
    for (int ix=0; ix<(int)arrayLen(kpkTable[0]); ix++) {
        int wKing = wKingSquare(ix), wPawn = wPawnSquare(ix);
        for (int bKing=0; bKing<boardSize; bKing++) {
            if (!inPawnZone(wPawn) || conflict(wKing, wPawn, bKing))
                continue;
            counts[0] -= !bInCheck(wKing, wPawn, bKing);
            counts[1] -= !wInCheck(wKing, wPawn, bKing);
            counts[2] -= !bInCheck(wKing, wPawn, bKing) && ((kpkTable[white][ix] >> bKing) & 1);
            counts[3] -= !wInCheck(wKing, wPawn, bKing) && ((kpkTable[black][ix] >> bKing) & 1);
        }
    }
    return !counts[0] && !counts[1] && !counts[2] && !counts[3];
}

/*added, RA. Saves the data as binary and as header include file.*/
int kpkSave(char *filename)
{
    const int32_t maxdata = 24575;

    FILE *outfile;
    FILE *include_file;

    outfile = fopen(filename, "wb");
    if (outfile == NULL)
        return(0);
    include_file = fopen("kpk_table.c", "wb");
    if (include_file == NULL)
    {
        fclose(outfile);
        return(0);
    }

    /*declaration of the bitbase array - name is fixed*/
    fputs("static FLASH_ROM __attribute__ ((aligned (4))) const uint8_t kpk_dat[24576] = {", include_file);

    for (int32_t i=0, cnt=0; i<2; i++)
    {
        for (int32_t j = 0; j < 24*64; j++)
        {
            /*to avoid any possible endianess issues, the saving is done
              byte-wise in a defined order so that reading will be a fast
              repeated shift-read-add sequence.*/
            uint8_t c;
            char conv_buffer[20];
            uint64_t val = kpkLookupTable[i][j];

            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) ((val >> 56) & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) ((val >> 48) & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) ((val >> 40) & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) ((val >> 32) & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) ((val >> 24) & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) ((val >> 16) & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) ((val >> 8) & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
            if ((cnt % 12) == 0)
                fputs("\r\n  ", include_file);

            c = (uint8_t) (val & 0xffull);
            putc(c, outfile);
            sprintf(conv_buffer, "0x%02x", c);
            if (cnt < maxdata) /*more data follow*/
            {
                strcat(conv_buffer, ",");
                if (((cnt+1) % 12) != 0) /*no trailing space at the line end*/
                    strcat(conv_buffer, " ");
            }
            fputs(conv_buffer, include_file);
            cnt++;
        }
    }

    /*end of bitbase array*/
    fputs("\r\n};\r\n", include_file);

    fclose(include_file);	
    fclose(outfile);
    return(1);
}

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/
