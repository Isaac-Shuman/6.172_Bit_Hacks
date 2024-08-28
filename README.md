// reverses the bits in a range in a given bitarray. We do this by first expanding our
// range to the nearest block boundaries, reversing the entire range, and then shifting
// over however much we need (the amount offset from the end boundary - the offset from the start). 
// Note a positive shift is to the left. For a simple example pretend we have bytes numbered
// [1,2,3,4,5,6,7,8] and we want to reverse from 3 to 7. Then we first get [8,7,6,5,4,3,2,1]
// We shift over by 1-2 = -1 to get [1,8,7,6,5,4,3,2] and then restore the start and end bits to get
// [1,2,7,6,5,4,3,8]. Note that we chose a range falling on byte boundaries for simplicity, so
// we wouldn't have to make the example [1...64]
// The point in reversing blocks is word level parallelism, we get about an 64x speedup over reversing 
// bit by bit. 



How would we reverse a completely aligned segment, byte offset can only be  a multiple of  64 and so can sub_array length


unsigned 64 *leftmost_word = array_base + offset
unsigned64 *rightmost_word = leftmost_word + sub_array_lenth -64

while leftmost < rightmost
	swap(reverse(leftmost), reverse(rightmost))

if leftmost == rightmost {
	bit_array->setword(leftmost, reverse(leftmost))
}


typedef ??? word;

void reverse (?? bytestring) {
	//initalize lwa and rwa
	
	//swap words
	while (lwa + 1 < rwa)         //for (int lwa = 0, lwa < 11/4/2, lwa += WORD, rwa -= WORD)
	{
		swap_reverse(lwa, rwa)
		lwa++
		rwa--
	}
	
	//base case???
}

void swap_reverse (word * lwa, word * rwa) {
	word lw = load_word(lwa);
	word rw = load_word(rwa);
	set_word(lwa, reverse_word(rw));
	set_word(rwa, reverse_word(lw));
}

word load_word(word * lwa, excess) {
	//excess is either left excess (x in diagram) or WORD - right excess (WORD- y in diagram)
	//lw = buffer[wa/WORD]
	//rw = buffer[wa/WORD + 1]
	
	word result;
	
	lw = buffer[wa]
	rw=buffer[wa+1]
	
	lw = (lw & TRAIL(excess)) << (excess)
	rw = (rw & LEAD(WORD - excess)) >> (WORD - excess)
	
	result = lw | rw
} 

void set64(wa, excess, val) {
	y = WORD-excess
	buffer[wa] = (buffer[wa] & LEAD(y)) | (val >> y)  //logical shift
	buffer[wa + 1] = (buffer[wa + 1] & TRAIL(WORD - y)) | (val << (WORD - y))
}

word reverse_word() {
	//talbe lookup
}

//generate the table

static const unsigned char BitReverseTable256[256] = 
{
#   define R2(n)     n,     n + 2*64,     n + 1*64,     n + 3*64
#   define R4(n) R2(n), R2(n + 2*16), R2(n + 1*16), R2(n + 3*16)
#   define R6(n) R4(n), R4(n + 2*4 ), R4(n + 1*4 ), R4(n + 3*4 )
    R6(0), R6(2), R6(1), R6(3)
};
unsigned int v; // reverse 32-bit value, 8 bits at time
unsigned int c; // c will get v reversed
// Option 1:
c = (BitReverseTable256[v & 0xff] << 24) | 
    (BitReverseTable256[(v >> 8) & 0xff] << 16) | 
    (BitReverseTable256[(v >> 16) & 0xff] << 8) |
    (BitReverseTable256[(v >> 24) & 0xff]);
// Option 2:
unsigned char * p = (unsigned char *) &v;
unsigned char * q = (unsigned char *) &c;
q[3] = BitReverseTable256[p[0]]; 
q[2] = BitReverseTable256[p[1]]; 
q[1] = BitReverseTable256[p[2]]; 
q[0] = BitReverseTable256[p[3]];
