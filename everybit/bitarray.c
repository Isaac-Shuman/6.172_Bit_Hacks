/**
 * Copyright (c) 2012 MIT License by 6.172 Staff
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 **/

// Implements the ADT specified in bitarray.h as a packed array of bits; a bit
// array containing bit_sz bits will consume roughly bit_sz/8 bytes of
// memory.


#include "./bitarray.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>

#include <sys/types.h>

//Added by isaac
#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define WORD_SIZE 64
#define TRAIL(x) (UINT64_MAX >> (WORD_SIZE-(x)))
#define LEAD(x) (UINT64_MAX << (WORD_SIZE-(x)))

// ********************************* Types **********************************

// Concrete data type representing an array of bits.
struct bitarray {
  // The number of bits represented by this bit array.
  // Need not be divisible by 8.
  size_t bit_sz;

  // The underlying memory buffer that stores the bits in
  // packed form (8 per byte).
  char* buf;
};

typedef uint64_t word;

// ******************** Prototypes for static functions *********************

// Rotates a subarray left by an arbitrary number of bits.
//
// bit_offset is the index of the start of the subarray
// bit_length is the length of the subarray, in bits
// bit_left_amount is the number of places to rotate the
//                    subarray left
//
// The subarray spans the half-open interval
// [bit_offset, bit_offset + bit_length)
// That is, the start is inclusive, but the end is exclusive.
static void bitarray_rotate_left(bitarray_t* const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount);

// Rotates a subarray left by one bit.
//
// bit_offset is the index of the start of the subarray
// bit_length is the length of the subarray, in bits
//
// The subarray spans the half-open interval
// [bit_offset, bit_offset + bit_length)
// That is, the start is inclusive, but the end is exclusive.
static void bitarray_rotate_left_one(bitarray_t* const bitarray,
                                     const size_t bit_offset,
                                     const size_t bit_length);

// Portable modulo operation that supports negative dividends.
//
// Many programming languages define modulo in a manner incompatible with its
// widely-accepted mathematical definition.
// http://stackoverflow.com/questions/1907565/c-python-different-behaviour-of-the-modulo-operation
// provides details; in particular, C's modulo
// operator (which the standard calls a "remainder" operator) yields a result
// signed identically to the dividend e.g., -1 % 10 yields -1.
// This is obviously unacceptable for a function which returns size_t, so we
// define our own.
//
// n is the dividend and m is the divisor
//
// Returns a positive integer r = n (mod m), in the range
// 0 <= r < m.
static size_t modulo(const ssize_t n, const size_t m);

// Produces a mask which, when ANDed with a byte, retains only the
// bit_index th byte.
//
// Example: bitmask(5) produces the byte 0b00100000.
//
// (Note that here the index is counted from right
// to left, which is different from how we represent bitarrays in the
// tests.  This function is only used by bitarray_get and bitarray_set,
// however, so as long as you always use bitarray_get and bitarray_set
// to access bits in your bitarray, this reverse representation should
// not matter.
static char bitmask(const size_t bit_index);

static void bitarray_reverse_slow(bitarray_t* const bitarray,
                                  const size_t bit_offset,
                                  const size_t bit_length);

static void bitarray_rotate_slow(bitarray_t* const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount);

static word bitarray_get_word(const bitarray_t* const bitarray, const size_t bit_index);

void do_isaac_stuff(void);

static void print_word(word a_word);

static void print_bitarray(const bitarray_t* const bitarray);
// ******************************* Functions ********************************

bitarray_t* bitarray_new(const size_t bit_sz) {
  // Allocate an underlying buffer of ceil(bit_sz/8) bytes.
  char* const buf = calloc(1, (bit_sz+7) / 8);
  if (buf == NULL) {
    return NULL;
  }

  // Allocate space for the struct.
  bitarray_t* const bitarray = malloc(sizeof(struct bitarray));
  if (bitarray == NULL) {
    free(buf);
    return NULL;
  }

  bitarray->buf = buf;
  bitarray->bit_sz = bit_sz;
  return bitarray;
}

