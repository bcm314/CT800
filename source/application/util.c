/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2020, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of the CT800 (utility functions).
 *
 *  CT800/NGPlay is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  CT800/NGPlay is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CT800/NGPlay. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <stdint.h>
#include <stddef.h>
#include "ctdefs.h"

/*--------- external variables ---------*/

/*--------- module variables ------------*/

/*a CRC is needed when loading/saving a game for checking that the contents of the
backup RAM actually contain a saved game and not just arbitrary data.

used polynomial: 0xEDB88320

CRC algorithm with 8 bits per iteration: invented by Dilip V. Sarwate in 1988.*/

static FLASH_ROM const uint32_t Crc32Table[256] = {
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

static FLASH_ROM const uint8_t crc8_table[256] = {
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
/*CRC table generator, just for info:
#define POLYNOMIAL  0xEDB88320UL
uint32_t crc, i, j;
for (i = 0; i <= 0xFFU; i++)
{
    crc = i;
    for (j = 0; j < 8; j++)
        crc = (crc >> 1) ^ (-(int32_t)(crc & 1) & POLYNOMIAL);
    Crc32Table[i] = crc;
}

CRC-8 similarly with polynomial 0xB2. For the reason choosing this polynomial,
cf. the paper "Cyclic Redundancy Code (CRC) Polynomial Selection For
Embedded Networks" by Philip Koopman and Tridib Chakravarty. The polynomial 0xA6
(in Koopman notation) is optimum for data length > 120 bits (a book position
has 65 bytes, i.e. 520 bits). 0xA6 in Koopman notation is x^8 +x^6 +x^3 +x^2 +1.
In conventional notation, this 0x4D (the x^8 term is left out). What we need is
the reverse of that, and that is 0xB2.
*/

char Util_Key_To_Char(enum E_KEY key)
{
    switch (key)
    {
    case KEY_A1:
        return('a');
    case KEY_B2:
        return('b');
    case KEY_C3:
        return('c');
    case KEY_D4:
        return('d');
    case KEY_E5:
        return('e');
    case KEY_F6:
        return('f');
    case KEY_G7:
        return('g');
    case KEY_H8:
        return('h');
    default:
        return('?');
    }
}

char Util_Key_To_Digit(enum E_KEY key)
{
    switch (key)
    {
    case KEY_A1:
        return('1');
    case KEY_B2:
        return('2');
    case KEY_C3:
        return('3');
    case KEY_D4:
        return('4');
    case KEY_E5:
        return('5');
    case KEY_F6:
        return('6');
    case KEY_G7:
        return('7');
    case KEY_H8:
        return('8');
    default:
        return('?');
    }
}

char Util_Key_To_Prom(enum E_KEY key)
{
    switch (key)
    {
    case KEY_PROM_QUEEN:
        return(WQUEEN_CHAR);
    case KEY_PROM_ROOK:
        return(WROOK_CHAR);
    case KEY_PROM_BISHOP:
        return(WBISHOP_CHAR);
    case KEY_PROM_KNIGHT:
        return(WKNIGHT_CHAR);
    default:
        return('?');
    }
}

/*CRC algorithm with 8 bits per iteration: invented by Dilip V. Sarwate in 1988.*/
uint32_t Util_Crc32(const void *buffer, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    const uint8_t *databyte = (const uint8_t *) buffer;
    size_t rest_len = len & 0x07U;

    /*unroll the CRC loop 8 times.*/
    len >>= 3;
    while (len--)
    {
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];
    }

    /*do the possible leftovers from the unrolled loop.*/
    while (rest_len--)
        crc = (crc >> 8) ^ Crc32Table[(uint8_t)(crc ^ *databyte++)];

    return(~crc);
}

uint8_t Util_Crc8(const void *buffer, size_t len)
{
    uint8_t crc = 0xFFU;
    const uint8_t *databyte = (const uint8_t *) buffer;
    size_t rest_len = len & 0x07U;

    /*unroll the CRC loop 8 times.*/
    len >>= 3;
    while (len--)
    {
        crc = crc8_table[crc ^ *databyte++];
        crc = crc8_table[crc ^ *databyte++];
        crc = crc8_table[crc ^ *databyte++];
        crc = crc8_table[crc ^ *databyte++];
        crc = crc8_table[crc ^ *databyte++];
        crc = crc8_table[crc ^ *databyte++];
        crc = crc8_table[crc ^ *databyte++];
        crc = crc8_table[crc ^ *databyte++];
    }

    /*do the possible leftovers from the unrolled loop.*/
    while (rest_len--)
        crc = crc8_table[crc ^ *databyte++];

    return(~crc);
}


/*that's used for retrieving the CRC from the opening book. To avoid
possible endianess issues in the future, the opening book tool has
saved the 32bit CRC bytewise, most significant byte first.*/
uint32_t Util_Hex_Long_To_Int(const uint8_t *buffer)
{
    uint32_t ret;

    ret = *buffer++;
    ret = (ret << 8) | *buffer++;
    ret = (ret << 8) | *buffer++;
    ret = (ret << 8) | *buffer;

    return(ret);
}

/*converts a 32 bit number into a hex ASCII buffer*/
void Util_Long_Int_To_Hex(uint32_t value, char *buffer)
{
    uint32_t i, mask = 0xF0000000UL;

    for (i = 0; i < 8; i++)
    {
        uint32_t nibble;

        nibble   = value & mask;
        nibble >>= 28U - 4U * i;
        buffer[i] = (nibble < 10U) ? nibble + '0' : nibble - 10 + 'a';
        mask >>= 4;
    }
}

/*copies a string, but does not return a pointer.*/
void Util_Strcpy(char *dest, const char *src)
{
    while ((*dest++ = *src++)) ;
}

/*attaches a string, but does not return a pointer.*/
void Util_Strcat(char *dest, const char *src)
{
    /*look for the end of the first string*/
    while (*dest) dest++;
    /*and append the source string*/
    while ((*dest++ = *src++)) ;
}

/*inserts a string without copying the terminating 0.*/
void Util_Strins(char *dest, const char *src)
{
    while ((*src) && (*dest))
        *dest++ = *src++;
}

/*checks only for equality! 0 is equal, 1 is not equal.*/
int Util_Strcmp(const char *str1, const char *str2)
{
    while ((*str1) && (*str1 == *str2))
    {
        str1++;
        str2++;
    }
    if (*str1 != *str2) /*different strings*/
        return(1);

    return(0); /*same strings*/
}

size_t Util_Strlen(const char *str)
{
    const char *s = str;

    while (*s) s++;

    return(s - str);
}

/* converts milliseconds to "h:mm:ss".
This routine does NOT zero-terminate the string; if need be, the
caller has to do that. This way, we can print the time directly into
a user given string at any given position. The caller has to take care
that there are 7 bytes of buffer memory to print into.
Note that the Cortex M has native support for the modulo operator.
Otherwise, the way of conversion used here would be quite expensive.
Still, the caller logic should take care only to call this function
and do the rest of the printout if at least one second has elapsed since the
last display update.
Since this will ultimately result in doing parallel I/O to the display,
this should not be called from the timer interrupt.*/
void Util_Time_To_String(char *buffer, int32_t intime, enum E_UT_LEAD_ZEROS leading_zeros, enum E_UT_ROUND rounding_time)
{
    if (intime >= 0)
    {
        int32_t tmp;
        if (intime > ((10L*60L*60L - 1L)*MILLISECONDS))
            /* more than 9:59:59. Can happen in time per move mode when the user
            doesn't move because he lets the machine just stand there.*/
        {
            buffer[0] = '9';
            buffer[1] = ':';
            buffer[2] = '6';
            buffer[3] = '0';
            buffer[4] = ':';
            buffer[5] = '0';
            buffer[6] = '0';
            return;
        }

        tmp = intime % MILLISECONDS;
        intime /= MILLISECONDS; /*convert to seconds*/
        /*if fractional time, use appropriate rounding*/
        if ((tmp != 0) && (rounding_time == UT_TIME_ROUND_CEIL))
            intime++;
        /*intime now in seconds*/
        tmp = intime % 60L;
        buffer[6] = (tmp % 10L) + '0';
        buffer[5] = (tmp / 10L) + '0';
        buffer[4] = ':';

        intime /= 60L; /*intime now in minutes*/
        tmp = intime % 60L;
        buffer[3] = (tmp % 10L) + '0';
        /*less than ten minutes, omit the leading zero digits?*/
        if ((intime < 10L) && (leading_zeros == UT_NO_LEAD_ZEROS))
        {
            buffer[0] = ' ';
            buffer[1] = ' ';
            buffer[2] = ' ';
            return;
        }
        buffer[2] = (tmp / 10L) + '0';

        intime /= 60L; /*intime now in hours*/
        /*less than one hour, omit the leading zero digits?*/
        if ((intime == 0)  && (leading_zeros == UT_NO_LEAD_ZEROS))
        {
            buffer[0] = ' ';
            buffer[1] = ' ';
            return;
        }
        buffer[1] = ':';
        buffer[0] = intime + '0';
    } else /*can happen in case of loss on time where intime might become slightly negative*/
    {
        /*display "   0:00" and not "0:00:00" because the few seconds before,
        the display looked like "   0:05", so don't suddenly plop back to the long format.
        Besides, if e.g. "game in 15 minutes" has been used, the long format has never been used
        throughout the whole game.*/
        buffer[0] = ' ';
        buffer[1] = ' ';
        buffer[2] = ' ';
        buffer[3] = '0';
        buffer[4] = ':';
        buffer[5] = '0';
        buffer[6] = '0';
    }
}

/*one additional hour position, for tens of hours. in mating search mode,
this will still fit the display.*/
void Util_Long_Time_To_String(char *buffer, int32_t intime, enum E_UT_ROUND rounding_time)
{
    int32_t tmp;
    enum E_UT_LEAD_ZEROS lead_zeros;
    tmp = intime / (10L*60L*60L*MILLISECONDS);
    if (tmp != 0)
    {
        buffer[0] = (tmp <= 9L) ? ((char) tmp) + '0' : 'X';
        lead_zeros = UT_LEAD_ZEROS;
    } else
    {
        buffer[0] = ' ';
        lead_zeros = UT_NO_LEAD_ZEROS;
    }
    intime %= (10L*60L*60L*MILLISECONDS);
    /*round downwards in mating mode*/
    Util_Time_To_String(buffer+1, intime, lead_zeros, rounding_time);
}

void Util_Depth_To_String(char *buffer, int depth)
{
    if ((depth < 0) || (depth > 99))
    {
        /*should never happen*/
        buffer[0] = buffer[1] = '.';
        return;
    }

    if (depth == 0)
    {
        buffer[0] = ' ';
        buffer[1] = '0';
        return;
    }

    buffer[1] = (depth % 10) + '0';
    depth /= 10;
    buffer[0] = (depth != 0) ? depth + '0' : ' ';
}

void Util_Centipawns_To_String(char *buffer, int centipawns)
{
    int sign;

    if (centipawns < 0)
    {
        sign = 1;
        centipawns = -centipawns;
    } else
        sign = 0;

    if (centipawns > 9999)
        centipawns = 9999;

    buffer[5] = (centipawns % 10) + '0';
    centipawns /= 10L;
    buffer[4] = (centipawns % 10) + '0';
    buffer[3] = '.';
    centipawns /= 10L;
    buffer[2] = (centipawns % 10) + '0';
    centipawns /= 10;
    if (centipawns > 0)
    {
        buffer[0] = (sign) ? '-' : '+';
        buffer[1] = centipawns + '0';
    } else {
        buffer[0] = ' ';
        buffer[1] = (sign) ? '-' : '+';
    }
}

/*converts 3-digit integers in the range of 0-999 to ascii, with leading spaces (not 0-terminated).
the buffer must have at least 3 bytes of space.*/
void Util_Itoa(char *buffer, int number)
{
    int tmp, cents_set;

    if ((number < 0) || (number > 999))
    {
        buffer[0] = '-';
        buffer[1] = '-';
        buffer[2] = '-';
        return;
    }

    tmp = number / 100;
    if (tmp == 0)
    {
        buffer[0] = ' ';
        cents_set = 0;
    }
    else
    {
        cents_set = 1;
        buffer[0] = tmp + '0';
    }
    number %= 100;

    tmp = number / 10;
    if ((tmp == 0) && (cents_set == 0))
        buffer[1] = ' ';
    else
        buffer[1] = tmp + '0';
    number %= 10;

    buffer[2] = number + '0';
}

/*converts 2-digit integers in the range of -99 to +99 to ascii,
with leading sign and appended ' ps'(not 0-terminated).
the buffer must have at least 6 bytes of space.*/
void Util_Pawns_To_String(char *buffer, int material)
{
    int tmp, buf_cnt;

    if ((material < -99) || (material > 99))
    {
        buffer[0] = ' ';
        buffer[1] = '-';
        buffer[2] = '-';
        buffer[3] = ' ';
        buffer[4] = 'p';
        buffer[5] = 's';
        return;
    }

    /*the sign*/
    if (material < 0)
    {
        buffer[0] = '-';
        material = -material;
    } else
        buffer[0] = '+';

    buf_cnt = 1;
    tmp = material / 10;
    if (tmp > 0)
        buffer[buf_cnt++] = tmp + '0';

    tmp = material % 10;

    buffer[buf_cnt++] = tmp + '0';
    buffer[buf_cnt++] = ' ';
    buffer[buf_cnt++] = 'p';
    buffer[buf_cnt  ] = 's';
}

/*converts the move and returns how many characters this move takes*/
int Util_Convert_Moves(MOVE m, char *buffer)
{
    if (m.u != MV_NO_MOVE_MASK)
    {
        buffer[0] = m.m.from%10 - 1 + 'a';
        buffer[1] = m.m.from/10 - 2 + '1';
        buffer[2] = m.m.to%10 - 1 + 'a';
        buffer[3] = m.m.to/10 - 2 + '1';

        switch(m.m.flag)
        {
        case WROOK  :
        case BROOK  :
            buffer[4] = WROOK_CHAR;
            buffer[5] = '\0';
            return(5);
        case WKNIGHT:
        case BKNIGHT:
            buffer[4] = WKNIGHT_CHAR;
            buffer[5] = '\0';
            return(5);
        case WBISHOP:
        case BBISHOP:
            buffer[4] = WBISHOP_CHAR;
            buffer[5] = '\0';
            return(5);
        case WQUEEN :
        case BQUEEN :
            buffer[4] = WQUEEN_CHAR;
            buffer[5] = '\0';
            return(5);
        default:
            buffer[4] = '\0';
            break;
        }
    } else
    {
        buffer[0] = buffer[1] = buffer[2] = buffer[3] = '-';
        buffer[4] = '\0';
    }
    return(4);
}

/*zeros out a memory buffer, no special alignment or size assumptions.*/
void Util_Memzero(void *ptr, size_t n_bytes)
{
    uint8_t  *byte_ptr;
    uint32_t *uint_ptr;
    size_t align_n;

    /*zero out until the first alignment*/
    byte_ptr = (uint8_t *) ptr;
    while ( (((uintptr_t)byte_ptr) & 0x00000003UL) && (n_bytes))
    {
        n_bytes--; /*this decrement must be in the loop since otherwise, calling Util_Memzero with n_bytes=0 would not work correctly.*/
        *byte_ptr++ = 0;
    }

    uint_ptr = (uint32_t *) byte_ptr;
    /*now zero out the aligned stuff.*/
    align_n = n_bytes >> 5; /*four bytes at a time, so integer division by four (shift by 2).
                        and eight-fold unrolled loop, so division by eight (shift by 3 - plus 2 is shift by 5).*/
    while (align_n--)
    {
        *uint_ptr++ = 0;
        *uint_ptr++ = 0;
        *uint_ptr++ = 0;
        *uint_ptr++ = 0;
        *uint_ptr++ = 0;
        *uint_ptr++ = 0;
        *uint_ptr++ = 0;
        *uint_ptr++ = 0;
    }

    /*some last 32bit work to do, i.e. the possible 32bit accesses left over from
    unrolling the loop above.*/
    align_n = (n_bytes >> 2) & 0x00000007UL;
    while (align_n--)
        *uint_ptr++ = 0;

    /*now do the rest bytewise*/
    n_bytes &= 0x00000003UL; /*that is what will be left over at the end, 0 to 3 bytes.*/
    byte_ptr = (uint8_t *) uint_ptr;
    while (n_bytes--)
        *byte_ptr++ = 0;
}

/*taking advantage of possible alignment if the pointers are 32bit-aligned.
since the rest of the program takes care of proper aligning, that's the normal case.

Note: this routine is 64bit compatible, but optimised for 32bit.*/
void Util_Memcpy(void *dest, const void *src, size_t n_bytes)
{
    /*optimise in case of 32bit pointer alignment*/
    if (( (((uintptr_t) dest) | ((uintptr_t) src)) & 0x00000003UL) == 0)
    {
        size_t n_words, unroll_words;
        uint32_t *uint_dst = (uint32_t *) dest;
        const uint32_t *uint_src = (const uint32_t *) src;
        n_words = n_bytes >> 2; /*division by four because we copy four bytes at a time*/

        /*unroll the loop eight times*/
        unroll_words = n_words >> 3;
        while (unroll_words--)
        {
            *uint_dst++ = *uint_src++;
            *uint_dst++ = *uint_src++;
            *uint_dst++ = *uint_src++;
            *uint_dst++ = *uint_src++;
            *uint_dst++ = *uint_src++;
            *uint_dst++ = *uint_src++;
            *uint_dst++ = *uint_src++;
            *uint_dst++ = *uint_src++;
        }

        /*finish the remainder of the loop unrolling*/
        n_words &= 0x00000007UL;
        while (n_words--)
            *uint_dst++ = *uint_src++;

        /*now there may be up to three bytes left from the word-copy.
        the copy length does not have to be word-wise just because
        the pointers are 32bit aligned - that may even be by luck.*/
        {
            uint8_t *byte_dst = (uint8_t *) uint_dst;
            const uint8_t *byte_src = (const uint8_t *) uint_src;

            n_bytes &= 0x00000003UL;
            while (n_bytes--)
                *byte_dst++ = *byte_src++;
        }
    } else /*byte-wise copy for byte-oriented structures*/
    {
        size_t unroll_bytes;
        uint8_t *byte_dst = (uint8_t *) dest;
        const uint8_t *byte_src = (const uint8_t *) src;

        /*unroll the loop eight times*/
        unroll_bytes = n_bytes >> 3;
        while (unroll_bytes--)
        {
            *byte_dst++ = *byte_src++;
            *byte_dst++ = *byte_src++;
            *byte_dst++ = *byte_src++;
            *byte_dst++ = *byte_src++;
            *byte_dst++ = *byte_src++;
            *byte_dst++ = *byte_src++;
            *byte_dst++ = *byte_src++;
            *byte_dst++ = *byte_src++;
        }

        /*finish the remainder of the loop unrolling*/
        n_bytes &= 0x00000007UL;
        while (n_bytes--)
            *byte_dst++ = *byte_src++;
    }
}

/*copies a move line. note that n is not the byte count, but the element count.*/
void Util_Movelinecpy(CMOVE *dest_ptr, const CMOVE *src_ptr, size_t n_moves)
{
    while (n_moves--)
        *dest_ptr++ = *src_ptr++;
}
