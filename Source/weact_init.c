/* Copyright (C) CheshirCa 2026   https://t.me/cheshircanest
 * WeAct Display Tools Contributors; see LICENSE (MIT).
 * Managed FS35 initialization and owned lifecycle.
 */
#include "weact_internal.h"
#include <errno.h>
#include <string.h>

static wx_init_report_t empty_report(void)
{
    return (wx_init_report_t){.struct_size=sizeof(wx_init_report_t)};
}
static bool valid_report(const wx_init_report_t *r)
{
    return !r || r->struct_size == sizeof(*r);
}
static wx_status_t error(wx_init_report_t *r, wx_status_t status,
                         wx_init_reason_t reason, int os_error)
{
    r->status=status;
    r->primary_status=status;
    r->reason=reason;
    r->primary_errno=os_error;
    return status;
}
static wx_status_t publish(wx_init_report_t *out, const wx_init_report_t *r)
{
    if (out) *out=*r;
    return r->status;
}
static void state(wx_init_metadata_t *m, wx_init_state_t next)
{
    m->info.state=next;
    if (m->report.state_count < sizeof(m->report.state_history)/sizeof(m->report.state_history[0]))
        m->report.state_history[m->report.state_count++]=next;
}

wx_status_t wx_init_dispose_channel(wx_device_t *d, bool failed)
{
    wx_init_metadata_t *m=d->init;
    d->failed=true;
    m->info.geometry_usable=false;
    m->info.confirmed_valid=false;
    m->info.confirmed_width=m->info.confirmed_height=0;
    m->info.fb_sync=WX_FB_UNKNOWN;
    if (m->info.state != WX_INIT_FAILED && m->info.state != WX_INIT_CLOSED)
        state(m, failed ? WX_INIT_FAILED : WX_INIT_CLOSED);
    if (!m->owned) return WX_OK;
    /* Relinquish engine ownership before callbacks. Do not dereference their
     * context after release: release may free it even when reporting failure. */
    wx_init_channel_t channel=m->channel;
    m->owned=false;
    m->channel=(wx_init_channel_t){0};
    d->transport=(wx_transport_t){0};
    if (!m->sealed && channel.seal) {
        m->sealed=true;
        m->report.seal_called=true;
        channel.seal(channel.tx.context);
    }
    wx_init_report_t cleanup=empty_report();
    m->report.release_called=true;
    errno=0;
    wx_status_t status=channel.release ? channel.release(channel.tx.context,&cleanup) : WX_INVALID;
    int saved=errno;
    m->report.cleanup_status=status;
    m->report.release_succeeded=status==WX_OK;
    m->report.release_errno=cleanup.release_errno;
    m->report.close_errno=cleanup.close_errno;
    m->report.close_called=cleanup.close_called;
    m->report.close_succeeded=cleanup.close_succeeded;
    if (status!=WX_OK) {
        if (!m->report.release_errno && !m->report.close_errno)
            m->report.release_errno=saved;
        if (m->info.state!=WX_INIT_FAILED) state(m,WX_INIT_FAILED);
        if (m->report.primary_status==WX_OK) m->report.reason=WX_INIT_RELEASE_FAILED;
        m->report.status=m->report.primary_status==WX_OK ? status : m->report.primary_status;
        m->info.last_reason=m->report.reason;
    }
    return status;
}
void wx_init_metadata_destroy(wx_device_t *d)
{
    if (!d->init) return;
    (void)wx_init_dispose_channel(d,false);
    wx_release(&d->allocator,d->init);
    d->init=NULL;
}
/* Placeholder callbacks allow the existing pure host allocator to be reused
 * before acquisition. They cannot transmit and are replaced only by a valid
 * lease. No public create semantics or public structures are changed. */
