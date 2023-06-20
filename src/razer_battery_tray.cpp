
#include <libusb-1.0/libusb.h>
#include <vector>

#include <windows.h>
#include <shellapi.h>
#include <iostream>
#include <cstring>
#include <string>

#include <thread>
#include <mutex>
#include <condition_variable>
#include <math.h>

#define VENDOR_ID 0x1532    // Razer dethadder v2 Pro
#define TRAN_ID 0x3f        // Razer dethadder v2 Pro
#define PRODUCT_ID 0x007D   // Razer dethadder v2 Pro

#define TIMEOUT 500

std::mutex m;
std::condition_variable cv;
bool active = true;

class RazerTrayIcon
{
    public:
        std::string ico_path;
        void run()
        {
            createWindow();
            checkmsgs();
        }
    
        void setTitle(float battery)
        {
            std::memset(&nid, 0, sizeof(nid));
            nid.cbSize = sizeof(nid);
            nid.hWnd = hWnd;
            nid.uID = 0;
            nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
            nid.uCallbackMessage = WM_APP + 1;

            std::string title = "Razer battery: " + std::to_string(battery) + "%";
            std::string path = ico_path + "bat_" + std::to_string(static_cast <int>(std::floor(battery))) + ".ico"; 

            nid.hIcon = (HICON) LoadImage(nullptr, path.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE);
            lstrcpyA(nid.szTip, title.c_str());
            Shell_NotifyIcon(NIM_MODIFY, &nid);
        }
        
    private:
        LPCSTR lpszClass = "__hidden__";
        BOOL WINAPI ConsoleRoutine(DWORD dwCtrlType);
        HINSTANCE hInstance = GetModuleHandle(nullptr);
        WNDCLASSA wc;
        HWND hWnd;
        MSG msg;
        NOTIFYICONDATAA nid;
        static RazerTrayIcon* currentInstance;

        void createWindow()
        {
            wc.cbClsExtra = 0;
            wc.cbWndExtra = 0;
            wc.hbrBackground = nullptr;
            wc.hCursor = nullptr;
            wc.hIcon = nullptr;
            wc.hInstance = hInstance;
            wc.lpfnWndProc = WndProc;
            wc.lpszClassName = lpszClass;
            wc.lpszMenuName = nullptr;
            wc.style = 0;
            RegisterClassA(&wc);

            hWnd = CreateWindowA(lpszClass, lpszClass, WS_OVERLAPPEDWINDOW,
                CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                nullptr, nullptr, hInstance, nullptr);
            currentInstance = this;

        }
        
        void checkmsgs()
        {
            while (GetMessage(&msg, nullptr, 0, 0))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }

        void closeapp()
        {
            active = false;
            PostQuitMessage(0);
        }

        static LRESULT CALLBACK WndProc(HWND hWnd, UINT iMsg, WPARAM wParam, LPARAM lParam)
        {
            static NOTIFYICONDATAA nid;
             const int EXITAPP = 101;

            switch (iMsg)
            {
                case WM_CREATE:
                    std::memset(&nid, 0, sizeof(nid));
                    nid.cbSize = sizeof(nid);
                    nid.hWnd = hWnd;
                    nid.uID = 0;
                    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
                    nid.uCallbackMessage = WM_APP + 1;
                    nid.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
                    lstrcpyA(nid.szTip, "Razer battery:");
                    Shell_NotifyIcon(NIM_ADD, &nid);
                    Shell_NotifyIcon(NIM_SETVERSION, &nid);
                    return 0;

                case WM_COMMAND:
                    switch (LOWORD(wParam))
                    {
                        case EXITAPP:
                            currentInstance->closeapp();
                            break;
                    }

                case WM_APP + 1:
                    switch (lParam)
                    {                            
                        case WM_LBUTTONDBLCLK:
                        case WM_RBUTTONDOWN:
                        case WM_CONTEXTMENU:
                            HMENU hMenu = CreatePopupMenu();
                            AppendMenu(hMenu, MF_STRING, EXITAPP, "Quit");
                            POINT pt;
                            GetCursorPos(&pt);
                            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, pt.x, pt.y, 0, hWnd, nullptr);
                            DestroyMenu(hMenu);
                            break;
                    }
                    break;
            }
            return DefWindowProc(hWnd,iMsg,wParam,lParam);
        }
};

