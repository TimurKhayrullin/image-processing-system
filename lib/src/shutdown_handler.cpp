#include "shutdown_handler.hpp"
#include <string>
#include <iostream>
#include <atomic>
#include <csignal>

// Definition of static member
std::atomic<bool> ShutdownHandler::keepRunning(true);

// Install handlers for all relevant signals
void ShutdownHandler::init() {
    std::signal(SIGINT,  handleSignal);   // Ctrl + C
    std::signal(SIGTERM, handleSignal);   // system kill
    std::signal(SIGQUIT, handleSignal);   // Ctrl + backslash
    std::signal(SIGHUP,  handleSignal);   // terminal death or reload
    std::signal(SIGUSR1, handleSignal);   // user hook
    std::signal(SIGUSR2, handleSignal);   // user hook
}

// Returns false when shutdown requested
bool ShutdownHandler::running() {
    return keepRunning.load(std::memory_order_relaxed);
}

void ShutdownHandler::handleSignal(int sig) {
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

    keepRunning.store(false, std::memory_order_relaxed);
}