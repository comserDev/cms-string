/// @author comser.dev
///
/// 고정 크기 버퍼를 소유하여 힙 메모리 할당 없이 동작하는 문자열 컨테이너입니다.
/// 제로 힙(Zero-Heap) 정책을 준수하며, StringBase의 로직을 상속받아 코드 중복을 최소화합니다.

#pragma once

#include <stdarg.h>  // va_list
#include <stdio.h>   // size_t
#include <cstring>   // strlen, memcpy, memmove
#include "cmsStringUtil.h" // cms::string helpers (UTF-8, regex, etc.)
#include "cmsStringBase.h" // StringBase API

namespace cms {

// ==================================================================================================
// [String] 개요
// - 왜 존재하는가: 임베디드 환경에서 메모리 파편화 없이 안전하게 문자열을 다루기 위해 존재합니다.
// - 어떻게 동작하는가: 템플릿 인자 N만큼의 정적 배열을 소유하며, StringBase를 통해 가공 로직을 수행합니다.
// ==================================================================================================

    /// 고정 크기 버퍼를 소유하는 문자열 컨테이너 클래스 템플릿입니다.
    ///
    /// Why: 런타임 중 동적 할당을 배제하여 시스템 안정성을 확보하기 위함입니다.
    /// How: 클래스 내부에 char 배열을 직접 포함하며, 상속을 통해 공통 로직을 재사용합니다.
    ///
    /// @tparam N 널 종료 문자를 포함한 물리적 버퍼 크기 (단위: bytes)
    template<size_t N>
    class String : public StringBase {
    public:
        // --------------------------------------------------------------------------------------------------
        // 스택 오버플로우 방지를 위한 최대 안전 크기 가이드라인입니다. (Byte)
        // --------------------------------------------------------------------------------------------------
        static constexpr size_t MAX_SAFE_SIZE = 1024;

        // --------------------------------------------------------------------------------------------------
        // [컴파일 타임 검증] 버퍼 크기 N이 최소 1바이트 이상인지 확인합니다.
        // --------------------------------------------------------------------------------------------------
        static_assert(N > 0, "cms::String size N must be at least 1 for the null terminator.");

        // --------------------------------------------------------------------------------------------------
        // [컴파일 타임 검증] 버퍼 크기가 안전 한계치를 초과하지 않는지 확인합니다.
        // --------------------------------------------------------------------------------------------------
        static_assert(N <= MAX_SAFE_SIZE, "cms::String size N exceeds safety limit. Large buffers can cause stack overflow.");

        /// 실제 문자열 데이터가 저장되는 정적 char 배열입니다.
        char _data[N];

        /// 기본 생성자입니다. 버퍼를 비우고 관리 체계를 초기화합니다.
        ///
        /// Why: 객체 생성 시 안전한 빈 문자열 상태를 보장하기 위함입니다.
        /// How: 부모 클래스에 버퍼 주소를 전달하고 첫 바이트를 null로 설정합니다.
        ///
        /// 사용 예:
        /// @code
        /// cms::String<32> s;
        /// @endcode
        String() : StringBase(_data, N, 0) {
            // Why: 배열 전체를 0으로 초기화(_data{})하는 것은 대형 버퍼에서 성능 저하를 유발함.
            // How: 첫 바이트만 null로 설정하여 논리적 빈 문자열 상태를 만듦.
            if (N > 0) _data[0] = '\0';
        }

        /// 문자열 리터럴로부터 객체를 생성합니다.
        ///
        /// Why: 선언과 동시에 값을 할당하는 편의성을 제공하기 위함입니다.
        /// How: 컴파일 타임에 리터럴 크기를 검사하고 루프를 통해 데이터를 복사합니다.
        ///
        /// 사용 예:
        /// @code
        /// cms::String<16> s("Hello");
        /// @endcode
        ///
        /// @param src 복사할 문자열 리터럴 (배열 참조)
        template<size_t M>
        String(const char (&src)[M]) : StringBase(_data, N, M - 1) {
            static_assert(M <= N, "String literal exceeds cms::String buffer capacity.");
            memcpy(_data, src, M); // 널 종료 문자 포함 복사
            updatePeak();
        }

        /// C 스타일 문자열 포인터로부터 객체를 생성합니다.
        ///
        /// Why: 외부에서 전달된 문자열 포인터를 기반으로 객체를 생성하기 위함입니다.
        /// How: 대입 연산자를 호출하여 안전하게 데이터를 복사합니다.
        ///
        /// 사용 예:
        /// @code
        /// cms::String<32> s(somePtr);
        /// @endcode
        ///
        /// @param src 복사할 원본 문자열 포인터
        String(const char* src) : StringBase(_data, N) {
            *this = src;
        }

