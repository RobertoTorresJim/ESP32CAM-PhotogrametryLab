//>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>> ESP32_CAM_Send_Photo_to_Google_Drive

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// WARNING!!! Make sure that you have either selected Ai Thinker ESP32-CAM or ESP32 Wrover Module or another board which has PSRAM enabled.                   //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Reference :                                                                                                                                                //
// - esp32cam-gdrive : https://github.com/gsampallo/esp32cam-gdrive                                                                                           //
// - ESP32 CAM Send Images to Google Drive, IoT Security Camera : https://www.electroniclinic.com/esp32-cam-send-images-to-google-drive-iot-security-camera/  //
// - esp32cam-google-drive : https://github.com/RobertSasak/esp32cam-google-drive                                                                             //
//                                                                                                                                                            //
// When the ESP32-CAM takes photos or the process of sending photos to Google Drive is in progress, the ESP32-CAM requires a large amount of power.           //
// So I suggest that you use a 5V power supply with a current of approximately 2A.                                                                            //
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

//======================================== Including the libraries.
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "Base64.h"
#include "esp_camera.h"
#include "esp_timer.h"
#include "img_converters.h"
#include "Arduino.h"
#include "driver/rtc_io.h"
#include <ESPAsyncWebServer.h>
#include <SPIFFS.h>
#include <FS.h>
//======================================== 



// Photo File Name to save in SPIFFS
#define FILE_PHOTO "/photo.jpg"

//======================================== CAMERA_MODEL_AI_THINKER GPIO.
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27

#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
//======================================== 

// LED Flash PIN (GPIO 4)
#define FLASH_LED_PIN 4
#define START_LED_PIN 33


// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
camera_fb_t * fb;
boolean takeNewPhoto = false;
boolean savePhoto = false;
//======================================== Enter your WiFi ssid and password.
const char* ssid = "INFINITUM84E7_2.4";//"moto g(60)_Rob";
const char* password = "J6yccyCW2C";//"Pandeelote";
//======================================== 
const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { text-align:center; 
    	font-family: Verdana, sans-serif;
        }
    .vert { margin-bottom: 10%; }
    .hori{ margin-bottom: 0%; }
    .button {
      background-color: #008CBA; /* Blue */
      border: none;
      color: white;
      padding: 20px;
      text-align: center;
      text-decoration: none;
      display: inline-block;
      font-size: 12px;
      margin: 4px 2px;
      cursor: pointer;
      border-radius: 4px;
    }
    div.gallery {
  	  margin: 5px;
  	  border: 1px solid #ccc;
  	  float: left;
  	  width: 180px;
	}

    div.gallery:hover {
  	  border: 1px solid #777;
	}

	div.gallery img {
  	  width: 100%;
  	  height: auto;
	}

	div.desc {
  	  padding: 15px;
      text-align: center;
	}
  </style>
</head>
<body>
  <div id="container">
    <h2>ESP32 Photogrametry Lab</h2>
    <p>Photogrametry lab for one camera only...</p>
    <p>
      <button class ="button" onclick="rotatePhoto();">ROTATE</button>
      <button class ="button" onclick="capturePhoto()">TAKE PHOTO</button>
      <button class ="button" onclick="saveImages();">SAVE</button>
    </p>
  </div>
  <div class="gallery">
  	<a target="_blank" href="saved-photo">
    	<img src="saved-photo" id="photo" alt="No picture yet" width="600" height="400">
  	</a>
  	<div class="desc">Cam 1</div> 
  </div>
  <div class="gallery">
  	<a target="_blank" href="saved-photo">
    	<img src="saved-photo" alt="No picture yet" width="600" height="400">
  	</a>
  	<div class="desc">Cam 1</div> 
  </div>
</body>
<script>
  var deg = 0;
  function capturePhoto() {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/capture", true);
    xhr.send();
    setTimeout(function(){
    window.location.reload(1);
    }, 5000);
  }
  function rotatePhoto() {
    var img = document.getElementById("photo");
    deg += 180;
    img.style.transform = "rotate(" + deg + "deg)";
  }
  function saveImages(){
    var xhr = new XMLHttpRequest();
    xhr.open('GET', "/save-image", true);
    xhr.send();
  }
  function loadPhoto(){
    var xhr = new XMLHTTPRequest();
    xhr.open(GET , "/load", true);
    xhr.send();
  }
  function isOdd(n) { return Math.abs(n % 2) == 1; }
</script>
</html>)rawliteral";

//======================================== 

