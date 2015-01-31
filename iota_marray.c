#include "iota_marray.h"

/*
 * 		TODO
 * 	Criar um iterador para as metainformações.
 *  Métodos GET and SET.
 */

/*
 *		DEBUG
 */

//#define DEBUG 1

#if defined(DEBUG) && DEBUG > 0
	#define DEBUG_PRINT(fmt, args...) fprintf(stderr, "DEBUG: %s:%d:%s(): " fmt, __FILE__, __LINE__, __func__, ##args)
#else
	#define DEBUG_PRINT(fmt, args...)
#endif

/*
 * 		ERROR
 */

typedef enum iota_error{
	IOTA_SUCCESS,
	IOTA_INDEX_OUT_OF_RANGE,
	IOTA_DIMENSION_OUT_OF_RANGE
}IOTA_ERROR;

static const char * IOTA_ERROR_NAMES[] = {
	"SUCCESS", 
	"INDEX OUT OF RANGE",
	"DIMENSION OUT OF RANGE"
};

/*
 * 		FORWARD DECLARATIONS
 */

static int view_copy2marray(lua_State *L, int m, int n);
static imarray * iota_newmarray(lua_State *L, const int rank, const int * dimensions);

/*
 *		AUXILIARY
 */

static IOTA_ERROR checkdimensions(const imarray * ima, const int *idx)
{
	const int rank = ima->vw == NULL ? ima->rank : ima->vw->rank;
	const int * dim = ima->vw == NULL ? ima->dimensions : ima->vw->dimensions;
	
	for(int i=0; i<rank;i++)
		if ((idx[i] < 0) || (idx[i] >= dim[i]))
			return IOTA_DIMENSION_OUT_OF_RANGE;
			
	return IOTA_SUCCESS;
}

int * stride_from_dimension(const int rank, const int * dimensions)
{
	int * stride = (int *)calloc(rank, sizeof(int));
	for(int i=0; i<rank; i++) {
		stride[i] = 1;
		for(int j=0; j<i; j++) stride[i] *= dimensions[j]; 
	}
	return stride;
}

imarray * iota_createmarray(lua_State *L, const int size, const int rank, const int * dimensions)
{
	imarray * ima = (imarray *)lua_newuserdata(L, sizeof(imarray));
	
	if (ima == NULL) {
		lua_pushnil(L);
		return ima;
	}
	
	lua_newtable(L);
	lua_setuservalue(L, -2);
	
	ima->dimensions = (int *)calloc(rank, sizeof(int));
	for(int i=0; i<rank; i++) ima->dimensions[i] = dimensions[i];

	ima->stride = stride_from_dimension(rank, ima->dimensions);
	ima->rank = rank;
	ima->size = size;
	ima->data = (double *)calloc(size, sizeof(double));
	ima->vw = NULL;

	return ima;
}

void iota_checkudata(lua_State *L, int arg)
{
	if (!(luaL_testudata(L, arg, "iota_marray") != NULL || luaL_testudata(L, arg, "iota_view") != NULL))
		luaL_error(L, "Expected userdata, 'iota_marray' or 'iota_view', got %s", lua_typename(L,arg));		
}

void iota_meta_getmt(lua_State *L, int n)
{
	lua_pushvalue(L, n);
	if (((imarray *)lua_touserdata(L,n))->vw != NULL) {
		lua_getuservalue(L, -1);
		lua_remove(L, -2);
	}	
	lua_getuservalue(L, -1);
	lua_remove(L, -2);
	if (lua_type(L,-1) != LUA_TTABLE)
		luaL_error(L, "Error obtaining meta information table of 'iota_marray'\n");
}

static inline bool _str_isequal(const char * str1, const char * str2)
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

