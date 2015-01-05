/* vim: set sw=4 ts=4 et : */
/* matcha.hpp: matcher objects for GoogleTest
 *
 * Copyright (C) 2014 Alexandre Moreno
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#ifndef _MATCHA_H_
#define _MATCHA_H_

#include <utility>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <iterator> 
#include <functional>
#include <set>
#include <vector>
#include <string>
#include <tuple>
#include <cstring>
#include <cctype>
#include <type_traits>
#include <regex>
#include "prettyprint.hpp"

#if defined(MATCHA_GTEST)
#include "gtest/gtest.h"

/* we need the assertThat logic in a macro, so that
 * the ADD_FAILURE will report the right __FILE__ and __LINE__
 */
#define assertThat(actual,matcher)  \
    ASSERT_PRED_FORMAT2(assertResult, actual, matcher)

#else

#define assertThat(actual, matcher) \
    assertResult(nullptr, nullptr, actual, matcher)

#endif

namespace matcha {

// SFINAE type trait to detect whether type T satisfies EqualityComparable.

template<typename T, typename = void>
struct is_equality_comparable : std::false_type
{ };

template<typename T>
struct is_equality_comparable<T,
    typename std::enable_if<
        true,
        decltype((std::declval<T>() == std::declval<T>()), (void)0)
        >::type
    > : std::true_type
{ };

// SFINAE type trait to detect whether type T satisfies LessThanComparable.

template<typename T, typename = void>
struct is_lessthan_comparable : std::false_type
{ };

template<typename T>
struct is_lessthan_comparable<T,
    typename std::enable_if<
        true,
        decltype((std::declval<T>() < std::declval<T>()), (void)0)
        >::type
    > : std::true_type
{ };

// templated operator<< when type T is not std container (prettyprint)
// and user-defined insertion operator is not provided

template<typename T, typename TChar, typename TCharTraits>
typename std::enable_if<
    !::pretty_print::is_container<T>::value, 
    std::basic_ostream<TChar, TCharTraits>&
    >::type
operator<<(std::basic_ostream<TChar, TCharTraits> &os, const T &)
{
    const char s[] = "<unknown-type>";
    os.write(s, sizeof(s) - 1);
    return os;
}

/*
 * character traits to provide case-insensitive comparison
 * http://www.gotw.ca/gotw/029.htm 
 */
struct ci_char_traits : public std::char_traits<char> {
    static bool eq(char c1, char c2) {
         return std::toupper(c1) == std::toupper(c2);
     }
    static bool lt(char c1, char c2) {
         return std::toupper(c1) <  std::toupper(c2);
    }
    static int compare(const char* s1, const char* s2, size_t n) {
        while (n-- != 0) {
            if (std::toupper(*s1) < std::toupper(*s2))
                return -1;
            if (std::toupper(*s1) > std::toupper(*s2)) 
                return 1;
            ++s1; ++s2;
        }
        return 0;
    }
    static const char* find(const char* s, int n, char a) {
        auto const ua (std::toupper(a));
        while (n-- != 0) {
            if (std::toupper(*s) == ua)
                return s;
            s++;
        }
        return nullptr;
    }
};
 
// case-insensitive string class
typedef std::basic_string<char, ci_char_traits> ci_string;
 
std::ostream& operator<<(std::ostream& os, const ci_string& str) 
{
    return os.write(str.data(), str.size());
}

struct StandardOutputPolicy {
    typedef bool return_type;
protected:
    bool print(std::string const& expected, std::string const& actual, bool assertion) const {
        if (!assertion) {
            std::cout << "Expected: " << expected << "\n but got: " << actual << std::endl;
            return false;
        }
        return true;
    }
};

struct ExceptionOutputPolicy {
    typedef void return_type;
protected:
    void print(std::string const& expected, std::string const& actual, bool assertion) const {
        if (!assertion) {
            std::ostringstream ostream;
            ostream << "Expected: " << expected << "\n but got: " << actual << std::endl;
            throw std::logic_error(ostream.str());
            return;
        }
        return;
    }
};

#if defined(MATCHA_GTEST)

struct GTestOutputPolicy {
    typedef ::testing::AssertionResult return_type;
protected:
    // Google Test implementation of matcher assertions
    ::testing::AssertionResult print(std::string const& expected, 
                                     std::string const& actual, 
                                     bool assertion) const {
        if (!assertion) {
            return ::testing::AssertionFailure() 
                   << "Expected: " << expected << "\n but got: " << actual;
        }
        return ::testing::AssertionSuccess();
    }
};

#define MATCHA_OUTPUT_POLICY GTestOutputPolicy

#endif

#if !defined(MATCHA_OUTPUT_POLICY)

#define MATCHA_OUTPUT_POLICY StandardOutputPolicy

#endif

template<class MatcherPolicy,class ExpectedType = void, class OutputPolicy = MATCHA_OUTPUT_POLICY>
class Matcher : private MatcherPolicy, private OutputPolicy {
public:
    typedef Matcher<MatcherPolicy,ExpectedType> type;
    Matcher(ExpectedType const& value = ExpectedType()) : expected_(value)
    { }

