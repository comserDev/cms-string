/// @author comser.dev
/// @brief LoggerBase 비-템플릿 클래스의 구현부입니다.
/// 이 파일은 독립적으로 컴파일되어 코드 비대화를 방지합니다.

#include <ctime>
#include <cstdio>
#include <cstring>
#include "cmsAsyncLogger.h"

// ANSI 이스케이프 시퀀스 정의
#define ANSI_ESC        "\033["
#define ANSI_RESET      "\033[0m"
#define ANSI_BOLD_RED   "\033[1;91m"

#define ANSI_COLOR_CYN  ANSI_ESC "36m"
#define ANSI_COLOR_GRN  ANSI_ESC "32m"
#define ANSI_COLOR_YEL  ANSI_ESC "33m"
#define ANSI_COLOR_RED  ANSI_ESC "31m"

namespace {
    // 강조 키워드 정의
    struct Keyword { const char* name; size_t len; };
    static constexpr Keyword KEYWORDS[] = { {"ERROR", 5}, {"CRITICAL", 8}, {"FATAL", 5}, {"FAIL", 4} };
    static constexpr size_t KEYWORD_COUNT = sizeof(KEYWORDS) / sizeof(KEYWORDS[0]);
    static const char* TAG_COLORS[] = { "92", "93", "94", "95", "96", "32", "33", "35", "36" };
}

namespace cms {

    /// [begin] 로거 초기화 구현
    void LoggerBase::begin(LogLevel level, bool useColor) noexcept {
        _runtimeLevel = level;
        _useColor = useColor;
    }
    /// [systemTimeSynced] 시간 동기화 플래그 설정 구현
    void LoggerBase::systemTimeSynced(bool synced) noexcept { _timeSynced = synced; }
    /// [setRuntimeLevel] 런타임 필터 레벨 설정 구현
    void LoggerBase::setRuntimeLevel(LogLevel level) noexcept { _runtimeLevel = level; }
    /// [setUseColor] 색상 모드 설정 구현
    void LoggerBase::setUseColor(bool useColor) noexcept { _useColor = useColor; }
    /// [setLogLevel] 별칭 메서드 구현
    void LoggerBase::setLogLevel(LogLevel level) noexcept { _runtimeLevel = level; }

    /// [d/i/w/e/log] 가변 인자 래퍼 함수들
    /// 각 레벨에 맞는 vlog 가상 함수를 호출하여 실제 가공을 시작합니다.
    void LoggerBase::d(const char* format, ...) {
        va_list args; va_start(args, format); vlog(LogLevel::Debug, format, args); va_end(args);
    }
    void LoggerBase::i(const char* format, ...) {
        va_list args; va_start(args, format); vlog(LogLevel::Info, format, args); va_end(args);
    }
    void LoggerBase::w(const char* format, ...) {
        va_list args; va_start(args, format); vlog(LogLevel::Warn, format, args); va_end(args);
    }
    void LoggerBase::e(const char* format, ...) {
        va_list args; va_start(args, format); vlog(LogLevel::Error, format, args); va_end(args);
    }
    void LoggerBase::log(LogLevel level, const char* format, ...) {
        if (level < _runtimeLevel || !format) return;
        va_list args; va_start(args, format); vlog(level, format, args); va_end(args);
    }

    /// [logV] 로그 메시지 조립 상세 구현
    ///
    /// 1) 타임스탬프 생성 (KST 또는 Uptime)
    /// 2) 레벨 배지 및 색상 코드 추가
    /// 3) 메시지 본문 포맷팅
    /// 4) [태그] 및 키워드 스타일링 적용
    void LoggerBase::logV(cms::StringBase& out, cms::StringBase& tmp, LogLevel level, const char* format, va_list args) {
        if (level < _runtimeLevel || !format) return;
        out.clear();

        if (_timeSynced) {
            std::time_t now = std::time(nullptr);
            now += 9 * 3600;
            struct std::tm* ti = std::gmtime(&now);
            if (ti) out.appendPrintf("[%02d:%02d:%02d] ", ti->tm_hour, ti->tm_min, ti->tm_sec);
        } else {
#ifdef ARDUINO
            out.appendPrintf("[%lu] ", (unsigned long)millis());
#else
            out.appendPrintf("[%lu] ", (unsigned long)std::clock());
#endif
        }

        if (_useColor) out << getColorCode(level);
        out << "[" << getLevelString(level) << "]";
        if (_useColor) out << ANSI_RESET;
        out << " ";

        tmp.clear();
        tmp.appendPrintf(format, args);

        if (_useColor) applyStyling(out, tmp.c_str(), level);
        else out << tmp;

        dispatchLog(out.c_str());
    }

