#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <errno.h>
#include <string.h>
/* Exercise production queue reconciliation using the real Zephyr kernel. */
struct event { int key; bool pressed; };
#define SAA_STATE_KEYS 256
static int saa_event_key(const void *p) { return ((const struct event *)p)->key; }
static bool saa_event_pressed(const void *p) { return ((const struct event *)p)->pressed; }
static void saa_make_event(void *p, int key, bool pressed) {
    *(struct event *)p=(struct event){key,pressed};
}
#include "../../src/key_state_queue.inc"
K_MSGQ_DEFINE(events,sizeof(struct event),4,4);
static bool observed[SAA_STATE_KEYS];
static void put(int key,bool pressed) {
    struct event ev={key,pressed};
    __ASSERT_NO_MSG(saa_state_put(&events,&ev,K_NO_WAIT)==0);
}
static struct event take(void) {
    struct event ev;
    __ASSERT_NO_MSG(saa_state_get(&events,&ev,K_NO_WAIT)==0);
    if(ev.key>=0) observed[ev.key]=ev.pressed;
    return ev;
}
static void drain(void) {
    struct event ev;
    int count=0;
    while(saa_state_get(&events,&ev,K_NO_WAIT)==0) {
        __ASSERT_NO_MSG(++count<300);
        if(ev.key>=0) observed[ev.key]=ev.pressed;
    }
}
#define POS_STATE_LEN 16
#define CONFIG_ZMK_SPLIT_BLE_PERIPHERAL_POSITION_QUEUE_SIZE 10
static uint8_t position_state[POS_STATE_LEN],sent[100][POS_STATE_LEN];
static struct { int attrs[2]; } split_svc;
static int transport_error, attempts, sent_count;
static int bt_gatt_notify(void *conn,const int *attr,const void *data,size_t len) {
    __ASSERT_NO_MSG(k_current_get()==k_work_queue_thread_get(&k_sys_work_q));
    attempts++;
    if(transport_error) return transport_error;
    __ASSERT_NO_MSG(len==POS_STATE_LEN && sent_count<100);
    memcpy(sent[sent_count++],data,len);return 0;
}
#include "../../src/split_position_queue.inc"
static void publish(uint8_t state) {
    k_spinlock_key_t key=k_spin_lock(&saa_position_lock);
    position_state[0]=state;
    k_spin_unlock(&saa_position_lock,key);
    int64_t start=k_uptime_get();
    __ASSERT_NO_MSG(send_position_state()==0);
    __ASSERT_NO_MSG(k_uptime_get()-start<2);
}
int main(void) {
    /* Normal ordering is unchanged, including a quick press/release. */
    put(0,true);put(0,false);put(1,true);put(1,false);
    for(int i=0;i<4;i++) {
        struct event e=take();
        __ASSERT_NO_MSG(e.key==i/2 && e.pressed==(i%2==0));
    }
    /* This fifth release is silently lost by the original four-entry queue. */
    put(0,true);put(1,true);put(0,false);put(2,true);put(1,false);
    drain();
    __ASSERT_NO_MSG(!observed[0] && !observed[1] && observed[2]);
    put(2,false);drain();
    printk("SCAN_OVERFLOW: release repaired without another key transition\n");
    /* Multiple peripherals, direction changes and disconnect releases. */
    for(int i=0;i<100;i++) {put(i%5,true);put(128+i%5,true);}
    for(int i=0;i<5;i++) {put(i,false);put(128+i,false);}
    drain();
    for(int i=0;i<256;i++) __ASSERT_NO_MSG(!observed[i]);
    /* Non-key traffic cannot starve reconciliation after the queued prefix. */
    put(3,true);drain();
    for(int i=0;i<4;i++)put(-1,false);
    put(3,false);put(4,true);
    for(int i=0;i<4;i++){__ASSERT_NO_MSG(take().key==-1);put(-1,false);}
    struct event e=take();__ASSERT_NO_MSG(e.key==3 && !e.pressed);
    e=take();__ASSERT_NO_MSG(e.key==4 && e.pressed);
    drain();put(4,false);drain();
    struct event invalid={256,true};
    __ASSERT_NO_MSG(saa_state_put(&events,&invalid,K_NO_WAIT)==-EINVAL);
    invalid.key=-2;
    __ASSERT_NO_MSG(saa_state_put(&events,&invalid,K_NO_WAIT)==-EINVAL);
    printk("CENTRAL: sources isolated, non-key flood bounded, invalid indices rejected\n");
    /* Real scheduled worker: preserve a failed head while producers fill queue. */
    transport_error=-ENOMEM;position_state[0]=1;saa_position_subscription(true);
    k_msleep(20);
    for(int i=0;i<100;i++)publish(i%2?0:2);
    publish(0);k_msleep(30);
    __ASSERT_NO_MSG(sent_count==0 && attempts>0 && attempts<30);
    transport_error=0;k_msleep(200);
    __ASSERT_NO_MSG(sent_count>1 && sent[0][0]==1 && sent[sent_count-1][0]==0);
    __ASSERT_NO_MSG(!saa_position_pending && !saa_position_overflow);
    printk("PERIPHERAL: failed head retained, final release delivered, producer nonblocking\n");
    int before=sent_count;
    publish(4);publish(0);k_msleep(40);
    __ASSERT_NO_MSG(sent_count==before+2 && sent[before][0]==4 && sent[before+1][0]==0);
    transport_error=-ENOMEM;publish(8);k_msleep(20);
    saa_position_subscription(false);publish(0);k_msleep(20);
    before=sent_count;int tries=attempts;k_msleep(20);
    __ASSERT_NO_MSG(attempts==tries);
    transport_error=0;saa_position_subscription(true);k_msleep(40);
    __ASSERT_NO_MSG(sent_count==before+1 && sent[before][0]==0);
    printk("SUBSCRIPTION: disabled retry stops, reconnect sends current state only\n");
    printk("ALL_TESTS_PASS\n");return 0;
}
