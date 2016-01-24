/* By Evan Zalys-Geller */

#include <stdlib.h>
#include <stdio.h>
#include <emmintrin.h>
#include <string.h>

#include "core.h"


#define _mm_extract_epi8(x, imm) \
	((((imm) & 0x1) == 0) ? \
	_mm_extract_epi16((x), (imm) >> 1) & 0xff : \
	_mm_extract_epi16(_mm_srli_epi16((x), 8), (imm) >> 1))


void printval(__m128i in){	
	int i;

	printf("|");
#define OUT(i) printf("%i|", _mm_extract_epi8(in, 15-i))
	OUT(0); OUT(1); OUT(2); OUT(3); OUT(4); OUT(5); 
	OUT(6); OUT(7); OUT(8); OUT(9); OUT(10); OUT(11); 
	OUT(12); OUT(13); OUT(14); OUT(15);
#undef OUT
}

void printval_starred(__m128i in){	
	int i;

#define OUT(i) printf("%c", _mm_extract_epi8(in, 15-i) > 0 ? '*' : '-')
	OUT(0); OUT(1); OUT(2); OUT(3); OUT(4); OUT(5); 
	OUT(6); OUT(7); OUT(8); OUT(9); OUT(10); OUT(11); 
	OUT(12); OUT(13); OUT(14); OUT(15);
#undef OUT
}


__m128i one;
__m128i four;
__m128i three;
__m128i count_mask;


void dump_row(__m128i A[]){
	int i;
	
	for(i=0;i<BRICKS_PER_COLUMN;i++){
		printval(A[i]);
		printf("+");
	}
	
	printf("\n");
}

void dump_row_starred(__m128i A[]){
	int i;
	
	for(i=0;i<BRICKS_PER_COLUMN;i++){
		printval_starred(A[i]);
		printf("+");
	}
	
	printf("\n");
}

void dump_ncount(__m128i A[]){
	int i;
	
	for(i=0;i<BRICKS_PER_COLUMN;i++){
		printval(_mm_and_si128(A[i], count_mask));
		printf("+");
	}
	
	printf("\n");
}

// Fortunately the reverse is very easy and cheap. Damnit, intel.
 __m128i load16(unsigned short in){
	
	const u64 a = 0x0101010101010101L;
	const u64 b = 0x8040201008040201L;

	__m128i x, y; 
	
	x = _mm_set_epi64x((a * (in>>8))     & b,
					   (a * (0xff&(in))) & b);
	
	 // If there's a bit pop'd in the field, set the field to 1
	y = _mm_setzero_si128();
	x = _mm_cmpeq_epi8(x, y);
	x = _mm_and_si128(x, one);  // Knock off the high bits from the comparison
	x = _mm_xor_si128(x, one);
	
	return x;
	
}

typedef struct gol_column{
	u64 *left_stripe;
	u16 *column_data;
	u64 *right_stripe;
};

// Pretty slow I'd think. Not liking the branch.
// Maybe a more intelligent way to do this.
inline __m128i make_top_mask(unsigned int top_bits){
	top_bits *= 8;
	top_bits = 128-top_bits;
	
	u64 lo;
	u64 hi;

	if(top_bits < 64){
		hi = (u64)0xFFFFFFFFFFFFFFFFL;
		lo = (u64)0xFFFFFFFFFFFFFFFFL << top_bits;
	}else{
		hi = (u64)0xFFFFFFFFFFFFFFFFL << (top_bits-64);
		lo = 0;
	}
	
	return _mm_set_epi64x(hi, lo);
}

// This CAN be done with a shuffle. Too busy to try it now.
inline __m128i set_1_field(int shift){
	__m128i o = _mm_cvtsi32_si128(1);
	switch(shift){
	case 0: return o;
	case 1: return _mm_slli_si128(o, 1);
	case 2: return _mm_slli_si128(o, 2);
	case 3: return _mm_slli_si128(o, 3);
	case 4: return _mm_slli_si128(o, 4);
	case 5: return _mm_slli_si128(o, 5);
	case 6: return _mm_slli_si128(o, 6);
	case 7: return _mm_slli_si128(o, 7);
	case 8: return _mm_slli_si128(o, 8);
	case 9: return _mm_slli_si128(o, 9);
	case 10: return _mm_slli_si128(o, 10);
	case 11: return _mm_slli_si128(o, 11);
	case 12: return _mm_slli_si128(o, 12);
	case 13: return _mm_slli_si128(o, 13);
	case 14: return _mm_slli_si128(o, 14);
	case 15: return _mm_slli_si128(o, 15);
	}
}