void bitarray_free(bitarray_t* const bitarray) {
  if (bitarray == NULL) {
    return;
  }
  free(bitarray->buf);
  bitarray->buf = NULL;
  free(bitarray);
}

size_t bitarray_get_bit_sz(const bitarray_t* const bitarray) {
  return bitarray->bit_sz;
}

bool bitarray_get(const bitarray_t* const bitarray, const size_t bit_index) {
  assert(bit_index < bitarray->bit_sz);

  // We're storing bits in packed form, 8 per byte.  So to get the nth
  // bit, we want to look at the (n mod 8)th bit of the (floor(n/8)th)
  // byte.
  //
  // In C, integer division is floored explicitly, so we can just do it to
  // get the byte; we then bitwise-and the byte with an appropriate mask
  // to produce either a zero byte (if the bit was 0) or a nonzero byte
  // (if it wasn't).  Finally, we convert that to a boolean.
  return (bitarray->buf[bit_index / 8] & bitmask(bit_index)) ?
         true : false;
}

void bitarray_set(bitarray_t* const bitarray,
                  const size_t bit_index,
                  const bool value) {
  assert(bit_index < bitarray->bit_sz);

  // We're storing bits in packed form, 8 per byte.  So to set the nth
  // bit, we want to set the (n mod 8)th bit of the (floor(n/8)th) byte.
  //
  // In C, integer division is floored explicitly, so we can just do it to
  // get the byte; we then bitwise-and the byte with an appropriate mask
  // to clear out the bit we're about to set.  We bitwise-or the result
  // with a byte that has either a 1 or a 0 in the correct place.
  bitarray->buf[bit_index / 8] =
    (bitarray->buf[bit_index / 8] & ~bitmask(bit_index)) |
    (value ? bitmask(bit_index) : 0);
}

void bitarray_randfill(bitarray_t* const bitarray){
  int32_t *ptr = (int32_t *)bitarray->buf;
  for (int64_t i=0; i<bitarray->bit_sz/32 + 1; i++){
    ptr[i] = rand();
  }
}

void bitarray_rotate(bitarray_t* const bitarray,
                     const size_t bit_offset,
                     const size_t bit_length,
                     const ssize_t bit_right_amount) {
  assert(bit_offset + bit_length <= bitarray->bit_sz);

  if (bit_length == 0) {
    return;
  }

  // Convert a rotate left or right to a left rotate only, and eliminate
  // multiple full rotations.
  // bitarray_rotate_left(bitarray, bit_offset, bit_length,
  //                      modulo(-bit_right_amount, bit_length));
  bitarray_rotate_slow(bitarray,
                       bit_offset,
                       bit_length,
                       modulo(-bit_right_amount, bit_length));
}

static void bitarray_rotate_left(bitarray_t* const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount) {
  for (size_t i = 0; i < bit_left_amount; i++) {
    bitarray_rotate_left_one(bitarray, bit_offset, bit_length);
  }
}

static void bitarray_rotate_left_one(bitarray_t* const bitarray,
                                     const size_t bit_offset,
                                     const size_t bit_length) {
  // Grab the first bit in the range, shift everything left by one, and
  // then stick the first bit at the end.
  const bool first_bit = bitarray_get(bitarray, bit_offset);
  size_t i;
  for (i = bit_offset; i + 1 < bit_offset + bit_length; i++) {
    bitarray_set(bitarray, i, bitarray_get(bitarray, i + 1));
  }
  bitarray_set(bitarray, i, first_bit);
}

static size_t modulo(const ssize_t n, const size_t m) {
  const ssize_t signed_m = (ssize_t)m;
  assert(signed_m > 0);
  const ssize_t result = ((n % signed_m) + signed_m) % signed_m;
  assert(result >= 0);
  return (size_t)result;
}

static char bitmask(const size_t bit_index) {
  return 1 << (bit_index % 8);
}

