/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (opening book tool utility functions).
 *
 *  CT800 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  CT800 is distributed in the hope that it will be usefUL,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CT800. If not, see <http://www.gnu.org/licenses/>.
 *
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "booktool.h"
#include "util.h"

/*CRC algorithm with 8 bits per iteration: invented by Dilip V. Sarwate in 1988.*/
static const uint32_t crc32_table[256] = {
    0x00000000UL, 0x77073096UL, 0xEE0E612CUL, 0x990951BAUL, 0x076DC419UL, 0x706AF48FUL, 0xE963A535UL, 0x9E6495A3UL,
    0x0EDB8832UL, 0x79DCB8A4UL, 0xE0D5E91EUL, 0x97D2D988UL, 0x09B64C2BUL, 0x7EB17CBDUL, 0xE7B82D07UL, 0x90BF1D91UL,
    0x1DB71064UL, 0x6AB020F2UL, 0xF3B97148UL, 0x84BE41DEUL, 0x1ADAD47DUL, 0x6DDDE4EBUL, 0xF4D4B551UL, 0x83D385C7UL,
    0x136C9856UL, 0x646BA8C0UL, 0xFD62F97AUL, 0x8A65C9ECUL, 0x14015C4FUL, 0x63066CD9UL, 0xFA0F3D63UL, 0x8D080DF5UL,
    0x3B6E20C8UL, 0x4C69105EUL, 0xD56041E4UL, 0xA2677172UL, 0x3C03E4D1UL, 0x4B04D447UL, 0xD20D85FDUL, 0xA50AB56BUL,
    0x35B5A8FAUL, 0x42B2986CUL, 0xDBBBC9D6UL, 0xACBCF940UL, 0x32D86CE3UL, 0x45DF5C75UL, 0xDCD60DCFUL, 0xABD13D59UL,
    0x26D930ACUL, 0x51DE003AUL, 0xC8D75180UL, 0xBFD06116UL, 0x21B4F4B5UL, 0x56B3C423UL, 0xCFBA9599UL, 0xB8BDA50FUL,
    0x2802B89EUL, 0x5F058808UL, 0xC60CD9B2UL, 0xB10BE924UL, 0x2F6F7C87UL, 0x58684C11UL, 0xC1611DABUL, 0xB6662D3DUL,
    0x76DC4190UL, 0x01DB7106UL, 0x98D220BCUL, 0xEFD5102AUL, 0x71B18589UL, 0x06B6B51FUL, 0x9FBFE4A5UL, 0xE8B8D433UL,
    0x7807C9A2UL, 0x0F00F934UL, 0x9609A88EUL, 0xE10E9818UL, 0x7F6A0DBBUL, 0x086D3D2DUL, 0x91646C97UL, 0xE6635C01UL,
    0x6B6B51F4UL, 0x1C6C6162UL, 0x856530D8UL, 0xF262004EUL, 0x6C0695EDUL, 0x1B01A57BUL, 0x8208F4C1UL, 0xF50FC457UL,
    0x65B0D9C6UL, 0x12B7E950UL, 0x8BBEB8EAUL, 0xFCB9887CUL, 0x62DD1DDFUL, 0x15DA2D49UL, 0x8CD37CF3UL, 0xFBD44C65UL,
    0x4DB26158UL, 0x3AB551CEUL, 0xA3BC0074UL, 0xD4BB30E2UL, 0x4ADFA541UL, 0x3DD895D7UL, 0xA4D1C46DUL, 0xD3D6F4FBUL,
    0x4369E96AUL, 0x346ED9FCUL, 0xAD678846UL, 0xDA60B8D0UL, 0x44042D73UL, 0x33031DE5UL, 0xAA0A4C5FUL, 0xDD0D7CC9UL,
    0x5005713CUL, 0x270241AAUL, 0xBE0B1010UL, 0xC90C2086UL, 0x5768B525UL, 0x206F85B3UL, 0xB966D409UL, 0xCE61E49FUL,
    0x5EDEF90EUL, 0x29D9C998UL, 0xB0D09822UL, 0xC7D7A8B4UL, 0x59B33D17UL, 0x2EB40D81UL, 0xB7BD5C3BUL, 0xC0BA6CADUL,
    0xEDB88320UL, 0x9ABFB3B6UL, 0x03B6E20CUL, 0x74B1D29AUL, 0xEAD54739UL, 0x9DD277AFUL, 0x04DB2615UL, 0x73DC1683UL,
    0xE3630B12UL, 0x94643B84UL, 0x0D6D6A3EUL, 0x7A6A5AA8UL, 0xE40ECF0BUL, 0x9309FF9DUL, 0x0A00AE27UL, 0x7D079EB1UL,
    0xF00F9344UL, 0x8708A3D2UL, 0x1E01F268UL, 0x6906C2FEUL, 0xF762575DUL, 0x806567CBUL, 0x196C3671UL, 0x6E6B06E7UL,
    0xFED41B76UL, 0x89D32BE0UL, 0x10DA7A5AUL, 0x67DD4ACCUL, 0xF9B9DF6FUL, 0x8EBEEFF9UL, 0x17B7BE43UL, 0x60B08ED5UL,
    0xD6D6A3E8UL, 0xA1D1937EUL, 0x38D8C2C4UL, 0x4FDFF252UL, 0xD1BB67F1UL, 0xA6BC5767UL, 0x3FB506DDUL, 0x48B2364BUL,
    0xD80D2BDAUL, 0xAF0A1B4CUL, 0x36034AF6UL, 0x41047A60UL, 0xDF60EFC3UL, 0xA867DF55UL, 0x316E8EEFUL, 0x4669BE79UL,
    0xCB61B38CUL, 0xBC66831AUL, 0x256FD2A0UL, 0x5268E236UL, 0xCC0C7795UL, 0xBB0B4703UL, 0x220216B9UL, 0x5505262FUL,
    0xC5BA3BBEUL, 0xB2BD0B28UL, 0x2BB45A92UL, 0x5CB36A04UL, 0xC2D7FFA7UL, 0xB5D0CF31UL, 0x2CD99E8BUL, 0x5BDEAE1DUL,
    0x9B64C2B0UL, 0xEC63F226UL, 0x756AA39CUL, 0x026D930AUL, 0x9C0906A9UL, 0xEB0E363FUL, 0x72076785UL, 0x05005713UL,
    0x95BF4A82UL, 0xE2B87A14UL, 0x7BB12BAEUL, 0x0CB61B38UL, 0x92D28E9BUL, 0xE5D5BE0DUL, 0x7CDCEFB7UL, 0x0BDBDF21UL,
    0x86D3D2D4UL, 0xF1D4E242UL, 0x68DDB3F8UL, 0x1FDA836EUL, 0x81BE16CDUL, 0xF6B9265BUL, 0x6FB077E1UL, 0x18B74777UL,
    0x88085AE6UL, 0xFF0F6A70UL, 0x66063BCAUL, 0x11010B5CUL, 0x8F659EFFUL, 0xF862AE69UL, 0x616BFFD3UL, 0x166CCF45UL,
    0xA00AE278UL, 0xD70DD2EEUL, 0x4E048354UL, 0x3903B3C2UL, 0xA7672661UL, 0xD06016F7UL, 0x4969474DUL, 0x3E6E77DBUL,
    0xAED16A4AUL, 0xD9D65ADCUL, 0x40DF0B66UL, 0x37D83BF0UL, 0xA9BCAE53UL, 0xDEBB9EC5UL, 0x47B2CF7FUL, 0x30B5FFE9UL,
    0xBDBDF21CUL, 0xCABAC28AUL, 0x53B39330UL, 0x24B4A3A6UL, 0xBAD03605UL, 0xCDD70693UL, 0x54DE5729UL, 0x23D967BFUL,
    0xB3667A2EUL, 0xC4614AB8UL, 0x5D681B02UL, 0x2A6F2B94UL, 0xB40BBE37UL, 0xC30C8EA1UL, 0x5A05DF1BUL, 0x2D02EF8DUL
};

