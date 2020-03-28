// vim: ft=c sts=2 sw=2 et si ai:
//
// AVR monitor program: avrmon
//
// Copyright 2020 Osamu Aoki <osamu@debian.org>
//
// License: GPL 2.0+
//
// Hardware configuration (Makefile -> config.h.in)
//
#include "config.h"
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

///////////////////////////////////////
// HW dependent
///////////////////////////////////////
#ifdef __AVR_ATmega328P__
#define PORT_BGN B
#define PORT_END E
#define LED_CONFIG (DDRB |= (1 << 5))
#define LED_ON (PORTB |= (1 << 5))
#define LED_OFF (PORTB &= ~(1 << 5))
// 32+64+160+2048 B SRAM
#define MIN_SRAM 0x20
#define MAX_SRAM 0x8ff
// 32 KB FLASH
#define MAX_FLASH 0x7fff
#endif
/////////////////////////////////////////
#ifdef __AVR_ATmega32U4__
#include "usb_serial.h"
#define PORT_BGN B
#define PORT_END E
#define LED_CONFIG (DDRD |= (1 << 6))
#define LED_ON (PORTD |= (1 << 6))
#define LED_OFF (PORTD &= ~(1 << 6))
#define CPU_PRESCALE(n) (CLKPR = 0x80, CLKPR = (n))
#define CPU_16MHz 0x00
#define CPU_8MHz 0x01
#define CPU_4MHz 0x02
#define CPU_2MHz 0x03
#define CPU_1MHz 0x04
#define CPU_500kHz 0x05
#define CPU_250kHz 0x06
#define CPU_125kHz 0x07
#define CPU_62kHz 0x08
// 32+64+160+2048 B SRAM
#define MIN_SRAM 0x20
#define MAX_SRAM 0x8ff
// 32 KB FLASH
#define MAX_FLASH 0x7fff
#endif
/////////////////////////////////////////
//
// Cliche to get CPP MACRO definition included as C quoted string
#define _QS(a) #a
#define QS(a) _QS(a)
// Cliche to get concatenated CPP MACRO definition
#define _CAT(a, b) a##b
#define CAT(a, b) _CAT(a, b)
// Cliche to get char from a string: [0]
#define PORT_BGN_CH QS(PORT_BGN)[0]
#define PORT_END_CH QS(PORT_END)[0]
#define N_PORTS PORT_END_CH - PORT_BGN_CH
// base port name
#define DDR_0 CAT(DDR, PORT_BGN)
#define PORT_0 CAT(PORT, PORT_BGN)
#define PIN_0 CAT(PIN, PORT_BGN)
// I/O registers indexed access
#define IOR(base, i) _SFR_MEM8(_SFR_ADDR(base) + 3 * i)
//
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
// Initialize Character I/O with F_CPU/BAUD
//
static void init_comm(void) {
#ifdef __AVR_ATmega328P__
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;
#if USE_2X
  UCSR0A |= _BV(U2X0);
#else
  UCSR0A &= ~_BV(U2X0);
#endif
  LED_CONFIG;
  LED_ON;
  // async, non-parity, 1-bit stop, 8-bit data
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
  // tx and rx enable
  UCSR0B = _BV(TXEN0) | _BV(RXEN0);
  // All Data Direction Register for input
  DDRB = DDRC = DDRD = 0;
  // pull-up for all
  PORTB = PORTC = PORTD = 0xff;
#endif
#ifdef __AVR_ATmega32U4__
  CPU_PRESCALE(CPU_16MHz);
  LED_CONFIG;
  LED_ON;

  // initialize the USB, and then wait for the host
  // to set configuration.  If the Teensy is powered
  // without a PC connected to the USB port, this
  // will wait forever.
  usb_init();
  do {
  } while (!usb_configured()); /* wait */
  _delay_ms(1000);
#endif
}

//
// Character output
//
static void print_c(char c) {
#ifdef __AVR_ATmega328P__
  // loop until UDRE0 bit is set in UCSR0A
  do {
  } while (!(UCSR0A & _BV(UDRE0)));
  UDR0 = c;
#endif
#ifdef __AVR_ATmega32U4__
  usb_serial_putchar(c);
#endif
}

//
// Control character output - down 1 line
//
static void print_crlf(void) {
  print_c('\r');
  print_c('\n');
}

//
// Control sequence output  - up 1 line
//
static void print_up(uint8_t n) {
  while (n-- > 0) {
    print_c(0x1b);
    print_c('[');
    print_c('A');
  }
}

