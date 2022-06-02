sequencer:
	clang++ -O0 -g sequencer.cpp services/capture_frame.cpp utils/error.c utils/log.c utils/time.c -o sequencer `pkg-config --libs opencv` -L/usr/lib -lopencv_core -lopencv_flann -lopencv_video -lpthread -lrt -lm -Wall

clean:
	rm -f sequencer
