g++ pbotest.cpp -std=c++17 -O2 -Wall $(sdl2-config --cflags --libs) -lGL -DSTB_IMAGE_IMPLEMENTATION -o pbotest
./pbotest
