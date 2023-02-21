// Rough program to demonstrate basic usage of scanvideo API

#include "pico.h"
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/scanvideo.h"
#include "pico/scanvideo/composable_scanline.h"
#include "pico/sync.h"

// VGA mode struct defines video timing and size

#define vga_mode vga_mode_640x480_60
//#define vga_mode vga_mode_320x240_60
//#define vga_mode vga_mode_213x160_60
//#define vga_mode vga_mode_160x120_60
//#define vga_mode vga_mode_tft_800x480_50
//#define vga_mode vga_mode_tft_400x240_50

// Semaphore used to block code from proceeding unitl video is initialized
static semaphore_t video_initted;

void draw(scanvideo_scanline_buffer_t *buffer) {
    // Get pointer to buffer data
    uint16_t *p = (uint16_t *) buffer->data;
    // Set token (COMPOSABLE_COLOR_RUN = draw all pixels up to a certain length)
    *p++ = COMPOSABLE_COLOR_RUN;
    // Set color for word (red)
    *p++ = PICO_SCANVIDEO_PIXEL_FROM_RGB5(0x1f,0,0);
    // Set number of pixels to draw (subtract 3 from desired number)
    *p++ = vga_mode.width - 3;

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

void core1_func() {
    // Configure scanvideo code based on VGA mode
    scanvideo_setup(&vga_mode);
    // Turn on scanvideo code
    scanvideo_timing_enable(true);
    // Release semaphore
    sem_release(&video_initted);

    while (true) {
        // Generate scanline buffer
        scanvideo_scanline_buffer_t *scanline_buffer = scanvideo_begin_scanline_generation(true);
        // Draw pixels to buffer
        draw(scanline_buffer);
        // Pass buffer to scanvideo code
        scanvideo_end_scanline_generation(scanline_buffer);
    }
}

int main(void) {
    // Initialize semaphore
    sem_init(&video_initted, 0, 1);
    // Run code on core 1
    multicore_launch_core1(core1_func);
    // Wait for video initialization to complete
    sem_acquire_blocking(&video_initted);

    while(true) {
        // Run additional code/controls here
    }
}

