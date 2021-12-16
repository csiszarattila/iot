import 'package:flutter/material.dart';
import 'package:web_socket_channel/web_socket_channel.dart';
import 'dart:convert';
import 'sensors_data.dart';
import 'package:firebase_core/firebase_core.dart';
import 'package:firebase_messaging/firebase_messaging.dart';
import 'firebase_options.dart';
 
void main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await Firebase.initializeApp(
    options: DefaultFirebaseOptions.currentPlatform,
  );

  FirebaseMessaging.onMessage.listen((RemoteMessage message) {
    print('Got a message whilst in the foreground!');
    print('Message data: ${message.data}');

    if (message.notification != null) {
      print('Message also contained a notification: ${message.notification?.body}');
    }
  });

  runApp(const MyApp());
}

class MyApp extends StatelessWidget {
  const MyApp({Key? key}) : super(key: key);

  // This widget is the root of your application.
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Flutter Demo',
      theme: ThemeData(
        // This is the theme of your application.
        //
        // Try running your application with "flutter run". You'll see the
        // application has a blue toolbar. Then, without quitting the app, try
        // changing the primarySwatch below to Colors.green and then invoke
        // "hot reload" (press "r" in the console where you ran "flutter run",
        // or simply save your changes to "hot reload" in a Flutter IDE).
        // Notice that the counter didn't reset back to zero; the application
        // is not restarted.
        primarySwatch: Colors.blue,
      ),
      home: const MyHomePage(title: 'AirQuality'),
    );
  }
}

class MyHomePage extends StatefulWidget {
  const MyHomePage({Key? key, required this.title}) : super(key: key);

  // This widget is the home page of your application. It is stateful, meaning
  // that it has a State object (defined below) that contains fields that affect
  // how it looks.

  // This class is the configuration for the state. It holds the values (in this
  // case the title) provided by the parent (in this case the App widget) and
  // used by the build method of the State. Fields in a Widget subclass are
  // always marked "final".

  final String title;

  @override
  State<MyHomePage> createState() => _MyHomePageState();
}

class _MyHomePageState extends State<MyHomePage> {
  int _aqi = 0;
  double _pm10 = 0.0;
  double _pm4 = 0.0;
  double _pm25 = 0.0;
  double _pm1 = 0.0;
  SensorsData sensorsData = SensorsData(0, 0, 0.0, 0.0, 0.0, 0.0);
  
  int _selectedScene = 0;

  String _token = '';

  late FirebaseMessaging messaging;

  @override
  void initState() { 
    super.initState();
    messaging = FirebaseMessaging.instance;
    messaging.getToken().then((value){
        print(value);
    });
  }

  static const List<Widget> _scenes = <Widget>[
    // Column(
    //   children: <Widget>[
    //     Text(
    //       "${_aqi}",
    //       style: Theme.of(context).textTheme.headline4,
    //     ),
    //     Text(
    //       'PM${"\u2081\u2080"} - ${sensorsData.pm10} µg/m3',
    //       style: Theme.of(context).textTheme.headline4,
    //     ),
    //     Text(
    //       'PM${"\u2084"} - ${sensorsData.pm4} µg/m3',
    //       style: Theme.of(context).textTheme.headline4,
    //     ),
    //     Text(
    //       'PM${"\u2082\u2085"} - ${sensorsData.pm25} µg/m3',
    //       style: Theme.of(context).textTheme.headline4,
    //     ),
    //     Text(
    //       'PM${"\u2081"} - ${sensorsData.pm1} µg/m3',
    //       style: Theme.of(context).textTheme.headline4,
    //     ),
    //   ],
    // ),
    Text("Settings"),
    Text("Mérések")
  ];

  @override
  Widget build(BuildContext context) {
    // This method is rerun every time setState is called, for instance as done
    // by the _incrementCounter method above.
    //
    // The Flutter framework has been optimized to make rerunning build methods
    // fast, so that you can just rebuild anything that needs updating rather
    // than having to individually change instances of widgets.
    final _channel = WebSocketChannel.connect(
      Uri.parse('ws://192.168.0.236/ws'),
    );

    _channel.stream.listen((message) {
      Map<String, dynamic> payload = jsonDecode(message);

      if (payload['event'] == 'sensors') {
        print(payload);
        setState(() {
          sensorsData = SensorsData.fromJson(payload['data']);
          _aqi = sensorsData.aqi;
        });
      }
    });

    return Scaffold(
      appBar: AppBar(
        // Here we take the value from the MyHomePage object that was created by
        // the App.build method, and use it to set our appbar title.
        title: Text(widget.title),
      ),
      body: IndexedStack(
        index: _selectedScene,
        children: _scenes,
      ),
      bottomNavigationBar: BottomNavigationBar(
        items: const <BottomNavigationBarItem>[
          BottomNavigationBarItem(label: "LM Index", icon: Icon(Icons.sensors_rounded)),
          BottomNavigationBarItem(label: "Beállítások", icon: Icon(Icons.settings_rounded)),
          BottomNavigationBarItem(label: "Mérések", icon: Icon(Icons.analytics_rounded))
        ],
        selectedItemColor: Colors.amber[800],
        currentIndex: _selectedScene,
        onTap: (int index) {
          setState(() {
            _selectedScene = index;
          });
        },
      ),
    );
  }
}
