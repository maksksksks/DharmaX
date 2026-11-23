#pragma once

/**
 * @brief Разбирает JSON-пакет от телефона и рассылает события в APP_EVENTS.
 * @param json нуль-терминированная строка
 */
void ble_notify_parse_json(const char *json);