CPPFLAGS=-I/usr/local/include
LDFLAGS=-L/usr/local/lib

rpd:	rpd.c
	$(CC) rpd.c -o rpd $(CPPFLAGS) $(LDFLAGS) -lcurl `mysql_config --cflags --libs`
