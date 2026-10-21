# SPDX-License-Identifier: 0BSD
# Author: Lasse Collin

CC = c99
CFLAGS = -g
LDFLAGS = -llzma

PROGS = \
	01_compress_easy \
	02_decompress \
	03_compress_custom \
	04_compress_easy_mt \
	05_decompress_mt \
	06_buffer_to_buffer \
	07_custom_allocator \
	11_file_info

all: $(PROGS)

.c:
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	-rm -f $(PROGS)
