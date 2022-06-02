sequencer:
	clang++ -O0 -g --std=c++17 sequencer.cpp services/*.cpp utils/error.c utils/log.c utils/time.c -o sequencer `pkg-config --libs opencv` -L/usr/lib -lopencv_core -lopencv_flann -lopencv_video -lpthread -lrt -lm -lstdc++fs -Wall

clean:
	rm -f sequencer
