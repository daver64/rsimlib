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
    std::vector<AnimationFrame> frames;
};
AnimatedSprite create_animated_sprite(sl::Atlas atlas,int start_tile_id,
    int numframes,int x, int y, int width, int height, float duration);
void draw_animated_sprite(AnimatedSprite &sprite, sl::Bitmap *destination);

class AnimationManager {
public:
    AnimationManager(){}
    ~AnimationManager(){}
    void update()
    {
        const double delta_t=sl::get_frame_time()/1000.0;
        for(auto &as : animated_sprites)
        {
            double time_left=as.frames[as.current_frame].time_left;
            time_left-=delta_t;
            if(time_left<0)
            {
                int next_frame=as.current_frame;// % as.frames.size();
                next_frame++;
                if(next_frame>=as.frames.size())
                    next_frame=0;
                as.current_frame=next_frame;
                
                as.frames[as.current_frame].time_left=as.frames[as.current_frame].duration;
                //std::fprintf(stderr,"rest time left %2.2lf next frame %d ",time_left,next_frame);
            }
            else
            {
                as.frames[as.current_frame].time_left=time_left;
            }
        }
    }
    void add_animated_sprite(AnimatedSprite &sprite)
    {
        animated_sprites.emplace_back(sprite);
    }
    void draw_sprites(sl::Bitmap *bitmap)
    {
        for(auto& sprite : animated_sprites)
        {
            draw_animated_sprite(sprite, bitmap);
        }
    }
private:
    std::vector<AnimatedSprite> animated_sprites;
};


}