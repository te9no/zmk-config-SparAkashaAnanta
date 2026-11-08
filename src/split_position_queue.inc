/* Zephyr system-workqueue ATT allocation is nonblocking. */
K_MSGQ_DEFINE(position_state_msgq, sizeof(char[POS_STATE_LEN]),
              CONFIG_ZMK_SPLIT_BLE_PERIPHERAL_POSITION_QUEUE_SIZE, 4);
static struct k_spinlock saa_position_lock;
static bool saa_position_enabled, saa_position_pending, saa_position_overflow;
/* Generation prevents an old in-flight attempt from consuming a new
 * subscription's state. Intermediate taps can coalesce only on overflow. */
static uint32_t saa_position_generation, saa_position_overflow_count;
static uint8_t saa_position_head[POS_STATE_LEN], saa_position_latest[POS_STATE_LEN];
static void send_position_state_callback(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(service_position_notify_work, send_position_state_callback);
int send_position_state(void) {
    k_spinlock_key_t key=k_spin_lock(&saa_position_lock);
    if(!saa_position_enabled) {
        k_spin_unlock(&saa_position_lock,key);return 0;
    }
    if(saa_position_overflow ||
       k_msgq_put(&position_state_msgq,position_state,K_NO_WAIT)!=0) {
        memcpy(saa_position_latest,position_state,POS_STATE_LEN);
        if(!saa_position_overflow) saa_position_overflow_count++;
        saa_position_overflow=true;
    }
    k_spin_unlock(&saa_position_lock,key);
    k_work_schedule(&service_position_notify_work,K_NO_WAIT);return 0;
}
static void saa_position_subscription(bool enabled) {
    k_spinlock_key_t key=k_spin_lock(&saa_position_lock);
    saa_position_generation++;
    saa_position_enabled=enabled;
    saa_position_pending=saa_position_overflow=false;
    k_msgq_purge(&position_state_msgq);
    if(enabled) k_msgq_put(&position_state_msgq,position_state,K_NO_WAIT);
    k_spin_unlock(&saa_position_lock,key);
    if(enabled) k_work_schedule(&service_position_notify_work,K_NO_WAIT);
}
static void send_position_state_callback(struct k_work *work) {
    ARG_UNUSED(work);
    uint8_t state[POS_STATE_LEN];
    k_spinlock_key_t key=k_spin_lock(&saa_position_lock);
    if(!saa_position_enabled) {
        k_spin_unlock(&saa_position_lock,key);return;
    }
    if(!saa_position_pending) {
        if(k_msgq_get(&position_state_msgq,saa_position_head,K_NO_WAIT)==0) {
            saa_position_pending=true;
        } else if(saa_position_overflow) {
            memcpy(saa_position_head,saa_position_latest,POS_STATE_LEN);
            saa_position_pending=true;saa_position_overflow=false;
        }
    }
    if(!saa_position_pending) {
        k_spin_unlock(&saa_position_lock,key);return;
    }
    uint32_t generation=saa_position_generation;
    memcpy(state,saa_position_head,POS_STATE_LEN);
    k_spin_unlock(&saa_position_lock,key);
    int err=bt_gatt_notify(NULL,&split_svc.attrs[1],state,sizeof(state));
    key=k_spin_lock(&saa_position_lock);
    if(generation==saa_position_generation && !err) saa_position_pending=false;
    bool again=saa_position_enabled &&
        (saa_position_pending || saa_position_overflow || k_msgq_num_used_get(&position_state_msgq));
    k_spin_unlock(&saa_position_lock,key);
    if(again) k_work_reschedule(&service_position_notify_work,K_MSEC(err?8:1));
}
