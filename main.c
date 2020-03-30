// vim: ft=c sts=2 sw=2 et si ai:
//
// AVR monitor program: avrmon
//
// Copyright 2020 - 2021 Osamu Aoki <osamu@debian.org>
//
// License: GPL 2.0+
//
// Hardware configuration (Makefile -> config.h.in)
//
#include "config.h"
#include "sub.h"
//
// C headers (C99 with GNU extension)
//
#include <ctype.h>
#include <stdint.h>
#include <string.h>
//
// AVR hardware headers
//
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <util/setbaud.h>

//
// Special MACROs defined in <avr/sfr_defs.h> called from <avr/io.h>
//
// #define _BV(bit) (1 << (bit))
//
// mem_addr is a number -- used in io*.h to define SFR register names
//
// #define _MMIO_BYTE(mem_addr) (*(volatile uint8_t *)(mem_addr))
// #define _SFR_MEM8(mem_addr) _MMIO_BYTE(mem_addr)
//
// sfr is a SFR register name string
//
// #define _SFR_MEM_ADDR(sfr) ((uint16_t) &(sfr))
// #define _SFR_ADDR(sfr) _SFR_MEM_ADDR(sfr)
// #define _SFR_BYTE(sfr) _MMIO_BYTE(_SFR_ADDR(sfr))
//
/////////////////////////////////////////////////////////////////////////////
//
// AVR monitor terminal communication
//
/////////////////////////////////////////////////////////////////////////////
// Global variables
uint16_t addr_sram;       // sram address: referred by "."
uint16_t addr_sram_last;  // sram address: referred by ","
uint16_t addr_flash;      // flash address: referred by ">"
uint16_t addr_flash_last; // flash address: referred by "<"
// bit operation to sram (default=LED pin)
uint16_t addr_pin;   // pin  for bit operation: sram address set by name (A-F)
uint16_t addr_ddr;   // ddr  for bit operation: sram address set by name (A-F)
uint16_t addr_port;  // port for bit operation: sram address set by name (A-F)
uint8_t addr_bit;    // bit for bit operation:  sram bit set by name (0-7)
uint8_t addr_mask;    // byte operation for mask index: (0-...)
// mask settings (active monitor bit = 1, ignore change bit = 0)
uint8_t mask[N_PORTS];
uint8_t mask_override;  // 0 or 0xff
// common through-away input buffer (space saving)
char sm[BUFSIZE]; // used by most excluding main dialog loop
// data record buffer
uint16_t data[DATASIZE];
uint16_t tspan = 0x800; // time span (multiple of unit time)
uint16_t unit = 1; // delay unit time in ms
uint8_t tcount = 5; // minimum count to trigger
uint8_t uunit1 = 5; // delay unit for MCU loop
uint8_t uunit3 = 15; // delay unit for MCU loop
uint8_t t_pwm = 3; // PWM duty 50/50
/////////////////////////////////////////////////////////////////////////////
//
// Convert string to uint16_t
//
// (.|(([+-]|)[0-9a-fA-F]{1,4})+
//
// Supported:
//    abs. hexadecimal                   F402
//    abs. hexadecimal uint16_t add/sub  +100-10+FF-1
//    rel. hexadecimal uint16_t add/sub  .+100
//    This also access current address pointers by , . < >
//
/////////////////////////////////////////////////////////////////////////////
static uint16_t str2word(char *s) {
  uint16_t m = 0; // return
  uint16_t n;
  uint8_t f;
  if (*s == '@') {  // mnemonic starting with '@'
    s++;
    if (!strncmp_P(s, PSTR("PIN"), 3)) {
      if (*(s + 3) >= 'A' && *(s + 3) <= 'F') {
        m = 3 * (*(s + 3) - 'A') + 0x20;
      } else {
        m = 0x20; // PINA
      }
    } else if (!strncmp_P(s, PSTR("DDR"), 3)) {
      if (*(s + 3) >= 'A' && *(s + 3) <= 'F') {
        m = 3 * (*(s + 3) - 'A') + 0x21;
      } else {
        m = 0x21; // DDRA
      }
    } else if (!strncmp_P(s, PSTR("PORT"), 4)) {
      if (*(s + 4) >= 'A' && *(s + 4) <= 'F') {
        m = 3 * (*(s + 4) - 'A') + 0x22;
      } else {
        m = 0x22; // PORTA
      }
#ifdef VERBOSE
    } else if (!strncmp_P(s, PSTR("TIFR"), 4)) {
      if (*(s + 4) >= '0' && *(s + 4) <= '3') {
        m = 3 * (*(s + 4) - '0') + 0x35;
      } else {
        m = 0x35; // TIFR0
      }
    } else if (!strcmp_P(s, PSTR("PCIFR"))) {
      m = 0x3b;
    } else if (!strcmp_P(s, PSTR("EIFR"))) {
      m = 0x3c;
    } else if (!strcmp_P(s, PSTR("EIMSK"))) {
      m = 0x3d;
    } else if (!strcmp_P(s, PSTR("GPIR0"))) {
      m = 0x3e;
    } else if (!strcmp_P(s, PSTR("EECR"))) {
      m = 0x3f;
    } else if (!strcmp_P(s, PSTR("EEDR"))) {
      m = 0x40;
    } else if (!strcmp_P(s, PSTR("EEARL"))) {
      m = 0x41;
    } else if (!strcmp_P(s, PSTR("EEARH"))) {
      m = 0x42;
    } else if (!strcmp_P(s, PSTR("GTCCR"))) {
      m = 0x43;
    } else if (!strcmp_P(s, PSTR("TCCR0A"))) {
      m = 0x44;
    } else if (!strcmp_P(s, PSTR("TCCR0B"))) {
      m = 0x45;
    } else if (!strcmp_P(s, PSTR("TCNT0"))) {
      m = 0x46;
    } else if (!strcmp_P(s, PSTR("OCR0A"))) {
      m = 0x47;
    } else if (!strcmp_P(s, PSTR("OCR0B"))) {
      m = 0x48;
    } else if (!strcmp_P(s, PSTR("PLLCSR"))) {
      m = 0x49;
    } else if (!strcmp_P(s, PSTR("GPIOR1"))) {
      m = 0x4a;
    } else if (!strcmp_P(s, PSTR("GPIOR2"))) {
      m = 0x4b;
    } else if (!strcmp_P(s, PSTR("SPCR"))) {
      m = 0x4c;
    } else if (!strcmp_P(s, PSTR("SPSR"))) {
      m = 0x4d;
    } else if (!strcmp_P(s, PSTR("SPDR"))) {
      m = 0x4e;
    } else if (!strcmp_P(s, PSTR("ACSR"))) {
      m = 0x50;
    } else if (!strcmp_P(s, PSTR("OCDR"))) {
      m = 0x51;
    } else if (!strcmp_P(s, PSTR("MONDR"))) {
      m = 0x51;
    } else if (!strcmp_P(s, PSTR("SMCR"))) {
      m = 0x53;
    } else if (!strcmp_P(s, PSTR("MCUSR"))) {
      m = 0x54;
    } else if (!strcmp_P(s, PSTR("MCUCR"))) {
      m = 0x55;
    } else if (!strcmp_P(s, PSTR("SPMCSR"))) {
      m = 0x57;
    } else if (!strcmp_P(s, PSTR("RPMZ"))) {
      m = 0x5b;
    } else if (!strcmp_P(s, PSTR("SPL"))) {
      m = 0x5d;
    } else if (!strcmp_P(s, PSTR("SPH"))) {
      m = 0x5e;
    } else if (!strcmp_P(s, PSTR("SREG"))) {
      m = 0x5f;
    } else if (!strcmp_P(s, PSTR("WDTCSR"))) {
      m = 0x60;
    } else if (!strcmp_P(s, PSTR("CLKPR"))) {
      m = 0x61;
    } else if (!strcmp_P(s, PSTR("PRR0"))) {
      m = 0x64;
    } else if (!strcmp_P(s, PSTR("PRR1"))) {
      m = 0x65;
    } else if (!strcmp_P(s, PSTR("OSCCAL"))) {
      m = 0x66;
    } else if (!strcmp_P(s, PSTR("PCICR"))) {
      m = 0x68;
    } else if (!strcmp_P(s, PSTR("EICRA"))) {
      m = 0x69;
    } else if (!strcmp_P(s, PSTR("EICRB"))) {
      m = 0x6a;
    } else if (!strcmp_P(s, PSTR("PCMSK0"))) {
      m = 0x6b;
    } else if (!strncmp_P(s, PSTR("TIMSK"), 5)) {
      if (*(s + 5) >= '0' && *(s + 5) <= '3') {
        m = 3 * (*(s + 5) - '0') + 0x6e;
      } else {
        m = 0x6e; // TIMSK0
      }
    } else if (!strcmp_P(s, PSTR("ADCL"))) {
      m = 0x78;
    } else if (!strcmp_P(s, PSTR("ADCH"))) {
      m = 0x79;
    } else if (!strcmp_P(s, PSTR("ADCSRA"))) {
      m = 0x7a;
    } else if (!strcmp_P(s, PSTR("ADCSRB"))) {
      m = 0x7b;
    } else if (!strcmp_P(s, PSTR("ADMUX"))) {
      m = 0x7c;
    } else if (!strcmp_P(s, PSTR("DIDR0"))) {
      m = 0x7e;
    } else if (!strcmp_P(s, PSTR("DIDR1"))) {
      m = 0x7f;
    } else if (!strncmp_P(s, PSTR("TCCR1"), 5)) {
      if (*(s + 5) >= 'A' && *(s + 5) <= 'C') {
        m = 3 * (*(s + 5) - 'A') + 0x80;
      } else {
        m = 0x80; // TCCR1A
      }
      // Give up after 0x84
#endif  // VERBOSE
    } else {
      m = 0;
    }
  } else { // non-nmemonic = number
    do {
      f = 0;
      if (*s == '-') {  // minus
        s++;
        f = 1;
      } else if (*s == '+') {
        s++;
      } else if (*s == '.') {  // sram pointer address
        s++;
        m += addr_sram;
      } else if (*s == ',') {  // last sram pointer address
        s++;
        m += addr_sram_last;
      } else if (*s == '>') {  // flash pointer address
        s++;
        m += addr_flash;
      } else if (*s == '<') {  // last flash pointer address
        s++;
        m += addr_flash_last;
      }
      n = 0;
      for (uint8_t i = 4; i > 0; --i) {
        if (*s == '\0') break;
        if (*s == '+' || *s == '-') break;
        if (*s >= '0' && *s <= '9') {
          n = n << 4;
          n |= *s - '0';
          s++;
        } else if (*s >= 'A' && *s <= 'F') {
          n = n << 4;
          n |= *s - 'A' + 10;
          s++;
        } else {
          *s = '\0'; // wacky char to stop
          break;
        }
      }
      m += f ? -n : n; // do add/sub math
    } while (*s != 0);
  }
  return m;
}
/////////////////////////////////////////////////////////////////////////////
//
// AVR HW monitor and control
//
/////////////////////////////////////////////////////////////////////////////
void data_dump(void) {
  uint16_t i; // data[] pointer
  char *buf1;
  char *buf2;
  if ((data[0] == 0x0f00) || (data[0] == 0x0f00)) {
    print_sP(PSTR("BIT DUMP "));
    if (data[0] == 0x0f00) {
      print_sP(PSTR("L->H triggered\n"));
      buf1 = "L->H";
      buf2 = "H->L";
    } else {
      print_sP(PSTR("H->L triggered\n"));
      buf1 = "H->L";
      buf2 = "L->H";
    }
    for (i=1; i < DATASIZE; i ++) {
      if (data[i] == 0xffff) break;
      print_hex4(data[i]);
      print_sP(PSTR(" "));
      if (i % 2) {
        print_s(buf1);
      } else {
        print_s(buf2);
      }
      print_crlf();
    }
    print_sP(PSTR("BIT DUMP END\n"));
  } else if (data[0] == 0x8888) {
    print_sP(PSTR("BYTE DUMP\n"));
    for (i=1; i < DATASIZE; i ++) {
      if (data[i] == 0xffff) break;
      print_hex4(i);
      print_sP(PSTR(": "));
      print_bin8((uint8_t) data[i], mask[addr_mask]);
      print_crlf();
    }
  } else {
    print_sP(PSTR("BROKEN BIT/BYTE DUMP DATA\n"));
  }
}

