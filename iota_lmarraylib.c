#include <stdio.h>
#include <stdlib.h>
//#include <string.h>
#include <stdbool.h>

#include <cblas.h>

#include "lua.h"
#include "lauxlib.h"

/*
 *		STRUCT AND TYPEDEF
 */

typedef struct iota_marray {
	int size;
	int rank;
	int *dimensions;
	double *data;
} imarray;

typedef struct iota_indexes {
	int rank;
	int deep;
	int indexes[1];
} iindexes;

typedef struct iota_view {
	// this view shape
	int rank;
	int *dimensions;
	// translate to marray indexes
	int *vector;
	int *matrix;
	int rows;
	int cols;
	// backup of marray shape
	int *ma_indexes;
	int ma_rank;
	int *ma_dimensions;
} iview;

typedef enum iota_type {
	IMARRAY = 1,
	IVIEW,
	IINDEXES
} itype;

/*
 *		AUXILIARY
 */

static inline itype iotaA_type(lua_State *L, const int n)
{
	if (luaL_testudata(L, n, "iota_marray") != NULL) return IMARRAY;
	else if (luaL_testudata(L, n, "iota_view") != NULL) return IVIEW;
	else if(luaL_testudata(L, n, "iota_indexes") != NULL) return IINDEXES;
	else return false;
}

static inline bool iotaA_strcmp(const char * str1, const char * str2)
{
	while(*str1==*str2) {
		if ( *str1 == '\0' || *str2 == '\0' ) break;
		str1++; str2++;
	}
	if( *str1 == '\0' && *str2 == '\0' )
		return true;
	else
		return false;
}

static imarray * iotaA_newmarray(lua_State *L, int rank, int * dimensions)
{
	int size = 1;
	for(int i=0; i<rank; i++) size *= dimensions[i];
	
	imarray * ima = (imarray *)lua_newuserdata(L, sizeof(imarray));
	int pos = lua_gettop(L);
	lua_newtable(L);
	lua_setuservalue(L, -2);
	
	luaL_getmetatable(L, "iota_marray");
	lua_setmetatable(L, -2);
	
	ima->dimensions = calloc(rank, sizeof(int));
	for(int i=0; i<rank; i++)	ima->dimensions[i] = dimensions[i];
	
	ima->rank = rank;
	ima->size = size;
	ima->data = calloc(size, sizeof(double));
	
	return ima;
}

//
//		MARRAY METAINFORMATION GET AND SET
//
/*
static void iota_meta_getkey(lua_State *L, int n, const char * key)
{
	lua_getuservalue(L, n);
	int posmeta = lua_gettop(L);
	
	//if (! lua_istable(L, -1)) printf("Não é uma tabela! \n");
	
	lua_getfield(L, posmeta, key);
	lua_remove(L, posmeta);
}

static void iota_meta_setstring(lua_State *L, int n, const char * key, const char * string)
{
	lua_getuservalue(L, n);
	int posmeta = lua_gettop(L);
	
	//if (! lua_istable(L, -1)) printf("Não é uma tabela! \n");
	
	lua_pushstring(L, string);
	lua_setfield(L, posmeta, key);
	lua_remove(L, posmeta);
}
*/

/*
 *		MARRAY
 */

static int iota_marray_new(lua_State *L)
{
	int rank = lua_gettop(L);
	
	imarray * ima = (imarray *)lua_newuserdata(L, sizeof(imarray));
	lua_newtable(L);
	lua_setuservalue(L, -2);
	
	ima->dimensions = calloc(rank, sizeof(int));
		
	int size = 1;

	for(int i=0; i < rank; i++) {
		int dim = luaL_checkinteger(L, 1+i);
		size *= dim;
		ima->dimensions[i] = dim;
	}

	ima->rank = rank;
	ima->size = size;
	ima->data = calloc(ima->size, sizeof(double));

	luaL_getmetatable(L, "iota_marray");
	lua_setmetatable(L, -2);

	return 1;
}

