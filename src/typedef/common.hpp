#pragma once

typedef struct _STR_TABLE_SINGLE {
	unsigned long    id;
	const char   *str1;
	const char   *str2;
} STR_TABLE_SINGLE;

typedef struct _STR_TABLE_ID1_ID2 {
	unsigned long    id1;
	unsigned long    id2;
	const char   *str1;
} STR_TABLE_ID1_ID2;


const char *findTableIDSingle1(const STR_TABLE_SINGLE *t, const unsigned long id);
const char *findTableIDSingle2(const STR_TABLE_SINGLE *t, const unsigned long id);

