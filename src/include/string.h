#ifndef STRING_H
#define STRING_H

#include "types.h"

void *memset(void *dest, int val, size_t count);
void *memcpy(void *dest, const void *src, size_t count);
void *memmove(void *dest, const void *src, size_t count);
int   memcmp(const void *s1, const void *s2, size_t n);

size_t strlen(const char *str);
char  *strcpy(char *dest, const char *src);
char  *strncpy(char *dest, const char *src, size_t n);
char  *strcat(char *dest, const char *src);
int    strcmp(const char *s1, const char *s2);
int    strncmp(const char *s1, const char *s2, size_t n);

/**
 * Convert integer to string (decimal). Returns pointer to dest.
 */
char *itoa(int64_t value, char *dest, int base);

/**
 * Convert unsigned integer to string (decimal). Returns pointer to dest.
 */
char *utoa(uint64_t value, char *dest, int base);

/**
 * Minimal snprintf supporting %s, %d, %u, %x, %%.
 */
int ksnprintf(char *buf, size_t size, const char *fmt, ...);

#endif // STRING_H