static int iota_marray_reshape(lua_State *L)
{
	int newrank = lua_gettop(L) - 1;
	int *newdim = calloc(newrank, sizeof(int));

	imarray *ima = luaL_checkudata(L, 1, "iota_marray");

	int newsize = 1;
	
	for(int i=0; i<newrank; i++){
		newdim[i] = luaL_checkinteger(L, 2+i);
		newsize *= newdim[i];
	}

	if (newsize != ima->size){
		free(newdim);
		luaL_error(L, "New size (%d) is different of the old (%d) one. You can reshape only same sized objects\n!", newsize, ima->size);
	}

	free(ima->dimensions);
	
	ima->dimensions = newdim;
	ima->rank = newrank;
	
	return 0;
}

static int iota_marray__gc(lua_State *L)
{
	imarray * ima = (imarray *)lua_touserdata(L, 1);
	free(ima->dimensions);
	free(ima->data);
	return 1;
}

static int iota_marray__newindex(lua_State *L)
{
	imarray * ima = (imarray *)lua_touserdata(L, 1);
	
	if ( (lua_isnumber(L, 2)) && (ima->rank == 1) ) {
		
		int pos = luaL_checkinteger(L, 2);
		ima->data[pos-1] = luaL_checknumber(L, 3);
	}
	else if ( lua_isstring(L, 2) && (
			iotaA_strcmp(lua_tostring(L, 2), "rank") ||
			iotaA_strcmp(lua_tostring(L, 2), "size") ||
			iotaA_strcmp(lua_tostring(L, 2), "dim")
		                               ) ) {
																		 
		printf("You can't change this field!\n");
		// do nothing!
	}	
	else {
		lua_getuservalue(L, 1);
		lua_insert(L, 2);
		lua_rawset(L, 2);
	}
	return 0;
}

static int iota_marray__index(lua_State *L)
{
	imarray * ima = (imarray *)lua_touserdata(L, 1);
	
	switch (lua_type(L, 2)) {
		case LUA_TNUMBER: {
			if (ima->rank == 1)
				lua_pushnumber(L, ima->data[luaL_checkinteger(L, 2) - 1]);
			else {
				size_t nbytes = sizeof(iindexes) + (ima->rank - 1)*sizeof(int);
				iindexes *idx = (iindexes *)lua_newuserdata(L, nbytes);
		
				lua_pushvalue(L, 1);
				lua_setuservalue(L, -2);
		
				idx->rank = ima->rank;
				idx->deep = 1;
				idx->indexes[0] = luaL_checkinteger(L, 2) - 1;
		
				luaL_getmetatable(L, "iota_indexes");
				lua_setmetatable(L, -2);
			}
			break;
		}
		case LUA_TSTRING: {
			const char * string = luaL_checkstring(L, 2);
			
			if (iotaA_strcmp(string, "rank"))	lua_pushinteger(L, ima->rank);
			else if (iotaA_strcmp(string, "size"))	lua_pushinteger(L, ima->size);
			else if (iotaA_strcmp(string,  "dim")) {
				lua_createtable(L, ima->rank, 0);
				for (int i = 0; i < ima->rank; i++) {
					lua_pushinteger(L, i+1);
					lua_pushinteger(L, ima->dimensions[i]);
					lua_rawset(L, -3);
				}
			}
			else {
				lua_getuservalue(L, 1);
				lua_insert(L, 2);
				lua_rawget(L, -2);
			}
			break;
		}
		default: {
			lua_getuservalue(L, 1);
			lua_insert(L, 2);
			lua_rawget(L, -2);
		}
	}
	
	return 1;
}

static inline int iota_indexes2index(int rank, const int *dim, const  int *idx)
{
	if (rank == 2) return idx[0] + dim[0]*idx[1];
	else if (rank == 3) return idx[0] + dim[0]*(idx[1] + dim[1]*idx[2]);
	else if (rank == 4) return idx[0] + dim[0]*(idx[1] + dim[1]*(idx[2] + dim[2]*idx[3]));
	else {
		printf("index not implemented for rank %d \n", rank);
		return -1;
	}
}