// ******************************* Functions. Added By Isaac ***********************

static void bitarray_reverse_slow(bitarray_t* const bitarray,
                                  const size_t bit_offset,
                                  const size_t bit_length) {
  int lp = bit_offset;
  int rp = bit_offset + bit_length - 1;
  bool lbit, rbit;
  for(int i = 0; i < bit_length/2 ; i++) {
    lbit = bitarray_get(bitarray, lp);
    rbit = bitarray_get(bitarray, rp);
    bitarray_set(bitarray, lp, rbit);
    bitarray_set(bitarray, rp, lbit);
    lp++;
    rp--;
  }
}

static void bitarray_rotate_slow(bitarray_t* const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount) {
  bitarray_reverse_slow(bitarray, bit_offset, bit_left_amount); //reverse a
  bitarray_reverse_slow(bitarray, bit_offset + bit_left_amount, bit_length-bit_left_amount); //reverse b
  bitarray_reverse_slow(bitarray, bit_offset, bit_length);
}

word bitarray_get_aligned_block(const bitarray_t *const bitarray, const size_t byte_index) {
  //assert(byte_index*8 < bitarray->bit_sz); 
  return ((word *) bitarray->buf)[byte_index];
}

static word bitarray_get_word(const bitarray_t* const bitarray, const size_t bit_index) {
	word result;
  //this does not work as intendend because it is stored right to left instead of left to right.
	word lw = ((word *) bitarray->buf)[bit_index/WORD_SIZE];
	word rw = ((word *) bitarray->buf)[bit_index/WORD_SIZE + 1]; //relies on buf contianing a 64 bit word to the right of the word containing the current bi
  printf("lw is: ");
  print_word(lw);
  printf("lw is: %lX\n", lw);
  printf("rw is: ");
  print_word(rw);
  printf("rw is: %lX\n", rw);
  
  uint_fast8_t x = WORD_SIZE - modulo(bit_index, WORD_SIZE);

	//lw = ((word) (lw & TRAIL(x))) >> ((uint_fast8_t) (WORD_SIZE - x)); //the &s here are redundant?
  lw = lw >> (WORD_SIZE - x);
	rw = rw << x; //how shifts manipulate the underlying memory is dependent on endianess
  //rw = ((word) (rw & LEAD(WORD_SIZE - x))) >> (x);

  printf("x is: %hhi\n", x);
  printf("lw shifted is: ");
  print_word(lw);
    printf("lw is: %lX\n", lw);
  printf("rw shifted is: ");
  print_word(rw);
  printf("rw is: %lX\n", rw);
	
	result = lw | rw;
  return result;
} 
static void print_bitarray(const bitarray_t* const bitarray) {
  for (size_t bit_index = 0; bit_index < bitarray->bit_sz; bit_index++)
    printf("%s", bitarray_get(bitarray, bit_index) ? "1" : "0");
  printf("\n");
}

static void print_word(const word a_word) {
  bitarray_t* word_bitarray = bitarray_new((size_t) 64);
  memcpy(word_bitarray->buf, &a_word, sizeof(a_word));
  print_bitarray(word_bitarray);
  free(word_bitarray);
}
void do_isaac_stuff(void) {
  struct bitarray* a_bitarray = bitarray_new((size_t) 128);
  char values[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x1F, 0x2E, 0x3D, 0x4C, 0x5B, 0x6A, 0x79, 0x88};
  memcpy(a_bitarray->buf, values, sizeof(values));
  // for (int i = 0; i < sizeof(values); i++) {
  //   printf("%hhX ", ( a_bit_array->buf)[i]);
  // }
  print_bitarray(a_bitarray);

  word a_word = bitarray_get_word(a_bitarray, 63);
  print_word(a_word);
  
  free(a_bitarray);
  a_bitarray = NULL;
  printf("The word retrieved is: %lX\n", a_word);
  //initialize bitarray to a know value
  //try getting a word, printf(hex)
  //cry when it fails
}