//
// String output
//
static void print_s(char *s) {
  char c;
  if (s != NULL) {
    while (1) {
      c = *s++;
      if (c == '\0') break;
      if (c == '\n') print_c('\r');
      print_c(c);
    }
  }
}

static void print_sP(PGM_P s) {
  char c;
  if (s != NULL) {
    while (1) {
      //  pgm_read_byte defined in <avr/pgmspace.h>
      c = pgm_read_byte(s++);
      if (c == '\0') break;
      if (c == '\n') print_c('\r');
      print_c(c);
    }
  }
}

//
// Output uint8_t in binary[8] with mask
//
static void print_bin8(uint8_t u, uint8_t m) {
  for (int8_t i = 7; i >= 0; i--) {
    print_c((m & _BV(i)) ? ((u & _BV(i)) ? '1' : '0') : '_');
  }
}

//
// Output uint8_t in hexadecimal[2]
//
static void print_hex2(uint8_t u) {
  uint8_t ux;
  ux = u >> 4;
  print_c((ux > 9) ? ux - 10 + 'A' : ux + '0');
  ux = u & 0xf;
  print_c((ux > 9) ? ux - 10 + 'A' : ux + '0');
}

//
// Output uint8_t in 3-styles: bin hex ~hex
//
static void print_byte(uint8_t u, uint8_t m) {
  print_bin8(u, m);
  if (m == 0xff) {
    print_c('=');
    print_hex2(u);
    print_c('=');
    print_c('~');
    print_hex2(~u);
    print_c(' ');
  } else {
    print_sP(PSTR("        "));
  }
};

//
// Output uint8_t in ascii
//
static void print_ascii(uint8_t u) {
  char c;
  c = (char)u;
  if (c >= ' ' && c <= '~') {
    print_c(c);
  } else {
    print_c('.');
  }
}

//
// Output uint16_t in hexadecimal[4]
//
static void print_hex4(uint16_t u) {
  print_hex2(u >> 8);
  print_hex2(u & 0xff);
}

// Check if input data exists
//   USART Control and Status Register A
//   Bit 7 – RXC0: USART Receive Complete
//
static uint8_t check_input(void) {
  char typed;
#ifdef __AVR_ATmega328P__
  char __attribute__((unused)) c;  // unused intentionally
  typed = (UCSR0A & _BV(RXC0));
  if (typed) c = UDR0;  // discard typed character
#endif
#ifdef __AVR_ATmega32U4__
  typed = usb_serial_available();
#endif
  return typed;  // true if typed
}
//
// Character input
//
static char input_char(void) {
  char c;
#ifdef __AVR_ATmega328P__
  // loop until  RXC0 bit is set in UCSR0A
  do {
  } while (!(UCSR0A & _BV(RXC0)));
  if (UCSR0A & (_BV(FE0) | _BV(DOR0))) {
    // ERR | EOF --> treat as \n
    // Bit 4 - FE0: Frame Error
    // Bit 3 - DOR0: Data OverRun
    c = UDR0;  // unused intentionally
    c = '\n';  // replace borked input with '\n'
  } else {
    c = UDR0;
    if (c == '\r' || c == (char)0x1b) c = '\n';  // CR, ESC -> NL
    if (c >= 'a' && c <= 'z') c &= ~0x20;        // toupper
  }
#endif
#ifdef __AVR_ATmega32U4__
  int16_t cc;
  cc = usb_serial_getchar();
  if (cc < 0) {
    usb_serial_flush_input();
    c = '\n';  // replace borked input with '\n'
  } else {
    c = cc & 0xff;
  }
#endif
  return c;
}