void bit_pin(char *pin, char *mode) {
  uint16_t port;
  // *pin -> "A6" etc. / initialize with NULL or LED_PIN
  if (pin == NULL) {
    pin = LED_PIN ; // if NULL or invalid, set default as LED
    mode = NULL;
  }
  if (mode == NULL) mode = "OL" ; // if NULL, set default as output low
  print_sP(PSTR("I/O_pin="));
  if (pin[0] >= PORT_BGN_CH && pin[0] <= PORT_END_CH) {
    port = pin[0] - PORT_BGN_CH;
    if (port < PORT_BGN_CH && port > PORT_END_CH) port = PORT_BGN_CH;
    addr_pin = _SFR_ADDR(PIN_0) + 3 * port; // PIN address
    addr_ddr = _SFR_ADDR(DDR_0) + 3 * port; // DDR address
    addr_port = _SFR_ADDR(PORT_0) + 3 * port; // PORT address
    addr_mask = port; // mask address (really an index)
    addr_bit = (pin[1] - '0') & 0x7; // bit 0-8
    if (!strcmp_P(mode, PSTR("IH")) || !strcmp_P(mode, PSTR("IP"))) {
      _SFR_MEM8(addr_ddr) &= ~_BV(addr_bit); // set for input
      _SFR_MEM8(addr_port) |= _BV(addr_bit); // set for (high) or pull up
    } else if (!strcmp_P(mode, PSTR("IL")) || !strcmp_P(mode, PSTR("IT"))) {
      _SFR_MEM8(addr_ddr) &= ~_BV(addr_bit); // set for input
      _SFR_MEM8(addr_port) &= ~_BV(addr_bit); // set for (low) or tri-state
    } else if (!strcmp_P(mode, PSTR("OH"))) {
      _SFR_MEM8(addr_ddr) |= _BV(addr_bit); // set for output
      _SFR_MEM8(addr_port) |= _BV(addr_bit); // set for high or (pull up)
    } else if (!strcmp_P(mode, PSTR("OL"))) {
      _SFR_MEM8(addr_ddr) |= _BV(addr_bit); // set for output
      _SFR_MEM8(addr_port) &= ~_BV(addr_bit); // set for low or (tri-state)
    } else {
      print_sP(PSTR("\nInvalid mode: "));
      print_s(mode);
      print_crlf();
    }
  } else if ((pin[0] != '?') && (pin[0] != '/') &&
             (pin[0] != 'P') ) {
    print_sP(PSTR("\nInvalid pin: "));
    print_s(pin);
    print_crlf();
  }
  // report actual status
  print_c((addr_pin - _SFR_ADDR(PIN_0)) / 3 + PORT_BGN_CH); // A,B,C,...
  print_c(addr_bit + '0'); // 0,1,2,...
  print_sP(PSTR("  mode="));
  if (_SFR_MEM8(addr_ddr)  & _BV(addr_bit)) {
    // OUTPUT
    if (_SFR_MEM8(addr_port) & _BV(addr_bit)) {
      print_sP(PSTR("OH (output 1)"));
    } else {
      print_sP(PSTR("OL (output 0)"));
    }
  } else {
    // INPUT
    if (_SFR_MEM8(addr_port) & _BV(addr_bit)) {
      print_sP(PSTR("IH (IP, input w/ pull-up)"));
    } else {
      print_sP(PSTR("IL (IT, input tri-state)"));
    }
  }
  print_sP(PSTR(" duration="));
  print_hex4(tspan);
  print_sP(PSTR(" * unit="));
  if (unit ==0) {
    print_sP(PSTR(" ~ us"));
  } else {
    print_hex4(unit);
    print_sP(PSTR(" ms"));
  }
  print_sP(PSTR(" trigger#="));
  print_hex2(tcount);
  print_sP(PSTR(" LED#="));
  print_hex2(uunit1);
  print_sP(PSTR(","));
  print_hex2(uunit3);
  print_sP(PSTR(" PWM="));
  print_hex2(t_pwm);
  print_crlf();
  if (pin[0] == 'P') {
    data_dump();
  }
}
//
// Initialize DDR (basically all input)
//
void initialize_ddr_in(void) {
  print_sP(PSTR("Initialize DDR for all input\n"));
#ifdef BOARD_nano
  DDRB = 0x00;
  DDRC = 0x00;
  DDRD = 0x00;
#endif
#ifdef BOARD_teensy2
  DDRB = 0x00;
  DDRC = 0x00;
  DDRD = 0x00;
  DDRE = 0x00;
  DDRF = 0x00;
#endif
#ifdef BOARD_teensy2pp
  DDRA = 0x00;
  DDRB = 0x00;
  DDRC = 0x00;
  DDRD = 0x00;
  DDRE = 0x00;
  DDRF = 0x00;
#endif
  bit_pin(NULL, NULL); // bit operation to sram (default=LED pin)
}
//
// Initialize DDR (input and output)
//
void initialize_ddr_inout(void) {
  print_sP(PSTR("Initialize DDR for input and output (Top view with USB left)\n"));
  print_sP(PSTR("  Customize source code for target board wiring\n"));
#ifdef BOARD_nano
  print_sP(PSTR("  DDR: OUT = near: PC0-PC5, PB5=LED\n"));
  print_sP(PSTR("  DDR: IN  = far : PB* PD*\n"));
  DDRB = 0b00100000; // PB5: LED output
  DDRC = 0b00111111; // Scan (anode side)
  DDRD = 0x00;
#endif
#ifdef BOARD_teensy2
  print_sP(PSTR("  DDR: OUT = near: PB0-PB3 PB7 PD0-PD3 PC6-PC7/ far = PD6=LED\n"));
  print_sP(PSTR("  DDR: IN  = far : PF0-PF1 PF4-PF7 PB6-PB4 PD7/ in = PE6/ side = PD4 PD5\n"));
  DDRB = 0b10001111;
  DDRC = 0b11000000;
  DDRD = 0b01001111; // PD6: LED output
  DDRE = 0b00000000;
  DDRF = 0b00000000;
#endif
#ifdef BOARD_teensy2pp
  print_sP(PSTR("  DDR: OUT = near: PB7 PD0-PD5 PD7 PE0-PE1 PC0-PC7/ PD6=LED\n"));
  print_sP(PSTR("  DDR: IN  = far : PB0-PB6 PE7-PE6 PF0-PF7 in = PE4-PE5 PA0-PA7\n"));
  DDRA = 0b00000000;
  DDRB = 0b10000000;
  DDRC = 0b11111111;
  DDRD = 0b11111111; // PD6: LED output
  DDRE = 0b00000011;
  DDRF = 0b00000000;
#endif
  bit_pin(NULL, NULL); // bit operation to sram (default=LED pin)
}
//
// Initialize MASK (1: actively monitored/controlled, 0: excluded/ignored)
//
void initialize_mask(void) {
#ifdef BOARD_nano
  mask[0] = 0b11111111u;
  mask[1] = 0b01111111u;
  mask[2] = 0b11111111u;
#endif
#ifdef BOARD_teensy2
  mask[0] = 0b11111111u;
  mask[1] = 0b11000000u;
  mask[2] = 0b11111111u;
  mask[3] = 0b01000100u;
  mask[4] = 0b11110011u;
#endif
#ifdef BOARD_teensy2pp
  mask[0] = 0b11111111u;
  mask[1] = 0b11111111u;
  mask[2] = 0b11111111u;
  mask[3] = 0b11111111u;
  mask[4] = 0b11111111u;
  mask[5] = 0b11111111u;
#endif
}

