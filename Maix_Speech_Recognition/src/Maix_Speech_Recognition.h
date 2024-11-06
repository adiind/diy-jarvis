#ifndef __MAIX_SPEECH_RECOGNITION_H
#define __MAIX_SPEECH_RECOGNITION_H

#include "Arduino.h"

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sysctl.h"
#include "plic.h"
#include "uarths.h"
#include "util/g_def.h"
#include "i2s.h"

#include "util/VAD.h"
#include "util/MFCC.h"
#include "util/DTW.h"
#include "util/flash.h"
#include "util/ADC.h"


class SpeechRecognizer {
public:
    SpeechRecognizer();
    ~SpeechRecognizer();
    int begin();    
    int record(uint8_t keyword_num, uint8_t model_num); 
    int recognize();  
    int addVoiceModel(uint8_t keyword_num, uint8_t model_num, const int16_t *voice_model, uint16_t frame_num);
    int print_model(uint8_t keyword_num, uint8_t model_num);

private:
    uint8_t save_mdl(uint16_t *v_dat, uint32_t addr);
    uint8_t spch_recg(uint16_t *v_dat, uint32_t *mtch_dis);
    uint8_t comm;

    // New tracking variables
    static const uint8_t MAX_KEYWORDS = 14;
    static const uint8_t MAX_MODELS_PER_KEY = 6;
    uint8_t models_per_command[MAX_KEYWORDS];  // Track models for each command
    bool is_model_valid[MAX_KEYWORDS][MAX_MODELS_PER_KEY];  // Track valid models
};

#endif