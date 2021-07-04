////////////////////////////////////////////////////////////////////////////////
// TinyBasic Plus
////////////////////////////////////////////////////////////////////////////////
//
// Authors:
//    Gordon Brandly (Tiny Basic for 68000)
//    Mike Field <hamster@snap.net.nz> (Arduino Basic) (port to Arduino)
//    Scott Lawrence <yorgle@gmail.com> (TinyBasic Plus) (features, etc)
//    Roberto Ceccarelli <strawberryfield@altervista.org> (refactoring)
//
// Contributors:
//          Brian O'Dell <megamemnon@megamemnon.com> (INPUT)
//    (full list tbd)

//  For full history of Tiny Basic, please see the wikipedia entry here:
//    https://en.wikipedia.org/wiki/Tiny_BASIC

// LICENSING NOTES:
//    Mike Field based his C port of Tiny Basic on the 68000
//    Tiny BASIC which carried the following license:
/*
******************************************************************
*                                                                *
*               Tiny BASIC for the Motorola MC68000              *
*                                                                *
* Derived from Palo Alto Tiny BASIC as published in the May 1976 *
* issue of Dr. Dobb's Journal.  Adapted to the 68000 by:         *
*       Gordon Brandly                                           *
*       12147 - 51 Street                                        *
*       Edmonton AB  T5W 3G8                                     *
*       Canada                                                   *
*       (updated mailing address for 1996)                       *
*                                                                *
* This version is for MEX68KECB Educational Computer Board I/O.  *
*                                                                *
******************************************************************
*    Copyright (C) 1984 by Gordon Brandly. This program may be   *
*    freely distributed for personal use only. All commercial    *
*                      rights are reserved.                      *
******************************************************************
*/
//    ref: http://members.shaw.ca:80/gbrandly/68ktinyb.html
//
//    However, Mike did not include a license of his own for his
//    version of this.
//    ref: http://hamsterworks.co.nz/mediawiki/index.php/Arduino_Basic
//
//    From discussions with him, I felt that the MIT license is
//    the most applicable to his intent.
//
//    I am in the process of further determining what should be
//    done wrt licensing further.  This entire header will likely
//    change with the next version 0.16, which will hopefully nail
//    down the whole thing so we can get back to implementing
//    features instead of licenses.  Thank you for your time.

// IF testing with Visual C, this needs to be the first thing in the file.
//#include "stdafx.h"

char eliminateCompileErrors = 1; // fix to suppress arduino build errors

// hack to let makefiles work with this file unchanged
#ifdef FORCE_DESKTOP
#undef ARDUINO
#include "desktop.h"
#else
#define ARDUINO 1
#endif

#include "platform.h"
#include "globals.h"
#include "strings.h"
#include "keywords.h"
#include "streamio.h"
#include "usermem.h"

streamioClass IO;
usermemClass mem;

static boolean inhibitOutput = false;
static boolean runAfterLoad = false;
static boolean triggerRun = false;