// 16 MHz clock -- CPU clock (Typ)
// 50 - 200 KHz best resolution
//      ADPS * CLKPS  -- ADC clock
// 16,000,000 /    2  -- 8,000,000 Hz
// 16,000,000 /    4  -- 4,000,000 Hz
// 16,000,000 /    8  -- 2,000,000 Hz
// 16,000,000 /   16  -- 1,000,000 Hz
// 16,000,000 /   32  --   500,000 Hz
// 16,000,000 /   64  --   250,000 Hz OK
// 16,000,000 /  128  --   125,000 Hz GOOD
// 16,000,000 /  256  --    62,500 Hz GOOD
//
// 125 KHz 25 ADC Clock -> 5 KHz 0.2 ms
//
void adps_set(char *para) {
  uint8_t adps;
  if (*para >= '1'  && *para <= '7') {
    adps = (*para - '0') & 0x07;
    ADMUX &= ~0x07;
    ADMUX |= adps;
  }
  // report
  adps = ADMUX & 0x07;
  print_sP(PSTR("Analog prescaler = 2 ^ "));
  print_hex1(adps);
  print_crlf();
}

void aref_set(char *para) {
  uint8_t aref;
  if (*para == '0' || *para == '1' || *para == '3') {
    aref = ((*para - '0') & 0x03 ) << 6;
    ADMUX &= ~(0x03 << 6);
    ADMUX |= aref;
  }
  aref = ADMUX >> 6;
  print_sP(PSTR("Analog reference source: "));
  if (aref == 0) {
    print_sP(PSTR("0: External AREF"));
  } else if (aref == 1) {
    print_sP(PSTR("1: AVcc"));
  } else if (aref == 3) {
#ifdef BOARD_nano
    print_sP(PSTR("3: Internal 1.1V V REF"));
#endif
#if defined BOARD_teensy2 || defined BOARD_teensy2pp
    print_sP(PSTR("3: Internal 2.56V V REF"));
#endif
  }
  print_crlf();
}

static void analog_off(void) {
  print_sP(PSTR("Digital Input Enabled.  Analog input disabled\n"));
  DIDR0 = 0;
#if defined BOARD_teensy2
  DIDR2 = 0;
#endif
}

void bit_toggle(void) {
  print_sP(PSTR("BIT TOGGLE\n"));
  _SFR_MEM8(addr_port) ^= _BV(addr_bit);
}

void bit_on(void) {
  print_sP(PSTR("BIT ON\n"));
  _SFR_MEM8(addr_port) |= _BV(addr_bit);
}

void bit_off(void) {
  print_sP(PSTR("BIT OFF\n"));
  _SFR_MEM8(addr_port) &= ~_BV(addr_bit);
}

void bit_blink(uint16_t t) {
  uint16_t tt;
  if (t == 0) t = 5; // default
  print_sP(PSTR("BIT BLINK START\n"));
  do {
    _SFR_MEM8(addr_port) ^= _BV(addr_bit);
    tt = t;
    do {
      _delay_ms(100);
    } while (tt--);
  } while (!check_input());
  print_sP(PSTR("BIT BLINK END\n"));
}

static void inline uunit_delay1(void) {
  for (uint8_t i=uunit1; i>0; i--) {
    // NOP
    asm volatile(
    "       nop\n\t"
    );
  }
}
static void inline uunit_delay3(void) {
  for (uint8_t i=uunit3; i>0; i--) {
    // NOP
    asm volatile(
    "       nop\n\t"
    );
  }
}

static void pixel_0(void) {
    uunit_delay1();
    _SFR_MEM8(addr_port) ^= ~_BV(addr_bit);
    uunit_delay3();
    _SFR_MEM8(addr_port) |= _BV(addr_bit);
}

static void pixel_1(void) {
    uunit_delay3();
    _SFR_MEM8(addr_port) ^= ~_BV(addr_bit);
    uunit_delay1();
    _SFR_MEM8(addr_port) |= _BV(addr_bit);
}

static void pixel_reset(void) {
    _SFR_MEM8(addr_port) ^= ~_BV(addr_bit);
    for (uint16_t i=330; i>0; i--) {
      uunit_delay3();
    }
    _SFR_MEM8(addr_port) |= _BV(addr_bit);
}

