#include <assert.h>
#include <stddef.h>
#include <errno.h>
#include <stdio.h>
#define K_NO_WAIT 0
#define BT_SECURITY_L2 2
#define LOG_DBG(...) ((void)0)
struct k_work { int dummy; };
struct report { int value; };
#define zmk_hid_keyboard_report_body report
#define zmk_hid_consumer_report_body report
#define zmk_hid_mouse_report_body report
struct bt_conn { int dummy; };
struct bt_gatt_notify_params { const int *attr; const void *data; size_t len; };
static struct { int attrs[14]; } hog_svc;
static int zmk_hog_mouse_msgq=1,zmk_hog_keyboard_msgq=1,zmk_hog_consumer_msgq;
static int hog_work_q, mouse_count, keyboard_count, resubmits, notify_error, security;
static int connected=1;
static struct bt_conn conn;
static int k_msgq_get(int *q,struct report *out,int timeout) {
    if(!*q) return -EAGAIN;
    --*q; out->value=1; return 0;
}
static int k_msgq_num_used_get(int *q) { return *q; }
static int k_work_submit_to_queue(int *q,struct k_work *work) { ++resubmits; return 1; }
static struct bt_conn *zmk_ble_active_profile_conn(void) { return connected?&conn:NULL; }
static void bt_conn_unref(struct bt_conn *c) {}
static void bt_conn_set_security(struct bt_conn *c,int level) { security=level; }
static int bt_gatt_notify_cb(struct bt_conn *c,struct bt_gatt_notify_params *params) {
    if(params->attr==&hog_svc.attrs[13]) {
        ++mouse_count;
        // Model input continuing while notification waits for a free TX buffer.
        if(mouse_count<100) ++zmk_hog_mouse_msgq;
    } else if(params->attr==&hog_svc.attrs[5]) {
        ++keyboard_count;
    }
    return notify_error;
}
/* CALLBACKS */
int main(void) {
    (void)hog_work_q;
    struct k_work work={0};
    send_mouse_report_callback(&work);
    assert(mouse_count==BUDGET);
    assert(keyboard_count==0 && zmk_hog_keyboard_msgq==1);
    if(BUDGET==1) assert(resubmits==1 && zmk_hog_mouse_msgq==1);
    // The shared work queue can now run the already queued keyboard work.
    send_keyboard_report_callback(&work);
    assert(keyboard_count==1);
    printf("mouse reports before queued keyboard gets a turn: %d\n",mouse_count);
    zmk_hog_mouse_msgq=0;
    int before=resubmits;
    send_mouse_report_callback(&work);
    assert(resubmits==before);
    connected=0; zmk_hog_mouse_msgq=1;
    send_mouse_report_callback(&work);
    assert(resubmits==before);
    connected=1; notify_error=-EPERM; zmk_hog_keyboard_msgq=1;
    send_keyboard_report_callback(&work);
    assert(security==BT_SECURITY_L2);
    return 0;
}
