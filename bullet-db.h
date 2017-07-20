#include <stdio.h>
#include <stdlib.h>

typedef struct contact {
	char first_name[10];
	char last_name[10];
} contact;

typedef struct bdb {
	byte *buffer;
	size_t buf_size;
} bdb;

bdb *bdb_new();
bdb *bdb_open(char *file_name);
void bdb_insert(bdb *db, contact new_record);
void bdb_save(char *file_name, bdb *db);
contact *bdb_read(bdb *db);