// This does a number of things. It shifts each cell right and adds,
// and then shifts each cell left and then adds again. It also sets the
// fifth bit if the cell is populated.
volatile void left_right_shift_add(__m128i A[], int oaw, bool carry_in_left, bool carry_in_right){
	__m128i temp[BRICKS_PER_COLUMN];
	int i;
	
	//printf("%c <--> %c\n", carry_in_left ? 'x' : '.' , carry_in_right ? 'x' : '.');
	
	for(i=0;i<BRICKS_PER_COLUMN;i++){

		__m128i x;
		
		int cells_remaining = oaw - i*CELLS_PER_BRICK;
		/* --- LEFT SHIFTED COMPONENT --- */
		// protect from shifting in stray data
		__m128i ls = A[i];
		if(cells_remaining <= CELLS_PER_BRICK){
			ls = _mm_and_si128(ls, make_top_mask(cells_remaining));
		}
		ls = _mm_slli_si128(ls, 1);
	
		if(cells_remaining <= CELLS_PER_BRICK){
			if(carry_in_right){
				x = set_1_field(CELLS_PER_BRICK - cells_remaining);
				ls = _mm_or_si128(ls, x);
			}
		}else{
			x = _mm_srli_si128(A[i+1], 15);
			ls = _mm_or_si128(ls, x);
		}
		
		/* --- RIGHT SHIFTED COMPONENT --- */
		__m128i rs = _mm_srli_si128(A[i], 1);		
		if(cells_remaining <= CELLS_PER_BRICK){
			rs = _mm_and_si128(rs, make_top_mask(cells_remaining));
		}
		
		if(i == 0){
			if(carry_in_left){
				x = _mm_slli_si128(one, 15);
				rs = _mm_or_si128(rs, x);
			}
		}else{
			x = _mm_slli_si128(A[i-1], 15);
			rs = _mm_or_si128(rs, x);
		}
		
		
		/* --- PULL TOGETHER --- */
		temp[i] = _mm_add_epi8(rs, ls);
		// Handle the high bit setting
		x = _mm_cmpeq_epi8(A[i], one);
		x = _mm_and_si128(x, _mm_set1_epi8(32));
		
		temp[i] = _mm_add_epi8(temp[i], A[i]);
		temp[i] = _mm_add_epi8(temp[i], x);

	}

	for(i=0;i<BRICKS_PER_COLUMN;i++){
		A[i] = temp[i];
	}
}


/* A is a row of the grid convolved with a 3x3 matrix of 1s. */
void do_gol(__m128i A[], u16 * backing_store, int oaw, int cob, u64 *left_hand_carry, u64 *right_hand_carry){
	int i;
		
	__m128i final[BRICKS_PER_COLUMN];

	for(i=0;i<BRICKS_PER_COLUMN;i++){
		__m128i neighbors = _mm_and_si128(A[i], count_mask);
	
		__m128i live = _mm_cmpgt_epi8(A[i], count_mask);
		__m128i dead;
		
		dead = _mm_andnot_si128(live, neighbors);
		live = _mm_and_si128(live, neighbors);
		
		final[i] = A[i];
		
		dead = _mm_cmpeq_epi8(dead, three);
		
		// Three and four since we count the current cell, 
		// as we're using a convolution kernel
		__m128i tmp  = _mm_cmpeq_epi8(three, live); 
		live = _mm_cmpeq_epi8(four, live);
		live = _mm_or_si128(tmp,live);
		live = _mm_or_si128(live, dead);
		
		// What a great instruction...
		u16 out = (u16)_mm_movemask_epi8(live);
		
		if(i == 0 && out&0x8000){
			*left_hand_carry |= (1UL<<cob);
		}
		
		if(oaw <= CELLS_PER_BRICK){
			if(out & 1UL<<(CELLS_PER_BRICK-oaw))
				*right_hand_carry |= (1UL<<cob);

			backing_store[i] = out & (0xFFFF<<(CELLS_PER_BRICK-oaw));
			break;
		}else{
			backing_store[i] = out;
		}
		
		oaw -= CELLS_PER_BRICK;
	}
	
}

// Second argument is the current row, C is the lookahead.
// This is important so we preserve the live bit.
inline void sum_row(__m128i A[], __m128i B[], __m128i C[]){
	int i;
	
	for(i=0;i<BRICKS_PER_COLUMN;i++){
		A[i] = _mm_add_epi8(A[i],C[i]);
		A[i] = _mm_add_epi8(_mm_and_si128(A[i],count_mask), B[i]);
	}	
}

