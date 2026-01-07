/**
 * @file main.cpp
 * @author comser.dev
 * @brief cms-embedded-utils 라이브러리 종합 사용 예제
 *
 * 이 예제는 문자열 조작, 스레드 안전 큐, 그리고 비동기 로거의
 * 기본적인 사용법을 보여줍니다.
 */

#include "../src/cmsString.h"
#include "../src/cmsQueue.h"
#include "../src/cmsAsyncLogger.h"
#include <iostream>

// 1. AsyncLogger를 상속받아 기능을 확장한 커스텀 로거 정의
// 필터링(handleLog)과 출력 대상 변경(outputLog)을 한 클래스에서 처리합니다.
class MyCustomLogger : public cms::AsyncLogger<128, 16> {
protected:
    /**
     * @brief [필터링/가로채기] 로그가 비동기 큐에 쌓이기 전 호출됩니다.
     *
     * @return
     *   - true: 로그 처리를 여기서 완료함 (큐에 저장 안 함). 보안 차단이나 즉시 처리에 사용.
     *   - false: 로거가 메시지를 내부 큐에 저장하도록 허용함 (기본값).
     */
    bool handleLog(const cms::StringBase& msg) override {
        if (msg.contains("SECRET")) {
            std::cout << "[Filter] 보안 키워드 감지: 로그 차단됨 (큐 저장 안함)" << std::endl;
            return true; // 큐에 저장하지 않고 여기서 종료
        }

        if (msg.contains("RETRY")) {
            // pushToQueue()를 사용하여 메시지를 변형해 수동으로 큐에 넣을 수 있습니다.
            cms::String<128> retryMsg = "[RETRY-SYSTEM] ";
            retryMsg << msg;
            pushToQueue(retryMsg);
            return true; // 원본 대신 수정본을 넣었으므로 원본은 차단
        }

        return false; // 나머지는 정상적으로 비동기 큐에 저장
    }

    /**
     * @brief [출력 대상 변경] 큐에서 꺼내진 로그를 실제 장치로 보낼 때 호출됩니다.
     * TCP, BLE, SD 카드 등으로 보내고 싶을 때 이 함수를 재정의합니다.
     */
    void outputLog(const cms::StringBase& msg) override {
        // 기본 Serial 출력 대신 커스텀 장치(예: LCD, 네트워크)로 출력한다고 가정합니다.
        std::cout << "[MyDevice] " << msg.c_str() << std::endl;
    }
};

int main() {
    // =========================================================
    // [1] cms::String - Zero-Heap & UTF-8 Safe Strings
    // =========================================================
    std::cout << "--- String Example ---" << std::endl;

    // 64바이트 고정 버퍼 문자열 선언
    cms::String<64> str = "System";

    // 스트림 스타일 결합 및 포맷팅
    str << " Status: " << 200 << " [OK]";
    std::cout << "Formatted: " << str.c_str() << std::endl;

    // UTF-8 안전한 조작 (한글 포함)
    cms::String<64> utf8Str = "온도: 25.5도";
    std::cout << "UTF-8 Count: " << utf8Str.count() << " chars" << std::endl;

    // 부분 문자열 추출 (글자 단위 인덱스 사용)
    cms::String<32> sub;
    utf8Str.substring(sub, 0, 2); // "온도"
    std::cout << "Substring: " << sub.c_str() << std::endl;


    // =========================================================
    // [2] cms::ThreadSafeQueue - Inter-task Communication
    // =========================================================
    std::cout << "\n--- Queue Example ---" << std::endl;

    // 5개의 정수를 담을 수 있는 스레드 안전 큐
    cms::ThreadSafeQueue<int, 5> sensorQueue;

    // 데이터 투입
    sensorQueue.enqueue(101);
    sensorQueue.enqueue(102);

    // 데이터 추출
    int data;
    if (sensorQueue.pop(data)) {
        std::cout << "Popped from queue: " << data << std::endl;
    }


    // =========================================================
    // [3] cms::Logger - Asynchronous Styling Logger
    // =========================================================
    std::cout << "\n--- Logger Example ---" << std::endl;

    // 커스텀 로거 인스턴스 사용
    MyCustomLogger myLog;
    myLog.begin(cms::LogLevel::Debug, true);

    // 다양한 레벨의 로그 출력
    // printf 형식을 사용하여 동적 데이터를 포함할 수 있습니다.
    myLog.d("디버그 메시지입니다. (Code: %d)", 101);
    myLog.i("정보 메시지이며 [%s] 태그를 포함합니다.", "Network");
    myLog.w("경고! [Sensor] 데이터가 불안정합니다. (현재값: %.2f)", 85.43f);
    myLog.e("이 로그는 SECRET 정보를 포함하므로 무시됩니다.");
    myLog.i("시스템 재시도 중... RETRY 명령 확인");

    // 비동기 로거의 핵심:
    // 로그는 즉시 출력되지 않고 내부 큐에 저장됩니다.
    // 실제 출력은 CPU 여유가 있을 때(Idle task 등) 아래 함수를 호출하여 수행합니다.
    std::cout << "\n[Processing Logs...]" << std::endl;
    while (myLog.update()) {
        // 큐가 빌 때까지 로그를 하나씩 꺼내 출력합니다.
    }

    // 리소스 모니터링
    std::cout << "\n--- Resource Profiling ---" << std::endl;
    std::cout << "String Buffer Utilization: " << str.utilization() << "%" << std::endl;

    return 0;
}