        /// Token 객체로부터 객체를 생성합니다.
        String(const cms::string::Token& token) : StringBase(_data, N, 0) {
            *this = token;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator=] C 문자열을 대입합니다.
        //
        // Usage: s = "OK";
        //
        // @param src 복사할 원본 문자열 포인터 (Null-terminated)
        // @return 자기 자신의 참조
        // --------------------------------------------------------------------------------------------------
        String& operator=(const char* src) {
            StringBase::operator=(src);
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator=] 동일한 크기의 String 객체를 대입합니다.
        //
        // Usage: s = other;
        // --------------------------------------------------------------------------------------------------
        String& operator=(const String& other) {
            StringBase::operator=(other);
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator=] 다른 크기(M)의 String 객체를 대입합니다.
        //
        // Usage: s = other;
        //
        // @param other 복사 대상 String 객체
        // @return 자기 자신의 참조
        // --------------------------------------------------------------------------------------------------
        template<size_t M>
        String& operator=(const String<M>& other) {
            StringBase::operator=(other);
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator=] Token 객체를 대입합니다.
        // --------------------------------------------------------------------------------------------------
        String& operator=(const cms::string::Token& token) {
            StringBase::operator=(token);
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator+=] C 문자열을 덧붙입니다.
        //
        // Usage: s += "OK";
        // --------------------------------------------------------------------------------------------------
        String& operator+=(const char* src) {
            StringBase::operator+=(src);
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator+=] 단일 문자를 덧붙입니다.
        //
        // Usage: s += '!';
        // --------------------------------------------------------------------------------------------------
        String& operator+=(char c) {
            StringBase::operator+=(c);
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator+=] 다른 크기 String을 덧붙입니다.
        //
        // Usage: s += other;
        // --------------------------------------------------------------------------------------------------
        template<size_t M>
        String& operator+=(const String<M>& other) {
            append(other.c_str(), other.length());
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator+=] Token 객체를 덧붙입니다.
        // --------------------------------------------------------------------------------------------------
        String& operator+=(const cms::string::Token& token) {
            StringBase::operator+=(token);
            return *this;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator+] C 문자열을 결합해 새 객체를 반환합니다.
        //
        // Usage: auto out = s + "OK";
        //
        // @param src 결합할 문자열 포인터
        // @return 결합된 결과를 담은 새로운 String<N> 객체
        // --------------------------------------------------------------------------------------------------
        String<N> operator+(const char* src) const {
            String<N> res = *this;
            res += src;
            return res;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator+] 단일 문자를 결합해 새 객체를 반환합니다.
        //
        // Usage: auto out = s + '!';
        // --------------------------------------------------------------------------------------------------
        String<N> operator+(char c) const {
            String<N> res = *this;
            res += c;
            return res;
        }

        // --------------------------------------------------------------------------------------------------
        // [operator+] 다른 String을 결합해 새 객체를 반환합니다.
        //
        // Usage: auto out = s + other;
        // --------------------------------------------------------------------------------------------------
        template<size_t M>
        String<N> operator+(const String<M>& other) const {
            String<N> res = *this;
            res.append(other.c_str(), other.length());
            return res;
        }

        /// 함수 호출 연산자를 통한 암시적 초기화를 차단합니다.
        ///
        /// Why: 과거 operator()를 사용한 초기화 방식이 로직상의 혼선을 주고 버퍼 관리 버그를 유발했기 때문입니다.
        /// How: delete 키워드를 사용하여 컴파일 타임에 사용을 금지합니다.
        ///
        /// @note 문자열 초기화는 명시적인 clear() 또는 operator=를 사용하세요.
        String<N>& operator()(const char* s = nullptr) = delete;

        // --------------------------------------------------------------------------------------------------
        // [operator<<] 다양한 타입을 스트림 방식으로 결합합니다.
        //
        // Usage: s << "A" << 1;
        // --------------------------------------------------------------------------------------------------
        String<N>& operator<<(const char* s) { StringBase::operator<<(s); return *this; }

        /// 리터럴 전용 스트림 연산자 오버라이드 (반환 타입 유지용)
        template<size_t M>
        String<N>& operator<<(const char (&s)[M]) {
            StringBase::operator<<(s);
            return *this;
        }

        template<size_t M>
        String<N>& operator+=(const char (&s)[M]) {
            StringBase::operator+=(s);
            return *this;
        }

        String<N>& operator<<(char c) { StringBase::operator<<(c); return *this; }
        String<N>& operator<<(int v) { StringBase::operator<<(v); return *this; }
        String<N>& operator<<(long v) { StringBase::operator<<(v); return *this; }
        String<N>& operator<<(unsigned long v) { StringBase::operator<<(v); return *this; }
        String<N>& operator<<(float v) { StringBase::operator<<(v); return *this; }
        String<N>& operator<<(double v) { StringBase::operator<<(v); return *this; }
        String<N>& operator<<(const StringBase& other) { StringBase::operator<<(other); return *this; }
        String<N>& operator<<(const cms::string::Token& token) { StringBase::operator<<(token); return *this; }