    template<class ActualType>
    bool matches(ActualType const& actual) const {
        return MatcherPolicy::matches(expected_, actual);
    }

    template<size_t N>
    bool matches(ExpectedType const (&actual)[N]) const {
        std::vector<ExpectedType> wrapper(std::begin(actual), std::end(actual));
        return MatcherPolicy::matches(expected_, wrapper);
    }

    template<size_t N>
    bool matches(char const (&actual)[N]) const {
        using namespace std;
        return MatcherPolicy::matches(expected_, string(actual));
    }

    template<class ActualType>
    friend auto assertResult(const char*, 
                             const char*,
                             ActualType const& actual,
                             type const& matcher) -> typename OutputPolicy::return_type {
        std::ostringstream sactual, smatcher;
        sactual << actual;
        smatcher << matcher;
        return matcher.print(smatcher.str(), sactual.str(), matcher.matches(actual));
    }

    template<size_t N>
    friend auto assertResult(const char*,
                             const char*,
                             ExpectedType const (&actual)[N],
                             type const& matcher) -> typename OutputPolicy::return_type {
        std::ostringstream sactual, smatcher;
        sactual << actual;
        smatcher << matcher;
        return matcher.print(smatcher.str(), sactual.str(), matcher.matches(actual));
    }

    friend std::ostream& operator<<(std::ostream& o, type const& matcher) {
        matcher.describe(o);
        return o; 
    }
private:
    void describe(std::ostream& o) const {
        MatcherPolicy::describe(o, expected_);
    }
    ExpectedType expected_;
};

// there is no expected value, for matcher not taking input parameters
template<class MatcherPolicy, class OutputPolicy>
class Matcher<MatcherPolicy,void,OutputPolicy> : private MatcherPolicy, private OutputPolicy {
public:
    typedef Matcher<MatcherPolicy> type;

    template<class ActualType>
    bool matches(ActualType const& actual) const {
        return MatcherPolicy::matches(actual);
    }

    template<class ActualType>
    friend auto assertResult(const char*, 
                             const char*,
                             ActualType const& actual,
                             type const& matcher) -> typename OutputPolicy::return_type {
        std::ostringstream sactual, smatcher;
        sactual << actual;
        smatcher << matcher;
        return matcher.print(smatcher.str(), sactual.str(), matcher.matches(actual));
    }

    friend std::ostream& operator<<(std::ostream& o, type const& matcher) {
        matcher.describe(o);
        return o; 
    }
private:
    void describe(std::ostream& o) const {
        MatcherPolicy::describe(o);
    }
};

// pointers
template<class MatcherPolicy, class ExpectedType, class OutputPolicy>
class Matcher<MatcherPolicy,ExpectedType*,OutputPolicy> : private MatcherPolicy, private OutputPolicy {
public:
    typedef Matcher<MatcherPolicy,ExpectedType*> type;
    Matcher(ExpectedType const* pvalue) : expected_(pvalue) { }

