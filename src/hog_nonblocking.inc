/*
 * Ananta bounded HID queues. Included after the pinned HOG service definition.
 * Zephyr's system workqueue notification path allocates ATT buffers K_NO_WAIT.
 * No producer waits for transport or queue space.
 */
#include <limits.h>
#include <string.h>

union saa_report {
    struct zmk_hid_keyboard_report_body keyboard;
    struct zmk_hid_consumer_report_body consumer;
#if IS_ENABLED(CONFIG_ZMK_POINTING)
    struct zmk_hid_mouse_report_body mouse;
#endif
};
struct saa_frame {
    union saa_report report;
    int profile;
    /* Relative movement is retained beyond a single HID report's range. */
    int64_t delta[4];
};
struct saa_queue {
    struct saa_frame *frames;
    unsigned capacity, head, count;
    struct saa_frame pending;
    bool has_pending;
    uint32_t overflow;
};
static struct k_spinlock saa_lock;
static struct saa_frame saa_keys[CONFIG_ZMK_BLE_KEYBOARD_REPORT_QUEUE_SIZE];
static struct saa_frame saa_consumers[CONFIG_ZMK_BLE_CONSUMER_REPORT_QUEUE_SIZE];
static struct saa_queue saa_q[] = {
    {.frames=saa_keys, .capacity=ARRAY_SIZE(saa_keys)},
    {.frames=saa_consumers, .capacity=ARRAY_SIZE(saa_consumers)},
#if IS_ENABLED(CONFIG_ZMK_POINTING)
#define SAA_MOUSE 2
#endif
};
#if IS_ENABLED(CONFIG_ZMK_POINTING)
static struct saa_frame saa_mice[CONFIG_ZMK_BLE_MOUSE_REPORT_QUEUE_SIZE];
static struct saa_queue saa_mouse = {.frames=saa_mice, .capacity=ARRAY_SIZE(saa_mice)};
#endif
static void saa_send(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(saa_send_work, saa_send);

static void saa_enqueue(struct saa_queue *q, const struct saa_frame *frame, bool mouse) {
    k_spinlock_key_t lock = k_spin_lock(&saa_lock);
    if (q->count) {
        unsigned tail = (q->head + q->count - 1) % q->capacity;
        struct saa_frame *last = &q->frames[tail];
#if IS_ENABLED(CONFIG_ZMK_POINTING)
        if (mouse && last->profile == frame->profile &&
            last->report.mouse.buttons == frame->report.mouse.buttons) {
            /* Merge only adjacent movement with identical buttons/profile.
             * A press/release boundary always gets a separate queue entry. */
            for (int i=0; i<4; i++) {
                /* Explicit saturation: finite RAM cannot retain infinite input. */
                int64_t d=frame->delta[i];
                if ((d>0 && last->delta[i]>INT64_MAX-d) ||
                    (d<0 && last->delta[i]<INT64_MIN-d)) {
                    last->delta[i] = d>0 ? INT64_MAX : INT64_MIN;
                    q->overflow++;
                } else {
                    last->delta[i]+=d;
                }
            }
            k_spin_unlock(&saa_lock,lock);
            k_work_schedule(&saa_send_work,K_NO_WAIT);
            return;
        }
#endif
        if (q->count == q->capacity) {
            /* Preserve oldest FIFO entries plus the newest absolute state.
             * At sustained overload intermediate taps/clicks can be lost, but
             * a final release must replace an older tail, never be rejected.
             * Mouse movement cannot cross a button boundary safely: record
             * this exceptional loss rather than replay it as a wrong drag. */
            *last=*frame;
            q->overflow++;
            k_spin_unlock(&saa_lock,lock);
            k_work_schedule(&saa_send_work,K_NO_WAIT);
            return;
        }
    }
    q->frames[(q->head+q->count)%q->capacity]=*frame;
    q->count++;
    k_spin_unlock(&saa_lock,lock);
    k_work_schedule(&saa_send_work,K_NO_WAIT);
}
static int16_t saa_chunk(int64_t value) {
    return value>INT16_MAX ? INT16_MAX : value<INT16_MIN ? INT16_MIN : value;
}
/* A single worker owns pending; producers only touch the locked ring. */
static bool saa_one(struct saa_queue *q, int kind, struct bt_conn *conn, int profile,
                    bool *progress) {
    k_spinlock_key_t lock=k_spin_lock(&saa_lock);
    if (!q->has_pending && q->count) {
        q->pending=q->frames[q->head];
        q->head=(q->head+1)%q->capacity;
        q->count--;
        q->has_pending=true;
    }
    k_spin_unlock(&saa_lock,lock);
    if (!q->has_pending) return false;
    if (q->pending.profile != profile) {
        q->has_pending=false;
        *progress=true;
        return true;
    }
    union saa_report report=q->pending.report;
    const void *data=&report.keyboard;
    size_t len=sizeof(report.keyboard);
    unsigned attr=5;
#if IS_ENABLED(CONFIG_ZMK_POINTING)
    int16_t delta[4]={0};
    if (kind==SAA_MOUSE) {
        for(int i=0;i<4;i++) delta[i]=saa_chunk(q->pending.delta[i]);
        report.mouse.d_x=delta[0];report.mouse.d_y=delta[1];
        report.mouse.d_scroll_y=delta[2];report.mouse.d_scroll_x=delta[3];
        data=&report.mouse;len=sizeof(report.mouse);attr=13;
    } else
#endif
    if (kind==1) { data=&report.consumer;len=sizeof(report.consumer);attr=9; }
    struct bt_gatt_notify_params params={.attr=&hog_svc.attrs[attr],.data=data,.len=len};
    int err=bt_gatt_notify_cb(conn,&params);
    if (err==-EPERM) bt_conn_set_security(conn,BT_SECURITY_L2);
    if (!err) {
        q->has_pending=false;
#if IS_ENABLED(CONFIG_ZMK_POINTING)
        if(kind==SAA_MOUSE) {
            for(int i=0;i<4;i++) {
                q->pending.delta[i]-=delta[i];
                if(q->pending.delta[i]) q->has_pending=true;
            }
        }
#endif
        *progress=true;
    }
    /* On allocation/security/subscription failure retain the exact report.
     * Retry is delayed; other report kinds still get their turn. */
    return true;
}
static void saa_clear(struct saa_queue *q) {
    k_spinlock_key_t lock=k_spin_lock(&saa_lock);
    q->head=q->count=0;
    q->has_pending=false;
    k_spin_unlock(&saa_lock,lock);
}
static void saa_send(struct k_work *work) {
    ARG_UNUSED(work);
    struct bt_conn *conn=zmk_ble_active_profile_conn();
    if (!conn) {
        saa_clear(&saa_q[0]);saa_clear(&saa_q[1]);
#if IS_ENABLED(CONFIG_ZMK_POINTING)
        saa_clear(&saa_mouse);
#endif
        return;
    }
    int profile=zmk_ble_profile_index(bt_conn_get_dst(conn));
    bool progress=false,more=false;
    /* Keyboard always gets the first opportunity; one attempt per kind. */
    more |= saa_one(&saa_q[0],0,conn,profile,&progress);
    more |= saa_one(&saa_q[1],1,conn,profile,&progress);
#if IS_ENABLED(CONFIG_ZMK_POINTING)
    more |= saa_one(&saa_mouse,SAA_MOUSE,conn,profile,&progress);
#endif
    bt_conn_unref(conn);
    if(more) k_work_reschedule(&saa_send_work,K_MSEC(progress?1:8));
}
int zmk_hog_send_keyboard_report(struct zmk_hid_keyboard_report_body *report) {
    struct saa_frame frame={.report.keyboard=*report,.profile=zmk_ble_active_profile_index()};
    saa_enqueue(&saa_q[0],&frame,false);return 0;
}
int zmk_hog_send_consumer_report(struct zmk_hid_consumer_report_body *report) {
    struct saa_frame frame={.report.consumer=*report,.profile=zmk_ble_active_profile_index()};
    saa_enqueue(&saa_q[1],&frame,false);return 0;
}
#if IS_ENABLED(CONFIG_ZMK_POINTING)
int zmk_hog_send_mouse_report(struct zmk_hid_mouse_report_body *report) {
    struct saa_frame frame={.report.mouse=*report,.profile=zmk_ble_active_profile_index(),
        .delta={report->d_x,report->d_y,report->d_scroll_y,report->d_scroll_x}};
    saa_enqueue(&saa_mouse,&frame,true);return 0;
}
#endif
