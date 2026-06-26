/*
 * mc_tex_convert.c
 *
 * Converts a Minecraft Java Edition texture pack zip into a game-engine
 * texture zip.  For every block that has per-face variants (e.g.
 * acacia_log / acacia_log_top / acacia_log_side) all faces are packed into
 * one output file with a small metadata header per face:
 *
 *   "<W>x<H>\0<face_name>\0<png bytes>\0end\0"
 *
 * Dependencies: libzip, stb_image (header-only, vendored inline below)
 *
 * Build:
 *   cc mc_tex_convert.c -lzip -lz -o mc_tex_convert
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zip.h>

/* ── stb_image (decode only) ─────────────────────────────────────────────── */
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO /* we feed buffers directly */
#define STBI_ONLY_PNG
#include "../src/include/stb/stb_image.h"

/* ── stb_image_write (encode PNG to memory) ──────────────────────────────── */
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WRITE_NO_STDIO
#include "../src/include/stb/stb_image_write.h"

/* ── constants ───────────────────────────────────────────────────────────── */

/* All known per-face suffixes Minecraft uses, in the order we'll write them */
static const char* FACE_SUFFIXES[] = {
  "top", "bottom", "side", "front", "back", "inner", "outer", NULL /* sentinel */
};

/* Prefix for block textures inside the jar/zip */
static const char* TEXTURE_PREFIX = "Default-Java-1.21.11/assets/minecraft/textures/block/";

/* Maximum number of distinct block base-names we expect */
#define MAX_BLOCKS 4096

/* ── visited-name set (open-addressing hash table) ───────────────────────── */

#define VISITED_CAP (MAX_BLOCKS * 8)

static char* visited[VISITED_CAP];

static unsigned int fnv1a(const char* s) {
  unsigned int h = 2166136261u;
  while (*s) {
    h ^= (unsigned char)*s++;
    h *= 16777619u;
  }
  return h;
}

/* Returns 1 if already seen, 0 if newly inserted */
static int visited_check_insert(const char* name) {
  unsigned int idx = fnv1a(name) % VISITED_CAP;
  for (int i = 0; i < VISITED_CAP; i++) {
    unsigned int slot = (idx + i) % VISITED_CAP;
    if (!visited[slot]) {
      visited[slot] = strdup(name);
      return 0; /* new */
    }
    if (strcmp(visited[slot], name) == 0) return 1; /* already seen */
  }
  fprintf(stderr, "visited table full\n");
  exit(1);
}

static void visited_free(void) {
  for (int i = 0; i < VISITED_CAP; i++) {
    free(visited[i]);
    visited[i] = NULL;
  }
}

/* ── stb_image_write callback → dynamic buffer ───────────────────────────── */

typedef struct {
  unsigned char* data;
  size_t         len;
  size_t         cap;
} WBuf;

static void wbuf_write(void* ctx, void* data, int size) {
  WBuf* b = ctx;
  if (b->len + size > b->cap) {
    b->cap  = (b->len + size) * 2 + 4096;
    b->data = realloc(b->data, b->cap);
    if (!b->data) {
      fputs("OOM\n", stderr);
      exit(1);
    }
  }
  memcpy(b->data + b->len, data, size);
  b->len += size;
}

/* ── output buffer (assembles one block's packed file) ───────────────────── */

typedef struct {
  unsigned char* data;
  size_t         len;
  size_t         cap;
} OBuf;

static void obuf_append(OBuf* b, const void* src, size_t n) {
  if (b->len + n > b->cap) {
    b->cap  = (b->len + n) * 2 + 4096;
    b->data = (unsigned char*)realloc(b->data, b->cap);
    if (!b->data) {
      fputs("OOM\n", stderr);
      exit(1);
    }
  }
  memcpy(b->data + b->len, src, n);
  b->len += n;
}

/* append a NUL-terminated string including the NUL */
static void obuf_str(OBuf* b, const char* s) {
  obuf_append(b, s, strlen(s) + 1);
}

