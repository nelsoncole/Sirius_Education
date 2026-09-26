#include <assert.h>
#include <stdio.h>  // você pode substituir por seu próprio printf
#include <stdlib.h> // ou sua função abort()

void __assert_fail(const char *expr, const char *file, int line, const char *func) {
    printf("ASSERTION FAILED: %s\n", expr);
    printf("  in function: %s\n", func);
    printf("  at file: %s:%d\n", file, line);
    
    // Encerrar o programa - substitua se não tiver exit()
    abort();
}
