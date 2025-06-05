#ifndef _UTIL_H_
#define _UTIL_H_

#ifdef __cplusplus
    extern "C" {
#endif

#include <inttypes.h>

#define my_min(a, b) (((a) < (b)) ? (a) : (b))

uint32_t os_get_tick (void);
void os_delay (uint32_t ms);

uint16_t get_u16(void *p);
uint32_t get_u32(void *p);
void put_u16(void *p, uint16_t val);
void put_u32(void *p, uint32_t val);
void put_u64(void *p, uint64_t val);
uint32_t make_crc32(uint32_t crc, const void *buf, uint32_t len);

#if 0
#ifdef _WIN32
#include <windows.h>
#endif

#define is_print(c)     ((c) >= ' ' && (c) <= '~')
#define is_digit(c)     ((c) >= '0' && (c) <= '9')
#define is_xdigit(c)    (is_digit(c) || (((c) >= 'a') && ((c) <= 'f')) || (((c) >= 'A') && ((c) <= 'F')))
#define x_to_bin(x)  (is_digit(x) ? (x - '0') : (x - ('A' - 10)))

#define to_lower(c)     ((c) | 0x20)

int64_t get_tick(void);      // unit : 100ns
void os_sleep (uint32_t ms);
int os_100nanosleep (uint64_t ns);
int os_usleep (uint64_t us);
uint8_t xtoi (uint8_t c);
int win_kbhit(void);
int win_getch (void);
int str_ncmp (const void *s1, const void *s2);



#ifdef _WIN32
typedef HANDLE os_sem_t;
#endif

/*
    sem = os_sem_create (1, 1);
    os_sem_wait (sem, 1000 * 60 * 60);
    os_sem_post (sem);
*/
os_sem_t os_sem_create (int start_count, int max_count);
int os_sem_destroy (os_sem_t sem);
int os_sem_wait (os_sem_t sem, uint32_t timeout);
int os_sem_post (os_sem_t sem);

uint16_t make_char_crc16(uint16_t crc, uint8_t c);
uint16_t make_crc16(uint16_t crc, const void *buffer, uint32_t len);
uint32_t make_char_crc32 (uint32_t crc, uint8_t data);


uint16_t get_u16(void *p);
uint32_t get_u32(void *p);
void put_u16(void *p, uint16_t val);
void put_u32(void *p, uint32_t val);
void put_u64(void *p, uint64_t val);
float get_float (void *p);
float get_float_swap (void *p);

typedef void (*task_func_t)(void *);
void *os_create_task (task_func_t func, int priority, int stack_size);
#endif


#ifdef __cplusplus
    }
#endif

#endif