        using StringBase::substring;
        using StringBase::byteSubstring;

        // --------------------------------------------------------------------------------------------------
        // [substring] 글자 단위 범위를 추출합니다.
        //
        // Usage: auto sub = s.substring(0, 5);
        //
        // @param left 시작 글자 인덱스 (0부터 시작)
        // @param right 종료 글자 인덱스 (0일 경우 끝까지 추출)
        // @return 추출된 문자열을 담은 새로운 String<N> 객체
        // @note 편의성을 위한 함수이며, 대형 버퍼 사용 시 스택 소모에 주의하세요.
        // --------------------------------------------------------------------------------------------------
        String<N> substring(size_t left, size_t right = 0) const {
            String<N> res;
            StringBase::substring(res, left, right);
            return res;
        }

        // --------------------------------------------------------------------------------------------------
        // [byteSubstring] 바이트 오프셋 기준으로 부분 문자열을 추출합니다.
        //
        // Usage: auto sub = s.byteSubstring(0, 8);
        // --------------------------------------------------------------------------------------------------
        String<N> byteSubstring(size_t startByte, size_t endByte = 0) const {
            String<N> res;
            StringBase::byteSubstring(res, startByte, endByte);
            return res;
        }
    };

    // ==================================================================================================
    // [Helper Functions]
    // ==================================================================================================

    /// [copyTokens] Token 배열의 내용을 String<N> 배열로 안전하게 복사합니다.
    ///
    /// Why: Token은 원본 문자열을 참조만 하는 포인터 래퍼이므로, 원본이 사라지기 전에 독립적인 메모리(String 객체)로 데이터를 옮겨야 할 때 사용합니다.
    /// How: 대상 배열의 크기(M)와 실제 토큰 개수 중 작은 값을 기준으로 순회하며, 각 String 객체의 append를 호출하여 데이터를 복사합니다.
    ///
    /// 사용 예:
    /// @code
    /// cms::string::Token tokens[3];
    /// cms::String<32> results[3];
    /// size_t n = copyTokens(tokens, 3, results);
    /// @endcode
    ///
    /// @tparam N 대상 String의 버퍼 크기
    /// @tparam M 대상 배열의 크기
    /// @param tokens split 결과로 얻은 토큰 배열
    /// @param tokenCount 실제 분리된 토큰 개수
    /// @param dest 복사될 String<N> 배열
    /// @return 실제 복사된 개수
    template<size_t N, size_t M>
    size_t copyTokens(const cms::string::Token* tokens, size_t tokenCount, String<N> (&dest)[M]) {
        // 1) 경계 검사: 토큰 개수가 목적지 배열 크기를 넘지 않도록 제한
        size_t count = (tokenCount < M) ? tokenCount : M;
        for (size_t i = 0; i < count; ++i) {
            // 2) 데이터 복사: 기존 내용을 비우고 토큰이 가리키는 영역을 복사
            dest[i].clear();
            dest[i].append(tokens[i].ptr, tokens[i].len);
        }
        return count;
    }

    /// [splitTo] 문자열을 분리하여 즉시 String<N> 배열로 복사합니다.
    ///
    /// Why: 분리와 복사라는 두 단계를 하나로 합쳐 사용자 코드의 가독성을 높이고 실수를 방지하기 위함입니다.
    /// How: 내부적으로 임시 Token 배열을 스택에 생성하여 비파괴적 split을 수행한 뒤, copyTokens를 통해 최종 배열로 데이터를 옮깁니다.
    ///
    /// 사용 예:
    /// @code
    /// cms::String<32> params[3];
    /// size_t count = splitTo(src, ':', params);
    /// @endcode
    ///
    /// @tparam N 결과 String의 버퍼 크기
    /// @tparam M 결과 배열의 크기
    /// @param src 원본 문자열 객체
    /// @param delimiter 분리 기준 문자
    /// @param dest 결과를 저장할 String<N> 배열
    /// @return 실제 분리 및 복사된 토큰 개수
    template<size_t N, size_t M>
    size_t splitTo(const StringBase& src, char delimiter, String<N> (&dest)[M]) {
        // 1) 임시 토큰 배열 생성 (스택 할당)
        cms::string::Token tokens[M];
        // 2) 비파괴적 분할 수행
        size_t count = src.split(delimiter, tokens, M);
        // 3) 결과를 String 객체 배열로 변환하여 반환
        return copyTokens(tokens, count, dest);
    }
}
