
#include "FreeRTOS.h"
#include "SSD1306.h"
#include "acceleration.h"
#include "cli_handlers.h"
#include "clock.h"
#include "delay.h"
#include "event_groups.h"
#include "ff.h"
#include "gpio.h"
#include "gpio_lab.h"
#include "i2c2_slave_functions.h"
#include "i2c_slave_init.h"
#include "lpc40xx.h"
#include "lpc_peripherals.h"
#include "mp3.h"
#include "queue.h"
#include "sj2_cli.h"
#include "song_list.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

// void configure_uart2_pin_functions(void);
// void uart_read_task(void *p);
// void uart_write_task(void *p);
// void board_1_sender_task(void *p);
// void board_2_receiver_task(void *p);
// void producer(void *p);
// void consumer(void *p);
// void watchdog_task(void *p);
// void configure_sw3(void);
// static void task_one(void *task_parameter);

// typedef enum { switch__off, switch__on } switch_e;

// switch_e get_switch_input_from_switch0(void);

// static QueueHandle_t switch_queue;

// lab 9 Watchdog
// part 0
/*
static QueueHandle_t sensor_queue;
static EventGroupHandle_t event_group;
#define BIT_0 (1 << 0)
#define BIT_1 (1 << 1)
*/
void mp3_reader_task(void *p);
void mp3_player_task(void *p);
void setup_volume_ctrl_sws();

QueueHandle_t Q_songdata;
QueueHandle_t Q_trackname;
int main(void) {
  // if (acceleration__init) {
  // xTaskCreate(producer, "producer", (4096 * 4) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  // xTaskCreate(consumer, "consumer", (4096 * 4) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  // xTaskCreate(watchdog_task, "watchdog_task", (4096 * 4) / sizeof(void *), NULL, PRIORITY_HIGH, NULL);
  // sensor_queue =
  // xQueueCreate(1, sizeof(float)); // Choose depth of item being our enum (1 should be okay for this example)
  // event_group = xEventGroupCreate();
  //}

  // set mode to no pull up no pull down
  // LPC_IOCON->P0_1 &= ~(3 << 3);
  // LPC_IOCON->P0_0 &= ~(3 << 3);
  /*
  const gpio_s sda_0 = gpio__construct_with_function(GPIO__PORT_0, 0, GPIO__FUNCTION_3);
  const gpio_s scl_0 = gpio__construct_with_function(GPIO__PORT_0, 1, GPIO__FUNCTION_3);
  gpio__enable_open_drain(sda_0);
  gpio__enable_open_drain(scl_0);
  */

  /*
    i2c2__slave_init(0x20);

    while (1) {
      printf("Slave memory 0: %d, Slave memory 1: %d\n ", slave_memory[0], slave_memory[1]);
      delay__ms(2000);
    }
*/

  song_list__populate();
  delay__ms(10);
  SSD1306_Init();
  SSD1306_Clear();
  SSD1306_InvertDisplay(true);
  int i = 0;
  while (i < 8) {
    if (i == 0) {
      SSD1306_SetPageStartAddr(i);
      SSD1306_SetColStartAddr(15);
    } else {
      SSD1306_SetPageStartAddr(i);
      SSD1306_SetColStartAddr(0);
    }
    char song_name[24];
    strncpy(song_name, song_list__get_name_for_item(i), 23);
    printf("Song name for index %d = %s\n", i, song_list__get_name_for_item(i));
    printf("Strncpy version = %s \n \n", song_name);
    SSD1306_PrintString(song_name);
    i++;
  }

  // SSD1306_startscrollright(0x00, 0x01);

  // SSD1306_startscrollright(0x00, 0x00);

  Q_trackname = xQueueCreate(1, sizeof(trackname_t));
  Q_songdata = xQueueCreate(1, 512);

  init_SPI();
  mp3_setup();
  sj2_cli__init();
  xTaskCreate(mp3_reader_task, "reader", (4096 * 4) / sizeof(void *), NULL, PRIORITY_LOW, NULL);
  xTaskCreate(mp3_player_task, "player", (4096 * 4) / sizeof(void *), NULL, PRIORITY_MEDIUM, NULL);

  vTaskStartScheduler();

  /*
      gpio__construct_with_function(GPIO__PORT_0, 10, GPIO__FUNCITON_0_IO_PIN);
      gpio0__set_as_output(10);
      gpio__construct_with_function(GPIO__PORT_0, 29, GPIO__FUNCITON_0_IO_PIN);
      gpio0__set_as_input(29);
      while (1) {
        if (gpio0__get_level(29)) {
          gpio0__set_high(10);
          printf("Input is high \t");
        } else {
          gpio0__set_low(10);
          printf("Input is low \t");
        }
      }


    gpio__construct_with_function(GPIO__PORT_1, 4, GPIO__FUNCITON_0_IO_PIN);
    gpio__lab__set_as_output(1, 4);
    gpio__construct_with_function(GPIO__PORT_0, 29, GPIO__FUNCITON_0_IO_PIN);
    gpio0__set_as_input(29);
    while (1) {
      if (gpio0__get_level(29)) {
        gpio__lab_set_high(1, 4);
        printf("Input is high \t");
      } else {
        gpio__lab_set_low(1, 4);
        printf("Input is low \t");
      }
    }
  */

  return 0;
}
void setup_volume_ctrl_sws(){
    //p0.29 (SW3) -> volume up
    gpio__construct_with_function(GPIO__PORT_0,29,GPIO__FUNCITON_0_IO_PIN);
    gpio__lab__set_as_input(0,29);

    //p0.30 (SW2) -> volume down
    gpio__construct_with_function(GPIO__PORT_0,30,GPIO__FUNCITON_0_IO_PIN);
    gpio__lab__set_as_input(0,30);
}