/***************************************************************************/
void loop()
{
    unsigned char *start;
    unsigned char *newEnd;
    unsigned char linelen;
    boolean isDigital;
    boolean alsoWait = false;
    int val;

#ifdef ARDUINO
#ifdef ENABLE_TONES
    noTone(kPiezoPin);
#endif
#endif

    mem.program_start = mem.program;
    mem.program_reset();
    mem.sp = mem.program + sizeof(mem.program); // Needed for printnum
#ifdef ALIGN_MEMORY
    // Ensure these memory blocks start on even pages
    mem.stack_limit = ALIGN_DOWN(mem.program + sizeof(mem.program) - STACK_SIZE);
    mem.variables_begin = ALIGN_DOWN(mem.stack_limit - 27 * VAR_SIZE);
#else
    mem.stack_limit = mem.program + sizeof(mem.program) - STACK_SIZE;
    mem.variables_begin = mem.stack_limit - 27 * VAR_SIZE;
#endif

    // memory free
    IO.printnum(mem.free_mem());
    IO.printmsg(memorymsg);
#ifdef ARDUINO
#ifdef ENABLE_EEPROM
    // eprom size
    IO.printnum(E2END + 1);
    IO.printmsg(eeprommsg);
#endif /* ENABLE_EEPROM */
#endif /* ARDUINO */

warmstart:
    // this signifies that it is running in 'direct' mode.
    mem.current_line = 0;
    mem.sp = mem.program + sizeof(mem.program);
    IO.printmsg(okmsg);

prompt:
    if (triggerRun)
    {
        triggerRun = false;
        mem.current_line = mem.program_start;
        goto execline;
    }

    IO.getln('>');
    mem.toUppercaseBuffer();
    mem.txtpos = mem.program_end + sizeof(unsigned short);

    // Find the end of the freshly entered line
    mem.find_newline();

    // Move it to the end of program_memory
    {
        unsigned char *dest;
        dest = mem.variables_begin - 1;
        while (1)
        {
            *dest = *mem.txtpos;
            if (mem.txtpos == mem.program_end + sizeof(unsigned short))
                break;
            dest--;
            mem.txtpos--;
        }
        mem.txtpos = dest;
    }

    // Now see if we have a line number
    mem.linenum = mem.testnum();
    mem.ignore_blanks();
    if (mem.linenum == 0)
        goto direct;

    if (mem.linenum == 0xFFFF)
        goto qhow;

    // Find the length of what is left, including the (yet-to-be-populated) line header
    linelen = 0;
    while (mem.txtpos[linelen] != NL)
        linelen++;
    linelen++;                                        // Include the NL in the line length
    linelen += sizeof(unsigned short) + sizeof(char); // Add space for the line number and line length

    // Now we have the number, add the line header.
    mem.txtpos -= 3;

#ifdef ALIGN_MEMORY
    // Line starts should always be on 16-bit pages
    if (ALIGN_DOWN(mem.txtpos) != mem.txtpos)
    {
        mem.txtpos--;
        linelen++;
        // As the start of the line has moved, the data should move as well
        unsigned char *tomove;
        tomove = mem.txtpos + 3;
        while (tomove < mem.txtpos + linelen - 1)
        {
            *tomove = *(tomove + 1);
            tomove++;
        }
    }
#endif

    *((unsigned short *)mem.txtpos) = mem.linenum;
    mem.txtpos[sizeof(LINENUM)] = linelen;

    // Merge it into the rest of the program
    start = mem.findline();

    // If a line with that number exists, then remove it
    if (start != mem.program_end && *((LINENUM *)start) == mem.linenum)
    {
        unsigned char *dest, *from;
        unsigned tomove;

        from = start + start[sizeof(LINENUM)];
        dest = start;

        tomove = mem.program_end - from;
        while (tomove > 0)
        {
            *dest = *from;
            from++;
            dest++;
            tomove--;
        }
        mem.program_end = dest;
    }

    if (mem.txtpos[sizeof(LINENUM) + sizeof(char)] == NL) // If the line has no txt, it was just a delete
        goto prompt;

    // Make room for the new line, either all in one hit or lots of little shuffles
    while (linelen > 0)
    {
        unsigned int tomove;
        unsigned char *from, *dest;
        unsigned int space_to_make;

        space_to_make = mem.txtpos - mem.program_end;

        if (space_to_make > linelen)
            space_to_make = linelen;
        newEnd = mem.program_end + space_to_make;
        tomove = mem.program_end - start;

        // Source and destination - as these areas may overlap we need to move bottom up
        from = mem.program_end;
        dest = newEnd;
        while (tomove > 0)
        {
            from--;
            dest--;
            *dest = *from;
            tomove--;
        }

        // Copy over the bytes into the new space
        for (tomove = 0; tomove < space_to_make; tomove++)
        {
            *start = *mem.txtpos;
            mem.txtpos++;
            start++;
            linelen--;
        }
        mem.program_end = newEnd;
    }
    goto prompt;

unimplemented:
    IO.printmsg(unimplimentedmsg);
    goto prompt;

qhow:
    IO.printmsg(howmsg);
    goto prompt;

qwhat:
    IO.printmsgNoNL(whatmsg);
    if (mem.current_line != NULL)
    {
        unsigned char tmp = *mem.txtpos;
        if (*mem.txtpos != NL)
            *mem.txtpos = '^';
        mem.list_line = mem.current_line;
        IO.printline();
        *mem.txtpos = tmp;
    }
    IO.line_terminator();
    goto prompt;

qsorry:
    IO.printmsg(sorrymsg);
    goto warmstart;

run_next_statement:
    while (*mem.txtpos == ':')
        mem.txtpos++;
    mem.ignore_blanks();
    if (*mem.txtpos == NL)
        goto execnextline;
    goto interperateAtTxtpos;

direct:
    mem.txtpos = mem.program_end + sizeof(LINENUM);
    if (*mem.txtpos == NL)
        goto prompt;

interperateAtTxtpos:
    if (IO.breakcheck())
    {
        IO.printmsg(breakmsg);
        goto warmstart;
    }

    mem.scantable(keywords);

    switch (mem.table_index)
    {
    case KW_DELAY:
    {
#ifdef ARDUINO
        val = mem.expression();
        delay(val);
        goto execnextline;
#else
        goto unimplemented;
#endif
    }

    case KW_FILES:
        goto files;
    case KW_LIST:
        goto list;
    case KW_CHAIN:
        goto chain;
    case KW_LOAD:
        goto load;
    case KW_MEM:
        goto mem;
    case KW_NEW:
        if (mem.txtpos[0] != NL)
            goto qwhat;
        mem.program_reset();
        goto prompt;
    case KW_RUN:
        mem.current_line = mem.program_start;
        goto execline;
    case KW_SAVE:
        goto save;
    case KW_NEXT:
        goto next;
    case KW_LET:
        goto assignment;
    case KW_IF:
        short int val;
        val = mem.expression();
        if (mem.expression_error || *mem.txtpos == NL)
            goto qhow;
        if (val != 0)
            goto interperateAtTxtpos;
        goto execnextline;

    case KW_GOTO:
        mem.linenum = mem.expression();
        if (mem.expression_error || *mem.txtpos != NL)
            goto qhow;
        mem.current_line = mem.findline();
        goto execline;

    case KW_GOSUB:
        goto gosub;
    case KW_RETURN:
        goto gosub_return;
    case KW_REM:
    case KW_QUOTE:
        goto execnextline; // Ignore line completely
    case KW_FOR:
        goto forloop;
    case KW_INPUT:
        goto input;
    case KW_PRINT:
    case KW_QMARK:
        goto print;
    case KW_POKE:
        goto poke;
    case KW_END:
    case KW_STOP:
        // This is the easy way to end - set the current line to the end of program attempt to run it
        if (mem.txtpos[0] != NL)
            goto qwhat;
        mem.current_line = mem.program_end;
        goto execline;
    case KW_BYE:
        // Leave the basic interperater
        return;

    case KW_AWRITE: // AWRITE <pin>, HIGH|LOW
        isDigital = false;
        goto awrite;
    case KW_DWRITE: // DWRITE <pin>, HIGH|LOW
        isDigital = true;
        goto dwrite;

    case KW_RSEED:
        goto rseed;

#ifdef ENABLE_TONES
    case KW_TONEW:
        alsoWait = true;
    case KW_TONE:
        goto tonegen;
    case KW_NOTONE:
        goto tonestop;
#endif

#ifdef ARDUINO
#ifdef ENABLE_EEPROM
    case KW_EFORMAT:
        goto eformat;
    case KW_ESAVE:
        goto esave;
    case KW_ELOAD:
        goto eload;
    case KW_ELIST:
        goto elist;
    case KW_ECHAIN:
        goto echain;
#endif
#endif

    case KW_DEFAULT:
        goto assignment;
    default:
        break;
    }

execnextline:
    if (mem.current_line == NULL) // Processing direct commands?
        goto prompt;
    mem.current_line += mem.current_line[sizeof(LINENUM)];

execline:
    if (mem.current_line == mem.program_end) // Out of lines to run
        goto warmstart;
    mem.txtpos = mem.current_line + sizeof(LINENUM) + sizeof(char);
    goto interperateAtTxtpos;

#ifdef ARDUINO
#ifdef ENABLE_EEPROM
elist:
{
    int i;
    for (i = 0; i < (E2END + 1); i++)
    {
        val = EEPROM.read(i);

        if (val == '\0')
        {
            goto execnextline;
        }
        IO.outchar_printable(val);
    }
}
    goto execnextline;

eformat:
{
    for (int i = 0; i < E2END; i++)
    {
        if ((i & 0x03f) == 0x20)
            IO.outchar('.');
        EEPROM.write(i, 0);
    }
    IO.outchar(LF);
}
    goto execnextline;

esave:
{
    IO.outStream = streamioClass::streamType::kStreamEEProm;
    eepos = 0;

    // copied from "List"
    mem.list_line = mem.findline();
    while (mem.list_line != mem.program_end)
    {
        IO.printline();
    }
    IO.outchar('\0');

    // go back to standard output, close the file
    IO.outStream = streamioClass::streamType::kStreamSerial;

    goto warmstart;
}

echain:
    runAfterLoad = true;

eload:
    // clear the program
    mem.program_reset();

    // load from a file into memory
    eepos = 0;
    IO.inStream = streamioClass::streamType::kStreamEEProm;
    inhibitOutput = true;
    goto warmstart;
#endif /* ENABLE_EEPROM */
#endif

input:
{
    unsigned char var;
    int value;
    mem.ignore_blanks();
    if (mem.isNotAlpha())
        goto qwhat;
    var = *mem.txtpos;
    mem.txtpos++;
    mem.ignore_blanks();
    if (*mem.txtpos != NL && *mem.txtpos != ':')
        goto qwhat;
inputagain:
    mem.tmptxtpos = mem.txtpos;
    IO.getln('?');
    mem.toUppercaseBuffer();
    mem.txtpos = mem.program_end + sizeof(unsigned short);
    mem.ignore_blanks();
    value = mem.expression();
    if (mem.expression_error)
        goto inputagain;
    mem.set_var(var, value);
    mem.txtpos = mem.tmptxtpos;

    goto run_next_statement;
}

forloop:
{
    unsigned char var;
    short int initial, step, terminal;
    mem.ignore_blanks();
    if (mem.isNotAlpha())
        goto qwhat;
    var = *mem.txtpos;
    mem.txtpos++;
    mem.ignore_blanks();
    if (*mem.txtpos != '=')
        goto qwhat;
    mem.txtpos++;
    mem.ignore_blanks();

    initial = mem.expression();
    if (mem.expression_error)
        goto qwhat;

    mem.scantable(to_tab);
    if (mem.table_index != 0)
        goto qwhat;

    terminal = mem.expression();
    if (mem.expression_error)
        goto qwhat;

    mem.scantable(step_tab);
    if (mem.table_index == 0)
    {
        step = mem.expression();
        if (mem.expression_error)
            goto qwhat;
    }
    else
        step = 1;
    mem.ignore_blanks();
    if (*mem.txtpos != NL && *mem.txtpos != ':')
        goto qwhat;

    if (!mem.expression_error && *mem.txtpos == NL)
    {
        struct stack_for_frame *f;
        if (mem.sp + sizeof(struct stack_for_frame) < mem.stack_limit)
            goto qsorry;

        mem.sp -= sizeof(struct stack_for_frame);
        f = (struct stack_for_frame *)mem.sp;
        ((short int *)mem.variables_begin)[var - 'A'] = initial;
        f->frame_type = STACK_FOR_FLAG;
        f->for_var = var;
        f->terminal = terminal;
        f->step = step;
        f->txtpos = mem.txtpos;
        f->current_line = mem.current_line;
        goto run_next_statement;
    }
}
    goto qhow;

gosub:
    mem.linenum = mem.expression();
    if (!mem.expression_error && *mem.txtpos == NL)
    {
        struct stack_gosub_frame *f;
        if (mem.sp + sizeof(struct stack_gosub_frame) < mem.stack_limit)
            goto qsorry;

        mem.sp -= sizeof(struct stack_gosub_frame);
        f = (struct stack_gosub_frame *)mem.sp;
        f->frame_type = STACK_GOSUB_FLAG;
        f->txtpos = mem.txtpos;
        f->current_line = mem.current_line;
        mem.current_line = mem.findline();
        goto execline;
    }
    goto qhow;

next:
    // Fnd the variable name
    mem.ignore_blanks();
    if (mem.isNotAlpha())
        goto qhow;
    mem.txtpos++;
    mem.ignore_blanks();
    if (*mem.txtpos != ':' && *mem.txtpos != NL)
        goto qwhat;

gosub_return:
    // Now walk up the stack frames and find the frame we want, if present
    mem.tempsp = mem.sp;
    while (mem.tempsp < mem.program + sizeof(mem.program) - 1)
    {
        switch (mem.tempsp[0])
        {
        case STACK_GOSUB_FLAG:
            if (mem.table_index == KW_RETURN)
            {
                struct stack_gosub_frame *f = (struct stack_gosub_frame *)mem.tempsp;
                mem.current_line = f->current_line;
                mem.txtpos = f->txtpos;
                mem.sp += sizeof(struct stack_gosub_frame);
                goto run_next_statement;
            }
            // This is not the loop you are looking for... so Walk back up the stack
            mem.tempsp += sizeof(struct stack_gosub_frame);
            break;
        case STACK_FOR_FLAG:
            // Flag, Var, Final, Step
            if (mem.table_index == KW_NEXT)
            {
                struct stack_for_frame *f = (struct stack_for_frame *)mem.tempsp;
                // Is the the variable we are looking for?
                if (mem.txtpos[-1] == f->for_var)
                {
                    short int *varaddr = ((short int *)mem.variables_begin) + mem.txtpos[-1] - 'A';
                    *varaddr = *varaddr + f->step;
                    // Use a different test depending on the sign of the step increment
                    if ((f->step > 0 && *varaddr <= f->terminal) || (f->step < 0 && *varaddr >= f->terminal))
                    {
                        // We have to loop so don't pop the stack
                        mem.txtpos = f->txtpos;
                        mem.current_line = f->current_line;
                        goto run_next_statement;
                    }
                    // We've run to the end of the loop. drop out of the loop, popping the stack
                    mem.sp = mem.tempsp + sizeof(struct stack_for_frame);
                    goto run_next_statement;
                }
            }
            // This is not the loop you are looking for... so Walk back up the stack
            mem.tempsp += sizeof(struct stack_for_frame);
            break;
        default:
            //printf("Stack is stuffed!\n");
            goto warmstart;
        }
    }
    // Didn't find the variable we've been looking for
    goto qhow;

assignment:
{
    short int value;
    short int *var;

    if (mem.isNotAlpha())
        goto qhow;
    var = (short int *)mem.variables_begin + *mem.txtpos - 'A';
    mem.txtpos++;

    mem.ignore_blanks();

    if (*mem.txtpos != '=')
        goto qwhat;
    mem.txtpos++;
    mem.ignore_blanks();
    value = mem.expression();
    if (mem.expression_error)
        goto qwhat;
    // Check that we are at the end of the statement
    if (*mem.txtpos != NL && *mem.txtpos != ':')
        goto qwhat;
    *var = value;
}
    goto run_next_statement;
poke:
{
    short int value;
    unsigned char *address;

    // Work out where to put it
    mem.expression_error = 0;
    value = mem.expression();
    if (mem.expression_error)
        goto qwhat;
    address = (unsigned char *)value;

    // check for a comma
    mem.ignore_blanks();
    if (*mem.txtpos != ',')
        goto qwhat;
    mem.txtpos++;
    mem.ignore_blanks();

    // Now get the value to assign
    value = mem.expression();
    if (mem.expression_error)
        goto qwhat;
    //printf("Poke %p value %i\n",address, (unsigned char)value);
    // Check that we are at the end of the statement
    if (*mem.txtpos != NL && *mem.txtpos != ':')
        goto qwhat;
}
    goto run_next_statement;

list:
    mem.linenum = mem.testnum(); // Retuns 0 if no line found.

    // Should be EOL
    if (mem.txtpos[0] != NL)
        goto qwhat;

    // Find the line
    mem.list_line = mem.findline();
    while (mem.list_line != mem.program_end)
        IO.printline();
    goto warmstart;

print:
    // If we have an empty list then just put out a NL
    if (*mem.txtpos == ':')
    {
        IO.line_terminator();
        mem.txtpos++;
        goto run_next_statement;
    }
    if (*mem.txtpos == NL)
    {
        goto execnextline;
    }

    while (1)
    {
        mem.ignore_blanks();
        if (IO.print_quoted_string())
        {
            ;
        }
        else if (*mem.txtpos == '"' || *mem.txtpos == '\'')
            goto qwhat;
        else
        {
            short int e = mem.expression();
            if (mem.expression_error)
                goto qwhat;
            IO.printnum(e);
        }

        // At this point we have three options, a comma or a new line
        if (*mem.txtpos == ',')
            mem.txtpos++; // Skip the comma and move onto the next
        else if (mem.txtpos[0] == ';' && (mem.txtpos[1] == NL || mem.txtpos[1] == ':'))
        {
            mem.txtpos++; // This has to be the end of the print - no newline
            break;
        }
        else if (*mem.txtpos == NL || *mem.txtpos == ':')
        {
            IO.line_terminator(); // The end of the print statement
            break;
        }
        else
            goto qwhat;
    }
    goto run_next_statement;

mem:
    // memory free
    IO.printnum(mem.free_mem());
    IO.printmsg(memorymsg);
#ifdef ARDUINO
#ifdef ENABLE_EEPROM
    {
        // eprom size
        IO.printnum(E2END + 1);
        IO.printmsg(eeprommsg);

        // figure out the memory usage;
        val = ' ';
        int i;
        for (i = 0; (i < (E2END + 1)) && (val != '\0'); i++)
        {
            val = EEPROM.read(i);
        }
        IO.printnum((E2END + 1) - (i - 1));

        IO.printmsg(eepromamsg);
    }
#endif /* ENABLE_EEPROM */
#endif /* ARDUINO */
    goto run_next_statement;

    /*************************************************/

#ifdef ARDUINO
awrite: // AWRITE <pin>,val
dwrite:
{
    short int pinNo;
    short int value;
    unsigned char *txtposBak;

    // Get the pin number
    pinNo = mem.expression();
    if (mem.expression_error)
        goto qwhat;

    // check for a comma
    mem.ignore_blanks();
    if (*mem.txtpos != ',')
        goto qwhat;
    mem.txtpos++;
    mem.ignore_blanks();

    txtposBak = mem.txtpos;
    mem.scantable(highlow_tab);
    if (mem.table_index != HIGHLOW_UNKNOWN)
    {
        if (mem.table_index <= HIGHLOW_HIGH)
        {
            value = 1;
        }
        else
        {
            value = 0;
        }
    }
    else
    {

        // and the value (numerical)
        value = mem.expression();
        if (mem.expression_error)
            goto qwhat;
    }
    pinMode(pinNo, OUTPUT);
    if (isDigital)
    {
        digitalWrite(pinNo, value);
    }
    else
    {
        analogWrite(pinNo, value);
    }
}
    goto run_next_statement;
#else
pinmode: // PINMODE <pin>, I/O
awrite:  // AWRITE <pin>,val
dwrite:
    goto unimplemented;
#endif

    /*************************************************/
files:
    // display a listing of files on the device.
    // version 1: no support for subdirectories

#ifdef ENABLE_FILEIO
    cmd_Files();
    goto warmstart;
#else
    goto unimplemented;
#endif // ENABLE_FILEIO

chain:
    runAfterLoad = true;

load:
    // clear the program
    mem.program_reset();

    // load from a file into memory
#ifdef ENABLE_FILEIO
    {
        unsigned char *filename;

        // Work out the filename
        mem.expression_error = 0;
        filename = filenameWord();
        if (mem.expression_error)
            goto qwhat;

#ifdef ARDUINO
        // Arduino specific
        if (!SD.exists((char *)filename))
        {
            printmsg(sdfilemsg);
        }
        else
        {

            fp = SD.open((const char *)filename);
            inStream = kStreamFile;
            inhibitOutput = true;
        }
#else   // ARDUINO
            // Desktop specific
#endif  // ARDUINO
        // this will kickstart a series of events to read in from the file.
    }
    goto warmstart;
#else  // ENABLE_FILEIO
    goto unimplemented;
#endif // ENABLE_FILEIO

save:
    // save from memory out to a file
#ifdef ENABLE_FILEIO
{
    unsigned char *filename;

    // Work out the filename
    mem.expression_error = 0;
    filename = filenameWord();
    if (mem.expression_error)
        goto qwhat;

#ifdef ARDUINO
    // remove the old file if it exists
    if (SD.exists((char *)filename))
    {
        SD.remove((char *)filename);
    }

    // open the file, switch over to file output
    fp = SD.open((const char *)filename, FILE_WRITE);
    outStream = kStreamFile;

    // copied from "List"
    list_line = mem.findline();
    while (mem.list_line != mem.program_end)
        mem.printline();

    // go back to standard output, close the file
    outStream = kStreamSerial;

    fp.close();
#else  // ARDUINO
        // desktop
#endif // ARDUINO
    goto warmstart;
}
#else  // ENABLE_FILEIO
    goto unimplemented;
#endif // ENABLE_FILEIO

rseed:
{
    short int value = mem.expression();
    if (mem.expression_error)
        goto qwhat;

#ifdef ARDUINO
    randomSeed(value);
#else  // ARDUINO
    srand(value);
#endif // ARDUINO
    goto run_next_statement;
}

#ifdef ENABLE_TONES
tonestop:
    noTone(kPiezoPin);
    goto run_next_statement;

tonegen:
{
    // TONE freq, duration
    // if either are 0, tones turned off

    //Get the frequency
    short int freq = mem.expression();
    if (mem.expression_error)
        goto qwhat;

    mem.ignore_blanks();
    if (*mem.txtpos != ',')
        goto qwhat;
    mem.txtpos++;
    mem.ignore_blanks();

    //Get the duration
    short int duration = mem.expression();
    if (mem.expression_error)
        goto qwhat;

    if (freq == 0 || duration == 0)
        goto tonestop;

    tone(kPiezoPin, freq, duration);
    if (alsoWait)
    {
        delay(duration);
        alsoWait = false;
    }
    goto run_next_statement;
}
#endif /* ENABLE_TONES */
}

