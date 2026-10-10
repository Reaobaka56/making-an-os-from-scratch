#include "sys.h"

void _start(void) {
    print("Hello from userspace! pid=");
    printnum(sys_getpid());
    print("\n");

    i64 fd = sys_open("/etc/motd");
    if (fd >= 0) {
        char buf[128];
        i64 n;
        while ((n = sys_read(fd, buf, sizeof buf)) > 0) sys_write(1, buf, n);
        sys_close(fd);
    } else {
        print("motd missing\n");
    }

    i64 bad = sys_write(1, (const char*)0x1234, 5);   // kernel must reject this, not crash
    print("write to a bad pointer returned ");
    printnum(bad);
    print("\n");

    for (int i = 0; i < 3; i++) {
        print("uptime ms: ");
        printnum(sys_uptime());
        print("\n");
        sys_sleep(50);
    }
    sys_exit(42);
}
