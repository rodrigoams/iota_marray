#ifndef IOTA_MARRAY_H
#define IOTA_MARRAY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include <lua.h>
#include <lauxlib.h>

/*
 *		STRUCT AND TYPEDEF
 */

typedef enum iota_type {
	IMARRAY = 1,
	IVIEW
//	IINDEXES
} itype;

typedef struct iota_view {
	// view
	int rank;
	int *dimensions;
	// translate to marray indexes
	int *vector;
	int *matrix;
	int rows;
	int cols;
	int *indexes;
} iview;

typedef struct iota_marray {
	iview *vw;
	int size;
	int rank;
	int *stride;
	int *dimensions;
	double *data;
} imarray;

typedef struct iota_indexes {
	bool isview;
	int rank;
	int deep;
	int indexes[1];
} iindexes;

extern int * stride_from_dimension(const int rank, const int * dimensions);
extern imarray * iota_createmarray(lua_State *L, const int size, const int rank, const int * dimensions);
extern void iota_checkudata(lua_State *L, int arg);
extern void iota_meta_getmt(lua_State *L, int n);
extern int iota_getindex(const imarray * ima, const int * idx);


#endif /* IOTA_MARRAY_H */