//======================================== Replace with your "Deployment ID" and Folder Name.
String myDeploymentID = "AKfycbzqj6h87q81rVIxg6-noaZvqQFwVFO5vpvfmwPxC9JlgeXEYQ_-fLyisycA31jxk0lHqQ";
String myMainFolderName = "ESP32-CAM";
//======================================== 

//======================================== Variables for Timer/Millis.
unsigned long previousMillis = 0; 
const int Interval = 20000; //--> Capture and Send a photo every 20 seconds.

//======================================== 

// Variable to set capture photo with LED Flash.
// Set to "false", then the Flash LED will not light up when capturing a photo.
// Set to "true", then the Flash LED lights up when capturing a photo.
bool LED_Flash_ON = true;

// Initialize WiFiClientSecure.
WiFiClientSecure client;

//________________________________________________________________________________ Test_Con()
// This subroutine is to test the connection to "script.google.com".
void Test_Con() {
  const char* host = "script.google.com";
  while(1) {
    Serial.println("-----------");
    Serial.println("Connection Test...");
    Serial.println("Connect to " + String(host));
  
    client.setInsecure();
  
    if (client.connect(host, 443)) {
      Serial.println("Connection successful.");
      Serial.println("-----------");
      client.stop();
      break;
    } else {
      Serial.println("Connected to " + String(host) + " failed.");
      Serial.println("Wait a moment for reconnecting.");
      Serial.println("-----------");
      client.stop();
    }
  
    delay(1000);
  }
}
//________________________________________________________________________________
void takePhoto(void){
  bool ok = 0;
    //---------------------------------------- The Flash LED blinks once to indicate start.
  digitalWrite(FLASH_LED_PIN, HIGH);
  delay(100);
  
  //---------------------------------------- 
  //.............................. Taking a photo.
    Serial.println();
    Serial.println("Taking a photo...");
    
    for (int i = 0; i <= 3; i++) {
      fb = NULL;
      fb = esp_camera_fb_get();
      Serial.println("Enter For: ");
       if(!fb) {
          Serial.println("Camera capture failed");
          Serial.println("Restarting the ESP32 CAM.");
          delay(1000);
          ESP.restart();
          return;
        }
        if(i == 3){
          // Photo file name
          Serial.println("Enter if when i is: ");
          Serial.printf("Picture file name: %s\n", FILE_PHOTO);
          File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

          // Insert the data in the photo file
          if (!file) {
            Serial.println("Failed to open file in writing mode");
          }
          else {
            file.write(fb->buf, fb->len); // payload (image), payload length
            Serial.print("The picture has been saved in ");
            Serial.print(FILE_PHOTO);
            Serial.print(" - Size: ");
            Serial.print(file.size());
            Serial.println(" bytes");
          }
          // Close the file
          file.close();
          // check if file has been correctly saved in SPIFFS
          ok = checkPhoto(SPIFFS);
          if(!ok){ i = 0;}
        }
      esp_camera_fb_return(fb);
      delay(200);
    }
  
    camera_fb_t * fb = NULL;
    fb = esp_camera_fb_get();
    if(!fb) {
      Serial.println("Camera capture failed");
      Serial.println("Restarting the ESP32 CAM.");
      delay(1000);
      ESP.restart();
      return;
    } 
  
    if (LED_Flash_ON == true) digitalWrite(FLASH_LED_PIN, LOW);
    
    Serial.println("Taking a photo was successful.");
}
//________________________________________________________________________________ 

void capturePhotoSaveSpiffs( void ) {//DEPRECATED
  camera_fb_t * fb = NULL; // pointer
  bool ok = 0; // Boolean indicating if the picture has been taken correctly

  do {
    // Take a photo with the camera
    Serial.println("Taking a photo...");

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      return;
    }

    // Photo file name
    Serial.printf("Picture file name: %s\n", FILE_PHOTO);
    File file = SPIFFS.open(FILE_PHOTO, FILE_WRITE);

    // Insert the data in the photo file
    if (!file) {
      Serial.println("Failed to open file in writing mode");
    }
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.print("The picture has been saved in ");
      Serial.print(FILE_PHOTO);
      Serial.print(" - Size: ");
      Serial.print(file.size());
      Serial.println(" bytes");
    }
    // Close the file
    file.close();
    esp_camera_fb_return(fb);

    // check if file has been correctly saved in SPIFFS
    ok = checkPhoto(SPIFFS);
  } while ( !ok );
}

