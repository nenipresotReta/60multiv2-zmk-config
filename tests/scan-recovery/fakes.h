#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#define ZMK_KEYMAP_LEN 66
#define K_NO_WAIT 0
#define CONFIG_PM_DEVICE_RUNTIME 1
#define CONFIG_PM_DEVICE 1
#define IS_ENABLED(x) (x)
#define USE_PHY_LAYOUTS 0
#define DT_HAS_CHOSEN(x) 0
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#define ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL 0
#define LOG_DBG(...) ((void)0)
#define LOG_ERR(...) ((void)0)
#define LOG_WRN(...) fake_warning()

struct device { int id; };
struct zmk_physical_layout { const struct device *kscan; const int *matrix_transform; };
static const struct device scanner_a = {0}, scanner_b = {1};
static const int normal_transform = 0, reversed_transform = 1;
static const struct zmk_physical_layout layout_a = {&scanner_a, &normal_transform};
static const struct zmk_physical_layout layout_b = {&scanner_b, &reversed_transform};
static const struct zmk_physical_layout *layouts[] = {&layout_a, &layout_b};
static const struct zmk_physical_layout *active;
static bool locked, work_pending;
static int warnings, pm_error_once, map_error;
static bool driver_enabled[2];
static void (*warning_hook)(void), (*event_hook)(void), (*disable_hook)(void);
static int64_t now;

static void fake_warning(void) {
    assert(!locked);
    warnings++;
    if (warning_hook) {
        void (*hook)(void) = warning_hook;
        warning_hook = NULL;
        hook();
    }
}
static int32_t zmk_matrix_transform_row_column_to_position(const int *transform,
                                                           uint32_t row, uint32_t col) {
    if (row >= 5 || col >= 15 || row * 15 + col >= ZMK_KEYMAP_LEN) {
        return -EINVAL;
    }
    int pos = row * 15 + col;
    return *transform ? ZMK_KEYMAP_LEN - pos - 1 : pos;
}
static int zmk_physical_layouts_get_position_map(uint8_t source, uint8_t dest, size_t len,
                                                uint32_t *map) {
    if (map_error) { return map_error; }
    if (source >= 2 || dest >= 2) { return -EINVAL; }
    for (size_t i = 0; i < len; i++) { map[i] = dest ? len - i - 1 : i; }
    return len;
}
struct k_spinlock { int unused; };
typedef int k_spinlock_key_t;
static k_spinlock_key_t k_spin_lock(struct k_spinlock *lock) {
    assert(!locked); locked = true; return 0;
}
static void k_spin_unlock(struct k_spinlock *lock, k_spinlock_key_t key) {
    assert(locked); locked = false;
}
struct k_work { int unused; };
static int k_work_submit(struct k_work *work) {
    assert(!locked); work_pending = true; return 1;
}
struct k_msgq { size_t size, count, capacity; unsigned char *data; };
#define K_MSGQ_DEFINE(name, size_, capacity_, align_) \
    static unsigned char name##_data[(size_) * (capacity_)]; \
    static struct k_msgq name = {size_, 0, capacity_, name##_data}
static int k_msgq_put(struct k_msgq *q, const void *p, int wait) {
    assert(locked && wait == K_NO_WAIT);
    if (q->count == q->capacity) { return -ENOMSG; }
    memcpy(q->data + q->size * q->count++, p, q->size); return 0;
}
static int k_msgq_get(struct k_msgq *q, void *p, int wait) {
    assert(locked && wait == K_NO_WAIT);
    if (!q->count) { return -ENOMSG; }
    memcpy(p, q->data, q->size); q->count--;
    memmove(q->data, q->data + q->size, q->size * q->count); return 0;
}
static unsigned int k_msgq_num_used_get(struct k_msgq *q) {
    assert(locked); return q->count;
}
static int64_t k_uptime_get(void) { return now++; }
struct zmk_position_state_changed { uint8_t source; bool state; uint32_t position; int64_t timestamp; };
static int raise_zmk_position_state_changed(struct zmk_position_state_changed event);
static int kscan_disable_callback(const struct device *dev) {
    assert(!locked); driver_enabled[dev->id] = false;
    if (disable_hook) { void (*hook)(void) = disable_hook; disable_hook = NULL; hook(); }
    return 0;
}
static int kscan_enable_callback(const struct device *dev) {
    assert(!locked); driver_enabled[dev->id] = true; return 0;
}
static int kscan_config(const struct device *dev,
                        void (*cb)(const struct device *, uint32_t, uint32_t, bool)) {
    assert(!locked); return 0;
}
static int pm_device_runtime_put(const struct device *dev) { assert(!locked); return 0; }
static int pm_device_runtime_get(const struct device *dev) {
    assert(!locked);
    int result = pm_error_once; pm_error_once = 0; return result;
}