    template<class ActualType>
    bool matches(ActualType const* actual) const {
        return MatcherPolicy::matches(expected_, actual);
    }

    template<class ActualType>
    friend auto assertResult(const char*,
                             const char*,
                             ActualType const* actual, 
                             type const& matcher) -> typename OutputPolicy::return_type {
        std::ostringstream sactual, smatcher;
        sactual << actual;
        smatcher << matcher;
        return matcher.print(smatcher.str(), sactual.str(), matcher.matches(actual));
    }

    friend std::ostream& operator<<(std::ostream& o, type const& matcher) {
        matcher.describe(o);
        return o; 
    }
private:
    void describe(std::ostream& o) const {
        MatcherPolicy::describe(o, expected_);
    }
    ExpectedType const* expected_;
};

// raw C-style arrays are wrapped in std::vector
template<class MatcherPolicy, class ExpectedType, size_t N, class OutputPolicy>
class Matcher<MatcherPolicy,ExpectedType[N],OutputPolicy> : private MatcherPolicy, private OutputPolicy {
public:
    typedef Matcher<MatcherPolicy,ExpectedType[N]> type;
    Matcher(ExpectedType const (&value)[N]) 
        : expected_(std::begin(value), std::end(value)) { }

    template<size_t M>
    bool matches(ExpectedType const (&actual)[M]) const {
        std::vector<ExpectedType> wrapper(std::begin(actual), std::end(actual));
        return MatcherPolicy::matches(expected_, wrapper);
    }

    template<size_t M>
    friend auto assertResult(const char*,
                             const char*,
                             ExpectedType const (&actual)[M], 
                             type const& matcher) -> typename OutputPolicy::return_type {
        std::ostringstream sactual, smatcher;
        sactual << actual;
        smatcher << matcher;
        return matcher.print(smatcher.str(), sactual.str(), matcher.matches(actual));
    }

    friend std::ostream& operator<<(std::ostream& o, type const& matcher) {
        matcher.describe(o);
        return o; 
    }
private:
    void describe(std::ostream& o) const {
        MatcherPolicy::describe(o, expected_);
    }
    std::vector<ExpectedType> expected_;
};


// null-terminated strings are converted to std::string
template<class MatcherPolicy, size_t N, class OutputPolicy>
class Matcher<MatcherPolicy,char[N],OutputPolicy> : private MatcherPolicy, private OutputPolicy {
public:
    typedef Matcher<MatcherPolicy,char[N]> type;
    Matcher(char const (&value)[N]) 
        : expected_(value) { }

    template<size_t M>
    bool matches(char const (&actual)[M]) const {
        using namespace std;
        return MatcherPolicy::matches(expected_, string(actual));
    }

    bool matches(std::string const& actual) const {
        return MatcherPolicy::matches(expected_, actual);
    }

    template<size_t M>
    friend auto assertResult(const char*,
                             const char*,
                             char const (&actual)[M],
                             type const& matcher) -> typename OutputPolicy::return_type {
        std::ostringstream sactual, smatcher;
        sactual << actual;
        smatcher << matcher;
        return matcher.print(smatcher.str(), sactual.str(), matcher.matches(actual));
    }

    friend std::ostream& operator<<(std::ostream& o, type const& matcher) {
        matcher.describe(o);
        return o; 
    }
private:
    void describe(std::ostream& o) const {
        MatcherPolicy::describe(o, expected_);
    }
    std::string expected_;
};

struct Is_ {
protected:
    template<class Policy, class ExpType, class ActualType>
    bool matches(Matcher<Policy,ExpType> const& expected, ActualType const& actual) const {
        return expected.matches(actual);
    }

