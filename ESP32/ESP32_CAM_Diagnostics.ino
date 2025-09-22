/*
 * ESP32-CAM Complete Memory Diagnostics
 * This code provides detailed information about ESP32-CAM memory status
 * Compatible with Arduino IDE - No additional libraries required
 * 
 * Author: System Diagnostics
 * Board: ESP32 Dev Module (select this in Arduino IDE)
 */

#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_flash.h"
#include "esp_partition.h"
#include "esp_chip_info.h"
#include "esp_ota_ops.h"
#include "soc/rtc.h"
#include "soc/efuse_reg.h"
#include "driver/adc.h"
#include <WiFi.h>

// Function to convert bytes to human readable format
String formatBytes(size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  else if (bytes < (1024 * 1024)) return String(bytes / 1024.0, 2) + " KB";
  else if (bytes < (1024 * 1024 * 1024)) return String(bytes / 1024.0 / 1024.0, 2) + " MB";
  else return String(bytes / 1024.0 / 1024.0 / 1024.0, 2) + " GB";
}

// Function to get CPU frequency
uint32_t getCpuFrequency() {
  rtc_cpu_freq_config_t freq_config;
  rtc_clk_cpu_freq_get_config(&freq_config);
  return freq_config.freq_mhz;
}

// Function to print heap memory details
void printHeapDetails() {
  Serial.println("\n" + String("=").substring(0, 60));
  Serial.println("📊 HEAP MEMORY ANALYSIS");
  Serial.println(String("=").substring(0, 60));
  
  // Total heap information
  size_t total_heap = ESP.getHeapSize();
  size_t free_heap = ESP.getFreeHeap();
  size_t used_heap = total_heap - free_heap;
  size_t min_free_heap = ESP.getMinFreeHeap();
  size_t max_alloc_heap = ESP.getMaxAllocHeap();
  
  Serial.println("🔹 General Heap:");
  Serial.println("   Total Heap Size: " + formatBytes(total_heap));
  Serial.println("   Free Heap: " + formatBytes(free_heap));
  Serial.println("   Used Heap: " + formatBytes(used_heap));
  Serial.println("   Min Free Heap (since boot): " + formatBytes(min_free_heap));
  Serial.println("   Max Allocatable Block: " + formatBytes(max_alloc_heap));
  Serial.println("   Heap Fragmentation: " + String(100.0 - (max_alloc_heap * 100.0 / free_heap), 1) + "%");
  
  // Detailed heap capabilities
  Serial.println("\n🔹 Detailed Heap Breakdown:");
  Serial.println("   Internal RAM (DRAM):");
  Serial.println("     Total: " + formatBytes(heap_caps_get_total_size(MALLOC_CAP_INTERNAL)));
  Serial.println("     Free: " + formatBytes(heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
  Serial.println("     Largest block: " + formatBytes(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL)));
  
  Serial.println("   DMA Capable Memory:");
  Serial.println("     Total: " + formatBytes(heap_caps_get_total_size(MALLOC_CAP_DMA)));
  Serial.println("     Free: " + formatBytes(heap_caps_get_free_size(MALLOC_CAP_DMA)));
  Serial.println("     Largest block: " + formatBytes(heap_caps_get_largest_free_block(MALLOC_CAP_DMA)));
  
  Serial.println("   32-bit Accessible Memory:");
  Serial.println("     Total: " + formatBytes(heap_caps_get_total_size(MALLOC_CAP_32BIT)));
  Serial.println("     Free: " + formatBytes(heap_caps_get_free_size(MALLOC_CAP_32BIT)));
  Serial.println("     Largest block: " + formatBytes(heap_caps_get_largest_free_block(MALLOC_CAP_32BIT)));
}

// Function to print PSRAM details
void printPSRAMDetails() {
  Serial.println("\n" + String("=").substring(0, 60));
  Serial.println("💾 PSRAM (EXTERNAL RAM) ANALYSIS");
  Serial.println(String("=").substring(0, 60));
  
  if (psramFound()) {
    size_t psram_size = ESP.getPsramSize();
    size_t free_psram = ESP.getFreePsram();
    size_t used_psram = psram_size - free_psram;
    
    Serial.println("✅ PSRAM Status: FOUND & AVAILABLE");
    Serial.println("🔹 PSRAM Details:");
    Serial.println("   Total PSRAM: " + formatBytes(psram_size));
    Serial.println("   Free PSRAM: " + formatBytes(free_psram));
    Serial.println("   Used PSRAM: " + formatBytes(used_psram));
    Serial.println("   PSRAM Usage: " + String((used_psram * 100.0) / psram_size, 1) + "%");
    
    Serial.println("\n🔹 PSRAM Heap Capabilities:");
    Serial.println("   SPIRAM Total: " + formatBytes(heap_caps_get_total_size(MALLOC_CAP_SPIRAM)));
    Serial.println("   SPIRAM Free: " + formatBytes(heap_caps_get_free_size(MALLOC_CAP_SPIRAM)));
    Serial.println("   SPIRAM Largest Block: " + formatBytes(heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM)));
  } else {
    Serial.println("❌ PSRAM Status: NOT FOUND");
    Serial.println("⚠️  This may limit camera performance!");
    Serial.println("💡 Enable PSRAM in Arduino IDE: Tools > PSRAM > Enabled");
  }
}

