#include <FATFileSystem.h>
#include <Arduino_USBHostMbed5.h>

#include <Arduino_Portenta_OTA.h>

// The sketch expects the update file to be in the root of the USB key.
// 
// The file can be both a plain .bin generated from the IDEs or the CLI (-e option)
// or an armored OTA file generated by the `ota-builder` or
// the `lzss.py` + `bin2ota.py` tools. The sketch will take care of both the cases.
//
// Feel free to change the name of the file accordingly.
static const char OTA_FILE[] { "update.ota" }; // Armored OTA file
// static const char OTA_FILE[] { "update.bin" }; // Plain BIN file

// !!! DO NOT TOUCH !!!
// These are the names for the OTA files on the QSPIF
// which the Arduino_Portenta_OTA library looks for.
static const char UPDATE_FILE_NAME[] { "/fs/UPDATE.BIN" };
static const char UPDATE_FILE_NAME_LZSS[] { "/fs/UPDATE.BIN.LZSS" };

// The internal mountpoint for the USB filesystem
static const char USB_MOUNT_POINT[] { "usb" };
mbed::FATFileSystem usb(USB_MOUNT_POINT);
USBHostMSD msd;

// Reuse QSPIF Block Device from WiFi Driver
extern QSPIFBlockDevice* qspi_bd;

constexpr auto PMC_USBA_VBUS_ENABLE { PB_14 };

void setup()
{
    // If you are using the Portenta Machine Control 
    // enable power on the USB connector.
    pinMode(PMC_USBA_VBUS_ENABLE, OUTPUT);
    digitalWrite(PMC_USBA_VBUS_ENABLE, LOW);

    Serial.begin(115200);
    while (!Serial) { }

    delay(2500);
    Serial.println("Starting OTA via USB example...");

    Arduino_Portenta_OTA_QSPI ota(QSPI_FLASH_FATFS_MBR, 2);
    Arduino_Portenta_OTA::Error ota_err = Arduino_Portenta_OTA::Error::None;

    if (!ota.isOtaCapable()) {
        Serial.println("Higher version bootloader required to perform OTA.");
        Serial.println("Please update the bootloader.");
        Serial.println("File -> Examples -> Portenta_System -> PortentaH7_updateBootloader");
        return;
    }

    // Initialize the QSPIF Block Device only if the WiFi Driver has not initialized it already
    if (qspi_bd == nullptr) {
        Serial.print("Initializing the QSPIF device...");
        qspi_bd = new QSPIFBlockDevice(PD_11, PD_12, PF_7, PD_13, PF_10, PG_6, QSPIF_POLARITY_MODE_1, 40000000);
        if (qspi_bd->init() != QSPIF_BD_ERROR_OK) {
            Serial.println(" Error.");
            return;
        }
        Serial.println(" Done.");
    }

    if ((ota_err = ota.begin()) != Arduino_Portenta_OTA::Error::None) {
        Serial.print("Arduino_Portenta_OTA::begin() failed with error code ");
        Serial.println((int)ota_err);
        return;
    }
    Serial.println("Initializing OTA storage. Done.");

    Serial.print("Please, insert the USB Key...");
    while (!msd.connect()) {
        Serial.print(".");
        delay(1000);
    }
    Serial.println();

    Serial.print("Mounting USB device...");
    int err = usb.mount(&msd);
    if (err) {
        Serial.print(" Error: ");
        Serial.println(err);
        return;
    }
    Serial.println(" Done.");

    String otaFileLocation;
    otaFileLocation += "/";
    otaFileLocation += USB_MOUNT_POINT;
    otaFileLocation += "/";
    otaFileLocation += OTA_FILE;

    Serial.print("Opening source file \"");
    Serial.print(otaFileLocation);
    Serial.print("\"");
    FILE* src = fopen(otaFileLocation.c_str(), "rb+");
    if (src == nullptr) {
        Serial.print(" Error opening file.");
        return;
    }

    // Get file length
    fseek(src, 0, SEEK_END);
    auto fileLen = ftell(src);
    fseek(src, 0, SEEK_SET);

    Serial.print(" [");
    Serial.print(fileLen);
    Serial.println(" bytes].");

    // Check for plain .BIN or armored .OTA and select destination file accordingly.
    String updateFileName;
    if (otaFileLocation.endsWith(".ota") || otaFileLocation.endsWith(".lzss"))
        updateFileName = UPDATE_FILE_NAME_LZSS;
    else
        updateFileName = UPDATE_FILE_NAME;

    Serial.print("Opening destination file \"");
    Serial.print(updateFileName);
    Serial.print("\"...");
    FILE* dst = fopen(updateFileName.c_str(), "wb");
    if (dst == nullptr) {
        Serial.print("Error opening file ");
        Serial.println(updateFileName);
        return;
    }
    Serial.println(" Done.");

    Serial.print("Copying OTA file from USB key to QSPI...");
    constexpr size_t bufLen { 1024u };
    char buf[bufLen] {};
    int ota_download {};
    size_t rb {};
    while ((rb = fread(buf, 1, bufLen, src)) > 0)
        ota_download += fwrite(buf, 1, rb, dst);

    if (ota_download != fileLen) {
        Serial.print("Download from USB Key failed with error code ");
        Serial.println(ota_download);
        return;
    }
    Serial.print(" [");
    Serial.print(ota_download);
    Serial.println(" bytes].");

    Serial.print("Closing source file...");
    err = fclose(src);
    if (err < 0) {
        Serial.print("fclose error:");
        Serial.print(strerror(errno));
        Serial.print(" (");
        Serial.print(-errno);
        Serial.print(")");
        return;
    }
    Serial.println(" Done.");

    Serial.print("Closing destination file...");
    err = fclose(dst);
    if (err < 0) {
        Serial.print("fclose error:");
        Serial.print(strerror(errno));
        Serial.print(" (");
        Serial.print(-errno);
        Serial.print(")");
        return;
    }
    Serial.println(" Done.");

    // Decompress file in case of armored .OTA
    if (otaFileLocation.endsWith(".ota") || otaFileLocation.endsWith(".lzss")) {
        Serial.print("Decompressing LZSS compressed file... ");
        const auto ota_decompress = ota.decompress();
        if (ota_decompress < 0) {
            Serial.print("Arduino_Portenta_OTA_QSPI::decompress() failed with error code");
            Serial.println(ota_decompress);
            return;
        }
        Serial.print(ota_decompress);
        Serial.println(" bytes decompressed.");
    }

    Serial.print("Storing parameters for firmware update in bootloader accessible non-volatile memory...");
    if ((ota_err = ota.update()) != Arduino_Portenta_OTA::Error::None) {
        Serial.print(" ota.update() failed with error code ");
        Serial.println((int)ota_err);
        return;
    }
    Serial.println(" Done.");

    Serial.println("Performing a reset after which the bootloader will update the firmware.");
    Serial.flush();
    delay(1000); /* Make sure the serial message gets out before the reset. */
    ota.reset();
}

void loop()
{
    delay(1000);
    // handle disconnection and reconnection
    if (!msd.connected()) {
        msd.connect();
    }
}
