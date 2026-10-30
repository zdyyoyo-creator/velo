/* ============================================================================
 * velo_rt.c -- Velo 语言运行时库实现
 * 注意：整个文件以 UTF-8 保存，编译时需加 /utf-8 (MSVC) 或 -finput-charset=UTF-8
 * ========================================================================== */
#define _CRT_SECURE_NO_WARNINGS 1
#include "velo_rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <errno.h>
#include <float.h>
#include <limits.h>

#if defined(_WIN32)
#  include <windows.h>
#  include <io.h>
#  include <fcntl.h>
#endif

/* ========================================================================== */
/* 内存                                                                        */
/* ========================================================================== */
void* vl_alloc(int64_t n) {
    void* p;
    if (n <= 0) n = 1;
    p = malloc((size_t)n);
    if (!p) {
        fputs("velo: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

void* vl_alloc_zero(int64_t n) {
    void* p;
    if (n <= 0) n = 1;
    p = calloc(1, (size_t)n);
    if (!p) {
        fputs("velo: out of memory\n", stderr);
        exit(1);
    }
    return p;
}

void* vl_realloc(void* p, int64_t n) {
    void* q;
    if (n <= 0) n = 1;
    q = realloc(p, (size_t)n);
    if (!q) {
        fputs("velo: out of memory\n", stderr);
        exit(1);
    }
    return q;
}

void vl_dealloc(void* p) { free(p); }

/* ========================================================================== */
/* 数字 -> 文本                                                                */
/* ========================================================================== */
static int u64_to_buf(char* buf, uint64_t v) {
    char tmp[24];
    int n = 0, i;
    if (v == 0) { buf[0] = '0'; return 1; }
    while (v) { tmp[n++] = (char)('0' + (int)(v % 10)); v /= 10; }
    for (i = 0; i < n; i++) buf[i] = tmp[n - 1 - i];
    return n;
}

static int i64_to_buf(char* buf, int64_t v) {
    if (v < 0) {
        uint64_t mag;
        buf[0] = '-';
        mag = (uint64_t)(-(v + 1)) + 1u;   /* 避免 INT64_MIN 溢出 */
        return 1 + u64_to_buf(buf + 1, mag);
    }
    return u64_to_buf(buf, (uint64_t)v);
}

/* 浮点数 -> 最短可往返文本（Python repr 风格） */
static int f64_to_buf(char* buf, int cap, double v) {
    int n, i, looks_float = 0;
    if (v != v) { memcpy(buf, "nan", 3); buf[3] = 0; return 3; }
    if (v == (double)INFINITY) { memcpy(buf, "inf", 3); buf[3] = 0; return 3; }
    if (v == -(double)INFINITY) { memcpy(buf, "-inf", 4); buf[4] = 0; return 4; }
    n = 0;
    for (i = 1; i <= 17; i++) {
        n = snprintf(buf, (size_t)cap, "%.*g", i, v);
        if (n < 0) { n = 0; break; }
        if (strtod(buf, NULL) == v) break;
    }
    for (i = 0; i < n; i++) {
        char c = buf[i];
        if (c == '.' || c == 'e' || c == 'E' || c == 'n' || c == 'i') { looks_float = 1; break; }
    }
    if (!looks_float && n + 2 < cap) { buf[n++] = '.'; buf[n++] = '0'; buf[n] = 0; }
    return n;
}

/* ========================================================================== */
/* 字符串                                                                      */
/* ========================================================================== */
vl_str* vl_str_alloc(int64_t n) {
    vl_str* s = (vl_str*)vl_alloc((int64_t)sizeof(vl_str));
    if (n < 0) n = 0;
    s->rc = 1;
    s->len = n;
    s->data = (char*)vl_alloc(n + 1);
    s->data[n] = '\0';
    return s;
}

vl_str* vl_str_new_len(const char* p, int64_t n) {
    vl_str* s = vl_str_alloc(n);
    if (n > 0 && p) memcpy(s->data, p, (size_t)n);
    s->data[n] = '\0';
    return s;
}

vl_str* vl_str_new(const char* c) {
    if (!c) return vl_str_alloc(0);
    return vl_str_new_len(c, (int64_t)strlen(c));
}

void vl_str_free(vl_str* s) {
    if (!s) return;
    vl_dealloc(s->data);
    vl_dealloc(s);
}

/* ------------------------------------------------------------------ 构建器 */
void vl_sb_init(vl_sb* b) { b->buf = NULL; b->len = 0; b->cap = 0; }

static void sb_grow(vl_sb* b, int64_t need) {
    int64_t cap;
    if (b->len + need <= b->cap) return;
    cap = b->cap ? b->cap : 64;
    while (cap < b->len + need) cap *= 2;
    b->buf = (char*)vl_realloc(b->buf, cap);
    b->cap = cap;
}

void vl_sb_put_bytes(vl_sb* b, const char* p, int64_t n) {
    if (!p || n <= 0) return;
    sb_grow(b, n);
    memcpy(b->buf + b->len, p, (size_t)n);
    b->len += n;
}

void vl_sb_put_cstr(vl_sb* b, const char* c) {
    if (!c) return;
    vl_sb_put_bytes(b, c, (int64_t)strlen(c));
}

void vl_sb_put_char(vl_sb* b, char c) {
    sb_grow(b, 1);
    b->buf[b->len++] = c;
}

void vl_sb_put_i64(vl_sb* b, int64_t v) {
    char tmp[24];
    int n = i64_to_buf(tmp, v);
    vl_sb_put_bytes(b, tmp, n);
}

void vl_sb_put_u64(vl_sb* b, uint64_t v) {
    char tmp[24];
    int n = u64_to_buf(tmp, v);
    vl_sb_put_bytes(b, tmp, n);
}

void vl_sb_put_f64(vl_sb* b, double v) {
    char tmp[40];
    int n = f64_to_buf(tmp, (int)sizeof(tmp), v);
    vl_sb_put_bytes(b, tmp, n);
}

void vl_sb_put_bool(vl_sb* b, bool v) {
    if (v) vl_sb_put_cstr(b, "True");
    else   vl_sb_put_cstr(b, "False");
}

void vl_sb_put_str(vl_sb* b, const vl_str* s) {
    if (!s) return;
    vl_sb_put_bytes(b, s->data, s->len);
}

void vl_sb_put_str_repr(vl_sb* b, const vl_str* s) {
    int64_t i;
    vl_sb_put_char(b, '"');
    if (s) {
        for (i = 0; i < s->len; i++) {
            unsigned char c = (unsigned char)s->data[i];
            switch (c) {
                case '"':  vl_sb_put_cstr(b, "\\\""); break;
                case '\\': vl_sb_put_cstr(b, "\\\\"); break;
                case '\n': vl_sb_put_cstr(b, "\\n"); break;
                case '\t': vl_sb_put_cstr(b, "\\t"); break;
                case '\r': vl_sb_put_cstr(b, "\\r"); break;
                default:
                    if (c < 0x20) {
                        char tmp[8];
                        snprintf(tmp, sizeof(tmp), "\\x%02x", (unsigned)c);
                        vl_sb_put_cstr(b, tmp);
                    } else {
                        vl_sb_put_char(b, (char)c);
                    }
            }
        }
    }
    vl_sb_put_char(b, '"');
}

vl_str* vl_sb_finish(vl_sb* b) {
    vl_str* s = vl_str_new_len(b->buf ? b->buf : "", b->len);
    if (b->buf) vl_dealloc(b->buf);
    b->buf = NULL;
    b->len = 0;
    b->cap = 0;
    return s;
}

/* ------------------------------------------------------------------ 运算 */
vl_str* vl_str_concat(const vl_str* a, const vl_str* b) {
    int64_t la = a ? a->len : 0;
    int64_t lb = b ? b->len : 0;
    vl_str* s = vl_str_alloc(la + lb);
    if (la > 0) memcpy(s->data, a->data, (size_t)la);
    if (lb > 0) memcpy(s->data + la, b->data, (size_t)lb);
    return s;
}

static int64_t norm_index(int64_t i, int64_t len) {
    if (i < 0) i += len;
    if (i < 0) i = 0;
    if (i > len) i = len;
    return i;
}

vl_str* vl_str_slice(const vl_str* s, int64_t start, int64_t end) {
    int64_t len = s ? s->len : 0;
    int64_t a = norm_index(start, len);
    int64_t b = norm_index(end, len);
    if (b < a) b = a;
    return vl_str_new_len(s ? s->data + a : "", b - a);
}

vl_str* vl_str_repeat(const vl_str* s, int64_t n) {
    vl_str* out;
    int64_t i, len = s ? s->len : 0;
    if (n <= 0 || len == 0) return vl_str_alloc(0);
    out = vl_str_alloc(len * n);
    for (i = 0; i < n; i++) memcpy(out->data + i * len, s->data, (size_t)len);
    return out;
}

vl_str* vl_str_upper(const vl_str* s) {
    vl_str* out;
    int64_t i;
    if (!s) return vl_str_alloc(0);
    out = vl_str_new_len(s->data, s->len);
    for (i = 0; i < out->len; i++) {
        unsigned char c = (unsigned char)out->data[i];
        if (c >= 'a' && c <= 'z') out->data[i] = (char)(c - 32);
    }
    return out;
}

vl_str* vl_str_lower(const vl_str* s) {
    vl_str* out;
    int64_t i;
    if (!s) return vl_str_alloc(0);
    out = vl_str_new_len(s->data, s->len);
    for (i = 0; i < out->len; i++) {
        unsigned char c = (unsigned char)out->data[i];
        if (c >= 'A' && c <= 'Z') out->data[i] = (char)(c + 32);
    }
    return out;
}

static int is_space_ch(unsigned char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

vl_str* vl_str_trim(const vl_str* s) {
    int64_t a = 0, b;
    if (!s) return vl_str_alloc(0);
    b = s->len;
    while (a < b && is_space_ch((unsigned char)s->data[a])) a++;
    while (b > a && is_space_ch((unsigned char)s->data[b - 1])) b--;
    return vl_str_new_len(s->data + a, b - a);
}

vl_str* vl_str_trim_start(const vl_str* s) {
    int64_t a = 0, b;
    if (!s) return vl_str_alloc(0);
    b = s->len;
    while (a < b && is_space_ch((unsigned char)s->data[a])) a++;
    return vl_str_new_len(s->data + a, b - a);
}

vl_str* vl_str_trim_end(const vl_str* s) {
    int64_t b;
    if (!s) return vl_str_alloc(0);
    b = s->len;
    while (b > 0 && is_space_ch((unsigned char)s->data[b - 1])) b--;
    return vl_str_new_len(s->data, b);
}

vl_str* vl_str_char_at(const vl_str* s, int64_t i) {
    int k;
    if (!s) return vl_str_alloc(0);
    if (i < 0) i += s->len;
#if VL_BOUNDS_CHECK
    if (i < 0 || i >= s->len) vl_index_error(i, s->len);
#endif
    {
        unsigned char c = (unsigned char)s->data[i];
        if (c < 0x80) k = 1;
        else if ((c & 0xE0) == 0xC0) k = 2;
        else if ((c & 0xF0) == 0xE0) k = 3;
        else if ((c & 0xF8) == 0xF0) k = 4;
        else k = 1;
    }
    if (i + k > s->len) k = 1;
    return vl_str_new_len(s->data + i, k);
}

static int64_t find_bytes(const char* hay, int64_t hlen, const char* ndl, int64_t nlen, int64_t from) {
    int64_t i;
    if (nlen == 0) return from <= hlen ? from : -1;
    if (nlen > hlen) return -1;
    if (from < 0) from = 0;
    for (i = from; i + nlen <= hlen; i++) {
        if (hay[i] == ndl[0] && memcmp(hay + i, ndl, (size_t)nlen) == 0) return i;
    }
    return -1;
}

vl_str* vl_str_replace(const vl_str* s, const vl_str* old_s, const vl_str* new_s) {
    vl_sb b;
    int64_t pos = 0, olen, nlen;
    if (!s) return vl_str_alloc(0);
    olen = old_s ? old_s->len : 0;
    nlen = new_s ? new_s->len : 0;
    if (olen == 0) return vl_str_new_len(s->data, s->len);
    vl_sb_init(&b);
    for (;;) {
        int64_t idx = find_bytes(s->data, s->len, old_s->data, olen, pos);
        if (idx < 0) {
            vl_sb_put_bytes(&b, s->data + pos, s->len - pos);
            break;
        }
        vl_sb_put_bytes(&b, s->data + pos, idx - pos);
        if (nlen > 0) vl_sb_put_bytes(&b, new_s->data, nlen);
        pos = idx + olen;
    }
    return vl_sb_finish(&b);
}

vl_str* vl_str_from_i64(int64_t v) {
    char tmp[24];
    int n = i64_to_buf(tmp, v);
    return vl_str_new_len(tmp, n);
}

vl_str* vl_str_from_u64(uint64_t v) {
    char tmp[24];
    int n = u64_to_buf(tmp, v);
    return vl_str_new_len(tmp, n);
}

vl_str* vl_str_from_f64(double v) {
    char tmp[40];
    int n = f64_to_buf(tmp, (int)sizeof(tmp), v);
    return vl_str_new_len(tmp, n);
}

vl_str* vl_str_from_bool(bool v) { return vl_str_new(v ? "True" : "False"); }

/* ------------------------------------------------------------------ 比较 */
int64_t vl_str_cmp(const vl_str* a, const vl_str* b) {
    int64_t al = a ? a->len : 0;
    int64_t bl = b ? b->len : 0;
    int64_t m = al < bl ? al : bl;
    int c = m > 0 ? memcmp(a->data, b->data, (size_t)m) : 0;
    if (c != 0) return c < 0 ? -1 : 1;
    if (al == bl) return 0;
    return al < bl ? -1 : 1;
}

bool vl_str_eq(const vl_str* a, const vl_str* b) {
    int64_t al = a ? a->len : 0;
    int64_t bl = b ? b->len : 0;
    if (al != bl) return false;
    if (al == 0) return true;
    return memcmp(a->data, b->data, (size_t)al) == 0;
}

bool vl_str_lt(const vl_str* a, const vl_str* b) { return vl_str_cmp(a, b) < 0; }
bool vl_str_le(const vl_str* a, const vl_str* b) { return vl_str_cmp(a, b) <= 0; }
bool vl_str_gt(const vl_str* a, const vl_str* b) { return vl_str_cmp(a, b) > 0; }
bool vl_str_ge(const vl_str* a, const vl_str* b) { return vl_str_cmp(a, b) >= 0; }

int64_t vl_str_find(const vl_str* hay, const vl_str* needle, int64_t start) {
    if (!hay || !needle) return -1;
    return find_bytes(hay->data, hay->len, needle->data, needle->len, start);
}

int64_t vl_str_rfind(const vl_str* hay, const vl_str* needle) {
    int64_t i;
    if (!hay || !needle || needle->len == 0) return -1;
    for (i = hay->len - needle->len; i >= 0; i--) {
        if (memcmp(hay->data + i, needle->data, (size_t)needle->len) == 0) return i;
    }
    return -1;
}

bool vl_str_contains(const vl_str* hay, const vl_str* needle) {
    return vl_str_find(hay, needle, 0) >= 0;
}

bool vl_str_starts_with(const vl_str* s, const vl_str* prefix) {
    if (!s || !prefix) return false;
    if (prefix->len > s->len) return false;
    return memcmp(s->data, prefix->data, (size_t)prefix->len) == 0;
}

bool vl_str_ends_with(const vl_str* s, const vl_str* suffix) {
    if (!s || !suffix) return false;
    if (suffix->len > s->len) return false;
    return memcmp(s->data + s->len - suffix->len, suffix->data, (size_t)suffix->len) == 0;
}

uint64_t vl_str_hash(const vl_str* s) {
    uint64_t h = 1469598103934665603ull;
    int64_t i;
    if (!s) return h;
    for (i = 0; i < s->len; i++) {
        h ^= (unsigned char)s->data[i];
        h *= 1099511628211ull;
    }
    return h;
}

int64_t vl_str_utf8_len(const vl_str* s) {
    int64_t i, n = 0;
    if (!s) return 0;
    for (i = 0; i < s->len; i++) {
        if (((unsigned char)s->data[i] & 0xC0) != 0x80) n++;
    }
    return n;
}

int64_t vl_str_byte_at(const vl_str* s, int64_t i) {
    if (!s) vl_index_error(i, 0);
    if (i < 0) i += s->len;
#if VL_BOUNDS_CHECK
    if (i < 0 || i >= s->len) vl_index_error(i, s->len);
#endif
    return (int64_t)(unsigned char)s->data[i];
}

int64_t vl_str_to_int(const vl_str* s, bool* ok) {
    int64_t v = 0;
    bool good = false;
    if (s && s->len > 0) {
        char* end = NULL;
        char buf[64];
        int64_t n = s->len < 63 ? s->len : 63;
        memcpy(buf, s->data, (size_t)n);
        buf[n] = 0;
        errno = 0;
        v = (int64_t)strtoll(buf, &end, 10);
        if (errno == 0 && end != buf && *end == 0 && n == s->len) good = true;
    }
    if (ok) *ok = good;
    if (!good) vl_panic("cannot convert string to int");
    return v;
}

double vl_str_to_float(const vl_str* s, bool* ok) {
    double v = 0;
    bool good = false;
    if (s && s->len > 0) {
        char* end = NULL;
        char buf[64];
        int64_t n = s->len < 63 ? s->len : 63;
        memcpy(buf, s->data, (size_t)n);
        buf[n] = 0;
        errno = 0;
        v = strtod(buf, &end);
        if (errno == 0 && end != buf && *end == 0 && n == s->len) good = true;
    }
    if (ok) *ok = good;
    if (!good) vl_panic("cannot convert string to float");
    return v;
}

/* ========================================================================== */
/* 动态数组                                                                    */
/* ========================================================================== */
VL_NORETURN void vl_index_error(int64_t i, int64_t len) {
    char buf[160];
    snprintf(buf, sizeof(buf), "index out of range: %lld (length %lld)",
             (long long)i, (long long)len);
    vl_panic(buf);
}

VL_NORETURN void vl_div_zero_error(void) {
    vl_panic("integer division by zero");
}

#define VL_LIST_IMPL(NAME, T)                                                        \
    vl_##NAME* vl_##NAME##_new(void) {                                               \
        vl_##NAME* l = (vl_##NAME*)vl_alloc((int64_t)sizeof(vl_##NAME));             \
        l->rc = 1; l->len = 0; l->cap = 0; l->data = NULL;                           \
        return l;                                                                    \
    }                                                                                \
    void vl_##NAME##_reserve(vl_##NAME* l, int64_t n) {                              \
        int64_t cap;                                                                 \
        if (n <= l->cap) return;                                                     \
        cap = l->cap ? l->cap : 8;                                                   \
        while (cap < n) cap = cap + (cap >> 1) + 1;                                  \
        l->data = (T*)vl_realloc(l->data, cap * (int64_t)sizeof(T));                 \
        l->cap = cap;                                                                \
    }                                                                                \
    vl_##NAME* vl_##NAME##_with_capacity(int64_t n) {                                \
        vl_##NAME* l = vl_##NAME##_new();                                            \
        if (n > 0) vl_##NAME##_reserve(l, n);                                        \
        return l;                                                                    \
    }                                                                                \
    void vl_##NAME##_push(vl_##NAME* l, T v) {                                       \
        if (l->len == l->cap) vl_##NAME##_reserve(l, l->cap + (l->cap >> 1) + 4);    \
        l->data[l->len++] = v;                                                       \
    }                                                                                \
    T vl_##NAME##_pop(vl_##NAME* l) {                                                \
        if (!l || l->len <= 0) vl_panic("pop from empty list");                      \
        return l->data[--l->len];                                                    \
    }                                                                                \
    void vl_##NAME##_insert(vl_##NAME* l, int64_t i, T v) {                          \
        if (i < 0) i += l->len;                                                      \
        if (i < 0 || i > l->len) vl_index_error(i, l->len);                          \
        if (l->len == l->cap) vl_##NAME##_reserve(l, l->cap + (l->cap >> 1) + 4);    \
        memmove(l->data + i + 1, l->data + i,                                        \
                (size_t)((l->len - i) * (int64_t)sizeof(T)));                        \
        l->data[i] = v;                                                              \
        l->len++;                                                                    \
    }                                                                                \
    void vl_##NAME##_remove(vl_##NAME* l, int64_t i) {                               \
        if (i < 0) i += l->len;                                                      \
        if (i < 0 || i >= l->len) vl_index_error(i, l->len);                         \
        memmove(l->data + i, l->data + i + 1,                                        \
                (size_t)((l->len - i - 1) * (int64_t)sizeof(T)));                    \
        l->len--;                                                                    \
    }                                                                                \
    void vl_##NAME##_clear(vl_##NAME* l) { if (l) l->len = 0; }                      \
    void vl_##NAME##_fill(vl_##NAME* l, int64_t n, T v) {                            \
        int64_t i;                                                                   \
        if (n < 0) n = 0;                                                            \
        vl_##NAME##_reserve(l, n);                                                   \
        for (i = 0; i < n; i++) l->data[i] = v;                                      \
        l->len = n;                                                                  \
    }                                                                                \
    void vl_##NAME##_free(vl_##NAME* l) {                                            \
        if (!l) return;                                                              \
        vl_dealloc(l->data);                                                         \
        vl_dealloc(l);                                                               \
    }

