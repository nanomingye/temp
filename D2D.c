#include "circle_buff.h"
#include "D2D_cfg.h"
#include "D2D.h"
#include "stm32f4xx.h"
#include <stdint.h>

#include "stm32f429i_discovery.h"
/* Private macros ------------------------------------------------------------*/
/*
command list
*/
#define DRAW_COLOR_BLOCK_RGB565            ((uint32_t)0x00)
#define LOAD_CLUT                          ((uint32_t)0x01) 
#define COPY_SPRITE                        ((uint32_t)0x02)  

/*
Mask
*/
#define D2D_COMM_MASK               0x000000FF

/* Private variables ---------------------------------------------------------*/
//the control struct for d2d command buff
static cbuff_word_object_t D2D_command_buff;
static uint32_t buff_mem[D2D_COMMAND_BUFF_SIZE];

/* Public variables ----------------------------------------------------------*/
volatile d2d_clut_t *current_fg_clut = (d2d_clut_t *)(0);
volatile d2d_clut_t *current_bg_clut = (d2d_clut_t *)(0);
volatile eD2D_statement_t D2D_state = D2D_IDLE ;



/* Private function prototypes -----------------------------------------------*/

static void Command0_Draw_ColorBlock(uint32_t command); //draw a color block on buff
static void Command1_Copy_LUT(uint32_t command); //update the clut


static void Command2_Copy_Sprite(uint32_t command);//copy sprite to buff, no mix source target have same type
static void Command3_Mix_L4_Sprite_565_Buff(uint32_t command);
/*
static void Command3_Mix_565_Sprite_Color_Target565(uint32_t command);
static void Command4_Mix_L4_Sprite_Color_Target565(uint32_t command);
static void Command5_Mix_L8_Sprite_Color_Target565(uint32_t command);



static void command6_Mix_Sprite_Buff_Target565(uint32_t command);
*/

/* Private functions ---------------------------------------------------------*/
static void Command0_Draw_ColorBlock(uint32_t command){
    //command 0....7  command name
    //command 8....10   output color mode
            /*
            000: ARGB8888
            001: RGB888
            010: RGB565
            011: ARGB1555
            100: ARGB4444
            */
    //command 11~24 OOR
            //set the mode to reg to memory mode
    uint32_t temp[3];
    
    DMA2D->CR &= ~(uint32_t)DMA2D_CR_MODE;
    DMA2D->CR |= DMA2D_R2M;
            //set the output color mode
    DMA2D->OPFCCR &= ~(uint32_t)DMA2D_OPFCCR_CM;
    DMA2D->OPFCCR |= (DMA2D_OPFCCR_CM&(command>>8));
    //set the line shift
    DMA2D->OOR &=  ~(uint32_t)DMA2D_OOR_LO;   
    DMA2D->OOR |= (DMA2D_OOR_LO&(command>>11));  // to setup the line shift for target
    
    CBUF_PopSerial_Notsafe(&D2D_command_buff,temp, 3);
            //set the output color value
    DMA2D->OCOLR = temp[0];  //the second word is the output color  
            //set the output address
    DMA2D->OMAR = temp[1];  //the third word is to setup the target address
            //set the line number and line pixel number
    DMA2D->NLR &= ~(DMA2D_NLR_NL | DMA2D_NLR_PL);
    DMA2D->NLR |= temp[2]; //the fourth word is to setup the line number and line pixel        
    //start the DMA2D
    DMA2D->CR |= (uint32_t)DMA2D_CR_START;
}



static void Command1_Copy_LUT(uint32_t command){
    //command 0....7  command name
    //command 8....15 numbers
    //command 16      layer
    //command 17      ccm
    uint32_t layer;
    uint32_t temp;
    temp = CBUF_PopAWord_Notsafe(&D2D_command_buff);
    layer = (command>>16)&(0x00000001UL);
    if (0 == layer){
        DMA2D->FGCMAR = temp;//address
        DMA2D->FGPFCCR &=~(DMA2D_FGPFCCR_CCM|DMA2D_FGPFCCR_CS);//load number,decide color format
        temp = (command&0x0000ff00UL)|(((command>>17)&(0x00000001UL))<<4);
        DMA2D->FGPFCCR |=  temp;
        temp = CBUF_PopAWord_Notsafe(&D2D_command_buff);
        current_fg_clut = (d2d_clut_t *)temp;
        DMA2D->FGPFCCR |=DMA2D_FGPFCCR_START;//start
    }else{
        DMA2D->BGCMAR = temp;
        DMA2D->BGPFCCR &= ~(DMA2D_BGPFCCR_CCM|DMA2D_BGPFCCR_CS);
        temp = (command&0x0000ff00UL)|(((command>>17)&(0x00000001UL))<<4);
        DMA2D->BGPFCCR |= temp;
        temp = CBUF_PopAWord_Notsafe(&D2D_command_buff);
        current_bg_clut = (d2d_clut_t *)temp;
        DMA2D->BGPFCCR |= DMA2D_BGPFCCR_START;
    }
}