/*
 *		VIEWS
 */

static int iota_view__gc(lua_State *L)
{
	iview * ivw = (iview *)luaL_checkudata(L, 1, "iota_view");
	free(ivw->dimensions);
	free(ivw->vector);
	free(ivw->matrix);
	free(ivw->ma_indexes);
	free(ivw->ma_dimensions);
	return 1;
}

static inline void iota_view_matrixset(iview *ivw, const int i, const int j, const int v)
{
	ivw->matrix[i + ivw->rows*j] = v;
}

static inline int iota_view_matrixget(const iview *ivw, const int i, const int j)
{
	return ivw->matrix[i + ivw->rows*j];
}

static void iota__printview(const iview *ivw)
{
	printf("'matrix': %d X %d\n", ivw->rows, ivw->cols);
	for(int i=0; i<ivw->rows; i++) {
		for(int j=0; j<ivw->cols; j++)
			printf("%d ", iota_view_matrixget(ivw,i,j) );
		printf("\n");
	}
	printf("'vector': %d\n", ivw->rows);
	for (int i=0; i<ivw->rows; i++) printf("%d\n", ivw->vector[i]);
}

static inline int iota_view_getindex(const iview * ivw, const int * viewidx)
{
	if (ivw->ma_rank == 1)
		return viewidx[0];
	else {
		for(int i=0; i<ivw->rows; i++){
			ivw->ma_indexes[i] = ivw->vector[i];
			for(int j=0; j<ivw->cols; j++)
				ivw->ma_indexes[i] += iota_view_matrixget(ivw, i, j)*viewidx[j];
		}
		return iota_indexes2index(ivw->ma_rank, ivw->ma_dimensions, ivw->ma_indexes);
	}
}

static void iota_view_frommarray(lua_State *L, int n)
{
	imarray *ima = luaL_checkudata(L, n, "iota_marray");

	iview * ivw = (iview *)lua_newuserdata(L, sizeof(iview));
	luaL_getmetatable(L, "iota_view");
	lua_setmetatable(L, -2);
	
	lua_pushvalue(L, n);
	lua_setuservalue(L, -2);
	
	ivw->rank = ima->rank;
	ivw->rows = ima->rank;
	ivw->cols = ima->rank;
	
	ivw->dimensions = (int*)calloc(ivw->rank, sizeof(int));
	for(int i=0; i<ivw->rank; i++) ivw->dimensions[i] = ima->dimensions[i];
	
	ivw->matrix = (int*)calloc(ivw->rank*ivw->rank, sizeof(int));
	for(int i=0; i<ivw->rank; i++) iota_view_matrixset(ivw, i, i, 1);
	
	ivw->vector = (int*)calloc(ivw->rank, sizeof(int));
	ivw->ma_indexes = (int*)calloc(ima->rank, sizeof(int));
	
	ivw->ma_rank = ima->rank;
	
	ivw->ma_dimensions = (int*)calloc(ima->rank, sizeof(int));
	for(int i=0; i<ima->rank; i++) ivw->ma_dimensions[i] = ima->dimensions[i];
	
	lua_replace(L, n);
}

