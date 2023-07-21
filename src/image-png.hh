#ifndef _IMAGE_PNG_HH
#define _IMAGE_PNG_HH

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "image.hh"

int png_init(image_ctx_t * ctx);
int png_add_pixel(image_ctx_t * ctx,
                  const uint8_t r,
                  const uint8_t g,
                  const uint8_t b,
                  const uint8_t a);
int png_write(image_ctx_t * ctx, std::vector<char> * out);

image_dispatch_table_t png_dt = {.init      = png_init,
                                 .add_pixel = png_add_pixel,
                                 .set_pixel = NULL,
                                 .get_pixel = NULL,
                                 .write     = png_write,
                                 .free      = NULL};

#endif
