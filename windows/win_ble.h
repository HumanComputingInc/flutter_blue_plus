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

#include <fstream>
#include <string>
#include <iostream>

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

class ServiceData
{
public:
    GattDeviceService service{nullptr};
    std::vector<GattCharacteristic> chars;
    std::map<std::string, winrt::event_token> valueChangedTokens;
};

//////////////////////////////////////////////////////////////////////////////////////////

class BLEHelper
{
    DeviceWatcher mDeviceWatcher{ nullptr };
    //used for system device call
    DeviceWatcher mSysDeviceWatcher{ nullptr };
    Radio mRadio{ nullptr };
    BluetoothAdapter mAdapter{ nullptr };
    HANDLE mhSysScanEvent;

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
    winrt::event_token gatt_services_changed_token_;
    std::map<std::string, flutter::EncodableMap> mDevices;
    std::map<std::string, flutter::EncodableMap> mSysDevices;
    std::ofstream mLogFile;

    //connected device data
    BluetoothLEDevice mConnectedDevice{ nullptr };
    std::vector<ServiceData> mSerivcesData;
public:
    BLEHelper(
        );

    virtual ~BLEHelper(
        );

    IAsyncOperation<int> InitRadio(
    );

    VOID DisconnectDevice(
        std::string mac
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
    void WriteLogFile(std::string msg);
    winrt::fire_and_forget GetServices();
    winrt::fire_and_forget FakeRSSI();

    winrt::fire_and_forget SetCharNotify(
        bool Enable,
        bool forceIndications,
        GattCharacteristic theChar,
        std::string deviceID,
        ServiceData serviceData,
        std::string service_uuid,
        std::string characteristic_uuid
        );

    void GattCharacteristic_ValueChanged(
        GattCharacteristic sender,
        GattValueChangedEventArgs args
        );


    int GetServiceDataIndex(
        std::string uuid
        );

    ServiceData GetServiceData(
        int nIndex
        );

    GattCharacteristic FindCharactersitic(
        ServiceData serviceData,
        std::string charUUID
    );

    flutter::EncodableMap ExportCharacteristic(
        GattCharacteristic theChar,
        GattDescriptorsResult descriptors,
        std::string deviceID
        );

    void GattService_Changed(
        BluetoothLEDevice device,
        IInspectable object
    );

    winrt::fire_and_forget FakeDeviceDisconnect(
        std::string deviceID
        );

    winrt::fire_and_forget ReadCharactersitc(
        GattCharacteristic theChar,
        std::string deviceID,
        ServiceData serviceData,
        std::string service_uuid,
        std::string characteristic_uuid
        );

    winrt::fire_and_forget WriteCharactersitc(
        GattCharacteristic theChar,
        std::string deviceID,
        ServiceData serviceData,
        std::string service_uuid,
        std::string characteristic_uuid,
        std::string value,
        int writeType
        );
};

//////////////////////////////////////////////////////////////////////////////////////////

#endif

//////////////////////////////////////////////////////////////////////////////////////////