//________________________________________________________________________________ SendCapturedPhotos()
// Subroutine for capturing and sending photos to Google Drive.
//https://script.google.com/macros/s/AKfycbx7wr_pp12oKHClRYP-KAn-FLmAj_tpPcazfbKn_Eb8cKEoKRB2F3a8NEu8snTnEaDcLQ/exec
void SendCapturedPhotos() {
  const char* host = "script.google.com";
  Serial.println();
  Serial.println("-----------");
  Serial.println("Connect to " + String(host));
  
  client.setInsecure();



  //---------------------------------------- The process of connecting, capturing and sending photos to Google Drive.
  if (client.connect(host, 443)) {
    Serial.println("Connection successful.");
    
    if (LED_Flash_ON == true) {
      digitalWrite(FLASH_LED_PIN, HIGH);
      delay(100);
    }

    //.............................. Sending image to Google Drive.
    Serial.println();
    Serial.println("Sending image to Google Drive.");
    Serial.println("Size: " + String(fb->len) + "byte");
    
    //https://script.google.com/macros/s/AKfycbx7wr_pp12oKHClRYP-KAn-FLmAj_tpPcazfbKn_Eb8cKEoKRB2F3a8NEu8snTnEaDcLQ/exec
    String url = "/macros/s/" + myDeploymentID + "/exec?folder=" + myMainFolderName;

    client.println("POST " + url + " HTTP/1.1");
    client.println("Host: " + String(host));
    client.println("Transfer-Encoding: chunked");
    client.println();

    int fbLen = fb->len;
    char *input = (char *)fb->buf;
    int chunkSize = 3 * 1000; //--> must be multiple of 3.
    int chunkBase64Size = base64_enc_len(chunkSize);
    char output[chunkBase64Size + 1];

    Serial.println();
    int chunk = 0;
    for (int i = 0; i < fbLen; i += chunkSize) {
      int l = base64_encode(output, input, min(fbLen - i, chunkSize));
      client.print(l, HEX);
      client.print("\r\n");
      client.print(output);
      client.print("\r\n");
      delay(100);
      input += chunkSize;
      Serial.print(".");
      chunk++;
      if (chunk % 50 == 0) {
        Serial.println();
      }
    }
    client.print("0\r\n");
    client.print("\r\n");

    esp_camera_fb_return(fb);
    //.............................. 

    //.............................. Waiting for response.
    Serial.println("Waiting for response.");
    long int StartTime = millis();
    while (!client.available()) {
      Serial.print(".");
      delay(100);
      if ((StartTime + 10 * 1000) < millis()) {
        Serial.println();
        Serial.println("No response.");
        break;
      }
    }
    Serial.println();
    while (client.available()) {
      Serial.print(char(client.read()));
    }
    //.............................. 

    //.............................. Flash LED blinks once as an indicator of successfully sending photos to Google Drive.
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(500);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(500);
    //.............................. 
  }
  else {
    Serial.println("Connected to " + String(host) + " failed.");
    
    //.............................. Flash LED blinks twice as a failed connection indicator.
    digitalWrite(START_LED_PIN, HIGH);
    delay(500);
    digitalWrite(START_LED_PIN, LOW);
    delay(500);
    digitalWrite(START_LED_PIN, HIGH);
    delay(500);
    digitalWrite(START_LED_PIN, LOW);
    delay(500);
    //.............................. 
  }
  //---------------------------------------- 

  Serial.println("-----------");
  client.stop();

}
//________________________________________________________________________________ 
// Check if photo capture was successful
bool checkPhoto( fs::FS &fs ) {
  File f_pic = fs.open( FILE_PHOTO );
  unsigned int pic_sz = f_pic.size();
  return ( pic_sz > 100 );
}
//________________________________________________________________________________ VOID SETUP()
void setup() {
  // put your setup code here, to run once:

  //=============================================

  // Disable brownout detector.
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);
  
  Serial.begin(115200);
  Serial.println();
  delay(1000);

  pinMode(FLASH_LED_PIN, OUTPUT);
  pinMode(START_LED_PIN, OUTPUT);
  // Setting the ESP32 WiFi to station mode.
  Serial.println();
  Serial.println("Setting the ESP32 WiFi to station mode.");
  WiFi.mode(WIFI_STA);

  //---------------------------------------- The process of connecting ESP32 CAM with WiFi Hotspot / WiFi Router.
  Serial.println();
  Serial.print("Connecting to : ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  if (!SPIFFS.begin(true)) {
    Serial.println("An Error has occurred while mounting SPIFFS");
    ESP.restart();
  }
  else {
    delay(500);
    Serial.println("SPIFFS mounted successfully");
  }
  // The process timeout of connecting ESP32 CAM with WiFi Hotspot / WiFi Router is 20 seconds.
  // If within 20 seconds the ESP32 CAM has not been successfully connected to WiFi, the ESP32 CAM will restart.
  // I made this condition because on my ESP32-CAM, there are times when it seems like it can't connect to WiFi, so it needs to be restarted to be able to connect to WiFi.
  int connecting_process_timed_out = 20; //--> 20 = 20 seconds.
  connecting_process_timed_out = connecting_process_timed_out * 2;
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    digitalWrite(FLASH_LED_PIN, HIGH);
    delay(250);
    digitalWrite(FLASH_LED_PIN, LOW);
    delay(250);
    if(connecting_process_timed_out > 0) connecting_process_timed_out--;
    if(connecting_process_timed_out == 0) {
      Serial.println();
      Serial.print("Failed to connect to ");
      Serial.println(ssid);
      Serial.println("Restarting the ESP32 CAM.");
      delay(1000);
      ESP.restart();
    }
  }

  digitalWrite(FLASH_LED_PIN, LOW);
  digitalWrite(START_LED_PIN, LOW);
  Serial.println();
  Serial.print("Successfully connected to ");
  Serial.println(ssid);
  //Serial.print("ESP32-CAM IP Address: ");
  //Serial.println(WiFi.localIP());
  //---------------------------------------- 
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(200, "text/html", index_html);
  });
  server.on("/capture", HTTP_GET, [](AsyncWebServerRequest * request) {
    takeNewPhoto = true;
    request->send(200, "text/plain", "Taking Photo");
  });
  server.on("/saved-photo", HTTP_GET, [](AsyncWebServerRequest * request) {
    request->send(SPIFFS, FILE_PHOTO, "image/jpg", false);
  });
  server.on("/save-image", HTTP_GET, [](AsyncWebServerRequest * request){
    savePhoto = true;
    request-> send(200, "text/plain", "Saving Photo");
  });
  server.on("/load", HTTP_GET, [](AsyncWebServerRequest * request){
    request->send(200,"text/html", index_html);
  });
