#pragma once
#include <atomic>
#include <csignal>

class ShutdownHandler {
public:
    static void init();
    static bool running();

private:
    static void handle_signal(int sig);
    static std::atomic<bool> keep_running;
};