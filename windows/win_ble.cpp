//////////////////////////////////////////////////////////////////////////////////////////

#include "win_ble.h"
#include "flutter_blue_plus_plugin.h"
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

//////////////////////////////////////////////////////////////////////////////////////////

int bmAdapterStateEnum(Windows::Devices::Radios::RadioState as) {
    switch (as)
    {
    case Windows::Devices::Radios::RadioState::Off:
        return 6;
    case Windows::Devices::Radios::RadioState::On:
        return 4;
        //case Windows::Devices::Radios::RadioState::Disabled
        //    return 4;
    default:
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

int bmConnectionStatusEnum(BluetoothConnectionStatus status) {
    switch (status)
    {
    case BluetoothConnectionStatus::Connected:
        return 1;
    case BluetoothConnectionStatus::Disconnected:
        return 0;
    default:
        return 0;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

LONG64 MacToLong(
    std::string deviceID
    )
{
    deviceID.erase(std::remove(deviceID.begin(), deviceID.end(), ':'), deviceID.end());
    return std::stoull(deviceID,nullptr,16);
}

//////////////////////////////////////////////////////////////////////////////////////////
//mac is always with ::::
static std::string DeviceIdToMac(
    std::string deviceID
    )
{
    std::string::size_type pos = deviceID.find("-", 0);

    if (pos != std::string::npos)
    {
        pos += 1;
        if (pos != std::wstring::npos)
        {
            deviceID = deviceID.substr(pos, std::string::npos);
            //std::string address = deviceID.substr(pos, std::string::npos);
            //address.erase(std::remove(address.begin(), address.end(), ':'), address.end());
            //return _cstoll_l(address.data(), NULL, 16, NULL);
            return deviceID;
        }
    }

    return deviceID;
}

//////////////////////////////////////////////////////////////////////////////////////////

BLEHelper::BLEHelper(
    )
{
    try
    {
        mhSysScanEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        mLogFile = std::ofstream("log.txt");
        //all ble devices and connected
        auto sysDevciesFilter = L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\" AND System.Devices.Aep.IsConnected:=System.StructuredQueryType.Boolean#True";
        auto allBleDeviceFilte = L"System.Devices.Aep.ProtocolId:=\"{bb7bb05e-5972-42b5-94fc-76eaa7084d49}\"";

            auto requestedProperties = {
                L"System.Devices.Aep.Category",
                L"System.Devices.Aep.ContainerId",
                L"System.Devices.Aep.DeviceAddress",
                L"System.Devices.Aep.IsConnected"
                //L"System.Devices.Aep.Bluetooth.Le.IsConnectable",
                //L"System.Devices.Aep.SignalStrength"
        };

        mDeviceWatcher = DeviceInformation::CreateWatcher(
            allBleDeviceFilte,
                requestedProperties,
                DeviceInformationKind::AssociationEndpoint
        );

        if(mDeviceWatcher != nullptr){
            mDeviceWatcherAddedToken = mDeviceWatcher.Added({ this, &BLEHelper::DeviceWatcher_Added });
            mDeviceWatcherUpdatedToken = mDeviceWatcher.Updated({ this, &BLEHelper::DeviceWatcher_Updated });
            mDeviceWatcherRemovedToken = mDeviceWatcher.Removed({ this, &BLEHelper::DeviceWatcher_Removed });
            mDeviceWatcherEnumerationCompletedToken = mDeviceWatcher.EnumerationCompleted({ this, &BLEHelper::DeviceWatcher_EnumerationCompleted });
            mDeviceWatcherStoppedToken = mDeviceWatcher.Stopped({ this, &BLEHelper::DeviceWatcher_Stopped });
        }

        mSysDeviceWatcher = DeviceInformation::CreateWatcher(
            sysDevciesFilter,
            requestedProperties,
            DeviceInformationKind::AssociationEndpoint
        );

        if (mSysDeviceWatcher != nullptr) {
            mSysDeviceWatcherAddedToken = mSysDeviceWatcher.Added({ this, &BLEHelper::SysDeviceWatcher_Added });
            mSysDeviceWatcherUpdatedToken = mSysDeviceWatcher.Updated({ this, &BLEHelper::SysDeviceWatcher_Updated });
            mSysDeviceWatcherRemovedToken = mSysDeviceWatcher.Removed({ this, &BLEHelper::SysDeviceWatcher_Removed });
            mSysDeviceWatcherEnumerationCompletedToken = mSysDeviceWatcher.EnumerationCompleted({ this, &BLEHelper::SysDeviceWatcher_EnumerationCompleted });
            mSysDeviceWatcherStoppedToken = mSysDeviceWatcher.Stopped({ this, &BLEHelper::SysDeviceWatcher_Stopped });
        }

        SysStartScan();
        InitRadio();
    }
    catch (...)
    {
        //mDeviceProps = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

BLEHelper::~BLEHelper(
    )
{
    mLogFile.close();
    CloseHandle(mhSysScanEvent);
    mDeviceWatcher = nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////

IAsyncOperation<int> BLEHelper::InitRadio(
    )
{
    if (mDeviceWatcher == nullptr)
        co_return ERR_NO_DEVICE_WATCHER;

    mAdapter = co_await BluetoothAdapter::GetDefaultAsync();
    if (mAdapter == nullptr)
        co_return ERR_GET_ADAPTER;

    mRadio = co_await mAdapter.GetRadioAsync();
    if (mRadio == nullptr)
        co_return ERR_GET_RADIO;

    co_return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

VOID BLEHelper::DisconnectDevice(
    )
{
    if (mConnectedDevice)
    {
        mConnectedDevice = nullptr;
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

LONG BLEHelper::StartScan(
    )
{
    mDevices.clear();
    if(mDeviceWatcher == nullptr)
        return ERR_NO_DEVICE_WATCHER;

    mDeviceWatcher.Start();

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

LONG BLEHelper::StopScan(
    )
{
    WriteLogFile("StopScan1");
    if (mDeviceWatcher != nullptr)
        mDeviceWatcher.Stop();

    WriteLogFile("StopScan2");
    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

LONG BLEHelper::SysStartScan(
    )
{
    ResetEvent(mhSysScanEvent);
    mSysDevices.clear();
    if (mSysDeviceWatcher == nullptr)
        return ERR_NO_DEVICE_WATCHER;

    mSysDeviceWatcher.Start();

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////
//we may never call it?
//////////////////////////////////////////////////////////////////////////////////////////

LONG BLEHelper::SysStopScan(
    )
{
    if (mSysDeviceWatcher != nullptr)
        mSysDeviceWatcher.Stop();

    return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

RadioState BLEHelper::GetBtState(
    )
{
    if (mAdapter == nullptr || mRadio == nullptr)
        return Windows::Devices::Radios::RadioState::Unknown;

    return mRadio.State();
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::DeviceWatcher_Added(
        DeviceWatcher sender,
        DeviceInformation info
    )
{
    std::string name = winrt::to_string(info.Name());
    std::string mac = DeviceIdToMac(winrt::to_string(info.Id()));

    flutter::EncodableMap map = {
            {"remote_id",mac},
            {"platform_name",name},
    };

    mDevices[mac] = map;

    std::vector<flutter::EncodableValue> device;
    //itertae the map and push all devices
    std::map<std::string, flutter::EncodableMap>::iterator it;
    for (it = mDevices.begin(); it != mDevices.end(); ++it) {
        device.push_back(it->second);
    }

    flutter::EncodableMap map2 = {
            {"advertisements",device}
    };

    std::unique_ptr<flutter::EncodableValue> res = std::make_unique<flutter::EncodableValue>(map2);
    //notify for update
    flutter_blue_plus::FlutterBluePlusPlugin::mMethodChannel->InvokeMethod("OnScanResponse",std::move(res));
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::DeviceWatcher_Updated(
    DeviceWatcher sender,
    DeviceInformationUpdate info
    )
{
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::DeviceWatcher_Removed(
    DeviceWatcher sender,
    DeviceInformationUpdate info
    )
{

}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::DeviceWatcher_EnumerationCompleted(
    DeviceWatcher sender,
    IInspectable obj
    )
{

}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::DeviceWatcher_Stopped(
    DeviceWatcher sender,
    IInspectable obj
    )
{

}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::SendConnectionState(
    BluetoothLEDevice device,
    BluetoothConnectionStatus status
    )
{
    std::string mac = DeviceIdToMac(winrt::to_string(device.DeviceId()));
    SendConnectionState(mac, status);
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::SendConnectionState(
    std::string device,
    BluetoothConnectionStatus status
    )
{
    std::string mac = device;
    flutter::EncodableMap map = {
        {"remote_id",mac},
        {"connection_state",bmConnectionStatusEnum(status)},
        {"disconnect_reason_code", 23789258},
        {"disconnect_reason_string", "connection canceled"}
    };

    std::unique_ptr<flutter::EncodableValue> res = std::make_unique<flutter::EncodableValue>(map);

    WriteLogFile(("OnConnectionStateChanged on " + mac).c_str());
    flutter_blue_plus::FlutterBluePlusPlugin::mMethodChannel->InvokeMethod("OnConnectionStateChanged", std::move(res));
}

//////////////////////////////////////////////////////////////////////////////////////////

IAsyncOperation<int> BLEHelper::TestCall(
    )
{
    DeviceInformation dev{ nullptr };
    DeviceInformationCollection devcies = co_await
        DeviceInformation::FindAllAsync(BluetoothDevice::GetDeviceSelector());

    for (uint32_t i = 0; i < devcies.Size();i++) {
        dev = devcies.GetAt(i);
        std::string mac = DeviceIdToMac(winrt::to_string(dev.Id()));
        std::string name = winrt::to_string(dev.Name());

        printf(mac.c_str());
        printf(name.c_str());    
    }

    co_return 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::SysDeviceWatcher_Added(
    DeviceWatcher sender,
    DeviceInformation info
    )
{
    std::string name = winrt::to_string(info.Name());
    std::string mac = DeviceIdToMac(winrt::to_string(info.Id()));

    flutter::EncodableMap map = {
            {"remote_id",mac},
            {"platform_name",name},
    };

    mSysDevices[mac] = map;
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::SysDeviceWatcher_Updated(
    DeviceWatcher sender,
    DeviceInformationUpdate info
    )
{
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::SysDeviceWatcher_Removed(
    DeviceWatcher sender,
    DeviceInformationUpdate info
    )
{

}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::SysDeviceWatcher_EnumerationCompleted(
    DeviceWatcher sender,
    IInspectable obj
    )
{
    //we are done
    SetEvent(mhSysScanEvent);
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::SysDeviceWatcher_Stopped(
    DeviceWatcher sender,
    IInspectable obj
    )
{

}

//////////////////////////////////////////////////////////////////////////////////////////

std::map<std::string, flutter::EncodableMap> BLEHelper::GetSysDevices(
    )
{
    //we do not care about the results right now
    WaitForSingleObject(mhSysScanEvent, 500);
    return mSysDevices;
}

//////////////////////////////////////////////////////////////////////////////////////////

std::string LongToMac(
    uint64_t addresss
    )
{
    char buff[20];

    sprintf_s(buff,
        "%02x:%02x:%02x:%02x:%02x:%02x",
        (unsigned char)((addresss >> 40) & 0xFF),
        (unsigned char)((addresss >> 32) & 0xFF),
        (unsigned char)((addresss >> 24) & 0xFF),
        (unsigned char)((addresss >> 16) & 0xFF),
        (unsigned char)((addresss >> 8) & 0xFF),
        (unsigned char)((addresss) & 0xFF)
    );

    return buff;
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::BluetoothLEDevice_ConnectionStatusChanged(
    BluetoothLEDevice sender,
    IInspectable args
    )
{
    std::string mac = LongToMac(sender.BluetoothAddress());

    OutputDebugString((L"BluetoothLEDevice_ConnectionStatusChanged on " + winrt::to_hstring(mac) + L"\n").c_str());
    WriteLogFile(("BluetoothLEDevice_ConnectionStatusChanged on " + mac).c_str());
    SendConnectionState(mac, sender.ConnectionStatus());
    WriteLogFile("SendConnectionState done");
}

//////////////////////////////////////////////////////////////////////////////////////////

winrt::fire_and_forget BLEHelper::ConnectDevice(
    std::string deviceID
    )
{
    LONG64 deviceID64 = MacToLong(deviceID);

    OutputDebugString( (L"ConnectDevice " + winrt::to_hstring(deviceID) + L"\n").c_str() );
    WriteLogFile(("ConnectDevice  " + deviceID).c_str());

    try {
        mConnectedDevice = co_await BluetoothLEDevice::FromBluetoothAddressAsync(deviceID64);
        OutputDebugString(L"FromBluetoothAddressAsync finished");
        WriteLogFile("FromBluetoothAddressAsync finished");
    }
    catch (...)
    {
        OutputDebugString(L"FromBluetoothAddressAsync crashed");
        WriteLogFile("FromBluetoothAddressAsync crashed");
    }

    if (mConnectedDevice == nullptr) {
        WriteLogFile("mConnectedDevice is null");
        SendConnectionState(deviceID, BluetoothConnectionStatus::Disconnected);
        co_return;
    }

    OutputDebugString(L"we got device!");
    WriteLogFile("we got device!");
    mConnectedDevice.ConnectionStatusChanged({ this, &BLEHelper::BluetoothLEDevice_ConnectionStatusChanged });
    BluetoothLEDevice_ConnectionStatusChanged(mConnectedDevice,nullptr);

    co_return;
}

//////////////////////////////////////////////////////////////////////////////////////////
/**
winrt::fire_and_forget BLEHelper::GetServices(
    )
{
    //get sevceis and call on service found
}
**/
//////////////////////////////////////////////////////////////////////////////////////////

winrt::fire_and_forget BLEHelper::NotifyForAdapterState(
    )
{
    Windows::Devices::Radios::RadioState BtState = GetBtState();
    int btStateMap = bmAdapterStateEnum(BtState);
    flutter::EncodableMap resMap = flutter::EncodableMap{ {"adapter_state",btStateMap} };

    for (int i = 0; i < 5;i++)
    {
        Sleep(500);
        std::unique_ptr<flutter::EncodableValue> res = std::make_unique<flutter::EncodableValue>(resMap);

        WriteLogFile("OnAdapterStateChanged");
        flutter_blue_plus::FlutterBluePlusPlugin::mMethodChannel->InvokeMethod(
            "OnAdapterStateChanged", std::move(res));
    }

    co_return;
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::WriteLogFile(
    std::string msg
    )
{
    mLogFile << msg << std::endl;
}

//////////////////////////////////////////////////////////////////////////////////////////