//location.reload()
  //----------------------------------------
  server.begin();
  //---------------------------------------- Set the camera ESP32 CAM.
  Serial.println();
  Serial.println("Set the camera ESP32 CAM...");
  
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  
  // init with high specs to pre-allocate larger buffers
  if(psramFound()){
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 5;  //0-63 lower number means higher quality
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 5;  //0-63 lower number means higher quality
    config.fb_count = 1;
  }
  
  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    Serial.println();
    Serial.println("Restarting the ESP32 CAM.");
    delay(1000);
    ESP.restart();
  }

  sensor_t * s = esp_camera_sensor_get();

  // Selectable camera resolution details :
  // -UXGA   = 1600 x 1200 pixels
  // -SXGA   = 1280 x 1024 pixels
  // -XGA    = 1024 x 768  pixels
  // -SVGA   = 800 x 600   pixels
  // -VGA    = 640 x 480   pixels
  // -CIF    = 352 x 288   pixels
  // -QVGA   = 320 x 240   pixels
  // -HQVGA  = 240 x 160   pixels
  // -QQVGA  = 160 x 120   pixels
  s->set_framesize(s, FRAMESIZE_SXGA);  //--> UXGA|SXGA|XGA|SVGA|VGA|CIF|QVGA|HQVGA|QQVGA

  Serial.println("Setting the camera successfully.");
  Serial.println();

  delay(1000);

  Test_Con();

  Serial.println();
  Serial.println("Take photos and sendit to Google drive then sleep press Reset to take another...");
  Serial.println();
  delay(2000);



  //esp_deep_sleep_start();//SLEEP MODE OFF
}
//________________________________________________________________________________ 

//________________________________________________________________________________ VOID LOOP()
void loop() {
  // put your main code here, to run repeatedly:
  if (takeNewPhoto) {
    //capturePhotoSaveSpiffs();
    takePhoto();
    takeNewPhoto = false;
  }
  if (savePhoto){
    SendCapturedPhotos();
    savePhoto = false;
  }
  delay(1);
}
  //---------------------------------------- Timer/Millis to capture and send photos to Google Drive every 20 seconds (see Interval variable).
  //Serial.println("THIS WILL NEVER BE DISPLAYED");
  //---------------------------------------- 

