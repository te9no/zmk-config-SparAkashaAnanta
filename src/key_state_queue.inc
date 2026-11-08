/* FIFO until overflow; then reconcile latest state after the queued prefix. */
struct saa_state_queue {
    struct k_spinlock lock;
    bool desired[SAA_STATE_KEYS], delivered[SAA_STATE_KEYS], recovering;
    unsigned prefix;
    uint32_t overflows;
};
/* All access to this queue must go through these wrappers. Keep the outer
 * lock across msgq operations so overflow cannot race the consumer. Calls are
 * always nonblocking. The queued prefix ends before reconciliation; non-key
 * events appended later must not starve repair of a lost key release. */
static struct saa_state_queue saa_states;
static int saa_state_put(struct k_msgq *queue, const void *event, k_timeout_t timeout) {
    ARG_UNUSED(timeout);
    int key=saa_event_key(event);
    if(key < -1 || key >= SAA_STATE_KEYS) return -EINVAL;
    k_spinlock_key_t lock=k_spin_lock(&saa_states.lock);
    if(key>=0) {
        saa_states.desired[key]=saa_event_pressed(event);
        if(saa_states.recovering) {
            k_spin_unlock(&saa_states.lock,lock);return 0;
        }
    }
    int err=k_msgq_put(queue,event,K_NO_WAIT);
    if(err && key>=0) {
        saa_states.recovering=true;
        saa_states.prefix=k_msgq_num_used_get(queue);
        saa_states.overflows++;err=0;
    }
    k_spin_unlock(&saa_states.lock,lock);return err;
}
static int saa_state_get(struct k_msgq *queue, void *event, k_timeout_t timeout) {
    ARG_UNUSED(timeout);
    k_spinlock_key_t lock=k_spin_lock(&saa_states.lock);
    if(saa_states.recovering && saa_states.prefix==0) {
        for(int pressed=0;pressed<2;pressed++) {
            for(int key=0;key<SAA_STATE_KEYS;key++) {
                if(saa_states.desired[key]!=saa_states.delivered[key] &&
                   saa_states.desired[key]==(bool)pressed) {
                    saa_make_event(event,key,pressed);
                    saa_states.delivered[key]=pressed;
                    k_spin_unlock(&saa_states.lock,lock);return 0;
                }
            }
        }
        saa_states.recovering=false;
    }
    int err=k_msgq_get(queue,event,K_NO_WAIT);
    if(!err) {
        int key=saa_event_key(event);
        if(key>=0 && key<SAA_STATE_KEYS) saa_states.delivered[key]=saa_event_pressed(event);
        if(saa_states.recovering && saa_states.prefix) saa_states.prefix--;
    }
    k_spin_unlock(&saa_states.lock,lock);return err;
}
