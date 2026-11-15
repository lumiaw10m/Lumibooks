#include "mupdf/fitz.h"
#include "Mu.h"
#include <stdlib.h>
#include <string.h>

fz_archive* fz_extract_html_from_mobi(fz_context* ctx, fz_buffer* mobi);

static char last_error[256];

static void keep(fz_context* ctx)
{
    fz_strlcpy(last_error, fz_caught_message(ctx), sizeof last_error);
}

const char* mu_error(void)
{
    return last_error;
}

struct mu_doc
{
    fz_context* ctx;
    fz_document* doc;
    fz_buffer* buf;
};

mu_doc* mu_open(const unsigned char* data, size_t len, const char* magic, int* err)
{
    mu_doc* d = calloc(1, sizeof *d);
    *err = MU_FAILED;
    last_error[0] = 0;
    if (!d) return NULL;
    d->ctx = fz_new_context(NULL, NULL, 64 << 20);
    if (!d->ctx) { free(d); return NULL; }
    fz_try(d->ctx)
    {
        fz_register_document_handlers(d->ctx);
        d->buf = fz_new_buffer_from_shared_data(d->ctx, data, len);
        d->doc = fz_open_document_with_buffer(d->ctx, magic, d->buf);
        *err = fz_needs_password(d->ctx, d->doc) ? MU_PASSWORD : MU_OK;
    }
    fz_catch(d->ctx)
    {
        keep(d->ctx);
        mu_close(d);
        return NULL;
    }
    return d;
}

int mu_authenticate(mu_doc* d, const char* password)
{
    int ok = 0;
    fz_try(d->ctx) ok = fz_authenticate_password(d->ctx, d->doc, password);
    fz_catch(d->ctx) ok = 0;
    return ok;
}

void mu_close(mu_doc* d)
{
    if (!d) return;
    fz_drop_document(d->ctx, d->doc);
    fz_drop_buffer(d->ctx, d->buf);
    fz_drop_context(d->ctx);
    free(d);
}

int mu_pages(mu_doc* d)
{
    int n = 0;
    fz_try(d->ctx) n = fz_count_pages(d->ctx, d->doc);
    fz_catch(d->ctx) n = 0;
    return n;
}

int mu_page_size(mu_doc* d, int page, float* w, float* h)
{
    fz_page* p = NULL;
    int ok = 1;
    fz_var(p);
    fz_try(d->ctx)
    {
        fz_rect r;
        p = fz_load_page(d->ctx, d->doc, page);
        r = fz_bound_page(d->ctx, p);
        *w = r.x1 - r.x0;
        *h = r.y1 - r.y0;
    }
    fz_always(d->ctx) fz_drop_page(d->ctx, p);
    fz_catch(d->ctx) ok = 0;
    return ok;
}

static fz_pixmap* draw(mu_doc* d, int page, float scale, int alpha)
{
    fz_context* ctx = d->ctx;
    fz_page* p = NULL;
    fz_pixmap* pix = NULL;
    fz_device* dev = NULL;
    fz_var(p);
    fz_var(pix);
    fz_var(dev);
    fz_try(ctx)
    {
        fz_matrix m = fz_scale(scale, scale);
        fz_irect bb;
        p = fz_load_page(ctx, d->doc, page);
        bb = fz_round_rect(fz_transform_rect(fz_bound_page(ctx, p), m));
        pix = fz_new_pixmap_with_bbox(ctx, alpha ? fz_device_bgr(ctx) : fz_device_rgb(ctx), bb, NULL, alpha);
        fz_clear_pixmap_with_value(ctx, pix, 255);
        dev = fz_new_draw_device(ctx, fz_identity, pix);
        fz_run_page(ctx, p, dev, m, NULL);
        fz_close_device(ctx, dev);
    }
    fz_always(ctx)
    {
        fz_drop_device(ctx, dev);
        fz_drop_page(ctx, p);
    }
    fz_catch(ctx)
    {
        fz_drop_pixmap(ctx, pix);
        pix = NULL;
    }
    return pix;
}

