/*
 * Copyright (c) 2016-2020, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *  ======== adcsinglechannel.c ========
 */
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <unistd.h>

/* POSIX Header files */
#include <pthread.h>

/* Driver Header files */
#include <ti/drivers/ADC.h>
#include <ti/display/Display.h>
#include <ti/drivers/Timer.h>

/* Driver configuration */
#include "ti_drivers_config.h"

/* ADC sample count */
#define ADC_SAMPLE_COUNT  (6000)

//these two have to be the same
// this is 2^28
#define TIMER_PERIOD (268435456)
float timer_period = 268435456;

/* ADC conversion result variables */
uint16_t adcValue0[ADC_SAMPLE_COUNT];
uint16_t adcValue1[ADC_SAMPLE_COUNT];
uint16_t adcValue2[ADC_SAMPLE_COUNT];
float adcValue0MicroVolt[ADC_SAMPLE_COUNT];
float adcValue1MicroVolt[ADC_SAMPLE_COUNT];
float adcValue2MicroVolt[ADC_SAMPLE_COUNT];
float accel[ADC_SAMPLE_COUNT];
float interval[ADC_SAMPLE_COUNT];
long difference[ADC_SAMPLE_COUNT];
int count = 0;

static Display_Handle display;
void store_one_accel_sample(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, int count);
void print_data();

void ADC_setup(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, ADC_Params *params){
    ADC_init();
    ADC_Params_init(params);

    *adc0 = ADC_open(CONFIG_ADC_0, params);
    if (*adc0 == NULL) {
        Display_printf(display, 0, 0, "Error initializing CONFIG_ADC_0\n");
        while (1);
    }
    Display_printf(display, 0, 0, "Successfully initialized CONFIG_ADC_0\n");

    *adc1 = ADC_open(CONFIG_ADC_1, params);
    if (*adc1 == NULL) {
        Display_printf(display, 0, 0, "Error initializing CONFIG_ADC_1\n");
        while (1);
    }
    Display_printf(display, 0, 0, "Successfully initialized CONFIG_ADC_1\n");

    *adc2 = ADC_open(CONFIG_ADC_2, params);
    if (*adc2 == NULL) {
        Display_printf(display, 0, 0, "Error initializing CONFIG_ADC_2\n");
        while (1);
    }
    Display_printf(display, 0, 0, "Successfully initialized CONFIG_ADC_2\n");
}

void Display_setup(Display_Handle *display0){
    /* Call driver init functions */
    Display_init();

    /* Open the display for output */
    *display0 = Display_open(Display_Type_UART, NULL);
    if (*display0 == NULL) {
        /* Failed to open display driver */
        while (1);
    }
    Display_printf(display, 0, 0, "Successfully initialized display\n");
}


void Timer_setup_continuous(Timer_Handle *handle){
    //sets up a continuously running timer

    Timer_init();
    Timer_Params timer_params;
    Timer_Params_init(&timer_params);
    timer_params.periodUnits = Timer_PERIOD_COUNTS;
    timer_params.period = TIMER_PERIOD;
    timer_params.timerMode  = Timer_FREE_RUNNING;
    *handle = Timer_open(CONFIG_TIMER_0, &timer_params);
    if(*handle == NULL){
        Display_printf(display, 0, 0, "Error opening timer handle");
        while(1){
        }
    }
    int32_t status;
    status = Timer_start(*handle);
    if (status == Timer_STATUS_ERROR) {
        Display_printf(display, 0, 0, "Error starting timer");
        //Timer_start() failed
        while (1);
    }
}


void Timer_setup_callback(Timer_Handle *handle){
    //sets up a timer with callback function store_one_accel_sample

    Timer_init();
    Timer_Params timer_params;
    Timer_Params_init(&timer_params);
    timer_params.periodUnits = Timer_PERIOD_US;
    timer_params.period = 100000;
    timer_params.timerMode  = Timer_CONTINUOUS_CALLBACK;
    timer_params.timerCallback = store_one_accel_sample;

    *handle = Timer_open(CONFIG_TIMER_0, &timer_params);
    if(*handle == NULL){
        Display_printf(display, 0, 0, "Error opening timer handle");
        while(1){
        }
    }
    int32_t status;
    status = Timer_start(*handle);
    if (status == Timer_STATUS_ERROR) {
        Display_printf(display, 0, 0, "Error starting timer");
        //Timer_start() failed
        while (1);
    }
}