    template<class Policy, class ExpType>
    void describe(std::ostream& o, Matcher<Policy,ExpType> const& expected) const {
        o << "is " << expected;  
    }
};

template<class T>
using Is = Matcher<Is_,T>;

template<class Policy, class ExpType>
Is<Matcher<Policy,ExpType>> is(Matcher<Policy,ExpType> const& value) {
    return Is<Matcher<Policy,ExpType>>(value);
}

struct IsNot_ {
protected:
    template<class Policy, class ExpType, class ActualType>
    bool matches(Matcher<Policy,ExpType> const& expected, ActualType const& actual) const {
        return !expected.matches(actual);
    }

    template<class Policy, class ExpType>
    void describe(std::ostream& o, Matcher<Policy,ExpType> const& expected) const {
        o << "not " << expected;  
    }
};

template<class T>
using IsNot = Matcher<IsNot_,T>;


template<class Policy, class T>
IsNot<Matcher<Policy,T>> operator!(Matcher<Policy,T> const& value) {
    return IsNot<Matcher<Policy,T>>(value);
}

struct IsNull_ {
protected:
    template<typename T>
    bool matches(std::nullptr_t expected, T const* actual) const {
        return actual == expected;
    }

    template<typename T>
    void describe(std::ostream& o, T const& expected) const {
        o << "null pointer";  
    }
};

using IsNull = Matcher<IsNull_,std::nullptr_t>;

IsNull null() {
    return IsNull(nullptr);
}

struct IsEqual_ {
protected:
    template<typename T>
    bool matches(T const& expected, T const& actual,
                 typename std::enable_if<
                    is_equality_comparable<T>::value
                    >::type* = 0) const
    {
        return expected == actual;
    }

    template<typename T>
    bool matches(T const& expected, T const& actual,
                 typename std::enable_if<
                    !is_equality_comparable<T>::value
                    && std::is_pod<T>::value
                    >::type* = 0) const
    {
        return !std::memcmp(&expected, &actual, sizeof expected);
    }

    template<typename T>
    void describe(std::ostream& o, T const& expected) const {
       o << expected;  
    }
};

template<>
void IsEqual_::describe(std::ostream& o, std::string const& expected) const {
   o << "\"" << expected << "\"";  
}

template<class T>
using IsEqual = Matcher<IsEqual_,T>;

template<typename T>
IsEqual<T> equalTo(T const& value) {
    return IsEqual<T>(value);
}

template<typename T, size_t N>
IsEqual<T[N]> equalTo(T const (&value)[N]) {
    return IsEqual<T[N]>(value);
}

struct IsContaining_ {
protected:
    template<typename C, typename T,
         typename std::enable_if<std::is_same<typename C::value_type,T>::value>::type* = nullptr>
    bool matches(T const& item, C const& cont) const {
        return std::end(cont) != std::find(std::begin(cont), std::end(cont), item);
    }

    template<typename C = std::string, typename T = std::string>
    bool matches(std::string const& substr, std::string const& actual) const {
        return std::string::npos != actual.find(substr);
    }

    // overload for checking whether container values match a predicate specified by a Matcher
    template<typename C, typename T, typename Policy,
         typename std::enable_if<std::is_same<typename C::value_type,T>::value>::type* = nullptr>
    bool matches(Matcher<Policy,T> const& itemMatcher, C const& cont) const {
        using namespace std::placeholders;
        auto pred = std::bind(&Matcher<Policy,T>::template matches<T>, &itemMatcher, _1);
        return std::all_of(std::begin(cont), std::end(cont), pred);
    }

