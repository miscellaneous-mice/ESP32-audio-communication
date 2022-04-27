#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <WiFiAP.h>


#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "FS.h"
#include "SD.h"
#include "SPI.h"
#include <driver/i2s.h>

#define BUFFER_SIZE (32*1024)
#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 32
#define NUM_CHANNELS 2
#define BYTE_RATE (SAMPLE_RATE * (BITS_PER_SAMPLE / 8)) * NUM_CHANNELS
const i2s_port_t I2S_PORT = I2S_NUM_0;
const int WAVE_HEADER_SIZE = 44;


int START_CONDITION = 0;            //to check if the recording should be started
int STOP_CONDITION = 0;             //to check if recording should be stopped
int flash_wr_size = 0;
//int TRANSFER_CONDITION = 0;
//const char* PARAM_MESSAGE = "message";


bool keepRecording = false;

const char* ssid = "LiSpire1";        //Write your own Wi-Fi name here
const char* password = "aaaaa11111";  //Write your own password here

#define RED    26 
#define GREEN  13
#define BLUE   21

AsyncWebServer server(80);   //object created on default port 80

void initmicroSDCard(){
  if(!SD.begin()){
    Serial.println("Initialization Failed");
    return;
  }
  uint8_t cardType = SD.cardType();

  if(cardType == CARD_NONE){
    Serial.println("SD card is not present!");
    return;
  }

  Serial.print("SD Card Type: ");
  if(cardType == CARD_MMC){
    Serial.println("MMC");
  } else if(cardType == CARD_SD){
    Serial.println("SDSC");
  } else if(cardType == CARD_SDHC){
    Serial.println("SDHC");
  } else {
    Serial.println("UNKNOWN");
  }
  uint64_t cardSize = SD.cardSize() / (1024 * 1024);
  Serial.printf("SD Card Size: %lluMB\n", cardSize);
}

void generate_wav_header(char* wav_header, uint32_t wav_size, uint32_t sample_rate){
    // See this for reference: http://soundfile.sapp.org/doc/WaveFormat/
    uint32_t file_size = wav_size + WAVE_HEADER_SIZE - 8;
    uint32_t byte_rate = BYTE_RATE;

    const char set_wav_header[] = {
        'R','I','F','F', // ChunkID
        file_size, file_size >> 8, file_size >> 16, file_size >> 24, // ChunkSize
        'W','A','V','E', // Format
        'f','m','t',' ', // Subchunk1ID
        0x10, 0x00, 0x00, 0x00, // Subchunk1Size (16 for PCM)
        0x01, 0x00, // AudioFormat (1 for PCM)
        0x02, 0x00, // NumChannels (2 channel)
        sample_rate, sample_rate >> 8, sample_rate >> 16, sample_rate >> 24, // SampleRate
        byte_rate, byte_rate >> 8, byte_rate >> 16, byte_rate >> 24, // ByteRate
        0x02, 0x00, // BlockAlign
        0x20, 0x00, // BitsPerSample (16 bits)
        'd','a','t','a', // Subchunk2ID
        wav_size, wav_size >> 8, wav_size >> 16, wav_size >> 24, // Subchunk2Size
    };

    memcpy(wav_header, set_wav_header, sizeof(set_wav_header));
}

void record(void*)
{
   // Use POSIX and C standard library functions to work with files.
//    int flash_wr_size = 0;
    Serial.println("Opening file");

    char wav_header_fmt[WAVE_HEADER_SIZE];
    
    // Write out empty header
    memset(wav_header_fmt, 0, WAVE_HEADER_SIZE);

    // First check if file exists before creating a new file.
    if (SD.exists("/record.wav")) {
        // Delete it if it exists
        SD.remove("/record.wav");
    }

    // Create new WAV file
    File f = SD.open("/record.wav", FILE_WRITE);
    if (!f) {
        Serial.println("Failed to open file for writing");
        return;
    }

    // Write the header to the WAV file
    f.write((const uint8_t *)wav_header_fmt, WAVE_HEADER_SIZE);

    static char i2s_readraw_buff[BUFFER_SIZE];
    size_t bytes_read;
    // Start recording
    keepRecording = true;
    while (keepRecording) {
      if(flash_wr_size<3500000)
      {
//    Serial.printf("Stations connected = %d\n", WiFi.softAPgetStationNum());
        if(WiFi.softAPgetStationNum())
        {
          // Read the RAW samples from the microphone
          i2s_read(I2S_PORT, (char *)i2s_readraw_buff, BUFFER_SIZE, &bytes_read, portMAX_DELAY);
          // Write the samples to the WAV file
          f.write((const uint8_t *)i2s_readraw_buff, bytes_read);
          flash_wr_size += bytes_read;
        }
        else
        {
          keepRecording = false;
        }
      }
      else
       {
         keepRecording = false;
       }
    }

    // Write out the header at the beginning
//    transfer_status();

//    Serial.println(flash_wr_size);
    generate_wav_header(wav_header_fmt, flash_wr_size, SAMPLE_RATE);
    f.seek(0);
    f.write((const unsigned char*)wav_header_fmt, WAVE_HEADER_SIZE);
    
    Serial.print("Recording done!");
    f.close();
    Serial.println(" File written on SDCard.");
    transfer_status();

    vTaskDelete(NULL);
}