void print_N_accel_samples(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2){
    //prints N accelerometer readings without timestamps, N = ADC_SAMPLE_COUNT

    int i, res0, res1, res2;
    unsigned long timer_value_now = 0;
    for (i = 0; i < ADC_SAMPLE_COUNT; i++) {
        //Get accel readings
        res0 = ADC_convert(*adc0, &adcValue0[i], &timer_value_now);
        res1 = ADC_convert(*adc1, &adcValue1[i], &timer_value_now);
        res2 = ADC_convert(*adc2, &adcValue2[i], &timer_value_now);

        //make sure readings were successful
        if (res0 == ADC_STATUS_SUCCESS) {
            adcValue0MicroVolt[i] = (float)adcValue0[i]/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_0 convert failed (%d)\n", i);
        }
        if (res1 == ADC_STATUS_SUCCESS) {
            adcValue1MicroVolt[i] = (float)adcValue1[i]/1.15;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_1 convert failed (%d)\n", i);
        }
        if (res2 == ADC_STATUS_SUCCESS) {
            adcValue2MicroVolt[i] = (float)adcValue2[i]/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_2 convert failed (%d)\n", i);
        }
        accel[i] = sqrt(pow(adcValue0MicroVolt[i], 2) + pow(adcValue1MicroVolt[i], 2) + pow(adcValue2MicroVolt[i], 2));
    }
    for (i = 0; i < ADC_SAMPLE_COUNT; i++) {
        Display_printf(display, 0, 0, "Sample: %d, accel: %f, ADC_0: %fmv, ADC_1: %fmv, ADC_2: %fmv\n",
                       i+1, accel[i], adcValue0MicroVolt[i], adcValue1MicroVolt[i], adcValue2MicroVolt[i]);
    }
}


void store_one_accel_sample(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, int count){
    //gets an accelerometer reading and stores it in the global variables.  Can be used as a
    //callback function

    int res0, res1, res2;
    unsigned long timer_value_now = 0;
    //Get accel readings
    res0 = ADC_convert(*adc0, &adcValue0[count], &timer_value_now);
    res1 = ADC_convert(*adc1, &adcValue1[count], &timer_value_now);
    res2 = ADC_convert(*adc2, &adcValue2[count], &timer_value_now);

    //make sure readings were successful
    if (res0 == ADC_STATUS_SUCCESS) {
        adcValue0MicroVolt[count] = (float)adcValue0[count]/1.14;
    }
    else {
        Display_printf(display, 0, 0, "CONFIG_ADC_0 convert failed (%d)\n", count);
    }
    if (res1 == ADC_STATUS_SUCCESS) {
        adcValue1MicroVolt[count] = (float)adcValue1[count]/1.15;
    }
    else {
        Display_printf(display, 0, 0, "CONFIG_ADC_1 convert failed (%d)\n", count);
    }
    if (res2 == ADC_STATUS_SUCCESS) {
        adcValue2MicroVolt[count] = (float)adcValue2[count]/1.14;
    }
    else {
        Display_printf(display, 0, 0, "CONFIG_ADC_2 convert failed (%d)\n", count);
    }
    accel[count] = sqrt(pow(adcValue0MicroVolt[count], 2) + pow(adcValue1MicroVolt[count], 2) + pow(adcValue2MicroVolt[count], 2));
    count++;
    if(count > ADC_SAMPLE_COUNT){
        print_data();
        exit(0);
    }
}


