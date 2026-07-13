#include "screen_calc_logic.h"
#include "ui.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
#include <ctype.h>

static const char *TAG = "SCREEN_CALC";

#define EXPR_BUF_SIZE 48
static char s_expr[EXPR_BUF_SIZE] = "";

// =========================================================
// ВЫЧИСЛЕНИЕ ВЫРАЖЕНИЯ (+ - * / ^, без скобок, унарный минус)
// =========================================================
static int precedence(char op)
{
    switch (op) {
    case '^': return 3;
    case '*': case '/': return 2;
    case '+': case '-': return 1;
    default: return 0;
    }
}

static double apply_op(double a, double b, char op)
{
    switch (op) {
    case '+': return a + b;
    case '-': return a - b;
    case '*': return a * b;
    case '/': return (b != 0) ? a / b : NAN;
    case '^': return pow(a, b);
    }
    return 0;
}

static bool eval_expr(const char *expr, double *out)
{
    double vals[32]; int vtop = 0;
    char ops[32]; int otop = 0;
    const char *p = expr;
    bool expect_operand = true;

    while (*p) {
        if (*p == ' ') { p++; continue; }

        if ((*p >= '0' && *p <= '9') || *p == '.' || (expect_operand && *p == '-')) {
            char *end;
            double v = strtod(p, &end);
            if (end == p) return false;
            if (vtop >= 32) return false;
            vals[vtop++] = v;
            p = end;
            expect_operand = false;
            continue;
        }

        if (*p == '+' || *p == '-' || *p == '*' || *p == '/' || *p == '^') {
            char op = *p;
            while (otop > 0 && precedence(ops[otop - 1]) >= precedence(op)) {
                if (vtop < 2) return false;
                double b = vals[--vtop], a = vals[--vtop];
                vals[vtop++] = apply_op(a, b, ops[--otop]);
            }
            if (otop >= 32) return false;
            ops[otop++] = op;
            p++;
            expect_operand = true;
            continue;
        }

        return false; // неизвестный символ
    }

    while (otop > 0) {
        if (vtop < 2) return false;
        double b = vals[--vtop], a = vals[--vtop];
        vals[vtop++] = apply_op(a, b, ops[--otop]);
    }

    if (vtop != 1) return false;
    *out = vals[0];
    return isfinite(*out);
}

static void format_number(double v, char *buf, size_t buf_sz)
{
    if (v == (long long)v && fabs(v) < 1e15) {
        snprintf(buf, buf_sz, "%lld", (long long)v);
    } else {
        snprintf(buf, buf_sz, "%.6g", v);
    }
}

// =========================================================
// ОТОБРАЖЕНИЕ
// =========================================================
static void refresh_display(void)
{
    lv_label_set_text(ui_Label19, s_expr[0] ? s_expr : "0");

    double result;
    if (s_expr[0] && eval_expr(s_expr, &result)) {
        char buf[32];
        format_number(result, buf, sizeof(buf));
        lv_label_set_text(ui_Label20, buf);
    } else {
        lv_label_set_text(ui_Label20, "");
    }
}

static void append_str(const char *s)
{
    size_t len = strlen(s_expr);
    size_t add_len = strlen(s);
    if (len + add_len >= EXPR_BUF_SIZE - 1) return;
    strcat(s_expr, s);
    refresh_display();
}

static void do_backspace(void)
{
    size_t len = strlen(s_expr);
    if (len > 0) s_expr[len - 1] = '\0';
    refresh_display();
}

static void do_toggle_sign(void)
{
    size_t len = strlen(s_expr);
    if (len == 0) return;

    // Находим начало последнего числа (идём назад, пока цифры/точка)
    size_t num_start = len;
    while (num_start > 0 && (isdigit((unsigned char)s_expr[num_start - 1]) || s_expr[num_start - 1] == '.')) {
        num_start--;
    }
    if (num_start == len) return; // числа нет - нечего инвертировать

    bool has_unary_minus = (num_start > 0 && s_expr[num_start - 1] == '-' &&
                            (num_start == 1 || strchr("+-*/^", s_expr[num_start - 2]) != NULL));

    if (has_unary_minus) {
        // убираем минус
        memmove(&s_expr[num_start - 1], &s_expr[num_start], len - num_start + 1);
    } else {
        // вставляем минус перед числом
        if (len + 1 >= EXPR_BUF_SIZE - 1) return;
        memmove(&s_expr[num_start + 1], &s_expr[num_start], len - num_start + 1);
        s_expr[num_start] = '-';
    }
    refresh_display();
}