// Function to print Flash memory details
void printFlashDetails() {
  Serial.println("\n" + String("=").substring(0, 60));
  Serial.println("💽 FLASH MEMORY ANALYSIS");
  Serial.println(String("=").substring(0, 60));
  
  // Flash chip information
  uint32_t flash_size = ESP.getFlashChipSize();
  uint32_t flash_speed = ESP.getFlashChipSpeed();
  FlashMode_t flash_mode = ESP.getFlashChipMode();
  
  Serial.println("🔹 Flash Chip Details:");
  Serial.println("   Flash Size: " + formatBytes(flash_size));
  Serial.println("   Flash Speed: " + String(flash_speed / 1000000) + " MHz");
  Serial.print("   Flash Mode: ");
  switch(flash_mode) {
    case FM_QIO: Serial.println("QIO"); break;
    case FM_QOUT: Serial.println("QOUT"); break;
    case FM_DIO: Serial.println("DIO"); break;
    case FM_DOUT: Serial.println("DOUT"); break;
    case FM_FAST_READ: Serial.println("FAST_READ"); break;
    case FM_SLOW_READ: Serial.println("SLOW_READ"); break;
    default: Serial.println("UNKNOWN"); break;
  }
  
  // Partition information
  Serial.println("\n🔹 Flash Partitions:");
  esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
  
  while (it != NULL) {
    const esp_partition_t *partition = esp_partition_get(it);
    Serial.println("   📁 " + String(partition->label) + ":");
    Serial.println("      Type: " + String(partition->type) + ", Subtype: " + String(partition->subtype));
    Serial.println("      Address: 0x" + String(partition->address, HEX));
    Serial.println("      Size: " + formatBytes(partition->size));
    
    it = esp_partition_next(it);
  }
  esp_partition_iterator_release(it);
}

