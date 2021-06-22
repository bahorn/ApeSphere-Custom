#include "inputdisp.h"

#include <gc/gc.h>
#include <mkb/mkb.h>
#include <cstring>

#include "patch.h"
#include "pad.h"
#include "draw.h"

namespace inputdisp
{

static bool s_visible = false;
static bool s_alternate_loc = false;

static void (*s_PADRead_tramp)(gc::PADStatus *statuses);

static gc::PADStatus s_raw_inputs[4];

static void draw_ring(u32 pts, Vec2f center, f32 inner_radius, f32 outer_radius, gc::GXColor color)
{
    // "Blank" texture object which seems to let us set a color and draw a poly with it idk??
    gc::GXTexObj *texobj = reinterpret_cast<gc::GXTexObj *>(0x807ad0e0);
    mkb::GXLoadTexObj_cached(texobj, gc::GX_TEXMAP0);
    gc::GXSetTevColor(gc::GX_TEVREG0, color);
    float z = -1.0f / 128.0f;

    gc::GXBegin(gc::GX_QUADS, gc::GX_VTXFMT0, pts * 4);

    for (u32 i = 0; i < pts; i++)
    {
        u16 angle = 0xFFFF * i / pts;
        f32 sin_cos[2];
        mkb::math_sin_cos_v(static_cast<s32>(angle), sin_cos);
        f32 curr_inner_x = sin_cos[0] * inner_radius + center.x;
        f32 curr_inner_y = sin_cos[1] * inner_radius + center.y;
        f32 curr_outer_x = sin_cos[0] * outer_radius + center.x;
        f32 curr_outer_y = sin_cos[1] * outer_radius + center.y;

        // TODO factor this out or something?
        u16 next_angle = 0xFFFF * ((i + 1) % pts) / pts;
        f32 next_sin_cos[2];
        mkb::math_sin_cos_v(static_cast<s32>(next_angle), next_sin_cos);
        f32 next_inner_x = next_sin_cos[0] * inner_radius + center.x;
        f32 next_inner_y = next_sin_cos[1] * inner_radius + center.y;
        f32 next_outer_x = next_sin_cos[0] * outer_radius + center.x;
        f32 next_outer_y = next_sin_cos[1] * outer_radius + center.y;

        gc::GXPosition3f32(next_inner_x, next_inner_y, z);
        gc::GXTexCoord2f32(0, 0);
        gc::GXPosition3f32(next_outer_x, next_outer_y, z);
        gc::GXTexCoord2f32(0, 0);
        gc::GXPosition3f32(curr_outer_x, curr_outer_y, z);
        gc::GXTexCoord2f32(0, 0);
        gc::GXPosition3f32(curr_inner_x, curr_inner_y, z);
        gc::GXTexCoord2f32(0, 0);
    }
}

static void draw_circle(u32 pts, Vec2f center, f32 radius, gc::GXColor color)
{
    // "Blank" texture object which seems to let us set a color and draw a poly with it idk??
    gc::GXTexObj *texobj = reinterpret_cast<gc::GXTexObj *>(0x807ad0e0);
    mkb::GXLoadTexObj_cached(texobj, gc::GX_TEXMAP0);
    gc::GXSetTevColor(gc::GX_TEVREG0, color);
    float z = -1.0f / 128.0f;

    gc::GXBegin(gc::GX_TRIANGLEFAN, gc::GX_VTXFMT0, pts + 2);
    gc::GXPosition3f32(center.x, center.y, z);
    gc::GXTexCoord2f32(0, 0);

    for (s32 i = static_cast<s32>(pts) * 2 - 1; i >= static_cast<s32>(pts) - 1; i--)
    {
        u16 angle = 0xFFFF * i / pts;
        f32 sin_cos[2];
        mkb::math_sin_cos_v(static_cast<s32>(angle), sin_cos);
        f32 x = sin_cos[0] * radius + center.x;
        f32 y = sin_cos[1] * radius + center.y;
        gc::GXPosition3f32(x, y, z);
        gc::GXTexCoord2f32(0, 0);
    }
}

static void set_sprite_visible(bool visible)
{
    if (mkb::main_mode != mkb::MD_GAME) return;

    // Hide distracting score sprites under the input display
    for (u32 i = 0; i < mkb::sprite_pool_info.upper_bound; i++)
    {
        if (mkb::sprite_pool_info.status_list[i] == 0) continue;

        // TODO set visibility based on whether input display is enabled
        mkb::Sprite &sprite = mkb::sprites[i];
        if (sprite.g_texture_id == 0x503 ||
            sprite.tick_func == mkb::sprite_monkey_counter_tick ||
            sprite.disp_func == mkb::sprite_monkey_counter_icon_disp ||
            sprite.g_texture_id == 0x502 ||
            sprite.tick_func == mkb::sprite_banana_icon_tick ||
            sprite.tick_func == mkb::sprite_banana_icon_shadow_tick ||
            sprite.tick_func == mkb::sprite_banana_count_tick ||
            strcmp(sprite.text, ":") == 0 ||
            sprite.disp_func == mkb::sprite_hud_player_num_disp)
        {
            if ((visible && sprite.depth < 0.f) || (!visible && sprite.depth >= 0.f))
            {
                sprite.depth = -sprite.depth;
            }
        }
    }
}

void init()
{
    // Hook PADRead to give us raw PAD inputs before the game processes them
    s_PADRead_tramp = patch::hook_function(
        gc::PADRead, [](gc::PADStatus *statuses)
        {
            s_PADRead_tramp(statuses);
            memcpy(s_raw_inputs, statuses, sizeof(s_raw_inputs));
        }
    );
}

void tick()
{
    set_sprite_visible(!s_visible);
}

void set_visible(bool visible)
{
    s_visible = visible;
}

bool is_visible() { return s_visible; }

static bool get_notch_pos(Vec2f *out_pos)
{
    constexpr f32 DIAG = 0.7071067811865476f; // sin(pi/4) or sqrt(2)/2
    bool notch_found = false;

    for (u32 i = 0; i < 4; i++)
    {
        gc::PADStatus &status = mkb::pad_status_groups[i].raw;
        if (status.err != gc::PAD_ERR_NONE) continue;

        if (status.stickX == 0 && status.stickY == 60)
        {
            *out_pos = {0, 1};
            notch_found = true;
        }
        else if (status.stickX == 0 && status.stickY == -60)
        {
            *out_pos = {0, -1};
            notch_found = true;
        }
        else if (status.stickX == 60 && status.stickY == 0)
        {
            *out_pos = {1, 0};
            notch_found = true;
        }
        else if (status.stickX == -60 && status.stickY == 0)
        {
            *out_pos = {-1, 0};
            notch_found = true;
        }
        else if (status.stickX == 60 && status.stickY == 60)
        {
            *out_pos = {DIAG, DIAG};
            notch_found = true;
        }
        else if (status.stickX == 60 && status.stickY == -60)
        {
            *out_pos = {DIAG, -DIAG};
            notch_found = true;
        }
        else if (status.stickX == -60 && status.stickY == 60)
        {
            *out_pos = {-DIAG, DIAG};
            notch_found = true;
        }
        else if (status.stickX == -60 && status.stickY == -60)
        {
            *out_pos = {-DIAG, -DIAG};
            notch_found = true;
        }
    }

    return notch_found;
}

void disp()
{
    if (!s_visible) return;

    Vec2f center = {534, 60};
    f32 scale = 0.6f;

    draw_ring(8, center, 54 * scale, 60 * scale, {0x00, 0x00, 0x00, 0xFF});
    draw_circle(8, center, 54 * scale, {0x00, 0x00, 0x00, 0x7F});
    draw_ring(8, center, 50 * scale, 58 * scale, {0xb1, 0x5a, 0xff, 0xff});

    // Accumulate stick inputs from all controllers since we don't always
    // know which player is active, like in menus
    s32 x = 0, y = 0;
    if (!pad::get_exclusive_mode())
    {
        for (int i = 0; i < 4; i++)
        {
            gc::PADStatus &status = s_raw_inputs[i];
            if (status.err == gc::PAD_ERR_NONE)
            {
                x += status.stickX;
                y += status.stickY;
            }
        }
    }

    Vec2f scaled_input = {
        center.x + static_cast<f32>(x) / 2.7f * scale,
        center.y - static_cast<f32>(y) / 2.7f * scale,
    };

    draw_circle(16, scaled_input, 9 * scale, {0xFF, 0xFF, 0xFF, 0xFF});

    // Show buttons
    if (pad::button_down(gc::PAD_BUTTON_START))
    {
        draw::debug_text(center.x + 65 * scale, center.y - 45 * scale, draw::Color::White, "Start");
    }
    if (pad::button_down(gc::PAD_BUTTON_A))
    {
        draw::debug_text(center.x + 65 * scale, center.y - 25 * scale, draw::Color::Green, "A");
    }
    if (pad::button_down(gc::PAD_BUTTON_B))
    {
        draw::debug_text(center.x + 90 * scale, center.y - 25 * scale, draw::Color::Red, "B");
    }
    if (pad::button_down(gc::PAD_BUTTON_X))
    {
        draw::debug_text(center.x + 65 * scale, center.y - 05 * scale, draw::Color::White, "X");
    }
    if (pad::button_down(gc::PAD_BUTTON_Y))
    {
        draw::debug_text(center.x + 90 * scale, center.y - 05 * scale, draw::Color::White, "Y");
    }
    if (pad::button_down(gc::PAD_TRIGGER_L))
    {
        draw::debug_text(center.x + 65 * scale, center.y + 15 * scale, draw::Color::White, "L");
    }
    if (pad::button_down(gc::PAD_TRIGGER_R))
    {
        draw::debug_text(center.x + 90 * scale, center.y + 15 * scale, draw::Color::White, "R");
    }
    if (pad::button_down(gc::PAD_TRIGGER_Z))
    {
        draw::debug_text(center.x + 115 * scale, center.y + 15 * scale, draw::Color::Blue, "Z");
    }

    // Show notch indicators
    Vec2f notch_norm = {};
    if (get_notch_pos(&notch_norm))
    {
        Vec2f notch_pos = {
            .x = notch_norm.x * 60 * scale + center.x,
            .y = -notch_norm.y * 60 * scale + center.y,
        };
        draw_circle(6, notch_pos, 5 * scale, {0xFF, 0xFF, 0xFF, 0xFF});
    }
}

void set_in_center_loc(bool alternate_loc) { s_alternate_loc = alternate_loc; }
bool is_in_center_loc() { return s_alternate_loc; }

}