static void do_equals(void)
{
    double result;
    if (eval_expr(s_expr, &result)) {
        char buf[32];
        format_number(result, buf, sizeof(buf));
        strncpy(s_expr, buf, EXPR_BUF_SIZE - 1);
        s_expr[EXPR_BUF_SIZE - 1] = '\0';
    }
    refresh_display();
}

static void do_sqrt(void)
{
    double result;
    if (!eval_expr(s_expr, &result) || result < 0) {
        lv_label_set_text(ui_Label20, "Error");
        return;
    }
    double sq = sqrt(result);
    char buf[32];
    format_number(sq, buf, sizeof(buf));
    strncpy(s_expr, buf, EXPR_BUF_SIZE - 1);
    s_expr[EXPR_BUF_SIZE - 1] = '\0';
    refresh_display();
}

// =========================================================
// ОБРАБОТЧИК КНОПОК
// =========================================================
static void btn_cb(lv_event_t *e)
{
    const char *action = (const char *)lv_event_get_user_data(e);
    if (strcmp(action, "del") == 0) do_backspace();
    else if (strcmp(action, "=") == 0) do_equals();
    else if (strcmp(action, "sqrt") == 0) do_sqrt();
    else if (strcmp(action, "+/-") == 0) do_toggle_sign();
    else append_str(action);
}

// =========================================================
// ОФОРМЛЕНИЕ + ПРИВЯЗКА
// =========================================================
static void style_button(lv_obj_t *btn, uint32_t bg_color, uint32_t text_color)
{
    lv_obj_set_style_bg_color(btn, lv_color_hex(bg_color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(btn, 255, LV_PART_MAIN);
    lv_obj_set_style_radius(btn, 14, LV_PART_MAIN);
    if (lv_obj_get_child_count(btn) > 0) {
        lv_obj_t *lbl = lv_obj_get_child(btn, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(text_color), LV_PART_MAIN);
    }
}

static void bind_button(lv_obj_t *btn, const char *action, bool is_op, bool is_equals)
{
    lv_obj_add_event_cb(btn, btn_cb, LV_EVENT_CLICKED, (void *)action);
    if (is_equals) {
        style_button(btn, UI_COLOR_ACCENT, 0x000000);
    } else if (is_op) {
        style_button(btn, UI_COLOR_SURFACE_ALT, UI_COLOR_ACCENT);
    } else {
        style_button(btn, UI_COLOR_SURFACE, UI_COLOR_TEXT_PRIMARY);
    }
}

void screen_calc_logic_init(void)
{
    UI_APPLY_SCREEN_BG(ui_ScreenCalc);
    lv_obj_set_style_text_color(ui_Label19, lv_color_hex(UI_COLOR_TEXT_PRIMARY), LV_PART_MAIN);
    lv_obj_set_style_text_color(ui_Label20, lv_color_hex(UI_COLOR_TEXT_SECONDARY), LV_PART_MAIN);
    lv_label_set_text(ui_Label22, "+/-"); // в SquareLine было "?" - задаём реальный смысл кнопки

    bind_button(ui_Button1, "del", true, false);
    bind_button(ui_Button2, "/", true, false);
    bind_button(ui_Button3, "*", true, false);
    bind_button(ui_Button4, "+", true, false);
    bind_button(ui_Button5, "7", false, false);
    bind_button(ui_Button6, "8", false, false);
    bind_button(ui_Button7, "9", false, false);
    bind_button(ui_Button8, "-", true, false);
    bind_button(ui_Button9, "4", false, false);
    bind_button(ui_Button10, "5", false, false);
    bind_button(ui_Button11, "6", false, false);
    bind_button(ui_Button12, "^", true, false);
    bind_button(ui_Button13, "1", false, false);
    bind_button(ui_Button14, "2", false, false);
    bind_button(ui_Button15, "3", false, false);
    bind_button(ui_Button16, "sqrt", true, false);
    bind_button(ui_Button17, "+/-", true, false);
    bind_button(ui_Button18, "0", false, false);
    bind_button(ui_Button19, ".", false, false);
    bind_button(ui_Button20, "=", false, true);

    s_expr[0] = '\0';
    refresh_display();

    ESP_LOGI(TAG, "Calculator screen logic initialized");
}

void ui_ScreenCalc_full_init(void)
{
    ui_ScreenCalc_screen_init();
    screen_calc_logic_init();
}