static void check_isequal(lua_State *L, int m, int n)
{	
	iota_checkudata(L,m);
	iota_checkudata(L,n);

	imarray * ima = (imarray *)lua_touserdata(L, m);
	const int *dim1 = ima->vw == NULL ? ima->dimensions : ima->vw->dimensions;
	const int rank1 = ima->vw == NULL ? ima->rank : ima->vw->rank;
	
	ima = (imarray *)lua_touserdata(L, n);
	const int * dim2 = ima->vw == NULL ? ima->dimensions : ima->vw->dimensions;
	const int rank2 = ima->vw == NULL ? ima->rank : ima->vw->rank;
	
	if (rank1 != rank2)	luaL_error(L, "The ranks (%d, %d) do not match\n", rank1, rank2);

	for (int i=0; i<rank1; i++)
		if (dim1[i] != dim2[i]) luaL_error(L, "The dimension %d does not match!\n", i+1);
}

static void intarray2table(lua_State *L, int len, int * array)
{
	lua_createtable(L, len, 0);
	for (int i=0; i<len; i++) {
		lua_pushinteger(L, array[i]);
		lua_rawseti(L, -2, i+1);
	}
}

/*
static int l_marray_create(lua_State *L)
{
	int rank = lua_gettop(L) - 1;
	int * dimensions = calloc(rank, sizeof(int));
	int size = luaL_checkinteger(L, 1);
	
	for(int i=0; i<rank; i++) {
		int dim = luaL_checkinteger(L, 2+i);
		dimensions[i] = dim;
	}

	iota_createmarray(L, size, rank, dimensions);
	free(dimensions);
	return 1;
}
*/

/*
 *		MARRAY METAINFORMATION GET AND SET
 */

static int meta_geti(lua_State *L, int n, int i)
{
	iota_meta_getmt(L,n);
	int res = lua_rawgeti(L, -1, i);
	lua_remove(L, -2);
	return res;
}

static void meta_seti(lua_State *L, int n, int k, int i)
{
	iota_meta_getmt(L,n);
	lua_pushvalue(L,k);
	lua_rawseti(L, -2, i);
	lua_remove(L,-1);
}

/*
 *		MARRAY
 */

static inline int marray_getindex(const int rank, const int *stride, const  int *idx)
{
	int index = 0;
	for(int i=0; i<rank; i++) index += stride[i]*idx[i];
	return index;
}

static int l_marray_reshape(lua_State *L)
{
	int newrank = lua_gettop(L) - 1;
	int *newdim = (int *)calloc(newrank, sizeof(int));
	
	imarray *ima = luaL_checkudata(L, 1, "iota_marray");

	int newsize = 1;
	
	for(int i=0; i<newrank; i++){
		newdim[i] = luaL_checkinteger(L, 2+i);
		if (newdim[i] <= 0) luaL_error(L,"New dimensions must be > 0");
		newsize *= newdim[i];
	}

	if (newsize != ima->size){
		free(newdim);
		luaL_error(L, "New size (%d) is different of the old (%d) one. You can reshape only same sized objects\n!", newsize, ima->size);
	}

	ima->rank = newrank;
	
	free(ima->dimensions);
	ima->dimensions = newdim;

	free(ima->stride);
	ima->stride = stride_from_dimension(newrank, newdim);
	
	return 0;
}

static int marray__gc(lua_State *L)
{
	imarray * ima = (imarray *)lua_touserdata(L, 1);
	free(ima->dimensions);
	free(ima->stride);
	free(ima->data);
	if (ima->vw != NULL) { printf("PROBLEMAO\n"); fflush(stdout); }
	free(ima->vw);
	return 0;
}

/*
 *		VIEWS
 */

static int view__gc(lua_State *L)
{
	imarray * ima = (imarray *)lua_touserdata(L, 1);
	free(ima->vw->dimensions);
	free(ima->vw->vector);
	free(ima->vw->matrix);
	free(ima->vw->indexes);
	free(ima->vw);
	free(ima->dimensions);
	free(ima->stride);
	return 0;
}

static inline void view_matrixset(imarray *ivw, const int i, const int j, const int v)
{
	ivw->vw->matrix[i + ivw->vw->rows*j] = v;
}