VL_LIST_IMPL(i64list, int64_t)
VL_LIST_IMPL(f64list, double)
VL_LIST_IMPL(i8list, int8_t)
VL_LIST_IMPL(i16list, int16_t)
VL_LIST_IMPL(i32list, int32_t)
VL_LIST_IMPL(u8list, uint8_t)
VL_LIST_IMPL(u16list, uint16_t)
VL_LIST_IMPL(u32list, uint32_t)
VL_LIST_IMPL(u64list, uint64_t)
VL_LIST_IMPL(boollist, uint8_t)

/* ---- str 列表：逐元素引用计数 ---- */
vl_strlist* vl_strlist_new(void) {
    vl_strlist* l = (vl_strlist*)vl_alloc((int64_t)sizeof(vl_strlist));
    l->rc = 1; l->len = 0; l->cap = 0; l->data = NULL;
    return l;
}

void vl_strlist_reserve(vl_strlist* l, int64_t n) {
    int64_t cap;
    if (n <= l->cap) return;
    cap = l->cap ? l->cap : 8;
    while (cap < n) cap = cap + (cap >> 1) + 1;
    l->data = (vl_str**)vl_realloc(l->data, cap * (int64_t)sizeof(vl_str*));
    l->cap = cap;
}

vl_strlist* vl_strlist_with_capacity(int64_t n) {
    vl_strlist* l = vl_strlist_new();
    if (n > 0) vl_strlist_reserve(l, n);
    return l;
}

