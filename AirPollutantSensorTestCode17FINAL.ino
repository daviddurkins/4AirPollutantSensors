//Code for Alphasense 4-Electrode ISB
//Written by David Durkin for Peter Weiss
//Started: June 26th 2019
//Version 1.7 (September 3rd,2019)

//This code will take the analong output from the Alphasense 4 Electrode ISBs and convert that value to ppb. The sensors themselves can withstand up to 1000 ppm concentration of pollutants,
//but have specific applications in measuring ppb up to around ~15ppm. The Analog pin outputs values between 0-1023, and conversion equation is used to determine the PPB from that.

#include <SPI.h>
#include <SD.h>                 //This is for the MicroSD card and...  
#include <Wire.h>               //this is for the RTC DS1307 clock, by adafruit
#include "RTClib.h"

//Specic data and calibration values for each sensor
//Carbon Monoxide Sensor CO-B4 SN:162482654
float ElecZero_CO_WE_mV = 324;  //the elctronic zero of the carbon monoxide working electrode. Basically the baseline for 0ppb, see write up for alternative values
float ElecZero_CO_AE_mV = 342;  //similarly , the electronic zero of the auxialary electrode, which is isolated from air to give unbiased values for stability
float CO_Sensitivity = 0.401;   //the sensitivity of the CO sensor in (mV/ppb)

//Nitrogen Diozide Sensor NO2-B43F SN:202481508
float ElecZero_NO2_WE_mV = 190;
float ElecZero_NO2_AE_mV = 192;
float NO2_Sensitivity = 0.227;

//Ozone + Nitrogen Dioxide Sensor OX-B431 SN:204660047
float ElecZero_OX_WE_mV = 221;
float ElecZero_OX_AE_mV = 222;
float OX_Sensitivity = 0.354;

//Nitric Oxide Sensor NO-B4 SN:160660809
float ElecZero_NO_WE_mV = 345;
float ElecZero_NO_AE_mV = 327;
float NO_Sensitivity = 0.471;

float Vin = 5074;          //This is the input voltage(mV), that we want to be stable, using bypass capacitors and a linear regulator. Do not use the Arduino as a voltage source.
int TimeInterval = 1000;   //Millisecond value of time between data samples
int TimeInSeconds = (TimeInterval / 1000);
int Trial = 0;
int i;
int ppb_CO[75];           //arrays holding multiple data samples, used in mode function to pick reoccuring value
int ppb_NO2[75];
int ppb_OX[75];
int ppb_NO[75];
char dataSD[100];           //array for SD card
char dataMonitor[100];      //array for serial monitor
int n = 75;               // number of samples taken to determine the mode in mode function
int pinCS = 53 ;          // Chip select allows us to write and read from two modules at the same time.
char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

enum STATES {REST,
             Sample,
             Display,
             Write,
            } states;

uint8_t state = REST;

// Singleton instance of RTC driver
RTC_DS1307 rtc;

// for the SD card Reader
File myFile;

void setup() {
  Serial.begin(115200);  // initialize serial communication at 9600 bits per second

  //Define analog pins as input readers for incoming voltage values
  pinMode(A15, INPUT); //CO WE
  pinMode(A14, INPUT); //CO AE
  pinMode(A13, INPUT); //NO2 WE
  pinMode(A12, INPUT); //NO2 AE
  pinMode(A11, INPUT); //OX WE
  pinMode(A10, INPUT); //OX AE
  pinMode(A9, INPUT);  //NO WE
  pinMode(A8, INPUT);  //NO AE

  pinMode(pinCS, OUTPUT);
  digitalWrite(pinCS, LOW);        //let the gate down to initialize the SD card

  if (SD.begin(53))                  //uses chip select pin 53
  {
    Serial.println("SD card is ready to use.");
    Serial.println("");
  } else
  {
    Serial.println("SD card initialization failed");
    Serial.println("");
  }

  if (! rtc.begin()) {                          //Now lets initialize the RTC PCF8523
    Serial.println("Couldn't find RTC");
  }

  //rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  if (! rtc.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }
}

