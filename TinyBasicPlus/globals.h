/// @file
/// Tiny basic plus globals.
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

#ifndef _GLOBALS_H_
#define _GLOBALS_H_

#include "platform.h"

// some settings based things
extern boolean inhibitOutput;
extern boolean runAfterLoad;
extern boolean triggerRun;


typedef short unsigned LINENUM;
#ifdef ARDUINO
#define ECHO_CHARS 1
#else
#define ECHO_CHARS 0
#endif


extern unsigned char program[kRamSize];
extern unsigned char *txtpos,*list_line, *tmptxtpos;
extern unsigned char expression_error;
extern unsigned char *tempsp;


struct stack_for_frame {
  char frame_type;
  char for_var;
  short int terminal;
  short int step;
  unsigned char *current_line;
  unsigned char *txtpos;
};

struct stack_gosub_frame {
  char frame_type;
  unsigned char *current_line;
  unsigned char *txtpos;
};


#define STACK_SIZE (sizeof(struct stack_for_frame)*5)
#define VAR_SIZE sizeof(short int) // Size of variables in bytes

extern unsigned char *stack_limit;
extern unsigned char *program_start;
extern unsigned char *program_end;
extern unsigned char *stack; // Software stack for things that should go on the CPU stack
extern unsigned char *variables_begin;
extern unsigned char *current_line;
extern unsigned char *sp;
#define STACK_GOSUB_FLAG 'G'
#define STACK_FOR_FLAG 'F'
extern unsigned char table_index;
extern LINENUM linenum;

////////////////////////////////////////////////////////////////////////////////
// ASCII Characters
#define CR	'\r'
#define NL	'\n'
#define LF      0x0a
#define TAB	'\t'
#define BELL	'\b'
#define SPACE   ' '
#define SQUOTE  '\''
#define DQUOTE  '\"'
#define CTRLC	0x03
#define CTRLH	0x08
#define CTRLS	0x13
#define CTRLX	0x18

#endif