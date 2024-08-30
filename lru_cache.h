#include <stdbool.h>

typedef struct cache_element cache_element;

struct cache_element{

     char* data;   // the response from server
     int len;    // size of response in bytes
     char* url;  // requested url
     cache_element* next;
     cache_element* prev;

};

extern int CACHE_CAPACITY;
extern int ELEMENTS_IN_CACHE;

void init_cache();

bool store_in_cache(cache_element* obj);

cache_element* find_in_cache(char* url);

bool relocate_cache(cache_element* obj);

void print_cache();