static inline int view_matrixget(const imarray *ivw, const int i, const int j)
{
	return ivw->vw->matrix[i + ivw->vw->rows*j];
}

#ifdef DEBUG
static void printview(const imarray *ima)
{
	const iview * ivw = ima->vw;
	
	printf("'matrix': %d X %d\n", ivw->rows, ivw->cols);
	for(int i=0; i<ivw->rows; i++) {
		for(int j=0; j<ivw->cols; j++)
			printf("%d ", view_matrixget(ima,i,j) );
		printf("\n");
	}
	printf("'vector': %d\n", ivw->rows);
	for (int i=0; i<ivw->rows; i++) printf("%d\n", ivw->vector[i]);
}
#endif

static inline int view_getindex(const imarray * ivw, const int * viewidx)
{
	for(int i=0; i<ivw->vw->rows; i++){
		ivw->vw->indexes[i] = ivw->vw->vector[i];
		for(int j=0; j<ivw->vw->cols; j++)
			ivw->vw->indexes[i] += view_matrixget(ivw, i, j)*viewidx[j];
	}
	return marray_getindex(ivw->rank, ivw->stride, ivw->vw->indexes);
}

static void view_frommarray(lua_State *L, int n)
{
	imarray *ima1 = luaL_checkudata(L, n, "iota_marray");

	imarray * ima2 = (imarray *)lua_newuserdata(L, sizeof(imarray)); 
	ima2->vw = (iview *)calloc(1,sizeof(iview));;
	iview * ivw = ima2->vw;
		
	luaL_getmetatable(L, "iota_view");
	lua_setmetatable(L, -2);
	
	lua_pushvalue(L, n);
	lua_setuservalue(L, -2);
	
	ivw->rank = ima1->rank;
	ivw->rows = ima1->rank;
	ivw->cols = ima1->rank;

	ima2->rank = ima1->rank;
	ima2->data = ima1->data;
	ima2->dimensions = (int*)calloc(ima1->rank, sizeof(int));
	ima2->size = ima1->size;
	
	ivw->dimensions = (int*)calloc(ivw->rank, sizeof(int));
	ivw->matrix = (int*)calloc(ivw->rank*ivw->rank, sizeof(int));
	
	for(int i=0; i<ivw->rank; i++) {
		ima2->dimensions[i] = ima1->dimensions[i];
		ivw->dimensions[i] = ima1->dimensions[i];
		view_matrixset(ima2, i, i, 1);
	}
	
	ima2->stride = stride_from_dimension(ima2->rank, ima2->dimensions);
	
	ivw->vector = (int*)calloc(ivw->rank, sizeof(int));
	ivw->indexes = (int*)calloc(ima1->rank, sizeof(int));

	lua_replace(L, n);
}