void vl_strlist_push(vl_strlist* l, vl_str* v) {
    if (l->len == l->cap) vl_strlist_reserve(l, l->cap + (l->cap >> 1) + 4);
    l->data[l->len++] = v;
}

vl_str* vl_strlist_pop(vl_strlist* l) {
    if (!l || l->len <= 0) vl_panic("pop from empty list");
    return l->data[--l->len];
}

void vl_strlist_insert(vl_strlist* l, int64_t i, vl_str* v) {
    if (i < 0) i += l->len;
    if (i < 0 || i > l->len) vl_index_error(i, l->len);
    if (l->len == l->cap) vl_strlist_reserve(l, l->cap + (l->cap >> 1) + 4);
    memmove(l->data + i + 1, l->data + i, (size_t)((l->len - i) * (int64_t)sizeof(vl_str*)));
    l->data[i] = v;
    l->len++;
}

void vl_strlist_remove(vl_strlist* l, int64_t i) {
    vl_str* old;
    if (i < 0) i += l->len;
    if (i < 0 || i >= l->len) vl_index_error(i, l->len);
    old = l->data[i];
    memmove(l->data + i, l->data + i + 1, (size_t)((l->len - i - 1) * (int64_t)sizeof(vl_str*)));
    l->len--;
    vl_str_release(old);
}

