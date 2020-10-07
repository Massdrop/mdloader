/*
 *  Copyright (C) 2018-2020 Massdrop Inc.
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

char verbose;
char testmode;
char first_device;
int restart_after_program;
int hex_cols;
int hex_colw;

//SAM-BA Settings
mailbox_t initparams;
mailbox_t appletinfo;   //Applet information reported by binary
appinfo_t appinfo;      //Applet application information from end of applet binary

mcu_t mcus[] = {
      //Name,       Chip ID     Chip ID,    Program Memory, Data Memory,    Program Addr,   Data Addr
      //            Address                 (FLASH_SIZE)    (HSRAM_SIZE)    (FLASH_ADDR)    (HSRAM_ADDR)
{"SAME54P19A", 0x41002018, 0x61840001, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME54P20A", 0x41002018, 0x61840000, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME54N19A", 0x41002018, 0x61840003, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME54N20A", 0x41002018, 0x61840002, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME53N20A", 0x41002018, 0x61830002, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME53N19A", 0x41002018, 0x61830003, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME53J18A", 0x41002018, 0x61830006, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME53J19A", 0x41002018, 0x61830005, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME53J20A", 0x41002018, 0x61830004, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME51N19A", 0x41002018, 0x61810001, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME51N20A", 0x41002018, 0x61810000, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME51J18A", 0x41002018, 0x61810003, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME51J19A", 0x41002018, 0x61810002, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME51J20A", 0x41002018, 0x61810004, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51P20A", 0x41002018, 0x60060000, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51P19A", 0x41002018, 0x60060001, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51N19A", 0x41002018, 0x60060003, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51N20A", 0x41002018, 0x60060002, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51J18A", 0x41002018, 0x60060006, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51J19A", 0x41002018, 0x60060005, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51J20A", 0x41002018, 0x60060004, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51G18A", 0x41002018, 0x60060008, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAMD51G19A", 0x41002018, 0x60060007, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME51G18A", 0x41002018, 0x61810306, 0x40000,        0x20000,        0x00000000,     0x20000000},
{"SAME51G19A", 0x41002018, 0x61810305, 0x40000,        0x20000,        0x00000000,     0x20000000},

};

mcu_t *mcu; //Pointer to mcus entry if found
uint32_t bootloader_length;

//Guard against bootloader area writing
//Return 1 if attempt to write to bootloader area
//Return 0 otherwise
//The applet also performs this check
int check_bootloader_write_attempt(int addr)
{
    if (addr < mcu->flash_addr + bootloader_length)
    {
        printf("Attempt to write to bootloader section denied!\n");
        return 1;
    }

    return 0;
}

int read_error; //Cleared on read attempt and set to 1 if read fails

//Print bootloader's serial number if programmed in
//Return 1 on sucess, 0 on failure
int print_bootloader_serial(void)
{
    int seroffset = read_word(bootloader_length - 4);

    if (read_error)
    {
        printf("Serial Number: Read error!\n");
        return 0;
    }

    if (verbose > 0) printf("Serial Number offset: 0x%08x\n", seroffset);

    if (seroffset >= mcu->flash_addr + bootloader_length)
    {
        printf("Serial Number: Not programmed!\n");
        return 0;
    }

    char *readbuf = recv_file(seroffset, SERIAL_MAX_LENGTH * 2);

    if (!readbuf)
    {
        printf("Serial Number: Error retrieving!\n");
        return 0;
    }

    printf("Serial Number: ");
    int ind;
    for (ind = 0; ind < SERIAL_MAX_LENGTH * 2; ind += 2)
        printf("%c", readbuf[ind]);
    printf("\n");

    free(readbuf); //Free buffer created in recv_file

    return 1;
}

//Write mailbox data
//Return 1 on sucess, 0 on failure
int send_mail(mailbox_t *mail)
{
    int byte;
    int *pmail = (int *)mail;
    if (verbose) printf("Sending mail {\n");
    for (byte = 0; byte < sizeof(mailbox_t) / sizeof(int); byte++)
    {
        if (verbose) printf("  ");
        write_word(appinfo.mail_addr+byte*sizeof(int), *pmail);
        pmail++;
    }
    if (verbose) printf("}\n");

    return 1;
}

//Read mailbox data
//Return 1 on sucess, 0 on failure
int read_mail(mailbox_t *mail)
{
    int byte;
    int *pmail = (int *)mail;
    if (verbose) printf("Retrieving mail {\n");
    for (byte = 0; byte < sizeof(mailbox_t) / sizeof(int); byte++)
    {
        if (verbose) printf("  ");
        *pmail = read_word(appinfo.mail_addr+byte*sizeof(int));
        pmail++;
    }
    if (verbose) printf("}\n");

    return 1;
}

//Print mailbox data
//Return 1 on sucess, 0 on failure
int print_mail(mailbox_t *mail)
{
    int arg;
    int *pmail = (int *)mail;

    printf("Mailbox contents:\n");
    printf("Command: %i\n", mail->command);
    printf("Status: %i\n", mail->status);

    pmail += 2; //Bypass command and status
    for (arg = 0; arg < (sizeof(mailbox_t) / sizeof(int)) - 2; arg++)
    {
        printf("Arg %i: %08X\n", arg, *pmail);
        pmail++;
    }

    return 1;
}

//Run applet command and wait for device response
//Return 1 on sucess, 0 on failure
int run_applet(mailbox_t *mail)
{
    int retries;
    int command;

    command = mail->command;

    if (command == APPLET_CMD_FULL_ERASE) retries = APPLET_RETRY_ERASE;
    else retries = APPLET_RETRY_NORMAL;

    goto_address(appinfo.load_addr); //Run the applet

    if (verbose) printf("RUN: Command out: %08x\n", command);
    do
    {
        slp(APPLET_WAIT_MS); //Allow applet to run
        if (verbose) printf("RUN: Waiting on applet return\n");
        mail->command = read_word(appinfo.mail_addr);
    } while ((mail->command != ~command) && retries--);

    if (verbose) print_mail(mail);

    if (retries == -1)
    {
        if (verbose) printf("RUN: Error running applet\n");
        return 0;
    }

    read_mail(mail);
    if (mail->status == STATUS_OK)
    {
        if (verbose) printf("RUN: Applet return OK!\n");
        return 1;
    }
    else
    {
        printf("RUN: Applet return ERROR!\n");
        if (verbose) print_mail(mail);
        return 0;
    }
}

//Print a formatted hex/ascii listing of supplied data
void print_hex_listing(char *data, int binfilesize, int markbyte, int base_addr)
{
    unsigned char *pbinfile = (unsigned char *)data;
    int chrnum;
    int binfileend = binfilesize;
    int addr = base_addr;
    int pf = (markbyte == 0);
    unsigned char *ascii;
    unsigned char *pascii = NULL;
    unsigned int cols = hex_cols;
    unsigned int colw = hex_colw;

    if (cols < 1)
    {
        printf("Error: Hex listing column count invalid!\n");
        return;
    }

    if (colw < 1)
    {
        printf("Error: Hex listing column width invalid!\n");
        return;
    }

    ascii = (unsigned char *)calloc(cols * colw * sizeof(unsigned char) + 1, sizeof(unsigned char));

    if (ascii == NULL)
    {
        printf("Error: Could not allocate memory for data listing!\n");
        return;
    }

    pascii = ascii;

    printf("\n");

    for (chrnum = 0; chrnum < binfileend; chrnum++, pbinfile++)
    {
        if (chrnum)
        {
            if (chrnum % (cols * colw) == 0)
            {
                *pascii = 0;
                printf("|%s|", ascii);
                if (chrnum > markbyte && !pf)
                {
                    printf(" @%i", markbyte % (cols * colw) + 1);
                    pf = 1;
                }
                printf("\n");
                addr += cols * colw;
                printf("%08X | ", addr);
                pascii = ascii;
            }
            else if (chrnum % colw == 0) printf("%c", COLCHAR);
        }
        else
            printf("%08X | ", addr);

        if (*pbinfile >= 0x20 && *pbinfile <= 0x7E)
            *pascii = *pbinfile;
        else
            *pascii = ' ';
        pascii++;

        if (*pbinfile < 0x10)
            printf("0");
        printf("%X%c", *pbinfile, TOKCHAR);
    }

    //Finish off a line with spaces if data ended early
    while (chrnum % (cols * colw) != 0)
    {
        if (chrnum % colw == 0) printf("%c", COLCHAR);

        *pascii = ' ';
        pascii++;

        printf("  %c", TOKCHAR);

        chrnum++;
    }

    //Print last ascii line
    *pascii = 0;
    printf("|%s|", ascii);
    if (chrnum > markbyte && !pf)
        printf(" @%i", markbyte % (colw * cols) + 1);

    printf("\n\n");

    free(ascii);
}

int test_port(char *portname, char silent)
{

    if (!open_port(portname, silent))
    {
        if (!silent) printf("Error: Could not open port! (Correct port?)\n");
        return 0;
    }

    if (!config_port())
    {
        if (!silent) printf("Error: Could not configure port! (Correct port?)\n");
        close_port(silent);
        return 0;
    }

    if (!set_normal_mode())
    {
        if (!silent) printf("Error: Could not communicate with device! (Correct port?)\n");
        close_port(silent);
        return 0;
    }

    return 1;
}

int test_mcu(char silent)
{
    //Find MCU via device ID
    int8_t mcu_index = -1;
    int8_t mcu_max = sizeof(mcus) / sizeof(mcu_t);
    int deviceid;

    for (mcu_index = 0; mcu_index < mcu_max; mcu_index++)
    {
        mcu = (mcu_t *)&mcus[mcu_index];
        deviceid = read_word(mcu->cidr_addr);
        if (read_error)
        {
            if (!silent && verbose) printf("Notice: Could not read device ID at %08X!\n", mcu->cidr_addr);
            continue;
        }

        if ((deviceid & CIDR_DIE_REVISION_MASK) == mcu->cidr)
        {
            if (!silent && verbose) printf("Found supported device ID: %08X\n", deviceid);
            break;
        }
    }

    if (mcu_index == mcu_max)
    {
        if (!silent) printf("Error: Could not find matching device ID!\n");
        return 0;
    }

    return 1;
}

#define DSU 0x41002000
#define DSU_STATUSB (DSU + 2)

#define NVMCTRL 0x41004000
#define NVMCTRL_CTRLA (NVMCTRL)
#define NVMCTRL_CTRLB (NVMCTRL + 4)
#define NVMCTRL_ADDR (NVMCTRL + 0x14)

#define NVMCTRL_CTRLB_CMDEX_KEY 0xA500
#define NVMCTRL_CTRLB_CMD_WQW 0x4
#define NVMCTRL_CTRLB_CMD_PBC 0x15
#define NVMCTRL_CTRLB_CMD_EP 0x0

#define NVMCTRL_USER 0x00804000

int write_user_row(uint32_t *data)
{
    uint16_t status = read_half_word(DSU_STATUSB);
    printf("dsu statusb: %08x\n", status);
    write_half_word(DSU_STATUSB, status);

    uint32_t cfg = read_word(NVMCTRL_CTRLA); /* clear nvm interrupt status bits */
    printf("nvm config: %08x\n", cfg);
    cfg &= ~(0xf0);
    write_word(NVMCTRL_CTRLA, cfg);

    write_word(NVMCTRL_ADDR, NVMCTRL_USER); /* set user row address */
    write_half_word(NVMCTRL_CTRLB, (NVMCTRL_CTRLB_CMDEX_KEY|NVMCTRL_CTRLB_CMD_EP)); /* erase page */
    slp(100);
    write_half_word(NVMCTRL_CTRLB, (NVMCTRL_CTRLB_CMDEX_KEY|NVMCTRL_CTRLB_CMD_PBC)); /* erase write buffer */
    slp(100);
    for(int i = 0; i < 4; i++) {
        write_word(NVMCTRL_USER + i * 4, data[i]); /* write in the write buffer */
    }
    write_word(NVMCTRL_ADDR, NVMCTRL_USER); /* set user row address */
    write_half_word(NVMCTRL_CTRLB, (NVMCTRL_CTRLB_CMDEX_KEY|NVMCTRL_CTRLB_CMD_WQW)); /* program quad word (128bits) */
    slp(100);
    return 0;
}