static IOTA_ERROR view_newslice(lua_State *L, int n, const int dim, const int start, const int end, const int step)
{
	imarray * imao = (imarray *)luaL_checkudata(L, n, "iota_view");
	iview * ivwo = imao->vw;
	
	/* check index */
	IOTA_ERROR err;
	int idx[imao->rank];
	for (int i=0; i<imao->rank; i++) idx[i] = 0;
	idx[dim] = start;
	if ((err = checkdimensions(imao, idx)) != IOTA_SUCCESS) {
		lua_pushnil(L);
		return err;
	}
	idx[dim] = end;
	if ((err = checkdimensions(imao, idx)) != IOTA_SUCCESS) {
		lua_pushnil(L);
		return err; 
	}
	/* */
	
	imarray * ima = (imarray *)lua_newuserdata(L,sizeof(imarray));
	iview * ivw = (iview *)calloc(1,sizeof(iview));
	ima->vw = ivw;
	
	lua_getuservalue(L, n);
	lua_setuservalue(L, -2);
	
	luaL_getmetatable(L, "iota_view");
	lua_setmetatable(L, -2);
	
	ima->size = imao->size;
	ima->data = imao->data; 
	ivw->indexes = (int*)calloc(imao->rank, sizeof(int));
	ima->rank = imao->rank;
	ima->stride = (int*)calloc(imao->rank, sizeof(int));
	ima->dimensions = (int*)calloc(imao->rank, sizeof(int));
	for(int i=0; i<imao->rank; i++){
		 ima->stride[i] = imao->stride[i];
		 ima->dimensions[i] = imao->dimensions[i];
	}
	
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
				if ((end-start) < 0 ) luaL_error(L, "This slice doesn't make sense.");
				int dim=0; for(int i=start;i<=end;i+=step, dim++);
				ivw->dimensions[count] = dim;
				mat[i][count] = step;
				count++;
			} else if ((start != end) && (i != dim)) {
				ivw->dimensions[count] = ivwo->dimensions[i];
				mat[i][count] = 1;
				count++;
			}
		}
	// allocating memory
	ivw->rows = ima->rank;
	ivw->cols = ivw->rank;
	ivw->matrix = (int*)calloc(ivw->rows * ivw->cols ,sizeof(int));
	ivw->vector = (int*)calloc(ivw->rows, sizeof(int));
	// converting to original shape
	for (int i=0; i<ivwo->rows; i++) {
		
		for (int j=0; j<ivwo->rank; j++)
			ivw->vector[i] += view_matrixget(imao, i, j) * vec[j];
		ivw->vector[i] += ivwo->vector[i];
		
		for(int j=0; j<ivw->cols; j++) {
			int b = 0;
			for(int k=0; k<ivwo->cols; k++)
				b += view_matrixget(imao, i, k)*mat[k][j];
			
			view_matrixset(ima, i, j, b);
		}
	}
	
	#ifdef DEBUG
		printview(imao);
		printview(ima);
	#endif
	
	return IOTA_SUCCESS;
}

static IOTA_ERROR view_fromindexes(lua_State *L, int n)
{
	IOTA_ERROR err;
	iindexes *idx = (iindexes *)luaL_checkudata(L, n, "iota_indexes");
	
	lua_getuservalue(L, n);
	int pos = lua_gettop(L);
	
	if (((imarray *)lua_touserdata(L,pos))->vw == NULL) view_frommarray(L, pos);

	int count = 0;
	for (int i=0; i<idx->deep; i++) {
		if (idx->indexes[i] < 0) {
			
			if (meta_geti(L,pos, -idx->indexes[i]) != LUA_TTABLE) printf("table not found!\n");
			lua_rawgeti(L,-1,1);	int start = lua_tointeger(L, -1) - 1; 	lua_pop(L,1);
			lua_rawgeti(L,-1,2);	int   end = lua_tointeger(L, -1) - 1;	lua_pop(L,1);	
			lua_rawgeti(L,-1,3);	int  step = lua_tointeger(L, -1);	lua_pop(L,1);
			lua_pop(L,1);
			
			if ((err = view_newslice(L, pos, count, start, end, step)) != IOTA_SUCCESS) goto error;
			if (start != end) count++;
			lua_remove(L, pos);
		}
		else if (idx->indexes[i] > 0) {
			if ((err = view_newslice(L, pos, count, idx->indexes[i]-1, idx->indexes[i]-1, 1)) != IOTA_SUCCESS) goto error;
			lua_remove(L, pos); //remove old view from stack
		}
		else count++; //dummy
	}
	
	lua_replace(L, n);
	luaL_checkudata(L,n,"iota_view");
	return IOTA_SUCCESS;
	
	error:
	lua_remove(L,pos);
	lua_replace(L,n);
	luaL_checktype(L,n,LUA_TNIL);
	return err;
}

/*
 *		INDEXES
 */

int iota_getindex(const imarray * ima, const int * idx)
{
	int index = 0;	
	if (ima->vw == NULL) index = marray_getindex(ima->rank, ima->stride, idx);
	else index = view_getindex(ima, idx);
	return index;
}

