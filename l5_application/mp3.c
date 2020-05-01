#include "mp3.h"

#include <stdio.h>
#include <stdlib.h>

#include "gpio.h"
#include "gpio_lab.h"
#include "ssp0.h"

void init_GPIO() {
  // setup DREQ
  gpio__construct_with_function(GPIO__PORT_2, 0, GPIO__FUNCITON_0_IO_PIN);
  gpio__lab__set_as_input(2, 0);

  // setup CS (low)
  gpio__construct_with_function(GPIO__PORT_2, 1, GPIO__FUNCITON_0_IO_PIN);
  gpio__lab__set_as_output(2, 1);

  // setup DCS (low)
  gpio__construct_with_function(GPIO__PORT_2, 2, GPIO__FUNCITON_0_IO_PIN);
  gpio__lab__set_as_output(2, 2);

  // setup Reset (low)
  gpio__construct_with_function(GPIO__PORT_2, 4, GPIO__FUNCITON_0_IO_PIN);
  gpio__lab__set_as_output(2, 4);
}

void init_SPI() {
  // set SPI pins to SPI function
  // sck: p0.15
  gpio__construct_with_function(GPIO__PORT_0, 15, GPIO__FUNCTION_2);

  // mis0: p0.17
  gpio__construct_with_function(GPIO__PORT_0, 17, GPIO__FUNCTION_2);

  // mosi: p0.18
  gpio__construct_with_function(GPIO__PORT_0, 18, GPIO__FUNCTION_2);
}

void mp3_setup() {

  // deactivate reset
  gpio__lab_set_high(2, 4);
  // setup GPIO pins for DREQ, CS, DCS, RESET
  init_GPIO();

  // init SPI on SJ2
  SSP0__initialize(1000);

  // send dummy byte
  SSP0__exchange_byte(0xFF);

  // Deselect dcs & cs
  gpio__lab_set(2, 1, true);
  gpio__lab_set(2, 2, true);

  // set volume
  Mp3WriteRegister(SCI_VOL, 25, 25);

  // Let's check the status of the VS1053
  int MP3Mode = Mp3ReadRegister(SCI_MODE);
  int MP3Status = Mp3ReadRegister(SCI_STATUS);
  int MP3Clock = Mp3ReadRegister(SCI_CLOCKF);

  printf("SCIMODE = 0x%x", MP3Mode);

  // Serial.print("SCI_Status (0x48) = 0x");
  // Serial.println(MP3Status, HEX);

  int vsVersion = (MP3Status >> 4) & 0x000F; // Mask out only the four version bits
  printf("Version of VS1053: %d", vsVersion);

  // Serial.print("SCI_ClockF = 0x");
  // Serial.println(MP3Clock, HEX);

  // Now that we have the VS1053 up and running, increase the VS1053 internal clock multiplier and up our SPI rate
  Mp3WriteRegister(SCI_CLOCKF, 0x60, 0x00); // Set multiplier to 3.0x

  // From page 12 of datasheet, max SCI reads are CLKI/7. Input clock is 12.288MHz.
  // Internal clock multiplier is now 3x.
  // Therefore, max SPI speed is 5MHz. 4MHz will be safe.
  SSP0__set_max_clock(4000); // Set SPI bus speed to 4MHz (16MHz / 4 = 4MHz)

  MP3Clock = Mp3ReadRegister(SCI_CLOCKF);
  printf("SCICLK = 0x%x", MP3Clock);
}

void Mp3WriteRegister(unsigned char addressbyte, unsigned char highbyte, unsigned char lowbyte) {
  while (!gpio__lab_get_level(2, 0))
    ; // Wait for DREQ to go high indicating IC is available
  gpio__lab_set(2, 1, false);

  // SCI consists of instruction byte, address byte, and 16-bit data word.
  SSP0__exchange_byte(0x02); // Write instruction
  SSP0__exchange_byte(addressbyte);
  SSP0__exchange_byte(highbyte);
  SSP0__exchange_byte(lowbyte);
  while (!gpio__lab_get_level(2, 0))
    ; // Wait for DREQ to go high indicating command is complete
  gpio__lab_set(2, 1, true);
}

unsigned int Mp3ReadRegister(unsigned char addressbyte) {
  while (!gpio__lab_get_level(2, 0))
    ; // Wait for DREQ to go high indicating IC is available
  gpio__lab_set(2, 1, false);

  // SCI consists of instruction byte, address byte, and 16-bit data word.
  SSP0__exchange_byte(0x03); // Read instruction
  SSP0__exchange_byte(addressbyte);

  char response1 = SSP0__exchange_byte(0xFF); // Read the first byte
  while (!gpio__lab_get_level(2, 0))
    ;                                         // Wait for DREQ to go high indicating command is complete
  char response2 = SSP0__exchange_byte(0xFF); // Read the second byte
  while (!gpio__lab_get_level(2, 0))
    ; // Wait for DREQ to go high indicating command is complete

  gpio__lab_set(2, 1, true);

  int resultvalue = response1 << 8;
  resultvalue |= response2;
  return resultvalue;
}

void SPI_send_mp3_data(char byte) {
  // Once DREQ is released (high) we can now send a byte of data
  gpio__lab_set(2, 2, false); // Select Data
  SSP0__exchange_byte(byte);  // Send SPI byte
  gpio__lab_set(2, 2, true);  // Deselect Data
}
