#pragma once
#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

// Общий виджет "лента карточек со свайпом для удаления" - вынесен из
// старой логики экрана уведомлений, чтобы им могли пользоваться
// notifications / events / news без копирования кода.

typedef struct card_feed_s *card_feed_t;

// --- Жизненный цикл ленты, переживающей экран --------------------------------
// Экран с лентой (notify / events / news) уничтожается навигатором, а карточки
// нельзя терять: сервис ленты создаёт её "в стороне" и принимает события, пока
// экрана нет; экран при create() подключает ленту, при destroy() отключает.
card_feed_t card_feed_create_detached(uint32_t auto_delete_ms);  // лента во внутреннем скрытом контейнере
void card_feed_attach(card_feed_t feed, lv_obj_t *parent);       // перенести ленту на экран
void card_feed_detach(card_feed_t feed);                         // убрать с экрана обратно в скрытый контейнер
void card_feed_destroy(card_feed_t feed);                        // полностью удалить ленту, карточки и их таймеры

// Создаёт вертикальный скроллируемый список карточек внутри parent
// (например, вкладки TabView или самого экрана).
// auto_delete_ms - через сколько мс карточка исчезнет сама (0 = никогда).
card_feed_t card_feed_create(lv_obj_t *parent, uint32_t auto_delete_ms);

typedef struct {
    const char *id;           // уникальный id карточки (для точечного удаления), может быть ""
    const char *grp;          // группа (для массового удаления), может быть ""
    const char *header_left;  // напр. имя приложения/отправителя
    const char *header_right; // напр. время "HH:MM"
    const char *title;        // необязательно, если "" - строка не показывается
    const char *body;         // основной текст карточки
} card_feed_item_t;

void card_feed_add(card_feed_t feed, const card_feed_item_t *item);
void card_feed_remove_by_id(card_feed_t feed, const char *id);
void card_feed_remove_by_group(card_feed_t feed, const char *grp);
void card_feed_clear(card_feed_t feed);

// Доступ к контейнеру-списку, если вызывающему коду нужно вручную
// задать ему размер/позицию (напр. лента занимает не весь экран, а
// только часть под заголовком).
lv_obj_t *card_feed_get_container(card_feed_t feed);

#ifdef __cplusplus
} /*extern "C"*/
#endif
