#g++ pbotest.cpp -std=c++17 -O2 $(sdl2-config --cflags --libs) -lGL -DSTB_IMAGE_IMPLEMENTATION -o pbotest
#echo "Running"
#./pbotest


g++ pbotest.cpp -std=c++17 -g -O0 -fno-omit-frame-pointer $(sdl2-config --cflags --libs) -lGL -DSTB_IMAGE_IMPLEMENTATION -o pbotest



