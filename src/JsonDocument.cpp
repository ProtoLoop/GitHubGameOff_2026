#include "JsonDocument.h"

#include "InlineString.h"
#include "Shortcuts.h"

#include <cerrno>
#include <charconv>
#include <cstdlib>
#include <string_view>
#include <system_error>

namespace
{

Result<>
ParseFloat(const std::string_view text, float& out) noexcept
{
    GG_CHECKB(!text.empty(), "Input text is empty");

    // Null terminate the string
    const InlineString<32> str{ text };

    const char* cstr = str.c_str();
    char* end{};
    errno = 0;
    const float value = std::strtof(cstr, &end);

    GG_CHECKB(end > cstr || *end != '\0' || errno == ERANGE, "Failed to parse number");

    out = value;
    return Result<>::Ok;
}

size_t
TokLen(const jsmntok_t& token)
{
    return static_cast<size_t>(token.end - token.start);
}

std::string_view
TokStr(const jsmntok_t& token, const std::string_view json)
{
    return json.substr(static_cast<size_t>(token.start), TokLen(token));
}

size_t
GetLine(
    const size_t tokenIndex, const std::span<const jsmntok_t> tokens, const std::string_view json)
{
    const size_t offset = scast<size_t>(tokens[tokenIndex].start);

    size_t line = 1;
    for(size_t i = 0; i < offset && i < json.size(); ++i)
    {
        if(json[i] == '\n')
        {
            ++line;
        }
    }
    return line;
}
} // namespace

/// JsonValue

JsonValue::JsonValue(
    const size_t index, const std::span<const jsmntok_t> tokens, const std::string_view json)
    : m_Index(index),
      m_Tokens(tokens),
      m_Json(json)
{
    GG_ASSERT(m_Index < m_Tokens.size(), "JsonValue out of bounds");
    GG_ASSERT(m_Tokens[m_Index].type == JSMN_PRIMITIVE
            || m_Tokens[m_Index].type == JSMN_STRING
            || m_Tokens[m_Index].type == JSMN_OBJECT
            || m_Tokens[m_Index].type == JSMN_ARRAY,
        "Expected a valid JSON value token");
}

Result<>
JsonValue::TryGetUint32(uint32_t& value) const
{
    GG_CHECK(m_Index < m_Tokens.size(), "JsonValue out of bounds");

    const jsmntok_t& tok = m_Tokens[m_Index];
    GG_CHECK(tok.type == JSMN_PRIMITIVE, "Expected token to be a primitive");

    const std::string_view sv = TokStr(tok, m_Json);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

    GG_CHECK(ec == std::errc{},
        "Failed to parse number (line:{})",
        GetLine());
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    GG_CHECK(ptr == sv.data() + sv.size(),
        "Unexpected trailing characters: (line:{})",
        GetLine());

    return Result<>::Ok;
}

Result<>
JsonValue::TryGetFloat(float& value) const
{
    GG_CHECK(m_Index < m_Tokens.size(), "JsonValue out of bounds");

    const jsmntok_t& tok = m_Tokens[m_Index];
    GG_CHECK(tok.type == JSMN_PRIMITIVE, "Expected token to be a primitive");

    const std::string_view sv = TokStr(tok, m_Json);

    GG_CHECKB(ParseFloat(sv, value),
        "Failed to parse number (line:{})",
        GetLine());

    // std::from_chars() not available in xcode
    //  NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    /*const auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), value);

    GG_CHECK(ec == std::errc{},
        "Failed to parse number (line:{})",
        GetLine(m_Index, m_Tokens, m_Json));
    // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    GG_CHECK(ptr == sv.data() + sv.size(),
        "Unexpected trailing characters: (line:{})",
        GetLine(m_Index, m_Tokens, m_Json));*/

    return Result<>::Ok;
}

Result<>
JsonValue::TryGetBool(bool& value) const
{
    GG_CHECK(m_Index < m_Tokens.size(), "JsonValue out of bounds");

    const jsmntok_t& tok = m_Tokens[m_Index];
    GG_CHECK(tok.type == JSMN_PRIMITIVE,
        "Expected token to be a primitive (line:{})",
        GetLine());

    const std::string_view sv = TokStr(tok, m_Json);

    if(sv == "true")
    {
        value = true;

        return Result<>::Ok;
    }

    GG_CHECKB(sv == "false", "Invalid boolean value (line:{})", GetLine());

    value = false;

    return Result<>::Ok;
}

Result<>
JsonValue::TryGetString(std::string_view& value) const
{
    GG_CHECK(m_Index < m_Tokens.size(), "JsonValue out of bounds");

    const jsmntok_t& tok = m_Tokens[m_Index];
    GG_CHECK(tok.type == JSMN_STRING,
        "Expected token to be a string (line:{})",
        GetLine());

    value = TokStr(tok, m_Json);

    return Result<>::Ok;
}

