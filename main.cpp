/* See LICENSE file for license details */

/* Standard library includes */
#include <stdio.h>
#include <string.h>

/* Pico libs*/
#include "pico/time.h"
#include "pico/sem.h"
#include "pico/multicore.h"
#include "hardware/vreg.h"

/* Murmulator Specific Libs*/
#include "vga.h"
#include "ps2kbd_mrmltr.h"
#include "ff.h"

#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 144
#define LED_PIN 25

static FATFS fs;
void __not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const *report, hid_keyboard_report_t const *prev_report)
{
    printf("HID key report modifiers %2.2X report ", report->modifier);
    for (int i = 0; i < 6; ++i)
        printf("%2.2X", report->keycode[i]);
    printf("\n");
}
static Ps2Kbd_Mrmltr ps2kbd(pio1, PS2KBD_GPIO_FIRST, process_kbd_report);

static const sVmode *vmode = NULL;
struct semaphore vga_start_semaphore;

static uint32_t screen[SCREEN_WIDTH * SCREEN_HEIGHT];

/* Renderer loop on Pico's second core */
void __time_critical_func(render_loop)()
{
    // Allow core to be locked from other core
    multicore_lockout_victim_init();

    VgaLineBuf *linebuf;
    printf("Render on Core#%i running...\n", get_core_num());

    sem_acquire_blocking(&vga_start_semaphore);
    VgaInit(vmode, 640, 480);

    while ((linebuf = get_vga_line()))
    {
        memcpy(&linebuf->line, &screen[linebuf->row * 160], 160 * 4);
    }

    __builtin_unreachable();
}

/* Main program loop on Pico's first core */
void __time_critical_func(main_loop)()
{
    printf("Main loop on Core#%i running...\n", get_core_num());

    for (;;)
    {
        ps2kbd.tick();
        //            memcpy(&screen[(line_count * SCREEN_WIDTH)], tia_line_buffer, TIA_COLOUR_CLOCK_VISIBLE * 4);
    }
    __builtin_unreachable();
}

/******************************************************************************
 * Main code entry point
 *****************************************************************************/

bool initSDCard()
{
    FRESULT fr;
    DIR dir;
    FILINFO file;

    sleep_ms(1000);

    printf("Mounting SDcard");
    fr = f_mount(&fs, "", 1);
    if (fr != FR_OK)
    {
        printf("SD card mount error: %d", fr);

        return false;
    }
    printf("\n");

    fr = f_chdir("/");
    if (fr != FR_OK)
    {
        printf("Cannot change dir to / : %d\r\n", fr);
        return false;
    }

    printf("Listing %s\n", ".");

    f_opendir(&dir, ".");
    while (f_readdir(&dir, &file) == FR_OK && file.fname[0])
    {
        printf("%s", file.fname);
    }

    f_closedir(&dir);

    return true;
}

int main()
{
    /* Overclock. */
    {
        const unsigned vco = 1596 * 1000 * 1000; /* 266MHz */
        const unsigned div1 = 6, div2 = 1;

        vreg_set_voltage(VREG_VOLTAGE_1_15);
        sleep_ms(2);
        set_sys_clock_pll(vco, div1, div2);
        sleep_ms(2);
    }
    stdio_init_all();

    gpio_init(LED_PIN);
    gpio_set_dir(LED_PIN, GPIO_OUT);

    vmode = Video(DEV_VGA, RES_HVGA);

    sleep_ms(50);
    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_loop);
    sem_release(&vga_start_semaphore);

    gpio_put(LED_PIN, 1);

    ps2kbd.init_gpio();

    main_loop();
}