static int iota_view__index(lua_State *L)
{
	const iview * ivw = (iview * )luaL_checkudata(L, 1, "iota_view");
	
	switch (lua_type(L, 2)) {
		case LUA_TNUMBER: {
			if (ivw->rank == 1) {
				int idx[1] = {luaL_checkinteger(L, 2) - 1};	
			
				lua_getuservalue(L, 1);
				imarray * ima = (imarray *)luaL_checkudata(L, -1, "iota_marray");

				lua_pushnumber(L, ima->data[iota_view_getindex(ivw, idx)]);
			}
			else {
				size_t nbytes = sizeof(iindexes) + (ivw->rank - 1)*sizeof(int);
				iindexes *idx = (iindexes *)lua_newuserdata(L, nbytes);
		
				lua_pushvalue(L, 1);
				lua_setuservalue(L, -2);
		
				idx->rank = ivw->rank;
				idx->deep = 1;
				idx->indexes[0] = luaL_checkinteger(L, 2) - 1;
		
				luaL_getmetatable(L, "iota_indexes");
				lua_setmetatable(L, -2);
			}
		break;
		}	
		case LUA_TSTRING: {
			const char * string = luaL_checkstring(L, 2);

			if (iotaA_strcmp(string, "rank")) lua_pushinteger(L, ivw->rank);
			else if (iotaA_strcmp(string,  "dim")) {
				lua_createtable(L, ivw->rank, 0);
				for (int i = 0; i < ivw->rank; i++) {
					lua_pushinteger(L, i+1);
					lua_pushinteger(L, ivw->dimensions[i]);
					lua_rawset(L, -3);
				}
			}
			else if (iotaA_strcmp(string, "ma_rank")) lua_pushinteger(L, ivw->ma_rank);
			else if (iotaA_strcmp(string,  "ma_dimensions")) {
				lua_createtable(L, ivw->ma_rank, 0);
				for (int i = 0; i < ivw->ma_rank; i++) {
					lua_pushinteger(L, i+1);
					lua_pushinteger(L, ivw->ma_dimensions[i]);
					lua_rawset(L, -3);
				}
			}
			else if (iotaA_strcmp(string,  "ma_indexes")) {
				lua_createtable(L, ivw->ma_rank, 0);
				for (int i = 0; i < ivw->ma_rank; i++) {
					lua_pushinteger(L, i+1);
					lua_pushinteger(L, ivw->ma_dimensions[i]);
					lua_rawset(L, -3);
				}
			}
			else {
				lua_getuservalue(L, 1);
				lua_insert(L, 2);
				lua_gettable(L, -2);
			}
		break; }
		default: {
			lua_getuservalue(L, 1);
			lua_insert(L, 2);
			lua_gettable(L, -2);
		}
	}
	
	return 1;
}

static int iota_view__newindex(lua_State *L)
{
	iview * ivw = (iview *)lua_touserdata(L, 1);
	
	if ( (lua_isnumber(L, 2)) && (ivw->rank == 1) ) {
		int idx[1] = {luaL_checkinteger(L, 2) - 1};
		lua_getuservalue(L, 1);
		imarray * ima = (imarray *)lua_touserdata(L,-1);
		ima->data[iota_view_getindex(ivw, idx)] = luaL_checknumber(L, 3);
	}
	else if ( lua_isstring(L, 2) && (
             iotaA_strcmp(lua_tostring(L, 2), "rank") ||
             iotaA_strcmp(lua_tostring(L, 2), "dim")
             ) ) {
										   
		printf("Voçê não pode alterar esses campos! \n");
		// do nothing!
	}
	else {	
		lua_getuservalue(L, 1);
		lua_insert(L, 1);
		lua_settable(L, 1);
	}
	return 0;
}