void print_data(){
    //prints stored accelerometer and timestamp data

    int i =0;
    for (i = 0; i < ADC_SAMPLE_COUNT; i++) {
        Display_printf(display, 0, 0, "Sample: %d, accel: %f, ADC_0: %fmv, ADC_1: %fmv, ADC_2: %fmv",
                       i+1, accel[i], adcValue0MicroVolt[i], adcValue1MicroVolt[i], adcValue2MicroVolt[i]);
    }
}


void print_timestamped_data(float total_time, int samples_to_print){
    //prints stored accelerometer and timestamp data

    int i =0;
    for (i = 0; i < samples_to_print; i++) {
        Display_printf(display, 0, 0, "Sample: %d, accel: %f, ADC_0: %fmv, ADC_1: %fmv, ADC_2: %fmv, Interval: %fms",
                               i+1, accel[i], adcValue0MicroVolt[i], adcValue1MicroVolt[i], adcValue2MicroVolt[i], interval[i]);
    }
    Display_printf(display, 0, 0, "Total time: %fms\n", total_time);
}


void print_N_accel_samples_timer(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, Timer_Handle *timer_handle){
    //prints N accelerometer samples according to a continuous timer, N = ADC_SAMPLE_COUNT

    int i, res0, res1, res2;
    unsigned long timer_value_now = 0;
    int timer_clock_before = 0;
    int timer_clock_now = 0;
    float total_time;
    for (i = 0; i < ADC_SAMPLE_COUNT; i++) {
        //Get accel readings
        res0 = ADC_convert(*adc0, &adcValue0[i], &timer_value_now);
        res1 = ADC_convert(*adc1, &adcValue1[i], &timer_value_now);
        res2 = ADC_convert(*adc2, &adcValue2[i], &timer_value_now);

        timer_clock_before = timer_clock_now;
        timer_clock_now = Timer_getCount(*timer_handle);
        // Gets the time between 2 system clock readings <now> and <before>
        // based on the 80Mhz system clock, 1 tick = 0.0000125 milliseconds
        interval[i] = (float)(0.0000125)*(timer_clock_now - timer_clock_before);
        difference[i] = timer_clock_now - timer_clock_before;

        //make sure readings were successful
        if (res0 == ADC_STATUS_SUCCESS) {
            adcValue0MicroVolt[i] = (float)adcValue0[i]/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_0 convert failed (%d)\n", i);
        }
        if (res1 == ADC_STATUS_SUCCESS) {
            adcValue1MicroVolt[i] = (float)adcValue1[i]/1.15;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_1 convert failed (%d)\n", i);
        }
        if (res2 == ADC_STATUS_SUCCESS) {
            adcValue2MicroVolt[i] = (float)adcValue2[i]/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_2 convert failed (%d)\n", i);
        }
        accel[i] = sqrt(pow(adcValue0MicroVolt[i], 2) + pow(adcValue1MicroVolt[i], 2) + pow(adcValue2MicroVolt[i], 2));
    }

    for (i = 0; i < ADC_SAMPLE_COUNT; i++) {
        Display_printf(display, 0, 0, "Sample: %d, accel: %f, ADC_0: %fmv, ADC_1: %fmv, ADC_2: %fmv, Interval: %fms",
                       i+1, accel[i], adcValue0MicroVolt[i], adcValue1MicroVolt[i], adcValue2MicroVolt[i], interval[i]);
    }

    total_time = (float)(0.0000125)*(timer_clock_now - difference[0]);
    Display_printf(display, 0, 0, "Total time: %fms\n", total_time);
}