// returns 1 if the character is valid in a filename
static int isValidFnChar(char c)
{
    if (c >= '0' && c <= '9')
        return 1; // number
    if (c >= 'A' && c <= 'Z')
        return 1; // LETTER
    if (c >= 'a' && c <= 'z')
        return 1; // letter (for completeness)
    if (c == '_')
        return 1;
    if (c == '+')
        return 1;
    if (c == '.')
        return 1;
    if (c == '~')
        return 1; // Window~1.txt

    return 0;
}

unsigned char *filenameWord(void)
{
    // SDL - I wasn't sure if this functionality existed above, so I figured i'd put it here
    unsigned char *ret = mem.txtpos;
    mem.expression_error = 0;

    // make sure there are no quotes or spaces, search for valid characters
    //while(*txtpos == SPACE || *txtpos == TAB || *txtpos == SQUOTE || *txtpos == DQUOTE ) txtpos++;
    while (!isValidFnChar(*mem.txtpos))
        mem.txtpos++;
    ret = mem.txtpos;

    if (*ret == '\0')
    {
        mem.expression_error = 1;
        return ret;
    }

    // now, find the next nonfnchar
    mem.txtpos++;
    while (isValidFnChar(*mem.txtpos))
        mem.txtpos++;
    if (mem.txtpos != ret)
        *mem.txtpos = '\0';

    // set the error code if we've got no string
    if (*ret == '\0')
    {
        mem.expression_error = 1;
    }

    return ret;
}

