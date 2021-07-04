/// @file
/// IO management class definition.
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

#ifndef _STREAMIO_H_
#define _STREAMIO_H_

#include "Arduino.h"
#include "globals.h"
#include "strings.h"

class streamioClass
{
private:
    void pushb(unsigned char b);
    unsigned char popb();

public:
    /** these will select, at runtime, where IO happens through for load/save */
    enum class streamType
    {
        kStreamSerial = 0,
        kStreamEEProm,
        kStreamFile
    };

    streamType inStream = streamType::kStreamSerial;
    streamType outStream = streamType::kStreamSerial;

    void printnum(int num);
    void printUnum(unsigned int num);
    unsigned char print_quoted_string(void);
    void printmsgNoNL(const unsigned char *msg);
    void printmsg(const unsigned char *msg);
    void getln(char prompt);
    void printline();
    void line_terminator(void);
    int inchar();
    void outchar(unsigned char c);
    unsigned char breakcheck(void);
};

#endif