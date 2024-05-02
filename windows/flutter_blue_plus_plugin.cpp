//////////////////////////////////////////////////////////////////////////////////////////

#include "flutter_blue_plus_plugin.h"

//////////////////////////////////////////////////////////////////////////////////////////
// This must be included before many other Windows headers.
//////////////////////////////////////////////////////////////////////////////////////////

#include <windows.h>

#include <winrt\base.h>
#include <winrt\Windows.Foundation.h>
#pragma comment(lib, "windowsapp.lib")

//////////////////////////////////////////////////////////////////////////////////////////
// For getPlatformVersion; remove unless needed for your plugin implementation.
//////////////////////////////////////////////////////////////////////////////////////////
#include <VersionHelpers.h>

//////////////////////////////////////////////////////////////////////////////////////////

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

//////////////////////////////////////////////////////////////////////////////////////////

#include <memory>
#include <sstream>

//////////////////////////////////////////////////////////////////////////////////////////

namespace flutter_blue_plus {

//////////////////////////////////////////////////////////////////////////////////////////
//copy from android so we have the same result
//////////////////////////////////////////////////////////////////////////////////////////

flutter::MethodChannel<flutter::EncodableValue>* FlutterBluePlusPlugin::mMethodChannel{
    nullptr
};

//////////////////////////////////////////////////////////////////////////////////////////

void FlutterBluePlusPlugin::RegisterWithRegistrar(
    flutter::PluginRegistrarWindows *registrar
    )
{
    mMethodChannel = new flutter::MethodChannel<flutter::EncodableValue>(
          registrar->messenger(), "flutter_blue_plus/methods",
          &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_unique<FlutterBluePlusPlugin>();

    mMethodChannel->SetMethodCallHandler(
      [plugin_pointer = plugin.get()](const auto &call, auto result) {
        plugin_pointer->HandleMethodCall(call, std::move(result));
      });

  registrar->AddPlugin(std::move(plugin));
}

//////////////////////////////////////////////////////////////////////////////////////////

FlutterBluePlusPlugin::FlutterBluePlusPlugin(
    )
{
    mpBleHelper = new BLEHelper();
}

//////////////////////////////////////////////////////////////////////////////////////////

FlutterBluePlusPlugin::~FlutterBluePlusPlugin(
    )
{
    if (mMethodChannel)
    {
        delete mMethodChannel;
        mMethodChannel = NULL;
    }

    delete mpBleHelper;
}

//////////////////////////////////////////////////////////////////////////////////////////

void FlutterBluePlusPlugin::HandleMethodCall(
    const flutter::MethodCall<flutter::EncodableValue> &method_call,
    std::unique_ptr<flutter::MethodResult<flutter::EncodableValue>> result
    )
{
    mpBleHelper->WriteLogFile(("HandleMethodCall " + method_call.method_name()).c_str());
    OutputDebugString((L"HandleMethodCall " + winrt::to_hstring(method_call.method_name()) + L"\n").c_str());
  if (method_call.method_name().compare("getPlatformVersion") == 0) {
    std::ostringstream version_stream;
    version_stream << "Windows ";
    if (IsWindows10OrGreater())
      version_stream << "10+";
    else if (IsWindows8OrGreater())
      version_stream << "8";
    else if (IsWindows7OrGreater())
      version_stream << "7";

    result->Success(flutter::EncodableValue(version_stream.str()));
  }
  else if (method_call.method_name().compare("flutterHotRestart") == 0)
  {
      result->Success(flutter::EncodableValue(0));
  }
  else if (method_call.method_name().compare("setLogLevel") == 0)
  {
      result->Success(flutter::EncodableValue(true));
  }
  else if (method_call.method_name().compare("getAdapterState") == 0)
  {
      Windows::Devices::Radios::RadioState BtState = mpBleHelper->GetBtState();
      int btStateMap = bmAdapterStateEnum(BtState);

      //hack for not getting 
      //mpBleHelper->NotifyForAdapterState();

      result->Success(flutter::EncodableMap{{"adapter_state",btStateMap}});
  }
  else if (method_call.method_name().compare("startScan") == 0)
  {
      flutter::EncodableMap args = std::get<flutter::EncodableMap>(*method_call.arguments());
      flutter::EncodableValue withServices = args[flutter::EncodableValue("with_services")];
      std::vector<flutter::EncodableValue> serviceList = std::get<std::vector<flutter::EncodableValue>>(withServices);

      mpBleHelper->StartScan();

      result->Success(true);
  }
  else if (method_call.method_name().compare("getSystemDevices") == 0)
  {
      std::map<std::string, flutter::EncodableMap> systemDevices = mpBleHelper->GetSysDevices();

      std::vector<flutter::EncodableValue> device;
      //itertae the map and push all devices
      std::map<std::string, flutter::EncodableMap>::iterator it;
      for (it = systemDevices.begin(); it != systemDevices.end(); ++it) {
          device.push_back(it->second);
      }

      flutter::EncodableMap map2 = {
              {"devices",device}
      };

      result->Success(map2);
  }
  else if (method_call.method_name().compare("stopScan") == 0)
  {
      mpBleHelper->StopScan();
      result->Success(true);
  }
  else if (method_call.method_name().compare("connect") == 0)
  {
      flutter::EncodableMap args = std::get<flutter::EncodableMap>(*method_call.arguments());
      std::string  mac = std::get<std::string>(args[flutter::EncodableValue("remote_id")]);

      mpBleHelper->ConnectDevice(mac);
      result->Success(true);
  }
  else if (method_call.method_name().compare("disconnect") == 0)
  {
      mpBleHelper->DisconnectDevice();
      result->Success(true);
  }
 
  else if (method_call.method_name().compare("discoverServices") == 0)
  {
      mpBleHelper->GetServices();
      result->Success(true);
  }
  else if (method_call.method_name().compare("readRssi") == 0)
  {
      mpBleHelper->FakeRSSI();
      result->Success(true);
  }
  else if (method_call.method_name().compare("setNotifyValue") == 0)
  {
      /**
      flutter::EncodableMap args = std::get<flutter::EncodableMap>(*method_call.arguments());
      std::string mac = std::get<std::string>(args[flutter::EncodableValue("remote_id")]);
      std::string service_uuid = std::get<std::string>(args[flutter::EncodableValue("service_uuid")]);
      std::string characteristic_uuid = std::get<std::string>(args[flutter::EncodableValue("characteristic_uuid")]);
      bool enable = std::get<bool>(args[flutter::EncodableValue("remote_id")]);
      bool forceIndications = std::get<bool>(args[flutter::EncodableValue("force_indications")]);
      **/
      result->Success(true);
  }
  else
  {
        mpBleHelper->WriteLogFile(("Not implemented - " + method_call.method_name()).c_str());
        result->NotImplemented();
  }
}

//////////////////////////////////////////////////////////////////////////////////////////

}  // namespace flutter_blue_plus

//////////////////////////////////////////////////////////////////////////////////////////