void bit_pixel_dump(uint8_t xlen, uint8_t *x) {
  if ( xlen > LEDSIZE) xlen = LEDSIZE;
  uint8_t xxlen = xlen / 3;
  print_sP(PSTR("LED LENGTH: "));
  print_hex2(xxlen);
  print_crlf();
  for (uint8_t i=0; i < xxlen; i++) {
    print_sP(PSTR("LED["));
    print_hex2(i);
    print_sP(PSTR("] = G:"));
    print_hex2((uint8_t) *x++);
    print_sP(PSTR(" R:"));
    print_hex2((uint8_t) *x++);
    print_sP(PSTR(" B:"));
    print_hex2((uint8_t) *x++);
    print_crlf();
  }
}

uint8_t bit_pixel_set(char * token, uint8_t *x) {
  uint8_t i = 0;
  char buf[3]; // buffer to parse color string 2 chars
  while ((token != NULL) && (*token != '\0')) {
    buf[0] = *token++;
    buf[1] = *token++;
    buf[2] = '\0';
    if (buf[0] == '.') {
      // G    R    B data
      // 0xff 0x00 0x00 lime
      // 0x80 0x00 0x00 green
      // 0x00 0xff 0x00 red
      // 0x00 0x80 0x00 maroon
      // 0x00 0x00 0xff blue
      // 0x00 0x00 0x80 navy
      // 0xff 0xff 0x00 yellow
      // 0xff 0x00 0xff cyan
      // 0x00 0xff 0xff magenta
      // 0xff 0xff 0xff white
      // 0x80 0x80 0x80 gray
      if (buf[1] == 'G') {
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0x00;
        i += 3;
      } else if (buf[1] == 'R') {
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0x00;
        i += 3;
      } else if (buf[1] == 'B') {
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0xff;
        i += 3;
      } else if (buf[1] == 'W') {
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0xff;
        i += 3;
      } else {
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0x00;
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0xff;
        *x++ = (uint8_t) 0xff;
        i = 12;
      }
    } else {
      *x++ = (uint8_t) str2byte(buf);
      i+=2;
    }
  }
  return i;
}

void bit_pixel(uint8_t xlen, uint8_t *x) {
  print_sP(PSTR("LED PIXEL OUTPUT START\n"));
  pixel_reset();
  for (uint8_t i=xlen; i>0; i--) {
    ( *x & (uint8_t) 0x80 ) ? pixel_1() : pixel_0();
    ( *x & (uint8_t) 0x40 ) ? pixel_1() : pixel_0();
    ( *x & (uint8_t) 0x20 ) ? pixel_1() : pixel_0();
    ( *x & (uint8_t) 0x10 ) ? pixel_1() : pixel_0();
    ( *x & (uint8_t) 0x08 ) ? pixel_1() : pixel_0();
    ( *x & (uint8_t) 0x04 ) ? pixel_1() : pixel_0();
    ( *x & (uint8_t) 0x02 ) ? pixel_1() : pixel_0();
    ( *x & (uint8_t) 0x01 ) ? pixel_1() : pixel_0();
  }
  print_sP(PSTR("LED PIXEL OUTPUT END\n"));
}

void bit_wave(char *token) {
  //          pwm  HI  LOW    BRIGHT    TIME
  // TOKEN     0    4   0     100%      4
  // TOKEN     1    3   1      75%      4
  // TOKEN     2    2   1      67%      4
  // TOKEN     3    1   1      50%      4
  // TOKEN     4    1   2      33%      5
  // TOKEN     5    1   3      25%      6
  // TOKEN     6    1   4      20%      7
  // TOKEN     7    1   5      18%      8
  // TOKEN     8    1   6      14%      9
  // TOKEN     ..
  // TOKEN    255   1   253
  uint8_t t_pwm_l; // PWM HIGH period
  uint8_t t_pwm_h; // PWM LOW period
  uint8_t t_fact; // PWM duration adjust
  if (token != NULL && *token != '\0') {
    t_pwm = str2byte(token); // set low length
  }
  if (t_pwm < 4) {
    t_fact = 4;
    t_pwm_h = 4 - t_pwm;
  } else {
    t_pwm_h = 1;
    t_fact = t_pwm -1;
  }
  if (t_pwm < 1) {
    t_pwm_l = 0;
  } else if (t_pwm < 4) {
    t_pwm_l = 1;
  } else {
    t_pwm_l = t_pwm - 2;
  }
  print_sP(PSTR("BIT PWM WIDTH HIGH="));
  print_hex2(t_pwm_h);
  print_sP(PSTR(" LOW="));
  print_hex2(t_pwm_l);
  print_crlf();
  for (uint16_t time = (tspan/t_fact); time > 0; time --) {
    // HIGH
    _SFR_MEM8(addr_port) |= _BV(addr_bit);
    for (uint16_t t = (uint16_t) unit * t_pwm_h; t > 0; t--) {
      _delay_ms(1);
    }
    // LOW
    _SFR_MEM8(addr_port) &= ~ _BV(addr_bit);
    for (uint16_t t = (uint16_t) unit * t_pwm_l; t > 0; t--) {
      _delay_ms(1);
    }
  }
  print_sP(PSTR("BIT WAVE END\n"));
}

void bit_record(void) {
  uint8_t s; // state
  // s: state
  // 0: original state 0
  // 1:0->2: transitional to 2
  // 2: alt. state 2
  // 3: 2->0: transitional to 0
  // 0: original state after trigger (=0)
  uint8_t x; // current data
  uint16_t xx; // trigger direction
  uint8_t x0; // previous data
  uint16_t t; // time lapse (cumulative)
  uint16_t t0; // time lapse of the last trigger
  uint16_t td; // time lapse since the last trigger
  uint16_t i; // data[] record index
  uint16_t i0; // data[] record index (old)
  print_sP(PSTR("RECORDING one BIT PIN START\n"));
  x = _SFR_MEM8(addr_pin) & _BV(addr_bit);
  if (x) {
    xx = 0xf000; // high -> low as start
  } else {
    xx = 0x0f00; // low -> high as start
  }
  data[0] = xx; // trigger direction
  // original state
  s = 0;
  t = 0; // cumulative time
  t0 = 0; // t @ last trigger
  td = 0; // del(t)
  i = 1; // record index
  i0 = 0; // record index
  if  (unit != 0) {
    // reasonable unit in ms given
    while ((i < DATASIZE - 2) && (t < tspan)) {
      if (s == 0) {
        t = 0;  // not-triggered
        td = 0; // not-triggered
        t0 = 0; // not-triggered
      }
      t ++;
      td++;
      for (uint8_t t = unit; t == 0; t--) {
        _delay_ms(1);
      }
      x0 = x; // one older value
      x = _SFR_MEM8(addr_pin) & _BV(addr_bit);
      if (x != x0) {
        if (s == 0) {
          s = 1;
          t0 = t;
          td = 0;
          i0 = i;
          data[i++] = t;
        } else if (s == 1) {
          if (td < tcount) {
            s = 4;
            i = i0;
            td = t - t0;
          } else {
            s = 2;
          }
        } else if (s == 2) {
          s = 3;
          t0 = t;
          td = 0;
          i0 = i;
          data[i++] = t;
        } else if (s == 3) {
          if (td < tcount) {
            s = 2;
            i = i0;
            td = t - t0;
          } else {
            s = 4;
          }
        } else if (s == 4) {
          s = 1;
          t0 = t;
          td = 0;
          i0 = i;
          data[i++] = t;
        } else {
          // NO WAY
        }
      }
    }
  } else {
    // this should loop fastest w/o sub-loop
    while ((i < DATASIZE - 2) && (t < tspan)) {
      if (s == 0) {
        t = 0;  // not-triggered
        td = 0; // not-triggered
        t0 = 0; // not-triggered
      }
      t ++;
      td++;
      x0 = x; // one older value
      x = _SFR_MEM8(addr_pin) & _BV(addr_bit);
      if (x != x0) {
        if (s == 0) {
          s = 1;
          t0 = t;
          td = 0;
          i0 = i;
          data[i++] = t;
        } else if (s == 1) {
          if (td < tcount) {
            s = 4;
            i = i0;
            td = t - t0;
          } else {
            s = 2;
          }
        } else if (s == 2) {
          s = 3;
          t0 = t;
          td = 0;
          i0 = i;
          data[i++] = t;
        } else if (s == 3) {
          if (td < tcount) {
            s = 2;
            i = i0;
            td = t - t0;
          } else {
            s = 4;
          }
        } else if (s == 4) {
          s = 1;
          t0 = t;
          td = 0;
          i0 = i;
          data[i++] = t;
        } else {
          // NO WAY
        }
      }
    }
  }
  data[i] = 0xffff; // end of data marker
  print_sP(PSTR("RECORDING one BIT PIN END\n"));
}

