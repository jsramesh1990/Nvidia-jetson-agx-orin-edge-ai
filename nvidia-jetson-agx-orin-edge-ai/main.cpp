#include <iostream>
#include <chrono>
#include <thread>
#include <signal.h>
#include <vector>
#include <opencv2/opencv.hpp>

#include "drivers/camera/camera_driver.h"
#include "drivers/gpio/gpio_driver.h"
#include "drivers/storage/storage_driver.h"
#include "drivers/spi/spi_driver.h"
#include "drivers/uart/uart_driver.h"
#include "drivers/i2c/i2c_driver.h"

#include "backend/api/api_server.h"
#include "communication/mqtt/mqtt_client.h"

using namespace EdgeAI;

// Global running flag
volatile sig_atomic_t running = 1;

void signalHandler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        std::cout << "\nShutting down..." << std::endl;
        running = 0;
    }
}

int main(int argc, char** argv) {
    // Register signal handlers
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "🚀 NVIDIA Jetson AGX Orin Edge AI System" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Starting system... (Press Ctrl+C to stop)" << std::endl;
    
    try {
        // 1. Initialize Camera
        std::cout << "\n📷 Initializing Camera..." << std::endl;
        CameraConfig cam_config;
        cam_config.type = CAMERA_CSI;
        cam_config.resolution = RES_1920x1080;
        cam_config.fps = FPS_30;
        cam_config.device = "/dev/video0";
        
        CameraDriver camera(cam_config);
        if (!camera.initialize()) {
            std::cerr << "⚠️  Failed to initialize camera. Continuing without camera." << std::endl;
        } else {
            camera.startStream();
            std::cout << "✅ Camera initialized successfully" << std::endl;
        }
        
        // 2. Initialize GPIO
        std::cout << "\n🔌 Initializing GPIO..." << std::endl;
        GPIODriver gpio;
        if (gpio.setupPin(17, GPIO_OUT)) {
            std::cout << "✅ GPIO initialized successfully" << std::endl;
        } else {
            std::cerr << "⚠️  Failed to initialize GPIO" << std::endl;
        }
        
        // 3. Initialize Storage
        std::cout << "\n💾 Initializing Storage..." << std::endl;
        StorageConfig storage_config;
        storage_config.root_path = "./data";
        storage_config.temp_path = "/tmp/edge_ai";
        storage_config.max_size_gb = 10;
        
        StorageDriver storage(storage_config);
        if (storage.initialize()) {
            std::cout << "✅ Storage initialized successfully" << std::endl;
            auto stats = storage.getStorageStats();
            std::cout << "   Total: " << stats.total_space_gb << " GB" << std::endl;
            std::cout << "   Free: " << stats.free_space_gb << " GB" << std::endl;
        } else {
            std::cerr << "⚠️  Failed to initialize storage" << std::endl;
        }
        
        // 4. Initialize SPI (if available)
        std::cout << "\n🔌 Initializing SPI..." << std::endl;
        SPIConfig spi_config;
        spi_config.bus = SPI_BUS_0;
        spi_config.cs = SPI_CS_0;
        spi_config.speed = 1000000;
        
        SPIDriver spi(spi_config);
        if (spi.initialize()) {
            std::cout << "✅ SPI initialized successfully" << std::endl;
        } else {
            std::cerr << "⚠️  Failed to initialize SPI (may not be available)" << std::endl;
        }
        
        // 5. Initialize UART
        std::cout << "\n🔌 Initializing UART..." << std::endl;
        UARTConfig uart_config;
        uart_config.device = "/dev/ttyTHS1";
        uart_config.baudrate = 115200;
        uart_config.data_bits = UART_DATA_BITS_8;
        uart_config.parity = UART_PARITY_NONE;
        uart_config.stop_bits = UART_STOP_BITS_1;
        
        UARTDriver uart(uart_config);
        if (uart.initialize()) {
            std::cout << "✅ UART initialized successfully" << std::endl;
        } else {
            std::cerr << "⚠️  Failed to initialize UART (may not be available)" << std::endl;
        }
        
        // 6. Initialize I2C
        std::cout << "\n🔌 Initializing I2C..." << std::endl;
        I2CConfig i2c_config;
        i2c_config.device = "/dev/i2c-1";
        i2c_config.speed = 100000;
        i2c_config.use_smbus = true;
        
        I2CDriver i2c(i2c_config);
        if (i2c.initialize()) {
            std::cout << "✅ I2C initialized successfully" << std::endl;
            
            // Scan I2C bus
            std::vector<uint8_t> devices;
            if (i2c.scanBus(devices)) {
                std::cout << "   Found devices at: ";
                for (auto addr : devices) {
                    std::cout << "0x" << std::hex << (int)addr << " ";
                }
                std::cout << std::dec << std::endl;
            }
        } else {
            std::cerr << "⚠️  Failed to initialize I2C (may not be available)" << std::endl;
        }
        
        // 7. Initialize MQTT
        std::cout << "\n📡 Initializing MQTT..." << std::endl;
        MQTTClient mqtt("localhost", 1883);
        if (mqtt.connect("edge_ai_client")) {
            std::cout << "✅ MQTT connected successfully" << std::endl;
        } else {
            std::cerr << "⚠️  Failed to connect to MQTT broker" << std::endl;
        }
        
        // 8. Initialize API Server
        std::cout << "\n🌐 Starting API Server..." << std::endl;
        APIServer api(8000);
        std::thread api_thread([&api]() {
            api.start();
        });
        std::cout << "✅ API Server started on port 8000" << std::endl;
        
        // 9. Main Loop
        std::cout << "\n✅ System is running!" << std::endl;
        std::cout << "Press Ctrl+C to stop." << std::endl;
        std::cout << "\n🔍 Monitoring..." << std::endl;
        
        int frame_count = 0;
        auto start_time = std::chrono::steady_clock::now();
        
        while (running) {
            // Capture frame if camera is available
            if (camera.isRunning()) {
                cv::Mat frame = camera.captureFrame();
                if (!frame.empty()) {
                    frame_count++;
                    
                    // Process every 30 frames
                    if (frame_count % 30 == 0) {
                        auto now = std::chrono::steady_clock::now();
                        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                            now - start_time).count();
                        
                        if (elapsed > 0) {
                            float fps = frame_count / elapsed;
                            std::cout << "\r📊 FPS: " << std::fixed << std::setprecision(2) 
                                     << fps << " | Frames: " << frame_count << std::flush;
                        }
                        
                        // Blink LED to show system is alive
                        gpio.togglePin(17);
                        
                        // Publish status to MQTT
                        if (mqtt.isConnected()) {
                            std::string status = "{\"status\":\"running\",\"frames\":" + 
                                                std::to_string(frame_count) + "}";
                            mqtt.publish("edge_ai/status", status);
                        }
                    }
                }
            }
            
            // Small sleep to prevent CPU hogging
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Cleanup
        std::cout << "\n\n🧹 Cleaning up..." << std::endl;
        
        camera.stopStream();
        spi.close();
        uart.close();
        i2c.close();
        mqtt.disconnect();
        gpio.cleanup();
        api.stop();
        storage.shutdown();
        
        if (api_thread.joinable()) {
            api_thread.join();
        }
        
        std::cout << "✅ System shutdown complete!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "\n❌ Fatal error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