void print_accel_samples_seconds(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, Timer_Handle *timer_handle, float seconds){
    //prints accelerometer samples according to a continuous timer, will print for <seconds> seconds

    int res0, res1, res2;
    int i = 0;
    unsigned long timer_value_now = 0;
    int timer_clock_before = 0;
    int timer_clock_now = 0;
    float total_time = 0;
    float milliseconds = seconds * 1000;
    while(total_time < milliseconds){
        //Get accel readings
        res0 = ADC_convert(*adc0, &adcValue0[i], &timer_value_now);
        res1 = ADC_convert(*adc1, &adcValue1[i], &timer_value_now);
        res2 = ADC_convert(*adc2, &adcValue2[i], &timer_value_now);

        timer_clock_before = timer_clock_now;
        timer_clock_now = Timer_getCount(*timer_handle);
        // Gets the time between 2 system clock readings <now> and <before>
        // based on the 80Mhz system clock, 1 tick = 0.0000125 milliseconds
        interval[i] = (float)(0.0000125)*(timer_clock_now - timer_clock_before);
        difference[i] = timer_clock_now - timer_clock_before;

        //make sure readings were successful
        if (res0 == ADC_STATUS_SUCCESS) {
            adcValue0MicroVolt[i] = (float)adcValue0[i]/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_0 convert failed (%d)\n", i);
        }
        if (res1 == ADC_STATUS_SUCCESS) {
            adcValue1MicroVolt[i] = (float)adcValue1[i]/1.15;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_1 convert failed (%d)\n", i);
        }
        if (res2 == ADC_STATUS_SUCCESS) {
            adcValue2MicroVolt[i] = (float)adcValue2[i]/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_2 convert failed (%d)\n", i);
        }
        accel[i] = sqrt(pow(adcValue0MicroVolt[i], 2) + pow(adcValue1MicroVolt[i], 2) + pow(adcValue2MicroVolt[i], 2));

        total_time += interval[i];
        i++;

        if(i == (int)(ADC_SAMPLE_COUNT)){
            print_timestamped_data(total_time, i);
            i = 0;
        }
    }

    print_timestamped_data(total_time, i);
}





void print_accel_forever_timer(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, Timer_Handle *timer_handle, bool csv){
    // prints accelerometer readings forever according to a continuous timer,
    // and waits 1 second between readings if the first line of the while loop is not commented out

    int i, res0, res1, res2;
    uint16_t adcraw0, adcraw1, adcraw2;
    float adc0mv, adc1mv, adc2mv;
    float ms_interval, acceleration;
    unsigned long timer_value_now = 0;
    int timer_clock_before = 0;
    int timer_clock_now = 0;
    float timestamp = 0;
    i = 0;

    if(csv){
        Display_printf(display, 0, 0, "Sample,ADC_0,ADC_1,ADC_2,Timestamp,Acceleration");
    }

    while(1) {
        //sleep(1);
        i++;
        //Get accel readings
        res0 = ADC_convert(*adc0, &adcraw0, &timer_value_now);
        res1 = ADC_convert(*adc1, &adcraw1, &timer_value_now);
        res2 = ADC_convert(*adc2, &adcraw2, &timer_value_now);

        timer_clock_before = timer_clock_now;
        timer_clock_now = Timer_getCount(*timer_handle);
        // Gets the time between 2 system clock readings <now> and <before>
        // based on the 80Mhz system clock, 1 tick = 0.0000125 milliseconds
        if (timer_clock_before < timer_clock_now){
            ms_interval = (float)(0.0000125)*(timer_clock_now - timer_clock_before);
            timestamp += ms_interval;
        }
        else {
            ms_interval = (float)(0.0000125)*(timer_period + timer_clock_now - timer_clock_before);
            timestamp += ms_interval;
        }

        //make sure readings were successful
        if (res0 == ADC_STATUS_SUCCESS) {
            adc0mv = (float)adcraw0/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_0 convert failed (%d)\n", i);
        }
        if (res1 == ADC_STATUS_SUCCESS) {
            adc1mv = (float)adcraw1/1.15;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_1 convert failed (%d)\n", i);
        }
        if (res2 == ADC_STATUS_SUCCESS) {
            adc2mv = (float)adcraw2/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_2 convert failed (%d)\n", i);
        }
        acceleration = sqrt(pow(adc0mv, 2) + pow(adc1mv, 2) + pow(adc2mv, 2))/1040;
        if(csv){
            Display_printf(display, 0, 0, "%d,%f,%f,%f,%f,%f",
                                           i, adc0mv, adc1mv, adc2mv, timestamp, acceleration);
        }
        else{
            Display_printf(display, 0, 0, "Sample: %d, accel: %f, ADC_0: %fmv, ADC_1: %fmv, ADC_2: %fmv, Interval: %fms",
                                           i, acceleration, adc0mv, adc1mv, adc2mv, ms_interval);
        }
    }
}



