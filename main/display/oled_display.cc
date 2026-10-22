#include "oled_display.h"
#include "assets/lang_config.h"
#include "lvgl_font.h"
#include "lvgl_theme.h"
#include "settings.h"

#include <algorithm>
#include <string>

#include <esp_err.h>
#include <esp_log.h>
#include <esp_lvgl_port.h>
#include <material_symbols.h>
#include <noto_emoji.h>

#define TAG "OledDisplay"

LV_FONT_DECLARE(BUILTIN_TEXT_FONT);
LV_FONT_DECLARE(BUILTIN_ICON_FONT);
LV_FONT_DECLARE(font_material_symbols_30_1);
LV_FONT_DECLARE(font_noto_emoji_30_1);

OledDisplay::OledDisplay(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_handle_t panel,
                         int width, int height, bool mirror_x, bool mirror_y)
    : panel_io_(panel_io), panel_(panel) {
    width_ = width;
    height_ = height;

    auto text_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_TEXT_FONT);
    auto icon_font = std::make_shared<LvglBuiltInFont>(&BUILTIN_ICON_FONT);
    auto large_icon_font = std::make_shared<LvglBuiltInFont>(&font_material_symbols_30_1);
    auto emoji_font = std::make_shared<LvglBuiltInFont>(&font_noto_emoji_30_1);

    auto dark_theme = new LvglTheme("dark");
    dark_theme->set_text_font(text_font);
    dark_theme->set_icon_font(icon_font);
    dark_theme->set_large_icon_font(large_icon_font);
    dark_theme->set_emoji_font(emoji_font);

    auto& theme_manager = LvglThemeManager::GetInstance();
    theme_manager.RegisterTheme("dark", dark_theme);
    current_theme_ = dark_theme;

    ESP_LOGI(TAG, "Initialize LVGL");
    lvgl_port_cfg_t port_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    port_cfg.task_priority = 1;
    port_cfg.task_stack = 6144;
#if CONFIG_SOC_CPU_CORES_NUM > 1
    port_cfg.task_affinity = 1;
#endif
    lvgl_port_init(&port_cfg);

    ESP_LOGI(TAG, "Adding OLED display");
    const lvgl_port_display_cfg_t display_cfg = {
        .io_handle = panel_io_,
        .panel_handle = panel_,
        .control_handle = nullptr,
        .buffer_size = static_cast<uint32_t>(width_ * height_),
        .double_buffer = false,
        .trans_size = 0,
        .hres = static_cast<uint32_t>(width_),
        .vres = static_cast<uint32_t>(height_),
        .monochrome = true,
        .rotation =
            {
                .swap_xy = false,
                .mirror_x = mirror_x,
                .mirror_y = mirror_y,
            },
        .flags =
            {
                .buff_dma = 1,
                .buff_spiram = 0,
                .sw_rotate = 0,
                .full_refresh = 0,
                .direct_mode = 0,
            },
    };

    display_ = lvgl_port_add_disp(&display_cfg);
    if (display_ == nullptr) {
        ESP_LOGE(TAG, "Failed to add display");
        return;
    }

    // Note: SetupUI() should be called by Application::Initialize(), not in constructor
    // to ensure lvgl objects are created after the display is fully initialized.
}

void OledDisplay::SetupUI() {
    // Prevent duplicate calls - if already called, return early
    if (setup_ui_called_) {
        ESP_LOGW(TAG, "SetupUI() called multiple times, skipping duplicate call");
        return;
    }

    Display::SetupUI();  // Mark SetupUI as called
    if (height_ == 64) {
        SetupUI_128x64();
    } else {
        SetupUI_128x32();
    }
}

OledDisplay::~OledDisplay() {
    if (content_ != nullptr) {
        lv_obj_del(content_);
    }

    bool is_128x64_layout = (top_bar_ != nullptr);
    if (status_bar_ != nullptr && is_128x64_layout) {
        status_label_ = nullptr;
        notification_label_ = nullptr;
        lv_obj_del(status_bar_);
    }
    if (top_bar_ != nullptr) {
        network_label_ = nullptr;
        mute_label_ = nullptr;
        battery_label_ = nullptr;
        lv_obj_del(top_bar_);
    }
    if (side_bar_ != nullptr) {
        if (!is_128x64_layout) {
            status_label_ = nullptr;
            notification_label_ = nullptr;
            network_label_ = nullptr;
            mute_label_ = nullptr;
            battery_label_ = nullptr;
        }
        lv_obj_del(side_bar_);
    }
    if (container_ != nullptr) {
        lv_obj_del(container_);
    }

    if (panel_ != nullptr) {
        esp_lcd_panel_del(panel_);
    }
    if (panel_io_ != nullptr) {
        esp_lcd_panel_io_del(panel_io_);
    }
    lvgl_port_deinit();
}