static void Command2_Copy_Sprite(uint32_t command){
    //command 0....7  command name
    //command 8...11 cm
    uint32_t temp[4];
    uint32_t temp_command;
    CBUF_PopSerial_Notsafe(&D2D_command_buff,temp, 4);
    
    DMA2D->CR &= ~(uint32_t)DMA2D_CR_MODE;
    DMA2D->CR |= DMA2D_M2M;
    
    DMA2D->FGPFCCR &= ~(DMA2D_FGPFCCR_CM);
    DMA2D->FGPFCCR = (command>>8)&DMA2D_FGPFCCR_CM ;//cm decided color type
    
    DMA2D->FGMAR = temp[0];//2nd word, source address
    
    DMA2D->OMAR = temp[1]; //3nd word, target address   

    DMA2D->NLR &= ~(DMA2D_NLR_NL | DMA2D_NLR_PL);
    DMA2D->NLR |= temp[2]; //4th word,  line pixel
    
    temp_command = temp[3];
    
    //for 5th word, low 16bit is offset for source, high 16bit is offset for target
    DMA2D->FGOR =(temp_command)&(DMA2D_FGOR_LO);// offset for source   
    DMA2D->OOR = (temp_command>>16)&(DMA2D_OOR_LO);//two offset for 1 word offset for target
    
    //start the DMA2D
    DMA2D->CR |= (uint32_t)DMA2D_CR_START;
}



void Command3_Mix_L4_Sprite_565_Buff(uint32_t command){
    
}
/* Exported functions ------------------------------------------------------- */
void D2D_Init(void){
    NVIC_InitTypeDef   NVIC_InitStructure;
    
    //enable the clock for D2D system
    /* Enable the DMA2D Clock */
    RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_DMA2D, ENABLE);
    
    //enable the IRQs
    NVIC_InitStructure.NVIC_IRQChannel = DMA2D_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = D2D_IRQS_PRIORITY;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0x00;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);
    
    DMA2D_ITConfig(DMA2D_IT_TC , ENABLE);
    DMA2D_ITConfig(DMA2D_IT_CTC , ENABLE);
    
    //init the command buff
    CBUF_Word_Init(&D2D_command_buff, D2D_COMMAND_BUFF_SIZE, buff_mem);
    //set the globe statement
    D2D_state = D2D_IDLE;
}



/*
The main handler for DMA2D engine
*/
void D2D_Handler(void){
    uint32_t command;

/* close irq is not necessary here
    //disable all irqs
    __set_PRIMASK(1);    //because now we are in the irq handler, so irq must be enabled....
                         //so we directly disable it
*/    
    //clear all it pending bit
     
    DMA2D->IFCR = (uint32_t)((DMA2D_IT_TC | DMA2D_IT_CTC)>>8);
    
    if (CBUF_IsEmpty_Notsafe(&D2D_command_buff)){
        //if there is nothing in the command buff
        D2D_state = D2D_IDLE;
        //exit the handler
    }else{
        D2D_state = D2D_ACTIVE;
        //pop the first word the diecide the command type
        command = CBUF_PopAWord_Notsafe( &D2D_command_buff );
        //the low 16bit of command will be the command name
        switch (command&(D2D_COMM_MASK)){
        case 0: // filling a pure color area to buff            
            Command0_Draw_ColorBlock(command);  
            break;       
        case 1: 
            Command1_Copy_LUT(command);           
            break;
        case 2:
            Command2_Copy_Sprite(command);
            break;
        case 3:
            Command3_Mix_L4_Sprite_565_Buff(command);
            break;
            
        }
    }
/*  close irq is not necessary here  
    //enable the irqs
    __set_PRIMASK(0);
*/
}



/*
public call functions, draw a rgb565 color block
*/
void D2D_Draw_RGB565_Rect(rambuff_16bit_t *buff, color_block_16bit_t *cblock){
    uint32_t temp[4];
    uint32_t temp_command;
    
    int16_t s_x,s_y;
    int16_t  p_x,p_y;
    int16_t dx=0,dy=0;
//same for ever function    
    uint32_t current_IRQ_state;    
    current_IRQ_state =__get_PRIMASK();    
    __set_PRIMASK(1);
//end     
    p_x = cblock->position_x;
    p_y = cblock->position_y;
    s_x = cblock->size_x;
    s_y = cblock->size_y;
    
    
    
    if(((p_x < buff->size_x)&&(p_y < buff->size_y))&&(((p_x+s_x)>=0)&&((p_y+s_y)>=0))){  //test if there is some part of the block is inside the area
/////////////////////////////////////////////
        //calculate the active size_x size_y and p_x p_y       
        if (p_x<0){
            dx = p_x;
            p_x = 0;           
        }
        if (p_y<0){
            dy = p_y;
            p_y = 0;
        }
        
        s_x += dx;
        s_y += dy;
        
        dx = s_x + p_x;
        if (dx>(buff->size_x)){
            dx = (buff->size_x)- dx;
            s_x += dx;
        }
        dy = s_y + p_y;
        if (dy>(buff->size_y)){
            dy = (buff->size_y)- dy;
            s_y += dy;
        }
//////////////////////////////////////////////
        temp_command = (DRAW_COLOR_BLOCK_RGB565)|((DMA2D_RGB565)<<8);
        temp_command |= (((buff->size_x)- s_x)<<11); 
        temp[0] = temp_command; //1st word command + color mode + oor
        temp[1]= cblock->color;//2nd word color
        //size 0...15 y
        //size 16...29 x
        temp_command = (uint32_t)((buff->mem)+ p_x + p_y*(buff->size_x));
        temp[2] = temp_command;//3nd word address
        temp_command = ((((uint32_t)s_x)<<16)|((uint32_t)s_y)) &((DMA2D_NLR_NL | DMA2D_NLR_PL));
        temp[3] = temp_command;//4th size
        CBUF_PushSerial_Notsafe(&D2D_command_buff,temp,4);
/////////////////////////////////////////////    
//same for ever function
        if ( D2D_IDLE == D2D_state){
            NVIC_SetPendingIRQ(DMA2D_IRQn);  //start the D2D handler by software
        }
    }
    if (!current_IRQ_state){
        __set_PRIMASK(0);
    }
}




