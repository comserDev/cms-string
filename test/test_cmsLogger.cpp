#define CMS_LOGGER_TEST     1

#ifdef CMS_LOGGER_TEST

#include <iostream>
#include "../src/cmsAsyncLogger.h"

/**
 * @brief 테스트용 커스텀 로거
 * handleLog 오버라이딩을 통해 필터링 기능을 테스트합니다.
 */
class TestLogger : public cms::AsyncLogger<128, 16> {
protected:
    bool handleLog(const cms::StringBase& msg) override {
        if (msg.contains("SECRET")) {
            std::cout << "[Override] 보안 정책에 의해 로그가 차단되었습니다. (큐 저장 안함)" << std::endl;
            return true; // true 반환 시 큐에 저장되지 않음
        }
        if (msg.contains("CRITICAL")) {
            std::cout << "!!! 긴급 알림 발생 !!!" << std::endl;
            // pushToQueue를 사용하여 메시지를 변형해 수동으로 넣을 수도 있습니다.
            if (msg.contains("RETRY")) {
                cms::String<128> retryMsg = "[RETRY-SYSTEM] ";
                retryMsg << msg;
                pushToQueue(retryMsg);
                return true; // 원본 대신 수정본을 넣었으므로 true 반환
            }
        }
        return false; // false 반환 시 로거가 평소대로 큐에 저장
    }
};

int main() {
    // 1. 로거 인스턴스 획득 및 초기화
    auto& logger = cms::AsyncLogger<>::instance();
    logger.begin(cms::LogLevel::Debug, true);

    std::cout << "=== Test 1: 기본 로깅 및 자동 스타일링 ===" << std::endl;
    logger.d("디버그 메시지입니다. (Code: %d)", 101);
    logger.i("정보 메시지이며 [%s] 태그를 포함합니다.", "Network");
    logger.w("경고! [Sensor] 데이터가 불안정합니다. (현재값: %.2f)", 85.43f);
    logger.e("에러 발생: FATAL 오류가 감지되었습니다."); // FATAL은 강조 키워드

    // 비동기 로거이므로 큐에 쌓인 내용을 명시적으로 출력 (실제 환경에선 백그라운드 루프가 수행)
    while (logger.update());

    std::cout << "\n=== Test 2: 런타임 로그 레벨 필터링 ===" << std::endl;
    logger.setRuntimeLevel(cms::LogLevel::Warn);

    logger.i("이 정보 로그는 출력되지 않아야 합니다.");
    logger.w("이 경고 로그는 출력되어야 합니다.");

    while (logger.update());

    std::cout << "\n=== Test 3: 커스텀 핸들러 (Dispatch) 테스트 ===" << std::endl;
    TestLogger testLog;
    testLog.begin(cms::LogLevel::Debug);

    testLog.i("이것은 일반적인 로그입니다.");
    testLog.e("이 로그는 SECRET 정보를 포함하고 있어 차단됩니다.");
    testLog.w("이것은 CRITICAL 경고입니다.");

    while (testLog.update());

    std::cout << "\n=== Test 4: 큐 오버플로우 테스트 ===" << std::endl;

    // 큐 깊이(16)보다 많은 로그를 빠르게 투입
    for (int i = 0; i < 20; ++i) {
        logger.i("연속 로그 테스트 #%d", i);
    }

    std::cout << "큐에 저장된 마지막 16개의 로그만 출력됩니다:" << std::endl;
    while (logger.update());

    return 0;
}

#endif // CMS_LOGGER_TEST