void transfer_status()
{
  Serial.println("stopped recording");
  STOP_CONDITION = 1;
  digitalWrite(GREEN, LOW);
  flash_wr_size = 0;
  
}


void setup() {
  Serial.begin(115200);
  initmicroSDCard();
  pinMode(RED,   OUTPUT);     //indicates recording stop status
  pinMode(GREEN, OUTPUT);     //indicates recording stop status
  pinMode(BLUE,  OUTPUT);     //indicates recording transfer
  
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(BLUE, LOW);
  
  Serial.setDebugOutput(true);
  Serial.print("\n");
  Serial.print("Configuring I2S...");
  esp_err_t err;

  // The I2S config as per the example
  const i2s_config_t i2s_config = {
      .mode = i2s_mode_t(I2S_MODE_MASTER | I2S_MODE_RX), // Receive, not transfer
      .sample_rate = SAMPLE_RATE,                         // 16KHz
      .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT, // could only get it to work with 32bits
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, // although the SEL config should be left, it seems to transmit on right
      .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_I2S | I2S_COMM_FORMAT_I2S_MSB),
      .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,     // Interrupt level 1
      .dma_buf_count = 4,                           // number of buffers
      .dma_buf_len = 8                              // 8 samples per buffer (minimum)
  };

  // The pin config as per the setup
  const i2s_pin_config_t pin_config = {
      .bck_io_num = 14,   // BCKL
      .ws_io_num = 15,    // LRCL
      .data_out_num = -1, // not used (only for speakers)
      .data_in_num = 32   // DOUT
  };

  // Configuring the I2S driver and pins.
  // This function must be called before any I2S driver read/write operations.
  err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) {
    Serial.printf(" FAILED installing driver: %d\n", err);
  }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) {
    Serial.printf(" FAILED setting pin: %d\n", err);
  }
  Serial.println(" DONE");


  Serial.printf("Starting WiFi AP = %s", ssid);
  WiFi.softAP(ssid, password);
  Serial.println(" DONE");

server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");      //hosting files from sd card stored in www folder

server.on("/do/start", HTTP_GET, [](AsyncWebServerRequest *request){
      
    if((START_CONDITION == 0)||(STOP_CONDITION == 1))       //to check if we have multiple start requests
      {   
          START_CONDITION = 0;
          STOP_CONDITION = 0;
          digitalWrite(RED, LOW);

          START_CONDITION = START_CONDITION + 1;
      
          request->send(200, "text/plain", "Starting recording");

//          request->send(200, "text/plain", "Test message");

          digitalWrite(BLUE, LOW);
    
          digitalWrite(GREEN, HIGH);

          xTaskCreate(record, "start_recording", BUFFER_SIZE + 1000, NULL, 1, NULL);
       }

      else
      {
        request->send(200, "text/plain", "Already started recording");
      }
  });
  

server.on("/do/stop", HTTP_GET, [](AsyncWebServerRequest *request){
    
    if( START_CONDITION == 1 )             //to check if recording is started in order to stop recording
    {
      if( STOP_CONDITION == 0 )            //to check if we have multiple stop requests
      {
        STOP_CONDITION = STOP_CONDITION + 1;
        
        Serial.println("Recieved request to STOP recording...");
    
        digitalWrite(GREEN, LOW);
        digitalWrite(RED, HIGH);

        request->send(200, "text/plain", "Stopped recording");
  
        keepRecording = false;
//        dataname = "/record.wav";
//        datatype = "application/x-binary";

      }
      else
      {
        request->send(200, "text/plain", "Already stopped recording");
      }
    }

    else
    {
      request->send(200, "text/plain", "First start recording");
    }
  });
  
server.on("/transfer", HTTP_GET, [](AsyncWebServerRequest *request)
{
if( STOP_CONDITION == 1 )
{
        START_CONDITION = 0;
        digitalWrite(RED, LOW);
        digitalWrite(BLUE, HIGH);
        request->send(SD, "/record.wav", "application/x-binary");
}
else
{
  request->send(200, "text/plain", "First stop recording");
  
}   
});

 server.on("/transferack", HTTP_POST, [](AsyncWebServerRequest *request){          //used for feedback if transfer is finished

        String receive =String(16);
        receive = request->arg("message");
        Serial.println(receive);
        
   if( receive == "done")
   {
          size_t p;
          p = request->params();
          Serial.println("no. of params");
          Serial.println(p);
          request->send(200, "text/plain", "Transfer success");
          digitalWrite(BLUE, LOW);
   }
   else
   {
      request->send(200, "text/plain", "Did not transfer successfully");
   }
});


   server.onNotFound([](AsyncWebServerRequest *request){request->send(404);});


  
  server.serveStatic("/", SD, "/");

  server.begin();
}

void loop() {
  
}
