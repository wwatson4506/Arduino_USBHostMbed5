/*
  Portenta - DirList

  The sketch shows how to mount an usb storage device and how to
  get a list of the existing folders and files.

  The circuit:
   - Portenta H7

  This example code is in the public domain.
*/

#include <DigitalOut.h>
#include <FATFileSystem.h>
#include <Arduino_USBHostMbed5.h>
//#include <sys/stat.h>

USBHostMSD msd;
mbed::FATFileSystem usb("usb");

// If you are using a Portenta Machine Control uncomment the following line
// mbed::DigitalOut otg(PB_14, 0);

#define NUMSPACES			40

// Directory entry types.
static const char *entry_to_str(uint8_t type)
{
	switch (type) {
	case DT_UNKNOWN:
		return "<UNKNOWN>";
	case DT_REG:
		return "<FILE>";
	case DT_DIR:
		return "<DIR>";
	case DT_CHR:
		return "<CHARDEV>";
	case DT_BLK:
		return "<BLOCKDEV>";
	case DT_FIFO:
		return "<FIFO>";
	case DT_SOCK:
		return "<SOCKET>";
	case DT_LNK:
		return "<SYMLINK>";
	default:
		break;
	}
	return "[???]";
}

void setup()
{

    struct stat fsize;

    Serial.begin(115200);
    while (!Serial)
        ;

    Serial.println("Starting USB Dir List example...");

    // if you are using a Max Carrier uncomment the following line
    // start_hub();

    while (!msd.connect()) {
        //while (!port.connected()) {
        delay(1000);
    }

    Serial.print("Mounting USB device... ");
    int err = usb.mount(&msd);
    if (err) {
        Serial.print("Error mounting USB device ");
        Serial.println(err);
        while (1);
    }
    Serial.println("done.");

    char buf[512];
	uint8_t spacing = NUMSPACES;

    // Display the root directory
    Serial.print("Opening the root directory... ");
    DIR* d = opendir("/usb/diskio/");
    Serial.println(!d ? "Fail :(" : "Done");
    if (!d) {
        snprintf(buf, sizeof(buf), "error: %s (%d)\r\n", strerror(errno), -errno);
        Serial.print(buf);
    }
    Serial.println("done.");

    Serial.println("Root directory:");
    unsigned int count { 0 };
    while (true) {
        struct dirent* e = readdir(d);
        if (!e) {
            break;
        }
        usb.stat((const char *)e->d_name,&fsize);
        snprintf(buf, sizeof(buf), "%s", e->d_name);
        Serial.print(buf);
		uint8_t fnlen = strlen(e->d_name);
		if(fnlen > spacing) // Check for filename bigger the NUMSPACES.
			spacing += (fnlen - spacing); // Correct for buffer overun.
		else
			spacing = NUMSPACES;
		for(int i = 0; i < (spacing-fnlen); i++)
			Serial.print((char)' ');
        count++;
        snprintf(buf, sizeof(buf), "%10s   ", entry_to_str(e->d_type));
        Serial.print(buf);
        if(e->d_type == DT_DIR) {
			Serial.println();
        } else {
			snprintf(buf, sizeof(buf),"0x%x\n", (uint64_t)fsize.st_size);
			Serial.print(buf);
		}
    }
    Serial.print(count);
    Serial.println(" files found!");

    snprintf(buf, sizeof(buf), "Closing the root directory... ");
    Serial.print(buf);
    fflush(stdout);
    err = closedir(d);
    snprintf(buf, sizeof(buf), "%s\r\n", (err < 0 ? "Fail :(" : "OK"));
    Serial.print(buf);
    if (err < 0) {
        snprintf(buf, sizeof(buf), "error: %s (%d)\r\n", strerror(errno), -errno);
        Serial.print(buf);
    }
}

void loop()
{
    delay(1000);
    // handle disconnection and reconnection
    if (!msd.connected()) {
        msd.connect();
    }
}