void vl_strlist_clear(vl_strlist* l) {
    int64_t i;
    if (!l) return;
    for (i = 0; i < l->len; i++) vl_str_release(l->data[i]);
    l->len = 0;
}

void vl_strlist_fill(vl_strlist* l, int64_t n, vl_str* v) {
    int64_t i;
    if (n < 0) n = 0;
    vl_strlist_reserve(l, n);
    for (i = 0; i < n; i++) l->data[i] = vl_str_retain(v);
    l->len = n;
}

void vl_strlist_free(vl_strlist* l) {
    int64_t i;
    if (!l) return;
    for (i = 0; i < l->len; i++) vl_str_release(l->data[i]);
    vl_dealloc(l->data);
    vl_dealloc(l);
}

/* ---- 排序 / 反转 / 查找 ---- */
static int cmp_i64(const void* a, const void* b) {
    int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}
static int cmp_f64(const void* a, const void* b) {
    double x = *(const double*)a, y = *(const double*)b;
    return x < y ? -1 : (x > y ? 1 : 0);
}
static int cmp_str(const void* a, const void* b) {
    const vl_str* x = *(const vl_str* const*)a;
    const vl_str* y = *(const vl_str* const*)b;
    int64_t c = vl_str_cmp(x, y);
    return c < 0 ? -1 : (c > 0 ? 1 : 0);
}

