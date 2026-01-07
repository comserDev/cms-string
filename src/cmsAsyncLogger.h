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

    /// [LogLevel] 로그 출력 우선순위 정의
    ///
    /// 시스템 상태의 심각도를 분류하여 런타임에 필터링하기 위해 사용됩니다.
    enum class LogLevel : uint8_t {
        Debug = 0,  ///< 상세 디버깅 정보
        Info,       ///< 일반적인 시스템 상태 알림
        Warn,       ///< 잠재적 문제 경고
        Error,      ///< 실행 중 발생한 오류
        None        ///< 모든 로그 차단
    };

// ==================================================================================================
// [LoggerBase] 개요
// - 왜 존재하는가: 템플릿 인자(N)에 의존하지 않는 공통 로깅 로직을 분리하여 코드 비대화(Code Bloat)를 방지합니다.
// - 어떻게 동작하는가: 문자열 포맷팅, 타임스탬프 생성, ANSI 스타일링 등 무거운 로직을 처리하고 가상 함수를 통해 최종 출력을 위임합니다.
// ==================================================================================================

    /// LoggerBase의 역할 요약
    ///
    /// Why: 다양한 버퍼 크기를 가진 로거들이 동일한 가공 로직을 공유하여 Flash 메모리를 절약하기 위함입니다.
    /// How: 외부에서 주입된 StringBase 버퍼를 기반으로 인플레이스 가공을 수행하며, 순수 가상 함수로 출력 인터페이스를 정의합니다.
    class LoggerBase {
    public:
        /// [begin] 로거 초기화
        ///
        /// 로거의 동작에 필요한 초기 로그 레벨과 색상 사용 여부를 설정합니다.
        ///
        /// 사용 예:
        /// @code
        /// logger.begin(LogLevel::Info, true);
        /// @endcode
        ///
        /// @param level 초기 출력 제한 레벨 (기본값: Debug)
        /// @param useColor ANSI 색상 코드 적용 여부 (기본값: true)
        void begin(LogLevel level = LogLevel::Debug, bool useColor = true) noexcept;

        /// [systemTimeSynced] 시간 동기화 상태 설정
        ///
        /// 시스템 시간이 동기화되었는지 여부를 설정하여 로그 타임스탬프 형식을 결정합니다.
        /// @param synced true: [HH:MM:SS] 형식, false: [Uptime] 형식
        void systemTimeSynced(bool synced) noexcept;

        /// [setRuntimeLevel] 출력 레벨 변경
        ///
        /// 실행 중에 로그 출력 레벨을 동적으로 변경합니다. 설정된 레벨보다 낮은 로그는 무시됩니다.
        void setRuntimeLevel(LogLevel level) noexcept;

        /// [getRuntimeLevel] 현재 출력 레벨 조회
        LogLevel getRuntimeLevel() const noexcept { return _runtimeLevel; }

        /// [setUseColor] 색상 모드 설정
        void setUseColor(bool useColor) noexcept;

        /// [isUsingColor] 색상 모드 활성화 여부 확인
        bool isUsingColor() const noexcept { return _useColor; }

        /// [setLogLevel] setRuntimeLevel의 별칭 (하위 호환성)
        void setLogLevel(LogLevel level) noexcept;

        // ---------------------------------------------------------
        // [i/d/w/e] 편리한 로그 출력을 위한 헬퍼 메서드 (Base로 이동)
        // ---------------------------------------------------------

        /// [d] Debug 레벨 로그 출력 (Cyan)
        void d(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Debug
        /// [i] Info 레벨 로그 출력 (Green)
        void i(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Info
        /// [w] Warn 레벨 로그 출력 (Yellow)
        void w(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Warn
        /// [e] Error 레벨 로그 출력 (Red)
        void e(const char* format, ...) CMS_PRINTF_CHECK(2, 3); // Error

        /// [log] 지정된 레벨로 로그 출력
        ///
        /// 런타임 레벨 체크를 수행한 후 비동기 큐에 로그를 쌓습니다.
        void log(LogLevel level, const char* format, ...) CMS_PRINTF_CHECK(3, 4);

    protected:
        LoggerBase() = default;
        virtual ~LoggerBase() = default;

        bool _timeSynced = false;           ///< 시간 동기화 여부 플래그
        bool _useColor = true;              ///< ANSI 색상 사용 여부 플래그
        LogLevel _runtimeLevel = LogLevel::Debug; ///< 현재 필터링 레벨

        /// [handleLog] 로그 가로채기 및 필터링
        ///
        /// 가공된 로그가 비동기 큐에 들어가기 직전에 호출되어 보안 필터링이나 즉각적인 대응을 수행합니다.
        ///
        /// @param msg 가공이 완료된 최종 로그 문자열
        /// @return true: 로그를 가로채서 처리함(큐 저장 안함), false: 큐에 저장하도록 허용함 (기본값)
        virtual bool handleLog(const cms::StringBase& msg) { (void)msg; return false; }

        /// [getLevelString] 레벨별 약어 문자열 반환 (D, I, W, E)
        const char* getLevelString(LogLevel level) noexcept;
        /// [getColorCode] 레벨별 ANSI 색상 코드 반환
        const char* getColorCode(LogLevel level) noexcept;
        /// [applyStyling] [태그] 및 키워드 강조 스타일링 적용
        void applyStyling(cms::StringBase& out, const char* rawMsg, LogLevel level);
        /// [appendWithKeywords] 특정 키워드(FATAL 등)를 찾아 강조 스타일 추가
        void appendWithKeywords(cms::StringBase& out, const char* src, size_t len);

        /// [logV] 로그 메시지 조립 핵심 로직
        ///
        /// 타임스탬프, 레벨 배지, 스타일링을 적용하여 최종 로그 문자열을 완성합니다.
        /// @param out 결과가 저장될 버퍼
        /// @param tmp 포맷팅에 사용될 임시 버퍼
        /// @param level 로그 레벨
        /// @param format printf 스타일 포맷
        /// @param args 가변 인자 리스트
        void logV(cms::StringBase& out, cms::StringBase& tmp, LogLevel level, const char* format, va_list args);

        /// [vlog] 자식 클래스에 버퍼 제공 요청 (순수 가상 함수)
        virtual void vlog(LogLevel level, const char* format, va_list args) = 0;
        /// [dispatchLog] 가공된 로그를 큐로 전달 (순수 가상 함수)
        virtual void dispatchLog(const char* msg) = 0;

        /// [outputLog] 실제 데이터 출력
        ///
        /// 큐에서 꺼내진 로그 메시지를 시리얼, 네트워크, 파일 등 물리적 매체로 전송합니다.
        /// @param msg 출력할 최종 로그 메시지
        virtual void outputLog(const cms::StringBase& msg);
    };

// ==================================================================================================
// [AsyncLogger] 개요
// - 왜 존재하는가: 로깅 시 발생하는 I/O 지연이 메인 로직의 실시간성에 영향을 주지 않도록 비동기 큐를 제공합니다.
// - 어떻게 동작하는가: 로그 발생 시 메시지를 내부 원형 큐에 즉시 저장하고, update() 호출 시점에 하나씩 꺼내어 출력합니다.
// ==================================================================================================

    /// AsyncLogger의 역할 요약
    ///
    /// Why: 로깅 성능 최적화 및 스레드 안전한 로그 수집을 위함입니다.
    /// How: 템플릿 인자로 지정된 크기의 정적 큐와 스택 버퍼를 관리합니다.
    ///
    /// @tparam MSG_SIZE 로그 한 줄의 최대 바이트 크기
    /// @tparam QUEUE_DEPTH 로그 큐에 저장할 수 있는 최대 메시지 개수
    template <uint16_t MSG_SIZE = 256, uint8_t QUEUE_DEPTH = 16>
    class AsyncLogger : public LoggerBase {
    public:
        /// [instance] 싱글톤 인스턴스 접근
        static AsyncLogger& instance() {
            static AsyncLogger inst;
            return inst;
        }

        /// [pushToQueue] 가공된 로그를 큐에 수동 투입
        ///
        /// handleLog() 내부에서 메시지를 변형한 후 다시 큐에 넣을 때 주로 사용합니다.
        void pushToQueue(const cms::String<MSG_SIZE>& logMsg) { _queue.enqueue(logMsg); }

        /// [update] 보류된 로그 처리
        ///
        /// 비동기 큐에 쌓여있는 로그 메시지를 하나 꺼내어 실제 출력 장치(outputLog)로 전송합니다.
        /// @return true: 로그를 하나 처리함, false: 처리할 로그가 없음
        bool update();

    protected:
        /// [dispatchLog] 문자열을 큐에 저장 가능한 객체로 변환하여 전달
        void dispatchLog(const char* msg) override;
        /// [dispatchLog] 실제 큐 저장 로직 (handleLog 필터링 포함)
        virtual void dispatchLog(const cms::String<MSG_SIZE>& logMsg);
        /// [vlog] 템플릿 크기에 맞는 스택 버퍼를 생성하여 가공 로직 호출
        void vlog(LogLevel level, const char* format, va_list args) override;

    private:
        /// 로그 메시지를 보관하는 스레드 안전 원형 큐
        cms::ThreadSafeQueue<cms::String<MSG_SIZE>, QUEUE_DEPTH, uint8_t> _queue;
    };
} // namespace cms

// 템플릿 구현부 포함 (컴파일을 위해 헤더 하단에 위치)
#include "cmsAsyncLogger.tpp"
