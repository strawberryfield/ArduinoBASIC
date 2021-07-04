/// @file
/// User memory area manager definition.
///
/// @author
/// copyright (c) 2021 Roberto Ceccarelli - Casasoft
/// http://strawberryfield.altervista.org
///
/// original work by
///    Gordon Brandly (Tiny Basic for 68000)
///    Mike Field <hamster@snap.net.nz> (Arduino Basic) (port to Arduino)
///    Scott Lawrence <yorgle@gmail.com> (TinyBasic Plus) (features, etc)
///
/// @copyright
/// This is free software:
/// you can redistribute it and/or modify it
/// under the terms of the GNU General Public License as published by
/// the Free Software Foundation, either version 3 of the License, or
/// (at your option) any later version.
///
/// You should have received a copy of the GNU General Public License
/// along with these files.
/// If not, see <http://www.gnu.org/licenses/>.
///
/// @remark
/// This software is distributed in the hope that it will be useful,
/// but WITHOUT ANY WARRANTY; without even the implied warranty of
/// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
/// See the GNU General Public License for more details.

#ifndef _USERMEM_H_
#define _USERMEM_H_

#include "Arduino.h"
#include "platform.h"
#include "keywords.h"
#include "globals.h"

typedef short unsigned LINENUM;

class usermemClass
{
private:
    short int expr4(void);
    short int expr3(void);
    short int expr2(void);

public:
    unsigned char program[kRamSize];
    unsigned char *txtpos, *list_line, *tmptxtpos;
    unsigned char expression_error;
    unsigned char *tempsp;

    unsigned char *stack_limit;
    unsigned char *program_start;
    unsigned char *program_end;
    unsigned char *stack; // Software stack for things that should go on the CPU stack
    unsigned char *variables_begin;
    unsigned char *current_line;
    unsigned char *sp;

    unsigned char table_index;
    LINENUM linenum;

    void ignore_blanks(void);
    void scantable(const unsigned char *table);
    unsigned short testnum(void);
    unsigned char *findline(void);
    void toUppercaseBuffer(void);

    short int expression(void);

    /** execute new command */ 
    void program_reset();
    /** return free memory amount */
    unsigned short free_mem();
    /** Find the end of the freshly entered line */
    void find_newline();
    /** Store value in var */
    void set_var(char var, short value);
    /** Check if current char is not an alpha character */
    bool isNotAlpha();
};

extern usermemClass mem;

#endif