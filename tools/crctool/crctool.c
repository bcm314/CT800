/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 *  Copyright (C) 2016-2019, Rasmus Althoff <althoff@ct800.net>
 *
 *  This file is part of CT800 (CRC firmware tool).
 *
 *  CT800 is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  any later version.
 *
 *  CT800 is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with CT800. If not, see <http://www.gnu.org/licenses/>.
 *
*/

/* this tool calculates a CRC32 over the firmware file.

If you give the inputfile "example.bin",
the program will generate "example_crc.bin" and "example_crc.hex".

example usage:

./crctool ct800fw.bin 0x08000000
=> result: ct800fw_crc.bin (384k) and ct800fw_crc.hex (384k, starting at 0x08000000)

The start address can be given as 0x1234 or just 1234 (still in hexadecimal). Leading
zeros can be omitted.

The binary input file must not be bigger than 384k. It can be smaller, the rest
is automatically filled up with 0xff. The last four bytes contain the CRC32, most significant
byte first. The byte order is written byte by byte to avoid endianess issues.

In fact, there are enough make-crc-tools out there, but to dig into them to generate exactly
what's needed here would have taken more time than just code the already implemented CRC
running over a binary buffer and then spit out a hex file.

Despite, the whole project should be as self-contained as possible.
*/

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*BIN_FILE_SIZE must be a multiple of 64k*/
#define BIN_FILE_SIZE (384UL * 1024UL)

static uint8_t bin_file_buffer[BIN_FILE_SIZE];