unsigned char* mu_render(mu_doc* d, int page, float scale, int* w, int* h)
{
    fz_pixmap* pix = draw(d, page, scale, 1);
    unsigned char *out, *src;
    int y, stride;
    if (!pix) return NULL;
    *w = fz_pixmap_width(d->ctx, pix);
    *h = fz_pixmap_height(d->ctx, pix);
    stride = (int)fz_pixmap_stride(d->ctx, pix);
    src = fz_pixmap_samples(d->ctx, pix);
    out = malloc((size_t)*w * *h * 4);
    if (out)
        for (y = 0; y < *h; y++) memcpy(out + (size_t)y * *w * 4, src + (size_t)y * stride, (size_t)*w * 4);
    fz_drop_pixmap(d->ctx, pix);
    return out;
}

unsigned char* mu_render_png(mu_doc* d, int page, int width, size_t* len)
{
    float w, h;
    fz_pixmap* pix;
    fz_buffer* png = NULL;
    unsigned char* out = NULL;
    if (!mu_page_size(d, page, &w, &h) || w <= 0) return NULL;
    pix = draw(d, page, width / w, 0);
    if (!pix) return NULL;
    fz_try(d->ctx)
    {
        unsigned char* p;
        png = fz_new_buffer_from_pixmap_as_png(d->ctx, pix, fz_default_color_params);
        *len = fz_buffer_storage(d->ctx, png, &p);
        out = malloc(*len);
        if (out) memcpy(out, p, *len);
    }
    fz_always(d->ctx)
    {
        fz_drop_buffer(d->ctx, png);
        fz_drop_pixmap(d->ctx, pix);
    }
    fz_catch(d->ctx) out = NULL;
    return out;
}

int mu_meta(mu_doc* d, const char* key, char* buf, int size)
{
    int n = -1;
    buf[0] = 0;
    fz_try(d->ctx) n = fz_lookup_metadata(d->ctx, d->doc, key, buf, size);
    fz_catch(d->ctx) n = -1;
    return n > 0;
}

static void walk(mu_doc* d, fz_outline* o, int depth, mu_outline_fn fn, void* user)
{
    for (; o; o = o->next)
    {
        int page = -1;
        fz_try(d->ctx)
        {
            if (o->page.page >= 0) page = fz_page_number_from_location(d->ctx, d->doc, o->page);
            else if (o->uri)
            {
                float x, y;
                page = fz_page_number_from_location(d->ctx, d->doc, fz_resolve_link(d->ctx, d->doc, o->uri, &x, &y));
            }
        }
        fz_catch(d->ctx) page = -1;
        if (o->title) fn(user, o->title, page, depth);
        walk(d, o->down, depth + 1, fn, user);
    }
}

void mu_outline(mu_doc* d, mu_outline_fn fn, void* user)
{
    fz_outline* o = NULL;
    fz_var(o);
    fz_try(d->ctx)
    {
        o = fz_load_outline(d->ctx, d->doc);
        walk(d, o, 0, fn, user);
    }
    fz_always(d->ctx) fz_drop_outline(d->ctx, o);
    fz_catch(d->ctx) {}
}

int mu_mobi(const unsigned char* data, size_t len, mu_entry_fn fn, void* user)
{
    fz_context* ctx = fz_new_context(NULL, NULL, 0);
    fz_buffer* buf = NULL;
    fz_archive* arch = NULL;
    int ok = 0;
    last_error[0] = 0;
    if (!ctx) return 0;
    fz_var(buf);
    fz_var(arch);
    fz_try(ctx)
    {
        int i;
        char name[16];
        buf = fz_new_buffer_from_shared_data(ctx, data, len);
        arch = fz_extract_html_from_mobi(ctx, buf);
        for (i = 0;; i++)
        {
            fz_buffer* e;
            unsigned char* p;
            size_t z;
            if (i == 0) fz_strlcpy(name, "index.html", sizeof name);
            else fz_snprintf(name, sizeof name, "%05d", i);
            if (!fz_has_archive_entry(ctx, arch, name)) break;
            e = fz_read_archive_entry(ctx, arch, name);
            z = fz_buffer_storage(ctx, e, &p);
            fn(user, name, p, z);
            fz_drop_buffer(ctx, e);
        }
        ok = i > 0;
    }
    fz_always(ctx)
    {
        fz_drop_archive(ctx, arch);
        fz_drop_buffer(ctx, buf);
    }
    fz_catch(ctx)
    {
        keep(ctx);
        ok = 0;
    }
    fz_drop_context(ctx);
    return ok;
}

void mu_free(void* p)
{
    free(p);
}
