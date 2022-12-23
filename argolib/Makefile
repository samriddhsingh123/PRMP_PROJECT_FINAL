
PCMROOT=/mnt/hdd2/home/yuvraj2022/prmp/PRMP_Project/pmu/pcm-202210
argobots_path=/mnt/hdd2/home/yuvraj2022/argo-install
argolib_path=/mnt/hdd2/home/yuvraj2022/prmp/PRMP_Project/argolib
# LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$/mnt/hdd2/home/yuvraj2022/prmp/PRMP_Project/pmu/pcm-202210/build/lib
# argobots_path=/home/sam/Desktop/argo
# argolib_path=/home/sam/Desktop/project/PRMP_Project/argolib
path2=-I$(PCMROOT)/src -L$(PCMROOT)/build/lib 
path=-L$(argobots_path)/lib -I$(argobots_path)/include 
# path=-L/home/sam/Desktop/argo/lib -I/home/sam/Desktop/argo/include
all: prog libargo.so prog1 shared_c shared_cpp itr_avg 


prog: prog.c argolib_dec.o 
	gcc -O3 $(path) $(path2)-o $@ $^ -labt -lpcm -lpthread
	
argolib_dec.o: argolib_dec.c argolib.h 
	gcc -O3 -c $(path) $(path2) argolib_dec.c -lpcm -lpthread

libargo.so: argolib_dec.c argolib.h argolib_dec.hpp
	clang -Wall -g $(path) $(path2) -fPIC -shared -o $@ argolib_dec.c -lc -labt -lpcm -lpthread

shared_cpp: prog1.cpp
	g++ -std=c++17 -O3 $(path) $(path2) -o $@ $^ -L/$(argolib_path) -largo -labt -lpcm -lpthread

shared_c: prog.c
	gcc -O3 $(path) $(path2) -o $@ $^ -L/$(argolib_path) -largo -labt -lpcm -lpthread

prog1: prog1.cpp argolib_dec.hpp
	g++ -std=c++17 $(path) $(path2) -o fib prog1.cpp -labt -lpcm -lpthread

itr_avg: itr_avg.c
	gcc -O3 $(path) $(path2) -o $@ $^ -L/$(argolib_path) -largo -labt -lpcm -lpthread

clean: 
	rm *.o *.so prog fib shared_c shared_cpp itr_avg

