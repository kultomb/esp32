#include "emo_display.h"
#include <esp_log.h>
#include <esp_random.h>
#include <cmath>
#include <algorithm>

static const char* TAG = "EmoDisplay";

static constexpr uint32_t kTickIntervalMs = 40; // 25 FPS
static constexpr float kLerpSpeed = 0.22f;       // Hệ số mượt mà khi đổi dáng mắt

EmoDisplay::EmoDisplay(esp_lcd_panel_io_handle_t io_handle,
                       esp_lcd_panel_handle_t panel_handle,
                       int width, int height,
                       bool mirror_x, bool mirror_y)
    : OledDisplay(io_handle, panel_handle, width, height, mirror_x, mirror_y) {
    // Tỷ lệ cho màn hình (mặc định chuẩn 128x64)
    float scale = std::min(width_ / 128.0f, height_ / 64.0f);
    base_w_ = static_cast<int>(40 * scale);
    base_h_ = static_cast<int>(36 * scale);
    base_radius_ = static_cast<int>(12 * scale);
    eye_gap_ = static_cast<int>(14 * scale);

    // Căn giữa màn hình hoàn hảo
    int total_w = base_w_ * 2 + eye_gap_;
    cur_x_l_ = tgt_x_l_ = (width_ - total_w) / 2.0f;
    cur_x_r_ = tgt_x_r_ = cur_x_l_ + base_w_ + eye_gap_;
    center_y_ = (height_ - base_h_) / 2.0f;
    cur_y_l_ = tgt_y_l_ = center_y_;
    cur_y_r_ = tgt_y_r_ = center_y_;
    cur_w_l_ = tgt_w_l_ = cur_w_r_ = tgt_w_r_ = base_w_;
    cur_h_l_ = tgt_h_l_ = cur_h_r_ = tgt_h_r_ = base_h_;
}

EmoDisplay::~EmoDisplay() {
    if (anim_timer_) {
        lv_timer_del(anim_timer_);
        anim_timer_ = nullptr;
    }
}

void EmoDisplay::SetupUI() {
    Display::SetupUI();
    BuildFace();
    ScheduleNextBlink();
    ScheduleNextQuirk();
    ApplyState(EmoState::kIdle);

    anim_timer_ = lv_timer_create(OnAnimTick, kTickIntervalMs, this);
    ESP_LOGI(TAG, "EmoDisplay initialized: 2 large EMO eyes on %dx%d OLED", width_, height_);
}

