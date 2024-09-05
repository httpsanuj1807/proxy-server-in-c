#include "map.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "lru_cache.h"


cache_store* hash_table[TABLE_SIZE];

void init_map(){

    for(int i = 0; i < TABLE_SIZE; i++){

        hash_table[i] = NULL;

    }

}

int hash(char* url){

    int url_len = strlen(url);
    unsigned long long hash_value = 0;
    int common_base = 31;

    for(int i = 0; i < url_len; i++){

        hash_value = (hash_value * common_base + (unsigned char) url[i]) % TABLE_SIZE;

    }

    return (int) hash_value;

}


void print_map(){

    

    for(int i = 0; i < TABLE_SIZE; i++){

        if(hash_table[i] == NULL){

            printf("%d\tNULL\n", i);

        }
        else{

            cache_store* temp = hash_table[i];

            printf("%d\t", i);

            while(temp){

                printf("%.35s ", temp -> url);
                temp = temp -> next;

            }

            printf("\n");

        }

    }
    

}

bool insert(cache_element* head){

    if(head == NULL) return false;  // accidental insert
    
    int index = hash(head -> url);

    cache_store* temp = (cache_store*) malloc(sizeof(cache_store));

    if (!temp) {
        printf("Memory allocation failed for cache_store\n");
        return false;
    }

    temp -> url = head -> url;
    temp -> address = head;
    temp -> next = hash_table[index];
    hash_table[index] = temp;

    return true;

}

cache_element* find(char *url){

    int index = hash(url);
    cache_store* temp = hash_table[index];
    
    while(temp != NULL && (strcmp(temp -> url, url) != 0)) temp = temp -> next;

    return (temp != NULL) ? temp -> address : NULL;

}


bool erase(cache_element* obj){

    if (obj == NULL) return false;

    int index = hash(obj -> url);

    if(hash_table[index] != NULL){

        cache_store* temp = hash_table[index];

        if(strcmp(temp -> url, obj -> url) == 0){  // if its head
            hash_table[index] = temp -> next;
            free(temp);
            return true;

        }

        cache_store* prev = temp;
        temp = temp -> next;
        
        while(temp){

            if(strcmp(temp -> url, obj -> url) == 0){

                prev -> next = temp -> next;
                free(temp);
                return true;

            }

            prev = temp;
            temp = temp -> next;

        }

    }

    return false;

}

