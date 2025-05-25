#include <string>

class Settings {
public:
    static Settings& getInstance() {
        static Settings instance;
        return instance;
    }

    Settings(const Settings&) = delete;
    Settings& operator=(const Settings&) = delete;

    bool showImGuiShaderDebugMenu = false;
    std::string shaderDebugMode = "none";

    int shaderDebugModeToInt(const std::string& mode) {
        if (mode == "none")
            return 0;
        if (mode == "normal")
            return 1;
        if (mode == "albedo")
            return 2;
        if (mode == "depth")
            return 3;
        if (mode == "metallic")
            return 4;
        if (mode == "roughness")
            return 5;
        if (mode == "emission")
            return 6;
        if (mode == "ambient")
            return 7;
        if (mode == "metailic_roughness")
            return 8;
        if (mode == "wireframe")
            return 9;
        if (mode == "texture_coordinates")
            return 10;

        return -1; // Invalid mode
    }

private:
    Settings() {}
};

// Usage
// Settings& settings = Settings::getInstance();
// settings.showImGuiShaderDebugMenu = true;
// int debugMode = settings.shaderDebugModeToInt(settings.shaderDebugMode);