void print_accel_forever_timer_detection(ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, Timer_Handle *timer_handle){
    // prints accelerometer readings forever according to a continuous timer,
    // and waits 1 second between readings if the first line of the while loop is not commented out

    int i, res0, res1, res2;
    uint16_t adcraw0, adcraw1, adcraw2;
    float adc0mv, adc1mv, adc2mv;
    float ms_interval, new_accel;
    unsigned long timer_value_now = 0;
    int timer_clock_before = 0;
    int timer_clock_now = 0;
    float timestamp = 0;
    i = 0;
    float alpha = 0.125;
    float threshold = 0.15;
    float beta = 0.25;
    float mov_avg = 1;
    float dev_accel = 0;


    /*
     * Global variables:
    float adcValue0MicroVolt[ADC_SAMPLE_COUNT];
    float adcValue1MicroVolt[ADC_SAMPLE_COUNT];
    float adcValue2MicroVolt[ADC_SAMPLE_COUNT];
    float accel[ADC_SAMPLE_COUNT];
    float interval[ADC_SAMPLE_COUNT];
    long difference[ADC_SAMPLE_COUNT];
     *
     */


    while(1) {
        //sleep(1);
        if(i > ADC_SAMPLE_COUNT){i = 0;}
        else{i++;}

        //Get accel readings
        res0 = ADC_convert(*adc0, &adcraw0, &timer_value_now);
        res1 = ADC_convert(*adc1, &adcraw1, &timer_value_now);
        res2 = ADC_convert(*adc2, &adcraw2, &timer_value_now);

        timer_clock_before = timer_clock_now;
        timer_clock_now = Timer_getCount(*timer_handle);
        // Gets the time between 2 system clock readings <now> and <before>
        // based on the 80Mhz system clock, 1 tick = 0.0000125 milliseconds
        if (timer_clock_before < timer_clock_now){
            ms_interval = (float)(0.0000125)*(timer_clock_now - timer_clock_before);
            timestamp += ms_interval;
        }
        else {
            ms_interval = (float)(0.0000125)*(timer_period + timer_clock_now - timer_clock_before);
            timestamp += ms_interval;
        }

        //make sure readings were successful
        if (res0 == ADC_STATUS_SUCCESS) {
            adc0mv = (float)adcraw0/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_0 convert failed (%d)\n", i);
        }
        if (res1 == ADC_STATUS_SUCCESS) {
            adc1mv = (float)adcraw1/1.15;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_1 convert failed (%d)\n", i);
        }
        if (res2 == ADC_STATUS_SUCCESS) {
            adc2mv = (float)adcraw2/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_2 convert failed (%d)\n", i);
        }

        new_accel = sqrt(pow(adc0mv, 2) + pow(adc1mv, 2) + pow(adc2mv, 2))/1040;
        accel[i] = new_accel;
        interval[i] = ms_interval;

        mov_avg = (1 - alpha) * (mov_avg) + alpha * (new_accel);
        dev_accel = (1 - beta) * (dev_accel) + beta * (fabs(new_accel - mov_avg));

        Display_printf(display, 0, 0, "Sample: %d, accel: %f, moving avg: %f, dev_accel: %f, Interval: %fms",
                                                   i, new_accel, mov_avg, dev_accel, ms_interval);

        if(dev_accel > threshold){
            Display_printf(display, 0, 0, "Detected event");
            send_data(i, &adc0, &adc1, &adc2, &timer_handle);
            Display_printf(display, 0, 0, "Detected event");
        }
    }
}