/* ── read a zip entry into a heap buffer ─────────────────────────────────── */

static unsigned char* zip_read_entry(zip_t* z, zip_int64_t idx, size_t* out_len) {
  struct zip_stat st;
  zip_stat_index(z, (zip_uint64_t)idx, 0, &st);

  zip_file_t* zf = zip_fopen_index(z, (zip_uint64_t)idx, 0);
  if (!zf) return NULL;

  unsigned char* buf = (unsigned char*)malloc(st.size);
  if (!buf) {
    zip_fclose(zf);
    return NULL;
  }

  zip_int64_t nread = zip_fread(zf, buf, st.size);
  zip_fclose(zf);

  if (nread != (zip_int64_t)st.size) {
    free(buf);
    return NULL;
  }
  *out_len = st.size;
  return buf;
}

/* ── strip directory prefix and .png extension, return base name ─────────── */
/*  e.g. "assets/minecraft/textures/block/acacia_log_top.png" -> "acacia_log_top" */

static void base_name(const char* path, char* out, size_t outsz) {
  const char* slash = strrchr(path, '/');
  const char* start = slash ? slash + 1 : path;
  const char* dot   = strrchr(start, '.');
  size_t      len   = dot ? (size_t)(dot - start) : strlen(start);
  if (len >= outsz) len = outsz - 1;
  memcpy(out, start, len);
  out[len] = '\0';
}

/* ── check whether base_name ends with one of the known face suffixes ─────── */
/* Returns the suffix string, or NULL if it's a "plain" texture.               */

static const char* detect_suffix(const char* bname) {
  for (int i = 0; FACE_SUFFIXES[i]; i++) {
    const char* suf  = FACE_SUFFIXES[i];
    size_t      blen = strlen(bname);
    size_t      slen = strlen(suf);
    if (blen > slen + 1 && bname[blen - slen - 1] == '_' && strcmp(bname + blen - slen, suf) == 0)
      return suf;
  }
  return NULL;
}

/* ── strip trailing "_<suffix>" from a base name to get the block name ────── */

static void strip_suffix(const char* bname, const char* suf, char* out, size_t outsz) {
  size_t cut = strlen(bname) - strlen(suf) - 1;
  if (cut >= outsz) cut = outsz - 1;
  memcpy(out, bname, cut);
  out[cut] = '\0';
}

/* ── pack one face into the output buffer ────────────────────────────────── */

static int pack_face(zip_t* z, const char* zip_path, const char* face_label, OBuf* out) {
  zip_int64_t idx = zip_name_locate(z, zip_path, 0);
  if (idx < 0) return 0;

  size_t         raw_len;
  unsigned char* raw = zip_read_entry(z, idx, &raw_len);
  if (!raw) return 0;

  int            w, h, ch;
  unsigned char* pixels = stbi_load_from_memory(raw, (int)raw_len, &w, &h, &ch, 4);
  free(raw);
  if (!pixels) {
    fprintf(stderr, "  stb_image failed on %s\n", zip_path);
    return 0;
  }

  WBuf wb = {0};
  stbi_write_png_to_func(wbuf_write, &wb, w, h, 4, pixels, w * 4);
  stbi_image_free(pixels);

  if (!wb.data) return 0;

  /* Write metadata header: "<W>x<H>\0<face_label>\0" */
  char dim[64];
  snprintf(dim, sizeof(dim), "%dx%d", w, h);
  obuf_str(out, dim);
  obuf_str(out, face_label);

  /* PNG bytes */
  obuf_append(out, wb.data, wb.len);
  free(wb.data);

  /* Footer */
  obuf_str(out, "");
  obuf_str(out, "end"); /* "end\0" – the leading \0 closes the PNG section */

  printf("    packed face %-8s  %s (%dx%d)\n", face_label, zip_path, w, h);
  return 1;
}