void dump_cd(u16 cd[], int rows){
	int i,j;
	
	for(i=0;i<BRICKS_PER_COLUMN*rows;i++){
		if(i%BRICKS_PER_COLUMN == 0) printf("\n %p : ", &cd[i]);
		printf("%04x ", cd[i]);
	} printf("\n");
}

// Assumes a local rows variable is defined
#define BRICK_AT(x, y) cd[(x)+BRICKS_PER_COLUMN*(((y)+rows) % rows)]

void prime_neighbor_in(u64 *neighbor_in_left, u64 *neighbor_in_right, int rows, int oaw, u16 *cd){

	one = _mm_set1_epi8(1);
	four = _mm_set1_epi8(4);
	three = _mm_set1_epi8(3);
	count_mask = _mm_set1_epi8(31);

	memset(neighbor_in_left, 0, div_round_up(rows, 64)*sizeof(u64));
	memset(neighbor_in_right, 0, div_round_up(rows, 64)*sizeof(u64));
	
	int row;
	for(row=0;row<rows;row++){
		if(BRICK_AT(0, row)&0x8000){
			neighbor_in_left [(row)>>6] |= 1UL<<(row&63);
		}
		if(BRICK_AT(oaw>>4, row) & 1<<(CELLS_PER_BRICK-(oaw%CELLS_PER_BRICK))){
			neighbor_in_right[(row)>>6] |= 1UL<<(row&63);
		}
	}
}

int run_rows(u16 *cd,
            u64 *neighbor_in_left,
            u64 *neighbor_in_right,
            u64 *neighbor_out_left,
            u64 *neighbor_out_right,
            int oaw,
            int rows){
	int i, row;
	
	// This right here is what we desperately want to keep in cache.
	__m128i A[BRICKS_PER_COLUMN];
	__m128i B[BRICKS_PER_COLUMN];
	__m128i C[BRICKS_PER_COLUMN];
	
	// Does not have to be kept hot
	u16 head[BRICKS_PER_COLUMN];

	memset(neighbor_out_left, 0, div_round_up(rows, 64)*sizeof(u64));
	memset(neighbor_out_right, 0, div_round_up(rows, 64)*sizeof(u64));

	for(i=0;i<BRICKS_PER_COLUMN;i++)
		head[i] = BRICK_AT(i,0);

	for(i=0;i<BRICKS_PER_COLUMN;i++) A[i] = load16(BRICK_AT(i, -1)); // -1st
	for(i=0;i<BRICKS_PER_COLUMN;i++) B[i] = load16(BRICK_AT(i, 0)); // 0th
	for(i=0;i<BRICKS_PER_COLUMN;i++) C[i] = load16(BRICK_AT(i, 1)); // 1st


#define NIB(dir, row) (neighbor_in_##dir [((row+rows)%rows)>>6])&(1UL<<(((row+rows)%rows)&63))

#define LRSAPRE do{ \
;;\
} while(0)

	left_right_shift_add(A, oaw, NIB(left, rows-1), NIB(right, rows-1));

	
	left_right_shift_add(B, oaw, NIB(left, 0), NIB(right, 0));
	left_right_shift_add(C, oaw, NIB(left, 1), NIB(right, 1));
	
	sum_row(A,B,C);

	do_gol(A, &BRICK_AT(0, 0), oaw, 0, &neighbor_out_left[0], &neighbor_out_right[0]);

#define LOAD(X) do { \
	if(row+1 != rows) \
		for(i=0;i<BRICKS_PER_COLUMN;i++) X[i] = load16(BRICK_AT(i, row+1)); \
	else \
		for(i=0;i<BRICKS_PER_COLUMN;i++) X[i] = load16(head[i]); \
	} while(0)
	
#define LRSA(X) left_right_shift_add(X, oaw, NIB(left, row+1), NIB(right, row+1));
#define GOL(X) do_gol(X, &BRICK_AT(0, row), oaw, (row&63), &neighbor_out_left[row>>6], &neighbor_out_right[row>>6]);


	for(row = 1;;){
		LOAD(A);
		LRSAPRE;
		LRSA(A);
		sum_row(B, C, A);
		GOL(B);
	
		if(++row == rows) break;

		LOAD(B);
		LRSAPRE;
		LRSA(B);
		sum_row(C, A, B);
		GOL(C);
	
		if(++row == rows) break;

		LOAD(C);
		LRSAPRE;
		LRSA(C);
		sum_row(A, B, C);
		GOL(A);
	
		if(++row == rows) break;
	}

#undef LOAD
	
	return 0;

}
