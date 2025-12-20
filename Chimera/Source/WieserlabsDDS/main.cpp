#include "wieserlabsdds.hpp"
#include <iostream>

int main() {
    try {
        // 🔧 Set this to your FlexDDS IP
        const std::string ip = "192.168.105.5";

        std::cout << "Connecting to FlexDDS at " << ip << "..." << std::endl;

        // Create DDS client
        WieserlabsClient dds(ip, 10.0);

        // ========== SINGLE TONE TEST ==========
        std::cout << "Setting single tone at 85 MHz..." << std::endl;

        dds.singleToneNow(
            0,      // slot index
            0,      // channel
            85e6,   // frequency (Hz)
            0.5,    // amplitude (0..1)
            0.0     // phase (degrees)
        );

        std::cout << "Tone set. Exiting program." << std::endl;
    }
    catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
    }

    return 0;
}
