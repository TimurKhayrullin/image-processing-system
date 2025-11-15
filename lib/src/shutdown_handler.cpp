#include "shutdown_handler.hpp"
#include <string>
#include <iostream>
#include <atomic>
#include <csignal>

// Definition of static member
std::atomic<bool> ShutdownHandler::keep_running(true);

// Install handlers for all relevant signals
void ShutdownHandler::init() {
    std::signal(SIGINT,  handle_signal);   // Ctrl + C
    std::signal(SIGTERM, handle_signal);   // system kill
    std::signal(SIGQUIT, handle_signal);   // Ctrl + backslash
    std::signal(SIGHUP,  handle_signal);   // terminal death or reload
    std::signal(SIGUSR1, handle_signal);   // user hook
    std::signal(SIGUSR2, handle_signal);   // user hook
}

// Returns false when shutdown requested
bool ShutdownHandler::running() {
    return keep_running.load(std::memory_order_relaxed);
}

void ShutdownHandler::handle_signal(int sig) {
    switch (sig) {
    case SIGINT:
        std::cerr << "\n[ShutdownHandler] Caught SIGINT (Ctrl+C).\n";
        break;
    case SIGTERM:
        std::cerr << "\n[ShutdownHandler] Caught SIGTERM (termination request).\n";
        break;
    case SIGQUIT:
        std::cerr << "\n[ShutdownHandler] Caught SIGQUIT.\n";
        break;
    case SIGHUP:
        std::cerr << "\n[ShutdownHandler] Caught SIGHUP (terminal closed or reload request).\n";
        break;
    case SIGUSR1:
        std::cerr << "\n[ShutdownHandler] Caught SIGUSR1.\n";
        break;
    case SIGUSR2:
        std::cerr << "\n[ShutdownHandler] Caught SIGUSR2.\n";
        break;
    default:
        std::cerr << "\n[ShutdownHandler] Caught unknown signal: " << sig << "\n";
        break;
    }

    keep_running.store(false, std::memory_order_relaxed);
}