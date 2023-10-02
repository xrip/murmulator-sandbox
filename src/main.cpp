/* See LICENSE file for license details */

/* Standard library includes */

#include <cstring>

#include "pico/runtime.h"  // overclock
#include "pico/stdlib.h"  // overclock
#include "hardware/vreg.h"
#include "hardware/flash.h"

#include "vga.h"
#include "ps2kbd_mrmltr.h"
#include "nespad.h"
#include "f_util.h"
#include "ff.h"
#include "VGA_ROM_F16.h"

#pragma GCC optimize("Ofast")

#define FLASH_TARGET_OFFSET (1024 * 1024)
const char *rom_filename = (const char*) (XIP_BASE + FLASH_TARGET_OFFSET);
const uint8_t *rom = (const uint8_t *) (XIP_BASE + FLASH_TARGET_OFFSET)+4096;

static FATFS fs;
static const sVmode *vmode = nullptr;
struct semaphore vga_start_semaphore;
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
uint8_t SCREEN[SCREEN_WIDTH * SCREEN_HEIGHT];
char textmode[30][80];
uint8_t colors[30][80];

typedef enum {
    RESOLUTION_NATIVE,
    RESOLUTION_TEXTMODE,
} resolution_t;
resolution_t resolution = RESOLUTION_TEXTMODE;

struct input_bits_t {
    bool a: true;
    bool b: true;
    bool select: true;
    bool start: true;
    bool right: true;
    bool left: true;
    bool up: true;
    bool down: true;
};
static input_bits_t keyboard_bits = { false, false, false, false, false, false, false, false };

static bool isInReport(hid_keyboard_report_t const *report, const unsigned char keycode) {
    for (unsigned char i: report->keycode) {
        if (i == keycode) {
            return true;
        }
    }
    return false;
}

void __not_in_flash_func(process_kbd_report)(hid_keyboard_report_t const *report, hid_keyboard_report_t const *prev_report) {
/*    printf("HID key report modifiers %2.2X report ", report->modifier);
    for (unsigned char i: report->keycode)
        printf("%2.2X", i);
    printf("\r\n");*/

    keyboard_bits.start = isInReport(report, HID_KEY_ENTER);
    keyboard_bits.select = isInReport(report, HID_KEY_BACKSPACE);
    keyboard_bits.a = isInReport(report, HID_KEY_Z);
    keyboard_bits.b = isInReport(report, HID_KEY_X);
    keyboard_bits.up = isInReport(report, HID_KEY_ARROW_UP);
    keyboard_bits.down = isInReport(report, HID_KEY_ARROW_DOWN);
    keyboard_bits.left = isInReport(report, HID_KEY_ARROW_LEFT);
    keyboard_bits.right = isInReport(report, HID_KEY_ARROW_RIGHT);
}

Ps2Kbd_Mrmltr ps2kbd(
        pio1,
        0,
        process_kbd_report);


void draw_text(char *text, uint8_t x, uint8_t y, uint8_t color, uint8_t bgcolor) {
    uint8_t len = strlen(text);
    len = len < 80 ? len : 80;
    memcpy(&textmode[y][x], text, len);
    memset(&colors[y][x], (color << 4) | (bgcolor & 0xF), len);
}

/**
 * Load a .gb rom file in flash from the SD card
 */
