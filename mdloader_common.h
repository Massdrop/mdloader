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

#ifndef _MDLOADER_COMMON_H
#define _MDLOADER_COMMON_H

#define PROGRAM_NAME  "Massdrop Loader"
#define VERSION_MAJOR 1
#define VERSION_MINOR 5 //0-99

#ifdef _WIN32
#define INITGUID
#include <windows.h>
#include <setupapi.h>
#include <devguid.h>
#include "tchar.h"
#include <conio.h>
#else
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <dirent.h>
#endif //_WIN32

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>

//Atmel files
#include "./atmel/applet.h"
#include "./atmel/status_codes.h"

#define FALSE   0
#define TRUE    1

#ifdef _WIN32
#define slp(ms) Sleep(ms)
extern HANDLE gport;
#else
#define slp(ms) usleep(ms * 1000)
extern int gport;
#endif

extern char verbose;
extern char testmode;
extern int restart_after_program;
extern int hex_cols;
extern int hex_colw;

//These parameters must match the bootloader's
#define SERIAL_MAX_LENGTH       20

//SAM-BA Applet commands
#define CMD_END                 '#'
#define CMD_SET_NORMAL_MODE     'N' //N#            N#                                      done
#define CMD_SET_TERMINAL_MODE   'T' //T#            T#                                      NOT SUPPORTING
#define CMD_WRITE_ADDR_8        'O' //Addr,Value#   O00004000,AA#                           done
#define CMD_READ_ADDR_8         'o' //Addr,#        o00004000,#                             done
#define CMD_WRITE_ADDR_16       'H' //Addr,Value#   H00004000,AABB#                         done
#define CMD_READ_ADDR_16        'h' //Addr,#        h00004000,#                             done
#define CMD_WRITE_ADDR_32       'W' //Addr,Value#   W00004000,AABBCCDD#                     done
#define CMD_READ_ADDR_32        'w' //Addr,#        w00004000,#                             done
#define CMD_SEND_FILE           'S' //Addr,Bytes#   S00004000,1000#                         done
#define CMD_RECV_FILE           'R' //Addr,Bytes#   R00004000,1000#                         done
#define CMD_GOTO_ADDR           'G' //Addr#         G00004000#                              done
#define CMD_READ_VERSION        'V' //V#            V#                                      done
#define CMD_LOAD_APP            'X' //Load program  (Note: Custom to Massdrop Bootloader)   done

//Applet info consists of 8 32-bit words found at the end of applet binary
#pragma pack(push, 1)
typedef struct appinfo_s {
    uint32_t magic;
    uint32_t load_addr;
    uint32_t mail_addr;
    uint32_t unused[5];
} appinfo_t;
#pragma pack(pop)

//SAM-BA Settings
extern mailbox_t initparams;
extern mailbox_t appletinfo;
extern appinfo_t appinfo;

#define CIDR_DIE_REVISION_MASK 0xFFFFF0FF

typedef struct mcu_s {
    char name[20];      //MCU Name
    int cidr_addr;      //Chip ID Address
    int cidr;           //Chip ID
    int flash_size;     //Program Memory (FLASH_SIZE)
    int ram_size;       //Data Memory (HSRAM_SIZE)
    int flash_addr;     //Program Addr (FLASH_ADDR)
    int ram_addr;       //Data Addr (HSRAM_ADDR)
} mcu_t;

extern mcu_t mcus[];
extern mcu_t *mcu;
extern uint32_t bootloader_length;
int check_bootloader_write_attempt(int addr);
extern int read_error;

int print_bootloader_serial(void);
int send_mail(mailbox_t *mail);
int read_mail(mailbox_t *mail);
int print_mail(mailbox_t *mail);

#define APPLET_WAIT_MS          10  //Time between retries
#define APPLET_RETRY_NORMAL     10  //Normal operation retries
#define APPLET_RETRY_ERASE      25  //Erase operation retries

int run_applet(mailbox_t *mail);

void display_version(void);

void display_copyright(void);
void display_help(void);

enum command {
    CMD_NONE = 0,
    CMD_HELP,
    CMD_VERSION,
    CMD_LISTDEV,
    CMD_DOWNLOAD,
    CMD_UPLOAD,
    CMD_TEST,
    CMD_ABORT,
    CMD_CONFIG_SMARTEEPROM
};

extern struct option long_options[];

#define COLW 4
#define COLS 8
#define TOKCHAR ' '
#define COLCHAR ' '

void print_hex_listing(char *data, int binfilesize, int markbyte, int base_addr);

