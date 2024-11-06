#include "Maix_Speech_Recognition.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "sysctl.h"
#include "plic.h"
#include "uarths.h"
#include "util/g_def.h"
#include "i2s.h"
#include "fpioa.h"

#include "util/VAD.h"
#include "util/MFCC.h"
#include "util/DTW.h"
#include "util/flash.h"
#include "util/ADC.h"

#define USART1_printf Serial.printf

uint16_t VcBuf[atap_len];
atap_tag    atap_arg;
valid_tag   valid_voice[max_vc_con];
v_ftr_tag   ftr;
v_ftr_tag   ftr_temp;
v_ftr_tag   ftr_mdl_temp[10];
v_ftr_tag   *pftr_mdl_temp[10];

#define save_ok     0
#define VAD_fail    1
#define MFCC_fail   2
#define Flash_fail  3

#define FFT_N 512

uint16_t rx_buf[FRAME_LEN];
uint32_t g_rx_dma_buf[FRAME_LEN * 2];
uint64_t fft_out_data[FFT_N / 2];

volatile uint32_t g_index;
volatile uint8_t uart_rec_flag;
volatile uint32_t receive_char;
volatile uint8_t i2s_rec_flag;
volatile uint8_t i2s_start_flag = 0;

int i2s_dma_irq(void *ctx)
{
    uint32_t i;
    if(i2s_start_flag)
    {
        int16_t s_tmp;
        if(g_index)
        {
            i2s_receive_data_dma(I2S_DEVICE_0, &g_rx_dma_buf[g_index], frame_mov * 2, DMAC_CHANNEL3);
            g_index = 0;
            for(i = 0; i < frame_mov; i++)
            {
                s_tmp = (int16_t)(g_rx_dma_buf[2 * i] & 0xffff); //g_rx_dma_buf[2 * i + 1] Low left
                rx_buf[i] = s_tmp + 32768;
            }
            i2s_rec_flag = 1;
        }
        else
        {
            i2s_receive_data_dma(I2S_DEVICE_0, &g_rx_dma_buf[0], frame_mov * 2, DMAC_CHANNEL3);
            g_index = frame_mov * 2;
            for(i = frame_mov; i < frame_mov * 2; i++)
            {
                s_tmp = (int16_t)(g_rx_dma_buf[2 * i] & 0xffff);//g_rx_dma_buf[2 * i + 1] Low left
                rx_buf[i] = s_tmp + 32768;
            }
            i2s_rec_flag = 2;
        }
    }
    else
    {
        i2s_receive_data_dma(I2S_DEVICE_0, &g_rx_dma_buf[0], frame_mov * 2, DMAC_CHANNEL3);
        g_index = frame_mov * 2;
    }
    return 0;
}

SpeechRecognizer::SpeechRecognizer() {
    // Initialize tracking arrays
    memset(models_per_command, 0, MAX_KEYWORDS);
    memset(is_model_valid, 0, MAX_KEYWORDS * MAX_MODELS_PER_KEY);
}
SpeechRecognizer::~SpeechRecognizer(){
    
}

int
SpeechRecognizer::begin()
{
    //io_mux_init
    fpioa_set_function(20, FUNC_I2S0_IN_D0);
    fpioa_set_function(18, FUNC_I2S0_SCLK);
    fpioa_set_function(19, FUNC_I2S0_WS);

    //i2s init
    i2s_init(I2S_DEVICE_0, I2S_RECEIVER, 0x3);

    i2s_rx_channel_config(I2S_DEVICE_0, I2S_CHANNEL_0,
            RESOLUTION_16_BIT, SCLK_CYCLES_32,
            TRIGGER_LEVEL_4, STANDARD_MODE);

    i2s_set_sample_rate(I2S_DEVICE_0, 8000);

    dmac_init();
    dmac_set_irq(DMAC_CHANNEL3, i2s_dma_irq, NULL, 3);
    i2s_receive_data_dma(I2S_DEVICE_0, &g_rx_dma_buf[0], frame_mov * 2, DMAC_CHANNEL3);

    /* Enable the machine interrupt */
    sysctl_enable_irq();
    return 0;
}

int
SpeechRecognizer::record(uint8_t keyword_num, uint8_t model_num)
{
    if (keyword_num > 10) return -1;
    if (model_num > 4)    return -2;
    
    comm = keyword_num;
    uint8_t prc_count = model_num;
    uint32_t addr = 0;

    g_index = 0;
    i2s_rec_flag = 0;
    i2s_start_flag = 1;
    
    addr = ftr_start_addr + comm * size_per_comm + prc_count * size_per_ftr;

    if (save_mdl(VcBuf, addr) == save_ok) {
        return 0;
    }else{
        return -3;
    }

}