Result<>
JsonValue::TryGetObject(JsonObject& value) const
{
    GG_CHECK(m_Index < m_Tokens.size(), "JsonValue out of bounds");

    const jsmntok_t& tok = m_Tokens[m_Index];
    GG_CHECK(tok.type == JSMN_OBJECT,
        "Expected token to be an object (line:{})",
        GetLine());

    const size_t size = static_cast<size_t>(tok.size);
    GG_CHECKB(m_Index + size <= m_Tokens.size(),
        "Object tokens out of bounds: (line:{})",
        GetLine());

    value = JsonObject(m_Index, m_Tokens, m_Json);
    return Result<>::Ok;
}

Result<>
JsonValue::TryGetArray(JsonArray& value) const
{
    GG_CHECK(m_Index < m_Tokens.size(), "JsonValue out of bounds");

    const jsmntok_t& tok = m_Tokens[m_Index];
    GG_CHECK(tok.type == JSMN_ARRAY,
        "Expected value to be an array (line:{})",
        GetLine());

    // Get the child tokens of the array, skipping the array token itself.
    const size_t size = static_cast<size_t>(tok.size);

    GG_CHECKB(m_Index + size <= m_Tokens.size(),
        "Array tokens out of bounds: (line:{})",
        GetLine());

    value = JsonArray(m_Index, m_Tokens, m_Json);
    return Result<>::Ok;
}

size_t
JsonValue::GetLine() const
{
    return ::GetLine(m_Index, m_Tokens, m_Json);
}

/// JsonProperty

Result<>
JsonProperty::TryGetKey(std::string_view& key) const
{
    GG_CHECKB(m_Index < m_Tokens.size(), "JsonProperty out of bounds");

    key = TokStr(m_Tokens[m_Index], m_Json);

    return Result<>::Ok;
}

Result<>
JsonProperty::TryGetValue(JsonValue& value) const
{
    GG_CHECKB(m_Index + 1 < m_Tokens.size(), "JsonProperty out of bounds");

    value = JsonValue(m_Index + 1, m_Tokens, m_Json);
    return Result<>::Ok;
}

Result<>
JsonProperty::TryGetUint32(uint32_t& value) const
{
    JsonValue jsonValue;
    GG_CHECKB(TryGetValue(jsonValue), "Failed to get value for uint32");
    GG_CHECKB(jsonValue.TryGetUint32(value), "Failed to get uint32");

    return Result<>::Ok;
}

Result<>
JsonProperty::TryGetFloat(float& value) const
{
    JsonValue jsonValue;
    GG_CHECKB(TryGetValue(jsonValue), "Failed to get value for float");
    GG_CHECKB(jsonValue.TryGetFloat(value), "Failed to get float");

    return Result<>::Ok;
}

Result<>
JsonProperty::TryGetBool(bool& value) const
{
    JsonValue jsonValue;
    GG_CHECKB(TryGetValue(jsonValue), "Failed to get value for bool");
    GG_CHECKB(jsonValue.TryGetBool(value), "Failed to get bool");

    return Result<>::Ok;
}

Result<>
JsonProperty::TryGetString(std::string_view& value) const
{
    JsonValue jsonValue;
    GG_CHECKB(TryGetValue(jsonValue), "Failed to get value for string");
    GG_CHECKB(jsonValue.TryGetString(value), "Failed to get string");

    return Result<>::Ok;
}

Result<>
JsonProperty::TryGetObject(JsonObject& value) const
{
    JsonValue jsonValue;
    GG_CHECKB(TryGetValue(jsonValue), "Failed to get value for object");
    GG_CHECKB(jsonValue.TryGetObject(value), "Failed to get object");

    return Result<>::Ok;
}

Result<>
JsonProperty::TryGetArray(JsonArray& value) const
{
    JsonValue jsonValue;
    GG_CHECKB(TryGetValue(jsonValue), "Failed to get value for array");
    GG_CHECKB(jsonValue.TryGetArray(value), "Failed to get array");

    return Result<>::Ok;
}

JsonProperty::JsonProperty(
    const size_t index, const std::span<const jsmntok_t> tokens, const std::string_view json)
    : m_Index(index),
      m_Tokens(tokens),
      m_Json(json)
{
    GG_ASSERT(m_Index + 1 < m_Tokens.size(), "JsonProperty out of bounds");
    GG_ASSERT(m_Tokens[m_Index].type == JSMN_STRING, "Expected property key to be as string");
}