void loop() {
  DateTime now = rtc.now();

  switch (state) {
    case REST: {
        //Start off Serial Monitor by stating the date and time
        Serial.print("Concentrations of Carbon Monoxide(CO), Nitrogen Dioxide(NO2), Ozone(OX), and Nitric Oxide(NO) on "); Serial.print(now.year()); Serial.print("/"); Serial.print(now.month()); Serial.print("/"); Serial.print(now.day()); Serial.print(" "); Serial.println(daysOfTheWeek[now.dayOfTheWeek()]);
        Serial.print("Trial ");  Serial.print("        Time(delta T = "); Serial.print(TimeInSeconds); Serial.println("s)        CO          NO2       OX        NO        ");
        delay(1000);
        state = Sample;
      }
      break;

    case Sample: {
        for (i = 0; i < n; i++) {                  //this for loop will take data points every 10ms, statistically it is redommended to have at least 50 data points to choose a mode or average from, we use n = 75
          delay(10);                      //Analog to Digital conversion can happen up to 10,000 times per second on the arduino, but a 10ms delay is easier on the machine.
          //Carbon Monoxide Part Per Billion Equaiotn and Reading----------------------------------------------------------------------------------------------------------------------------------------
          // read the input on analog pin A15, A14:
          int CO_WE_Value = analogRead(A15); //this is the Carbon Monoxide Working Eletrode value (0-1023)
          int CO_AE_Value = analogRead(A14); //this is the Auxialary electrode, which is not exposed to any air to be used as a reference for stability.

          // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5000mV), and correct by subtacting the electronic zero(mV) of each, giving us the real mV of each:
          float CO_WE_mV = (CO_WE_Value * (Vin / 1023.0)) - ElecZero_CO_WE_mV;
          float CO_AE_mV = (CO_AE_Value * (Vin / 1023.0)) - ElecZero_CO_AE_mV;

          //take the difference of the working and auxialry electrode voltages to get the actual voltage
          float dV_CO = CO_WE_mV - CO_AE_mV;

          //divide this value by sensitivity to get parts per billion (since dV is in mV and sensitivity is in (mV/ppb), so we work with the reciprical 1/sensitivity to get ppb/mV
          ppb_CO[i] = dV_CO / CO_Sensitivity;




          //Nitrogen Dioxide Part Per Billion Equaiotn and Reading-----------------------------------------------------------------------------------------------------------------------------------------
          // read the input on analog pin A13, A12:
          int NO2_WE_Value = analogRead(A13); //this is the Carbon Monoxide Working Eletrode value (0-1023)
          int NO2_AE_Value = analogRead(A12); //this is the Auxialary electrode, which is not exposed to any air to be used as a reference for stability.

          // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5000mV), and correct by subtacting the electronic zero(mV) of each, giving us the real mV of each:
          float NO2_WE_mV = (NO2_WE_Value * (Vin / 1023.0)) - ElecZero_NO2_WE_mV;
          float NO2_AE_mV = (NO2_AE_Value * (Vin / 1023.0)) - ElecZero_NO2_AE_mV;

          //take the difference of the working and auxialry electrode voltages to get the actual voltage
          float dV_NO2 = NO2_WE_mV - NO2_AE_mV;

          //divide this value by sensitivity to get parts per billion
          ppb_NO2[i] = dV_NO2 / NO2_Sensitivity;



          //Ozone + Nittogen Dioxide Part Per Billion Equaiotn and Reading---------------------------------------------------------------------------------------------------------------------------------
          // read the input on analog pin A11, A10:
          int OX_WE_Value = analogRead(A11); //this is the Carbon Monoxide Working Eletrode value (0-1023)
          int OX_AE_Value = analogRead(A10); //this is the Auxialary electrode, which is not exposed to any air to be used as a reference for stability.

          // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5000mV), and correct by subtacting the electronic zero(mV) of each, giving us the real mV of each:
          float OX_WE_mV = (OX_WE_Value * (Vin / 1023.0)) - ElecZero_OX_WE_mV;
          float OX_AE_mV = (OX_AE_Value * (Vin / 1023.0)) - ElecZero_OX_AE_mV;

          //take the difference of the working and auxialry electrode voltages to get the actual voltage
          float dV_OX = OX_WE_mV - OX_AE_mV;

          //divide this value by sensitivity to get parts per billion
          ppb_OX[i] = dV_OX / OX_Sensitivity;




          //Nitric Oxide Part Per Billion Equaiotn and Reading-----------------------------------------------------------------------------------------------------------------------------------------
          // read the input on analog pin A9, A8:
          int NO_WE_Value = analogRead(A9); //this is the Carbon Monoxide Working Eletrode value (0-1023)
          int NO_AE_Value = analogRead(A8); //this is the Auxialary electrode, which is not exposed to any air to be used as a reference for stability.

          // Convert the analog reading (which goes from 0 - 1023) to a voltage (0 - 5000mV), and correct by subtacting the electronic zero(mV) of each, giving us the real mV of each:
          float NO_WE_mV = (NO_WE_Value * (Vin / 1023.0)) - ElecZero_NO_WE_mV;
          float NO_AE_mV = (NO_AE_Value * (Vin / 1023.0)) - ElecZero_NO_AE_mV;

          //take the difference of the working and auxialry electrode voltages to get the actual voltage
          float dV_NO = NO_WE_mV - NO_AE_mV;

          //divide this value by sensitivity to get parts per billion
          ppb_NO[i] = dV_NO / NO_Sensitivity;
        }
        state = Display;
      }
      break;

    case Display: {
       //chooses the most common number of the 75 samples taken in .75sec
        int mode_ppb_CO = mode(ppb_CO, n);
        int mode_ppb_NO2 = mode(ppb_NO2, n);
        int mode_ppb_OX = mode(ppb_OX, n);
        int mode_ppb_NO = mode(ppb_NO, n);

        //Now lets store the data on the SD card with a time stamp and repeat the process
        Trial = Trial + 1;          //increments trial number each time
        sprintf(dataMonitor, "  %d               %d:%d:%d              %d        %d        %d        %d        (ppb)     ", Trial, now.hour(), now.minute(), now.second(), mode_ppb_CO, mode_ppb_NO2, mode_ppb_OX, mode_ppb_NO);
        Serial.print(dataMonitor);
        sprintf(dataSD, "%d, %d, %d, %d, %d, %d/%d/%d, %d:%d:%d", Trial, mode_ppb_CO, mode_ppb_NO2, mode_ppb_OX, mode_ppb_NO , now.month(), now.day(), now.year(), now.hour(), now.minute(), now.second());
        //data to SD card is deliminated by a comma. As follows, it is "Trial, ppb CO, ppb NO2, ppb OX, ppb NO, date (m/d/y), time (h:m:s)", link to how to export to excel https://spreadsheeto.com/text-to-columns/

        int remainingtime = TimeInterval - ((n * 10) + 100) ;   //ignoring run time, this will subtract the time it takes to collect data samples and how long between displaying data.
        delay(remainingtime);
        state = Write;
      }
      break;

    case Write: {
        myFile = SD.open("file.txt", FILE_WRITE);

        if (myFile) {
          myFile.println(dataSD);     // Write to file
          myFile.close();             // close the file
          Serial.println("Saved.");
        }
        else {
          Serial.println("error opening PPB_Air_Pollutants.txt"); // if the file didn't open, print this error
        }
        state = Sample;
      }
      break;
  }
}

//Mode function to find the most common value out of n trails to display and store, instead of average that looses resolution intervals
int mode(int a[], int n) {
  int maxValue = 0, maxCount = 0, i, j;
  for (i = 0; i < n; ++i) {
    int count = 0;
    for (j = 0; j < n; ++j) {
      if (a[j] == a[i])
        ++count;
    }
    if (count > maxCount) {
      maxCount = count;
      maxValue = a[i];
    }
  }
  return maxValue;
}