int read_user_row(void)
{
    uint32_t user_row[4];
    printf("user row: ");
    for (int i = 0; i < 4; i++) {
        user_row[i] = read_word(0x804000 + i * 4);
        printf("0x%08x ", user_row[i]);
    }
    printf("\n");
    if ((user_row[1] & 0x7f) != 0x12) {
        printf("SmartEEPROM not configured, proceed\n");
        user_row[1] |= (0x12);
        write_user_row(user_row);
    } else {
        printf("SmartEEPROM already configured\n");
    }
    return 0;
}

//Upper case any lower case characters in a string
void strlower(char *str)
{
    char *c = str;
    while (*c)
    {
        if (*c >= 'A' && *c <= 'Z') *c += 32;
        c++;
    }
}

//Lower case any upper case characters in a string
void strupper(char *str)
{
    char *c = str;
    while (*c)
    {
        if (*c >= 'a' && *c <= 'z') *c -= 32;
        c++;
    }
}

//Return file size of given file
int filesize(char *fname)
{
    struct stat st;
    stat(fname, &st);
    return (int)st.st_size;
}

//Read byte from device
//Must check read_error for a read error after return
//Return unsigned byte from memory location
int read_byte(int addr)
{
    return read_data(addr, 1);
}

//Read half word from device
//Must check read_error for a read error after return
//Return unsigned half word from memory location
int read_half_word(int addr)
{
    return read_data(addr, 2);
}