/***********************************************************/
void setup()
{
#ifdef ARDUINO
    Serial.begin(kConsoleBaud); // opens serial port
    while (!Serial)
        ; // for Leonardo

    IO.printmsg(initmsg);

#ifdef ENABLE_FILEIO
    initSD();

#ifdef ENABLE_AUTORUN
    if (SD.exists(kAutorunFilename))
    {
        program_end = program_start;
        fp = SD.open(kAutorunFilename);
        inStream = kStreamFile;
        inhibitOutput = true;
        runAfterLoad = true;
    }
#endif /* ENABLE_AUTORUN */

#endif /* ENABLE_FILEIO */

#ifdef ENABLE_EEPROM
#ifdef ENABLE_EAUTORUN
    // read the first byte of the eeprom. if it's a number, assume it's a program we can load
    int val = EEPROM.read(0);
    if (val >= '0' && val <= '9')
    {
        mem.program_end = mem.program_start;
        IO.inStream = streamioClass::streamType::kStreamEEProm;
        eepos = 0;
        inhibitOutput = true;
        runAfterLoad = true;
    }
#endif /* ENABLE_EAUTORUN */
#endif /* ENABLE_EEPROM */

#endif /* ARDUINO */
}

/***********************************************************/
/* SD Card helpers */

#if ARDUINO && ENABLE_FILEIO

