
For ARDUINO-based projects, "ArincShieldAutoTest.ino" is the sole software file required.
Download it in an arduino UNO and you are done !

Nearly as simple in Nucleo world :
* upload "nucleo_main.cpp" (rename it "main.cpp") and "mbed_app.json" in your mbed account,
* compile and download into a nucleo board
* look at the trace messages sent by the nucleo with e.g. hyperterminal

**NB**: on nucleo 144 boards, there is a conflict between SPI/MOSI pin and Ethernet interface.
Using a nucleo board with NAV429 is obviously for implementing some kind of ethernet<=>arinc bridge. So this is to be solved.
The workaround is given [here](https://os.mbed.com/teams/ST/wiki/Nucleo-144pins-ethernet-spi-conflict).
When both ethernet and SPI are used simultaneously (this is the case for an ethernet<=>arinc bridge), a jumper modification is to be performed, as well as a configuration adaptation.
Please note that this github repository contains the required "mbed_app.json" file.
