#pragma once
#ifndef LOCALIZE_CONTROLLER_H
#define LOCALIZE_CONTROLLER_H

/**
 * @file Localizer.h
 * @brief Header-only localization library for C++ applications.
 * @version 1.0.0
 * @author 0x1mer
 * @license MIT
 *
 * @details
 * Provides a thread-safe, JSON-driven localization system for C++.
 * Supports hierarchical namespaces, runtime JSON reload, debug output, and parameter substitution.
 *
 * ### Example
 * @code
 * LocalizeController::loadFromDirectory("langs");
 * LocalizeController::setLocale("en");
 * std::cout << L("ui.button.play");
 * @endcode
 */

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

// Version macros
#define LOC_VERSION_MAJOR 1
#define LOC_VERSION_MINOR 1
#define LOC_VERSION_PATCH 0

#if __cplusplus < 201703L
#error "LocalizeController requires C++17 or higher"
#endif

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Standard headers
#include <string>        ///< std::string
#include <unordered_map> ///< std::unordered_map
#include <fstream>       ///< std::ifstream
#include <iostream>      ///< std::cout, std::cerr
#include <filesystem>    ///< std::filesystem
#include <vector>        ///< std::vector
#include <algorithm>     ///< std::find
#include "json.hpp"      ///< nlohmann::json dependency

// Optional regex support
#ifndef LOC_USE_REGEX
#define LOC_USE_REGEX 0
#endif

#if LOC_USE_REGEX
#include <regex> ///< std::regex, std::regex_replace
#pragma message("LocalizeController: using <regex> version of placeholder parser")
#else
#pragma message("LocalizeController: using fast non-regex placeholder parser")
#endif

// Thread-safety configuration
#ifndef LOC_THREAD_SAFE
#define LOC_THREAD_SAFE 1
#endif

/*
Cerror configuration (on/off) 0 - off (need to set callback function), 1 - on
Example:

void errorCallback(const std::string& errorMsg) {
    std::cout << "[ERR] " + errorMsg + "\n";
}
LocalizeController::setErrorCallback(errorCallback);

!!! Note that you need to do this, before call function loadFromDirectory();
*/
#ifndef LOC_CERR
#define LOC_CERR 0
#endif

#if LOG_CERR == 0
using ErrorCallback = std::function<void(const std::string &, int)>;
#endif

#if LOC_THREAD_SAFE
#include <mutex>        ///< std::mutex
#include <shared_mutex> ///< std::shared_mutex
#define LOC_READ_LOCK std::shared_lock<std::shared_mutex> lock(getMutex());
#define LOC_WRITE_LOCK std::unique_lock<std::shared_mutex> lock(getMutex());
#else
#define LOC_READ_LOCK
#define LOC_WRITE_LOCK
#endif

// Default configuration macros
#ifndef LOC_DEFAULT_LOCALE
#define LOC_DEFAULT_LOCALE "en"
#endif

#ifndef LOC_NAMESPACE_SEPARATOR
#define LOC_NAMESPACE_SEPARATOR "."
#endif

#ifndef LOC_COLOR_DEFAULT
#define LOC_COLOR_DEFAULT "\x1b[32m"
#endif

#ifndef LOC_COLOR_RESET
#define LOC_COLOR_RESET "\x1b[0m"
#endif

// ============================================================================
// DebugOptions
// ============================================================================

/**
 * @struct DebugOptions
 * @brief Configuration for debug output of localization keys.
 */
struct DebugOptions
{
    bool enabled = false;                     ///< Enable debug printing.
    bool coloredOutput = true;                ///< Enable ANSI color output.
    std::string keyColor = LOC_COLOR_DEFAULT; ///< ANSI color for key highlight.
    std::string resetColor = LOC_COLOR_RESET; ///< ANSI color reset sequence.
    std::string prefix = "";                  ///< Prefix added before each debug line.

    /**
     * @brief Constructs debug options.
     * @param enabled Whether debug mode is active.
     * @param coloredOutput Whether to use colored output.
     * @param keyColor ANSI color for key display.
     * @param resetColor ANSI reset sequence.
     * @param prefix Optional prefix text.
     */
    DebugOptions(bool enabled = false,
                 bool coloredOutput = true,
                 std::string keyColor = LOC_COLOR_DEFAULT,
                 std::string resetColor = LOC_COLOR_RESET,
                 std::string prefix = "")
        : enabled(enabled),
          coloredOutput(coloredOutput),
          keyColor(std::move(keyColor)),
          resetColor(std::move(resetColor)),
          prefix(std::move(prefix))
    {
    }
};

