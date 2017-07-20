#include <stdio.h>
#include <stdlib.h>
#include <Windows.h>

#include "bullet-db.h"

int main(int args_count, char *args[]) {
	bdb *db = bdb_open("contacts.tbl");
	printf("File Size: %lu\n", db->buf_size);

	contact *new_contact = bdb_read(db);
	printf(new_contact->first_name);
	//contact jessica = {"Jessica", "Rust"};
	//bdb_insert(db, jessica);
	//bdb_save("contacts.tbl", db);
}

bdb *bdb_new() {
	bdb *db = malloc(sizeof(*db));
	db->buffer = malloc(256);
	db->buf_size = 256;
	return db;
}

bdb *bdb_open(char *file_name) {
	FILE *db = fopen(file_name, "rb+");
	fseek(db, 0, SEEK_END);
	size_t buf_size = ftell(db);
	rewind(db);

	// TODO: check malloc()'s result & add appropriate error handling
	byte *buffer = malloc(buf_size);
	// TODO: check fread()'s result & add appropriate error handling
	fread(buffer, buf_size, 1, db);
	fclose(db);

	bdb *ret_db = malloc(sizeof(*ret_db));
	ret_db->buffer = buffer;
	ret_db->buf_size = buf_size;
	return ret_db;
}

void bdb_save(char *file_name, bdb *db) {
	FILE *db_file = fopen(file_name, "wb+");
	fwrite(db->buffer, db->buf_size, 1, db_file);
	fclose(db_file);
}

void bdb_insert(bdb *db, contact new_record) {
	size_t jess_size = sizeof(new_record);
	// size_t fname_size = strlen(new_record.first_name) + 1;
	// printf("Sizeof 'Jessica': %lu\n", fname_size);
	// size_t lname_size = strlen(new_record.last_name) + 1;

	db->buf_size += jess_size;// + fname_size + lname_size;
	printf("New buffer Size: %lu\n", db->buf_size);

	db->buffer = realloc(db->buffer, db->buf_size);
	// memcpy(db->buffer, new_record.first_name, fname_size);
	// new_record.first_name = db->buffer;

	// memcpy(db->buffer + fname_size, new_record.last_name, lname_size);
	// new_record.last_name = db->buffer + fname_size;

	memcpy(db->buffer /*+ fname_size + lname_size*/, &new_record, jess_size);
}

contact *bdb_read(bdb *db) {
	contact *new_contact = malloc(sizeof(contact));
	memcpy(new_contact, db->buffer, sizeof(contact));
	return new_contact;
}