bool OledDisplay::Lock(int timeout_ms) { return lvgl_port_lock(timeout_ms); }

void OledDisplay::Unlock() { lvgl_port_unlock(); }

void OledDisplay::SetChatMessage(const char* role, const char* content) {
    // Disabled subtitle to eliminate continuous I2C redraws and prevent screen stutter
    return;
}

void OledDisplay::RemoveChatMessage(const char* role, const char* content) {
    return;
}

void OledDisplay::SetupUI_128x64() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);

    /* Dedicated Full-Screen (128x64) Cozmo Robot Eyes */
    content_ = lv_obj_create(screen);
    lv_obj_set_size(content_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_scrollbar_mode(content_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_radius(content_, 0, 0);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_bg_color(content_, lv_color_black(), 0);
    lv_obj_set_flex_flow(content_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_flex_main_place(content_, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_flex_cross_place(content_, LV_FLEX_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_column(content_, 14, 0);

    // Left Eye: 42x36 rounded dome
    left_eye_ = lv_obj_create(content_);
    lv_obj_set_size(left_eye_, 42, 36);
    lv_obj_set_style_radius(left_eye_, 16, 0);
    lv_obj_set_style_bg_color(left_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(left_eye_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(left_eye_, 0, 0);
    lv_obj_set_style_pad_all(left_eye_, 0, 0);
    lv_obj_set_scrollbar_mode(left_eye_, LV_SCROLLBAR_MODE_OFF);

    // Left Pupil: curved black cutout at bottom creating smiling arch
    left_pupil_ = lv_obj_create(left_eye_);
    lv_obj_set_size(left_pupil_, 34, 16);
    lv_obj_set_style_radius(left_pupil_, 10, 0);
    lv_obj_set_style_bg_color(left_pupil_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(left_pupil_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(left_pupil_, 0, 0);
    lv_obj_set_style_pad_all(left_pupil_, 0, 0);
    lv_obj_align(left_pupil_, LV_ALIGN_BOTTOM_MID, 0, 5);

    // Right Eye: 42x36 rounded dome
    right_eye_ = lv_obj_create(content_);
    lv_obj_set_size(right_eye_, 42, 36);
    lv_obj_set_style_radius(right_eye_, 16, 0);
    lv_obj_set_style_bg_color(right_eye_, lv_color_white(), 0);
    lv_obj_set_style_bg_opa(right_eye_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(right_eye_, 0, 0);
    lv_obj_set_style_pad_all(right_eye_, 0, 0);
    lv_obj_set_scrollbar_mode(right_eye_, LV_SCROLLBAR_MODE_OFF);

    // Right Pupil: curved black cutout at bottom creating smiling arch
    right_pupil_ = lv_obj_create(right_eye_);
    lv_obj_set_size(right_pupil_, 34, 16);
    lv_obj_set_style_radius(right_pupil_, 10, 0);
    lv_obj_set_style_bg_color(right_pupil_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(right_pupil_, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(right_pupil_, 0, 0);
    lv_obj_set_style_pad_all(right_pupil_, 0, 0);
    lv_obj_align(right_pupil_, LV_ALIGN_BOTTOM_MID, 0, 5);

    container_ = nullptr;
    top_bar_ = nullptr;
    status_bar_ = nullptr;
    status_label_ = nullptr;
    notification_label_ = nullptr;
    network_label_ = nullptr;
    mute_label_ = nullptr;
    battery_label_ = nullptr;
    emotion_label_ = nullptr;
    content_left_ = nullptr;
    content_right_ = nullptr;
    chat_message_label_ = nullptr;

    low_battery_popup_ = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(low_battery_popup_, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_size(low_battery_popup_, LV_HOR_RES * 0.9, text_font->line_height * 2);
    lv_obj_align(low_battery_popup_, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(low_battery_popup_, lv_color_black(), 0);
    lv_obj_set_style_radius(low_battery_popup_, 10, 0);
    low_battery_label_ = lv_label_create(low_battery_popup_);
    lv_label_set_text(low_battery_label_, Lang::Strings::BATTERY_NEED_CHARGE);
    lv_obj_set_style_text_color(low_battery_label_, lv_color_white(), 0);
    lv_obj_center(low_battery_label_);
    lv_obj_add_flag(low_battery_popup_, LV_OBJ_FLAG_HIDDEN);
}

void OledDisplay::SetupUI_128x32() {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    auto text_font = lvgl_theme->text_font()->font();
    auto icon_font = lvgl_theme->icon_font()->font();
    auto large_icon_font = lvgl_theme->large_icon_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);

    /* Container */
    container_ = lv_obj_create(screen);
    lv_obj_set_size(container_, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_flex_flow(container_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(container_, 0, 0);
    lv_obj_set_style_border_width(container_, 0, 0);
    lv_obj_set_style_pad_column(container_, 0, 0);

    /* Emotion label on the left side */
    content_ = lv_obj_create(container_);
    lv_obj_set_size(content_, 32, 32);
    lv_obj_set_style_pad_all(content_, 0, 0);
    lv_obj_set_style_border_width(content_, 0, 0);
    lv_obj_set_style_radius(content_, 0, 0);

    emotion_label_ = lv_label_create(content_);
    lv_obj_set_style_text_font(emotion_label_, large_icon_font, 0);
    lv_label_set_text(emotion_label_, MATERIAL_SYMBOLS_ROBOT_2);
    lv_obj_center(emotion_label_);

    /* Right side */
    side_bar_ = lv_obj_create(container_);
    lv_obj_set_size(side_bar_, width_ - 32, 32);
    lv_obj_set_flex_flow(side_bar_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(side_bar_, 0, 0);
    lv_obj_set_style_border_width(side_bar_, 0, 0);
    lv_obj_set_style_radius(side_bar_, 0, 0);
    lv_obj_set_style_pad_row(side_bar_, 0, 0);

    /* Status bar */
    status_bar_ = lv_obj_create(side_bar_);
    lv_obj_set_size(status_bar_, width_ - 32, 16);
    lv_obj_set_style_radius(status_bar_, 0, 0);
    lv_obj_set_flex_flow(status_bar_, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(status_bar_, 0, 0);
    lv_obj_set_style_border_width(status_bar_, 0, 0);
    lv_obj_set_style_pad_column(status_bar_, 0, 0);

    status_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(status_label_, 1);
    lv_obj_set_style_pad_left(status_label_, 2, 0);
    lv_label_set_text(status_label_, Lang::Strings::INITIALIZING);

    notification_label_ = lv_label_create(status_bar_);
    lv_obj_set_flex_grow(notification_label_, 1);
    lv_obj_set_style_pad_left(notification_label_, 2, 0);
    lv_label_set_text(notification_label_, "");
    lv_obj_add_flag(notification_label_, LV_OBJ_FLAG_HIDDEN);

    mute_label_ = lv_label_create(status_bar_);
    lv_label_set_text(mute_label_, "");
    lv_obj_set_style_text_font(mute_label_, icon_font, 0);

    network_label_ = lv_label_create(status_bar_);
    lv_label_set_text(network_label_, "");
    lv_obj_set_style_text_font(network_label_, icon_font, 0);

    battery_label_ = lv_label_create(status_bar_);
    lv_label_set_text(battery_label_, "");
    lv_obj_set_style_text_font(battery_label_, icon_font, 0);

    chat_message_label_ = lv_label_create(side_bar_);
    lv_obj_set_size(chat_message_label_, width_ - 32, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_left(chat_message_label_, 2, 0);
    lv_label_set_long_mode(chat_message_label_, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(chat_message_label_, "");

    // Start scrolling subtitle after a delay
    static lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_delay(&a, 1000);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_obj_set_style_anim(chat_message_label_, &a, LV_PART_MAIN);
    lv_obj_set_style_anim_duration(chat_message_label_, lv_anim_speed_clamped(60, 300, 60000),
                                   LV_PART_MAIN);
}

void OledDisplay::SetStatus(const char* status) {
    DisplayLockGuard lock(this);
    if (left_eye_ == nullptr || right_eye_ == nullptr || status == nullptr) {
        return;
    }
    if (strstr(status, "Listening") || strstr(status, "nghe")) {
        // Eyes open wider in attention
        lv_obj_set_size(left_eye_, 42, 40);
        lv_obj_set_size(right_eye_, 42, 40);
        lv_obj_set_style_radius(left_eye_, 18, 0);
        lv_obj_set_style_radius(right_eye_, 18, 0);
        if (left_pupil_) lv_obj_clear_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (right_pupil_) lv_obj_clear_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
    } else if (strstr(status, "Speaking") || strstr(status, "nói")) {
        // Happy speaking expression
        lv_obj_set_size(left_eye_, 42, 28);
        lv_obj_set_size(right_eye_, 42, 28);
        lv_obj_set_style_radius(left_eye_, 14, 0);
        lv_obj_set_style_radius(right_eye_, 14, 0);
        if (left_pupil_) lv_obj_clear_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (right_pupil_) lv_obj_clear_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
    } else if (strstr(status, "Thinking") || strstr(status, "nghĩ")) {
        lv_obj_set_size(left_eye_, 42, 32);
        lv_obj_set_size(right_eye_, 42, 32);
        lv_obj_set_style_radius(left_eye_, 14, 0);
        lv_obj_set_style_radius(right_eye_, 14, 0);
        if (left_pupil_) lv_obj_clear_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (right_pupil_) lv_obj_clear_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
    } else {
        // Standby / normal (like in the photo)
        lv_obj_set_size(left_eye_, 42, 36);
        lv_obj_set_size(right_eye_, 42, 36);
        lv_obj_set_style_radius(left_eye_, 16, 0);
        lv_obj_set_style_radius(right_eye_, 16, 0);
        if (left_pupil_) lv_obj_clear_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
        if (right_pupil_) lv_obj_clear_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
    }
}

bool OledDisplay::SetEmotion(const char* emotion) {
    DisplayLockGuard lock(this);
    if (left_eye_ != nullptr && right_eye_ != nullptr) {
        if (emotion != nullptr && (strcmp(emotion, "blink") == 0 || strcmp(emotion, "sleepy") == 0)) {
            lv_obj_set_size(left_eye_, 42, 4);
            lv_obj_set_size(right_eye_, 42, 4);
            lv_obj_set_style_radius(left_eye_, 2, 0);
            lv_obj_set_style_radius(right_eye_, 2, 0);
            if (left_pupil_) lv_obj_add_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
            if (right_pupil_) lv_obj_add_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        } else if (emotion != nullptr && strcmp(emotion, "happy") == 0) {
            lv_obj_set_size(left_eye_, 42, 24);
            lv_obj_set_size(right_eye_, 42, 24);
            lv_obj_set_style_radius(left_eye_, 12, 0);
            lv_obj_set_style_radius(right_eye_, 12, 0);
            if (left_pupil_) lv_obj_clear_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
            if (right_pupil_) lv_obj_clear_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        } else if (emotion != nullptr && strcmp(emotion, "surprised") == 0) {
            lv_obj_set_size(left_eye_, 42, 42);
            lv_obj_set_size(right_eye_, 42, 42);
            lv_obj_set_style_radius(left_eye_, 18, 0);
            lv_obj_set_style_radius(right_eye_, 18, 0);
            if (left_pupil_) lv_obj_clear_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
            if (right_pupil_) lv_obj_clear_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        } else {
            // Neutral / standard eyes
            lv_obj_set_size(left_eye_, 42, 36);
            lv_obj_set_size(right_eye_, 42, 36);
            lv_obj_set_style_radius(left_eye_, 16, 0);
            lv_obj_set_style_radius(right_eye_, 16, 0);
            if (left_pupil_) lv_obj_clear_flag(left_pupil_, LV_OBJ_FLAG_HIDDEN);
            if (right_pupil_) lv_obj_clear_flag(right_pupil_, LV_OBJ_FLAG_HIDDEN);
        }
        return true;
    }
    if (emotion_label_ == nullptr) {
        return false;
    }
    auto lvgl_theme = static_cast<LvglTheme*>(current_theme_);
    const char* utf8 = noto_emoji_get_utf8(emotion);
    const lv_font_t* emotion_font = lvgl_theme->emoji_font()->font();
    if (utf8 == nullptr) {
        utf8 = material_symbols_get_utf8(emotion);
        emotion_font = lvgl_theme->large_icon_font()->font();
    }
    if (utf8 != nullptr) {
        lv_obj_set_style_text_font(emotion_label_, emotion_font, 0);
        lv_label_set_text(emotion_label_, utf8);
        return true;
    }
    // Neither font has a glyph for this name, so what is shown is the neutral placeholder and
    // not the emotion that was asked for.
    lv_obj_set_style_text_font(emotion_label_, lvgl_theme->emoji_font()->font(), 0);
    lv_label_set_text(emotion_label_, NOTO_EMOJI_NEUTRAL);
    return false;
}

void OledDisplay::SetTheme(Theme* theme) {
    DisplayLockGuard lock(this);

    auto lvgl_theme = static_cast<LvglTheme*>(theme);
    auto text_font = lvgl_theme->text_font()->font();

    auto screen = lv_screen_active();
    lv_obj_set_style_text_font(screen, text_font, 0);
}

void OledDisplay::SetPowerSaveMode(bool on) {
    if (panel_) {
        Settings settings("wifi", false);
        if (settings.GetBool("power_save_display_off", false)) {
            esp_lcd_panel_disp_on_off(panel_, !on);
        }
    }
    LvglDisplay::SetPowerSaveMode(on);
}
