#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MD5Builder.h>
#include "des.h"
#include <HTTPClient.h>
#include <MD5Builder.h>
#include "Freenove_WS2812_Lib_for_ESP32.h"
const char* ssid = "WIFI";
const char* password = "WIFI PSWD";


const char* USERNAME = "20241111111";
const char* PASSWORD = "dut PSWD";


// 苏宁时间API（获取毫秒时间戳）
const char* TIME_API_URL = "http://f.m.suning.com/api/ct.do";

// 触发配置
#define TRIGGER_BUTTON_PIN 0
#define MIJIA_PIN 4
#define DEBOUNCE_DELAY 200
#define MIJIA_DELAY_TIME 2400
// Token更新间隔（1小时 = 3600000毫秒）
#define TOKEN_UPDATE_INTERVAL 3600000UL


//LED配置
#define LEDS_COUNT  3
#define LEDS_PIN	48
#define CHANNEL		0

Freenove_ESP32_WS2812 strip = Freenove_ESP32_WS2812(LEDS_COUNT, LEDS_PIN, CHANNEL, TYPE_GRB);
// ==================== 全局变量 ====================

unsigned long long apiCurrentTime = 0;  // 毫秒时间戳
unsigned long apiMillisAtUpdate = 0;
bool timeSynced = false;

String currentToken = "";  // 当前有效的token
unsigned long lastTokenUpdate = 0;  // 上次更新token的时间

String cookieJar;

String httpGET(const char* host,String url)
{
    WiFiClient client;

    client.connect(host,80);

    client.print(
        String("GET ")+url+" HTTP/1.1\r\n"+
        "Host: "+host+"\r\n"+
        "User-Agent: Mozilla/5.0\r\n"+
        "Cookie: "+cookieJar+"\r\n"+
        "Connection: close\r\n\r\n"
    );

    String res="";

    while(client.connected() || client.available())
    {
        if(client.available())
            res += (char)client.read();
    }

    return res;
}

// ================= 读取HTTP响应 =================
String readResponse(WiFiClientSecure &client)
{
    String res="";

    while(client.connected() || client.available())
    {
        if(client.available())
        {
            char c = client.read();
            res += c;
        }
    }

    return res;
}

// ================= 提取header =================
String getHeader(String res,String name)
{
    int p = res.indexOf(name+":");
    if(p==-1) return "";

    p += name.length()+1;

    int e = res.indexOf("\r\n",p);

    return res.substring(p,e);
}

// ================= 保存cookie =================
void saveCookie(String res)
{
    int p = 0;

    while(true)
    {
        p = res.indexOf("Set-Cookie:",p);
        if(p==-1) break;

        p += 11;

        int e = res.indexOf(";",p);

        cookieJar += res.substring(p,e) + ";";
    }
}

// ================= 提取html字段 =================
String extract(String html,String l,String r)
{
    int s = html.indexOf(l);
    if(s==-1) return "";

    s += l.length();

    int e = html.indexOf(r,s);

    return html.substring(s,e);
}

// ================= GET =================
String httpsGET(const char* host,String url)
{
    WiFiClientSecure client;
    client.setInsecure();

    client.connect(host,443);

    client.print(
        String("GET ")+url+" HTTP/1.1\r\n"+
        "Host: "+host+"\r\n"+
        "User-Agent: Mozilla/5.0\r\n"+
        "Cookie: "+cookieJar+"\r\n"+
        "Connection: close\r\n\r\n"
    );

    return readResponse(client);
}

// ================= POST =================
String httpsPOST(const char* host,String url,String body)
{
    WiFiClientSecure client;
    client.setInsecure();

    client.connect(host,443);

    client.print(
        String("POST ")+url+" HTTP/1.1\r\n"+
        "Host: "+host+"\r\n"+
        "User-Agent: Mozilla/5.0\r\n"+
        "Content-Type: application/x-www-form-urlencoded\r\n"+
        "Content-Length: "+body.length()+"\r\n"+
        "Origin: https://sso.dlut.edu.cn\r\n"+
        "Referer: https://sso.dlut.edu.cn/cas/login\r\n"+
        "Cookie: "+cookieJar+"\r\n"+
        "Connection: close\r\n\r\n"+
        body
    );

    return readResponse(client);
}

