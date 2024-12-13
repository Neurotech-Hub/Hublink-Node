#ifndef HUBLINK_BLE_H_
#define HUBLINK_BLE_H_

#include "../../src/ble/BLEDevice.h"
#include "../../src/ble/BLEServer.h"
#include "BLEUtils.h"
#include "../../src/ble/BLE2902.h"

namespace Hublink
{
    static int serverCount = 0;
    static int serviceCount = 0;
    static int characteristicCount = 0;
    static int descriptorCount = 0;

    struct MemoryStats
    {
        static void print(const char *label)
        {
            Serial.printf("MEM [%s] Free: %d, Min Free: %d, Largest Block: %d\n",
                          label,
                          ESP.getFreeHeap(),
                          ESP.getMinFreeHeap(),
                          ESP.getMaxAllocHeap());
            delay(50);
        }
    };

    class BLE2902 : public ::BLE2902
    {
    public:
        BLE2902() : ::BLE2902() {}
    };

    class BLECharacteristic
    {
    private:
        ::BLECharacteristic *m_pCharacteristic;
        BLE2902 *m_descriptor = nullptr; // Optional descriptor, only created when needed
        bool m_hasDescriptor = false;

    public:
        static const uint32_t PROPERTY_READ = ::BLECharacteristic::PROPERTY_READ;
        static const uint32_t PROPERTY_WRITE = ::BLECharacteristic::PROPERTY_WRITE;
        static const uint32_t PROPERTY_NOTIFY = ::BLECharacteristic::PROPERTY_NOTIFY;
        static const uint32_t PROPERTY_INDICATE = ::BLECharacteristic::PROPERTY_INDICATE;

        BLECharacteristic() : m_pCharacteristic(nullptr), m_hasDescriptor(false) {}

        BLECharacteristic(::BLECharacteristic *characteristic) : m_pCharacteristic(characteristic), m_hasDescriptor(false)
        {
            characteristicCount++;
            MemoryStats::print("Characteristic Constructor");
        }

        void addDescriptor()
        {
            MemoryStats::print("Before Add Descriptor");
            if (m_pCharacteristic && !m_hasDescriptor)
            {
                m_descriptor = new BLE2902(); // Create only when needed
                descriptorCount++;
                m_pCharacteristic->addDescriptor(m_descriptor);
                m_hasDescriptor = true;
            }
            MemoryStats::print("After Add Descriptor");
        }

        operator ::BLECharacteristic *() { return m_pCharacteristic; }

        void releaseDescriptor()
        {
            if (m_hasDescriptor)
            {
                m_hasDescriptor = false;
                descriptorCount--;
            }
        }

        ~BLECharacteristic()
        {
            if (m_hasDescriptor)
            {
                descriptorCount--;
            }
            characteristicCount--;
        }
    };

    class BLEService
    {
    private:
        ::BLEService *m_pService;
        static BLECharacteristic m_characteristics[4]; // Static array
        static int m_charCount;

    public:
        BLEService(::BLEService *service) : m_pService(service)
        {
            serviceCount++;
            m_charCount = 0;
        }

        void start()
        {
            if (m_pService)
            {
                m_pService->start();
            }
        }

        void stop()
        {
            if (m_pService)
            {
                m_pService->stop();
            }
        }

        operator ::BLEService *() { return m_pService; }

        BLECharacteristic *createCharacteristic(const char *uuid, uint32_t properties)
        {
            if (m_charCount >= 4)
                return nullptr;

            ::BLECharacteristic *coreChar = m_pService->createCharacteristic(uuid, properties);
            if (!coreChar)
                return nullptr;

            m_characteristics[m_charCount] = BLECharacteristic(coreChar);
            return &m_characteristics[m_charCount++];
        }

        ~BLEService()
        {
            for (int i = 0; i < m_charCount; i++)
            {
                m_characteristics[i].releaseDescriptor();
            }
            serviceCount--;
        }
    };

    class BLEServer
    {
    private:
        ::BLEServer *m_pServer;
        static BLEService m_service; // Static service instance

    public:
        BLEServer() : m_pServer(nullptr) {}

        BLEServer(::BLEServer *server) : m_pServer(server)
        {
            serverCount++;
            MemoryStats::print("Server Constructor");
        }

        BLEService *createService(const char *uuid)
        {
            if (!m_pServer)
                return nullptr;

            ::BLEService *coreService = m_pServer->createService(uuid);
            if (!coreService)
                return nullptr;

            m_service = BLEService(coreService);
            return &m_service;
        }

