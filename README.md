iota_marray
===========

Fast Multi-Dimensional Arrays for Lua (ALPHA VERSION)

An multi-dimensional (marray) is a common numeric array with a specific size but indexed by n numbers. We can create a new marray with 2 dimensions (rank 2) and size 6000 in Lua with

local ima = require"marray"
ma = ima.new(10, 20, 30)

that can be read/write from the traditional syntax

for i=1,ma.dim[1] do
  for j=1,ma.dim[2] do
    for k=1,ma.dim[3] do
      ma[i][j][k] = math.random()
    end
  end
end

where we have made use of the dimensions attribute. We note that marrays have more two intrinsic attributes, namely, ma.rank and ma.size, with obviosly meaning.

The marrays also possess some basic mathematical attributes, you can perform some operations on two identical size marrays, for example

mb = ia.new(10, 600)

mb = ma*3.0
mb = ma - 0.5*mb

are all valid operations, but the result is not so obviouly!

To be more funny the marrays also possess a regular Lua table that can be indexed

ma.name = 'say hello :'
ma.HI = function() return 'HI!' end

then print(ma.name, ma.HI()) will print

say hello : HI!

RESHAPE

We can use the reshape facility

ia.reshape(mb, 10*600)

such that mb is now indexed with 1 index, and inittialize very easy 

for i=1,mb.dim[1] do
  mb[i] = math.random()
end

After the assigmentent, we can reshape again

is.reshape(mb, 10, 600)

to obtain the original shape. You can always reshape a marray, but do not change its size.

SLICES AND VIEWS

The reshape is very funny, but what if we need a sub-portion of the marray? simple, slice it!

v1 = ia.slice(ma, 2, 10)

yields a new VIEW, with rank = mb.rank-1 such that va[i][j] equals ma[i][10][k]. We can also specify an 'end' and 'step' options to slice, such that

v2 = ia.slice(ma, 3, 10, 20, 5)

creates a view with the same rank as mb, but the third dimension goes from 10 to 20 with steps of 5, namely,

v2[i][j][1] == ma[i][j][10] 
v2[i][j][2] == ma[i][j][15]
v2[i][j][3] == ma[i][j][20]

with i=1,mb.dim[1] and j=1,mb.dim[2] and k=1,2,3, and the respective dimensions are updated, then

for i=1,v2.dim[1] do
  for j=1,v2.dim[2] do
    for k=1,v2.dim3[3] do
      v2[i][j][k] = math.random()
    end
  end
end

is also valid. Indexed marrays/views are also 'sliceable', 

v3 = ia.toview(ma[5][10])

is such that

v3[i] == ma[5][10][i]

The views always 'remember' the orinal shape of the marray, then you can reshape your marray 

ia.reshape(ma, 6000)

and v3[i], v2[i][j][k] and v1[i][j] access to the same numbers as before! 

Finally, v3.name == ma.name !

TODO

Index check, I do not check any index.
Mathematical operations on views.
More?


















