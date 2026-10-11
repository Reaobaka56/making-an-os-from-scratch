#!/usr/bin/env python3
"""End-to-end tests: boot RaptorOS headless in QEMU and drive it over serial / the QEMU monitor.

usage: tests/run_tests.py [--iso build/os.iso] [--disk build/disk.img] [-k SUBSTRING] [-v]

Each test gets a fresh copy of the disk image, so tests never touch your own build/disk.img.
Exit status is the number of failed tests.
"""
import argparse, os, re, shutil, socket, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PROMPT = "os$ "


class Vm:
    def __init__(self, iso, disk, verbose=False):
        self.dir = tempfile.mkdtemp(prefix="raptor-test-")
        self.disk = os.path.join(self.dir, "disk.img")
        shutil.copy(disk, self.disk)
        self.iso, self.verbose = iso, verbose
        self.buf = ""
        self.proc = None

    # -- lifecycle ------------------------------------------------------------------------------
    def start(self):
        ser, mon = os.path.join(self.dir, "ser.sock"), os.path.join(self.dir, "mon.sock")
        self.proc = subprocess.Popen(
            ["qemu-system-x86_64", "-m", "256M", "-display", "none", "-no-reboot",
             "-drive", f"file={self.disk},format=raw,if=ide", "-cdrom", self.iso, "-boot", "d",
             "-serial", f"unix:{ser},server=on,wait=off", "-monitor", f"unix:{mon},server=on,wait=off"],
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        self.ser = self._connect(ser)
        self.mon = self._connect(mon)
        self.buf = ""

    def _connect(self, path):
        for _ in range(100):
            try:
                s = socket.socket(socket.AF_UNIX)
                s.connect(path)
                s.settimeout(0.2)
                return s
            except (FileNotFoundError, ConnectionRefusedError):
                time.sleep(0.1)
        raise RuntimeError("could not connect to QEMU")

    def stop(self):
        if self.proc:
            self.proc.kill()
            self.proc.wait()
            self.proc = None

    def cleanup(self):
        self.stop()
        shutil.rmtree(self.dir, ignore_errors=True)

    # -- I/O ------------------------------------------------------------------------------------
    def _pump(self):
        try:
            d = self.ser.recv(4096)
            if d:
                self.buf += d.decode(errors="replace")
        except socket.timeout:
            pass

    def wait_for(self, text, timeout=30, start=0):
        end = time.time() + timeout
        while time.time() < end:
            i = self.buf.find(text, start)
            if i >= 0:
                return i
            self._pump()
        raise TimeoutError(f"timed out waiting for {text!r}; last output:\n{self.buf[-600:]}")

    def boot(self, timeout=60):
        self.start()
        self.wait_for(PROMPT, timeout)

    def run(self, cmd, timeout=30):
        """Type a command at the shell, wait for the next prompt, return the command's output."""
        start = len(self.buf)
        self.ser.send((cmd + "\r").encode())
        i = self.wait_for(PROMPT, timeout, start + len(cmd))
        out = self.buf[start:i]
        return out.split("\n", 1)[1] if "\n" in out else ""

    def sendkeys(self, keys):
        for k in keys:
            self.mon.send(f"sendkey {k}\n".encode())
            time.sleep(0.12)


# -- tests --------------------------------------------------------------------------------------
TESTS = []


def test(fn):
    TESTS.append(fn)
    return fn


@test
def boot_log(vm):
    """kernel boot: allocator/heap self-test, APIC timer, filesystem, first user process"""
    vm.boot()
    log = vm.buf
    assert "[FAIL]" not in log, "a kernel self-test failed"
    for needle in ["[PASS] heap fully coalesced after free", "apic: id=", "interrupts, scheduler and syscalls ready",
                   "RaptorFS mounted", "[A] step 2", "[B] step 2", "Hello from userspace!",
                   "write to a bad pointer returned -1", "0 frames leaked"]:
        assert needle in log, f"missing in boot log: {needle!r}"
    assert "exited with code 42" in log, "hello should exit with 42"
    assert "[hello] [from] [the] [kernel]" in log, "argv was not passed to the first process"


@test
def shell_basics(vm):
    """shell: ls, cat, echo, unknown command, program exit status"""
    vm.boot()
    out = vm.run("ls")
    for f in ["/bin/hello", "/bin/sh", "/etc/motd", "/README.txt"]:
        assert f in out, f"ls does not list {f}"
    assert "RaptorFS disk image" in vm.run("cat /README.txt")
    assert vm.run("echo one two  three").strip() == "one two three"
    assert "command not found" in vm.run("nosuchprogram")
    out = vm.run("hello a b")
    assert "[hello] [a] [b]" in out and "[exit 42]" in out, out


@test
def userspace_selftest(vm):
    """tests/user/selftest.c: libc, malloc, brk/mmap, fork/exec/wait, files"""
    vm.boot()
    out = vm.run("selftest", timeout=120)
    assert "[FAIL]" not in out, "\n".join(l for l in out.splitlines() if "FAIL" in l)
    assert "SELFTEST DONE failures=0" in out, out[-500:]
    assert out.count("[PASS]") >= 47, f"only {out.count('[PASS]')} checks ran"


@test
def fault_isolation(vm):
    """a crashing user process is killed; the kernel and shell keep running"""
    vm.boot()
    out = vm.run("fault")
    assert "EXCEPTION 14: Page fault" in out and "process killed" in out, out
    assert "[exit -1]" in out
    assert vm.run("echo still alive").strip() == "still alive"


@test
def no_leaks(vm):
    """frames are returned after processes run, fork, exec, pipelines and crashes"""
    vm.boot()

    def free_frames():
        m = re.search(r"frames: (\d+) free", vm.run("mem"))
        return int(m.group(1))

    before = free_frames()
    vm.run("selftest", timeout=120)
    vm.run("fault")
    vm.run("hello")
    vm.run("ls | grep bin | wc")
    vm.run("echo leak check > /leak.tmp")
    vm.run("rm /leak.tmp")
    after = free_frames()
    assert before == after, f"leaked {before - after} frames"


@test
def background_jobs(vm):
    """commands ending in & run in the background and show up in ps"""
    vm.boot()
    assert "started" in vm.run("echo bg &")
    out = vm.run("ps")
    assert "idle" in out and "init" in out and "sh" in out, out


@test
def pipes_and_redirects(vm):
    """shell pipelines (a | b | c) and redirection (< file, > file)"""
    vm.boot()
    assert vm.run("echo hello world | wc").strip() == "1 2 12"
    out = vm.run("ls | grep bin")
    assert "/bin/sh" in out and "/etc/motd" not in out, out
    assert vm.run("ls | grep /bin/ | wc").strip().split()[0] == str(len(re.findall(r"/bin/", vm.run("ls"))))
    assert vm.run("echo a | cat | cat | cat").strip() == "a"
    vm.run("echo redirected text > /out.txt")
    assert vm.run("cat /out.txt").strip() == "redirected text"
    assert vm.run("cat < /out.txt | wc").strip() == "1 2 16"
    assert vm.run("grep README < /README.txt | wc").strip().startswith("1 ")
    assert "[exit 1]" in vm.run("echo nothing | grep zzz")
    assert "command not found" in vm.run("echo x | nosuchprogram")
    assert "syntax error" in vm.run("echo x |")
    assert "cannot open" in vm.run("cat < /does-not-exist")
    vm.run("rm /out.txt")


@test
def timer_advances(vm):
    """the LAPIC timer drives uptime"""
    vm.boot()
    a = int(re.search(r"(\d+) ms", vm.run("uptime")).group(1))
    time.sleep(1.0)
    b = int(re.search(r"(\d+) ms", vm.run("uptime")).group(1))
    assert 500 < b - a < 5000, f"uptime moved {b - a} ms in ~1 s"


@test
def ps2_keyboard(vm):
    """PS/2 keyboard IRQ -> shell (typed through the QEMU monitor)"""
    vm.boot()
    start = len(vm.buf)
    vm.sendkeys(["e", "c", "h", "o", "spc", "k", "b", "d", "ret"])
    vm.wait_for(PROMPT, 20, start + 8)
    seg = vm.buf[start:]
    assert "echo kbd" in seg and "\nkbd\n" in seg, seg


@test
def disk_persistence(vm):
    """files written in one boot are still there after a reboot"""
    vm.boot()
    vm.run("write /persist.txt remember this")
    assert "remember this" in vm.run("cat /persist.txt")
    vm.stop()
    vm.start()
    vm.wait_for(PROMPT, 60)
    assert "remember this" in vm.run("cat /persist.txt"), "file lost across reboot"
    assert "persist.txt" in vm.run("ls")
    vm.run("rm /persist.txt")
    assert "no such file" in vm.run("cat /persist.txt")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--iso", default=os.path.join(ROOT, "build/os.iso"))
    ap.add_argument("--disk", default=os.path.join(ROOT, "build/disk.img"))
    ap.add_argument("-k", default="", help="only run tests whose name contains this")
    ap.add_argument("-v", action="store_true", help="print the serial log of failing tests")
    args = ap.parse_args()

    for path in (args.iso, args.disk):
        if not os.path.exists(path):
            sys.exit(f"missing {path} - run `make iso disk` first")
    if not shutil.which("qemu-system-x86_64"):
        sys.exit("qemu-system-x86_64 not found")

    failed = 0
    selected = [t for t in TESTS if args.k in t.__name__]
    for t in selected:
        vm = Vm(args.iso, args.disk, args.v)
        t0 = time.time()
        try:
            t(vm)
            status = "PASS"
        except Exception as e:  # noqa: BLE001 - report any failure, keep going
            status = "FAIL"
            failed += 1
            detail = f"{type(e).__name__}: {e}"
        print(f"[{status}] {t.__name__:<20} {t.__doc__.splitlines()[0] if t.__doc__ else ''} ({time.time() - t0:.1f}s)", flush=True)
        if status == "FAIL":
            print("       " + detail.replace("\n", "\n       "))
            if args.v:
                print("       --- serial log ---\n       " + vm.buf.replace("\n", "\n       "))
        vm.cleanup()
    print(f"\n{len(selected) - failed}/{len(selected)} tests passed")
    sys.exit(failed)


main()
