/// @file 
/// static messages for ArduinoBASIC.
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


#ifndef _STRINGS_H_
#define _STRINGS_H_

#include "platform.h"

#define kVersion "v0.16"

static const unsigned char okmsg[]            PROGMEM = "Ok.";
static const unsigned char whatmsg[]          PROGMEM = "Syntax error: ";
static const unsigned char howmsg[]           PROGMEM =	"How?";
static const unsigned char sorrymsg[]         PROGMEM = "Sorry!";
static const unsigned char initmsg[]          PROGMEM = " ** Casasoft Arduino BASIC " kVersion " **";
static const unsigned char memorymsg[]        PROGMEM = " bytes free.";
#ifdef ARDUINO
#ifdef ENABLE_EEPROM
static const unsigned char eeprommsg[]        PROGMEM = " EEProm bytes total.";
static const unsigned char eepromamsg[]       PROGMEM = " EEProm bytes available.";
#endif
#endif
static const unsigned char breakmsg[]         PROGMEM = "break!";
static const unsigned char unimplimentedmsg[] PROGMEM = "Unimplemented.";
static const unsigned char backspacemsg[]     PROGMEM = "\b \b";
static const unsigned char indentmsg[]        PROGMEM = "    ";
static const unsigned char sderrormsg[]       PROGMEM = "SD card error.";
static const unsigned char sdfilemsg[]        PROGMEM = "SD file error.";
static const unsigned char dirextmsg[]        PROGMEM = "(dir)";
static const unsigned char slashmsg[]         PROGMEM = "/";
static const unsigned char spacemsg[]         PROGMEM = " ";


#endif