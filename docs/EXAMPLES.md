# cms-embedded-utils Examples

이 문서는 `cms-embedded-utils` 라이브러리를 실무에서 극한까지 활용하는 다양한 시나리오별 예제 코드 모음입니다. 모든 예제는 **Zero-Heap(동적 할당 미사용)** 원칙을 준수합니다.

---

## 1. 문자열 조작 및 포맷팅 (String & Formatting)

### 1.1 복합 포맷팅 및 체이닝 (Chaining)
`printf` 스타일과 C++ 스트림 스타일을 자유롭게 혼합하여 사용할 수 있습니다.
```cpp
#include <cmsString.h>

void statusReport() {
    cms::String<128> s;
    int battery = 85;
    float voltage = 4.12f;

    // printf로 시작하여 스트림으로 결합
    s.printf("BAT: %d%% (%.2fV)", battery, voltage) << " | Mode: " << "POWER_SAVE";

    // 결과: "BAT: 85% (4.12V) | Mode: POWER_SAVE"
}
```

### 1.2 UTF-8 안전한 한글 조작
바이트 단위가 아닌 논리적 글자 단위로 동작하여 한글 깨짐을 방지합니다.
```cpp
void processKorean() {
    cms::String<64> msg = "알림: [화재] 감지됨";
    cms::String<32> tag;

    // "화재" 부분만 추출 (글자 인덱스 5번부터 2글자)
    msg.substring(tag, 5, 7);

    if (tag == "화재") {
        msg.replace("감지됨", "발생!!!"); // "알림: [화재] 발생!!!"
    }
}
```

---

## 2. 프로토콜 파싱 및 토큰화 (Parsing & Tokenize)

### 2.1 `splitTo`를 이용한 즉시 배열 변환 (권장)
가장 깔끔하고 안전하게 문자열을 분리하여 고정 크기 배열에 담습니다.
```cpp
void parseConfig(const char* line) {
    cms::String<64> src = line; // 예: "SSID:MyHome:WPA2"
    cms::String<32> params[3];

    // ':' 기준으로 분리하여 params 배열에 순서대로 복사
    size_t count = cms::splitTo(src, ':', params);

    if (count == 3) {
        // params[0]: "SSID", params[1]: "MyHome", params[2]: "WPA2"
    }
}
```

### 2.2 `Token`을 이용한 비파괴적 파싱
원본 문자열을 수정하지 않고 포인터와 길이 정보만으로 빠르게 파싱합니다.
```cpp
void onDataReceived(const char* raw) {
    cms::String<64> data = raw;
    cms::string::Token tokens[3];

    // "CMD:SET_TEMP:24.5" 분리
    if (data.split(':', tokens, 3) == 3) {
        if (tokens[0] == "CMD" && tokens[1] == "SET_TEMP") {
            float temp = tokens[2].toFloat();
        }
    }
}
```

### 2.3 `copyTokens`를 이용한 단계별 복사
먼저 토큰으로 분석한 뒤, 필요한 부분만 문자열 객체로 복사할 때 유용합니다.
```cpp
void processLog(const char* raw) {
    cms::string::Token tokens[5];
    size_t n = cms::string::split(raw, ',', tokens, 5);

    if (n > 2) {
        cms::String<16> category;
        // 특정 토큰만 실제 String 객체로 안전하게 복사
        category.append(tokens[1].ptr, tokens[1].len);
    }
}
```

---

## 3. 큐 활용 (Queue & ThreadSafeQueue)

### 3.1 기본 큐를 이용한 상태 머신 (Single-task)
뮤텍스 오버헤드가 없어 단일 루프나 인터럽트가 없는 환경에서 매우 빠릅니다.
```cpp
#include <cmsQueue.h>

cms::Queue<char, 10> cmdQueue;

void loop() {
    char cmd;
    if (cmdQueue.pop(cmd)) {
        switch(cmd) {
            case 'R': resetSystem(); break;
            case 'S': startSystem(); break;
        }
    }
}
```

### 3.2 스레드 안전 큐를 이용한 태스크 간 통신 (Multi-task)
```cpp
#include <cmsQueue.h>

cms::ThreadSafeQueue<float, 20> sensorData;

// Task 1 (Core 0): 센서 읽기
void sensorTask(void* p) {
    while(1) {
        sensorData.enqueue(readADC());
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// Task 2 (Core 1): 데이터 분석
void analysisTask(void* p) {
    while(1) {
        float val;
        if (sensorData.pop(val)) {
            // 분석 로직 수행
        }
    }
}
```