// ============================================================================
// Localizer
// ============================================================================

/**
 * @class Localizer
 * @brief Static class for managing all localization operations.
 *
 * @details
 * Provides a centralized localization manager capable of:
 * - Loading translation files from directories.
 * - Flattening JSON hierarchies into key-value maps.
 * - Handling runtime reloads and file modification detection.
 * - Performing thread-safe access to localization data.
 */
class Localizer
{
public:
    static constexpr const char *DEFAULT_LOCALE = LOC_DEFAULT_LOCALE; ///< Default locale identifier.

private:
#if LOC_THREAD_SAFE
    /**
     * @brief Returns global shared mutex for synchronization.
     * @return Reference to static shared_mutex.
     */
    static std::shared_mutex &getMutex()
    {
        static std::shared_mutex m;
        return m;
    }
#endif

    /**
     * @struct Node
     * @brief Internal helper structure for JSON traversal.
     */
    struct Node
    {
        const nlohmann::json *json; ///< Pointer to JSON node.
        std::string prefix;         ///< Current namespace prefix.
    };

    /**
     * @brief Flattens nested JSON into a flat key-value map.
     * @param root Root JSON object.
     * @param basePrefix Prefix for current hierarchy.
     * @param out Output map of flattened keys and values.
     */
    static void flattenJsonIterative(const nlohmann::json &root,
                                     const std::string &basePrefix,
                                     std::unordered_map<std::string, std::string> &out)
    {
        std::vector<Node> stack{{&root, basePrefix}};
        while (!stack.empty())
        {
            auto [node, prefix] = stack.back();
            stack.pop_back();
            for (auto &[key, value] : node->items())
            {
                std::string fullKey = prefix.empty() ? key : prefix + LOC_NAMESPACE_SEPARATOR + key;
                if (value.is_object())
                    stack.push_back({&value, fullKey});
                else if (value.is_string())
                    out[fullKey] = value.get<std::string>();
            }
        }
    }

    // --- Internal static data -------------------------------------------------
    inline static std::string currentLocale = DEFAULT_LOCALE; ///< Currently selected locale.
    inline static std::unordered_map<std::string,
                                     std::unordered_map<std::string, std::string>>
        translations;                                                                              ///< Language code → (key → string) map.
    inline static std::vector<std::filesystem::path> jsons;                                        ///< Loaded JSON paths.
    inline static std::unordered_map<std::string, std::filesystem::file_time_type> fileTimestamps; ///< File timestamps.
    inline static DebugOptions debugOptions;                                                       ///< Current debug configuration.
#if LOG_CERR == 0
    inline static ErrorCallback errorCallback;
#endif

public:
#if LOG_CERR == 0
    static void setErrorCallback(ErrorCallback cb)
    {
        LOC_WRITE_LOCK
        errorCallback = std::move(cb);
    }
#endif

    /**
     * @brief Loads translation data from a single JSON file.
     * @param path Path to the JSON file.
     * @throws std::runtime_error If the file cannot be opened or parsed.
     */
    static void loadFromFile(const std::string &path)
    {
        LOC_WRITE_LOCK
        using json = nlohmann::json;

        std::ifstream file(path);
        if (!file.is_open())
        {
#if LOG_CERR == 1
            std::cerr << "Cannot open language file: " + path;
#else
            if (errorCallback)
                errorCallback("Cannot open language file: " + path, 0);
#endif
        }

        json data;
        file >> data;

        std::filesystem::path p(path);
        std::string ns = p.stem().string();

        fileTimestamps[path] = std::filesystem::last_write_time(p);
        if (std::find(jsons.begin(), jsons.end(), p) == jsons.end())
            jsons.push_back(p);

        for (auto &[lang, root] : data.items())
        {
            std::unordered_map<std::string, std::string> flatMap;
            flattenJsonIterative(root, "", flatMap);
            for (auto &[key, value] : flatMap)
            {
                std::string namespacedKey = ns + LOC_NAMESPACE_SEPARATOR + key;
                translations[lang][namespacedKey] = value;
            }
        }
    }

