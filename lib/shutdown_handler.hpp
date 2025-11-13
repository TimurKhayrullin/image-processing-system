#pragma once
#include <atomic>
#include <csignal>

class ShutdownHandler {
public:
    static void init();
    static bool running();

private:
    static void handleSignal(int sig);
    static std::atomic<bool> keepRunning;
};