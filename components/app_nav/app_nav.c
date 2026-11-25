#include "app_nav.h"

#include <stdint.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "APP_NAV";

// Запас сверх длительности анимации, после которого переход принудительно
// завершается, если LV_EVENT_SCREEN_UNLOADED почему-то не пришёл.
#define NAV_WATCHDOG_MARGIN_MS  400
#define NAV_WATCHDOG_RETRY_MS   100
#define NAV_WATCHDOG_MAX_RETRY  20

typedef struct {
    const screen_desc_t *desc;
    lv_obj_t *scr;
} nav_entry_t;

typedef enum {
    NAV_OP_PUSH,
    NAV_OP_POP,
    NAV_OP_REPLACE,
    NAV_OP_HOME,
} nav_op_t;

typedef struct {
    nav_op_t op;
    const screen_desc_t *target;      // для PUSH / REPLACE
    lv_screen_load_anim_t anim;
    uint32_t ms;
} nav_req_t;

static struct {
    const screen_desc_t *registry;
    size_t count;

    nav_entry_t stack[NAV_STACK_MAX];  // stack[0] - всегда home
    size_t depth;

    // --- текущий переход ---
    bool busy;
    uint32_t gen;                      // номер перехода, отсекает "протухшие" колбэки
    lv_obj_t *from;                    // экран, который уходит
    lv_obj_t *to;                      // экран, который приходит
    nav_entry_t doomed[NAV_STACK_MAX]; // что уничтожить, когда переход закончится
    size_t doomed_n;
    lv_timer_t *watchdog;
    uint8_t retries;

    // --- очередь из одного слота ---
    bool has_queued;
    nav_req_t queued;
} s;

static void execute(const nav_req_t *r);

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------
static const screen_desc_t *find_desc(const char *id)
{
    if (!id) return NULL;
    for (size_t i = 0; i < s.count; i++) {
        if (s.registry[i].id && strcmp(s.registry[i].id, id) == 0) return &s.registry[i];
    }
    return NULL;
}

static bool is_home(const screen_desc_t *d)
{
    return d && d->id && strcmp(d->id, NAV_HOME_ID) == 0;
}

static bool in_stack(const screen_desc_t *d)
{
    for (size_t i = 0; i < s.depth; i++) {
        if (s.stack[i].desc == d) return true;
    }
    return false;
}

static bool is_doomed(const lv_obj_t *scr)
{
    for (size_t i = 0; i < s.doomed_n; i++) {
        if (s.doomed[i].scr == scr) return true;
    }
    return false;
}

static nav_entry_t *top(void)
{
    return s.depth ? &s.stack[s.depth - 1] : NULL;
}

// ---------------------------------------------------------------------------
// завершение перехода: вот здесь, и только здесь, вызываются destroy()
// ---------------------------------------------------------------------------
static void watchdog_cb(lv_timer_t *t);
static void on_from_unloaded(lv_event_t *e);

static void arm_watchdog(uint32_t period_ms)
{
    s.watchdog = lv_timer_create(watchdog_cb, period_ms, (void *)(uintptr_t)s.gen);
    lv_timer_set_repeat_count(s.watchdog, 1);   // сработает один раз и удалится сам
}

static void finalize(uint32_t gen)
{
    if (!s.busy || gen != s.gen) return;        // устаревший вызов

    if (s.watchdog) {
        lv_timer_delete(s.watchdog);
        s.watchdog = NULL;
    }

    // Не трогаем старые экраны, пока целевой не стал активным: удаление
    // экрана посреди анимации ломает отрисовку.
    if (s.to && lv_screen_active() != s.to && s.retries < NAV_WATCHDOG_MAX_RETRY) {
        s.retries++;
        ESP_LOGW(TAG, "target screen not active yet, retry %u", (unsigned)s.retries);
        arm_watchdog(NAV_WATCHDOG_RETRY_MS);
        return;
    }

    // Наш колбэк остаётся только на экране, который выжил (push / pop->prev).
    if (s.from && !is_doomed(s.from)) {
        lv_obj_remove_event_cb_with_user_data(s.from, on_from_unloaded, (void *)(uintptr_t)gen);
    }

    // Порядок: сначала destroy() всех обречённых (он снимает подписки/таймеры
    // и делает lv_obj_del), и только потом переход считается законченным.
    for (size_t i = 0; i < s.doomed_n; i++) {
        nav_entry_t e = s.doomed[i];
        if (e.desc && e.desc->destroy) {
            ESP_LOGI(TAG, "destroy '%s'", e.desc->id);
            e.desc->destroy(e.scr);
        }
    }
    s.doomed_n = 0;
    s.from = NULL;
    s.to = NULL;
    s.busy = false;

    if (s.has_queued) {
        nav_req_t req = s.queued;
        s.has_queued = false;
        execute(&req);
    }
}

