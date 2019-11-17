all:
	clang -Wall -Wextra -Os -I../IMUTracker/IMUTracker Serial.m main.m -std=gnu11 -fobjc-arc -fobjc-weak -fmodules -framework Foundation -o flasher

clean:
	rm flasher
