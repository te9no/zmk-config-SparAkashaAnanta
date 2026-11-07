#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <stdint.h>
#include <errno.h>
#define CONFIG_ZMK_POINTING 1
#define CONFIG_ZMK_BLE_KEYBOARD_REPORT_QUEUE_SIZE 20
#define CONFIG_ZMK_BLE_CONSUMER_REPORT_QUEUE_SIZE 5
#define CONFIG_ZMK_BLE_MOUSE_REPORT_QUEUE_SIZE 20
#define BT_SECURITY_L2 2
struct zmk_hid_keyboard_report_body { uint8_t keys[8]; };
struct zmk_hid_consumer_report_body { uint8_t keys[2]; };
struct zmk_hid_mouse_report_body { uint8_t buttons; int16_t d_x,d_y,d_scroll_y,d_scroll_x; };
struct bt_conn { int dummy; };
struct bt_gatt_notify_params { const int *attr; const void *data; size_t len; };
static struct { int attrs[14]; } hog_svc;
static struct bt_conn conn;
static bool connected=true;
static int profile, error, tries, key_count, mouse_count;
static int64_t sum_x,sum_y,max_enqueue_ms;
static uint8_t last_key,last_button;
static struct bt_conn *zmk_ble_active_profile_conn(void) {return connected?&conn:NULL;}
static int zmk_ble_active_profile_index(void) {return profile;}
static const int *bt_conn_get_dst(struct bt_conn *c) {return &profile;}
static int zmk_ble_profile_index(const int *p) {return *p;}
static void bt_conn_unref(struct bt_conn *c) {}
static void bt_conn_set_security(struct bt_conn *c,int level) {}
static int bt_gatt_notify_cb(struct bt_conn *c,struct bt_gatt_notify_params *p) {
 /* Production uses this context to choose nonblocking ATT allocation. */
 __ASSERT_NO_MSG(k_current_get()==k_work_queue_thread_get(&k_sys_work_q));
 tries++;
 if(error) return error;
 if(p->attr==&hog_svc.attrs[5]) {
  const struct zmk_hid_keyboard_report_body *r=p->data;
  last_key=r->keys[0];key_count++;
 } else if(p->attr==&hog_svc.attrs[13]) {
  const struct zmk_hid_mouse_report_body *r=p->data;
  sum_x+=r->d_x;sum_y+=r->d_y;last_button=r->buttons;mouse_count++;
 }
 return 0;
}
#include "../../src/hog_nonblocking.inc"
static void enqueue_key(int key) {
 struct zmk_hid_keyboard_report_body r={.keys={key}};
 int64_t t=k_uptime_get();zmk_hog_send_keyboard_report(&r);
 int64_t elapsed=k_uptime_get()-t;
 if(elapsed>max_enqueue_ms)max_enqueue_ms=elapsed;
}
static void settle(void) {k_msleep(250);}
int main(void) {
 /* Hold the modeled transport unavailable while producers continue. */
 error=-ENOMEM;
 for(int i=0;i<100;i++) {
  enqueue_key(i%2?0:4);
  struct zmk_hid_mouse_report_body r={.d_x=1000,.d_y=-1000};
  zmk_hog_send_mouse_report(&r);
  k_msleep(1);
 }
 enqueue_key(0);
 k_msleep(50);
 __ASSERT_NO_MSG(key_count==0 && mouse_count==0 && tries<100);
 __ASSERT_NO_MSG(max_enqueue_ms<2);
 printk("STALL: producer_max_ms=%lld attempts=%d retained_x=100000\n",max_enqueue_ms,tries);
 error=0;settle();
 __ASSERT_NO_MSG(last_key==0 && key_count>0);
 __ASSERT_NO_MSG(sum_x==100000 && sum_y==-100000);
 printk("RECOVERY: key_release=OK dx=%lld dy=%lld overflow=%u\n",sum_x,sum_y,saa_q[0].overflow);
 /* Press and release must remain separate reports. */
 int before=mouse_count;
 struct zmk_hid_mouse_report_body press={.buttons=1,.d_x=17};
 struct zmk_hid_mouse_report_body release={.buttons=0,.d_x=-17};
 zmk_hog_send_mouse_report(&press);zmk_hog_send_mouse_report(&release);
 settle();
 __ASSERT_NO_MSG(mouse_count==before+2 && last_button==0 && sum_x==100000);
 /* Saturated button transitions still converge to the final release.
  * Intermediate clicks are explicitly not lossless under overload. */
 error=-ENOMEM;
 for(int i=0;i<60;i++) {
  struct zmk_hid_mouse_report_body r={.buttons=i%2?0:1};
  zmk_hog_send_mouse_report(&r);
 }
 k_msleep(24);error=0;settle();
 __ASSERT_NO_MSG(last_button==0 && saa_mouse.overflow>0);
 /* Persistent error is retried at a bounded frequency. */
 error=-EPERM;tries=0;enqueue_key(4);k_msleep(160);
 __ASSERT_NO_MSG(tries>=1 && tries<=25);
 printk("SECURITY_RETRY: attempts_160ms=%d\n",tries);
 /* Disconnection clears stale queued input rather than replaying later. */
 connected=false;settle();
 __ASSERT_NO_MSG(!saa_q[0].has_pending && saa_q[0].count==0);
 connected=true;error=0;settle();
 /* Switching profiles must not replay queued data to the new profile. */
 error=-ENOMEM;enqueue_key(5);k_msleep(16);
 profile=1;error=0;before=key_count;settle();
 __ASSERT_NO_MSG(key_count==before);
 enqueue_key(0);settle();__ASSERT_NO_MSG(last_key==0);
 printk("BUTTON_BOUNDARIES_DISCONNECT_PROFILE: PASS\nALL_TESTS_PASS\n");
 return 0;
}
