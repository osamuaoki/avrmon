# `avrmon`: Monitor for ATmega328P, ATmega32u4, etc.

(C) 2020 - 2021    Osamu Aoki <osamu@debian.org>, GPL 2.0+

This is a command line shell program for AVR MCU to investigate bad wiring
around the AVR MCU from the host PC's terminal window.  AVR MCU appears as a
serial device from your host PC.  The terminal program needs to support a few
control codes: CR LF BS Up-escape-sequence

For the VT-100 terminal program, `picocom` on Debian/Ubuntu is recommended.

There is no loop nor conditional branching capability so this is not a real
interpreter language environment.  This is intentionally made simple by not
using interrupt service routine.  Please consider this as a platform to build a
test system.

## Command line

Command can be typed in the lower case (echoed in the upper case)

### command syntax

Here, MASK is used limit I/O register bits to be monitored and scanned.

* val:    8 bit number
* word:  16 bit number
* addr0: 16 bit number
* addr1: 16 bit number
* pin:   A0, A1, ...,F7,?
* para:  0,...,?
* `**` indicates they run continuously until a key is pressed.
* `XX` indicates missing features
* `.` for addr gives the default sram address (incremented)
* `,` for addr gives the last sram address
* `>` for addr gives the default flash address (incremented)
* `<` for addr gives the last flash address

```
a1 a2     a3     action
R  addr0  addr1  read a sram block and print (hexdump w/ ascii)
RA addr0  addr1  read a sram byte and print (alldump: binary hex ~hex ascii)
RP addr0  addr1  read a program memory block and print (hexdump w/ ascii)
RE addr0  addr1  read a eeprom byte and print (alldump: binary hex ~hex ascii) XXX
W  addr   val    write a sram byte              and verify result: `=`
WA addr   val    write a sram byte as and-value and verify result: `&=`
WO addr   val    write a sram byte as or-value  and verify result: `|=`
WP addr   val    write a program memory byte and verify result: `=`            XXX
WE addr   val    write a eeprom byte and verify result: `=`                    XXX

D                Display all DDR/PORT/PIN/MASK (A,B,C,...)
DC               Display all changed PIN states (A,B,C,...) (1 ms) **

S                Set all INPUT and set default MASK (initial default)
SK               Set some pins to OUTPUT pins and set default MASK
SM               Set MASK values (manual dialog)
SMD              Set to apply MASK for display disabled
SME              Set to apply MASK for display enabled
SOH              Set all active OUTPUT pins (MASK bit=1) to 1 (high)
SOL              Set all active OUTPUT pins (MASK bit=1) to 0 (low)
SIP              Set all active INPUT pins (MASK bit=1) to 1 (pull-up)
SIH              ,, (alias)
SIT              Set all active INPUT pins (MASK bit=1) to 0 (tri-state)
SIL              ,, (alias)
SPD              Set PUD (Pull-Up Disabled) of MCU to 1 (Pull-up disabled)
SPE              Set PUD (Pull-Up Disabled) of MCU to 0 (Pull-up may be enabled)

B                Toggle the BIT to 0/1 (O), Start record (I)
B pin mode       Set a BIT pin to mode=OH/OL/IH/IL (defailt: LED pin and OL)
BL               Set the BIT to 0 (low) (O)
BH               Set the BIT to 1 (high) (O)
BTU word         Set time unit in ms (1) (I/O)
BTS tspan tcount Set time span and trigger stability count (0x8000 5) (I/O)
BTX var1 var2    Set time unit for LED pixel driver (MPU loops) (5 15)
BP               Print recorded data (time val pair) (-)
BB word          Blink the BIT with specified word (5) (unit 100 ms) (O) **
BW var           PWM wave of BIT 0=bright 3=50/50 (O)
BX               Send LED data, (? for print)
BX led_data      Set LED data (GRB-sequence in FFFFFF-like or .R.G.B.W-like)

AR para          Set analog reference source.   para=0,1,3,?
AP para          Set analog prescaler.          para=1..7,?
A                Monitor all available analog inputs, accumulated.
AX               Analog input off

? val            print  8 bit value (calculator)
?? word          print 16 bit value (calculator)
```


### 8 bit value input syntax

* `[~](%[01]{1,8}|([0-9][a-f]){1,2})`
    * hexadecimal as default
    * `~` flips bit values.
    * `%` indicates radix 2 (binary)

### 16 bit address and word input syntax

* `{.|,|<|>|(-|+|)%[01]{1,16}|([0-9][a-f]){1,4}|@some_mnemonic|}...`
    * hexadecimal as default
    * `.` current sram address pointer
    * `,` last sram address pointer
    * `>` current flash address pointer
    * `<` last flash address pointer
    * `-` negative / subtract value
    * `+` positive / add value
    * `@` mnemonic name of I/O register, e.g., `@ddrb`

## Makefile

This source comes with a customized WinAvr Makefile.

* `make reformat`: reformat C source
* `make term`: start `picocom`
* `make run`: compile C source, program MCU, start terminal

For ATmega328P on Arduino Nano/Arduino Uno, compile and program with AVRISP
mkII via ISP:

```
 $ make BOARD=nano PROGRAMMER=avrisp2 program
```

For ATmega32u4 on teensy 2.0, compile and program with AVRISP
mkII via ISP:

```
 $ make BOARD=teensy2 PROGRAMMER=avrisp2 program
```

For AT90USB1286 on teensy 2.0++, compile and program with AVRISP
mkII via ISP:

```
 $ make BOARD=teensy2pp PROGRAMMER=avrisp2 program
```

If using Arduino programmer, substitute as `PROGRAMMER=arduino`.

If using LUFA HID programmer, substitute as `PROGRAMMER=hid`.

Usually, the last 2 are easier.

## Neopixel support (TODO)

https://github.com/cpldcpu/light_ws2812