void EmoDisplay::BuildFace() {
    DisplayLockGuard lock(this);

    auto screen = lv_screen_active();
    lv_obj_set_style_bg_color(screen, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    face_root_ = lv_obj_create(screen);
    lv_obj_remove_style_all(face_root_);
    lv_obj_set_size(face_root_, width_, height_);
    lv_obj_center(face_root_);
    lv_obj_set_style_bg_color(face_root_, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(face_root_, LV_OPA_COVER, 0);
    lv_obj_clear_flag(face_root_, LV_OBJ_FLAG_SCROLLABLE);

    auto make_eye = [&]() {
        lv_obj_t* eye = lv_obj_create(face_root_);
        lv_obj_remove_style_all(eye);
        lv_obj_set_size(eye, base_w_, base_h_);
        lv_obj_set_style_bg_color(eye, lv_color_white(), 0);
        lv_obj_set_style_bg_opa(eye, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(eye, base_radius_, 0);
        lv_obj_clear_flag(eye, LV_OBJ_FLAG_SCROLLABLE);
        return eye;
    };

    left_eye_  = make_eye();
    right_eye_ = make_eye();

    // Mí dưới che tạo mắt cười (Happy / Laughing)
    auto make_cut = [&](lv_obj_t* parent, lv_align_t align) {
        lv_obj_t* cut = lv_obj_create(parent);
        lv_obj_remove_style_all(cut);
        lv_obj_set_size(cut, base_w_ + 4, 0);
        lv_obj_set_style_bg_color(cut, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(cut, LV_OPA_COVER, 0);
        lv_obj_set_style_radius(cut, 8, 0);
        lv_obj_align(cut, align, 0, 0);
        lv_obj_clear_flag(cut, LV_OBJ_FLAG_SCROLLABLE);
        return cut;
    };

    left_cut_bottom_  = make_cut(left_eye_,  LV_ALIGN_BOTTOM_MID);
    right_cut_bottom_ = make_cut(right_eye_, LV_ALIGN_BOTTOM_MID);
    left_cut_top_     = make_cut(left_eye_,  LV_ALIGN_TOP_MID);
    right_cut_top_    = make_cut(right_eye_, LV_ALIGN_TOP_MID);
}

void EmoDisplay::ScheduleNextBlink() {
    uint32_t delta = (2200 + (esp_random() % 3000)) / kTickIntervalMs; // 2.2s - 5.2s
    next_blink_tick_ = tick_count_ + delta;
    is_blinking_ = false;
}

void EmoDisplay::ScheduleNextQuirk() {
    uint32_t delta = (4500 + (esp_random() % 4500)) / kTickIntervalMs; // 4.5s - 9s
    next_quirk_tick_ = tick_count_ + delta;
    current_quirk_ = EmoQuirk::kNone;
}

void EmoDisplay::ApplyState(EmoState state) {
    current_state_ = state;
    tgt_cut_bottom_ = 0.0f;
    tgt_cut_top_ = 0.0f;

    int total_w = base_w_ * 2 + eye_gap_;
    float base_xl = (width_ - total_w) / 2.0f;
    float base_xr = base_xl + base_w_ + eye_gap_;

    switch (state) {
        case EmoState::kListening:
            // LẮNG NGHE: Mắt mở to, hơi ngước lên và nghiêng tò mò (cún cưng nghiêng đầu)
            tgt_w_l_ = tgt_w_r_ = base_w_ + 2;
            tgt_h_l_ = base_h_ + 2;
            tgt_h_r_ = base_h_ + 1;
            tgt_y_l_ = center_y_ - 3.0f; // Trái cao hơn
            tgt_y_r_ = center_y_ - 0.5f;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            break;

        case EmoState::kSpeaking:
            // ĐANG NÓI: Mắt tươi tỉnh, chuyển động nhấp nhô sẽ do UpdateAnimation() kích hoạt
            tgt_w_l_ = tgt_w_r_ = base_w_;
            tgt_h_l_ = tgt_h_r_ = base_h_;
            tgt_y_l_ = tgt_y_r_ = center_y_;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            break;

        case EmoState::kThinking:
            // SUY NGHĨ: Mắt hơi thu hẹp, liếc nhìn góc trên
            tgt_w_l_ = tgt_w_r_ = base_w_ - 2;
            tgt_h_l_ = tgt_h_r_ = base_h_ - 6;
            tgt_y_l_ = tgt_y_r_ = center_y_ - 4.0f;
            tgt_x_l_ = base_xl + 6.0f;
            tgt_x_r_ = base_xr + 6.0f;
            break;

        case EmoState::kHappy:
        case EmoState::kLaughing:
            // VUI VẺ: Mắt nheo cười hình vầng trăng khuyết ^_^
            tgt_w_l_ = tgt_w_r_ = base_w_ + 2;
            tgt_h_l_ = tgt_h_r_ = base_h_ - 4;
            tgt_y_l_ = tgt_y_r_ = center_y_ - 2.0f;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            tgt_cut_bottom_ = (state == EmoState::kLaughing) ? 20.0f : 16.0f;
            break;

        case EmoState::kSad:
        case EmoState::kCrying:
            // BUỒN: Mắt cụp xuống dưới, hẹp lại
            tgt_w_l_ = tgt_w_r_ = base_w_;
            tgt_h_l_ = tgt_h_r_ = base_h_ - 10;
            tgt_y_l_ = tgt_y_r_ = center_y_ + 4.0f;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            tgt_cut_top_ = 8.0f;
            break;

        case EmoState::kAngry:
            // GIẬN DỮ: Mí trên hạ xếch vào trong
            tgt_w_l_ = tgt_w_r_ = base_w_ + 1;
            tgt_h_l_ = tgt_h_r_ = base_h_ - 8;
            tgt_y_l_ = tgt_y_r_ = center_y_ + 2.0f;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            tgt_cut_top_ = 12.0f;
            break;

        case EmoState::kSurprised:
            // BẤT NGỜ: Mắt mở to hết cỡ (tròn xoe)
            tgt_w_l_ = tgt_w_r_ = base_w_ + 2;
            tgt_h_l_ = tgt_h_r_ = base_h_ + 4;
            tgt_y_l_ = tgt_y_r_ = center_y_ - 2.0f;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            break;

        case EmoState::kLoving:
            // YÊU THƯƠNG: Mắt to mềm mại, hơi nheo nhẹ hạnh phúc
            tgt_w_l_ = tgt_w_r_ = base_w_;
            tgt_h_l_ = tgt_h_r_ = base_h_ - 2;
            tgt_y_l_ = tgt_y_r_ = center_y_ - 1.0f;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            tgt_cut_bottom_ = 8.0f;
            break;

        case EmoState::kSleepy:
            // BUỒN NGỦ: Mắt híp mỏng, đặt thấp
            tgt_w_l_ = tgt_w_r_ = base_w_ - 2;
            tgt_h_l_ = tgt_h_r_ = 5.0f;
            tgt_y_l_ = tgt_y_r_ = center_y_ + 6.0f;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            break;

        case EmoState::kWinking:
            // NHÁY MẮT: Mắt trái nhắm, mắt phải mở to cười
            tgt_w_l_ = base_w_;
            tgt_h_l_ = 4.0f;
            tgt_y_l_ = center_y_ + 4.0f;
            tgt_w_r_ = base_w_ + 2;
            tgt_h_r_ = base_h_;
            tgt_y_r_ = center_y_;
            tgt_cut_bottom_ = 10.0f;
            break;

        case EmoState::kCool:
            // NGẦU: Hơi nheo một bên, nghiêng tự tin
            tgt_w_l_ = base_w_;
            tgt_h_l_ = base_h_ - 6;
            tgt_y_l_ = center_y_ - 1.0f;
            tgt_w_r_ = base_w_;
            tgt_h_r_ = base_h_ - 10;
            tgt_y_r_ = center_y_ + 1.0f;
            break;

        case EmoState::kConfused:
            // BỐI RỐI: Mắt lệch một to một nhỏ
            tgt_w_l_ = base_w_ + 2;
            tgt_h_l_ = base_h_;
            tgt_y_l_ = center_y_ - 2.0f;
            tgt_w_r_ = base_w_ - 4;
            tgt_h_r_ = base_h_ - 12;
            tgt_y_r_ = center_y_ + 2.0f;
            break;

        default: // kIdle / kCurious / kWarning...
            tgt_w_l_ = tgt_w_r_ = base_w_;
            tgt_h_l_ = tgt_h_r_ = base_h_;
            tgt_y_l_ = tgt_y_r_ = center_y_;
            tgt_x_l_ = base_xl;
            tgt_x_r_ = base_xr;
            break;
    }
}

void EmoDisplay::OnAnimTick(lv_timer_t* timer) {
    auto self = static_cast<EmoDisplay*>(lv_timer_get_user_data(timer));
    self->UpdateAnimation();
}

void EmoDisplay::UpdateAnimation() {
    tick_count_++;

    // 1. Xử lý Chớp Mắt (Auto-blink)
    if (!is_blinking_ && tick_count_ >= next_blink_tick_) {
        is_blinking_ = true;
        blink_end_tick_ = tick_count_ + 3; // Chớp trong ~120ms (3 ticks)
    } else if (is_blinking_ && tick_count_ >= blink_end_tick_) {
        is_blinking_ = false;
        ScheduleNextBlink();
    }

    // 2. Xử lý Quirk ngẫu nhiên khi Idle
    if (current_state_ == EmoState::kIdle) {
        if (current_quirk_ == EmoQuirk::kNone && tick_count_ >= next_quirk_tick_) {
            uint32_t r = esp_random() % 4;
            current_quirk_ = (r == 0) ? EmoQuirk::kLookLeft :
                             (r == 1) ? EmoQuirk::kLookRight :
                             (r == 2) ? EmoQuirk::kCuriousTilt : EmoQuirk::kLookUp;
            quirk_end_tick_ = tick_count_ + 30; // Giữ quirk trong ~1.2s
        } else if (current_quirk_ != EmoQuirk::kNone && tick_count_ >= quirk_end_tick_) {
            current_quirk_ = EmoQuirk::kNone;
            ScheduleNextQuirk();
        }
    }

    // 3. Tính toán độ dịch chuyển (Bobbing / Bounce)
    float bob_y = 0.0f;
    float quirk_x = 0.0f;
    float quirk_y = 0.0f;
    float tilt_diff = 0.0f;

    if (current_state_ == EmoState::kIdle) {
        // Nhịp thở bồng bềnh êm ái (chu kỳ ~3s)
        bob_y = std::sin(tick_count_ * 0.06f) * 2.2f;

        if (current_quirk_ == EmoQuirk::kLookLeft)  quirk_x = -7.0f;
        if (current_quirk_ == EmoQuirk::kLookRight) quirk_x =  7.0f;
        if (current_quirk_ == EmoQuirk::kLookUp)    quirk_y = -3.5f;
        if (current_quirk_ == EmoQuirk::kCuriousTilt) tilt_diff = 2.5f;

    } else if (current_state_ == EmoState::kSpeaking) {
        // ĐANG NÓI: Mắt nảy nhấp nhô theo nhịp nói (bouncy bobbing)
        bob_y = std::sin(tick_count_ * 0.32f) * 3.5f;

    } else if (current_state_ == EmoState::kListening) {
        // LẮNG NGHE: Nhấp nhô nhẹ nhàng tò mò
        bob_y = std::sin(tick_count_ * 0.08f) * 1.5f;
        tilt_diff = 2.0f;

    } else if (current_state_ == EmoState::kLoving) {
        // Nhịp tim đập nhẹ
        bob_y = std::sin(tick_count_ * 0.18f) * 1.8f;
    }

    // 4. Nội suy mượt mà (Lerp)
    float target_h_l = is_blinking_ ? 3.0f : tgt_h_l_;
    float target_h_r = is_blinking_ ? 3.0f : tgt_h_r_;

    cur_w_l_ += (tgt_w_l_ - cur_w_l_) * kLerpSpeed;
    cur_w_r_ += (tgt_w_r_ - cur_w_r_) * kLerpSpeed;
    cur_h_l_ += (target_h_l - cur_h_l_) * (is_blinking_ ? 0.6f : kLerpSpeed);
    cur_h_r_ += (target_h_r - cur_h_r_) * (is_blinking_ ? 0.6f : kLerpSpeed);

    float target_y_l = tgt_y_l_ + bob_y + quirk_y - tilt_diff;
    float target_y_r = tgt_y_r_ + bob_y + quirk_y + tilt_diff;
    cur_y_l_ += (target_y_l - cur_y_l_) * kLerpSpeed;
    cur_y_r_ += (target_y_r - cur_y_r_) * kLerpSpeed;

    cur_x_l_ += (tgt_x_l_ + quirk_x - cur_x_l_) * kLerpSpeed;
    cur_x_r_ += (tgt_x_r_ + quirk_x - cur_x_r_) * kLerpSpeed;

    cur_cut_bottom_ += (tgt_cut_bottom_ - cur_cut_bottom_) * kLerpSpeed;
    cur_cut_top_    += (tgt_cut_top_    - cur_cut_top_)    * kLerpSpeed;

    // 5. Cập nhật vị trí & kích thước vào đối tượng LVGL
    DisplayLockGuard lock(this);
    if (!left_eye_ || !right_eye_) return;

    // Khi chớp mắt, giữ tâm Y cố định để mắt khép tự nhiên từ trên và dưới vào giữa
    float draw_y_l = cur_y_l_ + (tgt_h_l_ - cur_h_l_) / 2.0f;
    float draw_y_r = cur_y_r_ + (tgt_h_r_ - cur_h_r_) / 2.0f;

    lv_obj_set_pos(left_eye_, static_cast<int>(cur_x_l_), static_cast<int>(draw_y_l));
    lv_obj_set_size(left_eye_, static_cast<int>(cur_w_l_), static_cast<int>(cur_h_l_));

    lv_obj_set_pos(right_eye_, static_cast<int>(cur_x_r_), static_cast<int>(draw_y_r));
    lv_obj_set_size(right_eye_, static_cast<int>(cur_w_r_), static_cast<int>(cur_h_r_));

    // Cập nhật mí che góc biểu cảm
    int cut_b = static_cast<int>(cur_cut_bottom_);
    lv_obj_set_size(left_cut_bottom_, static_cast<int>(cur_w_l_ + 4), cut_b);
    lv_obj_set_size(right_cut_bottom_, static_cast<int>(cur_w_r_ + 4), cut_b);

    int cut_t = static_cast<int>(cur_cut_top_);
    lv_obj_set_size(left_cut_top_, static_cast<int>(cur_w_l_ + 4), cut_t);
    lv_obj_set_size(right_cut_top_, static_cast<int>(cur_w_r_ + 4), cut_t);
}

// =====================================================================
// BẢNG 19+ BIỂU CẢM CHUẨN CỦA XIAOZHI
// =====================================================================
bool EmoDisplay::SetEmotion(const char* emotion) {
    if (emotion == nullptr) {
        ApplyState(EmoState::kIdle);
        return true;
    }

    std::string e(emotion);
    EmoState next = EmoState::kIdle;

    if (e == "happy" || e == "delicious") {
        next = EmoState::kHappy;
    } else if (e == "laughing" || e == "funny" || e == "silly") {
        next = EmoState::kLaughing;
    } else if (e == "sad") {
        next = EmoState::kSad;
    } else if (e == "crying") {
        next = EmoState::kCrying;
    } else if (e == "angry") {
        next = EmoState::kAngry;
    } else if (e == "surprised" || e == "shocked") {
        next = EmoState::kSurprised;
    } else if (e == "loving" || e == "kissy") {
        next = EmoState::kLoving;
    } else if (e == "embarrassed") {
        next = EmoState::kEmbarrassed;
    } else if (e == "confused") {
        next = EmoState::kConfused;
    } else if (e == "cool" || e == "confident" || e == "relaxed") {
        next = EmoState::kCool;
    } else if (e == "winking") {
        next = EmoState::kWinking;
    } else if (e == "sleepy") {
        next = EmoState::kSleepy;
    } else if (e == "thinking" || e == "connecting" || e == "microchip_ai") {
        next = EmoState::kThinking;
    } else if (e == "listening") {
        next = EmoState::kListening;
    } else if (e == "speaking") {
        next = EmoState::kSpeaking;
    } else if (e == "warning") {
        next = EmoState::kWarning;
    } else if (e == "disgusted") {
        next = EmoState::kDisgusted;
    } else if (e == "curious") {
        next = EmoState::kCurious;
    } else {
        next = EmoState::kIdle;
    }

    ApplyState(next);
    return true;
}

void EmoDisplay::SetStatus(const char* status) {
    if (status == nullptr) return;
    if (strstr(status, "Listening") || strstr(status, "nghe")) {
        SetEmotion("listening");
    } else if (strstr(status, "Speaking") || strstr(status, "nói")) {
        SetEmotion("speaking");
    } else if (strstr(status, "Thinking") || strstr(status, "nghĩ") || strstr(status, "Connecting")) {
        SetEmotion("thinking");
    } else {
        SetEmotion("idle");
    }
}
