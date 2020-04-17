/* SPDX-License-Identifier: BSD-2-Clause */

/*----------------------------------------------------------------------+
 |                                                                      |
 |      kpk.h -- pretty fast KPK endgame table generator                |
 |                                                                      |
 +----------------------------------------------------------------------*/

/*
 *  Copyright (C) 2015, Marcel van Kervinck
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

/*
 *  Use an externally defined board geometry to avoid conversions
 *  from the caller's own square indexing. All 8 orientations are
 *  supported by changing this header file. As a demonstration of
 *  this, geometry-h8g8.h is provided.
 */
#include "ng-geometry.h"

/*----------------------------------------------------------------------+
 |      Functions                                                       |
 +----------------------------------------------------------------------*/

/*
 *  Probe a KPK position from the in memory endgame table.
 *  Returns 0 for draw, 1 for win and -1 for loss.
 *
 *  The position must be legal for meaningful results.
 *  `side' is 0 for white to move and 1 for black to move.
 *
 *  If the table has not been generated yet, this will be
 *  done automatically at the first invocation.
 */
int kpkProbe(int side, int wKing, int wPawn, int bKing);

/*
 *  Explicitly generate the KPK table.
 *  Returns the memory size for info.
 *  This can take up to 2 milliseconds on a 2.6GHz Intel i7.
 */
int kpkGenerate(void);

/*
 *  Perform a self check on the bitbase.
 *  Returns 0 on failure, 1 for success.
 */
int kpkSelfCheck(void);

/* added, RA
 * Saves the bitbase both binary and as header include file.
 *  Returns 0 on failure, 1 for success.
 */
int kpkSave(char *filename);

/*----------------------------------------------------------------------+
 |                                                                      |
 +----------------------------------------------------------------------*/