void mp3_reader_task(void *p) {
  trackname_t name;
  char bytes_512[512];
  UINT br;
  while (1) {
    xQueueReceive(Q_trackname, name, portMAX_DELAY);
    printf("Received song to play: %s\n", name);

    // open file
    const char *filename = name; // check if accurate
    FIL file;
    FRESULT result = f_open(&file, filename, (FA_READ));
    if (FR_OK == result) {
      while (br != 0) {
        // read 512 bytes from file to bytes_512
        f_read(&file, bytes_512, sizeof(bytes_512), &br);
        xQueueSend(Q_songdata, bytes_512, portMAX_DELAY);

        if (uxQueueMessagesWaiting(Q_trackname)) {
          printf("New play song request\n");
          break;
        }
      }
      // close file
      f_close(&file);
    } else {
      printf("Failed to open mp3 file \n");
    }
  }
}

void mp3_player_task(void *p) {
  int counter = 1;
  while (1) {
    char bytes_512[512];
    xQueueReceive(Q_songdata, bytes_512, portMAX_DELAY);
    for (int i = 0; i < 512; i++) {
      // printf("%x \t", bytes_512[i]);
      while (!gpio__lab_get_level(2, 0)) {
        vTaskDelay(1);
      }
      SPI_send_mp3_data(bytes_512[i]);
    }
    printf("Received 512 bytes : %d\n", counter);
    counter++;
  }
}

