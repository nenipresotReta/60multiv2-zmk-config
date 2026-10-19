static void reset(void) {
    profiles[0] = (struct profile){.peer = 1};
    profiles[1] = (struct profile){.peer = 2};
    active_profile = 0;
    advertising_status = ZMK_ADV_NONE;
    advertising_on = false;
    now = 0;
    start_calls = stop_calls = start_error = stop_error = resource_refs = profile_events = 0;
    during_start = NULL;
    advertising_update_running = 0;
    advertising_retry_delay_ms = ADV_RETRY_MIN_MS;
    advertising_retry_work.scheduled = false;
    update_advertising_work.queued = false;
    raise_profile_changed_event_work.queued = false;
}

static void step_retry(void) {
    assert(advertising_retry_work.scheduled);
    now = advertising_retry_work.due;
    advertising_retry_work.scheduled = false;
    advertising_retry_work.work.handler(&advertising_retry_work.work);
}

static void step_work(struct k_work *work) {
    assert(work->queued);
    work->queued = false;
    work->handler(work);
}

static void request_concurrently(void) { assert(update_advertising() == 0); }

static void connect_during_start(void) {
    profiles[active_profile].connected = true;
    assert(update_advertising() == 0);
}

int main(void) {
    reset();
    assert(update_advertising() == 0);
    assert(start_calls == 1 && advertising_on && !advertising_retry_work.scheduled);
    assert(update_advertising() == 0 && start_calls == 1);
    puts("PASS normal advertising succeeds once without periodic work");

    reset();
    struct bt_conn old_conn = {.role = BT_CONN_ROLE_PERIPHERAL, .peer = 1, .refs = 1};
    resource_refs = 1;
    disconnected(&old_conn, 0x13);
    assert(start_calls == 0 && update_advertising_work.queued);
    assert(raise_profile_changed_event_work.queued);
    step_work(&raise_profile_changed_event_work);
    step_work(&update_advertising_work);
    assert(start_calls == 1 && advertising_retry_work.due == 100 && profile_events == 1);
    step_retry();
    assert(start_calls == 2 && advertising_retry_work.due == 300);
    resource_refs = 0; /* Last HOG reference is finally released. No new key/connection event. */
    step_retry();
    assert(start_calls == 3 && advertising_on && !advertising_retry_work.scheduled);
    puts("PASS disconnect with retained references recovers without a new external event");

    reset();
    start_error = -ENOMEM;
    assert(update_advertising() == -ENOMEM);
    const int64_t deadlines[] = {100, 300, 700, 1500, 2500, 3500, 4500};
    for (unsigned i = 0; i < ARRAY_SIZE(deadlines); i++) {
        assert(advertising_retry_work.due == deadlines[i]);
        step_retry();
    }
    assert(start_calls == 8 && advertising_retry_delay_ms == 1000);
    start_error = 0;
    step_retry();
    assert(advertising_on && advertising_retry_delay_ms == 100 && !advertising_retry_work.scheduled);
    puts("PASS persistent shortage uses bounded exponential backoff and resets after recovery");

    const int transient_errors[] = {-ENOMEM, -ENOBUFS, -EAGAIN, -EBUSY};
    for (unsigned i = 0; i < ARRAY_SIZE(transient_errors); i++) {
        reset(); start_error = transient_errors[i];
        assert(update_advertising() == transient_errors[i]);
        assert(advertising_retry_work.due == 100);
        start_error = 0; step_retry();
        assert(advertising_on && !advertising_retry_work.scheduled);
    }
    puts("PASS transient connection/buffer/controller shortages are retried");

    const int permanent_errors[] = {-EINVAL, -ENOTSUP, -EIO};
    for (unsigned i = 0; i < ARRAY_SIZE(permanent_errors); i++) {
        reset(); start_error = permanent_errors[i];
        assert(update_advertising() == permanent_errors[i]);
        assert(!advertising_retry_work.scheduled);
    }
    puts("PASS permanent errors are returned without an endless retry loop");

    reset(); start_error = -ENOMEM; update_advertising();
    active_profile = 1; profiles[1].connected = true;
    start_error = 0; step_retry();
    assert(start_calls == 1 && !advertising_on && !advertising_retry_work.scheduled);
    puts("PASS pending retry rechecks a newly selected connected profile");

    reset(); start_error = -ENOMEM; update_advertising();
    active_profile = 1; profiles[1].open = true;
    start_error = 0; step_retry();
    assert(advertising_on && start_calls == 2 && !advertising_retry_work.scheduled);
    puts("PASS pending retry follows a newly selected open profile");

    reset(); start_error = -ENOMEM; update_advertising();
    profiles[0].connected = true; start_error = 0;
    connected(&old_conn, 0);
    assert(raise_profile_changed_event_work.queued);
    step_retry();
    assert(start_calls == 1 && !advertising_on && !advertising_retry_work.scheduled);
    puts("PASS successful connection preserves profile event and stops retrying");

    reset(); resource_refs = 1;
    connected(&old_conn, 0x3e);
    assert(start_calls == 1 && advertising_retry_work.scheduled);
    assert(!raise_profile_changed_event_work.queued);
    resource_refs = 0; step_retry();
    assert(advertising_on);
    puts("PASS failed connection also recovers when its connection slot becomes free");

    reset(); start_error = -ENOMEM; update_advertising();
    now = 90; update_advertising();
    assert(advertising_retry_work.due == 100);
    puts("PASS repeated external updates do not postpone an existing retry");

    reset(); during_start = request_concurrently;
    assert(update_advertising() == 0 && start_calls == 1 && advertising_retry_work.scheduled);
    step_retry();
    assert(start_calls == 1 && !advertising_retry_work.scheduled);
    puts("PASS overlapping updates do not call the advertising stack concurrently");

    reset(); during_start = connect_during_start;
    assert(update_advertising() == 0 && advertising_on && advertising_retry_work.scheduled);
    step_retry();
    assert(!advertising_on && stop_calls == 1 && !advertising_retry_work.scheduled);
    puts("PASS successful outer update does not cancel a newer concurrent profile change");

    reset(); assert(update_advertising() == 0);
    profiles[0].connected = true; stop_error = -ENOBUFS;
    assert(update_advertising() == -ENOBUFS && advertising_status == ZMK_ADV_CONN);
    assert(advertising_on && advertising_retry_work.scheduled);
    stop_error = 0; step_retry();
    assert(!advertising_on && advertising_status == ZMK_ADV_NONE && stop_calls == 2);
    assert(!advertising_retry_work.scheduled);
    puts("PASS failed stop retains actual advertising state until its retry succeeds");

    reset();
    struct bt_conn central = {.role = 0, .peer = 1};
    connected(&central, 0); disconnected(&central, 0x13);
    assert(start_calls == 0 && !update_advertising_work.queued);
    assert(!raise_profile_changed_event_work.queued && !advertising_retry_work.scheduled);
    puts("PASS central-role callbacks retain their original no-op behavior");
    return 0;
}
