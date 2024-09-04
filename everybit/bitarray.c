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
#define TRAIL(x) (((x) > 0) ? (UINT64_MAX << (WORD_SIZE-(x))) : 0)
#define LEAD(x) (((x) > 0) ? (UINT64_MAX >> (WORD_SIZE-(x))) : 0)

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

static void print_bitarray(const bitarray_t* const bitarray, const size_t bit_index);

static void bitarray_set_word(const bitarray_t* const bitarray, const size_t bit_index, const word a_word);

static word reverse_word(word v);

static void bitarray_reverse_fast(bitarray_t* const bitarray,
                                  const size_t bit_offset,
                                  const size_t bit_length);

static bitarray_t* bitarray_newrand(const size_t bit_sz, const unsigned int seed);

static void test_reverse(const unsigned int seed, const size_t bit_sz, const size_t bit_offset, const size_t bit_length);

static void test_rotate(const unsigned int seed, const size_t bit_sz, const size_t bit_offset, const size_t bit_length, const size_t bit_left_amount);

static void bitarray_rotate_fast(bitarray_t* const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount);

static size_t bitarray_cmp(const bitarray_t* const first, const bitarray_t* const second);
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
  bitarray_rotate_fast(bitarray,
                       bit_offset,
                       bit_length,
                       modulo(-bit_right_amount, bit_length));
  bitarray_randfill(bitarray);
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

static void bitarray_reverse_fast(bitarray_t* const bitarray,
                                  const size_t bit_offset,
                                  const size_t bit_length) {
  //assert(bit_offset + bit_length <= bitarray->bit_sz);
  size_t lp = bit_offset;
  size_t rp = bit_offset + bit_length -1;
  bool lbit, rbit;
  word lword, rword;
  //assert(rp < bitarray->bit_sz);
  if (bit_length < WORD_SIZE*4) {
    bitarray_reverse_slow(bitarray, bit_offset, bit_length);
  }
  else {
    for(int i = 0; i < 2*WORD_SIZE ; i++) {
      lbit = bitarray_get(bitarray, lp);
      rbit = bitarray_get(bitarray, rp);
      bitarray_set(bitarray, lp, rbit);
      bitarray_set(bitarray, rp, lbit);
      lp++;
      rp--;
    }
    rp -= WORD_SIZE - 1;
    //assert(lp < rp);
    #ifdef IDEBUG
    printf("bitarray prior to paralleism: ");
    print_bitarray(bitarray, 257);
    #endif
    while(lp <= rp - WORD_SIZE) {
      #ifdef IDEBUG
      printf("we're actually using parallelism\n");
      #endif
      lword = bitarray_get_word(bitarray, lp);
      rword = bitarray_get_word(bitarray, rp);

      #ifdef IDEBUG
      printf("lword is: ");
      print_word(lword);
      printf("rword is: ");
      print_word(rword);
      printf("lword reverse is: ");
      print_word(reverse_word(lword));
      printf("rword reverse is: "); 
      print_word(reverse_word(rword));
      #endif
      bitarray_set_word(bitarray, lp, reverse_word(rword));
      bitarray_set_word(bitarray, rp, reverse_word(lword));
      lp+=WORD_SIZE;
      rp-=WORD_SIZE;
    }
    rp += WORD_SIZE-1;
    #ifdef IDEBUG
    printf("lp is: %i and rp is: %i\n", lp, rp);
    #endif
    while (lp < rp) {
      lbit = bitarray_get(bitarray, lp);
      rbit = bitarray_get(bitarray, rp);
      bitarray_set(bitarray, lp, rbit);
      bitarray_set(bitarray, rp, lbit);
      lp++;
      rp--;
    }
  }
  return;
}

static void bitarray_rotate_slow(bitarray_t* const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount) {
  assert(bit_length >= bit_left_amount);
  bitarray_reverse_slow(bitarray, bit_offset, bit_left_amount); //reverse a
  bitarray_reverse_slow(bitarray, bit_offset + bit_left_amount, bit_length-bit_left_amount); //reverse b
  bitarray_reverse_slow(bitarray, bit_offset, bit_length);
}

