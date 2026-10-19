static void reset(void) {
    assert(!keyboard_sender.pending.conn && !consumer_sender.pending.conn);
    assert(!zmk_hog_keyboard_msgq.count && !zmk_hog_consumer_msgq.count && !zmk_hog_mouse_msgq.count);
    assert(!keyboard_tx.outstanding && !consumer_tx.outstanding && !mouse_tx.outstanding);
    for(int i=0;i<3;i++) { assert(connections[i].refs==0); connections[i]=(struct bt_conn){i,1,0,true}; }
    lookup[0]=&connections[0]; lookup[1]=&connections[1];
    selected=(struct zmk_endpoint_instance){1,{0}};
    now=0; failures_left=0; notify_error=-ENOMEM; security_calls=0;
    disconnect_calls=0; disconnect_error=0; sent_count=0; attempts=0;
    hog_keyboard_work.scheduled=false; hog_consumer_work.scheduled=false; hog_mouse_work.queued=false;
    wdt_feed_work.scheduled=false; feed_count=0; usb_status=USB_DC_CONFIGURED;
    notify_probe=NULL; zmk_wdt_note_tx_ok();
}
static void enqueue(struct reliable_report_sender *sender, int keys) {
    if(sender==&keyboard_sender) {
        struct zmk_hid_keyboard_report_body r={(uint8_t)keys};
        assert(zmk_hog_send_keyboard_report(&r)==0);
    } else {
        struct zmk_hid_consumer_report_body r={(uint8_t)keys};
        assert(zmk_hog_send_consumer_report(&r)==0);
    }
}
static void step(struct reliable_report_sender *sender) {
    struct k_work_delayable *work=sender->work;
    assert(work->scheduled);
    if(now<work->due) now=work->due;
    work->scheduled=false; work->work.handler(&work->work);
}
static void drain(struct reliable_report_sender *sender) {
    for(int i=0;sender->work->scheduled;i++) { assert(i<200); step(sender); }
}
static void sender_cases(struct reliable_report_sender *sender) {
    reset(); enqueue(sender,1); enqueue(sender,0); drain(sender);
    assert(sent_count==2 && sent_keys[0]==1 && sent_keys[1]==0);
    puts("PASS normal press/release with value attribute");

    reset(); enqueue(sender,1); drain(sender); failures_left=2; enqueue(sender,0); step(sender);
    assert(sent_count==1 && sender->pending.conn && sender->work->due==20);
    drain(sender); assert(sent_count==2 && sent_keys[1]==0 && now==40);
    puts("PASS final release retries without a new key event");

    reset(); failures_left=1; enqueue(sender,1); step(sender); enqueue(sender,0); enqueue(sender,2);
    assert(sender->work->due==20); drain(sender);
    assert(sent_count==3 && sent_keys[0]==1 && sent_keys[1]==0 && sent_keys[2]==2);
    puts("PASS FIFO ordering during retry");

    reset(); connections[0].encrypted=false; enqueue(sender,0); step(sender);
    assert(security_calls==1 && sent_count==0 && sender->pending.conn);
    step(sender); assert(security_calls==1 && sent_count==0);
    connections[0].encrypted=true; drain(sender); assert(sent_count==1 && sent_keys[0]==0);
    puts("PASS unencrypted state is retained until encryption recovers");

    reset(); failures_left=-1; enqueue(sender,0); enqueue(sender,1); drain(sender);
    assert(disconnect_calls==1 && sent_count==0 && now==1000);
    assert(connections[0].state==BT_CONN_STATE_DISCONNECTING);
    puts("PASS permanent failure disconnects before obsolete states can be sent");

    reset(); failures_left=-1; disconnect_error=-ENOMEM; enqueue(sender,0);
    for(int i=0;i<51;i++) step(sender);
    assert(disconnect_calls==1 && sender->pending.conn);
    failures_left=0; drain(sender); assert(sent_count==1 && sent_keys[0]==0);
    puts("PASS failed disconnect retains final release");

    reset(); failures_left=1; enqueue(sender,1); step(sender);
    connections[0].state=0; lookup[0]=&connections[2]; enqueue(sender,0); drain(sender);
    assert(sent_count==1 && sent_host[0]==2 && sent_keys[0]==0);
    puts("PASS reconnect cannot replay old connection data");

    reset(); enqueue(sender,0); selected.ble.profile_index=1; enqueue(sender,2); drain(sender);
    assert(sent_count==2 && sent_host[0]==0 && sent_keys[0]==0 && sent_host[1]==1);
    puts("PASS profile switch keeps old endpoint release on old connection");

    reset(); for(int i=1;i<=8;i++) enqueue(sender,i); enqueue(sender,0); drain(sender);
    assert(sent_count==3 && sent_keys[2]==0);
    puts("PASS overflow preserves newest release and balances references");

    reset(); failures_left=2; enqueue(sender,1); step(sender);
    for(int i=2;i<=8;i++) enqueue(sender,i);
    enqueue(sender,0); drain(sender);
    assert(sent_count==4 && sent_keys[0]==1 && sent_keys[3]==0);
    puts("PASS overflow with pending retry still ends at all-keys-up");

    reset(); enqueue(sender,1); enqueue(sender,0); enqueue(sender,0);
    selected.ble.profile_index=1; enqueue(sender,2); drain(sender);
    assert(disconnect_calls==1 && sent_count==1 && sent_host[0]==1);
    puts("PASS cross-host overflow disconnects old host");

    reset(); uint8_t snapshot=7;
    assert(queue_reliable_report(sender,&snapshot)==0); snapshot=0; drain(sender);
    assert(sent_count==1 && sent_keys[0]==7);
    puts("PASS snapshot copy isolates later HID mutations");

    reset(); lookup[0]=NULL; assert(queue_reliable_report(sender,&snapshot)==-ENOTCONN);
    selected.transport=ZMK_TRANSPORT_USB; assert(queue_reliable_report(sender,&snapshot)==-ENOTCONN);
    assert(!sender->tx->outstanding);
    puts("PASS disconnected and USB enqueue rejection");
}
static void probe_single_inflight(void) {
    assert(!zmk_hog_keyboard_msgq.count && keyboard_tx.outstanding==1);
    now=CONFIG_ZMK_UNRESPONSE_FIX_TX_STALL_MS;
    assert(zmk_hog_tx_stalled(CONFIG_ZMK_UNRESPONSE_FIX_TX_STALL_MS));
    assert(transport_tx_stalled());
}
static void probe_progress(void) {
    now+=1000;
    assert(!zmk_hog_tx_stalled(CONFIG_ZMK_UNRESPONSE_FIX_TX_STALL_MS));
    if(attempts<100) enqueue(&keyboard_sender,0);
}
int main(void) {
    puts("KEYBOARD:"); sender_cases(&keyboard_sender);
    puts("CONSUMER:"); sender_cases(&consumer_sender);

    reset(); enqueue(&keyboard_sender,1); drain(&keyboard_sender); now=3600000;
    assert(!zmk_hog_tx_stalled(30000) && !transport_tx_stalled());
    puts("PASS legitimate long hold and inactivity cannot trigger TX timeout");

    reset(); enqueue(&keyboard_sender,0); notify_probe=probe_single_inflight; drain(&keyboard_sender);
    assert(!zmk_hog_tx_stalled(30000));
    puts("PASS single final report inside notify is visible to watchdog");

    reset(); enqueue(&keyboard_sender,1); enqueue(&keyboard_sender,0); notify_probe=probe_progress;
    drain(&keyboard_sender); assert(sent_count==101 && now==101000);
    puts("PASS continuously nonempty successful queue advances watchdog watermark");

    reset(); failures_left=-1; disconnect_error=-ENOMEM; enqueue(&keyboard_sender,0);
    for(int i=0;i<=1500;i++) step(&keyboard_sender);
    assert(transport_tx_stalled() && keyboard_tx.outstanding==1);
    failures_left=0; drain(&keyboard_sender); assert(!transport_tx_stalled());
    puts("PASS permanent retry/disconnect failure remains visible after timeout");

    reset(); enqueue(&keyboard_sender,0); now=30000;
    assert(transport_tx_stalled());
    connections[0].state=0; assert(!transport_tx_stalled());
    connections[0].state=1; selected.transport=ZMK_TRANSPORT_USB; assert(!transport_tx_stalled());
    selected.transport=ZMK_TRANSPORT_BLE; drain(&keyboard_sender);
    puts("PASS disconnected or unselected BLE cannot trigger restart");

    reset(); enqueue(&keyboard_sender,0); now=30000;
    wdt_feed_handler(NULL); assert(feed_count==0 && wdt_feed_work.scheduled);
    connections[0].state=0; now=31000; wdt_feed_handler(NULL); assert(feed_count==1);
    drain(&keyboard_sender);
    puts("PASS disconnect during hardware watchdog grace period resumes feeding");

    reset(); enqueue(&keyboard_sender,0); now=30000;
    wdt_feed_handler(NULL); assert(feed_count==0);
    drain(&keyboard_sender); now+=1000; wdt_feed_handler(NULL); assert(feed_count==1);
    puts("PASS genuine TX recovery during watchdog grace period resumes feeding");

    reset(); selected.transport=ZMK_TRANSPORT_USB;
    for(int i=0;i<=1500;i++) { now=i*20; zmk_wdt_note_tx_fail(); }
    assert(transport_tx_stalled());
    zmk_wdt_note_tx_ok(); assert(!transport_tx_stalled());
    puts("PASS frequent USB failures cannot continually reset timeout");

    reset(); selected.transport=ZMK_TRANSPORT_USB; zmk_wdt_note_tx_fail(); now=30000;
    assert(transport_tx_stalled());
    usb_status=0; assert(!transport_tx_stalled());
    usb_status=USB_DC_CONFIGURED; assert(!transport_tx_stalled());
    puts("PASS one final USB failure remains visible; disconnect clears old fault");

    reset(); now=UINT32_MAX-10000LL; enqueue(&keyboard_sender,0); now+=30000;
    assert(zmk_hog_tx_stalled(30000)); drain(&keyboard_sender);
    puts("PASS uptime wrap preserves BLE timeout arithmetic");

    reset(); now=UINT32_MAX-10000LL; selected.transport=ZMK_TRANSPORT_USB;
    zmk_wdt_note_tx_fail(); now+=30000; assert(transport_tx_stalled()); zmk_wdt_note_tx_ok();
    puts("PASS uptime wrap preserves USB timeout arithmetic");

    reset(); struct zmk_hid_mouse_report_body mouse={1};
    assert(zmk_hog_send_mouse_report(&mouse)==0); assert(mouse_tx.outstanding==1);
    hog_mouse_work.handler(&hog_mouse_work);
    assert(sent_count==1 && sent_attr[0]==14 && !mouse_tx.outstanding);
    puts("PASS mouse notify uses encrypted value attribute and tracks completion");

    reset(); for(int i=0;i<6;i++) assert(zmk_hog_send_mouse_report(&mouse)==0);
    hog_mouse_work.handler(&hog_mouse_work); assert(!mouse_tx.outstanding && sent_count==3);
    puts("PASS legacy mouse overflow balances outstanding counter");

    reset(); failures_left=1; enqueue(&keyboard_sender,0); step(&keyboard_sender);
    enqueue(&consumer_sender,1); drain(&consumer_sender);
    assert(sent_count==1 && sent_attr[0]==10);
    drain(&keyboard_sender); assert(sent_count==2 && sent_attr[1]==6);
    puts("PASS keyboard retry yields HOG queue for consumer work");

    reset(); puts("PASS all queues empty and all connection references balanced");
    return 0;
}