static iindexes * create_iota_indexes(lua_State *L, int rank)
{	
	size_t nbytes = sizeof(iindexes) + (rank - 1)*sizeof(int);
	iindexes *idx = (iindexes *)lua_newuserdata(L, nbytes);
		
	lua_pushvalue(L, 1);
	lua_setuservalue(L, -2);
	idx->rank = rank;
	idx->deep = 0;
	idx->isview = false;
			
	luaL_getmetatable(L, "iota_indexes");
	lua_setmetatable(L, -2);

	lua_replace(L,1);
	return idx;
}

static int indexes__index(lua_State *L)
{
	IOTA_ERROR err;
	iindexes * idx = (iindexes *)lua_touserdata(L, 1);

	bool is_newindex = lua_gettop(L) == 3 ? true : false;

	switch(lua_type(L,2)) {
		case LUA_TNUMBER:
		{
			idx->indexes[idx->deep] = luaL_checkinteger(L, 2);
			break;
		}
		case LUA_TTABLE:
		{
			idx->indexes[idx->deep] = -(idx->deep + 1);
			lua_getuservalue(L,1);
			meta_seti(L,-1,2,idx->deep+1);
			lua_pop(L,1);
			idx->isview = true;
			break;
		}
		case LUA_TNIL:
		{
			idx->indexes[idx->deep] = 0;
			idx->isview = true;
			break;
		}
	}
	
	idx->deep = idx->deep + 1;
	
	if (idx->deep == idx->rank) {
		if (idx->isview) {
			if( (err = view_fromindexes(L,1)) != IOTA_SUCCESS) {
				//lua_pushstring(L,IOTA_ERROR_NAMES[err]);
				lua_pushnil(L);
				return 1;
			}
			if (is_newindex) view_copy2marray(L,3,1);
			else lua_pushvalue(L,1);
		} else {
			for (int i=0; i<idx->rank; i++) idx->indexes[i] -= 1;
			lua_getuservalue(L, 1);
			imarray *ima = (imarray *)lua_touserdata(L, -1);
			if ((err = checkdimensions(ima, idx->indexes)) != IOTA_SUCCESS) {
				if (is_newindex)
					return luaL_error(L,IOTA_ERROR_NAMES[err]);
				else {
					lua_pushnil(L);
					return 1;
				}
			}
			if (is_newindex) ima->data[iota_getindex(ima, idx->indexes)] = luaL_checknumber(L, 3);
			else lua_pushnumber(L, ima->data[iota_getindex(ima, idx->indexes)]);
		}
	}
	else lua_pushvalue(L,1);
	
	return is_newindex ? 0 : 1;
}

static int view__index(lua_State *L)
{
	const imarray * ima = (imarray * )luaL_checkudata(L, 1, "iota_view");
	const iview * ivw = ima->vw;
	
	bool is_newindex = lua_gettop(L) == 3 ? true : false;
	
	switch (lua_type(L, 2)) {
		case LUA_TTABLE:
		case LUA_TNUMBER:
		case LUA_TNIL:
		{
			create_iota_indexes(L, ivw->rank);
			indexes__index(L);
			break;
		}
		case LUA_TSTRING:
		{
			if (! is_newindex) {
				const char * string = luaL_checkstring(L, 2);
				if (_str_isequal(string, "rank")) {lua_pushinteger(L, ivw->rank); break;}
				if (_str_isequal(string,  "dim")) {intarray2table(L,ivw->rank,ivw->dimensions); break;}
				if (_str_isequal(string, "type")) {lua_pushstring(L, "iota_view"); break;}
			}
		}
		default:
		{
			lua_getuservalue(L, 1);
			lua_insert(L,1);
			if (is_newindex) lua_settable(L, 1);
			else lua_gettable(L, 1);
		}
	}
	
	return is_newindex ? 0 : 1;
}

