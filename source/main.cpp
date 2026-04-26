#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <sys/stat.h>
#include <cstring>
#include <string>
#include <fstream>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>

#define SYSMODULE_PROGRAM_ID 0x420000000000000EULL
#define FLAG_PATH   "sdmc:/atmosphere/contents/420000000000000E/flags/boot2.flag"
#define FLAGS_DIR   "sdmc:/atmosphere/contents/420000000000000E/flags"
#define CONFIG_PATH "sdmc:/config/sys-ftpd/config.ini"

char hostname[128] = {0};

// ─── Config ──────────────────────────────────────────────────────────────────

struct FtpConfig {
    std::string user;
    std::string password;
    std::string port          = "5000";
    std::string anonymous     = "0";
    std::string led           = "1";
    std::string pauseDisabled = "0";
    std::string pauseKeycombo = "PLUS+MINUS+X";
    std::string passLength    = "6";   // 4 | 6 | 8 | 10 | 12
    std::string passUppercase = "1";   // 1 = uppercase only, 0 = mixed case
    std::string passChars     = "1";   // 1 = include special characters
    std::vector<std::string> userPresets;
    std::vector<std::string> portPresets;
};

std::vector<std::string> splitCSV(const std::string &s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        while (!item.empty() && item[0] == ' ') item = item.substr(1);
        while (!item.empty() && item.back() == ' ') item.pop_back();
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

std::string joinCSV(const std::vector<std::string> &v) {
    std::string out;
    for (size_t i = 0; i < v.size(); i++) {
        if (i > 0) out += ",";
        out += v[i];
    }
    return out;
}

FtpConfig readConfig() {
    FtpConfig cfg;
    // Default presets
    cfg.userPresets = {"switch", "ftpd"};
    cfg.portPresets = {"2121", "5000"};

    std::ifstream f(CONFIG_PATH);
    if (!f.is_open()) return cfg;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        auto eq = line.find(":=");
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 2);
        while (!key.empty() && key.back() == ' ') key.pop_back();
        while (!val.empty() && val[0] == ' ')     val = val.substr(1);
        auto hash = val.find(" #");
        if (hash != std::string::npos) val = val.substr(0, hash);

        if      (key == "user")          cfg.user          = val;
        else if (key == "password")      cfg.password      = val;
        else if (key == "port")          cfg.port          = val.empty() ? "5000" : val;
        else if (key == "anonymous")     cfg.anonymous     = val;
        else if (key == "led")           cfg.led           = val;
        else if (key == "disabled")      cfg.pauseDisabled = val;
        else if (key == "keycombo")      cfg.pauseKeycombo = val;
        else if (key == "pass_length")   cfg.passLength    = val.empty() ? "4" : val;
        else if (key == "pass_upper")    cfg.passUppercase = val;
        else if (key == "pass_chars")    cfg.passChars     = val;
        else if (key == "users")         cfg.userPresets   = splitCSV(val);
        else if (key == "ports")         cfg.portPresets   = splitCSV(val);
    }
    // Remove "anonymous" from user presets if present
    cfg.userPresets.erase(
        std::remove(cfg.userPresets.begin(), cfg.userPresets.end(), "anonymous"),
        cfg.userPresets.end());

    // If the current user isn't in the presets list, add it
    if (!cfg.user.empty() && cfg.user != "anonymous") {
        bool found = false;
        for (const auto &u : cfg.userPresets)
            if (u == cfg.user) { found = true; break; }
        if (!found)
            cfg.userPresets.insert(cfg.userPresets.begin(), cfg.user);
    }

    return cfg;
}

void writeConfig(const FtpConfig &cfg) {
    mkdir("sdmc:/config",          0777);
    mkdir("sdmc:/config/sys-ftpd", 0777);
    std::ofstream f(CONFIG_PATH);
    if (!f.is_open()) return;
    f << "[User]\n"      << "user:="      << cfg.user          << "\n"
      << "[Password]\n"  << "password:="  << cfg.password      << "\n"
      << "[Port]\n"      << "port:="      << cfg.port          << "\n"
      << "[Anonymous]\n" << "anonymous:=" << cfg.anonymous     << "\n"
      << "[Pause]\n"
      << "disabled:="    << cfg.pauseDisabled                  << "\n"
      << "keycombo:="    << cfg.pauseKeycombo                  << "\n"
      << "[LED]\n"       << "led:="       << cfg.led           << "\n"
      << "[Advanced]\n"
      << "pass_length:=" << cfg.passLength                     << "\n"
      << "pass_upper:="  << cfg.passUppercase                  << "\n"
      << "pass_chars:="  << cfg.passChars                      << "\n"
      << "[Presets]\n"
      << "users:="       << joinCSV(cfg.userPresets)           << "\n"
      << "ports:="       << joinCSV(cfg.portPresets)           << "\n";
}

// ─── Sysmodule ────────────────────────────────────────────────────────────────

bool isBootEnabled() { struct stat st; return stat(FLAG_PATH, &st) == 0; }
void enableBoot()    { mkdir(FLAGS_DIR, 0777); FILE *f = fopen(FLAG_PATH, "w"); if (f) fclose(f); }
void disableBoot()   { remove(FLAG_PATH); }

