#include "test_utils.h"

namespace test_utils {

// Define static members for NetworkFactorySingleton
std::unique_ptr<INetworkSocketFactory> NetworkFactorySingleton::factory;
INetworkSocketFactory* NetworkFactorySingleton::instance = nullptr;

} // namespace test_utils
