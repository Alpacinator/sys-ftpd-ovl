#define TESLA_INIT_IMPL
#include <tesla.hpp>
#include <sys/stat.h>
#include <cstring>

#define SYSMODULE_PROGRAM_ID 0x420000000000000EULL
#define FLAG_PATH "sdmc:/atmosphere/contents/420000000000000E/flags/boot2.flag"
#define FLAGS_DIR "sdmc:/atmosphere/contents/420000000000000E/flags"

char hostname[128] = {0};

bool isBootEnabled() {
    struct stat st;
    return stat(FLAG_PATH, &st) == 0;
}

void enableBoot() {
    mkdir(FLAGS_DIR, 0777);
    FILE *fp = fopen(FLAG_PATH, "w");
    if (fp) fclose(fp);
}

void disableBoot() {
    remove(FLAG_PATH);
}

bool isRunning() {
    u64 pid = 0;
    Result rc = pmdmntGetProcessId(&pid, SYSMODULE_PROGRAM_ID);
    return R_SUCCEEDED(rc) && pid != 0;
}

void startModule() {
    NcmProgramLocation loc = {
        .program_id = SYSMODULE_PROGRAM_ID,
        .storageID  = NcmStorageId_None,
    };
    pmshellLaunchProgram(0, &loc, nullptr);
}

void stopModule() {
    pmshellTerminateProgram(SYSMODULE_PROGRAM_ID);
}

class GuiMain : public tsl::Gui {
public:
    GuiMain() {}

    virtual tsl::elm::Element* createUI() override {
        auto frame = new tsl::elm::OverlayFrame("FTP Sysmodule", "sys-ftpd");
        auto list  = new tsl::elm::List();

        // IP address as a normal list item so it shows at full size
        m_ipItem = new tsl::elm::ListItem("IP Address");
        m_ipItem->setValue(std::string(hostname));
        list->addItem(m_ipItem);

        // Clickable status — press A to start/stop live
        m_statusItem = new tsl::elm::ListItem("FTP Server");
        m_statusItem->setClickListener([this](u64 keys) -> bool {
            if (keys & HidNpadButton_A) {
                if (isRunning()) {
                    stopModule();
                } else {
                    startModule();
                }
                updateStatusText();
                return true;
            }
            return false;
        });
        updateStatusText();
        list->addItem(m_statusItem);

        // Boot toggle — only manages the flag, no start/stop
        m_bootToggle = new tsl::elm::ToggleListItem("Start on boot", isBootEnabled());
        m_bootToggle->setStateChangedListener([](bool enabled) {
            if (enabled) {
                enableBoot();
            } else {
                disableBoot();
            }
        });
        list->addItem(m_bootToggle);

        frame->setContent(list);
        return frame;
    }

    virtual void update() override {
        m_frameCount++;
        if (m_frameCount >= 60) {
            m_frameCount = 0;
            updateStatusText();
            if (m_bootToggle)
                m_bootToggle->setState(isBootEnabled());
        }
    }

    virtual bool handleInput(u64 keysDown, u64 keysHeld, const HidTouchState &touchInput,
                             HidAnalogStickState leftJoyStick, HidAnalogStickState rightJoyStick) override {
        return false;
    }

private:
    tsl::elm::ListItem       *m_ipItem     = nullptr;
    tsl::elm::ListItem       *m_statusItem = nullptr;
    tsl::elm::ToggleListItem *m_bootToggle = nullptr;
    int m_frameCount = 0;

    void updateStatusText() {
        if (m_statusItem)
            m_statusItem->setValue(isRunning() ? "Running" : "Stopped");
    }
};

class OverlayMain : public tsl::Overlay {
public:
    virtual void initServices() override {
        tsl::hlp::doWithSmSession([]{
            gethostname(hostname, sizeof(hostname));
            if (strlen(hostname) == 0)
                strcpy(hostname, "No IP");
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