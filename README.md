# image_scale
scale rgb\NV21 img with low memory

scale img to jpeg with libjpeg_turbo.

use: g++ -g -c test_low_mem.cpp scale.c cpu_id.c -I[libjpeg include file path]
     g++ test_low_mem.o scale.o cpu_id.o -L[libjpeg lib file path] -ljpeg -o test