size_t
JsonProperty::GetLine() const
{
    return ::GetLine(m_Index, m_Tokens, m_Json);
}

/// JsonArray

template<>
JsonIterator<JsonValue>&
JsonIterator<JsonValue>::operator++()
{
    if(m_Remaining > 0)
    {
        const jsmntok_t& curValue = m_Tokens[m_Index];

        ++m_Index;

        while(m_Index < m_Tokens.size() && m_Tokens[m_Index].start < curValue.end)
        {
            ++m_Index;
        }

        --m_Remaining;

        GG_ASSERT(m_Index < m_Tokens.size() || m_Remaining == 0,
            "JsonArray::iterator out of bounds");
    }

    return *this;
}

template<>
JsonValue
JsonIterator<JsonValue>::operator*() const
{
    if(!GG_VERIFYB(m_Index < m_Tokens.size(), "JsonValue out of bounds"))
    {
        return JsonValue();
    }

    return JsonValue(m_Index, m_Tokens, m_Json);
}

JsonIterator<JsonValue>
JsonArray::begin() const
{
    if(m_Index >= m_Tokens.size() || m_Tokens[m_Index].type != JSMN_ARRAY)
    {
        return iterator(m_Index, 0, 0, m_Tokens, m_Json);
    }

    const size_t childTokenCount = static_cast<size_t>(m_Tokens[m_Index].size);
    return iterator(m_Index, m_Index + 1, childTokenCount, m_Tokens, m_Json);
}

JsonIterator<JsonValue>
// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
JsonArray::end() const
{
    return iterator(m_Index, 0, 0, m_Tokens, m_Json);
}

size_t
JsonArray::size() const
{
    size_t count = 0;
    for(auto it = begin(); it != end(); ++it)
    {
        ++count;
    }

    return count;
}

JsonArray::JsonArray(
    const size_t index, const std::span<const jsmntok_t> tokens, const std::string_view json)
    : m_Index(index),
      m_Tokens(tokens),
      m_Json(json)
{
    GG_ASSERT(m_Index < m_Tokens.size(), "JsonArray out of bounds");
    GG_ASSERT(m_Tokens[m_Index].type == JSMN_ARRAY, "Expected an array");
}

size_t
JsonArray::GetLine() const
{
    return ::GetLine(m_Index, m_Tokens, m_Json);
}

/// JsonObject

template<>
JsonIterator<JsonProperty>&
JsonIterator<JsonProperty>::operator++()
{
    if(m_Remaining > 0)
    {
        // m_Tokens[m_Index] is the key token
        // m_Tokens[m_Index + 1] is the value token

        if(!GG_VERIFYB(m_Index + 1 < m_Tokens.size(), "JsonObject::iterator out of bounds"))
        {
            m_Remaining = 0;
            return *this;
        }

        // Skip over the value to the next key.
        const jsmntok_t& curValue = m_Tokens[m_Index + 1];

        m_Index += 2;

        while(m_Index < m_Tokens.size() && m_Tokens[m_Index].start < curValue.end)
        {
            ++m_Index;
        }

        if(m_Index >= m_Tokens.size())
        {
            m_Remaining = 0;
        }
        else
        {
            --m_Remaining;
        }
    }

    return *this;
}

template<>
JsonProperty
JsonIterator<JsonProperty>::operator*() const
{
    // The current token (they key) and the next token (the value) must exist.
    if(!GG_VERIFYB(m_Index + 1 < m_Tokens.size(), "JsonProperty value out of bounds"))
    {
        return JsonProperty();
    }
    return JsonProperty(m_Index, m_Tokens, m_Json);
}

JsonObject::JsonObject(
    const size_t index, const std::span<const jsmntok_t> tokens, const std::string_view json)
    : m_Index(index),
      m_Tokens(tokens),
      m_Json(json)
{
    GG_ASSERT(m_Index < m_Tokens.size(), "Object out of bounds");
    GG_ASSERT(m_Tokens[m_Index].type == JSMN_OBJECT, "Expected an object");
}

Result<>
JsonObject::TryGetValue(const std::string_view key, JsonValue& value) const
{
    bool found = false;
    for(const JsonProperty& prop : *this)
    {
        if(std::string_view propKey;
            !GG_VERIFYB(prop.TryGetKey(propKey), "Failed to get property key"))
        {
            return Result<>::Error;
        }
        else if(propKey == key)
        {
            GG_CHECKB(!found, "Duplicate property: {}", key);

            GG_CHECKB(prop.TryGetValue(value), "Failed to get value for property: {}", key);
            found = true;
        }
    }

    GG_CHECK(found, "Property not found: {}", key);

    return Result<>::Ok;
}

