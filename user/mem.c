#include <stdio.h>
#include <unistd.h>
int main(void) {
    MemInfo m;
    meminfo(&m);
    printf("  frames: %lu free of %lu (%lu MiB free)\n", (unsigned long)m.free_frames, (unsigned long)m.total_frames,
           (unsigned long)(m.free_frames * 4 / 1024));
    printf("  kernel heap: %lu KiB, disk free: %lu KiB, threads: %lu\n", (unsigned long)m.heap_kib,
           (unsigned long)m.disk_free_kib, (unsigned long)m.threads);
    return 0;
}
