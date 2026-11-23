#include <atomic>
#include <cstdio>
#include <exception>
#include <pthread.h>

int psprecomp_guest_main(int, char **);
bool native_ui_start();
void native_ui_tick();
void native_ui_stop();

namespace {
struct Invocation {
    int argc;
    char **argv;
    int result{1};
    std::atomic<bool> done{false};
};
void *run_guest(void *opaque) {
    auto &call = *static_cast<Invocation *>(opaque);
    try { call.result = psprecomp_guest_main(call.argc, call.argv); }
    catch (const std::exception &error) { std::fprintf(stderr, "%s\n", error.what()); }
    catch (...) { std::fprintf(stderr, "Unhandled host exception\n"); }
    call.done.store(true, std::memory_order_release);
    return nullptr;
}
}

int main(int argc, char **argv) {
    // Cocoa and SDL video stay on the process main thread. AOT gets a large stack.
    if (!native_ui_start()) return 1;
    Invocation call{argc, argv};
    pthread_attr_t attr;
    if (pthread_attr_init(&attr) != 0) { native_ui_stop(); return 1; }
    int error = pthread_attr_setstacksize(&attr, 64u * 1024u * 1024u);
    pthread_t thread{};
    if (error == 0) error = pthread_create(&thread, &attr, run_guest, &call);
    pthread_attr_destroy(&attr);
    if (error != 0) {
        std::fprintf(stderr, "Cannot start guest thread (%d)\n", error);
        native_ui_stop();
        return 1;
    }
    while (!call.done.load(std::memory_order_acquire)) native_ui_tick();
    pthread_join(thread, nullptr);
    native_ui_stop();
    return call.result;
}