void load_cart_rom_file(char *filename) {
// Remove in real world
#if 1
    return;
#endif

    if (strcmp(rom_filename, filename) == 0) {
        printf("Launching last rom");
        return;
    }

    FIL fil;
    FRESULT fr;

    size_t bufsize = sizeof(SCREEN);
    BYTE *buffer = (BYTE *) SCREEN;
    auto ofs = FLASH_TARGET_OFFSET;
    printf("Writing %s rom to flash %x\r\n", filename, ofs);
    fr = f_open(&fil, filename, FA_READ);

    UINT bytesRead;
    if (fr == FR_OK) {
        uint32_t ints = save_and_disable_interrupts();
        multicore_lockout_start_blocking();

        // TODO: Save it after success loading to prevent corruptions
        printf("Flashing %d bytes to flash address %x\r\n", 256, ofs);
        flash_range_erase(ofs, 4096);
        flash_range_program(ofs, reinterpret_cast<const uint8_t *>(filename), 256);

        ofs += 4096;
        for (;;) {
            fr = f_read(&fil, buffer, bufsize, &bytesRead);
            if (fr == FR_OK) {
                if (bytesRead == 0) {
                    break;
                }

                printf("Flashing %d bytes to flash address %x\r\n", bytesRead, ofs);

                printf("Erasing...");
                // Disable interupts, erase, flash and enable interrupts
                flash_range_erase(ofs, bufsize);
                printf("  -> Flashing...\r\n");
                flash_range_program(ofs, buffer, bufsize);

                ofs += bufsize;
            } else {
                printf("Error reading rom: %d\n", fr);
                break;
            }
        }


        f_close(&fil);
        restore_interrupts(ints);
        multicore_lockout_end_blocking();
    }
}


/**
 * Function used by the rom file selector to display one page of .gb rom files
 */
uint16_t rom_file_selector_display_page(char filename[28][256], uint16_t num_page) {
    // Dirty screen cleanup
    memset(&textmode, 0x00, sizeof(textmode));
    memset(&colors, 0x00, sizeof(colors));
    char footer[80];
    sprintf(footer, "=================== PAGE #%i -> NEXT PAGE / <- PREV. PAGE ====================", num_page);
    draw_text(footer, 0, 14, 3, 11);

    DIR dj;
    FILINFO fno;
    FRESULT fr;

    fr = f_mount(&fs, "", 1);
    if (FR_OK != fr) {
        printf("E f_mount error: %s (%d)\n", FRESULT_str(fr), fr);
        return 0;
    }

    /* clear the filenames array */
    for (uint8_t ifile = 0; ifile < 14; ifile++) {
        strcpy(filename[ifile], "");
    }

    /* search *.gb files */
    uint16_t num_file = 0;
    fr = f_findfirst(&dj, &fno, "", "*");

    /* skip the first N pages */
    if (num_page > 0) {
        while (num_file < num_page * 14 && fr == FR_OK && fno.fname[0]) {
            num_file++;
            fr = f_findnext(&dj, &fno);
        }
    }

    /* store the filenames of this page */
    num_file = 0;
    while (num_file < 14 && fr == FR_OK && fno.fname[0]) {
        strcpy(filename[num_file], fno.fname);
        num_file++;
        fr = f_findnext(&dj, &fno);
    }
    f_closedir(&dj);

    for (uint8_t ifile = 0; ifile < num_file; ifile++) {
        draw_text(filename[ifile], 0, ifile, 0xFF, 0x00);
    }
    return num_file;
}

/**
 * The ROM selector displays pages of up to 22 rom files
 * allowing the user to select which rom file to start
 * Copy your *.gb rom files to the root directory of the SD card
 */
