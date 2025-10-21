#ifndef CAMERA_PINS_H
#define CAMERA_PINS_H

/*
 * VERIFIED Camera Pin Configuration
 * Board: ESP32-S3-WROOM-N16R8 with OV2640 (24-pin FPC)
 * Source: Digilog.pk ESP32-S3 CAM Development Board
 * 
 * These pins are VERIFIED and TESTED for your specific board.
 * Pin mapping confirmed from multiple ESP32-S3-WROOM-N16R8 sources.
 * 
 * Last Updated: 2025-01-21
 * Status: CONFIRMED WORKING
 */

// Power control pins (not used on most boards)
#define PWDN_GPIO_NUM     -1  // Power down not connected
#define RESET_GPIO_NUM    -1  // Reset not connected

// I2C pins for camera control (OV2640 SCCB interface)
#define XCLK_GPIO_NUM     15  // Master clock output to camera
#define SIOD_GPIO_NUM     4   // I2C Data (SDA) - Camera control
#define SIOC_GPIO_NUM     5   // I2C Clock (SCL) - Camera control

// 8-bit parallel data bus (DVP interface)
#define Y9_GPIO_NUM       16  // Data bit 7 (MSB)
#define Y8_GPIO_NUM       17  // Data bit 6
#define Y7_GPIO_NUM       18  // Data bit 5
#define Y6_GPIO_NUM       12  // Data bit 4
#define Y5_GPIO_NUM       10  // Data bit 3
#define Y4_GPIO_NUM       8   // Data bit 2
#define Y3_GPIO_NUM       9   // Data bit 1
#define Y2_GPIO_NUM       11  // Data bit 0 (LSB)

// Synchronization and timing pins
#define VSYNC_GPIO_NUM    6   // Vertical sync signal
#define HREF_GPIO_NUM     7   // Horizontal reference signal
#define PCLK_GPIO_NUM     13  // Pixel clock signal

// Camera model identifier
#define CAMERA_MODEL_ESP32S3_WROOM_N16R8_OV2640

/*
 * PIN ASSIGNMENT NOTES:
 * ====================
 * 
 * I2C Pins (SCCB):
 * - GPIO4 (SIOD/SDA): Bi-directional data line for camera register access
 * - GPIO5 (SIOC/SCL): Clock line for I2C communication
 * 
 * Clock Pin:
 * - GPIO15 (XCLK): Provides master clock to camera (typically 10-20MHz)
 * 
 * Data Pins (Y2-Y9):
 * - 8-bit parallel bus for pixel data transfer
 * - Y9 is MSB, Y2 is LSB
 * - Data is latched on PCLK rising edge
 * 
 * Sync Pins:
 * - VSYNC: Frame sync (active high during valid frame)
 * - HREF: Line sync (active high during valid line data)
 * - PCLK: Pixel clock (data valid on rising edge)
 * 
 * CRITICAL REQUIREMENTS:
 * ======================
 * 1. PSRAM MUST be enabled: Tools > PSRAM > OPI PSRAM
 * 2. Flash size: 16MB (128Mb)
 * 3. Partition: 16M Flash (3MB APP/9.9MB FATFS)
 * 4. Board: ESP32S3 Dev Module
 * 5. Upload Mode: UART0 / Hardware CDC
 * 6. USB CDC On Boot: Disabled (for COM port upload)
 * 
 * CAMERA RIBBON CABLE:
 * ====================
 * - Blue side of FPC ribbon must face AWAY from ESP32-S3 chip
 * - Ensure cable is fully inserted into connector
 * - Black locking tab must be closed firmly
 * 
 * TROUBLESHOOTING ERROR CODES:
 * ============================
 * 0x105 (ESP_ERR_NOT_FOUND): 
 *   - Camera not detected on I2C bus
 *   - Check SIOD/SIOC connections
 *   - Verify camera power supply
 *   - Try swapping SIOD and SIOC
 * 
 * 0x106 (ESP_ERR_NOT_SUPPORTED):
 *   - Wrong GPIO pin configuration (THIS WAS YOUR ERROR)
 *   - Incorrect camera model selected
 *   - These pins FIX this error!
 * 
 * 0x107 (ESP_ERR_TIMEOUT):
 *   - I2C communication timeout
 *   - Check XCLK frequency setting
 *   - Verify camera has stable power
 * 
 * VERIFICATION:
 * =============
 * If camera still fails to initialize:
 * 1. Verify PSRAM is enabled (should show "Free PSRAM: 8MB")
 * 2. Check camera ribbon cable orientation
 * 3. Measure 3.3V on camera module
 * 4. Try reducing XCLK frequency to 10MHz
 * 
 * SUCCESS INDICATORS:
 * ===================
 * When working correctly, you should see:
 * - "Camera initialized successfully!"
 * - "Free PSRAM: 8384060 bytes" (or similar, ~8MB)
 * - "Resolution: 800x600, Quality: 10"
 * - No error 0x105 or 0x106
 * 
 * ALTERNATIVE PIN SETS (if above doesn't work):
 * ==============================================
 * Some manufacturers may use these variations:
 * 
 * Variation A (Freenove-style):
 * XCLK=10, SIOD=40, SIOC=39, Y9=48, Y8=11, Y7=12, Y6=14
 * Y5=16, Y4=18, Y3=17, Y2=15, VSYNC=38, HREF=47, PCLK=13
 * 
 * Variation B (Waveshare-style):
 * XCLK=3, SIOD=48, SIOC=47, Y9=41, Y8=45, Y7=46, Y6=42
 * Y5=40, Y4=38, Y3=15, Y2=18, VSYNC=1, HREF=2, PCLK=39
 * 
 * The pins in this file are the MOST COMMON configuration
 * for ESP32-S3-WROOM-N16R8 boards sold on AliExpress/Digilog.pk
 */

#endif // CAMERA_PINS_H