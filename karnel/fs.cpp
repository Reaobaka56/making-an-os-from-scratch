#include "fs.hpp"
#include "ata.hpp"
#include "sched.hpp"
#include "irq.hpp"
#include "console.hpp"
#include <string.h>

namespace {
constexpr uint32_t DIR_ENTRIES = 64;

struct Super {
    char     magic[8];       // "RAPTORFS"
    uint32_t version, total_sectors, dir_start, dir_sectors, data_start, next_free;
};
struct DirEnt {
    char     name[48];
    uint32_t start, size, alloc, used;   // start/alloc in sectors
};
static_assert(sizeof(DirEnt) == 64);

Super  sb;
DirEnt dir[DIR_ENTRIES];
bool   mounted;
volatile bool busy;

struct Lock {
    Lock() { for (;;) { { IrqGuard g; if (!busy) { busy = true; return; } } yield(); } }
    ~Lock() { busy = false; }
};

bool flush_meta() {
    uint8_t sec[512] = {};
    memcpy(sec, &sb, sizeof sb);
    return ata_write(0, 1, sec) && ata_write(sb.dir_start, sb.dir_sectors, dir);
}
}  // namespace

bool fs_mount() {
    if (!ata_init()) return false;
    uint8_t sec[512];
    if (!ata_read(0, 1, sec)) return false;
    memcpy(&sb, sec, sizeof sb);
    if (memcmp(sb.magic, "RAPTORFS", 8) != 0 || sb.dir_sectors * 512 != sizeof dir) return false;
    if (!ata_read(sb.dir_start, sb.dir_sectors, dir)) return false;
    mounted = true;
    return true;
}

bool fs_mounted() { return mounted; }
int  fs_capacity() { return DIR_ENTRIES; }
bool fs_used(int i) { return i >= 0 && i < (int)DIR_ENTRIES && dir[i].used; }
uint32_t fs_size(int i) { return fs_used(i) ? dir[i].size : 0; }
const char* fs_name(int i) { return fs_used(i) ? dir[i].name : ""; }

int fs_find(const char* name) {
    for (int i = 0; i < (int)DIR_ENTRIES; i++)
        if (dir[i].used && strncmp(dir[i].name, name, sizeof dir[i].name) == 0) return i;
    return -1;
}

int64_t fs_read(int idx, uint32_t off, void* buf, uint32_t n) {
    if (!fs_used(idx)) return -1;
    Lock lock;
    const DirEnt& e = dir[idx];
    if (off >= e.size) return 0;
    if (n > e.size - off) n = e.size - off;
    auto* out = static_cast<uint8_t*>(buf);
    uint8_t sec[512];
    uint32_t done = 0;
    while (done < n) {
        uint32_t pos = off + done;
        if (!ata_read(e.start + pos / 512, 1, sec)) return -1;
        uint32_t chunk = 512 - pos % 512;
        if (chunk > n - done) chunk = n - done;
        memcpy(out + done, sec + pos % 512, chunk);
        done += chunk;
    }
    return done;
}

bool fs_write(const char* name, const void* data, uint32_t len) {
    size_t nl = strlen(name);
    if (!mounted || nl == 0 || nl >= sizeof dir[0].name) return false;
    Lock lock;
    uint32_t need = (len + 511) / 512;
    int idx = fs_find(name);
    if (idx < 0) {
        for (int i = 0; i < (int)DIR_ENTRIES; i++) if (!dir[i].used) { idx = i; break; }
        if (idx < 0) return false;
        memset(&dir[idx], 0, sizeof dir[idx]);
        strncpy(dir[idx].name, name, sizeof dir[idx].name - 1);
    }
    DirEnt& e = dir[idx];
    if (!e.used || e.alloc < need) {               // needs a new extent at the end of used space
        if (sb.next_free + need > sb.total_sectors) return false;
        e.start = sb.next_free; e.alloc = need;
        sb.next_free += need;
    }
    auto* src = static_cast<const uint8_t*>(data);
    uint8_t sec[512];
    for (uint32_t s = 0; s < need; s++) {
        uint32_t chunk = len - s * 512 < 512 ? len - s * 512 : 512;
        memset(sec, 0, 512);
        memcpy(sec, src + s * 512, chunk);
        if (!ata_write(e.start + s, 1, sec)) return false;
    }
    e.size = len; e.used = 1;
    return flush_meta();
}

bool fs_remove(const char* name) {
    Lock lock;
    int idx = fs_find(name);
    if (idx < 0) return false;
    dir[idx].used = 0;
    return flush_meta();
}

uint32_t fs_free_bytes() { return (sb.total_sectors - sb.next_free) * 512; }