static void finalize_async_cb(void *user_data)
{
    finalize((uint32_t)(uintptr_t)user_data);
}

// LV_EVENT_SCREEN_UNLOADED приходит из внутренностей lv_screen_load_anim()
// (иногда синхронно, иногда в конце анимации), поэтому саму работу переносим
// на следующий тик LVGL: нельзя удалять экран из его же event-колбэка.
static void on_from_unloaded(lv_event_t *e)
{
    lv_async_call(finalize_async_cb, lv_event_get_user_data(e));
}

static void watchdog_cb(lv_timer_t *t)
{
    uint32_t gen = (uint32_t)(uintptr_t)lv_timer_get_user_data(t);
    if (s.watchdog == t) s.watchdog = NULL;     // таймер удалится сам (repeat_count = 1)
    if (s.busy && gen == s.gen) {
        ESP_LOGW(TAG, "transition watchdog fired, finalizing");
        finalize(gen);
    }
}

static void begin_transition(lv_obj_t *from, lv_obj_t *to, lv_screen_load_anim_t anim, uint32_t ms)
{
    if (anim == LV_SCR_LOAD_ANIM_NONE) ms = 0;

    s.busy = true;
    s.gen++;
    s.from = from;
    s.to = to;
    s.retries = 0;

    // Колбэк и watchdog - ДО lv_screen_load_anim(): при ms == 0 событие
    // UNLOADED прилетает синхронно изнутри этого вызова.
    lv_obj_add_event_cb(from, on_from_unloaded, LV_EVENT_SCREEN_UNLOADED, (void *)(uintptr_t)s.gen);
    arm_watchdog(ms + NAV_WATCHDOG_MARGIN_MS);

    lv_screen_load_anim(to, anim, ms, 0, false);   // auto_del = false: удаляем сами, через destroy()
}

// ---------------------------------------------------------------------------
// операции
// ---------------------------------------------------------------------------
static void bootstrap_home(const screen_desc_t *d)
{
    lv_obj_t *scr = d->create();
    if (!scr) {
        ESP_LOGE(TAG, "create('%s') returned NULL", d->id);
        return;
    }
    s.stack[0] = (nav_entry_t){ .desc = d, .scr = scr };
    s.depth = 1;
    lv_screen_load(scr);
    ESP_LOGI(TAG, "home is up (resident)");
}

static void do_home(const nav_req_t *r)
{
    if (s.depth <= 1) return;                   // уже на home

    lv_obj_t *from = top()->scr;
    lv_obj_t *to = s.stack[0].scr;

    s.doomed_n = 0;
    for (size_t i = 1; i < s.depth; i++) s.doomed[s.doomed_n++] = s.stack[i];
    s.depth = 1;

    begin_transition(from, to, r->anim, r->ms);
}

static void do_push(const nav_req_t *r)
{
    const screen_desc_t *t = r->target;

    if (is_home(t)) {
        if (s.depth == 0) bootstrap_home(t);
        else do_home(r);
        return;
    }
    if (s.depth == 0) {
        ESP_LOGE(TAG, "push('%s') before home: call nav_push(\"home\") first", t->id);
        return;
    }
    if (s.depth >= NAV_STACK_MAX) {
        ESP_LOGE(TAG, "stack full, push('%s') dropped", t->id);
        return;
    }
    if (in_stack(t)) {
        ESP_LOGW(TAG, "'%s' already in stack, push ignored", t->id);
        return;
    }

    lv_obj_t *scr = t->create();
    if (!scr) {
        ESP_LOGE(TAG, "create('%s') returned NULL", t->id);
        return;
    }

    lv_obj_t *from = top()->scr;
    s.stack[s.depth++] = (nav_entry_t){ .desc = t, .scr = scr };
    s.doomed_n = 0;                             // push ничего не уничтожает
    begin_transition(from, scr, r->anim, r->ms);
}

