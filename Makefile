all: myhttpd

myhttpd: myhttpd.c
	gcc -W -Wall -lpthread -o myhttpd myhttpd.c

clean:
	rm myhttpd
