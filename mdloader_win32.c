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

#ifdef _MSC_VER
#pragma warning(disable:4996)
#endif

#define EXAMPLE_PORT "COM23"
#define PORT_SEARCH_STRING "COM"

HANDLE gport = NULL; //Port of device

#define WRITE_SIZE      500     //Maximum bytes to write per call
#define READ_SIZE       500     //Maximum bytes to read per call
#define READ_RETRIES    10      //Maximum read retries before fail

void print_com_example(void)
{
    printf("Example: -p " EXAMPLE_PORT "\n");
}

//Read data from device
//Must check read_error for a read error after return
//Return unsigned word from memory location
int read_data(int addr, int readsize)
{
    read_error = 1; //Set read error flag as default

    int readdata = 0;
    char wbuf[] = "!XXXXXXXX,#"; //Largest buffer needed
    DWORD ret;

    if (readsize == 1)
        sprintf(wbuf,"%c%02x,%c",CMD_READ_ADDR_8,addr,CMD_END);
    else if (readsize == 2)
        sprintf(wbuf,"%c%04x,%c",CMD_READ_ADDR_16,addr,CMD_END);
    else if (readsize == 4)
        sprintf(wbuf,"%c%08x,%c",CMD_READ_ADDR_32,addr,CMD_END);
    else
    {
        if (verbose) printf("Error: Invalid read size given (%i)\n",readsize);
        return 0;
    }

    if (verbose > 0) printf("Write: [%s]\n",wbuf);

    PurgeComm(gport, PURGE_RXCLEAR | PURGE_TXCLEAR); //Flush any remaining data from a bad read

    int writelen = strlen(wbuf);
    if (!WriteFile(gport,wbuf,writelen,&ret,0))
    {
        if (verbose) printf("Error writing port [%s](%lu)\n",wbuf,GetLastError());
        return 0;
    }

    if (ret != writelen)
    {
        if (verbose) printf("Error writing %i bytes [%lu]\n",writelen,ret);
        return 0;
    }

    if (!ReadFile(gport, &readdata, readsize, &ret, NULL))
    {
        if (verbose) printf("Error reading port [%i][%lu](%lu)\n",readsize,ret,GetLastError());
        return 0;
    }

    if (ret != readsize)
    {
        if (verbose) printf("Error reading %i bytes! [%lu]\n",readsize,ret);
        return 0;
    }

    read_error = 0; //Clear read error flag

    return readdata;
}

//Write data to device
//Return 1 on success, 0 on failure
int write_data(int addr, int writesize, int data)
{
    if (check_bootloader_write_attempt(addr)) return 0; //Prevent writes to bootloader section

    char wbuf[] = "!XXXXXXXX,XXXXXXXX#"; //Largest buffer needed
    DWORD ret;

    if (writesize == 1)
        sprintf(wbuf,"%c%08x,%02x%c",CMD_WRITE_ADDR_8,addr,data,CMD_END);
    else if (writesize == 2)
        sprintf(wbuf,"%c%08x,%04x%c",CMD_WRITE_ADDR_16,addr,data,CMD_END);
    else if (writesize == 4)
        sprintf(wbuf,"%c%08x,%08x%c",CMD_WRITE_ADDR_32,addr,data,CMD_END);
    else
    {
        if (verbose) printf("Error: Invalid write size given (%i)\n",writesize);
        return 0;
    }

    if (verbose) printf("Write %i bytes: [%s]\n",writesize,wbuf);

    int writelen = strlen(wbuf);
    if (!WriteFile(gport,wbuf,writelen,&ret,0))
    {
        if (verbose) printf("Error writing port [%s](%lu)\n",wbuf,GetLastError());
        return 0;
    }

    if (ret != writelen)
    {
        if (verbose) printf("Error writing %i bytes! [%lu]\n",writesize,ret);
        return 0;
    }

    return 1;
}

