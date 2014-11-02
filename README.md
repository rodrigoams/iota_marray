iota_marray
===========

Fast Multi-Dimensional Arrays for Lua (DEVELOPMENT VERSION)

A multi-dimensional array (marray) is a contiguous memory block with fixed size and indexed by n numbers. We can create a new marray with 2 dimensions (rank 2) and size 6000 in Lua with

local i = require"marray"
ma = i.new(10, 20, 30)

that can be read/write using the table syntax

for i=1,ma.dim[1] do
  for j=1,ma.dim[2] do
    for k=1,ma.dim[3] do
      ma[i][j][k] = math.random()
    end
  end
end

where we have made use of the dimensions attribute. We note that marrays have more two intrinsic attributes, namely

ma.rank == 600
ma.size = 10*20*30

To be more funny the marrays also possess a regular Lua table that can be indexed for non-number, for example, if we do

ma.name = 'say hello :'
ma.HI = function() return 'HI!' end

then print(ma.name, ma.HI()) will print

say hello : HI!

You can always use lua objects as indexes, except numeric ones that are mapped to the marray.


RESHAPE

The intrinsic attributes can't be changed directly, instead we need to use the reshape facility

i.reshape(mb, 10*20*30)

such that ma is now indexed with only 1 index, and is easily initialized

for i=1,ma.dim[1] do
  ma[i] = math.random()
end

After the initialization,  we can reshape again

i.reshape(ma, 10, 20, 30)

to obtain the original shape. You can always reshape a marray since that its size remains unchanged.


SLICES AND VIEWS

The reshape is very funny, but if we need a sub-portion of the marray? simple, slice it!

v1 = i.slice(ma, 2, 10)

yields a new VIEW, with rank = ma.rank - 1 such that va[i][j] equals ma[i][10][k]. We can also specify an 'end' and 'step' options to slice

v2 = i.slice(ma, 3, 10, 20, 5)

creates another view with the same rank as ma, but the third dimension goes from 10 to 20 with steps of 5, namely,

v2[i][j][1] == ma[i][j][10] 
v2[i][j][2] == ma[i][j][15]
v2[i][j][3] == ma[i][j][20]

with 

i=1,mb.dim[1]
j=1,mb.dim[2] 

and k=1,2,3. The respective dimensions are updated, then

for i=1,v2.dim[1] do
  for j=1,v2.dim[2] do
    for k=1,v2.dim3[3] do
      v2[i][j][k] = math.random()
    end
  end
end

is perfectly correct. 

The specific case 

v3 = i.slice(i.slice(ma, 5, 1), 10, 1)

has the equivalent construction

v3 = i.toview(ma[5][10])

The views always 'remember' the original shape of the marray, then you can reshape your marray again

i.reshape(ma, 6000)

but v3[i], v2[i][j][k] and v1[i][j] remains accessing the same numbers as before! 

Finally, v3.name == ma.name !

ARITHMETIC OPERATIONS

Marray/views also possess some basic mathematical operations. You can perform some fast operations between two marrays with the same size, for example

mb = ma*3.0
mb = ma - 0.5*mb

are all valid operations.

It is also possible to use views, 

vd = i.toview(ma[2])
mc = ma[1]*0.5  - i.slice(ma, 2) * math.random() + 5.0*vd

TO COMPILE

On Ubuntu,

gcc -std=c99 -I./ iota_lmarraylib.c -fPIC -shared -o marray.so -L./liblua.a -lblas -lm

but this can change with you specific OS.

TODO

Basic index check!
Some intrinsic mathematical operations.
Clean the code.
More?

