void bit_record_pins(void) {
  uint8_t s; // state
  // s: state
  // 0: original state 0
  // 1: after trigger
  uint8_t x; // current data
  uint8_t x0; // previous data
  uint16_t i; // index (time)
  print_sP(PSTR("RECORDING PINS START\n"));
  x = _SFR_MEM8(addr_pin);
  data[0] = 0x8888; // byte record
  // original state
  s = 0;
  i = 1; // record index
    // reasonable unit in ms given
    while ((i < DATASIZE - 2) && (i < tspan)) {
      x0 = x; // one older value
      x = _SFR_MEM8(addr_pin);
      data[i++] = (uint16_t) x;
      for (uint8_t t = unit; t == 0; t--) {
        _delay_ms(1);
      }
      if (s == 0) {
        i = 1;  // not-triggered
      }
      if (x != x0) {
        s = 1;
      }
    }
  data[i] = 0xffff; // end of data marker
  print_sP(PSTR("RECORDING PINS END\n"));
}

/////////////////////////////////////////////////////////////////////////////
//
// I/O Memory access (HW general)
//
/////////////////////////////////////////////////////////////////////////////
static uint8_t read_sram(uint16_t addr) { return _SFR_MEM8(addr); }

static void write_sram(uint16_t addr, uint8_t value) {
  _SFR_MEM8(addr) = value;
}

static void write_and_sram(uint16_t addr, uint8_t value) {
  _SFR_MEM8(addr) &= value;
}

static void write_or_sram(uint16_t addr, uint8_t value) {
  _SFR_MEM8(addr) |= value;
}

static uint8_t read_flash(uint16_t addr) { return pgm_read_byte(addr); }

void mask_set(void) {
  uint8_t i;
  print_sP(PSTR("MASK excludes non-valid PINs from monitoring\n"));
  print_sP(PSTR("0: excluded from monitoring and controlling\n"));
  print_sP(PSTR("1: actively monitored and controlled\n"));
  for (i = 0; i < N_PORTS; i++) {
    print_sP(PSTR("   >>> mask[PORT"));
    print_c(PORT_BGN_CH + i);
    print_sP(PSTR("] (enter a binary byte number starting with %): "));
    read_line(sm);
    mask[i] = str2byte(sm);
  }
  print_crlf();
  if (mask_override == 0) {
    print_sP(PSTR("Mask ENabled for display, 'SMD' command to disable it."));
  } else {
    print_sP(PSTR("Mask DISabled for display, 'SME' command to enable it."));
  }
}
//
// Print all dump
//
static void print_alldump(uint16_t a0, uint16_t a1,
                          uint8_t (*read_fn_p)(uint16_t)) {
  uint16_t a;
  uint8_t v;
  for (a = a0; a <= a1; a++) {
    print_hex4(a);
    print_c(' ');
    v = (*read_fn_p)(a);
    print_bin8(v, 0xff);
    print_c('=');
    print_hex2(v);
    print_c('=');
    print_c('~');
    print_hex2(~v);
    print_c(' ');
    print_ascii(v);
    print_crlf();
  }
}
//
// Print hex dump
//
static void print_hexdump(uint16_t a0, uint16_t a1,
                          uint8_t (*read_fn_p)(uint16_t)) {
  uint16_t ax;
  uint8_t i;
  uint16_t a;
  uint8_t v;
  for (ax = a0 / 16; ax <= (a1 / 16); ax++) {
    print_hex4(ax * 16);
    print_c(' ');
    for (i = 0; i <= 15; i++) {
      a = ax * 16 + i;
      if (a >= a0 && a <= a1) {
        v = (*read_fn_p)(a);
        print_hex2(v);
      } else {
        print_c(' ');
        print_c(' ');
      }
      print_c(' ');
      if (i == 7) print_c(' ');
    }
    for (i = 0; i <= 15; i++) {
      a = ax * 16 + i;
      if (a >= a0 && a <= a1) {
        v = (*read_fn_p)(a);
        print_ascii(v);
      } else {
        print_c(' ');
      }
    }
    print_crlf();
  }
}
//
// Display
//
static void display_digital(void) {
  uint8_t i;
  print_sP(PSTR("SRAM: *0x"));
  print_hex4(str2word("."));
  print_sP(PSTR(",  "));
  print_sP(PSTR("FLASH: *0x"));
  print_hex4(str2word(">"));
  print_crlf();
  bit_pin("?", NULL);
  print_sP(PSTR(">>>>>>  state of mask and digital I/O ports  <<<<<<\n"));
  print_sP(PSTR("MASK:   "));
  for (i = 0; i < N_PORTS; i++) {
    print_c(PORT_BGN_CH + i);
    print_c(':');
    print_byte(mask[i], 0xff);
    print_c(' ');
  }
  print_crlf();
  print_sP(PSTR("PIN:    "));
  for (i = 0; i < N_PORTS; i++) {
    print_c(PORT_BGN_CH + i);
    print_c(':');
    print_byte(IOREG(PIN_0, i), mask_override | mask[i]);
    print_c(' ');
  }
  print_crlf();
  print_sP(PSTR("DDR:    "));
  for (i = 0; i < N_PORTS; i++) {
    print_c(PORT_BGN_CH + i);
    print_c(':');
    print_byte(IOREG(DDR_0, i), mask_override | mask[i]);
    print_c(' ');
  }
  print_crlf();
  print_sP(PSTR("PORT:   "));
  for (i = 0; i < N_PORTS; i++) {
    print_c(PORT_BGN_CH + i);
    print_c(':');
    print_byte(IOREG(PORT_0, i), mask_override | mask[i]);
    print_c(' ');
  }
  print_crlf();
  print_crlf();
}
//
// Monitor (only active input pins)
//
static void monitor_digital(void) {
  uint8_t i;
  uint16_t j;
  uint8_t same;
  uint8_t typed;
  uint8_t pin0[N_PORTS];  // DDR=0 masked PIN data (new)
  uint8_t pin1[N_PORTS];  // DDR=0 masked PIN data (last)
  //
  for (i = 0; i < N_PORTS; i++) {
    pin0[i] = IOREG(PIN_0, i) & (~IOREG(DDR_0, i)) & mask[i];
  }
  display_digital();
  print_sP(PSTR("=== Monitoring ... Type any key to exit ===\n"));
  //
  j = 0;
  do {
    print_sP(PSTR("X_"));
    print_hex4(j++); // indexed output
    print_sP(PSTR(": "));
    for (i = 0; i < N_PORTS; i++) {
      print_c(PORT_BGN_CH + i);
      print_c(':');
      pin1[i] = pin0[i]; // update previous value pin1
      print_byte(pin1[i], (~IOREG(DDR_0, i) & (mask[i])) | mask_override);
      print_c(' ');
    }
    print_crlf();
    do {
      same = 1;  // same
      for (i = 0; i < N_PORTS; i++) {
        pin0[i] = IOREG(PIN_0, i) & (~IOREG(DDR_0, i)) & mask[i];
        same = same && (pin1[i] == pin0[i]);
      }
      typed = check_input();
    } while (same && (!typed));
    _delay_ms(1); // no pint reading so quickly :-)
  } while (!typed);
}
//
/////////////////////////////////////////////////////////////////////////////
//
// Set OUTPUT
//
static void output_high(void) {
  uint8_t i;
  for (i = 0; i < N_PORTS; i++) {
    IOREG(PORT_0, i) |= mask[i] &  IOREG(DDR_0, i); // active output bit to 1
  }
  print_sP(PSTR("Set all active OUTPUT to 1\n"));
}
static void output_low(void) {
  uint8_t i;
  for (i = 0; i < N_PORTS; i++) {
    IOREG(PORT_0, i) &= ~mask[i] | ~IOREG(DDR_0, i); // active output bit to 0
  }
  print_sP(PSTR("Set all active OUTPUT to 0\n"));
}
void input_pullup(void) {
  uint8_t i;
  for (i = 0; i < N_PORTS; i++) {
    IOREG(PORT_0, i) |= mask[i] & ~IOREG(DDR_0, i); // active input bit to 1;
  }
  print_sP(PSTR("Set all active INPUT to pull-up\n"));
}
void input_tristate(void) {
  uint8_t i;
  for (i = 0; i < N_PORTS; i++) {
    IOREG(PORT_0, i) &= ~mask[i] | IOREG(DDR_0, i); // active input bit to 0;
  }
  print_sP(PSTR("Set all active INPUT to tri-state\n"));
}
void set_pud(void) {
  MCUCR |= _BV(PUD); // Set Pull-up Disable;
  print_sP(PSTR("Set Pull-up Disable\n"));
}
void reset_pud(void) {
  MCUCR &= ~_BV(PUD); // Reset Pull-up Disable;
  print_sP(PSTR("Reset Pull-up Disable\n"));
}
void mask_disabled_for_display(void) {
  mask_override = 0xff; // disable mask for display
  print_sP(PSTR("Mask DISabled for display\n"));
}
void mask_enabled_for_display(void) {
  mask_override = 0; // enable mask for display
  print_sP(PSTR("Mask ENabled for display\n"));
}
//
// Analog
//
// oversampling OSPWR times (OSPWR can be 2^5 w/o overflow)
#define OSPWR 4
// reported number is averaged

