#include <stdbool.h>
#define TABLE_SIZE 5

typedef struct cache_element cache_element;

typedef struct cache_store cache_store;

struct cache_store
{
    char* url;
    cache_element* address;
    cache_store* next;

};


void init_map();


// hash function that returns index to store at the hash table
int hash(char* url);

// print the hash table array
void print_map();


// return true after inserting
bool insert(cache_element* head);

// return NULL if not found, else the address of the object found
cache_element* find(char *url);

// return NULL, if not found, if deleted return the address of deleted object
bool erase(cache_element* obj);