### 3.3 ADC 샘플링용 이동 평균 필터 (Moving Average)
```cpp
cms::Queue<uint16_t, 10> samples;

uint32_t getAverage() {
    uint32_t sum = 0;
    uint16_t val;
    for (uint8_t i = 0; i < samples.size(); ++i) {
        samples.getAt(i, val); // 큐를 비우지 않고 조회
        sum += val;
    }
    return samples.isEmpty() ? 0 : (sum / samples.size());
}
```

---

## 4. AsyncLogger 확장 및 커스텀 (Logger)

### 4.1 보안 필터링 및 가로채기 (`handleLog`)
로그가 큐에 쌓이기 전 보안 정보를 가리거나 특정 조건에서 즉각 대응합니다.
```cpp
#include <cmsAsyncLogger.h>

class SecureLogger : public cms::AsyncLogger<256, 16> {
protected:
    bool handleLog(const cms::StringBase& msg) override {
        // 1. 특정 키워드 포함 시 로그 완전 차단 (큐 저장 안함)
        if (msg.contains("PASSWORD") || msg.contains("AUTH_TOKEN")) {
            return true;
        }

        // 2. 특정 조건에서 메시지 변형 후 수동 투입
        if (msg.contains("RETRY")) {
            cms::String<128> newMsg = "[AUTO-RECOVERY] ";
            newMsg << msg;
            pushToQueue(newMsg); // 가공된 메시지를 큐에 넣음
            return true; // 원본은 차단
        }
        return false; // 나머지는 정상적으로 비동기 처리
    }
};
```

### 4.2 SD 카드 파일 로깅 (`outputLog`)
시리얼이 아닌 파일 시스템으로 로그 출력 방향을 바꿉니다.
```cpp
class FileLogger : public cms::AsyncLogger<256, 32> {
protected:
    void outputLog(const cms::StringBase& msg) override {
        // 시리얼 대신 SD 카드의 파일로 로그 기록
        myFile.println(msg.c_str());
        myFile.flush();
    }
};
```

### 4.3 조건부 즉시 출력 (Emergency Bypass)
에러 로그는 큐를 거치지 않고 즉시 출력하여 지연을 방지합니다.
```cpp
class SmartLogger : public cms::AsyncLogger<256, 16> {
protected:
    bool handleLog(const cms::StringBase& msg) override {
        // 에러 로그([E])는 비동기 큐를 거치지 않고 즉시 장치로 출력
        if (msg.contains("[E]")) {
            outputLog(msg);
            return true; // 큐 저장 생략
        }
        return false;
    }
};
```

### 4.4 표준 사용법 및 비동기 처리 (Standard Usage)
가장 일반적인 로거 초기화 및 비동기 출력 처리 흐름입니다. `printf` 형식을 사용하여 동적 데이터를 포함할 수 있습니다.
```cpp
void standardLoggingExample() {
    // 1. 로거 인스턴스 획득 및 설정
    auto& myLog = cms::AsyncLogger<>::instance();
    myLog.begin(cms::LogLevel::Debug);
    myLog.setUseColor(true);

    // 2. 다양한 레벨의 로그 출력 (printf 형식 지원)
    myLog.d("디버그 메시지입니다. (Code: %d)", 101);
    myLog.i("정보 메시지이며 [%s] 태그를 포함합니다.", "Network");
    myLog.w("경고! [Sensor] 데이터가 불안정합니다. (현재값: %.2f)", 85.43f);
    myLog.e("이 로그는 SECRET 정보를 포함하므로 무시됩니다.");
    myLog.i("시스템 재시도 중... RETRY 명령 확인");

    // 3. 비동기 로거의 핵심: 실제 출력 처리
    // 로그는 즉시 출력되지 않고 내부 큐에 저장됩니다.
    // 실제 출력은 CPU 여유가 있을 때(Idle task 등) 아래 함수를 호출하여 수행합니다.
    std::cout << "\n[Processing Logs...]" << std::endl;
    while (myLog.processNextLog()) {
        // 큐가 빌 때까지 로그를 하나씩 꺼내 출력합니다.
    }
}
```