static int iota_view_newslice(lua_State *L, int n, const int dim, const int start, const int end, const int step)
{
	iview * ivwo = (iview *)luaL_checkudata(L, n, "iota_view");
	
	iview * ivw = (iview *)lua_newuserdata(L, sizeof(iview));
	
	lua_getuservalue(L, n);
	lua_setuservalue(L, -2);
	
	luaL_getmetatable(L, "iota_view");
	lua_setmetatable(L, -2);
	
	ivw->ma_indexes = (int*)calloc(ivwo->ma_rank, sizeof(int));
	ivw->ma_rank = ivwo->ma_rank;
	ivw->ma_dimensions = (int*)calloc(ivwo->ma_rank, sizeof(int));
	for(int i=0; i<ivwo->ma_rank; i++) ivw->ma_dimensions[i] = ivwo->ma_dimensions[i];
	
	ivw->rank = (start == end) ? ivwo->rank - 1 : ivwo->rank;
	ivw->dimensions = (int*)calloc(ivw->rank, sizeof(int));

	// buffer
	int vec[ivwo->rank];
	int mat[ivwo->rank][ivw->rank];
	for (int i=0; i<ivwo->rank; i++){
		vec[i] = 0;
		for(int j=0; j<ivw->rank; j++) mat[i][j] = 0;
	}
	// transformation vector
	vec[dim] = start;
	// transformation matrix
	for (int i=0, count=0; i<ivwo->rank; i++)
		if (count < ivw->rank) {
			if ((start == end) && (i != dim)) {
				ivw->dimensions[count] = ivwo->dimensions[i];
				mat[i][count] = 1;
				count++;
			} else if ((start != end) && (i == dim )){
				ivw->dimensions[count] = (int)((end - start)/step);
				mat[i][count] = step;
				count++;
			} else if ((start != end) && (i != dim)) {
				ivw->dimensions[count] = ivwo->dimensions[i];
				mat[i][count] = 1;
				count++;
			}
		}
	// allocating memory
	ivw->rows = ivw->ma_rank;
	ivw->cols = ivw->rank;
	ivw->matrix = (int*)calloc(ivw->rows * ivw->cols ,sizeof(int));
	ivw->vector = (int*)calloc(ivw->rows, sizeof(int));
	// converting to original shape
	for (int i=0; i<ivwo->rows; i++) {
		
		for (int j=0; j<ivwo->rank; j++)
			ivw->vector[i] += iota_view_matrixget(ivwo, i, j) * vec[j];
		ivw->vector[i] += ivwo->vector[i];
		
		for(int j=0; j<ivw->cols; j++) {
			int b = 0;
			for(int k=0; k<ivwo->cols; k++)
				b += iota_view_matrixget(ivwo, i, k)*mat[k][j];
			
			iota_view_matrixset(ivw, i, j, b);
		}
	}

	//iota__printview(ivwo);
	//iota__printview(ivw);
	
	return 1;
}

static void iota_view_fromindexes(lua_State *L, int n)
{
	iindexes *idx = (iindexes *)luaL_checkudata(L, n, "iota_indexes");
	
	lua_getuservalue(L, n);
	int pos = lua_gettop(L);
	
	if (iotaA_type(L, pos) == IMARRAY) {
		iota_view_frommarray(L, pos);
		//lua_remove(L, pos); // remove uservalue
	}
	
	for (int i=0; i<idx->deep; i++) {
		iota_view_newslice(L, pos, 0, idx->indexes[i], idx->indexes[i], 1);
		lua_remove(L, pos); //remove old view from stack
	}
	
	lua_replace(L, n);
}

static int iota_view_slice(lua_State *L)
{
	int nargs = lua_gettop(L);
	if ((nargs < 3) || (nargs > 5)) luaL_error(L, "Invalid number of arguments.\n");
	
	int dim = luaL_checkinteger(L, 2) - 1;
	int start = luaL_checkinteger(L, 3) - 1;
	int end = (nargs >= 4) ? luaL_checkinteger(L, 4) - 1 : start;
	int step = (nargs == 5) ? luaL_checkinteger(L, 5) : 1;
	
	switch (iotaA_type(L, 1)) {
		case IMARRAY: {
			iota_view_frommarray(L, 1);
			iota_view_newslice(L, 1, dim, start, end, step);
		break ; }
		case IINDEXES: iota_view_fromindexes(L, 1);
		case IVIEW : iota_view_newslice(L, 1, dim, start, end, step); break;
	}
	
	return 1;
}

static int iota_marray_toview(lua_State *L)
{
	switch(iotaA_type(L, 1))
	{
		case IMARRAY: iota_view_frommarray(L, 1); break;
		case IINDEXES: iota_view_fromindexes(L, 1);	break;
		case IVIEW: printf("Just a view!\n");
	}
	
	return 1;
}