int
SpeechRecognizer::recognize()
{
    u8 res;
    u32 dis;

    g_index = 0;
    i2s_rec_flag = 0;
    i2s_start_flag = 1;

    res = spch_recg(VcBuf, &dis);
    if(dis != dis_err) 
        return res;
    else
        return -1;
}
int SpeechRecognizer::addVoiceModel(uint8_t keyword_num, uint8_t model_num, const int16_t *voice_model, uint16_t frame_num) {
    if(keyword_num >= MAX_KEYWORDS || model_num >= MAX_MODELS_PER_KEY)
        return -1;
        
    ftr_save[keyword_num * MAX_MODELS_PER_KEY + model_num].save_sign = save_mask;
    ftr_save[keyword_num * MAX_MODELS_PER_KEY + model_num].frm_num = frame_num;

    for (int i = 0; i < (vv_frm_max * mfcc_num); i++)
        ftr_save[keyword_num * MAX_MODELS_PER_KEY + model_num].mfcc_dat[i] = voice_model[i];
        
    // Update tracking
    is_model_valid[keyword_num][model_num] = true;
    if(model_num >= models_per_command[keyword_num])
        models_per_command[keyword_num] = model_num + 1;
        
    return 0;
}
int
SpeechRecognizer::print_model(uint8_t keyword_num, uint8_t model_num)
{
    Serial.printf("frm_num=%d\n", ftr_save[keyword_num*4 + model_num].frm_num);
    for (int i = 0; i < (vv_frm_max * mfcc_num); i++) {
        if (((i + 1) % 35) == 0)
            Serial.printf("%d,\n", ftr_save[keyword_num*4 + model_num].mfcc_dat[i]);
        else
            Serial.printf("%d, ", ftr_save[keyword_num*4 + model_num].mfcc_dat[i]);
    }
    Serial.printf("\nprint model ok!\n");
    return 0;
}

