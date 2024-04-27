//////////////////////////////////////////////////////////////////////////////////////////

#ifndef INC_BLE_H
#define INC_BLE_H

//////////////////////////////////////////////////////////////////////////////////////////

#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Storage.Streams.h>
#include <winrt/Windows.Devices.Radios.h>
#include <winrt/Windows.Devices.Bluetooth.h>
#include <winrt/Windows.Devices.Bluetooth.Advertisement.h>
#include <winrt/Windows.Devices.Bluetooth.GenericAttributeProfile.h>
#include <winrt/Windows.Devices.Enumeration.h>

#include <flutter/method_channel.h>
#include <flutter/basic_message_channel.h>
#include <flutter/event_channel.h>
#include <flutter/event_stream_handler_functions.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>
#include <flutter/standard_message_codec.h>

#include <map>
#include <memory>
#include <sstream>
#include <algorithm>
#include <iomanip>

//////////////////////////////////////////////////////////////////////////////////////////

using namespace winrt;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Storage::Streams;
using namespace winrt::Windows::Devices::Radios;
using namespace winrt::Windows::Devices::Bluetooth;
using namespace winrt::Windows::Devices::Bluetooth::Advertisement;
using namespace winrt::Windows::Devices::Bluetooth::GenericAttributeProfile;
using namespace winrt::Windows::Devices::Enumeration;

//////////////////////////////////////////////////////////////////////////////////////////

#define ERR_OK                  0
//FromBluetoothAddressAsync
#define ERR_GET_DEVICE          1
#define ERR_GET_SERVICES        2
#define ERR_GET_ADAPTER         3
#define ERR_GET_RADIO           4
#define ERR_NO_DEVICE_WATCHER   5

//////////////////////////////////////////////////////////////////////////////////////////

int bmAdapterStateEnum(Windows::Devices::Radios::RadioState as);

//////////////////////////////////////////////////////////////////////////////////////////

int bmConnectionStatusEnum(BluetoothConnectionStatus status);

//////////////////////////////////////////////////////////////////////////////////////////


class BLEHelper
{
    DeviceWatcher mDeviceWatcher{ nullptr };
    //used for system device call
    DeviceWatcher mSysDeviceWatcher{ nullptr };
    BluetoothLEDevice mConnectedDevice{ nullptr };
    Radio mRadio{ nullptr };
    BluetoothAdapter mAdapter{ nullptr };
    HANDLE mhSystemDevices;

    winrt::event_token mDeviceWatcherAddedToken;
    winrt::event_token mDeviceWatcherUpdatedToken;
    winrt::event_token mDeviceWatcherRemovedToken;
    winrt::event_token mDeviceWatcherEnumerationCompletedToken;
    winrt::event_token mDeviceWatcherStoppedToken;

    winrt::event_token mSysDeviceWatcherAddedToken;
    winrt::event_token mSysDeviceWatcherUpdatedToken;
    winrt::event_token mSysDeviceWatcherRemovedToken;
    winrt::event_token mSysDeviceWatcherEnumerationCompletedToken;
    winrt::event_token mSysDeviceWatcherStoppedToken;

    winrt::event_token mConnnectionStatusChangedToken;
    std::map<std::string, flutter::EncodableMap> mDevices;
    std::map<std::string, flutter::EncodableMap> mSysDevices;
public:
    BLEHelper(
        );

    virtual ~BLEHelper(
        );

    IAsyncOperation<int> InitRadio(
    );

    VOID DisconnectDevice(
        );

    LONG StartScan(
        );

    LONG StopScan(
        );

    LONG SysStartScan(
    );

    LONG SysStopScan(
    );

    RadioState GetBtState(
        );

    //concurrency::task<int> GetConnectedDevices();

    void DeviceWatcher_Added(
        DeviceWatcher sender,
        DeviceInformation info
        );

    void DeviceWatcher_Updated(
        DeviceWatcher sender,
        DeviceInformationUpdate info
        );

    void DeviceWatcher_Removed(
        DeviceWatcher sender,
        DeviceInformationUpdate info
        );

    void DeviceWatcher_EnumerationCompleted(
        DeviceWatcher sender,
        IInspectable obj
        );

    void DeviceWatcher_Stopped(
        DeviceWatcher sender,
        IInspectable obj
        );

    winrt::fire_and_forget ConnectDevice(
        std::string deviceID
        );

    void SendConnectionState(
        BluetoothLEDevice device,
        BluetoothConnectionStatus status
        );

    void SendConnectionState(
        std::string device,
        BluetoothConnectionStatus status
    );

    IAsyncOperation<int> TestCall(
    );

    void SysDeviceWatcher_Added(
        DeviceWatcher sender,
        DeviceInformation info
    );

    void SysDeviceWatcher_Updated(
        DeviceWatcher sender,
        DeviceInformationUpdate info
    );

    void SysDeviceWatcher_Removed(
        DeviceWatcher sender,
        DeviceInformationUpdate info
    );

    void SysDeviceWatcher_EnumerationCompleted(
        DeviceWatcher sender,
        IInspectable obj
    );


    void SysDeviceWatcher_Stopped(
        DeviceWatcher sender,
        IInspectable obj
    );

    std::map<std::string, flutter::EncodableMap> GetSysDevices(
    );

    void BluetoothLEDevice_ConnectionStatusChanged(BluetoothLEDevice sender, IInspectable args);

    winrt::fire_and_forget NotifyForAdapterState();
};


//////////////////////////////////////////////////////////////////////////////////////////

#endif

//////////////////////////////////////////////////////////////////////////////////////////