//Jump to address and run
//Return 1 on success, 0 on failure
int goto_address(int addr)
{
    char wbuf[] = "!XXXXXXXX#";
    DWORD ret;

    sprintf(wbuf,"%c%08x%c",CMD_GOTO_ADDR,addr,CMD_END);

    if (verbose) printf("Write: [%s]\n",wbuf);

    int writelen = strlen(wbuf);
    if (!WriteFile(gport,wbuf,strlen(wbuf),&ret,0))
    {
        if (verbose) printf("Error writing port [%s](%lu)\n",wbuf,GetLastError());
        return 0;
    }

    if (ret != writelen)
    {
        if (verbose) printf("Error writing goto address! [%lu]\n",ret);
        return 0;
    }

    return 1;
}

//Read data from device RAM
//Return pointer to buffer on success, NULL on failure
char *recv_file(int addr, int bytes)
{
    char wbuf[] = "!XXXXXXXX,XXXXXXXX#";
    char *data = NULL;

    data = (char *)calloc(bytes+1,sizeof(char));
    if (data == NULL)
    {
        printf("Error allocating read buffer memory!\n");
        return NULL;
    }

    char *pdata = data;
    DWORD ret;
    int retries = READ_RETRIES;
    int readsize = READ_SIZE;

    //Constrain read size to buffer size
    if (initparams.argument.outputInit.bufferSize > 0 && initparams.argument.outputInit.bufferSize < readsize)
        readsize = initparams.argument.outputInit.bufferSize;

    while (bytes > 0)
    {
        if (readsize > bytes) readsize = bytes;

        sprintf(wbuf,"%c%08x,%08x%c",CMD_RECV_FILE,addr,readsize,CMD_END);
        if (verbose > 0) printf("Write: [%s]\n",wbuf);

        PurgeComm(gport, PURGE_RXCLEAR | PURGE_TXCLEAR); //Flush any remaining data from a bad read

        long writelen = strlen(wbuf);
        if (!WriteFile(gport,wbuf,writelen,&ret,0))
        {
            if (verbose) printf("Error writing port [%s](%lu)\n",wbuf,GetLastError());
            free(data);
            return NULL;
        }

        if (!ReadFile(gport, pdata, bytes, &ret, NULL))
        {
            if (verbose) printf("Error reading port [%i][%lu](%lu)\n",readsize,ret,GetLastError());
            free(data);
            return NULL;
        }

        if (ret != readsize)
        {
            if (verbose) printf("Error reading %i bytes! [%lu](%lu)\n",readsize,ret,GetLastError());

            if (retries <= 0)
            {
                if (verbose) printf("No retries remain!\n");
                free(data);
                return NULL;
            }

            retries--;

            if (verbose) printf("Retrying read... (%i)\n",READ_RETRIES - retries);

            continue; //Attempt read again
        }

        if (verbose > 0) printf("Recv: [%lu]\n",ret);

        retries = READ_RETRIES; //Reset retry limit

        bytes -= ret;  //Decrement remaining bytes
        addr += ret;   //Increment to next address
        pdata += ret;  //Increment pointer in recv buffer
    }

    return data;
}

//Write data to device
//Return 1 on sucess, 0 on failure
int send_file(int addr, int bytes, char *data)
{
    if (check_bootloader_write_attempt(addr)) return 0; //Prevent writes to bootloader section

    if (bytes < 1)
    {
        printf("Error: No data to send!\n");
        return 0;
    }

    char wbuf[] = "!XXXXXXXX,XXXXXXXX#";
    DWORD ret;

    char *pdata = data;
    int writesize = WRITE_SIZE;

    //Constrain write size to buffer size
    if (initparams.argument.outputInit.bufferSize > 0 && initparams.argument.outputInit.bufferSize < writesize)
        writesize = initparams.argument.outputInit.bufferSize;

    while (bytes > 0)
    {
        if (writesize > bytes) writesize = bytes;

        sprintf(wbuf,"%c%08x,%08x%c",CMD_SEND_FILE,addr,writesize,CMD_END);
        if (verbose > 0) printf("Write: [%s]\n",wbuf);

        if (!WriteFile(gport, wbuf, strlen(wbuf), &ret, 0))
        {
            if (verbose) printf("Error writing port [%s](%lu)\n",wbuf,GetLastError());
            return 0;
        }

        if (ret != strlen(wbuf))
        {
            if (verbose) printf("Error writing port [%s](%lu)\n",wbuf,GetLastError());
            return 0;
        }

        if (verbose > 0) printf("Write: %i bytes\n",writesize);
        if (!WriteFile(gport, pdata, writesize, &ret, 0))
        {
            if (verbose) printf("Error writing port [%lu][%i](%lu)\n",ret,writesize,GetLastError());
            return 0;
        }

        if (ret != writesize)
        {
            printf("Error writing port [%lu][%i](%lu)\n",ret,writesize,GetLastError());
            return 0;
        }

        bytes -= ret;  //Decrement remaining bytes
        addr += ret;   //Increment to next address
        pdata += ret;  //Increment pointer in send buffer
    }

    return 1;
}

