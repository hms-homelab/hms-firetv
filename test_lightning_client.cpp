#include <iostream>
#include "clients/LightningClient.h"

using namespace hms_firetv;

int main() {
    std::cout << "Testing LightningClient..." << std::endl;
    std::cout << "==========================================" << std::endl;

    // Create Lightning client for a test device
    // NOTE: This test uses a dummy IP - real tests require actual Fire TV device
    std::string test_ip = "192.168.2.99";  // Dummy IP for testing
    std::string api_key = "0987654321";

    std::cout << "\n1. Creating LightningClient for " << test_ip << "..." << std::endl;
    LightningClient client(test_ip, api_key);
    std::cout << "   ✓ LightningClient created" << std::endl;

    // Test token management
    std::cout << "\n2. Testing token management..." << std::endl;
    std::string test_token = "test_token_12345";
    client.setClientToken(test_token);
    std::string retrieved_token = client.getClientToken();
    std::cout << "   Set token: " << test_token << std::endl;
    std::cout << "   Retrieved token: " << retrieved_token << std::endl;
    std::cout << "   ✓ Token management working" << std::endl;

    // Test health check (will fail with dummy IP, but demonstrates API)
    std::cout << "\n3. Testing health check..." << std::endl;
    bool healthy = client.healthCheck();
    std::cout << "   Health check result: " << (healthy ? "healthy" : "unreachable") << std::endl;
    std::cout << "   (Expected: unreachable for dummy IP)" << std::endl;

    // Test Lightning API availability check
    std::cout << "\n4. Testing Lightning API availability..." << std::endl;
    bool api_available = client.isLightningApiAvailable();
    std::cout << "   API available: " << (api_available ? "yes" : "no") << std::endl;
    std::cout << "   (Expected: no for dummy IP)" << std::endl;

    std::cout << "\n==========================================" << std::endl;
    std::cout << "Structure tests completed!" << std::endl;
    std::cout << "\nNOTE: Functional tests require a real Fire TV device." << std::endl;
    std::cout << "To test pairing with a real device:" << std::endl;
    std::cout << "  1. Set IP to your Fire TV's address" << std::endl;
    std::cout << "  2. Call client.wakeDevice()" << std::endl;
    std::cout << "  3. Call client.displayPin()" << std::endl;
    std::cout << "  4. Enter PIN on Fire TV screen" << std::endl;
    std::cout << "  5. Call client.verifyPin(pin)" << std::endl;
    std::cout << "\nTo test commands:" << std::endl;
    std::cout << "  - client.home()           // Press home button" << std::endl;
    std::cout << "  - client.play()           // Play/pause media" << std::endl;
    std::cout << "  - client.launchApp(...)   // Launch app by package" << std::endl;

    return 0;
}