    /**
     * @brief Loads all JSON translation files from a directory.
     * @param folderPath Directory containing language JSONs.
     * @param recursive Whether to include subdirectories.
     * @throws std::runtime_error If directory does not exist.
     */
    static void loadFromDirectory(const std::string &folderPath, bool recursive = false)
    {
        namespace fs = std::filesystem;
        if (!fs::exists(folderPath))
            throw std::runtime_error("Directory not found: " + folderPath);

        fs::directory_options options = fs::directory_options::skip_permission_denied;

        auto tryLoad = [](const fs::directory_entry &entry)
        {
            if (entry.is_regular_file() && entry.path().extension() == ".json")
            {
                try
                {
                    loadFromFile(entry.path().string());
                }
                catch (const std::exception &ex)
                {
#if LOG_CERR == 1
                    std::cerr << "[!] Failed to load " << entry.path() << ": " << ex.what() << "\n";
#else
                    if (errorCallback)
                        errorCallback("[!] Failed to load " + entry.path().string() + ": " + ex.what() + "\n", 1);
#endif
                }
            }
        };

        if (recursive)
        {
            for (const auto &entry : fs::recursive_directory_iterator(folderPath, options))
                tryLoad(entry);
        }
        else
        {
            for (const auto &entry : fs::directory_iterator(folderPath, options))
                tryLoad(entry);
        }
    }

    /**
     * @brief Reloads all loaded JSON files.
     * @param clearBefore If true, clears all existing translations before reload.
     */
    static void reloadAllJsons(bool clearBefore = false)
    {
        LOC_WRITE_LOCK
        if (clearBefore)
            translations.clear();

        for (const auto &json : jsons)
        {
            try
            {
                loadFromFile(json.string());
            }
            catch (const std::exception &ex)
            {
#if LOG_CERR == 1
                std::cerr << "[!] Failed to reload " << json << ": " << ex.what() << "\n";
#else
                if (errorCallback)
                    errorCallback("[!] Failed to reload " + json.string() + ": " + ex.what() + "\n", 2);
#endif
            }
        }
    }

    /**
     * @brief Checks for modified JSON files and reloads changed ones.
     */
    static void checkForJsonChanges()
    {
        LOC_WRITE_LOCK
        for (auto &[path, oldTime] : fileTimestamps)
        {
            if (!std::filesystem::exists(path))
                continue;
            auto newTime = std::filesystem::last_write_time(path);
            if (newTime != oldTime)
            {
                oldTime = newTime;
                std::cout << "🔁 Detected change in " << path << std::endl;
                loadFromFile(path);
            }
        }
    }

    /**
     * @brief Sets current locale.
     * @param locale Language code (e.g., "en", "fr").
     * @return true if locale exists, false otherwise.
     */
    [[nodiscard]] static bool setLocale(const std::string &locale)
    {
        LOC_WRITE_LOCK
        if (translations.contains(locale))
        {
            currentLocale = locale;
            return true;
        }
        return false;
    }

    /**
     * @brief Retrieves current locale.
     * @return Current locale code.
     */
    [[nodiscard]] static std::string getLocale() noexcept
    {
        LOC_READ_LOCK
        return currentLocale;
    }

    /**
     * @brief Translates a key into localized text.
     * @param key Translation key (e.g., "ui.button.play").
     * @return Localized string or missing-key placeholder.
     */
    [[nodiscard]] static std::string translate(const std::string &key)
    {
        LOC_READ_LOCK
        const auto &dbg = debugOptions;
        std::string prefix;
        if (dbg.enabled)
        {
            prefix = dbg.prefix;
            if (dbg.coloredOutput)
                prefix += dbg.keyColor + "[" + key + "]" + dbg.resetColor + " ";
            else
                prefix += "[" + key + "] ";
        }

        if (auto it = translations[currentLocale].find(key);
            it != translations[currentLocale].end())
            return prefix + it->second;

        if (auto it = translations[DEFAULT_LOCALE].find(key);
            it != translations[DEFAULT_LOCALE].end())
            return prefix + it->second;

        std::string missing = "[Missing:" + key + "]";
        if (dbg.enabled && dbg.coloredOutput)
            return dbg.keyColor + missing + dbg.resetColor;
        return missing;
    }

    /**
     * @brief Checks if the key exists in current or default locale.
     * @param key Translation key.
     * @return true if key exists.
     */
    [[nodiscard]] static bool hasKey(const std::string &key) noexcept
    {
        LOC_READ_LOCK
        return translations[currentLocale].contains(key) ||
               translations[DEFAULT_LOCALE].contains(key);
    }

    /**
     * @brief Enables or disables debug mode.
     * @param debugMode true to enable, false to disable.
     */
    static void setDebugMode(bool debugMode) noexcept
    {
        LOC_WRITE_LOCK
        debugOptions.enabled = debugMode;
    }