static const uint8_t crc8_table[256] = {
    0x00U, 0x3EU, 0x7CU, 0x42U, 0xF8U, 0xC6U, 0x84U, 0xBAU,
    0x95U, 0xABU, 0xE9U, 0xD7U, 0x6DU, 0x53U, 0x11U, 0x2FU,
    0x4FU, 0x71U, 0x33U, 0x0DU, 0xB7U, 0x89U, 0xCBU, 0xF5U,
    0xDAU, 0xE4U, 0xA6U, 0x98U, 0x22U, 0x1CU, 0x5EU, 0x60U,
    0x9EU, 0xA0U, 0xE2U, 0xDCU, 0x66U, 0x58U, 0x1AU, 0x24U,
    0x0BU, 0x35U, 0x77U, 0x49U, 0xF3U, 0xCDU, 0x8FU, 0xB1U,
    0xD1U, 0xEFU, 0xADU, 0x93U, 0x29U, 0x17U, 0x55U, 0x6BU,
    0x44U, 0x7AU, 0x38U, 0x06U, 0xBCU, 0x82U, 0xC0U, 0xFEU,
    0x59U, 0x67U, 0x25U, 0x1BU, 0xA1U, 0x9FU, 0xDDU, 0xE3U,
    0xCCU, 0xF2U, 0xB0U, 0x8EU, 0x34U, 0x0AU, 0x48U, 0x76U,
    0x16U, 0x28U, 0x6AU, 0x54U, 0xEEU, 0xD0U, 0x92U, 0xACU,
    0x83U, 0xBDU, 0xFFU, 0xC1U, 0x7BU, 0x45U, 0x07U, 0x39U,
    0xC7U, 0xF9U, 0xBBU, 0x85U, 0x3FU, 0x01U, 0x43U, 0x7DU,
    0x52U, 0x6CU, 0x2EU, 0x10U, 0xAAU, 0x94U, 0xD6U, 0xE8U,
    0x88U, 0xB6U, 0xF4U, 0xCAU, 0x70U, 0x4EU, 0x0CU, 0x32U,
    0x1DU, 0x23U, 0x61U, 0x5FU, 0xE5U, 0xDBU, 0x99U, 0xA7U,
    0xB2U, 0x8CU, 0xCEU, 0xF0U, 0x4AU, 0x74U, 0x36U, 0x08U,
    0x27U, 0x19U, 0x5BU, 0x65U, 0xDFU, 0xE1U, 0xA3U, 0x9DU,
    0xFDU, 0xC3U, 0x81U, 0xBFU, 0x05U, 0x3BU, 0x79U, 0x47U,
    0x68U, 0x56U, 0x14U, 0x2AU, 0x90U, 0xAEU, 0xECU, 0xD2U,
    0x2CU, 0x12U, 0x50U, 0x6EU, 0xD4U, 0xEAU, 0xA8U, 0x96U,
    0xB9U, 0x87U, 0xC5U, 0xFBU, 0x41U, 0x7FU, 0x3DU, 0x03U,
    0x63U, 0x5DU, 0x1FU, 0x21U, 0x9BU, 0xA5U, 0xE7U, 0xD9U,
    0xF6U, 0xC8U, 0x8AU, 0xB4U, 0x0EU, 0x30U, 0x72U, 0x4CU,
    0xEBU, 0xD5U, 0x97U, 0xA9U, 0x13U, 0x2DU, 0x6FU, 0x51U,
    0x7EU, 0x40U, 0x02U, 0x3CU, 0x86U, 0xB8U, 0xFAU, 0xC4U,
    0xA4U, 0x9AU, 0xD8U, 0xE6U, 0x5CU, 0x62U, 0x20U, 0x1EU,
    0x31U, 0x0FU, 0x4DU, 0x73U, 0xC9U, 0xF7U, 0xB5U, 0x8BU,
    0x75U, 0x4BU, 0x09U, 0x37U, 0x8DU, 0xB3U, 0xF1U, 0xCFU,
    0xE0U, 0xDEU, 0x9CU, 0xA2U, 0x18U, 0x26U, 0x64U, 0x5AU,
    0x3AU, 0x04U, 0x46U, 0x78U, 0xC2U, 0xFCU, 0xBEU, 0x80U,
    0xAFU, 0x91U, 0xD3U, 0xEDU, 0x57U, 0x69U, 0x2BU, 0x15U
};

