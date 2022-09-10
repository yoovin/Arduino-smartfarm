#include <ESP8266WiFi.h>
#include <SparkFun_RHT03.h>
#include <WiFiUdp.h>
#include <NTPClient.h>
#include <EEPROM.h>

const char* ssid     = ""; // 사용 중 인 와이파이 이름
const char* password = ""; // 와이파이 패스워드
WiFiServer server(80); // 서버

// NTP 서버시간
const char* ntpServer = "pool.ntp.org";
uint8_t timeZone = 9;
uint8_t summerTime = 0;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, ntpServer);

String formettedTime;
int year, month, day, hour, minute, second;


// Pin 선언
int motorPin[] = {D1, D2};
int ledPin = D3;
int submotorPin = D4;
int soilPin[] = {D5, D6};
int rhtPin = D7;

// 릴레이모듈을 사용하므로 HIGH가 꺼진상태
int isLedOn = 1; 
int motorOn[] = {1, 1};
int isSubmotorOn = 1;

// 온습도 선언
RHT03 rht;
float tempC;
float humidity;

// 수분차트
byte soilHumidity1[24] = {0};
byte soilHumidity2[24] = {0};

bool doItJustOnce = false;

// Log를 저장하기위한 배열
String logs[1000];
int logcount = 0;

int soilValue(int pin){
    for(int i = 0; i < 2; i++){
        if(i == pin){
            digitalWrite(soilPin[i], HIGH);
        }else{
            digitalWrite(soilPin[i], LOW);
        }
    }
    return analogRead(A0);
}

void writeLog(String text){
    if(logcount > 999) logcount = 0; // 로그 배열이 다 차면 다시 0으로 돌아감
    String currentTime = String(year) + "-" + String(month) + "-" + String(day) + " " + formettedTime;
    logs[logcount] = currentTime + " | " + text;
    logcount += 1;
}

void setup(){
    // 기본 설정
    writeLog("서버 켜짐");
    pinMode(A0,INPUT);
    pinMode(ledPin, OUTPUT);
    pinMode(submotorPin, OUTPUT);
    for(int i = 0; i < 2; i++){
        pinMode(motorPin[i], OUTPUT);
        pinMode(soilPin[i], OUTPUT);
    }
    rht.begin(rhtPin);
    EEPROM.begin(48);
    
    writeLog("핀 설정 및 RHT, EEPROM 시작 완료");

    // 습도 값 불러오기
    for(int i = 0; i < EEPROM.length(); i++){
        if(EEPROM.read(i) > 100) continue;
        if(i < 24){
            soilHumidity1[i] = EEPROM.read(i);
        }else{
            soilHumidity2[i - 24] = EEPROM.read(i);
        }
    }
    writeLog("EEPROM에 저장되어있는 습도 값 불러오기 완료");


    // 전체적으로 꺼주기
    digitalWrite(ledPin, isLedOn); // LED 켜고 끔
    digitalWrite(submotorPin, isSubmotorOn); // 물버림 모터 켜고 끔
    for(int i = 0; i < 2; i++){
        digitalWrite(motorPin[i], motorOn[i]);
    }


    Serial.begin(115200); // 시리얼 통신, 속도 115200
    delay(10);

  // 와이파이 연결
    writeLog("와이파이 연결 중");
    WiFi.mode(WIFI_STA);
    
    writeLog("와이파이" + String(ssid) + "에 연결");

    WiFi.begin(ssid, password);

    while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    Serial.print(".");
    }

    writeLog("와이파이 연결 완료");

    // 서버 시작
    server.begin();
    writeLog("서버 시작 됨");

    // server시간 가져오기
    timeClient.begin();
    timeClient.setTimeOffset(3600 * timeZone);
    timeClient.update();
    formettedTime = timeClient.getFormattedTime();
    hour = timeClient.getHours();
    minute = timeClient.getMinutes();
    second = timeClient.getSeconds();
    writeLog("NTP 시간 불러오기 완료");
}

