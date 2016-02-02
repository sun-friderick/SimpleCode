/* Code for general_printf() */
/* Change extension to .c before compiling */
#ifndef __GENERAL_PRINTF_H__
#define __GENERAL_PRINTF_H__



#define BITS_PER_BYTE           8

struct parameters
{
  int number_of_output_chars;
  short minimum_field_width;
  char options;
    #define MINUS_SIGN    1
    #define RIGHT_JUSTIFY 2
    #define ZERO_PAD      4
    #define CAPITAL_HEX   8
  short edited_string_length;
  short leading_zeros;
  int (*output_function)(void *, int);
  void *output_pointer;
};

#ifdef __cplusplus
extern "C"{
#endif

int general_printf(int (*output_function)(void *, int), void *output_pointer, const char *control_string, const int *argument_pointer);

#ifdef __cplusplus
}
#endif

#endif  //__GENERAL_PRINTF_H__