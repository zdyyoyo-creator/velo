/* ============================================================================
 * velo_rt.h -- Velo 语言运行时库
 *
 * 设计目标：
 *   1. 零隐藏开销：数值运算不经过任何运行时调用，直接映射为机器指令。
 *   2. 字符串 / 动态数组使用引用计数，编译期自动插入 retain / release。
 *   3. 所有热路径（push / get / len / retain）都是 static inline，可被内联。
 *   4. 输出走 64KB 缓冲，避免逐字符 printf 的系统调用开销。
 *
 * 兼容：MSVC 19.x (C11) / GCC / Clang
 * ========================================================================== */
#ifndef VELO_RT_H
#define VELO_RT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <math.h>

/* ---------------------------------------------------------------- 平台宏 */
#if defined(_MSC_VER)
#  define VL_NORETURN __declspec(noreturn)
#  define VL_INLINE   static __inline
#  define VL_EXPORT
#else
#  define VL_NORETURN __attribute__((noreturn))
#  define VL_INLINE   static inline
#  define VL_EXPORT   __attribute__((visibility("default")))
#endif

#ifndef VL_NAN_PANIC
#  define VL_NAN_PANIC 0
#endif

/* 边界检查开关：默认开启（安全）。定义 VL_BOUNDS_CHECK=0 可关闭，
 * 用 --fast 构建时会自动关闭，性能可进一步提升。 */
#ifndef VL_BOUNDS_CHECK
#  define VL_BOUNDS_CHECK 1
#endif

/* ---------------------------------------------------------------- 内存 */
void* vl_alloc(int64_t n);
void* vl_alloc_zero(int64_t n);
void* vl_realloc(void* p, int64_t n);
void  vl_dealloc(void* p);

/* ---------------------------------------------------------------- 字符串 */
/* vl_str 是不可变的 UTF-8 字节串，引用计数。data 保证以 '\0' 结尾，
 * 因此可以直接传给需要 C 字符串的 API。 */
typedef struct vl_str {
    int64_t rc;
    int64_t len;   /* 字节长度（不含结尾 '\0'） */
    char*   data;  /* 保证 data[len] == '\0' */
} vl_str;

vl_str* vl_str_new(const char* c);
vl_str* vl_str_new_len(const char* p, int64_t n);
vl_str* vl_str_alloc(int64_t n);      /* len = n，内容未初始化，末尾已补 '\0' */
void    vl_str_free(vl_str* s);

VL_INLINE vl_str* vl_str_retain(vl_str* s)  { if (s) s->rc++; return s; }
VL_INLINE void    vl_str_release(vl_str* s) { if (s && --s->rc == 0) vl_str_free(s); }
VL_INLINE int64_t vl_str_len(const vl_str* s)  { return s ? s->len : 0; }
VL_INLINE const char* vl_str_cstr(const vl_str* s) { return s ? s->data : ""; }

/* 字符串构建器：插值 / 拼接 / 数字转字符串都走这里，一次分配 */
typedef struct vl_sb {
    char*   buf;
    int64_t len;
    int64_t cap;
} vl_sb;

void    vl_sb_init(vl_sb* b);
void    vl_sb_put_bytes(vl_sb* b, const char* p, int64_t n);
void    vl_sb_put_cstr(vl_sb* b, const char* c);
void    vl_sb_put_char(vl_sb* b, char c);
void    vl_sb_put_i64(vl_sb* b, int64_t v);
void    vl_sb_put_u64(vl_sb* b, uint64_t v);
void    vl_sb_put_f64(vl_sb* b, double v);
void    vl_sb_put_bool(vl_sb* b, bool v);
void    vl_sb_put_str(vl_sb* b, const vl_str* s);
void    vl_sb_put_str_repr(vl_sb* b, const vl_str* s);  /* 带引号与转义 */
vl_str* vl_sb_finish(vl_sb* b);

/* 字符串运算，全部返回"新引用"（所有权交给调用者） */
vl_str* vl_str_concat(const vl_str* a, const vl_str* b);
vl_str* vl_str_slice(const vl_str* s, int64_t start, int64_t end);
vl_str* vl_str_repeat(const vl_str* s, int64_t n);
vl_str* vl_str_upper(const vl_str* s);
vl_str* vl_str_lower(const vl_str* s);
vl_str* vl_str_trim(const vl_str* s);
vl_str* vl_str_trim_start(const vl_str* s);
vl_str* vl_str_trim_end(const vl_str* s);
vl_str* vl_str_replace(const vl_str* s, const vl_str* old_s, const vl_str* new_s);
vl_str* vl_str_char_at(const vl_str* s, int64_t i);
vl_str* vl_str_from_i64(int64_t v);
vl_str* vl_str_from_u64(uint64_t v);
vl_str* vl_str_from_f64(double v);
vl_str* vl_str_from_bool(bool v);