static wx_status_t unconnected_write(void *c,const uint8_t *b,size_t n,size_t *written)
{
    (void)c;(void)b;(void)n;*written=0;return WX_STREAM_FAILED;
}
static uint64_t unconnected_now(void *c) { (void)c;return UINT64_MAX; }
static wx_status_t unconnected_wait(void *c,uint64_t end)
{ (void)c;(void)end;return WX_STREAM_FAILED; }
static bool valid_channel(const wx_init_channel_t *c)
{
    return c->struct_size==sizeof(*c) && c->tx.write && c->tx.now_ms &&
           c->tx.wait_writable && c->read && c->wait_readable && c->probe_quiet &&
           c->check_guards && c->seal && c->release;
}
static wx_status_t check_time(const wx_init_connector_t *c,wx_init_report_t *r,
                              uint64_t *last)
{
    uint64_t now=0;
    errno=0;
    wx_status_t status=c->now_ms(c->context,&now);
    int saved=errno;
    if (status!=WX_OK || now<*last)
        return error(r,WX_IO,WX_INIT_CLOCK_ERROR,saved);
    r->finish_ms=now;
    *last=now;
    if (now>=r->deadline_ms) return error(r,WX_TIMEOUT,WX_INIT_DEADLINE,0);
    return WX_OK;
}
static wx_status_t factory_validation(const wx_init_options_t *o,wx_device_t **out,
                                      wx_init_report_t *r)
{
    if (!out || *out || !o || o->struct_size!=sizeof(*o))
        return error(r,WX_INVALID,WX_INIT_INVALID_OPTIONS,0);
    if (!wx_allocator_valid(&o->allocator) || o->timeout_ms!=2000 ||
        o->connector.struct_size!=sizeof(o->connector) || !o->connector.now_ms ||
        !o->connector.acquire)
        return error(r,WX_INVALID,WX_INIT_INVALID_OPTIONS,0);
    if (o->model!=WX_FS35 || o->orientation!=2)
        return error(r,WX_INVALID,WX_INIT_PROFILE_NOT_ENABLED,0);
    if (o->stride && o->stride<960)
        return error(r,WX_INVALID,WX_INIT_INVALID_OPTIONS,0);
    if (o->stride>SIZE_MAX/320)
        return error(r,WX_OVERFLOW,WX_INIT_INVALID_OPTIONS,0);
    return WX_OK;
}
/* All init operations consume one deadline. Writes are never retried. */
static wx_status_t timed(wx_device_t *d)
{
    wx_init_report_t *r=&d->init->report;
    uint64_t now=d->transport.now_ms(d->transport.context);
    if (now==UINT64_MAX || now<r->finish_ms)
        return error(r,WX_IO,WX_INIT_CLOCK_ERROR,errno);
    r->finish_ms=now;
    return now>=r->deadline_ms ? error(r,WX_TIMEOUT,WX_INIT_DEADLINE,0) : WX_OK;
}
static wx_status_t outcome(wx_device_t *d,wx_status_t status,wx_init_reason_t reason)
{
    int saved=errno;
    wx_status_t t=timed(d);
    if (t!=WX_OK) return t;
    return status==WX_OK ? WX_OK : error(&d->init->report,status,reason,saved);
}
static wx_status_t quiet(wx_device_t *d)
{
    if (timed(d)!=WX_OK) return d->init->report.status;
    return outcome(d,d->init->channel.probe_quiet(d->transport.context,
                   d->init->report.deadline_ms),WX_INIT_UNEXPECTED_INPUT);
}
static wx_status_t guard(wx_device_t *d,wx_init_stage_t stage)
{
    wx_init_report_t *r=&d->init->report;r->last_stage=stage;
    if (timed(d)!=WX_OK) return r->status;
    wx_status_t status=d->init->channel.check_guards(d->transport.context,stage,r->deadline_ms,r);
    return outcome(d,status,r->reason==WX_INIT_REASON_NONE ? WX_INIT_TRANSPORT_ERROR : r->reason);
}
static wx_status_t wait_until(wx_device_t *d,bool rx)
{
    wx_status_t status;
    do {
        if (timed(d)!=WX_OK) return d->init->report.status;
        status=rx ? d->init->channel.wait_readable(d->transport.context,d->init->report.deadline_ms) :
                    d->transport.wait_writable(d->transport.context,d->init->report.deadline_ms);
        if (timed(d)!=WX_OK) return d->init->report.status;
    } while (status==WX_AGAIN || status==WX_INTERRUPTED);
    return outcome(d,status,WX_INIT_TRANSPORT_ERROR);
}
static wx_status_t once(wx_device_t *d,const uint8_t *p,size_t n,size_t *accepted,wx_init_stage_t stage)
{
    wx_init_report_t *r=&d->init->report;r->last_stage=stage;
    if (wait_until(d,false)!=WX_OK || quiet(d)!=WX_OK) return r->status;
    size_t written=0;
    if (stage==WX_INIT_STAGE_FILL_TX) r->fill_allowed=r->fill_attempted=true;
    ++r->write_calls;
    wx_status_t status=d->transport.write(d->transport.context,p,n,&written);
    *accepted=written<=n ? written : 0;
    if (outcome(d,status,WX_INIT_TRANSPORT_ERROR)!=WX_OK) return r->status;
    if (written!=n) return error(r,WX_IO,written ? WX_INIT_SHORT_WRITE : WX_INIT_ZERO_WRITE,0);
    return WX_OK;
}
static wx_status_t query_orientation(wx_device_t *d, bool preserve)
{
    wx_init_metadata_t *m=d->init;wx_init_report_t *r=&m->report;
    static const uint8_t query[]={0x82,0x0a};
    m->info.geometry_usable=false;m->info.confirmed_valid=false;
    if (guard(d,WX_INIT_STAGE_QUERY_TX)!=WX_OK ||
        once(d,query,2,&r->query_accepted,WX_INIT_STAGE_QUERY_TX)!=WX_OK) return r->status;
    r->last_stage=WX_INIT_STAGE_RX;
    while (r->rx_length<3) {
        if (wait_until(d,true)!=WX_OK) return r->status;
        size_t n=0,capacity=sizeof(r->rx)-r->rx_length;
        wx_status_t status=m->channel.read(d->transport.context,r->rx+r->rx_length,capacity,&n);
        if (timed(d)!=WX_OK) return r->status;
        if ((status==WX_AGAIN || status==WX_INTERRUPTED) && !n) continue;
        if (outcome(d,status,WX_INIT_TRANSPORT_ERROR)!=WX_OK) return r->status;
        if (!n || n>capacity) return error(r,WX_IO,WX_INIT_RX_ZERO_OR_EOF,0);
        r->rx_length+=n;
    }
    r->response_complete=true;
    if (quiet(d)!=WX_OK) return r->status;
    if (r->rx_length!=3 || r->rx[0]!=0x82 || r->rx[2]!=0x0a)
        return error(r,WX_IO,WX_INIT_INVALID_RESPONSE,0);
    r->response_valid=true;
    if (r->rx[1]>3) return error(r,WX_INVALID,WX_INIT_UNKNOWN_ORIENTATION,0);
    /* Firmware uses both landscape codes on FS35: 2 and 3 are 480x320.
     * Preserve/confirm accepts either without transmitting a setter. */
    if (r->rx[1]<2 || r->rx[1]>3 || (!preserve && r->rx[1]!=2))
        return error(r,WX_INVALID,WX_INIT_ORIENTATION_MISMATCH,0);
    r->orientation_matching=true;
    m->info.confirmed_valid=m->info.geometry_usable=true;
    m->info.confirmed_orientation=r->rx[1];m->info.confirmed_width=480;m->info.confirmed_height=320;
    state(m,WX_INIT_ORIENTATION_CONFIRMED);
    return WX_OK;
}
static wx_status_t initialize(wx_device_t *d)
{
    wx_init_metadata_t *m=d->init;wx_init_report_t *r=&m->report;
    static const uint8_t setter[]={2,2,10};
    static const uint8_t fill[]={4,0,0,0,0,0xdf,1,0x3f,1,0,0,10};
    m->info.requested_valid=true;m->info.requested_orientation=2;
    if (guard(d,WX_INIT_STAGE_SET_ORIENTATION)!=WX_OK ||
        once(d,setter,3,&r->setter_accepted,WX_INIT_STAGE_SET_ORIENTATION)!=WX_OK ||
        query_orientation(d,false)!=WX_OK || guard(d,WX_INIT_STAGE_PRE_FILL)!=WX_OK) return r->status;
    if (once(d,fill,12,&r->fill_accepted,WX_INIT_STAGE_FILL_TX)!=WX_OK) return r->status;
    r->fill_tx_complete=true;state(m,WX_INIT_DISPLAY_CLEARED_TX);
    r->last_stage=WX_INIT_STAGE_COMMIT;
    memset(d->surface->pixels,0,d->surface->info.capacity);
    if (timed(d)!=WX_OK) return r->status;
    m->info.fb_sync=WX_FB_BLACK_BASELINE_ASSUMED;r->fb_baseline_at_return=true;
    return WX_OK;
}
static wx_status_t factory(const wx_init_options_t *o,wx_device_t **out,
                            wx_init_report_t *report, bool preserve)
{
    if (!valid_report(report)) return WX_INVALID;
    wx_init_report_t r=empty_report();
    if (factory_validation(o,out,&r)!=WX_OK) return publish(report,&r);
    uint64_t start=0;
    errno=0;
    wx_status_t status=o->connector.now_ms(o->connector.context,&start);
    int saved=errno;
    if (status!=WX_OK) {
        (void)error(&r,WX_IO,WX_INIT_CLOCK_ERROR,saved);return publish(report,&r);
    }
    r.start_ms=r.finish_ms=start;
    if (start>UINT64_MAX-o->timeout_ms) {
        (void)error(&r,WX_OVERFLOW,WX_INIT_DEADLINE,0);return publish(report,&r);
    }
    r.deadline_ms=start+o->timeout_ms;
    r.last_stage=WX_INIT_STAGE_ALLOCATE;
    wx_init_metadata_t *m=wx_allocate(&o->allocator,sizeof(*m));
    if (!m) {
        (void)error(&r,WX_NOMEM,WX_INIT_ALLOCATION_FAILED,0);return publish(report,&r);
    }
    *m=(wx_init_metadata_t){.report=r};
    wx_device_options_t options={.struct_size=sizeof(options),.model=o->model,
        .width=480,.height=320,.stride=o->stride,.orientation=o->orientation,
        .timeout_ms=o->timeout_ms,.allocator=o->allocator,
        .transport={NULL,unconnected_write,unconnected_now,unconnected_wait}};
    wx_device_t *d=NULL;
    status=wx_device_create(&options,&d);
    if (status!=WX_OK) {
        wx_release(&o->allocator,m);
        (void)error(&r,status,WX_INIT_ALLOCATION_FAILED,0);return publish(report,&r);
    }
    d->init=m;
    m->info=(wx_init_info_t){.struct_size=sizeof(m->info),.managed=true,
        .model=o->model,.configured_orientation=o->orientation,.fb_sync=WX_FB_UNKNOWN};
    state(m,WX_INIT_CREATED);
    uint64_t last=start;
    status=check_time(&o->connector,&m->report,&last);
    if (status!=WX_OK) goto failed;
    m->report.last_stage=WX_INIT_STAGE_ACQUIRE;
    wx_init_channel_t channel={0};
    errno=0;
    status=o->connector.acquire(o->connector.context,m->report.deadline_ms,&channel,&m->report);
    saved=errno;
    if (status!=WX_OK) {
        /* Failed acquire owns/cleans all partial resources itself; never try
         * to release the provider's failed/empty descriptor a second time. */
        (void)error(&m->report,status,m->report.reason==WX_INIT_REASON_NONE ?
                    WX_INIT_ACQUIRE_FAILED : m->report.reason,
                    m->report.primary_errno ? m->report.primary_errno : saved);
        goto failed;
    }
    m->channel=channel;
    m->owned=true;
    m->report.acquire_succeeded=true;
    if (!valid_channel(&channel)) {
        (void)error(&m->report,WX_INVALID,WX_INIT_INVALID_CHANNEL,0);goto failed;
    }
    d->transport=channel.tx;
    state(m,WX_INIT_CONNECTED);
    status=check_time(&o->connector,&m->report,&last);
    if (status!=WX_OK) goto failed;
    if (!preserve) {
        status=initialize(d);
        if (status!=WX_OK) goto failed;
    }
    /* Preserve never invokes read/write/wait/quiet/check_guards: acquire is
     * responsible for connection policy. No query and no auto-flush. */
    m->report.last_stage=WX_INIT_STAGE_COMPLETE;
    state(m,preserve ? WX_INIT_READY_PRESERVED : WX_INIT_READY);
    m->report.status=m->report.primary_status=WX_OK;
    *out=d;
    return publish(report,&m->report);
failed:
    m->info.last_reason=m->report.reason;
    (void)wx_init_dispose_channel(d,true);
    r=m->report;
    wx_device_destroy(d);
    return publish(report,&r);
}
wx_status_t wx_device_init(const wx_init_options_t *o,wx_device_t **out,wx_init_report_t *r)
{ return factory(o,out,r,false); }
wx_status_t wx_device_init_preserve(const wx_init_options_t *o,wx_device_t **out,wx_init_report_t *r)
{ return factory(o,out,r,true); }
wx_status_t wx_device_confirm_orientation(wx_device_t *d,wx_init_report_t *report)
{
    if (!valid_report(report)) return WX_INVALID;
    wx_init_report_t r=empty_report();
    if (!d || !d->init || d->failed) {
        error(&r,d && d->failed ? WX_STREAM_FAILED : WX_INVALID,
              d && d->failed ? WX_INIT_CHANNEL_CLOSED : WX_INIT_INVALID_OPTIONS,0);
        return publish(report,&r);
    }
    wx_init_metadata_t *m=d->init;
    m->report=empty_report();
    uint64_t now=d->transport.now_ms(d->transport.context);
    m->report.start_ms=m->report.finish_ms=now;
    wx_status_t status;
    if (now>UINT64_MAX-2000) status=error(&m->report,WX_IO,WX_INIT_CLOCK_ERROR,errno);
    else {
        m->report.deadline_ms=now+2000;
        status=query_orientation(d,true);
    }
    if (status!=WX_OK) {
        m->info.last_reason=m->report.reason;
        (void)wx_init_dispose_channel(d,true);
    } else {
        m->info.last_reason=WX_INIT_REASON_NONE;
        state(m,WX_INIT_READY_PRESERVED);
    }
    return publish(report,&m->report);
}
wx_status_t wx_device_get_init_info(const wx_device_t *d,wx_init_info_t *out)
{
    if (!d || !out || out->struct_size!=sizeof(*out)) return WX_INVALID;
    *out=d->init ? d->init->info : (wx_init_info_t){.struct_size=sizeof(*out),
        .state=WX_INIT_CREATED,.model=d->model,.configured_orientation=d->orientation,
        .fb_sync=WX_FB_UNKNOWN};
    return WX_OK;
}
wx_status_t wx_device_init_close(wx_device_t *d,wx_init_report_t *report)
{
    if (!valid_report(report)) return WX_INVALID;
    wx_init_report_t r=empty_report();
    if (!d || !d->init) {
        (void)error(&r,WX_INVALID,WX_INIT_INVALID_OPTIONS,0);return publish(report,&r);
    }
    wx_init_metadata_t *m=d->init;
    m->report.last_stage=WX_INIT_STAGE_CLOSE;
    bool owned=m->owned;
    wx_status_t status=wx_init_dispose_channel(d,false);
    if (m->info.last_reason==WX_INIT_REASON_NONE) m->info.last_reason=WX_INIT_CHANNEL_CLOSED;
    if (report) *report=m->report;
    return owned ? status : WX_OK; /* Repeated close does not retry cleanup. */
}
