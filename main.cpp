#include "FATFileSystem.h"
#include "FlashIAP.h"
#include "SPIFBlockDevice.h"
#include "USBMSD.h"
#include "mbed.h"

#define SPIF_MOUNT_PATH "spif"
#define FULL_UPDATE_FILE_PATH "/" SPIF_MOUNT_PATH "/" MBED_CONF_APP_UPDATE_FILE

#if !defined(POST_APPLICATION_ADDR)
#error "target.restrict_size must be set for your target in mbed_app.json"
#endif

#define BLINKING_RATE 500ms


DigitalOut led(LED1);
DigitalIn usb_det(BUTTON1);


FATFileSystem fs(SPIF_MOUNT_PATH);
SPIFBlockDevice bd(MBED_CONF_SPIF_DRIVER_SPI_MOSI,
                   MBED_CONF_SPIF_DRIVER_SPI_MISO,
                   MBED_CONF_SPIF_DRIVER_SPI_CLK, MBED_CONF_SPIF_DRIVER_SPI_CS);

FlashIAP flash;

Thread usb_spif_msd_thread;
Thread isr_thread(osPriorityAboveNormal, 0x400, nullptr, "isr_queue_thread");
EventQueue isr_queue;

// volatile bool isWake = false;

// void fall_wake() { isWake = true; }

void USB_SPIF_MSD(SPIFBlockDevice *spif_bd) {
  USBMSD usb(spif_bd);
  while (true) {
    usb.process();
  }
}

void init_fs() {
  bd.init();
  printf("Initialising FATFileSystem!!!\r\n");
  // fs=new FATFileSystem("spif");
  int err = fs.mount(&bd);

  if (err) {
    printf("%s FileSystem Mount Failed\r\n", fs.getName());
    err = fs.format(&bd);
  }

  if (err) {
    printf("ERROR: Unable to format /mount the devices. \r\n try to reformat "
           "devices\r\n");
    err = fs.reformat(&bd);

    if (err) {
      printf("Error : Unable to reformat \r\n");
      while (true)
        ;
    }
  }
}


void apply_update(FILE *file, uint32_t address) {
  fseek(file, 0, SEEK_END);
  long len = ftell(file);
  printf("Firmware size is %ld bytes\r\n", len);
  fseek(file, 0, SEEK_SET);

  flash.init();

  const uint32_t page_size = flash.get_page_size();
  char *page_buffer = new char[page_size];
  uint32_t addr = address;
  uint32_t next_sector = addr + flash.get_sector_size(addr);
  bool sector_erased = false;
  size_t pages_flashed = 0;
  uint32_t percent_done = 0;
  while (true) {

    // Read data for this page
    memset(page_buffer, 0, sizeof(char) * page_size);
    int size_read = fread(page_buffer, 1, page_size, file);
    if (size_read <= 0) {
      break;
    }

    // Erase this page if it hasn't been erased
    if (!sector_erased) {
      flash.erase(addr, flash.get_sector_size(addr));
      sector_erased = true;
    }

    // Program page
    flash.program(page_buffer, addr, page_size);

    addr += page_size;
    if (addr >= next_sector) {
      next_sector = addr + flash.get_sector_size(addr);
      sector_erased = false;
    }

    if (++pages_flashed % 3 == 0) {
      uint32_t percent_done_new = ftell(file) * 100 / len;
      if (percent_done != percent_done_new) {
        percent_done = percent_done_new;
        printf("Flashed %3d%%\r", percent_done);
      }
    }
  }
  printf("Flashed 100%%\r\n");

  delete[] page_buffer;

  flash.deinit();
}

int main() {

  if (!usb_det.read()) {

    // pdone = 1;
    printf("\r\n--------------------\r\n");
    printf("| Hello UPSMON MSD |\r\n");
    printf("--------------------\r\n");
    // pdone = 0;

    init_fs();
    usb_spif_msd_thread.start(callback(USB_SPIF_MSD, &bd));
    // pwake.fall(&fall_wake);

    isr_thread.start(callback(&isr_queue, &EventQueue::dispatch_forever));
    // tpl5010.init(&isr_queue);

    while (true) {

      //   if (isWake) {
      //     pdone = 1;
      //     isWake = false;
      //     pdone = 0;
      //   }

    //   if (tpl5010.get_wdt()) {
    //     tpl5010.set_wdt(false);
    //   }

      led = !led;
      ThisThread::sleep_for(BLINKING_RATE);
    }

  } else {
    led = 0;
    // printf("\r\nStart address 0x%x\r\n", POST_APPLICATION_ADDR );

    bd.init();
    int err = fs.mount(&bd);

    if (err) {
      printf("%s filesystem mount failed\r\n", fs.getName());
    }

    FILE *file = fopen(FULL_UPDATE_FILE_PATH, "rb");

    if (file != NULL) {
      printf("Firmware update found\r\n");

      apply_update(file, POST_APPLICATION_ADDR);

      fclose(file);
      remove(FULL_UPDATE_FILE_PATH);
    } else {
      printf("No update found to apply\r\n");
    }

    fs.unmount();
    bd.deinit();

    printf("Starting application\r\n");
    mbed_start_application(POST_APPLICATION_ADDR);
  }

  return 0;
}