/*CRC-32 table generator, just for info:
#define POLYNOMIAL  0xEDB88320UL
uint32_t i;
for (i = 0; i <= 0xFFU; i++)
{
    uint32_t crc, j;
    crc = i;
    for (j = 0; j < 8; j++)
        crc = (crc >> 1) ^ (-(int32_t)(crc & 1) & POLYNOMIAL);
    crc32_table[i] = crc;
}

CRC-8 similarly with polynomial 0xB2. For the reason choosing this polynomial,
cf. the paper "Cyclic Redundancy Code (CRC) Polynomial Selection For
Embedded Networks" by Philip Koopman and Tridib Chakravarty. The polynomial 0xA6
(in Koopman notation) is optimum for data length > 120 bits (a book position
has 65 bytes, i.e. 520 bits). 0xA6 in Koopman notation is x^8 +x^6 +x^3 +x^2 +1.
In conventional notation, this 0x4D (the x^8 term is left out). What we need is
the reverse of that, and that is 0xB2.
*/


/***************************************************************************/
/****************** resource management wrapper functions ******************/
/***************************************************************************/

/*this is a demonstration on how to provide additional robustness when
working with manual resource management (memory and files). prerequisite is
that every pointer is initialised to NULL upon declaration. otherwise, there
will be a compiler warning anyway (uninitialised access).

for production code, such debug routines can be redefined to be the normal
calloc/free resp. fopen/fclose so that the release version does not incur
performance issues. it's just commenting out the USE_RESOURCE_WRAPPERS define
in booktool.h .

actually, this approach not necessary for a simple program like this, but
it is a nice exercise for demonstrating the concept.*/

