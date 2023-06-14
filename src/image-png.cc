#include <errno.h>
#include <png.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cstring>

#include "image.hh"

#define PNG_DEBUG 3

void abort_(const char *s, ...)
{
    va_list args;
    va_start(args, s);
    vfprintf(stderr, s, args);
    fprintf(stderr, "\n");
    va_end(args);
}

int png_init(image_ctx_t *ctx)
{
    size_t buf_size;

    /* Perform simple sanity checking */
    if (ctx->canvas != NULL) return EINVAL;
    ctx->model = IMAGE_RGBA;  // Override this as we only support RGB
    ctx->format = IMAGE_PNG;
    ctx->extension = (char *)".png";

    buf_size = ctx->width * ctx->height * RGBA_SIZE;

    /* Allocate the canvas buffer */
    ctx->canvas = (uint8_t *)calloc(buf_size, sizeof(uint8_t));
    ctx->next_pixel = ctx->canvas;
    if (ctx->canvas == NULL) return ENOMEM;

    return 0;
}

int png_add_pixel(image_ctx_t *ctx, const uint8_t r, const uint8_t g, const uint8_t b, const uint8_t a)
{
    uint8_t *next;

    next = ctx->next_pixel;

    next[0] = r;
    next[1] = g;
    next[2] = b;
    next[3] = a;

    ctx->next_pixel += RGBA_SIZE;

    return 0;
}

struct png_writer {
    size_t pos;
    char *out;
};

void write_png_data(png_structp png_ptr, png_bytep data, size_t length)
{
    std::vector<char> *out = (std::vector<char> *)png_get_io_ptr(png_ptr);
    out->insert(out->end(), data, data + length);
    return;
}

void flush_png_data(png_structp png_ptr) { return; }

int png_write(image_ctx_t *ctx, std::vector<char> *out)
{
    // TODO better error handling here
    png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info_ptr = png_create_info_struct(png_ptr);
    png_set_write_fn(png_ptr, (void *)out, write_png_data, flush_png_data);
    if (setjmp(png_jmpbuf(png_ptr))) abort_("[write_png_file] Error during writing header");
    png_set_IHDR(png_ptr, info_ptr, ctx->width, ctx->height, 8, PNG_COLOR_TYPE_RGB_ALPHA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);

    png_write_info(png_ptr, info_ptr);

    if (setjmp(png_jmpbuf(png_ptr))) {
        abort_("[write_png_file] Error during writing bytes");
        return 1;
    }

    png_bytep row_pointer = ctx->canvas;

    for (size_t i = 0; i < ctx->height; i++) {
        png_write_row(png_ptr, row_pointer);
        row_pointer += ctx->width * RGBA_SIZE;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        abort_("[write_png_file] Error during end of write");
        return 1;
    }

    png_write_end(png_ptr, info_ptr);

    png_destroy_write_struct(&png_ptr, &info_ptr);

    return 0;
}
