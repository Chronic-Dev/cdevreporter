all: launcher

launcher: launcher.c
	gcc -m32 -arch i386 -O3 -o $@ $< -framework Security

clean:
	rm -f launcher

