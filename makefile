CC=gcc
CFLAGS=-I.
DEPS = dberror.h storage_mgr.h test_helper.h buffer_mgr.h buffer_mgr_stat.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

test_assign2: dberror.o test_assign2_1.o storage_mgr.o buffer_mgr.o buffer_mgr_stat.o
	$(CC) -o test_assign2 dberror.o test_assign2_1.o storage_mgr.o buffer_mgr.o buffer_mgr_stat.o