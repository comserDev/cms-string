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
        /**
         * @brief 커스텀 로그 처리 콜백 타입
         * @return true: 핸들러가 로그를 처리함, false: 로거가 기본 큐에 저장해야 함
         */
        using HandlerFunc = bool (*)(LoggerBase&, const cms::StringBase&);

        /**
         * @brief 로거를 초기화합니다.
         *
         * @param level 초기 런타임 로그 레벨 (기본값: Debug)
         * @param useColor ANSI 색상 코드 사용 여부 (기본값: true)
         */
        void begin(LogLevel level = LogLevel::Debug, bool useColor = true) noexcept;
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

        /// @brief 커스텀 핸들러를 설정합니다.
        void setHandler(HandlerFunc handler) noexcept { _handler = handler; }

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
        HandlerFunc _handler = nullptr;

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
        /// @brief 플랫폼별 실제 출력을 담당하는 가상 함수
        virtual void outputLog(const cms::StringBase& msg);
    };

    /// @brief 컴파일 타임 로그 레벨 필터링을 지원하는 고성능 로거
    /// @tparam MSG_SIZE 로그 한 줄의 최대 바이트 크기
    /// @tparam QUEUE_DEPTH 로그 큐에 저장할 수 있는 최대 메시지 개수
    template <uint16_t MSG_SIZE = 256, uint8_t QUEUE_DEPTH = 16>
    class AsyncLogger : public LoggerBase {
    public:
        using HandlerFunc = LoggerBase::HandlerFunc;

        static AsyncLogger& instance() {
            static AsyncLogger inst;
            return inst;
        }

        /// @brief 가공된 로그를 큐에 직접 넣습니다. (핸들러 내부에서 호출용)
        void pushToQueue(const cms::String<MSG_SIZE>& logMsg) { _queue.enqueue(logMsg); }

        /// @brief 큐에 쌓인 로그를 하나 꺼내어 출력합니다. (백그라운드 태스크에서 호출)
        bool update();

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

// ==================================================================================================
// [Global Logger Instance]
// logger.i(...)와 같이 편리하게 사용할 수 있도록 전역 참조 객체를 제공합니다.
// ==================================================================================================
inline cms::AsyncLogger<>& logger = cms::AsyncLogger<>::instance();