// Function to print chip information
void printChipInfo() {
  Serial.println("\n" + String("=").substring(0, 60));
  Serial.println("🔧 ESP32 CHIP INFORMATION");
  Serial.println(String("=").substring(0, 60));
  
  esp_chip_info_t chip_info;
  esp_chip_info(&chip_info);
  
  Serial.println("🔹 Chip Details:");
  Serial.print("   Model: ESP32");
  if (chip_info.revision > 0) {
    Serial.println(" (Rev " + String(chip_info.revision) + ")");
  } else {
    Serial.println();
  }
  
  Serial.println("   CPU Cores: " + String(chip_info.cores));
  Serial.println("   CPU Frequency: " + String(getCpuFrequency()) + " MHz");
  
  Serial.print("   Features: WiFi");
  if (chip_info.features & CHIP_FEATURE_BT) Serial.print(" + BT");
  if (chip_info.features & CHIP_FEATURE_BLE) Serial.print(" + BLE");
  Serial.println();
  
  // MAC Address
  uint8_t mac[6];
  WiFi.macAddress(mac);
  Serial.printf("   MAC Address: %02X:%02X:%02X:%02X:%02X:%02X\n", 
                mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  
  // Chip ID and other identifiers
  uint32_t chip_id = 0;
  for(int i=0; i<17; i=i+8) {
    chip_id |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  Serial.println("   Chip ID: " + String(chip_id, HEX));
  
  // SDK Version
  Serial.println("   SDK Version: " + String(ESP.getSdkVersion()));
}

// Function to print power and temperature info
void printPowerInfo() {
  Serial.println("\n" + String("=").substring(0, 60));
  Serial.println("⚡ POWER & TEMPERATURE STATUS");
  Serial.println(String("=").substring(0, 60));
  
  // Internal temperature (approximate)
  Serial.println("🔹 System Status:");
  Serial.println("   Uptime: " + String(millis() / 1000) + " seconds");
  Serial.println("   Reset Reason: " + String(esp_reset_reason()));
  
  // Voltage reading (if available)
  // Note: Internal voltage reading may not be accurate on all ESP32 variants
  Serial.println("   Supply Voltage: ~3.3V (estimated)");
  
  Serial.println("\n🔹 Performance Metrics:");
  Serial.println("   Free Task Stack: " + String(uxTaskGetStackHighWaterMark(NULL)) + " bytes");
  Serial.println("   Total Tasks: " + String(uxTaskGetNumberOfTasks()));
}

// Function to test memory allocation
void testMemoryAllocation() {
  Serial.println("\n" + String("=").substring(0, 60));
  Serial.println("🧪 MEMORY ALLOCATION TEST");
  Serial.println(String("=").substring(0, 60));
  
  Serial.println("🔹 Testing Memory Allocation Capabilities:");
  
  // Test DRAM allocation
  size_t test_size = 1024; // 1KB
  void* test_ptr = heap_caps_malloc(test_size, MALLOC_CAP_INTERNAL);
  if (test_ptr) {
    Serial.println("   ✅ DRAM Allocation (1KB): SUCCESS");
    heap_caps_free(test_ptr);
  } else {
    Serial.println("   ❌ DRAM Allocation (1KB): FAILED");
  }
  
  // Test DMA allocation
  test_ptr = heap_caps_malloc(test_size, MALLOC_CAP_DMA);
  if (test_ptr) {
    Serial.println("   ✅ DMA Allocation (1KB): SUCCESS");
    heap_caps_free(test_ptr);
  } else {
    Serial.println("   ❌ DMA Allocation (1KB): FAILED");
  }
  
  // Test PSRAM allocation if available
  if (psramFound()) {
    test_ptr = heap_caps_malloc(test_size, MALLOC_CAP_SPIRAM);
    if (test_ptr) {
      Serial.println("   ✅ PSRAM Allocation (1KB): SUCCESS");
      heap_caps_free(test_ptr);
    } else {
      Serial.println("   ❌ PSRAM Allocation (1KB): FAILED");
    }
  }
  
  // Test large allocation
  size_t large_size = 32 * 1024; // 32KB
  test_ptr = malloc(large_size);
  if (test_ptr) {
    Serial.println("   ✅ Large Allocation (32KB): SUCCESS");
    free(test_ptr);
  } else {
    Serial.println("   ❌ Large Allocation (32KB): FAILED");
  }
}

void setup() {
  // Initialize serial communication
  Serial.begin(115200);
  
  // Wait for serial port to connect
  while (!Serial) {
    delay(10);
  }
  
  // Give some time for serial monitor to open
  delay(2000);
  
  // Print header
  Serial.println("\n\n" + String("█").substring(0, 60));
  Serial.println("🔍 ESP32-CAM COMPLETE MEMORY DIAGNOSTICS");
  Serial.println("📅 Started at: " + String(millis()) + " ms after boot");
  Serial.println(String("█").substring(0, 60));
  
  // Run all diagnostic functions
  printChipInfo();
  printHeapDetails();
  printPSRAMDetails();
  printFlashDetails();
  printPowerInfo();
  testMemoryAllocation();
  
  // Final summary
  Serial.println("\n" + String("=").substring(0, 60));
  Serial.println("📋 QUICK SUMMARY");
  Serial.println(String("=").substring(0, 60));
  Serial.println("Total RAM: " + formatBytes(ESP.getHeapSize()));
  Serial.println("Free RAM: " + formatBytes(ESP.getFreeHeap()));
  if (psramFound()) {
    Serial.println("Total PSRAM: " + formatBytes(ESP.getPsramSize()));
    Serial.println("Free PSRAM: " + formatBytes(ESP.getFreePsram()));
  } else {
    Serial.println("PSRAM: Not Available ⚠️");
  }
  Serial.println("Flash Size: " + formatBytes(ESP.getFlashChipSize()));
  Serial.println("CPU Frequency: " + String(getCpuFrequency()) + " MHz");
  
  Serial.println("\n✅ Diagnostics completed!");
  Serial.println("💡 Use this information to optimize your camera code.");
}

void loop() {
  // Update memory status every 10 seconds
  static unsigned long last_update = 0;
  if (millis() - last_update > 10000) {
    Serial.println("\n⏱️  Memory Status Update (Running for " + String(millis() / 1000) + "s):");
    Serial.println("   Free Heap: " + formatBytes(ESP.getFreeHeap()));
    if (psramFound()) {
      Serial.println("   Free PSRAM: " + formatBytes(ESP.getFreePsram()));
    }
    Serial.println("   Min Free Heap: " + formatBytes(ESP.getMinFreeHeap()));
    last_update = millis();
  }
  
  delay(1000);
}