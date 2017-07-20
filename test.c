#include <stdio.h>
#include <stdbool.h>

#define LEN(arr) (sizeof((arr)) / sizeof((arr)[0]))

// globals shared between assert() and the test loop
char* error_msg = NULL;
bool passing = true;
bool assert(bool passed, char* msg) {
    if (!passed) {
        passing = false;
        if (!error_msg)
            error_msg = msg;
    }
    return passed;
}

char* simple_test() {
    assert(6 == 7, "foo != 7");
    return "a simple test";
}

char* (*tests[1])() = {&simple_test};

int main(int args_count, char *args[]) {
    int test_num = 0;
    int num_tests_passed = 0;
    int num_tests_failed = 0;
    int num_tests = LEN(tests);
    printf("1..%i\n", num_tests);
    
    for (int i = 0; i < num_tests; i++) {
        // reset test-specific globals
        passing = true;
        error_msg = NULL;
        test_num++;

        char* test_name = (*tests[i])();
        if (passing) {
            printf("ok %i - %s\n", test_num, test_name);
            num_tests_passed++;
        }
        else {
            printf("not ok %i - %s\n", test_num, test_name);
            printf("# Error: %s\n", error_msg);
        }
    }
    printf("# tests %i\n", num_tests);
    printf("# pass %i\n", num_tests_passed);
    printf("# fail %i\n", num_tests_failed);
}