void vl_i64list_sort(vl_i64list* l) { if (l && l->len > 1) qsort(l->data, (size_t)l->len, sizeof(int64_t), cmp_i64); }
void vl_f64list_sort(vl_f64list* l) { if (l && l->len > 1) qsort(l->data, (size_t)l->len, sizeof(double), cmp_f64); }
void vl_strlist_sort(vl_strlist* l) { if (l && l->len > 1) qsort(l->data, (size_t)l->len, sizeof(vl_str*), cmp_str); }

#define VL_REV_IMPL(NAME, T)                                                    \
    void vl_##NAME##_reverse(vl_##NAME* l) {                                    \
        int64_t i, j;                                                           \
        if (!l || l->len < 2) return;                                           \
        for (i = 0, j = l->len - 1; i < j; i++, j--) {                          \
            T t = l->data[i]; l->data[i] = l->data[j]; l->data[j] = t;          \
        }                                                                       \
    }
VL_REV_IMPL(i64list, int64_t)
VL_REV_IMPL(f64list, double)
VL_REV_IMPL(strlist, vl_str*)

int64_t vl_i64list_index_of(const vl_i64list* l, int64_t v) {
    int64_t i;
    if (!l) return -1;
    for (i = 0; i < l->len; i++) if (l->data[i] == v) return i;
    return -1;
}

