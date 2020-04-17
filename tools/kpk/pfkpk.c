/* SPDX-License-Identifier: BSD-2-Clause */

/*----------------------------------------------------------------------+
 |                                                                      |
 |      pfkpk.c -- pretty fast KPK endgame table generator tester       |
 |                                                                      |
 +----------------------------------------------------------------------*/

/*
 *  Copyright (C) 2015, Marcel van Kervinck
 *  Copyright (portions) (C) 2016, Rasmus Althoff (added saving of the
 *                                 binary table to a file.)
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

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "kpk.h"

struct {
    int side, wKing, wPawn, bKing, expected;
} tests[] = {
    { 0, a1, a2, a8, 0 },
    { 0, a1, a2, h8, 1 },
    { 1, a1, a2, a8, 0 },
    { 1, a1, a2, h8, -1 },
    { 1, a1, a2, g2, 0 },
    { 1, a1, a2, g1, -1 },
    { 0, a5, a4, d4, 1 },
    { 1, a5, a4, d4, 0 },
    { 0, a1, f4, a3, 1 },
    { 1, a1, f4, a3, 0 },
    { 1, a3, a4, f3, -1 },
    { 0, h6, g6, g8, 1 },
    { 0, h3, h2, b7, 1 },
    { 1, a5, a4, e6, 0 },
    { 1, f8, g6, h8, 0 },
    { 0, f6, g5, g8, 1 },
    { 0, d1, c3, f8, 1 },
    { 0, d4, c4, e6, 1 },
    { 0, c6, d6, d8, 1 },
    { 1, d6, e6, d8, -1 },
    { 0, g6, g5, h8, 1 },
    { 1, g6, g5, h8, -1 },
    { 0, e4, e3, e6, 0 },
    { 1, e4, e3, e6, -1 },
    { 1, h3, b2, h5, -1 },
    { 0, g2, b2, g5, 1 },
    { 0, g5, g4, h7, 1 },
    { 1, g5, g4, h7, 0 },
    { 0, e2, a7, c7, 1 },
    { 1, e2, a7, c7, 0 },
    { 0, e2, d7, b7, 1 },
    { 1, e2, d7, b7, 0 },
    {-1,  0,  0, 0,  0 },
};



int main(void)
{
    int err = 0;
    bool passed;

    /*
     *  kpkGenerate speed
     */
    clock_t start = clock();
    int size = kpkGenerate();
    clock_t finish = clock();
    printf("kpkGenerate CPU time [seconds]: %g\n",
    (double)(finish - start) / CLOCKS_PER_SEC);

    /*
     *  kpkTable size
     */
    printf("kpkTable size [bytes]: %d\n", size);

    /*
     *  kpkSelfCheck
     */
    passed = kpkSelfCheck();
    printf("kpkSelfCheck: %s\n", passed ? "OK" : "FAILED");
    if (!passed)
    err = EXIT_FAILURE;

    /*
     *  kpkProbe
     */
    int nrPassed = 0;
    int ix;
    for (ix=0; tests[ix].side>=0; ix++) {
        int result = kpkProbe(tests[ix].side,
        tests[ix].wKing,
        tests[ix].wPawn,
        tests[ix].bKing);
        if (result == tests[ix].expected)
        nrPassed++;
    }
    passed = (nrPassed == ix);
    printf("kpkProbe %d/%d: %s\n", nrPassed, ix, passed ? "OK" : "FAILED");
    if (!passed)
    err = EXIT_FAILURE;

    kpkSave("kpk.dat");
    return err;
}
