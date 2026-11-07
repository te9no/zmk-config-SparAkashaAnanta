#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <errno.h>
#define BIT(n) (1U << (n))
#define unlikely(x) (x)
#define LOG_WRN(...) ((void)0)
#define LOG_DBG(...) ((void)0)
#define K_FOREVER 0
#define K_NO_WAIT 0
#define K_MSEC(x) (x)
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define CLAMP(v,l,h) ((v)<(l)?(l):((v)>(h)?(h):(v)))
#define CONTAINER_OF(ptr,type,member) ((type *)((char *)(ptr)-offsetof(type,member)))
#define PMW3610_BURST_SIZE 7
#define PMW3610_REG_MOTION_BURST 0x12
#define PMW3610_REG_MOTION 2
#define PMW3610_X_L_POS 1
#define PMW3610_Y_L_POS 2
#define PMW3610_XY_H_POS 3
#define PMW3610_SHUTTER_H_POS 5
#define PMW3610_SHUTTER_L_POS 6
struct k_work { int dummy; };
struct k_work_delayable { struct k_work work; };
struct device { void *data; const void *config; };
struct pixart_runtime_config {
    uint32_t report_interval_min_ms;
    bool swap_xy, invert_x, invert_y, smart_algorithm;
};
struct pixart_config { bool disable_burst_read; int evt_type, x_input_code, y_input_code; };
struct pixart_data {
    bool ready, capture_active, sw_smart_flag;
    int lock;
    struct pixart_runtime_config runtime;
    int32_t dx,dy;
    int64_t last_smp_time,last_rpt_time;
    const struct device *dev;
    struct k_work_delayable trigger_work;
};
static int saa_pmw_queue, sent_x, sent_y, read_error;
static int64_t now_ms, due;
static bool irq_enabled;
static uint8_t sample[7];
static int64_t k_uptime_get(void) { return now_ms; }
static void k_mutex_lock(int *lock,int timeout) { (void)lock; (void)timeout; }
static void k_mutex_unlock(int *lock) { (void)lock; }
static struct k_work_delayable *k_work_delayable_from_work(struct k_work *work) {
    return CONTAINER_OF(work,struct k_work_delayable,work);
}
static int k_work_reschedule_for_queue(int *queue,struct k_work_delayable *work,int64_t delay) {
    (void)queue; (void)work; due=now_ms+delay; return 0;
}
static int pmw3610_read(const struct device *dev,int reg,uint8_t *buf,size_t size) {
    (void)dev; (void)reg;
    if(read_error) return read_error;
    memcpy(buf,sample,size); return 0;
}
static int pmw3610_write(const struct device *dev,int reg,int value) {
    (void)dev; (void)reg; (void)value; return 0;
}
static int pmw3610_set_interrupt(const struct device *dev,bool enabled) {
    (void)dev; irq_enabled=enabled; return 0;
}
static int input_report(const struct device *dev,int type,int code,int value,bool sync,int timeout) {
    (void)dev; (void)type; (void)sync; (void)timeout;
    if(code==0) sent_x+=value; else sent_y+=value;
    return 0;
}
/* PRODUCTION_FUNCTIONS */
static void motion(int x,int y,bool mot) {
    memset(sample,0,sizeof(sample));
    sample[0]=mot?128:0;
    sample[1]=x&255; sample[2]=y&255;
    sample[3]=((x>>4)&0xf0)|((y>>8)&15);
}
int main(void) {
    struct pixart_config cfg={.y_input_code=1};
    struct pixart_data data={.ready=true,.runtime={.report_interval_min_ms=14},
                              .last_rpt_time=100};
    struct device dev={.data=&data,.config=&cfg}; data.dev=&dev;
    now_ms=105; motion(1,-2,true);
    pmw3610_work_callback(&data.trigger_work.work);
    assert(sent_x==0 && sent_y==0 && data.dx==1 && due==114 && irq_enabled);
    now_ms=107; motion(1,-1,true);
    pmw3610_work_callback(&data.trigger_work.work);
    assert(due==114 && data.dx==2 && irq_enabled);
    // No further physical motion. Timer must flush once, without a new IRQ.
    now_ms=due; due=0; motion(500,500,false);
    pmw3610_work_callback(&data.trigger_work.work);
    assert(sent_x==2 && sent_y==-3 && data.dx==0 && due==0 && irq_enabled);
    // New measured motion before the deadline joins the pending report.
    now_ms=115; motion(-3,4,true); irq_enabled=false;
    pmw3610_work_callback(&data.trigger_work.work);
    assert(due==128 && data.dx==-3);
    now_ms=128; due=0; motion(2,-1,true);
    pmw3610_work_callback(&data.trigger_work.work);
    assert(sent_x==1 && sent_y==0 && due==0);
    // Invalid bus response and read errors terminate retries.
    now_ms=129; motion(2,2,true); pmw3610_work_callback(&data.trigger_work.work);
    now_ms=due; due=0; memset(sample,255,sizeof(sample));
    pmw3610_work_callback(&data.trigger_work.work);
    assert(data.dx==0 && due==0 && sent_x==1);
    now_ms=143; motion(2,2,true); pmw3610_work_callback(&data.trigger_work.work);
    now_ms=144; motion(1,1,true); pmw3610_work_callback(&data.trigger_work.work);
    now_ms=due; due=0; read_error=-EIO;
    pmw3610_work_callback(&data.trigger_work.work);
    assert(data.dx==0 && data.dy==0 && due==0);
    read_error=0;
    // Capture owns the IRQ. The timer must neither poll nor enable it.
    data.capture_active=true; irq_enabled=false; data.dx=3;
    pmw3610_work_callback(&data.trigger_work.work);
    assert(!irq_enabled && due==0 && data.dx==0 && data.dy==0);
    data.capture_active=false; data.dx=0;
    // Disabled rate limiter reports immediately.
    data.runtime.report_interval_min_ms=0; motion(-1,-1,true);
    int before=sent_x; pmw3610_work_callback(&data.trigger_work.work);
    assert(sent_x==before-1 && due==0);
    return 0;
}
