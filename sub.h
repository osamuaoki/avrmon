// subroutines
#ifndef sub_h__
#define sub_h__

#include <stdint.h>
#include <avr/pgmspace.h>

void init_comm(void) ;
void print_c(char c);
void print_up(uint8_t n);
void print_s(char *s);
void print_sP(PGM_P s);
void print_crlf(void);
void print_bin8(uint8_t u, uint8_t m);
void print_hex1(uint8_t u);
void print_hex2(uint8_t u);
void print_byte(uint8_t u, uint8_t m);
void print_ascii(uint8_t u);
void print_hex4(uint16_t u);
uint8_t check_input(void);
char input_char(void);
void read_line(char *s);
uint8_t str2byte(char *s);

#endif