    /**
     * @brief Checks whether debug mode is active.
     * @return true if enabled.
     */
    [[nodiscard]] static bool isDebugModeOn() noexcept
    {
        LOC_READ_LOCK
        return debugOptions.enabled;
    }

    /**
     * @brief Sets the debug output configuration.
     * @param options DebugOptions structure.
     */
    static void setDebugOptions(const DebugOptions &options)
    {
        LOC_WRITE_LOCK
        debugOptions = options;
    }

    /**
     * @brief Retrieves modifiable debug options reference.
     * @return Reference to DebugOptions.
     */
    [[nodiscard]] static DebugOptions &getDebugOptions()
    {
        LOC_WRITE_LOCK
        return debugOptions;
    }

    /**
     * @brief Retrieves const reference to current debug options.
     * @return Const reference to DebugOptions.
     */
    [[nodiscard]] static const DebugOptions &getDebugOptionsConst() noexcept
    {
        LOC_READ_LOCK
        return debugOptions;
    }

    /**
     * @brief Prints summary statistics of loaded languages.
     */
    static void printStats()
    {
        LOC_READ_LOCK
        std::cout << "📦 LocalizeController loaded " << translations.size() << " languages:\n";
        for (const auto &[lang, map] : translations)
            std::cout << "  🌐 " << lang << " -> " << map.size() << " keys\n";
    }
};

// ============================================================================
// LocalizedString
// ============================================================================

/**
 * @class LocalizedString
 * @brief Represents a localized string with optional placeholder substitution.
 */
class LocalizedString
{
private:
    std::string key;                                     ///< Localization key.
    std::unordered_map<std::string, std::string> params; ///< Placeholder substitutions.

    /**
     * @brief Applies placeholder substitutions on the text.
     * @param text Original text with placeholders.
     * @param params Map of placeholder → value pairs.
     * @return Text with replaced placeholders.
     */
    static std::string applyPlaceholders(const std::string &text,
                                         const std::unordered_map<std::string, std::string> &params)
    {
#if LOC_USE_REGEX
        std::string result = text;
        for (const auto &[name, value] : params)
        {
            std::regex pattern("\\{" + name + "\\}");
            result = std::regex_replace(result, pattern, value);
        }
        return result;
#else
        std::string result;
        result.reserve(text.size() + 32);

        std::size_t pos = 0;
        while (pos < text.size())
        {
            auto open = text.find('{', pos);
            if (open == std::string::npos)
            {
                result.append(text, pos, text.size() - pos);
                break;
            }

            result.append(text, pos, open - pos);

            auto close = text.find('}', open + 1);
            if (close == std::string::npos)
            {
                result.append(text, open, text.size() - open);
                break;
            }

            std::string key = text.substr(open + 1, close - open - 1);
            if (auto it = params.find(key); it != params.end())
                result += it->second;
            else
                result.append(text, open, close - open + 1);

            pos = close + 1;
        }
        return result;
#endif
    }

public:
    /**
     * @brief Constructs a localized string without parameters.
     * @param key Translation key.
     */
    explicit LocalizedString(std::string key) : key(std::move(key)) {}

    /**
     * @brief Constructs a localized string with parameters.
     * @param key Translation key.
     * @param params Map of placeholder values.
     */
    LocalizedString(std::string key, std::unordered_map<std::string, std::string> params)
        : key(std::move(key)), params(std::move(params)) {}

    /**
     * @brief Retrieves the resolved localized string.
     * @return Localized text with substituted parameters.
     */
    [[nodiscard]] std::string str() const
    {
        std::string base = Localizer::translate(key);
        return params.empty() ? base : applyPlaceholders(base, params);
    }

    /**
     * @brief Implicit conversion to std::string.
     */
    [[nodiscard]] operator std::string() const { return str(); }

    /**
     * @brief Stream output operator.
     * @param os Output stream.
     * @param s LocalizedString instance.
     * @return Reference to output stream.
     */
    friend std::ostream &operator<<(std::ostream &os, const LocalizedString &s)
    {
        return os << s.str();
    }
};

/**
 * @def L
 * @brief Convenience macro for creating localized strings.
 * @param key Translation key.
 * @param ... Optional parameters.
 */
#ifndef L
#define L(key, ...) LocalizedString(key, ##__VA_ARGS__)
#endif

#endif // LOCALIZE_CONTROLLER_H