static void bitarray_rotate_fast(bitarray_t* const bitarray,
                                 const size_t bit_offset,
                                 const size_t bit_length,
                                 const size_t bit_left_amount) {
  assert(bit_length >= bit_left_amount);
  bitarray_reverse_fast(bitarray, bit_offset, bit_left_amount); //reverse a
  bitarray_reverse_fast(bitarray, bit_offset + bit_left_amount, bit_length-bit_left_amount); //reverse b
  bitarray_reverse_fast(bitarray, bit_offset, bit_length);
}

word bitarray_get_aligned_block(const bitarray_t *const bitarray, const size_t byte_index) {
  //assert(byte_index*8 < bitarray->bit_sz); 
  return ((word *) bitarray->buf)[byte_index];
}

static word bitarray_get_word(const bitarray_t* const bitarray, const size_t bit_index) {
  #ifdef IDEBUG
  printf("bit_index: %lu. bitarray->bit_size: %lu\n", bit_index, bitarray->bit_sz);
  #endif
  assert(bit_index <= bitarray->bit_sz - 2*WORD_SIZE);
	word result;
  //this does not work as intendend because it is stored right to left instead of left to right.
	word lw = ((word *) bitarray->buf)[bit_index/WORD_SIZE];
	word rw = ((word *) bitarray->buf)[bit_index/WORD_SIZE + 1]; //relies on buf contianing a 64 bit word to the right of the word containing the current bi
  #ifdef IDEBUG
  printf("lw is: ");
  print_word(lw);
  //printf("lw is: %lX\n", lw);
  printf("rw is: ");
  print_word(rw);
  //printf("rw is: %lX\n", rw);
  #endif
  uint_fast8_t x = WORD_SIZE - modulo(bit_index, WORD_SIZE);

	//lw = ((word) (lw & TRAIL(x))) >> ((uint_fast8_t) (WORD_SIZE - x)); //the &s here are redundant?
  if (WORD_SIZE - x >= WORD_SIZE)
    lw = 0;
  else
    lw = lw >> (WORD_SIZE - x);
  if (x >= WORD_SIZE)
    rw = 0;
  else
    rw = rw << x; //how shifts manipulate the underlying memory is dependent on endianess
  //rw = ((word) (rw & LEAD(WORD_SIZE - x))) >> (x);

  #ifdef IDEBUG
  // printf("x is: %hhi\n", x);
  printf("lw shifted is: ");
  print_word(lw);
  //   printf("lw is: %lX\n", lw);
  printf("rw shifted is: ");
  print_word(rw);
  // printf("rw is: %lX\n", rw);
	#endif
	result = lw | rw;
  // printf("the result is: ");
  // print_word(result);

  return result;
} 

static size_t bitarray_cmp(const bitarray_t* const first, const bitarray_t* const second) {
  //assert sizes are equal
  if (first->bit_sz != second->bit_sz)
    return 1;
  
  //iterate through both buffers and check they are equal
  for (size_t bit_index = 0; bit_index < first->bit_sz; bit_index++)
    if (bitarray_get(first, bit_index) != bitarray_get(second, bit_index))
      return bit_index;
  return 0;
}
static bool bitarray_copy(const bitarray_t* const src, bitarray_t* dst) {
  //assert sizes are equal
  if (src->bit_sz != dst->bit_sz)
    return false;
  for (size_t bit_index = 0; bit_index < src->bit_sz; bit_index++)
    bitarray_set(dst, bit_index, bitarray_get(src, bit_index));
  return true;
}