static void monitor_analog(void) {
  uint8_t i, j, k;
  uint16_t av;
  DIDR0 = 0x3f;  // Digital Input Disable
  ADCSRA = _BV(ADEN);  // ADC Enable
  print_sP(PSTR("Analog Input Enabled.  Digital input disabled\n"));
  adps_set("?"); // ADC prescaler 1/128
  aref_set("?"); // VREF=Vcc
  while (1) {
#ifdef BOARD_nano
    uint8_t imax = 11;
#endif
#if defined BOARD_teensy2
    uint8_t imax = 15;
#endif
#if defined BOARD_teensy2pp
    uint8_t imax = 10;
#endif
    for (i = 0; i < imax; i++) {
#ifdef BOARD_nano
      j = ((i > 8) ? (i + 5) : i);
      ADMUX = (ADMUX & 0b11110000) | j;  // MUX* setting
#endif
#if defined BOARD_teensy2
      // 01  4567   89ABCD
      j = (i > 5) ? (i + 8) : ((i > 1) ? (i + 2) : i) ;
      if ( i < 2 ) {
        j = i;
      } else if ( i < 6 ) {
        j = i + 2;
      } else if ( i <  12 ) {
        j = i + 32 -8 ;
      } else if ( i == 12 ) {
        j = 0b100111; // Temp.
      } else if ( i == 13 ) {
        j = 0b011110; // 1.1V V BG ref
      } else if ( i == 14 ) {
        j = 0b011111; // 0V (GND)
      }
      ADMUX  = (ADMUX  & 0b11100000) | (j & 0b01111) ;  // MUX* setting
      ADCSRB = (ADCSRB & 0b00010000) | (j & 0b10000) ;  // MUX* setting
#endif
#if defined BOARD_teensy2pp
      j = i ;
      if ( i == 8 ) j = 0b011110; // 1.1V V BG ref
      if ( i == 9 ) j = 0b011111; // 0V (GND)
      ADMUX = (ADMUX & 0b11100000) | j;  // MUX* setting
#endif
      av = 0;
      for (k = (1 << OSPWR); k > 0; k--) {
        ADCSRA |= _BV(ADSC);  // ADC Start Conversion
        _delay_ms(1);
        do {
        } while (ADCSRA & _BV(ADSC));  // until clear
        av += ADC;
      }
      print_hex4(av >> OSPWR);
      if (i < imax -3 ) {
        print_sP(PSTR("  ADC/PC"));
#if defined BOARD_teensy2
        print_hex2((i > 1) ? (i + 2) : i);
#else
        print_hex2(i);
#endif
#if defined BOARD_nano || defined BOARD_teensy2
      } else if (i == imax -3) {
        print_sP(PSTR("  Temperature Sensor"));
#endif
      } else if (i == imax -2) {
        print_sP(PSTR("  1.1V Internal Ref."));
      } else if (i == imax -1) {
        print_sP(PSTR("  0.0V"));
      }
      print_crlf();
      _delay_ms(1);
    }
    if (check_input()) break;
    print_up(imax);
  }
}

