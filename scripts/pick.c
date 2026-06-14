/*
 * png_pack.c
 *
 * Usage:
 *   png_pack output.zip tex1.png tex2.png tex3.png
 *
 * Produces:
 *   tex1.gtex
 *   tex2.gtex
 *   tex3.gtex
 *
 * Format:
 *   "<W>x<H>\0front\0<png bytes>\0end\0"
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_ONLY_PNG
#include "../src/include/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../src/include/stb/stb_image_write.h"

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} WBuf;

static void wbuf_write(void *ctx, void *data, int size) {
    WBuf *b = ctx;

    if (b->len + size > b->cap) {
        b->cap = (b->len + size) * 2 + 4096;
        b->data = realloc(b->data, b->cap);

        if (!b->data) {
            fprintf(stderr, "OOM\n");
            exit(1);
        }
    }

    memcpy(b->data + b->len, data, size);
    b->len += size;
}

typedef struct {
    unsigned char *data;
    size_t len;
    size_t cap;
} OBuf;

static void obuf_append(OBuf *b, const void *src, size_t n) {
    if (b->len + n > b->cap) {
        b->cap = (b->len + n) * 2 + 4096;
        b->data = realloc(b->data, b->cap);

        if (!b->data) {
            fprintf(stderr, "OOM\n");
            exit(1);
        }
    }

    memcpy(b->data + b->len, src, n);
    b->len += n;
}

static void obuf_str(OBuf *b, const char *s) {
    obuf_append(b, s, strlen(s) + 1);
}

static unsigned char *read_file(const char *path, size_t *len) {
    FILE *f = fopen(path, "rb");

    if (!f)
        return NULL;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    rewind(f);

    if (sz < 0) {
        fclose(f);
        return NULL;
    }

    unsigned char *buf = malloc((size_t)sz);

    if (!buf) {
        fclose(f);
        return NULL;
    }

    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf);
        fclose(f);
        return NULL;
    }

    fclose(f);

    *len = (size_t)sz;
    return buf;
}

static void basename_no_ext(const char *path,
                            char *out,
                            size_t outsz) {
    const char *name = strrchr(path, '/');

#ifdef _WIN32
    const char *name2 = strrchr(path, '\\');
    if (!name || (name2 && name2 > name))
        name = name2;
#endif

    name = name ? name + 1 : path;

    const char *dot = strrchr(name, '.');

    size_t len = dot ? (size_t)(dot - name) : strlen(name);

    if (len >= outsz)
        len = outsz - 1;

    memcpy(out, name, len);
    out[len] = 0;
}

static int pack_png(const char *png_path, OBuf *out) {
    size_t raw_len;

    unsigned char *raw = read_file(png_path, &raw_len);

    if (!raw) {
        fprintf(stderr, "Failed to read %s\n", png_path);
        return 0;
    }

    int w, h, ch;

    unsigned char *pixels =
        stbi_load_from_memory(raw,
                              (int)raw_len,
                              &w,
                              &h,
                              &ch,
                              4);

    free(raw);

    if (!pixels) {
        fprintf(stderr, "Failed to decode %s\n", png_path);
        return 0;
    }

    WBuf wb = {0};

    stbi_write_png_to_func(
        wbuf_write,
        &wb,
        w,
        h,
        4,
        pixels,
        w * 4);

    stbi_image_free(pixels);

    if (!wb.data)
        return 0;

    char dim[64];

    snprintf(dim, sizeof(dim), "%dx%d", w, h);

    obuf_str(out, dim);
    obuf_str(out, "front");

    obuf_append(out, wb.data, wb.len);

    free(wb.data);

    obuf_str(out, "");
    obuf_str(out, "end");

    return 1;
}

int main(int argc, char **argv) {
    if (argc < 3) {
        fprintf(stderr,
                "Usage: %s output.zip image1.png [image2.png ...]\n",
                argv[0]);
        return 1;
    }

    int zerr = 0;

    zip_t *zout =
        zip_open(argv[1],
                 ZIP_CREATE | ZIP_TRUNCATE,
                 &zerr);

    if (!zout) {
        fprintf(stderr,
                "Cannot create %s\n",
                argv[1]);
        return 1;
    }

    for (int i = 2; i < argc; i++) {
        OBuf packed = {0};

        if (!pack_png(argv[i], &packed)) {
            free(packed.data);
            continue;
        }

        char name[256];
        char entry[300];

        basename_no_ext(argv[i], name, sizeof(name));

        snprintf(entry,
                 sizeof(entry),
                 "%s.gtex",
                 name);

        zip_source_t *src =
            zip_source_buffer(zout,
                              packed.data,
                              packed.len,
                              1);

        if (!src) {
            fprintf(stderr,
                    "zip_source_buffer failed for %s\n",
                    entry);
            free(packed.data);
            continue;
        }

        if (zip_file_add(zout,
                         entry,
                         src,
                         ZIP_FL_OVERWRITE) < 0) {
            fprintf(stderr,
                    "zip_file_add failed for %s\n",
                    entry);

            zip_source_free(src);
        } else {
            printf("Added %s\n", entry);
        }
    }

    if (zip_close(zout) != 0) {
        fprintf(stderr,
                "Failed to finalize zip\n");
        return 1;
    }

    return 0;
}