static void bitarray_set_word(const bitarray_t* const bitarray, const size_t bit_index, const word a_word) {
  assert(bit_index <= bitarray->bit_sz - 2*WORD_SIZE);
  uint_fast8_t y = modulo(bit_index, WORD_SIZE);
  assert (y >= 0);
  assert(WORD_SIZE - y >= 0);
  word lw, rw;

  #ifdef IDEBUG
  printf("y is: %hhi bit_index is: %lu \n a_word is: %lu", y, bit_index, a_word);
  #endif

  if (y >= WORD_SIZE)
    lw = 0;
  else
    lw = a_word << y;

  if (WORD_SIZE - y >= WORD_SIZE)
    rw = 0;
  else
    rw = a_word >> (WORD_SIZE - y);
  
  #ifdef IDEBUG
  printf("lw is: ");
  print_word(lw);
  printf("rw is: ");
  print_word(rw);
  #endif

  word * buff = (word *) bitarray->buf;
  lw |= buff[bit_index/WORD_SIZE] & LEAD(y);
  #ifdef IDEBUG
  printf("LEAD(y) is %lX\n", LEAD(y));
  #endif
  rw |=  buff[bit_index/WORD_SIZE + 1] & TRAIL(WORD_SIZE - y);
  #ifdef IDEBUG
  printf("TRAIL(WORD_SIZE - y) is %lX\n", TRAIL(WORD_SIZE - y));
  #endif

  #ifdef IDEBUG
  printf("lw final is: ");
  print_word(lw);
  printf("rw final is: ");
  print_word(rw);
  #endif
  buff[bit_index/WORD_SIZE] = lw;
  buff[bit_index/WORD_SIZE + 1] = rw;

  return;
}

static void print_bitarray(const bitarray_t* const bitarray, const size_t bit_index) {
  size_t i = bit_index;
  for (; i < bitarray->bit_sz; i++)
    printf("%s", bitarray_get(bitarray, i) ? "1" : "0");
  printf("\n");
}

static void print_word(const word a_word) {
  bitarray_t* word_bitarray = bitarray_new((size_t) 64);
  memcpy(word_bitarray->buf, &a_word, sizeof(a_word));
  print_bitarray(word_bitarray, 0);
  free(word_bitarray);
}

static word reverse_word(word v) {
	static const unsigned char BitReverseTable256[256] = 
{
#   define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
    R6(0), R6(2), R6(1), R6(3)
};
	word c; // c will get v reversed
	unsigned char * p = (unsigned char *) &v;
	unsigned char * q = (unsigned char *) &c;
	q[7] = BitReverseTable256[p[0]]; 
	q[6] = BitReverseTable256[p[1]]; 
	q[5] = BitReverseTable256[p[2]]; 
	q[4] = BitReverseTable256[p[3]];
	q[3] = BitReverseTable256[p[4]]; 
	q[2] = BitReverseTable256[p[5]]; 
	q[1] = BitReverseTable256[p[6]]; 
	q[0] = BitReverseTable256[p[7]];

	return c;
}


void do_isaac_stuff(void) {
  // int bit_index = 2;
  // int bit_sz = 256;
  // struct bitarray* a_bitarray = bitarray_new((size_t) bit_sz);
  // struct bitarray* b_bitarray = bitarray_new((size_t) bit_sz);
  // char values[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF, 0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF};
  // 0x1F, 0x2E, 0x3D, 0x4C, 0x5B, 0x6A, 0x79, 0x88};
  // memcpy(a_bitarray->buf, values, sizeof(values));
  // for (int i = 0; i < sizeof(values); i++) {
  //   printf("%hhX ", ( a_bit_array->buf)[i]);
  // }
  // printf("A bitarray is initially: ");
  // print_bitarray(a_bitarray);
  // //bitarray_copy(a_bitarray, b_bitarray);
  // printf("B bitarray is initially: ");
  // print_bitarray(b_bitarray);

  // word a_word = bitarray_get_word(a_bitarray, bit_index);
  // printf("the returned word is: ");
  // print_word(a_word);
  // //a_word = reverse_word(a_word);
  // //printf("the reversed word is: ");

  // bitarray_set_word(b_bitarray, bit_index, a_word);
  // printf("b bitarray is now:     ");
  // print_bitarray(b_bitarray);

  //assert(!bitarray_cmp(a_bitarray, b_bitarray));
  size_t bit_length = 2048;
  size_t bit_offset = 1024;
  test_rotate(0, bit_length, bit_offset, bit_length-bit_offset, 27);
  // test_reverse(0, bit_length, 0, bit_length);
  // bit_length = 10053;
  // test_reverse(0, bit_length, bit_offset, bit_length-bit_offset);

  //a_word = 0;
  //printf("The word retrieved is: %lX\n", a_word);
}