String login()
{
    cookieJar="";

    // STEP1 获取登录页
    String res = httpsGET(
    "sso.dlut.edu.cn",
    "/cas/login?service=http://menjin.dlut.edu.cn/cser/static/menjin/index.html"
    );

    saveCookie(res);

    String lt = extract(res,
    "name=\"lt\" value=\"","\"");

    String execution = extract(res,
    "name=\"execution\" value=\"","\"");

    Serial.println(lt);
    Serial.println(execution);



    // STEP2 rsa
    String data =
    String(USERNAME)+
    PASSWORD+
    lt;

    char rsa[512];

    strEnc(data.c_str(),"1","2","3",rsa);



    // STEP3 POST 登录
    String body =
    "rsa="+String(rsa)+
    "&ul="+String(strlen(USERNAME))+
    "&pl="+String(strlen(PASSWORD))+
    "&sl=0"+
    "&lt="+lt+
    "&execution="+execution+
    "&_eventId=submit";

    res = httpsPOST(
    "sso.dlut.edu.cn",
    "/cas/login?service=http://menjin.dlut.edu.cn/cser/static/menjin/index.html",
    body
    );

    saveCookie(res);

    String location = getHeader(res,"Location");

    Serial.println("redirect=");
    Serial.println(location);



    // STEP4 ticket
    int p = location.indexOf(
    "http://menjin.dlut.edu.cn");

    location = location.substring(p);

    res = httpGET(
    "menjin.dlut.edu.cn",
    location
    );

    saveCookie(res);



    // STEP5 index
    res = httpGET(
    "menjin.dlut.edu.cn",
    "/cser/static/menjin/index.html"
    );

    saveCookie(res);



    // STEP6 token
    int t = cookieJar.indexOf("token=");

    int e = cookieJar.indexOf(";",t);

    return cookieJar.substring(t+6,e);
}



/**
 * 生成随机字符串（4位，字母+数字）
 */
String randomString(int length) {
    const char chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    String result = "";
    for (int i = 0; i < length; i++) {
        result += chars[esp_random() % (sizeof(chars) - 1)];
    }
    return result;
}

/**
 * 计算MD5哈希
 */
String md5Hash(const String& input) {
    MD5Builder md5;
    md5.begin();
    md5.add(input);
    md5.calculate();
    return md5.toString();
}

/**
 * 从苏宁API获取毫秒时间戳
 */
bool getTimeFromAPI(int maxRetries = 5) {
    Serial.println("\n[获取网络时间...]");

    HTTPClient http;
    http.setTimeout(10000);

    for (int i = 0; i < maxRetries; i++) {
        http.begin(TIME_API_URL);
        int httpCode = http.GET();

        if (httpCode == 200) {
            String payload = http.getString();
            http.end();

            // 解析 {"api":"time","code":"1","currentTime":1774456155486,"msg":""}
            int idx = payload.indexOf("\"currentTime\":");
            if (idx != -1) {
                int start = idx + 14;
                int end = payload.indexOf(",", start);
                if (end == -1) end = payload.indexOf("}", start);

                String timeStr = payload.substring(start, end);
                timeStr.trim();

                apiCurrentTime = strtoull(timeStr.c_str(), NULL, 10);
                apiMillisAtUpdate = millis();
                timeSynced = true;

                Serial.printf("时间戳: %llu\n", apiCurrentTime);
                return true;
            }
        }

        http.end();
        delay(1000 * (i + 1));
    }

    return false;
}

/**
 * 获取当前毫秒时间戳（基于API时间+运行时长）
 */
String getTimestamp() {
    if (!timeSynced) return "0";
    unsigned long elapsed = millis() - apiMillisAtUpdate;
    unsigned long long now = apiCurrentTime + elapsed;
    return String(now);
}


// ==================== Token管理 ====================



/**
 * 更新Token
 */
bool updateToken() {
    Serial.println("\n[更新Token...]");
    String newToken = login();

    if (newToken.length() > 0) {
        currentToken = newToken;
        lastTokenUpdate = millis();
        Serial.println("Token更新成功");
        Serial.println("Token: " + currentToken.substring(0, 20) + "...");
        return true;
    } else {
        Serial.println("Token更新失败！");
        return false;
    }
}

/**
 * 检查是否需要更新Token
 */
void checkAndUpdateToken() {
    if (millis() - lastTokenUpdate >= TOKEN_UPDATE_INTERVAL) {
        Serial.println("\n[到达Token更新周期]");
        updateToken();
    }
}

// ==================== 开门函数 ====================

/**
 * 执行开门请求
 */
bool executeOpenDoor(const String& token) {
    // 生成时间戳和随机数
    String ts = getTimestamp();
    String rand = randomString(4);

    // 生成签名
    String signData = ts + "#2323dsfadfewrasa3434#" + rand;
    String sign = md5Hash(signData) + rand;

    Serial.println("\n[开门请求]");
    Serial.println("ts: " + ts);
    Serial.println("rand: " + rand);
    Serial.println("sign: " + sign);

    // 构建POST数据
    String postData = "";
    postData += "commandCode=OPEN";
    postData += "&conditions=%7B%22personId%22%3A%22 STUDENTNUMBER %22%2C%22delStatus%22%3A%220%22%7D";
    postData += "&deviceCode=DL-LY-114514";
    postData += "&isCommon=yes";
    postData += "&pageSize=-1";
    postData += "&projectCd=DA_LIAN_LI_GONG_MENJIN";
    postData += "&token=" + token;

    // 发送HTTP POST请求
    HTTPClient http;
    http.begin("http://menjin.dlut.edu.cn/cser/device/info/command/sendRoomBatch");
    http.setTimeout(10000);

    // 设置Headers
    http.addHeader("AUTH-SIGN", sign);
    http.addHeader("AUTH-TIMESTAMP", ts);
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");

    int httpCode = http.POST(postData);
    String response = http.getString();
    http.end();

    // 打印响应
    Serial.println("\n----- HTTP Response -----");
    Serial.printf("Status: %d\n", httpCode);
    Serial.println("Body: " + response);
    Serial.println("----- Response End -----");

    // 判断成功
    bool success = (response.indexOf("\"success\":true") != -1);
    Serial.println(success ? "Result: SUCCESS" : "Result: FAILED");

    return success;
}