/*
void uart_read_task(void *p) {
  while (1) {
    // TODO: Use uart_lab__polled_get() function and printf the received value
    char input_char;
    if (uart_lab__get_char_from_queue(UART_2, &input_char)) {
      fprintf(stderr, "Received char: %c \n", input_char);
    }
    vTaskDelay(500);
  }
}

void uart_write_task(void *p) {
  while (1) {
    // TODO: Use uart_lab__polled_put() function and send a value
    char output_char = 'c';
    uart_lab__polled_put(UART_2, output_char);
    vTaskDelay(500);
  }
}

void configure_uart2_pin_functions(void) {
  // congfigure tx
  gpio__construct_with_function(GPIO__PORT_2, 8, GPIO__FUNCTION_2);
  // configure rx
  gpio__construct_with_function(GPIO__PORT_2, 9, GPIO__FUNCTION_2);
}


// This task is done for you, but you should understand what this code is doing
void board_1_sender_task(void *p) {
  char number_as_string[16] = {0};

  while (true) {
    const int number = rand();
    sprintf(number_as_string, "%i", number);

    // Send one char at a time to the other board including terminating NULL char
    for (int i = 0; i <= strlen(number_as_string); i++) {
      uart_lab__polled_put(UART_2, number_as_string[i]);
      printf("Sent: %c\n", number_as_string[i]);
    }

    printf("Sent: %i over UART to the other board\n", number);
    vTaskDelay(3000);
  }
}

void board_2_receiver_task(void *p) {
  char number_as_string[11] = {0};
  int counter = 0;
  while (true) {
    char byte = 0;
    uart_lab__get_char_from_queue(&byte, portMAX_DELAY);
    printf("Received: %c\n", byte);
    // This is the last char, so print the number
    if ('\0' == byte) {
      number_as_string[counter] = '\0';
      counter = 0;
      printf("Received this number from the other board: %s\n", number_as_string);
    }
    // We have not yet received the NULL '\0' char, so buffer the data
    else {
      if (counter != 5) {
        number_as_string[counter] = byte;
        counter++;
      }
      // TODO: Store data to number_as_string[] array one char at a time
      // Hint: Use counter as an index, and increment it as long as we do not reach max value of 16
    }
  }
}

void producer(void *p) {
  switch_e switch_value;
  while (1) {

    // Get some input value from your board
    switch_value = get_switch_input_from_switch0();

    // Print a message before xQueueSend()
    if (switch_value == switch__on) {
      printf("About to send switch on to queue \n");
    } else {
      printf("About to send switch off to queue \n");
    }

    xQueueSend(switch_queue, &switch_value, 0);

    // Print a message after xQueueSend()
    printf("Sent item to queue \n");

    vTaskDelay(1000);
  }
}

void consumer(void *p) {
  switch_e x;
  while (1) {
    // Print a message before xQueueReceive()
    printf("About to check queue \n");

    xQueueReceive(switch_queue, &x, portMAX_DELAY);

    // Print a message after xQueueReceive()
    if (x == switch__on) {
      printf("Received switch on from queue \n");
    } else if (x == switch__off) {
      printf("Received switch off from queue \n");
    }
  }
}

switch_e get_switch_input_from_switch0(void) {
  if (gpio0__get_level(29)) {
    return switch__on;
  }
  return switch__off;
}

void configure_sw3(void) {
  gpio__construct_with_function(GPIO__PORT_0, 29, GPIO__FUNCITON_0_IO_PIN);
  gpio0__set_as_input(29);
}

static void task_one(void *task_parameter) {
  while (true) {
    fprintf(stderr, "AAAAAAAAAAAA");

    // Sleep for 100ms
    vTaskDelay(100000);
  }
}


void producer(void *p) {
  while (1) {
    acceleration__axis_data_s sensor_data;
    float average_z = 0;
    int i = 0;
    while (xTaskGetTickCount() % 100 != 0) {
      sensor_data = acceleration__get_data();
      average_z += sensor_data.z;
    }
    average_z = average_z / 100; /// 100;
    // printf("before Send %f\n", average_z);
    xQueueSend(sensor_queue, &average_z, 0);
    // printf("after Send %f\n", average_z);
    xEventGroupSetBits(event_group, BIT_0);
    vTaskDelay(100);
  }
}

void consumer(void *p) {
  float average = 0;
  while (1) {
    // printf("before Receive %f\n", average);
    xQueueReceive(sensor_queue, &average, portMAX_DELAY);
    // printf("after Receive %f\n", average);
    const char *filename = "file.txt";
    FIL file; // File handle
    UINT bytes_written = 0;
    FRESULT result = f_open(&file, filename, (FA_WRITE | FA_OPEN_APPEND));
    if (FR_OK == result) {
      char string[64];
      sprintf(string, "%f\t", average);
      if (FR_OK == f_write(&file, string, strlen(string), &bytes_written)) {
        // printf("Wrote data: %f ... %u bytes written", average, bytes_written);
      } else {
        printf("ERROR: Failed to write data to file\n");
      }
      f_close(&file);
    } else {
      printf("ERROR: Failed to open: %s\n", filename);
    }
    xEventGroupSetBits(event_group, BIT_1);
  }
}

void watchdog_task(void *p) {
  while (1) {
    // vTaskDelay(200);
    EventBits_t uxBits;
    uxBits = xEventGroupWaitBits(event_group, BIT_0 | BIT_1, pdTRUE, pdTRUE, 200);
    if ((uxBits & (BIT_0 | BIT_1)) == (BIT_0 | BIT_1)) {
      printf("producer and consumer running normally\n");
    } else if ((uxBits & BIT_0) != 0) {
      printf("consumer missed\n");
    } else if ((uxBits & BIT_1) != 0) {
      printf("producer missed\n");
      const char *filename = "file.txt";
      FIL file; // File handle
      UINT bytes_written = 0;
      FRESULT result = f_open(&file, filename, (FA_WRITE | FA_OPEN_APPEND));
      if (FR_OK == result) {
        char string[64];
        sprintf(string, "Producer Missed");
        if (FR_OK == f_write(&file, string, strlen(string), &bytes_written)) {
          // printf("%u bytes written", bytes_written);
        } else {
          printf("ERROR: Failed to write data to file\n");
        }
        f_close(&file);
      } else {
        printf("ERROR: Failed to open: %s\n", filename);
      }
    } else {
      printf("consumer and producer missed\n");
    }
  }
}
*/