// vim: ft=c sts=2 sw=2 et si ai:
// subroutines
//
// C headers (C99 with GNU extension)
//
#include "config.h"
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
// Initialize Character I/O with F_CPU/BAUD
//
void init_comm(void) {
#ifdef IO_SERIAL
  UBRR0H = UBRRH_VALUE;
  UBRR0L = UBRRL_VALUE;
#if USE_2X
  UCSR0A |= _BV(U2X0);
#else
  UCSR0A &= ~_BV(U2X0);
#endif
  // async, non-parity, 1-bit stop, 8-bit data
  UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);
  // tx and rx enable
  UCSR0B = _BV(TXEN0) | _BV(RXEN0);
  // All Data Direction Register for input
  DDRB = DDRC = DDRD = 0;
  // pull-up for all
  PORTB = PORTC = PORTD = 0xff;
#endif
#ifdef IO_USB
  // initialize the USB, and then wait for the host
  // to set configuration.  If the Teensy is powered
  // without a PC connected to the USB port, this
  // will wait forever.
  usb_init();
  do {
  } while (!usb_configured()); /* wait */
  // wait for the user to run their terminal emulator program
  // which sets DTR to indicate it is ready to receive.
  while (!(usb_serial_get_control() & USB_SERIAL_DTR)) /* wait */ ;

  // discard anything that was received prior.  Sometimes the
  // operating system or other software will send a modem
  // "AT command", which can still be buffered.
  usb_serial_flush_input();
#endif
}

//
// Character output
//
void print_c(char c) {
#ifdef IO_SERIAL
  // loop until UDRE0 bit is set in UCSR0A
  do {
  } while (!(UCSR0A & _BV(UDRE0)));
  UDR0 = c;
#endif
#ifdef IO_USB
  usb_serial_putchar(c);
#endif
}

//
// Control character output - down 1 line
//
void print_crlf(void) {
  print_c('\r');
  print_c('\n');
}

//
// Control sequence output  - up 1 line on terminal
//
void print_up(uint8_t n) {
  while (n-- > 0) {
    print_c(0x1b);
    print_c('[');
    print_c('A');
  }
}

//
// String output
//
void print_s(char *s) {
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

void print_sP(PGM_P s) {
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
// Output uint8_t u in binary[8] with mask=m
//
void print_bin8(uint8_t u, uint8_t m) {
  // if a bit in m = 0 -> then print '_'
  for (int8_t i = 7; i >= 0; i--) {
    print_c((m & _BV(i)) ? ((u & _BV(i)) ? '1' : '0') : '_');
  }
}

//
// Output uint8_t in hexadecimal[1]
//
void print_hex1(uint8_t u) {
  uint8_t ux;
  ux = u & 0xf;
  print_c((ux > 9) ? ux - 10 + 'A' : ux + '0');
}

//
// Output uint8_t in hexadecimal[2]
//
void print_hex2(uint8_t u) {
  print_hex1(u >> 4);
  print_hex1(u & 0xf);
}

//
// Output uint8_t in 3-styles: bin hex ~hex
//
void print_byte(uint8_t u, uint8_t m) {
  print_bin8(u, m);
  print_c('=');
  print_hex2(u & m);
  print_c('=');
  print_c('~');
  print_hex2(~(u & m));
  print_c(' ');
};

//
// Output uint8_t in ascii
//
void print_ascii(uint8_t u) {
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
void print_hex4(uint16_t u) {
  print_hex2(u >> 8);
  print_hex2(u & 0xff);
}

// Check if input data exists
//   USART Control and Status Register A
//   Bit 7 – RXC0: USART Receive Complete
//
uint8_t check_input(void) {
  char typed;
#ifdef IO_SERIAL
  char __attribute__((unused)) c;  // unused intentionally
  typed = (UCSR0A & _BV(RXC0));
  if (typed) c = UDR0;  // discard typed character
#endif
#ifdef IO_USB
  typed = usb_serial_available();
#endif
  return typed;  // true if typed
}
//
// Character input
//
char input_char(void) {
  char c;
#ifdef IO_SERIAL
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
  }
#endif
#ifdef IO_USB
  int16_t cc;
  while (1) {
    cc = usb_serial_getchar();
    if (cc != -1) {
      break;
    }
    // loop until you get serial input
  }
  if (cc <= 0 ) {
    print_sP(PSTR("\nE: Oops, non-valid negative key code hex="));
    print_hex4(cc);
    print_crlf();
    c = '\n';  // replace borked input with '\n'
  }
  c = cc & 0xff;
#endif
  if (c == '\r' || c == (char)0x1b) c = '\n';  // CR, ESC -> NL
  if (c >= 'a' && c <= 'z') c &= ~0x20;        // to upper case
  return c;
}

//
// String input with TAB/BS/^W/^U support
//
void read_line(char *s) {
  char c;
  char *xs;
  uint8_t i;
  xs = s;
  i = 0;  // 0 ... BUFSIZE-1
  while (1) {
    c = input_char();
    if (i >= BUFSIZE - 1 || c == '\n' || c == '\r' || c == 0x1b /* ESC */) {
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
uint8_t str2byte(char *s) {
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
      if (*s == '\0') break;
      n = n << 1;
      if (*s == '1') {
        n |= 1;
      } else if (*s == 0) {
        break; // this may be shorter than 8 bits
      }
      s++;
    }
  } else if ((*s >= '0' && *s <= '9') || (*s >= 'A' && *s <= 'F')) {  // hex
    for (uint8_t i = 2; i > 0; --i) {
      if (*s == '\0') break;
      n = n << 4;
      if (*s >= '0' && *s <= '9') {
        n |= *s - '0';
      } else if (*s >= 'A' && *s <= 'F') {
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

