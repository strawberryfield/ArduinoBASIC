/// @file 
/// platform configuration for ArduinoBASIC.
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


#ifndef _PLATFORM_H_
#define _PLATFORM_H_

////////////////////////////////////////////////////////////////////////////////
// Feature option configuration...

// This enables LOAD, SAVE, FILES commands through the Arduino SD Library
// it adds 9k of usage as well.
//#define ENABLE_FILEIO 1
#undef ENABLE_FILEIO

// this turns on "autorun".  if there's FileIO, and a file "autorun.bas",
// then it will load it and run it when starting up
//#define ENABLE_AUTORUN 1
#undef ENABLE_AUTORUN
// and this is the file that gets run
#define kAutorunFilename  "autorun.bas"

// this is the alternate autorun.  Autorun the program in the eeprom.
// it will load whatever is in the EEProm and run it
#define ENABLE_EAUTORUN 1
//#undef ENABLE_EAUTORUN

// this will enable the "TONE", "NOTONE" command using a piezo
// element on the specified pin.  Wire the red/positive/piezo to the kPiezoPin,
// and the black/negative/metal disc to ground.
// it adds 1.5k of usage as well.
//#define ENABLE_TONES 1
#undef ENABLE_TONES
#define kPiezoPin 5

// we can use the EEProm to store a program during powerdown.  This is 
// 1kbyte on the '328, and 512 bytes on the '168.  Enabling this here will
// allow for this funcitonality to work.  Note that this only works on AVR
// arduino.  Disable it for DUE/other devices.
#define ENABLE_EEPROM 1
//#undef ENABLE_EEPROM

// Sometimes, we connect with a slower device as the console.
// Set your console D0/D1 baud rate here (9600 baud default)
#define kConsoleBaud 9600


////////////////////////////////////////////////////////////////////////////////
// fixes for RAMEND on some platforms
#ifndef RAMEND
  // RAMEND is defined for Uno type Arduinos
  #ifdef ARDUINO
    // probably DUE or 8266?
    #ifdef ESP8266
      #define RAMEND (8192-1)
    #else
      // probably DUE - ARM rather than AVR
      #define RAMEND (4096-1)
    #endif
  #endif
#endif


// Enable memory alignment for certain processers (e.g. some ESP8266-based devices)
#ifdef ESP8266
  // Uses up to one extra byte per program line of memory
  #define ALIGN_MEMORY 1
#else
  #undef ALIGN_MEMORY
#endif

#ifndef ARDUINO
  // not an arduino, so we can disable these features.
  // turn off EEProm
  #undef ENABLE_EEPROM
  #undef ENABLE_TONES
#endif


// includes, and settings for Arduino-specific features
#ifdef ARDUINO

  // EEPROM
  #ifdef ENABLE_EEPROM
    #include <EEPROM.h>  /* NOTE: case sensitive */
    int eepos = 0;
  #endif

  // SD card File io
  #ifdef ENABLE_FILEIO
    #include <SD.h>
    #include <SPI.h> /* needed as of 1.5 beta */

    // set this to the card select for your Arduino SD shield
    #define kSD_CS 10

    #define kSD_Fail  0
    #define kSD_OK    1

    File fp;
  #endif

  // set up our RAM buffer size for program and user input
  // NOTE: This number will have to change if you include other libraries.
  //       It is also an estimation.  Might require adjustments...
  #ifdef ENABLE_FILEIO
    #define kRamFileIO (1030) /* approximate */
  #else
    #define kRamFileIO (0)
  #endif

  #ifdef ENABLE_TONES
    #define kRamTones (40)
  #else
    #define kRamTones (0)
  #endif

  #define kRamSize  (RAMEND - 1160 - kRamFileIO - kRamTones) 

#endif /* ARDUINO Specifics */


// set up file includes for things we need, or desktop specific stuff.

#ifdef ARDUINO
  // Use pgmspace/PROGMEM directive to store strings in progmem to save RAM
  #include <avr/pgmspace.h>
#else
  #include <stdio.h>
  #include <stdlib.h>
  #undef ENABLE_TONES

  // size of our program ram
  #define kRamSize   64*1024 /* arbitrary - not dependant on libraries */

  #ifdef ENABLE_FILEIO
    FILE * fp;
  #endif
#endif

////////////////////

// memory alignment
//  necessary for some esp8266-based devices
#ifdef ALIGN_MEMORY
  // Align memory addess x to an even page
  #define ALIGN_UP(x) ((unsigned char*)(((unsigned int)(x + 1) >> 1) << 1))
  #define ALIGN_DOWN(x) ((unsigned char*)(((unsigned int)x >> 1) << 1))
#else
  #define ALIGN_UP(x) x
  #define ALIGN_DOWN(x) x
#endif


////////////////////
// various other desktop-tweaks and such.

#ifndef boolean 
  #define boolean int
  #define true 1
  #define false 0
#endif

#ifndef byte
  typedef unsigned char byte;
#endif

// some catches for AVR based text string stuff...
#ifndef PROGMEM
  #define PROGMEM
#endif
#ifndef pgm_read_byte
  #define pgm_read_byte( A ) *(A)
#endif

////////////////////

#ifdef ENABLE_FILEIO
  // functions defined elsehwere
  void cmd_Files( void );
  unsigned char * filenameWord(void);
  static boolean sd_is_initialized = false;
#endif


#endif