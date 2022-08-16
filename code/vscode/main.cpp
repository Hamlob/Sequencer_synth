#include "mbed.h"
#include "TextLCD.h"

#define NOTE2FREQ(note) (440 * pow(2, ((note - 58) / 12.0f)))                   //58 is the position of A4==440HZ

PwmOut pwm(p25);
AnalogOut Aout(p18);
AnalogIn Ain1(p19);
AnalogIn Ain2(p20);
DigitalIn Din1(p30);                                //play/stop
DigitalIn Din2(p27);                                //next
DigitalIn Din3(p26);                                //prev
DigitalIn EncA(p29);
DigitalIn EncB(p28);

BusOut LEDS(p21, p22, p23, p24);
TextLCD lcd(p9, p10, p11, p12, p13, p14, TextLCD::LCD16x2);                     // 4bit bus: rs, e, d4-d7 
Ticker TimerInt;

float step=0.005;                                               
float t=20;                       //time counter for sequence playing, starts at 20 to immediately trigger the first note of sequence
int c=0;                                                                        //counter for reduced step size for refreshing the bpm value on LCD
int pos=0;                                                                     //position of the note in sequence (0-15)
int bpm=0;
int pulses;                                                                     //recorded pulses from encoder
bool play=0;
bool play_old, next_old, prev_old=0;                                            //stored values of inputs from previous cycle
bool encA_old=1;
const char text[6]={'B', 'P', 'M', ':', ' ', '\0'};
const char seq_init[17]={'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o', 'o','\0'};
char seq[16]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};                              //char array that holds values corresponding to the set notes.

//0-3.3V range with resolution of 256 (char)= 3.3/256= 128.9mV. 
//DAC is 10 bit (1024 steps) so 128.9mV is exactly a multiple of DAC step size (3.3V/1024).
//However 256/12 is 21 octaves, so we can increase the step for better accuracy. We do want to retain 1024/steps, to be an int.
//If we take 128 steps, we get over 10 octaves, and we can use the unused char values (128-255) to represent things such as note off.


void display_init(){
    lcd.setAddress(0, 0);                                  
    lcd.printf(text);
    lcd.setAddress(0, 1);
    lcd.printf(seq_init);
    }
    

void update_bpm(int bpm){
    char buffer[4];                                 //bpm is maximum 3 digits + terminating character = 4 bytes     
    if (bpm<10){
        snprintf(buffer, 4, "  %d", bpm);
        }
    else if (bpm<100){
        snprintf(buffer, 4, " %d", bpm);
        }
    else {
        snprintf(buffer, 4, "%d", bpm);
        }
    lcd.setAddress(5,0);                                                                                                                                                                                                                                                                                                                                                                                                                                                            
    lcd.printf(buffer);
    }


void update_seq(int p){
    if (p==0){
        lcd.setAddress(15,1);
        lcd.printf("o");
        lcd.setAddress(0,1);
        lcd.printf("x");
        pos=0;
        }
    else {
        lcd.setAddress(pos-1,1);
        lcd.printf("ox");                       //rewrites adjacent elements to clear any x
        }
    }


void display_note(char value){
    char note[4];
    if (value==0) {
        note[0]='O';                              //snprintf(note,4,"OFF");
        note[1]='F';
        note[2]='F';
        note[3]='\0';
        }
    else {
        int oct=value/12;                           //rounded to the nearest lower int, represent the octave
        int rem=(value%12);                         //multiples of 12 represent C in different octaves, starting at c0=1, c1=13, c2=25... 
        switch(rem){                                //Remainder when divided by 12, gives us 1+d, where d is the distance from C in semitones
            case 1:  snprintf(note,4," C%d",oct);
                     break;
            case 2:  snprintf(note,4,"#C%d",oct);
                     break;
            case 3:  snprintf(note,4," D%d",oct);
                     break;
            case 4:  snprintf(note,4,"#D%d",oct);
                     break;
            case 5:  snprintf(note,4," E%d",oct);
                     break;
            case 6:  snprintf(note,4," F%d",oct);
                     break;
            case 7:  snprintf(note,4,"#F%d",oct);
                     break;
            case 8:  snprintf(note,4," G%d",oct);
                     break;
            case 9:  snprintf(note,4,"#G%d",oct);
                     break;
            case 10: snprintf(note,4," A%d",oct);
                     break;
            case 11: snprintf(note,4,"#A%d",oct);
                     break;
            case 0: snprintf(note,4," B%d",oct-1);
                     break;
            }
        }
        lcd.setAddress(13,0);
        lcd.printf(note);
    }                                              

void update_pulses(void){
    if (EncA!=encA_old){
                if (EncA==0 and EncB==0){
                    pulses=pulses+1;
                    }
                else if (EncA==0 and EncB==1){
                    pulses=pulses-1;
                    }
                }                    
    }

int main() {
    TimerInt.attach(&update_pulses, 0.003); // setup ticker to call flip at 1Hz
    display_init();
    display_note(seq[pos]);
    while(1){
        if(Din1!=play_old){
            if (Din1==1){                       //waits until btn is pressed (holding button does not do anything since btn==btn_old)
                play=!play;
                display_init();
                pos=0;
                }
            play_old=Din1;
            }    
        if (c==20){                                                 //bpm value check and update
            bpm=(int)(200*Ain1+0.5)+40;
            update_bpm(bpm);
            c=0;
            }
        if (play==1){                                               // play mode
            if (t>15.0/bpm){                                        //15.0 because quotient of two INT is an int and 15 because there are four 16 notes in one beat
                if (pos>15){
                    pos=0;
                    }
                pwm.period(1/NOTE2FREQ(seq[pos]));
                if (seq[pos]==0){
                    pwm.write(0);
                    }
                else {
                    pwm.write(0.5);
                    }
                //Aout.write_u16((unsigned short)(516*seq[pos]));   //analog out using unsigned short value. Range 0-65535. To scale our char ->65535/127=516.
                update_seq(pos);
                display_note(seq[pos]);
                LEDS=pos;
                pos=pos+1;
                t=0;
                }
            t=t+step;
            }
        else{
            pwm.write(0);                                               //edit mode
            if (Din2!=next_old){                            //check if there is a change on digital input
                if (Din2==1){                              //check  the input value
                    if (pos==15){                               
                        pos=0;
                        }
                    else{
                        pos=pos+1;
                        }
                    }
                next_old=Din2;
                LEDS=pos;
                }                                           
            else if (Din3!=prev_old){
                if (Din3==1){ 
                    if (pos==0){
                        pos=15;
                        }
                    else {
                        pos=pos-1;
                        }
                    }
                prev_old=Din3;
                LEDS=pos;
                }
            seq[pos]=seq[pos]+pulses;
            if (seq[pos]>127){
                seq[pos]=128;
                }
            else if (seq[pos]<1){
                seq[pos]=0;
                }
            pulses=0;
            display_note(seq[pos]);
            lcd.setAddress(pos,1);
            }
        c=c+1;
        wait(step);
             
    }

}