/*
load clut to DMA2d
*/

void D2D_Load_CLUT(d2d_clut_t *clut,e_d2d_layer_t layer){
    uint32_t temp[3];
    uint32_t temp_command;
    
    uint32_t current_IRQ_state;    
    current_IRQ_state =__get_PRIMASK();    
    __set_PRIMASK(1);
    ////////////////////////////////////////////
    temp_command = (LOAD_CLUT)|(((uint32_t)((clut->number)&0xff))<<8);
    temp_command |= ((((uint32_t)layer)&0x01)<<16);    
    temp_command |= ((((uint32_t)(clut->type))&0x01)<<17);
    temp[0]=temp_command;
    temp[1]= (uint32_t)(clut->data);
    temp[2]= (uint32_t)(clut); //push the pointer of clut
    CBUF_PushSerial_Notsafe(&D2D_command_buff,temp,3);
    
    /////////////////////////////////////////////    
    if ( D2D_IDLE == D2D_state){
            NVIC_SetPendingIRQ(DMA2D_IRQn);  //start the D2D handler by software
    }

    if (!current_IRQ_state){
        __set_PRIMASK(0);
    }
    
}



void D2D_Draw_RGB565_Sprite_on_RGB565_Buff(rambuff_16bit_t *buff, d2d_16bit_sprite_t *sprite){
    uint32_t temp[5];
    uint32_t temp_command;
    
    int16_t s_x, s_y, p_x,p_y; //sx = pixel on target s_y = line on target
//    int16_t source_x,source_y,source_offset;
    int16_t dx = 0,dy =0;
    
    //same for ever function    
    uint32_t current_IRQ_state;    
    current_IRQ_state =__get_PRIMASK();    
    __set_PRIMASK(1);
//end 
    
    
    p_x = sprite->position_x;
    p_y = sprite->position_y;
    s_x = sprite->size_x;
    s_y = sprite->size_y; 
    
    
    if(((p_x < buff->size_x)&&(p_y < buff->size_y))&&(((p_x+s_x)>=0)&&((p_y+s_y)>=0))){
//        source_offset = sprite->offset;
        //calculate the active size_x size_y and p_x p_y       
        if (p_x<0){
            dx = p_x;
            p_x = 0;           
        }
        if (p_y<0){
            dy = p_y;
            p_y = 0;
        }
        
        s_x += dx;
        s_y += dy;
        
        dx = s_x + p_x;
        if (dx>(buff->size_x)){
            dx = (buff->size_x)- dx;
            s_x += dx;
        }
        dy = s_y + p_y;
        if (dy>(buff->size_y)){
            dy = (buff->size_y)- dy;
            s_y += dy;
        }
        //now the s_x ,s_y = size
               
        temp[0] = (COPY_SPRITE )|((CM_RGB565)<<8);//1st command word and color mode       
        temp_command = (uint32_t)((buff->mem)+ p_x + p_y*(buff->size_x));
        temp[2] =temp_command;//3th target address
        temp_command = ((((uint32_t)s_x)<<16)|((uint32_t)s_y)) &((DMA2D_NLR_NL | DMA2D_NLR_PL));
        temp[3] = temp_command; //4th word the size     
        temp_command = (sprite->offset + sprite->size_x - s_x)|(((buff->size_x)- s_x)<<16);
        temp[4] =temp_command;//5th offsets            
        temp_command = (uint32_t)((sprite->data) + (p_x - sprite->position_x) + (p_y - sprite->position_y)*(sprite->offset + sprite->size_x));
        temp[1] = temp_command;//2nd source address
        CBUF_PushSerial_Notsafe(&D2D_command_buff,temp,5);
    /////////////////////////////////////////////    
//same for ever function
        if ( D2D_IDLE == D2D_state){
            NVIC_SetPendingIRQ(DMA2D_IRQn);  //start the D2D handler by software
        }
    }
    if (!current_IRQ_state){
        __set_PRIMASK(0);
    }
}
