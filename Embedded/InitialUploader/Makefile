all:
	clang -Wall -Wextra -Os -I../IMUTracker/IMUTracker -I../Flasher ../Flasher/Serial.m main.m -std=gnu11 -fobjc-arc -fobjc-weak -fmodules -framework Foundation -o initial_uploader

clean:
	rm initial_uploader
