#ifndef __DMA2D_ENGINE_H__
#define __DMA2D_ENGINE_H__


#include "D2D_cfg.h"
#include "stm32f4xx.h"
#include <stdint.h>

typedef enum {
    D2D_IDLE = 0,
    D2D_ACTIVE = 1,
} eD2D_statement_t;

typedef struct rambuff_16bit{
    uint16_t size_x;
    uint16_t size_y;
    uint16_t *mem;
    struct rambuff_16bit *next;
}rambuff_16bit_t;

typedef struct {
    uint32_t color;
    int16_t position_x;
    int16_t position_y;
    uint16_t size_x;
    uint16_t size_y;   
}color_block_16bit_t;

//for clut_t.type
#define D2D_CLUT_ARGB8888       0
#define D2D_CLUT_RGB888         1
//for clut layer
typedef enum{
    fg = 0,
    bg = 1,
} e_d2d_layer_t;

typedef struct {
    uint8_t index;
    uint8_t type;
    uint16_t number;
    uint32_t *data;
}d2d_clut_t;

typedef struct {
    uint8_t type;
    uint8_t aplha;
    uint16_t offset; //offset pixels between each line
    int16_t position_x;
    int16_t position_y;
    uint16_t size_x;
    uint16_t size_y;
    uint8_t *data;
    d2d_clut_t *clut;
}d2d_sprite_clut_t;

typedef struct {
    uint8_t type;
    uint8_t  alpha;
    uint16_t  offset;
    int16_t position_x;
    int16_t position_y;
    uint16_t size_x;
    uint16_t size_y;
    uint16_t *data;
}d2d_16bit_sprite_t;




/* Public variables ----------------------------------------------------------*/
extern volatile eD2D_statement_t D2D_state;
extern volatile d2d_clut_t *current_fg_clut;
extern volatile d2d_clut_t *current_bg_clut;
/* Public function prototypes ------------------------------------------------*/
extern void D2D_Init(void);
extern void D2D_Handler(void);

extern void D2D_Draw_RGB565_Rect(rambuff_16bit_t *buff, color_block_16bit_t *cblock);

extern void D2D_Load_CLUT(d2d_clut_t *clut,e_d2d_layer_t layer);

extern void D2D_Draw_RGB565_Sprite_on_RGB565_Buff(rambuff_16bit_t *buff, d2d_16bit_sprite_t *sprite);



#endif