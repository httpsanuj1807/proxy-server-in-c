#include "lru_cache.h"
#include <stdlib.h>
#include "map.h"
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>


cache_element* head = NULL;
cache_element* tail = NULL;
int ELEMENTS_IN_CACHE;
int CACHE_CAPACITY;
pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

void init_cache() {

    printf("\nInitializing cache...\n");
    head = NULL;
    tail = NULL;
    ELEMENTS_IN_CACHE = 0;
    CACHE_CAPACITY = 5;
    init_map();
    print_cache();

}

void insert_at_head(cache_element* obj){

    if(obj == NULL){

        printf("\nCant insert NULL obj");
        return;

    }
    
    obj -> next = head;
    obj -> prev = NULL;

    if(head != NULL){

        head -> prev = obj;

    }

    head = obj;

    // if is the first element in DLL

    if(tail == NULL){

        tail = obj;

    }

    if(!insert(head)) printf("STATUS : FAILED (operation inserting into map)\n");
    else printf("STATUS : SUCCESS (stored in cache)\n");

}

void delete_from_tail(){   

    if(tail == NULL){

        printf("\nNo element available to delete\n");
        return;

    }

    printf("\nCache full %d/%d. Evicting....\n", ELEMENTS_IN_CACHE, CACHE_CAPACITY);
    printf("\nRemoving Least Recently used in cache, URL : %s\n", tail -> url); 

    cache_element* temp = tail;
    tail = tail -> prev;

    if(tail != NULL) tail -> next = NULL;
    else head = NULL;

    if(!erase(temp)){
        printf("STATUS : FAILED (unable to erase in map)\n");
    }
    else{
        printf("STATUS : SUCCESS (eviction successfull)\n");
    }
    
}


bool relocate_cache(cache_element* obj){


    if(obj == NULL){

        printf("NULL object passed to relocate in cache\n");
        return false;

    }

    if(obj == head) return true; // no need to relocate as already at head

    if(obj == tail){

        obj -> prev -> next = NULL;
        tail = obj -> prev;
        

    }
    else{   // middle obj

        obj -> prev -> next = obj -> next;
        obj -> next -> prev = obj -> prev;
        obj -> next = NULL;

    }

    obj -> prev = NULL;
    insert_at_head(obj);
    return true;

}


bool store_in_cache(char* data, int size, char* url){

    pthread_mutex_lock(&cache_mutex);
    printf("Cache Lock acquired\n");
    cache_element* obj = (cache_element*) malloc(sizeof(cache_element));
    if (obj == NULL) {
        printf("Error allocating memory for cache element\n");
        printf("Cache Lock released\n");
        pthread_mutex_unlock(&cache_mutex);
        return 0;
    }
    obj -> data = (char*) malloc((size + 1) *  sizeof(char));
    strcpy(obj -> data, data);
    obj -> url = (char*) malloc((strlen(url) + 1) * sizeof(char));
    strcpy(obj -> url, url);
    obj -> len = size;
    obj -> next = NULL;
    obj -> prev = NULL;

    printf("\nStoring in cache, URL : %s\n", obj -> url);
    
    if(ELEMENTS_IN_CACHE >= CACHE_CAPACITY) delete_from_tail();
    else ELEMENTS_IN_CACHE++;

    insert_at_head(obj);
    pthread_mutex_unlock(&cache_mutex);
    printf("Cache Lock released\n");
    return 1;

}

cache_element* find_in_cache(char* url){
    pthread_mutex_lock(&cache_mutex);
    printf("Cache Lock acquired\n");
    if(strlen(url) == 0){

        printf("URL size not sufficient to find in cache");
        pthread_mutex_unlock(&cache_mutex);
        printf("Cache Lock released\n");
        return NULL;

    }

    // we will not look directly into our doubly linked list in order to reduce complextiy we will do a O(1) operation in hashmap for lookup
    cache_element* temp = find(url); // looking into hashmap, if not in map then return NULL, else the cache element address (in DLL)
    
    if(temp){ // cache hit

        printf("\nCache HIT : %s\n", temp -> url);
        printf("Relocating to the most recent in cache\n");

        if(relocate_cache(temp)){ // relocate to head
            printf("STATUS : SUCCESS (relocated in cache)\n");
        }
        else{
            printf("STATUS :  (unable to relocate in cache)\n");
        }
        pthread_mutex_unlock(&cache_mutex);
        printf("Cache Lock released\n");
        return temp;
    }
    else{
        printf("\nCache MISS : %s\n", url);
    }

    pthread_mutex_unlock(&cache_mutex);
    printf("Cache Lock released\n");
    return NULL;

}

void print_cache(){

    cache_element* temp = head;
    printf("\nCache State: %d/%d\n", ELEMENTS_IN_CACHE, CACHE_CAPACITY);

    if(ELEMENTS_IN_CACHE == 0) {

        printf("Nothing to print, cache currently empty.\n");
        return;

    }
    
    
   
    while(temp){

        if(temp != head) printf("  --->  ");;
        printf("%s", temp -> url);
        temp = temp -> next;

    }

    printf("\n");
    
    printf("\nMap State:\n");
    print_map();

}