void send_data(int i, ADC_Handle *adc0, ADC_Handle *adc1, ADC_Handle *adc2, Timer_Handle *timer_handle){

    int res0, res1, res2;
    uint16_t adcraw0, adcraw1, adcraw2;
    float adc0mv, adc1mv, adc2mv;
    float ms_interval, sample_accel;
    unsigned long timer_value_now = 0;
    int timer_clock_before = 0;
    int timer_clock_now = 0;
    float timestamp = 0;

    int start = i;
    int j;
    // read 100 more samples
    for(j=0; j<100; j++){
        if(i > ADC_SAMPLE_COUNT){i = 0;}
        else{i++;}

        //Get accel readings
        res0 = ADC_convert(*adc0, &adcraw0, &timer_value_now);
        res1 = ADC_convert(*adc1, &adcraw1, &timer_value_now);
        res2 = ADC_convert(*adc2, &adcraw2, &timer_value_now);

        timer_clock_before = timer_clock_now;
        timer_clock_now = Timer_getCount(*timer_handle);
        // Gets the time between 2 system clock readings <now> and <before>
        // based on the 80Mhz system clock, 1 tick = 0.0000125 milliseconds
        if (timer_clock_before < timer_clock_now){
            ms_interval = (float)(0.0000125)*(timer_clock_now - timer_clock_before);
            timestamp += ms_interval;
        }
        else {
            ms_interval = (float)(0.0000125)*(timer_period + timer_clock_now - timer_clock_before);
            timestamp += ms_interval;
        }

        //make sure readings were successful
        if (res0 == ADC_STATUS_SUCCESS) {
            adc0mv = (float)adcraw0/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_0 convert failed (%d)\n", i);
        }
        if (res1 == ADC_STATUS_SUCCESS) {
            adc1mv = (float)adcraw1/1.15;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_1 convert failed (%d)\n", i);
        }
        if (res2 == ADC_STATUS_SUCCESS) {
            adc2mv = (float)adcraw2/1.14;
        }
        else {
            Display_printf(display, 0, 0, "CONFIG_ADC_2 convert failed (%d)\n", i);
        }

        sample_accel = sqrt(pow(adc0mv, 2) + pow(adc1mv, 2) + pow(adc2mv, 2))/1040;
        accel[i] = sample_accel;
        interval[i] = ms_interval;
    }

    // reconstruct array
    float new_accel[ADC_SAMPLE_COUNT];
    float new_interval[ADC_SAMPLE_COUNT];
    int count = 0;

    for(j = start; j < ADC_SAMPLE_COUNT; j++){
        new_accel[count] = accel[j];
        new_interval[count] = interval[j];
        count ++;
    }
    for(j = 0; j < start; j++){
        new_accel[count] = accel[j];
        new_interval[count] = interval[j];
        count++;
    }

    for(j=0; j<ADC_SAMPLE_COUNT; j++){
        Display_printf(display, 0, 0, "%d,%f,%f", j, new_accel[j], new_interval[j]);
    }

}


/*
 *  ======== mainThread ========
 */
void *mainThread(void *arg0)
{
    ADC_Handle   adc0, adc1, adc2;
    ADC_Params   params;
    Timer_Handle timer_handle;
    bool csv = 1;

    Display_setup(&display);
    ADC_setup(&adc0, &adc1, &adc2, &params);
    Timer_setup_continuous(&timer_handle);
    //Timer_setup_callback(&timer_handle);

    Display_printf(display, 0, 0, "Starting the accelerometer readings\n");

    //print_N_accel_samples_timer(&adc0, &adc1, &adc2, &timer_handle);
    //float seconds = 1;
    //print_accel_samples_seconds(&adc0, &adc1, &adc2, &timer_handle, seconds);
    //print_accel_forever_timer(&adc0, &adc1, &adc2, &timer_handle, csv);
    print_accel_forever_timer_detection(&adc0, &adc1, &adc2, &timer_handle);


    ADC_close(adc0);
    ADC_close(adc1);
    ADC_close(adc2);

    Display_printf(display, 0, 0, "Ending the acdsinglechannel example\n");


    return (NULL);
}
