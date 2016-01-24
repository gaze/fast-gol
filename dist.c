/* By: Evan Zalys-Geller */

#include "core.h"
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <cilkview.h>

u64 ** neighbor_1_left;
u64 ** neighbor_2_left;
u64 ** neighbor_1_right;
u64 ** neighbor_2_right;

u16 ** data;

int units;

int livecnt[10];
int livecnt_gen = 0;

#ifdef PROFILE
cilk::cilkview cv;
#endif

int init(int rows, int columns){

	int i;
	units = div_round_up(columns, CELLS_PER_BRICK*BRICKS_PER_COLUMN);
	int neighbor_list_size = div_round_up(rows, 64);
	
	neighbor_1_left   = new u64* [units];
	neighbor_2_left  = new u64* [units];
	neighbor_1_right  = new u64* [units];
	neighbor_2_right = new u64* [units];
	
	data = new u16 * [units];
	
	for(i=0;i<units;i++){
		data[i] = new u16 [rows*BRICKS_PER_COLUMN];
		
		memset(data[i], 0, rows*BRICKS_PER_COLUMN*sizeof(u16));
		
		neighbor_1_left[i] = new u64 [neighbor_list_size];
		neighbor_2_left[i] = new u64 [neighbor_list_size];
		neighbor_1_right[i] = new u64 [neighbor_list_size];
		neighbor_2_right[i] = new u64 [neighbor_list_size];
	}
}

int dump_nla(int rows, u64 * x){
	int i;
	int neighbor_list_size = div_round_up(rows, 64);
	
	for(i=0;i<neighbor_list_size;i++){
		printf("%lx ", x[i]);
	}
	printf("\n");
}

inline void set_cell(int row, int column, int value){
	int unit = column/(CELLS_PER_BRICK*BRICKS_PER_COLUMN);
	int unit_column = column % (CELLS_PER_BRICK*BRICKS_PER_COLUMN);
	int brick_column = unit_column/CELLS_PER_BRICK;
	int bit = (CELLS_PER_BRICK-1)-unit_column%CELLS_PER_BRICK;
	int brick_index = brick_column+BRICKS_PER_COLUMN*row;

	if(value)
		data[unit][brick_index] |= (1<<(bit));
	else
		data[unit][brick_index] &= ~(1<<(bit));
}

inline int get_cell(int row, int column){
	int unit = column/(CELLS_PER_BRICK*BRICKS_PER_COLUMN);
	int unit_column = column % (CELLS_PER_BRICK*BRICKS_PER_COLUMN);
	int brick_column = unit_column/CELLS_PER_BRICK;
	int bit = (CELLS_PER_BRICK-1)-unit_column%CELLS_PER_BRICK;
	int brick_index = brick_column+BRICKS_PER_COLUMN*row;
		
	return data[unit][brick_index] & (1<<(bit));
}

inline int dump_grid(int rows, int columns){
	int row, column;
	
	for(row=0;row<rows;row++){
		for(column=0;column<columns;column++){
			if(column % (CELLS_PER_BRICK*BRICKS_PER_COLUMN) == 0 && column != 0)
				printf("#");
			if(column % (CELLS_PER_BRICK) == 0)
				printf("^");
			printf("%c", get_cell(row,column) ? 'X' : '.');
	
		}
		printf("\n");
	}
	printf("\n");
}

void read_file(int lines, int columns){
	int line=0;
	int column=0;
	
	char c;
	
	for(;;){
		c = fgetc(stdin);

		if(c == '0' || c == '1'){
			if(c=='0'){
				set_cell(line, column, 0);
			}else{
				set_cell(line, column, 1);
			}
			column++;
		}

		if(c == '\n'){
			if(column != columns){
				printf("Incorrect width specified, or bad formatting\n");
				printf("Found %i columns\n", column);
				exit(1);
			}
			column = 0;
			line++;
			if(line == lines){
				return;
			}
		}
	}
}

int gen_life(int rows, int columns){
	int i, j;
	
	for(i=0;i<units;i++){
		for(j=0; j<rows*BRICKS_PER_COLUMN; j++){
			data[i][j] = rand();
		}
	}


}

