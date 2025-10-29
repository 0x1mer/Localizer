#include <iostream>
#include <thread>
#include <chrono>
#include <Localizer.h>

void errorCallback(const std::string& errorMsg, int errorCode) {
    std::cout
        << "\x1b[31m[ERR] "
        << errorMsg
        << " [ERROR_CODE: "
        << errorCode
        << "]\x1b[0m\n";
}

/**
 * @brief Demonstration program for the LocalizeController library.
 *
 * This example shows how to:
 * - Set callback function to intersect errors
 * - Load JSON translation files
 * - Retrieve localized strings
 * - Use parameter substitution
 * - Switch between locales at runtime
 * - Enable debug/colored output
 * - Detect and reload modified JSON files
 */
int main()
{
    using namespace std::chrono_literals;

    try
    {
        std::cout << "===============================\n";
        std::cout << " 🈶 Localizer Showcase\n";
        std::cout << "===============================\n\n";

        // 1️⃣ Load translation files
        std::cout << "📂 Loading translations...\n";
        Localizer::setErrorCallback(errorCallback);
        Localizer::loadFromDirectory("C:\\Users\\Oximer\\Documents\\VSCodeProjects\\LocalizedString\\langs");
        Localizer::setDebugMode(false);
        Localizer::printStats();

        // 2️⃣ Set the current locale
        std::cout << "\n🌐 Setting locale to 'en'...\n";
        if (!Localizer::setLocale("en"))
        {
            std::cerr << "❌ Locale 'en' not found!\n";
            return 1;
        }

        // 3️⃣ Basic translation
        std::cout << "\n=== Basic translation ===\n";
        std::cout << L("ui.button.play") << "\n";
        std::cout << L("ui.menu.exit") << "\n";
        std::cout << L("ui.nonexistent") << "\n";

        // 4️⃣ Locale switching
        std::cout << "\n=== Locale switching ===\n";
        if (Localizer::setLocale("fr"))
        {
            std::cout << L("ui.button.play") << "\n";
            std::cout << L("ui.menu.exit") << "\n";
        }
        else
        {
            std::cout << "⚠️  Locale 'fr' not found, staying on 'en'.\n";
            Localizer::setLocale("en");
        }

        // 5️⃣ Placeholder substitution
        std::cout << "\n=== Placeholder substitution ===\n";
        std::unordered_map<std::string, std::string> params = {
            {"username", "Oksi"},
            {"score", "9000"}};

        std::cout << L("messages.welcome", params) << "\n";

        // 6️⃣ Key existence check
        std::cout << "\n=== Key existence check ===\n";
        std::cout << "Has 'ui.button.play'? "
                  << (Localizer::hasKey("ui.button.play") ? "✅" : "❌") << "\n";
        std::cout << "Has 'ui.random.thing'? "
                  << (Localizer::hasKey("ui.random.thing") ? "✅" : "❌") << "\n";

        // 7️⃣ Debug mode (colored key visualization)
        std::cout << "\n=== Debug mode ===\n";
        DebugOptions opts;
        opts.enabled = true;
        opts.coloredOutput = true;
        opts.keyColor = "\x1b[36m"; // cyan
        opts.prefix = "[DBG] ";
        Localizer::setDebugOptions(opts);

        std::cout << L("ui.button.play") << "\n";
        std::cout << L("ui.menu.exit") << "\n";
        std::cout << L("messages.welcome", params) << "\n";

        // 8️⃣ Hot reload demonstration
        std::cout << "\n=== Hot reload simulation ===\n";
        std::cout << "💡 Edit any JSON file (e.g. ui.json) and save it while the program is running.\n";

        for (int i = 1; i <= 5; ++i)
        {
            std::this_thread::sleep_for(2s);
            Localizer::checkForJsonChanges();
            std::cout << "Check " << i << ": " << L("ui.button.play") << "\n";
        }

        std::cout << "\n✅ All tests completed successfully.\n";
        return 0;
    }
    catch (const std::exception &ex)
    {
        std::cerr << "\n❌ Exception: " << ex.what() << "\n";
        return 1;
    }
}