        operator ::BLEServer *() { return m_pServer; }

        ~BLEServer()
        {
            serverCount--;
        }

        void stopAllServices()
        {
            m_service.stop();
        }

        BLEService *getService() { return &m_service; }
        void clearService() { /* No direct access needed */ }
        void stopService()
        {
            m_service.stop();
        }
    };

    class BLEDevice
    {
    private:
        static bool initialized;
        static ::BLEServer *m_pCoreServer;         // Core server pointer
        static Hublink::BLEServer m_serverWrapper; // Static wrapper instance
        static BLEAdvertising *m_bleAdvertising;

    public:
        static void init(String deviceName)
        {
            Serial.println("=== BLE Init Sequence Start ===");
            Serial.printf("Initial Memory - Free: %d, Min: %d, Block: %d\n",
                          ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());

            if (!initialized)
            {
                Serial.println("1. Starting BLE initialization");
                Serial.printf("Memory before core init - Free: %d\n", ESP.getFreeHeap());

                // Core BLE init (includes btStart)
                ::BLEDevice::init(deviceName);
                Serial.printf("Memory after BLEDevice init - Free: %d\n", ESP.getFreeHeap());
                delay(50);

                // Controller state
                esp_bt_controller_status_t controller_state = esp_bt_controller_get_status();
                Serial.printf("BLE Controller state: %d (2=enabled)\n", controller_state);
                Serial.printf("Memory after controller check - Free: %d\n", ESP.getFreeHeap());
                delay(50);

                initialized = true;
            }
            else
            {
                Serial.println("Already initialized, skipping");
            }

            Serial.printf("Final Memory - Free: %d, Min: %d, Block: %d\n",
                          ESP.getFreeHeap(), ESP.getMinFreeHeap(), ESP.getMaxAllocHeap());
            Serial.println("=== BLE Init Sequence Complete ===");
        }

        static void deinit(bool release_memory = false)
        {
            if (initialized)
            {
                Serial.println("=== BLE Deinit Sequence Start ===");
                Serial.printf("Memory before stop - Free: %d\n", ESP.getFreeHeap());

                // Stop advertising first
                if (m_bleAdvertising)
                {
                    stopAdvertising();
                    delay(20); // Let advertising stop settle
                }

                // Clear our pointers first
                m_pCoreServer = nullptr;
                m_bleAdvertising = nullptr;
                delay(50); // Let pointer cleanup settle

                // Core BLE cleanup
                ::BLEDevice::deinit(false);
                delay(100); // Give stack time to cleanup

                initialized = false;
                delay(50); // Final cleanup pause

                Serial.printf("Final Memory - Free: %d\n", ESP.getFreeHeap());
                Serial.println("=== BLE Deinit Sequence Complete ===");
            }
        }

        static Hublink::BLEServer *createServer()
        {
            MemoryStats::print("Before createServer");

            if (!initialized)
            {
                Serial.println("Cannot create server - BLE not initialized");
                return nullptr;
            }

            Serial.println("Creating core server...");
            m_pCoreServer = ::BLEDevice::createServer();
            MemoryStats::print("After core server creation");

            if (m_pCoreServer == nullptr)
            {
                Serial.println("Core server creation failed");
                return nullptr;
            }

            m_serverWrapper = Hublink::BLEServer(m_pCoreServer);
            return &m_serverWrapper;
        }

        static BLEAdvertising *getAdvertising()
        {
            if (m_bleAdvertising == nullptr)
            {
                m_bleAdvertising = ::BLEDevice::getAdvertising();
            }
            return m_bleAdvertising;
        }

        static void startAdvertising()
        {
            ::BLEDevice::startAdvertising();
        }

        static void stopAdvertising()
        {
            ::BLEDevice::stopAdvertising();
        }
    };

} // namespace Hublink

// Initialize static members
bool Hublink::BLEDevice::initialized = false;
::BLEServer *Hublink::BLEDevice::m_pCoreServer = nullptr;
Hublink::BLEServer Hublink::BLEDevice::m_serverWrapper;
BLEAdvertising *Hublink::BLEDevice::m_bleAdvertising = nullptr;
Hublink::BLEService Hublink::BLEServer::m_service(nullptr);
Hublink::BLECharacteristic Hublink::BLEService::m_characteristics[4];
int Hublink::BLEService::m_charCount = 0;

#endif // HUBLINK_BLE_H_