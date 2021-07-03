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

char eliminateCompileErrors = 1;  // fix to suppress arduino build errors

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

streamioClass IO;

static boolean inhibitOutput = false;
static boolean runAfterLoad = false;
static boolean triggerRun = false;

static unsigned char program[kRamSize];
static unsigned char *txtpos,*list_line, *tmptxtpos;
static unsigned char expression_error;
static unsigned char *tempsp;

static unsigned char *stack_limit;
static unsigned char *program_start;
static unsigned char *program_end;
static unsigned char *stack; // Software stack for things that should go on the CPU stack
static unsigned char *variables_begin;
static unsigned char *current_line;
static unsigned char *sp;

static unsigned char table_index;
static LINENUM linenum;


static short int expression(void);
static unsigned char breakcheck(void);


/***************************************************************************/
static void ignore_blanks(void)
{
  while(*txtpos == SPACE || *txtpos == TAB)
    txtpos++;
}


/***************************************************************************/
static void scantable(const unsigned char *table)
{
  int i = 0;
  table_index = 0;
  while(1)
  {
    // Run out of table entries?
    if(pgm_read_byte( table ) == 0)
      return;

    // Do we match this character?
    if(txtpos[i] == pgm_read_byte( table ))
    {
      i++;
      table++;
    }
    else
    {
      // do we match the last character of keywork (with 0x80 added)? If so, return
      if(txtpos[i]+0x80 == pgm_read_byte( table ))
      {
        txtpos += i+1;  // Advance the pointer to following the keyword
        ignore_blanks();
        return;
      }

      // Forward to the end of this keyword
      while((pgm_read_byte( table ) & 0x80) == 0)
        table++;

      // Now move on to the first character of the next word, and reset the position index
      table++;
      table_index++;
      ignore_blanks();
      i = 0;
    }
  }
}


/***************************************************************************/
static unsigned short testnum(void)
{
  unsigned short num = 0;
  ignore_blanks();

  while(*txtpos>= '0' && *txtpos <= '9' )
  {
    // Trap overflows
    if(num >= 0xFFFF/10)
    {
      num = 0xFFFF;
      break;
    }

    num = num *10 + *txtpos - '0';
    txtpos++;
  }
  return	num;
}

/***************************************************************************/
static unsigned char *findline(void)
{
  unsigned char *line = program_start;
  while(1)
  {
    if(line == program_end)
      return line;

    if(((LINENUM *)line)[0] >= linenum)
      return line;

    // Add the line length onto the current address, to get to the next line;
    line += line[sizeof(LINENUM)];
  }
}


/***************************************************************************/
static void toUppercaseBuffer(void)
{
  unsigned char *c = program_end+sizeof(LINENUM);
  unsigned char quote = 0;

  while(*c != NL)
  {
    // Are we in a quoted string?
    if(*c == quote)
      quote = 0;
    else if(*c == '"' || *c == '\'')
      quote = *c;
    else if(quote == 0 && *c >= 'a' && *c <= 'z')
      *c = *c + 'A' - 'a';
    c++;
  }
}

/***************************************************************************/
static short int expr4(void)
{
  // fix provided by Jurg Wullschleger wullschleger@gmail.com
  // fixes whitespace and unary operations
  ignore_blanks();

  if( *txtpos == '-' ) {
    txtpos++;
    return -expr4();
  }
  // end fix

  if(*txtpos == '0')
  {
    txtpos++;
    return 0;
  }

  if(*txtpos >= '1' && *txtpos <= '9')
  {
    short int a = 0;
    do 	{
      a = a*10 + *txtpos - '0';
      txtpos++;
    } 
    while(*txtpos >= '0' && *txtpos <= '9');
    return a;
  }

  // Is it a function or variable reference?
  if(txtpos[0] >= 'A' && txtpos[0] <= 'Z')
  {
    short int a;
    // Is it a variable reference (single alpha)
    if(txtpos[1] < 'A' || txtpos[1] > 'Z')
    {
      a = ((short int *)variables_begin)[*txtpos - 'A'];
      txtpos++;
      return a;
    }

    // Is it a function with a single parameter
    scantable(func_tab);
    if(table_index == FUNC_UNKNOWN)
      goto expr4_error;

    unsigned char f = table_index;

    if(*txtpos != '(')
      goto expr4_error;

    txtpos++;
    a = expression();
    if(*txtpos != ')')
      goto expr4_error;
    txtpos++;
    switch(f)
    {
    case FUNC_PEEK:
      return program[a];
      
    case FUNC_ABS:
      if(a < 0) 
        return -a;
      return a;

#ifdef ARDUINO
    case FUNC_AREAD:
      pinMode( a, INPUT );
      return analogRead( a );                        
    case FUNC_DREAD:
      pinMode( a, INPUT );
      return digitalRead( a );
#endif

    case FUNC_RND:
#ifdef ARDUINO
      return( random( a ));
#else
      return( rand() % a );
#endif
    }
  }

  if(*txtpos == '(')
  {
    short int a;
    txtpos++;
    a = expression();
    if(*txtpos != ')')
      goto expr4_error;

    txtpos++;
    return a;
  }

expr4_error:
  expression_error = 1;
  return 0;

}