    template<typename T>
    void describe(std::ostream& o, T const& expected) const {
       o << "contains " << expected;  
    }
};

template<>
void IsContaining_::describe(std::ostream& o, std::string const& expected) const {
   o << "contains " << "\"" << expected << "\"";  
}

template<class T>
using IsContaining = Matcher<IsContaining_,T>;

template<typename T>
IsContaining<T> contains(T const& value) {
    return IsContaining<T>(value);
}

template<typename T, size_t N>
IsContaining<T[N]> contains(T const (&value)[N]) {
    return IsContaining<T[N]>(value);
}

template<class Key, class T>
IsContaining<std::pair<const Key,T>> contains(Key const& key, T const& value) {
    return IsContaining<std::pair<const Key,T>>(std::pair<const Key,T>(key, value));
}

template<typename T, typename Policy>
IsContaining<Matcher<Policy,T>> everyItem(Matcher<Policy,T> const& itemMatcher) {
    return IsContaining<Matcher<Policy,T>>(itemMatcher);
}

struct IsContainingKey {
protected:
    template<typename C, typename T,
         typename std::enable_if<std::is_same<typename C::key_type,T>::value>::type* = nullptr>
    bool matches(T const& key, C const& cont) const {
        for (auto const& val : cont) {
            if (val.first == key)
                return true;
        }
        return false;
    }

    template<typename T>
    void describe(std::ostream& o, T const& expected) const {
       o << "has key " << expected;  
    }
};

template<typename T>
Matcher<IsContainingKey,T> hasKey(T const& key) {
    return Matcher<IsContainingKey,T>(key);
}

struct IsIn_ {
protected:
    template<typename C, typename T,
         typename std::enable_if<std::is_same<typename C::value_type,T>::value>::type* = nullptr>
    bool matches(C const& cont, T const& item) const {
        return std::end(cont) != std::find(std::begin(cont), std::end(cont), item);
    }

    template<typename C>
    void describe(std::ostream& o, C const& expected) const {
       o << "one of " << expected;  
    }
    
};

template<typename C>
using IsIn = Matcher<IsIn_,C>;

template<typename C>
IsIn<C> in(C const& cont) {
    return IsIn<C>(cont);
}

struct IsEqualIgnoringCase_ {
protected:
    bool matches(std::string const& expected, std::string const& actual) const {
        ci_string ci_exp, ci_act;

        ci_exp.assign(expected.begin(), expected.end());
        ci_act.assign(actual.begin(), actual.end());
        return ci_exp == ci_act;
    }

    void describe(std::ostream& o, std::string const& expected) const {
       o << "Equal to " << "\"" << expected << "\"" << " ignoring case";
    }
};

using IsEqualIgnoringCase = Matcher<IsEqualIgnoringCase_,std::string>;

IsEqualIgnoringCase equalToIgnoringCase(std::string const& val) {
    return IsEqualIgnoringCase(val);
}

struct IsEqualIgnoringWhiteSpace {
protected:
    bool matches(std::string const& expected, std::string const& actual) const {
        std::string exp(expected), act(actual);

        exp.erase(std::remove_if(exp.begin(),
                                 exp.end(),
                                 [](char x){return std::isspace(x);}),
                  exp.end());
        act.erase(std::remove_if(act.begin(),
                                 act.end(),
                                 [](char x){return std::isspace(x);}),
                  act.end());
        return exp == act;
    }

    void describe(std::ostream& o, std::string const& expected) const {
       o << "Equal to " << "\"" << expected << "\"" << " ignoring white space";  
    }
};

Matcher<IsEqualIgnoringWhiteSpace,std::string> equalToIgnoringWhiteSpace(std::string const& val) {
    return Matcher<IsEqualIgnoringWhiteSpace,std::string>(val);
}
    
struct StringStartsWith_ {
protected:
    bool matches(std::string const& substr, std::string const& actual) const {
        return !actual.compare(0, substr.size(), substr);
    }
    
    void describe(std::ostream& o, std::string const& expected) const {
       o << "starts with " << "\"" << expected << "\"";  
    }
};

using StringStartsWith = Matcher<StringStartsWith_,std::string>;

StringStartsWith startsWith(std::string const& val) {
    return StringStartsWith(val);
}

struct StringEndsWith_ {
protected:
    bool matches(std::string const& substr, std::string const& actual) const {
        return !actual.compare(actual.size() - substr.size(), substr.size(), substr);
    }
    