bool isRunning() {
    u64 pid = 0;
    return R_SUCCEEDED(pmdmntGetProcessId(&pid, SYSMODULE_PROGRAM_ID)) && pid != 0;
}

void startModule() {
    NcmProgramLocation loc = { .program_id = SYSMODULE_PROGRAM_ID, .storageID = NcmStorageId_None };
    pmshellLaunchProgram(0, &loc, nullptr);
}

void stopModule() { pmshellTerminateProgram(SYSMODULE_PROGRAM_ID); }

// ─── Helpers ─────────────────────────────────────────────────────────────────

std::string generatePassword(const FtpConfig &cfg) {
    static bool seeded = false;
    if (!seeded) { srand((unsigned)time(nullptr)); seeded = true; }

    int len = std::stoi(cfg.passLength);
    bool uppercase = (cfg.passUppercase == "1");
    bool useChars  = (cfg.passChars     == "1");

    // Build the character pool
    std::string letters = uppercase ? "ABCDEFGHJKLMNPQRSTUVWXYZ"
                                    : "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghjkmnpqrstuvwxyz";
    const std::string digits  = "0123456789";
    const std::string special = "!@#$%^&*";

    // Guarantee at least one digit, and one special char when enabled
    std::string out;
    out += letters[rand() % letters.size()];      // at least one letter
    out += digits [rand() % digits.size()];       // at least one digit
    if (useChars)
        out += special[rand() % special.size()];  // at least one special

    // Build a full pool for the remaining characters
    std::string pool = letters + digits;
    if (useChars) pool += special;

    while ((int)out.size() < len)
        out += pool[rand() % pool.size()];

    // Shuffle so the guaranteed chars aren't always at the front
    for (int i = (int)out.size() - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        std::swap(out[i], out[j]);
    }
    return out;
}

// Returns index of val in list, or 0 if not found
size_t findIndex(const std::vector<std::string> &list, const std::string &val) {
    for (size_t i = 0; i < list.size(); i++)
        if (list[i] == val) return i;
    return 0;
}

// ─── GUI ─────────────────────────────────────────────────────────────────────

class GuiMain : public tsl::Gui {
public:
    GuiMain() {
        m_cfg = readConfig();
        // Write back immediately so any newly added presets are persisted
        writeConfig(m_cfg);
    }

    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("Manage FTPD", "sys-ftpd");
        auto list  = new tsl::elm::List();

        // IP
        list->addItem(new tsl::elm::ListItem(std::string(hostname) + ":" + m_cfg.port));

        // ── Info text ─────────────────────────────────────────────────────
        list->addItem(new tsl::elm::CustomDrawer([](tsl::gfx::Renderer *r, s32 x, s32 y, s32 w, s32 h) {
            const tsl::Color col = r->a(tsl::style::color::ColorDescription);
            const float sz = 15.0f;
            const s32 lh   = 20; // line height
            r->drawString("Turn the FTP server off and on to",  false, x + 19, y + lh * 1, sz, col);
            r->drawString("apply settings. You can add your",   false, x + 19, y + lh * 2, sz, col);
            r->drawString("own username to the presets in the", false, x + 19, y + lh * 3, sz, col);
            r->drawString("config file located at:",            false, x + 19, y + lh * 4, sz, col);
            r->drawString(CONFIG_PATH,                          false, x + 19, y + lh * 5, sz, col);
        }), 115);

        // Status section
        list->addItem(new tsl::elm::CategoryHeader("Status"));

        // Status / live toggle
        m_statusItem = new tsl::elm::ListItem("FTP Server");
        m_statusItem->setClickListener([this](u64 keys) -> bool {
            if (keys & HidNpadButton_A) {
                if (isRunning()) stopModule(); else startModule();
                updateStatusText();
                return true;
            }
            return false;
        });
        updateStatusText();
        list->addItem(m_statusItem);

        // Boot toggle
        auto bootToggle = new tsl::elm::ToggleListItem("Start on boot", isBootEnabled());
        bootToggle->setStateChangedListener([](bool en) { en ? enableBoot() : disableBoot(); });
        list->addItem(bootToggle);

        // Config section
        list->addItem(new tsl::elm::CategoryHeader("Configuration"));

        // Anonymous toggle
        auto anonToggle = new tsl::elm::ToggleListItem("Anonymous login", m_cfg.anonymous == "1");
        anonToggle->setStateChangedListener([this](bool en) {
            m_cfg.anonymous = en ? "1" : "0";
            writeConfig(m_cfg);
        });
        list->addItem(anonToggle);

        // Username
        m_userItem = new tsl::elm::ListItem("Username");
        m_userItem->setValue(m_cfg.user.empty() ? "anonymous" : m_cfg.user);
        m_userItem->setClickListener([this](u64 keys) -> bool {
            if (keys & HidNpadButton_A) {
                if (m_cfg.userPresets.empty()) return true;
                size_t cur = findIndex(m_cfg.userPresets, m_cfg.user.empty() ? "anonymous" : m_cfg.user);
                size_t next = (cur + 1) % m_cfg.userPresets.size();
                m_cfg.user = m_cfg.userPresets[next];
                m_userItem->setValue(m_cfg.user);
                writeConfig(m_cfg);
                return true;
            }
            return false;
        });
        list->addItem(m_userItem);