static int marray__index(lua_State *L)
{
	imarray * ima = (imarray *)luaL_checkudata(L, 1, "iota_marray");

	bool is_newindex = lua_gettop(L) == 3 ? true : false;

	switch (lua_type(L, 2)) {
		case LUA_TTABLE:
		case LUA_TNUMBER:
		case LUA_TNIL:
		{
			create_iota_indexes(L, ima->rank);
			indexes__index(L);
			break;
		}
		case LUA_TSTRING:
		{
			if (! is_newindex) {
				const char * string = luaL_checkstring(L, 2);
				if (_str_isequal(string, "rank")) { lua_pushinteger(L, ima->rank); break; }
				if (_str_isequal(string, "size")) { lua_pushinteger(L, ima->size); break; }
				if (_str_isequal(string,  "dim")) { intarray2table(L, ima->rank, ima->dimensions); break; }
				if (_str_isequal(string, "type")) {lua_pushstring(L, "iota_marray"); break;}
			}
		}
		default:
		{
			lua_getuservalue(L, 1);
			lua_insert(L,1);
			if (is_newindex) lua_rawset(L, 1);
			else lua_rawget(L, 1);
		}
	}
	
	return is_newindex ? 0 : 1;
}

/*
 *		BASIC ITERATOR AND COPY 
 */

static inline void _iterator(int * buf, int d, const int rank, const int * dim, void * s1, void * s2, void *s3, void (*f)(int * idx, void *, void *, void *))
{
	for(int i=0; i<dim[d]; i++) {
		buf[d] = i;
		if (d+1 < rank) _iterator(buf, d+1, rank, dim, s1, s2, s3, f);
		else f(buf, s1, s2, s3);
	}
}

static inline void iota_iter(const int rank, const int * dim, void * s1, void * s2, void *s3, void (*f)(int * idx, void *, void *, void *))
{
	int buf[rank];
	int d = 0;
	_iterator(buf, d, rank, dim, s1, s2, s3, f);
}

static void data_op_copy(int * idx, void * s1, void * s2, void * s3)
{
	const imarray * ima1 = (imarray *)s1;
	imarray * ima2 = (imarray *)s2;
	const int index1 = iota_getindex(ima1, idx);
	const int index2 = iota_getindex(ima2, idx);
	ima2->data[index2] = ima1->data[index1];
}

static int marray_tomarray(lua_State *L)
{
	iota_checkudata(L, 1);
	imarray * ima1 = (imarray *)lua_touserdata(L, 1);
	imarray * ima2 = iota_newmarray(L, ima1->vw == NULL ? ima1->rank : ima1->vw->rank , 
	                                   ima1->vw == NULL ? ima1->dimensions : ima1->vw->dimensions);
	if (ima2 == NULL) lua_pushnil(L);
	else iota_iter(ima2->rank, ima2->dimensions, ima1, ima2, NULL, data_op_copy);
	return 1;
}

static int view_copy2marray(lua_State *L, int m, int n)
{
	iota_checkudata(L,m);
	iota_checkudata(L,n);
	
	check_isequal(L,m,n);
	
	imarray * ima1 = (imarray *)lua_touserdata(L, m);
	imarray * ima2 = (imarray *)lua_touserdata(L, n);
	
	iota_iter(ima1->vw == NULL ? ima1->rank : ima1->vw->rank,
				ima1->vw == NULL ? ima1->dimensions : ima1->vw->dimensions,
				ima1, ima2, NULL, data_op_copy);
	
	return 0;
}

/*
 *		IN-PLACE ARITHMETIC OPERATIONS
 */

static inline void data_op_mulnumber(int *idx, void *s1, void *s2, void *s3)
{
	imarray * ima = (imarray *) s1;
	const double alpha = *((double *)s2);
	// s3 = NULL
	ima->data[iota_getindex(ima,idx)] *= alpha;
}