static void do_pop(const nav_req_t *r)
{
    if (s.depth < 2) return;                    // на home pop ничего не делает

    nav_entry_t gone = s.stack[s.depth - 1];
    nav_entry_t back = s.stack[s.depth - 2];
    s.depth--;

    s.doomed[0] = gone;
    s.doomed_n = 1;
    begin_transition(gone.scr, back.scr, r->anim, r->ms);
}

static void do_replace(const nav_req_t *r)
{
    const screen_desc_t *t = r->target;

    if (is_home(t)) {
        do_home(r);
        return;
    }
    if (s.depth == 0) {
        ESP_LOGE(TAG, "replace('%s') before home", t->id);
        return;
    }
    nav_entry_t *cur = top();
    if (cur->desc == t) return;                 // уже здесь
    if (in_stack(t)) {
        ESP_LOGW(TAG, "'%s' already in stack, replace ignored", t->id);
        return;
    }
    if (is_home(cur->desc)) {                   // home не уничтожаем: ведём себя как push
        do_push(r);
        return;
    }

    lv_obj_t *scr = t->create();
    if (!scr) {
        ESP_LOGE(TAG, "create('%s') returned NULL", t->id);
        return;
    }

    nav_entry_t old = *cur;
    *cur = (nav_entry_t){ .desc = t, .scr = scr };
    s.doomed[0] = old;
    s.doomed_n = 1;
    begin_transition(old.scr, scr, r->anim, r->ms);
}

static void execute(const nav_req_t *r)
{
    switch (r->op) {
    case NAV_OP_PUSH:    do_push(r);    break;
    case NAV_OP_POP:     do_pop(r);     break;
    case NAV_OP_REPLACE: do_replace(r); break;
    case NAV_OP_HOME:    do_home(r);    break;
    }
}

static void submit(const nav_req_t *r)
{
    if (s.busy) {
        s.queued = *r;                          // последний запрос вытесняет предыдущий
        s.has_queued = true;
        return;
    }
    execute(r);
}

// ---------------------------------------------------------------------------
// публичный API
// ---------------------------------------------------------------------------
void nav_init(const screen_desc_t *registry, size_t count)
{
    memset(&s, 0, sizeof(s));
    s.registry = registry;
    s.count = count;
}

void nav_push_ex(const char *screen_id, lv_screen_load_anim_t anim, uint32_t time_ms)
{
    const screen_desc_t *d = find_desc(screen_id);
    if (!d) { ESP_LOGE(TAG, "push: unknown screen '%s'", screen_id ? screen_id : "(null)"); return; }
    nav_req_t r = { .op = NAV_OP_PUSH, .target = d, .anim = anim, .ms = time_ms };
    submit(&r);
}

void nav_pop_ex(lv_screen_load_anim_t anim, uint32_t time_ms)
{
    nav_req_t r = { .op = NAV_OP_POP, .anim = anim, .ms = time_ms };
    submit(&r);
}

void nav_replace_ex(const char *screen_id, lv_screen_load_anim_t anim, uint32_t time_ms)
{
    const screen_desc_t *d = find_desc(screen_id);
    if (!d) { ESP_LOGE(TAG, "replace: unknown screen '%s'", screen_id ? screen_id : "(null)"); return; }
    nav_req_t r = { .op = NAV_OP_REPLACE, .target = d, .anim = anim, .ms = time_ms };
    submit(&r);
}

void nav_back_to_home_ex(lv_screen_load_anim_t anim, uint32_t time_ms)
{
    nav_req_t r = { .op = NAV_OP_HOME, .anim = anim, .ms = time_ms };
    submit(&r);
}

void nav_push(const char *screen_id)    { nav_push_ex(screen_id, LV_SCR_LOAD_ANIM_NONE, 0); }
void nav_pop(void)                      { nav_pop_ex(LV_SCR_LOAD_ANIM_NONE, 0); }
void nav_replace(const char *screen_id) { nav_replace_ex(screen_id, LV_SCR_LOAD_ANIM_NONE, 0); }
void nav_back_to_home(void)             { nav_back_to_home_ex(LV_SCR_LOAD_ANIM_NONE, 0); }

const char *nav_current_id(void)
{
    nav_entry_t *t = top();
    return t ? t->desc->id : NULL;
}