int write_word(int addr, int data);
int write_half_word(int addr, int data);
int write_byte(int addr, int data);
int read_byte(int addr);
int read_half_word(int addr);
int read_word(int addr);
int set_terminal_mode(void);

//OS specific commands
void print_com_example(void);
int goto_address(int addr);
char *recv_file(int addr, int bytes);
int send_file(int addr, int bytes, char *data);
int print_bootloader_version(void);
int set_normal_mode(void);
int jump_application(void);
int open_port(char *portname, char silent);
int close_port(char silent);
int config_port(void);
int test_port(char *portname, char silent);
int test_mcu(char silent);
int filesize(char *fname);
int read_data(int addr, int readsize);
int write_data(int addr, int writesize, int data);
void list_devices(char *first);
void strupper(char *str);
void strlower(char *str);
void configure_smarteeprom(void);

// Smart EEPROM specific
#define NVMCTRL 0x41004000
#define NVMCTRL_CTRLA (NVMCTRL)
#define NVMCTRL_CTRLB (NVMCTRL + 4)
#define NVMCTRL_ADDR (NVMCTRL + 0x14)

#define NVMCTRL_CTRLA_WMODE_MAN 0x0
#define NVMCTRL_CTRLB_CMDEX_KEY 0xA5
#define NVMCTRL_CTRLB_CMD_WQW 0x4
#define NVMCTRL_CTRLB_CMD_PBC 0x15
#define NVMCTRL_CTRLB_CMD_EP 0x0

#define NVMCTRL_USER 0x00804000

#define SLEEP_BETWEEN_WRITES 200

typedef union {
    struct {
        uint32_t SBLK          : 4; /* bit: 35:32 - Number of NVM Blocks composing a SmartEEPROM sector   */
        uint32_t PSZ           : 3; /* bit: 38:36 - SmartEEPROM Page Size                                 */
        uint32_t RAM_ECCDIS    : 1; /* bit:    39 - RAM ECC Disable                                       */
        uint32_t               : 8; /* bit: 47:40 - Factory settings - do not change                      */
        uint32_t WDT_ENABLE    : 1; /* bit:    48 - WDT Enable at power-on                                */
        uint32_t WDT_ALWAYS_ON : 1; /* bit:    49 - WDT Always-On at power-on                             */
        uint32_t WDT_PERIOD    : 4; /* bit: 53:50 - WDT Period at power-on                                */
        uint32_t WDT_WINDOW    : 4; /* bit: 57:54 - WDT Window mode time-out at power - on                */
        uint32_t WDT_EWOFFSET  : 4; /* bit: 61:58 - WDT Early Warning Interrupt Time Offset at power - on */
        uint32_t WDT_WEN       : 1; /* bit:    62 - WDT Window Mode Enable at power - on                  */
        uint32_t               : 1; /* bit:    63 - Factory settings - do not change                      */
    } bit;
    uint32_t reg;
} NVMCTRL_USER_ROW_MAPPING1_Type;

typedef union {
    struct {
        uint16_t           : 2; /* bit:  1:0  Reserved                                                                   */
        uint16_t AUTOWS    : 1; /* bit:    2  Auto Wait State Enable                                                     */
        uint16_t SUSPEN    : 1; /* bit:    3  Suspend Enable                                                             */
        uint16_t WMODE     : 2; /* bit:  5:4  Write Mode                                                                 */
        uint16_t PRM       : 2; /* bit:  7:6  Power Reduction Mode during Sleep                                          */
        uint16_t RWS       : 4; /* bit: 11:8  NVM Read Wait States                                                       */
        uint16_t AHBNS0    : 1; /* bit:   12  Force AHB0 access to NONSEQ, burst transfers are continuously rearbitrated */
        uint16_t AHBNS1    : 1; /* bit:   13  Force AHB1 access to NONSEQ, burst transfers are continuously rearbitrated */
        uint16_t CACHEDIS0 : 1; /* bit:   14  AHB0 Cache Disable                                                         */
        uint16_t CACHEDIS1 : 1; /* bit:   15  AHB1 Cache Disable                                                         */
    } bit;
    uint16_t reg;
} NVMCTRL_CTRLA_Type;

typedef union {
    struct {
        uint16_t CMD   : 7; /* bit:  6:0  Command           */
        uint16_t       : 1; /* bit:    7  Reserved          */
        uint16_t CMDEX : 8; /* bit: 15:8  Command Execution */
    } bit;
    uint16_t reg;
} NVMCTRL_CTRLB_Type;

#endif //_MDLOADER_COMMON_H

