#include "backbuffer.h"
#include <stdio.h>

INIT_BACKBUFFER_STATUS init_backbuffer(GameBackBuffer *buffer, int width,
                                       int height, int bytes_per_pixel) {
  buffer->memory.base = NULL;
  buffer->width = width;
  buffer->height = height;
  buffer->bytes_per_pixel = bytes_per_pixel;
  buffer->pitch = width * bytes_per_pixel;
  int initial_initial_buffer_size = buffer->pitch * buffer->height;
  De100MemoryBlock de100_memory_block =
      de100_memory_alloc(NULL, initial_initial_buffer_size,
                         De100_MEMORY_FLAG_READ | De100_MEMORY_FLAG_WRITE |
                             De100_MEMORY_FLAG_ZEROED);

  if (!de100_memory_is_valid(de100_memory_block)) {
    fprintf(stderr,
            "de100_memory_alloc failed: could not allocate %d "
            "bytes\n",
            initial_initial_buffer_size);
    fprintf(stderr, "   Code: %s\n",
            de100_memory_error_str(de100_memory_block.error_code));
    return INIT_BACKBUFFER_STATUS_MMAP_FAILED;
  }

  buffer->memory = de100_memory_block;

  return INIT_BACKBUFFER_STATUS_SUCCESS;
}