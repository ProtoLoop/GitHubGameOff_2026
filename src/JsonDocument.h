#pragma once

#include "Result.h"

#include <span>
#include <string_view>
#include <vector>

#define JSMN_HEADER
#include "jsmn.h"

/// Model:
/// JsonDocument: The entire dcoument
/// JsonObject: A collection of JsonProperties
/// JsonProperty: A string key and a JsonValue
/// JsonValue: String, number, boolean, JsonArray, or JsonObject
/// JsonArray: An array of JsonValues

class JsonDocument;
class JsonObject;
class JsonArray;

/// An iterator over JsonArray or JsonObject elements
template<typename T>
class JsonIterator
{
public:
    JsonIterator& operator++();

    T operator*() const;

    friend bool operator==(const JsonIterator& lhs, const JsonIterator& rhs)
    {
        return lhs.m_ParentTokenIndex == rhs.m_ParentTokenIndex
            && lhs.m_Tokens.data() == rhs.m_Tokens.data()
            && lhs.m_Remaining == rhs.m_Remaining;
    }

    friend bool operator!=(const JsonIterator& lhs, const JsonIterator& rhs)
    {
        return !(lhs == rhs);
    }

private:
    friend class JsonObject;
    friend class JsonArray;

    JsonIterator(const size_t parentTokenIndex,
        const size_t index,
        const size_t remaining,
        const std::span<const jsmntok_t> tokens,
        const std::string_view json)
        : m_ParentTokenIndex(parentTokenIndex),
          m_Index(index),
          m_Remaining(remaining),
          m_Tokens(tokens),
          m_Json(json)
    {
        GG_ASSERT(!m_Tokens.empty() || m_Remaining == 0, "JsonIterator tokens cannot be empty");
        GG_ASSERT(m_Index < m_Tokens.size() || m_Remaining == 0, "JsonIterator out of bounds");
    }

    size_t m_ParentTokenIndex;
    size_t m_Index;
    size_t m_Remaining; // Remaining elements in the iteration
    std::span<const jsmntok_t> m_Tokens;
    std::string_view m_Json;
};

/// A JSON value, which can be a string, number, boolean, object, or array.
class JsonValue
{
public:
    JsonValue() = default;

    Result<> TryGetUint32(uint32_t& value) const;

    Result<> TryGetBool(bool& value) const;

    /// Convert the JSON value to a string.
    /// Does not handle escape sequences.
    Result<> TryGetString(std::string_view& value) const;

    Result<> TryGetObject(JsonObject& value) const;

    Result<> TryGetArray(JsonArray& value) const;

private:
    friend class JsonProperty;
    friend class JsonIterator<JsonValue>;

    JsonValue(
        const size_t index, const std::span<const jsmntok_t> tokens, const std::string_view json);

    size_t m_Index{ 0 };
    std::span<const jsmntok_t> m_Tokens;
    std::string_view m_Json;
};

/// A JSON property, consisting of a string key and a JSON value.
class JsonProperty
{
public:
    JsonProperty() = default;

    /// Get the key of the JSON property.
    /// Does not handle escape sequences.
    Result<> TryGetKey(std::string_view& key) const;

    Result<> TryGetValue(JsonValue& value) const;

private:
    friend class JsonIterator<JsonProperty>;

    JsonProperty(
        const size_t index, const std::span<const jsmntok_t> tokens, const std::string_view json);

    size_t m_Index{ 0 };
    std::span<const jsmntok_t> m_Tokens;
    std::string_view m_Json;
};

/// A JSON array, which contains a sequence of JSON values.
class JsonArray
{
public:
    using iterator = JsonIterator<JsonValue>;

    JsonArray() = default;

    iterator begin() const;

    iterator end() const;

    /// Get the number of elements in the JSON array.
    /// WARNING: This is O(n) where n is the number of elements in the array.
    size_t size() const;

private:
    friend class JsonValue;

    JsonArray(
        const size_t index, const std::span<const jsmntok_t> tokens, const std::string_view json);

    size_t m_Index{ 0 };
    std::span<const jsmntok_t> m_Tokens;
    std::string_view m_Json;
};

/// A JSON object, which contains a set of JSON properties (key-value pairs).
class JsonObject
{
public:
    using iterator = JsonIterator<JsonProperty>;

    JsonObject() = default;

    iterator begin() const;

    iterator end() const;

private:
    friend class JsonDocument;
    friend class JsonValue;

    JsonObject(const size_t index, std::span<const jsmntok_t> tokens, const std::string_view json);

    size_t m_Index{ 0 };
    std::span<const jsmntok_t> m_Tokens;
    std::string_view m_Json;
};

/// Owns the JSON bytes and parsed tokens.
/// Objects, arrays, properties, values, iterators, and string views borrow this
/// storage. They remain valid only while the storage remains alive.
/// Moving the document transfers ownership without invalidating existing views.
/// Move assignment invalidates views into the destination's previous storage.
/// A moved-from document may only be destroyed or assigned a new value.
class JsonDocument
{
public:
    JsonDocument() = delete;
    ~JsonDocument() = default;
    JsonDocument(const JsonDocument&) = delete;
    JsonDocument& operator=(const JsonDocument&) = delete;
    JsonDocument(JsonDocument&&) = default;
    JsonDocument& operator=(JsonDocument&&) = default;

    static Result<JsonDocument> Create(std::vector<std::byte> data);

    JsonObject GetRootObject() const;

private:
    JsonDocument(std::vector<jsmntok_t>&& tokens, std::vector<std::byte>&& data);

    std::vector<jsmntok_t> m_TokenVector;
    std::vector<std::byte> m_Bytes;
    std::string_view m_Json;
};