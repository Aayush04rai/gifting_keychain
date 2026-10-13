#include "character.h"
#include "watchface_companion.h"

lv_timer_t *watchface_companion_build(lv_obj_t *parent)
{
    return character_build(parent);
}