//
// String input with TAB/BS/^W/^U support
//
static void read_line(char *s) {
  char c;
  char *xs;
  uint8_t i;
  xs = s;
  i = 0;  // 0 ... BUFSIZE-1
  while (1) {
    c = input_char();
    if (i >= BUFSIZE - 1 || c == '\n' || c == 0x1b /* ESC */) {
      c = '\0';
      *xs = c;
      print_c('\n');
      print_c('\r');
      break;
    } else if (c == '\b' || c == 0x7f /* DEL */) {
      if (i > 0) {
        print_c('\b');
        print_c(' ');
        print_c('\b');
        xs--;
        i--;
      }
    } else if (c == ('W' & 0x1f) /* ^W */) {
      while (xs > s && *(xs - 1) == ' ') {
        print_c('\b');
        xs--;
        i--;
      }
      while (xs > s && *(xs - 1) != ' ') {
        print_c('\b');
        print_c(' ');
        print_c('\b');
        xs--;
        i--;
      }
    } else if (c == ('U' & 0x1f) /* ^U */) {
      while (xs > s) {
        print_c('\b');
        print_c(' ');
        print_c('\b');
        xs--;
        i--;
      }
    } else if (c == '\t') {
      do {
        print_c(' ');
        *xs = ' ';
        xs++;
        i++;
      } while (i < (BUFSIZE - 1) && (i % 8) != 0);
      if (i >= (BUFSIZE - 1)) {
        c = '\0';
        *xs = c;
        break;
      }
    } else if (c >= ' ' && c <= '~') {
      *xs = c;
      print_c(c);
      xs++;
      i++;
    } else {
      print_sP(PSTR("\nE: non-valid key code hex="));
      print_hex2(c);
      print_crlf();
    }
  }
}

//
// Convert string to uint8_t
//
// [~](%[01]{1,8}|[0-9a-fA-F]{1,2})
//
// Supported:
//    binary                 %11110100
//    hexadecimal                F4
//    hexadecimal (bit flip)    ~0B
//    mnemonic                 @pinb
//
static uint8_t str2byte(char *s) {
  uint8_t n = 0;
  uint8_t f = 0;
  if (s == NULL) return 0;
  while (*s == ' ') {
    s++;
  }                 // drop ' '
  if (*s == '~') {  // flip bits
    s++;
    f = 1;
  }
  while (*s == ' ') {
    s++;
  }                 // drop ' '
  if (*s == '%') {  // binary
    s++;
    for (uint8_t i = 8; i > 0; --i) {
      if (*s == '1') {
        n = n << 1;
        n |= 1;
      } else if (*s == '0') {
        n = n << 1;
      } else if (*s == 0) {
        break;
      }
      s++;
    }
  } else if ((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'F')) {  // hex
    for (uint8_t i = 2; i > 0; --i) {
      if (*s >= '0' && *s <= '9') {
        n = n << 4;
        n |= *s - '0';
      } else if (*s >= 'A' && *s <= 'F') {
        n = n << 4;
        n |= *s - 'A' + 10;
      } else if (*s == 0) {
        break;
      }
      s++;
    }
  } else {
    n = 0;
  }
  return f ? ~n : n;
}

//
// Convert string to uint16_t
//
// (([+-]|)[0-9a-fA-F]{1,4})+
//
// Supported:
//    hexadecimal                   F402
//    hexadecimal uint16_t add/sub  +100-10+FF-1
//
static uint16_t str2word(char *s) {
  uint16_t n;
  uint16_t m = 0;
  uint8_t f;
  if (*s == '@') {  // mnemonic
    s++;
    if (!strncmp_P(s, PSTR("PIN"), 3)) {
      if (*(s + 3) >= 'A' && *(s + 3) <= 'F') {
        m = 3 * (*(s + 3) - 'A') + 0x23;
      } else {
        m = 0x23;
      }
    } else if (!strncmp_P(s, PSTR("DDR"), 3)) {
      if (*(s + 3) >= 'A' && *(s + 3) <= 'F') {
        m = 3 * (*(s + 3) - 'A') + 0x24;
      } else {
        m = 0x24;
      }
    } else if (!strncmp_P(s, PSTR("PORT"), 4)) {
      if (*(s + 4) >= 'A' && *(s + 4) <= 'F') {
        m = 3 * (*(s + 4) - 'A') + 0x25;
      } else {
        m = 0x25;
      }
#ifdef VERBOSE
    } else if (!strncmp_P(s, PSTR("TIFR"), 4)) {
      if (*(s + 4) >= '0' && *(s + 4) <= '3') {
        m = 3 * (*(s + 4) - '0') + 0x35;
      } else {
        m = 0x35;
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
        m = 0x6e;
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
        m = 0x80;
      }
      // Give up after 0x84
#endif  // VERBOSE
    } else {
      m = 0;
    }
  } else {
    do {
      f = 0;
      if (*s == '-') {  // minus
        s++;
        f = 1;
      } else if (*s == '+') {
        s++;
      }
      n = 0;
      for (uint8_t i = 4; i > 0; --i) {
        if (*s >= '0' && *s <= '9') {
          n = n << 4;
          n |= *s - '0';
          s++;
        } else if (*s >= 'A' && *s <= 'F') {
          n = n << 4;
          n |= *s - 'A' + 10;
          s++;
        } else if (*s == '+' || *s == '-') {
          break;
        } else if (*s == 0) {
          break;
        } else {
          *s = '\0';
          break;
        }
      }
      m += f ? -n : n;
    } while (*s != 0);
  }
  return m;
}
/////////////////////////////////////////////////////////////////////////////
//
// AVR HW monitor and control
//
/////////////////////////////////////////////////////////////////////////////
//
// Memory initialize (HW specific/Localize)
//
/////////////////////////////////////////////////////////////////////////////
void prep_m(volatile uint8_t mask[]) {
  print_sP(PSTR("Monitor all digital inputs\n"));
  DDRB = 0x00;
  DDRC = 0x00;
  DDRD = 0x00;
  print_sP(PSTR("Mask for usable RW PORT\n"));
  // PB5     -- used by Led
  // PB6 PB7 -- used by Xtal
  // PC6     -- used by /RESET
  // PC7     -- not writable
  // PD0 PD1 -- used by serial I/O
  mask[0] = 0b00011111;
  mask[1] = 0b00111111;
  mask[2] = 0b11111100;
}