int64_t vl_f64list_index_of(const vl_f64list* l, double v) {
    int64_t i;
    if (!l) return -1;
    for (i = 0; i < l->len; i++) if (l->data[i] == v) return i;
    return -1;
}

int64_t vl_strlist_index_of(const vl_strlist* l, const vl_str* v) {
    int64_t i;
    if (!l) return -1;
    for (i = 0; i < l->len; i++) if (vl_str_eq(l->data[i], v)) return i;
    return -1;
}

/* ---- 字符串切分 / 连接 ---- */
vl_strlist* vl_str_split(const vl_str* s, const vl_str* sep) {
    vl_strlist* out = vl_strlist_new();
    int64_t pos = 0;
    if (!s) { vl_strlist_push(out, vl_str_alloc(0)); return out; }
    if (!sep || sep->len == 0) {
        int64_t i = 0;
        if (s->len == 0) { vl_strlist_push(out, vl_str_alloc(0)); return out; }
        while (i < s->len) {
            unsigned char c = (unsigned char)s->data[i];
            int k = (c < 0x80) ? 1 : ((c & 0xE0) == 0xC0 ? 2 : ((c & 0xF0) == 0xE0 ? 3 : ((c & 0xF8) == 0xF0 ? 4 : 1)));
            if (i + k > s->len) k = 1;
            vl_strlist_push(out, vl_str_new_len(s->data + i, k));
            i += k;
        }
        return out;
    }
    for (;;) {
        int64_t idx = find_bytes(s->data, s->len, sep->data, sep->len, pos);
        if (idx < 0) {
            vl_strlist_push(out, vl_str_new_len(s->data + pos, s->len - pos));
            break;
        }
        vl_strlist_push(out, vl_str_new_len(s->data + pos, idx - pos));
        pos = idx + sep->len;
    }
    return out;
}