uint8_t SpeechRecognizer::save_mdl(uint16_t *v_dat, uint32_t addr)
{
    u16 i, num;
    u16 frame_index;
get_noise1:
    frame_index = 0;
    num = atap_len / frame_mov;
    //wait for finish
    while (1) {
        while (i2s_rec_flag == 0)
            continue;
        if (i2s_rec_flag == 1) {
            for (i = 0; i < frame_mov; i++)
                v_dat[frame_mov * frame_index + i] = rx_buf[i];
        } else {
            for (i = 0; i < frame_mov; i++)
                v_dat[frame_mov * frame_index + i] = rx_buf[i + frame_mov];
        }
        i2s_rec_flag = 0;
        frame_index++;
        if (frame_index >= num)
            break;
    }
//  for(i = 0; i < atap_len; i++)
//      Serial.printf("noise: %d\n", v_dat[i]);
    noise_atap(v_dat, atap_len, &atap_arg);
    if (atap_arg.s_thl > 10000) {
        Serial.printf("get noise again...\n");
        goto get_noise1;
    }
    Serial.printf("speeking...\n");
//wait for finish
    while (i2s_rec_flag == 0)
        continue;
    if (i2s_rec_flag == 1) {
        for (i = 0; i < frame_mov; i++)
            v_dat[i + frame_mov] = rx_buf[i];
    } else {
        for (i = 0; i < frame_mov; i++)
            v_dat[i + frame_mov] = rx_buf[i + frame_mov];
    }
    i2s_rec_flag = 0;
    while (1) {
        while (i2s_rec_flag == 0)
            continue;
        if (i2s_rec_flag == 1) {
            for (i = 0; i < frame_mov; i++) {
                v_dat[i] = v_dat[i + frame_mov];
                v_dat[i + frame_mov] = rx_buf[i];
            }
        } else {
            for (i = 0; i < frame_mov; i++) {
                v_dat[i] = v_dat[i + frame_mov];
                v_dat[i + frame_mov] = rx_buf[i + frame_mov];
            }
        }
        i2s_rec_flag = 0;
        if (VAD2(v_dat, valid_voice, &atap_arg) == 1)
            break;
        if (receive_char == 's')
            return MFCC_fail;
    }
//  if (valid_voice[0].end == ((void *)0)) {
//      Serial.printf("VAD_fail\n");
//      return VAD_fail;
//  }

    get_mfcc(&(valid_voice[0]), &ftr, &atap_arg);
    if (ftr.frm_num == 0) {
        //Serial.printf("MFCC_fail\n");
        return MFCC_fail;
    }
//  ftr.word_num = valid_voice[0].word_num;
    return save_ftr_mdl(&ftr, addr);
//  ftr_mdl_temp[addr] = ftr;
//  return save_ok;
}
uint8_t SpeechRecognizer::spch_recg(uint16_t *v_dat, uint32_t *mtch_dis) {
    u16 i;
    u32 ftr_addr;
    u32 min_avg_dis = dis_max;
    u16 min_comm = 0;
    u32 cur_dis;
    v_ftr_tag *ftr_mdl;
    u16 num;
    u16 frame_index;
    uint32_t cycle0, cycle1;

    // Arrays for distance averaging
    u32 cmd_distances[MAX_KEYWORDS] = {0};
    u8 valid_matches[MAX_KEYWORDS] = {0};

get_noise2:
    frame_index = 0;
    num = atap_len / frame_mov;
    //wait for finish
    i2s_rec_flag = 0;
    while (1) {
        while (i2s_rec_flag == 0)
            continue;
        if (i2s_rec_flag == 1) {
            for (i = 0; i < frame_mov; i++)
                v_dat[frame_mov * frame_index + i] = rx_buf[i];
        } else {
            for (i = 0; i < frame_mov; i++)
                v_dat[frame_mov * frame_index + i] = rx_buf[i + frame_mov];
        }
        i2s_rec_flag = 0;
        frame_index++;
        if (frame_index >= num)
            break;
    }
    noise_atap(v_dat, atap_len, &atap_arg);
    if (atap_arg.s_thl > 10000) {
        Serial.printf("get noise again...\n");
        goto get_noise2;
    }

    Serial.printf("speaking...\n");

    //wait for finish
    while (i2s_rec_flag == 0)
        continue;
    if (i2s_rec_flag == 1) {
        for (i = 0; i < frame_mov; i++)
            v_dat[i + frame_mov] = rx_buf[i];
    } else {
        for (i = 0; i < frame_mov; i++)
            v_dat[i + frame_mov] = rx_buf[i + frame_mov];
    }
    i2s_rec_flag = 0;
    while (1) {
        while (i2s_rec_flag == 0)
            continue;
        if (i2s_rec_flag == 1) {
            for (i = 0; i < frame_mov; i++) {
                v_dat[i] = v_dat[i + frame_mov];
                v_dat[i + frame_mov] = rx_buf[i];
            }
        } else {
            for (i = 0; i < frame_mov; i++) {
                v_dat[i] = v_dat[i + frame_mov];
                v_dat[i + frame_mov] = rx_buf[i + frame_mov];
            }
        }
        i2s_rec_flag = 0;
        if (VAD2(v_dat, valid_voice, &atap_arg) == 1)
            break;
        if (receive_char == 's') {
            *mtch_dis = dis_err;
            Serial.printf("send 'c' to start\n");
            return 0;
        }
    }
    Serial.printf("vad ok\n");

    get_mfcc(&(valid_voice[0]), &ftr, &atap_arg);
    if (ftr.frm_num == 0) {
        *mtch_dis = dis_err;
        Serial.printf("MFCC fail ");
        return 0;
    }

    
    Serial.printf("mfcc ok\n");
    i = 0;
    cycle0 = read_csr(mcycle);

    // Compare with stored models
    for (uint8_t cmd = 0; cmd < MAX_KEYWORDS; cmd++) {
        if (models_per_command[cmd] == 0) continue;  // Skip commands with no models

        for (uint8_t mdl = 0; mdl < models_per_command[cmd]; mdl++) {
            if (!is_model_valid[cmd][mdl]) continue;

            ftr_mdl = (v_ftr_tag *)(&ftr_save[cmd * MAX_MODELS_PER_KEY + mdl]);
            cur_dis = dtw(ftr_mdl, &ftr);

            // Original verbose output
            Serial.printf("no. %d, frm_num = %d, save_mask=%d", 
                        cmd * MAX_MODELS_PER_KEY + mdl + 1, 
                        ftr_mdl->frm_num, 
                        ftr_mdl->save_sign);
            Serial.printf("cur_dis=%d\n", cur_dis);

            if (cur_dis != dis_err && cur_dis > 0) {
                cmd_distances[cmd] += cur_dis;
                valid_matches[cmd]++;
            }
        }
    }

    // Calculate averages and find best match
    for (i = 0; i < MAX_KEYWORDS; i++) {
        if (valid_matches[i] > 0) {
            u32 avg_dis = cmd_distances[i] / valid_matches[i];
            Serial.printf("Command %d: avg distance = %d (from %d models)\n", 
                         i+1, avg_dis, valid_matches[i]);
            
            if (avg_dis < min_avg_dis && avg_dis > 0) {
                min_avg_dis = avg_dis;
                min_comm = i + 1;
            }
        }
    }

    cycle1 = read_csr(mcycle) - cycle0;
    Serial.printf("[INFO] recg cycle = 0x%08x\n", cycle1);

    // Apply threshold check
    if (min_avg_dis > 450 || min_avg_dis == dis_max) {
        *mtch_dis = dis_err;
        Serial.printf("No match found (min avg distance: %d)\n", min_avg_dis);
        return 0;
    }

    *mtch_dis = min_avg_dis;
    return (u8)min_comm;
}   