#ifdef USE_RESOURCE_WRAPPERS
/*safe code:
  remap the alloc routine to the wrapper and add line number and filename.
  
- when allocating memory to a pointer that isn't NULL beforehand, that might
  point to memory leakage because the free_safe() routine has been forgotten.
- when allocating memory, check for NULL return pointer and print a warning;
  if that is not accompanied by some application warning output, that points
  to a forgotten check of the return value.*/
void *alloc_safe_exec(size_t nitems, size_t size, void *buffer, char *filename, int32_t line, uint32_t verbosity)
{
    if ((buffer != NULL) && (verbosity > 0))
        fprintf(stderr, "WARNING: leftover pointer in %s, line %"PRId32".\r\n", filename, line);

    buffer = calloc(nitems, size);
    if ((buffer == NULL) && (verbosity > 0))
        fprintf(stderr, "WARNING: out of memory in %s, line %"PRId32".\r\n", filename, line);

    return(buffer);
}

/*safe code:
  remap the free routine to the wrapper and add line number and filename.
- when freeing, check whether the buffer pointer is NULL. the underlying
  problem is trying to free a buffer more than once.
- after freeing the buffer, set the pointer to NULL. if it is used afterwards
  (dangling pointer), that will give a segfault, which at least clearly
  indicates that there is a problem.*/
void free_safe_exec(void **buffer, char *filename, int32_t line, uint32_t verbosity)
{
    if (*buffer == NULL)
    {
        if (verbosity > 0)
            fprintf(stderr, "WARNING: freeing zero pointer in %s, line %"PRId32".\r\n", filename, line);
        return;
    }
    free(*buffer); /*free the actual memory*/
    *buffer = NULL; /*and re-initialise the buffer pointer*/
}

/*safe code:
  remap the fopen routine to the wrapper and add line number and filename.
- when opening a file to a pointer that isn't NULL beforehand, that might
  point to resource leakage because the fclose_safe() routine has been forgotten.
- when opening a file, check for NULL return pointer and print a warning;
  if that is not accompanied by some application warning output, that points
  to a forgotten check of the return value.*/
FILE *fopen_safe_exec(const char *openfilename, const char *openmode, FILE *fileptr, char *filename, int32_t line, uint32_t verbosity)
{
    if ((fileptr != NULL) && (verbosity > 0))
        fprintf(stderr, "WARNING: leftover file in %s, line %"PRId32".\r\n", filename, line);

    if (strlen(openfilename) == 0)
    {
        if (verbosity > 0)
            fprintf(stderr, "WARNING: empty file name in %s, line %"PRId32".\r\n", filename, line);
        return(NULL);
    }
    
    fileptr = fopen(openfilename, openmode);
    if ((fileptr == NULL) && (verbosity > 0))
        fprintf(stderr, "WARNING: file open failed in %s, line %"PRId32".\r\n", filename, line);

    return(fileptr);
}

/*safe code:
  remap the fclose routine to the wrapper and add line number and filename.
- when closing, check whether the file pointer is NULL. the underlying
  problem is trying to close a file more than once.
- after closing the file, set the pointer to NULL.*/
void fclose_safe_exec(FILE **fileptr, char *filename, int32_t line, uint32_t verbosity)
{
    if (*fileptr == NULL)
    {
        if (verbosity > 0)
            fprintf(stderr, "WARNING: closing closed file in %s, line %"PRId32".\r\n", filename, line);
        return;
    }
    fclose(*fileptr); /*close the actual file*/
    *fileptr = NULL; /*and re-initialise the file pointer*/
}

