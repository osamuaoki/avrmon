# `avrmon`: Monitor for ATmega328P, ATmega32u4, etc.

(C) 2020    Osamu Aoki <osamu@debian.org>, GPL 2.0+

This is a command line shell program for AVR MCU to investigate bad wiring
around the AVR MCU from the host PC's terminal window.  The terminal program
needs to support a few control codes: CR LF BS Up-escape-sequence

For the VT-100 terminal program, `picocom` on Debian/Ubuntu is recommended.

There is no loop nor conditional branching capability so this is not a real
interpreter language environment.  This is intentionally made simple by not
using interrupt service routine.  Please consider this as a platform to build a
test system.

## Command line

Command can be typed in the lower case (echoed in the upper case)

### command syntax

```
a1 a2     a3     action
R  addr0  addr1  read sram and print (binary hex ~hex ascii)
RS addr0  addr1  read sram and print (hexdump ascii)
RP addr0  addr1  read program memory and print (hexdump ascii)
RE addr0  addr1  read eeprom and print (binary hex ~hex ascii)
W  val    addr   write sram and verify result: `=`
WA val    addr   write sram as and-value and verify result: `&=`
WO val    addr   write sram as or-value and verify result: `|=`

MASK             set mask values via dialog
D                display all digital ports (DDR/PORT/PIN-ABC)
M                monitor digital input ports
P M              pre-set digital ports for monitor
S                scan keyboard matrix
P S              pre-set digital ports for scan
A                monitor analog input (PC0-PC6 ADC7 ADC8) accumulated.
A F              Analog input off
L                LED on
L F              LED off
L B word         LED blink val (unit 100 ms)
? val            print 8 bit value
?? word          print 16 bit value
```
Here, mask is used limit I/O register bits to be monitored and scanned.

### 8 bit value input syntax

* `[~](%[01]{1,8}|([0-9][a-f]){1,2})`
    * hexadecimal as default
    * `~` flips bit values.
    * `%` indicates radux 2 (binary)


### 16 bit address and word input syntax

* `[~](%[01]{1,16}|([0-9][a-f]){1,4}|@some_mnemonic)`
    * hexadecimal as default
    * `-` negative/substruct value
    * `+` positive/add value
    * `@` mneminic name of I/O register, e.g., `@ddrb`

## Makefile

This source comes with a customized WinAvr Makefile.

* `make reformat`: reformat C source
* `make term`: start `picocom`
* `make run`: compile C source, program MCU, start terminal

For ATmega328P, compile with:

```
 $ make clean; make MCU=atmega328p
```

For ATmega32u4, compile with:

```
 $ make clean; make MCU=atmega32u4
```