/*
 *		INDEXES
 */

static int iota_indexes__index(lua_State *L)
{
	iindexes * idx = (iindexes *)lua_touserdata(L, 1);
	
	idx->indexes[idx->deep] = luaL_checkinteger(L, 2) - 1;
	idx->deep = idx->deep + 1;
	
	if (idx->deep == idx->rank) {
		
		lua_getuservalue(L, 1);
		
		switch (iotaA_type(L, -1))
		{
			case IMARRAY: {
				imarray *ima = (imarray *)lua_touserdata(L, -1);
				int index = iota_indexes2index(idx->rank, ima->dimensions, idx->indexes);
			
				lua_pushnumber(L, ima->data[index]);
				break;
			}
			case IVIEW: {
				iview * ivw = (iview *)luaL_checkudata(L, -1, "iota_view");
				int index = iota_view_getindex(ivw, idx->indexes);

				lua_getuservalue(L, -1);
				imarray *ima = (imarray *)luaL_checkudata(L, -1, "iota_marray");

				lua_pushnumber(L, ima->data[index]);
				break;
			}
		}
	}
	else
	{
		lua_pushvalue(L,1);
	}
	return 1;
}

static int iota_indexes__newindex(lua_State *L)
{
	iindexes * idx = (iindexes *)lua_touserdata(L, 1);
	
	double value = luaL_checknumber(L, 3);
	
	idx->indexes[idx->deep] = luaL_checkinteger(L, 2) - 1;
	idx->deep = idx->deep + 1;
	
	if (idx->deep == idx->rank) {
		
		lua_getuservalue(L, 1);
		
		switch (iotaA_type(L, -1)) {
			case IMARRAY:
			{
				imarray *ima = (imarray *)lua_touserdata(L, -1);
				int index =  iota_indexes2index(idx->rank, ima->dimensions, idx->indexes);

				ima->data[index] = value;
			}
			break;
			case IVIEW:
			{
				iview * ivw = (iview *)lua_touserdata(L, -1);
				int index = iota_view_getindex(ivw, idx->indexes);
			
				lua_getuservalue(L, -1);
				imarray *ima = (imarray *)lua_touserdata(L, -1);
			
				ima->data[index] = value;
			}
			break;
		}
	} else {
		
		printf("PROBLEMA COM __NEWINDEX\n");
	}
	
	return 0;
}

/*
 *		ARITHMETIC METAMETHODS
 */

static imarray * iota_view_getmarray(lua_State *L, int pos)
{
	lua_getuservalue(L, pos);
	imarray * ima = luaL_checkudata(L, -1, "iota_marray");
	lua_pop(L, 1);
	return ima;
}

static inline void iota_view_op_copy2marray(int * idx, void * s1, void * s2, void * s3)
{
	iview * ivw = (iview *)s1;
	imarray * ima1 = (imarray *)s2;
	imarray * ima2 = (imarray *)s3;
		
	ima2->data[iota_indexes2index(ivw->rank, ima2->dimensions, idx)] = ima1->data[iota_view_getindex(ivw, idx)];
}

static inline void iota_iterator(int * buf, int d, int rank, int * dim, void * s1, void * s2, void *s3, void (*f)(int * idx, void *, void *, void *))
{
        int deep = d + 1;
        if (deep == 1) buf = (int *)calloc(rank, sizeof(int));
        for(int i=0; i<dim[deep-1]; i++) {
                buf[deep-1] = i;
                if (deep < rank) iota_iterator(buf, deep, rank, dim, s1, s2, s3, f);
                else f(buf, s1, s2, s3);
        }
        if (deep == 1) free(buf);
}