/*safe code:
  check that a pointer is NULL and frees the resource if it isn't.
  - to be used at function exits: when a pointer is not NULL, the associated
    memory has not been freed (memory leakage). Or a file has not been closed.*/
void leak_safe_exec(void **buffer, enum E_LEAKAGE type, char *filename, int32_t line, uint32_t verbosity)
{
    if (*buffer != NULL)
    {
        switch (type)
        {
            case TYPE_MEM:
                if (verbosity > 0)
                    fprintf(stderr, "WARNING: memory leakage in %s, line %"PRId32".\r\n", filename, line);
                free_safe_exec(buffer, filename, line, verbosity);
                break;
            case TYPE_FILE:
                if (verbosity > 0)
                    fprintf(stderr, "WARNING: file not closed in %s, line %"PRId32".\r\n", filename, line);
                fclose_safe_exec((FILE **)buffer, filename, line, verbosity);
                break;
            default:
                if (verbosity > 0)
                    fprintf(stderr, "WARNING: unknown resource leakage in %s, line %"PRId32".\r\n", filename, line);
                break;
        }
    }
}
#endif /*USE_RESOURCE_WRAPPERS */


/***************************************************************************/
/****************************** CRC algorithm ******************************/
/***************************************************************************/
/*1st/2nd pass: CRC algorithm with 8 bits per iteration.
invented by Dilip V. Sarwate in 1988.*/
uint32_t Util_Crc32(const void *buffer, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    const uint8_t* databyte = (const uint8_t*) buffer;

    while (len--)
    {
        uint32_t lookup = crc;
        lookup &= 0xFFUL;
        lookup ^= *databyte++;
        crc >>= 8;
        crc ^= crc32_table[lookup];
    }

    return(~crc);
}

uint8_t Util_Crc8(const void *buffer, size_t len)
{
    uint8_t crc = 0xFFU;
    const uint8_t* databyte = (const uint8_t*) buffer;

    while (len--)
        crc = crc8_table[crc ^ (*databyte++)];

    return(~crc);
}


/***************************************************************************/
/**************************** sorting algorithm ****************************/
/***************************************************************************/
/*2nd pass: that's a shell sort. not using the standard library's sorting
algorithm because its implementation might differ across different PC
platforms and compilers. the resulting book would be equivalent, but
not identical: different entries with the same sorting key might be in
different order. this way, the binary book can be exactly reproduced on
all platforms.

the Ciura sequence above 1750 is extended automatically as needed according
to the list length N. The extension is H_next = floor(2.25 * H_prev).*/

/*the inner sorting loop - as inlinable function to avoid code duplication.*/
static void Util_Sort_Exec(BOOK_POS *poslist, size_t N, size_t gap)
{
    size_t i;
    for (i = gap; i < N; i++)
    {
        ssize_t j;
        BOOK_POS value;
        
        memcpy(&value, poslist + i, sizeof(BOOK_POS));

        for (j = ((ssize_t) i) - ((ssize_t) gap);
             (j >= 0) && (poslist[j].crc40 > value.crc40);
             j -= ((ssize_t) gap))
        {
            memcpy(poslist + j + gap, poslist + j, sizeof(BOOK_POS));
        }
        memcpy(poslist + j + gap, &value, sizeof(BOOK_POS));
    }
}

/*the shellsort frame.*/
void Util_Sort_Moves(BOOK_POS *poslist, size_t N)
{
    static const int32_t shell_sort_gaps[] = 
        {1L, 4L, 10L, 23L, 57L,
         132L, 301L, 701L, 1750L};
    size_t max_conf_gap, ext_gap, N_div225;
    ssize_t sizeIndex;
    
    /*start out with the last known Ciura gap*/
    max_conf_gap = shell_sort_gaps[sizeof(shell_sort_gaps)/sizeof(shell_sort_gaps[0]) - 1L];
    ext_gap = max_conf_gap;
    N_div225 = (size_t) (( ((double) N) / 2.25) + 0.5);
    
    /*search the biggest extended gap that is still smaller than the list
    length. doing the check against N divided down avoids integer overflow
    in the loop in case of unrealistically high N.*/
    while (ext_gap < N_div225)
        ext_gap = ext_gap*2L + ext_gap/4L; /* multiplication with 2.25 and rounding downwards*/
    
    /*allow for some rounding error during the downscaling.*/
    max_conf_gap += 1;
    
    /*sorting with the extended gap sequence.*/
    while (ext_gap > max_conf_gap) /*skipped if N < 3938.*/
    {
        Util_Sort_Exec(poslist, N, ext_gap);
        ext_gap = (size_t) (( ((double) ext_gap) / 2.25) + 0.5);
    }

    /*switch to the fixed Ciura sequence*/
    for (sizeIndex = sizeof(shell_sort_gaps)/sizeof(shell_sort_gaps[0]) - 1L; sizeIndex >= 0; sizeIndex--)
        Util_Sort_Exec(poslist, N, shell_sort_gaps[sizeIndex]);
    
}

