# Continuous pointing and temporary key stalls

Reported on both halves running input-motion rc.1, with wireless host output
and trackball use at the time of the key stall.

The pinned HOG implementation drains each report-type FIFO until empty on one
shared work queue. Notification allocation can wait for TX buffers on this
dedicated queue. A continuously replenished mouse FIFO can therefore keep the
mouse callback running while keyboard work waits behind it.

The test extracts the actual pinned callback functions. A mocked notification
refills the mouse queue for 100 reports. The original callback drains all 100
before returning. The patched callback returns after one report and resubmits
itself if more are queued, allowing already queued keyboard work to run.
FIFO order within each report type is preserved. Empty queues, disconnected
profiles and the security-upgrade path are also exercised.

This is a host-side reproduction of a starvation path, not a measured device
stall or proof that the user's symptoms have this single cause. Notification
allocation can still block one callback. Host link throughput, input queue
pressure, sensor rest modes and interrupted software SPI remain candidates
for varying pointer response.

The trial patch is Ananta-local and tied to the exact HOG source hash. It applies
the same one-report budget to keyboard, consumer and mouse callbacks.
Existing rc.1 sensor and rendering changes are retained for this comparison.
Hardware validation and any rc.2 publication are pending.