vl_strlist* vl_str_split_ws(const vl_str* s) {
    vl_strlist* out = vl_strlist_new();
    int64_t i = 0;
    if (!s) return out;
    while (i < s->len) {
        int64_t start;
        while (i < s->len && is_space_ch((unsigned char)s->data[i])) i++;
        if (i >= s->len) break;
        start = i;
        while (i < s->len && !is_space_ch((unsigned char)s->data[i])) i++;
        vl_strlist_push(out, vl_str_new_len(s->data + start, i - start));
    }
    return out;
}

vl_str* vl_str_join(const vl_strlist* parts, const vl_str* sep) {
    vl_sb b;
    int64_t i, n = parts ? parts->len : 0;
    vl_sb_init(&b);
    for (i = 0; i < n; i++) {
        if (i > 0) vl_sb_put_str(&b, sep);
        vl_sb_put_str(&b, parts->data[i]);
    }
    return vl_sb_finish(&b);
}

/* ========================================================================== */
/* 输出                                                                        */
/* ========================================================================== */
#define VL_OUTBUF_SIZE (1 << 16)
static char  g_outbuf[VL_OUTBUF_SIZE];
static int64_t g_outlen = 0;

void vl_out_flush(void) {
    if (g_outlen > 0) {
        fwrite(g_outbuf, 1, (size_t)g_outlen, stdout);
        g_outlen = 0;
    }
    fflush(stdout);
}

void vl_out_bytes(const char* p, int64_t n) {
    if (!p || n <= 0) return;
    if (n >= VL_OUTBUF_SIZE) {
        vl_out_flush();
        fwrite(p, 1, (size_t)n, stdout);
        return;
    }
    if (g_outlen + n > VL_OUTBUF_SIZE) vl_out_flush();
    memcpy(g_outbuf + g_outlen, p, (size_t)n);
    g_outlen += n;
}

void vl_out_cstr(const char* c) { if (c) vl_out_bytes(c, (int64_t)strlen(c)); }
void vl_out_char(char c) { vl_out_bytes(&c, 1); }

void vl_out_i64(int64_t v) {
    char tmp[24];
    int n = i64_to_buf(tmp, v);
    vl_out_bytes(tmp, n);
}

void vl_out_u64(uint64_t v) {
    char tmp[24];
    int n = u64_to_buf(tmp, v);
    vl_out_bytes(tmp, n);
}

void vl_out_f64(double v) {
    char tmp[40];
    int n = f64_to_buf(tmp, (int)sizeof(tmp), v);
    vl_out_bytes(tmp, n);
}

void vl_out_bool(bool v) { vl_out_cstr(v ? "True" : "False"); }

void vl_out_str(const vl_str* s) { if (s) vl_out_bytes(s->data, s->len); }

void vl_out_repr_str(const vl_str* s) {
    int64_t i;
    vl_out_char('"');
    if (s) {
        for (i = 0; i < s->len; i++) {
            unsigned char c = (unsigned char)s->data[i];
            switch (c) {
                case '"':  vl_out_cstr("\\\""); break;
                case '\\': vl_out_cstr("\\\\"); break;
                case '\n': vl_out_cstr("\\n"); break;
                case '\t': vl_out_cstr("\\t"); break;
                case '\r': vl_out_cstr("\\r"); break;
                default:
                    if (c < 0x20) {
                        char tmp[8];
                        snprintf(tmp, sizeof(tmp), "\\x%02x", (unsigned)c);
                        vl_out_cstr(tmp);
                    } else {
                        vl_out_char((char)c);
                    }
            }
        }
    }
    vl_out_char('"');
}

void vl_out_space(void) { vl_out_char(' '); }
void vl_out_endl(void) { vl_out_char('\n'); }

void vl_err_bytes(const char* p, int64_t n) {
    if (!p || n <= 0) return;
    vl_out_flush();
    fwrite(p, 1, (size_t)n, stderr);
}
void vl_err_cstr(const char* c) { if (c) vl_err_bytes(c, (int64_t)strlen(c)); }
void vl_err_i64(int64_t v) {
    char tmp[24];
    int n = i64_to_buf(tmp, v);
    vl_err_bytes(tmp, n);
}
void vl_err_f64(double v) {
    char tmp[40];
    int n = f64_to_buf(tmp, (int)sizeof(tmp), v);
    vl_err_bytes(tmp, n);
}
void vl_err_str(const vl_str* s) { if (s) vl_err_bytes(s->data, s->len); }
void vl_err_endl(void) { vl_err_bytes("\n", 1); }