/***************************************************************************/
/***************************** initialisation ******************************/
/***************************************************************************/
/*1st/2nd pass: initialises the board to the starting position.*/
void Util_Set_Start_Pos(BOARD_POS *bpos, int32_t *epsquare)
{
    int32_t i;

    /*set up the white pieces.*/
    bpos->board[A1] = bpos->board[H1] = WROOK;
    bpos->board[B1] = bpos->board[G1] = WKNIGHT;
    bpos->board[C1] = bpos->board[F1] = WBISHOP;
    bpos->board[D1] = WQUEEN;
    bpos->board[E1] = WKING;

    /*set up the black pieces.*/
    bpos->board[A8] = bpos->board[H8] = BROOK;
    bpos->board[B8] = bpos->board[G8] = BKNIGHT;
    bpos->board[C8] = bpos->board[F8] = BBISHOP;
    bpos->board[D8] = BQUEEN;
    bpos->board[E8] = BKING;

    /*set up the white pawns.*/
    for (i = A2; i <= H2; i += FILE_DIFF)
        bpos->board[i] = WPAWN;

    /*set up the black pawns.*/
    for (i = A7; i <= H7; i += FILE_DIFF)
        bpos->board[i] = BPAWN;

    /*clear the squares of rank 3-6.*/
    for (i = A3; i <= H6; i++)
        bpos->board[i] = NO_PIECE;

    /*no en passant sqare in the initial position.*/
    *epsquare = NOSQUARE;

    /*castling flags reset:
      wra1moved=wrh1moved=wkmoved=bra8moved=brh8moved=bkmoved=0; 
      black to move = 0;*/
    bpos->board[STATUS_FLAGS] = FLAGS_RESET; 
}

/***************************************************************************/
/******************************** conversion *******************************/
/***************************************************************************/
/*1st/2nd pass: converts a move from ASCII string to board indices.*/
void Util_Move_Conv(const char *move, int32_t *from, int32_t *to)
{
    int32_t rank, file;

    if ((move[0] >= 'a') && (move[0] <= 'z'))
        file = move[0] - 'a';
    else
        file = move[0] - 'A';
    
    rank = move[1] - '1';

    *from = file + RANK_DIFF*rank;

    if ((move[2] >= 'a') && (move[2] <= 'z'))
        file = move[2] - 'a';
    else
        file = move[2] - 'A';

    rank = move[3] - '1';

    *to = file + RANK_DIFF*rank;
}

/***************************************************************************/
/******************************** line check *******************************/
/***************************************************************************/
/*1st/2nd pass: checks whether the character denotes the end of the line.
the comment starter '(' is recognised. if you use different comment starter
characters, just add them here.*/
int32_t Util_Is_Line_End(char line_char)
{
    if ((line_char == '\r') || (line_char == '\n') || (line_char == 0) || (line_char == '('))
        return(1);

    return(0);
}

/*1st/2nd pass: checks whether the character is a whitespace.*/
int32_t Util_Is_Whitespace(char line_char)
{
    if ((line_char == ' ') || (line_char == '\t'))
        return(1);

    return(0);
}

/*1st/2nd pass: checks whether the character denotes a passive knowledge move.*/
int32_t Util_Is_Passivemarker(char line_char)
{
    if ((line_char == '?') || (line_char == 'x') || (line_char == 'X'))
        return(1);

    return(0);
}

/*1st/2nd pass: checks whether the character starts a comment line.*/
int32_t Util_Is_Commentline(char line_char)
{
    if (line_char == '#')
        return(1);

    return(0);
}