bool    vl_str_eq(const vl_str* a, const vl_str* b);
int64_t vl_str_cmp(const vl_str* a, const vl_str* b);
bool    vl_str_lt(const vl_str* a, const vl_str* b);
bool    vl_str_le(const vl_str* a, const vl_str* b);
bool    vl_str_gt(const vl_str* a, const vl_str* b);
bool    vl_str_ge(const vl_str* a, const vl_str* b);
int64_t vl_str_find(const vl_str* hay, const vl_str* needle, int64_t start);
int64_t vl_str_rfind(const vl_str* hay, const vl_str* needle);
bool    vl_str_contains(const vl_str* hay, const vl_str* needle);
bool    vl_str_starts_with(const vl_str* s, const vl_str* prefix);
bool    vl_str_ends_with(const vl_str* s, const vl_str* suffix);
uint64_t vl_str_hash(const vl_str* s);
int64_t vl_str_utf8_len(const vl_str* s);              /* 码点数量 */
int64_t vl_str_byte_at(const vl_str* s, int64_t i);    /* 越界则 panic */
int64_t vl_str_to_int(const vl_str* s, bool* ok);
double  vl_str_to_float(const vl_str* s, bool* ok);

/* ---------------------------------------------------------------- 动态数组 */
/* 布局统一为 { rc, len, cap, T* data }，这样不同元素类型之间可以共用
 * 通用的辅助逻辑，也方便 codegen 与运行时互操作。 */
typedef struct vl_strlist vl_strlist;   /* 前置声明，供下方函数原型使用 */

/* 下标越界检查（--fast 构建时可关闭以获得极限性能） */
#if VL_BOUNDS_CHECK
#  define VL_LIST_IDX_CHECK(l, i) \
       do { if (!(l) || (i) < 0 || (i) >= (l)->len) vl_index_error((i), (l) ? (l)->len : 0); } while (0)
#else
#  define VL_LIST_IDX_CHECK(l, i) ((void)0)
#endif

#define VL_LIST_DECLARE(NAME, T)                                                    \
    typedef struct vl_##NAME { int64_t rc, len, cap; T* data; } vl_##NAME;          \
    vl_##NAME*  vl_##NAME##_new(void);                                             \
    vl_##NAME*  vl_##NAME##_with_capacity(int64_t n);                              \
    void        vl_##NAME##_reserve(vl_##NAME* l, int64_t n);                      \
    void        vl_##NAME##_push(vl_##NAME* l, T v);                               \
    T           vl_##NAME##_pop(vl_##NAME* l);                                     \
    void        vl_##NAME##_insert(vl_##NAME* l, int64_t i, T v);                  \
    void        vl_##NAME##_remove(vl_##NAME* l, int64_t i);                       \
    void        vl_##NAME##_clear(vl_##NAME* l);                                   \
    void        vl_##NAME##_fill(vl_##NAME* l, int64_t n, T v);                    \
    void        vl_##NAME##_free(vl_##NAME* l);                                    \
    VL_INLINE vl_##NAME* vl_##NAME##_retain(vl_##NAME* l) { if (l) l->rc++; return l; } \
    VL_INLINE void vl_##NAME##_release(vl_##NAME* l) { if (l && --l->rc == 0) vl_##NAME##_free(l); } \
    VL_INLINE int64_t vl_##NAME##_len(const vl_##NAME* l) { return l ? l->len : 0; } \
    VL_INLINE int64_t vl_##NAME##_cap(const vl_##NAME* l) { return l ? l->cap : 0; } \
    VL_INLINE T vl_##NAME##_get(const vl_##NAME* l, int64_t i) {                    \
        VL_LIST_IDX_CHECK(l, i);                                                    \
        return l->data[i];                                                          \
    }                                                                               \
    VL_INLINE void vl_##NAME##_set(vl_##NAME* l, int64_t i, T v) {                  \
        VL_LIST_IDX_CHECK(l, i);                                                    \
        l->data[i] = v;                                                             \
    }

VL_NORETURN void vl_index_error(int64_t i, int64_t len);

/* 基础元素类型（数值 / 布尔）：值语义，无引用计数的元素 */
VL_LIST_DECLARE(i64list, int64_t)
VL_LIST_DECLARE(f64list, double)
VL_LIST_DECLARE(i8list,  int8_t)
VL_LIST_DECLARE(i16list, int16_t)
VL_LIST_DECLARE(i32list, int32_t)
VL_LIST_DECLARE(u8list,  uint8_t)
VL_LIST_DECLARE(u16list, uint16_t)
VL_LIST_DECLARE(u32list, uint32_t)
VL_LIST_DECLARE(u64list, uint64_t)
VL_LIST_DECLARE(boollist, uint8_t)

/* str 元素：需要逐元素 retain / release */
struct vl_strlist { int64_t rc, len, cap; vl_str** data; };

