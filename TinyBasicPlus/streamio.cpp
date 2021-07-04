/// @file
/// IO management class implementation.
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

#include "streamio.h"

void streamioClass::printnum(int num)
{
    int digits = 0;

    if (num < 0)
    {
        num = -num;
        outchar('-');
    }
    do
    {
        pushb(num % 10 + '0');
        num = num / 10;
        digits++;
    } while (num > 0);

    while (digits > 0)
    {
        outchar(popb());
        digits--;
    }
}

void streamioClass::printUnum(unsigned int num)
{
    int digits = 0;

    do
    {
        pushb(num % 10 + '0');
        num = num / 10;
        digits++;
    } while (num > 0);

    while (digits > 0)
    {
        outchar(popb());
        digits--;
    }
}

unsigned char streamioClass::print_quoted_string(void)
{
    int i = 0;
    unsigned char delim = *mem.txtpos;
    if (delim != '"' && delim != '\'')
        return 0;
    mem.txtpos++;

    // Check we have a closing delimiter
    while (mem.txtpos[i] != delim)
    {
        if (mem.txtpos[i] == NL)
            return 0;
        i++;
    }

    // Print the characters
    while (*mem.txtpos != delim)
    {
        outchar(*mem.txtpos);
        mem.txtpos++;
    }
    mem.txtpos++; // Skip over the last delimiter

    return 1;
}

void streamioClass::printmsgNoNL(const unsigned char *msg)
{
    while (pgm_read_byte(msg) != 0)
    {
        outchar(pgm_read_byte(msg++));
    };
}

void streamioClass::printmsg(const unsigned char *msg)
{
    printmsgNoNL(msg);
    line_terminator();
}

void streamioClass::getln(char prompt)
{
    outchar(prompt);
    mem.txtpos = mem.program_end + sizeof(LINENUM);

    while (1)
    {
        char c = inchar();
        switch (c)
        {
        case NL:
            //break;
        case CR:
            line_terminator();
            // Terminate all strings with a NL
            mem.txtpos[0] = NL;
            return;
        case CTRLH:
            if (mem.txtpos == mem.program_end)
                break;
            mem.txtpos--;

            printmsg(backspacemsg);
            break;
        default:
            // We need to leave at least one space to allow us to shuffle the line into order
            if (mem.txtpos == mem.variables_begin - 2)
                outchar(BELL);
            else
            {
                mem.txtpos[0] = c;
                mem.txtpos++;
                outchar(c);
            }
        }
    }
}

void streamioClass::printline()
{
    LINENUM line_num;

    line_num = *((LINENUM *)(mem.list_line));
    mem.list_line += sizeof(LINENUM) + sizeof(char);

    // Output the line */
    printnum(line_num);
    outchar(' ');
    while (*mem.list_line != NL)
    {
        outchar(*mem.list_line);
        mem.list_line++;
    }
    mem.list_line++;
#ifdef ALIGN_MEMORY
    // Start looking for next line on even page
    if (ALIGN_UP(list_line) != list_line)
        mem.list_line++;
#endif
    line_terminator();
}

void streamioClass::line_terminator(void)
{
    outchar(CR);
    outchar(NL);
}

int streamioClass::inchar()
{
    int v;
#ifdef ARDUINO

    switch (inStream)
    {
    case (streamType::kStreamFile):
#ifdef ENABLE_FILEIO
        v = fp.read();
        if (v == NL)
            v = CR; // file translate
        if (!fp.available())
        {
            fp.close();
            goto inchar_loadfinish;
        }
        return v;
#else
#endif
        break;
    case (streamType::kStreamEEProm):
#ifdef ENABLE_EEPROM
#ifdef ARDUINO
        v = EEPROM.read(eepos++);
        if (v == '\0')
        {
            goto inchar_loadfinish;
        }
        return v;
#endif
#else
        inStream = streamType::kStreamSerial;
        return NL;
#endif
        break;
    case (streamType::kStreamSerial):
    default:
        while (1)
        {
            if (Serial.available())
                return Serial.read();
        }
    }

inchar_loadfinish:
    inStream = streamType::kStreamSerial;
    inhibitOutput = false;

    if (runAfterLoad)
    {
        runAfterLoad = false;
        triggerRun = true;
    }
    return NL; // trigger a prompt.

#else
    // otherwise. desktop!
    int got = getchar();

    // translation for desktop systems
    if (got == LF)
        got = CR;

    return got;
#endif
}

void streamioClass::outchar(unsigned char c)
{
    if (inhibitOutput)
        return;

#ifdef ARDUINO
#ifdef ENABLE_FILEIO
    if (outStream == kStreamFile)
    {
        // output to a file
        fp.write(c);
    }
    else
#endif
#ifdef ARDUINO
#ifdef ENABLE_EEPROM
        if (outStream == streamType::kStreamEEProm)
    {
        EEPROM.write(eepos++, c);
    }
    else
#endif /* ENABLE_EEPROM */
#endif /* ARDUINO */
        Serial.write(c);

#else
    putchar(c);
#endif
}

void streamioClass::pushb(unsigned char b)
{
    mem.sp--;
    *mem.sp = b;
}

unsigned char streamioClass::popb()
{
    unsigned char b;
    b = *mem.sp;
    mem.sp++;
    return b;
}

unsigned char streamioClass::breakcheck(void)
{
#ifdef ARDUINO
    if (Serial.available())
        return Serial.read() == CTRLC;
    return 0;
#else
#ifdef __CONIO__
    if (kbhit())
        return getch() == CTRLC;
    else
#endif
        return 0;
#endif
}

void streamioClass::outchar_printable(unsigned char val)
{
    if (((val < ' ') || (val > '~')) && (val != NL) && (val != CR))
    {
        outchar('?');
    }
    else
    {
        outchar(val);
    }
}