/***************************************************************************/
/****************************** move execution *****************************/
/***************************************************************************/
/*1st/2nd pass: makes the move on the board.*/
void Util_Move_Do(BOARD_POS *bpos, int32_t *epsquare, int32_t from, int32_t to)
{
    int32_t diff;

    /*reset the en passant square resulting from this move.*/
    *epsquare = NOSQUARE;

    /*which side is to move?*/
    if ((bpos->board[STATUS_FLAGS] & BLACK_MV) == FLAGS_RESET)
        bpos->board[STATUS_FLAGS] |= BLACK_MV;
    else
        bpos->board[STATUS_FLAGS] &= ~((uint8_t) BLACK_MV);

    if (bpos->board[from] == WKING) /*white king move*/
    {
        bpos->board[STATUS_FLAGS] |= WKMOVED | WRH1MOVED | WRA1MOVED;
        if ((from == E1) && (to == G1)) /*castling kingside*/
        {
            bpos->board[F1] = bpos->board[H1];
            bpos->board[H1] = NO_PIECE;
        }
        if ((from == E1) && (to == C1)) /*castling queenside*/
        {
            bpos->board[D1] = bpos->board[A1];
            bpos->board[A1] = NO_PIECE;
        }
    }
    if (bpos->board[from] == BKING) /*black king move*/
    {
        bpos->board[STATUS_FLAGS] |= BKMOVED | BRH8MOVED | BRA8MOVED;
        if ((from == E8) && (to == G8)) /*castling kingside*/
        {
            bpos->board[F8] = bpos->board[H8];
            bpos->board[H8] = NO_PIECE;
        }
        if ((from == E8) && (to == C8)) /*castling queenside*/
        {
            bpos->board[D8] = bpos->board[A8];
            bpos->board[A8] = NO_PIECE;
        }
    }

    /*if a rook has moved, mark it as "not suitable for future castling". If both rooks
    have moved, castling is not possible anymore, so mark the king also as having moved.*/

    if (from == A1) /*white queenside rook moved*/
    {
        bpos->board[STATUS_FLAGS] |= WRA1MOVED;
        if (bpos->board[STATUS_FLAGS] & WRH1MOVED)
            bpos->board[STATUS_FLAGS] |= WKMOVED;
    }
    if (from == H1) /*white kingside rook moved*/
    {
        bpos->board[STATUS_FLAGS] |= WRH1MOVED;
        if (bpos->board[STATUS_FLAGS] & WRA1MOVED)
            bpos->board[STATUS_FLAGS] |= WKMOVED;
    }
    if (from == A8) /*black queenside rook moved*/
    {
        bpos->board[STATUS_FLAGS] |= BRA8MOVED;
        if (bpos->board[STATUS_FLAGS] & BRH8MOVED)
            bpos->board[STATUS_FLAGS] |= BKMOVED;
    }
    if (from == H8) /*black kingside rook moved*/
    {
        bpos->board[STATUS_FLAGS] |= BRH8MOVED;
        if (bpos->board[STATUS_FLAGS] & BRA8MOVED)
            bpos->board[STATUS_FLAGS] |= BKMOVED;
    }

    if ((bpos->board[from] == WPAWN) && (bpos->board[to] == NO_PIECE)) /*might be e.p.*/
    {
        diff = to-from;
        
        if ((diff == UP_LEFT) || (diff == UP_RIGHT))
            bpos->board[to-RANK_DIFF] = NO_PIECE; /*en passant*/

        if (diff == 2 * RANK_DIFF) /*white pawn two ranks forward - set up the en passant square*/
        {
            *epsquare = from+RANK_DIFF;
        }
    }

    if ((bpos->board[from] == BPAWN) && (bpos->board[to] == NO_PIECE)) /*might be e.p.*/
    {
        diff = from-to;
        
        if ((diff == UP_LEFT) || (diff == UP_RIGHT))
            bpos->board[to+RANK_DIFF] = NO_PIECE; /*en passant*/

        if (diff == 2 * RANK_DIFF) /*black pawn two ranks forward - set up the en passant square*/
            *epsquare = from-RANK_DIFF;
    }

    /*set the piece to its destination*/
    if ((bpos->board[from] == WPAWN) && (RANK(to) == RANK_8)) /*automatic queen promotion*/
        bpos->board[to] = WQUEEN;
    else if ((bpos->board[from] == BPAWN) && (RANK(to) == RANK_1)) /*automatic queen promotion*/
        bpos->board[to] = BQUEEN;
    else
        bpos->board[to] = bpos->board[from];
    
    bpos->board[from] = NO_PIECE; /*and clear the origin*/
}