class RazerBattery
{
    private:
        std::vector<unsigned char> battery_msg() 
        {
        std::vector<unsigned char> msg = {0x00, TRAN_ID, 0x00, 0x00, 0x00, 0x02, 0x07, 0x80};
        unsigned char crc = 0;
        for (size_t i = 2; i < msg.size(); i++) {
            crc ^= msg[i];
        }
        msg.insert(msg.end(), 80, 0);
        msg.push_back(crc);
        msg.push_back(0);
        return msg;
    }

    public:
        libusb_device* FindRazer()
        {
            libusb_init(NULL);
            libusb_device **device_list;
            ssize_t count = libusb_get_device_list(NULL, &device_list);
            libusb_device_handle *handle = NULL;
            for (int i = 0; i < count; i++) 
            {
                int ret;
                libusb_device *device = device_list[i];
                struct libusb_device_descriptor desc;
                libusb_get_device_descriptor(device, &desc);
                if (desc.idVendor == VENDOR_ID && desc.idProduct == PRODUCT_ID) 
                {
                    return device;
                }
            }
            return NULL;
        }

        float battery_status()
        {
            float battery_value = 0.;
            int ret;
            std::vector<unsigned char> battery_msg_vec = battery_msg();
            unsigned char* byteArray = new unsigned char[battery_msg_vec.size()];
            std::copy(battery_msg_vec.begin(), battery_msg_vec.end(), byteArray);

            libusb_device_handle *handle = NULL;
            libusb_device* device = FindRazer();

            ret = libusb_open(device, &handle);
            if (ret != LIBUSB_SUCCESS) 
            {
                return 0.;
            }

            ret = libusb_claim_interface(handle, 0);
            if (ret != LIBUSB_SUCCESS) 
            {
                libusb_close(handle);
                return 0.;
            }

            ret = libusb_control_transfer(handle, 0x21, 0x09, 0x300, 0x00, byteArray, 90, TIMEOUT);
            if (ret < 0) 
            {
                libusb_release_interface(handle, 0);
                libusb_close(handle);
                return 0.;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));

            unsigned char buffer[90];
            int result = libusb_control_transfer(handle, 0xa1, 0x01, 0x300, 0x00, buffer, 90, TIMEOUT);
            
            battery_value = buffer[9] / 255. * 100.;

            libusb_release_interface(handle, 0);
            libusb_close(handle);

            libusb_exit(NULL);
            return battery_value;
        }
};


RazerTrayIcon* RazerTrayIcon::currentInstance = nullptr;

void timerThreadFunction(RazerTrayIcon* rti, uint16_t timeout)
{
    //set values without waiting cv timeout
    RazerBattery rb;
    float bat_st = rb.battery_status();
    rti->setTitle(bat_st);

    std::unique_lock lock(m);
    while (active)
    {
        if (cv.wait_for(lock, std::chrono::seconds(timeout)) == std::cv_status::timeout)
        {
            RazerBattery rb;
            float bat_st = rb.battery_status();
            rti->setTitle(bat_st);
        }
        else
        {
            break;
        }
    }
}


int main(int argc, char* argv[])
{
    int prod_id = 0;
    std::string ico_path = "E://razer_icons//";
    int timeout = 300;
    for (int i = 0; i < argc; i++) {
        if (strcmp(argv[i], "--timeout") == 0 && i+1 < argc) {
            timeout = atoi(argv[i+1]);
        }
        if (strcmp(argv[i], "--icons") == 0 && i+1 < argc) {
            ico_path = argv[i+1];
        }
    }
    
    RazerTrayIcon rti;
    rti.ico_path = ico_path;
    std::thread t(&RazerTrayIcon::run, &rti);

    std::thread timerThread(&timerThreadFunction, &rti, timeout);
    t.join();
    cv.notify_one();
    timerThread.join();
}

