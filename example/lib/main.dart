import 'dart:async';
import 'dart:io';
import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:logger/logger.dart';
import 'package:path_provider/path_provider.dart';
import 'screens/bluetooth_off_screen.dart';
import 'screens/scan_screen.dart';
import 'package:intl/intl.dart';
import 'package:path/path.dart' as PathUtils;
import 'package:logger/logger.dart';

void main() async{
  String fileName = DateFormat("ddMMMyy").format(DateTime.now());
  Directory applicationDir = await getApplicationDocumentsDirectory();
  Directory logsDir = Directory(PathUtils.join(applicationDir.path,"BTLogs"));

  String logFile = PathUtils.join(
      logsDir.path,
      "BT_${fileName}.log");

  Logger logger = Logger(
      filter: ProductionFilter(),
      level: Level.all,
      printer: PrettyPrinter(colors: false, noBoxingByDefault: true, printEmojis: false,printTime:false,methodCount: 0),
      output:FileOutput(file: File(logFile)));

  if(!logsDir.existsSync()){
    logsDir.createSync();
  }

  runZonedGuarded(() async {
    logger.i("app starting");
    WidgetsFlutterBinding.ensureInitialized();
    FlutterBluePlus.setLogLevel(LogLevel.verbose, color: true);
    runApp( FlutterBlueApp(logger));
    FlutterError.onError = (FlutterErrorDetails details) {
      FlutterError.presentError(details);
      logger.e("crash!" + details.toString());
      logger.e("crash!",error:details.exception,stackTrace: details.stack);
      //LogsLib.FlutterLogs.logInfo("APP", "FLUTTER ERROR1", details.toString());
    };
  }, (Object error, StackTrace stack) {
    logger.e("crash3",error:error,stackTrace: stack);
  });
}

//
// This widget shows BluetoothOffScreen or
// ScanScreen depending on the adapter state
//
class FlutterBlueApp extends StatefulWidget {
  Logger logger;
  FlutterBlueApp(this.logger,{Key? key}) : super(key: key);

  @override
  State<FlutterBlueApp> createState() => _FlutterBlueAppState();
}

class _FlutterBlueAppState extends State<FlutterBlueApp> {
  BluetoothAdapterState _adapterState = BluetoothAdapterState.unknown;

  late StreamSubscription<BluetoothAdapterState> _adapterStateStateSubscription;

  @override
  void initState() {
    super.initState();
    _adapterStateStateSubscription = FlutterBluePlus.adapterState.listen((state) {
      _adapterState = state;
      if (mounted) {
        setState(() {});
      }
    });
  }

  @override
  void dispose() {
    _adapterStateStateSubscription.cancel();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    Widget screen = _adapterState == BluetoothAdapterState.on
        ? ScanScreen(widget.logger)
        : BluetoothOffScreen(adapterState: _adapterState);

    return MaterialApp(
      color: Colors.lightBlue,
      home: ScanScreen(widget.logger),
      navigatorObservers: [BluetoothAdapterStateObserver()],
    );
  }
}

//
// This observer listens for Bluetooth Off and dismisses the DeviceScreen
//
class BluetoothAdapterStateObserver extends NavigatorObserver {
  StreamSubscription<BluetoothAdapterState>? _adapterStateSubscription;

  @override
  void didPush(Route route, Route? previousRoute) {
    super.didPush(route, previousRoute);
    if (route.settings.name == '/DeviceScreen') {
      // Start listening to Bluetooth state changes when a new route is pushed
      _adapterStateSubscription ??= FlutterBluePlus.adapterState.listen((state) {
        if (state != BluetoothAdapterState.on) {
          // Pop the current route if Bluetooth is off
          navigator?.pop();
        }
      });
    }
  }

  @override
  void didPop(Route route, Route? previousRoute) {
    super.didPop(route, previousRoute);
    // Cancel the subscription when the route is popped
    _adapterStateSubscription?.cancel();
    _adapterStateSubscription = null;
  }
}