        // Password
        m_passItem = new tsl::elm::ListItem("Password");
        m_passItem->setValue(m_cfg.password.empty() ? "Generate" : m_cfg.password);
        m_passItem->setClickListener([this](u64 keys) -> bool {
            if (keys & HidNpadButton_A) {
                m_cfg.password = generatePassword(m_cfg);
                m_passItem->setValue(m_cfg.password);
                writeConfig(m_cfg);
                return true;
            }
            return false;
        });
        list->addItem(m_passItem);

        // Port — always shown
        m_portItem = new tsl::elm::ListItem("Port");
        m_portItem->setValue(m_cfg.port);
        m_portItem->setClickListener([this](u64 keys) -> bool {
            if (keys & HidNpadButton_A) {
                if (m_cfg.portPresets.empty()) return true;
                size_t cur  = findIndex(m_cfg.portPresets, m_cfg.port);
                size_t next = (cur + 1) % m_cfg.portPresets.size();
                m_cfg.port  = m_cfg.portPresets[next];
                m_portItem->setValue(m_cfg.port);
                writeConfig(m_cfg);
                return true;
            }
            return false;
        });
        list->addItem(m_portItem);

        // LED
        auto ledToggle = new tsl::elm::ToggleListItem("LED on connect", m_cfg.led == "1");
        ledToggle->setStateChangedListener([this](bool en) {
            m_cfg.led = en ? "1" : "0";
            writeConfig(m_cfg);
        });
        list->addItem(ledToggle);

        // ── Advanced section ──────────────────────────────────────────────
        list->addItem(new tsl::elm::CategoryHeader("Advanced"));

        // Password length cycler (4 → 6 → 8 → 10 → 12)
        static const std::vector<std::string> passLengths = {"4","6","8","10","12"};
        m_passLenItem = new tsl::elm::ListItem("Password length");
        m_passLenItem->setValue(m_cfg.passLength);
        m_passLenItem->setClickListener([this](u64 keys) -> bool {
            if (keys & HidNpadButton_A) {
                size_t cur  = findIndex(passLengths, m_cfg.passLength);
                size_t next = (cur + 1) % passLengths.size();
                m_cfg.passLength = passLengths[next];
                m_passLenItem->setValue(m_cfg.passLength);
                writeConfig(m_cfg);
                return true;
            }
            return false;
        });
        list->addItem(m_passLenItem);

        // Uppercase toggle
        auto upperToggle = new tsl::elm::ToggleListItem("Uppercase only", m_cfg.passUppercase == "1");
        upperToggle->setStateChangedListener([this](bool en) {
            m_cfg.passUppercase = en ? "1" : "0";
            writeConfig(m_cfg);
        });
        list->addItem(upperToggle);

        // Special characters toggle
        auto charsToggle = new tsl::elm::ToggleListItem("Special characters", m_cfg.passChars == "1");
        charsToggle->setStateChangedListener([this](bool en) {
            m_cfg.passChars = en ? "1" : "0";
            writeConfig(m_cfg);
        });
        list->addItem(charsToggle);

        frame->setContent(list);

        return frame;
    }

    virtual void update() override {
        m_frameCount++;
        if (m_frameCount >= 60) {
            m_frameCount = 0;
            updateStatusText();
        }
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchInput,
                             HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
        return false;
    }

private:
    FtpConfig             m_cfg;
    tsl::elm::ListItem   *m_statusItem  = nullptr;
    tsl::elm::ListItem   *m_userItem    = nullptr;
    tsl::elm::ListItem   *m_passItem    = nullptr;
    tsl::elm::ListItem   *m_portItem    = nullptr;
    tsl::elm::ListItem   *m_passLenItem = nullptr;
    int m_frameCount = 0;

    void updateStatusText() {
        if (m_statusItem)
            m_statusItem->setValue(isRunning() ? "On" : "Off");
    }
};

// ─── Overlay entry ────────────────────────────────────────────────────────────

class OverlayMain : public tsl::Overlay {
public:
    virtual void initServices() override {
        tsl::hlp::doWithSmSession([]{
            gethostname(hostname, sizeof(hostname));
            if (strlen(hostname) == 0) strcpy(hostname, "No IP");
        });
        fsInitialize();
        fsdevMountSdmc();
        pmdmntInitialize();
        pmshellInitialize();
    }

    virtual void exitServices() override {
        pmshellExit();
        pmdmntExit();
        fsdevUnmountAll();
        fsExit();
    }

    virtual void onShow() override {}
    virtual void onHide() override {}

    virtual std::unique_ptr<tsl::Gui> loadInitialGui() override {
        return initially<GuiMain>();
    }
};

int main(int argc, char **argv) {
    return tsl::loop<OverlayMain>(argc, argv);
}