/* ========================================================================== */
/* 错误                                                                        */
/* ========================================================================== */
VL_NORETURN void vl_panic(const char* msg) {
    vl_out_flush();
    fputs("panic: ", stderr);
    fputs(msg ? msg : "(unknown)", stderr);
    fputc('\n', stderr);
    fflush(stderr);
    exit(1);
}

VL_NORETURN void vl_panic_str(const vl_str* msg) {
    vl_out_flush();
    fputs("panic: ", stderr);
    if (msg) fwrite(msg->data, 1, (size_t)msg->len, stderr);
    fputc('\n', stderr);
    fflush(stderr);
    exit(1);
}

void vl_assert_fail(const char* expr, const char* file, int64_t line) {
    char buf[512];
    vl_out_flush();
    snprintf(buf, sizeof(buf), "assertion failed: %s (%s:%lld)",
             expr ? expr : "?", file ? file : "?", (long long)line);
    vl_panic(buf);
}

/* ========================================================================== */
/* 初始化 / 命令行参数                                                          */
/* ========================================================================== */
int32_t vl_argc = 0;
char**  vl_argv = NULL;

void vl_rt_init(void) {
#if defined(_WIN32)
    SetConsoleOutputCP(65001);
    SetConsoleCP(65001);
#endif
    atexit(vl_out_flush);
}

void vl_rt_shutdown(void) { vl_out_flush(); }

int64_t vl_arg_count(void) { return vl_argc > 1 ? (int64_t)(vl_argc - 1) : 0; }

vl_str* vl_arg_get(int64_t i) {
    if (i < 0 || i + 1 >= (int64_t)vl_argc) return vl_str_alloc(0);
    return vl_str_new(vl_argv[i + 1]);
}

/* ========================================================================== */
/* 文件                                                                        */
/* ========================================================================== */
#if defined(_WIN32)
static wchar_t* utf8_to_wide(const char* s) {
    int n;
    wchar_t* w;
    if (!s) s = "";
    n = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
    if (n <= 0) return NULL;
    w = (wchar_t*)vl_alloc((int64_t)n * (int64_t)sizeof(wchar_t));
    MultiByteToWideChar(CP_UTF8, 0, s, -1, w, n);
    return w;
}
#endif

vl_str* vl_read_file(const vl_str* path) {
    FILE* f = NULL;
    char* buf = NULL;
    int64_t cap = 0, len = 0;
    if (!path) return vl_str_alloc(0);
#if defined(_WIN32)
    {
        wchar_t* wp = utf8_to_wide(path->data);
        if (!wp) return vl_str_alloc(0);
        f = _wfopen(wp, L"rb");
        vl_dealloc(wp);
    }
#else
    f = fopen(path->data, "rb");
#endif
    if (!f) return vl_str_alloc(0);
    cap = 8192;
    buf = (char*)vl_alloc(cap);
    for (;;) {
        size_t got;
        if (len + 1 >= cap) {
            cap *= 2;
            buf = (char*)vl_realloc(buf, cap);
        }
        got = fread(buf + len, 1, (size_t)(cap - len - 1), f);
        if (got == 0) break;
        len += (int64_t)got;
    }
    fclose(f);
    {
        vl_str* s = vl_str_new_len(buf, len);
        vl_dealloc(buf);
        return s;
    }
}

int64_t vl_write_file(const vl_str* path, const vl_str* content) {
    FILE* f = NULL;
    size_t wrote = 0;
    const char* data = content ? content->data : "";
    int64_t len = content ? content->len : 0;
    if (!path) return -1;
#if defined(_WIN32)
    {
        wchar_t* wp = utf8_to_wide(path->data);
        if (!wp) return -1;
        f = _wfopen(wp, L"wb");
        vl_dealloc(wp);
    }
#else
    f = fopen(path->data, "wb");
#endif
    if (!f) return -1;
    if (len > 0) wrote = fwrite(data, 1, (size_t)len, f);
    fclose(f);
    return (int64_t)wrote;
}

bool vl_file_exists(const vl_str* path) {
    if (!path) return false;
#if defined(_WIN32)
    {
        wchar_t* wp = utf8_to_wide(path->data);
        DWORD attr;
        if (!wp) return false;
        attr = GetFileAttributesW(wp);
        vl_dealloc(wp);
        return attr != INVALID_FILE_ATTRIBUTES;
    }
#else
    return access(path->data, 0) == 0;
#endif
}

/* ========================================================================== */
/* 数学辅助（其余内联函数在 velo_rt.h 中）                                       */
/* ========================================================================== */
