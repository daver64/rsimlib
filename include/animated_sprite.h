#pragma once

#include "sl.h"

namespace sl 
{

struct AnimationFrame {
    float duration=0.100f;
    float time_left=0.100f;
    int tile_id=0;

};

struct AnimatedSprite {
    sl::Atlas atlas;
    int current_frame;
    int x=0;
    int y=0;
    int width=32;
    int height=32;
    int start_tile_id=0;
    float rotation_angle=0.0f;
    float pivotx=0.0f;
    float pivoty=0.0f;
    bool paused=false;
    bool draw_rotated=false;
    std::vector<AnimationFrame> frames;
};
AnimatedSprite create_animated_sprite(sl::Atlas atlas,int start_tile_id,
    int numframes,int x, int y, int width, int height, float duration);
void draw_animated_sprite(AnimatedSprite &sprite, sl::Bitmap *destination);
void draw_rotated_animated_sprite(AnimatedSprite &sprite, sl::Bitmap *destination);

class AnimationManager {
public:
    AnimationManager(){}
    ~AnimationManager(){}
    void update()
    {
        const double delta_t=sl::get_frame_time()/1000.0;
        for(auto &as : animated_sprites)
        {
            if(as.paused)
                continue;
            double time_left=as.frames[as.current_frame].time_left;
            time_left-=delta_t;
            if(time_left<0)
            {
                int next_frame=as.current_frame;
                next_frame++;
                if(next_frame>=as.frames.size())
                    next_frame=0;
                as.current_frame=next_frame;
                as.frames[as.current_frame].time_left=as.frames[as.current_frame].duration;
            }
            else
            {
                as.frames[as.current_frame].time_left=time_left;
            }
        }
    }
    const int  add_animated_sprite(AnimatedSprite &sprite)
    {
        animated_sprites.emplace_back(sprite);
        return (int)animated_sprites.size()-1;
    }
    void draw_sprites(sl::Bitmap *bitmap)
    {
        for(auto& sprite : animated_sprites)
        {
            if(!sprite.draw_rotated)
                draw_animated_sprite(sprite, bitmap);
            else
                draw_rotated_animated_sprite(sprite,bitmap);
        }
    }
    bool get_sprite(int index, AnimatedSprite **sprite)
    {
        if(index<0 || index>-animated_sprites.size())
        {
            (*sprite)=nullptr;
            return false;
        }
        (*sprite)=&animated_sprites[index];
        return true;
    }
    bool set_sprite_tiles(int index, std::vector<int> &tiles)
    {
        AnimatedSprite *sprite;
        bool sprite_valid=get_sprite(index,&sprite);
        if(sprite_valid)
        {
            int i=0;
            if(sprite->frames.size()!= tiles.size())
                return false;
            for(auto & af : sprite->frames)
            {
                af.tile_id=tiles[i];
                i++;
            }
            return true;
        }
        return false;
    }
    bool set_sprite_position(int index, int x, int y)
    {
        AnimatedSprite *sprite;
        bool sprite_valid=get_sprite(index,&sprite);
        if(sprite_valid)
        {
            sprite->x=x;
            sprite->y=y;
            return true;
        }
        return false;
    }
    bool set_sprite_rotation(int index, float angle)
    {
        float passed_angle=std::fmod(angle,360.0f);
        AnimatedSprite *sprite;
        bool sprite_valid=get_sprite(index,&sprite);
        if(sprite_valid)
        {
            sprite->rotation_angle=passed_angle;
            return true;
        }
        return false;
    }
    // not sure we need this...
    bool get_sprite_position(int index, int &x, int &y)
    {
        AnimatedSprite *sprite;
        bool sprite_valid=get_sprite(index,&sprite);
        if(sprite_valid)
        {
            x=sprite->x;
            y=sprite->y;
            return true;
        }
        return false;
    }
    void toggle_pause(int index)
    {
        AnimatedSprite *sprite;
        bool sprite_valid=get_sprite(index,&sprite);
        if(sprite_valid)
        {
            sprite->paused=!sprite->paused;
        }
    }
private:
    std::vector<AnimatedSprite> animated_sprites;
};


}