//Print bootloader version
//Return 1 on sucess, 0 on failure
int print_bootloader_version(void)
{
    char wbuf[] = "!#";
    char readdata[128] = "";
    DWORD ret;
    int readsize = sizeof(readdata) - 1;

    sprintf(wbuf,"%c%c",CMD_READ_VERSION,CMD_END);

    if (verbose > 0) printf("Write: [%s]\n",wbuf);

    int writelen = strlen(wbuf);
    if (!WriteFile(gport,wbuf,writelen,&ret,0))
    {
        if (verbose) printf("Version: Error writing port [%s](%lu)\n",wbuf,GetLastError());
        else printf("Version: Error retrieving!\n");
        return 0;
    }

    if (!ReadFile(gport, &readdata, readsize, &ret, NULL))
    {
        if (verbose) printf("Version: Error reading port [%i][%lu](%lu)\n",readsize,ret,GetLastError());
        else printf("Version: Error retrieving!\n");
        return 0;
    }

    while (readdata[strlen(readdata)-1] == '\n' || readdata[strlen(readdata)-1] == '\r') readdata[strlen(readdata)-1] = 0;

    printf("Bootloader version: %s\n",readdata);

    return 1;
}

//Set normal command mode
//Return 1 on sucess, 0 on failure
int set_normal_mode(void)
{
    if (verbose) printf("Setting normal mode... ");

    int retbuf = 0;
    char wbuf[] = "!#";
    DWORD ret;

    sprintf(wbuf,"%c%c",CMD_SET_NORMAL_MODE,CMD_END);

    int writelen = strlen(wbuf);
    if (!WriteFile(gport,wbuf,writelen,&ret,0))
    {
        if (verbose) printf("Failed! (%lu)\n",GetLastError());
        return 0;
    }

    if (ret != writelen)
    {
        if (verbose) printf("Error writing %i bytes! [%lu](%lu)\n",writelen,ret,GetLastError());
        return 0;
    }

    int readlen = 2;
    if (!ReadFile(gport, &retbuf, readlen, &ret, NULL))
    {
        if (verbose) printf("Error reading port [%i][%lu](%lu)\n",readlen,ret,GetLastError());
        return 0;
    }

    if (ret != readlen)
    {
        if (verbose) printf("Error reading %i bytes! [%lu][%04x](%lu)\n",readlen,ret,retbuf,GetLastError());
        return 0;
    }

    if ((retbuf & 0xFFFF) != 0x0D0A)
    {
        if (verbose) printf("Error: Incorrect response! [%lu][%04x](%lu)\n",ret,retbuf,GetLastError());
        return 0;
    }

    if (verbose) printf("Success!\n");

    return 1;
}

//Jump to loaded application
//Return 1 on sucess, 0 on failure
int jump_application(void)
{
    printf("Booting device... ");

    if (testmode)
    {
        printf("(test mode disables restart)\n");
        return 1;
    }

    char wbuf[] = "!#";
    DWORD ret;
    DWORD writelen = strlen(wbuf);
    sprintf(wbuf,"%c%c",CMD_LOAD_APP,CMD_END);
    if (!WriteFile(gport,wbuf,writelen,&ret,0))
    {
        printf("Failed! (%lu)\n",GetLastError());
        return 0;
    }

    printf("Success!\n");
    return 1;
}