    void describe(std::ostream& o, std::string const& expected) const {
       o << "ends with " << "\"" << expected << "\"";  
    }
};

using StringEndsWith = Matcher<StringEndsWith_,std::string>;

StringEndsWith endsWith(std::string const& val) {
    return StringEndsWith(val);
}

struct AnyOf_ {
protected:
    template<class PolicyA, class PolicyB, class TA, class TB, class ActualType>
    bool matches(std::tuple<Matcher<PolicyA,TA>,Matcher<PolicyB,TB>> const& matchers, ActualType const& actual) const {
        return std::get<0>(matchers).matches(actual) || std::get<1>(matchers).matches(actual);
    }

    template<class PolicyA, class PolicyB, class TA, class TB>
    void describe(std::ostream& o, std::tuple<Matcher<PolicyA,TA>,Matcher<PolicyB,TB>> const& matchers) const {
        o << "any of " << std::get<0>(matchers) << " or " <<  std::get<1>(matchers);  
    }
};

template<class PolicyA, class PolicyB, class TA, class TB>
using AnyOf = Matcher<AnyOf_,std::tuple<Matcher<PolicyA,TA>,Matcher<PolicyB,TB>>>;

template<class PolicyA, class PolicyB, class TA, class TB>
AnyOf<PolicyA,PolicyB,TA,TB> anyOf(Matcher<PolicyA,TA> const& ma, 
                                   Matcher<PolicyB,TB> const& mb) 
{
    return AnyOf<PolicyA,PolicyB,TA,TB>(std::make_tuple(ma, mb));
}

struct AllOf_ {
protected:
    template<class PolicyA, class PolicyB, class TA, class TB, class ActualType>
    bool matches(std::tuple<Matcher<PolicyA,TA>,Matcher<PolicyB,TB>> const& matchers, ActualType const& actual) const {
        return std::get<0>(matchers).matches(actual) && std::get<1>(matchers).matches(actual);
    }

    template<class PolicyA, class PolicyB, class TA, class TB>
    void describe(std::ostream& o, std::tuple<Matcher<PolicyA,TA>,Matcher<PolicyB,TB>> const& matchers) const {
        o << "all of " << std::get<0>(matchers) << " and " <<  std::get<1>(matchers);  
    }
};

template<class PolicyA, class PolicyB, class TA, class TB>
using AllOf = Matcher<AllOf_,std::tuple<Matcher<PolicyA,TA>,Matcher<PolicyB,TB>>>;

template<class PolicyA, class PolicyB, class TA, class TB>
AllOf<PolicyA,PolicyB,TA,TB> allOf(Matcher<PolicyA,TA> const& ma, 
                                   Matcher<PolicyB,TB> const& mb) 
{
    return AllOf<PolicyA,PolicyB,TA,TB>(std::make_tuple(ma, mb));
}

struct IsCloseTo {
    template<typename T, 
             typename = typename 
               std::enable_if<std::is_floating_point<T>::value>::type>
    bool matches (std::pair<T,T> const& expected, T const& actual) const {
        T value = expected.first;
        T delta = expected.second;

        return std::fabs(actual - value) <= delta;
    }

    template<typename T>
    void describe(std::ostream& o, std::pair<T,T> const& expected) const {
       o << "a numeric value within +/-" << expected.second 
         << " of " << expected.first;  
    }
};

template<typename T>
Matcher<IsCloseTo,std::pair<T,T>> closeTo(T const& operand, T const& error) {
    return Matcher<IsCloseTo,std::pair<T,T>>(std::make_pair(operand, error));
}


struct MatchesPattern_ {
    bool matches(std::string const& reg, std::string const& actual) const {
        return std::regex_match(actual, std::regex(reg));
    }

    void describe(std::ostream& o, std::string const& expected) const {
       o << "a string matching the pattern " << expected;  
    }
};

using MatchesPattern = Matcher<MatchesPattern_,std::string>;

MatchesPattern matchesPattern(std::string const& reg_exp) {
    return MatchesPattern(reg_exp);
}

MatchesPattern matches(std::string const& reg_exp) {
    return MatchesPattern(reg_exp);
}


} // namespace matcha

#endif // _MATCHA_H_
