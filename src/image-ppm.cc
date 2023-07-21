#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "image.hh"

int ppm_init(image_ctx_t * ctx) {
    size_t buf_size;

    /* Perform simple sanity checking */
    if (ctx->canvas != NULL)
        return EINVAL;
    ctx->model     = IMAGE_RGB; // Override this as we only support RGB
    ctx->format    = IMAGE_PPM;
    ctx->extension = (char *)".ppm";

    buf_size = ctx->width * ctx->height * RGB_SIZE;

    /* Allocate the canvas buffer */
    ctx->canvas     = (uint8_t *)calloc(buf_size, sizeof(uint8_t));
    ctx->next_pixel = ctx->canvas;
    if (ctx->canvas == NULL)
        return ENOMEM;

    return 0;
}

int ppm_add_pixel(image_ctx_t * ctx,
                  const uint8_t r,
                  const uint8_t g,
                  const uint8_t b,
                  const uint8_t) {
    uint8_t * next;

    next = ctx->next_pixel;

    next[0] = r;
    next[1] = g;
    next[2] = b;

    ctx->next_pixel += 3;

    return 0;
}

int ppm_write(image_ctx_t * ctx, std::vector<char> * out) {
    size_t count;

    count = ctx->width * ctx->height * RGB_SIZE;

    out->insert(out->begin(), ctx->canvas, ctx->canvas + count);

    return 0;
}
