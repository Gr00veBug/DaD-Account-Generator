#include "generator.hpp"
#include "overlay.hpp"
#include <iostream>

int main() {
    std::cout << "=== DaD Account Generator ===\n";
    std::cout << "Initializing overlay...\n";
    
    // Create and initialize overlay (API key will be loaded from config or prompted)
    Overlay overlay;
    if (!overlay.initialize("")) {
        std::cout << "Failed to initialize overlay!\n";
        return -1;
    }
    
    std::cout << "Overlay initialized successfully!\n";
    std::cout << "Starting overlay application...\n";
    
    // Run the overlay (this will block until the window is closed)
    overlay.run();
    
    std::cout << "Application closed.\n";
    return 0;
}