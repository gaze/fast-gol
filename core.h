/* By Evan Zalys-Geller */

#ifndef CORE_H
#define CORE_H


typedef unsigned short u16;
typedef unsigned long long int u64;


void prime_neighbor_in(u64 *neighbor_in_left, u64 *neighbor_in_right, int rows, int oaw, u16 *cd);

int run_rows(u16 *cd,
            u64 *neighbor_in_left,
            u64 *neighbor_in_right,
            u64 *neighbor_out_left,
            u64 *neighbor_out_right,
            int oaw,
            int rows);

//#define BRICKS_PER_COLUMN 32
#define CELLS_PER_BRICK 16

#ifndef BRICKS_PER_COLUMN
#define BRICKS_PER_COLUMN 3
#endif


void dump_cd(u16 cd[], int rows);

#define MIN(a,b) (((a)<(b))?(a):(b))
#define MAX(a,b) (((a)>(b))?(a):(b))


static inline int div_round_up(int x, int y)
{
   int a = (x -1)/y +1;

   return a;
}


#endif
