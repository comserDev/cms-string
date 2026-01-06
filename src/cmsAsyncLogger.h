/// @author comser.dev
/// @brief cms-embedded-utils 기반의 고성능 비동기 로거

#pragma once

#include <cstdint>          // uint8_t
#include <cstdarg>          // va_list
#include <ctime>            // time, gmtime
#include <cstdio>           // printf
#include "cmsString.h"
#include "cmsQueue.h"

namespace cms {

    /// 로그 레벨 정의
    enum class LogLevel : uint8_t {
        Debug = 0,
        Info,
        Warn,
        Error,
        None
    };

    /// @brief 템플릿 비대화를 방지하기 위한 비-템플릿 베이스 클래스
    /// 템플릿 인자에 의존하지 않는 공통 로직을 이곳에 모아 .cpp에서 한 번만 컴파일합니다.
    class LoggerBase {
    public:
        /// @brief 로거를 초기화하고 초기 런타임 로그 레벨을 설정합니다.
        void begin(LogLevel level) noexcept;
        /// @brief 시스템 시간 동기화 여부를 설정합니다.
        void systemTimeSynced(bool synced) noexcept;
        /// @brief 런타임 로그 레벨을 설정합니다.
        void setRuntimeLevel(LogLevel level) noexcept;
        LogLevel getRuntimeLevel() const noexcept { return _runtimeLevel; }
        /// @brief ANSI 색상 코드 사용 여부를 설정합니다.
        void setUseColor(bool useColor) noexcept;
        bool isUsingColor() const noexcept { return _useColor; }
        /// @brief setRuntimeLevel의 별칭입니다.
        void setLogLevel(LogLevel level) noexcept;

        // ---------------------------------------------------------
        // [i/d/w/e] 편리한 로그 출력을 위한 헬퍼 메서드 (Base로 이동)
        // ---------------------------------------------------------
        void d(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Debug
        void i(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Info
        void w(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Warn
        void e(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Error

        /// @brief 로그를 큐에 쌓습니다. (비동기 방식)
        void log(LogLevel level, const char* format, ...) CMS_PRINTF_CHECK(3, 4);

    protected:
        LoggerBase() = default;
        virtual ~LoggerBase() = default;

        bool _timeSynced = false;
        bool _useColor = true;
        LogLevel _runtimeLevel = LogLevel::Debug;

        /**
         * @brief 로그가 비동기 큐에 쌓이기 전 필터링하거나 가로채기 위한 가상 함수입니다.
         *
         * @param msg 가공(타임스탬프, 레벨 배지, 스타일링 등)이 완료된 최종 로그 메시지
         * @return
         *   - true: 로그 처리를 여기서 완료함. 로거는 이 메시지를 내부 큐에 저장하지 않습니다.
         *          (특정 키워드 필터링, 보안 로그 차단, 또는 즉시 전송 시 사용)
         *   - false: 로거가 메시지를 내부 큐에 저장하도록 허용합니다. (기본값)
         *
         * @note 이 함수 내부에서 `pushToQueue()`를 호출하여 조건에 따라 메시지를
         *       수동으로 큐에 넣거나, 내용을 수정하여 다시 투입할 수 있습니다.
         *
         * @example
         * bool handleLog(const StringBase& msg) override {
         *     if (msg.contains("RETRY")) { pushToQueue(msg); return true; }
         *     return false;
         * }
         */
        virtual bool handleLog(const cms::StringBase& msg) { (void)msg; return false; }

        // StringBase&를 사용하여 MSG_SIZE에 상관없이 동일한 코드를 공유합니다.
        const char* getLevelString(LogLevel level) noexcept;
        const char* getColorCode(LogLevel level) noexcept;
        void applyStyling(cms::StringBase& out, const char* rawMsg, LogLevel level);
        void appendWithKeywords(cms::StringBase& out, const char* src, size_t len);
        /// @brief 공통 로그 포맷팅 로직 (템플릿 비대화 방지 핵심)
        void logV(cms::StringBase& out, cms::StringBase& tmp, LogLevel level, const char* format, va_list args);

        /// @brief 템플릿 자식 클래스에서 버퍼를 제공받기 위한 가상 함수
        virtual void vlog(LogLevel level, const char* format, va_list args) = 0;
        virtual void dispatchLog(const char* msg) = 0;
        /**
         * @brief 큐에서 꺼내진 로그 메시지를 실제 출력 장치로 전송하는 가상 함수입니다.
         *
         * @details 기본 구현은 Arduino 환경에서 Serial.println(),
         *          그 외 환경에서는 std::printf()를 사용합니다.
         *          TCP, BLE, SD 카드 등 출력 대상을 변경하려면 이 함수를 재정의하세요.
         *
         * @param msg 출력할 최종 로그 메시지
         */
        virtual void outputLog(const cms::StringBase& msg);
    };

    /// @brief 컴파일 타임 로그 레벨 필터링을 지원하는 고성능 로거
    /// @tparam MSG_SIZE 로그 한 줄의 최대 바이트 크기
    /// @tparam QUEUE_DEPTH 로그 큐에 저장할 수 있는 최대 메시지 개수
    template <uint16_t MSG_SIZE = 256, uint8_t QUEUE_DEPTH = 16>
    class AsyncLogger : public LoggerBase {
    public:
        static AsyncLogger& instance() {
            static AsyncLogger inst;
            return inst;
        }

        /// @brief 가공된 로그를 큐에 직접 넣습니다. (핸들러 내부에서 호출용)
        void pushToQueue(const cms::String<MSG_SIZE>& logMsg) { _queue.enqueue(logMsg); }

        /// @brief 큐에 쌓인 로그를 하나 꺼내어 출력합니다. (백그라운드 태스크에서 호출)
        bool processNextLog();

    protected:
        /// @brief 가공된 로그를 최종 목적지(큐 또는 핸들러)로 전달합니다.
        void dispatchLog(const char* msg) override;
        virtual void dispatchLog(const cms::String<MSG_SIZE>& logMsg);
        void vlog(LogLevel level, const char* format, va_list args) override;

    private:
        cms::ThreadSafeQueue<cms::String<MSG_SIZE>, QUEUE_DEPTH, uint8_t> _queue;
    };
} // namespace cms

// 템플릿 구현부 포함 (컴파일을 위해 헤더 하단에 위치)
#include "cmsAsyncLogger.tpp"
