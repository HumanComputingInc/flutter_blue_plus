//////////////////////////////////////////////////////////////////////////////////////////

#include "win_ble.h"
#include "flutter_blue_plus_plugin.h"
#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

//////////////////////////////////////////////////////////////////////////////////////////

#define GUID_FORMAT "%08x-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
#define GUID_ARG(guid) guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]

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

std::string UuidToString(
    winrt::guid guid
    )
{
    char chars[36 + 1];
    sprintf_s(chars, GUID_FORMAT, GUID_ARG(guid));

    return std::string{ chars };
}

//////////////////////////////////////////////////////////////////////////////////////////

std::vector<uint8_t> to_bytevc(
    IBuffer buffer
    )
{
    auto reader = DataReader::FromBuffer(buffer);
    auto result = std::vector<uint8_t>(reader.UnconsumedBufferLength());
    reader.ReadBytes(result);
    return result;
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
//////////////////////////////////////////////////////////////////////////////////////////

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
    if (mConnectedDevice == nullptr)
    {
        return;
    }

    mConnectedDevice.ConnectionStatusChanged(mConnnectionStatusChangedToken);
    for (int i = 0; i < mSerivcesData.size();i++)
    {
        ServiceData serivceData = mSerivcesData.at(i);
        for (int j = 0; j < serivceData.chars.size();j++)
        {
            GattCharacteristic theChar = serivceData.chars.at(j);
            theChar.ValueChanged(serivceData.valueChangedTokens[UuidToString(theChar.Uuid())]);
        }
    }

    mSerivcesData.clear();

    if (mConnectedDevice != nullptr)
        mConnectedDevice = nullptr;
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
        mSerivcesData.clear();
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
    mConnnectionStatusChangedToken = mConnectedDevice.ConnectionStatusChanged({ this, &BLEHelper::BluetoothLEDevice_ConnectionStatusChanged });
    BluetoothLEDevice_ConnectionStatusChanged(mConnectedDevice,nullptr);

    co_return;
}

//////////////////////////////////////////////////////////////////////////////////////////

