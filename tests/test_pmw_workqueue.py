import unittest
from test_pmw_guard import guard


class MotionQueue(unittest.TestCase):
    def test_motion_only_and_start_before_irq(self):
        source = '''static void pmw3610_gpio_callback(void) {
    k_work_submit(&data->trigger_work);
}
static void init(void) {
    k_work_init(&data->trigger_work, pmw3610_work_callback);
    k_work_init_delayable(&data->init_work, pmw3610_async_init);
}
static void work(void) {
    pmw3610_report_data(dev);
    pmw3610_set_interrupt(dev, true);
}
'''
        result = guard.isolate_motion_work(source)
        self.assertNotIn('k_work_submit(&data->trigger_work)', result)
        self.assertIn('k_work_submit_to_queue(&saa_pmw_queue, &data->trigger_work)', result)
        self.assertLess(result.index('k_work_queue_start('), result.index('k_work_init(&data->trigger_work'))
        self.assertIn('k_work_init_delayable(&data->init_work, pmw3610_async_init)', result)
        self.assertIn('k_mutex_lock(&data->lock, K_FOREVER);', result)
        self.assertIn('if (!data->capture_active)', result)

    def test_missing_or_duplicate_hooks_fail_closed(self):
        for source in ('', '    k_work_submit(&data->trigger_work);' * 2):
            with self.assertRaises(ValueError):
                guard.isolate_motion_work(source)