#ifdef VERBOSE
void display_help(void) {
  print_sP(PSTR(
"===== Command syntax =====\n"
"a1 a2     a3    action        / variant commands\n"
"R  addr0  addr1 sram hexdump  / RA: sram alldump, RP: program hexdump\n"
"W  addr   val   sram =write   / WA: sram &=write, WO: sram |=write\n"
"D               PIN state     / DC: PIN state (changed **)\n"
"S               Set initial   / SK: Set alternative, SM: Set MASK\n"
"SMD             Set mask disabled for display / SME: Set mask enabled\n"
"SOH             Set OUTPUT 1  / SOL: Set OUTPUT 0\n"
"SIP             Set INPUT pull-up / SIP: Set tri-state, alias: SIH, SIL\n"
"SPD             Set MCUCR PUD to disable pull-up / SPE: enable pull-up\n"

"B               Toggle BIT (output), Triggered BIT read (input)\n"
"B pin mode      Set a BIT to mode=OH/OL/IH/IL (" QS(LED_PIN) " OL), or '?','P'\n"
"BL              BIT to 0 (low) / BH: BIT to 1 (high), BD: Dump recorded data\n"
"BTS tspan tcount  Set time span and trigger count / BTU: Set unit in ms (5)\n"
"BTX var1 var2   Set time unit for LED pixel driver (MCU loops) (5 15)\n"
"BP [P]          Record pins around BIT pin (w/ P, print recorded data)\n"
"BB word         Blink  BIT (unit 100 ms) (O) **\n"
"BW var          PWM wave of BIT var=0 bright, var=3 50/50 (O)\n"
"BX              Send LED data / BX ?: Print pixel LED dat\n"
"BX color        Set pixel LED data FFFFFF-like or .R.G.B-like series\n"

"A               Monitor analog inputs / AX: Analog input off\n"
"AP para         Set analog prescaler　/ AP: Set analog prescaler\n"
"? val           print 8 bit   / ??: 16 bit value (calculator)\n"
"\n"
"Numbers: hexadecimal / ~: bit flip, %....: binary, @...: mnemonic\n"
"         '.' means sram next,  ',' means sram previous\n"
"         '>' means flash next, '<' means flash previous\n"
));
}
#endif // VERBOSE
///////////////////////////////////////////////////////////////////////////
//
// System monitor
//
/////////////////////////////////////////////////////////////////////////////
int main(void) {
  // command line tokenizer for main loop (w/ history)
  char s[BUFSIZE];               // line
  char sx[BUFSIZE];              // lastline
  char *str_main, *str_sub;                                 // str
  char *token_main, *token_sub1, *token_sub2, *token_sub3;  // token
  char *saveptr_main, *saveptr_sub;                         // saveptr
  // byte value to set
  uint8_t val = 0;
  // sram pointer
  addr_sram = 0x00;                 // start of SRAM
  addr_sram_last = 0x00;            // start of SRAM (last)
  uint16_t addr_sram_tmp;           // temp
  uint16_t addr_sram_end = 0x00;    // same as start
  uint16_t addr_sram_range = 0xff;  // range 256 bytes from 0
  // program memory hexdump pointer
  addr_flash = 0x00;                // start flash
  addr_flash_last = 0x00;           // start
  uint16_t addr_flash_tmp;          // temp
  uint16_t addr_flash_end = 0xff;   // end
  uint16_t addr_flash_range = 0xff; // range from 0
  // LED data buffer
  uint8_t pixled[LEDSIZE];
  uint8_t ledlen; // 3 * (#LEDs)
  // initialize MCU
  CPU_PRESCALE;
  // initialize USB
  init_comm();
  // opening messages
  print_sP(PSTR("\nAVRmon v 0.2\n"));
  print_sP(PSTR("  MCU   = " QS(MCU) "  F_CPU = " QS(F_CPU) "\n"));
  print_sP(PSTR("  BOARD = " QS(BOARD) "     BAUD = " QS(BAUD) "\n"));
  print_sP(PSTR("  GPL 2.0+, Copyright 2020 - 2021, <osamu@debian.org>\n\n"));
  print_sP(PSTR("Commands are case insensitive.  'H' for more help.\n"));
  print_sP(PSTR("Numbers are typed as binary or hexadecimal: %11110100 == ~%0001011 == F4 == ~0B\n"));
  // initialize bit operation
  initialize_ddr_in(); // set up default ddr (input)
  initialize_mask(); // set up default mask
  mask_enabled_for_display(); // enable mask for display
  input_pullup(); // Enable pull-up for input
  ledlen = bit_pixel_set(".P", pixled);  // pre-set value for pixel
  bit_pixel_dump(ledlen, pixled); // display pixel preset values
  adps_set("7"); // ADC prescaler 1/128
  aref_set("1"); // VREF=Vcc
  strcpy(sx, "D");  // initial safe value
  print_crlf(); // end of initialization
  // display mcu states
  display_digital();
  // main infinite loop
  while (1) {
    s[0] = '\0';  // clear line
    print_sP(PSTR("command > "));
    read_line(s);
    str_main = s;
    while (*str_main == ' ' || *str_main == ';') {
      str_main++;
    }  // drop leading ' ' and ';'
    if (*str_main != '\0') {
      strcpy(sx, str_main);  // if not return, keep this as the last line
    } else {
      strcpy(s, sx);  // if return, restore the last line
      print_sP(PSTR("\e[Acommand > "));
      print_s(s);
      print_crlf();
    }
    while (1) {
      // tokenize (if zero length, make it NULL)
      token_main = strtok_r(str_main, ";", &saveptr_main);
      if (token_main == NULL || *token_main == '\0') break;  // next line
      str_sub = token_main;
      token_sub1 = strtok_r(str_sub, " ", &saveptr_sub);
      if (token_sub1 == NULL || *token_sub1 == '\0') {
        break;  // next line
      } else {
        str_sub = NULL;
        token_sub2 = strtok_r(str_sub, " ", &saveptr_sub);
        if (token_sub2 == NULL) {
          token_sub3 = NULL;
        } else if (*token_sub2 == '\0') {
          token_sub3 = token_sub2 = NULL;
        } else {
          token_sub3 = strtok_r(str_sub, " ", &saveptr_sub);
          if (*token_sub3 == '\0') token_sub3 = NULL;
        }
        //
        // process tokens
        //
        if (!strcmp_P(token_sub1, PSTR("R"))) {
          // read from SRAM (hexdump) progressive location
          if (token_sub2 != NULL) {
            addr_sram_tmp = addr_sram;
            addr_sram = str2word(token_sub2);
            addr_sram_last = addr_sram_tmp;
          }
          if (addr_sram > MAX_SRAM) addr_sram = MAX_SRAM;
          if (token_sub3 != NULL) {
            addr_sram_end = str2word(token_sub3);
          } else {
            addr_sram_end = addr_sram + addr_sram_range;
          }
          if (addr_sram_end > MAX_SRAM) addr_sram_end = MAX_SRAM;
          if (addr_sram_end < addr_sram) addr_sram_end = addr_sram;
          //
          print_hexdump(addr_sram, addr_sram_end, &read_sram);
          //
          addr_sram_range = addr_sram_end - addr_sram;
          addr_sram = addr_sram_end + 1;
          if (addr_sram > MAX_SRAM) addr_sram = 0;
          addr_sram_end = addr_sram + addr_sram_range;
          if (addr_sram_end > MAX_SRAM) addr_sram_end = MAX_SRAM;
        } else if (!strcmp_P(token_sub1, PSTR("RA"))) {
          // read from SRAM (alldump) progressive location
          if (token_sub2 != NULL) {
            addr_sram_tmp = addr_sram;
            addr_sram = str2word(token_sub2);
            addr_sram_last = addr_sram_tmp;
          }
          if (addr_sram > MAX_SRAM) addr_sram = MAX_SRAM;
          if (token_sub3 != NULL) {
            addr_sram_end = str2word(token_sub3);
          } else {
            addr_sram_end = addr_sram + addr_sram_range;
          }
          if (addr_sram_end > MAX_SRAM) addr_sram_end = MAX_SRAM;
          if (addr_sram_end < addr_sram) addr_sram_end = addr_sram;
          //
          print_alldump(addr_sram, addr_sram_end, &read_sram);
          //
          addr_sram_range = addr_sram_end - addr_sram;
          addr_sram = addr_sram_end + 1;
          if (addr_sram > MAX_SRAM) addr_sram = 0;
          addr_sram_end = addr_sram + addr_sram_range;
          if (addr_sram_end > MAX_SRAM) addr_sram_end = MAX_SRAM;
        } else if (!strcmp_P(token_sub1, PSTR("RP"))) {
          // read from program memory (hexdump) progressive location
          if (token_sub2 != NULL) {
            addr_flash_tmp = addr_flash;
            addr_flash = str2word(token_sub2);
            addr_flash_last = addr_flash_tmp;
          }
          if (addr_flash > MAX_FLASH) addr_flash = MAX_FLASH;
          if (token_sub3 != NULL) {
            addr_flash_end = str2word(token_sub3);
          } else {
            addr_flash_end = addr_flash + addr_flash_range;
          }
          if (addr_flash_end > MAX_FLASH) addr_flash_end = MAX_FLASH;
          if (addr_flash_end < addr_flash) addr_flash_end = addr_flash;
          //
          print_hexdump(addr_flash, addr_flash_end, &read_flash);
          //
          addr_flash_range = addr_flash_end - addr_flash;
          addr_flash = addr_flash_end + 1;
          if (addr_flash > MAX_FLASH) addr_flash = 0;
          addr_flash_end = addr_flash + addr_flash_range;
          if (addr_flash_end > MAX_FLASH) addr_flash_end = MAX_FLASH;
        } else if (!strcmp_P(token_sub1, PSTR("RE"))) {
          // XXX FIXME XXX
        //
        ///////////////////////////////////////////////////////////////
        //
        } else if (!strcmp_P(token_sub1, PSTR("W"))) {
          // write to SRAM (alldump) progressive location
          if (token_sub2 != NULL) {
            addr_sram_tmp = addr_sram;
            addr_sram = str2word(token_sub2);
            addr_sram_last = addr_sram_tmp;
          }
          if (token_sub3 != NULL) val = str2byte(token_sub3);
          if (addr_sram < MIN_SRAM) break;  // ignore
          if (addr_sram > MAX_SRAM) break;  // ignore
          write_sram(addr_sram, val);
          print_alldump(addr_sram, addr_sram, &read_sram);
          addr_sram++;
          if (addr_sram > MAX_SRAM) addr_sram = MIN_SRAM;
          print_alldump(addr_sram, addr_sram, &read_sram);
        } else if (!strcmp_P(token_sub1, PSTR("WA"))) {
          // write to SRAM (alldump) progressive location
          if (token_sub2 != NULL) {
            addr_sram_tmp = addr_sram;
            addr_sram = str2word(token_sub2);
            addr_sram_last = addr_sram_tmp;
          }
          if (token_sub3 != NULL) val = str2byte(token_sub3);
          if (addr_sram < MIN_SRAM) break;  // ignore
          if (addr_sram > MAX_SRAM) break;  // ignore
          write_and_sram(addr_sram, val);
          print_alldump(addr_sram, addr_sram, &read_sram);
          addr_sram++;
          if (addr_sram > MAX_SRAM) addr_sram = MIN_SRAM;
          print_alldump(addr_sram, addr_sram, &read_sram);
        } else if (!strcmp_P(token_sub1, PSTR("WO"))) {
          // write to SRAM (alldump) progressive location
          if (token_sub2 != NULL) {
            addr_sram_tmp = addr_sram;
            addr_sram = str2word(token_sub2);
            addr_sram_last = addr_sram_tmp;
          }
          if (token_sub3 != NULL) val = str2byte(token_sub3);
          if (addr_sram < MIN_SRAM) break;  // ignore
          if (addr_sram > MAX_SRAM) break;  // ignore
          write_or_sram(addr_sram, val);
          print_alldump(addr_sram, addr_sram, &read_sram);
          addr_sram++;
          if (addr_sram > MAX_SRAM) addr_sram = MIN_SRAM;
          print_alldump(addr_sram, addr_sram, &read_sram);
        } else if (!strcmp_P(token_sub1, PSTR("WP"))) {
          // XXX FIXME XXX
        } else if (!strcmp_P(token_sub1, PSTR("WE"))) {
          // XXX FIXME XXX
        //
        ///////////////////////////////////////////////////////////////
        //
#ifdef VERBOSE
        } else if (!strcmp_P(token_sub1, PSTR("H"))) {
          display_help();
        //
#endif // VERBOSE
        ///////////////////////////////////////////////////////////////
        //
        } else if (!strcmp_P(token_sub1, PSTR("D"))) {
          // display mcu states
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("DC"))) {
          monitor_digital();
        //
        ///////////////////////////////////////////////////////////////
        //
        // For switching between:
        // * Tri-state ({DDxn, PORTxn} = 0b00)
        // * Output high ({DDxn, PORTxn} = 0b11)
        // We must step through:
        // * Pull-up enabled {DDxn, PORTxn} = 0b01)
        // * Output low ({DDxn, PORTxn} = 0b10)
        //
        // For switching between:
        // * Pull-up enabled {DDxn, PORTxn} = 0b01)
        // * Output low ({DDxn, PORTxn} = 0b10)
        // We must step through:
        // * Tri-state ({DDxn, PORTxn} = 0b00)
        // * Output high ({DDxn, PORTxn} = 0b11)
        //
        // Set PUD bit in MCUCR to disable pull-up
        //
        } else if (!strcmp_P(token_sub1, PSTR("S"))) {
          output_low();
          input_tristate();
          initialize_ddr_in();
          initialize_mask();
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("SK"))) {
          output_low();
          input_tristate();
          initialize_ddr_inout();
          initialize_mask();
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("SM"))) {
          mask_set();
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("SMD"))) {
          mask_disabled_for_display();
        } else if (!strcmp_P(token_sub1, PSTR("SME"))) {
          mask_enabled_for_display();
        } else if (!strcmp_P(token_sub1, PSTR("SOH"))) {
          output_high();
        } else if (!strcmp_P(token_sub1, PSTR("SOL"))) {
          output_low();
        } else if ((!strcmp_P(token_sub1, PSTR("SIP"))) ||
                  (!strcmp_P(token_sub1, PSTR("SIH")))) {
          input_pullup();
        } else if ((!strcmp_P(token_sub1, PSTR("SIT"))) ||
                  (!strcmp_P(token_sub1, PSTR("SIL")))) {
          input_tristate();
        } else if (!strcmp_P(token_sub1, PSTR("SPD"))) {
          set_pud();
        } else if (!strcmp_P(token_sub1, PSTR("SPE"))) {
          reset_pud();
        //
        ///////////////////////////////////////////////////////////////
        //
        } else if (!strcmp_P(token_sub1, PSTR("B"))) {
          if (token_sub2 == NULL) {
            if (_SFR_MEM8(addr_ddr) & _BV(addr_bit)) {
              bit_toggle(); // output
              display_digital();
            } else {
              bit_record(); // input
              data_dump(); // input
            }
          } else {
            bit_pin(token_sub2, token_sub3);
          }
        } else if (!strcmp_P(token_sub1, PSTR("BL"))) {
          bit_off();
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("BH"))) {
          bit_on();
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("BTU"))) {
          if (token_sub2 == NULL) {
            unit = 1; // 1 ms is minimum on normal AVR coding
          } else {
            unit = str2word(token_sub2);
          }
        } else if (!strcmp_P(token_sub1, PSTR("BTS"))) {
          if (token_sub2 == NULL) {
            tspan = 0x8000;
            tcount = 5; // 5ms is typical switch delay
          } else {
            tspan = str2word(token_sub2);
            if (token_sub3 == NULL) {
              tcount = 5; // 5ms is typical switch delay
            } else {
              tcount = str2byte(token_sub3);
            }
          }
        } else if (!strcmp_P(token_sub1, PSTR("BTX"))) {
          if (token_sub2 == NULL) {
            uunit1 = 5;
            uunit3 = 15;
          } else {
            uunit1 = str2word(token_sub2);
          }
          if (token_sub3 == NULL) {
            uunit3 = 3 * uunit1;
          } else {
            uunit3 = str2word(token_sub3);
          }
        } else if (!strcmp_P(token_sub1, PSTR("BP"))) {
          if (token_sub2[0] != 'P') {
            bit_record_pins();
          }
          data_dump();
        } else if (!strcmp_P(token_sub1, PSTR("BB"))) {
          bit_blink(str2word(token_sub2));
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("BW"))) {
          bit_wave(token_sub2);
          display_digital();
        } else if (!strcmp_P(token_sub1, PSTR("BX"))) {
          if (token_sub2 == NULL || *token_sub2 == '\0') {
            bit_pixel(ledlen, pixled);
          } else if (token_sub2[0] == '?' || token_sub2[0] == '/') {
            // NOP
          } else {
            ledlen = bit_pixel_set(token_sub2, pixled);
          }
          bit_pixel_dump(ledlen, pixled);
        //
        ///////////////////////////////////////////////////////////////
        //
        } else if (!strcmp_P(token_sub1, PSTR("AR"))) {
          aref_set(token_sub2);
        } else if (!strcmp_P(token_sub1, PSTR("AP"))) {
          adps_set(token_sub2);
        } else if (!strcmp_P(token_sub1, PSTR("A"))) {
          monitor_analog();
        } else if (!strcmp_P(token_sub1, PSTR("AX"))) {
          analog_off();
        //
        ///////////////////////////////////////////////////////////////
        //
        } else if (!strcmp_P(token_sub1, PSTR("?")) ||
                   !strcmp_P(token_sub1, PSTR("/")) ) {
          print_sP(PSTR("byte -> bin / hex / ~hex / ascii >> "));
          print_byte(str2byte(token_sub2), 0xff);
          print_ascii(str2byte(token_sub2));
          print_crlf();
        } else if (!strcmp_P(token_sub1, PSTR("??")) ||
                   !strcmp_P(token_sub1, PSTR("//")) ) {
          print_sP(PSTR("word -> hex / ~hex >> "));
          print_hex4(str2word(token_sub2));
          print_sP(PSTR(" = ~"));
          print_hex4(~str2word(token_sub2));
          print_crlf();
        } else {
          print_sP(PSTR("Unexpected input tokens: "));
          print_c('"');
          print_s(token_sub1);
          print_c('"');
          print_c(',');
          print_c(' ');
          print_c('"');
          print_s(token_sub2);
          print_c('"');
          print_c(',');
          print_c(' ');
          print_c('"');
          print_s(token_sub3);
          print_c('"');
          print_crlf();
        }
        str_main = NULL; // to read next
      }
    }
  }
  print_sP(PSTR("\n!!! NEVER HERE !!! DEAD !!!\n"));
}
