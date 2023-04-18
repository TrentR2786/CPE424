// Test program for drawing rectangle

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/sync.h"
#include <math.h>
#include "hardware/adc.h"

// VGA mode struct defines video timing and size

//#define vga_mode vga_mode_640x480_60
#define vga_mode vga_mode_320x240_60
//#define vga_mode vga_mode_213x160_60
//#define vga_mode vga_mode_160x120_60
//#define vga_mode vga_mode_tft_800x480_50
//#define vga_mode vga_mode_tft_400x240_50


// Semaphore used to block code from proceeding unitl video is initialized
static semaphore_t video_initted;

// Offset value
static int16_t i = 0;

// Values to control speed
uint8_t i_frame = 1; // Number of frames until offset updates
uint8_t i_inc = 2; // Amount by which offset increases

// Functions to simplify code writing
void color_run(uint16_t* data, uint8_t r, uint8_t g, uint8_t b, uint16_t length) {
    data[0] = COMPOSABLE_COLOR_RUN;
    data[1] = PICO_SCANVIDEO_PIXEL_FROM_RGB5(r, g, b);
    data[2] = length - 3;
}

// Version of color_run function that draws one horizontal block
void draw_block(uint16_t* data, uint8_t r, uint8_t g, uint8_t b) {
    data[0] = COMPOSABLE_COLOR_RUN;
    data[1] = PICO_SCANVIDEO_PIXEL_FROM_RGB5(r, g, b);
    data[2] = 1;
}

// Function to set block size based on potentiometer input
uint16_t setBlockSize() {
    uint16_t pot_raw = adc_read();
    uint8_t pot = round(5 * (float)pot_raw / (1 << 12));

    switch(pot) {
        case 1: 
            return 64;
        case 2: 
            return 32;
        case 3: 
            return 16;
        case 4: 
            return 8;
        case 5: 
            return 4;
        default: // Input = 0 
            return 128;
    }
}

// Function to determine if square should be black
bool blackSquare(uint16_t x, uint16_t y, uint16_t block_size) {
    return (y % (2*block_size) < block_size && x % (block_size / 2) < (block_size/4)) || (y % (2*block_size) >= block_size && x % (block_size / 2) >= (block_size/4));
}


void draw(scanvideo_scanline_buffer_t *buffer) {

    uint16_t width = vga_mode.width;
    uint16_t w_blocks = width/4;
    uint16_t height = vga_mode.height;

    uint16_t y = scanvideo_scanline_number(buffer->scanline_id);

    uint16_t *p = (uint16_t *) buffer->data;

    uint16_t block_size = setBlockSize();

    for(int x = 0; x < w_blocks; x++) {
        if(blackSquare(x, y, block_size)) {
            draw_block(p, 0, 0, 0);
        } else {
            draw_block(p, 0x1f, 0x1f, 0x1f);
        }
        p += 3;
    }

    // 32 * 3, so we should be word aligned
    assert(!(3u & (uintptr_t) p));

    // Black pixel to end line (required to prevent color from bleeding into blanking)
    *p++ = COMPOSABLE_RAW_1P;
    *p++ = 0;
    // End of line with alignment padding
    // Use COMPOSABLE_EOL_ALIGN if number of tokens used is odd, COMPOSABLE_EOL_SKIP_ALIGH if even
    *p++ = COMPOSABLE_EOL_SKIP_ALIGN;
    *p++ = 0;

    // Set number of words used and check if it exceeds buffer size
    buffer->data_used = ((uint32_t *) p) - buffer->data;
    assert(buffer->data_used < buffer->data_max);

    // Set buffer status to be ready for use
    buffer->status = SCANLINE_OK;
}

void core1_func() {
    // Configure scanvideo code based on VGA mode
    scanvideo_setup(&vga_mode);
    // Turn on scanvideo code
    scanvideo_timing_enable(true);
    // Release semaphore
    sem_release(&video_initted);

    static uint32_t last_frame_num = 0;
    static uint8_t frame_count = 0;

    while (true) {
        // Generate scanline buffer
        scanvideo_scanline_buffer_t *scanline_buffer = scanvideo_begin_scanline_generation(true);

        // Update offset every new frame
        uint32_t frame_num = scanvideo_frame_number(scanline_buffer->scanline_id);
        if(frame_num != last_frame_num) {
            last_frame_num = frame_num;
            frame_count++;

            if(frame_count % i_frame == 0) {
                 i += i_inc;
                 frame_count = 0;
            }
        }

        // Draw pixels to buffer
        draw(scanline_buffer);
        // Pass buffer to scanvideo code
        scanvideo_end_scanline_generation(scanline_buffer);
    }
}

int main(void) {
    // Initialize semaphore
    sem_init(&video_initted, 0, 1);
    // Initialize ADC for potentiometer
    adc_init();
    adc_gpio_init(26);
    adc_select_input(0);
    // Run code on core 1
    multicore_launch_core1(core1_func);
    // Wait for video initialization to complete
    sem_acquire_blocking(&video_initted);

    while(true) {
        // Run additional code/controls here
    }
}