//Read word from device
//Must check read_error for a read error after return
//Return unsigned word from memory location
int read_word(int addr)
{
    return read_data(addr, 4);
}

//Write word to device
//Return 1 on success, 0 on failure
int write_word(int addr, int data)
{
    return write_data(addr, 4, data);
}

//Write half word to device
//Return 1 on success, 0 on failure
int write_half_word(int addr, int data)
{
    return write_data(addr, 2, data);
}

//Write byte to device
//Return 1 on success, 0 on failure
int write_byte(int addr, int data)
{
    return write_data(addr, 1, data);
}

//Set terminal command mode
//Return 0 always
int set_terminal_mode(void)
{
    //NOT SUPPORTING TERMINAL MODE
    return 0;
}

//Display program version
void display_version(void)
{
    printf(PROGRAM_NAME " %i.%02i\n", VERSION_MAJOR, VERSION_MINOR);
    printf("\n");
}

//Display program copyright
void display_copyright(void)
{
    printf(PROGRAM_NAME "  Copyright (C) 2018-2020 Massdrop Inc.\n");
    printf("This program is Free Software and has ABSOLUTELY NO WARRANTY\n");
    printf("\n");
}

//Display program help
void display_help(void)
{
    printf("Usage: mdloader [options] ...\n");
    printf("  -h --help                      Print this help message\n");
    printf("  -v --verbose                   Print verbose messages\n");
    printf("  -V --version                   Print version information\n");
    printf("  -f --first                     Use first found device port as programming port\n");
    printf("  -l --list                      Print valid attached devices for programming\n");
    printf("  -p --port port                 Specify programming port\n");
    printf("  -U --upload file               Read firmware from device into <file>\n");
    printf("  -a --addr address              Read firmware starting from <address>\n");
    printf("  -s --size size                 Read firmware size of <size>\n");
    printf("  -D --download file             Write firmware from <file> into device\n");
    printf("  -t --test                      Test mode (download/upload writes disabled, upload outputs data to stdout, restart disabled)\n");
    printf("     --smarteep                  Enable Smart EEPROM MCU feature\n");
    printf("     --cols count                Hex listing column count <count> [%i]\n", COLS);
    printf("     --colw width                Hex listing column width <width> [%i]\n", COLW);
    printf("     --restart                   Restart device after successful programming\n");
    printf("\n");
}