static int data_inplace_scal(lua_State *L)
{
	iota_checkudata(L, 1);
	imarray * ima = (imarray *) lua_touserdata(L,1);
	double alpha = luaL_checknumber(L,2);
	iota_iter(ima->vw == NULL ? ima->rank : ima->vw->rank,
					ima->vw == NULL ? ima->dimensions : ima->vw->dimensions,
					ima, &alpha, NULL, data_op_mulnumber);
	return 0;
}

static inline void data_op_unm(int * idx, void * s1, void * s2, void * s3)
{
	imarray * ima = (imarray *) s1;
	// s2 = NULL
	// s3 = NULL
	const int index = iota_getindex(ima,idx);
	ima->data[index] = -ima->data[index];
}

static int data_inplace_unm(lua_State *L)
{
	iota_checkudata(L, 1);
	imarray * ima = (imarray *)lua_touserdata(L,1);
	iota_iter(ima->vw == NULL ? ima->rank : ima->vw->rank,
					ima->vw == NULL ? ima->dimensions : ima->vw->dimensions,
					ima, NULL, NULL, data_op_unm);	
	return 0;
}

static inline void data_op_adddata(int * idx, void * s1, void * s2, void * s3)
{
	imarray * ima1 = (imarray *)s1;
	const imarray * ima2 = (imarray *)s2;
	// s3 = NULL
	ima1->data[iota_getindex(ima1, idx)] += ima2->data[iota_getindex(ima2, idx)];
}

static int data_inplace_add(lua_State *L)
{
	imarray * ima1 = (imarray *)lua_touserdata(L, 1);
	imarray * ima2 = (imarray *)lua_touserdata(L, 2);
	check_isequal(L, 1,2);
	iota_iter(ima1->vw == NULL ? ima1->rank : ima1->vw->rank,
					ima1->vw == NULL ? ima1->dimensions : ima1->vw->dimensions,
					ima1, ima2, NULL, data_op_adddata);			
	return 0;
}

static inline void data_op_subdata(int * idx, void * s1, void * s2, void * s3)
{
	imarray * ima1 = (imarray *)s1;
	const imarray * ima2 = (imarray *)s2;
	// s3 = NULL
	ima1->data[iota_getindex(ima1, idx)] -= ima2->data[iota_getindex(ima2, idx)];
}

static int data_inplace_sub(lua_State *L)
{
	imarray * ima1 = (imarray *)lua_touserdata(L, 1);
	imarray * ima2 = (imarray *)lua_touserdata(L, 2);
	check_isequal(L, 1,2);
	iota_iter(ima1->vw == NULL ? ima1->rank : ima1->vw->rank,
					ima1->vw == NULL? ima1->dimensions : ima1->vw->dimensions,
					ima1, ima2, NULL, data_op_subdata);			
	return 0;
}

/*
 * 		ARITHMETIC METAMETHOD
 */

static int __add(lua_State *L)
{
	check_isequal(L,1,2);
	marray_tomarray(L);
	lua_replace(L,1);
	data_inplace_add(L);
	lua_pushvalue(L,1);
	return 1;
}

static int __sub(lua_State *L)
{
	check_isequal(L,1,2);
	marray_tomarray(L);
	lua_replace(L,1);
	data_inplace_sub(L);
	lua_pushvalue(L,1);
	return 1;
}

static int __mul(lua_State *L)
{
	if (lua_type(L,1) == LUA_TNUMBER && lua_type(L,2) == LUA_TUSERDATA) { 
		//printf("__mul number, data\n"); fflush(stdout);
		lua_insert(L,1);
		marray_tomarray(L);
		lua_replace(L,1);
	}
	else if (lua_type(L,2) == LUA_TNUMBER && lua_type(L,1) == LUA_TUSERDATA) {
		//printf("__mul data, number\n"); fflush(stdout);
		marray_tomarray(L);
		//printf("tomarray OK\n");
		lua_replace(L,1);
	}
	else luaL_error(L,"Operation not defined.\n");
	
	data_inplace_scal(L);
	lua_pushvalue(L,1);
	
	return 1;
}

