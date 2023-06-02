#include <stdint.h>
#include <unistd.h>
#include <stdexcept>
#include <fmt/format.h>
#include "kerninfra.hpp"
#include <time.h>
#include <sys/time.h>

int _VERBOSE = 0;
#define FLOG(f_, ...)                                                                            \
{                                                                                                 \
    if (_VERBOSE) {                                                                                \
        struct tm _tm123_;                                                                            \
        struct timeval _xxtv123_;                                                                     \
        gettimeofday(&_xxtv123_, NULL);                                                               \
        localtime_r(&_xxtv123_.tv_sec, &_tm123_);                                                     \
        printf("%02d:%02d:%02d.%06d\t", _tm123_.tm_hour, _tm123_.tm_min, _tm123_.tm_sec, _xxtv123_.tv_usec); \
        printf((f_), ##__VA_ARGS__);                                                                  \
        printf("\n");                                                                               \
    }                                                                                              \
};

addr_t proc_of_pid(pid_t pid) {
    addr_t procAddr = kernel_read64(allproc);
    uint64_t current_pid = 0;

    while (procAddr) {
        auto proc = proc_t_p(procAddr);
        current_pid = proc.p_pid().load();
        if (current_pid == pid) return procAddr;
        procAddr = proc.nextproc().load();
        FLOG("proc_of_pid: proc %llx pid %llu\n", procAddr, current_pid);
    }
    throw std::runtime_error(fmt::format("proc_of_pid failed to find proc for {}", pid));
}

addr_t current_proc() {
    addr_t proc = proc_of_pid(getpid());
    //printf("proc: %llx\n", proc);
    FLOG("current_proc: found proc %llx\n", proc);
    return proc;
}


addr_t vnode_from_fd(int fd) {
    auto proc = proc_t_p(current_proc());
    addr_t ofiles = proc.p_fd().fd_ofiles().load();
    FLOG("vnode_from_fd: got ofiles array %llx\n", ofiles);
    auto fp = fileproc_p(kpointer_t<>(ofiles + 8 * fd).load_addr());
    return fp.f_fglob().fg_data().load();
}

addr_t lookup_vm_map_entry(addr_t _vmmap, addr_t va) {
    auto vmmap = _vm_map_t_p(_vmmap);
    int n = vmmap.nentries().load();
    FLOG("lookup_vm_map_entry: Totally %d vm entries\n", n);
    addr_t curEntry = vmmap.link_next().load();
    for (int i = 0; i < n; i++) {
        auto _curEntry = _vm_map_entry_p(curEntry);
        FLOG("lookup_vm_map_entry: VM Entry: %p - %p\n", (void *)_curEntry.start().load(), (void *)_curEntry.end().load());
        if (va >= _curEntry.start().load() && va < _curEntry.end().load()) {
            return curEntry;
        }
        curEntry = _curEntry.link_next().load();
    }
    return 0;
}