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

// Offset value for sinusoidal demo
static int16_t i = 0;

// Values to control speed of sinusoidal demo
static uint8_t i_frame = 1; // Number of frames until offset updates
static uint8_t i_inc = 2; // Amount by which offset increases

// Draw one horizontal block (must increment data pointer by 3 after calling)
void draw_block(uint16_t* data, uint8_t r, uint8_t g, uint8_t b) {
    data[0] = COMPOSABLE_COLOR_RUN;
    data[1] = PICO_SCANVIDEO_PIXEL_FROM_RGB5(r, g, b);
    data[2] = 1;
}

// Set block size for checkerboard demo based on potentiometer input
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

// Determine if square in checkerboard demo should be black
bool blackSquare(uint16_t x, uint16_t y, uint16_t block_size) {
    return (y % (2*block_size) < block_size && x % (block_size / 2) < (block_size/4)) || (y % (2*block_size) >= block_size && x % (block_size / 2) >= (block_size/4));
}

// Write pixel data for checkerboard demo
void draw_checkerboard(scanvideo_scanline_buffer_t *buffer) {

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

// Function to update speed of sinusoidal demo based on potentiometer input
void updateSpeed() {
    uint16_t pot_raw = adc_read();
    uint8_t pot = round(4 * (float)pot_raw / (1 << 12));

    switch(pot) {
        case 1: 
            // 1.5x speed 
            i_frame = 2;
            i_inc = 3;
        case 2: 
            // 1x speed 
            i_frame = 1;
            i_inc = 1;
        case 3: 
            // 0.5x speed 
            i_frame = 2;
            i_inc = 1;
        case 4: 
            // 0.25x speed 
            i_frame = 4;
            i_inc = 1;
        default: // Input = 0
            // 2x speed 
            i_frame = 1;
            i_inc = 2;
    }
}

void draw_sine(scanvideo_scanline_buffer_t *buffer) {

    updateSpeed();

    uint16_t width = vga_mode.width;
    uint16_t w_blocks = width/4;
    uint16_t height = vga_mode.height;

    uint16_t y = scanvideo_scanline_number(buffer->scanline_id);

    uint16_t *p = (uint16_t *) buffer->data;

    for(int x = 0; x < w_blocks; x++) {
        double rf = fabs(cos(2*(double)(y+i) * M_PI / 180));
        double bf = fabs(cos(2*(double)(y-i) * M_PI / 180));
        uint8_t r = round(0x1f * rf);
        uint8_t b = round(0x1f * bf);
        draw_block(p, r, 0, b);
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

// Code sent to core 1 (handles drawing to screen)
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

        // Check if frame has updated and try to increment offset
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
        draw_sine(scanline_buffer);
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

