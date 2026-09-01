#include "../include/string.h"

void *memset(void *dest, int val, size_t count) {
    uint8_t *temp = (uint8_t *)dest;
    for (size_t i = 0; i < count; i++) {
        temp[i] = (uint8_t)val;
    }
    return dest;
}

void *memcpy(void *dest, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    for (size_t i = 0; i < count; i++) {
        d[i] = s[i];
    }
    return dest;
}

void *memmove(void *dest, const void *src, size_t count) {
    uint8_t *d = (uint8_t *)dest;
    const uint8_t *s = (const uint8_t *)src;
    if (d < s) {
        for (size_t i = 0; i < count; i++) {
            d[i] = s[i];
        }
    } else if (d > s) {
        for (size_t i = count; i > 0; i--) {
            d[i - 1] = s[i - 1];
        }
    }
    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *p1 = (const uint8_t *)s1;
    const uint8_t *p2 = (const uint8_t *)s2;
    for (size_t i = 0; i < n; i++) {
        if (p1[i] != p2[i]) {
            return p1[i] - p2[i];
        }
    }
    return 0;
}

size_t strlen(const char *str) {
    size_t len = 0;
    while (str[len] != '\0') {
        len++;
    }
    return len;
}

char *strcpy(char *dest, const char *src) {
    char *ret = dest;
    while ((*dest++ = *src++) != '\0');
    return ret;
}

char *strncpy(char *dest, const char *src, size_t n) {
    size_t i;
    for (i = 0; i < n && src[i] != '\0'; i++) {
        dest[i] = src[i];
    }
    for (; i < n; i++) {
        dest[i] = '\0';
    }
    return dest;
}

int strcmp(const char *s1, const char *s2) {
    while (*s1 && (*s1 == *s2)) {
        s1++;
        s2++;
    }
    return *(const unsigned char *)s1 - *(const unsigned char *)s2;
}

int strncmp(const char *s1, const char *s2, size_t n) {
    for (size_t i = 0; i < n; i++) {
        if (s1[i] != s2[i] || s1[i] == '\0') {
            return (unsigned char)s1[i] - (unsigned char)s2[i];
        }
    }
    return 0;
}

char *strcat(char *dest, const char *src) {
    char *d = dest;
    while (*d) d++;
    while ((*d++ = *src++) != '\0');
    return dest;
}

char *utoa(uint64_t value, char *dest, int base) {
    if (base < 2 || base > 16) {
        dest[0] = '\0';
        return dest;
    }
    static const char digits[] = "0123456789abcdef";
    char tmp[65];
    int i = 0;

    if (value == 0) {
        dest[0] = '0';
        dest[1] = '\0';
        return dest;
    }

    while (value > 0) {
        tmp[i++] = digits[value % base];
        value /= base;
    }

    int j = 0;
    while (i > 0) {
        dest[j++] = tmp[--i];
    }
    dest[j] = '\0';
    return dest;
}

char *itoa(int64_t value, char *dest, int base) {
    if (value < 0 && base == 10) {
        dest[0] = '-';
        utoa((uint64_t)(-value), dest + 1, base);
    } else {
        utoa((uint64_t)value, dest, base);
    }
    return dest;
}

// Variadic argument handling (freestanding)
typedef __builtin_va_list va_list;
#define va_start(ap, last) __builtin_va_start(ap, last)
#define va_arg(ap, type)   __builtin_va_arg(ap, type)
#define va_end(ap)         __builtin_va_end(ap)

int ksnprintf(char *buf, size_t size, const char *fmt, ...) {
    if (!buf || size == 0 || !fmt) return 0;

    va_list args;
    va_start(args, fmt);

    size_t pos = 0;

    for (size_t i = 0; fmt[i] != '\0' && pos < size - 1; i++) {
        if (fmt[i] == '%' && fmt[i + 1] != '\0') {
            i++;
            switch (fmt[i]) {
                case 's': {
                    const char *s = va_arg(args, const char *);
                    if (!s) s = "(null)";
                    while (*s && pos < size - 1) {
                        buf[pos++] = *s++;
                    }
                    break;
                }
                case 'd': {
                    int64_t val = va_arg(args, int64_t);
                    char tmp[32];
                    itoa(val, tmp, 10);
                    for (int j = 0; tmp[j] && pos < size - 1; j++) {
                        buf[pos++] = tmp[j];
                    }
                    break;
                }
                case 'u': {
                    uint64_t val = va_arg(args, uint64_t);
                    char tmp[32];
                    utoa(val, tmp, 10);
                    for (int j = 0; tmp[j] && pos < size - 1; j++) {
                        buf[pos++] = tmp[j];
                    }
                    break;
                }
                case 'x': {
                    uint64_t val = va_arg(args, uint64_t);
                    char tmp[32];
                    utoa(val, tmp, 16);
                    for (int j = 0; tmp[j] && pos < size - 1; j++) {
                        buf[pos++] = tmp[j];
                    }
                    break;
                }
                case '%':
                    buf[pos++] = '%';
                    break;
                default:
                    buf[pos++] = '%';
                    if (pos < size - 1) buf[pos++] = fmt[i];
                    break;
            }
        } else {
            buf[pos++] = fmt[i];
        }
    }

    buf[pos] = '\0';
    va_end(args);
    return (int)pos;
}
