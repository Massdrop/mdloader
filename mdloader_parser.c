/*
 *  Copyright (C) 2018  Massdrop Inc.
 *
 *  This file is part of Massdrop Loader.
 *
 *  Massdrop Loader is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Massdrop Loader is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Massdrop Loader.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "mdloader_common.h"
#include "mdloader_parser.h"

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

void free_data(data_t *data)
{
    if (!data)
    {
        printf("Error: Parser: Attempt to free NULL data!\n");
        return;
    }

    if (data->data)
    {
        free(data->data);
        data->data = NULL;
    }

    free(data);
    data = NULL;
}

data_t *create_data(uint32_t data_length)
{
    data_t *data = (data_t *)malloc(sizeof(data_t));
    if (!data)
    {
        printf("ERROR: Parser: Could not allocate parser memory!\n");
        return NULL;
    }

    data->data = (char *)malloc(data_length);
    if (!data->data)
    {
        printf("ERROR: Parser: Could not allocate parser data memory!\n");
        free_data(data);
        return NULL;
    }

    return data;
}

//Assuming validity checks on incoming variables have been completed prior to call
data_t *parse_bin(char *rawdata, uint32_t rawlength)
{
    if (!rawdata)
    {
        printf("ERROR: Parser: Bin: Raw data null!\n");
        return NULL;
    }

    if (!rawlength)
    {
        printf("ERROR: Parser: Bin: Raw data length zero!\n");
        return NULL;
    }

    data_t *data = create_data(rawlength);

    if (data)
    {
        memcpy(data->data, rawdata, rawlength);
        data->size = rawlength;
        data->addr = mcu->flash_addr + bootloader_length;
    }
    else
        printf("Error: Parser: Bin: Error creating parser storage!\n");

    return data;
}

char hex_conv_error; //Cleared on conversion attempt and set to 1 if read fails

//Must check hex_conv_error != 0 for conversion error after call
char ascii_to_hex(char bh, char bl)
{
    hex_conv_error = 0;

    if (bh >= '0' && bh <= '9') bh -= '0';
    else if (bh >= 'A' && bh <= 'F') bh -= 'A' - 0xA;
    else if (bh >= 'a' && bh <= 'a') bh -= 'a' - 0xA;
    else
    {
        hex_conv_error = 1;
        return 0;
    }

    if (bl >= '0' && bl <= '9') bl -= '0';
    else if (bl >= 'A' && bl <= 'F') bl -= 'A' - 0xA;
    else if (bl >= 'a' && bl <= 'a') bl -= 'a' - 0xA;
    else
    {
        hex_conv_error = 1;
        return 0;
    }

    return (bh << 4) | bl;
}

//Assuming validity checks on incoming variables have been completed prior to call
data_t *parse_hex(char *rawdata, uint32_t rawlength)
{
    if (!rawdata)
    {
        printf("ERROR: Parser: Hex: Raw data null!\n");
        return NULL;
    }

    if (!rawlength)
    {
        printf("ERROR: Parser: Hex: Raw data length zero!\n");
        return NULL;
    }

    uint8_t first_address_set = 0;
    uint32_t base_address = 0;
    uint32_t start_offset;
    uint8_t *rds = (uint8_t *)rawdata;
    uint8_t *rde = (uint8_t *)rawdata + rawlength;
    uint8_t *bindata = (uint8_t *)rawdata;
    hex_record_t *hex;
    uint8_t *hex_data;
    uint8_t checksum;
    uint8_t checksum_given;
    uint16_t line = 0;
    uint32_t binlength = 0;
    uint8_t byte_count;
    uint32_t next_address = 0;
    uint32_t start_segment_address = 0;
    uint8_t start_segment_address_set = 0;
    while (rds < rde)
    {
        line++;
        if (rde - rds < sizeof(hex_record_t))
        {
            printf("Error: Parser: Hex: Unexpected end of header! (Line %i)\n",line);
            return NULL;
        }

        hex = (hex_record_t *)rds;
        if (hex->start_code != ':')
        {
            printf("Error: Parser: Hex: Invalid start code! (Line %i)\n",line);
            return NULL;
        }

        checksum = 0;

        //Convert byte count to safe storage
        byte_count = ascii_to_hex(*hex->byte_count, *(hex->byte_count+1));
        if (hex_conv_error)
        {
            printf("Error: Parser: Hex: Unexpected ASCII in byte count! (Line %i)\n",line);
            return NULL;
        }
        checksum += byte_count;

        //Convert record type in place
        *hex->record_type = ascii_to_hex(*hex->record_type, *(hex->record_type+1));
        if (hex_conv_error)
        {
            printf("Error: Parser: Hex: Unexpected ASCII in record type! (Line %i)\n",line);
            return NULL;
        }
        checksum += *hex->record_type;

        //Convert address in place
        *hex->address.c = ascii_to_hex(*(hex->address.c+0), *(hex->address.c+1));
        if (hex_conv_error)
        {
            printf("Error: Parser: Hex: Unexpected ASCII in address byte 1! (Line %i)\n",line);
            return NULL;
        }
        checksum += *hex->address.c;
        *(hex->address.c+1) = ascii_to_hex(*(hex->address.c+2), *(hex->address.c+3));
        if (hex_conv_error)
        {
            printf("Error: Parser: Hex: Unexpected ASCII in address byte 2! (Line %i)\n",line);
            return NULL;
        }
        checksum += *(hex->address.c+1);
        hex->address.i = (*hex->address.c << 8) + *(hex->address.c+1);

        //Check enough data remains to parse
        if (rde - rds < sizeof(hex_record_t) + (byte_count * 2) + 2)
        {
            printf("Error: Parser: Hex: Malformed data! (Line %i)\n",line);
            return NULL;
        }

        hex_data = rds + sizeof(hex_record_t);

        //Convert data in place
        for (start_offset = 0; start_offset < byte_count * 2; start_offset += 2)
        {
            *(hex_data + start_offset / 2) = ascii_to_hex(*(hex_data + start_offset), *(hex_data + start_offset + 1));
            if (hex_conv_error)
            {
                printf("Error: Parser: Hex: Unexpected ASCII in data byte! (Line %i)\n",line);
                return NULL;
            }
            checksum += *(hex_data + start_offset / 2);
        }

        checksum *= -1;

        //Convert checksum
        checksum_given = ascii_to_hex(*(hex_data + start_offset), *(hex_data + start_offset + 1));
        if (hex_conv_error)
        {
            printf("Error: Parser: Hex: Unexpected ASCII in checksum byte! (Line %i)\n",line);
            return NULL;
        }
        if (checksum != checksum_given)
        {
            printf("Error: Parser: Hex: Checksum mismatch! (Line %i)\n",line);
            return NULL;
        }

        //Print hex line
        //printf("%c ",hex->start_code);
        //printf("%02X ",byte_count);
        //printf("%04X ",hex->address.i);
        //for (start_offset = 0; start_offset < byte_count; start_offset++)
        //    printf("%02X",*(hex_data + start_offset));
        //if (byte_count) printf(" ");
        //printf("%02X\n",checksum);

        if (*hex->record_type == 0) //Data
        {
            if (!first_address_set)
            {
                first_address_set = 1;
                next_address = base_address + hex->address.i;
            }

            if (hex->address.i + base_address != next_address)
            {
                printf("Error: Parser: Hex: Address not contiguous! (Line %i)\n",line);
                return NULL;
            }

            //Write binary data into rawdata buffer (write position will never reach read position)
            binlength += byte_count;
            for (start_offset = 0; start_offset < byte_count; start_offset++)
            {
                *bindata = *(hex_data + start_offset);
                bindata++;
            }

            next_address = base_address + ((next_address + byte_count) & 0xFFFF);
        }
        else if (*hex->record_type == 1) //EOF
        {
            break; //No need to go further
        }
        else if (*hex->record_type == 2) //Extended Segment Address
        {
            base_address = ((*hex_data << 8) + *(hex_data+1)) << 16;
            next_address += base_address;
        }
        else if (*hex->record_type == 3) //Start Segment Address
        {
            start_segment_address = ((*hex_data << 24) + (*(hex_data+1) << 16) + (*(hex_data+2) << 8) + (*(hex_data+3)));
            start_segment_address_set = 1;
        }
        else if (*hex->record_type == 4) //Extended Linear Address
        {
            printf("Error: Parser: Hex: 32-bit addressing is not supported! (Line %i)\n",line);
            return NULL;
        }
        else if (*hex->record_type == 5) //Start Linear Address
        {
            printf("Error: Parser: Hex: Start linear address is not supported! (Line %i)\n",line);
            return NULL;
        }
        else
        {
            printf("Error: Parser: Hex: Unknown record type! (Line %i)\n",line);
            return NULL;
        }

        rds += sizeof(hex_record_t) + (byte_count * 2) + 2; //Bypass header, data, checksum

        while (rds < rde && (*rds == '\r' || *rds == '\n')) rds++; //Bypass EOL characters
    }

    if (!start_segment_address_set)
    {
        printf("Error: Parser: Hex: Missing start segment address!\n");
        return NULL;
    }

    data_t *data = create_data(binlength);

    if (data)
    {
        memcpy(data->data, rawdata, binlength);
        data->size = binlength;
        data->addr = start_segment_address;
    }
    else
        printf("Error: Parser: Hex: Error creating parser storage!\n");

    return data;
}

char get_type_by_ext(char *fname)
{
    char *pext = fname + (strlen(fname) - 4);

    if (!strcmp(pext,EXT_HEX)) return FTYPE_HEX;
    else if (!strcmp(pext,EXT_BIN)) return FTYPE_BIN;
    else return FTYPE_NONE;
}

data_t *load_file(char *fname)
{
    if (!fname)
    {
        printf("ERROR: Parser: No file given!\n");
        return NULL;
    }

    if (strlen(fname) < 5)
    {
        printf("ERROR: Parser: File name must end in %s or %s!\n", EXT_HEX, EXT_BIN);
        return NULL;
    }

    char ftype = get_type_by_ext(fname);
    if (ftype == FTYPE_NONE)
    {
        printf("ERROR: Parser: File name must end in %s or %s!\n", EXT_HEX, EXT_BIN);
        return NULL;
    }

    uint32_t fsize = filesize(fname);
    if (fsize == 0)
    {
        printf("ERROR: Parser: File is empty!\n");
        return NULL;
    }

    FILE *fIn = fopen(fname, "rb");
    if (!fIn)
    {
        printf("ERROR: Parser: Could not open file for read!\n");
        return NULL;
    }

    char *rawdata = (char *)malloc(fsize);
    if (!rawdata)
    {
        printf("ERROR: Parser: Could no allocated file memory buffer!\n");
        fclose(fIn);
        return NULL;
    }

    uint32_t readbytes = (uint32_t)fread(rawdata, 1, fsize, fIn);

    fclose(fIn);

    if (readbytes != fsize)
    {
        printf("ERROR: Parser: File read size mismatch!\n");
        free(rawdata);
        return NULL;
    }

    if (ftype == FTYPE_HEX)
        return parse_hex(rawdata, readbytes);
    else if (ftype == FTYPE_BIN)
        return parse_bin(rawdata, readbytes);
    else
        printf("ERROR: Parser: Unknown file type!\n");

    return NULL;
}
