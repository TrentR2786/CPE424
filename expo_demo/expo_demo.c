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
//#define vga_mode vga_mode_320x240_60
//#define vga_mode vga_mode_213x160_60
#define vga_mode vga_mode_160x120_60
//#define vga_mode vga_mode_tft_800x480_50
//#define vga_mode vga_mode_tft_400x240_50


// Semaphore used to block code from proceeding unitl video is initialized
static semaphore_t video_initted;

// Draw one horizontal block (must increment data pointer by 3 after calling)
void draw_block(uint16_t* data, uint8_t r, uint8_t g, uint8_t b) {
    data[0] = COMPOSABLE_COLOR_RUN;
    data[1] = PICO_SCANVIDEO_PIXEL_FROM_RGB5(r, g, b);
    data[2] = 1;
}

// Offset value for animated demos
static uint16_t i = 0;

// Functions to update speed of animated demos on potentiometer input
uint8_t speedFrame() {
    adc_select_input(0);
    uint16_t pot_raw = adc_read();
    uint8_t pot = round(4 * (float)pot_raw / (1 << 12));

    switch(pot) {
        case 1: 
            // 1.5x speed 
            return 2;
        case 2: 
            // 1x speed 
            return 1;
        case 3: 
            // 0.5x speed 
            return 2;
        case 4: 
            // 0.25x speed 
            return 4;
        default: // Input = 0
            // 2x speed 
            return 1;
    }
}

uint8_t speedInc() {
    adc_select_input(0);
    uint16_t pot_raw = adc_read();
    uint8_t pot = round(4 * (float)pot_raw / (1 << 12));

    switch(pot) {
        case 1: 
            // 1.5x speed 
            return 3;
        case 2: 
            // 1x speed 
            return 1;
        case 3: 
            // 0.5x speed 
            return 1;
        case 4: 
            // 0.25x speed 
            return 1;
        default: // Input = 0
            // 2x speed 
            return 2;
    }
}

// Write pixel data for test pattern demo
// Modified version of https://github.com/raspberrypi/pico-playground/blob/master/scanvideo/test_pattern/test_pattern.c 
void draw_pattern(scanvideo_scanline_buffer_t *buffer) {
    // figure out 1/32 of the color value
    uint line_num = scanvideo_scanline_number(buffer->scanline_id);
    uint32_t primary_color = 1u + (line_num * 7 / vga_mode.height);

    adc_select_input(0);
    uint16_t pot_raw = adc_read();
    uint8_t pot = round(0x1f * (float)pot_raw / (1 << 12));

    uint32_t color_mask = PICO_SCANVIDEO_PIXEL_FROM_RGB5(0x1f * (primary_color & 1u), 0x1f * ((primary_color >> 1u) & 1u), 0x1f * ((primary_color >> 2u) & 1u));
    uint bar_width = vga_mode.width / 32;

    uint16_t *p = (uint16_t *) buffer->data;

    for (uint bar = 0; bar < 32; bar++) {
        *p++ = COMPOSABLE_COLOR_RUN;
        uint32_t color = PICO_SCANVIDEO_PIXEL_FROM_RGB5(bar & pot, bar & pot, bar & pot);
        *p++ = color & color_mask;
        *p++ = bar_width - 3;
    }

    // 32 * 3, so we should be word aligned
    assert(!(3u & (uintptr_t) p));

    // black pixel to end line
    *p++ = COMPOSABLE_RAW_1P;
    *p++ = 0;
    // end of line with alignment padding
    *p++ = COMPOSABLE_EOL_SKIP_ALIGN;
    *p++ = 0;

    buffer->data_used = ((uint32_t *) p) - buffer->data;
    assert(buffer->data_used < buffer->data_max);

    buffer->status = SCANLINE_OK;
}

// Write pixel data for box demo
void draw_box(scanvideo_scanline_buffer_t *buffer) {

    uint16_t w_blocks = vga_mode.width/4;
    uint16_t height = vga_mode.height;
    uint16_t y = scanvideo_scanline_number(buffer->scanline_id);

    uint16_t *p = (uint16_t *) buffer->data;

    if(i > 2*w_blocks/3) {
        i = 0;
    }

    for(int x = 0; x < w_blocks; x++) {
        if((y >= height/3 && y <= 2*height/3) && (x >= i && x <= ((w_blocks / 3) + i))) {
            draw_block(p, 0x1f, 0x1f, 0);
        } else {
            draw_block(p, 0x1f, 0, 0x1f);
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

// Set block size for checkerboard demo based on potentiometer input
uint16_t setBlockSize() {
    adc_select_input(0);
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

    uint16_t w_blocks = vga_mode.width/4;
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

// Write pixel data for sinusoidal demo
void draw_sine(scanvideo_scanline_buffer_t *buffer) {

    uint16_t w_blocks = vga_mode.width/4;
    uint16_t y = scanvideo_scanline_number(buffer->scanline_id);

    uint16_t *p = (uint16_t *) buffer->data;

    for(int x = 0; x < w_blocks; x++) {
        double rf = fabs(cos(2*(double)(y+i) * M_PI / 180));
        double gf = fabs(cos(2*(double)(y-i) * M_PI / 180));
        uint8_t r = round(0x1f * rf);
        uint8_t g = round(0x1f * gf);
        draw_block(p, r, g, 0x1f);
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

    // Values to control frame updates/toggling demos
    static uint32_t last_frame_num = 0;
    static uint8_t frame_count = 0;

    while (true) {

        // Values to control speed of sinusoidal demo
        // Speed factor = i_inc/i_frame
        uint8_t i_inc = speedInc(); // Amount by which offset increases
        uint8_t i_frame = speedFrame(); // Number of frames until offset updates

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

            // Keep offset value low so that sinusoidal demo does not struggle to keep up
            if(i >= 10000) {
                i = 0;
            }
        }

        // Select demo based on potentiometer input
        adc_select_input(1);
        uint16_t pot_raw = adc_read();
        uint8_t pot = round(3*(float)pot_raw / (1 << 12));

        // Draw pixels to buffer
        /*
        switch(pot) {
            case 2:
                draw_box(scanline_buffer);
            case 1:
                draw_checkerboard(scanline_buffer);
            default: // Input = 0
                draw_sine(scanline_buffer);
        }
        */
       if(pot == 3) {
            draw_pattern(scanline_buffer);
       } else if (pot == 2) {
            draw_box(scanline_buffer);
       } else if (pot == 1) {
            draw_checkerboard(scanline_buffer);
       } else {
            draw_sine(scanline_buffer);
       }
        // Pass buffer to scanvideo code
        scanvideo_end_scanline_generation(scanline_buffer);
    }
}

int main(void) {
    // Initialize semaphore
    sem_init(&video_initted, 0, 1);
    // Initialize ADC for potentiometer
    adc_init();
    adc_gpio_init(26); // ADC input 0
    adc_gpio_init(27); // ADC input 1
    // Run code on core 1
    multicore_launch_core1(core1_func);
    // Wait for video initialization to complete
    sem_acquire_blocking(&video_initted);

    while(true) {
        // Run additional code/controls here
    }
}