static imarray * iota_view_copy2marray(lua_State *L, int arg1)
{
	iview *ivw = luaL_checkudata(L, arg1, "iota_view");
	imarray *ima1 = iota_view_getmarray(L, arg1);
	
	int size = 1;
	for(int i=0; i<ivw->rank; i++) size *= ivw->dimensions[i];
	
	imarray * ima2 = iotaA_newmarray(L, ivw->rank, ivw->dimensions);
	
	iota_iterator(NULL, 0, ivw->rank, ivw->dimensions, ivw, ima1, ima2, iota_view_op_copy2marray);
	
	return ima2;
}

static inline void iota_marray_mulnumber(lua_State *L, int arg1, int arg2)
{
	double scal = luaL_checknumber(L, arg2);
	imarray *ima1 = luaL_checkudata(L, arg1, "iota_marray");
	
	imarray *ima2 = iotaA_newmarray(L, ima1->rank, ima1->dimensions);
	
	cblas_dcopy(ima2->size, ima1->data, 1, ima2->data, 1);
	cblas_dscal(ima2->size, scal, ima2->data, 1);
}

static inline void iota_view_mulnumber(lua_State *L, int arg1, int arg2)
{
	double scal = luaL_checknumber(L, arg2);
	iview *ivw = luaL_checkudata(L, arg1, "iota_view");
	
	imarray * ima = iota_view_copy2marray(L, arg1);
	cblas_dscal(ima->size, scal, ima->data, 1);
}

static int iota__mul(lua_State *L)
{	
	itype ity = 0;
	int nnum = 0;
	int ntyp = 0;
	
	if (lua_isnumber(L,1) && iotaA_type(L, 2) ) {
		ity = iotaA_type(L, 2);
		nnum = 1;
		ntyp = 2;
	}
	else if (iotaA_type(L, 1) && lua_isnumber(L, 2)) {
		nnum = 2;
		ity = iotaA_type(L, 1);
		ntyp = 1;
	}
	else luaL_error(L, "You need to multiply by a number\n");
	
	switch(ity) {
		case IMARRAY: iota_marray_mulnumber(L, ntyp, nnum); break;
		case IINDEXES: iota_view_fromindexes(L, ntyp);
		case IVIEW: iota_view_mulnumber(L, ntyp, nnum); break;
	}
	
	return 1;
}

static void iota_marray_isequal(lua_State *L, imarray *ima1, imarray *ima2)
{
	if (ima1->rank != ima2->rank) luaL_error(L, "The rank does not match\n");

	for (int i=0; i<ima1->rank; i++)
		if (ima1->dimensions[i] != ima2->dimensions[i])
		 luaL_error(L, "The dimension %d does not match!\n", i+1);
}

static inline void iota_marray_op_addview(int * idx, void * s1, void * s2, void * s3)
{
	imarray * ima1 = (imarray *) s1;
	iview * ivw = (iview *) s2;
	imarray * ima2 = (imarray *) s3;
	
	const int index1 = iota_indexes2index(ima1->rank, ima1->dimensions, idx);
	const int index2 = iota_view_getindex(ivw, idx);
	
	ima1->data[index1] = ima1->data[index1] + ima2->data[index2];
}

static int iota_marray_add(lua_State *L)
{
	imarray * ima1 = luaL_checkudata(L, 1, "iota_marray");
	
	switch (iotaA_type(L, 2)) {
		case IMARRAY: {
			imarray * ima2 = luaL_checkudata(L, 2, "iota_marray");
			iota_marray_isequal(L, ima1, ima2);
			cblas_daxpy(ima1->size, 1.0, ima2->data, 1, ima1->data, 1);
		} break;
		case IINDEXES: iota_view_fromindexes(L, 2);
		case IVIEW: {
			iview * ivw = luaL_checkudata(L, 2, "iota_view");
			imarray * ima2 = iota_view_getmarray(L, 2);
			iota_iterator(NULL, 0, ima1->rank, ima1->dimensions, ima1, ivw, ima2, iota_marray_op_addview);
		} break;
	}
}