winrt::fire_and_forget BLEHelper::NotifyForAdapterState(
    )
{
    Windows::Devices::Radios::RadioState BtState = GetBtState();
    int btStateMap = bmAdapterStateEnum(BtState);
    flutter::EncodableMap resMap = flutter::EncodableMap{ {"adapter_state",btStateMap} };

    for (int i = 0; i < 5;i++)
    {
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

int HasProp(
    GattCharacteristicProperties props,
    GattCharacteristicProperties prop
    )
{
    return ((props & prop) == prop) ? 1 : 0;
}

//////////////////////////////////////////////////////////////////////////////////////////

flutter::EncodableMap BLEHelper::ExportCharacteristic(
    GattCharacteristic theChar,
    GattDescriptorsResult descriptors,
    std::string deviceID
    )
{
    std::vector<flutter::EncodableValue> descriptorsArray;

    WriteLogFile("DESCRIPTORS on " + UuidToString(theChar.Service().Uuid()) + " char " + UuidToString(theChar.Uuid()) + ":");
    for (uint32_t i = 0; i < descriptors.Descriptors().Size();i++)
    {
        GattDescriptor descr = descriptors.Descriptors().GetAt(i);
        flutter::EncodableMap descrMap = {
            {"service_uuid",UuidToString(theChar.Service().Uuid())},
            {"remote_id",deviceID},
            {"descriptor_uuid",UuidToString(descr.Uuid())},
            {"characteristic_uuid", UuidToString(theChar.Uuid())}
        };

        WriteLogFile("\t" + UuidToString(descr.Uuid()));
        descriptorsArray.push_back(descrMap);
    }

    GattCharacteristicProperties props = theChar.CharacteristicProperties();

    flutter::EncodableMap propsMap = {
        {"broadcast", HasProp(props,GattCharacteristicProperties::Broadcast)},
        {"write_without_response", HasProp(props,GattCharacteristicProperties::WriteWithoutResponse)},
        //todo
        {"notify_encryption_required", 0},
        {"read", HasProp(props,GattCharacteristicProperties::Read)},
        {"authenticated_signed_writes", HasProp(props,GattCharacteristicProperties::AuthenticatedSignedWrites)},
        {"extended_properties", HasProp(props,GattCharacteristicProperties::ExtendedProperties)},
        {"indicate", HasProp(props,GattCharacteristicProperties::Indicate)},
        //todo?
        {"indicate_encryption_required", 0},
        {"write", HasProp(props,GattCharacteristicProperties::Write)},
        {"notify", HasProp(props,GattCharacteristicProperties::Notify)}
    };

    flutter::EncodableMap characteristic = {
        {"descriptors",descriptorsArray},
        {"service_uuid",UuidToString(theChar.Service().Uuid())},
        {"remote_id",deviceID},
        {"characteristic_uuid",UuidToString(theChar.Uuid())},
        {"properties",propsMap}
    };


    return characteristic;
}

//////////////////////////////////////////////////////////////////////////////////////////

winrt::fire_and_forget BLEHelper::GetServices(
    )
{
    if (mConnectedDevice == nullptr)
        co_return;

    GattDeviceServicesResult Services = co_await mConnectedDevice.GetGattServicesAsync(BluetoothCacheMode::Uncached);

    if (Services == nullptr)
        co_return;

    std::string mac = DeviceIdToMac(winrt::to_string(mConnectedDevice.DeviceId()));

    std::vector<flutter::EncodableValue> servicesArray;
    uint32_t servicesCount = Services.Services().Size();

    for (uint32_t i = 0; i < servicesCount; i++)
    {
        GattDeviceService service = Services.Services().GetAt(i);
        std::string serviceUUID = UuidToString(service.Uuid());
        GattCharacteristicsResult chars = co_await service.GetCharacteristicsAsync(BluetoothCacheMode::Uncached);

        ServiceData serviceData;

        serviceData.service = service;
        std::vector<flutter::EncodableValue> charsArray;

        if (chars != nullptr)
        {
            for (uint32_t j = 0; j < chars.Characteristics().Size();j++)
            {
                //single charactersitic
                GattCharacteristic theChar = chars.Characteristics().GetAt(j);
                GattDescriptorsResult descriptors = co_await theChar.GetDescriptorsAsync();
                charsArray.push_back(ExportCharacteristic(theChar,descriptors,mac));
                serviceData.chars.push_back(theChar);
            }
        }

        std::vector<flutter::EncodableValue> includedservices;
        flutter::EncodableMap serviceMap = {
            {"included_services",includedservices},
            {"is_primary",1},
            {"service_uuid",serviceUUID},
            {"remote_id", mac},
            {"characteristics",charsArray}
        };

        servicesArray.push_back(serviceMap);
        mSerivcesData.push_back(serviceData);
    }

    //export it all now
    flutter::EncodableMap servicesMap = {
        {"error_string","GATT_SUCCESS"},
        {"success", 1},
        {"remote_id",mac},
        {"error_code", 0},
        {"services",servicesArray}
    };

    std::unique_ptr<flutter::EncodableValue> res = std::make_unique<flutter::EncodableValue>(servicesMap);

    flutter_blue_plus::FlutterBluePlusPlugin::mMethodChannel->InvokeMethod("OnDiscoveredServices", std::move(res));
}

//////////////////////////////////////////////////////////////////////////////////////////

winrt::fire_and_forget BLEHelper::FakeRSSI(
    )
{
    std::string mac = DeviceIdToMac(winrt::to_string(mConnectedDevice.DeviceId()));
    Sleep(100);

    flutter::EncodableMap servicesMap = {
        {"remote_id",mac },
        {"rssi", -90},
        {"success",1},
        {"error_code", 0},
        {"error_string","GATT_SUCCESS"}
    };

    std::unique_ptr<flutter::EncodableValue> res = std::make_unique<flutter::EncodableValue>(servicesMap);

    flutter_blue_plus::FlutterBluePlusPlugin::mMethodChannel->InvokeMethod("OnReadRssi", std::move(res));

    co_return;
}

//////////////////////////////////////////////////////////////////////////////////////////

void BLEHelper::GattCharacteristic_ValueChanged(
    GattCharacteristic sender,
    GattValueChangedEventArgs args
    )
{
    std::string uuid = UuidToString(sender.Uuid());
    std::vector<uint8_t> bytes = to_bytevc(args.CharacteristicValue());
}

//////////////////////////////////////////////////////////////////////////////////////////

std::string GetFullUUID(
    std::string uuid
    )
{
    size_t size_s;
    std::string four_len("0000%s-0000-1000-8000-00805f9b34fb");
    std::string eight_len("%s-0000-1000-8000-00805f9b34fb");
    char* pszBuff = NULL;
    switch (uuid.length())
    {
        case 4:
            size_s = four_len.length() + uuid.length() + 1;
            pszBuff = new char[size_s];
            sprintf_s(pszBuff, size_s,four_len.c_str(),uuid.c_str());
        break;
        case 8:
            size_s = eight_len.length() + uuid.length() + 1;
            pszBuff = new char[size_s];
            sprintf_s(pszBuff, size_s,eight_len.c_str(), uuid.c_str());
        break;
        default:
        return uuid;
    }

    std::string res(pszBuff);
    delete[] pszBuff;

    return res;
}

//////////////////////////////////////////////////////////////////////////////////////////

int BLEHelper::GetServiceDataIndex(
    std::string uuid
    )
{
    std::string searchUUID = GetFullUUID(uuid);
    for (int i = 0; mSerivcesData.size();i++)
    {
        ServiceData data = mSerivcesData.at(i);
        std::string serviceUUID = UuidToString(data.service.Uuid());
        if (!serviceUUID.compare(searchUUID))
            return i;
    }

    return -1;
}

//////////////////////////////////////////////////////////////////////////////////////////

GattCharacteristic BLEHelper::FindCharactersitic(
    ServiceData serviceData,
    std::string charUUID
    )
{
    std::string searchUUID = GetFullUUID(charUUID);

    for (int i = 0; i < serviceData.chars.size();i++)
    {
        GattCharacteristic theChar = serviceData.chars.at(i);
        std::string theCharUUID = UuidToString(theChar.Uuid());
        if (!theCharUUID.compare(searchUUID))
            return theChar;

    }

    return nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////

winrt::fire_and_forget BLEHelper::SetCharNotify(
    bool Enable,
    bool forceIndications,
    GattCharacteristic theChar,
    std::string deviceID,
    ServiceData serviceData,
    //as the dart side can ask for short servcie and char we need to passback the same
    std::string service_uuid,
    std::string characteristic_uuid
    )
{
    GattClientCharacteristicConfigurationDescriptorValue descrValue;
    GattCharacteristicProperties props = theChar.CharacteristicProperties();
    std::string full_char_uuid = UuidToString(theChar.Uuid());
    bool hasInidcate = ((props & GattCharacteristicProperties::Indicate) == GattCharacteristicProperties::Indicate);
    bool hasNotify = ((props & GattCharacteristicProperties::Notify) == GattCharacteristicProperties::Notify);

    OutputDebugString((L"setNotifyValue1 on\nService " + winrt::to_hstring(serviceData.service.Uuid()) +
        L" on char " + winrt::to_hstring(full_char_uuid) + L"\n").c_str());

    WriteLogFile("setNotifyValue1 on\nService (short uuid) " + service_uuid +
        " on char (full uuid) " + full_char_uuid + "\n");

    if (!hasInidcate && !hasNotify)
    {
        WriteLogFile("char has no inidcate or notif, going out");
        co_return;
    }

    //follow android logic
    if(!hasInidcate && forceIndications)
    {
        WriteLogFile("char has no inidcate but force is on, going out");
        co_return;
    }

    // If a characteristic supports both notifications and indications,
    // we use notifications. This matches how CoreBluetooth works on iOS.
    // Except of course, if forceIndications is enabled.
    if(hasInidcate)
        descrValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;
    if(hasNotify)
        descrValue = GattClientCharacteristicConfigurationDescriptorValue::Notify;
    if(forceIndications)
        descrValue = GattClientCharacteristicConfigurationDescriptorValue::Indicate;

    //Notify, Indicate, None
    GattCommunicationStatus status;
    if (Enable)
    {
        WriteLogFile("enabling notif on ");
        try
        {
            std::string full_char_uuid2 = UuidToString(theChar.Uuid());
            WriteLogFile("config on full_char_uuid2 - " + full_char_uuid2);
            status = co_await theChar.WriteClientCharacteristicConfigurationDescriptorAsync(descrValue);
            WriteLogFile("no crash at least");
        }
        catch (...)
        {
            WriteLogFile("crash on setting config");
            std::string full_char_uuid2 = UuidToString(theChar.Uuid());
            WriteLogFile("full_char_uuid2 - " + full_char_uuid2 );

            OutputDebugString((L"setNotifyValue ERROR on\nService " + winrt::to_hstring(serviceData.service.Uuid()) +
                L" on char " + winrt::to_hstring(full_char_uuid) + L"\n").c_str());
            status = GattCommunicationStatus::ProtocolError;
        }

        if (status == GattCommunicationStatus::Success)
        {
            serviceData.valueChangedTokens[full_char_uuid] = theChar.ValueChanged({ this, &BLEHelper::GattCharacteristic_ValueChanged });
        }
    }
    else
    {
        status = co_await theChar.WriteClientCharacteristicConfigurationDescriptorAsync(GattClientCharacteristicConfigurationDescriptorValue::None);
        theChar.ValueChanged(std::exchange(serviceData.valueChangedTokens[full_char_uuid], {}));
    }

    flutter::EncodableMap map = {
        {"remote_id",deviceID},
        {"service_uuid",service_uuid},
        {"characteristic_uuid",characteristic_uuid},
        {"descriptor_uuid",UuidToString(theChar.Uuid())},
        {"value",""},
        {"success",(status == GattCommunicationStatus::Success ? 1 : 0)},
        {"error_code", (int)status},
        {"error_string", ""}
    };

    //now add the callback!
    std::unique_ptr<flutter::EncodableValue> res = std::make_unique<flutter::EncodableValue>(map);

    flutter_blue_plus::FlutterBluePlusPlugin::mMethodChannel->InvokeMethod("OnDescriptorWritten", std::move(res));
}

//////////////////////////////////////////////////////////////////////////////////////////

ServiceData BLEHelper::GetServiceData(
    int nIndex
    )
{
    return mSerivcesData.at(nIndex);
}

//////////////////////////////////////////////////////////////////////////////////////////


