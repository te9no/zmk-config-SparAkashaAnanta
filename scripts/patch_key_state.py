"""Generate keyboard-local, revision-checked key state recovery paths."""
import hashlib
from pathlib import Path
import sys
sys.dont_write_bytecode = True
from patch_scan_startup import MAIN_SHA, physical as patch_physical_startup, main as patch_main_startup
ROOT=Path(__file__).resolve().parents[1]
HASH={"main.c": MAIN_SHA,
"physical_layouts.c":"eccce3df9ca6cba0cb70e85dfafe70baa3535f0d66a418fdbe59fac4d9ae8ed2",
"central.c":"f53e47e276e98aeb2757c4633bbd201a48462274b0a6c777c6eb8589b15f23c0",
"service.c":"434844179c06edf11cc96d87675f7fada7e1f211cacfb226c69438350ad749b7"}
def patch(source,name):
    if hashlib.sha256(source.encode()).hexdigest()!=HASH[name]:
        raise ValueError("Key state source changed: "+name)
    if name=="main.c":
        return patch_main_startup(source)
    if name=="physical_layouts.c":
        source=patch_physical_startup(source)
    if name=="service.c":
        start=source.index("K_MSGQ_DEFINE(position_state_msgq,")
        end=source.index("static int zmk_split_bt_position_pressed(",start)
        source=source[:start]+(ROOT/"src/split_position_queue.inc").read_text()+"\n"+source[end:]
        source=source.replace("static void split_svc_pos_state_ccc(", "static void saa_position_subscription(bool enabled);\n\nstatic void split_svc_pos_state_ccc(")
        old='    LOG_DBG("value %d", value);\n}\n\n#if IS_ENABLED(CONFIG_ZMK_SPLIT_RELAY_EVENT)'
        assert source.count(old)==1
        source=source.replace(old,'    LOG_DBG("value %d", value);\n    saa_position_subscription(value == BT_GATT_CCC_NOTIFY);\n}\n\n#if IS_ENABLED(CONFIG_ZMK_SPLIT_RELAY_EVENT)')
        for state in ("true","false"):
            old=f"    WRITE_BIT(position_state[position / 8], position % 8, {state});"
            new=f"""    if (position >= POS_STATE_LEN * 8) return -EINVAL;
    k_spinlock_key_t key=k_spin_lock(&saa_position_lock);
{old}
    k_spin_unlock(&saa_position_lock,key);"""
            assert source.count(old)==1
            source=source.replace(old,new)
        return source
    if name=="physical_layouts.c":
        queue="physical_layouts_kscan_msgq"
        marker="#if MATRIX_INPUT_SUPPORT\n\nstatic struct zmk_kscan_event pending_input_event;"
        adapter="""
#define SAA_STATE_KEYS 84 /* Ananta fixed matrix: rows 0..6, columns 0..11. */
static int saa_event_key(const void *ptr) {
    const struct zmk_kscan_event *ev=ptr;
    if(ev->row>=7 || ev->column>=12) return -EINVAL;
    return ev->row*12+ev->column;
}
static bool saa_event_pressed(const void *ptr) {
    return ((const struct zmk_kscan_event *)ptr)->state==ZMK_KSCAN_EVENT_STATE_PRESSED;
}
static void saa_make_event(void *ptr,int key,bool pressed) {
    *(struct zmk_kscan_event *)ptr=(struct zmk_kscan_event){
        .row=key/12,.column=key%12,
        .state=pressed?ZMK_KSCAN_EVENT_STATE_PRESSED:ZMK_KSCAN_EVENT_STATE_RELEASED};
}
"""
    else:
        queue="peripheral_event_msgq"
        marker="void peripheral_event_work_callback(struct k_work *work);"
        adapter="""
#define SAA_STATE_KEYS (ZMK_SPLIT_BLE_PERIPHERAL_COUNT * POSITION_STATE_DATA_LEN * 8)
static int saa_event_key(const void *ptr) {
    const struct peripheral_event_wrapper *ev=ptr;
    if(ev->event.type!=ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_KEY_POSITION_EVENT) return -1;
    unsigned position=ev->event.data.key_position_event.position;
    if(ev->source>=ZMK_SPLIT_BLE_PERIPHERAL_COUNT || position>=POSITION_STATE_DATA_LEN*8) return -EINVAL;
    return ev->source*POSITION_STATE_DATA_LEN*8+position;
}
static bool saa_event_pressed(const void *ptr) {
    return ((const struct peripheral_event_wrapper *)ptr)->event.data.key_position_event.pressed;
}
static void saa_make_event(void *ptr,int key,bool pressed) {
    *(struct peripheral_event_wrapper *)ptr=(struct peripheral_event_wrapper){
        .source=key/(POSITION_STATE_DATA_LEN*8),
        .event={.type=ZMK_SPLIT_TRANSPORT_PERIPHERAL_EVENT_TYPE_KEY_POSITION_EVENT,
            .data={.key_position_event={.position=key%(POSITION_STATE_DATA_LEN*8),.pressed=pressed}}}};
}
"""
    assert source.count(marker)==1
    source=source.replace(f"k_msgq_put(&{queue},",f"saa_state_put(&{queue},")
    source=source.replace(f"k_msgq_get(&{queue},",f"saa_state_get(&{queue},")
    return source.replace(marker,adapter+(ROOT/"src/key_state_queue.inc").read_text()+"\n"+marker)
if __name__=="__main__":
    path=Path(sys.argv[1])
    Path(sys.argv[2]).write_text(patch(path.read_text(),path.name))
