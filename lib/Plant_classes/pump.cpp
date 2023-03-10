#include <Arduino.h>
#include <RTClib.h>
#include <SPI.h>
#include "Plant.h"

uint8_t     Pump::Output_Port;
uint8_t     Pump::LED_Pump = 0;
uint8_t     Pump::Valve1_Port = 0;
uint8_t     Pump::Valve2_Port = 0;
uint8_t     Pump::Valve3_Port = 0;

bool        Pump::M_Control_active = false;
DateTime    Pump::CurrentTime;
DateTime    Pump::Prev_Sec;
bool        Pump::Pump_availaible = true;
uint8_t     Pump::PrescalerRamp = 0;
uint16_t    Pump::Counter_MS = 0;

uint8_t     Pump::Waterspeed = 0;
uint32_t    Pump::Wateringtime_ms = 0;
DateTime    Pump::Start_Time;

uint8_t     Pump::Valve1_Condition = 0;
uint8_t     Pump::Valve2_Condition = 0;
uint8_t     Pump::Valve3_Condition = 0;
uint8_t     Pump::Waterspeed_out = 0;

void Pump::Setup(uint8_t Output_Port, uint8_t LED_Pump, uint8_t Valve1_Port, uint8_t Valve2_Port, uint8_t Valve3_Port){

    Pump_availaible = true;

    if(!(Output_Port==0))
    {
        Pump::Output_Port = Output_Port;
    }
    if(!(LED_Pump==0))
    {
        Pump::LED_Pump = LED_Pump;
    }
    if(!(Valve1_Port==0))
    {
        Pump::Valve1_Port = Valve1_Port;
    }
    if(!(Valve2_Port==0))
    {
        Pump::Valve2_Port = Valve2_Port;
    }
    if(!(Valve3_Port==0))
    {
        Pump::Valve3_Port = Valve3_Port;
    }
    Serial.println("Setup pump done!\n");
}

void Pump::OutputControl(){
    if(Valve1_Condition)  {digitalWrite(Valve1_Port,LOW);}
    else        {digitalWrite(Valve1_Port,HIGH);}

    if(Valve2_Condition)  {digitalWrite(Valve2_Port,LOW);}
    else        {digitalWrite(Valve2_Port,HIGH);}

    if(Valve3_Condition)  {digitalWrite(Valve3_Port,LOW);}
    else        {digitalWrite(Valve3_Port,HIGH);}

    analogWrite(Output_Port, Waterspeed_out);

    if(Waterspeed_out>0)
        digitalWrite(LED_Pump,LOW);
    else
        digitalWrite(LED_Pump,HIGH);
}


void Pump::Start_Water_Process(uint8_t Waterspeed, uint32_t Wateringtime_ms, uint8_t Valve){
    Pump::Wateringtime_ms = Wateringtime_ms + 1;
    Pump::Waterspeed = Waterspeed;
    Pump::Pump_availaible = false;
    Pump::Start_Time = CurrentTime;

    if(Valve==0){Valve1_Condition=0;Valve2_Condition=0;Valve3_Condition=0;}
    if(Valve==1){Valve1_Condition=1;Valve2_Condition=0;Valve3_Condition=0;}
    if(Valve==2){Valve1_Condition=0;Valve2_Condition=1;Valve3_Condition=0;}
    if(Valve==3){Valve1_Condition=0;Valve2_Condition=0;Valve3_Condition=1;}
}

void Pump::Ramp(){

    //Schleife wird einmal pro Millisekunde durchlaufen und desto h??her der Prescaler, 
    //desto seltener wird die PWM korrigiert.
    if(PrescalerRamp<=PRESCALER_RAMP)
        PrescalerRamp++;
    else
        PrescalerRamp = 0;

    //Wenn eine Bew??sserungszeit sowie Startzeit gesetzt wurden, beginnt die Rampe mit der Erh??hung 
    //der Geschwindigkeit bis zur Vorgabe. Faktor 1000 wird zur umrechnung auf Millisekunden ben??tigt.
    if(((Start_Time.secondstime()*1000) + Wateringtime_ms) > (CurrentTime.secondstime()*1000)+Counter_MS)
    {    
        if(Waterspeed_out<Waterspeed && PrescalerRamp==0)
        {
            Waterspeed_out++;
        }
        //Im Fehlerfall korrektur der PWM
        else if (Waterspeed_out>Waterspeed)
        {
            Waterspeed_out--;
        }
        
        //Counter_MS z??hlt immer von 0-999 um Millisekunden zu generieren
        if(!(CurrentTime.secondstime()==Prev_Sec.secondstime()))
        {
            Counter_MS = 0;
            Prev_Sec = CurrentTime.secondstime();
        }
        Counter_MS++;       
    }
    //Wenn Bew??sserungszeit erreicht, PWM reduzieren und bei 0 Pumpe sowie Hilfsvariablen Freigeben.
    else
    {
        if(Waterspeed_out>0 && PrescalerRamp==0)
        {
            Waterspeed_out--;
        }
        if(Waterspeed_out == 0)
        {
            Wateringtime_ms = 0;
            Waterspeed = 0;
            Pump_availaible = true;
            Valve1_Condition = 0;
            Valve2_Condition = 0;
            Valve3_Condition = 0;
            M_Control_active = false;
        }
    }
}

void Pump::Activation(DateTime CurrentTime_input, Plantlist *Pflanzenliste){

    Pump::CurrentTime = CurrentTime_input;

    if(Pump::Pump_availaible==false)
    {Pump::Ramp();}

    OutputControl(); 


    if(CurrentTime.hour() == 0
    && CurrentTime.minute() == 0
    && CurrentTime.second() == 0)
    {
        for(int i = 0; i<=Pflanzenliste->Plant_counter; i++)
        {
            Pflanzenliste->Plantlist_ARRAY[i].Watered_Today = false;
        }
    }
    else{
        for(int i = 0; i<=Pflanzenliste->Plant_counter; i++)
        {
            DateTime Plant_Water_Time = {CurrentTime.year(), CurrentTime.month(), CurrentTime.day(), Pflanzenliste->Plantlist_ARRAY[i].W_Hour,Pflanzenliste->Plantlist_ARRAY[i].W_Minute,1};
            
            if(Plant_Water_Time.secondstime()<=CurrentTime.secondstime()
            && Plant_Water_Time.secondstime()>=(CurrentTime.secondstime()-60)
            && Pflanzenliste->Plantlist_ARRAY[i].Watered_Today == false)
            {
                Pflanzenliste->Plantlist_ARRAY[i].NeedsToBeWatered = true;
                Pflanzenliste->Plantlist_ARRAY[i].Watered_Today = true;
            }

        }

    }

}

void Pump::ControlTask(){

    Serial.printf("Valve1       = %d            Valve2          = %d            Valve3          = %d\n", Valve1_Condition, Valve2_Condition, Valve3_Condition);
    Serial.printf("Pumpe_Out    = %d           Starttime       = %d   Wateringtime    = %d\n", Waterspeed_out, Start_Time.secondstime(), Wateringtime_ms);
    Serial.printf("Current Time = %d  Differenztime   = %d  M_Control activ = %d\n\n",CurrentTime.secondstime(), ((Start_Time.secondstime()*1000) + Wateringtime_ms) - (CurrentTime.secondstime()*1000),M_Control_active);
}