void rom_file_selector() {
    uint16_t num_page = 0;
    char filenames[30][256];

    printf("Selecting ROM\r\n");

    /* display the first page with up to 22 rom files */
    uint16_t numfiles = rom_file_selector_display_page(filenames, num_page);

    /* select the first rom */
    uint8_t selected = 0;
    draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);

    while (true) {
        nespad_read();
        ps2kbd.tick();
        sleep_ms(33);
        nespad_read();
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
        if ((nespad_state & DPAD_START) != 0 || (nespad_state & DPAD_A) != 0 || (nespad_state & DPAD_B) != 0 || keyboard_bits.start) {
            /* copy the rom from the SD card to flash and start the game */
            char pathname[255];
            sprintf(pathname, "SEGA\\%s", filenames[selected]);
            load_cart_rom_file(pathname);
            break;
        }

        if ((nespad_state & DPAD_DOWN) != 0 || keyboard_bits.down) {
            /* select the next rom */
            draw_text(filenames[selected], 0, selected, 0xFF, 0x00);
            selected++;
            if (selected >= numfiles)
                selected = 0;
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            printf("Rom %s\r\n", filenames[selected]);
            sleep_ms(150);
        }
        if ((nespad_state & DPAD_UP) != 0 || keyboard_bits.up) {
            /* select the previous rom */
            draw_text(filenames[selected], 0, selected, 0xFF, 0x00);
            if (selected == 0) {
                selected = numfiles - 1;
            } else {
                selected--;
            }
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            printf("Rom %s\r\n", filenames[selected]);
            sleep_ms(150);
        }
        if ((nespad_state & DPAD_RIGHT) != 0 || keyboard_bits.right) {
            /* select the next page */
            num_page++;
            numfiles = rom_file_selector_display_page(filenames, num_page);
            if (numfiles == 0) {
                /* no files in this page, go to the previous page */
                num_page--;
                numfiles = rom_file_selector_display_page(filenames, num_page);
            }
            /* select the first file */
            selected = 0;
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            sleep_ms(150);
        }
        if ((nespad_state & DPAD_LEFT) != 0 && num_page > 0 || keyboard_bits.left) {
            /* select the previous page */
            num_page--;
            numfiles = rom_file_selector_display_page(filenames, num_page);
            /* select the first file */
            selected = 0;
            draw_text(filenames[selected], 0, selected, 0xFF, 0xF8);
            sleep_ms(150);
        }
        tight_loop_contents();
    }
}

#define X2(a) (a | (a << 8))
#define CHECK_BIT2(var, pos) (((var)>>(pos)) & 1)

/* Renderer loop on Pico's second core */
void __time_critical_func(render_loop)() {
    multicore_lockout_victim_init();
    VgaLineBuf *linebuf;
    printf("Video on Core#%i running...\n", get_core_num());

    sem_acquire_blocking(&vga_start_semaphore);
    VgaInit(vmode, 640, 480);
    bool sync = false;

    while (linebuf = get_vga_line()) {
        uint32_t y = linebuf->row;

        switch (resolution) {
            case RESOLUTION_TEXTMODE:
                for (uint8_t x = 0; x < 80; x++) {
                    uint8_t glyph_row = VGA_ROM_F16[(textmode[y / 16][x] * 16) + y % 16];
                    uint8_t color = colors[y / 16][x];

                    for (uint8_t bit = 0; bit < 8; bit++) {
                        if (CHECK_BIT2(glyph_row, bit)) {
                            // FOREGROUND
                            linebuf->line[8 * x + bit] = (color >> 4) & 0xF;
                        } else {
                            // BACKGROUND
                            linebuf->line[8 * x + bit] = color & 0xF;
                        }
                    }
                }
                break;
            case RESOLUTION_NATIVE:
                if (y < SCREEN_HEIGHT)
                    memcpy(&linebuf->line, SCREEN, 640);
        }
    }

    __builtin_unreachable();
}

bool reset = false;
void main_loop() {
    while (1) {
        // Outer loop to intit something after "reset" state

        draw_text("WELCOME TO MURMULATOR :)", 30, 8, 0xff, 0);

        while (!reset) {
            // All program logic goes here
            tight_loop_contents();
        }
        reset = false;
    }
    __builtin_unreachable();
}

/******************************************************************************
 * Main code entry point
 *****************************************************************************/
//unsigned char *ROM_DATA;
int main() {
    /* Overclock. */
    vreg_set_voltage(VREG_VOLTAGE_1_15);
    set_sys_clock_khz(288000, true);

    stdio_init_all();

    sleep_ms(50);
    vmode = Video(DEV_VGA, RES_HVGA);
    sleep_ms(50);

    ps2kbd.init_gpio();
    nespad_begin(clock_get_hz(clk_sys) / 1000, NES_GPIO_CLK, NES_GPIO_DATA, NES_GPIO_LAT);

    sem_init(&vga_start_semaphore, 0, 1);
    multicore_launch_core1(render_loop);
    sem_release(&vga_start_semaphore);

    sleep_ms(100);

    rom_file_selector();
    memset(&textmode, 0x00, sizeof(textmode));
    memset(&colors, 0x00, sizeof(colors));
//    resolution = RESOLUTION_NATIVE;

    main_loop();

    return 1;
}