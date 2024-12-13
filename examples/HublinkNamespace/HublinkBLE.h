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

    class BLE2902 : public ::BLE2902
    {
    public:
        BLE2902() : ::BLE2902() {}
    };

    class BLECharacteristic
    {
    private:
        ::BLECharacteristic *m_pCharacteristic;
        BLE2902 *m_p2902 = nullptr; // Track single descriptor

    public:
        static const uint32_t PROPERTY_READ = ::BLECharacteristic::PROPERTY_READ;
        static const uint32_t PROPERTY_WRITE = ::BLECharacteristic::PROPERTY_WRITE;
        static const uint32_t PROPERTY_NOTIFY = ::BLECharacteristic::PROPERTY_NOTIFY;
        static const uint32_t PROPERTY_INDICATE = ::BLECharacteristic::PROPERTY_INDICATE;

        BLECharacteristic(::BLECharacteristic *characteristic) : m_pCharacteristic(characteristic) { characteristicCount++; }

        void addDescriptor(BLE2902 *descriptor)
        {
            if (descriptor && m_pCharacteristic)
            {
                descriptorCount++;
                m_p2902 = descriptor; // Store descriptor pointer
                m_pCharacteristic->addDescriptor(descriptor);
            }
        }

        operator ::BLECharacteristic *() { return m_pCharacteristic; }

        ~BLECharacteristic()
        {
            if (m_p2902)
            {
                delete m_p2902;
                m_p2902 = nullptr;
                descriptorCount--;
            }
            characteristicCount--;
        }
    };

    class BLEService
    {
    private:
        ::BLEService *m_pService;
        std::vector<BLECharacteristic *> m_characteristics; // Track characteristics we create

    public:
        BLEService(::BLEService *service) : m_pService(service) { serviceCount++; }

        BLECharacteristic *createCharacteristic(const char *uuid, uint32_t properties)
        {
            if (!m_pService)
            {
                return nullptr;
            }
            auto characteristic = new BLECharacteristic(m_pService->createCharacteristic(uuid, properties));
            m_characteristics.push_back(characteristic);
            return characteristic;
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

        ~BLEService()
        {
            // Clean up characteristics we created
            for (auto characteristic : m_characteristics)
            {
                delete characteristic;
            }
            m_characteristics.clear();
            serviceCount--;
        }
    };

    class BLEServer
    {
    private:
        ::BLEServer *m_pServer;
        std::vector<BLEService *> m_services; // Track services we create

    public:
        BLEServer(::BLEServer *server) : m_pServer(server) { serverCount++; }

        BLEService *createService(const char *uuid)
        {
            if (!m_pServer)
            {
                return nullptr;
            }
            auto service = new BLEService(m_pServer->createService(uuid));
            m_services.push_back(service);
            return service;
        }

        operator ::BLEServer *() { return m_pServer; }

        ~BLEServer()
        {
            // Clean up services we created
            for (auto service : m_services)
            {
                delete service;
            }
            m_services.clear();
            serverCount--;
        }
    };

    class BLEDevice
    {
    private:
        static bool initialized;
        static BLEServer *m_pServer; // Change back to pointer
        static BLEAdvertising *m_bleAdvertising;

    public:
        static void init(String deviceName)
        {
            Serial.println("  + BLEDevice::init called");
            if (!initialized)
            {
                Serial.println("  + Cleaning up any previous state");
                if (m_pServer != nullptr)
                {
                    Serial.println("  + Deleting previous server");
                    delete m_pServer;
                    m_pServer = nullptr;
                }
                m_bleAdvertising = nullptr;

                Serial.println("  + Calling core BLE init");
                ::BLEDevice::init(deviceName);
                Serial.println("  + Core BLE init complete");
                initialized = true;
            }
            else
            {
                Serial.println("  + Already initialized, skipping");
            }
        }

        static void deinit(bool release_memory = false)
        {
            if (initialized)
            {
                Serial.println("  - Resetting wrapper state");
                m_bleAdvertising = nullptr;
                m_pServer = nullptr;
                initialized = false;
            }
        }

        static BLEServer *createServer()
        {
            Serial.println("  + createServer called");
            if (!initialized)
            {
                Serial.println("  + Cannot create server - BLE not initialized");
                return nullptr;
            }

            if (m_pServer != nullptr)
            {
                Serial.println("  + Returning existing server");
                return m_pServer;
            }

            // Add delay to ensure BT stack is ready
            Serial.println("  + Waiting for BT stack...");
            delay(100);

            Serial.println("  + Creating core server");
            ::BLEServer *coreServer = ::BLEDevice::createServer();
            if (coreServer == nullptr)
            {
                Serial.println("  + Failed to create core server");
                return nullptr;
            }

            Serial.println("  + Creating server wrapper");
            try
            {
                m_pServer = new BLEServer(coreServer);
                if (m_pServer == nullptr)
                {
                    Serial.println("  + Failed to create server wrapper");
                    return nullptr;
                }
                Serial.println("  + Server wrapper created successfully");
            }
            catch (...)
            {
                Serial.println("  + Exception creating server wrapper");
                return nullptr;
            }

            return m_pServer;
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
Hublink::BLEServer *Hublink::BLEDevice::m_pServer = nullptr;
BLEAdvertising *Hublink::BLEDevice::m_bleAdvertising = nullptr;

#endif // HUBLINK_BLE_H_