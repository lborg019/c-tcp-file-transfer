all:	ftpclient ftpserver

ftpclient: ftpclient.c
	gcc -std=gnu99 -Wall $< -o $@

ftpserver: ftpserver.c
	gcc -std=gnu99 -Wall $< -o $@

clean:
	rm -f ftpclient ftpserver *.o *~ core