static int __unm(lua_State *L)
{
	//iota_checkudata(L,1);
	marray_tomarray(L);
	lua_replace(L,1);
	data_inplace_unm(L);
	lua_pushvalue(L,1);
	return 1;
}

/*
 *		luaL_Reg & LAUMOD_API
 */

static const struct luaL_Reg marray_metamethods [] = {
	{"__add", __add},
	{"__sub", __sub},
	{"__mul", __mul},
	{"__unm", __unm},
	{NULL,NULL}
};

static const struct luaL_Reg marray_operators [] = {
	{"scal",data_inplace_scal},
	{"add",data_inplace_add},
	{"sub",data_inplace_sub},
	{"unm",data_inplace_unm},
	{"tomarray", marray_tomarray},
	{NULL, NULL}
};

static imarray * iota_newmarray(lua_State *L, const int rank, const int * dimensions)
{
	int size = 1;
	for(int i=0; i<rank; i++) size *= dimensions[i];
	if (size <= 0) return NULL;
	
	imarray * ima = iota_createmarray(L, size, rank, dimensions);
	if (ima == NULL) return ima;
	
	luaL_getmetatable(L, "iota_marray");
	lua_setmetatable(L, -2);
	iota_meta_getmt(L,-1);
	luaL_setfuncs(L, marray_operators, 0);
	lua_pop(L,1);
	
	return ima;
}

static int l_marray_new(lua_State *L)
{
	int rank = lua_gettop(L);
	
	if (rank <= 0) {
		lua_pushnil(L);
		return 1;
	}
	
	int dimensions[rank];
	for(int i=0; i<rank; i++) {
		dimensions[i] = luaL_checkinteger(L, 1+i);
		if (dimensions[i] <= 0) luaL_error(L,"The dimensions must be > 0\n");
	}
	
	if (iota_newmarray(L, rank, dimensions) == NULL) lua_pushnil(L);
	
	return 1;
}

static const struct luaL_Reg iota_lib [] = {
	{"new", l_marray_new},
	{"reshape", l_marray_reshape},
	{NULL, NULL}
};

static const struct luaL_Reg marray_mt [] = {
	{"__index", marray__index},
	{"__newindex", marray__index},
	{"__gc", marray__gc},
	{NULL,NULL}
};

static const struct luaL_Reg indexes_mt [] = {
	{"__index", indexes__index},
	{"__newindex", indexes__index},
	{NULL,NULL}
};

static const struct luaL_Reg view_mt [] = {
	{"__index", view__index},
	{"__newindex", view__index},
	{"__gc", view__gc},
	{NULL, NULL}
};

#define MULTLINE(...) #__VA_ARGS__

	static char iterator[] = { MULTLINE(
local function iter(ma)
	local ima = ma
	return coroutine.wrap( function ()
		local i = {}
		local n = assert(ima.dim)
			local function recfor(d)
				for k=1,n[d] do
					i[d] = k
					if d == ima.rank then
						if not ima.idx then
							coroutine.yield(table.unpack(i))
						elseif ima.idx(i) then
							coroutine.yield(table.unpack(i))
						end
					end
					if d+1 <= ima.rank then	recfor(d+1)	end
				end
			end
			recfor(1)
		end)
end
return iter
	)};

LUAMOD_API int luaopen_iota_marray (lua_State *L)
{	
	luaL_newmetatable(L, "iota_marray");
	luaL_setfuncs(L, marray_mt, 0);
	luaL_setfuncs(L, marray_metamethods, 0);

	luaL_newmetatable(L, "iota_indexes");
	luaL_setfuncs(L, indexes_mt, 0);
	
	luaL_newmetatable(L, "iota_view");
	luaL_setfuncs(L, view_mt, 0);
	luaL_setfuncs(L, marray_metamethods, 0);
	
	luaL_newlib(L, iota_lib);
	
	if (luaL_dostring(L, iterator)) luaL_error(L, "Error");
	lua_setfield(L, -2, "iter");

	return 1;
}