/*CRC algorithm with 8 bits per iteration: invented by Dilip V. Sarwate in 1988.*/
static const uint32_t Crc32Table[256] = {
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

/*CRC table generator, just for info:
#define POLYNOMIAL  0xEDB88320UL
uint32_t crc, i, j;
for (i = 0; i <= 0xFFUL; i++)
{
    crc = i;
    for (j = 0; j < 8; j++)
        crc = (crc >> 1) ^ (-int32_t(crc & 1) & POLYNOMIAL);
    Crc32Table[i] = crc;
}
*/

/*CRC algorithm with 8 bits per iteration: invented by Dilip V. Sarwate in 1988.*/
static uint32_t Ct_Crc32(const void *buffer, size_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    const uint8_t* databyte = (const uint8_t*) buffer;

    while (len--)
    {
        uint32_t lookup = crc;
        lookup &= 0xFFUL;
        lookup ^= *databyte++;
        crc >>= 8;
        crc ^= Crc32Table[lookup];
    }

    return(~crc);
}

/*each 64k block starts with an extended address.*/
static void Get_Extended_Address(char *address_record, uint32_t address)
{
    char buffer[12];
    uint8_t byte;
    uint8_t chksum = 6U;
    strcpy(address_record, ":02000004"); /*record type*/

    byte = (address >> 24 ) & 0xFFUL;
    chksum += byte;
    sprintf(buffer, "%02X", byte);
    strcat(address_record, buffer);

    byte = (address >> 16 ) & 0xFFUL;
    chksum += byte;
    sprintf(buffer, "%02X", byte);
    strcat(address_record, buffer);

    chksum = ~chksum;
    chksum++;

    sprintf(buffer, "%02X\r\n", chksum);
    strcat(address_record, buffer);
}

/*end of file.*/
static void Get_End_Record(char *end_record)
{
    strcpy(end_record, ":00000001FF\r\n");
}

/*one line of regular data, 16 bytes per line.*/
static void Get_Data_Record_16(char *data_record, const uint8_t *inputbuffer, uint32_t address)
{
    char buffer[12];
    uint8_t byte, i;
    uint8_t chksum = 16U;
    strcpy(data_record, ":10"); /*record length*/

    /*16 bit address within a 64 block*/
    byte = (address >> 8) & 0xFFUL;
    chksum += byte;
    sprintf(buffer, "%02X", byte);
    buffer[2] = '\0';
    strcat(data_record, buffer);
    byte = (address     ) & 0xFFUL;
    chksum += byte;
    sprintf(buffer, "%02X", byte);
    buffer[2] = '\0';
    strcat(data_record, buffer);

    strcat(data_record, "00"); /*record type*/

    for (i = 0; i < 16; i++)
    {
        byte = inputbuffer[i];
        chksum += byte;
        sprintf(buffer, "%02X", byte);
        buffer[2] = '\0';
        strcat(data_record, buffer);
    }

    chksum = ~chksum;
    chksum++;

    sprintf(buffer, "%02X\r\n", chksum);
    strcat(data_record, buffer);
}


int main(int argc, char* argv[])
{
    FILE *bin_file;
    size_t bytes_read, bytes_written;
    uint32_t crc32, addr, segment, blocks, start_addr_param, i;
    int ret; /*for error checking with file operations*/
    uint8_t crc_byte;
    char line[64];
    char filename[524];
    char ch;
    
    fprintf(stdout, "\r\nCT800 CRC tool V1.10\r\n\r\n");

    if (argc < 3)
    {
        fprintf(stderr, "ERROR. usage: crctool binfile hex-start-address\r\n");
        fprintf(stderr, "example: crctool my_bin_file.bin 0x08000000\r\n");
        return(-1);
    }

    strncpy(filename, argv[1], 510);
    filename[510] = '\0';
    i = strlen(filename);
    if (i > 0)
    {
        /*cut off a potential CR/LF at the end
        - that can happen if called from within a shell script.*/
        if ((filename[i-1] == '\r') || (filename[i-1] == '\n'))
        {
            filename[i-1] = '\0';
            i--;
            if (i > 0)
                if ((filename[i-1] == '\r') || (filename[i-1] == '\n'))
                    filename[i-1] = '\0';
        }
    }

    strncpy(line, argv[2], 20);
    line[20] = '\0';
    i = 0;
    
    /*if the hexadecimal start address is given as 0x12345678,
    ignore the 0x part.	but it might also be given as 56789ABC.
    or with less than eight characters, omitting leading zeros.*/
    if ((line[1]=='x') || (line[1]=='X'))
        i += 2;
    start_addr_param = 0;
    ch = line[i];
    if ((ch >= 'a') && (ch <= 'f'))
        ch += 'A' - 'a';
    while (((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F')))
    {
        start_addr_param <<= 4;
        if ((ch >= '0') && (ch <= '9'))
            start_addr_param += ch - '0';
        else
            start_addr_param += ch - 'A';
        i++;
        ch = line[i];
        if ((ch >= 'a') && (ch <= 'f'))
            ch += 'A' - 'a';
    }

    filename[510] = '\0';
    bin_file = fopen(filename, "rb");
    if (bin_file == NULL)
    {
        fprintf(stderr, "ERROR: file %s not found.\r\n", filename);
        return(-2);
    }

    bytes_read = fread(bin_file_buffer, sizeof(uint8_t), BIN_FILE_SIZE, bin_file);
    /*close the input file*/
    fclose(bin_file);

    if (bytes_read < BIN_FILE_SIZE)
    {
        for (addr = bytes_read; addr < BIN_FILE_SIZE; addr++)
            bin_file_buffer[addr] = 0xFFU;
    }
    
    fprintf(stdout, "INFO: %"PRIu32"k read.\r\n", (uint32_t) (bytes_read/1024UL));
    /*be clear to the user what the actual start address is.*/
    fprintf(stdout, "INFO: start address: 0x%08X\r\n", start_addr_param);

    /*last four bytes are for storing the CRC*/
    crc32 = Ct_Crc32(bin_file_buffer, BIN_FILE_SIZE - sizeof(uint32_t));

    fprintf(stdout, "INFO: CRC is (hex): ");

    crc_byte = (crc32 & 0xFF000000UL) >> 24;
    fprintf(stdout, "%02X ", crc_byte);
    bin_file_buffer[BIN_FILE_SIZE - 4] = crc_byte;

    crc_byte = (crc32 & 0x00FF0000UL) >> 16;
    fprintf(stdout, "%02X ", crc_byte);
    bin_file_buffer[BIN_FILE_SIZE - 3] = crc_byte;

    crc_byte = (crc32 & 0x0000FF00UL) >> 8;
    fprintf(stdout, "%02X ", crc_byte);
    bin_file_buffer[BIN_FILE_SIZE - 2] = crc_byte;

    crc_byte = (crc32 & 0x000000FFUL);
    fprintf(stdout, "%02X\r\n", crc_byte);
    bin_file_buffer[BIN_FILE_SIZE - 1] = crc_byte;


    /*generate a filename for the output file.
    if "example.bin" has been the input file name , then "example_crc.bin" will be
    the binary output file name.*/
    i = strlen(filename);
    if (i > 4)
        if (filename[i-4] == '.')
            filename[i-4] = '\0';
    strcat(filename, "_crc.bin");

    /*that's the binary output file*/
    bin_file = fopen(filename, "wb");
    if (bin_file == NULL)
    {
        fprintf(stderr, "ERROR: file %s not writable.\r\n", filename);
        return(-3);
    }

    /*write the binary output file to disk*/
    bytes_written = fwrite(bin_file_buffer, sizeof(uint8_t), BIN_FILE_SIZE, bin_file);
    fclose(bin_file);

    if (bytes_written != BIN_FILE_SIZE)
    {
        fprintf(stderr, "ERROR: file %s not writable.\r\n", filename);
        /*try to delete it, it is corrupted anyway.*/
        if (remove(filename) == 0)
            return(-4);
        else
            return(-5);
    }

    /*generate a filename for the hex format output file.
    if "example.bin" has been the input file name , then "example_crc.hex" will be
    the hex format output file name.*/
    i = strlen(filename);
    if (i > 4)
        if (filename[i-4] == '.')
            filename[i-4] = '\0';

    /*put out the hex format*/
    strcat(filename, ".hex");

    /*that's the hex output file*/
    bin_file = fopen(filename, "wb");
    if (bin_file == NULL)
    {
        fprintf(stderr, "ERROR: file %s not writable.\r\n", filename);
        return(-6);
    }

    addr = 0; /*reading the binary buffer*/
    segment = start_addr_param; /*the offset of the binary file*/

    for (blocks = 0; blocks < (BIN_FILE_SIZE/0x10000UL); blocks++, segment += 0x10000UL) /*64k blocks*/
    {
        Get_Extended_Address(line, segment);
        ret = fputs(line, bin_file);
        if (ret < 0) goto hex_file_end;
        /*there are 4096 lines of 16 bytes within a 64k block.*/
        for (i = 0; i < 4096UL; i++, addr += 16UL)
        {
            Get_Data_Record_16(line, &bin_file_buffer[addr], addr);
            ret = fputs(line, bin_file);
            if (ret < 0) goto hex_file_end; /*error handling if write fails*/
        }
    }

    /*end record*/
    Get_End_Record(line);
    ret = fputs(line, bin_file);

hex_file_end:
    fclose(bin_file);
    if (ret < 0)
    {
        /*writing the hex file failed.*/
        fprintf(stderr, "ERROR: file %s not writable.\r\n", filename);
        
        /*try to delete it, it is corrupted anyway.*/
        if (remove(filename) == 0)
            return(-7);
        else
            return(-8);
    }

    fprintf(stdout, "SUCCESS: %"PRIu32"k written.\r\n", (uint32_t)(BIN_FILE_SIZE/1024UL));
    return(0);
}
