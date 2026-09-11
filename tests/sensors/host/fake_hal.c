#include "fake_hal.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#undef assert
#define assert(x) do { if (!(x)) { fprintf(stderr,"fake HAL %d: %s\n",__LINE__,#x); exit(1); } } while(0)
uint32_t fake_irq, fake_ms, fake_us, fake_delay;
TIM_TypeDef fake_tim6;
void HAL_NVIC_ClearPendingIRQ(int irq) { assert(irq==TIM6_DAC_IRQn); }
int fake_adc_clock, fake_adc_instance;
HAL_StatusTypeDef timer_start, timer_stop, i2c_result, adc_start, adc_stop, adc_poll, adc_config;
uint8_t bmp_regs[256], dht_raw[5];
uint16_t bh_raw, adc_values[16];
unsigned bh_commands, adc_index, bmp_busy_reads, bmp_reset_count, dht_counter_reads;
int dht_stuck, timer_stuck, bad_pulse, corrupt_config, fail_after_data;
int fail_receive;
static uint8_t reg_pointer;
static uint32_t waveform_start;
void fake_reset(void)
{
    fake_irq=fake_ms=fake_us=fake_delay=0;
    timer_start=timer_stop=i2c_result=adc_start=adc_stop=adc_poll=adc_config=HAL_OK;
    bh_commands=adc_index=bmp_busy_reads=bmp_reset_count=dht_counter_reads=0;
    dht_stuck=timer_stuck=bad_pulse=corrupt_config=fail_after_data=0;
    fail_receive=0;
    memset(bmp_regs,0,sizeof(bmp_regs));
    memset(adc_values,0,sizeof(adc_values));
    dht_raw[0]=2; dht_raw[1]=43; dht_raw[2]=0; dht_raw[3]=234; dht_raw[4]=23; /* 55.5%,23.4C */
    bh_raw=1200;
}
void fake_bmp_vector(void)
{
    /* Bosch datasheet compensation example: 25.08 C, 100653 Pa. */
    const int coeff[]={27504,26435,-1000,36477,-10685,3024,2855,140,-7,15500,-14600,6000};
    for (unsigned i=0;i<12;i++) { uint16_t v=(uint16_t)coeff[i]; bmp_regs[0x88+2*i]=v; bmp_regs[0x89+2*i]=v>>8; }
    bmp_regs[0xd0]=0x58;
    uint32_t p=415148u<<4, t=519888u<<4;
    bmp_regs[0xf7]=p>>16; bmp_regs[0xf8]=p>>8; bmp_regs[0xf9]=p;
    bmp_regs[0xfa]=t>>16; bmp_regs[0xfb]=t>>8; bmp_regs[0xfc]=t;
}
uint32_t HAL_GetTick(void) { return fake_ms + fake_us/1000; }
void HAL_Delay(uint32_t ms) { fake_ms+=ms; fake_delay+=ms; }
uint32_t fake_timer_counter(TIM_HandleTypeDef *h)
{
    ++dht_counter_reads;
    if (!timer_stuck) h->Instance->CNT=++fake_us;
    return h->Instance->CNT & 0xffff;
}
HAL_StatusTypeDef HAL_TIM_Base_Start_IT(TIM_HandleTypeDef *h)
{ if(timer_start==HAL_OK) {h->Instance->CR1|=1;h->Instance->DIER|=1;} return timer_start; }
HAL_StatusTypeDef HAL_TIM_Base_Stop_IT(TIM_HandleTypeDef *h)
{ if(timer_stop==HAL_OK) {h->Instance->CR1&=~1u;h->Instance->DIER&=~1u;} return timer_stop; }
void HAL_GPIO_Init(GPIO_TypeDef *g, GPIO_InitTypeDef *cfg)
{ (void)g; if(cfg->Mode==GPIO_MODE_INPUT) waveform_start=fake_us; }
void HAL_GPIO_WritePin(GPIO_TypeDef *g, uint16_t p, GPIO_PinState state) { (void)g;(void)p;(void)state; }
GPIO_PinState HAL_GPIO_ReadPin(GPIO_TypeDef *g, uint16_t p)
{
    (void)g;(void)p; ++fake_us;
    if(dht_stuck) return dht_stuck==1 ? GPIO_PIN_SET : GPIO_PIN_RESET;
    uint32_t t=fake_us-waveform_start;
    if(t<20) return GPIO_PIN_SET;
    if(t<100) return GPIO_PIN_RESET;
    if(t<180) return GPIO_PIN_SET;
    t-=180;
    for(unsigned i=0;i<40;i++) {
        if(t<50) return GPIO_PIN_RESET;
        t-=50;
        unsigned width=(dht_raw[i/8] & (1u<<(7-i%8))) ? 70 : 27;
        if(bad_pulse && i==0) width=8;
        if(t<width) return GPIO_PIN_SET;
        t-=width;
    }
    return GPIO_PIN_RESET;
}
HAL_StatusTypeDef HAL_I2C_Master_Transmit(I2C_HandleTypeDef *h,uint16_t addr,uint8_t *b,uint16_t n,uint32_t timeout)
{
    (void)h; assert(timeout>0 && timeout<=100);
    if(i2c_result!=HAL_OK) return i2c_result;
    if(addr==(0x23<<1)) { ++bh_commands; return HAL_OK; }
    assert(n==1 || n==2); reg_pointer=b[0];
    if(n==2) {
        if(b[0]==0xe0) {++bmp_reset_count;bmp_regs[0xf4]=bmp_regs[0xf5]=0;}
        else bmp_regs[b[0]]=b[1];
        if(corrupt_config && b[0]==0xf5) bmp_regs[0xf5]^=4;
    }
    return HAL_OK;
}
HAL_StatusTypeDef HAL_I2C_Master_Receive(I2C_HandleTypeDef *h,uint16_t addr,uint8_t *b,uint16_t n,uint32_t timeout)
{
    (void)h;assert(timeout>0 && timeout<=100);
    if(fail_receive) return HAL_TIMEOUT;
    if(i2c_result!=HAL_OK) return i2c_result;
    if(addr==(0x23<<1)) {assert(n==2);b[0]=bh_raw>>8;b[1]=bh_raw;return HAL_OK;}
    memcpy(b,bmp_regs+reg_pointer,n);
    if(reg_pointer==0xf3 && bmp_busy_reads) { b[0]|=8;--bmp_busy_reads; }
    if(reg_pointer==0xf7 && fail_after_data) bmp_regs[0xf4]=0;
    return HAL_OK;
}
HAL_StatusTypeDef HAL_ADC_ConfigChannel(ADC_HandleTypeDef *h,ADC_ChannelConfTypeDef *cfg) {(void)h;(void)cfg;return adc_config;}
HAL_StatusTypeDef HAL_ADC_Start(ADC_HandleTypeDef *h) {(void)h;return adc_start;}
HAL_StatusTypeDef HAL_ADC_Stop(ADC_HandleTypeDef *h) {(void)h;return adc_stop;}
HAL_StatusTypeDef HAL_ADC_PollForConversion(ADC_HandleTypeDef *h,uint32_t t) {(void)h;assert(t<=10);return adc_poll;}
uint32_t HAL_ADC_GetValue(ADC_HandleTypeDef *h) {(void)h;assert(adc_index<16);return adc_values[adc_index++];}