#define SW_COLS     1000
#define SW_COLW     1001
#define SW_SMARTEEP 1002

//Program command line options
struct option long_options[] = {
    //Flags
    { "restart",        no_argument,        &restart_after_program, 1 },
    //Other
    { "verbose",        no_argument,        0,  'v' },
    { "help",           no_argument,        0,  'h' },
    { "version",        no_argument,        0,  'V' },
    { "list",           no_argument,        0,  'l' },
    { "first",          no_argument,        0,  'f' },
    { "port",           required_argument,  0,  'p' },
    { "download",       required_argument,  0,  'D' },
    { "upload",         required_argument,  0,  'U' },
    { "addr",           required_argument,  0,  'a' },
    { "size",           required_argument,  0,  's' },
    { "test",           no_argument,        0,  't' },
    { "smarteep",       no_argument,        0,  SW_SMARTEEP },
    { "cols",           required_argument,  0,  SW_COLS },
    { "colw",           required_argument,  0,  SW_COLW },
    { 0, 0, 0, 0 }
};

int main(int argc, char *argv[])
{
    verbose = 0;
    testmode = 0;
    first_device = 0;
    restart_after_program = 0;
    hex_cols = COLS;
    hex_colw = COLW;

    display_version();
    display_copyright();

    int command = CMD_NONE;
    char portname[500] = "";
    char fname[1024] = "";
    int upload_address = 0;
    int upload_size = 0;
    int upload_address_set = 0;
    int upload_size_set = 0;

    while (command != CMD_ABORT && command != CMD_HELP)
    {
        int c;
        int option_index = 0;
        int base;

        c = getopt_long(argc, argv, "hvVlftp:D:U:a:s:", long_options, &option_index);

        if (c == -1)
            break;

        switch (c)
        {
            case 0:
                break;

            case 'v':
                verbose = 1;
                break;

            case 'h':
                command = CMD_HELP;
                break;

            case 'V':
                command = CMD_VERSION;
                break;

            case 'l':
                if (command == CMD_NONE)
                    command = CMD_LISTDEV;
                else
                {
                    printf("Error: Another command conflicts with list!\n\n");
                    command = CMD_ABORT;
                }
                break;

            case 'f':
                first_device = 1;
                break;

            case 'p':
                sprintf(portname, "%s", optarg);
                break;

            case 'U':
                sprintf(fname, "%s", optarg);
                if (command == CMD_NONE)
                    command = CMD_UPLOAD;
                else
                {
                    printf("Error: Another command conflicts with upload!\n\n");
                    command = CMD_ABORT;
                }
                break;

            case 'a':
                strlower(optarg);
                if (strstr(optarg, "0x") != NULL) base = 16;
                else base = 10;
                upload_address = (int)strtol(optarg, NULL, base);
                upload_address_set = 1;
                break;

            case 's':
                strlower(optarg);
                if (strstr(optarg, "0x") != NULL) base = 16;
                else base = 10;
                upload_size = (int)strtol(optarg, NULL, base);
                upload_size_set = 1;
                break;

            case 'D':
                sprintf(fname, "%s", optarg);
                if (command == CMD_NONE)
                    command = CMD_DOWNLOAD;
                else
                {
                    printf("Error: Another command conflicts with download!\n\n");
                    command = CMD_ABORT;
                }
                break;

            case 't':
                testmode = 1;
                break;

            case SW_SMARTEEP:
                command = CMD_READ_USER_ROW;
                break;

            case SW_COLS:
                hex_cols = atoi(optarg);
                if (hex_cols < 1)
                {
                    printf("Error: Hex listing column count must be greater than 0\n\n");
                    command = CMD_ABORT;
                }
                break;

            case SW_COLW:
                hex_colw = atoi(optarg);
                if (hex_colw < 1)
                {
                    printf("Error: Hex listing column width must be greater than 0\n\n");
                    command = CMD_ABORT;
                }
                break;

            default:
                command = CMD_ABORT;
                break;
        }
    }

    if (command == CMD_HELP || command == CMD_ABORT)
    {
        display_help();
        goto exitProgram;
    }

    if (command == CMD_VERSION)
    {
        //Already displayed version by default
        goto exitProgram;
    }

    if (command == CMD_LISTDEV)
    {
        list_devices(NULL);
        goto exitProgram;
    }

    if (command == CMD_NONE)
    {
        if (!testmode && !restart_after_program) //Allow certain commands/flags through if alone
        {
            display_help();
            goto exitProgram;
        }
    }

    if (command == CMD_UPLOAD)
    {
        int upload_error = 0;
        if (!upload_address_set)
        {
            printf("Error: Upload address must be set! Ex: --addr 0x4000\n");
            upload_error = 1;
        }
        if (!upload_size_set)
        {
            printf("Error: Upload size must be set! Ex: --size 8192\n");
            upload_error = 1;
        }

        if (upload_error)
        {
            goto exitProgram;
        }
    }

    if (first_device)
    {
        //Set port according to first discovered device

        int tries = 60;

        printf("Scanning for device for %i seconds\n", tries);
        while (tries)
        {
            printf(".");
            fflush(stdout);
            list_devices(portname);
            if (*portname != 0)
            {
                printf("\n");
                break; //Device port set
            }
            tries--;
            slp(1000); //Sleep 1s
        }

        if (!tries)
        {
            printf("\n");
            printf("Error: Could not find a valid device port!\n");
            goto exitProgram;
        }
    }

    if (*portname == 0)
    {
        printf("Error: No port specified!\n");
        print_com_example();
        goto exitProgram;
    }

    if (testmode)
    {
        printf("NOTICE: Test mode is active. Writes are disabled!\n\n");
    }

    mailbox_t mail;

    if (!test_port(portname, FALSE)) goto exitProgram;

    if (!test_mcu(FALSE)) goto closePort;

    printf("Found MCU: %s\n", mcu->name);

    print_bootloader_version();
    if (verbose) printf("Device ID: %08X\n", mcu->cidr);

    if (command == CMD_READ_USER_ROW)
    {
        read_user_row();
        goto exitProgram;
    }



    //Load applet
    FILE *fIn;
    char appletfname[128] = "";
    strlower(mcu->name);

    //sprintf(appletfname, "applet-flash-%s.bin", mcu->name);
    sprintf(appletfname, "applet-mdflash.bin");  //make filename non-dependent upon mcu->name
      
    printf("Applet file: %s\n", appletfname);

    fIn = fopen(appletfname, "rb");
    if (!fIn)
    {
        printf("Error: Could not open applet file: %s\n", appletfname);
        goto closePort;
    }
    else
    {
        char *appletbuf;
        int filebytes;
        int readbytes;

        filebytes = filesize(appletfname);
        if (filebytes == 0)
        {
            printf("Error: Applet file is empty!\n");
            fclose(fIn);
            goto closePort;
        }

        appletbuf = (char *)calloc(filebytes,1);
        if (appletbuf == NULL)
        {
            printf("Error: Could not allocate memory for applet file!\n");
            fclose(fIn);
            goto closePort;
        }

        readbytes = (int)fread(appletbuf, 1, filebytes, fIn);
        fclose(fIn);

        if (readbytes != filebytes)
        {
            printf("Error: Applet read error!\n");
            goto closePort;
        }

        if (readbytes < sizeof(appinfo_t))
        {
            printf("Error: Applet binary too small!\n");
            goto closePort;
        }

        memcpy(&appinfo, appletbuf + readbytes - sizeof(appinfo_t), sizeof(appinfo_t));
        if (appinfo.magic != 0x4142444D)
        {
            printf("Error: Applet info not found!\n");
            goto closePort;
        }

        if (verbose)
        {
            printf("Applet load address: %08X\n", appinfo.load_addr);
            printf("Applet mail address: %08X\n", appinfo.mail_addr);
        }

        //printf("Applet data:\n");
        //print_hex_listing(appletbuf, readbytes, 0, 0);

        if (verbose) printf("Applet size: %i\n", readbytes);

        if (!send_file(appinfo.load_addr, readbytes, appletbuf))
        {
            printf("Error: Could not send applet!\n");
            free(appletbuf);
            goto closePort;
        }

        free(appletbuf);

        //printf("Applet data in RAM:\n");
        //char *data_recv = recv_file(appinfo.load_addr, readbytes);
        //if (data_recv)
        //{
        //    print_hex_listing(data_recv, readbytes, 0, appinfo.load_addr);
        //    free(data_recv); //Free memory allocated in recv_file
        //}
    }

    initparams.command = APPLET_CMD_INIT;
    initparams.status = STATUS_BUSY;
    initparams.argument.inputInit.bank = 0;
    initparams.argument.inputInit.comType = USB_COM_TYPE;
    initparams.argument.inputInit.traceLevel = 0;

    send_mail(&initparams);

    if (run_applet(&initparams) == 0)
    {
        printf("Error: Applet run error for init!\n");
        goto closePort;
    }

    if (verbose)
    {
        printf("Memory Size: %08X\n", initparams.argument.outputInit.memorySize);
        printf("Buffer Addr: %08X\n", initparams.argument.outputInit.bufferAddress);
        printf("Buffer Size: %08X\n", initparams.argument.outputInit.bufferSize);
        printf("Lock Region Size: %04X\n", initparams.argument.outputInit.memoryInfo.lockRegionSize);
        printf("Lock Region Bits: %04X\n", initparams.argument.outputInit.memoryInfo.numbersLockBits);
        printf("Page Size: %08X\n", initparams.argument.outputInit.pageSize);
        printf("Number of Pages: %08X\n", initparams.argument.outputInit.nbPages);
        printf("App Start Page: %08X\n", initparams.argument.outputInit.appStartPage);
        printf("\n");
    }

    appletinfo.command = APPLET_CMD_INFO;
    appletinfo.status = STATUS_BUSY;

    send_mail(&appletinfo);

    if (run_applet(&appletinfo) == 0)
    {
        printf("Error: Applet run error for info!\n");
        goto closePort;
    }

    printf("Applet Version: %i\n", appletinfo.argument.outputInfo.version_number);

    if (initparams.argument.outputInit.memorySize != mcu->flash_size)
    {
        printf("Error: MCU memory size mismatch! (Given %08X, Applet reported %08X)\n", mcu->flash_size, initparams.argument.outputInit.memorySize);
        goto closePort;
    }

    bootloader_length = initparams.argument.outputInit.appStartPage * initparams.argument.outputInit.pageSize;

    if (bootloader_length == 0)
    {
        printf("Error: Applet reported zero length bootloader!\n");
        goto closePort;
    }

    if (verbose)
    {
        printf("Bootloader length: 0x%X\n", bootloader_length);
        print_bootloader_serial();
    }

    if (command == CMD_DOWNLOAD)
    {
        //Load application
        data_t *data = NULL;

        data = load_file(fname);

        if (!data)
        {
            printf("Error: Could not parse file!\n");
            goto closePort;
        }

        if (data->addr < (mcu->flash_addr + bootloader_length))
        {
            printf("Error: Attempt to write to bootloader section!\n"); //This check is also performed in the loaded applet
            free_data(data);
            goto closePort;
        }

        if (data->size > mcu->flash_size - (mcu->flash_addr + bootloader_length))
        {
            printf("Error: Attempt to write outside memory bounds!\n");
            free_data(data);
            goto closePort;
        }

        char *pds = data->data;
        char *pde = data->data + data->size;

        int readbytes;

        printf("Writing firmware... ");
        if (testmode)
            printf("(test mode disables writes) ");

        readbytes = pde - pds < initparams.argument.outputInit.bufferSize ? pde - pds : initparams.argument.outputInit.bufferSize;
        int memoryOffset = data->addr;
        while (readbytes > 0)
        {
            //printf("Send firmware (%i):\n", readbytes);
            //print_hex_listing(pds, readbytes, 0, 0);

            if (!send_file(initparams.argument.outputInit.bufferAddress, readbytes, pds))
            {
                printf("\nError: Failed write to applet buffer!\n");
                free_data(data);
                goto closePort;
            }

            memset(&mail, 0, sizeof(mailbox_t));

            //Note: Testmode will turn writes into reads
            if (testmode)
            {
                mail.command = APPLET_CMD_READ;
                mail.argument.inputRead.bufferAddr = initparams.argument.outputInit.bufferAddress;
                mail.argument.inputRead.bufferSize = readbytes;
                mail.argument.inputRead.memoryOffset = memoryOffset;
            }
            else
            {
                mail.command = APPLET_CMD_WRITE;
                mail.argument.inputWrite.bufferAddr = initparams.argument.outputInit.bufferAddress;
                mail.argument.inputWrite.bufferSize = readbytes;
                mail.argument.inputWrite.memoryOffset = memoryOffset;
            }
            send_mail(&mail);
            run_applet(&mail);

            if (mail.status != STATUS_OK)
            {
                printf("\nError: Applet failed write!\n");
                free_data(data);
                goto closePort;
            }

            if (testmode)
            {
                if (mail.argument.outputRead.bytesRead != readbytes)
                {
                    printf("\nError: Sent bytes != written bytes (%i != %i)\n", readbytes, mail.argument.outputRead.bytesRead);
                    free_data(data);
                    goto closePort;
                }
            }
            else
            {
                if (mail.argument.outputWrite.bytesWritten != readbytes)
                {
                    printf("\nError: Sent bytes != written bytes (%i != %i)\n", readbytes, mail.argument.outputWrite.bytesWritten);
                    free_data(data);
                    goto closePort;
                }
            }

            memoryOffset += readbytes;

            pds += readbytes;
            readbytes = pde - pds < initparams.argument.outputInit.bufferSize ? pde - pds : initparams.argument.outputInit.bufferSize;
        }

        free_data(data);

        printf("Complete!\n");

        if (restart_after_program)
            jump_application();
    }

    if (command == CMD_UPLOAD)
    {
        if (upload_address + upload_size > mcu->flash_size)
        {
            printf("Error: Attempt to read outside memory bounds!\n");
            goto closePort;
        }

        if (upload_size <= 0)
        {
            printf("Error: Invalid read size!\n");
            goto closePort;
        }

        //Read memory
        char *readbuffer;
        char *preadbuffer;

        readbuffer = (char *)malloc(upload_size);
        preadbuffer = readbuffer;

        if (!readbuffer)
        {
            printf("Error: Could not allocate memory for firmware read!\n");
        }
        else
        {
            printf("Reading memory %08X - %08X... \n", upload_address, upload_address+upload_size-1);
            //printf("Address: %08X\n", upload_address);
            //printf("Size: %08X\n", upload_size);

            int readbytes = upload_size;
            int curbytes = initparams.argument.outputInit.bufferSize;
            int memoryOffset = upload_address;
            while (readbytes > 0)
            {
                if (curbytes > readbytes) curbytes = readbytes;

                memset(&mail, 0, sizeof(mailbox_t));

                mail.command = APPLET_CMD_READ;
                mail.argument.inputRead.bufferAddr = initparams.argument.outputInit.bufferAddress;
                mail.argument.inputRead.bufferSize = curbytes;
                mail.argument.inputRead.memoryOffset = memoryOffset;

                if (send_mail(&mail) != 1)
                {
                    printf("\nError: Failed to send applet mail!\n");
                    free(readbuffer);
                    goto closePort;
                }

                if (run_applet(&mail) != 1)
                {
                    printf("\nError: Failed to run applet!\n");
                    free(readbuffer);
                    goto closePort;
                }

                if (mail.status != STATUS_OK)
                {
                    printf("\nError: Applet status not OK! [%i]\n", mail.status);
                    free(readbuffer);
                    goto closePort;
                }

                if (mail.argument.outputRead.bytesRead != curbytes)
                {
                    printf("\nError: Sent bytes != written bytes (%i != %i)\n", curbytes, mail.argument.outputRead.bytesRead);
                    free(readbuffer);
                    goto closePort;
                }

                char *bufread = recv_file(initparams.argument.outputInit.bufferAddress, curbytes);
                if (bufread)
                {
                    memcpy(preadbuffer, bufread, curbytes);
                    preadbuffer += curbytes;
                    free(bufread);
                }
                else
                {
                    printf("Error: Could not read data buffer!\n");
                    free(readbuffer);
                    goto closePort;
                }

                memoryOffset += curbytes;

                readbytes -= curbytes;
            }

            if (testmode)
                print_hex_listing(readbuffer, upload_size, 0, upload_address);
            else
            {
                printf("Writing firmware to file... ");
                FILE *fOut = fopen(fname, "wb");

                if (!fOut)
                {
                    printf("Failed!\n");
                    printf("Error: Could not open file for firmware read output!\n");
                    goto closePort;
                }

                fwrite(readbuffer, upload_size, sizeof(char), fOut);

                fclose(fOut);

                printf("Success!\n");
            }

            free(readbuffer);

            printf("Complete!\n");
        }
    }

    //Allow for restart flag with no command to restart the device
    if (command == CMD_NONE && restart_after_program)
        jump_application();

closePort:

    close_port(FALSE);

exitProgram:

    return 0;
}