int last_unit_width(int columns){
	if(columns % (CELLS_PER_BRICK*BRICKS_PER_COLUMN) == 0)
		return (CELLS_PER_BRICK*BRICKS_PER_COLUMN);
	else
		return columns%(CELLS_PER_BRICK*BRICKS_PER_COLUMN);
}

static inline int popcnt(long word)
{
  long dummy, dummy2;

asm("          popcnt  %1, %0     " "\n\t"
:   "=&r"(dummy), "=&r" (dummy2)
:   "1"((long) (word))
:   "cc");
  return (dummy);
}

int popcnt16(u16 data){
	unsigned int x = data;
	return popcnt(data);
}

void run_live_count(int rows){
	int sum=0, i;
	int unit;
	
	for(unit=0; unit < units; unit++){		
		for(i=0;i<rows*BRICKS_PER_COLUMN;i++){
			sum += popcnt16(data[unit][i]);
		}
	}
	
	livecnt[livecnt_gen++] = sum;
}

int run_life(int rows, int columns, int iter){
	int cell, i;
	bool do_livecnt = false;
	
	if(iter % 10 == 0){
		do_livecnt = true;
	}
	
	for(int unit=0; unit < units; unit++){
		int oaw = unit == units-1 ? last_unit_width(columns) : (CELLS_PER_BRICK*BRICKS_PER_COLUMN);
		
		prime_neighbor_in(neighbor_2_left[unit], neighbor_2_right[unit], rows, oaw, data[unit]);
	}
	
#ifdef PROFILE
	cv.start();
#endif

	for(i=0;;){
		cilk_for(int unit=0; unit < units; unit++){
			int oaw = unit == units-1 ? last_unit_width(columns) : (CELLS_PER_BRICK*BRICKS_PER_COLUMN);
			
			// And now the great shuffle.
			int unit_left = ((unit-1 + units)%units);
			int unit_right = ((unit+1 + units)%units);

			run_rows(data[unit], neighbor_2_right[unit_left], neighbor_2_left[unit_right],
								 neighbor_1_right[unit_left], neighbor_1_left[unit_right], oaw, rows);			 
		}
		
#ifdef DEBUG		
		if(do_livecnt){
			if(((i+1) % (iter/10)) == 0){
//				dump_grid(rows, columns);
				run_live_count(rows);
			}
		}
#endif
		
		if(++i == iter) break;

		cilk_for(int unit=0; unit < units; unit++){
			int oaw = unit == units-1 ? last_unit_width(columns) : (CELLS_PER_BRICK*BRICKS_PER_COLUMN);
		
			run_rows(data[unit], neighbor_1_left[unit], neighbor_1_right[unit],
								 neighbor_2_left[unit], neighbor_2_right[unit], oaw, rows);					 
		}
		
#ifdef DEBUG
		if(do_livecnt){
			if(((i+1) % (iter/10)) == 0){
//				dump_grid(rows, columns);
				run_live_count(rows);
			}
		}
#endif
		
		if(++i == iter) break;

	}
	

#ifdef PROFILE
	cv.stop();
	printf("time elapsed: %i ms\n", cv.accumulated_milliseconds());
#endif

#ifdef DEBUG
	if(do_livecnt){
		for(int i = 0; i < 10; i++)
			printf("%d ",livecnt[i]);
		printf("\n");
	}
#endif

}

int cilk_main(int argc, char** argv){
	// Initialize life matrix through either reading from standard input or initializing as required. 
	int n;
	int iter;
	
	if(argc < 3)
	{
		printf("Usage : ./life [r] <matrix size> <number of iterations>\n");	
		exit(-1);
	}
	
	// Read from file
	if(argv[1][0] == 'r')	
	{
		n = (unsigned int)atoi(argv[2]);
		iter = (unsigned int)atoi(argv[3]);
		
		init(n,n);
		
		read_file(n,n);
	}else{
		n = (unsigned int)atoi(argv[1]);
		iter = (unsigned int)atoi(argv[2]);
		
		init(n,n);
		gen_life(n,n);

	}
	
	run_life(n,n, iter);

	exit(0);

}


