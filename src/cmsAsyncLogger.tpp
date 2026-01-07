/// @author comser.dev
/// @brief cmsAsyncLogger 템플릿 클래스의 구현부입니다.
/// @note 이 파일은 확장자가 .tpp이며 cmsAsyncLogger.h 하단에 포함됩니다.


#pragma once

namespace cms {

    template <uint16_t MSG_SIZE, uint8_t QUEUE_DEPTH>
    bool AsyncLogger<MSG_SIZE, QUEUE_DEPTH>::update() {
        cms::String<MSG_SIZE> msg;
        if (_queue.pop(msg)) {
            outputLog(msg);
            return true;
        }
        return false;
    }

    template <uint16_t MSG_SIZE, uint8_t QUEUE_DEPTH>
    void AsyncLogger<MSG_SIZE, QUEUE_DEPTH>::vlog(LogLevel level, const char* format, va_list args) {
        if (level < _runtimeLevel) return;

        cms::String<MSG_SIZE> finalLog;
        cms::String<MSG_SIZE> rawBody;

        // 베이스 클래스의 공통 로직 호출
        LoggerBase::logV(finalLog, rawBody, level, format, args);
    }

    template <uint16_t MSG_SIZE, uint8_t QUEUE_DEPTH>
    void AsyncLogger<MSG_SIZE, QUEUE_DEPTH>::dispatchLog(const char* msg) {
        cms::String<MSG_SIZE> s = msg;
        dispatchLog(s);
    }

    template <uint16_t MSG_SIZE, uint8_t QUEUE_DEPTH>
    void AsyncLogger<MSG_SIZE, QUEUE_DEPTH>::dispatchLog(const cms::String<MSG_SIZE>& logMsg) {
        if (!handleLog(logMsg)) {
            // handleLog가 false를 반환한 경우에만 기본 큐에 저장합니다.
            _queue.enqueue(logMsg);
        }
    }

} // namespace cms