    /// [applyStyling] 태그 스타일링 구현
    ///
    /// 대괄호로 감싸진 [TAG]를 찾아 DJB2 해시를 기반으로 고유 색상을 입힙니다.
    /// 태그가 아닌 일반 텍스트는 키워드 강조 로직으로 전달합니다.
    void LoggerBase::applyStyling(cms::StringBase& out, const char* rawMsg, LogLevel level) {
        const char* p = rawMsg;
        const char* startBracket;
        while ((startBracket = strchr(p, '[')) != nullptr) {
            appendWithKeywords(out, p, startBracket - p);
            const char* endBracket = strchr(startBracket, ']');
            if (endBracket && (endBracket > startBracket + 1)) {
                unsigned int hash = 5381;
                for (const char* h = startBracket + 1; h < endBracket; h++) {
                    char c = (*h >= 'a' && *h <= 'z') ? (*h - 'a' + 'A') : *h;
                    hash = ((hash << 5) + hash) + (unsigned char)c;
                }
                const char* color = TAG_COLORS[hash % (sizeof(TAG_COLORS) / sizeof(TAG_COLORS[0]))];
                out << ANSI_ESC << color << "m";
                out.append(startBracket, (endBracket - startBracket) + 1);
                out << ANSI_RESET;
                p = endBracket + 1;
            } else {
                out << "[";
                p = startBracket + 1;
            }
        }
        appendWithKeywords(out, p, strlen(p));
    }

    /// [appendWithKeywords] 중요 키워드 강조 구현
    ///
    /// ERROR, FATAL 등 미리 정의된 키워드를 대소문자 구분 없이 찾아
    /// Bold Red 스타일을 적용하여 시인성을 높입니다.
    void LoggerBase::appendWithKeywords(cms::StringBase& out, const char* src, size_t len) {
        if (len == 0) return;
        const char* p = src;
        const char* end = src + len;
        while (p < end) {
            int matchIdx = -1;
            for (size_t i = 0; i < KEYWORD_COUNT; ++i) {
                if (p + KEYWORDS[i].len <= end && cms::string::startsWith(p, KEYWORDS[i].name, KEYWORDS[i].len, true)) {
                    matchIdx = (int)i;
                    break;
                }
            }
            if (matchIdx != -1) {
                out << ANSI_BOLD_RED;
                out.append(p, KEYWORDS[matchIdx].len);
                out << ANSI_RESET;
                p += KEYWORDS[matchIdx].len;
            } else {
                char c = *p++;
                out.append(&c, 1);
            }
        }
    }

    /// [getLevelString] 레벨 약어 매핑 구현
    const char* LoggerBase::getLevelString(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Debug: return "D";
            case LogLevel::Info:  return "I";
            case LogLevel::Warn:  return "W";
            case LogLevel::Error: return "E";
            default:              return "?";
        }
    }

    /// [getColorCode] 레벨 색상 매핑 구현
    const char* LoggerBase::getColorCode(LogLevel level) noexcept {
        switch (level) {
            case LogLevel::Debug: return ANSI_COLOR_CYN;
            case LogLevel::Info:  return ANSI_COLOR_GRN;
            case LogLevel::Warn:  return ANSI_COLOR_YEL;
            case LogLevel::Error: return ANSI_COLOR_RED;
            default:              return "";
        }
    }

    /// [outputLog] 기본 출력 장치 구현
    void LoggerBase::outputLog(const cms::StringBase& msg) {
#ifdef ARDUINO
        Serial.println(msg.c_str());
#else
        std::printf("%s\n", msg.c_str());
#endif
    }

} // namespace cms
