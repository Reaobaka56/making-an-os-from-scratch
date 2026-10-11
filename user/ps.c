#include <stdio.h>
#include <unistd.h>
int main(void) {
    PsInfo p[32];
    int n = ps(p, 32);
    printf("  PID  PPID STATE KIND NAME\n");
    for (int i = 0; i < n; i++)
        printf("  %-4lu %-4lu %-5c %-4s %s\n", (unsigned long)p[i].pid, (unsigned long)p[i].parent, p[i].state,
               p[i].user ? "user" : "kern", p[i].name);
    return 0;
}
