set "avr=C:\Program Files (x86)\Arduino\hardware\tools\avr\bin\"
set "dump=%avr%avr-objdump"
set build_dir=/projects/ArduinoBasic/bin
"%dump%" -Pmem-usage %build_dir%/%1.elf