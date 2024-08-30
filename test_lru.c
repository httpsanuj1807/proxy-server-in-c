#include <stdio.h>
#include <string.h>
#include "lru_cache.h"


int main(){

    init_cache();

    cache_element obj1 = {.data = "<html>I am response of google.com</html>", .len=4, .url="www.google.com", .next = NULL, .prev = NULL};
    
    
    store_in_cache(&obj1);
    print_cache();

    cache_element obj2 = {.data = "<html>I am response of leetcode.com</html>", .len=4, .url="www.leetcode.com", .next = NULL, .prev = NULL};

    store_in_cache(&obj2);
    print_cache();

    cache_element obj3 = {.data = "<html>I am response of wikipedia.com</html>", .len=4, .url="www.wikipedia/wiki/developer/login", .next = NULL, .prev = NULL};

    store_in_cache(&obj3);
    print_cache();

    cache_element obj4 = {.data = "<html>I am response of myntra.com</html>", .len=4, .url="www.myntra/shop/catalog/shirts", .next = NULL, .prev = NULL};

    store_in_cache(&obj4);
    print_cache();

    cache_element obj5 = {.data = "<html>I am response of amazon.com</html>", .len=4, .url="www.amazon/apis/v2/insertrow", .next = NULL, .prev = NULL};

    store_in_cache(&obj5);
    print_cache();

    cache_element obj6 = {.data = "<html>I am response of chitkara.com</html>", .len=4, .url="www.chitkara/faculty/auth/otp", .next = NULL, .prev = NULL};

    store_in_cache(&obj6);
    print_cache();

    cache_element* res = find_in_cache("www.leetcode.com");

    print_cache();

    res = find_in_cache("www.google.com");

    print_cache();

    return 0;
}