//Open port
//Return 1 on sucess, 0 on failure
int open_port(char *portname, char silent)
{
    char portnamebuf[64] = "";

    if (!silent || verbose) printf("Opening port '%s'... ",portname);

    sprintf(portnamebuf,"\\\\.\\%s",portname);
    gport = CreateFile(portnamebuf,GENERIC_READ|GENERIC_WRITE,0,NULL,OPEN_EXISTING,0,NULL);

    if (gport == INVALID_HANDLE_VALUE)
    {
        if (!silent || verbose)
        {
            printf("Failed!");

            LPTSTR errstr = NULL;

            FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|
                        FORMAT_MESSAGE_IGNORE_INSERTS|FORMAT_MESSAGE_MAX_WIDTH_MASK,
                        NULL,
                        GetLastError(),
                        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                        (LPTSTR)&errstr,
                        0,
                        NULL);

            if (errstr != NULL)
            {
                char *pstr = errstr + strlen(errstr);
                while (pstr > errstr && *pstr <= 32) { *pstr = 0; pstr--; } //Clean up end of message
                if (!silent) printf(" (%s)",errstr);
                LocalFree(errstr);
            }

            printf("\n");
        }

        return 0;
    }

    if (!silent || verbose) printf("Success!\n");
    return 1;
}

//Close port
//Return 1 on sucess, 0 on failure
int close_port(char silent)
{
    if (!silent || verbose) printf("Closing port... ");
    if (CloseHandle(gport) == 0)
    {
        if (!silent || verbose) printf("Failed! (%lu)\n",GetLastError());
        return 0;
    }
    if (!silent || verbose) printf("Success!\n");
    return 1;
}

//Configure port
//Return 1 on sucess, 0 on failure
int config_port(void)
{
    DCB dcb;
    memset(&dcb, 0, sizeof(DCB));

    if (verbose) printf("Configuring port... \n");

    if (verbose) printf("  Get config... ");
    if (GetCommState(gport,&dcb) == 0)
    {
        if (verbose) printf("Failed! (%lu)\n",GetLastError());
        return 0;
    }
    if (verbose) printf("Success!\n");

    dcb.BaudRate = CBR_115200;
    dcb.StopBits = ONESTOPBIT;
    dcb.Parity = NOPARITY;
    dcb.ByteSize = 8;

    if (verbose) printf("  Set config... ");
    if (!SetCommState(gport,&dcb))
    {
        if (verbose) printf("Failed! (%lu)\n",GetLastError());
        return 0;
    }
    if (verbose) printf("Success!\n");


    COMMTIMEOUTS comTimeOut;
    comTimeOut.ReadIntervalTimeout = 1;
    comTimeOut.ReadTotalTimeoutMultiplier = 1;
    comTimeOut.ReadTotalTimeoutConstant = 1;
    comTimeOut.WriteTotalTimeoutMultiplier = 1;
    comTimeOut.WriteTotalTimeoutConstant = 1;

    if (verbose) printf("  Set timeouts... ");
    if (!SetCommTimeouts(gport,&comTimeOut))
    {
        if (verbose) printf("Failed! (%lu)\n",GetLastError());
        return 0;
    }
    if (verbose) printf("Success!\n");

    PurgeComm(gport, PURGE_RXCLEAR | PURGE_TXCLEAR); //Flush port
    return 1;
}

//List devices which communicate properly
//If first_device is not null, store first found device and return
void list_devices(char *first_device)
{
    int portnum = 1;
    int portmax = 255; //Inclusive
    int portcount = 0;
    char portname[] = "COM255"; //Max buffer necessary

    if (first_device == NULL)
    {
        printf("Bootloader port listing\n");
        printf("-----------------------------\n");
    }

    while (portnum <= portmax)
    {
        sprintf(portname,PORT_SEARCH_STRING "%i",portnum);
        if (test_port(portname,TRUE))
        {
            if (test_mcu(TRUE))
            {
                if (first_device) printf("\n");
                printf("Device port: %s (%s)\n",portname,mcu->name);
                if (first_device != NULL)
                {
                    close_port(TRUE);
                    strcpy(first_device,portname);
                    return;
                }
                portcount++;
            }
            close_port(TRUE);
        }
        portnum++;
    }

    if (first_device == NULL)
    {
        if (portcount == 0)
            printf("No devices found!\n");
    }
}