static int iota__add(lua_State *L)
{
	itype ity1 = iotaA_type(L, 1);
	itype ity2 = iotaA_type(L, 2);
	
	if(! ity1 || ! ity2) luaL_error(L, "I don't known what to do!\n");
	
	switch(ity1) {
		case IMARRAY : {
			imarray * ima = luaL_checkudata(L, 1, "iota_marray");
			imarray * imacopy = iotaA_newmarray(L, ima->rank, ima->dimensions);
			lua_replace(L, 1);
			cblas_dcopy(ima->size, ima->data, 1, imacopy->data, 1);
			iota_marray_add(L);
			lua_remove(L, 2);
		break; }
		case IINDEXES: iota_view_fromindexes(L, 1);
		case IVIEW : {
			iota_view_copy2marray(L, 1);
			lua_replace(L, 1);
			iota_marray_add(L);
			lua_remove(L, 2);
		break; }
	}
	return 1;
}

static int iota__unm(lua_State *L)
{
	itype ity1 = iotaA_type(L, 1);
	if(! ity1 ) luaL_error(L, "I don't known what to do!\n");
	
	switch(ity1) {
		case IMARRAY : {
			imarray *ima1 = luaL_checkudata(L, 1, "iota_marray");
			imarray *ima2 = iotaA_newmarray(L, ima1->rank, ima1->dimensions);
			cblas_dcopy(ima2->size, ima1->data, 1, ima2->data, 1);
			cblas_dscal(ima2->size, -1.0, ima2->data, 1);
			lua_remove(L, 1);
		break; }
		case IINDEXES: iota_view_fromindexes(L, 1);
		case IVIEW : {
			iview * ivw = luaL_checkudata(L, 1, "iota_view");
			imarray * ima = iota_view_copy2marray(L, 1);
			cblas_dscal(ima->size, -1.0, ima->data, 1);
			lua_remove(L, 1);
		break; }
	}
	
	return 1;
}

static int iota__sub(lua_State *L)
{
	itype ity1 = iotaA_type(L, 1);
	if(! ity1 ) luaL_error(L, "I don't known what to do!\n");
	
	lua_insert(L, 1);
	iota__unm(L); // just copied
	lua_insert(L, 1);
	iota_marray_add(L);
	lua_remove(L, 2);
	
	return 1;
}

//
//		luaL_Reg & LAUMOD_API
//

static const struct luaL_Reg iota_lib [] = {
	{"new", iota_marray_new},
	//{"set", iota_marray_set},
	//{"get", iota_marray_get},
	{"reshape", iota_marray_reshape},
	{"toview", iota_marray_toview},
	{"slice", iota_view_slice},
	{NULL, NULL}
};

static const struct luaL_Reg iota_marray_mt [] = {
	{"__index", iota_marray__index},
	{"__newindex", iota_marray__newindex},
	{"__gc", iota_marray__gc},
	{"__add", iota__add},
	{"__sub", iota__sub},
	{"__unm", iota__unm},
	{"__mul", iota__mul},
	{NULL,NULL}
};

static const struct luaL_Reg iota_indexes_mt [] = {
	{"__index", iota_indexes__index},
	{"__newindex", iota_indexes__newindex},
	{"__mul", iota__mul},
	{NULL,NULL}
};

static const struct luaL_Reg iota_view_mt [] = {
	{"__index", iota_view__index},
	{"__newindex", iota_view__newindex},
	{"__gc", iota_view__gc},
	{"__mul", iota__mul},
	{"__add", iota__add},
	{"__sub", iota__sub},
	{"__unm", iota__unm},
	{NULL, NULL}
};

LUAMOD_API int luaopen_marray (lua_State *L)
{	
	luaL_newmetatable(L, "iota_marray");
	luaL_setfuncs(L, iota_marray_mt, 0);

	luaL_newmetatable(L, "iota_indexes");
	luaL_setfuncs(L, iota_indexes_mt, 0);
	
	luaL_newmetatable(L, "iota_view");
	luaL_setfuncs(L, iota_view_mt, 0);
	
	luaL_newlib(L, iota_lib);
	return 1;
}

