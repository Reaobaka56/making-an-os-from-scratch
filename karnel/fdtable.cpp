#include "fdtable.hpp"
#include "fs.hpp"
#include "heap.hpp"
#include "irq.hpp"
#include <string.h>

constexpr uint32_t PIPE_SIZE = 4096;
constexpr uint32_t WFILE_MAX = 1u << 20;

struct Pipe { uint8_t buf[PIPE_SIZE]; uint32_t head, count; int readers, writers; };
struct WFile { char name[48]; uint8_t* buf; uint32_t len, cap; int refs; };

void fd_init_std(Thread* t) {
    t->files[0].kind = FD_CONSOLE_IN;
    t->files[1].kind = FD_CONSOLE_OUT;
    t->files[2].kind = FD_CONSOLE_OUT;
}

int fd_alloc(Thread* t) {
    for (int i = 0; i < MAX_FDS; i++) if (t->files[i].kind == FD_NONE) return i;
    return -1;
}

void fd_dup_into(OpenFile& dst, const OpenFile& src) {
    IrqGuard g;
    dst = src;
    if (src.kind == FD_PIPE_R) src.pipe->readers++;
    else if (src.kind == FD_PIPE_W) src.pipe->writers++;
    else if (src.kind == FD_WFILE) src.wfile->refs++;
}

bool fd_release(OpenFile& f, bool can_block) {
    bool ok = true;
    WFile* commit = nullptr;
    {
        IrqGuard g;
        switch (f.kind) {
        case FD_PIPE_R: case FD_PIPE_W: {
            Pipe* p = f.pipe;
            if (f.kind == FD_PIPE_R) p->readers--; else p->writers--;
            if (p->readers == 0 && p->writers == 0) kfree(p);
            break;
        }
        case FD_WFILE:
            if (--f.wfile->refs == 0) commit = f.wfile;
            break;
        default: break;
        }
        memset(&f, 0, sizeof f);
    }
    if (commit) {
        if (can_block) ok = fs_write(commit->name, commit->buf, commit->len);   // last close commits the file
        kfree(commit->buf);
        kfree(commit);
    }
    return ok;
}

void fd_close_all(Thread* t, bool can_block) {
    for (auto& f : t->files) if (f.kind != FD_NONE) fd_release(f, can_block);
}

Pipe* pipe_create() {
    auto* p = static_cast<Pipe*>(kmalloc(sizeof(Pipe)));
    if (!p) return nullptr;
    memset(p, 0, sizeof *p);
    p->readers = p->writers = 1;
    return p;
}

int64_t pipe_read(Pipe* p, void* buf, uint64_t n) {
    auto* out = static_cast<uint8_t*>(buf);
    for (;;) {
        {
            IrqGuard g;
            if (p->count) {
                uint64_t k = n < p->count ? n : p->count;
                for (uint64_t i = 0; i < k; i++) out[i] = p->buf[(p->head + i) % PIPE_SIZE];
                p->head = (p->head + k) % PIPE_SIZE;
                p->count -= k;
                return k;
            }
            if (p->writers == 0) return 0;                    // EOF
        }
        yield();
    }
}

int64_t pipe_write(Pipe* p, const void* buf, uint64_t n) {
    auto* in = static_cast<const uint8_t*>(buf);
    uint64_t done = 0;
    while (done < n) {
        {
            IrqGuard g;
            if (p->readers == 0) return -1;                   // nobody will ever read this
            uint64_t space = PIPE_SIZE - p->count;
            if (space) {
                uint64_t k = n - done < space ? n - done : space;
                for (uint64_t i = 0; i < k; i++) p->buf[(p->head + p->count + i) % PIPE_SIZE] = in[done + i];
                p->count += k;
                done += k;
                continue;
            }
        }
        yield();                                              // pipe full: let the reader run
    }
    return done;
}

WFile* wfile_create(const char* name) {
    auto* f = static_cast<WFile*>(kmalloc(sizeof(WFile)));
    if (!f) return nullptr;
    memset(f, 0, sizeof *f);
    strncpy(f->name, name, sizeof f->name - 1);
    f->cap = 512;
    f->buf = static_cast<uint8_t*>(kmalloc(f->cap));
    if (!f->buf) { kfree(f); return nullptr; }
    f->refs = 1;
    return f;
}

int64_t wfile_write(WFile* f, const void* buf, uint64_t n) {
    if (f->len + n > WFILE_MAX) return -1;
    if (f->len + n > f->cap) {
        uint32_t cap = f->cap;
        while (cap < f->len + n) cap *= 2;
        auto* nb = static_cast<uint8_t*>(kmalloc(cap));
        if (!nb) return -1;
        memcpy(nb, f->buf, f->len);
        kfree(f->buf);
        f->buf = nb; f->cap = cap;
    }
    memcpy(f->buf + f->len, buf, n);
    f->len += n;
    return n;
}