static bitarray_t* bitarray_newrand(const size_t bit_sz, const unsigned int seed) {
  bitarray_t* test_bitarray = bitarray_new(bit_sz);
  assert(test_bitarray != NULL);

  // Reseed the RNG with whatever we were passed; this ensures that we can
  // repeat the test deterministically by specifying the same seed.
  srand(seed);

  bitarray_randfill(test_bitarray);

  return test_bitarray;
}

static void test_reverse(const unsigned int seed, const size_t bit_sz, const size_t bit_offset, const size_t bit_length) {
  bitarray_t* a_bitarray =  bitarray_newrand(bit_sz, seed);
  bitarray_t* b_bitarray =  bitarray_newrand(bit_sz, seed);
  ssize_t index = 0;

  assert(!bitarray_cmp(a_bitarray, b_bitarray));
  bitarray_reverse_fast(a_bitarray, bit_offset, bit_length);
  bitarray_reverse_slow(b_bitarray, bit_offset, bit_length);
  
  printf("\n seed: %u bit_sz: %lu bit_offset: %lu bit_length: %lu status: ", seed, bit_sz, bit_offset, bit_length);
  index = bitarray_cmp(a_bitarray, b_bitarray);
  if (index) {
    index = 257;
    printf("FAILURE");
    printf("\nCorrect bitarray: ");
    print_bitarray(b_bitarray, index);
    printf("My Bitarray     : ");
    print_bitarray(a_bitarray, index);
    printf("index is: %lu\n", index);
    exit(EXIT_FAILURE);
  }
  else {
    printf("SUCCESS");
  }
  printf("\n");
  bitarray_free(a_bitarray);
  bitarray_free(b_bitarray);
}

static void test_rotate(const unsigned int seed, const size_t bit_sz, const size_t bit_offset, const size_t bit_length, const size_t bit_left_amount) {
  bitarray_t* a_bitarray =  bitarray_newrand(bit_sz, seed);
  bitarray_t* b_bitarray =  bitarray_newrand(bit_sz, seed);
  bitarray_t* c_bitarray =  bitarray_newrand(bit_sz, seed);

  assert(!bitarray_cmp(a_bitarray, b_bitarray));
  bitarray_rotate_fast(a_bitarray, bit_offset, bit_length, bit_left_amount);
  bitarray_rotate_left(b_bitarray, bit_offset, bit_length, bit_left_amount);

  printf("\n seed: %u bit_sz: %lu bit_offset: %lu bit_length: %lu bit_left_amount %lu status: ", seed, bit_sz, bit_offset, bit_length, bit_left_amount);
  if (bitarray_cmp(a_bitarray, b_bitarray)) {
    printf("FAILURE");
    printf("\nCorrect result: ");
    print_bitarray(b_bitarray, 0);
    printf("My result     : ");
    print_bitarray(a_bitarray, 0);
    printf("Initial bitarray:");
    exit(EXIT_FAILURE);
  }
  else {
    printf("SUCCESS");
  }
  printf("\n");

  bitarray_free(a_bitarray);
  bitarray_free(b_bitarray);
  bitarray_free(c_bitarray);
}



//001100110001100110100100001110011011001000011110010011000011010010011001001111011001000111100011001101011100010110100010101100111
//101100110001100110100100001110011011001000011110010011000011000010011001001111011001000111100011001101011100010110100010101100111
//101100110001100110100100001110011011001000011110010011000011000