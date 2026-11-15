#pragma once
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct mu_doc mu_doc;
typedef void (*mu_outline_fn)(void* user, const char* title, int page, int depth);
typedef void (*mu_entry_fn)(void* user, const char* name, const unsigned char* data, size_t len);

enum { MU_OK = 0, MU_FAILED = 1, MU_PASSWORD = 2 };

mu_doc* mu_open(const unsigned char* data, size_t len, const char* magic, int* err);
int mu_authenticate(mu_doc* d, const char* password);
void mu_close(mu_doc* d);
int mu_pages(mu_doc* d);
int mu_page_size(mu_doc* d, int page, float* w, float* h);
unsigned char* mu_render(mu_doc* d, int page, float scale, const char* mark, int* w, int* h);
char* mu_page_text(mu_doc* d, int page);
unsigned char* mu_render_png(mu_doc* d, int page, int width, size_t* len);
int mu_meta(mu_doc* d, const char* key, char* buf, int size);
void mu_outline(mu_doc* d, mu_outline_fn fn, void* user);
int mu_mobi(const unsigned char* data, size_t len, mu_entry_fn fn, void* user);
void mu_free(void* p);
const char* mu_error(void);

#ifdef __cplusplus
}
#endif