/***************************************************************************/
static short int expr3(void)
{
  short int a,b;

  a = expr4();

  ignore_blanks(); // fix for eg:  100 a = a + 1

  while(1)
  {
    if(*txtpos == '*')
    {
      txtpos++;
      b = expr4();
      a *= b;
    }
    else if(*txtpos == '/')
    {
      txtpos++;
      b = expr4();
      if(b != 0)
        a /= b;
      else
        expression_error = 1;
    }
    else
      return a;
  }
}

/***************************************************************************/
static short int expr2(void)
{
  short int a,b;

  if(*txtpos == '-' || *txtpos == '+')
    a = 0;
  else
    a = expr3();

  while(1)
  {
    if(*txtpos == '-')
    {
      txtpos++;
      b = expr3();
      a -= b;
    }
    else if(*txtpos == '+')
    {
      txtpos++;
      b = expr3();
      a += b;
    }
    else
      return a;
  }
}
/***************************************************************************/
static short int expression(void)
{
  short int a,b;

  a = expr2();

  // Check if we have an error
  if(expression_error)	return a;

  scantable(relop_tab);
  if(table_index == RELOP_UNKNOWN)
    return a;

  switch(table_index)
  {
  case RELOP_GE:
    b = expr2();
    if(a >= b) return 1;
    break;
  case RELOP_NE:
  case RELOP_NE_BANG:
    b = expr2();
    if(a != b) return 1;
    break;
  case RELOP_GT:
    b = expr2();
    if(a > b) return 1;
    break;
  case RELOP_EQ:
    b = expr2();
    if(a == b) return 1;
    break;
  case RELOP_LE:
    b = expr2();
    if(a <= b) return 1;
    break;
  case RELOP_LT:
    b = expr2();
    if(a < b) return 1;
    break;
  }
  return 0;
}

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
  noTone( kPiezoPin );
#endif
#endif

  program_start = program;
  program_end = program_start;
  sp = program+sizeof(program);  // Needed for printnum
#ifdef ALIGN_MEMORY
  // Ensure these memory blocks start on even pages
  stack_limit = ALIGN_DOWN(program+sizeof(program)-STACK_SIZE);
  variables_begin = ALIGN_DOWN(stack_limit - 27*VAR_SIZE);
#else
  stack_limit = program+sizeof(program)-STACK_SIZE;
  variables_begin = stack_limit - 27*VAR_SIZE;
#endif

  // memory free
  IO.printnum(variables_begin-program_end);
  IO.printmsg(memorymsg);
#ifdef ARDUINO
#ifdef ENABLE_EEPROM
  // eprom size
  IO.printnum( E2END+1 );
  IO.printmsg( eeprommsg );
#endif /* ENABLE_EEPROM */
#endif /* ARDUINO */

warmstart:
  // this signifies that it is running in 'direct' mode.
  current_line = 0;
  sp = program+sizeof(program);
  IO.printmsg(okmsg);