/* ── main ────────────────────────────────────────────────────────────────── */

int main(int argc, char* argv[]) {
  if (argc != 3) {
    fprintf(stderr, "Usage: %s <input.zip> <output.zip>\n", argv[0]);
    return 1;
  }

  int    zerr = 0;
  zip_t* zin  = zip_open(argv[1], ZIP_RDONLY, &zerr);
  if (!zin) {
    fprintf(stderr, "Cannot open input zip '%s' (err %d)\n", argv[1], zerr);
    return 1;
  }

  zip_t* zout = zip_open(argv[2], ZIP_CREATE | ZIP_TRUNCATE, &zerr);
  if (!zout) {
    fprintf(stderr, "Cannot create output zip '%s' (err %d)\n", argv[2], zerr);
    zip_close(zin);
    return 1;
  }

  zip_int64_t total = zip_get_num_entries(zin, 0);
  printf("Scanning %lld entries in %s\n", (long long)total, argv[1]);

  for (zip_int64_t i = 0; i < total; i++) {
    struct zip_stat st;
    if (zip_stat_index(zin, (zip_uint64_t)i, 0, &st) != 0) continue;

    const char* path = st.name;

    if (strncmp(path, TEXTURE_PREFIX, strlen(TEXTURE_PREFIX)) != 0) continue;
    if (path[strlen(path) - 1] == '/') continue;
    const char* ext = strrchr(path, '.');
    if (!ext || strcmp(ext, ".png") != 0) continue;

    char bname[256];
    base_name(path, bname, sizeof(bname));

    if (visited_check_insert(bname)) continue;

    const char* suf = detect_suffix(bname);
    char        block[256];
    if (suf) {
      strip_suffix(bname, suf, block, sizeof(block));
    } else {
      strncpy(block, bname, sizeof(block) - 1);
      block[sizeof(block) - 1] = '\0';
    }

    printf("Block: %s\n", block);

    OBuf packed = {0};
    int  nfaces = 0;

    char plain_path[512];
    snprintf(plain_path, sizeof(plain_path), "%s%s.png", TEXTURE_PREFIX, block);
    if (zip_name_locate(zin, plain_path, 0) >= 0) {
      visited_check_insert(block);
      nfaces += pack_face(zin, plain_path, "", &packed);
    }

    for (int j = 0; FACE_SUFFIXES[j]; j++) {
      char face_path[1024], face_bname[512];
      snprintf(face_path, sizeof(face_path), "%s%s_%s.png", TEXTURE_PREFIX, block,
               FACE_SUFFIXES[j]);
      snprintf(face_bname, sizeof(face_bname), "%s_%s", block, FACE_SUFFIXES[j]);

      visited_check_insert(face_bname);

      nfaces += pack_face(zin, face_path, FACE_SUFFIXES[j], &packed);
    }

    if (nfaces == 0) {
      free(packed.data);
      continue;
    }

    char out_entry[512];
    snprintf(out_entry, sizeof(out_entry), "%s.gtex", block);

    zip_source_t* src = zip_source_buffer(zout, packed.data, packed.len, 1);
    if (!src) {
      fprintf(stderr, "  zip_source_buffer failed for %s\n", out_entry);
      free(packed.data);
      continue;
    }

    if (zip_file_add(zout, out_entry, src, ZIP_FL_OVERWRITE) < 0) {
      fprintf(stderr, "  zip_file_add failed for %s: %s\n", out_entry, zip_strerror(zout));
      zip_source_free(src);
    } else {
      printf("  → %s  (%zu bytes, %d face(s))\n\n", out_entry, packed.len, nfaces);
    }
  }

  zip_close(zin);
  if (zip_close(zout) != 0) {
    fprintf(stderr, "Error finalising output zip: %s\n", zip_strerror(zout));
    visited_free();
    return 1;
  }

  visited_free();
  printf("Done.\n");
  return 0;
}
