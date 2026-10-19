static struct zmk_position_state_changed events[1024];
static bool consumer_state[ZMK_KEYMAP_LEN];
static int event_count, layer_depth;

static int raise_zmk_position_state_changed(struct zmk_position_state_changed event) {
    assert(!locked && scan_processing);
    assert(event.position < ZMK_KEYMAP_LEN);
    /* A duplicated keydown could leak behavior/HID reference counts. */
    assert(consumer_state[event.position] != event.state);
    consumer_state[event.position] = event.state;
    if (event.position == 0) { layer_depth += event.state ? 1 : -1; }
    assert(layer_depth >= 0 && layer_depth <= 1);
    assert(event_count < (int)ARRAY_SIZE(events));
    events[event_count++] = event;
    if (event_hook) { void (*hook)(void) = event_hook; event_hook = NULL; hook(); }
    return 0;
}
static void edge(int physical_pos, bool pressed) {
    zmk_physical_layout_kscan_callback(active->kscan, physical_pos / 15, physical_pos % 15,
                                      pressed);
}
static void drain(void) {
    for (int i = 0; work_pending; i++) {
        assert(i < 100); work_pending = false;
        zmk_physical_layouts_kscan_process_msgq(NULL);
    }
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) {
        assert(consumer_state[p] == scan_latest_state[p]);
        assert(consumer_state[p] == scan_reported_state[p]);
    }
    assert(!scan_overflowed && !scan_processing);
}
static void reset(void) {
    assert(!locked);
    physical_layouts_kscan_msgq.count = 0;
    memset(scan_latest_state, 0, sizeof(scan_latest_state));
    memset(scan_reported_state, 0, sizeof(scan_reported_state));
    memset(consumer_state, 0, sizeof(consumer_state));
    scan_overflowed = scan_processing = scan_accepting = work_pending = false;
    warnings = event_count = layer_depth = pm_error_once = map_error = 0;
    warning_hook = event_hook = disable_hook = NULL;
    active = NULL;
    assert(zmk_physical_layouts_select_layout(&layout_a) == 0);
    assert(scan_accepting && active == &layout_a);
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) { assert(selected_to_stock_map[p] == p); }
}
static void flood(int pos) {
    for (int i = 0; i < CONFIG_ZMK_KSCAN_EVENT_QUEUE_SIZE + 2; i++) { edge(pos, i % 2 == 0); }
}
static void press_after_snapshot(void) { edge(0, true); }
static void release_after_snapshot(void) { edge(0, false); }
static void try_switch_in_event(void) {
    assert(zmk_physical_layouts_select_layout(&layout_b) == -EBUSY);
}
static void stale_callback_during_switch(void) {
    zmk_physical_layout_kscan_callback(&scanner_a, 0, 0, true);
}
static bool last_state_for_position(int position, int offset) {
    for (int i = event_count - 1; i >= 0; i--) {
        if (events[i].position == position && offset-- == 0) { return events[i].state; }
    }
    assert(false); return false;
}
int main(void) {
    reset(); edge(2, true); edge(2, false); drain();
    assert(event_count == 2 && events[0].state && !events[1].state);
    puts("PASS ordinary press/release order");

    reset(); edge(0, true); drain(); flood(2); edge(2, false); edge(0, false); drain();
    assert(warnings == 1 && !consumer_state[0] && layer_depth == 0);
    puts("PASS lost final release recovers through the ordinary behavior event path");

    reset(); edge(0, true); drain(); flood(2); edge(2, false); drain();
    assert(consumer_state[0] && layer_depth == 1);
    edge(0, false); drain(); assert(layer_depth == 0);
    puts("PASS overflow does not release a genuinely held key");

    reset(); flood(2); edge(2, false); edge(4, true); drain();
    assert(consumer_state[4]); edge(4, false); drain();
    puts("PASS lost press is restored once and paired with its eventual release");

    reset(); edge(0, true); drain(); flood(2); edge(2, false); edge(0, false);
    warning_hook = press_after_snapshot; drain();
    assert(consumer_state[0] && !last_state_for_position(0, 1) && last_state_for_position(0, 0));
    edge(0, false); drain();
    puts("PASS post-snapshot press follows the recovered release");

    reset(); flood(2); edge(2, false); edge(0, true);
    warning_hook = release_after_snapshot; drain();
    assert(!consumer_state[0] && layer_depth == 0 &&
           last_state_for_position(0, 1) && !last_state_for_position(0, 0));
    puts("PASS post-snapshot release follows the recovered press");

    reset(); edge(0, true); edge(0, false);
    assert(zmk_physical_layouts_select_layout(&layout_b) == -EBUSY);
    drain(); edge(0, true); drain();
    assert(zmk_physical_layouts_select_layout(&layout_b) == -EBUSY);
    event_hook = try_switch_in_event; edge(0, false); drain();
    disable_hook = stale_callback_during_switch;
    assert(zmk_physical_layouts_select_layout(&layout_b) == 0);
    assert(!scan_latest_state[0] && !work_pending);
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) {
        assert(selected_to_stock_map[p] == ZMK_KEYMAP_LEN - p - 1);
        assert(!scan_latest_state[p] && !scan_reported_state[p]);
    }
    edge(0, true); drain();
    assert(consumer_state[ZMK_KEYMAP_LEN - 1] && !consumer_state[0]);
    edge(0, false); drain();
    puts("PASS busy layout changes reject safely; a quiescent change resets the state and mapping");

    reset(); map_error = -EINVAL;
    assert(zmk_physical_layouts_select_layout(&layout_b) == -EINVAL);
    map_error = 0;
    assert(active == &layout_a && scan_accepting);
    pm_error_once = -EIO;
    assert(zmk_physical_layouts_select_layout(&layout_b) == -EIO);
    assert(active == &layout_a && scan_accepting && driver_enabled[0]);
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) { assert(selected_to_stock_map[p] == p); }
    edge(0, true); edge(0, false); drain();
    puts("PASS mapping/PM failures leave the old layout accepting scan events");

    reset();
    zmk_physical_layout_kscan_callback(&scanner_b, 0, 0, true);
    zmk_physical_layout_kscan_callback(&scanner_a, 99, 99, true);
    drain(); assert(event_count == 0);
    puts("PASS inactive scanner and invalid matrix positions are ignored");

    reset();
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) { edge(p, true); drain(); }
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) { edge(p, false); }
    drain(); assert(layer_depth == 0 && event_count == 2 * ZMK_KEYMAP_LEN);
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) { assert(!consumer_state[p]); }
    if (CONFIG_ZMK_KSCAN_EVENT_QUEUE_SIZE >= 75) { assert(warnings == 0); }
    else { assert(warnings == 1); }
    puts("PASS all 66 keys released in one scan, including forced small-queue overflow");

    reset();
    uint32_t random = 1234567;
    bool physical[ZMK_KEYMAP_LEN] = {0};
    for (int burst = 0; burst < 2000; burst++) {
        for (int i = 0; i < 80; i++) {
            random = random * 1664525U + 1013904223U;
            int p = random % ZMK_KEYMAP_LEN;
            physical[p] = !physical[p]; edge(p, physical[p]);
        }
        drain();
        for (int p = 0; p < ZMK_KEYMAP_LEN; p++) { assert(physical[p] == consumer_state[p]); }
        event_count = 0;
    }
    for (int p = 0; p < ZMK_KEYMAP_LEN; p++) { if (physical[p]) { edge(p, false); } }
    drain(); assert(layer_depth == 0);
    printf("PASS 160000 randomized edges; all states converge (queue capacity %d)\n",
           CONFIG_ZMK_KSCAN_EVENT_QUEUE_SIZE);
    return 0;
}
