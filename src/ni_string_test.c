#include <limits.h>
#include <stdio.h>
#include "ni_test.h"

int ni_string_test() {
    {
        ni_string x = ni_string_new("foo"), y;

        test_cond("Create a string and obtain the length",
            ni_string_len(x) == 3 && memcmp(x, "foo\0", 4) == 0)

        ni_string_free(x);
        x = ni_string_new_len("foo", 2);
        test_cond("Create a string with specified length",
            ni_string_len(x) == 2 && memcmp(x, "fo\0", 3) == 0)

        x = ni_string_cat(x, "bar");
        test_cond("Strings concatenation",
            ni_string_len(x) == 5 && memcmp(x, "fobar\0", 6) == 0);

        x = ni_string_cpy(x, "a");
        test_cond("ni_string_cpy() against an originally longer string",
            ni_string_len(x) == 1 && memcmp(x, "a\0", 2) == 0)

        x = ni_string_cpy(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk");
        test_cond("ni_string_cpy() against an originally shorter string",
            ni_string_len(x) == 33 &&
            memcmp(x, "xyzxxxxxxxxxxyyyyyyyyyykkkkkkkkkk\0", 33) == 0)

        ni_string_free(x);
        x = ni_string_cat_printf(ni_string_empty(), "%d", 123);
        test_cond("ni_string_cat_printf() seems working in the base case",
            ni_string_len(x) == 3 && memcmp(x, "123\0", 4) == 0)

        ni_string_free(x);
        x = ni_string_new("--");
        x = ni_string_cat_fmt(x, "Hello %s World %I,%I--", "Hi!", LLONG_MIN, LLONG_MAX);
        test_cond("ni_string_cat_fmt() seems working in the base case",
            ni_string_len(x) == 60 &&
            memcmp(x, "--Hello Hi! World -9223372036854775808,"
                "9223372036854775807--", 60) == 0)
        printf("[%s]\n", x);

        ni_string_free(x);
        x = ni_string_new("--");
        x = ni_string_cat_fmt(x, "%u,%U--", UINT_MAX, ULLONG_MAX);
        test_cond("ni_string_cat_fmt() seems working with unsigned numbers",
            ni_string_len(x) == 35 &&
            memcmp(x, "--4294967295,18446744073709551615--", 35) == 0)

        ni_string_free(x);
        x = ni_string_new(" x ");
        ni_string_trim(x, " x");
        test_cond("ni_string_trim() works when all chars match",
            ni_string_len(x) == 0)

        ni_string_free(x);
        x = ni_string_new(" x ");
        ni_string_trim(x, " ");
        test_cond("ni_string_trim() works when a single char remains",
            ni_string_len(x) == 1 && x[0] == 'x')

        ni_string_free(x);
        x = ni_string_new("xxciaoyyy");
        ni_string_trim(x, "xy");
        test_cond("ni_string_trim() correctly trims characters",
            ni_string_len(x) == 4 && memcmp(x, "ciao\0", 5) == 0)

        y = ni_string_dup(x);
        ni_string_range(y, 1, 1);
        test_cond("ni_string_range(...,1,1)",
            ni_string_len(y) == 1 && memcmp(y, "i\0", 2) == 0)

        ni_string_free(y);
        y = ni_string_dup(x);
        ni_string_range(y, 1, -1);
        test_cond("ni_string_range(...,1,-1)",
            ni_string_len(y) == 3 && memcmp(y, "iao\0", 4) == 0)

        ni_string_free(y);
        y = ni_string_dup(x);
        ni_string_range(y, -2, -1);
        test_cond("ni_string_range(...,-2,-1)",
            ni_string_len(y) == 2 && memcmp(y, "ao\0", 3) == 0)

        ni_string_free(y);
        y = ni_string_dup(x);
        ni_string_range(y, 2, 1);
        test_cond("ni_string_range(...,2,1)",
            ni_string_len(y) == 0 && memcmp(y, "\0", 1) == 0)

        ni_string_free(y);
        y = ni_string_dup(x);
        ni_string_range(y, 1, 100);
        test_cond("ni_string_range(...,1,100)",
            ni_string_len(y) == 3 && memcmp(y, "iao\0", 4) == 0)

        ni_string_free(y);
        y = ni_string_dup(x);
        ni_string_range(y, 100, 100);
        test_cond("ni_string_range(...,100,100)",
            ni_string_len(y) == 0 && memcmp(y, "\0", 1) == 0)

        ni_string_free(y);
        ni_string_free(x);
        x = ni_string_new("foo");
        y = ni_string_new("foa");
        test_cond("ni_string_cmp(foo, foa)", ni_string_cmp(x, y) > 0)

        ni_string_free(y);
        ni_string_free(x);
        x = ni_string_new("bar");
        y = ni_string_new("bar");
        test_cond("ni_string_cmp(bar, bar)", ni_string_cmp(x, y) == 0)

        ni_string_free(y);
        ni_string_free(x);
        x = ni_string_new("aar");
        y = ni_string_new("bar");
        test_cond("ni_string_cmp(bar, bar)", ni_string_cmp(x, y) < 0)

        ni_string_free(y);
        ni_string_free(x);
        x = ni_string_new_len("\a\n\0foo\r", 7);
        y = ni_string_cat_repr(ni_string_empty(), x, ni_string_len(x));
        test_cond("ni_string_cat_repr(...data...)",
            memcmp(y, "\"\\a\\n\\x00foo\\r\"", 15) == 0)

        {
            unsigned int oldfree;
            char *p;
            int step = 10, j, i;

            ni_string_free(x);
            ni_string_free(y);
            x = ni_string_new("0");
            test_cond("ni_string_new() free/len buffers", ni_string_len(x) == 1 && ni_string_avail(x) == 0);

            /* Run the test a few times in order to hit the first two
             * ni_string header types. */
            for (i = 0; i < 10; i++) {
                int oldlen = ni_string_len(x);
                x = ni_string_make_room_for(x, step);
                int type = x[-1] & NI_STRING_TYPE_MASK;

                test_cond("ni_string_make_room_for() len", ni_string_len(x) == oldlen);
                if (type != NI_STRING_TYPE_5) {
                    test_cond("ni_string_make_room_for() free", ni_string_avail(x) >= step);
                    oldfree = ni_string_avail(x);
                }
                p = x + oldlen;
                for (j = 0; j < step; j++) {
                    p[j] = 'A' + j;
                }
                ni_string_incr_len(x, step);
            }
            test_cond("ni_string_make_room_for() content",
                memcmp("0ABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJABCDEFGHIJ", x, 101) == 0);
            test_cond("ni_string_make_room_for() final length", ni_string_len(x) == 101);

            ni_string_free(x);
        }
    }
    test_report()
    return 0;
}