void prep_s(volatile uint8_t mask[]) {
  print_sP(PSTR("Scan keyboard matrix.  (Top down with USB left\n"));
  print_sP(PSTR("        DDR: OUT = near side: PC0-PC5, PB5=LED\n"));
  print_sP(PSTR("        DDR: IN  = far  side: PB* PD*\n"));
  DDRB = 0b00100000;
  DDRC = 0b00111111;
  DDRD = 0b00000000;
  print_sP(PSTR("Mask for usable RW PORT\n"));
  // PB5     -- used by Led
  // PB6 PB7 -- used by Xtal
  // PC6     -- used by /RESET
  // PC7     -- not writable
  // PD0 PD1 -- used by serial I/O
  mask[0] = 0b00011111;
  mask[1] = 0b00111111;
  mask[2] = 0b11111100;
}

static void analog_off(void) {
  print_sP(PSTR("Digital Input Enabled.  Analog input disabled\n"));
  DIDR0 = 0;
}

void led_on(void) {
  print_sP(PSTR("LED ON\n"));
  LED_CONFIG;
  LED_ON;
}
void led_off(void) {
  print_sP(PSTR("LED OFF\n"));
  LED_CONFIG;
  LED_OFF;
}
void led_blink(uint16_t t) {
  double tt;
  uint8_t typed = 0;
  print_sP(PSTR("LED BLINK START (unit 0.1s, hexadecimal): "));
  print_hex4(t);
  print_crlf();
  do {
    LED_CONFIG;
    LED_ON;
    tt = t;
    do {
      _delay_ms(50);
      if (typed |= check_input()) break;
    } while (tt--);
    LED_OFF;
    tt = t;
    do {
      _delay_ms(50);
      if (typed |= check_input()) break;
    } while (tt--);
  } while (!typed);
  print_sP(PSTR("LED BLINK END\n"));
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

void pullup_port(uint8_t mask[]) {
  uint8_t i;
  print_sP(PSTR("Set PORT* as all pull-up/high for all un-masked\n"));
  for (i = 0; i < N_PORTS; i++) {
    IOR(PORT_0, i) = mask[i];
  }
}

void mask_set(uint8_t mask[]) {
  char sm[BUFSIZE];
  uint8_t i;
  print_sP(PSTR("Mask recommendation: B=1f C=7f D=~3\n"));
  for (i = 0; i < N_PORTS; i++) {
    print_sP(PSTR("   >>> mask for "));
    print_c(PORT_BGN_CH + i);
    print_sP(PSTR(" = "));
    read_line(sm);
    mask[i] = str2byte(sm);
  }
  print_crlf();
}
//
// Print bin dump
//
static void print_bindump(uint16_t a0, uint16_t a1,
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
static void display_digital(uint8_t mask[]) {
  uint8_t i;
  print_sP(PSTR(">>>>>>  state of mask and digital I/O ports  <<<<<<\n"));
  print_sP(PSTR("MASK:   "));
  for (i = 0; i < N_PORTS; i++) {
    print_c('_');
    print_c(PORT_BGN_CH + i);
    print_c('_');
    print_c(':');
    print_byte(mask[i], 0xff);
    print_c(' ');
  }
  print_crlf();
  print_sP(PSTR("PIN:    "));
  for (i = 0; i < N_PORTS; i++) {
    print_c('@');
    print_hex2((uint16_t) & (IOR(PIN_0, i)));
    print_c(':');
    print_byte(IOR(PIN_0, i), 0xff);
    print_c(' ');
  }
  print_crlf();
  print_sP(PSTR("DDR:    "));
  for (i = 0; i < N_PORTS; i++) {
    print_c('@');
    print_hex2((uint16_t) & (IOR(DDR_0, i)));
    print_c(':');
    print_byte(IOR(DDR_0, i), 0xff);
    print_c(' ');
  }
  print_crlf();
  print_sP(PSTR("PORT:   "));
  for (i = 0; i < N_PORTS; i++) {
    print_c('@');
    print_hex2((uint16_t) & (IOR(PORT_0, i)));
    print_c(':');
    print_byte(IOR(PORT_0, i), 0xff);
    print_c(' ');
  }
  print_crlf();
  print_crlf();
}
//
// Monitor
//
static void monitor_digital(uint8_t mask[]) {
  uint8_t i;
  uint8_t same;
  uint8_t typed;
  uint8_t pin0[N_PORTS];  // DDR=0 masked PIN data (new)
  uint8_t pin1[N_PORTS];  // DDR=0 masked PIN data (last)
  //
  for (i = 0; i < N_PORTS; i++) {
    pin0[i] = IOR(PIN_0, i) & (~IOR(DDR_0, i)) & mask[i];
  }
  display_digital(mask);
  print_sP(PSTR("=== Monitoring ... Type any key to exit ===\n"));
  //
  do {
    print_sP(PSTR("MON:    "));
    for (i = 0; i < N_PORTS; i++) {
      print_c('@');
      print_hex2((uint16_t) & (IOR(PIN_0, i)));
      print_c(':');
      pin1[i] = pin0[i];
      print_byte(pin1[i], (~IOR(DDR_0, i)) & mask[i]);
      print_c(' ');
    }
    print_crlf();
    do {
      same = 1;  // same
      for (i = 0; i < N_PORTS; i++) {
        pin0[i] = IOR(PIN_0, i) & (~IOR(DDR_0, i)) & mask[i];
        same = same && (pin1[i] == pin0[i]);
      }
      typed = check_input();
    } while (same && (!typed));
  } while (!typed);
}
//
// Scan
//
static void scan_matrix(uint8_t mask[]) {
  uint8_t i, j;
  uint8_t same;
  uint8_t typed;
  // 3*8*3=72 -- 6*8*6=288
  uint8_t pin0[N_PORTS * 8][N_PORTS];  // DDR=0 masked PIN data (new)
  uint8_t pin1[N_PORTS * 8][N_PORTS];  // DDR=0 masked PIN data (last)
  uint8_t mask_in, mask_out;           // mask setting with DDR
  uint8_t in[N_PORTS * 8];             // read input pin position
  uint8_t out[N_PORTS * 8];            // scan output pin position
  uint8_t n_in;
  uint8_t n_out;
  pullup_port(mask);
  display_digital(mask);
  // check matrix
  n_in = 0;
  n_out = 0;
  for (i = 0; i < N_PORTS; i++) {
    mask_in = (~IOR(DDR_0, i)) & mask[i];
    mask_out = IOR(DDR_0, i) & mask[i];
    for (j = 0; j < 8; j++) {
      if (mask_in & _BV(j)) {
        in[n_in] = (i << 4) | j;
        n_in++;
      }
      if (mask_out & _BV(j)) {
        out[n_out] = (i << 4) | j;
        n_out++;
      }
    }
  }
  // report matrix
  print_sP(PSTR("Scanning cathode side output to low(0): "));
  for (j = 0; j < n_out; j++) {
    print_c('P');
    print_c((out[j] >> 4) + PORT_BGN_CH);
    print_c((out[j] & 0xf) + '0');
    print_c(' ');
  }
  print_crlf();
  print_crlf();
  //
  print_sP(PSTR("Anode-> "));
  for (i = 0; i < n_in; i++) {
    print_c('P');
    print_c((in[i] >> 4) + PORT_BGN_CH);
    print_c((in[i] & 0xf) + '0');
    print_c(' ');
  }
  print_crlf();
  // Display base state
  print_sP(PSTR("Base == "));
  for (i = 0; i < n_in; i++) {
    if ((IOR(PIN_0, (in[i] >> 4))) & (_BV((in[i] & 0xf)))) {
      print_sP(PSTR(" 1  "));
    } else {
      print_sP(PSTR(" 0  "));
    }
  }
  print_crlf();
  //
  for (j = 0; j < n_out; j++) {
    // output port (out[j] >> 4) bit (out[j] & 0xf) to 0
    IOR(PORT_0, (out[j] >> 4)) &= ~_BV((out[j] & 0xf));
    for (i = 0; i < N_PORTS; i++) {
      pin0[j][i] = IOR(PIN_0, i) & (~IOR(DDR_0, i)) & mask[i];
    }
    // output port (out[j] >> 4) bit (out[j] & 0xf) to 1
    IOR(PORT_0, (out[j] >> 4)) |= _BV((out[j] & 0xf));
  }
  //
  while (1) {
    // print key-pressed map
    for (j = 0; j < n_out; j++) {
      print_c('P');
      print_c((out[j] >> 4) + PORT_BGN_CH);
      print_c((out[j] & 0xf) + '0');
      print_sP(PSTR(" -|<-"));
      for (i = 0; i < n_in; i++) {
        if ((pin0[j][(in[i] >> 4)] & _BV((in[i] & 0xf))) == 0) {
          print_c('P');
          print_c((in[i] >> 4) + PORT_BGN_CH);
          print_c((in[i] & 0xf) + '0');
          print_c(' ');
        } else {
          print_sP(PSTR(" -  "));
        }
      }
      for (i = 0; i < N_PORTS; i++) {
        pin1[j][i] = pin0[j][i];
      }
      print_crlf();
    }
    print_sP(PSTR("....... Type any key to exit\n"));
    // loop until change
    do {
      same = 1;  // same
      for (j = 0; j < n_out; j++) {
        // output port (out[j] >> 4) bit (out[j] & 0xf) to 0
        IOR(PORT_0, (out[j] >> 4)) &= ~_BV((out[j] & 0xf));
        for (i = 0; i < N_PORTS; i++) {
          pin0[j][i] = IOR(PIN_0, i) & (~IOR(DDR_0, i)) & mask[i];
          same = same && (pin1[j][i] == pin0[j][i]);
        }
        // output port (out[j] >> 4) bit (out[j] & 0xf) to 1
        IOR(PORT_0, (out[j] >> 4)) |= _BV((out[j] & 0xf));
      }
      typed = check_input();
    } while (same && (!typed));
    if (typed) break;
    print_up(n_out + 1);
  }
}
//
// Analog
//
// oversampling 2^OSPWR times (OSPWR=0..6)
#define OSPWR 4
static void monitor_analog(uint8_t admux, uint8_t adps) {
  uint8_t i, j, k;
  uint16_t av;
  print_sP(PSTR("Analog Input Enabled.  Digital input disabled\n"));
  DIDR0 = 0x3f;  // Digital Input Disable
  // Vref setting
  // ADMUX = _BV(REFS0)| BV(REFS1)   // Vref = Int. (1.1V for m328p)
  // ADMUX = _BV(REFS0);             // Vref = AVcc (5V)
  // ADMUX = 0;                      // Vref = External
  // ADMUX |= _BV(ADLAR);            // left justified
  ADMUX = admux & 0xc0;  // REFS1, REFS0 / Right just.
  // ADCSRA = _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0);// prescaler /128
  ADCSRA = (adps & 0x7) | _BV(ADEN);  // ADC Enable 2^adps
  while (1) {
    for (i = 0; i < 11; i++) {
      j = ((i > 8) ? (i + 5) : i);
      ADMUX = (ADMUX & 0xe0) | j;  // MUX* setting
      av = 0;
      for (k = (1 << OSPWR); k > 0; k--) {
        ADCSRA |= _BV(ADSC);  // ADC Start Conversion
        do {
        } while (ADCSRA & _BV(ADSC));  // until clear
        av += ADC;
      }
      print_hex4(av >> OSPWR);
      if (j < 8) {
        print_sP(PSTR("  ADC/PC"));
        print_hex2(j);
      } else if (j == 8) {
        print_sP(PSTR("  Temperature Sensor"));
      } else if (j == 0xe) {
        print_sP(PSTR("  1.1V Internal Ref."));
      } else if (j == 0xf) {
        print_sP(PSTR("  0.0V"));
      }
      print_crlf();
    }
    if (check_input()) break;
    print_up(11);
  }
}

/////////////////////////////////////////////////////////////////////////////
//
// System monitor
//
/////////////////////////////////////////////////////////////////////////////
int main(void) {
  // command line tokenizer
  char s[BUFSIZE];               // line
  char sx[BUFSIZE] = "R 23 2B";  // lastline
  // mask settings
  uint8_t mask[N_PORTS];
  char *str_main, *str_sub;                                 // str
  char *token_main, *token_sub1, *token_sub2, *token_sub3;  // token
  char *saveptr_main, *saveptr_sub;                         // saveptr
  // read sram bindump
  uint16_t addr_r0 = 0x23;  // start
  uint16_t addr_r1 = 0x2b;  // end
  // read sram hexdump
  uint16_t addr_s0 = 0x00;  // start
  uint16_t addr_s1 = 0xff;  // end
  uint16_t addr_s2 = 0xff;  // range
  // weite sram bindump
  uint8_t val;
  uint16_t addr_w0 = 0x23;  // start
  // read flash hexdump
  uint16_t addr_f0 = 0x00;  // start
  uint16_t addr_f1 = 0xff;  // end
  uint16_t addr_f2 = 0xff;  // range
  // initialize
  init_comm();
  print_sP(PSTR("AVRmon v 0.1\n"));
  val = _SFR_MEM8(addr_w0);
  print_sP(PSTR("  mcu=" QS(MCU) " baud=" QS(BAUD) " f_cpu=" QS(F_CPU) "\n"));
  print_sP(PSTR("  GPL 2.0+, Copyright 2020, <osamu@debian.org>\n\n"));
  //
  // display mcu states
  display_digital(mask);
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
    }
    while (1) {
      // tokenize (if zero length, makwe it NULL)
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
        // process tokens
        if (!strcmp_P(token_sub1, PSTR("D"))) {
          // display mcu states
          display_digital(mask);
        } else if (!strcmp_P(token_sub1, PSTR("M"))) {
          monitor_digital(mask);
        } else if (!strcmp_P(token_sub1, PSTR("S"))) {
          scan_matrix(mask);
        } else if (!strcmp_P(token_sub1, PSTR("A"))) {
          // AVcc(5V) as Vref, prescale 1/128, 15 bits
          if (token_sub2 == NULL) {
            monitor_analog(_BV(REFS0), _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0));
          } else if (!strcmp_P(token_sub2, PSTR("F"))) {
            analog_off();
          } else if (token_sub3 == NULL) {
            monitor_analog(str2byte(token_sub2) & 0xc0,
                           _BV(ADPS2) | _BV(ADPS1) | _BV(ADPS0));
          } else {
            monitor_analog(str2byte(token_sub2) & 0xc0,
                           str2byte(token_sub3) & 0x07);
          }
        } else if (!strcmp_P(token_sub1, PSTR("MASK"))) {
          // set mask values
          mask_set(mask);
          display_digital(mask);
        } else if (!strcmp_P(token_sub1, PSTR("P"))) {
          if (!strcmp_P(token_sub2, PSTR("S"))) {
            // initialize for scanning key matrix
            prep_s(mask);
          } else {
            // initialize for monitoring pins
            prep_m(mask);
          }
          pullup_port(mask);
          display_digital(mask);
        } else if (!strcmp_P(token_sub1, PSTR("R"))) {
          // read from SRAM (bindump) same location
          if (token_sub2 != NULL) addr_r0 = str2word(token_sub2);
          if (token_sub3 != NULL) addr_r1 = str2word(token_sub3);
          if (addr_r1 < addr_r0) addr_r1 = addr_r0;
          print_bindump(addr_r0, addr_r1, &read_sram);
        } else if (!strcmp_P(token_sub1, PSTR("RS"))) {
          // read from SRAM (hexdump) progressive location
          if (token_sub2 != NULL) addr_s0 = str2word(token_sub2);
          if (token_sub3 != NULL) addr_s1 = str2word(token_sub3);
          if (addr_s0 > MAX_SRAM) addr_s0 = 0;
          if (addr_s1 < addr_s0) addr_s1 = addr_s0;
          print_hexdump(addr_s0, addr_s1, &read_sram);
          addr_s2 = addr_s1 - addr_s0;
          addr_s0 = addr_s0 + addr_s2 + 1;
          if (addr_s0 > MAX_SRAM) addr_s0 = 0;
          addr_s1 = addr_s0 + addr_s2;
          if (addr_s1 > MAX_SRAM) addr_s1 = MAX_SRAM;
        } else if (!strcmp_P(token_sub1, PSTR("W"))) {
          // write to SRAM (bindump) progressive location
          if (token_sub2 != NULL) val = str2byte(token_sub2);
          if (token_sub3 != NULL) addr_w0 = str2word(token_sub3);
          if (addr_w0 < MIN_SRAM) break;  // ignore
          if (addr_w0 > MAX_SRAM) break;  // ignore
          write_sram(addr_w0, val);
          print_bindump(addr_w0, addr_w0, &read_sram);
          addr_w0++;
          if (addr_w0 > MAX_SRAM) addr_w0 = MIN_SRAM;
          print_bindump(addr_w0, addr_w0, &read_sram);
        } else if (!strcmp_P(token_sub1, PSTR("WA"))) {
          // write to SRAM (bindump) progressive location
          if (token_sub2 != NULL) val = str2byte(token_sub2);
          if (token_sub3 != NULL) addr_w0 = str2word(token_sub3);
          if (addr_w0 < MIN_SRAM) break;  // ignore
          if (addr_w0 > MAX_SRAM) break;  // ignore
          write_and_sram(addr_w0, val);
          print_bindump(addr_w0, addr_w0, &read_sram);
          addr_w0++;
          if (addr_w0 > MAX_SRAM) addr_w0 = MIN_SRAM;
          print_bindump(addr_w0, addr_w0, &read_sram);
        } else if (!strcmp_P(token_sub1, PSTR("WO"))) {
          // write to SRAM (bindump) progressive location
          if (token_sub2 != NULL) val = str2byte(token_sub2);
          if (token_sub3 != NULL) addr_w0 = str2word(token_sub3);
          if (addr_w0 < MIN_SRAM) break;  // ignore
          if (addr_w0 > MAX_SRAM) break;  // ignore
          write_or_sram(addr_w0, val);
          print_bindump(addr_w0, addr_w0, &read_sram);
          addr_w0++;
          if (addr_w0 > MAX_SRAM) addr_w0 = MIN_SRAM;
          print_bindump(addr_w0, addr_w0, &read_sram);
        } else if (!strcmp_P(token_sub1, PSTR("RF"))) {
          // read from FLASH (hexdump) progressive location
          if (token_sub2 != NULL) addr_f0 = str2word(token_sub2);
          if (token_sub3 != NULL) {
            addr_f1 = str2word(token_sub3);
            if (addr_f1 < addr_f0) addr_f1 = addr_f0;
          } else {
            addr_f1 = addr_f0 + addr_f2;
          }
          print_hexdump(addr_f0, addr_f1, &read_flash);
          addr_f2 = addr_f1 - addr_f0;
          addr_f0 = addr_f0 + addr_f2 + 1;
          if (addr_f0 > MAX_FLASH) addr_f0 = 0;
          addr_f1 = addr_f0 + addr_f2;
          if (addr_f1 > MAX_FLASH) addr_f1 = MAX_FLASH;
        } else if (!strcmp_P(token_sub1, PSTR("L"))) {
          if (!strcmp_P(token_sub2, PSTR("B"))) {
            led_blink(str2word(token_sub3));
          } else if (!strcmp_P(token_sub2, PSTR("F"))) {
            led_off();
          } else {
            led_on();
          }
          display_digital(mask);
        } else if (!strcmp_P(token_sub1, PSTR("?"))) {
          print_sP(PSTR("byte -> bin / hex / ~hex / ascii >> "));
          print_byte(str2byte(token_sub2), 0xff);
          print_ascii(str2byte(token_sub2));
          print_crlf();
        } else if (!strcmp_P(token_sub1, PSTR("??"))) {
          print_sP(PSTR("word -> hex / ~hex >> "));
          print_hex4(str2word(token_sub2));
          print_sP(PSTR(" = ~"));
          print_hex4(~str2word(token_sub2));
          print_crlf();
          //        } else if (!strcmp_P(token_sub1, PSTR("PGM_P"))) {
          //          print_sP(PSTR(">> sizeof(PGM_P)= "));
          //          print_hex4(sizeof(PGM_P));
          //          print_crlf();
          //          print_c('\n');
          //          print_c('\r');
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
        str_main = NULL;
      }
    }
  }
  print_sP(PSTR("\n!!! NEVER HERE !!! DEAD !!!\n"));
}
