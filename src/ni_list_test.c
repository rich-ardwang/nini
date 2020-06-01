#include <stdio.h>
#include "ni_test.h"

typedef struct ni_person {
    int     ni_age;
    char    ni_name[30];
    char    ni_male;
} ni_person;

int ni_list_test() {
    //create person list
    ni_list *lst = NULL;
    printf("person list used memory: %zu\n", ni_malloc_used_memory());
    lst = ni_list_create();
    if (!lst) return -1;
    lstSetFreeMethod(lst, ni_free);

    //insert person node value
    ni_person *person = NULL;
    for (int i = 0; i < 20000000; i++) {
        person = (ni_person *)ni_malloc(sizeof(ni_person));
        person->ni_age = i;
        strcpy(person->ni_name, "Richard Wang");
        person->ni_male = 0;
        if (person)
            lst = ni_list_add_node_tail(lst, (void *)person);
    }

    //output to stdin
    ni_list_iter *lst_iter = ni_list_get_iterator(lst, AL_START_HEAD);
    size_t lst_len = lstLen(lst);
    printf("person list size: %d.\n", lst_len);
    ni_list_node *nd = NULL;
    for (int i = 0; i < lst_len; i++) {
        nd = ni_list_next(lst_iter);
        if (nd)
            printf("Person:[Name:%s, Age:%d, male:%d].\n",
            ((ni_person *)nd->value)->ni_name,
                ((ni_person *)nd->value)->ni_age,
                ((ni_person *)nd->value)->ni_male);
    }

    //release
    printf("person list used memory: %zu\n", ni_malloc_used_memory());
    ni_list_empty(lst);
    printf("person list used memory: %zu\n", ni_malloc_used_memory());
    ni_list_release(lst);
    printf("person list used memory: %zu\n", ni_malloc_used_memory());
    ni_list_release_iterator(lst_iter);
    printf("person list used memory: %zu\n", ni_malloc_used_memory());
    return 0;
}