void loop() {
    delay(50);
    WiFiClient client = server.available();
    timeClient.update(); // 시간 업데이트

    time_t epochTime = timeClient.getEpochTime();
    struct tm *ptm = gmtime ((time_t *)&epochTime); 

    formettedTime = timeClient.getFormattedTime();
    hour = timeClient.getHours();
    minute = timeClient.getMinutes();
    second = timeClient.getSeconds();

    year = ptm->tm_year+1900;
    month = ptm->tm_mon+1;
    day = ptm->tm_mday;

    // GET 요청
    String req = client.readStringUntil('\r');
    client.flush();

    if(req.indexOf("led/on") != -1){
        isLedOn = 0;
        writeLog("사용자로 인해 led 켜짐");
    } else if (req.indexOf("led/off") != -1){
        isLedOn = 1;
        writeLog("사용자로 인해 led 꺼짐");
    } else if (req.indexOf("motor/sub/on") != -1){
        isSubmotorOn = 0;
        writeLog("사용자로 인해 물빼기 모터 켜짐");
    }else if(req.indexOf("motor/sub/off") != -1){
        isSubmotorOn = 1;
        writeLog("사용자로 인해 물빼기 모터 꺼짐");
    }

    

    digitalWrite(ledPin, isLedOn); // LED 켜고 끔
    digitalWrite(submotorPin, isSubmotorOn); // 물버림 모터 켜고 끔

    int soilValues[2] = {0};
    int soilPercents[2] = {0};
    
    for(int i = 0; i < 2; i++){
        soilValues[i] = soilValue(i);
        delay(100);
    }

    for(int i = 0; i < 2; i++){
        if(soilValues[i] > 500){ // 물주는값
            writeLog("물주는값으로 인해 " + String(i+1) + "번 모터 켜짐");
            motorOn[i] = 0; // 모터켜짐
        }else{
            if(!motorOn[i]) writeLog("물주는값으로 인해 " + String(i+1) + "번 모터 꺼짐");
            motorOn[i] = 1; // 모터꺼짐
        }
        digitalWrite(motorPin[i], motorOn[i]);
    }
    
    for(int i = 0; i < 2; i++){
        soilPercents[i] = map(soilValues[i], 1024, 0, 0, 100);
    }

    if(timeClient.getMinutes() == 1 && !doItJustOnce){ // 정시에 한번만 실행 (실행시차를 맞추기위해 1분에 실행 함)
        if(!hour){ // 정각이라면
            // EEPROM 초기화
            for(int i = 0; i < EEPROM.length(); i++){
                EEPROM.write(i, 0);
            }
            writeLog("EEPROM 초기화");
        }

        if(hour >= 6 && hour <= 19){ // 낮시간동안 켜짐
            if(isLedOn){
                writeLog("시간 상 led 켜짐");
                isLedOn = 0;
            }
        }else{
            if(!isLedOn){ // 밤에는 꺼짐
                writeLog("시간 상 led 꺼짐");
                isLedOn = 1;
            }
        }

        // 습도 EEPROM에 저장
        // 0 ~ 23은 1번 수분
        EEPROM.write(hour, soilPercents[0]);
        soilHumidity1[hour] = soilPercents[0];
        // 24 ~ 47은 2번 수분
        EEPROM.write(hour + 24, soilPercents[1]);
        soilHumidity2[hour] = soilPercents[1];
        EEPROM.commit();
        doItJustOnce = true;

        writeLog("1번 수분 " + String(soilPercents[0]) + " EEPROM 기록");
        writeLog("2번 수분 " + String(soilPercents[1]) + " EEPROM 기록");
    }else if(timeClient.getMinutes() == 2 && doItJustOnce){ // 다음 정시에 다시 실행될 수 있게 값을 바꿔 줌
        doItJustOnce = false;
    }

    // 온습도 확인하기
    int updateRht = rht.update(); // RHT를 통해 온, 습도가 불러진 경우 1을 반환 함
    
    if(updateRht == 1){
        humidity = rht.humidity();
        tempC = rht.tempC();
        writeLog("온습도 업데이트 됨 온도: " + String(tempC) + "°C, 습도: " + String(humidity) + "%");
    }

    /*
    ====== HTML 선언부 ======
    */
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");
    client.println();
    client.println("<!DOCTYPE html>");
    client.println("<html xmlns='http://www.w3.org/1999/xhtml'>");
    client.println("<head>\n<meta http-equiv='Content-Type' content='text/html; charset=utf-8' />");
    client.println("<script src=\"https://cdn.tailwindcss.com\"></script>");
    client.println("<title>Smart Farm</title>"); // 웹 서버 페이지 제목 설정
    client.println("</head>");

    // body 태그 선언부
    client.println("<body>");
    client.println("<div class=\"text-8xl p-8 font-bold\"><a href=\"/\">Yoo Farm</a></div>");
    client.println("<div class=\"border-4 border-gray-800 rounded m-2 p-2\">");
    client.println("<div class=\"flex flex-row mb-6\">");
    client.println("<div class=\"basis-1/2 relative mb-6\">");

     // LED
    client.print("<span class=\"text-2xl\">LED: ");
    isLedOn
    ? client.println("<span class=\"text-2xl font-bold\">OFF</span>")
    : client.println("<span class=\"text-2xl font-bold text-green-400\">ON</span>");
    client.println("</span>");
    client.println("<div class=\"flex\">\
                    <div class=\"inline-flex shadow-md hover:shadow-lg focus:shadow-lg\" role=\"group\">\
                        <button type=\"button\" class=\"rounded-l inline-block px-6 py-2.5 bg-blue-600 text-white font-medium text-xs leading-tight uppercase hover:bg-blue-700 focus:bg-blue-700 focus:outline-none focus:ring-0 active:bg-blue-800 transition duration-150 ease-in-out\" onclick=\"location.href='/led/on'\">On</button>\
                        <button type=\"button\" class=\" rounded-r inline-block px-6 py-2.5 bg-blue-600 text-white font-medium text-xs leading-tight uppercase hover:bg-blue-700 focus:bg-blue-700 focus:outline-none focus:ring-0 active:bg-blue-800 transition duration-150 ease-in-out\" onclick=\"location.href='/led/off'\">Off</button></div></div>"); // LED 끄고켜기 버튼
    client.println("</div>"); // <div class="basis-1/2 relative mb-6">
    
    
    client.println("<div class=\"basis-1/2 relative\">");

    // 물빼기 모터
    client.print("<span class=\"text-2xl\">물빼기 모터: "); 
    isSubmotorOn
    ? client.println("<span class=\"text-2xl font-bold\">OFF</span>")
    : client.println("<span class=\"text-2xl font-bold text-green-400\">ON</span>");
    client.println("</span>");
    client.println("<div class=\"flex\">\
                    <div class=\"inline-flex shadow-md hover:shadow-lg focus:shadow-lg\" role=\"group\">\
                        <button type=\"button\" class=\"rounded-l inline-block px-6 py-2.5 bg-blue-600 text-white font-medium text-xs leading-tight uppercase hover:bg-blue-700 focus:bg-blue-700 focus:outline-none focus:ring-0 active:bg-blue-800 transition duration-150 ease-in-out\" onclick=\"location.href='/motor/sub/on'\">On</button>\
                        <button type=\"button\" class=\" rounded-r inline-block px-6 py-2.5 bg-blue-600 text-white font-medium text-xs leading-tight uppercase hover:bg-blue-700 focus:bg-blue-700 focus:outline-none focus:ring-0 active:bg-blue-800 transition duration-150 ease-in-out\" onclick=\"location.href='/motor/sub/off'\">Off</button></div></div>");
    client.println("</div>"); // <div class="basis-1/2 relative">

    // 온습도, 메모리
    client.println("<div class=\"basis-1/4\">");
    client.println("<div><span class=\"text-2xl\">온도: </span>");
    client.print("<span class=\"text-2xl font-bold\">");
    client.print(tempC); // 온도
    client.println("<span class=\"text-red-600\">°C</span></span>");
    client.println("</div>");
    client.println("<div><span class=\"text-2xl\">습도: </span>");
    client.print("<span class=\"text-2xl font-bold\">");
    client.print(humidity); // 습도
    client.println("<span>%</span></span>");
    client.println("</div>");

    client.println("</div>"); // <div class="basis-1/4">
    client.println("</div>"); // <div class="flex flex-row mb-6">

    // 화분카드 선언부
    client.println("<div class=\"h-fit grid grid-cols-2 gap-4 text-center\">");
    client.println("<div class=\"border-2 border-violet-400 rounded\">");
    client.println("<span class=\"mb-1 text-2xl font-bold\">1번 화분</span><hr/>");
    client.print("<div class=\"mb-3 mt-3 text-3xl font-bold\">모터 ");
    motorOn[0]
    ? client.println("<span class=\"text-3xl font-bold\">OFF</span>")
    : client.println("<span class=\"text-3xl font-bold text-green-400\">ON</span>");
    client.println("</div>"); // <div class="mb-3 mt-3 text-3xl font-bold">

    client.println("<div class=\"mb-1 text-lg font-bold\">수분</div>");
    client.println("<div class=\"mx-auto w-9/12 h-6 bg-gray-200 rounded-full dark:bg-gray-700\">");
    client.println("<div class=\"h-6 bg-gradient-to-r from-cyan-500 to-indigo-500 rounded-full font-bold text-slate-200\" style=\"width:");
    client.print(soilPercents[0]);
    client.print("%\">");
    client.print(soilPercents[0]);
    client.println("%</div></div>");
    client.print("<div class=\"p-5 shadow-lg rounded-lg overflow-hidden\">\
                    <div class=\"py-3 px-5 bg-gray-50 font-bold\">");
    client.print(String(year) + "-" + String(month) + "-" + String(day));
    client.println(" 1번 화분 수분량</div>\
                    <canvas class=\"p-1\" id=\"chartLine1\"></canvas>\
                </div>");
    client.println("</div>");
    
    client.println("<div class=\"border-2 border-violet-400 rounded\">");
    client.println("<span class=\"mb-1 text-2xl font-bold\">2번 화분</span><hr/>");
    client.print("<div class=\"mb-3 mt-3 text-3xl font-bold\">모터 ");
    motorOn[1]
    ? client.println("<span class=\"text-3xl font-bold\">OFF</span>")
    : client.println("<span class=\"text-3xl font-bold text-green-400\">ON</span>");
    client.println("</div>");

    client.println("<div class=\"mb-1 text-lg font-bold\">수분</div>");
    client.println("<div class=\"mx-auto w-9/12 h-6 bg-gray-200 rounded-full dark:bg-gray-700\">");
    client.println("<div class=\"h-6 bg-gradient-to-r from-cyan-500 to-indigo-500 rounded-full font-bold text-slate-200\" style=\"width:");
    client.print(soilPercents[1]);
    client.print("%\">");
    client.print(soilPercents[1]);
    client.println("%</div></div>");
    
    // 차트
    client.print("<div class=\"p-5 shadow-lg rounded-lg overflow-hidden\">\
                    <div class=\"py-3 px-5 bg-gray-50 font-bold\">");
    client.print(String(year) + "-" + String(month) + "-" + String(day));
    client.println(" 2번 화분 수분량</div>\
                    <canvas class=\"p-1\" id=\"chartLine2\"></canvas>\
                </div>");

    client.println("</div></div></div>");

    // Log창
    client.println("<div class=\"m-2 mb-0 p-2 pl-4 rounded-t-lg bg-slate-400\">\
        <span class=\"text-4xl font-bold\">Log</span>\
    </div>");
    client.println("<div class=\"m-2 mt-0 p-2 pl-4 h-full bg-slate-600 font-bold text-white overflow-scroll\" style=\"height: 30vh;\">");
    for(int i = logcount; i >= 0; i--){
        client.print("<div>");
        client.print(logs[i]);
        client.println("</div>");
    }
    client.println("</div>");
    client.println("</body>");

    // Line 차트
    client.println("<script src=\"https://cdn.jsdelivr.net/npm/chart.js\"></script>");
    client.println("<script>");


    client.print("const humidity = ");
    client.print("[[");
    for(int i = 0; i < 24; i++){
        client.print(soilHumidity1[i]);
        client.print(",");
    }
    client.print("],[");
    for(int i = 0; i < 24; i++){
        client.print(soilHumidity2[i]);
        client.print(",");
    }
    client.println("]]");

    client.println("const labels = [\"0시\", \"1시\", \"2시\", \"3시\", \"4시\", \"5시\", \"6시\", \"7시\", \"8시\", \"9시\", \"10시\", \"11시\", \"12시\", \"13시\", \"14시\", \"15시\", \"16시\", \"17시\", \"18시\", \"19시\", \"20시\", \"21시\", \"22시\", \"23시\"]");
    client.println("const data = [{");
    client.println("labels: labels,");
    client.println("datasets: [{");
    client.println("label: \"수분량\",");
    client.println("backgroundColor: \"hsl(252, 82.9%, 67.8%)\",");
    client.println("borderColor: \"hsl(252, 82.9%, 67.8%)\",");
    client.println("data: humidity[0],},");
    client.println("],},{");
    client.println("labels: labels,");
    client.println("datasets: [{");
    client.println("label: \"수분량\",");
    client.println("backgroundColor: \"hsl(252, 82.9%, 67.8%)\",");
    client.println("borderColor: \"hsl(252, 82.9%, 67.8%)\",");
    client.println("data: humidity[1],},],}]");
    client.println("var chartLine = new Chart(");
    client.println("document.getElementById(\"chartLine1\"),");
    client.println("{type: \"line\",");
    client.println("data:data[0],");
    client.println("options: {},})");
    client.println("var chartLine2 = new Chart(");
    client.println("document.getElementById(\"chartLine2\"),");
    client.println("{type: \"line\",");
    client.println("data:data[1],");
    client.println("options: {},})");
    
    client.println("</script>");

    // html 닫기
    client.println("</html>");
}
