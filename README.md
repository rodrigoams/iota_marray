# iota_marray
=============

## BASIC

Fast Multi-Dimensional Arrays for Lua 5.3 (Version 31th January 2015)

A multi-dimensional array (marray) is a contiguous memory block with fixed size indexed by n integer numbers. We can create a new marray with 3 dimensions (rank 3) and size 6000 with

```Lua
local marray = require"iota.marray"
ima = marray.new(10, 20, 30)
```

that can be read/write using the table syntax

```Lua
for a,b,c marray.iter(ima) do
   ima[a][b][c] = math.random()
end

```
where we have made use of the **dim** (dimension) attribute and the **iterator**. We note that marrays have more two intrinsic attributes, namely, rank and size, as expected:

```Lua
ima.rank == 3
ima.size == 10*20*30
```

also, the index start with 1!

To be more funny the marrays also possess a regular Lua table that can be indexed for non-number, for example, if we do

```Lua
ima.name = 'say hello :'
ima.HI = function() return 'HI!' end
```

then `print(ma.name, ma.HI())` will print

```Lua
say hello : HI!
```

You can always use lua objects as indexes, except numeric ones and lua tables, that are mapped to the marray (see next).

## RESHAPE

The three intrinsic attributes can't be changed directly, instead we need to use the reshape facility

```Lua
marray.reshape(ima, 10*20, 30)
```

such that `ima` is now indexed with only 2 index. This operation do not alter the data, only the access pattern to the memory segment. 

## SLICES AND VIEWS

The reshape is very funny, but if we need a sub-portion of the marray? simple, **slice** it!

```Lua
ivw1 = ima[{1,200,5}][6]
```

yields a new rank 1 **view**, such that `ivw1[a]` equals `ima[b][6]` with

```Lua
ivw[1] == ima[1][6]
ivw[2] == ima[6][6]
ivw[3] == ima[11][6]
...
```

i.e. a step of 5 elements.

Views and marray are almost the same objects, but a view do not posses a proper memory segment, it inherit from the original marray. Also, the original shape are store, such that you can reshape the marray without alter the access pattern of the view.
 
## EXPRESS ASSIGNMENTS AND DUMMY INDEX

Now, if you have another marray, say

```Lua
imb = marray.new(40)
```

and want to copy the elements of `ivw1`, simple do

```Lua
imb[_] = ivw1[_]
```

where we used a **dummy index** (_). This type of index can be used to select *all* elements of a given dimension, and can be used to create new views

```Lua
marray.reshare(ima,10,20,30)
ivw2 = ima[5][_][{5,30,4}]
``` 

such that

```Lua
ivw2[1][1] == ima[5][1][5]
ivw2[2][2] == ima[5][2][9]
... 
ivw2[20][7] == ima[5][20][29]
```

## ARITHMETIC OPERATIONS

Marray and views possess some basic mathematical operations. You can perform some fast operations between two marrays with the same dimensions, for example:

```Lua
imc = imb*3.0 - ivw1/5
```

## COMPILE

See the Makefile!