Result<>
JsonObject::TryGetUint32(const std::string_view key, uint32_t& value) const
{
    JsonValue jsonValue;
    if(TryGetValue(key, jsonValue) && jsonValue.TryGetUint32(value))
    {
        return Result<>::Ok;
    }
    else
    {
        return Result<>::Error;
    }
}

Result<>
JsonObject::TryGetFloat(const std::string_view key, float& value) const
{
    JsonValue jsonValue;
    if(TryGetValue(key, jsonValue) && jsonValue.TryGetFloat(value))
    {
        return Result<>::Ok;
    }
    else
    {
        return Result<>::Error;
    }
}

Result<>
JsonObject::TryGetBool(const std::string_view key, bool& value) const
{
    JsonValue jsonValue;
    if(TryGetValue(key, jsonValue) && jsonValue.TryGetBool(value))
    {
        return Result<>::Ok;
    }
    else
    {
        return Result<>::Error;
    }
}

Result<>
JsonObject::TryGetString(const std::string_view key, std::string_view& value) const
{
    JsonValue jsonValue;
    if(TryGetValue(key, jsonValue) && jsonValue.TryGetString(value))
    {
        return Result<>::Ok;
    }
    else
    {
        return Result<>::Error;
    }
}

Result<>
JsonObject::TryGetObject(const std::string_view key, JsonObject& value) const
{
    JsonValue jsonValue;
    if(TryGetValue(key, jsonValue) && jsonValue.TryGetObject(value))
    {
        return Result<>::Ok;
    }
    else
    {
        return Result<>::Error;
    }
}

Result<>
JsonObject::TryGetArray(const std::string_view key, JsonArray& value) const
{
    JsonValue jsonValue;
    if(TryGetValue(key, jsonValue) && jsonValue.TryGetArray(value))
    {
        return Result<>::Ok;
    }
    else
    {
        return Result<>::Error;
    }
}

JsonObject::iterator
JsonObject::begin() const
{
    if(m_Index + 1 >= m_Tokens.size() || m_Tokens[m_Index].type != JSMN_OBJECT)
    {
        return iterator(m_Index, 0, 0, m_Tokens, m_Json);
    }

    const size_t childTokenCount = static_cast<size_t>(m_Tokens[m_Index].size);
    return iterator(m_Index, m_Index + 1, childTokenCount, m_Tokens, m_Json);
}

JsonObject::iterator
JsonObject::end() const
{
    return iterator(m_Index, 0, 0, m_Tokens, m_Json);
}

size_t
JsonObject::GetLine() const
{
    return ::GetLine(m_Index, m_Tokens, m_Json);
}

/// JsonDocument

Result<JsonDocument>
JsonDocument::Create(std::vector<std::byte>&& data)
{
    jsmn_parser parser;

    const std::string_view json = std::string_view(recast<const char*>(data.data()), data.size());

    jsmn_init(&parser);
    const int tokenCountA = jsmn_parse(&parser, json.data(), json.size(), nullptr, 0);

    GG_CHECK(tokenCountA > 0,
        "Failed to parse JSON: {}",
        tokenCountA == JSMN_ERROR_INVAL ? "Invalid JSON" : "Unknown error");

    std::vector<jsmntok_t> tokens(static_cast<size_t>(tokenCountA));
    jsmn_init(&parser);
    const int tokenCountB = jsmn_parse(&parser,
        json.data(),
        json.size(),
        tokens.data(),
        static_cast<unsigned>(tokenCountA));

    GG_CHECK(tokenCountB >= 0,
        "Failed to parse JSON: {}",
        tokenCountB == JSMN_ERROR_INVAL ? "Invalid JSON" : "Unknown error");

    GG_CHECK(static_cast<size_t>(tokenCountB) == tokens.size(),
        "Token count mismatch: expected {}, got {}",
        tokens.size(),
        tokenCountB);

    GG_CHECK(tokens.front().type == JSMN_OBJECT, "Expected top-level JSON element to be an object");

    return JsonDocument(std::move(tokens), std::move(data));
}

JsonObject
JsonDocument::GetRootObject() const
{
    return JsonObject(0, m_TokenVector, std::string_view(m_Json.data(), m_Json.size()));
}

JsonDocument::JsonDocument(std::vector<jsmntok_t>&& tokens, std::vector<std::byte>&& data)
    : m_TokenVector(std::move(tokens)),
      m_Bytes(std::move(data)),
      m_Json(std::string_view(recast<const char*>(m_Bytes.data()), m_Bytes.size()))
{
    GG_ASSERT(m_TokenVector.front().type == JSMN_OBJECT,
        "Expected top-level JSON element to be an object");
}