prompt:
  if( triggerRun ){
    triggerRun = false;
    current_line = program_start;
    goto execline;
  }

  IO.getln( '>' );
  toUppercaseBuffer();
  txtpos = program_end+sizeof(unsigned short);

  // Find the end of the freshly entered line
  while(*txtpos != NL)
    txtpos++;

  // Move it to the end of program_memory
  {
    unsigned char *dest;
    dest = variables_begin-1;
    while(1)
    {
      *dest = *txtpos;
      if(txtpos == program_end+sizeof(unsigned short))
        break;
      dest--;
      txtpos--;
    }
    txtpos = dest;
  }

  // Now see if we have a line number
  linenum = testnum();
  ignore_blanks();
  if(linenum == 0)
    goto direct;

  if(linenum == 0xFFFF)
    goto qhow;

  // Find the length of what is left, including the (yet-to-be-populated) line header
  linelen = 0;
  while(txtpos[linelen] != NL)
    linelen++;
  linelen++; // Include the NL in the line length
  linelen += sizeof(unsigned short)+sizeof(char); // Add space for the line number and line length

  // Now we have the number, add the line header.
  txtpos -= 3;

#ifdef ALIGN_MEMORY
  // Line starts should always be on 16-bit pages
  if (ALIGN_DOWN(txtpos) != txtpos)
  {
    txtpos--;
    linelen++;
    // As the start of the line has moved, the data should move as well
    unsigned char *tomove;
    tomove = txtpos + 3;
    while (tomove < txtpos + linelen - 1)
    {
      *tomove = *(tomove + 1);
      tomove++;
    }
  }
