#include "animated_sprite.h"

namespace sl
{

AnimatedSprite create_animated_sprite(sl::Atlas atlas, int start_tile_id, int numframes, int x, int y, int width,
                                      int height, float duration)
{
    AnimatedSprite sprite;
    sprite.atlas = atlas;
    sprite.current_frame = 0;
    sprite.width = width;
    sprite.height = height;
    sprite.x = x;
    sprite.y = y;
    sprite.start_tile_id = start_tile_id;
    for (int i = 0; i < numframes; i++)
    {
        AnimationFrame frame;
        frame.duration = duration;
        frame.tile_id = i + start_tile_id;
        frame.time_left = duration;
        sprite.frames.emplace_back(frame);
    }
    return sprite;
}

void draw_animated_sprite(AnimatedSprite &sprite, sl::Bitmap *destination)
{
    sl::atlas_stretch_blit(sprite.atlas, destination, sprite.frames[sprite.current_frame].tile_id, sprite.x, sprite.y,
                           sprite.width, sprite.height);
}
void draw_rotated_animated_sprite(AnimatedSprite &sprite, sl::Bitmap *destination)
{
    sl::atlas_rotate_stretch_sprite(sprite.atlas,sprite.frames[sprite.current_frame].tile_id,sprite.x,sprite.y,
        sprite.rotation_angle,sprite.width,sprite.height,sprite.pivotx,sprite.pivoty);
}
} // namespace sl