static int initSD(void)
{
    // if the card is already initialized, we just go with it.
    // there is no support (yet?) for hot-swap of SD Cards. if you need to
    // swap, pop the card, reset the arduino.)

    if (sd_is_initialized == true)
        return kSD_OK;

    // due to the way the SD Library works, pin 10 always needs to be
    // an output, even when your shield uses another line for CS
    pinMode(10, OUTPUT); // change this to 53 on a mega

    if (!SD.begin(kSD_CS))
    {
        // failed
        printmsg(sderrormsg);
        return kSD_Fail;
    }
    // success - quietly return 0
    sd_is_initialized = true;

    // and our file redirection flags
    outStream = kStreamSerial;
    inStream = kStreamSerial;
    inhibitOutput = false;

    return kSD_OK;
}
#endif

#if ENABLE_FILEIO
void cmd_Files(void)
{
    File dir = SD.open("/");
    dir.seek(0);

    while (true)
    {
        File entry = dir.openNextFile();
        if (!entry)
        {
            entry.close();
            break;
        }

        // common header
        printmsgNoNL(indentmsg);
        printmsgNoNL((const unsigned char *)entry.name());
        if (entry.isDirectory())
        {
            printmsgNoNL(slashmsg);
        }

        if (entry.isDirectory())
        {
            // directory ending
            for (int i = strlen(entry.name()); i < 16; i++)
            {
                printmsgNoNL(spacemsg);
            }
            printmsgNoNL(dirextmsg);
        }
        else
        {
            // file ending
            for (int i = strlen(entry.name()); i < 17; i++)
            {
                printmsgNoNL(spacemsg);
            }
            printUnum(entry.size());
        }
        line_terminator();
        entry.close();
    }
    dir.close();
}
#endif