#endif

  *((unsigned short *)txtpos) = linenum;
  txtpos[sizeof(LINENUM)] = linelen;


  // Merge it into the rest of the program
  start = findline();

  // If a line with that number exists, then remove it
  if(start != program_end && *((LINENUM *)start) == linenum)
  {
    unsigned char *dest, *from;
    unsigned tomove;

    from = start + start[sizeof(LINENUM)];
    dest = start;

    tomove = program_end - from;
    while( tomove > 0)
    {
      *dest = *from;
      from++;
      dest++;
      tomove--;
    }	
    program_end = dest;
  }

  if(txtpos[sizeof(LINENUM)+sizeof(char)] == NL) // If the line has no txt, it was just a delete
    goto prompt;



  // Make room for the new line, either all in one hit or lots of little shuffles
  while(linelen > 0)
  {	
    unsigned int tomove;
    unsigned char *from,*dest;
    unsigned int space_to_make;

    space_to_make = txtpos - program_end;

    if(space_to_make > linelen)
      space_to_make = linelen;
    newEnd = program_end+space_to_make;
    tomove = program_end - start;


    // Source and destination - as these areas may overlap we need to move bottom up
    from = program_end;
    dest = newEnd;
    while(tomove > 0)
    {
      from--;
      dest--;
      *dest = *from;
      tomove--;
    }

    // Copy over the bytes into the new space
    for(tomove = 0; tomove < space_to_make; tomove++)
    {
      *start = *txtpos;
      txtpos++;
      start++;
      linelen--;
    }
    program_end = newEnd;
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
  if(current_line != NULL)
  {
    unsigned char tmp = *txtpos;
    if(*txtpos != NL)
      *txtpos = '^';
    list_line = current_line;
    IO.printline();
    *txtpos = tmp;
  }
  IO.line_terminator();
  goto prompt;

qsorry:	
  IO.printmsg(sorrymsg);
  goto warmstart;

run_next_statement:
  while(*txtpos == ':')
    txtpos++;
  ignore_blanks();
  if(*txtpos == NL)
    goto execnextline;
  goto interperateAtTxtpos;

direct: 
  txtpos = program_end+sizeof(LINENUM);
  if(*txtpos == NL)
    goto prompt;

interperateAtTxtpos:
  if(breakcheck())
  {
    IO.printmsg(breakmsg);
    goto warmstart;
  }

  scantable(keywords);

  switch(table_index)
  {
  case KW_DELAY:
    {
#ifdef ARDUINO
      expression_error = 0;
      val = expression();
      delay( val );
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
    if(txtpos[0] != NL)
      goto qwhat;
    program_end = program_start;
    goto prompt;
  case KW_RUN:
    current_line = program_start;
    goto execline;
  case KW_SAVE:
    goto save;
  case KW_NEXT:
    goto next;
  case KW_LET:
    goto assignment;
  case KW_IF:
    short int val;
    expression_error = 0;
    val = expression();
    if(expression_error || *txtpos == NL)
      goto qhow;
    if(val != 0)
      goto interperateAtTxtpos;
    goto execnextline;

  case KW_GOTO:
    expression_error = 0;
    linenum = expression();
    if(expression_error || *txtpos != NL)
      goto qhow;
    current_line = findline();
    goto execline;

  case KW_GOSUB:
    goto gosub;
  case KW_RETURN:
    goto gosub_return; 
  case KW_REM:
  case KW_QUOTE:
    goto execnextline;	// Ignore line completely
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
    if(txtpos[0] != NL)
      goto qwhat;
    current_line = program_end;
    goto execline;
  case KW_BYE:
    // Leave the basic interperater
    return;

  case KW_AWRITE:  // AWRITE <pin>, HIGH|LOW
    isDigital = false;
    goto awrite;
  case KW_DWRITE:  // DWRITE <pin>, HIGH|LOW
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
  if(current_line == NULL)		// Processing direct commands?
    goto prompt;
  current_line +=	 current_line[sizeof(LINENUM)];

execline:
  if(current_line == program_end) // Out of lines to run
    goto warmstart;
  txtpos = current_line+sizeof(LINENUM)+sizeof(char);
  goto interperateAtTxtpos;

#ifdef ARDUINO
#ifdef ENABLE_EEPROM
elist:
  {
    int i;
    for( i = 0 ; i < (E2END +1) ; i++ )
    {
      val = EEPROM.read( i );

      if( val == '\0' ) {
        goto execnextline;
      }

      if( ((val < ' ') || (val  > '~')) && (val != NL) && (val != CR))  {
        IO.outchar( '?' );
      } 
      else {
        IO.outchar( val );
      }
    }
  }
  goto execnextline;

eformat:
  {
    for( int i = 0 ; i < E2END ; i++ )
    {
      if( (i & 0x03f) == 0x20 ) IO.outchar( '.' );
      EEPROM.write( i, 0 );
    }
    IO.outchar( LF );
  }
  goto execnextline;

esave:
  {
    IO.outStream = streamioClass::streamType::kStreamEEProm;
    eepos = 0;

    // copied from "List"
    list_line = findline();
    while(list_line != program_end) {
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
  program_end = program_start;

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
    ignore_blanks();
    if(*txtpos < 'A' || *txtpos > 'Z')
      goto qwhat;
    var = *txtpos;
    txtpos++;
    ignore_blanks();
    if(*txtpos != NL && *txtpos != ':')
      goto qwhat;
inputagain:
    tmptxtpos = txtpos;
    IO.getln( '?' );
    toUppercaseBuffer();
    txtpos = program_end+sizeof(unsigned short);
    ignore_blanks();
    expression_error = 0;
    value = expression();
    if(expression_error)
      goto inputagain;
    ((short int *)variables_begin)[var-'A'] = value;
    txtpos = tmptxtpos;

    goto run_next_statement;
  }

forloop:
  {
    unsigned char var;
    short int initial, step, terminal;
    ignore_blanks();
    if(*txtpos < 'A' || *txtpos > 'Z')
      goto qwhat;
    var = *txtpos;
    txtpos++;
    ignore_blanks();
    if(*txtpos != '=')
      goto qwhat;
    txtpos++;
    ignore_blanks();

    expression_error = 0;
    initial = expression();
    if(expression_error)
      goto qwhat;

    scantable(to_tab);
    if(table_index != 0)
      goto qwhat;

    terminal = expression();
    if(expression_error)
      goto qwhat;

    scantable(step_tab);
    if(table_index == 0)
    {
      step = expression();
      if(expression_error)
        goto qwhat;
    }
    else
      step = 1;
    ignore_blanks();
    if(*txtpos != NL && *txtpos != ':')
      goto qwhat;


    if(!expression_error && *txtpos == NL)
    {
      struct stack_for_frame *f;
      if(sp + sizeof(struct stack_for_frame) < stack_limit)
        goto qsorry;

      sp -= sizeof(struct stack_for_frame);
      f = (struct stack_for_frame *)sp;
      ((short int *)variables_begin)[var-'A'] = initial;
      f->frame_type = STACK_FOR_FLAG;
      f->for_var = var;
      f->terminal = terminal;
      f->step     = step;
      f->txtpos   = txtpos;
      f->current_line = current_line;
      goto run_next_statement;
    }
  }
  goto qhow;

gosub:
  expression_error = 0;
  linenum = expression();
  if(!expression_error && *txtpos == NL)
  {
    struct stack_gosub_frame *f;
    if(sp + sizeof(struct stack_gosub_frame) < stack_limit)
      goto qsorry;

    sp -= sizeof(struct stack_gosub_frame);
    f = (struct stack_gosub_frame *)sp;
    f->frame_type = STACK_GOSUB_FLAG;
    f->txtpos = txtpos;
    f->current_line = current_line;
    current_line = findline();
    goto execline;
  }
  goto qhow;

next:
  // Fnd the variable name
  ignore_blanks();
  if(*txtpos < 'A' || *txtpos > 'Z')
    goto qhow;
  txtpos++;
  ignore_blanks();
  if(*txtpos != ':' && *txtpos != NL)
    goto qwhat;

gosub_return:
  // Now walk up the stack frames and find the frame we want, if present
  tempsp = sp;
  while(tempsp < program+sizeof(program)-1)
  {
    switch(tempsp[0])
    {
    case STACK_GOSUB_FLAG:
      if(table_index == KW_RETURN)
      {
        struct stack_gosub_frame *f = (struct stack_gosub_frame *)tempsp;
        current_line	= f->current_line;
        txtpos			= f->txtpos;
        sp += sizeof(struct stack_gosub_frame);
        goto run_next_statement;
      }
      // This is not the loop you are looking for... so Walk back up the stack
      tempsp += sizeof(struct stack_gosub_frame);
      break;
    case STACK_FOR_FLAG:
      // Flag, Var, Final, Step
      if(table_index == KW_NEXT)
      {
        struct stack_for_frame *f = (struct stack_for_frame *)tempsp;
        // Is the the variable we are looking for?
        if(txtpos[-1] == f->for_var)
        {
          short int *varaddr = ((short int *)variables_begin) + txtpos[-1] - 'A'; 
          *varaddr = *varaddr + f->step;
          // Use a different test depending on the sign of the step increment
          if((f->step > 0 && *varaddr <= f->terminal) || (f->step < 0 && *varaddr >= f->terminal))
          {
            // We have to loop so don't pop the stack
            txtpos = f->txtpos;
            current_line = f->current_line;
            goto run_next_statement;
          }
          // We've run to the end of the loop. drop out of the loop, popping the stack
          sp = tempsp + sizeof(struct stack_for_frame);
          goto run_next_statement;
        }
      }
      // This is not the loop you are looking for... so Walk back up the stack
      tempsp += sizeof(struct stack_for_frame);
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

    if(*txtpos < 'A' || *txtpos > 'Z')
      goto qhow;
    var = (short int *)variables_begin + *txtpos - 'A';
    txtpos++;

    ignore_blanks();

    if (*txtpos != '=')
      goto qwhat;
    txtpos++;
    ignore_blanks();
    expression_error = 0;
    value = expression();
    if(expression_error)
      goto qwhat;
    // Check that we are at the end of the statement
    if(*txtpos != NL && *txtpos != ':')
      goto qwhat;
    *var = value;
  }
  goto run_next_statement;
poke:
  {
    short int value;
    unsigned char *address;

    // Work out where to put it
    expression_error = 0;
    value = expression();
    if(expression_error)
      goto qwhat;
    address = (unsigned char *)value;

    // check for a comma
    ignore_blanks();
    if (*txtpos != ',')
      goto qwhat;
    txtpos++;
    ignore_blanks();

    // Now get the value to assign
    expression_error = 0;
    value = expression();
    if(expression_error)
      goto qwhat;
    //printf("Poke %p value %i\n",address, (unsigned char)value);
    // Check that we are at the end of the statement
    if(*txtpos != NL && *txtpos != ':')
      goto qwhat;
  }
  goto run_next_statement;

list:
  linenum = testnum(); // Retuns 0 if no line found.

  // Should be EOL
  if(txtpos[0] != NL)
    goto qwhat;

  // Find the line
  list_line = findline();
  while(list_line != program_end)
    IO.printline();
  goto warmstart;

print:
  // If we have an empty list then just put out a NL
  if(*txtpos == ':' )
  {
    IO.line_terminator();
    txtpos++;
    goto run_next_statement;
  }
  if(*txtpos == NL)
  {
    goto execnextline;
  }

  while(1)
  {
    ignore_blanks();
    if(IO.print_quoted_string())
    {
      ;
    }
    else if(*txtpos == '"' || *txtpos == '\'')
      goto qwhat;
    else
    {
      short int e;
      expression_error = 0;
      e = expression();
      if(expression_error)
        goto qwhat;
      IO.printnum(e);
    }

    // At this point we have three options, a comma or a new line
    if(*txtpos == ',')
      txtpos++;	// Skip the comma and move onto the next
    else if(txtpos[0] == ';' && (txtpos[1] == NL || txtpos[1] == ':'))
    {
      txtpos++; // This has to be the end of the print - no newline
      break;
    }
    else if(*txtpos == NL || *txtpos == ':')
    {
      IO.line_terminator();	// The end of the print statement
      break;
    }
    else
      goto qwhat;	
  }
  goto run_next_statement;

mem:
  // memory free
  IO.printnum(variables_begin-program_end);
  IO.printmsg(memorymsg);
#ifdef ARDUINO
#ifdef ENABLE_EEPROM
  {
    // eprom size
    IO.printnum( E2END+1 );
    IO.printmsg( eeprommsg );
    
    // figure out the memory usage;
    val = ' ';
    int i;   
    for( i=0 ; (i<(E2END+1)) && (val != '\0') ; i++ ) {
      val = EEPROM.read( i );    
    }
    IO.printnum( (E2END +1) - (i-1) );
    
    IO.printmsg( eepromamsg );
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
    expression_error = 0;
    pinNo = expression();
    if(expression_error)
      goto qwhat;

    // check for a comma
    ignore_blanks();
    if (*txtpos != ',')
      goto qwhat;
    txtpos++;
    ignore_blanks();


    txtposBak = txtpos; 
    scantable(highlow_tab);
    if(table_index != HIGHLOW_UNKNOWN)
    {
      if( table_index <= HIGHLOW_HIGH ) {
        value = 1;
      } 
      else {
        value = 0;
      }
    } 
    else {

      // and the value (numerical)
      expression_error = 0;
      value = expression();
      if(expression_error)
        goto qwhat;
    }
    pinMode( pinNo, OUTPUT );
    if( isDigital ) {
      digitalWrite( pinNo, value );
    } 
    else {
      analogWrite( pinNo, value );
    }
  }
  goto run_next_statement;
#else
pinmode: // PINMODE <pin>, I/O
awrite: // AWRITE <pin>,val
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
  program_end = program_start;

  // load from a file into memory
#ifdef ENABLE_FILEIO
  {
    unsigned char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if(expression_error)
      goto qwhat;

#ifdef ARDUINO
    // Arduino specific
    if( !SD.exists( (char *)filename ))
    {
      printmsg( sdfilemsg );
    } 
    else {

      fp = SD.open( (const char *)filename );
      inStream = kStreamFile;
      inhibitOutput = true;
    }
#else // ARDUINO
    // Desktop specific
#endif // ARDUINO
    // this will kickstart a series of events to read in from the file.

  }
  goto warmstart;
#else // ENABLE_FILEIO
  goto unimplemented;
#endif // ENABLE_FILEIO



save:
  // save from memory out to a file
#ifdef ENABLE_FILEIO
  {
    unsigned char *filename;

    // Work out the filename
    expression_error = 0;
    filename = filenameWord();
    if(expression_error)
      goto qwhat;

#ifdef ARDUINO
    // remove the old file if it exists
    if( SD.exists( (char *)filename )) {
      SD.remove( (char *)filename );
    }

    // open the file, switch over to file output
    fp = SD.open( (const char *)filename, FILE_WRITE );
    outStream = kStreamFile;

    // copied from "List"
    list_line = findline();
    while(list_line != program_end)
      printline();

    // go back to standard output, close the file
    outStream = kStreamSerial;

    fp.close();
#else // ARDUINO
    // desktop
#endif // ARDUINO
    goto warmstart;
  }
#else // ENABLE_FILEIO
  goto unimplemented;
#endif // ENABLE_FILEIO

rseed:
  {
    short int value;

    //Get the pin number
    expression_error = 0;
    value = expression();
    if(expression_error)
      goto qwhat;

#ifdef ARDUINO
    randomSeed( value );
#else // ARDUINO
    srand( value );
#endif // ARDUINO
    goto run_next_statement;
  }

#ifdef ENABLE_TONES
tonestop:
  noTone( kPiezoPin );
  goto run_next_statement;

tonegen:
  {
    // TONE freq, duration
    // if either are 0, tones turned off
    short int freq;
    short int duration;

    //Get the frequency
    expression_error = 0;
    freq = expression();
    if(expression_error)
      goto qwhat;

    ignore_blanks();
    if (*txtpos != ',')
      goto qwhat;
    txtpos++;
    ignore_blanks();


    //Get the duration
    expression_error = 0;
    duration = expression();
    if(expression_error)
      goto qwhat;

    if( freq == 0 || duration == 0 )
      goto tonestop;

    tone( kPiezoPin, freq, duration );
    if( alsoWait ) {
      delay( duration );
      alsoWait = false;
    }
    goto run_next_statement;
  }
#endif /* ENABLE_TONES */
}

// returns 1 if the character is valid in a filename
static int isValidFnChar( char c )
{
  if( c >= '0' && c <= '9' ) return 1; // number
  if( c >= 'A' && c <= 'Z' ) return 1; // LETTER
  if( c >= 'a' && c <= 'z' ) return 1; // letter (for completeness)
  if( c == '_' ) return 1;
  if( c == '+' ) return 1;
  if( c == '.' ) return 1;
  if( c == '~' ) return 1;  // Window~1.txt

  return 0;
}

unsigned char * filenameWord(void)
{
  // SDL - I wasn't sure if this functionality existed above, so I figured i'd put it here
  unsigned char * ret = txtpos;
  expression_error = 0;

  // make sure there are no quotes or spaces, search for valid characters
  //while(*txtpos == SPACE || *txtpos == TAB || *txtpos == SQUOTE || *txtpos == DQUOTE ) txtpos++;
  while( !isValidFnChar( *txtpos )) txtpos++;
  ret = txtpos;

  if( *ret == '\0' ) {
    expression_error = 1;
    return ret;
  }

  // now, find the next nonfnchar
  txtpos++;
  while( isValidFnChar( *txtpos )) txtpos++;
  if( txtpos != ret ) *txtpos = '\0';

  // set the error code if we've got no string
  if( *ret == '\0' ) {
    expression_error = 1;
  }

  return ret;
}


/***********************************************************/
void setup()
{
#ifdef ARDUINO
  Serial.begin(kConsoleBaud);	// opens serial port
  while( !Serial ); // for Leonardo
  
  IO.printmsg(initmsg);

#ifdef ENABLE_FILEIO
  initSD();
  
#ifdef ENABLE_AUTORUN
  if( SD.exists( kAutorunFilename )) {
    program_end = program_start;
    fp = SD.open( kAutorunFilename );
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
  if( val >= '0' && val <= '9' ) {
    program_end = program_start;
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
static unsigned char breakcheck(void)
{
#ifdef ARDUINO
  if(Serial.available())
    return Serial.read() == CTRLC;
  return 0;
#else
#ifdef __CONIO__
  if(kbhit())
    return getch() == CTRLC;
  else
#endif
    return 0;
#endif
}

/***********************************************************/
/* SD Card helpers */

#if ARDUINO && ENABLE_FILEIO

static int initSD( void )
{
  // if the card is already initialized, we just go with it.
  // there is no support (yet?) for hot-swap of SD Cards. if you need to 
  // swap, pop the card, reset the arduino.)

  if( sd_is_initialized == true ) return kSD_OK;

  // due to the way the SD Library works, pin 10 always needs to be 
  // an output, even when your shield uses another line for CS
  pinMode(10, OUTPUT); // change this to 53 on a mega

  if( !SD.begin( kSD_CS )) {
    // failed
    printmsg( sderrormsg );
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
void cmd_Files( void )
{
  File dir = SD.open( "/" );
  dir.seek(0);

  while( true ) {
    File entry = dir.openNextFile();
    if( !entry ) {
      entry.close();
      break;
    }

    // common header
    printmsgNoNL( indentmsg );
    printmsgNoNL( (const unsigned char *)entry.name() );
    if( entry.isDirectory() ) {
      printmsgNoNL( slashmsg );
    }

    if( entry.isDirectory() ) {
      // directory ending
      for( int i=strlen( entry.name()) ; i<16 ; i++ ) {
        printmsgNoNL( spacemsg );
      }
      printmsgNoNL( dirextmsg );
    }
    else {
      // file ending
      for( int i=strlen( entry.name()) ; i<17 ; i++ ) {
        printmsgNoNL( spacemsg );
      }
      printUnum( entry.size() );
    }
    line_terminator();
    entry.close();
  }
  dir.close();
}
#endif