/**
 * 完整的开门流程（包含失败重试逻辑）
 */
void openDoorProcess() {
    Serial.println("\n[开始开门流程]");
    strip.setAllLedsColor(0, 0, 0);
    // 第一次尝试开门
    bool success = executeOpenDoor(currentToken);

    if (success) {
        // 成功：快闪1下，熄灭
        Serial.println("开门成功！");
        strip.setAllLedsColor(0, 255, 0);
        delay(400);
        strip.setAllLedsColor(0, 0, 0);
        return;
    }

    // 第一次失败：持续快闪，重新获取token，重新尝试
    Serial.println("第一次开门失败，正在重新获取Token并重试...");
    strip.setAllLedsColor(255, 255, 0);

    if (!updateToken()) {
        // Token获取失败：常亮
        Serial.println("Token获取失败，进入错误状态");
        strip.setAllLedsColor(255, 0, 0);
        return;
    }

    // 第二次尝试开门
    success = executeOpenDoor(currentToken);

    if (success) {
        // 第二次成功：快闪1下，熄灭
        Serial.println("第二次尝试开门成功！");
        strip.setAllLedsColor(0, 255, 0);
        delay(400);
        strip.setAllLedsColor(0, 0, 0);
    } else {
        // 第二次失败：常亮
        Serial.println("第二次开门失败，进入错误状态");
        strip.setAllLedsColor(255, 0, 0);
    }
}

// ==================== 系统初始化 ====================

void setup() {
    Serial.begin(115200);
    delay(1000);
    pinMode(MIJIA_PIN, INPUT_PULLDOWN);
    Serial.println("\nESP32 Door Control - Auto Token Update");
    //LED
    strip.begin();
    strip.setBrightness(20);  
    // 连接WiFi
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    Serial.print("Connecting WiFi");
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connected: " + WiFi.localIP().toString());

    // 同步网络时间
    if (!getTimeFromAPI(5)) {
        Serial.println("Time sync failed!");
        // 时间同步失败时，LED快闪警告
        while (true) {
            strip.setAllLedsColor(0, 0, 255);
            delay(500);
            strip.setAllLedsColor(0, 0, 0);
        }
    }

    // 初始化获取Token
    Serial.println("\n[初始化获取Token...]");
    if (!updateToken()) {
        Serial.println("初始Token获取失败！");
        strip.setAllLedsColor(255, 0, 0);
        while (true) {
            delay(1000);  // 停止在这里，需要重启
        }
    }

    Serial.println("\n系统就绪。每小时自动更新Token，按下按钮开门。");

    // 启动完成指示灯
    for (int i = 0; i < 3; i++) {
        strip.setAllLedsColor(0, 255, 0);
        delay(100);
        strip.setAllLedsColor(0, 0, 0);
        delay(100);
    }
}

// ==================== 主循环 ====================

unsigned long lastButtonPress = 0;
bool lastButtonState = HIGH;
unsigned long lastButtonPress_MIJIA = 0;
bool lastButtonState_MIJIA = HIGH;
long long last_mijia = 0;
void loop() {
    // 每小时检查并更新Token
    checkAndUpdateToken();

    // 按钮检测
    bool buttonState = digitalRead(TRIGGER_BUTTON_PIN);
    bool buttonState_MIJIA = digitalRead(MIJIA_PIN);
    // 检测按钮按下（下降沿触发）
    if ((lastButtonState == HIGH && buttonState == LOW)) {
        if (millis() - lastButtonPress > DEBOUNCE_DELAY) {
            lastButtonPress = millis();

            Serial.println("\n[Button Pressed]");

            // 执行完整的开门流程
            openDoorProcess();
        }
    }
    if (lastButtonState_MIJIA == LOW && buttonState_MIJIA == HIGH) {
        if (millis() - lastButtonPress_MIJIA > MIJIA_DELAY_TIME) {
            lastButtonPress_MIJIA = millis();

            Serial.println("\n[Button Pressed]");

            // 执行完整的开门流程
            openDoorProcess();
        }
    }
    lastButtonState = buttonState;
    lastButtonState_MIJIA = buttonState_MIJIA;
    delay(10);
}