---

## 5. 저수준 유틸리티 직접 사용 (`cms::string`)

`String` 클래스 인스턴스를 만들 여유조차 없는 극도의 저사양 환경이나 ISR 내부에서 유용합니다.

### 5.1 원시 버퍼(char*) 고속 처리
```cpp
#include <cmsStringUtil.h>

void onRawData(char* buf) {
    // 1. 즉시 공백 제거 (In-place)
    size_t newLen = cms::string::trim(buf);

    // 2. 16진수 여부 확인 및 변환
    if (cms::string::isHex(buf)) {
        int val = cms::string::hexToInt(buf);
    }

    // 3. 대소문자 무시 비교
    if (cms::string::equals(buf, "START", true)) {
        // 시스템 시작
    }
}
```

### 5.2 경량 포맷팅 엔진 (`appendPrintf`)
표준 `vsnprintf`를 쓰기엔 스택이 부족할 때 사용합니다.
```cpp
char myBuf[64];
size_t curLen = 0;

// 가변 인자를 받아 버퍼에 직접 포맷팅
cms::string::appendPrintf(myBuf, sizeof(myBuf), curLen, "ID:%d, VAL:%.2f", 10, 3.14f);
```

---

## 6. 실전 복합 시나리오

### 6.1 바이너리 패킷 덤프 도구
```cpp
void dumpPacket(const uint8_t* data, size_t len) {
    auto& logger = cms::AsyncLogger<>::instance();
    cms::String<128> hex;
    hex << "PKT [" << (int)len << " bytes]: ";

    for (size_t i = 0; i < len; ++i) {
        hex.appendPrintf("%02X ", data[i]);

        // 버퍼가 거의 다 차면 중간에 한 번 출력하고 비움
        if (hex.utilization() > 90.0f) {
            logger.i("%s", hex.c_str());
            hex.clear() << "  > ";
        }
    }
    logger.i("%s", hex.c_str());
}
```

### 6.2 JSON 스타일 데이터 조립
```cpp
void sendJsonStatus(int id, float temp) {
    cms::String<128> json;
    json.printf("{\"id\":%d,\"temp\":%.2f,\"status\":\"OK\"}", id, temp);

    // 큐를 통해 전송 전용 태스크로 전달
    static cms::ThreadSafeQueue<cms::String<128>, 5> mqttQueue;
    mqttQueue.enqueue(json);
}
```

### 6.3 명령어 히스토리 관리
```cpp
cms::Queue<cms::String<32>, 5> history;
auto& logger = cms::AsyncLogger<>::instance();

void onCommand(const char* cmd) {
    cms::String<32> s = cmd;
    history.enqueue(s); // 가장 오래된 명령어부터 자동으로 밀려남
}

void showHistory() {
    cms::String<32> item;
    logger.i("--- Last 5 Commands ---");
    for (uint8_t i = 0; i < history.size(); ++i) {
        history.getAt(i, item);
        logger.i("%d: %s", i + 1, item.c_str());
    }
}
```

---

## 7. 성능 및 리소스 최적화 팁

### 7.1 프로파일링을 통한 버퍼 크기 결정
```cpp
auto& logger = cms::AsyncLogger<>::instance();
cms::String<512> testStr;
// ... 실제 로직 수행 ...
logger.i("Peak usage: %.1f%%", testStr.peakUtilization());
// 만약 Peak가 20% 미만이라면 버퍼 크기를 128로 줄여 RAM을 절약하세요.
```

### 7.2 큐 인덱스 타입 최적화
```cpp
// 큐 크기가 255 이하인 경우 IndexType을 uint8_t로 지정하여 RAM을 아끼세요.
cms::Queue<int, 50, uint8_t> smallQueue;
```

### 7.3 리터럴 비교 최적화
```cpp
// 아래 코드는 내부적으로 strlen을 호출하지 않고 컴파일 타임에 최적화됩니다.
if (str == "START") { ... }
```

---

> **Tip**: 모든 예제는 `Zero-Heap` 원칙을 준수합니다. `cms::String<N>`의 `N` 크기는 해당 함수가 실행되는 스택(Stack) 크기를 고려하여 적절히 설정하세요. 대형 버퍼는 `static` 또는 전역 변수로 선언하는 것이 안전합니다.