vl_strlist* vl_strlist_new(void);
vl_strlist* vl_strlist_with_capacity(int64_t n);
void        vl_strlist_reserve(vl_strlist* l, int64_t n);
void        vl_strlist_push(vl_strlist* l, vl_str* v);
vl_str*     vl_strlist_pop(vl_strlist* l);
void        vl_strlist_insert(vl_strlist* l, int64_t i, vl_str* v);
void        vl_strlist_remove(vl_strlist* l, int64_t i);
void        vl_strlist_clear(vl_strlist* l);
void        vl_strlist_fill(vl_strlist* l, int64_t n, vl_str* v);
void        vl_strlist_free(vl_strlist* l);
VL_INLINE vl_strlist* vl_strlist_retain(vl_strlist* l) { if (l) l->rc++; return l; }
VL_INLINE void vl_strlist_release(vl_strlist* l) { if (l && --l->rc == 0) vl_strlist_free(l); }
VL_INLINE int64_t vl_strlist_len(const vl_strlist* l) { return l ? l->len : 0; }
VL_INLINE int64_t vl_strlist_cap(const vl_strlist* l) { return l ? l->cap : 0; }
VL_INLINE vl_str* vl_strlist_get(const vl_strlist* l, int64_t i) {
    VL_LIST_IDX_CHECK(l, i);
    return l->data[i];
}
VL_INLINE void vl_strlist_set(vl_strlist* l, int64_t i, vl_str* v) {
    VL_LIST_IDX_CHECK(l, i);
    vl_str_retain(v);
    vl_str_release(l->data[i]);
    l->data[i] = v;
}

/* 排序 / 查找 / 反转 */
void vl_i64list_sort(vl_i64list* l);
void vl_f64list_sort(vl_f64list* l);
void vl_strlist_sort(vl_strlist* l);
void vl_i64list_reverse(vl_i64list* l);
void vl_f64list_reverse(vl_f64list* l);
void vl_strlist_reverse(vl_strlist* l);
int64_t vl_i64list_index_of(const vl_i64list* l, int64_t v);
int64_t vl_f64list_index_of(const vl_f64list* l, double v);
int64_t vl_strlist_index_of(const vl_strlist* l, const vl_str* v);

/* 字符串 <-> 列表 */
vl_strlist* vl_str_split(const vl_str* s, const vl_str* sep);
vl_strlist* vl_str_split_ws(const vl_str* s);
vl_str*     vl_str_join(const vl_strlist* parts, const vl_str* sep);

/* ---------------------------------------------------------------- 输出 */
/* 缓冲输出，默认 64KB。程序正常退出时自动 flush。 */
void vl_out_bytes(const char* p, int64_t n);
void vl_out_cstr(const char* c);
void vl_out_char(char c);
void vl_out_i64(int64_t v);
void vl_out_u64(uint64_t v);
void vl_out_f64(double v);
void vl_out_bool(bool v);
void vl_out_str(const vl_str* s);
void vl_out_repr_str(const vl_str* s);
void vl_out_space(void);
void vl_out_endl(void);
void vl_out_flush(void);

void vl_err_bytes(const char* p, int64_t n);
void vl_err_cstr(const char* c);
void vl_err_i64(int64_t v);
void vl_err_f64(double v);
void vl_err_str(const vl_str* s);
void vl_err_endl(void);

/* ---------------------------------------------------------------- 错误 */
VL_NORETURN void vl_panic(const char* msg);
VL_NORETURN void vl_panic_str(const vl_str* msg);
VL_NORETURN void vl_div_zero_error(void);
void vl_assert_fail(const char* expr, const char* file, int64_t line);

/* ---------------------------------------------------------------- 文件 */
vl_str* vl_read_file(const vl_str* path);
int64_t vl_write_file(const vl_str* path, const vl_str* content);
bool    vl_file_exists(const vl_str* path);

/* ---------------------------------------------------------------- 初始化 */
void vl_rt_init(void);
void vl_rt_shutdown(void);

/* 命令行参数 */
extern int32_t  vl_argc;
extern char**   vl_argv;
int64_t  vl_arg_count(void);
vl_str*  vl_arg_get(int64_t i);

/* ---------------------------------------------------------------- 数学 */
VL_INLINE int64_t vl_imod(int64_t a, int64_t b) {  /* Python 风格取模，结果与除数同号 */
    int64_t r;
    if (b == 0) vl_div_zero_error();
    r = a % b;
    if (r != 0 && ((r < 0) != (b < 0))) r += b;
    return r;
}
VL_INLINE int64_t vl_idiv(int64_t a, int64_t b) {  /* Python 风格整除 */
    if (b == 0) vl_div_zero_error();
    int64_t q = a / b;
    if ((a % b != 0) && ((a < 0) != (b < 0))) q -= 1;
    return q;
}
VL_INLINE int64_t vl_ipow(int64_t base, int64_t exp) {
    int64_t r = 1;
    while (exp > 0) {
        if (exp & 1) r *= base;
        base *= base;
        exp >>= 1;
    }
    return r;
}
VL_INLINE double vl_fpow(double base, double exp) { return pow(base, exp); }

#endif /* VELO_RT_H */
